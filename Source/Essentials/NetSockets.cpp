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
#include "WinApi-Include.h"
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

FO_BEGIN_NAMESPACE

constexpr socket_t INVALID_SOCKET_VALUE = static_cast<socket_t>(-1);
constexpr int32 SOCKET_ERROR_VALUE = static_cast<int32>(-1);
using sock_addr_t = decltype(std::declval<sockaddr_in>().sin_addr.s_addr);

static void CloseSocket(socket_t sock)
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    ::closesocket(sock);
#else
    ::close(sock);
#endif
}

static auto ResolveIpv4Address(string_view host, sock_addr_t& addr) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (host.empty() || host == "0.0.0.0" || host == "*") {
        addr = ::htonl(INADDR_ANY);
        return true;
    }

    if (host == "127.0.0.1" || host == "localhost") {
        addr = ::htonl(INADDR_LOOPBACK);
        return true;
    }

    addr = ::inet_addr(string(host).c_str());

    if (addr == INADDR_NONE) {
        return false;
    }

    return true;
}

static auto WaitSocketReady(socket_t sock, bool check_read, bool check_write, timespan timeout) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    fd_set read_set {};
    fd_set write_set {};
    fd_set* read_ptr = nullptr;
    fd_set* write_ptr = nullptr;

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

    const auto timeout_ms = std::max<int64>(timeout.milliseconds(), 0);

    timeval timeout_tv {};
    timeout_tv.tv_sec = numeric_cast<decltype(timeout_tv.tv_sec)>(timeout_ms / 1000);
    timeout_tv.tv_usec = numeric_cast<decltype(timeout_tv.tv_usec)>((timeout_ms % 1000) * 1000);

#if FO_WINDOWS
    const auto select_result = ::select(0, read_ptr, write_ptr, nullptr, &timeout_tv);
#else
    const auto select_result = ::select(numeric_cast<int32>(sock) + 1, read_ptr, write_ptr, nullptr, &timeout_tv);
#endif

    if (select_result <= 0) {
        return false;
    }

    return (read_ptr != nullptr && FD_ISSET(sock, &read_set)) || (write_ptr != nullptr && FD_ISSET(sock, &write_set));
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

void net_sockets::shutdown() noexcept
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    ::WSACleanup();
#endif
}

tcp_socket::tcp_socket(socket_t sock) noexcept :
    _sock {unique_del_ptr<socket_t>(SafeAlloc::MakeRaw<socket_t>(sock), [](const socket_t* p) {
        FO_RUNTIME_ASSERT(*p != INVALID_SOCKET_VALUE);
        CloseSocket(*p);
        delete p;
    })}
{
    FO_STACK_TRACE_ENTRY();
}

