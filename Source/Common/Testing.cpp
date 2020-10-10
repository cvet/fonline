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

#ifdef FO_TESTING
#define CATCH_CONFIG_RUNNER
#include "catch.hpp"
#endif

#include "Testing.h"

#include "FileSystem.h"
#include "GenericUtils.h"
#include "Log.h"
#include "MessageBox.h"
#include "ScriptSystem.h"
#include "StringUtils.h"
#include "Timer.h"
#include "Version-Include.h"
#include "WinApi-Include.h"

#if defined(FO_WINDOWS) && !defined(WINRT)
#pragma warning(disable : 4091)
#pragma warning(disable : 4996)
#include <DbgHelp.h>
#include <Psapi.h>
#include <TlHelp32.h>
#endif
#if !defined(FO_WINDOWS) && !defined(FO_ANDROID) && !defined(FO_WEB) && !defined(FO_IOS)
#ifdef FO_LINUX
#define BACKWARD_HAS_BFD 1
#endif
#include "backward.hpp"
#include <execinfo.h>
#include <signal.h>
#include <sys/utsname.h>
#endif

#ifdef FO_TESTING
#include "SDL_main.h"
#endif

struct TestingData
{
    string AppName {};
    string AppVer {};
    string ManualDumpAppendix {};
    string ManualDumpMessage {};
};
GLOBAL_DATA(TestingData, Data);

#ifdef FO_TESTING
extern "C" int main(int argc, char** argv) // Handled by SDL
{
    CreateGlobalData();
    CatchExceptions("FOnlineTesting", FO_VERSION);
    LogToFile("FOnlineTesting.log");
    return Catch::Session().run(argc, argv);
}
#endif

#if defined(FO_WINDOWS) && !defined(WINRT)
#if UINTPTR_MAX == 0xFFFFFFFF
#define WIN32BIT
#pragma warning(disable : 4748)
#endif

constexpr auto STACKWALK_MAX_NAMELEN = 1024;

static LONG WINAPI TopLevelFilterReadableDump(EXCEPTION_POINTERS* except);
static void DumpAngelScript(DiskFile& file);

// Old version of the structure, used before Vista
typedef struct ImagehlpModule64V2
{
    DWORD SizeOfStruct; // set to sizeof(IMAGEHLP_MODULE64)
    DWORD64 BaseOfImage; // base load address of module
    DWORD ImageSize; // virtual size of the loaded module
    DWORD TimeDateStamp; // date/time stamp from pe header
    DWORD CheckSum; // checksum from the pe header
    DWORD NumSyms; // number of symbols in the symbol table
    SYM_TYPE SymType; // type of symbols loaded
    CHAR ModuleName[32]; // module name
    CHAR ImageName[256]; // image name
    CHAR LoadedImageName[256]; // symbol file name
} imagehlp_module64_v2;

void CatchExceptions(const string& app_name, int app_ver)
{
    Data->AppName = app_name;
    Data->AppVer = _str("{:#x}", app_ver);

    if (!app_name.empty()) {
        SetUnhandledExceptionFilter(TopLevelFilterReadableDump);
    }
    else {
        SetUnhandledExceptionFilter(nullptr);
    }
}

