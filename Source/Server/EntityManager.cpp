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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "EntitySync.h"
#include "ItemManager.h"
#include "MapManager.h"
#include "PropertiesSerializator.h"
#include "ProtoManager.h"
#include "Server.h"

FO_BEGIN_NAMESPACE

EntityManager::EntityManager(ServerEngine* engine) :
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
    _protoMigrationRuleName {engine->Hashes.ToHashedString("Proto")},
    _removeMigrationReplacement {engine->Hashes.ToHashedString("Remove")}
{
    FO_STACK_TRACE_ENTRY();
}

auto EntityManager::GetEntity(ident_t id) const noexcept -> refcount_ptr<const ServerEntity>
{
    FO_STACK_TRACE_ENTRY();

    shared_lock lock {_registryLock};

    if (const auto it = _allEntities.find(id); it != _allEntities.end()) {
        return refcount_ptr<const ServerEntity>(it->second.get());
    }

    return {};
}

auto EntityManager::GetEntity(ident_t id) noexcept -> refcount_ptr<ServerEntity>
{
    FO_STACK_TRACE_ENTRY();

    shared_lock lock {_registryLock};

    if (const auto it = _allEntities.find(id); it != _allEntities.end()) {
        return refcount_ptr<ServerEntity>(it->second.get());
    }

    return {};
}

auto EntityManager::GetEntities() noexcept -> vector<refcount_ptr<ServerEntity>>
{
    FO_STACK_TRACE_ENTRY();

    shared_lock lock {_registryLock};

    vector<refcount_ptr<ServerEntity>> result;
    result.reserve(_allEntities.size());

    for (auto& [id, ptr] : _allEntities) {
        result.emplace_back(ptr.get_no_const());
    }

    return result;
}

auto EntityManager::GetEntitiesCount() const noexcept -> size_t
{
    FO_STACK_TRACE_ENTRY();

    shared_lock lock {_registryLock};

    return _allEntities.size();
}

auto EntityManager::GetPlayer(ident_t id) const noexcept -> refcount_ptr<const Player>
{
    FO_STACK_TRACE_ENTRY();

    shared_lock lock {_registryLock};

    if (const auto it = _allPlayers.find(id); it != _allPlayers.end()) {
        return refcount_ptr<const Player>(it->second.get());
    }

    return {};
}

auto EntityManager::GetPlayer(ident_t id) noexcept -> refcount_ptr<Player>
{
    FO_STACK_TRACE_ENTRY();

    shared_lock lock {_registryLock};

    if (const auto it = _allPlayers.find(id); it != _allPlayers.end()) {
        return refcount_ptr<Player>(it->second.get());
    }

    return {};
}

auto EntityManager::GetPlayers() noexcept -> vector<refcount_ptr<Player>>
{
    FO_STACK_TRACE_ENTRY();

    shared_lock lock {_registryLock};

    vector<refcount_ptr<Player>> result;
    result.reserve(_allPlayers.size());

    for (auto& [id, ptr] : _allPlayers) {
        result.emplace_back(ptr.get_no_const());
    }

    return result;
}

auto EntityManager::GetPlayersCount() const noexcept -> size_t
{
    FO_STACK_TRACE_ENTRY();

    shared_lock lock {_registryLock};

    return _allPlayers.size();
}

auto EntityManager::GetLocation(ident_t id) const noexcept -> refcount_ptr<const Location>
{
    FO_STACK_TRACE_ENTRY();

    shared_lock lock {_registryLock};

    if (const auto it = _allLocations.find(id); it != _allLocations.end()) {
        return refcount_ptr<const Location>(it->second.get());
    }

    return {};
}

auto EntityManager::GetLocation(ident_t id) noexcept -> refcount_ptr<Location>
{
    FO_STACK_TRACE_ENTRY();

    shared_lock lock {_registryLock};

    if (const auto it = _allLocations.find(id); it != _allLocations.end()) {
        return refcount_ptr<Location>(it->second.get());
    }

    return {};
}

auto EntityManager::GetLocations() noexcept -> vector<refcount_ptr<Location>>
{
    FO_STACK_TRACE_ENTRY();

    shared_lock lock {_registryLock};

    vector<refcount_ptr<Location>> result;
    result.reserve(_allLocations.size());

    for (const auto& [id, ptr] : _allLocations) {
        result.emplace_back(ptr.get_no_const());
    }

    return result;
}

auto EntityManager::GetLocationsCount() const noexcept -> size_t
{
    FO_STACK_TRACE_ENTRY();

    shared_lock lock {_registryLock};

    return _allLocations.size();
}

auto EntityManager::GetMap(ident_t id) const noexcept -> refcount_ptr<const Map>
{
    FO_STACK_TRACE_ENTRY();

    shared_lock lock {_registryLock};

    if (const auto it = _allMaps.find(id); it != _allMaps.end()) {
        return refcount_ptr<const Map>(it->second.get());
    }

    return {};
}

auto EntityManager::GetMap(ident_t id) noexcept -> refcount_ptr<Map>
{
    FO_STACK_TRACE_ENTRY();

    shared_lock lock {_registryLock};

    if (const auto it = _allMaps.find(id); it != _allMaps.end()) {
        return refcount_ptr<Map>(it->second.get());
    }

    return {};
}

auto EntityManager::GetMaps() noexcept -> vector<refcount_ptr<Map>>
{
    FO_STACK_TRACE_ENTRY();

    shared_lock lock {_registryLock};

    vector<refcount_ptr<Map>> result;
    result.reserve(_allMaps.size());

    for (const auto& [id, ptr] : _allMaps) {
        result.emplace_back(ptr.get_no_const());
    }

    return result;
}

auto EntityManager::GetMapsCount() const noexcept -> size_t
{
    FO_STACK_TRACE_ENTRY();

    shared_lock lock {_registryLock};

    return _allMaps.size();
}

auto EntityManager::GetCritter(ident_t id) const noexcept -> refcount_ptr<const Critter>
{
    FO_STACK_TRACE_ENTRY();

    shared_lock lock {_registryLock};

    if (const auto it = _allCritters.find(id); it != _allCritters.end()) {
        return refcount_ptr<const Critter>(it->second.get());
    }

    return {};
}

auto EntityManager::GetCritter(ident_t id) noexcept -> refcount_ptr<Critter>
{
    FO_STACK_TRACE_ENTRY();

    shared_lock lock {_registryLock};

    if (const auto it = _allCritters.find(id); it != _allCritters.end()) {
        return refcount_ptr<Critter>(it->second.get());
    }

    return {};
}

