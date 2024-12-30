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
// Copyright (c) 2006 - 2024, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
    _engine {engine},
    _entityTypeMapCollection {engine->ToHashedString("EntityTypeMap")},
    _playerTypeName {engine->ToHashedString(Player::ENTITY_TYPE_NAME)},
    _locationTypeName {engine->ToHashedString(Location::ENTITY_TYPE_NAME)},
    _mapTypeName {engine->ToHashedString(Map::ENTITY_TYPE_NAME)},
    _critterTypeName {engine->ToHashedString(Critter::ENTITY_TYPE_NAME)},
    _itemTypeName {engine->ToHashedString(Item::ENTITY_TYPE_NAME)},
    _playerCollectionName {engine->ToHashedString(strex("{}s", Player::ENTITY_TYPE_NAME))},
    _locationCollectionName {engine->ToHashedString(strex("{}s", Location::ENTITY_TYPE_NAME))},
    _mapCollectionName {engine->ToHashedString(strex("{}s", Map::ENTITY_TYPE_NAME))},
    _critterCollectionName {engine->ToHashedString(strex("{}s", Critter::ENTITY_TYPE_NAME))},
    _itemCollectionName {engine->ToHashedString(strex("{}s", Item::ENTITY_TYPE_NAME))},
    _removeMigrationRuleName {engine->ToHashedString("Remove")}
{
    STACK_TRACE_ENTRY();
}

auto EntityManager::GetEntity(ident_t id) noexcept -> ServerEntity*
{
    NO_STACK_TRACE_ENTRY();

    if (const auto it = _allEntities.find(id); it != _allEntities.end()) {
        return it->second;
    }

    return nullptr;
}

auto EntityManager::GetPlayer(ident_t id) noexcept -> Player*
{
    NO_STACK_TRACE_ENTRY();

    if (const auto it = _allPlayers.find(id); it != _allPlayers.end()) {
        return it->second;
    }

    return nullptr;
}

auto EntityManager::GetLocation(ident_t id) noexcept -> Location*
{
    NO_STACK_TRACE_ENTRY();

    if (const auto it = _allLocations.find(id); it != _allLocations.end()) {
        return it->second;
    }

    return nullptr;
}

auto EntityManager::GetMap(ident_t id) noexcept -> Map*
{
    NO_STACK_TRACE_ENTRY();

    if (const auto it = _allMaps.find(id); it != _allMaps.end()) {
        return it->second;
    }

    return nullptr;
}

auto EntityManager::GetCritter(ident_t id) noexcept -> Critter*
{
    NO_STACK_TRACE_ENTRY();

    if (const auto it = _allCritters.find(id); it != _allCritters.end()) {
        return it->second;
    }

    return nullptr;
}

auto EntityManager::GetItem(ident_t id) noexcept -> Item*
{
    NO_STACK_TRACE_ENTRY();

    if (const auto it = _allItems.find(id); it != _allItems.end()) {
        return it->second;
    }

    return nullptr;
}

void EntityManager::LoadEntities()
{
    STACK_TRACE_ENTRY();

    WriteLog("Load entities");

    bool is_error = false;

    LoadInnerEntities(_engine, is_error);

    const auto loc_ids = _engine->DbStorage.GetAllIds(_locationCollectionName);

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
    WriteLog("Loaded {} other entities", _allEntities.size() - _allLocations.size() - _allMaps.size() - _allCritters.size() - _allItems.size());

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

    auto&& [loc_doc, loc_pid] = LoadEntityDoc(_locationTypeName, _locationCollectionName, loc_id, true, is_error);

    if (!loc_pid) {
        WriteLog("Location {} invalid document", loc_id);
        is_error = true;
        return nullptr;
    }

    const auto* loc_proto = _engine->ProtoMngr.GetProtoLocationSafe(loc_pid);

    if (loc_proto == nullptr) {
        WriteLog("Location {} proto {} not found", loc_id, loc_pid);
        is_error = true;
        return nullptr;
    }

    auto* loc = new Location(_engine, loc_id, loc_proto);

    if (!PropertiesSerializator::LoadFromDocument(&loc->GetPropertiesForEdit(), loc_doc, *_engine, *_engine)) {
        WriteLog("Failed to restore location {} {} properties", loc_pid, loc_id);
        is_error = true;
        return nullptr;
    }

    RegisterEntity(loc);

    const auto map_ids = loc->GetMapIds();
    bool map_ids_changed = false;

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
        else {
            map_ids_changed = true;
        }
    }

    if (map_ids_changed) {
        const auto actual_map_ids = vec_transform(loc->GetMapsRaw(), [](const Map* map) -> ident_t { return map->GetId(); });
        loc->SetMapIds(actual_map_ids);
    }

    // Inner entities
    LoadInnerEntities(loc, is_error);

    return loc;
}

