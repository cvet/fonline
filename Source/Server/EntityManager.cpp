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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "MapManager.h"
#include "PropertiesSerializator.h"
#include "ProtoManager.h"
#include "Server.h"

FO_BEGIN_NAMESPACE();

EntityManager::EntityManager(FOServer* engine) :
    _engine {engine},
    _playerTypeName {engine->Hashes.ToHashedString(Player::ENTITY_TYPE_NAME)},
    _locationTypeName {engine->Hashes.ToHashedString(Location::ENTITY_TYPE_NAME)},
    _mapTypeName {engine->Hashes.ToHashedString(Map::ENTITY_TYPE_NAME)},
    _critterTypeName {engine->Hashes.ToHashedString(Critter::ENTITY_TYPE_NAME)},
    _itemTypeName {engine->Hashes.ToHashedString(Item::ENTITY_TYPE_NAME)},
    _playerCollectionName {engine->Hashes.ToHashedString(strex("{}s", Player::ENTITY_TYPE_NAME))},
    _locationCollectionName {engine->Hashes.ToHashedString(strex("{}s", Location::ENTITY_TYPE_NAME))},
    _mapCollectionName {engine->Hashes.ToHashedString(strex("{}s", Map::ENTITY_TYPE_NAME))},
    _critterCollectionName {engine->Hashes.ToHashedString(strex("{}s", Critter::ENTITY_TYPE_NAME))},
    _itemCollectionName {engine->Hashes.ToHashedString(strex("{}s", Item::ENTITY_TYPE_NAME))},
    _removeMigrationRuleName {engine->Hashes.ToHashedString("Remove")}
{
    FO_STACK_TRACE_ENTRY();
}

auto EntityManager::GetEntity(ident_t id) const noexcept -> const ServerEntity*
{
    FO_NO_STACK_TRACE_ENTRY();

    if (const auto it = _allEntities.find(id); it != _allEntities.end()) {
        return it->second.get();
    }

    return nullptr;
}

auto EntityManager::GetEntity(ident_t id) noexcept -> ServerEntity*
{
    FO_NO_STACK_TRACE_ENTRY();

    if (const auto it = _allEntities.find(id); it != _allEntities.end()) {
        return it->second.get();
    }

    return nullptr;
}

auto EntityManager::GetPlayer(ident_t id) const noexcept -> const Player*
{
    FO_NO_STACK_TRACE_ENTRY();

    if (const auto it = _allPlayers.find(id); it != _allPlayers.end()) {
        return it->second.get();
    }

    return nullptr;
}

auto EntityManager::GetPlayer(ident_t id) noexcept -> Player*
{
    FO_NO_STACK_TRACE_ENTRY();

    if (const auto it = _allPlayers.find(id); it != _allPlayers.end()) {
        return it->second.get();
    }

    return nullptr;
}

auto EntityManager::GetLocation(ident_t id) const noexcept -> const Location*
{
    FO_NO_STACK_TRACE_ENTRY();

    if (const auto it = _allLocations.find(id); it != _allLocations.end()) {
        return it->second.get();
    }

    return nullptr;
}

auto EntityManager::GetLocation(ident_t id) noexcept -> Location*
{
    FO_NO_STACK_TRACE_ENTRY();

    if (const auto it = _allLocations.find(id); it != _allLocations.end()) {
        return it->second.get();
    }

    return nullptr;
}

auto EntityManager::GetMap(ident_t id) const noexcept -> const Map*
{
    FO_NO_STACK_TRACE_ENTRY();

    if (const auto it = _allMaps.find(id); it != _allMaps.end()) {
        return it->second.get();
    }

    return nullptr;
}

auto EntityManager::GetMap(ident_t id) noexcept -> Map*
{
    FO_NO_STACK_TRACE_ENTRY();

    if (const auto it = _allMaps.find(id); it != _allMaps.end()) {
        return it->second.get();
    }

    return nullptr;
}

auto EntityManager::GetCritter(ident_t id) const noexcept -> const Critter*
{
    FO_NO_STACK_TRACE_ENTRY();

    if (const auto it = _allCritters.find(id); it != _allCritters.end()) {
        return it->second.get();
    }

    return nullptr;
}

