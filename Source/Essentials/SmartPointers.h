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

namespace details
{
    template<typename T>
    [[nodiscard]] FO_FORCE_INLINE auto make_void_ptr(T* value) noexcept -> void*
    {
        return const_cast<void*>(static_cast<const void*>(value));
    }
}

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
class unique_ptr;
template<typename T>
class unique_nptr;
template<typename T>
class unique_del_ptr;
template<typename T>
class unique_del_nptr;
template<typename T>
class unique_arr_ptr;
template<typename T>
class shared_ptr;

// Owning smart pointers (refcount_ptr/unique_ptr/shared_ptr/unique_del_ptr and their nullable variants)
// implicitly convert to a borrowed view (ptr/nptr) via ptr's/nptr's converting constructors below.
template<typename>
inline constexpr bool is_nonnull_owning_pointer_v = false;
template<typename T>
inline constexpr bool is_nonnull_owning_pointer_v<refcount_ptr<T>> = true;
template<typename T>
inline constexpr bool is_nonnull_owning_pointer_v<unique_ptr<T>> = true;
template<typename T>
inline constexpr bool is_nonnull_owning_pointer_v<unique_del_ptr<T>> = true;

template<typename T>
inline constexpr bool is_owning_pointer_v = is_nonnull_owning_pointer_v<T>;
template<typename T>
inline constexpr bool is_owning_pointer_v<refcount_nptr<T>> = true;
template<typename T>
inline constexpr bool is_owning_pointer_v<unique_nptr<T>> = true;
template<typename T>
inline constexpr bool is_owning_pointer_v<unique_del_nptr<T>> = true;
template<typename T>
inline constexpr bool is_owning_pointer_v<unique_arr_ptr<T>> = true;
template<typename T>
inline constexpr bool is_owning_pointer_v<shared_ptr<T>> = true;

// The nullable borrow wrapper. A guard-checked nptr implicitly narrows to a non-null ptr (with an always-on
// non-null assert) through ptr's converting constructor below, so a checked nptr needs no manual .as_ptr().
template<typename>
inline constexpr bool is_nptr_v = false;
template<typename T>
inline constexpr bool is_nptr_v<nptr<T>> = true;

template<typename T>
class ptr
{
    template<typename U>
    friend class ptr;
    template<typename U>
    friend class nptr;

public:
    using element_type = T;

