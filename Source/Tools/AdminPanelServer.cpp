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

#include "AdminPanelServer.h"

#include "Application.h"
#include "Server.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

static constexpr auto ADMIN_SNAPSHOT_INTERVAL = std::chrono::milliseconds {250};

static auto MakeAdminField(string_view value) -> string
{
    string result;
    result.reserve(value.size());

    for (const auto ch : value) {
        if (ch == '\t' || ch == '\r' || ch == '\n') {
            result += ' ';
        }
        else {
            result += ch;
        }
    }

    return result;
}

static auto SplitAdminFields(string_view value) -> vector<string_view>
{
    vector<string_view> fields;
    size_t begin = 0;

    while (begin <= value.size()) {
        const auto separator_pos = value.find('\t', begin);

        if (separator_pos == string_view::npos) {
            fields.emplace_back(value.substr(begin));
            break;
        }

        fields.emplace_back(value.substr(begin, separator_pos - begin));
        begin = separator_pos + 1;
    }

    return fields;
}

static auto ParsePositiveNumber(string_view value, size_t& result) -> bool
{
    const auto value_ex = strvex(value);

    if (!value_ex.is_number()) {
        return false;
    }

    const auto parsed = value_ex.to_int32();

    if (parsed <= 0) {
        return false;
    }

    result = numeric_cast<size_t>(parsed);
    return true;
}

static auto GetServerStateCode(AdminServerState state) -> uint32_t
{
    return static_cast<uint32_t>(state);
}

static auto FormatRemoteAddress(string_view host, uint16_t port) -> string
{
    return strex("{}:{}", host, port).str();
}

AdminServerHost::AdminServerHost(string_view log_callback_key, AdminServerHostCallbacks callbacks) :
    _logCallbackKey(log_callback_key),
    _callbacks(std::move(callbacks))
{
    ignore_unused(net_sockets::startup());
    _serverStateChangedAt = nanotime::now();

    _listenerReady = _listener.listen(GetApp()->Settings.AdminDiscoveryBindHost, numeric_cast<uint16_t>(GetApp()->Settings.AdminControlPort), 16);

    if (GetApp()->Settings.AdminDiscoveryEnabled) {
        _discoveryReady = _discoverySocket.bind(GetApp()->Settings.AdminDiscoveryBindHost, numeric_cast<uint16_t>(GetApp()->Settings.AdminDiscoveryPort), true) && _discoverySocket.set_broadcast(true);
    }

    SetLogCallback(_logCallbackKey, [this](LogType, string_view str, nptr<const CatchedStackTraceData>) FO_DEFERRED { OnLog(str); });
}

AdminServerHost::~AdminServerHost()
{
    SetLogCallback(_logCallbackKey, nullptr);
}

void AdminServerHost::OnLog(string_view text)
{
    if (text.empty()) {
        return;
    }

    auto locker = std::unique_lock {_logLocker};

    AdminLogEntry entry;

    for (const auto& line : strex(text).split('\n')) {
        if (line.empty()) {
            continue;
        }

        entry.Lines.emplace_back(line);
    }

    if (entry.Lines.empty()) {
        return;
    }

    _logLineCount += entry.Lines.size();
    _logLines.emplace_back(std::move(entry));

    while (_logLineCount > numeric_cast<size_t>(GetApp()->Settings.MaxServerLogLines) && !_logLines.empty()) {
        _logLineCount -= _logLines.front().Lines.size();
        _logLines.pop_front();
        _logOffset++;
    }
}

void AdminServerHost::QueueSessionLine(Session& session, string_view prefix, string_view text)
{
    AppendOutgoing(session, strex("{}\t{}\n", prefix, MakeAdminField(text)).str());
}

void AdminServerHost::QueueSessionLogEntry(Session& session, const AdminLogEntry& entry)
{
    string line = "log";

    for (const auto& log_line : entry.Lines) {
        line += '\t';
        line += MakeAdminField(log_line);
    }

    line += '\n';
    AppendOutgoing(session, line);
}

void AdminServerHost::AppendOutgoing(Session& session, string_view text)
{
    string encoded_text(text);
    session.OutgoingCipher.Apply(encoded_text.data(), encoded_text.size());
    session.OutgoingData += encoded_text;
}

