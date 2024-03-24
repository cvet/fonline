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
// Copyright (c) 2006 - 2023, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
// Todo: cast between numeric types via numeric_cast<to>(from)
// Todo: improve custom exceptions for every subsustem
// Todo: temporary entities, disable writing to data base
// Todo: RUNTIME_ASSERT to assert?
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

// Standard API
#include <algorithm>
#include <any>
#include <array>
#include <atomic>
#include <cassert>
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
#include <numeric>
#include <optional>
#include <random>
#include <set>
#include <span.hpp>
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
DISABLE_WARNINGS_PUSH()
#include "fmt/chrono.h"
#include "fmt/format.h"
DISABLE_WARNINGS_POP()

// Todo: improve named enums
template<typename T, std::enable_if_t<std::is_enum_v<T>, int> = 0>
inline auto format_as(T v)
{
    return fmt::underlying(v);
}

#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

// WinAPI implicitly included in WinRT so add it globally for macro undefining
#if FO_UWP
#include "WinApiUndef-Include.h"
#endif

// Base types
using int8 = char;
using uint8 = unsigned char;
using int16 = short;
using uint16 = unsigned short;
using uint = unsigned int;
using int64 = int64_t;
using uint64 = uint64_t;

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
using std::map;
using std::multimap;
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
using std::vector;
using std::weak_ptr;
using tcb::span;

template<typename T>
using const_span = span<const T>;

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

inline auto hash_combine(size_t h1, size_t h2) -> size_t
{
    return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
}

struct pair_hash
{
    template<typename T, typename U>
    auto operator()(const std::pair<T, U>& v) const noexcept -> size_t
    {
        return hash_combine(std::hash<T> {}(v.first), std::hash<U> {}(v.second));
    }
};

