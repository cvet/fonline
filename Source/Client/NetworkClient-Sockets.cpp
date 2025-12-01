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

#include "NetworkClient.h"

#include "WinApi-Include.h"

#if !FO_WINDOWS
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#define SOCKET int32
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define closesocket close
#define SD_RECEIVE SHUT_RD
#define SD_SEND SHUT_WR
#define SD_BOTH SHUT_RDWR
#endif

FO_BEGIN_NAMESPACE();

// Proxy types
constexpr auto PROXY_SOCKS4 = 1;
constexpr auto PROXY_SOCKS5 = 2;
constexpr auto PROXY_HTTP = 3;

class NetworkClientConnection_Sockets final : public NetworkClientConnection
{
public:
    explicit NetworkClientConnection_Sockets(ClientNetworkSettings& settings);
    NetworkClientConnection_Sockets(const NetworkClientConnection_Sockets&) = delete;
    NetworkClientConnection_Sockets(NetworkClientConnection_Sockets&&) noexcept = delete;
    auto operator=(const NetworkClientConnection_Sockets&) = delete;
    auto operator=(NetworkClientConnection_Sockets&&) noexcept = delete;
    ~NetworkClientConnection_Sockets() override;

protected:
    auto CheckStatusImpl(bool for_write) -> bool override;
    auto SendDataImpl(span<const uint8> buf) -> size_t override;
    auto ReceiveDataImpl(vector<uint8>& buf) -> size_t override;
    void DisconnectImpl() noexcept override;

private:
    void FillSockAddr(sockaddr_in& saddr, string_view host, uint16 port) const;
    auto GetLastSocketError() const -> string;

    sockaddr_in _sockAddr {};
    sockaddr_in _proxyAddr {};
    SOCKET _netSock {INVALID_SOCKET};
    fd_set _netSockSet {};
};

auto NetworkClientConnection::CreateSocketsConnection(ClientNetworkSettings& settings) -> unique_ptr<NetworkClientConnection>
{
    FO_STACK_TRACE_ENTRY();

    return SafeAlloc::MakeUnique<NetworkClientConnection_Sockets>(settings);
}

NetworkClientConnection_Sockets::NetworkClientConnection_Sockets(ClientNetworkSettings& settings) :
    NetworkClientConnection(settings)
{
    FO_STACK_TRACE_ENTRY();

    string_view host = _settings->ServerHost;
    auto port = numeric_cast<uint16>(_settings->ServerPort);

#if FO_WEB
    port++;

    if (!_settings->SecuredWebSockets) {
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
        throw NetworkClientException("WSAStartup error", GetLastSocketError());
    }
#endif

    FillSockAddr(_sockAddr, host, port);

    if (_settings->ProxyType != 0) {
        FillSockAddr(_proxyAddr, _settings->ProxyHost, numeric_cast<uint16>(_settings->ProxyPort));
    }

#if FO_LINUX
    constexpr auto sock_type = SOCK_STREAM | SOCK_CLOEXEC;
#else
    constexpr auto sock_type = SOCK_STREAM;
#endif

    if ((_netSock = ::socket(PF_INET, sock_type, IPPROTO_TCP)) == INVALID_SOCKET) {
        throw NetworkClientException("Create socket error", GetLastSocketError());
    }

    // Nagle
#if !FO_WEB
    if (_settings->DisableTcpNagle) {
        auto optval = 1;

        if (::setsockopt(_netSock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&optval), sizeof(optval)) != 0) {
            WriteLog("Can't set TCP_NODELAY (disable Nagle) to socket, error '{}'", GetLastSocketError());
        }
    }
