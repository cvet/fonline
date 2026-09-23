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
#define HAS_NATIVE_TRACE 1
#else
#define HAS_NATIVE_TRACE 0
#endif

// Stack walking and symbol lookup sit below the operating system modules in the Essentials order, so this module calls
// the platform itself
#if HAS_NATIVE_TRACE
#if FO_WINDOWS
#include <Windows.h>
#include <dbghelp.h>
#else
#include <cxxabi.h>
#include <dlfcn.h>
#if FO_LINUX
#include "backtrace.h"
#include "libunwind.h"
#elif FO_MAC
#include <libunwind.h>
#endif
#endif
#endif

#include "WinApiUndef.inc"

FO_BEGIN_NAMESPACE

struct resolved_native_frame_cache_entry
{
    // Innermost inlined call first, the function that owns the machine frame last
    std::vector<stack_trace::frame> frames {};
    uintptr_t function_key {};
};

struct stack_trace_state
{
    std::mutex provider_locker {};
    std::vector<std::pair<std::string, stack_trace::script_provider>> providers {};
    std::mutex resolved_native_frames_locker {};
    std::unordered_map<uintptr_t, resolved_native_frame_cache_entry> resolved_native_frames {};
    std::deque<uintptr_t> resolved_native_frame_order {};
#if HAS_NATIVE_TRACE
    // Serializes the symbol engines below, neither of which is thread safe
    std::mutex native_resolver_locker {};
#endif
#if HAS_NATIVE_TRACE && FO_WINDOWS
    bool dbghelp_ready {};
#endif
#if HAS_NATIVE_TRACE && FO_LINUX
    bool backtrace_created {};
    backtrace_state* backtrace {};
#endif
};

#if HAS_NATIVE_TRACE && !FO_WINDOWS
enum class native_first_frame : uint8_t
{
    omitted,
    call_site,
    faulting,
};
#endif

static void attach_script_layers(stack_trace::data& st) noexcept;
static void drop_requesting_constructors(const stack_trace::data& st, std::vector<stack_trace::frame>& frames) noexcept;
static void collect_script_layers(const stack_trace::data& st, std::vector<stack_trace::script_layer>& out_layers) noexcept;
static void ResolveLayerRegion(std::span<const stack_trace::native_frame_address> source, uint32_t from, uint32_t to, const std::vector<stack_trace::native_frame_address>& hidden, const stack_trace::script_layer& layer, bool collapse_head, std::vector<stack_trace::frame>& out);
static void resolve_native_range(std::span<const stack_trace::native_frame_address> frames, uint32_t from, uint32_t to, const std::vector<stack_trace::native_frame_address>& hidden, bool collapse_head, std::vector<stack_trace::frame>& out) noexcept;
static auto find_layer_native_anchor(std::span<const stack_trace::native_frame_address> trace, const stack_trace::script_layer& layer, uint32_t search_from) noexcept -> uint32_t;
static auto FindLayerBirthOverlap(std::span<const stack_trace::native_frame_address> trace, const stack_trace::script_layer& layer, uint32_t search_from) noexcept -> uint32_t;
static auto same_frame_function(stack_trace::native_frame_address a, stack_trace::native_frame_address b) noexcept -> bool;
static auto resolve_function_key(stack_trace::native_frame_address addr) noexcept -> uintptr_t;
static auto resolve_native_frame(stack_trace::native_frame_address addr) -> resolved_native_frame_cache_entry;
static auto resolve_native_frame_uncached(stack_trace::native_frame_address addr) noexcept -> resolved_native_frame_cache_entry;
#if HAS_NATIVE_TRACE && FO_WINDOWS
static void walk_windows_context(const CONTEXT& source, HANDLE thread, std::array<stack_trace::native_frame_address, stack_trace::MAX_NATIVE_FRAMES>& out_frames, uint32_t& out_count, bool& out_truncated) noexcept;
static auto resolve_windows_symbol(stack_trace_state& state, stack_trace::native_frame_address addr, stack_trace::frame& out_frame) noexcept -> bool;
static void prepare_dbghelp(stack_trace_state& state) noexcept;
static auto make_symbol_search_path(const void* module_anchor) -> std::wstring;
#if defined(_M_IX86)
static void walk_windows_frame_chain(uintptr_t pc, uintptr_t frame, uint32_t skip, std::array<stack_trace::native_frame_address, stack_trace::MAX_NATIVE_FRAMES>& out_frames, uint32_t& out_count, bool& out_truncated) noexcept;
#else
static void walk_windows_function_tables(CONTEXT& context, uint32_t skip, std::array<stack_trace::native_frame_address, stack_trace::MAX_NATIVE_FRAMES>& out_frames, uint32_t& out_count, bool& out_truncated) noexcept;
static auto unwind_windows_frame(CONTEXT& context) noexcept -> bool;
static void store_resume_registers(const CONTEXT& context, stack_trace::resume_point& point) noexcept;
static void load_resume_registers(const stack_trace::resume_point& point, CONTEXT& context) noexcept;
#endif
#elif HAS_NATIVE_TRACE
static void walk_native_cursor(unw_cursor_t& cursor, native_first_frame first, bool from_signal_frame, uint32_t skip, std::array<stack_trace::native_frame_address, stack_trace::MAX_NATIVE_FRAMES>& out_frames, uint32_t& out_count, bool& out_truncated) noexcept;
static auto walk_signal_context(const ucontext_t& uctx, std::array<stack_trace::native_frame_address, stack_trace::MAX_NATIVE_FRAMES>& out_frames, uint32_t& out_count, bool& out_truncated) noexcept -> bool;
static void resolve_posix_frames(stack_trace_state& state, stack_trace::native_frame_address addr, std::vector<stack_trace::frame>& out_frames);
static auto resolve_dynamic_symbol(stack_trace::native_frame_address addr) -> std::string;
static auto demangle_native_name(const char* name) -> std::string;
#endif
#if HAS_NATIVE_TRACE && FO_LINUX
static auto get_backtrace_state(stack_trace_state& state) noexcept -> backtrace_state*;
static auto on_backtrace_frame(void* data, uintptr_t pc, const char* filename, int32_t lineno, const char* function) -> int32_t;
static void on_backtrace_symbol(void* data, uintptr_t pc, const char* symname, uintptr_t symval, uintptr_t symsize);
static void on_backtrace_error(void* data, const char* msg, int32_t errnum);
#endif
static auto try_get_resolved_native_frame_from_cache(stack_trace::native_frame_address addr) -> std::optional<resolved_native_frame_cache_entry>;
static void store_resolved_native_frame_in_cache(stack_trace::native_frame_address addr, const resolved_native_frame_cache_entry& entry) noexcept;
static auto make_native_address_cache_entry(stack_trace::native_frame_address addr) noexcept -> resolved_native_frame_cache_entry;
static auto make_native_address_frame(stack_trace::native_frame_address addr) noexcept -> stack_trace::frame;
static auto make_native_function_key(stack_trace::native_frame_address addr, std::string_view name) noexcept -> uintptr_t;
static auto make_native_address_key(stack_trace::native_frame_address addr) noexcept -> uintptr_t;
static auto is_low_native_address(stack_trace::native_frame_address addr) noexcept -> bool;
static auto is_unresolved_native_name(std::string_view s) noexcept -> bool;
static auto is_constructor_name(std::string_view name) noexcept -> bool;
static void trim_in_place(std::string& s) noexcept;
static auto get_stack_trace_state() noexcept -> stack_trace_state&;

