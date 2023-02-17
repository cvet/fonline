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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

Map::Map(FOServer* engine, ident_t id, const ProtoMap* proto, Location* location, const StaticMap* static_map) :
    ServerEntity(engine, id, engine->GetPropertyRegistrator(ENTITY_CLASS_NAME)),
    EntityWithProto(this, proto),
    MapProperties(GetInitRef()),
    _staticMap {static_map},
    _mapLocation {location}
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_staticMap);

    _name = _str("{}_{}", proto->GetName(), id);

    _hexFlagsSize = GetWidth() * GetHeight();
    _hexFlags = new uchar[_hexFlagsSize];
    std::memset(_hexFlags, 0, _hexFlagsSize);
}

Map::~Map()
{
    STACK_TRACE_ENTRY();

    delete[] _hexFlags;
}

void Map::Process()
{
    STACK_TRACE_ENTRY();

    const auto tick = _engine->GameTime.GameTick();
    ProcessLoop(0, GetLoopTime1(), tick);
    ProcessLoop(1, GetLoopTime2(), tick);
    ProcessLoop(2, GetLoopTime3(), tick);
    ProcessLoop(3, GetLoopTime4(), tick);
    ProcessLoop(4, GetLoopTime5(), tick);
}

void Map::ProcessLoop(int index, uint time, uint tick)
{
    STACK_TRACE_ENTRY();

    if (time != 0u && tick - _loopLastTick[index] >= time) {
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

auto Map::FindStartHex(ushort hx, ushort hy, uint multihex, uint seek_radius, bool skip_unsafe) const -> optional<tuple<ushort, ushort>>
{
    STACK_TRACE_ENTRY();

    if (IsHexesPassed(hx, hy, multihex) && !(skip_unsafe && (IsHexStaticTrigger(hx, hy) || IsHexTrigger(hx, hy)))) {
        return tuple {hx, hy};
    }
    if (seek_radius == 0u) {
        return std::nullopt;
    }
    if (seek_radius > MAX_HEX_OFFSET) {
        seek_radius = MAX_HEX_OFFSET;
    }

    const auto [sx, sy] = _engine->Geometry.GetHexOffsets((hx % 2) != 0);
    auto hx_ = static_cast<short>(hx);
    auto hy_ = static_cast<short>(hy);
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

        const auto nx = static_cast<short>(hx_) + sx[pos];
        const auto ny = static_cast<short>(hy_) + sy[pos];

        if (nx < 0 || nx >= GetWidth() || ny < 0 || ny >= GetHeight()) {
            continue;
        }

        const auto nx_ = static_cast<ushort>(nx);
        const auto ny_ = static_cast<ushort>(ny);

        if (!IsHexesPassed(nx_, ny_, multihex)) {
            continue;
        }
        if (skip_unsafe && (IsHexStaticTrigger(nx_, ny_) || IsHexTrigger(nx_, ny_))) {
            continue;
        }

        break;
    }

    hx_ += sx[pos];
    hy_ += sy[pos];
    return tuple {static_cast<ushort>(hx_), static_cast<ushort>(hy_)};
}

void Map::AddCritter(Critter* cr)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_crittersMap.count(cr->GetId()) == 0);

    _crittersMap.emplace(cr->GetId(), cr);
    _critters.push_back(cr);

    if (cr->IsOwnedByPlayer()) {
        _playerCritters.push_back(cr);
    }
    if (cr->IsNpc()) {
        _nonPlayerCritters.push_back(cr);
    }

    SetFlagCritter(cr->GetHexX(), cr->GetHexY(), cr->GetMultihex(), cr->IsDead());
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

    if (cr->IsOwnedByPlayer()) {
        const auto it = std::find(_playerCritters.begin(), _playerCritters.end(), cr);
        RUNTIME_ASSERT(it != _playerCritters.end());
        _playerCritters.erase(it);
    }
    else {
        const auto it = std::find(_nonPlayerCritters.begin(), _nonPlayerCritters.end(), cr);
        RUNTIME_ASSERT(it != _nonPlayerCritters.end());
        _nonPlayerCritters.erase(it);
    }
}

