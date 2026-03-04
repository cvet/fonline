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

#pragma once

#include "BasicCore.h"
#include "Containers.h"
#include "TimeRelated.h"

FO_BEGIN_NAMESPACE

#if FO_WINDOWS
using socket_t = uintptr_t;
#else
using socket_t = int32;
#endif

class net_sockets final
{
public:
    static auto startup() noexcept -> bool;
};

class tcp_socket final
{
public:
    tcp_socket() noexcept = default;
    tcp_socket(const tcp_socket&) = delete;
    tcp_socket(tcp_socket&& other) noexcept = default;
    auto operator=(const tcp_socket&) -> tcp_socket& = delete;
    auto operator=(tcp_socket&& other) noexcept -> tcp_socket& = default;
    ~tcp_socket() = default;

    [[nodiscard]] auto is_valid() const noexcept -> bool { return !!_sock; }

    auto connect(string_view host, uint16 port) noexcept -> bool;
    auto can_read(timespan timeout = {}) const noexcept -> bool;
    auto can_write(timespan timeout = {}) const noexcept -> bool;
    auto send(const_span<uint8> data) noexcept -> int32;
    auto receive(span<uint8> data) noexcept -> int32;
    void close() noexcept;

private:
    friend class tcp_server;
    explicit tcp_socket(socket_t sock) noexcept;

    unique_del_ptr<socket_t> _sock;
};

class tcp_server final
{
public:
    tcp_server() noexcept = default;
    tcp_server(const tcp_server&) = delete;
    tcp_server(tcp_server&& other) noexcept = default;
    auto operator=(const tcp_server&) -> tcp_server& = delete;
    auto operator=(tcp_server&& other) noexcept -> tcp_server& = default;
    ~tcp_server() = default;

    [[nodiscard]] auto is_valid() const noexcept -> bool { return !!_listenSock; }

    auto listen(string_view bind_host, uint16 port, int32 backlog = 1) noexcept -> bool;
    auto can_accept(timespan timeout = {}) const noexcept -> bool;
    auto accept() noexcept -> tcp_socket;
    void close() noexcept;

private:
    unique_del_ptr<socket_t> _listenSock;
};

class udp_socket final
{
public:
    udp_socket() noexcept = default;
    udp_socket(const udp_socket&) = delete;
    udp_socket(udp_socket&& other) noexcept = default;
    auto operator=(const udp_socket&) -> udp_socket& = delete;
    auto operator=(udp_socket&& other) noexcept -> udp_socket& = default;
    ~udp_socket() = default;

    [[nodiscard]] auto is_valid() const noexcept -> bool { return !!_sock; }

    auto bind(string_view bind_host, uint16 port, bool reuse_addr = true) noexcept -> bool;
    auto can_read(timespan timeout = {}) const noexcept -> bool;
    auto can_write(timespan timeout = {}) const noexcept -> bool;
    auto set_broadcast(bool enabled) noexcept -> bool;
    auto send_to(string_view host, uint16 port, const_span<uint8> data) noexcept -> int32;
    auto receive_from(span<uint8> data, string& out_host, uint16& out_port) noexcept -> int32;
    void close() noexcept;

private:
    unique_del_ptr<socket_t> _sock;
};

FO_END_NAMESPACE
