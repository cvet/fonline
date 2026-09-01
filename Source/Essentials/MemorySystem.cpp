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

#include "MemorySystem.h"
#include "BaseLogging.h"
#include "GlobalData.h"
#include "StackTrace.h"

// Only the non-rpmalloc Windows path needs the CRT aligned allocation entry points
#if !FO_HAVE_RPMALLOC && FO_WINDOWS
#include <malloc.h>
#endif

FO_BEGIN_NAMESPACE

struct memory_system_data
{
    memory_system_data() { init_backup_memory_chunks(); }

    bad_alloc_callback callback {};
};
FO_GLOBAL_DATA(memory_system_data, memory_system);

// Unpoliced primitives that only report failure by returning null, kept out of the public header so
// every caller goes through safe_alloc and its out-of-memory contract
static auto mem_malloc(size_t size) noexcept -> nptr<void>;
static auto mem_calloc(size_t num, size_t size) noexcept -> nptr<void>;
static auto mem_realloc(nptr<void> ptr, size_t size) noexcept -> nptr<void>;
static void mem_free(nptr<void> ptr) noexcept;
static auto mem_aligned_malloc(size_t size, size_t alignment) noexcept -> nptr<void>;
static void mem_aligned_free(nptr<void> ptr) noexcept;

static constexpr size_t BACKUP_MEMORY_CHUNKS = 100;
static constexpr size_t BACKUP_MEMORY_CHUNK_SIZE = 100000; // 100 chunks x 100kb = 10mb
static unique_arr_ptr<unique_arr_ptr<uint8_t>> backup_memory_chunks;
static std::atomic_size_t backup_memory_chunks_count;

// Replace memory allocator
#if FO_HAVE_RPMALLOC

FO_END_NAMESPACE

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
    tracy_free(p);
    tracy::rpfree(p);
#else
    rpfree(p);
#endif
}

extern void CRTDECL operator delete[](void* p) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_TRACY
    tracy_free(p);
    tracy::rpfree(p);
#else
    rpfree(p);
#endif
}

extern void* CRTDECL operator new(std::size_t size) noexcept(false)
{
    FO_NO_STACK_TRACE_ENTRY();

    void* p = nullptr;
#if FO_TRACY
    tracy::init_rpmalloc();
    p = tracy::rpmalloc(size);
    tracy_alloc(p, size);
#else
    p = rpmalloc(size);
#endif
    if (p == nullptr) {
        throw std::bad_alloc();
    }
    return p;
}

extern void* CRTDECL operator new[](std::size_t size) noexcept(false)
{
    FO_NO_STACK_TRACE_ENTRY();

    void* p = nullptr;
#if FO_TRACY
    tracy::init_rpmalloc();
    p = tracy::rpmalloc(size);
    tracy_alloc(p, size);
#else
    p = rpmalloc(size);
#endif
    if (p == nullptr) {
        throw std::bad_alloc();
    }
    return p;
}

extern void* CRTDECL operator new(std::size_t size, const std::nothrow_t& /*tag*/) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    void* p = nullptr;
#if FO_TRACY
    tracy::init_rpmalloc();
    p = tracy::rpmalloc(size);
    tracy_alloc(p, size);
#else
    p = rpmalloc(size);
#endif
    return p;
}

extern void* CRTDECL operator new[](std::size_t size, const std::nothrow_t& /*tag*/) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    void* p = nullptr;
#if FO_TRACY
    tracy::init_rpmalloc();
    p = tracy::rpmalloc(size);
    tracy_alloc(p, size);
#else
    p = rpmalloc(size);
#endif
    return p;
}

extern void CRTDECL operator delete(void* p, std::size_t /*size*/) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_TRACY
    tracy_free(p);
    tracy::rpfree(p);
#else
    rpfree(p);
#endif
}

extern void CRTDECL operator delete[](void* p, std::size_t /*size*/) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_TRACY
    tracy_free(p);
    tracy::rpfree(p);
