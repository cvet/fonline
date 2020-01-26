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
#include "ItemManager.h"
#include "Location.h"
#include "Log.h"
#include "MapManager.h"
#include "Settings.h"
#include "StringUtils.h"
#include "Testing.h"
#include "Timer.h"

PROPERTIES_IMPL(Map);
CLASS_PROPERTY_IMPL(Map, LoopTime1);
CLASS_PROPERTY_IMPL(Map, LoopTime2);
CLASS_PROPERTY_IMPL(Map, LoopTime3);
CLASS_PROPERTY_IMPL(Map, LoopTime4);
CLASS_PROPERTY_IMPL(Map, LoopTime5);
CLASS_PROPERTY_IMPL(Map, FileDir);
CLASS_PROPERTY_IMPL(Map, Width);
CLASS_PROPERTY_IMPL(Map, Height);
CLASS_PROPERTY_IMPL(Map, WorkHexX);
CLASS_PROPERTY_IMPL(Map, WorkHexY);
CLASS_PROPERTY_IMPL(Map, LocId);
CLASS_PROPERTY_IMPL(Map, LocMapIndex);
CLASS_PROPERTY_IMPL(Map, RainCapacity);
CLASS_PROPERTY_IMPL(Map, CurDayTime);
CLASS_PROPERTY_IMPL(Map, ScriptId);
CLASS_PROPERTY_IMPL(Map, DayTime);
CLASS_PROPERTY_IMPL(Map, DayColor);
CLASS_PROPERTY_IMPL(Map, IsNoLogOut);

Map::Map(
    uint id, ProtoMap* proto, Location* location, StaticMap* static_map, MapSettings& sett, ScriptSystem& script_sys) :
    Entity(id, EntityType::Map, PropertiesRegistrator, proto),
    mapLocation {location},
    staticMap {static_map},
    settings {sett},
    geomHelper(settings),
    scriptSys {script_sys}
{
    RUNTIME_ASSERT(proto);
    RUNTIME_ASSERT(staticMap);

    hexFlagsSize = GetWidth() * GetHeight();
    hexFlags = new uchar[hexFlagsSize];
    memzero(hexFlags, hexFlagsSize);
}

Map::~Map()
{
    SAFEDELA(hexFlags);
}

void Map::Process()
{
    uint tick = Timer::GameTick();
    ProcessLoop(0, GetLoopTime1(), tick);
    ProcessLoop(1, GetLoopTime2(), tick);
    ProcessLoop(2, GetLoopTime3(), tick);
    ProcessLoop(3, GetLoopTime4(), tick);
    ProcessLoop(4, GetLoopTime5(), tick);
}

void Map::ProcessLoop(int index, uint time, uint tick)
{
    if (time && tick - loopLastTick[index] >= time)
    {
        loopLastTick[index] = tick;
        scriptSys.RaiseInternalEvent(ServerFunctions.MapLoop, this, index + 1);
    }
}

ProtoMap* Map::GetProtoMap()
{
    return (ProtoMap*)Proto;
}

Location* Map::GetLocation()
{
    return mapLocation;
}

void Map::SetLocation(Location* loc)
{
    mapLocation = loc;
}

bool Map::FindStartHex(ushort& hx, ushort& hy, uint multihex, uint seek_radius, bool skip_unsafe)
{
    if (IsHexesPassed(hx, hy, multihex) && !(skip_unsafe && (IsHexStaticTrigger(hx, hy) || IsHexTrigger(hx, hy))))
        return true;
    if (!seek_radius)
        return false;
    if (seek_radius > MAX_HEX_OFFSET)
        seek_radius = MAX_HEX_OFFSET;

    short hx_ = hx;
    short hy_ = hy;
    short *sx, *sy;
    geomHelper.GetHexOffsets(hx & 1, sx, sy);
    int max_pos = GenericUtils::NumericalNumber(seek_radius) * settings.MapDirCount;

    int pos = -1;
    while (true)
    {
        pos++;
        if (pos >= max_pos)
            return false;

        pos++;
        if (pos >= max_pos)
            pos = 0;
        short nx = hx_ + sx[pos];
        short ny = hy_ + sy[pos];

        if (nx < 0 || nx >= GetWidth() || ny < 0 || ny >= GetHeight())
            continue;
        if (!IsHexesPassed(nx, ny, multihex))
            continue;
        if (skip_unsafe && (IsHexStaticTrigger(nx, ny) || IsHexTrigger(nx, ny)))
            continue;
        break;
    }

    hx_ += sx[pos];
    hy_ += sy[pos];
    hx = hx_;
    hy = hy_;
    return true;
}