// Strong types
template<typename T>
struct strong_type
{
    using underlying_type = typename T::underlying_type;
    static constexpr bool is_strong_type = true;
    static constexpr const char* type_name = T::name;
    static constexpr const char* underlying_type_name = T::underlying_type_name;

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

template<typename T>
struct std::hash<strong_type<T>>
{
    auto operator()(const strong_type<T>& v) const noexcept -> size_t { return std::hash<typename strong_type<T>::underlying_type> {}(v.underlying_value()); }
};

template<typename T>
struct fmt::formatter<strong_type<T>>
{
    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template<typename FormatContext>
    auto format(const strong_type<T>& s, FormatContext& ctx)
    {
        return format_to(ctx.out(), "{}", s.underlying_value());
    }
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

///@ ExportValueType ident ident_t HardStrong HasValueAccessor Layout = uint
#define IDENT_T_NAME "ident"
struct ident_t_traits
{
    static constexpr const char* name = IDENT_T_NAME;
    static constexpr const char* underlying_type_name = "uint";
    using underlying_type = uint;
};
using ident_t = strong_type<ident_t_traits>;
static_assert(sizeof(ident_t) == sizeof(uint));
static_assert(std::is_standard_layout_v<ident_t>);

///@ ExportValueType tick_t tick_t RelaxedStrong HasValueAccessor Layout = uint
#define TICK_T_NAME "tick_t"
struct tick_t_traits
{
    static constexpr const char* name = TICK_T_NAME;
    static constexpr const char* underlying_type_name = "uint";
    using underlying_type = uint;
};
using tick_t = strong_type<tick_t_traits>;
static_assert(sizeof(tick_t) == sizeof(uint));
static_assert(std::is_standard_layout_v<tick_t>);

// Custom any as string
// Todo: export any_t with ExportType
class any_t : public string
{
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

inline auto time_point_desc(const time_point& t) -> std::tm
{
    const auto unix_time = time_point_to_unix_time(t);
    const auto tm_struct = fmt::localtime(unix_time);
    return tm_struct;
}

template<typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
static constexpr auto time_duration_to_ms(const time_duration& duration) -> T
{
    return static_cast<T>(std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
}

static constexpr auto time_duration_div(time_duration duration1, time_duration duration2) -> float
{
    return static_cast<float>(static_cast<double>(duration1.count()) / static_cast<double>(duration2.count()));
}

template<>
struct fmt::formatter<time_duration>
{
    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template<typename FormatContext>
    auto format(const time_duration& t, FormatContext& ctx)
    {
        if (t < std::chrono::milliseconds {1}) {
            const auto us = std::chrono::duration_cast<std::chrono::microseconds>(t).count() % 1000;
            const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t).count() % 1000;
            return format_to(ctx.out(), "{}.{:03} us", us, ns);
        }
        else if (t < std::chrono::seconds {1}) {
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t).count() % 1000;
            const auto us = std::chrono::duration_cast<std::chrono::microseconds>(t).count() % 1000;
            return format_to(ctx.out(), "{}.{:03} ms", ms, us);
        }
        else if (t < std::chrono::minutes {1}) {
            const auto sec = std::chrono::duration_cast<std::chrono::seconds>(t).count();
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t).count() % 1000;
            return format_to(ctx.out(), "{}.{:03} sec", sec, ms);
        }
        else if (t < std::chrono::hours {24}) {
            const auto hour = std::chrono::duration_cast<std::chrono::hours>(t).count();
            const auto min = std::chrono::duration_cast<std::chrono::minutes>(t).count() % 60;
            const auto sec = std::chrono::duration_cast<std::chrono::seconds>(t).count() % 60;
            return format_to(ctx.out(), "{:02}:{:02}:{:02} sec", hour, min, sec);
        }
        else {
            const auto day = std::chrono::duration_cast<std::chrono::hours>(t).count() / 24;
            const auto hour = std::chrono::duration_cast<std::chrono::hours>(t).count() % 24;
            const auto min = std::chrono::duration_cast<std::chrono::minutes>(t).count() % 60;
            const auto sec = std::chrono::duration_cast<std::chrono::seconds>(t).count() % 60;
            return format_to(ctx.out(), "{} day{} {:02}:{:02}:{:02} sec", day, day > 1 ? "s" : "", hour, min, sec);
        }
    }
};

template<>
struct fmt::formatter<time_point>
{
    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template<typename FormatContext>
    auto format(const time_point& t, FormatContext& ctx)
    {
        const auto td = time_point_desc(t);
        return format_to(ctx.out(), "{}-{:02}-{:02} {:02}:{:02}:{:02}", 1900 + td.tm_year, td.tm_mon + 1, td.tm_mday, td.tm_hour, td.tm_min, td.tm_sec);
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
template<typename T>
class has_size
{
    using one = char;
    struct two
    {
        char x[2];
    };

    template<typename C>
    static auto test(decltype(&C::size)) -> one;
    template<typename C>
    static auto test(...) -> two;

public:
    enum
    {
        value = sizeof(test<T>(0)) == sizeof(char)
    };
};

template<typename Test, template<typename...> class Ref>
struct is_specialization : std::false_type
{
};

template<template<typename...> class Ref, typename... Args>
struct is_specialization<Ref<Args...>, Ref> : std::true_type
{
};

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
extern auto GetStackTraceEntry(size_t deep) -> const SourceLocationData*;

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
extern void ShowExceptionMessageBox(bool enabled);

class ExceptionInfo
{
public:
    virtual ~ExceptionInfo() = default;
    [[nodiscard]] virtual auto StackTrace() const noexcept -> const string& = 0;
};

struct ExceptionStackTraceData
{
    string StackTrace {};
};

// Todo: pass name to exceptions context args
#define DECLARE_EXCEPTION(exception_name) \
    class exception_name : public std::exception, public ExceptionInfo \
    { \
    public: \
        exception_name() = delete; \
        exception_name(const exception_name&) = delete; \
        exception_name(exception_name&&) = default; \
        auto operator=(const exception_name&) = delete; \
        auto operator=(exception_name&&) noexcept = delete; \
        ~exception_name() override = default; \
        template<typename... Args> \
        explicit exception_name(string_view message, Args... args) : \
            _exceptionParams {fmt::format("{}", std::forward<Args>(args))...} \
        { \
            _exceptionMessage = #exception_name ": "; \
            _exceptionMessage.append(message); \
            for (auto& param : _exceptionParams) { \
                _exceptionMessage.append("\n- ").append(param); \
            } \
            _stackTrace = GetStackTrace(); \
        } \
        template<typename... Args> \
        exception_name(ExceptionStackTraceData data, string_view message, Args... args) : \
            _exceptionParams {fmt::format("{}", std::forward<Args>(args))...} \
        { \
            _exceptionMessage = #exception_name ": "; \
            _exceptionMessage.append(message); \
            for (auto& param : _exceptionParams) { \
                _exceptionMessage.append("\n- ").append(param); \
            } \
            _stackTrace = std::move(data.StackTrace); \
        } \
        [[nodiscard]] auto what() const noexcept -> const char* override \
        { \
            return _exceptionMessage.c_str(); \
        } \
        [[nodiscard]] auto StackTrace() const noexcept -> const string& override \
        { \
            return _stackTrace; \
        } \
\
    private: \
        string _exceptionMessage {}; \
        string _stackTrace {}; \
        vector<string> _exceptionParams {}; \
    }

// Todo: split RUNTIME_ASSERT to real uncoverable assert and some kind of runtime error
#if !FO_NO_EXTRA_ASSERTS
#define RUNTIME_ASSERT(expr) \
    if (!(expr)) \
    throw AssertationException(#expr, __FILE__, __LINE__)
#define RUNTIME_ASSERT_STR(expr, str) \
    if (!(expr)) \
    throw AssertationException(str, __FILE__, __LINE__)
#else
#define RUNTIME_ASSERT(expr)
#define RUNTIME_ASSERT_STR(expr, str)
#endif

// Common exceptions
DECLARE_EXCEPTION(GenericException);
DECLARE_EXCEPTION(AssertationException);
DECLARE_EXCEPTION(UnreachablePlaceException);
DECLARE_EXCEPTION(NotSupportedException);
DECLARE_EXCEPTION(NotImplementedException);
DECLARE_EXCEPTION(InvalidCallException);
DECLARE_EXCEPTION(NotEnabled3DException);

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

    auto operator()(Args... args) -> EventDispatcher&
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

// Raw pointer observation
// Todo: improve ptr<> system for leng term pointer observing
class RefCounter
{
    template<typename>
    friend class ptr;

public:
    RefCounter() = default;
    RefCounter(const RefCounter&) = delete;
    RefCounter(RefCounter&&) = delete;
    auto operator=(const RefCounter&) -> RefCounter& = delete;
    auto operator=(RefCounter&&) -> RefCounter& = delete;
    virtual ~RefCounter();

private:
    std::atomic_int _ptrCounter {};
};

template<typename T>
class ptr final
{
    static_assert(std::is_base_of_v<RefCounter, T>, "T must inherit from RefCounter");
    using type = ptr<T>;

public:
    ptr() = default;

    explicit ptr(T* p) :
        _value {p}
    {
        if (_value != nullptr) {
            ++_value->ptrCounter;
        }
    }

    ptr(const type& other)
    {
        _value = other._value;
        ++_value->ptrCounter;
    }

    auto operator=(const type& other) -> ptr&
    {
        if (this != &other) {
            if (_value != nullptr) {
                --_value->ptrCounter;
            }
            _value = other._value;
            if (_value != nullptr) {
                ++_value->ptrCounter;
            }
        }
        return *this;
    }

    ptr(type&& other) noexcept
    {
        _value = other._value;
        other._value = nullptr;
    }

    auto operator=(type&& other) noexcept -> ptr&
    {
        if (this != &other) {
            if (_value != nullptr) {
                --_value->ptrCounter;
            }
            _value = other._value;
            other._value = nullptr;
        }
        return *this;
    }

    ~ptr()
    {
        if (_value != nullptr) {
            --_value->ptrCounter;
        }
    }

    auto operator->() -> T* { return _value; }
    auto operator*() -> T& { return *_value; }
    explicit operator bool() { return !!_value; }
    auto operator==(const T* other) -> bool { return _value == other; }
    auto operator==(const type& other) -> bool { return _value == other._value; }
    auto operator!=(const T* other) -> bool { return _value != other; }
    auto operator!=(const type& other) -> bool { return _value != other._value; }
    auto get() -> T* { return _value; }

private:
    T* _value {};
};

// C-strings literal helpers
constexpr auto const_hash(const char* input) -> uint
{
    return *input != 0 ? static_cast<uint>(*input) + 33 * const_hash(input + 1) : 5381;
}

auto constexpr operator""_hash(const char* str, size_t size) -> uint
{
    (void)size;
    return const_hash(str);
}

auto constexpr operator""_len(const char* str, size_t size) -> size_t
{
    (void)str;
    return size;
}

// Scriptable object class decorator
#define SCRIPTABLE_OBJECT_BEGIN() \
    void AddRef() \
    { \
        ++RefCounter; \
    } \
    void Release() \
    { \
        if (--RefCounter == 0) { \
            delete this; \
        } \
    } \
    int RefCounter \
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
    explicit RefCountHolder(T* ref) :
        _ref {ref}
    {
        _ref->AddRef();
    }
    RefCountHolder(const RefCountHolder& other) :
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

    [[nodiscard]] auto get() const -> T* { return _ref; }

private:
    T* _ref;
};

// Scope callback helpers
template<typename T>
class [[nodiscard]] ScopeCallback
{
public:
    explicit ScopeCallback(T callback) :
        _callback {std::move(callback)}
    {
    }

    ScopeCallback(const ScopeCallback& other) = delete;
    ScopeCallback(ScopeCallback&& other) noexcept = default;
    auto operator=(const ScopeCallback& other) = delete;
    auto operator=(ScopeCallback&& other) noexcept = delete;

    ~ScopeCallback()
    {
        if constexpr (std::is_nothrow_invocable_v<T>) {
            _callback();
        }
        else {
            try {
                _callback();
            }
            catch (const std::exception& ex) {
                ReportExceptionAndContinue(ex);
            }
        }
    }

private:
    T _callback;
};

// Float safe comparator
template<typename T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
[[nodiscard]] constexpr auto float_abs(T f) noexcept -> float
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
    TRect() :
        Left(0),
        Top(0),
        Right(0),
        Bottom(0)
    {
    }
    template<typename T2>
    // ReSharper disable once CppNonExplicitConvertingConstructor
    TRect(const TRect<T2>& fr) :
        Left(static_cast<T>(fr.Left)),
        Top(static_cast<T>(fr.Top)),
        Right(static_cast<T>(fr.Right)),
        Bottom(static_cast<T>(fr.Bottom))
    {
    }
    TRect(T l, T t, T r, T b) :
        Left(l),
        Top(t),
        Right(r),
        Bottom(b)
    {
    }
    TRect(T l, T t, T r, T b, T ox, T oy) :
        Left(l + ox),
        Top(t + oy),
        Right(r + ox),
        Bottom(b + oy)
    {
    }
    TRect(const TRect& fr, T ox, T oy) :
        Left(fr.Left + ox),
        Top(fr.Top + oy),
        Right(fr.Right + ox),
        Bottom(fr.Bottom + oy)
    {
    }

    template<typename T2>
    auto operator=(const TRect<T2>& fr) -> TRect&
    {
        Left = static_cast<T>(fr.Left);
        Top = static_cast<T>(fr.Top);
        Right = static_cast<T>(fr.Right);
        Bottom = static_cast<T>(fr.Bottom);
        return *this;
    }

    void Clear()
    {
        Left = 0;
        Top = 0;
        Right = 0;
        Bottom = 0;
    }

    [[nodiscard]] auto IsZero() const -> bool { return !Left && !Top && !Right && !Bottom; }
    [[nodiscard]] auto Width() const -> T { return Right - Left; }
    [[nodiscard]] auto Height() const -> T { return Bottom - Top; }
    [[nodiscard]] auto CenterX() const -> T { return Left + Width() / 2; }
    [[nodiscard]] auto CenterY() const -> T { return Top + Height() / 2; }

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
        throw UnreachablePlaceException(LINE_STR);
    }

