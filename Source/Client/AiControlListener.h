// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(AiControlException);

// Values come from project settings; Engine owns only the transport policy applied to them
struct AiControlListenerConfig
{
    string Host {}; // IPv4 address or host name, empty selects 127.0.0.1
    uint16_t Port {};
    string Token {}; // Empty is accepted only for a loopback address
    int32_t MaxQueuedCommands {};
    int32_t MaxEvents {};
};

// One accepted act request; the standard slots are decoded and ParamsJson keeps the complete params object
struct AiControlCommand
{
    uint32_t Seq {};
    string Type {};
    ident_t TargetId {};
    ident_t ItemId {};
    ident_t AuxId {};
    int32_t X {};
    int32_t Y {};
    int32_t ScreenX {};
    int32_t ScreenY {};
    int32_t IntArg {};
    string StringArg {};
    bool Append {};
    string ParamsJson {};
};

struct AiControlCommandResult
{
    bool Success {};
    string Message {};
};

using AiControlCommandHandler = function<AiControlCommandResult(const AiControlCommand&)>;

#if FO_HAVE_AI_CONTROL

// Development TCP listener for the AiControl protocol: the socket thread only parses and queues,
// commands run on the thread that calls DispatchCommands or TryPullCommand (the client loop)
class AiControlListener final
{
public:
    static constexpr int32_t MAX_LINE_BYTES {1024 * 1024};

    AiControlListener() = default;
    AiControlListener(const AiControlListener&) = delete;
    AiControlListener(AiControlListener&&) noexcept = delete;
    auto operator=(const AiControlListener&) = delete;
    auto operator=(AiControlListener&&) noexcept = delete;
    ~AiControlListener();

    [[nodiscard]] auto IsRunning() const noexcept -> bool;
    [[nodiscard]] auto GetStatusJson() const -> string;

    void RegisterCommandHandler(string_view type, AiControlCommandHandler handler);
    void Start(const AiControlListenerConfig& config);
    void DispatchCommands();
    auto TryPullCommand(AiControlCommand& command) -> bool;
    void SetObservation(string_view observation_json);
    void PushEvent(string_view event_json);
    void CompleteCommand(uint32_t command_seq, bool success, string_view message);
    void Stop();

private:
    struct QueuedEvent
    {
        uint64_t Seq {};
        string Json {};
    };

    void RunConnections();
    void ProcessInput(tcp_socket& connection, string& input, bool& authorized);
    auto HandleLine(string_view line, bool& authorized) -> string;
    auto EnqueueCommand(AiControlCommand command) -> optional<uint32_t>;
    auto BuildEventsJson(uint64_t after_seq, int32_t limit) const -> string;
    auto SendResponse(tcp_socket& connection, string_view response) -> bool;
    auto TakeNextCommand(AiControlCommand& command, nptr<AiControlCommandHandler>& handler, bool take_unhandled) -> bool;
    void RunCommandHandler(AiControlCommandHandler& handler, const AiControlCommand& command);
    void AppendEvent(string event_json);
    void FailQueuedCommands(string_view message);
    void SetLastError(string_view error);
    void RegisterLogCallback();
    void UnregisterLogCallback() noexcept;

    AiControlListenerConfig _config {};
    tcp_server _server {};
    thread _thread {};
    std::atomic_bool _stopRequested {};
    std::atomic_bool _running {};
    string _logCallbackKey {};
    unordered_map<string, AiControlCommandHandler> _commandHandlers {};

    mutable mutex _locker {};
    uint32_t _nextCommandSeq FO_TSA_GUARDED_BY(_locker) {};
    uint64_t _nextEventSeq FO_TSA_GUARDED_BY(_locker) {};
    uint64_t _observationSeq FO_TSA_GUARDED_BY(_locker) {};
    string _observationJson FO_TSA_GUARDED_BY(_locker) {"{}"};
    deque<AiControlCommand> _commands FO_TSA_GUARDED_BY(_locker) {};
    deque<QueuedEvent> _events FO_TSA_GUARDED_BY(_locker) {};
    string _lastError FO_TSA_GUARDED_BY(_locker) {};
};

#endif

FO_END_NAMESPACE
