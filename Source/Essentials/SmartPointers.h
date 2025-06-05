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

// ReSharper disable CppInconsistentNaming

#pragma once

#include "BasicCore.h"

FO_BEGIN_NAMESPACE();

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
        _ptr = std::move(p._ptr);
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
        p._ptr = nullptr;
        return *this;
    }
    template<typename U>
        requires(std::is_convertible_v<U, T>)
    FO_FORCE_INLINE auto operator=(raw_ptr<U>&& p) noexcept -> raw_ptr&
    {
        _ptr = std::move(p._ptr);
        p._ptr = nullptr;
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

    [[nodiscard]] FO_FORCE_INLINE explicit operator bool() const noexcept { return _ptr != nullptr; }
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
static_assert(sizeof(raw_ptr<int32>) == sizeof(int32*));
static_assert(std::is_standard_layout_v<raw_ptr<int>>);

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
class uniq_ptr
{
    static_assert(std::is_class_v<T> || std::is_arithmetic_v<T>);

    template<typename U>
    friend class uniq_ptr;

public:
    FO_FORCE_INLINE constexpr uniq_ptr() noexcept :
        _ptr(nullptr)
    {
    }
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr uniq_ptr(std::nullptr_t) noexcept :
        _ptr(nullptr)
    {
    }
    FO_FORCE_INLINE auto operator=(std::nullptr_t) noexcept -> uniq_ptr&
    {
        reset();
        _ptr = nullptr;
        return *this;
    }
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr uniq_ptr(T* p) noexcept :
        _ptr(p)
    {
    }
    FO_FORCE_INLINE uniq_ptr(uniq_ptr&& p) noexcept
    {
        _ptr = std::move(p._ptr);
        p._ptr = nullptr;
    }
    template<typename U>
        requires(std::is_convertible_v<U, T>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr uniq_ptr(uniq_ptr<U>&& p) noexcept
    {
        _ptr = p._ptr;
        p._ptr = nullptr;
    }
    FO_FORCE_INLINE auto operator=(uniq_ptr&& p) noexcept -> uniq_ptr&
    {
        reset();
        _ptr = std::move(p._ptr);
        p._ptr = nullptr;
        return *this;
    }
    template<typename U>
        requires(std::is_convertible_v<U, T>)
    FO_FORCE_INLINE auto operator=(uniq_ptr<U>&& p) noexcept -> uniq_ptr&
    {
        reset();
        _ptr = std::move(p._ptr);
        p._ptr = nullptr;
        return *this;
    }

    FO_FORCE_INLINE uniq_ptr(const uniq_ptr& p) noexcept = delete;
    template<typename U>
        requires(std::is_convertible_v<U, T>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr uniq_ptr(const uniq_ptr<U>& p) noexcept = delete;
    FO_FORCE_INLINE auto operator=(const uniq_ptr& p) noexcept -> uniq_ptr& = delete;
    template<typename U>
        requires(std::is_convertible_v<U, T>)
    FO_FORCE_INLINE auto operator=(const uniq_ptr<U>& p) noexcept -> uniq_ptr& = delete;

    FO_FORCE_INLINE ~uniq_ptr() noexcept { reset(); }

    [[nodiscard]] FO_FORCE_INLINE explicit operator bool() const noexcept { return _ptr != nullptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const uniq_ptr& other) const noexcept -> bool { return _ptr == other._ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator!=(const uniq_ptr& other) const noexcept -> bool { return _ptr != other._ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const uniq_ptr& other) const noexcept -> bool { return _ptr < other._ptr; }
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
        auto* old = std::exchange(_ptr, p);
        delete old;
    }

private:
    T* _ptr;
};
static_assert(sizeof(uniq_ptr<int32>) == sizeof(int32*));
static_assert(std::is_standard_layout_v<uniq_ptr<int>>);

FO_END_NAMESPACE();
template<typename T>
struct FO_HASH_NAMESPACE hash<FO_NAMESPACE uniq_ptr<T>>
{
    using is_avalanching = void;
    auto operator()(const FO_NAMESPACE uniq_ptr<T>& v) const noexcept -> size_t { return ptr_hash(v.get()); }
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
    };

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

    FO_FORCE_INLINE explicit refcount_ptr(adopt_tag /*tag*/, T* p) noexcept { _ptr = p; }

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
    FO_FORCE_INLINE refcount_ptr(const refcount_ptr<U>& other) noexcept
    {
        _ptr = other._ptr;
        safe_addref();
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE refcount_ptr(refcount_ptr<U>&& other) noexcept
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

    [[nodiscard]] FO_FORCE_INLINE explicit operator bool() const noexcept { return _ptr != nullptr; }
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
static_assert(std::is_standard_layout_v<refcount_ptr<int>>);

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
    FO_FORCE_INLINE constexpr explicit propagate_const(const T& p) noexcept :
        _smartPtr(p)
    {
    }
    FO_FORCE_INLINE constexpr explicit propagate_const(element_type* p) noexcept :
        _smartPtr(p)
    {
    }
    FO_FORCE_INLINE constexpr explicit propagate_const(element_type* p, std::function<void(element_type*)>&& deleter) noexcept :
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

static_assert(std::is_standard_layout_v<unique_ptr<int>>);

FO_END_NAMESPACE();
template<typename T>
struct FO_HASH_NAMESPACE hash<FO_NAMESPACE propagate_const<T>>
{
    using is_avalanching = void;
    auto operator()(const FO_NAMESPACE propagate_const<T>& v) const noexcept -> size_t { return ptr_hash(v.get()); }
};
FO_BEGIN_NAMESPACE();

// Template helpers
FO_TYPE_HAS_MEMBER(has_addref, AddRef);
FO_TYPE_HAS_MEMBER(has_release, Release);

template<typename T>
static constexpr bool is_refcounted_v = has_addref_v<T> && has_release_v<T>;

FO_END_NAMESPACE();
