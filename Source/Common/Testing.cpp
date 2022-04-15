//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "Testing.h"

#include "Application.h"
#include "FileSystem.h"
#include "Log.h"
#include "ScriptSystem.h"
#include "StringUtils.h"
#include "Timer.h"
#include "Version-Include.h"

#if FO_WINDOWS || FO_LINUX || FO_MAC
#if !FO_WINDOWS
#if __has_include(<libunwind.h>)
#define BACKWARD_HAS_LIBUNWIND 1
#elif __has_include(<bfd.h>)
#define BACKWARD_HAS_BFD 1
#endif
#endif
#include "backward.hpp"
#if FO_WINDOWS
#undef MessageBox
#endif
#endif

#if FO_LINUX || FO_MAC
#include <execinfo.h>
#include <signal.h>
#include <sys/utsname.h>
#endif

static const char* ManualDumpAppendix;
static const char* ManualDumpMessage;

static void DumpAngelScript(DiskFile& file);

#if FO_WINDOWS && !FO_UWP
static LONG WINAPI TopLevelFilterReadableDump(EXCEPTION_POINTERS* except);

void CatchSystemExceptions()
{
    ::SetUnhandledExceptionFilter(TopLevelFilterReadableDump);
}

void CreateDump(const char* appendix, const char* message)
{
    ManualDumpAppendix = appendix;
    ManualDumpMessage = message;

    TopLevelFilterReadableDump(nullptr);
}

