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

#pragma once

#include "Common.h"

#include "AdminPanelCommon.h"
#include "NetSockets.h"

FO_BEGIN_NAMESPACE

struct DiscoveredServer
{
    string Id {};
    string Host {};
    uint16_t ServerPort {};
    uint16_t AdminPort {};
    AdminServerState State {AdminServerState::Stopped};
    int64_t StateDurationMs {};
    int32_t Online {};
    bool SupportsControl {};
    string Name {};
    nanotime LastSeen {};
};

class AdminDiscoveryClient final
{
public:
    void Tick();
    void SetEnabled(bool enabled);

    [[nodiscard]] auto IsEnabled() const noexcept -> bool { return _enabled; }
    [[nodiscard]] auto IsReady() const noexcept -> bool { return _initializedOk; }
    [[nodiscard]] auto GetLastError() const noexcept -> const string& { return _lastError; }
    [[nodiscard]] auto GetServers() const -> vector<DiscoveredServer>;

private:
    static auto SplitFields(string_view value, char separator) -> vector<string_view>;

    void SendProbe();
    void PollResponses();
    void CleanupStaleServers(nanotime now);

    bool _enabled {true};
    bool _initialized {};
    bool _initializedOk {};
    string _lastError {};
    udp_socket _socket {};
    nanotime _nextProbeTime {};
    unordered_map<string, DiscoveredServer> _servers {};
};

class AdminProtocolClient final
{
public:
    AdminProtocolClient();

    void Connect(string_view host, uint16_t port, string_view psk, bool request_control);
    void Disconnect();
    void Tick();
    void SetDesiredControl(bool desired_control);
    void QueueInput(string_view command);

    [[nodiscard]] auto IsConnected() const noexcept -> bool { return _connected; }
    [[nodiscard]] auto IsAuthorized() const noexcept -> bool { return _adminAuthorized; }
    [[nodiscard]] auto IsController() const noexcept -> bool { return _isController; }
    [[nodiscard]] auto GetStatus() const noexcept -> const string& { return _status; }
    [[nodiscard]] auto GetSnapshot() const noexcept -> const AdminSnapshot& { return _snapshot; }
    [[nodiscard]] auto GetEvents() const noexcept -> const vector<AdminLogEntry>& { return _events; }
    [[nodiscard]] auto GetSessionsInfo() const noexcept -> const string& { return _sessionsInfo; }
    void ClearEvents();

private:
    void AddEvent(string_view text, bool local, string_view source);
    void ResetConnectionState(bool emit_disconnect_event, string_view status);
    void QueueLine(string_view line);
    void FlushOutgoing();
    void ReceiveIncoming();
    void ProcessIncomingLines();
    void HandleLine(string_view line);

    tcp_socket _socket {};
    string _incomingData {};
    string _outgoingData {};
    size_t _outgoingPos {};

    bool _connected {};
    bool _adminAuthorized {};
    bool _isController {};
    bool _desiredControl {};
    bool _authSent {};
    bool _pendingControlRequest {};
    nanotime _nextSessionsRequestTime {};

    string _host {};
    uint16_t _port {};
    string _psk {};
    string _status {"Not connected"};
    string _sessionsInfo {"Sessions: pending"};
    string _pendingInput {};
    AdminSnapshot _snapshot {};
    vector<AdminLogEntry> _events {};
    AdminChannelCipher _incomingCipher {};
    AdminChannelCipher _outgoingCipher {};
};

class AdminPanel final
{
public:
    AdminPanel();

    AdminPanel(const AdminPanel&) = delete;
    AdminPanel(AdminPanel&&) noexcept = delete;
    auto operator=(const AdminPanel&) = delete;
    auto operator=(AdminPanel&&) noexcept = delete;

    void MainLoop();

private:
    AdminDiscoveryClient _discovery {};
    AdminProtocolClient _connection {};
    string _selectedServerId {};
    bool _requestControl {true};
    std::array<char, 128> _manualHost {};
    int32_t _manualPort {};
    std::array<char, 128> _passwordInput {};
    std::array<char, 512> _announceInput {};
    size_t _lastRenderedEventCount {};
};

FO_END_NAMESPACE
