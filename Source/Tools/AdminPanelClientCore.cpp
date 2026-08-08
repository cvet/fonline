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

#include "AdminPanel.h"

#include "Application.h"
#include "Settings.h"

#include <charconv>
#include <cstdlib>
#include <limits>

FO_BEGIN_NAMESPACE

static constexpr string_view ADMIN_DISCOVERY_BROADCAST_ADDRESS = "255.255.255.255";
static constexpr string_view ADMIN_DISCOVERY_LOOPBACK_ADDRESS = "127.0.0.1";
static constexpr auto ADMIN_DISCOVERY_REFRESH = std::chrono::milliseconds {1000};
static constexpr auto ADMIN_DISCOVERY_SERVER_TTL = std::chrono::milliseconds {5000};

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

static auto SplitAdminFields(string_view value, char separator = '\t') -> vector<string_view>
{
    vector<string_view> fields;
    size_t begin = 0;

    while (begin <= value.size()) {
        const auto separator_pos = value.find(separator, begin);

        if (separator_pos == string_view::npos) {
            fields.emplace_back(value.substr(begin));
            break;
        }

        fields.emplace_back(value.substr(begin, separator_pos - begin));
        begin = separator_pos + 1;
    }

    return fields;
}

static auto ParseInt64Value(string_view value, int64_t& out_value) -> bool
{
    int64_t parsed = 0;
    const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), parsed);

    if (ec != std::errc {} || ptr != value.data() + value.size()) {
        return false;
    }

    out_value = parsed;
    return true;
}

static auto ParseUInt32Value(string_view value, uint32_t& out_value) -> bool
{
    uint32_t parsed = 0;
    const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), parsed);

    if (ec != std::errc {} || ptr != value.data() + value.size()) {
        return false;
    }

    out_value = parsed;
    return true;
}

static auto ParseFloatValue(string_view value, float32_t& out_value) -> bool
{
    const string tmp {value};
    out_value = static_cast<float32_t>(std::atof(tmp.c_str()));
    return true;
}

auto AdminDiscoveryClient::SplitFields(string_view value, char separator) -> vector<string_view>
{
    return SplitAdminFields(value, separator);
}

void AdminDiscoveryClient::Tick()
{
    const auto now = nanotime::now();

    if (!_initialized) {
        _initialized = true;
        _initializedOk = _socket.bind("0.0.0.0", 0, true) && _socket.set_broadcast(true);

        if (!_initializedOk) {
            _lastError = "Can't initialize UDP discovery socket";
        }
    }

    if (_initializedOk && _enabled) {
        if (now >= _nextProbeTime) {
            SendProbe();
            _nextProbeTime = now + ADMIN_DISCOVERY_REFRESH;
        }

        PollResponses();
    }

    CleanupStaleServers(now);
}

void AdminDiscoveryClient::SetEnabled(bool enabled)
{
    _enabled = enabled;

    if (!enabled) {
        _servers.clear();
        _nextProbeTime = {};
    }
}

auto AdminDiscoveryClient::GetServers() const -> vector<DiscoveredServer>
{
    vector<DiscoveredServer> result;

    for (const auto& entry : _servers | std::views::values) {
        result.emplace_back(entry);
    }

    std::ranges::sort(result, [](const DiscoveredServer& l, const DiscoveredServer& r) {
        if (l.Name != r.Name) {
            return l.Name < r.Name;
        }

        if (l.Host != r.Host) {
            return l.Host < r.Host;
        }

        return l.ServerPort < r.ServerPort;
    });

    return result;
}

void AdminDiscoveryClient::SendProbe()
{
    const auto probe = span<const uint8_t>(reinterpret_cast<const uint8_t*>(ADMIN_DISCOVERY_PROBE.data()), ADMIN_DISCOVERY_PROBE.size());
    _socket.send_to(ADMIN_DISCOVERY_BROADCAST_ADDRESS, numeric_cast<uint16_t>(GetApp()->Settings.AdminDiscoveryPort), probe);
    _socket.send_to(ADMIN_DISCOVERY_LOOPBACK_ADDRESS, numeric_cast<uint16_t>(GetApp()->Settings.AdminDiscoveryPort), probe);
}

