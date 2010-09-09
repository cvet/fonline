#ifndef __LOG__
#define __LOG__

#include <string>

#define _LOG_ __FUNCTION__ ":" __LINE__ " - "
#define MAX_LOGTEXT         (2048)
typedef void(*LogFuncPtr)(char* str);

// Types of Logging
#define LOG_FILE     (0x01)
#define LOG_FUNC     (0x02)
#define LOG_DLG      (0x04)
#define LOG_BUFFER   (0x08)

// Write formatted text
void WriteLog(const char* frmt, ...);

// Append logging to
void LogToFile(const char* fname); // File
void LogToFunc(LogFuncPtr func_ptr); // Extern function
void LogToDlg(void* dlg_item); // Dialog item
void LogToBuffer(void* event); // Buffer, to get value use LogGetBuffer
void LogSetThreadName(const char* name); // Set name for current thread

int LogGetType();
void LogFinish(int log_type); // Finish logging
void LogWithTime(bool enable); // Logging with time
void LogWithThread(bool enabled); // Logging with thread name
void LogGetBuffer(std::string& buf); // Get buffer, if used LogBuffer

#endif // __LOG__