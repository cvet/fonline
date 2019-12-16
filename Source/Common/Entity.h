#pragma once

#include "Common.h"
#include "MsgFiles.h"
#include "Properties.h"

enum class EntityType
{
    None = 0,
    EntityProto = 1,
    ItemProto = 2,
    CritterProto = 3,
    MapProto = 4,
    LocationProto = 5,
#if defined(FONLINE_SERVER) || defined(FONLINE_EDITOR)
    Item = 6,
    Client = 7,
    Npc = 8,
    Map = 9,
    Location = 10,
#endif
#if defined(FONLINE_CLIENT) || defined(FONLINE_EDITOR)
    ItemView = 11,
    ItemHexView = 12,
    CritterView = 13,
    MapView = 14,
    LocationView = 15,
#endif
    Custom = 16,
    Global = 17,
    Max = 28,
};

class Entity;
using EntityVec = vector<Entity*>;
using EntityMap = map<uint, Entity*>;
#if defined(FONLINE_SERVER) || defined(FONLINE_EDITOR)
class Item;
using ItemVec = vector<Item*>;
using ItemMap = map<uint, Item*>;
class Critter;
using CritterMap = map<uint, Critter*>;
using CritterVec = vector<Critter*>;
class Npc;
using NpcMap = map<uint, Npc*>;
using NpcVec = vector<Npc*>;
class Client;
using ClientMap = map<uint, Client*>;
using ClientVec = vector<Client*>;
class Map;
using MapVec = vector<Map*>;
using MapMap = map<uint, Map*>;
class Location;
using LocationVec = vector<Location*>;
using LocationMap = map<uint, Location*>;
#endif
#if defined(FONLINE_CLIENT) || defined(FONLINE_EDITOR)
class ItemView;
using ItemViewVec = vector<ItemView*>;
using ItemViewMap = map<uint, ItemView*>;
class ItemHexView;
using ItemHexViewVec = vector<ItemHexView*>;
using ItemHexViewMap = map<uint, ItemHexView*>;
class CritterView;
using CritterViewMap = map<uint, CritterView*>;
using CritterViewVec = vector<CritterView*>;
class MapView;
using MapViewVec = vector<MapView*>;
using MapViewMap = map<uint, MapView*>;
class LocationView;
using LocationViewVec = vector<LocationView*>;
using LocationViewMap = map<uint, LocationView*>;
#endif
class ProtoEntity;
using ProtoEntityVec = vector<ProtoEntity*>;
using ProtoEntityMap = map<hash, ProtoEntity*>;
class ProtoLocation;
using ProtoLocationVec = vector<ProtoLocation*>;
using ProtoLocationMap = map<hash, ProtoLocation*>;
class ProtoMap;
using ProtoMapVec = vector<ProtoMap*>;
using ProtoMapMap = map<hash, ProtoMap*>;
class ProtoCritter;
using ProtoCritterMap = map<hash, ProtoCritter*>;
using ProtoCritterVec = vector<ProtoCritter*>;
class ProtoItem;
using ProtoItemVec = vector<ProtoItem*>;
using ProtoItemMap = map<hash, ProtoItem*>;

class ProtoManager;
class SpriteManager;
class ResourceManager;
class CScriptArray;

class Entity
{
protected:
    Entity(uint id, EntityType type, PropertyRegistrator* registartor, ProtoEntity* proto);
    ~Entity();

public:
    Properties Props;
    const uint Id;
    const EntityType Type;
    ProtoEntity* Proto;
    uint MonoHandle;
    mutable long RefCounter;
    bool IsDestroyed;
    bool IsDestroying;

    uint GetId() const;
    void SetId(uint id);
    hash GetProtoId() const;
    string GetName() const;
    EntityVec GetChildren() const;
    void AddRef() const;
    void Release() const;
};

class ProtoEntity : public Entity
{
protected:
    ProtoEntity(hash proto_id, EntityType type, PropertyRegistrator* registrator);

public:
    const hash ProtoId;
    UIntVec TextsLang;
    FOMsgVec Texts;
    HashSet Components;
#ifdef FONLINE_EDITOR
    string CollectionName;
#endif

