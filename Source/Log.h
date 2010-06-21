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
void LogToDlg(HWND dlg_item); // Dialog item
void LogToBuffer(HANDLE event); // Buffer, to get value use LogGetBuffer

int LogGetType();
void LogFinish(int log_type); // Finish logging
void LogWithTime(bool enable); // Logging with time
void LogGetBuffer(std::string& buf); // Get buffer, if used LogBuffer

#endif // __LOG__