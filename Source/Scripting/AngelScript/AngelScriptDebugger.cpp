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

#include "AngelScriptDebugger.h"

#if FO_ANGELSCRIPT_SCRIPTING

#include "AngelScriptBackend.h"
#include "AngelScriptHelpers.h"

#include <json.hpp>
#include <preprocessor.h>

FO_BEGIN_NAMESPACE

static constexpr uint16 ANGELSCRIPT_DEBUGGER_TCP_BASE_PORT = 43000;
static constexpr uint16 ANGELSCRIPT_DEBUGGER_TCP_PORT_SPAN = 2000;
static constexpr uint16 ANGELSCRIPT_DEBUGGER_DISCOVERY_PORT = 43001;
static constexpr string_view ANGELSCRIPT_DEBUGGER_DISCOVERY_PROBE = "fos-debug-discover-v1";
static constexpr timespan ANGELSCRIPT_DEBUGGER_IO_POLL_TIMEOUT = std::chrono::milliseconds(10);
static constexpr timespan ANGELSCRIPT_DEBUGGER_PAUSE_POLL_SLEEP = std::chrono::milliseconds(10);

struct DebuggerStepState
{
    DebuggerStepMode Mode {};
    uint32 BaseDepth {};
    uint32 LastStoppedDepth {};
    bool HasLastStoppedDepth {};
    optional<string> BreakpointSourcePath {};
    uint32 BreakpointSourceLine {};
    optional<string> BaseSourcePath {};
    uint32 BaseSourceLine {};
};

class DebuggerEndpointServer::Impl
{
public:
    struct LocalVariableInfo
    {
        string Name {};
        string Type {};
        string Value {};
    };

    explicit Impl(const AngelScriptBackend* backend, DebuggerEndpointServer* owner);

    Impl(const Impl&) = delete;
    auto operator=(const Impl&) = delete;
    Impl(Impl&&) noexcept = delete;
    auto operator=(Impl&&) noexcept = delete;

    ~Impl();

    [[nodiscard]] auto IsPaused() const noexcept -> bool { return _paused.load(); }
    [[nodiscard]] auto IsClientConnected() const noexcept -> bool { return _clientConnected.load(); }
    [[nodiscard]] auto HasAnyBreakpoints() const noexcept -> bool { return _hasAnyBreakpoints.load(); }
    [[nodiscard]] auto IsLineProcessingNeeded() const noexcept -> bool { return _clientConnected.load() && (_paused.load() || _pauseStartPending.load() || _singleStepRequested.load() || _hasAnyBreakpoints.load()); }

    void RequestPause();
    auto ConsumePauseStart() -> bool;
    void RequestSingleStep(DebuggerStepMode mode);
    auto ConsumeSingleStepRequest() -> DebuggerStepMode;
    void SetBreakpoints(string_view source_path, const vector<uint32>& lines);
    void SetupContext(AngelScript::asIScriptContext* ctx, AngelScriptContextSetupReason reason);
    void ProcessLine(AngelScript::asIScriptContext* ctx);
    auto HasBreakpoint(string_view source_path, uint32 line) const -> bool;
    void EmitEvent(string_view event_name, string_view body_json = "{}");
    void Stop() noexcept;

private:
    struct RequestResult
    {
        string Response;
        bool Disconnect {};
        optional<string> Event {};
    };

    auto MakeAttachHandshakeMessage() const -> string;
    auto ExtractRequestId(string_view message) const -> optional<int32>;
    auto ExtractRequestCommand(string_view message) const -> string;
    auto MakeDebuggerResponse(int32 request_id, string_view command, bool success, string_view body_json = "{}") const -> string;
    auto MakeDebuggerEventMessage(string_view event_name, string_view body_json = "{}") const -> string;
    auto SendToActiveClient(string_view message) -> bool;
    auto HandleRequestLine(string_view line) -> RequestResult;
    void Run();
    void RunTcp();
    void HandleTcpClient(tcp_socket client_sock);
    void RunDiscoveryResponder();

    raw_ptr<DebuggerEndpointServer> _owner {};
    string _endpoint {};
    string _instanceId {};
    string _host {};
    string _bindHost {};
    string _targetName {};
    uint16 _port {};
    uint16 _discoveryPort {ANGELSCRIPT_DEBUGGER_DISCOVERY_PORT};
    std::atomic_bool _paused {false};
    std::atomic_bool _pauseStartPending {false};
    std::atomic_bool _singleStepRequested {false};
    std::atomic<DebuggerStepMode> _singleStepMode {DebuggerStepMode::In};
    raw_ptr<AngelScript::asIScriptContext> _pausedContext {};
    raw_ptr<AngelScript::asIScriptContext> _pausedRootContext {};
    std::atomic_bool _clientConnected {false};
    std::atomic_bool _hasAnyBreakpoints {false};
    std::atomic_bool _stopped {false};
    std::thread _thread {};
    std::thread _discoveryThread {};
    std::shared_mutex _clientIoLocker {};
    mutable std::mutex _stackTraceLocker {};
    StackTraceData _lastStackTrace {};
    vector<vector<LocalVariableInfo>> _lastLocalsByFrame {};
    string _lastStoppedSourcePath {};
    uint32 _lastStoppedSourceLine {};
    string _lastStoppedFunction {};
    bool _hasLastStoppedLocation {};
    mutable std::mutex _breakpointsLocker {};
    unordered_map<string, unordered_set<uint32>> _lineBreakpoints {};
    tcp_server _tcpServer {};
    tcp_socket _activeClientSock {};
    udp_socket _discoverySocket {};
};

