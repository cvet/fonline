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

#include "TimeRelated.h"
#include "ExceptionHadling.h"
#include "GlobalData.h"

FO_BEGIN_NAMESPACE();

const timespan timespan::zero;
const nanotime nanotime::zero;
const synctime synctime::zero;

struct LocalTimeData
{
    LocalTimeData()
    {
        const auto now = std::chrono::system_clock::now();
        const auto t = std::chrono::system_clock::to_time_t(now);
        std::tm ltm = *std::localtime(&t); // NOLINT(concurrency-mt-unsafe)
        const std::time_t lt = std::mktime(&ltm);
        std::tm gtm = *std::gmtime(&lt); // NOLINT(concurrency-mt-unsafe)
        const std::time_t gt = std::mktime(&gtm);
        const int64 offset = lt - gt;
        Offset = std::chrono::seconds(offset);
    }

    std::chrono::system_clock::duration Offset {};
};
FO_GLOBAL_DATA(LocalTimeData, LocalTime);

auto make_time_desc(timespan time_offset, bool local) -> time_desc_t
{
    time_desc_t result;

    const auto now_sys = std::chrono::system_clock::now() + (local ? LocalTime->Offset : std::chrono::seconds(0));
    const auto time_sys = now_sys + std::chrono::duration_cast<std::chrono::system_clock::duration>(time_offset.value());

    const auto ymd_days = std::chrono::floor<std::chrono::days>(time_sys);
    const auto ymd = std::chrono::year_month_day(std::chrono::sys_days(ymd_days.time_since_epoch()));

    if (!ymd.ok()) {
        throw GenericException("Invalid year/month/day");
    }

    auto rest_day = time_sys - ymd_days;

    const auto hours = std::chrono::duration_cast<std::chrono::hours>(rest_day);
    rest_day -= hours;
    const auto minutes = std::chrono::duration_cast<std::chrono::minutes>(rest_day);
    rest_day -= minutes;
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(rest_day);
    rest_day -= seconds;
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(rest_day);
    rest_day -= milliseconds;
    const auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(rest_day);
    rest_day -= microseconds;
    const auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(rest_day);

    result.year = static_cast<int32>(ymd.year());
    result.month = static_cast<int32>(static_cast<uint32>(ymd.month()));
    result.day = static_cast<int32>(static_cast<uint32>(ymd.day()));
    result.hour = static_cast<int32>(hours.count());
    result.minute = static_cast<int32>(minutes.count());
    result.second = static_cast<int32>(seconds.count());
    result.millisecond = static_cast<int32>(milliseconds.count());
    result.microsecond = static_cast<int32>(microseconds.count());
    result.nanosecond = static_cast<int32>(nanoseconds.count());

    return result;
}

auto make_time_offset(int32 year, int32 month, int32 day, int32 hour, int32 minute, int32 second, int32 millisecond, int32 microsecond, int32 nanosecond, bool local) -> timespan
{
    FO_STACK_TRACE_ENTRY();

    const auto ymd = std::chrono::year_month_day {std::chrono::year {year}, std::chrono::month {static_cast<uint32>(month)}, std::chrono::day {static_cast<uint32>(day)}};

    if (!ymd.ok()) {
        throw GenericException("Invalid year/month/day");
    }

    const auto days_sys = std::chrono::sys_days {ymd};
    const auto time_of_day = std::chrono::hours {hour} + std::chrono::minutes {minute} + std::chrono::seconds {second} + std::chrono::milliseconds {millisecond} + std::chrono::microseconds {microsecond} + std::chrono::nanoseconds {nanosecond};
    const auto target_sys = std::chrono::sys_time<std::chrono::nanoseconds> {days_sys + time_of_day};
    const auto now_sys = std::chrono::system_clock::now() + (local ? LocalTime->Offset : std::chrono::seconds(0));
    const auto delta = target_sys - now_sys;

    return std::chrono::duration_cast<steady_time_point::duration>(delta);
}

FO_END_NAMESPACE();
