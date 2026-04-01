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

#include "NetworkClient.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

namespace
{
    static std::atomic_uint16_t TestClientPort {48000};

    class ThrowingNetworkClientConnection final : public NetworkClientConnection
    {
    public:
        explicit ThrowingNetworkClientConnection(ClientNetworkSettings& settings) :
            NetworkClientConnection(settings)
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

        auto SendDataImpl(const_span<uint8> buf) -> size_t override
        {
            if (_throwOnSend) {
                throw std::runtime_error("send failure");
            }

            return buf.size();
        }

        auto ReceiveDataImpl(vector<uint8>& buf) -> size_t override
        {
            if (_throwOnReceive) {
                throw std::runtime_error("receive failure");
            }

            if (buf.empty()) {
                return 0;
            }

            buf[0] = uint8 {42};
            return 1;
        }

        void DisconnectImpl() noexcept override { _disconnectCount++; }

    private:
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
    const auto port = TestClientPort.fetch_add(1);
    BakerTests::OverrideSetting(settings.ServerPort, port);

    InterthreadDataCallback server_send_to_client;
    vector<uint8> server_received;
    size_t client_disconnect_count = 0;

    InterthreadListeners.emplace(port, [&](InterthreadDataCallback client_receive) -> InterthreadDataCallback {
        server_send_to_client = std::move(client_receive);

        return [&](const_span<uint8> buf) {
            if (buf.empty()) {
                client_disconnect_count++;
            }
            else {
                server_received.assign(buf.begin(), buf.end());
            }
        };
    });

    const auto cleanup = scope_exit([port]() noexcept { safe_call([port] { InterthreadListeners.erase(port); }); });

    auto conn = NetworkClientConnection::CreateInterthreadConnection(settings);
    REQUIRE(conn != nullptr);
    REQUIRE(server_send_to_client);

    CHECK_FALSE(conn->IsConnecting());
    CHECK(conn->IsConnected());
    CHECK(conn->CheckStatus(true));
    CHECK_FALSE(conn->CheckStatus(false));

    const vector<uint8> incoming_data {1, 2, 3};
    server_send_to_client(incoming_data);

    CHECK(conn->CheckStatus(false));

    const auto recv_data = conn->ReceiveData();
    CHECK(vector<uint8>(recv_data.begin(), recv_data.end()) == incoming_data);
    CHECK(conn->GetBytesReceived() == incoming_data.size());

    const vector<uint8> outgoing_data {4, 5, 6, 7};
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
    const auto port = TestClientPort.fetch_add(1);
    BakerTests::OverrideSetting(settings.ServerPort, port);

    InterthreadDataCallback server_send_to_client;

    InterthreadListeners.emplace(port, [&](InterthreadDataCallback client_receive) -> InterthreadDataCallback {
        server_send_to_client = std::move(client_receive);

        return [](const_span<uint8>) { };
    });

    const auto cleanup = scope_exit([port]() noexcept { safe_call([port] { InterthreadListeners.erase(port); }); });

    auto conn = NetworkClientConnection::CreateInterthreadConnection(settings);
    REQUIRE(conn != nullptr);
    REQUIRE(server_send_to_client);

    server_send_to_client({});

    CHECK_FALSE(conn->CheckStatus(false));
    CHECK_FALSE(conn->IsConnecting());
    CHECK_FALSE(conn->IsConnected());
    CHECK(conn->ReceiveData().empty());
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

        CHECK_THROWS(conn.SendData(vector<uint8> {7, 8, 9}));
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

FO_END_NAMESPACE