DebuggerEndpointServer::Impl::Impl(const AngelScriptBackend* backend, DebuggerEndpointServer* owner) :
    _owner {owner}
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_owner);

    _bindHost = "0.0.0.0";

    if (backend->HasGameEngine()) {
        const string host = backend->GetGameEngine()->Settings.DebuggerBindHost;

        if (!host.empty()) {
            _bindHost = host;
        }
    }

    _host = _bindHost;

    if (_host.empty() || _host == "0.0.0.0" || _host == "*") {
        _host = "127.0.0.1";
    }

    switch (backend->GetMetadata()->GetSide()) {
    case EngineSideKind::ServerSide:
        _targetName = "server";
        break;
    case EngineSideKind::ClientSide:
        _targetName = "client";
        break;
    case EngineSideKind::MapperSide:
        _targetName = "mapper";
        break;
    }

    const bool sockets_startup = net_sockets::startup();
    FO_RUNTIME_ASSERT(sockets_startup);

    constexpr uint16 base_port = ANGELSCRIPT_DEBUGGER_TCP_BASE_PORT;
    constexpr uint16 span = ANGELSCRIPT_DEBUGGER_TCP_PORT_SPAN;
    const int32 pid_num = strvex(Platform::GetCurrentProcessIdStr()).to_int32();
    const uint16 start_offset = pid_num > 0 ? numeric_cast<uint16>(pid_num % span) : uint16 {0};

    bool listen_ok = false;

    for (uint16 i = 0; i < span; i++) {
        const uint16 candidate_offset = numeric_cast<uint16>((start_offset + i) % span);
        const uint16 candidate_port = numeric_cast<uint16>(base_port + candidate_offset);

        if (_tcpServer.listen(_bindHost, candidate_port, 1)) {
            _port = candidate_port;
            _endpoint = strex("tcp://{}:{}", _host, _port).str();
            _instanceId = strex("{}:{}", Platform::GetCurrentProcessIdStr(), _port).str();
            listen_ok = true;
            break;
        }
    }

    if (!listen_ok) {
        throw std::runtime_error("Can't listen AngelScript debugger tcp endpoint in configured port range");
    }

    _thread = std::thread([this]() { this->Run(); });
    _discoveryThread = std::thread([this]() { this->RunDiscoveryResponder(); });
}

DebuggerEndpointServer::Impl::~Impl()
{
    FO_STACK_TRACE_ENTRY();

    Stop();
}

void DebuggerEndpointServer::Impl::RequestPause()
{
    FO_STACK_TRACE_ENTRY();

    if (!_paused.exchange(true)) {
        _pauseStartPending = true;
    }
}

auto DebuggerEndpointServer::Impl::ConsumePauseStart() -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _pauseStartPending.exchange(false);
}

void DebuggerEndpointServer::Impl::RequestSingleStep(DebuggerStepMode mode)
{
    FO_STACK_TRACE_ENTRY();

    _pauseStartPending = false;
    _paused = false;
    _singleStepMode = mode;
    _singleStepRequested = true;
}

auto DebuggerEndpointServer::Impl::ConsumeSingleStepRequest() -> DebuggerStepMode
{
    FO_NO_STACK_TRACE_ENTRY();

    const bool requested = _singleStepRequested.exchange(false);

    if (requested) {
        return _singleStepMode.load();
    }

    return DebuggerStepMode::None;
}

void DebuggerEndpointServer::Impl::SetBreakpoints(string_view source_path, const vector<uint32>& lines)
{
    FO_STACK_TRACE_ENTRY();

    const string key = strvex(source_path).extract_file_name().str();

    std::scoped_lock locker {_breakpointsLocker};

    if (lines.empty()) {
        _lineBreakpoints.erase(key);
    }
    else {
        auto& source_breakpoints = _lineBreakpoints[key];
        source_breakpoints.clear();

        for (const uint32 line : lines) {
            source_breakpoints.emplace(line);
        }
    }

    _hasAnyBreakpoints = !_lineBreakpoints.empty();
}

