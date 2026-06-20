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

#include "Platform.h"
#include "StackTrace.h"
#include "StringUtils.h"

#if FO_WINDOWS
#include "WinApi-Include.h"
#include <psapi.h>
#endif

#if FO_LINUX || FO_MAC
#include <dlfcn.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#if FO_LINUX || FO_ANDROID
#include <cstdio>
#endif

#if FO_MAC
#include <libproc.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/processor_info.h>
#include <mach/task.h>
#include <mach/task_info.h>
#endif

#if FO_ANDROID
#include <android/log.h>
#include <unistd.h>
#endif

FO_BEGIN_NAMESPACE

#if FO_WINDOWS
template<typename T>
static auto WinApi_GetProcAddress(const char* mod, const char* name) -> T
{
    FO_STACK_TRACE_ENTRY();

    if (auto* hmod = ::GetModuleHandleA(mod); hmod != nullptr) {
        return reinterpret_cast<T>(::GetProcAddress(hmod, name)); // NOLINT(clang-diagnostic-cast-function-type-strict)
    }

    return nullptr;
}
#endif

void Platform::InfoLog(const string& str) noexcept
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    ::OutputDebugStringW(strex(str).to_wide_char().c_str());
#elif FO_ANDROID
    __android_log_write(ANDROID_LOG_INFO, "FO", str.c_str());
#endif
}

void Platform::SetThreadName(const string& str) noexcept
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    using SetThreadDescriptionFn = HRESULT(WINAPI*)(HANDLE, PCWSTR);
    const static auto set_thread_description = WinApi_GetProcAddress<SetThreadDescriptionFn>("kernel32.dll", "SetThreadDescription");

    if (set_thread_description != nullptr) {
        set_thread_description(::GetCurrentThread(), strex(str).to_wide_char().c_str());
    }
#endif
}

auto Platform::GetExePath() noexcept -> optional<string>
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    vector<wchar_t> path;
    path.resize(FILENAME_MAX);
    auto size = ::GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));

    if (size == 0) {
        return std::nullopt;
    }

    while (size == path.size()) {
        path.resize(path.size() * 2);
        size = ::GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));

        if (size == 0) {
            return std::nullopt;
        }
    }

    return strex().parse_wide_char(path.data());

#elif FO_LINUX
    char path[FILENAME_MAX];
    const auto size = ::readlink("/proc/self/exe", path, sizeof(path) - 1);

    if (size == -1) {
        return std::nullopt;
    }

    path[size] = '\0';
    return path;

#elif FO_MAC
    char path[PROC_PIDPATHINFO_MAXSIZE];

    const auto pid = ::getpid();

    if (::proc_pidpath(pid, path, sizeof(path)) <= 0) {
        return std::nullopt;
    }

    return path;

#else
    return std::nullopt;
#endif
}

auto Platform::GetUserDataBase() noexcept -> string
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    if (const char* local = std::getenv("LOCALAPPDATA"); local != nullptr && local[0] != '\0') {
        return local;
    }
    if (const char* roaming = std::getenv("APPDATA"); roaming != nullptr && roaming[0] != '\0') {
        return roaming;
    }
    return "";
#elif FO_MAC || FO_IOS
    if (const char* home = std::getenv("HOME"); home != nullptr && home[0] != '\0') {
        return strex(home).combine_path("Library/Application Support").str();
    }
    return "";
#else
    if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg != nullptr && xdg[0] != '\0') {
        return xdg;
    }
    if (const char* home = std::getenv("HOME"); home != nullptr && home[0] != '\0') {
        return strex(home).combine_path(".local/share").str();
    }
    return "";
#endif
}

auto Platform::ForkProcess() noexcept -> bool // NOLINT(clang-diagnostic-missing-noreturn)
{
    FO_STACK_TRACE_ENTRY();

#if FO_LINUX || FO_MAC
    const pid_t pid = ::fork();

    if (pid < 0) {
        return false;
    }
    else if (pid != 0) {
        ExitApp(true);
    }

    ::close(STDIN_FILENO);
    ::close(STDOUT_FILENO);
    ::close(STDERR_FILENO);
    ::setsid();

    return true;

#else
    return false;
#endif
}

auto Platform::GetCurrentProcessIdStr() noexcept -> string
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    return strex("{}", ::GetCurrentProcessId()).str();
#elif FO_LINUX || FO_MAC
    return strex("{}", ::getpid()).str();
#else
    return "0";
#endif
}

