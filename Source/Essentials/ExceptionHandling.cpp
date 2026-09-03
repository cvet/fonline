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

#include "ExceptionHandling.h"
#include "BaseLogging.h"
#include "GlobalData.h"
#include "StringUtils.h"

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

struct except_handling_data
{
    except_handling_data()
    {
#if HAS_NATIVE_TRACE
        if (!is_run_in_debugger()) {
            [[maybe_unused]] static backward::SignalHandling sh;
            assert(sh.loaded());
        }
#endif
    }

    std::mutex callback_locker {};
    exceptions::callback callback {};
    optional<stack_trace::data> crash_stack_trace {};
    optional<string> crash_info {};
};
FO_GLOBAL_DATA(except_handling_data, exception_handling);

class backward_o_stream_buffer : public std::streambuf
{
public:
    backward_o_stream_buffer() = default;
    backward_o_stream_buffer(const backward_o_stream_buffer&) = delete;
    backward_o_stream_buffer(backward_o_stream_buffer&&) noexcept = delete;
    auto operator=(const backward_o_stream_buffer&) = delete;
    auto operator=(backward_o_stream_buffer&&) noexcept -> backward_o_stream_buffer& = delete;
    ~backward_o_stream_buffer() override = default;

    auto underflow() -> int_type override;
    auto overflow(int_type ch) -> int_type override;
    auto xsputn(const char_type* s, std::streamsize count) -> std::streamsize override /*noexcept*/;

private:
    void write_header() const noexcept;

    bool _first_call = true;
};

static auto make_error_stack_trace(const std::exception& ex) noexcept -> stack_trace::catched_data;
static void set_crash_info(string info) noexcept;
static auto safe_write_crash_info() noexcept -> bool;
static auto format_seh_crash_info(uint32_t code, uint32_t flags, nptr<const void> address) -> string;
static auto format_signal_crash_info(int32_t signum, int32_t code, nptr<const void> address) -> string;
static auto format_runtime_crash_info(nptr<const char> reason) -> string;
static auto get_seh_exception_name(uint32_t code) noexcept -> string_view;
static auto get_signal_name(int32_t signum) noexcept -> string_view;

static backward_o_stream_buffer crash_stream_buf;
static auto crash_stream = std::ostream(&crash_stream_buf); // Passed to Printer::print in backward.hpp

FO_END_NAMESPACE
void SetCrashStackTrace() noexcept // Called in backward.hpp
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        FO_NAMESPACE exception_handling->crash_stack_trace = FO_NAMESPACE stack_trace::get();
    }
    catch (...) {
        // Best effort: keep the original fatal error alive even if stack capture fails
    }
}

void SetCrashSignalInfo(int32_t signum, int32_t code, const void* address) noexcept // Called in backward.hpp
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        FO_NAMESPACE set_crash_info(FO_NAMESPACE format_signal_crash_info(signum, code, address));
    }
    catch (...) {
    }
}

void SetCrashSehInfo(uint32_t code, uint32_t flags, const void* address) noexcept // Called in backward.hpp
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        FO_NAMESPACE set_crash_info(FO_NAMESPACE format_seh_crash_info(code, flags, address));
    }
    catch (...) {
    }
}

void SetCrashTerminationInfo(const char* reason) noexcept // Called in backward.hpp
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        FO_NAMESPACE set_crash_info(FO_NAMESPACE format_runtime_crash_info(reason));
    }
    catch (...) {
    }
}

auto GetCrashStream() noexcept -> std::ostream& // Passed to Printer::print in backward.hpp
{
    FO_NO_STACK_TRACE_ENTRY();

    return FO_NAMESPACE crash_stream;
}
FO_BEGIN_NAMESPACE

void exceptions::report_and_exit(const std::exception& ex) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        auto st = make_error_stack_trace(ex);

        if (auto callback = exceptions::get_callback()) {
            callback(ex.what(), st, true);
        }
        else {
            logging::write_base(strex("{}\n", ex.what()), &st);
            logging::write_base("Shutdown!\n\n");
        }
    }
    catch (...) {
    }

    break_into_debugger();
    exit_app(false);
}

void exceptions::report_and_continue(const std::exception& ex) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        auto st = make_error_stack_trace(ex);

        if (auto callback = exceptions::get_callback()) {
            callback(ex.what(), st, false);
        }
        else {
            logging::write_base(strex("{}\n", ex.what()), &st);
            logging::write_base("\n\n");
        }
    }
    catch (...) {
    }

    break_into_debugger();
}

