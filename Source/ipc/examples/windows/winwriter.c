#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "ipc.h"

void __usleep(__int64 usec)
{
		HANDLE timer;
		LARGE_INTEGER ft;

		ft.QuadPart = -(10*usec); // Convert to 100 nanosecond interval, negative value indicates relative time

		timer = CreateWaitableTimer(NULL, TRUE, NULL);
		SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
		WaitForSingleObject(timer, INFINITE);
		CloseHandle(timer);
}

int main(int argc, char const *argv[]) {
	Connection* conn = connectionCreate("ipcdemo", CONN_TYPE_ALL);

	printf("Sent: message1\n");
	Message* msg = messageCreate("Message 1", 10);
	connectionSend(conn, msg);
	__usleep(500);
	messageDestroy(msg);

	printf("Sent: message2\n");
	msg = messageCreate("Message 2", 10);
	connectionSend(conn, msg);
	__usleep(500);
	messageDestroy(msg);

	printf("Sent: message3\n");
	msg = messageCreate("Message 3", 10);
	connectionSend(conn, msg);
	__usleep(500);
	messageDestroy(msg);
}
