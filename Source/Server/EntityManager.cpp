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
// Copyright (c) 2006 - 2023, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "ProtoManager.h"
#include "Server.h"
#include "StringUtils.h"

EntityManager::EntityManager(FOServer* engine) :
    _engine {engine}
{
    STACK_TRACE_ENTRY();
}

auto EntityManager::GetPlayer(ident_t id) -> Player*
{
    STACK_TRACE_ENTRY();

    if (const auto it = _allPlayers.find(id); it != _allPlayers.end()) {
        return it->second;
    }

    return nullptr;
}

auto EntityManager::GetPlayers() -> const unordered_map<ident_t, Player*>&
{
    STACK_TRACE_ENTRY();

    return _allPlayers;
}

auto EntityManager::GetLocation(ident_t id) -> Location*
{
    STACK_TRACE_ENTRY();

    if (const auto it = _allLocations.find(id); it != _allLocations.end()) {
        return it->second;
    }

    return nullptr;
}

auto EntityManager::GetLocationByPid(hstring pid, uint skip_count) -> Location*
{
    STACK_TRACE_ENTRY();

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

auto EntityManager::GetLocations() -> const unordered_map<ident_t, Location*>&
{
    STACK_TRACE_ENTRY();

    return _allLocations;
}

auto EntityManager::GetMap(ident_t id) -> Map*
{
    STACK_TRACE_ENTRY();

    if (const auto it = _allMaps.find(id); it != _allMaps.end()) {
        return it->second;
    }

    return nullptr;
}

auto EntityManager::GetMapByPid(hstring pid, uint skip_count) -> Map*
{
    STACK_TRACE_ENTRY();

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

auto EntityManager::GetMaps() -> const unordered_map<ident_t, Map*>&
{
    STACK_TRACE_ENTRY();

    return _allMaps;
}

auto EntityManager::GetCritter(ident_t id) -> Critter*
{
    STACK_TRACE_ENTRY();

    if (const auto it = _allCritters.find(id); it != _allCritters.end()) {
        return it->second;
    }

    return nullptr;
}

auto EntityManager::GetCritters() -> const unordered_map<ident_t, Critter*>&
{
    STACK_TRACE_ENTRY();

    return _allCritters;
}

auto EntityManager::GetItem(ident_t id) -> Item*
{
    STACK_TRACE_ENTRY();

    if (const auto it = _allItems.find(id); it != _allItems.end()) {
        return it->second;
    }

    return nullptr;
}

auto EntityManager::GetItems() -> const unordered_map<ident_t, Item*>&
{
    STACK_TRACE_ENTRY();

    return _allItems;
}

void EntityManager::LoadEntities()
{
    STACK_TRACE_ENTRY();

    WriteLog("Load entities");

    bool is_error = false;

    const auto loc_ids = _engine->DbStorage.GetAllIds(_engine->LocationsCollectionName);

    for (const auto loc_id : loc_ids) {
        LoadLocation(ident_t {loc_id}, is_error);
    }

    // Todo: load global map critters

    if (is_error) {
        throw ServerInitException("Load entities failed");
    }

    WriteLog("Loaded {} locations", _allLocations.size());
    WriteLog("Loaded {} maps", _allMaps.size());
    WriteLog("Loaded {} critters", _allCritters.size());
    WriteLog("Loaded {} items", _allItems.size());

    WriteLog("Init entities");

    for (auto* loc : copy_hold_ref(_allLocations)) {
        if (!loc->IsDestroyed()) {
            CallInit(loc, false);
        }
    }

    for (auto* cr : copy_hold_ref(_allCritters)) {
        if (!cr->IsDestroyed()) {
            _engine->MapMngr.ProcessVisibleCritters(cr);
        }
        if (!cr->IsDestroyed()) {
            _engine->MapMngr.ProcessVisibleItems(cr);
        }
    }
}

auto EntityManager::LoadLocation(ident_t loc_id, bool& is_error) -> Location*
{
    STACK_TRACE_ENTRY();

    auto&& [loc_doc, loc_pid] = LoadEntityDoc(_engine->LocationsCollectionName, loc_id, is_error);

    if (!loc_pid) {
        return {};
    }

    const auto* loc_proto = _engine->ProtoMngr.GetProtoLocation(loc_pid);

    if (loc_proto == nullptr) {
        WriteLog("Location proto {} not found", loc_pid);
        is_error = true;
        return {};
    }

    auto* loc = new Location(_engine, loc_id, loc_proto);

    if (!PropertiesSerializator::LoadFromDocument(&loc->GetPropertiesForEdit(), loc_doc, *_engine, *_engine)) {
        WriteLog("Failed to restore location {} {} properties", loc_pid, loc_id);
        is_error = true;
        return {};
    }

    loc->BindScript();

    RegisterEntity(loc);

    const auto map_ids = loc->GetMapIds();

    for (const auto& map_id : map_ids) {
        auto* map = LoadMap(map_id, is_error);

        if (map != nullptr) {
            RUNTIME_ASSERT(map->GetLocId() == loc->GetId());

            const auto loc_map_index = map->GetLocMapIndex();

            auto& loc_maps = loc->GetMapsRaw();
            if (loc_map_index >= static_cast<uint>(loc_maps.size())) {
                loc_maps.resize(loc_map_index + 1);
            }

            loc_maps[loc_map_index] = map;

            map->SetLocation(loc);
        }
    }

    return loc;
}

auto EntityManager::LoadMap(ident_t map_id, bool& is_error) -> Map*
{
    STACK_TRACE_ENTRY();

    auto&& [map_doc, map_pid] = LoadEntityDoc(_engine->MapsCollectionName, map_id, is_error);

    if (!map_pid) {
        return {};
    }

    const auto* map_proto = _engine->ProtoMngr.GetProtoMap(map_pid);

    if (map_proto == nullptr) {
        WriteLog("Map proto {} not found", map_pid);
        is_error = true;
        return {};
    }

    const auto* static_map = _engine->MapMngr.GetStaticMap(map_proto);
    auto* map = new Map(_engine, map_id, map_proto, nullptr, static_map);

    if (!PropertiesSerializator::LoadFromDocument(&map->GetPropertiesForEdit(), map_doc, *_engine, *_engine)) {
        WriteLog("Failed to restore map {} {} properties", map_pid, map_id);
        is_error = true;
        return {};
    }

    RegisterEntity(map);

    const auto cr_ids = map->GetCritterIds();

    for (const auto& cr_id : cr_ids) {
        auto* cr = LoadCritter(cr_id, is_error);

        if (cr != nullptr) {
            RUNTIME_ASSERT(cr->GetMapId() == map->GetId());

            if (const auto hex = cr->GetHex(); !map->GetSize().IsValidPos(hex)) {
                cr->SetHex(map->GetSize().ClampPos(hex));
            }

            map->AddCritter(cr);
        }
    }

    const auto item_ids = map->GetItemIds();

    for (const auto& item_id : item_ids) {
        auto* item = LoadItem(item_id, is_error);

        if (item != nullptr) {
            RUNTIME_ASSERT(item->GetMapId() == map->GetId());

            if (const auto hex = item->GetHex(); !map->GetSize().IsValidPos(hex)) {
                item->SetHex(map->GetSize().ClampPos(hex));
            }

            map->SetItem(item, item->GetHex());
        }
    }

    return map;
}

auto EntityManager::LoadCritter(ident_t cr_id, bool& is_error) -> Critter*
{
    STACK_TRACE_ENTRY();

    auto&& [cr_doc, cr_pid] = LoadEntityDoc(_engine->CrittersCollectionName, cr_id, is_error);

    if (!cr_pid) {
        return {};
    }

    const auto* proto = _engine->ProtoMngr.GetProtoCritter(cr_pid);

    if (proto == nullptr) {
        WriteLog("Proto critter {} not found", cr_pid);
        is_error = true;
        return {};
    }

    auto* cr = new Critter(_engine, cr_id, proto);

    if (!PropertiesSerializator::LoadFromDocument(&cr->GetPropertiesForEdit(), cr_doc, *_engine, *_engine)) {
        WriteLog("Failed to restore critter {} {} properties", cr_pid, cr_id);
        is_error = true;
        return {};
    }

    RegisterEntity(cr);

    const auto item_ids = cr->GetItemIds();

    for (const auto& item_id : item_ids) {
        auto* inv_item = LoadItem(item_id, is_error);

        if (inv_item != nullptr) {
            RUNTIME_ASSERT(inv_item->GetCritterId() == cr->GetId());

            cr->SetItem(inv_item);
        }
    }

    return cr;
}

auto EntityManager::LoadItem(ident_t item_id, bool& is_error) -> Item*
{
    STACK_TRACE_ENTRY();

    auto&& [item_doc, item_pid] = LoadEntityDoc(_engine->ItemsCollectionName, item_id, is_error);

    if (!item_pid) {
        return {};
    }

    const auto* proto = _engine->ProtoMngr.GetProtoItem(item_pid);

    if (proto == nullptr) {
        WriteLog("Proto item {} is not loaded", item_pid);
        is_error = true;
        return {};
    }

    auto* item = new Item(_engine, item_id, proto);

    if (!PropertiesSerializator::LoadFromDocument(&item->GetPropertiesForEdit(), item_doc, *_engine, *_engine)) {
        WriteLog("Failed to restore item {} {} properties", item_pid, item_id);
        is_error = true;
        return {};
    }

    item->SetIsStatic(false);

    if (item->GetIsRadio()) {
        _engine->ItemMngr.RegisterRadio(item);
    }

    RegisterEntity(item);

    const auto inner_item_ids = item->GetInnerItemIds();

    for (const auto& inner_item_id : inner_item_ids) {
        auto* inner_item = LoadItem(inner_item_id, is_error);

        if (inner_item != nullptr) {
            RUNTIME_ASSERT(inner_item->GetContainerId() == item->GetId());

            _engine->ItemMngr.SetItemToContainer(item, inner_item); // NOLINT(readability-suspicious-call-argument)
        }
    }

    return item;
}

auto EntityManager::LoadEntityDoc(hstring collection_name, ident_t id, bool& is_error) const -> tuple<AnyData::Document, hstring>
{
    STACK_TRACE_ENTRY();

    auto doc = _engine->DbStorage.Get(collection_name, id);

    if (doc.empty()) {
        WriteLog("{} document {} not found", collection_name, id);
        is_error = true;
        return {};
    }

    const auto proto_it = doc.find("_Proto");
    if (proto_it == doc.end()) {
        WriteLog("{} '_Proto' section not found in entity {}", collection_name, id);
        is_error = true;
        return {};
    }

    if (proto_it->second.index() != AnyData::STRING_VALUE) {
        WriteLog("{} '_Proto' section of entity {} is not string type (but {})", collection_name, id, proto_it->second.index());
        is_error = true;
        return {};
    }

    const auto& proto_name = std::get<string>(proto_it->second);
    if (proto_name.empty()) {
        WriteLog("{} '_Proto' section of entity {} is empty", collection_name, id);
        is_error = true;
        return {};
    }

    auto proto_id = _engine->ToHashedString(proto_name);

    return {doc, proto_id};
}

void EntityManager::CallInit(Location* loc, bool first_time)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!loc->IsDestroyed());

    if (loc->IsInitCalled()) {
        return;
    }

    auto loc_holder = RefCountHolder(loc);

    loc->SetInitCalled();

    _engine->OnLocationInit.Fire(loc, first_time);

    if (!loc->IsDestroyed()) {
        ScriptHelpers::CallInitScript(_engine->ScriptSys, loc, loc->GetInitScript(), first_time);
    }

    if (!loc->IsDestroyed()) {
        for (auto* map : copy_hold_ref(loc->GetMaps())) {
            if (!map->IsDestroyed()) {
                CallInit(map, first_time);
            }
        }
    }
}