void AdminServerHost::QueueSnapshot(Session& session)
{
    AdminSnapshot snapshot {};
    nptr<ServerEngine> server = _callbacks.GetServer ? _callbacks.GetServer() : nullptr;
    const auto state_duration_ms = GetStateDurationMs();
    const auto downtime_ms = _serverState == AdminServerState::Running ? 0 : state_duration_ms;

    if (server) {
        if (server->Lock(std::chrono::milliseconds {10})) {
            const auto server_snapshot = server->GetAdminPanelSnapshotData();
            snapshot.ServerRunning = server_snapshot.ServerRunning;
            snapshot.ServerStartingError = server_snapshot.ServerStartingError;
            snapshot.ServerVersion = server_snapshot.ServerVersion;
            snapshot.GameVersion = server_snapshot.GameVersion;
            snapshot.CompatibilityVersion = server_snapshot.CompatibilityVersion;
            snapshot.UptimeMs = server_snapshot.UptimeMs;
            snapshot.Online = server_snapshot.Online;
            snapshot.MaxOnline = server_snapshot.MaxOnline;
            snapshot.LoopsPerSecond = server_snapshot.LoopsPerSecond;
            snapshot.Players = server_snapshot.Players;
            snapshot.Critters = server_snapshot.Critters;
            snapshot.Locations = server_snapshot.Locations;
            snapshot.Maps = server_snapshot.Maps;
            snapshot.Items = server_snapshot.Items;
            snapshot.DbCommitJobs = server_snapshot.DbCommitJobs;
            snapshot.LoopAvgMs = server_snapshot.LoopAvgMs;
            snapshot.LoopMinMs = server_snapshot.LoopMinMs;
            snapshot.LoopMaxMs = server_snapshot.LoopMaxMs;
            _lastSnapshot = snapshot;
            _lastSnapshotValid = true;
            server->Unlock();
        }
        else {
            if (_lastSnapshotValid) {
                snapshot = _lastSnapshot;
            }

            snapshot.ServerRunning = server->IsStarted();
            snapshot.ServerStartingError = server->IsStartingError();
        }
    }
    else {
        _lastSnapshot = {};
        _lastSnapshotValid = false;
    }

    snapshot.Valid = true;
    snapshot.State = _serverState;
    snapshot.StateDurationMs = state_duration_ms;
    snapshot.DowntimeMs = downtime_ms;
    snapshot.IsController = session.IsController;
    snapshot.HasController = _controllerSession != nullptr;

    AppendOutgoing(session, strex("snapshot\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\n", snapshot.ServerRunning ? 1 : 0, snapshot.ServerStartingError ? 1 : 0, GetServerStateCode(_serverState), state_duration_ms, snapshot.UptimeMs, downtime_ms, snapshot.Online, snapshot.MaxOnline, snapshot.LoopsPerSecond, snapshot.Players, snapshot.Critters, snapshot.Locations, snapshot.Maps, snapshot.Items, snapshot.DbCommitJobs, snapshot.LoopAvgMs, snapshot.LoopMinMs, snapshot.LoopMaxMs, MakeAdminField(snapshot.ServerVersion), MakeAdminField(snapshot.GameVersion), MakeAdminField(snapshot.CompatibilityVersion), session.IsController ? 1 : 0, _controllerSession != nullptr ? 1 : 0).str());
}

void AdminServerHost::FlushSession(Session& session)
{
    while (session.OutgoingPos < session.OutgoingData.size()) {
        if (!session.Socket.can_write(std::chrono::milliseconds {0})) {
            return;
        }

        const auto data = span<const uint8_t>(reinterpret_cast<const uint8_t*>(session.OutgoingData.data() + session.OutgoingPos), session.OutgoingData.size() - session.OutgoingPos);
        const auto sent = session.Socket.send(data);

        if (sent <= 0) {
            session.Closed = true;
            return;
        }

        session.OutgoingPos += numeric_cast<size_t>(sent);
    }

    if (session.OutgoingPos >= session.OutgoingData.size()) {
        session.OutgoingData.clear();
        session.OutgoingPos = 0;
    }
}

