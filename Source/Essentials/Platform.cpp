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
#include "ExceptionHandling.h"
#include "StackTrace.h"
#include "StringUtils.h"
#include "TextConversions.h"

#if FO_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <psapi.h>
#endif
#include "WinApiUndef.inc"

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

static void AppendModuleExtension(u8string& path, u8string_view extension)
{
    FO_STACK_TRACE_ENTRY();

    if (!path.view().native_view().ends_with(extension.native_view())) {
        path.append(extension);
    }
}

#if FO_WINDOWS
static auto WinApi_GetEnvironmentUtf8(string_view_nt variable_name) -> optional<u8string>
{
    FO_STACK_TRACE_ENTRY();

    wide_string variable_name_wide = string_to_wide_string(variable_name);
    ptr<const wchar_t> variable_name_cstr {variable_name_wide.c_str()};
    DWORD required_size = ::GetEnvironmentVariableW(variable_name_cstr.get(), nullptr, 0);

    if (required_size == 0) {
        return std::nullopt;
    }

    vector<wchar_t> value(static_cast<size_t>(required_size));

    while (true) {
        ptr<wchar_t> value_data {value.data()};
        DWORD value_size = ::GetEnvironmentVariableW(variable_name_cstr.get(), value_data.get(), static_cast<DWORD>(value.size()));

        if (value_size == 0) {
            return std::nullopt;
        }

        if (static_cast<size_t>(value_size) < value.size()) {
            utf16_string value_utf16 = wide_to_utf16(std::wstring_view {value_data.get(), static_cast<size_t>(value_size)});
            u8string result = utf16_to_utf8(std::u16string_view {value_utf16.data(), value_utf16.size()});
            return result.empty() ? optional<u8string> {} : optional<u8string> {std::move(result)};
        }

        value.resize(static_cast<size_t>(value_size));
    }
}

template<typename T>
static auto WinApi_GetProcAddress(string_view_nt mod, string_view_nt name) -> T
{
    FO_STACK_TRACE_ENTRY();

    ptr<const char> module_name {mod.c_str()};
    ptr<const char> proc_name {name.c_str()};

    auto hmod = ::GetModuleHandleA(module_name.get());

    if (hmod != nullptr) {
        FARPROC proc = ::GetProcAddress(hmod, proc_name.get());
        return reinterpret_cast<T>(proc); // NOLINT(clang-diagnostic-cast-function-type-strict)
    }

    return nullptr;
}

static auto WinApiModuleHandle(nptr<void> module_handle) noexcept -> HMODULE
{
    FO_STACK_TRACE_ENTRY();

    if (!module_handle) {
        return ::GetModuleHandleW(nullptr);
    }

    return module_handle.reinterpret_as<std::remove_pointer_t<HMODULE>>().get();
}

static auto WinApi_GetProcAddressRaw(nptr<void> module_handle, ptr<const char> func_name_cstr) noexcept -> nptr<void>
{
    FO_STACK_TRACE_ENTRY();

    HMODULE hmodule = WinApiModuleHandle(module_handle);
    FARPROC proc = ::GetProcAddress(hmodule, func_name_cstr.get());

    if (proc == nullptr) {
        return nullptr;
    }

    return reinterpret_cast<void*>(proc);
}
#endif

#if !FO_WINDOWS
static auto Posix_GetEnvironmentUtf8(string_view_nt variable_name) -> optional<u8string>
{
    FO_STACK_TRACE_ENTRY();

    ptr<const char> variable_name_cstr {variable_name.c_str()};
    nptr<const char> value {std::getenv(variable_name_cstr.get())};

    if (!value || value[0] == char {}) {
        return std::nullopt;
    }

    size_t value_size = std::char_traits<char>::length(value.get());
    return utf8_from_char_span(const_span<char> {value.get(), value_size});
}

static void AppendPosixPathComponent(u8string& path, u8string_view component)
{
    FO_STACK_TRACE_ENTRY();

    if (!path.view().native_view().ends_with(u8'/')) {
        path.append(u8"/");
    }

    path.append(component);
}
#endif