// Kept out of line so that its own frame is the one the capture skips
FO_NO_INLINE auto stack_trace::get() noexcept -> stack_trace::data
{
    FO_NO_STACK_TRACE_ENTRY();

    stack_trace::data st;

    stack_trace::capture_native_frames(st.native_frames, st.native_frame_count, st.native_truncated, 1);
    st.native_head_is_request = true;
    attach_script_layers(st);

    return st;
}

auto stack_trace::get_from_context(const void* os_context, void* os_thread) noexcept -> stack_trace::data
{
    FO_NO_STACK_TRACE_ENTRY();

    stack_trace::data st;

    stack_trace::capture_native_frames_from_context(os_context, os_thread, st.native_frames, st.native_frame_count, st.native_truncated);
    attach_script_layers(st);

    return st;
}

// A script exception that already unwound back into native code has only its recorded frames left; they become the
// innermost layer, entered from the native point where st was captured
void stack_trace::add_unwound_script_frames(stack_trace::data& st, stack_trace::script_layer layer)
{
    FO_NO_STACK_TRACE_ENTRY();

    layer.birth_native_frames = st.native_frames;
    layer.birth_native_frame_count = st.native_frame_count;
    layer.birth_native_truncated = st.native_truncated;

    std::vector<stack_trace::script_layer> layers;
    layers.emplace_back(std::move(layer));

    if (st.script_layers) {
        layers.insert(layers.end(), st.script_layers->begin(), st.script_layers->end());
    }

    st.script_layers = std::make_shared<const std::vector<stack_trace::script_layer>>(std::move(layers));
}

// A script exception caught by script code unwound only down to the catching frame, which is still live in the innermost
// layer, so the recorded frames replace the live frames above it
void stack_trace::splice_caught_script_frames(stack_trace::data& st, stack_trace::script_layer layer)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (layer.script_frames.empty()) {
        return;
    }

    std::vector<stack_trace::script_layer> layers;

    if (st.script_layers) {
        layers.assign(st.script_layers->begin(), st.script_layers->end());
    }

    if (layers.empty()) {
        layers.emplace_back(std::move(layer));
    }
    else {
        std::vector<stack_trace::frame>& live_frames = layers.front().script_frames;
        const std::string& catching_function = layer.script_frames.back().function;
        auto catching_it = std::ranges::find_if(live_frames, [&](const stack_trace::frame& frame) { return frame.function == catching_function; });
        auto below_catch_it = catching_it != live_frames.end() ? std::next(catching_it) : live_frames.begin();

        layer.script_frames.insert(layer.script_frames.end(), below_catch_it, live_frames.end());
        live_frames = std::move(layer.script_frames);
    }

    st.script_layers = std::make_shared<const std::vector<stack_trace::script_layer>>(std::move(layers));
}

auto stack_trace::resolve(const stack_trace::data& st) -> std::vector<stack_trace::frame>
{
    FO_NO_STACK_TRACE_ENTRY();

    std::vector<stack_trace::frame> frames;
    std::span<const stack_trace::native_frame_address> source {st.native_frames.data(), st.native_frame_count};
    bool collapse_head = st.native_head_is_request;

    if (!st.script_layers || st.script_layers->empty()) {
        frames.reserve(source.size());
        resolve_native_range(source, 0, st.native_frame_count, {}, collapse_head, frames);
        drop_requesting_constructors(st, frames);
        return frames;
    }

    const auto& layers = *st.script_layers;

    size_t reserve_count = st.native_frame_count;
    std::vector<stack_trace::native_frame_address> hidden;

    for (const auto& layer : layers) {
        reserve_count += layer.script_frames.size() + layer.birth_native_frame_count;
        hidden.insert(hidden.end(), layer.runtime_native_frames.begin(), layer.runtime_native_frames.end());
    }

    frames.reserve(reserve_count);

    uint32_t pos = 0;

    for (const auto& layer : layers) {
        uint32_t anchor = find_layer_native_anchor(source, layer, pos);

        if (anchor < source.size()) {
            ResolveLayerRegion(source, pos, anchor, hidden, layer, collapse_head, frames);
            pos = anchor;
        }
        else if (layer.birth_native_frame_count != 0) {
            // The trace ends above the frame that entered this layer, as it does when the native unwinder cannot step
            // through script-runtime generated code, so the stack below the layer is read from its own birth capture
            ResolveLayerRegion(source, pos, FindLayerBirthOverlap(source, layer, pos), hidden, layer, collapse_head, frames);
            source = std::span<const stack_trace::native_frame_address> {layer.birth_native_frames.data(), layer.birth_native_frame_count};
            pos = 0;
            collapse_head = false;
        }
        else {
            ResolveLayerRegion(source, pos, pos, hidden, layer, collapse_head, frames);
        }
    }

    resolve_native_range(source, pos, static_cast<uint32_t>(source.size()), hidden, collapse_head, frames);
    drop_requesting_constructors(st, frames);
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

// Kept out of line: the walk starts in this frame, and the frames skipped are counted from its caller
FO_NO_INLINE void stack_trace::capture_native_frames(std::array<stack_trace::native_frame_address, stack_trace::MAX_NATIVE_FRAMES>& out_frames, uint32_t& out_count, bool& out_truncated, uint32_t skip) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    out_count = 0;
    out_truncated = false;

#if HAS_NATIVE_TRACE && FO_WINDOWS && defined(_M_IX86)
    // The same walk a resume point gets, so the two agree; it starts in this frame, which the skip leaves out too
    stack_trace::resume_point here;
    (void)stack_trace::save_resume_point(&here);
    walk_windows_frame_chain(static_cast<uintptr_t>(here.context[0]), static_cast<uintptr_t>(here.context[2]), 1 + skip, out_frames, out_count, out_truncated);

#elif HAS_NATIVE_TRACE && FO_WINDOWS
    // The same walk a resume point gets, so the two agree; it starts in this frame, which the skip leaves out too
    CONTEXT context;
    RtlCaptureContext(&context);
    walk_windows_function_tables(context, 1 + skip, out_frames, out_count, out_truncated);

#elif HAS_NATIVE_TRACE
    unw_context_t context;
    unw_cursor_t cursor;

    if (unw_getcontext(&context) != UNW_ESUCCESS || unw_init_local(&cursor, &context) != UNW_ESUCCESS) {
        return;
    }

    walk_native_cursor(cursor, native_first_frame::omitted, false, skip, out_frames, out_count, out_truncated);

#else
    (void)out_frames;
    (void)skip;
#endif
}

