#if !defined _STDAFX_H
#define _STDAFX_H

#pragma once

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#pragma warning (disable : 4786)

#include <stdio.h>
#include <stdarg.h>
#include <winsock2.h>
#include <conio.h>
#include <stack>
#include <queue>
#include <map>
#include <string>
#include <fstream>
#include "zlib\zlib.h"
#pragma comment (lib, "Ws2_32.lib")

using namespace std;

#define NETMSG_LOGIN				1 //!Cvet edit
//////////////////////////////////////////////////////////////////////////
// клиент при подключении шлет это сообщение, называя логин и пароль
// params:
// char login[MAX_LOGIN]
// char pass[MAX_LOGIN]
//////////////////////////////////////////////////////////////////////////

#define NETMSG_LOGINOK				2
//////////////////////////////////////////////////////////////////////////
// ответ сервера на NETMSG_NAME - логин и пароль принят
// params:
//////////////////////////////////////////////////////////////////////////

#define NETMSG_CREATE_CLIENT		3 //!Cvet
//////////////////////////////////////////////////////////////////////////
// запрос серверу на создание аккаунта
// params:
//////////////////////////////////////////////////////////////////////////

#define NETMSG_LOGMSG				4 //!Cvet
//////////////////////////////////////////////////////////////////////////
// ответ сервера на логин/регистрацию
// params:
// BYTE LogMsg
//////////////////////////////////////////////////////////////////////////

#define NETMSG_STARTGAME			5 //!Deniska
//////////////////////////////////////////////////////////////////////////
// Запрос клиента на старт игры
// params: none
//////////////////////////////////////////////////////////////////////////

#define NETMSG_STARTCONSOLE			6 //!Deniska
//////////////////////////////////////////////////////////////////////////
// Клиент работает как консоль
// params: none
//////////////////////////////////////////////////////////////////////////

#define NETMSG_CONSOLE_OK			7 //!Deniska 
//////////////////////////////////////////////////////////////////////////
// Сервер посылает подтверждение, что клиент - консоль.
// params: none
//////////////////////////////////////////////////////////////////////////

#define NETMSG_CONSOLEMSG			8 //!Deniska
//////////////////////////////////////////////////////////////////////////
// Клиент-консоль посылает команду серверу.
// params: 
//		BYTE cmd		- команда
//		WORD len		- размер буфера в байтах
//		BYTE buf[len]	- передаваемые данные
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// Запросы консоли
#define CONSOLE_CMD_EXIT		1
#define CONSOLE_CMD_CONNECT		2
#define CONSOLE_CMD_HELP		3
#define CONSOLE_CMD_SET			4
#define CONSOLE_CMD_SHOW		5
#define CONSOLE_CMD_DISCONNECT  6
#define CONSOLE_CMD_EXEC		7
#define CONSOLE_CMD_SHOW_SCRIPT 8 
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// Ответ сервера
#define CONSOLE_RES_OK				0	// Подтверждение. Команда содержит ответ
#define CONSOLE_RES_INVALID_OP		1	// Запрос неудовлетворен. Неправильный операнд
//////////////////////////////////////////////////////////////////////////

typedef int CmdID;

//---------------------Лог--------------------------------------------------------------------
#define LOG_FILE_NAME "console.log"	
	bool logging = true;			// Установить false, если лог не нужен.

	FILE * log;						// Внутрення переменная лога. Не использовать явно.

	bool LogStart(); 
	void LogFinish(); 
	void WriteLog(char* frmt, ...);
	void printLog(char* frmt, ...);
//---------------------------------------------------------------------------------------------

#define MAX_NAME_LEN 80
#define MAX_FILENAME_LEN 1024

#define SAFEDELA(x)   {if (x){delete [] x; x = NULL;}}

#define MSGTYPE		BYTE

#define STATE_DISCONNECT  0			// Отсоединен
#define STATE_LOGIN		  1			// Послал запрос на логин
#define STATE_LOGINOK	  2			// Получил ответ на логин. Послал NETMSG_STARTCONSOLE
#define STATE_CONNECTED   3			// Присоединен ке серверу
#define STATE_WAIT_ANSWER 4			// Ожидаем ответа от сервера

void *zlib_alloc(void *opaque, unsigned int items, unsigned int size);
void zlib_free(void *opaque, void *address);

int getstring(char * buffer, size_t max_len);

#endif 
