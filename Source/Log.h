#ifndef __LOG__
#define __LOG__

#include "Common.h"

typedef void ( *LogFuncPtr )( const char* str );

// Write formatted text
void WriteLogMessage( const string& message );
template< typename ... Args >
inline void WriteLog( const string& message, Args ... args ) { WriteLogMessage( _str( message, args ... ) ); }

// Control
void LogToFile( const string& fname );
void LogToFunc( LogFuncPtr func_ptr, bool enable );
void LogToBuffer( bool enable );
void LogGetBuffer( string& buf );

#endif // __LOG__
