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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "ServerConnection.h"
#include "ClientScripting.h"
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

struct ServerConnection::Impl
{
    auto GetLastSocketError() -> string;
    auto FillSockAddr(sockaddr_in& saddr, string_view host, ushort port) -> bool;

    z_stream ZStream {};
    bool ZStreamActive {};
    sockaddr_in SockAddr {};
    sockaddr_in ProxyAddr {};
    SOCKET NetSock {INVALID_SOCKET};
    fd_set NetSockSet {};
};

ServerConnection::ServerConnection(ClientNetworkSettings& settings) :
    _settings {settings}, //
    _impl {new Impl()},
    _netIn(_settings.NetBufferSize),
    _netOut(_settings.NetBufferSize)
{
    STACK_TRACE_ENTRY();

    _incomeBuf.resize(_settings.NetBufferSize);

    AddMessageHandler(NETMSG_DISCONNECT, [this] { Disconnect(); });
    AddMessageHandler(NETMSG_PING, [this] { Net_OnPing(); });
}

ServerConnection::~ServerConnection()
{
    STACK_TRACE_ENTRY();

    if (_impl->NetSock != INVALID_SOCKET) {
        ::closesocket(_impl->NetSock);
    }
    if (_impl->ZStreamActive) {
        ::inflateEnd(&_impl->ZStream);
    }
    delete _impl;
}

void ServerConnection::AddConnectHandler(ConnectCallback handler)
{
    STACK_TRACE_ENTRY();

    _connectCallback = std::move(handler);
}

void ServerConnection::AddDisconnectHandler(DisconnectCallback handler)
{
    STACK_TRACE_ENTRY();

    _disconnectCallback = std::move(handler);
}

void ServerConnection::AddMessageHandler(uint msg, MessageCallback handler)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_handlers.count(msg) == 0u);

    _handlers.emplace(msg, std::move(handler));
}

void ServerConnection::Connect()
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!_isConnected);
    RUNTIME_ASSERT(!_isConnecting);

    if (!ConnectToHost(_settings.ServerHost, static_cast<ushort>(_settings.ServerPort))) {
        if (_connectCallback) {
            _connectCallback(false);
        }
    }
}

void ServerConnection::Process()
{
    STACK_TRACE_ENTRY();

    if (_isConnecting) {
        if (!CheckSocketStatus(true)) {
            return;
        }
    }

    if (_isConnected) {
        if (ReceiveData(true) >= 0) {
            while (_isConnected && _netIn.NeedProcess()) {
                uint msg = 0;
                _netIn >> msg;

                CHECK_SERVER_IN_BUF_ERROR(*this);

                if (_settings.DebugNet) {
                    _msgCount++;
                    WriteLog("{}) Input net message {}", _msgCount, msg);
                }

                const auto it = _handlers.find(msg);
                if (it != _handlers.end()) {
                    it->second();

                    CHECK_SERVER_IN_BUF_ERROR(*this);
                }
                else {
                    WriteLog("No handler for message {}. Disconnect", msg);
                    Disconnect();
                    return;
                }
            }

            if (_interthreadCommunication && _isConnected) {
                RUNTIME_ASSERT(_netIn.GetReadPos() == _netIn.GetEndPos());
            }

            if (_isConnected && _netOut.IsEmpty() && _pingTick == 0.0 && _settings.PingPeriod != 0u && Timer::RealtimeTick() >= _pingCallTick) {
                _netOut << NETMSG_PING;
                _netOut << PING_SERVER;
                _pingTick = Timer::RealtimeTick();
            }

            DispatchData();
        }
        else {
            Disconnect();
        }
    }
}

auto ServerConnection::CheckSocketStatus(bool for_write) -> bool
{
    STACK_TRACE_ENTRY();

    if (_interthreadCommunication) {
        return for_write ? true : !_interthreadReceived.empty();
    }

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

            if (_connectCallback) {
                _connectCallback(true);
            }
        }
        return true;
    }

    if (r == 0) {
        auto error = 0;
#if FO_WINDOWS
        int len = sizeof(error);
#else
        socklen_t len = sizeof(error);
#endif
        if (::getsockopt(_impl->NetSock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &len) != SOCKET_ERROR && error == 0) {
            return false;
        }

        WriteLog("Socket error '{}'", _impl->GetLastSocketError());
    }
    else {
        // Error
        WriteLog("Socket select error '{}'", _impl->GetLastSocketError());
    }

    if (_isConnecting) {
        WriteLog("Can't connect to server");
        _isConnecting = false;

        if (_connectCallback) {
            _connectCallback(false);
        }
    }

    Disconnect();
    return false;
}

