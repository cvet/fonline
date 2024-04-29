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
    ServerEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_CLASS_NAME), props != nullptr ? props : &proto->GetProperties()),
    EntityWithProto(proto),
    MapProperties(GetInitRef()),
    _staticMap {static_map},
    _mapLocation {location}
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_staticMap);

    _name = _str("{}_{}", proto->GetName(), id);
    _mapSize = GetSize();
    _hexField.SetSize(_mapSize);
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

auto Map::GetProtoMap() const -> const ProtoMap*
{
    STACK_TRACE_ENTRY();

    return static_cast<const ProtoMap*>(_proto);
}

auto Map::GetLocation() -> Location*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    return _mapLocation;
}

auto Map::GetLocation() const -> const Location*
{
    STACK_TRACE_ENTRY();

    return _mapLocation;
}

void Map::SetLocation(Location* loc)
{
    STACK_TRACE_ENTRY();

    _mapLocation = loc;
}

auto Map::FindStartHex(mpos hex, uint multihex, uint seek_radius, bool skip_unsafe) const -> optional<mpos>
{
    STACK_TRACE_ENTRY();

    if (IsHexesMovable(hex, multihex) && !(skip_unsafe && (IsStaticItemTrigger(hex) || IsItemTrigger(hex)))) {
        return hex;
    }
    if (seek_radius == 0) {
        return std::nullopt;
    }
    if (seek_radius > MAX_HEX_OFFSET) {
        seek_radius = MAX_HEX_OFFSET;
    }

    const auto [sx, sy] = _engine->Geometry.GetHexOffsets(hex);
    const int max_pos = static_cast<int>(GenericUtils::NumericalNumber(seek_radius) * GameSettings::MAP_DIR_COUNT);
    int pos = -1;

    while (true) {
        if (++pos >= max_pos) {
            return std::nullopt;
        }

        if (++pos >= max_pos) {
            pos = 0;
        }

        const auto raw_check_hex = ipos {hex.x + sx[pos], hex.y + sy[pos]};

        if (!_mapSize.IsValidPos(raw_check_hex)) {
            continue;
        }

        const auto check_hex = _mapSize.FromRawPos(raw_check_hex);

        if (!IsHexesMovable(check_hex, multihex)) {
            continue;
        }
        if (skip_unsafe && (IsStaticItemTrigger(check_hex) || IsItemTrigger(check_hex))) {
            continue;
        }

        break;
    }

    const auto result_hex = _mapSize.FromRawPos(ipos {hex.x + sx[pos], hex.y + sy[pos]});

    return result_hex;
}

void Map::AddCritter(Critter* cr)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_crittersMap.count(cr->GetId()) == 0);

    _crittersMap.emplace(cr->GetId(), cr);
    _critters.emplace_back(cr);

    if (cr->GetIsControlledByPlayer()) {
        _playerCritters.emplace_back(cr);
    }
    else {
        _nonPlayerCritters.emplace_back(cr);
    }

    AddCritterToField(cr);
}

void Map::EraseCritter(Critter* cr)
{
    STACK_TRACE_ENTRY();

    {
        const auto it = _crittersMap.find(cr->GetId());
        RUNTIME_ASSERT(it != _crittersMap.end());
        _crittersMap.erase(it);
    }

    {
        const auto it = std::find(_critters.begin(), _critters.end(), cr);
        RUNTIME_ASSERT(it != _critters.end());
        _critters.erase(it);
    }

    if (cr->GetIsControlledByPlayer()) {
        const auto it = std::find(_playerCritters.begin(), _playerCritters.end(), cr);
        RUNTIME_ASSERT(it != _playerCritters.end());
        _playerCritters.erase(it);
    }
    else {
        const auto it = std::find(_nonPlayerCritters.begin(), _nonPlayerCritters.end(), cr);
        RUNTIME_ASSERT(it != _nonPlayerCritters.end());
        _nonPlayerCritters.erase(it);
    }

    RemoveCritterFromField(cr);
}

void Map::AddCritterToField(Critter* cr)
{
    STACK_TRACE_ENTRY();

    const auto hex = cr->GetHex();
    RUNTIME_ASSERT(_mapSize.IsValidPos(hex));

    auto& field = _hexField.GetCellForWriting(hex);

    RUNTIME_ASSERT(std::find(field.Critters.begin(), field.Critters.end(), cr) == field.Critters.end());
    field.Critters.emplace_back(cr);

    RecacheHexFlags(field);

    SetMultihexCritter(cr, true);
}

