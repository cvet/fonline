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
#include "Posix.h"
#include "StringUtils.h"
#include "WinApi.h"

// The sanitizer runtimes own crash reporting where they watch memory or threads
#if (FO_WINDOWS || FO_LINUX || FO_MAC) && !FO_MEMORY_SANITIZER && !FO_THREAD_SANITIZER
#define HAS_CRASH_HANDLERS 1
#else
#define HAS_CRASH_HANDLERS 0
#endif

FO_BEGIN_NAMESPACE

static void install_crash_handlers() noexcept;
#if HAS_CRASH_HANDLERS && FO_WINDOWS
static void on_crash_exception(uint32_t code, uint32_t flags, nptr<const void> address, nptr<const void> context) noexcept;
static void on_crash_report(nptr<const void> context, nptr<void> thread) noexcept;
static void on_crash_signal(int32_t signum) noexcept;
static void on_crash_runtime_error(string_view reason) noexcept;
#elif HAS_CRASH_HANDLERS
static void on_crash_signal(int32_t signum, int32_t code, nptr<const void> address, nptr<const void> context) noexcept;
#endif
#if HAS_CRASH_HANDLERS
static void on_terminate();
static auto claim_crash_report() noexcept -> bool;
#endif
static auto make_error_stack_trace(const std::exception& ex) noexcept -> stack_trace::catched_data;
static void set_crash_info(string info) noexcept;
static auto safe_write_crash_info() noexcept -> bool;
static auto format_seh_crash_info(uint32_t code, uint32_t flags, nptr<const void> address) -> string;
static auto format_signal_crash_info(int32_t signum, int32_t code, nptr<const void> address) -> string;
static auto format_runtime_crash_info(string_view reason) -> string;
static auto get_seh_exception_name(uint32_t code) noexcept -> string_view;
static auto get_signal_name(int32_t signum) noexcept -> string_view;

struct except_handling_data
{
    except_handling_data()
    {
        if (!is_run_in_debugger()) {
            install_crash_handlers();
        }
    }

    std::mutex callback_locker {};
    exceptions::callback callback {};
};
FO_GLOBAL_DATA(except_handling_data, exception_handling);

// A crash handler fires at any moment of the process, the client host running on after its runtime tore the global
// data down included, so what it records lives as long as the handlers do
static optional<string> crash_info;
#if HAS_CRASH_HANDLERS
static std::atomic<bool> crash_handlers_installed {};
static std::atomic<bool> crash_report_claimed {};
#endif
#if HAS_CRASH_HANDLERS && FO_WINDOWS
struct crash_exception_record
{
    uint32_t code {};
    uint32_t flags {};
    nptr<const void> address {};
};

static optional<crash_exception_record> crash_exception;
static optional<stack_trace::data> crash_stack_trace;
#endif

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

    // Exceptions are still reported after the global data is torn down, by a thread the set did not own or
    // by the host the runtime returned to. They go to the base log, which keeps working without the set
    if (!exception_handling.is_created()) {
        return {};
    }

    std::scoped_lock locker {exception_handling->callback_locker};

    return exception_handling->callback;
}

void exceptions::install_crash_handler_stack() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if HAS_CRASH_HANDLERS && !FO_WINDOWS
    if (is_run_in_debugger()) {
        return; // the crash handlers are not installed under a debugger
    }

    posix::install_crash_signal_stack();
#endif
}

void exceptions::set_crash_signal_reason(int32_t signum, int32_t code, nptr<const void> address) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        set_crash_info(format_signal_crash_info(signum, code, address));
    }
    catch (...) {
        // Best effort: the report still goes out, with its reason marked unavailable
    }
}

void exceptions::set_crash_exception_reason(uint32_t code, uint32_t flags, nptr<const void> address) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        set_crash_info(format_seh_crash_info(code, flags, address));
    }
    catch (...) {
        // Best effort: the report still goes out, with its reason marked unavailable
    }
}

void exceptions::set_crash_termination_reason(string_view reason) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        set_crash_info(format_runtime_crash_info(reason));
    }
    catch (...) {
        // Best effort: the report still goes out, with its reason marked unavailable
    }
}

void exceptions::write_crash_report(const stack_trace::data& st) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    logging::suspend_async_writing();

    logging::write_base("\nFATAL ERROR!\n");

    if (!safe_write_crash_info()) {
        logging::write_base("Crash reason: unavailable\n");
    }

    logging::write_base("\n");
    logging::safe_write_stack_trace(st);
}

