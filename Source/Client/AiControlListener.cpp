// FOnline Engine
// SPDX-License-Identifier: MIT

#include "AiControlListener.h"

#if FO_HAVE_AI_CONTROL

FO_DISABLE_WARNINGS_PUSH()
#include <json.hpp>
FO_DISABLE_WARNINGS_POP()

FO_BEGIN_NAMESPACE

static constexpr timespan AI_CONTROL_POLL_TIMEOUT {std::chrono::milliseconds(50)};
static constexpr timespan AI_CONTROL_SEND_TIMEOUT {std::chrono::seconds(10)};
static constexpr int32_t AI_CONTROL_READ_CHUNK {4096};
static constexpr int32_t AI_CONTROL_ENVELOPE_RESERVE {1024};
static constexpr int32_t AI_CONTROL_MAX_EVENT_BYTES {64 * 1024};
static constexpr int32_t AI_CONTROL_MAX_MESSAGE_BYTES {4096};
static constexpr int32_t AI_CONTROL_MAX_ID_BYTES {128};
static constexpr int32_t AI_CONTROL_DEFAULT_EVENT_LIMIT {100};
static constexpr int32_t AI_CONTROL_MAX_EVENT_LIMIT {500};

static constexpr int32_t AI_CONTROL_ERROR_PARSE {-32700};
static constexpr int32_t AI_CONTROL_ERROR_INVALID_REQUEST {-32600};
static constexpr int32_t AI_CONTROL_ERROR_METHOD_NOT_FOUND {-32601};
static constexpr int32_t AI_CONTROL_ERROR_INVALID_PARAMS {-32602};
static constexpr int32_t AI_CONTROL_ERROR_UNAUTHORIZED {-32001};
static constexpr int32_t AI_CONTROL_ERROR_QUEUE_FULL {-32002};

static auto DumpJson(const nlohmann::json& value) -> string;
static auto MakeResult(const nlohmann::json& id, string_view result_json) -> string;
static auto MakeError(const nlohmann::json& id, int32_t code, string_view message) -> string;
static auto CompactJsonObject(string_view json, int32_t max_bytes) -> string;
static auto IsValidRequestId(const nlohmann::json& id) -> bool;
static auto IsLoopbackAddress(uint32_t addr_net_order) -> bool;
static auto TokensEqual(string_view left, string_view right) noexcept -> bool;
static auto ReadCommandFields(const nlohmann::json& params, AiControlCommand& command) -> bool;
static auto ReadIdent(const nlohmann::json& params, const std::string& name, ident_t& value) -> bool;
static auto ReadInt32(const nlohmann::json& params, const std::string& name, int32_t& value) -> bool;
static auto ReadString(const nlohmann::json& params, const std::string& name, string& value) -> bool;
static auto ReadBool(const nlohmann::json& params, const std::string& name, bool& value) -> bool;
static auto ReadEventCursor(const nlohmann::json& params, uint64_t& after_seq, int32_t& limit) -> bool;
static auto MakeLogExceptionEvent(logging::type type, string_view message, bool has_stack_trace) -> optional<string>;
static auto ClassifyLogException(logging::type type, string_view message) -> string_view;
static auto LogTypeToString(logging::type type) noexcept -> string_view;
static auto TrimText(string_view text) -> string_view;
static auto LimitText(string_view text, int32_t max_bytes) -> string_view;
static auto ContainsCaseInsensitive(string_view text, string_view needle) noexcept -> bool;

AiControlListener::~AiControlListener()
{
    safe_call([this] { Stop(); });
}

auto AiControlListener::IsRunning() const noexcept -> bool
{
    return _running.load(std::memory_order_acquire);
}

auto AiControlListener::GetStatusJson() const -> string
{
    scoped_lock locker {_locker};

    nlohmann::json status {};
    status["running"] = _running.load(std::memory_order_acquire);
    status["host"] = std::string(_config.Host);
    status["port"] = _config.Port;
    status["queuedCommands"] = _commands.size();
    status["maxQueuedCommands"] = _config.MaxQueuedCommands;
    status["events"] = _events.size();
    status["maxEvents"] = _config.MaxEvents;
    status["observationSeq"] = _observationSeq;
    status["lastError"] = std::string(_lastError);

    return DumpJson(status);
}

