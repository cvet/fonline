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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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

#include "ClientConnection.h"
#include "NetSockets.h"
#include "NetworkServer.h"
#include "SecureChannel.h"
#include "ServerConnection.h"
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
    // Base 49100 keeps clear of the ranges the other socket suites walk
    static std::atomic_uint16_t TestChannelPort {49100};

    auto MakeTestSettings() -> GlobalSettings
    {
        auto settings = GlobalSettings(false);

        settings.ApplyDefaultSettings();
        settings.ApplyAutoSettings();

        return settings;
    }

    auto MakeRandomBytes(size_t size) -> vector<uint8_t>
    {
        vector<uint8_t> data(size);
        crypto::fill_random(data);
        return data;
    }

    auto ContainsSequence(const_span<uint8_t> haystack, const_span<uint8_t> needle) -> bool
    {
        return std::ranges::search(haystack, needle).begin() != haystack.end();
    }

    // Passes everything one side has for the other, in the chunk size given, until both have nothing left
    void Exchange(SecureChannel& client, SecureChannel& server, size_t chunk_size)
    {
        auto deliver = [chunk_size](const vector<uint8_t>& data, SecureChannel& receiver) {
            vector<uint8_t> plaintext;

            for (size_t offset = 0; offset < data.size(); offset += chunk_size) {
                receiver.Receive(const_span<uint8_t> {data}.subspan(offset, std::min(chunk_size, data.size() - offset)), plaintext);
            }

            FO_VERIFY_AND_THROW(plaintext.empty(), "Handshake exchange produced transport plaintext");
        };

        while (client.HasHandshakeOutput() || server.HasHandshakeOutput()) {
            vector<uint8_t> client_output;
            client.TakeHandshakeOutput(client_output);
            deliver(client_output, server);

            vector<uint8_t> server_output;
            server.TakeHandshakeOutput(server_output);
            deliver(server_output, client);
        }
    }

    auto SealAll(SecureChannel& channel, const vector<vector<uint8_t>>& messages) -> vector<uint8_t>
    {
        vector<uint8_t> sealed;

        for (const auto& message : messages) {
            channel.Seal(message, sealed);
        }

        return sealed;
    }
}

TEST_CASE("SecureChannelCarriesAStreamInAnyChunking")
{
    SecureChannelIdentity identity {crypto::generate_secret_key()};
    vector<crypto::key_bytes> server_keys {identity.GetPublicKey()};
    SecureChannel client {server_keys};
    SecureChannel server {identity};

    CHECK_FALSE(client.IsServer());
    CHECK(server.IsServer());
    CHECK(client.HasHandshakeOutput());
    CHECK_FALSE(server.HasHandshakeOutput());

    Exchange(client, server, 1);

    REQUIRE(client.IsEstablished());
    REQUIRE(server.IsEstablished());

    // One message larger than a frame, so it is split and reassembled
    vector<vector<uint8_t>> messages {MakeRandomBytes(1), MakeRandomBytes(SecureChannel::MAX_TRANSPORT_PAYLOAD * 3 + 17), vector<uint8_t>(5000, uint8_t {0x5A})};
    vector<uint8_t> expected;

    for (const auto& message : messages) {
        expected.insert(expected.end(), message.begin(), message.end());
    }

    for (bool from_client : {true, false}) {
        INFO(from_client);

        SecureChannel& sender = from_client ? client : server;
        SecureChannel& receiver = from_client ? server : client;
        vector<uint8_t> sealed = SealAll(sender, messages);

        CHECK_FALSE(ContainsSequence(sealed, messages[2]));

        vector<uint8_t> plaintext;
        size_t offset = 0;
        size_t chunk_size = 1;

        while (offset < sealed.size()) {
            size_t size = std::min(chunk_size, sealed.size() - offset);
            receiver.Receive(const_span<uint8_t> {sealed}.subspan(offset, size), plaintext);
            offset += size;
            chunk_size = chunk_size * 3 + 1;
        }

        CHECK(plaintext == expected);
    }
}

TEST_CASE("SecureChannelOffersEveryPinnedKey")
{
    SecureChannelIdentity identity {crypto::generate_secret_key()};
    crypto::key_bytes other_key = crypto::derive_public_key(crypto::generate_secret_key());

    for (bool server_key_first : {true, false}) {
        INFO(server_key_first);

        // The next key is pinned ahead of a rotation, so either position must open
        vector<crypto::key_bytes> server_keys = server_key_first ? vector<crypto::key_bytes> {identity.GetPublicKey(), other_key} : vector<crypto::key_bytes> {other_key, identity.GetPublicKey()};
        SecureChannel client {server_keys};
        SecureChannel server {identity};

        Exchange(client, server, 7);

        REQUIRE(client.IsEstablished());
        REQUIRE(server.IsEstablished());

        vector<uint8_t> sealed;
        vector<uint8_t> plaintext;
        client.Seal(vector<uint8_t> {1, 2, 3}, sealed);
        server.Receive(sealed, plaintext);

        CHECK(plaintext == vector<uint8_t> {1, 2, 3});
    }
}

