#ifndef __LOG__
#define __LOG__

#include "Common.h"

using LogFunc = std::function< void(const string&) >;

// Write formatted text
void WriteLogMessage( const string& message );
template< typename ... Args >
inline void WriteLog( const string& message, Args ... args ) { WriteLogMessage( fmt::format( message, args ... ) ); }

// Control
void LogToFile( const string& fname );
void LogToFunc( LogFunc func, bool enable );
void LogToBuffer( bool enable );
void LogGetBuffer( string& buf );

#endif // __LOG__
