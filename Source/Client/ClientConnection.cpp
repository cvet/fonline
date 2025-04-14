//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include "ClientConnection.h"
#include "ClientScripting.h"
#include "GenericUtils.h"
#include "Log.h"
#include "NetCommand.h"
#include "StringUtils.h"
#include "Timer.h"
#include "Version-Include.h"
#include "WinApi-Include.h"

#include "zlib.h"
#if !FO_WINDOWS
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define closesocket close
#define SD_RECEIVE SHUT_RD
#define SD_SEND SHUT_WR
#define SD_BOTH SHUT_RDWR
#endif

struct ClientConnection::Impl
{
    auto GetLastSocketError() const -> string;
    void FillSockAddr(sockaddr_in& saddr, string_view host, uint16 port) const;

    z_stream ZStream {};
    bool ZStreamActive {};
    sockaddr_in SockAddr {};
    sockaddr_in ProxyAddr {};
    SOCKET NetSock {INVALID_SOCKET};
    fd_set NetSockSet {};
};

ClientConnection::ClientConnection(ClientNetworkSettings& settings) :
    _settings {settings},
    _impl {SafeAlloc::MakeUnique<Impl>()},
    _netIn(_settings.NetBufferSize),
    _netOut(_settings.NetBufferSize)
{
    STACK_TRACE_ENTRY();

    _connectCallback = [](bool) {};
    _disconnectCallback = [] {};

    _incomeBuf.resize(_settings.NetBufferSize);

    AddMessageHandler(NETMSG_DISCONNECT, [this] { Disconnect(); });
    AddMessageHandler(NETMSG_PING, [this] { Net_OnPing(); });
}

ClientConnection::~ClientConnection()
{
    STACK_TRACE_ENTRY();

    if (_impl->NetSock != INVALID_SOCKET) {
        ::closesocket(_impl->NetSock);
    }
    if (_impl->ZStreamActive) {
        inflateEnd(&_impl->ZStream);
    }
}

void ClientConnection::AddConnectHandler(ConnectCallback handler)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(handler);

    _connectCallback = std::move(handler);
}

void ClientConnection::AddDisconnectHandler(DisconnectCallback handler)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(handler);

    _disconnectCallback = std::move(handler);
}

void ClientConnection::AddMessageHandler(uint msg, MessageCallback handler)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(handler);
    RUNTIME_ASSERT(CheckNetMsgSignature(msg));
    RUNTIME_ASSERT(_handlers.count(ExtractNetMsgNumber(msg)) == 0);

    _handlers.emplace(ExtractNetMsgNumber(msg), std::move(handler));
}

void ClientConnection::Connect()
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!_isConnected);
    RUNTIME_ASSERT(!_isConnecting);

    try {
        ConnectToHost();
    }
    catch (const ClientConnectionException& ex) {
        WriteLog("Connecting error: {}", ex.what());
        Disconnect();
        _connectCallback(false);
    }
    catch (const NetBufferException& ex) {
        WriteLog("Connecting error: {}", ex.what());
        Disconnect();
        _connectCallback(false);
    }
    catch (...) {
        safe_call([this] { Disconnect(); });
        throw;
    }
}

void ClientConnection::Process()
{
    STACK_TRACE_ENTRY();

    try {
        ProcessConnection();
    }
    catch (const ClientConnectionException& ex) {
        WriteLog("Connection error: {}", ex.what());
        Disconnect();
    }
    catch (const NetBufferException& ex) {
        WriteLog("Connection error: {}", ex.what());
        Disconnect();
    }
    catch (...) {
        safe_call([this] { Disconnect(); });
        throw;
    }
}

