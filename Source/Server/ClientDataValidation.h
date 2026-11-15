// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "NetBuffer.h"

FO_BEGIN_NAMESPACE

class EngineMetadata;
class Property;

// A peer's request that fails validation is a malformed frame for the connection that sent it, not an engine fault
FO_DECLARE_EXCEPTION_EXT(ClientDataValidationException, NetBufferException);

void ValidateInboundRemoteCallData(const RemoteCallDesc& inbound_call, const_span<uint8_t> data, const EngineMetadata& meta);
void ValidateInboundPropertyData(ptr<const Property> prop, const_span<uint8_t> data, const EngineMetadata& meta);

FO_END_NAMESPACE
