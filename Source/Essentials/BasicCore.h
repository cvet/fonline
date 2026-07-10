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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "EngineConfig.gen.h"

// Operating system (passed outside)
#if FO_WINDOWS + FO_LINUX + FO_MAC + FO_ANDROID + FO_IOS + FO_WEB == 0
#error "Operating system not specified"
#endif
#if FO_WINDOWS + FO_LINUX + FO_MAC + FO_ANDROID + FO_IOS + FO_WEB != 1
#error "Multiple operating systems not allowed"
#endif

#if __cplusplus < 202002L
#error "Invalid __cplusplus version, must be at least C++20 (202002)"
#endif
#ifndef __has_feature
#define __has_feature(x) 0
#endif

// Standard API
#include <algorithm>
#include <any>
#include <array>
#include <atomic>
#include <cassert>
#include <cctype>
#include <cfloat>
#include <charconv>
#include <chrono>
#include <climits>
#include <clocale>
#include <cmath>
#include <concepts>
#include <condition_variable>
#include <csignal>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <numbers>
#include <numeric>
#include <optional>
#include <random>
#include <ranges>
#include <set>
#include <shared_mutex>
#include <span>
#include <sstream>
#include <stack>
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

// Small vector
#define GCH_SMALL_VECTOR_DEFAULT_SIZE 64
#define GCH_UNRESTRICTED_DEFAULT_BUFFER_SIZE
#include "small_vector.hpp"

// Custom hashmaps
#include "ankerl/unordered_dense.h"

// OS specific API
#if FO_MAC || FO_IOS
#include <TargetConditionals.h>
#elif FO_WEB
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

// Tracy
#if FO_TRACY
#ifndef TRACY_ENABLE
#error TRACY_ENABLE not defined
#endif

#include "tracy/Tracy.hpp"
#include "tracy/TracyC.h"
#endif

// Compiler warnings disable helper
#if defined(__clang__) && defined(_MSC_VER)
#define FO_DISABLE_WARNINGS_PUSH() __pragma(warning(push, 0)) _Pragma("clang diagnostic push") _Pragma("clang diagnostic ignored \"-Weverything\"")
#define FO_DISABLE_WARNINGS_POP() _Pragma("clang diagnostic pop") __pragma(warning(pop))
#elif defined(__clang__)
#define FO_DISABLE_WARNINGS_PUSH() _Pragma("clang diagnostic push") _Pragma("clang diagnostic ignored \"-Weverything\"")
#define FO_DISABLE_WARNINGS_POP() _Pragma("clang diagnostic pop")
#elif defined(_MSC_VER)
#define FO_DISABLE_WARNINGS_PUSH() __pragma(warning(push, 0))
#define FO_DISABLE_WARNINGS_POP() __pragma(warning(pop))
#else
#define FO_DISABLE_WARNINGS_PUSH()
#define FO_DISABLE_WARNINGS_POP()
#endif

// Compiler-specific single-warning suppression helpers
#if defined(__GNUC__) && !defined(__clang__)
#define FO_GCC_IGNORE_WARNINGS_PUSH_STRINGIFY(x) _Pragma(#x)
#define FO_GCC_IGNORE_WARNINGS_PUSH(warning) _Pragma("GCC diagnostic push") FO_GCC_IGNORE_WARNINGS_PUSH_STRINGIFY(GCC diagnostic ignored warning)
#define FO_GCC_IGNORE_WARNINGS_POP() _Pragma("GCC diagnostic pop")
#else
#define FO_GCC_IGNORE_WARNINGS_PUSH(warning)
#define FO_GCC_IGNORE_WARNINGS_POP()
#endif

