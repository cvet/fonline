#include "MessageWrap.h"

using namespace std;
using namespace v8;
using namespace node;


NAN_MODULE_INIT(MessageWrap::Init){
  v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("Message").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(3);

  Nan::SetPrototypeMethod(tpl, "setPID", setPID);
  Nan::SetPrototypeMethod(tpl, "setSubject", setSubject);
  Nan::SetPrototypeMethod(tpl, "getData", getData);

  constructor().Reset(Nan::GetFunction(tpl).ToLocalChecked());
  Nan::Set(target, Nan::New("Message").ToLocalChecked(),
    Nan::GetFunction(tpl).ToLocalChecked());
}

MessageWrap::MessageWrap(char* data, size_t len): data(data){
  msg = new IPC::Message(data, len);
}

MessageWrap::~MessageWrap(){
  delete msg;
  if (data != NULL){
    free(data);
  }
}

NAN_METHOD(MessageWrap::New){
  MessageWrap* obj;
  if(info[0]->IsNumber()){
    obj = new MessageWrap(info[0]->Uint32Value(), info[1]->Uint32Value());
  }else{
    Local<Object> bufferObj = info[0]->ToObject();
    char* data_tmp = Buffer::Data(bufferObj);
    size_t len = Buffer::Length(bufferObj);

    char* datacpy = (char*) malloc(len);
    memcpy(datacpy, data_tmp, len);

    obj = new MessageWrap(datacpy, len);
  }

  obj->Wrap(info.This());
  info.GetReturnValue().Set(info.This());
}

NAN_METHOD(MessageWrap::setPID){
  IPC::Message* msg = Nan::ObjectWrap::Unwrap<MessageWrap>(info.Holder())->msg;
  PID pid = info[0]->Uint32Value();
  msg->setPID(pid);
}

NAN_METHOD(MessageWrap::setSubject){
  IPC::Message* msg = Nan::ObjectWrap::Unwrap<MessageWrap>(info.Holder())->msg;
  std::string subject(*Nan::Utf8String(info[0].As<String>()));
  msg->setSubject((char*) subject.c_str());
}

NAN_METHOD(MessageWrap::getData){
  IPC::Message* msg = Nan::ObjectWrap::Unwrap<MessageWrap>(info.Holder())->msg;

  size_t len = msg->getLen();
  char* datacpy = (char*) malloc(len);
  memcpy(datacpy, msg->getData(), len);

  info.GetReturnValue().Set(Nan::NewBuffer(datacpy, len).ToLocalChecked());
}
