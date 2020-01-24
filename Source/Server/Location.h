#pragma once

#include "Common.h"

#include "Entity.h"

class Map;
using MapVec = vector<Map*>;
using MapMap = map<uint, Map*>;
class Location;
using LocationVec = vector<Location*>;
using LocationMap = map<uint, Location*>;

class Location : public Entity
{
public:
    Location(uint id, ProtoLocation* proto);
    void BindScript();
    ProtoLocation* GetProtoLoc();
    bool IsLocVisible();
    MapVec& GetMapsRaw();
    MapVec GetMaps();
    uint GetMapsCount();
    Map* GetMapByIndex(uint index);
    Map* GetMapByPid(hash map_pid);
    uint GetMapIndex(hash map_pid);
    bool IsCanEnter(uint players_count);
    bool IsNoCrit();
    bool IsNoPlayer();
    bool IsNoNpc();
    bool IsCanDelete();

    PROPERTIES_HEADER();
    CLASS_PROPERTY(CScriptArray*, MapProtos); // hash[]
    CLASS_PROPERTY(CScriptArray*, MapEntrances); // hash[]
    CLASS_PROPERTY(CScriptArray*, Automaps); // hash[]
    CLASS_PROPERTY(uint, MaxPlayers);
    CLASS_PROPERTY(bool, AutoGarbage);
    CLASS_PROPERTY(bool, GeckVisible);
    CLASS_PROPERTY(hash, EntranceScript);
    CLASS_PROPERTY(ushort, WorldX);
    CLASS_PROPERTY(ushort, WorldY);
    CLASS_PROPERTY(ushort, Radius);
    CLASS_PROPERTY(bool, Hidden);
    CLASS_PROPERTY(bool, ToGarbage);
    CLASS_PROPERTY(uint, Color);

    // Todo: encapsulate Location data
    uint EntranceScriptBindId {};
    int GeckCount {};

private:
    MapVec locMaps {};
};