#if defined(__clang__)
#define FO_CLANG_IGNORE_WARNINGS_PUSH_STRINGIFY(x) _Pragma(#x)
#define FO_CLANG_IGNORE_WARNINGS_PUSH(warning) _Pragma("clang diagnostic push") FO_CLANG_IGNORE_WARNINGS_PUSH_STRINGIFY(clang diagnostic ignored warning)
#define FO_CLANG_IGNORE_WARNINGS_POP() _Pragma("clang diagnostic pop")
#else
#define FO_CLANG_IGNORE_WARNINGS_PUSH(warning)
#define FO_CLANG_IGNORE_WARNINGS_POP()
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#define FO_MSVC_IGNORE_WARNINGS_PUSH(warning_code) __pragma(warning(push)) __pragma(warning(disable : warning_code))
#define FO_MSVC_IGNORE_WARNINGS_POP() __pragma(warning(pop))
#else
#define FO_MSVC_IGNORE_WARNINGS_PUSH(warning_code)
#define FO_MSVC_IGNORE_WARNINGS_POP()
#endif

// Force inline helper
#if defined(__GNUC__)
#define FO_FORCE_INLINE __attribute__((always_inline)) inline
#elif defined(_MSC_VER)
#define FO_FORCE_INLINE __forceinline
#else
#define FO_FORCE_INLINE inline
#endif

// Prevent inline helper
#if defined(__GNUC__)
#define FO_NO_INLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define FO_NO_INLINE __declspec(noinline)
#else
#define FO_NO_INLINE
#endif

// Export symbol
#if defined(__GNUC__)
#define FO_EXPORT_FUNC extern "C" __attribute__((visibility("default"), used))
#elif defined(_MSC_VER)
#define FO_EXPORT_FUNC extern "C" __declspec(dllexport)
#else
#define FO_EXPORT_FUNC extern "C"
#endif

// Keep data symbol from being eliminated by LTO (used for markers patched at packaging time)
#if defined(__GNUC__)
#define FO_KEEP_DATA_SYMBOL [[gnu::used]] alignas(uint32_t) static volatile
#else
#define FO_KEEP_DATA_SYMBOL alignas(uint32_t) static volatile
#endif

// Compiler instrumentation
#if __has_feature(memory_sanitizer) || defined(__SANITIZE_MEMORY__)
#define FO_MEMORY_SANITIZER 1
#else
#define FO_MEMORY_SANITIZER 0
#endif
#if __has_feature(thread_sanitizer) || defined(__SANITIZE_THREAD__)
#define FO_THREAD_SANITIZER 1
extern "C" void __tsan_acquire(void* addr);
extern "C" void __tsan_release(void* addr);
#else
#define FO_THREAD_SANITIZER 0
#endif

