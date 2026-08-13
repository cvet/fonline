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
#include "StringUtils.h"
#include "TextConversions.h"
#include "TextFormatting.h"
#include "TimeRelated.h"
#include "WorkThread.h"

FO_BEGIN_NAMESPACE

extern void LogToFile(const std::filesystem::path& path, bool append);

static void EmitLogMessage(LogType type, u8string_view message, nptr<const CatchedStackTraceData> st);
static void FlushLogMessageRepeatsLocked();
static auto IsSameAsLastLogMessage(LogType type, u8string_view message) -> bool;
static void RememberLastLogMessage(LogType type, u8string_view message) noexcept;
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
    u8string LastLogMessage {};
    uint64_t SameLogMessageCount {};
    bool TagsDisabled {};
};
FO_GLOBAL_DATA(LoggingData, Logging);

extern void LogToFile(string_view path, bool append)
{
    FO_STACK_TRACE_ENTRY();

    LogToFile(std::filesystem::path {path}, append);
}

extern void LogToFile(u8string_view path, bool append)
{
    FO_STACK_TRACE_ENTRY();

    u8string checked_path = u8string::FromChecked(path.native_view());
    LogToFile(std::filesystem::path {checked_path.view().native_view()}, append);
}

extern void WriteBaseLog(u8string_view message, nptr<const CatchedStackTraceData> st) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (validate_utf8_text(message.native_view())) {
        u8string_view rejected {u8"Base log message rejected: invalid UTF-8\n"};
        WriteBaseLogBytes(utf8_to_byte_span(rejected), st ? st.get() : nullptr);
        return;
    }

    WriteBaseLogBytes(utf8_to_byte_span(message), st ? st.get() : nullptr);
}

extern void WriteLogMessage(LogType type, u8string_view message, nptr<const CatchedStackTraceData> st) noexcept
{
    FO_STACK_TRACE_ENTRY();

    try {
        u8string checked_message = u8string::FromChecked(message.native_view());

        if (Logging == nullptr) {
            u8string result;
            result.reserve(checked_message.size() + 1);
            result.append(checked_message.view());
            result.append("\n");
            WriteBaseLog(result.view(), st);

            return;
        }

        std::scoped_lock locker {Logging->Locker};

        if (IsSameAsLastLogMessage(type, checked_message.view())) {
            Logging->SameLogMessageCount++;
            return;
        }

        FlushLogMessageRepeatsLocked();
        EmitLogMessage(type, checked_message.view(), st);
        RememberLastLogMessage(type, checked_message.view());
    }
    catch (...) {
        BreakIntoDebugger();
    }
}

extern void SetLogCallback(string_view key, LogFunc callback)
{
    FO_STACK_TRACE_ENTRY();

    string checked_key {key};

    std::scoped_lock locker {Logging->Locker};

    FlushLogMessageRepeatsLocked();

    if (!checked_key.empty()) {
        std::erase_if(Logging->LogFunctions, [&checked_key](const auto& e) { return e.first == checked_key; });

        if (callback) {
            Logging->LogFunctions.emplace_back(checked_key, std::move(callback));
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

static void EmitLogMessage(LogType type, u8string_view message, nptr<const CatchedStackTraceData> st)
{
    FO_STACK_TRACE_ENTRY();

    // Make message
    u8string result;
    result.reserve(message.size() + 64);

    if (!Logging->TagsDisabled) {
        time_desc_t time = nanotime::now().desc(true);
        string date_tag = strex("[{:02}/{:02}/{:02}] ", time.day, time.month, time.year % 100);
        string time_tag = strex("[{:02}:{:02}:{:02}] ", time.hour, time.minute, time.second);
        result.append(date_tag);
        result.append(time_tag);

        if (std::thread::id thread_id = std::this_thread::get_id(); thread_id != Logging->MainThreadId) {
            string_view thread_name = get_this_thread_name();
            u8string thread_name_utf8 = thread_name;
            u8string thread_tag = FormatUtf8("[{}] ", thread_name_utf8);
            result.append(thread_tag.view());
        }
    }

    result.append(message);
    result.append("\n");

    WriteBaseLog(result.view(), st);

    if (!Logging->LogFunctions.empty()) {
        if (Logging->LogFunctionsInProcess) {
            return;
        }

        Logging->LogFunctionsInProcess = true;
        auto reset_in_process = scope_exit([]() noexcept { Logging->LogFunctionsInProcess = false; });

        for (const auto& func : Logging->LogFunctions | std::views::values) {
            func(type, result.view(), st);
        }
    }

    if constexpr (FO_DEBUG) {
        Platform::InfoLog(result.view_nt());
    }

#if FO_TRACY
    const_span<char> tracy_message = utf8_to_char_span(result.view());
    TracyMessage(tracy_message.data(), tracy_message.size());
#endif
}

static void FlushLogMessageRepeatsLocked()
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!Logging->LastLogType.has_value()) {
        return;
    }

    optional<LogType> last_log_type = Logging->LastLogType;
    uint64_t same_message_count = Logging->SameLogMessageCount;

    ClearLastLogMessage();

    if (same_message_count == 0) {
        return;
    }

    u8string repeat_message;

    if (same_message_count == 1) {
        repeat_message = u8string {u8"...and 1 more same message"};
    }
    else {
        repeat_message = FormatUtf8("...and {} more same messages", same_message_count);
    }

    EmitLogMessage(*last_log_type, repeat_message.view(), nullptr);
}

static void FlushLogAtExit()
{
    FO_NO_STACK_TRACE_ENTRY();

    if (Logging != nullptr) {
        std::scoped_lock locker {Logging->Locker};

        FlushLogMessageRepeatsLocked();
    }
}

static auto IsSameAsLastLogMessage(LogType type, u8string_view message) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return Logging->LastLogType.has_value() && *Logging->LastLogType == type && Logging->LastLogMessage.view() == message;
}

static void RememberLastLogMessage(LogType type, u8string_view message) noexcept
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
