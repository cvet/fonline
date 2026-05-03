//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
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

#include "catch_amalgamated.hpp"

#include <future>
#include <thread>

#include "NetSockets.h"
#include "NetworkServer.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

namespace
{
    static std::atomic_uint16_t TestServerPort {47000};

    template<typename Predicate>
    auto WaitForCondition(Predicate&& predicate, std::chrono::milliseconds timeout = std::chrono::milliseconds {1000}) -> bool
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;

        while (std::chrono::steady_clock::now() < deadline) {
            if (predicate()) {
                return true;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds {5});
        }

        return predicate();
    }

    static auto MakeServerNetworkSettings() -> GlobalSettings
    {
        auto settings = GlobalSettings(false);

        settings.ApplyDefaultSettings();
        settings.ApplyAutoSettings();

        BakerTests::ApplySelfContainedServerSettings(settings);

        return settings;
    }
}

TEST_CASE("NetworkServerDummyConnectionIsDisconnected")
{
    auto settings = MakeServerNetworkSettings();

    auto conn = NetworkServer::CreateDummyConnection(settings);
    REQUIRE(conn != nullptr);

    CHECK(conn->GetHost() == "Dummy");
    CHECK(conn->IsDisconnected());

    conn->Dispatch();
    conn->Disconnect();

    CHECK(conn->IsDisconnected());
}

TEST_CASE("NetworkServerInterthreadBuffersDispatchesAndShutsDown")
{
    auto settings = MakeServerNetworkSettings();
    const auto port = TestServerPort.fetch_add(1);
    BakerTests::OverrideSetting(settings.ServerPort, port);

    shared_ptr<NetworkServerConnection> accepted_conn;
    vector<uint8_t> received_data;
    vector<uint8_t> sent_data;
    const vector<uint8_t> response_data {4, 5, 6};
    size_t client_disconnect_count = 0;
    size_t server_disconnect_count = 0;

    auto server = NetworkServer::StartInterthreadServer(settings, [&](shared_ptr<NetworkServerConnection> conn) { accepted_conn = std::move(conn); });
    REQUIRE(server != nullptr);

    const auto shutdown = scope_exit([&server, port]() noexcept {
        safe_call([&server] {
            if (server) {
                server->Shutdown();
            }
        });

        safe_call([port] { InterthreadListeners.erase(port); });
    });

    REQUIRE(InterthreadListeners.count(port) == 1);
    CHECK_THROWS(NetworkServer::StartInterthreadServer(settings, [](shared_ptr<NetworkServerConnection>) { }));

    auto client_send = InterthreadListeners[port]([&](const_span<uint8_t> buf) {
        if (buf.empty()) {
            client_disconnect_count++;
        }
        else {
            sent_data.assign(buf.begin(), buf.end());
        }
    });

    REQUIRE(accepted_conn != nullptr);
    REQUIRE(client_send);

    client_send(vector<uint8_t> {1, 2, 3});

    accepted_conn->SetAsyncCallbacks([&]() -> const_span<uint8_t> { return response_data; }, [&](const_span<uint8_t> buf) { received_data.assign(buf.begin(), buf.end()); }, [&]() { server_disconnect_count++; });

    CHECK(received_data == vector<uint8_t>({1, 2, 3}));

    accepted_conn->Dispatch();
    CHECK(sent_data == vector<uint8_t>({4, 5, 6}));

    accepted_conn->Disconnect();
    CHECK(accepted_conn->IsDisconnected());
    CHECK(client_disconnect_count == 1);
    CHECK(server_disconnect_count == 1);

    accepted_conn->Disconnect();
    CHECK(client_disconnect_count == 1);
    CHECK(server_disconnect_count == 1);

    server->Shutdown();
    server.reset();

    CHECK(InterthreadListeners.count(port) == 0);
}

#if FO_HAVE_ASIO
TEST_CASE("NetworkServerAsioRearmsAcceptAfterCallbackException")
{
    REQUIRE(net_sockets::startup());

    auto settings = MakeServerNetworkSettings();

    std::atomic_int callback_count {};
    std::promise<shared_ptr<NetworkServerConnection>> second_connection_promise;
    auto second_connection_future = second_connection_promise.get_future();

    uint16_t port = 0;
    string startup_error;
    unique_ptr<NetworkServer> server;

    for (int32_t attempt = 0; attempt != 64 && !server; ++attempt) {
        port = TestServerPort.fetch_add(1);
        BakerTests::OverrideSetting(settings.ServerPort, port);

        try {
            server = NetworkServer::StartAsioServer(settings, [&](shared_ptr<NetworkServerConnection> conn) {
                const auto callback_index = callback_count.fetch_add(1);

                if (callback_index == 0) {
                    throw std::runtime_error("test accept callback failure");
                }

                second_connection_promise.set_value(std::move(conn));
            });
        }
        catch (const std::exception& ex) {
            startup_error = ex.what();
        }
    }

    INFO(startup_error);
    REQUIRE(server != nullptr);

    const auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server) {
                server->Shutdown();
            }
        });
    });

    tcp_socket first_client;
    REQUIRE(first_client.connect("127.0.0.1", port));
    REQUIRE(WaitForCondition([&callback_count] { return callback_count.load() >= 1; }));
    first_client.close();

    tcp_socket second_client;
    REQUIRE(second_client.connect("127.0.0.1", port));
    REQUIRE(second_connection_future.wait_for(std::chrono::seconds {1}) == std::future_status::ready);

    auto second_connection = second_connection_future.get();
    REQUIRE(second_connection != nullptr);
    CHECK_FALSE(second_connection->GetHost().empty());
    CHECK(second_connection->GetPort() != 0);

    second_connection->Disconnect();
    second_client.close();
}
#endif

FO_END_NAMESPACE
