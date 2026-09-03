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

#include "Posix.h"
#include "FatalError.h"
#include "StackTrace.h"
#include "StringUtils.h"

#if !FO_WINDOWS && !FO_WEB
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

#if FO_LINUX || FO_MAC
#include <dlfcn.h>
#endif

#if FO_ANDROID
#include <unistd.h>
#endif

#if FO_MAC
#include <libproc.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/processor_info.h>
#include <mach/task.h>
#include <mach/task_info.h>
#endif

#if !FO_WINDOWS

FO_BEGIN_NAMESPACE

// The kernel counters arrive as decimal text, and a field that is not a whole number is a field we misread
static auto parse_counter(string_view text, uint64_t& value) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (text.empty()) {
        return false;
    }

    auto text_begin = make_nptr(text.data());
    ptr<const char> text_end = text_begin.offset(text.size());
    auto parse_result = std::from_chars(text_begin.get(), text_end.get(), value);

    return parse_result.ec == std::errc {} && text_end == parse_result.ptr;
}

auto posix::get_current_process_id() noexcept -> int32_t
{
    FO_STACK_TRACE_ENTRY();

#if FO_LINUX || FO_MAC || FO_ANDROID
    return static_cast<int32_t>(::getpid());
#else
    return 0;
#endif
}

auto posix::get_executable_path() noexcept -> optional<string>
{
    FO_STACK_TRACE_ENTRY();

#if FO_LINUX
    char path[FILENAME_MAX];
    auto path_data = make_ptr(path);
    ssize_t size = ::readlink("/proc/self/exe", path_data.get(), sizeof(path) - 1);

    if (size == -1) {
        return std::nullopt;
    }

    path_data[static_cast<size_t>(size)] = 0;
    return string {path_data.get()};

#elif FO_MAC
    char path[PROC_PIDPATHINFO_MAXSIZE];
    auto path_data = make_ptr(path);

    auto pid = ::getpid();

    if (::proc_pidpath(pid, path_data.get(), sizeof(path)) <= 0) {
        return std::nullopt;
    }

    return string {path_data.get()};

#else
    return std::nullopt;
#endif
}

