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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "NetSockets.h"
#include "SafeArithmetics.h"
#include "StackTrace.h"
#include "StringUtils.h"

#if FO_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>
#include <Windows.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif
#include "WinApiUndef.inc"

FO_BEGIN_NAMESPACE

constexpr socket_t INVALID_SOCKET_VALUE = static_cast<socket_t>(-1);
constexpr int32_t SOCKET_ERROR_VALUE = static_cast<int32_t>(-1);

static void CloseSocket(socket_t sock) noexcept
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    ::closesocket(sock);
#else
    ::close(sock);
#endif
}

static auto MakeSocketHolder(socket_t sock) -> unique_del_ptr<socket_t>
{
    FO_STACK_TRACE_ENTRY();

    auto socket_holder = SafeAlloc::MakeUnique<socket_t>(sock);
    return make_unique_del_ptr(socket_holder.release(), [](ptr<socket_t> p) {
        auto owned_socket = adopt_unique_ptr(p);
        FO_VERIFY_AND_THROW(*owned_socket != INVALID_SOCKET_VALUE, "Socket handle is invalid");
        CloseSocket(*owned_socket);
    });
}

static auto WaitSocketReady(socket_t sock, bool check_read, bool check_write, timespan timeout) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    fd_set read_set {};
    fd_set write_set {};
    nptr<fd_set> read_ptr = nullptr;
    nptr<fd_set> write_ptr = nullptr;

    if (check_read) {
        FD_ZERO(&read_set);
        FD_SET(sock, &read_set);
        read_ptr = &read_set;
    }

    if (check_write) {
        FD_ZERO(&write_set);
        FD_SET(sock, &write_set);
        write_ptr = &write_set;
    }

    auto timeout_ms = std::max<int64_t>(timeout.milliseconds(), 0);

    timeval timeout_tv {};
    timeout_tv.tv_sec = numeric_cast<decltype(timeout_tv.tv_sec)>(timeout_ms / 1000);
    timeout_tv.tv_usec = numeric_cast<decltype(timeout_tv.tv_usec)>((timeout_ms % 1000) * 1000);

#if FO_WINDOWS
    int32_t select_result = ::select(0, read_ptr.get(), write_ptr.get(), nullptr, &timeout_tv);
#else
    int32_t select_result = ::select(numeric_cast<int32_t>(sock) + 1, read_ptr.get(), write_ptr.get(), nullptr, &timeout_tv);
#endif

    if (select_result <= 0) {
        return false;
    }

    return (read_ptr && FD_ISSET(sock, &read_set)) || (write_ptr && FD_ISSET(sock, &write_set));
}

auto net_sockets::startup() noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    WSADATA wsa {};
    return ::WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
#else
    return true;
#endif
}

auto net_sockets::resolve_ipv4(string_view host) noexcept -> optional<uint32_t>
{
    FO_STACK_TRACE_ENTRY();

    if (host.empty() || host == "0.0.0.0" || host == "*") {
        return numeric_cast<uint32_t>(htonl(INADDR_ANY));
    }

    if (host == "127.0.0.1" || host == "localhost") {
        return numeric_cast<uint32_t>(htonl(INADDR_LOOPBACK));
    }

    string host_name = string(host);
    auto host_name_cstr = make_ptr(host_name.c_str());
    uint32_t addr = numeric_cast<uint32_t>(::inet_addr(host_name_cstr.get()));

    if (addr != INADDR_NONE) {
        return addr;
    }

    static std::mutex gethostbyname_locker;
    auto locker = std::scoped_lock {gethostbyname_locker};

    auto h = make_nptr(::gethostbyname(host_name_cstr.get())); // NOLINT(concurrency-mt-unsafe)

    if (!h) {
        return std::nullopt;
    }

    nptr<const char> resolved_addr = h->h_addr;

    if (!resolved_addr || h->h_length < numeric_cast<int32_t>(sizeof(addr))) {
        return std::nullopt;
    }

    MemCopy(&addr, resolved_addr, sizeof(addr));
    return addr;
}

auto net_sockets::ipv4_to_string(uint32_t addr_net_order) noexcept -> string
{
    FO_STACK_TRACE_ENTRY();

    uint32_t addr_host_order = numeric_cast<uint32_t>(ntohl(addr_net_order));

    return strex("{}.{}.{}.{}", //
        numeric_cast<uint8_t>((addr_host_order >> 24) & 0xFF), //
        numeric_cast<uint8_t>((addr_host_order >> 16) & 0xFF), //
        numeric_cast<uint8_t>((addr_host_order >> 8) & 0xFF), //
        numeric_cast<uint8_t>(addr_host_order & 0xFF))
        .str();
}

