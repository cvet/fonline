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
#include "SmartPointers.h"

FO_BEGIN_NAMESPACE

#if !FO_HAVE_RPMALLOC
static_assert(__STDCPP_DEFAULT_NEW_ALIGNMENT__ >= MAX_ALIGNMENT);
#endif

// Safe memory allocation
using BadAllocCallback = function<void()>;

extern void InitBackupMemoryChunks();
extern auto FreeBackupMemoryChunk() noexcept -> bool;
extern void SetBadAllocCallback(BadAllocCallback callback) noexcept;
extern void ReportBadAlloc(string_view message, string_view type_str, size_t count, size_t size) noexcept;
[[noreturn]] extern void ReportAndExit(string_view message) noexcept;
extern auto AllocatorGetInUseBytes() noexcept -> size_t;

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

    // ReSharper disable once CppInconsistentNaming
    [[nodiscard]] auto allocate(size_t count) const noexcept -> T*
    {
        if (count > static_cast<size_t>(-1) / sizeof(T)) {
            ReportBadAlloc("Safe allocator bad size", typeid(T).name(), count, count * sizeof(T));
            ReportAndExit("Alloc size overflow");
        }

        nptr<void> ptr = ::operator new(sizeof(T) * count, std::nothrow);

        if (!ptr) {
            ReportBadAlloc("Safe allocator failed", typeid(T).name(), count, count * sizeof(T));

            while (!ptr && FreeBackupMemoryChunk()) {
                ptr = ::operator new(sizeof(T) * count, std::nothrow);
            }

            if (!ptr) {
                ReportAndExit("Failed to allocate from backup pool");
            }
        }

        return cast_from_void<T*>(ptr.get());
    }

    // ReSharper disable once CppInconsistentNaming
    void deallocate(T* ptr, size_t count) const noexcept
    {
        (void)count;
        FO_GCC_IGNORE_WARNINGS_PUSH("-Wfree-nonheap-object")
        ::operator delete(ptr);
        FO_GCC_IGNORE_WARNINGS_POP()
    }
};

class SafeAlloc
{
public:
    SafeAlloc() = delete;

    template<typename T, typename... Args>
        requires(!refcountable<T>)
    static auto MakeRaw(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) -> T*
    {
        auto alloc = [&]() { return nptr<T>(new (std::nothrow) T(std::forward<Args>(args)...)); };
        auto ptr = AllocWithBackupRetry<T>(alloc, "Make raw failed", "Failed to allocate raw from backup pool", 1, sizeof(T));
        return ptr.get();
    }

    template<typename T, typename... Args>
        requires(!refcountable<T>)
    static auto MakeUnique(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) -> unique_ptr<T>
    {
        return unique_ptr<T>(MakeRaw<T>(std::forward<Args>(args)...));
    }

    template<typename T, typename... Args>
        requires(refcountable<T>)
    static auto MakeRefCounted(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) -> refcount_ptr<T>
    {
        auto alloc = [&]() { return nptr<T>(new (std::nothrow) T(std::forward<Args>(args)...)); };
        auto ptr = AllocWithBackupRetry<T>(alloc, "Make ref counted failed", "Failed to allocate ref counted from backup pool", 1, sizeof(T));
        return refcount_ptr<T>::from_adopted_ref(ptr.get());
    }

    template<typename T, typename... Args>
        requires(!refcountable<T>)
    static auto MakeShared(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) -> shared_ptr<T>
    {
        auto alloc = [&]() { return nptr<shared_ptr_storage_block<T>>(new (std::nothrow) shared_ptr_storage_block<T>(std::forward<Args>(args)...)); };
        auto block = AllocWithBackupRetry<shared_ptr_storage_block<T>>(alloc, "Make shared ptr failed", "Failed to allocate shared ptr from backup pool", 1, sizeof(T));

        nptr<T> obj = block->stored_object();
        shared_ptr<T> result = shared_ptr<T>(block.get(), obj.get());
        init_shared_from_this_weak(result, obj.get());
        return result;
    }

    template<typename T>
        requires(!refcountable<T>)
    static auto MakeRawArr(size_t count) noexcept(std::is_nothrow_default_constructible_v<T>) -> T*
    {
        if (count > static_cast<size_t>(-1) / sizeof(T)) {
            ReportBadAlloc("Make raw array bad size", typeid(T).name(), count, count * sizeof(T));
            ReportAndExit("Alloc size overflow");
        }

        auto alloc = [&]() { return nptr<T>(new (std::nothrow) T[count]()); };
        auto ptr = AllocWithBackupRetry<T>(alloc, "Make raw array failed", "Failed to allocate from backup pool", count, count * sizeof(T));
        return ptr.get();
    }

    template<typename T>
        requires(!refcountable<T>)
    static auto MakeUniqueArr(size_t count) noexcept(std::is_nothrow_default_constructible_v<T>) -> unique_arr_ptr<T>
    {
        return unique_arr_ptr<T>(MakeRawArr<T>(count));
    }

private:
    template<typename T, typename AllocFunc>
    static auto AllocWithBackupRetry(AllocFunc&& alloc, const char* alloc_desc, const char* exhausted_desc, size_t count, size_t size) -> ptr<T>
    {
        nptr<T> ptr = alloc();

        if (!ptr) {
            ReportBadAlloc(alloc_desc, typeid(T).name(), count, size);

            while (!ptr && FreeBackupMemoryChunk()) {
                ptr = alloc();
            }

            if (!ptr) {
                ReportAndExit(exhausted_desc);
            }
        }

        return ptr.as_ptr();
    }
};

// Memory low level management.
extern auto MemMalloc(size_t size) noexcept -> nptr<void>;
extern auto MemCalloc(size_t num, size_t size) noexcept -> nptr<void>;
extern auto MemRealloc(nptr<void> ptr, size_t size) noexcept -> nptr<void>;
extern void MemFree(nptr<void> ptr) noexcept;

inline void MemCopy(nptr<void> dest, nptr<const void> src, size_t size) noexcept
{
    // Standard: If either dest or src is an invalid or null pointer, the behavior is undefined, even if count is zero.
    // So check size first.
    if (size != 0) {
        std::memcpy(dest.get(), src.get(), size);
    }
}

inline void MemMove(nptr<void> dest, nptr<const void> src, size_t size) noexcept
{
    if (size != 0) {
        std::memmove(dest.get(), src.get(), size);
    }
}

inline void MemFill(nptr<void> ptr, int32_t value, size_t size) noexcept
{
    if (size != 0) {
        std::memset(ptr.get(), value, size);
    }
}

inline auto MemCompare(nptr<const void> ptr1, nptr<const void> ptr2, size_t size) noexcept -> bool
{
    return size == 0 || std::memcmp(ptr1.get(), ptr2.get(), size) == 0;
}

template<typename T>
inline auto MemReadUnaligned(nptr<const void> src) noexcept -> T
{
    static_assert(std::is_trivially_copyable_v<T>);
    T value;
    std::memcpy(&value, src.get(), sizeof(T));
    return value;
}

template<typename T>
inline void MemWriteUnaligned(nptr<void> dest, const T& value) noexcept
{
    static_assert(std::is_trivially_copyable_v<T>);
    std::memcpy(dest.get(), &value, sizeof(T));
}

FO_END_NAMESPACE
