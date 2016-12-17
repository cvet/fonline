#ifndef __LOG__
#define __LOG__

#include "Common.h"

typedef void ( *LogFuncPtr )( const char* str );

// Write formatted text
void WriteLogMessage( const string& message );
template< typename ... Args >
inline void WriteLog( const string& message, Args ... args ) { WriteLogMessage( fmt::format( message, args ... ) ); }

// Append logging to
void LogToFile( const string& fname );              // File
void LogToFunc( LogFuncPtr func_ptr, bool enable ); // Extern function
void LogToTextBox( void* text_box );                // Text box (Fl_Text_Display)
void LogToBuffer( bool enable );                    // Buffer, to get value use LogGetBuffer
void LogToDebugOutput( bool enable );               // OutputDebugString / printf

// Other stuff
void LogFinish();                                   // Finish all logging
void LogWithTime( bool enable );                    // Logging with time
void LogWithThread( bool enable );                  // Logging with thread name
void LogGetBuffer( string& buf );                   // Get buffer, if used LogBuffer

#endif // __LOG__
