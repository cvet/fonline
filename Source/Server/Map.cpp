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

#include "Map.h"
#include "Critter.h"
#include "CritterManager.h"
#include "Item.h"
#include "Location.h"
#include "Player.h"
#include "Server.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

Map::Map(ptr<ServerEngine> engine, ident_t id, ptr<const ProtoMap> proto, nptr<Location> location, ptr<StaticMap> static_map, nptr<const Properties> props) noexcept :
    ServerEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_TYPE_NAME).as_ptr(), props ? props : nptr<const Properties> {&proto->GetProperties()}, &proto->GetProperties()),
    EntityWithProto(proto),
    MapProperties(*GetInitRef()),
    _protoMap {proto},
    _staticMap {static_map},
    _mapSize {GetSize()},
    _hexField {CreateHexField(_mapSize, engine->Settings->MapInstanceStaticGrid)},
    _mapLocation {location}
{
    FO_STACK_TRACE_ENTRY();

    FO_NO_VALIDATE_ENTITY_ACCESS();

    SetEntityLock(&_ownedLock);
}

Map::~Map()
{
    FO_STACK_TRACE_ENTRY();

    FO_NO_VALIDATE_ENTITY_ACCESS();

    if (!_engine->IsShutdownInProgress()) {
        FO_VERIFY_AND_CONTINUE(_spectatorPlayers.empty(), "Server map has spectator players during destruction", GetId(), _spectatorPlayers.size());
        FO_VERIFY_AND_CONTINUE(_critters.empty(), "Server map has critters during destruction", GetId(), _critters.size());
        FO_VERIFY_AND_CONTINUE(_crittersMap.empty(), "Server map has critter map entries during destruction", GetId(), _crittersMap.size());
        FO_VERIFY_AND_CONTINUE(_playerCritters.empty(), "Server map has player critters during destruction", GetId(), _playerCritters.size());
        FO_VERIFY_AND_CONTINUE(_nonPlayerCritters.empty(), "Server map has non-player critters during destruction", GetId(), _nonPlayerCritters.size());
        FO_VERIFY_AND_CONTINUE(_items.empty(), "Server map has items during destruction", GetId(), _items.size());
        FO_VERIFY_AND_CONTINUE(_itemsMap.empty(), "Server map has item map entries during destruction", GetId(), _itemsMap.size());
    }
}

auto Map::CreateHexField(msize map_size, bool static_grid) -> unique_ptr<TwoDimensionalGrid<Field, mpos, msize>>
{
    FO_STACK_TRACE_ENTRY();

    if (static_grid) {
        return SafeAlloc::MakeUnique<StaticTwoDimensionalGrid<Field, mpos, msize>>(map_size);
    }

    return SafeAlloc::MakeUnique<DynamicTwoDimensionalGrid<Field, mpos, msize>>(map_size);
}

void Map::AddSpectatorPlayer(ptr<Player> player)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();

    scoped_lock lock {_spectatorLock};

    vec_add_unique_value(_spectatorPlayers, player);
}

void Map::RemoveSpectatorPlayer(ptr<Player> player)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();

    scoped_lock lock {_spectatorLock};

    vec_remove_unique_value(_spectatorPlayers, player);
}

auto Map::GetSpectatorPlayersForSend() -> vector<refcount_ptr<Player>>
{
    FO_STACK_TRACE_ENTRY();

    FO_NO_VALIDATE_ENTITY_ACCESS();

    shared_lock lock {_spectatorLock};

    vector<refcount_ptr<Player>> recipients;
    recipients.reserve(_spectatorPlayers.size());

    for (ptr<Player> player : _spectatorPlayers) {
        recipients.emplace_back(player.hold_ref());
    }

    return recipients;
}

void Map::SetLocation(nptr<Location> loc) noexcept
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    _mapLocation = loc;
    SetParent(loc);
}

auto Map::FindStartHex(mpos hex, int32_t multihex, int32_t seek_radius, bool skip_unsafe) const -> optional<mpos>
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();

    if (IsHexesMovable(hex, multihex) && !(skip_unsafe && (IsTriggerStaticItemOnHex(hex) || IsTriggerItemOnHex(hex)))) {
        return hex;
    }
    if (seek_radius == 0) {
        return std::nullopt;
    }

    const auto hexes_around = GeometryHelper::HexesInRadius(seek_radius);
    int32_t pos = GetEngine()->Random(1, hexes_around);
    int32_t iterations = 0;

    while (true) {
        if (++iterations > hexes_around) {
            return std::nullopt;
        }
        if (++pos > hexes_around) {
            pos = 1;
        }

        auto check_hex = hex;

        if (!GeometryHelper::MoveHexAroundAway(check_hex, pos, _mapSize)) {
            continue;
        }
        if (!IsHexesMovable(check_hex, multihex)) {
            continue;
        }
        if (skip_unsafe && (IsTriggerStaticItemOnHex(check_hex) || IsTriggerItemOnHex(check_hex))) {
            continue;
        }

        break;
    }

    auto result_hex = hex;
    GeometryHelper::MoveHexAroundAway(result_hex, pos, _mapSize);
    return result_hex;
}

