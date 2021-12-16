//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "MapManager.h"
#include "Server.h"
#include "Settings.h"

Map::Map(FOServer* engine, uint id, const ProtoMap* proto, Location* location, const StaticMap* static_map) : ServerEntity(engine, id, engine->GetPropertyRegistrator("Map"), proto), MapProperties(Props), _staticMap {static_map}, _mapLocation {location}
{
    RUNTIME_ASSERT(proto);
    RUNTIME_ASSERT(_staticMap);

    _hexFlagsSize = GetWidth() * GetHeight();
    _hexFlags = new uchar[_hexFlagsSize];
    std::memset(_hexFlags, 0, _hexFlagsSize);
}

Map::~Map()
{
    delete[] _hexFlags;
}

void Map::Process()
{
    const auto tick = _engine->GameTime.GameTick();
    ProcessLoop(0, GetLoopTime1(), tick);
    ProcessLoop(1, GetLoopTime2(), tick);
    ProcessLoop(2, GetLoopTime3(), tick);
    ProcessLoop(3, GetLoopTime4(), tick);
    ProcessLoop(4, GetLoopTime5(), tick);
}

void Map::ProcessLoop(int index, uint time, uint tick)
{
    if (time != 0u && tick - _loopLastTick[index] >= time) {
        _loopLastTick[index] = tick;
        _engine->MapLoopEvent.Raise(this, index + 1);
    }
}

auto Map::GetProtoMap() const -> const ProtoMap*
{
    return dynamic_cast<const ProtoMap*>(Proto);
}

auto Map::GetLocation() -> Location*
{
    NON_CONST_METHOD_HINT();

    return _mapLocation;
}

auto Map::GetLocation() const -> const Location*
{
    return _mapLocation;
}

void Map::SetLocation(Location* loc)
{
    _mapLocation = loc;
}