auto EntityManager::GetCritter(ident_t id) noexcept -> Critter*
{
    FO_NO_STACK_TRACE_ENTRY();

    if (const auto it = _allCritters.find(id); it != _allCritters.end()) {
        return it->second.get();
    }

    return nullptr;
}

auto EntityManager::GetItem(ident_t id) const noexcept -> const Item*
{
    FO_NO_STACK_TRACE_ENTRY();

    if (const auto it = _allItems.find(id); it != _allItems.end()) {
        return it->second.get();
    }

    return nullptr;
}

auto EntityManager::GetItem(ident_t id) noexcept -> Item*
{
    FO_NO_STACK_TRACE_ENTRY();

    if (const auto it = _allItems.find(id); it != _allItems.end()) {
        return it->second.get();
    }

    return nullptr;
}

void EntityManager::LoadEntities()
{
    FO_STACK_TRACE_ENTRY();

    WriteLog("Load entities");

    bool is_error = false;

    LoadInnerEntities(_engine.get(), is_error);

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

auto EntityManager::LoadLocation(ident_t loc_id, bool& is_error) noexcept -> Location*
{
    FO_STACK_TRACE_ENTRY();

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

    auto loc = SafeAlloc::MakeRefCounted<Location>(_engine.get(), loc_id, loc_proto);

    if (!PropertiesSerializator::LoadFromDocument(&loc->GetPropertiesForEdit(), loc_doc, _engine->Hashes, *_engine)) {
        WriteLog("Failed to restore location {} {} properties", loc_pid, loc_id);
        is_error = true;
        return nullptr;
    }

    try {
        RegisterLocation(loc.get());
    }
    catch (const std::exception& ex) {
        WriteLog("Failed to register location {} {}", loc_pid, loc_id);
        ReportExceptionAndContinue(ex);
        is_error = true;
        return nullptr;
    }

    try {
        const auto map_ids = loc->GetMapIds();
        bool map_ids_changed = false;

        for (const auto& map_id : map_ids) {
            auto* map = LoadMap(map_id, is_error);

            if (map != nullptr) {
                FO_RUNTIME_ASSERT(map->GetLocId() == loc->GetId());

                const auto loc_map_index = map->GetLocMapIndex();
                auto& loc_maps = loc->GetMaps();

                if (loc_map_index >= numeric_cast<int32>(loc_maps.size())) {
                    loc_maps.resize(loc_map_index + 1);
                }

                loc_maps[loc_map_index] = map;
                map->SetLocation(loc.get());
            }
            else {
                map_ids_changed = true;
            }
        }

        if (map_ids_changed) {
            const auto actual_map_ids = vec_transform(loc->GetMaps(), [](auto&& map) -> ident_t { return map->GetId(); });
            loc->SetMapIds(actual_map_ids);
        }

        // Inner entities
        LoadInnerEntities(loc.get(), is_error);
    }
    catch (const std::exception& ex) {
        WriteLog("Failed during restore location content {} {}", loc_pid, loc_id);
        ReportExceptionAndContinue(ex);
        is_error = true;
    }

    return loc.get();
}

auto EntityManager::LoadMap(ident_t map_id, bool& is_error) noexcept -> Map*
{
    FO_STACK_TRACE_ENTRY();

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

    auto* static_map = _engine->MapMngr.GetStaticMap(map_proto);
    auto map = SafeAlloc::MakeRefCounted<Map>(_engine.get(), map_id, map_proto, nullptr, static_map);

    if (!PropertiesSerializator::LoadFromDocument(&map->GetPropertiesForEdit(), map_doc, _engine->Hashes, *_engine)) {
        WriteLog("Failed to restore map {} {} properties", map_pid, map_id);
        is_error = true;
        return nullptr;
    }

    try {
        RegisterMap(map.get());
    }
    catch (const std::exception& ex) {
        WriteLog("Failed to register map {} {}", map_pid, map_id);
        ReportExceptionAndContinue(ex);
        is_error = true;
        return nullptr;
    }

    try {
        // Map critters
        const auto cr_ids = map->GetCritterIds();
        bool cr_ids_changed = false;

        for (const auto& cr_id : cr_ids) {
            auto* cr = LoadCritter(cr_id, is_error);

            if (cr != nullptr) {
                FO_RUNTIME_ASSERT(cr->GetMapId() == map->GetId());

                if (const auto hex = cr->GetHex(); !map->GetSize().is_valid_pos(hex)) {
                    cr->SetHex(map->GetSize().clamp_pos(hex));
                }

                map->AddCritter(cr);
            }
            else {
                cr_ids_changed = true;
            }
        }

        if (cr_ids_changed) {
            const auto actual_cr_ids = vec_transform(map->GetCritters(), [](auto&& cr) -> ident_t { return cr->GetId(); });
            map->SetCritterIds(actual_cr_ids);
        }

        // Map items
        const auto item_ids = map->GetItemIds();
        bool item_ids_changed = false;

        for (const auto& item_id : item_ids) {
            auto* item = LoadItem(item_id, is_error);

            if (item != nullptr) {
                FO_RUNTIME_ASSERT(item->GetOwnership() == ItemOwnership::MapHex);
                FO_RUNTIME_ASSERT(item->GetMapId() == map->GetId());

                if (const auto hex = item->GetHex(); !map->GetSize().is_valid_pos(hex)) {
                    item->SetHex(map->GetSize().clamp_pos(hex));
                }

                map->SetItem(item);
            }
            else {
                item_ids_changed = true;
            }
        }

        if (item_ids_changed) {
            const auto actual_item_ids = vec_transform(map->GetItems(), [](auto&& item) -> ident_t { return item->GetId(); });
            map->SetItemIds(actual_item_ids);
        }

        // Inner entities
        LoadInnerEntities(map.get(), is_error);
    }
    catch (const std::exception& ex) {
        WriteLog("Failed during restore map content {} {}", map_pid, map_id);
        ReportExceptionAndContinue(ex);
        is_error = true;
    }

    return map.get();
}

auto EntityManager::LoadCritter(ident_t cr_id, bool& is_error) noexcept -> Critter*
{
    FO_STACK_TRACE_ENTRY();

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

    auto cr = SafeAlloc::MakeRefCounted<Critter>(_engine.get(), cr_id, proto);

    if (!PropertiesSerializator::LoadFromDocument(&cr->GetPropertiesForEdit(), cr_doc, _engine->Hashes, *_engine)) {
        WriteLog("Failed to restore critter {} {} properties", cr_pid, cr_id);
        is_error = true;
        return nullptr;
    }

    try {
        RegisterCritter(cr.get());
    }
    catch (const std::exception& ex) {
        WriteLog("Failed to register critter {} {}", cr_pid, cr_id);
        ReportExceptionAndContinue(ex);
        is_error = true;
        return nullptr;
    }

    try {
        // Inventory
        const auto item_ids = cr->GetItemIds();
        bool item_ids_changed = false;

        for (const auto& item_id : item_ids) {
            auto* inv_item = LoadItem(item_id, is_error);

            if (inv_item != nullptr) {
                FO_RUNTIME_ASSERT(inv_item->GetOwnership() == ItemOwnership::CritterInventory);
                FO_RUNTIME_ASSERT(inv_item->GetCritterId() == cr->GetId());

                cr->SetItem(inv_item);
            }
            else {
                item_ids_changed = true;
            }
        }

        if (item_ids_changed) {
            const auto actual_item_ids = vec_transform(cr->GetInvItems(), [](auto&& item) -> ident_t { return item->GetId(); });
            cr->SetItemIds(actual_item_ids);
        }

        // Inner entities
        LoadInnerEntities(cr.get(), is_error);
    }
    catch (const std::exception& ex) {
        WriteLog("Failed during restore critter content {} {}", cr_pid, cr_id);
        ReportExceptionAndContinue(ex);
        is_error = true;
    }

    return cr.get();
}

auto EntityManager::LoadItem(ident_t item_id, bool& is_error) noexcept -> Item*
{
    FO_STACK_TRACE_ENTRY();

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

    auto item = SafeAlloc::MakeRefCounted<Item>(_engine.get(), item_id, proto);

    if (!PropertiesSerializator::LoadFromDocument(&item->GetPropertiesForEdit(), item_doc, _engine->Hashes, *_engine)) {
        WriteLog("Failed to restore item {} {} properties", item_pid, item_id);
        is_error = true;
        return nullptr;
    }

    try {
        item->SetStatic(false);
        RegisterItem(item.get());
    }
    catch (const std::exception& ex) {
        WriteLog("Failed to register item {} {}", item_pid, item_id);
        ReportExceptionAndContinue(ex);
        is_error = true;
        return nullptr;
    }

    try {
        // Inner items
        const auto inner_item_ids = item->GetInnerItemIds();
        bool inner_item_ids_changed = false;

        for (const auto& inner_item_id : inner_item_ids) {
            auto* inner_item = LoadItem(inner_item_id, is_error);

            if (inner_item != nullptr) {
                FO_RUNTIME_ASSERT(inner_item->GetOwnership() == ItemOwnership::ItemContainer);
                FO_RUNTIME_ASSERT(inner_item->GetContainerId() == item->GetId());

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
        LoadInnerEntities(item.get(), is_error);
    }
    catch (const std::exception& ex) {
        WriteLog("Failed during restore item content {} {}", item_pid, item_id);
        ReportExceptionAndContinue(ex);
        is_error = true;
    }

    return item.get();
}

void EntityManager::LoadInnerEntities(Entity* holder, bool& is_error) noexcept
{
    FO_STACK_TRACE_ENTRY();

    try {
        const auto& holder_type_info = _engine->GetEntityTypeInfo(holder->GetTypeName());

        for (const auto& entry : holder_type_info.HolderEntries | std::views::keys) {
            LoadInnerEntitiesEntry(holder, entry, is_error);
        }
    }
    catch (const std::exception& ex) {
        WriteLog("Failed during restore inner entities for {}", holder->GetTypeName());
        ReportExceptionAndContinue(ex);
        is_error = true;
    }
}

void EntityManager::LoadInnerEntitiesEntry(Entity* holder, hstring entry, bool& is_error) noexcept
{
    FO_STACK_TRACE_ENTRY();

    try {
        const auto* holder_prop = _engine->GetEntityHolderIdsProp(holder, entry);
        auto& holder_props = holder->GetPropertiesForEdit();
        const auto inner_entity_ids = holder_props.GetValueFast<vector<ident_t>>(holder_prop);

        if (inner_entity_ids.empty()) {
            return;
        }

        bool inner_entity_ids_changed = false;
        ident_t holder_id = {};

        if (const auto* holder_with_id = dynamic_cast<ServerEntity*>(holder)) {
            holder_id = holder_with_id->GetId();
        }

        const auto& holder_type_info = _engine->GetEntityTypeInfo(holder->GetTypeName());
        const auto inner_entity_type_name = std::get<0>(holder_type_info.HolderEntries.at(entry));

        for (const auto& id : inner_entity_ids) {
            auto* custom_entity = LoadCustomEntity(inner_entity_type_name, id, is_error);

            if (custom_entity != nullptr) {
                FO_RUNTIME_ASSERT(custom_entity->GetCustomHolderId() == holder_id);

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
            const auto* inner_entities = holder->GetInnerEntities(entry);

            if (inner_entities != nullptr) {
                actual_inner_entity_ids = vec_transform(*inner_entities, [](auto&& entity) -> ident_t {
                    return static_cast<const CustomEntity*>(entity.get())->GetId(); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                });
            }

            holder_props.SetValue(holder_prop, actual_inner_entity_ids);
        }
    }
    catch (const std::exception& ex) {
        WriteLog("Failed during restore inner entities for {}", holder->GetTypeName());
        ReportExceptionAndContinue(ex);
        is_error = true;
    }
}

auto EntityManager::LoadEntityDoc(hstring type_name, hstring collection_name, ident_t id, bool expect_proto, bool& is_error) const noexcept -> tuple<AnyData::Document, hstring>
{
    FO_STACK_TRACE_ENTRY();

    try {
        FO_RUNTIME_ASSERT(id.underlying_value() != 0);

        auto doc = _engine->DbStorage.Get(collection_name, id);

        if (doc.Empty()) {
            WriteLog("{} document {} not found", collection_name, id);
            is_error = true;
            return {};
        }

        if (!doc.Contains("_Proto")) {
            if (expect_proto) {
                WriteLog("{} '_Proto' section not found in entity {}", collection_name, id);
                is_error = true;
            }

            return {std::move(doc), hstring()};
        }

        const auto& proto_value = doc["_Proto"];

        if (proto_value.Type() != AnyData::ValueType::String) {
            WriteLog("{} '_Proto' section of entity {} is not string type (but {})", collection_name, id, proto_value.Type());
            is_error = true;
            return {};
        }

        const auto& proto_name = proto_value.AsString();

        if (proto_name.empty()) {
            WriteLog("{} '_Proto' section of entity {} is empty", collection_name, id);
            is_error = true;
            return {};
        }

        auto proto_id = _engine->Hashes.ToHashedString(proto_name);

        if (_engine->CheckMigrationRule(_removeMigrationRuleName, type_name, proto_id).has_value()) {
            return {};
        }

        return {std::move(doc), proto_id};
    }
    catch (const std::exception& ex) {
        WriteLog("Failed during load document {} {}", collection_name, id);
        ReportExceptionAndContinue(ex);
        is_error = true;
        return {};
    }
}

void EntityManager::CallInit(Location* loc, bool first_time)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!loc->IsDestroyed());

    if (loc->IsInitCalled()) {
        return;
    }

    refcount_ptr loc_holder = loc;

    loc->SetInitCalled();

    _engine->OnLocationInit.Fire(loc, first_time);

    if (!loc->IsDestroyed()) {
        ScriptHelpers::CallInitScript(_engine->ScriptSys, loc, loc->GetInitScript(), first_time);
    }

    if (!loc->IsDestroyed()) {
        for (auto& map : copy_hold_ref(loc->GetMaps())) {
            if (!map->IsDestroyed()) {
                CallInit(map.get(), first_time);
            }
        }
    }
}

void EntityManager::CallInit(Map* map, bool first_time)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!map->IsDestroyed());

    if (map->IsInitCalled()) {
        return;
    }

    refcount_ptr map_holder = map;

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
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!cr->IsDestroyed());

    if (cr->IsInitCalled()) {
        return;
    }

    refcount_ptr cr_holder = cr;

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
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!item->IsDestroyed());

    if (item->IsInitCalled()) {
        return;
    }

    refcount_ptr item_holder = item;

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

