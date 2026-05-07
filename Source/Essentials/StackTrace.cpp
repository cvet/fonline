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

#include "StackTrace.h"
#include "GlobalData.h"

#if FO_WINDOWS || FO_LINUX || FO_MAC
#if !FO_WINDOWS
#if __has_include(<libunwind.h>) && !(FO_MAC && defined(__aarch64__))
#define BACKWARD_HAS_LIBUNWIND 1
#elif __has_include(<bfd.h>)
#define BACKWARD_HAS_BFD 1
#endif
#endif
#include "backward.hpp"
#define HAS_NATIVE_TRACE 1
#else
#define HAS_NATIVE_TRACE 0
#endif

#include "WinApiUndef-Include.h"

FO_BEGIN_NAMESPACE

struct StackTraceState
{
    std::mutex ProviderLocker {};
    ScriptStackTraceProvider Provider {};
};

static auto GetStackTraceState() noexcept -> StackTraceState&
{
    static StackTraceState state;
    return state;
}

extern void SetScriptStackTraceProvider(ScriptStackTraceProvider provider) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    auto& state = GetStackTraceState();
    std::scoped_lock locker(state.ProviderLocker);
    state.Provider = std::move(provider);
}

extern auto HasScriptStackTraceProvider() noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    auto& state = GetStackTraceState();
    std::scoped_lock locker(state.ProviderLocker);
    return !!state.Provider;
}

static void CollectScriptLayers(std::vector<ScriptStackTraceLayer>& out_layers) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    ScriptStackTraceProvider provider;

    {
        auto& state = GetStackTraceState();
        std::scoped_lock locker(state.ProviderLocker);
        provider = state.Provider;
    }

    if (provider) {
        try {
            provider(out_layers);
        }
        catch (...) {
            BreakIntoDebugger();
        }
    }
}

extern void CaptureNativeStackFrames(std::array<void*, STACK_TRACE_MAX_NATIVE_FRAMES>& out_frames, uint32_t& out_count, bool& out_truncated, uint32_t skip) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    out_count = 0;
    out_truncated = false;

#if FO_WINDOWS
    const ULONG skip_count = 1u + skip;
    constexpr ULONG REQUEST_COUNT = static_cast<ULONG>(STACK_TRACE_MAX_NATIVE_FRAMES) + 1u;
    void* raw_frames[REQUEST_COUNT] = {};
    const USHORT captured = RtlCaptureStackBackTrace(skip_count, REQUEST_COUNT, raw_frames, nullptr);
    out_truncated = captured > STACK_TRACE_MAX_NATIVE_FRAMES;
    const uint32_t n = std::min<uint32_t>(captured, static_cast<uint32_t>(STACK_TRACE_MAX_NATIVE_FRAMES));

    for (uint32_t i = 0; i < n; i++) {
        out_frames[i] = raw_frames[i];
    }

    out_count = n;

#elif HAS_NATIVE_TRACE
    try {
        backward::StackTrace native;
        native.load_here(STACK_TRACE_MAX_NATIVE_FRAMES + 1);
        out_truncated = native.size() > STACK_TRACE_MAX_NATIVE_FRAMES;
        native.skip_n_firsts(static_cast<size_t>(2) + skip);
        const size_t count = std::min(native.size(), STACK_TRACE_MAX_NATIVE_FRAMES);

        for (size_t i = 0; i < count; i++) {
            out_frames[i] = native[i].addr;
        }

        out_count = static_cast<uint32_t>(count);
    }
    catch (...) {
        out_count = 0;
    }

#else
    (void)out_frames;
    (void)skip;
#endif
}

