// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

#include "Test_ClientServerIntegrationHelpers.h"

FO_BEGIN_NAMESPACE

using namespace TestClientServerIntegration;

TEST_CASE("ServerRejectsMalformedPreHandshakePayloadWithoutExceptionReport")
{
    using namespace TestClientServerIntegration;

    auto port {IntegrationTestPort.fetch_add(1)};
    auto server_settings {MakeServerTestSettings(port)};
    auto server {TestClientServerIntegration::MakeServerEngine(server_settings)};

    auto shutdown {scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    })};

    string startup_error {TestClientServerIntegration::WaitForServerStart(server)};
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(HasInterthreadListener(port));

    auto previous_exception_callback {exceptions::get_callback()};
    std::atomic_int exception_reports {};
    exceptions::set_callback([&exception_reports](string_view, const stack_trace::catched_data&, bool) { exception_reports.fetch_add(1); });
    auto restore_exception_callback {scope_exit([previous = std::move(previous_exception_callback)]() mutable noexcept { exceptions::set_callback(std::move(previous)); })};

    RawChannelClient client {port};
    REQUIRE(WaitForServerConnectionCount(server, 1));

    auto malformed_handshake {NetOutBuffer(64)};
    malformed_handshake.StartMsg(NetMessage::Handshake);
    malformed_handshake.Write<uint32_t>(std::numeric_limits<uint32_t>::max());
    malformed_handshake.Write<uint16_t>(uint16_t {0});
    malformed_handshake.EndMsg();
    client.Send(malformed_handshake.GetData());

    REQUIRE(WaitForServerConnectionCount(server, 0));
    CHECK(client.IsDisconnected());
    CHECK(exception_reports.load() == 0);
}

TEST_CASE("ServerRejectsNonHandshakeFirstMessageWithoutExceptionReport")
{
    auto port {IntegrationTestPort.fetch_add(1)};
    auto server_settings {MakeServerTestSettings(port)};
    BakerTests::OverrideSetting(server_settings.Network.DisableZlibCompression, true);
    auto server {TestClientServerIntegration::MakeServerEngine(server_settings)};

    auto shutdown {scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    })};

    string startup_error {TestClientServerIntegration::WaitForServerStart(server)};
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(HasInterthreadListener(port));

    auto previous_exception_callback {exceptions::get_callback()};
    std::atomic_int exception_reports {};
    exceptions::set_callback([&exception_reports](string_view, const stack_trace::catched_data&, bool) { exception_reports.fetch_add(1); });
    auto restore_exception_callback {scope_exit([previous = std::move(previous_exception_callback)]() mutable noexcept { exceptions::set_callback(std::move(previous)); })};

    RawChannelClient live_client {port};
    RawChannelClient faulty_client {port};
    REQUIRE(WaitForServerConnectionCount(server, 2));
    SendCompatibleHandshake(live_client, server);
    REQUIRE(WaitForServerMessageCount(live_client, NetMessage::InitData, 1));

    auto ping {NetOutBuffer(64)};
    ping.StartMsg(NetMessage::Ping);
    ping.Write(false);
    ping.Write(nanotime::now().milliseconds());
    ping.EndMsg();
    live_client.Send(ping.GetData());
    REQUIRE(WaitForServerMessageCount(live_client, NetMessage::TimeSync, 1));

    faulty_client.Send(ping.GetData());
    CHECK(WaitForServerConnectionCount(server, 1));
    CHECK(faulty_client.IsDisconnected());
    CHECK(exception_reports.load() == 0);
    CHECK_FALSE(live_client.IsDisconnected());
    live_client.Send(ping.GetData());
    CHECK(WaitForServerMessageCount(live_client, NetMessage::TimeSync, 2));
    CHECK(exception_reports.load() == 0);
}

TEST_CASE("ServerRejectsMalformedPingAfterHandshakeWithoutExceptionReport")
{
    using namespace TestClientServerIntegration;

    // A ping body is the answer flag and the sender clock; the sizes cover a missing flag, a clock cut short (the
    // frame read in the field is the flag alone) and bytes left over after the clock
    size_t ping_body_size {GENERATE(size_t {0}, size_t {1}, size_t {5}, size_t {10})};
    CAPTURE(ping_body_size);

    auto port {IntegrationTestPort.fetch_add(1)};
    auto server_settings {MakeServerTestSettings(port)};
    BakerTests::OverrideSetting(server_settings.Network.DisableZlibCompression, true);
    auto server {TestClientServerIntegration::MakeServerEngine(server_settings)};

    auto shutdown {scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    })};

    string startup_error {TestClientServerIntegration::WaitForServerStart(server)};
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(HasInterthreadListener(port));

    auto previous_exception_callback {exceptions::get_callback()};
    std::atomic_int exception_reports {};
    exceptions::set_callback([&exception_reports](string_view, const stack_trace::catched_data&, bool) { exception_reports.fetch_add(1); });
    auto restore_exception_callback {scope_exit([previous = std::move(previous_exception_callback)]() mutable noexcept { exceptions::set_callback(std::move(previous)); })};

    auto send_ping {[](RawChannelClient& client, optional<size_t> body_size) {
        NetOutBuffer ping {64};
        ping.StartMsg(NetMessage::Ping);

        if (body_size) {
            vector<uint8_t> body {vector<uint8_t>(*body_size)};
            ping.Push(body);
        }
        else {
            ping.Write(false);
            ping.Write(nanotime::now().milliseconds());
        }

        ping.EndMsg();
        client.Send(ping.GetData());
    }};

    RawChannelClient live_client {port};
    RawChannelClient faulty_client {port};
    REQUIRE(WaitForServerConnectionCount(server, 2));

    SendCompatibleHandshake(live_client, server);
    SendCompatibleHandshake(faulty_client, server);
    REQUIRE(WaitForServerMessageCount(live_client, NetMessage::InitData, 1));
    REQUIRE(WaitForServerMessageCount(faulty_client, NetMessage::InitData, 1));

    // Control: a valid ping on the same completed handshake is answered, so the fault below is the ping itself
    send_ping(live_client, {});
    send_ping(faulty_client, {});
    REQUIRE(WaitForServerMessageCount(live_client, NetMessage::TimeSync, 1));
    REQUIRE(WaitForServerMessageCount(faulty_client, NetMessage::TimeSync, 1));

    send_ping(faulty_client, ping_body_size);

    REQUIRE(WaitForServerConnectionCount(server, 1));
    CHECK(faulty_client.IsDisconnected());
    CHECK(exception_reports.load() == 0);

    // The independent connection keeps being served after the faulty one is gone
    CHECK_FALSE(live_client.IsDisconnected());
    send_ping(live_client, {});
    CHECK(WaitForServerMessageCount(live_client, NetMessage::TimeSync, 2));
    CHECK(GetServerConnectionCount(server) == 1);
    CHECK(exception_reports.load() == 0);
}

TEST_CASE("ServerRejectsForgedRemoteCallWithoutExceptionReport")
{
    using namespace TestClientServerIntegration;

    // Forged calls the server refuses for that connection alone: an undeclared name, a bad size, a payload that
    // does not fit the declared arguments or leaves bytes over
    enum class Forgery
    {
        UnknownCall,
        NegativeSize,
        SizePastFrame,
        ShortPayload,
        TrailingBytes,
    };

    Forgery forgery {GENERATE(Forgery::UnknownCall, Forgery::NegativeSize, Forgery::SizePastFrame, Forgery::ShortPayload, Forgery::TrailingBytes)};
    CAPTURE(static_cast<int32_t>(forgery));

    auto port {IntegrationTestPort.fetch_add(1)};
    auto server_settings {MakeServerTestSettings(port)};
    BakerTests::OverrideSetting(server_settings.Network.DisableZlibCompression, true);
    auto server {MakeServerEngine(server_settings)};

    auto shutdown {scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    })};

    string startup_error {WaitForServerStart(server)};
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(HasInterthreadListener(port));

    auto previous_exception_callback {exceptions::get_callback()};
    std::atomic_int exception_reports {};
    exceptions::set_callback([&exception_reports](string_view, const stack_trace::catched_data&, bool) { exception_reports.fetch_add(1); });
    auto restore_exception_callback {scope_exit([previous = std::move(previous_exception_callback)]() mutable noexcept { exceptions::set_callback(std::move(previous)); })};

    auto send_ping {[](RawChannelClient& client) {
        NetOutBuffer ping {64};
        ping.StartMsg(NetMessage::Ping);
        ping.Write(false);
        ping.Write(nanotime::now().milliseconds());
        ping.EndMsg();
        client.Send(ping.GetData());
    }};

    auto send_remote_call {[&](RawChannelClient& client, Forgery kind) {
        NetOutBuffer call {64};
        call.StartMsg(NetMessage::RemoteCall);

        switch (kind) {
        case Forgery::UnknownCall:
            call.Write<hstring>(server->Hashes.to_hashed_string("UnitTestNoSuchRemoteCall"));
            call.Write<int32_t>(0);
            break;
        case Forgery::NegativeSize:
            call.Write<hstring>(server->Hashes.to_hashed_string("UnitTestWorldStep"));
            call.Write<int32_t>(-1);
            break;
        case Forgery::SizePastFrame:
            call.Write<hstring>(server->Hashes.to_hashed_string("UnitTestWorldStep"));
            call.Write<int32_t>(4096);
            call.Write<int32_t>(7);
            break;
        case Forgery::ShortPayload:
            call.Write<hstring>(server->Hashes.to_hashed_string("UnitTestWorldStep"));
            call.Write<int32_t>(2);
            call.Write<int16_t>(7);
            break;
        case Forgery::TrailingBytes:
            call.Write<hstring>(server->Hashes.to_hashed_string("UnitTestWorldStep"));
            call.Write<int32_t>(8);
            call.Write<int32_t>(7);
            call.Write<int32_t>(7);
            break;
        }

        call.EndMsg();
        client.Send(call.GetData());
    }};

    RawChannelClient live_client {port};
    RawChannelClient faulty_client {port};
    REQUIRE(WaitForServerConnectionCount(server, 2));

    SendCompatibleHandshake(live_client, server);
    SendCompatibleHandshake(faulty_client, server);
    REQUIRE(WaitForServerMessageCount(live_client, NetMessage::InitData, 1));
    REQUIRE(WaitForServerMessageCount(faulty_client, NetMessage::InitData, 1));

    send_remote_call(faulty_client, forgery);

    REQUIRE(WaitForServerConnectionCount(server, 1));
    CHECK(faulty_client.IsDisconnected());
    CHECK(exception_reports.load() == 0);

    // The independent connection keeps being served after the forged call is refused
    CHECK_FALSE(live_client.IsDisconnected());
    send_ping(live_client);
    CHECK(WaitForServerMessageCount(live_client, NetMessage::TimeSync, 1));
    CHECK(GetServerConnectionCount(server) == 1);
    CHECK(exception_reports.load() == 0);
}

