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
#include "SmartPointers.h"

FO_BEGIN_NAMESPACE

#if !FO_HAVE_RPMALLOC
// Rpmalloc guarantee 16 byte alignment
static_assert(__STDCPP_DEFAULT_NEW_ALIGNMENT__ >= MAX_SERIALIZED_ALIGNMENT);
#endif

// Safe memory allocation
using bad_alloc_callback = function<void()>;

extern void init_backup_memory_chunks();
extern auto free_backup_memory_chunk() noexcept -> bool;
extern void set_bad_alloc_callback(bad_alloc_callback callback) noexcept;
extern void report_bad_alloc(string_view message, string_view type_str, size_t count, size_t size) noexcept;
[[noreturn]] extern void report_and_exit(string_view message) noexcept;
extern auto allocator_get_in_use_bytes() noexcept -> size_t;

template<typename T>
class safe_allocator
{
public:
    using value_type = T;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_copy_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;
    using is_always_equal = std::true_type;

    safe_allocator() noexcept = default;
    template<typename U>
    // ReSharper disable once CppNonExplicitConvertingConstructor
    constexpr safe_allocator(const safe_allocator<U>& other) noexcept
    {
        (void)other;
    }
    template<typename U>
    [[nodiscard]] auto operator==(const safe_allocator<U>& other) const noexcept -> bool
    {
        (void)other;
        return true;
    }

    [[nodiscard]] auto allocate(size_t count) const noexcept -> T*
    {
        if (count > static_cast<size_t>(-1) / sizeof(T)) {
            report_bad_alloc("Safe allocator bad size", typeid(T).name(), count, count * sizeof(T));
            report_and_exit("Alloc size overflow");
        }

        size_t size = sizeof(T) * count;
        nptr<void> mem = allocate_raw(size);

        if (!mem) {
            report_bad_alloc("Safe allocator failed", typeid(T).name(), count, size);

            while (!mem && free_backup_memory_chunk()) {
                mem = allocate_raw(size);
            }

            if (!mem) {
                report_and_exit("Failed to allocate from backup pool");
            }
        }

        auto typed_mem = mem.reinterpret_as<T>();
        return typed_mem.get();
    }

    // Size is not passed to the sized delete overload on purpose: callers such as the ImGui and zlib free
    // callbacks receive no size from their library and must pass a placeholder count
    void deallocate(T* ptr, size_t count) const noexcept
    {
        (void)count;
        deallocate_raw(ptr);
    }

private:
    // An over-aligned element must take the aligned new/delete overloads. A function rather than a
    // constexpr member: alignof(T) needs a complete T, the allocator must accept an incomplete one
    static constexpr auto is_over_aligned() noexcept -> bool { return alignof(T) > __STDCPP_DEFAULT_NEW_ALIGNMENT__; }

    static auto allocate_raw(size_t size) noexcept -> nptr<void>
    {
        if constexpr (is_over_aligned()) {
            return ::operator new(size, std::align_val_t {alignof(T)}, std::nothrow);
        }
        else {
            return ::operator new(size, std::nothrow);
        }
    }

    static void deallocate_raw(T* ptr) noexcept
    {
        FO_GCC_IGNORE_WARNINGS_PUSH("-Wfree-nonheap-object")

        if constexpr (is_over_aligned()) {
            ::operator delete(ptr, std::align_val_t {alignof(T)});
        }
        else {
            ::operator delete(ptr);
        }

        FO_GCC_IGNORE_WARNINGS_POP()
    }
};

class safe_alloc
{
public:
    safe_alloc() = delete;

    template<typename T, typename... Args>
        requires(!refcountable<T>)
    static auto make_raw(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) -> ptr<T>
    {
        auto alloc = [&]() { return nptr<T>(new (std::nothrow) T(std::forward<Args>(args)...)); };
        return alloc_with_backup_retry<T>(alloc, "Make raw failed", "Failed to allocate raw from backup pool", 1, sizeof(T));
    }

    template<typename T, typename... Args>
        requires(!refcountable<T>)
    static auto make_unique(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) -> unique_ptr<T>
    {
        auto alloc = [&]() { return nptr<T>(new (std::nothrow) T(std::forward<Args>(args)...)); };
        auto ptr = alloc_with_backup_retry<T>(alloc, "Make unique failed", "Failed to allocate unique from backup pool", 1, sizeof(T));
        return adopt_unique_ptr(ptr);
    }

    template<typename T, typename... Args>
        requires(refcountable<T>)
    static auto make_refcounted(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) -> refcount_ptr<T>
    {
        auto alloc = [&]() { return nptr<T>(new (std::nothrow) T(std::forward<Args>(args)...)); };
        auto ptr = alloc_with_backup_retry<T>(alloc, "Make ref counted failed", "Failed to allocate ref counted from backup pool", 1, sizeof(T));
        return refcount_ptr<T>::from_adopted_ref(ptr.get());
    }

