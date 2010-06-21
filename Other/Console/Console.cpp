// Console.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "dictionary.h"
#include "BufMngr.h"
#include "net.h"

CBufMngr bout;
CBufMngr bin;

#include <iostream>
using namespace std;

//SOCKET client_s = INVALID_SOCKET;	// сокет клиента

// Сервер
char host[MAX_NAME_LEN] = "";
WORD port = 0;
// Клиент
#define MAX_LOGIN 10
char user[MAX_LOGIN] = "root";
char passwd[MAX_LOGIN] = "root";
char cfg_fname[MAX_FILENAME_LEN];

void Loop();

bool loop = true;

//HANDLE hDump = CreateFileA("dump.dat", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, NULL);

// Функция считывания строки
int getstring(char * buffer, size_t max_len);

// Обработка вводимой команды
void Process_Command(void);
// Обработка ответа сервера на запрос.
void Process_ServerAnswer(void);
// Обработка команды сервера
void Process_Server(void);
// Обработка тек. состояния
void Process_CurrentState(void);

void cmdShow(void);			// Обработка команды show
void cmdConnect(void);		// Обработка команды connect
void cmdHelp(void);			// Обработка команды help
void cmdSet(void);			// Обработка команды set
void cmdExec(void);			// Обработка команды exec

extern d_map dict;
extern string_map desc_map;

void Send_Login(void);		// Посылка логина и пароля
void Send_Console(void);	// посылка запроса на начало работы 
// Послылка команды серверу
void Send_Cmd(BYTE cmd, const char * buf, WORD len);

// Вспомогательная функция. Используется для отображения 
// некорректных агрументов.
void PrintErrorArguments(void);

int state = STATE_DISCONNECT;

DWORD start = 0;			// Начальное время операции

#define DISCONNECT() {Disconnect(); state = STATE_DISCONNECT;}

int main(int argc, char* argv[])
{
	LogStart();

	GetCurrentDirectoryA(MAX_FILENAME_LEN, cfg_fname);
	strncat(cfg_fname, CFG_FILE_NAME, MAX_FILENAME_LEN);
	printLog("Welcome to the FOnline console!\n");
	printLog("Load config from %s\n", cfg_fname);

	// Загрузить настройки из конфига.
	GetPrivateProfileStringA("network","host","error",host,MAX_NAME_LEN,cfg_fname);
	port=GetPrivateProfileIntA("network","port",0,cfg_fname);

	if (!strcmp(host, "error") || !port) 
	{
		printLog("Invalid server address\n");
		goto error_exit;
	}	

	strtolow(host, strlen(host));

	if (!strcmp(host, "localhost"))
		strcpy(host, "127.0.0.1");
	
	printLog("Server address is %s:%d\n", host, port);
	
	if (InitNet() == INVALID_SOCKET)
	{
		printLog("Сетевой интерфейс не был инициализирован. Выход\n");
		goto error_exit;
	}

	if (!InitLexAnalyser())
	{
		printLog("Лексический анализатор не был инициализирован. Выход\n");
		goto error_exit;
	}

	Loop();

error_exit:
	DISCONNECT();
	FinishNet();
	LogFinish();
	getch();
	return -1;
}

void Loop()
{
	timeval tv;
	tv.tv_sec  = 0;
	tv.tv_usec = 0;

	char buffer[MAX_LINE_LENGTH];
	int len = 0;

	loop = true;
	WriteLog("Начало основного цикла\n");

	printLog("Type help to get help\n\n");
	while (loop)
	{
		Process_CurrentState();

		// Проверяем готовность сокета
		CheckNet(tv);

		if (ISSetWrite())
		{
			if (bout.NeedProcess())
			{
				if (Output(bout) == false)
				{
					printLog("\nThe message was not send. Disconnecting......\n");
					DISCONNECT();
				}
			}
		}
		
		if (ISSetRead())
		{
			if (InputCompressed(bin) == false)
			{
				printLog("\nData was not receive from server. Disconnecting....\n");
				DISCONNECT();
			}
			// Обрабатываем команду сервера
			Process_Server();
		} 

		if (!(state == STATE_DISCONNECT || state == STATE_CONNECTED)) continue;

		printLog("> ");
		len = getstring(buffer, MAX_LINE_LENGTH);
		WriteLog("%s", buffer);
		char * s = extractLexems(buffer, len);
		Process_Command();

/*		if (kbhit())
		{
			c = _getch();
			if (c == 13) c = '\n';		// Почему то вместо конца строки возвращает 13
			buffer[len++] = c;
			printLog("%c", c);
			if (c == '\n')				// Конец строки
			{
				buffer[len] = 0;
				WriteLog("%s", buffer);
				// Разбиваемна лексемы.
				char * s = extractLexems(buffer, len);
				// s содержит указатель на нераспакованные лекскемы
				ProcessCommand();
				// Очищаем очередь лексем
				restoreLexBuffer();
				len = 0;
				// Для красоты
				if (loop) printLog("> ");
			} else if (c == 8)			// Backspace
			{
				cur_pos--;
				fsetpos(stdout, &cur_pos);
				buffer[--len] = 0;
				printLog(" ");
			} else fgetpos(stdout, &cur_pos);

		} 
*/
	}
	WriteLog("Завершение основного цикла\n");
}