#else
    rpfree(p);
#endif
}

extern void CRTDECL operator delete(void* p, std::align_val_t /*align*/) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_TRACY
    tracy_free(p);
    tracy::rpfree(p);
#else
    rpfree(p);
#endif
}

extern void CRTDECL operator delete[](void* p, std::align_val_t /*align*/) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_TRACY
    tracy_free(p);
    tracy::rpfree(p);
#else
    rpfree(p);
#endif
}

extern void CRTDECL operator delete(void* p, std::size_t /*size*/, std::align_val_t /*align*/) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_TRACY
    tracy_free(p);
    tracy::rpfree(p);
#else
    rpfree(p);
#endif
}

extern void CRTDECL operator delete[](void* p, std::size_t /*size*/, std::align_val_t /*align*/) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_TRACY
    tracy_free(p);
    tracy::rpfree(p);
#else
    rpfree(p);
#endif
}

extern void* CRTDECL operator new(std::size_t size, std::align_val_t align) noexcept(false)
{
    FO_NO_STACK_TRACE_ENTRY();

    void* p = nullptr;
#if FO_TRACY
    tracy::init_rpmalloc();
    p = tracy::rpaligned_alloc(static_cast<size_t>(align), size);
    tracy_alloc(p, size);
#else
    p = rpaligned_alloc(static_cast<size_t>(align), size);
#endif
    if (p == nullptr) {
        throw std::bad_alloc();
    }
    return p;
}

extern void* CRTDECL operator new[](std::size_t size, std::align_val_t align) noexcept(false)
{
    FO_NO_STACK_TRACE_ENTRY();

    void* p = nullptr;
#if FO_TRACY
    tracy::init_rpmalloc();
    p = tracy::rpaligned_alloc(static_cast<size_t>(align), size);
    tracy_alloc(p, size);
#else
    p = rpaligned_alloc(static_cast<size_t>(align), size);
#endif
    if (p == nullptr) {
        throw std::bad_alloc();
    }
    return p;
}

extern void* CRTDECL operator new(std::size_t size, std::align_val_t align, const std::nothrow_t& /*tag*/) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    void* p = nullptr;
#if FO_TRACY
    tracy::init_rpmalloc();
    p = tracy::rpaligned_alloc(static_cast<size_t>(align), size);
    tracy_alloc(p, size);
#else
    p = rpaligned_alloc(static_cast<size_t>(align), size);
#endif
    return p;
}

extern void* CRTDECL operator new[](std::size_t size, std::align_val_t align, const std::nothrow_t& /*tag*/) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    void* p = nullptr;
#if FO_TRACY
    tracy::init_rpmalloc();
    p = tracy::rpaligned_alloc(static_cast<size_t>(align), size);
    tracy_alloc(p, size);
#else
    p = rpaligned_alloc(static_cast<size_t>(align), size);
#endif
    return p;
}

#undef CRTDECL
FO_BEGIN_NAMESPACE

#endif

auto safe_alloc::malloc_raw(size_t size) noexcept -> nptr<void>
{
    FO_NO_STACK_TRACE_ENTRY();

    nptr<void> mem = mem_malloc(size);

    if (!mem && size != 0) {
        report_bad_alloc("Raw malloc failed", "byte", 1, size);

        while (!mem && free_backup_memory_chunk()) {
            mem = mem_malloc(size);
        }

        if (!mem) {
            report_and_exit("Failed to allocate raw from backup pool");
        }
    }

    return mem;
}

auto safe_alloc::calloc_raw(size_t num, size_t size) noexcept -> nptr<void>
{
    FO_NO_STACK_TRACE_ENTRY();

    if (size != 0 && num > std::numeric_limits<size_t>::max() / size) {
        report_bad_alloc("Raw calloc size overflow", "byte", num, size);
        report_and_exit("Raw calloc size overflow");
    }

    nptr<void> mem = mem_calloc(num, size);

    if (!mem && num != 0 && size != 0) {
        report_bad_alloc("Raw calloc failed", "byte", num, size);

        while (!mem && free_backup_memory_chunk()) {
            mem = mem_calloc(num, size);
        }

        if (!mem) {
            report_and_exit("Failed to allocate raw zeroed from backup pool");
        }
    }

    return mem;
}

