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
#include "SmartPointers.h"
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
static auto WinApi_GetProcAddress(string_view_nt mod, string_view_nt name) -> T
{
    FO_STACK_TRACE_ENTRY();

    ptr<const char> module_name = mod.c_str();
    ptr<const char> proc_name = name.c_str();

    const auto hmod = ::GetModuleHandleA(module_name.get());
    if (hmod != nullptr) {
        FARPROC proc = ::GetProcAddress(hmod, proc_name.get());
        return reinterpret_cast<T>(proc); // NOLINT(clang-diagnostic-cast-function-type-strict)
    }

    return nullptr;
}

static auto WinApiModuleHandle(nptr<void> module_handle) noexcept -> HMODULE
{
    FO_STACK_TRACE_ENTRY();

    assert(module_handle);

    return module_handle.template cast<std::remove_pointer_t<HMODULE>>().get_no_const();
}

static auto WinApi_GetProcAddressRaw(nptr<void> module_handle, const string& func_name) noexcept -> nptr<void>
{
    FO_STACK_TRACE_ENTRY();

    HMODULE hmodule = WinApiModuleHandle(module_handle);
    ptr<const char> func_name_cstr = func_name.c_str();
    FARPROC proc = ::GetProcAddress(hmodule, func_name_cstr.get());

    if (proc == nullptr) {
        return nullptr;
    }

    return reinterpret_cast<void*>(proc);
}
#endif

template<typename T>
static auto ObjectOutPtr(T& value) noexcept -> ptr<T>
{
    FO_NO_STACK_TRACE_ENTRY();

    return &value;
}

#if FO_LINUX || FO_ANDROID
static auto ParseUInt64(string_view text, uint64_t& value) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (text.empty()) {
        return false;
    }

    ptr<const char> text_begin = text.data();
    ptr<const char> text_end = text_begin.get() + text.size();
    const auto result = std::from_chars(text_begin.get(), text_end.get(), value);
    return result.ec == std::errc {} && text_end == result.ptr;
}
#endif

void Platform::InfoLog(const string& str) noexcept
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    const wstring message = strex(str).to_wide_char();
    ptr<const wchar_t> message_cstr = message.c_str();
    ::OutputDebugStringW(message_cstr.get());
#elif FO_ANDROID
    ptr<const char> message_cstr = str.c_str();
    __android_log_write(ANDROID_LOG_INFO, "FO", message_cstr.get());
#endif
}

void Platform::SetThreadName(const string& str) noexcept
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    using SetThreadDescriptionFn = HRESULT(WINAPI*)(HANDLE, PCWSTR);
    const static auto set_thread_description = WinApi_GetProcAddress<SetThreadDescriptionFn>("kernel32.dll", "SetThreadDescription");

    if (set_thread_description != nullptr) {
        const wstring thread_name = strex(str).to_wide_char();
        ptr<const wchar_t> thread_name_cstr = thread_name.c_str();
        set_thread_description(::GetCurrentThread(), thread_name_cstr.get());
    }
#endif
}

auto Platform::GetExePath() noexcept -> optional<string>
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    vector<wchar_t> path;
    path.resize(FILENAME_MAX);
    ptr<wchar_t> path_data = path.data();
    auto size = ::GetModuleFileNameW(nullptr, path_data.get(), static_cast<DWORD>(path.size()));

    if (size == 0) {
        return std::nullopt;
    }

    while (size == path.size()) {
        path.resize(path.size() * 2);
        path_data = path.data();
        size = ::GetModuleFileNameW(nullptr, path_data.get(), static_cast<DWORD>(path.size()));

        if (size == 0) {
            return std::nullopt;
        }
    }

    return strex().parse_wide_char(path_data.get());

#elif FO_LINUX
    char path[FILENAME_MAX];
    ptr<char> path_data = path;
    const auto size = ::readlink("/proc/self/exe", path_data.get(), sizeof(path) - 1);

    if (size == -1) {
        return std::nullopt;
    }

    path_data[numeric_cast<size_t>(size)] = '\0';
    return string {path_data.get()};

