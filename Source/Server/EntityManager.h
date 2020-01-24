#pragma once

#include "Common.h"

#include "Critter.h"
#include "Entity.h"
#include "Item.h"
#include "Location.h"
#include "Map.h"

class MapManager;
class CritterManager;
class ItemManager;

class EntityManager
{
public:
    EntityManager(MapManager& map_mngr, CritterManager& cr_mngr, ItemManager& item_mngr);

    void RegisterEntity(Entity* entity);
    void UnregisterEntity(Entity* entity);
    Entity* GetEntity(uint id, EntityType type);
    void GetEntities(EntityType type, EntityVec& entities);
    uint GetEntitiesCount(EntityType type);

    void GetItems(ItemVec& items);
    void GetCritterItems(uint crid, ItemVec& items);
    Critter* GetCritter(uint crid);
    void GetCritters(CritterVec& critters);
    Map* GetMapByPid(hash pid, uint skip_count);
    void GetMaps(MapVec& maps);
    Location* GetLocationByPid(hash pid, uint skip_count);
    void GetLocations(LocationVec& locations);

    bool LoadEntities();
    void ClearEntities();

private:
    bool LinkMaps();
    bool LinkNpc();
    bool LinkItems();
    void InitAfterLoad();

    MapManager& mapMngr;
    CritterManager& crMngr;
    ItemManager& itemMngr;
    EntityMap allEntities {};
    uint entitiesCount[(int)EntityType::Max] {};
};
