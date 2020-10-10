//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include "EntityManager.h"
#include "CritterManager.h"
#include "ItemManager.h"
#include "Log.h"
#include "MapManager.h"
#include "PropertiesSerializator.h"
#include "StringUtils.h"

EntityManager::EntityManager(MapManager& map_mngr, CritterManager& cr_mngr, ItemManager& item_mngr, ServerScriptSystem& script_sys, DataBase* db_storage, GlobalVars* globs) : _mapMngr {map_mngr}, _crMngr {cr_mngr}, _itemMngr {item_mngr}, _scriptSys {script_sys}, _dbStorage {db_storage}, _globals {globs}
{
}

void EntityManager::RegisterEntity(Entity* entity)
{
    if (entity->Id == 0u) {
        auto id = _globals->GetLastEntityId() + 1;
        id = std::max(id, 2u);
        _globals->SetLastEntityId(id);

        entity->SetId(id);

        auto doc = PropertiesSerializator::SaveToDbDocument(&entity->Props, entity->Proto != nullptr ? &entity->Proto->Props : nullptr, _scriptSys);
        doc["_Proto"] = entity->Proto != nullptr ? entity->Proto->GetName() : "";

        if (entity->Type == EntityType::Location) {
            _dbStorage->Insert("Locations", id, doc);
        }
        else if (entity->Type == EntityType::Map) {
            _dbStorage->Insert("Maps", id, doc);
        }
        else if (entity->Type == EntityType::Npc) {
            _dbStorage->Insert("Critters", id, doc);
        }
        else if (entity->Type == EntityType::Item) {
            _dbStorage->Insert("Items", id, doc);
        }
        else if (entity->Type == EntityType::Custom) {
            _dbStorage->Insert(entity->Props.GetRegistrator()->GetClassName() + "s", id, doc);
        }
        else {
            throw UnreachablePlaceException(LINE_STR);
        }
    }

    const auto [it, inserted] = _allEntities.insert(std::make_pair(entity->Id, entity));
    RUNTIME_ASSERT(inserted);
}

void EntityManager::UnregisterEntity(Entity* entity)
{
    const auto it = _allEntities.find(entity->Id);
    RUNTIME_ASSERT(it != _allEntities.end());
    _allEntities.erase(it);

    _scriptSys.RemoveEntity(entity);

    if (entity->Type == EntityType::Location) {
        _dbStorage->Delete("Locations", entity->Id);
    }
    else if (entity->Type == EntityType::Map) {
        _dbStorage->Delete("Maps", entity->Id);
    }
    else if (entity->Type == EntityType::Npc) {
        _dbStorage->Delete("Critters", entity->Id);
    }
    else if (entity->Type == EntityType::Item) {
        _dbStorage->Delete("Items", entity->Id);
    }
    else if (entity->Type == EntityType::Custom) {
        _dbStorage->Delete(entity->Props.GetRegistrator()->GetClassName() + "s", entity->Id);
    }

    entity->SetId(0);
}

auto EntityManager::GetEntity(uint id, EntityType type) -> Entity*
{
    const auto it = _allEntities.find(id);
    if (it != _allEntities.end() && it->second->Type == type) {
        return it->second;
    }
    return nullptr;
}

void EntityManager::GetEntities(EntityType type, EntityVec& entities)
{
    entities.reserve(entities.size() + _entitiesCount[static_cast<int>(type)]);
    for (auto& [id, entity] : _allEntities) {
        if (entity->Type == type) {
            entities.push_back(entity);
        }
    }
}

auto EntityManager::GetEntitiesCount(EntityType type) const -> uint
{
    return _entitiesCount[static_cast<int>(type)];
}

void EntityManager::GetItems(ItemVec& items)
{
    items.reserve(items.size() + _entitiesCount[static_cast<int>(EntityType::Item)]);
    for (auto& [id, entity] : _allEntities) {
        if (entity->Type == EntityType::Item) {
            items.push_back(dynamic_cast<Item*>(entity));
        }
    }
}

void EntityManager::GetCritterItems(uint crid, ItemVec& items)
{
    for (auto& [id, entity] : _allEntities) {
        if (entity->Type == EntityType::Item) {
            auto* item = dynamic_cast<Item*>(entity);
            if (item->GetAccessory() == ITEM_ACCESSORY_CRITTER && item->GetCritId() == crid) {
                items.push_back(item);
            }
        }
    }
}

auto EntityManager::GetCritter(uint id) -> Critter*
{
    const auto it = _allEntities.find(id);
    if (it != _allEntities.end() && (it->second->Type == EntityType::Npc || it->second->Type == EntityType::Client)) {
        return dynamic_cast<Critter*>(it->second);
    }
    return nullptr;
}