auto Map::FindStartHex(ushort hx, ushort hy, uint multihex, uint seek_radius, bool skip_unsafe) const -> optional<tuple<ushort, ushort>>
{
    if (IsHexesPassed(hx, hy, multihex) && !(skip_unsafe && (IsHexStaticTrigger(hx, hy) || IsHexTrigger(hx, hy)))) {
        return tuple {hx, hy};
    }
    if (seek_radius == 0u) {
        return std::nullopt;
    }
    if (seek_radius > MAX_HEX_OFFSET) {
        seek_radius = MAX_HEX_OFFSET;
    }

    const auto [sx, sy] = _engine->GeomHelper.GetHexOffsets((hx % 2) != 0);
    auto hx_ = static_cast<short>(hx);
    auto hy_ = static_cast<short>(hy);
    const auto max_pos = static_cast<int>(GenericUtils::NumericalNumber(seek_radius) * _engine->Settings.MapDirCount);

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

auto Map::FindPlaceOnMap(ushort hx, ushort hy, Critter* cr, uint radius) const -> optional<tuple<ushort, ushort>>
{
    const auto multihex = cr->GetMultihex();
    auto r = FindStartHex(hx, hy, multihex, radius, true);
    if (r) {
        return r;
    }
    return FindStartHex(hx, hy, multihex, radius, false);
}

void Map::AddCritter(Critter* cr)
{
    RUNTIME_ASSERT(std::find(_mapCritters.begin(), _mapCritters.end(), cr) == _mapCritters.end());

    if (cr->IsPlayer()) {
        _mapPlayerCritters.push_back(cr);
    }
    if (cr->IsNpc()) {
        _mapNonPlayerCritters.push_back(cr);
    }
    _mapCritters.push_back(cr);

    SetFlagCritter(cr->GetHexX(), cr->GetHexY(), cr->GetMultihex(), cr->IsDead());

    cr->SetTimeoutBattle(0);
}

void Map::EraseCritter(Critter* cr)
{
    // Erase critter from collections
    if (cr->IsPlayer()) {
        const auto it = std::find(_mapPlayerCritters.begin(), _mapPlayerCritters.end(), cr);
        RUNTIME_ASSERT(it != _mapPlayerCritters.end());
        _mapPlayerCritters.erase(it);
    }
    else {
        const auto it = std::find(_mapNonPlayerCritters.begin(), _mapNonPlayerCritters.end(), cr);
        RUNTIME_ASSERT(it != _mapNonPlayerCritters.end());
        _mapNonPlayerCritters.erase(it);
    }

    const auto it = std::find(_mapCritters.begin(), _mapCritters.end(), cr);
    RUNTIME_ASSERT(it != _mapCritters.end());
    _mapCritters.erase(it);

    cr->SetTimeoutBattle(0);
}

auto Map::AddItem(Item* item, ushort hx, ushort hy) -> bool
{
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

    // Process critters view
    item->ViewPlaceOnMap = true;
    for (auto* cr : GetCritters()) {
        if (!item->GetIsHidden() || item->GetIsAlwaysView()) {
            if (!item->GetIsAlwaysView()) // Check distance for non-hide items
            {
                bool allowed;
                if (item->GetIsTrap() && IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_ITEM_SCRIPT)) {
                    allowed = _engine->MapCheckTrapLookEvent.Raise(this, cr, item);
                }
                else {
                    int dist = _engine->GeomHelper.DistGame(cr->GetHexX(), cr->GetHexY(), hx, hy);
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
            _engine->CritterShowItemOnMapEvent.Raise(cr, item, false, nullptr);
        }
    }
    item->ViewPlaceOnMap = false;

    return true;
}

void Map::SetItem(Item* item, ushort hx, ushort hy)
{
    RUNTIME_ASSERT(!_mapItemsById.count(item->GetId()));

    item->SetOwnership(ItemOwnership::MapHex);
    item->SetMapId(GetId());
    item->SetHexX(hx);
    item->SetHexY(hy);

    _mapItems.push_back(item);
    _mapItemsById.insert(std::make_pair(item->GetId(), item));
    _mapItemsByHex.insert(std::make_pair(hy << 16 | hx, vector<Item*>())).first->second.push_back(item);

    if (item->GetIsGeck()) {
        _mapLocation->GeckCount++;
    }

    RecacheHexFlags(hx, hy);

    if (item->IsNonEmptyBlockLines()) {
        PlaceItemBlocks(hx, hy, item);
    }
}

void Map::EraseItem(uint item_id)
{
    RUNTIME_ASSERT(item_id);
    const auto it = _mapItemsById.find(item_id);
    RUNTIME_ASSERT(it != _mapItemsById.end());
    auto* item = it->second;
    _mapItemsById.erase(it);

    const auto it_all = std::find(_mapItems.begin(), _mapItems.end(), item);
    RUNTIME_ASSERT(it_all != _mapItems.end());
    _mapItems.erase(it_all);

    const auto hx = item->GetHexX();
    const auto hy = item->GetHexY();
    auto it_hex_all = _mapItemsByHex.find(hy << 16 | hx);
    RUNTIME_ASSERT(it_hex_all != _mapItemsByHex.end());
    const auto it_hex = std::find(it_hex_all->second.begin(), it_hex_all->second.end(), item);
    RUNTIME_ASSERT(it_hex != it_hex_all->second.end());
    it_hex_all->second.erase(it_hex);
    if (it_hex_all->second.empty()) {
        _mapItemsByHex.erase(it_hex_all);
    }

    item->SetOwnership(ItemOwnership::Nowhere);
    item->SetMapId(0);
    item->SetHexX(0);
    item->SetHexY(0);

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
            _engine->CritterHideItemOnMapEvent.Raise(cr, item, item->ViewPlaceOnMap, item->ViewByCritter);
        }
    }
    item->ViewPlaceOnMap = false;
}

void Map::SendProperty(NetProperty::Type type, const Property* prop, ServerEntity* entity)
{
    if (type == NetProperty::MapItem) {
        auto* item = dynamic_cast<Item*>(entity);
        for (auto* cr : GetCritters()) {
            if (cr->CountIdVisItem(item->GetId())) {
                cr->Send_Property(type, prop, entity);
                _engine->CritterChangeItemOnMapEvent.Raise(cr, item);
            }
        }
    }
    else if (type == NetProperty::Map || type == NetProperty::Location) {
        for (auto* cr : GetCritters()) {
            cr->Send_Property(type, prop, entity);
            if (type == NetProperty::Map && (prop == GetPropertyDayTime() || prop == GetPropertyDayColor())) {
                cr->Send_GameInfo(nullptr);
            }
        }
    }
    else {
        RUNTIME_ASSERT(false);
    }
}