void EntityManager::RegisterPlayer(Player* player, ident_t id)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(id);
    player->SetId(id);
    RegisterEntity(player);
    const auto [it, inserted] = _allPlayers.emplace(player->GetId(), player);
    FO_RUNTIME_ASSERT(inserted);
}

void EntityManager::UnregisterPlayer(Player* player)
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _allPlayers.find(player->GetId());
    FO_RUNTIME_ASSERT(it != _allPlayers.end());
    _allPlayers.erase(it);
    UnregisterEntity(player, false);
}

void EntityManager::RegisterLocation(Location* loc)
{
    FO_STACK_TRACE_ENTRY();

    RegisterEntity(loc);
    const auto [it, inserted] = _allLocations.emplace(loc->GetId(), loc);
    FO_RUNTIME_ASSERT(inserted);
}

void EntityManager::UnregisterLocation(Location* loc)
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _allLocations.find(loc->GetId());
    FO_RUNTIME_ASSERT(it != _allLocations.end());
    _allLocations.erase(it);
    UnregisterEntity(loc, true);
}

void EntityManager::RegisterMap(Map* map)
{
    FO_STACK_TRACE_ENTRY();

    RegisterEntity(map);
    const auto [it, inserted] = _allMaps.emplace(map->GetId(), map);
    FO_RUNTIME_ASSERT(inserted);
}

