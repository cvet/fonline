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

#include "ClientConnection.h"
#include "NetworkClient.h"
#include "NetworkServer.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

namespace
{
    static std::atomic_uint16_t TestClientPort {48000};

    class ThrowingNetworkClientConnection final : public NetworkClientConnection
    {
    public:
        explicit ThrowingNetworkClientConnection(ClientNetworkSettings& settings) :
            NetworkClientConnection(SettingsPtr(settings))
        {
        }

        void SetConnectingState() noexcept
        {
            _isConnecting = true;
            _isConnected = false;
        }

        void SetConnectedState() noexcept
        {
            _isConnecting = false;
            _isConnected = true;
        }

        void ThrowOnCheck() noexcept { _throwOnCheck = true; }
        void ThrowOnSend() noexcept { _throwOnSend = true; }
        void ThrowOnReceive() noexcept { _throwOnReceive = true; }

        [[nodiscard]] auto GetDisconnectCount() const noexcept -> size_t { return _disconnectCount; }

    protected:
        auto CheckStatusImpl(bool for_write) -> bool override
        {
            ignore_unused(for_write);

            if (_throwOnCheck) {
                throw std::runtime_error("check failure");
            }

            return true;
        }

        auto SendDataImpl(const_span<uint8_t> buf) -> size_t override
        {
            if (_throwOnSend) {
                throw std::runtime_error("send failure");
            }

            return buf.size();
        }

        auto ReceiveDataImpl(vector<uint8_t>& buf) -> size_t override
        {
            if (_throwOnReceive) {
                throw std::runtime_error("receive failure");
            }

            if (buf.empty()) {
                return 0;
            }

            buf[0] = uint8_t {42};
            return 1;
        }

        void DisconnectImpl() noexcept override { _disconnectCount++; }

    private:
        static auto SettingsPtr(ClientNetworkSettings& settings) noexcept -> ptr<ClientNetworkSettings>
        {
            FO_NO_STACK_TRACE_ENTRY();

            return &settings;
        }

        bool _throwOnCheck {};
        bool _throwOnSend {};
        bool _throwOnReceive {};
        size_t _disconnectCount {};
    };

    static auto MakeClientNetworkSettings() -> GlobalSettings
    {
        auto settings = GlobalSettings(false);

        settings.ApplyDefaultSettings();
        settings.ApplyAutoSettings();

        return settings;
    }
}

TEST_CASE("NetworkClientInterthreadSendReceiveAndDisconnect")
{
    auto settings = MakeClientNetworkSettings();
    auto port = TestClientPort.fetch_add(1);
    BakerTests::OverrideSetting(settings.ServerPort, port);

    InterthreadDataCallback server_send_to_client;
    vector<uint8_t> server_received;
    size_t client_disconnect_count = 0;

    InterthreadListeners.emplace(port, [&](InterthreadDataCallback client_receive) -> InterthreadDataCallback {
        server_send_to_client = std::move(client_receive);

        return [&](const_span<uint8_t> buf) {
            if (buf.empty()) {
                client_disconnect_count++;
            }
            else {
                server_received.assign(buf.begin(), buf.end());
            }
        };
    });

    auto cleanup = scope_exit([port]() noexcept { safe_call([port] { InterthreadListeners.erase(port); }); });

    auto conn = NetworkClientConnection::CreateInterthreadConnection(&settings);
    REQUIRE(server_send_to_client);

    CHECK_FALSE(conn->IsConnecting());
    CHECK(conn->IsConnected());
    CHECK(conn->CheckStatus(true));
    CHECK_FALSE(conn->CheckStatus(false));

    vector<uint8_t> incoming_data {1, 2, 3};
    server_send_to_client(incoming_data);

    CHECK(conn->CheckStatus(false));

    auto recv_data = conn->ReceiveData();
    CHECK(vector<uint8_t>(recv_data.begin(), recv_data.end()) == incoming_data);
    CHECK(conn->GetBytesReceived() == incoming_data.size());

    vector<uint8_t> outgoing_data {4, 5, 6, 7};
    CHECK(conn->SendData(outgoing_data) == outgoing_data.size());
    CHECK(server_received == outgoing_data);
    CHECK(conn->GetBytesSend() == outgoing_data.size());

    conn->Disconnect();

    CHECK_FALSE(conn->IsConnecting());
    CHECK_FALSE(conn->IsConnected());
    CHECK(client_disconnect_count == 1);
    CHECK_FALSE(conn->CheckStatus(true));
    CHECK(conn->SendData(outgoing_data) == 0);
    CHECK(conn->ReceiveData().empty());

    conn->Disconnect();
    CHECK(client_disconnect_count == 1);
}