void Map::ChangeViewItem(Item* item)
{
    for (auto* cr : GetCritters()) {
        if (cr->CountIdVisItem(item->GetId())) {
            if (item->GetIsHidden()) {
                cr->DelIdVisItem(item->GetId());
                cr->Send_EraseItemFromMap(item);
                _engine->CritterHideItemOnMapEvent.Raise(cr, item, false, nullptr);
            }
            else if (!item->GetIsAlwaysView()) // Check distance for non-hide items
            {
                bool allowed;
                if (item->GetIsTrap() && IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_ITEM_SCRIPT)) {
                    allowed = _engine->MapCheckTrapLookEvent.Raise(this, cr, item);
                }
                else {
                    int dist = _engine->GeomHelper.DistGame(cr->GetHexX(), cr->GetHexY(), item->GetHexX(), item->GetHexY());
                    if (item->GetIsTrap()) {
                        dist += item->GetTrapValue();
                    }
                    allowed = dist <= static_cast<int>(cr->GetLookDistance());
                }
                if (!allowed) {
                    cr->DelIdVisItem(item->GetId());
                    cr->Send_EraseItemFromMap(item);
                    _engine->CritterHideItemOnMapEvent.Raise(cr, item, false, nullptr);
                }
            }
        }
        else if (!item->GetIsHidden() || item->GetIsAlwaysView()) {
            if (!item->GetIsAlwaysView()) // Check distance for non-hide items
            {
                bool allowed;
                if (item->GetIsTrap() && IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_ITEM_SCRIPT)) {
                    allowed = _engine->MapCheckTrapLookEvent.Raise(this, cr, item);
                }
                else {
                    int dist = _engine->GeomHelper.DistGame(cr->GetHexX(), cr->GetHexY(), item->GetHexX(), item->GetHexY());
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
            _engine->CritterShowItemOnMapEvent.Raise(cr, item, false, nullptr);
        }
    }
}

void Map::AnimateItem(Item* item, uchar from_frm, uchar to_frm)
{
    for (auto* cr : _mapPlayerCritters) {
        if (cr->CountIdVisItem(item->GetId())) {
            cr->Send_AnimateItem(item, from_frm, to_frm);
        }
    }
}

auto Map::GetItem(uint item_id) -> Item*
{
    const auto it = _mapItemsById.find(item_id);
    return it != _mapItemsById.end() ? it->second : nullptr;
}

auto Map::GetItemHex(ushort hx, ushort hy, hash item_pid, Critter* picker) -> Item*
{
    auto it_hex_all = _mapItemsByHex.find(hy << 16 | hx);
    if (it_hex_all != _mapItemsByHex.end()) {
        for (auto* item : it_hex_all->second) {
            if ((item_pid == 0 || item->GetProtoId() == item_pid) && (picker == nullptr || (!item->GetIsHidden() && picker->CountIdVisItem(item->GetId())))) {
                return item;
            }
        }
    }
    return nullptr;
}

