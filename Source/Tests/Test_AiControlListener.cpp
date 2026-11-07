// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

#include "catch_amalgamated.hpp"

#include "AiControlListener.h"

#if FO_HAVE_AI_CONTROL

FO_DISABLE_WARNINGS_PUSH()
#include <json.hpp>
FO_DISABLE_WARNINGS_POP()

FO_BEGIN_NAMESPACE

// Away from the other socket suites and from 47001, which Windows reserves for WinRM
static std::atomic_uint16_t AiControlTestPort {46100};
static constexpr timespan AI_CONTROL_TEST_TIMEOUT {std::chrono::seconds(5)};

static auto MakeAiControlTestConfig(string_view token = {}) -> AiControlListenerConfig
{
    AiControlListenerConfig config {};
    config.Host = "127.0.0.1";
    config.Token = string {token};
    config.MaxQueuedCommands = 8;
    config.MaxEvents = 16;
    return config;
}

static auto StartAiControlTestListener(AiControlListener& listener, AiControlListenerConfig config) -> uint16_t
{
    for (int32_t attempt {}; attempt != 64; attempt++) {
        config.Port = AiControlTestPort.fetch_add(1);

        try {
            listener.Start(config);
            return config.Port;
        }
        catch (const AiControlException& ex) {
            // Only an occupied port is retried; every other refusal is a test failure
            if (!string_view {ex.what()}.starts_with("AiControl listener cannot listen")) {
                throw;
            }
        }
    }

    FAIL("Unable to bind an AiControl test listener");
    return 0;
}

static auto ConnectAiControlTestClient(uint16_t port) -> tcp_socket
{
    tcp_socket client {};
    REQUIRE(client.connect("127.0.0.1", port));
    return client;
}

static void SendAiControlTestText(tcp_socket& client, string_view text)
{
    const_span<uint8_t> remaining {make_const_span(text.data(), text.size())};

    while (!remaining.empty()) {
        REQUIRE(client.can_write(AI_CONTROL_TEST_TIMEOUT));
        int32_t sent {client.send(remaining)};
        REQUIRE(sent > 0);
        remaining = remaining.subspan(numeric_cast<size_t>(sent));
    }
}

// Returns an empty optional when the listener closed the connection before a full line arrived
static auto ReadAiControlTestLine(tcp_socket& client, string& pending) -> optional<string>
{
    nanotime deadline {nanotime::now() + AI_CONTROL_TEST_TIMEOUT};

    while (true) {
        size_t line_end {pending.find('\n')};

        if (line_end != string::npos) {
            string line {pending.substr(0, line_end)};
            pending.erase(0, line_end + 1);
            return line;
        }

        REQUIRE(nanotime::now() < deadline);

        if (!client.can_read(std::chrono::milliseconds(50))) {
            continue;
        }

        array<uint8_t, 4096> buffer {};
        int32_t received {client.receive(span<uint8_t> {buffer.data(), buffer.size()})};

        if (received <= 0) {
            return std::nullopt;
        }

        pending.append(span_to_string(const_span<uint8_t> {buffer.data(), numeric_cast<size_t>(received)}));
    }
}

static auto CallAiControlTestLine(tcp_socket& client, string& pending, string_view request_line) -> nlohmann::json
{
    SendAiControlTestText(client, strex("{}\n", request_line).str());

    optional<string> line {ReadAiControlTestLine(client, pending)};
    REQUIRE(line.has_value());

    nlohmann::json response {};
    response = nlohmann::json::parse(line->begin(), line->end());
    REQUIRE(response.is_object());
    return response;
}

static auto CallAiControlTest(tcp_socket& client, string& pending, string_view method, string_view params_json = "{}") -> nlohmann::json
{
    return CallAiControlTestLine(client, pending, strex(R"({{"jsonrpc":"2.0","id":7,"method":"{}","params":{}}})", method, params_json).str());
}

static auto GetAiControlTestErrorCode(const nlohmann::json& response) -> int64_t
{
    REQUIRE(response.contains("error"));
    REQUIRE_FALSE(response.contains("result"));
    return response["error"]["code"].get<int64_t>();
}

