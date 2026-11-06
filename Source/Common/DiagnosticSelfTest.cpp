// FOnline Engine
// SPDX-License-Identifier: MIT

#include "DiagnosticSelfTest.h"

FO_BEGIN_NAMESPACE

static void RunSelfTestCrash(string_view mode);
[[noreturn]] static void CrashByBadPointerAccess(uintptr_t address, bool write);
[[noreturn]] static void CrashByBadCall();
[[noreturn]] static void CrashByStackOverflow();
[[noreturn]] static void CrashByIntegerDivideByZero();
[[noreturn]] static void CrashByAbort();
[[noreturn]] static void CrashByNoexceptThrow() noexcept;
[[noreturn]] static void CrashByThrow();
[[noreturn]] static void CrashByStrongAssert();
[[noreturn]] static void CrashByBasicStrongAssert();
[[noreturn]] static void CrashByFatalExit();
static void ThrowSelfTestException();
FO_NO_INLINE static auto RecurseUntilStackOverflow(int depth) -> int;
using StackOverflowRecursor = int (*)(int);
static auto GetStackOverflowRecursor() -> StackOverflowRecursor;

void DiagnosticSelfTest::RunIfRequested()
{
    const char* env {std::getenv("FO_SELFTEST_CRASH")};

    if (env == nullptr || env[0] == '\0') {
        return;
    }

    RunSelfTestCrash(string_view {env});
}

static void RunSelfTestCrash(string_view mode)
{
    logging::write(logging::type::warning, "Diagnostic self-test: inducing crash '{}'", mode);

    // Raw std::thread models an unguarded engine-thread failure.
    // work_thread catches job exceptions and therefore cannot exercise process termination
    if (strvex(mode).starts_with("thread_")) {
        string main_mode {strex("main_{}", mode.substr(std::size("thread_") - 1)).str()};

        auto crasher {std::thread([main_mode] {
            // Mirror what every long-lived engine worker thread does at entry so the self-test
            // faithfully exercises a properly set-up thread (notably for stack-overflow crashes)
            exceptions::install_crash_handler_stack();
            RunSelfTestCrash(main_mode);
        })};

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
    else if (mode == "main_bad_call") {
        CrashByBadCall();
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
    else if (mode == "main_basic_strong_assert") {
        CrashByBasicStrongAssert();
    }
    else if (mode == "main_fatal_exit") {
        CrashByFatalExit();
    }
    else if (mode == "main_failure_exit") {
        logging::write("Self-test controlled failure exit");
        exit_app(false);
    }
    else {
        logging::write(logging::type::warning, "Diagnostic self-test: unknown crash mode '{}', continuing", mode);
    }
}

static void CrashByBadPointerAccess(uintptr_t address, bool write)
{
    // Launder the address through volatile so the optimizer cannot fold the
    // access away as undefined behavior; the volatile-qualified access must be emitted
    volatile uintptr_t laundered {address};
    volatile int* ptr {reinterpret_cast<volatile int*>(laundered)};

    if (write) {
        *ptr = 0x1234;
    }
    else {
        volatile int value {*ptr};
        ignore_unused(value);
    }

    // Unreachable on any real platform; abort loudly if the access was somehow tolerated
    std::abort();
}

static void CrashByBadCall()
{
    // A function pointer read back through volatile cannot be proven null, so the call is emitted and faults on the
    // instruction fetch, leaving no frame of its own
    using CrashTarget = void (*)();
    volatile uintptr_t laundered {0};
    auto target {std::bit_cast<CrashTarget>(static_cast<uintptr_t>(laundered))};
    target();

    // Unreachable on any real platform; abort loudly if the call was somehow tolerated
    std::abort();
}

static void CrashByStackOverflow()
{
    int32_t result {RecurseUntilStackOverflow(0)};
    ignore_unused(result);

    std::abort();
}

FO_GCC_IGNORE_WARNINGS_PUSH("-Winfinite-recursion")
FO_CLANG_IGNORE_WARNINGS_PUSH("-Winfinite-recursion")
FO_MSVC_IGNORE_WARNINGS_PUSH(4717)
static auto RecurseUntilStackOverflow(int depth) -> int
{
    // A large volatile frame plus using it after the recursive call defeats both
    // tail-call optimization and register promotion, so each frame really grows the stack
    volatile char blocker[8192] {};
    blocker[0] = static_cast<char>(depth & 0xFF);
    blocker[sizeof(blocker) - 1] = static_cast<char>(depth & 0xFF);

    StackOverflowRecursor recursor {GetStackOverflowRecursor()};
    int32_t deeper {recursor(depth + 1)};

    return deeper + blocker[0] + blocker[sizeof(blocker) - 1];
}
FO_MSVC_IGNORE_WARNINGS_POP()
FO_CLANG_IGNORE_WARNINGS_POP()
FO_GCC_IGNORE_WARNINGS_POP()

static auto GetStackOverflowRecursor() -> StackOverflowRecursor
{
    static StackOverflowRecursor volatile recursor {RecurseUntilStackOverflow};

    return recursor;
}

static void CrashByIntegerDivideByZero()
{
    volatile int zero {0};
    volatile int one {1};
    volatile int result {one / zero};
    ignore_unused(result);

    std::abort();
}

static void CrashByAbort()
{
    std::abort();
}

FO_MSVC_IGNORE_WARNINGS_PUSH(4702)
static void CrashByNoexceptThrow() noexcept
{
    // Escaping a noexcept boundary invokes std::terminate.
    // The trailing abort is intentionally unreachable
    ThrowSelfTestException();
    std::abort();
}
FO_MSVC_IGNORE_WARNINGS_POP()

static void ThrowSelfTestException()
{
    throw GenericException("Self-test crash: exception escaping noexcept function");
}

static void CrashByThrow()
{
    // Propagates up to the application entry point's catch and the graceful exception reporter
    throw GenericException("Self-test crash: unhandled exception");
}

static void CrashByStrongAssert()
{
    FO_STRONG_ASSERT(false, "Self-test crash: strong assertion");
    std::abort();
}

static void CrashByBasicStrongAssert()
{
    bool self_test_condition {false};
    FO_BASIC_STRONG_ASSERT(self_test_condition);
    std::abort();
}

static void CrashByFatalExit()
{
    fatal::report_and_exit("Self-test crash: fatal exit");
}

FO_END_NAMESPACE
