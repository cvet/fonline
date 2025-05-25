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

// Todo: make entities positioning free in space, without hard-linking to hex
// Todo: add third 'up' coordinate to positioning that allow create multidimensional maps
// Todo: use smart pointers instead raw
// Todo: fix all PVS Studio warnings
// Todo: SHA replace to openssl SHA
// Todo: wrap fonline code to namespace
// Todo: hash_t 8 byte integer
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
#if FO_WINDOWS + FO_LINUX + FO_MAC + FO_ANDROID + FO_IOS + FO_WEB == 0
#error "Operating system not specified"
#endif
#if FO_WINDOWS + FO_LINUX + FO_MAC + FO_ANDROID + FO_IOS + FO_WEB != 1
#error "Multiple operating systems not allowed"
#endif

#if __cplusplus < 202002L
#error "Invalid __cplusplus version, must be at least C++20 (202002)"
#endif

#define FO_SCRIPT_API extern

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
#include <format>
#include <fstream>
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
#include <shared_mutex>
#include <span>
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

// WinAPI implicitly included in WinRT so add it globally for macro undefining
#if FO_UWP
#include "WinApiUndef-Include.h"
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
using std::initializer_list;
using std::optional;
using std::pair;
using std::span;
using std::string_view;
using std::tuple;
using std::variant;

template<typename T>
using const_span = span<const T>;

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
    }; \
    template<typename T> \
    static constexpr bool name##_v = name<T>::value

TEMPLATE_HAS_MEMBER(has_size, size);
TEMPLATE_HAS_MEMBER(has_inlined, inlined); // small_vector test
TEMPLATE_HAS_MEMBER(has_addref, AddRef);
TEMPLATE_HAS_MEMBER(has_release, Release);

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
static constexpr bool is_vector_v = is_specialization<T, std::vector>::value || has_inlined_v<T> /*small_vector test*/;
template<typename T>
static constexpr bool is_map_v = is_specialization<T, std::map>::value || is_specialization<T, std::unordered_map>::value || is_specialization<T, ankerl::unordered_dense::segmented_map>::value;
template<typename T>
static constexpr bool is_refcounted_v = has_addref_v<T> && has_release_v<T>;

// String view for null terminated string
class string_view_nt : public string_view
{
public:
    // ReSharper disable once CppNonExplicitConvertingConstructor
    constexpr string_view_nt(const char* str) noexcept :
        string_view(str)
    {
    }

    [[nodiscard]] auto c_str() const noexcept -> const char* { return data(); }
};

// Smart pointers
template<typename T>
class raw_ptr
{
    static_assert(std::is_class_v<T> || std::is_arithmetic_v<T>);

    template<typename U>
    friend class raw_ptr;

public:
    FO_FORCE_INLINE constexpr raw_ptr() noexcept :
        _ptr(nullptr)
    {
    }
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr raw_ptr(std::nullptr_t) noexcept :
        _ptr(nullptr)
    {
    }
    FO_FORCE_INLINE auto operator=(std::nullptr_t) noexcept -> raw_ptr&
    {
        _ptr = nullptr;
        return *this;
    }
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr raw_ptr(T* p) noexcept :
        _ptr(p)
    {
    }
    FO_FORCE_INLINE auto operator=(T* p) noexcept -> raw_ptr&
    {
        _ptr = p;
        return *this;
    }

