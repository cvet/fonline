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

#include "MemorySystem.h"
#include "BaseLogging.h"
#include "GlobalData.h"
#include "StackTrace.h"

FO_BEGIN_NAMESPACE();

struct MemorySystemData
{
    MemorySystemData() { InitBackupMemoryChunks(); }

    BadAllocCallback Callback {};
};
FO_GLOBAL_DATA(MemorySystemData, MemorySystem);

static constexpr size_t BACKUP_MEMORY_CHUNKS = 100;
static constexpr size_t BACKUP_MEMORY_CHUNK_SIZE = 100000; // 100 chunks x 100kb = 10mb
static std::unique_ptr<std::unique_ptr<uint8[]>[]> BackupMemoryChunks;
static std::atomic_size_t BackupMemoryChunksCount;

// Replace memory allocator
#if FO_HAVE_RPMALLOC

FO_END_NAMESPACE();

#if FO_TRACY
#include "client/tracy_rpmalloc.hpp"
#else
#include "rpmalloc.h"
#endif

#include <new>

#if FO_WINDOWS
#define CRTDECL __CRTDECL
#else
#define CRTDECL
#endif

extern void CRTDECL operator delete(void* p) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_TRACY
    TracyFree(p);
    tracy::rpfree(p);
#else
    rpfree(p);
#endif
}

extern void CRTDECL operator delete[](void* p) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_TRACY
    TracyFree(p);
    tracy::rpfree(p);
#else
    rpfree(p);
#endif
}

extern void* CRTDECL operator new(std::size_t size) noexcept(false)
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_TRACY
    tracy::InitRpmalloc();
    auto* p = tracy::rpmalloc(size);
    TracyAlloc(p, size);
#else
    auto* p = rpmalloc(size);
#endif
    if (p == nullptr) {
        throw std::bad_alloc();
    }
    return p;
}

extern void* CRTDECL operator new[](std::size_t size) noexcept(false)
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_TRACY
    tracy::InitRpmalloc();
    auto* p = tracy::rpmalloc(size);
    TracyAlloc(p, size);
#else
    auto* p = rpmalloc(size);
#endif
    if (p == nullptr) {
        throw std::bad_alloc();
    }
    return p;
}

extern void* CRTDECL operator new(std::size_t size, const std::nothrow_t& /*tag*/) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_TRACY
    tracy::InitRpmalloc();
    auto* p = tracy::rpmalloc(size);
    TracyAlloc(p, size);
#else
    auto* p = rpmalloc(size);
#endif
    return p;
}

extern void* CRTDECL operator new[](std::size_t size, const std::nothrow_t& /*tag*/) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_TRACY
    tracy::InitRpmalloc();
    auto* p = tracy::rpmalloc(size);
    TracyAlloc(p, size);
#else
    auto* p = rpmalloc(size);
#endif
    return p;
}

extern void CRTDECL operator delete(void* p, std::size_t /*size*/) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_TRACY
    TracyFree(p);
    tracy::rpfree(p);
#else
    rpfree(p);
#endif
}

extern void CRTDECL operator delete[](void* p, std::size_t /*size*/) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_TRACY
    TracyFree(p);
    tracy::rpfree(p);
#else
    rpfree(p);
#endif
}

extern void CRTDECL operator delete(void* p, std::align_val_t /*align*/) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_TRACY
    TracyFree(p);
    tracy::rpfree(p);
#else
    rpfree(p);
#endif
}

extern void CRTDECL operator delete[](void* p, std::align_val_t /*align*/) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_TRACY
    TracyFree(p);
    tracy::rpfree(p);
#else
    rpfree(p);
#endif
}

extern void CRTDECL operator delete(void* p, std::size_t /*size*/, std::align_val_t /*align*/) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_TRACY
    TracyFree(p);
    tracy::rpfree(p);
#else
    rpfree(p);
#endif
}

extern void CRTDECL operator delete[](void* p, std::size_t /*size*/, std::align_val_t /*align*/) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_TRACY
    TracyFree(p);
    tracy::rpfree(p);
#else
    rpfree(p);
#endif
}

extern void* CRTDECL operator new(std::size_t size, std::align_val_t align) noexcept(false)
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_TRACY
    tracy::InitRpmalloc();
    auto* p = tracy::rpaligned_alloc(static_cast<size_t>(align), size);
    TracyAlloc(p, size);
#else
    auto* p = rpaligned_alloc(static_cast<size_t>(align), size);
#endif
    if (p == nullptr) {
        throw std::bad_alloc();
    }
    return p;
}

extern void* CRTDECL operator new[](std::size_t size, std::align_val_t align) noexcept(false)
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_TRACY
    tracy::InitRpmalloc();
    auto* p = tracy::rpaligned_alloc(static_cast<size_t>(align), size);
    TracyAlloc(p, size);
#else
    auto* p = rpaligned_alloc(static_cast<size_t>(align), size);
#endif
    if (p == nullptr) {
        throw std::bad_alloc();
    }
    return p;
}

extern void* CRTDECL operator new(std::size_t size, std::align_val_t align, const std::nothrow_t& /*tag*/) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_TRACY
    tracy::InitRpmalloc();
    auto* p = tracy::rpaligned_alloc(static_cast<size_t>(align), size);
    TracyAlloc(p, size);
