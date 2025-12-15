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
#define FO_HASH_NAMESPACE ankerl::unordered_dense::

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
#if defined(_MSC_VER)
#define FO_DISABLE_WARNINGS_PUSH() __pragma(warning(push, 0))
#define FO_DISABLE_WARNINGS_POP() __pragma(warning(pop))
#elif defined(__clang__)
#define FO_DISABLE_WARNINGS_PUSH() _Pragma("clang diagnostic push") _Pragma("clang diagnostic ignored \"-Weverything\"")
#define FO_DISABLE_WARNINGS_POP() _Pragma("clang diagnostic pop")
#else
#define FO_DISABLE_WARNINGS_PUSH()
#define FO_DISABLE_WARNINGS_POP()
#endif

// Force inline helper
#if defined(__GNUC__)
#define FO_FORCE_INLINE __attribute__((always_inline)) inline
#elif defined(_MSC_VER)
#define FO_FORCE_INLINE __forceinline
#else
#define FO_FORCE_INLINE inline
#endif

// Export symbol
#if defined(__GNUC__)
#define FO_EXPORT_FUNC extern "C" __attribute__((visibility("default"), used))
#elif defined(_MSC_VER)
#define FO_EXPORT_FUNC extern "C" __declspec(dllexport)
#else
#define FO_EXPORT_FUNC extern "C"
#endif

