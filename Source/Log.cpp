#include "Log.h"
#include "Timer.h"
#include "FileSystem.h"
#include <stdarg.h>

#ifdef FO_ANDROID
# include <android/log.h>
#endif

#ifndef NO_THREADING
static Mutex                LogLocker;
#endif
static void*                LogFileHandle;
static vector< LogFuncPtr > LogFunctions;
static bool                 LogFunctionsInProcess;
static string*              LogBufferStr;

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

void LogToFunc( LogFuncPtr func_ptr, bool enable )
{
    #ifndef NO_THREADING
    SCOPE_LOCK( LogLocker );
    #endif

    if( func_ptr )
    {
        vector< LogFuncPtr >::iterator it = std::find( LogFunctions.begin(), LogFunctions.end(), func_ptr );
        if( enable && it == LogFunctions.end() )
            LogFunctions.push_back( func_ptr );
        else if( !enable && it != LogFunctions.end() )
            LogFunctions.erase( it );
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
        time += fmt::format( "[{:0=3}:{:0=2}:{:0=2}:{:0=3}] ", hours, minutes, seconds % 60, delta % 1000 );
    else if( minutes )
        time += fmt::format( "[{:0=2}:{:0=2}:{:0=3}] ", minutes, seconds % 60, delta % 1000 );
    else
        time += fmt::format( "[{:0=2}:{:0=3}] ", seconds % 60, delta % 1000 );

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
        for( size_t i = 0, j = LogFunctions.size(); i < j; i++ )
            ( *LogFunctions[ i ] )( result.c_str() );
        LogFunctionsInProcess = false;
    }

    #ifdef FO_WINDOWS
    OutputDebugStringW( CharToWideChar( result ).c_str() );
    #endif

    #ifndef FO_ANDROID
    printf( "%s", result.c_str() );
    #else
    __android_log_print( ANDROID_LOG_INFO, "FOnline", "%s", result.c_str() );
    #endif
}
