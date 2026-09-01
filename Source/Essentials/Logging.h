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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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
#include "StringUtils.h"

FO_BEGIN_NAMESPACE

enum class log_type : uint8_t
{
    info,
    info_section,
    warning,
    error,
};

using log_func = function<void(log_type, string_view, nptr<const catched_stack_trace_data>)>;

// Write formatted text
extern void write_log_message(log_type type, string_view message, nptr<const catched_stack_trace_data> st = nullptr) noexcept;

template<typename... Args>
void write_log(std::format_string<Args...>&& format, Args&&... args) noexcept
{
    write_log_message(log_type::info, strex(strex::safe_format, std::move(format), std::forward<Args>(args)...));
}

template<typename... Args>
void write_log(log_type type, std::format_string<Args...>&& format, Args&&... args) noexcept
{
    write_log_message(type, strex(strex::safe_format, std::move(format), std::forward<Args>(args)...));
}

inline void write_log(string_view str) noexcept
{
    write_log_message(log_type::info, strex(strex::safe_format, "{}", str));
}

// Control
extern void set_log_callback(string_view key, log_func callback);
extern void log_disable_tags();

FO_END_NAMESPACE
