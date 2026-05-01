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

// Maximum number of native PCs captured per snapshot.
inline constexpr size_t STACK_TRACE_MAX_NATIVE_FRAMES = 128;

// One level of script execution within the unified stack trace. The provider returns layers
// in nesting order — innermost (active context) first, then up the parent-context chain.
struct ScriptStackTraceLayer
{
    std::vector<StackTraceFrame> ScriptFrames {};
    std::array<void*, STACK_TRACE_MAX_NATIVE_FRAMES> BirthNativeFrames {};
    uint32_t BirthNativeFrameCount {};
};

struct StackTraceData
{
    std::array<void*, STACK_TRACE_MAX_NATIVE_FRAMES> NativeFrames {};
    uint32_t NativeFrameCount {};
    std::shared_ptr<const std::vector<ScriptStackTraceLayer>> ScriptLayers {};
};

#if FO_TRACY
#define FO_STACK_TRACE_ENTRY() ZoneScoped
#define FO_STACK_TRACE_ENTRY_NAMED(name) ZoneScopedN(name)
#else
#define FO_STACK_TRACE_ENTRY()
#define FO_STACK_TRACE_ENTRY_NAMED(name)
#endif
#define FO_NO_STACK_TRACE_ENTRY()

// Provider that contributes script layers to a stack trace capture. Called synchronously
// from GetStackTrace(). Each layer represents one script context; layers must be appended in
// innermost-first order (active context, then its parent, then its parent's parent, ...).
using ScriptStackTraceProvider = std::function<void(std::vector<ScriptStackTraceLayer>& out_layers)>;

extern void SetScriptStackTraceProvider(ScriptStackTraceProvider provider) noexcept;
[[nodiscard]] extern auto HasScriptStackTraceProvider() noexcept -> bool;

// Capture the current stack trace. The result is an opaque blob of data that can be resolved.
[[nodiscard]] extern auto GetStackTrace() noexcept -> StackTraceData;

// Capture only the native call stack PCs into a fixed-size buffer. Used by higher layers
// (e.g., AngelScript) to remember each script context's birth-time native call site so the
// stack-trace resolver can interleave native and script frames correctly across nested
// contexts. `skip` excludes additional caller frames on top of the capture machinery.
extern void CaptureNativeStackFrames(std::array<void*, STACK_TRACE_MAX_NATIVE_FRAMES>& out_frames, uint32_t& out_count, uint32_t skip = 0) noexcept;

// Resolve the entire stack into a flat list of frames. The result interleaves layers and
// native slices in nesting order: innermost layer's script frames, then the native frames
// added between that layer's birth and the previous (more recent) layer's birth, then the
// next layer's script frames, etc. Trailing native frames (from main() down to the root
// context's birth) come last.
[[nodiscard]] extern auto ResolveStackTrace(const StackTraceData& st) -> std::vector<StackTraceFrame>;

// Look up a single resolved frame at depth `deep` (0 = topmost). Returns nullopt if `deep`
// is out of range.
[[nodiscard]] extern auto GetStackTraceEntry(uint32_t deep) noexcept -> std::optional<StackTraceFrame>;

// Format the trace as a human-readable, multi-line string.
[[nodiscard]] extern auto FormatStackTrace(const StackTraceData& st) -> std::string;

// Write the trace to the base log. Used in crash / OOM paths: tries to avoid fresh allocations
// (resolution is best-effort and falls back to hex addresses on failure).
extern void SafeWriteStackTrace(const StackTraceData& st) noexcept;

FO_END_NAMESPACE