auto Map::GetItemGag(ushort hx, ushort hy) -> Item*
{
    auto it_hex_all = _mapItemsByHex.find(hy << 16 | hx);
    if (it_hex_all != _mapItemsByHex.end()) {
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
    NON_CONST_METHOD_HINT();

    return _mapItems;
}

auto Map::GetItemsHex(ushort hx, ushort hy) -> vector<Item*>
{
    vector<Item*> items;
    auto it_hex_all = _mapItemsByHex.find(hy << 16 | hx);
    if (it_hex_all != _mapItemsByHex.end()) {
        for (auto* item : it_hex_all->second) {
            items.push_back(item);
        }
    }
    return items;
}

auto Map::GetItemsHexEx(ushort hx, ushort hy, uint radius, hash pid) -> vector<Item*>
{
    vector<Item*> items;
    for (auto* item : _mapItems) {
        if ((pid == 0u || item->GetProtoId() == pid) && _engine->GeomHelper.DistGame(item->GetHexX(), item->GetHexY(), hx, hy) <= radius) {
            items.push_back(item);
        }
    }
    return items;
}

auto Map::GetItemsPid(hash pid) -> vector<Item*>
{
    vector<Item*> items;
    for (auto* item : _mapItems) {
        if (pid == 0u || item->GetProtoId() == pid) {
            items.push_back(item);
        }
    }
    return items;
}

auto Map::GetItemsTrigger(ushort hx, ushort hy) -> vector<Item*>
{
    vector<Item*> traps;
    auto it_hex_all = _mapItemsByHex.find(hy << 16 | hx);
    if (it_hex_all != _mapItemsByHex.end()) {
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
    if (!IsHexPassed(hx, hy)) {
        return false;
    }

    auto is_critter = false;
    _engine->GeomHelper.ForEachBlockLines(proto_item->GetBlockLines(), hx, hy, GetWidth(), GetHeight(), [this, &is_critter](auto hx2, auto hy2) { is_critter = is_critter || IsHexCritter(hx2, hy2); });
    return !is_critter;
}

void Map::PlaceItemBlocks(ushort hx, ushort hy, Item* item)
{
    _engine->GeomHelper.ForEachBlockLines(item->GetBlockLines(), hx, hy, GetWidth(), GetHeight(), [this, item](auto hx2, auto hy2) {
        _mapBlockLinesByHex.insert(std::make_pair((hy2 << 16) | hx2, vector<Item*>())).first->second.push_back(item);
        RecacheHexFlags(hx2, hy2);
    });
}

void Map::RemoveItemBlocks(ushort hx, ushort hy, Item* item)
{
    _engine->GeomHelper.ForEachBlockLines(item->GetBlockLines(), hx, hy, GetWidth(), GetHeight(), [this, item](auto hx2, auto hy2) {
        auto it_hex_all_bl = _mapBlockLinesByHex.find((hy2 << 16) | hx2);
        RUNTIME_ASSERT(it_hex_all_bl != _mapBlockLinesByHex.end());

        auto it_hex_bl = std::find(it_hex_all_bl->second.begin(), it_hex_all_bl->second.end(), item);
        RUNTIME_ASSERT(it_hex_bl != it_hex_all_bl->second.end());

        it_hex_all_bl->second.erase(it_hex_bl);
        if (it_hex_all_bl->second.empty()) {
            _mapBlockLinesByHex.erase(it_hex_all_bl);
        }

        RecacheHexFlags(hx2, hy2);
    });
}

void Map::RecacheHexFlags(ushort hx, ushort hy)
{
    UnsetHexFlag(hx, hy, FH_BLOCK_ITEM);
    UnsetHexFlag(hx, hy, FH_NRAKE_ITEM);
    UnsetHexFlag(hx, hy, FH_GAG_ITEM);
    UnsetHexFlag(hx, hy, FH_TRIGGER);

    auto is_block = false;
    auto is_nrake = false;
    auto is_gag = false;
    auto is_trap = false;
    auto is_trigger = false;

    auto it_hex_all = _mapItemsByHex.find(hy << 16 | hx);
    if (it_hex_all != _mapItemsByHex.end()) {
        for (auto* item : it_hex_all->second) {
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
        auto it_hex_all_bl = _mapBlockLinesByHex.find(hy << 16 | hx);
        if (it_hex_all_bl != _mapBlockLinesByHex.end()) {
            is_block = true;

            for (auto* item : it_hex_all_bl->second) {
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
    const auto hi = static_cast<ushort>(static_cast<ushort>(_hexFlags[hy * GetWidth() + hx]) << 8);
    const auto lo = static_cast<ushort>(GetStaticMap()->HexFlags[hy * GetWidth() + hx]);
    return hi | lo;
}

void Map::SetHexFlag(ushort hx, ushort hy, uchar flag)
{
    NON_CONST_METHOD_HINT();

    SetBit(_hexFlags[hy * GetWidth() + hx], flag);
}

void Map::UnsetHexFlag(ushort hx, ushort hy, uchar flag)
{
    NON_CONST_METHOD_HINT();

    UnsetBit(_hexFlags[hy * GetWidth() + hx], flag);
}

auto Map::IsHexPassed(ushort hx, ushort hy) const -> bool
{
    return !IsBitSet(GetHexFlags(hx, hy), FH_NOWAY);
}

auto Map::IsHexRaked(ushort hx, ushort hy) const -> bool
{
    return !IsBitSet(GetHexFlags(hx, hy), FH_NOSHOOT);
}

auto Map::IsHexesPassed(ushort hx, ushort hy, uint radius) const -> bool
{
    // Base
    if (IsBitSet(GetHexFlags(hx, hy), FH_NOWAY)) {
        return false;
    }
    if (radius == 0u) {
        return true;
    }

    // Neighbors
    const auto [sx, sy] = _engine->GeomHelper.GetHexOffsets((hx % 2) != 0);
    const auto count = GenericUtils::NumericalNumber(radius) * _engine->Settings.MapDirCount;
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

auto Map::IsMovePassed(ushort hx, ushort hy, uchar dir, uint multihex) const -> bool
{
    // Single hex
    if (multihex == 0u) {
        return IsHexPassed(hx, hy);
    }

    // Multihex
    const auto map_width = GetWidth();
    const auto map_height = GetHeight();

    int hx_ = hx;
    int hy_ = hy;
    for (uint k = 0; k < multihex; k++) {
        if (!_engine->GeomHelper.MoveHexByDirUnsafe(hx_, hy_, dir, map_width, map_height)) {
            return false;
        }
    }
    if (!IsHexPassed(static_cast<ushort>(hx_), static_cast<ushort>(hy_))) {
        return false;
    }

    const auto is_square_corner = (!_engine->Settings.MapHexagonal && (dir % 2) != 0);
    const auto steps_count = (is_square_corner ? multihex * 2 : multihex);

    // Clock wise hexes
    auto dir_cw = static_cast<uchar>(_engine->Settings.MapHexagonal ? (dir + 2) % 6 : (dir + 2) % 8);
    if (is_square_corner) {
        dir_cw = (dir_cw + 1) % 8;
    }

    auto hx_cw = hx_;
    auto hy_cw = hy_;
    for (uint k = 0; k < steps_count; k++) {
        if (!_engine->GeomHelper.MoveHexByDirUnsafe(hx_cw, hy_cw, dir_cw, map_width, map_height)) {
            return false;
        }
        if (!IsHexPassed(static_cast<ushort>(hx_cw), static_cast<ushort>(hy_cw))) {
            return false;
        }
    }

    // Counter clock wise hexes
    auto dir_ccw = static_cast<uchar>(_engine->Settings.MapHexagonal ? (dir + 4) % 6 : (dir + 6) % 8);
    if (is_square_corner) {
        dir_ccw = (dir_ccw + 7) % 8;
    }

    auto hx_ccw = hx_;
    auto hy_ccw = hy_;
    for (uint k = 0; k < steps_count; k++) {
        if (!_engine->GeomHelper.MoveHexByDirUnsafe(hx_ccw, hy_ccw, dir_ccw, map_width, map_height)) {
            return false;
        }
        if (!IsHexPassed(static_cast<ushort>(hx_ccw), static_cast<ushort>(hy_ccw))) {
            return false;
        }
    }

    return true;
}

auto Map::IsHexTrigger(ushort hx, ushort hy) const -> bool
{
    return IsBitSet(_hexFlags[hy * GetWidth() + hx], FH_TRIGGER);
}

auto Map::IsHexCritter(ushort hx, ushort hy) const -> bool
{
    return IsBitSet(_hexFlags[hy * GetWidth() + hx], FH_CRITTER) || IsBitSet(_hexFlags[hy * GetWidth() + hx], FH_DEAD_CRITTER);
}

auto Map::IsHexGag(ushort hx, ushort hy) const -> bool
{
    return IsBitSet(_hexFlags[hy * GetWidth() + hx], FH_GAG_ITEM);
}

auto Map::IsHexStaticTrigger(ushort hx, ushort hy) const -> bool
{
    return IsBitSet(GetStaticMap()->HexFlags[hy * GetWidth() + hx], FH_STATIC_TRIGGER);
}

auto Map::IsFlagCritter(ushort hx, ushort hy, bool dead) const -> bool
{
    if (dead) {
        return IsBitSet(_hexFlags[hy * GetWidth() + hx], FH_DEAD_CRITTER);
    }

    return IsBitSet(_hexFlags[hy * GetWidth() + hx], FH_CRITTER);
}

void Map::SetFlagCritter(ushort hx, ushort hy, uint multihex, bool dead)
{
    if (dead) {
        SetHexFlag(hx, hy, FH_DEAD_CRITTER);
    }
    else {
        SetHexFlag(hx, hy, FH_CRITTER);

        if (multihex != 0u) {
            const auto [sx, sy] = _engine->GeomHelper.GetHexOffsets((hx % 2) != 0);
            const auto count = GenericUtils::NumericalNumber(multihex) * _engine->Settings.MapDirCount;
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
    if (dead) {
        uint dead_count = 0;
        for (auto* cr : _mapCritters) {
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
            const auto [sx, sy] = _engine->GeomHelper.GetHexOffsets((hx % 2) != 0);
            const auto count = GenericUtils::NumericalNumber(multihex) * _engine->Settings.MapDirCount;
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

auto Map::GetNpcCount(hash npc_role, uchar find_type) const -> uint
{
    uint result = 0;
    for (auto* npc : _mapNonPlayerCritters) {
        if (npc->GetNpcRole() == npc_role && npc->CheckFind(find_type)) {
            result++;
        }
    }
    return result;
}

auto Map::GetCritter(uint crid) -> Critter*
{
    for (auto* cr : _mapCritters) {
        if (cr->GetId() == crid) {
            return cr;
        }
    }
    return nullptr;
}

auto Map::GetNpc(hash npc_role, uchar find_type, uint skip_count) -> Critter*
{
    for (auto* npc : _mapNonPlayerCritters) {
        if (npc->GetNpcRole() == npc_role && npc->CheckFind(find_type)) {
            if (skip_count != 0u) {
                skip_count--;
            }
            else {
                return npc;
            }
        }
    }
    return nullptr;
}

auto Map::GetHexCritter(ushort hx, ushort hy, bool dead) -> Critter*
{
    if (!IsFlagCritter(hx, hy, dead)) {
        return nullptr;
    }

    for (auto* cr : _mapCritters) {
        if (cr->IsDead() == dead) {
            const int mh = cr->GetMultihex();
            if (mh == 0) {
                if (cr->GetHexX() == hx && cr->GetHexY() == hy) {
                    return cr;
                }
            }
            else {
                if (_engine->GeomHelper.CheckDist(cr->GetHexX(), cr->GetHexY(), hx, hy, mh)) {
                    return cr;
                }
            }
        }
    }
    return nullptr;
}

auto Map::GetCrittersHex(ushort hx, ushort hy, uint radius, uchar find_type) -> vector<Critter*>
{
    vector<Critter*> critters;
    critters.reserve(_mapCritters.size());

    for (auto* cr : _mapCritters) {
        if (cr->CheckFind(find_type) && _engine->GeomHelper.CheckDist(hx, hy, cr->GetHexX(), cr->GetHexY(), radius + cr->GetMultihex())) {
            critters.push_back(cr);
        }
    }
    return critters;
}

auto Map::GetCritters() -> vector<Critter*>
{
    NON_CONST_METHOD_HINT();

    return _mapCritters;
}

auto Map::GetPlayers() -> vector<Critter*>
{
    NON_CONST_METHOD_HINT();

    return _mapPlayerCritters;
}

auto Map::GetNpcs() -> vector<Critter*>
{
    NON_CONST_METHOD_HINT();

    return _mapNonPlayerCritters;
}

auto Map::GetCrittersRaw() -> vector<Critter*>&
{
    return _mapCritters;
}

auto Map::GetPlayersRaw() -> vector<Critter*>&
{
    return _mapPlayerCritters;
}

auto Map::GetNpcsRaw() -> vector<Critter*>&
{
    return _mapNonPlayerCritters;
}

auto Map::GetCrittersCount() const -> uint
{
    return static_cast<uint>(_mapCritters.size());
}

auto Map::GetPlayersCount() const -> uint
{
    return static_cast<uint>(_mapPlayerCritters.size());
}

auto Map::GetNpcsCount() const -> uint
{
    return static_cast<uint>(_mapNonPlayerCritters.size());
}

void Map::SendEffect(hash eff_pid, ushort hx, ushort hy, ushort radius)
{
    for (auto* cr : _mapPlayerCritters) {
        if (_engine->GeomHelper.CheckDist(cr->GetHexX(), cr->GetHexY(), hx, hy, cr->LookCacheValue + radius)) {
            cr->Send_Effect(eff_pid, hx, hy, radius);
        }
    }
}

void Map::SendFlyEffect(hash eff_pid, uint from_crid, uint to_crid, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy)
{
    for (auto* cr : _mapPlayerCritters) {
        if (GenericUtils::IntersectCircleLine(cr->GetHexX(), cr->GetHexY(), cr->LookCacheValue, from_hx, from_hy, to_hx, to_hy)) {
            cr->Send_FlyEffect(eff_pid, from_crid, to_crid, from_hx, from_hy, to_hx, to_hy);
        }
    }
}

auto Map::SetScript(string_view /*func*/, bool /*first_time*/) -> bool
{
    /*if (func)
    {
        hash func_num = scriptSys.BindScriptFuncNumByFunc(func);
        if (!func_num)
        {
            WriteLog("Script bind fail, map '{}'.\n", GetName());
            return false;
        }
        SetScriptId(func_num);
    }

    if (GetScriptId())
    {
        scriptSys.PrepareScriptFuncContext(GetScriptId(), _str("Map '{}' ({})", GetName(), GetId()));
        scriptSys.SetArgEntity(this);
        scriptSys.SetArgBool(first_time);
        scriptSys.RunPrepared();
    }*/
    return true;
}

void Map::SetText(ushort hx, ushort hy, uint color, string_view text, bool unsafe_text)
{
    if (hx >= GetWidth() || hy >= GetHeight()) {
        return;
    }

    for (auto* cr : _mapPlayerCritters) {
        if (cr->LookCacheValue >= _engine->GeomHelper.DistGame(hx, hy, cr->GetHexX(), cr->GetHexY())) {
            cr->Send_MapText(hx, hy, color, text, unsafe_text);
        }
    }
}

void Map::SetTextMsg(ushort hx, ushort hy, uint color, ushort text_msg, uint num_str)
{
    if (hx >= GetWidth() || hy >= GetHeight() || num_str == 0u) {
        return;
    }

    for (auto* cr : _mapPlayerCritters) {
        if (cr->LookCacheValue >= _engine->GeomHelper.DistGame(hx, hy, cr->GetHexX(), cr->GetHexY())) {
            cr->Send_MapTextMsg(hx, hy, color, text_msg, num_str);
        }
    }
}

void Map::SetTextMsgLex(ushort hx, ushort hy, uint color, ushort text_msg, uint num_str, string_view lexems)
{
    if (hx >= GetWidth() || hy >= GetHeight() || num_str == 0u) {
        return;
    }

    for (auto* cr : _mapPlayerCritters) {
        if (cr->LookCacheValue >= _engine->GeomHelper.DistGame(hx, hy, cr->GetHexX(), cr->GetHexY())) {
            cr->Send_MapTextMsgLex(hx, hy, color, text_msg, num_str, lexems);
        }
    }
}

auto Map::GetStaticItemTriggers(ushort hx, ushort hy) -> vector<Item*>
{
    NON_CONST_METHOD_HINT();

    vector<Item*> triggers;
    for (auto* item : _staticMap->TriggerItemsVec) {
        if (item->GetHexX() == hx && item->GetHexY() == hy) {
            triggers.push_back(item);
        }
    }
    return triggers;
}

auto Map::GetStaticItem(ushort hx, ushort hy, hash pid) -> Item*
{
    NON_CONST_METHOD_HINT();

    for (auto* item : _staticMap->StaticItemsVec) {
        if ((pid == 0u || item->GetProtoId() == pid) && item->GetHexX() == hx && item->GetHexY() == hy) {
            return item;
        }
    }
    return nullptr;
}

auto Map::GetStaticItemsHex(ushort hx, ushort hy) -> vector<Item*>
{
    NON_CONST_METHOD_HINT();

    vector<Item*> items;
    for (auto* item : _staticMap->StaticItemsVec) {
        if (item->GetHexX() == hx && item->GetHexY() == hy) {
            items.push_back(item);
        }
    }
    return items;
}

auto Map::GetStaticItemsHexEx(ushort hx, ushort hy, uint radius, hash pid) -> vector<Item*>
{
    NON_CONST_METHOD_HINT();

    vector<Item*> items;
    for (auto* item : _staticMap->StaticItemsVec) {
        if ((pid == 0u || item->GetProtoId() == pid) && _engine->GeomHelper.DistGame(item->GetHexX(), item->GetHexY(), hx, hy) <= radius) {
            items.push_back(item);
        }
    }
    return items;
}

auto Map::GetStaticItemsByPid(hash pid) -> vector<Item*>
{
    NON_CONST_METHOD_HINT();

    vector<Item*> items;
    for (auto* item : _staticMap->StaticItemsVec) {
        if (pid == 0u || item->GetProtoId() == pid) {
            items.push_back(item);
        }
    }
    return items;
}
