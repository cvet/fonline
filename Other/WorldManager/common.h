#ifndef __COMMON_H__
#define __COMMON_H__

/********************************************************************
	created:	22:05:2007   19:39

	author:		Anton Tvetinsky aka Cvet
	
	purpose:	
*********************************************************************/

#define CrID		DWORD
#define CrTYPE		BYTE

int LogStart();
void LogFinish();
void WriteLog(char* frmt, ...);

#endif //__COMMON_H__