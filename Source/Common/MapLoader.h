#pragma once

#include "Common.h"

#include "Entity.h"
#include "ProtoManager.h"

DECLARE_EXCEPTION(MapLoaderException);

struct MapTile
{
    hash Name {};
    ushort HexX {};
    ushort HexY {};
    short OffsX {};
    short OffsY {};
    uchar Layer {};
    bool IsRoof {};
    bool IsSelected {}; // Todo: remove mapper specific IsSelected from MapTile
};
using MapTileVec = vector<MapTile>;

class MapLoader : public StaticClass
{
public:
    using CrLoadFunc = std::function<bool(uint id, ProtoCritter* proto, const StrMap& kv)>;
    using ItemLoadFunc = std::function<bool(uint id, ProtoItem* proto, const StrMap& kv)>;
    using TileLoadFunc = std::function<void(MapTile&& tile)>;

    static void Load(const string& name, FileManager& file_mngr, ProtoManager& proto_mngr, CrLoadFunc cr_load,
        ItemLoadFunc item_load, TileLoadFunc tile_load);
};