TEST_CASE("NetworkClientInterthreadHandlesServerDisconnect")
{
    auto settings = MakeClientNetworkSettings();
    auto port = TestClientPort.fetch_add(1);
    BakerTests::OverrideSetting(settings.ServerPort, port);

    InterthreadDataCallback server_send_to_client;

    InterthreadListeners.emplace(port, [&](InterthreadDataCallback client_receive) -> InterthreadDataCallback {
        server_send_to_client = std::move(client_receive);

        return [](const_span<uint8_t>) { };
    });

    auto cleanup = scope_exit([port]() noexcept { safe_call([port] { InterthreadListeners.erase(port); }); });

    auto conn = NetworkClientConnection::CreateInterthreadConnection(&settings);
    REQUIRE(server_send_to_client);

    server_send_to_client({});

    CHECK_FALSE(conn->CheckStatus(false));
    CHECK_FALSE(conn->IsConnecting());
    CHECK_FALSE(conn->IsConnected());
    CHECK(conn->ReceiveData().empty());
}

TEST_CASE("ClientConnectionDisconnectsOnMalformedCompressedInput")
{
    auto settings = MakeClientNetworkSettings();
    auto port = TestClientPort.fetch_add(1);
    BakerTests::OverrideSetting(settings.ServerPort, port);
    BakerTests::OverrideSetting(settings.DisableZlibCompression, false);

    InterthreadDataCallback server_send_to_client;
    size_t client_disconnect_count = 0;

    InterthreadListeners.emplace(port, [&](InterthreadDataCallback client_receive) -> InterthreadDataCallback {
        server_send_to_client = std::move(client_receive);

        return [&](const_span<uint8_t> buf) {
            if (buf.empty()) {
                client_disconnect_count++;
            }
        };
    });

    auto cleanup = scope_exit([port]() noexcept { safe_call([port] { InterthreadListeners.erase(port); }); });

    optional<ClientConnection::ConnectResult> connect_result;
    ClientConnection client {&settings};
    client.SetConnectHandler([&](ClientConnection::ConnectResult result) { connect_result = result; });
    client.Connect();
    REQUIRE(server_send_to_client);

    vector<uint8_t> invalid = {0x00, 0x00};
    server_send_to_client(invalid);

    CHECK_NOTHROW(client.Process());
    CHECK_FALSE(client.IsConnecting());
    CHECK_FALSE(client.IsConnected());
    CHECK(connect_result == ClientConnection::ConnectResult::Failed);
    CHECK(client_disconnect_count == 1);
}

TEST_CASE("NetworkClientWrapperDisconnectsAndRethrowsOnImplExceptions")
{
    auto settings = MakeClientNetworkSettings();
    ThrowingNetworkClientConnection conn {settings};

    SECTION("CheckStatusWhileConnecting")
    {
        conn.SetConnectingState();
        conn.ThrowOnCheck();

        CHECK_THROWS(conn.CheckStatus(false));
        CHECK_FALSE(conn.IsConnecting());
        CHECK_FALSE(conn.IsConnected());
        CHECK(conn.GetDisconnectCount() == 1);
    }

    SECTION("SendDataWhileConnected")
    {
        conn.SetConnectedState();
        conn.ThrowOnSend();

        CHECK_THROWS(conn.SendData(vector<uint8_t> {7, 8, 9}));
        CHECK_FALSE(conn.IsConnecting());
        CHECK_FALSE(conn.IsConnected());
        CHECK(conn.GetDisconnectCount() == 1);
        CHECK(conn.GetBytesSend() == 0);
    }

    SECTION("ReceiveDataWhileConnected")
    {
        conn.SetConnectedState();
        conn.ThrowOnReceive();

        CHECK_THROWS(conn.ReceiveData());
        CHECK_FALSE(conn.IsConnecting());
        CHECK_FALSE(conn.IsConnected());
        CHECK(conn.GetDisconnectCount() == 1);
        CHECK(conn.GetBytesReceived() == 0);
    }
}