    [[nodiscard]] auto operator[](int index) const -> const T& { return (*const_cast<TRect<T>*>(this))[index]; }

    void Advance(T ox, T oy)
    {
        Left += ox;
        Top += oy;
        Right += ox;
        Bottom += oy;
    }

    auto Interpolate(const TRect<T>& to, int procent) const -> TRect<T>
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
using IRect = TRect<int>; // Todo: move IRect to irect
using FRect = TRect<float>; // Todo: move FRect to frect

// Color type
///@ ExportValueType ucolor ucolor HardStrong HasValueAccessor Layout = uint
struct ucolor
{
    using underlying_type = uint;
    static constexpr bool is_strong_type = true;
    static constexpr const char* type_name = "ucolor";
    static constexpr const char* underlying_type_name = "uint";

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
    auto operator()(const ucolor& v) const noexcept -> size_t { return std::hash<uint> {}(v.rgba); }
};

template<>
struct fmt::formatter<ucolor>
{
    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template<typename FormatContext>
    auto format(const ucolor& c, FormatContext& ctx)
    {
        return format_to(ctx.out(), "0x{:x}", c.rgba);
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

    hstring() = default;
    constexpr explicit hstring(const entry* static_storage_entry) :
        _entry {static_storage_entry}
    {
    }
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
    auto operator()(const hstring& v) const noexcept -> size_t { return std::hash<hstring::hash_t> {}(v.as_hash()); }
};

template<>
struct fmt::formatter<hstring>
{
    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template<typename FormatContext>
    auto format(const hstring& s, FormatContext& ctx)
    {
        return format_to(ctx.out(), "{}", s.as_str());
    }
};

// Plain data
template<typename T>
constexpr bool is_valid_pod_type_v = std::is_standard_layout_v<T> && !is_strong_type_v<T> && !std::is_same_v<T, any_t> && //
    !std::is_same_v<T, string> && !std::is_same_v<T, string_view> && !std::is_same_v<T, hstring> && !std::is_arithmetic_v<T> && //
    !std::is_enum_v<T> && !is_specialization<T, vector>::value && !is_specialization<T, map>::value;

template<typename T>
inline auto parse_from_string(const string& str) -> T;

template<typename...>
struct always_false : std::false_type
{
};

template<typename T>
inline auto parse_from_string(const string& str) -> T
{
    static_assert(always_false<T>::value, "No specialization exists for parse_from_string");
}

#define DECLARE_FORMATTER(type, ...) \
    template<> \
    struct fmt::formatter<type> \
    { \
        template<typename ParseContext> \
        constexpr auto parse(ParseContext& ctx) \
        { \
            return ctx.begin(); \
        } \
        template<typename FormatContext> \
        auto format(const type& value, FormatContext& ctx) \
        { \
            return format_to(ctx.out(), __VA_ARGS__); \
        } \
    }

#define DECLARE_TYPE_PARSER(type, ...) \
    template<> \
    inline auto parse_from_string<type>(const string& str)->type \
    { \
        type value = {}; \
        istringstream sstr {str}; \
        __VA_ARGS__; \
        return value; \
    }

// Position types
///@ ExportValueType isize isize HardStrong Layout = int-int
struct isize
{
    [[nodiscard]] constexpr auto operator==(const isize& other) const noexcept -> bool { return width == other.width && height == other.height; }
    [[nodiscard]] constexpr auto operator!=(const isize& other) const noexcept -> bool { return width != other.width || height != other.height; }
    [[nodiscard]] constexpr auto GetSquare() const noexcept -> int { return width * height; }
    template<typename T>
    [[nodiscard]] constexpr auto IsValidPos(T pos) const noexcept -> bool
    {
        return pos.x >= 0 && pos.y >= 0 && pos.x < width && pos.y < height;
    }

