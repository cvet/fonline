#include "StdAfx.h"
#include "Exception.h"

char DumpMessRus[] =
{
    "Пожалуйста вышлите этот файл '%s' на e-mail 'support@fonline.ru'."
    "Тема письма должна содержать слово 'dump'. В письме укажите при "
    "каких обстоятельствах произошел сбой. Спасибо."
};

char DumpMessEng[] =
{
    "Please send this file '%s' on e-mail 'support@fonline.ru'."
    "The theme of the letter should contain a word 'dump'. In the letter specify "
    "under what circumstances there was a failure. Thanks."
};

char* DumpMess = DumpMessEng;

void SetExceptionsRussianText()
{
    DumpMess = DumpMessRus;
}

char AppName[ 128 ] = { 0 };
char AppVer[ 128 ] = { 0 };
char ManualDumpAppendix[ 128 ] = { 0 };

#ifdef FO_WINDOWS

# include <windows.h>
# include <stdio.h>
# include <DbgHelp.h>
# pragma comment(lib, "Dbghelp.lib")
# include <Psapi.h>
# pragma comment(lib, "Psapi.lib")
# include <tlhelp32.h>
# include "Timer.h"
# include "FileManager.h"

LONG WINAPI TopLevelFilterReadableDump( EXCEPTION_POINTERS* except );
LONG WINAPI TopLevelFilterMiniDump( EXCEPTION_POINTERS* except );

// Old version of the structure, used before Vista
typedef struct _IMAGEHLP_MODULE64_V2
{
    DWORD    SizeOfStruct;           // set to sizeof(IMAGEHLP_MODULE64)
    DWORD64  BaseOfImage;            // base load address of module
    DWORD    ImageSize;              // virtual size of the loaded module
    DWORD    TimeDateStamp;          // date/time stamp from pe header
    DWORD    CheckSum;               // checksum from the pe header
    DWORD    NumSyms;                // number of symbols in the symbol table
    SYM_TYPE SymType;                // type of symbols loaded
    CHAR     ModuleName[ 32 ];       // module name
    CHAR     ImageName[ 256 ];       // image name
    CHAR     LoadedImageName[ 256 ]; // symbol file name
} IMAGEHLP_MODULE64_V2;

void CatchExceptions( const char* app_name, unsigned int app_ver )
{
    if( app_name )
        Str::Copy( AppName, app_name );
    Str::Format( AppVer, "%04X", app_ver );

    if( app_name )
    # ifndef EXCEPTION_MINIDUMP
        SetUnhandledExceptionFilter( TopLevelFilterReadableDump );
    # else
        SetUnhandledExceptionFilter( TopLevelFilterMiniDump );
    # endif
    else
        SetUnhandledExceptionFilter( NULL );
}

