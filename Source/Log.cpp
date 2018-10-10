#include "Log.h"
#include "Timer.h"
#include "FileSystem.h"
#include <stdarg.h>
#include <time.h>

#ifdef FO_ANDROID
# include <android/log.h>
#endif

#ifndef NO_THREADING
static Mutex             LogLocker;
#endif
static bool              LogDisableTimestamp;
static void*             LogFileHandle;
static vector< LogFunc > LogFunctions;
static bool              LogFunctionsInProcess;
static string*           LogBufferStr;

void LogWithoutTimestamp()
{
    #ifndef NO_THREADING
    SCOPE_LOCK( LogLocker );
    #endif

    LogDisableTimestamp = true;
}

void LogToFile( const string& fname )
{
    #ifndef NO_THREADING
    SCOPE_LOCK( LogLocker );
    #endif

    if( LogFileHandle )
        FileClose( LogFileHandle );
    LogFileHandle = nullptr;

    if( !fname.empty() )
        LogFileHandle = FileOpen( fname, true, true );
}

void LogToFunc( LogFunc func, bool enable )
{
    #ifndef NO_THREADING
    SCOPE_LOCK( LogLocker );
    #endif

    if( func )
    {
        size_t index = 0;
        for( auto& f : LogFunctions )
        {
            if( f.target< LogFunc >() == func.target< LogFunc >() )
            {
                LogFunctions.erase( LogFunctions.begin() + index );
                break;
            }
            index++;
        }
        if( enable )
            LogFunctions.push_back( func );
    }
    else if( !enable )
    {
        LogFunctions.clear();
    }
}

void LogToBuffer( bool enable )
{
    #ifndef NO_THREADING
    SCOPE_LOCK( LogLocker );
    #endif

    SAFEDEL( LogBufferStr );
    if( enable )
        LogBufferStr = new string();
}

void LogGetBuffer( std::string& buf )
{
    #ifndef NO_THREADING
    SCOPE_LOCK( LogLocker );
    #endif

    if( LogBufferStr )
    {
        buf = *LogBufferStr;
        LogBufferStr->clear();
    }
}

void WriteLogMessage( const string& message )
{
    #ifndef NO_THREADING
    SCOPE_LOCK( LogLocker );
    #endif

    // Avoid recursive calls
    if( LogFunctionsInProcess )
        return;

    // Make message
    string result;
    if( !LogDisableTimestamp )
    {
        time_t     now = time( nullptr );
        struct tm* t = localtime( &now );
        result += _str( "[{:0=2}:{:0=2}:{:0=2}] ", t->tm_hour, t->tm_min, t->tm_sec );
    }
    result += message;

    // Write logs
    if( LogFileHandle )
        FileWrite( LogFileHandle, result.c_str(), (uint) result.length() );

    if( LogBufferStr )
        *LogBufferStr += result;

    if( !LogFunctions.empty() )
    {
        LogFunctionsInProcess = true;
        for( auto& func : LogFunctions )
            func( result );
        LogFunctionsInProcess = false;
    }

    #ifdef FO_WINDOWS
    OutputDebugStringW( _str( result ).toWideChar().c_str() );
    #endif

    #ifndef FO_ANDROID
    printf( "%s", result.c_str() );
    #else
    __android_log_print( ANDROID_LOG_INFO, "FOnline", "%s", result.c_str() );
    #endif
}
