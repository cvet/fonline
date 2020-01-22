#pragma once

#include "Common.h"

#include "Entity.h"
#include "Properties.h"

class ProtoManager;
class SpriteManager;
class ResourceManager;
class CScriptArray;

class ProtoMap : public ProtoEntity
{
public:
    struct Tile
    {
        hash Name {};
        ushort HexX {};
        ushort HexY {};
        short OffsX {};
        short OffsY {};
        uchar Layer {};
        bool IsRoof {};
#if defined(FONLINE_EDITOR)
        bool IsSelected {};
#endif
    };
    using TileVec = vector<Tile>;

    PROPERTIES_HEADER();
    CLASS_PROPERTY(string, FilePath);
    CLASS_PROPERTY(ushort, Width);
    CLASS_PROPERTY(ushort, Height);
    CLASS_PROPERTY(ushort, WorkHexX);
    CLASS_PROPERTY(ushort, WorkHexY);
    CLASS_PROPERTY(int, CurDayTime);
    CLASS_PROPERTY(hash, ScriptId);
    CLASS_PROPERTY(CScriptArray*, DayTime); // 4 int
    CLASS_PROPERTY(CScriptArray*, DayColor); // 12 uchar
    CLASS_PROPERTY(bool, IsNoLogOut);

    ProtoMap(hash pid);
    ~ProtoMap();

private:
    using CrLoadFunc = std::function<bool(uint id, ProtoCritter* proto, const StrMap& kv)>;
    using ItemLoadFunc = std::function<bool(uint id, ProtoItem* proto, const StrMap& kv)>;
    using TileLoadFunc = std::function<void(Tile&& tile)>;

    bool BaseLoad(FileManager& file_mngr, ProtoManager& proto_mngr, CrLoadFunc cr_load, ItemLoadFunc item_load,
        TileLoadFunc tile_load);

#if defined(FONLINE_SERVER) || defined(FONLINE_EDITOR)
public:
    bool ServerLoad(FileManager& file_mngr, ProtoManager& proto_mngr);
    void GetStaticItemTriggers(ushort hx, ushort hy, ItemVec& triggers);
    Item* GetStaticItem(ushort hx, ushort hy, hash pid);
    void GetStaticItemsHex(ushort hx, ushort hy, ItemVec& items);
    void GetStaticItemsHexEx(ushort hx, ushort hy, uint radius, hash pid, ItemVec& items);
    void GetStaticItemsByPid(hash pid, ItemVec& items);

#if !defined(FONLINE_EDITOR)
    TileVec Tiles;
#endif
    UCharVec SceneryData;
    hash HashTiles;
    hash HashScen;
    CritterVec CrittersVec;
    ItemVec AllItemsVec;
    ItemVec HexItemsVec;
    ItemVec ChildItemsVec;
    ItemVec StaticItemsVec;
    ItemVec TriggerItemsVec;
    uchar* HexFlags;

private:
    bool OnAfterLoad();
    bool BindScripts();
#endif

#if defined(FONLINE_EDITOR)
public:
    bool EditorLoad(
        FileManager& file_mngr, ProtoManager& proto_mngr, SpriteManager& spr_mngr, ResourceManager& res_mngr);
    void EditorSave(FileManager& file_mngr, const string& custom_name);

    TileVec Tiles;
    EntityVec AllEntities;
    uint LastEntityId;
#endif
};
