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

#include "BasicCore.h"

FO_BEGIN_NAMESPACE

static constexpr uint32 STACK_TRACE_MAX_SIZE = 63;

#if FO_TRACY
using SourceLocationData = tracy::SourceLocationData;
#else
struct SourceLocationData;
#endif

struct StackTraceData
{
    uint32 CallsCount {};
    array<const SourceLocationData*, STACK_TRACE_MAX_SIZE + 1> CallTree {};
};

namespace details
{
    inline thread_local StackTraceData StackTrace;
}

[[maybe_unused]] inline void StackTraceView()
{
    (void)details::StackTrace; // <<< Hover to view stack trace in visual studio debugger
}

// Profiling & stack trace obtaining
#if FO_TRACY
#if !FO_NO_MANUAL_STACK_TRACE
#define FO_STACK_TRACE_ENTRY() \
    ZoneScoped; \
    auto ___fo_stack_entry = FO_NAMESPACE StackTraceScopeEntry(TracyConcat(__tracy_source_location, TracyLine))
#define FO_STACK_TRACE_ENTRY_NAMED(name) \
    ZoneScopedN(name); \
    auto ___fo_stack_entry = FO_NAMESPACE StackTraceScopeEntry(TracyConcat(__tracy_source_location, TracyLine))
#else
#define FO_STACK_TRACE_ENTRY() ZoneScoped
#define FO_STACK_TRACE_ENTRY_NAMED(name) ZoneScopedN(name)
#endif
#define FO_NO_STACK_TRACE_ENTRY()

#else
struct SourceLocationData // Same as tracy::SourceLocationData
{
    const char* name;
    const char* function;
    const char* file;
    uint32_t line;
};

#if !FO_NO_MANUAL_STACK_TRACE
#define FO_STACK_TRACE_ENTRY() \
    static constexpr FO_NAMESPACE SourceLocationData FO_CONCAT(___fo_source_location, __LINE__) {nullptr, __FUNCTION__, __FILE__, static_cast<uint32_t>(__LINE__)}; \
    auto ___fo_stack_entry = FO_NAMESPACE StackTraceScopeEntry(FO_CONCAT(___fo_source_location, __LINE__))
#define FO_STACK_TRACE_ENTRY_NAMED(name) \
    static constexpr FO_NAMESPACE SourceLocationData FO_CONCAT(___fo_source_location, __LINE__) {nullptr, name, __FILE__, static_cast<uint32_t>(__LINE__)}; \
    auto ___fo_stack_entry = FO_NAMESPACE StackTraceScopeEntry(FO_CONCAT(___fo_source_location, __LINE__))
#else
#define FO_STACK_TRACE_ENTRY()
#define FO_STACK_TRACE_ENTRY_NAMED(name)
#endif
#define FO_NO_STACK_TRACE_ENTRY()
#endif

extern void PushStackTrace(const SourceLocationData* loc) noexcept;
extern void PopStackTrace() noexcept;
extern auto GetStackTrace() noexcept -> const StackTraceData&;
extern auto GetStackTraceEntry(uint32 deep) noexcept -> const SourceLocationData*;
extern void SafeWriteStackTrace(const StackTraceData& st) noexcept;

#if !FO_NO_MANUAL_STACK_TRACE
struct StackTraceScopeEntry
{
    FO_FORCE_INLINE explicit StackTraceScopeEntry(const SourceLocationData& loc) noexcept
    {
        auto& st = details::StackTrace;
        const uint32 cc = st.CallsCount;
        const uint32 idx = cc < STACK_TRACE_MAX_SIZE ? cc : STACK_TRACE_MAX_SIZE;
        st.CallTree[idx] = &loc;
        st.CallsCount = cc + 1;
        StackTraceCounter = &st.CallsCount;
    }
    FO_FORCE_INLINE ~StackTraceScopeEntry() noexcept { (*StackTraceCounter)--; }

    StackTraceScopeEntry(const StackTraceScopeEntry&) = delete;
    StackTraceScopeEntry(StackTraceScopeEntry&&) noexcept = delete;
    auto operator=(const StackTraceScopeEntry&) -> StackTraceScopeEntry& = delete;
    auto operator=(StackTraceScopeEntry&&) noexcept -> StackTraceScopeEntry& = delete;

    uint32* StackTraceCounter;
};
#endif

FO_END_NAMESPACE
