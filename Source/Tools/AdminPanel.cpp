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
#include "ImGuiStuff.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

static constexpr string_view ADMIN_DISCOVERY_LOOPBACK_ADDRESS = "127.0.0.1";

static auto GetServerStateName(AdminServerState state) -> string_view
{
    switch (state) {
    case AdminServerState::Starting:
        return "Starting";
    case AdminServerState::Running:
        return "Running";
    case AdminServerState::StartingError:
        return "Starting error";
    case AdminServerState::Stopped:
    default:
        return "Stopped";
    }
}

static auto FormatDurationMs(int64_t duration_ms) -> string
{
    if (duration_ms <= 0) {
        return "0 sec";
    }

    const auto total_seconds = duration_ms / 1000;
    const auto days = total_seconds / (24 * 60 * 60);
    const auto hours = (total_seconds / (60 * 60)) % 24;
    const auto minutes = (total_seconds / 60) % 60;
    const auto seconds = total_seconds % 60;

    if (days > 0) {
        return strex("{}d {:02}h {:02}m {:02}s", days, hours, minutes, seconds).str();
    }

    if (hours > 0) {
        return strex("{}h {:02}m {:02}s", hours, minutes, seconds).str();
    }

    if (minutes > 0) {
        return strex("{}m {:02}s", minutes, seconds).str();
    }

    return strex("{} sec", seconds).str();
}

static auto BuildServerInfoRows(const AdminSnapshot& snapshot) -> vector<std::pair<string, string>>
{
    vector<std::pair<string, string>> rows;
    rows.reserve(16);
    const auto state_name = GetServerStateName(snapshot.State);
    const auto state_time = snapshot.State == AdminServerState::Running ? FormatDurationMs(snapshot.UptimeMs) : FormatDurationMs(snapshot.DowntimeMs != 0 ? snapshot.DowntimeMs : snapshot.StateDurationMs);
    const auto state_time_label = snapshot.State == AdminServerState::Running ? string_view("Uptime") : string_view("Downtime");

    rows.emplace_back("Game version", snapshot.GameVersion.empty() ? string("n/a") : snapshot.GameVersion);
    rows.emplace_back("Compatibility version", snapshot.CompatibilityVersion.empty() ? string("n/a") : snapshot.CompatibilityVersion);
    rows.emplace_back("Engine version", snapshot.ServerVersion.empty() ? string("n/a") : snapshot.ServerVersion);
    rows.emplace_back("State", string(state_name));
    rows.emplace_back(string(state_time_label), state_time);
    rows.emplace_back("Online", strex("{} / {}", snapshot.Online, snapshot.MaxOnline).str());
    rows.emplace_back("Loops per second", strex("{}", snapshot.LoopsPerSecond).str());
    rows.emplace_back("Players", strex("{}", snapshot.Players).str());
    rows.emplace_back("Critters", strex("{}", snapshot.Critters).str());
    rows.emplace_back("Locations", strex("{}", snapshot.Locations).str());
    rows.emplace_back("Maps", strex("{}", snapshot.Maps).str());
    rows.emplace_back("Items", strex("{}", snapshot.Items).str());
    rows.emplace_back("DB commit jobs", strex("{}", snapshot.DbCommitJobs).str());
    rows.emplace_back("Loop average", strex("{:.2f} ms", snapshot.LoopAvgMs).str());
    rows.emplace_back("Loop minimum", strex("{:.2f} ms", snapshot.LoopMinMs).str());
    rows.emplace_back("Loop maximum", strex("{:.2f} ms", snapshot.LoopMaxMs).str());
    return rows;
}

static auto CanStartServer(const AdminSnapshot& snapshot, bool is_controller) -> bool
{
    return is_controller && snapshot.Valid && snapshot.State == AdminServerState::Stopped;
}

static auto CanStopServer(const AdminSnapshot& snapshot, bool is_controller) -> bool
{
    return is_controller && snapshot.Valid && snapshot.State != AdminServerState::Stopped;
}

