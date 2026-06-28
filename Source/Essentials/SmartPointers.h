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

#include "BasicCore.h"

FO_BEGIN_NAMESPACE

// Template helpers
template<typename T>
concept refcountable = requires(T t) {
    t.AddRef();
    t.Release();
};

template<typename T>
concept try_refcountable = refcountable<T> && requires(T t) {
    { t.TryAddRef() } -> std::convertible_to<bool>;
};

template<typename From, typename To>
concept dynamically_castable_to = requires(From& from) { dynamic_cast<To&>(from); };

template<typename T>
class refcount_ptr;

template<typename T>
class refcount_nptr;

template<typename T>
class ptr;

template<typename T>
class nptr;

// True for the borrow pointer-wrapper vocabulary (ptr/nptr/refcount_ptr/refcount_nptr).
// Used by marshalling layers to treat a wrapped entity pointer like the raw pointer it borrows.
template<typename>
inline constexpr bool is_borrow_pointer_wrapper_v = false;
template<typename T>
inline constexpr bool is_borrow_pointer_wrapper_v<ptr<T>> = true;
template<typename T>
inline constexpr bool is_borrow_pointer_wrapper_v<nptr<T>> = true;
template<typename T>
inline constexpr bool is_borrow_pointer_wrapper_v<refcount_ptr<T>> = true;
template<typename T>
inline constexpr bool is_borrow_pointer_wrapper_v<refcount_nptr<T>> = true;

template<typename T>
class ptr
{
    template<typename U>
    friend class ptr;
    template<typename U>
    friend class nptr;

public:
    using element_type = T;

#if !FO_STRICT_PTR_NONNULL
    FO_FORCE_INLINE constexpr ptr() noexcept :
        _ptr(nullptr)
    {
    }
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr ptr(std::nullptr_t) noexcept :
        _ptr(nullptr)
    {
    }
    FO_FORCE_INLINE auto operator=(std::nullptr_t) noexcept -> ptr&
    {
        _ptr = nullptr;
        return *this;
    }
#else
    FO_FORCE_INLINE constexpr ptr() noexcept = delete;
    FO_FORCE_INLINE constexpr ptr(std::nullptr_t) noexcept = delete;
    FO_FORCE_INLINE auto operator=(std::nullptr_t) noexcept -> ptr& = delete;
#endif
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr ptr(T* other) noexcept :
        _ptr(other)
    {
#if FO_STRICT_PTR_NONNULL
        assert(_ptr != nullptr);
#endif
    }
    FO_FORCE_INLINE auto operator=(T* other) noexcept -> ptr&
    {
#if FO_STRICT_PTR_NONNULL
        assert(other != nullptr);
#endif
        _ptr = other;
        return *this;
    }

