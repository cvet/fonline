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

#include "WinApi.h"
#include "StackTrace.h"
#include "StringUtils.h"

#if FO_WINDOWS
#include <fcntl.h>
#include <io.h>
#include <share.h>
#endif

#if FO_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <psapi.h>
#endif
#include "WinApiUndef.inc"

#if FO_WINDOWS

FO_BEGIN_NAMESPACE

// WinApiUndef.inc removes the unsuffixed macro names, so every call below names the wide entry point it means

static auto resolve_kernel_entry(const char* func_name) noexcept -> FARPROC
{
    FO_NO_STACK_TRACE_ENTRY();

    auto module_name = make_ptr("kernel32.dll");
    HMODULE hmodule = ::GetModuleHandleA(module_name.get());

    if (hmodule == nullptr) {
        return nullptr;
    }

    auto proc_name = make_ptr(func_name);
    return ::GetProcAddress(hmodule, proc_name.get());
}

static auto to_module_handle(nptr<void> module_handle) noexcept -> HMODULE
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!module_handle) {
        return ::GetModuleHandleW(nullptr);
    }

    return module_handle.reinterpret_as<std::remove_pointer_t<HMODULE>>().get();
}

// FILETIME carries a count of 100 ns ticks split across two halves
static auto file_time_to_ticks(FILETIME time) noexcept -> uint64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    ULARGE_INTEGER value {};
    value.LowPart = time.dwLowDateTime;
    value.HighPart = time.dwHighDateTime;
    return value.QuadPart;
}

void winapi::output_debug_string(const string& text) noexcept
{
    FO_STACK_TRACE_ENTRY();

    wstring message = strex(text).to_wide_char();
    auto message_cstr = make_ptr(message.c_str());
    ::OutputDebugStringW(message_cstr.get());
}

auto winapi::set_thread_description(const string& name) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    using set_thread_description_fn = HRESULT(WINAPI*)(HANDLE, PCWSTR);
    auto entry = reinterpret_cast<set_thread_description_fn>(resolve_kernel_entry("SetThreadDescription")); // NOLINT(clang-diagnostic-cast-function-type-strict)

    if (entry == nullptr) {
        return false;
    }

    wstring thread_name = strex(name).to_wide_char();
    auto thread_name_cstr = make_ptr(thread_name.c_str());
    return SUCCEEDED(entry(::GetCurrentThread(), thread_name_cstr.get()));
}

auto winapi::get_current_process_id() noexcept -> uint32_t
{
    FO_STACK_TRACE_ENTRY();

    return ::GetCurrentProcessId();
}

auto winapi::get_module_file_name() noexcept -> optional<string>
{
    FO_STACK_TRACE_ENTRY();

    vector<wchar_t> path;
    path.resize(FILENAME_MAX);

    auto path_data = make_nptr(path.data());
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

    return strex().parse_wide_char(path_data.as_ptr()).str();
}

auto winapi::get_process_working_set_size() noexcept -> size_t
{
    FO_STACK_TRACE_ENTRY();

    PROCESS_MEMORY_COUNTERS pmc {};

    if (::GetProcessMemoryInfo(::GetCurrentProcess(), &pmc, sizeof(pmc)) != 0) {
        return static_cast<size_t>(pmc.WorkingSetSize);
    }

    return 0;
}

auto winapi::get_process_private_usage() noexcept -> size_t
{
    FO_STACK_TRACE_ENTRY();

    PROCESS_MEMORY_COUNTERS_EX pmc {};
    auto pmc_counters = make_ptr(&pmc).reinterpret_as<PROCESS_MEMORY_COUNTERS>();

    if (::GetProcessMemoryInfo(::GetCurrentProcess(), pmc_counters.get(), sizeof(pmc)) != 0) {
        return pmc.PrivateUsage;
    }

    return 0;
}

auto winapi::get_process_cpu_time_ns() noexcept -> optional<uint64_t>
{
    FO_STACK_TRACE_ENTRY();

    FILETIME creation_time {};
    FILETIME exit_time {};
    FILETIME kernel_time {};
    FILETIME user_time {};

    if (::GetProcessTimes(::GetCurrentProcess(), &creation_time, &exit_time, &kernel_time, &user_time) == 0) {
        return std::nullopt;
    }

    return (file_time_to_ticks(kernel_time) + file_time_to_ticks(user_time)) * 100;
}