auto posix::fork_into_background() noexcept -> bool // NOLINT(clang-diagnostic-missing-noreturn)
{
    FO_STACK_TRACE_ENTRY();

#if FO_LINUX || FO_MAC
    pid_t pid = ::fork();

    if (pid < 0) {
        return false;
    }
    else if (pid != 0) {
        exit_app(true);
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

auto posix::get_process_resident_size() noexcept -> size_t
{
    FO_STACK_TRACE_ENTRY();

#if FO_LINUX || FO_ANDROID
    // /proc/self/statm: size resident shared text lib data dt, in pages
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

auto posix::get_process_private_size() noexcept -> size_t
{
    FO_STACK_TRACE_ENTRY();

#if FO_LINUX || FO_ANDROID
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

auto posix::get_process_cpu_time_ns() noexcept -> optional<uint64_t>
{
    FO_STACK_TRACE_ENTRY();

#if FO_LINUX || FO_ANDROID
    std::ifstream file {"/proc/self/stat"};

    if (!file) {
        return std::nullopt;
    }

    string text;
    getline(file, text);

    // The command name is parenthesized and may itself contain spaces, so the fields start after its close
    size_t comm_end = text.rfind(')');

    if (comm_end == string::npos || comm_end + 2 >= text.size()) {
        return std::nullopt;
    }

    string_view fields_text = string_view {text}.substr(comm_end + 2);
    vector<string_view> fields = strvex(fields_text).split(' ');

    if (fields.size() <= 12) {
        return std::nullopt;
    }

    uint64_t user_ticks = 0;
    uint64_t system_ticks = 0;

    if (!parse_counter(fields[11], user_ticks) || !parse_counter(fields[12], system_ticks)) {
        return std::nullopt;
    }

    long ticks_per_second = ::sysconf(_SC_CLK_TCK);

    if (ticks_per_second <= 0) {
        return std::nullopt;
    }

    return (user_ticks + system_ticks) * 1000000000ULL / static_cast<uint64_t>(ticks_per_second);

#elif FO_MAC
    mach_task_basic_info_data_t info {};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    auto task_info_data = make_ptr(&info).reinterpret_as<integer_t>();

    if (::task_info(::mach_task_self(), MACH_TASK_BASIC_INFO, task_info_data.get(), &count) != KERN_SUCCESS) {
        return std::nullopt;
    }

    // task_info time_value_t fields are seconds plus microseconds
    return static_cast<uint64_t>(info.user_time.seconds) * 1000000000ULL + static_cast<uint64_t>(info.user_time.microseconds) * 1000ULL + static_cast<uint64_t>(info.system_time.seconds) * 1000000000ULL + static_cast<uint64_t>(info.system_time.microseconds) * 1000ULL;

#else
    return std::nullopt;
#endif
}

auto posix::get_logical_core_count() noexcept -> uint32_t
{
    FO_STACK_TRACE_ENTRY();

#if FO_LINUX || FO_MAC || FO_ANDROID
    long core_count = ::sysconf(_SC_NPROCESSORS_ONLN);

    return core_count > 0 ? static_cast<uint32_t>(core_count) : 1U;
#else
    return 1U;
#endif
}

auto posix::get_system_cpu_times() noexcept -> vector<posix::cpu_core_times>
{
    FO_STACK_TRACE_ENTRY();

    vector<posix::cpu_core_times> result;

#if FO_LINUX || FO_ANDROID
    // /proc/stat per-CPU counters appear in kernel order:
    //   user nice system idle iowait irq softirq steal guest guest_nice
    constexpr size_t MAX_CPU_FIELDS = 10;
    constexpr size_t IDLE_FIELD_INDEX = 3;
    constexpr size_t IOWAIT_FIELD_INDEX = 4;
    constexpr size_t TOTAL_TIME_FIELD_COUNT = 8; // user..steal; guest/guest_nice are folded into user/nice

    std::ifstream stat_file {"/proc/stat"};

    if (!stat_file) {
        return result;
    }

    string line;

    while (getline(stat_file, line)) {
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

            if (!parse_counter(field, value)) {
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

        result.emplace_back(posix::cpu_core_times {
            .idle_time = values[IDLE_FIELD_INDEX] + (values_count > IOWAIT_FIELD_INDEX ? values[IOWAIT_FIELD_INDEX] : 0),
            .total_time = total_time,
        });
    }

#elif FO_MAC
    natural_t processor_count = 0;
    processor_info_array_t raw_processor_info {};
    mach_msg_type_number_t processor_info_count = 0;

    if (::host_processor_info(::mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &processor_count, &raw_processor_info, &processor_info_count) == KERN_SUCCESS) {
        FO_BASIC_STRONG_ASSERT(raw_processor_info != nullptr);
        auto processor_info = make_ptr(raw_processor_info);
        auto load_info_data = processor_info.reinterpret_as<const processor_cpu_load_info_data_t>();
        const_span<processor_cpu_load_info_data_t> load_info {load_info_data.get(), static_cast<size_t>(processor_count)};

        result.reserve(static_cast<size_t>(processor_count));

        for (natural_t i = 0; i < processor_count; i++) {
            const processor_cpu_load_info_data_t& info = load_info[i];
            uint64_t user_time = static_cast<uint64_t>(info.cpu_ticks[CPU_STATE_USER]);
            uint64_t system_time = static_cast<uint64_t>(info.cpu_ticks[CPU_STATE_SYSTEM]);
            uint64_t idle_time = static_cast<uint64_t>(info.cpu_ticks[CPU_STATE_IDLE]);
            uint64_t nice_time = static_cast<uint64_t>(info.cpu_ticks[CPU_STATE_NICE]);

            result.emplace_back(posix::cpu_core_times {
                .idle_time = idle_time,
                .total_time = user_time + system_time + idle_time + nice_time,
            });
        }

        (void)::vm_deallocate(::mach_task_self(), reinterpret_cast<vm_address_t>(raw_processor_info), processor_info_count * sizeof(integer_t));
    }
#endif

    return result;
}

auto posix::open_exclusive_file(const string& path) noexcept -> int32_t
{
    FO_STACK_TRACE_ENTRY();

#if !FO_WEB
    int32_t fd = ::open(path.c_str(), O_RDWR | O_CREAT, 0666);

    if (fd < 0) {
        return -1;
    }

    // Windows denies the share at open time; here the lock is a separate step, so a failure closes again
    if (::flock(fd, LOCK_EX | LOCK_NB) != 0) {
        ::close(fd);
        return -1;
    }

    return fd;
#else
    ignore_unused(path);
    return -1;
#endif
}

void posix::close_exclusive_file(int32_t fd) noexcept
{
    FO_STACK_TRACE_ENTRY();

#if !FO_WEB
    (void)::flock(fd, LOCK_UN);
    (void)::close(fd);
#else
    ignore_unused(fd);
#endif
}

auto posix::seek_file_end(int32_t fd) noexcept -> int64_t
{
    FO_STACK_TRACE_ENTRY();

#if !FO_WEB
    return ::lseek(fd, 0, SEEK_END);
#else
    ignore_unused(fd);
    return -1;
#endif
}

auto posix::seek_file_begin(int32_t fd) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

#if !FO_WEB
    return ::lseek(fd, 0, SEEK_SET) >= 0;
#else
    ignore_unused(fd);
    return false;
#endif
}

auto posix::read_file_chunk(int32_t fd, ptr<char> buffer, size_t size) noexcept -> int64_t
{
    FO_STACK_TRACE_ENTRY();

#if !FO_WEB
    return ::read(fd, buffer.get(), size);
#else
    ignore_unused(fd);
    ignore_unused(buffer);
    ignore_unused(size);
    return -1;
#endif
}

auto posix::write_file_chunk(int32_t fd, ptr<const char> data, size_t size) noexcept -> int64_t
{
    FO_STACK_TRACE_ENTRY();

#if !FO_WEB
    return ::write(fd, data.get(), size);
#else
    ignore_unused(fd);
    ignore_unused(data);
    ignore_unused(size);
    return -1;
#endif
}

auto posix::truncate_file(int32_t fd) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

#if !FO_WEB
    return ::ftruncate(fd, 0) == 0;
#else
    ignore_unused(fd);
    return false;
#endif
}

auto posix::sync_file(int32_t fd) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

#if !FO_WEB
    return ::fsync(fd) == 0;
#else
    ignore_unused(fd);
    return false;
#endif
}

auto posix::run_process_capturing_output(const string& command, const function<void(string_view)>& on_output) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

#if !FO_WEB
    auto command_cstr = make_ptr(command.c_str());
    auto in = make_nptr(::popen(command_cstr.get(), "r"));

    if (!in) {
        return -1;
    }

    auto pipe_guard = scope_fail([&in]() noexcept { (void)::pclose(in.get()); });

    char buf[4096];

    while (std::fgets(buf, sizeof(buf), in.get()) != nullptr) {
        on_output(string_view {buf});
    }

    pipe_guard.release();
    return ::pclose(in.get());
#else
    ignore_unused(command);
    ignore_unused(on_output);
    return 1;
#endif
}

auto posix::load_library(const string& path) noexcept -> nptr<void>
{
    FO_STACK_TRACE_ENTRY();

#if FO_LINUX || FO_MAC
    auto path_cstr = make_ptr(path.c_str());
    return ::dlopen(path_cstr.get(), RTLD_LAZY | RTLD_LOCAL);
#else
    ignore_unused(path);
    return nullptr;
#endif
}

void posix::free_library(nptr<void> module_handle) noexcept
{
    FO_STACK_TRACE_ENTRY();

#if FO_LINUX || FO_MAC
    (void)::dlclose(module_handle.get());
#else
    ignore_unused(module_handle);
#endif
}

auto posix::get_symbol_address(nptr<void> module_handle, const string& symbol_name) noexcept -> nptr<void>
{
    FO_STACK_TRACE_ENTRY();

#if FO_LINUX || FO_MAC
    auto symbol_cstr = make_ptr(symbol_name.c_str());
    return ::dlsym(module_handle ? module_handle.get() : RTLD_DEFAULT, symbol_cstr.get());
#else
    ignore_unused(module_handle);
    ignore_unused(symbol_name);
    return nullptr;
#endif
}

FO_END_NAMESPACE

#endif