auto EntityManager::GetCritters() noexcept -> vector<refcount_ptr<Critter>>
{
    FO_STACK_TRACE_ENTRY();

    shared_lock lock {_registryLock};

    vector<refcount_ptr<Critter>> result;
    result.reserve(_allCritters.size());

    for (const auto& [id, ptr] : _allCritters) {
        result.emplace_back(ptr.get_no_const());
    }

    return result;
}

auto EntityManager::GetCrittersCount() const noexcept -> size_t
{
    FO_STACK_TRACE_ENTRY();

    shared_lock lock {_registryLock};

    return _allCritters.size();
}

auto EntityManager::GetItem(ident_t id) const noexcept -> refcount_ptr<const Item>
{
    FO_STACK_TRACE_ENTRY();

    shared_lock lock {_registryLock};

    if (const auto it = _allItems.find(id); it != _allItems.end()) {
        return refcount_ptr<const Item>(it->second.get());
    }

    return {};
}

auto EntityManager::GetItem(ident_t id) noexcept -> refcount_ptr<Item>
{
    FO_STACK_TRACE_ENTRY();

    shared_lock lock {_registryLock};

    if (const auto it = _allItems.find(id); it != _allItems.end()) {
        return refcount_ptr<Item>(it->second.get());
    }

    return {};
}

auto EntityManager::GetItems() noexcept -> vector<refcount_ptr<Item>>
{
    FO_STACK_TRACE_ENTRY();

    shared_lock lock {_registryLock};

    vector<refcount_ptr<Item>> result;
    result.reserve(_allItems.size());

    for (const auto& [id, ptr] : _allItems) {
        result.emplace_back(ptr.get_no_const());
    }

    return result;
}

auto EntityManager::GetItemsCount() const noexcept -> size_t
{
    FO_STACK_TRACE_ENTRY();

    shared_lock lock {_registryLock};

    return _allItems.size();
}

// Thread-safety analysis is disabled here: LoadEntities runs single-threaded during server init (before the worker
// pool processes any entity), so it reads the registry maps without _registryLock and iterates them while calling
// back into the engine (CallInit / ProcessVisible*), which itself re-locks the registry — holding the lock across
// that would self-deadlock. See Engine/Docs/ThreadSafetyAnalysis.md.
void EntityManager::LoadEntities() FO_TSA_NO_ANALYSIS
{
    FO_STACK_TRACE_ENTRY();

    WriteLog("Load entities");

    const int64_t last = _engine->GetLastEntityId().underlying_value();
    const int64_t start = _engine->Settings.EntityStartId;
    _lastEntityId = std::max(last, start);
    _persistedEntityId = _lastEntityId;

    bool is_error = false;

    LoadInnerEntities(_engine.get(), is_error);

    const auto loc_ids = _engine->DbStorage.GetAllIntIds(_locationCollectionName);

    for (const auto loc_id : loc_ids) {
        LoadLocation(loc_id, is_error);
    }

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

void EntityManager::FlushExactEntityId()
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock lock {_registryLock};

    _persistedEntityId = _lastEntityId;
    _engine->LockForPropertyAccess();
    auto unlock_prop = scope_exit([this]() noexcept { _engine->UnlockForPropertyAccess(); });
    _engine->SetLastEntityId(ident_t {numeric_cast<int64_t>(_lastEntityId)});
}