void EntityManager::UnregisterMap(Map* map)
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _allMaps.find(map->GetId());
    FO_RUNTIME_ASSERT(it != _allMaps.end());
    _allMaps.erase(it);
    UnregisterEntity(map, true);
}

void EntityManager::RegisterCritter(Critter* cr)
{
    FO_STACK_TRACE_ENTRY();

    RegisterEntity(cr);
    const auto [it, inserted] = _allCritters.emplace(cr->GetId(), cr);
    FO_RUNTIME_ASSERT(inserted);
}

void EntityManager::UnregisterCritter(Critter* cr)
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _allCritters.find(cr->GetId());
    FO_RUNTIME_ASSERT(it != _allCritters.end());
    _allCritters.erase(it);
    UnregisterEntity(cr, !cr->GetControlledByPlayer());
}

void EntityManager::RegisterItem(Item* item)
{
    FO_STACK_TRACE_ENTRY();

    RegisterEntity(item);
    const auto [it, inserted] = _allItems.emplace(item->GetId(), item);
    FO_RUNTIME_ASSERT(inserted);
}

void EntityManager::UnregisterItem(Item* item, bool delete_from_db)
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _allItems.find(item->GetId());
    FO_RUNTIME_ASSERT(it != _allItems.end());
    _allItems.erase(it);
    UnregisterEntity(item, delete_from_db);
}