auto net_sockets::host_to_net_u16(uint16_t value) noexcept -> uint16_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return htons(value);
}

auto net_sockets::last_recv_was_would_block() noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    return ::WSAGetLastError() == WSAEWOULDBLOCK;
#else
    return errno == EAGAIN || errno == EWOULDBLOCK;
#endif
}

auto net_sockets::error_text(const std::error_code& error) noexcept -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    string_view description;

#if FO_WINDOWS
    switch (error.value()) {
    case WSAEACCES:
        description = "Permission denied";
        break;
    case WSAEADDRINUSE:
        description = "Address already in use";
        break;
    case WSAEADDRNOTAVAIL:
        description = "Address not available";
        break;
    case WSAECONNABORTED:
        description = "Connection aborted";
        break;
    case WSAECONNREFUSED:
        description = "Connection refused";
        break;
    case WSAECONNRESET:
        description = "Connection reset";
        break;
    case WSAEHOSTUNREACH:
        description = "Host unreachable";
        break;
    case WSAENETDOWN:
        description = "Network is down";
        break;
    case WSAENETUNREACH:
        description = "Network unreachable";
        break;
    case WSAENOTCONN:
        description = "Socket is not connected";
        break;
    case WSAETIMEDOUT:
        description = "Operation timed out";
        break;
    case WSAEWOULDBLOCK:
        description = "Operation would block";
        break;
    default:
        break;
    }
#endif

    if (description.empty()) {
        auto condition = error.default_error_condition();

        if (condition == std::errc::permission_denied) {
            description = "Permission denied";
        }
        else if (condition == std::errc::address_in_use) {
            description = "Address already in use";
        }
        else if (condition == std::errc::address_not_available) {
            description = "Address not available";
        }
        else if (condition == std::errc::connection_aborted) {
            description = "Connection aborted";
        }
        else if (condition == std::errc::connection_refused) {
            description = "Connection refused";
        }
        else if (condition == std::errc::connection_reset) {
            description = "Connection reset";
        }
        else if (condition == std::errc::host_unreachable) {
            description = "Host unreachable";
        }
        else if (condition == std::errc::network_down) {
            description = "Network is down";
        }
        else if (condition == std::errc::network_unreachable) {
            description = "Network unreachable";
        }
        else if (condition == std::errc::not_connected) {
            description = "Socket is not connected";
        }
        else if (condition == std::errc::operation_canceled) {
            description = "Operation canceled";
        }
        else if (condition == std::errc::timed_out) {
            description = "Operation timed out";
        }
        else if (condition == std::errc::operation_would_block) {
            description = "Operation would block";
        }
    }

    string_view category = error.category().name();
    return description.empty() ? strex("Network error ({}:{})", category, error.value()) : strex("{} ({}:{})", description, category, error.value());
}

auto net_sockets::last_error_text() noexcept -> string
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    return error_text(std::error_code {::WSAGetLastError(), std::system_category()});
#else
    return error_text(std::error_code {errno, std::system_category()});
#endif
}

tcp_socket::tcp_socket(socket_t sock) noexcept :
    _sock {MakeSocketHolder(sock)}
{
    FO_STACK_TRACE_ENTRY();
}

auto tcp_socket::connect(string_view host, uint16_t port) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    close();

    socket_t sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (sock == INVALID_SOCKET_VALUE) {
        return false;
    }

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    auto resolved = net_sockets::resolve_ipv4(host);

    if (!resolved.has_value()) {
        CloseSocket(sock);
        return false;
    }

    addr.sin_addr.s_addr = *resolved;
    auto addr_ptr = make_ptr(&addr).reinterpret_as<const sockaddr>();

    if (::connect(sock, addr_ptr.get(), sizeof(addr)) == SOCKET_ERROR_VALUE) {
        CloseSocket(sock);
        return false;
    }

    _sock = MakeSocketHolder(sock);

    return true;
}

auto tcp_socket::connect_async(string_view host, uint16_t port) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    close();

    socket_t sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (sock == INVALID_SOCKET_VALUE) {
        return false;
    }

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    auto resolved = net_sockets::resolve_ipv4(host);

    if (!resolved.has_value()) {
        CloseSocket(sock);
        return false;
    }

    addr.sin_addr.s_addr = *resolved;

    // Switch to non-blocking before kicking off connect so we can return immediately on EWOULDBLOCK/EINPROGRESS.
