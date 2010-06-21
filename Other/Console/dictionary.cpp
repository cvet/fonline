
#include "stdafx.h"
#include "dictionary.h"

#include <fstream>

char lexems[MAX_LEXEMS][MAX_LEXEM_LEN];
d_map			dict;
string_map		desc_map;
stack<char*>	lex_buffer;
queue<char*>	lex_queue;

const char * getCmdDesc(CmdID cmd);

int loadDictionary(d_map & mp, char * file_name)
{
	if (!file_name) return FALSE;

	FILE * file = fopen(file_name, "r");
	if (!file) 
	{
		WriteLog("не могу открыть файл с командами %s\n", file_name);
		return -1;
	}
	
	char buf[MAX_LINE_LENGTH];
	
	char name[MAX_LEXEM_LEN];
	char desc[MAX_LINE_LENGTH];
	
	WORD id;
	
	while (!feof(file))
	{
		if (fscanf(file, "%s", buf) != 1) 
			goto error_exit;
		if (buf[0] != '#' ) 			  goto error_exit;
		
		// ID переменной
		if (sscanf(&buf[1], "%d", &id) == 0) goto error_exit;
	
		// Имя переменной
		if (fscanf(file, "%s", name) != 1) goto error_exit;
		
		// Читаем описание переменной
		int i = 0;
		int j = 0;
		desc[0] = 0;
		
		char ch[2];
		ch[1] = 0;
		
		while (!feof(file) && i < MAX_LINE_LENGTH)
		{
			if (fscanf(file, "%c", ch) != 1) goto error_exit;
			if (j == 0)
			{
				// Читаем открывающуюся скобку.
				if (ch[0] == '{') j = 1; 
				else if (ch[0] == '}' ) goto error_exit;
			} else if (j == 1)
			{
				if (ch[0] == '}') break;
				else
				{
					// читаем символы описания
					desc[i] = ch[0];
					i++;
				}
			}
		}
		
		desc[i] = 0;
		if (ch[0] != '}') goto error_exit;

		string nm = string(name);
		string ds = string(desc);

		d_map::iterator p = mp.find(nm);
		if (p == mp.end())
		{
			// Добавляем в список новую переменную
			mp.insert(d_map::value_type(nm, id));
			desc_map.insert(string_map::value_type(id, ds));
		} else printLog("Словарь содержит повторяющиеся команды: %s\n", nm.c_str());
	}
error_exit:
//	printLog("ОШИБКА при загрузке словаря\n");
	fclose(file);
	return mp.size();
}

void strtoup(char * str, size_t len)
{
	if (!str) return;
	for (int i =0; i < len; i++ )
		str[i] = toupper(str[i]);
	
}
void strtolow(char * str, size_t len)
{
	if (!str) return;
	for (int i =0; i < len; i++ )
		str[i] = tolower(str[i]);
}

int isskobka(char c)
{
	return c == '{' || c == '[' || c == '(' || c == '}' || c == ']' || c == ')';
}

int islexem(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || 
		(c >= 'а' && c <= 'я') || (c >= 'А' && c <= 'Я') || (c == 'ё') ||
		(c >= '0' && c <= '9') || c == '_';
}

int isoper(char c)
{
	return ispunct(c);
	
}

CmdID getCommand(char * s)
{
	if (!s) return 0;
	string str = string(s);
	d_map::iterator p = dict.find(str);
	if (p == dict.end()) return 0;
	return p->second;
}

int gettype(char c)
{
	if (isspace(c)) return TYPE_SPACE;
	else if (isskobka(c)) return TYPE_SKOBKA;
	else if (islexem(c)) return TYPE_LEXEM;
	else if (isoper(c)) return TYPE_OPER;
	return TYPE_NONE;
}