static LONG WINAPI TopLevelFilterReadableDump(EXCEPTION_POINTERS* except)
{
    string message;
    string traceback;

    DiskFileSystem::ResetCurDir();
    const auto dt = Timer::GetCurrentDateTime();
    auto dump_str = except != nullptr ? "CrashDump" : Data->ManualDumpAppendix;
    string dump_path = _str("{}_{}_{}_{:04}.{:02}.{:02}_{:02}-{:02}-{:02}.txt", dump_str, Data->AppName, Data->AppVer, dt.Year, dt.Month, dt.Day, dt.Hour, dt.Minute, dt.Second);

    DiskFileSystem::MakeDirTree(dump_path);
    if (auto file = DiskFileSystem::OpenFile(dump_path, true)) {
        if (except == nullptr) {
            message = Data->ManualDumpMessage;
        }
        else {
            message = "Exception";
        }

        // Generic info
        file.Write(_str("Message\n"));
        file.Write(_str("{}\n", message));
        file.Write(_str("\n"));
        file.Write(_str("Application\n"));
        file.Write(_str("\tName        {}\n", Data->AppName));
        file.Write(_str("\tVersion     {}\n", Data->AppVer));
        OSVERSIONINFOW ver;
        std::memset(&ver, 0, sizeof(OSVERSIONINFOW));
        ver.dwOSVersionInfoSize = sizeof(ver);
        if (::GetVersionExW(&ver) != 0) {
            file.Write(_str("\tOS          {}.{}.{} ({})\n", ver.dwMajorVersion, ver.dwMinorVersion, ver.dwBuildNumber, _str().parseWideChar(ver.szCSDVersion)));
        }
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

        // AngelScript dump
        DumpAngelScript(file);

        // Collect current threads
        auto* process = GetCurrentProcess();
        DWORD threads_ids[1024] = {GetCurrentThreadId()};
        uint threads_ids_count = 1;
        if (auto* snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0); snapshot != INVALID_HANDLE_VALUE) {
            THREADENTRY32 te;
            te.dwSize = sizeof(te);
            if (Thread32First(snapshot, &te) != 0) {
                while (true) {
                    if (te.dwSize >= FIELD_OFFSET(THREADENTRY32, th32OwnerProcessID) + sizeof(te.th32OwnerProcessID)) {
                        if (te.th32OwnerProcessID == GetCurrentProcessId() && te.th32ThreadID != threads_ids[0]) {
                            threads_ids[threads_ids_count++] = te.th32ThreadID;
                        }
                    }
                    te.dwSize = sizeof(te);
                    if (Thread32Next(snapshot, &te) == 0) {
                        break;
                    }
                }
            }
            CloseHandle(snapshot);
        }
        else {
            file.Write("CreateToolhelp32Snapshot fail\n");
        }

        // Set exe dir for PDB symbols
        SetCurrentDirectoryW(_str(DiskFileSystem::GetExePath()).extractDir().toWideChar().c_str());

        // Init symbols
        SymInitialize(process, nullptr, TRUE);
        SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS);

        // Print information about each thread
        for (uint i = 0; i < threads_ids_count; i++) {
            auto tid = threads_ids[i];
            auto* t = OpenThread(THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION | THREAD_GET_CONTEXT, FALSE, tid);
            file.Write(_str("Thread {}\n", tid));

            CONTEXT context;
            std::memset(&context, 0, sizeof(context));
            context.ContextFlags = CONTEXT_FULL;

            if (tid == GetCurrentThreadId()) {
                if (except != nullptr) {
                    std::memcpy(&context, except->ContextRecord, sizeof(CONTEXT));
                }
                else {
#ifdef WIN32BIT
                    __asm label : __asm mov[context.Ebp], ebp;
                    __asm mov[context.Esp], esp;
                    __asm mov eax, [label];
                    __asm mov[context.Eip], eax;
#else
                    RtlCaptureContext(&context);
#endif
                }
            }
            else {
                SuspendThread(t); // -V720
                GetThreadContext(t, &context);
                ResumeThread(t);
            }

            STACKFRAME64 stack;
            std::memset(&stack, 0, sizeof(stack));

#ifdef WIN32BIT
            DWORD machine_type = IMAGE_FILE_MACHINE_I386;
            stack.AddrFrame.Mode = AddrModeFlat;
            stack.AddrFrame.Offset = context.Ebp;
            stack.AddrPC.Mode = AddrModeFlat;
            stack.AddrPC.Offset = context.Eip;
            stack.AddrStack.Mode = AddrModeFlat;
            stack.AddrStack.Offset = context.Esp;
#else
            DWORD machine_type = IMAGE_FILE_MACHINE_AMD64;
            stack.AddrPC.Offset = context.Rip;
            stack.AddrPC.Mode = AddrModeFlat;
            stack.AddrFrame.Offset = context.Rsp;
            stack.AddrFrame.Mode = AddrModeFlat;
            stack.AddrStack.Offset = context.Rsp;
            stack.AddrStack.Mode = AddrModeFlat;
#endif

            char symbol_buffer[sizeof(SYMBOL_INFO) + STACKWALK_MAX_NAMELEN];
            auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbol_buffer);
            std::memset(symbol, 0, sizeof(SYMBOL_INFO) + STACKWALK_MAX_NAMELEN);
            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            symbol->MaxNameLen = STACKWALK_MAX_NAMELEN;

            struct Rpm
            {
                static BOOL __stdcall Call(HANDLE hProcess, DWORD64 qwBaseAddress, PVOID lpBuffer, DWORD nSize, LPDWORD lpNumberOfBytesRead)
                {
                    SIZE_T st = 0;
                    const auto ret = ReadProcessMemory(hProcess, reinterpret_cast<LPVOID>(qwBaseAddress), lpBuffer, nSize, &st);
                    *lpNumberOfBytesRead = static_cast<DWORD>(st);
                    return ret;
                }
            };

            auto frame_num = 0;
            while (StackWalk64(machine_type, process, t, &stack, &context, Rpm::Call, SymFunctionTableAccess64, SymGetModuleBase64, nullptr) != 0) {
                struct Callstack
                {
                    struct CallstackEntry
                    {
                        DWORD64 Offset {};
                        CHAR Name[STACKWALK_MAX_NAMELEN] {};
                        CHAR UndName[STACKWALK_MAX_NAMELEN] {};
                        CHAR UndFullName[STACKWALK_MAX_NAMELEN] {};
                        DWORD64 OffsetFromSmybol {};
                        DWORD OffsetFromLine {};
                        DWORD LineNumber {};
                        CHAR LineFileName[STACKWALK_MAX_NAMELEN] {};
                        DWORD SymType {};
                        LPCSTR SymTypeString {};
                        CHAR ModuleName[STACKWALK_MAX_NAMELEN] {};
                        DWORD64 BaseOfImage {};
                        CHAR LoadedImageName[STACKWALK_MAX_NAMELEN] {};
                    };

                    static void OnCallstackEntry(bool is_current_thread, string& traceback, DiskFile& file, int frame_num, CallstackEntry& entry)
                    {
                        if (frame_num >= 0 && entry.Offset != 0) {
                            if (entry.Name[0] == 0) {
                                strcpy_s(entry.Name, "(function-name not available)");
                            }
                            if (entry.UndName[0] != 0) {
                                strcpy_s(entry.Name, entry.UndName);
                            }
                            if (entry.UndFullName[0] != 0) {
                                strcpy_s(entry.Name, entry.UndFullName);
                            }
                            if (entry.ModuleName[0] == 0) {
                                strcpy_s(entry.ModuleName, "???");
                            }

                            file.Write(_str("\t{}, {} + {}", entry.ModuleName, entry.Name, entry.OffsetFromSmybol));
                            if (is_current_thread) {
                                traceback += _str("    {}, {}", entry.ModuleName, entry.Name);
                            }
                            if (entry.LineFileName[0] != 0) {
                                file.Write(_str(", {} (line {})\n", entry.LineFileName, entry.LineNumber));
                                if (is_current_thread) {
                                    traceback += _str(", {} at line {}\n", entry.LineFileName, entry.LineNumber);
                                }
                            }
                            else {
                                file.Write(_str("\n"));
                                if (is_current_thread) {
                                    traceback += _str("+ {}\n", entry.OffsetFromSmybol);
                                }
                            }
                        }
                    }
                };

                Callstack::CallstackEntry callstack;
                std::memset(&callstack, 0, sizeof(callstack));
                callstack.Offset = stack.AddrPC.Offset;

                IMAGEHLP_LINE64 line;
                std::memset(&line, 0, sizeof(line));
                line.SizeOfStruct = sizeof(line);

                if (stack.AddrPC.Offset == stack.AddrReturn.Offset) {
                    file.Write(_str("\tEndless callstack!\n"));
                    break;
                }

                if (stack.AddrPC.Offset != 0) {
                    if (SymFromAddr(process, stack.AddrPC.Offset, &callstack.OffsetFromSmybol, symbol) != 0) {
                        strcpy_s(callstack.Name, symbol->Name);
                        UnDecorateSymbolName(symbol->Name, callstack.UndName, STACKWALK_MAX_NAMELEN, UNDNAME_NAME_ONLY);
                        UnDecorateSymbolName(symbol->Name, callstack.UndFullName, STACKWALK_MAX_NAMELEN, UNDNAME_COMPLETE);
                    }

                    if (SymGetLineFromAddr64(process, stack.AddrPC.Offset, &callstack.OffsetFromLine, &line) != 0) {
                        callstack.LineNumber = line.LineNumber;
                        strcpy_s(callstack.LineFileName, _str(line.FileName).extractFileName().c_str());
                    }

                    IMAGEHLP_MODULE64 module;
                    std::memset(&module, 0, sizeof(IMAGEHLP_MODULE64));
                    module.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
                    if ((SymGetModuleInfo64(process, stack.AddrPC.Offset, &module) != 0) || ((module.SizeOfStruct = sizeof(imagehlp_module64_v2), SymGetModuleInfo64(process, stack.AddrPC.Offset, &module)) != 0)) {
                        switch (module.SymType) {
                        case SymNone:
                            callstack.SymTypeString = "-nosymbols-";
                            break;
                        case SymCoff:
                            callstack.SymTypeString = "COFF";
                            break;
                        case SymCv:
                            callstack.SymTypeString = "CV";
                            break;
                        case SymPdb:
                            callstack.SymTypeString = "PDB";
                            break;
                        case SymExport:
                            callstack.SymTypeString = "-exported-";
                            break;
                        case SymDeferred:
                            callstack.SymTypeString = "-deferred-";
                            break;
                        case SymSym:
                            callstack.SymTypeString = "SYM";
                            break;
#if API_VERSION_NUMBER >= 9
                        case SymDia:
                            callstack.SymTypeString = "DIA";
                            break;
#endif
                        case SymVirtual:
                            callstack.SymTypeString = "Virtual";
                            break;
                        default:
                            callstack.SymTypeString = nullptr;
                            break;
                        }

                        strcpy_s(callstack.ModuleName, module.ModuleName);
                        callstack.BaseOfImage = module.BaseOfImage;
                        strcpy_s(callstack.LoadedImageName, module.LoadedImageName);
                    }
                }

                Callstack::OnCallstackEntry(i == 0, traceback, file, frame_num, callstack);
                if (stack.AddrReturn.Offset == 0) {
                    break;
                }

                frame_num++;
            }

            file.Write(_str("\n"));
            CloseHandle(t);
        }

        // Print modules
        HMODULE modules[1024];
        DWORD needed = 0;
        file.Write(_str("Loaded modules\n"));
        if (EnumProcessModules(process, modules, sizeof(modules), &needed)) {
            for (auto i = 0; i < static_cast<int>(needed / sizeof(HMODULE)); i++) {
                wchar_t module_name[MAX_PATH] = {0};
                if (GetModuleFileNameExW(process, modules[i], module_name, sizeof(module_name) / sizeof(wchar_t))) {
                    file.Write(_str("\t{} ({})\n", _str().parseWideChar(module_name).c_str(), static_cast<void*>(modules[i])));
                }
                else {
                    file.Write(_str("\tGetModuleFileNameExW fail\n"));
                }
            }
        }

        SymCleanup(process);
        CloseHandle(process);
    }

    if (except != nullptr) {
        MessageBox::ShowErrorMessage(message, traceback);
    }

    return EXCEPTION_EXECUTE_HANDLER;
}

