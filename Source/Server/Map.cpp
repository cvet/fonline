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

#include "Map.h"
#include "Critter.h"
#include "CritterManager.h"
#include "GenericUtils.h"
#include "Item.h"
#include "Location.h"
#include "Server.h"
#include "Settings.h"
#include "StringUtils.h"

Map::Map(FOServer* engine, ident_t id, const ProtoMap* proto, Location* location, const StaticMap* static_map, const Properties* props) :
    ServerEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_TYPE_NAME), props != nullptr ? props : &proto->GetProperties()),
    EntityWithProto(proto),
    MapProperties(GetInitRef()),
    _staticMap {static_map},
    _mapLocation {location}
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_staticMap);

    _name = strex("{}_{}", proto->GetName(), id);

    _width = GetWidth();
    _height = GetHeight();

    if (engine->Settings.MapInstanceStaticGrid) {
        _hexField = std::make_unique<StaticTwoDimensionalGrid<Field, uint16>>(_width, _height);
    }
    else {
        _hexField = std::make_unique<DynamicTwoDimensionalGrid<Field, uint16>>(_width, _height);
    }
}

Map::~Map()
{
    STACK_TRACE_ENTRY();
}

void Map::Process()
{
    STACK_TRACE_ENTRY();

    const auto time = _engine->GameTime.GameplayTime();

    ProcessLoop(0, GetLoopTime1(), time_duration_to_ms<uint>(time.time_since_epoch()));
    ProcessLoop(1, GetLoopTime2(), time_duration_to_ms<uint>(time.time_since_epoch()));
    ProcessLoop(2, GetLoopTime3(), time_duration_to_ms<uint>(time.time_since_epoch()));
    ProcessLoop(3, GetLoopTime4(), time_duration_to_ms<uint>(time.time_since_epoch()));
    ProcessLoop(4, GetLoopTime5(), time_duration_to_ms<uint>(time.time_since_epoch()));
}

void Map::ProcessLoop(int index, uint time, uint tick)
{
    STACK_TRACE_ENTRY();

    if (time != 0 && tick - _loopLastTick[index] >= time) {
        _loopLastTick[index] = tick;

        if (index == 0) {
            OnLoop.Fire();
            _engine->OnMapLoop.Fire(this);
        }
        else {
            OnLoopEx.Fire(index);
            _engine->OnMapLoopEx.Fire(this, index);
        }
    }
}

void Map::SetLocation(Location* loc)
{
    STACK_TRACE_ENTRY();

    _mapLocation = loc;
}

auto Map::FindStartHex(uint16 hx, uint16 hy, uint multihex, uint seek_radius, bool skip_unsafe) const -> optional<tuple<uint16, uint16>>
{
    STACK_TRACE_ENTRY();

    if (IsHexesMovable(hx, hy, multihex) && !(skip_unsafe && (IsStaticItemTrigger(hx, hy) || IsItemTrigger(hx, hy)))) {
        return tuple {hx, hy};
    }
    if (seek_radius == 0) {
        return std::nullopt;
    }
    if (seek_radius > MAX_HEX_OFFSET) {
        seek_radius = MAX_HEX_OFFSET;
    }

    const auto [sx, sy] = _engine->Geometry.GetHexOffsets((hx % 2) != 0);
    auto hx_ = static_cast<int16>(hx);
    auto hy_ = static_cast<int16>(hy);
    const auto max_pos = static_cast<int>(GenericUtils::NumericalNumber(seek_radius) * GameSettings::MAP_DIR_COUNT);

    auto pos = -1;
    while (true) {
        pos++;
        if (pos >= max_pos) {
            return std::nullopt;
        }

        pos++;
        if (pos >= max_pos) {
            pos = 0;
        }

        const int nx = hx_ + sx[pos];
        const int ny = hy_ + sy[pos];

        if (nx < 0 || nx >= _width || ny < 0 || ny >= _height) {
            continue;
        }

        const auto nx_ = static_cast<uint16>(nx);
        const auto ny_ = static_cast<uint16>(ny);

        if (!IsHexesMovable(nx_, ny_, multihex)) {
            continue;
        }
        if (skip_unsafe && (IsStaticItemTrigger(nx_, ny_) || IsItemTrigger(nx_, ny_))) {
            continue;
        }

        break;
    }

    hx_ = static_cast<int16>(hx_ + sx[pos]);
    hy_ = static_cast<int16>(hy_ + sy[pos]);
    return tuple {static_cast<uint16>(hx_), static_cast<uint16>(hy_)};
}