auto Platform::GetProcessMemoryUsage() noexcept -> size_t
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    PROCESS_MEMORY_COUNTERS pmc {};
    if (::GetProcessMemoryInfo(::GetCurrentProcess(), &pmc, sizeof(pmc)) != 0) {
        return static_cast<size_t>(pmc.WorkingSetSize);
    }
    return 0;

#elif FO_LINUX || FO_ANDROID
    // /proc/self/statm: size resident shared text lib data dt (values in pages)
    std::FILE* file = std::fopen("/proc/self/statm", "r");
    if (file == nullptr) {
        return 0;
    }
    unsigned long size_pages = 0;
    unsigned long rss_pages = 0;
    const int matched = std::fscanf(file, "%lu %lu", &size_pages, &rss_pages);
    std::fclose(file);
    if (matched != 2) {
        return 0;
    }
    const long page_size = ::sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
        return 0;
    }
    return static_cast<size_t>(rss_pages) * static_cast<size_t>(page_size);

#elif FO_MAC
    mach_task_basic_info_data_t info {};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (::task_info(::mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS) {
        return static_cast<size_t>(info.resident_size);
    }
    return 0;

#else
    return 0;
#endif
}

auto Platform::GetProcessPrivateMemoryUsage() noexcept -> size_t
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    PROCESS_MEMORY_COUNTERS_EX pmc {};
    if (::GetProcessMemoryInfo(::GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc)) != 0) {
        return pmc.PrivateUsage;
    }
    return 0;

#elif FO_LINUX || FO_ANDROID
    std::FILE* file = std::fopen("/proc/self/status", "r");
    if (file == nullptr) {
        return 0;
    }

    char line[256] {};
    size_t result = 0;
    while (std::fgets(line, sizeof(line), file) != nullptr) {
        size_t value_kib = 0;
        if (std::sscanf(line, "VmData:%zu kB", &value_kib) == 1) {
            result = value_kib * 1024;
            break;
        }
    }
    std::fclose(file);
    return result;

#else
    return 0;
#endif
}

auto Platform::GetCpuUsageSnapshot() noexcept -> CpuUsageSnapshot
{
    FO_STACK_TRACE_ENTRY();

    CpuUsageSnapshot result;

#if FO_WINDOWS
    struct WindowsSystemProcessorPerformanceInformation
    {
        LARGE_INTEGER IdleTime {};
        LARGE_INTEGER KernelTime {};
        LARGE_INTEGER UserTime {};
        LARGE_INTEGER DpcTime {};
        LARGE_INTEGER InterruptTime {};
        ULONG InterruptCount {};
    };

    const auto file_time_to_uint64 = [](FILETIME time) noexcept -> uint64_t {
        ULARGE_INTEGER value {};
        value.LowPart = time.dwLowDateTime;
        value.HighPart = time.dwHighDateTime;
        return value.QuadPart;
    };

    FILETIME creation_time {};
    FILETIME exit_time {};
    FILETIME kernel_time {};
    FILETIME user_time {};

    if (::GetProcessTimes(::GetCurrentProcess(), &creation_time, &exit_time, &kernel_time, &user_time) != 0) {
        // FILETIME process times are in 100 ns units.
        result.ProcessTimeNs = (file_time_to_uint64(kernel_time) + file_time_to_uint64(user_time)) * 100;
    }

    using NtQuerySystemInformationFn = LONG(WINAPI*)(ULONG, PVOID, ULONG, PULONG);
    constexpr ULONG SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION_CLASS = 8;
    const auto nt_query_system_information = WinApi_GetProcAddress<NtQuerySystemInformationFn>("ntdll.dll", "NtQuerySystemInformation");

    if (nt_query_system_information != nullptr) {
        DWORD processor_count = ::GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);

        if (processor_count == 0) {
            processor_count = 1;
        }

        vector<WindowsSystemProcessorPerformanceInformation> processor_info;
        processor_info.resize(static_cast<size_t>(processor_count));

        ULONG returned_length = 0;
        const ULONG buffer_size = static_cast<ULONG>(processor_info.size() * sizeof(WindowsSystemProcessorPerformanceInformation));
        const LONG status = nt_query_system_information(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION_CLASS, processor_info.data(), buffer_size, &returned_length);

        if (status >= 0) {
            const size_t actual_count = returned_length != 0 ? std::min(processor_info.size(), static_cast<size_t>(returned_length / sizeof(WindowsSystemProcessorPerformanceInformation))) : processor_info.size();

            result.Cores.reserve(actual_count);

            for (size_t i = 0; i < actual_count; i++) {
                const WindowsSystemProcessorPerformanceInformation& info = processor_info[i];
                result.Cores.emplace_back(CpuUsageCoreSnapshot {
                    .IdleTime = static_cast<uint64_t>(info.IdleTime.QuadPart),
                    .TotalTime = static_cast<uint64_t>(info.KernelTime.QuadPart + info.UserTime.QuadPart),
                });
            }
        }
    }

    if (result.Cores.empty()) {
        FILETIME idle_time {};
        FILETIME system_kernel_time {};
        FILETIME system_user_time {};

        if (::GetSystemTimes(&idle_time, &system_kernel_time, &system_user_time) != 0) {
            result.Cores.emplace_back(CpuUsageCoreSnapshot {
                .IdleTime = file_time_to_uint64(idle_time),
                .TotalTime = file_time_to_uint64(system_kernel_time) + file_time_to_uint64(system_user_time),
            });
        }
    }