void EntityManager::GetCritters(CritterVec& critters)
{
    critters.reserve(critters.size() + _entitiesCount[static_cast<int>(EntityType::Npc)] + _entitiesCount[static_cast<int>(EntityType::Client)]);
    for (auto& [id, entity] : _allEntities) {
        if (entity->Type == EntityType::Npc || entity->Type == EntityType::Client) {
            critters.push_back(dynamic_cast<Critter*>(entity));
        }
    }
}

auto EntityManager::GetMapByPid(hash pid, uint skip_count) -> Map*
{
    for (auto& [id, entity] : _allEntities) {
        if (entity->Type == EntityType::Map) {
            auto* map = dynamic_cast<Map*>(entity);
            if (map->GetProtoId() == pid) {
                if (skip_count == 0u) {
                    return map;
                }

                skip_count--;
            }
        }
    }
    return nullptr;
}

void EntityManager::GetMaps(MapVec& maps)
{
    maps.reserve(maps.size() + _entitiesCount[static_cast<int>(EntityType::Map)]);
    for (auto& [id, entity] : _allEntities) {
        if (entity->Type == EntityType::Map) {
            maps.push_back(dynamic_cast<Map*>(entity));
        }
    }
}

auto EntityManager::GetLocationByPid(hash pid, uint skip_count) -> Location*
{
    for (auto& [id, entity] : _allEntities) {
        if (entity->Type == EntityType::Location) {
            auto* loc = dynamic_cast<Location*>(entity);
            if (loc->GetProtoId() == pid) {
                if (skip_count == 0u) {
                    return loc;
                }

                skip_count--;
            }
        }
    }
    return nullptr;
}

void EntityManager::GetLocations(LocationVec& locs)
{
    locs.reserve(locs.size() + _entitiesCount[static_cast<int>(EntityType::Location)]);
    for (auto& [id, entity] : _allEntities) {
        if (entity->Type == EntityType::Location) {
            locs.push_back(dynamic_cast<Location*>(entity));
        }
    }
}

auto EntityManager::LoadEntities() -> bool
{
    NON_CONST_METHOD_HINT(_dummy);

    WriteLog("Load entities...\n");

    EntityType query[] = {
        EntityType::Location,
        EntityType::Map,
        EntityType::Npc,
        EntityType::Item,
        EntityType::Custom,
    };

    StrVec custom_types; // = scriptSys.GetCustomEntityTypes();
    auto custom_index = -1;

    for (auto q = 0; q < 5; q++) {
        const auto type = query[q];

        if (type == EntityType::Custom) {
            custom_index++;
            if (custom_index >= static_cast<int>(custom_types.size())) {
                break;
            }

            q--;
        }

        string collection_name;
        if (type == EntityType::Location) {
            collection_name = "Locations";
        }
        else if (type == EntityType::Map) {
            collection_name = "Maps";
        }
        else if (type == EntityType::Npc) {
            collection_name = "Critters";
        }
        else if (type == EntityType::Item) {
            collection_name = "Items";
        }
        else if (type == EntityType::Custom) {
            collection_name = custom_types[custom_index] + "s";
        }

        auto ids = _dbStorage->GetAllIds(collection_name);
        for (auto id : ids) {
            auto doc = _dbStorage->Get(collection_name, id);
            auto proto_it = doc.find("_Proto");
            RUNTIME_ASSERT(proto_it != doc.end());
            RUNTIME_ASSERT((proto_it->second.index() == DataBase::STRING_VALUE));

            const auto proto_id = !std::get<string>(proto_it->second).empty() ? _str(std::get<string>(proto_it->second)).toHash() : 0;

            if (type == EntityType::Location) {
                if (!_mapMngr.RestoreLocation(id, proto_id, doc)) {
                    WriteLog("Fail to restore location {}.\n", id);
                }
            }
            else if (type == EntityType::Map) {
                if (!_mapMngr.RestoreMap(id, proto_id, doc)) {
                    WriteLog("Fail to restore map {}.\n", id);
                }
            }
            else if (type == EntityType::Npc) {
                if (!_crMngr.RestoreNpc(id, proto_id, doc)) {
                    WriteLog("Fail to restore npc {}.\n", id);
                }
            }
            else if (type == EntityType::Item) {
                if (!_itemMngr.RestoreItem(id, proto_id, doc)) {
                    WriteLog("Fail to restore item {}.\n", id);
                }
            }
            else if (type == EntityType::Custom) {
                /*if (!scriptSys.RestoreCustomEntity(custom_types[custom_index], id, doc))
                {
                    WriteLog("Fail to restore entity {} of type {}.\n", id, custom_types[custom_index]);
                    continue;
                }*/
            }
            else {
                throw UnreachablePlaceException(LINE_STR);
            }
        }
    }

    WriteLog("Load entities complete.\n");

    if (!LinkMaps()) {
        return false;
    }
    if (!LinkNpc()) {
        return false;
    }
    if (!LinkItems()) {
        return false;
    }
    InitAfterLoad();
    return true;
}

