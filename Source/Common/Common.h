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
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <deque>
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
#include <span.hpp>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
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

// String formatting lib
#include "fmt/format.h"

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
#include "WinApi-Include.h"
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
using std::unordered_set;
using std::variant;
using std::vector;
using tcb::span;

template<typename T>
using unique_del_ptr = unique_ptr<T, std::function<void(T*)>>;
template<typename T>
using const_span = span<const T>;

// Strong types
template<typename T>
struct strong_type
{
    using underlying_type = T;

    constexpr strong_type() noexcept :
        _value {}
    {
    }

    constexpr explicit strong_type(T v) noexcept :
        _value {v}
    {
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept { return _value != T {}; }

    [[nodiscard]] constexpr auto operator==(const strong_type& other) const noexcept -> bool { return _value == other._value; }
    [[nodiscard]] constexpr auto operator!=(const strong_type& other) const noexcept -> bool { return _value != other._value; }

    [[nodiscard]] constexpr auto underlying_value() noexcept -> T& { return _value; }
    [[nodiscard]] constexpr auto underlying_value() const noexcept -> const T& { return _value; }

private:
    T _value;
};

template<typename T>
struct std::hash<strong_type<T>>
{
    size_t operator()(const strong_type<T>& t) const noexcept { return std::hash<T>()(t.underlying_value()); }
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

template<typename T>
constexpr bool is_strong_type_func(const strong_type<T>*)
{
    return true;
}
constexpr bool is_strong_type_func(...)
{
    return false;
}
template<typename T>
struct is_strong_type : std::integral_constant<bool, is_strong_type_func(static_cast<T*>(nullptr))>
{
};

///@ ExportType ident_t uint RelaxedStrong
using ident_t = strong_type<uint>;
static_assert(sizeof(ident_t) == sizeof(uint));

///@ ExportType tick_t uint RelaxedStrong
using tick_t = strong_type<uint>;
static_assert(sizeof(tick_t) == sizeof(uint));

// Game clock (since program start)
struct steady_clock_since_program_start
{
    using rep = long long;
    using period = std::nano;
    using duration = std::chrono::nanoseconds;
    using time_point = std::chrono::time_point<std::chrono::steady_clock>;
    static constexpr bool is_steady = true;

    static time_point start;

    [[nodiscard]] static time_point now() noexcept { return time_point {std::chrono::steady_clock::now() - start}; }
};

using time_point = steady_clock_since_program_start::time_point;
using time_duration = steady_clock_since_program_start::duration;

template<typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
inline static constexpr auto time_duration_to_ms(const time_duration& duration) -> T
{
    return static_cast<T>(std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
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
        return format_to(ctx.out(), "{}", time_duration_to_ms<size_t>(t));
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
        return format_to(ctx.out(), "{}", time_duration_to_ms<size_t>(t.time_since_epoch()));
    }
};

// Math types
// Todo: replace depedency from Assimp types (matrix/vector/quaternion/color)
#include "assimp/types.h"
using vec3 = aiVector3D;
using mat44 = aiMatrix4x4;
using quaternion = aiQuaternion;
using color4 = aiColor4D;

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
#ifdef TRACY_ENABLE
#include "tracy/Tracy.hpp"
#include "tracy/TracyC.h"

using tracy::SourceLocationData;

#if !FO_NO_MANUAL_STACK_TRACE
#define STACK_TRACE_FIRST_ENTRY() \
    SetMainThread(); \
    STACK_TRACE_ENTRY()
#define STACK_TRACE_ENTRY() \
    ZoneScoped; \
    auto ___fo_stack_entry = StackTraceScopeEntry(TracyConcat(__tracy_source_location, __LINE__))
#else
#define STACK_TRACE_FIRST_ENTRY() \
    SetMainThread(); \
    STACK_TRACE_ENTRY()
#define STACK_TRACE_ENTRY() ZoneScoped
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
#define STACK_TRACE_FIRST_ENTRY() \
    SetMainThread(); \
    STACK_TRACE_ENTRY()
#define STACK_TRACE_ENTRY() \
    static constexpr SourceLocationData CONCAT(___fo_source_location, __LINE__) {nullptr, __FUNCTION__, __FILE__, (uint32_t)__LINE__}; \
    auto ___fo_stack_entry = StackTraceScopeEntry(CONCAT(___fo_source_location, __LINE__))
#else
#define STACK_TRACE_FIRST_ENTRY() SetMainThread()
#define STACK_TRACE_ENTRY()
#endif
#define NO_STACK_TRACE_ENTRY()
#endif

extern void SetMainThread() noexcept;
extern auto IsMainThread() noexcept -> bool;
extern void PushStackTrace(const SourceLocationData& loc) noexcept;
extern void PopStackTrace() noexcept;
extern auto GetStackTrace() -> string;

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
extern auto BreakIntoDebugger(string_view error_message = "") -> bool;
extern void CreateDumpMessage(string_view appendix, string_view message);
[[noreturn]] extern void ReportExceptionAndExit(const std::exception& ex);
extern void ReportExceptionAndContinue(const std::exception& ex);
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
                _exceptionMessage.append("\n  - ").append(param); \
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
                _exceptionMessage.append("\n  - ").append(param); \
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