TEST_CASE("SecureChannelRefusesAServerItHasNotPinned")
{
    SecureChannelIdentity identity {crypto::generate_secret_key()};
    vector<crypto::key_bytes> server_keys {crypto::derive_public_key(crypto::generate_secret_key()), crypto::derive_public_key(crypto::generate_secret_key())};
    SecureChannel client {server_keys};
    SecureChannel server {identity};

    vector<uint8_t> offer;
    vector<uint8_t> plaintext;
    client.TakeHandshakeOutput(offer);

    CHECK_THROWS_AS(server.Receive(offer, plaintext), SecureChannelException);
    CHECK_FALSE(server.IsEstablished());
    CHECK_FALSE(server.HasHandshakeOutput());
    CHECK_THROWS_AS(server.Receive(offer, plaintext), SecureChannelException);
}

TEST_CASE("SecureChannelRefusesMalformedFrames")
{
    SecureChannelIdentity identity {crypto::generate_secret_key()};
    vector<crypto::key_bytes> server_keys {identity.GetPublicKey()};
    vector<uint8_t> plaintext;

    // Only the length header is delivered: a frame the channel would refuse is refused before it is buffered
    SECTION("an offer size that fits no offer is refused at the header")
    {
        for (uint16_t frame_size : {uint16_t {0}, uint16_t {1}, uint16_t {48}, uint16_t {50}, uint16_t {1 + 5 * NoiseHandshakeNK::MESSAGE_OVERHEAD}, uint16_t {0xFFFF}}) {
            INFO(frame_size);

            SecureChannel server {identity};
            vector<uint8_t> header {numeric_cast<uint8_t>(frame_size >> 8), numeric_cast<uint8_t>(frame_size & 0xFF)};

            CHECK_THROWS_AS(server.Receive(header, plaintext), SecureChannelException);
        }
    }

    SECTION("an offer count that disagrees with its size is refused")
    {
        SecureChannel client {server_keys};
        SecureChannel server {identity};
        vector<uint8_t> offer;
        client.TakeHandshakeOutput(offer);
        offer[2] = 2;

        CHECK_THROWS_AS(server.Receive(offer, plaintext), SecureChannelException);
    }

    SECTION("an answer to an offer that was never made is refused")
    {
        SecureChannel client {server_keys};
        SecureChannel server {identity};
        vector<uint8_t> offer;
        vector<uint8_t> answer;
        client.TakeHandshakeOutput(offer);
        server.Receive(offer, plaintext);
        server.TakeHandshakeOutput(answer);
        answer[2] = 1;

        CHECK_THROWS_AS(client.Receive(answer, plaintext), SecureChannelException);
        CHECK_FALSE(client.IsEstablished());
    }

    SECTION("an answer of the wrong size is refused at the header")
    {
        SecureChannel client {server_keys};
        vector<uint8_t> header {0x00, numeric_cast<uint8_t>(NoiseHandshakeNK::MESSAGE_OVERHEAD)};

        CHECK_THROWS_AS(client.Receive(header, plaintext), SecureChannelException);
    }

    SECTION("a transport frame too short for its tag is refused")
    {
        SecureChannel client {server_keys};
        SecureChannel server {identity};
        Exchange(client, server, 64);

        vector<uint8_t> frame {0x00, 0x0F};
        frame.resize(frame.size() + 15);

        CHECK_THROWS_AS(server.Receive(frame, plaintext), SecureChannelException);
    }
}