void Map::AddCritter(Critter* cr)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!_crittersMap.count(cr->GetId()));

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
    STACK_TRACE_ENTRY();

    const auto it = _crittersMap.find(cr->GetId());
    RUNTIME_ASSERT(it != _crittersMap.end());
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
    STACK_TRACE_ENTRY();

    const auto hx = cr->GetHexX();
    const auto hy = cr->GetHexY();
    RUNTIME_ASSERT(hx < _width && hy < _height);

    auto& field = _hexField->GetCellForWriting(hx, hy);

    vec_add_unique_value(field.Critters, cr);
    RecacheHexFlags(field);
    SetMultihexCritter(cr, true);
}

void Map::RemoveCritterFromField(Critter* cr)
{
    STACK_TRACE_ENTRY();

    const auto hx = cr->GetHexX();
    const auto hy = cr->GetHexY();
    RUNTIME_ASSERT(hx < _width && hy < _height);

    auto& field = _hexField->GetCellForWriting(hx, hy);

    vec_remove_unique_value(field.Critters, cr);
    RecacheHexFlags(field);
    SetMultihexCritter(cr, false);
}

void Map::SetMultihexCritter(Critter* cr, bool set)
{
    STACK_TRACE_ENTRY();

    const uint multihex = cr->GetMultihex();

    if (multihex != 0) {
        const uint16 hx = cr->GetHexX();
        const uint16 hy = cr->GetHexY();
        auto&& [sx, sy] = _engine->Geometry.GetHexOffsets((hx % 2) != 0);

        for (uint i = 0, j = GenericUtils::NumericalNumber(multihex) * GameSettings::MAP_DIR_COUNT; i < j; i++) {
            const auto nx = static_cast<int>(hx) + sx[i];
            const auto ny = static_cast<int>(hy) + sy[i];

            if (nx >= 0 && ny >= 0 && nx < _width && ny < _height) {
                auto& field = _hexField->GetCellForWriting(static_cast<uint16>(nx), static_cast<uint16>(ny));

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

void Map::AddItem(Item* item, uint16 hx, uint16 hy, Critter* dropper)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(item);
    RUNTIME_ASSERT(!item->GetStatic());

    if (hx >= _width || hy >= _height) {
        throw GenericException("Invalid item pos");
    }

    item->SetOwnership(ItemOwnership::MapHex);
    item->SetMapId(GetId());
    item->SetHexX(hx);
    item->SetHexY(hy);

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
                    int dist = static_cast<int>(GeometryHelper::DistGame(cr->GetHexX(), cr->GetHexY(), hx, hy));

                    if (item->GetIsTrap()) {
                        dist += item->GetTrapValue();
                    }

                    allowed = dist <= static_cast<int>(cr->GetLookDistance());
                }

                if (!allowed) {
                    continue;
                }
            }

            cr->AddIdVisItem(item->GetId());
            cr->Send_AddItemOnMap(item);
            cr->OnItemOnMapAppeared.Fire(item, dropper);
        }
    }
}

void Map::SetItem(Item* item)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!_itemsMap.count(item->GetId()));

    _itemsMap.emplace(item->GetId(), item);
    vec_add_unique_value(_items, item);

    const auto hx = item->GetHexX();
    const auto hy = item->GetHexY();
    auto& field = _hexField->GetCellForWriting(hx, hy);

    vec_add_unique_value(field.Items, item);

    RecacheHexFlags(field);

    if (item->IsNonEmptyBlockLines()) {
        GeometryHelper::ForEachBlockLines(item->GetBlockLines(), hx, hy, _width, _height, [this, item](uint16 hx2, uint16 hy2) {
            auto& field2 = _hexField->GetCellForWriting(hx2, hy2);
            field2.BlockLines.emplace_back(item);
            RecacheHexFlags(field2);
        });
    }
}

