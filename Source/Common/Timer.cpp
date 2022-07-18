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

#include "Timer.h"
#include "WinApi-Include.h"

#if FO_WINDOWS
#include <mmsystem.h>
#else
#include <sys/time.h>
#endif

struct TimerData
{
    TimerData()
    {
#if FO_WINDOWS
        LARGE_INTEGER qpc_frequency;
        ::QueryPerformanceFrequency(&qpc_frequency);
        QpcFrequency = static_cast<double>(qpc_frequency.QuadPart);
        LARGE_INTEGER qpc_value;
        ::QueryPerformanceCounter(&qpc_value);
        InitialRealtimeTick = static_cast<double>(qpc_value.QuadPart) / QpcFrequency * 1000.0;

#elif FO_WEB
        InitialRealtimeTick = ::emscripten_get_now();

#else
        struct timeval tv;
        ::gettimeofday(&tv, nullptr);
        InitialRealtimeTick = static_cast<double>(tv.tv_sec * 1000000 + tv.tv_usec) / 1000.0;
#endif
    }

    double InitialRealtimeTick {};
#if FO_WINDOWS
    double QpcFrequency {};
#endif
};
GLOBAL_DATA(TimerData, Data);

GameTimer::GameTimer(TimerSettings& settings) : _settings {settings}
{
    Reset(static_cast<ushort>(_settings.StartYear), 1, 1, 0, 0, 0, 1);
}

void GameTimer::Reset(ushort year, ushort month, ushort day, ushort hour, ushort minute, ushort second, int multiplier)
{
#if FO_SINGLEPLAYER
    _isPaused = false;
#endif

    _gameTimeMultiplier = multiplier;
    _gameTickBase = _gameTickFast = FrameTick();

    const DateTimeStamp dt = {year, month, 0, day, hour, minute, second, 0};
    _yearStartFullTime = Timer::DateTimeToFullTime(dt);

    _fullSecond = _fullSecondBase = EvaluateFullSecond(year, month, day, hour, minute, second);
}

auto GameTimer::FrameAdvance() -> bool
{
    _timerTick = static_cast<uint>(Timer::RealtimeTick());

#if FO_SINGLEPLAYER
    if (_isPaused) {
        return false;
    }
#endif

    const auto dt = GameTick() - _gameTickBase;
    const auto fs = _fullSecondBase + (dt / 1000 * _gameTimeMultiplier + dt % 1000 * _gameTimeMultiplier / 1000);

    if (_fullSecond != fs) {
        _fullSecond = fs;
        return true;
    }

    return false;
}

auto GameTimer::FrameTick() const -> uint
{
    return _timerTick;
}

auto GameTimer::GameTick() const -> uint
{
#if FO_SINGLEPLAYER
    if (_isPaused) {
        return _gameTickBase;
    }
#endif
    return _gameTickBase + (FrameTick() - _gameTickFast);
}

auto GameTimer::GetFullSecond() const -> uint
{
    return _fullSecond;
}

auto GameTimer::EvaluateFullSecond(ushort year, ushort month, ushort day, ushort hour, ushort minute, ushort second) const -> uint
{
    const DateTimeStamp dt = {year, month, 0, day, hour, minute, second, 0};
    auto ft = Timer::DateTimeToFullTime(dt);
    ft -= _yearStartFullTime;
    return static_cast<uint>(ft / 10000000ULL);
}

auto GameTimer::GetGameTime(uint full_second) const -> DateTimeStamp
{
    const auto ft = _yearStartFullTime + static_cast<uint64>(full_second) * 10000000ULL;
    return Timer::FullTimeToDateTime(ft);
}

auto GameTimer::GameTimeMonthDay(ushort year, ushort month) const -> uint
{
    switch (month) {
    case 1:
    case 3:
    case 5:
    case 7:
    case 8:
    case 10:
    case 12: // 31
        return 31;
    case 2: // 28-29
        if (year % 400 == 0 || (year % 4 == 0 && year % 100 != 0)) {
            return 29;
        }
        return 28;
    default: // 30
        return 30;
    }
}

#if FO_SINGLEPLAYER
void GameTimer::SetGamePause(bool pause)
{
    if (_isPaused == pause) {
        return;
    }

    _gameTickBase = GameTick();
    _gameTickFast = FrameTick();
    _isPaused = pause;
}

auto GameTimer::IsGamePaused() const -> bool
{
    return _isPaused;
}
#endif

auto Timer::RealtimeTick() -> double
{
#if FO_WINDOWS
    LARGE_INTEGER qpc_value;
    ::QueryPerformanceCounter(&qpc_value);
    return static_cast<double>(qpc_value.QuadPart) / Data->QpcFrequency * 1000.0 - Data->InitialRealtimeTick;

#elif FO_WEB
    return ::emscripten_get_now() - Data->InitialRealtimeTick;

#else
    struct timeval tv;
    ::gettimeofday(&tv, nullptr);
    return static_cast<double>(tv.tv_sec * 1000000 + tv.tv_usec) / 1000.0 - Data->InitialRealtimeTick;
#endif
}