TEST_CASE("ServerRejectsForgedGameplayMessagesWithoutExceptionReport")
{
    using namespace TestClientServerIntegration;

    // Well-framed messages a hostile client sends: out-of-contract values for each handler class, and messages the
    // session may not send at all. Each one is refused for that connection alone
    enum class Forgery
    {
        UnexpectedMessagePreLogin,
        UnexpectedMessage,
        PingStamp,
        DirAngle,
        StopMoveAngle,
        MoveFinishedAngle,
        MoveStamp,
        MoveLeaseNotFinite,
        MoveLeaseOutOfRange,
        MoveControlStepsBeyondPlan,
        MovePlanLeavesMap,
        MoveLeaseDistanceNotFinite,
        MoveLeaseDistanceZero,
        MoveLeaseStamp,
        PropertyUnknownIndex,
        PropertyUnknownType,
        PropertyNotSynced,
        PropertyPublicNotForAnyClient,
        PropertyTooLong,
    };

    struct LoginIds
    {
        ident_t MapId {};
        ident_t CritterId {};
    };

    mutex logins_locker {};
    vector<LoginIds> logins {};

    auto port {IntegrationTestPort.fetch_add(1)};
    auto server_settings {MakeServerTestSettings(port)};
    BakerTests::OverrideSetting(server_settings.Network.DisableZlibCompression, true);
    auto server {MakeServerEngine(server_settings)};

    auto shutdown {scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    })};

    string startup_error {WaitForServerStart(server)};
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(HasInterthreadListener(port));

    {
        REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
        auto unlock {scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); })};

        // The same login for every backend: a session with a controlled critter on its own map
        server->SetRemoteCallHandler(
            server->Hashes.to_hashed_string("UnitTestLogin"),
            [&](hstring, nptr<Entity> caller, span<uint8_t>, shared_ptr<ScriptCallCompletion>) {
                ptr<Player> player {caller.dyn_cast<Player>()};
                auto logged {server->LoginPlayerToTempSession(player)};
                auto cr {server->CreateCritter(server->Hashes.to_hashed_string("UnitTestSharedCritter"), true)};
                auto loc {server->MapMngr.CreateLocation(server->Hashes.to_hashed_string("UnitTestSharedLocation"), vector<hstring> {server->Hashes.to_hashed_string("UnitTestSharedMap")})};
                auto map {loc->GetMapByIndex(0)};
                server->MapMngr.TransferToMap(cr, map, mpos {20, 20}, mdir {}, std::nullopt);
                server->SwitchPlayerCritter(logged, cr);

                scoped_lock locker {logins_locker};
                logins.emplace_back(LoginIds {.MapId = map->GetId(), .CritterId = cr->GetId()});
            },
            true);
    }

    auto previous_exception_callback {exceptions::get_callback()};
    std::atomic_int exception_reports {};
    exceptions::set_callback([&exception_reports](string_view, const stack_trace::catched_data&, bool) { exception_reports.fetch_add(1); });
    auto restore_exception_callback {scope_exit([previous = std::move(previous_exception_callback)]() mutable noexcept { exceptions::set_callback(std::move(previous)); })};

    auto send_ping {[](RawChannelClient& client, int64_t stamp) {
        NetOutBuffer ping {64};
        ping.StartMsg(NetMessage::Ping);
        ping.Write(false);
        ping.Write(stamp);
        ping.EndMsg();
        client.Send(ping.GetData());
    }};

    auto register_session {[&](RawChannelClient& client, bool login) -> LoginIds {
        SendCompatibleHandshake(client, server);
        REQUIRE(WaitForServerMessageCount(client, NetMessage::InitData, 1));

        if (!login) {
            return {};
        }

        size_t known_logins {};

        {
            scoped_lock locker {logins_locker};
            known_logins = logins.size();
        }

        NetOutBuffer call {32};
        call.StartMsg(NetMessage::RemoteCall);
        call.Write<hstring>(server->Hashes.to_hashed_string("UnitTestLogin"));
        call.Write<int32_t>(0);
        call.EndMsg();
        client.Send(call.GetData());
        REQUIRE(WaitForServerMessageCount(client, NetMessage::LoginSuccess, 1));

        // The login message is sent before the handler has created the critter and the map, so the session is used
        // only after the handler has recorded them
        for (int32_t i {0}; i < 2000; i++) {
            {
                scoped_lock locker {logins_locker};

                if (logins.size() == known_logins + 1) {
                    return logins.back();
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds {2});
        }

        FAIL("The login handler did not finish");
        return {};
    }};

    auto find_property_index {[&](string_view type_name, string_view prop_name) -> uint16_t {
        auto registrar {server->GetPropertyRegistrar(type_name)};
        REQUIRE(registrar);
        auto prop {registrar->FindProperty(prop_name)};
        REQUIRE(prop);
        return prop->GetRegIndex();
    }};

    uint16_t critter_mark_index {find_property_index("Critter", "UnitTestClientMark")};
    uint16_t login_mark_index {find_property_index("Player", "UnitTestLoginMark")};

    auto send_property {[](RawChannelClient& client, uint32_t declared_size, NetProperty type, ident_t critter_id, uint16_t index, bool with_data) {
        NetOutBuffer prop {64};
        prop.StartMsg(NetMessage::SendProperty);
        prop.Write(declared_size);
        prop.Write(type);

        if (type == NetProperty::Critter) {
            prop.Write(critter_id);
        }

        prop.Write(index);

        if (with_data) {
            prop.Write<int32_t>(5);
        }

        prop.EndMsg();
        client.Send(prop.GetData());
    }};

    // The same message with every value inside its contract, or with the forged one, so that a control proves the
    // session is in the state the handler needs and the rejection is the forged value alone
    auto send_message = [&](RawChannelClient& client, Forgery kind, bool forged, const LoginIds& ids) {
        int64_t stamp {forged ? std::numeric_limits<int64_t>::max() : nanotime::now().milliseconds()};
        int64_t now_ms {nanotime::now().milliseconds()};
        mdir valid_dir {90};

        auto send_move {[&](mpos start_hex, const vector<hdir>& steps, const vector<uint16_t>& control_steps, float32_t lease_distance, int64_t move_stamp) {
            NetOutBuffer move {256};
            move.StartMsg(NetMessage::SendCritterMove);
            move.Write(ids.MapId);
            move.Write(ids.CritterId);
            move.Write<uint16_t>(100);
            move.Write(start_hex);
            move.Write(numeric_cast<uint16_t>(steps.size()));

            for (hdir step : steps) {
                move.Write(step);
            }

            move.Write(numeric_cast<uint16_t>(control_steps.size()));

            for (uint16_t control_step : control_steps) {
                move.Write(control_step);
            }

            move.Write(ipos16 {});
            move.Write<uint32_t>(1);
            move.Write(lease_distance);
            move.Write(move_stamp);
            move.EndMsg();
            client.Send(move.GetData());
        }};
        auto send_lease {[&](float32_t lease_distance, int64_t lease_stamp) {
            NetOutBuffer lease {64};
            lease.StartMsg(NetMessage::SendCritterMoveLease);
            lease.Write(ids.MapId);
            lease.Write(ids.CritterId);
            lease.Write<uint32_t>(1);
            lease.Write(lease_distance);
            lease.Write(lease_stamp);
            lease.EndMsg();
            client.Send(lease.GetData());
        }};
        auto send_dir_message {[&](NetMessage msg, optional<int16_t> raw_angle) {
            NetOutBuffer dir {64};
            dir.StartMsg(msg);
            dir.Write(ids.MapId);
            dir.Write(ids.CritterId);

            if (msg == NetMessage::SendStopCritterMove) {
                dir.Write(mpos {20, 20});
                dir.Write(ipos16 {});
            }
            else if (msg == NetMessage::SendCritterMoveFinished) {
                dir.Write(mpos {21, 20});
                dir.Write(mpos {20, 20});
                dir.Write(ipos16 {});
            }

            if (raw_angle) {
                dir.Write<int16_t>(raw_angle.value());
            }
            else {
                dir.Write(valid_dir);
            }

            if (msg == NetMessage::SendStopCritterMove) {
                dir.Write(now_ms);
            }

            dir.EndMsg();
            client.Send(dir.GetData());
        }};

        switch (kind) {
        case Forgery::UnexpectedMessagePreLogin:
            send_dir_message(NetMessage::SendCritterDir, {});
            break;
        case Forgery::UnexpectedMessage: {
            NetOutBuffer time_sync {64};
            time_sync.StartMsg(NetMessage::TimeSync);
            time_sync.Write<int64_t>(0);
            time_sync.EndMsg();
            client.Send(time_sync.GetData());
        } break;
        case Forgery::PingStamp:
            send_ping(client, stamp);
            break;
        case Forgery::DirAngle:
            send_dir_message(NetMessage::SendCritterDir, forged ? optional<int16_t> {1000} : optional<int16_t> {});
            break;
        case Forgery::StopMoveAngle:
            send_dir_message(NetMessage::SendStopCritterMove, forged ? optional<int16_t> {-5} : optional<int16_t> {});
            break;
        case Forgery::MoveFinishedAngle:
            send_dir_message(NetMessage::SendCritterMoveFinished, forged ? optional<int16_t> {360} : optional<int16_t> {});
            break;
        case Forgery::MoveStamp:
            send_move(mpos {20, 20}, {hdir {0}}, {1}, 0.0f, stamp);
            break;
        case Forgery::MoveLeaseNotFinite:
            send_move(mpos {20, 20}, {hdir {0}}, {1}, forged ? std::numeric_limits<float32_t>::quiet_NaN() : 0.0f, now_ms);
            break;
        case Forgery::MoveLeaseOutOfRange:
            send_move(mpos {20, 20}, {hdir {0}}, {1}, forged ? 1.0e30f : 0.0f, now_ms);
            break;
        case Forgery::MoveControlStepsBeyondPlan:
            send_move(mpos {20, 20}, {hdir {0}, hdir {0}}, forged ? vector<uint16_t> {7, 2} : vector<uint16_t> {2}, 0.0f, now_ms);
            break;
        case Forgery::MovePlanLeavesMap:
            send_move(forged ? mpos {0, 0} : mpos {20, 20}, vector<hdir>(8, hdir {0}), {8}, 5.0f, now_ms);
            break;
        case Forgery::MoveLeaseDistanceNotFinite:
            send_lease(forged ? std::numeric_limits<float32_t>::infinity() : 5.0f, now_ms);
            break;
        case Forgery::MoveLeaseDistanceZero:
            send_lease(forged ? 0.0f : 5.0f, now_ms);
            break;
        case Forgery::MoveLeaseStamp:
            send_lease(5.0f, stamp);
            break;
        case Forgery::PropertyUnknownIndex:
            send_property(client, sizeof(int32_t), NetProperty::Chosen, ids.CritterId, forged ? uint16_t {60000} : critter_mark_index, true);
            break;
        case Forgery::PropertyUnknownType:
            send_property(client, sizeof(int32_t), forged ? static_cast<NetProperty>(99) : NetProperty::Chosen, ids.CritterId, critter_mark_index, true);
            break;
        case Forgery::PropertyNotSynced:
            send_property(client, sizeof(int32_t), forged ? NetProperty::Player : NetProperty::Chosen, ids.CritterId, forged ? login_mark_index : critter_mark_index, true);
            break;
        case Forgery::PropertyPublicNotForAnyClient:
            send_property(client, sizeof(int32_t), forged ? NetProperty::Critter : NetProperty::Chosen, ids.CritterId, critter_mark_index, true);
            break;
        case Forgery::PropertyTooLong:
            send_property(client, forged ? numeric_cast<uint32_t>(server->Settings->ServerNetwork.MaxPropertyDataSize) + 1 : sizeof(int32_t), NetProperty::Chosen, ids.CritterId, critter_mark_index, !forged);
            break;
        }
    };

    RawChannelClient live_client {port};
    REQUIRE(WaitForServerConnectionCount(server, 1));
    register_session(live_client, true);
    size_t live_answers {0};

    for (Forgery kind : {Forgery::UnexpectedMessagePreLogin, Forgery::UnexpectedMessage, Forgery::PingStamp, Forgery::DirAngle, Forgery::StopMoveAngle, Forgery::MoveFinishedAngle, Forgery::MoveStamp, Forgery::MoveLeaseNotFinite, Forgery::MoveLeaseOutOfRange, Forgery::MoveControlStepsBeyondPlan, Forgery::MovePlanLeavesMap, Forgery::MoveLeaseDistanceNotFinite, Forgery::MoveLeaseDistanceZero, Forgery::MoveLeaseStamp, Forgery::PropertyUnknownIndex, Forgery::PropertyUnknownType, Forgery::PropertyNotSynced, Forgery::PropertyPublicNotForAnyClient, Forgery::PropertyTooLong}) {
        CAPTURE(static_cast<int32_t>(kind));

        bool pre_login {kind == Forgery::UnexpectedMessagePreLogin};
        bool has_control {kind != Forgery::UnexpectedMessagePreLogin && kind != Forgery::UnexpectedMessage};

        RawChannelClient faulty_client {port};
        REQUIRE(WaitForServerConnectionCount(server, 2));
        LoginIds ids {register_session(faulty_client, !pre_login)};

        if (has_control) {
            // The message with every value inside its contract keeps the session, so the rejection below is the value
            send_message(faulty_client, kind, false, ids);
            send_ping(faulty_client, nanotime::now().milliseconds());
            REQUIRE(WaitForServerMessageCount(faulty_client, NetMessage::TimeSync, 1));
            REQUIRE_FALSE(faulty_client.IsDisconnected());
        }

        send_message(faulty_client, kind, true, ids);

        REQUIRE(WaitForServerConnectionCount(server, 1));
        CHECK(faulty_client.IsDisconnected());
        CHECK(exception_reports.load() == 0);

        // The independent session keeps being served after the forged message is refused
        CHECK_FALSE(live_client.IsDisconnected());
        send_ping(live_client, nanotime::now().milliseconds());
        live_answers++;
        CHECK(WaitForServerMessageCount(live_client, NetMessage::TimeSync, live_answers));
        CHECK(GetServerConnectionCount(server) == 1);
        CHECK(exception_reports.load() == 0);
    }
}