#if FO_WINDOWS
    u_long mode = 1;

    if (::ioctlsocket(sock, FIONBIO, &mode) != 0) {
        CloseSocket(sock);
        return false;
    }
#else
    const int32_t flags = ::fcntl(sock, F_GETFL, 0);

    if (flags < 0 || ::fcntl(sock, F_SETFL, flags | O_NONBLOCK) != 0) {
        CloseSocket(sock);
        return false;
    }
#endif

    auto addr_ptr = make_ptr(&addr).reinterpret_as<const sockaddr>();
    int32_t r = ::connect(sock, addr_ptr.get(), sizeof(addr));

    if (r == SOCKET_ERROR_VALUE) {
#if FO_WINDOWS
        if (::WSAGetLastError() != WSAEWOULDBLOCK) {
            CloseSocket(sock);
            return false;
        }
#else
        if (errno != EINPROGRESS) {
            CloseSocket(sock);
            return false;
        }
#endif
    }

    _sock = MakeSocketHolder(sock);

    return true;
}

auto tcp_socket::can_read(timespan timeout) const noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!is_valid()) {
        return false;
    }

    return WaitSocketReady(*_sock, true, false, timeout);
}

auto tcp_socket::can_write(timespan timeout) const noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!is_valid()) {
        return false;
    }

    return WaitSocketReady(*_sock, false, true, timeout);
}

auto tcp_socket::set_nodelay(bool enabled) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!is_valid()) {
        return false;
    }

    int32_t optval = enabled ? 1 : 0;
    auto optval_data = make_ptr(&optval).reinterpret_as<const char>();

    return ::setsockopt(*_sock, IPPROTO_TCP, TCP_NODELAY, optval_data.get(), sizeof(optval)) == 0;
}

auto tcp_socket::peek_socket_error() const noexcept -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    if (!is_valid()) {
        return -1;
    }

    int32_t error = 0;
#if FO_WINDOWS
    int32_t len = sizeof(error);
#else
    socklen_t len = sizeof(error);
#endif

    auto error_data = make_ptr(&error).reinterpret_as<char>();

    if (::getsockopt(*_sock, SOL_SOCKET, SO_ERROR, error_data.get(), &len) != 0) {
        return -1;
    }

    return error;
}

auto tcp_socket::send(const_span<uint8_t> data) noexcept -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    if (!is_valid() || data.empty()) {
        return 0;
    }

    return ::send(*_sock, make_ptr(data.data()).reinterpret_as<const char>().get(), numeric_cast<int32_t>(data.size()), 0);
}

auto tcp_socket::receive(span<uint8_t> data) noexcept -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    if (!is_valid() || data.empty()) {
        return 0;
    }

    return ::recv(*_sock, make_ptr(data.data()).reinterpret_as<char>().get(), numeric_cast<int32_t>(data.size()), 0);
}

void tcp_socket::close() noexcept
{
    FO_STACK_TRACE_ENTRY();

    _sock.reset();
}

auto tcp_server::listen(string_view bind_host, uint16_t port, int32_t backlog) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    close();

    socket_t sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (sock == INVALID_SOCKET_VALUE) {
        return false;
    }

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    auto resolved = net_sockets::resolve_ipv4(bind_host);

    if (!resolved.has_value()) {
        CloseSocket(sock);
        return false;
    }

    addr.sin_addr.s_addr = *resolved;

    auto addr_ptr = make_ptr(&addr).reinterpret_as<const sockaddr>();

    if (::bind(sock, addr_ptr.get(), sizeof(addr)) == SOCKET_ERROR_VALUE) {
        CloseSocket(sock);
        return false;
    }

    if (::listen(sock, backlog > 0 ? backlog : 1) == SOCKET_ERROR_VALUE) {
        CloseSocket(sock);
        return false;
    }

    _listenSock = MakeSocketHolder(sock);

    return true;
}

auto tcp_server::can_accept(timespan timeout) const noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!is_valid()) {
        return false;
    }

    return WaitSocketReady(*_listenSock, true, false, timeout);
}