void Platform::InfoLog(u8string_view_nt str)
{
    FO_STACK_TRACE_ENTRY();

    const_span<char8_t> storage {str.c_str(), str.size() + 1};
    u8string_view_nt checked_str = u8string_view_nt::FromChecked(storage);

#if FO_WINDOWS
    utf16_string message_utf16 = utf8_to_utf16(checked_str.view());
    wstring message = utf16_to_wide(std::u16string_view {message_utf16.data(), message_utf16.size()});
    auto message_cstr = make_ptr(message.c_str());
    ::OutputDebugStringW(message_cstr.get());
#elif FO_ANDROID
    ptr<const char> message_cstr = utf8_to_c_str(checked_str);
    __android_log_write(ANDROID_LOG_INFO, "FO", message_cstr.get());
#else
    ignore_unused(checked_str);
#endif
}

void Platform::SetThreadName(u8string_view_nt str)
{
    FO_STACK_TRACE_ENTRY();

    const_span<char8_t> storage {str.c_str(), str.size() + 1};
    u8string_view_nt checked_str = u8string_view_nt::FromChecked(storage);

#if FO_WINDOWS
    using SetThreadDescriptionFn = HRESULT(WINAPI*)(HANDLE, PCWSTR);
    const static auto set_thread_description = WinApi_GetProcAddress<SetThreadDescriptionFn>(string_view_nt {"kernel32.dll"}, string_view_nt {"SetThreadDescription"});

    if (set_thread_description != nullptr) {
        utf16_string thread_name_utf16 = utf8_to_utf16(checked_str.view());
        wstring thread_name = utf16_to_wide(std::u16string_view {thread_name_utf16.data(), thread_name_utf16.size()});
        auto thread_name_cstr = make_ptr(thread_name.c_str());
        set_thread_description(::GetCurrentThread(), thread_name_cstr.get());
    }
#else
    ignore_unused(checked_str);
#endif
}

auto Platform::GetExePath() -> optional<u8string>
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    vector<wchar_t> path(static_cast<size_t>(FILENAME_MAX));
    ptr<wchar_t> path_data {path.data()};
    DWORD path_size = ::GetModuleFileNameW(nullptr, path_data.get(), static_cast<DWORD>(path.size()));

    if (path_size == 0) {
        return std::nullopt;
    }

    while (static_cast<size_t>(path_size) == path.size()) {
        path.resize(path.size() * 2);
        path_data = path.data();
        path_size = ::GetModuleFileNameW(nullptr, path_data.get(), static_cast<DWORD>(path.size()));

        if (path_size == 0) {
            return std::nullopt;
        }
    }

    utf16_string path_utf16 = wide_to_utf16(std::wstring_view {path_data.get(), static_cast<size_t>(path_size)});
    return utf16_to_utf8(std::u16string_view {path_utf16.data(), path_utf16.size()});

#elif FO_LINUX
    vector<char> path(static_cast<size_t>(FILENAME_MAX));

    while (true) {
        ptr<char> path_data {path.data()};
        ssize_t path_size_raw = ::readlink("/proc/self/exe", path_data.get(), path.size());

        if (path_size_raw < 0) {
            return std::nullopt;
        }

        size_t path_size = static_cast<size_t>(path_size_raw);

        if (path_size < path.size()) {
            return utf8_from_char_span(const_span<char> {path_data.get(), path_size});
        }

        path.resize(path.size() * 2);
    }

#elif FO_MAC
    array<char, PROC_PIDPATHINFO_MAXSIZE> path {};
    ptr<char> path_data {path.data()};
    pid_t pid = ::getpid();
    int32_t path_size = ::proc_pidpath(pid, path_data.get(), static_cast<uint32_t>(path.size()));

    if (path_size <= 0) {
        return std::nullopt;
    }

    return utf8_from_char_span(const_span<char> {path_data.get(), static_cast<size_t>(path_size)});

#else
    return std::nullopt;
#endif
}