    int width {};
    int height {};
};
static_assert(std::is_standard_layout_v<isize>);
static_assert(sizeof(isize) == 8);
DECLARE_FORMATTER(isize, "{} {}", value.width, value.height);
DECLARE_TYPE_PARSER(isize, sstr >> value.width, sstr >> value.height);

///@ ExportValueType ipos ipos HardStrong Layout = int-int
struct ipos
{
    [[nodiscard]] constexpr auto operator==(const ipos& other) const noexcept -> bool { return x == other.x && y == other.y; }
    [[nodiscard]] constexpr auto operator!=(const ipos& other) const noexcept -> bool { return x != other.x || y != other.y; }
    [[nodiscard]] constexpr auto operator+(const ipos& other) const noexcept -> ipos { return {x + other.x, y + other.y}; }
    [[nodiscard]] constexpr auto operator-(const ipos& other) const noexcept -> ipos { return {x - other.x, y - other.y}; }
    [[nodiscard]] constexpr auto operator*(const ipos& other) const noexcept -> ipos { return {x * other.x, y * other.y}; }
    [[nodiscard]] constexpr auto operator/(const ipos& other) const noexcept -> ipos { return {x / other.x, y / other.y}; }

    int x {};
    int y {};
};
static_assert(std::is_standard_layout_v<ipos>);
static_assert(sizeof(ipos) == 8);
DECLARE_FORMATTER(ipos, "{} {}", value.x, value.y);
DECLARE_TYPE_PARSER(ipos, sstr >> value.x, sstr >> value.y);

template<>
struct std::hash<ipos>
{
    auto operator()(const ipos& v) const noexcept -> size_t { return hash_combine(std::hash<int> {}(v.x), std::hash<int> {}(v.y)); }
};

///@ ExportValueType irect irect HardStrong Layout = int-int-int-int
struct irect
{
    constexpr irect() noexcept = default;
    constexpr irect(ipos pos, isize size) noexcept :
        x {pos.x},
        y {pos.y},
        width {size.width},
        height {size.height}
    {
    }
    constexpr irect(int x_, int y_, isize size) noexcept :
        x {x_},
        y {y_},
        width {size.width},
        height {size.height}
    {
    }
    constexpr irect(ipos pos, int width_, int height_) noexcept :
        x {pos.x},
        y {pos.y},
        width {width_},
        height {height_}
    {
    }
    constexpr irect(int x_, int y_, int width_, int height_) noexcept :
        x {x_},
        y {y_},
        width {width_},
        height {height_}
    {
    }
    [[nodiscard]] constexpr auto operator==(const irect& other) const noexcept -> bool { return x == other.x && y == other.y && width == other.width && width == other.width; }
    [[nodiscard]] constexpr auto operator!=(const irect& other) const noexcept -> bool { return x != other.x || y != other.y || height != other.height || height != other.height; }

