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

#include "Map.h"
#include "Critter.h"
#include "CritterManager.h"
#include "Item.h"
#include "Location.h"
#include "Server.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE();

Map::Map(FOServer* engine, ident_t id, const ProtoMap* proto, Location* location, const StaticMap* static_map, const Properties* props) noexcept :
    ServerEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_TYPE_NAME), props != nullptr ? props : &proto->GetProperties()),
    EntityWithProto(proto),
    MapProperties(GetInitRef()),
    _staticMap {static_map},
    _mapLocation {location}
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(_staticMap);

    _mapSize = GetSize();

    if (engine->Settings.MapInstanceStaticGrid) {
        _hexField = SafeAlloc::MakeUnique<StaticTwoDimensionalGrid<Field, mpos, msize>>(_mapSize);
    }
    else {
        _hexField = SafeAlloc::MakeUnique<DynamicTwoDimensionalGrid<Field, mpos, msize>>(_mapSize);
    }
}

Map::~Map()
{
    FO_STACK_TRACE_ENTRY();
}

void Map::SetLocation(Location* loc) noexcept
{
    FO_STACK_TRACE_ENTRY();

    _mapLocation = loc;
}

auto Map::FindStartHex(mpos hex, int32 multihex, int32 seek_radius, bool skip_unsafe) const -> optional<mpos>
{
    FO_STACK_TRACE_ENTRY();

    if (IsHexesMovable(hex, multihex) && !(skip_unsafe && (IsStaticItemTrigger(hex) || IsItemTrigger(hex)))) {
        return hex;
    }
    if (seek_radius == 0) {
        return std::nullopt;
    }

    const auto max_pos = GenericUtils::NumericalNumber(seek_radius) * GameSettings::MAP_DIR_COUNT;
    int32 pos = GenericUtils::Random(0, max_pos - 1);
    int32 iterations = 0;

    while (true) {
        if (++iterations > max_pos) {
            return std::nullopt;
        }
        if (++pos >= max_pos) {
            pos = 0;
        }

        auto raw_check_hex = ipos32 {hex.x, hex.y};
        GeometryHelper::MoveHexAroundAway(raw_check_hex, pos);

        if (!_mapSize.isValidPos(raw_check_hex)) {
            continue;
        }

        const auto check_hex = _mapSize.fromRawPos(raw_check_hex);

        if (!IsHexesMovable(check_hex, multihex)) {
            continue;
        }
        if (skip_unsafe && (IsStaticItemTrigger(check_hex) || IsItemTrigger(check_hex))) {
            continue;
        }

        break;
    }

    auto raw_hex = ipos32 {hex.x, hex.y};
    GeometryHelper::MoveHexAroundAway(raw_hex, pos);
    const auto result_hex = _mapSize.fromRawPos(raw_hex);

    return result_hex;
}

void Map::AddCritter(Critter* cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!_crittersMap.count(cr->GetId()));

    _crittersMap.emplace(cr->GetId(), cr);
    vec_add_unique_value(_critters, cr);

    if (cr->GetControlledByPlayer()) {
        vec_add_unique_value(_playerCritters, cr);
    }
    else {
        vec_add_unique_value(_nonPlayerCritters, cr);
    }

    AddCritterToField(cr);
}

void Map::RemoveCritter(Critter* cr)
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _crittersMap.find(cr->GetId());
    FO_RUNTIME_ASSERT(it != _crittersMap.end());
    _crittersMap.erase(it);

    vec_remove_unique_value(_critters, cr);

    if (cr->GetControlledByPlayer()) {
        vec_remove_unique_value(_playerCritters, cr);
    }
    else {
        vec_remove_unique_value(_nonPlayerCritters, cr);
    }

    RemoveCritterFromField(cr);
}

void Map::AddCritterToField(Critter* cr)
{
    FO_STACK_TRACE_ENTRY();

    const auto hex = cr->GetHex();
    FO_RUNTIME_ASSERT(_mapSize.isValidPos(hex));

    auto& field = _hexField->GetCellForWriting(hex);

    vec_add_unique_value(field.Critters, cr);
    RecacheHexFlags(field);
    SetMultihexCritter(cr, true);
}

