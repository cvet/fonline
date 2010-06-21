#include "stdafx.h"

#define MAX_LEXEM_LEN	256
#define MAX_DICTIONARY  256

/*
const char * dict[] = 
{
	"exit", 
	"connect",
	"server"
};

const char * disc_desc[] = 
{
	"Exti console\n",
	"Connect to server. The address is typed by \'server\' comand\n",
	"Server address in format: <host:port>"
}; */

#define CFG_FILE_NAME "\\Console.cfg"
#define DICT_FILE_NAME "comands.txt"

#define HELP_DIR	"Help\\"

//--------------Тип лексемы--------------------------------------
#define TYPE_NONE   0 
#define TYPE_LEXEM  1
#define TYPE_OPER   2
#define TYPE_SKOBKA 3
#define TYPE_SPACE  4

int gettype(char c);
//---------------------------------------------------------------

#define MAX_LINE_LENGTH 2048
#define MAX_LEXEMS 32		// Макс. количество лексем в строке.

//------------------Строковые функции----------------------------
void strtoup(char * str, size_t len);
void strtolow(char * str, size_t len);
int isskobka(char c);
int islexem(char c);
int isoper(char c);
//---------------------------------------------------------------

typedef map<string, CmdID, less<string> > d_map;
typedef map<CmdID, string, less<CmdID> >  string_map;	

CmdID getCommand(char * s);

bool InitLexAnalyser(void);							// Инициализация лексического анализатора
int loadDictionary(d_map & mp, char * file_name);	// Загрузка словаря.

int getstring(char * buffer, size_t max_len);

char * getLexBuffer(void);
void restoreLexBuffer(void);
int getLexCount(void);

char * extractLexems(char * buffer, size_t len);
char * getLexem(void);
bool putLexem(char * s);
bool loadCmdHelp(char * name);

// Соединение оставшихся лексем в 1 строку.
// buf - выходная строка. inlen - размер буфера
size_t ConcateLexems(char * buf, size_t inlen);