void DebuggerEndpointServer::Impl::SetupContext(AngelScript::asIScriptContext* ctx, AngelScriptContextSetupReason reason)
{
    FO_STACK_TRACE_ENTRY();

    auto* ctx_ext = AngelScriptContextExtendedData::Get(ctx);
    FO_RUNTIME_ASSERT(ctx_ext);

    if (reason == AngelScriptContextSetupReason::Create) {
        int32 as_result = 0;
        FO_AS_VERIFY(ctx->SetLineCallback(asFUNCTION(AngelScriptLine), cast_to_void(_owner.get()), AngelScript::asCALL_CDECL));
        ctx_ext->StepState = SafeAlloc::MakeShared<DebuggerStepState>();
    }
    else if (reason == AngelScriptContextSetupReason::Request) {
        if (ctx_ext->Parent != nullptr) {
            auto* parent_ctx_ext = AngelScriptContextExtendedData::Get(ctx_ext->Parent.get());
            FO_RUNTIME_ASSERT(parent_ctx_ext);

            ctx_ext->StepState->Mode = parent_ctx_ext->StepState->Mode == DebuggerStepMode::In ? DebuggerStepMode::In : DebuggerStepMode::None;
        }
    }
    else if (reason == AngelScriptContextSetupReason::Return) {
        if (ctx_ext->Parent != nullptr) {
            auto* parent_ctx_ext = AngelScriptContextExtendedData::Get(ctx_ext->Parent.get());
            FO_RUNTIME_ASSERT(parent_ctx_ext);

            if (ctx_ext->StepState->Mode == DebuggerStepMode::None) {
                parent_ctx_ext->StepState->Mode = parent_ctx_ext->StepState->Mode == DebuggerStepMode::Out ? DebuggerStepMode::Out : DebuggerStepMode::None;
            }
            else {
                parent_ctx_ext->StepState->Mode = DebuggerStepMode::In;
                parent_ctx_ext->StepState->BaseSourcePath.reset();
            }
        }

        *ctx_ext->StepState = {};
    }
}