void Map::RemoveItem(ident_t item_id)
{
    STACK_TRACE_ENTRY();

    Item* item;

    {
        RUNTIME_ASSERT(item_id);
        const auto it = _itemsMap.find(item_id);
        RUNTIME_ASSERT(it != _itemsMap.end());
        item = it->second;
        _itemsMap.erase(it);
    }

    vec_remove_unique_value(_items, item);

    const auto hx = item->GetHexX();
    const auto hy = item->GetHexY();
    auto& field = _hexField->GetCellForWriting(hx, hy);

    vec_remove_unique_value(field.Items, item);

    item->SetOwnership(ItemOwnership::Nowhere);
    item->SetMapId(ident_t {});
    item->SetHexX(0);
    item->SetHexY(0);

    auto item_ids = GetItemIds();
    vec_remove_unique_value(item_ids, item->GetId());
    SetItemIds(item_ids);

    RecacheHexFlags(field);

    if (item->IsNonEmptyBlockLines()) {
        GeometryHelper::ForEachBlockLines(item->GetBlockLines(), hx, hy, _width, _height, [this, item](uint16 hx2, uint16 hy2) {
            auto& field2 = _hexField->GetCellForWriting(hx2, hy2);
            const auto it = std::find(field2.BlockLines.begin(), field2.BlockLines.end(), item);
            RUNTIME_ASSERT(it != field2.BlockLines.end());
            field2.BlockLines.erase(it);
            RecacheHexFlags(field2);
        });
    }

    // Process critters view
    for (auto* cr : copy_hold_ref(GetCritters())) {
        if (cr->IsDestroyed()) {
            continue;
        }

        if (cr->DelIdVisItem(item->GetId())) {
            cr->Send_RemoveItemFromMap(item);
            cr->OnItemOnMapDisappeared.Fire(item, nullptr);
        }
    }
}

void Map::SendProperty(NetProperty type, const Property* prop, ServerEntity* entity)
{
    STACK_TRACE_ENTRY();

    if (type == NetProperty::Map) {
        const auto* map = dynamic_cast<Map*>(entity);
        RUNTIME_ASSERT(map == this);
        for (auto* cr : GetCritters()) {
            cr->Send_Property(type, prop, entity);
        }
    }
    else if (type == NetProperty::MapItem) {
        auto* item = dynamic_cast<Item*>(entity);
        for (auto* cr : copy_hold_ref(GetCritters())) {
            if (cr->CountIdVisItem(item->GetId())) {
                cr->Send_Property(type, prop, entity);
                cr->OnItemOnMapChanged.Fire(item);
            }
        }
    }
    else {
        RUNTIME_ASSERT(false);
    }
}

auto Map::IsHexMovable(uint16 hx, uint16 hy) const noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hx, hy);
    const auto& static_field = _staticMap->HexField->GetCellForReading(hx, hy);

    return !field.MoveBlocked && !static_field.IsMoveBlocked;
}

auto Map::IsHexShootable(uint16 hx, uint16 hy) const noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hx, hy);
    const auto& static_field = _staticMap->HexField->GetCellForReading(hx, hy);

    return !field.ShootBlocked && !static_field.IsShootBlocked;
}

auto Map::IsHexesMovable(uint16 hx, uint16 hy, uint radius) const -> bool
{
    NO_STACK_TRACE_ENTRY();

    // Base
    if (!IsHexMovable(hx, hy)) {
        return false;
    }
    if (radius == 0) {
        return true;
    }

    // Neighbors
    const auto [sx, sy] = _engine->Geometry.GetHexOffsets((hx % 2) != 0);
    const auto count = GenericUtils::NumericalNumber(radius) * GameSettings::MAP_DIR_COUNT;

    for (uint i = 0; i < count; i++) {
        const auto nx = static_cast<int16>(hx) + sx[i];
        const auto ny = static_cast<int16>(hy) + sy[i];

        if (nx >= 0 && ny >= 0 && nx < _width && ny < _height) {
            if (!IsHexMovable(static_cast<uint16>(nx), static_cast<uint16>(ny))) {
                return false;
            }
        }
    }

    return true;
}

