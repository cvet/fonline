#ifndef __LOG__
#define __LOG__

#include <string>

#define _LOG_          __FUNCTION__ ":" __LINE__ " - "
#define MAX_LOGTEXT    UTF8_BUF_SIZE( 2048 )
typedef void ( *LogFuncPtr )( const char* str );

// Write formatted text
void WriteLog( const char* frmt, ... );
void WriteLogF( const char* func, const char* frmt, ... );

// Append logging to
void LogToFile( const char* fname );                // File
void LogToFunc( LogFuncPtr func_ptr, bool enable ); // Extern function
void LogToTextBox( void* text_box );                // Text box (Fl_Text_Display)
void LogToBuffer( bool enable );                    // Buffer, to get value use LogGetBuffer
void LogToDebugOutput( bool enable );               // OutputDebugString / printf

// Other stuff
void LogFinish();                                   // Finish all logging
void LogWithTime( bool enable );                    // Logging with time
void LogWithThread( bool enable );                  // Logging with thread name
void LogGetBuffer( std::string& buf );              // Get buffer, if used LogBuffer

#endif // __LOG__
