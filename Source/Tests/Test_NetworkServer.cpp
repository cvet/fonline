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

#if FO_HAVE_WEB_SOCKETS
#define ASIO_STANDALONE 1
#define ASIO_NO_DEPRECATED 1
#include "asio.hpp"
// ReSharper disable CppInconsistentNaming
#define _WEBSOCKETPP_CPP11_FUNCTIONAL_ // NOLINT(clang-diagnostic-reserved-macro-identifier, bugprone-reserved-identifier)
#define _WEBSOCKETPP_CPP11_SYSTEM_ERROR_ // NOLINT(clang-diagnostic-reserved-macro-identifier, bugprone-reserved-identifier)
#define _WEBSOCKETPP_CPP11_RANDOM_DEVICE_ // NOLINT(clang-diagnostic-reserved-macro-identifier, bugprone-reserved-identifier)
#define _WEBSOCKETPP_CPP11_MEMORY_ // NOLINT(clang-diagnostic-reserved-macro-identifier, bugprone-reserved-identifier)
#define _WEBSOCKETPP_CPP11_STL_ // NOLINT(clang-diagnostic-reserved-macro-identifier, bugprone-reserved-identifier)
// ReSharper restore CppInconsistentNaming
FO_DISABLE_WARNINGS_PUSH()
#include "websocketpp/client.hpp"
#include "websocketpp/config/asio_no_tls_client.hpp"
FO_DISABLE_WARNINGS_POP()
#endif

FO_BEGIN_NAMESPACE

namespace
{
    // Base 47100: the sequential fetch_add(1) walk must not cross 47001, which Windows reserves for the
    // WinRM HTTP listener on effectively every machine (bind fails with WSAEACCES there).
    static std::atomic_uint16_t TestServerPort {47100};