extern auto GetStackTrace() noexcept -> StackTraceData
{
    FO_NO_STACK_TRACE_ENTRY();

    StackTraceData st;

    CaptureNativeStackFrames(st.NativeFrames, st.NativeFrameCount, st.NativeTruncated, 1);

    try {
        std::vector<ScriptStackTraceLayer> script_layers;
        CollectScriptLayers(script_layers);

        if (!script_layers.empty()) {
            st.ScriptLayers = std::make_shared<const std::vector<ScriptStackTraceLayer>>(std::move(script_layers));
        }
    }
    catch (...) {
        BreakIntoDebugger();
    }

    return st;
}

static void ResolveNativeRange(const StackTraceData& st, uint32_t from, uint32_t to, std::vector<StackTraceFrame>& out) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (from >= to) {
        return;
    }

#if HAS_NATIVE_TRACE
    const auto trim_in_place = [](std::string& s) {
        const auto first = s.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            s.clear();
            return;
        }
        const auto last = s.find_last_not_of(" \t\r\n");
        s.erase(last + 1);
        s.erase(0, first);
    };

    const auto is_unresolved_name = [](const std::string& s) { return s.empty() || s == "??" || s == "???" || s == "??:0"; };

    try {
        backward::TraceResolver resolver;

        for (uint32_t i = from; i < to; i++) {
            void* addr = st.NativeFrames[i];

            if (addr == nullptr) {
                continue;
            }

            backward::ResolvedTrace resolved = resolver.resolve(backward::Trace(addr, i));

            StackTraceFrame frame;
            frame.Type = StackTraceFrame::FrameType::Native;
            frame.Function = resolved.source.function.empty() ? resolved.object_function : resolved.source.function;
            frame.File = resolved.source.filename;
            frame.Line = resolved.source.line;

            trim_in_place(frame.Function);
            trim_in_place(frame.File);

            if (is_unresolved_name(frame.Function)) {
                char hex_buf[32];
                (void)std::snprintf(hex_buf, sizeof(hex_buf), "%p", addr);
                frame.Function = hex_buf;
            }

            if (is_unresolved_name(frame.File)) {
                frame.File.clear();
                frame.Line = 0;
            }

            out.emplace_back(std::move(frame));
        }
    }
    catch (...) {
        BreakIntoDebugger();
    }

#else
    (void)st;
    (void)out;
#endif
}

static auto ResolveFunctionBase(void* addr) noexcept -> void*
{
    FO_NO_STACK_TRACE_ENTRY();

#if HAS_NATIVE_TRACE
    try {
        backward::TraceResolver resolver;
        const auto resolved = resolver.resolve(backward::Trace(addr, 0));

        if (resolved.object_function.empty() && resolved.source.function.empty()) {
            return addr;
        }

        // backward stores the function entry address in `object_base + relative offset` form
        // on POSIX, but on Windows we approximate via the resolved.addr field which points
        // to the IP back; for matching we collapse all IPs that resolve to the same name
        // into a single key by hashing the function name.
        const auto& name = !resolved.source.function.empty() ? resolved.source.function : resolved.object_function;
        size_t h = std::hash<std::string> {}(name);
        return reinterpret_cast<void*>(static_cast<uintptr_t>(h));
    }
    catch (...) {
        return addr;
    }
#else
    return addr;
#endif
}

static auto SameFrameFunction(void* a, void* b) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (a == b) {
        return true;
    }

    if (a == nullptr || b == nullptr) {
        return false;
    }

    return ResolveFunctionBase(a) == ResolveFunctionBase(b);
}

static auto FindLayerNativeAnchor(const StackTraceData& st, const ScriptStackTraceLayer& layer, uint32_t search_from) noexcept -> uint32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    if (layer.BirthNativeFrameCount == 0) {
        return st.NativeFrameCount;
    }

    const uint32_t birth_n = layer.BirthNativeFrameCount;
    const uint32_t trace_n = st.NativeFrameCount;

    uint32_t matched = 0;

    while (matched < birth_n && matched < trace_n) {
        void* birth_addr = layer.BirthNativeFrames[birth_n - 1 - matched];
        void* trace_addr = st.NativeFrames[trace_n - 1 - matched];

        if (!SameFrameFunction(birth_addr, trace_addr)) {
            break;
        }

        matched++;
    }

    if (matched == 0) {
        return st.NativeFrameCount;
    }

    const uint32_t anchor = trace_n - matched;

    if (anchor < search_from) {
        return st.NativeFrameCount;
    }

    return anchor;
}