    template<typename T, typename... Args>
        requires(!refcountable<T>)
    static auto make_shared(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) -> shared_ptr<T>
    {
        auto alloc = [&]() { return nptr<shared_ptr_storage_block<T>>(new (std::nothrow) shared_ptr_storage_block<T>(std::forward<Args>(args)...)); };
        auto block = alloc_with_backup_retry<shared_ptr_storage_block<T>>(alloc, "Make shared ptr failed", "Failed to allocate shared ptr from backup pool", 1, sizeof(T));

        nptr<T> obj = block->stored_object();
        shared_ptr<T> result = shared_ptr<T>(block.get(), obj.get());
        init_shared_from_this_weak(result, obj.get());
        return result;
    }

    template<typename T>
        requires(!refcountable<T>)
    static auto make_raw_arr(size_t count) noexcept(std::is_nothrow_default_constructible_v<T>) -> T*
    {
        if (count > static_cast<size_t>(-1) / sizeof(T)) {
            report_bad_alloc("Make raw array bad size", typeid(T).name(), count, count * sizeof(T));
            report_and_exit("Alloc raw size overflow");
        }

        auto alloc = [&]() { return nptr<T>(new (std::nothrow) T[count]()); };
        auto ptr = alloc_with_backup_retry<T>(alloc, "Make raw array failed", "Failed to allocate raw from backup pool", count, count * sizeof(T));
        return ptr.get();
    }

    // The only entry point for third-party hooks that demand realloc or an untyped byte block (SDL,
    // spine, curl); it keeps the safe_allocator out-of-memory policy, and a zero size passes through
    [[nodiscard]] static auto malloc_raw(size_t size) noexcept -> nptr<void>;
    [[nodiscard]] static auto calloc_raw(size_t num, size_t size) noexcept -> nptr<void>;
    [[nodiscard]] static auto realloc_raw(nptr<void> ptr, size_t size) noexcept -> nptr<void>;
    static void free_raw(nptr<void> ptr) noexcept;

    // Aligned counterparts. Freeing needs no alignment argument, which is what lets these back library
    // callbacks that hand back only the pointer (Effekseer's AlignedFreeFunc)
    [[nodiscard]] static auto malloc_aligned_raw(size_t size, size_t alignment) noexcept -> nptr<void>;
    static void free_aligned_raw(nptr<void> ptr) noexcept;

    template<typename T>
        requires(!refcountable<T>)
    static auto make_unique_arr(size_t count) noexcept(std::is_nothrow_default_constructible_v<T>) -> unique_arr_ptr<T>
    {
        if (count > static_cast<size_t>(-1) / sizeof(T)) {
            report_bad_alloc("Make unique array bad size", typeid(T).name(), count, count * sizeof(T));
            report_and_exit("Alloc unique size overflow");
        }

        auto alloc = [&]() { return nptr<T>(new (std::nothrow) T[count]()); };
        auto ptr = alloc_with_backup_retry<T>(alloc, "Make unique array failed", "Failed to allocate unique from backup pool", count, count * sizeof(T));
        return unique_arr_ptr<T>(ptr.get());
    }

private:
    template<typename T, typename AllocFunc>
    static auto alloc_with_backup_retry(AllocFunc&& alloc, string_view alloc_desc, string_view exhausted_desc, size_t count, size_t size) -> ptr<T>
    {
        nptr<T> ptr = alloc();

        if (!ptr) {
            report_bad_alloc(alloc_desc, typeid(T).name(), count, size);

            while (!ptr && free_backup_memory_chunk()) {
                ptr = alloc();
            }

            if (!ptr) {
                report_and_exit(exhausted_desc);
            }
        }

        return ptr;
    }
};

// Memory block operations
inline void mem_copy(nptr<void> dest, nptr<const void> src, size_t size) noexcept
{
    // Standard: If either dest or src is an invalid or null pointer, the behavior is undefined, even if count is zero.
    // So check size first
    if (size != 0) {
        std::memcpy(dest.get(), src.get(), size);
    }
}

inline void mem_move(nptr<void> dest, nptr<const void> src, size_t size) noexcept
{
    if (size != 0) {
        std::memmove(dest.get(), src.get(), size);
    }
}

inline void mem_fill(nptr<void> ptr, int32_t value, size_t size) noexcept
{
    if (size != 0) {
        std::memset(ptr.get(), value, size);
    }
}

inline auto mem_compare(nptr<const void> ptr1, nptr<const void> ptr2, size_t size) noexcept -> bool
{
    return size == 0 || std::memcmp(ptr1.get(), ptr2.get(), size) == 0;
}

template<typename T>
inline auto mem_read_unaligned(nptr<const void> src) noexcept -> T
{
    static_assert(std::is_trivially_copyable_v<T>);
    T value;
    std::memcpy(&value, src.get(), sizeof(T));
    return value;
}

template<typename T>
inline void mem_write_unaligned(nptr<void> dest, const T& value) noexcept
{
    static_assert(std::is_trivially_copyable_v<T>);
    std::memcpy(dest.get(), &value, sizeof(T));
}

FO_END_NAMESPACE
