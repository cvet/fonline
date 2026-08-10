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

#pragma once

#include "BaseLogging.h"
#include "BasicCore.h"
#include "SmartPointers.h"
#include "StackTrace.h"
#include "TextFormatting.h"
#include "TextTypes.h"

FO_BEGIN_NAMESPACE

enum class LogType : uint8_t
{
    Info,
    InfoSection,
    Warning,
    Error,
};

using LogFunc = function<void(LogType, u8string_view, nptr<const CatchedStackTraceData>)>;

// Write strict UTF-8 directly to the base logger
extern void WriteBaseLog(u8string_view message, nptr<const CatchedStackTraceData> st = nullptr) noexcept;
inline void WriteBaseLog(const u8string& message, nptr<const CatchedStackTraceData> st = nullptr) noexcept
{
    WriteBaseLog(message.view(), st);
}

// Write formatted text
extern void WriteLogMessage(LogType type, u8string_view message, nptr<const CatchedStackTraceData> st = nullptr) noexcept;

namespace logging_detail
{
    template<typename... Args>
    void write_formatted_log(LogType type, format_string<std::type_identity_t<Args>...> format, Args&&... args) noexcept
    {
        try {
            u8string message = FormatUtf8(format, std::forward<Args>(args)...);
            WriteLogMessage(type, message);
        }
        catch (...) {
            BreakIntoDebugger();
            WriteLogMessage(type, u8"Log message rejected: invalid UTF-8 or formatting arguments");
        }
    }

    template<typename... Args>
    void write_formatted_log(LogType type, u8format_string<std::type_identity_t<Args>...> format, Args&&... args) noexcept
    {
        try {
            u8string message = FormatUtf8(format, std::forward<Args>(args)...);
            WriteLogMessage(type, message);
        }
        catch (...) {
            BreakIntoDebugger();
            WriteLogMessage(type, u8"Log message rejected: invalid UTF-8 or formatting arguments");
        }
    }
}

template<typename... Args>
void WriteLog(format_string<std::type_identity_t<Args>...> format, Args&&... args) noexcept
{
    logging_detail::write_formatted_log(LogType::Info, format, std::forward<Args>(args)...);
}

template<typename... Args>
void WriteLog(LogType type, format_string<std::type_identity_t<Args>...> format, Args&&... args) noexcept
{
    logging_detail::write_formatted_log(type, format, std::forward<Args>(args)...);
}

template<typename... Args>
void WriteLog(u8format_string<std::type_identity_t<Args>...> format, Args&&... args) noexcept
{
    logging_detail::write_formatted_log(LogType::Info, format, std::forward<Args>(args)...);
}

template<typename... Args>
void WriteLog(LogType type, u8format_string<std::type_identity_t<Args>...> format, Args&&... args) noexcept
{
    logging_detail::write_formatted_log(type, format, std::forward<Args>(args)...);
}

inline void WriteLog(u8string_view str) noexcept
{
    WriteLogMessage(LogType::Info, str);
}

inline void WriteLog(LogType type, u8string_view str) noexcept
{
    WriteLogMessage(type, str);
}

inline void WriteLog(const u8string& str) noexcept
{
    WriteLogMessage(LogType::Info, str.view());
}

inline void WriteLog(LogType type, const u8string& str) noexcept
{
    WriteLogMessage(type, str.view());
}

// Control
extern void SetLogCallback(string_view key, LogFunc callback);
extern void LogDisableTags();

FO_END_NAMESPACE
