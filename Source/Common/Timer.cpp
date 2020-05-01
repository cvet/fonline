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

#include "Timer.h"
#include "WinApi_Include.h"

#ifdef FO_WINDOWS
#include <Mmsystem.h>
#else
#include <sys/time.h>
#endif

static double InitialRealtimeTick;
#ifdef FO_WINDOWS
static int64 QPCStartValue;
static int64 QPCFrequency;
#endif

class TimerInit
{
public:
    TimerInit()
    {
#ifdef FO_WINDOWS
        QueryPerformanceCounter((LARGE_INTEGER*)&QPCStartValue);
        QueryPerformanceFrequency((LARGE_INTEGER*)&QPCFrequency);
#endif

        InitialRealtimeTick = Timer::RealtimeTick();
    }
};
static TimerInit CrtTimerInit;

double Timer::RealtimeTick()
{
#if defined(FO_WINDOWS)
    int64 qpc_value;
    QueryPerformanceCounter((LARGE_INTEGER*)&qpc_value);
    return (double)((double)(qpc_value - QPCStartValue) / (double)QPCFrequency * 1000.0) - InitialRealtimeTick;
#elif defined(FO_WEB)
    return emscripten_get_now();
#else
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return (double)(tv.tv_sec * 1000000 + tv.tv_usec) / 1000.0 - InitialRealtimeTick;
#endif
}

void Timer::GetCurrentDateTime(DateTimeStamp& dt)
{
#ifdef FO_WINDOWS
    SYSTEMTIME st;
    GetLocalTime(&st);
    dt.Year = st.wYear, dt.Month = st.wMonth, dt.DayOfWeek = st.wDayOfWeek, dt.Day = st.wDay, dt.Hour = st.wHour,
    dt.Minute = st.wMinute, dt.Second = st.wSecond, dt.Milliseconds = st.wMilliseconds;
#else
    time_t long_time;
    time(&long_time);
    struct tm* lt = localtime(&long_time);
    dt.Year = lt->tm_year + 1900, dt.Month = lt->tm_mon + 1, dt.DayOfWeek = lt->tm_wday, dt.Day = lt->tm_mday,
    dt.Hour = lt->tm_hour, dt.Minute = lt->tm_min, dt.Second = lt->tm_sec;
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    dt.Milliseconds = tv.tv_usec / 1000;
#endif
}

#ifdef _MSC_VER
#pragma optimize("", off)
#endif
void Timer::DateTimeToFullTime(const DateTimeStamp& dt, uint64& ft)
{
    // Minor year
    ft = (uint64)(dt.Year - 1601) * 365ULL * 24ULL * 60ULL * 60ULL * 1000ULL * 1000ULL;

    // Leap days
    uint leap_days = (dt.Year - 1601) / 4;
    leap_days += (dt.Year - 1601) / 400;
    leap_days -= (dt.Year - 1601) / 100;

    // Current month
    static const uint count1[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    static const uint count2[12] = {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335}; // Leap
    if (dt.Year % 400 == 0 || (dt.Year % 4 == 0 && dt.Year % 100 != 0))
        ft += (uint64)(count2[dt.Month - 1]) * 24ULL * 60ULL * 60ULL * 1000ULL * 1000ULL;
    else
        ft += (uint64)(count1[dt.Month - 1]) * 24ULL * 60ULL * 60ULL * 1000ULL * 1000ULL;

    // Other calculations
    ft += (uint64)(dt.Day - 1 + leap_days) * 24ULL * 60ULL * 60ULL * 1000ULL * 1000ULL;
    ft += (uint64)dt.Hour * 60ULL * 60ULL * 1000ULL * 1000ULL;
    ft += (uint64)dt.Minute * 60ULL * 1000ULL * 1000ULL;
    ft += (uint64)dt.Second * 1000ULL * 1000ULL;
    ft += (uint64)dt.Milliseconds * 1000ULL;
    ft *= (uint64)10ULL;
}

void Timer::FullTimeToDateTime(uint64 ft, DateTimeStamp& dt)
{
    // Base
    ft /= 10000ULL;
    dt.Milliseconds = (ushort)(ft % 1000ULL);
    ft /= 1000ULL;
    dt.Second = (ushort)(ft % 60ULL);
    ft /= 60ULL;
    dt.Minute = (ushort)(ft % 60ULL);
    ft /= 60ULL;
    dt.Hour = (ushort)(ft % 24ULL);
    ft /= 24ULL;

    // Year
    int year = (int)(ft / 365ULL);
    int days = (int)(ft % 365ULL);
    days -= year / 4 + year / 400 - year / 100;
    while (days < 0)
    {
        if (year % 400 == 0 || (year % 4 == 0 && year % 100 != 0))
            days += 366;
        else
            days += 365;
        year--;
    }
    dt.Year = 1601 + year;
    ft = days;

    // Month
    static const uint count1[13] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365};
    static const uint count2[13] = {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366}; // Leap
    const uint* count = ((dt.Year % 400 == 0 || (dt.Year % 4 == 0 && dt.Year % 100 != 0)) ? count2 : count1);
    for (int i = 0; i < 12; i++)
    {
        if ((uint)ft >= count[i] && (uint)ft < count[i + 1])
        {
            ft -= count[i];
            dt.Month = i + 1;
            break;
        }
    }

    // Day
    dt.Day = (ushort)ft + 1;

    // Day of week
    int a = (14 - dt.Month) / 12;
    int y = dt.Year - a;
    int m = dt.Month + 12 * a - 2;
    dt.DayOfWeek = (7000 + (dt.Day + y + y / 4 - y / 100 + y / 400 + (31 * m) / 12)) % 7;
}
#ifdef _MSC_VER
#pragma optimize("", on)
#endif