void DebuggerEndpointServer::Impl::ProcessLine(AngelScript::asIScriptContext* ctx)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!IsLineProcessingNeeded()) {
        return;
    }

    auto* ctx_ext = AngelScriptContextExtendedData::Get(ctx);

    if (ctx_ext == nullptr) {
        return;
    }

    const auto resolve_debug_location = [&]() {
        const uint32 ctx_line = numeric_cast<uint32>(ctx->GetLineNumber());
        const auto* lnt = cast_from_void<Preprocessor::LineNumberTranslator*>(ctx->GetEngine()->GetUserData(5));
        const auto& orig_file = Preprocessor::ResolveOriginalFile(ctx_line, lnt);
        const uint32 orig_line = Preprocessor::ResolveOriginalLine(ctx_line, lnt);
        const uint32 debugger_line = orig_line > 0 ? numeric_cast<uint32>(orig_line - 1) : 0u;
        return std::make_pair(string {orig_file}, debugger_line);
    };

    const auto get_function_name = [&]() -> string_view {
        const auto* current_func = ctx->GetFunction();
        return current_func != nullptr ? string_view {current_func->GetName()} : string_view {};
    };

    const auto emit_stopped = [&](string_view reason, string_view text, string_view source_path, uint32 source_line, string_view function_name) {
        nlohmann::json body;
        body["reason"] = reason;

        if (!text.empty()) {
            body["text"] = text;
        }

        if (!source_path.empty()) {
            body["source"] = source_path;
        }

        body["line"] = source_line;

        if (!function_name.empty()) {
            body["function"] = function_name;
        }

        EmitEvent("stopped", body.dump());
    };

    const auto capture_stack_trace = [&](string_view source_path, uint32 source_line, string_view function_name) {
        std::scoped_lock locker {_stackTraceLocker};

        _lastStackTrace = GetStackTrace();
        _lastStoppedSourcePath = string {source_path};
        _lastStoppedSourceLine = source_line;
        _lastStoppedFunction = string {function_name};
        _hasLastStoppedLocation = !_lastStoppedSourcePath.empty() || !_lastStoppedFunction.empty();

        _lastLocalsByFrame.clear();
        const uint32 callstack_size = ctx->GetCallstackSize();
        _lastLocalsByFrame.resize(callstack_size > 0 ? numeric_cast<size_t>(callstack_size) : 0);

        for (uint32 stack_level = 0; stack_level < callstack_size; stack_level++) {
            const AngelScript::asUINT as_stack_level = numeric_cast<AngelScript::asUINT>(stack_level);
            const int32 vars_count = ctx->GetVarCount(as_stack_level);
            auto& frame_vars = _lastLocalsByFrame[numeric_cast<size_t>(stack_level)];
            frame_vars.reserve(vars_count > 0 ? numeric_cast<size_t>(vars_count) : 0);

            for (int32 var_index = 0; var_index < vars_count; var_index++) {
                const AngelScript::asUINT as_var_index = numeric_cast<AngelScript::asUINT>(var_index);
                const char* var_name = nullptr;
                int var_type_id = 0;
                AngelScript::asETypeModifiers type_modifiers = AngelScript::asTM_NONE;
                bool is_var_on_heap = false;
                int stack_offset = 0;
                ctx->GetVar(as_var_index, as_stack_level, &var_name, &var_type_id, &type_modifiers, &is_var_on_heap, &stack_offset);
                ignore_unused(type_modifiers);
                ignore_unused(is_var_on_heap);
                ignore_unused(stack_offset);
                const auto* var_decl = ctx->GetVarDeclaration(as_var_index, as_stack_level, true);
                auto* var_addr = ctx->GetAddressOfVar(as_var_index, as_stack_level, false, false);

                LocalVariableInfo var;
                var.Name = var_name != nullptr && *var_name != '\0' ? string {var_name} : strex("var{}", var_index).str();
                var.Type = var_decl != nullptr ? string {var_decl} : string {};

                if (var_addr != nullptr) {
                    const bool is_handle = (var_type_id & AngelScript::asTYPEID_OBJHANDLE) != 0;
                    const int32 base_type_id = var_type_id & ~(AngelScript::asTYPEID_OBJHANDLE | AngelScript::asTYPEID_HANDLETOCONST);

                    if (is_handle) {
                        const auto* obj = *static_cast<void**>(var_addr);
                        var.Value = obj != nullptr ? GetScriptObjectInfo(obj, base_type_id) : string {"null"};
                    }
                    else {
                        var.Value = GetScriptObjectInfo(var_addr, base_type_id);
                    }
                }
                else {
                    var.Value = "<unavailable>";
                }

                frame_vars.emplace_back(std::move(var));
            }
        }
    };

    auto* step_state = ctx_ext->StepState.get();

    if (ConsumePauseStart()) {
        const auto& [source_path, source_line] = resolve_debug_location();

        step_state->LastStoppedDepth = ctx->GetCallstackSize();
        step_state->HasLastStoppedDepth = true;
        step_state->BreakpointSourcePath = source_path;
        step_state->BreakpointSourceLine = source_line;

        _pausedContext = ctx;
        _pausedRootContext = ctx_ext->Root;

        capture_stack_trace(source_path, source_line, get_function_name());
        emit_stopped("pause", "", source_path, source_line, get_function_name());
    }

    while (IsPaused()) {
        std::this_thread::sleep_for(ANGELSCRIPT_DEBUGGER_PAUSE_POLL_SLEEP.value());
    }

    if (HasAnyBreakpoints()) {
        const auto& [source_path, source_line] = resolve_debug_location();

        if (HasBreakpoint(source_path, source_line)) {
            step_state->LastStoppedDepth = ctx->GetCallstackSize();
            step_state->HasLastStoppedDepth = true;
            step_state->BreakpointSourcePath = source_path;
            step_state->BreakpointSourceLine = source_line;

            _pausedContext = ctx;
            _pausedRootContext = ctx_ext->Root;

            capture_stack_trace(source_path, source_line, get_function_name());
            emit_stopped("breakpoint", "", source_path, source_line, get_function_name());

            RequestPause();
            ConsumePauseStart();

            while (IsPaused()) {
                std::this_thread::sleep_for(ANGELSCRIPT_DEBUGGER_PAUSE_POLL_SLEEP.value());
            }

            return;
        }
    }

    auto step_mode = DebuggerStepMode::None;

    if (_singleStepRequested.load()) {
        const auto pending_mode = _singleStepMode.load();
        bool can_consume = false;

        if (pending_mode == DebuggerStepMode::In) {
            can_consume = _pausedContext == ctx || _pausedRootContext == ctx_ext->Root;
        }
        else {
            can_consume = _pausedContext == ctx;
        }

        if (can_consume) {
            step_mode = ConsumeSingleStepRequest();
        }
    }

    if (step_mode != DebuggerStepMode::None) {
        step_state->Mode = step_mode;
        step_state->BaseDepth = step_state->HasLastStoppedDepth ? step_state->LastStoppedDepth : ctx->GetCallstackSize();

        if (step_state->BreakpointSourcePath.has_value()) {
            step_state->BaseSourcePath = step_state->BreakpointSourcePath;
            step_state->BaseSourceLine = step_state->BreakpointSourceLine;
        }
        else {
            const auto& [source_path, source_line] = resolve_debug_location();
            step_state->BaseSourcePath = source_path;
            step_state->BaseSourceLine = source_line;
        }
    }

    if (step_state->Mode != DebuggerStepMode::None) {
        const uint32 depth = ctx->GetCallstackSize();
        const auto& [source_path, source_line] = resolve_debug_location();
        const bool has_moved_from_base_location = !step_state->BaseSourcePath.has_value() || step_state->BaseSourceLine != source_line || step_state->BaseSourcePath != source_path;

        const bool should_stop = [&]() -> bool {
            switch (step_state->Mode) {
            case DebuggerStepMode::In:
                return has_moved_from_base_location;
            case DebuggerStepMode::Over:
                return depth <= step_state->BaseDepth && has_moved_from_base_location;
            case DebuggerStepMode::Out:
                return depth < step_state->BaseDepth && has_moved_from_base_location;
            case DebuggerStepMode::None:
                return false;
            }
            return false;
        }();

        if (should_stop) {
            step_state->Mode = DebuggerStepMode::None;
            step_state->LastStoppedDepth = depth;
            step_state->HasLastStoppedDepth = true;
            step_state->BreakpointSourcePath = source_path;
            step_state->BreakpointSourceLine = source_line;
            step_state->BaseSourcePath.reset();

            _pausedContext = ctx;
            _pausedRootContext = ctx_ext->Root;
            _pauseStartPending = false;
            _paused = true;

            capture_stack_trace(source_path, source_line, get_function_name());
            emit_stopped("step", "", source_path, source_line, get_function_name());

            while (IsPaused()) {
                std::this_thread::sleep_for(ANGELSCRIPT_DEBUGGER_PAUSE_POLL_SLEEP.value());
            }
        }
    }
}

