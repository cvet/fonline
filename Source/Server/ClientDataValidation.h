// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "Geometry.h"
#include "NetBuffer.h"

FO_BEGIN_NAMESPACE

class EngineMetadata;
class Property;

// A peer's request that fails validation is a malformed frame for the connection that sent it, not an engine fault
FO_DECLARE_EXCEPTION_EXT(ClientDataValidationException, NetBufferException);

void ValidateInboundRemoteCallData(const RemoteCallDesc& inbound_call, const_span<uint8_t> data, const EngineMetadata& meta);
void ValidateInboundPropertyData(ptr<const Property> prop, const_span<uint8_t> data, const EngineMetadata& meta);

// Values a peer's movement messages carry: each one is rejected as a malformed frame, so a forged number cannot reach
// the movement and link-delay code that treats it as an invariant
void ValidateInboundSenderStamp(int64_t sender_ms);
void ValidateInboundMoveLease(float32_t lease_distance, bool allow_zero);
void ValidateInboundMovePlan(msize map_size, mpos start_hex, const vector<mdir>& steps, const vector<uint16_t>& control_steps);
void ValidateInboundDir(mdir dir);

FO_END_NAMESPACE