void AiControlListener::RegisterCommandHandler(string_view type, AiControlCommandHandler handler)
{
    if (!handler) {
        throw AiControlException("AiControl command handler is empty", type);
    }

    string key {type};

    if (_commandHandlers.contains(key)) {
        throw AiControlException("AiControl command handler is already registered", type);
    }

    _commandHandlers.emplace(std::move(key), std::move(handler));
}

void AiControlListener::Start(const AiControlListenerConfig& config)
{
    FO_TRACE_ZONE(Network);

    if (_thread.joinable()) {
        throw AiControlException("AiControl listener is already started", _config.Host, _config.Port);
    }

    if (config.Port == 0) {
        throw AiControlException("AiControl listener port must be positive");
    }

    if (config.MaxQueuedCommands <= 0 || config.MaxEvents <= 0) {
        throw AiControlException("AiControl listener queue and event limits must be positive", config.MaxQueuedCommands, config.MaxEvents);
    }

    string host {config.Host.empty() ? string {"127.0.0.1"} : config.Host};

    if (!net_sockets::startup()) {
        throw AiControlException("AiControl listener cannot initialize sockets", net_sockets::last_error_text());
    }

    optional<uint32_t> address {net_sockets::resolve_ipv4(host)};

    if (!address.has_value()) {
        throw AiControlException("AiControl listener cannot resolve the host", host);
    }

    // An empty token authorizes every peer, so beyond loopback it would be an open remote-control channel
    if (config.Token.empty() && !IsLoopbackAddress(*address)) {
        throw AiControlException("AiControl listener on a non-loopback address requires a non-empty token", host);
    }

    string bind_address {net_sockets::ipv4_to_string(*address)};

    if (!_server.listen(bind_address, config.Port, 1, true)) {
        throw AiControlException("AiControl listener cannot listen", bind_address, config.Port, net_sockets::last_error_text());
    }

    _config = config;
    _config.Host = std::move(host);

    {
        scoped_lock locker {_locker};
        _lastError.clear();
    }

    _stopRequested.store(false, std::memory_order_release);
    _running.store(true, std::memory_order_release);

    RegisterLogCallback();
    _thread = run_thread("AiControl", [this] { RunConnections(); });
}

void AiControlListener::DispatchCommands()
{
    AiControlCommand command {};
    nptr<AiControlCommandHandler> handler {};

    while (TakeNextCommand(command, handler, false)) {
        if (handler) {
            RunCommandHandler(*handler, command);
        }
    }
}

auto AiControlListener::TryPullCommand(AiControlCommand& command) -> bool
{
    AiControlCommand next {};
    nptr<AiControlCommandHandler> handler {};

    while (TakeNextCommand(next, handler, true)) {
        if (!handler) {
            command = std::move(next);
            return true;
        }

        RunCommandHandler(*handler, next);
    }

    return false;
}

void AiControlListener::SetObservation(string_view observation_json)
{
    string observation {CompactJsonObject(observation_json, MAX_LINE_BYTES - AI_CONTROL_ENVELOPE_RESERVE)};

    scoped_lock locker {_locker};

    _observationSeq++;
    _observationJson = std::move(observation);
}

void AiControlListener::PushEvent(string_view event_json)
{
    AppendEvent(CompactJsonObject(event_json, AI_CONTROL_MAX_EVENT_BYTES));
}

void AiControlListener::CompleteCommand(uint32_t command_seq, bool success, string_view message)
{
    nlohmann::json event {};
    event["type"] = "command_completed";
    event["commandSeq"] = command_seq;
    event["success"] = success;
    event["message"] = std::string(LimitText(message, AI_CONTROL_MAX_MESSAGE_BYTES));

    AppendEvent(DumpJson(event));
}

void AiControlListener::Stop()
{
    FO_TRACE_ZONE(Network);

    _stopRequested.store(true, std::memory_order_release);
    UnregisterLogCallback();

    if (_thread.joinable()) {
        _thread.join();
    }

    _server.close();
    _running.store(false, std::memory_order_release);

    FailQueuedCommands("AiControl listener stopped");
}

