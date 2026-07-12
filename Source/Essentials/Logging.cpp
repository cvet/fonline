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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "Logging.h"
#include "BaseLogging.h"
#include "GlobalData.h"
#include "Platform.h"
#include "StackTrace.h"
#include "TimeRelated.h"
#include "WorkThread.h"

FO_BEGIN_NAMESPACE

static void EmitLogMessage(LogType type, string_view message, nptr<const CatchedStackTraceData> st);
static void FlushLogMessageRepeatsLocked();
static auto IsSameAsLastLogMessage(LogType type, string_view message) -> bool;
static void RememberLastLogMessage(LogType type, string_view message) noexcept;
static void ClearLastLogMessage() noexcept;
static void FlushLogAtExit();

struct LoggingData
{
    LoggingData()
    {
        FO_STACK_TRACE_ENTRY();

#if !FO_WEB && !FO_MAC && !FO_IOS && !FO_ANDROID
        (void)std::at_quick_exit(FlushLogAtExit);
#else
        ignore_unused(FlushLogAtExit);
#endif

        MainThreadId = std::this_thread::get_id();
    }

    std::recursive_mutex Locker {};
    vector<pair<string, LogFunc>> LogFunctions {};
    std::atomic_bool LogFunctionsInProcess {};
    std::thread::id MainThreadId {};
    optional<LogType> LastLogType {};
    string LastLogMessage {};
    uint64_t SameLogMessageCount {};
    bool TagsDisabled {};
};
FO_GLOBAL_DATA(LoggingData, Logging);

extern void WriteLogMessage(LogType type, string_view message, nptr<const CatchedStackTraceData> st) noexcept
{
    FO_STACK_TRACE_ENTRY();

    try {
        if (Logging == nullptr) {
            string result;
            result.reserve(message.length() + 1);
            result += message;
            result += '\n';

            if (st) {
                WriteBaseLog(result, st.get());
            }
            else {
                WriteBaseLog(result);
            }

            return;
        }

        std::scoped_lock locker {Logging->Locker};

        if (IsSameAsLastLogMessage(type, message)) {
            Logging->SameLogMessageCount++;
            return;
        }

        FlushLogMessageRepeatsLocked();
        EmitLogMessage(type, message, st);
        RememberLastLogMessage(type, message);
    }
    catch (...) {
        BreakIntoDebugger();
    }
}

extern void SetLogCallback(string_view key, LogFunc callback)
{
    FO_STACK_TRACE_ENTRY();

    std::scoped_lock locker {Logging->Locker};

    FlushLogMessageRepeatsLocked();

    if (!key.empty()) {
        std::erase_if(Logging->LogFunctions, [key](auto&& e) { return e.first == key; });

        if (callback) {
            Logging->LogFunctions.emplace_back(key, std::move(callback));
        }
    }
    else {
        Logging->LogFunctions.clear();
    }
}

extern void LogDisableTags()
{
    FO_STACK_TRACE_ENTRY();

    std::scoped_lock locker {Logging->Locker};

    FlushLogMessageRepeatsLocked();

    Logging->TagsDisabled = true;
}

static void EmitLogMessage(LogType type, string_view message, nptr<const CatchedStackTraceData> st)
{
    FO_STACK_TRACE_ENTRY();

    // Make message
    string result;
    result.reserve(message.length() + 64);

    if (!Logging->TagsDisabled) {
        const time_desc_t time = nanotime::now().desc(true);
        result += strex("[{:02}/{:02}/{:02}] ", time.day, time.month, time.year % 100);
        result += strex("[{:02}:{:02}:{:02}] ", time.hour, time.minute, time.second);

        if (const std::thread::id thread_id = std::this_thread::get_id(); thread_id != Logging->MainThreadId) {
            result += strex("[{}] ", get_this_thread_name());
        }
    }

    result += message;
    result += '\n';

    if (st) {
        WriteBaseLog(result, st.get());
    }
    else {
        WriteBaseLog(result);
    }

    if (!Logging->LogFunctions.empty()) {
        if (Logging->LogFunctionsInProcess) {
            return;
        }

        Logging->LogFunctionsInProcess = true;
        auto reset_in_process = scope_exit([]() noexcept { Logging->LogFunctionsInProcess = false; });

        for (const auto& func : Logging->LogFunctions | std::views::values) {
            func(type, result, st);
        }
    }

    if constexpr (FO_DEBUG) {
        Platform::InfoLog(result);
    }

#if FO_TRACY
    auto tracy_message = make_ptr(result.c_str());
    TracyMessage(tracy_message.get(), result.length());
#endif
}

static void FlushLogMessageRepeatsLocked()
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!Logging->LastLogType.has_value()) {
        return;
    }

    const optional<LogType> last_log_type = Logging->LastLogType;
    const uint64_t same_message_count = Logging->SameLogMessageCount;

    ClearLastLogMessage();

    if (same_message_count == 0) {
        return;
    }

    string repeat_message;

    if (same_message_count == 1) {
        repeat_message = "...and 1 more same message";
    }
    else {
        repeat_message = strex("...and {} more same messages", same_message_count);
    }

    EmitLogMessage(*last_log_type, repeat_message, nullptr);
}

static void FlushLogAtExit()
{
    FO_NO_STACK_TRACE_ENTRY();

    if (Logging != nullptr) {
        std::scoped_lock locker {Logging->Locker};

        FlushLogMessageRepeatsLocked();
    }
}

static auto IsSameAsLastLogMessage(LogType type, string_view message) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return Logging->LastLogType.has_value() && *Logging->LastLogType == type && string_view {Logging->LastLogMessage} == message;
}

static void RememberLastLogMessage(LogType type, string_view message) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    Logging->LastLogType = type;
    Logging->LastLogMessage.assign(message);
    Logging->SameLogMessageCount = 0;
}

static void ClearLastLogMessage() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    Logging->LastLogType.reset();
    Logging->LastLogMessage.clear();
    Logging->SameLogMessageCount = 0;
}

FO_END_NAMESPACE