auto DebuggerEndpointServer::Impl::HasBreakpoint(string_view source_path, uint32 line) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    const string key = strvex(source_path).extract_file_name().str();

    std::scoped_lock locker {_breakpointsLocker};

    const auto it = _lineBreakpoints.find(key);

    if (it == _lineBreakpoints.end()) {
        return false;
    }

    return it->second.count(line) != 0;
}

void DebuggerEndpointServer::Impl::EmitEvent(string_view event_name, string_view body_json)
{
    FO_STACK_TRACE_ENTRY();

    const string message = MakeDebuggerEventMessage(event_name, body_json);
    SendToActiveClient(message);
}

void DebuggerEndpointServer::Impl::Stop() noexcept
{
    FO_STACK_TRACE_ENTRY();

    if (_stopped.exchange(true)) {
        return;
    }

    _paused = false;
    _pauseStartPending = false;
    _singleStepRequested = false;

    {
        std::scoped_lock locker {_clientIoLocker};
        _activeClientSock.close();
    }

    _tcpServer.close();
    _discoverySocket.close();

    if (_thread.joinable()) {
        _thread.join();
    }
    if (_discoveryThread.joinable()) {
        _discoveryThread.join();
    }
}

auto DebuggerEndpointServer::Impl::MakeAttachHandshakeMessage() const -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    return strex("{{\"type\":\"attachAccepted\",\"protocolVersion\":1,\"enginePid\":{}}}\n", Platform::GetCurrentProcessIdStr()).str();
}

auto DebuggerEndpointServer::Impl::ExtractRequestId(string_view message) const -> optional<int32>
{
    FO_STACK_TRACE_ENTRY();

    try {
        const auto msg_json = nlohmann::json::parse(string {message});

        if (msg_json.contains("id") && msg_json["id"].is_number_integer()) {
            return msg_json["id"].get<int32>();
        }
    }
    catch (const std::exception& ex) {
        ReportExceptionAndContinue(ex);
    }

    return std::nullopt;
}

auto DebuggerEndpointServer::Impl::ExtractRequestCommand(string_view message) const -> string
{
    FO_STACK_TRACE_ENTRY();

    try {
        const auto msg_json = nlohmann::json::parse(string {message});

        if (msg_json.contains("command") && msg_json["command"].is_string()) {
            return msg_json["command"].get<string>();
        }
    }
    catch (const std::exception& ex) {
        ReportExceptionAndContinue(ex);
    }

    return {};
}

auto DebuggerEndpointServer::Impl::MakeDebuggerResponse(int32 request_id, string_view command, bool success, string_view body_json) const -> string
{
    FO_STACK_TRACE_ENTRY();

    return strex("{{\"type\":\"response\",\"requestId\":{},\"success\":{},\"command\":\"{}\",\"body\":{}}}\n", request_id, success ? "true" : "false", command, body_json).str();
}

auto DebuggerEndpointServer::Impl::MakeDebuggerEventMessage(string_view event_name, string_view body_json) const -> string
{
    FO_STACK_TRACE_ENTRY();

    return strex("{{\"type\":\"event\",\"event\":\"{}\",\"body\":{}}}\n", event_name, body_json).str();
}

auto DebuggerEndpointServer::Impl::SendToActiveClient(string_view message) -> bool
{
    FO_STACK_TRACE_ENTRY();

    std::shared_lock locker {_clientIoLocker};

    if (!_activeClientSock.can_write(ANGELSCRIPT_DEBUGGER_IO_POLL_TIMEOUT)) {
        return false;
    }

    const auto bytes = span<const uint8>(reinterpret_cast<const uint8*>(message.data()), message.size());
    return _activeClientSock.send(bytes) > 0;
}

