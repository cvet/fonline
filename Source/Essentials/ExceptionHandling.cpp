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

#include "ExceptionHandling.h"
#include "BaseLogging.h"
#include "GlobalData.h"
#include "StringUtils.h"

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

struct ExceptHandlingData
{
    ExceptHandlingData()
    {
#if HAS_NATIVE_TRACE
        if (!IsRunInDebugger()) {
            [[maybe_unused]] static backward::SignalHandling sh;
            assert(sh.loaded());
        }
#endif
    }

    std::mutex CallbackLocker {};
    ExceptionCallback Callback {};
    optional<StackTraceData> CrashStackTrace {};
    optional<string> CrashInfo {};
};
FO_GLOBAL_DATA(ExceptHandlingData, ExceptionHandling);

class BackwardOStreamBuffer : public std::streambuf
{
public:
    BackwardOStreamBuffer() = default;
    BackwardOStreamBuffer(const BackwardOStreamBuffer&) = delete;
    BackwardOStreamBuffer(BackwardOStreamBuffer&&) noexcept = delete;
    auto operator=(const BackwardOStreamBuffer&) = delete;
    auto operator=(BackwardOStreamBuffer&&) noexcept -> BackwardOStreamBuffer& = delete;
    ~BackwardOStreamBuffer() override = default;

    auto underflow() -> int_type override;
    auto overflow(int_type ch) -> int_type override;
    auto xsputn(const char_type* s, std::streamsize count) -> std::streamsize override /*noexcept*/;

private:
    void WriteHeader() const noexcept;

    bool _firstCall = true;
};

static auto MakeErrorStackTrace(const std::exception& ex) noexcept -> CatchedStackTraceData;
static void SetCrashInfo(string info) noexcept;
static auto SafeWriteCrashInfo() noexcept -> bool;
static auto FormatSehCrashInfo(uint32_t code, uint32_t flags, const void* address) -> string;
static auto FormatSignalCrashInfo(int32_t signum, int32_t code, const void* address) -> string;
static auto FormatRuntimeCrashInfo(const char* reason) -> string;
static auto GetSehExceptionName(uint32_t code) noexcept -> string_view;
static auto GetSignalName(int32_t signum) noexcept -> string_view;

static BackwardOStreamBuffer CrashStreamBuf;
static auto CrashStream = std::ostream(&CrashStreamBuf); // Passed to Printer::print in backward.hpp

FO_END_NAMESPACE
extern void SetCrashStackTrace() noexcept // Called in backward.hpp
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        FO_NAMESPACE ExceptionHandling->CrashStackTrace = FO_NAMESPACE GetStackTrace();
    }
    catch (...) {
        // Best effort: keep the original fatal error alive even if stack capture fails.
    }
}

extern void SetCrashSignalInfo(int32_t signum, int32_t code, const void* address) noexcept // Called in backward.hpp
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        FO_NAMESPACE SetCrashInfo(FO_NAMESPACE FormatSignalCrashInfo(signum, code, address));
    }
    catch (...) {
    }
}

extern void SetCrashSehInfo(uint32_t code, uint32_t flags, const void* address) noexcept // Called in backward.hpp
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        FO_NAMESPACE SetCrashInfo(FO_NAMESPACE FormatSehCrashInfo(code, flags, address));
    }
    catch (...) {
    }
}

extern void SetCrashTerminationInfo(const char* reason) noexcept // Called in backward.hpp
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        FO_NAMESPACE SetCrashInfo(FO_NAMESPACE FormatRuntimeCrashInfo(reason));
    }
    catch (...) {
    }
}

extern auto GetCrashStream() noexcept -> std::ostream& // Passed to Printer::print in backward.hpp
{
    FO_NO_STACK_TRACE_ENTRY();

    return FO_NAMESPACE CrashStream;
}
FO_BEGIN_NAMESPACE

extern void ReportExceptionAndExit(const std::exception& ex) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        const auto st = MakeErrorStackTrace(ex);

        if (const auto callback = GetExceptionCallback()) {
            callback(ex.what(), st, true);
        }
        else {
            WriteBaseLog(strex("{}\n", ex.what()), &st);
            WriteBaseLog("Shutdown!\n\n");
        }
    }
    catch (...) {
    }

    BreakIntoDebugger();
    ExitApp(false);
}

