// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "ResourcePack.h"

FO_BEGIN_NAMESPACE

// One file the server offers, as listed by the update descriptor every handshake answer carries. The server
// writes it and both the updater and the game client read it, so all three go through this one module
struct UpdateDescriptorEntry
{
    string Name {};
    uint64_t Size {};
    uint64_t Hash {};
    UpdateFileTarget Target {};
    uint32_t FileIndex {};
    optional<ResourcePackHeader> PackHeader {}; // Present exactly for UpdateFileTarget::ClientResources
};

void WriteUpdateDescriptor(vector<uint8_t>& desc, const_span<UpdateDescriptorEntry> entries);
auto ReadUpdateDescriptor(const_span<uint8_t> desc) -> vector<UpdateDescriptorEntry>;

FO_END_NAMESPACE
