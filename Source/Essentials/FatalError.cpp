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

#include "FatalError.h"
#include "BaseLogging.h"
#include "StackTrace.h"

FO_BEGIN_NAMESPACE

[[noreturn]] extern void report_fatal_and_exit(string_view message) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    suspend_async_log_writing();

    write_base_log("\nFATAL ERROR!\n");
    write_base_log(message);
    write_base_log("\n\n");

    stack_trace_data st;
    capture_native_stack_frames(st.native_frames, st.native_frame_count, st.native_truncated, 2);
    safe_write_stack_trace(st);

    break_into_debugger();
    exit_app(false);
}

[[noreturn]] extern void report_strong_assert_and_exit(string_view expression, string_view file, int32_t line) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    suspend_async_log_writing();

    char line_buf[64] = {};

    write_base_log("\nSTRONG ASSERTION FAILED!\n");
    write_base_log("Expression: ");
    write_base_log(expression);
    write_base_log("\nFile: ");
    write_base_log(file);
    write_base_log("\nLine: ");
    write_base_log(itoa(line, line_buf, 10));
    write_base_log("\n\n");

    stack_trace_data st;
    capture_native_stack_frames(st.native_frames, st.native_frame_count, st.native_truncated, 2);
    safe_write_stack_trace(st);

    break_into_debugger();
    exit_app(false);
}

FO_END_NAMESPACE
