#include "EntityManager.h"
#include "Critter.h"
#include "CritterManager.h"
#include "DataBase.h"
#include "Item.h"
#include "ItemManager.h"
#include "Location.h"
#include "Log.h"
#include "Map.h"
#include "MapManager.h"
#include "Script.h"
#include "StringUtils.h"
#include "Testing.h"

EntityManager::EntityManager(MapManager& map_mngr, CritterManager& cr_mngr, ItemManager& item_mngr) :
    mapMngr(map_mngr), crMngr(cr_mngr), itemMngr(item_mngr)
{
    memzero(entitiesCount, sizeof(entitiesCount));
}

static const char* GetEntityTypeMonoName(EntityType type)
{
    if (type == EntityType::Global)
        return "Global";
    else if (type == EntityType::Custom)
        return "Custom";
#if defined(FONLINE_SERVER) || defined(FONLINE_EDITOR)
    else if (type == EntityType::Item)
        return "Item";
    else if (type == EntityType::Client)
        return "Critter";
    else if (type == EntityType::Npc)
        return "Critter";
    else if (type == EntityType::Location)
        return "Location";
    else if (type == EntityType::Map)
        return "Map";
#endif
#if defined(FONLINE_CLIENT) || defined(FONLINE_EDITOR)
    else if (type == EntityType::ItemView)
        return "Item";
    else if (type == EntityType::CritterView)
        return "Critter";
    else if (type == EntityType::ItemHexView)
        return "Item";
    else if (type == EntityType::LocationView)
        return "Location";
    else if (type == EntityType::MapView)
        return "Map";
#endif

    UNREACHABLE_PLACE;
    return nullptr;
}

void EntityManager::RegisterEntity(Entity* entity)
{
    if (!entity->Id)
    {
        uint id = Globals->GetLastEntityId() + 1;
        id = MAX(id, 2);
        Globals->SetLastEntityId(id);

        entity->SetId(id);

        DataBase::Document doc = entity->Props.SaveToDbDocument(entity->Proto ? &entity->Proto->Props : nullptr);
        doc["_Proto"] = entity->Proto ? entity->Proto->GetName() : "";

        if (entity->Type == EntityType::Location)
            DbStorage->Insert("Locations", id, doc);
        else if (entity->Type == EntityType::Map)
            DbStorage->Insert("Maps", id, doc);
        else if (entity->Type == EntityType::Npc)
            DbStorage->Insert("Critters", id, doc);
        else if (entity->Type == EntityType::Item)
            DbStorage->Insert("Items", id, doc);
        else if (entity->Type == EntityType::Custom)
            DbStorage->Insert(entity->Props.GetRegistrator()->GetClassName() + "s", id, doc);
        else
            UNREACHABLE_PLACE;
    }

    auto it = allEntities.insert(std::make_pair(entity->Id, entity));
    RUNTIME_ASSERT(it.second);
    entitiesCount[(int)entity->Type]++;

    entity->MonoHandle = Script::CreateMonoObject(GetEntityTypeMonoName(entity->Type));
    RUNTIME_ASSERT(entity->MonoHandle);

    long long_id = entity->Id;
    Script::CallMonoObjectMethod("Entity", "Init(long)", entity->MonoHandle, &long_id);
}

void EntityManager::UnregisterEntity(Entity* entity)
{
    Script::CallMonoObjectMethod("Entity", "Destroy()", entity->MonoHandle, nullptr);
    Script::DestroyMonoObject(entity->MonoHandle);

    entity->MonoHandle = 0;

    auto it = allEntities.find(entity->Id);
    RUNTIME_ASSERT(it != allEntities.end());
    allEntities.erase(it);
    entitiesCount[(int)entity->Type]--;

    Script::RemoveEventsEntity(entity);

    if (entity->Type == EntityType::Location)
        DbStorage->Delete("Locations", entity->Id);
    else if (entity->Type == EntityType::Map)
        DbStorage->Delete("Maps", entity->Id);
    else if (entity->Type == EntityType::Npc)
        DbStorage->Delete("Critters", entity->Id);
    else if (entity->Type == EntityType::Item)
        DbStorage->Delete("Items", entity->Id);
    else if (entity->Type == EntityType::Custom)
        DbStorage->Delete(entity->Props.GetRegistrator()->GetClassName() + "s", entity->Id);

    entity->SetId(0);
}

Entity* EntityManager::GetEntity(uint id, EntityType type)
{
    auto it = allEntities.find(id);
    if (it != allEntities.end() && it->second->Type == type)
        return it->second;
    return nullptr;
}

void EntityManager::GetEntities(EntityType type, EntityVec& entities)
{
    entities.reserve(entities.size() + entitiesCount[(int)type]);
    for (auto it = allEntities.begin(); it != allEntities.end(); ++it)
    {
        if (it->second->Type == type)
            entities.push_back(it->second);
    }
}

