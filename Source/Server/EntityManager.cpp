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
#include "Server.h"
#include "StringUtils.h"

EntityManager::EntityManager(FOServer* engine) : _engine {engine}
{
}

void EntityManager::RegisterEntity(ServerEntity* entity)
{
    if (entity->GetId() == 0u) {
        auto id = _engine->GetLastEntityId() + 1;
        id = std::max(id, 2u);
        _engine->SetLastEntityId(id);

        entity->SetId(id);

        auto doc = PropertiesSerializator::SaveToDbDocument(&entity->GetProperties(), entity->Proto != nullptr ? &entity->Proto->GetProperties() : nullptr, *_engine->ScriptSys);
        doc["_Proto"] = (entity->Proto != nullptr ? entity->Proto->GetName() : "");
        _engine->DbStorage.Insert(_str("{}s", entity->GetClassName()), id, doc);
    }

    const auto [it, inserted] = _allEntities.insert(std::make_pair(entity->GetId(), entity));
    RUNTIME_ASSERT(inserted);
}

void EntityManager::UnregisterEntity(ServerEntity* entity)
{
    const auto it = _allEntities.find(entity->GetId());
    RUNTIME_ASSERT(it != _allEntities.end());
    _allEntities.erase(it);

    _engine->ScriptSys->RemoveEntity(entity);
    _engine->DbStorage.Delete(_str("{}s", entity->GetClassName()), entity->GetId());

    entity->SetId(0);
}

auto EntityManager::GetEntity(uint id) -> ServerEntity*
{
    if (const auto it = _allEntities.find(id); it != _allEntities.end()) {
        return it->second;
    }

    return nullptr;
}

auto EntityManager::GetEntities() -> vector<ServerEntity*>
{
    vector<ServerEntity*> entities;
    entities.reserve(_allEntities.size());

    for (auto [id, entity] : _allEntities) {
        entities.push_back(entity);
    }

    return entities;
}

auto EntityManager::GetPlayer(uint id) -> Player*
{
    if (const auto it = _allEntities.find(id); it != _allEntities.end()) {
        return dynamic_cast<Player*>(it->second);
    }

    return nullptr;
}

auto EntityManager::GetPlayers() -> vector<Player*>
{
    vector<Player*> players;
    players.reserve(_allEntities.size());

    for (auto [id, entity] : _allEntities) {
        if (auto* player = dynamic_cast<Player*>(entity); player != nullptr) {
            players.push_back(player);
        }
    }

    return players;
}

auto EntityManager::GetPlayers() const -> vector<const Player*>
{
    vector<const Player*> players;
    players.reserve(_allEntities.size());

    for (auto [id, entity] : _allEntities) {
        if (const auto* player = dynamic_cast<const Player*>(entity); player != nullptr) {
            players.push_back(player);
        }
    }

    return players;
}

auto EntityManager::GetItem(uint id) -> Item*
{
    if (const auto it = _allEntities.find(id); it != _allEntities.end()) {
        return dynamic_cast<Item*>(it->second);
    }

    return nullptr;
}

auto EntityManager::GetItems() -> vector<Item*>
{
    vector<Item*> items;
    items.reserve(_allEntities.size());

    for (auto [id, entity] : _allEntities) {
        if (auto* item = dynamic_cast<Item*>(entity); item != nullptr) {
            items.push_back(item);
        }
    }

    return items;
}

auto EntityManager::FindCritterItems(uint crid) -> vector<Item*>
{
    vector<Item*> items;

    for (auto [id, entity] : _allEntities) {
        if (auto* item = dynamic_cast<Item*>(entity); item != nullptr) {
            if (item->GetOwnership() == ItemOwnership::CritterInventory && item->GetCritId() == crid) {
                items.push_back(item);
            }
        }
    }

    return items;
}

auto EntityManager::GetCritter(uint id) -> Critter*
{
    if (const auto it = _allEntities.find(id); it != _allEntities.end()) {
        return dynamic_cast<Critter*>(it->second);
    }

    return nullptr;
}

auto EntityManager::GetCritters() -> vector<Critter*>
{
    NON_CONST_METHOD_HINT();

    vector<Critter*> critters;
    critters.reserve(_allEntities.size());

    for (auto [id, entity] : _allEntities) {
        if (auto* cr = dynamic_cast<Critter*>(entity); cr != nullptr) {
            critters.push_back(cr);
        }
    }

    return critters;
}

auto EntityManager::GetMap(uint id) -> Map*
{
    if (const auto it = _allEntities.find(id); it != _allEntities.end()) {
        return dynamic_cast<Map*>(it->second);
    }

    return nullptr;
}

