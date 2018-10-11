extern "C"{
 #include "ipc.h"
}
typedef Message IPC_Message;
typedef Connection IPC_Connection;
typedef ConnectionCallback IPC_ConnectionCallback;

#include "ipc_cpp.h"

extern "C"{
	#include "hashtable.h"
}

#define HT_CAPACITY 50

hashtable_t* ht;
int initialized = 0;

void init(){
	ht = ht_create(HT_CAPACITY);
	initialized = 1;
}

void cCallback(IPC_Message* msg){
	IPC::Message cppMsg(msg);

	char* name = msg->data + msg->len + sizeof(size_t);
	IPC::ConnectionCallback cb = (IPC::ConnectionCallback) ht_get(ht, name);

	if (cb != NULL){
		cb(cppMsg);
	}
}

IPC::Connection::Connection(char* name, int type, int create): destroy(1){
	if(!initialized){
		init();
	}

	if (create){
		ptr = connectionCreate(name, type);
	}else{
		ptr = connectionConnect(name, type);
	}
}

char* IPC::Connection::getName(){
  return ((IPC_Connection*) ptr)->name;
}

void IPC::Connection::startAutoDispatch(){
	connectionStartAutoDispatch((IPC_Connection*) ptr);
}

void IPC::Connection::stopAutoDispatch(){
	connectionStopAutoDispatch((IPC_Connection*) ptr);
}

void IPC::Connection::setCallback(IPC::ConnectionCallback cb){
	ht_put(ht, ((IPC_Connection*) ptr)->name, (void*) cb);
	connectionSetCallback((IPC_Connection*) ptr, cCallback);
}

IPC::ConnectionCallback IPC::Connection::getCallback(){
	return (IPC::ConnectionCallback) ht_get(ht, ((IPC_Connection*) ptr)->name);
}

void IPC::Connection::removeCallback(){
	ht_remove(ht, ((IPC_Connection*) ptr)->name);
	connectionRemoveCallback((IPC_Connection*) ptr);
}

void IPC::Connection::send(Message msg){
	connectionSend((IPC_Connection*) ptr, (IPC_Message*) msg.getCPointer());
}

void IPC::Connection::subscribe(char* subject){
	connectionSubscribe((IPC_Connection*) ptr, subject);
}

void IPC::Connection::removeSubscription(char* subject){
	connectionRemoveSubscription((IPC_Connection*) ptr, subject);
}

void IPC::Connection::close(){
	connectionClose((IPC_Connection*) ptr);
}

IPC::Connection::~Connection(){
	if (destroy){
		connectionDestroy((IPC_Connection*) ptr);
	}
}