void EntityManager::RegisterCustomEntity(CustomEntity* custom_entity)
{
    FO_STACK_TRACE_ENTRY();

    RegisterEntity(custom_entity);
    auto& custom_entities = _allCustomEntities[custom_entity->GetTypeName()];
    const auto [it, inserted] = custom_entities.emplace(custom_entity->GetId(), custom_entity);
    FO_RUNTIME_ASSERT(inserted);
}

void EntityManager::UnregisterCustomEntity(CustomEntity* custom_entity, bool delete_from_db)
{
    FO_STACK_TRACE_ENTRY();

    auto& custom_entities = _allCustomEntities[custom_entity->GetTypeName()];
    const auto it = custom_entities.find(custom_entity->GetId());
    FO_RUNTIME_ASSERT(it != custom_entities.end());
    custom_entities.erase(it);
    UnregisterEntity(custom_entity, delete_from_db);
}

void EntityManager::RegisterEntity(ServerEntity* entity)
{
    FO_STACK_TRACE_ENTRY();

    if (!entity->GetId()) {
        const auto id_num = std::max(_engine->GetLastEntityId().underlying_value() + 1, _engine->Settings.EntityStartId);
        const auto id = ident_t {id_num};
        const auto collection_name = entity->GetTypeNamePlural();

        FO_RUNTIME_ASSERT(_allEntities.count(id) == 0);

        _engine->SetLastEntityId(id);

        if (const auto* entity_with_proto = dynamic_cast<EntityWithProto*>(entity); entity_with_proto != nullptr) {
            const auto* proto = entity_with_proto->GetProto();
            auto doc = PropertiesSerializator::SaveToDocument(&entity->GetProperties(), &proto->GetProperties(), _engine->Hashes, *_engine);

            doc.Emplace("_Proto", string(proto->GetName()));

            _engine->DbStorage.Insert(collection_name, id, doc);
        }
        else {
            const auto doc = PropertiesSerializator::SaveToDocument(&entity->GetProperties(), nullptr, _engine->Hashes, *_engine);

            _engine->DbStorage.Insert(collection_name, id, doc);
        }

        entity->SetId(id);
    }

    const auto [it, inserted] = _allEntities.emplace(entity->GetId(), entity);
    FO_RUNTIME_ASSERT(inserted);
}