TEST_CASE("SecureChannelRefusesTamperingReplayAndLoss")
{
    SecureChannelIdentity identity {crypto::generate_secret_key()};
    vector<crypto::key_bytes> server_keys {identity.GetPublicKey()};
    SecureChannel client {server_keys};
    SecureChannel server {identity};
    Exchange(client, server, 64);

    vector<uint8_t> first;
    vector<uint8_t> second;
    client.Seal(vector<uint8_t> {1, 2, 3, 4}, first);
    client.Seal(vector<uint8_t> {5, 6, 7, 8}, second);
    vector<uint8_t> plaintext;

    SECTION("a changed byte")
    {
        for (size_t i = 2; i < first.size(); i++) {
            SecureChannel fresh_client {server_keys};
            SecureChannel fresh_server {identity};
            Exchange(fresh_client, fresh_server, 64);

            vector<uint8_t> sealed;
            fresh_client.Seal(vector<uint8_t> {1, 2, 3, 4}, sealed);
            sealed[i] ^= 0x10;

            CHECK_THROWS_AS(fresh_server.Receive(sealed, plaintext), SecureChannelException);
        }
    }

    SECTION("a replayed frame")
    {
        server.Receive(first, plaintext);

        CHECK(plaintext == vector<uint8_t> {1, 2, 3, 4});
        CHECK_THROWS_AS(server.Receive(first, plaintext), SecureChannelException);
    }

    SECTION("a dropped frame")
    {
        CHECK_THROWS_AS(server.Receive(second, plaintext), SecureChannelException);
    }

    SECTION("a frame sent back to its sender")
    {
        CHECK_THROWS_AS(client.Receive(first, plaintext), SecureChannelException);
    }
}

TEST_CASE("SecureChannelHandshakeEdges")
{
    SecureChannelIdentity identity {crypto::generate_secret_key()};

    SECTION("a client needs at least one pin and at most the offer limit")
    {
        CHECK_THROWS_AS(SecureChannel(vector<crypto::key_bytes> {}), SecureChannelException);

        vector<crypto::key_bytes> too_many(SecureChannel::MAX_OFFERED_KEYS + 1, identity.GetPublicKey());

        CHECK_THROWS_AS(SecureChannel(too_many), SecureChannelException);
    }

    SECTION("nothing is sealed before the handshake completes")
    {
        vector<crypto::key_bytes> server_keys {identity.GetPublicKey()};
        SecureChannel client {server_keys};
        SecureChannel server {identity};
        vector<uint8_t> out;

        CHECK_THROWS(client.Seal(vector<uint8_t> {1}, out));
        CHECK_THROWS(server.Seal(vector<uint8_t> {1}, out));
    }

    SECTION("a handshake cut off halfway leaves both sides waiting")
    {
        vector<crypto::key_bytes> server_keys {identity.GetPublicKey()};
        SecureChannel client {server_keys};
        SecureChannel server {identity};
        vector<uint8_t> offer;
        vector<uint8_t> plaintext;
        client.TakeHandshakeOutput(offer);
        server.Receive(const_span<uint8_t> {offer}.first(offer.size() / 2), plaintext);

        CHECK_FALSE(server.IsEstablished());
        CHECK_FALSE(server.HasHandshakeOutput());
        CHECK_FALSE(client.IsEstablished());
    }

    SECTION("the server answers with its handshake frame ahead of the first sealed one")
    {
        vector<crypto::key_bytes> server_keys {identity.GetPublicKey()};
        SecureChannel client {server_keys};
        SecureChannel server {identity};
        vector<uint8_t> offer;
        vector<uint8_t> plaintext;
        client.TakeHandshakeOutput(offer);
        server.Receive(offer, plaintext);

        vector<uint8_t> output;
        server.Seal(vector<uint8_t> {9, 9, 9}, output);
        client.Receive(output, plaintext);

        CHECK(client.IsEstablished());
        CHECK(plaintext == vector<uint8_t> {9, 9, 9});
    }

    SECTION("keys come from settings as 64 hex digits")
    {
        CHECK(ParseSecureChannelKey(crypto::format_key(identity.GetPublicKey()), "Test.Key") == identity.GetPublicKey());
        CHECK_THROWS_AS(ParseSecureChannelKey("", "Test.Key"), SecureChannelException);
        CHECK_THROWS_AS(ParseSecureChannelKey("abc", "Test.Key"), SecureChannelException);
    }
}

namespace
{
    // A server side as ServerEngine runs it, minus the engine: every accepted transport gets a ServerConnection
    class ChannelTestServer final
    {
    public:
        explicit ChannelTestServer(GlobalSettings& settings) :
            _settings {&settings},
            _identity {crypto::generate_secret_key()}
        {
        }

        [[nodiscard]] auto GetIdentity() const noexcept -> const SecureChannelIdentity& { return _identity; }

        [[nodiscard]] auto GetConnection() -> nptr<ServerConnection>
        {
            std::scoped_lock locker {_connectionsLocker};

            return !_connections.empty() ? _connections.front().get() : nullptr;
        }

