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

struct memory_system_data;
static void fill_backup_memory_chunks(memory_system_data& data);

struct memory_system_data
{
    memory_system_data() { fill_backup_memory_chunks(*this); }

    memory::bad_alloc_callback callback {};
    unique_arr_ptr<unique_arr_ptr<uint8_t>> backup_memory_chunks {};
    std::atomic_size_t backup_memory_chunks_count {};
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

// Replace memory allocator
#if FO_HAVE_RPMALLOC

FO_END_NAMESPACE

// The profiler headers bring Tracy's private rpmalloc copy, whose GCC attribute macros are spelled differently from the
// engine rpmalloc the allocator is declared with
#if FO_TRACE_ENABLED
#undef RPMALLOC_ATTRIB_MALLOC
#undef RPMALLOC_ATTRIB_ALLOC_SIZE
#undef RPMALLOC_ATTRIB_ALLOC_SIZE2
#endif

#include "rpmalloc.h"

#include <new>

#if FO_WINDOWS
#define CRTDECL __CRTDECL
#else
#define CRTDECL
#endif

// Profiling builds allocate through the same rpmalloc as release builds and only account on top of it: each thread counts
// its own allocations for measurements that must not see other threads, and the opt-in Memory category reports every block
#if FO_TRACE_ENABLED
static thread_local uint64_t ThreadAllocationCount {};
static thread_local uint64_t ThreadAllocatedBytes {};
#endif

static void TrackAlloc(void* p, size_t size) noexcept
{
#if FO_TRACE_CATEGORY_ENABLED(Memory)
    TracyAlloc(p, size);
#else
    FO_NAMESPACE ignore_unused(p);
#endif
#if FO_TRACE_ENABLED
    ThreadAllocationCount++;
    ThreadAllocatedBytes += size;
#else
    FO_NAMESPACE ignore_unused(size);
#endif
}

static void TrackFree(void* p) noexcept
{
#if FO_TRACE_CATEGORY_ENABLED(Memory)
    TracyFree(p);
#else
    FO_NAMESPACE ignore_unused(p);
#endif
}

void CRTDECL operator delete(void* p) noexcept
{
    TrackFree(p);
    rpfree(p);
}

void CRTDECL operator delete[](void* p) noexcept
{
    TrackFree(p);
    rpfree(p);
}

void* CRTDECL operator new(std::size_t size) noexcept(false)
{
    void* p = rpmalloc(size);
    TrackAlloc(p, size);

    if (p == nullptr) {
        throw std::bad_alloc();
    }

    return p;
}

void* CRTDECL operator new[](std::size_t size) noexcept(false)
{
    void* p = rpmalloc(size);
    TrackAlloc(p, size);

    if (p == nullptr) {
        throw std::bad_alloc();
    }

    return p;
}

void* CRTDECL operator new(std::size_t size, const std::nothrow_t& /*tag*/) noexcept
{
    void* p = rpmalloc(size);
    TrackAlloc(p, size);
    return p;
}

void* CRTDECL operator new[](std::size_t size, const std::nothrow_t& /*tag*/) noexcept
{
    void* p = rpmalloc(size);
    TrackAlloc(p, size);
    return p;
}

void CRTDECL operator delete(void* p, std::size_t /*size*/) noexcept
{
    TrackFree(p);
    rpfree(p);
}

void CRTDECL operator delete[](void* p, std::size_t /*size*/) noexcept
{
    TrackFree(p);
    rpfree(p);
}

void CRTDECL operator delete(void* p, std::align_val_t /*align*/) noexcept
{
    TrackFree(p);
    rpfree(p);
}

void CRTDECL operator delete[](void* p, std::align_val_t /*align*/) noexcept
{
    TrackFree(p);
    rpfree(p);
}

void CRTDECL operator delete(void* p, std::size_t /*size*/, std::align_val_t /*align*/) noexcept
{
    TrackFree(p);
    rpfree(p);
}

void CRTDECL operator delete[](void* p, std::size_t /*size*/, std::align_val_t /*align*/) noexcept
{
    TrackFree(p);
    rpfree(p);
}

void* CRTDECL operator new(std::size_t size, std::align_val_t align) noexcept(false)
{
    void* p = rpaligned_alloc(static_cast<size_t>(align), size);
    TrackAlloc(p, size);

    if (p == nullptr) {
        throw std::bad_alloc();
    }

    return p;
}

void* CRTDECL operator new[](std::size_t size, std::align_val_t align) noexcept(false)
{
    void* p = rpaligned_alloc(static_cast<size_t>(align), size);
    TrackAlloc(p, size);

    if (p == nullptr) {
        throw std::bad_alloc();
    }

    return p;
}

void* CRTDECL operator new(std::size_t size, std::align_val_t align, const std::nothrow_t& /*tag*/) noexcept
{
    void* p = rpaligned_alloc(static_cast<size_t>(align), size);
    TrackAlloc(p, size);
    return p;
}

void* CRTDECL operator new[](std::size_t size, std::align_val_t align, const std::nothrow_t& /*tag*/) noexcept
{
    void* p = rpaligned_alloc(static_cast<size_t>(align), size);
    TrackAlloc(p, size);
    return p;
}

#undef CRTDECL
FO_BEGIN_NAMESPACE

#endif

auto safe_alloc::malloc_raw(size_t size) noexcept -> nptr<void>
{
    nptr<void> mem = mem_malloc(size);

    if (!mem && size != 0) {
        memory::report_bad_alloc("Raw malloc failed", "byte", 1, size);

        while (!mem && memory::free_backup_chunk()) {
            mem = mem_malloc(size);
        }

        if (!mem) {
            memory::report_and_exit("Failed to allocate raw from backup pool");
        }
    }

    return mem;
}

auto safe_alloc::calloc_raw(size_t num, size_t size) noexcept -> nptr<void>
{
    if (size != 0 && num > std::numeric_limits<size_t>::max() / size) {
        memory::report_bad_alloc("Raw calloc size overflow", "byte", num, size);
        memory::report_and_exit("Raw calloc size overflow");
    }

    nptr<void> mem = mem_calloc(num, size);

    if (!mem && num != 0 && size != 0) {
        memory::report_bad_alloc("Raw calloc failed", "byte", num, size);

        while (!mem && memory::free_backup_chunk()) {
            mem = mem_calloc(num, size);
        }

        if (!mem) {
            memory::report_and_exit("Failed to allocate raw zeroed from backup pool");
        }
    }

    return mem;
}

auto safe_alloc::realloc_raw(nptr<void> ptr, size_t size) noexcept -> nptr<void>
{
    nptr<void> mem = mem_realloc(ptr, size);

    if (!mem && size != 0) {
        memory::report_bad_alloc("Raw realloc failed", "byte", 1, size);

        while (!mem && memory::free_backup_chunk()) {
            mem = mem_realloc(ptr, size);
        }

        if (!mem) {
            memory::report_and_exit("Failed to reallocate raw from backup pool");
        }
    }

    return mem;
}

void safe_alloc::free_raw(nptr<void> ptr) noexcept
{
    mem_free(ptr);
}

auto safe_alloc::malloc_aligned_raw(size_t size, size_t alignment) noexcept -> nptr<void>
{
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        memory::report_bad_alloc("Raw aligned malloc received invalid alignment", "byte", alignment, size);
        memory::report_and_exit("Raw aligned allocation alignment is invalid");
    }