void stack_trace::capture_native_frames_from_context(const void* os_context, void* os_thread, std::array<stack_trace::native_frame_address, stack_trace::MAX_NATIVE_FRAMES>& out_frames, uint32_t& out_count, bool& out_truncated) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    out_count = 0;
    out_truncated = false;

#if HAS_NATIVE_TRACE && FO_WINDOWS
    if (os_context == nullptr) {
        stack_trace::capture_native_frames(out_frames, out_count, out_truncated);
        return;
    }

    walk_windows_context(*static_cast<const CONTEXT*>(os_context), static_cast<HANDLE>(os_thread), out_frames, out_count, out_truncated);

#elif HAS_NATIVE_TRACE
    (void)os_thread;

    if (os_context != nullptr && walk_signal_context(*static_cast<const ucontext_t*>(os_context), out_frames, out_count, out_truncated)) {
        return;
    }

    // Without a register layout to start from, the live stack is walked through the signal trampoline and kept from
    // the frame the signal interrupted; a walk that never meets that frame is kept whole
    unw_context_t context;
    unw_cursor_t cursor;

    if (unw_getcontext(&context) != UNW_ESUCCESS || unw_init_local(&cursor, &context) != UNW_ESUCCESS) {
        return;
    }

    walk_native_cursor(cursor, native_first_frame::omitted, true, 0, out_frames, out_count, out_truncated);

    if (out_count == 0) {
        stack_trace::capture_native_frames(out_frames, out_count, out_truncated);
    }

#else
    (void)os_context;
    (void)os_thread;
    (void)out_frames;
#endif
}

#if FO_STACK_TRACE_RESUME_CONTEXT && FO_WINDOWS && defined(_M_IX86)
// No prologue: the frame pointer is still the caller's and the stack pointer points at its return address; the words
// kept are the return address, the caller's stack pointer after the return and its frame pointer
__declspec(naked) auto stack_trace::save_resume_point(stack_trace::resume_point* point) noexcept -> int
{
    FO_NO_STACK_TRACE_ENTRY();

    // clang-format off
    __asm {
        mov eax, [esp + 4]
        mov ecx, [esp]
        mov [eax], ecx
        mov dword ptr [eax + 4], 0
        lea ecx, [esp + 4]
        mov [eax + 8], ecx
        mov dword ptr [eax + 12], 0
        mov [eax + 16], ebp
        mov dword ptr [eax + 20], 0
        xor eax, eax
        ret
    }
    // clang-format on
}

#elif FO_STACK_TRACE_RESUME_CONTEXT && FO_WINDOWS
// Kept out of line: it captures its own frame and steps back once, to the frame that called it
FO_NO_INLINE auto stack_trace::save_resume_point(stack_trace::resume_point* point) noexcept -> int
{
    FO_NO_STACK_TRACE_ENTRY();

    CONTEXT context;
    RtlCaptureContext(&context);

    if (!unwind_windows_frame(context)) {
        point->context.fill(0);
        return -1;
    }

    store_resume_registers(context, *point);
    return 0;
}

#elif FO_STACK_TRACE_RESUME_CONTEXT
static_assert(sizeof(unw_context_t) <= sizeof(stack_trace::resume_point::context) && alignof(unw_context_t) <= alignof(stack_trace::resume_point), "Resume point must hold an unwind context");
#else
// Kept out of line so that the frame the capture skips is this one and not the opening frame it was inlined into
FO_NO_INLINE auto stack_trace::save_resume_point(stack_trace::resume_point* point) noexcept -> int
{
    FO_NO_STACK_TRACE_ENTRY();

    capture_native_frames(point->frames, point->frame_count, point->truncated, 1);
    return 0;
}
#endif

void stack_trace::resolve_resume_point(const stack_trace::resume_point& point, std::array<stack_trace::native_frame_address, stack_trace::MAX_NATIVE_FRAMES>& out_frames, uint32_t& out_count, bool& out_truncated) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_STACK_TRACE_RESUME_CONTEXT && FO_WINDOWS && defined(_M_IX86)
    out_count = 0;
    out_truncated = false;

    walk_windows_frame_chain(static_cast<uintptr_t>(point.context[0]), static_cast<uintptr_t>(point.context[2]), 0, out_frames, out_count, out_truncated);

#elif FO_STACK_TRACE_RESUME_CONTEXT && FO_WINDOWS
    out_count = 0;
    out_truncated = false;

    CONTEXT context {};
    load_resume_registers(point, context);

    walk_windows_function_tables(context, 0, out_frames, out_count, out_truncated);

#elif FO_STACK_TRACE_RESUME_CONTEXT
    out_count = 0;
    out_truncated = false;

    unw_context_t context;
    std::memcpy(&context, point.context.data(), sizeof(context));
    unw_cursor_t cursor;

    if (unw_init_local(&cursor, &context) != UNW_ESUCCESS) {
        return;
    }

    // The saved instruction pointer is the return address of the call that saved the point
    walk_native_cursor(cursor, native_first_frame::call_site, false, 0, out_frames, out_count, out_truncated);

#else
    std::copy_n(point.frames.begin(), point.frame_count, out_frames.begin());
    out_count = point.frame_count;
    out_truncated = point.truncated;
#endif
}

