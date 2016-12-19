#include "Log.h"
#include "Timer.h"
#include <stdarg.h>

#if defined ( FONLINE_SERVER ) && !defined ( SERVER_DAEMON )
# include "FL/Fl_Text_Display.H"
#endif

static Mutex                LogLocker;
static void*                LogFileHandle;
static vector< LogFuncPtr > LogFunctions;
static bool                 LogFunctionsInProcess;
static void*                LogTextBox;
static string*              LogBufferStr;
static bool                 ToDebugOutput;
static bool                 LoggingWithTime;
static bool                 LoggingWithThread;

void LogToFile( const string& fname )
{
    SCOPE_LOCK( LogLocker );

    if( LogFileHandle )
        FileClose( LogFileHandle );
    LogFileHandle = nullptr;

    if( !fname.empty() )
        LogFileHandle = FileOpen( fname.c_str(), true, true );
}

void LogToFunc( LogFuncPtr func_ptr, bool enable )
{
    SCOPE_LOCK( LogLocker );

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

void LogToTextBox( void* text_box )
{
    SCOPE_LOCK( LogLocker );

    LogTextBox = text_box;
}

void LogToBuffer( bool enable )
{
    SCOPE_LOCK( LogLocker );

    SAFEDEL( LogBufferStr );
    if( enable )
        LogBufferStr = new string();
}

void LogToDebugOutput( bool enable )
{
    SCOPE_LOCK( LogLocker );

    ToDebugOutput = enable;
}

void LogFinish()
{
    SCOPE_LOCK( LogLocker );

    LogToFile( "" );
    LogToFunc( nullptr, false );
    LogToTextBox( nullptr );
    LogToBuffer( false );
    LogToDebugOutput( false );
}

void WriteLogMessage( const string& message )
{
    SCOPE_LOCK( LogLocker );

    if( LogFunctionsInProcess )
        return;

    string result;

    #if !defined ( FONLINE_NPCEDITOR ) && !defined ( FONLINE_MRFIXIT )
    if( LoggingWithThread )
    {
        const char* name = Thread::GetCurrentName();
        if( name[ 0 ] )
            result += fmt::format( "[{}] ", name );
        else
            result += fmt::format( "[{}] ", Thread::GetCurrentId() );
    }
    #endif

    if( LoggingWithTime )
    {
        uint delta = ( uint ) Timer::AccurateTick();
        uint seconds = delta / 1000;
        uint minutes = seconds / 60 % 60;
        uint hours = seconds / 60 / 60;
        if( hours )
            result += fmt::format( "[{:0=3}:{:0=2}:{:0=2}:{:0=3}] ", hours, minutes, seconds % 60, delta % 1000 );
        else if( minutes )
            result += fmt::format( "[{:0=2}:{:0=2}:{:0=3}] ", minutes, seconds % 60, delta % 1000 );
        else
            result += fmt::format( "[{:0=2}:{:0=3}] ", seconds % 60, delta % 1000 );
    }

    result += message;

    if( LogFileHandle )
    {
        FileWrite( LogFileHandle, result.c_str(), (uint) result.length() );
    }
    if( !LogFunctions.empty() )
    {
        LogFunctionsInProcess = true;
        for( size_t i = 0, j = LogFunctions.size(); i < j; i++ )
            ( *LogFunctions[ i ] )( result.c_str() );
        LogFunctionsInProcess = false;
    }
    if( LogTextBox )
    {
        #if defined ( FONLINE_SERVER ) && !defined ( SERVER_DAEMON )
        ( (Fl_Text_Display*) LogTextBox )->buffer()->append( result.c_str() );
        #endif
    }
    if( LogBufferStr )
    {
        *LogBufferStr += result;
    }
    if( ToDebugOutput )
    {
        #ifdef FO_WINDOWS
        OutputDebugString( result.c_str() );
        #endif
    }

    #if !defined ( FO_WINDOWS ) || defined ( FONLINE_SCRIPT_COMPILER )
    printf( "%s", result.c_str() );
    #endif
}

void LogWithTime( bool enable )
{
    SCOPE_LOCK( LogLocker );

    LoggingWithTime = enable;
}

void LogWithThread( bool enable )
{
    SCOPE_LOCK( LogLocker );

    LoggingWithThread = enable;
}

void LogGetBuffer( std::string& buf )
{
    SCOPE_LOCK( LogLocker );

    if( LogBufferStr )
    {
        buf = *LogBufferStr;
        LogBufferStr->clear();
    }
}
