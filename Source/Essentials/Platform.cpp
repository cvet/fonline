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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "StackTrace.h"
#include "StringUtils.h"

#if FO_WINDOWS
#include "WinApi-Include.h"
#endif

#if FO_LINUX || FO_MAC
#include <dlfcn.h>
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

FO_BEGIN_NAMESPACE();

#if FO_WINDOWS
template<typename T>
static auto WinApi_GetProcAddress(const char* mod, const char* name) -> T
{
    FO_STACK_TRACE_ENTRY();

    if (auto* hmod = ::GetModuleHandleA(mod); hmod != nullptr) {
        return reinterpret_cast<T>(::GetProcAddress(hmod, name)); // NOLINT(clang-diagnostic-cast-function-type-strict)
    }

    return nullptr;
}
#endif

void Platform::InfoLog(const string& str) noexcept
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    ::OutputDebugStringW(strex(str).to_wide_char().c_str());
#elif FO_ANDROID
    __android_log_write(ANDROID_LOG_INFO, "FO", str.c_str());
#endif
}

void Platform::SetThreadName(const string& str) noexcept
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    using SetThreadDescriptionFn = HRESULT(WINAPI*)(HANDLE, PCWSTR);
    const static auto set_thread_description = WinApi_GetProcAddress<SetThreadDescriptionFn>("kernel32.dll", "SetThreadDescription");

    if (set_thread_description != nullptr) {
        set_thread_description(::GetCurrentThread(), strex(str).to_wide_char().c_str());
    }
#endif
}

auto Platform::GetExePath() noexcept -> optional<string>
{
    FO_STACK_TRACE_ENTRY();

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

    return strex().parse_wide_char(path.data());

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

auto Platform::ForkProcess() noexcept -> bool // NOLINT(clang-diagnostic-missing-noreturn)
{
    FO_STACK_TRACE_ENTRY();

#if FO_LINUX || FO_MAC
    const pid_t pid = ::fork();

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

auto Platform::LoadModule(const string& module_name) noexcept -> void*
{
    FO_STACK_TRACE_ENTRY();

    void* module_handle = nullptr;

#if FO_WINDOWS
    module_handle = ::LoadLibraryW(strex(strex::safe_format, "{}.dll", module_name).to_wide_char().c_str());
#elif FO_LINUX
    module_handle = ::dlopen(strex(strex::safe_format, "{}.so", module_name).c_str(), RTLD_LAZY | RTLD_LOCAL);
#elif FO_MAC
    module_handle = ::dlopen(strex(strex::safe_format, "{}.dylib", module_name).c_str(), RTLD_LAZY | RTLD_LOCAL);
#endif

    return module_handle;
}

void Platform::UnloadModule(void* module_handle) noexcept
{
    FO_STACK_TRACE_ENTRY();

    if (module_handle == nullptr) {
        return;
    }

#if FO_WINDOWS
    ::FreeLibrary(static_cast<HMODULE>(module_handle));
#elif FO_LINUX || FO_MAC
    ::dlclose(module_handle);
#endif
}

auto Platform::GetFuncAddr(void* module_handle, const string& func_name) noexcept -> void*
{
    FO_STACK_TRACE_ENTRY();

    void* func = nullptr;

#if FO_WINDOWS
    func = reinterpret_cast<void*>(::GetProcAddress(static_cast<HMODULE>(module_handle), func_name.c_str()));
#elif FO_LINUX || FO_MAC
    func = ::dlsym(module_handle != nullptr ? module_handle : RTLD_DEFAULT, func_name.c_str());
#endif

    return func;
}

auto Platform::IsShiftDown() noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    return (::GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
#else
    return false;
#endif
}

FO_END_NAMESPACE();
