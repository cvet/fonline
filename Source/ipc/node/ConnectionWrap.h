#ifndef CONNECTION_WRAP_H
#define CONNECTION_WRAP_H

#include "ipc_cpp.h"
#include <nan.h>

class ConnectionWrap: public Nan::ObjectWrap {
public:
  static NAN_MODULE_INIT(Init);

private:
  IPC::Connection* conn;

  ConnectionWrap(char* name, int type, int create);
  ~ConnectionWrap();

  static NAN_METHOD(New);
  static NAN_METHOD(startAutoDispatch);
  static NAN_METHOD(stopAutoDispatch);
  static NAN_METHOD(setCallback);
  static NAN_METHOD(getCallback);
  static NAN_METHOD(removeCallback);
  static NAN_METHOD(send);
  static NAN_METHOD(subscribe);
  static NAN_METHOD(removeSubscription);
  static NAN_METHOD(close);

  static inline Nan::Persistent<v8::Function> & constructor();
};

#endif