void Map::RemoveCritterFromField(Critter* cr)
{
    FO_STACK_TRACE_ENTRY();

    const auto hex = cr->GetHex();
    FO_RUNTIME_ASSERT(_mapSize.isValidPos(hex));

    auto& field = _hexField->GetCellForWriting(hex);

    vec_remove_unique_value(field.Critters, cr);
    RecacheHexFlags(field);
    SetMultihexCritter(cr, false);
}

void Map::SetMultihexCritter(Critter* cr, bool set)
{
    FO_STACK_TRACE_ENTRY();

    const auto multihex = cr->GetMultihex();

    if (multihex != 0) {
        const auto hex = cr->GetHex();
        const auto max_hexes = GenericUtils::NumericalNumber(multihex) * GameSettings::MAP_DIR_COUNT;

        for (int32 i = 0; i < max_hexes; i++) {
            auto raw_mh_hex = ipos32 {hex.x, hex.y};
            GeometryHelper::MoveHexAroundAway(raw_mh_hex, i);

            if (_mapSize.isValidPos(raw_mh_hex)) {
                const auto mh_hex = _mapSize.fromRawPos(raw_mh_hex);
                auto& field = _hexField->GetCellForWriting(mh_hex);

                if (set) {
                    vec_add_unique_value(field.MultihexCritters, cr);
                }
                else {
                    vec_remove_unique_value(field.MultihexCritters, cr);
                }

                RecacheHexFlags(field);
            }
        }
    }
}

void Map::AddItem(Item* item, mpos hex, Critter* dropper)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(item);
    FO_RUNTIME_ASSERT(!item->GetStatic());
    FO_RUNTIME_ASSERT(_mapSize.isValidPos(hex));

    item->SetOwnership(ItemOwnership::MapHex);
    item->SetMapId(GetId());
    item->SetHex(hex);

    SetItem(item);

    auto item_ids = GetItemIds();
    vec_add_unique_value(item_ids, item->GetId());
    SetItemIds(item_ids);

    // Process critters view
    for (auto* cr : copy_hold_ref(GetCritters())) {
        if (cr->IsDestroyed()) {
            continue;
        }

        if (!item->GetHidden()) {
            if (!item->GetAlwaysView()) {
                bool allowed;

                if (item->GetIsTrap() && IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_ITEM_SCRIPT)) {
                    allowed = _engine->OnMapCheckTrapLook.Fire(this, cr, item);
                }
                else {
                    auto dist = GeometryHelper::GetDistance(cr->GetHex(), hex);

                    if (item->GetIsTrap()) {
                        dist += item->GetTrapValue();
                    }

                    allowed = dist <= cr->GetLookDistance();
                }

                if (!allowed) {
                    continue;
                }
            }

            cr->AddVisibleItem(item->GetId());
            cr->Send_AddItemOnMap(item);
            cr->OnItemOnMapAppeared.Fire(item, dropper);
        }
    }
}

void Map::SetItem(Item* item)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!_itemsMap.count(item->GetId()));

    _itemsMap.emplace(item->GetId(), item);
    vec_add_unique_value(_items, item);

    const auto hex = item->GetHex();
    auto& field = _hexField->GetCellForWriting(hex);

    vec_add_unique_value(field.Items, item);

    RecacheHexFlags(field);

    if (item->IsNonEmptyBlockLines()) {
        GeometryHelper::ForEachBlockLines(item->GetBlockLines(), hex, _mapSize, [this, item](mpos block_hex) {
            auto& block_field = _hexField->GetCellForWriting(block_hex);
            block_field.BlockLines.emplace_back(item);
            RecacheHexFlags(block_field);
        });
    }
}