auto Map::AddItem(Item* item, ushort hx, ushort hy) -> bool
{
    STACK_TRACE_ENTRY();

    if (item == nullptr) {
        return false;
    }
    if (item->IsStatic()) {
        return false;
    }
    if (hx >= GetWidth() || hy >= GetHeight()) {
        return false;
    }

    SetItem(item, hx, hy);

    auto item_ids = GetItemIds();
    RUNTIME_ASSERT(std::find(item_ids.begin(), item_ids.end(), item->GetId()) == item_ids.end());
    item_ids.emplace_back(item->GetId());
    SetItemIds(std::move(item_ids));

    // Process critters view
    item->ViewPlaceOnMap = true;
    for (auto* cr : GetCritters()) {
        if (!item->GetIsHidden() || item->GetIsAlwaysView()) {
            if (!item->GetIsAlwaysView()) {
                // Check distance for non-hide items
                bool allowed;
                if (item->GetIsTrap() && IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_ITEM_SCRIPT)) {
                    allowed = _engine->OnMapCheckTrapLook.Fire(this, cr, item);
                }
                else {
                    int dist = _engine->Geometry.DistGame(cr->GetHexX(), cr->GetHexY(), hx, hy);
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
            cr->OnItemOnMapAppeared.Fire(item, false, nullptr);
        }
    }
    item->ViewPlaceOnMap = false;

    return true;
}

void Map::SetItem(Item* item, ushort hx, ushort hy)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!_itemsMap.count(item->GetId()));

    item->SetOwnership(ItemOwnership::MapHex);
    item->SetMapId(GetId());
    item->SetHexX(hx);
    item->SetHexY(hy);

    _items.push_back(item);
    _itemsMap.insert(std::make_pair(item->GetId(), item));
    _itemsByHex.emplace(tuple {hx, hy}, vector<Item*>()).first->second.push_back(item);

    if (item->GetIsGeck()) {
        _mapLocation->GeckCount++;
    }

    RecacheHexFlags(hx, hy);

    if (item->IsNonEmptyBlockLines()) {
        PlaceItemBlocks(hx, hy, item);
    }
}

void Map::EraseItem(ident_t item_id)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(item_id);
    const auto it = _itemsMap.find(item_id);
    RUNTIME_ASSERT(it != _itemsMap.end());
    auto* item = it->second;
    _itemsMap.erase(it);

    const auto it_all = std::find(_items.begin(), _items.end(), item);
    RUNTIME_ASSERT(it_all != _items.end());
    _items.erase(it_all);

    const auto hx = item->GetHexX();
    const auto hy = item->GetHexY();
    const auto it_hex_all = _itemsByHex.find(tuple {hx, hy});
    RUNTIME_ASSERT(it_hex_all != _itemsByHex.end());
    const auto it_hex = std::find(it_hex_all->second.begin(), it_hex_all->second.end(), item);
    RUNTIME_ASSERT(it_hex != it_hex_all->second.end());
    it_hex_all->second.erase(it_hex);
    if (it_hex_all->second.empty()) {
        _itemsByHex.erase(it_hex_all);
    }

    item->SetOwnership(ItemOwnership::Nowhere);
    item->SetMapId(ident_t {});
    item->SetHexX(0);
    item->SetHexY(0);

    auto item_ids = GetItemIds();
    const auto item_id_it = std::find(item_ids.begin(), item_ids.end(), item->GetId());
    RUNTIME_ASSERT(item_id_it != item_ids.end());
    item_ids.erase(item_id_it);
    SetItemIds(std::move(item_ids));

    if (item->GetIsGeck()) {
        _mapLocation->GeckCount--;
    }

    RecacheHexFlags(hx, hy);

    if (item->IsNonEmptyBlockLines()) {
        RemoveItemBlocks(hx, hy, item);
    }

    // Process critters view
    item->ViewPlaceOnMap = true;
    for (auto* cr : GetCritters()) {
        if (cr->DelIdVisItem(item->GetId())) {
            cr->Send_EraseItemFromMap(item);
            cr->OnItemOnMapDisappeared.Fire(item, item->ViewPlaceOnMap, item->ViewByCritter);
        }
    }
    item->ViewPlaceOnMap = false;
}

