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

#if (FO_WINDOWS || FO_LINUX || FO_MAC) && !FO_MEMORY_SANITIZER
#if !FO_WINDOWS
#if __has_include(<libunwind.h>) && !(FO_MAC && defined(__aarch64__))
#define BACKWARD_HAS_LIBUNWIND 1
#elif __has_include(<bfd.h>)
#define BACKWARD_HAS_BFD 1
#endif
#endif
FO_DISABLE_WARNINGS_PUSH()
#include "backward.hpp"
FO_DISABLE_WARNINGS_POP()
#define HAS_NATIVE_TRACE 1
#else
#define HAS_NATIVE_TRACE 0
#endif

#include "WinApiUndef-Include.h"

FO_BEGIN_NAMESPACE

struct ResolvedNativeFrameCacheEntry
{
    StackTraceFrame Frame {};
    uintptr_t FunctionKey {};
};

struct StackTraceState
{
    std::mutex ProviderLocker {};
    ScriptStackTraceProvider Provider {};
    std::mutex ResolvedNativeFramesLocker {};
    std::unordered_map<uintptr_t, ResolvedNativeFrameCacheEntry> ResolvedNativeFrames {};
    std::deque<uintptr_t> ResolvedNativeFrameOrder {};
#if HAS_NATIVE_TRACE
    std::mutex NativeResolverLocker {};
#endif
};

static void CollectScriptLayers(std::vector<ScriptStackTraceLayer>& out_layers) noexcept;
static void ResolveNativeRange(const StackTraceData& st, uint32_t from, uint32_t to, std::vector<StackTraceFrame>& out) noexcept;
static auto FindLayerNativeAnchor(const StackTraceData& st, const ScriptStackTraceLayer& layer, uint32_t search_from) noexcept -> uint32_t;
static auto SameFrameFunction(NativeStackFrameAddress a, NativeStackFrameAddress b) noexcept -> bool;
static auto ResolveFunctionKey(NativeStackFrameAddress addr) noexcept -> uintptr_t;
static auto ResolveNativeFrame(NativeStackFrameAddress addr, uint32_t index) -> ResolvedNativeFrameCacheEntry;
static auto ResolveNativeFrameUncached(NativeStackFrameAddress addr, uint32_t index) noexcept -> ResolvedNativeFrameCacheEntry;
#if HAS_NATIVE_TRACE
static auto GetNativeTraceResolver() noexcept -> backward::TraceResolver&;
#endif
static auto TryGetResolvedNativeFrameFromCache(NativeStackFrameAddress addr) -> std::optional<ResolvedNativeFrameCacheEntry>;
static void StoreResolvedNativeFrameInCache(NativeStackFrameAddress addr, const ResolvedNativeFrameCacheEntry& entry) noexcept;
static auto MakeNativeAddressCacheEntry(NativeStackFrameAddress addr) noexcept -> ResolvedNativeFrameCacheEntry;
static auto MakeNativeAddressFrame(NativeStackFrameAddress addr) noexcept -> StackTraceFrame;
static auto MakeNativeFunctionKey(NativeStackFrameAddress addr, std::string_view name) noexcept -> uintptr_t;
static auto MakeNativeAddressKey(NativeStackFrameAddress addr) noexcept -> uintptr_t;
static auto IsLowNativeAddress(NativeStackFrameAddress addr) noexcept -> bool;
static auto IsUnresolvedNativeName(std::string_view s) noexcept -> bool;
static void TrimInPlace(std::string& s) noexcept;
static auto GetStackTraceState() noexcept -> StackTraceState&;

extern auto MakeScriptStackTraceLayers(std::vector<ScriptStackTraceLayer>&& layers) -> std::shared_ptr<const std::vector<ScriptStackTraceLayer>>
{
    FO_NO_STACK_TRACE_ENTRY();

    return std::make_shared<const std::vector<ScriptStackTraceLayer>>(std::move(layers));
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
            st.ScriptLayers = MakeScriptStackTraceLayers(std::move(script_layers));
        }
    }
    catch (...) {
        BreakIntoDebugger();
    }

    return st;
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