        auto MakeAcceptCallback() -> NetworkServer::NewConnectionCallback
        {
            return [this](shared_ptr<NetworkServerConnection> net_connection) {
                auto connection = safe_alloc::make_unique<ServerConnection>(_settings, std::move(net_connection), _identity);
                std::scoped_lock locker {_connectionsLocker};
                _connections.emplace_back(std::move(connection));
            };
        }

        void Clear()
        {
            std::scoped_lock locker {_connectionsLocker};
            _connections.clear();
        }

    private:
        ptr<ServerNetworkSettings> _settings;
        SecureChannelIdentity _identity;
        std::mutex _connectionsLocker;
        vector<unique_ptr<ServerConnection>> _connections;
    };

    enum class ChannelTestTransport : uint8_t
    {
        Tcp,
        Udp,
    };

    auto StartChannelTestServer(ChannelTestTransport transport, GlobalSettings& settings, ChannelTestServer& test_server, uint16_t& port) -> unique_ptr<NetworkServer>
    {
        string startup_error;

        for (int32_t attempt = 0; attempt != 64; ++attempt) {
            port = TestChannelPort.fetch_add(1);
            BakerTests::OverrideSetting(settings.Network.ServerPort, port);

            try {
                if (transport == ChannelTestTransport::Udp) {
                    return NetworkServer::StartUdpSocketsServer(&settings, test_server.MakeAcceptCallback());
                }

#if FO_HAVE_ASIO
                return NetworkServer::StartAsioServer(&settings, test_server.MakeAcceptCallback());
#else
                throw std::runtime_error("TCP server transport is not built");
#endif
            }
            catch (const std::exception& ex) {
                startup_error = ex.what();
            }
        }

        throw std::runtime_error(startup_error.c_str());
    }

    // Reads every message the client sent so far; the answer to its handshake makes the connect succeed
    void ServeClientMessages(ServerConnection& connection, vector<NetMessage>& received)
    {
        bool answer_handshake = false;

        {
            auto in_buf = connection.ReadBuf();

            while (in_buf->NeedProcess()) {
                NetMessage msg = in_buf->ReadMsg();
                received.emplace_back(msg);

                if (msg == NetMessage::Handshake) {
                    (void)in_buf->Read<string>();
                    (void)in_buf->Read<string>();
                    CHECK(in_buf->Read<uint32_t>() == FO_UPDATER_VERSION);
                    (void)in_buf->Read<string>();
                    answer_handshake = true;
                }
                else if (msg == NetMessage::Ping) {
                    (void)in_buf->Read<bool>();
                }
                else {
                    FAIL("Unexpected client message");
                }
            }

            in_buf->ShrinkReadBuf();
        }

        if (answer_handshake) {
            auto out_buf = connection.WriteMsg(NetMessage::HandshakeAnswer);
            out_buf->Write(false);
            out_buf->Write(false);
            out_buf->Write(false);
            out_buf->Write(string_view {"channel-test"});
        }
    }

    void RunClientAgainstServer(ChannelTestTransport transport, bool pin_server_key)
    {
        REQUIRE(net_sockets::startup());

        auto server_settings = MakeTestSettings();
        auto client_settings = MakeTestSettings();
        ChannelTestServer test_server {server_settings};
        uint16_t port = 0;
        unique_ptr<NetworkServer> server = StartChannelTestServer(transport, server_settings, test_server, port);

        // Connections hold asio objects, so they go before the io context they reference
        auto shutdown_server = scope_exit([&]() noexcept {
            safe_call([&] { test_server.Clear(); });
            safe_call([&server] { server->Shutdown(); });
        });

        crypto::key_bytes pinned_key = pin_server_key ? test_server.GetIdentity().GetPublicKey() : crypto::derive_public_key(crypto::generate_secret_key());
        BakerTests::OverrideSetting(client_settings.ClientNetwork.ServerHost, string {"127.0.0.1"});
        BakerTests::OverrideSetting(client_settings.Network.ServerPort, port);
        BakerTests::OverrideSetting(client_settings.ClientNetwork.UseUdp, transport == ChannelTestTransport::Udp);
        BakerTests::OverrideSetting(client_settings.ClientNetwork.ChannelServerKeys, vector<string> {crypto::format_key(pinned_key)});

        optional<ClientConnection::ConnectResult> connect_result;
        ClientConnection client {&client_settings};
        client.SetConnectHandler([&](ClientConnection::ConnectResult result) { connect_result = result; });
        client.Connect();

        vector<NetMessage> server_received;
        bool server_pinged = false;

        for (int32_t i = 0; i < 1000; i++) {
            client.Process();

            if (auto connection = test_server.GetConnection()) {
                // What ServerEngine::ProcessConnection does with a latched rejection
                if (connection->IsInputRejected() && !connection->IsHardDisconnected()) {
                    connection->HardDisconnect(DisconnectReason::ProtocolError);
                }

                ServeClientMessages(*connection, server_received);

                if (connect_result == ClientConnection::ConnectResult::Success && !server_pinged) {
                    auto out_buf = connection->WriteMsg(NetMessage::Ping);
                    out_buf->Write(false);
                    server_pinged = true;
                }
            }

            bool pong_received = std::ranges::count(server_received, NetMessage::Ping) >= 2;

            if ((connect_result.has_value() && connect_result != ClientConnection::ConnectResult::Success) || pong_received) {
                break;
            }

            coarse_sleep(std::chrono::milliseconds {5});
        }

        auto connection = test_server.GetConnection();
        REQUIRE(connection);

        if (pin_server_key) {
            REQUIRE(connect_result == ClientConnection::ConnectResult::Success);
            CHECK(client.IsConnected());
            CHECK_FALSE(connection->IsInputRejected());
            REQUIRE(server_received.size() >= 3);
            CHECK(server_received.front() == NetMessage::Handshake);
            // The server's own ping came back answered, so the stream runs both ways after the handshake
            CHECK(std::ranges::count(server_received, NetMessage::Ping) >= 2);
        }
        else {
            CHECK(connect_result == ClientConnection::ConnectResult::Failed);
            CHECK_FALSE(client.IsConnected());
            CHECK(connection->IsInputRejected());
            CHECK(server_received.empty());
        }

        client.Disconnect();
    }
}

