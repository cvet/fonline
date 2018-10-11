#include <stdio.h>
#include <stdlib.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include "connection.h"

#define MAX_CB 50
#define SUBS_LEN 10
#ifdef _WIN32
		 #define getpid GetCurrentProcessId
	#define TID HANDLE
	#define THREAD_RET_TYPE DWORD WINAPI
	#define PIPE_PREFIX "\\\\.\\pipe\\"
#else
	#define TID pthread_t
	#define THREAD_RET_TYPE void*
	#define PIPE_PREFIX "/tmp/"
#endif
#define PIPE_PREFIX_LEN strlen(PIPE_PREFIX) + 1

#ifdef _WIN32
void usleep(__int64 usec)
{
		HANDLE timer;
		LARGE_INTEGER ft;

		ft.QuadPart = -(10*usec); // Convert to 100 nanosecond interval, negative value indicates relative time

		timer = CreateWaitableTimer(NULL, TRUE, NULL);
		SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
		WaitForSingleObject(timer, INFINITE);
		CloseHandle(timer);
}
#endif


//TODO: Implement hashmap
struct ConnCallbackElement {
	Connection* conn;
	ConnectionCallback cb;
	TID tid;
	struct dispatcherArgs* args;
};

struct dispatcherArgs{
	ConnectionCallback cb;
	int type;
	#ifdef _WIN32
		HANDLE hPipe;
	#else
		char* path;
	#endif
	int* cont;
	char** subs;
	int* numSubs;
};

struct cbCallerArgs{
	Message* msg;
	ConnectionCallback cb;
};

struct writerArgs{
	Message* msg;
	char* path;
};

struct ConnCallbackElement* cbs[MAX_CB];
int numCallbacks;

int nextEmpty(char** strs, int len){
	int i;
	for (i = 0; i < len; i++) {
		if(strs[i] == NULL){
			return i;
		}
	}
	return -1;
}

int findEqual(char** strs, char* other, int len){
	int i;
	for (i = 0; i < len; i++){
		if(strs[i] && strcmp(strs[i], other) == 0){
			return i;
		}
	}
	return -1;
}

THREAD_RET_TYPE writer(void* args){
	struct writerArgs* argz = args;

	#ifdef _WIN32
		HANDLE hPipe = CreateFile(TEXT(argz->path),
								GENERIC_READ | GENERIC_WRITE,
								0,
								NULL,
								OPEN_EXISTING,
								0,
								NULL);
	#else
		int fd = open(argz->path, O_WRONLY);
	#endif


	size_t* name_len = argz->msg->data + argz->msg->len;
	size_t len = sizeof(Message) + argz->msg->len + sizeof(size_t) + *name_len;
	char* data = malloc(len);

	memcpy(data, argz->msg, sizeof(Message));
	memcpy((data + sizeof(Message)), argz->msg->data, len - sizeof(Message));


	if (argz->msg->type == CONN_TYPE_SUB){
		data = realloc(data, len + sizeof(size_t) + strlen(argz->msg->subject) + 1);
		size_t sub_len = strlen(argz->msg->subject) + 1;
		memcpy((data + len), &sub_len, sizeof(size_t));
		memcpy((data + len + sizeof(size_t)), argz->msg->subject, strlen(argz->msg->subject) + 1);
		len += sizeof(size_t) + strlen(argz->msg->subject) + 1;
	}

	#ifdef _WIN32
		WriteFile(hPipe, data, len, NULL, NULL);
		CloseHandle(hPipe);
	#else
		write(fd, data, len);
		close(fd);
	 #endif

	free(data);
	free(argz->path);
	free(argz->msg->data);
	messageDestroy(argz->msg);
	free(argz);

	return 0;
}

THREAD_RET_TYPE cbCaller(void* args){
	struct cbCallerArgs* argz = args;

	(argz->cb)(argz->msg);

	if(argz->msg->type == CONN_TYPE_SUB){
		free(argz->msg->subject);
	}
	free(argz->msg->data);
	free(argz->msg);
	free(argz);

	return 0;
}

void dispatch(ConnectionCallback cb, Message* msg){
	struct cbCallerArgs* cbargs = malloc(sizeof(struct cbCallerArgs));
	cbargs->cb = cb;
	cbargs->msg = msg;

	#ifdef _WIN32
		CreateThread(NULL, 0, cbCaller, cbargs, 0, NULL);
	#else
		pthread_t tid;
		pthread_create(&tid, NULL, cbCaller, cbargs);
	#endif
}

