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

#include "AdminPanelHeadless.h"

#include "Application.h"
#include "Settings.h"

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/component/loop.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"

#include <thread>

FO_BEGIN_NAMESPACE

using namespace ftxui;

static auto ToStdString(string_view value) -> std::string
{
    return {value.begin(), value.end()};
}

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

static auto FormatDurationMs(int64_t duration_ms) -> std::string
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
        return ToStdString(strex("{}d {:02}h {:02}m {:02}s", days, hours, minutes, seconds).str());
    }

    if (hours > 0) {
        return ToStdString(strex("{}h {:02}m {:02}s", hours, minutes, seconds).str());
    }

    if (minutes > 0) {
        return ToStdString(strex("{}m {:02}s", minutes, seconds).str());
    }

    return ToStdString(strex("{} sec", seconds).str());
}

static auto BuildServerInfoRows(const AdminSnapshot& snapshot) -> std::vector<std::pair<std::string, std::string>>
{
    std::vector<std::pair<std::string, std::string>> rows;
    rows.reserve(16);

    const auto state_name = GetServerStateName(snapshot.State);
    const auto state_time = snapshot.State == AdminServerState::Running ? FormatDurationMs(snapshot.UptimeMs) : FormatDurationMs(snapshot.DowntimeMs != 0 ? snapshot.DowntimeMs : snapshot.StateDurationMs);
    const auto state_time_label = snapshot.State == AdminServerState::Running ? string_view("Uptime") : string_view("Downtime");

    rows.emplace_back("Game version", snapshot.GameVersion.empty() ? std::string("n/a") : ToStdString(snapshot.GameVersion));
    rows.emplace_back("Compatibility version", snapshot.CompatibilityVersion.empty() ? std::string("n/a") : ToStdString(snapshot.CompatibilityVersion));
    rows.emplace_back("Engine version", snapshot.ServerVersion.empty() ? std::string("n/a") : ToStdString(snapshot.ServerVersion));
    rows.emplace_back("State", ToStdString(state_name));
    rows.emplace_back(ToStdString(state_time_label), state_time);
    rows.emplace_back("Online", ToStdString(strex("{} / {}", snapshot.Online, snapshot.MaxOnline).str()));
    rows.emplace_back("Loops per second", ToStdString(strex("{}", snapshot.LoopsPerSecond).str()));
    rows.emplace_back("Players", ToStdString(strex("{}", snapshot.Players).str()));
    rows.emplace_back("Critters", ToStdString(strex("{}", snapshot.Critters).str()));
    rows.emplace_back("Locations", ToStdString(strex("{}", snapshot.Locations).str()));
    rows.emplace_back("Maps", ToStdString(strex("{}", snapshot.Maps).str()));
    rows.emplace_back("Items", ToStdString(strex("{}", snapshot.Items).str()));
    rows.emplace_back("DB commit jobs", ToStdString(strex("{}", snapshot.DbCommitJobs).str()));
    rows.emplace_back("Loop average", ToStdString(strex("{:.2f} ms", snapshot.LoopAvgMs).str()));
    rows.emplace_back("Loop minimum", ToStdString(strex("{:.2f} ms", snapshot.LoopMinMs).str()));
    rows.emplace_back("Loop maximum", ToStdString(strex("{:.2f} ms", snapshot.LoopMaxMs).str()));
    return rows;
}

