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

// Todo: server logs append not rewrite (with checking of size)
// Todo: add timestamps and process id and thread id to file logs
// Todo: delete \n appendix from WriteLog

#include "Log.h"
#include "DiskFileSystem.h"
#include "StringUtils.h"
#include "WinApi-Include.h"

#if FO_ANDROID
#include <android/log.h>
#endif

[[maybe_unused]] static void FlushLogAtExit();

struct LogData
{
#if !FO_WEB && !FO_MAC && !FO_IOS && !FO_ANDROID
    LogData()
    {
        const auto result = std::at_quick_exit(FlushLogAtExit);
        UNUSED_VARIABLE(result);
    }
#endif

    std::mutex LogLocker {};
    bool LogDisableTimestamp {};
    unique_ptr<DiskFile> LogFileHandle {};
    map<string, LogFunc> LogFunctions {};
    std::atomic_bool LogFunctionsInProcess {};
};
GLOBAL_DATA(LogData, Data);

static void FlushLogAtExit()
{
    if (Data != nullptr && Data->LogFileHandle) {
        Data->LogFileHandle.reset();
    }
}

void LogWithoutTimestamp()
{
    std::lock_guard locker(Data->LogLocker);

    Data->LogDisableTimestamp = true;
}

void LogToFile()
{
    std::lock_guard locker(Data->LogLocker);

    const auto fname = _str("{}.log", GetAppName()).str();
    Data->LogFileHandle = std::make_unique<DiskFile>(DiskFile {DiskFileSystem::OpenFile(fname, true, true)});
}

void SetLogCallback(string_view key, LogFunc callback)
{
    std::lock_guard locker(Data->LogLocker);

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

void WriteLogMessage(LogType type, string_view message)
{
    // Avoid recursive calls
    if (Data->LogFunctionsInProcess) {
        return;
    }

    std::lock_guard locker(Data->LogLocker);

    // Make message
    string result;
    if (!Data->LogDisableTimestamp) {
        const auto now = ::time(nullptr);
        const auto* t = ::localtime(&now);
        result += _str("[{}:{}:{}] ", t->tm_hour, t->tm_min, t->tm_sec);
    }

    result.reserve(result.size() + message.length() + 1u);
    result += message;
    result += '\n';

    // Write logs
    if (Data->LogFileHandle) {
        Data->LogFileHandle->Write(result);
    }

    if (!Data->LogFunctions.empty()) {
        Data->LogFunctionsInProcess = true;
        for (auto&& [func_name, func] : Data->LogFunctions) {
            func(result);
        }
        Data->LogFunctionsInProcess = false;
    }

#if FO_WINDOWS
    ::OutputDebugStringW(_str(result).toWideChar().c_str());
#endif

#if FO_ANDROID
    __android_log_print(ANDROID_LOG_INFO, "FOnline", "%s", result.c_str());
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
        // std::cout << color << result << "\033[39m";
        std::cout << result;
    }
    else {
        std::cout << result;
    }

    std::cout.flush();
}