static LONG WINAPI TopLevelFilterReadableDump(EXCEPTION_POINTERS* except)
{
    const auto traceback = GetStackTrace();
    string message = (except == nullptr ? ManualDumpMessage : "Exception");

    const auto dt = Timer::GetCurrentDateTime();
    const auto* dump_str = (except != nullptr ? "CrashDump" : ManualDumpAppendix);
    string dump_path = _str("{}_{}_{}_{:04}.{:02}.{:02}_{:02}-{:02}-{:02}.txt", dump_str, GetAppName(), FO_GAME_VERSION, dt.Year, dt.Month, dt.Day, dt.Hour, dt.Minute, dt.Second);

    if (auto file = DiskFileSystem::OpenFile(dump_path, true)) {
        // Generic info
        file.Write(_str("Message\n"));
        file.Write(_str("{}\n", message));
        file.Write(_str("\n"));
        file.Write(_str("Application\n"));
        file.Write(_str("\tName        {}\n", GetAppName()));
        file.Write(_str("\tVersion     {}\n", FO_GAME_VERSION));
        file.Write(_str("\tOS          Windows\n"));
        file.Write(_str("\tTimestamp   {:04}.{:02}.{:02} {:02}:{:02}:{:02}\n", dt.Year, dt.Month, dt.Day, dt.Hour, dt.Minute, dt.Second));
        file.Write(_str("\n"));

        // Exception information
        if (except != nullptr) {
            file.Write(_str("Exception\n"));
            file.Write(_str("\tCode      "));
            switch (except->ExceptionRecord->ExceptionCode) {
#define CASE_EXCEPTION(e) \
    case e: \
        file.Write(#e); \
        message = #e; \
        break
                CASE_EXCEPTION(EXCEPTION_ACCESS_VIOLATION);
                CASE_EXCEPTION(EXCEPTION_DATATYPE_MISALIGNMENT);
                CASE_EXCEPTION(EXCEPTION_BREAKPOINT);
                CASE_EXCEPTION(EXCEPTION_SINGLE_STEP);
                CASE_EXCEPTION(EXCEPTION_ARRAY_BOUNDS_EXCEEDED);
                CASE_EXCEPTION(EXCEPTION_FLT_DENORMAL_OPERAND);
                CASE_EXCEPTION(EXCEPTION_FLT_DIVIDE_BY_ZERO);
                CASE_EXCEPTION(EXCEPTION_FLT_INEXACT_RESULT);
                CASE_EXCEPTION(EXCEPTION_FLT_INVALID_OPERATION);
                CASE_EXCEPTION(EXCEPTION_FLT_OVERFLOW);
                CASE_EXCEPTION(EXCEPTION_FLT_STACK_CHECK);
                CASE_EXCEPTION(EXCEPTION_FLT_UNDERFLOW);
                CASE_EXCEPTION(EXCEPTION_INT_DIVIDE_BY_ZERO);
                CASE_EXCEPTION(EXCEPTION_INT_OVERFLOW);
                CASE_EXCEPTION(EXCEPTION_PRIV_INSTRUCTION);
                CASE_EXCEPTION(EXCEPTION_IN_PAGE_ERROR);
                CASE_EXCEPTION(EXCEPTION_ILLEGAL_INSTRUCTION);
                CASE_EXCEPTION(EXCEPTION_NONCONTINUABLE_EXCEPTION);
                CASE_EXCEPTION(EXCEPTION_STACK_OVERFLOW);
                CASE_EXCEPTION(EXCEPTION_INVALID_DISPOSITION);
                CASE_EXCEPTION(EXCEPTION_GUARD_PAGE);
                CASE_EXCEPTION(EXCEPTION_INVALID_HANDLE);
#undef CASE_EXCEPTION
            case 0xE06D7363:
                file.Write(_str("Unhandled C++ Exception"));
                message = "Unhandled C++ Exception";
                break;
            default:
                file.Write(_str("{}", except->ExceptionRecord->ExceptionCode));
                message = _str("Unknown Exception (code {})", except->ExceptionRecord->ExceptionCode);
                break;
            }
            file.Write(_str("\n"));
            file.Write(_str("\tAddress   {}\n", except->ExceptionRecord->ExceptionAddress));
            file.Write(_str("\tFlags     {}\n", except->ExceptionRecord->ExceptionFlags));
            if (except->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION || except->ExceptionRecord->ExceptionCode == EXCEPTION_IN_PAGE_ERROR) {
                auto read_write = static_cast<int>(except->ExceptionRecord->ExceptionInformation[0]);
                if (read_write == 0) {
                    file.Write(_str("\tInfo      Attempted to read to an {}", reinterpret_cast<void*>(except->ExceptionRecord->ExceptionInformation[1])));
                }
                else if (read_write == 1) {
                    file.Write(_str("\tInfo      Attempted to write to an {}", reinterpret_cast<void*>(except->ExceptionRecord->ExceptionInformation[1])));
                }
                else if (read_write == 8) {
                    file.Write(_str("\tInfo      Data execution prevention to an {}", reinterpret_cast<void*>(except->ExceptionRecord->ExceptionInformation[1])));
                }
                if (except->ExceptionRecord->ExceptionCode == EXCEPTION_IN_PAGE_ERROR) {
                    file.Write(_str(", NTSTATUS {}", reinterpret_cast<void*>(except->ExceptionRecord->ExceptionInformation[2])));
                }
                file.Write(_str("\n"));
            }
            else {
                for (DWORD i = 0; i < except->ExceptionRecord->NumberParameters; i++) {
                    file.Write(_str("\tInfo {}    {}\n", i, reinterpret_cast<void*>(except->ExceptionRecord->ExceptionInformation[i])));
                }
            }
            file.Write(_str("\n"));
        }

        // Stacktrace
        file.Write(traceback);
        file.Write(_str("\n"));

        // AngelScript dump
        DumpAngelScript(file);
    }

    if (except != nullptr) {
        MessageBox::ShowErrorMessage("System Exception", message, traceback);
    }

    return EXCEPTION_EXECUTE_HANDLER;
}

#elif FO_LINUX || FO_MAC
static void TerminationHandler(int signum, siginfo_t* siginfo, void* context);

static struct sigaction OldSIGSEGV;
static struct sigaction OldSIGFPE;

void CatchSystemExceptions()
{
    // SIGILL, SIGFPE, SIGSEGV, SIGBUS, SIGTERM
    // CTRL-C - sends SIGINT which default action is to terminate the application.
    // CTRL-\ - sends SIGQUIT which default action is to terminate the application dumping core.
    // CTRL-Z - sends SIGSTOP that suspends the program.

    struct sigaction act;

    // SIGSEGV
    // Description     Invalid memory reference
    // Default action  Abnormal termination of the process
    std::memset(&act, 0, sizeof(act));
    act.sa_sigaction = &TerminationHandler;
    act.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &act, &OldSIGSEGV);

    // SIGFPE
    // Description     Erroneous arithmetic operation
    // Default action  bnormal termination of the process
    std::memset(&act, 0, sizeof(act));
    act.sa_sigaction = &TerminationHandler;
    act.sa_flags = SA_SIGINFO;
    sigaction(SIGFPE, &act, &OldSIGFPE);
}

void CreateDump(const char* appendix, const char* message)
{
    ManualDumpAppendix = appendix;
    ManualDumpMessage = message;

    TerminationHandler(0, nullptr, nullptr);
}

