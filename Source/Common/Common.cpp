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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "Common.h"
#include "DiskFileSystem.h"
#include "Log.h"
#include "StringUtils.h"
#include "Timer.h"
#include "Version-Include.h"
#include "WinApi-Include.h"

#if FO_MAC
#include <sys/sysctl.h>
#endif

#if FO_WINDOWS || FO_LINUX || FO_MAC
#if !FO_WINDOWS
#if __has_include(<libunwind.h>)
#define BACKWARD_HAS_LIBUNWIND 1
#elif __has_include(<bfd.h>)
#define BACKWARD_HAS_BFD 1
#endif
#endif
#include "backward.hpp"
#if FO_WINDOWS
#undef MessageBox
#endif
#endif

hstring::entry hstring::_zeroEntry;

map<ushort, std::function<InterthreadDataCallback(InterthreadDataCallback)>> InterthreadListeners;

GlobalDataCallback CreateGlobalDataCallbacks[MAX_GLOBAL_DATA_CALLBACKS];
GlobalDataCallback DeleteGlobalDataCallbacks[MAX_GLOBAL_DATA_CALLBACKS];
int GlobalDataCallbacksCount;

void CreateGlobalData()
{
    for (auto i = 0; i < GlobalDataCallbacksCount; i++) {
        CreateGlobalDataCallbacks[i]();
    }
}

void DeleteGlobalData()
{
    for (auto i = 0; i < GlobalDataCallbacksCount; i++) {
        DeleteGlobalDataCallbacks[i]();
    }
}

auto GetStackTrace() -> string
{
    // Todo: apply scripts strack trace

#if FO_WINDOWS || FO_LINUX || FO_MAC
    backward::TraceResolver resolver;
    backward::StackTrace st;
    st.load_here();
    st.skip_n_firsts(2);
    backward::Printer printer;
    printer.snippet = false;
    std::stringstream ss;
    printer.print(st, ss);
    auto st_str = ss.str();
    return st_str.back() == '\n' ? st_str.substr(0, st_str.size() - 1) : st_str;
#else

    return "Stack trace: Not supported";
#endif
}

auto IsRunInDebugger() -> bool
{
    static bool run_in_debugger = false;

#if FO_WINDOWS
    static std::once_flag once;
    std::call_once(once, [] { run_in_debugger = ::IsDebuggerPresent() != FALSE; });

#elif FO_LINUX
    static std::once_flag once;
    std::call_once(once, [] {
        const auto status_fd = ::open("/proc/self/status", O_RDONLY);
        if (status_fd != -1) {
            char buf[4096] = {0};
            const auto num_read = ::read(status_fd, buf, sizeof(buf) - 1);
            ::close(status_fd);
            if (num_read > 0) {
                const auto* tracer_pid_str = ::strstr(buf, "TracerPid:");
                if (tracer_pid_str != nullptr) {
                    for (const char* s = tracer_pid_str + "TracerPid:"_len; s <= buf + num_read; ++s) {
                        if (::isspace(*s) == 0) {
                            run_in_debugger = ::isdigit(*s) != 0 && *s != '0';
                            break;
                        }
                    }
                }
            }
        }
    });

#elif FO_MAC
    static std::once_flag once;
    std::call_once(once, [] {
        int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
        struct kinfo_proc info = {};
        size_t size = sizeof(info);
        if (::sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, nullptr, 0) == 0) {
            run_in_debugger = (info.kp_proc.p_flag & P_TRACED) != 0;
        }
    });
#endif

    return run_in_debugger;
}

auto BreakIntoDebugger([[maybe_unused]] string_view error_message) -> bool
{
    if (IsRunInDebugger()) {
#if FO_WINDOWS
        ::DebugBreak();
        return true;
#elif FO_LINUX
#if __has_builtin(__builtin_debugtrap)
        __builtin_debugtrap();
#else
        ::raise(SIGTRAP);
#endif
        return true;
#elif FO_MAC
        __builtin_debugtrap();
        return true;
#endif
    }

    return false;
}

void CreateDumpMessage(string_view appendix, string_view message)
{
    const auto traceback = GetStackTrace();
    const auto dt = Timer::GetCurrentDateTime();
    const string fname = _str("{}_{}_{:04}.{:02}.{:02}_{:02}-{:02}-{:02}.txt", FO_DEV_NAME, appendix, dt.Year, dt.Month, dt.Day, dt.Hour, dt.Minute, dt.Second);

    if (auto file = DiskFileSystem::OpenFile(fname, true)) {
        file.Write(_str("{}\n", appendix));
        file.Write(_str("{}\n", message));
        file.Write(_str("\n"));
        file.Write(_str("Application\n"));
        file.Write(_str("\tName        {}\n", FO_DEV_NAME));
        file.Write(_str("\tVersion     {}\n", FO_GAME_VERSION));
        file.Write(_str("\tOS          Windows\n"));
        file.Write(_str("\tTimestamp   {:04}.{:02}.{:02} {:02}:{:02}:{:02}\n", dt.Year, dt.Month, dt.Day, dt.Hour, dt.Minute, dt.Second));
        file.Write(_str("\n"));
        file.Write(traceback);
        file.Write(_str("\n"));
    }
}

// Dummy symbols for web build to avoid linker errors
#if FO_WEB
void* SDL_LoadObject(const char* sofile)
{
    throw UnreachablePlaceException(LINE_STR);
}

void* SDL_LoadFunction(void* handle, const char* name)
{
    throw UnreachablePlaceException(LINE_STR);
}

void SDL_UnloadObject(void* handle)
{
    throw UnreachablePlaceException(LINE_STR);
}

void emscripten_sleep(unsigned int ms)
{
    throw UnreachablePlaceException(LINE_STR);
}
#endif