void Map::AddCritter(ptr<Critter> cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    FO_VERIFY_AND_THROW(!IsDestroyed(), "Cannot add a critter to an already destroyed map", GetId());
    FO_VERIFY_AND_THROW(!IsDestroying(), "Cannot add a critter to a map that is being destroyed", GetId());
    FO_VERIFY_AND_THROW(!_crittersMap.count(cr->GetId()), "Server map already contains a critter with the same id", GetId(), cr->GetId());

    _crittersMap.emplace(cr->GetId(), cr);
    vec_add_unique_value(_critters, cr);

    if (cr->GetControlledByPlayer()) {
        vec_add_unique_value(_playerCritters, cr);
    }
    else {
        vec_add_unique_value(_nonPlayerCritters, cr);
    }

    ptr<Map> map = this;
    cr->SetParent(map);

    AddCritterToField(cr);

    if (IsPersistent()) {
        _engine->EntityMngr.MakePersistent(cr, true);
    }
}

void Map::RemoveCritter(ptr<Critter> cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    const auto it = _crittersMap.find(cr->GetId());
    FO_VERIFY_AND_THROW(it != _crittersMap.end(), "Lookup failed in critters map");
    _crittersMap.erase(it);

    vec_remove_unique_value(_critters, cr);

    if (cr->GetControlledByPlayer()) {
        vec_remove_unique_value(_playerCritters, cr);
    }
    else {
        vec_remove_unique_value(_nonPlayerCritters, cr);
    }

    cr->SetParent(nullptr);

    RemoveCritterFromField(cr);

    if (cr->IsPersistent() && !cr->IsExplicitlyPersistent()) {
        _engine->EntityMngr.MakePersistent(cr, false);
    }
}

void Map::RefreshCritterPlayerState(ptr<Critter> cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    FO_VERIFY_AND_THROW(_crittersMap.count(cr->GetId()), "Server map is refreshing a critter that is absent from the map critter lookup", GetId(), cr->GetId(), _crittersMap.size());

    const bool removed_from_players = vec_safe_remove_unique_value(_playerCritters, cr);
    const bool removed_from_non_players = vec_safe_remove_unique_value(_nonPlayerCritters, cr);
    FO_VERIFY_AND_THROW(removed_from_players || removed_from_non_players, "Critter was not removed from any map critter bucket");

    auto cr_ids = GetCritterIds();
    vec_safe_remove_unique_value(cr_ids, cr->GetId());

    if (!cr->GetControlledByPlayer()) {
        vec_add_unique_value(cr_ids, cr->GetId());
    }

    SetCritterIds(cr_ids);

    if (cr->GetControlledByPlayer()) {
        vec_add_unique_value(_playerCritters, cr);
    }
    else {
        vec_add_unique_value(_nonPlayerCritters, cr);
    }
}

void Map::AddCritterToField(ptr<Critter> cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    const auto hex = cr->GetHex();
    FO_VERIFY_AND_THROW(_mapSize.is_valid_pos(hex), "Server map cannot add critter to a field outside map bounds", GetId(), cr->GetId(), hex, _mapSize);

    ptr<Field> field = _hexField->GetCellForWriting(hex);

    vec_add_unique_value(field->Critters, cr);
    RecacheHexFlags(field);
    SetMultihexCritter(cr, true);
}

void Map::RemoveCritterFromField(ptr<Critter> cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    const auto hex = cr->GetHex();
    FO_VERIFY_AND_THROW(_mapSize.is_valid_pos(hex), "Server map cannot remove critter from a field outside map bounds", GetId(), cr->GetId(), hex, _mapSize);

    ptr<Field> field = _hexField->GetCellForWriting(hex);

    vec_remove_unique_value(field->Critters, cr);
    RecacheHexFlags(field);
    SetMultihexCritter(cr, false);
}

void Map::SetMultihexCritter(ptr<Critter> cr, bool set)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    const auto multihex = cr->GetMultihex();

    if (multihex != 0) {
        const auto hex = cr->GetHex();
        const auto hexes_around = GeometryHelper::HexesInRadius(multihex);

        for (int32_t i = 1; i < hexes_around; i++) {
            if (auto hex_around = hex; GeometryHelper::MoveHexAroundAway(hex_around, i, _mapSize)) {
                ptr<Field> field = _hexField->GetCellForWriting(hex_around);

                if (set) {
                    vec_add_unique_value(field->Critters, cr);
                }
                else {
                    vec_remove_unique_value(field->Critters, cr);
                }

                RecacheHexFlags(field);
            }
        }
    }
}

