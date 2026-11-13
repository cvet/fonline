// FOnline Engine
// SPDX-License-Identifier: MIT

#include "FatalError.h"

#include "BaseLogging.h"
#include "StackTrace.h"

FO_BEGIN_NAMESPACE

[[noreturn]] void fatal::report_and_exit(string_view message) noexcept
{
    logging::suspend_async_writing();

    logging::write_base("\nFATAL ERROR!\n");
    logging::write_base(message);
    logging::write_base("\n\n");

    stack_trace::data st {};
    stack_trace::capture_native_frames(st.native_frames, st.native_frame_count, st.native_truncated, 1);
    logging::safe_write_stack_trace(st);

    break_into_debugger();
    exit_app(false);
}

[[noreturn]] void fatal::report_strong_assert_and_exit(string_view expression, string_view file, int32_t line) noexcept
{
    logging::suspend_async_writing();

    char line_buf[64] {};

    logging::write_base("\nSTRONG ASSERTION FAILED!\n");
    logging::write_base("Expression: ");
    logging::write_base(expression);
    logging::write_base("\nFile: ");
    logging::write_base(file);
    logging::write_base("\nLine: ");
    logging::write_base(itoa(line, line_buf, 10));
    logging::write_base("\n\n");

    stack_trace::data st {};
    stack_trace::capture_native_frames(st.native_frames, st.native_frame_count, st.native_truncated, 1);
    logging::safe_write_stack_trace(st);

    break_into_debugger();
    exit_app(false);
}

FO_END_NAMESPACE