void EntityManager::CallInit(Map* map, bool first_time)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!map->IsDestroyed());

    if (map->IsInitCalled()) {
        return;
    }

    auto map_holder = RefCountHolder(map);

    map->SetInitCalled();

    _engine->OnMapInit.Fire(map, first_time);

    if (!map->IsDestroyed()) {
        ScriptHelpers::CallInitScript(_engine->ScriptSys, map, map->GetInitScript(), first_time);
    }

    if (!map->IsDestroyed()) {
        for (auto* cr : copy_hold_ref(map->GetCritters())) {
            if (!cr->IsDestroyed()) {
                CallInit(cr, first_time);
            }
        }
    }

    if (!map->IsDestroyed()) {
        for (auto* item : copy_hold_ref(map->GetItems())) {
            if (!item->IsDestroyed()) {
                CallInit(item, first_time);
            }
        }
    }
}

void EntityManager::CallInit(Critter* cr, bool first_time)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!cr->IsDestroyed());

    if (cr->IsInitCalled()) {
        return;
    }

    auto cr_holder = RefCountHolder(cr);

    cr->SetInitCalled();

    _engine->OnCritterInit.Fire(cr, first_time);

    if (!cr->IsDestroyed()) {
        ScriptHelpers::CallInitScript(_engine->ScriptSys, cr, cr->GetInitScript(), first_time);
    }

    if (!cr->IsDestroyed()) {
        for (auto* item : copy_hold_ref(cr->GetRawInvItems())) {
            if (!item->IsDestroyed()) {
                CallInit(item, first_time);
            }
        }
    }
}