void Map::SendProperty(NetProperty type, const Property* prop, ServerEntity* entity)
{
    STACK_TRACE_ENTRY();

    if (type == NetProperty::MapItem) {
        auto* item = dynamic_cast<Item*>(entity);
        for (auto* cr : GetCritters()) {
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

void Map::ChangeViewItem(Item* item)
{
    STACK_TRACE_ENTRY();

    for (auto* cr : GetCritters()) {
        if (cr->CountIdVisItem(item->GetId())) {
            if (item->GetIsHidden()) {
                cr->DelIdVisItem(item->GetId());
                cr->Send_EraseItemFromMap(item);
                cr->OnItemOnMapDisappeared.Fire(item, false, nullptr);
            }
            else if (!item->GetIsAlwaysView()) // Check distance for non-hide items
            {
                bool allowed;
                if (item->GetIsTrap() && IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_ITEM_SCRIPT)) {
                    allowed = _engine->OnMapCheckTrapLook.Fire(this, cr, item);
                }
                else {
                    int dist = _engine->Geometry.DistGame(cr->GetHexX(), cr->GetHexY(), item->GetHexX(), item->GetHexY());
                    if (item->GetIsTrap()) {
                        dist += item->GetTrapValue();
                    }
                    allowed = dist <= static_cast<int>(cr->GetLookDistance());
                }
                if (!allowed) {
                    cr->DelIdVisItem(item->GetId());
                    cr->Send_EraseItemFromMap(item);
                    cr->OnItemOnMapDisappeared.Fire(item, false, nullptr);
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
                    int dist = _engine->Geometry.DistGame(cr->GetHexX(), cr->GetHexY(), item->GetHexX(), item->GetHexY());
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
            cr->OnItemOnMapAppeared.Fire(item, false, nullptr);
        }
    }
}

void Map::AnimateItem(Item* item, uchar from_frm, uchar to_frm)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    for (auto* cr : _playerCritters) {
        if (cr->CountIdVisItem(item->GetId())) {
            cr->Send_AnimateItem(item, from_frm, to_frm);
        }
    }
}

auto Map::GetItem(ident_t item_id) -> Item*
{
    STACK_TRACE_ENTRY();

    const auto it = _itemsMap.find(item_id);
    return it != _itemsMap.end() ? it->second : nullptr;
}

auto Map::GetItemHex(ushort hx, ushort hy, hstring item_pid, Critter* picker) -> Item*
{
    STACK_TRACE_ENTRY();

    const auto it_hex_all = _itemsByHex.find(tuple {hx, hy});
    if (it_hex_all != _itemsByHex.end()) {
        for (auto* item : it_hex_all->second) {
            if ((!item_pid || item->GetProtoId() == item_pid) && (picker == nullptr || (!item->GetIsHidden() && picker->CountIdVisItem(item->GetId())))) {
                return item;
            }
        }
    }

    return nullptr;
}

auto Map::GetItemGag(ushort hx, ushort hy) -> Item*
{
    STACK_TRACE_ENTRY();

    const auto it_hex_all = _itemsByHex.find(tuple {hx, hy});
    if (it_hex_all != _itemsByHex.end()) {
        for (auto* item : it_hex_all->second) {
            if (item->GetIsGag()) {
                return item;
            }
        }
    }
    return nullptr;
}

auto Map::GetItems() -> vector<Item*>
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    return _items;
}

auto Map::GetItemsHex(ushort hx, ushort hy) -> vector<Item*>
{
    STACK_TRACE_ENTRY();

    vector<Item*> items;

    const auto it_hex_all = _itemsByHex.find(tuple {hx, hy});
    if (it_hex_all != _itemsByHex.end()) {
        for (auto* item : it_hex_all->second) {
            items.push_back(item);
        }
    }

    return items;
}

auto Map::GetItemsHexEx(ushort hx, ushort hy, uint radius, hstring pid) -> vector<Item*>
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    vector<Item*> items;
    for (auto* item : _items) {
        if ((!pid || item->GetProtoId() == pid) && _engine->Geometry.DistGame(item->GetHexX(), item->GetHexY(), hx, hy) <= radius) {
            items.push_back(item);
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
            items.push_back(item);
        }
    }
    return items;
}

auto Map::GetItemsTrigger(ushort hx, ushort hy) -> vector<Item*>
{
    STACK_TRACE_ENTRY();

    vector<Item*> traps;
    const auto it_hex_all = _itemsByHex.find(tuple {hx, hy});
    if (it_hex_all != _itemsByHex.end()) {
        for (auto* item : it_hex_all->second) {
            if (item->GetIsTrap() || item->GetIsTrigger()) {
                traps.push_back(item);
            }
        }
    }
    return traps;
}

auto Map::IsPlaceForProtoItem(ushort hx, ushort hy, const ProtoItem* proto_item) const -> bool
{
    STACK_TRACE_ENTRY();

    if (!IsHexPassed(hx, hy)) {
        return false;
    }

    auto is_critter = false;
    _engine->Geometry.ForEachBlockLines(proto_item->GetBlockLines(), hx, hy, GetWidth(), GetHeight(), [this, &is_critter](auto hx2, auto hy2) { is_critter = is_critter || IsHexCritter(hx2, hy2); });
    return !is_critter;
}

void Map::PlaceItemBlocks(ushort hx, ushort hy, Item* item)
{
    STACK_TRACE_ENTRY();

    _engine->Geometry.ForEachBlockLines(item->GetBlockLines(), hx, hy, GetWidth(), GetHeight(), [this, item](auto hx2, auto hy2) {
        _blockLinesByHex.emplace(tuple {hx2, hy2}, vector<Item*>()).first->second.push_back(item);
        RecacheHexFlags(hx2, hy2);
    });
}

void Map::RemoveItemBlocks(ushort hx, ushort hy, Item* item)
{
    STACK_TRACE_ENTRY();

    _engine->Geometry.ForEachBlockLines(item->GetBlockLines(), hx, hy, GetWidth(), GetHeight(), [this, item](auto hx2, auto hy2) {
        auto it_hex_all_bl = _blockLinesByHex.find(tuple {hx2, hy2});
        RUNTIME_ASSERT(it_hex_all_bl != _blockLinesByHex.end());

        auto it_hex_bl = std::find(it_hex_all_bl->second.begin(), it_hex_all_bl->second.end(), item);
        RUNTIME_ASSERT(it_hex_bl != it_hex_all_bl->second.end());

        it_hex_all_bl->second.erase(it_hex_bl);
        if (it_hex_all_bl->second.empty()) {
            _blockLinesByHex.erase(it_hex_all_bl);
        }

        RecacheHexFlags(hx2, hy2);
    });
}

void Map::RecacheHexFlags(ushort hx, ushort hy)
{
    STACK_TRACE_ENTRY();

    UnsetHexFlag(hx, hy, FH_BLOCK_ITEM);
    UnsetHexFlag(hx, hy, FH_NRAKE_ITEM);
    UnsetHexFlag(hx, hy, FH_GAG_ITEM);
    UnsetHexFlag(hx, hy, FH_TRIGGER);

    auto is_block = false;
    auto is_nrake = false;
    auto is_gag = false;
    auto is_trap = false;
    auto is_trigger = false;

    const auto it_hex_all = _itemsByHex.find(tuple {hx, hy});
    if (it_hex_all != _itemsByHex.end()) {
        for (const auto* item : it_hex_all->second) {
            if (!is_block && !item->GetIsNoBlock()) {
                is_block = true;
            }
            if (!is_nrake && !item->GetIsShootThru()) {
                is_nrake = true;
            }
            if (!is_gag && item->GetIsGag()) {
                is_gag = true;
            }
            if (!is_trap && item->GetIsTrap()) {
                is_trap = true;
            }
            if (!is_trigger && item->GetIsTrigger()) {
                is_trigger = true;
            }
            if (is_block && is_nrake && is_gag && is_trap && is_trigger) {
                break;
            }
        }
    }

    if (!is_block && !is_nrake) {
        const auto it_hex_all_bl = _blockLinesByHex.find(tuple {hx, hy});
        if (it_hex_all_bl != _blockLinesByHex.end()) {
            is_block = true;

            for (const auto* item : it_hex_all_bl->second) {
                if (!item->GetIsShootThru()) {
                    is_nrake = true;
                    break;
                }
            }
        }
    }

    if (is_block) {
        SetHexFlag(hx, hy, FH_BLOCK_ITEM);
    }
    if (is_nrake) {
        SetHexFlag(hx, hy, FH_NRAKE_ITEM);
    }
    if (is_gag) {
        SetHexFlag(hx, hy, FH_GAG_ITEM);
    }
    if (is_trap) {
        SetHexFlag(hx, hy, FH_TRIGGER);
    }
    if (is_trigger) {
        SetHexFlag(hx, hy, FH_TRIGGER);
    }
}

auto Map::GetHexFlags(ushort hx, ushort hy) const -> ushort
{
    STACK_TRACE_ENTRY();

    const auto hi = static_cast<ushort>(static_cast<ushort>(_hexFlags[hy * GetWidth() + hx]) << 8);
    const auto lo = static_cast<ushort>(GetStaticMap()->HexFlags[hy * GetWidth() + hx]);
    return hi | lo;
}

void Map::SetHexFlag(ushort hx, ushort hy, uchar flag)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    SetBit(_hexFlags[hy * GetWidth() + hx], flag);
}

