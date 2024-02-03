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
// Copyright (c) 2006 - 2023, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "Log.h"
#include "StringUtils.h"
#include "Version-Include.h"

#if FO_WINDOWS
#include "WinApi-Include.h"
#endif

#if FO_LINUX || FO_MAC
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#if FO_MAC
#include <libproc.h>
#endif

#if FO_ANDROID
#include <android/log.h>
#endif

#if FO_WINDOWS
template<typename T>
static auto WinApi_GetProcAddress(const char* mod, const char* name) -> T
{
    if (auto* hmod = ::GetModuleHandleA(mod); hmod != nullptr) {
        return reinterpret_cast<T>(::GetProcAddress(hmod, name)); // NOLINT(clang-diagnostic-cast-function-type-strict)
    }

    return nullptr;
}
#endif

void Platform::InfoLog(const string& str)
{
#if FO_WINDOWS
    ::OutputDebugStringW(_str(str).toWideChar().c_str());
#elif FO_ANDROID
    __android_log_write(ANDROID_LOG_INFO, FO_DEV_NAME, str.c_str());
#endif
}

void Platform::SetThreadName(const string& str)
{
#if FO_WINDOWS
    using SetThreadDescriptionFn = HRESULT(WINAPI*)(HANDLE, PCWSTR);
    const static auto set_thread_description = WinApi_GetProcAddress<SetThreadDescriptionFn>("kernel32.dll", "SetThreadDescription");

    if (set_thread_description != nullptr) {
        set_thread_description(::GetCurrentThread(), _str(str).toWideChar().c_str());
    }
#endif
}

auto Platform::GetExePath() -> optional<string>
{
    STACK_TRACE_ENTRY();

#if FO_WINDOWS
    vector<wchar_t> path;
    path.resize(FILENAME_MAX);

    auto size = ::GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));

    if (size == 0) {
        return std::nullopt;
    }

    while (size == path.size()) {
        path.resize(path.size() * 2);
        size = ::GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));

        if (size == 0) {
            return std::nullopt;
        }
    }

    return _str().parseWideChar(path.data()).str();

#elif FO_LINUX
    char path[FILENAME_MAX];

    const auto size = ::readlink("/proc/self/exe", path, sizeof(path) - 1);

    if (size == -1) {
        return std::nullopt;
    }

    path[size] = '\0';

    return path;

#elif FO_MAC
    char path[PROC_PIDPATHINFO_MAXSIZE];

    const auto pid = ::getpid();

    if (::proc_pidpath(pid, path, sizeof(path)) <= 0) {
        return std::nullopt;
    }

    return path;

#else
    return std::nullopt;
#endif
}

void Platform::ForkProcess() // NOLINT(clang-diagnostic-missing-noreturn)
{
    STACK_TRACE_ENTRY();

#if FO_LINUX || FO_MAC
    const pid_t pid = ::fork();
    if (pid < 0) {
        WriteLog(LogType::Warning, "fork() failed");
        return;
    }
    else if (pid != 0) {
        ExitApp(true);
    }

    ::close(STDIN_FILENO);
    ::close(STDOUT_FILENO);
    ::close(STDERR_FILENO);

    if (::setsid() < 0) {
        WriteLog(LogType::Warning, "setsid() failed");
    }

#else
    WriteLog(LogType::Warning, "Can't fork() on this platform");
#endif
}