void Map::RemoveItem(ident_t item_id)
{
    FO_STACK_TRACE_ENTRY();

    Item* item;

    {
        FO_RUNTIME_ASSERT(item_id);
        const auto it = _itemsMap.find(item_id);
        FO_RUNTIME_ASSERT(it != _itemsMap.end());
        item = it->second;
        _itemsMap.erase(it);
    }

    vec_remove_unique_value(_items, item);

    const auto hex = item->GetHex();
    auto& field = _hexField->GetCellForWriting(hex);

    vec_remove_unique_value(field.Items, item);

    item->SetOwnership(ItemOwnership::Nowhere);
    item->SetMapId(ident_t {});
    item->SetHex({});

    auto item_ids = GetItemIds();
    vec_remove_unique_value(item_ids, item->GetId());
    SetItemIds(item_ids);

    RecacheHexFlags(field);

    if (item->IsNonEmptyBlockLines()) {
        GeometryHelper::ForEachBlockLines(item->GetBlockLines(), hex, _mapSize, [this, item](mpos block_hex) {
            auto& block_field = _hexField->GetCellForWriting(block_hex);
            const auto it = std::ranges::find(block_field.BlockLines, item);
            FO_RUNTIME_ASSERT(it != block_field.BlockLines.end());
            block_field.BlockLines.erase(it);
            RecacheHexFlags(block_field);
        });
    }

    // Process critters view
    for (auto* cr : copy_hold_ref(GetCritters())) {
        if (cr->IsDestroyed()) {
            continue;
        }

        if (cr->RemoveVisibleItem(item->GetId())) {
            cr->Send_RemoveItemFromMap(item);
            cr->OnItemOnMapDisappeared.Fire(item, nullptr);
        }
    }
}

void Map::SendProperty(NetProperty type, const Property* prop, ServerEntity* entity)
{
    FO_STACK_TRACE_ENTRY();

    if (type == NetProperty::Map) {
        const auto* map = dynamic_cast<Map*>(entity);
        FO_RUNTIME_ASSERT(map == this);
        for (auto* cr : GetCritters()) {
            cr->Send_Property(type, prop, entity);
        }
    }
    else if (type == NetProperty::MapItem) {
        auto* item = dynamic_cast<Item*>(entity);
        for (auto* cr : copy_hold_ref(GetCritters())) {
            if (cr->CheckVisibleItem(item->GetId())) {
                cr->Send_Property(type, prop, entity);
                cr->OnItemOnMapChanged.Fire(item);
            }
        }
    }
    else {
        FO_RUNTIME_ASSERT(false);
    }
}

auto Map::IsHexMovable(mpos hex) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hex);
    const auto& static_field = _staticMap->HexField->GetCellForReading(hex);

    return !field.MoveBlocked && !static_field.MoveBlocked;
}

auto Map::IsHexShootable(mpos hex) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hex);
    const auto& static_field = _staticMap->HexField->GetCellForReading(hex);

    return !field.ShootBlocked && !static_field.ShootBlocked;
}

auto Map::IsHexesMovable(mpos hex, int32 radius) const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    // Base
    if (!IsHexMovable(hex)) {
        return false;
    }
    if (radius == 0) {
        return true;
    }

    // Neighbors
    const auto count = GenericUtils::NumericalNumber(radius) * GameSettings::MAP_DIR_COUNT;

    for (int32 i = 0; i < count; i++) {
        auto raw_check_hex = ipos32 {hex.x, hex.y};
        GeometryHelper::MoveHexAroundAway(raw_check_hex, i);

        if (_mapSize.isValidPos(raw_check_hex)) {
            if (!IsHexMovable(_mapSize.fromRawPos(raw_check_hex))) {
                return false;
            }
        }
    }

    return true;
}

auto Map::IsHexesMovable(mpos hex, int32 radius, Critter* skip_cr) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_engine->Settings.CritterBlockHex) {
        if (skip_cr != nullptr && !skip_cr->IsDead()) {
            // Todo: make movable checks without critter removing
            RemoveCritterFromField(skip_cr);
            const auto result = IsHexesMovable(hex, radius);
            AddCritterToField(skip_cr);
            return result;
        }
    }

    return IsHexesMovable(hex, radius);
}