void Map::AddItem(ptr<Item> item, mpos hex, nptr<Critter> dropper)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    FO_VERIFY_AND_THROW(!item->GetStatic(), "Item is static and cannot be attached here");
    FO_VERIFY_AND_THROW(_mapSize.is_valid_pos(hex), "Server map cannot place item on a hex outside map bounds", GetId(), item->GetId(), item->GetProtoId(), hex, _mapSize);
    ptr<Map> self = this;
    auto map_holder = self.hold_ref();
    auto item_holder = item.hold_ref();
    ignore_unused(map_holder);
    ignore_unused(item_holder);

    item->SetOwnership(ItemOwnership::MapHex);
    item->SetMapId(GetId());
    item->SetHex(hex);

    auto nullable_entity_lock = GetEntityLock();
    FO_VERIFY_AND_THROW(nullable_entity_lock, "Map has no entity lock");
    auto entity_lock = nullable_entity_lock.as_ptr();
    PropagateEntityLock(item, entity_lock);

    SetItem(item);

    auto item_ids = GetItemIds();
    vec_add_unique_value(item_ids, item->GetId());
    SetItemIds(item_ids);

    if (IsPersistent()) {
        _engine->EntityMngr.MakePersistent(item, true);
    }

    const ident_t initial_item_map_id = item->GetMapId();
    const mpos initial_item_hex = item->GetHex();

    // Process critters view
    for (ptr<Critter> cr : copy_hold_ref(_critters)) {
        if (cr->IsDestroyed()) {
            continue;
        }

        if (cr->CanSeeItemOnMap(item)) {
            cr->AddVisibleItem(item->GetId());
            cr->Send_AddItemOnMap(item);
            ValidateEntityAccess(cr);
            ValidateEntityAccess(item);
            ValidateEntityAccess(dropper);
            cr->OnItemOnMapAppeared.Fire(item.get(), dropper.get());

            if (IsMapItemContextChanged(item, initial_item_map_id, initial_item_hex)) {
                return;
            }
        }
    }

    // Notify spectators
    for (refcount_ptr<Player> player : GetSpectatorPlayersForSend()) {
        player->Send_AddItemOnMap(item);
    }
}

void Map::SetItem(ptr<Item> item)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    FO_VERIFY_AND_THROW(!IsDestroyed(), "Cannot add an item to an already destroyed map", GetId());
    FO_VERIFY_AND_THROW(!IsDestroying(), "Cannot add an item to a map that is being destroyed", GetId());
    FO_VERIFY_AND_THROW(!_itemsMap.count(item->GetId()), "Server map already contains an item with the same id", GetId(), item->GetId(), item->GetProtoId());

    _itemsMap.emplace(item->GetId(), item);
    vec_add_unique_value(_items, item);
    ptr<Map> map = this;
    item->SetParent(map);

    const auto hex = item->GetHex();
    ptr<Field> field = _hexField->GetCellForWriting(hex);

    vec_add_unique_value(field->Items, item);

    RecacheHexFlags(field);

    if (item->IsNonEmptyMultihexLines() || item->IsNonEmptyMultihexMesh()) {
        vector<mpos> multihex_entries;
        const auto multihex_lines = item->GetMultihexLines();
        const auto multihex_mesh = item->GetMultihexMesh();
        multihex_entries.reserve(multihex_lines.size() + multihex_mesh.size());

        GeometryHelper::ForEachMultihexLines(multihex_lines, hex, _mapSize, [&](mpos multihex) {
            ptr<Field> multihex_field = _hexField->GetCellForWriting(multihex);

            if (vec_safe_add_unique_value(multihex_field->Items, item)) {
                RecacheHexFlags(multihex_field);
                multihex_entries.emplace_back(multihex);
            }
        });

        for (const auto multihex : multihex_mesh) {
            if (multihex != hex && _mapSize.is_valid_pos(multihex)) {
                ptr<Field> multihex_field = _hexField->GetCellForWriting(multihex);

                if (vec_safe_add_unique_value(multihex_field->Items, item)) {
                    RecacheHexFlags(multihex_field);
                    multihex_entries.emplace_back(multihex);
                }
            }
        }

        if (!multihex_entries.empty()) {
            item->SetMultihexEntries(std::move(multihex_entries));
        }
    }
}