void EntityManager::CallInit(Item* item, bool first_time)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!item->IsDestroyed());

    if (item->IsInitCalled()) {
        return;
    }

    auto item_holder = RefCountHolder(item);

    item->SetInitCalled();

    _engine->OnItemInit.Fire(item, first_time);

    if (!item->IsDestroyed()) {
        ScriptHelpers::CallInitScript(_engine->ScriptSys, item, item->GetInitScript(), first_time);
    }

    if (!item->IsDestroyed() && item->IsInnerItems()) {
        for (auto* inner_item : copy_hold_ref(item->GetRawInnerItems())) {
            if (!inner_item->IsDestroyed()) {
                CallInit(inner_item, first_time);
            }
        }
    }
}

void EntityManager::RegisterEntity(Player* entity, ident_t id)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(id);
    entity->SetId(id);
    RegisterEntityEx(entity);
    const auto [it, inserted] = _allPlayers.emplace(entity->GetId(), entity);
    RUNTIME_ASSERT(inserted);
}

void EntityManager::UnregisterEntity(Player* entity)
{
    STACK_TRACE_ENTRY();

    const auto it = _allPlayers.find(entity->GetId());
    RUNTIME_ASSERT(it != _allPlayers.end());
    _allPlayers.erase(it);
    UnregisterEntityEx(entity, false);
}