void AdminServerHost::SendLogBacklog(Session& session)
{
    auto locker = std::unique_lock {_logLocker};

    for (size_t index = _logOffset; index < _logOffset + _logLines.size(); index++) {
        QueueSessionLogEntry(session, _logLines[index - _logOffset]);
    }

    session.NextLogIndex = _logOffset + _logLines.size();
}

void AdminServerHost::UpdateSessionLogs(Session& session)
{
    auto locker = std::unique_lock {_logLocker};

    if (session.NextLogIndex < _logOffset) {
        session.NextLogIndex = _logOffset;
    }

    while (session.NextLogIndex < _logOffset + _logLines.size()) {
        QueueSessionLogEntry(session, _logLines[session.NextLogIndex - _logOffset]);
        session.NextLogIndex++;
    }
}

auto AdminServerHost::GrantControl(Session& session) -> bool
{
    if (_controllerSession != nullptr && _controllerSession != &session) {
        return false;
    }

    _controllerSession = &session;
    session.IsController = true;
    return true;
}

void AdminServerHost::ReleaseControl(Session& session)
{
    if (_controllerSession == &session) {
        _controllerSession = nullptr;
    }

    session.IsController = false;
}

auto AdminServerHost::MakeSessionsStatus() -> string
{
    size_t authorized = 0;
    size_t observers = 0;
    size_t controllers = 0;
    size_t players = 0;
    nptr<ServerEngine> server = _callbacks.GetServer ? _callbacks.GetServer() : nullptr;

    if (server && server->Lock(std::chrono::milliseconds {10})) {
        players = server->EntityMngr.GetPlayersCount();
        server->Unlock();
    }

    for (auto& session : _sessions) {
        if (!session.Authorized) {
            continue;
        }

        authorized++;

        if (session.IsController) {
            controllers++;
        }
        else {
            observers++;
        }
    }

    return strex("Sessions: players={}, admin_authorized={}, admin_observers={}, admin_controllers={}, global_controller={}", players, authorized, observers, controllers, _controllerSession != nullptr ? "yes" : "no").str();
}

auto AdminServerHost::EvaluateServerState() const -> AdminServerState
{
    nptr<ServerEngine> server = _callbacks.GetServer ? _callbacks.GetServer() : nullptr;

    if (!server) {
        return AdminServerState::Stopped;
    }

    if (server->IsStarted()) {
        return AdminServerState::Running;
    }

    if (server->IsStartingError()) {
        return AdminServerState::StartingError;
    }

    return AdminServerState::Starting;
}

void AdminServerHost::UpdateServerState()
{
    const auto state = EvaluateServerState();

    if (state != _serverState) {
        _serverState = state;
        _serverStateChangedAt = nanotime::now();
    }
}

auto AdminServerHost::GetStateDurationMs() const -> int64_t
{
    return (_serverStateChangedAt != nanotime {}) ? ((nanotime::now() - _serverStateChangedAt).to_ms<int64_t>()) : 0;
}