auto EntityManager::LoadLocation(ident_t loc_id, bool& is_error) noexcept -> Location*
{
    FO_STACK_TRACE_ENTRY();

    auto&& [loc_doc, loc_pid] = LoadEntityDoc(_locationTypeName, _locationCollectionName, loc_id, true, is_error);

    if (!loc_pid) {
        return nullptr;
    }

    const auto* loc_proto = _engine->GetProtoLocation(loc_pid);

    if (loc_proto == nullptr) {
        WriteLog(LogType::Warning, "Location {} proto {} not found", loc_id, loc_pid);
        is_error = true;
        return nullptr;
    }

    auto loc = SafeAlloc::MakeRefCounted<Location>(_engine.get(), loc_id, loc_proto);

    if (!PropertiesSerializator::LoadFromDocument(&loc->GetPropertiesForEdit(), loc_doc, _engine->Hashes, *_engine)) {
        WriteLog(LogType::Warning, "Failed to restore location {} {} properties", loc_pid, loc_id);
        is_error = true;
        return nullptr;
    }

    try {
        RegisterLocation(loc.get());
        loc->SetPersistent(true);
    }
    catch (const std::exception& ex) {
        WriteLog(LogType::Warning, "Failed to register location {} {}", loc_pid, loc_id);
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
                FO_VERIFY_AND_THROW(map->GetLocId() == loc->GetId(), "Loaded map belongs to a different location");

                const auto loc_map_index = map->GetLocMapIndex();
                auto loc_maps = loc->GetRawMaps();

                if (loc_map_index >= numeric_cast<int32_t>(loc_maps.size())) {
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
        WriteLog(LogType::Warning, "Failed during restore location content {} {}", loc_pid, loc_id);
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
        return nullptr;
    }

    const auto* map_proto = _engine->GetProtoMap(map_pid);

    if (map_proto == nullptr) {
        WriteLog(LogType::Warning, "Map {} proto {} not found", map_id, map_pid);
        is_error = true;
        return nullptr;
    }

    auto* static_map = _engine->MapMngr.GetStaticMap(map_proto);
    auto map = SafeAlloc::MakeRefCounted<Map>(_engine.get(), map_id, map_proto, nullptr, static_map);

    if (!PropertiesSerializator::LoadFromDocument(&map->GetPropertiesForEdit(), map_doc, _engine->Hashes, *_engine)) {
        WriteLog(LogType::Warning, "Failed to restore map {} {} properties", map_pid, map_id);
        is_error = true;
        return nullptr;
    }

    try {
        RegisterMap(map.get());
        map->SetPersistent(true);
    }
    catch (const std::exception& ex) {
        WriteLog(LogType::Warning, "Failed to register map {} {}", map_pid, map_id);
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
                cr->SetMapId(map->GetId());
                FO_VERIFY_AND_THROW(cr->GetMapId() == map->GetId(), "Critter belongs to a different map");

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
                FO_VERIFY_AND_THROW(item->GetOwnership() == ItemOwnership::MapHex, "Item is not placed on map hex");
                FO_VERIFY_AND_THROW(item->GetMapId() == map->GetId(), "Item belongs to a different map");

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
        WriteLog(LogType::Warning, "Failed during restore map content {} {}", map_pid, map_id);
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
        return nullptr;
    }

    const auto* proto = _engine->GetProtoCritter(cr_pid);

    if (proto == nullptr) {
        WriteLog(LogType::Warning, "Critter {} proto {} not found", cr_id, cr_pid);
        is_error = true;
        return nullptr;
    }

    auto cr = SafeAlloc::MakeRefCounted<Critter>(_engine.get(), cr_id, proto);

    if (!PropertiesSerializator::LoadFromDocument(&cr->GetPropertiesForEdit(), cr_doc, _engine->Hashes, *_engine)) {
        WriteLog(LogType::Warning, "Failed to restore critter {} {} properties", cr_pid, cr_id);
        is_error = true;
        return nullptr;
    }

    try {
        cr->SetMapId({});
        RegisterCritter(cr.get());
        cr->SetPersistent(true);
    }
    catch (const std::exception& ex) {
        WriteLog(LogType::Warning, "Failed to register critter {} {}", cr_pid, cr_id);
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
                FO_VERIFY_AND_THROW(inv_item->GetOwnership() == ItemOwnership::CritterInventory, "Loaded critter inventory item has a non-inventory ownership state", inv_item->GetId(), cr->GetId(), inv_item->GetOwnership());
                FO_VERIFY_AND_THROW(inv_item->GetCritterId() == cr->GetId(), "Loaded inventory item belongs to a different critter");

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
        WriteLog(LogType::Warning, "Failed during restore critter content {} {}", cr_pid, cr_id);
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
        return nullptr;
    }

    const auto* proto = _engine->GetProtoItem(item_pid);

    if (proto == nullptr) {
        WriteLog(LogType::Warning, "Item {} proto {} not found", item_id, item_pid);
        is_error = true;
        return nullptr;
    }

    auto item = SafeAlloc::MakeRefCounted<Item>(_engine.get(), item_id, proto);

    if (!PropertiesSerializator::LoadFromDocument(&item->GetPropertiesForEdit(), item_doc, _engine->Hashes, *_engine)) {
        WriteLog(LogType::Warning, "Failed to restore item {} {} properties", item_pid, item_id);
        is_error = true;
        return nullptr;
    }

    try {
        item->SetStatic(false);
        RegisterItem(item.get());
        item->SetPersistent(true);
    }
    catch (const std::exception& ex) {
        WriteLog(LogType::Warning, "Failed to register item {} {}", item_pid, item_id);
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
                FO_VERIFY_AND_THROW(inner_item->GetOwnership() == ItemOwnership::ItemContainer, "Loaded container item has a non-container ownership state", inner_item->GetId(), item->GetId(), inner_item->GetOwnership());
                FO_VERIFY_AND_THROW(inner_item->GetContainerId() == item->GetId(), "Loaded inner item belongs to a different container");

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
        WriteLog(LogType::Warning, "Failed during restore item content {} {}", item_pid, item_id);
        ReportExceptionAndContinue(ex);
        is_error = true;
    }

    return item.get();
}

void EntityManager::LoadInnerEntities(Entity* holder, bool& is_error) noexcept
{
    FO_STACK_TRACE_ENTRY();

    try {
        const auto& holder_type = _engine->GetEntityType(holder->GetTypeName());

        for (const auto& entry : holder_type.HolderEntries | std::views::keys) {
            LoadInnerEntitiesEntry(holder, entry, is_error);
        }
    }
    catch (const std::exception& ex) {
        WriteLog(LogType::Warning, "Failed during restore inner entities for {}", holder->GetTypeName());
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

        const auto& holder_type = _engine->GetEntityType(holder->GetTypeName());
        const auto inner_entity_type_name = holder_type.HolderEntries.at(entry).TargetType;

        for (const auto& id : inner_entity_ids) {
            auto* custom_entity = LoadCustomEntity(inner_entity_type_name, id, is_error);

            if (custom_entity != nullptr) {
                FO_VERIFY_AND_THROW(custom_entity->GetCustomHolderId() == holder_id, "Custom entity belongs to a different holder");

                if (auto* holder_entity = dynamic_cast<ServerEntity*>(holder); holder_entity != nullptr) {
                    custom_entity->SetParent(holder_entity);
                }

                holder->AddInnerEntity(custom_entity->GetCustomHolderEntry(), custom_entity);

                // Propagate holder's lock to loaded custom entity. ServerEntity holders supply
                // their own lock; engine-as-holder supplies the engine's singleton lock.
                if (const auto* holder_entity = dynamic_cast<ServerEntity*>(holder); holder_entity != nullptr) {
                    auto* holder_lock = holder_entity->GetEntityLock();
                    FO_VERIFY_AND_THROW(holder_lock, "Missing required holder lock");
                    custom_entity->SetEntityLock(holder_lock);
                }
                else {
                    FO_VERIFY_AND_THROW(_engine.get() == holder, "Entity holder is not the engine singleton");
                    custom_entity->SetEntityLock(_engine->GetEntityLock());
                }

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
        WriteLog(LogType::Warning, "Failed during restore inner entities for {}", holder->GetTypeName());
        ReportExceptionAndContinue(ex);
        is_error = true;
    }
}

auto EntityManager::LoadEntityDoc(hstring type_name, hstring collection_name, ident_t id, bool expect_proto, bool& is_error) const noexcept -> tuple<AnyData::Document, hstring>
{
    FO_STACK_TRACE_ENTRY();

    try {
        FO_VERIFY_AND_THROW(id.underlying_value() != 0, "Generated entity id is zero");

        auto doc = _engine->DbStorage.Get(collection_name, id);

        if (doc.Empty()) {
            WriteLog(LogType::Warning, "{} document {} not found", collection_name, id);
            is_error = true;
            return {};
        }

        if (!doc.Contains("_Proto")) {
            if (expect_proto) {
                WriteLog(LogType::Warning, "{} '_Proto' section not found in entity {}", collection_name, id);
                is_error = true;
            }

            return {std::move(doc), hstring()};
        }

        const auto& proto_value = doc["_Proto"];

        if (proto_value.Type() != AnyData::ValueType::String) {
            WriteLog(LogType::Warning, "{} '_Proto' section of entity {} is not string type (but {})", collection_name, id, proto_value.Type());
            is_error = true;
            return {};
        }

        const auto& proto_name = proto_value.AsString();

        if (proto_name.empty()) {
            WriteLog(LogType::Warning, "{} '_Proto' section of entity {} is empty", collection_name, id);
            is_error = true;
            return {};
        }

        auto proto_id = _engine->Hashes.ToHashedString(proto_name);

        // A proto whose migration rule resolves to the "Remove" sentinel was deleted on purpose: skip
        // the entity cleanly (without is_error) so callers drop it instead of failing the whole load.
        // A genuinely missing proto (no rule) keeps proto_id and surfaces later as proto-not-found.
        if (const auto migrated = _engine->CheckMigrationRule(_protoMigrationRuleName, type_name, proto_id); migrated.has_value() && migrated.value() == _removeMigrationReplacement) {
            WriteLog(LogType::Info, "{} {} dropped: proto {} removed by migration rule", collection_name, id, proto_id);
            return {};
        }

        return {std::move(doc), proto_id};
    }
    catch (const std::exception& ex) {
        WriteLog(LogType::Warning, "Failed during load document {} {}", collection_name, id);
        ReportExceptionAndContinue(ex);
        is_error = true;
        return {};
    }
}

void EntityManager::CallInit(Location* loc, bool first_time)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!loc->IsDestroyed(), "Location is already destroyed");
    ValidateEntityAccess(loc);

    if (loc->IsInitCalled()) {
        return;
    }

    refcount_ptr loc_holder = loc;

    loc->SetInitCalled();

    _engine->OnLocationInit.Fire(loc, first_time);

    if (!loc->IsDestroyed()) {
        ScriptHelpers::CallInitScript(_engine.get(), loc, loc->GetInitScript(), first_time);
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
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Map is already destroyed");
    ValidateEntityAccess(map);

    if (map->IsInitCalled()) {
        return;
    }

    refcount_ptr map_holder = map;

    map->SetInitCalled();

    _engine->OnMapInit.Fire(map, first_time);

    if (!map->IsDestroyed()) {
        ScriptHelpers::CallInitScript(_engine.get(), map, map->GetInitScript(), first_time);
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

    FO_VERIFY_AND_THROW(!cr->IsDestroyed(), "Critter is already destroyed");
    ValidateEntityAccess(cr);

    if (cr->IsInitCalled()) {
        return;
    }

    refcount_ptr cr_holder = cr;

    cr->SetInitCalled();

    _engine->OnCritterInit.Fire(cr, first_time);

    if (!cr->IsDestroyed()) {
        ScriptHelpers::CallInitScript(_engine.get(), cr, cr->GetInitScript(), first_time);
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

    FO_VERIFY_AND_THROW(!item->IsDestroyed(), "Item is already destroyed");
    ValidateEntityAccess(item);

    if (item->IsInitCalled()) {
        return;
    }

    refcount_ptr item_holder = item;

    item->SetInitCalled();

    _engine->OnItemInit.Fire(item, first_time);

    if (!item->IsDestroyed()) {
        ScriptHelpers::CallInitScript(_engine.get(), item, item->GetInitScript(), first_time);
    }

    if (!item->IsDestroyed() && item->HasInnerItems()) {
        for (auto* inner_item : copy_hold_ref(item->GetAllInnerItems())) {
            if (!inner_item->IsDestroyed()) {
                CallInit(inner_item, first_time);
            }
        }
    }
}

void EntityManager::RegisterPlayer(Player* player, ident_t id, bool persistent)
{
    FO_STACK_TRACE_ENTRY();

    if (id) {
        player->SetId(id);
    }

    scoped_lock lock {_registryLock};

    RegisterEntity(player);
    player->SetPersistent(persistent);
    const auto [it, inserted] = _allPlayers.emplace(player->GetId(), player);
    FO_STRONG_ASSERT(inserted, "Player id is already registered", player->GetId());
}

void EntityManager::UnregisterPlayer(Player* player)
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock lock {_registryLock};

    const auto it = _allPlayers.find(player->GetId());
    FO_STRONG_ASSERT(it != _allPlayers.end(), "Lookup failed in all players");
    _allPlayers.erase(it);
    UnregisterEntity(player, false);
}

void EntityManager::RegisterLocation(Location* loc)
{
    FO_STACK_TRACE_ENTRY();

    {
        scoped_lock lock {_registryLock};

        RegisterEntity(loc);
        const auto [it, inserted] = _allLocations.emplace(loc->GetId(), loc);
        FO_STRONG_ASSERT(inserted, "Location id is already registered", loc->GetId(), loc->GetProtoId());
    }

    // Engine-wide invariant: Register* is reached only from a thread that already has a
    // SyncContext active (worker-pool jobs, _starter / _mainWorker jobs wrapped via
    // WrapJobWithSync, or the Shutdown setup ctx). Any caller without one is a bug.
    auto* ctx = _engine->GetCurrentSyncContext();
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");
    ctx->EnsureEntitySynced(loc);
}

void EntityManager::UnregisterLocation(Location* loc)
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock lock {_registryLock};

    const auto it = _allLocations.find(loc->GetId());
    FO_STRONG_ASSERT(it != _allLocations.end(), "Lookup failed in all locations");
    _allLocations.erase(it);
    UnregisterEntity(loc, true);
}

void EntityManager::RegisterMap(Map* map)
{
    FO_STACK_TRACE_ENTRY();

    {
        scoped_lock lock {_registryLock};

        RegisterEntity(map);
        const auto [it, inserted] = _allMaps.emplace(map->GetId(), map);
        FO_STRONG_ASSERT(inserted, "Map id is already registered", map->GetId(), map->GetProtoId());
    }

    // See RegisterLocation: SyncContext invariant.
    auto* ctx = _engine->GetCurrentSyncContext();
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");
    ctx->EnsureEntitySynced(map);
}

void EntityManager::UnregisterMap(Map* map)
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock lock {_registryLock};

    const auto it = _allMaps.find(map->GetId());
    FO_STRONG_ASSERT(it != _allMaps.end(), "Lookup failed in all maps");
    _allMaps.erase(it);
    UnregisterEntity(map, true);
}

void EntityManager::RegisterCritter(Critter* cr)
{
    FO_STACK_TRACE_ENTRY();

    {
        scoped_lock lock {_registryLock};

        RegisterEntity(cr);
        const auto [it, inserted] = _allCritters.emplace(cr->GetId(), cr);
        FO_STRONG_ASSERT(inserted, "Critter id is already registered", cr->GetId(), cr->GetProtoId());
    }

    // See RegisterLocation: SyncContext invariant.
    auto* ctx = _engine->GetCurrentSyncContext();
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");
    ctx->EnsureEntitySynced(cr);
}

void EntityManager::UnregisterCritter(Critter* cr)
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock lock {_registryLock};

    const auto it = _allCritters.find(cr->GetId());
    FO_STRONG_ASSERT(it != _allCritters.end(), "Lookup failed in all critters");
    _allCritters.erase(it);
    UnregisterEntity(cr, !cr->GetControlledByPlayer());
}