auto EntityManager::LoadMap(ident_t map_id, bool& is_error) -> Map*
{
    STACK_TRACE_ENTRY();

    auto&& [map_doc, map_pid] = LoadEntityDoc(_mapTypeName, _mapCollectionName, map_id, true, is_error);

    if (!map_pid) {
        WriteLog("Map {} invalid document", map_pid);
        is_error = true;
        return nullptr;
    }

    const auto* map_proto = _engine->ProtoMngr.GetProtoMapSafe(map_pid);

    if (map_proto == nullptr) {
        WriteLog("Map {} proto {} not found", map_id, map_pid);
        is_error = true;
        return nullptr;
    }

    const auto* static_map = _engine->MapMngr.GetStaticMap(map_proto);
    auto* map = new Map(_engine, map_id, map_proto, nullptr, static_map);

    if (!PropertiesSerializator::LoadFromDocument(&map->GetPropertiesForEdit(), map_doc, *_engine, *_engine)) {
        WriteLog("Failed to restore map {} {} properties", map_pid, map_id);
        is_error = true;
        return nullptr;
    }

    RegisterEntity(map);

    // Map critters
    const auto cr_ids = map->GetCritterIds();
    bool cr_ids_changed = false;

    for (const auto& cr_id : cr_ids) {
        auto* cr = LoadCritter(cr_id, is_error);

        if (cr != nullptr) {
            RUNTIME_ASSERT(cr->GetMapId() == map->GetId());

            if (const auto hex = cr->GetHex(); !map->GetSize().IsValidPos(hex)) {
                cr->SetHex(map->GetSize().ClampPos(hex));
            }

            map->AddCritter(cr);
        }
        else {
            cr_ids_changed = true;
        }
    }

    if (cr_ids_changed) {
        const auto actual_cr_ids = vec_transform(map->GetCritters(), [](const Critter* cr) -> ident_t { return cr->GetId(); });
        map->SetCritterIds(actual_cr_ids);
    }

    // Map items
    const auto item_ids = map->GetItemIds();
    bool item_ids_changed = false;

    for (const auto& item_id : item_ids) {
        auto* item = LoadItem(item_id, is_error);

        if (item != nullptr) {
            RUNTIME_ASSERT(item->GetOwnership() == ItemOwnership::MapHex);
            RUNTIME_ASSERT(item->GetMapId() == map->GetId());

            if (const auto hex = item->GetHex(); !map->GetSize().IsValidPos(hex)) {
                item->SetHex(map->GetSize().ClampPos(hex));
            }

            map->SetItem(item);
        }
        else {
            item_ids_changed = true;
        }
    }

    if (item_ids_changed) {
        const auto actual_item_ids = vec_transform(map->GetItems(), [](const Item* item) -> ident_t { return item->GetId(); });
        map->SetItemIds(actual_item_ids);
    }

    // Inner entities
    LoadInnerEntities(map, is_error);

    return map;
}

auto EntityManager::LoadCritter(ident_t cr_id, bool& is_error) -> Critter*
{
    STACK_TRACE_ENTRY();

    auto&& [cr_doc, cr_pid] = LoadEntityDoc(_critterTypeName, _critterCollectionName, cr_id, true, is_error);

    if (!cr_pid) {
        WriteLog("Critter {} invalid document", cr_id);
        is_error = true;
        return nullptr;
    }

    const auto* proto = _engine->ProtoMngr.GetProtoCritterSafe(cr_pid);

    if (proto == nullptr) {
        WriteLog("Critter {} proto {} not found", cr_id, cr_pid);
        is_error = true;
        return nullptr;
    }

    auto* cr = new Critter(_engine, cr_id, proto);

    if (!PropertiesSerializator::LoadFromDocument(&cr->GetPropertiesForEdit(), cr_doc, *_engine, *_engine)) {
        WriteLog("Failed to restore critter {} {} properties", cr_pid, cr_id);
        is_error = true;
        return nullptr;
    }

    RegisterEntity(cr);

    // Inventory
    const auto item_ids = cr->GetItemIds();
    bool item_ids_changed = false;

    for (const auto& item_id : item_ids) {
        auto* inv_item = LoadItem(item_id, is_error);

        if (inv_item != nullptr) {
            RUNTIME_ASSERT(inv_item->GetOwnership() == ItemOwnership::CritterInventory);
            RUNTIME_ASSERT(inv_item->GetCritterId() == cr->GetId());

            cr->SetItem(inv_item);
        }
        else {
            item_ids_changed = true;
        }
    }

    if (item_ids_changed) {
        const auto actual_item_ids = vec_transform(cr->GetInvItems(), [](const Item* item) -> ident_t { return item->GetId(); });
        cr->SetItemIds(actual_item_ids);
    }

    // Inner entities
    LoadInnerEntities(cr, is_error);

    return cr;
}