auto winapi::get_active_processor_count() noexcept -> uint32_t
{
    FO_STACK_TRACE_ENTRY();

    DWORD processor_count = ::GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);

    return processor_count != 0 ? static_cast<uint32_t>(processor_count) : 1U;
}

auto winapi::get_system_cpu_times() noexcept -> optional<winapi::cpu_core_times>
{
    FO_STACK_TRACE_ENTRY();

    FILETIME idle_time {};
    FILETIME kernel_time {};
    FILETIME user_time {};

    if (::GetSystemTimes(&idle_time, &kernel_time, &user_time) == 0) {
        return std::nullopt;
    }

    return winapi::cpu_core_times {
        .idle_time = file_time_to_ticks(idle_time),
        .total_time = file_time_to_ticks(kernel_time) + file_time_to_ticks(user_time),
    };
}

auto winapi::load_library(const string& path) noexcept -> nptr<void>
{
    FO_STACK_TRACE_ENTRY();

    wstring path_wide = strex(path).to_wide_char();
    auto path_cstr = make_ptr(path_wide.c_str());

    return ::LoadLibraryW(path_cstr.get());
}

void winapi::free_library(nptr<void> module_handle) noexcept
{
    FO_STACK_TRACE_ENTRY();

    (void)::FreeLibrary(to_module_handle(module_handle));
}

auto winapi::get_proc_address(nptr<void> module_handle, const string& func_name) noexcept -> nptr<void>
{
    FO_STACK_TRACE_ENTRY();

    auto name_cstr = make_ptr(func_name.c_str());
    FARPROC proc = ::GetProcAddress(to_module_handle(module_handle), name_cstr.get());

    if (proc == nullptr) {
        return nullptr;
    }

    return reinterpret_cast<void*>(proc);
}

auto winapi::open_exclusive_file(const string& path) noexcept -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    int32_t fd = -1;

    if (::_sopen_s(&fd, path.c_str(), _O_BINARY | _O_RDWR | _O_CREAT, _SH_DENYRW, _S_IREAD | _S_IWRITE) != 0) {
        return -1;
    }

    return fd;
}

void winapi::close_exclusive_file(int32_t fd) noexcept
{
    FO_STACK_TRACE_ENTRY();

    (void)::_close(fd);
}

auto winapi::seek_file_end(int32_t fd) noexcept -> int64_t
{
    FO_STACK_TRACE_ENTRY();

    return ::_lseeki64(fd, 0, SEEK_END);
}

auto winapi::seek_file_begin(int32_t fd) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    return ::_lseeki64(fd, 0, SEEK_SET) >= 0;
}

auto winapi::read_file_chunk(int32_t fd, ptr<char> buffer, size_t size) noexcept -> int64_t
{
    FO_STACK_TRACE_ENTRY();

    auto chunk = static_cast<unsigned int>(std::min(size, static_cast<size_t>(std::numeric_limits<int>::max())));

    return ::_read(fd, buffer.get(), chunk);
}

auto winapi::write_file_chunk(int32_t fd, ptr<const char> data, size_t size) noexcept -> int64_t
{
    FO_STACK_TRACE_ENTRY();

    auto chunk = static_cast<unsigned int>(std::min(size, static_cast<size_t>(std::numeric_limits<int>::max())));

    return ::_write(fd, data.get(), chunk);
}

auto winapi::truncate_file(int32_t fd) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    return ::_chsize_s(fd, 0) == 0;
}

auto winapi::sync_file(int32_t fd) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    return ::_commit(fd) == 0;
}