    FO_FORCE_INLINE raw_ptr(raw_ptr&& p) noexcept
    {
        _ptr = p._ptr;
        p._ptr = nullptr;
    }
    template<typename U>
        requires(std::is_convertible_v<U, T>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr raw_ptr(raw_ptr<U>&& p) noexcept
    {
        _ptr = p._ptr;
        p._ptr = nullptr;
    }
    FO_FORCE_INLINE auto operator=(raw_ptr&& p) noexcept -> raw_ptr&
    {
        _ptr = std::move(p._ptr);
        return *this;
    }
    template<typename U>
        requires(std::is_convertible_v<U, T>)
    FO_FORCE_INLINE auto operator=(raw_ptr<U>&& p) noexcept -> raw_ptr&
    {
        _ptr = std::move(p._ptr);
        return *this;
    }

#if 0 // Copy constructible/assignable
    FO_FORCE_INLINE raw_ptr(const raw_ptr& p) noexcept :
        _ptr(p._ptr)
    {
    }
    template<typename U>
        requires(std::is_convertible_v<U, T>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr raw_ptr(const raw_ptr<U>& p) noexcept :
        _ptr(p._ptr)
    {
    }
    FO_FORCE_INLINE auto operator=(const raw_ptr& p) noexcept -> raw_ptr&
    {
        _ptr = p._ptr;
        return *this;
    }
    template<typename U>
        requires(std::is_convertible_v<U, T>)
    FO_FORCE_INLINE auto operator=(const raw_ptr<U>& p) noexcept -> raw_ptr&
    {
        _ptr = p._ptr;
        return *this;
    }
#else
    FO_FORCE_INLINE raw_ptr(const raw_ptr& p) noexcept = delete;
    template<typename U>
        requires(std::is_convertible_v<U, T>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr raw_ptr(const raw_ptr<U>& p) noexcept = delete;
    FO_FORCE_INLINE auto operator=(const raw_ptr& p) noexcept -> raw_ptr& = delete;
    template<typename U>
        requires(std::is_convertible_v<U, T>)
    FO_FORCE_INLINE auto operator=(const raw_ptr<U>& p) noexcept -> raw_ptr& = delete;
#endif

#if 0 // Implicit conversion to pointer
    // ReSharper disable once CppNonExplicitConversionOperator
    FO_FORCE_INLINE operator T*() noexcept { return _ptr; }
    // ReSharper disable once CppNonExplicitConversionOperator
    FO_FORCE_INLINE operator const T*() const noexcept { return _ptr; }
#endif

    FO_FORCE_INLINE ~raw_ptr() noexcept = default;

    [[nodiscard]] FO_FORCE_INLINE explicit operator bool() const noexcept { return !!_ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const raw_ptr& other) const noexcept -> bool { return _ptr == other._ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator!=(const raw_ptr& other) const noexcept -> bool { return _ptr != other._ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const raw_ptr& other) const noexcept -> bool { return _ptr < other._ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const T* other) const noexcept -> bool { return _ptr == other; }
    [[nodiscard]] FO_FORCE_INLINE auto operator!=(const T* other) const noexcept -> bool { return _ptr != other; }
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const T* other) const noexcept -> bool { return _ptr < other; }
    [[nodiscard]] FO_FORCE_INLINE auto operator->() noexcept -> T* { return _ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator->() const noexcept -> const T* { return _ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator*() noexcept -> T& { return *_ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator*() const noexcept -> const T& { return *_ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto get() noexcept -> T* { return _ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto get() const noexcept -> const T* { return _ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto get_no_const() const noexcept -> T* { return _ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator[](size_t index) noexcept -> T& { return _ptr[index]; }
    [[nodiscard]] FO_FORCE_INLINE auto operator[](size_t index) const noexcept -> const T& { return _ptr[index]; }
    FO_FORCE_INLINE void reset(T* p = nullptr) noexcept { _ptr = p; }

private:
    T* _ptr;
};
static_assert(sizeof(raw_ptr<int>) == sizeof(int*));

inline auto ptr_hash(const void* p) noexcept -> size_t
{
    if constexpr (sizeof(p) == sizeof(uint64_t)) {
        return FO_HASH_NAMESPACE detail::wyhash::hash(static_cast<uint64_t>(reinterpret_cast<size_t>(p)));
    }
    else if constexpr (sizeof(p) < sizeof(uint64_t)) {
        return static_cast<size_t>(FO_HASH_NAMESPACE detail::wyhash::hash(static_cast<uint64_t>(reinterpret_cast<size_t>(p))));
    }
    else {
        return FO_HASH_NAMESPACE detail::wyhash::hash(static_cast<const void*>(&p), sizeof(p));
    }
}

FO_END_NAMESPACE();
template<typename T>
struct FO_HASH_NAMESPACE hash<FO_NAMESPACE raw_ptr<T>>
{
    using is_avalanching = void;
    auto operator()(const FO_NAMESPACE raw_ptr<T>& v) const noexcept -> size_t { return ptr_hash(v.get()); }
};
FO_BEGIN_NAMESPACE();

template<typename T>
class refcount_ptr final
{
    template<typename U>
    friend class refcount_ptr;

public:
    FO_FORCE_INLINE refcount_ptr() noexcept :
        _ptr(nullptr)
    {
    }

    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr refcount_ptr(std::nullptr_t) noexcept :
        _ptr(nullptr)
    {
    }
    FO_FORCE_INLINE auto operator=(std::nullptr_t) noexcept -> refcount_ptr&
    {
        safe_release();
        _ptr = nullptr;
        return *this;
    }

    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE refcount_ptr(T* p) noexcept
    {
        _ptr = p;
        safe_addref();
    }

    FO_FORCE_INLINE refcount_ptr(const refcount_ptr& other) noexcept
    {
        _ptr = other._ptr;
        safe_addref();
    }
    FO_FORCE_INLINE refcount_ptr(refcount_ptr&& other) noexcept
    {
        _ptr = other._ptr;
        other._ptr = nullptr;
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr refcount_ptr(const refcount_ptr<U>& other) noexcept
    {
        _ptr = other._ptr;
        safe_addref();
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr refcount_ptr(refcount_ptr<U>&& other) noexcept
    {
        _ptr = other._ptr;
        other._ptr = nullptr;
    }

    FO_FORCE_INLINE auto operator=(T* p) noexcept -> refcount_ptr&
    {
        reset(p);
        return *this;
    }
    FO_FORCE_INLINE auto operator=(const refcount_ptr& other) noexcept -> refcount_ptr&
    {
        if (this != &other) {
            reset(other._ptr);
        }
        return *this;
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    FO_FORCE_INLINE auto operator=(const refcount_ptr<U>& other) noexcept -> refcount_ptr&
    {
        reset(other._ptr);
        return *this;
    }
    FO_FORCE_INLINE auto operator=(refcount_ptr&& other) noexcept -> refcount_ptr&
    {
        if (this != &other) {
            safe_release();
            _ptr = other._ptr;
            other._ptr = nullptr;
        }
        return *this;
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    FO_FORCE_INLINE auto operator=(refcount_ptr<U>&& other) noexcept -> refcount_ptr&
    {
        safe_release();
        _ptr = other._ptr;
        other._ptr = nullptr;
        return *this;
    }

    FO_FORCE_INLINE ~refcount_ptr() { safe_release(); }

    [[nodiscard]] FO_FORCE_INLINE explicit operator bool() const noexcept { return !!_ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const refcount_ptr& other) const noexcept -> bool { return _ptr == other._ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator!=(const refcount_ptr& other) const noexcept -> bool { return _ptr != other._ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const refcount_ptr& other) const noexcept -> bool { return _ptr < other._ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const T* other) const noexcept -> bool { return _ptr == other; }
    [[nodiscard]] FO_FORCE_INLINE auto operator!=(const T* other) const noexcept -> bool { return _ptr != other; }
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const T* other) const noexcept -> bool { return _ptr < other; }
    [[nodiscard]] FO_FORCE_INLINE auto operator->() noexcept -> T* { return _ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator->() const noexcept -> const T* { return _ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator*() noexcept -> T& { return *_ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator*() const noexcept -> const T& { return *_ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto get() noexcept -> T* { return _ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto get() const noexcept -> const T* { return _ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto get_no_const() const noexcept -> T* { return _ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator[](size_t index) noexcept -> T& { return _ptr[index]; }
    [[nodiscard]] FO_FORCE_INLINE auto operator[](size_t index) const noexcept -> const T& { return _ptr[index]; }

    FO_FORCE_INLINE void reset(T* p = nullptr) noexcept
    {
        safe_release();
        _ptr = p;
        safe_addref();
    }

private:
    FO_FORCE_INLINE void safe_addref() noexcept
    {
        if (_ptr != nullptr) {
            _ptr->AddRef();
        }
    }
    FO_FORCE_INLINE void safe_release() noexcept
    {
        if (_ptr != nullptr) {
            _ptr->Release();
        }
    }

    T* _ptr {};
};

FO_END_NAMESPACE();
template<typename T>
struct FO_HASH_NAMESPACE hash<FO_NAMESPACE refcount_ptr<T>>
{
    using is_avalanching = void;
    auto operator()(const FO_NAMESPACE refcount_ptr<T>& v) const noexcept -> size_t { return ptr_hash(v.get()); }
};
FO_BEGIN_NAMESPACE();

template<typename T>
class propagate_const
{
    template<typename U>
    friend class propagate_const;

public:
    using element_type = typename T::element_type;
    static_assert(std::is_class_v<element_type> || std::is_arithmetic_v<element_type>);

    FO_FORCE_INLINE constexpr propagate_const() noexcept :
        _smartPtr(nullptr)
    {
    }
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr propagate_const(std::nullptr_t) noexcept :
        _smartPtr(nullptr)
    {
    }
    FO_FORCE_INLINE auto operator=(std::nullptr_t) noexcept -> propagate_const&
    {
        _smartPtr = nullptr;
        return *this;
    }
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr propagate_const(T&& p) noexcept :
        _smartPtr(std::move(p))
    {
    }
    FO_FORCE_INLINE explicit constexpr propagate_const(const T& p) noexcept :
        _smartPtr(p)
    {
    }
    FO_FORCE_INLINE explicit constexpr propagate_const(element_type* p) noexcept :
        _smartPtr(p)
    {
    }
    FO_FORCE_INLINE explicit constexpr propagate_const(element_type* p, std::function<void(element_type*)>&& deleter) noexcept :
        _smartPtr(p, std::move(deleter))
    {
    }

    FO_FORCE_INLINE propagate_const(propagate_const&& p) noexcept :
        _smartPtr(std::move(p._smartPtr))
    {
    }
    template<typename U>
        requires(std::is_convertible_v<U, T>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr propagate_const(propagate_const<U>&& p) noexcept :
        _smartPtr(std::move(p._smartPtr))
    {
    }
    FO_FORCE_INLINE auto operator=(propagate_const&& p) noexcept -> propagate_const&
    {
        _smartPtr = std::move(p._smartPtr);
        return *this;
    }
    template<typename U>
        requires(std::is_convertible_v<U, T>)
    FO_FORCE_INLINE auto operator=(propagate_const<U>&& p) noexcept -> propagate_const&
    {
        _smartPtr = std::move(p._smartPtr);
        return *this;
    }

#if 1 // Copy constructible/assignable
    FO_FORCE_INLINE propagate_const(const propagate_const& p) noexcept :
        _smartPtr(p._smartPtr)
    {
    }
    template<typename U>
        requires(std::is_convertible_v<U, T>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr propagate_const(const propagate_const<U>& p) noexcept :
        _smartPtr(p._smartPtr)
    {
    }
    FO_FORCE_INLINE auto operator=(const propagate_const& p) noexcept -> propagate_const&
    {
        _smartPtr = p._smartPtr;
        return *this;
    }
    template<typename U>
        requires(std::is_convertible_v<U, T>)
    FO_FORCE_INLINE auto operator=(const propagate_const<U>& p) noexcept -> propagate_const&
    {
        _smartPtr = p._smartPtr;
        return *this;
    }
#else
    FO_FORCE_INLINE propagate_const(const propagate_const& p) noexcept = delete;
    template<typename U>
        requires(std::is_convertible_v<U, T>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr propagate_const(const propagate_const<U>& p) noexcept = delete;
    FO_FORCE_INLINE auto operator=(const propagate_const& p) noexcept -> propagate_const& = delete;
    template<typename U>
        requires(std::is_convertible_v<U, T>)
    FO_FORCE_INLINE auto operator=(const propagate_const<U>& p) noexcept -> propagate_const& = delete;
    [[nodiscard]] FO_FORCE_INLINE auto copy() noexcept -> T { return propagate_const(_smartPtr); }
#endif

#if 0 // Implicit conversion to pointer
    // ReSharper disable once CppNonExplicitConversionOperator
   FO_FORCE_INLINE operator element_type*() noexcept { return _smartPtr.get(); }
    // ReSharper disable once CppNonExplicitConversionOperator
   FO_FORCE_INLINE operator const element_type*() const noexcept { return _smartPtr.get(); }
#endif

    FO_FORCE_INLINE ~propagate_const() noexcept = default;

    [[nodiscard]] FO_FORCE_INLINE explicit operator bool() const noexcept { return !!_smartPtr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const propagate_const& other) const noexcept -> bool { return _smartPtr == other._smartPtr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator!=(const propagate_const& other) const noexcept -> bool { return _smartPtr != other._smartPtr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const propagate_const& other) const noexcept -> bool { return _smartPtr < other._smartPtr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const element_type* other) const noexcept -> bool { return _smartPtr.get() == other; }
    [[nodiscard]] FO_FORCE_INLINE auto operator!=(const element_type* other) const noexcept -> bool { return _smartPtr.get() != other; }
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const element_type* other) const noexcept -> bool { return _smartPtr.get() < other; }
    [[nodiscard]] FO_FORCE_INLINE auto operator->() noexcept -> element_type* { return _smartPtr.get(); }
    [[nodiscard]] FO_FORCE_INLINE auto operator->() const noexcept -> const element_type* { return _smartPtr.get(); }
    [[nodiscard]] FO_FORCE_INLINE auto operator*() noexcept -> element_type& { return *_smartPtr.get(); }
    [[nodiscard]] FO_FORCE_INLINE auto operator*() const noexcept -> const element_type& { return *_smartPtr.get(); }
    [[nodiscard]] FO_FORCE_INLINE auto get() noexcept -> element_type* { return _smartPtr.get(); }
    [[nodiscard]] FO_FORCE_INLINE auto get() const noexcept -> const element_type* { return _smartPtr.get(); }
    [[nodiscard]] FO_FORCE_INLINE auto operator[](size_t index) noexcept -> element_type& { return _smartPtr[index]; }
    [[nodiscard]] FO_FORCE_INLINE auto operator[](size_t index) const noexcept -> const element_type& { return _smartPtr[index]; }
    [[nodiscard]] FO_FORCE_INLINE auto release() noexcept -> element_type* { return _smartPtr.release(); }
    [[nodiscard]] FO_FORCE_INLINE auto lock() noexcept { return propagate_const<std::shared_ptr<element_type>>(_smartPtr.lock()); }
    [[nodiscard]] FO_FORCE_INLINE auto use_count() const noexcept -> size_t { return _smartPtr.use_count(); }
    [[nodiscard]] FO_FORCE_INLINE auto get_underlying() noexcept -> T& { return _smartPtr; }
    [[nodiscard]] FO_FORCE_INLINE auto get_underlying() const noexcept -> const T& { return _smartPtr; }
    FO_FORCE_INLINE void reset(element_type* p = nullptr) noexcept { _smartPtr.reset(p); }

private:
    T _smartPtr;
};

template<typename T>
using shared_ptr = propagate_const<std::shared_ptr<T>>;
template<typename T>
using unique_ptr = propagate_const<std::unique_ptr<T>>;
template<typename T>
using weak_ptr = propagate_const<std::weak_ptr<T>>;

template<typename T>
using unique_del_ptr = propagate_const<std::unique_ptr<T, std::function<void(T*)>>>;

FO_END_NAMESPACE();
template<typename T>
struct FO_HASH_NAMESPACE hash<FO_NAMESPACE propagate_const<T>>
{
    using is_avalanching = void;
    auto operator()(const FO_NAMESPACE propagate_const<T>& v) const noexcept -> size_t { return ptr_hash(v.get()); }
};
FO_BEGIN_NAMESPACE();

// Safe memory allocation
extern void InitBackupMemoryChunks();
extern auto FreeBackupMemoryChunk() noexcept -> bool;
extern void ReportBadAlloc(string_view message, string_view type_str, size_t count, size_t size) noexcept;
[[noreturn]] extern void ReportAndExit(string_view message) noexcept;

template<typename T>
class SafeAllocator
{
public:
    using value_type = T;

    SafeAllocator() noexcept = default;
    template<typename U>
    // ReSharper disable once CppNonExplicitConvertingConstructor
    constexpr SafeAllocator(const SafeAllocator<U>& other) noexcept
    {
        (void)other;
    }
    template<typename U>
    [[nodiscard]] auto operator==(const SafeAllocator<U>& other) const noexcept -> bool
    {
        (void)other;
        return true;
    }
    template<typename U>
    [[nodiscard]] auto operator!=(const SafeAllocator<U>& other) const noexcept -> bool
    {
        (void)other;
        return false;
    }

    // ReSharper disable once CppInconsistentNaming
    [[nodiscard]] auto allocate(size_t count) const noexcept -> T*
    {
        if (count == 0) {
            return nullptr;
        }

        if (count > static_cast<size_t>(-1) / sizeof(T)) {
            ReportBadAlloc("Safe allocator bad size", typeid(T).name(), count, count * sizeof(T));
            ReportAndExit("Alloc size overflow");
        }

        auto* ptr = ::operator new(sizeof(T) * count, std::nothrow);

        if (ptr == nullptr) {
            ReportBadAlloc("Safe allocator failed", typeid(T).name(), count, count * sizeof(T));

            while (ptr == nullptr && FreeBackupMemoryChunk()) {
                ptr = ::operator new(sizeof(T) * count, std::nothrow);
            }

            if (ptr == nullptr) {
                ReportAndExit("Failed to allocate from backup pool");
            }
        }

        return static_cast<T*>(ptr);
    }

    // ReSharper disable once CppInconsistentNaming
    void deallocate(T* ptr, size_t count) const noexcept
    {
        (void)count;
        ::operator delete(ptr);
    }
};

class SafeAlloc
{
public:
    SafeAlloc() = delete;

    template<typename T, typename... Args>
    static auto MakeRaw(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) -> T*
    {
        static_assert(!is_refcounted_v<T>);

        auto* ptr = new (std::nothrow) T(std::forward<Args>(args)...);

        if (ptr == nullptr) {
            ReportBadAlloc("Make raw failed", typeid(T).name(), 1, sizeof(T));

            while (ptr == nullptr && FreeBackupMemoryChunk()) {
                ptr = new (std::nothrow) T(std::forward<Args>(args)...);
            }

            if (ptr == nullptr) {
                ReportAndExit("Failed to allocate raw from backup pool");
            }
        }

        return ptr;
    }

    template<typename T, typename... Args>
    static auto MakeUnique(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) -> unique_ptr<T>
    {
        static_assert(!is_refcounted_v<T>);

        return unique_ptr<T>(MakeRaw<T>(std::forward<Args>(args)...));
    }

    template<typename T, typename... Args>
    static auto MakeRefCounted(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) -> refcount_ptr<T>
    {
        static_assert(is_refcounted_v<T>);

        auto* ptr = new (std::nothrow) T(std::forward<Args>(args)...);

        if (ptr == nullptr) {
            ReportBadAlloc("Make ref counted failed", typeid(T).name(), 1, sizeof(T));

            while (ptr == nullptr && FreeBackupMemoryChunk()) {
                ptr = new (std::nothrow) T(std::forward<Args>(args)...);
            }

            if (ptr == nullptr) {
                ReportAndExit("Failed to allocate ref counted from backup pool");
            }
        }

        return ptr;
    }

    template<typename T, typename... Args>
    static auto MakeShared(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) -> shared_ptr<T>
    {
        static_assert(!is_refcounted_v<T>);

        try {
            return std::make_shared<T>(std::forward<Args>(args)...);
        }
        catch (const std::bad_alloc&) {
            ReportBadAlloc("Make shared ptr failed", typeid(T).name(), 1, sizeof(T));

            while (true) {
                if (!FreeBackupMemoryChunk()) {
                    ReportAndExit("Failed to allocate shared ptr from backup pool");
                }

                try {
                    return std::make_shared<T>(std::forward<Args>(args)...);
                }
                catch (const std::bad_alloc&) {
                }
            }
        }
    }

    template<typename T>
    static auto MakeRawArr(size_t count) noexcept(std::is_nothrow_default_constructible_v<T>) -> T*
    {
        static_assert(!is_refcounted_v<T>);

        if (count == 0) {
            return nullptr;
        }

        if (count > static_cast<size_t>(-1) / sizeof(T)) {
            ReportBadAlloc("Make raw array bad size", typeid(T).name(), count, count * sizeof(T));
            ReportAndExit("Alloc size overflow");
        }

        auto* ptr = new (std::nothrow) T[count]();

        if (ptr == nullptr) {
            ReportBadAlloc("Make raw array failed", typeid(T).name(), count, count * sizeof(T));

            while (ptr == nullptr && FreeBackupMemoryChunk()) {
                ptr = new (std::nothrow) T[count]();
            }

            if (ptr == nullptr) {
                ReportAndExit("Failed to allocate from backup pool");
            }
        }

        return ptr;
    }

    template<typename T>
    static auto MakeUniqueArr(size_t count) noexcept(std::is_nothrow_default_constructible_v<T>) -> unique_ptr<T[]>
    {
        static_assert(!is_refcounted_v<T>);

        return unique_ptr<T[]>(MakeRawArr<T>(count));
    }
};

// Memory low level management
extern auto MemMalloc(size_t size) noexcept -> void*;
extern auto MemCalloc(size_t num, size_t size) noexcept -> void*;
extern auto MemRealloc(void* ptr, size_t size) noexcept -> void*;
extern void MemFree(void* ptr) noexcept;
extern void MemCopy(void* dest, const void* src, size_t size) noexcept;
extern void MemFill(void* ptr, int value, size_t size) noexcept;
extern auto MemCompare(const void* ptr1, const void* ptr2, size_t size) noexcept -> bool;

// Basic types with safe allocator
using string = std::basic_string<char, std::char_traits<char>, SafeAllocator<char>>;
using istringstream = std::basic_istringstream<char, std::char_traits<char>, SafeAllocator<char>>;

template<typename T>
using list = std::list<T, SafeAllocator<T>>;
template<typename T>
using deque = std::deque<T, SafeAllocator<T>>;

template<typename K, typename V>
using map = std::map<K, V, std::less<>, SafeAllocator<pair<const K, V>>>;
template<typename K, typename V>
using multimap = std::multimap<K, V, std::less<>, SafeAllocator<pair<const K, V>>>;
template<typename K>
using set = std::set<K, std::less<>, SafeAllocator<K>>;

template<typename K, typename V, typename H = FO_HASH_NAMESPACE hash<K>>
using unordered_map = ankerl::unordered_dense::segmented_map<K, V, H, std::equal_to<>, SafeAllocator<pair<K, V>>>;
template<typename K, typename V, typename H = FO_HASH_NAMESPACE hash<K>>
using unordered_multimap = std::unordered_multimap<K, V, H, std::equal_to<>, SafeAllocator<pair<const K, V>>>;
template<typename K, typename H = FO_HASH_NAMESPACE hash<K>>
using unordered_set = ankerl::unordered_dense::segmented_set<K, H, std::equal_to<>, SafeAllocator<K>>;

template<typename T, unsigned InlineCapacity>
using small_vector = gch::small_vector<T, InlineCapacity, SafeAllocator<T>>;
template<typename T>
using vector = std::vector<T, SafeAllocator<T>>;

// Smart pointer helpers
template<typename T, typename U>
inline auto dynamic_ptr_cast(unique_ptr<U>&& p) noexcept -> unique_ptr<T>
{
    if (T* casted = dynamic_cast<T*>(p.get())) {
        (void)p.release();
        return unique_ptr<T> {casted};
    }
    return {};
}

template<typename T, typename U>
inline auto dynamic_ptr_cast(shared_ptr<U> p) noexcept -> shared_ptr<T>
{
    return std::dynamic_pointer_cast<T>(p.get_underlying());
}

template<typename T>
inline void make_if_not_exists(unique_ptr<T>& ptr)
{
    if (!ptr) {
        ptr = SafeAlloc::MakeUnique<T>();
    }
}

template<typename T>
inline void destroy_if_empty(unique_ptr<T>& ptr) noexcept
{
    if (ptr && ptr->empty()) {
        ptr.reset();
    }
}

// Vector formatter
FO_END_NAMESPACE();
template<typename T>
struct std::formatter<T, std::enable_if_t<FO_NAMESPACE is_vector_v<T>, char>> : formatter<FO_NAMESPACE string_view>
{
    template<typename FormatContext>
    auto format(const T& value, FormatContext& ctx) const
    {
        FO_NAMESPACE string result;

        for (const auto& e : value) {
            if constexpr (std::is_same_v<T, FO_NAMESPACE vector<FO_NAMESPACE string>>) {
                result += e + " ";
            }
            else if constexpr (std::is_same_v<T, FO_NAMESPACE vector<bool>>) {
                result += e ? "True " : "False ";
            }
            else {
                result += std::to_string(e) + " ";
            }
        }

        if (!result.empty()) {
            result.pop_back();
        }

        return formatter<FO_NAMESPACE string_view>::format(result, ctx);
    }
};
FO_BEGIN_NAMESPACE();

// Enum formatter
// Todo: improve named enums
FO_END_NAMESPACE();
template<typename T>
struct std::formatter<T, std::enable_if_t<std::is_enum_v<T>, char>> : formatter<std::underlying_type_t<T>>
{
    template<typename FormatContext>
    auto format(const T& value, FormatContext& ctx) const
    {
        return formatter<std::underlying_type_t<T>>::format(static_cast<std::underlying_type_t<T>>(value), ctx);
    }
};
FO_BEGIN_NAMESPACE();

// Strong types
template<typename T>
struct strong_type
{
    using underlying_type = typename T::underlying_type;
    static constexpr bool is_strong_type = true;
    static constexpr string_view type_name = T::name;
    static constexpr string_view underlying_type_name = T::underlying_type_name;

    constexpr strong_type() noexcept :
        _value {}
    {
    }
    constexpr explicit strong_type(underlying_type v) noexcept :
        _value {v}
    {
    }
    strong_type(const strong_type&) noexcept = default;
    strong_type(strong_type&&) noexcept = default;
    auto operator=(const strong_type&) noexcept -> strong_type& = default;
    auto operator=(strong_type&&) noexcept -> strong_type& = default;
    ~strong_type() = default;

    [[nodiscard]] constexpr explicit operator bool() const noexcept { return _value != underlying_type {}; }
    [[nodiscard]] constexpr auto operator==(const strong_type& other) const noexcept -> bool { return _value == other._value; }
    [[nodiscard]] constexpr auto operator!=(const strong_type& other) const noexcept -> bool { return _value != other._value; }
    [[nodiscard]] constexpr auto operator<(const strong_type& other) const noexcept -> bool { return _value < other._value; }
    [[nodiscard]] constexpr auto operator<=(const strong_type& other) const noexcept -> bool { return _value <= other._value; }
    [[nodiscard]] constexpr auto operator>(const strong_type& other) const noexcept -> bool { return _value > other._value; }
    [[nodiscard]] constexpr auto operator>=(const strong_type& other) const noexcept -> bool { return _value >= other._value; }
    [[nodiscard]] constexpr auto operator+(const strong_type& other) const noexcept -> strong_type { return strong_type(_value + other._value); }
    [[nodiscard]] constexpr auto operator-(const strong_type& other) const noexcept -> strong_type { return strong_type(_value - other._value); }
    [[nodiscard]] constexpr auto operator*(const strong_type& other) const noexcept -> strong_type { return strong_type(_value * other._value); }
    [[nodiscard]] constexpr auto operator/(const strong_type& other) const noexcept -> strong_type { return strong_type(_value / other._value); }
    [[nodiscard]] constexpr auto operator%(const strong_type& other) const noexcept -> strong_type { return strong_type(_value % other._value); }
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

FO_END_NAMESPACE();
template<typename T>
struct FO_HASH_NAMESPACE hash<FO_NAMESPACE strong_type<T>>
{
    using is_avalanching = void;
    auto operator()(const FO_NAMESPACE strong_type<T>& v) const noexcept -> size_t
    {
        static_assert(std::has_unique_object_representations_v<FO_NAMESPACE strong_type<T>>);
        return detail::wyhash::hash(v.underlying_value());
    }
};
FO_BEGIN_NAMESPACE();

FO_END_NAMESPACE();
template<typename T>
struct std::formatter<T, std::enable_if_t<FO_NAMESPACE is_strong_type_v<T>, char>> : formatter<typename T::underlying_type>
{
    template<typename FormatContext>
    auto format(const T& value, FormatContext& ctx) const
    {
        return formatter<typename T::underlying_type>::format(value.underlying_value(), ctx);
    }
};
FO_BEGIN_NAMESPACE();

///@ ExportValueType ident ident_t HardStrong HasValueAccessor Layout = int64-value
#define FO_IDENT_NAME "ident"
struct ident_t_traits
{
    static constexpr string_view name = FO_IDENT_NAME;
    static constexpr string_view underlying_type_name = "int64";
    using underlying_type = int64;
};
using ident_t = strong_type<ident_t_traits>;
static_assert(sizeof(ident_t) == sizeof(ident_t_traits::underlying_type));
static_assert(std::is_standard_layout_v<ident_t>);

// Game time
using steady_time_point = std::chrono::time_point<std::chrono::steady_clock>;
static_assert(sizeof(steady_time_point::clock::rep) >= 8);
static_assert(std::ratio_less_equal_v<steady_time_point::clock::period, std::micro>);

///@ ExportValueType timespan timespan HardStrong Layout = int64-value
#define FO_TIMESPAN_NAME "timespan"
class timespan
{
public:
    using underlying_type = int64;
    static constexpr bool is_strong_type = true;
    static constexpr string_view type_name = FO_TIMESPAN_NAME;
    static constexpr string_view underlying_type_name = "int64";
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
    [[nodiscard]] constexpr auto compare(const timespan& other) const noexcept -> int { return _value < other._value ? -1 : (_value > other._value ? 1 : 0); }
    [[nodiscard]] constexpr auto operator==(const timespan& other) const noexcept -> bool { return value() == other.value(); }
    [[nodiscard]] constexpr auto operator!=(const timespan& other) const noexcept -> bool { return value() != other.value(); }
    [[nodiscard]] constexpr auto operator<(const timespan& other) const noexcept -> bool { return value() < other.value(); }
    [[nodiscard]] constexpr auto operator<=(const timespan& other) const noexcept -> bool { return value() <= other.value(); }
    [[nodiscard]] constexpr auto operator>(const timespan& other) const noexcept -> bool { return value() > other.value(); }
    [[nodiscard]] constexpr auto operator>=(const timespan& other) const noexcept -> bool { return value() >= other.value(); }
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
        return static_cast<T>(static_cast<double>(_value) / static_cast<double>(other._value));
    }

    static const timespan zero;

private:
    int64 _value {};
};
static_assert(sizeof(timespan) == sizeof(int64));
static_assert(std::is_standard_layout_v<timespan>);

struct time_desc_t
{
    int year {};
    int month {}; // 1..12
    int day {}; // 1..31
    int hour {}; // 0..23
    int minute {}; // 0..59
    int second {}; // 0..59
    int millisecond {}; // 0..999
    int microsecond {}; // 0..999
    int nanosecond {}; // 0..999
};

auto make_time_desc(timespan time_offset, bool local) -> time_desc_t;
auto make_time_offset(int year, int month, int day, int hour, int minute, int second, int millisecond, int microsecond, int nanosecond, bool local) -> timespan;

///@ ExportValueType nanotime nanotime HardStrong Layout = int64-value
#define FO_NANOTIME_NAME "nanotime"
class nanotime
{
public:
    using underlying_type = int64;
    static constexpr bool is_strong_type = true;
    static constexpr string_view type_name = FO_NANOTIME_NAME;
    static constexpr string_view underlying_type_name = "int64";
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
    [[nodiscard]] constexpr auto compare(const nanotime& other) const noexcept -> int { return _value < other._value ? -1 : (_value > other._value ? 1 : 0); }
    [[nodiscard]] constexpr auto operator==(const nanotime& other) const noexcept -> bool { return value() == other.value(); }
    [[nodiscard]] constexpr auto operator!=(const nanotime& other) const noexcept -> bool { return value() != other.value(); }
    [[nodiscard]] constexpr auto operator<(const nanotime& other) const noexcept -> bool { return value() < other.value(); }
    [[nodiscard]] constexpr auto operator<=(const nanotime& other) const noexcept -> bool { return value() <= other.value(); }
    [[nodiscard]] constexpr auto operator>(const nanotime& other) const noexcept -> bool { return value() > other.value(); }
    [[nodiscard]] constexpr auto operator>=(const nanotime& other) const noexcept -> bool { return value() >= other.value(); }
    [[nodiscard]] constexpr auto operator-(const nanotime& other) const noexcept -> timespan { return value() - other.value(); }
    [[nodiscard]] constexpr auto operator+(const timespan& other) const noexcept -> nanotime { return value() + other.value(); }
    [[nodiscard]] constexpr auto operator-(const timespan& other) const noexcept -> nanotime { return value() - other.value(); }
    [[nodiscard]] constexpr auto operator==(const steady_time_point& other) const noexcept -> bool { return value() == other; }
    [[nodiscard]] constexpr auto operator!=(const steady_time_point& other) const noexcept -> bool { return value() != other; }
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
static_assert(sizeof(nanotime) == sizeof(int64));
static_assert(std::is_standard_layout_v<nanotime>);

///@ ExportValueType synctime synctime HardStrong Layout = int64-value
#define FO_SYNCTIME_NAME "synctime"
class synctime
{
public:
    using underlying_type = int64;
    static constexpr bool is_strong_type = true;
    static constexpr string_view type_name = FO_SYNCTIME_NAME;
    static constexpr string_view underlying_type_name = "int64";
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
    [[nodiscard]] constexpr auto compare(const synctime& other) const noexcept -> int { return _value < other._value ? -1 : (_value > other._value ? 1 : 0); }
    [[nodiscard]] constexpr auto operator==(const synctime& other) const noexcept -> bool { return value() == other.value(); }
    [[nodiscard]] constexpr auto operator!=(const synctime& other) const noexcept -> bool { return value() != other.value(); }
    [[nodiscard]] constexpr auto operator<(const synctime& other) const noexcept -> bool { return value() < other.value(); }
    [[nodiscard]] constexpr auto operator<=(const synctime& other) const noexcept -> bool { return value() <= other.value(); }
    [[nodiscard]] constexpr auto operator>(const synctime& other) const noexcept -> bool { return value() > other.value(); }
    [[nodiscard]] constexpr auto operator>=(const synctime& other) const noexcept -> bool { return value() >= other.value(); }
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
static_assert(sizeof(synctime) == sizeof(int64));
static_assert(std::is_standard_layout_v<synctime>);

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

// Custom any as string
// Todo: export any_t with ExportType
class any_t : public string
{
};

FO_END_NAMESPACE();
template<>
struct std::formatter<FO_NAMESPACE any_t> : formatter<FO_NAMESPACE string_view>
{
    template<typename FormatContext>
    auto format(const FO_NAMESPACE any_t& value, FormatContext& ctx) const
    {
        return formatter<FO_NAMESPACE string_view>::format(static_cast<const FO_NAMESPACE string&>(value), ctx);
    }
};
FO_BEGIN_NAMESPACE();

// Math types
// Todo: replace depedency from Assimp types (matrix/vector/quaternion/color)
FO_END_NAMESPACE();
#include "assimp/types.h"
FO_BEGIN_NAMESPACE();
using vec3 = aiVector3t<float>;
using dvec3 = aiVector3t<double>;
using mat44 = aiMatrix4x4t<float>;
using dmat44 = aiMatrix4x4t<double>;
using quaternion = aiQuaterniont<float>;
using dquaternion = aiQuaterniont<double>;
using color4 = aiColor4t<float>;
using dcolor4 = aiColor4t<double>;

// Atomic formatter
template<typename T>
auto constexpr is_atomic_v = is_specialization<T, std::atomic>::value;

FO_END_NAMESPACE();
template<typename T>
struct std::formatter<T, std::enable_if_t<FO_NAMESPACE is_atomic_v<T>, char>> : formatter<decltype(std::declval<T>().load())>
{
    template<typename FormatContext>
    auto format(const T& value, FormatContext& ctx) const
    {
        return formatter<decltype(std::declval<T>().load())>::format(value.load(), ctx);
    }
};
FO_BEGIN_NAMESPACE();

// Profiling & stack trace obtaining
#define FO_CONCAT(x, y) FO_CONCAT_INDIRECT(x, y)
#define FO_CONCAT_INDIRECT(x, y) x##y

// Todo: improve automatic checker of FO_STACK_TRACE_ENTRY/FO_NO_STACK_TRACE_ENTRY in every .cpp function
#if FO_TRACY
#ifndef TRACY_ENABLE
#error TRACY_ENABLE not defined
#endif

FO_END_NAMESPACE();
#include "tracy/Tracy.hpp"
#include "tracy/TracyC.h"
FO_BEGIN_NAMESPACE();

using SourceLocationData = tracy::SourceLocationData;

#if !FO_NO_MANUAL_STACK_TRACE
#define FO_STACK_TRACE_ENTRY() \
    ZoneScoped; \
    auto ___fo_stack_entry = FO_NAMESPACE StackTraceScopeEntry(TracyConcat(__tracy_source_location, __LINE__))
#define FO_STACK_TRACE_ENTRY_NAMED(name) \
    ZoneScopedN(name); \
    auto ___fo_stack_entry = FO_NAMESPACE StackTraceScopeEntry(TracyConcat(__tracy_source_location, __LINE__))
#else
#define FO_STACK_TRACE_ENTRY() ZoneScoped
#define FO_STACK_TRACE_ENTRY_NAMED(name) ZoneScopedN(name)
#endif
#define FO_NO_STACK_TRACE_ENTRY()

#else
struct SourceLocationData // Same as tracy::SourceLocationData
{
    const char* name;
    const char* function;
    const char* file;
    uint32_t line;
};

#if !FO_NO_MANUAL_STACK_TRACE
#define FO_STACK_TRACE_ENTRY() \
    static constexpr FO_NAMESPACE SourceLocationData FO_CONCAT(___fo_source_location, __LINE__) {nullptr, __FUNCTION__, __FILE__, static_cast<uint32_t>(__LINE__)}; \
    auto ___fo_stack_entry = FO_NAMESPACE StackTraceScopeEntry(FO_CONCAT(___fo_source_location, __LINE__))
#define FO_STACK_TRACE_ENTRY_NAMED(name) \
    static constexpr FO_NAMESPACE SourceLocationData FO_CONCAT(___fo_source_location, __LINE__) {nullptr, name, __FILE__, static_cast<uint32_t>(__LINE__)}; \
    auto ___fo_stack_entry = FO_NAMESPACE StackTraceScopeEntry(FO_CONCAT(___fo_source_location, __LINE__))
#else
#define FO_STACK_TRACE_ENTRY()
#define FO_STACK_TRACE_ENTRY_NAMED(name)
#endif
#define FO_NO_STACK_TRACE_ENTRY()
#endif

extern void PushStackTrace(const SourceLocationData& loc) noexcept;
extern void PopStackTrace() noexcept;
extern auto GetStackTrace() -> string;
extern auto GetStackTraceEntry(size_t deep) noexcept -> const SourceLocationData*;

struct StackTraceScopeEntry
{
    FO_FORCE_INLINE explicit StackTraceScopeEntry(const SourceLocationData& loc) noexcept { PushStackTrace(loc); }
    FO_FORCE_INLINE ~StackTraceScopeEntry() noexcept { PopStackTrace(); }

    StackTraceScopeEntry(const StackTraceScopeEntry&) = delete;
    StackTraceScopeEntry(StackTraceScopeEntry&&) noexcept = delete;
    auto operator=(const StackTraceScopeEntry&) -> StackTraceScopeEntry& = delete;
    auto operator=(StackTraceScopeEntry&&) noexcept -> StackTraceScopeEntry& = delete;
};

// Engine exception handling
extern void InstallSystemExceptionHandler();
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

#define FO_DECLARE_EXCEPTION(exception_name) FO_DECLARE_EXCEPTION_EXT(exception_name, FO_NAMESPACE BaseEngineException)

// Todo: pass name to exceptions context args
#define FO_DECLARE_EXCEPTION_EXT(exception_name, base_exception_name) \
    class exception_name : public base_exception_name \
    { \
    public: \
        exception_name() = delete; \
        auto operator=(const exception_name&) = delete; \
        auto operator=(exception_name&&) noexcept = delete; \
        ~exception_name() override = default; \
        template<typename... Args> \
        explicit exception_name(FO_NAMESPACE string_view message, Args&&... args) noexcept : \
            base_exception_name(#exception_name, nullptr, message, std::forward<Args>(args)...) \
        { \
        } \
        template<typename... Args> \
        exception_name(FO_NAMESPACE ExceptionStackTraceData data, FO_NAMESPACE string_view message, Args&&... args) noexcept : \
            base_exception_name(#exception_name, &data, message, std::forward<Args>(args)...) \
        { \
        } \
        exception_name(const exception_name& other) noexcept : \
            base_exception_name(other) \
        { \
        } \
        exception_name(exception_name&& other) noexcept : \
            base_exception_name(std::move(other)) \
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

            const vector<string> params = {string(std::format("{}", std::forward<Args>(args)))...};

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
#define FO_RUNTIME_ASSERT(expr) \
    if (!(expr)) { \
        throw FO_NAMESPACE AssertationException(#expr, __FILE__, __LINE__); \
    }
#define FO_RUNTIME_ASSERT_STR(expr, str) \
    if (!(expr)) { \
        throw FO_NAMESPACE AssertationException(str, __FILE__, __LINE__); \
    }
#define FO_RUNTIME_VERIFY(expr, ...) \
    if (!(expr)) { \
        FO_NAMESPACE ReportVerifyFailed(#expr, __FILE__, __LINE__); \
        return __VA_ARGS__; \
    }
#else
#define FO_RUNTIME_ASSERT(expr)
#define FO_RUNTIME_ASSERT_STR(expr, str)
#define FO_RUNTIME_VERIFY(expr, ...)
#endif

#if FO_DEBUG
#define FO_STRONG_ASSERT(expr) \
    if (!(expr)) { \
        FO_NAMESPACE ReportStrongAssertAndExit(#expr, __FILE__, __LINE__); \
    }
#else
#define FO_STRONG_ASSERT(expr)
#endif

#define FO_UNREACHABLE_PLACE() throw FO_NAMESPACE UnreachablePlaceException(__FILE__, __LINE__)
#define FO_UNKNOWN_EXCEPTION() FO_NAMESPACE ReportStrongAssertAndExit("Unknown exception", __FILE__, __LINE__)

// Common exceptions
FO_DECLARE_EXCEPTION(GenericException);
FO_DECLARE_EXCEPTION(AssertationException);
FO_DECLARE_EXCEPTION(StrongAssertationException);
FO_DECLARE_EXCEPTION(VerifyFailedException);
FO_DECLARE_EXCEPTION(UnreachablePlaceException);
FO_DECLARE_EXCEPTION(NotSupportedException);
FO_DECLARE_EXCEPTION(NotImplementedException);
FO_DECLARE_EXCEPTION(InvalidCallException);
FO_DECLARE_EXCEPTION(NotEnabled3DException);
FO_DECLARE_EXCEPTION(InvalidOperationException);

// Event system
class EventUnsubscriberCallback final
{
    template<typename...>
    friend class EventObserver;
    friend class EventUnsubscriber;

public:
    EventUnsubscriberCallback() = delete;
    EventUnsubscriberCallback(const EventUnsubscriberCallback&) = delete;
    EventUnsubscriberCallback(EventUnsubscriberCallback&&) noexcept = default;
    auto operator=(const EventUnsubscriberCallback&) = delete;
    auto operator=(EventUnsubscriberCallback&&) noexcept -> EventUnsubscriberCallback& = default;
    ~EventUnsubscriberCallback() = default;

private:
    using Callback = std::function<void()>;
    explicit EventUnsubscriberCallback(Callback cb) noexcept :
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
    EventUnsubscriber() noexcept = default;
    EventUnsubscriber(const EventUnsubscriber&) = delete;
    EventUnsubscriber(EventUnsubscriber&&) noexcept = default;
    auto operator=(const EventUnsubscriber&) = delete;
    auto operator=(EventUnsubscriber&&) noexcept -> EventUnsubscriber& = default;
    ~EventUnsubscriber() { Unsubscribe(); }

    auto operator+=(EventUnsubscriberCallback&& cb) noexcept -> EventUnsubscriber&
    {
        _unsubscribeCallbacks.emplace_back(std::move(cb));
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
                FO_UNKNOWN_EXCEPTION();
            }
        }
    }

private:
    using Callback = std::function<void()>;
    explicit EventUnsubscriber(EventUnsubscriberCallback cb) noexcept { _unsubscribeCallbacks.emplace_back(std::move(cb)); }
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
                throw GenericException("Some of subscriber still subscribed", _subscriberCallbacks.size());
            }
            catch (const std::exception& ex) {
                ReportExceptionAndContinue(ex);
            }
            catch (...) {
                FO_UNKNOWN_EXCEPTION();
            }
        }
    }

    [[nodiscard]] auto operator+=(Callback cb) noexcept -> EventUnsubscriberCallback
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
    mutable int RefCounter \
    { \
        1 \
    }
#define FO_SCRIPTABLE_OBJECT_END() \
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
    int _initCount {std::uncaught_exceptions()};
};

// Float safe comparator
template<typename T>
    requires(std::is_floating_point_v<T>)
[[nodiscard]] constexpr auto float_abs(T f) noexcept -> T
{
    return f < 0 ? -f : f;
}

template<typename T>
    requires(std::is_floating_point_v<T>)
[[nodiscard]] constexpr auto is_float_equal(T f1, T f2) noexcept -> bool
{
    if (float_abs(f1 - f2) <= 1.0e-5f) {
        return true;
    }
    return float_abs(f1 - f2) <= 1.0e-5f * std::max(float_abs(f1), float_abs(f2));
}

// Generic helpers
template<typename...>
struct always_false : std::false_type
{
};

template<typename... T>
FO_FORCE_INLINE constexpr void ignore_unused(T const&... /*unused*/)
{
}

#define FO_STRINGIFY(x) FO_STRINGIFY2(x)
#define FO_STRINGIFY2(x) #x
#define FO_LINE_STR FO_STRINGIFY(__LINE__)
#define FO_NON_CONST_METHOD_HINT() _nonConstHelper = !_nonConstHelper
#define FO_NON_CONST_METHOD_HINT_ONELINE() _nonConstHelper = !_nonConstHelper;
#define FO_NON_NULL // Pointer annotation

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

template<typename T>
    requires(std::is_enum_v<T>)
constexpr auto IsEnumSet(T value, T check) noexcept -> bool
{
    return (static_cast<size_t>(value) & static_cast<size_t>(check)) != 0;
}

template<typename T>
    requires(std::is_enum_v<T>)
constexpr auto CombineEnum(T v1, T v2) noexcept -> T
{
    return static_cast<T>(static_cast<size_t>(v1) | static_cast<size_t>(v2));
}

// Formatters
FO_END_NAMESPACE();
template<typename T>
inline auto parse_from_string(const FO_NAMESPACE string& str) -> T;

template<typename T>
inline auto parse_from_string(const FO_NAMESPACE string& str) -> T
{
    static_assert(FO_NAMESPACE always_false<T>::value, "No specialization exists for parse_from_string");
}
FO_BEGIN_NAMESPACE();

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
    template<> \
    inline auto parse_from_string<type>(const FO_NAMESPACE string& str) -> type \
    { \
        type value = {}; \
        FO_NAMESPACE istringstream sstr {str}; \
        __VA_ARGS__; \
        return value; \
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

FO_END_NAMESPACE();
template<>
struct FO_HASH_NAMESPACE hash<FO_NAMESPACE string>
{
    using is_transparent = void;
    using is_avalanching = void;
    auto operator()(FO_NAMESPACE string_view str) const noexcept -> uint64_t { return hash<FO_NAMESPACE string_view> {}(str); }
};
FO_BEGIN_NAMESPACE();

// Data serialization helpers
FO_DECLARE_EXCEPTION(DataReadingException);

class DataReader
{
public:
    explicit DataReader(const_span<uint8> buf) :
        _dataBuf {buf}
    {
    }

    template<typename T>
        requires(std::is_arithmetic_v<T>)
    auto Read() -> T
    {
        if (_readPos + sizeof(T) > _dataBuf.size()) {
            throw DataReadingException("Unexpected end of buffer");
        }

        T data = *reinterpret_cast<const T*>(_dataBuf.data() + _readPos);
        _readPos += sizeof(T);
        return data;
    }

    template<typename T>
    auto ReadPtr(size_t size) -> const T*
    {
        if (_readPos + size > _dataBuf.size()) {
            throw DataReadingException("Unexpected end of buffer");
        }

        if (size != 0) {
            const T* ptr = reinterpret_cast<const T*>(_dataBuf.data() + _readPos);
            _readPos += size;
            return ptr;
        }

        return nullptr;
    }

    template<typename T>
    void ReadPtr(T* ptr)
    {
        if (_readPos + sizeof(T) > _dataBuf.size()) {
            throw DataReadingException("Unexpected end of buffer");
        }

        MemCopy(ptr, _dataBuf.data() + _readPos, sizeof(T));
        _readPos += sizeof(T);
    }

    template<typename T>
    void ReadPtr(T* ptr, size_t size)
    {
        if (_readPos + size > _dataBuf.size()) {
            throw DataReadingException("Unexpected end of buffer");
        }

        MemCopy(ptr, _dataBuf.data() + _readPos, size);
        _readPos += size;
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

    explicit DataWriter(vector<uint8>& buf) noexcept :
        _dataBuf {&buf}
    {
        _dataBuf->reserve(BUF_RESERVE_SIZE);
    }

    template<typename T>
        requires(std::is_arithmetic_v<T>)
    void Write(std::enable_if_t<true, T> data) noexcept
    {
        ResizeBuf(sizeof(T));
        *reinterpret_cast<T*>(_dataBuf->data() + _dataBuf->size() - sizeof(T)) = data;
    }

    template<typename T>
    void WritePtr(const T* data) noexcept
    {
        ResizeBuf(sizeof(T));
        MemCopy(_dataBuf->data() + _dataBuf->size() - sizeof(T), data, sizeof(T));
    }

    template<typename T>
    void WritePtr(const T* data, size_t size) noexcept
    {
        if (size != 0) {
            ResizeBuf(size);
            MemCopy(_dataBuf->data() + _dataBuf->size() - size, data, size);
        }
    }

private:
    void ResizeBuf(size_t size) noexcept
    {
        while (size > _dataBuf->capacity() - _dataBuf->size()) {
            _dataBuf->reserve(_dataBuf->capacity() * 2);
        }

        _dataBuf->resize(_dataBuf->size() + size);
    }

    raw_ptr<vector<uint8>> _dataBuf;
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
        throw InvalidOperationException(FO_LINE_STR);
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
using IRect = TRect<int>; // Todo: move IRect to irect
using FRect = TRect<float>; // Todo: move FRect to frect

// Color type
///@ ExportValueType ucolor ucolor HardStrong HasValueAccessor Layout = uint-value
struct ucolor
{
    using underlying_type = uint;
    static constexpr bool is_strong_type = true;
    static constexpr string_view type_name = "ucolor";
    static constexpr string_view underlying_type_name = "uint";

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
FO_DECLARE_TYPE_HASHER_EXT(FO_NAMESPACE ucolor, v.underlying_value());

FO_END_NAMESPACE();
template<>
struct std::formatter<FO_NAMESPACE ucolor> : formatter<FO_NAMESPACE string_view>
{
    template<typename FormatContext>
    auto format(const FO_NAMESPACE ucolor& value, FormatContext& ctx) const
    {
        FO_NAMESPACE string buf;
        std::format_to(std::back_inserter(buf), "0x{:x}", value.rgba);
        return formatter<FO_NAMESPACE string_view>::format(buf, ctx);
    }
};
FO_BEGIN_NAMESPACE();

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
FO_DECLARE_TYPE_HASHER_EXT(FO_NAMESPACE hstring, v.as_hash());

FO_END_NAMESPACE();
template<>
struct std::formatter<FO_NAMESPACE hstring> : formatter<FO_NAMESPACE string_view>
{
    template<typename FormatContext>
    auto format(const FO_NAMESPACE hstring& value, FormatContext& ctx) const
    {
        return formatter<FO_NAMESPACE string_view>::format(value.as_str(), ctx);
    }
};
FO_BEGIN_NAMESPACE();

// Plain data
template<typename T>
constexpr bool is_valid_pod_type_v = std::is_standard_layout_v<T> && !is_strong_type_v<T> && !std::is_same_v<T, any_t> && //
    !std::is_same_v<T, string> && !std::is_same_v<T, string_view> && !std::is_same_v<T, hstring> && !std::is_arithmetic_v<T> && //
    !std::is_enum_v<T> && !is_specialization<T, vector>::value && !is_specialization<T, map>::value && !is_vector_v<T> && !is_map_v<T>;

// Position types
///@ ExportValueType isize isize HardStrong Layout = int-width+int-height
struct isize
{
    constexpr isize() noexcept = default;
    constexpr isize(int width_, int height_) noexcept :
        width {width_},
        height {height_}
    {
    }

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
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE isize, "{} {}", value.width, value.height);
FO_DECLARE_TYPE_PARSER(FO_NAMESPACE isize, sstr >> value.width, sstr >> value.height);
FO_DECLARE_TYPE_HASHER(FO_NAMESPACE isize);

///@ ExportValueType ipos ipos HardStrong Layout = int-x+int-y
struct ipos
{
    constexpr ipos() noexcept = default;
    constexpr ipos(int x_, int y_) noexcept :
        x {x_},
        y {y_}
    {
    }

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
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE ipos, "{} {}", value.x, value.y);
FO_DECLARE_TYPE_PARSER(FO_NAMESPACE ipos, sstr >> value.x, sstr >> value.y);
FO_DECLARE_TYPE_HASHER(FO_NAMESPACE ipos);

///@ ExportValueType irect irect HardStrong Layout = int-x+int-y+int-width+int-height
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
    [[nodiscard]] constexpr auto operator==(const irect& other) const noexcept -> bool { return x == other.x && y == other.y && width == other.width && height == other.height; }
    [[nodiscard]] constexpr auto operator!=(const irect& other) const noexcept -> bool { return x != other.x || y != other.y || width != other.width || height != other.height; }

    int x {};
    int y {};
    int width {};
    int height {};
};
static_assert(std::is_standard_layout_v<irect>);
static_assert(sizeof(irect) == 16);
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE irect, "{} {} {} {}", value.x, value.y, value.width, value.height);
FO_DECLARE_TYPE_PARSER(FO_NAMESPACE irect, sstr >> value.x, sstr >> value.y, sstr >> value.width, sstr >> value.height);
FO_DECLARE_TYPE_HASHER(FO_NAMESPACE irect);

///@ ExportValueType ipos16 ipos16 HardStrong Layout = int16-x+int16-y
struct ipos16
{
    constexpr ipos16() noexcept = default;
    constexpr ipos16(int16 x_, int16 y_) noexcept :
        x {x_},
        y {y_}
    {
    }

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
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE ipos16, "{} {}", value.x, value.y);
FO_DECLARE_TYPE_PARSER(FO_NAMESPACE ipos16, sstr >> value.x, sstr >> value.y);
FO_DECLARE_TYPE_HASHER(FO_NAMESPACE ipos16);

///@ ExportValueType upos16 upos16 HardStrong Layout = uint16-x+uint16-y
struct upos16
{
    constexpr upos16() noexcept = default;
    constexpr upos16(uint16 x_, uint16 y_) noexcept :
        x {x_},
        y {y_}
    {
    }

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
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE upos16, "{} {}", value.x, value.y);
FO_DECLARE_TYPE_PARSER(FO_NAMESPACE upos16, sstr >> value.x, sstr >> value.y);
FO_DECLARE_TYPE_HASHER(FO_NAMESPACE upos16);

///@ ExportValueType ipos8 ipos8 HardStrong Layout = int8-x+int8-y
struct ipos8
{
    constexpr ipos8() noexcept = default;
    constexpr ipos8(int8 x_, int8 y_) noexcept :
        x {x_},
        y {y_}
    {
    }

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
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE ipos8, "{} {}", value.x, value.y);
FO_DECLARE_TYPE_PARSER(FO_NAMESPACE ipos8, sstr >> value.x, sstr >> value.y);
FO_DECLARE_TYPE_HASHER(FO_NAMESPACE ipos8);

///@ ExportValueType fsize fsize HardStrong Layout = float-width+float-height
struct fsize
{
    constexpr fsize() noexcept = default;
    constexpr fsize(float width_, float height_) noexcept :
        width {width_},
        height {height_}
    {
    }
    constexpr fsize(int width_, int height_) noexcept :
        width {static_cast<float>(width_)},
        height {static_cast<float>(height_)}
    {
    }
    constexpr explicit fsize(isize size) noexcept :
        width {static_cast<float>(size.width)},
        height {static_cast<float>(size.height)}
    {
    }

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
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE fsize, "{} {}", value.width, value.height);
FO_DECLARE_TYPE_PARSER(FO_NAMESPACE fsize, sstr >> value.width, sstr >> value.height);

///@ ExportValueType fpos fpos HardStrong Layout = float-x+float-y
struct fpos
{
    constexpr fpos() noexcept = default;
    constexpr fpos(float x_, float y_) noexcept :
        x {x_},
        y {y_}
    {
    }
    constexpr fpos(int x_, int y_) noexcept :
        x {static_cast<float>(x_)},
        y {static_cast<float>(y_)}
    {
    }
    constexpr explicit fpos(ipos pos) noexcept :
        x {static_cast<float>(pos.x)},
        y {static_cast<float>(pos.y)}
    {
    }

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
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE fpos, "{} {}", value.x, value.y);
FO_DECLARE_TYPE_PARSER(FO_NAMESPACE fpos, sstr >> value.x, sstr >> value.y);

///@ ExportValueType frect frect HardStrong Layout = float-x+float-y+float-width+float-height
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
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE frect, "{} {} {} {}", value.x, value.y, value.width, value.height);
FO_DECLARE_TYPE_PARSER(FO_NAMESPACE frect, sstr >> value.x, sstr >> value.y, sstr >> value.width, sstr >> value.height);

// Generic constants
// Todo: eliminate as much defines as possible
// Todo: convert all defines to constants and enums
static constexpr auto LOCAL_CONFIG_NAME = "LocalSettings.focfg";
static constexpr auto PROCESS_TALK_TIME = 1000;
static constexpr float MIN_ZOOM = 0.1f;
static constexpr float MAX_ZOOM = 20.0f;
static constexpr uint PING_CLIENT_LIFE_TIME = 15000;

// Float constants
constexpr auto PI_FLOAT = 3.14159265f;
constexpr auto SQRT3_X2_FLOAT = 3.4641016151f;
constexpr auto SQRT3_FLOAT = 1.732050807f;
constexpr auto RAD_TO_DEG_FLOAT = 57.29577951f;
constexpr auto DEG_TO_RAD_FLOAT = 0.017453292f;

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
static constexpr auto CMD_LOG = 37;

///@ ExportEnum
enum class EngineInfoMessage : uint16
{
    None = 0,

    BarterNoBarterNow = 486,
    DialogNpcNotLife = 801,
    DialogDistTooLong = 803,
    DialogFromLinkNotFound = 807,
    DialogCompileFail = 808,
    DialogNpcNotFound = 809,
    DialogManyTalkers = 805,

    NetWrongLogin = 1001,
    NetWrongPass = 1002,
    NetPlayerAlready = 1003,
    NetPlayerInGame = 1004,
    NetWrongSpecial = 1005,
    NetRegSuccess = 1006,
    NetConnection = 1007,
    NetConnError = 1008,
    NetLoginPassWrong = 1009,
    NetConnSuccess = 1010,
    NetHexesBusy = 1012,
    NetDisconnByDemand = 1013,
    NetWrongName = 1014,
    NetWrongCases = 1015,
    NetWrongGender = 1016,
    NetWrongAge = 1017,
    NetConnFail = 1018,
    NetWrongData = 1019,
    NetStartLocFail = 1020,
    NetStartMapFail = 1021,
    NetStartCoordFail = 1022,
    NetBdError = 1023,
    NetWrongNetProto = 1024,
    NetDataTransErr = 1025,
    NetNetMsgErr = 1026,
    NetSetProtoErr = 1027,
    NetLoginOk = 1028,
    NetWrongTagSkill = 1029,
    NetDifferentLang = 1030,
    NetManySymbols = 1031,
    NetBeginEndSpaces = 1032,
    NetTwoSpace = 1033,
    NetBanned = 1034,
    NetNameWrongChars = 1035,
    NetPassWrongChars = 1036,
    NetFailToLoadIface = 1037,
    NetFailRunStartScript = 1038,
    NetLanguageNotSupported = 1039,
    NetKnockKnock = 1041,
    NetRegistrationIpWait = 1042,
    NetBannedIp = 1043,
    NetTimeLeft = 1045,
    NetBan = 1046,
    NetBanReason = 1047,
    NetLoginScriptFail = 1048,
    NetPermanentDeath = 1049,

    KickedFromGame = 5000,
    ServerLog = 5001,
};

// Network messages
enum class NetMessage : uint8
{
    Handshake = 1,
    Disconnect = 2,
    HandshakeAnswer = 3,
    InitData = 4,
    Login = 6,
    LoginSuccess = 7,
    Register = 11,
    RegisterSuccess = 13,
    Ping = 15,
    PlaceToGameComplete = 17,
    GetUpdateFile = 19,
    GetUpdateFileData = 21,
    UpdateFileData = 23,
    AddCritter = 25,
    RemoveCritter = 27,
    SendCommand = 28,
    InfoMessage = 32,
    SendCritterDir = 41,
    CritterDir = 42,
    SendCritterMove = 45,
    SendStopCritterMove = 46,
    CritterMove = 47,
    CritterMoveSpeed = 48,
    CritterPos = 49,
    CritterAttachments = 50,
    CritterTeleport = 52,
    ChosenAddItem = 65,
    ChosenRemoveItem = 66,
    AddItemOnMap = 71,
    RemoveItemFromMap = 74,
    AnimateItem = 75,
    SomeItems = 83,
    CritterAction = 91,
    CritterMoveItem = 93,
    CritterAnimate = 95,
    CritterSetAnims = 96,
    Effect = 100,
    FlyEffect = 101,
    PlaySound = 102,
    SendTalkNpc = 103,
    TalkNpc = 105,
    TimeSync = 107,
    LoadMap = 109,
    RemoteCall = 111,
    ViewMap = 115,
    AddCustomEntity = 116,
    RemoveCustomEntity = 117,
    Property = 120,
    SendProperty = 121,
};

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

template<typename T>
    requires(std::is_integral_v<T>)
constexpr auto xrange(T value) noexcept
{
    return irange_loop<T> {0, value};
}

template<typename T>
    requires(has_size_v<T>)
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
        FO_UNKNOWN_EXCEPTION();
    }
}

// Ref holders
template<typename T>
class ref_hold_vector : public vector<T>
{
public:
    static_assert(!std::is_polymorphic_v<vector<T>>);
    ref_hold_vector() noexcept = default;
    ref_hold_vector(const ref_hold_vector&) = delete;
    ref_hold_vector(ref_hold_vector&&) noexcept = default;
    auto operator=(const ref_hold_vector&) -> ref_hold_vector& = delete;
    auto operator=(ref_hold_vector&&) noexcept -> ref_hold_vector& = delete;
    ~ref_hold_vector()
    {
        for (auto&& ref : *this) {
            ref->Release();
        }
    }
};

template<typename T>
constexpr auto copy_hold_ref(const vector<T>& value) -> ref_hold_vector<T>
{
    ref_hold_vector<T> ref_vec;
    ref_vec.reserve(value.size());
    for (auto&& ref : value) {
        FO_RUNTIME_ASSERT(ref);
        ref->AddRef();
        ref_vec.emplace_back(ref);
    }
    return ref_vec;
}

template<typename T, typename U>
constexpr auto copy_hold_ref(const unordered_map<T, U>& value) -> ref_hold_vector<U>
{
    ref_hold_vector<U> ref_vec;
    ref_vec.reserve(value.size());
    for (auto&& [id, ref] : value) {
        FO_RUNTIME_ASSERT(ref);
        ref->AddRef();
        ref_vec.emplace_back(ref);
    }
    return ref_vec;
}

template<typename T>
constexpr auto copy_hold_ref(const unordered_set<T>& value) -> ref_hold_vector<T>
{
    ref_hold_vector<T> ref_vec;
    ref_vec.reserve(value.size());
    for (auto&& ref : value) {
        FO_RUNTIME_ASSERT(ref);
        ref->AddRef();
        ref_vec.emplace_back(ref);
    }
    return ref_vec;
}

template<typename T>
constexpr auto copy_hold_ref(const unordered_set<raw_ptr<T>>& value) -> ref_hold_vector<T*>
{
    static_assert(!std::is_const_v<T>);
    ref_hold_vector<T*> ref_vec;
    ref_vec.reserve(value.size());
    for (auto&& ref : value) {
        FO_RUNTIME_ASSERT(ref);
        ref->AddRef();
        ref_vec.emplace_back(ref.get_no_const());
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
constexpr void vec_add_unique_value(T& vec, typename T::value_type value)
{
    const auto it = std::find(vec.begin(), vec.end(), value);
    FO_RUNTIME_ASSERT(it == vec.end());
    vec.emplace_back(std::move(value));
}

template<typename T>
constexpr void vec_remove_unique_value(T& vec, typename T::value_type value)
{
    const auto it = std::find(vec.begin(), vec.end(), value);
    FO_RUNTIME_ASSERT(it != vec.end());
    vec.erase(it);
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
constexpr auto vec_transform(T& vec, const U& transfromer) -> auto
{
    vector<decltype(transfromer(nullptr))> result;
    result.reserve(vec.size());
    for (auto&& value : vec) {
        result.emplace_back(transfromer(value));
    }
    return result;
}

template<typename T>
constexpr auto vec_exists(const vector<T>& vec, const T& value) noexcept -> bool
{
    return std::find(vec.begin(), vec.end(), value) != vec.end();
}

template<typename T, typename U>
constexpr auto vec_exists(const vector<T>& vec, const U& predicate) noexcept -> bool
{
    return std::find_if(vec.begin(), vec.end(), predicate) != vec.end();
}

// Numeric cast
FO_DECLARE_EXCEPTION(OverflowException);

template<typename T, typename U>
inline constexpr auto numeric_cast(U value) -> T
{
    static_assert(std::is_arithmetic_v<T>);
    static_assert(std::is_arithmetic_v<U>);

    if constexpr (std::is_unsigned_v<T> && std::is_unsigned_v<U> && sizeof(T) >= sizeof(U)) {
        // Always fit
    }
    else if constexpr (std::is_unsigned_v<T> && std::is_unsigned_v<U> && sizeof(T) < sizeof(U)) {
        if (value > static_cast<U>(std::numeric_limits<T>::max())) {
            throw OverflowException("Numeric cast overflow", typeid(U).name(), typeid(T).name(), value, std::numeric_limits<T>::max());
        }
    }
    else if constexpr (std::is_signed_v<T> && std::is_signed_v<U> && sizeof(T) >= sizeof(U)) {
        // Always fit
    }
    else if constexpr (std::is_signed_v<T> && std::is_signed_v<U> && sizeof(T) < sizeof(U)) {
        if (value > static_cast<U>(std::numeric_limits<T>::max())) {
            throw OverflowException("Numeric cast overflow", typeid(U).name(), typeid(T).name(), value, std::numeric_limits<T>::max());
        }
        if (value < static_cast<U>(std::numeric_limits<T>::min())) {
            throw OverflowException("Numeric cast underflow", typeid(U).name(), typeid(T).name(), value, std::numeric_limits<T>::min());
        }
    }
    else if constexpr (std::is_unsigned_v<T> && std::is_signed_v<U> && sizeof(T) >= sizeof(U)) {
        if (value < 0) {
            throw OverflowException("Numeric cast underflow", typeid(U).name(), typeid(T).name(), value, 0);
        }
    }
    else if constexpr (std::is_unsigned_v<T> && std::is_signed_v<U> && sizeof(T) < sizeof(U)) {
        if (value > static_cast<U>(std::numeric_limits<T>::max())) {
            throw OverflowException("Numeric cast overflow", typeid(U).name(), typeid(T).name(), value, std::numeric_limits<T>::max());
        }
        if (value < 0) {
            throw OverflowException("Numeric cast underflow", typeid(U).name(), typeid(T).name(), value, 0);
        }
    }
    else if constexpr (std::is_signed_v<T> && std::is_unsigned_v<U> && sizeof(T) == sizeof(U)) {
        if (value > static_cast<U>(std::numeric_limits<T>::max())) {
            throw OverflowException("Numeric cast overflow", typeid(U).name(), typeid(T).name(), value, std::numeric_limits<T>::max());
        }
    }
    else if constexpr (std::is_signed_v<T> && std::is_unsigned_v<U> && sizeof(T) > sizeof(U)) {
        // Always fit
    }
    else if constexpr (std::is_signed_v<T> && std::is_unsigned_v<U> && sizeof(T) < sizeof(U)) {
        if (value > static_cast<U>(std::numeric_limits<T>::max())) {
            throw OverflowException("Numeric cast overflow", typeid(U).name(), typeid(T).name(), value, std::numeric_limits<T>::max());
        }
    }

    return static_cast<T>(value);
}

template<typename T, typename U>
inline constexpr auto check_numeric_cast(U value) noexcept -> bool
{
    static_assert(std::is_arithmetic_v<T>);
    static_assert(std::is_arithmetic_v<U>);

    if constexpr (std::is_unsigned_v<T> && std::is_unsigned_v<U> && sizeof(T) >= sizeof(U)) {
        // Always fit
    }
    else if constexpr (std::is_unsigned_v<T> && std::is_unsigned_v<U> && sizeof(T) < sizeof(U)) {
        if (value > static_cast<U>(std::numeric_limits<T>::max())) {
            return false;
        }
    }
    else if constexpr (std::is_signed_v<T> && std::is_signed_v<U> && sizeof(T) >= sizeof(U)) {
        // Always fit
    }
    else if constexpr (std::is_signed_v<T> && std::is_signed_v<U> && sizeof(T) < sizeof(U)) {
        if (value > static_cast<U>(std::numeric_limits<T>::max())) {
            return false;
        }
        if (value < static_cast<U>(std::numeric_limits<T>::min())) {
            return false;
        }
    }
    else if constexpr (std::is_unsigned_v<T> && std::is_signed_v<U> && sizeof(T) >= sizeof(U)) {
        if (value < 0) {
            return false;
        }
    }
    else if constexpr (std::is_unsigned_v<T> && std::is_signed_v<U> && sizeof(T) < sizeof(U)) {
        if (value > static_cast<U>(std::numeric_limits<T>::max())) {
            return false;
        }
        if (value < 0) {
            return false;
        }
    }
    else if constexpr (std::is_signed_v<T> && std::is_unsigned_v<U> && sizeof(T) == sizeof(U)) {
        if (value > static_cast<U>(std::numeric_limits<T>::max())) {
            return false;
        }
    }
    else if constexpr (std::is_signed_v<T> && std::is_unsigned_v<U> && sizeof(T) > sizeof(U)) {
        // Always fit
    }
    else if constexpr (std::is_signed_v<T> && std::is_unsigned_v<U> && sizeof(T) < sizeof(U)) {
        if (value > static_cast<U>(std::numeric_limits<T>::max())) {
            return false;
        }
    }

    return true;
}

// Lerp
template<typename T, typename U = std::decay_t<T>>
    requires(!std::is_integral_v<U>)
constexpr U lerp(T v1, T v2, float t) noexcept
{
    return (t <= 0.0f) ? v1 : ((t >= 1.0f) ? v2 : v1 + (v2 - v1) * t);
}

template<typename T, typename U = std::decay_t<T>>
    requires(std::is_integral_v<U> && std::is_signed_v<U>)
constexpr U lerp(T v1, T v2, float t) noexcept
{
    return (t <= 0.0f) ? v1 : ((t >= 1.0f) ? v2 : v1 + static_cast<U>((v2 - v1) * t));
}

template<typename T, typename U = std::decay_t<T>>
    requires(std::is_integral_v<U> && std::is_unsigned_v<U>)
constexpr U lerp(T v1, T v2, float t) noexcept
{
    return (t <= 0.0f) ? v1 : ((t >= 1.0f) ? v2 : static_cast<U>(v1 * (1 - t) + v2 * t));
}

template<typename T>
    requires(std::is_floating_point_v<T>)
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
FO_DECLARE_EXCEPTION(HashResolveException);
FO_DECLARE_EXCEPTION(HashCollisionException);

class HashResolver
{
public:
    virtual ~HashResolver() = default;
    [[nodiscard]] virtual auto ToHashedString(string_view s) -> hstring = 0;
    [[nodiscard]] virtual auto ResolveHash(hstring::hash_t h) const -> hstring = 0;
    [[nodiscard]] virtual auto ResolveHash(hstring::hash_t h, bool* failed) const noexcept -> hstring = 0;
};

FO_DECLARE_EXCEPTION(EnumResolveException);

struct BaseTypeInfo
{
    using StructLayoutEntry = pair<string, BaseTypeInfo>;
    using StructLayoutInfo = std::vector<StructLayoutEntry>;

    string TypeName {};
    bool IsString {};
    bool IsHash {};
    bool IsEnum {};
    bool IsEnumSigned {};
    bool IsPrimitive {}; // IsInt or IsFloat or IsBool
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
    bool IsStruct {};
    const StructLayoutInfo* StructLayout {};
    size_t Size {};
};

class NameResolver
{
public:
    virtual ~NameResolver() = default;
    [[nodiscard]] virtual auto ResolveBaseType(string_view type_str) const -> BaseTypeInfo = 0;
    [[nodiscard]] virtual auto GetEnumInfo(string_view enum_name, const BaseTypeInfo** underlying_type) const -> bool = 0;
    [[nodiscard]] virtual auto GetValueTypeInfo(string_view type_name, size_t& size, const BaseTypeInfo::StructLayoutInfo** layout) const -> bool = 0;
    [[nodiscard]] virtual auto ResolveEnumValue(string_view enum_value_name, bool* failed = nullptr) const -> int = 0;
    [[nodiscard]] virtual auto ResolveEnumValue(string_view enum_name, string_view value_name, bool* failed = nullptr) const -> int = 0;
    [[nodiscard]] virtual auto ResolveEnumValueName(string_view enum_name, int value, bool* failed = nullptr) const -> const string& = 0;
    [[nodiscard]] virtual auto ResolveGenericValue(string_view str, bool* failed = nullptr) const -> int = 0;
    [[nodiscard]] virtual auto CheckMigrationRule(hstring rule_name, hstring extra_info, hstring target) const noexcept -> optional<hstring> = 0;
};

class FrameBalancer
{
public:
    FrameBalancer() = default;
    FrameBalancer(bool enabled, int sleep, int fixed_fps);

    [[nodiscard]] auto GetLoopDuration() const -> timespan { return _loopDuration; }

    void StartLoop();
    void EndLoop();

private:
    bool _enabled {};
    int _sleep {};
    int _fixedFps {};
    nanotime _loopStart {};
    timespan _loopDuration {};
    timespan _idleTimeBalance {};
};

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

FO_DECLARE_EXCEPTION(InfinityLoopException);

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
    using Job = std::function<optional<timespan>()>;
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
    void AddJob(timespan delay, Job job);
    void Clear();
    void Wait() const;

private:
    void AddJobInternal(timespan delay, Job job, bool no_notify);
    void ThreadEntry() noexcept;

    string _name {};
    ExceptionHandler _exceptionHandler {};
    std::thread _thread {};
    vector<pair<nanotime, Job>> _jobs {};
    bool _jobActive {};
    mutable std::mutex _dataLocker {};
    std::condition_variable _workSignal {};
    mutable std::condition_variable _doneSignal {};
    bool _clearJobs {};
    bool _finish {};
};

extern void SetThisThreadName(const string& name);
extern auto GetThisThreadName() -> const string&;

// Interthread communication between server and client
using InterthreadDataCallback = std::function<void(const_span<uint8>)>;
extern map<uint16, std::function<InterthreadDataCallback(InterthreadDataCallback)>> InterthreadListeners;

#define FO_GLOBAL_DATA(class_name, instance_name) \
    static class_name* instance_name; \
    static void FO_CONCAT(Create_, class_name)() \
    { \
        assert(!(instance_name)); \
        (instance_name) = new class_name(); \
    } \
    static void FO_CONCAT(Delete_, class_name)() \
    { \
        delete (instance_name); \
        (instance_name) = nullptr; \
    } \
    struct FO_CONCAT(Register_, class_name) \
    { \
        FO_CONCAT(Register_, class_name)() \
        { \
            assert(FO_NAMESPACE GlobalDataCallbacksCount < FO_NAMESPACE MAX_GLOBAL_DATA_CALLBACKS); \
            FO_NAMESPACE CreateGlobalDataCallbacks[FO_NAMESPACE GlobalDataCallbacksCount] = FO_CONCAT(Create_, class_name); \
            FO_NAMESPACE DeleteGlobalDataCallbacks[FO_NAMESPACE GlobalDataCallbacksCount] = FO_CONCAT(Delete_, class_name); \
            FO_NAMESPACE GlobalDataCallbacksCount++; \
        } \
    }; \
    static FO_CONCAT(Register_, class_name) FO_CONCAT(Register_Instance_, class_name)

constexpr auto MAX_GLOBAL_DATA_CALLBACKS = 40;
using GlobalDataCallback = void (*)();
extern GlobalDataCallback CreateGlobalDataCallbacks[MAX_GLOBAL_DATA_CALLBACKS];
extern GlobalDataCallback DeleteGlobalDataCallbacks[MAX_GLOBAL_DATA_CALLBACKS];
extern int GlobalDataCallbacksCount;

extern void CreateGlobalData();
extern void DeleteGlobalData();

FO_END_NAMESPACE();

#endif // FO_PRECOMPILED_HEADER_GUARD
