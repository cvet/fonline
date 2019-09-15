#ifdef FO_TESTING
# include "SDL_main.h"
# define CATCH_CONFIG_RUNNER
#endif

#include "Testing.h"
#include "Log.h"
#include "Script.h"
#include "Threading.h"
#include "StringUtils.h"
#include "Timer.h"
#include "Version_Include.h"

static string AppName;
static string AppVer;
static string ManualDumpAppendix;
static string ManualDumpMessage;

#ifdef FO_TESTING
extern "C" int main( int argc, char** argv ) // Handled by SDL
{
    return Catch::Session().run( argc, argv );
}
#endif

#if defined ( FO_WINDOWS )

# define WIN32_LEAN_AND_MEAN
# include <Windows.h>
# include <stdio.h>
# pragma warning( disable : 4091 )
# pragma warning( disable : 4996 )
# include <DbgHelp.h>
# pragma comment(lib, "Dbghelp.lib")
# include <Psapi.h>
# pragma comment(lib, "Psapi.lib")
# include <tlhelp32.h>
# include "Timer.h"
# include "FileUtils.h"

static LONG WINAPI TopLevelFilterReadableDump( EXCEPTION_POINTERS* except );

static void DumpAngelScript( FILE* f );

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

void CatchExceptions( const string& app_name, int64 app_ver )
{
    AppName = app_name;
    AppVer = _str( "{:#x}", app_ver );

    if( !app_name.empty() )
        SetUnhandledExceptionFilter( TopLevelFilterReadableDump );
    else
        SetUnhandledExceptionFilter( nullptr );
}

