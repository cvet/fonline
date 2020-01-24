#include "Location.h"
#include "Critter.h"
#include "Map.h"
#include "Script.h"
#include "StringUtils.h"

PROPERTIES_IMPL(Location);
CLASS_PROPERTY_IMPL(Location, MapProtos);
CLASS_PROPERTY_IMPL(Location, MapEntrances);
CLASS_PROPERTY_IMPL(Location, Automaps);
CLASS_PROPERTY_IMPL(Location, MaxPlayers);
CLASS_PROPERTY_IMPL(Location, AutoGarbage);
CLASS_PROPERTY_IMPL(Location, GeckVisible);
CLASS_PROPERTY_IMPL(Location, EntranceScript);
CLASS_PROPERTY_IMPL(Location, WorldX);
CLASS_PROPERTY_IMPL(Location, WorldY);
CLASS_PROPERTY_IMPL(Location, Radius);
CLASS_PROPERTY_IMPL(Location, Hidden);
CLASS_PROPERTY_IMPL(Location, ToGarbage);
CLASS_PROPERTY_IMPL(Location, Color);

Location::Location(uint id, ProtoLocation* proto) : Entity(id, EntityType::Location, PropertiesRegistrator, proto)
{
    RUNTIME_ASSERT(proto);
}

void Location::BindScript()
{
    EntranceScriptBindId = 0;

    if (GetEntranceScript())
    {
        string func_name = _str().parseHash(GetEntranceScript());
        EntranceScriptBindId =
            Script::BindByFuncName(func_name, "bool %s(Location, Critter[], uint8 entranceIndex)", false);
    }
}

ProtoLocation* Location::GetProtoLoc()
{
    return (ProtoLocation*)Proto;
}

bool Location::IsLocVisible()
{
    return !GetHidden() || (GetGeckVisible() && GeckCount > 0);
}

MapVec& Location::GetMapsRaw()
{
    return locMaps;
};

MapVec Location::GetMaps()
{
    return locMaps;
}

uint Location::GetMapsCount()
{
    return (uint)locMaps.size();
}

Map* Location::GetMapByIndex(uint index)
{
    if (index >= locMaps.size())
        return nullptr;
    return locMaps[index];
}

Map* Location::GetMapByPid(hash map_pid)
{
    for (Map* map : locMaps)
    {
        if (map->GetProtoId() == map_pid)
            return map;
    }
    return nullptr;
}

uint Location::GetMapIndex(hash map_pid)
{
    uint index = 0;
    for (Map* map : locMaps)
    {
        if (map->GetProtoId() == map_pid)
            return index;
        index++;
    }
    return uint(-1);
}

bool Location::IsCanEnter(uint players_count)
{
    uint max_palyers = GetMaxPlayers();
    if (!max_palyers)
        return true;

    for (Map* map : locMaps)
    {
        players_count += map->GetPlayersCount();
        if (players_count >= max_palyers)
            return false;
    }
    return true;
}

bool Location::IsNoCrit()
{
    for (Map* map : locMaps)
        if (map->GetCrittersCount())
            return false;
    return true;
}

bool Location::IsNoPlayer()
{
    for (Map* map : locMaps)
        if (map->GetPlayersCount())
            return false;
    return true;
}

bool Location::IsNoNpc()
{
    for (Map* map : locMaps)
        if (map->GetNpcsCount())
            return false;
    return true;
}

bool Location::IsCanDelete()
{
    if (GeckCount > 0)
        return false;

    // Check for players
    for (Map* map : locMaps)
        if (map->GetPlayersCount())
            return false;

    // Check for npc
    MapVec maps = locMaps;
    for (Map* map : maps)
    {
        for (Npc* npc : map->GetNpcs())
        {
            if (npc->GetIsGeck() || (!npc->GetIsNoHome() && npc->GetHomeMapId() != map->GetId()) ||
                npc->IsHaveGeckItem())
                return false;
        }
    }
    return true;
}