TEST_CASE("ServerDisconnectsPreLoginConnectionAfterLoginTimeout")
{
    using namespace TestClientServerIntegration;

    bool simulation_paused {GENERATE(false, true)};
    bool inactivity_timeout {GENERATE(false, true)};
    CAPTURE(simulation_paused, inactivity_timeout);
    uint16_t port {IntegrationTestPort.fetch_add(1)};
    auto server_settings {MakeServerTestSettings(port)};
    BakerTests::OverrideSetting(server_settings.ServerNetwork.InactivityDisconnectTime, inactivity_timeout ? 100 : 0);
    BakerTests::OverrideSetting(server_settings.ServerNetwork.LoginTimeout, inactivity_timeout ? 0 : 100);
    auto server {TestClientServerIntegration::MakeServerEngine(server_settings)};

    auto shutdown {scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    })};

    string startup_error {TestClientServerIntegration::WaitForServerStart(server)};
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(HasInterthreadListener(port));

    if (simulation_paused) {
        server->PauseSimulation();
    }
    synctime simulation_time {server->GameTime.GetSynchronizedTime()};
    RawChannelClient client {port};
    REQUIRE(WaitForServerConnectionCount(server, 1));

    for (int32_t i {0}; i < 2000 && !client.IsDisconnected(); i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds {2});
    }

    CHECK(client.IsDisconnected());
    CHECK(WaitForServerConnectionCount(server, 0));
    if (simulation_paused) {
        CHECK(server->GameTime.GetSynchronizedTime() == simulation_time);
    }
}

TEST_CASE("ServerReportsMetadataMismatchInHandshake")
{
    using namespace TestClientServerIntegration;

    uint16_t port {IntegrationTestPort.fetch_add(1)};
    auto server_settings {MakeServerTestSettings(port)};
    BakerTests::OverrideSetting(server_settings.Network.DisableZlibCompression, true);
    auto server {TestClientServerIntegration::MakeServerEngine(server_settings)};

    auto shutdown {scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    })};

    string startup_error {TestClientServerIntegration::WaitForServerStart(server)};
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(HasInterthreadListener(port));

    RawChannelClient client {port};
    REQUIRE(WaitForServerConnectionCount(server, 1));

    // A binary-compatible client whose resources come from another bake: the layout verdict is what keeps its
    // property payloads from reaching deserialization
    REQUIRE_FALSE(server->GetMetadataVersion().empty());
    auto handshake {NetOutBuffer(128)};
    handshake.StartMsg(NetMessage::Handshake);
    handshake.Write(server_settings.Network.CompatibilityVersion);
    handshake.Write<string_view>("0123456789abcdef");
    handshake.Write<uint32_t>(FO_UPDATER_VERSION);
    handshake.Write<string_view>("Linux-x64");
    handshake.EndMsg();
    client.Send(handshake.GetData());

    bool received_answer {false};

    for (int32_t i {0}; i < 2000 && !received_answer; i++) {
        vector<uint8_t> response_data {client.GetPlaintext()};

        if (!response_data.empty()) {
            NetInBuffer response {response_data.size()};
            response.AddData(response_data);

            if (response.NeedProcess()) {
                REQUIRE(response.ReadMsg() == NetMessage::HandshakeAnswer);
                CHECK_FALSE(response.Read<bool>());
                CHECK_FALSE(response.Read<bool>());
                CHECK(response.Read<bool>());
                CHECK(response.Read<string>() == server->GetMetadataVersion());
                received_answer = true;
            }
        }

        if (!received_answer) {
            std::this_thread::sleep_for(std::chrono::milliseconds {2});
        }
    }

    REQUIRE(received_answer);
    client.Close();
    CHECK(WaitForServerConnectionCount(server, 0));
}

TEST_CASE("ServerRejectsUnsafeUpdaterGenerationBeforeInitData")
{
    using namespace TestClientServerIntegration;

    uint16_t port {IntegrationTestPort.fetch_add(1)};
    auto server_settings {MakeServerTestSettings(port)};
    BakerTests::OverrideSetting(server_settings.Network.DisableZlibCompression, true);
    auto server {TestClientServerIntegration::MakeServerEngine(server_settings)};

    auto shutdown {scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    })};

    string startup_error {TestClientServerIntegration::WaitForServerStart(server)};
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(HasInterthreadListener(port));

    RawChannelClient client {port};
    REQUIRE(WaitForServerConnectionCount(server, 1));

    static_assert(FO_UPDATER_VERSION > 1);
    auto handshake {NetOutBuffer(128)};
    handshake.StartMsg(NetMessage::Handshake);
    handshake.Write(server_settings.Network.CompatibilityVersion);
    handshake.Write(server->GetMetadataVersion());
    handshake.Write<uint32_t>(FO_UPDATER_VERSION - 1);
    handshake.Write<string_view>("Linux-x64");
    handshake.EndMsg();
    client.Send(handshake.GetData());

    bool received_rejection {false};

    for (int32_t i {0}; i < 2000 && !received_rejection; i++) {
        vector<uint8_t> response_data {client.GetPlaintext()};

        if (!response_data.empty()) {
            NetInBuffer response {response_data.size()};
            response.AddData(response_data);

            if (response.NeedProcess()) {
                REQUIRE(response.ReadMsg() == NetMessage::HandshakeAnswer);
                CHECK_FALSE(response.Read<bool>());
                CHECK(response.Read<bool>());
                CHECK_FALSE(response.Read<bool>());
                CHECK(response.Read<string>() == server->GetMetadataVersion());

                if (response.NeedProcess()) {
                    REQUIRE(response.ReadMsg() == NetMessage::Disconnect);
                    response.ShrinkReadBuf();
                    CHECK(response.GetDataSize() == 0);
                    received_rejection = true;
                }
            }
        }

        if (!received_rejection) {
            std::this_thread::sleep_for(std::chrono::milliseconds {2});
        }
    }

    REQUIRE(received_rejection);
    client.Close();
    REQUIRE(WaitForServerConnectionCount(server, 0));
}

