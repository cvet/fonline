#include "stdafx.h"

/********************************************************************
	created:	22:05:2007   19:39

	author:		Anton Tvetinsky aka Cvet
	
	purpose:	
*********************************************************************/

HANDLE hLogFile=NULL;

int LogStart()
{
	if(hLogFile) return 1;

	hLogFile=CreateFile(".\\FOserv.log",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_FLAG_WRITE_THROUGH,NULL);
	
	if(hLogFile==INVALID_HANDLE_VALUE) return 0;
	
	return 1;
}

void LogFinish()
{
	if(!hLogFile) return;
	
	CloseHandle(hLogFile);
	hLogFile=NULL;
}

void WriteLog(char* frmt, ...)
{
	char str[2048];
	
	va_list list;
	
	va_start(list, frmt);
	wvsprintf(str, frmt, list);
	va_end(list);
	
	DWORD br;
	WriteFile(hLogFile,str,strlen(str),&br,NULL);
}