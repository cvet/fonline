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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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
#include "FatalError.h"

FO_BEGIN_NAMESPACE

template<typename Signature>
class move_only_function;
template<typename Signature>
class copyable_function;

// Inline target budget. Sized so a closure capturing up to six pointers, or holding one string by value,
// stays in place and the whole wrapper still fits a 64 byte cache line on a 64-bit target
inline constexpr size_t FUNCTION_INLINE_TARGET_SIZE = 48;

namespace details
{
    // Fundamental alignment only: raising it to the new-expression alignment pads the union on MSVC (C4324),
    // and an over-aligned target is exactly what the heap tier's aligned allocation is for
    union function_storage
    {
        void* heap;
        alignas(std::max_align_t) std::byte inlined[FUNCTION_INLINE_TARGET_SIZE];
    };

    // A throwing move would defeat the noexcept move of the wrapper itself, so such a target goes to the heap
    // where moving is a pointer steal
    template<typename T>
    inline constexpr bool function_target_fits_inline = sizeof(T) <= FUNCTION_INLINE_TARGET_SIZE && alignof(T) <= alignof(function_storage) && std::is_nothrow_move_constructible_v<T>;

    template<typename T>
    inline constexpr bool is_function_wrapper_v = false;
    template<typename Signature>
    inline constexpr bool is_function_wrapper_v<move_only_function<Signature>> = true;
    template<typename Signature>
    inline constexpr bool is_function_wrapper_v<copyable_function<Signature>> = true;

    // copy is populated only for a target adopted through copyable_function; move_only_function leaves it null
    struct function_vtable
    {
        void (*destroy)(function_storage& storage) noexcept;
        void (*move)(function_storage& to, function_storage& from) noexcept;
        void (*copy)(function_storage& to, const function_storage& from);
        bool heap_allocated;
    };

    // This module sits above SmartPointers and MemorySystem in the Essentials order, so safe_alloc is not
    // reachable; the globally replaced operator new still routes to the engine allocator and OOM exits
    [[nodiscard]] inline auto function_alloc_target(size_t size, size_t alignment) noexcept -> void*
    {
        void* mem;

        if (alignment > __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
            mem = ::operator new(size, std::align_val_t {alignment}, std::nothrow);
        }
        else {
            mem = ::operator new(size, std::nothrow);
        }

        if (mem == nullptr) {
            report_fatal_and_exit("Failed to allocate function target");
        }

        return mem;
    }

    inline void function_free_target(void* mem, size_t alignment) noexcept
    {
        if (alignment > __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
            ::operator delete(mem, std::align_val_t {alignment});
        }
        else {
            ::operator delete(mem);
        }
    }

    // Frees the block only while the target constructor has not taken it over yet
    class function_target_block
    {
    public:
        function_target_block(size_t size, size_t alignment) noexcept :
            _mem(function_alloc_target(size, alignment)),
            _alignment(alignment)
        {
        }
        function_target_block(const function_target_block&) = delete;
        function_target_block(function_target_block&&) = delete;
        auto operator=(const function_target_block&) -> function_target_block& = delete;
        auto operator=(function_target_block&&) -> function_target_block& = delete;
        ~function_target_block() noexcept
        {
            if (_mem != nullptr) {
                function_free_target(_mem, _alignment);
            }
        }

        [[nodiscard]] auto memory() const noexcept -> void* { return _mem; }
        void release() noexcept { _mem = nullptr; }

    private:
        void* _mem;
        size_t _alignment;
    };

    template<typename T, bool Inlined>
    [[nodiscard]] FO_FORCE_INLINE auto function_target(const function_storage& storage) noexcept -> T*
    {
        // The stored target is mutable through a const wrapper, matching how a callback is normally held by
        // const reference and still invoked
        if constexpr (Inlined) {
            return std::launder(reinterpret_cast<T*>(const_cast<std::byte*>(storage.inlined)));
        }
        else {
            return static_cast<T*>(storage.heap);
        }
    }

