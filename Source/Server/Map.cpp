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

Map::Map(FOServer* engine, ident_t id, const ProtoMap* proto, Location* location, StaticMap* static_map, const Properties* props) noexcept :
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

    if (IsHexesMovable(hex, multihex) && !(skip_unsafe && (IsTriggerStaticItemOnHex(hex) || IsTriggerItemOnHex(hex)))) {
        return hex;
    }
    if (seek_radius == 0) {
        return std::nullopt;
    }

    const auto hexes_around = GeometryHelper::HexesInRadius(seek_radius);
    int32 pos = GenericUtils::Random(1, hexes_around);
    int32 iterations = 0;

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
    FO_RUNTIME_ASSERT(_mapSize.is_valid_pos(hex));

    auto& field = _hexField->GetCellForWriting(hex);

    vec_add_unique_value(field.Critters, cr);
    RecacheHexFlags(field);
    SetMultihexCritter(cr, true);
}

void Map::RemoveCritterFromField(Critter* cr)
{
    FO_STACK_TRACE_ENTRY();

    const auto hex = cr->GetHex();
    FO_RUNTIME_ASSERT(_mapSize.is_valid_pos(hex));

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
        const auto hexes_around = GeometryHelper::HexesInRadius(multihex);

        for (int32 i = 1; i < hexes_around; i++) {
            if (auto hex_around = hex; GeometryHelper::MoveHexAroundAway(hex_around, i, _mapSize)) {
                auto& field = _hexField->GetCellForWriting(hex_around);

                if (set) {
                    vec_add_unique_value(field.Critters, cr);
                }
                else {
                    vec_remove_unique_value(field.Critters, cr);
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
    FO_RUNTIME_ASSERT(_mapSize.is_valid_pos(hex));

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

    if (item->IsNonEmptyMultihexLines() || item->IsNonEmptyMultihexMesh()) {
        vector<mpos> multihex_entries;
        const auto multihex_lines = item->GetMultihexLines();
        const auto multihex_mesh = item->GetMultihexMesh();
        multihex_entries.reserve(multihex_lines.size() + multihex_mesh.size());

        GeometryHelper::ForEachMultihexLines(multihex_lines, hex, _mapSize, [&](mpos multihex) {
            auto& multihex_field = _hexField->GetCellForWriting(multihex);

            if (vec_safe_add_unique_value(multihex_field.Items, item)) {
                RecacheHexFlags(multihex_field);
                multihex_entries.emplace_back(multihex);
            }
        });

        for (const auto multihex : multihex_mesh) {
            if (multihex != hex && _mapSize.is_valid_pos(multihex)) {
                auto& multihex_field = _hexField->GetCellForWriting(multihex);

                if (vec_safe_add_unique_value(multihex_field.Items, item)) {
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

    Item* item;

    {
        FO_RUNTIME_ASSERT(item_id);
        const auto it = _itemsMap.find(item_id);
        FO_RUNTIME_ASSERT(it != _itemsMap.end());
        item = it->second.get();
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

    if (item->HasMultihexEntries()) {
        for (const auto multihex : item->GetMultihexEntries()) {
            auto& multihex_field = _hexField->GetCellForWriting(multihex);
            vec_remove_unique_value(multihex_field.Items, item);
            RecacheHexFlags(multihex_field);
        }

        item->SetMultihexEntries({});
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

        for (auto& cr : GetCritters()) {
            cr->Send_Property(type, prop, entity);
        }
    }
    else if (type == NetProperty::MapItem) {
        auto* item = dynamic_cast<Item*>(entity);
        FO_RUNTIME_ASSERT(item);

        for (auto* cr : copy_hold_ref(GetCritters())) {
            if (cr->CheckVisibleItem(item->GetId())) {
                cr->Send_Property(type, prop, entity);
                cr->OnItemOnMapChanged.Fire(item);
            }
        }
    }
    else {
        FO_UNREACHABLE_PLACE();
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

    const auto hexes_around = GeometryHelper::HexesInRadius(radius);

    for (int32 i = 0; i < hexes_around; i++) {
        if (auto check_hex = hex; GeometryHelper::MoveHexAroundAway(check_hex, i, _mapSize)) {
            if (!IsHexMovable(check_hex)) {
                return false;
            }
        }
    }

    return true;
}

auto Map::IsHexesMovable(mpos hex, int32 radius, Critter* ignore_cr) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_engine->Settings.CritterBlockHex) {
        if (ignore_cr != nullptr && !ignore_cr->IsDead()) {
            // Todo: make movable checks without critter removing
            RemoveCritterFromField(ignore_cr);
            const auto result = IsHexesMovable(hex, radius);
            AddCritterToField(ignore_cr);
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

auto Map::IsBlockItemOnHex(mpos hex) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hex);

    return field.HasNoMoveItem;
}

auto Map::IsTriggerItemOnHex(mpos hex) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hex);

    return field.HasTriggerItem;
}

auto Map::IsGagItemOnHex(mpos hex) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hex);

    return field.HasGagItem;
}

auto Map::GetItem(ident_t item_id) noexcept -> Item*
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto it = _itemsMap.find(item_id);

    return it != _itemsMap.end() ? it->second.get() : nullptr;
}

auto Map::GetItemOnHex(mpos hex, hstring item_pid, Critter* picker) -> Item*
{
    FO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hex);

    if (field.Items.empty()) {
        return nullptr;
    }

    auto& field2 = _hexField->GetCellForWriting(hex);

    for (auto& item : field2.Items) {
        if ((!item_pid || item->GetProtoId() == item_pid) && (picker == nullptr || (!item->GetHidden() && picker->CheckVisibleItem(item->GetId())))) {
            return item.get();
        }
    }

    return nullptr;
}

auto Map::GetGagItemOnHex(mpos hex) const noexcept -> const Item*
{
    FO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hex);

    if (field.Items.empty()) {
        return nullptr;
    }

    for (const auto& item : field.Items) {
        if (item->GetIsGag()) {
            return item.get();
        }
    }

    return nullptr;
}

auto Map::GetItemsOnHex(mpos hex) noexcept -> span<raw_ptr<Item>>
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hex);

    if (field.Items.empty()) {
        return {};
    }

    auto& field2 = _hexField->GetCellForWriting(hex);
    return field2.Items;
}