#elif FO_MAC
    char path[PROC_PIDPATHINFO_MAXSIZE];
    ptr<char> path_data = path;

    const auto pid = ::getpid();

    if (::proc_pidpath(pid, path_data.get(), sizeof(path)) <= 0) {
        return std::nullopt;
    }

    return string {path_data.get()};

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
    auto pmc_data = ObjectOutPtr(pmc);
    if (::GetProcessMemoryInfo(::GetCurrentProcess(), pmc_data.get(), sizeof(pmc)) != 0) {
        return static_cast<size_t>(pmc.WorkingSetSize);
    }
    return 0;

#elif FO_LINUX || FO_ANDROID
    // /proc/self/statm: size resident shared text lib data dt (values in pages)
    nptr<std::FILE> file = std::fopen("/proc/self/statm", "r");
    if (!file) {
        return 0;
    }
    unsigned long size_pages = 0;
    unsigned long rss_pages = 0;
    auto size_pages_data = ObjectOutPtr(size_pages);
    auto rss_pages_data = ObjectOutPtr(rss_pages);
    const int matched = std::fscanf(file.get(), "%lu %lu", size_pages_data.get(), rss_pages_data.get());
    std::fclose(file.get());
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
    auto info_data = ObjectOutPtr(info);
    ptr<integer_t> task_info_data = info_data.reinterpret_as<integer_t>();
    auto count_data = ObjectOutPtr(count);
    if (::task_info(::mach_task_self(), MACH_TASK_BASIC_INFO, task_info_data.get(), count_data.get()) == KERN_SUCCESS) {
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
    auto pmc_data = ObjectOutPtr(pmc);
    ptr<PROCESS_MEMORY_COUNTERS> pmc_counters = pmc_data.reinterpret_as<PROCESS_MEMORY_COUNTERS>();
    if (::GetProcessMemoryInfo(::GetCurrentProcess(), pmc_counters.get(), sizeof(pmc)) != 0) {
        return pmc.PrivateUsage;
    }
    return 0;

#elif FO_LINUX || FO_ANDROID
    nptr<std::FILE> file = std::fopen("/proc/self/status", "r");
    if (!file) {
        return 0;
    }

    char line[256] {};
    size_t result = 0;
    while (std::fgets(line, sizeof(line), file.get()) != nullptr) {
        size_t value_kib = 0;
        auto value_kib_data = ObjectOutPtr(value_kib);
        if (std::sscanf(line, "VmData:%zu kB", value_kib_data.get()) == 1) {
            result = value_kib * 1024;
            break;
        }
    }
    std::fclose(file.get());
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

    auto creation_time_data = ObjectOutPtr(creation_time);
    auto exit_time_data = ObjectOutPtr(exit_time);
    auto kernel_time_data = ObjectOutPtr(kernel_time);
    auto user_time_data = ObjectOutPtr(user_time);
    if (::GetProcessTimes(::GetCurrentProcess(), creation_time_data.get(), exit_time_data.get(), kernel_time_data.get(), user_time_data.get()) != 0) {
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
        ptr<WindowsSystemProcessorPerformanceInformation> processor_info_data = processor_info.data();
        ptr<void> processor_info_buffer = cast_to_void(processor_info_data.get());
        auto returned_length_data = ObjectOutPtr(returned_length);
        const LONG status = nt_query_system_information(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION_CLASS, processor_info_buffer.get(), buffer_size, returned_length_data.get());

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

        auto idle_time_data = ObjectOutPtr(idle_time);
        auto system_kernel_time_data = ObjectOutPtr(system_kernel_time);
        auto system_user_time_data = ObjectOutPtr(system_user_time);
        if (::GetSystemTimes(idle_time_data.get(), system_kernel_time_data.get(), system_user_time_data.get()) != 0) {
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

                uint64_t value = 0;

                if (!ParseUInt64(field, value)) {
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

        const size_t fields_offset = comm_end + 2;
        const string_view fields_text = string_view {text}.substr(fields_offset);
        const vector<string_view> fields = strvex(fields_text).split(' ');

        if (fields.size() <= 12) {
            return 0;
        }

        uint64_t user_ticks = 0;
        uint64_t system_ticks = 0;

        if (!ParseUInt64(fields[11], user_ticks)) {
            return 0;
        }

        if (!ParseUInt64(fields[12], system_ticks)) {
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
    processor_info_array_t raw_processor_info {};
    mach_msg_type_number_t processor_info_count = 0;

    auto processor_count_data = ObjectOutPtr(processor_count);
    auto processor_info_data = ObjectOutPtr(raw_processor_info);
    auto processor_info_count_data = ObjectOutPtr(processor_info_count);
    if (::host_processor_info(::mach_host_self(), PROCESSOR_CPU_LOAD_INFO, processor_count_data.get(), processor_info_data.get(), processor_info_count_data.get()) == KERN_SUCCESS) {
        FO_VERIFY_AND_THROW(raw_processor_info != nullptr, "processor_info");
        ptr<const integer_t> processor_info = raw_processor_info;
        ptr<const processor_cpu_load_info_data_t> load_info_data = processor_info.reinterpret_as<const processor_cpu_load_info_data_t>();
        span<const processor_cpu_load_info_data_t> load_info {load_info_data.get(), processor_count};

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

        (void)::vm_deallocate(::mach_task_self(), reinterpret_cast<vm_address_t>(raw_processor_info), processor_info_count * sizeof(integer_t));
    }

    result.ProcessTimeNs = []() noexcept -> uint64_t {
        mach_task_basic_info_data_t info {};
        mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
        auto info_data = ObjectOutPtr(info);
        ptr<integer_t> task_info_data = info_data.reinterpret_as<integer_t>();
        auto count_data = ObjectOutPtr(count);

        if (::task_info(::mach_task_self(), MACH_TASK_BASIC_INFO, task_info_data.get(), count_data.get()) != KERN_SUCCESS) {
            return 0;
        }

        // task_info time_value_t fields are seconds + microseconds.
        return static_cast<uint64_t>(info.user_time.seconds) * 1000000000ULL + static_cast<uint64_t>(info.user_time.microseconds) * 1000ULL + static_cast<uint64_t>(info.system_time.seconds) * 1000000000ULL + static_cast<uint64_t>(info.system_time.microseconds) * 1000ULL;
    }();
#endif

    return result;
}

auto Platform::LoadModule(const string& module_name) noexcept -> nptr<void>
{
    FO_STACK_TRACE_ENTRY();

    nptr<void> module_handle = nullptr;

    const auto add_extension = [](const string& path, string_view extension) -> string { //
        return path.ends_with(extension) ? path : strex(strex::safe_format, "{}{}", path, extension).str();
    };

#if FO_WINDOWS
    const string module_path = add_extension(module_name, ".dll");
    const wstring module_path_wide = strex(module_path).to_wide_char();
    ptr<const wchar_t> module_path_cstr = module_path_wide.c_str();
    module_handle = ::LoadLibraryW(module_path_cstr.get());
#elif FO_LINUX
    const string module_path = add_extension(module_name, ".so");
    ptr<const char> module_path_cstr = module_path.c_str();
    module_handle = ::dlopen(module_path_cstr.get(), RTLD_LAZY | RTLD_LOCAL);
#elif FO_MAC
    const string module_path = add_extension(module_name, ".dylib");
    ptr<const char> module_path_cstr = module_path.c_str();
    module_handle = ::dlopen(module_path_cstr.get(), RTLD_LAZY | RTLD_LOCAL);
#endif

    return module_handle;
}

void Platform::UnloadModule(nptr<void> module_handle) noexcept
{
    FO_STACK_TRACE_ENTRY();

    if (!module_handle) {
        return;
    }

#if FO_WINDOWS
    ::FreeLibrary(WinApiModuleHandle(module_handle));
#elif FO_LINUX || FO_MAC
    ::dlclose(module_handle.get());
#endif
}

auto Platform::GetFuncAddr(nptr<void> module_handle, const string& func_name) noexcept -> void*
{
    FO_STACK_TRACE_ENTRY();

    nptr<void> func = nullptr;

#if FO_WINDOWS
    func = WinApi_GetProcAddressRaw(module_handle, func_name);
#elif FO_LINUX || FO_MAC
    ptr<const char> func_name_cstr = func_name.c_str();
    func = ::dlsym(module_handle ? module_handle.get() : RTLD_DEFAULT, func_name_cstr.get());
#endif

    return func.get_no_const();
}

FO_END_NAMESPACE