void Map::RemoveCritterFromField(Critter* cr)
{
    STACK_TRACE_ENTRY();

    const auto hex = cr->GetHex();
    RUNTIME_ASSERT(_mapSize.IsValidPos(hex));

    auto& field = _hexField.GetCellForWriting(hex);

    const auto it = std::find(field.Critters.begin(), field.Critters.end(), cr);
    RUNTIME_ASSERT(it != field.Critters.end());
    field.Critters.erase(it);

    RecacheHexFlags(field);

    SetMultihexCritter(cr, false);
}

void Map::SetMultihexCritter(Critter* cr, bool set)
{
    STACK_TRACE_ENTRY();

    const uint multihex = cr->GetMultihex();

    if (multihex != 0) {
        const auto hex = cr->GetHex();
        auto&& [sx, sy] = _engine->Geometry.GetHexOffsets(hex);

        for (uint i = 0, j = GenericUtils::NumericalNumber(multihex) * GameSettings::MAP_DIR_COUNT; i < j; i++) {
            const auto raw_mh_hex = ipos {hex.x + sx[i], hex.y + sy[i]};

            if (_mapSize.IsValidPos(raw_mh_hex)) {
                const auto mh_hex = _mapSize.FromRawPos(raw_mh_hex);
                auto& field = _hexField.GetCellForWriting(mh_hex);

                if (set) {
                    RUNTIME_ASSERT(std::find(field.MultihexCritters.begin(), field.MultihexCritters.end(), cr) == field.MultihexCritters.end());
                    field.MultihexCritters.emplace_back(cr);
                }
                else {
                    const auto it = std::find(field.MultihexCritters.begin(), field.MultihexCritters.end(), cr);
                    RUNTIME_ASSERT(it != field.MultihexCritters.end());
                    field.MultihexCritters.erase(it);
                }

                RecacheHexFlags(field);
            }
        }
    }
}

