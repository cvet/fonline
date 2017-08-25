#include "Log.h"
#include "Timer.h"
#include "FileSystem.h"
#include <stdarg.h>

#ifdef FO_ANDROID
# include <android/log.h>
#endif

#ifndef NO_THREADING
static Mutex             LogLocker;
#endif
static void*             LogFileHandle;
static vector< LogFunc > LogFunctions;
static bool              LogFunctionsInProcess;
static string*           LogBufferStr;

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

    // Timestamp
    string time;
    uint   delta = ( uint ) Timer::AccurateTick();
    uint   seconds = delta / 1000;
    uint   minutes = seconds / 60 % 60;
    uint   hours = seconds / 60 / 60;
    if( hours )
        time += _str( "[{:0=3}:{:0=2}:{:0=2}:{:0=3}] ", hours, minutes, seconds % 60, delta % 1000 );
    else if( minutes )
        time += _str( "[{:0=2}:{:0=2}:{:0=3}] ", minutes, seconds % 60, delta % 1000 );
    else
        time += _str( "[{:0=2}:{:0=3}] ", seconds % 60, delta % 1000 );

    // Result message
    string result = time + message;

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