void exceptions::set_callback(exceptions::callback callback) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    std::scoped_lock locker {exception_handling->callback_locker};

    exception_handling->callback = std::move(callback);
}

auto exceptions::get_callback() noexcept -> exceptions::callback
{
    FO_NO_STACK_TRACE_ENTRY();

    std::scoped_lock locker {exception_handling->callback_locker};

    return exception_handling->callback;
}

#if HAS_NATIVE_TRACE && !FO_WINDOWS

// Retires the sigaltstack registration before its pages are released: freeing first leaves the kernel aiming the
// signal stack at reclaimed memory, and an instrumented allocator then unmaps the same region twice
class alt_signal_stack_releaser final
{
public:
    void operator()(uint8_t* buffer) const noexcept
    {
        stack_t ss {};
        ss.ss_flags = SS_DISABLE;
        (void)sigaltstack(&ss, nullptr);

        delete[] buffer;
    }
};

#endif

void exceptions::install_crash_handler_stack() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if HAS_NATIVE_TRACE && !FO_WINDOWS
    if (is_run_in_debugger()) {
        return; // backward-cpp does not install its signal handlers under a debugger
    }

    // 2 MiB is well above what the crash handler (unwinding + symbol resolution) needs; the pages are
    // touched only during a crash, so the reservation stays lazily committed for a thread that never faults
    constexpr size_t stack_size = size_t {2} * 1024 * 1024;

    // The kernel drops the sigaltstack registration only when the thread ends — later than this destructor — so
    // the releaser retires it before the pages go back; std::unique_ptr, not the engine alias, supports array storage
    static thread_local std::unique_ptr<uint8_t[], alt_signal_stack_releaser> alt_stack_buffer;

    if (alt_stack_buffer) {
        return; // already installed on this thread
    }

    alt_stack_buffer = std::unique_ptr<uint8_t[], alt_signal_stack_releaser> {new (std::nothrow) uint8_t[stack_size]};

    if (!alt_stack_buffer) {
        return;
    }

    stack_t ss {};
    ss.ss_sp = alt_stack_buffer.get();
    ss.ss_size = stack_size;
    ss.ss_flags = 0;

    if (sigaltstack(&ss, nullptr) != 0) {
        alt_stack_buffer.reset();
    }
#endif
}

static auto make_error_stack_trace(const std::exception& ex) noexcept -> stack_trace::catched_data
{
    FO_NO_STACK_TRACE_ENTRY();

    auto ex_ptr = make_nptr(&ex);

    if (auto base_engine_ex = ex_ptr.dyn_cast<const BaseEngineException>()) {
        return stack_trace::catched_data {base_engine_ex->stack_trace(), stack_trace::get()};
    }
    else {
        return stack_trace::catched_data {std::nullopt, stack_trace::get()};
    }
}

auto backward_o_stream_buffer::underflow() -> int_type
{
    FO_NO_STACK_TRACE_ENTRY();

    return traits_type::eof();
}

auto backward_o_stream_buffer::overflow(int_type ch) -> int_type
{
    FO_NO_STACK_TRACE_ENTRY();

    const char s[] = {static_cast<char>(ch)};
    logging::write_base(string_view {s, 1});
    return ch;
}

auto backward_o_stream_buffer::xsputn(const char_type* s, std::streamsize count) -> std::streamsize
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_first_call) {
        write_header();
        _first_call = false;
    }

    logging::write_base(string_view {s, static_cast<string_view::size_type>(count)});
    return count;
}

void backward_o_stream_buffer::write_header() const noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    logging::suspend_async_writing();

    logging::write_base("\nFATAL ERROR!\n");

    if (!safe_write_crash_info()) {
        logging::write_base("Crash reason: unavailable\n");
    }

    logging::write_base("\n");

    if (exception_handling->crash_stack_trace.has_value()) {
        logging::safe_write_stack_trace(*exception_handling->crash_stack_trace);
    }
}

static void set_crash_info(string info) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        exception_handling->crash_info = std::move(info);
    }
    catch (...) {
        // Best effort: crash handlers must not throw while recording context
    }
}

static auto safe_write_crash_info() noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        if (exception_handling->crash_info.has_value()) {
            logging::write_base(strex("Crash reason: {}\n", *exception_handling->crash_info));
            return true;
        }
    }
    catch (...) {
    }

    return false;
}