void ClientConnection::ProcessConnection()
{
    STACK_TRACE_ENTRY();

    if (_settings.ArtificalLags != 0 && !_artificalLagTime.has_value()) {
        _artificalLagTime = Timer::CurTime() + std::chrono::milliseconds {GenericUtils::Random(_settings.ArtificalLags / 2, _settings.ArtificalLags)};
    }

    if (_isConnecting) {
        if (!CheckSocketStatus(true)) {
            return;
        }
    }

    if (_isConnected) {
        if (ReceiveData(true)) {
            while (_isConnected && _netIn.NeedProcess()) {
                const auto msg = ExtractNetMsgNumber(_netIn.Read<uint>());
                const auto msg_len = _netIn.Read<uint>();
                UNUSED_VARIABLE(msg_len);

#if FO_DEBUG
                _msgHistory.insert(_msgHistory.begin(), msg);
#endif

                if (_settings.DebugNet) {
                    _msgCount++;
                    WriteLog("{}) Input net message {}", _msgCount, msg);
                }

                const auto it = _handlers.find(msg);

                if (it != _handlers.end()) {
                    it->second();
                }
                else {
                    throw ClientConnectionException("No handler for message", msg);
                }
            }

            if (_interthreadCommunication && _isConnected) {
                RUNTIME_ASSERT(_netIn.GetReadPos() == _netIn.GetEndPos());
            }
        }

        if (_isConnected && _netOut.IsEmpty() && _pingTime == time_point {} && _settings.PingPeriod != 0 && Timer::CurTime() >= _pingCallTime) {
            _netOut.StartMsg(NETMSG_PING);
            _netOut.Write(false);
            _pingTime = Timer::CurTime();
            _netOut.EndMsg();
        }

        DispatchData();
    }
}

auto ClientConnection::CheckSocketStatus(bool for_write) -> bool
{
    STACK_TRACE_ENTRY();

    if (_artificalLagTime.has_value()) {
        if (Timer::CurTime() >= _artificalLagTime.value()) {
            _artificalLagTime.reset();
        }
        else {
            return false;
        }
    }

    if (_interthreadCommunication) {
        if (_interthreadRequestDisconnect) {
            Disconnect();
            return false;
        }

        auto locker = std::unique_lock {_interthreadReceivedLocker};

        return for_write ? true : !_interthreadReceived.empty();
    }

    // ReSharper disable once CppLocalVariableMayBeConst
    timeval tv = {0, 0};

    FD_ZERO(&_impl->NetSockSet);
    FD_SET(_impl->NetSock, &_impl->NetSockSet);

    const auto r = ::select(static_cast<int>(_impl->NetSock) + 1, for_write ? nullptr : &_impl->NetSockSet, for_write ? &_impl->NetSockSet : nullptr, nullptr, &tv);
    if (r == 1) {
        if (_isConnecting) {
            WriteLog("Connection established");

            _isConnecting = false;
            _isConnected = true;
            Net_SendHandshake();
            _connectCallback(true);
        }

        return true;
    }

    if (r == 0) {
        int error = 0;
#if FO_WINDOWS
        int len = sizeof(error);
#else
        socklen_t len = sizeof(error);
#endif
        if (::getsockopt(_impl->NetSock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &len) != SOCKET_ERROR && error == 0) {
            return false;
        }

        throw ClientConnectionException("Socket error", _impl->GetLastSocketError());
    }
    else {
        throw ClientConnectionException("Socket select error", _impl->GetLastSocketError());
    }
}