void stack_trace::set_script_provider(std::string_view name, stack_trace::script_provider provider) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        stack_trace_state& state = get_stack_trace_state();
        std::scoped_lock locker {state.provider_locker};

        auto it = std::ranges::find_if(state.providers, [&](const auto& entry) { return entry.first == name; });

        if (!provider) {
            if (it != state.providers.end()) {
                state.providers.erase(it);
            }
        }
        else if (it != state.providers.end()) {
            it->second = std::move(provider);
        }
        else {
            state.providers.emplace_back(std::string {name}, std::move(provider));
            std::ranges::sort(state.providers, {}, &std::pair<std::string, stack_trace::script_provider>::first);
        }
    }
    catch (...) {
        break_into_debugger();
    }
}

auto stack_trace::has_script_provider(std::string_view name) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    stack_trace_state& state = get_stack_trace_state();
    std::scoped_lock locker {state.provider_locker};

    return std::ranges::any_of(state.providers, [&](const auto& entry) { return entry.first == name; });
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

static void attach_script_layers(stack_trace::data& st) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        std::vector<stack_trace::script_layer> script_layers;
        collect_script_layers(st, script_layers);

        if (!script_layers.empty()) {
            st.script_layers = std::make_shared<const std::vector<stack_trace::script_layer>>(std::move(script_layers));
        }
    }
    catch (...) {
        break_into_debugger();
    }
}

static void collect_script_layers(const stack_trace::data& st, std::vector<stack_trace::script_layer>& out_layers) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        std::vector<stack_trace::script_provider> providers;

        {
            stack_trace_state& state = get_stack_trace_state();
            std::scoped_lock locker {state.provider_locker};

            for (const auto& entry : state.providers) {
                providers.emplace_back(entry.second);
            }
        }

        size_t contributors = 0;

        for (const stack_trace::script_provider& provider : providers) {
            size_t prev_size = out_layers.size();

            try {
                provider(st, out_layers);
            }
            catch (...) {
                break_into_debugger();
            }

            if (out_layers.size() != prev_size) {
                contributors++;
            }
        }

        // Layers of different backends nest through native calls, and a deeper entry captured a longer birth stack
        if (contributors > 1) {
            std::ranges::stable_sort(out_layers, std::ranges::greater {}, &stack_trace::script_layer::birth_native_frame_count);
        }
    }
    catch (...) {
        break_into_debugger();
    }
}

// A trace asked for while an object is under construction, an exception above all, starts at the code that constructs it
static void drop_requesting_constructors(const stack_trace::data& st, std::vector<stack_trace::frame>& frames) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!st.native_head_is_request) {
        return;
    }

    auto first_kept = std::ranges::find_if(frames, [](const stack_trace::frame& frame) { return frame.type != stack_trace::frame::frame_type::native || !is_constructor_name(frame.function); });
    frames.erase(frames.begin(), first_kept);
}

// The native frames above a layer's anchor hold the script-runtime frames of that layer: natives above them were called
// by script, natives below them are the runtime entering it, so the script frames go where the runtime frames are
static void ResolveLayerRegion(std::span<const stack_trace::native_frame_address> source, uint32_t from, uint32_t to, const std::vector<stack_trace::native_frame_address>& hidden, const stack_trace::script_layer& layer, bool collapse_head, std::vector<stack_trace::frame>& out)
{
    FO_NO_STACK_TRACE_ENTRY();

    uint32_t first_runtime = to;
    uint32_t last_runtime = to;

    for (uint32_t i = from; i < to; i++) {
        if (std::ranges::find(hidden, source[i]) == hidden.end()) {
            continue;
        }
        if (first_runtime == to) {
            first_runtime = i;
        }

        last_runtime = i;
    }

    resolve_native_range(source, from, first_runtime, hidden, collapse_head, out);
    out.insert(out.end(), layer.script_frames.begin(), layer.script_frames.end());

    if (last_runtime != to) {
        resolve_native_range(source, last_runtime + 1, to, hidden, collapse_head, out);
    }
}

// A collapsed head keeps the function that owns the first frame and drops the code inlined into it on the way to the
// capture, which for an exception is its own constructor chain
static void resolve_native_range(std::span<const stack_trace::native_frame_address> frames, uint32_t from, uint32_t to, const std::vector<stack_trace::native_frame_address>& hidden, bool collapse_head, std::vector<stack_trace::frame>& out) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (from >= to) {
        return;
    }

    try {
        for (uint32_t i = from; i < to; i++) {
            stack_trace::native_frame_address addr = frames[i];

            if (addr == 0 || std::ranges::find(hidden, addr) != hidden.end()) {
                continue;
            }

            resolved_native_frame_cache_entry entry = resolve_native_frame(addr);

            if (collapse_head && i == 0) {
                out.emplace_back(std::move(entry.frames.back()));
            }
            else {
                out.insert(out.end(), std::make_move_iterator(entry.frames.begin()), std::make_move_iterator(entry.frames.end()));
            }
        }
    }
    catch (...) {
        break_into_debugger();
    }
}

static auto find_layer_native_anchor(std::span<const stack_trace::native_frame_address> trace, const stack_trace::script_layer& layer, uint32_t search_from) noexcept -> uint32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    uint32_t trace_n = static_cast<uint32_t>(trace.size());

    if (layer.birth_native_frame_count == 0) {
        return trace_n;
    }

    uint32_t birth_n = layer.birth_native_frame_count;
    uint32_t matched = 0;

    while (matched < birth_n && matched < trace_n) {
        stack_trace::native_frame_address birth_addr = layer.birth_native_frames[birth_n - 1 - matched];
        stack_trace::native_frame_address trace_addr = trace[trace_n - 1 - matched];

        if (!same_frame_function(birth_addr, trace_addr)) {
            break;
        }

        matched++;
    }

    if (matched == 0) {
        return trace_n;
    }

    uint32_t anchor = trace_n - matched;

    if (anchor < search_from) {
        return trace_n;
    }

    return anchor;
}

// A trace that stops above the layer entry may still reach into the head of the birth stack; that shared part is
// read once, from the birth stack
static auto FindLayerBirthOverlap(std::span<const stack_trace::native_frame_address> trace, const stack_trace::script_layer& layer, uint32_t search_from) noexcept -> uint32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    uint32_t trace_n = static_cast<uint32_t>(trace.size());
    uint32_t birth_n = layer.birth_native_frame_count;

    for (uint32_t start = std::max(search_from, trace_n > birth_n ? trace_n - birth_n : 0u); start < trace_n; start++) {
        bool overlaps = true;

        for (uint32_t i = start; i < trace_n; i++) {
            if (!same_frame_function(trace[i], layer.birth_native_frames[i - start])) {
                overlaps = false;
                break;
            }
        }

        if (overlaps) {
            return start;
        }
    }

    return trace_n;
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
        return resolve_native_frame(addr).function_key;
    }
    catch (...) {
        return make_native_address_key(addr);
    }
}

