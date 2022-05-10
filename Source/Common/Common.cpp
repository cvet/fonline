//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

static string* AppName;
GlobalDataCallback CreateGlobalDataCallbacks[MAX_GLOBAL_DATA_CALLBACKS];
GlobalDataCallback DeleteGlobalDataCallbacks[MAX_GLOBAL_DATA_CALLBACKS];
int GlobalDataCallbacksCount;

void InitApp(string_view name)
{
    assert(AppName == nullptr);

#if FO_WINDOWS || FO_LINUX || FO_MAC
    {
        [[maybe_unused]] static backward::SignalHandling sh;
        assert(sh.loaded());
    }
#endif

    AppName = new string();
    AppName->append(FO_DEV_NAME);
    if (!name.empty()) {
        AppName->append("_");
        AppName->append(name);
    }

    CreateGlobalData();
    LogToFile();
}

auto GetAppName() -> const string&
{
    assert(AppName != nullptr);
    return *AppName;
}

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

    if (AppName != nullptr) {
        delete AppName;
        AppName = nullptr;
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

bool BreakIntoDebugger()
{
#if FO_WINDOWS
    if (::IsDebuggerPresent() != FALSE) {
        ::DebugBreak();
        return true;
    }
#endif

    return false;
}

void ReportExceptionAndExit(const std::exception& ex)
{
    if (!BreakIntoDebugger()) {
        WriteLog(LogType::Error, "\n{}\n", ex.what());
        CreateDumpMessage("FatalException", ex.what());
        MessageBox::ShowErrorMessage("Fatal Error", ex.what(), GetStackTrace());
    }

    std::quick_exit(EXIT_FAILURE);
}

void ReportExceptionAndContinue(const std::exception& ex)
{
    if (BreakIntoDebugger()) {
        return;
    }

    WriteLog(LogType::Error, "\n{}\n", ex.what());

#if FO_DEBUG
    MessageBox::ShowErrorMessage("Error", ex.what(), GetStackTrace());
#endif
}

void CreateDumpMessage(string_view appendix, string_view message)
{
    const auto traceback = GetStackTrace();
    const auto dt = Timer::GetCurrentDateTime();
    const string fname = _str("{}_{}_{:04}.{:02}.{:02}_{:02}-{:02}-{:02}.txt", appendix, FO_DEV_NAME, dt.Year, dt.Month, dt.Day, dt.Hour, dt.Minute, dt.Second);

    if (auto file = DiskFileSystem::OpenFile(fname, true)) {
        file.Write(_str("{}\n", appendix));
        file.Write(_str("{}\n", message));
        file.Write(_str("\n"));
        file.Write(_str("Application\n"));
        file.Write(_str("\tName        {}\n", GetAppName()));
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