extern auto ResolveStackTrace(const StackTraceData& st) -> std::vector<StackTraceFrame>
{
    FO_NO_STACK_TRACE_ENTRY();

    std::vector<StackTraceFrame> frames;

    if (!st.ScriptLayers || st.ScriptLayers->empty()) {
        frames.reserve(st.NativeFrameCount);
        ResolveNativeRange(st, 0, st.NativeFrameCount, frames);
        return frames;
    }

    const auto& layers = *st.ScriptLayers;

    size_t reserve_count = st.NativeFrameCount;

    for (const auto& layer : layers) {
        reserve_count += layer.ScriptFrames.size();
    }

    frames.reserve(reserve_count);

    uint32_t prev_anchor = 0;

    for (const auto& layer : layers) {
        const uint32_t anchor = FindLayerNativeAnchor(st, layer, prev_anchor);

        if (anchor < st.NativeFrameCount && anchor > prev_anchor) {
            ResolveNativeRange(st, prev_anchor, anchor, frames);
            prev_anchor = anchor;
        }

        for (const auto& frame : layer.ScriptFrames) {
            frames.push_back(frame);
        }
    }

    if (prev_anchor < st.NativeFrameCount) {
        ResolveNativeRange(st, prev_anchor, st.NativeFrameCount, frames);
    }

    return frames;
}

extern auto GetStackTraceEntry(uint32_t deep) noexcept -> std::optional<StackTraceFrame>
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        const auto resolved = ResolveStackTrace(GetStackTrace());

        if (deep < resolved.size()) {
            return resolved[deep];
        }
    }
    catch (...) {
        BreakIntoDebugger();
    }

    return std::nullopt;
}

extern auto FormatStackTrace(const StackTraceData& st) -> std::string
{
    FO_NO_STACK_TRACE_ENTRY();

    std::ostringstream ss;
    ss << "Stack trace (most recent call first";

    if (st.NativeTruncated) {
        ss << ", truncated at " << STACK_TRACE_MAX_NATIVE_FRAMES << " frames";
    }

    ss << "):";

    for (const auto& frame : ResolveStackTrace(st)) {
        ss << "\n- [" << (frame.Type == StackTraceFrame::FrameType::Script ? "Script" : "Native") << "] " << frame.Function;

        if (!frame.File.empty()) {
            std::string_view file_name {frame.File};

            if (const auto pos = file_name.find_last_of("/\\"); pos != std::string_view::npos) {
                file_name = file_name.substr(pos + 1);
            }

            ss << " (" << file_name << " line " << frame.Line << ")";
        }
    }

    return ss.str();
}

auto FormatStackTrace(const CatchedStackTraceData& st) -> std::string
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!st.Origin.has_value()) {
        return "Catched at: " + FormatStackTrace(st.Catched);
    }

    const auto origin_formatted = FormatStackTrace(*st.Origin);
    const auto catched_st = FormatStackTrace(st.Catched);

    // Skip 'Stack trace (most recent ...'
    auto pos = catched_st.find('\n');

    if (pos == std::string::npos) {
        return origin_formatted;
    }

    // Find stack traces intersection
    pos = origin_formatted.find(catched_st.substr(pos + 1));

    if (pos == std::string::npos) {
        return origin_formatted;
    }

    // Insert at end of line
    pos = origin_formatted.find('\n', pos);
    return origin_formatted.substr(0, pos).append(" <- Catched here").append(pos != std::string::npos ? origin_formatted.substr(pos) : "");
}

FO_END_NAMESPACE
