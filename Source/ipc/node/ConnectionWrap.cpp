#include "ConnectionWrap.h"
#include "MessageWrap.h"
extern "C"{
	#include "hashtable.h"
}

using namespace std;
using namespace v8;
using namespace node;


#define HT_CAPACITY 50

IPC::Message* queue[50];
int queue_len = 0;
uv_mutex_t mutex;

hashtable_t* ht;
uv_async_t async;
uv_loop_t* loop;

void cbInMainThread(uv_async_t* handle){
	Nan::HandleScope scope;

	uv_mutex_lock(&mutex);

	for (int i = 0; i < queue_len; i++){
		IPC::Message* msg = queue[i];

		char* name = msg->getData() + msg->getLen();

		Nan::Callback* cb = (Nan::Callback*) ht_get(ht, name);

		Local<Function> cons = Nan::New(MessageWrap::constructor());

		uint64_t ptr = (uint64_t) msg->getCPointer();
		uint32_t ptr_hi = ptr >> 32;
		uint32_t ptr_lo = ptr & 0xffffffff;

		Local<Value> cons_argv[2] = {Nan::New<Uint32>(ptr_hi), Nan::New<Uint32>(ptr_lo)};
		Local<Value> argv[1] = {Nan::NewInstance(cons, 2, cons_argv).ToLocalChecked()};

		cb->Call(1, argv);

		delete msg;
	}

	queue_len = 0;
	uv_mutex_unlock(&mutex);
}


void cppCallback(IPC::Message msg){

	char* name = msg.getData() + msg.getLen() + sizeof(size_t);
	size_t name_len = strlen(name) + 1;

	size_t len = msg.getLen() + name_len;

	if (msg.getType() == 3){
		len += strlen(msg.getSubject()) + 1;
	}

	char* data = (char*) malloc(len);

	memcpy(data, msg.getData(), msg.getLen());
	memcpy(data + msg.getLen(), name, name_len);

	IPC::Message* copy = new IPC::Message(data, msg.getLen());

	if (msg.getType() == 2){
		copy->setPID(msg.getPID());
	}else if (msg.getType() == 3){
		memcpy(data + msg.getLen() + name_len, msg.getSubject(), strlen(msg.getSubject()) + 1);
		copy->setSubject(data + msg.getLen());
	}

	uv_mutex_lock(&mutex);
	queue[queue_len++] = copy;
	uv_mutex_unlock(&mutex);


	uv_async_send(&async);

}

void init(){
	ht = ht_create(HT_CAPACITY);
}


NAN_MODULE_INIT(ConnectionWrap::Init){
	init();

	v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("Connection").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(9);

	Nan::SetPrototypeMethod(tpl, "startAutoDispatch", startAutoDispatch);
	Nan::SetPrototypeMethod(tpl, "stopAutoDispatch", stopAutoDispatch);
	Nan::SetPrototypeMethod(tpl, "setCallback", setCallback);
	Nan::SetPrototypeMethod(tpl, "getCallback", getCallback);
	Nan::SetPrototypeMethod(tpl, "removeCallback", removeCallback);
	Nan::SetPrototypeMethod(tpl, "send", send);
	Nan::SetPrototypeMethod(tpl, "subscribe", subscribe);
	Nan::SetPrototypeMethod(tpl, "removeSubscription", removeSubscription);
	Nan::SetPrototypeMethod(tpl, "close", close);

	constructor().Reset(Nan::GetFunction(tpl).ToLocalChecked());
  Nan::Set(target, Nan::New("Connection").ToLocalChecked(),
    Nan::GetFunction(tpl).ToLocalChecked());
}

ConnectionWrap::ConnectionWrap(char* name, int type, int create){
  conn = new IPC::Connection(name, type, create);
}

ConnectionWrap::~ConnectionWrap(){
  delete conn;
}

NAN_METHOD(ConnectionWrap::New){
  std::string name(*Nan::Utf8String(info[0].As<String>()));
  int type = info[1]->IntegerValue();
  int create = 1;
  if (info.Length() == 3){
    create = info[2]->BooleanValue();
  }

  ConnectionWrap* obj = new ConnectionWrap((char*) name.c_str(), type, create);
  obj->Wrap(info.This());
  info.GetReturnValue().Set(info.This());
}

NAN_METHOD(ConnectionWrap::startAutoDispatch){
	loop = uv_default_loop();
	uv_async_init(loop, &async, cbInMainThread);
	uv_mutex_init(&mutex);

  IPC::Connection* conn = Nan::ObjectWrap::Unwrap<ConnectionWrap>(info.Holder())->conn;
  conn->startAutoDispatch();
}

NAN_METHOD(ConnectionWrap::stopAutoDispatch){
  IPC::Connection* conn = Nan::ObjectWrap::Unwrap<ConnectionWrap>(info.Holder())->conn;
  conn->stopAutoDispatch();
	uv_close((uv_handle_t*) &async, NULL);
	uv_mutex_destroy(&mutex);
}

NAN_METHOD(ConnectionWrap::setCallback){
	IPC::Connection* conn = Nan::ObjectWrap::Unwrap<ConnectionWrap>(info.Holder())->conn;

	Nan::Callback* cb = new Nan::Callback(Local<Function>::Cast(info[0]));
	ht_put(ht, conn->getName(), cb);

	conn->setCallback(cppCallback);
}

NAN_METHOD(ConnectionWrap::getCallback){
	IPC::Connection* conn = Nan::ObjectWrap::Unwrap<ConnectionWrap>(info.Holder())->conn;

	Nan::Callback* cb = (Nan::Callback*)ht_get(ht, conn->getName());
	info.GetReturnValue().Set(cb->GetFunction());
}

NAN_METHOD(ConnectionWrap::removeCallback){
	IPC::Connection* conn = Nan::ObjectWrap::Unwrap<ConnectionWrap>(info.Holder())->conn;

	conn->removeCallback();
	delete (Nan::Callback*)ht_remove(ht, conn->getName());
}

NAN_METHOD(ConnectionWrap::send){
	IPC::Connection* conn = Nan::ObjectWrap::Unwrap<ConnectionWrap>(info.Holder())->conn;

	IPC::Message msg = *(Nan::ObjectWrap::Unwrap<MessageWrap>(Nan::To<Object>(info[0]).ToLocalChecked())->msg);
	conn->send(msg);
}

NAN_METHOD(ConnectionWrap::subscribe){
	IPC::Connection* conn = Nan::ObjectWrap::Unwrap<ConnectionWrap>(info.Holder())->conn;

	std::string subject(*Nan::Utf8String(info[0].As<String>()));
	conn->subscribe((char*) subject.c_str());
}

NAN_METHOD(ConnectionWrap::removeSubscription){
	IPC::Connection* conn = Nan::ObjectWrap::Unwrap<ConnectionWrap>(info.Holder())->conn;

	std::string subject(*Nan::Utf8String(info[0].As<String>()));
	conn->removeSubscription((char*) subject.c_str());
}

NAN_METHOD(ConnectionWrap::close){
	IPC::Connection* conn = Nan::ObjectWrap::Unwrap<ConnectionWrap>(info.Holder())->conn;
	conn->close();
}

inline Nan::Persistent<v8::Function> & ConnectionWrap::constructor(){
	static Nan::Persistent<v8::Function> my_constructor;
  return my_constructor;
}