void EntityManager::UnregisterEntity(ServerEntity* entity, bool delete_from_db)
{
    FO_STACK_TRACE_ENTRY();

    const auto entity_id = entity->GetId();
    const auto type_name_plural = entity->GetTypeNamePlural();
    FO_RUNTIME_ASSERT(entity_id);

    const auto it = _allEntities.find(entity_id);
    FO_RUNTIME_ASSERT(it != _allEntities.end());
    _allEntities.erase(it); // Maybe last pointer to this entity

    if (delete_from_db) {
        _engine->DbStorage.Delete(type_name_plural, entity_id);
    }
}

void EntityManager::DestroyEntity(Entity* entity)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(entity);

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
    FO_STACK_TRACE_ENTRY();

    for (InfinityLoopDetector detector; holder->HasInnerEntities(); detector.AddLoop()) {
        for (auto&& [entry, entities] : copy(holder->GetInnerEntities())) {
            for (auto& entity : entities) {
                DestroyEntity(entity.get());
            }
        }
    }
}

void EntityManager::DestroyAllEntities()
{
    FO_STACK_TRACE_ENTRY();

    const auto destroy_entities = [this](auto& entities) {
        for (auto&& [id, entity] : copy(entities)) {
            entity->MarkAsDestroyed();
            entities.erase(id);
            _allEntities.erase(id);
        }

        FO_RUNTIME_ASSERT(entities.empty());
    };

    destroy_entities(_allPlayers);
    destroy_entities(_allLocations);
    destroy_entities(_allMaps);
    destroy_entities(_allCritters);
    destroy_entities(_allItems);

    for (auto& val : _allCustomEntities | std::views::values) {
        destroy_entities(val);
    }

    FO_RUNTIME_ASSERT(_allEntities.empty());
}