    void Unsubscribe()
    {
        for (auto& cb : _unsubscribeCallbacks) {
            cb._unsubscribeCallback();
        }
        _unsubscribeCallbacks.clear();
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
            ThrowException();
        }
    }

    [[nodiscard]] auto operator+=(Callback cb) -> EventUnsubscriberCallback
    {
        auto it = _subscriberCallbacks.insert(_subscriberCallbacks.end(), cb);
        return EventUnsubscriberCallback([this, it]() { _subscriberCallbacks.erase(it); });
    }

private:
    void ThrowException()
    {
        if (std::uncaught_exceptions() == 0) {
            throw GenericException("Some of subscriber still alive", _subscriberCallbacks.size());
        }
    }

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

    virtual ~RefCounter()
    {
        if (_ptrCounter != 0) {
            ThrowException();
        }
    }

private:
    void ThrowException() const
    {
        if (std::uncaught_exceptions() == 0) {
            throw GenericException("Some of pointer still alive", _ptrCounter.load());
        }
    }

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
        if (_value) {
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
            if (_value) {
                --_value->ptrCounter;
            }
            _value = other._value;
            if (_value) {
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
            if (_value) {
                --_value->ptrCounter;
            }
            _value = other._value;
            other._value = nullptr;
        }
        return *this;
    }