void Map::AddCritter(Critter* cr)
{
    RUNTIME_ASSERT(std::find(mapCritters.begin(), mapCritters.end(), cr) == mapCritters.end());

    if (cr->IsPlayer())
        mapPlayers.push_back((Client*)cr);
    if (cr->IsNpc())
        mapNpcs.push_back((Npc*)cr);
    mapCritters.push_back(cr);

    SetFlagCritter(cr->GetHexX(), cr->GetHexY(), cr->GetMultihex(), cr->IsDead());

    cr->SetTimeoutBattle(0);
}

void Map::EraseCritter(Critter* cr)
{
    // Erase critter from collections
    if (cr->IsPlayer())
    {
        auto it = std::find(mapPlayers.begin(), mapPlayers.end(), (Client*)cr);
        RUNTIME_ASSERT(it != mapPlayers.end());
        mapPlayers.erase(it);
    }
    else
    {
        auto it = std::find(mapNpcs.begin(), mapNpcs.end(), (Npc*)cr);
        RUNTIME_ASSERT(it != mapNpcs.end());
        mapNpcs.erase(it);
    }

    auto it = std::find(mapCritters.begin(), mapCritters.end(), cr);
    RUNTIME_ASSERT(it != mapCritters.end());
    mapCritters.erase(it);

    cr->SetTimeoutBattle(0);
}

bool Map::AddItem(Item* item, ushort hx, ushort hy)
{
    if (!item)
        return false;
    if (item->IsStatic())
        return false;
    if (hx >= GetWidth() || hy >= GetHeight())
        return false;

    SetItem(item, hx, hy);

    // Process critters view
    item->ViewPlaceOnMap = true;
    for (Critter* cr : GetCritters())
    {
        if (!item->GetIsHidden() || item->GetIsAlwaysView())
        {
            if (!item->GetIsAlwaysView()) // Check distance for non-hide items
            {
                bool allowed = false;
                if (item->GetIsTrap() && FLAG(settings.LookChecks, LOOK_CHECK_ITEM_SCRIPT))
                {
                    allowed = scriptSys.RaiseInternalEvent(ServerFunctions.MapCheckTrapLook, this, cr, item);
                }
                else
                {
                    int dist = geomHelper.DistGame(cr->GetHexX(), cr->GetHexY(), hx, hy);
                    if (item->GetIsTrap())
                        dist += item->GetTrapValue();
                    allowed = (dist <= (int)cr->GetLookDistance());
                }
                if (!allowed)
                    continue;
            }

            cr->AddIdVisItem(item->GetId());
            cr->Send_AddItemOnMap(item);
            scriptSys.RaiseInternalEvent(ServerFunctions.CritterShowItemOnMap, cr, item, false, nullptr);
        }
    }
    item->ViewPlaceOnMap = false;

    return true;
}

void Map::SetItem(Item* item, ushort hx, ushort hy)
{
    RUNTIME_ASSERT(!mapItemsById.count(item->GetId()));

    item->SetAccessory(ITEM_ACCESSORY_HEX);
    item->SetMapId(Id);
    item->SetHexX(hx);
    item->SetHexY(hy);

    mapItems.push_back(item);
    mapItemsById.insert(std::make_pair(item->GetId(), item));
    mapItemsByHex.insert(std::make_pair((hy << 16) | hx, ItemVec())).first->second.push_back(item);

    if (item->GetIsGeck())
        mapLocation->GeckCount++;

    RecacheHexFlags(hx, hy);

    if (item->IsNonEmptyBlockLines())
        PlaceItemBlocks(hx, hy, item);
}