#if FO_HAVE_ASIO
TEST_CASE("SecureChannelConnectsClientAndServerOverTcp")
{
    RunClientAgainstServer(ChannelTestTransport::Tcp, true);
}

TEST_CASE("SecureChannelTcpClientRefusesAnUnpinnedServer")
{
    RunClientAgainstServer(ChannelTestTransport::Tcp, false);
}
#endif

TEST_CASE("SecureChannelConnectsClientAndServerOverUdp")
{
    RunClientAgainstServer(ChannelTestTransport::Udp, true);
}

TEST_CASE("SecureChannelUdpClientRefusesAnUnpinnedServer")
{
    RunClientAgainstServer(ChannelTestTransport::Udp, false);
}

TEST_CASE("SecureChannelClientWithoutPinsNeverConnects")
{
    REQUIRE(net_sockets::startup());

    auto server_settings = MakeTestSettings();
    auto client_settings = MakeTestSettings();
    ChannelTestServer test_server {server_settings};
    uint16_t port = 0;
    unique_ptr<NetworkServer> server = StartChannelTestServer(ChannelTestTransport::Udp, server_settings, test_server, port);
    auto shutdown_server = scope_exit([&]() noexcept {
        safe_call([&] { test_server.Clear(); });
        safe_call([&server] { server->Shutdown(); });
    });

    BakerTests::OverrideSetting(client_settings.ClientNetwork.ServerHost, string {"127.0.0.1"});
    BakerTests::OverrideSetting(client_settings.Network.ServerPort, port);
    BakerTests::OverrideSetting(client_settings.ClientNetwork.UseUdp, true);

    optional<ClientConnection::ConnectResult> connect_result;
    ClientConnection client {&client_settings};
    client.SetConnectHandler([&](ClientConnection::ConnectResult result) { connect_result = result; });
    client.Connect();

    for (int32_t i = 0; i < 1000 && !connect_result.has_value(); i++) {
        client.Process();
        coarse_sleep(std::chrono::milliseconds {5});
    }

    CHECK(connect_result == ClientConnection::ConnectResult::Failed);
}

namespace
{
    // The transport is driven by hand, so every byte the server hands it can be inspected
    class ProbeConnection final : public NetworkServerConnection
    {
    public:
        explicit ProbeConnection(ptr<ServerNetworkSettings> settings) :
            NetworkServerConnection(settings)
        {
        }

        using NetworkServerConnection::SendCallback;

        void Receive(const_span<uint8_t> buf) { ReceiveCallback(buf); }

    private:
        void DispatchImpl() override { }
        void DisconnectImpl() override { }
    };
}