void ClientConnection::ConnectToHost()
{
    STACK_TRACE_ENTRY();

    string_view host = _settings.ServerHost;
    auto port = numeric_cast<uint16>(_settings.ServerPort);

    _netIn.ResetBuf();
    _netOut.ResetBuf();
    _netIn.SetEncryptKey(0);
    _netOut.SetEncryptKey(0);

    // Init Zlib
    if (!_impl->ZStreamActive) {
        _impl->ZStream = {};
        _impl->ZStream.zalloc = [](voidpf, uInt items, uInt size) -> void* {
            constexpr SafeAllocator<uint8> allocator;
            return allocator.allocate(static_cast<size_t>(items) * size);
        };
        _impl->ZStream.zfree = [](voidpf, voidpf address) {
            constexpr SafeAllocator<uint8> allocator;
            allocator.deallocate(static_cast<uint8*>(address), 0);
        };

        const auto inf_init = inflateInit(&_impl->ZStream);
        RUNTIME_ASSERT(inf_init == Z_OK);

        _impl->ZStreamActive = true;
    }

    // First try interthread communication
    if (InterthreadListeners.count(port) != 0) {
        _interthreadReceived.clear();

        _interthreadSend = InterthreadListeners[port]([this](const_span<uint8> buf) {
            if (!buf.empty()) {
                auto locker = std::unique_lock {_interthreadReceivedLocker};

                _interthreadReceived.insert(_interthreadReceived.end(), buf.begin(), buf.end());
            }
            else {
                _interthreadRequestDisconnect = true;
            }
        });

        _interthreadCommunication = true;

        WriteLog("Connected to server via interthread communication");

        _isConnected = true;
        Net_SendHandshake();
        _connectCallback(true);

        return;
    }
    else {
        _interthreadCommunication = false;
    }

#if FO_WEB
    port++;
    if (!_settings.SecuredWebSockets) {
        EM_ASM(Module['websocket']['url'] = 'ws://');
        WriteLog("Connecting to server 'ws://{}:{}'", host, port);
    }
    else {
        EM_ASM(Module['websocket']['url'] = 'wss://');
        WriteLog("Connecting to server 'wss://{}:{}'", host, port);
    }
#else
    WriteLog("Connecting to server '{}:{}'", host, port);
#endif

#if FO_WINDOWS
    WSADATA wsa;
    if (::WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        throw ClientConnectionException("WSAStartup error", _impl->GetLastSocketError());
    }
#endif

    _impl->FillSockAddr(_impl->SockAddr, host, port);

    if (_settings.ProxyType != 0) {
        _impl->FillSockAddr(_impl->ProxyAddr, _settings.ProxyHost, numeric_cast<uint16>(_settings.ProxyPort));
    }

#if FO_LINUX
    constexpr auto sock_type = SOCK_STREAM | SOCK_CLOEXEC;
#else
    constexpr auto sock_type = SOCK_STREAM;
#endif
    if ((_impl->NetSock = ::socket(PF_INET, sock_type, IPPROTO_TCP)) == INVALID_SOCKET) {
        throw ClientConnectionException("Create socket error", _impl->GetLastSocketError());
    }

    // Nagle
#if !FO_WEB
    if (_settings.DisableTcpNagle) {
        auto optval = 1;
        if (::setsockopt(_impl->NetSock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&optval), sizeof(optval)) != 0) {
            WriteLog("Can't set TCP_NODELAY (disable Nagle) to socket, error '{}'", _impl->GetLastSocketError());
        }
    }
