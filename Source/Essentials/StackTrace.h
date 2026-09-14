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

#include "BasicCore.h"

FO_BEGIN_NAMESPACE

struct StackTraceFrame
{
    enum class FrameType : uint8_t
    {
        Native,
        Script,
    };

    FrameType Type {FrameType::Native};
    std::string Function {};
    std::string File {};
    uint32_t Line {};
};

inline constexpr size_t STACK_TRACE_MAX_NATIVE_FRAMES = 128;
inline constexpr size_t STACK_TRACE_RESOLVE_CACHE_MAX_ENTRIES = 4096;
using NativeStackFrameAddress = uintptr_t;

struct ScriptStackTraceLayer
{
    std::vector<StackTraceFrame> ScriptFrames {};
    std::array<NativeStackFrameAddress, STACK_TRACE_MAX_NATIVE_FRAMES> BirthNativeFrames {};
    uint32_t BirthNativeFrameCount {};
    bool BirthNativeTruncated {};
    // Addresses inside code the script runtime generated (JIT output), which ScriptFrames already describe
    std::vector<NativeStackFrameAddress> RuntimeNativeFrames {};
};

struct StackTraceData
{
    std::array<NativeStackFrameAddress, STACK_TRACE_MAX_NATIVE_FRAMES> NativeFrames {};
    uint32_t NativeFrameCount {};
    bool NativeTruncated {};
    std::shared_ptr<const std::vector<ScriptStackTraceLayer>> ScriptLayers {};
};

struct CatchedStackTraceData
{
    optional<StackTraceData> Origin {};
    StackTraceData Catched {};
};

#if FO_TRACY
#define FO_STACK_TRACE_ENTRY() ZoneScoped
#define FO_STACK_TRACE_ENTRY_NAMED(name) ZoneScopedN(name)
#else
#define FO_STACK_TRACE_ENTRY()
#define FO_STACK_TRACE_ENTRY_NAMED(name)
#endif
#define FO_NO_STACK_TRACE_ENTRY()

// A provider appends its layers innermost first and may inspect the native frames already captured for the same trace
using ScriptStackTraceProvider = std::function<void(const StackTraceData& st, std::vector<ScriptStackTraceLayer>& out_layers)>;

extern void SetScriptStackTraceProvider(std::string_view name, ScriptStackTraceProvider provider) noexcept;
extern auto HasScriptStackTraceProvider(std::string_view name) noexcept -> bool;
extern auto GetStackTrace() noexcept -> StackTraceData;
extern void AddUnwoundScriptFrames(StackTraceData& st, ScriptStackTraceLayer layer);
extern void SpliceCaughtScriptFrames(StackTraceData& st, ScriptStackTraceLayer layer);
extern void CaptureNativeStackFrames(std::array<NativeStackFrameAddress, STACK_TRACE_MAX_NATIVE_FRAMES>& out_frames, uint32_t& out_count, bool& out_truncated, uint32_t skip = 0) noexcept;
extern void ClearResolvedStackTraceCache() noexcept;
extern auto GetResolvedStackTraceCacheSize() noexcept -> size_t;
extern auto ResolveStackTrace(const StackTraceData& st) -> std::vector<StackTraceFrame>;
extern auto GetStackTraceEntry(uint32_t deep) noexcept -> std::optional<StackTraceFrame>;
extern auto FormatStackTrace(const StackTraceData& st) -> std::string;
extern auto FormatStackTrace(const CatchedStackTraceData& st) -> std::string;

FO_END_NAMESPACE