TEST_CASE("SecureChannelServerSendsNothingInTheClear")
{
    auto settings = MakeTestSettings();
    BakerTests::OverrideSetting(settings.Network.DisableZlibCompression, true);

    SecureChannelIdentity identity {crypto::generate_secret_key()};
    auto net_connection = safe_alloc::make_shared<ProbeConnection>(&settings);
    auto connection = safe_alloc::make_unique<ServerConnection>(&settings, net_connection, identity);

    // Written before the client's channel stands, so it has to wait instead of leaving unsealed
    {
        auto out_buf = connection->WriteMsg(NetMessage::Ping);
        out_buf->Write(false);
    }

    CHECK(net_connection->SendCallback().empty());

    vector<crypto::key_bytes> server_keys {identity.GetPublicKey()};
    SecureChannel client {server_keys};
    vector<uint8_t> offer;
    client.TakeHandshakeOutput(offer);
    net_connection->Receive(offer);

    vector<uint8_t> wire = net_connection->SendCallback();
    uint32_t signature = NetBuffer::NETMSG_SIGNATURE;

    CHECK_FALSE(ContainsSequence(wire, make_const_span(&signature, sizeof(signature))));

    vector<uint8_t> plaintext;
    client.Receive(wire, plaintext);

    REQUIRE(client.IsEstablished());

    NetInBuffer in_buf {64};
    in_buf.AddData(plaintext);

    REQUIRE(in_buf.NeedProcess());
    CHECK(in_buf.ReadMsg() == NetMessage::Ping);
    CHECK_FALSE(in_buf.Read<bool>());
}

namespace
{
    // The protocol before the secure channel is frozen in shipped clients, so these helpers spell it out byte by byte
    // instead of borrowing NetBuffer, which is free to change
    constexpr uint32_t PRE_CHANNEL_SIGNATURE = 0x011E9422;

    void AppendLittleEndian(vector<uint8_t>& out, uint32_t value)
    {
        for (size_t i = 0; i < sizeof(value); i++) {
            out.emplace_back(numeric_cast<uint8_t>((value >> (i * 8)) & 0xFF));
        }
    }

    // What such a client sent first: its handshake message, in the clear
    auto MakePreChannelHandshake() -> vector<uint8_t>
    {
        vector<uint8_t> body;

        auto append_string = [&body](string_view value) {
            AppendLittleEndian(body, numeric_cast<uint32_t>(value.size()));
            body.insert(body.end(), value.begin(), value.end());
        };

        body.emplace_back(uint8_t {1}); // NetMessage::Handshake
        append_string("4fdb3c2a6d929a23");
        append_string("");
        AppendLittleEndian(body, 2);
        append_string("Windows-win64");
        AppendLittleEndian(body, 0x5A3C1E77);

        vector<uint8_t> message;
        AppendLittleEndian(message, PRE_CHANNEL_SIGNATURE);
        AppendLittleEndian(message, numeric_cast<uint32_t>(sizeof(uint32_t) * 2 + body.size()));
        message.insert(message.end(), body.begin(), body.end());
        return message;
    }

    struct PreChannelAnswer
    {
        bool CompatibilityOutdated {};
        bool UpdaterOutdated {};
        bool MetadataOutdated {};
        string MetadataVersion {};
    };

    // Reads the reply as such a client did: a zlib stream unless compression is off, holding one handshake answer
    // that must be consumed exactly
    auto ReadPreChannelAnswer(const_span<uint8_t> wire, bool compressed) -> PreChannelAnswer
    {
        vector<uint8_t> message;

        if (compressed) {
            stream_decompressor decompressor;
            decompressor.decompress(wire, message);
        }
        else {
            message.assign(wire.begin(), wire.end());
        }

        size_t pos = 0;

        auto read_u32 = [&]() -> uint32_t {
            REQUIRE(pos + sizeof(uint32_t) <= message.size());
            uint32_t value = 0;

            for (size_t i = 0; i < sizeof(uint32_t); i++) {
                value |= numeric_cast<uint32_t>(message[pos + i]) << (i * 8);
            }

            pos += sizeof(uint32_t);
            return value;
        };
        auto read_bool = [&]() -> bool {
            REQUIRE(pos < message.size());
            uint8_t value = message[pos++];
            REQUIRE(value <= 1);
            return value != 0;
        };

        CHECK(read_u32() == PRE_CHANNEL_SIGNATURE);
        CHECK(read_u32() == message.size());
        REQUIRE(pos < message.size());
        CHECK(message[pos++] == 3); // NetMessage::HandshakeAnswer

        PreChannelAnswer answer;
        answer.CompatibilityOutdated = read_bool();
        answer.UpdaterOutdated = read_bool();
        answer.MetadataOutdated = read_bool();

        uint32_t version_size = read_u32();
        REQUIRE(pos + version_size <= message.size());
        answer.MetadataVersion.assign(reinterpret_cast<const char*>(message.data() + pos), version_size);
        pos += version_size;

        (void)read_u32(); // Encryption key
        CHECK(pos == message.size());
        return answer;
    }
}