void Map::RemoveItem(ident_t item_id)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    FO_VERIFY_AND_THROW(item_id, "Missing required item id");
    const auto it = _itemsMap.find(item_id);
    FO_VERIFY_AND_THROW(it != _itemsMap.end(), "Lookup failed in items map");
    ptr<Item> item = it->second;
    ptr<Map> self = this;
    auto map_holder = self.hold_ref();
    auto item_holder = item.hold_ref();
    ignore_unused(map_holder);
    ignore_unused(item_holder);
    _itemsMap.erase(it);

    vec_remove_unique_value(_items, item);

    const auto hex = item->GetHex();
    ptr<Field> field = _hexField->GetCellForWriting(hex);

    vec_remove_unique_value(field->Items, item);

    RevertEntityLock(item);
    auto ctx = _engine->RequireCurrentSyncContext();
    ctx->EnsureEntitySynced(item);

    item->SetParent(nullptr);
    item->SetOwnership(ItemOwnership::Nowhere);
    item->SetMapId(ident_t {});
    item->SetHex({});

    auto item_ids = GetItemIds();
    vec_remove_unique_value(item_ids, item->GetId());
    SetItemIds(item_ids);

    if (item->IsPersistent() && !item->IsExplicitlyPersistent()) {
        _engine->EntityMngr.MakePersistent(item, false);
    }

    RecacheHexFlags(field);

    if (item->HasMultihexEntries()) {
        auto multihex_entries = item->GetMultihexEntries().as_ptr();

        for (const auto multihex : *multihex_entries) {
            ptr<Field> multihex_field = _hexField->GetCellForWriting(multihex);
            vec_remove_unique_value(multihex_field->Items, item);
            RecacheHexFlags(multihex_field);
        }

        item->SetMultihexEntries({});
    }

    // Process critters view
    for (ptr<Critter> cr : copy_hold_ref(_critters)) {
        if (cr->IsDestroyed()) {
            continue;
        }

        if (cr->RemoveVisibleItem(item->GetId())) {
            cr->Send_RemoveItemFromMap(item);
            ValidateEntityAccess(cr);
            ValidateEntityAccess(item);
            cr->OnItemOnMapDisappeared.Fire(item.get(), nullptr);

            if (IsDestroyed() || item->IsDestroyed()) {
                return;
            }
        }
    }

    // Notify spectators
    for (refcount_ptr<Player> player : GetSpectatorPlayersForSend()) {
        player->Send_RemoveItemFromMap(item);
    }
}

void Map::SendProperty(NetProperty type, ptr<const Property> prop, ptr<ServerEntity> entity)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();

    if (type == NetProperty::Map) {
        auto map = entity.dyn_cast<Map>();
        FO_VERIFY_AND_THROW(map == this, "Map callback was invoked for a different map instance");

        // Pure fan-out to every map critter's player plus the spectators (both resolved lock-free, pinned).
        // Send_Property validates the subject (this map) and reads its data live; the map is in sync here.
        for (ptr<Critter> cr : _critters) {
            if (refcount_nptr<Player> player = cr->GetPlayerForSend()) {
                player->Send_Property(type, prop, entity);
            }
        }

        for (refcount_ptr<Player> player : GetSpectatorPlayersForSend()) {
            player->Send_Property(type, prop, entity);
        }
    }
    else if (type == NetProperty::MapItem) {
        auto nullable_item = entity.dyn_cast<Item>();
        FO_VERIFY_AND_THROW(nullable_item, "Missing item instance");
        FO_VERIFY_AND_THROW(nullable_item->GetOwnership() == ItemOwnership::MapHex, "Item is not placed on map hex");
        FO_VERIFY_AND_THROW(nullable_item->GetMapId() == GetId(), "Item belongs to a different map");
        FO_VERIFY_AND_THROW(GetItem(nullable_item->GetId()) == nullable_item, "Map item index returned a different item instance");
        ptr<Map> self = this;
        auto item = nullable_item.as_ptr();
        auto map_holder = self.hold_ref();
        auto item_holder = item.hold_ref();
        ignore_unused(map_holder);
        ignore_unused(item_holder);

        const ident_t initial_item_map_id = nullable_item->GetMapId();
        const mpos initial_item_hex = nullable_item->GetHex();

        for (ptr<Critter> cr : copy_hold_ref(_critters)) {
            if (cr->CheckVisibleItem(nullable_item->GetId())) {
                cr->Send_Property(type, prop, entity);
                ValidateEntityAccess(cr);
                ValidateEntityAccess(nullable_item);
                cr->OnItemOnMapChanged.Fire(item.get());

                if (IsMapItemContextChanged(item, initial_item_map_id, initial_item_hex)) {
                    return;
                }
            }
        }

        for (refcount_ptr<Player> player : GetSpectatorPlayersForSend()) {
            player->Send_Property(type, prop, entity);
        }
    }
    else {
        FO_UNREACHABLE_PLACE();
    }
}

auto Map::IsHexMovable(mpos hex) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    const auto& field = _hexField->GetCellForReading(hex);
    const auto& static_field = _staticMap->HexField->GetCellForReading(hex);

    return !field.MoveBlocked && !static_field.MoveBlocked;
}

auto Map::IsHexShootable(mpos hex) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    const auto& field = _hexField->GetCellForReading(hex);
    const auto& static_field = _staticMap->HexField->GetCellForReading(hex);

    return !field.ShootBlocked && !static_field.ShootBlocked;
}

