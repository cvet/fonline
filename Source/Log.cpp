#include "StdAfx.h"
#include "Log.h"
#include <Richedit.h>
#pragma comment(lib,"User32.lib")

CRITICAL_SECTION LogCS;
int LoggingType=0;
static HANDLE LogFileHadle=NULL;
LogFuncPtr LogFunction=NULL;
HWND LogDlgItem=NULL;
std::string LogBufferStr;
HANDLE LogBufferEvent;
bool LoggingWithTime=false;
DWORD StartLogTime=Timer::FastTick();

struct _Initializator
{
	_Initializator()
	{
		InitializeCriticalSection(&LogCS);
	}

	~_Initializator()
	{
		DeleteCriticalSection(&LogCS);
	}
} Initializator;

void LogToFile(const char* fname)
{
	LogFinish(LOG_FILE);
	if(!fname) return;

	LogFileHadle=CreateFile(fname,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_FLAG_WRITE_THROUGH,NULL);
	if(LogFileHadle==INVALID_HANDLE_VALUE)
	{
		LogFileHadle=NULL;
		return;
	}
	LoggingType|=LOG_FILE;
}

void LogToFunc(LogFuncPtr func_ptr)
{
	LogFinish(LOG_FUNC);
	LogFunction=func_ptr;
	LoggingType|=LOG_FUNC;
}

void LogToDlg(HWND dlg_item)
{
	LogFinish(LOG_DLG);
	if(!dlg_item) return;
	LogDlgItem=dlg_item;
	LoggingType|=LOG_DLG;
}

void LogToBuffer(HANDLE event)
{
	LogFinish(LOG_BUFFER);
	LogBufferStr.reserve(MAX_LOGTEXT*2);
	LogBufferEvent=event;
	LoggingType|=LOG_BUFFER;
}

int LogGetType()
{
	return LoggingType;
}

void LogFinish(int log_type)
{
	if(log_type&LOG_FILE)
	{
		if(LogFileHadle) CloseHandle(LogFileHadle);
		LogFileHadle=NULL;
	}
	if(log_type&LOG_FUNC) LogFunction=NULL;
	if(log_type&LOG_DLG) LogDlgItem=NULL;
	if(log_type&LOG_BUFFER)
	{
		LogBufferStr.clear();
		LogBufferEvent=NULL;
	}
	LoggingType&=~log_type;
}

void WriteLog(const char* frmt, ...)
{
	if(!LoggingType) return;

	EnterCriticalSection(&LogCS);

	char str[MAX_LOGTEXT];
	va_list list;
	if(LoggingWithTime)
	{
		sprintf(str,"(%u)",Timer::FastTick()-StartLogTime);
		size_t len=strlen(str);
		va_start(list,frmt);
		vsprintf_s(&str[len],MAX_LOGTEXT-len,frmt,list);
		va_end(list);
	}
	else
	{
		va_start(list,frmt);
		vsprintf_s(str,frmt,list);
		va_end(list);
	}

	if(LoggingType&LOG_FILE)
	{
		DWORD br;
		WriteFile(LogFileHadle,str,strlen(str),&br,NULL);
	}
	if(LoggingType&LOG_FUNC)
	{
		(*LogFunction)(str);
	}
	if(LoggingType&LOG_DLG)
	{
		SendMessage(LogDlgItem,EM_SETSEL,-1,-1);
		SendMessage(LogDlgItem,EM_REPLACESEL,0,(LPARAM)str);
	}
	if(LoggingType&LOG_BUFFER)
	{
		LogBufferStr+=str;
		SetEvent(LogBufferEvent);
	}

	LeaveCriticalSection(&LogCS);
}

void LogWithTime(bool enabled)
{
	LoggingWithTime=enabled;
}

void LogGetBuffer(std::string& buf)
{
	if(LoggingType&LOG_BUFFER)
	{
		EnterCriticalSection(&LogCS);
		buf=LogBufferStr;
		LogBufferStr.clear();
		LeaveCriticalSection(&LogCS);
	}
	else
	{
		buf="";
	}
}