void CreateDump(const string& appendix, const string& message)
{
    Data->ManualDumpAppendix = appendix;
    Data->ManualDumpMessage = message;

    TopLevelFilterReadableDump(nullptr);
}

static auto GetTraceback() -> string
{
    string traceback;
    traceback.reserve(512);
    traceback = "Traceback:\n";

    auto* process = ::GetCurrentProcess();
    auto thread_id = ::GetCurrentThreadId();

    SetCurrentDirectoryW(_str(DiskFileSystem::GetExePath()).extractDir().toWideChar().c_str());
    SymInitialize(process, nullptr, TRUE);
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS);

    auto* t = OpenThread(THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION | THREAD_GET_CONTEXT, FALSE, thread_id);

    CONTEXT context;
    std::memset(&context, 0, sizeof(context));
    context.ContextFlags = CONTEXT_FULL;

#ifdef WIN32BIT
    __asm label : __asm mov[context.Ebp], ebp;
    __asm mov[context.Esp], esp;
    __asm mov eax, [label];
    __asm mov[context.Eip], eax;
#else
    RtlCaptureContext(&context);
#endif

    STACKFRAME64 stack;
    std::memset(&stack, 0, sizeof(stack));

#ifdef WIN32BIT
    DWORD machine_type = IMAGE_FILE_MACHINE_I386;
    stack.AddrFrame.Mode = AddrModeFlat;
    stack.AddrFrame.Offset = context.Ebp;
    stack.AddrPC.Mode = AddrModeFlat;
    stack.AddrPC.Offset = context.Eip;
    stack.AddrStack.Mode = AddrModeFlat;
    stack.AddrStack.Offset = context.Esp;