// Namespace management
#if FO_USE_NAMESPACE
#define FO_NAMESPACE_NAME fo
#define FO_NAMESPACE FO_NAMESPACE_NAME::
#define FO_BEGIN_NAMESPACE() \
    namespace FO_NAMESPACE_NAME \
    {
#define FO_END_NAMESPACE() }
#define FO_USING_NAMESPACE() using namespace FO_NAMESPACE_NAME
#else
#define FO_NAMESPACE_NAME
#define FO_NAMESPACE
#define FO_BEGIN_NAMESPACE()
#define FO_END_NAMESPACE()
#define FO_USING_NAMESPACE()
#endif

FO_BEGIN_NAMESPACE();

// Base types
using int8 = std::int8_t;
using uint8 = std::uint8_t;
using int16 = std::int16_t;
using int32 = std::int32_t;
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
using int64 = std::int64_t;
using uint64 = std::uint64_t;
using float32 = float;
using float64 = double;

// Check the sizes of base types
static_assert(sizeof(int8) == 1);
static_assert(sizeof(uint8) == 1);
static_assert(sizeof(int16) == 2);
static_assert(sizeof(uint16) == 2);
static_assert(sizeof(int32) == 4);
static_assert(sizeof(uint32) == 4);
static_assert(sizeof(int64) == 8);
static_assert(sizeof(uint64) == 8);
static_assert(sizeof(bool) == 1);
static_assert(sizeof(size_t) >= 4);
static_assert(sizeof(int) >= 4);
static_assert(sizeof(float32) == 4);
static_assert(sizeof(float64) == 8);
static_assert(CHAR_BIT == 8); // NOLINT(misc-redundant-expression)

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

FO_END_NAMESPACE();
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
FO_BEGIN_NAMESPACE();

// String for use in templates
template<size_t N>
struct fixed_string
{
    // ReSharper disable once CppNonExplicitConvertingConstructor
    constexpr fixed_string(char const (&s)[N]) { std::copy_n(s, N, data); }
    constexpr auto operator<=>(fixed_string const&) const = default;
    char data[N] {};
};

// Generic helpers
[[noreturn]] extern void ExitApp(bool success) noexcept;

extern auto IsRunInDebugger() noexcept -> bool;
extern auto BreakIntoDebugger() noexcept -> bool;

extern auto ItoA(int64 num, char buf[64], int32 base) noexcept -> const char*;

template<typename... T>
FO_FORCE_INLINE constexpr void ignore_unused(const T&... /*unused*/)
{
}

// Explicit copying
// Todo: optimize copy() to pass placement storage for value
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
constexpr auto const_hash(const char* input) noexcept -> uint32
{
    return *input != 0 ? static_cast<uint32>(*input) + 33 * const_hash(input + 1) : 5381;
}

auto constexpr operator""_hash(const char* str, size_t size) noexcept -> uint32
{
    (void)size;
    return const_hash(str);
}

auto constexpr operator""_len(const char* str, size_t size) noexcept -> size_t
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

template<typename From, typename To>
concept is_convertible_without_narrowing = requires(From&& x) {
    { std::type_identity_t<To[]> {std::forward<From>(x)} } -> std::same_as<To[1]>;
};

// End of scope callback
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

// Stack unwind detector
class StackUnwindDetector
{
public:
    StackUnwindDetector() = default;
    StackUnwindDetector(const StackUnwindDetector&) = delete;
    StackUnwindDetector(StackUnwindDetector&&) noexcept = default;
    auto operator=(const StackUnwindDetector&) = delete;
    auto operator=(StackUnwindDetector&&) noexcept = delete;
    ~StackUnwindDetector() = default;
    [[nodiscard]] explicit operator bool() const noexcept { return _initCount != std::uncaught_exceptions(); }

private:
    decltype(std::uncaught_exceptions()) _initCount {std::uncaught_exceptions()};
};

// Scriptable object class decorator
#define FO_SCRIPTABLE_OBJECT_BEGIN() \
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
    mutable std::atomic_int RefCounter {1}
#define FO_SCRIPTABLE_OBJECT_END() \
    bool _nonConstHelper \
    { \
    }

// Bit operation helpers
template<typename T>
    requires(std::is_unsigned_v<T>)
constexpr auto IsBitSet(T x, T y) noexcept -> bool
{
    return (x & y) != 0;
}

template<typename T>
    requires(std::is_unsigned_v<T>)
constexpr void SetBit(T& x, T y) noexcept
{
    x = x | y;
}

template<typename T>
    requires(std::is_unsigned_v<T>)
constexpr void UnsetBit(T& x, T y) noexcept
{
    x = (x | y) ^ y;
}

// Enum operation helpers
template<typename T>
    requires(std::is_enum_v<T>)
constexpr auto IsEnumSet(T value, T check) noexcept -> bool
{
    return (static_cast<size_t>(value) & static_cast<size_t>(check)) != 0;
}

template<typename T, typename... Args>
    requires(std::is_enum_v<T> && (std::is_same_v<T, Args> && ...))
constexpr auto CombineEnum(T first, Args... rest) noexcept -> T
{
    using U = std::underlying_type_t<T>;
    return static_cast<T>((static_cast<U>(first) | ... | static_cast<U>(rest)));
}

// Enum formatter
// Todo: improve named enums
FO_END_NAMESPACE();
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
FO_BEGIN_NAMESPACE();

// Formatters
#define FO_DECLARE_TYPE_FORMATTER(type, ...) \
    FO_END_NAMESPACE(); \
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
    FO_BEGIN_NAMESPACE()

#define FO_DECLARE_TYPE_PARSER(type, ...) \
    FO_END_NAMESPACE(); \
    inline auto operator>>(std::istream& istr, type& value)->std::istream& \
    { \
        if (!(istr >> __VA_ARGS__)) { \
            value = {}; \
        } \
        return istr; \
    } \
    FO_BEGIN_NAMESPACE()

#define FO_DECLARE_TYPE_HASHER(type) \
    FO_END_NAMESPACE(); \
    template<> \
    struct FO_HASH_NAMESPACE hash<type> \
    { \
        using is_avalanching = void; \
        auto operator()(const type& v) const noexcept \
        { \
            static_assert(std::has_unique_object_representations_v<type>); \
            return detail::wyhash::hash(&v, sizeof(v)); \
        } \
    }; \
    FO_BEGIN_NAMESPACE()

#define FO_DECLARE_TYPE_HASHER_EXT(type, ...) \
    FO_END_NAMESPACE(); \
    template<> \
    struct FO_HASH_NAMESPACE hash<type> \
    { \
        using is_avalanching = void; \
        auto operator()(const type& v) const noexcept \
        { \
            return detail::wyhash::hash(__VA_ARGS__); \
        } \
    }; \
    FO_BEGIN_NAMESPACE()

// Math constants
constexpr auto SQRT3_FLOAT = std::numbers::sqrt3_v<float32>;
constexpr auto SQRT3_X2_FLOAT = std::numbers::sqrt3_v<float32> * 2.0f;
constexpr auto RAD_TO_DEG_FLOAT = 180.0f / std::numbers::pi_v<float32>;
constexpr auto DEG_TO_RAD_FLOAT = std::numbers::pi_v<float32> / 180.0f;

FO_END_NAMESPACE();
