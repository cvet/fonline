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

    // Windows: OutputDebugStringW; Android: __android_log_write; other: no-op
    static void InfoLog(const string& str) noexcept;

    // Windows (>= 10): SetThreadDescription
    // Other: none
    static void SetThreadName(const string& str) noexcept;

    // Windows: GetModuleFileNameW; Linux: /proc/self/exe; macOS: proc_pidpath; other: nullopt
    static auto GetExePath() noexcept -> optional<string>;

    // Per-user writable data root from environment only: LOCALAPPDATA/APPDATA, Library/Application Support, or XDG_DATA_HOME.
    // Return an empty string when no platform path is available
    static auto GetUserDataBase() noexcept -> string;

    // Linux & Mac: fork
    // Other: warning log message
    static auto ForkProcess() noexcept -> bool;

    // Windows: GetCurrentProcessId; Linux and macOS: getpid; other: "0"
    static auto GetCurrentProcessIdStr() noexcept -> string;

    // Resident process bytes from WorkingSetSize, /proc/self/statm, or MACH_TASK_BASIC_INFO.
    // Return zero when unsupported
    static auto GetProcessMemoryUsage() noexcept -> size_t;

    // Private process bytes from PrivateUsage or /proc/self/status VmData.
    // Return zero when unsupported
    static auto GetProcessPrivateMemoryUsage() noexcept -> size_t;

    // Cumulative process and available per-core CPU counters; compare snapshots to derive usage.
    // LogicalCoreCount is always populated for normalization
    static auto GetCpuUsageSnapshot() noexcept -> CpuUsageSnapshot;

    // Windows: LoadLibraryW family; Linux and macOS: dlopen family; other: nullptr
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
