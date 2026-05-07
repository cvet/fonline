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
};
FO_GLOBAL_DATA(ExceptHandlingData, ExceptionHandling);

FO_END_NAMESPACE
extern void SetCrashStackTrace() noexcept // Called in backward.hpp
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_NAMESPACE ExceptionHandling->CrashStackTrace = FO_NAMESPACE GetStackTrace();
}
FO_BEGIN_NAMESPACE

class BackwardOStreamBuffer : public std::streambuf
{
public:
    BackwardOStreamBuffer() = default;
    BackwardOStreamBuffer(const BackwardOStreamBuffer&) = delete;
    BackwardOStreamBuffer(BackwardOStreamBuffer&&) noexcept = delete;
    auto operator=(const BackwardOStreamBuffer&) = delete;
    auto operator=(BackwardOStreamBuffer&&) noexcept -> BackwardOStreamBuffer& = delete;
    ~BackwardOStreamBuffer() override = default;

    auto underflow() -> int_type override
    {
        //
        return traits_type::eof();
    }

    auto overflow(int_type ch) -> int_type override
    {
        const char s[] = {static_cast<char>(ch)};
        WriteBaseLog(string_view {s, 1});
        return ch;
    }

    auto xsputn(const char_type* s, std::streamsize count) -> std::streamsize override /*noexcept*/
    {
        if (_firstCall) {
            WriteHeader();
            _firstCall = false;
        }

        WriteBaseLog(string_view {s, static_cast<string_view::size_type>(count)});
        return count;
    }

private:
    void WriteHeader() const noexcept
    {
        WriteBaseLog("\nFATAL ERROR!\n\n");

        if (ExceptionHandling->CrashStackTrace.has_value()) {
            SafeWriteStackTrace(*ExceptionHandling->CrashStackTrace);
        }
    }

    bool _firstCall = true;
};

static BackwardOStreamBuffer CrashStreamBuf;
static auto CrashStream = std::ostream(&CrashStreamBuf); // Passed to Printer::print in backward.hpp

FO_END_NAMESPACE
extern auto GetCrashStream() noexcept -> std::ostream& // Passed to Printer::print in backward.hpp
{
    FO_NO_STACK_TRACE_ENTRY();

    return FO_NAMESPACE CrashStream;
}
FO_BEGIN_NAMESPACE

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

FO_END_NAMESPACE