//////////////////////////////////////////////////////////////////////////
// Блок cmdXXXX
//////////////////////////////////////////////////////////////////////////
void cmdExec(void)
{
	// Буфер для выходной строки
	char buffer[MAX_LINE_LENGTH];

	int len = ConcateLexems(buffer, MAX_LINE_LENGTH);

	// Отправляем.
	Send_Cmd(CONSOLE_CMD_EXEC, buffer, len+1);
}


void cmdConnect(void)
{
	if (state != STATE_DISCONNECT)
	{
		printLog("Current state is not STATE_DISCONNECTED\n");
		return;
	}

	DWORD start = 0;

	int res = Connect(host, port);
	if (res == NET_ERR_NOT_INIT)	
	{
		if ( InitNet() == INVALID_SOCKET)
		{
			WriteLog("Ошибка инициализации сетевого интерфейса\n");
			return;
		}
		res = Connect(host, port);
	} 
	if (res != NET_OK)
	{
		WriteLog("Невозможно установить соединение с сервером %s%d\n", host, port);
		return;
	}

	Send_Login();
}

void PrintErrorArguments(void)
{
	if (getLexCount() > 0)
	{
		printLog("Invalid arguments: \n");
		while (getLexCount()) printLog("\t\t%s\n", getLexem());
		return;
	}
}

void cmdShow(void)
{
	char * arg1 = getLexem();
	string s = string(arg1);
	if (s == "script")
	{
		char buffer[MAX_LINE_LENGTH];
		size_t len = ConcateLexems(buffer, MAX_LINE_LENGTH);
		Send_Cmd(CONSOLE_CMD_SHOW_SCRIPT, buffer, len + 1);
		return; 
	}

	PrintErrorArguments();

	if (s == "host") printLog("\t server host: %s\n", host); 
	else if (s == "port") printLog("\t server port: %d\n", port);
	else if (s == "server") printLog("\t server address is %s:%d\n", host, port);
	else if (s == "state")
	{
		if (state == STATE_DISCONNECT) printLog("\t current state is STATE_DISCONNECTED\n");
		else if (state == STATE_CONNECTED) printLog("\t current state is STATE_CONNECTED\n");
	} else Send_Cmd(CONSOLE_CMD_SHOW, s.c_str(), s.length() + 1);
}

void cmdHelp(void)
{
	char * s = getLexem();
	if (s == NULL) 
	{
		printLog("Comands: \n");
		d_map::iterator p = dict.begin();
		string_map::iterator p2;
		CmdID id;
		while (p != dict.end())
		{
			id = p->second;
			p2 = desc_map.find(id);
			if (p2 == desc_map.end())
				printLog("\t %s\n", p->first.c_str());
			else printLog("\t %s - %s\n", p->first.c_str(), p2->second.c_str());
			p++;
		}
		printLog("\nType help \'comand\' to get additional information about comand\n");
	} else 
	{
		d_map::iterator p = dict.find(s);
		if (p == dict.end()) 
		{
			printLog("\n\tInvalid argument: %s\n", s);
			return;
		} else 
		if (loadCmdHelp(s) == false)
		{
			CmdID id = p->second;
			string_map::iterator p2 = desc_map.find(id);
			if (p2 != desc_map.end())
				printLog("\n\t %s \n", p2->second.c_str());			
			else printLog("\n\t This comand has not help\n\n");
		}	
	}
}

void cmdSet(void)
{
	char * s = getLexem();
	if (s == NULL)
	{
		printLog("\n\t Variable is not typed\n");
		return;
	}
	char * val = getLexem();
	if (val == NULL)
	{
		printLog("\n\t Value is not typed\n");
		return;
	}
	
	string str = string(s);
	if (str == "host")
	{
		char buffer[MAX_LINE_LENGTH];
		buffer[0]=0;

		int len = 0;
		while (val && len < MAX_LINE_LENGTH)
		{
			strcat(buffer, val);
			len+=strlen(val);
			val = getLexem();
		}

		strtolow(buffer, strlen(buffer));		
		if (!strcmp(buffer, "localhost")) strcpy(buffer, "127.0.0.1");

		if (inet_addr(buffer) == -1)
		{
			printLog("\n\t Invalid host address\n");
			return;
		}
		strcpy(host, buffer);
		printLog("\n\t New host - %s\n", host);
				
	} else if (str == "port")
	{
		char * tmp;
		if (tmp = getLexem())
		{
			printLog("\n\t Invalid argument %s\n", tmp);
			return;
		}
		if (sscanf(val,"%d", &port) != 1)
		{
			printLog("\n\t Invalid server's port %s\n", val);
			return;
		}
		printLog("\n\t New server's port - %d \n", port);
	} else
	{
		printLog("Unknown variable %s\n", s);
		return;
	}
}


