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
#include "SmartPointers.h"

FO_BEGIN_NAMESPACE();

#if !FO_HAVE_RPMALLOC
static_assert(__STDCPP_DEFAULT_NEW_ALIGNMENT__ >= alignof(std::max_align_t));
#endif

// Safe memory allocation
using BadAllocCallback = function<void()>;

extern void InitBackupMemoryChunks();
extern auto FreeBackupMemoryChunk() noexcept -> bool;
extern void SetBadAllocCallback(BadAllocCallback callback) noexcept;
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

    // ReSharper disable once CppInconsistentNaming
    [[nodiscard]] auto allocate(size_t count) const noexcept -> T*
    {
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
        requires(!is_refcounted<T>)
    static auto MakeRaw(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) -> T*
    {
        auto* ptr = new (std::nothrow) T(std::forward<Args>(args)...);

        if (ptr == nullptr) {
            ReportBadAlloc("Make raw failed", typeid(T).name(), 1, sizeof(T));

            while (ptr == nullptr && FreeBackupMemoryChunk()) {
                ptr = new (std::nothrow) T(std::forward<Args>(args)...); // Todo: fix use after possible move
            }

            if (ptr == nullptr) {
                ReportAndExit("Failed to allocate raw from backup pool");
            }
        }

        return ptr;
    }

    template<typename T, typename... Args>
        requires(!is_refcounted<T>)
    static auto MakeUnique(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) -> unique_ptr<T>
    {
        return unique_ptr<T>(MakeRaw<T>(std::forward<Args>(args)...));
    }

    template<typename T, typename... Args>
        requires(is_refcounted<T>)
    static auto MakeRefCounted(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) -> refcount_ptr<T>
    {
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

        return refcount_ptr<T>(refcount_ptr<T>::adopt, ptr);
    }

    template<typename T, typename... Args>
        requires(!is_refcounted<T>)
    static auto MakeShared(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) -> shared_ptr<T>
    {
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
                catch (const std::bad_alloc&) { // NOLINT(bugprone-empty-catch)
                    // Release next block and try again
                }
            }
        }
    }

    template<typename T>
        requires(!is_refcounted<T>)
    static auto MakeRawArr(size_t count) noexcept(std::is_nothrow_default_constructible_v<T>) -> T*
    {
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
        requires(!is_refcounted<T>)
    static auto MakeUniqueArr(size_t count) noexcept(std::is_nothrow_default_constructible_v<T>) -> unique_arr_ptr<T>
    {
        return unique_arr_ptr<T>(MakeRawArr<T>(count));
    }
};

// Memory low level management
extern auto MemMalloc(size_t size) noexcept -> void*;
extern auto MemCalloc(size_t num, size_t size) noexcept -> void*;
extern auto MemRealloc(void* ptr, size_t size) noexcept -> void*;
extern void MemFree(void* ptr) noexcept;
extern void MemCopy(void* dest, const void* src, size_t size) noexcept;
extern void MemMove(void* dest, const void* src, size_t size) noexcept;
extern void MemFill(void* ptr, int32 value, size_t size) noexcept;
extern auto MemCompare(const void* ptr1, const void* ptr2, size_t size) noexcept -> bool;

FO_END_NAMESPACE();
