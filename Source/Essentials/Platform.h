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
#include "Containers.h"
#include "SmartPointers.h"

FO_BEGIN_NAMESPACE

struct Platform
{
    Platform() = delete;

    struct CpuUsageCoreSnapshot
    {
        uint64_t IdleTime {};
        uint64_t TotalTime {};
    };

    struct CpuUsageSnapshot
    {
        vector<CpuUsageCoreSnapshot> Cores {};
        uint64_t ProcessTimeNs {};
        uint32_t LogicalCoreCount {};
    };

    // Windows: OutputDebugStringW
    // Android: __android_log_write ANDROID_LOG_INFO
    // Other: none
    static void InfoLog(const string& str) noexcept;

    // Windows (>= 10): SetThreadDescription
    // Other: none
    static void SetThreadName(const string& str) noexcept;

    // Windows: GetModuleFileNameW
    // Linux: readlink /proc/self/exe
    // Mac: proc_pidpath
    // Other: nullopt
    static auto GetExePath() noexcept -> optional<string>;

    // Base directory for per-user writable application data, from environment only (no SDL/shell32).
    // Windows: %LOCALAPPDATA% (else %APPDATA%)
    // Mac & iOS: $HOME/Library/Application Support
    // Linux, Android & other: $XDG_DATA_HOME (else $HOME/.local/share)
    // Not found: empty string
    static auto GetUserDataBase() noexcept -> string;

    // Linux & Mac: fork
    // Other: warning log message
    static auto ForkProcess() noexcept -> bool;

    // Windows: GetCurrentProcessId
    // Linux & Mac: getpid
    // Other: "0"
    static auto GetCurrentProcessIdStr() noexcept -> string;

    // Resident memory of the current process in bytes.
    // Windows: GetProcessMemoryInfo (PROCESS_MEMORY_COUNTERS::WorkingSetSize)
    // Linux & Android: /proc/self/statm (RSS pages * page size)
    // Mac: task_info MACH_TASK_BASIC_INFO (resident_size)
    // Other: 0
    static auto GetProcessMemoryUsage() noexcept -> size_t;

    // Private/committed memory of the current process in bytes when the platform exposes it.
    // Windows: GetProcessMemoryInfo (PROCESS_MEMORY_COUNTERS_EX::PrivateUsage)
    // Linux & Android: /proc/self/status (VmData)
    // Other: 0
    static auto GetProcessPrivateMemoryUsage() noexcept -> size_t;

    // Cumulative CPU counters for the current process, plus per-core counters where the OS exposes them
    // through a documented API. Percent usage is calculated by comparing two snapshots. LogicalCoreCount is
    // the logical-CPU count for normalization (always set, even when Cores holds only a system-wide aggregate).
    // Windows: GetProcessTimes + GetSystemTimes (system-wide aggregate) + GetActiveProcessorCount
    // Linux & Android: /proc/stat (per-core) + /proc/self/stat
    // Mac: host_processor_info(PROCESSOR_CPU_LOAD_INFO) (per-core) + task_info(MACH_TASK_BASIC_INFO)
    // Other: empty snapshot
    static auto GetCpuUsageSnapshot() noexcept -> CpuUsageSnapshot;

    // Windows: LoadLibraryW/FreeLibrary/GetProcAddress
    // Linux & Mac: dlopen/dlclose/dlsym
    // Other: nullptr
    static auto LoadModule(const string& module_name) noexcept -> nptr<void>;
    static void UnloadModule(nptr<void> module_handle) noexcept;
    static auto GetFuncAddr(nptr<void> module_handle, const string& func_name) noexcept -> void*;
    template<typename T>
    static auto GetFuncAddr(nptr<void> module_handle, const string& func_name) noexcept -> T
    {
        return reinterpret_cast<T>(GetFuncAddr(module_handle, func_name));
    }
};

FO_END_NAMESPACE