void Map::ChangeViewItem(Item* item)
{
    FO_STACK_TRACE_ENTRY();

    for (auto* cr : copy_hold_ref(GetCritters())) {
        if (cr->IsDestroyed()) {
            continue;
        }

        if (cr->CheckVisibleItem(item->GetId())) {
            if (item->GetHidden()) {
                cr->RemoveVisibleItem(item->GetId());
                cr->Send_RemoveItemFromMap(item);
                cr->OnItemOnMapDisappeared.Fire(item, nullptr);
            }
            else if (!item->GetAlwaysView()) { // Check distance for non-hide items
                bool allowed;

                if (item->GetIsTrap() && IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_ITEM_SCRIPT)) {
                    allowed = _engine->OnMapCheckTrapLook.Fire(this, cr, item);
                }
                else {
                    auto dist = GeometryHelper::GetDistance(cr->GetHex(), item->GetHex());

                    if (item->GetIsTrap()) {
                        dist += item->GetTrapValue();
                    }

                    allowed = dist <= cr->GetLookDistance();
                }

                if (!allowed) {
                    cr->RemoveVisibleItem(item->GetId());
                    cr->Send_RemoveItemFromMap(item);
                    cr->OnItemOnMapDisappeared.Fire(item, nullptr);
                }
            }
        }
        else if (!item->GetHidden()) {
            if (!item->GetAlwaysView()) { // Check distance for non-hide items
                bool allowed;

                if (item->GetIsTrap() && IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_ITEM_SCRIPT)) {
                    allowed = _engine->OnMapCheckTrapLook.Fire(this, cr, item);
                }
                else {
                    auto dist = GeometryHelper::GetDistance(cr->GetHex(), item->GetHex());

                    if (item->GetIsTrap()) {
                        dist += item->GetTrapValue();
                    }

                    allowed = dist <= cr->GetLookDistance();
                }

                if (!allowed) {
                    continue;
                }
            }

            cr->AddVisibleItem(item->GetId());
            cr->Send_AddItemOnMap(item);
            cr->OnItemOnMapAppeared.Fire(item, nullptr);
        }
    }
}

auto Map::IsBlockItem(mpos hex) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hex);

    return field.HasNoMoveItem;
}

auto Map::IsItemTrigger(mpos hex) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hex);

    return field.HasTriggerItem;
}

auto Map::IsItemGag(mpos hex) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hex);

    return field.HasGagItem;
}

auto Map::GetItem(ident_t item_id) noexcept -> Item*
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto it = _itemsMap.find(item_id);

    return it != _itemsMap.end() ? it->second : nullptr;
}

auto Map::GetItemHex(mpos hex, hstring item_pid, Critter* picker) -> Item*
{
    FO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hex);

    for (auto* item : field.Items) {
        if ((!item_pid || item->GetProtoId() == item_pid) && (picker == nullptr || (!item->GetHidden() && picker->CheckVisibleItem(item->GetId())))) {
            return item;
        }
    }

    return nullptr;
}

auto Map::GetItemGag(mpos hex) noexcept -> Item*
{
    FO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hex);

    for (auto* item : field.Items) {
        if (item->GetIsGag()) {
            return item;
        }
    }

    return nullptr;
}

auto Map::GetItems() noexcept -> const vector<Item*>&
{
    FO_NO_STACK_TRACE_ENTRY();

    return _items;
}

auto Map::GetItems(mpos hex) noexcept -> const vector<Item*>&
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hex);

    return field.Items;
}

auto Map::GetItemsInRadius(mpos hex, int32 radius, hstring pid) -> vector<Item*>
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    vector<Item*> items;

    // Todo: optimize items radius search by checking directly hexes in radius
    for (auto* item : _items) {
        if ((!pid || item->GetProtoId() == pid) && GeometryHelper::CheckDist(item->GetHex(), hex, radius)) {
            items.emplace_back(item);
        }
    }

    return items;
}

auto Map::GetItemsByProto(hstring pid) -> vector<Item*>
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    vector<Item*> items;

    for (auto* item : _items) {
        if (!pid || item->GetProtoId() == pid) {
            items.emplace_back(item);
        }
    }

    return items;
}