void AiControlListener::RunConnections()
{
    try {
        tcp_socket connection {};
        string input {};
        bool authorized {};

        while (!_stopRequested.load(std::memory_order_acquire)) {
            if (!connection.is_valid()) {
                if (!_server.can_accept(AI_CONTROL_POLL_TIMEOUT)) {
                    continue;
                }

                connection = _server.accept();

                if (!connection.is_valid() || !connection.set_nonblocking(true)) {
                    SetLastError(strex("AiControl accept failed: {}", net_sockets::last_error_text()));
                    connection.close();
                    continue;
                }

                input.clear();
                authorized = _config.Token.empty();
                continue;
            }

            if (!connection.can_read(AI_CONTROL_POLL_TIMEOUT)) {
                continue;
            }

            array<uint8_t, AI_CONTROL_READ_CHUNK> buffer {};
            int32_t received {connection.receive(span<uint8_t> {buffer.data(), buffer.size()})};

            if (received == 0) {
                connection.close();
                continue;
            }

            if (received < 0) {
                if (net_sockets::last_recv_was_would_block()) {
                    continue;
                }

                SetLastError(strex("AiControl read failed: {}", net_sockets::last_error_text()));
                connection.close();
                continue;
            }

            input.append(span_to_string(const_span<uint8_t> {buffer.data(), numeric_cast<size_t>(received)}));
            ProcessInput(connection, input, authorized);
        }
    }
    catch (const std::exception& ex) {
        SetLastError(ex.what());
        _running.store(false, std::memory_order_release);
        exceptions::report_and_continue(ex);
        return;
    }

    _running.store(false, std::memory_order_release);
}

void AiControlListener::ProcessInput(tcp_socket& connection, string& input, bool& authorized)
{
    size_t max_line_bytes {numeric_cast<size_t>(MAX_LINE_BYTES)};

    auto reject_oversized {[&] {
        SetLastError(strex("AiControl request exceeded {} bytes; connection closed", MAX_LINE_BYTES));
        (void)SendResponse(connection, MakeError(nullptr, AI_CONTROL_ERROR_INVALID_REQUEST, "Request too large"));
        connection.close();
        input.clear();
    }};

    while (connection.is_valid()) {
        size_t line_end {input.find('\n')};

        if (line_end == string::npos) {
            break;
        }

        if (line_end > max_line_bytes) {
            reject_oversized();
            return;
        }

        string line {input.substr(0, line_end)};
        input.erase(0, line_end + 1);

        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        string response {HandleLine(line, authorized)};

        if (!SendResponse(connection, response)) {
            connection.close();
        }
    }

    if (connection.is_valid() && input.size() > max_line_bytes) {
        reject_oversized();
    }
}

