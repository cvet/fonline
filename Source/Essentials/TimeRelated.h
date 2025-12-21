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

#include "BasicCore.h"
#include "Containers.h"
#include "StrongType.h"

FO_BEGIN_NAMESPACE();

// Game time
using steady_time_point = std::chrono::time_point<std::chrono::steady_clock>;
static_assert(sizeof(steady_time_point::clock::rep) >= 8);
static_assert(std::ratio_less_equal_v<steady_time_point::clock::period, std::micro>);

///@ ExportValueType timespan timespan Layout = int64-value
class timespan
{
public:
    using underlying_type = int64;
    using resolution = std::chrono::nanoseconds;

    constexpr timespan() noexcept = default;
    timespan(const timespan& other) noexcept = default;
    timespan(timespan&& other) noexcept = default;
    auto operator=(const timespan& other) noexcept -> timespan& = default;
    auto operator=(timespan&& other) noexcept -> timespan& = default;
    ~timespan() = default;

    template<typename T, typename U>
    // ReSharper disable once CppNonExplicitConvertingConstructor
    constexpr timespan(const std::chrono::duration<T, U>& other) noexcept :
        _value {std::chrono::duration_cast<resolution>(other).count()}
    {
    }
    constexpr explicit timespan(underlying_type value) noexcept :
        _value {value}
    {
    }
    auto operator+=(const timespan& other) noexcept -> timespan&
    {
        *this = value() + other.value();
        return *this;
    }
    auto operator-=(const timespan& other) noexcept -> timespan&
    {
        *this = value() - other.value();
        return *this;
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept { return _value != 0; }
    [[nodiscard]] constexpr auto operator==(const timespan& other) const noexcept -> bool { return _value == other._value; }
    [[nodiscard]] constexpr auto operator<(const timespan& other) const noexcept -> bool { return _value < other._value; }
    [[nodiscard]] constexpr auto operator<=(const timespan& other) const noexcept -> bool { return _value <= other._value; }
    [[nodiscard]] constexpr auto operator>(const timespan& other) const noexcept -> bool { return _value > other._value; }
    [[nodiscard]] constexpr auto operator>=(const timespan& other) const noexcept -> bool { return _value >= other._value; }
    [[nodiscard]] constexpr auto operator+(const timespan& other) const noexcept -> timespan { return value() + other.value(); }
    [[nodiscard]] constexpr auto operator-(const timespan& other) const noexcept -> timespan { return value() - other.value(); }
    [[nodiscard]] constexpr auto nanoseconds() const noexcept -> underlying_type { return std::chrono::duration_cast<std::chrono::nanoseconds>(value()).count(); }
    [[nodiscard]] constexpr auto microseconds() const noexcept -> underlying_type { return std::chrono::duration_cast<std::chrono::microseconds>(value()).count(); }
    [[nodiscard]] constexpr auto milliseconds() const noexcept -> underlying_type { return std::chrono::duration_cast<std::chrono::milliseconds>(value()).count(); }
    [[nodiscard]] constexpr auto seconds() const noexcept -> underlying_type { return std::chrono::duration_cast<std::chrono::seconds>(value()).count(); }
    [[nodiscard]] constexpr auto value() const noexcept -> steady_time_point::duration { return resolution(_value); }

    template<typename T>
    [[nodiscard]] constexpr auto to_ms() const noexcept -> T
    {
        return static_cast<T>(std::chrono::duration_cast<std::chrono::milliseconds>(value()).count());
    }
    template<typename T>
    [[nodiscard]] constexpr auto to_sec() const noexcept -> T
    {
        return static_cast<T>(std::chrono::duration_cast<std::chrono::seconds>(value()).count());
    }
    template<typename T>
    [[nodiscard]] constexpr auto div(const timespan& other) const noexcept -> T
    {
        return static_cast<T>(static_cast<float64>(_value) / static_cast<float64>(other._value));
    }

    static const timespan zero;

private:
    int64 _value {};
};
static_assert(is_strong_type<timespan>);

struct time_desc_t
{
    int32 year {};
    int32 month {}; // 1..12
    int32 day {}; // 1..31
    int32 hour {}; // 0..23
    int32 minute {}; // 0..59
    int32 second {}; // 0..59
    int32 millisecond {}; // 0..999
    int32 microsecond {}; // 0..999
    int32 nanosecond {}; // 0..999
};

auto make_time_desc(timespan time_offset, bool local) -> time_desc_t;
auto make_time_offset(int32 year, int32 month, int32 day, int32 hour, int32 minute, int32 second, int32 millisecond, int32 microsecond, int32 nanosecond, bool local) -> timespan;

///@ ExportValueType nanotime nanotime Layout = int64-value
class nanotime
{
public:
    using underlying_type = int64;
    using resolution = std::chrono::nanoseconds;