void EntityManager::RegisterItem(Item* item)
{
    FO_STACK_TRACE_ENTRY();

    {
        scoped_lock lock {_registryLock};

        RegisterEntity(item);
        const auto [it, inserted] = _allItems.emplace(item->GetId(), item);
        FO_STRONG_ASSERT(inserted, "Item id is already registered", item->GetId(), item->GetProtoId());
    }

    // See RegisterLocation: SyncContext invariant.
    auto* ctx = _engine->GetCurrentSyncContext();
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");
    ctx->EnsureEntitySynced(item);
}

void EntityManager::UnregisterItem(Item* item, bool delete_from_db)
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock lock {_registryLock};

    const auto it = _allItems.find(item->GetId());
    FO_STRONG_ASSERT(it != _allItems.end(), "Lookup failed in all items");
    _allItems.erase(it);
    UnregisterEntity(item, delete_from_db);
}

void EntityManager::RegisterCustomEntity(CustomEntity* custom_entity)
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock lock {_registryLock};

    RegisterEntity(custom_entity);
    auto& custom_entities = _allCustomEntities[custom_entity->GetTypeName()];
    const auto [it, inserted] = custom_entities.emplace(custom_entity->GetId(), custom_entity);
    FO_STRONG_ASSERT(inserted, "Custom entity id is already registered", custom_entity->GetTypeName(), custom_entity->GetId());
}