auto tcp_server::accept() noexcept -> tcp_socket
{
    FO_STACK_TRACE_ENTRY();

    if (!is_valid()) {
        return {};
    }

    sockaddr_in addr {};
#if FO_WINDOWS
    int32_t addr_len = sizeof(addr);
#else
    socklen_t addr_len = sizeof(addr);
#endif
    auto addr_len_ptr = make_ptr(&addr_len);
    auto addr_ptr = make_ptr(&addr).reinterpret_as<sockaddr>();
    socket_t client_sock = ::accept(*_listenSock, addr_ptr.get(), addr_len_ptr.get());

    if (client_sock == INVALID_SOCKET_VALUE) {
        return {};
    }

    return tcp_socket {client_sock};
}

void tcp_server::close() noexcept
{
    FO_STACK_TRACE_ENTRY();

    _listenSock.reset();
}

auto udp_socket::bind(string_view bind_host, uint16_t port, bool reuse_addr) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    close();

    socket_t sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (sock == INVALID_SOCKET_VALUE) {
        return false;
    }

#if !FO_WEB
    if (reuse_addr) {
        constexpr int32_t opt = 1;
        auto opt_data = make_ptr(&opt).reinterpret_as<const char>();

        if (::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, opt_data.get(), sizeof(opt)) == SOCKET_ERROR_VALUE) {
            CloseSocket(sock);
            return false;
        }
    }
#else
    ignore_unused(reuse_addr);
#endif

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    auto resolved = net_sockets::resolve_ipv4(bind_host);

    if (!resolved.has_value()) {
        CloseSocket(sock);
        return false;
    }

    addr.sin_addr.s_addr = *resolved;

    auto addr_ptr = make_ptr(&addr).reinterpret_as<const sockaddr>();

    if (::bind(sock, addr_ptr.get(), sizeof(addr)) == SOCKET_ERROR_VALUE) {
        CloseSocket(sock);
        return false;
    }

    _sock = MakeSocketHolder(sock);

    return true;
}

auto udp_socket::can_read(timespan timeout) const noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!is_valid()) {
        return false;
    }

    return WaitSocketReady(*_sock, true, false, timeout);
}

auto udp_socket::can_write(timespan timeout) const noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!is_valid()) {
        return false;
    }

    return WaitSocketReady(*_sock, false, true, timeout);
}

auto udp_socket::set_broadcast(bool enabled) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!is_valid()) {
        return false;
    }

#if !FO_WEB
    int32_t opt = enabled ? 1 : 0;
    auto opt_data = make_ptr(&opt).reinterpret_as<const char>();
    return ::setsockopt(*_sock, SOL_SOCKET, SO_BROADCAST, opt_data.get(), sizeof(opt)) != SOCKET_ERROR_VALUE;
#else
    return false;
#endif
}

auto udp_socket::send_to(string_view host, uint16_t port, const_span<uint8_t> data) noexcept -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    if (!is_valid() || data.empty()) {
        return 0;
    }

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    auto resolved = net_sockets::resolve_ipv4(host);

    if (!resolved.has_value()) {
        return 0;
    }

    addr.sin_addr.s_addr = *resolved;

    auto addr_ptr = make_ptr(&addr).reinterpret_as<const sockaddr>();
    return ::sendto(*_sock, make_ptr(data.data()).reinterpret_as<const char>().get(), numeric_cast<int32_t>(data.size()), 0, addr_ptr.get(), sizeof(addr));
}

auto udp_socket::receive_from(span<uint8_t> data, string& out_host, uint16_t& out_port) noexcept -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    out_host.clear();
    out_port = 0;

    if (!is_valid() || data.empty()) {
        return 0;
    }

    sockaddr_in addr {};
#if FO_WINDOWS
    int32_t addr_len = sizeof(addr);
#else
    socklen_t addr_len = sizeof(addr);
#endif
    auto addr_len_ptr = make_ptr(&addr_len);
    auto addr_ptr = make_ptr(&addr).reinterpret_as<sockaddr>();
    int32_t result = ::recvfrom(*_sock, make_ptr(data.data()).reinterpret_as<char>().get(), numeric_cast<int32_t>(data.size()), 0, addr_ptr.get(), addr_len_ptr.get());

    if (result <= 0) {
        return result;
    }

    uint32_t ip = ntohl(addr.sin_addr.s_addr);
    out_host = strex("{}.{}.{}.{}", (ip >> 24U) & 0xFFU, (ip >> 16U) & 0xFFU, (ip >> 8U) & 0xFFU, ip & 0xFFU);
    out_port = ntohs(addr.sin_port);
    return result;
}

void udp_socket::close() noexcept
{
    FO_STACK_TRACE_ENTRY();

    _sock.reset();
}

FO_END_NAMESPACE
