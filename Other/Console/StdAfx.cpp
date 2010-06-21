// stdafx.cpp : source file that includes just the standard includes
//	Console.pch will be the pre-compiled header
//	stdafx.obj will contain the pre-compiled type information

#include "stdafx.h"

volatile bool busy = false;

bool LogStart() 
{
	if(log || !logging) return true;
	
	log = fopen(LOG_FILE_NAME, "w");

	WriteLog("ЛОГ пишется в файл %s\n", LOG_FILE_NAME);
		
	return log != NULL;
}
	
void LogFinish()
{
	if(!log || !logging) return;
	WriteLog("Завершение работы лога\n");
		
	fclose(log);
	log = NULL;
}
	
void WriteLog(char* frmt, ...)
{
	while (busy) continue;

	busy = true;

	if(!logging || !log) return;
	
	va_list list;
		
	va_start(list, frmt);
	vfprintf(log, frmt, list);
	va_end(list);
	fflush(log);

	busy = false;
	
}

void printLog(char* frmt, ...)
{
	char buffer[1024];
	va_list list;

	while (busy) continue;
	
	busy = true;
	
	va_start(list, frmt);
	_vsnprintf(buffer, sizeof(buffer), frmt, list);
	va_end(list);
	printf("%s", buffer);
	if (!log || !logging) {busy = false; return;}
	fprintf(log, "%s", buffer);
	fflush(log);
	busy = false;
}

void *zlib_alloc(void *opaque, unsigned int items, unsigned int size)
{
    return calloc(items, size);
}

void zlib_free(void *opaque, void *address)
{
    free(address);
}


int getstring(char * buffer, size_t max_len)
{
	if (!buffer) return -1;
	size_t i = 0;
	int c = 0;
	while (i < max_len - 1 )
	{
		c = getchar();
		buffer[i] = (char)c;
		i++;
		if (c == EOF || c == '\n') break;
	}
	// Ждем конца строки.
	while (c != '\n' && c != EOF) c = getchar();
	
	buffer[i] = 0;
	return i;
}