auto EntityManager::LinkMaps() -> bool
{
    NON_CONST_METHOD_HINT(_dummy);

    WriteLog("Link maps...\n");

    // Link maps to locations
    auto errors = 0;
    MapVec maps;
    _mapMngr.GetMaps(maps);
    for (auto& map : maps) {
        const auto loc_id = map->GetLocId();
        const auto loc_map_index = map->GetLocMapIndex();

        auto* loc = _mapMngr.GetLocation(loc_id);
        if (loc == nullptr) {
            WriteLog("Location {} for map '{}' ({}) not found.\n", loc_id, map->GetName(), map->Id);
            errors++;
            continue;
        }

        auto& loc_maps = loc->GetMapsRaw();
        if (loc_map_index >= static_cast<uint>(loc_maps.size())) {
            loc_maps.resize(loc_map_index + 1);
        }

        loc_maps[loc_map_index] = map;
        map->SetLocation(loc);
    }

    // Verify linkage result
    LocationVec locs;
    _mapMngr.GetLocations(locs);
    for (auto& loc : locs) {
        auto& loc_maps = loc->GetMapsRaw();
        for (size_t i = 0; i < loc_maps.size(); i++) {
            if (loc_maps[i] == nullptr) {
                WriteLog("Location '{}' ({}) map index {} is empty.\n", loc->GetName(), loc->Id, i);
                errors++;
            }
        }
    }

    WriteLog("Link maps complete.\n");
    return errors == 0;
}

auto EntityManager::LinkNpc() -> bool
{
    NON_CONST_METHOD_HINT(_dummy);

    WriteLog("Link npc...\n");

    auto errors = 0;
    CritterVec critters;
    _crMngr.GetCritters(critters);
    CritterVec critters_groups;
    critters_groups.reserve(critters.size());

    // Move all critters to local maps and global map leaders
    for (auto* cr : critters) {
        if (cr->GetMapId() == 0u && cr->GetGlobalMapLeaderId() != 0u && cr->GetGlobalMapLeaderId() != cr->Id) {
            critters_groups.push_back(cr);
            continue;
        }

        auto* map = _mapMngr.GetMap(cr->GetMapId());
        if (cr->GetMapId() != 0u && map == nullptr) {
            WriteLog("Map {} not found, critter '{}', hx {}, hy {}.\n", cr->GetMapId(), cr->GetName(), cr->GetHexX(), cr->GetHexY());
            errors++;
            continue;
        }

        if (!_mapMngr.CanAddCrToMap(cr, map, cr->GetHexX(), cr->GetHexY(), 0)) {
            WriteLog("Error parsing npc '{}' ({}) to map {}, hx {}, hy {}.\n", cr->GetName(), cr->Id, cr->GetMapId(), cr->GetHexX(), cr->GetHexY());
            errors++;
            continue;
        }

        _mapMngr.AddCrToMap(cr, map, cr->GetHexX(), cr->GetHexY(), cr->GetDir(), 0);

        if (map == nullptr) {
            cr->SetGlobalMapTripId(cr->GetGlobalMapTripId() - 1);
        }
    }

    // Move critters to global groups
    for (auto* cr : critters_groups) {
        if (!_mapMngr.CanAddCrToMap(cr, nullptr, 0, 0, cr->GetGlobalMapLeaderId())) {
            WriteLog("Error parsing npc to global group, critter '{}', rule id {}.\n", cr->GetName(), cr->GetGlobalMapLeaderId());
            errors++;
            continue;
        }
        _mapMngr.AddCrToMap(cr, nullptr, 0, 0, 0, cr->GetGlobalMapLeaderId());
    }

    WriteLog("Link npc complete.\n");
    return errors == 0;
}