auto EntityManager::LoadItem(ident_t item_id, bool& is_error) -> Item*
{
    STACK_TRACE_ENTRY();

    auto&& [item_doc, item_pid] = LoadEntityDoc(_itemTypeName, _itemCollectionName, item_id, true, is_error);

    if (!item_pid) {
        WriteLog("Item {} invalid document", item_id);
        is_error = true;
        return nullptr;
    }

    const auto* proto = _engine->ProtoMngr.GetProtoItemSafe(item_pid);

    if (proto == nullptr) {
        WriteLog("Item {} proto {} not found", item_id, item_pid);
        is_error = true;
        return nullptr;
    }

    auto* item = new Item(_engine, item_id, proto);

    if (!PropertiesSerializator::LoadFromDocument(&item->GetPropertiesForEdit(), item_doc, *_engine, *_engine)) {
        WriteLog("Failed to restore item {} {} properties", item_pid, item_id);
        is_error = true;
        return nullptr;
    }

    item->SetStatic(false);

    if (item->GetIsRadio()) {
        _engine->ItemMngr.RegisterRadio(item);
    }

    RegisterEntity(item);

    // Inner items
    const auto inner_item_ids = item->GetInnerItemIds();
    bool inner_item_ids_changed = false;

    for (const auto& inner_item_id : inner_item_ids) {
        auto* inner_item = LoadItem(inner_item_id, is_error);

        if (inner_item != nullptr) {
            RUNTIME_ASSERT(inner_item->GetOwnership() == ItemOwnership::ItemContainer);
            RUNTIME_ASSERT(inner_item->GetContainerId() == item->GetId());

            item->SetItemToContainer(inner_item);
        }
        else {
            inner_item_ids_changed = true;
        }
    }

    if (inner_item_ids_changed) {
        if (item->HasInnerItems()) {
            const auto actual_inner_item_ids = vec_transform(item->GetAllInnerItems(), [](const Item* inner_item) -> ident_t { return inner_item->GetId(); });
            item->SetInnerItemIds(actual_inner_item_ids);
        }
        else {
            item->SetInnerItemIds({});
        }
    }

    // Inner entities
    LoadInnerEntities(item, is_error);

    return item;
}

void EntityManager::LoadInnerEntities(Entity* holder, bool& is_error)
{
    STACK_TRACE_ENTRY();

    auto&& holder_props = EntityProperties(holder->GetPropertiesForEdit());
    auto&& inner_entity_ids = holder_props.GetInnerEntityIds();

    if (inner_entity_ids.empty()) {
        return;
    }

    bool inner_entity_ids_changed = false;
    ident_t holder_id = {};

    if (const auto* holder_with_id = dynamic_cast<ServerEntity*>(holder)) {
        holder_id = holder_with_id->GetId();
    }

    for (const auto& id : inner_entity_ids) {
        auto* custom_entity = LoadCustomEntity(id, is_error);

        if (custom_entity != nullptr) {
            RUNTIME_ASSERT(custom_entity->GetCustomHolderId() == holder_id);

            holder->AddInnerEntity(custom_entity->GetCustomHolderEntry(), custom_entity);

            // Inner entities
            LoadInnerEntities(custom_entity, is_error);
        }
        else {
            inner_entity_ids_changed = true;
        }
    }

    if (inner_entity_ids_changed) {
        vector<ident_t> actual_inner_entity_ids;

        if (holder->HasInnerEntities()) {
            for (auto&& [entry, inner_entities] : holder->GetRawInnerEntities()) {
                for (const auto* inner_entity : inner_entities) {
                    actual_inner_entity_ids.emplace_back(static_cast<const CustomEntity*>(inner_entity)->GetId()); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                }
            }
        }

        holder_props.SetInnerEntityIds(actual_inner_entity_ids);
    }
}

