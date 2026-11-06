// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE

class EngineMetadata;
class Property;

FO_DECLARE_EXCEPTION(ClientDataValidationException);

void ValidateInboundRemoteCallData(const RemoteCallDesc& inbound_call, const_span<uint8_t> data, const EngineMetadata& meta);
void ValidateInboundPropertyData(ptr<const Property> prop, const_span<uint8_t> data, const EngineMetadata& meta);

FO_END_NAMESPACE