#elif FO_LINUX || FO_ANDROID
    // /proc/stat per-CPU counters appear in kernel order:
    //   user nice system idle iowait irq softirq steal guest guest_nice
    constexpr size_t MAX_CPU_FIELDS = 10;
    constexpr size_t IDLE_FIELD_INDEX = 3;
    constexpr size_t IOWAIT_FIELD_INDEX = 4;
    constexpr size_t TOTAL_TIME_FIELD_COUNT = 8; // user..steal; guest/guest_nice are folded into user/nice

    std::ifstream stat_file {"/proc/stat"};

    if (stat_file) {
        string line;

        while (std::getline(stat_file, line)) {
            const string_view line_view {line};

            if (line_view.size() <= 3 || !line_view.starts_with("cpu") || !std::isdigit(static_cast<unsigned char>(line_view[3]))) {
                continue;
            }

            const size_t fields_pos = line_view.find(' ');

            if (fields_pos == string_view::npos) {
                continue;
            }

            uint64_t values[MAX_CPU_FIELDS] {};
            size_t values_count = 0;
            bool parse_failed = false;
            const vector<string_view> fields = strvex(line_view.substr(fields_pos + 1)).split(' ');

            for (const string_view field : fields) {
                if (values_count == std::size(values)) {
                    break;
                }

                const char* const field_begin = field.data();
                const char* const field_end = field_begin + field.size();
                uint64_t value = 0;
                const auto parse_result = std::from_chars(field_begin, field_end, value);

                if (parse_result.ec != std::errc {} || parse_result.ptr != field_end) {
                    parse_failed = true;
                    break;
                }

                values[values_count] = value;
                values_count++;
            }

            if (parse_failed || values_count <= IDLE_FIELD_INDEX) {
                continue;
            }

            uint64_t total_time = 0;
            const size_t total_field_count = std::min<size_t>(values_count, TOTAL_TIME_FIELD_COUNT);

            for (size_t i = 0; i < total_field_count; i++) {
                total_time += values[i];
            }

            result.Cores.emplace_back(CpuUsageCoreSnapshot {
                .IdleTime = values[IDLE_FIELD_INDEX] + (values_count > IOWAIT_FIELD_INDEX ? values[IOWAIT_FIELD_INDEX] : 0),
                .TotalTime = total_time,
            });
        }
    }

    result.ProcessTimeNs = []() noexcept -> uint64_t {
        std::ifstream file {"/proc/self/stat"};

        if (!file) {
            return 0;
        }

        string text;
        std::getline(file, text);

        const size_t comm_end = text.rfind(')');

        if (comm_end == string::npos || comm_end + 2 >= text.size()) {
            return 0;
        }

        const vector<string_view> fields = strvex(string_view {text.data() + comm_end + 2, text.size() - comm_end - 2}).split(' ');

        if (fields.size() <= 12) {
            return 0;
        }

        uint64_t user_ticks = 0;
        uint64_t system_ticks = 0;

        const char* const user_ticks_begin = fields[11].data();
        const char* const user_ticks_end = user_ticks_begin + fields[11].size();
        const auto user_ticks_result = std::from_chars(user_ticks_begin, user_ticks_end, user_ticks);

        if (user_ticks_result.ec != std::errc {} || user_ticks_result.ptr != user_ticks_end) {
            return 0;
        }

        const char* const system_ticks_begin = fields[12].data();
        const char* const system_ticks_end = system_ticks_begin + fields[12].size();
        const auto system_ticks_result = std::from_chars(system_ticks_begin, system_ticks_end, system_ticks);

        if (system_ticks_result.ec != std::errc {} || system_ticks_result.ptr != system_ticks_end) {
            return 0;
        }

        const long ticks_per_second = ::sysconf(_SC_CLK_TCK);

        if (ticks_per_second <= 0) {
            return 0;
        }

        return (user_ticks + system_ticks) * 1000000000ULL / static_cast<uint64_t>(ticks_per_second);
    }();