extern void ReportExceptionAndContinue(const std::exception& ex) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        const auto st = MakeErrorStackTrace(ex);

        if (const auto callback = GetExceptionCallback()) {
            callback(ex.what(), st, false);
        }
        else {
            WriteBaseLog(strex("{}\n", ex.what()), &st);
            WriteBaseLog("\n\n");
        }
    }
    catch (...) {
    }

    BreakIntoDebugger();
}

extern void SetExceptionCallback(ExceptionCallback callback) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    std::scoped_lock locker(ExceptionHandling->CallbackLocker);

    ExceptionHandling->Callback = std::move(callback);
}

extern auto GetExceptionCallback() noexcept -> ExceptionCallback
{
    FO_NO_STACK_TRACE_ENTRY();

    std::scoped_lock locker(ExceptionHandling->CallbackLocker);

    return ExceptionHandling->Callback;
}

extern void ReportStrongAssertAndExit(string_view message, const char* file, int32_t line) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        throw StrongAssertationException(message, file, line);
    }
    catch (const StrongAssertationException& ex) {
        ReportExceptionAndExit(ex);
    }
}

extern void ReportVerifyFailed(string_view message, const char* file, int32_t line) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        throw VerifyFailedException(message, file, line);
    }
    catch (const VerifyFailedException& ex) {
        ReportExceptionAndContinue(ex);
    }
}

static auto MakeErrorStackTrace(const std::exception& ex) noexcept -> CatchedStackTraceData
{
    FO_NO_STACK_TRACE_ENTRY();

    if (const auto* base_engine_ex = dynamic_cast<const BaseEngineException*>(&ex); base_engine_ex != nullptr) {
        return CatchedStackTraceData {base_engine_ex->stack_trace(), GetStackTrace()};
    }
    else {
        return CatchedStackTraceData {std::nullopt, GetStackTrace()};
    }
}

auto BackwardOStreamBuffer::underflow() -> int_type
{
    FO_NO_STACK_TRACE_ENTRY();

    return traits_type::eof();
}

auto BackwardOStreamBuffer::overflow(int_type ch) -> int_type
{
    FO_NO_STACK_TRACE_ENTRY();

    const char s[] = {static_cast<char>(ch)};
    WriteBaseLog(string_view {s, 1});
    return ch;
}

auto BackwardOStreamBuffer::xsputn(const char_type* s, std::streamsize count) -> std::streamsize
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_firstCall) {
        WriteHeader();
        _firstCall = false;
    }

    WriteBaseLog(string_view {s, static_cast<string_view::size_type>(count)});
    return count;
}

void BackwardOStreamBuffer::WriteHeader() const noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    WriteBaseLog("\nFATAL ERROR!\n");

    if (!SafeWriteCrashInfo()) {
        WriteBaseLog("Crash reason: unavailable\n");
    }

    WriteBaseLog("\n");

    if (ExceptionHandling->CrashStackTrace.has_value()) {
        SafeWriteStackTrace(*ExceptionHandling->CrashStackTrace);
    }
}

static void SetCrashInfo(string info) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        ExceptionHandling->CrashInfo = std::move(info);
    }
    catch (...) {
        // Best effort: crash handlers must not throw while recording context.
    }
}

static auto SafeWriteCrashInfo() noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        if (ExceptionHandling->CrashInfo.has_value()) {
            WriteBaseLog(strex("Crash reason: {}\n", *ExceptionHandling->CrashInfo));
            return true;
        }
    }
    catch (...) {
    }

    return false;
}

static auto FormatSehCrashInfo(uint32_t code, uint32_t flags, const void* address) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    return strex("SEH exception code: 0x{:08X} ({}) flags: 0x{:08X} address: {}", code, GetSehExceptionName(code), flags, address).str();
}

static auto FormatSignalCrashInfo(int32_t signum, int32_t code, const void* address) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    return strex("Signal {} ({}) code: {} address: {}", signum, GetSignalName(signum), code, address).str();
}

static auto FormatRuntimeCrashInfo(const char* reason) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    string info = strex("Runtime termination: {}", reason != nullptr ? reason : "unknown").str();
    const std::exception_ptr current_exception = std::current_exception();

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

static auto GetSehExceptionName(uint32_t code) noexcept -> string_view
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

static auto GetSignalName(int32_t signum) noexcept -> string_view
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