static auto MakeInfoEntry(string_view name, string_view value) -> std::string
{
    return ToStdString(strex("{}: {}", name, value).str());
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

static auto MakeDisabledCommandLabel(string_view label) -> Component
{
    return ftxui::Renderer([text_label = ToStdString(label)] { return text(text_label) | dim; });
}

static auto MakeCommandButton(string_view label, std::function<void()> handler) -> Component
{
    ButtonOption option = ButtonOption::Simple();
    option.transform = [](const EntryState& state) {
        auto element = text(state.label);

        if (state.focused) {
            element |= inverted;
        }

        if (state.active) {
            element |= bold;
        }

        return element;
    };

    return Button(ToStdString(label), std::move(handler), option);
}

static auto MakeServerLabel(const DiscoveredServer& server) -> std::string
{
    return ToStdString(strex("{} | {} | game {} | admin {} | {} | online {}", server.Name, server.Host, server.ServerPort, server.AdminPort, GetServerStateName(server.State), server.Online).str());
}

static auto MakeServerPanelTitle(string_view server_id) -> std::string
{
    return ToStdString(strex("Server: {}", server_id.empty() ? string_view("<manual>") : server_id).str());
}

AdminPanelHeadless::AdminPanelHeadless(AdminPanelHeadlessOptions options) :
    _options(std::move(options))
{
    _requestControl = _options.RequestControl;
    _discoveryEnabled = _options.DiscoveryEnabled;
    _manualHost = !_options.Host.empty() ? ToStdString(_options.Host) : std::string("127.0.0.1");
    _manualPort = ToStdString(strex("{}", _options.Port != 0 ? _options.Port : numeric_cast<uint16_t>(GetApp()->Settings.AdminControlPort)).str());
    _password = ToStdString(_options.Password);
}

void AdminPanelHeadless::UpdateDiscoveredServers()
{
    _servers = _discovery.GetServers();
    _serverEntries.clear();

    for (const auto& server : _servers) {
        _serverEntries.emplace_back(MakeServerLabel(server));
    }

    if (_serverEntries.empty()) {
        _serverEntries.emplace_back("<no servers discovered>");
        _selectedServerIndex = 0;
    }
    else {
        _selectedServerIndex = std::clamp(_selectedServerIndex, 0, numeric_cast<int>(_serverEntries.size() - 1));
    }
}

void AdminPanelHeadless::ConnectSelectedServer()
{
    if (_servers.empty()) {
        return;
    }

    const auto index = std::clamp(_selectedServerIndex, 0, numeric_cast<int>(_servers.size() - 1));
    const auto& server = _servers[numeric_cast<size_t>(index)];
    _selectedServerId = server.Id;
    _connection.Connect(server.Host, server.AdminPort, _password.c_str(), _requestControl);
    OpenServerPanel();
}

void AdminPanelHeadless::ConnectManualServer()
{
    const auto port_ex = strvex(_manualPort.c_str());

    if (_manualHost.empty() || !port_ex.is_number()) {
        return;
    }

    const auto port = port_ex.to_int32();
    if (port <= 0 || port > std::numeric_limits<uint16_t>::max()) {
        return;
    }

    _selectedServerId = strex("{}:{}", _manualHost, port).str();
    _connection.Connect(_manualHost.c_str(), numeric_cast<uint16_t>(port), _password.c_str(), _requestControl);
    OpenServerPanel();
}

void AdminPanelHeadless::Disconnect()
{
    _connection.Disconnect();
    _selectedServerId.clear();
    _selectedLogIndex = 0;
    _lastEventCount = 0;
    _expandedLogEntries.clear();
}

void AdminPanelHeadless::OpenServerPanel()
{
    _screenIndex = static_cast<int>(AdminPanelHeadlessScreen::Server);
    _focusServerExitPending = true;
    _selectedLogIndex = 0;
    _lastEventCount = 0;
    _expandedLogEntries.clear();
}

void AdminPanelHeadless::ReturnToBrowser(bool disconnect)
{
    if (disconnect) {
        Disconnect();
    }

    _screenIndex = static_cast<int>(AdminPanelHeadlessScreen::Browser);
    _focusPasswordPending = true;
}

void AdminPanelHeadless::Run()
{
    using namespace ftxui;

    ScreenInteractive screen = ScreenInteractive::Fullscreen();
    screen.TrackMouse(false);
    bool manual_connection_expanded = false;
    bool can_start = false;
    bool can_stop = false;
    bool can_restart = false;
    bool show_start_disabled = true;
    bool show_stop_disabled = true;
    bool show_restart_disabled = true;

    if (_options.AutoConnect) {
        ConnectManualServer();
    }

    InputOption password_option;
    password_option.password = true;
    password_option.multiline = false;

    InputOption host_option;
    host_option.multiline = false;

    InputOption port_option;
    port_option.multiline = false;
    port_option.on_enter = [&] { ConnectManualServer(); };

    InputOption announce_option;
    announce_option.multiline = false;
    announce_option.on_enter = [&] {
        if (!_announceInput.empty()) {
            _connection.QueueInput(strex("announce {}", _announceInput).str());
            _announceInput.clear();
        }
    };

    auto control_checkbox = Checkbox("Request control", &_requestControl);
    auto password_input = Input(&_password, "password", password_option);
    auto host_input = Input(&_manualHost, "host", host_option);
    auto port_input = Input(&_manualPort, "port", port_option);
    auto announce_input = Input(&_announceInput, "announce text", announce_option);

    MenuOption server_menu_option = MenuOption::Vertical();
    server_menu_option.on_enter = [&] { ConnectSelectedServer(); };
    auto server_menu = Menu(&_serverEntries, &_selectedServerIndex, server_menu_option);

    auto manual_connection_container = Container::Vertical({host_input, port_input});
    auto manual_connection_block = Collapsible("Manual connect", manual_connection_container, &manual_connection_expanded);

    auto start_button = MakeCommandButton("Start", [&] {
        if (CanStartServer(_connection.GetSnapshot(), _connection.IsController())) {
            _connection.QueueInput("start");
        }
    });
    auto stop_button = MakeCommandButton("Stop", [&] {
        if (CanStopServer(_connection.GetSnapshot(), _connection.IsController())) {
            _connection.QueueInput("stop");
        }
    });
    auto restart_button = MakeCommandButton("Restart", [&] {
        if (CanRestartServer(_connection.GetSnapshot(), _connection.IsController())) {
            _connection.QueueInput("restart");
        }
    });
    auto browser_exit_button = MakeCommandButton("Exit", screen.ExitLoopClosure());
    auto server_exit_button = MakeCommandButton("Exit", [&] { ReturnToBrowser(true); });
    auto start_disabled = MakeDisabledCommandLabel("Start");
    auto stop_disabled = MakeDisabledCommandLabel("Stop");
    auto restart_disabled = MakeDisabledCommandLabel("Restart");
    auto start_command = Maybe(start_button, &can_start);
    auto stop_command = Maybe(stop_button, &can_stop);
    auto restart_command = Maybe(restart_button, &can_restart);
    auto start_disabled_command = Maybe(start_disabled, &show_start_disabled);
    auto stop_disabled_command = Maybe(stop_disabled, &show_stop_disabled);
    auto restart_disabled_command = Maybe(restart_disabled, &show_restart_disabled);
    std::vector<std::string> info_entries {"Not connected"};
    MenuOption info_menu_option = MenuOption::Vertical();
    info_menu_option.entries_option.transform = [](const EntryState& state) {
        auto element = text(state.label);

        if (state.active) {
            element |= inverted;
        }

        if (state.focused) {
            element |= bold;
        }

        return element;
    };
    auto info_menu = Menu(&info_entries, &_selectedInfoIndex, info_menu_option);
    std::vector<std::string> log_entries {"No log events yet"};
    MenuOption log_menu_option = MenuOption::Vertical();
    log_menu_option.on_enter = [&] {
        if (_selectedLogIndex >= 0 && numeric_cast<size_t>(_selectedLogIndex) < _expandedLogEntries.size()) {
            _expandedLogEntries[numeric_cast<size_t>(_selectedLogIndex)] = !_expandedLogEntries[numeric_cast<size_t>(_selectedLogIndex)];
        }
    };
    log_menu_option.entries_option.transform = [&](const EntryState& state) {
        Element header = text(state.label);

        if (state.active) {
            header |= inverted;
        }

        if (state.focused) {
            header |= bold;
        }

        Elements entry_elements;
        entry_elements.emplace_back(std::move(header));

        if (state.index >= 0 && numeric_cast<size_t>(state.index) < _expandedLogEntries.size() && _expandedLogEntries[numeric_cast<size_t>(state.index)]) {
            const auto& entry = _connection.GetEvents()[numeric_cast<size_t>(state.index)];

            for (size_t line_index = 1; line_index < entry.Lines.size(); line_index++) {
                entry_elements.emplace_back(text(ToStdString(strex("    {}", entry.Lines[line_index]).str())) | dim);
            }
        }

        return vbox(std::move(entry_elements));
    };
    auto log_menu = Menu(&log_entries, &_selectedLogIndex, log_menu_option);

    auto server_container = Container::Vertical({
        control_checkbox,
        start_command,
        start_disabled_command,
        stop_command,
        stop_disabled_command,
        restart_command,
        restart_disabled_command,
        announce_input,
        server_exit_button,
        info_menu,
        log_menu,
    });
    auto browser_container = Container::Vertical({password_input, manual_connection_block, server_menu, browser_exit_button});
    auto root_container = Container::Tab({browser_container, server_container}, &_screenIndex);

    auto renderer = ftxui::Renderer(root_container, [&] {
        if (_screenIndex == static_cast<int>(AdminPanelHeadlessScreen::Browser)) {
            return vbox({
                       text("Admin Panel") | bold,
                       separator(),
                       hbox(text("Status: ") | bold, text(ToStdString(_connection.GetStatus()))),
                       separator(),
                       window(text("Connection"),
                           vbox({
                               hbox(text("Password: "), password_input->Render()),
                               separator(),
                               manual_connection_block->Render(),
                           })),
                       window(text("Active Servers"), server_menu->Render() | frame | size(HEIGHT, LESS_THAN, 12)),
                       browser_exit_button->Render(),
                       text("Select a server and press Enter to open it") | dim,
                       text("Hotkeys: Enter open selected, q/Esc quit") | dim,
                   }) |
                border;
        }

        return vbox({
                   text(MakeServerPanelTitle(_selectedServerId)) | bold,
                   separator(),
                   window(text("Commands"),
                       vbox({
                           control_checkbox->Render(),
                           start_command->Render(),
                           start_disabled_command->Render(),
                           stop_command->Render(),
                           stop_disabled_command->Render(),
                           restart_command->Render(),
                           restart_disabled_command->Render(),
                           separator(),
                           hbox(text("Announce: "), announce_input->Render()),
                           separator(),
                           server_exit_button->Render(),
                       })),
                   window(text("Server Info"), info_menu->Render() | frame | size(HEIGHT, LESS_THAN, 8)),
                   window(text("Log"), log_menu->Render() | frame | size(HEIGHT, LESS_THAN, 14)),
                   text("Commands: start stop restart exit") | dim,
                   text("Hotkeys: Esc back, q quit") | dim,
               }) |
            border;
    });

    auto root = ftxui::CatchEvent(renderer, [&](Event event) {
        if (event.is_mouse()) {
            return true;
        }

        if (_screenIndex == static_cast<int>(AdminPanelHeadlessScreen::Browser)) {
            if (event == Event::Character('q') || event == Event::Escape) {
                screen.Exit();
                return true;
            }

            if ((event == Event::Return || event == Event::Character('o')) && server_menu->Focused()) {
                ConnectSelectedServer();
                return true;
            }
        }
        else {
            if (event == Event::Escape || event == Event::Character('b')) {
                ReturnToBrowser(false);
                return true;
            }

            if (event == Event::Character('q')) {
                screen.Exit();
                return true;
            }
        }

        return false;
    });

    ftxui::Loop loop(&screen, root);
    bool prev_request_control = _requestControl;

    while (!loop.HasQuitted() && !GetApp()->IsQuitRequested()) {
        _discovery.SetEnabled(_discoveryEnabled);
        _discovery.Tick();
        _connection.Tick();

        if (_screenIndex == static_cast<int>(AdminPanelHeadlessScreen::Server) && !_connection.IsConnected()) {
            ReturnToBrowser(false);
        }

        if (_requestControl != prev_request_control) {
            _connection.SetDesiredControl(_requestControl);
            prev_request_control = _requestControl;
        }

        const auto& events = _connection.GetEvents();
        const auto had_events = _lastEventCount != 0;
        const auto was_at_bottom = !had_events || _selectedLogIndex >= numeric_cast<int>(_lastEventCount - 1);

        _expandedLogEntries.resize(events.size());
        log_entries.clear();
        info_entries.clear();

        const auto& snapshot = _connection.GetSnapshot();
        if (snapshot.Valid) {
            for (const auto& [name, value] : BuildServerInfoRows(snapshot)) {
                info_entries.emplace_back(MakeInfoEntry(name, value));
            }
        }
        else {
            info_entries.emplace_back("Not connected");
        }

        _selectedInfoIndex = std::clamp(_selectedInfoIndex, 0, numeric_cast<int>(info_entries.size() - 1));

        for (const auto& entry : events) {
            log_entries.emplace_back(entry.Lines.empty() ? std::string("<empty>") : ToStdString(entry.Lines.front()));
        }

        if (log_entries.empty()) {
            log_entries.emplace_back("No log events yet");
            _selectedLogIndex = 0;
        }
        else if (was_at_bottom) {
            _selectedLogIndex = numeric_cast<int>(log_entries.size() - 1);
        }
        else {
            _selectedLogIndex = std::clamp(_selectedLogIndex, 0, numeric_cast<int>(log_entries.size() - 1));
        }

        _lastEventCount = events.size();

        can_start = CanStartServer(_connection.GetSnapshot(), _connection.IsController());
        can_stop = CanStopServer(_connection.GetSnapshot(), _connection.IsController());
        can_restart = CanRestartServer(_connection.GetSnapshot(), _connection.IsController());
        show_start_disabled = !can_start;
        show_stop_disabled = !can_stop;
        show_restart_disabled = !can_restart;

        if (_focusPasswordPending) {
            password_input->TakeFocus();
            _focusPasswordPending = false;
        }

        if (_focusServerExitPending) {
            server_exit_button->TakeFocus();
            _focusServerExitPending = false;
        }

        UpdateDiscoveredServers();
        loop.RunOnce();
        std::this_thread::sleep_for(std::chrono::milliseconds {50});
    }

    Disconnect();
}

FO_END_NAMESPACE