auto EntityManager::LoadEntityDoc(hstring type_name, hstring collection_name, ident_t id, bool expect_proto, bool& is_error) const -> tuple<AnyData::Document, hstring>
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(id.underlying_value() != 0);

    auto doc = _engine->DbStorage.Get(collection_name, id);

    if (doc.Empty()) {
        WriteLog("{} document {} not found", collection_name, id);
        is_error = true;
        return {};
    }

    const auto proto_it = doc.Find("_Proto");

    if (proto_it == doc.end()) {
        if (expect_proto) {
            WriteLog("{} '_Proto' section not found in entity {}", collection_name, id);
            is_error = true;
        }

        return {std::move(doc), hstring()};
    }

    if (proto_it->second.Type() != AnyData::ValueType::String) {
        WriteLog("{} '_Proto' section of entity {} is not string type (but {})", collection_name, id, proto_it->second.Type());
        is_error = true;
        return {};
    }

    const auto& proto_name = proto_it->second.AsString();

    if (proto_name.empty()) {
        WriteLog("{} '_Proto' section of entity {} is empty", collection_name, id);
        is_error = true;
        return {};
    }

    auto proto_id = _engine->ToHashedString(proto_name);

    if (_engine->CheckMigrationRule(_removeMigrationRuleName, type_name, proto_id).has_value()) {
        return {};
    }

    return {std::move(doc), proto_id};
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
        for (auto* item : copy_hold_ref(cr->GetInvItems())) {
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

    if (!item->IsDestroyed() && item->HasInnerItems()) {
        for (auto* inner_item : copy_hold_ref(item->GetAllInnerItems())) {
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
    UnregisterEntityEx(entity, !entity->GetControlledByPlayer());
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

void EntityManager::RegisterEntity(CustomEntity* entity)
{
    STACK_TRACE_ENTRY();

    RegisterEntityEx(entity);
}

void EntityManager::UnregisterEntity(CustomEntity* entity, bool delete_from_db)
{
    STACK_TRACE_ENTRY();

    UnregisterEntityEx(entity, delete_from_db);
}

void EntityManager::RegisterEntityEx(ServerEntity* entity)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (entity->GetId()) {
        const auto [it, inserted] = _allEntities.emplace(entity->GetId(), entity);
        RUNTIME_ASSERT(inserted);
        return;
    }

    const auto id_num = std::max(_engine->GetLastEntityId().underlying_value() + 1, static_cast<ident_t::underlying_type>(2));
    const auto id = ident_t {id_num};
    const auto collection_name = entity->GetTypeNamePlural();

    _engine->SetLastEntityId(id);

    entity->SetId(id);

    const auto [it, inserted] = _allEntities.emplace(id, entity);
    RUNTIME_ASSERT(inserted);

    if (const auto* entity_with_proto = dynamic_cast<EntityWithProto*>(entity); entity_with_proto != nullptr) {
        const auto* proto = entity_with_proto->GetProto();
        auto doc = PropertiesSerializator::SaveToDocument(&entity->GetProperties(), &proto->GetProperties(), *_engine, *_engine);

        doc.Emplace("_Proto", string(proto->GetName()));

        _engine->DbStorage.Insert(collection_name, id, doc);
    }
    else {
        const auto doc = PropertiesSerializator::SaveToDocument(&entity->GetProperties(), nullptr, *_engine, *_engine);

        _engine->DbStorage.Insert(collection_name, id, doc);
    }

    {
        AnyData::Document type_map_id;
        type_map_id.Emplace("Type", string(entity->GetTypeName()));

        _engine->DbStorage.Insert(_entityTypeMapCollection, id, type_map_id);
    }
}

void EntityManager::UnregisterEntityEx(ServerEntity* entity, bool delete_from_db)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(entity->GetId());

    const auto it = _allEntities.find(entity->GetId());
    RUNTIME_ASSERT(it != _allEntities.end());
    _allEntities.erase(it);

    if (delete_from_db) {
        _engine->DbStorage.Delete(entity->GetTypeNamePlural(), entity->GetId());
        _engine->DbStorage.Delete(_entityTypeMapCollection, entity->GetId());
    }

    entity->SetId({});
}

void EntityManager::DestroyEntity(Entity* entity)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(entity);

    if (auto* loc = dynamic_cast<Location*>(entity); loc != nullptr) {
        _engine->MapMngr.DestroyLocation(loc);
    }
    else if (auto* cr = dynamic_cast<Critter*>(entity); cr != nullptr) {
        _engine->CrMngr.DestroyCritter(cr);
    }
    else if (auto* item = dynamic_cast<Item*>(entity); item != nullptr) {
        _engine->ItemMngr.DestroyItem(item);
    }
    else if (auto* custom_entity = dynamic_cast<CustomEntity*>(entity); custom_entity != nullptr) {
        DestroyCustomEntity(custom_entity);
    }
    else {
        WriteLog(LogType::Warning, "Trying to destroy entity: {}{}", dynamic_cast<ProtoEntity*>(entity) != nullptr ? "Proto" : "", entity->GetTypeName());
    }
}