void Map::EraseItem(uint item_id)
{
    RUNTIME_ASSERT(item_id);
    auto it = mapItemsById.find(item_id);
    RUNTIME_ASSERT(it != mapItemsById.end());
    Item* item = it->second;
    mapItemsById.erase(it);

    auto it_all = std::find(mapItems.begin(), mapItems.end(), item);
    RUNTIME_ASSERT(it_all != mapItems.end());
    mapItems.erase(it_all);

    ushort hx = item->GetHexX();
    ushort hy = item->GetHexY();
    auto it_hex_all = mapItemsByHex.find((hy << 16) | hx);
    RUNTIME_ASSERT(it_hex_all != mapItemsByHex.end());
    auto it_hex = std::find(it_hex_all->second.begin(), it_hex_all->second.end(), item);
    RUNTIME_ASSERT(it_hex != it_hex_all->second.end());
    it_hex_all->second.erase(it_hex);
    if (it_hex_all->second.empty())
        mapItemsByHex.erase(it_hex_all);

    item->SetAccessory(ITEM_ACCESSORY_NONE);
    item->SetMapId(0);
    item->SetHexX(0);
    item->SetHexY(0);

    if (item->GetIsGeck())
        mapLocation->GeckCount--;

    RecacheHexFlags(hx, hy);

    if (item->IsNonEmptyBlockLines())
        RemoveItemBlocks(hx, hy, item);

    // Process critters view
    item->ViewPlaceOnMap = true;
    for (Critter* cr : GetCritters())
    {
        if (cr->DelIdVisItem(item->GetId()))
        {
            cr->Send_EraseItemFromMap(item);
            scriptSys.RaiseInternalEvent(
                ServerFunctions.CritterHideItemOnMap, cr, item, item->ViewPlaceOnMap, item->ViewByCritter);
        }
    }
    item->ViewPlaceOnMap = false;
}

void Map::SendProperty(NetProperty::Type type, Property* prop, Entity* entity)
{
    if (type == NetProperty::MapItem)
    {
        Item* item = (Item*)entity;
        for (Critter* cr : GetCritters())
        {
            if (cr->CountIdVisItem(item->GetId()))
            {
                cr->Send_Property(type, prop, entity);
                scriptSys.RaiseInternalEvent(ServerFunctions.CritterChangeItemOnMap, cr, item);
            }
        }
    }
    else if (type == NetProperty::Map || type == NetProperty::Location)
    {
        for (Critter* cr : GetCritters())
        {
            cr->Send_Property(type, prop, entity);
            if (type == NetProperty::Map && (prop == Map::PropertyDayTime || prop == Map::PropertyDayColor))
                cr->Send_GameInfo(nullptr);
        }
    }
    else
    {
        RUNTIME_ASSERT(false);
    }
}

void Map::ChangeViewItem(Item* item)
{
    for (Critter* cr : GetCritters())
    {
        if (cr->CountIdVisItem(item->GetId()))
        {
            if (item->GetIsHidden())
            {
                cr->DelIdVisItem(item->GetId());
                cr->Send_EraseItemFromMap(item);
                scriptSys.RaiseInternalEvent(ServerFunctions.CritterHideItemOnMap, cr, item, false, nullptr);
            }
            else if (!item->GetIsAlwaysView()) // Check distance for non-hide items
            {
                bool allowed = false;
                if (item->GetIsTrap() && FLAG(settings.LookChecks, LOOK_CHECK_ITEM_SCRIPT))
                {
                    allowed = scriptSys.RaiseInternalEvent(ServerFunctions.MapCheckTrapLook, this, cr, item);
                }
                else
                {
                    int dist = geomHelper.DistGame(cr->GetHexX(), cr->GetHexY(), item->GetHexX(), item->GetHexY());
                    if (item->GetIsTrap())
                        dist += item->GetTrapValue();
                    allowed = (dist <= (int)cr->GetLookDistance());
                }
                if (!allowed)
                {
                    cr->DelIdVisItem(item->GetId());
                    cr->Send_EraseItemFromMap(item);
                    scriptSys.RaiseInternalEvent(ServerFunctions.CritterHideItemOnMap, cr, item, false, nullptr);
                }
            }
        }
        else if (!item->GetIsHidden() || item->GetIsAlwaysView())
        {
            if (!item->GetIsAlwaysView()) // Check distance for non-hide items
            {
                bool allowed = false;
                if (item->GetIsTrap() && FLAG(settings.LookChecks, LOOK_CHECK_ITEM_SCRIPT))
                {
                    allowed = scriptSys.RaiseInternalEvent(ServerFunctions.MapCheckTrapLook, this, cr, item);
                }
                else
                {
                    int dist = geomHelper.DistGame(cr->GetHexX(), cr->GetHexY(), item->GetHexX(), item->GetHexY());
                    if (item->GetIsTrap())
                        dist += item->GetTrapValue();
                    allowed = (dist <= (int)cr->GetLookDistance());
                }
                if (!allowed)
                    continue;
            }

            cr->AddIdVisItem(item->GetId());
            cr->Send_AddItemOnMap(item);
            scriptSys.RaiseInternalEvent(ServerFunctions.CritterShowItemOnMap, cr, item, false, nullptr);
        }
    }
}

