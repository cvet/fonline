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
#include "Application.h"
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

static std::thread::id MainThreadId;

static bool ExceptionMessageBox = false;

hstring::entry hstring::_zeroEntry;

map<ushort, std::function<InterthreadDataCallback(InterthreadDataCallback)>> InterthreadListeners;

GlobalDataCallback CreateGlobalDataCallbacks[MAX_GLOBAL_DATA_CALLBACKS];
GlobalDataCallback DeleteGlobalDataCallbacks[MAX_GLOBAL_DATA_CALLBACKS];
int GlobalDataCallbacksCount;

static constexpr size_t STACK_TRACE_MAX_SIZE = 128;

struct StackTraceData
{
    size_t CallsCount = {};
    array<const SourceLocationData*, STACK_TRACE_MAX_SIZE> CallTree = {};
};

static StackTraceData StackTrace;

void SetMainThread() noexcept
{
    MainThreadId = std::this_thread::get_id();
}

auto IsMainThread() noexcept -> bool
{
    return MainThreadId == std::this_thread::get_id();
}

void CreateGlobalData()
{
    STACK_TRACE_ENTRY();

    for (auto i = 0; i < GlobalDataCallbacksCount; i++) {
        CreateGlobalDataCallbacks[i]();
    }
}

void DeleteGlobalData()
{
    STACK_TRACE_ENTRY();

    for (auto i = 0; i < GlobalDataCallbacksCount; i++) {
        DeleteGlobalDataCallbacks[i]();
    }
}

void ReportExceptionAndExit(const std::exception& ex)
{
    STACK_TRACE_ENTRY();

    const auto* ex_info = dynamic_cast<const ExceptionInfo*>(&ex);

    if (ex_info != nullptr) {
        WriteLog(LogType::Error, "{}\n{}\nCatched at: {}\nShutdown!", ex.what(), ex_info->StackTrace(), GetStackTrace());
    }
    else {
        WriteLog(LogType::Error, "{}\nCatched at: {}\nShutdown!", ex.what(), GetStackTrace());
    }

    if (BreakIntoDebugger(ex.what())) {
        ExitApp(false);
    }

    CreateDumpMessage("FatalException", ex.what());

    if (ex_info != nullptr) {
        MessageBox::ShowErrorMessage("Error", ex.what(), ex_info->StackTrace());
    }
    else {
        MessageBox::ShowErrorMessage("Error", ex.what(), _str("Catched at: {}", GetStackTrace()));
    }

    ExitApp(false);
}

void ReportExceptionAndContinue(const std::exception& ex)
{
    STACK_TRACE_ENTRY();

    const auto* ex_info = dynamic_cast<const ExceptionInfo*>(&ex);

    if (ex_info != nullptr) {
        WriteLog(LogType::Error, "{}\n{}\nCatched at: {}", ex.what(), ex_info != nullptr ? ex_info->StackTrace() : "", GetStackTrace());
    }
    else {
        WriteLog(LogType::Error, "{}\nCatched at: {}", ex.what(), GetStackTrace());
    }

    if (BreakIntoDebugger(ex.what())) {
        return;
    }

    if (ExceptionMessageBox) {
        if (ex_info != nullptr) {
            MessageBox::ShowErrorMessage("Error", ex.what(), ex_info->StackTrace());
        }
        else {
            MessageBox::ShowErrorMessage("Error", ex.what(), _str("Catched at: {}", GetStackTrace()));
        }
    }
}

void ShowExceptionMessageBox(bool enabled)
{
    STACK_TRACE_ENTRY();

    ExceptionMessageBox = enabled;
}

void PushStackTrace(const SourceLocationData& loc) noexcept
{
    if (!IsMainThread()) {
        return;
    }

    if (StackTrace.CallsCount < STACK_TRACE_MAX_SIZE) {
        StackTrace.CallTree[StackTrace.CallsCount] = &loc;
    }

    StackTrace.CallsCount++;
}

void PopStackTrace() noexcept
{
    if (!IsMainThread()) {
        return;
    }

    if (StackTrace.CallsCount > 0) {
        StackTrace.CallsCount--;
    }
}

auto GetStackTrace() -> string
{
    if (!IsMainThread()) {
        return "Stack trace disabled for non main thread";
    }

    std::stringstream ss;

    ss << "Stack trace (most recent call first):\n";

    for (int i = std::min(static_cast<int>(StackTrace.CallsCount), static_cast<int>(STACK_TRACE_MAX_SIZE)) - 1; i >= 0; i--) {
        const auto& entry = StackTrace.CallTree[i];

        ss << "- " << entry->function << " (" << _str(entry->file).extractFileName().str() << " line " << entry->line << ")\n";
    }

    if (StackTrace.CallsCount > STACK_TRACE_MAX_SIZE) {
        ss << "- ..."
           << "and " << (StackTrace.CallsCount - STACK_TRACE_MAX_SIZE) << " more entries\n";
    }

    auto st_str = ss.str();

    if (!st_str.empty() && st_str.back() == '\n') {
        st_str.pop_back();
    }

    return st_str;
}

auto GetRealStackTrace() -> string
{
    if (IsRunInDebugger()) {
        return "Stack trace disabled (debugger detected)";
    }

#if FO_WINDOWS || FO_LINUX || FO_MAC
    backward::TraceResolver resolver;
    backward::StackTrace st;
    st.load_here(42);
    st.skip_n_firsts(2);

    std::stringstream ss;

    ss << "Stack trace (most recent call first):\n";

    for (size_t i = 0; i < st.size(); ++i) {
        backward::ResolvedTrace trace = resolver.resolve(st[i]);

        auto obj_func = trace.object_function;
        if (obj_func.length() > 100) {
            obj_func.resize(97);
            obj_func.append("...");
        }

        auto file_name = _str(trace.source.filename).extractFileName().str();
        if (!file_name.empty()) {
            file_name.append(" ");
        }

        file_name += _str("{}", trace.source.line).str();

        ss << "- " << obj_func << " (" << file_name << ")\n";
    }

    auto st_str = ss.str();

    if (!st_str.empty() && st_str.back() == '\n') {
        st_str.pop_back();
    }

    return st_str;
#else

    return "Stack trace not supported";
#endif
}

static bool RunInDebugger = false;
static std::once_flag RunInDebuggerOnce;

auto IsRunInDebugger() -> bool
{
    STACK_TRACE_ENTRY();

#if FO_WINDOWS
    std::call_once(RunInDebuggerOnce, [] { RunInDebugger = ::IsDebuggerPresent() != FALSE; });

#elif FO_LINUX
    std::call_once(RunInDebuggerOnce, [] {
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
                            RunInDebugger = ::isdigit(*s) != 0 && *s != '0';
                            break;
                        }
                    }
                }
            }
        }
    });

#elif FO_MAC
    std::call_once(RunInDebuggerOnce, [] {
        int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
        struct kinfo_proc info = {};
        size_t size = sizeof(info);
        if (::sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, nullptr, 0) == 0) {
            RunInDebugger = (info.kp_proc.p_flag & P_TRACED) != 0;
        }
    });

#else
    UNUSED_VARIABLE(RunInDebuggerOnce);
#endif

    return RunInDebugger;
}

auto BreakIntoDebugger([[maybe_unused]] string_view error_message) -> bool
{
    STACK_TRACE_ENTRY();

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
    STACK_TRACE_ENTRY();

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