# ifdef FO_X86
#  pragma warning( disable : 4748 )
# endif
LONG WINAPI TopLevelFilterReadableDump( EXCEPTION_POINTERS* except )
{
    LONG        retval = EXCEPTION_CONTINUE_SEARCH;
    char        mess[ MAX_FOTEXT ];
    char        dump_path[ MAX_FOPATH ];
    char        dump_path_dir[ MAX_FOPATH ];

    DateTime    dt;
    Timer::GetCurrentDateTime( dt );
    const char* dump_str = except ? "CrashDump" : ManualDumpAppendix;
    # ifdef FONLINE_SERVER
    FileManager::GetFullPath( NULL, PT_SERVER_DUMPS, dump_path_dir );
    # else
    FileManager::GetFullPath( NULL, PT_ROOT, dump_path_dir );
    # endif
    Str::Format( dump_path, "%s%s_%s_%s_%04d.%02d.%02d_%02d-%02d-%02d.txt",
                 dump_path_dir, dump_str, AppName, AppVer, dt.Year, dt.Month, dt.Day, dt.Hour, dt.Minute, dt.Second );

    FILE* f = fopen( dump_path, "wt" );
    if( f )
    {
        // Generic info
        fprintf( f, "Application\n" );
        fprintf( f, "\tName        %s\n", AppName );
        fprintf( f, "\tVersion     %s\n",  AppVer );
        OSVERSIONINFOA ver;
        memset( &ver, 0, sizeof( OSVERSIONINFOA ) );
        ver.dwOSVersionInfoSize = sizeof( ver );
        if( GetVersionEx( (OSVERSIONINFOA*) &ver ) )
        {
            fprintf( f, "\tOS          %d.%d.%d (%s)\n",
                     ver.dwMajorVersion, ver.dwMinorVersion, ver.dwBuildNumber, ver.szCSDVersion );
        }
        fprintf( f, "\tTimestamp   %04d.%02d.%02d %02d:%02d:%02d\n", dt.Year, dt.Month, dt.Day, dt.Hour, dt.Minute, dt.Second );
        fprintf( f, "\n" );

        // Exception information
        if( except )
        {
            fprintf( f, "Exception\n" );
            fprintf( f, "\tCode      " );
            switch( except->ExceptionRecord->ExceptionCode )
            {
                # define CASE_EXCEPTION( e ) \
                case e:                      \
                    fprintf( f, # e ); break
                CASE_EXCEPTION( EXCEPTION_ACCESS_VIOLATION );
                CASE_EXCEPTION( EXCEPTION_DATATYPE_MISALIGNMENT );
                CASE_EXCEPTION( EXCEPTION_BREAKPOINT );
                CASE_EXCEPTION( EXCEPTION_SINGLE_STEP );
                CASE_EXCEPTION( EXCEPTION_ARRAY_BOUNDS_EXCEEDED );
                CASE_EXCEPTION( EXCEPTION_FLT_DENORMAL_OPERAND );
                CASE_EXCEPTION( EXCEPTION_FLT_DIVIDE_BY_ZERO );
                CASE_EXCEPTION( EXCEPTION_FLT_INEXACT_RESULT );
                CASE_EXCEPTION( EXCEPTION_FLT_INVALID_OPERATION );
                CASE_EXCEPTION( EXCEPTION_FLT_OVERFLOW );
                CASE_EXCEPTION( EXCEPTION_FLT_STACK_CHECK );
                CASE_EXCEPTION( EXCEPTION_FLT_UNDERFLOW );
                CASE_EXCEPTION( EXCEPTION_INT_DIVIDE_BY_ZERO );
                CASE_EXCEPTION( EXCEPTION_INT_OVERFLOW );
                CASE_EXCEPTION( EXCEPTION_PRIV_INSTRUCTION );
                CASE_EXCEPTION( EXCEPTION_IN_PAGE_ERROR );
                CASE_EXCEPTION( EXCEPTION_ILLEGAL_INSTRUCTION );
                CASE_EXCEPTION( EXCEPTION_NONCONTINUABLE_EXCEPTION );
                CASE_EXCEPTION( EXCEPTION_STACK_OVERFLOW );
                CASE_EXCEPTION( EXCEPTION_INVALID_DISPOSITION );
                CASE_EXCEPTION( EXCEPTION_GUARD_PAGE );
                CASE_EXCEPTION( EXCEPTION_INVALID_HANDLE );
            default:
                fprintf( f, "0x%0X", except->ExceptionRecord->ExceptionCode );
                break;
            }
            fprintf( f, "\n" );
            fprintf( f, "\tAddress   0x%p\n", except->ExceptionRecord->ExceptionAddress );
            fprintf( f, "\tFlags     0x%0X\n", except->ExceptionRecord->ExceptionFlags );
            fprintf( f, "\n" );
        }

        // Collect current threads
        HANDLE process = GetCurrentProcess();
        DWORD  threads_ids[ 1024 ] = { GetCurrentThreadId() };
        uint   threads_ids_count = 1;
        HANDLE snapshot = CreateToolhelp32Snapshot( TH32CS_SNAPTHREAD, 0 );
        if( snapshot != INVALID_HANDLE_VALUE )
        {
            THREADENTRY32 te;
            te.dwSize = sizeof( te );
            if( Thread32First( snapshot, &te ) )
            {
                while( true )
                {
                    if( te.dwSize >= FIELD_OFFSET( THREADENTRY32, th32OwnerProcessID ) + sizeof( te.th32OwnerProcessID ) )
                    {
                        if( te.th32OwnerProcessID == GetCurrentProcessId() && te.th32ThreadID != threads_ids[ 0 ] )
                            threads_ids[ threads_ids_count++ ] = te.th32ThreadID;
                    }
                    te.dwSize = sizeof( te );
                    if( !Thread32Next( snapshot, &te ) )
                        break;
                }
            }
            CloseHandle( snapshot );
        }
        else
        {
            fprintf( f, "CreateToolhelp32Snapshot fail\n" );
        }

        // Init symbols
        SymInitialize( process, NULL, TRUE );
        SymSetOptions( SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS );

        // Print information about each thread
        for( uint i = 0; i < threads_ids_count; i++ )
        {
            DWORD       tid = threads_ids[ i ];
            HANDLE      t = OpenThread( THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION | THREAD_GET_CONTEXT, FALSE, tid );
            const char* tname = Thread::FindName( tid );
            fprintf( f, "Thread '%s' (%u%s)\n", tname ? tname : "Unknown", tid, !i ? ", current" : "" );

            CONTEXT context;
            memset( &context, 0, sizeof( context ) );
            context.ContextFlags = CONTEXT_FULL;

            if( tid == GetCurrentThreadId() )
            {
                if( except )
                {
                    memcpy( &context, except->ContextRecord, sizeof( CONTEXT ) );
                }
                else
                {
                    # ifdef FO_X86
                    __asm     label :
                    __asm mov[ context.Ebp ], ebp;
                    __asm     mov[ context.Esp ], esp;
                    __asm mov eax, [ label ];
                    __asm     mov[ context.Eip ], eax;
                    # else // FO_X64
                    RtlCaptureContext( &context );
                    # endif
                }
            }
            else
            {
                SuspendThread( t );
                GetThreadContext( t, &context );
                ResumeThread( t );
            }

            STACKFRAME64 stack;
            memset( &stack, 0, sizeof( stack ) );

            # ifdef FO_X86
            DWORD machine_type = IMAGE_FILE_MACHINE_I386;
            stack.AddrFrame.Mode   = AddrModeFlat;
            stack.AddrFrame.Offset = context.Ebp;
            stack.AddrPC.Mode      = AddrModeFlat;
            stack.AddrPC.Offset    = context.Eip;
            stack.AddrStack.Mode   = AddrModeFlat;
            stack.AddrStack.Offset = context.Esp;
            # else // FO_X64
            DWORD machine_type = IMAGE_FILE_MACHINE_AMD64;
            stack.AddrPC.Offset = context.Rip;
            stack.AddrPC.Mode = AddrModeFlat;
            stack.AddrFrame.Offset = context.Rsp;
            stack.AddrFrame.Mode = AddrModeFlat;
            stack.AddrStack.Offset = context.Rsp;
            stack.AddrStack.Mode = AddrModeFlat;
            # endif

            # define STACKWALK_MAX_NAMELEN    ( 1024 )
            char symbol_buffer[ sizeof( SYMBOL_INFO ) + STACKWALK_MAX_NAMELEN ];
            SYMBOL_INFO* symbol = (SYMBOL_INFO*) symbol_buffer;
            memset( symbol, 0, sizeof( SYMBOL_INFO ) + STACKWALK_MAX_NAMELEN );
            symbol->SizeOfStruct = sizeof( SYMBOL_INFO );
            symbol->MaxNameLen = STACKWALK_MAX_NAMELEN;

            struct RPM
            {
                static BOOL __stdcall Call( HANDLE hProcess, DWORD64 qwBaseAddress, PVOID lpBuffer, DWORD nSize, LPDWORD lpNumberOfBytesRead )
                {
                    SIZE_T st;
                    BOOL   bRet = ReadProcessMemory( hProcess, (LPVOID) qwBaseAddress, lpBuffer, nSize, &st );
                    *      lpNumberOfBytesRead = (DWORD) st;
                    return bRet;
                }
            };

            int frame_num = 0;
            while( StackWalk64( machine_type, process, t, &stack, &context, RPM::Call, SymFunctionTableAccess64, SymGetModuleBase64, NULL ) )
            {
                struct CSE
                {
                    struct CallstackEntry
                    {
                        DWORD64 offset;
                        CHAR    name[ STACKWALK_MAX_NAMELEN ];
                        CHAR    undName[ STACKWALK_MAX_NAMELEN ];
                        CHAR    undFullName[ STACKWALK_MAX_NAMELEN ];
                        DWORD64 offsetFromSmybol;
                        DWORD   offsetFromLine;
                        DWORD   lineNumber;
                        CHAR    lineFileName[ STACKWALK_MAX_NAMELEN ];
                        DWORD   symType;
                        LPCSTR  symTypeString;
                        CHAR    moduleName[ STACKWALK_MAX_NAMELEN ];
                        DWORD64 baseOfImage;
                        CHAR    loadedImageName[ STACKWALK_MAX_NAMELEN ];
                    };

                    static void OnCallstackEntry( FILE* f, int frame_num, CallstackEntry& entry )
                    {
                        if( frame_num >= 0 && entry.offset != 0 )
                        {
                            if( entry.name[ 0 ] == 0 )
                                strcpy_s( entry.name, "(function-name not available)" );
                            if( entry.undName[ 0 ] != 0 )
                                strcpy_s( entry.name, entry.undName );
                            if( entry.undFullName[ 0 ] != 0 )
                                strcpy_s( entry.name, entry.undFullName );
                            if( entry.moduleName[ 0 ] == 0 )
                                strcpy_s( entry.moduleName, "???" );

                            fprintf( f, "\t%s, %s + %d", entry.moduleName, entry.name, entry.offsetFromSmybol );
                            if( entry.lineFileName[ 0 ] != 0 )
                                fprintf( f, ", %s (%d)\n", entry.lineFileName, entry.lineNumber );
                            else
                                fprintf( f, "\n" );
                        }
                    }
                };

                CSE::CallstackEntry callstack;
                memzero( &callstack, sizeof( callstack ) );
                callstack.offset = stack.AddrPC.Offset;

                IMAGEHLP_LINE64 line;
                memset( &line, 0, sizeof( line ) );
                line.SizeOfStruct = sizeof( line );

                if( stack.AddrPC.Offset == stack.AddrReturn.Offset )
                {
                    fprintf( f, "\tEndless callstack!\n", 0, stack.AddrPC.Offset );
                    break;
                }

                if( stack.AddrPC.Offset != 0 )
                {
                    if( SymFromAddr( process, stack.AddrPC.Offset, &callstack.offsetFromSmybol, symbol ) )
                    {
                        strcpy_s( callstack.name, symbol->Name );
                        UnDecorateSymbolName( symbol->Name, callstack.undName, STACKWALK_MAX_NAMELEN, UNDNAME_NAME_ONLY );
                        UnDecorateSymbolName( symbol->Name, callstack.undFullName, STACKWALK_MAX_NAMELEN, UNDNAME_COMPLETE );
                    }

                    if( SymGetLineFromAddr64( process, stack.AddrPC.Offset, &callstack.offsetFromLine, &line ) )
                    {
                        callstack.lineNumber = line.LineNumber;
                        strcpy_s( callstack.lineFileName, line.FileName );
                        FileManager::ExtractFileName( callstack.lineFileName, callstack.lineFileName );
                    }

                    IMAGEHLP_MODULE64 module;
                    memset( &module, 0, sizeof( IMAGEHLP_MODULE64 ) );
                    module.SizeOfStruct = sizeof( IMAGEHLP_MODULE64 );
                    if( SymGetModuleInfo64( process, stack.AddrPC.Offset, &module ) ||
                        ( module.SizeOfStruct = sizeof( IMAGEHLP_MODULE64_V2 ),
                          SymGetModuleInfo64( process, stack.AddrPC.Offset, &module ) ) )
                    {
                        switch( module.SymType )
                        {
                        case SymNone:
                            callstack.symTypeString = "-nosymbols-";
                            break;
                        case SymCoff:
                            callstack.symTypeString = "COFF";
                            break;
                        case SymCv:
                            callstack.symTypeString = "CV";
                            break;
                        case SymPdb:
                            callstack.symTypeString = "PDB";
                            break;
                        case SymExport:
                            callstack.symTypeString = "-exported-";
                            break;
                        case SymDeferred:
                            callstack.symTypeString = "-deferred-";
                            break;
                        case SymSym:
                            callstack.symTypeString = "SYM";
                            break;
                            # if API_VERSION_NUMBER >= 9
                        case SymDia:
                            callstack.symTypeString = "DIA";
                            break;
                            # endif
                        case SymVirtual:
                            callstack.symTypeString = "Virtual";
                            break;
                        default:
                            callstack.symTypeString = NULL;
                            break;
                        }

                        strcpy_s( callstack.moduleName, module.ModuleName );
                        callstack.baseOfImage = module.BaseOfImage;
                        strcpy_s( callstack.loadedImageName, module.LoadedImageName );
                    }
                }

                CSE::OnCallstackEntry( f, frame_num, callstack );
                if( stack.AddrReturn.Offset == 0 )
                    break;

                frame_num++;
            }

            fprintf( f, "\n" );
            CloseHandle( t );
        }

        // Print modules
        HMODULE modules[ 1024 ];
        DWORD   needed;
        fprintf( f, "Loaded modules\n" );
        if( EnumProcessModules( process, modules, sizeof( modules ), &needed ) )
        {
            for( int i = 0; i < (int) ( needed / sizeof( HMODULE ) ); i++ )
            {
                char module_name[ MAX_PATH ] = { 0 };
                if( GetModuleFileNameEx( process, modules[ i ], module_name, sizeof( module_name ) ) )
                    fprintf( f, "\t%s (%p)\n", module_name, modules[ i ] );
                else
                    fprintf( f, "\tGetModuleFileNameEx fail\n" );
            }
        }

        SymCleanup( process );
        CloseHandle( process );
        fclose( f );

        Str::Format( mess, DumpMess, dump_path );
    }
    else
    {
        Str::Format( mess, "Error while create dump file - Error create file, path<%s>, err<%d>.", dump_path, GetLastError() );
    }

    if( except )
        ShowMessage( mess );

    return EXCEPTION_EXECUTE_HANDLER;
}

LONG WINAPI TopLevelFilterMiniDump( EXCEPTION_POINTERS* except )
{
    LONG       retval = EXCEPTION_CONTINUE_SEARCH;
    char       mess[ MAX_FOTEXT ];
    char       dump_path[ MAX_FOPATH ];
    char       dump_path_dir[ MAX_FOPATH ];

    DateTime    dt;
    Timer::GetCurrentDateTime( dt );
    const char* dump_str = except ? "CrashDump" : ManualDumpAppendix;
    # ifdef FONLINE_SERVER
    FileManager::GetFullPath( NULL, PT_SERVER_DUMPS, dump_path_dir );
    # else
    FileManager::GetFullPath( NULL, PT_ROOT, dump_path_dir );
    # endif
    Str::Format( dump_path, "%s%s_%s_%s_%04d.%02d.%02d_%02d-%02d-%02d.txt",
                 dump_path_dir, dump_str, AppName, AppVer, dt.Year, dt.Month, dt.Day, dt.Hour, dt.Minute, dt.Second );

    HANDLE f = CreateFile( dump_path, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
    if( f != INVALID_HANDLE_VALUE )
    {
        MINIDUMP_EXCEPTION_INFORMATION ex_info;
        ex_info.ThreadId = GetCurrentThreadId();
        ex_info.ExceptionPointers = except;
        ex_info.ClientPointers = FALSE;

        if( MiniDumpWriteDump( GetCurrentProcess(), GetCurrentProcessId(), f, MiniDumpNormal, except ? &ex_info : NULL, NULL, NULL ) )
        {
            Str::Format( mess, DumpMess, dump_path );
            retval = EXCEPTION_EXECUTE_HANDLER;
        }
        else
        {
            Str::Format( mess, "Error while create dump file - File save error, path<%s>, err<%d>.", dump_path, GetLastError() );
        }

        CloseHandle( f );
    }
    else
    {
        Str::Format( mess, "Error while create dump file - Error create file, path<%s>, err<%d>.", dump_path, GetLastError() );
    }

    if( except )
        ShowMessage( mess );

    return retval;
}

void CreateDump( const char* appendix )
{
    Str::Copy( ManualDumpAppendix, appendix );

    # ifndef EXCEPTION_MINIDUMP
    TopLevelFilterReadableDump( NULL );
    # else
    TopLevelFilterMiniDump( NULL );
    # endif
}

#else

# include "FileManager.h"
# include <signal.h>
# include <execinfo.h>
# include <cxxabi.h>
# include <sys/utsname.h>
# include <string.h>

# define BACKTRACE_BUFFSER_COUNT    ( 100 )

void TerminationHandler( int signum, siginfo_t* siginfo, void* context );
bool sigactionsSetted = false;
struct sigaction oldSIGSEGV;
struct sigaction oldSIGFPE;

void CatchExceptions( const char* app_name, unsigned int app_ver )
{
    if( app_name )
        Str::Copy( AppName, app_name );
    Str::Format( AppVer, "%04X", app_ver );

    if( app_name && !sigactionsSetted )
    {
        // SIGILL, SIGFPE, SIGSEGV, SIGBUS, SIGTERM
        // CTRL-C - sends SIGINT which default action is to terminate the application.
        // CTRL-\ - sends SIGQUIT which default action is to terminate the application dumping core.
        // CTRL-Z - sends SIGSTOP that suspends the program.

        struct sigaction act;

        /*
           SIGSEGV
           Description     Invalid memory reference
           Default action  Abnormal termination of the process
         */
        memset( &act, 0, sizeof( act ) );
        act.sa_sigaction = &TerminationHandler;
        act.sa_flags = SA_SIGINFO;
        sigaction( SIGSEGV, &act, &oldSIGSEGV );

        /*
           SIGFPE
           Description     Erroneous arithmetic operation
           Default action  bnormal termination of the process
         */
        memset( &act, 0, sizeof( act ) );
        act.sa_sigaction = &TerminationHandler;
        act.sa_flags = SA_SIGINFO;
        sigaction( SIGFPE, &act, &oldSIGFPE );

        sigactionsSetted = true;
    }
    else if( !app_name && sigactionsSetted )
    {
        sigaction( SIGSEGV, &oldSIGSEGV, NULL );
        sigaction( SIGFPE, &oldSIGFPE, NULL );

        sigactionsSetted = false;
    }
}

void CreateDump( const char* appendix )
{
    Str::Copy( ManualDumpAppendix, appendix );
    TerminationHandler( 0, NULL, NULL );
}

void TerminationHandler( int signum, siginfo_t* siginfo, void* context )
{
    char        mess[ MAX_FOTEXT ];
    char        dump_path[ MAX_FOPATH ];
    char        dump_path_dir[ MAX_FOPATH ];

    DateTime    dt;
    Timer::GetCurrentDateTime( dt );
    const char* dump_str = siginfo ? "CrashDump" : ManualDumpAppendix;
    # ifdef FONLINE_SERVER
    FileManager::GetFullPath( NULL, PT_SERVER_DUMPS, dump_path_dir );
    # else
    FileManager::GetFullPath( NULL, PT_ROOT, dump_path_dir );
    # endif
    Str::Format( dump_path, "%s%s_%s_%s_%04d.%02d.%02d_%02d-%02d-%02d.txt",
                 dump_path_dir, dump_str, AppName, AppVer, dt.Year, dt.Month, dt.Day, dt.Hour, dt.Minute, dt.Second );

    FILE* f = fopen( dump_path, "wt" );
    if( f )
    {
        // Generic info
        fprintf( f, "Application\n" );
        fprintf( f, "\tName        %s\n", AppName );
        fprintf( f, "\tVersion     %s\n",  AppVer );
        struct utsname ver;
        uname( &ver );
        fprintf( f, "\tOS          %s / %s / %s\n", ver.sysname, ver.release, ver.version );
        fprintf( f, "\tTimestamp   %02d.%02d.%04d %02d:%02d:%02d\n", dt.Day, dt.Month, dt.Year, dt.Hour, dt.Minute, dt.Second );
        fprintf( f, "\n" );

        // Exception information
        if( siginfo )
        {
            const char* str = NULL;
            static const char* str_SIGSEGV[] =
            {
                "Address not mapped to object, SEGV_MAPERR",
                "Invalid permissions for mapped object, SEGV_ACCERR",
            };
            if( siginfo->si_signo == SIGSEGV && siginfo->si_code >= 0 && siginfo->si_code < 2 )
                str = str_SIGSEGV[ siginfo->si_code ];
            static const char* str_SIGFPE[] =
            {
                "Integer divide by zero, FPE_INTDIV",
                "Integer overflow, FPE_INTOVF",
                "Floating-point divide by zero, FPE_FLTDIV",
                "Floating-point overflow, FPE_FLTOVF",
                "Floating-point underflow, FPE_FLTUND",
                "Floating-point inexact result, FPE_FLTRES",
                "Invalid floating-point operation, FPE_FLTINV",
                "Subscript out of range, FPE_FLTSUB",
            };
            if( siginfo->si_signo == SIGFPE && siginfo->si_code >= 0 && siginfo->si_code < 8 )
                str = str_SIGFPE[ siginfo->si_code ];

            fprintf( f, "Exception\n" );
            fprintf( f, "\tSigno   %s (%d)\n", strsignal( siginfo->si_signo ), siginfo->si_signo );
            fprintf( f, "\tCode    %s (%d)\n", str ? str : "No description", siginfo->si_code );
            fprintf( f, "\tErrno   %s (%d)\n", strerror( siginfo->si_errno ), siginfo->si_errno );
            fprintf( f, "\n" );
        }

        // Stack
        void* array[ BACKTRACE_BUFFSER_COUNT ];
        int size = backtrace( array, BACKTRACE_BUFFSER_COUNT );
        char** symbols = backtrace_symbols( array, size );

        fprintf( f, "Thread '%s' (%u%s)\n", Thread::GetCurrentName(), Thread::GetCurrentId(), ", current" );
        for( int i = 0; i < size; i++ )
            fprintf( f, "\t%s\n", symbols[ i ] );

        free( symbols );
        fclose( f );

        Str::Format( mess, DumpMess, dump_path );
    }
    else
    {
        Str::Format( mess, "Error while create dump file - Error create file, path<%s>.", dump_path );
    }

    // if( siginfo )
    //    MessageBox( NULL, mess, "FOnline Error", MB_OK );

    if( siginfo )
        ExitProcess( 1 );
}

#endif