    bool HaveComponent(hash name) const;
};

class CustomEntity : public Entity
{
public:
    CustomEntity(uint id, uint sub_type, PropertyRegistrator* registrator);
    const uint SubType;
};

class GlobalVars : public Entity
{
public:
    PROPERTIES_HEADER();
    CLASS_PROPERTY(ushort, YearStart);
    CLASS_PROPERTY(ushort, Year);
    CLASS_PROPERTY(ushort, Month);
    CLASS_PROPERTY(ushort, Day);
    CLASS_PROPERTY(ushort, Hour);
    CLASS_PROPERTY(ushort, Minute);
    CLASS_PROPERTY(ushort, Second);
    CLASS_PROPERTY(ushort, TimeMultiplier);
    CLASS_PROPERTY(uint, LastEntityId);
    CLASS_PROPERTY(uint, LastDeferredCallId);
    CLASS_PROPERTY(uint, HistoryRecordsId);

    GlobalVars();
};
extern GlobalVars* Globals;

class ProtoLocation : public ProtoEntity
{
public:
    PROPERTIES_HEADER();
    CLASS_PROPERTY(CScriptArray*, MapProtos);

    ProtoLocation(hash pid);
};

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
    CLASS_PROPERTY(string, FileDir);
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

    bool BaseLoad(ProtoManager& proto_mngr, CrLoadFunc cr_load, ItemLoadFunc item_load, TileLoadFunc tile_load);

#if defined(FONLINE_SERVER) || defined(FONLINE_EDITOR)
public:
    bool ServerLoad(ProtoManager& proto_mngr);
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
    static bool IsMapFile(const string& fname);
    void GenNew();
    bool EditorLoad(ProtoManager& proto_mngr, SpriteManager& spr_mngr, ResourceManager& res_mngr);
    bool EditorSave(const string& custom_name);

    TileVec Tiles;
    EntityVec AllEntities;
    uint LastEntityId;
#endif
};

class ProtoCritter : public ProtoEntity
{
public:
    PROPERTIES_HEADER();
    CLASS_PROPERTY(uint, Multihex);

    ProtoCritter(hash pid);
};

class ProtoItem : public ProtoEntity
{
public:
    PROPERTIES_HEADER();
    CLASS_PROPERTY(hash, PicMap);
    CLASS_PROPERTY(hash, PicInv);
    CLASS_PROPERTY(bool, Stackable);
    CLASS_PROPERTY(short, OffsetX);
    CLASS_PROPERTY(short, OffsetY);
    CLASS_PROPERTY(uchar, Slot);
    CLASS_PROPERTY(char, LightIntensity);
    CLASS_PROPERTY(uchar, LightDistance);
    CLASS_PROPERTY(uchar, LightFlags);
    CLASS_PROPERTY(uint, LightColor);
    CLASS_PROPERTY(uint, Count);
    CLASS_PROPERTY(bool, IsFlat);
    CLASS_PROPERTY(char, DrawOrderOffsetHexY);
    CLASS_PROPERTY(int, Corner);
    CLASS_PROPERTY(bool, DisableEgg);
    CLASS_PROPERTY(bool, IsStatic);
    CLASS_PROPERTY(bool, IsScenery);
    CLASS_PROPERTY(bool, IsWall);
    CLASS_PROPERTY(bool, IsBadItem);
    CLASS_PROPERTY(bool, IsColorize);
    CLASS_PROPERTY(bool, IsShowAnim);
    CLASS_PROPERTY(bool, IsShowAnimExt);
    CLASS_PROPERTY(uchar, AnimStay0);
    CLASS_PROPERTY(uchar, AnimStay1);
    CLASS_PROPERTY(CScriptArray*, BlockLines);

    ProtoItem(hash pid);

    int64 InstanceCount;

    bool IsStatic() { return GetIsStatic(); }
    bool IsAnyScenery() { return IsScenery() || IsWall(); }
    bool IsScenery() { return GetIsScenery(); }
    bool IsWall() { return GetIsWall(); }
};