    FO_FORCE_INLINE constexpr ptr() noexcept = delete;
    FO_FORCE_INLINE constexpr ptr(std::nullptr_t) noexcept = delete;
    FO_FORCE_INLINE auto operator=(std::nullptr_t) noexcept -> ptr& = delete;
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr ptr(T* other) noexcept :
        _ptr(other)
    {
        FO_BASIC_STRONG_ASSERT(_ptr != nullptr);
    }
    FO_FORCE_INLINE auto operator=(T* other) noexcept -> ptr&
    {
        FO_BASIC_STRONG_ASSERT(other != nullptr);
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

    // Implicit non-null borrow from an owning pointer: reads out the raw pointer without taking ownership and
    // asserts non-null exactly like as_ptr() did. Const-propagating through the owner's get().
    template<typename Owner>
        requires(is_owning_pointer_v<std::remove_cvref_t<Owner>> && std::is_convertible_v<decltype(std::declval<Owner>().get()), T*>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE ptr(Owner&& owner) noexcept :
        _ptr(owner.get())
    {
        FO_BASIC_STRONG_ASSERT(_ptr != nullptr);
    }

    // Implicit narrow from a nullable borrow (nptr): reads out the pointer and asserts non-null at the conversion
    // point, exactly like the raw-pointer constructor above. This lets a guard-checked nptr flow straight into a
    // ptr<T> parameter/member/return without a manual .as_ptr(). Const-propagating and mutability-preserving through
    // the source's get(), mirroring the owning-pointer borrow constructor: a const nptr<T> yields ptr<const T>.
    template<typename NPtr>
        requires(is_nptr_v<std::remove_cvref_t<NPtr>> && std::is_convertible_v<decltype(std::declval<NPtr>().get()), T*>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE ptr(NPtr&& other) noexcept :
        _ptr(other.get())
    {
        FO_BASIC_STRONG_ASSERT(_ptr != nullptr);
    }

    FO_FORCE_INLINE ~ptr() noexcept = default;

    [[nodiscard]] FO_FORCE_INLINE explicit operator bool() const noexcept = delete;
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const ptr& other) const noexcept -> bool { return _ptr == other._ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const ptr& other) const noexcept -> bool { return _ptr < other._ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const T* other) const noexcept -> bool { return _ptr == other; }
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const T* other) const noexcept -> bool { return _ptr < other; }
    [[nodiscard]] FO_FORCE_INLINE auto operator==(std::nullptr_t) const noexcept -> bool = delete;
    [[nodiscard]] FO_FORCE_INLINE auto operator!=(std::nullptr_t) const noexcept -> bool = delete;
    [[nodiscard]] FO_FORCE_INLINE constexpr auto operator->() noexcept -> T* { return _ptr; }
    [[nodiscard]] FO_FORCE_INLINE constexpr auto operator->() const noexcept -> const T* { return _ptr; }
    template<typename U = T>
        requires(std::is_same_v<std::remove_const_t<U>, char>)
    [[nodiscard]] FO_FORCE_INLINE auto as_str(size_t len) const noexcept -> string_view
    {
        return {_ptr, len};
    }
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
    // Pointer-to-member access. A raw `p->*pmf` expression is not a first-class value, so to support
    // `(sp->*pmf)(args...)` the operator returns a small forwarding callable bound to the pointee.
    template<typename M>
    [[nodiscard]] FO_FORCE_INLINE auto operator->*(M member) noexcept
    {
        return [obj = _ptr, member](auto&&... args) -> decltype(auto) { return (obj->*member)(std::forward<decltype(args)>(args)...); };
    }
    template<typename M>
    [[nodiscard]] FO_FORCE_INLINE auto operator->*(M member) const noexcept
    {
        return [obj = _ptr, member](auto&&... args) -> decltype(auto) { return (obj->*member)(std::forward<decltype(args)>(args)...); };
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
    [[nodiscard]] FO_FORCE_INLINE auto get_pp() noexcept -> T** = delete;
    [[nodiscard]] FO_FORCE_INLINE auto get_pp() const noexcept -> T* const* = delete;
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
    FO_FORCE_INLINE void reset(T* p) noexcept
    {
        FO_BASIC_STRONG_ASSERT(p != nullptr);
        _ptr = p;
    }
    FO_FORCE_INLINE void reset(std::nullptr_t) noexcept = delete;

    template<typename U>
        requires(dynamically_castable_to<T, U>)
    FO_FORCE_INLINE auto dyn_cast() noexcept -> nptr<U>;

    template<typename U>
        requires(dynamically_castable_to<T, const U>)
    FO_FORCE_INLINE auto dyn_cast() const noexcept -> nptr<const U>;

    // Reinterpret the pointee type through void for low-level byte/ABI views (e.g. `ucolor` <-> `uint8_t`),
    // replacing the `cast_from_void<U*>(cast_to_void(p.get()))` idiom. A `ptr<void>` source is also supported
    // (`void` -> `U`, as `GetPtrAs<U>()` uses). Source-pointee constness is preserved, so this can never
    // silently strip `const`.
    template<typename U>
    FO_FORCE_INLINE auto reinterpret_as() const noexcept -> ptr<std::conditional_t<std::is_const_v<T>, const U, U>>
    {
        using target_type = std::conditional_t<std::is_const_v<T>, const U, U>;
        if constexpr (std::is_void_v<std::remove_const_t<T>>) {
            return ptr<target_type>(static_cast<target_type*>(_ptr));
        }
        else {
            return ptr<target_type>(reinterpret_cast<target_type*>(_ptr));
        }
    }

    // Borrow this pointer as a raw opaque `void*` for immediate ABI handoff. `void*` is an opaque handle
    // (nothing is read/written through it), so pointee-const is dropped exactly as the old `cast_to_void` did.
    [[nodiscard]] FO_FORCE_INLINE auto void_cast() const noexcept -> void* { return details::make_void_ptr(_ptr); }

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
static_assert(!std::is_default_constructible_v<ptr<int32_t>>);
static_assert(!std::is_constructible_v<ptr<int32_t>, std::nullptr_t>);
static_assert(!std::is_assignable_v<ptr<int32_t>&, std::nullptr_t>);

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

    // Implicit nullable borrow from any owning pointer (including the nullable refcount_nptr/unique_nptr): reads
    // out the raw pointer without taking ownership. Const-propagating through the owner's get().
    template<typename Owner>
        requires(is_owning_pointer_v<std::remove_cvref_t<Owner>> && std::is_convertible_v<decltype(std::declval<Owner>().get()), T*>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE nptr(Owner&& owner) noexcept :
        _ptr(owner.get())
    {
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
        FO_BASIC_STRONG_ASSERT(_ptr != nullptr);
        return ptr<T>(_ptr);
    }
    [[nodiscard]] FO_FORCE_INLINE auto as_ptr() const noexcept -> ptr<const T>
    {
        FO_BASIC_STRONG_ASSERT(_ptr != nullptr);
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

    // Reinterpret the pointee type through void for low-level byte/ABI views, replacing the
    // `cast_from_void<U*>(cast_to_void(p.get()))` idiom. Nullability and source-pointee constness are
    // preserved (null reinterprets to null), so this can never silently strip `const`.
    template<typename U>
    FO_FORCE_INLINE auto reinterpret_as() const noexcept -> nptr<std::conditional_t<std::is_const_v<T>, const U, U>>
    {
        using target_type = std::conditional_t<std::is_const_v<T>, const U, U>;
        if constexpr (std::is_void_v<std::remove_const_t<T>>) {
            return nptr<target_type>(static_cast<target_type*>(_ptr));
        }
        else {
            return nptr<target_type>(reinterpret_cast<target_type*>(_ptr));
        }
    }

    // Borrow this pointer as a raw opaque `void*` for immediate ABI handoff. `void*` is an opaque handle
    // (nothing is read/written through it), so pointee-const is dropped exactly as the old `cast_to_void` did.
    [[nodiscard]] FO_FORCE_INLINE auto void_cast() const noexcept -> void* { return details::make_void_ptr(_ptr); }

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
    requires(std::is_pointer_v<T> && !std::is_function_v<std::remove_pointer_t<T>>)
[[nodiscard]] FO_FORCE_INLINE constexpr auto make_ptr(T value) noexcept -> ptr<std::remove_pointer_t<T>>
{
    return ptr<std::remove_pointer_t<T>> {value};
}

template<typename T>
    requires(std::is_pointer_v<T> && !std::is_function_v<std::remove_pointer_t<T>>)
[[nodiscard]] FO_FORCE_INLINE constexpr auto make_nptr(T value) noexcept -> nptr<std::remove_pointer_t<T>>
{
    return nptr<std::remove_pointer_t<T>> {value};
}

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

    FO_FORCE_INLINE constexpr unique_ptr() noexcept = delete;
    FO_FORCE_INLINE constexpr unique_ptr(std::nullptr_t) noexcept = delete;
    FO_FORCE_INLINE auto operator=(std::nullptr_t) noexcept -> unique_ptr& = delete;
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr unique_ptr(T* p) noexcept :
        _ptr(p)
    {
        FO_BASIC_STRONG_ASSERT(_ptr != nullptr);
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

    [[nodiscard]] FO_FORCE_INLINE explicit operator bool() const noexcept = delete;
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const unique_ptr& other) const noexcept -> bool { return _ptr == other._ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const unique_ptr& other) const noexcept -> bool { return _ptr < other._ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const T* other) const noexcept -> bool { return _ptr == other; }
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const T* other) const noexcept -> bool { return _ptr < other; }
    [[nodiscard]] FO_FORCE_INLINE auto operator==(std::nullptr_t) const noexcept -> bool = delete;
    [[nodiscard]] FO_FORCE_INLINE auto operator!=(std::nullptr_t) const noexcept -> bool = delete;
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
    [[nodiscard]] FO_FORCE_INLINE auto void_cast() const noexcept -> void* { return details::make_void_ptr(_ptr); }
    [[nodiscard]] FO_FORCE_INLINE auto operator[](size_t index) noexcept -> T& { return _ptr[index]; }
    [[nodiscard]] FO_FORCE_INLINE auto operator[](size_t index) const noexcept -> const T& { return _ptr[index]; }
    [[nodiscard]] FO_FORCE_INLINE auto release() noexcept -> ptr<T> { return ptr<T> {std::exchange(_ptr, nullptr)}; }

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

    FO_FORCE_INLINE void reset(T* p) noexcept
    {
        FO_BASIC_STRONG_ASSERT(p != nullptr);
        delete std::exchange(_ptr, p);
    }
    FO_FORCE_INLINE void reset(std::nullptr_t) noexcept = delete;

private:
    FO_FORCE_INLINE void delete_current() noexcept { delete std::exchange(_ptr, nullptr); }

    T* _ptr;
};
static_assert(sizeof(unique_ptr<int32_t>) == sizeof(int32_t*));
static_assert(std::is_standard_layout_v<unique_ptr<int32_t>>);
static_assert(!std::is_default_constructible_v<unique_ptr<int32_t>>);
static_assert(!std::is_constructible_v<unique_ptr<int32_t>, std::nullptr_t>);
static_assert(!std::is_assignable_v<unique_ptr<int32_t>&, std::nullptr_t>);

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
        _ptr = p.release().get();
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
        _ptr = p.release().get();
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
        FO_BASIC_STRONG_ASSERT(_ptr != nullptr);
        return ptr<T>(_ptr);
    }
    [[nodiscard]] FO_FORCE_INLINE auto as_ptr() const noexcept -> ptr<const T>
    {
        FO_BASIC_STRONG_ASSERT(_ptr != nullptr);
        return ptr<const T>(_ptr);
    }
    [[nodiscard]] FO_FORCE_INLINE auto as_nptr() noexcept -> nptr<T> { return nptr<T>(_ptr); }
    [[nodiscard]] FO_FORCE_INLINE auto as_nptr() const noexcept -> nptr<const T> { return nptr<const T>(_ptr); }
    [[nodiscard]] FO_FORCE_INLINE auto void_cast() const noexcept -> void* { return details::make_void_ptr(_ptr); }
    [[nodiscard]] FO_FORCE_INLINE auto operator[](size_t index) noexcept -> T& { return _ptr[index]; }
    [[nodiscard]] FO_FORCE_INLINE auto operator[](size_t index) const noexcept -> const T& { return _ptr[index]; }
    [[nodiscard]] FO_FORCE_INLINE auto release() noexcept -> nptr<T> { return nptr<T> {std::exchange(_ptr, nullptr)}; }
    [[nodiscard]] FO_FORCE_INLINE auto take_not_null() noexcept -> unique_ptr<T>
    {
        FO_BASIC_STRONG_ASSERT(_ptr != nullptr);
        auto released_ptr = ptr<T> {release()};
        return unique_ptr<T>(released_ptr.get());
    }

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

private:
    struct adopt_tag
    {
    } static constexpr adopt {};

private:
    FO_FORCE_INLINE constexpr refcount_ptr() noexcept :
        _ptr(nullptr)
    {
    }

public:
    FO_FORCE_INLINE constexpr refcount_ptr(std::nullptr_t) noexcept = delete;
    FO_FORCE_INLINE auto operator=(std::nullptr_t) noexcept -> refcount_ptr& = delete;

    [[nodiscard]] FO_FORCE_INLINE static auto from_add_ref(T* p) noexcept -> refcount_ptr
    {
        FO_BASIC_STRONG_ASSERT(p != nullptr);
        refcount_ptr result;
        result._ptr = p;
        result.add_ref();
        return result;
    }

    [[nodiscard]] FO_FORCE_INLINE static auto try_from_add_ref(T* p) noexcept -> refcount_nptr<T>;

    [[nodiscard]] FO_FORCE_INLINE static auto from_adopted_ref(T* p) noexcept -> refcount_ptr
    {
        FO_BASIC_STRONG_ASSERT(p != nullptr);
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

    [[nodiscard]] FO_FORCE_INLINE explicit operator bool() const noexcept = delete;
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const refcount_ptr& other) const noexcept -> bool { return _ptr == other._ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const refcount_ptr& other) const noexcept -> bool { return _ptr < other._ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const T* other) const noexcept -> bool { return _ptr == other; }
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const T* other) const noexcept -> bool { return _ptr < other; }
    [[nodiscard]] FO_FORCE_INLINE auto operator==(std::nullptr_t) const noexcept -> bool = delete;
    [[nodiscard]] FO_FORCE_INLINE auto operator!=(std::nullptr_t) const noexcept -> bool = delete;
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
    [[nodiscard]] FO_FORCE_INLINE auto void_cast() const noexcept -> void* { return details::make_void_ptr(_ptr); }
    [[nodiscard]] FO_FORCE_INLINE auto get_pp() noexcept -> T** { return &_ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto get_pp() const noexcept -> T* const* { return &_ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator[](size_t index) noexcept -> T& { return _ptr[index]; }
    [[nodiscard]] FO_FORCE_INLINE auto operator[](size_t index) const noexcept -> const T& { return _ptr[index]; }
    [[nodiscard]] FO_FORCE_INLINE auto release_ownership() & noexcept -> T* = delete;
    [[nodiscard]] FO_FORCE_INLINE auto release_ownership() && noexcept -> T* { return std::exchange(_ptr, nullptr); }

    FO_FORCE_INLINE void reset(T* p) noexcept
    {
        FO_BASIC_STRONG_ASSERT(p != nullptr);
        dec_ref();
        _ptr = p;
        add_ref();
    }
    FO_FORCE_INLINE void reset(std::nullptr_t) noexcept = delete;

    template<typename U>
        requires(dynamically_castable_to<T, U> && refcountable<U>)
    FO_FORCE_INLINE auto dyn_cast() noexcept -> refcount_nptr<U>;

    template<typename U>
        requires(dynamically_castable_to<T, U> && !refcountable<U>)
    FO_FORCE_INLINE auto dyn_cast() noexcept -> nptr<U>;

    template<typename U>
        requires(dynamically_castable_to<T, const U> && refcountable<const U>)
    FO_FORCE_INLINE auto dyn_cast() const noexcept -> refcount_nptr<const U>;

    template<typename U>
        requires(dynamically_castable_to<T, const U> && !refcountable<const U>)
    FO_FORCE_INLINE auto dyn_cast() const noexcept -> nptr<const U>;

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
static_assert(!std::is_default_constructible_v<refcount_ptr<int32_t>>);
static_assert(!std::is_constructible_v<refcount_ptr<int32_t>, std::nullptr_t>);
static_assert(!std::is_assignable_v<refcount_ptr<int32_t>&, std::nullptr_t>);

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
        FO_BASIC_STRONG_ASSERT(_ptr != nullptr);
        return ptr<T>(_ptr);
    }
    [[nodiscard]] FO_FORCE_INLINE auto as_ptr() const noexcept -> ptr<const T>
    {
        FO_BASIC_STRONG_ASSERT(_ptr != nullptr);
        return ptr<const T>(_ptr);
    }
    [[nodiscard]] FO_FORCE_INLINE auto as_nptr() noexcept -> nptr<T> { return nptr<T>(_ptr); }
    [[nodiscard]] FO_FORCE_INLINE auto as_nptr() const noexcept -> nptr<const T> { return nptr<const T>(_ptr); }
    [[nodiscard]] FO_FORCE_INLINE auto void_cast() const noexcept -> void* { return details::make_void_ptr(_ptr); }
    [[nodiscard]] FO_FORCE_INLINE auto get_pp() noexcept -> T** { return &_ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto get_pp() const noexcept -> T* const* { return &_ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator[](size_t index) noexcept -> T& { return _ptr[index]; }
    [[nodiscard]] FO_FORCE_INLINE auto operator[](size_t index) const noexcept -> const T& { return _ptr[index]; }
    [[nodiscard]] FO_FORCE_INLINE auto release_ownership() noexcept -> T* { return std::exchange(_ptr, nullptr); }
    [[nodiscard]] FO_FORCE_INLINE auto take_not_null() noexcept -> refcount_ptr<T>
    {
        FO_BASIC_STRONG_ASSERT(_ptr != nullptr);
        return refcount_ptr<T>::from_adopted_ref(release_ownership());
    }

    FO_FORCE_INLINE void reset(T* p = nullptr) noexcept
    {
        dec_ref();
        _ptr = p;
        add_ref();
    }

    template<typename U>
        requires(dynamically_castable_to<T, U> && refcountable<U>)
    FO_FORCE_INLINE auto dyn_cast() noexcept -> refcount_nptr<U>
    {
        return nptr<U>(dynamic_cast<U*>(_ptr)).try_hold_ref();
    }

    template<typename U>
        requires(dynamically_castable_to<T, U> && !refcountable<U>)
    FO_FORCE_INLINE auto dyn_cast() noexcept -> nptr<U>
    {
        return nptr<U>(dynamic_cast<U*>(_ptr));
    }

    template<typename U>
        requires(dynamically_castable_to<T, const U> && refcountable<const U>)
    FO_FORCE_INLINE auto dyn_cast() const noexcept -> refcount_nptr<const U>
    {
        return nptr<const U>(dynamic_cast<const U*>(_ptr)).try_hold_ref();
    }

    template<typename U>
        requires(dynamically_castable_to<T, const U> && !refcountable<const U>)
    FO_FORCE_INLINE auto dyn_cast() const noexcept -> nptr<const U>
    {
        return nptr<const U>(dynamic_cast<const U*>(_ptr));
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
    requires(dynamically_castable_to<T, U> && refcountable<U>)
FO_FORCE_INLINE auto refcount_ptr<T>::dyn_cast() noexcept -> refcount_nptr<U>
{
    return nptr<U>(dynamic_cast<U*>(_ptr)).try_hold_ref();
}

template<typename T>
template<typename U>
    requires(dynamically_castable_to<T, U> && !refcountable<U>)
FO_FORCE_INLINE auto refcount_ptr<T>::dyn_cast() noexcept -> nptr<U>
{
    return nptr<U>(dynamic_cast<U*>(_ptr));
}

template<typename T>
template<typename U>
    requires(dynamically_castable_to<T, const U> && refcountable<const U>)
FO_FORCE_INLINE auto refcount_ptr<T>::dyn_cast() const noexcept -> refcount_nptr<const U>
{
    return nptr<const U>(dynamic_cast<const U*>(_ptr)).try_hold_ref();
}

template<typename T>
template<typename U>
    requires(dynamically_castable_to<T, const U> && !refcountable<const U>)
FO_FORCE_INLINE auto refcount_ptr<T>::dyn_cast() const noexcept -> nptr<const U>
{
    return nptr<const U>(dynamic_cast<const U*>(_ptr));
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

// Shared-ownership control block: the strong count owns the object, the weak count owns the block.
// destroy_object() is a virtual hook, so shared_ptr/weak_ptr never need the complete pointee type at
// the destruction site (the same type erasure std::shared_ptr provides).
class shared_ptr_control_block
{
public:
    FO_FORCE_INLINE shared_ptr_control_block() noexcept = default;
    shared_ptr_control_block(const shared_ptr_control_block&) = delete;
    shared_ptr_control_block(shared_ptr_control_block&&) noexcept = delete;
    auto operator=(const shared_ptr_control_block&) = delete;
    auto operator=(shared_ptr_control_block&&) noexcept = delete;
    virtual ~shared_ptr_control_block() = default;

    FO_FORCE_INLINE void add_strong_ref() noexcept { _strongRefs.fetch_add(1, std::memory_order_relaxed); }
    [[nodiscard]] FO_FORCE_INLINE auto add_strong_ref_if_alive() noexcept -> bool
    {
        int64_t refs = _strongRefs.load(std::memory_order_relaxed);

        while (refs > 0) {
            if (_strongRefs.compare_exchange_weak(refs, refs + 1, std::memory_order_acquire, std::memory_order_relaxed)) {
                return true;
            }
        }

        return false;
    }
    FO_FORCE_INLINE void release_strong_ref() noexcept
    {
        if (_strongRefs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            destroy_object();
            release_weak_ref();
        }
    }
    FO_FORCE_INLINE void add_weak_ref() noexcept { _weakRefs.fetch_add(1, std::memory_order_relaxed); }
    FO_FORCE_INLINE void release_weak_ref() noexcept
    {
        if (_weakRefs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            delete this;
        }
    }
    [[nodiscard]] FO_FORCE_INLINE auto strong_ref_count() const noexcept -> size_t
    {
        const int64_t refs = _strongRefs.load(std::memory_order_relaxed);
        return refs > 0 ? static_cast<size_t>(refs) : 0;
    }

private:
    virtual void destroy_object() noexcept = 0;

    std::atomic<int64_t> _strongRefs {1};
    std::atomic<int64_t> _weakRefs {1}; // one weak ref is held collectively by all strong refs
};

// Control block with the object embedded in the same allocation (what SafeAlloc::MakeShared creates)
template<typename T>
class shared_ptr_storage_block final : public shared_ptr_control_block
{
public:
    template<typename... Args>
    FO_FORCE_INLINE explicit shared_ptr_storage_block(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
    {
        new (static_cast<void*>(&_storage)) T(std::forward<Args>(args)...);
    }

    [[nodiscard]] FO_FORCE_INLINE auto stored_object() noexcept -> T* { return std::launder(reinterpret_cast<T*>(&_storage)); }

private:
    void destroy_object() noexcept override { std::destroy_at(stored_object()); }

    alignas(T) uint8_t _storage[sizeof(T)];
};

class SafeAlloc;

template<typename T>
class shared_ptr;
template<typename T>
class weak_ptr;
template<typename T>
class enable_shared_from_this;
template<typename T, typename X>
FO_FORCE_INLINE void init_shared_from_this_weak(const shared_ptr<T>& owner, enable_shared_from_this<X>* base) noexcept;

template<typename T>
class shared_ptr
{
    template<typename U>
    friend class shared_ptr;
    template<typename U>
    friend class weak_ptr;
    friend class SafeAlloc;

public:
    using element_type = T;
    static_assert(std::is_class_v<T> || std::is_arithmetic_v<T>);

    FO_FORCE_INLINE constexpr shared_ptr() noexcept = default;
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr shared_ptr(std::nullptr_t) noexcept { }
    FO_FORCE_INLINE auto operator=(std::nullptr_t) noexcept -> shared_ptr&
    {
        reset();
        return *this;
    }

    FO_FORCE_INLINE shared_ptr(const shared_ptr& other) noexcept :
        _block(other._block),
        _obj(other._obj)
    {
        if (_block != nullptr) {
            _block->add_strong_ref();
        }
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE shared_ptr(const shared_ptr<U>& other) noexcept :
        _block(other._block),
        _obj(other._obj)
    {
        if (_block != nullptr) {
            _block->add_strong_ref();
        }
    }
    FO_FORCE_INLINE shared_ptr(shared_ptr&& other) noexcept :
        _block(std::exchange(other._block, nullptr)),
        _obj(std::exchange(other._obj, nullptr))
    {
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE shared_ptr(shared_ptr<U>&& other) noexcept : // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
        _block(std::exchange(other._block, nullptr)),
        _obj(std::exchange(other._obj, nullptr))
    {
    }
    // Aliasing constructor: shares ownership with the source but exposes a related object (cast results)
    template<typename U>
    FO_FORCE_INLINE shared_ptr(shared_ptr<U>&& other, T* aliased_obj) noexcept : // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
        _block(std::exchange(other._block, nullptr)),
        _obj(aliased_obj)
    {
        other._obj = nullptr;
        FO_BASIC_STRONG_ASSERT(_block != nullptr || _obj == nullptr);
    }
    FO_FORCE_INLINE auto operator=(const shared_ptr& other) noexcept -> shared_ptr&
    {
        if (this != &other) {
            // Read the source before releasing our reference: the source may live inside our pointee
            shared_ptr_control_block* other_block = other._block;
            T* other_obj = other._obj;

            if (other_block != nullptr) {
                other_block->add_strong_ref();
            }
            if (_block != nullptr) {
                _block->release_strong_ref();
            }

            _block = other_block;
            _obj = other_obj;
        }

        return *this;
    }
    FO_FORCE_INLINE auto operator=(shared_ptr&& other) noexcept -> shared_ptr&
    {
        if (this != &other) {
            // Steal the source before releasing our reference: the source may live inside our pointee
            shared_ptr_control_block* other_block = std::exchange(other._block, nullptr);
            T* other_obj = std::exchange(other._obj, nullptr);

            if (_block != nullptr) {
                _block->release_strong_ref();
            }

            _block = other_block;
            _obj = other_obj;
        }

        return *this;
    }

    FO_FORCE_INLINE ~shared_ptr()
    {
        if (_block != nullptr) {
            _block->release_strong_ref();
        }
    }

    [[nodiscard]] FO_FORCE_INLINE explicit operator bool() const noexcept { return _obj != nullptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const shared_ptr& other) const noexcept -> bool { return _obj == other._obj; }
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const shared_ptr& other) const noexcept -> bool { return _obj < other._obj; }
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const T* other) const noexcept -> bool { return _obj == other; }
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const T* other) const noexcept -> bool { return _obj < other; }
    [[nodiscard]] FO_FORCE_INLINE auto operator->() noexcept -> T* { return _obj; }
    [[nodiscard]] FO_FORCE_INLINE auto operator->() const noexcept -> const T* { return _obj; }
    [[nodiscard]] FO_FORCE_INLINE auto operator*() noexcept -> T& { return *_obj; }
    [[nodiscard]] FO_FORCE_INLINE auto operator*() const noexcept -> const T& { return *_obj; }
    [[nodiscard]] FO_FORCE_INLINE auto get() noexcept -> T* { return _obj; }
    [[nodiscard]] FO_FORCE_INLINE auto get() const noexcept -> const T* { return _obj; }
    [[nodiscard]] FO_FORCE_INLINE auto get_no_const() const noexcept -> T* { return _obj; }
    [[nodiscard]] FO_FORCE_INLINE auto as_ptr() noexcept -> ptr<T>
    {
        FO_BASIC_STRONG_ASSERT(_obj != nullptr);
        return ptr<T>(_obj);
    }
    [[nodiscard]] FO_FORCE_INLINE auto as_ptr() const noexcept -> ptr<const T>
    {
        FO_BASIC_STRONG_ASSERT(_obj != nullptr);
        return ptr<const T>(_obj);
    }
    [[nodiscard]] FO_FORCE_INLINE auto as_nptr() noexcept -> nptr<T> { return nptr<T>(_obj); }
    [[nodiscard]] FO_FORCE_INLINE auto as_nptr() const noexcept -> nptr<const T> { return nptr<const T>(_obj); }
    [[nodiscard]] FO_FORCE_INLINE auto void_cast() const noexcept -> void* { return details::make_void_ptr(_obj); }
    // ReSharper disable once CppInconsistentNaming
    [[nodiscard]] FO_FORCE_INLINE auto use_count() const noexcept -> size_t { return _block != nullptr ? _block->strong_ref_count() : 0; }
    template<typename U>
        requires(dynamically_castable_to<T, U>)
    [[nodiscard]] FO_FORCE_INLINE auto dyn_cast() noexcept -> shared_ptr<U>
    {
        if (U* casted = dynamic_cast<U*>(_obj)) {
            _block->add_strong_ref();
            return shared_ptr<U>(_block, casted);
        }

        return shared_ptr<U>();
    }
    template<typename U>
        requires(dynamically_castable_to<T, const U>)
    [[nodiscard]] FO_FORCE_INLINE auto dyn_cast() const noexcept -> shared_ptr<const U>
    {
        if (const U* casted = dynamic_cast<const U*>(_obj)) {
            _block->add_strong_ref();
            return shared_ptr<const U>(_block, casted);
        }

        return shared_ptr<const U>();
    }
    template<typename U = T>
        requires(std::is_const_v<U>)
    [[nodiscard]] FO_FORCE_INLINE auto cast_no_const() const noexcept -> shared_ptr<std::remove_const_t<U>>
    {
        using mutable_type = std::remove_const_t<U>;

        if (_obj == nullptr) {
            return shared_ptr<mutable_type>();
        }

        _block->add_strong_ref();
        return shared_ptr<mutable_type>(_block, const_cast<mutable_type*>(_obj));
    }
    FO_FORCE_INLINE void reset() noexcept
    {
        if (_block != nullptr) {
            _block->release_strong_ref();
        }

        _block = nullptr;
        _obj = nullptr;
    }

private:
    FO_FORCE_INLINE shared_ptr(shared_ptr_control_block* block, T* obj) noexcept :
        _block(block),
        _obj(obj)
    {
    }

    shared_ptr_control_block* _block {};
    T* _obj {};
};

template<typename T>
class weak_ptr
{
    template<typename U>
    friend class weak_ptr;

public:
    using element_type = T;

    FO_FORCE_INLINE constexpr weak_ptr() noexcept = default;
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr weak_ptr(std::nullptr_t) noexcept { }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE weak_ptr(const shared_ptr<U>& owner) noexcept :
        _block(owner._block),
        _obj(owner._obj)
    {
        if (_block != nullptr) {
            _block->add_weak_ref();
        }
    }

    FO_FORCE_INLINE weak_ptr(const weak_ptr& other) noexcept :
        _block(other._block),
        _obj(other._obj)
    {
        if (_block != nullptr) {
            _block->add_weak_ref();
        }
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE weak_ptr(const weak_ptr<U>& other) noexcept :
        _block(other._block),
        _obj(other._obj)
    {
        if (_block != nullptr) {
            _block->add_weak_ref();
        }
    }
    FO_FORCE_INLINE weak_ptr(weak_ptr&& other) noexcept :
        _block(std::exchange(other._block, nullptr)),
        _obj(std::exchange(other._obj, nullptr))
    {
    }
    FO_FORCE_INLINE auto operator=(const weak_ptr& other) noexcept -> weak_ptr&
    {
        if (this != &other) {
            // Read the source before releasing our reference: the source may live inside owned storage
            shared_ptr_control_block* other_block = other._block;
            T* other_obj = other._obj;

            if (other_block != nullptr) {
                other_block->add_weak_ref();
            }
            if (_block != nullptr) {
                _block->release_weak_ref();
            }

            _block = other_block;
            _obj = other_obj;
        }

        return *this;
    }
    FO_FORCE_INLINE auto operator=(weak_ptr&& other) noexcept -> weak_ptr&
    {
        if (this != &other) {
            // Steal the source before releasing our reference: the source may live inside owned storage
            shared_ptr_control_block* other_block = std::exchange(other._block, nullptr);
            T* other_obj = std::exchange(other._obj, nullptr);

            if (_block != nullptr) {
                _block->release_weak_ref();
            }

            _block = other_block;
            _obj = other_obj;
        }

        return *this;
    }
    template<typename U>
        requires(std::is_convertible_v<U*, T*>)
    FO_FORCE_INLINE auto operator=(const shared_ptr<U>& owner) noexcept -> weak_ptr&
    {
        // Read the source before releasing our reference: the source may live inside owned storage
        shared_ptr_control_block* owner_block = owner._block;
        T* owner_obj = owner._obj;

        if (owner_block != nullptr) {
            owner_block->add_weak_ref();
        }
        if (_block != nullptr) {
            _block->release_weak_ref();
        }

        _block = owner_block;
        _obj = owner_obj;

        return *this;
    }

    FO_FORCE_INLINE ~weak_ptr()
    {
        if (_block != nullptr) {
            _block->release_weak_ref();
        }
    }

    [[nodiscard]] FO_FORCE_INLINE auto lock() const noexcept -> shared_ptr<T>
    {
        if (_block != nullptr && _block->add_strong_ref_if_alive()) {
            return shared_ptr<T>(_block, _obj);
        }

        return shared_ptr<T>();
    }
    // ReSharper disable once CppInconsistentNaming
    [[nodiscard]] FO_FORCE_INLINE auto use_count() const noexcept -> size_t { return _block != nullptr ? _block->strong_ref_count() : 0; }
    [[nodiscard]] FO_FORCE_INLINE auto void_cast() const noexcept -> void* { return details::make_void_ptr(_obj); }

private:
    shared_ptr_control_block* _block {};
    T* _obj {};
};

template<typename T>
class enable_shared_from_this
{
    template<typename U, typename X>
    friend void init_shared_from_this_weak(const shared_ptr<U>& owner, enable_shared_from_this<X>* base) noexcept;

public:
    [[nodiscard]] FO_FORCE_INLINE auto shared_from_this() -> shared_ptr<T>
    {
        auto locked = _weakThis.lock();
        FO_BASIC_STRONG_ASSERT(locked);
        return locked;
    }
    [[nodiscard]] FO_FORCE_INLINE auto shared_from_this() const -> shared_ptr<const T>
    {
        shared_ptr<const T> locked = _weakThis.lock();
        FO_BASIC_STRONG_ASSERT(locked);
        return locked;
    }
    [[nodiscard]] FO_FORCE_INLINE auto weak_from_this() noexcept -> weak_ptr<T> { return _weakThis; }
    [[nodiscard]] FO_FORCE_INLINE auto weak_from_this() const noexcept -> weak_ptr<const T> { return weak_ptr<const T>(_weakThis); }

protected:
    FO_FORCE_INLINE constexpr enable_shared_from_this() noexcept = default;
    FO_FORCE_INLINE enable_shared_from_this(const enable_shared_from_this& other) noexcept { ignore_unused(other); }
    FO_FORCE_INLINE auto operator=(const enable_shared_from_this& other) noexcept -> enable_shared_from_this&
    {
        ignore_unused(other);
        return *this;
    }
    FO_FORCE_INLINE ~enable_shared_from_this() = default;

private:
    weak_ptr<T> _weakThis {};
};

// SafeAlloc::MakeShared wires the embedded weak reference right after construction through the public
// weak-from-shared assignment; the second overload is the no-op fallback for types that do not derive
// from enable_shared_from_this
template<typename T, typename X>
FO_FORCE_INLINE void init_shared_from_this_weak(const shared_ptr<T>& owner, enable_shared_from_this<X>* base) noexcept
{
    base->_weakThis = owner;
}

template<typename T>
FO_FORCE_INLINE void init_shared_from_this_weak(const shared_ptr<T>& owner, const volatile void* base) noexcept
{
    ignore_unused(owner, base);
}

template<typename T>
class unique_arr_ptr
{
public:
    using element_type = T;
    static_assert(std::is_class_v<T> || std::is_arithmetic_v<T>);

    FO_FORCE_INLINE constexpr unique_arr_ptr() noexcept = default;
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE constexpr unique_arr_ptr(std::nullptr_t) noexcept { }
    FO_FORCE_INLINE auto operator=(std::nullptr_t) noexcept -> unique_arr_ptr&
    {
        reset();
        return *this;
    }
    FO_FORCE_INLINE constexpr explicit unique_arr_ptr(T* arr) noexcept :
        _arr(arr)
    {
    }

    FO_FORCE_INLINE unique_arr_ptr(unique_arr_ptr&& other) noexcept :
        _arr(std::exchange(other._arr, nullptr))
    {
    }
    FO_FORCE_INLINE auto operator=(unique_arr_ptr&& other) noexcept -> unique_arr_ptr&
    {
        if (this != &other) {
            delete[] std::exchange(_arr, std::exchange(other._arr, nullptr));
        }

        return *this;
    }

    unique_arr_ptr(const unique_arr_ptr&) = delete;
    auto operator=(const unique_arr_ptr&) -> unique_arr_ptr& = delete;

    FO_FORCE_INLINE ~unique_arr_ptr() { delete[] _arr; }

    [[nodiscard]] FO_FORCE_INLINE explicit operator bool() const noexcept { return _arr != nullptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const unique_arr_ptr& other) const noexcept -> bool { return _arr == other._arr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const unique_arr_ptr& other) const noexcept -> bool { return _arr < other._arr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const T* other) const noexcept -> bool { return _arr == other; }
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const T* other) const noexcept -> bool { return _arr < other; }
    [[nodiscard]] FO_FORCE_INLINE auto operator[](size_t index) noexcept -> T& { return _arr[index]; }
    [[nodiscard]] FO_FORCE_INLINE auto operator[](size_t index) const noexcept -> const T& { return _arr[index]; }
    [[nodiscard]] FO_FORCE_INLINE auto get() noexcept -> T* { return _arr; }
    [[nodiscard]] FO_FORCE_INLINE auto get() const noexcept -> const T* { return _arr; }
    [[nodiscard]] FO_FORCE_INLINE auto get_no_const() const noexcept -> T* { return _arr; }
    [[nodiscard]] FO_FORCE_INLINE auto void_cast() const noexcept -> void* { return details::make_void_ptr(_arr); }
    [[nodiscard]] FO_FORCE_INLINE auto release() noexcept -> T* { return std::exchange(_arr, nullptr); }
    FO_FORCE_INLINE void reset(T* arr = nullptr) noexcept { delete[] std::exchange(_arr, arr); }

private:
    T* _arr {};
};

template<typename T>
class unique_del_nptr
{
public:
    using element_type = T;
    static_assert(std::is_class_v<T> || std::is_arithmetic_v<T>);

    FO_FORCE_INLINE unique_del_nptr() noexcept = default;
    // ReSharper disable once CppNonExplicitConvertingConstructor
    FO_FORCE_INLINE unique_del_nptr(std::nullptr_t) noexcept { }
    FO_FORCE_INLINE auto operator=(std::nullptr_t) noexcept -> unique_del_nptr&
    {
        reset();
        return *this;
    }
    FO_FORCE_INLINE explicit unique_del_nptr(T* p, function<void(T*)>&& deleter) noexcept :
        _ptr(p),
        _deleter(std::move(deleter))
    {
    }

    FO_FORCE_INLINE unique_del_nptr(unique_del_nptr&& other) noexcept :
        _ptr(std::exchange(other._ptr, nullptr)),
        _deleter(std::move(other._deleter))
    {
    }
    FO_FORCE_INLINE auto operator=(unique_del_nptr&& other) noexcept -> unique_del_nptr&
    {
        if (this != &other) {
            // Steal the source before destroying our pointee: the source may live inside it
            T* other_ptr = std::exchange(other._ptr, nullptr);
            function<void(T*)> other_deleter = std::move(other._deleter);

            destroy();

            _ptr = other_ptr;
            _deleter = std::move(other_deleter);
        }

        return *this;
    }

    unique_del_nptr(const unique_del_nptr&) = delete;
    auto operator=(const unique_del_nptr&) -> unique_del_nptr& = delete;

    FO_FORCE_INLINE ~unique_del_nptr() { destroy(); }

    [[nodiscard]] FO_FORCE_INLINE explicit operator bool() const noexcept { return _ptr != nullptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const unique_del_nptr& other) const noexcept -> bool { return _ptr == other._ptr; }
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const unique_del_nptr& other) const noexcept -> bool { return _ptr < other._ptr; }
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
    [[nodiscard]] FO_FORCE_INLINE auto as_ptr() noexcept -> ptr<T>
    {
        FO_BASIC_STRONG_ASSERT(_ptr != nullptr);
        return ptr<T>(_ptr);
    }
    [[nodiscard]] FO_FORCE_INLINE auto as_ptr() const noexcept -> ptr<const T>
    {
        FO_BASIC_STRONG_ASSERT(_ptr != nullptr);
        return ptr<const T>(_ptr);
    }
    [[nodiscard]] FO_FORCE_INLINE auto as_nptr() noexcept -> nptr<T> { return nptr<T>(_ptr); }
    [[nodiscard]] FO_FORCE_INLINE auto as_nptr() const noexcept -> nptr<const T> { return nptr<const T>(_ptr); }
    template<typename U>
    [[nodiscard]] FO_FORCE_INLINE auto reinterpret_as() noexcept
    {
        return as_nptr().template reinterpret_as<U>();
    }
    template<typename U>
    [[nodiscard]] FO_FORCE_INLINE auto reinterpret_as() const noexcept
    {
        return as_nptr().template reinterpret_as<U>();
    }
    [[nodiscard]] FO_FORCE_INLINE auto void_cast() const noexcept -> void* { return details::make_void_ptr(_ptr); }
    [[nodiscard]] FO_FORCE_INLINE auto release() noexcept -> T* { return std::exchange(_ptr, nullptr); }
    FO_FORCE_INLINE void reset(T* p = nullptr) noexcept
    {
        T* old = std::exchange(_ptr, p);

        if (old != nullptr) {
            FO_BASIC_STRONG_ASSERT(_deleter);
            _deleter(old);
        }
    }
    [[nodiscard]] FO_FORCE_INLINE auto get_deleter() noexcept -> function<void(T*)>& { return _deleter; }
    [[nodiscard]] FO_FORCE_INLINE auto get_deleter() const noexcept -> const function<void(T*)>& { return _deleter; }

private:
    FO_FORCE_INLINE void destroy() noexcept
    {
        if (_ptr != nullptr) {
            FO_BASIC_STRONG_ASSERT(_deleter);
            _deleter(std::exchange(_ptr, nullptr));
        }
    }

    T* _ptr {};
    function<void(T*)> _deleter {};
};

template<typename T>
class unique_del_ptr
{
    template<typename U>
    friend class unique_del_ptr;

public:
    using element_type = T;

    FO_FORCE_INLINE unique_del_ptr() noexcept = delete;
    FO_FORCE_INLINE unique_del_ptr(std::nullptr_t) noexcept = delete;
    FO_FORCE_INLINE auto operator=(std::nullptr_t) noexcept -> unique_del_ptr& = delete;
    FO_FORCE_INLINE unique_del_ptr(T* p, function<void(T*)>&& deleter) noexcept :
        _owner(p, std::move(deleter))
    {
        FO_BASIC_STRONG_ASSERT(p != nullptr);
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

    [[nodiscard]] FO_FORCE_INLINE explicit operator bool() const noexcept = delete;
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const unique_del_ptr& other) const noexcept -> bool { return _owner == other._owner; }
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const unique_del_ptr& other) const noexcept -> bool { return _owner < other._owner; }
    [[nodiscard]] FO_FORCE_INLINE auto operator==(const T* other) const noexcept -> bool { return _owner == other; }
    [[nodiscard]] FO_FORCE_INLINE auto operator<(const T* other) const noexcept -> bool { return _owner < other; }
    [[nodiscard]] FO_FORCE_INLINE auto operator==(std::nullptr_t) const noexcept -> bool = delete;
    [[nodiscard]] FO_FORCE_INLINE auto operator!=(std::nullptr_t) const noexcept -> bool = delete;
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
    [[nodiscard]] FO_FORCE_INLINE auto get_no_const() const noexcept -> T* { return _owner.get(); }
    [[nodiscard]] FO_FORCE_INLINE auto as_ptr() noexcept -> ptr<T> { return _owner; }
    [[nodiscard]] FO_FORCE_INLINE auto as_ptr() const noexcept -> ptr<const T> { return _owner; }
    [[nodiscard]] FO_FORCE_INLINE auto as_nptr() noexcept -> nptr<T> { return _owner; }
    [[nodiscard]] FO_FORCE_INLINE auto as_nptr() const noexcept -> nptr<const T> { return _owner; }
    template<typename U>
    [[nodiscard]] FO_FORCE_INLINE auto reinterpret_as() noexcept
    {
        return as_ptr().template reinterpret_as<U>();
    }
    template<typename U>
    [[nodiscard]] FO_FORCE_INLINE auto reinterpret_as() const noexcept
    {
        return as_ptr().template reinterpret_as<U>();
    }
    [[nodiscard]] FO_FORCE_INLINE auto void_cast() const noexcept -> void* { return _owner.void_cast(); }
    [[nodiscard]] FO_FORCE_INLINE auto release() noexcept -> ptr<T> { return ptr<T> {_owner.release()}; }
    [[nodiscard]] FO_FORCE_INLINE operator unique_del_nptr<T>() && noexcept { return unique_del_nptr<T> {std::move(_owner)}; }

private:
    unique_del_nptr<T> _owner;
};
static_assert(!std::is_default_constructible_v<unique_del_ptr<int32_t>>);
static_assert(!std::is_constructible_v<unique_del_ptr<int32_t>, std::nullptr_t>);
static_assert(!std::is_assignable_v<unique_del_ptr<int32_t>&, std::nullptr_t>);
static_assert(!std::is_copy_constructible_v<unique_del_ptr<int32_t>>);

// Heterogeneous wrapper comparison
template<typename L, typename R>
    requires((is_borrow_pointer_wrapper_v<L> || is_owning_pointer_v<L>) && (is_borrow_pointer_wrapper_v<R> || is_owning_pointer_v<R>) && !std::is_same_v<L, R> && requires(const L& l, const R& r) { l.get() == r.get(); })
[[nodiscard]] FO_FORCE_INLINE auto operator==(const L& lhs, const R& rhs) noexcept -> bool
{
    return lhs.get() == rhs.get();
}

template<typename T>
[[nodiscard]] FO_FORCE_INLINE auto adopt_unique_ptr(ptr<T> value) noexcept -> unique_ptr<T>
{
    return unique_ptr<T> {value.get()};
}

template<typename T>
[[nodiscard]] FO_FORCE_INLINE auto adopt_unique_ptr(nptr<T> value) noexcept -> unique_ptr<T>
{
    ptr<T> checked_value = value;
    return adopt_unique_ptr(checked_value);
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
    FO_BASIC_STRONG_ASSERT(value);
    auto value_ptr = ptr<T> {value};
    auto deleter = std::move(value.get_deleter());
    nptr<T> released = value.release();
    ignore_unused(released);
    return unique_del_ptr<T> {value_ptr.get(), std::move(deleter)};
}

FO_END_NAMESPACE
template<typename T>
struct FO_NAMESPACE hashing::hash<FO_NAMESPACE shared_ptr<T>>
{
    using is_avalanching = void;
    auto operator()(const FO_NAMESPACE shared_ptr<T>& v) const noexcept -> size_t { return ptr_hash(v.get()); }
};
template<typename T>
struct FO_NAMESPACE hashing::hash<FO_NAMESPACE unique_arr_ptr<T>>
{
    using is_avalanching = void;
    auto operator()(const FO_NAMESPACE unique_arr_ptr<T>& v) const noexcept -> size_t { return ptr_hash(v.get()); }
};
template<typename T>
struct FO_NAMESPACE hashing::hash<FO_NAMESPACE unique_del_nptr<T>>
{
    using is_avalanching = void;
    auto operator()(const FO_NAMESPACE unique_del_nptr<T>& v) const noexcept -> size_t { return ptr_hash(v.get()); }
};
template<typename T>
struct FO_NAMESPACE hashing::hash<FO_NAMESPACE unique_del_ptr<T>>
{
    using is_avalanching = void;
    auto operator()(const FO_NAMESPACE unique_del_ptr<T>& v) const noexcept -> size_t { return ptr_hash(v.get()); }
};
FO_BEGIN_NAMESPACE

FO_END_NAMESPACE