extern auto FormatStackTrace(const CatchedStackTraceData& st) -> std::string
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

extern void CaptureNativeStackFrames(std::array<NativeStackFrameAddress, STACK_TRACE_MAX_NATIVE_FRAMES>& out_frames, uint32_t& out_count, bool& out_truncated, uint32_t skip) noexcept
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
        out_frames[i] = std::bit_cast<NativeStackFrameAddress>(raw_frames[i]);
    }

    out_count = n;

#elif HAS_NATIVE_TRACE
    try {
        backward::StackTrace native;
        const size_t skip_count = static_cast<size_t>(2) + skip;
        native.load_here(STACK_TRACE_MAX_NATIVE_FRAMES + skip_count + 1);
        native.skip_n_firsts(skip_count);
        out_truncated = native.size() > STACK_TRACE_MAX_NATIVE_FRAMES;
        const size_t count = std::min(native.size(), STACK_TRACE_MAX_NATIVE_FRAMES);

        for (size_t i = 0; i < count; i++) {
            out_frames[i] = std::bit_cast<NativeStackFrameAddress>(native[i].addr);
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

extern void SetScriptStackTraceProvider(ScriptStackTraceProvider provider) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    StackTraceState& state = GetStackTraceState();
    std::scoped_lock locker {state.ProviderLocker};

    state.Provider = std::move(provider);
}

extern auto HasScriptStackTraceProvider() noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    StackTraceState& state = GetStackTraceState();
    std::scoped_lock locker {state.ProviderLocker};

    return !!state.Provider;
}

extern void ClearResolvedStackTraceCache() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        StackTraceState& state = GetStackTraceState();
        std::scoped_lock locker {state.ResolvedNativeFramesLocker};

        state.ResolvedNativeFrames.clear();
        state.ResolvedNativeFrameOrder.clear();
    }
    catch (...) {
        BreakIntoDebugger();
    }
}

extern auto GetResolvedStackTraceCacheSize() noexcept -> size_t
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        StackTraceState& state = GetStackTraceState();
        std::scoped_lock locker {state.ResolvedNativeFramesLocker};

        return state.ResolvedNativeFrames.size();
    }
    catch (...) {
        BreakIntoDebugger();
    }

    return 0;
}

static void CollectScriptLayers(std::vector<ScriptStackTraceLayer>& out_layers) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    ScriptStackTraceProvider provider;

    {
        StackTraceState& state = GetStackTraceState();
        std::scoped_lock locker {state.ProviderLocker};

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

static void ResolveNativeRange(const StackTraceData& st, uint32_t from, uint32_t to, std::vector<StackTraceFrame>& out) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (from >= to) {
        return;
    }

    try {
        for (uint32_t i = from; i < to; i++) {
            const NativeStackFrameAddress addr = st.NativeFrames[i];

            if (addr == 0) {
                continue;
            }

            out.emplace_back(ResolveNativeFrame(addr, i).Frame);
        }
    }
    catch (...) {
        BreakIntoDebugger();
    }
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
        const NativeStackFrameAddress birth_addr = layer.BirthNativeFrames[birth_n - 1 - matched];
        const NativeStackFrameAddress trace_addr = st.NativeFrames[trace_n - 1 - matched];

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

static auto SameFrameFunction(NativeStackFrameAddress a, NativeStackFrameAddress b) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (a == b) {
        return true;
    }

    if (a == 0 || b == 0) {
        return false;
    }

    return ResolveFunctionKey(a) == ResolveFunctionKey(b);
}

static auto ResolveFunctionKey(NativeStackFrameAddress addr) noexcept -> uintptr_t
{
    FO_NO_STACK_TRACE_ENTRY();

    if (IsLowNativeAddress(addr)) {
        return MakeNativeAddressKey(addr);
    }

    try {
        // backward stores the function entry address in `object_base + relative offset` form
        // on POSIX, but on Windows we approximate by collapsing all IPs that resolve to the
        // same symbol name into a single key.
        return ResolveNativeFrame(addr, 0).FunctionKey;
    }
    catch (...) {
        return MakeNativeAddressKey(addr);
    }
}