auto Map::IsHexesMovable(mpos hex, int32_t radius) const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    const auto hexes_around = GeometryHelper::HexesInRadius(radius);

    for (int32_t i = 0; i < hexes_around; i++) {
        if (auto check_hex = hex; GeometryHelper::MoveHexAroundAway(check_hex, i, _mapSize)) {
            if (!IsHexMovable(check_hex)) {
                return false;
            }
        }
    }

    return true;
}

auto Map::HasLivingCritter(mpos hex, nptr<const Critter> ignore_cr) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    const auto& field = _hexField->GetCellForReading(hex);

    for (ptr<const Critter> cr : field.Critters) {
        nptr<const Critter> field_cr = cr;

        if (!field_cr->IsDead() && !(field_cr == ignore_cr)) {
            return true;
        }
    }

    return false;
}

void Map::ChangeViewItem(ptr<Item> item)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    FO_VERIFY_AND_THROW(item->GetOwnership() == ItemOwnership::MapHex, "Item is not placed on map hex");
    FO_VERIFY_AND_THROW(item->GetMapId() == GetId(), "Item belongs to a different map");
    FO_VERIFY_AND_THROW(GetItem(item->GetId()) == item, "Map item index returned a different item instance");
    ptr<Map> self = this;
    auto map_holder = self.hold_ref();
    auto item_holder = item.hold_ref();
    ignore_unused(map_holder);
    ignore_unused(item_holder);

    const ident_t initial_item_map_id = item->GetMapId();
    const mpos initial_item_hex = item->GetHex();

    for (ptr<Critter> cr : copy_hold_ref(_critters)) {
        if (cr->IsDestroyed()) {
            continue;
        }

        if (cr->CheckVisibleItem(item->GetId())) {
            if (!cr->CanSeeItemOnMap(item)) {
                cr->RemoveVisibleItem(item->GetId());
                cr->Send_RemoveItemFromMap(item);
                ValidateEntityAccess(cr);
                ValidateEntityAccess(item);
                cr->OnItemOnMapDisappeared.Fire(item.get(), nullptr);

                if (IsMapItemContextChanged(item, initial_item_map_id, initial_item_hex)) {
                    return;
                }
            }
        }
        else if (cr->CanSeeItemOnMap(item)) {
            cr->AddVisibleItem(item->GetId());
            cr->Send_AddItemOnMap(item);
            ValidateEntityAccess(cr);
            ValidateEntityAccess(item);
            cr->OnItemOnMapAppeared.Fire(item.get(), nullptr);

            if (IsMapItemContextChanged(item, initial_item_map_id, initial_item_hex)) {
                return;
            }
        }
    }
}

auto Map::IsMapItemContextChanged(ptr<const Item> item, ident_t map_id, mpos hex) const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();

    if (IsDestroyed() || item->IsDestroyed()) {
        return true;
    }
    if (item->GetOwnership() != ItemOwnership::MapHex || item->GetMapId() != map_id || item->GetHex() != hex) {
        return true;
    }

    const auto it = _itemsMap.find(item->GetId());

    if (it == _itemsMap.end() || !(it->second == item)) {
        return true;
    }

    return false;
}

auto Map::IsBlockItemOnHex(mpos hex) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    const auto& field = _hexField->GetCellForReading(hex);

    return field.HasNoMoveItem;
}

auto Map::IsTriggerItemOnHex(mpos hex) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    const auto& field = _hexField->GetCellForReading(hex);

    return field.HasTriggerItem;
}

auto Map::GetItem(ident_t item_id) noexcept -> nptr<Item>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    const auto it = _itemsMap.find(item_id);

    if (it != _itemsMap.end()) {
        return it->second.as_nptr();
    }

    return nullptr;
}

auto Map::GetItemOnHex(mpos hex, hstring item_pid, nptr<const Critter> picker) -> nptr<Item>
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    const auto& field = _hexField->GetCellForReading(hex);

    if (field.Items.empty()) {
        return nullptr;
    }

    ptr<Field> field2 = _hexField->GetCellForWriting(hex);

    for (ptr<Item> item : field2->Items) {
        if ((!item_pid || item->GetProtoId() == item_pid) && (!picker || picker->CanSeeItemOnMap(item))) {
            return item.as_nptr();
        }
    }

    return nullptr;
}

auto Map::GetItemsOnHex(mpos hex) noexcept -> vector<ptr<Item>>
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    const auto& field = _hexField->GetCellForReading(hex);

    if (field.Items.empty()) {
        return {};
    }

    ptr<Field> field2 = _hexField->GetCellForWriting(hex);
    return field2->Items;
}

