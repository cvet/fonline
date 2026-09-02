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

#include "StackTrace.h"

#if (FO_WINDOWS || FO_LINUX || FO_MAC) && !FO_MEMORY_SANITIZER && !FO_THREAD_SANITIZER
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

#include "WinApiUndef.inc"

FO_BEGIN_NAMESPACE

struct resolved_native_frame_cache_entry
{
    stack_trace::frame frame {};
    uintptr_t function_key {};
};

struct stack_trace_state
{
    std::mutex provider_locker {};
    stack_trace::script_provider provider {};
    std::mutex resolved_native_frames_locker {};
    std::unordered_map<uintptr_t, resolved_native_frame_cache_entry> resolved_native_frames {};
    std::deque<uintptr_t> resolved_native_frame_order {};
#if HAS_NATIVE_TRACE
    std::mutex native_resolver_locker {};
#endif
};

static void collect_script_layers(std::vector<stack_trace::script_layer>& out_layers) noexcept;
static void resolve_native_range(const stack_trace::data& st, uint32_t from, uint32_t to, std::vector<stack_trace::frame>& out) noexcept;
static auto find_layer_native_anchor(const stack_trace::data& st, const stack_trace::script_layer& layer, uint32_t search_from) noexcept -> uint32_t;
static auto same_frame_function(stack_trace::native_frame_address a, stack_trace::native_frame_address b) noexcept -> bool;
static auto resolve_function_key(stack_trace::native_frame_address addr) noexcept -> uintptr_t;
static auto resolve_native_frame(stack_trace::native_frame_address addr, uint32_t index) -> resolved_native_frame_cache_entry;
static auto resolve_native_frame_uncached(stack_trace::native_frame_address addr, uint32_t index) noexcept -> resolved_native_frame_cache_entry;
#if HAS_NATIVE_TRACE
static auto get_native_trace_resolver() noexcept -> backward::TraceResolver&;
#endif
static auto try_get_resolved_native_frame_from_cache(stack_trace::native_frame_address addr) -> std::optional<resolved_native_frame_cache_entry>;
static void store_resolved_native_frame_in_cache(stack_trace::native_frame_address addr, const resolved_native_frame_cache_entry& entry) noexcept;
static auto make_native_address_cache_entry(stack_trace::native_frame_address addr) noexcept -> resolved_native_frame_cache_entry;
static auto make_native_address_frame(stack_trace::native_frame_address addr) noexcept -> stack_trace::frame;
static auto make_native_function_key(stack_trace::native_frame_address addr, std::string_view name) noexcept -> uintptr_t;
static auto make_native_address_key(stack_trace::native_frame_address addr) noexcept -> uintptr_t;
static auto is_low_native_address(stack_trace::native_frame_address addr) noexcept -> bool;
static auto is_unresolved_native_name(std::string_view s) noexcept -> bool;
static void trim_in_place(std::string& s) noexcept;
static auto get_stack_trace_state() noexcept -> stack_trace_state&;

auto stack_trace::get() noexcept -> stack_trace::data
{
    FO_NO_STACK_TRACE_ENTRY();

    stack_trace::data st;

    stack_trace::capture_native_frames(st.native_frames, st.native_frame_count, st.native_truncated, 1);

    try {
        std::vector<stack_trace::script_layer> script_layers;
        collect_script_layers(script_layers);

        if (!script_layers.empty()) {
            st.script_layers = std::make_shared<const std::vector<stack_trace::script_layer>>(std::move(script_layers));
        }
    }
    catch (...) {
        break_into_debugger();
    }

    return st;
}

auto stack_trace::resolve(const stack_trace::data& st) -> std::vector<stack_trace::frame>
{
    FO_NO_STACK_TRACE_ENTRY();

    std::vector<stack_trace::frame> frames;

    if (!st.script_layers || st.script_layers->empty()) {
        frames.reserve(st.native_frame_count);
        resolve_native_range(st, 0, st.native_frame_count, frames);
        return frames;
    }

    const auto& layers = *st.script_layers;

    size_t reserve_count = st.native_frame_count;

    for (const auto& layer : layers) {
        reserve_count += layer.script_frames.size();
    }

    frames.reserve(reserve_count);

    uint32_t prev_anchor = 0;

    for (const auto& layer : layers) {
        uint32_t anchor = find_layer_native_anchor(st, layer, prev_anchor);

        if (anchor < st.native_frame_count && anchor > prev_anchor) {
            resolve_native_range(st, prev_anchor, anchor, frames);
            prev_anchor = anchor;
        }

        for (const auto& frame : layer.script_frames) {
            frames.push_back(frame);
        }
    }

    if (prev_anchor < st.native_frame_count) {
        resolve_native_range(st, prev_anchor, st.native_frame_count, frames);
    }

    return frames;
}

