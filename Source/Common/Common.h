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
// Copyright (c) 2006 - 2024, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

// Todo: make entities positioning free in space, without hard-linking to hex
// Todo: add third 'up' coordinate to positioning that allow create multidimensional maps
// Todo: use smart pointers instead raw
// Todo: fix all PVS Studio warnings
// Todo: SHA replace to openssl SHA
// Todo: wrap fonline code to namespace
// Todo: ident_t 8 byte integer
// Todo: hash_t 8 byte integer
// Todo: tick_t 8 byte integer
// Todo: c-style arrays to std::array
// Todo: use more noexcept
// Todo: use more constexpr
// Todo: improve BitReader/BitWriter to better network/disk space utilization
// Todo: improve custom exceptions for every subsustem
// Todo: temporary entities, disable writing to data base
// Todo: move all return values from out refs to return values as tuple and nodiscard (and then use structuured binding)
// Todo: split meanings of int8/char and uint8/byte in code

// ReSharper disable CppClangTidyCppcoreguidelinesMacroUsage
// ReSharper disable CppInconsistentNaming

#pragma once

#ifndef FO_PRECOMPILED_HEADER_GUARD
#define FO_PRECOMPILED_HEADER_GUARD

// Operating system (passed outside)
#if FO_WINDOWS + FO_LINUX + FO_MAC + FO_ANDROID + FO_IOS + FO_WEB + FO_PS4 == 0
#error "Operating system not specified"
#endif
#if FO_WINDOWS + FO_LINUX + FO_MAC + FO_ANDROID + FO_IOS + FO_WEB + FO_PS4 != 1
#error "Multiple operating systems not allowed"
#endif

#if __cplusplus < 201703L
#error "Invalid __cplusplus version, must be at least C++17 (201703)"
#endif

#if __cplusplus >= 202002L
#define CPLUSPLUS_20 1
#else
#define CPLUSPLUS_20 0
#endif

// Standard API
#include <algorithm>
#include <any>
#include <array>
#include <atomic>
#include <cassert>
#include <cfloat>
#include <charconv>
#include <chrono>
#include <climits>
#include <clocale>
#include <cmath>
#include <condition_variable>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <forward_list>
#include <functional>
#include <future>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#define GCH_SMALL_VECTOR_DEFAULT_SIZE 64
#define GCH_UNRESTRICTED_DEFAULT_BUFFER_SIZE
#include <small_vector.hpp>

// OS specific API
#if FO_MAC || FO_IOS
#include <TargetConditionals.h>
#elif FO_WEB
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

// Compiler warnings disable helper
#if defined(_MSC_VER)
#define DISABLE_WARNINGS_PUSH() __pragma(warning(push, 0))
#define DISABLE_WARNINGS_POP() __pragma(warning(pop))
#elif defined(__clang__)
#define DISABLE_WARNINGS_PUSH() _Pragma("clang diagnostic push") _Pragma("clang diagnostic ignored \"-Weverything\"")
#define DISABLE_WARNINGS_POP() _Pragma("clang diagnostic pop")
#else
#define DISABLE_WARNINGS_PUSH()
#define DISABLE_WARNINGS_POP()
#endif

// String formatting lib
#if CPLUSPLUS_20
#include <format>
#define FMTNS std
#else
DISABLE_WARNINGS_PUSH()
#include "fmt/chrono.h"
#include "fmt/format.h"
DISABLE_WARNINGS_POP()
#define FMTNS fmt
#endif

// Span
#if CPLUSPLUS_20
#include <span>
using std::span;
#else
#include <span.hpp>
using tcb::span;
#endif

template<typename T>
using const_span = span<const T>;

// WinAPI implicitly included in WinRT so add it globally for macro undefining
#if FO_UWP
#include "WinApiUndef-Include.h"
#endif

// Base types
using int8 = std::int8_t;
using uint8 = std::uint8_t;
using int16 = std::int16_t;
using uint16 = std::uint16_t;
using uint = std::uint32_t;
using int64 = std::int64_t;
using uint64 = std::uint64_t;

// Check the sizes of base types
static_assert(sizeof(int8) == 1);
static_assert(sizeof(uint8) == 1);
static_assert(sizeof(int16) == 2);
static_assert(sizeof(uint16) == 2);
static_assert(sizeof(int) == 4);
static_assert(sizeof(uint) == 4);
static_assert(sizeof(int64) == 8);
static_assert(sizeof(uint64) == 8);
static_assert(sizeof(bool) == 1);
static_assert(sizeof(long long) >= 8);
static_assert(CHAR_BIT == 8);

// Bind to global scope frequently used types
using std::array;
using std::deque;
using std::forward_list;
using std::initializer_list;
using std::istringstream;
using std::list;
using std::optional;
using std::pair;
using std::set;
using std::shared_ptr;
using std::string;
using std::string_view;
using std::tuple;
using std::type_index;
using std::type_info;
using std::unique_ptr;
using std::unordered_map;
using std::unordered_multimap;
using std::unordered_set;
using std::variant;
using std::weak_ptr;

template<typename K, typename V>
using map = std::map<K, V, std::less<>>;
template<typename K, typename V>
using multimap = std::multimap<K, V, std::less<>>;

#if !FO_DEBUG
template<typename T>
using vector = gch::small_vector<T>;
#else
template<typename T>
using vector = std::vector<T>; // To see vector entries in debugger
#endif
using gch::small_vector;

template<typename T>
using unique_del_ptr = unique_ptr<T, std::function<void(T*)>>;

template<class T>
struct release_delete
{
    constexpr release_delete() noexcept = default;
    void operator()(T* p) const noexcept
    {
        if (p != nullptr) {
            p->Release();
        }
    }
};

template<typename T>
using unique_release_ptr = unique_ptr<T, release_delete<T>>;

template<typename T, typename U>
std::unique_ptr<T> dynamic_pointer_cast(std::unique_ptr<U>&& p) noexcept
{
    if (T* casted = dynamic_cast<T*>(p.get())) {
        (void)p.release();
        return std::unique_ptr<T> {casted};
    }
    return {};
}

template<typename T>
inline constexpr void make_if_not_exists(unique_ptr<T>& ptr)
{
    if (!ptr) {
        ptr = std::make_unique<T>();
    }
}

template<typename T>
inline constexpr void destroy_if_empty(unique_ptr<T>& ptr) noexcept
{
    if (ptr && ptr->empty()) {
        ptr.reset();
    }
}

struct pair_hash
{
    template<typename T, typename U>
    std::size_t operator()(const std::pair<T, U>& p) const noexcept
    {
        return std::hash<T> {}(p.first) ^ std::hash<U> {}(p.second);
    }
};

// Todo: improve named enums
template<typename T>
struct FMTNS::formatter<T, std::enable_if_t<std::is_enum_v<T>, char>> : formatter<std::underlying_type_t<T>>
{
    template<typename FormatContext>
    auto format(const T& value, FormatContext& ctx) const
    {
        return formatter<std::underlying_type_t<T>>::format(static_cast<std::underlying_type_t<T>>(value), ctx);
    }
};

// Strong types
template<typename T>
struct strong_type
{
    using underlying_type = typename T::type;
    static constexpr bool is_strong_type = true;
    static constexpr const char* type_name = T::name;

    constexpr strong_type() noexcept :
        _value {}
    {
    }

