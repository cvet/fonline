#include "StdAfx.h"
#include "Exception.h"
#include <windows.h>
#include <stdio.h>
#include <DbgHelp.h>
#pragma comment(lib, "Dbghelp.lib")
#include <Psapi.h>
#pragma comment(lib, "Psapi.lib")
#include <tlhelp32.h>
#include "Timer.h"
#include "FileManager.h"

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

LONG WINAPI TopLevelFilterReadableDump( EXCEPTION_POINTERS* except );
LONG WINAPI TopLevelFilterMiniDump( EXCEPTION_POINTERS* except );

void CatchExceptions( const char* app_name, unsigned int app_ver )
{
    if( app_name )
        Str::Copy( AppName, app_name );
    sprintf( AppVer, "%04X", app_ver );

    if( app_name )
    #ifndef EXCEPTION_MINIDUMP
        SetUnhandledExceptionFilter( TopLevelFilterReadableDump );
    #else
        SetUnhandledExceptionFilter( TopLevelFilterMiniDump );
    #endif
    else
        SetUnhandledExceptionFilter( NULL );
}

#ifdef FO_X86
# pragma warning( disable : 4748 )
#endif
LONG WINAPI TopLevelFilterReadableDump( EXCEPTION_POINTERS* except )
{
    LONG        retval = EXCEPTION_CONTINUE_SEARCH;
    char        mess[ 1024 ];
    char        dump_path[ 512 ];

    DateTime    dt;
    Timer::GetCurrentDateTime( dt );
    const char* dump_str = except ? "CrashDump" : ManualDumpAppendix;
    sprintf( dump_path, "%s_%s_%s_%02d%02d_%02d%02d.txt", dump_str, AppName, AppVer, dt.Day, dt.Month, dt.Hour, dt.Minute );

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
        fprintf( f, "\tTimestamp   %02d.%02d.%04d %02d:%02d:%02d\n", dt.Day, dt.Month, dt.Year, dt.Hour, dt.Minute, dt.Second );
        fprintf( f, "\n" );

        // Exception information
        if( except )
        {
            fprintf( f, "Exception\n" );
            fprintf( f, "\tCode      " );
            switch( except->ExceptionRecord->ExceptionCode )
            {
                #define CASE_EXCEPTION( e ) \
                case e:                     \
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
        HANDLE process = OpenProcess( PROCESS_ALL_ACCESS, FALSE, GetCurrentProcessId() );
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
            DWORD  tid = threads_ids[ i ];
            HANDLE t = OpenThread( THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION | THREAD_GET_CONTEXT, FALSE, tid );
            fprintf( f, "Thread %d%s\n", tid, !i ? " (current)" : "" );

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
                    #ifdef FO_X86
                    __asm     label :
                    __asm mov[ context.Ebp ], ebp;
                    __asm     mov[ context.Esp ], esp;
                    __asm mov eax, [ label ];
                    __asm     mov[ context.Eip ], eax;
                    #else // FO_X64
                    RtlCaptureContext( &context );
                    #endif
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

            #ifdef FO_X86
            DWORD machine_type = IMAGE_FILE_MACHINE_I386;
            stack.AddrFrame.Mode   = AddrModeFlat;
            stack.AddrFrame.Offset = context.Ebp;
            stack.AddrPC.Mode      = AddrModeFlat;
            stack.AddrPC.Offset    = context.Eip;
            stack.AddrStack.Mode   = AddrModeFlat;
            stack.AddrStack.Offset = context.Esp;
            #else // FO_X64
            DWORD machine_type = IMAGE_FILE_MACHINE_AMD64;
            stack.AddrPC.Offset = context.Rip;
            stack.AddrPC.Mode = AddrModeFlat;
            stack.AddrFrame.Offset = context.Rsp;
            stack.AddrFrame.Mode = AddrModeFlat;
            stack.AddrStack.Offset = context.Rsp;
            stack.AddrStack.Mode = AddrModeFlat;
            #endif

            #define STACKWALK_MAX_NAMELEN    ( 1024 )
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

            int frameNum = 0;
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

                    typedef enum CallstackEntryType
                    {
                        firstEntry,
                        nextEntry,
                        lastEntry,
                    };

                    static void OnCallstackEntry( FILE* f, CallstackEntryType eType, CallstackEntry& entry )
                    {
                        if( ( eType != lastEntry ) && ( entry.offset != 0 ) )
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
                callstack.offset = stack.AddrPC.Offset;
                callstack.name[ 0 ] = 0;
                callstack.undName[ 0 ] = 0;
                callstack.undFullName[ 0 ] = 0;
                callstack.offsetFromSmybol = 0;
                callstack.offsetFromLine = 0;
                callstack.lineFileName[ 0 ] = 0;
                callstack.lineNumber = 0;
                callstack.loadedImageName[ 0 ] = 0;
                callstack.moduleName[ 0 ] = 0;

                IMAGEHLP_LINE64 line;
                memset( &line, 0, sizeof( line ) );
                line.SizeOfStruct = sizeof( line );

                if( stack.AddrPC.Offset == stack.AddrReturn.Offset )
                {
                    fprintf( f, "\tStackWalk64-Endless-Callstack!", 0, stack.AddrPC.Offset );
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
                    memset( &module, 0, sizeof( module ) );
                    module.SizeOfStruct = sizeof( module );
                    if( SymGetModuleInfo64( process, stack.AddrPC.Offset, &module ) )
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
                            #if API_VERSION_NUMBER >= 9
                        case SymDia:
                            callstack.symTypeString = "DIA";
                            break;
                            #endif
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

                CSE::CallstackEntryType et = CSE::nextEntry;
                if( frameNum == 0 )
                    et = CSE::firstEntry;
                CSE::OnCallstackEntry( f, et, callstack );

                if( stack.AddrReturn.Offset == 0 )
                {
                    CSE::OnCallstackEntry( f, CSE::lastEntry, callstack );
                    SetLastError( ERROR_SUCCESS );
                    break;
                }

                frameNum++;
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

        sprintf( mess, DumpMess, dump_path );
    }
    else
    {
        sprintf( mess, "Error while create dump file - Error create file, path<%s>, err<%d>.", dump_path, GetLastError() );
    }

    if( except )
        MessageBox( NULL, mess, "FOnline Error", MB_OK );

    return EXCEPTION_EXECUTE_HANDLER;
}

LONG WINAPI TopLevelFilterMiniDump( EXCEPTION_POINTERS* except )
{
    LONG       retval = EXCEPTION_CONTINUE_SEARCH;
    char       mess[ 1024 ];
    char       dump_path[ 512 ];

    DateTime    dt;
    Timer::GetCurrentDateTime( dt );
    const char* dump_str = except ? "CrashDump" : ManualDumpAppendix;
    sprintf( dump_path, "%s_%s_%s_%02d%02d_%02d%02d.txt", dump_str, AppName, AppVer, dt.Day, dt.Month, dt.Hour, dt.Minute );

    HANDLE f = CreateFile( dump_path, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
    if( f != INVALID_HANDLE_VALUE )
    {
        MINIDUMP_EXCEPTION_INFORMATION ex_info;
        ex_info.ThreadId = GetCurrentThreadId();
        ex_info.ExceptionPointers = except;
        ex_info.ClientPointers = FALSE;

        if( MiniDumpWriteDump( GetCurrentProcess(), GetCurrentProcessId(), f, MiniDumpNormal, except ? &ex_info : NULL, NULL, NULL ) )
        {
            sprintf( mess, DumpMess, dump_path );
            retval = EXCEPTION_EXECUTE_HANDLER;
        }
        else
        {
            sprintf( mess, "Error while create dump file - File save error, path<%s>, err<%d>.", dump_path, GetLastError() );
        }

        CloseHandle( f );
    }
    else
    {
        sprintf( mess, "Error while create dump file - Error create file, path<%s>, err<%d>.", dump_path, GetLastError() );
    }

    if( except )
        MessageBox( NULL, mess, "FOnline Error", MB_OK );

    return retval;
}

void CreateDump( const char* appendix )
{
    Str::Copy( ManualDumpAppendix, appendix );

    #ifndef EXCEPTION_MINIDUMP
    TopLevelFilterReadableDump( NULL );
    #else
    TopLevelFilterMiniDump( NULL );
    #endif
}
