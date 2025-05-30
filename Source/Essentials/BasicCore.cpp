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

#include "BasicCore.h"
#include "WinApi-Include.h"

#if FO_MAC
#include <sys/sysctl.h>
#include <unistd.h>
#endif

FO_BEGIN_NAMESPACE();

static bool RunInDebugger = false;
static std::once_flag RunInDebuggerOnce;

void ExitApp(bool success) noexcept
{
    const auto code = success ? EXIT_SUCCESS : EXIT_FAILURE;

#if !FO_WEB && !FO_MAC && !FO_IOS && !FO_ANDROID
    std::quick_exit(code);
#else
    std::exit(code);
#endif
}

extern auto IsRunInDebugger() noexcept -> bool
{
#if FO_WINDOWS
    std::call_once(RunInDebuggerOnce, [] { RunInDebugger = ::IsDebuggerPresent() != FALSE; });

#elif FO_LINUX
    std::call_once(RunInDebuggerOnce, [] {
        std::ifstream status("/proc/self/status");
        if (status.is_open()) {
            std::string line;
            while (std::getline(status, line)) {
                if (line.starts_with("TracerPid:")) {
                    std::istringstream iss(line.substr("TracerPid:"_len));
                    std::string value;
                    iss >> value;
                    if (!value.empty() && std::all_of(value.begin(), value.end(), ::isdigit)) {
                        RunInDebugger = value != "0";
                    }
                    break;
                }
            }
        }
    });

#elif FO_MAC
    std::call_once(RunInDebuggerOnce, [] {
        int32 mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
        struct kinfo_proc info = {};
        size_t size = sizeof(info);
        if (::sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, nullptr, 0) == 0) {
            RunInDebugger = (info.kp_proc.p_flag & P_TRACED) != 0;
        }
    });

#else
    ignore_unused(RunInDebuggerOnce);
#endif

    return RunInDebugger;
}

extern auto BreakIntoDebugger() noexcept -> bool
{
    if (IsRunInDebugger()) {
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

extern auto ItoA(int64 num, char buf[64], int32 base) noexcept -> const char*
{
    int32 i = 0;
    bool is_negative = false;

    if (num == 0) {
        buf[i++] = '0';
        buf[i] = '\0';
        return buf;
    }

    if (num < 0 && base == 10) {
        is_negative = true;
        num = -num;
    }

    while (num != 0) {
        const auto rem = num % base;
        buf[i++] = static_cast<char>(rem > 9 ? rem - 10 + 'a' : rem + '0');
        num = num / base;
    }

    if (is_negative) {
        buf[i++] = '-';
    }

    buf[i] = '\0';

    int32 start = 0;
    int32 end = i - 1;

    while (start < end) {
        const char ch = buf[start];
        buf[start] = buf[end];
        buf[end] = ch;
        end--;
        start++;
    }

    return buf;
}

FO_END_NAMESPACE();