// Работает пока буфер лексем не будет заполнен полностью.
// или не закончится входная строка.
// Вовзращает указатель, где остановился.
// NULL в случае ошибки
char * extractLexems(char * str, size_t len)
{
	if (!str) return NULL;
	char c;
	size_t last_pos = 0;
	int last_type = gettype(str[last_pos]);

	char buffer[MAX_LINE_LENGTH];

	size_t l;
	int type;

//	bool not_space = false;

	int lexcount = getLexCount();

	for (size_t i = 0; i < len && lexcount < MAX_LEXEMS; i++)
	{
		c = str[i];
		type = gettype(c);
		if (type != last_type)
		{
			if (last_type != TYPE_SPACE)
			{
				// копируем.
				l = i - last_pos;
				memcpy(buffer, str + last_pos, i - last_pos );
				buffer[l] = 0;
				if (l > MAX_LEXEM_LEN) 
				{
					WriteLog("Лексема %s была урезана до %d символов\n", buffer, MAX_LEXEM_LEN - 1);
					l = MAX_LEXEM_LEN - 1;
					buffer[l] = 0;
				}
				
				putLexem(buffer);
				lexcount++;
				WriteLog("распакована лексема %s\n", buffer);
			} 
		/*	else
			{
				// Всего 1 пробел
				strcpy(buffer, " ");
				putLexem(buffer);
				lexcount++;
				WriteLog("Распакован пробел\n");
			}
		*/
			

			last_pos = i;
			last_type = type;
		}
	}	

	return str + i;
}

char * getLexem(void)
{
	//if (i >= lexcount) return NULL;
	//else return lexems[i];
	if (lex_queue.size() != 0)
	{
		// Освободим место в буферер.
		char * s = lex_queue.front();
		lex_buffer.push(s);
		lex_queue.pop();
		return s;
	}
	return NULL;
}

bool putLexem(char * s)
{
	if (getLexCount() >= MAX_LEXEMS || !s) return false;
	else 
	{
		char * buf = getLexBuffer();
		strcpy(buf, s);
		lex_queue.push(buf);
		char * t = lex_queue.front();
		return true;
	}
}

bool InitLexAnalyser()
{
	WriteLog("Инициализация лексического анализатора\n");
	
	for (int i = 0; i < MAX_LEXEMS; i++) lex_buffer.push(lexems[i]);
	
	// Инициализация словаря.
	//	for 

	int cnt = loadDictionary(dict, DICT_FILE_NAME);

	WriteLog("Загружен словарь на %d команд\n", cnt);
	
	WriteLog("Инициализация лексического анализатора завершена успешно\n");
	return true;
}

char * getLexBuffer(void)
{
	if (lex_buffer.size() != 0)	 
	{
		char * buf = lex_buffer.top();
		lex_buffer.pop();
		return buf;
	}	
	return NULL;
}

void restoreLexBuffer()
{
	for (int i = MAX_LEXEMS - getLexCount(); i < MAX_LEXEMS; i++)
	{
		lex_buffer.push(lexems[i]);
		lex_queue.pop();
	}
}

int getLexCount(void)
{
	return lex_queue.size();
}

const char * getCmdDesc(CmdID cmd)
{
	string_map::iterator p = desc_map.find(cmd);
	if (p == desc_map.end()) return NULL;
	else return p->second.c_str();
}

bool loadCmdHelp(char * name)
{
	char file_name[MAX_FILENAME_LEN];
	char buffer[MAX_LINE_LENGTH];

	sprintf(file_name, "%s%s.hlp", HELP_DIR, name);

	FILE * file = fopen(file_name, "r");
	if (!file)
	{
		WriteLog("не могу открыть файл %s\n", file_name);
		return false;
	}

	printLog("\n-----------------Help----------------------------------\n\n\n",name);

	while (!feof(file))
	{
		buffer[0] = 0;
		fgets(buffer, MAX_LINE_LENGTH, file);
		printLog("%s", buffer);
	}

	printLog("\n\n\n-------------------------------------------------------\n");	

	fclose(file);
	
	return true;
}

size_t ConcateLexems(char * buf, size_t inlen)
{
	if (!buf || !inlen)
	{
		WriteLog("ConcateLexem() необходимо задать выходной буфер\n");
		return 0;
	}	

	buf[0]=0;
	int totallen = 0;
	int len;
	
	BYTE last_type = 255;		// Неизвестный тип 
	BYTE type;
	
	char * s = getLexem();
	if (!s)
	{
		printLog("You must exec script statement\n");
		return 0;
	}
	len = strlen(s);
	while (s && (totallen + len + 1) < inlen)
	{
		len = strlen(s);
		
		type = gettype(s[0]);
		if (type == last_type)
		{
			// Если типы лексем не совпадают, то нужен пробел
			strcat(buf, " ");
			strcat(buf, s);
			totallen += len + 1;
		} else 
		{
			strcat(buf, s);
			totallen += len;
		}
		
		s = getLexem();
		last_type = type;
	}
	printLog("script: %s\n", buf);
	return strlen(buf);
}