#else
    auto* p = rpaligned_alloc(static_cast<size_t>(align), size);
#endif
    return p;
}

extern void* CRTDECL operator new[](std::size_t size, std::align_val_t align, const std::nothrow_t& /*tag*/) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_TRACY
    tracy::InitRpmalloc();
    auto* p = tracy::rpaligned_alloc(static_cast<size_t>(align), size);
    TracyAlloc(p, size);
#else
    auto* p = rpaligned_alloc(static_cast<size_t>(align), size);
#endif
    return p;
}

#undef CRTDECL
FO_BEGIN_NAMESPACE();

#endif

extern auto MemMalloc(size_t size) noexcept -> void*
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_HAVE_RPMALLOC && FO_TRACY
    tracy::InitRpmalloc();
    void* p = tracy::rpmalloc(size);
    TracyAlloc(p, size);
    return p;
#elif FO_HAVE_RPMALLOC && !FO_TRACY
    return rpmalloc(size);
#else
    return malloc(size);
#endif
}

extern auto MemCalloc(size_t num, size_t size) noexcept -> void*
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_HAVE_RPMALLOC && FO_TRACY
    tracy::InitRpmalloc();
    const auto result_size = num * size;
    if (num != 0 && size != 0 && result_size / num != size) {
        return nullptr; // Overflow
    }
    void* p = tracy::rpmalloc(result_size);
    if (p != nullptr) {
        MemFill(p, 0, result_size);
    }
    TracyAlloc(p, result_size);
    return p;
#elif FO_HAVE_RPMALLOC && !FO_TRACY
    return rpcalloc(num, size);
#else
    return calloc(num, size);
#endif
}

extern auto MemRealloc(void* ptr, size_t size) noexcept -> void*
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_HAVE_RPMALLOC && FO_TRACY
    tracy::InitRpmalloc();
    TracyFree(ptr);
    void* p = tracy::rprealloc(ptr, size);
    TracyAlloc(p, size);
    return p;
#elif FO_HAVE_RPMALLOC && !FO_TRACY
    return rprealloc(ptr, size);
#else
    return realloc(ptr, size);
#endif
}

extern void MemFree(void* ptr) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_HAVE_RPMALLOC && FO_TRACY
    TracyFree(ptr);
    tracy::rpfree(ptr);
#elif FO_HAVE_RPMALLOC && !FO_TRACY
    rpfree(ptr);
#else
    free(ptr);
#endif
}

extern void MemCopy(void* dest, const void* src, size_t size) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    // Standard: If either dest or src is an invalid or null pointer, the behavior is undefined, even if count is zero
    // So check size first
    if (size != 0) {
        std::memcpy(dest, src, size);
    }
}

void MemMove(void* dest, const void* src, size_t size) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (size != 0) {
        std::memmove(dest, src, size);
    }
}

extern void MemFill(void* ptr, int32 value, size_t size) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (size != 0) {
        std::memset(ptr, value, size);
    }
}

extern auto MemCompare(const void* ptr1, const void* ptr2, size_t size) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (size != 0) {
        return std::memcmp(ptr1, ptr2, size) == 0;
    }

    return true;
}

extern void InitBackupMemoryChunks()
{
    FO_STACK_TRACE_ENTRY();

    BackupMemoryChunks = std::make_unique<std::unique_ptr<uint8[]>[]>(BACKUP_MEMORY_CHUNKS);

    for (size_t i = 0; i < BACKUP_MEMORY_CHUNKS; i++) {
        BackupMemoryChunks[i] = std::make_unique<uint8[]>(BACKUP_MEMORY_CHUNK_SIZE);
    }

    BackupMemoryChunksCount.store(BACKUP_MEMORY_CHUNKS);
}

extern auto FreeBackupMemoryChunk() noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    while (true) {
        size_t cur_size = BackupMemoryChunksCount.load();

        if (cur_size == 0) {
            return false;
        }

        if (BackupMemoryChunksCount.compare_exchange_strong(cur_size, cur_size - 1)) {
            BackupMemoryChunks[cur_size].reset();
            return true;
        }
    }
}

void SetBadAllocCallback(BadAllocCallback callback) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    MemorySystem->Callback = std::move(callback);
}

extern void ReportBadAlloc(string_view message, string_view type_str, size_t count, size_t size) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    BreakIntoDebugger();

    char itoa_buf[64] = {};

    WriteBaseLog("\nBAD ALLOC!\n\n");
    WriteBaseLog(message);
    WriteBaseLog("\n");
    WriteBaseLog("Type: ");
    WriteBaseLog(type_str);
    WriteBaseLog("\n");
    WriteBaseLog("Count: ");
    WriteBaseLog(ItoA(static_cast<int64>(count), itoa_buf, 10));
    WriteBaseLog("\n");
    WriteBaseLog("Size: ");
    WriteBaseLog(ItoA(static_cast<int64>(size), itoa_buf, 10));
    WriteBaseLog("\n\n");
    SafeWriteStackTrace(GetStackTrace());

    if (MemorySystem->Callback) {
        MemorySystem->Callback();
    }
}

extern void ReportAndExit(string_view message) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    WriteBaseLog(message);

    ExitApp(false);
}

FO_END_NAMESPACE();