auto EntityManager::CreateCustomInnerEntity(Entity* holder, hstring entry, hstring pid) -> CustomEntity*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(holder);
    FO_RUNTIME_ASSERT(_engine->GetEntityTypeInfo(holder->GetTypeName()).HolderEntries.count(entry));

    const hstring type_name = std::get<0>(_engine->GetEntityTypeInfo(holder->GetTypeName()).HolderEntries.at(entry));

    auto* entity = CreateCustomEntity(type_name, pid);
    FO_RUNTIME_ASSERT(entity);

    if (const auto* holder_with_id = dynamic_cast<ServerEntity*>(holder); holder_with_id != nullptr) {
        FO_RUNTIME_ASSERT(holder_with_id->GetId());
        entity->SetCustomHolderId(holder_with_id->GetId());
    }
    else {
        FO_RUNTIME_ASSERT(_engine.get() == holder);
    }

    entity->SetCustomHolderEntry(entry);

    holder->AddInnerEntity(entry, entity);

    const auto* holder_prop = _engine->GetEntityHolderIdsProp(holder, entry);
    auto& holder_props = holder->GetPropertiesForEdit();
    auto inner_entity_ids = holder_props.GetValueFast<vector<ident_t>>(holder_prop);
    vec_add_unique_value(inner_entity_ids, entity->GetId());
    holder_props.SetValue(holder_prop, inner_entity_ids);

    ForEachCustomEntityView(entity, [entity](Player* player, bool owner) { player->Send_AddCustomEntity(entity, owner); });

    return entity;
}

auto EntityManager::CreateCustomEntity(hstring type_name, hstring pid) -> CustomEntity*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_engine->IsValidEntityType(type_name));

    const bool has_protos = _engine->GetEntityTypeInfo(type_name).HasProtos;
    const ProtoEntity* proto = nullptr;

    if (pid) {
        FO_RUNTIME_ASSERT(has_protos);
        proto = _engine->ProtoMngr.GetProtoEntity(type_name, pid);
        FO_RUNTIME_ASSERT(proto);
    }
    else {
        FO_RUNTIME_ASSERT(!has_protos);
    }

    const auto* registrator = _engine->GetPropertyRegistrator(type_name);

    refcount_ptr<CustomEntity> entity;

    if (proto != nullptr) {
        entity = SafeAlloc::MakeRefCounted<CustomEntityWithProto>(_engine.get(), ident_t {}, registrator, proto);
    }
    else {
        entity = SafeAlloc::MakeRefCounted<CustomEntity>(_engine.get(), ident_t {}, registrator, nullptr);
    }

    RegisterCustomEntity(entity.get());

    return entity.get();
}