auto EntityManager::LinkItems() -> bool
{
    NON_CONST_METHOD_HINT(_dummy);

    WriteLog("Link items...\n");

    auto errors = 0;
    CritterVec critters;
    _crMngr.GetCritters(critters);
    ItemVec game_items;
    _itemMngr.GetGameItems(game_items);
    for (auto& item : game_items) {
        if (item->IsStatic()) {
            WriteLog("Item '{}' ({}) is static.\n", item->GetName(), item->Id);
            errors++;
            continue;
        }

        switch (item->GetAccessory()) {
        case ITEM_ACCESSORY_CRITTER: {
            if (IS_CLIENT_ID(item->GetCritId())) {
                continue; // Skip player
            }

            Critter* npc = _crMngr.GetNpc(item->GetCritId());
            if (npc == nullptr) {
                WriteLog("Item '{}' ({}) npc not found, npc id {}.\n", item->GetName(), item->Id, item->GetCritId());
                errors++;
                continue;
            }

            npc->SetItem(item);
        } break;
        case ITEM_ACCESSORY_HEX: {
            auto* map = _mapMngr.GetMap(item->GetMapId());
            if (map == nullptr) {
                WriteLog("Item '{}' ({}) map not found, map id {}, hx {}, hy {}.\n", item->GetName(), item->Id, item->GetMapId(), item->GetHexX(), item->GetHexY());
                errors++;
                continue;
            }

            if (item->GetHexX() >= map->GetWidth() || item->GetHexY() >= map->GetHeight()) {
                WriteLog("Item '{}' ({}) invalid hex position, hx {}, hy {}.\n", item->GetName(), item->Id, item->GetHexX(), item->GetHexY());
                errors++;
                continue;
            }

            map->SetItem(item, item->GetHexX(), item->GetHexY());
        } break;
        case ITEM_ACCESSORY_CONTAINER: {
            auto* cont = _itemMngr.GetItem(item->GetContainerId());
            if (cont == nullptr) {
                WriteLog("Item '{}' ({}) container not found, container id {}.\n", item->GetName(), item->Id, item->GetContainerId());
                errors++;
                continue;
            }

            _itemMngr.SetItemToContainer(cont, item);
        } break;
        default:
            WriteLog("Unknown accessory id '{}' ({}), acc {}.\n", item->GetName(), item->Id, item->GetAccessory());
            errors++;
        }
    }

    WriteLog("Link items complete.\n");
    return errors == 0;
}

void EntityManager::InitAfterLoad()
{
    NON_CONST_METHOD_HINT(_dummy);

    WriteLog("Init entities after load...\n");

    // Process visible
    CritterVec critters;
    _crMngr.GetCritters(critters);
    for (auto& cr : critters) {
        _mapMngr.ProcessVisibleCritters(cr);
        _mapMngr.ProcessVisibleItems(cr);
    }

    // Other initialization
    auto entities = _allEntities;
    for (auto& [id, entity] : entities) {
        entity->AddRef();
    }

    for (auto& [id, entity] : entities) {
        if (entity->IsDestroyed) {
            entity->Release();
            continue;
        }

        if (entity->Type == EntityType::Location) {
            auto* loc = dynamic_cast<Location*>(entity);
            _scriptSys.LocationInitEvent(loc, false);
        }
        else if (entity->Type == EntityType::Map) {
            auto* map = dynamic_cast<Map*>(entity);
            _scriptSys.MapInitEvent(map, false);
            if (!map->IsDestroyed && map->GetScriptId() != 0u) {
                map->SetScript(nullptr, false);
            }
        }
        else if (entity->Type == EntityType::Npc) {
            auto* npc = dynamic_cast<Npc*>(entity);
            _scriptSys.CritterInitEvent(npc, false);
            if (!npc->IsDestroyed && npc->GetScriptId() != 0u) {
                npc->SetScript(nullptr, false);
            }
        }
        else if (entity->Type == EntityType::Item) {
            auto* item = dynamic_cast<Item*>(entity);
            if (item->GetIsRadio()) {
                _itemMngr.RadioRegister(item, true);
            }
            if (!item->IsDestroyed) {
                _scriptSys.ItemInitEvent(item, false);
            }
            if (!item->IsDestroyed && item->GetScriptId() != 0u) {
                item->SetScript(nullptr, false);
            }
        }

        entity->Release();
    }

    WriteLog("Init entities after load complete.\n");
}

void EntityManager::ClearEntities()
{
    for (auto& [id, entity] : _allEntities) {
        entity->IsDestroyed = true;
        _scriptSys.RemoveEntity(entity);
        _entitiesCount[static_cast<int>(entity->Type)]--;
        entity->Release();
    }
    _allEntities.clear();

    for (auto& i : _entitiesCount) {
        RUNTIME_ASSERT(i == 0);
    }
}