auto Platform::GetEnvironmentUtf8(string_view_nt variable_name) -> optional<u8string>
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    return WinApi_GetEnvironmentUtf8(variable_name);
#else
    return Posix_GetEnvironmentUtf8(variable_name);
#endif
}

auto Platform::GetUserDataBase() -> u8string
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    if (optional<u8string> local = GetEnvironmentUtf8(string_view_nt {"LOCALAPPDATA"})) {
        return std::move(*local);
    }
    if (optional<u8string> roaming = GetEnvironmentUtf8(string_view_nt {"APPDATA"})) {
        return std::move(*roaming);
    }
    return {};
#elif FO_MAC || FO_IOS
    if (optional<u8string> home = GetEnvironmentUtf8(string_view_nt {"HOME"})) {
        u8string result = std::move(*home);
        AppendPosixPathComponent(result, u8"Library/Application Support");
        return result;
    }
    return {};
#else
    if (optional<u8string> xdg = GetEnvironmentUtf8(string_view_nt {"XDG_DATA_HOME"})) {
        return std::move(*xdg);
    }
    if (optional<u8string> home = GetEnvironmentUtf8(string_view_nt {"HOME"})) {
        u8string result = std::move(*home);
        AppendPosixPathComponent(result, u8".local/share");
        return result;
    }
    return {};
#endif
}

auto Platform::ForkProcess() noexcept -> bool // NOLINT(clang-diagnostic-missing-noreturn)
{
    FO_STACK_TRACE_ENTRY();

#if FO_LINUX || FO_MAC
    pid_t pid = ::fork();

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
    auto file = make_nptr(std::fopen("/proc/self/statm", "r"));
    if (!file) {
        return 0;
    }
    unsigned long size_pages = 0;
    unsigned long rss_pages = 0;
    int matched = std::fscanf(file.get(), "%lu %lu", &size_pages, &rss_pages);
    std::fclose(file.get());
    if (matched != 2) {
        return 0;
    }
    long page_size = ::sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
        return 0;
    }
    return static_cast<size_t>(rss_pages) * static_cast<size_t>(page_size);