auto Map::IsHexesMovable(uint16 to_hx, uint16 to_hy, uint radius, Critter* skip_cr) -> bool
{
    NO_STACK_TRACE_ENTRY();

    if (_engine->Settings.CritterBlockHex) {
        if (skip_cr != nullptr && !skip_cr->IsDead()) {
            // Todo: make movable checks without critter removing
            RemoveCritterFromField(skip_cr);
            const auto result = IsHexesMovable(to_hx, to_hy, radius);
            AddCritterToField(skip_cr);
            return result;
        }
    }

    return IsHexesMovable(to_hx, to_hy, radius);
}

void Map::ChangeViewItem(Item* item)
{
    STACK_TRACE_ENTRY();

    for (auto* cr : copy_hold_ref(GetCritters())) {
        if (cr->IsDestroyed()) {
            continue;
        }

        if (cr->CountIdVisItem(item->GetId())) {
            if (item->GetHidden()) {
                cr->DelIdVisItem(item->GetId());
                cr->Send_RemoveItemFromMap(item);
                cr->OnItemOnMapDisappeared.Fire(item, nullptr);
            }
            else if (!item->GetAlwaysView()) { // Check distance for non-hide items
                bool allowed;

                if (item->GetIsTrap() && IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_ITEM_SCRIPT)) {
                    allowed = _engine->OnMapCheckTrapLook.Fire(this, cr, item);
                }
                else {
                    int dist = static_cast<int>(GeometryHelper::DistGame(cr->GetHexX(), cr->GetHexY(), item->GetHexX(), item->GetHexY()));
                    if (item->GetIsTrap()) {
                        dist += item->GetTrapValue();
                    }

                    allowed = dist <= static_cast<int>(cr->GetLookDistance());
                }

                if (!allowed) {
                    cr->DelIdVisItem(item->GetId());
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
                    int dist = static_cast<int>(GeometryHelper::DistGame(cr->GetHexX(), cr->GetHexY(), item->GetHexX(), item->GetHexY()));
                    if (item->GetIsTrap()) {
                        dist += item->GetTrapValue();
                    }

                    allowed = dist <= static_cast<int>(cr->GetLookDistance());
                }

                if (!allowed) {
                    continue;
                }
            }

            cr->AddIdVisItem(item->GetId());
            cr->Send_AddItemOnMap(item);
            cr->OnItemOnMapAppeared.Fire(item, nullptr);
        }
    }
}

void Map::AnimateItem(Item* item, hstring anim_name, bool looped, bool reversed)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    for (auto* cr : _playerCritters) {
        if (cr->CountIdVisItem(item->GetId())) {
            cr->Send_AnimateItem(item, anim_name, looped, reversed);
        }
    }
}

auto Map::IsBlockItem(uint16 hx, uint16 hy) const noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hx, hy);

    return field.HasNoMoveItem;
}

auto Map::IsItemTrigger(uint16 hx, uint16 hy) const noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hx, hy);

    return field.HasTriggerItem;
}

auto Map::IsItemGag(uint16 hx, uint16 hy) const noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hx, hy);

    return field.HasGagItem;
}

auto Map::GetItem(ident_t item_id) noexcept -> Item*
{
    NO_STACK_TRACE_ENTRY();

    const auto it = _itemsMap.find(item_id);

    return it != _itemsMap.end() ? it->second : nullptr;
}

auto Map::GetItemHex(uint16 hx, uint16 hy, hstring item_pid, Critter* picker) -> Item*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto& field = _hexField->GetCellForReading(hx, hy);

    for (auto* item : field.Items) {
        if ((!item_pid || item->GetProtoId() == item_pid) && (picker == nullptr || (!item->GetHidden() && picker->CountIdVisItem(item->GetId())))) {
            return item;
        }
    }

    return nullptr;
}