    ~ptr()
    {
        if (_value) {
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
constexpr uint const_hash(const char* input)
{
    return *input != 0 ? static_cast<uint>(*input) + 33 * const_hash(input + 1) : 5381;
}

auto constexpr operator""_hash(const char* str, size_t size) -> uint
{
    return const_hash(str);
}

auto constexpr operator""_len(const char* str, size_t size) -> size_t
{
    (void)str;
    return size;
}

// Scriptable object class decorator
#define SCRIPTABLE_OBJECT() \
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
    static_assert(std::is_nothrow_invocable_v<T>, "T must be noexcept invocable or use ScopeCallbackExt for callbacks that may throw");
    explicit ScopeCallback(T safe_callback) :
        _safeCallback {std::move(safe_callback)}
    {
    }
    ScopeCallback(ScopeCallback&& other) noexcept = default;
    ScopeCallback(const ScopeCallback& other) = delete;
    auto operator=(const ScopeCallback& other) = delete;
    auto operator=(ScopeCallback&& other) = delete;
    ~ScopeCallback() noexcept { _safeCallback(); }

private:
    T _safeCallback;
};

template<typename T, typename T2>
class [[nodiscard]] ScopeCallbackExt
{
public:
    static_assert(std::is_invocable_v<T>, "T must be invocable");
    static_assert(!std::is_nothrow_invocable_v<T>, "T invocable is safe, use ScopeCallback instead of this");
    static_assert(std::is_nothrow_invocable_v<T2>, "T2 must be noexcept invocable");
    ScopeCallbackExt(T unsafe_callback, T2 safe_callback) :
        _unsafeCallback {std::move(unsafe_callback)},
        _safeCallback {std::move(safe_callback)}
    {
    }
    ScopeCallbackExt(ScopeCallbackExt&& other) noexcept = default;
    ScopeCallbackExt(const ScopeCallbackExt& other) = delete;
    auto operator=(const ScopeCallbackExt& other) = delete;
    auto operator=(ScopeCallbackExt&& other) = delete;
    ~ScopeCallbackExt() noexcept(false)
    {
        if (std::uncaught_exceptions() == 0) {
            _unsafeCallback(); // May throw
        }
        else {
            _safeCallback();
        }
    }

private:
    T _unsafeCallback;
    T2 _safeCallback;
};

// Generic helpers
#define STRINGIFY(x) STRINGIFY2(x)
#define STRINGIFY2(x) #x
#define LINE_STR STRINGIFY(__LINE__)
#define UNUSED_VARIABLE(x) (void)(x)
#define NON_CONST_METHOD_HINT() _nonConstHelper = !_nonConstHelper
#define NON_CONST_METHOD_HINT_ONELINE() _nonConstHelper = !_nonConstHelper;
#define COLOR_RGBA(a, r, g, b) ((uint)((((a)&0xFF) << 24) | (((r)&0xFF) << 16) | (((g)&0xFF) << 8) | ((b)&0xFF)))
#define COLOR_RGB(r, g, b) COLOR_RGBA(0xFF, r, g, b)
#define COLOR_SWAP_RB(c) (((c)&0xFF00FF00) | (((c)&0x00FF0000) >> 16) | (((c)&0x000000FF) << 16))
#define COLOR_CHANGE_ALPHA(v, a) ((((v) | 0xFF000000) ^ 0xFF000000) | ((uint)(a)&0xFF) << 24)

// Bits
#define BIN_N(x) ((x) | (x) >> 3 | (x) >> 6 | (x) >> 9)
#define BIN_B(x) ((x)&0xf | (x) >> 12 & 0xf0)
#define BIN8(v) (BIN_B(BIN_N(0x##v)))
#define BIN16(bin16, bin8) ((BIN8(bin16) << 8) | (BIN8(bin8)))
#define BIN32(bin32, bin24, bin16, bin8) ((BIN8(bin32) << 24) | (BIN8(bin24) << 16) | (BIN8(bin16) << 8) | (BIN8(bin8)))

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

// Float constants
constexpr auto PI_FLOAT = 3.14159265f;
constexpr auto SQRT3_X2_FLOAT = 3.4641016151f;
constexpr auto SQRT3_FLOAT = 1.732050807f;
constexpr auto RAD_TO_DEG_FLOAT = 57.29577951f;
constexpr auto DEG_TO_RAD_FLOAT = 0.017453292f;

// Memory pool
template<typename T, int ChunkSize>
class MemoryPool final
{
public:
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool(MemoryPool&&) noexcept = default;
    auto operator=(const MemoryPool&) = delete;
    auto operator=(MemoryPool&&) noexcept -> MemoryPool& = delete;

    MemoryPool()
    {
        _freePtrs.reserve(ChunkSize);
        Grow();
    }

    ~MemoryPool()
    {
        for (const auto* chunk : _chunks) {
            delete[] chunk;
        }
    }

    auto Get() -> T*
    {
        if (_freePtrs.empty()) {
            Grow();
        }

        T* result = _freePtrs.back();
        _freePtrs.pop_back();
        return result;
    }

    void Put(T* ptr) //
    {
        _freePtrs.emplace_back(ptr);
    }

private:
    void Grow()
    {
        auto* new_chunk = new T[ChunkSize]();
        _chunks.emplace_back(new_chunk);
        for (auto i = 0; i < ChunkSize; i++) {
            _freePtrs.emplace_back(&new_chunk[i]);
        }
    }

    vector<T*> _chunks {};
    vector<T*> _freePtrs {};
};

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
using IRect = TRect<int>;
using FRect = TRect<float>;

template<typename T>
struct TPoint
{
    TPoint() :
        X(0),
        Y(0)
    {
    }
    template<typename T2>
    TPoint(const TPoint<T2>& r) :
        X(static_cast<T>(r.X)),
        Y(static_cast<T>(r.Y))
    {
    }
    TPoint(T x, T y) :
        X(x),
        Y(y)
    {
    }
    TPoint(const TPoint& fp, T ox, T oy) :
        X(fp.X + ox),
        Y(fp.Y + oy)
    {
    }

    template<typename T2>
    auto operator=(const TPoint<T2>& fp) -> TPoint&
    {
        X = static_cast<T>(fp.X);
        Y = static_cast<T>(fp.Y);
        return *this;
    }

    void Clear()
    {
        X = 0;
        Y = 0;
    }

    auto IsZero() const -> bool { return !X && !Y; }

    auto operator[](int index) -> T&
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

    auto operator()(T x, T y) -> TPoint&
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
    explicit hstring(entry* static_storage_entry) :
        _entry {static_storage_entry}
    {
    }
    // ReSharper disable once CppNonExplicitConversionOperator
    operator string_view() const { return _entry->Str; }
    explicit operator bool() const { return !(_entry->Hash == 0u); }
    auto operator==(const hstring& other) const { return _entry->Hash == other._entry->Hash; }
    auto operator!=(const hstring& other) const { return _entry->Hash != other._entry->Hash; }
    auto operator<(const hstring& other) const { return _entry->Hash < other._entry->Hash; }
    [[nodiscard]] auto as_hash() const -> hash_t { return _entry->Hash; }
    [[nodiscard]] auto as_int() const -> int { return static_cast<int>(_entry->Hash); }
    [[nodiscard]] auto as_uint() const -> uint { return _entry->Hash; }
    [[nodiscard]] auto as_str() const -> const string& { return _entry->Str; }

private:
    static entry _zeroEntry;

    entry* _entry {&_zeroEntry};
};
static_assert(sizeof(hstring::hash_t) == 4);
static_assert(std::is_standard_layout_v<hstring>);

template<>
struct std::hash<hstring>
{
    size_t operator()(const hstring& s) const noexcept { return std::hash<hstring::hash_t>()(s.as_hash()); }
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

// Generic constants
// Todo: eliminate as much defines as possible
// Todo: convert all defines to constants and enums
static constexpr auto LOCAL_CONFIG_NAME = "LocalSettings.focfg";
static constexpr auto MAX_HOLO_INFO = 250;
static constexpr auto PROCESS_TALK_TIME = 1000;
static constexpr auto MAX_ADDED_NOGROUP_ITEMS = 1000;
static constexpr auto LAYERS3D_COUNT = 30;
static constexpr float MIN_ZOOM = 0.1f;
static constexpr float MAX_ZOOM = 20.0f;

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

///@ ExportEnum
enum class CritterFindType : uint8
{
    Any = 0,
    Alive = 0x01,
    Dead = 0x02,
    Players = 0x10,
    Npc = 0x20,
    AlivePlayers = 0x11,
    DeadPlayers = 0x12,
    AliveNpc = 0x21,
    DeadNpc = 0x22,
};

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
static constexpr size_t GM_ZONES_FOG_SIZE = ((GM_MAXZONEX / 4) + ((GM_MAXZONEX % 4) != 0 ? 1 : 0)) * GM_MAXZONEY;
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

// Proto map hex flags
static constexpr uint8 FH_BLOCK = BIN8(00000001);
static constexpr uint8 FH_NOTRAKE = BIN8(00000010);
static constexpr uint8 FH_STATIC_TRIGGER = BIN8(00100000);

// Map instance hex flags
static constexpr uint8 FH_CRITTER = BIN8(00000001);
static constexpr uint8 FH_DEAD_CRITTER = BIN8(00000010);
static constexpr uint8 FH_DOOR = BIN8(00001000);
static constexpr uint8 FH_BLOCK_ITEM = BIN8(00010000);
static constexpr uint8 FH_NRAKE_ITEM = BIN8(00100000);
static constexpr uint8 FH_TRIGGER = BIN8(01000000);
static constexpr uint8 FH_GAG_ITEM = BIN8(10000000);

// Both proto map and map instance flags
static constexpr uint16 FH_NOWAY = BIN16(00010001, 00000001);
static constexpr uint16 FH_NOSHOOT = BIN16(00100000, 00000010);

// Coordinates
static constexpr uint16 MAXHEX_DEFAULT = 200;
static constexpr uint16 MAXHEX_MIN = 10;
static constexpr uint16 MAXHEX_MAX = 4000;

// Answer
static constexpr uint8 ANSWER_BEGIN = 0xF0;
static constexpr uint8 ANSWER_END = 0xF1;
static constexpr uint8 ANSWER_BARTER = 0xF2;

// Critter actions
// Flags for chosen:
// l - hardcoded local call
// s - hardcoded server call
// for all others critters actions call only server
//  flags actionExt item
static constexpr int ACTION_MOVE_ITEM = 2; // l s from slot +
static constexpr int ACTION_MOVE_ITEM_SWAP = 3; // l s from slot +
static constexpr int ACTION_DROP_ITEM = 5; // l s from slot +
static constexpr int ACTION_KNOCKOUT = 16; // s 0 - knockout anim2begin
static constexpr int ACTION_STANDUP = 17; // s 0 - knockout anim2end
static constexpr int ACTION_FIDGET = 18; // l
static constexpr int ACTION_DEAD = 19; // s dead type anim2 (see Anim2 in _animation.fos)
static constexpr int ACTION_CONNECT = 20;
static constexpr int ACTION_DISCONNECT = 21;
static constexpr int ACTION_RESPAWN = 22; // s
static constexpr int ACTION_REFRESH = 23; // s

// Look checks
static constexpr uint LOOK_CHECK_DIR = 0x01;
static constexpr uint LOOK_CHECK_SNEAK_DIR = 0x02;
static constexpr uint LOOK_CHECK_TRACE = 0x08;
static constexpr uint LOOK_CHECK_SCRIPT = 0x10;
static constexpr uint LOOK_CHECK_ITEM_SCRIPT = 0x20;
static constexpr uint LOOK_CHECK_TRACE_CLIENT = 0x40;

// Anims
static constexpr uint ANIM1_UNARMED = 1;
static constexpr uint ANIM2_IDLE = 1;
static constexpr uint ANIM2_WALK = 3;
static constexpr uint ANIM2_WALK_BACK = 15;
static constexpr uint ANIM2_LIMP = 4;
static constexpr uint ANIM2_RUN = 5;
static constexpr uint ANIM2_RUN_BACK = 16;
static constexpr uint ANIM2_TURN_RIGHT = 17;
static constexpr uint ANIM2_TURN_LEFT = 18;
static constexpr uint ANIM2_PANIC_RUN = 6;
static constexpr uint ANIM2_SNEAK_WALK = 7;
static constexpr uint ANIM2_SNEAK_RUN = 8;
static constexpr uint ANIM2_IDLE_PRONE_FRONT = 86;
static constexpr uint ANIM2_DEAD_FRONT = 102;

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

///@ ExportEnum
enum class EffectType : uint
{
    None = 0,
    GenericSprite = 0x00000001,
    CritterSprite = 0x00000002,
    TileSprite = 0x00000004,
    RoofSprite = 0x00000008,
    RainSprite = 0x00000010,
    SkinnedMesh = 0x00000400,
    Interface = 0x00001000,
    Contour = 0x00002000,
    Font = 0x00010000,
    Primitive = 0x00100000,
    Light = 0x00200000,
    Fog = 0x00400000,
    FlushRenderTarget = 0x01000000,
    FlushPrimitive = 0x04000000,
    FlushMap = 0x08000000,
    FlushLight = 0x10000000,
    FlushFog = 0x20000000,
    Offscreen = 0x40000000,
};

///@ ExportEnum
enum class ItemOwnership : uint8
{
    Nowhere = 0,
    CritterInventory = 1,
    MapHex = 2,
    ItemContainer = 3,
};

///@ ExportEnum
enum class CornerType : uint8
{
    NorthSouth = 0,
    West = 1,
    East = 2,
    South = 3,
    North = 4,
    EastWest = 5,
};

///@ ExportEnum
enum class CritterCondition : uint8
{
    Alive = 0,
    Knockout = 1,
    Dead = 2,
};

///@ ExportEnum
enum class MovingState : uint8
{
    InProgress = 0,
    Success = 1,
    TargetNotFound = 2,
    CantMove = 3,
    GagCritter = 4,
    GagItem = 5,
    InternalError = 6,
    HexTooFar = 7,
    HexBusy = 8,
    HexBusyRing = 9,
    Deadlock = 10,
    TraceFailed = 11,
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

// Client anim flags
static constexpr uint16 ANIMRUN_TO_END = 0x0001;
static constexpr uint16 ANIMRUN_FROM_END = 0x0002;
static constexpr uint16 ANIMRUN_CYCLE = 0x0004;
static constexpr uint16 ANIMRUN_STOP = 0x0008;
static constexpr auto ANIMRUN_SET_FRM(int frm) -> uint
{
    return static_cast<uint>(frm + 1) << 16;
}

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

class NameResolver
{
public:
    virtual ~NameResolver() = default;
    [[nodiscard]] virtual auto ResolveEnumValue(string_view enum_value_name, bool* failed = nullptr) const -> int = 0;
    [[nodiscard]] virtual auto ResolveEnumValue(string_view enum_name, string_view value_name, bool* failed = nullptr) const -> int = 0;
    [[nodiscard]] virtual auto ResolveEnumValueName(string_view enum_name, int value, bool* failed = nullptr) const -> string = 0;
    [[nodiscard]] virtual auto ToHashedString(string_view s, bool mustExists = false) const -> hstring = 0;
    [[nodiscard]] virtual auto ResolveHash(hstring::hash_t h, bool* failed = nullptr) const -> hstring = 0;
    [[nodiscard]] virtual auto ResolveGenericValue(string_view str, bool* failed = nullptr) -> int = 0;
};

class AnimationResolver
{
public:
    virtual ~AnimationResolver() = default;
    [[nodiscard]] virtual auto ResolveCritterAnimation(hstring arg1, uint arg2, uint arg3, uint& arg4, uint& arg5, int& arg6, int& arg7, string& arg8) -> bool = 0;
    [[nodiscard]] virtual auto ResolveCritterAnimationSubstitute(hstring arg1, uint arg2, uint arg3, hstring& arg4, uint& arg5, uint& arg6) -> bool = 0;
    [[nodiscard]] virtual auto ResolveCritterAnimationFallout(hstring arg1, uint& arg2, uint& arg3, uint& arg4, uint& arg5, uint& arg6) -> bool = 0;
};

class FrameBalancer
{
public:
    FrameBalancer(bool enabled, int fixed_fps);

    void StartLoop();
    void EndLoop();

private:
    bool _enabled {};
    int _fixedFps {};
    time_point _loopStart {};
    double _balance {};
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