auto DebuggerEndpointServer::Impl::HandleRequestLine(string_view line) -> RequestResult
{
    FO_STACK_TRACE_ENTRY();

    const int32 request_id = ExtractRequestId(line).value_or(0);
    const string command = ExtractRequestCommand(line);

    if (command == "capabilities") {
        return {
            .Response = MakeDebuggerResponse(request_id, command, true, strex(R"({{"protocolVersion":1,"supportsPause":true,"supportsContinue":true,"supportsDisconnect":true,"isPaused":{}}})", IsPaused() ? "true" : "false").str()),
        };
    }

    if (command == "pause") {
        if (IsPaused()) {
            return {
                .Response = MakeDebuggerResponse(request_id, command, true, R"({"state":"paused"})"),
            };
        }

        RequestPause();

        return {
            .Response = MakeDebuggerResponse(request_id, command, true, R"({"state":"pausing"})"),
        };
    }

    if (command == "continue") {
        _pauseStartPending = false;
        _paused = false;

        return {
            .Response = MakeDebuggerResponse(request_id, command, true, R"({"state":"running"})"),
            .Event = MakeDebuggerEventMessage("resumed", R"({"reason":"user"})"),
        };
    }

    if (command == "next" || command == "stepIn" || command == "stepOut") {
        auto mode = DebuggerStepMode::In;

        if (command == "next") {
            mode = DebuggerStepMode::Over;
        }
        else if (command == "stepOut") {
            mode = DebuggerStepMode::Out;
        }

        RequestSingleStep(mode);

        return {
            .Response = MakeDebuggerResponse(request_id, command, true, R"({"state":"running"})"),
            .Event = MakeDebuggerEventMessage("resumed", strex(R"({{"reason":"{}"}})", command).str()),
        };
    }

    if (command == "setBreakpoints") {
        try {
            const auto msg_json = nlohmann::json::parse(string {line});
            const auto payload = msg_json.contains("payload") ? msg_json["payload"] : nlohmann::json::object();
            const string source_path = payload.contains("source") && payload["source"].is_string() ? payload["source"].get<string>() : string {};

            vector<uint32> lines;

            if (payload.contains("lines") && payload["lines"].is_array()) {
                for (const auto& value : payload["lines"]) {
                    if (value.is_number_unsigned()) {
                        lines.emplace_back(value.get<uint32>());
                    }
                    else if (value.is_number_integer()) {
                        const int32 line_num = value.get<int32>();

                        if (line_num >= 0) {
                            lines.emplace_back(numeric_cast<uint32>(line_num));
                        }
                    }
                }
            }

            SetBreakpoints(source_path, lines);

            nlohmann::json body;
            body["breakpoints"] = nlohmann::json::array();

            for (const uint32 line_num : lines) {
                body["breakpoints"].push_back({{"verified", true}, {"line", line_num}});
            }

            return {
                .Response = MakeDebuggerResponse(request_id, command, true, body.dump()),
            };
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);

            return {
                .Response = MakeDebuggerResponse(request_id, command, false, R"({"error":"Invalid setBreakpoints payload"})"),
            };
        }
    }

    if (command == "stackTrace") {
        try {
            const auto msg_json = nlohmann::json::parse(string {line});
            const auto payload = msg_json.contains("payload") ? msg_json["payload"] : nlohmann::json::object();

            int32 start_frame = 0;
            int32 levels = numeric_cast<int32>(STACK_TRACE_MAX_SIZE);

            if (payload.contains("startFrame") && payload["startFrame"].is_number_integer()) {
                start_frame = std::max(0, payload["startFrame"].get<int32>());
            }

            if (payload.contains("levels") && payload["levels"].is_number_integer()) {
                levels = std::max(0, payload["levels"].get<int32>());
            }

            StackTraceData frames;
            bool has_stopped_location = false;
            string stopped_source;
            string stopped_function;
            uint32 stopped_line {};

            {
                std::scoped_lock locker {_stackTraceLocker};

                frames = _lastStackTrace;
                has_stopped_location = _hasLastStoppedLocation;
                stopped_source = _lastStoppedSourcePath;
                stopped_line = _lastStoppedSourceLine;
                stopped_function = _lastStoppedFunction;
            }

            const int32 raw_frames = numeric_cast<int32>(std::min(frames.CallsCount, STACK_TRACE_MAX_SIZE + 1));
            const int32 total_frames = raw_frames + (has_stopped_location ? 1 : 0);
            const int32 from = std::min(start_frame, total_frames);
            const int32 to = std::min(total_frames, from + levels);

            nlohmann::json body;
            body["stackFrames"] = nlohmann::json::array();
            body["totalFrames"] = total_frames;

            for (int32 i = from; i < to; i++) {
                if (has_stopped_location && i == 0) {
                    nlohmann::json frame;
                    frame["id"] = i + 1;
                    frame["name"] = !stopped_function.empty() ? stopped_function : string {"frame"};
                    frame["source"] = stopped_source;
                    frame["line"] = stopped_line;
                    body["stackFrames"].push_back(std::move(frame));
                    continue;
                }

                const int32 logical_raw_index = has_stopped_location ? (i - 1) : i;
                const int32 entry_index = raw_frames - 1 - logical_raw_index;
                const auto* entry = entry_index >= 0 ? frames.CallTree[numeric_cast<size_t>(entry_index)] : nullptr;

                if (entry == nullptr) {
                    continue;
                }

                nlohmann::json frame;
                frame["id"] = i + 1;
                frame["name"] = entry->function != nullptr ? string {entry->function} : string {"frame"};
                frame["source"] = entry->file != nullptr ? string {entry->file} : string {};
                frame["line"] = entry->line > 0 ? numeric_cast<uint32>(entry->line - 1) : 0u;
                body["stackFrames"].push_back(std::move(frame));
            }

            return {
                .Response = MakeDebuggerResponse(request_id, command, true, body.dump()),
            };
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);

            return {
                .Response = MakeDebuggerResponse(request_id, command, false, R"({"error":"Invalid stackTrace payload"})"),
            };
        }
    }

    if (command == "variables") {
        try {
            const auto msg_json = nlohmann::json::parse(string {line});
            const auto payload = msg_json.contains("payload") ? msg_json["payload"] : nlohmann::json::object();

            int32 frame_id = 1;

            if (payload.contains("frameId") && payload["frameId"].is_number_integer()) {
                frame_id = std::max(1, payload["frameId"].get<int32>());
            }

            int32 stack_level = frame_id - 1;
            vector<LocalVariableInfo> frame_vars;

            {
                std::scoped_lock locker {_stackTraceLocker};

                const int32 frames_count = numeric_cast<int32>(_lastLocalsByFrame.size());

                if (frames_count <= 0) {
                    return {
                        .Response = MakeDebuggerResponse(request_id, command, true, R"({"variables":[]})"),
                    };
                }

                if (_hasLastStoppedLocation && stack_level > 0) {
                    stack_level -= 1;
                }

                stack_level = std::clamp(stack_level, 0, frames_count - 1);
                frame_vars = _lastLocalsByFrame[numeric_cast<size_t>(stack_level)];
            }

            nlohmann::json body;
            body["variables"] = nlohmann::json::array();

            for (const auto& var_info : frame_vars) {
                nlohmann::json var;
                var["name"] = var_info.Name;
                var["type"] = var_info.Type;
                var["value"] = var_info.Value;
                body["variables"].push_back(std::move(var));
            }

            return {
                .Response = MakeDebuggerResponse(request_id, command, true, body.dump()),
            };
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);

            return {
                .Response = MakeDebuggerResponse(request_id, command, false, R"({"error":"Invalid variables payload"})"),
            };
        }
    }

    if (command == "disconnect") {
        return {
            .Response = MakeDebuggerResponse(request_id, command, true, "{}"),
            .Disconnect = true,
        };
    }

    return {
        .Response = MakeDebuggerResponse(request_id, command.empty() ? "unknown" : command, false, R"({"error":"Unknown command"})"),
    };
}