#endif

    // Direct connect
    if (_settings->ProxyType == 0) {
        // Set non blocking mode
#if FO_WINDOWS
        u_long mode = 1;
        if (::ioctlsocket(_netSock, FIONBIO, &mode) != 0) {
            throw NetworkClientException("Can't set non-blocking mode to socket", GetLastSocketError());
        }

#else
        int32 flags = ::fcntl(_netSock, F_GETFL, 0);
        FO_RUNTIME_ASSERT(flags >= 0);

        if (::fcntl(_netSock, F_SETFL, flags | O_NONBLOCK)) {
            throw NetworkClientException("Can't set non-blocking mode to socket", GetLastSocketError());
        }
#endif

        const auto r = ::connect(_netSock, reinterpret_cast<sockaddr*>(&_sockAddr), sizeof(sockaddr_in));
#if FO_WINDOWS
        if (r == SOCKET_ERROR && ::WSAGetLastError() != WSAEWOULDBLOCK) {
            throw NetworkClientException("Can't connect to the game server", GetLastSocketError());
        }
#else
        if (r == SOCKET_ERROR && errno != EINPROGRESS) {
            throw NetworkClientException("Can't connect to the game server", GetLastSocketError());
        }
#endif
    }
    else {
#if !FO_IOS && !FO_ANDROID && !FO_WEB
        // Proxy connect
        if (::connect(_netSock, reinterpret_cast<sockaddr*>(&_proxyAddr), sizeof(sockaddr_in)) != 0) {
            throw NetworkClientException("Can't connect to proxy server", GetLastSocketError());
        }

        auto send_recv = [this](span<const uint8> buf) -> vector<uint8> {
            if (!SendData(buf)) {
                throw NetworkClientException("Net output error");
            }

            const auto time = nanotime::now();

            while (true) {
                if (CheckStatus(false)) {
                    const auto result_buf = ReceiveData();
                    return vector<uint8>(result_buf.begin(), result_buf.end());
                }

                if (nanotime::now() - time >= std::chrono::milliseconds {10000}) {
                    throw NetworkClientException("Proxy answer timeout");
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        };

        vector<uint8> send_buf;
        vector<uint8> recv_buf;
        uint8 b1 = 0;
        uint8 b2 = 0;
        ignore_unused(b1);

        // Authentication
        if (_settings->ProxyType == PROXY_SOCKS4) {
            // Connect
            auto writer = DataWriter(send_buf);
            writer.Write<uint8>(numeric_cast<uint8>(4)); // Socks version
            writer.Write<uint8>(numeric_cast<uint8>(1)); // Connect command
            writer.Write<uint16>(numeric_cast<uint16>(_sockAddr.sin_port));
            writer.Write<uint32>(numeric_cast<uint32>(_sockAddr.sin_addr.s_addr));
            writer.Write<uint8>(numeric_cast<uint8>(0));

            recv_buf = send_recv(send_buf);

            auto reader = DataReader(recv_buf);
            b1 = reader.Read<uint8>(); // Null byte
            b2 = reader.Read<uint8>(); // Answer code

            if (b2 != 0x5A) {
                switch (b2) {
                case 0x5B:
                    throw NetworkClientException("Proxy connection error, request rejected or failed");
                case 0x5C:
                    throw NetworkClientException("Proxy connection error, request failed because client is not running identd (or not reachable from the server)");
                case 0x5D:
                    throw NetworkClientException("Proxy connection error, request failed because client's identd could not confirm the user ID string in the request");
                default:
                    throw NetworkClientException("Proxy connection error, unknown error", b2);
                }
            }
        }
        else if (_settings->ProxyType == PROXY_SOCKS5) {
            auto writer = DataWriter(send_buf);
            writer.Write<uint8>(numeric_cast<uint8>(5)); // Socks version
            writer.Write<uint8>(numeric_cast<uint8>(1)); // Count methods
            writer.Write<uint8>(numeric_cast<uint8>(2)); // Method

            recv_buf = send_recv(send_buf);

            auto reader = DataReader(recv_buf);
            b1 = reader.Read<uint8>(); // Socks version
            b2 = reader.Read<uint8>(); // Method

            if (b2 == 2) { // User/Password
                send_buf.clear();
                writer = DataWriter(send_buf);
                writer.Write<uint8>(numeric_cast<uint8>(1)); // Subnegotiation version
                writer.Write<uint8>(numeric_cast<uint8>(_settings->ProxyUser.length())); // Name length
                writer.WritePtr(_settings->ProxyUser.c_str(), _settings->ProxyUser.length()); // Name
                writer.Write<uint8>(numeric_cast<uint8>(_settings->ProxyPass.length())); // Pass length
                writer.WritePtr(_settings->ProxyPass.c_str(), _settings->ProxyPass.length()); // Pass

                recv_buf = send_recv(send_buf);

                reader = DataReader(recv_buf);
                b1 = reader.Read<uint8>(); // Subnegotiation version
                b2 = reader.Read<uint8>(); // Status

                if (b2 != 0) {
                    throw NetworkClientException("Invalid proxy user or password");
                }
            }
            else if (b2 != 0) { // Other authorization
                throw NetworkClientException("Socks server connect fail");
            }

            // Connect
            send_buf.clear();
            writer = DataWriter(send_buf);
            writer.Write<uint8>(numeric_cast<uint8>(5)); // Socks version
            writer.Write<uint8>(numeric_cast<uint8>(1)); // Connect command
            writer.Write<uint8>(numeric_cast<uint8>(0)); // Reserved
            writer.Write<uint8>(numeric_cast<uint8>(1)); // IP v4 address
            writer.Write<uint32>(numeric_cast<uint32>(_sockAddr.sin_addr.s_addr));
            writer.Write<uint16>(numeric_cast<uint16>(_sockAddr.sin_port));

            recv_buf = send_recv(send_buf);

            reader = DataReader(recv_buf);
            b1 = reader.Read<uint8>(); // Socks version
            b2 = reader.Read<uint8>(); // Answer code

            if (b2 != 0) {
                switch (b2) {
                case 1:
                    throw NetworkClientException("Proxy connection error, SOCKS-server error");
                case 2:
                    throw NetworkClientException("Proxy connection error, connections fail by proxy rules");
                case 3:
                    throw NetworkClientException("Proxy connection error, network is not aviable");
                case 4:
                    throw NetworkClientException("Proxy connection error, host is not aviable");
                case 5:
                    throw NetworkClientException("Proxy connection error, connection denied");
                case 6:
                    throw NetworkClientException("Proxy connection error, TTL expired");
                case 7:
                    throw NetworkClientException("Proxy connection error, command not supported");
                case 8:
                    throw NetworkClientException("Proxy connection error, address type not supported");
                default:
                    throw NetworkClientException("Proxy connection error, unknown error", b2);
                }
            }
        }
        else if (_settings->ProxyType == PROXY_HTTP) {
            const string request = strex("CONNECT {}:{} HTTP/1.0\r\n\r\n", ::inet_ntoa(_sockAddr.sin_addr), port); // NOLINT(concurrency-mt-unsafe)
            const auto result = send_recv({reinterpret_cast<const uint8*>(request.data()), request.length()});
            const auto result_str = string(reinterpret_cast<const char*>(result.data()), result.size());

            if (result_str.find(" 200 ") == string::npos) {
                throw NetworkClientException("Proxy connection error", request);
            }
        }
        else {
            throw NetworkClientException("Unknown proxy type", _settings->ProxyType);
        }

#else
        throw NetworkClientException("Proxy connection is not supported on this platform");
#endif
    }
}

NetworkClientConnection_Sockets::~NetworkClientConnection_Sockets()
{
    FO_STACK_TRACE_ENTRY();

    if (_netSock != INVALID_SOCKET) {
        closesocket(_netSock);
        _netSock = INVALID_SOCKET;
    }
}

auto NetworkClientConnection_Sockets::CheckStatusImpl(bool for_write) -> bool
{
    FO_STACK_TRACE_ENTRY();

    // ReSharper disable once CppLocalVariableMayBeConst
    timeval tv = {.tv_sec = 0, .tv_usec = 0};

    FD_ZERO(&_netSockSet);
    FD_SET(_netSock, &_netSockSet);

    const auto r = ::select(numeric_cast<int32>(_netSock) + 1, for_write ? nullptr : &_netSockSet, for_write ? &_netSockSet : nullptr, nullptr, &tv);

    if (r == 1) {
        if (_isConnecting) {
            WriteLog("Connection established");

            _isConnecting = false;
            _isConnected = true;
        }

        return true;
    }

    if (r == 0) {
        int32 error = 0;
#if FO_WINDOWS
        int32 len = sizeof(error);
#else
        socklen_t len = sizeof(error);
#endif

        if (::getsockopt(_netSock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &len) != SOCKET_ERROR && error == 0) {
            return false;
        }

        throw NetworkClientException("Socket error", GetLastSocketError());
    }
    else {
        throw NetworkClientException("Socket select error", GetLastSocketError());
    }
}

auto NetworkClientConnection_Sockets::SendDataImpl(span<const uint8> buf) -> size_t
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    WSABUF sock_buf;
    sock_buf.buf = reinterpret_cast<CHAR*>(const_cast<uint8*>(buf.data()));
    sock_buf.len = numeric_cast<ULONG>(buf.size());
    DWORD len;

    if (::WSASend(_netSock, &sock_buf, 1, &len, 0, nullptr, nullptr) == SOCKET_ERROR || len == 0) {
        throw NetworkClientException("Socket error while send to server", GetLastSocketError());
    }

#else
    const auto len = ::send(_netSock, buf.data(), buf.size(), 0);

    if (len <= 0) {
        throw NetworkClientException("Socket error while send to server", GetLastSocketError());
    }
#endif

    return numeric_cast<size_t>(len);
}