auto safe_alloc::realloc_raw(nptr<void> ptr, size_t size) noexcept -> nptr<void>
{
    FO_NO_STACK_TRACE_ENTRY();

    nptr<void> mem = mem_realloc(ptr, size);

    if (!mem && size != 0) {
        report_bad_alloc("Raw realloc failed", "byte", 1, size);

        while (!mem && free_backup_memory_chunk()) {
            mem = mem_realloc(ptr, size);
        }

        if (!mem) {
            report_and_exit("Failed to reallocate raw from backup pool");
        }
    }

    return mem;
}

void safe_alloc::free_raw(nptr<void> ptr) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    mem_free(ptr);
}

auto safe_alloc::malloc_aligned_raw(size_t size, size_t alignment) noexcept -> nptr<void>
{
    FO_NO_STACK_TRACE_ENTRY();

    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        report_bad_alloc("Raw aligned malloc received invalid alignment", "byte", alignment, size);
        report_and_exit("Raw aligned allocation alignment is invalid");
    }

    nptr<void> mem = mem_aligned_malloc(size, alignment);

    if (!mem && size != 0) {
        report_bad_alloc("Raw aligned malloc failed", "byte", alignment, size);

        while (!mem && free_backup_memory_chunk()) {
            mem = mem_aligned_malloc(size, alignment);
        }

        if (!mem) {
            report_and_exit("Failed to allocate raw aligned from backup pool");
        }
    }

    return mem;
}

void safe_alloc::free_aligned_raw(nptr<void> ptr) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    mem_aligned_free(ptr);
}

static auto mem_malloc(size_t size) noexcept -> nptr<void>
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_HAVE_RPMALLOC && FO_TRACY
    tracy::init_rpmalloc();
    void* p = tracy::rpmalloc(size);
    tracy_alloc(p, size);
    return p;
#elif FO_HAVE_RPMALLOC && !FO_TRACY
    return rpmalloc(size);
#else
    return malloc(size);
#endif
}

static auto mem_calloc(size_t num, size_t size) noexcept -> nptr<void>
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_HAVE_RPMALLOC && FO_TRACY
    tracy::init_rpmalloc();
    const auto result_size = num * size;
    if (num != 0 && size != 0 && result_size / num != size) {
        return nullptr; // Overflow
    }
    void* p = tracy::rpmalloc(result_size);
    if (p != nullptr) {
        mem_fill(p, 0, result_size);
    }
    tracy_alloc(p, result_size);
    return p;
#elif FO_HAVE_RPMALLOC && !FO_TRACY
    return rpcalloc(num, size);
#else
    return calloc(num, size);
#endif
}

static auto mem_realloc(nptr<void> ptr, size_t size) noexcept -> nptr<void>
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_HAVE_RPMALLOC && FO_TRACY
    tracy::init_rpmalloc();
    tracy_free(ptr.get());
    void* p = tracy::rprealloc(ptr.get(), size);
    tracy_alloc(p, size);
    return p;
#elif FO_HAVE_RPMALLOC && !FO_TRACY
    return rprealloc(ptr.get(), size);
#else
    return realloc(ptr.get(), size);
#endif
}

static void mem_free(nptr<void> ptr) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_HAVE_RPMALLOC && FO_TRACY
    tracy_free(ptr.get());
    tracy::rpfree(ptr.get());
#elif FO_HAVE_RPMALLOC && !FO_TRACY
    rpfree(ptr.get());
#else
    free(ptr.get());
#endif
}