void DebuggerEndpointServer::Impl::Run()
{
    FO_STACK_TRACE_ENTRY();

    RunTcp();
}

void DebuggerEndpointServer::Impl::RunTcp()
{
    FO_STACK_TRACE_ENTRY();

    WriteLog("AngelScript debugger TCP endpoint: {}", _endpoint);

    while (!_stopped) {
        if (!_tcpServer.can_accept(ANGELSCRIPT_DEBUGGER_IO_POLL_TIMEOUT)) {
            continue;
        }

        tcp_socket client_sock = _tcpServer.accept();

        if (!client_sock.is_valid()) {
            continue;
        }

        HandleTcpClient(std::move(client_sock));
    }
}

void DebuggerEndpointServer::Impl::HandleTcpClient(tcp_socket client_sock)
{
    FO_STACK_TRACE_ENTRY();

    constexpr size_t buffer_size = 2048;
    array<uint8, buffer_size> read_buf {};
    string pending;
    bool handshake_sent = false;

    {
        std::unique_lock locker {_clientIoLocker};
        _activeClientSock = std::move(client_sock);
        _clientConnected = _activeClientSock.is_valid();
    }

    const auto clear_active_client = scope_exit([&]() noexcept {
        std::unique_lock locker {_clientIoLocker};

        _activeClientSock.close();
        _clientConnected = false;
        _paused = false;
        _pauseStartPending = false;
        _singleStepRequested = false;
        _pausedContext = nullptr;
        _pausedRootContext = nullptr;
    });

    while (!_stopped) {
        int32 read_size {};

        {
            std::shared_lock locker {_clientIoLocker};

            if (!_activeClientSock.is_valid()) {
                break;
            }

            if (!_activeClientSock.can_read(ANGELSCRIPT_DEBUGGER_IO_POLL_TIMEOUT)) {
                continue;
            }

            read_size = _activeClientSock.receive(span<uint8>(read_buf.data(), buffer_size - 1));
        }

        if (read_size <= 0) {
            break;
        }

        pending.append(reinterpret_cast<const char*>(read_buf.data()), reinterpret_cast<const char*>(read_buf.data()) + read_size);

        for (;;) {
            const size_t line_end = pending.find('\n');

            if (line_end == string::npos) {
                break;
            }

            string line = pending.substr(0, line_end);
            pending.erase(0, line_end + 1);

            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            if (line.empty()) {
                continue;
            }

            if (!handshake_sent) {
                const string hello = MakeAttachHandshakeMessage();

                if (!SendToActiveClient(hello)) {
                    WriteLog("Can't write debugger handshake to tcp '{}:{}'", _host, _port);
                    return;
                }

                handshake_sent = true;
                continue;
            }

            const auto result = HandleRequestLine(line);

            if (!SendToActiveClient(result.Response)) {
                WriteLog("Can't write debugger response to tcp '{}:{}'", _host, _port);
                return;
            }

            if (result.Event.has_value()) {
                if (!SendToActiveClient(result.Event.value())) {
                    WriteLog("Can't write debugger event to tcp '{}:{}'", _host, _port);
                    return;
                }
            }

            if (result.Disconnect) {
                return;
            }
        }
    }
}