auto stack_trace::format(const stack_trace::data& st) -> std::string
{
    FO_NO_STACK_TRACE_ENTRY();

    std::ostringstream ss;
    ss << "Stack trace (most recent call first";

    if (st.native_truncated) {
        ss << ", truncated at " << stack_trace::MAX_NATIVE_FRAMES << " frames";
    }

    ss << "):";

    for (const auto& frame : stack_trace::resolve(st)) {
        ss << "\n- [" << (frame.type == stack_trace::frame::frame_type::script ? "Script" : "Native") << "] " << frame.function;

        if (!frame.file.empty()) {
            std::string_view file_name {frame.file};

            if (auto pos = file_name.find_last_of("/\\"); pos != std::string_view::npos) {
                file_name = file_name.substr(pos + 1);
            }

            ss << " (" << file_name << " line " << frame.line << ")";
        }
    }

    return ss.str();
}

auto stack_trace::format(const stack_trace::catched_data& st) -> std::string
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!st.origin.has_value()) {
        return "Catched at: " + stack_trace::format(st.catched);
    }

    auto origin_formatted = stack_trace::format(*st.origin);
    auto catched_st = stack_trace::format(st.catched);

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

auto stack_trace::get_entry(uint32_t deep) noexcept -> std::optional<stack_trace::frame>
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        auto resolved = stack_trace::resolve(stack_trace::get());

        if (deep < resolved.size()) {
            return resolved[deep];
        }
    }
    catch (...) {
        break_into_debugger();
    }

    return std::nullopt;
}

void stack_trace::capture_native_frames(std::array<stack_trace::native_frame_address, stack_trace::MAX_NATIVE_FRAMES>& out_frames, uint32_t& out_count, bool& out_truncated, uint32_t skip) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    out_count = 0;
    out_truncated = false;

#if FO_WINDOWS
    ULONG skip_count = 1u + skip;
    constexpr ULONG REQUEST_COUNT = static_cast<ULONG>(stack_trace::MAX_NATIVE_FRAMES) + 1u;
    void* raw_frames[REQUEST_COUNT];
    USHORT captured = RtlCaptureStackBackTrace(skip_count, REQUEST_COUNT, raw_frames, nullptr);
    out_truncated = captured > stack_trace::MAX_NATIVE_FRAMES;
    uint32_t n = std::min<uint32_t>(captured, static_cast<uint32_t>(stack_trace::MAX_NATIVE_FRAMES));

    for (uint32_t i = 0; i < n; i++) {
        out_frames[i] = std::bit_cast<stack_trace::native_frame_address>(raw_frames[i]);
    }

    out_count = n;

#elif HAS_NATIVE_TRACE
    try {
        backward::StackTrace native;
        size_t skip_count = static_cast<size_t>(2) + skip;
        native.load_here(stack_trace::MAX_NATIVE_FRAMES + skip_count + 1);
        native.skip_n_firsts(skip_count);
        out_truncated = native.size() > stack_trace::MAX_NATIVE_FRAMES;
        size_t count = std::min(native.size(), stack_trace::MAX_NATIVE_FRAMES);

        for (size_t i = 0; i < count; i++) {
            out_frames[i] = std::bit_cast<stack_trace::native_frame_address>(native[i].addr);
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

void stack_trace::set_script_provider(stack_trace::script_provider provider) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    stack_trace_state& state = get_stack_trace_state();
    std::scoped_lock locker {state.provider_locker};

    state.provider = std::move(provider);
}

auto stack_trace::has_script_provider() noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    stack_trace_state& state = get_stack_trace_state();
    std::scoped_lock locker {state.provider_locker};

    return !!state.provider;
}

void stack_trace::clear_resolved_cache() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        stack_trace_state& state = get_stack_trace_state();
        std::scoped_lock locker {state.resolved_native_frames_locker};

        state.resolved_native_frames.clear();
        state.resolved_native_frame_order.clear();
    }
    catch (...) {
        break_into_debugger();
    }
}

auto stack_trace::get_resolved_cache_size() noexcept -> size_t
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        stack_trace_state& state = get_stack_trace_state();
        std::scoped_lock locker {state.resolved_native_frames_locker};

        return state.resolved_native_frames.size();
    }
    catch (...) {
        break_into_debugger();
    }

    return 0;
}