TEST_CASE("NetworkClientSocketsTalksToARealServer")
{
    // The socket transports were previously assumed untestable, but a server on a loopback port is enough:
    // the client dials 127.0.0.1 and the whole connect / send / receive / disconnect path runs for real.
    REQUIRE(net_sockets::startup());

    auto server_settings = MakeClientNetworkSettings();
    auto client_settings = MakeClientNetworkSettings();

    std::mutex accepted_locker;
    vector<shared_ptr<NetworkServerConnection>> accepted;

    uint16_t port = 0;
    string startup_error;
    auto start_server = [&]() -> unique_ptr<NetworkServer> {
        for (int32_t attempt = 0; attempt != 64; ++attempt) {
            port = TestClientPort.fetch_add(1);
            BakerTests::OverrideSetting(server_settings.ServerPort, port);

            try {
                return NetworkServer::StartAsioServer(&server_settings, [&](shared_ptr<NetworkServerConnection> conn) {
                    std::scoped_lock locker {accepted_locker};
                    accepted.emplace_back(std::move(conn));
                });
            }
            catch (const std::exception& ex) {
                startup_error = ex.what();
            }
        }

        throw std::runtime_error(startup_error.empty() ? "NetworkServer ASIO start failed" : startup_error.c_str());
    };

    unique_ptr<NetworkServer> server = start_server();

    auto shutdown_server = scope_exit([&server]() noexcept { safe_call([&server] { server->Shutdown(); }); });

    BakerTests::OverrideSetting(client_settings.ServerHost, string {"127.0.0.1"});
    BakerTests::OverrideSetting(client_settings.ServerPort, port);
    BakerTests::OverrideSetting(client_settings.ProxyType, 0);

    auto conn = NetworkClientConnection::CreateSocketsConnection(&client_settings);

    // The connect is asynchronous and completes on writability, so the write-side status is what settles it
    bool connected = false;

    for (int32_t i = 0; i < 500 && !connected; i++) {
        (void)conn->CheckStatus(true);
        connected = conn->IsConnected();

        if (!connected) {
            std::this_thread::sleep_for(std::chrono::milliseconds {10});
        }
    }

    CHECK(connected);

    if (connected) {
        shared_ptr<NetworkServerConnection> server_conn;

        for (int32_t i = 0; i < 500 && !server_conn; i++) {
            {
                std::scoped_lock locker {accepted_locker};

                if (!accepted.empty()) {
                    server_conn = accepted.front();
                }
            }

            if (!server_conn) {
                std::this_thread::sleep_for(std::chrono::milliseconds {10});
            }
        }

        REQUIRE(static_cast<bool>(server_conn));
        CHECK_FALSE(server_conn->GetHost().empty());

        vector<uint8_t> outgoing {9, 8, 7, 6};
        CHECK(conn->SendData(outgoing) == outgoing.size());
        CHECK(conn->GetBytesSend() >= outgoing.size());

        // The accepted connection reports the payload through its async receive callback, and the same
        // callback set carries the downstream payload back; the callbacks can only be installed once
        std::atomic_size_t server_received {};
        vector<uint8_t> downstream {1, 1, 2, 3, 5, 8};
        std::atomic_bool downstream_sent {};
        server_conn->SetAsyncCallbacks(
            [&downstream, &downstream_sent]() -> const_span<uint8_t> {
                if (downstream_sent.exchange(true)) {
                    return {};
                }

                return downstream;
            },
            [&server_received](const_span<uint8_t> buf) { server_received.fetch_add(buf.size()); }, []() {});

        for (int32_t i = 0; i < 500 && server_received.load() == 0; i++) {
            server_conn->Dispatch();
            std::this_thread::sleep_for(std::chrono::milliseconds {10});
        }

        CHECK(server_received.load() > 0);

        // The downstream payload travels back on the same socket, so the client receive path runs for real
        size_t client_received = 0;

        for (int32_t i = 0; i < 500 && client_received == 0; i++) {
            server_conn->Dispatch();

            if (conn->CheckStatus(false)) {
                client_received = conn->ReceiveData().size();
            }

            if (client_received == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds {10});
            }
        }

        CHECK(client_received > 0);
        CHECK(conn->GetBytesReceived() > 0);

        server_conn->Disconnect();
    }

    conn->Disconnect();
    CHECK_FALSE(conn->IsConnected());
}

TEST_CASE("NetworkClientUdpSocketsTalksToARealServer")
{
    REQUIRE(net_sockets::startup());

    auto server_settings = MakeClientNetworkSettings();
    auto client_settings = MakeClientNetworkSettings();

    std::mutex accepted_locker;
    vector<shared_ptr<NetworkServerConnection>> accepted;

    uint16_t port = 0;
    string startup_error;
    auto start_server = [&]() -> unique_ptr<NetworkServer> {
        for (int32_t attempt = 0; attempt != 64; ++attempt) {
            port = TestClientPort.fetch_add(1);
            BakerTests::OverrideSetting(server_settings.ServerPort, port);

            try {
                return NetworkServer::StartUdpSocketsServer(&server_settings, [&](shared_ptr<NetworkServerConnection> conn) {
                    std::scoped_lock locker {accepted_locker};
                    accepted.emplace_back(std::move(conn));
                });
            }
            catch (const std::exception& ex) {
                startup_error = ex.what();
            }
        }

        throw std::runtime_error(startup_error.empty() ? "NetworkServer UDP start failed" : startup_error.c_str());
    };

    unique_ptr<NetworkServer> server = start_server();

    auto shutdown_server = scope_exit([&server]() noexcept { safe_call([&server] { server->Shutdown(); }); });

    BakerTests::OverrideSetting(client_settings.ServerHost, string {"127.0.0.1"});
    BakerTests::OverrideSetting(client_settings.ServerPort, port);

    auto conn = NetworkClientConnection::CreateUdpSocketsConnection(&client_settings);

    // The UDP transport performs its own handshake, so it needs pumping on both sides before it settles
    bool connected = false;

    for (int32_t i = 0; i < 800 && !connected; i++) {
        (void)conn->CheckStatus(false);
        connected = conn->IsConnected();

        if (!connected) {
            std::this_thread::sleep_for(std::chrono::milliseconds {10});
        }
    }

    REQUIRE(connected);

    vector<uint8_t> outgoing {1, 2, 3, 4, 5};
    CHECK(conn->SendData(outgoing) == outgoing.size());

    conn->Disconnect();
    CHECK_FALSE(conn->IsConnected());
}

FO_END_NAMESPACE