#elif FO_MAC
    mach_task_basic_info_data_t info {};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    auto task_info_data = make_ptr(&info).reinterpret_as<integer_t>();
    if (::task_info(::mach_task_self(), MACH_TASK_BASIC_INFO, task_info_data.get(), &count) == KERN_SUCCESS) {
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
    auto pmc_counters = make_ptr(&pmc).reinterpret_as<PROCESS_MEMORY_COUNTERS>();

    if (::GetProcessMemoryInfo(::GetCurrentProcess(), pmc_counters.get(), sizeof(pmc)) != 0) {
        return pmc.PrivateUsage;
    }
    return 0;

#elif FO_LINUX || FO_ANDROID
    auto file = make_nptr(std::fopen("/proc/self/status", "r"));
    if (!file) {
        return 0;
    }

    char line[256] {};
    size_t result = 0;
    while (std::fgets(line, sizeof(line), file.get()) != nullptr) {
        size_t value_kib = 0;
        if (std::sscanf(line, "VmData:%zu kB", &value_kib) == 1) {
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
    auto file_time_to_uint64 = [](FILETIME time) noexcept -> uint64_t {
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

    DWORD processor_count = ::GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    result.LogicalCoreCount = processor_count != 0 ? static_cast<uint32_t>(processor_count) : 1U;

    FILETIME idle_time {};
    FILETIME system_kernel_time {};
    FILETIME system_user_time {};

    if (::GetSystemTimes(&idle_time, &system_kernel_time, &system_user_time) != 0) {
        result.Cores.emplace_back(CpuUsageCoreSnapshot {
            .IdleTime = file_time_to_uint64(idle_time),
            .TotalTime = file_time_to_uint64(system_kernel_time) + file_time_to_uint64(system_user_time),
        });
    }

#elif FO_LINUX || FO_ANDROID
    // /proc/stat per-CPU counters appear in kernel order:
    //   user nice system idle iowait irq softirq steal guest guest_nice
    constexpr size_t MAX_CPU_FIELDS = 10;
    constexpr size_t IDLE_FIELD_INDEX = 3;
    constexpr size_t IOWAIT_FIELD_INDEX = 4;
    constexpr size_t TOTAL_TIME_FIELD_COUNT = 8; // user..steal; guest/guest_nice are folded into user/nice

    auto parse_uint64 = [](string_view text, uint64_t& value) noexcept -> bool {
        if (text.empty()) {
            return false;
        }

        auto text_begin = make_nptr(text.data());
        ptr<const char> text_end = text_begin.offset(text.size());
        auto parse_result = std::from_chars(text_begin.get(), text_end.get(), value);
        return parse_result.ec == std::errc {} && text_end == parse_result.ptr;
    };

    std::ifstream stat_file {"/proc/stat"};

    if (stat_file) {
        string line;

        while (std::getline(stat_file, line)) {
            string_view line_view {line};

            if (line_view.size() <= 3 || !line_view.starts_with("cpu") || !std::isdigit(static_cast<unsigned char>(line_view[3]))) {
                continue;
            }

            size_t fields_pos = line_view.find(' ');

            if (fields_pos == string_view::npos) {
                continue;
            }

            uint64_t values[MAX_CPU_FIELDS] {};
            size_t values_count = 0;
            bool parse_failed = false;
            vector<string_view> fields = strvex(line_view.substr(fields_pos + 1)).split(' ');

            for (string_view field : fields) {
                if (values_count == std::size(values)) {
                    break;
                }

                uint64_t value = 0;

                if (!parse_uint64(field, value)) {
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
            size_t total_field_count = std::min<size_t>(values_count, TOTAL_TIME_FIELD_COUNT);

            for (size_t i = 0; i < total_field_count; i++) {
                total_time += values[i];
            }

            result.Cores.emplace_back(CpuUsageCoreSnapshot {
                .IdleTime = values[IDLE_FIELD_INDEX] + (values_count > IOWAIT_FIELD_INDEX ? values[IOWAIT_FIELD_INDEX] : 0),
                .TotalTime = total_time,
            });
        }
    }

    result.LogicalCoreCount = static_cast<uint32_t>(result.Cores.size());

    result.ProcessTimeNs = [&parse_uint64]() noexcept -> uint64_t {
        std::ifstream file {"/proc/self/stat"};

        if (!file) {
            return 0;
        }

        string text;
        std::getline(file, text);

        size_t comm_end = text.rfind(')');

        if (comm_end == string::npos || comm_end + 2 >= text.size()) {
            return 0;
        }

        size_t fields_offset = comm_end + 2;
        string_view fields_text = string_view {text}.substr(fields_offset);
        vector<string_view> fields = strvex(fields_text).split(' ');

        if (fields.size() <= 12) {
            return 0;
        }

        uint64_t user_ticks = 0;
        uint64_t system_ticks = 0;

        if (!parse_uint64(fields[11], user_ticks)) {
            return 0;
        }

        if (!parse_uint64(fields[12], system_ticks)) {
            return 0;
        }

        long ticks_per_second = ::sysconf(_SC_CLK_TCK);

        if (ticks_per_second <= 0) {
            return 0;
        }

        return (user_ticks + system_ticks) * 1000000000ULL / static_cast<uint64_t>(ticks_per_second);
    }();

#elif FO_MAC
    natural_t processor_count = 0;
    processor_info_array_t raw_processor_info {};
    mach_msg_type_number_t processor_info_count = 0;

    if (::host_processor_info(::mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &processor_count, &raw_processor_info, &processor_info_count) == KERN_SUCCESS) {
        FO_VERIFY_AND_THROW(raw_processor_info != nullptr, "Processor info pointer is null");
        auto processor_info = make_ptr(raw_processor_info);
        auto load_info_data = processor_info.reinterpret_as<const processor_cpu_load_info_data_t>();
        auto load_info = make_span(load_info_data, processor_count);

        result.Cores.reserve(static_cast<size_t>(processor_count));

        for (natural_t i = 0; i < processor_count; i++) {
            processor_cpu_load_info_data_t& info = load_info[i];
            uint64_t user_time = static_cast<uint64_t>(info.cpu_ticks[CPU_STATE_USER]);
            uint64_t system_time = static_cast<uint64_t>(info.cpu_ticks[CPU_STATE_SYSTEM]);
            uint64_t idle_time = static_cast<uint64_t>(info.cpu_ticks[CPU_STATE_IDLE]);
            uint64_t nice_time = static_cast<uint64_t>(info.cpu_ticks[CPU_STATE_NICE]);

            result.Cores.emplace_back(CpuUsageCoreSnapshot {
                .IdleTime = idle_time,
                .TotalTime = user_time + system_time + idle_time + nice_time,
            });
        }

        (void)::vm_deallocate(::mach_task_self(), reinterpret_cast<vm_address_t>(raw_processor_info), processor_info_count * sizeof(integer_t));
    }

    result.LogicalCoreCount = static_cast<uint32_t>(result.Cores.size());

    result.ProcessTimeNs = []() noexcept -> uint64_t {
        mach_task_basic_info_data_t info {};
        mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
        auto task_info_data = make_ptr(&info).reinterpret_as<integer_t>();

        if (::task_info(::mach_task_self(), MACH_TASK_BASIC_INFO, task_info_data.get(), &count) != KERN_SUCCESS) {
            return 0;
        }

        // task_info time_value_t fields are seconds + microseconds.
        return static_cast<uint64_t>(info.user_time.seconds) * 1000000000ULL + static_cast<uint64_t>(info.user_time.microseconds) * 1000ULL + static_cast<uint64_t>(info.system_time.seconds) * 1000000000ULL + static_cast<uint64_t>(info.system_time.microseconds) * 1000ULL;
    }();
#endif

    return result;
}

auto Platform::LoadModule(u8string_view_nt module_name) -> nptr<void>
{
    FO_STACK_TRACE_ENTRY();

    nptr<void> module_handle = nullptr;
    const_span<char8_t> storage {module_name.c_str(), module_name.size() + 1};
    u8string_view_nt checked_module_name = u8string_view_nt::FromChecked(storage);
    u8string module_path {checked_module_name.view()};

#if FO_WINDOWS
    AppendModuleExtension(module_path, u8".dll");
    utf16_string module_path_utf16 = utf8_to_utf16(module_path.view());
    wstring module_path_wide = utf16_to_wide(std::u16string_view {module_path_utf16.data(), module_path_utf16.size()});
    ptr<const wchar_t> module_path_cstr {module_path_wide.c_str()};
    module_handle = ::LoadLibraryW(module_path_cstr.get());
#elif FO_LINUX
    AppendModuleExtension(module_path, u8".so");
    ptr<const char> module_path_cstr = utf8_to_c_str(module_path.view_nt());
    module_handle = ::dlopen(module_path_cstr.get(), RTLD_LAZY | RTLD_LOCAL);
#elif FO_MAC
    AppendModuleExtension(module_path, u8".dylib");
    ptr<const char> module_path_cstr = utf8_to_c_str(module_path.view_nt());
    module_handle = ::dlopen(module_path_cstr.get(), RTLD_LAZY | RTLD_LOCAL);
#else
    ignore_unused(module_path);
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

auto Platform::GetFuncAddr(nptr<void> module_handle, string_view_nt func_name) -> void*
{
    FO_STACK_TRACE_ENTRY();

    nptr<void> func = nullptr;
    ptr<const char> func_name_cstr {func_name.c_str()};

#if FO_WINDOWS
    func = WinApi_GetProcAddressRaw(module_handle, func_name_cstr);
#elif FO_LINUX || FO_MAC
    func = ::dlsym(module_handle ? module_handle.get() : RTLD_DEFAULT, func_name_cstr.get());
#else
    ignore_unused(func_name_cstr);
#endif

    return func.get();
}

FO_END_NAMESPACE