void EntityManager::DestroyInnerEntities(Entity* holder)
{
    STACK_TRACE_ENTRY();

    for (InfinityLoopDetector detector; holder->HasInnerEntities(); detector.AddLoop()) {
        for (auto&& [entry, entities] : copy(holder->GetRawInnerEntities())) {
            for (auto* entity : entities) {
                DestroyEntity(entity);
            }
        }
    }
}

void EntityManager::DestroyAllEntities()
{
    STACK_TRACE_ENTRY();

    const auto destroy_entities = [this](auto& entities) {
        for (auto&& [id, entity] : copy(entities)) {
            entity->MarkAsDestroyed();
            entity->Release();
            entities.erase(id);
            _allEntities.erase(id);
        }

        RUNTIME_ASSERT(entities.empty());
    };

    destroy_entities(_allPlayers);
    destroy_entities(_allLocations);
    destroy_entities(_allMaps);
    destroy_entities(_allCritters);
    destroy_entities(_allItems);
    for (auto& custom_entities : _allCustomEntities) {
        destroy_entities(custom_entities.second);
    }

    RUNTIME_ASSERT(_allEntities.empty());
}

auto EntityManager::CreateCustomInnerEntity(Entity* holder, hstring entry, hstring pid) -> CustomEntity*
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(holder);
    RUNTIME_ASSERT(_engine->GetEntityTypeInfo(holder->GetTypeName()).HolderEntries.count(entry));

    const hstring type_name = std::get<0>(_engine->GetEntityTypeInfo(holder->GetTypeName()).HolderEntries.at(entry));

    auto* entity = CreateCustomEntity(type_name, pid);
    RUNTIME_ASSERT(entity);

    if (const auto* holder_with_id = dynamic_cast<ServerEntity*>(holder); holder_with_id != nullptr) {
        RUNTIME_ASSERT(holder_with_id->GetId());
        entity->SetCustomHolderId(holder_with_id->GetId());
    }
    else {
        RUNTIME_ASSERT(holder == _engine);
    }

    entity->SetCustomHolderEntry(entry);

    holder->AddInnerEntity(entry, entity);

    auto&& holder_props = EntityProperties(holder->GetPropertiesForEdit());
    auto inner_entity_ids = holder_props.GetInnerEntityIds();
    vec_add_unique_value(inner_entity_ids, entity->GetId());
    holder_props.SetInnerEntityIds(inner_entity_ids);

    ForEachCustomEntityView(entity, [entity](Player* player, bool owner) { player->Send_AddCustomEntity(entity, owner); });

    return entity;
}

auto EntityManager::CreateCustomEntity(hstring type_name, hstring pid) -> CustomEntity*
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_engine->IsValidEntityType(type_name));

    const bool has_protos = _engine->GetEntityTypeInfo(type_name).HasProtos;
    const ProtoEntity* proto = nullptr;

    if (pid) {
        RUNTIME_ASSERT(has_protos);
        proto = _engine->ProtoMngr.GetProtoEntity(type_name, pid);
        RUNTIME_ASSERT(proto);
    }
    else {
        RUNTIME_ASSERT(!has_protos);
    }

    const auto* registrator = _engine->GetPropertyRegistrator(type_name);

    CustomEntity* entity;

    if (proto != nullptr) {
        entity = new CustomEntityWithProto(_engine, {}, registrator, nullptr, proto);
    }
    else {
        entity = new CustomEntity(_engine, {}, registrator, nullptr);
    }

    RegisterEntity(entity);

    auto& custom_entities = _allCustomEntities[type_name];
    const auto [it, inserted] = custom_entities.emplace(entity->GetId(), entity);
    RUNTIME_ASSERT(inserted);

    return entity;
}