#elif FO_MAC
    natural_t processor_count = 0;
    processor_info_array_t processor_info {};
    mach_msg_type_number_t processor_info_count = 0;

    if (::host_processor_info(::mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &processor_count, &processor_info, &processor_info_count) == KERN_SUCCESS) {
        const auto* load_info = reinterpret_cast<const processor_cpu_load_info_data_t*>(processor_info);

        result.Cores.reserve(static_cast<size_t>(processor_count));

        for (natural_t i = 0; i < processor_count; i++) {
            const processor_cpu_load_info_data_t& info = load_info[i];
            const uint64_t user_time = static_cast<uint64_t>(info.cpu_ticks[CPU_STATE_USER]);
            const uint64_t system_time = static_cast<uint64_t>(info.cpu_ticks[CPU_STATE_SYSTEM]);
            const uint64_t idle_time = static_cast<uint64_t>(info.cpu_ticks[CPU_STATE_IDLE]);
            const uint64_t nice_time = static_cast<uint64_t>(info.cpu_ticks[CPU_STATE_NICE]);

            result.Cores.emplace_back(CpuUsageCoreSnapshot {
                .IdleTime = idle_time,
                .TotalTime = user_time + system_time + idle_time + nice_time,
            });
        }

        (void)::vm_deallocate(::mach_task_self(), reinterpret_cast<vm_address_t>(processor_info), processor_info_count * sizeof(integer_t));
    }

    result.ProcessTimeNs = []() noexcept -> uint64_t {
        mach_task_basic_info_data_t info {};
        mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;

        if (::task_info(::mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &count) != KERN_SUCCESS) {
            return 0;
        }

        // task_info time_value_t fields are seconds + microseconds.
        return static_cast<uint64_t>(info.user_time.seconds) * 1000000000ULL + static_cast<uint64_t>(info.user_time.microseconds) * 1000ULL + static_cast<uint64_t>(info.system_time.seconds) * 1000000000ULL + static_cast<uint64_t>(info.system_time.microseconds) * 1000ULL;
    }();
#endif

    return result;
}

auto Platform::LoadModule(const string& module_name) noexcept -> void*
{
    FO_STACK_TRACE_ENTRY();

    void* module_handle = nullptr;

    const auto add_extension = [](const string& path, string_view extension) -> string { //
        return path.ends_with(extension) ? path : strex(strex::safe_format, "{}{}", path, extension).str();
    };

#if FO_WINDOWS
    module_handle = ::LoadLibraryW(strex(add_extension(module_name, ".dll")).to_wide_char().c_str());
#elif FO_LINUX
    module_handle = ::dlopen(add_extension(module_name, ".so").c_str(), RTLD_LAZY | RTLD_LOCAL);
#elif FO_MAC
    module_handle = ::dlopen(add_extension(module_name, ".dylib").c_str(), RTLD_LAZY | RTLD_LOCAL);
#endif

    return module_handle;
}

void Platform::UnloadModule(void* module_handle) noexcept
{
    FO_STACK_TRACE_ENTRY();

    if (module_handle == nullptr) {
        return;
    }

#if FO_WINDOWS
    ::FreeLibrary(static_cast<HMODULE>(module_handle));
#elif FO_LINUX || FO_MAC
    ::dlclose(module_handle);
#endif
}

auto Platform::GetFuncAddr(void* module_handle, const string& func_name) noexcept -> void*
{
    FO_STACK_TRACE_ENTRY();

    void* func = nullptr;

#if FO_WINDOWS
    func = reinterpret_cast<void*>(::GetProcAddress(static_cast<HMODULE>(module_handle), func_name.c_str()));
#elif FO_LINUX || FO_MAC
    func = ::dlsym(module_handle != nullptr ? module_handle : RTLD_DEFAULT, func_name.c_str());
#endif

    return func;
}

FO_END_NAMESPACE
