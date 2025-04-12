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

#include "Timer.h"
#include "WinApi-Include.h"

#if !FO_WINDOWS
#include <sys/time.h>
#endif

GameTimer::GameTimer(TimerSettings& settings) :
    _settings {settings}
{
    STACK_TRACE_ENTRY();

    _frameTime = time_point::clock::now();

    const DateTimeStamp dt = {static_cast<uint16>(_settings.StartYear), 1, 1, 0, 0, 0, 1};
    _servetTimeStartYear = Timer::DateTimeToFullTime(dt);
}

void GameTimer::SetServerTime(uint16 year, uint16 month, uint16 day, uint16 hour, uint16 minute, uint16 second, int multiplier)
{
    STACK_TRACE_ENTRY();

    _serverTimeMultiplier = multiplier;
    _gameplayTimeBase = _gameplayTimeFrame = _frameTime;

    _serverTime = _serverTimeBase = DateToServerTime(year, month, day, hour, minute, second);
}

auto GameTimer::FrameAdvance() -> bool
{
    STACK_TRACE_ENTRY();

    const auto prev_frame_time = _frameTime;
    const auto prev_gameplay_time = GameplayTime();

    const auto now_time = time_point::clock::now();

    if (IsRunInDebugger() && _settings.DebuggingDeltaTimeCap != 0) {
        const auto dt = time_duration_to_ms<uint>(now_time - _frameTime - _debuggingOffset);

        if (dt > _settings.DebuggingDeltaTimeCap) {
            _debuggingOffset += std::chrono::milliseconds {dt - _settings.DebuggingDeltaTimeCap};
        }

        _frameTime = now_time - _debuggingOffset;
    }
    else {
        _frameTime = now_time;
    }

    _frameDeltaTime = _frameTime - prev_frame_time;
    _gameplayDeltaTime = GameplayTime() - prev_gameplay_time;

    const auto whole_dt = time_duration_to_ms<tick_t::underlying_type>(GameplayTime() - _gameplayTimeBase);
    const auto server_time = tick_t {static_cast<tick_t::underlying_type>(_serverTimeBase.underlying_value() + (whole_dt / 1000 * _serverTimeMultiplier + whole_dt % 1000 * _serverTimeMultiplier / 1000))};

    if (_serverTime != server_time) {
        _serverTime = server_time;
        return true;
    }

    return false;
}

auto GameTimer::GetTime(bool gameplay_timer) const noexcept -> time_point
{
    NO_STACK_TRACE_ENTRY();

    return gameplay_timer ? GameplayTime() : FrameTime();
}

auto GameTimer::GetDeltaTime(bool gameplay_timer) const noexcept -> time_duration
{
    NO_STACK_TRACE_ENTRY();

    return gameplay_timer ? GameplayDeltaTime() : FrameDeltaTime();
}

auto GameTimer::FrameTime() const noexcept -> time_point
{
    NO_STACK_TRACE_ENTRY();

    return _frameTime;
}

auto GameTimer::FrameDeltaTime() const noexcept -> time_duration
{
    NO_STACK_TRACE_ENTRY();

    return _frameDeltaTime;
}

auto GameTimer::GameplayTime() const noexcept -> time_point
{
    NO_STACK_TRACE_ENTRY();

    return _gameplayTimeBase + (FrameTime() - _gameplayTimeFrame);
}

auto GameTimer::GameplayDeltaTime() const noexcept -> time_duration
{
    NO_STACK_TRACE_ENTRY();

    return _gameplayDeltaTime;
}

auto GameTimer::GetServerTime() const noexcept -> tick_t
{
    NO_STACK_TRACE_ENTRY();

    return _serverTime;
}

auto GameTimer::DateToServerTime(uint16 year, uint16 month, uint16 day, uint16 hour, uint16 minute, uint16 second) const noexcept -> tick_t
{
    NO_STACK_TRACE_ENTRY();

    const DateTimeStamp dt = {year, month, 0, day, hour, minute, second, 0};
    auto ft = Timer::DateTimeToFullTime(dt);
    ft -= _servetTimeStartYear;

    return tick_t {static_cast<tick_t::underlying_type>(ft / 10000000ULL)};
}

auto GameTimer::ServerToDateTime(tick_t server_time) const noexcept -> DateTimeStamp
{
    NO_STACK_TRACE_ENTRY();

    const auto ft = _servetTimeStartYear + static_cast<uint64>(server_time.underlying_value()) * 10000000ULL;

    return Timer::FullTimeToDateTime(ft);
}

auto Timer::CurTime() noexcept -> time_point
{
    NO_STACK_TRACE_ENTRY();

    return time_point::clock::now();
}