# ifdef FO_X86
#  pragma warning( disable : 4748 )
# endif
static LONG WINAPI TopLevelFilterReadableDump( EXCEPTION_POINTERS* except )
{
    LONG   retval = EXCEPTION_CONTINUE_SEARCH;
    string message;
    string traceback;

    File::ResetCurrentDir();
    DateTimeStamp dt;
    Timer::GetCurrentDateTime( dt );
    string        dump_str = except ? "CrashDump" : ManualDumpAppendix;
    string        dump_path = _str( "{}_{}_{}_{:04}.{:02}.{:02}_{:02}-{:02}-{:02}.txt",
                                    dump_str, AppName, AppVer, dt.Year, dt.Month, dt.Day, dt.Hour, dt.Minute, dt.Second );

    File::CreateDirectoryTree( dump_path );
    FILE* f = fopen( dump_path.c_str(), "wt" );
    if( f )
    {
        if( !except )
            message = ManualDumpMessage;
        else
            message = "Exception";

        // Generic info
        fprintf( f, "Message\n" );
        fprintf( f, "%s\n", message.c_str() );
        fprintf( f, "\n" );
        fprintf( f, "Application\n" );
        fprintf( f, "\tName        %s\n", AppName.c_str() );
        fprintf( f, "\tVersion     %s\n",  AppVer.c_str() );
        OSVERSIONINFOW ver;
        memset( &ver, 0, sizeof( OSVERSIONINFOW ) );
        ver.dwOSVersionInfoSize = sizeof( ver );
        if( GetVersionExW( (OSVERSIONINFOW*) &ver ) )
        {
            fprintf( f, "\tOS          %d.%d.%d (%s)\n",
                     ver.dwMajorVersion, ver.dwMinorVersion, ver.dwBuildNumber, _str().parseWideChar( ver.szCSDVersion ).c_str() );
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
                    fprintf( f, # e ); message = # e; break
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
                # undef CASE_EXCEPTION
            case 0xE06D7363:
                fprintf( f, "Unhandled C++ Exception" );
                message = "Unhandled C++ Exception";
                break;
            default:
                fprintf( f, "0x%0X", except->ExceptionRecord->ExceptionCode );
                message = _str( "Unknown Exception (code {})", except->ExceptionRecord->ExceptionCode );
                break;
            }
            fprintf( f, "\n" );
            fprintf( f, "\tAddress   0x%p\n", except->ExceptionRecord->ExceptionAddress );
            fprintf( f, "\tFlags     0x%0X\n", except->ExceptionRecord->ExceptionFlags );
            if( except->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION || except->ExceptionRecord->ExceptionCode == EXCEPTION_IN_PAGE_ERROR )
            {
                int readWrite = (int) except->ExceptionRecord->ExceptionInformation[ 0 ];
                if( readWrite == 0 )
                    fprintf( f, "\tInfo      Attempted to read to an 0x%p", (void*) except->ExceptionRecord->ExceptionInformation[ 1 ] );
                else if( readWrite == 1 )
                    fprintf( f, "\tInfo      Attempted to write to an 0x%p", (void*) except->ExceptionRecord->ExceptionInformation[ 1 ] );
                else if( readWrite == 8 )
                    fprintf( f, "\tInfo      Data execution prevention to an 0x%p", (void*) except->ExceptionRecord->ExceptionInformation[ 1 ] );
                if( except->ExceptionRecord->ExceptionCode == EXCEPTION_IN_PAGE_ERROR )
                    fprintf( f, ", NTSTATUS %p", (void*) except->ExceptionRecord->ExceptionInformation[ 2 ] );
                fprintf( f, "\n" );
            }
            else
            {
                for( DWORD i = 0; i < except->ExceptionRecord->NumberParameters; i++ )
                    fprintf( f, "\tInfo %u    0x%p\n", i, (void*) except->ExceptionRecord->ExceptionInformation[ i ] );
            }
            fprintf( f, "\n" );
        }

        // AngelScript dump
        DumpAngelScript( f );

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

        // Set exe dir for PDB symbols
        SetCurrentDirectoryW( _str( File::GetExePath() ).extractDir().toWideChar().c_str() );

        // Init symbols
        SymInitialize( process, nullptr, TRUE );
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
                SuspendThread( t ); // -V720
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
            while( StackWalk64( machine_type, process, t, &stack, &context, RPM::Call, SymFunctionTableAccess64, SymGetModuleBase64, nullptr ) )
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

                    static void OnCallstackEntry( bool is_current_thread, string& traceback, FILE* f, int frame_num, CallstackEntry& entry )
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

                            fprintf( f, "\t%s, %s + %lld", entry.moduleName, entry.name, entry.offsetFromSmybol );
                            if( is_current_thread )
                                traceback += _str( "    {}, {}", entry.moduleName, entry.name );
                            if( entry.lineFileName[ 0 ] != 0 )
                            {
                                fprintf( f, ", %s (line %d)\n", entry.lineFileName, entry.lineNumber );
                                if( is_current_thread )
                                    traceback += _str( ", {} at line {}\n", entry.lineFileName, entry.lineNumber );
                            }
                            else
                            {
                                fprintf( f, "\n" );
                                if( is_current_thread )
                                    traceback += _str( "+ {}\n", entry.offsetFromSmybol );
                            }
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
                    fprintf( f, "\tEndless callstack!\n" );
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
                        strcpy_s( callstack.lineFileName, _str( line.FileName ).extractFileName().c_str() );
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
                            callstack.symTypeString = nullptr;
                            break;
                        }

                        strcpy_s( callstack.moduleName, module.ModuleName );
                        callstack.baseOfImage = module.BaseOfImage;
                        strcpy_s( callstack.loadedImageName, module.LoadedImageName );
                    }
                }

                CSE::OnCallstackEntry( i == 0, traceback, f, frame_num, callstack );
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
                wchar_t module_name[ MAX_PATH ] = { 0 };
                if( GetModuleFileNameExW( process, modules[ i ], module_name, sizeof( module_name ) ) )
                    fprintf( f, "\t%s (%p)\n", _str().parseWideChar( module_name ).c_str(), modules[ i ] );
                else
                    fprintf( f, "\tGetModuleFileNameExW fail\n" );
            }
        }

        SymCleanup( process );
        CloseHandle( process );

        fclose( f );
    }

    if( except )
        ShowErrorMessage( message, traceback );

    return EXCEPTION_EXECUTE_HANDLER;
}

void CreateDump( const string& appendix, const string& message )
{
    ManualDumpAppendix = appendix;
    ManualDumpMessage = message;

    TopLevelFilterReadableDump( nullptr );
}