void EntityManager::RegisterEntity(Location* entity)
{
    STACK_TRACE_ENTRY();

    RegisterEntityEx(entity);
    const auto [it, inserted] = _allLocations.emplace(entity->GetId(), entity);
    RUNTIME_ASSERT(inserted);
}

void EntityManager::UnregisterEntity(Location* entity)
{
    STACK_TRACE_ENTRY();

    const auto it = _allLocations.find(entity->GetId());
    RUNTIME_ASSERT(it != _allLocations.end());
    _allLocations.erase(it);
    UnregisterEntityEx(entity, true);
}

void EntityManager::RegisterEntity(Map* entity)
{
    STACK_TRACE_ENTRY();

    RegisterEntityEx(entity);
    const auto [it, inserted] = _allMaps.emplace(entity->GetId(), entity);
    RUNTIME_ASSERT(inserted);
}

void EntityManager::UnregisterEntity(Map* entity)
{
    STACK_TRACE_ENTRY();

    const auto it = _allMaps.find(entity->GetId());
    RUNTIME_ASSERT(it != _allMaps.end());
    _allMaps.erase(it);
    UnregisterEntityEx(entity, true);
}

void EntityManager::RegisterEntity(Critter* entity)
{
    STACK_TRACE_ENTRY();

    RegisterEntityEx(entity);
    const auto [it, inserted] = _allCritters.emplace(entity->GetId(), entity);
    RUNTIME_ASSERT(inserted);
}

