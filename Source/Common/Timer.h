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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

struct DateTimeStamp
{
    uint16 Year {}; // 1601 .. 30827
    uint16 Month {}; // 1 .. 12
    uint16 DayOfWeek {}; // 0 .. 6
    uint16 Day {}; // 1 .. 31
    uint16 Hour {}; // 0 .. 23
    uint16 Minute {}; // 0 .. 59
    uint16 Second {}; // 0 .. 59
    uint16 Milliseconds {}; // 0 .. 999
};

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

    [[nodiscard]] auto FrameTime() const -> time_point;
    [[nodiscard]] auto GameplayTime() const -> time_point;

    [[nodiscard]] auto GetFullSecond() const -> tick_t;
    [[nodiscard]] auto EvaluateFullSecond(uint16 year, uint16 month, uint16 day, uint16 hour, uint16 minute, uint16 second) const -> tick_t;
    [[nodiscard]] auto EvaluateGameTime(tick_t full_second) const -> DateTimeStamp;
    [[nodiscard]] auto GameTimeMonthDays(uint16 year, uint16 month) const -> uint16;

#if FO_SINGLEPLAYER
    [[nodiscard]] auto IsGameplayPaused() const -> bool;
#endif

    void Reset(uint16 year, uint16 month, uint16 day, uint16 hour, uint16 minute, uint16 second, int multiplier);
    auto FrameAdvance() -> bool;
#if FO_SINGLEPLAYER
    void SetGameplayPause(bool pause);
#endif

private:
    TimerSettings& _settings;

    time_point _frameTime {};
    time_point _gameplayTimeBase {};
    time_point _gameplayTimeFrame {};
    time_duration _debuggingOffset {};

    uint64 _yearStartFullTime {};
    int _gameTimeMultiplier {};
    tick_t _fullSecondBase {};
    tick_t _fullSecond {};

#if FO_SINGLEPLAYER
    bool _isGameplayPaused {};
#endif
};

class Timer final
{
public:
    Timer() = delete;

    [[nodiscard]] static auto CurTime() -> time_point;
    [[nodiscard]] static auto GetCurrentDateTime() -> DateTimeStamp;
    [[nodiscard]] static auto DateTimeToFullTime(const DateTimeStamp& dt) -> uint64;
    [[nodiscard]] static auto FullTimeToDateTime(uint64 ft) -> DateTimeStamp;
    [[nodiscard]] static auto GetTimeDifference(const DateTimeStamp& dt1, const DateTimeStamp& dt2) -> int;
    [[nodiscard]] static auto AdvanceTime(const DateTimeStamp& dt, int seconds) -> DateTimeStamp;
};
