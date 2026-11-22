// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "ConfigFile.h"
#include "EntityProtos.h"

FO_BEGIN_NAMESPACE

// Baked map binaries carry no self-description, so a stale or foreign file has to be rejected by this marker
// instead of being read as element counts. Bump the version whenever the layout written by MapBaker changes
constexpr uint32_t BAKED_MAP_FILE_MAGIC {0x424D4F46}; // "FOMB"
constexpr uint32_t BAKED_MAP_FILE_VERSION {1};

FO_DECLARE_EXCEPTION(MapLoaderException);

class EngineMetadata;

namespace MapLoader
{
    using CrLoadFunc = function<void(ident_t id, ptr<const ProtoCritter> proto, ptr<const map<string_view, string_view>> kv)>;
    using ItemLoadFunc = function<void(ident_t id, ptr<const ProtoItem> proto, ptr<const map<string_view, string_view>> kv)>;

    void Load(string_view name, string_view file_name, const string& buf, const EngineMetadata& meta, hash_resolver& hashes, const CrLoadFunc& cr_load, const ItemLoadFunc& item_load);
    auto EnumerateMaps(string_view file_name, const string& buf) -> vector<string>;
    auto CanOverrideItemProperty(bool is_static, ptr<const Property> prop) -> bool;
    void ValidateItemProperties(const Properties& props, const Properties& proto_props, string_view proto_name = {}, ident_t id = {});
    void ReadBakedFileHeader(data_reader& reader, string_view map_name);
}

FO_END_NAMESPACE