auto AiControlListener::HandleLine(string_view line, bool& authorized) -> string
{
    FO_TRACE_ZONE(Network);

    nlohmann::json request {};
    request = nlohmann::json::parse(line.begin(), line.end(), nullptr, false);

    if (request.is_discarded()) {
        return MakeError(nullptr, AI_CONTROL_ERROR_PARSE, "Parse error");
    }

    if (!request.is_object()) {
        return MakeError(nullptr, AI_CONTROL_ERROR_INVALID_REQUEST, "Invalid request");
    }

    nlohmann::json id {};

    if (auto id_it {request.find("id")}; id_it != request.end()) {
        id = *id_it;
    }

    if (!IsValidRequestId(id)) {
        return MakeError(nullptr, AI_CONTROL_ERROR_INVALID_REQUEST, "Invalid request");
    }

    auto jsonrpc_it {request.find("jsonrpc")};
    auto method_it {request.find("method")};
    auto params_it {request.find("params")};
    bool valid_jsonrpc {jsonrpc_it != request.end() && jsonrpc_it->is_string() && jsonrpc_it->get<std::string>() == "2.0"};
    bool valid_method {method_it != request.end() && method_it->is_string() && !method_it->get<std::string>().empty()};
    bool valid_params {params_it == request.end() || params_it->is_object()};

    if (!valid_jsonrpc || !valid_method || !valid_params) {
        return MakeError(id, AI_CONTROL_ERROR_INVALID_REQUEST, "Invalid request");
    }

    nlohmann::json params {};
    params = params_it != request.end() ? *params_it : nlohmann::json::object();
    std::string method {method_it->get<std::string>()};

    if (method == "auth") {
        auto token_it {params.find("token")};
        bool token_matches {token_it != params.end() && token_it->is_string() && TokensEqual(token_it->get<std::string>(), _config.Token)};
        authorized = _config.Token.empty() || token_matches;
        return MakeResult(id, authorized ? R"({"authorized":true})" : R"({"authorized":false})");
    }

    if (!authorized) {
        return MakeError(id, AI_CONTROL_ERROR_UNAUTHORIZED, "Unauthorized");
    }

    if (method == "ping") {
        return MakeResult(id, R"({"ok":true})");
    }

    if (method == "status") {
        return MakeResult(id, GetStatusJson());
    }

    if (method == "observe") {
        string observation {};
        uint64_t observation_seq {};

        {
            scoped_lock locker {_locker};
            observation = _observationJson;
            observation_seq = _observationSeq;
        }

        return MakeResult(id, strex(R"({{"observationSeq":{},"observation":{}}})", observation_seq, observation).str());
    }

    if (method == "events") {
        uint64_t after_seq {};
        int32_t limit {AI_CONTROL_DEFAULT_EVENT_LIMIT};

        if (!ReadEventCursor(params, after_seq, limit)) {
            return MakeError(id, AI_CONTROL_ERROR_INVALID_PARAMS, "Invalid event cursor");
        }

        return MakeResult(id, BuildEventsJson(after_seq, limit));
    }

    if (method == "act") {
        auto type_it {params.find("type")};

        if (type_it == params.end() || !type_it->is_string() || type_it->get<std::string>().empty()) {
            return MakeError(id, AI_CONTROL_ERROR_INVALID_PARAMS, "Missing command type");
        }

        AiControlCommand command {};
        command.Type = string {type_it->get<std::string>()};

        if (!ReadCommandFields(params, command)) {
            return MakeError(id, AI_CONTROL_ERROR_INVALID_PARAMS, "Invalid command field");
        }

        command.ParamsJson = DumpJson(params);

        optional<uint32_t> command_seq {EnqueueCommand(std::move(command))};

        if (!command_seq.has_value()) {
            return MakeError(id, AI_CONTROL_ERROR_QUEUE_FULL, "Command queue full");
        }

        return MakeResult(id, strex(R"({{"accepted":true,"commandSeq":{}}})", *command_seq).str());
    }

    return MakeError(id, AI_CONTROL_ERROR_METHOD_NOT_FOUND, "Method not found");
}

auto AiControlListener::EnqueueCommand(AiControlCommand command) -> optional<uint32_t>
{
    scoped_lock locker {_locker};

    if (numeric_cast<int32_t>(_commands.size()) >= _config.MaxQueuedCommands) {
        return std::nullopt;
    }

    _nextCommandSeq++;
    command.Seq = _nextCommandSeq;
    _commands.emplace_back(std::move(command));

    return _nextCommandSeq;
}

auto AiControlListener::BuildEventsJson(uint64_t after_seq, int32_t limit) const -> string
{
    size_t max_result_bytes {numeric_cast<size_t>(MAX_LINE_BYTES - AI_CONTROL_ENVELOPE_RESERVE)};

    scoped_lock locker {_locker};

    string result {strex(R"({{"latestSeq":{},"events":[)", _nextEventSeq).str()};
    int32_t count {};

    for (const QueuedEvent& event : _events) {
        if (event.Seq <= after_seq) {
            continue;
        }

        string item {strex(R"({}{{"seq":{},"event":{}}})", count != 0 ? "," : "", event.Seq, event.Json).str()};

        // A partial page keeps the response inside the line limit; the client continues from the last returned seq
        if (result.size() + item.size() + 2 > max_result_bytes) {
            break;
        }

        result += item;
        count++;

        if (count == limit) {
            break;
        }
    }

    result += "]}";

    return result;
}

auto AiControlListener::SendResponse(tcp_socket& connection, string_view response) -> bool
{
    string line {response};
    line += '\n';

    const_span<uint8_t> remaining {make_const_span(line.data(), line.size())};
    nanotime deadline {nanotime::now() + AI_CONTROL_SEND_TIMEOUT};

    while (!remaining.empty()) {
        if (_stopRequested.load(std::memory_order_acquire)) {
            return false;
        }

        if (nanotime::now() >= deadline) {
            SetLastError("AiControl response was not consumed before the send timeout; connection closed");
            return false;
        }

        int32_t sent {connection.send(remaining)};

        if (sent > 0) {
            remaining = remaining.subspan(numeric_cast<size_t>(sent));
            continue;
        }

        if (sent < 0 && net_sockets::last_recv_was_would_block()) {
            (void)connection.can_write(AI_CONTROL_POLL_TIMEOUT);
            continue;
        }

        SetLastError(strex("AiControl write failed: {}", net_sockets::last_error_text()));
        return false;
    }

    return true;
}