    constexpr explicit strong_type(underlying_type v) noexcept :
        _value {v}
    {
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept { return _value != underlying_type {}; }
    [[nodiscard]] constexpr auto operator==(const strong_type& other) const noexcept -> bool { return _value == other._value; }
    [[nodiscard]] constexpr auto operator!=(const strong_type& other) const noexcept -> bool { return _value != other._value; }
    [[nodiscard]] constexpr auto underlying_value() noexcept -> underlying_type& { return _value; }
    [[nodiscard]] constexpr auto underlying_value() const noexcept -> const underlying_type& { return _value; }

private:
    underlying_type _value;
};

template<typename T, typename = int>
struct has_is_strong_type : std::false_type
{
};
template<typename T>
struct has_is_strong_type<T, decltype((void)T::is_strong_type, 0)> : std::true_type
{
};
template<typename T>
constexpr bool is_strong_type_v = has_is_strong_type<T>::value;

template<typename T>
struct std::hash<strong_type<T>> // NOLINT(cert-dcl58-cpp)
{
    size_t operator()(const strong_type<T>& t) const noexcept { return std::hash<typename strong_type<T>::underlying_type>()(t.underlying_value()); }
};

template<typename T>
struct FMTNS::formatter<T, std::enable_if_t<is_strong_type_v<T>, char>> : formatter<typename T::underlying_type>
{
    template<typename FormatContext>
    auto format(const T& value, FormatContext& ctx) const
    {
        return formatter<typename T::underlying_type>::format(value.underlying_value(), ctx);
    }
};

///@ ExportType ident ident_t uint HardStrong
#define IDENT_T_NAME "ident"
struct ident_t_traits
{
    static constexpr const char* name = IDENT_T_NAME;
    using type = uint;
};
using ident_t = strong_type<ident_t_traits>;
static_assert(sizeof(ident_t) == sizeof(uint));
static_assert(std::is_standard_layout_v<ident_t>);

///@ ExportType tick_t tick_t uint RelaxedStrong
#define TICK_T_NAME "tick_t"
struct tick_t_traits
{
    static constexpr const char* name = TICK_T_NAME;
    using type = uint;
};
using tick_t = strong_type<tick_t_traits>;
static_assert(sizeof(tick_t) == sizeof(uint));
static_assert(std::is_standard_layout_v<tick_t>);

// Custom any as string
class any_t : public string
{
};

template<>
struct FMTNS::formatter<any_t> : formatter<string_view>
{
    template<typename FormatContext>
    auto format(const any_t& value, FormatContext& ctx) const
    {
        return formatter<string_view>::format(static_cast<const string&>(value), ctx);
    }
};

// Game clock
using time_point = std::chrono::time_point<std::chrono::steady_clock>;
using time_duration = time_point::clock::duration;
static_assert(sizeof(time_point::clock::rep) >= 8);
static_assert(std::ratio_less_equal_v<time_point::clock::period, std::micro>);

inline auto time_point_to_unix_time(const time_point& t) -> time_t
{
    const auto system_clock_now = std::chrono::system_clock::now();
    const auto system_clock_time = system_clock_now + std::chrono::duration_cast<std::chrono::system_clock::duration>(t - time_point::clock::now());
    const auto unix_time = std::chrono::system_clock::to_time_t(system_clock_time);
    return unix_time;
}

struct time_point_desc_t
{
    int Year {};
    int Month {}; // 1..12
    int Day {}; // 1..31
    int Hour {}; // 0..23
    int Minute {}; // 0..59
    int Second {}; // 0..59
    int Millisecond {}; // 0..999
};

inline auto time_point_desc(const time_point& t) -> time_point_desc_t
{
    time_point_desc_t result;

#if CPLUSPLUS_20
    const auto ymd_days = std::chrono::floor<std::chrono::days>(t);
    const auto ymd = std::chrono::year_month_day(std::chrono::sys_days(ymd_days.time_since_epoch()));

    auto rest_day = t - ymd_days;

    const auto hours = std::chrono::duration_cast<std::chrono::hours>(rest_day);
    rest_day -= hours;
    const auto minutes = std::chrono::duration_cast<std::chrono::minutes>(rest_day);
    rest_day -= minutes;
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(rest_day);
    rest_day -= seconds;
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(rest_day);

    result.Year = static_cast<int>(ymd.year());
    result.Month = static_cast<int>(static_cast<uint>(ymd.month()));
    result.Day = static_cast<int>(static_cast<uint>(ymd.day()));
    result.Hour = static_cast<int>(hours.count());
    result.Minute = static_cast<int>(minutes.count());
    result.Second = static_cast<int>(seconds.count());
    result.Millisecond = static_cast<int>(milliseconds.count());
#else

    const auto unix_time = time_point_to_unix_time(t);
    const auto tm_struct = fmt::localtime(unix_time);

    result.Year = tm_struct.tm_year;
    result.Month = tm_struct.tm_mon;
    result.Day = tm_struct.tm_mday;
    result.Hour = tm_struct.tm_hour;
    result.Minute = tm_struct.tm_min;
    result.Second = tm_struct.tm_sec;
    result.Millisecond = 0;
#endif

    return result;
}

template<typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
inline constexpr auto time_duration_to_ms(const time_duration& duration) -> T
{
    return static_cast<T>(std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
}

inline constexpr auto time_duration_div(time_duration duration1, time_duration duration2) -> float
{
    return static_cast<float>(static_cast<double>(duration1.count()) / static_cast<double>(duration2.count()));
}

template<>
struct FMTNS::formatter<time_duration> : formatter<string_view>
{
    template<typename FormatContext>
    auto format(const time_duration& value, FormatContext& ctx) const
    {
        string buf;

        if (value < std::chrono::milliseconds {1}) {
            const auto us = std::chrono::duration_cast<std::chrono::microseconds>(value).count() % 1000;
            const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(value).count() % 1000;
            FMTNS::format_to(std::back_inserter(buf), "{}.{:03} us", us, ns);
        }
        else if (value < std::chrono::seconds {1}) {
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(value).count() % 1000;
            const auto us = std::chrono::duration_cast<std::chrono::microseconds>(value).count() % 1000;
            FMTNS::format_to(std::back_inserter(buf), "{}.{:03} ms", ms, us);
        }
        else if (value < std::chrono::minutes {1}) {
            const auto sec = std::chrono::duration_cast<std::chrono::seconds>(value).count();
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(value).count() % 1000;
            FMTNS::format_to(std::back_inserter(buf), "{}.{:03} sec", sec, ms);
        }
        else if (value < std::chrono::hours {24}) {
            const auto hour = std::chrono::duration_cast<std::chrono::hours>(value).count();
            const auto min = std::chrono::duration_cast<std::chrono::minutes>(value).count() % 60;
            const auto sec = std::chrono::duration_cast<std::chrono::seconds>(value).count() % 60;
            FMTNS::format_to(std::back_inserter(buf), "{:02}:{:02}:{:02} sec", hour, min, sec);
        }
        else {
            const auto day = std::chrono::duration_cast<std::chrono::hours>(value).count() / 24;
            const auto hour = std::chrono::duration_cast<std::chrono::hours>(value).count() % 24;
            const auto min = std::chrono::duration_cast<std::chrono::minutes>(value).count() % 60;
            const auto sec = std::chrono::duration_cast<std::chrono::seconds>(value).count() % 60;
            FMTNS::format_to(std::back_inserter(buf), "{} day{} {:02}:{:02}:{:02} sec", day, day > 1 ? "s" : "", hour, min, sec);
        }

        return formatter<string_view>::format(buf, ctx);
    }
};

template<>
struct FMTNS::formatter<time_point> : formatter<string_view>
{
    template<typename FormatContext>
    auto format(const time_point& value, FormatContext& ctx) const
    {
        string buf;

        const auto td = time_point_desc(value);
        FMTNS::format_to(std::back_inserter(buf), "{}-{:02}-{:02} {:02}:{:02}:{:02}", td.Year, td.Month, td.Day, td.Hour, td.Minute, td.Second);

        return formatter<string_view>::format(buf, ctx);
    }
};

// Math types
// Todo: replace depedency from Assimp types (matrix/vector/quaternion/color)
#include "assimp/types.h"
using vec3 = aiVector3t<float>;
using dvec3 = aiVector3t<double>;
using mat44 = aiMatrix4x4t<float>;
using dmat44 = aiMatrix4x4t<double>;
using quaternion = aiQuaterniont<float>;
using dquaternion = aiQuaterniont<double>;
using color4 = aiColor4t<float>;
using dcolor4 = aiColor4t<double>;

// Template helpers
#define TEMPLATE_HAS_MEMBER(name, member) \
    template<typename T> \
    class name \
    { \
        using one = char; \
        struct two \
        { \
            char x[2]; \
        }; \
        template<typename C> \
        static auto test(decltype(&C::member)) -> one; \
        template<typename C> \
        static auto test(...) -> two; \
\
    public: \
        enum \
        { \
            value = sizeof(test<T>(0)) == sizeof(char) \
        }; \
    };

TEMPLATE_HAS_MEMBER(has_size, size);
TEMPLATE_HAS_MEMBER(has_inlined, inlined); // small_vector test

#undef TEMPLATE_HAS_MEMBER

template<typename Test, template<typename...> typename Ref>
struct is_specialization : std::false_type
{
};

template<template<typename...> typename Ref, typename... Args>
struct is_specialization<Ref<Args...>, Ref> : std::true_type
{
};

template<typename T>
static constexpr bool is_vector_v = is_specialization<T, std::vector>::value || has_inlined<T>::value;
template<typename T>
static constexpr bool is_map_v = is_specialization<T, std::map>::value || is_specialization<T, std::unordered_map>::value;

// Profiling & stack trace obtaining
#define CONCAT(x, y) CONCAT_INDIRECT(x, y)
#define CONCAT_INDIRECT(x, y) x##y
#if defined(__GNUC__)
#define FORCE_INLINE __attribute__((always_inline)) inline
#elif defined(_MSC_VER)
#define FORCE_INLINE __forceinline
#else
#define FORCE_INLINE inline
#endif

// Todo: improve automatic checker of STACK_TRACE_ENTRY/NO_STACK_TRACE_ENTRY in every .cpp function
#if FO_TRACY
#ifndef TRACY_ENABLE
#error TRACY_ENABLE not defined
#endif

#include "tracy/Tracy.hpp"
#include "tracy/TracyC.h"

using tracy::SourceLocationData;

#if !FO_NO_MANUAL_STACK_TRACE
#define STACK_TRACE_ENTRY() \
    ZoneScoped; \
    auto ___fo_stack_entry = StackTraceScopeEntry(TracyConcat(__tracy_source_location, __LINE__))
#define STACK_TRACE_ENTRY_NAMED(name) \
    ZoneScopedN(name); \
    auto ___fo_stack_entry = StackTraceScopeEntry(TracyConcat(__tracy_source_location, __LINE__))
#else
#define STACK_TRACE_ENTRY() ZoneScoped
#define STACK_TRACE_ENTRY_NAMED(name) ZoneScopedN(name)
#endif
#define NO_STACK_TRACE_ENTRY()

#else
struct SourceLocationData // Same as tracy::SourceLocationData
{
    const char* name;
    const char* function;
    const char* file;
    uint32_t line;
};

#if !FO_NO_MANUAL_STACK_TRACE
#define STACK_TRACE_ENTRY() \
    static constexpr SourceLocationData CONCAT(___fo_source_location, __LINE__) {nullptr, __FUNCTION__, __FILE__, (uint32_t)__LINE__}; \
    auto ___fo_stack_entry = StackTraceScopeEntry(CONCAT(___fo_source_location, __LINE__))
#define STACK_TRACE_ENTRY_NAMED(name) \
    static constexpr SourceLocationData CONCAT(___fo_source_location, __LINE__) {nullptr, name, __FILE__, (uint32_t)__LINE__}; \
    auto ___fo_stack_entry = StackTraceScopeEntry(CONCAT(___fo_source_location, __LINE__))
#else
#define STACK_TRACE_ENTRY()
#define STACK_TRACE_ENTRY_NAMED(name)
#endif
#define NO_STACK_TRACE_ENTRY()
#endif

extern void PushStackTrace(const SourceLocationData& loc) noexcept;
extern void PopStackTrace() noexcept;
extern auto GetStackTrace() -> string;
extern auto GetStackTraceEntry(size_t deep) noexcept -> const SourceLocationData*;

struct StackTraceScopeEntry
{
    FORCE_INLINE explicit StackTraceScopeEntry(const SourceLocationData& loc) noexcept { PushStackTrace(loc); }
    FORCE_INLINE ~StackTraceScopeEntry() noexcept { PopStackTrace(); }

    StackTraceScopeEntry(const StackTraceScopeEntry&) = delete;
    StackTraceScopeEntry(StackTraceScopeEntry&&) noexcept = delete;
    auto operator=(const StackTraceScopeEntry&) -> StackTraceScopeEntry& = delete;
    auto operator=(StackTraceScopeEntry&&) noexcept -> StackTraceScopeEntry& = delete;
};

// Engine exception handling
extern auto GetRealStackTrace() -> string;
extern auto IsRunInDebugger() noexcept -> bool;
extern auto BreakIntoDebugger(string_view error_message = "") noexcept -> bool;
extern void CreateDumpMessage(string_view appendix, string_view message);
[[noreturn]] extern void ReportExceptionAndExit(const std::exception& ex) noexcept;
extern void ReportExceptionAndContinue(const std::exception& ex) noexcept;
extern void ShowExceptionMessageBox(bool enabled) noexcept;
[[noreturn]] extern void ReportStrongAssertAndExit(string_view message, const char* file, int line) noexcept;
extern void ReportVerifyFailed(string_view message, const char* file, int line) noexcept;

struct ExceptionStackTraceData
{
    string StackTrace {};
};

// Todo: pass name to exceptions context args
#define DECLARE_EXCEPTION(exception_name) \
    class exception_name : public BaseEngineException \
    { \
    public: \
        exception_name() = delete; \
        auto operator=(const exception_name&) = delete; \
        auto operator=(exception_name&&) noexcept = delete; \
        ~exception_name() override = default; \
        template<typename... Args> \
        explicit exception_name(string_view message, Args&&... args) noexcept : \
            BaseEngineException(#exception_name, nullptr, message, std::forward<Args>(args)...) \
        { \
        } \
        template<typename... Args> \
        exception_name(ExceptionStackTraceData data, string_view message, Args&&... args) noexcept : \
            BaseEngineException(#exception_name, &data, message, std::forward<Args>(args)...) \
        { \
        } \
        exception_name(const exception_name& other) noexcept : \
            BaseEngineException(other) \
        { \
        } \
        exception_name(exception_name&& other) noexcept : \
            BaseEngineException(other) \
        { \
        } \
    }

class BaseEngineException : public std::exception
{
public:
    BaseEngineException() = delete;
    auto operator=(const BaseEngineException&) = delete;
    auto operator=(BaseEngineException&&) noexcept = delete;
    ~BaseEngineException() override = default;

    template<typename... Args>
    explicit BaseEngineException(const char* name, ExceptionStackTraceData* st_data, string_view message, Args&&... args) noexcept :
        _name {name}
    {
        try {
            _message = _name;
            _message.append(": ");
            _message.append(message);

            const vector<string> params = {FMTNS::format("{}", std::forward<Args>(args))...};

            for (const auto& param : params) {
                _message.append("\n- ");
                _message.append(param);
            }
        }
        catch (...) {
            // Do nothing
        }

        if (st_data != nullptr) {
            _stackTrace = std::move(st_data->StackTrace);
        }
        else {
            try {
                _stackTrace = GetStackTrace();
            }
            catch (...) {
                // Do nothing
            }
        }
    }

    BaseEngineException(const BaseEngineException& other) noexcept :
        std::exception(other),
        _name {other._name}
    {
        try {
            _message = other._message;
        }
        catch (...) {
            // Do nothing
        }

        try {
            _stackTrace = other._stackTrace;
        }
        catch (...) {
            // Do nothing
        }
    }

    BaseEngineException(BaseEngineException&& other) noexcept :
        std::exception(other),
        _name {other._name}
    {
        _message = std::move(other._message);
        _stackTrace = std::move(other._stackTrace);
    }

    [[nodiscard]] auto what() const noexcept -> const char* override { return !_message.empty() ? _message.c_str() : _name; }
    [[nodiscard]] auto StackTrace() const noexcept -> const string& { return _stackTrace; }

private:
    const char* _name;
    string _message {};
    string _stackTrace {};
};

#if !FO_NO_EXTRA_ASSERTS
#define RUNTIME_ASSERT(expr) \
    if (!(expr)) { \
        throw AssertationException(#expr, __FILE__, __LINE__); \
    }
#define RUNTIME_ASSERT_STR(expr, str) \
    if (!(expr)) { \
        throw AssertationException(str, __FILE__, __LINE__); \
    }
#define RUNTIME_VERIFY(expr, ...) \
    if (!(expr)) { \
        ReportVerifyFailed(#expr, __FILE__, __LINE__); \
        return __VA_ARGS__; \
    }
#else
#define RUNTIME_ASSERT(expr)
#define RUNTIME_ASSERT_STR(expr, str)
#define RUNTIME_VERIFY(expr, ...)
#endif

#if FO_DEBUG
#define STRONG_ASSERT(expr) \
    if (!(expr)) { \
        ReportStrongAssertAndExit(#expr, __FILE__, __LINE__); \
    }
#else
#define STRONG_ASSERT(expr)
#endif

#define UNKNOWN_EXCEPTION() ReportStrongAssertAndExit("Unknown exception", __FILE__, __LINE__)

// Common exceptions
DECLARE_EXCEPTION(GenericException);
DECLARE_EXCEPTION(AssertationException);
DECLARE_EXCEPTION(StrongAssertationException);
DECLARE_EXCEPTION(VerifyFailedException);
DECLARE_EXCEPTION(UnreachablePlaceException);
DECLARE_EXCEPTION(NotSupportedException);
DECLARE_EXCEPTION(NotImplementedException);
DECLARE_EXCEPTION(InvalidCallException);
DECLARE_EXCEPTION(NotEnabled3DException);
DECLARE_EXCEPTION(InvalidOperationException);

// Event system
class EventUnsubscriberCallback final
{
    template<typename...>
    friend class EventObserver;
    friend class EventUnsubscriber;

public:
    EventUnsubscriberCallback() = delete;
    EventUnsubscriberCallback(const EventUnsubscriberCallback&) = delete;
    EventUnsubscriberCallback(EventUnsubscriberCallback&&) = default;
    auto operator=(const EventUnsubscriberCallback&) = delete;
    auto operator=(EventUnsubscriberCallback&&) noexcept -> EventUnsubscriberCallback& = default;
    ~EventUnsubscriberCallback() = default;

private:
    using Callback = std::function<void()>;
    explicit EventUnsubscriberCallback(Callback cb) :
        _unsubscribeCallback {std::move(cb)}
    {
    }
    Callback _unsubscribeCallback {};
};

class EventUnsubscriber final
{
    template<typename...>
    friend class EventObserver;

public:
    EventUnsubscriber() = default;
    EventUnsubscriber(const EventUnsubscriber&) = delete;
    EventUnsubscriber(EventUnsubscriber&&) = default;
    auto operator=(const EventUnsubscriber&) = delete;
    auto operator=(EventUnsubscriber&&) noexcept -> EventUnsubscriber& = default;
    ~EventUnsubscriber() { Unsubscribe(); }

    auto operator+=(EventUnsubscriberCallback&& cb) -> EventUnsubscriber&
    {
        _unsubscribeCallbacks.push_back(std::move(cb));
        return *this;
    }

    void Unsubscribe() noexcept
    {
        const auto callbacks = std::move(_unsubscribeCallbacks);
        _unsubscribeCallbacks.clear();

        for (const auto& cb : callbacks) {
            try {
                cb._unsubscribeCallback();
            }
            catch (const std::exception& ex) {
                ReportExceptionAndContinue(ex);
            }
            catch (...) {
                UNKNOWN_EXCEPTION();
            }
        }
    }

private:
    using Callback = std::function<void()>;
    explicit EventUnsubscriber(EventUnsubscriberCallback cb) { _unsubscribeCallbacks.push_back(std::move(cb)); }
    vector<EventUnsubscriberCallback> _unsubscribeCallbacks {};
};

template<typename... Args>
class EventObserver final
{
    template<typename...>
    friend class EventDispatcher;

public:
    using Callback = std::function<void(Args...)>;

    EventObserver() = default;
    EventObserver(const EventObserver&) = delete;
    EventObserver(EventObserver&&) noexcept = default;
    auto operator=(const EventObserver&) = delete;
    auto operator=(EventObserver&&) noexcept = delete;

    ~EventObserver()
    {
        if (!_subscriberCallbacks.empty()) {
            try {
                throw GenericException("Some of subscriber still alive", _subscriberCallbacks.size());
            }
            catch (const std::exception& ex) {
                ReportExceptionAndContinue(ex);
            }
            catch (...) {
                UNKNOWN_EXCEPTION();
            }
        }
    }

    [[nodiscard]] auto operator+=(Callback cb) -> EventUnsubscriberCallback
    {
        auto it = _subscriberCallbacks.insert(_subscriberCallbacks.end(), cb);
        return EventUnsubscriberCallback([this, it]() { _subscriberCallbacks.erase(it); });
    }

private:
    list<Callback> _subscriberCallbacks {};
};

template<typename... Args>
class EventDispatcher final
{
public:
    using ObserverType = EventObserver<Args...>;

    EventDispatcher() = delete;
    explicit EventDispatcher(ObserverType& obs) :
        _observer {obs}
    {
    }
    EventDispatcher(const EventDispatcher&) = delete;
    EventDispatcher(EventDispatcher&&) noexcept = default;
    auto operator=(const EventDispatcher&) = delete;
    auto operator=(EventDispatcher&&) noexcept -> EventDispatcher& = default;
    ~EventDispatcher() = default;

    auto operator()(Args&&... args) -> EventDispatcher&
    {
        // Todo: recursion guard for EventDispatcher
        if (!_observer._subscriberCallbacks.empty()) {
            for (auto& cb : _observer._subscriberCallbacks) {
                cb(std::forward<Args>(args)...);
            }
        }
        return *this;
    }

private:
    ObserverType& _observer;
};

// C-strings literal helpers
constexpr auto const_hash(const char* input) noexcept -> uint
{
    return *input != 0 ? static_cast<uint>(*input) + 33 * const_hash(input + 1) : 5381;
}

auto constexpr operator""_hash(const char* str, size_t size) noexcept -> uint
{
    (void)size;
    return const_hash(str);
}

auto constexpr operator""_len(const char* str, size_t size) noexcept -> size_t
{
    (void)str;
    return size;
}

// Scriptable object class decorator
#define SCRIPTABLE_OBJECT_BEGIN() \
    void AddRef() const /*noexcept*/ \
    { \
        ++RefCounter; \
    } \
    void Release() const /*noexcept*/ \
    { \
        if (--RefCounter == 0) { \
            delete this; \
        } \
    } \
    mutable int RefCounter \
    { \
        1 \
    }
#define SCRIPTABLE_OBJECT_END() \
    bool _nonConstHelper \
    { \
    }

// Ref counted objects scope holder
template<typename T>
class [[nodiscard]] RefCountHolder
{
public:
    explicit RefCountHolder(T* ref) noexcept :
        _ref {ref}
    {
        _ref->AddRef();
    }
    RefCountHolder(const RefCountHolder& other) noexcept :
        _ref {other._ref}
    {
        _ref->AddRef();
    }
    RefCountHolder(RefCountHolder&& other) noexcept :
        _ref {other._ref}
    {
        _ref->AddRef();
    }
    auto operator=(const RefCountHolder& other) = delete;
    auto operator=(RefCountHolder&& other) = delete;
    ~RefCountHolder() { _ref->Release(); }

    [[nodiscard]] auto get() const noexcept -> T* { return _ref; }

private:
    T* _ref;
};

// Scope callback helpers
template<typename T>
class [[nodiscard]] ScopeCallback
{
    static_assert(std::is_nothrow_invocable_v<T>);

public:
    explicit ScopeCallback(T callback) noexcept :
        _callback {std::move(callback)}
    {
    }

    ScopeCallback(const ScopeCallback& other) = delete;
    ScopeCallback(ScopeCallback&& other) noexcept = default;
    auto operator=(const ScopeCallback& other) = delete;
    auto operator=(ScopeCallback&& other) noexcept = delete;
    ~ScopeCallback() { _callback(); }

private:
    T _callback;
};

// Float safe comparator
template<typename T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
[[nodiscard]] constexpr auto float_abs(T f) noexcept -> T
{
    return f < 0 ? -f : f;
}

template<typename T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
[[nodiscard]] constexpr auto is_float_equal(T f1, T f2) noexcept -> bool
{
    if (float_abs(f1 - f2) <= 1.0e-5f) {
        return true;
    }
    return float_abs(f1 - f2) <= 1.0e-5f * std::max(float_abs(f1), float_abs(f2));
}

// Generic helpers
template<typename... T>
FORCE_INLINE constexpr void ignore_unused(T const&... /*unused*/)
{
}

#define STRINGIFY(x) STRINGIFY2(x)
#define STRINGIFY2(x) #x
#define LINE_STR STRINGIFY(__LINE__)
#define UNUSED_VARIABLE(...) ignore_unused(__VA_ARGS__)
#define NON_CONST_METHOD_HINT() _nonConstHelper = !_nonConstHelper
#define NON_CONST_METHOD_HINT_ONELINE() _nonConstHelper = !_nonConstHelper;
#define NON_NULL // Pointer annotation

// Bit operation helpers
template<typename T, std::enable_if_t<std::is_unsigned_v<T>, int> = 0>
constexpr auto IsBitSet(T x, T y) noexcept -> bool
{
    return (x & y) != 0;
}

template<typename T, std::enable_if_t<std::is_unsigned_v<T>, int> = 0>
constexpr void SetBit(T& x, T y) noexcept
{
    x = x | y;
}

template<typename T, std::enable_if_t<std::is_unsigned_v<T>, int> = 0>
constexpr void UnsetBit(T& x, T y) noexcept
{
    x = (x | y) ^ y;
}

template<typename T, std::enable_if_t<std::is_enum_v<T>, int> = 0>
constexpr bool IsEnumSet(T value, T check) noexcept
{
    return (static_cast<size_t>(value) & static_cast<size_t>(check)) != 0;
}

// Data serialization helpers
DECLARE_EXCEPTION(DataReadingException);

class DataReader
{
public:
    explicit DataReader(const_span<uint8> buf) :
        _dataBuf {buf}
    {
    }

    template<typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    auto Read() -> T
    {
        if (_readPos + sizeof(T) > _dataBuf.size()) {
            throw DataReadingException("Unexpected end of buffer");
        }

        T data;
        std::memcpy(&data, &_dataBuf[_readPos], sizeof(T));
        _readPos += sizeof(T);
        return data;
    }

    template<typename T>
    auto ReadPtr(size_t size) -> const T*
    {
        _readPos += size;
        return size != 0u ? reinterpret_cast<const T*>(&_dataBuf[_readPos - size]) : nullptr;
    }

    template<typename T>
    void ReadPtr(T* ptr)
    {
        _readPos += sizeof(T);
        std::memcpy(ptr, &_dataBuf[_readPos - sizeof(T)], sizeof(T));
    }

    template<typename T>
    void ReadPtr(T* ptr, size_t size)
    {
        if (size != 0u) {
            _readPos += size;
            std::memcpy(ptr, &_dataBuf[_readPos - size], size);
        }
    }

    void VerifyEnd() const
    {
        if (_readPos != _dataBuf.size()) {
            throw DataReadingException("Not all data read");
        }
    }

private:
    const_span<uint8> _dataBuf;
    size_t _readPos {};
};

class DataWriter
{
public:
    static constexpr size_t BUF_RESERVE_SIZE = 1024;

    explicit DataWriter(vector<uint8>& buf) :
        _dataBuf {buf}
    {
        if (_dataBuf.capacity() < BUF_RESERVE_SIZE) {
            _dataBuf.reserve(BUF_RESERVE_SIZE);
        }
    }

    template<typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    void Write(std::enable_if_t<true, T> data)
    {
        const auto cur = _dataBuf.size();
        _dataBuf.resize(cur + sizeof(data));
        std::memcpy(&_dataBuf[cur], &data, sizeof(data));
    }

    template<typename T>
    void WritePtr(const T* data)
    {
        const auto cur = _dataBuf.size();
        _dataBuf.resize(cur + sizeof(T));
        std::memcpy(&_dataBuf[cur], data, sizeof(T));
    }

    template<typename T>
    void WritePtr(const T* data, size_t size)
    {
        if (size == 0u) {
            return;
        }

        const auto cur = _dataBuf.size();
        _dataBuf.resize(cur + size);
        std::memcpy(&_dataBuf[cur], data, size);
    }

private:
    vector<uint8>& _dataBuf;
};

// Flex rect
template<typename T>
struct TRect
{
    static_assert(std::is_arithmetic_v<T>);

    TRect() noexcept = default;
    template<typename T2>
    // ReSharper disable once CppNonExplicitConvertingConstructor
    TRect(const TRect<T2>& other) noexcept :
        Left(static_cast<T>(other.Left)),
        Top(static_cast<T>(other.Top)),
        Right(static_cast<T>(other.Right)),
        Bottom(static_cast<T>(other.Bottom))
    {
    }
    TRect(T l, T t, T r, T b) noexcept :
        Left(l),
        Top(t),
        Right(r),
        Bottom(b)
    {
    }
    TRect(T l, T t, T r, T b, T ox, T oy) noexcept :
        Left(l + ox),
        Top(t + oy),
        Right(r + ox),
        Bottom(b + oy)
    {
    }
    TRect(const TRect& fr, T ox, T oy) noexcept :
        Left(fr.Left + ox),
        Top(fr.Top + oy),
        Right(fr.Right + ox),
        Bottom(fr.Bottom + oy)
    {
    }

    template<typename T2>
    auto operator=(const TRect<T2>& fr) noexcept -> TRect&
    {
        Left = static_cast<T>(fr.Left);
        Top = static_cast<T>(fr.Top);
        Right = static_cast<T>(fr.Right);
        Bottom = static_cast<T>(fr.Bottom);
        return *this;
    }

    void Clear() noexcept
    {
        Left = 0;
        Top = 0;
        Right = 0;
        Bottom = 0;
    }

    [[nodiscard]] auto IsZero() const noexcept -> bool { return !Left && !Top && !Right && !Bottom; }
    [[nodiscard]] auto Width() const noexcept -> T { return Right - Left; }
    [[nodiscard]] auto Height() const noexcept -> T { return Bottom - Top; }
    [[nodiscard]] auto CenterX() const noexcept -> T { return Left + Width() / 2; }
    [[nodiscard]] auto CenterY() const noexcept -> T { return Top + Height() / 2; }

    [[nodiscard]] auto operator[](int index) -> T&
    {
        switch (index) {
        case 0:
            return Left;
        case 1:
            return Top;
        case 2:
            return Right;
        case 3:
            return Bottom;
        default:
            break;
        }
        throw InvalidOperationException(LINE_STR);
    }

    [[nodiscard]] auto operator[](int index) const noexcept -> const T& { return (*const_cast<TRect<T>*>(this))[index]; }

    void Advance(T ox, T oy) noexcept
    {
        Left += ox;
        Top += oy;
        Right += ox;
        Bottom += oy;
    }

    auto Interpolate(const TRect<T>& to, int procent) const noexcept -> TRect<T>
    {
        TRect<T> result(Left, Top, Right, Bottom);
        result.Left += static_cast<T>(static_cast<int>(to.Left - Left) * procent / 100);
        result.Top += static_cast<T>(static_cast<int>(to.Top - Top) * procent / 100);
        result.Right += static_cast<T>(static_cast<int>(to.Right - Right) * procent / 100);
        result.Bottom += static_cast<T>(static_cast<int>(to.Bottom - Bottom) * procent / 100);
        return result;
    }

    T Left {};
    T Top {};
    T Right {};
    T Bottom {};
};
using IRect = TRect<int>;
using FRect = TRect<float>;

template<typename T>
struct TPoint
{
    TPoint() noexcept = default;
    template<typename T2>
    // ReSharper disable once CppNonExplicitConvertingConstructor
    TPoint(const TPoint<T2>& r) noexcept :
        X(static_cast<T>(r.X)),
        Y(static_cast<T>(r.Y))
    {
    }
    TPoint(T x, T y) noexcept :
        X(x),
        Y(y)
    {
    }
    TPoint(const TPoint& fp, T ox, T oy) noexcept :
        X(fp.X + ox),
        Y(fp.Y + oy)
    {
    }

    template<typename T2>
    auto operator=(const TPoint<T2>& fp) noexcept -> TPoint&
    {
        X = static_cast<T>(fp.X);
        Y = static_cast<T>(fp.Y);
        return *this;
    }

    void Clear() noexcept
    {
        X = 0;
        Y = 0;
    }

    [[nodiscard]] auto IsZero() const noexcept -> bool { return !X && !Y; }

    [[nodiscard]] auto operator[](int index) noexcept -> T&
    {
        switch (index) {
        case 0:
            return X;
        case 1:
            return Y;
        default:
            break;
        }
        return X;
    }

    [[nodiscard]] auto operator()(T x, T y) noexcept -> TPoint&
    {
        X = x;
        Y = y;
        return *this;
    }

    T X {};
    T Y {};
};
using IPoint = TPoint<int>;
using FPoint = TPoint<float>;

// Color type
///@ ExportType ucolor ucolor uint HardStrong
struct ucolor
{
    using underlying_type = uint;
    static constexpr bool is_strong_type = true;

    constexpr ucolor() noexcept :
        rgba {}
    {
    }
    explicit constexpr ucolor(uint rgba_) noexcept :
        rgba {rgba_}
    {
    }
    constexpr ucolor(uint8 r_, uint8 g_, uint8 b_) noexcept :
        comp {r_, g_, b_, 255}
    {
    }
    constexpr ucolor(uint8 r_, uint8 g_, uint8 b_, uint8 a_) noexcept :
        comp {r_, g_, b_, a_}
    {
    }
    explicit constexpr ucolor(const ucolor& other, uint8 a_) noexcept :
        rgba {other.rgba}
    {
        comp.a = a_;
    }

    [[nodiscard]] constexpr auto operator==(const ucolor& other) const noexcept { return rgba == other.rgba; }
    [[nodiscard]] constexpr auto operator!=(const ucolor& other) const noexcept { return rgba != other.rgba; }
    [[nodiscard]] constexpr auto operator<(const ucolor& other) const noexcept { return rgba < other.rgba; }
    [[nodiscard]] constexpr auto underlying_value() noexcept -> underlying_type& { return rgba; }
    [[nodiscard]] constexpr auto underlying_value() const noexcept -> const underlying_type& { return rgba; }

    struct components
    {
        uint8 r;
        uint8 g;
        uint8 b;
        uint8 a;
    };

    union
    {
        uint rgb : 24;
        uint rgba;
        components comp;
    };

    static const ucolor clear;
};
static_assert(sizeof(ucolor) == sizeof(uint));
static_assert(std::is_standard_layout_v<ucolor>);

template<>
struct std::hash<ucolor>
{
    size_t operator()(const ucolor& s) const noexcept { return std::hash<uint>()(s.rgba); }
};

template<>
struct FMTNS::formatter<ucolor> : formatter<string_view>
{
    template<typename FormatContext>
    auto format(const ucolor& value, FormatContext& ctx) const
    {
        string buf;
        FMTNS::format_to(std::back_inserter(buf), "0x{:x}", value.rgba);

        return formatter<string_view>::format(buf, ctx);
    }
};

// Hashing
struct hstring
{
    using hash_t = uint;

    struct entry
    {
        hash_t Hash {};
        string Str {};
    };

    constexpr hstring() noexcept = default;
    constexpr explicit hstring(const entry* static_storage_entry) noexcept :
        _entry {static_storage_entry}
    {
    }
    constexpr hstring(const hstring&) noexcept = default;
    constexpr hstring(hstring&&) noexcept = default;
    constexpr auto operator=(const hstring&) noexcept -> hstring& = default;
    constexpr auto operator=(hstring&&) noexcept -> hstring& = default;

    // ReSharper disable once CppNonExplicitConversionOperator
    [[nodiscard]] operator string_view() const noexcept { return _entry->Str; }
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return _entry->Hash != 0; }
    [[nodiscard]] constexpr auto operator==(const hstring& other) const noexcept { return _entry->Hash == other._entry->Hash; }
    [[nodiscard]] constexpr auto operator!=(const hstring& other) const noexcept { return _entry->Hash != other._entry->Hash; }
    [[nodiscard]] constexpr auto operator<(const hstring& other) const noexcept { return _entry->Hash < other._entry->Hash; }
    [[nodiscard]] constexpr auto as_hash() const noexcept -> hash_t { return _entry->Hash; }
    [[nodiscard]] constexpr auto as_int() const noexcept -> int { return static_cast<int>(_entry->Hash); }
    [[nodiscard]] constexpr auto as_uint() const noexcept -> uint { return _entry->Hash; }
    [[nodiscard]] constexpr auto as_str() const noexcept -> const string& { return _entry->Str; }

private:
    static entry _zeroEntry;

    const entry* _entry {&_zeroEntry};
};
static_assert(sizeof(hstring::hash_t) == 4);
static_assert(std::is_standard_layout_v<hstring>);

template<>
struct std::hash<hstring>
{
    size_t operator()(const hstring& s) const noexcept { return std::hash<hstring::hash_t>()(s.as_hash()); }
};

template<>
struct FMTNS::formatter<hstring> : formatter<string_view>
{
    template<typename FormatContext>
    auto format(const hstring& value, FormatContext& ctx) const
    {
        return formatter<string_view>::format(value.as_str(), ctx);
    }
};

// Generic constants
// Todo: eliminate as much defines as possible
// Todo: convert all defines to constants and enums
static constexpr auto LOCAL_CONFIG_NAME = "LocalSettings.focfg";
static constexpr auto MAX_HOLO_INFO = 250;
static constexpr auto PROCESS_TALK_TIME = 1000;
static constexpr auto MAX_ADDED_NOGROUP_ITEMS = 1000;
static constexpr float MIN_ZOOM = 0.1f;
static constexpr float MAX_ZOOM = 20.0f;

// Float constants
constexpr auto PI_FLOAT = 3.14159265f;
constexpr auto SQRT3_X2_FLOAT = 3.4641016151f;
constexpr auto SQRT3_FLOAT = 1.732050807f;
constexpr auto RAD_TO_DEG_FLOAT = 57.29577951f;
constexpr auto DEG_TO_RAD_FLOAT = 0.017453292f;

// Ping
static constexpr uint8 PING_SERVER = 0;
static constexpr uint8 PING_CLIENT = 2;

// Message box types
static constexpr int FOMB_GAME = 0;
static constexpr int FOMB_TALK = 1;

// Say types
static constexpr uint8 SAY_NORM = 1;
static constexpr uint8 SAY_NORM_ON_HEAD = 2;
static constexpr uint8 SAY_SHOUT = 3;
static constexpr uint8 SAY_SHOUT_ON_HEAD = 4;
static constexpr uint8 SAY_EMOTE = 5;
static constexpr uint8 SAY_EMOTE_ON_HEAD = 6;
static constexpr uint8 SAY_WHISP = 7;
static constexpr uint8 SAY_WHISP_ON_HEAD = 8;
static constexpr uint8 SAY_SOCIAL = 9;
static constexpr uint8 SAY_RADIO = 10;
static constexpr uint8 SAY_NETMSG = 11;
static constexpr uint8 SAY_DIALOG = 12;
static constexpr uint8 SAY_APPEND = 13;
static constexpr uint8 SAY_FLASH_WINDOW = 41;

// Global map
static constexpr int GM_MAXZONEX = 100;
static constexpr int GM_MAXZONEY = 100;
static constexpr size_t GM_ZONES_FOG_SIZE = ((static_cast<size_t>(GM_MAXZONEX) / 4) + ((GM_MAXZONEX % 4) != 0 ? 1 : 0)) * GM_MAXZONEY;
static constexpr uint8 GM_FOG_FULL = 0;
static constexpr uint8 GM_FOG_HALF = 1;
static constexpr uint8 GM_FOG_NONE = 3;

// GM Info
static constexpr uint8 GM_INFO_LOCATIONS = 0x01;
static constexpr uint8 GM_INFO_ZONES_FOG = 0x08;
static constexpr uint8 GM_INFO_FOG = 0x10;
static constexpr uint8 GM_INFO_LOCATION = 0x20;

// Coordinates
static constexpr uint16 MAXHEX_DEFAULT = 200;
static constexpr uint16 MAXHEX_MIN = 10;
static constexpr uint16 MAXHEX_MAX = 4000;

// Answer
static constexpr uint8 ANSWER_BEGIN = 0xF0;
static constexpr uint8 ANSWER_END = 0xF1;
static constexpr uint8 ANSWER_BARTER = 0xF2;

// Look checks
static constexpr uint LOOK_CHECK_DIR = 0x01;
static constexpr uint LOOK_CHECK_SNEAK_DIR = 0x02;
static constexpr uint LOOK_CHECK_TRACE = 0x08;
static constexpr uint LOOK_CHECK_SCRIPT = 0x10;
static constexpr uint LOOK_CHECK_ITEM_SCRIPT = 0x20;
static constexpr uint LOOK_CHECK_TRACE_CLIENT = 0x40;

// Property type in network interaction
enum class NetProperty : uint8
{
    None = 0,
    Game, // No extra args
    Player, // No extra args
    Critter, // One extra arg: cr_id
    Chosen, // No extra args
    MapItem, // One extra arg: item_id
    CritterItem, // Two extra args: cr_id item_id
    ChosenItem, // One extra arg: item_id
    Map, // No extra args
    Location, // No extra args
    CustomEntity, // One extra arg: id
};

// Generic fixed game settings
struct GameSettings
{
#if FO_GEOMETRY == 1
    static constexpr bool HEXAGONAL_GEOMETRY = true;
    static constexpr bool SQUARE_GEOMETRY = false;
    static constexpr uint MAP_DIR_COUNT = 6;
#elif FO_GEOMETRY == 2
    static constexpr bool HEXAGONAL_GEOMETRY = false;
    static constexpr bool SQUARE_GEOMETRY = true;
    static constexpr uint MAP_DIR_COUNT = 8;
#else
#error FO_GEOMETRY not specified
#endif
};

// Radio
static constexpr uint16 RADIO_DISABLE_SEND = 0x01;
static constexpr uint16 RADIO_DISABLE_RECV = 0x02;
static constexpr uint8 RADIO_BROADCAST_WORLD = 0;
static constexpr uint8 RADIO_BROADCAST_MAP = 20;
static constexpr uint8 RADIO_BROADCAST_LOCATION = 40;
static constexpr auto RADIO_BROADCAST_ZONE(int x) -> uint8
{
    return static_cast<uint8>(100 + std::clamp(x, 1, 100));
}
static constexpr uint8 RADIO_BROADCAST_FORCE_ALL = 250;

// Light
static constexpr auto LIGHT_DISABLE_DIR(int dir) -> uint8
{
    return static_cast<uint8>(1u << std::clamp(dir, 0, 5));
}
static constexpr uint8 LIGHT_GLOBAL = 0x40;
static constexpr uint8 LIGHT_INVERSE = 0x80;

// Access
static constexpr uint8 ACCESS_CLIENT = 0;
static constexpr uint8 ACCESS_TESTER = 1;
static constexpr uint8 ACCESS_MODER = 2;
static constexpr uint8 ACCESS_ADMIN = 3;

// Commands
static constexpr auto CMD_EXIT = 1;
static constexpr auto CMD_MYINFO = 2;
static constexpr auto CMD_GAMEINFO = 3;
static constexpr auto CMD_CRITID = 4;
static constexpr auto CMD_MOVECRIT = 5;
static constexpr auto CMD_DISCONCRIT = 7;
static constexpr auto CMD_TOGLOBAL = 8;
static constexpr auto CMD_PROPERTY = 10;
static constexpr auto CMD_GETACCESS = 11;
static constexpr auto CMD_ADDITEM = 12;
static constexpr auto CMD_ADDITEM_SELF = 14;
static constexpr auto CMD_ADDNPC = 15;
static constexpr auto CMD_ADDLOCATION = 16;
static constexpr auto CMD_RUNSCRIPT = 20;
static constexpr auto CMD_REGENMAP = 25;
static constexpr auto CMD_SETTIME = 32;
static constexpr auto CMD_LOG = 37;

// Todo: rework built-in string messages
#define STR_CRNORM (100)
#define STR_CRSHOUT (102)
#define STR_CREMOTE (104)
#define STR_CRWHISP (106)
#define STR_CRSOCIAL (108)
#define STR_MBNORM (120)
#define STR_MBSHOUT (122)
#define STR_MBEMOTE (124)
#define STR_MBWHISP (126)
#define STR_MBSOCIAL (128)
#define STR_MBRADIO (130)
#define STR_MBNET (132)

#define STR_RADIO_CANT_SEND (479)
#define STR_BARTER_NO_BARTER_NOW (486)
#define STR_DIALOG_NPC_NOT_LIFE (801)
#define STR_DIALOG_DIST_TOO_LONG (803)
#define STR_DIALOG_FROM_LINK_NOT_FOUND (807)
#define STR_DIALOG_COMPILE_FAIL (808)
#define STR_DIALOG_NPC_NOT_FOUND (809)
#define STR_DIALOG_MANY_TALKERS (805)

#define STR_CHECK_UPDATES (5)
#define STR_CONNECT_TO_SERVER (6)
#define STR_CANT_CONNECT_TO_SERVER (7)
#define STR_CONNECTION_ESTABLISHED (8)
#define STR_DATA_SYNCHRONIZATION (9)
#define STR_CONNECTION_FAILURE (10)
#define STR_FILESYSTEM_ERROR (11)
#define STR_CLIENT_OUTDATED (12)
#define STR_CLIENT_OUTDATED_APP_STORE (13)
#define STR_CLIENT_OUTDATED_GOOGLE_PLAY (14)
#define STR_CLIENT_UPDATED (15)

#define STR_NET_WRONG_LOGIN (1001)
#define STR_NET_WRONG_PASS (1002)
#define STR_NET_PLAYER_ALREADY (1003)
#define STR_NET_PLAYER_IN_GAME (1004)
#define STR_NET_WRONG_SPECIAL (1005)
#define STR_NET_REG_SUCCESS (1006)
#define STR_NET_CONNECTION (1007)
#define STR_NET_CONN_ERROR (1008)
#define STR_NET_LOGINPASS_WRONG (1009)
#define STR_NET_CONN_SUCCESS (1010)
#define STR_NET_HEXES_BUSY (1012)
#define STR_NET_DISCONN_BY_DEMAND (1013)
#define STR_NET_WRONG_NAME (1014)
#define STR_NET_WRONG_CASES (1015)
#define STR_NET_WRONG_GENDER (1016)
#define STR_NET_WRONG_AGE (1017)
#define STR_NET_CONN_FAIL (1018)
#define STR_NET_WRONG_DATA (1019)
#define STR_NET_STARTLOC_FAIL (1020)
#define STR_NET_STARTMAP_FAIL (1021)
#define STR_NET_STARTCOORD_FAIL (1022)
#define STR_NET_BD_ERROR (1023)
#define STR_NET_WRONG_NETPROTO (1024)
#define STR_NET_DATATRANS_ERR (1025)
#define STR_NET_NETMSG_ERR (1026)
#define STR_NET_SETPROTO_ERR (1027)
#define STR_NET_LOGINOK (1028)
#define STR_NET_WRONG_TAGSKILL (1029)
#define STR_NET_DIFFERENT_LANG (1030)
#define STR_NET_MANY_SYMBOLS (1031)
#define STR_NET_BEGIN_END_SPACES (1032)
#define STR_NET_TWO_SPACE (1033)
#define STR_NET_BANNED (1034)
#define STR_NET_NAME_WRONG_CHARS (1035)
#define STR_NET_PASS_WRONG_CHARS (1036)
#define STR_NET_FAIL_TO_LOAD_IFACE (1037)
#define STR_NET_FAIL_RUN_START_SCRIPT (1038)
#define STR_NET_LANGUAGE_NOT_SUPPORTED (1039)
#define STR_NET_KNOCK_KNOCK (1041)
#define STR_NET_REGISTRATION_IP_WAIT (1042)
#define STR_NET_BANNED_IP (1043)
#define STR_NET_TIME_LEFT (1045)
#define STR_NET_BAN (1046)
#define STR_NET_BAN_REASON (1047)
#define STR_NET_LOGIN_SCRIPT_FAIL (1048)
#define STR_NET_PERMANENT_DEATH (1049)

// Foreach helper
template<typename T>
class irange_iterator final
{
public:
    constexpr explicit irange_iterator(T v) noexcept :
        _value {v}
    {
    }
    constexpr auto operator!=(const irange_iterator& other) const noexcept -> bool { return _value != other._value; }
    constexpr auto operator*() const noexcept -> const T& { return _value; }
    constexpr auto operator++() noexcept -> irange_iterator&
    {
        ++_value;
        return *this;
    }

private:
    T _value;
};

template<typename T>
class irange_loop final
{
public:
    constexpr explicit irange_loop(T to) noexcept :
        _fromValue {0},
        _toValue {to}
    {
    }
    constexpr explicit irange_loop(T from, T to) noexcept :
        _fromValue {from},
        _toValue {to}
    {
    }
    [[nodiscard]] constexpr auto begin() const noexcept -> irange_iterator<T> { return irange_iterator<T>(_fromValue); }
    [[nodiscard]] constexpr auto end() const noexcept -> irange_iterator<T> { return irange_iterator<T>(_toValue); }

private:
    T _fromValue;
    T _toValue;
};

template<typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
constexpr auto xrange(T value) noexcept
{
    return irange_loop<T> {0, value};
}

template<typename T, std::enable_if_t<has_size<T>::value, int> = 0>
constexpr auto xrange(T value) noexcept
{
    return irange_loop<decltype(value.size())> {0, value.size()};
}

// Copy helper
// Todo: optimize copy() to pass placement storage for value
template<typename T>
constexpr auto copy(const T& value) -> T
{
    return T(value);
}

// Noexcept wrappers
template<typename T, typename... Args>
inline void safe_call(const T& callable, Args&&... args) noexcept
{
    static_assert(!std::is_nothrow_invocable_v<T, Args...>);

    try {
        std::invoke(callable, std::forward<Args>(args)...);
    }
    catch (const std::exception& ex) {
        ReportExceptionAndContinue(ex);
    }
    catch (...) {
        UNKNOWN_EXCEPTION();
    }
}

// Ref holders
template<typename T>
class ref_hold_vector : public vector<T>
{
public:
    static_assert(!std::is_polymorphic_v<vector<T>>);
    ref_hold_vector() = default;
    ref_hold_vector(const ref_hold_vector&) = delete;
    ref_hold_vector(ref_hold_vector&&) noexcept = default;
    auto operator=(const ref_hold_vector&) -> ref_hold_vector& = delete;
    auto operator=(ref_hold_vector&&) noexcept -> ref_hold_vector& = delete;
    ~ref_hold_vector()
    {
        for (auto* ref : *this) {
            ref->Release();
        }
    }
};

template<typename T>
constexpr auto copy_hold_ref(const vector<T>& value) -> ref_hold_vector<T>
{
    ref_hold_vector<T> ref_vec;
    ref_vec.reserve(value.size());
    for (auto* ref : value) {
        RUNTIME_ASSERT(ref);
        ref->AddRef();
        ref_vec.emplace_back(ref);
    }
    return ref_vec;
}

template<typename T, typename U, typename... Args>
constexpr auto copy_hold_ref(const unordered_map<T, U, Args...>& value) -> ref_hold_vector<U>
{
    ref_hold_vector<U> ref_vec;
    ref_vec.reserve(value.size());
    for (auto&& [id, ref] : value) {
        RUNTIME_ASSERT(ref);
        ref->AddRef();
        ref_vec.emplace_back(ref);
    }
    return ref_vec;
}

// Vector helpers
template<typename T, typename T2>
constexpr auto vec_static_cast(const vector<T2>& vec) -> vector<T>
{
    vector<T> result;
    result.reserve(vec.size());
    for (auto&& v : vec) {
        result.emplace_back(static_cast<T>(v));
    }
    return result;
}

template<typename T, typename T2>
constexpr auto vec_dynamic_cast(const vector<T2>& value) -> vector<T>
{
    vector<T> result;
    result.reserve(value.size());
    for (auto&& v : value) {
        if (auto* casted = dynamic_cast<T>(v); casted != nullptr) {
            result.emplace_back(casted);
        }
    }
    return result;
}

template<typename T>
constexpr auto vec_add_unique_value(vector<T>& vec, const T& value) -> vector<T>&
{
    const auto it = std::find(vec.begin(), vec.end(), value);
    RUNTIME_ASSERT(it == vec.end());
    vec.emplace_back(value);
    return vec;
}

template<typename T>
constexpr auto vec_remove_unique_value(vector<T>& vec, const T& value) -> vector<T>&
{
    const auto it = std::find(vec.begin(), vec.end(), value);
    RUNTIME_ASSERT(it != vec.end());
    vec.erase(it);
    return vec;
}

template<typename T, typename U>
constexpr auto vec_filter(const vector<T>& vec, const U& filter) -> vector<T>
{
    vector<T> result;
    result.reserve(vec.size());
    for (const auto& value : vec) {
        if (static_cast<bool>(filter(value))) {
            result.emplace_back(value);
        }
    }
    return result;
}

template<typename T, typename U>
constexpr auto vec_filter_first(const vector<T>& vec, const U& filter) noexcept(std::is_nothrow_invocable_v<U>) -> T
{
    for (const auto& value : vec) {
        if (static_cast<bool>(filter(value))) {
            return value;
        }
    }
    return T {};
}

template<typename T, typename U>
constexpr auto vec_transform(const vector<T>& vec, const U& transfromer) -> auto
{
    vector<decltype(transfromer(nullptr))> result;
    result.reserve(vec.size());
    for (const auto& value : vec) {
        result.emplace_back(transfromer(value));
    }
    return result;
}

// Numeric cast
DECLARE_EXCEPTION(OverflowException);

template<typename T, typename U>
inline constexpr auto numeric_cast(U value) -> T
{
    static_assert(std::is_arithmetic_v<T>);
    static_assert(std::is_arithmetic_v<U>);
    static_assert(std::is_integral_v<T>);
    static_assert(std::is_integral_v<U>);

    if constexpr (std::is_unsigned_v<T> && std::is_unsigned_v<U> && sizeof(T) >= sizeof(U)) {
        // Always fit
    }
    else if constexpr (std::is_unsigned_v<T> && std::is_unsigned_v<U> && sizeof(T) < sizeof(U)) {
        if (value > std::numeric_limits<T>::max()) {
            throw OverflowException("Numeric cast overflow", value, std::numeric_limits<T>::max());
        }
    }
    else if constexpr (std::is_signed_v<T> && std::is_signed_v<U> && sizeof(T) >= sizeof(U)) {
        // Always fit
    }
    else if constexpr (std::is_signed_v<T> && std::is_signed_v<U> && sizeof(T) < sizeof(U)) {
        if (value > std::numeric_limits<T>::max()) {
            throw OverflowException("Numeric cast overflow", value, std::numeric_limits<T>::max());
        }
        if (value < std::numeric_limits<T>::min()) {
            throw OverflowException("Numeric cast underflow", value, std::numeric_limits<T>::min());
        }
    }
    else if constexpr (std::is_unsigned_v<T> && std::is_signed_v<U> && sizeof(T) >= sizeof(U)) {
        if (value < 0) {
            throw OverflowException("Numeric cast underflow", value, 0);
        }
    }
    else if constexpr (std::is_unsigned_v<T> && std::is_signed_v<U> && sizeof(T) < sizeof(U)) {
        if (value > std::numeric_limits<T>::max()) {
            throw OverflowException("Numeric cast overflow", value, std::numeric_limits<T>::max());
        }
        if (value < 0) {
            throw OverflowException("Numeric cast underflow", value, 0);
        }
    }
    else if constexpr (std::is_signed_v<T> && std::is_unsigned_v<U> && sizeof(T) >= sizeof(U)) {
        if (value > std::numeric_limits<T>::max()) {
            throw OverflowException("Numeric cast overflow", value, std::numeric_limits<T>::max());
        }
    }
    else if constexpr (std::is_signed_v<T> && std::is_unsigned_v<U> && sizeof(T) < sizeof(U)) {
        if (value > std::numeric_limits<T>::max()) {
            throw OverflowException("Numeric cast overflow", value, std::numeric_limits<T>::max());
        }
    }

    return static_cast<T>(value);
}

// Lerp
template<typename T, typename U = std::decay_t<T>>
constexpr std::enable_if_t<!std::is_integral_v<U>, U> lerp(T v1, T v2, float t) noexcept
{
    return (t <= 0.0f) ? v1 : ((t >= 1.0f) ? v2 : v1 + (v2 - v1) * t);
}

template<typename T, typename U = std::decay_t<T>>
constexpr std::enable_if_t<std::is_integral_v<U> && std::is_signed_v<U>, U> lerp(T v1, T v2, float t) noexcept
{
    return (t <= 0.0f) ? v1 : ((t >= 1.0f) ? v2 : v1 + static_cast<U>((v2 - v1) * t));
}

template<typename T, typename U = std::decay_t<T>>
constexpr std::enable_if_t<std::is_integral_v<U> && std::is_unsigned_v<U>, U> lerp(T v1, T v2, float t) noexcept
{
    return (t <= 0.0f) ? v1 : ((t >= 1.0f) ? v2 : static_cast<U>(v1 * (1 - t) + v2 * t));
}

template<typename T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
constexpr auto iround(T value) noexcept -> int
{
    return static_cast<int>(std::lround(value));
}

template<typename T, typename U>
constexpr auto clamp_to(U value) noexcept -> T
{
    return static_cast<T>(std::clamp(value, static_cast<U>(std::numeric_limits<T>::min()), static_cast<U>(std::numeric_limits<T>::max())));
}

// Hashing
DECLARE_EXCEPTION(HashResolveException);
DECLARE_EXCEPTION(HashInsertException);
DECLARE_EXCEPTION(HashCollisionException);

class HashResolver
{
public:
    virtual ~HashResolver() = default;
    [[nodiscard]] virtual auto ToHashedString(string_view s) -> hstring = 0;
    [[nodiscard]] virtual auto ToHashedStringMustExists(string_view s) const -> hstring = 0;
    [[nodiscard]] virtual auto ResolveHash(hstring::hash_t h) const -> hstring = 0;
    [[nodiscard]] virtual auto ResolveHash(hstring::hash_t h, bool* failed) const noexcept -> hstring = 0;
};

DECLARE_EXCEPTION(EnumResolveException);

class NameResolver
{
public:
    virtual ~NameResolver() = default;
    // Todo: const string& -> string_view after moving to C++20 (unordered_map heterogenous lookup)
    [[nodiscard]] virtual auto ResolveEnumValue(const string& enum_value_name, bool* failed = nullptr) const -> int = 0;
    [[nodiscard]] virtual auto ResolveEnumValue(const string& enum_name, const string& value_name, bool* failed = nullptr) const -> int = 0;
    [[nodiscard]] virtual auto ResolveEnumValueName(const string& enum_name, int value, bool* failed = nullptr) const -> const string& = 0;
    [[nodiscard]] virtual auto ResolveGenericValue(const string& str, bool* failed = nullptr) -> int = 0;
    [[nodiscard]] virtual auto CheckMigrationRule(hstring rule_name, hstring extra_info, hstring target) const noexcept -> optional<hstring> = 0;
};

class FrameBalancer
{
public:
    FrameBalancer() = default;
    FrameBalancer(bool enabled, int sleep, int fixed_fps);

    [[nodiscard]] auto GetLoopDuration() const -> time_duration;

    void StartLoop();
    void EndLoop();

private:
    bool _enabled {};
    int _sleep {};
    int _fixedFps {};
    time_point _loopStart {};
    time_duration _loopDuration {};
    time_duration _idleTimeBalance {};
};

class [[nodiscard]] TimeMeter
{
public:
    TimeMeter() noexcept :
        _startTime {time_point::clock::now()}
    {
    }

    [[nodiscard]] auto GetDuration() const noexcept { return time_point::clock::now() - _startTime; }

private:
    time_point _startTime;
};

DECLARE_EXCEPTION(InfinityLoopException);

class InfinityLoopDetector
{
public:
    explicit InfinityLoopDetector(size_t max_count = 10) :
        _maxCount {max_count + 10}
    {
    }

    auto AddLoop() -> bool
    {
        if (++_counter >= _maxCount) {
            throw InfinityLoopException("Detected infinity loop", _counter);
        }

        return true;
    }

private:
    size_t _maxCount;
    size_t _counter {};
};

class WorkThread
{
public:
    using Job = std::function<optional<time_duration>()>;
    using ExceptionHandler = std::function<bool(const std::exception&)>; // Return true to clear jobs

    explicit WorkThread(string_view name);
    WorkThread(const WorkThread&) = delete;
    WorkThread(WorkThread&&) noexcept = delete;
    auto operator=(const WorkThread&) -> WorkThread& = delete;
    auto operator=(WorkThread&&) noexcept -> WorkThread& = delete;
    ~WorkThread();

    [[nodiscard]] auto GetThreadId() const -> std::thread::id { return _thread.get_id(); }
    [[nodiscard]] auto GetJobsCount() const -> size_t;

    void SetExceptionHandler(ExceptionHandler handler);
    void AddJob(Job job);
    void AddJob(time_duration delay, Job job);
    void Clear();
    void Wait();

private:
    void AddJobInternal(time_duration delay, Job job, bool no_notify);
    void ThreadEntry() noexcept;

    string _name {};
    ExceptionHandler _exceptionHandler {};
    std::thread _thread {};
    vector<pair<time_point, Job>> _jobs {};
    bool _jobActive {};
    mutable std::mutex _dataLocker {};
    std::condition_variable _workSignal {};
    std::condition_variable _doneSignal {};
    bool _clearJobs {};
    bool _finish {};
};

// Interthread communication between server and client
using InterthreadDataCallback = std::function<void(const_span<uint8>)>;
extern map<uint16, std::function<InterthreadDataCallback(InterthreadDataCallback)>> InterthreadListeners;

#define GLOBAL_DATA(class_name, instance_name) \
    static class_name* instance_name; \
    static void Create_##class_name() \
    { \
        assert(!(instance_name)); \
        (instance_name) = new class_name(); \
    } \
    static void Delete_##class_name() \
    { \
        delete (instance_name); \
        (instance_name) = nullptr; \
    } \
    struct Register_##class_name \
    { \
        Register_##class_name() \
        { \
            assert(GlobalDataCallbacksCount < MAX_GLOBAL_DATA_CALLBACKS); \
            CreateGlobalDataCallbacks[GlobalDataCallbacksCount] = Create_##class_name; \
            DeleteGlobalDataCallbacks[GlobalDataCallbacksCount] = Delete_##class_name; \
            GlobalDataCallbacksCount++; \
        } \
    }; \
    static Register_##class_name Register_##class_name##_Instance

constexpr auto MAX_GLOBAL_DATA_CALLBACKS = 20;
using GlobalDataCallback = void (*)();
extern GlobalDataCallback CreateGlobalDataCallbacks[MAX_GLOBAL_DATA_CALLBACKS];
extern GlobalDataCallback DeleteGlobalDataCallbacks[MAX_GLOBAL_DATA_CALLBACKS];
extern int GlobalDataCallbacksCount;

extern void CreateGlobalData();
extern void DeleteGlobalData();

#endif // FO_PRECOMPILED_HEADER_GUARD
