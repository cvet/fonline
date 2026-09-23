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

FO_BEGIN_NAMESPACE

namespace platform
{
    struct cpu_usage_core_snapshot
    {
        uint64_t idle_time {};
        uint64_t total_time {};
    };

    struct cpu_usage_snapshot
    {
        vector<cpu_usage_core_snapshot> cores {};
        uint64_t process_time_ns {};
        uint32_t logical_core_count {};
    };

    // Windows: OutputDebugStringW; Android: __android_log_write; other: no-op
    void info_log(const string& str) noexcept;

    // Windows (>= 10): SetThreadDescription
    // Other: none
    void set_thread_name(const string& str) noexcept;

    // Windows: GetModuleFileNameW; Linux: /proc/self/exe; macOS: proc_pidpath; other: nullopt
    auto get_exe_path() noexcept -> optional<string>;

    // Per-user writable data root from environment only: LOCALAPPDATA/APPDATA, Library/Application Support, or XDG_DATA_HOME.
    // Return an empty string when no platform path is available
    auto get_user_data_base() noexcept -> string;

    // Linux & Mac: fork
    // Other: warning log message
    auto fork_process() noexcept -> bool;

    // Windows: GetCurrentProcessId; Linux and macOS: getpid; other: "0"
    auto get_current_process_id_str() noexcept -> string;

    // One process for good: the id alone is handed on to the next process once this one ends, the id with the
    // start time is not. A zero start time means the platform could not tell
    struct process_identity
    {
        int64_t pid {};
        uint64_t start_time {};
    };

    // Windows: GetProcessTimes creation time; Linux: /proc stat start ticks; macOS: proc_pidinfo start; other: zero
    auto get_current_process_identity() noexcept -> process_identity;
    // Whether the process the identity names is still running. An identity the platform cannot tell is never
    // running, so a caller acting on the answer cannot act on a stranger that reused the id
    auto is_process_running(const process_identity& identity) noexcept -> bool;

    // Resident process bytes from WorkingSetSize, /proc/self/statm, or MACH_TASK_BASIC_INFO.
    // Return zero when unsupported
    auto get_process_memory_usage() noexcept -> size_t;

    // Private process bytes from PrivateUsage or /proc/self/status VmData.
    // Return zero when unsupported
    auto get_process_private_memory_usage() noexcept -> size_t;

    // Cumulative process and available per-core CPU counters; compare snapshots to derive usage.
    // logical_core_count is always populated for normalization
    auto get_cpu_usage_snapshot() noexcept -> cpu_usage_snapshot;

    // Windows: BCryptGenRandom; Linux and web: getentropy; macOS, iOS and Android: arc4random_buf
    auto fill_system_random(span<uint8_t> buf) noexcept -> bool;

    // Windows: LoadLibraryW family; Linux and macOS: dlopen family; other: nullptr
    auto load_module(const string& module_name) noexcept -> nptr<void>;
    // For a module that must never be unmapped, such as an engine library: statically linked runtimes install
    // process-wide hooks into it that cannot be withdrawn. unload_module on the handle leaves it mapped
    auto load_pinned_module(const string& module_name) noexcept -> nptr<void>;
    void unload_module(nptr<void> module_handle) noexcept;
    auto get_func_addr(nptr<void> module_handle, const string& func_name) noexcept -> void*;
    template<typename T>
    auto get_func_addr(nptr<void> module_handle, const string& func_name) noexcept -> T
    {
        return reinterpret_cast<T>(get_func_addr(module_handle, func_name));
    }
}

FO_END_NAMESPACE
