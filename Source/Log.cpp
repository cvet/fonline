#include "StdAfx.h"
#include "Log.h"
#include "Timer.h"
#include <stdarg.h>

#if defined ( FONLINE_SERVER ) && !defined ( SERVER_DAEMON )
# include "FL/Fl_Text_Display.H"
#endif

#if defined ( FO_LINUX )
// #include <syslog.h>
#endif

Mutex       LogLocker;
int         LoggingType = 0;
void*       LogFileHandle = NULL;
LogFuncPtr  LogFunction = NULL;
void*       LogTextBox = NULL;
std::string LogBufferStr;
bool        LoggingWithTime = false;
bool        LoggingWithThread = false;
THREAD char LogThreadName[ 64 ] = { 0 };
uint        StartLogTime = Timer::FastTick();
void WriteLogInternal( const char* func, const char* frmt, va_list& list );

void LogToFile( const char* fname )
{
    LogFinish( LOG_FILE );
    if( !fname )
        return;

    LogFileHandle = FileOpen( fname, true );
    if( !LogFileHandle )
    {
        LogFileHandle = NULL;
        return;
    }
    LoggingType |= LOG_FILE;
}

void LogToFunc( LogFuncPtr func_ptr )
{
    LogFinish( LOG_FUNC );
    LogFunction = func_ptr;
    LoggingType |= LOG_FUNC;
}

void LogToTextBox( void* text_box )
{
    #if !defined ( FONLINE_SERVER ) || defined ( SERVER_DAEMON )
    return;
    #endif
    LogFinish( LOG_TEXT_BOX );
    if( !text_box )
        return;
    LogTextBox = text_box;
    LoggingType |= LOG_TEXT_BOX;
}

void LogToBuffer()
{
    #if !defined ( FONLINE_SERVER )
    return;
    #endif
    LogFinish( LOG_BUFFER );
    LogBufferStr.reserve( MAX_LOGTEXT * 2 );
    LoggingType |= LOG_BUFFER;
}

void LogToDebugOutput()
{
    LogFinish( LOG_DEBUG_OUTPUT );
    LoggingType |= LOG_DEBUG_OUTPUT;
}

void LogSetThreadName( const char* name )
{
    Str::Copy( LogThreadName, name );
}

int LogGetType()
{
    return LoggingType;
}

void LogFinish( int log_type )
{
    log_type &= LoggingType;
    if( log_type & LOG_FILE )
    {
        if( LogFileHandle )
            FileClose( LogFileHandle );
        LogFileHandle = NULL;
    }
    if( log_type & LOG_FUNC )
        LogFunction = NULL;
    if( log_type & LOG_TEXT_BOX )
        LogTextBox = NULL;
    if( log_type & LOG_BUFFER )
        LogBufferStr.clear();
    LoggingType ^= log_type;
}

void WriteLog( const char* frmt, ... )
{
    if( !LoggingType )
        return;

    va_list list;
    va_start( list, frmt );
    WriteLogInternal( NULL, frmt, list );
    va_end( list );
}

void WriteLogF( const char* func, const char* frmt, ... )
{
    if( !LoggingType )
        return;

    va_list list;
    va_start( list, frmt );
    WriteLogInternal( func, frmt, list );
    va_end( list );
}

void WriteLogInternal( const char* func, const char* frmt, va_list& list )
{
    LogLocker.Lock();

    char str_tid[ 64 ] = { 0 };
    if( LoggingWithThread )
    {
        if( LogThreadName[ 0 ] )
            Str::Format( str_tid, "[%s]", LogThreadName );
        else
            Str::Format( str_tid, "[%04u]", GetCurThreadId() );
    }

    char str_time[ 64 ] = { 0 };
    if( LoggingWithTime )
    {
        uint delta = Timer::FastTick() - StartLogTime;
        uint seconds = delta / 1000;
        uint minutes = seconds / 60 % 60;
        uint hours = seconds / 60 / 60;
        if( hours )
            Str::Format( str_time, "[%03u:%02u:%02u:%03u]", hours, minutes, seconds % 60, delta % 1000 );
        else if( minutes )
            Str::Format( str_time, "[%02u:%02u:%03u]", minutes, seconds % 60, delta % 1000 );
        else
            Str::Format( str_time, "[%02u:%03u]", seconds % 60, delta % 1000 );
    }

    char str[ MAX_LOGTEXT ] = { 0 };
    if( str_tid[ 0 ] )
        Str::Append( str, str_tid );
    if( str_time[ 0 ] )
        Str::Append( str, str_time );
    if( str_tid[ 0 ] || str_time[ 0 ] )
        Str::Append( str, " " );
    if( func )
        Str::Append( str, func );

    size_t len = Str::Length( str );
    vsnprintf( &str[ len ], MAX_LOGTEXT - len, frmt, list );
    str[ MAX_LOGTEXT - 1 ] = 0;

    if( LoggingType & LOG_FILE )
    {
        FileWrite( LogFileHandle, str, Str::Length( str ) );
    }
    if( LoggingType & LOG_FUNC )
    {
        ( *LogFunction )( str );
    }
    if( LoggingType & LOG_TEXT_BOX )
    {
        #if defined ( FONLINE_SERVER ) && !defined ( SERVER_DAEMON )
        ( (Fl_Text_Display*) LogTextBox )->buffer()->append( str );
        #endif
    }
    if( LoggingType & LOG_BUFFER )
    {
        LogBufferStr += str;
    }
    if( LoggingType & LOG_DEBUG_OUTPUT )
    {
        #if defined ( FO_WINDOWS )
        OutputDebugString( str );
        #else
        // Todo: linux, syslog ?
        #endif
    }

    LogLocker.Unlock();
}

void LogWithTime( bool enabled )
{
    LoggingWithTime = enabled;
}

void LogWithThread( bool enabled )
{
    LoggingWithThread = enabled;
}

void LogGetBuffer( std::string& buf )
{
    SCOPE_LOCK( LogLocker );

    buf = LogBufferStr;
    LogBufferStr.clear();
}