static void collect_script_layers(std::vector<stack_trace::script_layer>& out_layers) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    stack_trace::script_provider provider;

    {
        stack_trace_state& state = get_stack_trace_state();
        std::scoped_lock locker {state.provider_locker};

        provider = state.provider;
    }

    if (provider) {
        try {
            provider(out_layers);
        }
        catch (...) {
            break_into_debugger();
        }
    }
}

static void resolve_native_range(const stack_trace::data& st, uint32_t from, uint32_t to, std::vector<stack_trace::frame>& out) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (from >= to) {
        return;
    }

    try {
        for (uint32_t i = from; i < to; i++) {
            stack_trace::native_frame_address addr = st.native_frames[i];

            if (addr == 0) {
                continue;
            }

            out.emplace_back(resolve_native_frame(addr, i).frame);
        }
    }
    catch (...) {
        break_into_debugger();
    }
}

static auto find_layer_native_anchor(const stack_trace::data& st, const stack_trace::script_layer& layer, uint32_t search_from) noexcept -> uint32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    if (layer.birth_native_frame_count == 0) {
        return st.native_frame_count;
    }

    uint32_t birth_n = layer.birth_native_frame_count;
    uint32_t trace_n = st.native_frame_count;

    uint32_t matched = 0;

    while (matched < birth_n && matched < trace_n) {
        stack_trace::native_frame_address birth_addr = layer.birth_native_frames[birth_n - 1 - matched];
        stack_trace::native_frame_address trace_addr = st.native_frames[trace_n - 1 - matched];

        if (!same_frame_function(birth_addr, trace_addr)) {
            break;
        }

        matched++;
    }

    if (matched == 0) {
        return st.native_frame_count;
    }

    uint32_t anchor = trace_n - matched;

    if (anchor < search_from) {
        return st.native_frame_count;
    }

    return anchor;
}

static auto same_frame_function(stack_trace::native_frame_address a, stack_trace::native_frame_address b) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (a == b) {
        return true;
    }

    if (a == 0 || b == 0) {
        return false;
    }

    return resolve_function_key(a) == resolve_function_key(b);
}

static auto resolve_function_key(stack_trace::native_frame_address addr) noexcept -> uintptr_t
{
    FO_NO_STACK_TRACE_ENTRY();

    if (is_low_native_address(addr)) {
        return make_native_address_key(addr);
    }

    try {
        // POSIX exposes object-relative function entries; Windows approximates them by symbol name
        return resolve_native_frame(addr, 0).function_key;
    }
    catch (...) {
        return make_native_address_key(addr);
    }
}

static auto resolve_native_frame(stack_trace::native_frame_address addr, uint32_t index) -> resolved_native_frame_cache_entry
{
    FO_NO_STACK_TRACE_ENTRY();

    auto cached = try_get_resolved_native_frame_from_cache(addr);

    if (cached.has_value()) {
        return cached.value();
    }

    if (is_low_native_address(addr)) {
        resolved_native_frame_cache_entry entry = make_native_address_cache_entry(addr);
        store_resolved_native_frame_in_cache(addr, entry);
        return entry;
    }

    resolved_native_frame_cache_entry entry = resolve_native_frame_uncached(addr, index);
    store_resolved_native_frame_in_cache(addr, entry);
    return entry;
}

#if HAS_NATIVE_TRACE
static auto resolve_native_frame_uncached(stack_trace::native_frame_address addr, uint32_t index) noexcept -> resolved_native_frame_cache_entry
{
    FO_NO_STACK_TRACE_ENTRY();

    if (is_low_native_address(addr)) {
        return make_native_address_cache_entry(addr);
    }

    try {
        backward::ResolvedTrace resolved;

        {
            stack_trace_state& state = get_stack_trace_state();
            std::scoped_lock locker {state.native_resolver_locker};

            resolved = get_native_trace_resolver().resolve(backward::Trace(std::bit_cast<void*>(addr), index));
        }

        stack_trace::frame frame;
        frame.type = stack_trace::frame::frame_type::native;
        frame.function = resolved.source.function.empty() ? resolved.object_function : resolved.source.function;
        frame.file = resolved.source.filename;
        frame.line = resolved.source.line;

        trim_in_place(frame.function);
        trim_in_place(frame.file);

        if (is_unresolved_native_name(frame.function)) {
            return make_native_address_cache_entry(addr);
        }

        if (is_unresolved_native_name(frame.file)) {
            frame.file.clear();
            frame.line = 0;
        }

        resolved_native_frame_cache_entry entry;
        entry.function_key = make_native_function_key(addr, frame.function);
        entry.frame = std::move(frame);
        return entry;
    }
    catch (...) {
        break_into_debugger();
        return make_native_address_cache_entry(addr);
    }
}

