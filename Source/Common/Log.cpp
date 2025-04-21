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
// Todo: add timestamps and process id and thread id to file logs

#include "Log.h"
#include "DiskFileSystem.h"
#include "Platform.h"

[[maybe_unused]] static void FlushLogAtExit();

struct LogData
{
#if !FO_WEB && !FO_MAC && !FO_IOS && !FO_ANDROID
    LogData()
    {
        NO_STACK_TRACE_ENTRY();

        const auto result = std::at_quick_exit(FlushLogAtExit);
        UNUSED_VARIABLE(result);

        MainThreadId = std::this_thread::get_id();
    }
#endif

    std::mutex LogLocker {};
    unique_ptr<DiskFile> LogFileHandle {};
    map<string, LogFunc> LogFunctions {};
    std::atomic_bool LogFunctionsInProcess {};
    std::thread::id MainThreadId {};
    bool BriefOutput {};
};
GLOBAL_DATA(LogData, Data);

static void FlushLogAtExit()
{
    NO_STACK_TRACE_ENTRY();

    if (Data != nullptr && Data->LogLocker.try_lock()) {
        std::scoped_lock locker(std::adopt_lock, Data->LogLocker);

        if (Data->LogFileHandle) {
            Data->LogFileHandle.reset();
        }
    }
}

void LogToFile(string_view fname)
{
    STACK_TRACE_ENTRY();

    std::scoped_lock locker(Data->LogLocker);

    auto log_file = DiskFile {DiskFileSystem::OpenFile(fname, true, true)};

    if (log_file) {
        Data->LogFileHandle = SafeAlloc::MakeUnique<DiskFile>(std::move(log_file));
    }
    else {
        const string log_err_msg = strex("Can't create log file '{}'", fname).str();

        Platform::InfoLog(log_err_msg);

        std::cout << log_err_msg << "\n";
        std::cout.flush();
    }
}

void SetLogCallback(string_view key, LogFunc callback)
{
    STACK_TRACE_ENTRY();

    std::scoped_lock locker(Data->LogLocker);

    if (!key.empty()) {
        if (callback) {
            Data->LogFunctions.emplace(string(key), std::move(callback));
        }
        else {
            Data->LogFunctions.erase(string(key));
        }
    }
    else {
        RUNTIME_ASSERT(!callback);
        Data->LogFunctions.clear();
    }
}

void LogBriefOutput()
{
    STACK_TRACE_ENTRY();

    std::scoped_lock locker(Data->LogLocker);

    Data->BriefOutput = true;
}

void WriteLogMessage(LogType type, string_view message) noexcept
{
    STACK_TRACE_ENTRY();

    try {
        // Avoid recursive calls
        if (Data->LogFunctionsInProcess) {
            return;
        }

        std::scoped_lock locker(Data->LogLocker);

        // Make message
        string out_message;
        out_message.reserve(message.length() + 128);

        const auto time = nanotime::now();

        if (!Data->BriefOutput) {
            const auto time_desc = time.desc(true);
            out_message += strex("[{:02}/{:02}/{:02}] ", time_desc.day, time_desc.month, time_desc.year % 100);
            out_message += strex("[{:02}:{:02}:{:02}] ", time_desc.hour, time_desc.minute, time_desc.second);

            if (const auto thread_id = std::this_thread::get_id(); thread_id != Data->MainThreadId) {
                out_message += strex("[{}] ", GetThisThreadName());
            }
        }

        out_message += message;
        out_message += '\n';

        auto log_entry = LogEntry {type, string(message), time, GetStackTraceEntries(1)};

        // Write logs
        if (Data->LogFileHandle) {
            Data->LogFileHandle->Write(out_message);
        }

        if (!Data->LogFunctions.empty()) {
            Data->LogFunctionsInProcess = true;
            auto reset_in_process = ScopeCallback([]() noexcept { Data->LogFunctionsInProcess = false; });

            for (auto&& [func_name, func] : Data->LogFunctions) {
                func(out_message);
            }
        }

        Platform::InfoLog(out_message);

#if FO_TRACY
        TracyMessage(out_message.c_str(), out_message.length());
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

        if (color != nullptr) {
            // std::cout << color << out_message << "\033[39m";
            std::cout << out_message;
        }
        else {
            std::cout << out_message;
        }

        std::cout.flush();
    }
    catch (...) {
        BreakIntoDebugger();
    }
}

extern void WriteLogFatalMessage(string_view message) noexcept
{
    NO_STACK_TRACE_ENTRY();

    try {
        std::scoped_lock locker(Data->LogLocker);

        if (Data->LogFileHandle) {
            Data->LogFileHandle->Write(message);
        }

        std::cout << message;
        std::cout.flush();
    }
    catch (...) {
        BreakIntoDebugger();
    }
}