auto Map::GetItemsInRadius(mpos hex, int32_t radius) -> vector<ptr<Item>>
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    vector<ptr<Item>> items;

    const int32_t hexes_around = GeometryHelper::HexesInRadius(radius);
    unordered_set<ident_t> seen;
    seen.reserve(numeric_cast<size_t>(hexes_around));
    items.reserve(numeric_cast<size_t>(hexes_around));

    for (int32_t i = 0; i < hexes_around; i++) {
        if (mpos hex_around = hex; GeometryHelper::MoveHexAroundAway(hex_around, i, _mapSize)) {
            const auto& field = _hexField->GetCellForReading(hex_around);

            if (!field.Items.empty()) {
                ptr<Field> field2 = _hexField->GetCellForWriting(hex_around);

                for (ptr<Item> item : field2->Items) {
                    if (seen.insert(item->GetId()).second) {
                        items.emplace_back(item);
                    }
                }
            }
        }
    }

    return items;
}

auto Map::GetTriggerItemsOnHex(mpos hex) noexcept -> vector<ptr<Item>>
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    const auto& field = _hexField->GetCellForReading(hex);

    if (field.Items.empty()) {
        return {};
    }

    ptr<Field> field2 = _hexField->GetCellForWriting(hex);
    vector<ptr<Item>> triggers;

    for (ptr<Item> item : field2->Items) {
        if (item->GetIsTrigger()) {
            triggers.emplace_back(item);
        }
    }

    return triggers;
}

auto Map::IsValidPlaceForItem(mpos hex, ptr<const ProtoItem> proto_item) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();

    if (!IsHexMovable(hex)) {
        return false;
    }

    bool is_critter = false;
    GeometryHelper::ForEachMultihexLines(proto_item->GetMultihexLines(), hex, _mapSize, [this, &is_critter](mpos multihex) { is_critter |= IsCritterOnHex(multihex, CritterFindType::Any); });
    return !is_critter;
}

void Map::RecacheHexFlags(mpos hex)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    ptr<Field> field = _hexField->GetCellForWriting(hex);

    RecacheHexFlags(field);
}

void Map::RecacheHexFlags(ptr<Field> field)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    FO_NON_CONST_METHOD_HINT();

    field->HasCritter = !field->Critters.empty();
    field->HasTriggerItem = false;
    field->HasNoMoveItem = false;
    field->MovableWithGag = false;
    field->HasNoShootItem = false;
    field->MovableWithGag = !field->ManualBlock;

    if (!field->Items.empty()) {
        for (ptr<const Item> item : field->Items) {
            if (!item->GetNoBlock()) {
                field->HasNoMoveItem = true;
                field->MovableWithGag = field->MovableWithGag && item->GetIsGag();
            }
            if (!item->GetShootThru()) {
                field->HasNoShootItem = true;
                field->MovableWithGag = field->MovableWithGag && item->GetIsGag();
            }
            if (!field->HasTriggerItem && item->GetIsTrigger()) {
                field->HasTriggerItem = true;
            }
        }
    }

    field->ShootBlocked = field->HasNoShootItem || (field->ManualBlock && field->ManualBlockFull);
    field->MoveBlocked = field->ShootBlocked || field->HasNoMoveItem || field->ManualBlock;
    field->MovableWithGag = field->MovableWithGag && (field->HasNoMoveItem || field->HasNoShootItem);
}

void Map::SetHexManualBlock(mpos hex, bool enable, bool full)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    ptr<Field> field = _hexField->GetCellForWriting(hex);

    field->ManualBlock = enable;
    field->ManualBlockFull = full;

    RecacheHexFlags(field);
}

auto Map::IsCritterOnHex(mpos hex, CritterFindType find_type) const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    const auto& field = _hexField->GetCellForReading(hex);

    if (field.HasCritter) {
        if (find_type == CritterFindType::Any) {
            return true;
        }

        for (ptr<const Critter> cr : field.Critters) {
            if (cr->CheckFind(find_type)) {
                return true;
            }
        }
    }

    return false;
}

auto Map::IsCritterOnHex(mpos hex, ptr<const Critter> cr) const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    const auto& field = _hexField->GetCellForReading(hex);

    if (field.HasCritter) {
        for (ptr<const Critter> field_cr : field.Critters) {
            if (field_cr == cr) {
                return true;
            }
        }
    }

    return false;
}

auto Map::GetCritter(ident_t cr_id) noexcept -> nptr<Critter>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();

    if (const auto it = _crittersMap.find(cr_id); it != _crittersMap.end()) {
        return it->second.as_nptr();
    }

    return nullptr;
}

auto Map::GetCritterOnHex(mpos hex, CritterFindType find_type) noexcept -> nptr<Critter>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    const auto& field = _hexField->GetCellForReading(hex);

    if (field.HasCritter) {
        ptr<Field> field2 = _hexField->GetCellForWriting(hex);

        for (ptr<Critter> cr : field2->Critters) {
            if (cr->CheckFind(find_type)) {
                return cr.as_nptr();
            }
        }
    }

    return nullptr;
}

