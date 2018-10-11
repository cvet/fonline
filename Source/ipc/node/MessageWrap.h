#ifndef MESSAGE_WRAP_H
#define MESSAGE_WRAP_H

#include "ipc_cpp.h"
#include <nan.h>


class MessageWrap: public Nan::ObjectWrap {
public:
  IPC::Message* msg;
  static NAN_MODULE_INIT(Init);
  static inline Nan::Persistent<v8::Function> & constructor(){
    static Nan::Persistent<v8::Function> msg_constructor;
    return msg_constructor;
  }

private:
  char* data;

  MessageWrap(char* data, size_t len);
  MessageWrap(uint32_t ptr_hi, uint32_t ptr_lo): msg(new IPC::Message((void*) (((uint64_t)ptr_hi << 32) + ptr_lo))){}
  ~MessageWrap();

  static NAN_METHOD(New);

  static NAN_METHOD(setPID);

  static NAN_METHOD(setSubject);

  static NAN_METHOD(getData);
};

#endif