uint EntityManager::GetEntitiesCount(EntityType type)
{
    return entitiesCount[(int)type];
}

void EntityManager::GetItems(ItemVec& items)
{
    items.reserve(items.size() + entitiesCount[(int)EntityType::Item]);
    for (auto it = allEntities.begin(); it != allEntities.end(); ++it)
    {
        if (it->second->Type == EntityType::Item)
            items.push_back((Item*)it->second);
    }
}

void EntityManager::GetCritterItems(uint crid, ItemVec& items)
{
    for (auto it = allEntities.begin(); it != allEntities.end(); ++it)
    {
        if (it->second->Type == EntityType::Item)
        {
            Item* item = (Item*)it->second;
            if (item->GetAccessory() == ITEM_ACCESSORY_CRITTER && item->GetCritId() == crid)
                items.push_back(item);
        }
    }
}

Critter* EntityManager::GetCritter(uint id)
{
    auto it = allEntities.find(id);
    if (it != allEntities.end() && (it->second->Type == EntityType::Npc || it->second->Type == EntityType::Client))
        return (Critter*)it->second;
    return nullptr;
}

void EntityManager::GetCritters(CritterVec& critters)
{
    critters.reserve(critters.size() + entitiesCount[(int)EntityType::Npc] + entitiesCount[(int)EntityType::Client]);
    for (auto it = allEntities.begin(); it != allEntities.end(); ++it)
    {
        if (it->second->Type == EntityType::Npc || it->second->Type == EntityType::Client)
            critters.push_back((Critter*)it->second);
    }
}

Map* EntityManager::GetMapByPid(hash pid, uint skip_count)
{
    for (auto it = allEntities.begin(); it != allEntities.end(); ++it)
    {
        if (it->second->Type == EntityType::Map)
        {
            Map* map = (Map*)it->second;
            if (map->GetProtoId() == pid)
            {
                if (!skip_count)
                    return map;
                else
                    skip_count--;
            }
        }
    }
    return nullptr;
}

void EntityManager::GetMaps(MapVec& maps)
{
    maps.reserve(maps.size() + entitiesCount[(int)EntityType::Map]);
    for (auto it = allEntities.begin(); it != allEntities.end(); ++it)
    {
        if (it->second->Type == EntityType::Map)
            maps.push_back((Map*)it->second);
    }
}

Location* EntityManager::GetLocationByPid(hash pid, uint skip_count)
{
    for (auto it = allEntities.begin(); it != allEntities.end(); ++it)
    {
        if (it->second->Type == EntityType::Location)
        {
            Location* loc = (Location*)it->second;
            if (loc->GetProtoId() == pid)
            {
                if (!skip_count)
                    return loc;
                else
                    skip_count--;
            }
        }
    }
    return nullptr;
}

void EntityManager::GetLocations(LocationVec& locs)
{
    locs.reserve(locs.size() + entitiesCount[(int)EntityType::Location]);
    for (auto it = allEntities.begin(); it != allEntities.end(); ++it)
    {
        if (it->second->Type == EntityType::Location)
            locs.push_back((Location*)it->second);
    }
}

bool EntityManager::LoadEntities()
{
    WriteLog("Load entities...\n");

    EntityType query[] = {
        EntityType::Location,
        EntityType::Map,
        EntityType::Npc,
        EntityType::Item,
        EntityType::Custom,
    };

    StrVec custom_types = Script::GetCustomEntityTypes();
    int custom_index = -1;

    for (int q = 0; q < 5; q++)
    {
        EntityType type = query[q];

        if (type == EntityType::Custom)
        {
            custom_index++;
            if (custom_index >= (int)custom_types.size())
                break;

            q--;
        }

        string collection_name;
        if (type == EntityType::Location)
            collection_name = "Locations";
        else if (type == EntityType::Map)
            collection_name = "Maps";
        else if (type == EntityType::Npc)
            collection_name = "Critters";
        else if (type == EntityType::Item)
            collection_name = "Items";
        else if (type == EntityType::Custom)
            collection_name = custom_types[custom_index] + "s";

        UIntVec ids = DbStorage->GetAllIds(collection_name);
        for (uint id : ids)
        {
            DataBase::Document doc = DbStorage->Get(collection_name, id);
            auto proto_it = doc.find("_Proto");
            RUNTIME_ASSERT(proto_it != doc.end());
            RUNTIME_ASSERT((proto_it->second.index() == DataBase::StringValue));

            hash proto_id =
                (!std::get<string>(proto_it->second).empty() ? _str(std::get<string>(proto_it->second)).toHash() : 0);

            if (type == EntityType::Location)
            {
                if (!mapMngr.RestoreLocation(id, proto_id, doc))
                {
                    WriteLog("Fail to restore location {}.\n", id);
                    continue;
                }
            }
            else if (type == EntityType::Map)
            {
                if (!mapMngr.RestoreMap(id, proto_id, doc))
                {
                    WriteLog("Fail to restore map {}.\n", id);
                    continue;
                }
            }
            else if (type == EntityType::Npc)
            {
                if (!crMngr.RestoreNpc(id, proto_id, doc))
                {
                    WriteLog("Fail to restore npc {}.\n", id);
                    continue;
                }
            }
            else if (type == EntityType::Item)
            {
                if (!itemMngr.RestoreItem(id, proto_id, doc))
                {
                    WriteLog("Fail to restore item {}.\n", id);
                    continue;
                }
            }
            else if (type == EntityType::Custom)
            {
                if (!Script::RestoreCustomEntity(custom_types[custom_index], id, doc))
                {
                    WriteLog("Fail to restore entity {} of type {}.\n", id, custom_types[custom_index]);
                    continue;
                }
            }
            else
            {
                UNREACHABLE_PLACE;
            }
        }
    }

    WriteLog("Load entities complete.\n");

    if (!LinkMaps())
        return false;
    if (!LinkNpc())
        return false;
    if (!LinkItems())
        return false;
    InitAfterLoad();
    return true;
}