auto AiControlListener::TakeNextCommand(AiControlCommand& command, nptr<AiControlCommandHandler>& handler, bool take_unhandled) -> bool
{
    scoped_lock locker {_locker};

    if (_commands.empty()) {
        return false;
    }

    auto handler_it {_commandHandlers.find(_commands.front().Type)};

    if (handler_it == _commandHandlers.end()) {
        handler_it = _commandHandlers.find(string {});
    }

    handler = handler_it != _commandHandlers.end() ? &handler_it->second : nullptr;

    if (!handler && !take_unhandled) {
        return false;
    }

    command = std::move(_commands.front());
    _commands.pop_front();

    return true;
}

void AiControlListener::RunCommandHandler(AiControlCommandHandler& handler, const AiControlCommand& command)
{
    AiControlCommandResult result {};

    try {
        result = handler(command);
    }
    catch (const std::exception& ex) {
        // The peer receives the terminal event before the failure reaches the client loop
        CompleteCommand(command.Seq, false, ex.what());
        throw;
    }

    CompleteCommand(command.Seq, result.Success, result.Message);
}

void AiControlListener::AppendEvent(string event_json)
{
    scoped_lock locker {_locker};

    _nextEventSeq++;
    _events.emplace_back(QueuedEvent {.Seq = _nextEventSeq, .Json = std::move(event_json)});

    while (numeric_cast<int32_t>(_events.size()) > _config.MaxEvents) {
        _events.pop_front();
    }
}

void AiControlListener::FailQueuedCommands(string_view message)
{
    deque<AiControlCommand> commands {};

    {
        scoped_lock locker {_locker};
        commands.swap(_commands);
    }

    for (const AiControlCommand& command : commands) {
        CompleteCommand(command.Seq, false, message);
    }
}

void AiControlListener::SetLastError(string_view error)
{
    scoped_lock locker {_locker};

    _lastError = string {LimitText(error, AI_CONTROL_MAX_MESSAGE_BYTES)};
}

void AiControlListener::RegisterLogCallback()
{
    // One listener owns a port at a time, so the port keeps the key unique per running listener
    _logCallbackKey = strex("AiControlListener.{}", _config.Port).str();

    logging::set_callback(_logCallbackKey, [this](logging::type type, string_view message, nptr<const stack_trace::catched_data> st) {
        if (_stopRequested.load(std::memory_order_acquire)) {
            return;
        }

        optional<string> event {MakeLogExceptionEvent(type, message, !!st)};

        if (event.has_value()) {
            AppendEvent(std::move(*event));
        }
    });
}

void AiControlListener::UnregisterLogCallback() noexcept
{
    if (_logCallbackKey.empty()) {
        return;
    }

    safe_call([this] { logging::set_callback(_logCallbackKey, {}); });
    _logCallbackKey.clear();
}

static auto DumpJson(const nlohmann::json& value) -> string
{
    // Project and log text is not guaranteed to be valid UTF-8, and a strict dump would throw on the socket thread
    return string {value.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace)};
}

static auto MakeResult(const nlohmann::json& id, string_view result_json) -> string
{
    return strex(R"({{"jsonrpc":"2.0","id":{},"result":{}}})", DumpJson(id), result_json).str();
}

static auto MakeError(const nlohmann::json& id, int32_t code, string_view message) -> string
{
    nlohmann::json response {};
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    response["error"] = {{"code", code}, {"message", std::string(message)}};

    return DumpJson(response);
}