#else
    DWORD machine_type = IMAGE_FILE_MACHINE_AMD64;
    stack.AddrPC.Offset = context.Rip;
    stack.AddrPC.Mode = AddrModeFlat;
    stack.AddrFrame.Offset = context.Rsp;
    stack.AddrFrame.Mode = AddrModeFlat;
    stack.AddrStack.Offset = context.Rsp;
    stack.AddrStack.Mode = AddrModeFlat;
#endif

    char symbol_buffer[sizeof(SYMBOL_INFO) + STACKWALK_MAX_NAMELEN];
    auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbol_buffer);
    std::memset(symbol, 0, sizeof(SYMBOL_INFO) + STACKWALK_MAX_NAMELEN);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = STACKWALK_MAX_NAMELEN;

    struct Rpm
    {
        static BOOL __stdcall Call(HANDLE hProcess, DWORD64 qwBaseAddress, PVOID lpBuffer, DWORD nSize, LPDWORD lpNumberOfBytesRead)
        {
            SIZE_T st = 0;
            const auto ret = ReadProcessMemory(hProcess, reinterpret_cast<LPVOID>(qwBaseAddress), lpBuffer, nSize, &st);
            *lpNumberOfBytesRead = static_cast<DWORD>(st);
            return ret;
        }
    };

    auto frame_num = 0;
    while (StackWalk64(machine_type, process, t, &stack, &context, Rpm::Call, SymFunctionTableAccess64, SymGetModuleBase64, nullptr) != 0) {
        struct Callstack
        {
            struct CallstackEntry
            {
                DWORD64 Offset;
                CHAR Name[STACKWALK_MAX_NAMELEN];
                CHAR UndName[STACKWALK_MAX_NAMELEN];
                CHAR UndFullName[STACKWALK_MAX_NAMELEN];
                DWORD64 OffsetFromSmybol;
                DWORD OffsetFromLine;
                DWORD LineNumber;
                CHAR LineFileName[STACKWALK_MAX_NAMELEN];
                DWORD SymType;
                LPCSTR SymTypeString;
                CHAR ModuleName[STACKWALK_MAX_NAMELEN];
                DWORD64 BaseOfImage;
                CHAR LoadedImageName[STACKWALK_MAX_NAMELEN];
            };

            static void OnCallstackEntry(string& traceback, int frame_num, CallstackEntry& entry)
            {
                if (frame_num >= 0 && entry.Offset != 0) {
                    if (entry.Name[0] == 0) {
                        strcpy_s(entry.Name, "(function-name not available)");
                    }
                    if (entry.UndName[0] != 0) {
                        strcpy_s(entry.Name, entry.UndName);
                    }
                    if (entry.UndFullName[0] != 0) {
                        strcpy_s(entry.Name, entry.UndFullName);
                    }
                    if (entry.ModuleName[0] == 0) {
                        strcpy_s(entry.ModuleName, "???");
                    }

                    traceback += _str("  {}, {}", entry.ModuleName, entry.Name);

                    if (entry.LineFileName[0] != 0) {
                        traceback += _str(", {} at line {}\n", entry.LineFileName, entry.LineNumber);
                    }
                    else {
                        traceback += _str("+ {}\n", entry.OffsetFromSmybol);
                    }
                }
            }
        };

        Callstack::CallstackEntry callstack {};
        std::memset(&callstack, 0, sizeof(callstack));
        callstack.Offset = stack.AddrPC.Offset;

        IMAGEHLP_LINE64 line;
        std::memset(&line, 0, sizeof(line));
        line.SizeOfStruct = sizeof(line);

        if (stack.AddrPC.Offset == stack.AddrReturn.Offset) {
            traceback += _str("  Endless callstack!\n");
            break;
        }

        if (stack.AddrPC.Offset != 0) {
            if (SymFromAddr(process, stack.AddrPC.Offset, &callstack.OffsetFromSmybol, symbol) != 0) {
                strcpy_s(callstack.Name, symbol->Name);
                UnDecorateSymbolName(symbol->Name, callstack.UndName, STACKWALK_MAX_NAMELEN, UNDNAME_NAME_ONLY);
                UnDecorateSymbolName(symbol->Name, callstack.UndFullName, STACKWALK_MAX_NAMELEN, UNDNAME_COMPLETE);
            }

            if (SymGetLineFromAddr64(process, stack.AddrPC.Offset, &callstack.OffsetFromLine, &line) != 0) {
                callstack.LineNumber = line.LineNumber;
                strcpy_s(callstack.LineFileName, _str(line.FileName).extractFileName().c_str());
            }

            IMAGEHLP_MODULE64 module;
            std::memset(&module, 0, sizeof(IMAGEHLP_MODULE64));
            module.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
            if ((SymGetModuleInfo64(process, stack.AddrPC.Offset, &module) != 0) || ((module.SizeOfStruct = sizeof(imagehlp_module64_v2), SymGetModuleInfo64(process, stack.AddrPC.Offset, &module)) != 0)) {
                switch (module.SymType) {
                case SymNone:
                    callstack.SymTypeString = "-nosymbols-";
                    break;
                case SymCoff:
                    callstack.SymTypeString = "COFF";
                    break;
                case SymCv:
                    callstack.SymTypeString = "CV";
                    break;
                case SymPdb:
                    callstack.SymTypeString = "PDB";
                    break;
                case SymExport:
                    callstack.SymTypeString = "-exported-";
                    break;
                case SymDeferred:
                    callstack.SymTypeString = "-deferred-";
                    break;
                case SymSym:
                    callstack.SymTypeString = "SYM";
                    break;
#if API_VERSION_NUMBER >= 9
                case SymDia:
                    callstack.SymTypeString = "DIA";
                    break;
#endif
                case SymVirtual:
                    callstack.SymTypeString = "Virtual";
                    break;
                default:
                    callstack.SymTypeString = nullptr;
                    break;
                }

                strcpy_s(callstack.ModuleName, module.ModuleName);
                callstack.BaseOfImage = module.BaseOfImage;
                strcpy_s(callstack.LoadedImageName, module.LoadedImageName);
            }
        }

        Callstack::OnCallstackEntry(traceback, frame_num, callstack);
        if (stack.AddrReturn.Offset == 0) {
            break;
        }

        frame_num++;
    }

    CloseHandle(t);
    SymCleanup(process);
    CloseHandle(process);

    return traceback;
}