int Timer::GetTimeDifference(const DateTimeStamp& dt1, const DateTimeStamp& dt2)
{
    uint64 ft1 = 0, ft2 = 0;
    DateTimeToFullTime(dt1, ft1);
    DateTimeToFullTime(dt2, ft2);
    return (int)((ft1 - ft2) / 10000000ULL);
}

void Timer::ContinueTime(DateTimeStamp& td, int seconds)
{
    uint64 ft;
    DateTimeToFullTime(td, ft);
    ft += (uint64)seconds * 10000000ULL;
    FullTimeToDateTime(ft, td);
}

GameTimer::GameTimer(TimerSettings& sett) : settings {sett}
{
    Reset(settings.StartYear, 1, 1, 0, 0, 0, 0);
}

void GameTimer::Reset(ushort year, ushort month, ushort day, ushort hour, ushort minute, ushort second, int multiplier)
{
    isPaused = false;
    gameTimeMultiplier = multiplier;
    gameTickBase = gameTickFast = FrameTick();
    DateTimeStamp dt = {year, month, 0, day, hour, minute, second, 0};
    Timer::DateTimeToFullTime(dt, yearStartFullTime);
    fullSecond = fullSecondBase = EvaluateFullSecond(year, month, day, hour, minute, second);
}

bool GameTimer::FrameAdvance()
{
    timerTick = (uint)Timer::RealtimeTick();

    if (!isPaused)
    {
        uint dt = GameTick() - gameTickBase;
        uint fs = fullSecondBase + (dt / 1000 * gameTimeMultiplier + dt % 1000 * gameTimeMultiplier / 1000);
        if (fullSecond != fs)
        {
            fullSecond = fs;
            return true;
        }
    }
    return false;
}

uint GameTimer::FrameTick()
{
    return timerTick;
}

uint GameTimer::GameTick()
{
    if (isPaused)
        return gameTickBase;
    return gameTickBase + (FrameTick() - gameTickFast);
}

uint GameTimer::GetFullSecond()
{
    return fullSecond;
}

uint GameTimer::EvaluateFullSecond(ushort year, ushort month, ushort day, ushort hour, ushort minute, ushort second)
{
    DateTimeStamp dt = {year, month, 0, day, hour, minute, second, 0};
    uint64 ft = 0;
    Timer::DateTimeToFullTime(dt, ft);
    ft -= yearStartFullTime;
    return (uint)(ft / 10000000ULL);
}

DateTimeStamp GameTimer::GetGameTime(uint full_second)
{
    uint64 ft = yearStartFullTime + uint64(full_second) * 10000000ULL;
    DateTimeStamp dt;
    Timer::FullTimeToDateTime(ft, dt);
    return dt;
}

uint GameTimer::GameTimeMonthDay(ushort year, ushort month)
{
    switch (month)
    {
    case 1:
    case 3:
    case 5:
    case 7:
    case 8:
    case 10:
    case 12: // 31
        return 31;
    case 2: // 28-29
        if (year % 400 == 0 || (year % 4 == 0 && year % 100 != 0))
            return 29;
        return 28;
    default: // 30
        return 30;
    }
    return 0;
}

void GameTimer::SetGamePause(bool pause)
{
    if (isPaused == pause)
        return;

    gameTickBase = GameTick();
    gameTickFast = FrameTick();
    isPaused = pause;
}

bool GameTimer::IsGamePaused()
{
    return isPaused;
}
