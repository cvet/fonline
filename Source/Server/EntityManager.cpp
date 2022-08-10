//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "Server.h"
#include "StringUtils.h"

EntityManager::EntityManager(FOServer* engine) : _engine {engine}
{
}

void EntityManager::RegisterEntity(Player* entity, uint id)
{
    RUNTIME_ASSERT(id != 0u);
    entity->SetId(id);
    RegisterEntityEx(entity);
    const auto [it, inserted] = _allPlayers.emplace(entity->GetId(), entity);
    RUNTIME_ASSERT(inserted);
}

void EntityManager::UnregisterEntity(Player* entity)
{
    const auto it = _allPlayers.find(entity->GetId());
    RUNTIME_ASSERT(it != _allPlayers.end());
    _allPlayers.erase(it);
    UnregisterEntityEx(entity, false);
}

void EntityManager::RegisterEntity(Location* entity)
{
    RegisterEntityEx(entity);
    const auto [it, inserted] = _allLocations.emplace(entity->GetId(), entity);
    RUNTIME_ASSERT(inserted);
}

void EntityManager::UnregisterEntity(Location* entity)
{
    const auto it = _allLocations.find(entity->GetId());
    RUNTIME_ASSERT(it != _allLocations.end());
    _allLocations.erase(it);
    UnregisterEntityEx(entity, true);
}

void EntityManager::RegisterEntity(Map* entity)
{
    RegisterEntityEx(entity);
    const auto [it, inserted] = _allMaps.emplace(entity->GetId(), entity);
    RUNTIME_ASSERT(inserted);
}

void EntityManager::UnregisterEntity(Map* entity)
{
    const auto it = _allMaps.find(entity->GetId());
    RUNTIME_ASSERT(it != _allMaps.end());
    _allMaps.erase(it);
    UnregisterEntityEx(entity, true);
}

void EntityManager::RegisterEntity(Critter* entity)
{
    RegisterEntityEx(entity);
    const auto [it, inserted] = _allCritters.emplace(entity->GetId(), entity);
    RUNTIME_ASSERT(inserted);
}

void EntityManager::UnregisterEntity(Critter* entity)
{
    const auto it = _allCritters.find(entity->GetId());
    RUNTIME_ASSERT(it != _allCritters.end());
    _allCritters.erase(it);
    UnregisterEntityEx(entity, !entity->IsOwnedByPlayer());
}

void EntityManager::RegisterEntity(Item* entity)
{
    RegisterEntityEx(entity);
    const auto [it, inserted] = _allItems.emplace(entity->GetId(), entity);
    RUNTIME_ASSERT(inserted);
}

void EntityManager::UnregisterEntity(Item* entity)
{
    const auto it = _allItems.find(entity->GetId());
    RUNTIME_ASSERT(it != _allItems.end());
    _allItems.erase(it);
    UnregisterEntityEx(entity, true);
}

void EntityManager::RegisterEntityEx(ServerEntity* entity)
{
    NON_CONST_METHOD_HINT();

    if (entity->GetId() == 0u) {
        auto id = _engine->GetLastEntityId() + 1;
        id = std::max(id, 2u);
        _engine->SetLastEntityId(id);

        entity->SetId(id);

        if (const auto* entity_with_proto = dynamic_cast<EntityWithProto*>(entity); entity_with_proto != nullptr) {
            const auto* proto = entity_with_proto->GetProto();
            auto doc = PropertiesSerializator::SaveToDocument(&entity->GetProperties(), &proto->GetProperties(), *_engine);
            doc["_Proto"] = string(proto->GetName());
            _engine->DbStorage.Insert(_str("{}s", entity->GetStorageName()), id, doc);
        }
        else {
            const auto doc = PropertiesSerializator::SaveToDocument(&entity->GetProperties(), nullptr, *_engine);
            _engine->DbStorage.Insert(_str("{}s", entity->GetStorageName()), id, doc);
        }
    }
}

void EntityManager::UnregisterEntityEx(ServerEntity* entity, bool delete_from_db)
{
    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(entity->GetId() != 0u);

    if (delete_from_db) {
        _engine->DbStorage.Delete(_str("{}s", entity->GetStorageName()), entity->GetId());
    }

    entity->SetId(0);
}

auto EntityManager::GetPlayer(uint id) -> Player*
{
    if (const auto it = _allPlayers.find(id); it != _allPlayers.end()) {
        return it->second;
    }

    return nullptr;
}

auto EntityManager::GetPlayers() -> const unordered_map<uint, Player*>&
{
    return _allPlayers;
}

auto EntityManager::GetLocation(uint id) -> Location*
{
    if (const auto it = _allLocations.find(id); it != _allLocations.end()) {
        return it->second;
    }

    return nullptr;
}