void AdminDiscoveryClient::PollResponses()
{
    constexpr size_t buffer_size = 2048;
    array<uint8_t, buffer_size> buffer {};

    while (_socket.can_read(std::chrono::milliseconds {0})) {
        string remote_host;
        uint16_t remote_port = 0;
        const auto read_size = _socket.receive_from(span<uint8_t>(buffer.data(), buffer.size() - 1), remote_host, remote_port);

        if (read_size <= 0) {
            continue;
        }

        const string_view response(reinterpret_cast<const char*>(buffer.data()), numeric_cast<size_t>(read_size));
        const string response_line = strex(response).trim().str();
        const auto fields = SplitFields(response_line, '\t');

        if (fields.size() < 7 || fields[0] != ADMIN_DISCOVERY_RESPONSE_PREFIX) {
            continue;
        }

        int32_t protocol_version = 0;
        int32_t server_port = 0;
        int32_t admin_port = 0;
        int32_t online = 0;
        int32_t supports_control = 0;
        uint32_t state_value = static_cast<uint32_t>(AdminServerState::Stopped);
        int64_t state_duration_ms = 0;

        if (!strvex(fields[1]).is_number()) {
            continue;
        }

        protocol_version = strvex(fields[1]).to_int32();

        if (protocol_version != ADMIN_DISCOVERY_PROTOCOL_VERSION) {
            continue;
        }

        if (!strvex(fields[2]).is_number()) {
            continue;
        }

        server_port = strvex(fields[2]).to_int32();

        if (server_port <= 0 || server_port > std::numeric_limits<uint16_t>::max()) {
            continue;
        }

        if (!strvex(fields[3]).is_number()) {
            continue;
        }

        admin_port = strvex(fields[3]).to_int32();

        if (admin_port <= 0 || admin_port > std::numeric_limits<uint16_t>::max()) {
            continue;
        }

        if (!strvex(fields[4]).is_number()) {
            continue;
        }

        online = strvex(fields[4]).to_int32();

        if (!strvex(fields[5]).is_number()) {
            continue;
        }

        supports_control = strvex(fields[5]).to_int32();

        if (fields.size() >= 9) {
            if (!ParseUInt32Value(fields[7], state_value) || !ParseInt64Value(fields[8], state_duration_ms)) {
                continue;
            }
        }

        const string id = strex("{}:{}", remote_host, admin_port).str();
        auto& server = _servers[id];

        server.Id = id;
        server.Host = remote_host;
        server.ServerPort = numeric_cast<uint16_t>(server_port);
        server.AdminPort = numeric_cast<uint16_t>(admin_port);
        server.State = static_cast<AdminServerState>(state_value);
        server.StateDurationMs = state_duration_ms;
        server.Online = online;
        server.SupportsControl = supports_control != 0;
        server.Name = string(fields[6]);
        server.LastSeen = nanotime::now();
    }
}

void AdminDiscoveryClient::CleanupStaleServers(nanotime now)
{
    std::erase_if(_servers, [now](const auto& entry) { return now - entry.second.LastSeen > ADMIN_DISCOVERY_SERVER_TTL; });
}

AdminProtocolClient::AdminProtocolClient()
{
    ignore_unused(net_sockets::startup());
}

void AdminProtocolClient::Connect(string_view host, uint16_t port, string_view psk, bool request_control)
{
    Disconnect();
    ClearEvents();

    if (!_socket.connect(host, port)) {
        _status = strex("Connect failed {}:{}", host, port).str();
        AddEvent(_status, true, "Connection");
        return;
    }

    _connected = true;
    _host = host;
    _port = port;
    _psk = string(psk);
    _incomingCipher.Init(_psk);
    _outgoingCipher.Init(_psk);
    _desiredControl = request_control;
    _status = strex("Connecting to {}:{}", host, port).str();
    _sessionsInfo = "Sessions: pending";
    _nextSessionsRequestTime = {};

    QueueLine(strex("auth\t{}\t{}", MakeAdminField(_psk), _desiredControl ? 1 : 0).str());
    _authSent = true;
    FlushOutgoing();
}

void AdminProtocolClient::Disconnect()
{
    if (_connected && _adminAuthorized) {
        try {
            QueueLine("control\t0");
            FlushOutgoing();
        }
        catch (...) {
        }
    }

    ResetConnectionState(false, "Not connected");
}

void AdminProtocolClient::ResetConnectionState(bool emit_disconnect_event, string_view status)
{
    const auto was_connected = _connected;

    _socket.close();
    _connected = false;
    _adminAuthorized = false;
    _isController = false;
    _desiredControl = false;
    _authSent = false;
    _pendingControlRequest = false;
    _host.clear();
    _port = 0;
    _psk.clear();
    _pendingInput.clear();
    _status = string(status);
    _sessionsInfo = "Sessions: pending";
    _snapshot = {};
    _incomingData.clear();
    _outgoingData.clear();
    _outgoingPos = 0;
    _nextSessionsRequestTime = {};
    _incomingCipher.Reset();
    _outgoingCipher.Reset();

    if (emit_disconnect_event && was_connected) {
        AddEvent("Disconnected", true, "Connection");
    }
}