    FO_FORCE_INLINE constexpr ptr(ptr&& other) noexcept
    {
        _ptr = std::move(other._ptr);
        other._ptr = nullptr;
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr ptr(ptr<U>&& other) noexcept // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
    {
        _ptr = other._ptr;
        other._ptr = nullptr;
    }
    FO_FORCE_INLINE constexpr auto operator=(ptr&& other) noexcept -> ptr&
    {
        _ptr = std::move(other._ptr);
        other._ptr = nullptr;
        return *this;
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    FO_FORCE_INLINE auto operator=(ptr<U>&& other) noexcept -> ptr& // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
    {
        _ptr = std::move(other._ptr);
        other._ptr = nullptr;
        return *this;
    }

    FO_FORCE_INLINE constexpr ptr(const ptr& other) noexcept :
        _ptr(other._ptr)
    {
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr ptr(const ptr<U>& other) noexcept :
        _ptr(other._ptr)
    {
    }
    FO_FORCE_INLINE constexpr auto operator=(const ptr& other) noexcept -> ptr& // NOLINT(modernize-use-equals-default)
    {
        if (this != &other) {
            _ptr = other._ptr;
        }
        return *this;
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    FO_FORCE_INLINE constexpr auto operator=(const ptr<U>& other) noexcept -> ptr&
    {
        _ptr = other._ptr;
        return *this;
    }

#if 0
    // ReSharper disable once CppNonExplicitConversionOperator
    FO_FORCE_INLINE operator T*() noexcept { return _ptr; }
    // ReSharper disable once CppNonExplicitConversionOperator
    FO_FORCE_INLINE operator const T*() const noexcept { return _ptr; }
#endif

    FO_FORCE_INLINE ~ptr() noexcept = default;

#if !FO_STRICT_PTR_NONNULL
    [[nodiscard]] FO_FORCE_INLINE explicit operator bool() const noexcept { return _ptr != nullptr; }
#else
    [[nodiscard]] FO_FORCE_INLINE explicit operator bool() const noexcept = delete;
#endif
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const ptr& other) const noexcept -> bool { return _ptr == other._ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const ptr& other) const noexcept -> bool { return _ptr < other._ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const T* other) const noexcept -> bool { return _ptr == other; }
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const T* other) const noexcept -> bool { return _ptr < other; }
#if FO_STRICT_PTR_NONNULL
    [[nodiscard]] FO_FORCE_INLINE auto operator==(std::nullptr_t) const noexcept -> bool = delete;
    [[nodiscard]] FO_FORCE_INLINE auto operator!=(std::nullptr_t) const noexcept -> bool = delete;
#endif
    [[nodiscard]] FO_FORCE_INLINE constexpr auto operator->() noexcept -> T* { return _ptr; }
    [[nodiscard]] FO_FORCE_INLINE constexpr auto operator->() const noexcept -> const T* { return _ptr; }
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
    [[nodiscard]] FO_FORCE_INLINE auto as_uintptr() const noexcept -> uintptr_t { return reinterpret_cast<uintptr_t>(_ptr); }
    [[nodiscard]] FO_FORCE_INLINE auto as_intptr() const noexcept -> intptr_t { return reinterpret_cast<intptr_t>(_ptr); }
    [[nodiscard]] FO_FORCE_INLINE auto as_ptr() noexcept -> ptr<T> { return ptr<T>(_ptr); }
    [[nodiscard]] FO_FORCE_INLINE auto as_ptr() const noexcept -> ptr<const T> { return ptr<const T>(_ptr); }
    [[nodiscard]] FO_FORCE_INLINE auto as_nptr() noexcept -> nptr<T> { return nptr<T>(_ptr); }
    [[nodiscard]] FO_FORCE_INLINE auto as_nptr() const noexcept -> nptr<const T> { return nptr<const T>(_ptr); }
#if !FO_STRICT_PTR_NONNULL
    [[nodiscard]] FO_FORCE_INLINE auto get_pp() noexcept -> T** { return &_ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto get_pp() const noexcept -> T* const* { return &_ptr; }
#else
    [[nodiscard]] FO_FORCE_INLINE auto get_pp() noexcept -> T** = delete;
    [[nodiscard]] FO_FORCE_INLINE auto get_pp() const noexcept -> T* const* = delete;
#endif
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
    template<typename U = T>
        requires(!std::is_void_v<U>)
    [[nodiscard]] FO_FORCE_INLINE auto offset(size_t count) const noexcept -> ptr<T>
    {
        return ptr<T> {_ptr + count};
    }
#if !FO_STRICT_PTR_NONNULL
    FO_FORCE_INLINE void reset(T* p = nullptr) noexcept { _ptr = p; }
#else
    FO_FORCE_INLINE void reset(T* p) noexcept
    {
        assert(p != nullptr);
        _ptr = p;
    }
    FO_FORCE_INLINE void reset(std::nullptr_t) noexcept = delete;
#endif

    template<typename U>
        requires(dynamically_castable_to<T, U>)
    FO_FORCE_INLINE auto dyn_cast() noexcept -> nptr<U>;

    template<typename U>
        requires(dynamically_castable_to<T, const U>)
    FO_FORCE_INLINE auto dyn_cast() const noexcept -> nptr<const U>;

    template<typename U>
        requires(requires(T* p) { static_cast<U*>(p); })
    FO_FORCE_INLINE auto cast() noexcept -> nptr<U>
    {
        return nptr<U>(static_cast<U*>(_ptr));
    }

    template<typename U>
        requires(requires(const T* p) { static_cast<const U*>(p); })
    FO_FORCE_INLINE auto cast() const noexcept -> nptr<const U>
    {
        return nptr<const U>(static_cast<const U*>(_ptr));
    }

    // Reinterpret the pointee type through void for low-level byte/ABI views (e.g. `ucolor` <-> `uint8_t`),
    // replacing the `cast_from_void<U*>(cast_to_void(p.get()))` idiom. A `ptr<void>` source is also supported
    // (`void` -> `U`, as `GetPtrAs<U>()` uses). Source-pointee constness is preserved, so this can never
    // silently strip `const`.
    template<typename U>
    FO_FORCE_INLINE auto reinterpret_as() const noexcept -> ptr<std::conditional_t<std::is_const_v<T>, const U, U>>
    {
        using target_type = std::conditional_t<std::is_const_v<T>, const U, U>;
        if constexpr (std::is_void_v<std::remove_const_t<T>>) {
            return ptr<target_type>(cast_from_void<target_type*>(_ptr));
        }
        else {
            return ptr<target_type>(cast_from_void<target_type*>(cast_to_void(_ptr)));
        }
    }

    template<typename U = T>
        requires(refcountable<U>)
    [[nodiscard]] FO_FORCE_INLINE auto hold_ref() const noexcept -> refcount_ptr<U>;

    template<typename U = T>
        requires(refcountable<U>)
    [[nodiscard]] FO_FORCE_INLINE auto try_hold_ref() const noexcept -> refcount_nptr<U>;

private:
    T* _ptr;
};
static_assert(sizeof(ptr<int32_t>) == sizeof(int32_t*));
static_assert(std::is_standard_layout_v<ptr<int32_t>>);
static_assert(FO_STRICT_PTR_NONNULL || std::is_default_constructible_v<ptr<int32_t>>);
static_assert(!FO_STRICT_PTR_NONNULL || !std::is_default_constructible_v<ptr<int32_t>>);
static_assert(FO_STRICT_PTR_NONNULL || std::is_constructible_v<ptr<int32_t>, std::nullptr_t>);
static_assert(!FO_STRICT_PTR_NONNULL || !std::is_constructible_v<ptr<int32_t>, std::nullptr_t>);
static_assert(FO_STRICT_PTR_NONNULL || std::is_assignable_v<ptr<int32_t>&, std::nullptr_t>);
static_assert(!FO_STRICT_PTR_NONNULL || !std::is_assignable_v<ptr<int32_t>&, std::nullptr_t>);

template<typename T>
class nptr
{
    template<typename U>
    friend class ptr;
    template<typename U>
    friend class nptr;

public:
    using element_type = T;

    FO_FORCE_INLINE constexpr nptr() noexcept :
        _ptr(nullptr)
    {
    }
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr nptr(std::nullptr_t) noexcept :
        _ptr(nullptr)
    {
    }
    FO_FORCE_INLINE auto operator=(std::nullptr_t) noexcept -> nptr&
    {
        _ptr = nullptr;
        return *this;
    }
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr nptr(T* other) noexcept :
        _ptr(other)
    {
    }
    FO_FORCE_INLINE auto operator=(T* other) noexcept -> nptr&
    {
        _ptr = other;
        return *this;
    }

    FO_FORCE_INLINE nptr(nptr&& other) noexcept
    {
        _ptr = std::move(other._ptr);
        other._ptr = nullptr;
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr nptr(nptr<U>&& other) noexcept // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
    {
        _ptr = other._ptr;
        other._ptr = nullptr;
    }
    FO_FORCE_INLINE auto operator=(nptr&& other) noexcept -> nptr&
    {
        _ptr = std::move(other._ptr);
        other._ptr = nullptr;
        return *this;
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    FO_FORCE_INLINE auto operator=(nptr<U>&& other) noexcept -> nptr& // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
    {
        _ptr = std::move(other._ptr);
        other._ptr = nullptr;
        return *this;
    }

    FO_FORCE_INLINE nptr(const nptr& other) noexcept :
        _ptr(other._ptr)
    {
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr nptr(const nptr<U>& other) noexcept :
        _ptr(other._ptr)
    {
    }
    FO_FORCE_INLINE auto operator=(const nptr& other) noexcept -> nptr& // NOLINT(modernize-use-equals-default)
    {
        if (this != &other) {
            _ptr = other._ptr;
        }
        return *this;
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    FO_FORCE_INLINE auto operator=(const nptr<U>& other) noexcept -> nptr&
    {
        _ptr = other._ptr;
        return *this;
    }

    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr nptr(const ptr<U>& other) noexcept :
        _ptr(other._ptr)
    {
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr nptr(ptr<U>&& other) noexcept // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
    {
        _ptr = other._ptr;
        other._ptr = nullptr;
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    FO_FORCE_INLINE auto operator=(const ptr<U>& other) noexcept -> nptr&
    {
        _ptr = other._ptr;
        return *this;
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    FO_FORCE_INLINE auto operator=(ptr<U>&& other) noexcept -> nptr& // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
    {
        _ptr = other._ptr;
        other._ptr = nullptr;
        return *this;
    }

    FO_FORCE_INLINE ~nptr() noexcept = default;

    [[nodiscard]] FO_FORCE_INLINE explicit operator bool() const noexcept { return _ptr != nullptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const nptr& other) const noexcept -> bool { return _ptr == other._ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const nptr& other) const noexcept -> bool { return _ptr < other._ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const T* other) const noexcept -> bool { return _ptr == other; }
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const T* other) const noexcept -> bool { return _ptr < other; }
    [[nodiscard]] FO_FORCE_INLINE auto operator->() noexcept -> T* { return _ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator->() const noexcept -> const T* { return _ptr; }
    template<typename U = T>
        requires(!std::is_void_v<U>)
    [[nodiscard]] FO_FORCE_INLINE constexpr auto operator*() noexcept -> U&
    {
        return *_ptr;
    }
    template<typename U = T>
        requires(!std::is_void_v<U>)
    [[nodiscard]] FO_FORCE_INLINE constexpr auto operator*() const noexcept -> const U&
    {
        return *_ptr;
    }
    [[nodiscard]] FO_FORCE_INLINE constexpr auto get() noexcept -> T* { return _ptr; }
    [[nodiscard]] FO_FORCE_INLINE constexpr auto get() const noexcept -> const T* { return _ptr; }
    [[nodiscard]] FO_FORCE_INLINE constexpr auto get_no_const() const noexcept -> T* { return _ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto as_uintptr() const noexcept -> uintptr_t { return reinterpret_cast<uintptr_t>(_ptr); }
    [[nodiscard]] FO_FORCE_INLINE auto as_intptr() const noexcept -> intptr_t { return reinterpret_cast<intptr_t>(_ptr); }
    [[nodiscard]] FO_FORCE_INLINE auto as_ptr() noexcept -> ptr<T>
    {
        assert(_ptr != nullptr);
        return ptr<T>(_ptr);
    }
    [[nodiscard]] FO_FORCE_INLINE auto as_ptr() const noexcept -> ptr<const T>
    {
        assert(_ptr != nullptr);
        return ptr<const T>(_ptr);
    }
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
    template<typename U = T>
        requires(!std::is_void_v<U>)
    [[nodiscard]] FO_FORCE_INLINE auto offset(size_t count) const noexcept -> nptr<T>
    {
        return nptr<T> {_ptr + count};
    }
    FO_FORCE_INLINE void reset(T* p = nullptr) noexcept { _ptr = p; }

    template<typename U>
        requires(dynamically_castable_to<T, U>)
    FO_FORCE_INLINE auto dyn_cast() noexcept -> nptr<U>
    {
        return nptr<U>(dynamic_cast<U*>(_ptr));
    }

    template<typename U>
        requires(dynamically_castable_to<T, const U>)
    FO_FORCE_INLINE auto dyn_cast() const noexcept -> nptr<const U>
    {
        return nptr<const U>(dynamic_cast<const U*>(_ptr));
    }

    template<typename U>
        requires(requires(T* p) { static_cast<U*>(p); })
    FO_FORCE_INLINE auto cast() noexcept -> ptr<U>
    {
        return ptr<U>(static_cast<U*>(_ptr));
    }

    template<typename U>
        requires(requires(const T* p) { static_cast<const U*>(p); })
    FO_FORCE_INLINE auto cast() const noexcept -> ptr<const U>
    {
        return ptr<const U>(static_cast<const U*>(_ptr));
    }

    // Reinterpret the pointee type through void for low-level byte/ABI views, replacing the
    // `cast_from_void<U*>(cast_to_void(p.get()))` idiom. Nullability and source-pointee constness are
    // preserved (null reinterprets to null), so this can never silently strip `const`.
    template<typename U>
    FO_FORCE_INLINE auto reinterpret_as() const noexcept -> nptr<std::conditional_t<std::is_const_v<T>, const U, U>>
    {
        using target_type = std::conditional_t<std::is_const_v<T>, const U, U>;
        if constexpr (std::is_void_v<std::remove_const_t<T>>) {
            return nptr<target_type>(cast_from_void<target_type*>(_ptr));
        }
        else {
            return nptr<target_type>(cast_from_void<target_type*>(cast_to_void(_ptr)));
        }
    }

    template<typename U = T>
        requires(refcountable<U>)
    [[nodiscard]] FO_FORCE_INLINE auto hold_ref() const noexcept -> refcount_ptr<U>;

    template<typename U = T>
        requires(refcountable<U>)
    [[nodiscard]] FO_FORCE_INLINE auto try_hold_ref() const noexcept -> refcount_nptr<U>;

private:
    T* _ptr;
};
static_assert(sizeof(nptr<int32_t>) == sizeof(int32_t*));
static_assert(std::is_standard_layout_v<nptr<int32_t>>);

template<typename T>
template<typename U>
    requires(dynamically_castable_to<T, U>)
FO_FORCE_INLINE auto ptr<T>::dyn_cast() noexcept -> nptr<U>
{
    return nptr<U>(dynamic_cast<U*>(_ptr));
}

template<typename T>
template<typename U>
    requires(dynamically_castable_to<T, const U>)
FO_FORCE_INLINE auto ptr<T>::dyn_cast() const noexcept -> nptr<const U>
{
    return nptr<const U>(dynamic_cast<const U*>(_ptr));
}

inline auto ptr_hash(const void* p) noexcept -> size_t
{
    if constexpr (sizeof(p) == sizeof(uint64_t)) {
        return hashing_ex::hash(static_cast<uint64_t>(reinterpret_cast<size_t>(p)));
    }
    else if constexpr (sizeof(p) < sizeof(uint64_t)) {
        return static_cast<size_t>(hashing_ex::hash(static_cast<uint64_t>(reinterpret_cast<size_t>(p))));
    }
    else {
        return hashing_ex::hash(static_cast<const void*>(&p), sizeof(p));
    }
}

FO_END_NAMESPACE
template<typename T>
struct FO_NAMESPACE hashing::hash<FO_NAMESPACE ptr<T>>
{
    using is_avalanching = void;
    auto operator()(const FO_NAMESPACE ptr<T>& v) const noexcept -> size_t { return ptr_hash(v.get()); }
};
template<typename T>
struct FO_NAMESPACE hashing::hash<FO_NAMESPACE nptr<T>>
{
    using is_avalanching = void;
    auto operator()(const FO_NAMESPACE nptr<T>& v) const noexcept -> size_t { return ptr_hash(v.get()); }
};
FO_BEGIN_NAMESPACE

template<typename T>
class unique_ptr
{
    static_assert(std::is_class_v<T> || std::is_arithmetic_v<T>);

    template<typename U>
    friend class unique_ptr;

public:
    using element_type = T;

#if !FO_STRICT_OWNING_NONNULL
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
#else
    FO_FORCE_INLINE constexpr unique_ptr() noexcept = delete;
    FO_FORCE_INLINE constexpr unique_ptr(std::nullptr_t) noexcept = delete;
    FO_FORCE_INLINE auto operator=(std::nullptr_t) noexcept -> unique_ptr& = delete;
#endif
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr unique_ptr(T* p) noexcept :
        _ptr(p)
    {
#if FO_STRICT_OWNING_NONNULL
        assert(_ptr != nullptr);
#endif
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
        delete_current();
        _ptr = std::move(p._ptr);
        p._ptr = nullptr;
        return *this;
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    FO_FORCE_INLINE auto operator=(unique_ptr<U>&& p) noexcept -> unique_ptr& // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
    {
        delete_current();
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

    FO_FORCE_INLINE ~unique_ptr() noexcept { delete_current(); }

#if !FO_STRICT_OWNING_NONNULL
    [[nodiscard]] FO_FORCE_INLINE explicit operator bool() const noexcept { return _ptr != nullptr; }
#else
    [[nodiscard]] FO_FORCE_INLINE explicit operator bool() const noexcept = delete;
#endif
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const unique_ptr& other) const noexcept -> bool { return _ptr == other._ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const unique_ptr& other) const noexcept -> bool { return _ptr < other._ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const T* other) const noexcept -> bool { return _ptr == other; }
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const T* other) const noexcept -> bool { return _ptr < other; }
#if FO_STRICT_OWNING_NONNULL
    [[nodiscard]] FO_FORCE_INLINE auto operator==(std::nullptr_t) const noexcept -> bool = delete;
    [[nodiscard]] FO_FORCE_INLINE auto operator!=(std::nullptr_t) const noexcept -> bool = delete;
#endif
    [[nodiscard]] FO_FORCE_INLINE auto operator->() noexcept -> T* { return _ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator->() const noexcept -> const T* { return _ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator*() noexcept -> T& { return *_ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator*() const noexcept -> const T& { return *_ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto get() noexcept -> T* { return _ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto get() const noexcept -> const T* { return _ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto get_no_const() const noexcept -> T* { return _ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto as_uintptr() const noexcept -> uintptr_t { return reinterpret_cast<uintptr_t>(_ptr); }
    [[nodiscard]] FO_FORCE_INLINE auto as_intptr() const noexcept -> intptr_t { return reinterpret_cast<intptr_t>(_ptr); }
    [[nodiscard]] FO_FORCE_INLINE auto as_ptr() noexcept -> ptr<T> { return ptr<T>(_ptr); }
    [[nodiscard]] FO_FORCE_INLINE auto as_ptr() const noexcept -> ptr<const T> { return ptr<const T>(_ptr); }
    [[nodiscard]] FO_FORCE_INLINE auto as_nptr() noexcept -> nptr<T> { return nptr<T>(_ptr); }
    [[nodiscard]] FO_FORCE_INLINE auto as_nptr() const noexcept -> nptr<const T> { return nptr<const T>(_ptr); }
    [[nodiscard]] FO_FORCE_INLINE auto operator[](size_t index) noexcept -> T& { return _ptr[index]; }
    [[nodiscard]] FO_FORCE_INLINE auto operator[](size_t index) const noexcept -> const T& { return _ptr[index]; }
#if !FO_STRICT_OWNING_NONNULL
    [[nodiscard]] FO_FORCE_INLINE auto release() noexcept -> T* { return std::exchange(_ptr, nullptr); }
#else
    [[nodiscard]] FO_FORCE_INLINE auto release() & noexcept -> ptr<T> = delete;
    [[nodiscard]] FO_FORCE_INLINE auto release() && noexcept -> ptr<T> { return ptr<T> {std::exchange(_ptr, nullptr)}; }
#endif

#if !FO_STRICT_OWNING_NONNULL
    FO_FORCE_INLINE void reset(T* p = nullptr) noexcept { delete std::exchange(_ptr, p); }
#else
    FO_FORCE_INLINE void reset(T* p) noexcept
    {
        assert(p != nullptr);
        delete std::exchange(_ptr, p);
    }
    FO_FORCE_INLINE void reset(std::nullptr_t) noexcept = delete;
#endif

private:
    FO_FORCE_INLINE void delete_current() noexcept { delete std::exchange(_ptr, nullptr); }

    T* _ptr;
};
static_assert(sizeof(unique_ptr<int32_t>) == sizeof(int32_t*));
static_assert(std::is_standard_layout_v<unique_ptr<int32_t>>);
static_assert(FO_STRICT_OWNING_NONNULL || std::is_default_constructible_v<unique_ptr<int32_t>>);
static_assert(!FO_STRICT_OWNING_NONNULL || !std::is_default_constructible_v<unique_ptr<int32_t>>);
static_assert(FO_STRICT_OWNING_NONNULL || std::is_constructible_v<unique_ptr<int32_t>, std::nullptr_t>);
static_assert(!FO_STRICT_OWNING_NONNULL || !std::is_constructible_v<unique_ptr<int32_t>, std::nullptr_t>);
static_assert(FO_STRICT_OWNING_NONNULL || std::is_assignable_v<unique_ptr<int32_t>&, std::nullptr_t>);
static_assert(!FO_STRICT_OWNING_NONNULL || !std::is_assignable_v<unique_ptr<int32_t>&, std::nullptr_t>);

template<typename T>
class unique_nptr
{
    static_assert(std::is_class_v<T> || std::is_arithmetic_v<T>);

    template<typename U>
    friend class unique_nptr;

public:
    using element_type = T;

    FO_FORCE_INLINE constexpr unique_nptr() noexcept :
        _ptr(nullptr)
    {
    }
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr unique_nptr(std::nullptr_t) noexcept :
        _ptr(nullptr)
    {
    }
    FO_FORCE_INLINE auto operator=(std::nullptr_t) noexcept -> unique_nptr&
    {
        reset();
        _ptr = nullptr;
        return *this;
    }
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr unique_nptr(T* p) noexcept :
        _ptr(p)
    {
    }
    FO_FORCE_INLINE unique_nptr(unique_nptr&& p) noexcept
    {
        _ptr = std::move(p._ptr);
        p._ptr = nullptr;
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr unique_nptr(unique_nptr<U>&& p) noexcept // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
    {
        _ptr = p._ptr;
        p._ptr = nullptr;
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE unique_nptr(unique_ptr<U>&& p) noexcept // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
    {
#if FO_STRICT_OWNING_NONNULL
        _ptr = std::move(p).release().get();
#else
        _ptr = std::move(p).release();
#endif
    }
    FO_FORCE_INLINE auto operator=(unique_nptr&& p) noexcept -> unique_nptr&
    {
        reset();
        _ptr = std::move(p._ptr);
        p._ptr = nullptr;
        return *this;
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    FO_FORCE_INLINE auto operator=(unique_nptr<U>&& p) noexcept -> unique_nptr& // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
    {
        reset();
        _ptr = std::move(p._ptr);
        p._ptr = nullptr;
        return *this;
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    FO_FORCE_INLINE auto operator=(unique_ptr<U>&& p) noexcept -> unique_nptr& // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
    {
        reset();
#if FO_STRICT_OWNING_NONNULL
        _ptr = std::move(p).release().get();
#else
        _ptr = std::move(p).release();
#endif
        return *this;
    }

    FO_FORCE_INLINE unique_nptr(const unique_nptr& p) noexcept = delete;
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr unique_nptr(const unique_nptr<U>& p) noexcept = delete;
    FO_FORCE_INLINE auto operator=(const unique_nptr& p) noexcept -> unique_nptr& = delete;
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    FO_FORCE_INLINE auto operator=(const unique_nptr<U>& p) noexcept -> unique_nptr& = delete;

    FO_FORCE_INLINE ~unique_nptr() noexcept { reset(); }

    [[nodiscard]] FO_FORCE_INLINE explicit operator bool() const noexcept { return _ptr != nullptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const unique_nptr& other) const noexcept -> bool { return _ptr == other._ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const unique_nptr& other) const noexcept -> bool { return _ptr < other._ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const T* other) const noexcept -> bool { return _ptr == other; }
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const T* other) const noexcept -> bool { return _ptr < other; }
    [[nodiscard]] FO_FORCE_INLINE auto operator->() noexcept -> T* { return _ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator->() const noexcept -> const T* { return _ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator*() noexcept -> T& { return *_ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator*() const noexcept -> const T& { return *_ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto get() noexcept -> T* { return _ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto get() const noexcept -> const T* { return _ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto get_no_const() const noexcept -> T* { return _ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto as_uintptr() const noexcept -> uintptr_t { return reinterpret_cast<uintptr_t>(_ptr); }
    [[nodiscard]] FO_FORCE_INLINE auto as_intptr() const noexcept -> intptr_t { return reinterpret_cast<intptr_t>(_ptr); }
    [[nodiscard]] FO_FORCE_INLINE auto as_ptr() noexcept -> ptr<T>
    {
        assert(_ptr != nullptr);
        return ptr<T>(_ptr);
    }
    [[nodiscard]] FO_FORCE_INLINE auto as_ptr() const noexcept -> ptr<const T>
    {
        assert(_ptr != nullptr);
        return ptr<const T>(_ptr);
    }
    [[nodiscard]] FO_FORCE_INLINE auto as_nptr() noexcept -> nptr<T> { return nptr<T>(_ptr); }
    [[nodiscard]] FO_FORCE_INLINE auto as_nptr() const noexcept -> nptr<const T> { return nptr<const T>(_ptr); }
    [[nodiscard]] FO_FORCE_INLINE auto operator[](size_t index) noexcept -> T& { return _ptr[index]; }
    [[nodiscard]] FO_FORCE_INLINE auto operator[](size_t index) const noexcept -> const T& { return _ptr[index]; }
    [[nodiscard]] FO_FORCE_INLINE auto release() noexcept -> nptr<T> { return nptr<T> {std::exchange(_ptr, nullptr)}; }
    [[nodiscard]] FO_FORCE_INLINE auto take_not_null() noexcept -> unique_ptr<T>
    {
        assert(_ptr != nullptr);
        nptr<T> released = release();
        auto released_ptr = released.as_ptr();
        return unique_ptr<T>(released_ptr.get());
    }

    FO_FORCE_INLINE void reset(T* p = nullptr) noexcept { delete std::exchange(_ptr, p); }

private:
    T* _ptr;
};
static_assert(sizeof(unique_nptr<int32_t>) == sizeof(int32_t*));
static_assert(std::is_standard_layout_v<unique_nptr<int32_t>>);

FO_END_NAMESPACE
template<typename T>
struct FO_NAMESPACE hashing::hash<FO_NAMESPACE unique_ptr<T>>
{
    using is_avalanching = void;
    auto operator()(const FO_NAMESPACE unique_ptr<T>& v) const noexcept -> size_t { return ptr_hash(v.get()); }
};
template<typename T>
struct FO_NAMESPACE hashing::hash<FO_NAMESPACE unique_nptr<T>>
{
    using is_avalanching = void;
    auto operator()(const FO_NAMESPACE unique_nptr<T>& v) const noexcept -> size_t { return ptr_hash(v.get()); }
};
FO_BEGIN_NAMESPACE

template<typename T>
class refcount_ptr final
{
    template<typename U>
    friend class ptr;
    template<typename U>
    friend class refcount_ptr;
    template<typename U>
    friend class refcount_nptr;

public:
    using element_type = T;

#if !FO_STRICT_REFCOUNT_EXPLICIT
    struct adopt_tag
    {
    } static constexpr adopt {};
#else
private:
    struct adopt_tag
    {
    } static constexpr adopt {};
#endif

#if FO_STRICT_OWNING_NONNULL
private:
#else
public:
#endif
    FO_FORCE_INLINE constexpr refcount_ptr() noexcept :
        _ptr(nullptr)
    {
    }

#if FO_STRICT_OWNING_NONNULL
public:
    FO_FORCE_INLINE constexpr refcount_ptr(std::nullptr_t) noexcept = delete;
    FO_FORCE_INLINE auto operator=(std::nullptr_t) noexcept -> refcount_ptr& = delete;
#else
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
#endif

#if !FO_STRICT_REFCOUNT_EXPLICIT
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE refcount_ptr(T* p) noexcept
    {
#if FO_STRICT_OWNING_NONNULL
        assert(p != nullptr);
#endif
        _ptr = p;
        add_ref();
    }

    FO_FORCE_INLINE explicit refcount_ptr(adopt_tag /*tag*/, T* p) noexcept
    {
#if FO_STRICT_OWNING_NONNULL
        assert(p != nullptr);
#endif
        _ptr = p;
    }
#endif

    [[nodiscard]] FO_FORCE_INLINE static auto from_add_ref(T* p) noexcept -> refcount_ptr
    {
#if FO_STRICT_OWNING_NONNULL
        assert(p != nullptr);
#endif
        refcount_ptr result;
        result._ptr = p;
        result.add_ref();
        return result;
    }

    [[nodiscard]] FO_FORCE_INLINE static auto try_from_add_ref(T* p) noexcept -> refcount_nptr<T>;

    [[nodiscard]] FO_FORCE_INLINE static auto from_adopted_ref(T* p) noexcept -> refcount_ptr
    {
#if FO_STRICT_OWNING_NONNULL
        assert(p != nullptr);
#endif
        refcount_ptr result;
        result._ptr = p;
        return result;
    }

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

#if !FO_STRICT_REFCOUNT_EXPLICIT
    FO_FORCE_INLINE auto operator=(T* p) noexcept -> refcount_ptr&
    {
        reset(p);
        return *this;
    }
#endif
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

#if !FO_STRICT_OWNING_NONNULL
    [[nodiscard]] FO_FORCE_INLINE explicit operator bool() const noexcept { return _ptr != nullptr; }
#else
    [[nodiscard]] FO_FORCE_INLINE explicit operator bool() const noexcept = delete;
#endif
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const refcount_ptr& other) const noexcept -> bool { return _ptr == other._ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const refcount_ptr& other) const noexcept -> bool { return _ptr < other._ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const T* other) const noexcept -> bool { return _ptr == other; }
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const T* other) const noexcept -> bool { return _ptr < other; }
#if FO_STRICT_OWNING_NONNULL
    [[nodiscard]] FO_FORCE_INLINE auto operator==(std::nullptr_t) const noexcept -> bool = delete;
    [[nodiscard]] FO_FORCE_INLINE auto operator!=(std::nullptr_t) const noexcept -> bool = delete;
#endif
    [[nodiscard]] FO_FORCE_INLINE auto operator->() noexcept -> T* { return _ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator->() const noexcept -> const T* { return _ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator*() noexcept -> T& { return *_ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator*() const noexcept -> const T& { return *_ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto get() noexcept -> T* { return _ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto get() const noexcept -> const T* { return _ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto get_no_const() const noexcept -> T* { return _ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto as_uintptr() const noexcept -> uintptr_t { return reinterpret_cast<uintptr_t>(_ptr); }
    [[nodiscard]] FO_FORCE_INLINE auto as_intptr() const noexcept -> intptr_t { return reinterpret_cast<intptr_t>(_ptr); }
    [[nodiscard]] FO_FORCE_INLINE auto as_ptr() noexcept -> ptr<T> { return ptr<T>(_ptr); }
    [[nodiscard]] FO_FORCE_INLINE auto as_ptr() const noexcept -> ptr<const T> { return ptr<const T>(_ptr); }
    [[nodiscard]] FO_FORCE_INLINE auto as_nptr() noexcept -> nptr<T> { return nptr<T>(_ptr); }
    [[nodiscard]] FO_FORCE_INLINE auto as_nptr() const noexcept -> nptr<const T> { return nptr<const T>(_ptr); }
    [[nodiscard]] FO_FORCE_INLINE auto get_pp() noexcept -> T** { return &_ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto get_pp() const noexcept -> T* const* { return &_ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator[](size_t index) noexcept -> T& { return _ptr[index]; }
    [[nodiscard]] FO_FORCE_INLINE auto operator[](size_t index) const noexcept -> const T& { return _ptr[index]; }
#if !FO_STRICT_OWNING_NONNULL
    [[nodiscard]] FO_FORCE_INLINE auto release_ownership() noexcept -> T* { return std::exchange(_ptr, nullptr); }
#else
    [[nodiscard]] FO_FORCE_INLINE auto release_ownership() & noexcept -> T* = delete;
    [[nodiscard]] FO_FORCE_INLINE auto release_ownership() && noexcept -> T* { return std::exchange(_ptr, nullptr); }
#endif

#if !FO_STRICT_OWNING_NONNULL
    FO_FORCE_INLINE void reset(T* p = nullptr) noexcept
    {
        dec_ref();
        _ptr = p;
        add_ref();
    }
#else
    FO_FORCE_INLINE void reset(T* p) noexcept
    {
        assert(p != nullptr);
        dec_ref();
        _ptr = p;
        add_ref();
    }
    FO_FORCE_INLINE void reset(std::nullptr_t) noexcept = delete;
#endif

    template<typename U>
        requires(dynamically_castable_to<T, U>)
    FO_FORCE_INLINE auto dyn_cast() noexcept -> refcount_nptr<U>;

    template<typename U>
        requires(dynamically_castable_to<T, const U>)
    FO_FORCE_INLINE auto dyn_cast() const noexcept -> refcount_nptr<const U>;

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
static_assert(std::is_standard_layout_v<refcount_ptr<int32_t>>);
static_assert(FO_STRICT_OWNING_NONNULL || std::is_default_constructible_v<refcount_ptr<int32_t>>);
static_assert(!FO_STRICT_OWNING_NONNULL || !std::is_default_constructible_v<refcount_ptr<int32_t>>);
static_assert(FO_STRICT_OWNING_NONNULL || std::is_constructible_v<refcount_ptr<int32_t>, std::nullptr_t>);
static_assert(!FO_STRICT_OWNING_NONNULL || !std::is_constructible_v<refcount_ptr<int32_t>, std::nullptr_t>);
static_assert(FO_STRICT_OWNING_NONNULL || std::is_assignable_v<refcount_ptr<int32_t>&, std::nullptr_t>);
static_assert(!FO_STRICT_OWNING_NONNULL || !std::is_assignable_v<refcount_ptr<int32_t>&, std::nullptr_t>);

template<typename T>
class refcount_nptr final
{
    template<typename U>
    friend class refcount_ptr;
    template<typename U>
    friend class refcount_nptr;

public:
    using element_type = T;

    FO_FORCE_INLINE constexpr refcount_nptr() noexcept :
        _ptr(nullptr)
    {
    }

    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr refcount_nptr(std::nullptr_t) noexcept :
        _ptr(nullptr)
    {
    }
    FO_FORCE_INLINE auto operator=(std::nullptr_t) noexcept -> refcount_nptr&
    {
        dec_ref();
        _ptr = nullptr;
        return *this;
    }

#if !FO_STRICT_REFCOUNT_EXPLICIT
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE refcount_nptr(T* p) noexcept
    {
        _ptr = p;
        add_ref();
    }
#endif

    [[nodiscard]] FO_FORCE_INLINE static auto from_add_ref(T* p) noexcept -> refcount_nptr
    {
        refcount_nptr result;
        result._ptr = p;
        result.add_ref();
        return result;
    }

    [[nodiscard]] FO_FORCE_INLINE static auto try_from_add_ref(T* p) noexcept -> refcount_nptr
    {
        if (p == nullptr) {
            return {};
        }

        if constexpr (try_refcountable<T>) {
            if (!p->TryAddRef()) {
                return {};
            }

            refcount_nptr result;
            result._ptr = p;
            return result;
        }
        else {
            return from_add_ref(p);
        }
    }

    [[nodiscard]] FO_FORCE_INLINE static auto from_adopted_ref(T* p) noexcept -> refcount_nptr
    {
        refcount_nptr result;
        result._ptr = p;
        return result;
    }

    FO_FORCE_INLINE refcount_nptr(const refcount_nptr& other) noexcept
    {
        _ptr = other._ptr;
        add_ref();
    }
    FO_FORCE_INLINE refcount_nptr(refcount_nptr&& other) noexcept
    {
        _ptr = other._ptr;
        other._ptr = nullptr;
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE refcount_nptr(const refcount_nptr<U>& other) noexcept
    {
        _ptr = other._ptr;
        add_ref();
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE refcount_nptr(refcount_nptr<U>&& other) noexcept // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
    {
        _ptr = other._ptr;
        other._ptr = nullptr;
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE refcount_nptr(const refcount_ptr<U>& other) noexcept
    {
        _ptr = other._ptr;
        add_ref();
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE refcount_nptr(refcount_ptr<U>&& other) noexcept // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
    {
        _ptr = other._ptr;
        other._ptr = nullptr;
    }

#if !FO_STRICT_REFCOUNT_EXPLICIT
    FO_FORCE_INLINE auto operator=(T* p) noexcept -> refcount_nptr&
    {
        reset(p);
        return *this;
    }
#endif
    FO_FORCE_INLINE auto operator=(const refcount_nptr& other) noexcept -> refcount_nptr&
    {
        if (this != &other) {
            reset(other._ptr);
        }
        return *this;
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    FO_FORCE_INLINE auto operator=(const refcount_nptr<U>& other) noexcept -> refcount_nptr&
    {
        reset(other._ptr);
        return *this;
    }
    FO_FORCE_INLINE auto operator=(refcount_nptr&& other) noexcept -> refcount_nptr&
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
    FO_FORCE_INLINE auto operator=(refcount_nptr<U>&& other) noexcept -> refcount_nptr& // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
    {
        dec_ref();
        _ptr = other._ptr;
        other._ptr = nullptr;
        return *this;
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    FO_FORCE_INLINE auto operator=(const refcount_ptr<U>& other) noexcept -> refcount_nptr&
    {
        reset(other._ptr);
        return *this;
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    FO_FORCE_INLINE auto operator=(refcount_ptr<U>&& other) noexcept -> refcount_nptr& // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
    {
        dec_ref();
        _ptr = other._ptr;
        other._ptr = nullptr;
        return *this;
    }

    FO_FORCE_INLINE ~refcount_nptr() { dec_ref(); }

    [[nodiscard]] FO_FORCE_INLINE explicit operator bool() const noexcept { return _ptr != nullptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const refcount_nptr& other) const noexcept -> bool { return _ptr == other._ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const refcount_nptr& other) const noexcept -> bool { return _ptr < other._ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const T* other) const noexcept -> bool { return _ptr == other; }
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const T* other) const noexcept -> bool { return _ptr < other; }
    [[nodiscard]] FO_FORCE_INLINE auto operator->() noexcept -> T* { return _ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator->() const noexcept -> const T* { return _ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator*() noexcept -> T& { return *_ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator*() const noexcept -> const T& { return *_ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto get() noexcept -> T* { return _ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto get() const noexcept -> const T* { return _ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto get_no_const() const noexcept -> T* { return _ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto as_uintptr() const noexcept -> uintptr_t { return reinterpret_cast<uintptr_t>(_ptr); }
    [[nodiscard]] FO_FORCE_INLINE auto as_intptr() const noexcept -> intptr_t { return reinterpret_cast<intptr_t>(_ptr); }
    [[nodiscard]] FO_FORCE_INLINE auto as_ptr() noexcept -> ptr<T>
    {
        assert(_ptr != nullptr);
        return ptr<T>(_ptr);
    }
    [[nodiscard]] FO_FORCE_INLINE auto as_ptr() const noexcept -> ptr<const T>
    {
        assert(_ptr != nullptr);
        return ptr<const T>(_ptr);
    }
    [[nodiscard]] FO_FORCE_INLINE auto as_nptr() noexcept -> nptr<T> { return nptr<T>(_ptr); }
    [[nodiscard]] FO_FORCE_INLINE auto as_nptr() const noexcept -> nptr<const T> { return nptr<const T>(_ptr); }
    [[nodiscard]] FO_FORCE_INLINE auto get_pp() noexcept -> T** { return &_ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto get_pp() const noexcept -> T* const* { return &_ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator[](size_t index) noexcept -> T& { return _ptr[index]; }
    [[nodiscard]] FO_FORCE_INLINE auto operator[](size_t index) const noexcept -> const T& { return _ptr[index]; }
    [[nodiscard]] FO_FORCE_INLINE auto release_ownership() noexcept -> T* { return std::exchange(_ptr, nullptr); }
    [[nodiscard]] FO_FORCE_INLINE auto take_not_null() noexcept -> refcount_ptr<T>
    {
        assert(_ptr != nullptr);
        return refcount_ptr<T>::from_adopted_ref(release_ownership());
    }

    FO_FORCE_INLINE void reset(T* p = nullptr) noexcept
    {
        dec_ref();
        _ptr = p;
        add_ref();
    }

    template<typename U>
        requires(dynamically_castable_to<T, U>)
    FO_FORCE_INLINE auto dyn_cast() noexcept -> refcount_nptr<U>
    {
        return nptr<U>(dynamic_cast<U*>(_ptr)).try_hold_ref();
    }

    template<typename U>
        requires(dynamically_castable_to<T, const U>)
    FO_FORCE_INLINE auto dyn_cast() const noexcept -> refcount_nptr<const U>
    {
        return nptr<const U>(dynamic_cast<const U*>(_ptr)).try_hold_ref();
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
static_assert(std::is_standard_layout_v<refcount_nptr<int32_t>>);

template<typename T>
FO_FORCE_INLINE auto refcount_ptr<T>::try_from_add_ref(T* p) noexcept -> refcount_nptr<T>
{
    return refcount_nptr<T>::try_from_add_ref(p);
}

template<typename T>
template<typename U>
    requires(dynamically_castable_to<T, U>)
FO_FORCE_INLINE auto refcount_ptr<T>::dyn_cast() noexcept -> refcount_nptr<U>
{
    return nptr<U>(dynamic_cast<U*>(_ptr)).try_hold_ref();
}

template<typename T>
template<typename U>
    requires(dynamically_castable_to<T, const U>)
FO_FORCE_INLINE auto refcount_ptr<T>::dyn_cast() const noexcept -> refcount_nptr<const U>
{
    return nptr<const U>(dynamic_cast<const U*>(_ptr)).try_hold_ref();
}

template<typename T>
template<typename U>
    requires(refcountable<U>)
FO_FORCE_INLINE auto ptr<T>::hold_ref() const noexcept -> refcount_ptr<U>
{
    static_assert(std::is_convertible_v<T*, U*>);
    return refcount_ptr<U>::from_add_ref(static_cast<U*>(_ptr));
}

template<typename T>
template<typename U>
    requires(refcountable<U>)
FO_FORCE_INLINE auto ptr<T>::try_hold_ref() const noexcept -> refcount_nptr<U>
{
    static_assert(std::is_convertible_v<T*, U*>);

    if (_ptr == nullptr) {
        return {};
    }

    return refcount_ptr<U>::try_from_add_ref(static_cast<U*>(_ptr));
}

template<typename T>
template<typename U>
    requires(refcountable<U>)
FO_FORCE_INLINE auto nptr<T>::hold_ref() const noexcept -> refcount_ptr<U>
{
    static_assert(std::is_convertible_v<T*, U*>);
    return refcount_ptr<U>::from_add_ref(static_cast<U*>(_ptr));
}

template<typename T>
template<typename U>
    requires(refcountable<U>)
FO_FORCE_INLINE auto nptr<T>::try_hold_ref() const noexcept -> refcount_nptr<U>
{
    static_assert(std::is_convertible_v<T*, U*>);

    if (_ptr == nullptr) {
        return {};
    }

    return refcount_ptr<U>::try_from_add_ref(static_cast<U*>(_ptr));
}

FO_END_NAMESPACE
template<typename T>
struct FO_NAMESPACE hashing::hash<FO_NAMESPACE refcount_ptr<T>>
{
    using is_avalanching = void;
    auto operator()(const FO_NAMESPACE refcount_ptr<T>& v) const noexcept -> size_t { return ptr_hash(v.get()); }
};
template<typename T>
struct FO_NAMESPACE hashing::hash<FO_NAMESPACE refcount_nptr<T>>
{
    using is_avalanching = void;
    auto operator()(const FO_NAMESPACE refcount_nptr<T>& v) const noexcept -> size_t { return ptr_hash(v.get()); }
};
FO_BEGIN_NAMESPACE

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
   FO_FORCE_INLINE operator element_type*() noexcept { return _smartPtr.get_no_const(); }
    // ReSharper disable once CppNonExplicitConversionOperator
   FO_FORCE_INLINE operator const element_type*() const noexcept { return _smartPtr.get_no_const(); }
#endif

    FO_FORCE_INLINE ~propagate_const() noexcept = default;

    [[nodiscard]] FO_FORCE_INLINE explicit operator bool() const noexcept { return !!_smartPtr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const propagate_const& other) const noexcept -> bool { return _smartPtr == other._smartPtr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const propagate_const& other) const noexcept -> bool { return _smartPtr < other._smartPtr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const element_type* other) const noexcept -> bool { return _smartPtr == other; }
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const element_type* other) const noexcept -> bool { return _smartPtr < other; }
    [[nodiscard]] FO_FORCE_INLINE auto operator->() noexcept -> element_type* { return get_raw(); }
    [[nodiscard]] FO_FORCE_INLINE auto operator->() const noexcept -> const element_type* { return get_raw(); }
    [[nodiscard]] FO_FORCE_INLINE auto operator*() noexcept -> element_type& { return *get_raw(); }
    [[nodiscard]] FO_FORCE_INLINE auto operator*() const noexcept -> const element_type& { return *get_raw(); }
    [[nodiscard]] FO_FORCE_INLINE auto get() noexcept -> element_type* { return get_raw(); }
    [[nodiscard]] FO_FORCE_INLINE auto get() const noexcept -> const element_type* { return get_raw(); }
    [[nodiscard]] FO_FORCE_INLINE auto as_ptr() noexcept -> ptr<element_type>
    {
        assert(get_raw() != nullptr);
        return ptr<element_type>(get_raw());
    }
    [[nodiscard]] FO_FORCE_INLINE auto as_ptr() const noexcept -> ptr<const element_type>
    {
        assert(get_raw() != nullptr);
        return ptr<const element_type>(get_raw());
    }
    [[nodiscard]] FO_FORCE_INLINE auto as_nptr() noexcept -> nptr<element_type> { return nptr<element_type>(get_raw()); }
    [[nodiscard]] FO_FORCE_INLINE auto as_nptr() const noexcept -> nptr<const element_type> { return nptr<const element_type>(get_raw()); }
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
    [[nodiscard]] FO_FORCE_INLINE auto get_raw() noexcept -> element_type*
    {
        if constexpr (requires { _smartPtr.get_no_const(); }) {
            return _smartPtr.get_no_const();
        }
        else {
            return _smartPtr.get();
        }
    }

    [[nodiscard]] FO_FORCE_INLINE auto get_raw() const noexcept -> const element_type*
    {
        if constexpr (requires { _smartPtr.get_no_const(); }) {
            return _smartPtr.get_no_const();
        }
        else {
            return _smartPtr.get();
        }
    }

    T _smartPtr;
};

template<typename T>
using shared_ptr = propagate_const<std::shared_ptr<T>>;
template<typename T>
using weak_ptr = propagate_const<std::weak_ptr<T>>;
template<typename T>
using unique_arr_ptr = propagate_const<std::unique_ptr<T[]>>;
template<typename T>
using unique_del_nptr = propagate_const<std::unique_ptr<T, function<void(T*)>>>;

template<typename T>
class unique_del_ptr
{
    template<typename U>
    friend class unique_del_ptr;

public:
    using element_type = T;

#if !FO_STRICT_OWNING_NONNULL
    FO_FORCE_INLINE unique_del_ptr() noexcept = default;
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE unique_del_ptr(std::nullptr_t) noexcept :
        _owner(nullptr)
    {
    }
    FO_FORCE_INLINE auto operator=(std::nullptr_t) noexcept -> unique_del_ptr&
    {
        _owner = nullptr;
        return *this;
    }
#else
    FO_FORCE_INLINE unique_del_ptr() noexcept = delete;
    FO_FORCE_INLINE unique_del_ptr(std::nullptr_t) noexcept = delete;
    FO_FORCE_INLINE auto operator=(std::nullptr_t) noexcept -> unique_del_ptr& = delete;
#endif
    FO_FORCE_INLINE unique_del_ptr(T* p, function<void(T*)>&& deleter) noexcept :
        _owner(p, std::move(deleter))
    {
#if FO_STRICT_OWNING_NONNULL
        assert(p != nullptr);
#endif
    }

    FO_FORCE_INLINE unique_del_ptr(unique_del_ptr&& p) noexcept :
        _owner(std::move(p._owner))
    {
    }
    FO_FORCE_INLINE auto operator=(unique_del_ptr&& p) noexcept -> unique_del_ptr&
    {
        _owner = std::move(p._owner);
        return *this;
    }

    FO_FORCE_INLINE unique_del_ptr(const unique_del_ptr& p) noexcept = delete;
    FO_FORCE_INLINE auto operator=(const unique_del_ptr& p) noexcept -> unique_del_ptr& = delete;

    FO_FORCE_INLINE ~unique_del_ptr() noexcept = default;

#if !FO_STRICT_OWNING_NONNULL
    [[nodiscard]] FO_FORCE_INLINE explicit operator bool() const noexcept { return !!_owner; }
#else
    [[nodiscard]] FO_FORCE_INLINE explicit operator bool() const noexcept = delete;
#endif
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const unique_del_ptr& other) const noexcept -> bool { return _owner == other._owner; }
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const unique_del_ptr& other) const noexcept -> bool { return _owner < other._owner; }
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const T* other) const noexcept -> bool { return _owner == other; }
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const T* other) const noexcept -> bool { return _owner < other; }
#if FO_STRICT_OWNING_NONNULL
    [[nodiscard]] FO_FORCE_INLINE auto operator==(std::nullptr_t) const noexcept -> bool = delete;
    [[nodiscard]] FO_FORCE_INLINE auto operator!=(std::nullptr_t) const noexcept -> bool = delete;
#endif
    [[nodiscard]] FO_FORCE_INLINE auto operator->() noexcept -> T* { return _owner.get(); }
    [[nodiscard]] FO_FORCE_INLINE auto operator->() const noexcept -> const T* { return _owner.get(); }
    template<typename U = T>
        requires(!std::is_void_v<U>)
    [[nodiscard]] FO_FORCE_INLINE auto operator*() noexcept -> U&
    {
        return *_owner;
    }
    template<typename U = T>
        requires(!std::is_void_v<U>)
    [[nodiscard]] FO_FORCE_INLINE auto operator*() const noexcept -> const U&
    {
        return *_owner;
    }
    [[nodiscard]] FO_FORCE_INLINE auto get() noexcept -> T* { return _owner.get(); }
    [[nodiscard]] FO_FORCE_INLINE auto get() const noexcept -> const T* { return _owner.get(); }
    [[nodiscard]] FO_FORCE_INLINE auto get_no_const() const noexcept -> T* { return _owner.get_no_const(); }
    [[nodiscard]] FO_FORCE_INLINE auto as_ptr() noexcept -> ptr<T> { return _owner.as_ptr(); }
    [[nodiscard]] FO_FORCE_INLINE auto as_ptr() const noexcept -> ptr<const T> { return _owner.as_ptr(); }
    [[nodiscard]] FO_FORCE_INLINE auto as_nptr() noexcept -> nptr<T> { return _owner.as_nptr(); }
    [[nodiscard]] FO_FORCE_INLINE auto as_nptr() const noexcept -> nptr<const T> { return _owner.as_nptr(); }
#if !FO_STRICT_OWNING_NONNULL
    [[nodiscard]] FO_FORCE_INLINE auto release() noexcept -> T* { return _owner.release(); }
#else
    [[nodiscard]] FO_FORCE_INLINE auto release() & noexcept -> ptr<T> = delete;
    [[nodiscard]] FO_FORCE_INLINE auto release() && noexcept -> ptr<T> { return ptr<T> {_owner.release()}; }
#endif
    [[nodiscard]] FO_FORCE_INLINE operator unique_del_nptr<T>() && noexcept { return unique_del_nptr<T> {std::move(_owner)}; }

private:
    unique_del_nptr<T> _owner;
};
static_assert(FO_STRICT_OWNING_NONNULL || std::is_default_constructible_v<unique_del_ptr<int32_t>>);
static_assert(!FO_STRICT_OWNING_NONNULL || !std::is_default_constructible_v<unique_del_ptr<int32_t>>);
static_assert(FO_STRICT_OWNING_NONNULL || std::is_constructible_v<unique_del_ptr<int32_t>, std::nullptr_t>);
static_assert(!FO_STRICT_OWNING_NONNULL || !std::is_constructible_v<unique_del_ptr<int32_t>, std::nullptr_t>);
static_assert(FO_STRICT_OWNING_NONNULL || std::is_assignable_v<unique_del_ptr<int32_t>&, std::nullptr_t>);
static_assert(!FO_STRICT_OWNING_NONNULL || !std::is_assignable_v<unique_del_ptr<int32_t>&, std::nullptr_t>);
static_assert(!std::is_copy_constructible_v<unique_del_ptr<int32_t>>);

template<typename T>
[[nodiscard]] FO_FORCE_INLINE auto adopt_unique_ptr(ptr<T> value) noexcept -> unique_ptr<T>
{
    return unique_ptr<T> {value.get()};
}

template<typename T, typename Deleter>
[[nodiscard]] FO_FORCE_INLINE auto make_unique_del_ptr(ptr<T> value, Deleter&& deleter) -> unique_del_ptr<T>
{
    return unique_del_ptr<T> {value.get(), std::forward<Deleter>(deleter)};
}

template<typename T, typename Deleter>
[[nodiscard]] FO_FORCE_INLINE auto make_unique_del_ptr(nptr<T> value, Deleter&& deleter) -> unique_del_nptr<T>
{
    return unique_del_nptr<T> {value.get(), std::forward<Deleter>(deleter)};
}

template<typename T>
[[nodiscard]] FO_FORCE_INLINE auto take_not_null(unique_del_nptr<T>& value) noexcept -> unique_del_ptr<T>
{
    assert(value);
    auto value_ptr = value.as_ptr();
    auto deleter = std::move(value.get_underlying().get_deleter());
    nptr<T> released = value.release();
    ignore_unused(released);
    return unique_del_ptr<T> {value_ptr.get(), std::move(deleter)};
}

FO_END_NAMESPACE
template<typename T>
struct FO_NAMESPACE hashing::hash<FO_NAMESPACE propagate_const<T>>
{
    using is_avalanching = void;
    auto operator()(const FO_NAMESPACE propagate_const<T>& v) const noexcept -> size_t { return ptr_hash(v.get()); }
};
FO_BEGIN_NAMESPACE

FO_END_NAMESPACE