static void TerminationHandler(int signum, siginfo_t* siginfo, void* context)
{
    const auto traceback = GetStackTrace();

    // Additional description
    static const char* str_SIGSEGV[] = {
        "Address not mapped to object", // SEGV_MAPERR
        "Invalid permissions for mapped object", // SEGV_ACCERR
    };
    static const char* str_SIGFPE[] = {
        "Integer divide by zero", // FPE_INTDIV
        "Integer overflow", // FPE_INTOVF
        "Floating-point divide by zero", // FPE_FLTDIV
        "Floating-point overflow", // FPE_FLTOVF
        "Floating-point underflow", // FPE_FLTUND
        "Floating-point inexact result", // FPE_FLTRES
        "Invalid floating-point operation", // FPE_FLTINV
        "Subscript out of range", // FPE_FLTSUB
    };

    const char* sig_desc = nullptr;
    if (siginfo) {
        if (siginfo->si_signo == SIGSEGV && siginfo->si_code >= 0 && siginfo->si_code < 2)
            sig_desc = str_SIGSEGV[siginfo->si_code];
        if (siginfo->si_signo == SIGFPE && siginfo->si_code >= 0 && siginfo->si_code < 8)
            sig_desc = str_SIGFPE[siginfo->si_code];
    }

    // Format message
    string message;

    if (siginfo) {
        message = strsignal(siginfo->si_signo);
        if (sig_desc)
            message += _str(" ({})", sig_desc);
    }
    else {
        message = ManualDumpMessage;
    }

    // Dump file
    const auto dt = Timer::GetCurrentDateTime();
    string dump_str = (siginfo ? "CrashDump" : ManualDumpAppendix);
    string dump_path = _str("{}_{}_{}_{:04}.{:02}.{:02}_{:02}-{:02}-{:02}.txt", dump_str, GetAppName(), FO_GAME_VERSION, dt.Year, dt.Month, dt.Day, dt.Hour, dt.Minute, dt.Second);

    DiskFile file = DiskFileSystem::OpenFile(dump_path, true);
    if (file) {
        // Generic info
        file.Write(_str("Message\n"));
        file.Write(_str("\t{}\n", message));
        file.Write(_str("\n"));
        file.Write(_str("Application\n"));
        file.Write(_str("\tName        {}\n", GetAppName()));
        file.Write(_str("\tVersion     {}\n", FO_GAME_VERSION));
        struct utsname ver;
        uname(&ver);
        file.Write(_str("\tOS          {} / {} / {}\n", ver.sysname, ver.release, ver.version));
        file.Write(_str("\tTimestamp   {:02}.{:02}.{:04} {:02}:{:02}:{:02}\n", dt.Day, dt.Month, dt.Year, dt.Hour, dt.Minute, dt.Second));
        file.Write(_str("\n"));

        // Exception information
        if (siginfo) {
            file.Write(_str("Exception\n"));
            file.Write(_str("\tSigno   {} ({})\n", strsignal(siginfo->si_signo), siginfo->si_signo));
            file.Write(_str("\tCode    {} ({})\n", sig_desc ? sig_desc : "No description", siginfo->si_code));
            file.Write(_str("\tErrno   {} ({})\n", strerror(siginfo->si_errno), siginfo->si_errno));
            file.Write(_str("\n"));
        }

        // Stacktrace
        file.Write(traceback);
        file.Write(_str("\n"));

        // AngelScript dump
        DumpAngelScript(file);
    }

    if (siginfo) {
        MessageBox::ShowErrorMessage("System Exception", message, traceback);
        exit(1);
    }
}

#else
// Todo: improve global exceptions handlers for mobile os

void CatchSystemExceptions()
{
    //
}

void CreateDump(const char* appendix, const char* message)
{
    //
}

#endif

auto GetStackTrace() -> string
{
#if FO_WINDOWS || FO_LINUX || FO_MAC
    [[maybe_unused]] backward::TraceResolver resolver;
    backward::StackTrace st;
    st.load_here(42);
    backward::Printer st_printer;
    st_printer.snippet = false;
    std::stringstream ss;
    st_printer.print(st, ss);
    return ss.str();
#else

    return "Not supported";
#endif
}

static void DumpAngelScript(DiskFile& file)
{
    // Todo: fix script system
    // string tb = Script::GetTraceback();
    // if (!tb.empty())
    //    file.Write(_str("AngelScript\n{}", tb));
}

static void ShowErrorMessage(string_view message, bool is_fatal)
{
    // Show message
    if (is_fatal) {
        MessageBox::ShowErrorMessage("Fatal Error", message, GetStackTrace());
    }
    else {
        MessageBox::ShowErrorMessage("Error", message, GetStackTrace());
    }
}

void ReportExceptionAndExit(const std::exception& ex)
{
    WriteLog("{}\n", ex.what());
    CreateDump("FatalException", ex.what());
    ShowErrorMessage(ex.what(), true);
    std::exit(1);
}

void ReportExceptionAndContinue(const std::exception& ex)
{
    WriteLog("{}\n", ex.what());
#if FO_DEBUG
    CreateDump("Exception", ex.what());
    ShowErrorMessage(ex.what(), false);
#endif
}