static auto ResolveNativeFrame(NativeStackFrameAddress addr, uint32_t index) -> ResolvedNativeFrameCacheEntry
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto cached = TryGetResolvedNativeFrameFromCache(addr);

    if (cached.has_value()) {
        return cached.value();
    }

    if (IsLowNativeAddress(addr)) {
        ResolvedNativeFrameCacheEntry entry = MakeNativeAddressCacheEntry(addr);
        StoreResolvedNativeFrameInCache(addr, entry);
        return entry;
    }

    ResolvedNativeFrameCacheEntry entry = ResolveNativeFrameUncached(addr, index);
    StoreResolvedNativeFrameInCache(addr, entry);
    return entry;
}

#if HAS_NATIVE_TRACE
static auto ResolveNativeFrameUncached(NativeStackFrameAddress addr, uint32_t index) noexcept -> ResolvedNativeFrameCacheEntry
{
    FO_NO_STACK_TRACE_ENTRY();

    if (IsLowNativeAddress(addr)) {
        return MakeNativeAddressCacheEntry(addr);
    }

    try {
        backward::ResolvedTrace resolved;

        {
            StackTraceState& state = GetStackTraceState();
            std::scoped_lock locker {state.NativeResolverLocker};

            resolved = GetNativeTraceResolver().resolve(backward::Trace(std::bit_cast<void*>(addr), index));
        }

        StackTraceFrame frame;
        frame.Type = StackTraceFrame::FrameType::Native;
        frame.Function = resolved.source.function.empty() ? resolved.object_function : resolved.source.function;
        frame.File = resolved.source.filename;
        frame.Line = resolved.source.line;

        TrimInPlace(frame.Function);
        TrimInPlace(frame.File);

        if (IsUnresolvedNativeName(frame.Function)) {
            return MakeNativeAddressCacheEntry(addr);
        }

        if (IsUnresolvedNativeName(frame.File)) {
            frame.File.clear();
            frame.Line = 0;
        }

        ResolvedNativeFrameCacheEntry entry;
        entry.FunctionKey = MakeNativeFunctionKey(addr, frame.Function);
        entry.Frame = std::move(frame);
        return entry;
    }
    catch (...) {
        BreakIntoDebugger();
        return MakeNativeAddressCacheEntry(addr);
    }
}

#else
static auto ResolveNativeFrameUncached(NativeStackFrameAddress addr, uint32_t index) noexcept -> ResolvedNativeFrameCacheEntry
{
    FO_NO_STACK_TRACE_ENTRY();

    ignore_unused(index);
    return MakeNativeAddressCacheEntry(addr);
}
#endif

static auto TryGetResolvedNativeFrameFromCache(NativeStackFrameAddress addr) -> std::optional<ResolvedNativeFrameCacheEntry>
{
    FO_NO_STACK_TRACE_ENTRY();

    StackTraceState& state = GetStackTraceState();
    std::scoped_lock locker {state.ResolvedNativeFramesLocker};

    const uintptr_t key = MakeNativeAddressKey(addr);
    const auto it = state.ResolvedNativeFrames.find(key);

    if (it == state.ResolvedNativeFrames.end()) {
        return std::nullopt;
    }

    return it->second;
}

