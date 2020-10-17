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
#include "Log.h"
#include "PropertiesSerializator.h"
#include "ProtoManager.h"
#include "StringUtils.h"

EntityManager::EntityManager(ProtoManager& proto_mngr, ServerScriptSystem& script_sys, DataBase& db_storage, GlobalVars* globs) : _protoMngr {proto_mngr}, _scriptSys {script_sys}, _dbStorage {db_storage}, _globals {globs}
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
            _dbStorage.Insert("Locations", id, doc);
        }
        else if (entity->Type == EntityType::Map) {
            _dbStorage.Insert("Maps", id, doc);
        }
        else if (entity->Type == EntityType::Npc) {
            _dbStorage.Insert("Critters", id, doc);
        }
        else if (entity->Type == EntityType::Item) {
            _dbStorage.Insert("Items", id, doc);
        }
        else if (entity->Type == EntityType::Custom) {
            _dbStorage.Insert(entity->Props.GetRegistrator()->GetClassName() + "s", id, doc);
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
        _dbStorage.Delete("Locations", entity->Id);
    }
    else if (entity->Type == EntityType::Map) {
        _dbStorage.Delete("Maps", entity->Id);
    }
    else if (entity->Type == EntityType::Npc) {
        _dbStorage.Delete("Critters", entity->Id);
    }
    else if (entity->Type == EntityType::Item) {
        _dbStorage.Delete("Items", entity->Id);
    }
    else if (entity->Type == EntityType::Custom) {
        _dbStorage.Delete(entity->Props.GetRegistrator()->GetClassName() + "s", entity->Id);
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

auto EntityManager::GetEntities(EntityType type) -> vector<Entity*>
{
    vector<Entity*> entities;
    entities.reserve(entities.size() + _entitiesCount[static_cast<int>(type)]);

    for (auto [id, entity] : _allEntities) {
        if (entity->Type == type) {
            entities.push_back(entity);
        }
    }

    return entities;
}

auto EntityManager::GetEntitiesCount(EntityType type) const -> uint
{
    return _entitiesCount[static_cast<int>(type)];
}

auto EntityManager::GetItem(uint id) -> Item*
{
    const auto it = _allEntities.find(id);
    if (it != _allEntities.end() && it->second->Type == EntityType::Item) {
        return dynamic_cast<Item*>(it->second);
    }
    return nullptr;
}

auto EntityManager::GetItems() -> vector<Item*>
{
    vector<Item*> items;
    items.reserve(items.size() + _entitiesCount[static_cast<int>(EntityType::Item)]);

    for (auto [id, entity] : _allEntities) {
        if (entity->Type == EntityType::Item) {
            items.push_back(dynamic_cast<Item*>(entity));
        }
    }

    return items;
}

auto EntityManager::FindCritterItems(uint crid) -> vector<Item*>
{
    vector<Item*> items;
    for (auto [id, entity] : _allEntities) {
        if (entity->Type == EntityType::Item) {
            auto* item = dynamic_cast<Item*>(entity);
            if (item->GetAccessory() == ITEM_ACCESSORY_CRITTER && item->GetCritId() == crid) {
                items.push_back(item);
            }
        }
    }
    return items;
}

auto EntityManager::GetCritter(uint id) -> Critter*
{
    const auto it = _allEntities.find(id);
    if (it != _allEntities.end() && (it->second->Type == EntityType::Npc || it->second->Type == EntityType::Client)) {
        return dynamic_cast<Critter*>(it->second);
    }
    return nullptr;
}

auto EntityManager::GetCritters() -> vector<Critter*>
{
    NON_CONST_METHOD_HINT(_dummy);

    vector<Critter*> critters;
    critters.reserve(critters.size() + _entitiesCount[static_cast<int>(EntityType::Npc)] + _entitiesCount[static_cast<int>(EntityType::Client)]);

    for (auto [id, entity] : _allEntities) {
        if (entity->Type == EntityType::Npc || entity->Type == EntityType::Client) {
            critters.push_back(dynamic_cast<Critter*>(entity));
        }
    }

    return critters;
}

auto EntityManager::GetMap(uint id) -> Map*
{
    const auto it = _allEntities.find(id);
    if (it != _allEntities.end() && it->second->Type == EntityType::Map) {
        return dynamic_cast<Map*>(it->second);
    }
    return nullptr;
}

auto EntityManager::GetMapByPid(hash pid, uint skip_count) -> Map*
{
    for (auto [id, entity] : _allEntities) {
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

auto EntityManager::GetMaps() -> vector<Map*>
{
    vector<Map*> maps;
    maps.reserve(maps.size() + _entitiesCount[static_cast<int>(EntityType::Map)]);

    for (auto [id, entity] : _allEntities) {
        if (entity->Type == EntityType::Map) {
            maps.push_back(dynamic_cast<Map*>(entity));
        }
    }

    return maps;
}

auto EntityManager::GetLocation(uint id) -> Location*
{
    const auto it = _allEntities.find(id);
    if (it != _allEntities.end() && it->second->Type == EntityType::Location) {
        return dynamic_cast<Location*>(it->second);
    }
    return nullptr;
}

auto EntityManager::GetLocationByPid(hash pid, uint skip_count) -> Location*
{
    for (auto [id, entity] : _allEntities) {
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

auto EntityManager::GetLocations() -> vector<Location*>
{
    vector<Location*> locations;
    locations.reserve(locations.size() + _entitiesCount[static_cast<int>(EntityType::Location)]);

    for (auto [id, entity] : _allEntities) {
        if (entity->Type == EntityType::Location) {
            locations.push_back(dynamic_cast<Location*>(entity));
        }
    }

    return locations;
}

void EntityManager::LoadEntities(const LocationFabric& loc_fabric, const MapFabric& map_fabric, const NpcFabric& npc_fabric, const ItemFabric& item_fabric)
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

    vector<string> custom_types; // = scriptSys.GetCustomEntityTypes();
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

        auto ids = _dbStorage.GetAllIds(collection_name);
        for (auto id : ids) {
            auto doc = _dbStorage.Get(collection_name, id);

            auto proto_it = doc.find("_Proto");
            if (proto_it == doc.end()) {
                throw EntitiesLoadException("'_Proto' section not found in entity", collection_name, id);
            }
            if (proto_it->second.index() != DataBase::STRING_VALUE) {
                throw EntitiesLoadException("'_Proto' section is not string type", collection_name, id, proto_it->second.index());
            }

            const auto proto_id = !std::get<string>(proto_it->second).empty() ? _str(std::get<string>(proto_it->second)).toHash() : 0;

            if (type == EntityType::Location) {
                const auto* proto = _protoMngr.GetProtoLocation(proto_id);
                if (proto == nullptr) {
                    throw EntitiesLoadException("Location proto not found", _str().parseHash(proto_id));
                }

                auto* loc = loc_fabric(id, proto);
                if (!PropertiesSerializator::LoadFromDbDocument(&loc->Props, doc, _scriptSys)) {
                    throw EntitiesLoadException("Failed to restore location properties", _str().parseHash(proto_id), id);
                }

                loc->BindScript();

                RegisterEntity(loc);
            }
            else if (type == EntityType::Map) {
                const auto* proto = _protoMngr.GetProtoMap(proto_id);
                if (proto == nullptr) {
                    throw EntitiesLoadException("Map proto not found", _str().parseHash(proto_id));
                }

                auto* map = map_fabric(id, proto);
                if (!PropertiesSerializator::LoadFromDbDocument(&map->Props, doc, _scriptSys)) {
                    throw EntitiesLoadException("Failed to restore map properties", _str().parseHash(proto_id), id);
                }

                RegisterEntity(map);
            }
            else if (type == EntityType::Npc) {
                const auto* proto = _protoMngr.GetProtoCritter(proto_id);
                if (proto == nullptr) {
                    throw EntitiesLoadException("Proto critter not found", _str().parseHash(proto_id));
                }

                auto* npc = npc_fabric(id, proto);
                if (!PropertiesSerializator::LoadFromDbDocument(&npc->Props, doc, _scriptSys)) {
                    throw EntitiesLoadException("Failed to restore critter properties", _str().parseHash(proto_id), id);
                }

                RegisterEntity(npc);
            }
            else if (type == EntityType::Item) {
                const auto* proto = _protoMngr.GetProtoItem(proto_id);
                if (proto == nullptr) {
                    throw EntitiesLoadException("Proto item is not loaded", _str().parseHash(proto_id));
                }

                auto* item = item_fabric(id, proto);
                if (!PropertiesSerializator::LoadFromDbDocument(&item->Props, doc, _scriptSys)) {
                    throw EntitiesLoadException("Failed to restore item properties", _str().parseHash(proto_id), id);
                }

                RegisterEntity(item);
            }
            else if (type == EntityType::Custom) {
                /*if (!scriptSys.RestoreCustomEntity(custom_types[custom_index], id, doc)) {
                    throw EntitiesLoadException("Fail to restore custom entity", id, custom_types[custom_index]);
                }*/
            }
            else {
                throw UnreachablePlaceException(LINE_STR);
            }
        }
    }

    WriteLog("Load entities complete.\n");
}

void EntityManager::InitAfterLoad()
{
    NON_CONST_METHOD_HINT(_dummy);

    WriteLog("Init entities after load...\n");

    auto entities = _allEntities;

    for (auto [id, entity] : entities) {
        entity->AddRef();
    }

    for (auto [id, entity] : entities) {
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

void EntityManager::FinalizeEntities()
{
    auto recursion_fuse = 0;

    while (!_allEntities.empty()) {
        auto entities_copy = _allEntities;
        for (auto [id, entity] : entities_copy) {
            entity->IsDestroyed = true;
            _scriptSys.RemoveEntity(entity);
            _entitiesCount[static_cast<int>(entity->Type)]--;
            entity->Release();
            _allEntities.erase(id);
        }

        if (++recursion_fuse == ENTITIES_FINALIZATION_FUSE_VALUE) {
            WriteLog("Entities finalizations fuse fired!");
            break;
        }
    }
}
