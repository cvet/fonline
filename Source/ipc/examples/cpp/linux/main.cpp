#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "ipc_cpp.h"

using namespace IPC;

int recived = 0;

void recive(Message msg){
	printf("Recived data: %s\n", msg.getData());

	recived++;
}

void parent(Connection conn){
	conn.setCallback(recive);
	conn.startAutoDispatch();

	while (recived < 2) {
		usleep(100);
	}
	conn.stopAutoDispatch();
	printf("Not Listening\n");
	conn.startAutoDispatch();
	while (recived < 3) {
		usleep(100);
	}
	conn.stopAutoDispatch();

	conn.startAutoDispatch();
	conn.subscribe("DEMO-SUBJECT");
	while (recived < 4) {
		usleep(100);
	}

	conn.removeSubscription("DEMO-SUBJECT");
	printf("Next message wont be recieved in 5 seconds\n");
	sleep(5);
	conn.stopAutoDispatch();
	conn.close();
}

void child(Connection conn){
	{
		char* str = "This string was sent over IPC using named pipes!";
		Message msg(str, strlen(str)+1);
		msg.setPID(getppid());
		conn.send(msg);
	}

	{
		char* str = "This is the second message!";
		Message msg(str, strlen(str)+1);
		msg.setPID(getppid());
		conn.send(msg);
	}

	//Wait for dispatcher to actually stop
	for (int i = 0; i < 50; i++) {
		usleep(100);
	}

	{
		char* str = "Listening again!";
		Message msg(str, strlen(str)+1);
		msg.setPID(getppid());
		conn.send(msg);
	}

	//Wait for dispatcher to actually stop
	for (int i = 0; i < 50; i++) {
		usleep(100);
	}

	{
		char* str = "Subject Message 1";
		Message msg(str, strlen(str)+1);
		msg.setSubject("DEMO-SUBJECT");
		conn.send(msg);
	}

	//Wait for unsubscription
	for (int i = 0; i < 50; i++) {
		usleep(100);
	}

	{
		char* str = "Subject Message 2";
		Message msg(str, strlen(str)+1);
		msg.setSubject("DEMO-SUBJECT");
		conn.send(msg);
	}

	for (int i = 0; i < 50; i++) {
		usleep(100);
	}
}


int main(int argc, char const *argv[]) {
	Connection conn("ipcdemo", CONN_TYPE_PID);

	pid_t pid = fork();

	if (pid == 0){
		child(conn);
	}else{
		parent(conn);
	}

	return 0;
}
