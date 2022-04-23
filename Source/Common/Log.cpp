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
#include "Version-Include.h"
#include "WinApi-Include.h"

#if FO_ANDROID
#include <android/log.h>
#endif

struct LogData
{
    std::mutex LogLocker {};
    bool LogDisableTimestamp {};
    unique_ptr<DiskFile> LogFileHandle {};
    map<string, LogFunc> LogFunctions {};
    bool LogFunctionsInProcess {};
    string* LogBufferStr {};
};
GLOBAL_DATA(LogData, Data);

void LogWithoutTimestamp()
{
    std::lock_guard locker(Data->LogLocker);

    Data->LogDisableTimestamp = true;
}

void LogToFile()
{
    std::lock_guard locker(Data->LogLocker);

    const auto fname = _str("{}{}{}.log", FO_DEV_NAME, GetAppName()[0] != '\0' ? "_" : "", GetAppName()).str();
    Data->LogFileHandle = std::make_unique<DiskFile>(DiskFile {DiskFileSystem::OpenFile(fname, true, true)});
}

void LogToFunc(string_view key, const LogFunc& func, bool enable)
{
    std::lock_guard locker(Data->LogLocker);

    if (func) {
        Data->LogFunctions.erase(string(key));

        if (enable) {
            Data->LogFunctions.insert(std::make_pair(key, func));
        }
    }
    else if (!enable) {
        Data->LogFunctions.clear();
    }
}

void LogToBuffer(bool enable)
{
    std::lock_guard locker(Data->LogLocker);

    delete Data->LogBufferStr;
    Data->LogBufferStr = nullptr;

    if (enable) {
        Data->LogBufferStr = new string();
    }
}

auto LogGetBuffer() -> string
{
    std::lock_guard locker(Data->LogLocker);

    if (Data->LogBufferStr != nullptr && !Data->LogBufferStr->empty()) {
        auto buf = *Data->LogBufferStr;
        Data->LogBufferStr->clear();
        return buf;
    }
    return string();
}

void WriteLogMessage(string_view message)
{
    std::lock_guard locker(Data->LogLocker);

    // Avoid recursive calls
    if (Data->LogFunctionsInProcess) {
        return;
    }

    // Make message
    string result;
    if (!Data->LogDisableTimestamp) {
        auto now = time(nullptr);
        const auto* t = localtime(&now);
        result += _str("[{}:{}:{}] ", t->tm_hour, t->tm_min, t->tm_sec);
    }
    result += message;

    // Write logs
    if (Data->LogFileHandle) {
        Data->LogFileHandle->Write(result);
    }

    if (Data->LogBufferStr != nullptr) {
        *Data->LogBufferStr += result;
    }

    if (!Data->LogFunctions.empty()) {
        Data->LogFunctionsInProcess = true;
        for (auto& [func_name, func] : Data->LogFunctions) {
            func(result);
        }
        Data->LogFunctionsInProcess = false;
    }

#if FO_WINDOWS
    OutputDebugStringW(_str(result).toWideChar().c_str());
#endif

#if !FO_ANDROID
    printf("%s", result.c_str());
#else
    __android_log_print(ANDROID_LOG_INFO, "FOnline", "%s", result.c_str());
#endif
}