// Once per module: installing again later would take the signals back from a script runtime that chains to these
// handlers, and a script runtime relies on catching its own faults first
static void install_crash_handlers() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if HAS_CRASH_HANDLERS
    if (crash_handlers_installed.exchange(true)) {
        return;
    }

    std::set_terminate(&on_terminate);

#if FO_WINDOWS
    winapi::crash_handlers handlers;
    handlers.on_exception = &on_crash_exception;
    handlers.on_report = &on_crash_report;
    handlers.on_signal = &on_crash_signal;
    handlers.on_runtime_error = &on_crash_runtime_error;
    winapi::install_crash_handlers(handlers);
#else
    posix::install_crash_signal_handlers(&on_crash_signal);
    exceptions::install_crash_handler_stack();
#endif
#endif
}

#if HAS_CRASH_HANDLERS && FO_WINDOWS
static void on_crash_exception(uint32_t code, uint32_t flags, nptr<const void> address, nptr<const void> context) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!claim_crash_report()) {
        return;
    }

    // Formatted by the report handler: after a stack overflow this thread has no room left to build the text
    crash_exception = crash_exception_record {code, flags, address};

    // Walked here while there is stack for it, so the script frames of this thread reach the report; a stack overflow
    // leaves no room, and the reporter thread walks the saved context instead
    constexpr uint32_t STACK_OVERFLOW_CODE = 0xC00000FDU;

    if (code != STACK_OVERFLOW_CODE) {
        try {
            crash_stack_trace = stack_trace::get_from_context(context.get(), nullptr);
        }
        catch (...) {
            // Best effort: the reporter thread walks the saved context instead
        }
    }
}

static void on_crash_report(nptr<const void> context, nptr<void> thread) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!crash_exception.has_value()) {
        return;
    }

    exceptions::set_crash_exception_reason(crash_exception->code, crash_exception->flags, crash_exception->address);

    try {
        if (!crash_stack_trace.has_value()) {
            crash_stack_trace = stack_trace::get_from_context(context.get(), thread.get());
        }

        exceptions::write_crash_report(*crash_stack_trace);
    }
    catch (...) {
        // Best effort: the process ends either way
    }
}

static void on_crash_signal(int32_t signum) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!claim_crash_report()) {
        return;
    }

    exceptions::set_crash_signal_reason(signum, 0, nullptr);
    exceptions::write_crash_report(stack_trace::get());
}

static void on_crash_runtime_error(string_view reason) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!claim_crash_report()) {
        return;
    }

    exceptions::set_crash_termination_reason(reason);
    exceptions::write_crash_report(stack_trace::get());
}

#elif HAS_CRASH_HANDLERS
static void on_crash_signal(int32_t signum, int32_t code, nptr<const void> address, nptr<const void> context) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!claim_crash_report()) {
        return;
    }

    exceptions::set_crash_signal_reason(signum, code, address);
    exceptions::write_crash_report(stack_trace::get_from_context(context.get(), nullptr));
}
#endif

#if HAS_CRASH_HANDLERS

// An exception escaping a noexcept function or a thread, or a rethrow with nothing to catch it, lands here; the
// default handler only writes to stderr, which a headless server discards
static void on_terminate()
{
    FO_NO_STACK_TRACE_ENTRY();

    if (claim_crash_report()) {
        exceptions::set_crash_termination_reason("std::terminate");
        exceptions::write_crash_report(stack_trace::get());
    }

    // The report is out already; ending here keeps abort from raising SIGABRT and reporting the crash a second time
    std::_Exit(EXIT_FAILURE);
}

// A crash inside the report, or on another thread while it is being written, ends the process without a second one
static auto claim_crash_report() noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return !crash_report_claimed.exchange(true);
}
#endif

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

static void set_crash_info(string info) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        crash_info = std::move(info);
    }
    catch (...) {
        // Best effort: crash handlers must not throw while recording context
    }
}

static auto safe_write_crash_info() noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        if (crash_info.has_value()) {
            logging::write_base(strex("Crash reason: {}\n", *crash_info));
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

static auto format_runtime_crash_info(string_view reason) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    string info = strex("Runtime termination: {}", !reason.empty() ? reason : string_view {"unknown"}).str();
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