#endif

    // Direct connect
    if (_settings.ProxyType == 0) {
        // Set non blocking mode
#if FO_WINDOWS
        u_long mode = 1;
        if (::ioctlsocket(_impl->NetSock, FIONBIO, &mode) != 0)
#else
        int flags = ::fcntl(_impl->NetSock, F_GETFL, 0);
        RUNTIME_ASSERT(flags >= 0);
        if (::fcntl(_impl->NetSock, F_SETFL, flags | O_NONBLOCK))
#endif
        {
            throw ClientConnectionException("Can't set non-blocking mode to socket", _impl->GetLastSocketError());
        }

        const auto r = ::connect(_impl->NetSock, reinterpret_cast<sockaddr*>(&_impl->SockAddr), sizeof(sockaddr_in));
#if FO_WINDOWS
        if (r == SOCKET_ERROR && ::WSAGetLastError() != WSAEWOULDBLOCK)
#else
        if (r == SOCKET_ERROR && errno != EINPROGRESS)
#endif
        {
            throw ClientConnectionException("Can't connect to the game server", _impl->GetLastSocketError());
        }
    }
    else {
#if !FO_IOS && !FO_ANDROID && !FO_WEB
        // Proxy connect
        if (::connect(_impl->NetSock, reinterpret_cast<sockaddr*>(&_impl->ProxyAddr), sizeof(sockaddr_in)) != 0) {
            throw ClientConnectionException("Can't connect to proxy server", _impl->GetLastSocketError());
        }

        auto send_recv = [this]() {
            if (!DispatchData()) {
                throw ClientConnectionException("Net output error");
            }

            const auto time = Timer::CurTime();

            while (true) {
                if (ReceiveData(false)) {
                    break;
                }

                if (Timer::CurTime() - time >= std::chrono::milliseconds {10000}) {
                    throw ClientConnectionException("Proxy answer timeout");
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        };

        uint8 b1 = 0;
        uint8 b2 = 0;

        UNUSED_VARIABLE(b1);

        _netIn.ResetBuf();
        _netOut.ResetBuf();

        // Authentication
        if (_settings.ProxyType == PROXY_SOCKS4) {
            // Connect
            _netOut.Write(numeric_cast<uint8>(4)); // Socks version
            _netOut.Write(numeric_cast<uint8>(1)); // Connect command
            _netOut.Write(numeric_cast<uint16>(_impl->SockAddr.sin_port));
            _netOut.Write(numeric_cast<uint>(_impl->SockAddr.sin_addr.s_addr));
            _netOut.Write(numeric_cast<uint8>(0));

            send_recv();

            b1 = _netIn.Read<uint8>(); // Null byte
            b2 = _netIn.Read<uint8>(); // Answer code

            if (b2 != 0x5A) {
                switch (b2) {
                case 0x5B:
                    throw ClientConnectionException("Proxy connection error, request rejected or failed");
                case 0x5C:
                    throw ClientConnectionException("Proxy connection error, request failed because client is not running identd (or not reachable from the server)");
                case 0x5D:
                    throw ClientConnectionException("Proxy connection error, request failed because client's identd could not confirm the user ID string in the request");
                default:
                    throw ClientConnectionException("Proxy connection error, unknown error", b2);
                }
            }
        }
        else if (_settings.ProxyType == PROXY_SOCKS5) {
            _netOut.Write(numeric_cast<uint8>(5)); // Socks version
            _netOut.Write(numeric_cast<uint8>(1)); // Count methods
            _netOut.Write(numeric_cast<uint8>(2)); // Method

            send_recv();

            b1 = _netIn.Read<uint8>(); // Socks version
            b2 = _netIn.Read<uint8>(); // Method

            if (b2 == 2) { // User/Password
                _netOut.Write(numeric_cast<uint8>(1)); // Subnegotiation version
                _netOut.Write(numeric_cast<uint8>(_settings.ProxyUser.length())); // Name length
                _netOut.Push(_settings.ProxyUser.c_str(), _settings.ProxyUser.length()); // Name
                _netOut.Write(numeric_cast<uint8>(_settings.ProxyPass.length())); // Pass length
                _netOut.Push(_settings.ProxyPass.c_str(), _settings.ProxyPass.length()); // Pass

                send_recv();

                b1 = _netIn.Read<uint8>(); // Subnegotiation version
                b2 = _netIn.Read<uint8>(); // Status
                if (b2 != 0) {
                    throw ClientConnectionException("Invalid proxy user or password");
                }
            }
            else if (b2 != 0) { // Other authorization
                throw ClientConnectionException("Socks server connect fail");
            }

            // Connect
            _netOut.Write(numeric_cast<uint8>(5)); // Socks version
            _netOut.Write(numeric_cast<uint8>(1)); // Connect command
            _netOut.Write(numeric_cast<uint8>(0)); // Reserved
            _netOut.Write(numeric_cast<uint8>(1)); // IP v4 address
            _netOut.Write(numeric_cast<uint>(_impl->SockAddr.sin_addr.s_addr));
            _netOut.Write(numeric_cast<uint16>(_impl->SockAddr.sin_port));

            send_recv();

            b1 = _netIn.Read<uint8>(); // Socks version
            b2 = _netIn.Read<uint8>(); // Answer code

            if (b2 != 0) {
                switch (b2) {
                case 1:
                    throw ClientConnectionException("Proxy connection error, SOCKS-server error");
                case 2:
                    throw ClientConnectionException("Proxy connection error, connections fail by proxy rules");
                case 3:
                    throw ClientConnectionException("Proxy connection error, network is not aviable");
                case 4:
                    throw ClientConnectionException("Proxy connection error, host is not aviable");
                case 5:
                    throw ClientConnectionException("Proxy connection error, connection denied");
                case 6:
                    throw ClientConnectionException("Proxy connection error, TTL expired");
                case 7:
                    throw ClientConnectionException("Proxy connection error, command not supported");
                case 8:
                    throw ClientConnectionException("Proxy connection error, address type not supported");
                default:
                    throw ClientConnectionException("Proxy connection error, unknown error", b2);
                }
            }
        }
        else if (_settings.ProxyType == PROXY_HTTP) {
            string buf;

            {
                static std::mutex inet_ntoa_locker;
                auto locker = std::scoped_lock {inet_ntoa_locker};

                buf = strex("CONNECT {}:{} HTTP/1.0\r\n\r\n", ::inet_ntoa(_impl->SockAddr.sin_addr), port); // NOLINT(concurrency-mt-unsafe)
            }

            _netOut.Push(buf.c_str(), buf.length());

            send_recv();

            buf = reinterpret_cast<const char*>(_netIn.GetData() + _netIn.GetReadPos());

            if (buf.find(" 200 ") == string::npos) {
                throw ClientConnectionException("Proxy connection error", buf);
            }
        }
        else {
            throw ClientConnectionException("Unknown proxy type", _settings.ProxyType);
        }

        _netIn.ResetBuf();
        _netOut.ResetBuf();

#else
        throw ClientConnectionException("Proxy connection is not supported on this platform");
#endif
    }

    _isConnecting = true;
}

void ClientConnection::Disconnect()
{
    STACK_TRACE_ENTRY();

    if (_interthreadCommunication) {
        InterthreadDataCallback interthread_send;

        if (!_interthreadRequestDisconnect) {
            interthread_send = std::move(_interthreadSend);
        }

        _interthreadCommunication = false;
        _interthreadSend = nullptr;
        _interthreadRequestDisconnect = false;

        if (interthread_send) {
            interthread_send({});
        }
    }

    if (_impl->NetSock != INVALID_SOCKET) {
        ::closesocket(_impl->NetSock);
        _impl->NetSock = INVALID_SOCKET;
    }

    if (_impl->ZStreamActive) {
        inflateEnd(&_impl->ZStream);
        _impl->ZStreamActive = false;
    }

    if (_isConnecting) {
        WriteLog("Can't connect to the server");

        _isConnecting = false;
        _connectCallback(false);
    }

    if (_isConnected) {
        WriteLog("Disconnect from server");
        WriteLog("Session statistics:");
        WriteLog("- Sent {} bytes", _bytesSend);
        WriteLog("- Received {} bytes", _bytesReceived);
        WriteLog("- Whole traffic {} bytes", _bytesReceived + _bytesSend);
        WriteLog("- Receive unpacked {} bytes", _bytesRealReceived);

        _netIn.ResetBuf();
        _netOut.ResetBuf();

        _isConnected = false;
        _disconnectCallback();
    }
}

auto ClientConnection::DispatchData() -> bool
{
    STACK_TRACE_ENTRY();

    while (true) {
        if (!_isConnected) {
            break;
        }
        if (_netOut.IsEmpty()) {
            break;
        }
        if (!CheckSocketStatus(true)) {
            break;
        }

        auto* send_buf = _netOut.GetData();
        const auto send_buf_size = _netOut.GetEndPos();

        size_t actual_send;

        if (_interthreadCommunication) {
            _interthreadSend({send_buf, send_buf_size});

            actual_send = send_buf_size;
        }
        else {
#if FO_WINDOWS
            WSABUF buf;
            buf.buf = reinterpret_cast<CHAR*>(send_buf);
            buf.len = numeric_cast<ULONG>(send_buf_size);
            DWORD len;

            if (::WSASend(_impl->NetSock, &buf, 1, &len, 0, nullptr, nullptr) == SOCKET_ERROR || len == 0)
#else
            int len = (int)::send(_impl->NetSock, send_buf, send_buf_size, 0);

            if (len <= 0)
#endif
            {
                throw ClientConnectionException("Socket error while send to server", _impl->GetLastSocketError());
            }

            actual_send = numeric_cast<size_t>(len);
        }

        _netOut.DiscardWriteBuf(actual_send);
        _bytesSend += actual_send;
    }

    if (_netOut.IsEmpty()) {
        _netOut.ResetBuf();
    }

    return _isConnected;
}

auto ClientConnection::ReceiveData(bool unpack) -> bool
{
    STACK_TRACE_ENTRY();

    if (!CheckSocketStatus(false)) {
        return false;
    }

    size_t whole_len;

    if (_interthreadCommunication) {
        auto locker = std::unique_lock {_interthreadReceivedLocker};

        RUNTIME_ASSERT(!_interthreadReceived.empty());
        whole_len = _interthreadReceived.size();
        _incomeBuf = _interthreadReceived;
        _interthreadReceived.clear();
    }
    else {
        RUNTIME_ASSERT(_impl->NetSock != INVALID_SOCKET);

#if FO_WINDOWS
        DWORD flags = 0;
        WSABUF buf;
        buf.buf = reinterpret_cast<CHAR*>(_incomeBuf.data());
        buf.len = numeric_cast<ULONG>(_incomeBuf.size());
        DWORD len;
        if (::WSARecv(_impl->NetSock, &buf, 1, &len, &flags, nullptr, nullptr) == SOCKET_ERROR)
#else
        int len = numeric_cast<int>(::recv(_impl->NetSock, _incomeBuf.data(), _incomeBuf.size(), 0));
        if (len == SOCKET_ERROR)
#endif
        {
            throw ClientConnectionException("Socket error while receive from server", _impl->GetLastSocketError());
        }
        if (len == 0) {
            throw ClientConnectionException("Socket is closed");
        }

        whole_len = len;
        while (whole_len == _incomeBuf.size()) {
            _incomeBuf.resize(_incomeBuf.size() * 2);

#if FO_WINDOWS
            flags = 0;
            buf.buf = reinterpret_cast<CHAR*>(_incomeBuf.data() + whole_len);
            buf.len = numeric_cast<ULONG>(_incomeBuf.size() - whole_len);
            if (::WSARecv(_impl->NetSock, &buf, 1, &len, &flags, nullptr, nullptr) == SOCKET_ERROR)
#else
            len = numeric_cast<int>(::recv(_impl->NetSock, _incomeBuf.data() + whole_len, _incomeBuf.size() - whole_len, 0));
            if (len == SOCKET_ERROR)
#endif
            {
#if FO_WINDOWS
                if (::WSAGetLastError() == WSAEWOULDBLOCK) {
#else
                if (errno == EINPROGRESS) {
#endif
                    break;
                }

                throw ClientConnectionException("Socket error (2) while receive from server", _impl->GetLastSocketError());
            }
            if (len == 0) {
                throw ClientConnectionException("Socket is closed (2)");
            }

            whole_len += len;
        }
    }

    _netIn.ShrinkReadBuf();

    const auto old_pos = _netIn.GetEndPos();

    if (unpack && !_settings.DisableZlibCompression) {
        _impl->ZStream.next_in = static_cast<Bytef*>(_incomeBuf.data());
        _impl->ZStream.avail_in = numeric_cast<uInt>(whole_len);
        _impl->ZStream.next_out = static_cast<Bytef*>(_netIn.GetData() + _netIn.GetEndPos());
        _impl->ZStream.avail_out = numeric_cast<uInt>(_netIn.GetAvailLen());

        const auto first_inflate = ::inflate(&_impl->ZStream, Z_SYNC_FLUSH);
        RUNTIME_ASSERT(first_inflate == Z_OK);

        auto uncompr = reinterpret_cast<size_t>(_impl->ZStream.next_out) - reinterpret_cast<size_t>(_netIn.GetData());
        _netIn.SetEndPos(uncompr);

        while (_impl->ZStream.avail_in != 0) {
            _netIn.GrowBuf(_settings.NetBufferSize);

            _impl->ZStream.next_out = static_cast<Bytef*>(_netIn.GetData() + _netIn.GetEndPos());
            _impl->ZStream.avail_out = numeric_cast<uInt>(_netIn.GetAvailLen());

            const auto next_inflate = ::inflate(&_impl->ZStream, Z_SYNC_FLUSH);
            RUNTIME_ASSERT(next_inflate == Z_OK);

            uncompr = reinterpret_cast<size_t>(_impl->ZStream.next_out) - reinterpret_cast<size_t>(_netIn.GetData());
            _netIn.SetEndPos(uncompr);
        }
    }
    else {
        _netIn.AddData(_incomeBuf.data(), whole_len);
    }

    _bytesReceived += whole_len;
    _bytesRealReceived += _netIn.GetEndPos() - old_pos;

    RUNTIME_ASSERT(_netIn.GetEndPos() > old_pos);

    return _netIn.GetEndPos() - old_pos > 0;
}

void ClientConnection::Impl::FillSockAddr(sockaddr_in& saddr, string_view host, uint16 port) const
{
    STACK_TRACE_ENTRY();

    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);

    if ((saddr.sin_addr.s_addr = ::inet_addr(string(host).c_str())) == static_cast<uint>(-1)) {
        static std::mutex gethostbyname_locker;
        auto locker = std::scoped_lock {gethostbyname_locker};

        const auto* h = ::gethostbyname(string(host).c_str()); // NOLINT(concurrency-mt-unsafe)

        if (h == nullptr) {
            throw ClientConnectionException("Can't resolve remote host", host, GetLastSocketError());
        }

        MemCopy(&saddr.sin_addr, h->h_addr, sizeof(in_addr));
    }
}

auto ClientConnection::Impl::GetLastSocketError() const -> string
{
    STACK_TRACE_ENTRY();

#if FO_WINDOWS
    const auto error_code = ::WSAGetLastError();
    wchar_t* ws = nullptr;
    ::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, //
        nullptr, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&ws), 0, nullptr);
    auto free_ws = ScopeCallback([ws]() noexcept { safe_call([ws] { ::LocalFree(ws); }); });
    const string error_str = strex().parseWideChar(ws).trim();

    return strex("{} ({})", error_str, error_code);