auto EntityManager::GetMapByPid(hash pid, uint skip_count) -> Map*
{
    for (auto [id, entity] : _allEntities) {
        if (auto* map = dynamic_cast<Map*>(entity); map != nullptr) {
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
    maps.reserve(_allEntities.size());

    for (auto [id, entity] : _allEntities) {
        if (auto* map = dynamic_cast<Map*>(entity); map != nullptr) {
            maps.push_back(map);
        }
    }

    return maps;
}

auto EntityManager::GetLocation(uint id) -> Location*
{
    if (const auto it = _allEntities.find(id); it != _allEntities.end()) {
        return dynamic_cast<Location*>(it->second);
    }

    return nullptr;
}

auto EntityManager::GetLocationByPid(hash pid, uint skip_count) -> Location*
{
    for (auto [id, entity] : _allEntities) {
        if (auto* loc = dynamic_cast<Location*>(entity); loc != nullptr) {
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
    locations.reserve(_allEntities.size());

    for (auto [id, entity] : _allEntities) {
        if (auto* loc = dynamic_cast<Location*>(entity); loc != nullptr) {
            locations.push_back(loc);
        }
    }

    return locations;
}

void EntityManager::LoadEntities(const LocationFabric& loc_fabric, const MapFabric& map_fabric, const NpcFabric& npc_fabric, const ItemFabric& item_fabric)
{
    NON_CONST_METHOD_HINT();

    WriteLog("Load entities...\n");

    string query[] = {
        "Locations",
        "Maps",
        "Critters",
        "Items",
    };

    for (auto q = 0; q < 4; q++) {
        // Todo: store player critters in separate collection
        string collection_name = query[q];

        auto ids = _engine->DbStorage.GetAllIds(collection_name);
        for (auto id : ids) {
            auto doc = _engine->DbStorage.Get(collection_name, id);

            auto proto_it = doc.find("_Proto");
            if (proto_it == doc.end()) {
                throw EntitiesLoadException("'_Proto' section not found in entity", collection_name, id);
            }
            if (proto_it->second.index() != DataBase::STRING_VALUE) {
                throw EntitiesLoadException("'_Proto' section is not string type", collection_name, id, proto_it->second.index());
            }

            const auto proto_id = !std::get<string>(proto_it->second).empty() ? _str(std::get<string>(proto_it->second)).toHash() : hash();

            if (q == 0) {
                const auto* proto = _engine->ProtoMngr.GetProtoLocation(proto_id);
                if (proto == nullptr) {
                    throw EntitiesLoadException("Location proto not found", _str().parseHash(proto_id));
                }

                auto* loc = loc_fabric(id, proto);
                if (!PropertiesSerializator::LoadFromDbDocument(&loc->GetPropertiesForEdit(), doc, *_engine->ScriptSys)) {
                    throw EntitiesLoadException("Failed to restore location properties", _str().parseHash(proto_id), id);
                }

                loc->BindScript();

                RegisterEntity(loc);
            }
            else if (q == 1) {
                const auto* proto = _engine->ProtoMngr.GetProtoMap(proto_id);
                if (proto == nullptr) {
                    throw EntitiesLoadException("Map proto not found", _str().parseHash(proto_id));
                }

                auto* map = map_fabric(id, proto);
                if (!PropertiesSerializator::LoadFromDbDocument(&map->GetPropertiesForEdit(), doc, *_engine->ScriptSys)) {
                    throw EntitiesLoadException("Failed to restore map properties", _str().parseHash(proto_id), id);
                }

                RegisterEntity(map);
            }
            else if (q == 2) {
                const auto* proto = _engine->ProtoMngr.GetProtoCritter(proto_id);
                if (proto == nullptr) {
                    throw EntitiesLoadException("Proto critter not found", _str().parseHash(proto_id));
                }

                auto* npc = npc_fabric(id, proto);
                if (!PropertiesSerializator::LoadFromDbDocument(&npc->GetPropertiesForEdit(), doc, *_engine->ScriptSys)) {
                    throw EntitiesLoadException("Failed to restore critter properties", _str().parseHash(proto_id), id);
                }

                RegisterEntity(npc);
            }
            else if (q == 3) {
                const auto* proto = _engine->ProtoMngr.GetProtoItem(proto_id);
                if (proto == nullptr) {
                    throw EntitiesLoadException("Proto item is not loaded", _str().parseHash(proto_id));
                }

                auto* item = item_fabric(id, proto);
                if (!PropertiesSerializator::LoadFromDbDocument(&item->GetPropertiesForEdit(), doc, *_engine->ScriptSys)) {
                    throw EntitiesLoadException("Failed to restore item properties", _str().parseHash(proto_id), id);
                }

                RegisterEntity(item);
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
    NON_CONST_METHOD_HINT();

    WriteLog("Init entities after load...\n");

    auto entities = _allEntities;

    for (auto [id, entity] : entities) {
        entity->AddRef();
    }

    for (auto [id, entity] : entities) {
        if (entity->IsDestroyed()) {
            entity->Release();
            continue;
        }

        if (auto* loc = dynamic_cast<Location*>(entity); loc != nullptr) {
            _engine->LocationInitEvent.Raise(loc, false);
        }
        else if (auto* map = dynamic_cast<Map*>(entity); map != nullptr) {
            _engine->MapInitEvent.Raise(map, false);

            if (!map->IsDestroyed() && map->GetScriptId() != 0u) {
                map->SetScript("", false);
            }
        }
        else if (auto* cr = dynamic_cast<Critter*>(entity); cr != nullptr) {
            _engine->CritterInitEvent.Raise(cr, false);

            if (!cr->IsDestroyed() && cr->GetScriptId() != 0u) {
                cr->SetScript("", false);
            }
        }
        else if (auto* item = dynamic_cast<Item*>(entity); item != nullptr) {
            _engine->ItemInitEvent.Raise(item, false);

            if (!item->IsDestroyed() && item->GetScriptId() != 0u) {
                item->SetScript("", false);
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
            entity->MarkAsDestroyed();
            _engine->ScriptSys->RemoveEntity(entity);
            entity->Release();
            _allEntities.erase(id);
        }

        if (++recursion_fuse == ENTITIES_FINALIZATION_FUSE_VALUE) {
            WriteLog("Entities finalizations fuse fired!");
            break;
        }
    }
}
