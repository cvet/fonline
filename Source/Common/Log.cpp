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
#include "Testing.h"
#include "WinApi_Include.h"

#ifdef FO_ANDROID
#include <android/log.h>
#endif

static std::mutex LogLocker;
static bool LogDisableTimestamp;
static unique_ptr<DiskFile> LogFileHandle;
static map<string, LogFunc> LogFunctions;
static bool LogFunctionsInProcess;
static string* LogBufferStr;

void LogWithoutTimestamp()
{
    SCOPE_LOCK(LogLocker);

    LogDisableTimestamp = true;
}

void LogToFile(const string& fname)
{
    SCOPE_LOCK(LogLocker);

    LogFileHandle = nullptr;

    if (!fname.empty())
        LogFileHandle = unique_ptr<DiskFile>(new DiskFile {DiskFileSystem::OpenFile(fname, true, true)});
}

void LogToFunc(const string& key, LogFunc func, bool enable)
{
    SCOPE_LOCK(LogLocker);

    if (func)
    {
        LogFunctions.erase(key);

        if (enable)
            LogFunctions.insert(std::make_pair(key, func));
    }
    else if (!enable)
    {
        LogFunctions.clear();
    }
}

void LogToBuffer(bool enable)
{
    SCOPE_LOCK(LogLocker);

    SAFEDEL(LogBufferStr);
    if (enable)
        LogBufferStr = new string();
}

void LogGetBuffer(std::string& buf)
{
    SCOPE_LOCK(LogLocker);

    if (LogBufferStr && !LogBufferStr->empty())
    {
        buf = *LogBufferStr;
        LogBufferStr->clear();
    }
}

void WriteLogMessage(const string& message)
{
    SCOPE_LOCK(LogLocker);

    // Avoid recursive calls
    if (LogFunctionsInProcess)
        return;

    // Make message
    string result;
    if (!LogDisableTimestamp)
    {
        time_t now = time(nullptr);
        struct tm* t = localtime(&now);
        result += _str("[{:0=2}:{:0=2}:{:0=2}] ", t->tm_hour, t->tm_min, t->tm_sec);
    }
    result += message;

    // Write logs
    if (LogFileHandle)
        LogFileHandle->Write(result.c_str(), (uint)result.length());

    if (LogBufferStr)
        *LogBufferStr += result;

    if (!LogFunctions.empty())
    {
        LogFunctionsInProcess = true;
        for (auto& kv : LogFunctions)
            kv.second(result);
        LogFunctionsInProcess = false;
    }

#ifdef FO_WINDOWS
    OutputDebugStringW(_str(result).toWideChar().c_str());
#endif

#ifndef FO_ANDROID
    printf("%s", result.c_str());
#else
    __android_log_print(ANDROID_LOG_INFO, "FOnline", "%s", result.c_str());
#endif
}
