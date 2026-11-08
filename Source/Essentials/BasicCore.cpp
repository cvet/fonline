// FOnline Engine
// SPDX-License-Identifier: MIT

#include "BasicCore.h"

#if FO_LINUX
#include <stdlib.h>
#include <sys/auxv.h>
#include <unistd.h>
#endif

#if FO_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif
#include "WinApiUndef.inc"

#if FO_MAC
#include <sys/sysctl.h>
#include <unistd.h>
#endif

FO_BEGIN_NAMESPACE

static bool run_in_debugger {false};
static std::once_flag run_in_debugger_once {};

void exit_app(bool success) noexcept
{
    int32_t code {success ? EXIT_SUCCESS : EXIT_FAILURE};

#if !FO_WEB && !FO_MAC && !FO_IOS && !FO_ANDROID
    std::quick_exit(code);
#else
    std::exit(code);
#endif
}

auto is_run_in_debugger() noexcept -> bool
{
#if FO_WINDOWS
    std::call_once(run_in_debugger_once, [] { run_in_debugger = ::IsDebuggerPresent() != FALSE; });

#elif FO_LINUX
    std::call_once(run_in_debugger_once, [] {
        std::ifstream status("/proc/self/status");

        if (status.is_open()) {
            std::string line {};

            while (std::getline(status, line)) {
                if (line.starts_with("TracerPid:")) {
                    std::istringstream iss(line.substr("TracerPid:"_len));
                    std::string value {};
                    iss >> value;

                    if (!value.empty() && std::all_of(value.begin(), value.end(), ::isdigit)) {
                        run_in_debugger = value != "0";
                    }

                    break;
                }
            }
        }
    });

#elif FO_MAC
    std::call_once(run_in_debugger_once, [] {
        int32_t mib[4] {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
        struct kinfo_proc info {};
        size_t size {sizeof(info)};
        if (::sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, nullptr, 0) == 0) {
            run_in_debugger = (info.kp_proc.p_flag & P_TRACED) != 0;
        }
    });

#else
    ignore_unused(run_in_debugger_once);
#endif

    return run_in_debugger;
}

auto break_into_debugger() noexcept -> bool
{
    if (is_run_in_debugger()) {
#if FO_WINDOWS
        ::DebugBreak();
        return true;
#elif FO_LINUX
#ifdef __has_builtin
#if __has_builtin(__builtin_debugtrap)
        __builtin_debugtrap();
#else
        std::raise(SIGTRAP);
#endif
#else
        std::raise(SIGTRAP);
#endif
        return true;
#elif FO_MAC
        __builtin_debugtrap();
        return true;
#endif
    }

    return false;
}

auto itoa(int64_t num, char buf[64], int32_t base) noexcept -> const char*
{
    int32_t i {0};
    bool is_negative {false};
    uint64_t unsigned_num {0};

    if (num == 0) {
        buf[i++] = '0';
        buf[i] = '\0';
        return buf;
    }

    if (num < 0 && base == 10) {
        is_negative = true;
        unsigned_num = static_cast<uint64_t>(-(num + 1)) + 1;

        while (unsigned_num != 0) {
            uint64_t rem {unsigned_num % static_cast<uint64_t>(base)};
            buf[i++] = static_cast<char>(rem > 9 ? rem - 10 + 'a' : rem + '0');
            unsigned_num /= static_cast<uint64_t>(base);
        }
    }
    else {
        while (num != 0) {
            int64_t rem {num % base};
            buf[i++] = static_cast<char>(rem > 9 ? rem - 10 + 'a' : rem + '0');
            num = num / base;
        }
    }

    if (is_negative) {
        buf[i++] = '-';
    }

    buf[i] = '\0';

    int32_t start {0};
    int32_t end {i - 1};

    while (start < end) {
        char ch {buf[start]};
        buf[start] = buf[end];
        buf[end] = ch;
        end--;
        start++;
    }

    return buf;
}

#if FO_LINUX
auto details::linux_executable_path() noexcept -> std::optional<std::string>
{
    char path[FILENAME_MAX] {};
    auto program_headers {::getauxval(AT_PHDR)};
    FILE* mappings {::fopen("/proc/self/maps", "r")};

    if (program_headers != 0 && mappings) {
        char line[FILENAME_MAX + 128] {};

        while (::fgets(line, sizeof(line), mappings) != nullptr) {
            unsigned long begin {};
            unsigned long end {};

            if (::sscanf(line, "%lx-%lx", &begin, &end) == 2 && begin <= program_headers && program_headers < end) {
                char mapping_path[128] {};
                ::snprintf(mapping_path, sizeof(mapping_path), "/proc/self/map_files/%lx-%lx", begin, end);
                ssize_t mapped_size {::readlink(mapping_path, path, sizeof(path) - 1)};

                if (mapped_size >= 0) {
                    ::fclose(mappings);
                    path[static_cast<size_t>(mapped_size)] = 0;
                    return std::string {path};
                }

                break;
            }
        }
    }

    if (mappings) {
        ::fclose(mappings);
    }

    const char* exec_name {reinterpret_cast<const char*>(::getauxval(AT_EXECFN))};

    // A mapped program header identifies the active image even after a rename. If map_files is restricted,
    // AT_EXECFN identifies an explicitly invoked loader's application; ordinary dynamic programs retain the kernel path
    if (::getauxval(AT_BASE) == 0 && exec_name && exec_name[0] == '/' && ::realpath(exec_name, path) != nullptr) {
        return std::string {path};
    }

    ssize_t size {::readlink("/proc/self/exe", path, sizeof(path) - 1)};

    if (size == -1) {
        return std::nullopt;
    }

    path[static_cast<size_t>(size)] = 0;
    return std::string {path};
}

#endif

FO_END_NAMESPACE
