//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
    ushort Year {}; // 1601 .. 30827
    ushort Month {}; // 1 .. 12
    ushort DayOfWeek {}; // 0 .. 6
    ushort Day {}; // 1 .. 31
    ushort Hour {}; // 0 .. 23
    ushort Minute {}; // 0 .. 59
    ushort Second {}; // 0 .. 59
    ushort Milliseconds {}; // 0 .. 999
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

    [[nodiscard]] auto FrameTick() const -> uint;
    [[nodiscard]] auto GameTick() const -> uint;
    [[nodiscard]] auto GetFullSecond() const -> uint;
    [[nodiscard]] auto EvaluateFullSecond(ushort year, ushort month, ushort day, ushort hour, ushort minute, ushort second) const -> uint;
    [[nodiscard]] auto GetGameTime(uint full_second) const -> DateTimeStamp;
    [[nodiscard]] auto GameTimeMonthDay(ushort year, ushort month) const -> uint;
    [[nodiscard]] auto IsGamePaused() const -> bool;

    void Reset(ushort year, ushort month, ushort day, ushort hour, ushort minute, ushort second, int multiplier);
    auto FrameAdvance() -> bool;
    void SetGamePause(bool pause);

private:
    TimerSettings& _settings;
    uint _timerTick {};
    uint64 _yearStartFullTime {};
    int _gameTimeMultiplier {};
    bool _isPaused {};
    uint _gameTickBase {};
    uint _gameTickFast {};
    uint _fullSecondBase {};
    uint _fullSecond {};
};

class Timer final
{
public:
    Timer() = delete;

    [[nodiscard]] static auto RealtimeTick() -> double;
    [[nodiscard]] static auto GetCurrentDateTime() -> DateTimeStamp;
    [[nodiscard]] static auto DateTimeToFullTime(const DateTimeStamp& dt) -> uint64;
    [[nodiscard]] static auto FullTimeToDateTime(uint64 ft) -> DateTimeStamp;
    [[nodiscard]] static auto GetTimeDifference(const DateTimeStamp& dt1, const DateTimeStamp& dt2) -> int;
    [[nodiscard]] static auto AdvanceTime(const DateTimeStamp& dt, int seconds) -> DateTimeStamp;
};