auto Map::GetItemsInRadius(mpos hex, int32 radius) -> vector<raw_ptr<Item>>
{
    FO_STACK_TRACE_ENTRY();

    const auto hexes_around = GeometryHelper::HexesInRadius(radius);

    vector<raw_ptr<Item>> items;
    items.reserve(numeric_cast<size_t>(hexes_around) * 2);

    for (int32 i = 0; i < hexes_around; i++) {
        if (mpos next_hex = hex; GeometryHelper::MoveHexAroundAway(next_hex, i, _mapSize)) {
            const auto& field = _hexField->GetCellForReading(next_hex);

            if (!field.Items.empty()) {
                auto& field2 = _hexField->GetCellForWriting(hex);

                for (auto& item : field2.Items) {
                    vec_safe_add_unique_value(items, item.get());
                }
            }
        }
    }

    return items;
}

auto Map::GetTriggerItemsOnHex(mpos hex) -> vector<Item*>
{
    FO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hex);

    if (field.Items.empty()) {
        return {};
    }

    auto& field2 = _hexField->GetCellForWriting(hex);
    vector<Item*> triggers;

    for (auto& item : field2.Items) {
        if (item->GetIsTrigger() || item->GetIsTrap()) {
            triggers.emplace_back(item.get());
        }
    }

    return triggers;
}

auto Map::IsValidPlaceForItem(mpos hex, const ProtoItem* proto_item) const -> bool
{
    FO_STACK_TRACE_ENTRY();

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

    auto& field = _hexField->GetCellForWriting(hex);

    RecacheHexFlags(field);
}

void Map::RecacheHexFlags(Field& field)
{
    FO_STACK_TRACE_ENTRY();

    field.HasCritter = false;
    field.HasBlockCritter = false;
    field.HasGagItem = false;
    field.HasTriggerItem = false;
    field.HasNoMoveItem = false;
    field.HasNoShootItem = false;

    field.HasCritter = !field.Critters.empty();

    if (_engine->Settings.CritterBlockHex && field.HasCritter) {
        field.HasBlockCritter = std::ranges::any_of(field.Critters, [](auto&& cr) { return !cr->IsDead(); });
    }

    if (!field.Items.empty()) {
        for (const auto& item : field.Items) {
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

auto Map::IsCritterOnHex(mpos hex, CritterFindType find_type) const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hex);

    if (field.HasCritter) {
        if (find_type == CritterFindType::Any) {
            return true;
        }

        for (const auto& cr : field.Critters) {
            if (cr->CheckFind(find_type)) {
                return true;
            }
        }
    }

    return false;
}

auto Map::IsCritterOnHex(mpos hex, const Critter* cr) const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(cr);

    const auto& field = _hexField->GetCellForReading(hex);

    if (field.HasCritter) {
        if (vec_exists(field.Critters, raw_ptr(cr))) {
            return true;
        }
    }

    return false;
}

auto Map::GetCritter(ident_t cr_id) noexcept -> Critter*
{
    FO_NO_STACK_TRACE_ENTRY();

    if (const auto it = _crittersMap.find(cr_id); it != _crittersMap.end()) {
        return it->second.get();
    }

    return nullptr;
}