    template<typename Predicate>
    auto WaitForCondition(Predicate&& predicate, std::chrono::milliseconds timeout = std::chrono::milliseconds {1000}) -> bool
    {
        auto deadline = std::chrono::steady_clock::now() + timeout;

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

    shared_ptr<NetworkServerConnection> conn = NetworkServer::CreateDummyConnection(&settings);
    REQUIRE(conn);

    CHECK(conn->GetHost() == "Dummy");
    CHECK(conn->IsDisconnected());

    conn->Dispatch();
    conn->Disconnect();

    CHECK(conn->IsDisconnected());
}

TEST_CASE("NetworkServerDummyConnectionCanStayConnected")
{
    auto settings = MakeServerNetworkSettings();

    shared_ptr<NetworkServerConnection> conn = NetworkServer::CreateDummyConnection(&settings, NetworkServer::DummyConnectionState::Connected);
    REQUIRE(conn);

    CHECK(conn->GetHost() == "Dummy");
    CHECK_FALSE(conn->IsDisconnected());

    conn->Dispatch();
    CHECK_FALSE(conn->IsDisconnected());

    conn->Disconnect();
    CHECK(conn->IsDisconnected());
}

TEST_CASE("NetworkServerInterthreadBuffersDispatchesAndShutsDown")
{
    auto settings = MakeServerNetworkSettings();
    auto port = TestServerPort.fetch_add(1);
    BakerTests::OverrideSetting(settings.ServerPort, port);

    shared_ptr<NetworkServerConnection> accepted_conn;
    vector<uint8_t> received_data;
    vector<uint8_t> sent_data;
    vector<uint8_t> response_data {4, 5, 6};
    size_t client_disconnect_count = 0;
    size_t server_disconnect_count = 0;

    auto server = NetworkServer::StartInterthreadServer(&settings, [&](shared_ptr<NetworkServerConnection> conn) { accepted_conn = std::move(conn); });

    auto shutdown = scope_exit([&server, port]() noexcept {
        safe_call([&server] { server->Shutdown(); });

        safe_call([port] { InterthreadListeners.erase(port); });
    });

    REQUIRE(InterthreadListeners.count(port) == 1);
    CHECK_THROWS(NetworkServer::StartInterthreadServer(&settings, [](shared_ptr<NetworkServerConnection>) { }));

    auto client_send = InterthreadListeners[port]([&](const_span<uint8_t> buf) {
        if (buf.empty()) {
            client_disconnect_count++;
        }
        else {
            sent_data.assign(buf.begin(), buf.end());
        }
    });

    REQUIRE(accepted_conn);
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

    CHECK(InterthreadListeners.count(port) == 0);
}

TEST_CASE("NetworkServerInterthreadCopiedListenerRejectsAfterShutdown")
{
    auto settings = MakeServerNetworkSettings();
    auto port = TestServerPort.fetch_add(1);
    BakerTests::OverrideSetting(settings.ServerPort, port);

    size_t accepted_count = 0;
    size_t client_disconnect_count = 0;
    function<InterthreadDataCallback(InterthreadDataCallback)> copied_listener;
    {
        auto server = NetworkServer::StartInterthreadServer(&settings, [&](shared_ptr<NetworkServerConnection>) { accepted_count++; });
        auto cleanup = scope_exit([&server, port]() noexcept {
            safe_call([&server] { server->Shutdown(); });
            safe_call([port] {
                scoped_lock locker {InterthreadListenersLocker};
                InterthreadListeners.erase(port);
            });
        });

        {
            scoped_lock locker {InterthreadListenersLocker};
            copied_listener = InterthreadListeners.at(port);
        }

        server->Shutdown();
    }
    REQUIRE(copied_listener);

    auto client_send = copied_listener([&](const_span<uint8_t> buf) {
        if (buf.empty()) {
            client_disconnect_count++;
        }
    });

    CHECK(client_send);
    CHECK(accepted_count == 0);
    CHECK(client_disconnect_count == 1);
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
    auto start_server = [&settings, &port, &startup_error, &callback_count, &second_connection_promise]() -> unique_ptr<NetworkServer> {
        for (int32_t attempt = 0; attempt != 64; ++attempt) {
            port = TestServerPort.fetch_add(1);
            BakerTests::OverrideSetting(settings.ServerPort, port);

            try {
                return NetworkServer::StartAsioServer(&settings, [&](shared_ptr<NetworkServerConnection> conn) {
                    auto callback_index = callback_count.fetch_add(1);

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

        throw std::runtime_error(startup_error.empty() ? "NetworkServer ASIO start failed" : startup_error.c_str());
    };

    unique_ptr<NetworkServer> server = start_server();

    auto shutdown = scope_exit([&server]() noexcept { safe_call([&server] { server->Shutdown(); }); });

    tcp_socket first_client;
    REQUIRE(first_client.connect("127.0.0.1", port));
    REQUIRE(WaitForCondition([&callback_count] { return callback_count.load() >= 1; }));
    first_client.close();

    tcp_socket second_client;
    REQUIRE(second_client.connect("127.0.0.1", port));
    REQUIRE(second_connection_future.wait_for(std::chrono::seconds {5}) == std::future_status::ready);

    auto second_connection = second_connection_future.get();
    REQUIRE(second_connection);
    CHECK_FALSE(second_connection->GetHost().empty());
    CHECK(second_connection->GetPort() != 0);

    second_connection->Disconnect();
    second_client.close();
}

TEST_CASE("NetworkServerAsioShutdownDisconnectsAcceptedConnections")
{
    REQUIRE(net_sockets::startup());

    auto settings = MakeServerNetworkSettings();
    uint16_t port = TestServerPort.fetch_add(1);
    BakerTests::OverrideSetting(settings.ServerPort, port);

    std::promise<shared_ptr<NetworkServerConnection>> accepted_connection_promise;
    auto accepted_connection_future = accepted_connection_promise.get_future();
    unique_ptr<NetworkServer> server = NetworkServer::StartAsioServer(&settings, [&](shared_ptr<NetworkServerConnection> conn) { accepted_connection_promise.set_value(std::move(conn)); });
    bool server_shutdown = false;
    auto shutdown = scope_exit([&server, &server_shutdown]() noexcept {
        if (!server_shutdown) {
            safe_call([&server] { server->Shutdown(); });
        }
    });

    tcp_socket client;
    REQUIRE(client.connect("127.0.0.1", port));
    REQUIRE(accepted_connection_future.wait_for(std::chrono::seconds {5}) == std::future_status::ready);

    shared_ptr<NetworkServerConnection> accepted_connection = accepted_connection_future.get();
    REQUIRE(accepted_connection);

    std::atomic_int disconnect_count {};
    accepted_connection->SetAsyncCallbacks([]() -> const_span<uint8_t> { return {}; }, [](const_span<uint8_t>) {}, [&disconnect_count] { disconnect_count.fetch_add(1); });
    CHECK_FALSE(accepted_connection->IsDisconnected());

    server->Shutdown();
    server_shutdown = true;

    CHECK(accepted_connection->IsDisconnected());
    CHECK(disconnect_count.load() == 1);

    client.close();
}
#endif

#if FO_HAVE_WEB_SOCKETS
// End-to-end WebSocket transport coverage (previously none, which is what let the transport's threading
// bugs ship). A real websocketpp client connects and sends a binary frame that must reach the connection's
// receive callback (the flaky inbound-delivery regression), then Shutdown() itself must disconnect the
// accepted wrapper and tear the transport down without racing the websocketpp io thread - the
// close() teardown, tracked-connection shutdown, and weak_from_this() handler lifetime. The sanitizer CI
// jobs turn any residual use-after-free in this path into a hard failure.
TEST_CASE("NetworkServerWebSocketsDeliversFrameAndTearsDownCleanly")
{
    REQUIRE(net_sockets::startup());

    auto settings = MakeServerNetworkSettings();
    auto port = TestServerPort.fetch_add(1);
    BakerTests::OverrideSetting(settings.WebSocketPort, static_cast<int32_t>(port));
    BakerTests::OverrideSetting(settings.SecuredWebSockets, false);

    mutex state_mutex;
    shared_ptr<NetworkServerConnection> accepted_conn;
    vector<uint8_t> received;

    auto server = NetworkServer::StartWebSocketsServer(&settings, [&](shared_ptr<NetworkServerConnection> conn) {
        conn->SetAsyncCallbacks([]() -> const_span<uint8_t> { return {}; },
            [&](const_span<uint8_t> buf) {
                scoped_lock lock {state_mutex};
                received.insert(received.end(), buf.begin(), buf.end());
            },
            []() {});

        scoped_lock lock {state_mutex};
        accepted_conn = std::move(conn);
    });

    auto shutdown_server = scope_exit([&server]() noexcept { safe_call([&server] { server->Shutdown(); }); });

    websocketpp::client<websocketpp::config::asio_client> client;
    client.clear_access_channels(websocketpp::log::alevel::all);
    client.clear_error_channels(websocketpp::log::elevel::all);
    client.init_asio();

    vector<uint8_t> payload {1, 2, 3};

    client.set_open_handler([&client, &payload](const websocketpp::connection_hdl& hdl) {
        std::error_code send_error;
        client.send(hdl, payload.data(), payload.size(), websocketpp::frame::opcode::binary, send_error);
        ignore_unused(send_error);
    });

    std::error_code connect_error;
    auto connection = client.get_connection("ws://127.0.0.1:" + std::to_string(port), connect_error);
    REQUIRE_FALSE(connect_error);

    std::error_code subprotocol_error;
    connection->add_subprotocol("binary", subprotocol_error);
    REQUIRE_FALSE(subprotocol_error);

    client.connect(connection);

    std::thread client_thread {[&client] {
        try {
            client.run();
        }
        catch (...) {
        }
    }};
    auto stop_client = scope_exit([&client, &client_thread]() noexcept {
        safe_call([&client] { client.stop(); });
        safe_call([&client_thread] {
            if (client_thread.joinable()) {
                client_thread.join();
            }
        });
    });

    REQUIRE(WaitForCondition(
        [&] {
            scoped_lock lock {state_mutex};
            return received.size() >= payload.size();
        },
        std::chrono::milliseconds {5000}));

    shared_ptr<NetworkServerConnection> connection_to_close;
    {
        scoped_lock lock {state_mutex};
        CHECK(received == payload);
        REQUIRE(accepted_conn);
        connection_to_close = accepted_conn;
    }

    server->Shutdown();
    CHECK(connection_to_close->IsDisconnected());

    client.stop();
    if (client_thread.joinable()) {
        client_thread.join();
    }
}
#endif

FO_END_NAMESPACE