void Map::AnimateItem(Item* item, uchar from_frm, uchar to_frm)
{
    for (Client* cl : mapPlayers)
        if (cl->CountIdVisItem(item->GetId()))
            cl->Send_AnimateItem(item, from_frm, to_frm);
}

Item* Map::GetItem(uint item_id)
{
    auto it = mapItemsById.find(item_id);
    return it != mapItemsById.end() ? it->second : nullptr;
}

Item* Map::GetItemHex(ushort hx, ushort hy, hash item_pid, Critter* picker)
{
    auto it_hex_all = mapItemsByHex.find((hy << 16) | hx);
    if (it_hex_all != mapItemsByHex.end())
    {
        for (Item* item : it_hex_all->second)
        {
            if ((item_pid == 0 || item->GetProtoId() == item_pid) &&
                (!picker || (!item->GetIsHidden() && picker->CountIdVisItem(item->GetId()))))
                return item;
        }
    }
    return nullptr;
}

Item* Map::GetItemGag(ushort hx, ushort hy)
{
    auto it_hex_all = mapItemsByHex.find((hy << 16) | hx);
    if (it_hex_all != mapItemsByHex.end())
    {
        for (Item* item : it_hex_all->second)
            if (item->GetIsGag())
                return item;
    }
    return nullptr;
}

ItemVec Map::GetItems()
{
    return mapItems;
}

void Map::GetItemsHex(ushort hx, ushort hy, ItemVec& items)
{
    auto it_hex_all = mapItemsByHex.find((hy << 16) | hx);
    if (it_hex_all != mapItemsByHex.end())
        for (Item* item : it_hex_all->second)
            items.push_back(item);
}

void Map::GetItemsHexEx(ushort hx, ushort hy, uint radius, hash pid, ItemVec& items)
{
    for (Item* item : mapItems)
        if ((!pid || item->GetProtoId() == pid) &&
            geomHelper.DistGame(item->GetHexX(), item->GetHexY(), hx, hy) <= radius)
            items.push_back(item);
}

void Map::GetItemsPid(hash pid, ItemVec& items)
{
    for (Item* item : mapItems)
        if (!pid || item->GetProtoId() == pid)
            items.push_back(item);
}

void Map::GetItemsTrigger(ushort hx, ushort hy, ItemVec& traps)
{
    auto it_hex_all = mapItemsByHex.find((hy << 16) | hx);
    if (it_hex_all != mapItemsByHex.end())
    {
        for (Item* item : it_hex_all->second)
            if (item->GetIsTrap() || item->GetIsTrigger())
                traps.push_back(item);
    }
}

bool Map::IsPlaceForProtoItem(ushort hx, ushort hy, ProtoItem* proto_item)
{
    if (!IsHexPassed(hx, hy))
        return false;

    // Todo: rework FOREACH_PROTO_ITEM_LINES
    // FOREACH_PROTO_ITEM_LINES(
    //    proto_item->GetBlockLines(), hx, hy, GetWidth(), GetHeight(), if (IsHexCritter(hx, hy)) return false;);
    return true;
}