#elif !defined ( FO_ANDROID ) && !defined ( FO_WEB ) && !defined ( FO_IOS )

# ifdef FO_LINUX
#  define BACKWARD_HAS_BFD    1
# endif
# include "backward.hpp"

# include "FileUtils.h"
# include <execinfo.h>
# include <sys/utsname.h>

static void TerminationHandler( int signum, siginfo_t* siginfo, void* context );
static bool SigactionsSetted = false;
static struct sigaction OldSIGSEGV;
static struct sigaction OldSIGFPE;

static void DumpAngelScript( FILE* f );

void CatchExceptions( const string& app_name, int64 app_ver )
{
    AppName = app_name;
    AppVer = _str( "{:#x}", app_ver );

    if( !app_name.empty() && !SigactionsSetted )
    {
        // SIGILL, SIGFPE, SIGSEGV, SIGBUS, SIGTERM
        // CTRL-C - sends SIGINT which default action is to terminate the application.
        // CTRL-\ - sends SIGQUIT which default action is to terminate the application dumping core.
        // CTRL-Z - sends SIGSTOP that suspends the program.

        struct sigaction act;

        // SIGSEGV
        // Description     Invalid memory reference
        // Default action  Abnormal termination of the process
        memset( &act, 0, sizeof( act ) );
        act.sa_sigaction = &TerminationHandler;
        act.sa_flags = SA_SIGINFO;
        sigaction( SIGSEGV, &act, &OldSIGSEGV );

        // SIGFPE
        // Description     Erroneous arithmetic operation
        // Default action  bnormal termination of the process
        memset( &act, 0, sizeof( act ) );
        act.sa_sigaction = &TerminationHandler;
        act.sa_flags = SA_SIGINFO;
        sigaction( SIGFPE, &act, &OldSIGFPE );

        SigactionsSetted = true;
    }
    else if( app_name.empty() && SigactionsSetted )
    {
        sigaction( SIGSEGV, &OldSIGSEGV, nullptr );
        sigaction( SIGFPE, &OldSIGFPE, nullptr );

        SigactionsSetted = false;
    }
}

void CreateDump( const string& appendix, const string& message )
{
    ManualDumpAppendix = appendix;
    ManualDumpMessage = message;

    TerminationHandler( 0, nullptr, nullptr );
}