void Map::UnsetHexFlag(ushort hx, ushort hy, uchar flag)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    UnsetBit(_hexFlags[hy * GetWidth() + hx], flag);
}

auto Map::IsHexPassed(ushort hx, ushort hy) const -> bool
{
    STACK_TRACE_ENTRY();

    return !IsBitSet(GetHexFlags(hx, hy), FH_NOWAY);
}

auto Map::IsHexRaked(ushort hx, ushort hy) const -> bool
{
    STACK_TRACE_ENTRY();

    return !IsBitSet(GetHexFlags(hx, hy), FH_NOSHOOT);
}

auto Map::IsHexesPassed(ushort hx, ushort hy, uint radius) const -> bool
{
    STACK_TRACE_ENTRY();

    // Base
    if (IsBitSet(GetHexFlags(hx, hy), FH_NOWAY)) {
        return false;
    }
    if (radius == 0u) {
        return true;
    }

    // Neighbors
    const auto [sx, sy] = _engine->Geometry.GetHexOffsets((hx % 2) != 0);
    const auto count = GenericUtils::NumericalNumber(radius) * GameSettings::MAP_DIR_COUNT;
    const auto maxhx = GetWidth();
    const auto maxhy = GetHeight();

    for (uint i = 0; i < count; i++) {
        const auto hx_ = static_cast<short>(hx) + sx[i];
        const auto hy_ = static_cast<short>(hy) + sy[i];
        if (hx_ >= 0 && hy_ >= 0 && hx_ < maxhx && hy_ < maxhy) {
            if (IsBitSet(GetHexFlags(static_cast<ushort>(hx_), static_cast<ushort>(hy_)), FH_NOWAY)) {
                return false;
            }
        }
    }
    return true;
}