    int x {};
    int y {};
    int width {};
    int height {};
};
static_assert(std::is_standard_layout_v<irect>);
static_assert(sizeof(irect) == 16);
DECLARE_FORMATTER(irect, "{} {} {} {}", value.x, value.y, value.width, value.height);
DECLARE_TYPE_PARSER(irect, sstr >> value.x, sstr >> value.y, sstr >> value.width, sstr >> value.height);

///@ ExportValueType ipos16 ipos16 HardStrong Layout = int16-int16
struct ipos16
{
    [[nodiscard]] constexpr auto operator==(const ipos16& other) const noexcept -> bool { return x == other.x && y == other.y; }
    [[nodiscard]] constexpr auto operator!=(const ipos16& other) const noexcept -> bool { return x != other.x || y != other.y; }
    [[nodiscard]] constexpr auto operator+(const ipos16& other) const noexcept -> ipos16 { return {static_cast<int16>(x + other.x), static_cast<int16>(y + other.y)}; }
    [[nodiscard]] constexpr auto operator-(const ipos16& other) const noexcept -> ipos16 { return {static_cast<int16>(x - other.x), static_cast<int16>(y - other.y)}; }
    [[nodiscard]] constexpr auto operator*(const ipos16& other) const noexcept -> ipos16 { return {static_cast<int16>(x * other.x), static_cast<int16>(y * other.y)}; }
    [[nodiscard]] constexpr auto operator/(const ipos16& other) const noexcept -> ipos16 { return {static_cast<int16>(x / other.x), static_cast<int16>(y / other.y)}; }

