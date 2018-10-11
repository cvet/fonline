extern "C"{
 #include "ipc.h"
}
typedef Message IPC_Message;

#include "ipc_cpp.h"


IPC::Message::Message(char* data, size_t len): destroy(1){
	ptr = messageCreate(data, len);
}

void IPC::Message::setPID(PID pid){
	messageSetPID((IPC_Message*) ptr, pid);
}

PID IPC::Message::getPID(){
	return ((IPC_Message*) ptr)->pid;
}

void IPC::Message::setSubject(char* subject){
	messageSetSubject((IPC_Message*) ptr, subject);
}

char* IPC::Message::getSubject(){
	return ((IPC_Message*) ptr)->subject;
}

size_t IPC::Message::getLen(){
	return ((IPC_Message*) ptr)->len;
}

char* IPC::Message::getData(){
	return ((IPC_Message*) ptr)->data;
}

void* IPC::Message::getCPointer(){
	return ptr;
}

int IPC::Message::getType(){
  return ((IPC_Message*) ptr)->type;
}

IPC::Message::~Message(){
	if (destroy){
		messageDestroy((IPC_Message*) ptr);
	}
}
