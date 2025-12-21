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

FO_BEGIN_NAMESPACE();

// Todo: make sure in debug mode ptr is not dangling
template<typename T>
class raw_ptr
{
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
    FO_FORCE_INLINE constexpr raw_ptr(T* other) noexcept :
        _ptr(other)
    {
    }
    FO_FORCE_INLINE auto operator=(T* other) noexcept -> raw_ptr&
    {
        _ptr = other;
        return *this;
    }

    FO_FORCE_INLINE raw_ptr(raw_ptr&& other) noexcept
    {
        _ptr = std::move(other._ptr);
        other._ptr = nullptr;
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr raw_ptr(raw_ptr<U>&& other) noexcept // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
    {
        _ptr = other._ptr;
        other._ptr = nullptr;
    }
    FO_FORCE_INLINE auto operator=(raw_ptr&& other) noexcept -> raw_ptr&
    {
        _ptr = std::move(other._ptr);
        other._ptr = nullptr;
        return *this;
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    FO_FORCE_INLINE auto operator=(raw_ptr<U>&& other) noexcept -> raw_ptr& // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
    {
        _ptr = std::move(other._ptr);
        other._ptr = nullptr;
        return *this;
    }

    FO_FORCE_INLINE raw_ptr(const raw_ptr& other) noexcept :
        _ptr(other._ptr)
    {
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr raw_ptr(const raw_ptr<U>& other) noexcept :
        _ptr(other._ptr)
    {
    }
    FO_FORCE_INLINE auto operator=(const raw_ptr& other) noexcept -> raw_ptr& // NOLINT(modernize-use-equals-default)
    {
        if (this != &other) {
            _ptr = other._ptr;
        }
        return *this;
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    FO_FORCE_INLINE auto operator=(const raw_ptr<U>& other) noexcept -> raw_ptr&
    {
        _ptr = other._ptr;
        return *this;
    }

#if 0 // Todo: raw_ptr maybe enable implicit conversion to pointer?
    // ReSharper disable once CppNonExplicitConversionOperator
    FO_FORCE_INLINE operator T*() noexcept { return _ptr; }
    // ReSharper disable once CppNonExplicitConversionOperator
    FO_FORCE_INLINE operator const T*() const noexcept { return _ptr; }
#endif

    FO_FORCE_INLINE ~raw_ptr() noexcept = default;

    [[nodiscard]] FO_FORCE_INLINE explicit operator bool() const noexcept { return _ptr != nullptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const raw_ptr& other) const noexcept -> bool { return _ptr == other._ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const raw_ptr& other) const noexcept -> bool { return _ptr < other._ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const T* other) const noexcept -> bool { return _ptr == other; }
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const T* other) const noexcept -> bool { return _ptr < other; }
    [[nodiscard]] FO_FORCE_INLINE auto operator->() noexcept -> T* { return _ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator->() const noexcept -> const T* { return _ptr; }
    template<typename U = T>
        requires(!std::is_void_v<U>)
    [[nodiscard]] FO_FORCE_INLINE auto operator*() noexcept -> U&
    {
        return *_ptr;
    }
    template<typename U = T>
        requires(!std::is_void_v<U>)
    [[nodiscard]] FO_FORCE_INLINE auto operator*() const noexcept -> const U&
    {
        return *_ptr;
    }
    [[nodiscard]] FO_FORCE_INLINE auto get() noexcept -> T* { return _ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto get() const noexcept -> const T* { return _ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto get_no_const() const noexcept -> T* { return _ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto get_pp() noexcept -> T** { return &_ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto get_pp() const noexcept -> T* const* { return &_ptr; }
    template<typename U = T>
        requires(!std::is_void_v<U>)
    [[nodiscard]] FO_FORCE_INLINE auto operator[](size_t index) noexcept -> U&
    {
        return _ptr[index];
    }
    template<typename U = T>
        requires(!std::is_void_v<U>)
    [[nodiscard]] FO_FORCE_INLINE auto operator[](size_t index) const noexcept -> const U&
    {
        return _ptr[index];
    }
    FO_FORCE_INLINE void reset(T* p = nullptr) noexcept { _ptr = p; }

    template<typename U>
        requires(std::is_base_of_v<T, U>)
    FO_FORCE_INLINE auto dyn_cast() noexcept -> raw_ptr<U>
    {
        return raw_ptr<U>(dynamic_cast<U*>(_ptr));
    }

    template<typename U>
        requires(std::is_base_of_v<T, U>)
    FO_FORCE_INLINE auto dyn_cast() const noexcept -> raw_ptr<const U>
    {
        return raw_ptr<const U>(dynamic_cast<const U*>(_ptr));
    }

    template<typename U>
        requires(std::is_base_of_v<U, T>)
    FO_FORCE_INLINE auto cast() noexcept -> T*
    {
        return static_cast<U*>(_ptr);
    }

    template<typename U>
        requires(std::is_base_of_v<U, T>)
    FO_FORCE_INLINE auto cast() const noexcept -> T*
    {
        return static_cast<const U*>(_ptr);
    }

private:
    T* _ptr;
};
static_assert(sizeof(raw_ptr<int32>) == sizeof(int32*));
static_assert(std::is_standard_layout_v<raw_ptr<int32>>);

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
class unique_ptr
{
    static_assert(std::is_class_v<T> || std::is_arithmetic_v<T>);

    template<typename U>
    friend class unique_ptr;

public:
    FO_FORCE_INLINE constexpr unique_ptr() noexcept :
        _ptr(nullptr)
    {
    }
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr unique_ptr(std::nullptr_t) noexcept :
        _ptr(nullptr)
    {
    }
    FO_FORCE_INLINE auto operator=(std::nullptr_t) noexcept -> unique_ptr&
    {
        reset();
        _ptr = nullptr;
        return *this;
    }
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr unique_ptr(T* p) noexcept :
        _ptr(p)
    {
    }
    FO_FORCE_INLINE unique_ptr(unique_ptr&& p) noexcept
    {
        _ptr = std::move(p._ptr);
        p._ptr = nullptr;
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr unique_ptr(unique_ptr<U>&& p) noexcept // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
    {
        _ptr = p._ptr;
        p._ptr = nullptr;
    }
    FO_FORCE_INLINE auto operator=(unique_ptr&& p) noexcept -> unique_ptr&
    {
        reset();
        _ptr = std::move(p._ptr);
        p._ptr = nullptr;
        return *this;
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    FO_FORCE_INLINE auto operator=(unique_ptr<U>&& p) noexcept -> unique_ptr& // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
    {
        reset();
        _ptr = std::move(p._ptr);
        p._ptr = nullptr;
        return *this;
    }

    FO_FORCE_INLINE unique_ptr(const unique_ptr& p) noexcept = delete;
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr unique_ptr(const unique_ptr<U>& p) noexcept = delete;
    FO_FORCE_INLINE auto operator=(const unique_ptr& p) noexcept -> unique_ptr& = delete;
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    FO_FORCE_INLINE auto operator=(const unique_ptr<U>& p) noexcept -> unique_ptr& = delete;

    FO_FORCE_INLINE ~unique_ptr() noexcept { reset(); }

    [[nodiscard]] FO_FORCE_INLINE explicit operator bool() const noexcept { return _ptr != nullptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const unique_ptr& other) const noexcept -> bool { return _ptr == other._ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const unique_ptr& other) const noexcept -> bool { return _ptr < other._ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const T* other) const noexcept -> bool { return _ptr == other; }
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
    [[nodiscard]] FO_FORCE_INLINE auto release() noexcept -> T* { return std::exchange(_ptr, nullptr); }

    FO_FORCE_INLINE void reset(T* p = nullptr) noexcept
    {
        auto* old = std::exchange(_ptr, p);
        delete old;
    }

private:
    T* _ptr;
};
static_assert(sizeof(unique_ptr<int32>) == sizeof(int32*));
static_assert(std::is_standard_layout_v<unique_ptr<int32>>);

FO_END_NAMESPACE();
template<typename T>
struct FO_HASH_NAMESPACE hash<FO_NAMESPACE unique_ptr<T>>
{
    using is_avalanching = void;
    auto operator()(const FO_NAMESPACE unique_ptr<T>& v) const noexcept -> size_t { return ptr_hash(v.get()); }
};
FO_BEGIN_NAMESPACE();

template<typename T>
class refcount_ptr final
{
    template<typename U>
    friend class refcount_ptr;

public:
    struct adopt_tag
    {
    } static constexpr adopt {};

    FO_FORCE_INLINE constexpr refcount_ptr() noexcept :
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
        dec_ref();
        _ptr = nullptr;
        return *this;
    }

    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE refcount_ptr(T* p) noexcept
    {
        _ptr = p;
        add_ref();
    }

    FO_FORCE_INLINE explicit refcount_ptr(adopt_tag /*tag*/, T* p) noexcept { _ptr = p; }

    FO_FORCE_INLINE refcount_ptr(const refcount_ptr& other) noexcept
    {
        _ptr = other._ptr;
        add_ref();
    }
    FO_FORCE_INLINE refcount_ptr(refcount_ptr&& other) noexcept
    {
        _ptr = other._ptr;
        other._ptr = nullptr;
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE refcount_ptr(const refcount_ptr<U>& other) noexcept
    {
        _ptr = other._ptr;
        add_ref();
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE refcount_ptr(refcount_ptr<U>&& other) noexcept // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
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
            dec_ref();
            _ptr = other._ptr;
            other._ptr = nullptr;
        }
        return *this;
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    FO_FORCE_INLINE auto operator=(refcount_ptr<U>&& other) noexcept -> refcount_ptr& // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
    {
        dec_ref();
        _ptr = other._ptr;
        other._ptr = nullptr;
        return *this;
    }

    FO_FORCE_INLINE ~refcount_ptr() { dec_ref(); }

    [[nodiscard]] FO_FORCE_INLINE explicit operator bool() const noexcept { return _ptr != nullptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const refcount_ptr& other) const noexcept -> bool { return _ptr == other._ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const refcount_ptr& other) const noexcept -> bool { return _ptr < other._ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const T* other) const noexcept -> bool { return _ptr == other; }
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
    [[nodiscard]] FO_FORCE_INLINE auto release_ownership() noexcept -> T* { return std::exchange(_ptr, nullptr); }

    FO_FORCE_INLINE void reset(T* p = nullptr) noexcept
    {
        dec_ref();
        _ptr = p;
        add_ref();
    }

    template<typename U>
        requires(std::is_base_of_v<T, U>)
    FO_FORCE_INLINE auto dyn_cast() noexcept -> refcount_ptr<U>
    {
        return refcount_ptr<U>(dynamic_cast<U*>(_ptr));
    }

    template<typename U>
        requires(std::is_base_of_v<T, U>)
    FO_FORCE_INLINE auto dyn_cast() const noexcept -> refcount_ptr<const U>
    {
        return refcount_ptr<const U>(dynamic_cast<const U*>(_ptr));
    }

private:
    FO_FORCE_INLINE void add_ref() noexcept
    {
        if (_ptr != nullptr) {
            _ptr->AddRef();
        }
    }
    FO_FORCE_INLINE void dec_ref() noexcept
    {
        if (_ptr != nullptr) {
            _ptr->Release();
        }
    }

    T* _ptr {};
};
static_assert(std::is_standard_layout_v<refcount_ptr<int32>>);

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
    using element_type = T::element_type;
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
    FO_FORCE_INLINE constexpr explicit propagate_const(const T& p) noexcept :
        _smartPtr(p)
    {
    }
    FO_FORCE_INLINE constexpr explicit propagate_const(element_type* p) noexcept :
        _smartPtr(p)
    {
    }
    FO_FORCE_INLINE constexpr explicit propagate_const(element_type* p, function<void(element_type*)>&& deleter) noexcept :
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
    FO_FORCE_INLINE constexpr propagate_const(propagate_const<U>&& p) noexcept : // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
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
    FO_FORCE_INLINE auto operator=(propagate_const<U>&& p) noexcept -> propagate_const& // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
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
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const propagate_const& other) const noexcept -> bool { return _smartPtr < other._smartPtr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const element_type* other) const noexcept -> bool { return _smartPtr.get() == other; }
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
    // ReSharper disable once CppInconsistentNaming
    [[nodiscard]] FO_FORCE_INLINE auto use_count() const noexcept -> size_t { return _smartPtr.use_count(); }
    [[nodiscard]] FO_FORCE_INLINE auto get_underlying() noexcept -> T& { return _smartPtr; }
    [[nodiscard]] FO_FORCE_INLINE auto get_underlying() const noexcept -> const T& { return _smartPtr; }
    FO_FORCE_INLINE void reset(element_type* p = nullptr) noexcept { _smartPtr.reset(p); }

private:
    T _smartPtr;
};

// Todo: apply nullable_raw_ptr to all nullable pointers
template<typename T>
using nullable_raw_ptr = raw_ptr<T>;

template<typename T>
using shared_ptr = propagate_const<std::shared_ptr<T>>;
template<typename T>
using weak_ptr = propagate_const<std::weak_ptr<T>>;
template<typename T>
using unique_arr_ptr = propagate_const<std::unique_ptr<T[]>>;
template<typename T>
using unique_del_ptr = propagate_const<std::unique_ptr<T, function<void(T*)>>>;

FO_END_NAMESPACE();
template<typename T>
struct FO_HASH_NAMESPACE hash<FO_NAMESPACE propagate_const<T>>
{
    using is_avalanching = void;
    auto operator()(const FO_NAMESPACE propagate_const<T>& v) const noexcept -> size_t { return ptr_hash(v.get()); }
};
FO_BEGIN_NAMESPACE();

// Template helpers
template<typename T>
concept is_refcounted = requires(T t) {
    t.AddRef();
    t.Release();
};

FO_END_NAMESPACE();