TEST_CASE("ClientShutdownDisconnectsActiveConnection")
{
    using namespace TestClientServerIntegration;

    auto port {IntegrationTestPort.fetch_add(1)};

    auto server_settings {MakeServerTestSettings(port)};
    auto client_settings {MakeClientTestSettings(port)};

    auto server {TestClientServerIntegration::MakeServerEngine(server_settings)};
    auto client {TestClientServerIntegration::MakeClientEngine(client_settings)};
    bool client_shutdown {false};
    int32_t shutdown_disconnected_calls {0};

    auto shutdown {scope_exit([&server, &client, &client_shutdown]() noexcept {
        safe_call([&client, &client_shutdown] {
            if (!client_shutdown) {
                client->Shutdown();
            }
        });

        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    })};

    string startup_error {TestClientServerIntegration::WaitForServerStart(server)};
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    client->Connect();
    REQUIRE(WaitForConnected(client, server));

    Entity::EventCallbackData shutdown_disconnect_observer {};
    shutdown_disconnect_observer.Callback = [&shutdown_disconnected_calls](FuncCallData&) {
        shutdown_disconnected_calls++;
        return Entity::EventResult::ContinueChain;
    };

    shutdown_disconnect_observer.SubscriptionPtr = reinterpret_cast<uintptr_t>(&shutdown_disconnected_calls);
    client->OnDisconnected.Subscribe(std::move(shutdown_disconnect_observer));

    client->Shutdown();
    client_shutdown = true;

    CHECK_FALSE(client->IsConnecting());
    CHECK_FALSE(client->IsConnected());
    CHECK_FALSE(static_cast<bool>(client->GetCurPlayer()));
    CHECK(shutdown_disconnected_calls == 1);
    REQUIRE(WaitForServerConnectionCount(server, 0));
}

TEST_CASE("ClientEndsAnEstablishedUdpSessionInsteadOfReconnectingOverTcp")
{
    using namespace TestClientServerIntegration;

    uint16_t port {IntegrationTestPort.fetch_add(1)};

    // The server listens on both transports, so a wrong fallback would find a TCP listener waiting
    auto server_settings {MakeServerTestSettings(port)};
    BakerTests::OverrideSetting(server_settings.Network.DisableInterthreadCommunication, true);
    BakerTests::OverrideSetting(server_settings.ServerNetwork.DisableNetworking, false);
    BakerTests::OverrideSetting(server_settings.ServerNetwork.EnableUdp, true);

    // The largest payload the packet header can carry does not fit one IP datagram together with that header: small messages
    // go through, and the first packet of a big one fails to send, which is how a UDP send error looks to the client
    auto client_settings {MakeClientTestSettings(port)};
    BakerTests::OverrideSetting(client_settings.ClientNetwork.ServerHost, string {"127.0.0.1"});
    BakerTests::OverrideSetting(client_settings.ClientNetwork.ProxyType, 0);
    BakerTests::OverrideSetting(client_settings.ClientNetwork.UseUdp, true);
    BakerTests::OverrideSetting(client_settings.Network.UdpPacketSize, 65535);

    std::mutex log_locker {};
    vector<string> log_lines {};
    logging::set_callback("udp-session-end", [&](logging::type, string_view message, nptr<const stack_trace::catched_data>) {
        std::scoped_lock locker {log_locker};
        log_lines.emplace_back(message);
    });
    auto restore_logger {scope_exit([]() noexcept { logging::set_callback("udp-session-end", {}); })};

    auto server {TestClientServerIntegration::MakeServerEngine(server_settings)};
    auto client {TestClientServerIntegration::MakeClientEngine(client_settings)};
    int32_t connected_calls {0};
    int32_t disconnected_calls {0};

    auto shutdown {scope_exit([&server, &client]() noexcept {
        safe_call([&client] { client->Shutdown(); });

        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    })};

    string startup_error {TestClientServerIntegration::WaitForServerStart(server)};
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    Entity::EventCallbackData connect_observer {};
    connect_observer.Callback = [&connected_calls](FuncCallData&) {
        connected_calls++;
        return Entity::EventResult::ContinueChain;
    };
    connect_observer.SubscriptionPtr = reinterpret_cast<uintptr_t>(&connected_calls);
    client->OnConnected.Subscribe(std::move(connect_observer));

    Entity::EventCallbackData disconnect_observer {};
    disconnect_observer.Callback = [&disconnected_calls](FuncCallData&) {
        disconnected_calls++;
        return Entity::EventResult::ContinueChain;
    };
    disconnect_observer.SubscriptionPtr = reinterpret_cast<uintptr_t>(&disconnected_calls);
    client->OnDisconnected.Subscribe(std::move(disconnect_observer));

    client->Connect();
    REQUIRE(WaitForConnected(client, server));
    CHECK(connected_calls == 1);

    {
        std::scoped_lock locker {log_locker};
        CHECK(std::ranges::count_if(log_lines, [](const string& line) { return line.find("Connected to server") != string::npos && line.find("over UDP") != string::npos; }) == 1);
    }

    // The message is never delivered: its first packet already fails to send
    client->GetConnection()->OutBuf->StartMsg(NetMessage::Ping);
    client->GetConnection()->OutBuf->Push(vector<uint8_t>(80000, uint8_t {0}));
    client->GetConnection()->OutBuf->EndMsg();

    // A session that the transport lost is over: the server would see a second one on a new connection, and
    // everything the client built from the first would then be stale
    for (int32_t i {0}; i < 2000 && disconnected_calls == 0; i++) {
        REQUIRE_NOTHROW(client->MainLoop());
        std::this_thread::sleep_for(std::chrono::milliseconds {2});
    }

    CHECK(WaitForDisconnected(client, server));
    CHECK(connected_calls == 1);
    CHECK(disconnected_calls == 1);

    std::scoped_lock locker {log_locker};
    CHECK(std::ranges::count_if(log_lines, [](const string& line) { return line.find("fallback to TCP") != string::npos; }) == 0);
}

TEST_CASE("ClientUpdaterKeepsConfirmedBytesWhenTheUdpServerDropsAndResumesOnTheNextRun")
{
    using namespace TestClientServerIntegration;

    string install {PrepareClientUpdaterBakeOutput()};
    string published_root {MakeTempClientUpdaterBakeDir("published")};
    string published {strex(published_root).combine_path("Client").str()};
    auto cleanup_published_root {scope_exit([&]() noexcept { (void)fs::remove_dir_tree(published_root); })};
    string writable {MakeTempClientUpdaterBakeDir("writable")};
    auto cleanup {scope_exit([&]() noexcept {
        (void)fs::remove_dir_tree(install);
        (void)fs::remove_dir_tree(published);
        (void)fs::remove_dir_tree(writable);
    })};
    REQUIRE(fs::create_directories(published));
    REQUIRE(fs::create_directories(writable));
    vector<uint8_t> metadata {BakerTests::MakeMetadataBlob({})};

    // This transport fixture has no packaged config, so its metadata uses a custom pack
    for (const string& directory : {install, published}) {
        ResourcePackWriter writer {strex(directory).combine_path("TestMetadata.fores").str()};
        writer.AddFile("Metadata.fometa-client", metadata);
        writer.Finish();
    }

    // Incompressible and served in small portions, so the transfer is still running when the server goes away
    vector<uint8_t> payload {vector<uint8_t>(512 * 1024)};
    random_generator generator {7};

    for (uint8_t& value : payload) {
        value = numeric_cast<uint8_t>(generator.next_between(0, 255));
    }

    string published_pack {strex(published).combine_path("Art.fores").str()};

    {
        ResourcePackWriter writer {published_pack, {0, 100}};
        writer.AddFile("Payload.bin", payload);
        writer.Finish();
    }

    auto published_bytes {fs::read_file(published_pack)};
    REQUIRE(published_bytes);

    string writable_resources {strex(writable).combine_path("Resources").str()};
    string temp_pack {strex(writable_resources).combine_path("~Art.fores").str()};
    string final_pack {strex(writable_resources).combine_path("Art.fores").str()};

    std::mutex log_locker {};
    vector<string> log_lines {};
    logging::set_callback("updater-resume", [&](logging::type, string_view message, nptr<const stack_trace::catched_data>) {
        std::scoped_lock locker {log_locker};
        log_lines.emplace_back(message);
    });
    auto restore_logger {scope_exit([]() noexcept { logging::set_callback("updater-resume", {}); })};

    auto count_log_lines {[&](string_view part) {
        std::scoped_lock locker {log_locker};
        return std::ranges::count_if(log_lines, [part](const string& line) { return line.find(part) != string::npos; });
    }};

    // Interrupt only after a prefix reaches disk, so slow UDP startup cannot turn a resume check into an empty download
    auto synchronize {[&](bool interrupt_transfer) -> UpdaterResult {
        uint16_t port {IntegrationTestPort.fetch_add(1)};
        GlobalSettings server_settings {MakeServerTestSettings(port)};
        BakerTests::OverrideSetting(server_settings.Common.Packaged, true);
        BakerTests::OverrideSetting(server_settings.Common.ResourcesPath, published_root);
        auto server_pack_config {ConfigFile("[ResourcePack]\nName = TestMetadata\nClientOnly = True\n[ResourcePack]\nName = Art\nClientOnly = True\n")};
        server_settings.ApplyConfigFile(server_pack_config, "");
        BakerTests::OverrideSetting(server_settings.Common.PlatformBinariesPath, strex(published).combine_path("NoBinaries").str());
        BakerTests::OverrideSetting(server_settings.Network.DisableInterthreadCommunication, true);
        BakerTests::OverrideSetting(server_settings.ServerNetwork.DisableNetworking, false);
        BakerTests::OverrideSetting(server_settings.ServerNetwork.EnableUdp, true);
        BakerTests::OverrideSetting(server_settings.Network.UpdateFileMaxPortionSize, 8192);
        auto server {TestClientServerIntegration::MakeServerEngine(server_settings)};
        auto shutdown {scope_exit([&]() noexcept {
            safe_call([&] {
                if (server->IsStarted()) {
                    server->Shutdown();
                }
            });
        })};
        string error {TestClientServerIntegration::WaitForServerStart(server)};
        INFO(error);
        REQUIRE(error.empty());

        GlobalSettings client_settings {MakeClientTestSettings(port)};
        BakerTests::OverrideSetting(client_settings.Common.Packaged, true);
        BakerTests::OverrideSetting(client_settings.Common.ResourcesPath, install);
        BakerTests::OverrideSetting(client_settings.ClientNetwork.ServerHost, string {"127.0.0.1"});
        BakerTests::OverrideSetting(client_settings.ClientNetwork.ProxyType, 0);
        BakerTests::OverrideSetting(client_settings.ClientNetwork.UseUdp, true);

        // A lost UDP disconnect packet must time out inside this fixture's processing window
        BakerTests::OverrideSetting(client_settings.ClientNetwork.PingTimeout, 1000);

        auto client_pack_config {ConfigFile("[ResourcePack]\nName = Embedded\nClientOnly = True\n[ResourcePack]\nName = TestMetadata\nClientOnly = True\n[ResourcePack]\nName = Art\nClientOnly = True\n")};
        client_settings.ApplyConfigFile(client_pack_config, "");
        client_settings.ApplyWritableRoot(writable);
        FileSystem metadata_source {};
        AddClientPackSource(metadata_source, client_settings, METADATA_PACK_NAME, true);
        REQUIRE_FALSE(ReadResourceConfig(metadata_source, false));

        Updater updater {&client_settings, &GetApp()->MainWindow};
        bool server_stopped {false};
        bool finished {false};

        for (int32_t i {0}; i < 6000 && !finished; i++) {
            finished = updater.Process();

            if (!finished && interrupt_transfer && !server_stopped) {
                auto confirmed_size {fs::file_size(temp_pack)};

                if (confirmed_size && *confirmed_size > 0 && *confirmed_size < numeric_cast<uint64_t>(published_bytes->size())) {
                    server->Shutdown();
                    server_stopped = true;
                }
            }

            if (!finished) {
                std::this_thread::sleep_for(std::chrono::milliseconds {2});
            }
        }

        REQUIRE(finished);
        CHECK(server_stopped == interrupt_transfer);
        return updater.GetResult();
    }};

    // The server goes away in the middle of the pack: the player sees a lost connection, not a broken client
    CHECK(synchronize(true) == UpdaterResult::ConnectionFailed);
    CHECK(count_log_lines("Update file list was already received") == 0);
    CHECK_FALSE(fs::exists(final_pack));

    // What the run confirmed stays on disk and is the true prefix of the published pack
    auto kept {fs::read_file(temp_pack)};
    REQUIRE(kept);
    REQUIRE_FALSE(kept->empty());
    REQUIRE(kept->size() < published_bytes->size());
    CHECK(published_bytes->starts_with(*kept));

    // An ordinary new run continues from those bytes instead of starting over
    CHECK(synchronize(false) == UpdaterResult::ResourcesReady);
    CHECK(count_log_lines("Client updater: resuming temp file") == 1);
    CHECK(fs::read_file(final_pack) == published_bytes);
    CHECK_FALSE(fs::exists(temp_pack));
}

