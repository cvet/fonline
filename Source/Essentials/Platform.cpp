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

#include "Platform.h"
#include "Posix.h"
#include "StackTrace.h"
#include "StringUtils.h"
#include "WinApi.h"

#if FO_ANDROID
#include <android/log.h>
#endif

FO_BEGIN_NAMESPACE

void platform::info_log(const string& str) noexcept
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    winapi::output_debug_string(str);
#elif FO_ANDROID
    // Not a POSIX entry point but an Android SDK one, so it stays with the platform dispatcher
    auto message_cstr = make_ptr(str.c_str());
    __android_log_write(ANDROID_LOG_INFO, "FO", message_cstr.get());
#else
    ignore_unused(str);
#endif
}

void platform::set_thread_name(const string& str) noexcept
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    (void)winapi::set_thread_description(str);
#else
    ignore_unused(str);
#endif
}

auto platform::get_exe_path() noexcept -> optional<string>
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    return winapi::get_module_file_name();
#else
    return posix::get_executable_path();
#endif
}

auto platform::get_user_data_base() noexcept -> string
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    if (const char* local = std::getenv("LOCALAPPDATA"); local != nullptr && local[0] != 0) {
        return local;
    }
    if (const char* roaming = std::getenv("APPDATA"); roaming != nullptr && roaming[0] != 0) {
        return roaming;
    }
    return "";
#elif FO_MAC || FO_IOS
    if (const char* home = std::getenv("HOME"); home != nullptr && home[0] != 0) {
        return strex(home).combine_path("Library/Application Support").str();
    }
    return "";
#else
    if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg != nullptr && xdg[0] != 0) {
        return xdg;
    }
    if (const char* home = std::getenv("HOME"); home != nullptr && home[0] != 0) {
        return strex(home).combine_path(".local/share").str();
    }
    return "";
#endif
}

auto platform::fork_process() noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    return false;
#else
    return posix::fork_into_background();
#endif
}

auto platform::get_current_process_id_str() noexcept -> string
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    return strex("{}", winapi::get_current_process_id()).str();
#else
    return strex("{}", posix::get_current_process_id()).str();
#endif
}

auto platform::get_process_memory_usage() noexcept -> size_t
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    return winapi::get_process_working_set_size();
#else
    return posix::get_process_resident_size();
#endif
}

auto platform::get_process_private_memory_usage() noexcept -> size_t
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    return winapi::get_process_private_usage();
#else
    return posix::get_process_private_size();
#endif
}

auto platform::get_cpu_usage_snapshot() noexcept -> cpu_usage_snapshot
{
    FO_STACK_TRACE_ENTRY();

    cpu_usage_snapshot result;

#if FO_WINDOWS
    result.process_time_ns = winapi::get_process_cpu_time_ns().value_or(0);
    result.logical_core_count = winapi::get_active_processor_count();

    // Windows reports one system-wide figure rather than a row per core
    if (optional<winapi::cpu_core_times> system_times = winapi::get_system_cpu_times(); system_times.has_value()) {
        result.cores.emplace_back(cpu_usage_core_snapshot {
            .idle_time = system_times->idle_time,
            .total_time = system_times->total_time,
        });
    }
#else
    vector<posix::cpu_core_times> core_times = posix::get_system_cpu_times();

    result.cores.reserve(core_times.size());

    for (const posix::cpu_core_times& core : core_times) {
        result.cores.emplace_back(cpu_usage_core_snapshot {
            .idle_time = core.idle_time,
            .total_time = core.total_time,
        });
    }

    // The kernel lists one row per online core, so the rows are the count when it produced any
    result.logical_core_count = !result.cores.empty() ? static_cast<uint32_t>(result.cores.size()) : posix::get_logical_core_count();
    result.process_time_ns = posix::get_process_cpu_time_ns().value_or(0);
#endif

    return result;
}

auto platform::load_module(const string& module_name) noexcept -> nptr<void>
{
    FO_STACK_TRACE_ENTRY();

    auto add_extension = [](const string& path, string_view extension) -> string { //
        return path.ends_with(extension) ? path : strex(strex::safe_format, "{}{}", path, extension).str();
    };

#if FO_WINDOWS
    return winapi::load_library(add_extension(module_name, ".dll"));
#elif FO_MAC
    return posix::load_library(add_extension(module_name, ".dylib"));
#else
    return posix::load_library(add_extension(module_name, ".so"));
#endif
}

void platform::unload_module(nptr<void> module_handle) noexcept
{
    FO_STACK_TRACE_ENTRY();

    if (!module_handle) {
        return;
    }

#if FO_WINDOWS
    winapi::free_library(module_handle);
#else
    posix::free_library(module_handle);
#endif
}

auto platform::get_func_addr(nptr<void> module_handle, const string& func_name) noexcept -> void*
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    return winapi::get_proc_address(module_handle, func_name).get();
#else
    return posix::get_symbol_address(module_handle, func_name).get();
#endif
}

FO_END_NAMESPACE