//////////////////////////////////////////////////////////////////////////
//Блок Proces_XXXXX
//////////////////////////////////////////////////////////////////////////

void Process_Server(void)
{
	if (!bin.NeedProcess()) return;

	DWORD cur = GetTickCount();

	MSGTYPE msg;
	bin >> msg;
	switch (msg)
	{
		case NETMSG_LOGMSG:
				printLog("Invalid login or passwr. Disconnecting....\n\n");
				DISCONNECT();
			break;
		case NETMSG_LOGINOK:
				printLog("Console was logined at time %d ms\n", cur - start);
				Send_Console();
				state = STATE_LOGINOK;
			break;	
		case NETMSG_CONSOLE_OK:
				printLog("Console was inited at time %d ms\n\n", cur - start);
				state = STATE_CONNECTED;
			break;
		case NETMSG_CONSOLEMSG:
				Process_ServerAnswer();
			break;
		default:
				printLog("Unknown server command %d\n", msg);
			break;
	}
	bin.reset();
}

void Process_ServerAnswer(void)
{
	DWORD cur = GetTickCount();
	char buf[MAX_LINE_LENGTH];

	if (!bin.NeedProcess())
	{
		printLog("Process_ServerAnswer() invalid cmd\n");
		return;
	}
	BYTE cmd;
	bin >> cmd;
	if (!bin.NeedProcess())
	{
		printLog("Process_ServerAnswer() invalid len\n");
		return;
	}
	WORD len;
	bin >> len;

	if (bin.pos - bin.read_pos != len)
	{
		printLog("Process_ServerAnswer() invalid buffer\n");
		return;
	}

	if (len)
	{
		bin.pop(buf, len);
	}

	switch(cmd)
	{
		case CONSOLE_RES_OK:
				printLog("Server's answer at time %d ms: %s\n", cur-start, buf);
				state = STATE_CONNECTED;
			break;
	}

}

void Process_CurrentState(void)
{
	DWORD cur = GetTickCount();
	switch(state)
	{
	case STATE_LOGIN:
		if (cur - start >= 5000) 
		{
			printLog("Server don't answer. disconnecting\n");
			DISCONNECT();
		}
		break;
	case STATE_LOGINOK:
		if (cur - start >= 5000)
		{
			printLog("Server don't answer. disconnecting....\n");
			DISCONNECT();
		}
		break;
	case STATE_WAIT_ANSWER:
		if (cur - start >= 5000)
		{
			printLog("Server don't answer. disconnecting....\n");
			DISCONNECT();
		}
		break;
	}
}

void Process_Command(void)
{
	char * lex = getLexem();
	if (!lex) return;
	CmdID cmd = getCommand(lex);
	switch(cmd)
	{
	case CONSOLE_CMD_EXIT: printLog("exiting ....\n"); loop = false; break;
	case CONSOLE_CMD_CONNECT: cmdConnect(); break;
	case CONSOLE_CMD_HELP: cmdHelp(); break;
	case CONSOLE_CMD_SHOW: cmdShow(); break;
	case CONSOLE_CMD_SET: cmdSet(); break;
	case CONSOLE_CMD_DISCONNECT: printLog("Disconnecting...\n");DISCONNECT(); break;
	case CONSOLE_CMD_EXEC: cmdExec(); break;
	default: printLog("Invalid command\n"); 
	}
	// Очищаем очередь лексем
	restoreLexBuffer();
}

//////////////////////////////////////////////////////////////////////////
// Блок Send_XXXX
//////////////////////////////////////////////////////////////////////////
void Send_Login(void)
{
	// Send login
	MSGTYPE msg = NETMSG_LOGIN;
	bout << msg;
	bout.push(user, MAX_LOGIN);
	bout.push(passwd, MAX_LOGIN);
	start = GetTickCount();
	state = STATE_LOGIN;
	WriteLog("Выполняется запрос на авторизацию %s:%s\n", user, passwd);
}

void Send_Console(void)
{
	MSGTYPE msg = NETMSG_STARTCONSOLE;
	bout << msg;
	start = GetTickCount();
	state = STATE_LOGINOK;
	WriteLog("Выполняется запрос на началоа работы \n");
}


void Send_Cmd(BYTE cmd, const char * buf, WORD len)
{
	if (state == STATE_DISCONNECT)
	{
		printLog("Connection with server is NULL. Command was not sent\n");
		return;
	}
	
	MSGTYPE msg = NETMSG_CONSOLEMSG;
	bout << msg;
	bout << cmd;
	bout << len;
	bout.push((char*)buf, len);
	start = GetTickCount();
	state = STATE_WAIT_ANSWER;		// Ждем ответа
}