    int16 x {};
    int16 y {};
};
static_assert(std::is_standard_layout_v<ipos16>);
static_assert(sizeof(ipos16) == 4);
DECLARE_FORMATTER(ipos16, "{} {}", value.x, value.y);
DECLARE_TYPE_PARSER(ipos16, sstr >> value.x, sstr >> value.y);

///@ ExportValueType upos16 upos16 HardStrong Layout = uint16-uint16
struct upos16
{
    [[nodiscard]] constexpr auto operator==(const upos16& other) const noexcept -> bool { return x == other.x && y == other.y; }
    [[nodiscard]] constexpr auto operator!=(const upos16& other) const noexcept -> bool { return x != other.x || y != other.y; }
    [[nodiscard]] constexpr auto operator+(const upos16& other) const noexcept -> upos16 { return {static_cast<uint16>(x + other.x), static_cast<uint16>(y + other.y)}; }
    [[nodiscard]] constexpr auto operator-(const upos16& other) const noexcept -> upos16 { return {static_cast<uint16>(x - other.x), static_cast<uint16>(y - other.y)}; }
    [[nodiscard]] constexpr auto operator*(const upos16& other) const noexcept -> upos16 { return {static_cast<uint16>(x * other.x), static_cast<uint16>(y * other.y)}; }
    [[nodiscard]] constexpr auto operator/(const upos16& other) const noexcept -> upos16 { return {static_cast<uint16>(x / other.x), static_cast<uint16>(y / other.y)}; }

    uint16 x {};
    uint16 y {};
};
static_assert(std::is_standard_layout_v<upos16>);
static_assert(sizeof(upos16) == 4);
DECLARE_FORMATTER(upos16, "{} {}", value.x, value.y);
DECLARE_TYPE_PARSER(upos16, sstr >> value.x, sstr >> value.y);

///@ ExportValueType ipos8 ipos8 HardStrong Layout = int8-int8
struct ipos8
{
    [[nodiscard]] constexpr auto operator==(const ipos8& other) const noexcept -> bool { return x == other.x && y == other.y; }
    [[nodiscard]] constexpr auto operator!=(const ipos8& other) const noexcept -> bool { return x != other.x || y != other.y; }
    [[nodiscard]] constexpr auto operator+(const ipos8& other) const noexcept -> ipos8 { return {static_cast<int8>(x + other.x), static_cast<int8>(y + other.y)}; }
    [[nodiscard]] constexpr auto operator-(const ipos8& other) const noexcept -> ipos8 { return {static_cast<int8>(x - other.x), static_cast<int8>(y - other.y)}; }
    [[nodiscard]] constexpr auto operator*(const ipos8& other) const noexcept -> ipos8 { return {static_cast<int8>(x * other.x), static_cast<int8>(y * other.y)}; }
    [[nodiscard]] constexpr auto operator/(const ipos8& other) const noexcept -> ipos8 { return {static_cast<int8>(x / other.x), static_cast<int8>(y / other.y)}; }

    int8 x {};
    int8 y {};
};
static_assert(std::is_standard_layout_v<ipos8>);
static_assert(sizeof(ipos8) == 2);
DECLARE_FORMATTER(ipos8, "{} {}", value.x, value.y);
DECLARE_TYPE_PARSER(ipos8, sstr >> value.x, sstr >> value.y);

///@ ExportValueType fsize fsize HardStrong Layout = float-float
struct fsize
{
    [[nodiscard]] constexpr auto operator==(const fsize& other) const noexcept -> bool { return is_float_equal(width, other.width) && is_float_equal(height, other.height); }
    [[nodiscard]] constexpr auto operator!=(const fsize& other) const noexcept -> bool { return !is_float_equal(width, other.width) || !is_float_equal(height, other.height); }
    [[nodiscard]] constexpr auto GetSquare() const noexcept -> float { return width * height; }
    template<typename T>
    [[nodiscard]] constexpr auto IsValidPos(T pos) const noexcept -> bool
    {
        return pos.x >= 0.0f && pos.y >= 0.0f && pos.x < width && pos.y < height;
    }