auto EntityManager::GetLocationByPid(hstring pid, uint skip_count) -> Location*
{
    for (auto&& [id, loc] : _allLocations) {
        if (loc->GetProtoId() == pid) {
            if (skip_count == 0u) {
                return loc;
            }
            skip_count--;
        }
    }

    return nullptr;
}

auto EntityManager::GetLocations() -> const unordered_map<uint, Location*>&
{
    return _allLocations;
}

auto EntityManager::GetMap(uint id) -> Map*
{
    if (const auto it = _allMaps.find(id); it != _allMaps.end()) {
        return it->second;
    }

    return nullptr;
}

auto EntityManager::GetMapByPid(hstring pid, uint skip_count) -> Map*
{
    for (auto&& [id, map] : _allMaps) {
        if (map->GetProtoId() == pid) {
            if (skip_count == 0u) {
                return map;
            }
            skip_count--;
        }
    }

    return nullptr;
}

auto EntityManager::GetMaps() -> const unordered_map<uint, Map*>&
{
    return _allMaps;
}

auto EntityManager::GetCritter(uint id) -> Critter*
{
    if (const auto it = _allCritters.find(id); it != _allCritters.end()) {
        return it->second;
    }

    return nullptr;
}

auto EntityManager::GetCritters() -> const unordered_map<uint, Critter*>&
{
    return _allCritters;
}

auto EntityManager::GetItem(uint id) -> Item*
{
    if (const auto it = _allItems.find(id); it != _allItems.end()) {
        return it->second;
    }

    return nullptr;
}

auto EntityManager::GetItems() -> const unordered_map<uint, Item*>&
{
    return _allItems;
}

auto EntityManager::GetCritterItems(uint cr_id) -> vector<Item*>
{
    vector<Item*> items;

    for (auto&& [id, item] : _allItems) {
        if (item->GetOwnership() == ItemOwnership::CritterInventory && item->GetCritterId() == cr_id) {
            items.push_back(item);
        }
    }

    return items;
}

void EntityManager::LoadEntities(const LocationFabric& loc_fabric, const MapFabric& map_fabric, const NpcFabric& npc_fabric, const ItemFabric& item_fabric)
{
    WriteLog("Load entities...");

    // Todo: load locations -> theirs maps -> critters/items on map -> items in critters/containers
    const string query[] = {
        "Locations",
        "Maps",
        "Critters",
        "Items",
    };

    for (auto q = 0; q < 4; q++) {
        string collection_name = query[q];

        const auto ids = _engine->DbStorage.GetAllIds(collection_name);

        for (const auto id : ids) {
            auto doc = _engine->DbStorage.Get(collection_name, id);

            const auto proto_it = doc.find("_Proto");
            if (proto_it == doc.end()) {
                throw EntitiesLoadException("'_Proto' section not found in entity", collection_name, id);
            }
            if (proto_it->second.index() != AnyData::STRING_VALUE) {
                throw EntitiesLoadException("'_Proto' section is not string type", collection_name, id, proto_it->second.index());
            }

            const auto& proto_name = std::get<string>(proto_it->second);
            if (proto_name.empty()) {
                throw EntitiesLoadException("'_Proto' section is empty", collection_name, id);
            }

            const auto proto_id = _engine->ToHashedString(std::get<string>(proto_it->second));

            if (q == 0) {
                const auto* proto = _engine->ProtoMngr.GetProtoLocation(proto_id);
                if (proto == nullptr) {
                    throw EntitiesLoadException("Location proto not found", proto_name);
                }

                auto* loc = loc_fabric(id, proto);
                if (!PropertiesSerializator::LoadFromDocument(&loc->GetPropertiesForEdit(), doc, *_engine)) {
                    throw EntitiesLoadException("Failed to restore location properties", proto_name, id);
                }

                loc->BindScript();

                RegisterEntity(loc);
            }
            else if (q == 1) {
                const auto* proto = _engine->ProtoMngr.GetProtoMap(proto_id);
                if (proto == nullptr) {
                    throw EntitiesLoadException("Map proto not found", proto_name);
                }

                auto* map = map_fabric(id, proto);
                if (!PropertiesSerializator::LoadFromDocument(&map->GetPropertiesForEdit(), doc, *_engine)) {
                    throw EntitiesLoadException("Failed to restore map properties", proto_name, id);
                }

                RegisterEntity(map);
            }
            else if (q == 2) {
                const auto* proto = _engine->ProtoMngr.GetProtoCritter(proto_id);
                if (proto == nullptr) {
                    throw EntitiesLoadException("Proto critter not found", proto_name);
                }

                auto* npc = npc_fabric(id, proto);
                if (!PropertiesSerializator::LoadFromDocument(&npc->GetPropertiesForEdit(), doc, *_engine)) {
                    throw EntitiesLoadException("Failed to restore critter properties", proto_name, id);
                }

                RegisterEntity(npc);
            }
            else if (q == 3) {
                const auto* proto = _engine->ProtoMngr.GetProtoItem(proto_id);
                if (proto == nullptr) {
                    throw EntitiesLoadException("Proto item is not loaded", proto_name);
                }

                auto* item = item_fabric(id, proto);
                if (!PropertiesSerializator::LoadFromDocument(&item->GetPropertiesForEdit(), doc, *_engine)) {
                    throw EntitiesLoadException("Failed to restore item properties", proto_name, id);
                }

                RegisterEntity(item);
            }
            else {
                throw UnreachablePlaceException(LINE_STR);
            }
        }
    }

    WriteLog("Load entities complete");
}