auto Map::GetCrittersOnHex(mpos hex, CritterFindType find_type) -> vector<ptr<Critter>>
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    vector<ptr<Critter>> critters;
    const auto& field = _hexField->GetCellForReading(hex);

    if (field.HasCritter) {
        ptr<Field> field2 = _hexField->GetCellForWriting(hex);

        for (ptr<Critter> cr : field2->Critters) {
            if (cr->CheckFind(find_type)) {
                critters.emplace_back(cr);
            }
        }
    }

    return critters;
}

auto Map::GetCrittersOnHex(mpos hex, CritterFindType find_type) const -> vector<ptr<const Critter>>
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    vector<ptr<const Critter>> critters;
    const auto& field = _hexField->GetCellForReading(hex);

    if (field.HasCritter) {
        for (ptr<const Critter> cr : field.Critters) {
            if (cr->CheckFind(find_type)) {
                critters.emplace_back(cr);
            }
        }
    }

    return critters;
}

auto Map::GetCrittersInRadius(mpos hex, int32_t radius, CritterFindType find_type) -> vector<ptr<Critter>>
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    vector<ptr<Critter>> critters;

    const int32_t hexes_in_radius = GeometryHelper::HexesInRadius(radius);
    unordered_set<ident_t> seen;
    seen.reserve(numeric_cast<size_t>(hexes_in_radius));

    for (int32_t i = 0; i < hexes_in_radius; i++) {
        if (mpos cur_hex = hex; GeometryHelper::MoveHexAroundAway(cur_hex, numeric_cast<int32_t>(i), _mapSize)) {
            const auto& field = _hexField->GetCellForReading(cur_hex);

            if (!field.HasCritter) {
                continue;
            }

            ptr<Field> field2 = _hexField->GetCellForWriting(cur_hex);

            for (ptr<Critter> cr : field2->Critters) {
                if (seen.insert(cr->GetId()).second && cr->CheckFind(find_type)) {
                    critters.emplace_back(cr);
                }
            }
        }
    }

    return critters;
}

auto Map::IsTriggerStaticItemOnHex(mpos hex) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    const auto& static_field = _staticMap->HexField->GetCellForReading(hex);

    return !static_field.TriggerItems.empty();
}

auto Map::GetStaticItem(ident_t id) noexcept -> nptr<StaticItem>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();

    if (const auto it = _staticMap->StaticItemsById.find(id); it != _staticMap->StaticItemsById.end()) {
        return it->second.as_nptr();
    }

    return nullptr;
}

auto Map::GetStaticItemOnHex(mpos hex, hstring pid) noexcept -> nptr<StaticItem>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    const auto& static_field = _staticMap->HexField->GetCellForReading(hex);

    if (static_field.StaticItems.empty()) {
        return nullptr;
    }

    for (ptr<StaticItem> item : _staticMap->HexField->GetCellForWriting(hex)->StaticItems) {
        if (!pid || item->GetProtoId() == pid) {
            return item.as_nptr();
        }
    }

    return nullptr;
}

auto Map::GetStaticItems(hstring pid) -> vector<ptr<StaticItem>>
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    vector<ptr<StaticItem>> items;

    for (ptr<StaticItem> item : _staticMap->StaticItems) {
        if (!pid || item->GetProtoId() == pid) {
            items.emplace_back(item);
        }
    }

    return items;
}

auto Map::GetStaticItemsOnHex(mpos hex) noexcept -> span<ptr<StaticItem>>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    const auto& static_field = _staticMap->HexField->GetCellForReading(hex);

    if (static_field.StaticItems.empty()) {
        return {};
    }

    return _staticMap->HexField->GetCellForWriting(hex)->StaticItems;
}

auto Map::GetStaticItemsInRadius(mpos hex, int32_t radius, hstring pid) -> vector<ptr<StaticItem>>
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    vector<ptr<StaticItem>> items;

    const int32_t hexes_in_radius = GeometryHelper::HexesInRadius(radius);
    unordered_set<ptr<const StaticItem>> seen;
    seen.reserve(numeric_cast<size_t>(hexes_in_radius));

    for (int32_t i = 0; i < hexes_in_radius; i++) {
        if (mpos cur_hex = hex; GeometryHelper::MoveHexAroundAway(cur_hex, i, _mapSize)) {
            const auto& static_field = _staticMap->HexField->GetCellForReading(cur_hex);

            if (static_field.StaticItems.empty()) {
                continue;
            }

            ptr<StaticMap::Field> static_field2 = _staticMap->HexField->GetCellForWriting(cur_hex);

            for (ptr<StaticItem> item : static_field2->StaticItems) {
                if (seen.insert(item).second && (!pid || item->GetProtoId() == pid)) {
                    items.emplace_back(item);
                }
            }
        }
    }

    return items;
}