    nptr<void> mem = mem_aligned_malloc(size, alignment);

    if (!mem && size != 0) {
        memory::report_bad_alloc("Raw aligned malloc failed", "byte", alignment, size);

        while (!mem && memory::free_backup_chunk()) {
            mem = mem_aligned_malloc(size, alignment);
        }

        if (!mem) {
            memory::report_and_exit("Failed to allocate raw aligned from backup pool");
        }
    }

    return mem;
}

void safe_alloc::free_aligned_raw(nptr<void> ptr) noexcept
{
    mem_aligned_free(ptr);
}

static auto mem_malloc(size_t size) noexcept -> nptr<void>
{
#if FO_HAVE_RPMALLOC
    void* p = rpmalloc(size);
    TrackAlloc(p, size);
    return p;
#else
    return malloc(size);
#endif
}

static auto mem_calloc(size_t num, size_t size) noexcept -> nptr<void>
{
#if FO_HAVE_RPMALLOC
    void* p = rpcalloc(num, size);

    // A null result may come from an overflowing product, which is no size to account
    if (p != nullptr) {
        TrackAlloc(p, num * size);
    }

    return p;
#else
    return calloc(num, size);
#endif
}

static auto mem_realloc(nptr<void> ptr, size_t size) noexcept -> nptr<void>
{
#if FO_HAVE_RPMALLOC
    TrackFree(ptr.get());
    void* p = rprealloc(ptr.get(), size);
    TrackAlloc(p, size);
    return p;
#else
    return realloc(ptr.get(), size);
#endif
}