void Map::PlaceItemBlocks(ushort hx, ushort hy, Item* item)
{
    // Todo: rework FOREACH_PROTO_ITEM_LINES
    // bool raked = item->GetIsShootThru();
    // FOREACH_PROTO_ITEM_LINES(
    //    item->GetBlockLines(), hx, hy, GetWidth(), GetHeight(),
    //    mapBlockLinesByHex.insert(std::make_pair((hy << 16) | hx, ItemVec())).first->second.push_back(item);
    //    RecacheHexFlags(hx, hy););
}

void Map::RemoveItemBlocks(ushort hx, ushort hy, Item* item)
{
    // Todo: rework FOREACH_PROTO_ITEM_LINES
    // bool raked = item->GetIsShootThru();
    // FOREACH_PROTO_ITEM_LINES(
    //    item->GetBlockLines(), hx, hy, GetWidth(), GetHeight(),
    //    auto it_hex_all_bl = mapBlockLinesByHex.find((hy << 16) | hx);
    //    RUNTIME_ASSERT(it_hex_all_bl != mapBlockLinesByHex.end());
    //    auto it_hex_bl = std::find(it_hex_all_bl->second.begin(), it_hex_all_bl->second.end(), item);
    //    RUNTIME_ASSERT(it_hex_bl != it_hex_all_bl->second.end()); it_hex_all_bl->second.erase(it_hex_bl);
    //    if (it_hex_all_bl->second.empty()) mapBlockLinesByHex.erase(it_hex_all_bl);
    //    RecacheHexFlags(hx, hy););
}

void Map::RecacheHexFlags(ushort hx, ushort hy)
{
    UnsetHexFlag(hx, hy, FH_BLOCK_ITEM);
    UnsetHexFlag(hx, hy, FH_NRAKE_ITEM);
    UnsetHexFlag(hx, hy, FH_GAG_ITEM);
    UnsetHexFlag(hx, hy, FH_TRIGGER);

    bool is_block = false;
    bool is_nrake = false;
    bool is_gag = false;
    bool is_trap = false;
    bool is_trigger = false;

    auto it_hex_all = mapItemsByHex.find((hy << 16) | hx);
    if (it_hex_all != mapItemsByHex.end())
    {
        for (Item* item : it_hex_all->second)
        {
            if (!is_block && !item->GetIsNoBlock())
                is_block = true;
            if (!is_nrake && !item->GetIsShootThru())
                is_nrake = true;
            if (!is_gag && item->GetIsGag())
                is_gag = true;
            if (!is_trap && item->GetIsTrap())
                is_trap = true;
            if (!is_trigger && item->GetIsTrigger())
                is_trigger = true;
            if (is_block && is_nrake && is_gag && is_trap && is_trigger)
                break;
        }
    }

    if (!is_block && !is_nrake)
    {
        auto it_hex_all_bl = mapBlockLinesByHex.find((hy << 16) | hx);
        if (it_hex_all_bl != mapBlockLinesByHex.end())
        {
            is_block = true;

            for (Item* item : it_hex_all_bl->second)
            {
                if (!item->GetIsShootThru())
                {
                    is_nrake = true;
                    break;
                }
            }
        }
    }

    if (is_block)
        SetHexFlag(hx, hy, FH_BLOCK_ITEM);
    if (is_nrake)
        SetHexFlag(hx, hy, FH_NRAKE_ITEM);
    if (is_gag)
        SetHexFlag(hx, hy, FH_GAG_ITEM);
    if (is_trap)
        SetHexFlag(hx, hy, FH_TRIGGER);
    if (is_trigger)
        SetHexFlag(hx, hy, FH_TRIGGER);
}

ushort Map::GetHexFlags(ushort hx, ushort hy)
{
    return (hexFlags[hy * GetWidth() + hx] << 8) | GetStaticMap()->HexFlags[hy * GetWidth() + hx];
}

void Map::SetHexFlag(ushort hx, ushort hy, uchar flag)
{
    SETFLAG(hexFlags[hy * GetWidth() + hx], flag);
}

void Map::UnsetHexFlag(ushort hx, ushort hy, uchar flag)
{
    UNSETFLAG(hexFlags[hy * GetWidth() + hx], flag);
}

bool Map::IsHexPassed(ushort hx, ushort hy)
{
    return !FLAG(GetHexFlags(hx, hy), FH_NOWAY);
}