void EntityManager::UnregisterEntity(Critter* entity)
{
    STACK_TRACE_ENTRY();

    const auto it = _allCritters.find(entity->GetId());
    RUNTIME_ASSERT(it != _allCritters.end());
    _allCritters.erase(it);
    UnregisterEntityEx(entity, !entity->GetIsControlledByPlayer());
}

void EntityManager::RegisterEntity(Item* entity)
{
    STACK_TRACE_ENTRY();

    RegisterEntityEx(entity);
    const auto [it, inserted] = _allItems.emplace(entity->GetId(), entity);
    RUNTIME_ASSERT(inserted);
}

void EntityManager::UnregisterEntity(Item* entity, bool delete_from_db)
{
    STACK_TRACE_ENTRY();

    const auto it = _allItems.find(entity->GetId());
    RUNTIME_ASSERT(it != _allItems.end());
    _allItems.erase(it);
    UnregisterEntityEx(entity, delete_from_db);
}

void EntityManager::RegisterEntityEx(ServerEntity* entity)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (!entity->GetId()) {
        const auto id_num = std::max(_engine->GetLastEntityId().underlying_value() + 1, static_cast<ident_t::underlying_type>(2));
        const auto id = ident_t {id_num};
        const auto collection_name = _engine->ToHashedString(_str("{}s", entity->GetClassName()));

        _engine->SetLastEntityId(id);

        entity->SetId(id);

        if (const auto* entity_with_proto = dynamic_cast<EntityWithProto*>(entity); entity_with_proto != nullptr) {
            const auto* proto = entity_with_proto->GetProto();
            auto doc = PropertiesSerializator::SaveToDocument(&entity->GetProperties(), &proto->GetProperties(), *_engine, *_engine);

            doc["_Proto"] = string(proto->GetName());

            _engine->DbStorage.Insert(collection_name, id, doc);
        }
        else {
            const auto doc = PropertiesSerializator::SaveToDocument(&entity->GetProperties(), nullptr, *_engine, *_engine);

            _engine->DbStorage.Insert(collection_name, id, doc);
        }
    }
}

void EntityManager::UnregisterEntityEx(ServerEntity* entity, bool delete_from_db)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(entity->GetId());

    if (delete_from_db) {
        const auto collection_name = _engine->ToHashedString(_str("{}s", entity->GetClassName()));

        _engine->DbStorage.Delete(collection_name, entity->GetId());
    }

    entity->SetId(ident_t {});
}

void EntityManager::FinalizeEntities()
{
    STACK_TRACE_ENTRY();

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

auto EntityManager::GetCustomEntity(string_view entity_class_name, ident_t id) -> ServerEntity*
{
    STACK_TRACE_ENTRY();

    auto& all_entities = _allCustomEntities[string(entity_class_name)];
    const auto it = all_entities.find(id);

    // Load if not exists
    if (it == all_entities.end()) {
        const auto collection_name = _engine->ToHashedString(_str("{}s", entity_class_name));
        const auto doc = _engine->DbStorage.Get(collection_name, id);
        if (doc.empty()) {
            return nullptr;
        }

        const auto* registrator = _engine->GetPropertyRegistrator(entity_class_name);
        auto props = Properties(registrator);
        if (!PropertiesSerializator::LoadFromDocument(&props, doc, *_engine, *_engine)) {
            return nullptr;
        }

        auto* entity = new ServerEntity(_engine, id, registrator, &props);

        RegisterEntityEx(entity);
        all_entities.emplace(id, entity);
        return entity;
    }

    return it->second;
}

auto EntityManager::CreateCustomEntity(string_view entity_class_name) -> ServerEntity*
{
    STACK_TRACE_ENTRY();

    const auto* registrator = _engine->GetPropertyRegistrator(entity_class_name);
    auto* entity = new ServerEntity(_engine, ident_t {}, registrator, nullptr);

    RegisterEntityEx(entity);
    auto& all_entities = _allCustomEntities[string(entity_class_name)];
    const auto [it, inserted] = all_entities.emplace(entity->GetId(), entity);
    RUNTIME_ASSERT(inserted);

    return entity;
}

void EntityManager::DeleteCustomEntity(string_view entity_class_name, ident_t id)
{
    STACK_TRACE_ENTRY();

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