void EntityManager::UnregisterCustomEntity(CustomEntity* custom_entity, bool delete_from_db)
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock lock {_registryLock};

    auto& custom_entities = _allCustomEntities[custom_entity->GetTypeName()];
    const auto it = custom_entities.find(custom_entity->GetId());
    FO_STRONG_ASSERT(it != custom_entities.end(), "Lookup failed in custom entities");
    custom_entities.erase(it);
    UnregisterEntity(custom_entity, delete_from_db);
}

void EntityManager::MakePersistent(ServerEntity* entity, bool persistent, bool explicitly_requested)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(entity, "Missing entity instance");
    FO_VERIFY_AND_THROW(entity->GetId(), "Entity has no assigned id");

    if (const auto* player = dynamic_cast<const Player*>(entity); player != nullptr) {
        throw EntityManagerException("Can't change persistence of player", player->GetId());
    }

    if (persistent) {
        if (explicitly_requested) {
            entity->SetExplicitlyPersistent(true);
        }

        unordered_set<ServerEntity*> processed;
        MakePersistentRecursive(entity, processed);
    }
    else {
        if (explicitly_requested) {
            entity->SetExplicitlyPersistent(false);
        }

        unordered_set<ServerEntity*> processed;
        MakeNonPersistentRecursive(entity, processed);
    }
}

void EntityManager::MakePersistentRecursive(ServerEntity* entity, unordered_set<ServerEntity*>& processed)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(entity, "Missing entity instance");

    if (processed.contains(entity)) {
        return;
    }

    processed.emplace(entity);

    EnsureCascadeNodeSynced(entity);

    if (!entity->IsPersistent()) {
        WriteLog("Store entity {} {} in database", entity->GetTypeName(), entity->GetId());
        _engine->DbStorage.Insert(entity->GetTypeNamePlural(), entity->GetId(), StoreEntityDoc(entity));
        entity->SetPersistent(true);
    }

    ForEachPersistentChildEntity(entity, [this, &processed](ServerEntity* child) { MakePersistentRecursive(child, processed); });
}

void EntityManager::MakeNonPersistentRecursive(ServerEntity* entity, unordered_set<ServerEntity*>& processed)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(entity, "Missing entity instance");

    if (processed.contains(entity)) {
        return;
    }

    processed.emplace(entity);

    EnsureCascadeNodeSynced(entity);

    ForEachPersistentChildEntity(entity, [this, &processed](ServerEntity* child) { MakeNonPersistentRecursive(child, processed); });

    if (entity->IsPersistent()) {
        WriteLog("Remove entity {} {} from database", entity->GetTypeName(), entity->GetId());
        _engine->DbStorage.Delete(entity->GetTypeNamePlural(), entity->GetId());
        entity->SetPersistent(false);
    }
}

void EntityManager::EnsureCascadeNodeSynced(ServerEntity* entity)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(entity, "Missing entity instance");

    // The (de)persist cascade walks an owned subtree (every node is reachable from a root the caller
    // already covers), so pulling each node's own lock into the current sync context is safe — and
    // necessary during a destroy cascade. There, a node is detached from its parent BEFORE being
    // de-persisted (e.g. CritterManager::RemoveItemFromCritter clears the critter id, then calls
    // MakePersistent(false) on the now-orphaned item), which severs the ancestor cover the caller
    // relied on and would otherwise trip ForEachPersistentChildEntity's access validation. This
    // mirrors the inner-item EnsureEntitySynced already used in ItemManager::DestroyItem.
    if (auto* ctx = _engine->GetCurrentSyncContext(); ctx != nullptr) {
        ctx->EnsureEntitySynced(entity);
    }
}