void AdminServerHost::ExecuteCommand(Session& session, string_view command)
{
    try {
        const string cmd = strex(command).trim().str();

        if (cmd.empty()) {
            QueueSessionLine(session, "status", "Command is empty");
            return;
        }

        if (cmd == "help") {
            QueueSessionLine(session, "status", "Commands: start, stop, restart, announce <utf8 text>");
            return;
        }

        if (cmd == "start" || cmd == "stop" || cmd == "restart" || strex(cmd).starts_with("announce ")) {
            if (!session.IsController) {
                QueueSessionLine(session, "status", "Control is required for this command");
                return;
            }
        }

        if (cmd == "start") {
            if (_callbacks.GetServer != nullptr && _callbacks.GetServer() != nullptr) {
                QueueSessionLine(session, "status", "Server is already running");
                return;
            }

            if (_callbacks.StartServer != nullptr) {
                _callbacks.StartServer();
            }
            QueueSessionLine(session, "status", "Server start requested");
            return;
        }

        if (cmd == "stop") {
            if (_callbacks.GetServer == nullptr || _callbacks.GetServer() == nullptr) {
                QueueSessionLine(session, "status", "Server is already stopped");
                return;
            }

            if (_callbacks.StopServer != nullptr) {
                _callbacks.StopServer();
            }
            QueueSessionLine(session, "status", "Server stop requested");
            return;
        }

        if (cmd == "restart") {
            if (_callbacks.RestartServer != nullptr) {
                _callbacks.RestartServer();
            }
            QueueSessionLine(session, "status", "Server restart requested");
            return;
        }

        nptr<ServerEngine> server = _callbacks.GetServer ? _callbacks.GetServer() : nullptr;

        if (cmd == "stats") {
            if (!server) {
                QueueSessionLine(session, "status", "Stats: server is stopped");
                return;
            }

            if (!server->Lock(std::chrono::milliseconds {25})) {
                QueueSessionLine(session, "status", "Stats: server is busy");
                return;
            }

            const auto snapshot = server->GetAdminPanelSnapshotData();
            server->Unlock();

            QueueSessionLine(session, "status", strex("Stats: online={}/{}, lps={}, players={}, critters={}, locations={}, maps={}, items={}, db_jobs={}, loop_ms(avg/min/max)={:.2f}/{:.2f}/{:.2f}", snapshot.Online, snapshot.MaxOnline, snapshot.LoopsPerSecond, snapshot.Players, snapshot.Critters, snapshot.Locations, snapshot.Maps, snapshot.Items, snapshot.DbCommitJobs, snapshot.LoopAvgMs, snapshot.LoopMinMs, snapshot.LoopMaxMs).str());
            return;
        }

        if (cmd == "sessions") {
            QueueSessionLine(session, "status", MakeSessionsStatus());
            return;
        }

        if (cmd == "players" || strex(cmd).starts_with("players ")) {
            if (!server) {
                QueueSessionLine(session, "status", "Players online (0), limit=20");
                return;
            }

            string prefix;
            size_t max_names_in_response = 20;

            if (cmd.size() > 7) {
                const auto args_str = strex(string_view(cmd).substr(7)).trim().str();

                if (!args_str.empty()) {
                    const string_view args {args_str};
                    vector<string_view> tokens;
                    size_t pos = 0;

                    while (pos < args.size()) {
                        while (pos < args.size() && args[pos] == ' ') {
                            pos++;
                        }

                        if (pos >= args.size()) {
                            break;
                        }

                        const auto next_space = args.find(' ', pos);

                        if (next_space == string::npos) {
                            tokens.emplace_back(args.substr(pos));
                            break;
                        }

                        tokens.emplace_back(args.substr(pos, next_space - pos));
                        pos = next_space + 1;
                    }

                    if (!tokens.empty()) {
                        size_t parsed_limit = 0;

                        if (tokens.size() == 1) {
                            if (ParsePositiveNumber(tokens[0], parsed_limit)) {
                                max_names_in_response = parsed_limit;
                            }
                            else {
                                prefix = string(tokens[0]);
                            }
                        }
                        else if (ParsePositiveNumber(tokens.back(), parsed_limit)) {
                            max_names_in_response = parsed_limit;

                            for (size_t i = 0; i + 1 < tokens.size(); i++) {
                                if (i != 0) {
                                    prefix += ' ';
                                }

                                prefix += tokens[i];
                            }
                        }
                        else {
                            prefix = args_str;
                        }
                    }
                }
            }

            max_names_in_response = std::clamp(max_names_in_response, size_t {1}, size_t {200});

            if (!server->Lock(std::chrono::milliseconds {25})) {
                QueueSessionLine(session, "status", "Players: server is busy");
                return;
            }

            vector<string> names;
            names.reserve(server->EntityMngr.GetPlayersCount());

            for (const auto& player : server->EntityMngr.GetPlayers()) {
                const auto player_name = player->GetName();

                if (!prefix.empty() && !strex(player_name).starts_with(prefix)) {
                    continue;
                }

                names.emplace_back(player_name);
            }

            server->Unlock();
            std::ranges::sort(names);

            string result = strex("Players online ({})", names.size()).str();

            if (!prefix.empty()) {
                result += strex(", prefix='{}'", prefix).str();
            }

            result += strex(", limit={}", max_names_in_response).str();

            if (!names.empty()) {
                result += ": ";

                const auto shown_count = std::min(names.size(), max_names_in_response);

                for (size_t i = 0; i < shown_count; i++) {
                    if (i != 0) {
                        result += ", ";
                    }

                    result += names[i];
                }

                if (shown_count < names.size()) {
                    result += ", ...";
                }
            }

            QueueSessionLine(session, "status", result);
            return;
        }

        if (strex(cmd).starts_with("announce ")) {
            const auto text = strex(string_view(cmd).substr(9)).trim().str();

            if (text.empty()) {
                QueueSessionLine(session, "status", "Announce text is empty");
                return;
            }

            if (!server) {
                QueueSessionLine(session, "status", "Server is stopped");
                return;
            }

            if (!server->Lock(std::chrono::milliseconds {25})) {
                QueueSessionLine(session, "status", "Announce failed: server is busy");
                return;
            }

            server->SendAdminTextToPlayers(text);
            server->Unlock();
            QueueSessionLine(session, "status", "Announce delivered");
            return;
        }

        QueueSessionLine(session, "status", "Unknown command. Allowed: start, stop, restart, announce <utf8 text>, stats, sessions");
    }
    catch (const std::exception& ex) {
        ReportExceptionAndContinue(ex);
        QueueSessionLine(session, "status", strex("Command failed: {}", ex.what()).str());
    }
}