auto Map::GetItemGag(uint16 hx, uint16 hy) noexcept -> Item*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto& field = _hexField->GetCellForReading(hx, hy);

    for (auto* item : field.Items) {
        if (item->GetIsGag()) {
            return item;
        }
    }

    return nullptr;
}

auto Map::GetItems() noexcept -> const vector<Item*>&
{
    NO_STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    return _items;
}

auto Map::GetItems(uint16 hx, uint16 hy) noexcept -> const vector<Item*>&
{
    NO_STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto& field = _hexField->GetCellForReading(hx, hy);

    return field.Items;
}

auto Map::GetItemsInRadius(uint16 hx, uint16 hy, uint radius, hstring pid) -> vector<Item*>
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    vector<Item*> items;

    // Todo: optimize iterms radius search by using GetHexOffsets
    for (auto* item : _items) {
        if ((!pid || item->GetProtoId() == pid) && GeometryHelper::CheckDist(item->GetHexX(), item->GetHexY(), hx, hy, radius)) {
            items.emplace_back(item);
        }
    }

    return items;
}

auto Map::GetItemsByProto(hstring pid) -> vector<Item*>
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    vector<Item*> items;

    for (auto* item : _items) {
        if (!pid || item->GetProtoId() == pid) {
            items.emplace_back(item);
        }
    }

    return items;
}

auto Map::GetItemsTrigger(uint16 hx, uint16 hy) -> vector<Item*>
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto& field = _hexField->GetCellForReading(hx, hy);

    return vec_filter(field.Items, [](const Item* item) -> bool { //
        return item->GetIsTrigger() || item->GetIsTrap();
    });
}

auto Map::IsPlaceForProtoItem(uint16 hx, uint16 hy, const ProtoItem* proto_item) const -> bool
{
    STACK_TRACE_ENTRY();

    if (!IsHexMovable(hx, hy)) {
        return false;
    }

    auto is_critter = false;
    GeometryHelper::ForEachBlockLines(proto_item->GetBlockLines(), hx, hy, _width, _height, [this, &is_critter](uint16 hx2, uint16 hy2) { is_critter = is_critter || IsAnyCritter(hx2, hy2); });
    return !is_critter;
}

void Map::RecacheHexFlags(uint16 hx, uint16 hy)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(hx < _width);
    RUNTIME_ASSERT(hy < _height);

    auto& field = _hexField->GetCellForWriting(hx, hy);

    RecacheHexFlags(field);
}

void Map::RecacheHexFlags(Field& field)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    field.HasNonDeadCritter = false;
    field.HasDeadCritter = false;
    field.HasGagItem = false;
    field.HasTriggerItem = false;
    field.HasNoMoveItem = false;
    field.HasNoShootItem = false;

    if (!field.Critters.empty()) {
        for (const auto* cr : field.Critters) {
            if (cr->IsDead()) {
                field.HasDeadCritter = true;
            }
            else {
                field.HasNonDeadCritter = true;
            }

            if (field.HasDeadCritter && field.HasNonDeadCritter) {
                break;
            }
        }
    }

    if (!field.MultihexCritters.empty()) {
        for (const auto* cr : field.MultihexCritters) {
            if (cr->IsDead()) {
                field.HasDeadCritter = true;
            }
            else {
                field.HasNonDeadCritter = true;
            }

            if (field.HasDeadCritter && field.HasNonDeadCritter) {
                break;
            }
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

    const auto is_critter_block = _engine->Settings.CritterBlockHex && field.HasNonDeadCritter;

    field.ShootBlocked = field.HasNoShootItem || (field.ManualBlock && field.ManualBlockFull);
    field.MoveBlocked = field.ShootBlocked || field.HasNoMoveItem || is_critter_block || field.ManualBlock;
}

void Map::SetHexManualBlock(uint16 hx, uint16 hy, bool enable, bool full)
{
    STACK_TRACE_ENTRY();

    auto& field = _hexField->GetCellForWriting(hx, hy);

    field.ManualBlock = enable;
    field.ManualBlockFull = full;

    RecacheHexFlags(field);
}

auto Map::IsAnyCritter(uint16 hx, uint16 hy) const noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hx, hy);

    return field.HasNonDeadCritter || field.HasDeadCritter;
}