bool Map::IsHexRaked(ushort hx, ushort hy)
{
    return !FLAG(GetHexFlags(hx, hy), FH_NOSHOOT);
}

bool Map::IsHexesPassed(ushort hx, ushort hy, uint radius)
{
    // Base
    if (FLAG(GetHexFlags(hx, hy), FH_NOWAY))
        return false;
    if (!radius)
        return true;

    // Neighbors
    short *sx, *sy;
    geomHelper.GetHexOffsets(hx & 1, sx, sy);
    uint count = GenericUtils::NumericalNumber(radius) * settings.MapDirCount;
    short maxhx = GetWidth();
    short maxhy = GetHeight();
    for (uint i = 0; i < count; i++)
    {
        short hx_ = (short)hx + sx[i];
        short hy_ = (short)hy + sy[i];
        if (hx_ >= 0 && hy_ >= 0 && hx_ < maxhx && hy_ < maxhy && FLAG(GetHexFlags(hx_, hy_), FH_NOWAY))
            return false;
    }
    return true;
}

bool Map::IsMovePassed(ushort hx, ushort hy, uchar dir, uint multihex)
{
    // Single hex
    if (!multihex)
        return IsHexPassed(hx, hy);

    // Multihex
    // Base hex
    int hx_ = hx, hy_ = hy;
    for (uint k = 0; k < multihex; k++)
        geomHelper.MoveHexByDirUnsafe(hx_, hy_, dir);
    if (hx_ < 0 || hy_ < 0 || hx_ >= GetWidth() || hy_ >= GetHeight())
        return false;
    if (!IsHexPassed(hx_, hy_))
        return false;

    // Clock wise hexes
    bool is_square_corner = (!settings.MapHexagonal && IS_DIR_CORNER(dir));
    uint steps_count = (is_square_corner ? multihex * 2 : multihex);
    int dir_ = (settings.MapHexagonal ? ((dir + 2) % 6) : ((dir + 2) % 8));
    if (is_square_corner)
        dir_ = (dir_ + 1) % 8;
    int hx__ = hx_, hy__ = hy_;
    for (uint k = 0; k < steps_count; k++)
    {
        geomHelper.MoveHexByDirUnsafe(hx__, hy__, dir_);
        if (!IsHexPassed(hx__, hy__))
            return false;
    }

    // Counter clock wise hexes
    dir_ = (settings.MapHexagonal ? ((dir + 4) % 6) : ((dir + 6) % 8));
    if (is_square_corner)
        dir_ = (dir_ + 7) % 8;
    hx__ = hx_, hy__ = hy_;
    for (uint k = 0; k < steps_count; k++)
    {
        geomHelper.MoveHexByDirUnsafe(hx__, hy__, dir_);
        if (!IsHexPassed(hx__, hy__))
            return false;
    }
    return true;
}

bool Map::IsHexTrigger(ushort hx, ushort hy)
{
    return FLAG(hexFlags[hy * GetWidth() + hx], FH_TRIGGER);
}

bool Map::IsHexCritter(ushort hx, ushort hy)
{
    return FLAG(hexFlags[hy * GetWidth() + hx], FH_CRITTER | FH_DEAD_CRITTER);
}

bool Map::IsHexGag(ushort hx, ushort hy)
{
    return FLAG(hexFlags[hy * GetWidth() + hx], FH_GAG_ITEM);
}

bool Map::IsHexStaticTrigger(ushort hx, ushort hy)
{
    return FLAG(GetStaticMap()->HexFlags[hy * GetWidth() + hx], FH_STATIC_TRIGGER);
}

bool Map::IsFlagCritter(ushort hx, ushort hy, bool dead)
{
    if (dead)
        return FLAG(hexFlags[hy * GetWidth() + hx], FH_DEAD_CRITTER);
    else
        return FLAG(hexFlags[hy * GetWidth() + hx], FH_CRITTER);
}