static void mem_free(nptr<void> ptr) noexcept
{
#if FO_HAVE_RPMALLOC
    TrackFree(ptr.get());
    rpfree(ptr.get());
#else
    free(ptr.get());
#endif
}

static auto mem_aligned_malloc(size_t size, size_t alignment) noexcept -> nptr<void>
{
#if FO_HAVE_RPMALLOC
    void* p = rpaligned_alloc(alignment, size);
    TrackAlloc(p, size);
    return p;
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
#if FO_HAVE_RPMALLOC
    TrackFree(ptr.get());
    rpfree(ptr.get());
#elif FO_WINDOWS
    _aligned_free(ptr.get());
#else
    free(ptr.get());
#endif
}

auto memory::get_thread_allocations(uint64_t& count, uint64_t& bytes) noexcept -> bool
{
#if FO_HAVE_RPMALLOC && FO_TRACE_ENABLED
    count = ThreadAllocationCount;
    bytes = ThreadAllocatedBytes;
    return true;
#else
    count = 0;
    bytes = 0;
    return false;
#endif
}

auto memory::get_in_use_bytes() noexcept -> size_t
{
#if FO_HAVE_RPMALLOC && (FO_DEBUG || FO_TRACE_ENABLED)
    rpmalloc_global_statistics_t stats {};
    ::rpmalloc_global_statistics(&stats);

    return stats.active;
#else
    return 0;
#endif
}

void memory::init_backup_chunks()
{
    fill_backup_memory_chunks(*memory_system);
}

auto memory::free_backup_chunk() noexcept -> bool
{
    // Out of memory after the set is torn down has no reserve left to give back
    if (!memory_system.is_created()) {
        return false;
    }

    while (true) {
        size_t cur_size = memory_system->backup_memory_chunks_count.load();

        if (cur_size == 0) {
            return false;
        }

        if (memory_system->backup_memory_chunks_count.compare_exchange_strong(cur_size, cur_size - 1)) {
            memory_system->backup_memory_chunks[cur_size - 1].reset();
            return true;
        }
    }
}

void memory::set_bad_alloc_callback(memory::bad_alloc_callback callback) noexcept
{
    memory_system->callback = std::move(callback);
}

void memory::report_bad_alloc(string_view message, string_view type_str, size_t count, size_t size) noexcept
{
    break_into_debugger();

    char itoa_buf[64] = {};

    logging::write_base("\nBAD ALLOC!\n\n");
    logging::write_base(message);
    logging::write_base("\n");
    logging::write_base("Type: ");
    logging::write_base(type_str);
    logging::write_base("\n");
    logging::write_base("Count: ");
    logging::write_base(itoa(static_cast<int64_t>(count), itoa_buf, 10));
    logging::write_base("\n");
    logging::write_base("Size: ");
    logging::write_base(itoa(static_cast<int64_t>(size), itoa_buf, 10));
    logging::write_base("\n\n");
    logging::safe_write_stack_trace(stack_trace::get());

    if (memory_system.is_created() && memory_system->callback) {
        memory_system->callback();
    }
}

void memory::report_and_exit(string_view message) noexcept
{
    logging::write_base(message);

    exit_app(false);
}

static void fill_backup_memory_chunks(memory_system_data& data)
{
    FO_TRACE_ZONE(Core);

    unique_arr_ptr<unique_arr_ptr<uint8_t>> new_chunks {new unique_arr_ptr<uint8_t>[BACKUP_MEMORY_CHUNKS]()};

    for (size_t i = 0; i < BACKUP_MEMORY_CHUNKS; i++) {
        new_chunks[i] = unique_arr_ptr<uint8_t> {new uint8_t[BACKUP_MEMORY_CHUNK_SIZE]()};
    }

    data.backup_memory_chunks = std::move(new_chunks);
    data.backup_memory_chunks_count.store(BACKUP_MEMORY_CHUNKS);
}

FO_END_NAMESPACE
