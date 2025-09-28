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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "StackTrace.h"
#include "BaseLogging.h"

FO_BEGIN_NAMESPACE();

static thread_local StackTraceData StackTrace;

extern void PushStackTrace(const SourceLocationData& loc) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if !FO_NO_MANUAL_STACK_TRACE
    auto& st = StackTrace;

    if (st.CallsCount < STACK_TRACE_MAX_SIZE) {
        st.CallTree[st.CallsCount] = &loc;
    }

    st.CallsCount++;
#endif
}

extern void PopStackTrace() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if !FO_NO_MANUAL_STACK_TRACE
    auto& st = StackTrace;

    if (st.CallsCount > 0) {
        st.CallsCount--;
    }
#endif
}

auto GetStackTrace() noexcept -> const StackTraceData&
{
    FO_NO_STACK_TRACE_ENTRY();

    return StackTrace;
}

extern auto GetStackTraceEntry(size_t deep) noexcept -> const SourceLocationData*
{
    FO_NO_STACK_TRACE_ENTRY();

#if !FO_NO_MANUAL_STACK_TRACE
    const auto& st = StackTrace;

    if (deep < st.CallsCount && st.CallsCount - 1 - deep < STACK_TRACE_MAX_SIZE) {
        return st.CallTree[st.CallsCount - 1 - deep];
    }
    else {
        return nullptr;
    }

#else

    return nullptr;
#endif
}

extern void SafeWriteStackTrace(const StackTraceData& st) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    WriteBaseLog("Stack trace (most recent call first):\n");

    char itoa_buf[64] = {};

    for (size_t i = std::min(st.CallsCount, STACK_TRACE_MAX_SIZE); i > 0; i--) {
        const auto& entry = st.CallTree[i - 1];
        string_view file_name = entry->file;

        if (const auto pos = file_name.find_last_of("/\\"); pos != string_view::npos) {
            file_name = file_name.substr(pos + 1);
        }

        WriteBaseLog("- ");
        WriteBaseLog(entry->function);
        WriteBaseLog(" (");
        WriteBaseLog(file_name);
        WriteBaseLog(" line ");
        WriteBaseLog(ItoA(entry->line, itoa_buf, 10));
        WriteBaseLog(")\n");
    }

    if (st.CallsCount > STACK_TRACE_MAX_SIZE) {
        WriteBaseLog("- ...and ");
        WriteBaseLog(ItoA(static_cast<int64>(st.CallsCount - STACK_TRACE_MAX_SIZE), itoa_buf, 10));
        WriteBaseLog(" more entries\n");
    }

    WriteBaseLog("\n");
}

FO_END_NAMESPACE();