static auto resolve_native_frame(stack_trace::native_frame_address addr) -> resolved_native_frame_cache_entry
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

    resolved_native_frame_cache_entry entry = resolve_native_frame_uncached(addr);
    store_resolved_native_frame_in_cache(addr, entry);
    return entry;
}

static auto resolve_native_frame_uncached(stack_trace::native_frame_address addr) noexcept -> resolved_native_frame_cache_entry
{
    FO_NO_STACK_TRACE_ENTRY();

#if HAS_NATIVE_TRACE
    if (is_low_native_address(addr)) {
        return make_native_address_cache_entry(addr);
    }

    try {
        resolved_native_frame_cache_entry entry;
        stack_trace_state& state = get_stack_trace_state();

        {
            std::scoped_lock locker {state.native_resolver_locker};

#if FO_WINDOWS
            stack_trace::frame frame;

            if (resolve_windows_symbol(state, addr, frame)) {
                entry.frames.emplace_back(std::move(frame));
            }
#else
            resolve_posix_frames(state, addr, entry.frames);
#endif
        }

        for (stack_trace::frame& frame : entry.frames) {
            frame.type = stack_trace::frame::frame_type::native;
            trim_in_place(frame.function);
            trim_in_place(frame.file);

            if (is_unresolved_native_name(frame.file)) {
                frame.file.clear();
                frame.line = 0;
            }
        }

        if (entry.frames.empty() || is_unresolved_native_name(entry.frames.back().function)) {
            return make_native_address_cache_entry(addr);
        }

        stack_trace::frame own_frame = std::move(entry.frames.back());
        entry.frames.pop_back();

        // Standard library code inlined on the way to a call is plumbing; the function that owns the frame stays
        std::erase_if(entry.frames, [](const stack_trace::frame& frame) { return is_unresolved_native_name(frame.function) || frame.file.find("/include/c++/") != std::string::npos; });
        entry.frames.emplace_back(std::move(own_frame));
        entry.function_key = make_native_function_key(addr, entry.frames.back().function);
        return entry;
    }
    catch (...) {
        break_into_debugger();
        return make_native_address_cache_entry(addr);
    }

#else
    return make_native_address_cache_entry(addr);
#endif
}

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
    entry.frames.emplace_back(make_native_address_frame(addr));
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

// Reads a demangled name as scope::Type::Type, with template arguments and the parameter list after either part
static auto is_constructor_name(std::string_view name) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (name.find("operator") != std::string_view::npos || name.find('{') != std::string_view::npos) {
        return false;
    }

    // Everything outside template arguments, up to the parameter list
    std::string qualified;
    int32_t template_depth = 0;

    for (char c : name) {
        if (c == '<') {
            template_depth++;
        }
        else if (c == '>') {
            template_depth--;
        }
        else if (template_depth == 0) {
            if (c == '(') {
                break;
            }

            qualified.push_back(c);
        }
    }

    size_t last_separator = qualified.rfind("::");

    if (last_separator == std::string::npos || last_separator == 0) {
        return false;
    }

    std::string_view scope {qualified.data(), last_separator};
    std::string_view member = std::string_view {qualified}.substr(last_separator + 2);

    if (size_t scope_separator = scope.rfind("::"); scope_separator != std::string_view::npos) {
        scope = scope.substr(scope_separator + 2);
    }

    return !member.empty() && member == scope;
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

#if HAS_NATIVE_TRACE && FO_WINDOWS
static void walk_windows_context(const CONTEXT& source, HANDLE thread, std::array<stack_trace::native_frame_address, stack_trace::MAX_NATIVE_FRAMES>& out_frames, uint32_t& out_count, bool& out_truncated) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    CONTEXT context;
    std::memcpy(&context, &source, sizeof(context));
    bool resumed_after_call = false;

    // A call through a null function pointer leaves no frame of its own. On x64 and x86 the return address it pushed is
    // still on top of the stack, so popping it the way a return would recovers the caller; ARM64 keeps it in a register
#if defined(_M_X64)
    if (context.Rip == 0 && context.Rsp != 0) {
        std::memcpy(&context.Rip, std::bit_cast<const void*>(static_cast<uintptr_t>(context.Rsp)), sizeof(context.Rip));
        context.Rsp += sizeof(context.Rip);
        resumed_after_call = true;
    }
#elif defined(_M_ARM64)
    if (context.Pc == 0 && context.Lr != 0) {
        context.Pc = context.Lr;
        resumed_after_call = true;
    }
#else
    if (context.Eip == 0 && context.Esp != 0) {
        std::memcpy(&context.Eip, std::bit_cast<const void*>(static_cast<uintptr_t>(context.Esp)), sizeof(context.Eip));
        context.Esp += sizeof(context.Eip);
        resumed_after_call = true;
    }
#endif

    STACKFRAME64 frame {};
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrStack.Mode = AddrModeFlat;
    frame.AddrFrame.Mode = AddrModeFlat;

#if defined(_M_X64)
    constexpr DWORD MACHINE = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset = context.Rip;
    frame.AddrStack.Offset = context.Rsp;
    frame.AddrFrame.Offset = context.Rbp;
#elif defined(_M_ARM64)
    constexpr DWORD MACHINE = IMAGE_FILE_MACHINE_ARM64;
    frame.AddrPC.Offset = context.Pc;
    frame.AddrStack.Offset = context.Sp;
    frame.AddrFrame.Offset = context.Fp;
#else
    constexpr DWORD MACHINE = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC.Offset = context.Eip;
    frame.AddrStack.Offset = context.Esp;
    frame.AddrFrame.Offset = context.Ebp;
#endif

    try {
        stack_trace_state& state = get_stack_trace_state();
        std::scoped_lock locker {state.native_resolver_locker};

        // The walk reads the function tables through the symbol handler
        prepare_dbghelp(state);

        HANDLE process = GetCurrentProcess();
        HANDLE walked_thread = thread != nullptr ? thread : GetCurrentThread();

        while (StackWalk64(MACHINE, process, walked_thread, &frame, &context, nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr) != FALSE) {
            DWORD64 pc = frame.AddrPC.Offset;

            if (pc == 0) {
                break;
            }
            if (out_count == stack_trace::MAX_NATIVE_FRAMES) {
                out_truncated = true;
                break;
            }

            // The faulting instruction is exact, every later frame holds a return address
            bool exact = out_count == 0 && !resumed_after_call;
            out_frames[out_count++] = static_cast<stack_trace::native_frame_address>(exact ? pc : pc - 1);

            if (frame.AddrReturn.Offset == 0) {
                break;
            }
        }
    }
    catch (...) {
        break_into_debugger();
    }
}