TEST_CASE("SecureChannelServerTellsAClientFromBeforeTheChannelToUpdate")
{
    bool compressed = GENERATE(true, false);
    CAPTURE(compressed);

    auto settings = MakeTestSettings();
    BakerTests::OverrideSetting(settings.Network.DisableZlibCompression, !compressed);

    SecureChannelIdentity identity {crypto::generate_secret_key()};
    auto net_connection = safe_alloc::make_shared<ProbeConnection>(&settings);
    auto connection = safe_alloc::make_unique<ServerConnection>(&settings, net_connection, identity);
    vector<uint8_t> handshake = MakePreChannelHandshake();

    net_connection->Receive(handshake);

    // The updater of such a client checks this flag first, then shows its install-the-latest-client message
    PreChannelAnswer answer = ReadPreChannelAnswer(net_connection->SendCallback(), compressed);

    CHECK(answer.UpdaterOutdated);
    CHECK(answer.CompatibilityOutdated);
    CHECK_FALSE(answer.MetadataOutdated);
    CHECK(answer.MetadataVersion.empty());
    CHECK_FALSE(connection->IsInputRejected());

    // It closes the connection itself once refused, and whatever it sends meanwhile is neither read nor answered
    net_connection->Receive(handshake);

    CHECK(net_connection->SendCallback().empty());
    CHECK_FALSE(connection->IsInputRejected());
    CHECK(connection->ReadBuf()->GetBufferedUnreadSize() == 0);
}

TEST_CASE("SecureChannelServerRejectsAStreamThatOnlyBeginsLikeAClientFromBeforeTheChannel")
{
    auto settings = MakeTestSettings();
    SecureChannelIdentity identity {crypto::generate_secret_key()};
    auto net_connection = safe_alloc::make_shared<ProbeConnection>(&settings);
    auto connection = safe_alloc::make_unique<ServerConnection>(&settings, net_connection, identity);
    vector<uint8_t> handshake = MakePreChannelHandshake();

    SECTION("a signature with one byte changed")
    {
        handshake[3] = uint8_t {0x02};
        net_connection->Receive(handshake);
    }

    // Only the first read of a connection can carry it
    SECTION("the signature after other bytes")
    {
        net_connection->Receive(vector<uint8_t> {0x00});
        net_connection->Receive(handshake);
    }

    CHECK(connection->IsInputRejected());
    CHECK(net_connection->SendCallback().empty());
}

namespace
{
    // The transports carry the bytes they carried before the channel, so a bare one plays such a client end to end
    void RunPreChannelClientAgainstServer(ChannelTestTransport transport)
    {
        REQUIRE(net_sockets::startup());

        auto server_settings = MakeTestSettings();
        auto client_settings = MakeTestSettings();
        ChannelTestServer test_server {server_settings};
        uint16_t port = 0;
        unique_ptr<NetworkServer> server = StartChannelTestServer(transport, server_settings, test_server, port);

        auto shutdown_server = scope_exit([&]() noexcept {
            safe_call([&] { test_server.Clear(); });
            safe_call([&server] { server->Shutdown(); });
        });

        BakerTests::OverrideSetting(client_settings.ClientNetwork.ServerHost, string {"127.0.0.1"});
        BakerTests::OverrideSetting(client_settings.Network.ServerPort, port);

        unique_ptr<NetworkClientConnection> client = transport == ChannelTestTransport::Udp ? NetworkClientConnection::CreateUdpSocketsConnection(&client_settings) : NetworkClientConnection::CreateSocketsConnection(&client_settings);
        vector<uint8_t> handshake = MakePreChannelHandshake();
        size_t sent_size = 0;
        vector<uint8_t> wire;
        int32_t quiet_passes = 0;

        // The refusal is one short write, so a few quiet passes after its first bytes mean it has all arrived
        for (int32_t i = 0; i < 1000 && quiet_passes < 20; i++) {
            if (client->IsConnecting()) {
                (void)client->CheckStatus(true);
            }
            else if (client->IsConnected()) {
                if (sent_size < handshake.size() && client->CheckStatus(true)) {
                    sent_size += client->SendData(const_span<uint8_t> {handshake}.subspan(sent_size));
                }

                if (client->CheckStatus(false)) {
                    const_span<uint8_t> data = client->ReceiveData();
                    wire.insert(wire.end(), data.begin(), data.end());
                    quiet_passes = 0;
                }
                else if (!wire.empty()) {
                    quiet_passes++;
                }
            }

            coarse_sleep(std::chrono::milliseconds {5});
        }

        REQUIRE(sent_size == handshake.size());
        REQUIRE_FALSE(wire.empty());

        PreChannelAnswer answer = ReadPreChannelAnswer(wire, true);

        CHECK(answer.UpdaterOutdated);
        CHECK(client->IsConnected());

        auto connection = test_server.GetConnection();
        REQUIRE(connection);
        CHECK_FALSE(connection->IsInputRejected());

        client->Disconnect();
    }
}