TEST_CASE("ClientAndServerInterthreadConnectionKeepsProcessingAfterHandshake")
{
    using namespace TestClientServerIntegration;

    auto port {IntegrationTestPort.fetch_add(1)};

    auto server_settings {MakeServerTestSettings(port)};
    auto client_settings {MakeClientTestSettings(port)};

    auto server {TestClientServerIntegration::MakeServerEngine(server_settings)};
    auto client {TestClientServerIntegration::MakeClientEngine(client_settings)};

    auto shutdown {scope_exit([&server, &client]() noexcept {
        safe_call([&client] {
            client->Disconnect();
            client->Shutdown();
        });

        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    })};

    string startup_error {TestClientServerIntegration::WaitForServerStart(server)};
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    client->Connect();
    REQUIRE(WaitForConnected(client, server));

    size_t bytes_received_after_handshake {client->GetConnection()->GetBytesReceived()};

    bool received_post_handshake_packet {false};

    for (int32_t i {0}; i < 1000; i++) {
        client->MainLoop();

        if (client->GetConnection()->GetBytesReceived() > bytes_received_after_handshake) {
            received_post_handshake_packet = true;
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds {2});
    }

    CHECK(received_post_handshake_packet);
}

TEST_CASE("ClientReportsUnresolvedHashAndLearnsWithoutDisconnect")
{
    using namespace TestClientServerIntegration;

    auto port {IntegrationTestPort.fetch_add(1)};

    auto server_settings {MakeServerTestSettings(port)};
    auto client_settings {MakeClientTestSettings(port)};

    auto server {TestClientServerIntegration::MakeServerEngine(server_settings)};
    auto client {TestClientServerIntegration::MakeClientEngine(client_settings)};

    auto shutdown {scope_exit([&server, &client]() noexcept {
        safe_call([&client] { client->Shutdown(); });

        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    })};

    string startup_error {TestClientServerIntegration::WaitForServerStart(server)};
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    client->Connect();
    REQUIRE(WaitForConnected(client, server));

    // A string the server knows but the client doesn't РІР‚вЂќ mimics a runtime hstring the client can't resolve
    hstring reported {server->Hashes.to_hashed_string("integration_test_only_hash")};

    // Send the exact wire message ClientEngine emits when it hits an unresolved hash
    client->GetConnection()->OutBuf->StartMsg(NetMessage::UnresolvedHash);
    client->GetConnection()->OutBuf->Write<hstring::hash_t>(reported.as_hash());
    client->GetConnection()->OutBuf->EndMsg();

    REQUIRE(WaitForLearnedHash(client, reported.as_hash(), "integration_test_only_hash"));
    CHECK(client->IsConnected());
    CHECK(GetServerConnectionCount(server) == 1);

    auto second_client {TestClientServerIntegration::MakeClientEngine(client_settings)};
    auto shutdown_second_client {scope_exit([&second_client]() noexcept { safe_call([&second_client] { second_client->Shutdown(); }); })};

    second_client->Connect();
    REQUIRE(WaitForConnected(second_client, server, 2));
    REQUIRE(WaitForLearnedHash(second_client, reported.as_hash(), "integration_test_only_hash"));
    CHECK(GetServerConnectionCount(server) == 2);
}