static auto mem_aligned_malloc(size_t size, size_t alignment) noexcept -> nptr<void>
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_HAVE_RPMALLOC && FO_TRACY
    tracy::init_rpmalloc();
    void* p = tracy::rpaligned_alloc(alignment, size);
    tracy_alloc(p, size);
    return p;
#elif FO_HAVE_RPMALLOC && !FO_TRACY
    return rpaligned_alloc(alignment, size);
#elif FO_WINDOWS
    return _aligned_malloc(size, alignment);
#else
    void* p = nullptr;
    // posix_memalign rejects alignments below sizeof(void*); over-aligning is always safe
    size_t effective_alignment = std::max(alignment, sizeof(void*));

    if (::posix_memalign(&p, effective_alignment, size) != 0) {
        return nullptr;
    }

    return p;
#endif
}

static void mem_aligned_free(nptr<void> ptr) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_HAVE_RPMALLOC && FO_TRACY
    tracy_free(ptr.get());
    tracy::rpfree(ptr.get());
#elif FO_HAVE_RPMALLOC && !FO_TRACY
    rpfree(ptr.get());
#elif FO_WINDOWS
    _aligned_free(ptr.get());
#else
    free(ptr.get());
#endif
}

extern auto allocator_get_in_use_bytes() noexcept -> size_t
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_HAVE_RPMALLOC && (FO_DEBUG || FO_TRACY)
#if FO_TRACY
    tracy::rpmalloc_global_statistics_t stats {};
    tracy::rpmalloc_global_statistics(&stats);

    size_t mapped = stats.mapped;
    size_t cached = stats.cached;

    if (mapped >= cached) {
        return mapped - cached;
    }
    return mapped;
#else
    rpmalloc_global_statistics_t stats {};
    ::rpmalloc_global_statistics(&stats);

    return stats.active;
#endif

#else
    return 0;
#endif
}

extern void init_backup_memory_chunks()
{
    FO_STACK_TRACE_ENTRY();

    unique_arr_ptr<unique_arr_ptr<uint8_t>> new_chunks {new unique_arr_ptr<uint8_t>[BACKUP_MEMORY_CHUNKS]()};

    for (size_t i = 0; i < BACKUP_MEMORY_CHUNKS; i++) {
        new_chunks[i] = unique_arr_ptr<uint8_t> {new uint8_t[BACKUP_MEMORY_CHUNK_SIZE]()};
    }

    backup_memory_chunks = std::move(new_chunks);
    backup_memory_chunks_count.store(BACKUP_MEMORY_CHUNKS);
}

extern auto free_backup_memory_chunk() noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    while (true) {
        size_t cur_size = backup_memory_chunks_count.load();

        if (cur_size == 0) {
            return false;
        }

        if (backup_memory_chunks_count.compare_exchange_strong(cur_size, cur_size - 1)) {
            backup_memory_chunks[cur_size - 1].reset();
            return true;
        }
    }
}

void set_bad_alloc_callback(bad_alloc_callback callback) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    memory_system->callback = std::move(callback);
}

extern void report_bad_alloc(string_view message, string_view type_str, size_t count, size_t size) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    break_into_debugger();

    char itoa_buf[64] = {};

    write_base_log("\nBAD ALLOC!\n\n");
    write_base_log(message);
    write_base_log("\n");
    write_base_log("Type: ");
    write_base_log(type_str);
    write_base_log("\n");
    write_base_log("Count: ");
    write_base_log(itoa(static_cast<int64_t>(count), itoa_buf, 10));
    write_base_log("\n");
    write_base_log("Size: ");
    write_base_log(itoa(static_cast<int64_t>(size), itoa_buf, 10));
    write_base_log("\n\n");
    safe_write_stack_trace(get_stack_trace());

    if (memory_system->callback) {
        memory_system->callback();
    }
}

extern void report_and_exit(string_view message) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    write_base_log(message);

    exit_app(false);
}

FO_END_NAMESPACE