static auto resolve_windows_symbol(stack_trace_state& state, stack_trace::native_frame_address addr, stack_trace::frame& out_frame) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    prepare_dbghelp(state);

    constexpr size_t NAME_CAPACITY = 1024;
    alignas(SYMBOL_INFO) std::array<char, sizeof(SYMBOL_INFO) + NAME_CAPACITY> symbol_buffer {};
    SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(symbol_buffer.data());
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = static_cast<ULONG>(NAME_CAPACITY);

    HANDLE process = GetCurrentProcess();
    DWORD64 displacement = 0;

    if (SymFromAddr(process, addr, &displacement, symbol) == FALSE) {
        // A module loaded after the handler took its module list is picked up here; code outside every module
        // (script-runtime output) is not worth a refresh
        HMODULE module = nullptr;
        bool in_module = GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, std::bit_cast<LPCWSTR>(addr), &module) != FALSE;

        if (!in_module || SymRefreshModuleList(process) == FALSE || SymFromAddr(process, addr, &displacement, symbol) == FALSE) {
            return false;
        }
    }

    try {
        out_frame.function.assign(symbol->Name, strnlen(symbol->Name, NAME_CAPACITY));

        IMAGEHLP_LINE64 line {};
        line.SizeOfStruct = sizeof(line);
        DWORD line_displacement = 0;

        if (SymGetLineFromAddr64(process, addr, &line_displacement, &line) != FALSE && line.FileName != nullptr) {
            out_frame.file = line.FileName;
            out_frame.line = static_cast<uint32_t>(line.LineNumber);
        }
    }
    catch (...) {
        break_into_debugger();
        return false;
    }

    return true;
}

static void prepare_dbghelp(stack_trace_state& state) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (state.dbghelp_ready) {
        return;
    }

    state.dbghelp_ready = true;
    (void)SymSetOptions(SymGetOptions() | SYMOPT_UNDNAME | SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_NO_PROMPTS);

    std::wstring search_path;

    try {
        search_path = make_symbol_search_path(&state);
    }
    catch (...) {
        break_into_debugger();
    }

    HANDLE process = GetCurrentProcess();

    // The engine modules of a process share one symbol handler, so a module that finds it initialized only brings
    // the module list up to date
    if (SymInitializeW(process, search_path.empty() ? nullptr : search_path.c_str(), TRUE) == FALSE) {
        (void)SymRefreshModuleList(process);
    }
}

// A PDB ships beside its module, which the default search path, the working directory, misses whenever a process
// starts from elsewhere
static auto make_symbol_search_path(const void* module_anchor) -> std::wstring
{
    FO_NO_STACK_TRACE_ENTRY();

    std::wstring search_path;

    auto append_module_dir = [&search_path](HMODULE module) {
        std::array<wchar_t, 4096> file_name {};
        DWORD length = GetModuleFileNameW(module, file_name.data(), static_cast<DWORD>(file_name.size()));

        if (length == 0 || length >= file_name.size()) {
            return;
        }

        std::wstring_view file {file_name.data(), length};
        size_t separator = file.find_last_of(L"\\/");

        if (separator == std::wstring_view::npos) {
            return;
        }

        search_path.append(file.substr(0, separator));
        search_path.push_back(L';');
    };

    append_module_dir(nullptr);

    HMODULE own_module = nullptr;

    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, static_cast<LPCWSTR>(module_anchor), &own_module) != FALSE) {
        append_module_dir(own_module);
    }

    search_path.push_back(L'.');

    // A search path of our own replaces the default one, which also reads these variables
    constexpr std::array<std::wstring_view, 2> PATH_VARIABLES = {L"_NT_SYMBOL_PATH", L"_NT_ALTERNATE_SYMBOL_PATH"};

    for (std::wstring_view variable : PATH_VARIABLES) {
        std::array<wchar_t, 4096> value {};
        DWORD length = GetEnvironmentVariableW(variable.data(), value.data(), static_cast<DWORD>(value.size()));

        if (length != 0 && length < value.size()) {
            search_path.push_back(L';');
            search_path.append(value.data(), length);
        }
    }

    return search_path;
}

#if defined(_M_IX86)
// Follows the saved frame pointers the way RtlCaptureStackBackTrace does here; the first address is a return address of
// the frame that saved it, as every later one is
static void walk_windows_frame_chain(uintptr_t pc, uintptr_t frame, uint32_t skip, std::array<stack_trace::native_frame_address, stack_trace::MAX_NATIVE_FRAMES>& out_frames, uint32_t& out_count, bool& out_truncated) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    const NT_TIB* thread_block = reinterpret_cast<const NT_TIB*>(NtCurrentTeb());
    auto stack_low = reinterpret_cast<uintptr_t>(thread_block->StackLimit);
    auto stack_high = reinterpret_cast<uintptr_t>(thread_block->StackBase);
    uint32_t to_skip = skip;

    while (pc != 0) {
        if (to_skip != 0) {
            to_skip--;
        }
        else if (out_count == stack_trace::MAX_NATIVE_FRAMES) {
            out_truncated = true;
            break;
        }
        else {
            out_frames[out_count++] = static_cast<stack_trace::native_frame_address>(pc - 1);
        }

        // A frame record is the saved frame pointer of the caller followed by the return address into it, and the chain
        // only moves toward the stack base
        std::array<uintptr_t, 2> record {};

        if (frame < stack_low || frame > stack_high - sizeof(record) || frame % sizeof(uintptr_t) != 0) {
            break;
        }

        std::memcpy(record.data(), std::bit_cast<const void*>(frame), sizeof(record));

        if (record[0] <= frame) {
            break;
        }

        pc = record[1];
        frame = record[0];
    }
}