    template<typename T, bool Inlined>
    void function_destroy_target(function_storage& storage) noexcept
    {
        T* target = function_target<T, Inlined>(storage);
        std::destroy_at(target);

        if constexpr (!Inlined) {
            function_free_target(storage.heap, alignof(T));
        }
    }

    template<typename T, bool Inlined>
    void function_move_target(function_storage& to, function_storage& from) noexcept
    {
        if constexpr (Inlined) {
            T* source = function_target<T, Inlined>(from);
            ::new (static_cast<void*>(to.inlined)) T(std::move(*source));
            std::destroy_at(source);
        }
        else {
            to.heap = from.heap;
            from.heap = nullptr;
        }
    }

    template<typename T, bool Inlined>
    void function_copy_target(function_storage& to, const function_storage& from)
    {
        const T* source = function_target<T, Inlined>(from);

        if constexpr (Inlined) {
            ::new (static_cast<void*>(to.inlined)) T(*source);
        }
        else {
            function_target_block block(sizeof(T), alignof(T));
            to.heap = ::new (block.memory()) T(*source);
            block.release();
        }
    }

    template<typename T, bool Inlined, typename R, typename... Args>
    auto function_invoke_target(const function_storage& storage, Args&&... args) -> R
    {
        T* target = function_target<T, Inlined>(storage);

        if constexpr (std::is_void_v<R>) {
            std::invoke(*target, std::forward<Args>(args)...);
        }
        else {
            return std::invoke(*target, std::forward<Args>(args)...);
        }
    }

    template<typename T, bool Inlined, bool Copyable>
    [[nodiscard]] constexpr auto function_make_vtable() noexcept -> function_vtable
    {
        if constexpr (Copyable) {
            return function_vtable {&function_destroy_target<T, Inlined>, &function_move_target<T, Inlined>, &function_copy_target<T, Inlined>, !Inlined};
        }
        else {
            return function_vtable {&function_destroy_target<T, Inlined>, &function_move_target<T, Inlined>, nullptr, !Inlined};
        }
    }

    template<typename T, bool Inlined, bool Copyable>
    inline constexpr function_vtable function_vtable_of = function_make_vtable<T, Inlined, Copyable>();
}

// Matches std::move_only_function: the target is owned by exactly one wrapper, so it may capture unique
// owners, and a small target lives inside the wrapper instead of on the heap
template<typename R, typename... Args>
class move_only_function<R(Args...)> final
{
    friend class copyable_function<R(Args...)>;

public:
    using result_type = R;

    move_only_function() noexcept { }
    // ReSharper disable once CppNonExplicitConvertingConstructor
    move_only_function(std::nullptr_t) noexcept { }