void Map::SetFlagCritter(ushort hx, ushort hy, uint multihex, bool dead)
{
    if (dead)
    {
        SetHexFlag(hx, hy, FH_DEAD_CRITTER);
    }
    else
    {
        SetHexFlag(hx, hy, FH_CRITTER);

        if (multihex)
        {
            short *sx, *sy;
            geomHelper.GetHexOffsets(hx & 1, sx, sy);
            int count = GenericUtils::NumericalNumber(multihex) * settings.MapDirCount;
            short maxhx = GetWidth();
            short maxhy = GetHeight();
            for (int i = 0; i < count; i++)
            {
                short hx_ = (short)hx + sx[i];
                short hy_ = (short)hy + sy[i];
                if (hx_ >= 0 && hy_ >= 0 && hx_ < maxhx && hy_ < maxhy)
                    SetHexFlag(hx_, hy_, FH_CRITTER);
            }
        }
    }
}

void Map::UnsetFlagCritter(ushort hx, ushort hy, uint multihex, bool dead)
{
    if (dead)
    {
        uint dead_count = 0;
        for (Critter* cr : mapCritters)
            if (cr->GetHexX() == hx && cr->GetHexY() == hy && cr->IsDead())
                dead_count++;

        if (dead_count <= 1)
            UnsetHexFlag(hx, hy, FH_DEAD_CRITTER);
    }
    else
    {
        UnsetHexFlag(hx, hy, FH_CRITTER);

        if (multihex > 0)
        {
            short *sx, *sy;
            geomHelper.GetHexOffsets(hx & 1, sx, sy);
            int count = GenericUtils::NumericalNumber(multihex) * settings.MapDirCount;
            short maxhx = GetWidth();
            short maxhy = GetHeight();
            for (int i = 0; i < count; i++)
            {
                short hx_ = (short)hx + sx[i];
                short hy_ = (short)hy + sy[i];
                if (hx_ >= 0 && hy_ >= 0 && hx_ < maxhx && hy_ < maxhy)
                    UnsetHexFlag(hx_, hy_, FH_CRITTER);
            }
        }
    }
}

uint Map::GetNpcCount(int npc_role, int find_type)
{
    uint result = 0;
    for (Npc* npc : GetNpcs())
        if (npc->GetNpcRole() == npc_role && npc->CheckFind(find_type))
            result++;
    return result;
}

Critter* Map::GetCritter(uint crid)
{
    for (Critter* cr : mapCritters)
        if (cr->GetId() == crid)
            return cr;
    return nullptr;
}

Critter* Map::GetNpc(int npc_role, int find_type, uint skip_count)
{
    for (Npc* npc : mapNpcs)
    {
        if (npc->GetNpcRole() == npc_role && npc->CheckFind(find_type))
        {
            if (skip_count)
                skip_count--;
            else
                return npc;
        }
    }
    return nullptr;
}

Critter* Map::GetHexCritter(ushort hx, ushort hy, bool dead)
{
    if (!IsFlagCritter(hx, hy, dead))
        return nullptr;

    for (Critter* cr : mapCritters)
    {
        if (cr->IsDead() == dead)
        {
            int mh = cr->GetMultihex();
            if (!mh)
            {
                if (cr->GetHexX() == hx && cr->GetHexY() == hy)
                    return cr;
            }
            else
            {
                if (geomHelper.CheckDist(cr->GetHexX(), cr->GetHexY(), hx, hy, mh))
                    return cr;
            }
        }
    }
    return nullptr;
}

void Map::GetCrittersHex(ushort hx, ushort hy, uint radius, int find_type, CritterVec& critters)
{
    CritterVec find_critters;
    find_critters.reserve(mapCritters.size());
    for (Critter* cr : mapCritters)
    {
        if (cr->CheckFind(find_type) &&
            geomHelper.CheckDist(hx, hy, cr->GetHexX(), cr->GetHexY(), radius + cr->GetMultihex()))
            find_critters.push_back(cr);
    }

    // Store result, append
    if (!find_critters.empty())
    {
        critters.reserve(critters.size() + find_critters.size());
        critters.insert(critters.end(), find_critters.begin(), find_critters.end());
    }
}

CritterVec Map::GetCritters()
{
    return mapCritters;
}

ClientVec Map::GetPlayers()
{
    return mapPlayers;
}

NpcVec Map::GetNpcs()
{
    return mapNpcs;
}

CritterVec& Map::GetCrittersRaw()
{
    return mapCritters;
}

ClientVec& Map::GetPlayersRaw()
{
    return mapPlayers;
}