#else
// Steps the frames through the function tables the way RtlCaptureStackBackTrace does; the first address is a return
// address of the frame the context was taken in, as every later one is
static void walk_windows_function_tables(CONTEXT& context, uint32_t skip, std::array<stack_trace::native_frame_address, stack_trace::MAX_NATIVE_FRAMES>& out_frames, uint32_t& out_count, bool& out_truncated) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    const NT_TIB* thread_block = reinterpret_cast<const NT_TIB*>(NtCurrentTeb());
    auto stack_low = reinterpret_cast<DWORD64>(thread_block->StackLimit);
    auto stack_high = reinterpret_cast<DWORD64>(thread_block->StackBase);
    uint32_t to_skip = skip;

    while (true) {
#if defined(_M_X64)
        DWORD64 pc = context.Rip;
        DWORD64 sp = context.Rsp;
#else
        DWORD64 pc = context.Pc;
        DWORD64 sp = context.Sp;
#endif

        if (pc == 0 || sp < stack_low || sp >= stack_high) {
            break;
        }

        if (to_skip != 0) {
            to_skip--;
        }
        else if (out_count == stack_trace::MAX_NATIVE_FRAMES) {
            out_truncated = true;
            break;
        }
        else {
            out_frames[out_count++] = static_cast<stack_trace::native_frame_address>(pc - 1);
        }

        // Code without unwind data (script-runtime output nobody registered) ends the walk, as it ends the other walkers
        if (!unwind_windows_frame(context)) {
            break;
        }
    }
}

// Moves the context from a frame to its caller through the function table entry of the code it is in
static auto unwind_windows_frame(CONTEXT& context) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

#if defined(_M_X64)
    DWORD64 pc = context.Rip;
#else
    DWORD64 pc = context.Pc;
#endif

    DWORD64 image_base = 0;
    PRUNTIME_FUNCTION function_entry = RtlLookupFunctionEntry(pc, &image_base, nullptr);

    if (function_entry == nullptr) {
        return false;
    }

    PVOID handler_data = nullptr;
    DWORD64 establisher_frame = 0;
    (void)RtlVirtualUnwind(UNW_FLAG_NHANDLER, image_base, pc, function_entry, &context, &handler_data, &establisher_frame, nullptr);
    return true;
}

// Only what the function tables need to go on unwinding: the instruction and stack pointers and the callee-saved registers
static void store_resume_registers(const CONTEXT& context, stack_trace::resume_point& point) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if defined(_M_X64)
    point.context = {context.Rip, context.Rsp, context.Rbp, context.Rbx, context.Rsi, context.Rdi, context.R12, context.R13, context.R14, context.R15};
#else
    point.context = {context.Pc, context.Sp, context.Fp, context.Lr};

    for (size_t i = 0; i < 10; i++) {
        point.context[4 + i] = context.X[19 + i];
    }
#endif
}

static void load_resume_registers(const stack_trace::resume_point& point, CONTEXT& context) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if defined(_M_X64)
    context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    context.Rip = point.context[0];
    context.Rsp = point.context[1];
    context.Rbp = point.context[2];
    context.Rbx = point.context[3];
    context.Rsi = point.context[4];
    context.Rdi = point.context[5];
    context.R12 = point.context[6];
    context.R13 = point.context[7];
    context.R14 = point.context[8];
    context.R15 = point.context[9];
#else
    context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    context.Pc = point.context[0];
    context.Sp = point.context[1];
    context.Fp = point.context[2];
    context.Lr = point.context[3];

    for (size_t i = 0; i < 10; i++) {
        context.X[19 + i] = point.context[4 + i];
    }
#endif
}
#endif

#elif HAS_NATIVE_TRACE
static void walk_native_cursor(unw_cursor_t& cursor, native_first_frame first, bool from_signal_frame, uint32_t skip, std::array<stack_trace::native_frame_address, stack_trace::MAX_NATIVE_FRAMES>& out_frames, uint32_t& out_count, bool& out_truncated) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    uint32_t to_skip = skip;
    bool recording = !from_signal_frame;

    // Answers false once the list is full
    auto record = [&](unw_word_t address) -> bool {
        if (to_skip != 0) {
            to_skip--;
            return true;
        }

        if (out_count == stack_trace::MAX_NATIVE_FRAMES) {
            out_truncated = true;
            return false;
        }

        out_frames[out_count++] = static_cast<stack_trace::native_frame_address>(address);
        return true;
    };

    unw_word_t ip = 0;

    if (unw_get_reg(&cursor, UNW_REG_IP, &ip) != UNW_ESUCCESS || ip == 0) {
        return;
    }

    if (first != native_first_frame::omitted && !record(first == native_first_frame::faulting ? ip : ip - 1)) {
        return;
    }

    while (true) {
        int32_t step = unw_step(&cursor);
        unw_word_t next_ip = 0;

        // A step onto code without unwind info (script-runtime output) ends the walk but still lands on that frame
        if (step < 0 || unw_get_reg(&cursor, UNW_REG_IP, &next_ip) != UNW_ESUCCESS || next_ip == 0 || (step == 0 && next_ip == ip)) {
            break;
        }

        ip = next_ip;

        // A frame a signal interrupted holds the faulting instruction itself, every other frame a return address
        bool interrupted = unw_is_signal_frame(&cursor) > 0;
        recording = recording || interrupted;

        if (recording && !record(interrupted ? ip : ip - 1)) {
            break;
        }
        if (step == 0) {
            break;
        }
    }

    if (!recording) {
        out_count = 0;
    }
}

static auto walk_signal_context(const ucontext_t& uctx, std::array<stack_trace::native_frame_address, stack_trace::MAX_NATIVE_FRAMES>& out_frames, uint32_t& out_count, bool& out_truncated) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_LINUX && (defined(__x86_64__) || defined(__aarch64__))
    unw_context_t context;

    if (unw_getcontext(&context) != UNW_ESUCCESS) {
        return false;
    }

    bool resumed_after_call = false;

#if defined(__x86_64__)
    // Registers_x86_64 keeps rax, rbx, rcx, rdx, rdi, rsi, rbp, rsp, r8-r15 and rip in this order
    constexpr std::array<int32_t, 17> REGISTERS = {REG_RAX, REG_RBX, REG_RCX, REG_RDX, REG_RDI, REG_RSI, REG_RBP, REG_RSP, REG_R8, REG_R9, REG_R10, REG_R11, REG_R12, REG_R13, REG_R14, REG_R15, REG_RIP};
    constexpr size_t SP_INDEX = 7;
    constexpr size_t IP_INDEX = 16;

    for (size_t i = 0; i < REGISTERS.size(); i++) {
        context.data[i] = static_cast<uint64_t>(uctx.uc_mcontext.gregs[REGISTERS[i]]);
    }

    // A page fault on an instruction fetch means a call went where no code is; the return address it pushed is still
    // on top of the stack, so popping it the way a return would recovers the caller
    constexpr greg_t PAGE_FAULT_TRAP = 14;
    constexpr greg_t INSTRUCTION_FETCH_ERROR = 0x10;
    bool fetch_fault = uctx.uc_mcontext.gregs[REG_TRAPNO] == PAGE_FAULT_TRAP && (uctx.uc_mcontext.gregs[REG_ERR] & INSTRUCTION_FETCH_ERROR) != 0;

    if ((context.data[IP_INDEX] == 0 || fetch_fault) && context.data[SP_INDEX] != 0) {
        std::memcpy(&context.data[IP_INDEX], std::bit_cast<const void*>(static_cast<uintptr_t>(context.data[SP_INDEX])), sizeof(uint64_t));
        context.data[SP_INDEX] += sizeof(uint64_t);
        resumed_after_call = true;
    }

