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
    ushort Year; // 1601 .. 30827
    ushort Month; // 1 .. 12
    ushort DayOfWeek; // 0 .. 6
    ushort Day; // 1 .. 31
    ushort Hour; // 0 .. 23
    ushort Minute; // 0 .. 59
    ushort Second; // 0 .. 59
    ushort Milliseconds; // 0 .. 999
};

class GameTimer : public NonMovable
{
public:
    GameTimer(TimerSettings& sett);
    void Reset(ushort year, ushort month, ushort day, ushort hour, ushort minute, ushort second, int multiplier);
    bool FrameAdvance();
    uint FrameTick();
    uint GameTick();
    uint GetFullSecond();
    uint EvaluateFullSecond(ushort year, ushort month, ushort day, ushort hour, ushort minute, ushort second);
    DateTimeStamp GetGameTime(uint full_second);
    uint GameTimeMonthDay(ushort year, ushort month);
    void SetGamePause(bool pause);
    bool IsGamePaused();

private:
    TimerSettings& settings;
    uint timerTick {};
    uint64 yearStartFullTime {};
    int gameTimeMultiplier {};
    bool isPaused {};
    uint gameTickBase {};
    uint gameTickFast {};
    uint fullSecondBase {};
    uint fullSecond {};
};

class Timer : public StaticClass
{
public:
    static double RealtimeTick();
    static void GetCurrentDateTime(DateTimeStamp& dt);
    static void DateTimeToFullTime(const DateTimeStamp& dt, uint64& ft);
    static void FullTimeToDateTime(uint64 ft, DateTimeStamp& dt);
    static int GetTimeDifference(const DateTimeStamp& dt1, const DateTimeStamp& dt2);
    static void ContinueTime(DateTimeStamp& dt, int seconds);
};