auto Map::IsNonDeadCritter(uint16 hx, uint16 hy) const noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hx, hy);

    return field.HasNonDeadCritter;
}

auto Map::IsDeadCritter(uint16 hx, uint16 hy) const noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hx, hy);

    return field.HasDeadCritter;
}

auto Map::IsCritter(uint16 hx, uint16 hy, const Critter* cr) const -> bool
{
    NO_STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(cr);

    const auto& field = _hexField->GetCellForReading(hx, hy);

    if (field.HasNonDeadCritter || field.HasDeadCritter) {
        const auto it1 = std::find(field.Critters.begin(), field.Critters.end(), cr);
        if (it1 != field.Critters.end()) {
            return true;
        }

        const auto it2 = std::find(field.MultihexCritters.begin(), field.MultihexCritters.end(), cr);
        if (it2 != field.MultihexCritters.end()) {
            return true;
        }
    }

    return false;
}

auto Map::GetCritter(ident_t cr_id) noexcept -> Critter*
{
    NO_STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (const auto it = _crittersMap.find(cr_id); it != _crittersMap.end()) {
        return it->second;
    }

    return nullptr;
}

auto Map::GetNonDeadCritter(uint16 hx, uint16 hy) noexcept -> Critter*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto& field = _hexField->GetCellForReading(hx, hy);

    if (field.HasNonDeadCritter) {
        for (auto* cr : field.Critters) {
            if (!cr->IsDead()) {
                return cr;
            }
        }

        for (auto* cr : field.MultihexCritters) {
            if (!cr->IsDead()) {
                return cr;
            }
        }
    }

    return nullptr;
}

auto Map::GetDeadCritter(uint16 hx, uint16 hy) noexcept -> Critter*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto& field = _hexField->GetCellForReading(hx, hy);

    if (field.HasDeadCritter) {
        for (auto* cr : field.Critters) {
            if (cr->IsDead()) {
                return cr;
            }
        }

        for (auto* cr : field.MultihexCritters) {
            if (cr->IsDead()) {
                return cr;
            }
        }
    }

    return nullptr;
}

auto Map::GetCritters(uint16 hx, uint16 hy, uint radius, CritterFindType find_type) -> vector<Critter*>
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    // Todo: optimize critters radius search by using GetHexOffsets
    return vec_filter(_critters, [&](const Critter* cr) -> bool { //
        return cr->CheckFind(find_type) && GeometryHelper::CheckDist(hx, hy, cr->GetHexX(), cr->GetHexY(), radius + cr->GetMultihex());
    });
}

auto Map::GetCritters() noexcept -> const vector<Critter*>&
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    return _critters;
}

auto Map::GetPlayerCritters() noexcept -> const vector<Critter*>&
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    return _playerCritters;
}

auto Map::GetNonPlayerCritters() noexcept -> const vector<Critter*>&
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    return _nonPlayerCritters;
}

auto Map::GetCrittersCount() const noexcept -> uint
{
    STACK_TRACE_ENTRY();

    return static_cast<uint>(_critters.size());
}

auto Map::GetPlayerCrittersCount() const noexcept -> uint
{
    STACK_TRACE_ENTRY();

    return static_cast<uint>(_playerCritters.size());
}

auto Map::GetNonPlayerCrittersCount() const noexcept -> uint
{
    STACK_TRACE_ENTRY();

    return static_cast<uint>(_nonPlayerCritters.size());
}

void Map::SendEffect(hstring eff_pid, uint16 hx, uint16 hy, uint16 radius)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    for (auto* cr : _playerCritters) {
        if (GeometryHelper::CheckDist(cr->GetHexX(), cr->GetHexY(), hx, hy, cr->GetLookDistance() + radius)) {
            cr->Send_Effect(eff_pid, hx, hy, radius);
        }
    }
}