auto Map::GetItemsTrigger(mpos hex) -> vector<Item*>
{
    FO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hex);

    return vec_filter(field.Items, [](const Item* item) -> bool { //
        return item->GetIsTrigger() || item->GetIsTrap();
    });
}

auto Map::IsPlaceForProtoItem(mpos hex, const ProtoItem* proto_item) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!IsHexMovable(hex)) {
        return false;
    }

    bool is_critter = false;
    GeometryHelper::ForEachBlockLines(proto_item->GetBlockLines(), hex, _mapSize, [this, &is_critter](mpos block_hex) { is_critter |= IsCritter(block_hex, CritterFindType::Any); });
    return !is_critter;
}

void Map::RecacheHexFlags(mpos hex)
{
    FO_STACK_TRACE_ENTRY();

    auto& field = _hexField->GetCellForWriting(hex);

    RecacheHexFlags(field);
}

void Map::RecacheHexFlags(Field& field)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    field.HasCritter = false;
    field.HasBlockCritter = false;
    field.HasGagItem = false;
    field.HasTriggerItem = false;
    field.HasNoMoveItem = false;
    field.HasNoShootItem = false;

    field.HasCritter = !field.Critters.empty() || !field.MultihexCritters.empty();

    if (_engine->Settings.CritterBlockHex && field.HasCritter) {
        if (!field.Critters.empty()) {
            field.HasBlockCritter = std::ranges::any_of(field.Critters, [](const Critter* cr) { return !cr->IsDead(); });
        }
        if (!field.HasBlockCritter && !field.MultihexCritters.empty()) {
            field.HasBlockCritter = std::ranges::any_of(field.MultihexCritters, [](const Critter* cr) { return !cr->IsDead(); });
        }
    }

    if (!field.Items.empty()) {
        for (const auto* item : field.Items) {
            if (!field.HasNoMoveItem && !item->GetNoBlock()) {
                field.HasNoMoveItem = true;
            }
            if (!field.HasNoShootItem && !item->GetShootThru()) {
                field.HasNoShootItem = true;
            }
            if (!field.HasGagItem && item->GetIsGag()) {
                field.HasGagItem = true;
            }
            if (!field.HasTriggerItem && (item->GetIsTrigger() || item->GetIsTrap())) {
                field.HasTriggerItem = true;
            }

            if (field.HasNoMoveItem && field.HasNoShootItem && field.HasGagItem && field.HasTriggerItem) {
                break;
            }
        }
    }

    if (!field.BlockLines.empty() && (!field.HasNoMoveItem || !field.HasNoShootItem)) {
        field.HasNoMoveItem = true;

        for (const auto* item : field.BlockLines) {
            if (!item->GetShootThru()) {
                field.HasNoShootItem = true;
                break;
            }
        }
    }

    field.ShootBlocked = field.HasNoShootItem || (field.ManualBlock && field.ManualBlockFull);
    field.MoveBlocked = field.ShootBlocked || field.HasNoMoveItem || field.HasBlockCritter || field.ManualBlock;
}

void Map::SetHexManualBlock(mpos hex, bool enable, bool full)
{
    FO_STACK_TRACE_ENTRY();

    auto& field = _hexField->GetCellForWriting(hex);

    field.ManualBlock = enable;
    field.ManualBlockFull = full;

    RecacheHexFlags(field);
}

auto Map::IsCritter(mpos hex, CritterFindType find_type) const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hex);

    if (field.HasCritter) {
        if (find_type == CritterFindType::Any) {
            return true;
        }

        for (const auto* cr : field.Critters) {
            if (cr->CheckFind(find_type)) {
                return true;
            }
        }

        for (const auto* cr : field.MultihexCritters) {
            if (cr->CheckFind(find_type)) {
                return true;
            }
        }
    }

    return false;
}

auto Map::IsCritter(mpos hex, const Critter* cr) const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(cr);

    const auto& field = _hexField->GetCellForReading(hex);

    if (field.HasCritter) {
        const auto it1 = std::ranges::find(field.Critters, cr);

        if (it1 != field.Critters.end()) {
            return true;
        }

        const auto it2 = std::ranges::find(field.MultihexCritters, cr);

        if (it2 != field.MultihexCritters.end()) {
            return true;
        }
    }

    return false;
}