THREAD_RET_TYPE dispatcher(void* args){
	struct dispatcherArgs* argz = args;

	#ifdef _WIN32
		ConnectNamedPipe(argz->hPipe, NULL);
	#else
		int fd = open(argz->path, O_RDONLY | O_NONBLOCK);
	#endif

	while (*(argz->cont)){

		Message* msg = malloc(sizeof(Message));
		#ifdef _WIN32
			int code = ReadFile(argz->hPipe, msg, sizeof(Message), NULL, NULL);
		#else
			int code = read(fd, msg, sizeof(Message));
		#endif

		#ifndef _WIN32
			if(code == -1){
				code = 0;
			}
		#endif

		char* data;
		if(code && msg->len < MAX_MSG_SIZE){
			data = malloc(msg->len + sizeof(size_t));
			#ifdef _WIN32
				ReadFile(argz->hPipe, data, msg->len + sizeof(size_t), NULL, NULL);
			#else
				read(fd, data, msg->len + sizeof(size_t));
			#endif

			size_t* name_len = data + msg->len;
			data = realloc(data, msg->len + sizeof(size_t) + *name_len);
			#ifdef _WIN32
				ReadFile(argz->hPipe, data + msg->len + sizeof(size_t), *name_len, NULL, NULL);
			#else
				read(fd, data + msg->len + sizeof(size_t), *name_len);
			#endif

			msg->data = data;
		}else{
				free(msg);
				continue;
		}

		if(msg->type == CONN_TYPE_SUB){
			size_t* sub_len = malloc(sizeof(size_t));
			#ifdef _WIN32
				ReadFile(argz->hPipe, sub_len, sizeof(size_t), NULL, NULL);
			#else
				read(fd, sub_len, sizeof(size_t));
			#endif
			char* subject = malloc(*sub_len);
			#ifdef _WIN32
				ReadFile(argz->hPipe, subject, *sub_len, NULL, NULL);
			#else
				read(fd, subject, *sub_len);
			#endif
			msg->subject = subject;
		}

		switch (msg->type) {
			case CONN_TYPE_ALL:
				dispatch(argz->cb, msg);
				break;
			case CONN_TYPE_SUB:
				if (findEqual(argz->subs, msg->subject, *(argz->numSubs)) != -1){
					dispatch(argz->cb, msg);
				}else{
					free(msg->subject);
					free(msg);
					free(data);
				}
				break;
			case CONN_TYPE_PID:
				if (msg->pid == getpid()){
					dispatch(argz->cb, msg);
				} else {
					free(msg);
					free(data);
				}
				break;
			default:
				free(msg);
				free(data);

			usleep(100);
		}

	}
	#ifdef _WIN32
		DisconnectNamedPipe(argz->hPipe);
	#else
		close(fd);
	#endif

	return 0;
}

int findFreeCBSlot(){
	if(numCallbacks < MAX_CB){
		int i;
		for (i = 0; i < MAX_CB; i++){
			if (cbs[i] == NULL){
				return i;
			}
		}
	}
	return -1;
}

int findInCBSlot(Connection* conn){
	int i;
	for (i = 0; i < MAX_CB; i++){
		if (cbs[i]->conn == conn){
			return i;
		}
	}
	return -1;
}

Connection* connectionCreate(char* name, int type){
	Connection* ret = malloc(sizeof(Connection));
	ret->name = malloc(strlen(name)+1);
	memcpy(ret->name, name, strlen(name)+1);
	ret->type = type;

	char* path = malloc(strlen(name) + 1 + PIPE_PREFIX_LEN);
	memcpy(path, PIPE_PREFIX, PIPE_PREFIX_LEN);
	strcat(path, name);
	#ifdef _WIN32
	ret->hPipe = CreateNamedPipe(TEXT(path),
								PIPE_ACCESS_DUPLEX | PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
								PIPE_NOWAIT,
								1,
								1024 * 16,
																	1024 * 16,
																	NMPWAIT_USE_DEFAULT_WAIT,
																	NULL);
	#else
		mkfifo(path, 0777);
	#endif
	free(path);

	ret->subscriptions = malloc(sizeof(char*) * SUBS_LEN);

	int i;
	for (i = 0; i < SUBS_LEN; i++){
		ret->subscriptions[i] = NULL;
	}

	ret->numSubs = SUBS_LEN;

	return ret;
}

Connection* connectionConnect(char* name, int type){
	Connection* ret = malloc(sizeof(Connection));
	ret->name = malloc(strlen(name) + 1);
	memcpy(ret->name, name, strlen(name) + 1);
	ret->type = type;

	#ifdef _WIN32
		char* path = malloc(strlen(name) + 1 + PIPE_PREFIX_LEN);
	memcpy(path, PIPE_PREFIX, PIPE_PREFIX_LEN);
	strcat(path, name);

		ret->hPipe = CreateFile(TEXT(path),
								GENERIC_READ | GENERIC_WRITE,
								0,
								NULL,
								OPEN_EXISTING,
								0,
								NULL);

		free(path);
	#endif

	ret->subscriptions = malloc(sizeof(char*) * SUBS_LEN);

	int i;
	for (i = 0; i < SUBS_LEN; i++){
		ret->subscriptions[i] = NULL;
	}

	ret->numSubs = SUBS_LEN;

	return ret;
}