auto EntityManager::LoadCustomEntity(hstring type_name, ident_t id, bool& is_error) noexcept -> CustomEntity*
{
    FO_STACK_TRACE_ENTRY();

    try {
        FO_RUNTIME_ASSERT(id.underlying_value() != 0);

        if (!_engine->IsValidEntityType(type_name)) {
            WriteLog("Custom entity {} type {} not valid", id, type_name);
            is_error = true;
            return nullptr;
        }

        FO_RUNTIME_ASSERT(_allCustomEntities[type_name].count(id) == 0);

        const auto collection_name = _engine->Hashes.ToHashedString(strex("{}s", type_name));
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
                proto = _engine->ProtoMngr.GetProtoEntity(type_name, _engine->Hashes.ToHashedString("Default"));
            }

            if (proto == nullptr) {
                WriteLog("Proto {} for custom entity {} with type {} not found", pid, id, type_name);
                is_error = true;
                return nullptr;
            }
        }

        const auto* registrator = _engine->GetPropertyRegistrator(type_name);
        refcount_ptr<CustomEntity> entity;

        if (proto != nullptr) {
            entity = SafeAlloc::MakeRefCounted<CustomEntityWithProto>(_engine.get(), id, registrator, proto);
        }
        else {
            entity = SafeAlloc::MakeRefCounted<CustomEntity>(_engine.get(), id, registrator, nullptr);
        }

        if (!PropertiesSerializator::LoadFromDocument(&entity->GetPropertiesForEdit(), doc, _engine->Hashes, *_engine)) {
            WriteLog("Failed to load properties for custom entity {} with type {}", id, type_name);
            is_error = true;
            return nullptr;
        }

        RegisterCustomEntity(entity.get());

        return entity.get();
    }
    catch (const std::exception& ex) {
        WriteLog("Failed during load custom entity {}", id);
        ReportExceptionAndContinue(ex);
        is_error = true;
        return nullptr;
    }
}

auto EntityManager::GetCustomEntity(hstring type_name, ident_t id) -> CustomEntity*
{
    FO_STACK_TRACE_ENTRY();

    auto& custom_entities = _allCustomEntities[type_name];
    const auto it = custom_entities.find(id);

    return it != custom_entities.end() ? it->second.get() : nullptr;
}

void EntityManager::DestroyCustomEntity(CustomEntity* entity)
{
    FO_STACK_TRACE_ENTRY();

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
        FO_RUNTIME_ASSERT(holder);
    }
    else {
        holder = _engine.get();
    }

    ForEachCustomEntityView(entity, [entity](Player* player, bool owner) {
        ignore_unused(owner);
        player->Send_RemoveCustomEntity(entity->GetId());
    });

    const auto entry = entity->GetCustomHolderEntry();
    holder->RemoveInnerEntity(entry, entity);

    const auto* holder_prop = _engine->GetEntityHolderIdsProp(holder, entry);
    auto& holder_props = holder->GetPropertiesForEdit();
    auto inner_entity_ids = holder_props.GetValueFast<vector<ident_t>>(holder_prop);
    vec_remove_unique_value(inner_entity_ids, entity->GetId());
    holder_props.SetValue(holder_prop, inner_entity_ids);

    entity->MarkAsDestroyed();
    UnregisterCustomEntity(entity, true);
}

void EntityManager::ForEachCustomEntityView(CustomEntity* entity, const function<void(Player* player, bool owner)>& callback)
{
    FO_STACK_TRACE_ENTRY();

    const auto view_callback = [&](Player* player, bool owner) {
        if (player != nullptr) {
            callback(player, owner);
        }
    };

    function<void(Entity*, EntityHolderEntryAccess)> find_players_recursively;

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
                for (auto* another_cr : cr->GetCritters(CritterSeeType::WhoSeeMe, CritterFindType::Any)) {
                    view_callback(another_cr->GetPlayer(), false);
                }
            }
        }
        else if (auto* player = dynamic_cast<Player*>(holder)) {
            view_callback(player, true);
        }
        else if (const auto* game = dynamic_cast<FOServer*>(holder); game != nullptr) {
            for (auto& game_player : GetPlayers() | std::views::values) {
                view_callback(game_player.get(), false);
            }
        }
        else if (auto* loc = dynamic_cast<Location*>(holder); loc != nullptr) {
            for (auto& loc_map : loc->GetMaps()) {
                find_players_recursively(loc_map.get(), derived_access);
            }
        }
        else if (auto* map = dynamic_cast<Map*>(holder)) {
            for (auto& map_cr : map->GetPlayerCritters()) {
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
                        for (auto& map_cr : item_map->GetPlayerCritters()) {
                            if (map_cr->IsSeeItem(item->GetId())) {
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

FO_END_NAMESPACE();
