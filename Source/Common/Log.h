#pragma once

#include "Common.h"

class ILogger
{
public:
    using LogCallback = std::function< void(const string&) >;

    virtual void WriteMessage( const string& message ) = 0;
    template< typename ... Args >
    inline void Write( const char* format, Args ... args ) { WriteMessage( fmt::format( format, args ... ) ); }
    virtual ~ILogger() = default;
};
using Logger = std::shared_ptr< ILogger >;

namespace Fabric
{
    Logger CreateNullLogger();
    Logger CreateConsoleLogger();
    Logger CreateFileLogger( const string& path );
    Logger CreateCallbackLogger( ILogger::LogCallback callback );
}


using LogFunc = std::function< void(const string&) >;

// Write formatted text
void WriteLogMessage( const string& message );
template< typename ... Args >
inline void WriteLog( const string& message, Args ... args ) { WriteLogMessage( fmt::format( message, args ... ) ); }

// Control
void LogWithoutTimestamp();
void LogToFile( const string& fname );
void LogToFunc( const string& key, LogFunc func, bool enable );
void LogToBuffer( bool enable );
void LogGetBuffer( string& buf );