auto Map::IsMovePassed(const Critter* cr, ushort to_hx, ushort to_hy, uint multihex) -> bool
{
    STACK_TRACE_ENTRY();

    if (cr != nullptr && !cr->IsDead() && multihex > 0) {
        UnsetFlagCritter(cr->GetHexX(), cr->GetHexY(), multihex, false);
        const auto result = IsHexesPassed(to_hx, to_hy, multihex);
        SetFlagCritter(cr->GetHexX(), cr->GetHexY(), multihex, false);
        return result;
    }

    return IsHexesPassed(to_hx, to_hy, multihex);
}

auto Map::IsHexTrigger(ushort hx, ushort hy) const -> bool
{
    STACK_TRACE_ENTRY();

    return IsBitSet(_hexFlags[hy * GetWidth() + hx], FH_TRIGGER);
}

auto Map::IsHexCritter(ushort hx, ushort hy) const -> bool
{
    STACK_TRACE_ENTRY();

    return IsBitSet(_hexFlags[hy * GetWidth() + hx], FH_CRITTER) || IsBitSet(_hexFlags[hy * GetWidth() + hx], FH_DEAD_CRITTER);
}

auto Map::IsHexGag(ushort hx, ushort hy) const -> bool
{
    STACK_TRACE_ENTRY();

    return IsBitSet(_hexFlags[hy * GetWidth() + hx], FH_GAG_ITEM);
}

auto Map::IsHexBlockItem(ushort hx, ushort hy) const -> bool
{
    STACK_TRACE_ENTRY();

    return IsBitSet(_hexFlags[hy * GetWidth() + hx], FH_BLOCK_ITEM);
}

auto Map::IsHexStaticTrigger(ushort hx, ushort hy) const -> bool
{
    STACK_TRACE_ENTRY();

    return IsBitSet(GetStaticMap()->HexFlags[hy * GetWidth() + hx], FH_STATIC_TRIGGER);
}

auto Map::IsFlagCritter(ushort hx, ushort hy, bool dead) const -> bool
{
    STACK_TRACE_ENTRY();

    if (dead) {
        return IsBitSet(_hexFlags[hy * GetWidth() + hx], FH_DEAD_CRITTER);
    }

    return IsBitSet(_hexFlags[hy * GetWidth() + hx], FH_CRITTER);
}