void EntityManager::InitAfterLoad()
{
    NON_CONST_METHOD_HINT();

    WriteLog("Init entities after load...");

    auto locs = copy(_allLocations);
    auto maps = copy(_allMaps);
    auto critters = copy(_allCritters);
    auto items = copy(_allItems);

    for (auto&& [id, loc] : locs) {
        if (!loc->IsDestroyed()) {
            _engine->OnLocationInit.Fire(loc, false);
        }
    }

    for (auto&& [id, map] : maps) {
        if (!map->IsDestroyed()) {
            _engine->OnMapInit.Fire(map, false);
            if (!map->IsDestroyed()) {
                ScriptHelpers::CallInitScript(_engine->ScriptSys, map, map->GetInitScript(), false);
            }
        }
    }

    for (auto&& [id, cr] : critters) {
        if (!cr->IsDestroyed()) {
            _engine->OnCritterInit.Fire(cr, false);
            if (!cr->IsDestroyed()) {
                ScriptHelpers::CallInitScript(_engine->ScriptSys, cr, cr->GetInitScript(), false);
            }
        }
    }

    for (auto&& [id, item] : items) {
        _engine->OnItemInit.Fire(item, false);
        if (!item->IsDestroyed()) {
            ScriptHelpers::CallInitScript(_engine->ScriptSys, item, item->GetInitScript(), false);
        }
    }

    WriteLog("Init entities after load complete");
}

void EntityManager::FinalizeEntities()
{
    const auto destroy_entities = [](auto& entities) {
        auto recursion_fuse = 0;

        while (!entities.empty()) {
            for (auto&& [id, entity] : copy(entities)) {
                entity->MarkAsDestroyed();
                entity->Release();
                entities.erase(id);
            }

            if (++recursion_fuse == ENTITIES_FINALIZATION_FUSE_VALUE) {
                WriteLog("Entities finalizations fuse fired!");
                break;
            }
        }
    };

    destroy_entities(_allPlayers);
    destroy_entities(_allLocations);
    destroy_entities(_allMaps);
    destroy_entities(_allCritters);
    destroy_entities(_allItems);
    for (auto& custom_entities : _allCustomEntities) {
        destroy_entities(custom_entities.second);
    }
}

auto EntityManager::GetCustomEntity(string_view entity_class_name, uint id) -> ServerEntity*
{
    auto& all_entities = _allCustomEntities[string(entity_class_name)];
    const auto it = all_entities.find(id);

    // Load if not exists
    if (it == all_entities.end()) {
        const auto doc = _engine->DbStorage.Get(_str("{}s", entity_class_name), id);
        if (doc.empty()) {
            return nullptr;
        }

        const auto* registrator = _engine->GetPropertyRegistrator(entity_class_name);
        auto props = Properties(registrator);
        if (!PropertiesSerializator::LoadFromDocument(&props, doc, *_engine)) {
            return nullptr;
        }

        auto* entity = new ServerEntity(_engine, id, registrator);
        entity->SetProperties(props);

        RegisterEntityEx(entity);
        all_entities.emplace(id, entity);
        return entity;
    }

    return it->second;
}

auto EntityManager::CreateCustomEntity(string_view entity_class_name) -> ServerEntity*
{
    const auto* registrator = _engine->GetPropertyRegistrator(entity_class_name);
    auto* entity = new ServerEntity(_engine, 0u, registrator);

    RegisterEntityEx(entity);
    auto& all_entities = _allCustomEntities[string(entity_class_name)];
    const auto [it, inserted] = all_entities.emplace(entity->GetId(), entity);
    RUNTIME_ASSERT(inserted);

    return entity;
}

void EntityManager::DeleteCustomEntity(string_view entity_class_name, uint id)
{
    auto* entity = GetCustomEntity(entity_class_name, id);
    if (entity != nullptr) {
        auto& all_entities = _allCustomEntities[string(entity_class_name)];
        const auto it = all_entities.find(entity->GetId());
        RUNTIME_ASSERT(it != all_entities.end());
        all_entities.erase(it);
        UnregisterEntityEx(entity, true);

        entity->MarkAsDestroyed();
        entity->Release();
    }
}