// Namespace management
#if FO_USE_NAMESPACE
#define FO_NAMESPACE_NAME fo
#define FO_NAMESPACE FO_NAMESPACE_NAME::
#define FO_BEGIN_NAMESPACE \
    namespace FO_NAMESPACE_NAME \
    {
#define FO_END_NAMESPACE }
#define FO_USING_NAMESPACE() using namespace FO_NAMESPACE_NAME
#else
#define FO_NAMESPACE_NAME
#define FO_NAMESPACE
#define FO_BEGIN_NAMESPACE
#define FO_END_NAMESPACE
#define FO_USING_NAMESPACE()
#endif

FO_BEGIN_NAMESPACE

// Base types
using float32_t = float;
using float64_t = double;

inline constexpr size_t MAX_SERIALIZED_ALIGNMENT = 8; // Fixed cross-platform serialized-data alignment contract

// Check the sizes of base types
static_assert(sizeof(bool) == 1);
static_assert(sizeof(size_t) >= 4);
static_assert(sizeof(int) >= 4);
static_assert(sizeof(float32_t) == 4);
static_assert(sizeof(float64_t) == 8);
static_assert(CHAR_BIT == 8); // NOLINT(misc-redundant-expression)
static_assert((MAX_SERIALIZED_ALIGNMENT & (MAX_SERIALIZED_ALIGNMENT - 1)) == 0);
static_assert(alignof(int64_t) <= MAX_SERIALIZED_ALIGNMENT);
static_assert(alignof(uint64_t) <= MAX_SERIALIZED_ALIGNMENT);
static_assert(alignof(float64_t) <= MAX_SERIALIZED_ALIGNMENT);

// Bind to global scope frequently used types
using std::array;
using std::function;
using std::initializer_list;
using std::optional;
using std::pair;
using std::span;
using std::string_view;
using std::tuple;
using std::variant;

template<typename T>
using const_span = span<const T>;

namespace hashing = ankerl::unordered_dense;
namespace hashing_ex = ankerl::unordered_dense::detail::wyhash;

// String view for null terminated string
class string_view_nt : public string_view
{
public:
    // ReSharper disable once CppNonExplicitConvertingConstructor
    constexpr string_view_nt(const char* str) noexcept :
        string_view(str)
    {
    }

    // ReSharper disable once CppInconsistentNaming
    [[nodiscard]] auto c_str() const noexcept -> const char* { return data(); }
};

FO_END_NAMESPACE
template<typename T>
    requires(std::is_same_v<T, FO_NAMESPACE string_view_nt>)
struct std::formatter<T> : formatter<FO_NAMESPACE string_view> // NOLINT(cert-dcl58-cpp)
{
    template<typename FormatContext>
    auto format(const T& value, FormatContext& ctx) const
    {
        return formatter<FO_NAMESPACE string_view>::format(static_cast<FO_NAMESPACE string_view>(value), ctx);
    }
};
FO_BEGIN_NAMESPACE

// String for using in templates
template<size_t N>
struct fixed_string
{
    // ReSharper disable once CppNonExplicitConvertingConstructor
    constexpr fixed_string(const char (&s)[N]) noexcept { std::copy_n(s, N, data); }
    constexpr auto operator<=>(const fixed_string&) const noexcept = default;
    constexpr auto c_str() const noexcept -> const char* { return data; }
    char data[N] {};
};

// Generic helpers
[[noreturn]] extern void ExitApp(bool success) noexcept;

// Always-on assertion for Essentials modules that sit above ExceptionHandling in the include order
// (e.g. SmartPointers) and therefore cannot use FO_STRONG_ASSERT. Defined in ExceptionHandling.cpp; it
// produces the same StrongAssertationException report and process exit as FO_STRONG_ASSERT.
[[noreturn]] extern void ReportStrongAssertAndExit(const char* message, const char* file, int32_t line) noexcept;

#define FO_BASIC_STRONG_ASSERT(expr) \
    if (!(expr)) [[unlikely]] { \
        FO_NAMESPACE ReportStrongAssertAndExit(#expr, __FILE__, __LINE__); \
    }

extern auto IsRunInDebugger() noexcept -> bool;
extern auto BreakIntoDebugger() noexcept -> bool;

extern auto ItoA(int64_t num, char buf[64], int32_t base) noexcept -> const char*;

template<typename... T>
FO_FORCE_INLINE constexpr void ignore_unused(const T&... /*unused*/)
{
}

FO_FORCE_INLINE void TSanAcquire(void* addr) noexcept
{
#if FO_THREAD_SANITIZER
    __tsan_acquire(addr);
#else
    ignore_unused(addr);
#endif
}

FO_FORCE_INLINE void TSanRelease(void* addr) noexcept
{
#if FO_THREAD_SANITIZER
    __tsan_release(addr);
#else
    ignore_unused(addr);
#endif
}

// Explicit copying
template<typename T>
    requires(std::is_copy_constructible_v<std::remove_cvref_t<T>>)
constexpr auto copy(T&& value) noexcept(std::is_nothrow_copy_constructible_v<std::remove_cvref_t<T>>) -> std::remove_cvref_t<T> // NOLINT(cppcoreguidelines-missing-std-forward)
{
    if (std::is_rvalue_reference_v<T>) {
        return std::move(value); // NOLINT(bugprone-move-forwarding-reference)
    }
    else {
        return std::remove_cvref_t<T>(value);
    }
}

// C-strings literal helpers
constexpr auto const_hash(const char* input) noexcept -> uint32_t
{
    return *input != 0 ? static_cast<uint32_t>(*input) + 33 * const_hash(input + 1) : 5381;
}

constexpr auto operator""_hash(const char* str, size_t size) noexcept -> uint32_t
{
    (void)size;
    return const_hash(str);
}

constexpr auto operator""_len(const char* str, size_t size) noexcept -> size_t
{
    (void)str;
    return size;
}

// Macro helpers
#define FO_SCRIPT_API extern
#define FO_CONCAT(x, y) FO_CONCAT_INDIRECT(x, y)
#define FO_CONCAT_INDIRECT(x, y) x##y
#define FO_STRINGIFY(x) FO_STRINGIFY_INDIRECT(x)
#define FO_STRINGIFY_INDIRECT(x) #x
#define FO_LINE_STR FO_STRINGIFY(__LINE__)

// Template helpers
template<typename T, auto Member, typename Ret = void, typename... Args>
concept has_member = std::is_member_function_pointer_v<decltype(Member)> && requires(T t) {
    { (t.*Member)(std::declval<Args>()...) } -> std::convertible_to<Ret>;
};

template<typename Test, template<typename...> typename Ref>
struct is_specialization : std::false_type
{
};

template<template<typename...> typename Ref, typename... Args>
struct is_specialization<Ref<Args...>, Ref> : std::true_type
{
};

template<typename T, template<typename...> typename Ref>
concept specialization_of = is_specialization<T, Ref>::value;

template<typename From, typename To>
concept convertible_without_narrowing = requires(From&& x) {
    { std::type_identity_t<To[]> {std::forward<From>(x)} } -> std::same_as<To[1]>;
};

template<typename T>
inline constexpr bool always_false_v = false;

namespace details
{
    template<typename T>
    struct remove_all_pointers
    {
        using type = T;
    };

    template<typename T>
    struct remove_all_pointers<T*>
    {
        using type = typename remove_all_pointers<std::remove_cv_t<T>>::type;
    };
}

template<typename T>
using remove_all_pointers_t = typename details::remove_all_pointers<T>::type;

template<bool Enabled>
[[nodiscard]] bool build_condition() noexcept
{
    if constexpr (Enabled) {
        return true;
    }
    else {
        return false;
    }
}

// Stack unwind detector
class stack_unwind_detector
{
public:
    stack_unwind_detector() = default;
    stack_unwind_detector(const stack_unwind_detector&) = delete;
    stack_unwind_detector(stack_unwind_detector&&) noexcept = default;
    auto operator=(const stack_unwind_detector&) = delete;
    auto operator=(stack_unwind_detector&&) noexcept = delete;
    ~stack_unwind_detector() = default;
    [[nodiscard]] explicit operator bool() const noexcept { return _initCount != std::uncaught_exceptions(); }

private:
    decltype(std::uncaught_exceptions()) _initCount {std::uncaught_exceptions()};
};

// End of scope callbacks
template<std::invocable T>
class [[nodiscard]] scope_exit
{
public:
    static_assert(std::is_nothrow_invocable_v<T>);

    explicit scope_exit(T callback) noexcept :
        _callback {std::move(callback)}
    {
    }

    scope_exit(const scope_exit& other) = delete;
    scope_exit(scope_exit&& other) noexcept = default;
    auto operator=(const scope_exit& other) = delete;
    auto operator=(scope_exit&& other) noexcept = delete;

    ~scope_exit() noexcept
    {
        if (!_released) {
            _callback();
        }
    }

    void release() { _released = true; }

private:
    T _callback;
    bool _released {};
};

template<std::invocable T>
class [[nodiscard]] scope_success
{
public:
    explicit scope_success(T callback) noexcept :
        _callback {std::move(callback)}
    {
    }

    scope_success(const scope_success& other) = delete;
    scope_success(scope_success&& other) noexcept = default;
    auto operator=(const scope_success& other) = delete;
    auto operator=(scope_success&& other) noexcept = delete;

    ~scope_success()
    {
        if (!_released && !_stackUnwind) {
            _callback();
        }
    }

    void release() { _released = true; }

private:
    T _callback;
    stack_unwind_detector _stackUnwind {};
    bool _released {};
};

template<std::invocable T>
class [[nodiscard]] scope_fail
{
    static_assert(std::is_nothrow_invocable_v<T>);

public:
    explicit scope_fail(T callback) noexcept :
        _callback {std::move(callback)}
    {
    }

    scope_fail(const scope_fail& other) = delete;
    scope_fail(scope_fail&& other) noexcept = default;
    auto operator=(const scope_fail& other) = delete;
    auto operator=(scope_fail&& other) noexcept = delete;

    ~scope_fail() noexcept
    {
        if (!_released && _stackUnwind) {
            _callback();
        }
    }

    void release() { _released = true; }

private:
    T _callback;
    stack_unwind_detector _stackUnwind {};
    bool _released {};
};

// Refcount base class
template<typename T>
class RefCounted
{
public:
    RefCounted() noexcept = default;
    RefCounted(const RefCounted&) = delete;
    RefCounted(RefCounted&&) noexcept = delete;
    auto operator=(const RefCounted&) = delete;
    auto operator=(RefCounted&&) noexcept = delete;
    ~RefCounted() = default;

    [[nodiscard]] auto GetRefCount() const noexcept -> int32_t { return _refCounter.load(std::memory_order_relaxed); }

    void AddRef() const noexcept { _refCounter.fetch_add(1, std::memory_order_relaxed); }

    void Release() const noexcept
    {
        if (_refCounter.fetch_sub(1, std::memory_order_release) == 1) {
            std::atomic_thread_fence(std::memory_order_acquire);
            delete static_cast<const T*>(this);
        }
    }

private:
    mutable std::atomic_int _refCounter {1};
};

// Enum operation helpers
template<typename T>
    requires(std::is_enum_v<T>)
constexpr auto IsEnumSet(T value, T check) noexcept -> bool
{
    using U = std::underlying_type_t<T>;
    return (static_cast<U>(value) & static_cast<U>(check)) != 0;
}

template<typename T, typename... Args>
    requires(std::is_enum_v<T> && (std::is_same_v<T, Args> && ...))
constexpr auto CombineEnum(T first, Args... rest) noexcept -> T
{
    using U = std::underlying_type_t<T>;
    return static_cast<T>((static_cast<U>(first) | ... | static_cast<U>(rest)));
}

// Enum formatter
FO_END_NAMESPACE
template<typename T>
    requires(std::is_enum_v<T>)
struct std::formatter<T> : formatter<std::underlying_type_t<T>> // NOLINT(cert-dcl58-cpp)
{
    template<typename FormatContext>
    auto format(const T& value, FormatContext& ctx) const
    {
        return formatter<std::underlying_type_t<T>>::format(static_cast<std::underlying_type_t<T>>(value), ctx);
    }
};
FO_BEGIN_NAMESPACE

// Formatters
#define FO_DECLARE_TYPE_FORMATTER(type, ...) \
    FO_END_NAMESPACE \
    template<> \
    struct std::formatter<type> : formatter<FO_NAMESPACE string_view> \
    { \
        template<typename FormatContext> \
        auto format(const type& value, FormatContext& ctx) const \
        { \
            FO_NAMESPACE string buf; \
            std::format_to(std::back_inserter(buf), __VA_ARGS__); \
            return formatter<FO_NAMESPACE string_view>::format(buf, ctx); \
        } \
    }; \
    FO_BEGIN_NAMESPACE

#define FO_DECLARE_TYPE_FORMATTER_EXT(type, ...) \
    FO_END_NAMESPACE \
    template<> \
    struct std::formatter<type> : formatter<FO_NAMESPACE string_view> \
    { \
        template<typename FormatContext> \
        auto format(const type& value, FormatContext& ctx) const \
        { \
            return formatter<FO_NAMESPACE string_view>::format(__VA_ARGS__, ctx); \
        } \
    }; \
    FO_BEGIN_NAMESPACE

#define FO_DECLARE_TYPE_FORMATTER_EXT2(type, ...) \
    FO_END_NAMESPACE \
    template<> \
    struct std::formatter<type> : formatter<FO_NAMESPACE string_view> \
    { \
        template<typename FormatContext> \
        auto format(const type& value, FormatContext& ctx) const \
        { \
            __VA_ARGS__; \
        } \
    }; \
    FO_BEGIN_NAMESPACE

#define FO_DECLARE_TYPE_PARSER(type, ...) \
    FO_END_NAMESPACE \
    inline auto operator>>(std::istream& istr, type& value)->std::istream& \
    { \
        if (!(istr >> __VA_ARGS__)) { \
            value = {}; \
        } \
        return istr; \
    } \
    FO_BEGIN_NAMESPACE

#define FO_DECLARE_TYPE_PARSER_EXT(type, op1, op2, op3) \
    FO_END_NAMESPACE \
    inline auto operator>>(std::istream& istr, type& value)->std::istream& \
    { \
        op1; \
        value = !!(istr >> op2) ? op3 : type {}; \
        return istr; \
    } \
    FO_BEGIN_NAMESPACE

#define FO_DECLARE_TYPE_PARSER_EXT2(type, ...) \
    FO_END_NAMESPACE \
    inline auto operator>>(std::istream& istr, type& value)->std::istream& \
    { \
        __VA_ARGS__; \
    } \
    FO_BEGIN_NAMESPACE

#define FO_DECLARE_TYPE_HASHER(type) \
    FO_END_NAMESPACE \
    template<> \
    struct FO_NAMESPACE hashing::hash<type> \
    { \
        using is_avalanching = void; \
        auto operator()(const type& v) const noexcept \
        { \
            static_assert(std::has_unique_object_representations_v<type>); \
            return FO_NAMESPACE hashing_ex::hash(&v, sizeof(v)); \
        } \
    }; \
    FO_BEGIN_NAMESPACE

#define FO_DECLARE_TYPE_HASHER_EXT(type, ...) \
    FO_END_NAMESPACE \
    template<> \
    struct FO_NAMESPACE hashing::hash<type> \
    { \
        using is_avalanching = void; \
        auto operator()(const type& v) const noexcept \
        { \
            return FO_NAMESPACE hashing_ex::hash(__VA_ARGS__); \
        } \
    }; \
    FO_BEGIN_NAMESPACE

#define FO_DECLARE_TYPE_HASHER_EXT2(type, ...) \
    FO_END_NAMESPACE \
    template<> \
    struct FO_NAMESPACE hashing::hash<type> \
    { \
        using is_avalanching = void; \
        auto operator()(const type& v) const noexcept \
        { \
            return __VA_ARGS__; \
        } \
    }; \
    FO_BEGIN_NAMESPACE

// Math constants
constexpr auto SQRT3_FLOAT = std::numbers::sqrt3_v<float32_t>;
constexpr auto SQRT3_X2_FLOAT = std::numbers::sqrt3_v<float32_t> * 2.0f;
constexpr auto RAD_TO_DEG_FLOAT = 180.0f / std::numbers::pi_v<float32_t>;
constexpr auto DEG_TO_RAD_FLOAT = std::numbers::pi_v<float32_t> / 180.0f;

FO_END_NAMESPACE