void EntityManager::ForEachPersistentChildEntity(ServerEntity* entity, const function<void(ServerEntity* child)>& callback) const
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(entity, "Missing entity instance");
    ValidateEntityAccess(entity);

    if (auto* loc = dynamic_cast<Location*>(entity); loc != nullptr) {
        for (auto* map : copy_hold_ref(loc->GetMaps())) {
            callback(map);
        }
    }
    else if (auto* map = dynamic_cast<Map*>(entity); map != nullptr) {
        for (auto* cr : copy_hold_ref(map->GetCritters())) {
            callback(cr);
        }

        for (auto* item : copy_hold_ref(map->GetItems())) {
            callback(item);
        }
    }
    else if (auto* cr = dynamic_cast<Critter*>(entity); cr != nullptr) {
        for (auto* item : copy_hold_ref(cr->GetInvItems())) {
            callback(item);
        }
    }
    else if (auto* item = dynamic_cast<Item*>(entity); item != nullptr && item->HasInnerItems()) {
        for (auto* inner_item : copy_hold_ref(item->GetAllInnerItems())) {
            callback(inner_item);
        }
    }

    if (entity->HasInnerEntities()) {
        const auto& entity_type = _engine->GetEntityType(entity->GetTypeName());

        for (auto& [entry, inner_entities] : entity->GetInnerEntities()) {
            FO_VERIFY_AND_THROW(entity_type.HolderEntries.count(entry), "Entity has an inner-entity holder entry that is not registered for its type", entity->GetTypeName(), entity->GetId(), entry, entity_type.HolderEntries.size());

            if (!entity_type.HolderEntries.at(entry).Persistent) {
                continue;
            }

            for (auto& inner_entity : inner_entities) {
                auto* server_entity = dynamic_cast<ServerEntity*>(inner_entity.get());
                FO_VERIFY_AND_THROW(server_entity, "Missing server entity instance");
                callback(server_entity);
            }
        }
    }
}

auto EntityManager::StoreEntityDoc(ServerEntity* entity) -> AnyData::Document
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(entity, "Missing entity instance");

    if (const auto* entity_with_proto = dynamic_cast<EntityWithProto*>(entity); entity_with_proto != nullptr) {
        const auto* proto = entity_with_proto->GetProto();
        auto doc = PropertiesSerializator::SaveToDocument(&entity->GetProperties(), &proto->GetProperties(), _engine->Hashes, *_engine);
        doc.Emplace("_Proto", string(proto->GetName()));
        return doc;
    }
    else {
        auto doc = PropertiesSerializator::SaveToDocument(&entity->GetProperties(), nullptr, _engine->Hashes, *_engine);
        return doc;
    }
}

void EntityManager::RegisterEntity(ServerEntity* entity)
{
    FO_STACK_TRACE_ENTRY();

    // Caller must hold _registryLock (unique).
    if (!entity->GetId()) {
        const int64_t id_num = ++_lastEntityId;
        const ident_t id {numeric_cast<int64_t>(id_num)};
        FO_STRONG_ASSERT(_allEntities.count(id) == 0, "Generated entity id is already present in the entity registry", entity->GetTypeName(), id);

        if (id_num > _persistedEntityId) {
            _persistedEntityId = id_num + _engine->Settings.EntityIdReserveBatch - 1;

            _engine->LockForPropertyAccess();
            auto unlock_prop = scope_exit([this]() noexcept { _engine->UnlockForPropertyAccess(); });
            _engine->SetLastEntityId(ident_t {numeric_cast<int64_t>(_persistedEntityId)});
        }

        entity->SetId(id);
    }
    else {
        const int64_t id_num = entity->GetId().underlying_value();
        _lastEntityId = std::max(_lastEntityId, id_num);
    }

    const auto [it, inserted] = _allEntities.emplace(entity->GetId(), entity);
    FO_VERIFY_AND_THROW(inserted, "Entity id is already registered in the global entity registry", entity->GetTypeName(), entity->GetId());
}

void EntityManager::UnregisterEntity(ServerEntity* entity, bool delete_from_db)
{
    FO_STACK_TRACE_ENTRY();

    // Caller must hold _registryLock (unique) for the erase portion.
    const auto entity_id = entity->GetId();
    const auto type_name_plural = entity->GetTypeNamePlural();
    const bool is_persistent = entity->IsPersistent();
    FO_VERIFY_AND_THROW(entity_id, "Missing required entity id");

    const auto it = _allEntities.find(entity_id);
    FO_STRONG_ASSERT(it != _allEntities.end(), "Lookup failed in all entities");
    _allEntities.erase(it); // This may be the last ptr to the entity, so it may be destroyed here.

    if (delete_from_db && is_persistent) {
        _engine->DbStorage.Delete(type_name_plural, entity_id);
    }
}

void EntityManager::DestroyEntity(Entity* entity)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(entity, "Missing entity instance");

    if (auto* loc = dynamic_cast<Location*>(entity); loc != nullptr) {
        _engine->MapMngr.DestroyLocation(loc);
    }
    else if (auto* cr = dynamic_cast<Critter*>(entity); cr != nullptr) {
        if (cr->GetControlledByPlayer()) {
            throw ScriptException("Cannot destroy a player-controlled critter; detach the player first", cr->GetId());
        }

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

    for (size_t prev_deps = std::numeric_limits<size_t>::max(); holder->HasInnerEntities();) {
        for (auto&& [entry, entities] : copy(holder->GetInnerEntities())) {
            for (auto& entity : entities) {
                DestroyEntity(entity.get());
            }
        }

        // Each pass must strictly reduce the holder's remaining inner entities; non-convergence is corruption.
        const size_t remaining_deps = holder->GetInnerEntitiesCount();
        FO_STRONG_ASSERT(remaining_deps < prev_deps, "Inner-entity destruction made no progress", holder->GetId(), remaining_deps, prev_deps);
        prev_deps = remaining_deps;
    }
}

void EntityManager::DestroyAllEntities() FO_TSA_NO_ANALYSIS
{
    FO_STACK_TRACE_ENTRY();

    const auto destroy_entities = [this](auto& entities) FO_TSA_NO_ANALYSIS {
        for (auto&& [id, entity] : copy(entities)) {
            entity->SetParent(nullptr);
            entity->MarkAsDestroyed();
            entities.erase(id);
            _allEntities.erase(id);
        }

        FO_VERIFY_AND_THROW(entities.empty(), "Entities must be empty before this operation");
    };

    destroy_entities(_allPlayers);
    destroy_entities(_allLocations);
    destroy_entities(_allMaps);
    destroy_entities(_allCritters);
    destroy_entities(_allItems);

    for (auto& val : _allCustomEntities | std::views::values) {
        destroy_entities(val);
    }

    FO_VERIFY_AND_THROW(_allEntities.empty(), "All entities must be empty before this operation");
}