auto Map::GetCritterOnHex(mpos hex, CritterFindType find_type) noexcept -> Critter*
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto& field = _hexField->GetCellForReading(hex);

    if (field.HasCritter) {
        auto& field2 = _hexField->GetCellForWriting(hex);

        for (auto& cr : field2.Critters) {
            if (cr->CheckFind(find_type)) {
                return cr.get();
            }
        }
    }

    return nullptr;
}

auto Map::GetCrittersOnHex(mpos hex, CritterFindType find_type) -> vector<Critter*>
{
    FO_STACK_TRACE_ENTRY();

    vector<Critter*> critters;
    const auto& field = _hexField->GetCellForReading(hex);

    if (field.HasCritter) {
        auto& field2 = _hexField->GetCellForWriting(hex);

        for (auto& cr : field2.Critters) {
            if (cr->CheckFind(find_type)) {
                critters.emplace_back(cr.get());
            }
        }
    }

    return critters;
}

auto Map::GetCrittersInRadius(mpos hex, int32 radius, CritterFindType find_type) -> vector<Critter*>
{
    FO_STACK_TRACE_ENTRY();

    vector<Critter*> critters;

    // Todo: optimize items radius search by checking directly hexes in radius
    for (auto& cr : _critters) {
        if (cr->CheckFind(find_type) && GeometryHelper::CheckDist(hex, cr->GetHex(), radius + cr->GetMultihex())) {
            critters.emplace_back(cr.get());
        }
    }

    return critters;
}

auto Map::IsTriggerStaticItemOnHex(mpos hex) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto& static_field = _staticMap->HexField->GetCellForReading(hex);

    return !static_field.TriggerItems.empty();
}

auto Map::GetStaticItem(ident_t id) noexcept -> StaticItem*
{
    FO_NO_STACK_TRACE_ENTRY();

    if (const auto it = _staticMap->StaticItemsById.find(id); it != _staticMap->StaticItemsById.end()) {
        return it->second.get();
    }

    return nullptr;
}

auto Map::GetStaticItemOnHex(mpos hex, hstring pid) noexcept -> StaticItem*
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto& static_field = _staticMap->HexField->GetCellForReading(hex);

    if (static_field.StaticItems.empty()) {
        return nullptr;
    }

    for (auto& item : _staticMap->HexField->GetCellForWriting(hex).StaticItems) {
        if (!pid || item->GetProtoId() == pid) {
            return item.get();
        }
    }

    return nullptr;
}

auto Map::GetStaticItems(hstring pid) -> vector<StaticItem*>
{
    FO_STACK_TRACE_ENTRY();

    vector<StaticItem*> items;

    for (auto& item : _staticMap->StaticItems) {
        if (!pid || item->GetProtoId() == pid) {
            items.emplace_back(item.get());
        }
    }

    return items;
}

auto Map::GetStaticItemsOnHex(mpos hex) noexcept -> span<raw_ptr<StaticItem>>
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto& static_field = _staticMap->HexField->GetCellForReading(hex);

    if (static_field.StaticItems.empty()) {
        return {};
    }

    return _staticMap->HexField->GetCellForWriting(hex).StaticItems;
}

auto Map::GetStaticItemsInRadius(mpos hex, int32 radius, hstring pid) -> vector<StaticItem*>
{
    FO_STACK_TRACE_ENTRY();

    vector<StaticItem*> items;

    // Todo: optimize items radius search by checking directly hexes in radius
    for (auto& item : _staticMap->StaticItems) {
        if ((!pid || item->GetProtoId() == pid) && GeometryHelper::GetDistance(item->GetHex(), hex) <= radius) {
            items.emplace_back(item.get());
        }
    }

    return items;
}

auto Map::GetTriggerStaticItemsOnHex(mpos hex) noexcept -> span<raw_ptr<StaticItem>>
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto& static_field = _staticMap->HexField->GetCellForReading(hex);

    if (static_field.TriggerItems.empty()) {
        return {};
    }

    return _staticMap->HexField->GetCellForWriting(hex).TriggerItems;
}

auto Map::IsOutsideArea(mpos hex) const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    const irect32 scroll_area = GetScrollAxialArea();

    if (!scroll_area.is_zero()) {
        const ipos32 axial_hex = _engine->Geometry.GetHexAxialCoord(hex);

        if (axial_hex.x < scroll_area.x || axial_hex.x > scroll_area.x + scroll_area.width || //
            axial_hex.y < scroll_area.y || axial_hex.y > scroll_area.y + scroll_area.height) {
            return true;
        }
    }

    return false;
}

FO_END_NAMESPACE();