#elif !defined(FO_WINDOWS) && !defined(FO_ANDROID) && !defined(FO_WEB) && !defined(FO_IOS)
static void TerminationHandler(int signum, siginfo_t* siginfo, void* context);
static bool SigactionsSetted = false;
static struct sigaction OldSIGSEGV;
static struct sigaction OldSIGFPE;

static void DumpAngelScript(DiskFile& file);

void CatchExceptions(const string& app_name, int app_ver)
{
    Data->AppName = app_name;
    Data->AppVer = _str("{:#x}", app_ver);

    if (!app_name.empty() && !SigactionsSetted) {
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

        SigactionsSetted = true;
    }
    else if (app_name.empty() && SigactionsSetted) {
        sigaction(SIGSEGV, &OldSIGSEGV, nullptr);
        sigaction(SIGFPE, &OldSIGFPE, nullptr);

        SigactionsSetted = false;
    }
}

void CreateDump(const string& appendix, const string& message)
{
    Data->ManualDumpAppendix = appendix;
    Data->ManualDumpMessage = message;

    TerminationHandler(0, nullptr, nullptr);
}

static void TerminationHandler(int signum, siginfo_t* siginfo, void* context)
{
    string message;
    string traceback;

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
    if (siginfo) {
        message = strsignal(siginfo->si_signo);
        if (sig_desc)
            message += _str(" ({})", sig_desc);
    }
    else {
        message = Data->ManualDumpMessage;
    }

    // Obtain traceback
    backward::StackTrace st;
    st.load_here(42);
    backward::Printer st_printer;
    st_printer.snippet = false;
    std::stringstream ss;
    st_printer.print(st, ss);
    traceback = ss.str();

    // Dump file
    DateTimeStamp dt;
    Timer::GetCurrentDateTime(dt);
    string dump_str = (siginfo ? "CrashDump" : Data->ManualDumpAppendix);
    string dump_path = _str("{}_{}_{}_{:04}.{:02}.{:02}_{:02}-{:02}-{:02}.txt", dump_str, Data->AppName, Data->AppVer, dt.Year, dt.Month, dt.Day, dt.Hour, dt.Minute, dt.Second);

    DiskFileSystem::ResetCurDir();
    DiskFile file = DiskFileSystem::OpenFile(dump_path, true);
    if (file) {
        // Generic info
        file.Write(_str("Message\n"));
        file.Write(_str("\t{}\n", message));
        file.Write(_str("\n"));
        file.Write(_str("Application\n"));
        file.Write(_str("\tName        {}\n", Data->AppName));
        file.Write(_str("\tVersion     {}\n", Data->AppVer));
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

        // AngelScript dump
        DumpAngelScript(file);

        // Stacktrace
        st_printer.print(st);
        // st_printer.print(st, f); // Todo: restore stack trace dumping in file
    }

    if (siginfo) {
        MessageBox::ShowErrorMessage(message, traceback);
        exit(1);
    }
}