void Map::AddItem(Item* item, mpos hex, Critter* dropper)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(item);
    RUNTIME_ASSERT(!item->GetIsStatic());
    RUNTIME_ASSERT(_mapSize.IsValidPos(hex));

    SetItem(item, hex);

    auto item_ids = GetItemIds();
    RUNTIME_ASSERT(std::find(item_ids.begin(), item_ids.end(), item->GetId()) == item_ids.end());
    item_ids.emplace_back(item->GetId());
    SetItemIds(std::move(item_ids));

    // Process critters view
    for (auto* cr : copy_hold_ref(GetCritters())) {
        if (cr->IsDestroyed()) {
            continue;
        }

        if (!item->GetIsHidden() || item->GetIsAlwaysView()) {
            if (!item->GetIsAlwaysView()) {
                bool allowed;

                if (item->GetIsTrap() && IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_ITEM_SCRIPT)) {
                    allowed = _engine->OnMapCheckTrapLook.Fire(this, cr, item);
                }
                else {
                    int dist = static_cast<int>(GeometryHelper::DistGame(cr->GetHex(), hex));
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

void Map::SetItem(Item* item, mpos hex)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!_itemsMap.count(item->GetId()));
    RUNTIME_ASSERT(_mapSize.IsValidPos(hex));

    item->SetOwnership(ItemOwnership::MapHex);
    item->SetMapId(GetId());
    item->SetHex(hex);

    _items.emplace_back(item);
    _itemsMap.emplace(item->GetId(), item);

    auto& field = _hexField.GetCellForWriting(hex);

    field.Items.emplace_back(item);

    if (item->GetIsGeck()) {
        _mapLocation->GeckCount++;
    }

    RecacheHexFlags(field);

    if (item->IsNonEmptyBlockLines()) {
        GeometryHelper::ForEachBlockLines(item->GetBlockLines(), hex, _mapSize, [this, item](mpos block_hex) {
            auto& block_field = _hexField.GetCellForWriting(block_hex);
            block_field.BlockLines.emplace_back(item);
            RecacheHexFlags(block_field);
        });
    }
}

void Map::EraseItem(ident_t item_id)
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

    {
        const auto it = std::find(_items.begin(), _items.end(), item);
        RUNTIME_ASSERT(it != _items.end());
        _items.erase(it);
    }

    const auto hex = item->GetHex();
    auto& field = _hexField.GetCellForWriting(hex);

    {
        const auto it = std::find(field.Items.begin(), field.Items.end(), item);
        RUNTIME_ASSERT(it != field.Items.end());
        field.Items.erase(it);
    }

    item->SetOwnership(ItemOwnership::Nowhere);
    item->SetMapId(ident_t {});
    item->SetHex({});

    {
        auto item_ids = GetItemIds();
        const auto item_id_it = std::find(item_ids.begin(), item_ids.end(), item->GetId());
        RUNTIME_ASSERT(item_id_it != item_ids.end());
        item_ids.erase(item_id_it);
        SetItemIds(std::move(item_ids));
    }

    if (item->GetIsGeck()) {
        _mapLocation->GeckCount--;
    }

    RecacheHexFlags(field);

    if (item->IsNonEmptyBlockLines()) {
        GeometryHelper::ForEachBlockLines(item->GetBlockLines(), hex, _mapSize, [this, item](mpos block_hex) {
            auto& block_field = _hexField.GetCellForWriting(block_hex);
            const auto it = std::find(block_field.BlockLines.begin(), block_field.BlockLines.end(), item);
            RUNTIME_ASSERT(it != block_field.BlockLines.end());
            block_field.BlockLines.erase(it);
            RecacheHexFlags(block_field);
        });
    }

    // Process critters view
    for (auto* cr : copy_hold_ref(GetCritters())) {
        if (cr->IsDestroyed()) {
            continue;
        }

        if (cr->DelIdVisItem(item->GetId())) {
            cr->Send_EraseItemFromMap(item);
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

auto Map::IsHexMovable(mpos hex) const -> bool
{
    STACK_TRACE_ENTRY();

    const auto& field = _hexField.GetCellForReading(hex);
    const auto& static_field = _staticMap->HexField.GetCellForReading(hex);

    return !field.IsMoveBlocked && !static_field.IsMoveBlocked;
}

auto Map::IsHexShootable(mpos hex) const -> bool
{
    STACK_TRACE_ENTRY();

    const auto& field = _hexField.GetCellForReading(hex);
    const auto& static_field = _staticMap->HexField.GetCellForReading(hex);

    return !field.IsShootBlocked && !static_field.IsShootBlocked;
}

auto Map::IsHexesMovable(mpos hex, uint radius) const -> bool
{
    STACK_TRACE_ENTRY();

    // Base
    if (!IsHexMovable(hex)) {
        return false;
    }
    if (radius == 0) {
        return true;
    }

    // Neighbors
    const auto [sx, sy] = _engine->Geometry.GetHexOffsets(hex);
    const auto count = GenericUtils::NumericalNumber(radius) * GameSettings::MAP_DIR_COUNT;

    for (uint i = 0; i < count; i++) {
        const auto raw_check_hex = ipos {hex.x + sx[i], hex.y + sy[i]};

        if (_mapSize.IsValidPos(raw_check_hex)) {
            if (!IsHexMovable(_mapSize.FromRawPos(raw_check_hex))) {
                return false;
            }
        }
    }

    return true;
}

auto Map::IsHexesMovable(mpos hex, uint radius, Critter* skip_cr) -> bool
{
    STACK_TRACE_ENTRY();

    if (_engine->Settings.CritterBlockHex) {
        if (skip_cr != nullptr && !skip_cr->IsDead()) {
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
    STACK_TRACE_ENTRY();

    for (auto* cr : copy_hold_ref(GetCritters())) {
        if (cr->IsDestroyed()) {
            continue;
        }

        if (cr->CountIdVisItem(item->GetId())) {
            if (item->GetIsHidden()) {
                cr->DelIdVisItem(item->GetId());
                cr->Send_EraseItemFromMap(item);
                cr->OnItemOnMapDisappeared.Fire(item, nullptr);
            }
            else if (!item->GetIsAlwaysView()) // Check distance for non-hide items
            {
                bool allowed;

                if (item->GetIsTrap() && IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_ITEM_SCRIPT)) {
                    allowed = _engine->OnMapCheckTrapLook.Fire(this, cr, item);
                }
                else {
                    int dist = static_cast<int>(GeometryHelper::DistGame(cr->GetHex(), item->GetHex()));
                    if (item->GetIsTrap()) {
                        dist += item->GetTrapValue();
                    }

                    allowed = dist <= static_cast<int>(cr->GetLookDistance());
                }

                if (!allowed) {
                    cr->DelIdVisItem(item->GetId());
                    cr->Send_EraseItemFromMap(item);
                    cr->OnItemOnMapDisappeared.Fire(item, nullptr);
                }
            }
        }
        else if (!item->GetIsHidden() || item->GetIsAlwaysView()) {
            if (!item->GetIsAlwaysView()) // Check distance for non-hide items
            {
                bool allowed;

                if (item->GetIsTrap() && IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_ITEM_SCRIPT)) {
                    allowed = _engine->OnMapCheckTrapLook.Fire(this, cr, item);
                }
                else {
                    int dist = static_cast<int>(GeometryHelper::DistGame(cr->GetHex(), item->GetHex()));
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

auto Map::IsBlockItem(mpos hex) const -> bool
{
    STACK_TRACE_ENTRY();

    const auto& field = _hexField.GetCellForReading(hex);

    return field.IsNoMoveItem;
}

auto Map::IsItemTrigger(mpos hex) const -> bool
{
    STACK_TRACE_ENTRY();

    const auto& field = _hexField.GetCellForReading(hex);

    return field.IsTriggerItem;
}

auto Map::IsItemGag(mpos hex) const -> bool
{
    STACK_TRACE_ENTRY();

    const auto& field = _hexField.GetCellForReading(hex);

    return field.IsGagItem;
}

auto Map::GetItem(ident_t item_id) -> Item*
{
    STACK_TRACE_ENTRY();

    const auto it = _itemsMap.find(item_id);
    return it != _itemsMap.end() ? it->second : nullptr;
}

auto Map::GetItemHex(mpos hex, hstring item_pid, Critter* picker) -> Item*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto& field = _hexField.GetCellForReading(hex);

    for (auto* item : field.Items) {
        if ((!item_pid || item->GetProtoId() == item_pid) && (picker == nullptr || (!item->GetIsHidden() && picker->CountIdVisItem(item->GetId())))) {
            return item;
        }
    }

    return nullptr;
}

auto Map::GetItemGag(mpos hex) -> Item*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto& field = _hexField.GetCellForReading(hex);

    for (auto* item : field.Items) {
        if (item->GetIsGag()) {
            return item;
        }
    }

    return nullptr;
}

auto Map::GetItems() -> const vector<Item*>&
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    return _items;
}

auto Map::GetItems(mpos hex) -> const vector<Item*>&
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto& field = _hexField.GetCellForReading(hex);

    return field.Items;
}

auto Map::GetItemsInRadius(mpos hex, uint radius, hstring pid) -> vector<Item*>
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    vector<Item*> items;

    // Todo: optimize iterms radius search by using GetHexOffsets
    for (auto* item : _items) {
        if ((!pid || item->GetProtoId() == pid) && GeometryHelper::CheckDist(item->GetHex(), hex, radius)) {
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

auto Map::GetItemsTrigger(mpos hex) -> vector<Item*>
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto& field = _hexField.GetCellForReading(hex);
    vector<Item*> triggers;
    triggers.reserve(field.Items.size());

    for (auto* item : field.Items) {
        if (item->GetIsTrigger() || item->GetIsTrap()) {
            triggers.emplace_back(item);
        }
    }

    return triggers;
}

auto Map::IsPlaceForProtoItem(mpos hex, const ProtoItem* proto_item) const -> bool
{
    STACK_TRACE_ENTRY();

    if (!IsHexMovable(hex)) {
        return false;
    }

    bool is_critter = false;
    GeometryHelper::ForEachBlockLines(proto_item->GetBlockLines(), hex, _mapSize, [this, &is_critter](mpos block_hex) { is_critter |= IsAnyCritter(block_hex); });
    return !is_critter;
}

void Map::RecacheHexFlags(mpos hex)
{
    STACK_TRACE_ENTRY();

    auto& field = _hexField.GetCellForWriting(hex);

    RecacheHexFlags(field);
}

void Map::RecacheHexFlags(Field& field)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    field.IsNonDeadCritter = false;
    field.IsDeadCritter = false;
    field.IsGagItem = false;
    field.IsTriggerItem = false;
    field.IsNoMoveItem = false;
    field.IsNoShootItem = false;

    if (!field.Critters.empty()) {
        for (const auto* cr : field.Critters) {
            if (cr->IsDead()) {
                field.IsDeadCritter = true;
            }
            else {
                field.IsNonDeadCritter = true;
            }

            if (field.IsDeadCritter && field.IsNonDeadCritter) {
                break;
            }
        }
    }

    if (!field.MultihexCritters.empty()) {
        for (const auto* cr : field.MultihexCritters) {
            if (cr->IsDead()) {
                field.IsDeadCritter = true;
            }
            else {
                field.IsNonDeadCritter = true;
            }

            if (field.IsDeadCritter && field.IsNonDeadCritter) {
                break;
            }
        }
    }

    if (!field.Items.empty()) {
        for (const auto* item : field.Items) {
            if (!field.IsNoMoveItem && !item->GetIsNoBlock()) {
                field.IsNoMoveItem = true;
            }
            if (!field.IsNoShootItem && !item->GetIsShootThru()) {
                field.IsNoShootItem = true;
            }
            if (!field.IsGagItem && item->GetIsGag()) {
                field.IsGagItem = true;
            }
            if (!field.IsTriggerItem && (item->GetIsTrigger() || item->GetIsTrap())) {
                field.IsTriggerItem = true;
            }

            if (field.IsNoMoveItem && field.IsNoShootItem && field.IsGagItem && field.IsTriggerItem) {
                break;
            }
        }
    }

    if (!field.BlockLines.empty() && (!field.IsNoMoveItem || !field.IsNoShootItem)) {
        field.IsNoMoveItem = true;

        for (const auto* item : field.BlockLines) {
            if (!item->GetIsShootThru()) {
                field.IsNoShootItem = true;
                break;
            }
        }
    }

    const auto is_critter_block = _engine->Settings.CritterBlockHex && field.IsNonDeadCritter;

    field.IsShootBlocked = field.IsNoShootItem || (field.ManualBlock && field.ManualBlockFull);
    field.IsMoveBlocked = field.IsShootBlocked || field.IsNoMoveItem || is_critter_block || field.ManualBlock;
}

void Map::SetHexManualBlock(mpos hex, bool enable, bool full)
{
    STACK_TRACE_ENTRY();

    auto& field = _hexField.GetCellForWriting(hex);

    field.ManualBlock = enable;
    field.ManualBlockFull = full;

    RecacheHexFlags(field);
}

auto Map::IsAnyCritter(mpos hex) const -> bool
{
    STACK_TRACE_ENTRY();

    const auto& field = _hexField.GetCellForReading(hex);

    return field.IsNonDeadCritter || field.IsDeadCritter;
}

auto Map::IsNonDeadCritter(mpos hex) const -> bool
{
    STACK_TRACE_ENTRY();

    const auto& field = _hexField.GetCellForReading(hex);

    return field.IsNonDeadCritter;
}

auto Map::IsDeadCritter(mpos hex) const -> bool
{
    STACK_TRACE_ENTRY();

    const auto& field = _hexField.GetCellForReading(hex);

    return field.IsDeadCritter;
}

auto Map::IsCritter(mpos hex, const Critter* cr) const -> bool
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(cr);

    const auto& field = _hexField.GetCellForReading(hex);

    if (field.IsNonDeadCritter || field.IsDeadCritter) {
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

auto Map::GetCritter(ident_t cr_id) -> Critter*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (const auto it = _crittersMap.find(cr_id); it != _crittersMap.end()) {
        return it->second;
    }

    return nullptr;
}

auto Map::GetNonDeadCritter(mpos hex) -> Critter*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto& field = _hexField.GetCellForReading(hex);

    if (field.IsNonDeadCritter) {
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

auto Map::GetDeadCritter(mpos hex) -> Critter*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto& field = _hexField.GetCellForReading(hex);

    if (field.IsDeadCritter) {
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

auto Map::GetCritters(mpos hex, uint radius, CritterFindType find_type) -> vector<Critter*>
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    vector<Critter*> critters;
    critters.reserve(_critters.size());

    // Todo: optimize critters radius search by using GetHexOffsets
    for (auto* cr : _critters) {
        if (cr->CheckFind(find_type) && GeometryHelper::CheckDist(hex, cr->GetHex(), radius + cr->GetMultihex())) {
            critters.emplace_back(cr);
        }
    }

    return critters;
}

auto Map::GetCritters() -> const vector<Critter*>&
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    return _critters;
}

auto Map::GetPlayerCritters() -> const vector<Critter*>&
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    return _playerCritters;
}

auto Map::GetNonPlayerCritters() -> const vector<Critter*>&
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    return _nonPlayerCritters;
}

auto Map::GetCrittersCount() const -> uint
{
    STACK_TRACE_ENTRY();

    return static_cast<uint>(_critters.size());
}

auto Map::GetPlayerCrittersCount() const -> uint
{
    STACK_TRACE_ENTRY();

    return static_cast<uint>(_playerCritters.size());
}

auto Map::GetNonPlayerCrittersCount() const -> uint
{
    STACK_TRACE_ENTRY();

    return static_cast<uint>(_nonPlayerCritters.size());
}

void Map::SendEffect(hstring eff_pid, mpos hex, uint16 radius)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    for (auto* cr : _playerCritters) {
        if (GeometryHelper::CheckDist(cr->GetHex(), hex, cr->GetLookDistance() + radius)) {
            cr->Send_Effect(eff_pid, hex, radius);
        }
    }
}

void Map::SendFlyEffect(hstring eff_pid, ident_t from_cr_id, ident_t to_cr_id, mpos from_hex, mpos to_hex)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    for (auto* cr : _playerCritters) {
        if (GenericUtils::IntersectCircleLine(cr->GetHex().x, cr->GetHex().y, static_cast<int>(cr->GetLookDistance()), from_hex.x, from_hex.y, to_hex.x, to_hex.y)) {
            cr->Send_FlyEffect(eff_pid, from_cr_id, to_cr_id, from_hex, to_hex);
        }
    }
}