TEST_CASE("AiControlListenerConfiguration")
{
    SECTION("RejectsUnsafeOrIncompleteConfiguration")
    {
        AiControlListener listener {};
        AiControlListenerConfig config {MakeAiControlTestConfig()};

        config.Port = 0;
        CHECK_THROWS_AS(listener.Start(config), AiControlException);

        config.Port = AiControlTestPort.fetch_add(1);
        config.MaxQueuedCommands = 0;
        CHECK_THROWS_AS(listener.Start(config), AiControlException);

        config.MaxQueuedCommands = 1;
        config.MaxEvents = 0;
        CHECK_THROWS_AS(listener.Start(config), AiControlException);

        // A wildcard bind is not loopback, so an empty token must be refused before any socket opens
        config.MaxEvents = 1;
        config.Host = "0.0.0.0";
        CHECK_THROWS_AS(listener.Start(config), AiControlException);
        CHECK_FALSE(listener.IsRunning());
    }

    SECTION("DefaultsToLoopbackAndRejectsSecondStart")
    {
        AiControlListener listener {};
        AiControlListenerConfig config {MakeAiControlTestConfig()};
        config.Host = "";
        uint16_t port {StartAiControlTestListener(listener, config)};

        CHECK(listener.IsRunning());

        nlohmann::json status {};
        status = nlohmann::json::parse(listener.GetStatusJson());
        CHECK(status["host"].get<std::string>() == "127.0.0.1");
        CHECK(status["port"].get<int64_t>() == port);

        config.Port = port;
        CHECK_THROWS_AS(listener.Start(config), AiControlException);

        listener.Stop();
        CHECK_FALSE(listener.IsRunning());
    }

    SECTION("RejectsInvalidPublishedPayloads")
    {
        AiControlListener listener {};

        CHECK_THROWS_AS(listener.SetObservation(""), AiControlException);
        CHECK_THROWS_AS(listener.SetObservation("{\"open\":"), AiControlException);
        CHECK_THROWS_AS(listener.SetObservation("[1,2]"), AiControlException);
        CHECK_THROWS_AS(listener.PushEvent("\"text\""), AiControlException);
        CHECK_THROWS_AS(listener.SetObservation(strex("{{\"blob\":\"{}\"}}", string(numeric_cast<size_t>(AiControlListener::MAX_LINE_BYTES), 'x')).str()), AiControlException);

        listener.SetObservation("{\"schemaVersion\":1}");
        listener.PushEvent("{\"type\":\"ready\"}");
    }
}

