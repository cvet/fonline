#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "ipc.h"

int recived = 0;

void recive(Message* msg){
	printf("Recived data: %s\nTo PID: %d\n", msg->data, msg->pid);

	recived += 1;
}

void parent(Connection* conn, pid_t pid){
	connectionSetCallback(conn, recive);
	connectionStartAutoDispatch(conn);

	while (recived < 2) {
		usleep(100);
	}
	connectionStopAutoDispatch(conn);
	printf("Not Listening\n");
	connectionStartAutoDispatch(conn);
	while (recived < 3) {
		usleep(100);
	}
	connectionStopAutoDispatch(conn);

	connectionStartAutoDispatch(conn);
	connectionSubscribe(conn, "DEMO-SUBJECT");
	while (recived < 4) {
		usleep(100);
	}

	connectionRemoveSubscription(conn, "DEMO-SUBJECT");
	printf("Next message wont be recieved in 5 seconds\n");
	sleep(5);
	connectionStopAutoDispatch(conn);
	connectionClose(conn);
}

void child(Connection* conn){


	char* str = "This string was sent over IPC using named pipes!";
	Message* msg = messageCreate(str, strlen(str)+1);
	messageSetPID(msg, getppid());
	connectionSend(conn, msg);
	messageDestroy(msg);

	str = "This is the second message!";
	msg = messageCreate(str, strlen(str)+1);
	messageSetPID(msg, getppid());
	connectionSend(conn, msg);
	messageDestroy(msg);

	//Wait for dispatcher to actually stop
	int i;
	for (i = 0; i < 50; i++) {
		usleep(100);
	}

	str = "Listening again!";
	msg = messageCreate(str, strlen(str)+1);
	messageSetPID(msg, getppid());
	connectionSend(conn, msg);
	messageDestroy(msg);

	//Wait for dispatcher to actually stop
	for (i = 0; i < 50; i++) {
		usleep(100);
	}

	str = "Subject Message 1";
	msg = messageCreate(str, strlen(str)+1);
	messageSetSubject(msg, "DEMO-SUBJECT");
	connectionSend(conn, msg);
	messageDestroy(msg);

	//Wait for unsubscription
	for (i = 0; i < 500; i++) {
		usleep(100);
	}

	str = "Subject Message 2";
	msg = messageCreate(str, strlen(str)+1);
	messageSetSubject(msg, "DEMO-SUBJECT");
	connectionSend(conn, msg);
	messageDestroy(msg);
}

int main(int argc, char const *argv[]) {
	Connection* conn = connectionCreate("ipcdemo", CONN_TYPE_PID);

	pid_t pid = fork();

	if (pid == 0){
		child(conn);
	}else{
		parent(conn, pid);
	}

	connectionDestroy(conn);

	return 0;
}
