#pragma once

#include "Common.h"

#include "Entity.h"

class Location : public Entity
{
    MapVec locMaps;

public:
    uint EntranceScriptBindId = 0;
    int GeckCount = 0;

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

    Location(uint id, ProtoLocation* proto);
    ~Location();

    void BindScript();
    ProtoLocation* GetProtoLoc() { return (ProtoLocation*)Proto; }
    bool IsLocVisible() { return !GetHidden() || (GetGeckVisible() && GeckCount > 0); }
    MapVec& GetMapsRaw() { return locMaps; };
    MapVec GetMaps();
    uint GetMapsCount() { return (uint)locMaps.size(); }
    Map* GetMapByIndex(uint index);
    Map* GetMapByPid(hash map_pid);
    uint GetMapIndex(hash map_pid);
    bool IsCanEnter(uint players_count);

    bool IsNoCrit();
    bool IsNoPlayer();
    bool IsNoNpc();
    bool IsCanDelete();
};