#else
    const string error_str = strex(::strerror(errno)).trim();

    return strex("{} ({})", error_str, errno);
#endif
}

void ClientConnection::Net_SendHandshake()
{
    STACK_TRACE_ENTRY();

    _netOut.StartMsg(NETMSG_HANDSHAKE);
    _netOut.Write(static_cast<uint>(FO_COMPATIBILITY_VERSION));

    const auto encrypt_key = NetBuffer::GenerateEncryptKey();
    _netOut.Write(encrypt_key);

    constexpr uint8 padding[28] = {};
    _netOut.Push(padding, sizeof(padding));
    _netOut.EndMsg();

    _netOut.SetEncryptKey(encrypt_key);
    _netIn.SetEncryptKey(encrypt_key);
}

void ClientConnection::Net_OnPing()
{
    STACK_TRACE_ENTRY();

    const auto answer = _netIn.Read<bool>();

    if (answer) {
        const auto time = Timer::CurTime();
        _settings.Ping = time_duration_to_ms<uint>(time - _pingTime);
        _pingTime = time_point {};
        _pingCallTime = time + std::chrono::milliseconds {_settings.PingPeriod};
    }
    else {
        _netOut.StartMsg(NETMSG_PING);
        _netOut.Write(true);
        _netOut.EndMsg();
    }
}