auto EntityManager::LoadCustomEntity(ident_t id, bool& is_error) -> CustomEntity*
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(id.underlying_value() != 0);

    const auto type_doc = _engine->DbStorage.Get(_entityTypeMapCollection, id);

    if (type_doc.Empty()) {
        WriteLog("Custom entity id {} type not mapped", id);
        is_error = true;
        return nullptr;
    }

    const auto it_type = type_doc.Find("Type");

    if (it_type == type_doc.end()) {
        WriteLog("Custom entity {} mapped type section 'Type' not found", id);
        is_error = true;
        return nullptr;
    }

    if (it_type->second.Type() != AnyData::ValueType::String) {
        WriteLog("Custom entity {} mapped type section 'Type' is not string but {}", id, it_type->second.Type());
        is_error = true;
        return nullptr;
    }

    const auto& type_name_str = it_type->second.AsString();
    const auto type_name = _engine->ToHashedString(type_name_str);

    if (!_engine->IsValidEntityType(type_name)) {
        WriteLog("Custom entity {} mapped type not valid {}", id, type_name);
        is_error = true;
        return nullptr;
    }

    RUNTIME_ASSERT(_allCustomEntities[type_name].count(id) == 0);

    const auto collection_name = _engine->ToHashedString(strex("{}s", type_name));
    auto&& [doc, pid] = LoadEntityDoc(type_name, collection_name, id, false, is_error);

    if (doc.Empty()) {
        WriteLog("Custom entity {} with type {} invalid document", id, type_name);
        is_error = true;
        return nullptr;
    }

    const bool has_protos = _engine->GetEntityTypeInfo(type_name).HasProtos;
    const ProtoEntity* proto = nullptr;

    if (has_protos) {
        if (pid) {
            proto = _engine->ProtoMngr.GetProtoEntity(type_name, pid);
        }
        else {
            proto = _engine->ProtoMngr.GetProtoEntity(type_name, _engine->ToHashedString("Default"));
        }

        if (proto == nullptr) {
            WriteLog("Proto {} for custom entity {} with type {} not found", pid, id, type_name);
            is_error = true;
            return nullptr;
        }
    }

    const auto* registrator = _engine->GetPropertyRegistrator(type_name);
    auto props = Properties(registrator);

    if (!PropertiesSerializator::LoadFromDocument(&props, doc, *_engine, *_engine)) {
        WriteLog("Failed to load properties for custom entity {} with type {}", id, type_name);
        is_error = true;
        return nullptr;
    }

    CustomEntity* entity;

    if (proto != nullptr) {
        entity = new CustomEntityWithProto(_engine, id, registrator, &props, proto);
    }
    else {
        entity = new CustomEntity(_engine, id, registrator, &props);
    }

    RegisterEntityEx(entity);

    auto& custom_entities = _allCustomEntities[type_name];
    const auto [it, inserted] = custom_entities.emplace(entity->GetId(), entity);
    RUNTIME_ASSERT(inserted);

    return entity;
}

auto EntityManager::GetCustomEntity(hstring type_name, ident_t id) -> CustomEntity*
{
    STACK_TRACE_ENTRY();

    auto& custom_entities = _allCustomEntities[type_name];
    const auto it = custom_entities.find(id);

    return it != custom_entities.end() ? it->second : nullptr;
}