TEST_CASE("ClientUpdaterResourcePatchLifecycle")
{
    using namespace TestClientServerIntegration;

    string install {PrepareClientUpdaterBakeOutput()};
    string published_root {MakeTempClientUpdaterBakeDir("published")};
    string published {strex(published_root).combine_path("Client").str()};
    auto cleanup_published_root {scope_exit([&]() noexcept { (void)fs::remove_dir_tree(published_root); })};
    string writable {MakeTempClientUpdaterBakeDir("writable")};
    auto cleanup {scope_exit([&]() noexcept {
        (void)fs::remove_dir_tree(install);
        (void)fs::remove_dir_tree(published);
        (void)fs::remove_dir_tree(writable);
    })};

    REQUIRE(fs::create_directories(published));
    REQUIRE(fs::create_directories(writable));
    vector<uint8_t> metadata {BakerTests::MakeMetadataBlob({})};

    // This patch-only fixture has no packaged native bootstrap or resource configuration.
    // Keep its metadata in a custom pack; the reserved Metadata pack also owns the startup config
    for (const string& directory : {install, published}) {
        ResourcePackWriter writer {strex(directory).combine_path("TestMetadata.fores").str()};
        writer.AddFile("Metadata.fometa-client", metadata);
        writer.Finish();
    }

    auto write_art {[](string_view directory, const vector<pair<string, string>>& files) {
        ResourcePackWriter writer {strex(directory).combine_path("Art.fores").str(), {0, 100}};

        for (const auto& [name, content] : files) {
            writer.AddFile(name, {reinterpret_cast<const uint8_t*>(content.data()), content.size()});
        }

        writer.Finish();
    }};

    string unchanged {string(65536, 'b')};
    write_art(install, {{"Keep.bin", unchanged}, {"Change.txt", "old"}, {"Removed.txt", "removed"}});
    write_art(published, {{"Keep.bin", unchanged}, {"Change.txt", "first update"}});
    string installed_base {strex(install).combine_path("Art.fores").str()};
    auto original {fs::read_file(installed_base)};
    REQUIRE(original);
    string writable_resources {strex(writable).combine_path("Resources").str()};
    string patch {strex(writable_resources).combine_path("Art.patch.fores").str()};
    string replacement {strex(writable_resources).combine_path("Art.fores").str()};

    auto synchronize {[&](bool in_memory, bool replace_published = false) {
        uint16_t port {IntegrationTestPort.fetch_add(1)};
        GlobalSettings server_settings {MakeServerTestSettings(port)};
        BakerTests::OverrideSetting(server_settings.Common.Packaged, true);
        BakerTests::OverrideSetting(server_settings.Common.ResourcesPath, published_root);
        auto server_pack_config {ConfigFile("[ResourcePack]\nName = TestMetadata\nClientOnly = True\n[ResourcePack]\nName = Art\nClientOnly = True\n")};
        server_settings.ApplyConfigFile(server_pack_config, "");
        BakerTests::OverrideSetting(server_settings.Common.PlatformBinariesPath, strex(published).combine_path("NoBinaries").str());
        BakerTests::OverrideSetting(server_settings.ServerNetwork.UpdateFilesInMemory, in_memory);
        auto server {TestClientServerIntegration::MakeServerEngine(server_settings)};
        auto shutdown {scope_exit([&]() noexcept { safe_call([&] { server->Shutdown(); }); })};
        string error {TestClientServerIntegration::WaitForServerStart(server)};
        INFO(error);
        REQUIRE(error.empty());

#if !FO_WINDOWS
        if (replace_published) {
            REQUIRE(fs::rename_durable(strex(published).combine_path("Art.fores").str(), strex(published).combine_path("Pinned.fores").str()));
            write_art(published, {{"Keep.bin", unchanged}, {"Change.txt", "replaced after server start"}});
        }
#else
        ignore_unused(replace_published);
#endif

        GlobalSettings client_settings {MakeClientTestSettings(port)};
        BakerTests::OverrideSetting(client_settings.Common.Packaged, true);
        BakerTests::OverrideSetting(client_settings.Common.ResourcesPath, install);
        auto client_pack_config {ConfigFile("[ResourcePack]\nName = Embedded\nClientOnly = True\n[ResourcePack]\nName = TestMetadata\nClientOnly = True\n[ResourcePack]\nName = Art\nClientOnly = True\n")};
        client_settings.ApplyConfigFile(client_pack_config, "");
        client_settings.ApplyWritableRoot(writable);
        FileSystem metadata_source {};
        AddClientPackSource(metadata_source, client_settings, METADATA_PACK_NAME, true);
        REQUIRE_FALSE(ReadResourceConfig(metadata_source, false));
        Updater updater {&client_settings, &GetApp()->MainWindow};
        REQUIRE(WaitForUpdaterResult(updater));
        REQUIRE(updater.GetResult() == UpdaterResult::ResourcesReady);
        REQUIRE_FALSE(updater.IsAborted());
        FileSystem resources {GetClientResources(client_settings)};
        CHECK(resources.ReadFileText("Keep.bin") == unchanged);
        CHECK_FALSE(resources.IsFileExists("Removed.txt"));
    }};

    synchronize(false);
    REQUIRE(fs::exists(patch));
    CHECK_FALSE(fs::exists(replacement));
    CHECK(fs::read_file(installed_base) == original);
    auto first {fs::read_file(patch)};
    REQUIRE(first);
    CHECK(first->size() < 1024);

    SECTION("SecondUpdateAppendsAndRetainsTheCommittedPrefix")
    {
        write_art(published, {{"Keep.bin", unchanged}, {"Renamed.txt", "first update"}, {"Added.txt", "second update"}});
        synchronize(true);
        auto second {fs::read_file(patch)};
        REQUIRE(second);
        CHECK(second->size() > first->size());
        CHECK(second->starts_with(*first));
        ResourcePackSource pair {installed_base, patch};
        CHECK(pair.IsFileExists("Renamed.txt"));
        CHECK_FALSE(pair.IsFileExists("Change.txt"));
        CHECK(pair.GetContentHash() == ResourcePackSource(strex(published).combine_path("Art.fores").str()).GetContentHash());
    }

    SECTION("DamagedBaseTheCatalogStillNamesIsDownloadedAgain")
    {
        // One flipped payload byte leaves the header and the catalog intact, so the pair still reports the advertised
        // content; only reading its bytes back tells the damage apart, and the whole pack is the repair
        string damaged {*original};
        damaged[RESOURCE_PACK_HEADER_SIZE] = static_cast<char>(damaged[RESOURCE_PACK_HEADER_SIZE] ^ 0x01);
        REQUIRE(fs::write_file(installed_base, damaged));

        synchronize(false);
        CHECK(fs::read_file(replacement) == fs::read_file(strex(published).combine_path("Art.fores").str()));
        CHECK_FALSE(fs::exists(patch));
        CHECK(fs::read_file(installed_base) == optional<string> {damaged});
    }

    SECTION("DamagedPatchPayloadIsFetchedAgain")
    {
        string damaged {*first};
        damaged[RESOURCE_PATCH_HEADER_SIZE] = static_cast<char>(damaged[RESOURCE_PATCH_HEADER_SIZE] ^ 0x01);
        REQUIRE(fs::write_file(patch, damaged));

        synchronize(false);
        CHECK_FALSE(fs::exists(replacement));
        auto repaired {fs::read_file(patch)};
        REQUIRE(repaired);
        CHECK(repaired->starts_with(damaged));
        ResourcePackSource pair {installed_base, patch};
        size_t size {0};
        uint64_t write_time {0};
        CHECK(static_cast<bool>(pair.OpenFile("Change.txt", size, write_time)));
        CHECK(size == string_view {"first update"}.size());
    }

    SECTION("PatchThatWouldOutgrowItsPackIsReplacedByThePack")
    {
        constexpr size_t large_size {65 * 1024 * 1024};

        {
            vector<uint8_t> content {vector<uint8_t>(large_size, 'n')};
            string intermediate {strex(writable).combine_path("Intermediate.fores").str()};

            {
                ResourcePackWriter writer {intermediate, {0, 100}};
                writer.AddFile("Keep.bin", {reinterpret_cast<const uint8_t*>(unchanged.data()), unchanged.size()});
                writer.AddFile("Change.txt", content);
                writer.Finish();
            }

            ResourcePackSource target {intermediate};
            ResourcePatchWriter writer {installed_base, patch, target.GetEntryRefs(), target.GetContentHash()};
            REQUIRE(writer.GetDownloads().size() == 1);
            REQUIRE(writer.GetDownloads().front().Path == "Change.txt");
            fs::disk_directory_lock patch_lock {strex(patch).extract_dir().str()};
            writer.Begin(patch_lock);
            writer.AddEncodedFile(content);
            writer.Finish();
        }

        // The published pack dropped the large payload again, so another append would leave a patch far larger than
        // the pack it patches: the pack replaces the pair instead
        REQUIRE(fs::file_size(patch).value() > large_size);
        synchronize(false);
        CHECK(fs::read_file(replacement) == fs::read_file(strex(published).combine_path("Art.fores").str()));
        CHECK_FALSE(fs::exists(patch));
        synchronize(true);
        CHECK_FALSE(fs::exists(patch));
        CHECK(fs::read_file(installed_base) == original);
    }

#if !FO_WINDOWS
    SECTION("DiskBackendPinsTheAdvertisedArtifactAcrossReplacement")
    {
        write_art(published, {{"Keep.bin", unchanged}, {"Pinned.txt", "advertised before replacement"}});
        synchronize(false, true);
        ResourcePackSource pair {installed_base, patch};
        CHECK(pair.IsFileExists("Pinned.txt"));
        CHECK_FALSE(pair.IsFileExists("Change.txt"));
        CHECK(pair.GetContentHash() == ResourcePackSource(strex(published).combine_path("Pinned.fores").str()).GetContentHash());
        CHECK(pair.GetContentHash() != ResourcePackSource(strex(published).combine_path("Art.fores").str()).GetContentHash());
    }
#endif
}

TEST_CASE("ClientUpdaterRepairsAStalePartialNativeDownloadBeforeResourcesReady")
{
    using namespace TestClientServerIntegration;

    if (!CanSelfUpdateNativeModules(GetCurrentUpdatePlatform())) {
        return;
    }

    bool in_memory {GENERATE(false, true)};
    string install {PrepareClientUpdaterBakeOutput()};
    string published_root {MakeTempClientUpdaterBakeDir("native-published")};
    string published {strex(published_root).combine_path("Client").str()};
    auto cleanup_published_root {scope_exit([&]() noexcept { (void)fs::remove_dir_tree(published_root); })};
    string writable {MakeTempClientUpdaterBakeDir("native-writable")};
    auto cleanup {scope_exit([&]() noexcept {
        (void)fs::remove_dir_tree(install);
        (void)fs::remove_dir_tree(published);
        (void)fs::remove_dir_tree(writable);
    })};
    REQUIRE(fs::create_directories(published));
    REQUIRE(fs::create_directories(writable));
    vector<uint8_t> metadata {BakerTests::MakeMetadataBlob({})};

    // The reserved Metadata pack also owns the startup configuration, which this fixture does not package
    for (const string& directory : {install, published}) {
        ResourcePackWriter writer {strex(directory).combine_path("TestMetadata.fores").str()};
        writer.AddFile("Metadata.fometa-client", metadata);
        writer.Finish();
    }

    string binary_dir {strex(published).combine_path("Binaries").str()};
    string platform_dir {strex(binary_dir).combine_path(GetCurrentBinaryUpdateTargetName()).str()};
    REQUIRE(fs::create_directories(platform_dir));
    string pdb_name {strex("{}.pdb", GetCurrentClientRuntimeLibraryName()).str()};
    uint16_t port {IntegrationTestPort.fetch_add(1)};
    GlobalSettings client_settings {MakeClientTestSettings(port)};
    string server_prefix {client_settings.GetPackagedRuntimeName()};

    if (server_prefix.empty()) {
        server_prefix = GetCurrentClientRuntimeLibraryName();
    }

    string source_path {strex(platform_dir).combine_path(strex("{}.pdb", server_prefix)).str()};
    string live_path {strex(writable).combine_path(pdb_name).str()};
    string temp_path {strex(writable).combine_path(strex("~{}", pdb_name)).str()};
    string staged_path {strex("{}-staging", live_path).str()};
    string expected {string(8192, 'n')};
    string obsolete {string(8192, 'o')};
    REQUIRE(fs::write_file(source_path, expected));
    auto expected_hash {fs::hash_file(source_path)};
    REQUIRE(expected_hash);
    bool corrupt_delivery {false};
    bool keep_existing {false};
    bool hold_temp {false};

    SECTION("StalePartialPrefixIsRecoveredInTheSameLaunch")
    {
        REQUIRE(fs::write_file(temp_path, string_view(obsolete).substr(0, 2048)));
    }
    SECTION("ValidPartialPrefixStillResumes")
    {
        REQUIRE(fs::write_file(temp_path, string_view(expected).substr(0, 2048)));
    }
    SECTION("CompleteWrongTempIsDownloadedAgain")
    {
        REQUIRE(fs::write_file(temp_path, obsolete));
    }
    SECTION("OversizedTempIsRemovedBeforeOneFullTransfer")
    {
        REQUIRE(fs::write_file(temp_path, obsolete + obsolete));
    }
    SECTION("ExistingHostSymbolsAreNotReplaced")
    {
        REQUIRE(fs::write_file(live_path, obsolete));
        keep_existing = true;
    }
#if FO_WINDOWS
    SECTION("RefusedCleanupOfCompleteWrongTempFailsWithoutPromotion")
    {
        REQUIRE(fs::write_file(temp_path, obsolete));
        hold_temp = true;
    }
#else
    SECTION("AnotherInvalidFullPayloadFailsWithoutPromotion")
    {
        if (in_memory) {
            return;
        }

        REQUIRE(fs::write_file(temp_path, string_view(obsolete).substr(0, 2048)));
        corrupt_delivery = true;
    }
#endif

#if FO_WINDOWS
    int32_t exclusive_fd {-1};
    auto release_temp {scope_exit([&exclusive_fd]() noexcept {
        if (exclusive_fd >= 0) {
            winapi::close_exclusive_file(exclusive_fd);
        }
    })};

    if (hold_temp) {
        // An open exclusive handle makes the operating system refuse the updater's own cleanup
        exclusive_fd = winapi::open_exclusive_file(temp_path);
        REQUIRE(exclusive_fd >= 0);
        REQUIRE_FALSE(fs::remove_file(temp_path));
    }
#endif

    GlobalSettings server_settings {MakeServerTestSettings(port)};
    BakerTests::OverrideSetting(server_settings.Common.Packaged, true);
    BakerTests::OverrideSetting(server_settings.Common.ResourcesPath, published_root);
    BakerTests::OverrideSetting(server_settings.Common.PlatformBinariesPath, binary_dir);
    BakerTests::OverrideSetting(server_settings.ServerNetwork.UpdateFilesInMemory, in_memory);
    auto server_pack_config {ConfigFile("[ResourcePack]\nName = TestMetadata\nClientOnly = True\n")};
    server_settings.ApplyConfigFile(server_pack_config, "");
    auto server {TestClientServerIntegration::MakeServerEngine(server_settings)};
    auto shutdown {scope_exit([&]() noexcept { safe_call([&] { server->Shutdown(); }); })};
    string error {TestClientServerIntegration::WaitForServerStart(server)};
    INFO(error);
    REQUIRE(error.empty());

#if !FO_WINDOWS
    if (corrupt_delivery) {
        // Overwrite the pinned file; atomic replacement would leave the backend reading the original bytes
        fs::disk_write_file corrupt_source {source_path};
        REQUIRE(static_cast<bool>(corrupt_source));
        REQUIRE(corrupt_source.write(make_const_span(obsolete.data(), obsolete.size())));
        REQUIRE(corrupt_source.flush());
    }
#endif

    BakerTests::OverrideSetting(client_settings.Common.Packaged, true);
    BakerTests::OverrideSetting(client_settings.Common.ResourcesPath, install);
    auto client_pack_config {ConfigFile("[ResourcePack]\nName = Embedded\nClientOnly = True\n[ResourcePack]\nName = TestMetadata\nClientOnly = True\n")};
    client_settings.ApplyConfigFile(client_pack_config, "");
    client_settings.ApplyWritableRoot(writable);
    FileSystem metadata_source {};
    AddClientPackSource(metadata_source, client_settings, METADATA_PACK_NAME, true);
    REQUIRE_FALSE(ReadResourceConfig(metadata_source, false));
    Updater updater {&client_settings, &GetApp()->MainWindow};
    REQUIRE(WaitForUpdaterResult(updater));

#if FO_WINDOWS
    // Release before reading the retained bytes or removing the directory, including on a failing assertion
    if (exclusive_fd >= 0) {
        winapi::close_exclusive_file(exclusive_fd);
        exclusive_fd = -1;
    }
#endif

    if (corrupt_delivery || hold_temp) {
        CHECK(updater.GetResult() == UpdaterResult::Failed);
        CHECK(updater.IsAborted());
        CHECK_FALSE(fs::exists(live_path));
        CHECK_FALSE(fs::exists(staged_path));
        REQUIRE(fs::exists(temp_path));
        CHECK(fs::hash_file(temp_path) != expected_hash);
    }
    else {
        CHECK(updater.GetResult() == UpdaterResult::ResourcesReady);
        CHECK_FALSE(updater.IsAborted());
        CHECK(fs::read_file(live_path) == optional<string> {keep_existing ? obsolete : expected});

        if (!keep_existing) {
            CHECK(fs::hash_file(live_path) == expected_hash);
            CHECK_FALSE(fs::exists(temp_path));
            CHECK_FALSE(fs::exists(staged_path));
        }
    }
}

