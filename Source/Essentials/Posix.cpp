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

#if !FO_WINDOWS
#include <fcntl.h>
#include <pwd.h>
#include <unistd.h>
#endif

#if !FO_WINDOWS && !FO_WEB
#include <sys/file.h>
#endif

#if FO_LINUX || FO_MAC
#include <dlfcn.h>
#include <pthread.h>
#include <signal.h>
#endif

#if FO_ANDROID
#include <unistd.h>
#endif

#if FO_WEB
#include <unistd.h>
#endif

#if FO_MAC || FO_IOS || FO_ANDROID
#include <stdlib.h>
#endif

#if FO_MAC
#include <libproc.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/processor_info.h>
#include <mach/task.h>
#include <mach/task_info.h>
#include <sys/proc.h>
#endif

#if !FO_WINDOWS

FO_BEGIN_NAMESPACE

#if FO_LINUX || FO_MAC
// Retires the sigaltstack registration before its pages are released: freeing first leaves the kernel aiming the
// signal stack at reclaimed memory, and an instrumented allocator then unmaps the same region twice
class alt_signal_stack_releaser final
{
public:
    void operator()(uint8_t* buffer) const noexcept
    {
        stack_t ss {};
        ss.ss_flags = SS_DISABLE;
        (void)::sigaltstack(&ss, nullptr);

        delete[] buffer;
    }
};

static void on_crash_signal(int32_t signum, siginfo_t* info, void* context);

// Signal dispositions belong to the process, so the handler they lead to lives as long as the process does
static std::atomic<posix::crash_signal_handler> installed_crash_signal_handler {};
#endif

// The kernel counters arrive as decimal text, and a field that is not a whole number is a field we misread
static auto parse_counter(string_view text, uint64_t& value) noexcept -> bool
{
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
#if FO_LINUX || FO_MAC || FO_ANDROID
    return static_cast<int32_t>(::getpid());
#else
    return 0;
#endif
}

auto posix::get_running_process_start_time(int32_t pid) noexcept -> optional<uint64_t>
{
    FO_TRACE_ZONE(Core);

    if (pid <= 0) {
        return std::nullopt;
    }

#if FO_LINUX || FO_ANDROID
    string stat_path = strex("/proc/{}/stat", pid).str();
    std::ifstream file {stat_path.c_str()};

    if (!file) {
        return std::nullopt;
    }

    string text;
    getline(file, text);

    // The command name is parenthesized and may itself contain spaces, so the fields start after its close:
    // the state is the first of them and the start time in clock ticks the twentieth
    size_t comm_end = text.rfind(')');

    if (comm_end == string::npos || comm_end + 2 >= text.size()) {
        return std::nullopt;
    }

    vector<string_view> fields = strvex(string_view {text}.substr(comm_end + 2)).split(' ');
    uint64_t start_time = 0;

    if (fields.size() <= 19 || fields[0] == "Z" || fields[0] == "X" || !parse_counter(fields[19], start_time)) {
        return std::nullopt;
    }

    return start_time;

#elif FO_MAC
    proc_bsdinfo info {};
    auto info_data = make_ptr(&info);

    if (::proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, info_data.get(), PROC_PIDTBSDINFO_SIZE) != PROC_PIDTBSDINFO_SIZE || info.pbi_status == SZOMB) {
        return std::nullopt;
    }

    return static_cast<uint64_t>(info.pbi_start_tvsec) * 1000000ULL + static_cast<uint64_t>(info.pbi_start_tvusec);

#else
    return std::nullopt;
#endif
}

auto posix::get_home_dir() noexcept -> optional<string>
{
    FO_TRACE_ZONE(Core);

#if !FO_WEB
    // The reentrant form: the shared one returns a pointer into storage another caller may replace
    passwd pwd {};
    passwd* result = nullptr;
    vector<char> buffer;
    buffer.resize(4096);

    if (::getpwuid_r(::getuid(), &pwd, buffer.data(), buffer.size(), &result) != 0 || result == nullptr) {
        return std::nullopt;
    }

    if (pwd.pw_dir == nullptr || pwd.pw_dir[0] == 0) {
        return std::nullopt;
    }

    return string {pwd.pw_dir};

#else
    return std::nullopt;
#endif
}