void EntityManager::DestroyCustomEntity(CustomEntity* entity)
{
    STACK_TRACE_ENTRY();

    if (entity->IsDestroying() || entity->IsDestroyed()) {
        return;
    }

    entity->MarkAsDestroying();

    for (InfinityLoopDetector detector; entity->HasInnerEntities(); detector.AddLoop()) {
        DestroyInnerEntities(entity);
    }

    Entity* holder;

    if (const auto id = entity->GetCustomHolderId()) {
        holder = GetEntity(id);
        RUNTIME_ASSERT(holder);
    }
    else {
        holder = _engine;
    }

    ForEachCustomEntityView(entity, [entity](Player* player, bool owner) {
        UNUSED_VARIABLE(owner);
        player->Send_RemoveCustomEntity(entity->GetId());
    });

    holder->RemoveInnerEntity(entity->GetCustomHolderEntry(), entity);

    auto&& holder_props = EntityProperties(holder->GetPropertiesForEdit());
    auto inner_entity_ids = holder_props.GetInnerEntityIds();
    vec_remove_unique_value(inner_entity_ids, entity->GetId());
    holder_props.SetInnerEntityIds(inner_entity_ids);

    auto& custom_entities = _allCustomEntities[entity->GetTypeName()];
    const auto it = custom_entities.find(entity->GetId());
    RUNTIME_ASSERT(it != custom_entities.end());
    custom_entities.erase(it);

    UnregisterEntity(entity, true);

    entity->MarkAsDestroyed();
    entity->Release();
}

void EntityManager::ForEachCustomEntityView(CustomEntity* entity, const std::function<void(Player* player, bool owner)>& callback)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto view_callback = [&](Player* player, bool owner) {
        if (player != nullptr) {
            callback(player, owner);
        }
    };

    std::function<void(Entity*, EntityHolderEntryAccess)> find_players_recursively;

    find_players_recursively = [this, &find_players_recursively, &view_callback](Entity* holder, EntityHolderEntryAccess derived_access) {
        if (const auto* custom_entity = dynamic_cast<CustomEntity*>(holder); custom_entity != nullptr) {
            if (Entity* custom_entity_holder = GetEntity(custom_entity->GetCustomHolderId()); custom_entity_holder != nullptr) {
                const auto custom_entity_holder_type_info = _engine->GetEntityTypeInfo(custom_entity_holder->GetTypeName());
                const auto entry = custom_entity->GetCustomHolderEntry();
                const auto entry_access = std::get<1>(custom_entity_holder_type_info.HolderEntries.at(entry));

                if (entry_access == EntityHolderEntryAccess::Protected || entry_access == EntityHolderEntryAccess::Public) {
                    find_players_recursively(custom_entity_holder, std::min(entry_access, derived_access));
                }
            }
        }
        else if (auto* cr = dynamic_cast<Critter*>(holder)) {
            view_callback(cr->GetPlayer(), true);

            if (derived_access == EntityHolderEntryAccess::Public) {
                for (auto* another_cr : cr->VisCr) {
                    view_callback(another_cr->GetPlayer(), false);
                }
            }
        }
        else if (auto* player = dynamic_cast<Player*>(holder)) {
            view_callback(player, true);
        }
        else if (const auto* game = dynamic_cast<FOServer*>(holder); game != nullptr) {
            for (auto&& [id, game_player] : GetPlayers()) {
                view_callback(game_player, false);
            }
        }
        else if (auto* loc = dynamic_cast<Location*>(holder); loc != nullptr) {
            for (auto* loc_map : loc->GetMaps()) {
                find_players_recursively(loc_map, derived_access);
            }
        }
        else if (auto* map = dynamic_cast<Map*>(holder)) {
            for (auto* map_cr : map->GetPlayerCritters()) {
                view_callback(map_cr->GetPlayer(), false);
            }
        }
        else if (const auto* item = dynamic_cast<Item*>(holder)) {
            switch (item->GetOwnership()) {
            case ItemOwnership::CritterInventory: {
                if (auto* item_cr = GetCritter(item->GetCritterId()); item_cr != nullptr) {
                    find_players_recursively(item_cr, derived_access);
                }
            } break;
            case ItemOwnership::MapHex: {
                if (derived_access == EntityHolderEntryAccess::Public) {
                    if (auto* item_map = GetMap(item->GetMapId()); item_map != nullptr) {
                        for (auto* map_cr : item_map->GetPlayerCritters()) {
                            if (map_cr->VisItem.count(item->GetId()) != 0) {
                                view_callback(map_cr->GetPlayer(), false);
                            }
                        }
                    }
                }
            } break;
            case ItemOwnership::ItemContainer: {
                if (auto* item_cont = GetItem(item->GetContainerId()); item_cont != nullptr) {
                    find_players_recursively(item_cont, derived_access);
                }
            } break;
            default:
                break;
            }
        }
    };

    find_players_recursively(entity, EntityHolderEntryAccess::Public);
}