void Map::SetFlagCritter(ushort hx, ushort hy, uint multihex, bool dead)
{
    STACK_TRACE_ENTRY();

    if (dead) {
        SetHexFlag(hx, hy, FH_DEAD_CRITTER);
    }
    else {
        SetHexFlag(hx, hy, FH_CRITTER);

        if (multihex != 0u) {
            const auto [sx, sy] = _engine->Geometry.GetHexOffsets((hx % 2) != 0);
            const auto count = GenericUtils::NumericalNumber(multihex) * GameSettings::MAP_DIR_COUNT;
            const auto maxhx = GetWidth();
            const auto maxhy = GetHeight();

            for (uint i = 0; i < count; i++) {
                const auto hx_ = static_cast<short>(hx) + sx[i];
                const auto hy_ = static_cast<short>(hy) + sy[i];
                if (hx_ >= 0 && hy_ >= 0 && hx_ < maxhx && hy_ < maxhy) {
                    SetHexFlag(static_cast<ushort>(hx_), static_cast<ushort>(hy_), FH_CRITTER);
                }
            }
        }
    }
}

void Map::UnsetFlagCritter(ushort hx, ushort hy, uint multihex, bool dead)
{
    STACK_TRACE_ENTRY();

    if (dead) {
        uint dead_count = 0;
        for (const auto* cr : _critters) {
            if (cr->GetHexX() == hx && cr->GetHexY() == hy && cr->IsDead()) {
                dead_count++;
            }
        }

        if (dead_count <= 1) {
            UnsetHexFlag(hx, hy, FH_DEAD_CRITTER);
        }
    }
    else {
        UnsetHexFlag(hx, hy, FH_CRITTER);

        if (multihex > 0) {
            const auto [sx, sy] = _engine->Geometry.GetHexOffsets((hx % 2) != 0);
            const auto count = GenericUtils::NumericalNumber(multihex) * GameSettings::MAP_DIR_COUNT;
            const auto maxhx = GetWidth();
            const auto maxhy = GetHeight();

            for (uint i = 0; i < count; i++) {
                const auto hx_ = static_cast<short>(hx) + sx[i];
                const auto hy_ = static_cast<short>(hy) + sy[i];
                if (hx_ >= 0 && hy_ >= 0 && hx_ < maxhx && hy_ < maxhy) {
                    UnsetHexFlag(static_cast<ushort>(hx_), static_cast<ushort>(hy_), FH_CRITTER);
                }
            }
        }
    }
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

auto Map::GetHexCritter(ushort hx, ushort hy, bool dead) -> Critter*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (!IsFlagCritter(hx, hy, dead)) {
        return nullptr;
    }

    for (auto* cr : _critters) {
        if (cr->IsDead() == dead) {
            const auto mh = cr->GetMultihex();
            if (mh == 0u) {
                if (cr->GetHexX() == hx && cr->GetHexY() == hy) {
                    return cr;
                }
            }
            else {
                if (_engine->Geometry.CheckDist(cr->GetHexX(), cr->GetHexY(), hx, hy, mh)) {
                    return cr;
                }
            }
        }
    }
    return nullptr;
}

auto Map::GetCrittersHex(ushort hx, ushort hy, uint radius, CritterFindType find_type) -> vector<Critter*>
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    vector<Critter*> critters;
    critters.reserve(_critters.size());

    for (auto* cr : _critters) {
        if (cr->CheckFind(find_type) && _engine->Geometry.CheckDist(hx, hy, cr->GetHexX(), cr->GetHexY(), radius + cr->GetMultihex())) {
            critters.push_back(cr);
        }
    }
    return critters;
}

auto Map::GetCritters() -> vector<Critter*>
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    return _critters;
}

auto Map::GetPlayers() -> vector<Critter*>
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    return _playerCritters;
}

auto Map::GetNpcs() -> vector<Critter*>
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    return _nonPlayerCritters;
}

auto Map::GetCrittersRaw() -> vector<Critter*>&
{
    STACK_TRACE_ENTRY();

    return _critters;
}

auto Map::GetPlayersRaw() -> vector<Critter*>&
{
    STACK_TRACE_ENTRY();

    return _playerCritters;
}

auto Map::GetNpcsRaw() -> vector<Critter*>&
{
    STACK_TRACE_ENTRY();

    return _nonPlayerCritters;
}

