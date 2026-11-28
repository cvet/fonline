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

#include "AdminPanel.h"

#include <string>

FO_BEGIN_NAMESPACE

struct AdminPanelHeadlessOptions
{
    string Host {};
    uint16_t Port {};
    string Password {};
    bool AutoConnect {};
    bool RequestControl {true};
    bool DiscoveryEnabled {true};
};

enum class AdminPanelHeadlessScreen
{
    Browser,
    Server,
};

class AdminPanelHeadless final
{
public:
    explicit AdminPanelHeadless(AdminPanelHeadlessOptions options);

    AdminPanelHeadless(const AdminPanelHeadless&) = delete;
    AdminPanelHeadless(AdminPanelHeadless&&) noexcept = delete;
    auto operator=(const AdminPanelHeadless&) = delete;
    auto operator=(AdminPanelHeadless&&) noexcept = delete;

    void Run();

private:
    void UpdateDiscoveredServers();
    void ConnectSelectedServer();
    void ConnectManualServer();
    void Disconnect();
    void OpenServerPanel();
    void ReturnToBrowser(bool disconnect);

    AdminPanelHeadlessOptions _options {};
    AdminDiscoveryClient _discovery {};
    AdminProtocolClient _connection {};
    vector<DiscoveredServer> _servers {};
    std::vector<std::string> _serverEntries {"<no servers discovered>"};
    string _selectedServerId {};
    int _selectedInfoIndex {};
    int _selectedServerIndex {};
    int _selectedLogIndex {};
    int _screenIndex {static_cast<int>(AdminPanelHeadlessScreen::Browser)};
    bool _requestControl {true};
    bool _discoveryEnabled {true};
    bool _focusPasswordPending {true};
    bool _focusServerExitPending {};
    std::string _manualHost {};
    std::string _manualPort {};
    std::string _password {};
    std::string _announceInput {};
    size_t _lastEventCount {};
    std::vector<bool> _expandedLogEntries {};
};

FO_END_NAMESPACE