NpcVec& Map::GetNpcsRaw()
{
    return mapNpcs;
}

uint Map::GetCrittersCount()
{
    return (uint)mapCritters.size();
}

uint Map::GetPlayersCount()
{
    return (uint)mapPlayers.size();
}

uint Map::GetNpcsCount()
{
    return (uint)mapNpcs.size();
}

void Map::SendEffect(hash eff_pid, ushort hx, ushort hy, ushort radius)
{
    for (Client* cl : mapPlayers)
        if (geomHelper.CheckDist(cl->GetHexX(), cl->GetHexY(), hx, hy, cl->LookCacheValue + radius))
            cl->Send_Effect(eff_pid, hx, hy, radius);
}

void Map::SendFlyEffect(
    hash eff_pid, uint from_crid, uint to_crid, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy)
{
    for (Client* cl : mapPlayers)
        if (GenericUtils::IntersectCircleLine(
                cl->GetHexX(), cl->GetHexY(), cl->LookCacheValue, from_hx, from_hy, to_hx, to_hy))
            cl->Send_FlyEffect(eff_pid, from_crid, to_crid, from_hx, from_hy, to_hx, to_hy);
}

bool Map::SetScript(asIScriptFunction* func, bool first_time)
{
    if (func)
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
    }
    return true;
}

void Map::SetText(ushort hx, ushort hy, uint color, const string& text, bool unsafe_text)
{
    if (hx >= GetWidth() || hy >= GetHeight())
        return;

    for (Client* cl : mapPlayers)
    {
        if (cl->LookCacheValue >= geomHelper.DistGame(hx, hy, cl->GetHexX(), cl->GetHexY()))
            cl->Send_MapText(hx, hy, color, text, unsafe_text);
    }
}

void Map::SetTextMsg(ushort hx, ushort hy, uint color, ushort text_msg, uint num_str)
{
    if (hx >= GetWidth() || hy >= GetHeight() || !num_str)
        return;

    for (Client* cl : mapPlayers)
    {
        if (cl->LookCacheValue >= geomHelper.DistGame(hx, hy, cl->GetHexX(), cl->GetHexY()))
            cl->Send_MapTextMsg(hx, hy, color, text_msg, num_str);
    }
}

void Map::SetTextMsgLex(
    ushort hx, ushort hy, uint color, ushort text_msg, uint num_str, const char* lexems, ushort lexems_len)
{
    if (hx >= GetWidth() || hy >= GetHeight() || !num_str)
        return;

    for (Client* cl : mapPlayers)
    {
        if (cl->LookCacheValue >= geomHelper.DistGame(hx, hy, cl->GetHexX(), cl->GetHexY()))
            cl->Send_MapTextMsgLex(hx, hy, color, text_msg, num_str, lexems, lexems_len);
    }
}

void Map::GetStaticItemTriggers(ushort hx, ushort hy, ItemVec& triggers)
{
    for (auto& item : staticMap->TriggerItemsVec)
        if (item->GetHexX() == hx && item->GetHexY() == hy)
            triggers.push_back(item);
}

Item* Map::GetStaticItem(ushort hx, ushort hy, hash pid)
{
    for (auto& item : staticMap->StaticItemsVec)
        if ((!pid || item->GetProtoId() == pid) && item->GetHexX() == hx && item->GetHexY() == hy)
            return item;
    return nullptr;
}

void Map::GetStaticItemsHex(ushort hx, ushort hy, ItemVec& items)
{
    for (auto& item : staticMap->StaticItemsVec)
        if (item->GetHexX() == hx && item->GetHexY() == hy)
            items.push_back(item);
}

void Map::GetStaticItemsHexEx(ushort hx, ushort hy, uint radius, hash pid, ItemVec& items)
{
    for (auto& item : staticMap->StaticItemsVec)
        if ((!pid || item->GetProtoId() == pid) &&
            geomHelper.DistGame(item->GetHexX(), item->GetHexY(), hx, hy) <= radius)
            items.push_back(item);
}

void Map::GetStaticItemsByPid(hash pid, ItemVec& items)
{
    for (auto& item : staticMap->StaticItemsVec)
        if (!pid || item->GetProtoId() == pid)
            items.push_back(item);
}