static void StoreResolvedNativeFrameInCache(NativeStackFrameAddress addr, const ResolvedNativeFrameCacheEntry& entry) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        StackTraceState& state = GetStackTraceState();
        std::scoped_lock locker {state.ResolvedNativeFramesLocker};

        const uintptr_t key = MakeNativeAddressKey(addr);

        if (state.ResolvedNativeFrames.find(key) != state.ResolvedNativeFrames.end()) {
            return;
        }

        while (state.ResolvedNativeFrames.size() >= STACK_TRACE_RESOLVE_CACHE_MAX_ENTRIES && !state.ResolvedNativeFrameOrder.empty()) {
            state.ResolvedNativeFrames.erase(state.ResolvedNativeFrameOrder.front());
            state.ResolvedNativeFrameOrder.pop_front();
        }

        if (state.ResolvedNativeFrames.size() >= STACK_TRACE_RESOLVE_CACHE_MAX_ENTRIES) {
            state.ResolvedNativeFrames.clear();
            state.ResolvedNativeFrameOrder.clear();
        }

        state.ResolvedNativeFrames.emplace(key, entry);
        state.ResolvedNativeFrameOrder.emplace_back(key);
    }
    catch (...) {
        // The cache is opportunistic; symbol resolution itself must still succeed if caching cannot allocate.
    }
}

static auto MakeNativeAddressCacheEntry(NativeStackFrameAddress addr) noexcept -> ResolvedNativeFrameCacheEntry
{
    FO_NO_STACK_TRACE_ENTRY();

    ResolvedNativeFrameCacheEntry entry;
    entry.Frame = MakeNativeAddressFrame(addr);
    entry.FunctionKey = MakeNativeAddressKey(addr);
    return entry;
}

static auto MakeNativeAddressFrame(NativeStackFrameAddress addr) noexcept -> StackTraceFrame
{
    FO_NO_STACK_TRACE_ENTRY();

    StackTraceFrame frame;
    frame.Type = StackTraceFrame::FrameType::Native;

    char hex_buf[32];
    (void)std::snprintf(hex_buf, sizeof(hex_buf), "%p", std::bit_cast<void*>(addr));
    frame.Function = hex_buf;

    return frame;
}

static auto MakeNativeFunctionKey(NativeStackFrameAddress addr, std::string_view name) noexcept -> uintptr_t
{
    FO_NO_STACK_TRACE_ENTRY();

    if (IsUnresolvedNativeName(name)) {
        return MakeNativeAddressKey(addr);
    }

    return static_cast<uintptr_t>(std::hash<std::string_view> {}(name));
}

static auto MakeNativeAddressKey(NativeStackFrameAddress addr) noexcept -> uintptr_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return addr;
}

static auto IsLowNativeAddress(NativeStackFrameAddress addr) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    // Unit tests synthesize native frames with small integer addresses. These are not
    // userspace instruction pointers on supported native platforms, and POSIX symbol
    // resolvers can be extremely slow or return the same "??" symbol for all of them.
    return addr < 0x10000U;
}

static auto IsUnresolvedNativeName(std::string_view s) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return s.empty() || s == "??" || s == "???" || s == "??:0";
}

static void TrimInPlace(std::string& s) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    const size_t first = s.find_first_not_of(" \t\r\n");

    if (first == std::string::npos) {
        s.clear();
        return;
    }

    const size_t last = s.find_last_not_of(" \t\r\n");
    s.erase(last + 1);
    s.erase(0, first);
}

static auto GetStackTraceState() noexcept -> StackTraceState&
{
    FO_NO_STACK_TRACE_ENTRY();

    static StackTraceState state;
    return state;
}

#if HAS_NATIVE_TRACE
static auto GetNativeTraceResolver() noexcept -> backward::TraceResolver&
{
    FO_NO_STACK_TRACE_ENTRY();

    // Intentionally process-lifetime and never destroyed. backward-cpp resolves native frames through
    // libbfd, which slurps each binary's symbol table and DWARF debug info into caches hung off the open
    // bfd handle and never fully releases them on bfd_close. Reusing one resolver loads every binary at
    // most once (instead of once per resolved range/key), and keeping it reachable from this static root
    // for the whole process keeps those libbfd caches reachable, so LeakSanitizer does not report them.
    // The resolver is not thread-safe; callers serialize access via StackTraceState::NativeResolverLocker.
    static backward::TraceResolver* resolver = new backward::TraceResolver();
    return *resolver;
}
#endif

FO_END_NAMESPACE
