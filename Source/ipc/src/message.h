#ifndef MESSAGE_H
#define MESSAGE_H


#ifdef _WIN32
	#include <windows.h>
	#define PID DWORD
#else
	#include <sys/types.h>
	#define PID pid_t
#endif


typedef struct {
	int type;
	union {
		PID pid;
		char* subject;
	};

	char* data;
	size_t len;
} Message;

#include "ipc.h"

Message* messageCreate(char* data, size_t len);
void messageSetPID(Message* msg, PID pid);
void messageSetSubject(Message* msg, char* subject);
void messageDestroy(Message* msg);

#endif