auto winapi::run_process_capturing_output(const string& command, const function<void(string_view)>& on_output) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    HANDLE out_read = nullptr;
    HANDLE out_write = nullptr;

    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    if (::CreatePipe(&out_read, &out_write, &sa, 0) == 0) {
        return -1;
    }

    auto pipe_guard = scope_exit([out_read, out_write]() noexcept {
        ::CloseHandle(out_read);
        ::CloseHandle(out_write);
    });

    if (::SetHandleInformation(out_read, HANDLE_FLAG_INHERIT, 0) == 0) {
        return -1;
    }

    STARTUPINFOW si = {};
    si.cb = sizeof(STARTUPINFO);
    si.hStdError = out_write;
    si.hStdOutput = out_write;
    si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};

    wstring wcommand = strex(command).to_wide_char();
    auto started = ::CreateProcessW(nullptr, wcommand.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi);

    if (started == 0) {
        return -1;
    }

    auto process_guard = scope_exit([pi]() noexcept {
        ::CloseHandle(pi.hProcess);
        ::CloseHandle(pi.hThread);
    });

    bool process_done = false;

    while (true) {
        while (true) {
            DWORD bytes = 0;

            if (::PeekNamedPipe(out_read, nullptr, 0, nullptr, &bytes, nullptr) == 0) {
                break;
            }

            if (bytes == 0) {
                break;
            }

            char buf[4096] = {};

            if (::ReadFile(out_read, buf, sizeof(buf), &bytes, nullptr) != 0) {
                on_output(string_view {buf, bytes});
            }
        }

        // Drain once more after the exit is observed: a fast command can write everything and terminate
        // between the last peek and this check, leaving its output buffered in the pipe
        if (process_done) {
            break;
        }

        if (::WaitForSingleObject(pi.hProcess, 1) != WAIT_TIMEOUT) {
            process_done = true;
        }
    }

    DWORD retval = 0;
    ::GetExitCodeProcess(pi.hProcess, &retval);

    return std::bit_cast<int32_t>(retval);
}

auto winapi::registry_read_value(const string& sub_key, const string& name) noexcept -> optional<string>
{
    FO_STACK_TRACE_ENTRY();

    HKEY hkey {};

    if (::RegOpenKeyExA(HKEY_CURRENT_USER, sub_key.c_str(), 0, KEY_READ, &hkey) != ERROR_SUCCESS) {
        return std::nullopt;
    }

    auto close_key = scope_exit([hkey]() noexcept { ::RegCloseKey(hkey); });
    DWORD type = 0;
    DWORD size = 0;

    if (::RegQueryValueExA(hkey, name.c_str(), nullptr, &type, nullptr, &size) != ERROR_SUCCESS || type != REG_SZ || size == 0) {
        return std::nullopt;
    }

    string value;
    value.resize(size);

    if (::RegQueryValueExA(hkey, name.c_str(), nullptr, nullptr, reinterpret_cast<LPBYTE>(value.data()), &size) != ERROR_SUCCESS) {
        return std::nullopt;
    }

    // REG_SZ counts its terminating null in the byte count, so dropping it recovers the original string
    if (!value.empty() && value.back() == '\0') {
        value.pop_back();
    }

    return value;
}

auto winapi::registry_write_value(const string& sub_key, const string& name, const string& value) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    HKEY hkey {};

    if (::RegCreateKeyExA(HKEY_CURRENT_USER, sub_key.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hkey, nullptr) != ERROR_SUCCESS) {
        return false;
    }

    auto close_key = scope_exit([hkey]() noexcept { ::RegCloseKey(hkey); });
    DWORD size = static_cast<DWORD>(value.size() + 1);

    return ::RegSetValueExA(hkey, name.c_str(), 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()), size) == ERROR_SUCCESS;
}

void winapi::registry_delete_value(const string& sub_key, const string& name) noexcept
{
    FO_STACK_TRACE_ENTRY();

    HKEY hkey {};

    if (::RegOpenKeyExA(HKEY_CURRENT_USER, sub_key.c_str(), 0, KEY_WRITE, &hkey) != ERROR_SUCCESS) {
        return;
    }

    auto close_key = scope_exit([hkey]() noexcept { ::RegCloseKey(hkey); });
    (void)::RegDeleteValueA(hkey, name.c_str());
}

auto winapi::create_high_resolution_timer() noexcept -> nptr<void>
{
    FO_STACK_TRACE_ENTRY();

    // The high resolution flag needs Windows 10 1803; without it the timer still beats the sleep tick
    HANDLE timer = ::CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);

    if (timer == nullptr) {
        timer = ::CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS);
    }

    return timer;
}

auto winapi::set_relative_timer(nptr<void> timer, int64_t delay_100ns) noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    // A negative due time is what makes the deadline relative instead of absolute
    LARGE_INTEGER due_time;
    due_time.QuadPart = -delay_100ns;

    return ::SetWaitableTimer(timer.get(), &due_time, 0, nullptr, nullptr, FALSE) != FALSE;
}

void winapi::wait_for_object(nptr<void> handle) noexcept
{
    FO_STACK_TRACE_ENTRY();

    (void)::WaitForSingleObject(handle.get(), INFINITE);
}

void winapi::close_handle(nptr<void> handle) noexcept
{
    FO_STACK_TRACE_ENTRY();

    (void)::CloseHandle(handle.get());
}

FO_END_NAMESPACE

#endif