void Map::SetText(mpos hex, ucolor color, string_view text, bool unsafe_text)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    for (auto* cr : _playerCritters) {
        if (cr->GetLookDistance() >= GeometryHelper::DistGame(hex, cr->GetHex())) {
            cr->Send_MapText(hex, color, text, unsafe_text);
        }
    }
}

void Map::SetTextMsg(mpos hex, ucolor color, TextPackName text_pack, TextPackKey str_num)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    for (auto* cr : _playerCritters) {
        if (cr->GetLookDistance() >= GeometryHelper::DistGame(hex, cr->GetHex())) {
            cr->Send_MapTextMsg(hex, color, text_pack, str_num);
        }
    }
}

void Map::SetTextMsgLex(mpos hex, ucolor color, TextPackName text_pack, TextPackKey str_num, string_view lexems)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    for (auto* cr : _playerCritters) {
        if (cr->GetLookDistance() >= GeometryHelper::DistGame(hex, cr->GetHex())) {
            cr->Send_MapTextMsgLex(hex, color, text_pack, str_num, lexems);
        }
    }
}

auto Map::IsStaticItemTrigger(mpos hex) const -> bool
{
    STACK_TRACE_ENTRY();

    const auto& static_field = _staticMap->HexField.GetCellForReading(hex);

    return !static_field.TriggerItems.empty();
}

