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

#include "DiagnosticSelfTest.h"

FO_BEGIN_NAMESPACE

static void RunSelfTestCrash(string_view mode);
[[noreturn]] static void CrashByBadPointerAccess(uintptr_t address, bool write);
[[noreturn]] static void CrashByStackOverflow();
[[noreturn]] static void CrashByIntegerDivideByZero();
[[noreturn]] static void CrashByAbort();
[[noreturn]] static void CrashByNoexceptThrow() noexcept;
[[noreturn]] static void CrashByThrow();
[[noreturn]] static void CrashByStrongAssert();
static void ThrowSelfTestException();
FO_NO_INLINE static auto RecurseUntilStackOverflow(int depth) -> int;

void DiagnosticSelfTest::RunIfRequested()
{
    FO_STACK_TRACE_ENTRY();

    const char* env = std::getenv("FO_SELFTEST_CRASH");

    if (env == nullptr || env[0] == '\0') {
        return;
    }

    RunSelfTestCrash(string_view {env});
}

static void RunSelfTestCrash(string_view mode)
{
    FO_STACK_TRACE_ENTRY();

    WriteLog(LogType::Warning, "Diagnostic self-test: inducing crash '{}'", mode);

    // Non-main-thread variants: a raw std::thread is the faithful model of an
    // unguarded engine thread dying (WorkThread jobs are try/catch wrapped, so a
    // job throw is reported-and-continued rather than a process kill).
    if (strvex(mode).starts_with("thread_")) {
        const string main_mode = strex("main_{}", mode.substr(std::size("thread_") - 1)).str();

        auto crasher = std::thread([main_mode] {
            // Mirror what every long-lived engine worker thread does at entry so the self-test
            // faithfully exercises a properly set-up thread (notably for stack-overflow crashes).
            InstallCrashHandlerStackForThisThread();
            RunSelfTestCrash(main_mode);
        });

        crasher.join();
        return;
    }

    if (mode == "main_null_read") {
        CrashByBadPointerAccess(0, false);
    }
    else if (mode == "main_null_write") {
        CrashByBadPointerAccess(0, true);
    }
    else if (mode == "main_wild_write") {
        CrashByBadPointerAccess(0xDEADBEEF, true);
    }
    else if (mode == "main_stack_overflow") {
        CrashByStackOverflow();
    }
    else if (mode == "main_fpe") {
        CrashByIntegerDivideByZero();
    }
    else if (mode == "main_abort") {
        CrashByAbort();
    }
    else if (mode == "main_noexcept_throw") {
        CrashByNoexceptThrow();
    }
    else if (mode == "main_throw") {
        CrashByThrow();
    }
    else if (mode == "main_strong_assert") {
        CrashByStrongAssert();
    }
    else {
        WriteLog(LogType::Warning, "Diagnostic self-test: unknown crash mode '{}', continuing", mode);
    }
}

static void CrashByBadPointerAccess(uintptr_t address, bool write)
{
    FO_NO_STACK_TRACE_ENTRY();

    // Launder the address through volatile so the optimizer cannot fold the
    // access away as undefined behavior; the volatile-qualified access must be emitted.
    volatile uintptr_t laundered = address;
    volatile int* ptr = reinterpret_cast<volatile int*>(laundered);

    if (write) {
        *ptr = 0x1234;
    }
    else {
        volatile int value = *ptr;
        ignore_unused(value);
    }

    // Unreachable on any real platform; abort loudly if the access was somehow tolerated.
    std::abort();
}

static void CrashByStackOverflow()
{
    FO_NO_STACK_TRACE_ENTRY();

    const int result = RecurseUntilStackOverflow(0);
    ignore_unused(result);

    std::abort();
}

FO_GCC_IGNORE_WARNINGS_PUSH("-Winfinite-recursion")
FO_CLANG_IGNORE_WARNINGS_PUSH("-Winfinite-recursion")
FO_MSVC_IGNORE_WARNINGS_PUSH(4717)
static auto RecurseUntilStackOverflow(int depth) -> int
{
    FO_NO_STACK_TRACE_ENTRY();

    // A large volatile frame plus using it after the recursive call defeats both
    // tail-call optimization and register promotion, so each frame really grows the stack.
    volatile char blocker[8192];
    blocker[0] = static_cast<char>(depth & 0xFF);
    blocker[sizeof(blocker) - 1] = static_cast<char>(depth & 0xFF);

    const int deeper = RecurseUntilStackOverflow(depth + 1);

    return deeper + blocker[0] + blocker[sizeof(blocker) - 1];
}
FO_MSVC_IGNORE_WARNINGS_POP()
FO_CLANG_IGNORE_WARNINGS_POP()
FO_GCC_IGNORE_WARNINGS_POP()

static void CrashByIntegerDivideByZero()
{
    FO_NO_STACK_TRACE_ENTRY();

    volatile int zero = 0;
    volatile int one = 1;
    volatile int result = one / zero;
    ignore_unused(result);

    std::abort();
}

static void CrashByAbort()
{
    FO_NO_STACK_TRACE_ENTRY();

    std::abort();
}

FO_MSVC_IGNORE_WARNINGS_PUSH(4702)
static void CrashByNoexceptThrow() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    // A noexcept function that calls a throwing helper: the escaping exception cannot cross the
    // noexcept boundary, so std::terminate is invoked. This is the classic real-world terminate bug.
    // The trailing abort is intentionally unreachable defensive code (the throw never returns).
    ThrowSelfTestException();
    std::abort();
}
FO_MSVC_IGNORE_WARNINGS_POP()

static void ThrowSelfTestException()
{
    FO_STACK_TRACE_ENTRY();

    throw GenericException("Self-test crash: exception escaping noexcept function");
}

static void CrashByThrow()
{
    FO_STACK_TRACE_ENTRY();

    // Propagates up to the application entry point's catch and the graceful exception reporter.
    throw GenericException("Self-test crash: unhandled exception");
}

static void CrashByStrongAssert()
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(false, "Self-test crash: strong assertion");
    std::abort();
}

FO_END_NAMESPACE