bool EntityManager::LinkMaps()
{
    WriteLog("Link maps...\n");

    // Link maps to locations
    int errors = 0;
    MapVec maps;
    mapMngr.GetMaps(maps);
    for (auto& map : maps)
    {
        uint loc_id = map->GetLocId();
        uint loc_map_index = map->GetLocMapIndex();
        Location* loc = mapMngr.GetLocation(loc_id);
        if (!loc)
        {
            WriteLog("Location {} for map '{}' ({}) not found.\n", loc_id, map->GetName(), map->Id);
            errors++;
            continue;
        }

        MapVec& maps = loc->GetMapsRaw();
        if (loc_map_index >= (uint)maps.size())
            maps.resize(loc_map_index + 1);
        maps[loc_map_index] = map;
        map->SetLocation(loc);
    }

    // Verify linkage result
    LocationVec locs;
    mapMngr.GetLocations(locs);
    for (auto& loc : locs)
    {
        MapVec& maps = loc->GetMapsRaw();
        for (size_t i = 0; i < maps.size(); i++)
        {
            if (!maps[i])
            {
                WriteLog("Location '{}' ({}) map index {} is empty.\n", loc->GetName(), loc->Id, i);
                errors++;
                continue;
            }
        }
    }

    WriteLog("Link maps complete.\n");
    return errors == 0;
}

bool EntityManager::LinkNpc()
{
    WriteLog("Link npc...\n");

    int errors = 0;
    CritterVec critters;
    crMngr.GetCritters(critters);
    CritterVec critters_groups;
    critters_groups.reserve(critters.size());

    // Move all critters to local maps and global map leaders
    for (auto it = critters.begin(), end = critters.end(); it != end; ++it)
    {
        Critter* cr = *it;
        if (!cr->GetMapId() && cr->GetGlobalMapLeaderId() && cr->GetGlobalMapLeaderId() != cr->Id)
        {
            critters_groups.push_back(cr);
            continue;
        }

        Map* map = mapMngr.GetMap(cr->GetMapId());
        if (cr->GetMapId() && !map)
        {
            WriteLog("Map {} not found, critter '{}', hx {}, hy {}.\n", cr->GetMapId(), cr->GetName(), cr->GetHexX(),
                cr->GetHexY());
            errors++;
            continue;
        }

        if (!mapMngr.CanAddCrToMap(cr, map, cr->GetHexX(), cr->GetHexY(), 0))
        {
            WriteLog("Error parsing npc '{}' ({}) to map {}, hx {}, hy {}.\n", cr->GetName(), cr->Id, cr->GetMapId(),
                cr->GetHexX(), cr->GetHexY());
            errors++;
            continue;
        }
        mapMngr.AddCrToMap(cr, map, cr->GetHexX(), cr->GetHexY(), cr->GetDir(), 0);

        if (!map)
            cr->SetGlobalMapTripId(cr->GetGlobalMapTripId() - 1);
    }

    // Move critters to global groups
    for (auto it = critters_groups.begin(), end = critters_groups.end(); it != end; ++it)
    {
        Critter* cr = *it;
        if (!mapMngr.CanAddCrToMap(cr, nullptr, 0, 0, cr->GetGlobalMapLeaderId()))
        {
            WriteLog("Error parsing npc to global group, critter '{}', rule id {}.\n", cr->GetName(),
                cr->GetGlobalMapLeaderId());
            errors++;
            continue;
        }
        mapMngr.AddCrToMap(cr, nullptr, 0, 0, 0, cr->GetGlobalMapLeaderId());
    }

    WriteLog("Link npc complete.\n");
    return errors == 0;
}

