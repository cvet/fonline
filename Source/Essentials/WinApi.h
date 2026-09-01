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
#include "Containers.h"
#include "SmartPointers.h"

#if FO_WINDOWS

FO_BEGIN_NAMESPACE

// The Windows API the engine calls, and the only place its header is included. <Windows.h> defines macros over
// hundreds of ordinary words, so every file that pulls it in has to undo them again through WinApiUndef.inc
namespace winapi
{
    struct cpu_core_times
    {
        uint64_t idle_time {};
        uint64_t total_time {};
    };

    void output_debug_string(const string& text) noexcept;

    // Absent before Windows 10 1607, so the entry point is resolved at call time and absence is not an error
    auto set_thread_description(const string& name) noexcept -> bool;

    auto get_current_process_id() noexcept -> uint32_t;
    auto get_module_file_name() noexcept -> optional<string>;
    auto get_process_working_set_size() noexcept -> size_t;
    auto get_process_private_usage() noexcept -> size_t;
    auto get_process_cpu_time_ns() noexcept -> optional<uint64_t>;
    auto get_active_processor_count() noexcept -> uint32_t;
    auto get_system_cpu_times() noexcept -> optional<cpu_core_times>;

    auto load_library(const string& path) noexcept -> nptr<void>;
    void free_library(nptr<void> module_handle) noexcept;
    // A null module handle means the running executable, which is where the loader search starts
    auto get_proc_address(nptr<void> module_handle, const string& func_name) noexcept -> nptr<void>;

    // A file this process holds alone: a second opener is refused rather than allowed to share it. The
    // descriptor is the platform's own, and -1 means the open failed
    auto open_exclusive_file(const string& path) noexcept -> int32_t;
    void close_exclusive_file(int32_t fd) noexcept;
    auto seek_file_end(int32_t fd) noexcept -> int64_t;
    auto seek_file_begin(int32_t fd) noexcept -> bool;
    auto read_file_chunk(int32_t fd, ptr<char> buffer, size_t size) noexcept -> int64_t;
    auto write_file_chunk(int32_t fd, ptr<const char> data, size_t size) noexcept -> int64_t;
    auto truncate_file(int32_t fd) noexcept -> bool;
    auto sync_file(int32_t fd) noexcept -> bool;

    // Runs a command with its output captured, chunk by chunk through the callback, returning its exit code or
    // -1. Not noexcept: the callback is caller code, and a throw from it must reach the caller, not end here
    auto run_process_capturing_output(const string& command, const function<void(string_view)>& on_output) -> int32_t;

    // Per-user registry values, always REG_SZ holding raw UTF-8 so the round trip preserves every byte
    auto registry_read_value(const string& sub_key, const string& name) noexcept -> optional<string>;
    auto registry_write_value(const string& sub_key, const string& name, const string& value) noexcept -> bool;
    void registry_delete_value(const string& sub_key, const string& name) noexcept;

    // Sub-millisecond waits: the plain sleep rounds its request up to the timer tick, this does not
    auto create_high_resolution_timer() noexcept -> nptr<void>;
    auto set_relative_timer(nptr<void> timer, int64_t delay_100ns) noexcept -> bool;
    void wait_for_object(nptr<void> handle) noexcept;
    void close_handle(nptr<void> handle) noexcept;
}

FO_END_NAMESPACE

#endif