#else
static auto resolve_native_frame_uncached(stack_trace::native_frame_address addr, uint32_t index) noexcept -> resolved_native_frame_cache_entry
{
    FO_NO_STACK_TRACE_ENTRY();

    ignore_unused(index);
    return make_native_address_cache_entry(addr);
}
#endif

static auto try_get_resolved_native_frame_from_cache(stack_trace::native_frame_address addr) -> std::optional<resolved_native_frame_cache_entry>
{
    FO_NO_STACK_TRACE_ENTRY();

    stack_trace_state& state = get_stack_trace_state();
    std::scoped_lock locker {state.resolved_native_frames_locker};

    uintptr_t key = make_native_address_key(addr);
    auto it = state.resolved_native_frames.find(key);

    if (it == state.resolved_native_frames.end()) {
        return std::nullopt;
    }

    return it->second;
}

static void store_resolved_native_frame_in_cache(stack_trace::native_frame_address addr, const resolved_native_frame_cache_entry& entry) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        stack_trace_state& state = get_stack_trace_state();
        std::scoped_lock locker {state.resolved_native_frames_locker};

        uintptr_t key = make_native_address_key(addr);

        if (state.resolved_native_frames.find(key) != state.resolved_native_frames.end()) {
            return;
        }

        while (state.resolved_native_frames.size() >= stack_trace::RESOLVE_CACHE_MAX_ENTRIES && !state.resolved_native_frame_order.empty()) {
            state.resolved_native_frames.erase(state.resolved_native_frame_order.front());
            state.resolved_native_frame_order.pop_front();
        }

        if (state.resolved_native_frames.size() >= stack_trace::RESOLVE_CACHE_MAX_ENTRIES) {
            state.resolved_native_frames.clear();
            state.resolved_native_frame_order.clear();
        }

        state.resolved_native_frames.emplace(key, entry);
        state.resolved_native_frame_order.emplace_back(key);
    }
    catch (...) {
        // The cache is opportunistic; symbol resolution itself must still succeed if caching cannot allocate
    }
}

static auto make_native_address_cache_entry(stack_trace::native_frame_address addr) noexcept -> resolved_native_frame_cache_entry
{
    FO_NO_STACK_TRACE_ENTRY();

    resolved_native_frame_cache_entry entry;
    entry.frame = make_native_address_frame(addr);
    entry.function_key = make_native_address_key(addr);
    return entry;
}

static auto make_native_address_frame(stack_trace::native_frame_address addr) noexcept -> stack_trace::frame
{
    FO_NO_STACK_TRACE_ENTRY();

    stack_trace::frame frame;
    frame.type = stack_trace::frame::frame_type::native;

    char hex_buf[32];
    (void)std::snprintf(hex_buf, sizeof(hex_buf), "%p", std::bit_cast<void*>(addr));
    frame.function = hex_buf;

    return frame;
}

static auto make_native_function_key(stack_trace::native_frame_address addr, std::string_view name) noexcept -> uintptr_t
{
    FO_NO_STACK_TRACE_ENTRY();

    if (is_unresolved_native_name(name)) {
        return make_native_address_key(addr);
    }

    return static_cast<uintptr_t>(std::hash<std::string_view> {}(name));
}

static auto make_native_address_key(stack_trace::native_frame_address addr) noexcept -> uintptr_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return addr;
}

static auto is_low_native_address(stack_trace::native_frame_address addr) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    // Small synthetic test addresses are not valid native instruction pointers.
    // Skip slow and ambiguous POSIX symbol resolution for them
    return addr < 0x10000U;
}

static auto is_unresolved_native_name(std::string_view s) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return s.empty() || s == "??" || s == "???" || s == "??:0";
}

static void trim_in_place(std::string& s) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    size_t first = s.find_first_not_of(" \t\r\n");

    if (first == std::string::npos) {
        s.clear();
        return;
    }

    size_t last = s.find_last_not_of(" \t\r\n");
    s.erase(last + 1);
    s.erase(0, first);
}

static auto get_stack_trace_state() noexcept -> stack_trace_state&
{
    FO_NO_STACK_TRACE_ENTRY();

    static stack_trace_state state;
    return state;
}

#if HAS_NATIVE_TRACE
static auto get_native_trace_resolver() noexcept -> backward::TraceResolver&
{
    FO_NO_STACK_TRACE_ENTRY();

    // Keep one process-lifetime resolver so libbfd caches each binary once and remains reachable to LeakSanitizer.
    // native_resolver_locker serializes this non-thread-safe object
    static backward::TraceResolver* resolver = new backward::TraceResolver();
    return *resolver;
}
#endif

FO_END_NAMESPACE