static auto CanRestartServer(const AdminSnapshot& snapshot, bool is_controller) -> bool
{
    return is_controller && snapshot.Valid && snapshot.State == AdminServerState::Running;
}

AdminPanel::AdminPanel()
{
    const auto default_host = ADMIN_DISCOVERY_LOOPBACK_ADDRESS;
    std::copy(default_host.begin(), default_host.end(), _manualHost.begin());
    _manualHost[default_host.size()] = '\0';
    _manualPort = GetApp()->Settings.AdminControlPort;
}

void AdminPanel::MainLoop()
{
    _discovery.Tick();
    _connection.Tick();

    const auto servers = _discovery.GetServers();

    ImGui::SetNextWindowPos(ImVec2(15, 15), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Admin Panel", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
        if (!_discovery.IsReady()) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "Discovery socket is not available: %s", _discovery.GetLastError().c_str());
        }

        bool discovery_enabled = _discovery.IsEnabled();
        if (ImGui::Checkbox("Enable discovery", &discovery_enabled)) {
            _discovery.SetEnabled(discovery_enabled);
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Password");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(220);
        ImGui::InputText("##Password", _passwordInput.data(), _passwordInput.size(), ImGuiInputTextFlags_Password);

        if (ImGui::CollapsingHeader("Manual connection")) {
            ImGui::TextUnformatted("Host");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(260);
            ImGui::InputText("##Host", _manualHost.data(), _manualHost.size());
            ImGui::SameLine();
            ImGui::TextUnformatted("Port");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120);
            ImGui::InputInt("##Port", &_manualPort);
            ImGui::SameLine();
            if (ImGui::Button("Connect##manual")) {
                if (_manualHost[0] != '\0' && _manualPort > 0 && _manualPort <= std::numeric_limits<uint16_t>::max()) {
                    _selectedServerId = strex("{}:{}", _manualHost.data(), _manualPort).str();
                    _connection.Connect(_manualHost.data(), numeric_cast<uint16_t>(_manualPort), _passwordInput.data(), _requestControl);
                }
            }
        }

        ImGui::Separator();
        ImGui::Text("Discovered servers: %zu", servers.size());

        const auto visible_rows = std::max<size_t>(servers.size() + 1, 2);
        const auto table_height = numeric_cast<float>(visible_rows) * ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().FramePadding.y * 2.0f;

        if (ImGui::BeginTable("Servers", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY, ImVec2(0, table_height))) {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Address");
            ImGui::TableSetupColumn("Game Port");
            ImGui::TableSetupColumn("Admin Port");
            ImGui::TableSetupColumn("State");
            ImGui::TableSetupColumn("Up/Down");
            ImGui::TableSetupColumn("Online");
            ImGui::TableHeadersRow();

            for (const auto& server : servers) {
                const bool connected_server = _connection.IsConnected() && _selectedServerId == server.Id;

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                const auto selectable_id = strex("##server_{}", server.Id).str();
                const bool row_clicked = ImGui::Selectable(selectable_id.c_str(), connected_server, ImGuiSelectableFlags_SpanAllColumns);
                ImGui::SameLine(0.0f, 0.0f);
                ImGui::TextUnformatted(server.Name.c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(server.Host.c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%u", server.ServerPort);
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%u", server.AdminPort);
                ImGui::TableSetColumnIndex(4);
                ImGui::TextUnformatted(GetServerStateName(server.State).data(), GetServerStateName(server.State).data() + GetServerStateName(server.State).size());
                ImGui::TableSetColumnIndex(5);
                ImGui::TextUnformatted(FormatDurationMs(server.StateDurationMs).c_str());
                ImGui::TableSetColumnIndex(6);
                ImGui::Text("%d", server.Online);

                if (row_clicked && !connected_server) {
                    _selectedServerId = server.Id;
                    _connection.Connect(server.Host, server.AdminPort, _passwordInput.data(), _requestControl);
                }
            }

            ImGui::EndTable();
        }
    }

    ImGui::End();

    if (_connection.IsConnected()) {
        const auto window_name = strex("Server: {}", _selectedServerId.empty() ? string_view("<manual>") : _selectedServerId).str();
        bool server_window_open = true;
        ImGui::SetNextWindowPos(ImVec2(15, 15), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(760, 760), ImGuiCond_FirstUseEver);

        if (ImGui::Begin(window_name.c_str(), &server_window_open, ImGuiWindowFlags_NoCollapse)) {
            const auto& snapshot = _connection.GetSnapshot();
            const auto can_start = CanStartServer(snapshot, _connection.IsController());
            const auto can_stop = CanStopServer(snapshot, _connection.IsController());
            const auto can_restart = CanRestartServer(snapshot, _connection.IsController());
            if (snapshot.Valid) {
                if (ImGui::BeginTable("ServerInfo", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthFixed, 220.0f);
                    ImGui::TableSetupColumn("Value");

                    const auto add_row = [](string_view name, string_view value) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextUnformatted(name.data(), name.data() + name.size());
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextUnformatted(value.data(), value.data() + value.size());
                    };

                    for (const auto& [name, value] : BuildServerInfoRows(snapshot)) {
                        add_row(name, value);
                    }

                    ImGui::EndTable();
                }
            }

            if (ImGui::Checkbox("Request control", &_requestControl)) {
                _connection.SetDesiredControl(_requestControl);
            }

            ImGui::BeginDisabled(!can_start);
            if (ImGui::Button("Start##cmd")) {
                _connection.QueueInput("start");
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(!can_stop);
            if (ImGui::Button("Stop##cmd")) {
                _connection.QueueInput("stop");
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(!can_restart);
            if (ImGui::Button("Restart##cmd")) {
                _connection.QueueInput("restart");
            }
            ImGui::EndDisabled();

            ImGui::Separator();
            ImGui::TextUnformatted("Announce");
            ImGui::SetNextItemWidth(420);
            ImGui::InputText("##Announce", _announceInput.data(), _announceInput.size());
            ImGui::SameLine();
            if (ImGui::Button("Send announce") && _announceInput[0] != '\0') {
                _connection.QueueInput(strex("announce {}", _announceInput.data()).str());
                _announceInput[0] = '\0';
            }

            ImGui::Separator();
            if (ImGui::Button("Clear log")) {
                _connection.ClearEvents();
                _lastRenderedEventCount = 0;
            }
            ImGui::SameLine();
            if (ImGui::Button("Disconnect")) {
                _connection.Disconnect();
                _selectedServerId.clear();
            }

            const auto& events = _connection.GetEvents();
            ImGui::BeginChild("CommandLog", ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar);
            for (size_t i = 0; i < events.size(); i++) {
                const auto title = strex("{}##log_{}", events[i].Lines.front(), i).str();

                if (events[i].Local) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.82f, 0.64f, 0.12f, 1.0f));
                }

                const auto opened = ImGui::TreeNodeEx(title.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth);

                if (events[i].Local) {
                    ImGui::PopStyleColor();
                }

                if (opened) {
                    for (size_t line_index = 1; line_index < events[i].Lines.size(); line_index++) {
                        ImGui::TextUnformatted(events[i].Lines[line_index].c_str(), events[i].Lines[line_index].c_str() + events[i].Lines[line_index].size());
                    }

                    ImGui::TreePop();
                }
            }

            if (events.size() != _lastRenderedEventCount && !events.empty()) {
                ImGui::SetScrollHereY(1.0f);
            }

            _lastRenderedEventCount = events.size();
            ImGui::EndChild();
        }

        ImGui::End();

        if (!server_window_open) {
            _connection.Disconnect();
            _selectedServerId.clear();
        }
    }
}

FO_END_NAMESPACE