void connectionStartAutoDispatch(Connection* conn){
	int i = findInCBSlot(conn);

	cbs[i]->args = malloc(sizeof(struct dispatcherArgs));


	cbs[i]->args->cont = malloc(sizeof(int));
	*(cbs[i]->args->cont) = 1;

	#ifdef _WIN32
		cbs[i]->args->hPipe = conn->hPipe;
	#else
		cbs[i]->args->path = malloc(strlen(conn->name) + 1 + PIPE_PREFIX_LEN);
		memcpy(cbs[i]->args->path, PIPE_PREFIX, PIPE_PREFIX_LEN);
		strcat(cbs[i]->args->path, conn->name);
	#endif

	cbs[i]->args->cb = cbs[i]->cb;

	cbs[i]->args->subs = conn->subscriptions;
	cbs[i]->args->numSubs = &(conn->numSubs);

	#ifdef _WIN32
		cbs[i]->tid = CreateThread(NULL, 0, dispatcher, cbs[i]->args, 0, NULL);
	#else
		pthread_create(&(cbs[i]->tid), NULL, dispatcher, cbs[i]->args);
	#endif
}

void connectionStopAutoDispatch(Connection* conn){
	int i = findInCBSlot(conn);

	*(cbs[i]->args->cont) = 0;
	#ifdef _WIN32
		WaitForSingleObject(cbs[i]->tid, INFINITE);
	#else
		pthread_join(cbs[i]->tid, NULL);
	#endif

	free(cbs[i]->args->cont);
	#ifndef _WIN32
		free(cbs[i]->args->path);
	#endif
	free(cbs[i]->args);
}

void connectionSetCallback(Connection* conn, ConnectionCallback cb){
	int i = findFreeCBSlot();

	cbs[i] = malloc(sizeof(struct ConnCallbackElement));
	cbs[i]->conn = conn;
	cbs[i]->cb = cb;
}

ConnectionCallback connectionGetCallback(Connection* conn){
	int i = findInCBSlot(conn);

	return cbs[i]->cb;
}

void connectionRemoveCallback(Connection* conn){
	int i = findInCBSlot(conn);

	free(cbs[i]);
	cbs[i] = NULL;
}

void connectionSend(Connection* conn, Message* msg){
	char* path = malloc(strlen(conn->name) + 1 + PIPE_PREFIX_LEN);
	memcpy(path, PIPE_PREFIX, PIPE_PREFIX_LEN);
	strcat(path, conn->name);

	struct writerArgs* args = malloc(sizeof(struct writerArgs));
	args->msg = malloc(sizeof(Message));
	memcpy(args->msg, msg, sizeof(Message));

	size_t name_len = strlen(conn->name) + 1;
	args->msg->data = malloc(msg->len + sizeof(size_t) + name_len);
	memcpy(args->msg->data, msg->data, msg->len);
	memcpy(args->msg->data + msg->len, &name_len, sizeof(size_t));
	memcpy(args->msg->data + msg->len + sizeof(size_t), conn->name, name_len);

	if (msg->type == CONN_TYPE_SUB){
		args->msg->subject = malloc(strlen(msg->subject) + 1);
		memcpy(args->msg->subject, msg->subject, strlen(msg->subject) + 1);
	}

	args->path = path;

	#ifdef _WIN32
		CreateThread(NULL, 0, writer, args, 0, NULL);
	#else
		pthread_t tid;
		pthread_create(&tid, NULL, writer, args);
	#endif
}

void connectionSubscribe(Connection* conn, char* subject){
	int i = nextEmpty(conn->subscriptions, conn->numSubs);

	if (i == -1){
		conn->subscriptions = realloc(conn->subscriptions, conn->numSubs + SUBS_LEN);
		int i;
		for(i = conn->numSubs; i < conn->numSubs + SUBS_LEN; i++){
			conn->subscriptions[i] = NULL;
		}
		conn->numSubs += SUBS_LEN;
		connectionSubscribe(conn, subject);
		return;
	}

	conn->subscriptions[i] = malloc(strlen(subject) + 1);
	memcpy(conn->subscriptions[i], subject, strlen(subject) + 1);
}

void connectionRemoveSubscription(Connection* conn, char* subject){
	int i = findEqual(conn->subscriptions, subject, conn->numSubs);
	if (i >= 0){
		free(conn->subscriptions[i]);
		conn->subscriptions[i] = NULL;
	}
}

void connectionClose(Connection* conn){
	#ifndef _WIN32
		char* path = malloc(strlen(conn->name) + 1 + PIPE_PREFIX_LEN);
		memcpy(path, PIPE_PREFIX, PIPE_PREFIX_LEN);
		strcat(path, conn->name);
		unlink(path);
	#endif
}

void connectionDestroy(Connection* conn){
	int i;
	for(i = 0; i<conn->numSubs; i++){
		if (conn->subscriptions[i] != NULL){
			free(conn->subscriptions[i]);
		}
	}
	free(conn->subscriptions);

	char* path = malloc(strlen(conn->name) + 1 + PIPE_PREFIX_LEN);
	memcpy(path, PIPE_PREFIX, PIPE_PREFIX_LEN);
	strcat(path, conn->name);
	#ifdef _WIN32
	DisconnectNamedPipe(conn->hPipe);
	CloseHandle(conn->hPipe);
	#endif
	free(path);

	free(conn->name);
	free(conn);
}