TEST_CASE("ClientUpdaterConsumesReportedHashListDuringHandshake")
{
    using namespace TestClientServerIntegration;

    auto port {IntegrationTestPort.fetch_add(1)};

    auto server_settings {MakeServerTestSettings(port)};
    auto client_settings {MakeClientTestSettings(port)};
    string updater_bake_output {PrepareClientUpdaterBakeOutput()};
    auto cleanup_updater_bake_output {scope_exit([&updater_bake_output]() noexcept { (void)fs::remove_dir_tree(updater_bake_output); })};
    BakerTests::OverrideSetting(client_settings.Baking.BakeOutput, updater_bake_output);

    // The rig has no resource packs to read a version back from, and this case is about the hash list rather
    // than about pack reading, so the client reports the version the test metadata carries
    BakerTests::OverrideSetting(client_settings.Network.ForceMetadataVersion, string(BakerTests::TEST_METADATA_VERSION));

    auto server {TestClientServerIntegration::MakeServerEngine(server_settings)};
    auto client {TestClientServerIntegration::MakeClientEngine(client_settings)};

    auto shutdown {scope_exit([&server, &client]() noexcept {
        safe_call([&client] { client->Shutdown(); });

        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    })};

    string startup_error {TestClientServerIntegration::WaitForServerStart(server)};
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    client->Connect();
    REQUIRE(WaitForConnected(client, server));

    hstring reported {server->Hashes.to_hashed_string("integration_test_updater_hash")};

    client->GetConnection()->OutBuf->StartMsg(NetMessage::UnresolvedHash);
    client->GetConnection()->OutBuf->Write<hstring::hash_t>(reported.as_hash());
    client->GetConnection()->OutBuf->EndMsg();

    REQUIRE(WaitForLearnedHash(client, reported.as_hash(), "integration_test_updater_hash"));
    client->Disconnect();

    Updater updater {&client_settings, &GetApp()->MainWindow};
    REQUIRE(WaitForUpdaterResult(updater));
    CHECK(updater.GetResult() == UpdaterResult::ResourcesReady);
    CHECK_FALSE(updater.IsAborted());
}

TEST_CASE("ClientUpdaterDoesNotSurfaceOutdatedMetadataLayoutBeforeRepair")
{
    using namespace TestClientServerIntegration;

    uint16_t port {IntegrationTestPort.fetch_add(1)};
    GlobalSettings client_settings {MakeClientTestSettings(port)};
    string updater_bake_output {PrepareClientUpdaterBakeOutput()};
    auto cleanup_updater_bake_output {scope_exit([&updater_bake_output]() noexcept { (void)fs::remove_dir_tree(updater_bake_output); })};
    string pack_name {"OutdatedClientPack"};
    string metadata_path {strex(updater_bake_output).combine_path(pack_name).combine_path("Metadata.fometa-client").str()};

    STATIC_REQUIRE(METADATA_FILE_VERSION > 1);
    constexpr uint16_t outdated_file_version {METADATA_FILE_VERSION - 1};
    vector<uint8_t> outdated_metadata {};
    data_writer outdated_writer {outdated_metadata};
    outdated_writer.write<uint32_t>(METADATA_FILE_MAGIC);
    outdated_writer.write<uint16_t>(outdated_file_version);
    outdated_writer.write<uint16_t>(numeric_cast<uint16_t>(BakerTests::TEST_METADATA_VERSION.length()));
    outdated_writer.write_string_bytes(BakerTests::TEST_METADATA_VERSION);
    outdated_writer.write<uint16_t>(uint16_t {0});

    REQUIRE(fs::write_file(metadata_path, outdated_metadata));
    BakerTests::OverrideSetting(client_settings.Baking.BakeOutput, updater_bake_output);
    auto client_pack_config {ConfigFile(strex("[ResourcePack]\nName = {}\nClientOnly = True\n", pack_name).str())};
    client_settings.ApplyConfigFile(client_pack_config, "");

    CHECK_NOTHROW([&client_settings] {
        Updater updater {&client_settings, &GetApp()->MainWindow};
        ignore_unused(updater);
    }());
}

TEST_CASE("SingleGameplayWorkerProcessesRequestsFromTwoClientsWithoutLostUpdates", "[network][player]")
{
    bool dedicated_remote_only {GENERATE(false, true)};
    bool simulation_paused {GENERATE(false, true)};
    CAPTURE(dedicated_remote_only, simulation_paused);

    int32_t login_calls {0};
    int32_t world_steps {0};
    int32_t world_sum {0};
    map<ident_t, int32_t> steps_by_player {};
    uint16_t port {IntegrationTestPort.fetch_add(1)};
    auto server_settings {MakeServerTestSettings(port)};
    auto first_settings {MakeClientTestSettings(port)};
    auto second_settings {MakeClientTestSettings(port)};
    BakerTests::OverrideSetting(server_settings.Server.SingleThreadedLogic, true);
    BakerTests::OverrideSetting(server_settings.ServerNetwork.DisableNetworking, false);
    BakerTests::OverrideSetting(server_settings.Network.DisableInterthreadCommunication, dedicated_remote_only);
    BakerTests::OverrideSetting(server_settings.Network.WebSocketPort, 0);
    BakerTests::OverrideSetting(first_settings.ClientNetwork.RequireInterthreadTransport, !dedicated_remote_only);
    BakerTests::OverrideSetting(second_settings.ClientNetwork.RequireInterthreadTransport, !dedicated_remote_only);

    auto server {TestClientServerIntegration::MakeServerEngine(server_settings)};
    auto first {TestClientServerIntegration::MakeClientEngine(first_settings)};
    auto second {TestClientServerIntegration::MakeClientEngine(second_settings)};
    auto shutdown {scope_exit([&]() noexcept {
        safe_call([&] { second->Shutdown(); });
        safe_call([&] { first->Shutdown(); });
        safe_call([&] { server->Shutdown(); });
    })};

    REQUIRE(TestClientServerIntegration::WaitForServerStart(server).empty());
    REQUIRE(server->GetWorkerThreadCount() == 1);
    CHECK(HasInterthreadListener(port) == !dedicated_remote_only);
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    {
        auto unlock {scope_exit([&]() noexcept { safe_call([&] { server->Unlock(); }); })};

        // Both scripting backends enter the same validated remote-call boundary. Native handlers keep this
        // worker/transport regression independent of the backend's fixture compiler and call syntax
        server->SetRemoteCallHandler(
            server->Hashes.to_hashed_string("UnitTestLogin"),
            [&](hstring, nptr<Entity> caller, span<uint8_t>, shared_ptr<ScriptCallCompletion>) {
                ptr<Player> player {caller.dyn_cast<Player>()};
                login_calls++;
                auto logged {server->LoginPlayerToTempSession(player)};
                auto cr {server->CreateCritter(server->Hashes.to_hashed_string("UnitTestSharedCritter"), true)};
                auto loc {server->MapMngr.CreateLocation(server->Hashes.to_hashed_string("UnitTestSharedLocation"), vector<hstring> {server->Hashes.to_hashed_string("UnitTestSharedMap")})};
                auto map {loc->GetMapByIndex(0)};
                server->MapMngr.TransferToMap(cr, map, mpos {20, 20}, mdir {}, std::nullopt);
                server->SwitchPlayerCritter(logged, cr);
            },
            true);
        server->SetRemoteCallHandler(
            server->Hashes.to_hashed_string("UnitTestWorldStep"),
            [&](hstring, nptr<Entity> caller, span<uint8_t> data, shared_ptr<ScriptCallCompletion>) {
                ptr<Player> player {caller.dyn_cast<Player>()};
                data_reader reader {data};
                int32_t step {reader.read<int32_t>()};
                world_steps++;
                world_sum += step;
                steps_by_player[player->GetId()]++;
            },
            true);
    }

    auto login {[&](ptr<ClientEngine> client, size_t expected_connections) {
        client->Connect();
        REQUIRE(WaitForConnected(client, server, expected_connections));
        client->SendRemoteCall(client->Hashes.to_hashed_string("UnitTestLogin"), client->GetCurPlayer(), {});
        nanotime deadline {nanotime::now() + std::chrono::seconds {10}};

        while (nanotime::now() < deadline) {
            first->MainLoop();
            second->MainLoop();

            if (client->GetChosen()) {
                return true;
            }

            coarse_sleep(std::chrono::milliseconds {2});
        }

        return false;
    }};

    REQUIRE(login(first, 1));
    REQUIRE(login(second, 2));
    CHECK(first->GetChosen()->GetId() != second->GetChosen()->GetId());
    if (simulation_paused) {
        server->PauseSimulation();
    }
    synctime simulation_time {server->GameTime.GetSynchronizedTime()};
    vector<uint8_t> payload {};
    data_writer writer {payload};
    writer.write<int32_t>(7);

    for (int32_t i {0}; i < 32; i++) {
        first->SendRemoteCall(first->Hashes.to_hashed_string("UnitTestWorldStep"), first->GetCurPlayer(), payload);
        second->SendRemoteCall(second->Hashes.to_hashed_string("UnitTestWorldStep"), second->GetCurPlayer(), payload);
        first->MainLoop();
        second->MainLoop();
    }

    bool complete {false};
    nanotime deadline {nanotime::now() + std::chrono::seconds {10}};

    while (nanotime::now() < deadline) {
        first->MainLoop();
        second->MainLoop();
        REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
        {
            auto unlock {scope_exit([&]() noexcept { safe_call([&] { server->Unlock(); }); })};
            complete = world_steps >= 64;

            if (complete) {
                CHECK(login_calls == 2);
                CHECK(world_steps == 64);
                CHECK(world_sum == 448);
                REQUIRE(steps_by_player.size() == 2);

                for (const auto& [id, steps] : steps_by_player) {
                    CAPTURE(id);
                    CHECK(steps == 32);
                }
            }
        }

        if (complete) {
            break;
        }

        coarse_sleep(std::chrono::milliseconds {2});
    }

    REQUIRE(complete);
    CHECK(GetServerConnectionCount(server) == 2);
    if (simulation_paused) {
        CHECK(server->IsSimulationPaused());
        CHECK(server->GameTime.GetSynchronizedTime() == simulation_time);
        server->ResumeSimulation();
    }
    first->Disconnect();
    second->Disconnect();
    REQUIRE(WaitForServerConnectionCount(server, 0));
}