static auto CompactJsonObject(string_view json, int32_t max_bytes) -> string
{
    if (!nlohmann::json::accept(json.begin(), json.end())) {
        throw AiControlException("AiControl payload is not valid JSON");
    }

    size_t begin {json.find_first_not_of(" \t\r\n")};

    if (begin == string_view::npos || json[begin] != '{') {
        throw AiControlException("AiControl payload must be a JSON object");
    }

    if (json.size() > numeric_cast<size_t>(max_bytes)) {
        throw AiControlException("AiControl payload exceeds the size limit", json.size(), max_bytes);
    }

    // Raw line breaks in valid JSON are only whitespace between tokens, and they would split the line framing
    string compact {json};

    for (char& ch : compact) {
        if (ch == '\n' || ch == '\r') {
            ch = ' ';
        }
    }

    return compact;
}

static auto IsValidRequestId(const nlohmann::json& id) -> bool
{
    if (id.is_null() || id.is_number_integer()) {
        return true;
    }

    return id.is_string() && id.get<std::string>().size() <= numeric_cast<size_t>(AI_CONTROL_MAX_ID_BYTES);
}

static auto IsLoopbackAddress(uint32_t addr_net_order) -> bool
{
    string text {net_sockets::ipv4_to_string(addr_net_order)};

    return string_view {text}.starts_with("127.");
}

static auto TokensEqual(string_view left, string_view right) noexcept -> bool
{
    // Every byte is compared so the reply time does not reveal how much of the token matched
    size_t length {std::max(left.size(), right.size())};
    uint32_t difference {left.size() == right.size() ? 0u : 1u};

    for (size_t i {}; i < length; i++) {
        uint8_t left_byte {i < left.size() ? std::bit_cast<uint8_t>(left[i]) : uint8_t {}};
        uint8_t right_byte {i < right.size() ? std::bit_cast<uint8_t>(right[i]) : uint8_t {}};
        difference |= numeric_cast<uint32_t>(left_byte ^ right_byte);
    }

    return difference == 0;
}

static auto ReadCommandFields(const nlohmann::json& params, AiControlCommand& command) -> bool
{
    return ReadIdent(params, "targetId", command.TargetId) && //
        ReadIdent(params, "itemId", command.ItemId) && //
        ReadIdent(params, "auxId", command.AuxId) && //
        ReadInt32(params, "x", command.X) && //
        ReadInt32(params, "y", command.Y) && //
        ReadInt32(params, "screenX", command.ScreenX) && //
        ReadInt32(params, "screenY", command.ScreenY) && //
        ReadInt32(params, "intArg", command.IntArg) && //
        ReadString(params, "stringArg", command.StringArg) && //
        ReadBool(params, "append", command.Append);
}

static auto ReadIdent(const nlohmann::json& params, const std::string& name, ident_t& value) -> bool
{
    auto it {params.find(name)};

    if (it == params.end() || it->is_null()) {
        return true;
    }

    if (it->is_number_integer()) {
        if (it->is_number_unsigned() && it->get<uint64_t>() > numeric_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
            return false;
        }

        value = ident_t {it->get<int64_t>()};
        return true;
    }

    // Decimal strings carry 64-bit identifiers past the precision of JSON number parsers
    if (it->is_string()) {
        std::string text {it->get<std::string>()};

        if (text.empty() || text.size() > 18 || !std::ranges::all_of(text, [](char ch) { return ch >= '0' && ch <= '9'; })) {
            return false;
        }

        value = ident_t {strvex(text).to_int64()};
        return true;
    }

    return false;
}

static auto ReadInt32(const nlohmann::json& params, const std::string& name, int32_t& value) -> bool
{
    auto it {params.find(name)};

    if (it == params.end() || it->is_null()) {
        return true;
    }

    if (!it->is_number_integer() || it->is_number_unsigned() && it->get<uint64_t>() > numeric_cast<uint64_t>(std::numeric_limits<int32_t>::max())) {
        return false;
    }

    int64_t number {it->get<int64_t>()};

    if (number < std::numeric_limits<int32_t>::min() || number > std::numeric_limits<int32_t>::max()) {
        return false;
    }

    value = numeric_cast<int32_t>(number);
    return true;
}

static auto ReadString(const nlohmann::json& params, const std::string& name, string& value) -> bool
{
    auto it {params.find(name)};

    if (it == params.end() || it->is_null()) {
        return true;
    }

    if (!it->is_string()) {
        return false;
    }

    value = string {it->get<std::string>()};
    return true;
}