    constexpr nanotime() noexcept = default;
    nanotime(const nanotime& other) noexcept = default;
    nanotime(nanotime&& other) noexcept = default;
    auto operator=(const nanotime& other) noexcept -> nanotime& = default;
    auto operator=(nanotime&& other) noexcept -> nanotime& = default;
    ~nanotime() = default;

    // ReSharper disable once CppNonExplicitConvertingConstructor
    constexpr nanotime(const steady_time_point& other) noexcept :
        _value {std::chrono::duration_cast<resolution>(other.time_since_epoch()).count()}
    {
    }
    constexpr explicit nanotime(const timespan& other) noexcept :
        _value {std::chrono::duration_cast<resolution>(other.value()).count()}
    {
    }
    constexpr explicit nanotime(underlying_type value) noexcept :
        _value {value}
    {
    }
    auto operator+=(const timespan& other) noexcept -> nanotime&
    {
        *this = value() + other.value();
        return *this;
    }
    auto operator-=(const timespan& other) noexcept -> nanotime&
    {
        *this = value() - other.value();
        return *this;
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept { return _value != 0; }
    [[nodiscard]] constexpr auto operator==(const nanotime& other) const noexcept -> bool { return _value == other._value; }
    [[nodiscard]] constexpr auto operator<(const nanotime& other) const noexcept -> bool { return _value < other._value; }
    [[nodiscard]] constexpr auto operator<=(const nanotime& other) const noexcept -> bool { return _value <= other._value; }
    [[nodiscard]] constexpr auto operator>(const nanotime& other) const noexcept -> bool { return _value > other._value; }
    [[nodiscard]] constexpr auto operator>=(const nanotime& other) const noexcept -> bool { return _value >= other._value; }
    [[nodiscard]] constexpr auto operator-(const nanotime& other) const noexcept -> timespan { return value() - other.value(); }
    [[nodiscard]] constexpr auto operator+(const timespan& other) const noexcept -> nanotime { return value() + other.value(); }
    [[nodiscard]] constexpr auto operator-(const timespan& other) const noexcept -> nanotime { return value() - other.value(); }
    [[nodiscard]] constexpr auto operator==(const steady_time_point& other) const noexcept -> bool { return value() == other; }
    [[nodiscard]] constexpr auto operator<(const steady_time_point& other) const noexcept -> bool { return value() < other; }
    [[nodiscard]] constexpr auto operator<=(const steady_time_point& other) const noexcept -> bool { return value() <= other; }
    [[nodiscard]] constexpr auto operator>(const steady_time_point& other) const noexcept -> bool { return value() > other; }
    [[nodiscard]] constexpr auto operator>=(const steady_time_point& other) const noexcept -> bool { return value() >= other; }
    [[nodiscard]] constexpr auto operator-(const steady_time_point& other) const noexcept -> timespan { return value() - other; }
    [[nodiscard]] constexpr auto nanoseconds() const noexcept -> underlying_type { return std::chrono::duration_cast<std::chrono::nanoseconds>(value().time_since_epoch()).count(); }
    [[nodiscard]] constexpr auto microseconds() const noexcept -> underlying_type { return std::chrono::duration_cast<std::chrono::microseconds>(value().time_since_epoch()).count(); }
    [[nodiscard]] constexpr auto milliseconds() const noexcept -> underlying_type { return std::chrono::duration_cast<std::chrono::milliseconds>(value().time_since_epoch()).count(); }
    [[nodiscard]] constexpr auto seconds() const noexcept -> underlying_type { return std::chrono::duration_cast<std::chrono::seconds>(value().time_since_epoch()).count(); }
    [[nodiscard]] constexpr auto value() const noexcept -> steady_time_point { return steady_time_point(resolution(_value)); }
    [[nodiscard]] constexpr auto duration_value() const noexcept -> timespan { return resolution(_value); }
    [[nodiscard]] auto desc(bool local) const -> time_desc_t { return make_time_desc(value() - now().value(), local); }

    static auto now() -> nanotime { return nanotime(steady_time_point::clock::now()); }

    static const nanotime zero;

private:
    int64 _value {};
};
static_assert(is_strong_type<nanotime>);

///@ ExportValueType synctime synctime Layout = int64-value
class synctime
{
public:
    using underlying_type = int64;
    using resolution = std::chrono::milliseconds;