static void TerminationHandler( int signum, siginfo_t* siginfo, void* context )
{
    string message;
    string traceback;

    // Additional description
    static const char* str_SIGSEGV[] =
    {
        "Address not mapped to object",          // SEGV_MAPERR
        "Invalid permissions for mapped object", // SEGV_ACCERR
    };
    static const char* str_SIGFPE[] =
    {
        "Integer divide by zero",           // FPE_INTDIV
        "Integer overflow",                 // FPE_INTOVF
        "Floating-point divide by zero",    // FPE_FLTDIV
        "Floating-point overflow",          // FPE_FLTOVF
        "Floating-point underflow",         // FPE_FLTUND
        "Floating-point inexact result",    // FPE_FLTRES
        "Invalid floating-point operation", // FPE_FLTINV
        "Subscript out of range",           // FPE_FLTSUB
    };

    const char* sig_desc = nullptr;
    if( siginfo )
    {
        if( siginfo->si_signo == SIGSEGV && siginfo->si_code >= 0 && siginfo->si_code < 2 )
            sig_desc = str_SIGSEGV[ siginfo->si_code ];
        if( siginfo->si_signo == SIGFPE && siginfo->si_code >= 0 && siginfo->si_code < 8 )
            sig_desc = str_SIGFPE[ siginfo->si_code ];
    }

    // Format message
    if( siginfo )
    {
        message = strsignal( siginfo->si_signo );
        if( sig_desc )
            message += _str( " ({})", sig_desc );
    }
    else
    {
        message = ManualDumpMessage;
    }

    // Obtain traceback
    backward::StackTrace st;
    st.load_here( 42 );
    backward::Printer st_printer;
    st_printer.snippet = false;
    std::stringstream ss;
    st_printer.print( st, ss );
    traceback = ss.str();

    // Dump file
    File::ResetCurrentDir();
    DateTimeStamp    dt;
    Timer::GetCurrentDateTime( dt );
    string dump_str = siginfo ? "CrashDump" : ManualDumpAppendix;
    string dump_path = _str( "{}_{}_{}_{:04}.{:02}.{:02}_{:02}-{:02}-{:02}.txt",
                             dump_str, AppName, AppVer, dt.Year, dt.Month, dt.Day, dt.Hour, dt.Minute, dt.Second );

    File::CreateDirectoryTree( dump_path );
    FILE* f = fopen( dump_path.c_str(), "wt" );
    if( f )
    {
        // Generic info
        fprintf( f, "Message\n" );
        fprintf( f, "\t%s\n", message.c_str() );
        fprintf( f, "\n" );
        fprintf( f, "Application\n" );
        fprintf( f, "\tName        %s\n", AppName.c_str() );
        fprintf( f, "\tVersion     %s\n",  AppVer.c_str() );
        struct utsname ver;
        uname( &ver );
        fprintf( f, "\tOS          %s / %s / %s\n", ver.sysname, ver.release, ver.version );
        fprintf( f, "\tTimestamp   %02d.%02d.%04d %02d:%02d:%02d\n", dt.Day, dt.Month, dt.Year, dt.Hour, dt.Minute, dt.Second );
        fprintf( f, "\n" );

        // Exception information
        if( siginfo )
        {
            fprintf( f, "Exception\n" );
            fprintf( f, "\tSigno   %s (%d)\n", strsignal( siginfo->si_signo ), siginfo->si_signo );
            fprintf( f, "\tCode    %s (%d)\n", sig_desc ? sig_desc : "No description", siginfo->si_code );
            fprintf( f, "\tErrno   %s (%d)\n", strerror( siginfo->si_errno ), siginfo->si_errno );
            fprintf( f, "\n" );
        }

        // AngelScript dump
        DumpAngelScript( f );

        // Threads
        fprintf( f, "Thread '%s' (%zu%s)\n", Thread::GetName(), Thread::GetId(), ", current" );

        // Stacktrace
        st_printer.print( st );
        st_printer.print( st, f );

        fclose( f );
    }

    if( siginfo )
    {
        ShowErrorMessage( message, traceback );
        ExitProcess( 1 );
    }
}

#else
# pragma MESSAGE( "Exception handling is disabled" )

void CatchExceptions( const string& app_name, int64 app_ver )
{
    //
}

void CreateDump( const string& appendix, const string& message )
{
    //
}

#endif

static void DumpAngelScript( FILE* f )
{
    string tb = Script::GetTraceback();
    if( !tb.empty() )
        fprintf( f, "AngelScript\n%s", tb.c_str() );
}

bool RaiseAssert( const string& message, const string& file, int line )
{
    string name = _str( file ).extractFileName();
    WriteLog( "Runtime assert: {} in {} ({})\n", message, name, line );

    #if defined ( FO_WINDOWS ) || defined ( FO_LINUX ) || defined ( FO_MAC )
    // Create dump
    CreateDump( _str( "AssertFailed_v{:#x}_{}({})", FO_VERSION, name, line ), message );

    // Show message
    string traceback = "";
    # if defined ( FO_LINUX ) || defined ( FO_MAC )
    backward::StackTrace st;
    st.load_here( 42 );
    backward::Printer st_printer;
    st_printer.snippet = false;
    std::stringstream ss;
    st_printer.print( st, ss );
    traceback = ss.str();
    # endif

    ShowErrorMessage( _str( "Assert failed!\nVersion: {:#x}\nFile: {} ({})\n\n{}", FO_VERSION, name, line, message ), traceback );
    #endif

    // Shut down
    ExitProcess( 1 );
    return true;
}

TEST_CASE()
{
    RUNTIME_ASSERT( 1 == 1 );

    TEST_SECTION()
    {
        RUNTIME_ASSERT( 2 == 2 );
    }
}

TEST_CASE()
{
    RUNTIME_ASSERT( 1 == 1 );

    TEST_SECTION()
    {
        RUNTIME_ASSERT( 2 == 2 );
    }
}
