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

// Todo: server logs append not rewrite (with checking of size)

#include "Logging.h"
#include "BaseLogging.h"
#include "GlobalData.h"
#include "Platform.h"
#include "StackTrace.h"
#include "TimeRelated.h"
#include "WorkThread.h"

FO_BEGIN_NAMESPACE();

[[maybe_unused]] static void FlushLogAtExit();

struct LoggingData
{
    LoggingData()
    {
        FO_STACK_TRACE_ENTRY();

        MainThreadId = std::this_thread::get_id();
    }

    map<string, LogFunc> LogFunctions {};
    std::atomic_bool LogFunctionsInProcess {};
    std::thread::id MainThreadId {};
    bool TagsDisabled {};
};
FO_GLOBAL_DATA(LoggingData, Logging);

void SetLogCallback(string_view key, LogFunc callback)
{
    FO_STACK_TRACE_ENTRY();

    std::scoped_lock locker(GetLogLocker());

    if (!key.empty()) {
        if (callback) {
            Logging->LogFunctions.emplace(key, std::move(callback));
        }
        else {
            Logging->LogFunctions.erase(string(key));
        }
    }
    else {
        Logging->LogFunctions.clear();
    }
}

void LogDisableTags()
{
    FO_STACK_TRACE_ENTRY();

    std::scoped_lock locker(GetLogLocker());

    Logging->TagsDisabled = true;
}

void WriteLogMessage(LogType type, string_view message) noexcept
{
    FO_STACK_TRACE_ENTRY();

    try {
        std::scoped_lock locker(GetLogLocker());

        // Make message
        string result;
        result.reserve(message.length() + 64);

        if (!Logging->TagsDisabled) {
            const auto time = nanotime::now().desc(true);
            result += strex("[{:02}/{:02}/{:02}] ", time.day, time.month, time.year % 100);
            result += strex("[{:02}:{:02}:{:02}] ", time.hour, time.minute, time.second);

            if (const auto thread_id = std::this_thread::get_id(); thread_id != Logging->MainThreadId) {
                result += strex("[{}] ", GetThisThreadName());
            }
        }

        result += message;
        result += '\n';

        // Write
        WriteBaseLog(result);

        if (!Logging->LogFunctions.empty()) {
            if (Logging->LogFunctionsInProcess) {
                return;
            }

            Logging->LogFunctionsInProcess = true;
            auto reset_in_process = ScopeCallback([]() noexcept { Logging->LogFunctionsInProcess = false; });

            for (const auto& func : Logging->LogFunctions | std::views::values) {
                func(result);
            }
        }

        Platform::InfoLog(result);

#if FO_TRACY
        TracyMessage(result.c_str(), result.length());
#endif

        // Todo: colorize log texts
        const char* color = nullptr;

        switch (type) {
        case LogType::InfoSection:
            color = "\033[32m"; // Green
            break;
        case LogType::Warning:
            color = "\033[33m"; // Yellow
            break;
        case LogType::Error:
            color = "\031[31m"; // Red
            break;
        default:
            break;
        }

        ignore_unused(color);
    }
    catch (...) {
        BreakIntoDebugger();
    }
}

FO_END_NAMESPACE();