auto ServerConnection::ConnectToHost(string_view host, ushort port) -> bool
{
    STACK_TRACE_ENTRY();

    _netIn.SetEncryptKey(0);
    _netOut.SetEncryptKey(0);

    // Init Zlib
    if (!_impl->ZStreamActive) {
        _impl->ZStream.zalloc = [](voidpf, uInt items, uInt size) { return calloc(items, size); };
        _impl->ZStream.zfree = [](voidpf, voidpf address) { free(address); };
        _impl->ZStream.opaque = nullptr;
        const auto inf_init = ::inflateInit(&_impl->ZStream);
        RUNTIME_ASSERT(inf_init == Z_OK);
        _impl->ZStreamActive = true;
    }

    // First try interthread communication
    if (InterthreadListeners.count(port) != 0u) {
        _interthreadReceived.clear();

        _interthreadSend = InterthreadListeners[port]([this](const_span<uchar> buf) {
            if (!buf.empty()) {
                _interthreadReceived.insert(_interthreadReceived.end(), buf.begin(), buf.end());
            }
            else {
                _interthreadSend = nullptr;
                Disconnect();
            }
        });

        _interthreadCommunication = true;

        WriteLog("Connected to server via interthread communication");

        _isConnecting = false;
        _isConnected = true;

        Net_SendHandshake();

        if (_connectCallback) {
            _connectCallback(true);
        }

        return true;
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
        WriteLog("WSAStartup error '{}'", _impl->GetLastSocketError());
        return false;
    }
#endif

    if (!_impl->FillSockAddr(_impl->SockAddr, host, port)) {
        return false;
    }
    if (_settings.ProxyType != 0u && !_impl->FillSockAddr(_impl->ProxyAddr, _settings.ProxyHost, static_cast<ushort>(_settings.ProxyPort))) {
        return false;
    }

#if FO_LINUX
    constexpr auto sock_type = SOCK_STREAM | SOCK_CLOEXEC;
#else
    constexpr auto sock_type = SOCK_STREAM;
#endif
    if ((_impl->NetSock = ::socket(PF_INET, sock_type, IPPROTO_TCP)) == INVALID_SOCKET) {
        WriteLog("Create socket error '{}'", _impl->GetLastSocketError());
        return false;
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
    if (_settings.ProxyType == 0u) {
        // Set non blocking mode
#if FO_WINDOWS
        unsigned long mode = 1;
        if (::ioctlsocket(_impl->NetSock, FIONBIO, &mode) != 0)
#else
        int flags = ::fcntl(_impl->NetSock, F_GETFL, 0);
        RUNTIME_ASSERT(flags >= 0);
        if (::fcntl(_impl->NetSock, F_SETFL, flags | O_NONBLOCK))
#endif
        {
            WriteLog("Can't set non-blocking mode to socket, error '{}'", _impl->GetLastSocketError());
            return false;
        }

        const auto r = ::connect(_impl->NetSock, reinterpret_cast<sockaddr*>(&_impl->SockAddr), sizeof(sockaddr_in));
#if FO_WINDOWS
        if (r == SOCKET_ERROR && ::WSAGetLastError() != WSAEWOULDBLOCK)
#else
        if (r == SOCKET_ERROR && errno != EINPROGRESS)
#endif
        {
            WriteLog("Can't connect to game server, error '{}'", _impl->GetLastSocketError());
            return false;
        }
    }
    else {
#if !FO_IOS && !FO_ANDROID && !FO_WEB
        // Proxy connect
        if (::connect(_impl->NetSock, reinterpret_cast<sockaddr*>(&_impl->ProxyAddr), sizeof(sockaddr_in)) != 0) {
            WriteLog("Can't connect to proxy server, error '{}'", _impl->GetLastSocketError());
            return false;
        }

        auto send_recv = [this]() {
            if (!DispatchData()) {
                WriteLog("Net output error");
                return false;
            }

            const auto tick = Timer::RealtimeTick();
            while (true) {
                const auto receive = ReceiveData(false);
                if (receive > 0) {
                    break;
                }

                if (receive < 0) {
                    WriteLog("Net input error");
                    return false;
                }

                if (Timer::RealtimeTick() - tick >= 10000.0) {
                    WriteLog("Proxy answer timeout");
                    return false;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            return true;
        };

        uchar b1 = 0;
        uchar b2 = 0;
        _netIn.ResetBuf();
        _netOut.ResetBuf();

        // Authentication
        if (_settings.ProxyType == PROXY_SOCKS4) {
            // Connect
            _netOut << static_cast<uchar>(4); // Socks version
            _netOut << static_cast<uchar>(1); // Connect command
            _netOut << static_cast<ushort>(_impl->SockAddr.sin_port);
            _netOut << static_cast<uint>(_impl->SockAddr.sin_addr.s_addr);
            _netOut << static_cast<uchar>(0);

            if (!send_recv()) {
                return false;
            }

            _netIn >> b1; // Null byte
            _netIn >> b2; // Answer code
            if (b2 != 0x5A) {
                switch (b2) {
                case 0x5B:
                    WriteLog("Proxy connection error, request rejected or failed");
                    break;
                case 0x5C:
                    WriteLog("Proxy connection error, request failed because client is not running identd (or not reachable from the server)");
                    break;
                case 0x5D:
                    WriteLog("Proxy connection error, request failed because client's identd could not confirm the user ID string in the request");
                    break;
                default:
                    WriteLog("Proxy connection error, Unknown error {}", b2);
                    break;
                }
                return false;
            }
        }
        else if (_settings.ProxyType == PROXY_SOCKS5) {
            _netOut << static_cast<uchar>(5); // Socks version
            _netOut << static_cast<uchar>(1); // Count methods
            _netOut << static_cast<uchar>(2); // Method

            if (!send_recv()) {
                return false;
            }

            _netIn >> b1; // Socks version
            _netIn >> b2; // Method
            if (b2 == 2) // User/Password
            {
                _netOut << static_cast<uchar>(1); // Subnegotiation version
                _netOut << static_cast<uchar>(_settings.ProxyUser.length()); // Name length
                _netOut.Push(_settings.ProxyUser.c_str(), _settings.ProxyUser.length()); // Name
                _netOut << static_cast<uchar>(_settings.ProxyPass.length()); // Pass length
                _netOut.Push(_settings.ProxyPass.c_str(), _settings.ProxyPass.length()); // Pass

                if (!send_recv()) {
                    return false;
                }

                _netIn >> b1; // Subnegotiation version
                _netIn >> b2; // Status
                if (b2 != 0) {
                    WriteLog("Invalid proxy user or password");
                    return false;
                }
            }
            else if (b2 != 0) // Other authorization
            {
                WriteLog("Socks server connect fail");
                return false;
            }

            // Connect
            _netOut << static_cast<uchar>(5); // Socks version
            _netOut << static_cast<uchar>(1); // Connect command
            _netOut << static_cast<uchar>(0); // Reserved
            _netOut << static_cast<uchar>(1); // IP v4 address
            _netOut << static_cast<uint>(_impl->SockAddr.sin_addr.s_addr);
            _netOut << static_cast<ushort>(_impl->SockAddr.sin_port);

            if (!send_recv()) {
                return false;
            }

            _netIn >> b1; // Socks version
            _netIn >> b2; // Answer code

            if (b2 != 0) {
                switch (b2) {
                case 1:
                    WriteLog("Proxy connection error, SOCKS-server error");
                    break;
                case 2:
                    WriteLog("Proxy connection error, connections fail by proxy rules");
                    break;
                case 3:
                    WriteLog("Proxy connection error, network is not aviable");
                    break;
                case 4:
                    WriteLog("Proxy connection error, host is not aviable");
                    break;
                case 5:
                    WriteLog("Proxy connection error, connection denied");
                    break;
                case 6:
                    WriteLog("Proxy connection error, TTL expired");
                    break;
                case 7:
                    WriteLog("Proxy connection error, command not supported");
                    break;
                case 8:
                    WriteLog("Proxy connection error, address type not supported");
                    break;
                default:
                    WriteLog("Proxy connection error, unknown error {}", b2);
                    break;
                }
                return false;
            }
        }
        else if (_settings.ProxyType == PROXY_HTTP) {
            string buf = _str("CONNECT {}:{} HTTP/1.0\r\n\r\n", ::inet_ntoa(_impl->SockAddr.sin_addr), port);
            _netOut.Push(buf.c_str(), buf.length());

            if (!send_recv()) {
                return false;
            }

            buf = reinterpret_cast<const char*>(_netIn.GetData() + _netIn.GetReadPos());
            if (buf.find(" 200 ") == string::npos) {
                WriteLog("Proxy connection error, receive message '{}'", buf);
                return false;
            }
        }
        else {
            WriteLog("Unknown proxy type {}", _settings.ProxyType);
            return false;
        }

        _netIn.ResetBuf();
        _netOut.ResetBuf();

#else
        throw GenericException("Proxy connection is not supported on this platform");
#endif
    }

    _isConnecting = true;
    return true;
}

void ServerConnection::Disconnect()
{
    STACK_TRACE_ENTRY();

    if (_interthreadCommunication) {
        _interthreadCommunication = false;

        if (_interthreadSend) {
            _interthreadSend({});
            _interthreadSend = nullptr;
        }
    }

    if (_impl->NetSock != INVALID_SOCKET) {
        ::closesocket(_impl->NetSock);
        _impl->NetSock = INVALID_SOCKET;
    }

    if (_impl->ZStreamActive) {
        ::inflateEnd(&_impl->ZStream);
        _impl->ZStreamActive = false;
    }

    if (_isConnecting) {
        _isConnecting = false;

        if (_connectCallback) {
            _connectCallback(false);
        }
    }

    if (_isConnected) {
        WriteLog("Disconnect. Session traffic: send {}, receive {}, whole {}, receive real {}", _bytesSend, _bytesReceived, _bytesReceived + _bytesSend, _bytesRealReceived);

        _isConnected = false;

        _netIn.ResetBuf();
        _netOut.ResetBuf();
        _netIn.SetEncryptKey(0);
        _netOut.SetEncryptKey(0);
        _netIn.SetError(false);
        _netOut.SetError(false);

        if (_disconnectCallback) {
            _disconnectCallback();
        }
    }
}

auto ServerConnection::DispatchData() -> bool
{
    STACK_TRACE_ENTRY();

    if (!_isConnected) {
        return false;
    }
    if (_netOut.IsEmpty()) {
        return true;
    }
    if (!CheckSocketStatus(true)) {
        return _isConnected;
    }

    const auto tosend = _netOut.GetEndPos();
    size_t sendpos = 0;
    while (sendpos < tosend) {
        if (_interthreadCommunication) {
            _interthreadSend({_netOut.GetData(), tosend});
            sendpos += tosend;
            _bytesSend += tosend;
        }
        else {
#if FO_WINDOWS
            WSABUF buf;
            buf.buf = reinterpret_cast<CHAR*>(_netOut.GetData() + sendpos);
            buf.len = static_cast<ULONG>(tosend - sendpos);
            DWORD len;
            if (::WSASend(_impl->NetSock, &buf, 1, &len, 0, nullptr, nullptr) == SOCKET_ERROR || len == 0)
#else
            int len = (int)::send(_impl->NetSock, _netOut.GetData() + sendpos, tosend - sendpos, 0);
            if (len <= 0)
#endif
            {
                WriteLog("Socket error while send to server, error '{}'", _impl->GetLastSocketError());
                Disconnect();
                return false;
            }

            sendpos += len;
            _bytesSend += len;
        }
    }

    _netOut.ResetBuf();
    return true;
}

auto ServerConnection::ReceiveData(bool unpack) -> int
{
    STACK_TRACE_ENTRY();

    if (!CheckSocketStatus(false)) {
        return 0;
    }

    size_t whole_len;

    if (_interthreadCommunication) {
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
        buf.len = static_cast<ULONG>(_incomeBuf.size());
        DWORD len;
        if (::WSARecv(_impl->NetSock, &buf, 1, &len, &flags, nullptr, nullptr) == SOCKET_ERROR)
#else
        int len = static_cast<int>(::recv(_impl->NetSock, _incomeBuf.data(), _incomeBuf.size(), 0));
        if (len == SOCKET_ERROR)
#endif
        {
            WriteLog("Socket error while receive from server, error '{}'", _impl->GetLastSocketError());
            return -1;
        }
        if (len == 0) {
            WriteLog("Socket is closed");
            return -2;
        }

        whole_len = len;
        while (whole_len == _incomeBuf.size()) {
            _incomeBuf.resize(_incomeBuf.size() * 2);

#if FO_WINDOWS
            flags = 0;
            buf.buf = reinterpret_cast<CHAR*>(_incomeBuf.data() + whole_len);
            buf.len = static_cast<ULONG>(_incomeBuf.size() - whole_len);
            if (::WSARecv(_impl->NetSock, &buf, 1, &len, &flags, nullptr, nullptr) == SOCKET_ERROR)
#else
            len = static_cast<int>(::recv(_impl->NetSock, _incomeBuf.data() + whole_len, _incomeBuf.size() - whole_len, 0));
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

                WriteLog("Socket error (2) while receive from server, error '{}'", _impl->GetLastSocketError());
                return -1;
            }
            if (len == 0) {
                WriteLog("Socket is closed (2)");
                return -2;
            }

            whole_len += len;
        }
    }

    _netIn.ShrinkReadBuf();

    const auto old_pos = _netIn.GetEndPos();

    if (unpack && !_settings.DisableZlibCompression) {
        _impl->ZStream.next_in = static_cast<Bytef*>(_incomeBuf.data());
        _impl->ZStream.avail_in = static_cast<uInt>(whole_len);
        _impl->ZStream.next_out = static_cast<Bytef*>(_netIn.GetData() + _netIn.GetEndPos());
        _impl->ZStream.avail_out = static_cast<uInt>(_netIn.GetAvailLen());

        const auto first_inflate = ::inflate(&_impl->ZStream, Z_SYNC_FLUSH);
        RUNTIME_ASSERT(first_inflate == Z_OK);

        auto uncompr = reinterpret_cast<size_t>(_impl->ZStream.next_out) - reinterpret_cast<size_t>(_netIn.GetData());
        _netIn.SetEndPos(uncompr);

        while (_impl->ZStream.avail_in != 0) {
            _netIn.GrowBuf(_settings.NetBufferSize);

            _impl->ZStream.next_out = static_cast<Bytef*>(_netIn.GetData() + _netIn.GetEndPos());
            _impl->ZStream.avail_out = static_cast<uInt>(_netIn.GetAvailLen());

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

    return static_cast<int>(_netIn.GetEndPos() - old_pos);
}

auto ServerConnection::Impl::FillSockAddr(sockaddr_in& saddr, string_view host, ushort port) -> bool
{
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    if ((saddr.sin_addr.s_addr = ::inet_addr(string(host).c_str())) == static_cast<uint>(-1)) {
        const auto* h = ::gethostbyname(string(host).c_str());
        if (h == nullptr) {
            WriteLog("Can't resolve remote host '{}', error '{}'.", host, GetLastSocketError());
            return false;
        }

        std::memcpy(&saddr.sin_addr, h->h_addr, sizeof(in_addr));
    }
    return true;
}

auto ServerConnection::Impl::GetLastSocketError() -> string
{
#if FO_WINDOWS
    const auto error_code = ::WSAGetLastError();
    wchar_t* ws = nullptr;
    ::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&ws), 0, nullptr);
    const string error_str = _str().parseWideChar(ws);
    ::LocalFree(ws);

    return _str("{} ({})", error_str, error_code);
#else
    return _str("{} ({})", strerror(errno), errno);
#endif
}

void ServerConnection::Net_SendHandshake()
{
    _netOut << NETMSG_HANDSHAKE;
    _netOut << static_cast<uint>(FO_COMPATIBILITY_VERSION);

    const auto encrypt_key = NetBuffer::GenerateEncryptKey();
    _netOut << encrypt_key;
    _netOut.SetEncryptKey(encrypt_key);
    _netIn.SetEncryptKey(encrypt_key);

    constexpr uchar padding[28] = {};
    _netOut.Push(padding, sizeof(padding));
}

void ServerConnection::Net_OnPing()
{
    uchar ping;
    _netIn >> ping;

    CHECK_SERVER_IN_BUF_ERROR(*this);

    if (ping == PING_CLIENT) {
        _netOut << NETMSG_PING;
        _netOut << PING_CLIENT;
    }
    else if (ping == PING_SERVER) {
        const auto cur_tick = Timer::RealtimeTick();
        _settings.Ping = iround(cur_tick - _pingTick);
        _pingTick = 0.0;
        _pingCallTick = cur_tick + _settings.PingPeriod;
    }
}
