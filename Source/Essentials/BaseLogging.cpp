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

#include "BaseLogging.h"
#include "GlobalData.h"
#include "WinApi-Include.h"

FO_BEGIN_NAMESPACE();

[[maybe_unused]] static void FlushLogAtExit();

struct BaseLoggingData
{
    BaseLoggingData()
    {
#if !FO_WEB && !FO_MAC && !FO_IOS && !FO_ANDROID
        const auto result = std::at_quick_exit(FlushLogAtExit);
        ignore_unused(result);
#endif

#if FO_WINDOWS
        SetConsoleOutputCP(CP_UTF8);
#endif
    }

    std::recursive_mutex LogLocker {};
    std::ofstream LogFileHandle {};
};
FO_GLOBAL_DATA(BaseLoggingData, BaseLogging);

static void FlushLogAtExit()
{
    if (BaseLogging != nullptr && BaseLogging->LogLocker.try_lock()) {
        std::scoped_lock locker(std::adopt_lock, BaseLogging->LogLocker);

        if (BaseLogging->LogFileHandle) {
            BaseLogging->LogFileHandle.close();
        }
    }
}

extern void LogToFile(string_view path)
{
    std::scoped_lock locker(BaseLogging->LogLocker);

    BaseLogging->LogFileHandle.open(std::string(path), std::ios::out | std::ios::binary | std::ios::trunc);

    if (!BaseLogging->LogFileHandle) {
        WriteBaseLog(std::string("Can't create log file '").append(path).append("'\n"));
    }
}

extern void WriteBaseLog(string_view message) noexcept
{
    try {
        std::scoped_lock locker(BaseLogging->LogLocker);

        if (BaseLogging->LogFileHandle) {
            BaseLogging->LogFileHandle << message;
            BaseLogging->LogFileHandle.flush();
        }

        std::cout << message;
        std::cout.flush();
    }
    catch (...) {
        BreakIntoDebugger();
    }
}

extern auto GetLogLocker() noexcept -> std::recursive_mutex&
{
    return BaseLogging->LogLocker;
}

FO_END_NAMESPACE();