auto EntityManager::CreateCustomInnerEntity(Entity* holder, hstring entry, hstring pid) -> CustomEntity*
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(holder, "Missing required holder");
    FO_VERIFY_AND_THROW(_engine->GetEntityType(holder->GetTypeName()).HolderEntries.count(entry), "Holder entity type has no custom inner entry with the requested name", holder->GetTypeName(), holder->GetId(), entry);

    const hstring type_name = _engine->GetEntityType(holder->GetTypeName()).HolderEntries.at(entry).TargetType;

    auto* entity = CreateCustomEntity(type_name, pid);
    FO_VERIFY_AND_THROW(entity, "Missing entity instance");

    if (auto* holder_with_id = dynamic_cast<ServerEntity*>(holder); holder_with_id != nullptr) {
        FO_VERIFY_AND_THROW(holder_with_id->GetId(), "Entity holder has no assigned id");
        entity->SetCustomHolderId(holder_with_id->GetId());
        entity->SetParent(holder_with_id);
    }
    else {
        FO_VERIFY_AND_THROW(_engine.get() == holder, "Entity holder is not the engine singleton");
        // Holder is the global engine singleton; no ServerEntity parent to track. The engine
        // isn't a ServerEntity, so SetParent is skipped, but the engine still owns its own
        // EntityLock (the same one Game.Lock()/Unlock() use). We propagate that lock directly
        // into _entityLock below so the validator's parent-chain walk finds a real cover on
        // the first iteration (against the entity itself, not via parent).
    }

    entity->SetCustomHolderEntry(entry);
    holder->AddInnerEntity(entry, entity);

    const auto* holder_prop = _engine->GetEntityHolderIdsProp(holder, entry);
    auto& holder_props = holder->GetPropertiesForEdit();
    auto inner_entity_ids = holder_props.GetValueFast<vector<ident_t>>(holder_prop);
    vec_add_unique_value(inner_entity_ids, entity->GetId());
    holder_props.SetValue(holder_prop, inner_entity_ids);

    if (const auto* holder_with_id = dynamic_cast<ServerEntity*>(holder); holder_with_id != nullptr) {
        const auto& holder_type = _engine->GetEntityType(holder->GetTypeName());

        if (holder_type.HolderEntries.at(entry).Persistent && holder_with_id->IsPersistent()) {
            MakePersistent(entity, true);
        }
    }

    // Propagate holder's lock to custom entity. ServerEntity holders supply their own lock;
    // engine-as-holder supplies the engine's singleton lock (same one Game.Lock() takes).
    if (const auto* holder_entity = dynamic_cast<ServerEntity*>(holder); holder_entity != nullptr) {
        auto* holder_lock = holder_entity->GetEntityLock();
        FO_VERIFY_AND_THROW(holder_lock, "Missing required holder lock");
        entity->SetEntityLock(holder_lock);
    }
    else {
        FO_VERIFY_AND_THROW(_engine.get() == holder, "Entity holder is not the engine singleton");
        entity->SetEntityLock(_engine->GetEntityLock());
    }

    ForEachCustomEntityView(entity, [entity](Player* player, bool owner) { player->Send_AddCustomEntity(entity, owner); });
    return entity;
}

auto EntityManager::CreateCustomEntity(hstring type_name, hstring pid) -> CustomEntity*
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_engine->IsValidEntityType(type_name), "Invalid entity type name");
    FO_VERIFY_AND_THROW(!_engine->GetEntityType(type_name).Exported, "Exported entity type cannot be created as custom entity");

    const bool has_protos = _engine->GetEntityType(type_name).HasProtos;
    const ProtoEntity* proto = nullptr;

    if (pid) {
        FO_VERIFY_AND_THROW(has_protos, "Missing required has protos");
        proto = _engine->GetProtoEntity(type_name, pid);
        FO_VERIFY_AND_THROW(proto, "Missing prototype instance");
    }
    else {
        FO_VERIFY_AND_THROW(!has_protos, "Has protos is already set");
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
        FO_VERIFY_AND_THROW(id.underlying_value() != 0, "Generated entity id is zero");

        if (!_engine->IsValidEntityType(type_name)) {
            WriteLog(LogType::Warning, "Custom entity {} type {} not valid", id, type_name);
            is_error = true;
            return nullptr;
        }

        {
            shared_lock lock {_registryLock};

            const auto type_it = _allCustomEntities.find(type_name);
            FO_VERIFY_AND_THROW(type_it == _allCustomEntities.end() || type_it->second.count(id) == 0, "Custom entity id is already registered for this type while loading from storage", type_name, id);
        }

        const auto collection_name = _engine->Hashes.ToHashedString(strex("{}s", type_name));
        auto&& [doc, pid] = LoadEntityDoc(type_name, collection_name, id, false, is_error);

        if (doc.Empty()) {
            WriteLog(LogType::Warning, "Custom entity {} with type {} invalid document", id, type_name);
            is_error = true;
            return nullptr;
        }

        const bool has_protos = _engine->GetEntityType(type_name).HasProtos;
        const ProtoEntity* proto = nullptr;

        if (has_protos) {
            if (pid) {
                proto = _engine->GetProtoEntity(type_name, pid);
            }
            else {
                proto = _engine->GetProtoEntity(type_name, _engine->Hashes.ToHashedString("Default"));
            }

            if (proto == nullptr) {
                WriteLog(LogType::Warning, "Proto {} for custom entity {} with type {} not found", pid, id, type_name);
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
            WriteLog(LogType::Warning, "Failed to load properties for custom entity {} with type {}", id, type_name);
            is_error = true;
            return nullptr;
        }

        RegisterCustomEntity(entity.get());
        entity->SetPersistent(true);
        return entity.get();
    }
    catch (const std::exception& ex) {
        WriteLog(LogType::Warning, "Failed during load custom entity {}", id);
        ReportExceptionAndContinue(ex);
        is_error = true;
        return nullptr;
    }
}

auto EntityManager::GetCustomEntity(hstring type_name, ident_t id) -> refcount_ptr<CustomEntity>
{
    FO_STACK_TRACE_ENTRY();

    shared_lock lock {_registryLock};

    const auto type_it = _allCustomEntities.find(type_name);

    if (type_it == _allCustomEntities.end()) {
        return {};
    }

    const auto it = type_it->second.find(id);

    if (it != type_it->second.end()) {
        return refcount_ptr<CustomEntity>(it->second.get());
    }

    return {};
}