void AdminProtocolClient::Tick()
{
    if (!_connected) {
        return;
    }

    try {
        ReceiveIncoming();
        ProcessIncomingLines();

        if (_adminAuthorized && _pendingControlRequest) {
            QueueLine(strex("control\t{}", _desiredControl ? 1 : 0).str());
            _pendingControlRequest = false;
        }

        if (_adminAuthorized && !_pendingInput.empty()) {
            QueueLine(strex("command\t{}", MakeAdminField(_pendingInput)).str());
            _pendingInput.clear();
        }

        FlushOutgoing();
    }
    catch (const std::exception& ex) {
        _status = strex("Protocol error: {}", ex.what()).str();
        AddEvent(_status, true, "Protocol");
        Disconnect();
    }
}

void AdminProtocolClient::SetDesiredControl(bool desired_control)
{
    _desiredControl = desired_control;
    _pendingControlRequest = _adminAuthorized;
}

void AdminProtocolClient::QueueInput(string_view command)
{
    _pendingInput = string(command);
}

void AdminProtocolClient::ClearEvents()
{
    _events.clear();
}

void AdminProtocolClient::AddEvent(string_view text, bool local, string_view source)
{
    if (text.empty()) {
        return;
    }

    AdminLogEntry entry;
    entry.Source = string(source);
    entry.Local = local;

    for (const auto& line : strex(text).split('\n')) {
        if (!line.empty()) {
            entry.Lines.emplace_back(line);
        }
    }

    if (entry.Lines.empty()) {
        return;
    }

    if (!_events.empty() && _events.back().Local == entry.Local && _events.back().Source == entry.Source && _events.back().Lines == entry.Lines) {
        return;
    }

    _events.emplace_back(std::move(entry));

    constexpr size_t max_events = 512;
    if (_events.size() > max_events) {
        _events.erase(_events.begin(), _events.begin() + numeric_cast<ptrdiff_t>(_events.size() - max_events));
    }
}

void AdminProtocolClient::QueueLine(string_view line)
{
    string encoded_line;

    if (!line.empty()) {
        encoded_line.assign(line.begin(), line.end());
    }

    encoded_line += '\n';
    _outgoingCipher.Apply(encoded_line.data(), encoded_line.size());
    _outgoingData += encoded_line;
}

void AdminProtocolClient::FlushOutgoing()
{
    while (_outgoingPos < _outgoingData.size()) {
        if (!_socket.can_write(std::chrono::milliseconds {0})) {
            return;
        }

        const auto data = span<const uint8_t>(reinterpret_cast<const uint8_t*>(_outgoingData.data() + _outgoingPos), _outgoingData.size() - _outgoingPos);
        const auto sent = _socket.send(data);

        if (sent <= 0) {
            throw GenericException("Failed to send admin panel data");
        }

        _outgoingPos += numeric_cast<size_t>(sent);
    }

    if (_outgoingPos >= _outgoingData.size()) {
        _outgoingData.clear();
        _outgoingPos = 0;
    }
}

void AdminProtocolClient::ReceiveIncoming()
{
    while (_socket.can_read(std::chrono::milliseconds {0})) {
        array<uint8_t, 64 * 1024> recv_data {};
        const auto recv_size = _socket.receive(recv_data);

        if (recv_size <= 0) {
            throw GenericException("Admin server disconnected");
        }

        _incomingCipher.Apply(recv_data.data(), numeric_cast<size_t>(recv_size));

        _incomingData.append(reinterpret_cast<const char*>(recv_data.data()), numeric_cast<size_t>(recv_size));
    }
}

void AdminProtocolClient::ProcessIncomingLines()
{
    size_t line_end = 0;

    while ((line_end = _incomingData.find('\n')) != string::npos) {
        auto line = string_view(_incomingData).substr(0, line_end);

        if (!line.empty() && line.back() == '\r') {
            line = line.substr(0, line.size() - 1);
        }

        HandleLine(line);
        _incomingData.erase(0, line_end + 1);
    }
}