void DebuggerEndpointServer::Impl::RunDiscoveryResponder()
{
    FO_STACK_TRACE_ENTRY();

    constexpr size_t buffer_size = 1024;
    array<uint8, buffer_size> read_buf {};

    WriteLog("AngelScript debugger UDP discovery port: {}", _discoveryPort);

    if (!_discoverySocket.bind(_bindHost, _discoveryPort, true) || !_discoverySocket.set_broadcast(true)) {
        WriteLog("Can't bind debugger discovery udp socket on port {}", _discoveryPort);
        return;
    }

    while (!_stopped) {
        if (!_discoverySocket.can_read(ANGELSCRIPT_DEBUGGER_IO_POLL_TIMEOUT)) {
            continue;
        }

        string remote_host;
        uint16 remote_port = 0;
        const int32 read_size = _discoverySocket.receive_from(span<uint8>(read_buf.data(), buffer_size - 1), remote_host, remote_port);

        if (read_size <= 0) {
            continue;
        }

        const string_view request = string_view(reinterpret_cast<const char*>(read_buf.data()), numeric_cast<size_t>(read_size));

        if (!strvex(request).starts_with(ANGELSCRIPT_DEBUGGER_DISCOVERY_PROBE)) {
            continue;
        }

        const string response = strex("{{\"type\":\"discovery\",\"processId\":\"{}\",\"endpoint\":\"{}\",\"targetName\":\"{}\",\"protocolVersion\":1}}\n", _instanceId, _endpoint, _targetName).str();
        const auto bytes = span<const uint8>(reinterpret_cast<const uint8*>(response.data()), response.size());
        _discoverySocket.send_to(remote_host, remote_port, bytes);
    }
}

DebuggerEndpointServer::DebuggerEndpointServer(const AngelScriptBackend* backend) :
    _impl {SafeAlloc::MakeUnique<Impl>(backend, this)}
{
    FO_STACK_TRACE_ENTRY();
}

DebuggerEndpointServer::~DebuggerEndpointServer()
{
    FO_STACK_TRACE_ENTRY();

    Stop();
}

auto DebuggerEndpointServer::IsPaused() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _impl->IsPaused();
}

void DebuggerEndpointServer::SetupContext(AngelScript::asIScriptContext* ctx, AngelScriptContextSetupReason reason)
{
    FO_NO_STACK_TRACE_ENTRY();

    _impl->SetupContext(ctx, reason);
}

void DebuggerEndpointServer::EmitEvent(string_view event_name, string_view body_json)
{
    FO_NO_STACK_TRACE_ENTRY();

    _impl->EmitEvent(event_name, body_json);
}

void DebuggerEndpointServer::Stop()
{
    FO_NO_STACK_TRACE_ENTRY();

    _impl->Stop();
}

void DebuggerEndpointServer::AngelScriptLine(AngelScript::asIScriptContext* ctx, void* param)
{
    FO_NO_STACK_TRACE_ENTRY();

    auto* debugger = cast_from_void<DebuggerEndpointServer*>(param);
    FO_RUNTIME_ASSERT(debugger);
    debugger->_impl->ProcessLine(ctx);
}

FO_END_NAMESPACE

#endif