auto Map::GetCrittersCount() const -> uint
{
    STACK_TRACE_ENTRY();

    return static_cast<uint>(_critters.size());
}

auto Map::GetPlayersCount() const -> uint
{
    STACK_TRACE_ENTRY();

    return static_cast<uint>(_playerCritters.size());
}

auto Map::GetNpcsCount() const -> uint
{
    STACK_TRACE_ENTRY();

    return static_cast<uint>(_nonPlayerCritters.size());
}

void Map::SendEffect(hstring eff_pid, ushort hx, ushort hy, ushort radius)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    for (auto* cr : _playerCritters) {
        if (_engine->Geometry.CheckDist(cr->GetHexX(), cr->GetHexY(), hx, hy, cr->LookCacheValue + radius)) {
            cr->Send_Effect(eff_pid, hx, hy, radius);
        }
    }
}

void Map::SendFlyEffect(hstring eff_pid, ident_t from_cr_id, ident_t to_cr_id, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    for (auto* cr : _playerCritters) {
        if (GenericUtils::IntersectCircleLine(cr->GetHexX(), cr->GetHexY(), cr->LookCacheValue, from_hx, from_hy, to_hx, to_hy)) {
            cr->Send_FlyEffect(eff_pid, from_cr_id, to_cr_id, from_hx, from_hy, to_hx, to_hy);
        }
    }
}

void Map::SetText(ushort hx, ushort hy, uint color, string_view text, bool unsafe_text)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (hx >= GetWidth() || hy >= GetHeight()) {
        return;
    }

    for (auto* cr : _playerCritters) {
        if (cr->LookCacheValue >= _engine->Geometry.DistGame(hx, hy, cr->GetHexX(), cr->GetHexY())) {
            cr->Send_MapText(hx, hy, color, text, unsafe_text);
        }
    }
}

void Map::SetTextMsg(ushort hx, ushort hy, uint color, ushort text_msg, uint num_str)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (hx >= GetWidth() || hy >= GetHeight() || num_str == 0u) {
        return;
    }

    for (auto* cr : _playerCritters) {
        if (cr->LookCacheValue >= _engine->Geometry.DistGame(hx, hy, cr->GetHexX(), cr->GetHexY())) {
            cr->Send_MapTextMsg(hx, hy, color, text_msg, num_str);
        }
    }
}

void Map::SetTextMsgLex(ushort hx, ushort hy, uint color, ushort text_msg, uint num_str, string_view lexems)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (hx >= GetWidth() || hy >= GetHeight() || num_str == 0u) {
        return;
    }

    for (auto* cr : _playerCritters) {
        if (cr->LookCacheValue >= _engine->Geometry.DistGame(hx, hy, cr->GetHexX(), cr->GetHexY())) {
            cr->Send_MapTextMsgLex(hx, hy, color, text_msg, num_str, lexems);
        }
    }
}

auto Map::GetStaticItem(ushort hx, ushort hy, hstring pid) -> StaticItem*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    for (auto* item : _staticMap->StaticItems) {
        if ((!pid || item->GetProtoId() == pid) && item->GetHexX() == hx && item->GetHexY() == hy) {
            return item;
        }
    }
    return nullptr;
}

auto Map::GetStaticItemsHex(ushort hx, ushort hy) -> vector<StaticItem*>
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    vector<StaticItem*> items;
    for (auto* item : _staticMap->StaticItems) {
        if (item->GetHexX() == hx && item->GetHexY() == hy) {
            items.push_back(item);
        }
    }
    return items;
}

auto Map::GetStaticItemsHexEx(ushort hx, ushort hy, uint radius, hstring pid) -> vector<StaticItem*>
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    vector<StaticItem*> items;
    for (auto* item : _staticMap->StaticItems) {
        if ((!pid || item->GetProtoId() == pid) && _engine->Geometry.DistGame(item->GetHexX(), item->GetHexY(), hx, hy) <= radius) {
            items.push_back(item);
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
            items.push_back(item);
        }
    }
    return items;
}

auto Map::GetStaticItemsTrigger(ushort hx, ushort hy) -> vector<StaticItem*>
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    vector<StaticItem*> triggers;
    for (auto* item : _staticMap->TriggerItems) {
        if (item->GetHexX() == hx && item->GetHexY() == hy) {
            triggers.push_back(item);
        }
    }
    return triggers;
}