#else
    // Registers_arm64 keeps x0-x28, fp, lr, sp and pc in this order
    constexpr size_t LR_INDEX = 30;
    constexpr size_t SP_INDEX = 31;
    constexpr size_t PC_INDEX = 32;

    for (size_t i = 0; i <= LR_INDEX; i++) {
        context.data[i] = uctx.uc_mcontext.regs[i];
    }

    context.data[SP_INDEX] = uctx.uc_mcontext.sp;
    context.data[PC_INDEX] = uctx.uc_mcontext.pc;

    // A call through a null function pointer left the return address in the link register
    if (context.data[PC_INDEX] == 0) {
        context.data[PC_INDEX] = context.data[LR_INDEX];
        resumed_after_call = true;
    }
#endif

    unw_cursor_t cursor;

    if (unw_init_local(&cursor, &context) != UNW_ESUCCESS) {
        return false;
    }

    walk_native_cursor(cursor, resumed_after_call ? native_first_frame::call_site : native_first_frame::faulting, false, 0, out_frames, out_count, out_truncated);
    return out_count != 0;

#else
    (void)uctx;
    (void)out_frames;
    (void)out_count;
    (void)out_truncated;
    return false;
#endif
}

static void resolve_posix_frames(stack_trace_state& state, stack_trace::native_frame_address addr, std::vector<stack_trace::frame>& out_frames)
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_LINUX
    if (backtrace_state* backtrace = get_backtrace_state(state); backtrace != nullptr) {
        (void)backtrace_pcinfo(backtrace, addr, &on_backtrace_frame, &on_backtrace_error, &out_frames);

        // Code without debug info still has its symbol table entry
        if (out_frames.empty() || out_frames.back().function.empty()) {
            std::string symbol;
            (void)backtrace_syminfo(backtrace, addr, &on_backtrace_symbol, &on_backtrace_error, &symbol);

            if (!symbol.empty()) {
                if (out_frames.empty()) {
                    out_frames.emplace_back();
                }

                out_frames.back().function = std::move(symbol);
            }
        }
    }
#else
    (void)state;
#endif

    // The dynamic linker knows every loaded module, those loaded after the debug info was read included
    if (out_frames.empty() || out_frames.back().function.empty()) {
        std::string symbol = resolve_dynamic_symbol(addr);

        if (!symbol.empty()) {
            if (out_frames.empty()) {
                out_frames.emplace_back();
            }

            out_frames.back().function = std::move(symbol);
        }
    }
}

static auto resolve_dynamic_symbol(stack_trace::native_frame_address addr) -> std::string
{
    FO_NO_STACK_TRACE_ENTRY();

    Dl_info info {};

    if (dladdr(std::bit_cast<const void*>(addr), &info) == 0) {
        return {};
    }

    if (info.dli_sname != nullptr) {
        return demangle_native_name(info.dli_sname);
    }

    // Without a symbol the module and the offset into it still locate the code for an offline lookup
    if (info.dli_fname == nullptr || info.dli_fbase == nullptr) {
        return {};
    }

    std::string_view module_name {info.dli_fname};

    if (size_t separator = module_name.find_last_of('/'); separator != std::string_view::npos) {
        module_name = module_name.substr(separator + 1);
    }

    std::array<char, 32> offset {};
    (void)std::snprintf(offset.data(), offset.size(), "+0x%zx", static_cast<size_t>(addr - std::bit_cast<uintptr_t>(info.dli_fbase)));
    return std::string {module_name}.append(offset.data());
}

static auto demangle_native_name(const char* name) -> std::string
{
    FO_NO_STACK_TRACE_ENTRY();

    if (name == nullptr) {
        return {};
    }

    int32_t status = 0;
    char* demangled = abi::__cxa_demangle(name, nullptr, nullptr, &status);

    if (demangled == nullptr) {
        return name;
    }

    std::string result {demangled};

    // The C++ runtime hands the demangled name over in malloc memory
    std::free(demangled);
    return result;
}
#endif

#if HAS_NATIVE_TRACE && FO_LINUX
// Created on first use rather than at startup: reading the debug info of every module costs time and memory a process
// that never resolves a frame should not pay
static auto get_backtrace_state(stack_trace_state& state) noexcept -> backtrace_state*
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!state.backtrace_created) {
        state.backtrace_created = true;
        state.backtrace = backtrace_create_state(nullptr, 1, &on_backtrace_error, nullptr);
    }

    return state.backtrace;
}

// Called for the innermost inlined call first and for the function that owns the machine frame last
static auto on_backtrace_frame(void* data, uintptr_t pc, const char* filename, int32_t lineno, const char* function) -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    (void)pc;

    if (filename == nullptr && function == nullptr) {
        return 0;
    }

    try {
        stack_trace::frame frame;
        frame.function = demangle_native_name(function);
        frame.file = filename != nullptr ? filename : "";
        frame.line = lineno > 0 ? static_cast<uint32_t>(lineno) : 0;
        static_cast<std::vector<stack_trace::frame>*>(data)->emplace_back(std::move(frame));
        return 0;
    }
    catch (...) {
        break_into_debugger();
        return 1;
    }
}

static void on_backtrace_symbol(void* data, uintptr_t pc, const char* symname, uintptr_t symval, uintptr_t symsize)
{
    FO_NO_STACK_TRACE_ENTRY();

    (void)pc;
    (void)symval;
    (void)symsize;

    try {
        *static_cast<std::string*>(data) = demangle_native_name(symname);
    }
    catch (...) {
        break_into_debugger();
    }
}

// Missing debug info is an ordinary answer here, the fallbacks after libbacktrace cover it
static void on_backtrace_error(void* data, const char* msg, int32_t errnum)
{
    FO_NO_STACK_TRACE_ENTRY();

    (void)data;
    (void)msg;
    (void)errnum;
}
#endif

FO_END_NAMESPACE