void AdminProtocolClient::HandleLine(string_view line)
{
    const auto fields = SplitAdminFields(line);

    if (fields.empty()) {
        return;
    }

    if (fields[0] == "auth") {
        if (fields.size() >= 4) {
            _adminAuthorized = fields[1] == "1";
            _isController = fields[2] == "1";
            _status = string(fields[3]);
            _nextSessionsRequestTime = {};

            if (!_adminAuthorized) {
                ResetConnectionState(false, _status);
            }
        }
        return;
    }

    if (fields[0] == "log") {
        if (fields.size() >= 2) {
            AdminLogEntry entry;
            entry.Source = "Remote log";
            entry.Local = false;

            for (size_t i = 1; i < fields.size(); i++) {
                if (!fields[i].empty()) {
                    entry.Lines.emplace_back(fields[i]);
                }
            }

            if (!entry.Lines.empty()) {
                if (_events.empty() || _events.back().Local != entry.Local || _events.back().Source != entry.Source || _events.back().Lines != entry.Lines) {
                    _events.emplace_back(std::move(entry));

                    constexpr size_t max_events = 512;
                    if (_events.size() > max_events) {
                        _events.erase(_events.begin(), _events.begin() + numeric_cast<ptrdiff_t>(_events.size() - max_events));
                    }
                }
            }
        }
        return;
    }

    if (fields[0] == "status") {
        if (fields.size() >= 2) {
            _status = string(fields[1]);

            if (strex(fields[1]).starts_with("Sessions:")) {
                _sessionsInfo = string(fields[1]);
                return;
            }

            AddEvent(_status, false, "Status");
        }
        return;
    }

    if (fields[0] == "snapshot") {
        if (fields.size() < 24) {
            return;
        }

        uint32_t state_value = static_cast<uint32_t>(AdminServerState::Stopped);
        int64_t state_duration_ms = 0;
        int64_t uptime_ms = 0;
        int64_t downtime_ms = 0;
        uint32_t online = 0;
        uint32_t max_online = 0;
        uint32_t loops_per_second = 0;
        uint32_t players = 0;
        uint32_t critters = 0;
        uint32_t locations = 0;
        uint32_t maps = 0;
        uint32_t items = 0;
        uint32_t db_jobs = 0;
        float32_t loop_avg = 0.0f;
        float32_t loop_min = 0.0f;
        float32_t loop_max = 0.0f;

        if (!ParseUInt32Value(fields[3], state_value) || !ParseInt64Value(fields[4], state_duration_ms) || !ParseInt64Value(fields[5], uptime_ms) || !ParseInt64Value(fields[6], downtime_ms) || !ParseUInt32Value(fields[7], online) || !ParseUInt32Value(fields[8], max_online) || !ParseUInt32Value(fields[9], loops_per_second) || !ParseUInt32Value(fields[10], players) || !ParseUInt32Value(fields[11], critters) || !ParseUInt32Value(fields[12], locations) || !ParseUInt32Value(fields[13], maps) || !ParseUInt32Value(fields[14], items) || !ParseUInt32Value(fields[15], db_jobs) || !ParseFloatValue(fields[16], loop_avg) || !ParseFloatValue(fields[17], loop_min) || !ParseFloatValue(fields[18], loop_max)) {
            return;
        }

        _snapshot.Valid = true;
        _snapshot.ServerRunning = fields[1] == "1";
        _snapshot.ServerStartingError = fields[2] == "1";
        _snapshot.State = static_cast<AdminServerState>(state_value);
        _snapshot.StateDurationMs = state_duration_ms;
        _snapshot.UptimeMs = uptime_ms;
        _snapshot.DowntimeMs = downtime_ms;
        _snapshot.Online = online;
        _snapshot.MaxOnline = max_online;
        _snapshot.LoopsPerSecond = loops_per_second;
        _snapshot.Players = players;
        _snapshot.Critters = critters;
        _snapshot.Locations = locations;
        _snapshot.Maps = maps;
        _snapshot.Items = items;
        _snapshot.DbCommitJobs = db_jobs;
        _snapshot.LoopAvgMs = loop_avg;
        _snapshot.LoopMinMs = loop_min;
        _snapshot.LoopMaxMs = loop_max;
        _snapshot.ServerVersion = string(fields[19]);
        _snapshot.GameVersion = string(fields[20]);
        _snapshot.CompatibilityVersion = string(fields[21]);
        _snapshot.IsController = fields[22] == "1";
        _snapshot.HasController = fields[23] == "1";
        _isController = _snapshot.IsController;
    }
}

FO_END_NAMESPACE