TEST_CASE("AiControlListenerProtocol")
{
    AiControlListener listener {};
    uint16_t port {StartAiControlTestListener(listener, MakeAiControlTestConfig("secret-token"))};
    tcp_socket client {ConnectAiControlTestClient(port)};
    string pending {};

    SECTION("TokenIsRequiredPerConnection")
    {
        CHECK(GetAiControlTestErrorCode(CallAiControlTest(client, pending, "ping")) == -32001);

        nlohmann::json wrong {CallAiControlTest(client, pending, "auth", R"({"token":"secret"})")};
        CHECK_FALSE(wrong["result"]["authorized"].get<bool>());
        CHECK(GetAiControlTestErrorCode(CallAiControlTest(client, pending, "status")) == -32001);

        nlohmann::json right {CallAiControlTest(client, pending, "auth", R"({"token":"secret-token"})")};
        CHECK(right["result"]["authorized"].get<bool>());
        CHECK(right["id"].get<int64_t>() == 7);
        CHECK(CallAiControlTest(client, pending, "ping")["result"]["ok"].get<bool>());

        // Authorization never carries over to a new connection
        client.close();
        tcp_socket second {ConnectAiControlTestClient(port)};
        string second_pending {};
        CHECK(GetAiControlTestErrorCode(CallAiControlTest(second, second_pending, "ping")) == -32001);
    }

    SECTION("EnvelopeAndFramingErrors")
    {
        (void)CallAiControlTest(client, pending, "auth", R"({"token":"secret-token"})");

        CHECK(GetAiControlTestErrorCode(CallAiControlTestLine(client, pending, "{not json")) == -32700);
        CHECK(GetAiControlTestErrorCode(CallAiControlTestLine(client, pending, "")) == -32700);
        CHECK(GetAiControlTestErrorCode(CallAiControlTestLine(client, pending, "[1,2,3]")) == -32600);
        CHECK(GetAiControlTestErrorCode(CallAiControlTestLine(client, pending, R"({"id":1,"method":"ping"})")) == -32600);
        CHECK(GetAiControlTestErrorCode(CallAiControlTestLine(client, pending, R"({"jsonrpc":"2.0","id":1,"method":""})")) == -32600);
        CHECK(GetAiControlTestErrorCode(CallAiControlTestLine(client, pending, R"({"jsonrpc":"2.0","id":1,"method":"ping","params":[]})")) == -32600);
        CHECK(GetAiControlTestErrorCode(CallAiControlTestLine(client, pending, R"({"jsonrpc":"2.0","id":{"nested":true},"method":"ping"})")) == -32600);
        CHECK(GetAiControlTestErrorCode(CallAiControlTest(client, pending, "dance")) == -32601);
        CHECK(GetAiControlTestErrorCode(CallAiControlTest(client, pending, "events", R"({"afterSeq":"zero"})")) == -32602);

        // CRLF terminators and several requests in one write are answered in order
        SendAiControlTestText(client, "{\"jsonrpc\":\"2.0\",\"id\":\"a\",\"method\":\"ping\"}\r\n{\"jsonrpc\":\"2.0\",\"id\":\"b\",\"method\":\"ping\"}\n");
        optional<string> first {ReadAiControlTestLine(client, pending)};
        optional<string> second {ReadAiControlTestLine(client, pending)};
        REQUIRE(first.has_value());
        REQUIRE(second.has_value());
        CHECK(nlohmann::json::parse(first->begin(), first->end())["id"].get<std::string>() == "a");
        CHECK(nlohmann::json::parse(second->begin(), second->end())["id"].get<std::string>() == "b");
    }

    SECTION("StatusAndObservation")
    {
        (void)CallAiControlTest(client, pending, "auth", R"({"token":"secret-token"})");

        nlohmann::json status {CallAiControlTest(client, pending, "status")["result"]};
        CHECK(status["running"].get<bool>());
        CHECK(status["port"].get<int64_t>() == port);
        CHECK(status["maxQueuedCommands"].get<int64_t>() == 8);
        CHECK(status["maxEvents"].get<int64_t>() == 16);
        CHECK(status["queuedCommands"].get<int64_t>() == 0);
        CHECK(status["observationSeq"].get<int64_t>() == 0);

        nlohmann::json initial {CallAiControlTest(client, pending, "observe")["result"]};
        CHECK(initial["observationSeq"].get<int64_t>() == 0);
        CHECK(initial["observation"].is_object());

        // A pretty-printed project snapshot must not break the line framing
        listener.SetObservation("{\n  \"schemaVersion\": 3,\r\n  \"ready\": true\n}");
        nlohmann::json updated {CallAiControlTest(client, pending, "observe")["result"]};
        CHECK(updated["observationSeq"].get<int64_t>() == 1);
        CHECK(updated["observation"]["schemaVersion"].get<int64_t>() == 3);
        CHECK(updated["observation"]["ready"].get<bool>());
    }
}