bool EntityManager::LinkItems()
{
    WriteLog("Link items...\n");

    int errors = 0;
    CritterVec critters;
    crMngr.GetCritters(critters);
    ItemVec game_items;
    itemMngr.GetGameItems(game_items);
    for (auto& item : game_items)
    {
        if (item->IsStatic())
        {
            WriteLog("Item '{}' ({}) is static.\n", item->GetName(), item->Id);
            errors++;
            continue;
        }

        switch (item->GetAccessory())
        {
        case ITEM_ACCESSORY_CRITTER: {
            if (IS_CLIENT_ID(item->GetCritId()))
                continue; // Skip player

            Critter* npc = crMngr.GetNpc(item->GetCritId());
            if (!npc)
            {
                WriteLog("Item '{}' ({}) npc not found, npc id {}.\n", item->GetName(), item->Id, item->GetCritId());
                errors++;
                continue;
            }

            npc->SetItem(item);
        }
        break;
        case ITEM_ACCESSORY_HEX: {
            Map* map = mapMngr.GetMap(item->GetMapId());
            if (!map)
            {
                WriteLog("Item '{}' ({}) map not found, map id {}, hx {}, hy {}.\n", item->GetName(), item->Id,
                    item->GetMapId(), item->GetHexX(), item->GetHexY());
                errors++;
                continue;
            }

            if (item->GetHexX() >= map->GetWidth() || item->GetHexY() >= map->GetHeight())
            {
                WriteLog("Item '{}' ({}) invalid hex position, hx {}, hy {}.\n", item->GetName(), item->Id,
                    item->GetHexX(), item->GetHexY());
                errors++;
                continue;
            }

            map->SetItem(item, item->GetHexX(), item->GetHexY());
        }
        break;
        case ITEM_ACCESSORY_CONTAINER: {
            Item* cont = itemMngr.GetItem(item->GetContainerId());
            if (!cont)
            {
                WriteLog("Item '{}' ({}) container not found, container id {}.\n", item->GetName(), item->Id,
                    item->GetContainerId());
                errors++;
                continue;
            }

            itemMngr.SetItemToContainer(cont, item);
        }
        break;
        default:
            WriteLog("Unknown accessory id '{}' ({}), acc {}.\n", item->GetName(), item->Id, item->GetAccessory());
            errors++;
            continue;
        }
    }

    WriteLog("Link items complete.\n");
    return errors == 0;
}

void EntityManager::InitAfterLoad()
{
    WriteLog("Init entities after load...\n");

    // Process visible
    CritterVec critters;
    crMngr.GetCritters(critters);
    for (auto& cr : critters)
    {
        mapMngr.ProcessVisibleCritters(cr);
        mapMngr.ProcessVisibleItems(cr);
    }

    // Other initialization
    EntityMap entities = allEntities;
    for (auto it = entities.begin(); it != entities.end(); ++it)
        it->second->AddRef();

    for (auto it = entities.begin(); it != entities.end(); ++it)
    {
        Entity* entity = it->second;
        if (entity->IsDestroyed)
        {
            entity->Release();
            continue;
        }

        if (entity->Type == EntityType::Location)
        {
            Location* loc = (Location*)entity;
            Script::RaiseInternalEvent(ServerFunctions.LocationInit, loc, false);
        }
        else if (entity->Type == EntityType::Map)
        {
            Map* map = (Map*)entity;
            Script::RaiseInternalEvent(ServerFunctions.MapInit, map, false);
            if (!map->IsDestroyed && map->GetScriptId())
                map->SetScript(nullptr, false);
        }
        else if (entity->Type == EntityType::Npc)
        {
            Npc* npc = (Npc*)entity;
            Script::RaiseInternalEvent(ServerFunctions.CritterInit, npc, false);
            if (!npc->IsDestroyed && npc->GetScriptId())
                npc->SetScript(nullptr, false);
        }
        else if (entity->Type == EntityType::Item)
        {
            Item* item = (Item*)entity;
            if (item->GetIsRadio())
                itemMngr.RadioRegister(item, true);
            if (!item->IsDestroyed)
                Script::RaiseInternalEvent(ServerFunctions.ItemInit, item, false);
            if (!item->IsDestroyed && item->GetScriptId())
                item->SetScript(nullptr, false);
        }

        entity->Release();
    }

    WriteLog("Init entities after load complete.\n");
}

void EntityManager::ClearEntities()
{
    for (auto it = allEntities.begin(); it != allEntities.end(); ++it)
    {
        it->second->IsDestroyed = true;
        Script::RemoveEventsEntity(it->second);
        entitiesCount[(int)it->second->Type]--;
        SAFEREL(it->second);
    }
    allEntities.clear();

    for (int i = 0; i < (int)EntityType::Max; i++)
        RUNTIME_ASSERT(entitiesCount[i] == 0);
}