auto Map::GetStaticItem(ident_t id) -> StaticItem*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (const auto it = _staticMap->StaticItemsById.find(id); it != _staticMap->StaticItemsById.end()) {
        return it->second;
    }

    return nullptr;
}

auto Map::GetStaticItem(mpos hex, hstring pid) -> StaticItem*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto& static_field = _staticMap->HexField.GetCellForReading(hex);

    for (auto* item : static_field.StaticItems) {
        if (!pid || item->GetProtoId() == pid) {
            return item;
        }
    }
    return nullptr;
}

auto Map::GetStaticItemsHex(mpos hex) -> vector<StaticItem*>
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto& static_field = _staticMap->HexField.GetCellForReading(hex);

    return static_field.StaticItems;
}

auto Map::GetStaticItemsHexEx(mpos hex, uint radius, hstring pid) -> vector<StaticItem*>
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    vector<StaticItem*> items;

    for (auto* item : _staticMap->StaticItems) {
        if ((!pid || item->GetProtoId() == pid) && GeometryHelper::DistGame(item->GetHex(), hex) <= radius) {
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

auto Map::GetStaticItemsTrigger(mpos hex) -> vector<StaticItem*>
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto& static_field = _staticMap->HexField.GetCellForReading(hex);

    return static_field.TriggerItems;
}