static string GetTraceback()
{
    backward::StackTrace st;
    st.load_here(42);
    backward::Printer st_printer;
    st_printer.snippet = false;
    std::stringstream ss;
    st_printer.print(st, ss);
    return ss.str();
}

#else
// Todo: improve global exceptions handlers for mobile os

void CatchExceptions(const string& app_name, int app_ver)
{
    //
}

void CreateDump(const string& appendix, const string& message)
{
    //
}

static string GetTraceback()
{
    return "No traceback!";
}

#endif

auto GetStackTrace() -> string
{
#ifdef FO_WINDOWS
    const string most_recent = "most recent call first";
#else
    string most_recent = "most recent call last";
#endif
    const auto traceback = GetTraceback();
    return _str("\n\nTraceback ({}):\n{}", most_recent, traceback);
}

static void DumpAngelScript(DiskFile& file)
{
    // Todo: fix script system
    // string tb = Script::GetTraceback();
    // if (!tb.empty())
    //    file.Write(_str("AngelScript\n{}", tb));
}

auto RaiseAssert(const string& message, const string& file, int line) -> bool
{
    // Break into debugger
#if defined(FO_WINDOWS)
    if (::IsDebuggerPresent() != 0) {
        ::DebugBreak();
        return true;
    }
#endif

    const string name = _str(file).extractFileName();
    WriteLog("Runtime assert: {} in {} ({})\n", message, name, line);

#if defined(FO_WINDOWS) || defined(FO_LINUX) || defined(FO_MAC)
    // Create dump
    CreateDump(_str("AssertFailed_{:#x}_{}({})", FO_VERSION, name, line), message);

    // Show message
    string traceback;
#if defined(FO_LINUX) || defined(FO_MAC)
    backward::StackTrace st;
    st.load_here(42);
    backward::Printer st_printer;
    st_printer.snippet = false;
    std::stringstream ss;
    st_printer.print(st, ss);
    traceback = ss.str();
#else
    traceback = "";
#endif

    MessageBox::ShowErrorMessage(_str("Assert failed!\nVersion: {:#x}\nFile: {} ({})\n\n{}", FO_VERSION, name, line, message), traceback);
#endif

    // Shut down
    std::exit(1);
}

void ReportExceptionAndExit(const std::exception& ex)
{
    WriteLog("{}\n", ex.what());
    std::exit(1);
}

void ReportExceptionAndContinue(const std::exception& ex)
{
    WriteLog("{}\n", ex.what());
}