auto Map::GetTriggerStaticItemsOnHex(mpos hex) noexcept -> span<ptr<StaticItem>>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    const auto& static_field = _staticMap->HexField->GetCellForReading(hex);

    if (static_field.TriggerItems.empty()) {
        return {};
    }

    return _staticMap->HexField->GetCellForWriting(hex)->TriggerItems;
}

auto Map::IsOutsideArea(mpos hex) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    const irect32 scroll_area = GetScrollAxialArea();

    if (!scroll_area.is_zero()) {
        const ipos32 axial_hex = GeometryHelper::GetHexAxialCoord(hex);

        if (axial_hex.x < scroll_area.x || axial_hex.x > scroll_area.x + scroll_area.width || //
            axial_hex.y < scroll_area.y || axial_hex.y > scroll_area.y + scroll_area.height) {
            return true;
        }
    }

    return false;
}

void Map::VerifyTrigger(ptr<Critter> cr, mpos from_hex, mpos to_hex, mdir dir)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();

    const ident_t initial_cr_map_id = cr->GetMapId();
    const mpos initial_cr_hex = cr->GetHex();

    const auto is_trigger_context_changed = [this, cr, initial_cr_map_id, initial_cr_hex]() noexcept -> bool {
        FO_NO_STACK_TRACE_ENTRY();

        if (IsDestroyed() || cr->IsDestroyed()) {
            return true;
        }
        if (cr->GetMapId() != initial_cr_map_id || cr->GetHex() != initial_cr_hex) {
            return true;
        }
        nptr<Critter> trigger_cr = cr;
        auto map_cr = GetCritter(cr->GetId());

        if (initial_cr_map_id == GetId() && !(map_cr == trigger_cr)) {
            return true;
        }

        return false;
    };

    if (is_trigger_context_changed()) {
        return;
    }

    if (IsTriggerStaticItemOnHex(from_hex)) {
        for (ptr<StaticItem> item : GetTriggerStaticItemsOnHex(from_hex)) {
            if (item->TriggerScriptFunc) {
                item->TriggerScriptFunc.Call(cr.get(), item.get(), false, dir);

                if (is_trigger_context_changed()) {
                    return;
                }
            }

            ValidateEntityAccess(item);
            ValidateEntityAccess(cr);
            _engine->OnStaticItemWalk.Fire(item.get(), cr.get(), false, dir);

            if (is_trigger_context_changed()) {
                return;
            }
        }
    }

    if (IsTriggerStaticItemOnHex(to_hex)) {
        for (ptr<StaticItem> item : GetTriggerStaticItemsOnHex(to_hex)) {
            if (item->TriggerScriptFunc) {
                item->TriggerScriptFunc.Call(cr.get(), item.get(), true, dir);

                if (is_trigger_context_changed()) {
                    return;
                }
            }

            ValidateEntityAccess(item);
            ValidateEntityAccess(cr);
            _engine->OnStaticItemWalk.Fire(item.get(), cr.get(), true, dir);

            if (is_trigger_context_changed()) {
                return;
            }
        }
    }

    if (IsTriggerItemOnHex(from_hex)) {
        for (ptr<Item> item : GetTriggerItemsOnHex(from_hex)) {
            if (item->IsDestroyed()) {
                continue;
            }

            ValidateEntityAccess(item);
            ValidateEntityAccess(cr);
            item->OnCritterWalk.Fire(cr.get(), false, dir);

            if (is_trigger_context_changed()) {
                return;
            }
        }
    }

    if (IsTriggerItemOnHex(to_hex)) {
        for (ptr<Item> item : GetTriggerItemsOnHex(to_hex)) {
            if (item->IsDestroyed()) {
                continue;
            }

            ValidateEntityAccess(item);
            ValidateEntityAccess(cr);
            item->OnCritterWalk.Fire(cr.get(), true, dir);

            if (is_trigger_context_changed()) {
                return;
            }
        }
    }
}

auto Map::CheckGagItems(mpos hex, int32_t radius, const function<bool(ptr<const Item>)>& gag_callback) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    const auto hexes_around = GeometryHelper::HexesInRadius(radius);

    for (int32_t i = 0; i < hexes_around; i++) {
        if (auto check_hex = hex; GeometryHelper::MoveHexAroundAway(check_hex, i, _mapSize)) {
            if (IsHexMovable(check_hex)) {
                continue;
            }
            if (!CheckGagItem(check_hex, gag_callback)) {
                return false;
            }
        }
    }

    return true;
}

auto Map::CheckGagItem(mpos hex, const function<bool(ptr<const Item>)>& gag_callback) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY_ACCESS();
    const auto& field = _hexField->GetCellForReading(hex);

    if (!field.MovableWithGag) {
        return false;
    }

    for (ptr<const Item> item : field.Items) {
        if (item->GetIsGag() && (!gag_callback || !gag_callback(item))) {
            return false;
        }
    }

    return true;
}

FO_END_NAMESPACE