struct RpcTransportProbe
{
    mutex Locker {};
    shared_ptr<ScriptCallCompletion> Retained {};
    shared_ptr<ServerConnection::RuntimeState> Runtime {};
    std::atomic<bool> Entered {};
    std::atomic<bool> Finished {};
    std::atomic<bool> Release {};
    std::atomic<bool> HasCompletion {};
    std::atomic<uint32_t> Warnings {};
};

template<typename Predicate>
static auto WaitForRpcTransportProbe(Predicate&& predicate) -> bool
{
    nanotime deadline {nanotime::now() + std::chrono::seconds {15}};

    while (!predicate()) {
        if (nanotime::now() >= deadline) {
            return false;
        }

        coarse_sleep(std::chrono::milliseconds {1});
    }

    return true;
}

static void CheckRpcRuntimeTransport(bool disconnect, bool deferred, bool disabled)
{
    auto probe {safe_alloc::make_shared<RpcTransportProbe>()};
    logging::set_callback("rpc-runtime-transport", [probe](logging::type, string_view message, nptr<const stack_trace::catched_data>) mutable {
        if (message.find("RPC runtime warning:") != string_view::npos && message.find("cause=pending-call") != string_view::npos) {
            probe->Warnings.fetch_add(1);
        }
    });
    auto restore_logger {scope_exit([]() noexcept { logging::set_callback("rpc-runtime-transport", {}); })};
    uint16_t port {IntegrationTestPort.fetch_add(1)};
    auto settings {MakeServerTestSettings(port)};
    BakerTests::OverrideSetting(settings.Server.SingleThreadedLogic, true);
    BakerTests::OverrideSetting(settings.Network.DisableZlibCompression, true);
    BakerTests::OverrideSetting(settings.ServerNetwork.RemoteCallWarningTimeMs, !disconnect && !disabled ? 1000 : 0);
    BakerTests::OverrideSetting(settings.ServerNetwork.RemoteCallDisconnectTimeMs, disconnect ? 1000 : 0);
    BakerTests::OverrideSetting(settings.ServerNetwork.MaxPendingRemoteCalls, disabled ? 0 : 8);
    auto server {TestClientServerIntegration::MakeServerEngine(settings)};
    auto shutdown {scope_exit([probe, &server]() mutable noexcept {
        probe->Release.store(true);

        {
            scoped_lock locker {probe->Locker};
            probe->Retained.reset();
        }

        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    })};
    string error {TestClientServerIntegration::WaitForServerStart(server)};
    INFO(error);
    REQUIRE(error.empty());
    hstring call_name {};

    {
        REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
        auto unlock {scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); })};
        call_name = server->Hashes.to_hashed_string("UnitTestLogin");
        server->SetRemoteCallHandler(
            call_name,
            [probe, deferred](hstring, nptr<Entity> caller, span<uint8_t>, shared_ptr<ScriptCallCompletion> completion) mutable {
                auto player {caller.dyn_cast<Player>()};
                FO_VERIFY_AND_THROW(player, "RPC transport probe requires a player");

                {
                    scoped_lock locker {probe->Locker};
                    probe->Runtime = player->GetConnection()->GetRuntimeState();

                    if (deferred) {
                        probe->Retained = completion;
                    }
                }

                probe->HasCompletion.store(!!completion);
                probe->Entered.store(true);

                if (!deferred) {
                    while (!probe->Release.load()) {
                        coarse_sleep(std::chrono::milliseconds {1});
                    }
                }

                probe->Finished.store(true);
            },
            true);
    }

    RawChannelClient client {port};
    NetOutBuffer handshake {128};
    handshake.StartMsg(NetMessage::Handshake);
    handshake.Write(settings.Network.CompatibilityVersion);
    handshake.Write<string_view>(server->GetMetadataVersion());
    handshake.Write<uint32_t>(FO_UPDATER_VERSION);
    handshake.Write<string_view>("Windows-win64");
    handshake.EndMsg();
    client.Send(handshake.GetData());
    REQUIRE(WaitForRpcTransportProbe([&] {
        auto data {client.GetPlaintext()};
        NetInBuffer response {data.size()};
        response.AddData(data);

        if (!response.NeedProcess()) {
            return false;
        }

        REQUIRE(response.ReadMsg() == NetMessage::HandshakeAnswer);
        CHECK_FALSE(response.Read<bool>());
        CHECK_FALSE(response.Read<bool>());
        CHECK_FALSE(response.Read<bool>());
        return true;
    }));
    NetOutBuffer request {32};
    request.StartMsg(NetMessage::RemoteCall);
    request.Write<hstring::hash_t>(call_name.as_hash());
    request.Write<int32_t>(0);
    request.EndMsg();
    client.Send(request.GetData());
    REQUIRE(WaitForRpcTransportProbe([&] { return probe->Entered.load(); }));
    shared_ptr<ServerConnection::RuntimeState> runtime {};

    {
        scoped_lock locker {probe->Locker};
        runtime = probe->Runtime;
    }

    REQUIRE(runtime);

    if (disabled) {
        CHECK_FALSE(probe->HasCompletion.load());
        CHECK_FALSE(ServerConnection::IsRuntimeStateActive(runtime));
        CHECK_FALSE(client.IsDisconnected());
    }
    else if (disconnect) {
        REQUIRE(WaitForRpcTransportProbe([&] { return client.IsDisconnected(); }));
        CHECK(ServerConnection::GetRuntimeDisconnectReason(runtime) == DisconnectReason::RpcRuntimeLimit);
        CHECK(probe->Finished.load() == deferred);
        REQUIRE(WaitForRpcTransportProbe([&] { return !ServerConnection::IsRuntimeStateActive(runtime); }));
        auto diagnostics {ServerConnection::GetRuntimeDiagnostics(runtime)};
        CHECK(diagnostics.PendingRemoteCalls == 0);
        CHECK(diagnostics.AbandonedRemoteCalls == 1);
        CHECK(diagnostics.RuntimeDisconnects == 1);
    }
    else {
        REQUIRE(WaitForRpcTransportProbe([&] { return probe->Warnings.load() != 0; }));
        CHECK_FALSE(client.IsDisconnected());
        CHECK(ServerConnection::GetRuntimeDiagnostics(runtime).PendingRemoteCalls == 1);
        CHECK(ServerConnection::GetRuntimeDisconnectReason(runtime) == DisconnectReason::None);
    }

    probe->Release.store(true);

    {
        scoped_lock locker {probe->Locker};
        probe->Retained.reset();
    }

    REQUIRE(WaitForRpcTransportProbe([&] { return probe->Finished.load(); }));
    REQUIRE(WaitForRpcTransportProbe([&] { return ServerConnection::GetRuntimeDiagnostics(runtime).PendingRemoteCalls == 0; }));
    client.Close();
    CHECK(WaitForServerConnectionCount(server, 0));
}

TEST_CASE("ServerRpcRuntimeWatchdogTransport", "[rpc-runtime-transport]")
{
    SECTION("A warning leaves the client connected")
    {
        CheckRpcRuntimeTransport(false, false, false);
    }

    SECTION("An independent watchdog disconnects while the only script worker is blocked")
    {
        CheckRpcRuntimeTransport(true, false, false);
    }

    SECTION("A deferred completion retains its deadline after the handler returns")
    {
        CheckRpcRuntimeTransport(true, true, false);
    }

    SECTION("Disabled limits do not create completion owners or disconnect a client")
    {
        CheckRpcRuntimeTransport(false, true, true);
    }
}

FO_END_NAMESPACE