auto posix::get_executable_path() noexcept -> optional<string>
{
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
    FO_TRACE_ZONE(Core);

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
    FO_TRACE_ZONE(Core);

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
    FO_TRACE_ZONE(Core);

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
#if FO_LINUX || FO_MAC || FO_ANDROID
    long core_count = ::sysconf(_SC_NPROCESSORS_ONLN);

    return core_count > 0 ? static_cast<uint32_t>(core_count) : 1U;
#else
    return 1U;
#endif
}

auto posix::get_system_cpu_times() noexcept -> vector<posix::cpu_core_times>
{
    FO_TRACE_ZONE(Core);

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

auto posix::fill_system_random(span<uint8_t> buf) noexcept -> bool
{
#if FO_MAC || FO_IOS || FO_ANDROID
    // Seeded and reseeded by the kernel, and unlike getentropy present on every Android level the engine supports
    if (!buf.empty()) {
        ::arc4random_buf(buf.data(), buf.size());
    }

    return true;

#else
    // getentropy refuses a request above 256 bytes
    constexpr size_t max_request_size = 256;
    size_t offset = 0;

    while (offset < buf.size()) {
        size_t request_size = std::min(buf.size() - offset, max_request_size);

        if (::getentropy(buf.data() + offset, request_size) != 0) {
            return false;
        }

        offset += request_size;
    }

    return true;
#endif
}

// Pack offsets are 64-bit, so the file calls must be too. A 32-bit Android ABI keeps off_t at 32 bits and is
// reached by the wide names; every other platform we build already has a wide off_t, which the assert proves
#if FO_ANDROID
static constexpr int32_t LARGE_FILE_OPEN_FLAG = O_LARGEFILE;
#else
static_assert(sizeof(off_t) == 8, "off_t must carry a 64-bit file offset, or the wide calls must be named here");
static constexpr int32_t LARGE_FILE_OPEN_FLAG = 0;
#endif

static auto seek_file(int32_t fd, int64_t offset, int32_t whence) noexcept -> int64_t
{
#if FO_ANDROID
    return ::lseek64(fd, offset, whence);
#else
    return ::lseek(fd, offset, whence);
#endif
}

static auto read_file_offset(int32_t fd, uint64_t offset, ptr<uint8_t> buffer, size_t size) noexcept -> int64_t
{
    // pread carries its own offset, so concurrent readers of one descriptor never fight over a shared cursor
#if FO_ANDROID
    return ::pread64(fd, buffer.get(), size, static_cast<int64_t>(offset));
#else
    return ::pread(fd, buffer.get(), size, static_cast<int64_t>(offset));
#endif
}

static auto truncate_file_to(int32_t fd, uint64_t size) noexcept -> int32_t
{
#if FO_ANDROID
    return ::ftruncate64(fd, static_cast<int64_t>(size));
#else
    return ::ftruncate(fd, static_cast<int64_t>(size));
#endif
}

#if !FO_WEB
auto posix::open_exclusive_file(const string& path) noexcept -> int32_t
{
    FO_TRACE_ZONE(FileSystem);

    int32_t fd = ::open(path.c_str(), O_RDWR | O_CREAT | LARGE_FILE_OPEN_FLAG, 0666);

    if (fd < 0) {
        return -1;
    }

    // Windows denies the share at open time; here the lock is a separate step, so a failure closes again
    if (::flock(fd, LOCK_EX | LOCK_NB) != 0) {
        ::close(fd);
        return -1;
    }

    return fd;
}

void posix::close_exclusive_file(int32_t fd) noexcept
{
    (void)::flock(fd, LOCK_UN);
    (void)::close(fd);
}
#endif

auto posix::seek_file_end(int32_t fd) noexcept -> int64_t
{
    return seek_file(fd, 0, SEEK_END);
}

auto posix::seek_file_begin(int32_t fd) noexcept -> bool
{
    return seek_file(fd, 0, SEEK_SET) >= 0;
}

auto posix::read_file_chunk(int32_t fd, ptr<char> buffer, size_t size) noexcept -> int64_t
{
    FO_TRACE_ZONE(FileSystem);

    return ::read(fd, buffer.get(), size);
}

auto posix::write_file_chunk(int32_t fd, ptr<const char> data, size_t size) noexcept -> int64_t
{
    FO_TRACE_ZONE(FileSystem);

    return ::write(fd, data.get(), size);
}

auto posix::truncate_file(int32_t fd) noexcept -> bool
{
    FO_TRACE_ZONE(FileSystem);

    return truncate_file_to(fd, 0) == 0;
}

auto posix::sync_file(int32_t fd) noexcept -> bool
{
    FO_TRACE_ZONE(FileSystem);

    return ::fsync(fd) == 0;
}

auto posix::lock_directory(const string& path) noexcept -> int32_t
{
    int32_t fd = ::open(path.empty() ? "." : path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);

#if !FO_WEB
    if (fd >= 0 && ::flock(fd, LOCK_EX | LOCK_NB) != 0) {
        ::close(fd);
        return -1;
    }
#endif

    return fd;
}

auto posix::sync_directory(const string& path) noexcept -> bool
{
#if FO_WEB
    ignore_unused(path);
    return true;
#else
    int32_t fd = ::open(path.empty() ? "." : path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);

    if (fd < 0) {
        return false;
    }

    bool synced = ::fsync(fd) == 0;
    ::close(fd);
    return synced;
#endif
}

auto posix::open_shared_read_file(const string& path) noexcept -> int32_t
{
    return ::open(path.c_str(), O_RDONLY | O_CLOEXEC | LARGE_FILE_OPEN_FLAG);
}

auto posix::open_new_write_file(const string& path, bool append) noexcept -> int32_t
{
    int32_t fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_CLOEXEC | LARGE_FILE_OPEN_FLAG, 0666);

    if (fd < 0) {
        return -1;
    }

#if !FO_WEB
    if (::flock(fd, LOCK_EX | LOCK_NB) != 0) {
        ::close(fd);
        return -1;
    }
#endif

    if ((!append && truncate_file_to(fd, 0) != 0) || seek_file(fd, 0, SEEK_END) < 0) {
        ::close(fd);
        return -1;
    }

    return fd;
}