void AdminServerHost::HandleSessionLine(Session& session, string_view line)
{
    const auto fields = SplitAdminFields(line);

    if (fields.empty()) {
        return;
    }

    if (fields[0] == "auth") {
        const auto was_authorized = session.Authorized;
        const auto provided_psk = fields.size() >= 2 ? string(fields[1]) : string {};
        const auto request_control = fields.size() >= 3 && fields[2] == "1";

        if (!GetApp()->Settings.AdminPsk.empty() && provided_psk != GetApp()->Settings.AdminPsk) {
            session.Authorized = false;
            AppendOutgoing(session, "auth\t0\t0\tInvalid password\n");
            session.Closed = true;
            return;
        }

        session.Authorized = true;
        session.WantsControl = request_control;

        if (!was_authorized) {
            WriteLog(strex("Admin connected from {}", FormatRemoteAddress(session.RemoteHost, session.RemotePort)).str());
        }

        string auth_message = "Observer access granted";

        if (request_control) {
            if (GrantControl(session)) {
                auth_message = "Controller access granted";
            }
            else {
                auth_message = "Observer access granted. Controller is already taken";
            }
        }

        AppendOutgoing(session, strex("auth\t{}\t{}\t{}\n", session.Authorized ? 1 : 0, session.IsController ? 1 : 0, MakeAdminField(auth_message)).str());
        SendLogBacklog(session);
        QueueSnapshot(session);
        session.NextSnapshotTime = nanotime::now() + ADMIN_SNAPSHOT_INTERVAL;
        return;
    }

    if (!session.Authorized) {
        QueueSessionLine(session, "status", "Authorization required");
        return;
    }

    if (fields[0] == "control") {
        const auto wants_control = fields.size() >= 2 && fields[1] == "1";
        session.WantsControl = wants_control;

        if (wants_control) {
            if (GrantControl(session)) {
                QueueSessionLine(session, "status", "Controller access granted");
            }
            else {
                QueueSessionLine(session, "status", "Controller is already taken");
            }
        }
        else {
            ReleaseControl(session);
            QueueSessionLine(session, "status", "Observer mode enabled");
        }

        QueueSnapshot(session);
        return;
    }

    if (fields[0] == "command") {
        ExecuteCommand(session, fields.size() >= 2 ? fields[1] : string_view {});
    }
}

