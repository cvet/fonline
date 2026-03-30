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

#include "catch_amalgamated.hpp"

#include <atomic>

#include "Common.h"
#include "NetSockets.h"

FO_BEGIN_NAMESPACE

namespace
{
    const timespan ShortTimeout {std::chrono::milliseconds {1}};
    const timespan WriteTimeout {std::chrono::milliseconds {50}};
    const timespan ReadTimeout {std::chrono::milliseconds {200}};

    auto AcquireTestPort() -> uint16
    {
        static std::atomic<uint16> next_port {42000};

        return next_port.fetch_add(1, std::memory_order_relaxed);
    }

    auto BindUdpLoopback(udp_socket& sock) -> uint16
    {
        for (int32 attempt = 0; attempt != 128; ++attempt) {
            const uint16 port = AcquireTestPort();

            if (sock.bind("127.0.0.1", port)) {
                return port;
            }
        }

        FAIL("Unable to bind UDP loopback test socket");
    }

    auto ListenTcpLoopback(tcp_server& server) -> uint16
    {
        for (int32 attempt = 0; attempt != 128; ++attempt) {
            const uint16 port = AcquireTestPort();

            if (server.listen("127.0.0.1", port)) {
                return port;
            }
        }

        FAIL("Unable to bind TCP loopback test server");
    }
}

TEST_CASE("NetSockets")
{
    REQUIRE(net_sockets::startup());

    SECTION("InvalidSocketsBehaveAsNoOps")
    {
        tcp_socket tcp;
        udp_socket udp;
        tcp_server server;
        uint8 data[8] {};
        string host;
        uint16 port = 123;

        CHECK_FALSE(tcp.is_valid());
        CHECK_FALSE(tcp.can_read(ShortTimeout));
        CHECK_FALSE(tcp.can_write(ShortTimeout));
        CHECK(tcp.send(data) == 0);
        CHECK(tcp.receive(data) == 0);

        CHECK_FALSE(server.is_valid());
        CHECK_FALSE(server.can_accept(ShortTimeout));
        CHECK_FALSE(server.accept().is_valid());

        CHECK_FALSE(udp.is_valid());
        CHECK_FALSE(udp.can_read(ShortTimeout));
        CHECK_FALSE(udp.can_write(ShortTimeout));
        CHECK_FALSE(udp.set_broadcast(true));
        CHECK(udp.send_to("127.0.0.1", 40000, data) == 0);
        CHECK(udp.receive_from(data, host, port) == 0);
        CHECK(host.empty());
        CHECK(port == 0);
    }

    SECTION("UdpLoopbackRoundTrip")
    {
        udp_socket receiver;
        udp_socket sender;
        const uint16 receiver_port = BindUdpLoopback(receiver);

        REQUIRE(sender.bind("127.0.0.1", 0));
        REQUIRE(sender.can_write(WriteTimeout));

        const array<uint8, 4> payload = {'p', 'i', 'n', 'g'};
        REQUIRE(sender.send_to("127.0.0.1", receiver_port, payload) == numeric_cast<int32>(payload.size()));
        REQUIRE(receiver.can_read(ReadTimeout));

        array<uint8, 16> buffer {};
        string host;
        uint16 port = 0;
        const int32 received = receiver.receive_from(buffer, host, port);

        REQUIRE(received == numeric_cast<int32>(payload.size()));
        CHECK(std::equal(payload.begin(), payload.end(), buffer.begin()));
        CHECK(host == "127.0.0.1");
        CHECK(port != 0);
    }

    SECTION("TcpLoopbackConnectSendAndReceive")
    {
        tcp_server server;
        const uint16 port = ListenTcpLoopback(server);

        tcp_socket client;
        REQUIRE(client.connect("127.0.0.1", port));
        REQUIRE(client.can_write(ReadTimeout));
        REQUIRE(server.can_accept(ReadTimeout));

        tcp_socket accepted = server.accept();
        REQUIRE(accepted.is_valid());

        const array<uint8, 5> payload = {'h', 'e', 'l', 'l', 'o'};
        REQUIRE(client.send(payload) == numeric_cast<int32>(payload.size()));
        REQUIRE(accepted.can_read(ReadTimeout));

        array<uint8, 16> buffer {};
        const int32 received = accepted.receive(buffer);

        REQUIRE(received == numeric_cast<int32>(payload.size()));
        CHECK(std::equal(payload.begin(), payload.end(), buffer.begin()));
    }
}

FO_END_NAMESPACE