TEST_CASE("AiControlListenerCommands")
{
    AiControlListener listener {};
    AiControlListenerConfig config {MakeAiControlTestConfig()};
    config.MaxQueuedCommands = 3;
    config.MaxEvents = 4;
    uint16_t port {StartAiControlTestListener(listener, config)};
    tcp_socket client {ConnectAiControlTestClient(port)};
    string pending {};

    SECTION("AcceptedCommandsCompleteThroughEvents")
    {
        CHECK(GetAiControlTestErrorCode(CallAiControlTest(client, pending, "act", R"({"targetId":5})")) == -32602);
        CHECK(GetAiControlTestErrorCode(CallAiControlTest(client, pending, "act", R"({"type":"move","x":"5"})")) == -32602);
        CHECK(GetAiControlTestErrorCode(CallAiControlTest(client, pending, "act", R"({"type":"move","x":4294967296})")) == -32602);
        CHECK(GetAiControlTestErrorCode(CallAiControlTest(client, pending, "act", R"({"type":"move","targetId":"12a"})")) == -32602);

        nlohmann::json accepted {CallAiControlTest(client, pending, "act", R"({"type":"move","targetId":"9007199254740993","itemId":4,"x":-3,"y":7,"screenX":100,"screenY":200,"intArg":9,"stringArg":"north","append":true,"custom":{"speed":2}})")};
        REQUIRE(accepted["result"]["accepted"].get<bool>());
        uint32_t command_seq {accepted["result"]["commandSeq"].get<uint32_t>()};
        CHECK(command_seq == 1);

        AiControlCommand command {};
        REQUIRE(listener.TryPullCommand(command));
        CHECK(command.Seq == command_seq);
        CHECK(command.Type == "move");
        CHECK(command.TargetId == ident_t {9007199254740993});
        CHECK(command.ItemId == ident_t {4});
        CHECK(command.AuxId == ident_t {});
        CHECK(command.X == -3);
        CHECK(command.Y == 7);
        CHECK(command.ScreenX == 100);
        CHECK(command.ScreenY == 200);
        CHECK(command.IntArg == 9);
        CHECK(command.StringArg == "north");
        CHECK(command.Append);

        nlohmann::json params {};
        params = nlohmann::json::parse(command.ParamsJson.begin(), command.ParamsJson.end());
        CHECK(params["custom"]["speed"].get<int64_t>() == 2);
        CHECK_FALSE(listener.TryPullCommand(command));

        listener.CompleteCommand(command_seq, true, "moved");

        nlohmann::json events {CallAiControlTest(client, pending, "events", R"({"afterSeq":0})")["result"]};
        REQUIRE(events["events"].size() == 1);
        CHECK(events["latestSeq"].get<int64_t>() == 1);
        CHECK(events["events"][0]["event"]["type"].get<std::string>() == "command_completed");
        CHECK(events["events"][0]["event"]["commandSeq"].get<uint32_t>() == command_seq);
        CHECK(events["events"][0]["event"]["success"].get<bool>());
        CHECK(events["events"][0]["event"]["message"].get<std::string>() == "moved");

        // The cursor is exclusive
        nlohmann::json after {CallAiControlTest(client, pending, "events", R"({"afterSeq":1})")["result"]};
        CHECK(after["events"].empty());
        CHECK(after["latestSeq"].get<int64_t>() == 1);
    }

    SECTION("QueueAndHistoryAreBounded")
    {
        for (int32_t i {}; i != 3; i++) {
            CHECK(CallAiControlTest(client, pending, "act", R"({"type":"wait"})")["result"]["accepted"].get<bool>());
        }

        // A full queue rejects instead of dropping an accepted command
        CHECK(GetAiControlTestErrorCode(CallAiControlTest(client, pending, "act", R"({"type":"wait"})")) == -32002);

        for (int32_t i {}; i != 6; i++) {
            listener.PushEvent(strex("{{\"type\":\"tick\",\"index\":{}}}", i).str());
        }

        nlohmann::json events {CallAiControlTest(client, pending, "events", R"({"limit":100})")["result"]};
        REQUIRE(events["events"].size() == 4);
        CHECK(events["latestSeq"].get<int64_t>() == 6);
        CHECK(events["events"][0]["seq"].get<int64_t>() == 3);
        CHECK(events["events"][3]["event"]["index"].get<int64_t>() == 5);

        nlohmann::json limited {CallAiControlTest(client, pending, "events", R"({"limit":-5})")["result"]};
        CHECK(limited["events"].size() == 1);
    }

    SECTION("RegisteredHandlersKeepQueueOrder")
    {
        vector<uint32_t> handled {};

        listener.RegisterCommandHandler("native", [&handled](const AiControlCommand& command) {
            handled.emplace_back(command.Seq);
            return AiControlCommandResult {.Success = true, .Message = "native done"};
        });
        CHECK_THROWS_AS(listener.RegisterCommandHandler("native", [](const AiControlCommand&) { return AiControlCommandResult {}; }), AiControlException);

        (void)CallAiControlTest(client, pending, "act", R"({"type":"native"})");
        (void)CallAiControlTest(client, pending, "act", R"({"type":"scripted"})");
        (void)CallAiControlTest(client, pending, "act", R"({"type":"native"})");

        // Dispatch stops at the first command without a native handler
        listener.DispatchCommands();
        CHECK(handled == vector<uint32_t> {1});

        AiControlCommand command {};
        REQUIRE(listener.TryPullCommand(command));
        CHECK(command.Type == "scripted");
        CHECK(command.Seq == 2);

        listener.DispatchCommands();
        CHECK(handled == vector<uint32_t> {1, 3});
        CHECK_FALSE(listener.TryPullCommand(command));

        nlohmann::json events {CallAiControlTest(client, pending, "events")["result"]};
        REQUIRE(events["events"].size() == 2);
        CHECK(events["events"][0]["event"]["commandSeq"].get<uint32_t>() == 1);
        CHECK(events["events"][1]["event"]["message"].get<std::string>() == "native done");
    }

    SECTION("FailingHandlerStillCompletesItsCommand")
    {
        listener.RegisterCommandHandler({}, [](const AiControlCommand& command) -> AiControlCommandResult { throw GenericException("Handler refused", command.Type); });

        (void)CallAiControlTest(client, pending, "act", R"({"type":"unknown"})");

        AiControlCommand command {};
        CHECK_THROWS_AS(listener.TryPullCommand(command), GenericException);

        nlohmann::json events {CallAiControlTest(client, pending, "events")["result"]};
        REQUIRE(events["events"].size() == 1);
        CHECK_FALSE(events["events"][0]["event"]["success"].get<bool>());
        CHECK(events["events"][0]["event"]["message"].get<std::string>().find("Handler refused") != std::string::npos);
    }
}