auto posix::resize_file(int32_t fd, uint64_t size) noexcept -> bool
{
    return size <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) && truncate_file_to(fd, size) == 0 && seek_file(fd, static_cast<int64_t>(size), SEEK_SET) >= 0;
}

void posix::close_file(int32_t fd) noexcept
{
    (void)::close(fd);
}

auto posix::get_file_size(int32_t fd) noexcept -> int64_t
{
    return seek_file(fd, 0, SEEK_END);
}

auto posix::read_file_at(int32_t fd, uint64_t offset, ptr<uint8_t> buffer, size_t size) noexcept -> int64_t
{
    return read_file_offset(fd, offset, buffer, size);
}

auto posix::preallocate_file(int32_t fd, uint64_t size) noexcept -> bool
{
    // Reserving the blocks is best effort: a filesystem that cannot do it reports so, and the size still has
    // to be set, which is what the caller actually depends on
#if FO_ANDROID
    if (::posix_fallocate64(fd, 0, static_cast<int64_t>(size)) == 0) {
        return true;
    }
#elif FO_LINUX
    if (::posix_fallocate(fd, 0, static_cast<int64_t>(size)) == 0) {
        return true;
    }
#endif

    return truncate_file_to(fd, size) == 0;
}

#if !FO_WEB
auto posix::run_process_capturing_output(const string& command, const function<void(string_view)>& on_output) -> int32_t
{
    FO_TRACE_ZONE(Core);

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
}
#endif

#if FO_LINUX || FO_MAC
auto posix::load_library(const string& path) noexcept -> nptr<void>
{
    FO_TRACE_ZONE(Core);

    auto path_cstr = make_ptr(path.c_str());
    return ::dlopen(path_cstr.get(), RTLD_LAZY | RTLD_LOCAL);
}