    float width {};
    float height {};
};
static_assert(std::is_standard_layout_v<fsize>);
static_assert(sizeof(fsize) == 8);
DECLARE_FORMATTER(fsize, "{} {}", value.width, value.height);
DECLARE_TYPE_PARSER(fsize, sstr >> value.width, sstr >> value.height);

///@ ExportValueType fpos fpos HardStrong Layout = float-float
struct fpos
{
    [[nodiscard]] constexpr auto operator==(const fpos& other) const noexcept -> bool { return is_float_equal(x, other.x) && is_float_equal(y, other.y); }
    [[nodiscard]] constexpr auto operator!=(const fpos& other) const noexcept -> bool { return !is_float_equal(x, other.x) || !is_float_equal(y, other.y); }
    [[nodiscard]] constexpr auto operator+(const fpos& other) const noexcept -> fpos { return {x + other.x, y + other.y}; }
    [[nodiscard]] constexpr auto operator-(const fpos& other) const noexcept -> fpos { return {x - other.x, y - other.y}; }
    [[nodiscard]] constexpr auto operator*(const fpos& other) const noexcept -> fpos { return {x * other.x, y * other.y}; }
    [[nodiscard]] constexpr auto operator/(const fpos& other) const noexcept -> fpos { return {x / other.x, y / other.y}; }

    float x {};
    float y {};
};
static_assert(std::is_standard_layout_v<fpos>);
static_assert(sizeof(fpos) == 8);
DECLARE_FORMATTER(fpos, "{} {}", value.x, value.y);
DECLARE_TYPE_PARSER(fpos, sstr >> value.x, sstr >> value.y);

///@ ExportValueType frect frect HardStrong Layout = float-float-float-float
struct frect
{
    constexpr frect() noexcept = default;
    constexpr frect(fpos pos, fsize size) noexcept :
        x {pos.x},
        y {pos.y},
        width {size.width},
        height {size.height}
    {
    }
    constexpr frect(float x_, float y_, fsize size) noexcept :
        x {x_},
        y {y_},
        width {size.width},
        height {size.height}
    {
    }
    constexpr frect(fpos pos, float width_, float height_) noexcept :
        x {pos.x},
        y {pos.y},
        width {width_},
        height {height_}
    {
    }
    constexpr frect(float x_, float y_, float width_, float height_) noexcept :
        x {x_},
        y {y_},
        width {width_},
        height {height_}
    {
    }
    [[nodiscard]] constexpr auto operator==(const frect& other) const noexcept -> bool { return is_float_equal(x, other.x) && is_float_equal(y, other.y) && is_float_equal(width, other.width) && is_float_equal(height, other.height); }
    [[nodiscard]] constexpr auto operator!=(const frect& other) const noexcept -> bool { return !is_float_equal(x, other.x) || !is_float_equal(y, other.y) || !is_float_equal(width, other.width) || !is_float_equal(height, other.height); }

