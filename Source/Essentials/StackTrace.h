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

#pragma once

#include "BasicCore.h"

FO_BEGIN_NAMESPACE();

static constexpr size_t STACK_TRACE_MAX_SIZE = 128;

// Profiling & stack trace obtaining
// Todo: improve automatic checker of FO_STACK_TRACE_ENTRY/FO_NO_STACK_TRACE_ENTRY in every .cpp function
#if FO_TRACY
using SourceLocationData = tracy::SourceLocationData;

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

struct StackTraceData
{
    size_t CallsCount = {};
    array<const SourceLocationData*, STACK_TRACE_MAX_SIZE> CallTree = {};
};

extern void PushStackTrace(const SourceLocationData& loc) noexcept;
extern void PopStackTrace() noexcept;
extern auto GetStackTrace() noexcept -> const StackTraceData&;
extern auto GetStackTraceEntry(size_t deep) noexcept -> const SourceLocationData*;
extern void SafeWriteStackTrace(const StackTraceData& st) noexcept;

struct StackTraceScopeEntry
{
    FO_FORCE_INLINE explicit StackTraceScopeEntry(const SourceLocationData& loc) noexcept { PushStackTrace(loc); }
    FO_FORCE_INLINE ~StackTraceScopeEntry() noexcept { PopStackTrace(); }

    StackTraceScopeEntry(const StackTraceScopeEntry&) = delete;
    StackTraceScopeEntry(StackTraceScopeEntry&&) noexcept = delete;
    auto operator=(const StackTraceScopeEntry&) -> StackTraceScopeEntry& = delete;
    auto operator=(StackTraceScopeEntry&&) noexcept -> StackTraceScopeEntry& = delete;
};

FO_END_NAMESPACE();
