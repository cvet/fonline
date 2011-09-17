#include "StdAfx.h"
#include "Exception.h"
#include <windows.h>
#include <stdio.h>

// #include <dbghelp.h>
typedef struct _MINIDUMP_EXCEPTION_INFORMATION
{
    DWORD               ThreadId;
    PEXCEPTION_POINTERS ExceptionPointers;
    BOOL                ClientPointers;
} MINIDUMP_EXCEPTION_INFORMATION, * PMINIDUMP_EXCEPTION_INFORMATION;

char DumpMessRus[] =
{
    "Пожалуйста вышлите этот файл <%s> на e-mail <support@fonline.ru>, "
    "тема письма должна содержать слово <dump>. В письме укажите при "
    "каких обстоятельствах произошел сбой. Спасибо.\n"
};

char DumpMessEng[] =
{
    "Please send this file <%s> on e-mail <support@fonline.ru>, "
    "the theme of the letter should contain a word <dump>. In the letter specify "
    "under what circumstances there was a failure. Thanks.\n"
};

char* DumpMess = DumpMessEng;

void SetExceptionsRussianText()
{
    DumpMess = DumpMessRus;
}

char AppName[ 128 ];
char AppVer[ 128 ];

LONG WINAPI TopLevelFilterDump( struct _EXCEPTION_POINTERS* except );
LONG WINAPI TopLevelFilterSimple( struct _EXCEPTION_POINTERS* except );

void CatchExceptions( const char* app_name, unsigned int app_ver )
{
    if( app_name )
        Str::Copy( AppName, app_name );
    sprintf( AppVer, "%04X", app_ver );

    if( app_name )
        SetUnhandledExceptionFilter( TopLevelFilterDump );
    else
        SetUnhandledExceptionFilter( NULL );
}

typedef bool ( WINAPI * MINIDUMPWRITEDUMP )( HANDLE, DWORD, HANDLE, int /*MINIDUMP_TYPE*/,
                                             const PMINIDUMP_EXCEPTION_INFORMATION,
                                             const void* /*PMINIDUMP_USER_STREAM_INFORMATION*/,
                                             const void* /*PMINIDUMP_CALLBACK_INFORMATION*/ );

LONG WINAPI TopLevelFilterDump( struct _EXCEPTION_POINTERS* except )
{
    LONG    retval = EXCEPTION_CONTINUE_SEARCH;
    char    mess[ 1024 ];
    HMODULE dll = LoadLibrary( "DBGHELP.DLL" );

    if( dll )
    {
        MINIDUMPWRITEDUMP dump = (MINIDUMPWRITEDUMP) GetProcAddress( dll, "MiniDumpWriteDump" );
        if( dump )
        {
            char       dump_path[ 512 ];
            SYSTEMTIME local_time;

            GetLocalTime( &local_time );
            sprintf( dump_path, ".\\%s_%s_%02d%02d_%02d%02d.dmp",
                     AppName, AppVer,
                     local_time.wDay, local_time.wMonth,
                     local_time.wHour, local_time.wMinute );

            HANDLE f = CreateFile( dump_path, GENERIC_WRITE, FILE_SHARE_WRITE, NULL,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );

            if( f != INVALID_HANDLE_VALUE )
            {
                _MINIDUMP_EXCEPTION_INFORMATION ex_info;

                ex_info.ThreadId = GetCurrentThreadId();
                ex_info.ExceptionPointers = except;
                ex_info.ClientPointers = NULL;

                bool ok = dump( GetCurrentProcess(), GetCurrentProcessId(),
                                f, 0 /*MiniDumpNormal*/, &ex_info, NULL, NULL );

                if( ok )
                {
                    sprintf( mess, DumpMess, &dump_path[ 2 ] );
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
        }
        else
        {
            return TopLevelFilterSimple( except );
        }
    }
    else
    {
        return TopLevelFilterSimple( except );
    }

    MessageBox( NULL, mess, "FOnline Error", MB_OK );
    return retval;
}

LONG WINAPI TopLevelFilterSimple( struct _EXCEPTION_POINTERS* except )
{
    LONG       retval = EXCEPTION_CONTINUE_SEARCH;
    char       mess[ 1024 ];
    char       dump_path[ 512 ];
    SYSTEMTIME local_time;

    GetLocalTime( &local_time );
    sprintf( dump_path, ".\\Error_%s_%s_%02d%02d_%02d%02d.txt",
             AppName, AppVer,
             local_time.wDay, local_time.wMonth,
             local_time.wHour, local_time.wMinute );

    void* file = FileOpen( dump_path, true );
    if( file )
    {
        char str[ 128 ];
        char dump_str[ 512 ];

        sprintf( str, "Name <%s>, Version <%s>\n", AppName, AppVer );
        Str::Copy( dump_str, str );
        sprintf( str, "ExceptionAddress <%p>\n", except->ExceptionRecord->ExceptionAddress );
        Str::Append( dump_str, str );
        sprintf( str, "ExceptionCode <0x%0x>\n", except->ExceptionRecord->ExceptionCode );
        Str::Append( dump_str, str );
        sprintf( str, "ExceptionFlags <0x%0x>\n", except->ExceptionRecord->ExceptionFlags );
        Str::Append( dump_str, str );

        if( FileWrite( file, dump_str, Str::Length( dump_str ) ) )
        {
            sprintf( mess, DumpMess, &dump_path[ 2 ] );
            retval = EXCEPTION_EXECUTE_HANDLER;
        }
        else
        {
            sprintf( mess, "Error while create dump file - file save error, path<%s>, err<%d>.", dump_path, GetLastError() );
        }

        FileClose( file );
    }
    else
    {
        sprintf( mess, "Error while create dump file - Error create file, path<%s>, err<%d>.", dump_path, GetLastError() );
    }

    MessageBox( NULL, mess, "FOnline Error", MB_OK );
    return retval;
}

void CreateDump( const char* appendix )
{
    HMODULE dll = LoadLibrary( "DBGHELP.DLL" );
    if( dll )
    {
        MINIDUMPWRITEDUMP dump = (MINIDUMPWRITEDUMP) GetProcAddress( dll, "MiniDumpWriteDump" );
        if( dump )
        {
            char       dump_path[ 512 ];
            SYSTEMTIME local_time;

            GetLocalTime( &local_time );
            sprintf( dump_path, ".\\%s_%s_%02d%02d_%02d%02d_%s.dmp",
                     AppName, AppVer,
                     local_time.wDay, local_time.wMonth,
                     local_time.wHour, local_time.wMinute,
                     appendix );

            HANDLE f = CreateFile( dump_path, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
            if( f != INVALID_HANDLE_VALUE )
            {
                dump( GetCurrentProcess(), GetCurrentProcessId(), f, 0 /*MiniDumpNormal*/, NULL, NULL, NULL );
                CloseHandle( f );
            }
        }
    }
}