    float x {};
    float y {};
    float width {};
    float height {};
};
static_assert(std::is_standard_layout_v<frect>);
static_assert(sizeof(frect) == 16);
DECLARE_FORMATTER(frect, "{} {} {} {}", value.x, value.y, value.width, value.height);
DECLARE_TYPE_PARSER(frect, sstr >> value.x, sstr >> value.y, sstr >> value.width, sstr >> value.height);

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

// Id helpers
// Todo: remove all id masks after moving to 64-bit hashes
#define DLGID_MASK (0xFFFFC000)
#define DLG_STR_ID(dlg_id, idx) (((dlg_id)&DLGID_MASK) | ((idx) & ~DLGID_MASK))
#define LOCPID_MASK (0xFFFFF000)
#define LOC_STR_ID(loc_pid, idx) (((loc_pid)&LOCPID_MASK) | ((idx) & ~LOCPID_MASK))
#define ITEMPID_MASK (0xFFFFFFF0)
#define ITEM_STR_ID(item_pid, idx) (((item_pid)&ITEMPID_MASK) | ((idx) & ~ITEMPID_MASK))
#define CRPID_MASK (0xFFFFFFF0)
#define CR_STR_ID(cr_pid, idx) (((cr_pid)&CRPID_MASK) | ((idx) & ~CRPID_MASK))

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
static constexpr uint8 GM_INFO_CRITTERS = 0x02;
static constexpr uint8 GM_INFO_ZONES_FOG = 0x08;
static constexpr uint8 GM_INFO_ALL = 0x0F;
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
#define STR_CONNECTION_FAILTURE (10)
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
    constexpr explicit irange_iterator(T v) :
        _value {v}
    {
    }
    constexpr auto operator!=(const irange_iterator& other) const -> bool { return _value != other._value; }
    constexpr auto operator*() const -> const T& { return _value; }
    constexpr auto operator++() -> irange_iterator&
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
    constexpr explicit irange_loop(T to) :
        _fromValue {0},
        _toValue {to}
    {
    }
    constexpr explicit irange_loop(T from, T to) :
        _fromValue {from},
        _toValue {to}
    {
    }
    [[nodiscard]] constexpr auto begin() const -> irange_iterator<T> { return irange_iterator<T>(_fromValue); }
    [[nodiscard]] constexpr auto end() const -> irange_iterator<T> { return irange_iterator<T>(_toValue); }

private:
    T _fromValue;
    T _toValue;
};

template<typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
constexpr auto xrange(T value)
{
    return irange_loop<T> {0, value};
}

template<typename T, std::enable_if_t<has_size<T>::value, int> = 0>
constexpr auto xrange(T value)
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

template<typename T, typename... Args>
class ref_vector : public vector<T, Args...>
{
public:
    ~ref_vector()
    {
        for (auto* ref : *this) {
            ref->Release();
        }
    }
};

template<typename T, typename... Args>
constexpr auto copy_hold_ref(const vector<T, Args...>& value) -> ref_vector<T, Args...>
{
    ref_vector<T, Args...> ref_vec;
    ref_vec.reserve(value.size());
    for (auto* ref : value) {
        ref->AddRef();
        ref_vec.emplace_back(ref);
    }
    return ref_vec;
}

template<typename T, typename U, typename... Args>
constexpr auto copy_hold_ref(const unordered_map<T, U, Args...>& value) -> ref_vector<U>
{
    ref_vector<U> ref_vec;
    ref_vec.reserve(value.size());
    for (auto&& [id, ref] : value) {
        ref->AddRef();
        ref_vec.emplace_back(ref);
    }
    return ref_vec;
}

// Vector pointer cast
template<typename T, typename T2>
constexpr auto vec_cast(const vector<T2>& value) -> vector<T>
{
    vector<T> result;
    result.reserve(value.size());
    for (auto&& v : value) {
        result.emplace_back(static_cast<T>(v));
    }
    return result;
}

// Lerp
template<typename T, typename U = std::decay_t<T>>
constexpr std::enable_if_t<!std::is_integral_v<U>, U> lerp(T v1, T v2, float t)
{
    return (t <= 0.0f) ? v1 : ((t >= 1.0f) ? v2 : v1 + (v2 - v1) * t);
}

template<typename T, typename U = std::decay_t<T>>
constexpr std::enable_if_t<std::is_integral_v<U> && std::is_signed_v<U>, U> lerp(T v1, T v2, float t)
{
    return (t <= 0.0f) ? v1 : ((t >= 1.0f) ? v2 : v1 + static_cast<U>((v2 - v1) * t));
}

template<typename T, typename U = std::decay_t<T>>
constexpr std::enable_if_t<std::is_integral_v<U> && std::is_unsigned_v<U>, U> lerp(T v1, T v2, float t)
{
    return (t <= 0.0f) ? v1 : ((t >= 1.0f) ? v2 : static_cast<U>(v1 * (1 - t) + v2 * t));
}

template<typename T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
constexpr auto iround(T value) -> int
{
    return static_cast<int>(std::lround(value));
}

// Other helpers
template<typename T>
constexpr auto get_if_non_zero(const T& value, const T& extra_value) -> const T&
{
    return !!value ? value : extra_value;
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
    [[nodiscard]] virtual auto ResolveHash(hstring::hash_t h, bool* failed = nullptr) const -> hstring = 0;
};

DECLARE_EXCEPTION(EnumResolveException);

struct BaseTypeInfo
{
    string TypeName {};
    bool IsString {};
    bool IsHash {};
    bool IsInt {};
    bool IsSignedInt {};
    bool IsInt8 {};
    bool IsInt16 {};
    bool IsInt32 {};
    bool IsInt64 {};
    bool IsUInt8 {};
    bool IsUInt16 {};
    bool IsUInt32 {};
    bool IsUInt64 {};
    bool IsFloat {};
    bool IsSingleFloat {};
    bool IsDoubleFloat {};
    bool IsBool {};
    bool IsEnum {};
    bool IsAggregatedType {};
    uint Size {};
};

class NameResolver
{
public:
    virtual ~NameResolver() = default;
    // Todo: const string& -> string_view after moving to C++20 (unordered_map heterogenous lookup)
    [[nodiscard]] virtual auto ResolveBaseType(string_view type_str) const -> BaseTypeInfo = 0;
    [[nodiscard]] virtual auto GetEnumInfo(const string& enum_name, size_t& size) const -> bool = 0;
    [[nodiscard]] virtual auto GetAggregatedTypeInfo(const string& type_name, size_t& size, const vector<BaseTypeInfo>** layout) const -> bool = 0;
    [[nodiscard]] virtual auto ResolveEnumValue(const string& enum_value_name, bool* failed = nullptr) const -> int = 0;
    [[nodiscard]] virtual auto ResolveEnumValue(const string& enum_name, const string& value_name, bool* failed = nullptr) const -> int = 0;
    [[nodiscard]] virtual auto ResolveEnumValueName(const string& enum_name, int value, bool* failed = nullptr) const -> const string& = 0;
    [[nodiscard]] virtual auto ResolveGenericValue(const string& str, bool* failed = nullptr) -> int = 0;
    [[nodiscard]] virtual auto CheckMigrationRule(hstring rule_name, hstring extra_info, hstring target) const -> optional<hstring> = 0;
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