    constexpr synctime() noexcept = default;
    synctime(const synctime& other) noexcept = default;
    synctime(synctime&& other) noexcept = default;
    auto operator=(const synctime& other) noexcept -> synctime& = default;
    auto operator=(synctime&& other) noexcept -> synctime& = default;
    ~synctime() = default;

    constexpr explicit synctime(const steady_time_point& other) noexcept :
        _value {std::chrono::duration_cast<resolution>(other.time_since_epoch()).count()}
    {
    }
    constexpr explicit synctime(const timespan& other) noexcept :
        _value {std::chrono::duration_cast<resolution>(other.value()).count()}
    {
    }
    constexpr explicit synctime(underlying_type value) noexcept :
        _value {value}
    {
    }
    auto operator+=(const timespan& other) noexcept -> synctime&
    {
        *this = synctime(value() + other.value());
        return *this;
    }
    auto operator-=(const timespan& other) noexcept -> synctime&
    {
        *this = synctime(value() - other.value());
        return *this;
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept { return _value != 0; }
    [[nodiscard]] constexpr auto operator==(const synctime& other) const noexcept -> bool { return _value == other._value; }
    [[nodiscard]] constexpr auto operator<(const synctime& other) const noexcept -> bool { return _value < other._value; }
    [[nodiscard]] constexpr auto operator<=(const synctime& other) const noexcept -> bool { return _value <= other._value; }
    [[nodiscard]] constexpr auto operator>(const synctime& other) const noexcept -> bool { return _value > other._value; }
    [[nodiscard]] constexpr auto operator>=(const synctime& other) const noexcept -> bool { return _value >= other._value; }
    [[nodiscard]] constexpr auto operator+(const timespan& other) const noexcept -> synctime { return synctime(value() + other.value()); }
    [[nodiscard]] constexpr auto operator-(const timespan& other) const noexcept -> synctime { return synctime(value() - other.value()); }
    [[nodiscard]] constexpr auto operator-(const synctime& other) const noexcept -> timespan { return value() - other.value(); }
    [[nodiscard]] constexpr auto nanoseconds() const noexcept -> underlying_type { return std::chrono::duration_cast<std::chrono::nanoseconds>(value().time_since_epoch()).count(); }
    [[nodiscard]] constexpr auto microseconds() const noexcept -> underlying_type { return std::chrono::duration_cast<std::chrono::microseconds>(value().time_since_epoch()).count(); }
    [[nodiscard]] constexpr auto milliseconds() const noexcept -> underlying_type { return std::chrono::duration_cast<std::chrono::milliseconds>(value().time_since_epoch()).count(); }
    [[nodiscard]] constexpr auto seconds() const noexcept -> underlying_type { return std::chrono::duration_cast<std::chrono::seconds>(value().time_since_epoch()).count(); }
    [[nodiscard]] constexpr auto value() const noexcept -> steady_time_point { return steady_time_point(resolution(_value)); }
    [[nodiscard]] constexpr auto duration_value() const noexcept -> timespan { return resolution(_value); }

    static const synctime zero;

private:
    int64 _value {};
};
static_assert(is_strong_type<synctime>);

FO_END_NAMESPACE();
template<>
struct std::formatter<FO_NAMESPACE steady_time_point::duration> : formatter<FO_NAMESPACE string_view>
{
    template<typename FormatContext>
    auto format(const FO_NAMESPACE steady_time_point::duration& value, FormatContext& ctx) const
    {
        FO_NAMESPACE string buf;

        if (value < std::chrono::milliseconds {1}) {
            const auto us = std::chrono::duration_cast<std::chrono::microseconds>(value).count() % 1000;
            const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(value).count() % 1000;
            std::format_to(std::back_inserter(buf), "{}.{:03} us", us, ns);
        }
        else if (value < std::chrono::seconds {1}) {
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(value).count() % 1000;
            const auto us = std::chrono::duration_cast<std::chrono::microseconds>(value).count() % 1000;
            std::format_to(std::back_inserter(buf), "{}.{:03} ms", ms, us);
        }
        else if (value < std::chrono::minutes {1}) {
            const auto sec = std::chrono::duration_cast<std::chrono::seconds>(value).count();
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(value).count() % 1000;
            std::format_to(std::back_inserter(buf), "{}.{:03} sec", sec, ms);
        }
        else if (value < std::chrono::hours {24}) {
            const auto hour = std::chrono::duration_cast<std::chrono::hours>(value).count();
            const auto min = std::chrono::duration_cast<std::chrono::minutes>(value).count() % 60;
            const auto sec = std::chrono::duration_cast<std::chrono::seconds>(value).count() % 60;
            std::format_to(std::back_inserter(buf), "{:02}:{:02}:{:02} sec", hour, min, sec);
        }
        else {
            const auto day = std::chrono::duration_cast<std::chrono::hours>(value).count() / 24;
            const auto hour = std::chrono::duration_cast<std::chrono::hours>(value).count() % 24;
            const auto min = std::chrono::duration_cast<std::chrono::minutes>(value).count() % 60;
            const auto sec = std::chrono::duration_cast<std::chrono::seconds>(value).count() % 60;
            std::format_to(std::back_inserter(buf), "{} day{} {:02}:{:02}:{:02} sec", day, day > 1 ? "s" : "", hour, min, sec);
        }

        return formatter<FO_NAMESPACE string_view>::format(buf, ctx);
    }
};
FO_BEGIN_NAMESPACE();

FO_END_NAMESPACE();
template<>
struct std::formatter<FO_NAMESPACE steady_time_point> : formatter<FO_NAMESPACE string_view>
{
    template<typename FormatContext>
    auto format(const FO_NAMESPACE steady_time_point& value, FormatContext& ctx) const
    {
        FO_NAMESPACE string buf;

        const auto td = FO_NAMESPACE nanotime(value).desc(true);
        std::format_to(std::back_inserter(buf), "{}-{:02}-{:02} {:02}:{:02}:{:02}", td.year, td.month, td.day, td.hour, td.minute, td.second);

        return formatter<FO_NAMESPACE string_view>::format(buf, ctx);
    }
};
FO_BEGIN_NAMESPACE();

FO_END_NAMESPACE();
template<>
struct std::formatter<FO_NAMESPACE timespan> : formatter<FO_NAMESPACE steady_time_point::duration>
{
    template<typename FormatContext>
    auto format(const FO_NAMESPACE timespan& value, FormatContext& ctx) const
    {
        return formatter<FO_NAMESPACE steady_time_point::duration>::format(value.value(), ctx);
    }
};
FO_BEGIN_NAMESPACE();

FO_END_NAMESPACE();
template<>
struct std::formatter<FO_NAMESPACE nanotime> : formatter<FO_NAMESPACE steady_time_point>
{
    template<typename FormatContext>
    auto format(const FO_NAMESPACE nanotime& value, FormatContext& ctx) const
    {
        return formatter<FO_NAMESPACE steady_time_point>::format(value.value(), ctx);
    }
};
FO_BEGIN_NAMESPACE();

FO_END_NAMESPACE();
template<>
struct std::formatter<FO_NAMESPACE synctime> : formatter<FO_NAMESPACE timespan>
{
    template<typename FormatContext>
    auto format(const FO_NAMESPACE synctime& value, FormatContext& ctx) const
    {
        return formatter<FO_NAMESPACE timespan>::format(value.duration_value(), ctx);
    }
};
FO_BEGIN_NAMESPACE();

// Time measuring
class [[nodiscard]] TimeMeter
{
public:
    TimeMeter() noexcept :
        _startTime {nanotime::now()}
    {
    }

    [[nodiscard]] auto GetDuration() const noexcept -> timespan { return nanotime::now() - _startTime; }

private:
    nanotime _startTime;
};

FO_END_NAMESPACE();