static auto format_seh_crash_info(uint32_t code, uint32_t flags, nptr<const void> address) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    return strex("SEH exception code: 0x{:08X} ({}) flags: 0x{:08X} address: {}", code, get_seh_exception_name(code), flags, address.get()).str();
}

static auto format_signal_crash_info(int32_t signum, int32_t code, nptr<const void> address) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    return strex("Signal {} ({}) code: {} address: {}", signum, get_signal_name(signum), code, address.get()).str();
}

static auto format_runtime_crash_info(nptr<const char> reason) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    string info = strex("Runtime termination: {}", reason ? string_view {reason.get()} : string_view {"unknown"}).str();
    std::exception_ptr current_exception = std::current_exception();

    if (current_exception) {
        try {
            std::rethrow_exception(current_exception);
        }
        catch (const std::exception& ex) {
            info += strex(": current exception: {} ({})", typeid(ex).name(), ex.what()).str();
        }
        catch (...) {
            info += ": current exception: non-std";
        }
    }

    return info;
}

static auto get_seh_exception_name(uint32_t code) noexcept -> string_view
{
    FO_NO_STACK_TRACE_ENTRY();

    switch (code) {
    case 0x40010005U:
        return "DBG_CONTROL_C";
    case 0x80000003U:
        return "EXCEPTION_BREAKPOINT";
    case 0x80000004U:
        return "EXCEPTION_SINGLE_STEP";
    case 0xC0000005U:
        return "EXCEPTION_ACCESS_VIOLATION";
    case 0xC0000006U:
        return "EXCEPTION_IN_PAGE_ERROR";
    case 0xC0000008U:
        return "EXCEPTION_INVALID_HANDLE";
    case 0xC000001DU:
        return "EXCEPTION_ILLEGAL_INSTRUCTION";
    case 0xC0000025U:
        return "EXCEPTION_NONCONTINUABLE_EXCEPTION";
    case 0xC000008CU:
        return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
    case 0xC000008DU:
        return "EXCEPTION_FLT_DENORMAL_OPERAND";
    case 0xC000008EU:
        return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
    case 0xC000008FU:
        return "EXCEPTION_FLT_INEXACT_RESULT";
    case 0xC0000090U:
        return "EXCEPTION_FLT_INVALID_OPERATION";
    case 0xC0000091U:
        return "EXCEPTION_FLT_OVERFLOW";
    case 0xC0000092U:
        return "EXCEPTION_FLT_STACK_CHECK";
    case 0xC0000093U:
        return "EXCEPTION_FLT_UNDERFLOW";
    case 0xC0000094U:
        return "EXCEPTION_INT_DIVIDE_BY_ZERO";
    case 0xC0000095U:
        return "EXCEPTION_INT_OVERFLOW";
    case 0xC0000096U:
        return "EXCEPTION_PRIV_INSTRUCTION";
    case 0xC00000FDU:
        return "EXCEPTION_STACK_OVERFLOW";
    case 0xE0434352U:
        return "CLR_EXCEPTION";
    case 0xE06D7363U:
        return "MSVC_CPP_EXCEPTION";
    default:
        return "UNKNOWN_SEH_EXCEPTION";
    }
}

static auto get_signal_name(int32_t signum) noexcept -> string_view
{
    FO_NO_STACK_TRACE_ENTRY();

    switch (signum) {
#ifdef SIGABRT
    case SIGABRT:
        return "SIGABRT";
#endif

#ifdef SIGBUS
    case SIGBUS:
        return "SIGBUS";
#endif

#ifdef SIGFPE
    case SIGFPE:
        return "SIGFPE";
#endif

#ifdef SIGILL
    case SIGILL:
        return "SIGILL";
#endif

#ifdef SIGQUIT
    case SIGQUIT:
        return "SIGQUIT";
#endif

#ifdef SIGSEGV
    case SIGSEGV:
        return "SIGSEGV";
#endif

#ifdef SIGSYS
    case SIGSYS:
        return "SIGSYS";
#endif

#ifdef SIGTERM
    case SIGTERM:
        return "SIGTERM";
#endif

#ifdef SIGTRAP
    case SIGTRAP:
        return "SIGTRAP";
#endif

#ifdef SIGXCPU
    case SIGXCPU:
        return "SIGXCPU";
#endif

#ifdef SIGXFSZ
    case SIGXFSZ:
        return "SIGXFSZ";
#endif
    default:
        return "UNKNOWN_SIGNAL";
    }
}

FO_END_NAMESPACE