static auto ReadBool(const nlohmann::json& params, const std::string& name, bool& value) -> bool
{
    auto it {params.find(name)};

    if (it == params.end() || it->is_null()) {
        return true;
    }

    if (!it->is_boolean()) {
        return false;
    }

    value = it->get<bool>();
    return true;
}

static auto ReadEventCursor(const nlohmann::json& params, uint64_t& after_seq, int32_t& limit) -> bool
{
    if (auto it {params.find("afterSeq")}; it != params.end()) {
        if (!it->is_number_integer()) {
            return false;
        }

        if (it->is_number_unsigned()) {
            after_seq = it->get<uint64_t>();
        }
        else {
            after_seq = numeric_cast<uint64_t>(std::max(it->get<int64_t>(), int64_t {}));
        }
    }

    if (auto it {params.find("limit")}; it != params.end()) {
        if (!it->is_number_integer()) {
            return false;
        }

        int64_t requested {it->is_number_unsigned() ? numeric_cast<int64_t>(std::min(it->get<uint64_t>(), uint64_t {AI_CONTROL_MAX_EVENT_LIMIT})) : it->get<int64_t>()};
        limit = numeric_cast<int32_t>(std::clamp(requested, int64_t {1}, int64_t {AI_CONTROL_MAX_EVENT_LIMIT}));
    }

    return true;
}

static auto MakeLogExceptionEvent(logging::type type, string_view message, bool has_stack_trace) -> optional<string>
{
    string_view category {ClassifyLogException(type, message)};

    if (category.empty()) {
        return std::nullopt;
    }

    nlohmann::json event {};
    event["type"] = "runtime_exception";
    event["source"] = "engine_log";
    event["level"] = std::string(LogTypeToString(type));
    event["category"] = std::string(category);
    event["message"] = std::string(LimitText(TrimText(message), AI_CONTROL_MAX_MESSAGE_BYTES));
    event["raw"] = std::string(LimitText(message, AI_CONTROL_MAX_MESSAGE_BYTES));
    event["hasStackTrace"] = has_stack_trace;

    return DumpJson(event);
}

static auto ClassifyLogException(logging::type type, string_view message) -> string_view
{
    if (ContainsCaseInsensitive(message, "ScriptException") || ContainsCaseInsensitive(message, "Script exception")) {
        return "script_exception";
    }

    if (ContainsCaseInsensitive(message, "assert") || ContainsCaseInsensitive(message, "verification")) {
        return "assertion";
    }

    if (ContainsCaseInsensitive(message, "fatal") || ContainsCaseInsensitive(message, "crash")) {
        return "fatal";
    }

    if (ContainsCaseInsensitive(message, "exception")) {
        return "exception";
    }

    if (type == logging::type::error || ContainsCaseInsensitive(message, "error :")) {
        return "error";
    }

    return {};
}

static auto LogTypeToString(logging::type type) noexcept -> string_view
{
    switch (type) {
    case logging::type::info:
        return "info";
    case logging::type::info_section:
        return "info_section";
    case logging::type::warning:
        return "warning";
    case logging::type::error:
        return "error";
    default:
        break;
    }

    return "unknown";
}

static auto TrimText(string_view text) -> string_view
{
    size_t begin {text.find_first_not_of(" \t\r\n")};

    if (begin == string_view::npos) {
        return {};
    }

    size_t end {text.find_last_not_of(" \t\r\n")};

    return text.substr(begin, end - begin + 1);
}

static auto LimitText(string_view text, int32_t max_bytes) -> string_view
{
    return text.substr(0, std::min(text.size(), numeric_cast<size_t>(max_bytes)));
}

static auto ContainsCaseInsensitive(string_view text, string_view needle) noexcept -> bool
{
    if (needle.size() > text.size()) {
        return false;
    }

    for (size_t i {}; i + needle.size() <= text.size(); i++) {
        bool match {true};

        for (size_t j {}; j < needle.size(); j++) {
            char left {text[i + j]};
            char right {needle[j]};

            if (left >= 'A' && left <= 'Z') {
                left = numeric_cast<char>(left - 'A' + 'a');
            }

            if (right >= 'A' && right <= 'Z') {
                right = numeric_cast<char>(right - 'A' + 'a');
            }

            if (left != right) {
                match = false;
                break;
            }
        }

        if (match) {
            return true;
        }
    }

    return false;
}

FO_END_NAMESPACE

#endif