auto Map::GetCritter(ident_t cr_id) noexcept -> Critter*
{
    FO_NO_STACK_TRACE_ENTRY();

    if (const auto it = _crittersMap.find(cr_id); it != _crittersMap.end()) {
        return it->second;
    }

    return nullptr;
}

auto Map::GetCritter(mpos hex, CritterFindType find_type) noexcept -> Critter*
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hex);

    if (field.HasCritter) {
        for (auto* cr : field.Critters) {
            if (cr->CheckFind(find_type)) {
                return cr;
            }
        }

        for (auto* cr : field.MultihexCritters) {
            if (cr->CheckFind(find_type)) {
                return cr;
            }
        }
    }

    return nullptr;
}

auto Map::GetCritters(mpos hex, int32 radius, CritterFindType find_type) -> vector<Critter*>
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    // Todo: optimize items radius search by checking directly hexes in radius
    return vec_filter(_critters, [&](const Critter* cr) -> bool { //
        return cr->CheckFind(find_type) && GeometryHelper::CheckDist(hex, cr->GetHex(), radius + cr->GetMultihex());
    });
}

auto Map::GetCritters(mpos hex, CritterFindType find_type) -> vector<Critter*>
{
    FO_STACK_TRACE_ENTRY();

    vector<Critter*> critters;
    const auto& field = _hexField->GetCellForReading(hex);

    if (!field.Critters.empty()) {
        for (auto* cr : field.Critters) {
            if (cr->CheckFind(find_type)) {
                critters.emplace_back(cr);
            }
        }
    }

    if (!field.MultihexCritters.empty()) {
        for (auto* cr : field.MultihexCritters) {
            if (cr->CheckFind(find_type)) {
                critters.emplace_back(cr);
            }
        }
    }

    return critters;
}

auto Map::IsStaticItemTrigger(mpos hex) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto& static_field = _staticMap->HexField->GetCellForReading(hex);

    return !static_field.TriggerItems.empty();
}

auto Map::GetStaticItem(ident_t id) noexcept -> StaticItem*
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    if (const auto it = _staticMap->StaticItemsById.find(id); it != _staticMap->StaticItemsById.end()) {
        return it->second;
    }

    return nullptr;
}

auto Map::GetStaticItem(mpos hex, hstring pid) noexcept -> StaticItem*
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    const auto& static_field = _staticMap->HexField->GetCellForReading(hex);

    for (auto* item : static_field.StaticItems) {
        if (!pid || item->GetProtoId() == pid) {
            return item;
        }
    }

    return nullptr;
}

auto Map::GetStaticItemsHex(mpos hex) noexcept -> const vector<StaticItem*>&
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    const auto& static_field = _staticMap->HexField->GetCellForReading(hex);

    return static_field.StaticItems;
}

auto Map::GetStaticItemsHexEx(mpos hex, int32 radius, hstring pid) -> vector<StaticItem*>
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    vector<StaticItem*> items;

    for (auto* item : _staticMap->StaticItems) {
        if ((!pid || item->GetProtoId() == pid) && GeometryHelper::GetDistance(item->GetHex(), hex) <= radius) {
            items.emplace_back(item);
        }
    }

    return items;
}

auto Map::GetStaticItemsByPid(hstring pid) -> vector<StaticItem*>
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    vector<StaticItem*> items;

    for (auto* item : _staticMap->StaticItems) {
        if (!pid || item->GetProtoId() == pid) {
            items.emplace_back(item);
        }
    }

    return items;
}

auto Map::GetStaticItemsTrigger(mpos hex) noexcept -> const vector<StaticItem*>&
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    const auto& static_field = _staticMap->HexField->GetCellForReading(hex);

    return static_field.TriggerItems;
}

auto Map::IsScrollBlock(mpos hex) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto& static_field = _staticMap->HexField->GetCellForReading(hex);

    return static_field.ScrollBlock;
}

FO_END_NAMESPACE();
