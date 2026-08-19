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

class ServerEngine;

struct AdminServerHostCallbacks
{
    function<ServerEngine*()> GetServer {};
    function<void()> StartServer {};
    function<void()> StopServer {};
    function<void()> RestartServer {};
};

class AdminServerHost final
{
public:
    AdminServerHost(string_view log_callback_key, AdminServerHostCallbacks callbacks);
    ~AdminServerHost();

    AdminServerHost(const AdminServerHost&) = delete;
    AdminServerHost(AdminServerHost&&) noexcept = delete;
    auto operator=(const AdminServerHost&) = delete;
    auto operator=(AdminServerHost&&) noexcept = delete;

    void Tick();
    void OnLog(string_view text);

private:
    [[nodiscard]] auto EvaluateServerState() const -> AdminServerState;
    void UpdateServerState();
    [[nodiscard]] auto GetStateDurationMs() const -> int64_t;

    struct Session
    {
        tcp_socket Socket {};
        bool Authorized {};
        bool WantsControl {};
        bool IsController {};
        bool Closed {};
        string RemoteHost {};
        uint16_t RemotePort {};
        string IncomingData {};
        string OutgoingData {};
        size_t OutgoingPos {};
        size_t NextLogIndex {};
        nanotime NextSnapshotTime {};
        AdminChannelCipher IncomingCipher {};
        AdminChannelCipher OutgoingCipher {};
    };

    void AppendOutgoing(Session& session, string_view text);
    void QueueSessionLine(Session& session, string_view prefix, string_view text);
    void QueueSessionLogEntry(Session& session, const AdminLogEntry& entry);
    void QueueSnapshot(Session& session);
    void FlushSession(Session& session);
    void SendLogBacklog(Session& session);
    void UpdateSessionLogs(Session& session);
    auto GrantControl(Session& session) -> bool;
    void ReleaseControl(Session& session);
    auto MakeSessionsStatus() -> string;
    void ExecuteCommand(Session& session, string_view command);
    void HandleSessionLine(Session& session, string_view line);
    void ReceiveSessionLines(Session& session);

    string _logCallbackKey {};
    AdminServerHostCallbacks _callbacks {};
    tcp_server _listener {};
    udp_socket _discoverySocket {};
    bool _listenerReady {};
    bool _discoveryReady {};
    std::mutex _logLocker {};
    std::deque<AdminLogEntry> _logLines {};
    size_t _logLineCount {};
    size_t _logOffset {};
    std::list<Session> _sessions {};
    Session* _controllerSession {};
    AdminServerState _serverState {AdminServerState::Stopped};
    nanotime _serverStateChangedAt {};
    AdminSnapshot _lastSnapshot {};
    bool _lastSnapshotValid {};
};

FO_END_NAMESPACE