    template<typename T>
        requires(!details::is_function_wrapper_v<std::decay_t<T>> && std::is_invocable_r_v<R, std::decay_t<T>&, Args...>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    move_only_function(T&& target)
    {
        adopt_target<false>(std::forward<T>(target));
    }

    move_only_function(move_only_function&& other) noexcept { steal_from(other); }
    auto operator=(move_only_function&& other) noexcept -> move_only_function&
    {
        if (this != &other) {
            release_target();
            steal_from(other);
        }

        return *this;
    }

    move_only_function(const move_only_function&) = delete;
    auto operator=(const move_only_function&) -> move_only_function& = delete;

    // Assigning a bare callable takes the template rather than converting through one of the wrapper
    // overloads below, which would otherwise be an ambiguous pair of user-defined conversions
    template<typename T>
        requires(!details::is_function_wrapper_v<std::decay_t<T>> && std::is_invocable_r_v<R, std::decay_t<T>&, Args...>)
    auto operator=(T&& target) -> move_only_function&
    {
        move_only_function replacement(std::forward<T>(target));
        release_target();
        steal_from(replacement);
        return *this;
    }

    // A copyable wrapper narrows to the move-only one by adopting its target in place, never by wrapping it
    // ReSharper disable once CppNonExplicitConvertingConstructor
    move_only_function(copyable_function<R(Args...)>&& other) noexcept { steal_from(other); }
    // ReSharper disable once CppNonExplicitConvertingConstructor
    move_only_function(const copyable_function<R(Args...)>& other) { clone_from(other); }
    auto operator=(copyable_function<R(Args...)>&& other) noexcept -> move_only_function&
    {
        release_target();
        steal_from(other);
        return *this;
    }
    auto operator=(const copyable_function<R(Args...)>& other) -> move_only_function&
    {
        move_only_function copy(other);
        release_target();
        steal_from(copy);
        return *this;
    }

    auto operator=(std::nullptr_t) noexcept -> move_only_function&
    {
        release_target();
        return *this;
    }

    ~move_only_function() noexcept { release_target(); }

    [[nodiscard]] explicit operator bool() const noexcept { return _vtable != nullptr; }
    [[nodiscard]] auto is_heap_allocated() const noexcept -> bool { return _vtable != nullptr && _vtable->heap_allocated; }

    auto operator()(Args... args) const -> R
    {
        FO_BASIC_STRONG_ASSERT(_vtable != nullptr);

        return _invoke(_storage, std::forward<Args>(args)...);
    }

    void reset() noexcept { release_target(); }

    void swap(move_only_function& other) noexcept
    {
        move_only_function tmp = std::move(other);
        other = std::move(*this);
        *this = std::move(tmp);
    }

private:
    using invoke_type = R (*)(const details::function_storage&, Args&&...);

    template<bool Copyable, typename T>
    void adopt_target(T&& target)
    {
        using target_type = std::decay_t<T>;

        // A null function or member pointer produces an empty wrapper, so a call hits the assert instead of
        // jumping through the null target
        if constexpr (std::is_pointer_v<target_type> || std::is_member_pointer_v<target_type>) {
            if (target == nullptr) {
                return;
            }
        }

        constexpr bool inlined = details::function_target_fits_inline<target_type>;

        if constexpr (inlined) {
            ::new (static_cast<void*>(_storage.inlined)) target_type(std::forward<T>(target));
        }
        else {
            details::function_target_block block(sizeof(target_type), alignof(target_type));
            _storage.heap = ::new (block.memory()) target_type(std::forward<T>(target));
            block.release();
        }

        _vtable = &details::function_vtable_of<target_type, inlined, Copyable>;
        _invoke = &details::function_invoke_target<target_type, inlined, R, Args...>;
    }

    template<typename Other>
    void steal_from(Other& other) noexcept
    {
        _vtable = other._vtable;

        if (_vtable != nullptr) {
            _invoke = other._invoke;
            _vtable->move(_storage, other._storage);
            other._vtable = nullptr;
        }
    }

    template<typename Other>
    void clone_from(const Other& other)
    {
        if (other._vtable != nullptr) {
            other._vtable->copy(_storage, other._storage);
            _vtable = other._vtable;
            _invoke = other._invoke;
        }
    }

    void release_target() noexcept
    {
        if (_vtable != nullptr) {
            _vtable->destroy(_storage);
            _vtable = nullptr;
        }
    }

    details::function_storage _storage;
    const details::function_vtable* _vtable {};
    invoke_type _invoke {};
};

// Matches std::copyable_function: a copy duplicates the target, so the two wrappers stay independent, and
// the same inline budget keeps a small target off the heap
template<typename R, typename... Args>
class copyable_function<R(Args...)> final
{
    friend class move_only_function<R(Args...)>;

public:
    using result_type = R;

    copyable_function() noexcept { }
    // ReSharper disable once CppNonExplicitConvertingConstructor
    copyable_function(std::nullptr_t) noexcept { }

    template<typename T>
        requires(!details::is_function_wrapper_v<std::decay_t<T>> && std::is_invocable_r_v<R, std::decay_t<T>&, Args...> && std::is_copy_constructible_v<std::decay_t<T>>)
    // ReSharper disable once CppNonExplicitConvertingConstructor
    copyable_function(T&& target)
    {
        adopt_target<true>(std::forward<T>(target));
    }