auto NetworkClientConnection_Sockets::ReceiveDataImpl(vector<uint8>& buf) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_netSock != INVALID_SOCKET);

#if FO_WINDOWS
    DWORD flags = 0;
    WSABUF sock_buf;
    sock_buf.buf = reinterpret_cast<CHAR*>(buf.data());
    sock_buf.len = numeric_cast<ULONG>(buf.size());
    DWORD len;

    if (::WSARecv(_netSock, &sock_buf, 1, &len, &flags, nullptr, nullptr) == SOCKET_ERROR) {
        throw NetworkClientException("Socket error while receive from server", GetLastSocketError());
    }

#else
    auto len = ::recv(_netSock, buf.data(), buf.size(), 0);

    if (len == SOCKET_ERROR) {
        throw NetworkClientException("Socket error while receive from server", GetLastSocketError());
    }
#endif

    if (len == 0) {
        throw NetworkClientException("Socket is closed");
    }

    auto whole_len = numeric_cast<size_t>(len);

    while (whole_len == buf.size()) {
        buf.resize(buf.size() * 2);

#if FO_WINDOWS
        flags = 0;
        sock_buf.buf = reinterpret_cast<CHAR*>(buf.data() + whole_len);
        sock_buf.len = numeric_cast<ULONG>(buf.size() - whole_len);

        if (::WSARecv(_netSock, &sock_buf, 1, &len, &flags, nullptr, nullptr) == SOCKET_ERROR) {
            if (::WSAGetLastError() == WSAEWOULDBLOCK) {
                break;
            }

            throw NetworkClientException("Socket error (2) while receive from server", GetLastSocketError());
        }

#else
        len = numeric_cast<int32>(::recv(_netSock, buf.data() + whole_len, buf.size() - whole_len, 0));

        if (len == SOCKET_ERROR) {
            if (errno == EINPROGRESS) {
                break;
            }

            throw NetworkClientException("Socket error (2) while receive from server", GetLastSocketError());
        }
#endif

        if (len == 0) {
            throw NetworkClientException("Socket is closed (2)");
        }

        whole_len += numeric_cast<size_t>(len);
    }

    return whole_len;
}