auto Timer::GetCurrentDateTime() -> DateTimeStamp
{
    STACK_TRACE_ENTRY();

    DateTimeStamp dt;

#if FO_WINDOWS
    SYSTEMTIME st;
    ::GetLocalTime(&st);

    dt.Year = st.wYear;
    dt.Month = st.wMonth;
    dt.DayOfWeek = st.wDayOfWeek;
    dt.Day = st.wDay;
    dt.Hour = st.wHour;
    dt.Minute = st.wMinute;
    dt.Second = st.wSecond;
    dt.Milliseconds = st.wMilliseconds;

#else
    time_t t;
    std::time(&t);

    const auto* lt = std::localtime(&t);
    dt.Year = static_cast<uint16>(lt->tm_year + 1900);
    dt.Month = static_cast<uint16>(lt->tm_mon + 1);
    dt.DayOfWeek = static_cast<uint16>(lt->tm_wday);
    dt.Day = static_cast<uint16>(lt->tm_mday);
    dt.Hour = static_cast<uint16>(lt->tm_hour);
    dt.Minute = static_cast<uint16>(lt->tm_min);
    dt.Second = static_cast<uint16>(lt->tm_sec);

    struct timeval tv;
    gettimeofday(&tv, nullptr);
    dt.Milliseconds = tv.tv_usec / 1000;
#endif

    return dt;
}

auto Timer::DateTimeToFullTime(const DateTimeStamp& dt) noexcept -> uint64
{
    NO_STACK_TRACE_ENTRY();

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
    ft += (static_cast<uint64>(dt.Day) - 1ULL + static_cast<uint64>(leap_days)) * 24ULL * 60ULL * 60ULL * 1000ULL * 1000ULL;
    ft += static_cast<uint64>(dt.Hour) * 60ULL * 60ULL * 1000ULL * 1000ULL;
    ft += static_cast<uint64>(dt.Minute) * 60ULL * 1000ULL * 1000ULL;
    ft += static_cast<uint64>(dt.Second) * 1000ULL * 1000ULL;
    ft += static_cast<uint64>(dt.Milliseconds) * 1000ULL;
    ft *= static_cast<uint64>(10ULL);

    return ft;
}

auto Timer::FullTimeToDateTime(uint64 ft) noexcept -> DateTimeStamp
{
    NO_STACK_TRACE_ENTRY();

    DateTimeStamp dt;

    // Base
    ft /= 10000ULL;
    dt.Milliseconds = static_cast<uint16>(ft % 1000ULL);
    ft /= 1000ULL;
    dt.Second = static_cast<uint16>(ft % 60ULL);
    ft /= 60ULL;
    dt.Minute = static_cast<uint16>(ft % 60ULL);
    ft /= 60ULL;
    dt.Hour = static_cast<uint16>(ft % 24ULL);
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

    dt.Year = static_cast<uint16>(year + 1601);
    ft = days;

    // Month
    static const uint COUNT1[13] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365};
    static const uint COUNT2[13] = {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366}; // Leap
    const auto* count = (dt.Year % 400 == 0 || (dt.Year % 4 == 0 && dt.Year % 100 != 0) ? COUNT2 : COUNT1);

    for (auto i = 0; i < 12; i++) {
        if (static_cast<uint>(ft) >= count[i] && static_cast<uint>(ft) < count[i + 1]) {
            ft -= count[i];
            dt.Month = static_cast<uint16>(i) + 1;
            break;
        }
    }

    // Day
    dt.Day = static_cast<uint16>(ft) + 1;

    // Day of week
    const auto a = (14 - dt.Month) / 12;
    const auto y = dt.Year - a;
    const auto m = dt.Month + 12 * a - 2;

    dt.DayOfWeek = (7000 + (dt.Day + y + y / 4 - y / 100 + y / 400 + 31 * m / 12)) % 7;

    return dt;
}

auto Timer::GetTimeDifference(const DateTimeStamp& dt1, const DateTimeStamp& dt2) noexcept -> int
{
    NO_STACK_TRACE_ENTRY();

    const auto ft1 = DateTimeToFullTime(dt1);
    const auto ft2 = DateTimeToFullTime(dt2);

    return static_cast<int>((ft1 - ft2) / 10000000ULL);
}

auto Timer::AdvanceTime(const DateTimeStamp& dt, int seconds) noexcept -> DateTimeStamp
{
    NO_STACK_TRACE_ENTRY();

    auto ft = DateTimeToFullTime(dt);
    ft += static_cast<uint64>(seconds) * 10000000ULL;

    return FullTimeToDateTime(ft);
}

auto Timer::GameTimeMonthDays(uint16 year, uint16 month) noexcept -> int
{
    NO_STACK_TRACE_ENTRY();

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
