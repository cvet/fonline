#include <node.h>
#include "ConnectionWrap.h"
#include "MessageWrap.h"

using namespace v8;

void InitAll(Handle<Object> exports) {
  ConnectionWrap::Init(exports);
  MessageWrap::Init(exports);
}

NODE_MODULE(IPC, InitAll)