void Map::SendFlyEffect(hstring eff_pid, ident_t from_cr_id, ident_t to_cr_id, uint16 from_hx, uint16 from_hy, uint16 to_hx, uint16 to_hy)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    for (auto* cr : _playerCritters) {
        if (GenericUtils::IntersectCircleLine(cr->GetHexX(), cr->GetHexY(), static_cast<int>(cr->GetLookDistance()), from_hx, from_hy, to_hx, to_hy)) {
            cr->Send_FlyEffect(eff_pid, from_cr_id, to_cr_id, from_hx, from_hy, to_hx, to_hy);
        }
    }
}

void Map::SetText(uint16 hx, uint16 hy, ucolor color, string_view text, bool unsafe_text)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(hx < _width);
    RUNTIME_ASSERT(hy < _height);

    for (auto* cr : _playerCritters) {
        if (cr->GetLookDistance() >= GeometryHelper::DistGame(hx, hy, cr->GetHexX(), cr->GetHexY())) {
            cr->Send_MapText(hx, hy, color, text, unsafe_text);
        }
    }
}

void Map::SetTextMsg(uint16 hx, uint16 hy, ucolor color, TextPackName text_pack, TextPackKey str_num)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(hx < _width);
    RUNTIME_ASSERT(hy < _height);

    for (auto* cr : _playerCritters) {
        if (cr->GetLookDistance() >= GeometryHelper::DistGame(hx, hy, cr->GetHexX(), cr->GetHexY())) {
            cr->Send_MapTextMsg(hx, hy, color, text_pack, str_num);
        }
    }
}

void Map::SetTextMsgLex(uint16 hx, uint16 hy, ucolor color, TextPackName text_pack, TextPackKey str_num, string_view lexems)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(hx < _width);
    RUNTIME_ASSERT(hy < _height);

    for (auto* cr : _playerCritters) {
        if (cr->GetLookDistance() >= GeometryHelper::DistGame(hx, hy, cr->GetHexX(), cr->GetHexY())) {
            cr->Send_MapTextMsgLex(hx, hy, color, text_pack, str_num, lexems);
        }
    }
}

auto Map::IsStaticItemTrigger(uint16 hx, uint16 hy) const noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    const auto& static_field = _staticMap->HexField->GetCellForReading(hx, hy);

    return !static_field.TriggerItems.empty();
}

auto Map::GetStaticItem(ident_t id) noexcept -> StaticItem*
{
    NO_STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (const auto it = _staticMap->StaticItemsById.find(id); it != _staticMap->StaticItemsById.end()) {
        return it->second;
    }

    return nullptr;
}

auto Map::GetStaticItem(uint16 hx, uint16 hy, hstring pid) noexcept -> StaticItem*
{
    NO_STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto& static_field = _staticMap->HexField->GetCellForReading(hx, hy);

    for (auto* item : static_field.StaticItems) {
        if (!pid || item->GetProtoId() == pid) {
            return item;
        }
    }

    return nullptr;
}

auto Map::GetStaticItemsHex(uint16 hx, uint16 hy) noexcept -> const vector<StaticItem*>&
{
    NO_STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto& static_field = _staticMap->HexField->GetCellForReading(hx, hy);

    return static_field.StaticItems;
}

auto Map::GetStaticItemsHexEx(uint16 hx, uint16 hy, uint radius, hstring pid) -> vector<StaticItem*>
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    vector<StaticItem*> items;

    for (auto* item : _staticMap->StaticItems) {
        if ((!pid || item->GetProtoId() == pid) && GeometryHelper::DistGame(item->GetHexX(), item->GetHexY(), hx, hy) <= radius) {
            items.emplace_back(item);
        }
    }

    return items;
}

auto Map::GetStaticItemsByPid(hstring pid) -> vector<StaticItem*>
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    vector<StaticItem*> items;

    for (auto* item : _staticMap->StaticItems) {
        if (!pid || item->GetProtoId() == pid) {
            items.emplace_back(item);
        }
    }

    return items;
}

auto Map::GetStaticItemsTrigger(uint16 hx, uint16 hy) noexcept -> const vector<StaticItem*>&
{
    NO_STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto& static_field = _staticMap->HexField->GetCellForReading(hx, hy);

    return static_field.TriggerItems;
}