TEST_CASE("AiControlListenerPeersAndShutdown")
{
    AiControlListener listener {};
    AiControlListenerConfig config {MakeAiControlTestConfig()};
    uint16_t port {StartAiControlTestListener(listener, config)};

    SECTION("OversizedRequestClosesOnlyThatConnection")
    {
        tcp_socket client {ConnectAiControlTestClient(port)};
        string pending {};

        SendAiControlTestText(client, string(numeric_cast<size_t>(AiControlListener::MAX_LINE_BYTES) + 16, 'x'));

        optional<string> line {ReadAiControlTestLine(client, pending)};
        REQUIRE(line.has_value());
        nlohmann::json response {};
        response = nlohmann::json::parse(line->begin(), line->end());
        CHECK(GetAiControlTestErrorCode(response) == -32600);
        CHECK_FALSE(ReadAiControlTestLine(client, pending).has_value());

        nlohmann::json status {};
        status = nlohmann::json::parse(listener.GetStatusJson());
        CHECK(status["lastError"].get<std::string>().find("exceeded") != std::string::npos);
        CHECK(listener.IsRunning());

        tcp_socket next {ConnectAiControlTestClient(port)};
        string next_pending {};
        CHECK(CallAiControlTest(next, next_pending, "ping")["result"]["ok"].get<bool>());
    }

    SECTION("AbandonedPartialRequestDoesNotStopTheListener")
    {
        {
            tcp_socket client {ConnectAiControlTestClient(port)};
            SendAiControlTestText(client, "{\"jsonrpc\":\"2.0\",\"id\":1,");
        }

        tcp_socket next {ConnectAiControlTestClient(port)};
        string next_pending {};
        CHECK(CallAiControlTest(next, next_pending, "ping")["result"]["ok"].get<bool>());
    }

    SECTION("StopIsBoundedAndFailsQueuedCommands")
    {
        tcp_socket client {ConnectAiControlTestClient(port)};
        string pending {};
        CHECK(CallAiControlTest(client, pending, "act", R"({"type":"wait"})")["result"]["accepted"].get<bool>());

        // The idle connection stays open while the listener stops
        nanotime stop_started {nanotime::now()};
        listener.Stop();
        CHECK((nanotime::now() - stop_started) < timespan {std::chrono::seconds(2)});
        CHECK_FALSE(listener.IsRunning());

        AiControlCommand command {};
        CHECK_FALSE(listener.TryPullCommand(command));

        // The same port is available again, and the history keeps the failed completion
        config.Port = port;
        listener.Start(config);
        tcp_socket next {ConnectAiControlTestClient(port)};
        string next_pending {};
        nlohmann::json events {CallAiControlTest(next, next_pending, "events")["result"]};
        REQUIRE(events["events"].size() == 1);
        CHECK(events["events"][0]["event"]["commandSeq"].get<uint32_t>() == 1);
        CHECK_FALSE(events["events"][0]["event"]["success"].get<bool>());
    }

    SECTION("DestructionStopsARunningListener")
    {
        uint16_t owned_port {};
        tcp_socket client {};

        {
            AiControlListener owned {};
            owned_port = StartAiControlTestListener(owned, config);
            client = ConnectAiControlTestClient(owned_port);
        }

        tcp_socket refused {};
        CHECK_FALSE(refused.connect("127.0.0.1", owned_port));
    }
}

FO_END_NAMESPACE

#endif