void NetworkClientConnection_Sockets::DisconnectImpl() noexcept
{
    FO_STACK_TRACE_ENTRY();

    if (_netSock != INVALID_SOCKET) {
        ::closesocket(_netSock);
        _netSock = INVALID_SOCKET;
    }
}

void NetworkClientConnection_Sockets::FillSockAddr(sockaddr_in& saddr, string_view host, uint16 port) const
{
    FO_STACK_TRACE_ENTRY();

    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);

    if ((saddr.sin_addr.s_addr = ::inet_addr(string(host).c_str())) == 0xFFFFFFFFu) {
        static std::mutex gethostbyname_locker;
        auto locker = std::scoped_lock {gethostbyname_locker};

        const auto* h = ::gethostbyname(string(host).c_str()); // NOLINT(concurrency-mt-unsafe)

        if (h == nullptr) {
            throw NetworkClientException("Can't resolve remote host", host, GetLastSocketError());
        }

        MemCopy(&saddr.sin_addr, h->h_addr, sizeof(in_addr));
    }
}

auto NetworkClientConnection_Sockets::GetLastSocketError() const -> string
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    const auto error_code = ::WSAGetLastError();
    wchar_t* ws = nullptr;
    ::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, //
        nullptr, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&ws), 0, nullptr);
    auto free_ws = ScopeCallback([ws]() noexcept { safe_call([ws] { ::LocalFree(ws); }); });
    const string error_str = strex().parse_wide_char(ws).trim();

    return strex("{} ({})", error_str, error_code);

#else
    const string error_str = strex(::strerror(errno)).trim();

    return strex("{} ({})", error_str, errno);
#endif
}

FO_END_NAMESPACE();