auto tcp_socket::connect(string_view host, uint16 port) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    close();

    const socket_t sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (sock == INVALID_SOCKET_VALUE) {
        return false;
    }

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(port);

    if (!ResolveIpv4Address(host, addr.sin_addr.s_addr)) {
        CloseSocket(sock);
        return false;
    }

    if (::connect(sock, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR_VALUE) {
        CloseSocket(sock);
        return false;
    }

    _sock = unique_del_ptr<socket_t>(SafeAlloc::MakeRaw<socket_t>(sock), [](const socket_t* p) {
        FO_RUNTIME_ASSERT(*p != INVALID_SOCKET_VALUE);
        CloseSocket(*p);
        delete p;
    });

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

auto tcp_socket::send(const_span<uint8> data) noexcept -> int32
{
    FO_STACK_TRACE_ENTRY();

    if (!is_valid() || data.empty()) {
        return 0;
    }

    return ::send(*_sock, reinterpret_cast<const char*>(data.data()), numeric_cast<int32>(data.size()), 0);
}

auto tcp_socket::receive(span<uint8> data) noexcept -> int32
{
    FO_STACK_TRACE_ENTRY();

    if (!is_valid() || data.empty()) {
        return 0;
    }

    return ::recv(*_sock, reinterpret_cast<char*>(data.data()), numeric_cast<int32>(data.size()), 0);
}

void tcp_socket::close() noexcept
{
    FO_STACK_TRACE_ENTRY();

    _sock.reset();
}

auto tcp_server::listen(string_view bind_host, uint16 port, int32 backlog) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    close();

    const socket_t sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (sock == INVALID_SOCKET_VALUE) {
        return false;
    }

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(port);

    if (!ResolveIpv4Address(bind_host, addr.sin_addr.s_addr)) {
        CloseSocket(sock);
        return false;
    }

    if (::bind(sock, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR_VALUE) {
        CloseSocket(sock);
        return false;
    }

    if (::listen(sock, backlog > 0 ? backlog : 1) == SOCKET_ERROR_VALUE) {
        CloseSocket(sock);
        return false;
    }

    _listenSock = unique_del_ptr<socket_t>(SafeAlloc::MakeRaw<socket_t>(sock), [](const socket_t* p) {
        FO_RUNTIME_ASSERT(*p != INVALID_SOCKET_VALUE);
        CloseSocket(*p);
        delete p;
    });

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
    int32 addr_len = sizeof(addr);
    const socket_t client_sock = ::accept(*_listenSock, reinterpret_cast<sockaddr*>(&addr), &addr_len);

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

auto udp_socket::bind(string_view bind_host, uint16 port, bool reuse_addr) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    close();

    const socket_t sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (sock == INVALID_SOCKET_VALUE) {
        return false;
    }

    if (reuse_addr) {
        constexpr int32 opt = 1;

        if (::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt)) == SOCKET_ERROR_VALUE) {
            CloseSocket(sock);
            return false;
        }
    }

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(port);

    if (!ResolveIpv4Address(bind_host, addr.sin_addr.s_addr)) {
        CloseSocket(sock);
        return false;
    }

    if (::bind(sock, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR_VALUE) {
        CloseSocket(sock);
        return false;
    }

    _sock = unique_del_ptr<socket_t>(SafeAlloc::MakeRaw<socket_t>(sock), [](const socket_t* p) {
        FO_RUNTIME_ASSERT(*p != INVALID_SOCKET_VALUE);
        CloseSocket(*p);
        delete p;
    });

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

    const int32 opt = enabled ? 1 : 0;
    return ::setsockopt(*_sock, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&opt), sizeof(opt)) != SOCKET_ERROR_VALUE;
}

auto udp_socket::send_to(string_view host, uint16 port, const_span<uint8> data) noexcept -> int32
{
    FO_STACK_TRACE_ENTRY();

    if (!is_valid() || data.empty()) {
        return 0;
    }

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(port);

    if (!ResolveIpv4Address(host, addr.sin_addr.s_addr)) {
        return 0;
    }

    return ::sendto(*_sock, reinterpret_cast<const char*>(data.data()), numeric_cast<int32>(data.size()), 0, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
}

auto udp_socket::receive_from(span<uint8> data, string& out_host, uint16& out_port) noexcept -> int32
{
    FO_STACK_TRACE_ENTRY();

    out_host.clear();
    out_port = 0;

    if (!is_valid() || data.empty()) {
        return 0;
    }

    sockaddr_in addr {};
    int32 addr_len = sizeof(addr);
    const int32 result = ::recvfrom(*_sock, reinterpret_cast<char*>(data.data()), numeric_cast<int32>(data.size()), 0, reinterpret_cast<sockaddr*>(&addr), &addr_len);

    if (result <= 0) {
        return result;
    }

    const uint32 ip = ::ntohl(addr.sin_addr.s_addr);
    out_host = strex("{}.{}.{}.{}", (ip >> 24U) & 0xFFU, (ip >> 16U) & 0xFFU, (ip >> 8U) & 0xFFU, ip & 0xFFU);
    out_port = ::ntohs(addr.sin_port);
    return result;
}

void udp_socket::close() noexcept
{
    FO_STACK_TRACE_ENTRY();

    _sock.reset();
}

FO_END_NAMESPACE