    template<typename T>
        requires(!details::is_function_wrapper_v<std::decay_t<T>> && std::is_invocable_r_v<R, std::decay_t<T>&, Args...> && std::is_copy_constructible_v<std::decay_t<T>>)
    auto operator=(T&& target) -> copyable_function&
    {
        copyable_function replacement(std::forward<T>(target));
        release_target();
        steal_from(replacement);
        return *this;
    }

    copyable_function(const copyable_function& other) { clone_from(other); }
    auto operator=(const copyable_function& other) -> copyable_function&
    {
        if (this != &other) {
            copyable_function copy(other);
            release_target();
            steal_from(copy);
        }

        return *this;
    }

    copyable_function(copyable_function&& other) noexcept { steal_from(other); }
    auto operator=(copyable_function&& other) noexcept -> copyable_function&
    {
        if (this != &other) {
            release_target();
            steal_from(other);
        }

        return *this;
    }

    auto operator=(std::nullptr_t) noexcept -> copyable_function&
    {
        release_target();
        return *this;
    }

    ~copyable_function() noexcept { release_target(); }

    [[nodiscard]] explicit operator bool() const noexcept { return _vtable != nullptr; }
    [[nodiscard]] auto is_heap_allocated() const noexcept -> bool { return _vtable != nullptr && _vtable->heap_allocated; }

    auto operator()(Args... args) const -> R
    {
        FO_BASIC_STRONG_ASSERT(_vtable != nullptr);

        return _invoke(_storage, std::forward<Args>(args)...);
    }

    void reset() noexcept { release_target(); }

    void swap(copyable_function& other) noexcept
    {
        copyable_function tmp = std::move(other);
        other = std::move(*this);
        *this = std::move(tmp);
    }

private:
    using invoke_type = R (*)(const details::function_storage&, Args&&...);

    template<bool Copyable, typename T>
    void adopt_target(T&& target)
    {
        using target_type = std::decay_t<T>;

        if constexpr (std::is_pointer_v<target_type> || std::is_member_pointer_v<target_type>) {
            if (target == nullptr) {
                return;
            }
        }

        constexpr bool inlined = details::function_target_fits_inline<target_type>;

        if constexpr (inlined) {
            ::new (static_cast<void*>(_storage.inlined)) target_type(std::forward<T>(target));
        }
        else {
            details::function_target_block block(sizeof(target_type), alignof(target_type));
            _storage.heap = ::new (block.memory()) target_type(std::forward<T>(target));
            block.release();
        }

        _vtable = &details::function_vtable_of<target_type, inlined, Copyable>;
        _invoke = &details::function_invoke_target<target_type, inlined, R, Args...>;
    }

    template<typename Other>
    void steal_from(Other& other) noexcept
    {
        _vtable = other._vtable;

        if (_vtable != nullptr) {
            _invoke = other._invoke;
            _vtable->move(_storage, other._storage);
            other._vtable = nullptr;
        }
    }

    template<typename Other>
    void clone_from(const Other& other)
    {
        if (other._vtable != nullptr) {
            other._vtable->copy(_storage, other._storage);
            _vtable = other._vtable;
            _invoke = other._invoke;
        }
    }

    void release_target() noexcept
    {
        if (_vtable != nullptr) {
            _vtable->destroy(_storage);
            _vtable = nullptr;
        }
    }

    details::function_storage _storage;
    const details::function_vtable* _vtable {};
    invoke_type _invoke {};
};

// Default engine callable: unique ownership, so a captured owner is never silently duplicated
template<typename Signature>
using function = move_only_function<Signature>;

static_assert(sizeof(move_only_function<void()>) == FUNCTION_INLINE_TARGET_SIZE + 2 * sizeof(void*));
static_assert(sizeof(copyable_function<void()>) == sizeof(move_only_function<void()>));
static_assert(!std::is_copy_constructible_v<move_only_function<void()>>);
static_assert(!std::is_copy_assignable_v<move_only_function<void()>>);
static_assert(std::is_nothrow_move_constructible_v<move_only_function<void()>>);
static_assert(std::is_copy_constructible_v<copyable_function<void()>>);
static_assert(std::is_nothrow_move_constructible_v<copyable_function<void()>>);

FO_END_NAMESPACE