auto posix::load_pinned_library(const string& path) noexcept -> nptr<void>
{
    FO_TRACE_ZONE(Core);

    auto path_cstr = make_ptr(path.c_str());
    return ::dlopen(path_cstr.get(), RTLD_LAZY | RTLD_LOCAL | RTLD_NODELETE);
}

void posix::free_library(nptr<void> module_handle) noexcept
{
    FO_TRACE_ZONE(Core);

    (void)::dlclose(module_handle.get());
}

auto posix::get_symbol_address(nptr<void> module_handle, const string& symbol_name) noexcept -> nptr<void>
{
    auto symbol_cstr = make_ptr(symbol_name.c_str());
    return ::dlsym(module_handle ? module_handle.get() : RTLD_DEFAULT, symbol_cstr.get());
}
#endif

void posix::install_crash_signal_handlers(crash_signal_handler handler) noexcept
{
#if FO_LINUX || FO_MAC
    installed_crash_signal_handler.store(handler);

    constexpr std::array CRASH_SIGNALS = {
        SIGABRT,
        SIGBUS,
        SIGFPE,
        SIGILL,
        SIGQUIT,
        SIGSEGV,
        SIGSYS,
        SIGTRAP,
        SIGXCPU,
        SIGXFSZ,
#if FO_MAC
        SIGEMT,
#endif
    };

    for (int32_t signum : CRASH_SIGNALS) {
        struct sigaction action {};
        action.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_NODEFER | SA_RESETHAND;
        action.sa_sigaction = &on_crash_signal;
        (void)::sigfillset(&action.sa_mask);
        (void)::sigdelset(&action.sa_mask, signum);
        (void)::sigaction(signum, &action, nullptr);
    }

#else
    ignore_unused(handler);
#endif
}

void posix::install_crash_signal_stack() noexcept
{
#if FO_LINUX || FO_MAC
    // 2 MiB is well above what the crash handler (unwinding + symbol resolution) needs; the pages are
    // touched only during a crash, so the reservation stays lazily committed for a thread that never faults
    constexpr size_t stack_size = size_t {2} * 1024 * 1024;

    // The kernel drops the sigaltstack registration only when the thread ends — later than this destructor — so
    // the releaser retires it before the pages go back; std::unique_ptr, not the engine alias, supports array storage
    static thread_local std::unique_ptr<uint8_t[], alt_signal_stack_releaser> alt_stack_buffer;

    if (alt_stack_buffer) {
        return; // already installed on this thread
    }

    alt_stack_buffer = std::unique_ptr<uint8_t[], alt_signal_stack_releaser> {new (std::nothrow) uint8_t[stack_size]};

    if (!alt_stack_buffer) {
        return;
    }

    stack_t ss {};
    ss.ss_sp = alt_stack_buffer.get();
    ss.ss_size = stack_size;
    ss.ss_flags = 0;

    if (::sigaltstack(&ss, nullptr) != 0) {
        alt_stack_buffer.reset();
    }
#endif
}

#if FO_LINUX || FO_MAC
static void on_crash_signal(int32_t signum, siginfo_t* info, void* context)
{
    if (posix::crash_signal_handler handler = installed_crash_signal_handler.load()) {
        int32_t code = 0;
        nptr<const void> address;

        if (info != nullptr) {
            code = info->si_code;
            address = info->si_addr;
        }

        handler(signum, code, address, context);
    }

    // A script runtime that installed its own handler later chains to this one and still holds the signal, so the
    // default action is put back explicitly before the signal is raised again
    struct sigaction default_action {};
    default_action.sa_handler = SIG_DFL;
    (void)::sigemptyset(&default_action.sa_mask);
    (void)::sigaction(signum, &default_action, nullptr);

    sigset_t unblocked {};
    (void)::sigemptyset(&unblocked);
    (void)::sigaddset(&unblocked, signum);
    (void)::pthread_sigmask(SIG_UNBLOCK, &unblocked, nullptr);

    (void)::raise(signum);
    ::_exit(EXIT_FAILURE);
}
#endif

FO_END_NAMESPACE

#endif