auto Timer::GetCurrentDateTime() -> DateTimeStamp
{
    DateTimeStamp dt;

#if FO_WINDOWS
    SYSTEMTIME st;
    GetLocalTime(&st);

    dt.Year = st.wYear;
    dt.Month = st.wMonth;
    dt.DayOfWeek = st.wDayOfWeek;
    dt.Day = st.wDay;
    dt.Hour = st.wHour;
    dt.Minute = st.wMinute;
    dt.Second = st.wSecond;
    dt.Milliseconds = st.wMilliseconds;

#else
    time_t long_time;
    time(&long_time);

    struct tm* lt = localtime(&long_time);
    dt.Year = lt->tm_year + 1900, dt.Month = lt->tm_mon + 1, dt.DayOfWeek = lt->tm_wday, dt.Day = lt->tm_mday, dt.Hour = lt->tm_hour, dt.Minute = lt->tm_min, dt.Second = lt->tm_sec;

    struct timeval tv;
    gettimeofday(&tv, nullptr);
    dt.Milliseconds = tv.tv_usec / 1000;
#endif

    return dt;
}

auto Timer::DateTimeToFullTime(const DateTimeStamp& dt) -> uint64
{
    // Minor year
    auto ft = static_cast<uint64>(dt.Year - 1601) * 365ULL * 24ULL * 60ULL * 60ULL * 1000ULL * 1000ULL;

    // Leap days
    uint leap_days = (dt.Year - 1601) / 4;
    leap_days += (dt.Year - 1601) / 400;
    leap_days -= (dt.Year - 1601) / 100;

    // Current month
    static const uint COUNT1[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    static const uint COUNT2[12] = {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335}; // Leap

    if (dt.Year % 400 == 0 || (dt.Year % 4 == 0 && dt.Year % 100 != 0)) {
        ft += static_cast<uint64>(COUNT2[dt.Month - 1]) * 24ULL * 60ULL * 60ULL * 1000ULL * 1000ULL;
    }
    else {
        ft += static_cast<uint64>(COUNT1[dt.Month - 1]) * 24ULL * 60ULL * 60ULL * 1000ULL * 1000ULL;
    }

    // Other calculations
    ft += static_cast<uint64>(dt.Day) - 1ULL + static_cast<uint64>(leap_days) * 24ULL * 60ULL * 60ULL * 1000ULL * 1000ULL;
    ft += static_cast<uint64>(dt.Hour) * 60ULL * 60ULL * 1000ULL * 1000ULL;
    ft += static_cast<uint64>(dt.Minute) * 60ULL * 1000ULL * 1000ULL;
    ft += static_cast<uint64>(dt.Second) * 1000ULL * 1000ULL;
    ft += static_cast<uint64>(dt.Milliseconds) * 1000ULL;
    ft *= static_cast<uint64>(10ULL);

    return ft;
}

auto Timer::FullTimeToDateTime(uint64 ft) -> DateTimeStamp
{
    DateTimeStamp dt;

    // Base
    ft /= 10000ULL;
    dt.Milliseconds = static_cast<ushort>(ft % 1000ULL);
    ft /= 1000ULL;
    dt.Second = static_cast<ushort>(ft % 60ULL);
    ft /= 60ULL;
    dt.Minute = static_cast<ushort>(ft % 60ULL);
    ft /= 60ULL;
    dt.Hour = static_cast<ushort>(ft % 24ULL);
    ft /= 24ULL;

    // Year
    auto year = static_cast<int>(ft / 365ULL);
    auto days = static_cast<int>(ft % 365ULL);

    days -= year / 4 + year / 400 - year / 100;

    while (days < 0) {
        if (year % 400 == 0 || (year % 4 == 0 && year % 100 != 0)) {
            days += 366;
        }
        else {
            days += 365;
        }
        year--;
    }

    dt.Year = static_cast<ushort>(year + 1601);
    ft = days;

    // Month
    static const uint COUNT1[13] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365};
    static const uint COUNT2[13] = {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366}; // Leap
    const auto* count = (dt.Year % 400 == 0 || (dt.Year % 4 == 0 && dt.Year % 100 != 0) ? COUNT2 : COUNT1);

    for (auto i = 0; i < 12; i++) {
        if (static_cast<uint>(ft) >= count[i] && static_cast<uint>(ft) < count[i + 1]) {
            ft -= count[i];
            dt.Month = static_cast<ushort>(i) + 1;
            break;
        }
    }

    // Day
    dt.Day = static_cast<ushort>(ft) + 1;

    // Day of week
    const auto a = (14 - dt.Month) / 12;
    const auto y = dt.Year - a;
    const auto m = dt.Month + 12 * a - 2;

    dt.DayOfWeek = (7000 + (dt.Day + y + y / 4 - y / 100 + y / 400 + 31 * m / 12)) % 7;

    return dt;
}

auto Timer::GetTimeDifference(const DateTimeStamp& dt1, const DateTimeStamp& dt2) -> int
{
    const auto ft1 = DateTimeToFullTime(dt1);
    const auto ft2 = DateTimeToFullTime(dt2);
    return static_cast<int>((ft1 - ft2) / 10000000ULL);
}

auto Timer::AdvanceTime(const DateTimeStamp& dt, int seconds) -> DateTimeStamp
{
    auto ft = DateTimeToFullTime(dt);
    ft += static_cast<uint64>(seconds) * 10000000ULL;
    return FullTimeToDateTime(ft);
}
