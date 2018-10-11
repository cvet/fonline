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

int recived = 0;

void recive(Message* msg){
	printf("Recived data: %s\n", msg->data);

	recived++;
}

int main(int argc, char const *argv[]) {
	Connection* conn = connectionCreate("ipcdemo", CONN_TYPE_ALL);
	connectionSetCallback(conn, recive);
	connectionStartAutoDispatch(conn);

	while (recived < 3){
		__usleep(100);
	}

	connectionStopAutoDispatch(conn);
	connectionClose(conn);
	connectionDestroy(conn);
}
