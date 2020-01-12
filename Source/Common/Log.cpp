#include "Log.h"
#include "FileSystem.h"
#include "StringUtils.h"
#include "Testing.h"
#include "WinApi_Include.h"
#ifdef FO_ANDROID
#include <android/log.h>
#endif

static std::mutex LogLocker;
static bool LogDisableTimestamp;
static void* LogFileHandle;
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

    if (LogFileHandle)
        FileClose(LogFileHandle);
    LogFileHandle = nullptr;

    if (!fname.empty())
        LogFileHandle = FileOpen(fname, true, true);
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
        FileWrite(LogFileHandle, result.c_str(), (uint)result.length());

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
