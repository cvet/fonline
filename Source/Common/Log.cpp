#include "Log.h"
#include "FileSystem.h"
#include "Threading.h"
#include "StringUtils.h"
#include "Testing.h"
#ifdef FO_ANDROID
# include <android/log.h>
#endif

class NullLogger: public ILogger
{
public:
    virtual void WriteMessage( const string& message ) override { /* Null */ }
};

Logger Fabric::CreateNullLogger()
{
    return std::make_shared< NullLogger >();
}

class ConsoleLogger: public ILogger
{
public:
    virtual void WriteMessage( const string& message ) override
    {
        static Mutex logLocker;
        SCOPE_LOCK( logLocker );

        #ifdef FO_WINDOWS
        OutputDebugStringW( _str( message ).toWideChar().c_str() );
        #endif

        #ifndef FO_ANDROID
        printf( "%s", message.c_str() );
        #else
        __android_log_print( ANDROID_LOG_INFO, "FOnline", "%s", message.c_str() );
        #endif
    }
};

Logger Fabric::CreateConsoleLogger()
{
    return std::make_shared< ConsoleLogger >();
}

class FileLogger: public ILogger
{
    Mutex logLocker;
    void* logFileHandle;

public:
    FileLogger( const string& path )
    {
        logFileHandle = FileOpen( path, true, true );
        RUNTIME_ASSERT( logFileHandle );
    }

    virtual void WriteMessage( const string& message ) override
    {
        SCOPE_LOCK( logLocker );

        FileWrite( logFileHandle, message.c_str(), (uint) message.length() );
    }

    virtual ~FileLogger() override
    {
        FileClose( logFileHandle );
    }
};

Logger Fabric::CreateFileLogger( const string& path )
{
    return std::make_shared< FileLogger >( path );
}

class CallbackLogger: public ILogger
{
    Mutex       logLocker;
    LogCallback logCallback;
    bool        callbackGuard = false;

public:
    CallbackLogger( LogCallback callback )
    {
        logCallback = callback;
    }

    virtual void WriteMessage( const string& message ) override
    {
        SCOPE_LOCK( logLocker );

        RUNTIME_ASSERT( !callbackGuard );
        callbackGuard = true;

        logCallback( message );

        callbackGuard = false;
    }
};

Logger Fabric::CreateCallbackLogger( ILogger::LogCallback callback )
{
    return std::make_shared< CallbackLogger >( callback );
}

TEST_CASE()
{
    string str;
    auto   callback_logger = Fabric::CreateCallbackLogger([ &str ] ( const string &message )
                                                          {
                                                              str += message;
                                                          } );
    callback_logger->Write( "Hello World!\n" );
    RUNTIME_ASSERT( str == "Hello World!\n" );
}


static Mutex                  LogLocker;
static bool                   LogDisableTimestamp;
static void*                  LogFileHandle;
static map< string, LogFunc > LogFunctions;
static bool                   LogFunctionsInProcess;
static string*                LogBufferStr;

void LogWithoutTimestamp()
{
    SCOPE_LOCK( LogLocker );

    LogDisableTimestamp = true;
}

void LogToFile( const string& fname )
{
    SCOPE_LOCK( LogLocker );

    if( LogFileHandle )
        FileClose( LogFileHandle );
    LogFileHandle = nullptr;

    if( !fname.empty() )
        LogFileHandle = FileOpen( fname, true, true );
}

void LogToFunc( const string& key, LogFunc func, bool enable )
{
    SCOPE_LOCK( LogLocker );

    if( func )
    {
        LogFunctions.erase( key );

        if( enable )
            LogFunctions.insert( std::make_pair( key, func ) );
    }
    else if( !enable )
    {
        LogFunctions.clear();
    }
}

void LogToBuffer( bool enable )
{
    SCOPE_LOCK( LogLocker );

    SAFEDEL( LogBufferStr );
    if( enable )
        LogBufferStr = new string();
}

void LogGetBuffer( std::string& buf )
{
    SCOPE_LOCK( LogLocker );

    if( LogBufferStr && !LogBufferStr->empty() )
    {
        buf = *LogBufferStr;
        LogBufferStr->clear();
    }
}

void WriteLogMessage( const string& message )
{
    SCOPE_LOCK( LogLocker );

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
        for( auto& kv : LogFunctions )
            kv.second( result );
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
