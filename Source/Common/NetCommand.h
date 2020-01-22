#pragma once

#include "Common.h"

#include "NetBuffer.h"

extern bool PackNetCommand(
    const string& str, NetBuffer* pbuf, std::function<void(const string&)> logcb, const string& name);
