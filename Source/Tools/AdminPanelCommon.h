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

FO_BEGIN_NAMESPACE

enum class AdminServerState : uint8_t
{
    Stopped = 0,
    Starting = 1,
    Running = 2,
    StartingError = 3,
};

struct AdminSnapshot
{
    bool Valid {};
    bool ServerRunning {};
    bool ServerStartingError {};
    string ServerVersion {};
    string GameVersion {};
    string CompatibilityVersion {};
    AdminServerState State {AdminServerState::Stopped};
    int64_t StateDurationMs {};
    int64_t UptimeMs {};
    int64_t DowntimeMs {};
    uint32_t Online {};
    uint32_t MaxOnline {};
    uint32_t LoopsPerSecond {};
    uint32_t Players {};
    uint32_t Critters {};
    uint32_t Locations {};
    uint32_t Maps {};
    uint32_t Items {};
    uint32_t DbCommitJobs {};
    float32_t LoopAvgMs {};
    float32_t LoopMinMs {};
    float32_t LoopMaxMs {};
    bool IsController {};
    bool HasController {};
};

struct AdminLogEntry
{
    vector<string> Lines {};
    string Source {};
    bool Local {};
};

class AdminChannelCipher final
{
public:
    void Init(string_view password) noexcept;
    void Reset() noexcept;

    [[nodiscard]] auto IsEnabled() const noexcept -> bool { return _enabled; }

    void Apply(void* data, size_t len) noexcept;

private:
    bool _enabled {};
    uint32_t _state {};
};

FO_END_NAMESPACE