#if FO_HAVE_ASIO
TEST_CASE("SecureChannelTcpServerTellsAClientFromBeforeTheChannelToUpdate")
{
    RunPreChannelClientAgainstServer(ChannelTestTransport::Tcp);
}
#endif

TEST_CASE("SecureChannelUdpServerTellsAClientFromBeforeTheChannelToUpdate")
{
    RunPreChannelClientAgainstServer(ChannelTestTransport::Udp);
}

#if FO_HAVE_WEB_SOCKETS
TEST_CASE("SecureChannelRunsOverWebSockets")
{
    REQUIRE(net_sockets::startup());

    auto settings = MakeTestSettings();
    BakerTests::OverrideSetting(settings.Network.SecuredWebSockets, false);
    BakerTests::OverrideSetting(settings.Network.DisableZlibCompression, true);

    ChannelTestServer test_server {settings};
    uint16_t port = 0;
    unique_nptr<NetworkServer> server;
    string startup_error;

    for (int32_t attempt = 0; attempt != 64 && !server; ++attempt) {
        port = TestChannelPort.fetch_add(1);
        BakerTests::OverrideSetting(settings.Network.WebSocketPort, static_cast<int32_t>(port));

        try {
            server = NetworkServer::StartWebSocketsServer(&settings, test_server.MakeAcceptCallback());
        }
        catch (const std::exception& ex) {
            startup_error = ex.what();
        }
    }

    CAPTURE(startup_error);
    REQUIRE(server);

    auto shutdown_server = scope_exit([&]() noexcept {
        safe_call([&] { test_server.Clear(); });
        safe_call([&server] { server->Shutdown(); });
    });

    // The browser client runs the same ClientConnection, so here the channel is driven by hand over a real WebSocket
    vector<crypto::key_bytes> server_keys {test_server.GetIdentity().GetPublicKey()};
    SecureChannel client_channel {server_keys};
    std::mutex client_locker;
    vector<uint8_t> client_plaintext;

    websocketpp::client<websocketpp::config::asio_client> client;
    client.clear_access_channels(websocketpp::log::alevel::all);
    client.clear_error_channels(websocketpp::log::elevel::all);
    client.init_asio();

    client.set_open_handler([&](const websocketpp::connection_hdl& hdl) {
        std::scoped_lock locker {client_locker};
        vector<uint8_t> offer;
        client_channel.TakeHandshakeOutput(offer);
        std::error_code send_error;
        client.send(hdl, offer.data(), offer.size(), websocketpp::frame::opcode::binary, send_error);
    });

    client.set_message_handler([&](const websocketpp::connection_hdl& hdl, const websocketpp::client<websocketpp::config::asio_client>::message_ptr& msg) {
        std::scoped_lock locker {client_locker};
        const std::string& payload = msg->get_payload();
        bool was_established = client_channel.IsEstablished();
        client_channel.Receive(make_const_span(payload.data(), payload.size()), client_plaintext);

        if (!was_established && client_channel.IsEstablished()) {
            NetOutBuffer out_buf {64};
            out_buf.StartMsg(NetMessage::Ping);
            out_buf.Write(true);
            out_buf.EndMsg();

            vector<uint8_t> sealed;
            client_channel.Seal(out_buf.GetData(), sealed);
            std::error_code send_error;
            client.send(hdl, sealed.data(), sealed.size(), websocketpp::frame::opcode::binary, send_error);
        }
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

    bool ping_received = false;

    for (int32_t i = 0; i < 1000 && !ping_received; i++) {
        if (auto server_connection = test_server.GetConnection()) {
            auto in_buf = server_connection->ReadBuf();

            if (in_buf->NeedProcess()) {
                REQUIRE(in_buf->ReadMsg() == NetMessage::Ping);
                CHECK(in_buf->Read<bool>());
                in_buf->ShrinkReadBuf();
                ping_received = true;
            }
        }

        coarse_sleep(std::chrono::milliseconds {5});
    }

    CHECK(ping_received);

    {
        std::scoped_lock locker {client_locker};
        CHECK(client_channel.IsEstablished());
        CHECK(client_plaintext.empty());
    }
}
#endif

FO_END_NAMESPACE
