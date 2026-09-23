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

// Script entries keep a register context instead of an eager unwind wherever native frames are walked: libunwind on Linux
// and macOS, the function tables of 64-bit Windows, the frame pointer chain of 32-bit Windows
#if (FO_LINUX || FO_MAC || FO_WINDOWS) && !FO_MEMORY_SANITIZER && !FO_THREAD_SANITIZER
#define FO_STACK_TRACE_RESUME_CONTEXT 1
#else
#define FO_STACK_TRACE_RESUME_CONTEXT 0
#endif

FO_BEGIN_NAMESPACE

namespace stack_trace
{
    inline constexpr size_t MAX_NATIVE_FRAMES = 128;
    inline constexpr size_t RESUME_CONTEXT_WORDS = 72;
    inline constexpr size_t RESOLVE_CACHE_MAX_ENTRIES = 4096;

    using native_frame_address = uintptr_t;

    struct frame
    {
        enum class frame_type : uint8_t
        {
            native,
            script,
        };

        frame_type type {frame_type::native};
        std::string function {};
        std::string file {};
        uint32_t line {};
    };

    struct script_layer
    {
        std::vector<frame> script_frames {};
        std::array<native_frame_address, MAX_NATIVE_FRAMES> birth_native_frames {};
        uint32_t birth_native_frame_count {};
        bool birth_native_truncated {};
        // Addresses inside code the script runtime generated (JIT output), which script_frames already describe
        std::vector<native_frame_address> runtime_native_frames {};
    };

    struct data
    {
        std::array<native_frame_address, MAX_NATIVE_FRAMES> native_frames {};
        uint32_t native_frame_count {};
        bool native_truncated {};
        // The first frame is the statement that asked for the trace, shown as the function it stands in rather than as
        // the code inlined there on the way to the capture
        bool native_head_is_request {};
        std::shared_ptr<const std::vector<script_layer>> script_layers {};
    };

    struct catched_data
    {
        optional<data> origin {};
        data catched {};
    };

    // Where a native frame entered script code, kept so a report can list the native frames below that entry. With a
    // saved register context the frames are unwound only when a report asks for them
    struct resume_point
    {
#if FO_STACK_TRACE_RESUME_CONTEXT
        alignas(16) std::array<uint64_t, RESUME_CONTEXT_WORDS> context;
#else
        std::array<native_frame_address, MAX_NATIVE_FRAMES> frames;
        uint32_t frame_count;
        bool truncated;
#endif
    };

    // A provider appends its layers innermost first and may inspect the native frames already captured for the same trace
    using script_provider = std::function<void(const data& st, std::vector<script_layer>& out_layers)>;

    void set_script_provider(std::string_view name, script_provider provider) noexcept;
    auto has_script_provider(std::string_view name) noexcept -> bool;
    auto get() noexcept -> data;
    // The stack of a crash: a POSIX signal's ucontext_t, or a Windows exception CONTEXT and the thread that raised it
    // when the walk runs on another one. The faulting instruction is the first frame
    auto get_from_context(const void* os_context, void* os_thread) noexcept -> data;
    void add_unwound_script_frames(data& st, script_layer layer);
    void splice_caught_script_frames(data& st, script_layer layer);
    // Starts at the caller and leaves out the next skip frames; every frame but a faulting one is recorded at its call
    void capture_native_frames(std::array<native_frame_address, MAX_NATIVE_FRAMES>& out_frames, uint32_t& out_count, bool& out_truncated, uint32_t skip = 0) noexcept;
    void capture_native_frames_from_context(const void* os_context, void* os_thread, std::array<native_frame_address, MAX_NATIVE_FRAMES>& out_frames, uint32_t& out_count, bool& out_truncated) noexcept;
    // Records the registers of the calling frame, which must stay active until the point is resolved
#if FO_STACK_TRACE_RESUME_CONTEXT && FO_MAC
    extern "C" int save_resume_point(resume_point* point) noexcept __asm__("_unw_getcontext");
#elif FO_STACK_TRACE_RESUME_CONTEXT && !FO_WINDOWS
    extern "C" int save_resume_point(resume_point* point) noexcept __asm__("unw_getcontext");
#else
    auto save_resume_point(resume_point* point) noexcept -> int;
#endif
    void resolve_resume_point(const resume_point& point, std::array<native_frame_address, MAX_NATIVE_FRAMES>& out_frames, uint32_t& out_count, bool& out_truncated) noexcept;
    void clear_resolved_cache() noexcept;
    auto get_resolved_cache_size() noexcept -> size_t;
    auto resolve(const data& st) -> std::vector<frame>;
    auto get_entry(uint32_t deep) noexcept -> std::optional<frame>;
    auto format(const data& st) -> std::string;
    auto format(const catched_data& st) -> std::string;
}

#if FO_TRACY
#define FO_STACK_TRACE_ENTRY() ZoneScoped
#define FO_STACK_TRACE_ENTRY_NAMED(name) ZoneScopedN(name)
#else
#define FO_STACK_TRACE_ENTRY()
#define FO_STACK_TRACE_ENTRY_NAMED(name)
#endif
#define FO_NO_STACK_TRACE_ENTRY()

FO_END_NAMESPACE