void EntityManager::DestroyCustomEntity(CustomEntity* entity)
{
    FO_STACK_TRACE_ENTRY();

    ValidateEntityAccess(entity);

    if (entity->IsDestroying() || entity->IsDestroyed()) {
        return;
    }

    entity->MarkAsDestroying();

    for (size_t prev_deps = std::numeric_limits<size_t>::max(); entity->HasInnerEntities();) {
        DestroyInnerEntities(entity);

        // Each pass must strictly reduce the entity's remaining inner entities; non-convergence is corruption.
        const size_t remaining_deps = entity->GetInnerEntitiesCount();
        FO_STRONG_ASSERT(remaining_deps < prev_deps, "Custom-entity destruction made no progress", entity->GetId(), remaining_deps, prev_deps);
        prev_deps = remaining_deps;
    }

    refcount_ptr<Entity> holder;

    if (const auto id = entity->GetCustomHolderId()) {
        auto parent = entity->GetParent();
        FO_STRONG_ASSERT(parent, "Missing required parent");
        FO_STRONG_ASSERT(parent->GetId() == id, "Child entity is linked to a different parent");
        ValidateEntityAccess(parent.get());
        holder = parent;
    }
    else {
        holder = _engine.get();
    }

    ForEachCustomEntityView(entity, [entity](Player* player, bool owner) {
        ignore_unused(owner);
        safe_call([&] { player->Send_RemoveCustomEntity(entity->GetId()); });
    });

    const auto entry = entity->GetCustomHolderEntry();
    holder->RemoveInnerEntity(entry, entity);

    const auto* holder_prop = _engine->GetEntityHolderIdsProp(holder.get(), entry);
    auto& holder_props = holder->GetPropertiesForEdit();
    auto inner_entity_ids = holder_props.GetValueFast<vector<ident_t>>(holder_prop);
    vec_remove_unique_value(inner_entity_ids, entity->GetId());
    holder_props.SetValue(holder_prop, inner_entity_ids);

    _engine->TimeEventMngr.CancelAllForEntity(entity);
    entity->SetParent(nullptr);
    entity->MarkAsDestroyed();
    UnregisterCustomEntity(entity, true);
}

void EntityManager::ForEachCustomEntityView(CustomEntity* entity, const function<void(Player* player, bool owner)>& callback)
{
    FO_STACK_TRACE_ENTRY();

    ValidateEntityAccess(entity);

    const auto view_callback = [&](Player* player, bool owner) {
        if (player != nullptr) {
            callback(player, owner);
        }
    };

    function<void(Entity*, EntityHolderEntrySync)> find_players_recursively;

    find_players_recursively = [this, &find_players_recursively, &view_callback](Entity* holder, EntityHolderEntrySync derived_sync) {
        if (auto* custom_entity = dynamic_cast<CustomEntity*>(holder); custom_entity != nullptr) {
            auto custom_entity_holder = custom_entity->GetParent();

            if (custom_entity_holder) {
                FO_VERIFY_AND_THROW(custom_entity_holder->GetId() == custom_entity->GetCustomHolderId(), "Custom entity holder id does not match custom entity owner");
                ValidateEntityAccess(custom_entity_holder.get());

                const auto custom_entity_holder_type = _engine->GetEntityType(custom_entity_holder->GetTypeName());
                const auto entry = custom_entity->GetCustomHolderEntry();
                const auto entry_sync = custom_entity_holder_type.HolderEntries.at(entry).Sync;

                if (entry_sync == EntityHolderEntrySync::OwnerSync || entry_sync == EntityHolderEntrySync::PublicSync) {
                    find_players_recursively(custom_entity_holder.get(), std::min(entry_sync, derived_sync));
                }
            }
        }
        else if (auto* cr = dynamic_cast<Critter*>(holder)) {
            view_callback(cr->GetPlayer(), true);

            if (derived_sync == EntityHolderEntrySync::PublicSync) {
                for (auto* another_cr : cr->GetCritters(CritterSeeType::WhoSeeMe, CritterFindType::Any)) {
                    view_callback(another_cr->GetPlayer(), false);
                }
            }
        }
        else if (auto* player = dynamic_cast<Player*>(holder)) {
            view_callback(player, true);
        }
        else if (const auto* game = dynamic_cast<ServerEngine*>(holder); game != nullptr) {
            for (auto& game_player : GetPlayers()) {
                view_callback(game_player.get(), false);
            }
        }
        else if (auto* loc = dynamic_cast<Location*>(holder); loc != nullptr) {
            for (auto* loc_map : loc->GetMaps()) {
                find_players_recursively(loc_map, derived_sync);
            }
        }
        else if (auto* map = dynamic_cast<Map*>(holder)) {
            for (auto& map_cr : map->GetPlayerCritters()) {
                view_callback(map_cr->GetPlayer(), false);
            }
        }
        else if (auto* item = dynamic_cast<Item*>(holder)) {
            switch (item->GetOwnership()) {
            case ItemOwnership::CritterInventory: {
                auto item_cr = item->GetParent<Critter>();
                FO_VERIFY_AND_THROW(item_cr, "Missing required item critter");
                FO_VERIFY_AND_THROW(item_cr->GetId() == item->GetCritterId(), "Inventory item critter owner id mismatch");
                ValidateEntityAccess(item_cr.get());
                find_players_recursively(item_cr.get(), derived_sync);
            } break;
            case ItemOwnership::MapHex: {
                if (derived_sync == EntityHolderEntrySync::PublicSync) {
                    auto item_map = item->GetParent<Map>();
                    FO_VERIFY_AND_THROW(item_map, "Missing required item map");
                    FO_VERIFY_AND_THROW(item_map->GetId() == item->GetMapId(), "Map item owner id mismatch");
                    ValidateEntityAccess(item_map.get());

                    for (auto& map_cr : item_map->GetPlayerCritters()) {
                        if (map_cr->IsSeeItem(item->GetId())) {
                            view_callback(map_cr->GetPlayer(), false);
                        }
                    }
                }
            } break;
            case ItemOwnership::ItemContainer: {
                auto item_cont = item->GetParent<Item>();
                FO_VERIFY_AND_THROW(item_cont, "Missing required item cont");
                FO_VERIFY_AND_THROW(item_cont->GetId() == item->GetContainerId(), "Container item owner id mismatch");
                ValidateEntityAccess(item_cont.get());
                find_players_recursively(item_cont.get(), derived_sync);
            } break;
            default:
                break;
            }
        }
    };

    find_players_recursively(entity, EntityHolderEntrySync::PublicSync);
}

FO_END_NAMESPACE
