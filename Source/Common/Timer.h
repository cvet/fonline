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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "Settings.h"

DECLARE_EXCEPTION(ServerTimeNotSetException);

class GameTimer final
{
public:
    GameTimer() = delete;
    explicit GameTimer(TimerSettings& settings);
    GameTimer(const GameTimer&) = delete;
    GameTimer(GameTimer&&) noexcept = default;
    auto operator=(const GameTimer&) = delete;
    auto operator=(GameTimer&&) noexcept = delete;
    ~GameTimer() = default;

    [[nodiscard]] auto GetFrameTime() const noexcept -> time_point_t { return _frameTime; }
    [[nodiscard]] auto GetFrameDeltaTime() const noexcept -> time_duration_t { return _frameDeltaTime; }
    [[nodiscard]] auto GetServerTime() const -> server_time_t;
    [[nodiscard]] auto GetFramesPerSecond() const noexcept -> int { return _fps; }

    void SetServerTime(server_time_t time);
    void FrameAdvance();

private:
    TimerSettings& _settings;

    time_point_t _frameTime {};
    time_duration_t _frameDeltaTime {};
    time_duration_t _debuggingOffset {};

    server_time_t _serverTime {};
    time_point_t _serverTimeSet {};

    int _fps {};
    time_point_t _fpsMeasureTime {};
    int _fpsMeasureCounter {};
};