void AdminServerHost::ReceiveSessionLines(Session& session)
{
    while (session.Socket.can_read(std::chrono::milliseconds {0})) {
        array<uint8_t, 64 * 1024> recv_data {};
        const auto recv_size = session.Socket.receive(recv_data);

        if (recv_size <= 0) {
            session.Closed = true;
            return;
        }

        session.IncomingCipher.Apply(recv_data.data(), numeric_cast<size_t>(recv_size));

        session.IncomingData.append(reinterpret_cast<const char*>(recv_data.data()), numeric_cast<size_t>(recv_size));
    }

    size_t line_end = 0;
    while ((line_end = session.IncomingData.find('\n')) != string::npos) {
        auto line = string_view(session.IncomingData).substr(0, line_end);

        if (!line.empty() && line.back() == '\r') {
            line = line.substr(0, line.size() - 1);
        }

        HandleSessionLine(session, line);
        session.IncomingData.erase(0, line_end + 1);
    }
}

void AdminServerHost::Tick()
{
    UpdateServerState();

    if (_discoveryReady) {
        while (_discoverySocket.can_read(std::chrono::milliseconds {0})) {
            array<uint8_t, 1024> read_buf {};
            string remote_host;
            uint16_t remote_port = 0;
            const auto read_size = _discoverySocket.receive_from(span<uint8_t>(read_buf.data(), read_buf.size() - 1), remote_host, remote_port);

            if (read_size <= 0) {
                continue;
            }

            const string_view request(reinterpret_cast<const char*>(read_buf.data()), numeric_cast<size_t>(read_size));

            if (!strex(request).starts_with(ADMIN_DISCOVERY_PROBE)) {
                continue;
            }

            int64_t online = 0;
            nptr<ServerEngine> server = _callbacks.GetServer ? _callbacks.GetServer() : nullptr;

            if (server) {
                online = numeric_cast<int64_t>(_lastSnapshot.Online);

                if (server->Lock(std::chrono::milliseconds {10})) {
                    online = numeric_cast<int64_t>(server->GetAdminPanelSnapshotData().Online);
                    _lastSnapshot.Online = numeric_cast<uint32_t>(online);
                    _lastSnapshotValid = true;
                    server->Unlock();
                }
            }

            const auto response = strex("{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\n", ADMIN_DISCOVERY_RESPONSE_PREFIX, ADMIN_DISCOVERY_PROTOCOL_VERSION, GetApp()->Settings.ServerPort, GetApp()->Settings.AdminControlPort, online, 1, MakeAdminField(GetApp()->Settings.GameName), GetServerStateCode(_serverState), GetStateDurationMs()).str();
            const auto bytes = span<const uint8_t>(reinterpret_cast<const uint8_t*>(response.data()), response.size());
            _discoverySocket.send_to(remote_host, remote_port, bytes);
        }
    }

    if (_listenerReady) {
        while (_listener.can_accept(std::chrono::milliseconds {0})) {
            string remote_host;
            uint16_t remote_port = 0;
            auto socket = _listener.accept(remote_host, remote_port);

            if (socket.is_valid()) {
                _sessions.emplace_back();
                auto& session = _sessions.back();
                session.Socket = std::move(socket);
                session.RemoteHost = std::move(remote_host);
                session.RemotePort = remote_port;
                session.IncomingCipher.Init(GetApp()->Settings.AdminPsk);
                session.OutgoingCipher.Init(GetApp()->Settings.AdminPsk);
                session.NextLogIndex = _logOffset + _logLines.size();
            }
        }
    }

    const auto now = nanotime::now();
    for (auto& session : _sessions) {
        ReceiveSessionLines(session);

        if (session.Closed) {
            continue;
        }

        if (session.Authorized) {
            UpdateSessionLogs(session);

            if (now >= session.NextSnapshotTime) {
                QueueSnapshot(session);
                session.NextSnapshotTime = now + ADMIN_SNAPSHOT_INTERVAL;
            }
        }

        FlushSession(session);
    }

    for (auto it = _sessions.begin(); it != _sessions.end();) {
        if (!it->Closed) {
            ++it;
            continue;
        }

        if (_controllerSession == &*it) {
            _controllerSession = nullptr;
        }

        if (it->Authorized) {
            WriteLog(strex("Admin disconnected from {}", FormatRemoteAddress(it->RemoteHost, it->RemotePort)).str());
        }

        it = _sessions.erase(it);
    }
}

FO_END_NAMESPACE
