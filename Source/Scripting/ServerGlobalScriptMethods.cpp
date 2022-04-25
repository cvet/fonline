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

#include "Common.h"

#include "FileSystem.h"
#include "GenericUtils.h"
#include "GeometryHelper.h"
#include "PropertiesSerializator.h"
#include "ScriptSystem.h"
#include "Server.h"
#include "StringUtils.h"
#include "TwoBitMask.h"

// ReSharper disable CppInconsistentNaming

///@ ExportMethod
[[maybe_unused]] uint Server_Game_DeferredCall(FOServer* server, uint delay, ScriptFuncName<void> func)
{
    return server->DeferredCallMngr.AddDeferredCall(delay, false, func, nullptr, nullptr, nullptr, nullptr);
}

///@ ExportMethod
[[maybe_unused]] uint Server_Game_DeferredCall(FOServer* server, uint delay, ScriptFuncName<void, int> func, int value)
{
    return server->DeferredCallMngr.AddDeferredCall(delay, false, func, &value, nullptr, nullptr, nullptr);
}

///@ ExportMethod
[[maybe_unused]] uint Server_Game_DeferredCall(FOServer* server, uint delay, ScriptFuncName<void, uint> func, uint value)
{
    return server->DeferredCallMngr.AddDeferredCall(delay, false, func, nullptr, nullptr, &value, nullptr);
}

///@ ExportMethod
[[maybe_unused]] uint Server_Game_DeferredCall(FOServer* server, uint delay, ScriptFuncName<void, vector<int>> func, const vector<int>& values)
{
    return server->DeferredCallMngr.AddDeferredCall(delay, false, func, nullptr, &values, nullptr, nullptr);
}

///@ ExportMethod
[[maybe_unused]] uint Server_Game_DeferredCall(FOServer* server, uint delay, ScriptFuncName<void, vector<uint>> func, const vector<uint>& values)
{
    return server->DeferredCallMngr.AddDeferredCall(delay, false, func, nullptr, nullptr, nullptr, &values);
}

///@ ExportMethod
[[maybe_unused]] uint Server_Game_SavedDeferredCall(FOServer* server, uint delay, ScriptFuncName<void> func)
{
    return server->DeferredCallMngr.AddDeferredCall(delay, true, func, nullptr, nullptr, nullptr, nullptr);
}

///@ ExportMethod
[[maybe_unused]] uint Server_Game_SavedDeferredCall(FOServer* server, uint delay, ScriptFuncName<void, int> func, int value)
{
    return server->DeferredCallMngr.AddDeferredCall(delay, true, func, &value, nullptr, nullptr, nullptr);
}

///@ ExportMethod
[[maybe_unused]] uint Server_Game_SavedDeferredCall(FOServer* server, uint delay, ScriptFuncName<void, uint> func, uint value)
{
    return server->DeferredCallMngr.AddDeferredCall(delay, true, func, nullptr, nullptr, &value, nullptr);
}

///@ ExportMethod
[[maybe_unused]] uint Server_Game_SavedDeferredCall(FOServer* server, uint delay, ScriptFuncName<void, vector<int>> func, const vector<int>& values)
{
    return server->DeferredCallMngr.AddDeferredCall(delay, true, func, nullptr, &values, nullptr, nullptr);
}

///@ ExportMethod
[[maybe_unused]] uint Server_Game_SavedDeferredCall(FOServer* server, uint delay, ScriptFuncName<void, vector<uint>> func, const vector<uint>& values)
{
    return server->DeferredCallMngr.AddDeferredCall(delay, true, func, nullptr, nullptr, nullptr, &values);
}

///@ ExportMethod
[[maybe_unused]] bool Server_Game_IsDeferredCallPending(FOServer* server, uint id)
{
    return server->DeferredCallMngr.IsDeferredCallPending(id);
}

///@ ExportMethod
[[maybe_unused]] bool Server_Game_CancelDeferredCall(FOServer* server, uint id)
{
    return server->DeferredCallMngr.CancelDeferredCall(id);
}

///@ ExportMethod
[[maybe_unused]] bool Server_Game_GetDeferredCallData(FOServer* server, uint id, uint& delay, vector<int>& values)
{
    /*ScriptInvoker* self = Script::GetInvoker();
    DeferredCall call;
    if (self->GetDeferredCallData(id, call)) {
        delay = (call.FireFullSecond > GameOpt.FullSecond ? (call.FireFullSecond - GameOpt.FullSecond) * Globals->GetTimeMultiplier() / 1000 : 0);
        if (values) {
            if (call.IsValue) {
                values->Resize(1);
                *(int*)values->At(0) = call.Value;
            }
            else if (call.IsValues) {
                values->Resize(0);
                Script::AppendVectorToArray(call.Values, values);
            }
        }
        return true;
    }*/
    return false;
}

///@ ExportMethod
[[maybe_unused]] uint Server_Game_GetDeferredCallsList(FOServer* server, const vector<int>& ids)
{
    /*ScriptInvoker* self = Script::GetInvoker();
    IntVec ids_;
    self->GetDeferredCallsList(ids_);
    if (ids)
        Script::AppendVectorToArray(ids_, ids);
    return ids_.size();*/
    return 0;
}

///# ...
///# param pid ...
///# param props ...
///# return ...
///@ ExportMethod
[[maybe_unused]] ItemProto* Server_Game_GetItemProto(FOServer* server, hstring pid)
{
    return server->ProtoMngr.GetProtoItem(pid);
}

///# ...
///# param hx1 ...
///# param hy1 ...
///# param hx2 ...
///# param hy2 ...
///# return ...
///@ ExportMethod
[[maybe_unused]] int Server_Game_GetDistance(FOServer* server, ushort hx1, ushort hy1, ushort hx2, ushort hy2)
{
    return server->GeomHelper.DistGame(hx1, hy1, hx2, hy2);
}

///# ...
///# param cr1 ...
///# param cr2 ...
///# return ...
///@ ExportMethod
[[maybe_unused]] int Server_Game_GetDistance(FOServer* server, Critter* cr1, Critter* cr2)
{
    if (cr1 == nullptr) {
        throw ScriptException("Critter1 arg is null");
    }
    if (cr2 == nullptr) {
        throw ScriptException("Critter2 arg is null");
    }
    if (cr1->GetMapId() != cr2->GetMapId()) {
        throw ScriptException("Differernt maps");
    }

    const auto dist = server->GeomHelper.DistGame(cr1->GetHexX(), cr1->GetHexY(), cr2->GetHexX(), cr2->GetHexY());
    return static_cast<int>(dist);
}

///# ...
///# param fromHx ...
///# param fromHy ...
///# param toHx ...
///# param toHy ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uchar Server_Game_GetDirection(FOServer* server, ushort fromHx, ushort fromHy, ushort toHx, ushort toHy)
{
    return server->GeomHelper.GetFarDir(fromHx, fromHy, toHx, toHy);
}

///# ...
///# param fromHx ...
///# param fromHy ...
///# param toHx ...
///# param toHy ...
///# param offset ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uchar Server_Game_GetDirection(FOServer* server, ushort fromHx, ushort fromHy, ushort toHx, ushort toHy, float offset)
{
    return server->GeomHelper.GetFarDir(fromHx, fromHy, toHx, toHy, offset);
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Server_Game_GetTick(FOServer* server)
{
    return server->GameTime.FrameTick();
}

///# ...
///# param itemId ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Item* Server_Game_GetItem(FOServer* server, uint itemId)
{
    if (itemId == 0u) {
        throw ScriptException("Item id arg is zero");
    }

    auto* item = server->ItemMngr.GetItem(itemId);
    if (!item || item->IsDestroyed()) {
        return static_cast<Item*>(nullptr);
    }

    return item;
}

///# ...
///# param item ...
///# param count ...
///# param toCr ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_MoveItem(FOServer* server, Item* item, uint count, Critter* toCr)
{
    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }
    if (toCr == nullptr) {
        throw ScriptException("Critter arg is null");
    }

    auto count_ = count;
    if (count_ == 0u) {
        count_ = item->GetCount();
    }
    if (count_ > item->GetCount()) {
        throw ScriptException("Count arg is greater than maximum");
    }

    server->ItemMngr.MoveItem(item, count_, toCr, false);
}

///# ...
///# param item ...
///# param count ...
///# param toCr ...
///# param skipChecks ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_MoveItem(FOServer* server, Item* item, uint count, Critter* toCr, bool skipChecks)
{
    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }
    if (toCr == nullptr) {
        throw ScriptException("Critter arg is null");
    }

    auto count_ = count;
    if (count_ == 0u) {
        count_ = item->GetCount();
    }
    if (count_ > item->GetCount()) {
        throw ScriptException("Count arg is greater than maximum");
    }

    server->ItemMngr.MoveItem(item, count_, toCr, skipChecks);
}

///# ...
///# param item ...
///# param count ...
///# param toMap ...
///# param toHx ...
///# param toHy ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_MoveItem(FOServer* server, Item* item, uint count, Map* toMap, ushort toHx, ushort toHy)
{
    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }
    if (toMap == nullptr) {
        throw ScriptException("Map arg is null");
    }
    if (toHx >= toMap->GetWidth() || toHy >= toMap->GetHeight()) {
        throw ScriptException("Invalid hexex args");
    }

    auto count_ = count;
    if (count_ == 0u) {
        count_ = item->GetCount();
    }
    if (count_ > item->GetCount()) {
        throw ScriptException("Count arg is greater than maximum");
    }

    server->ItemMngr.MoveItem(item, count_, toMap, toHx, toHy, false);
}

///# ...
///# param item ...
///# param count ...
///# param toMap ...
///# param toHx ...
///# param toHy ...
///# param skipChecks ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_MoveItem(FOServer* server, Item* item, uint count, Map* toMap, ushort toHx, ushort toHy, bool skipChecks)
{
    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }
    if (toMap == nullptr) {
        throw ScriptException("Map arg is null");
    }
    if (toHx >= toMap->GetWidth() || toHy >= toMap->GetHeight()) {
        throw ScriptException("Invalid hexex args");
    }

    auto count_ = count;
    if (count_ == 0u) {
        count_ = item->GetCount();
    }
    if (count_ > item->GetCount()) {
        throw ScriptException("Count arg is greater than maximum");
    }

    server->ItemMngr.MoveItem(item, count_, toMap, toHx, toHy, skipChecks);
}

///# ...
///# param item ...
///# param count ...
///# param toCont ...
///# param stackId ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_MoveItem(FOServer* server, Item* item, uint count, Item* toCont, uint stackId)
{
    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }
    if (toCont == nullptr) {
        throw ScriptException("Container arg is null");
    }

    auto count_ = count;
    if (count_ == 0u) {
        count_ = item->GetCount();
    }
    if (count_ > item->GetCount()) {
        throw ScriptException("Count arg is greater than maximum");
    }

    server->ItemMngr.MoveItem(item, count_, toCont, stackId, false);
}

///# ...
///# param item ...
///# param count ...
///# param toCont ...
///# param stackId ...
///# param skipChecks ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_MoveItem(FOServer* server, Item* item, uint count, Item* toCont, uint stackId, bool skipChecks)
{
    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }
    if (toCont == nullptr) {
        throw ScriptException("Container arg is null");
    }

    auto count_ = count;
    if (count_ == 0u) {
        count_ = item->GetCount();
    }
    if (count_ > item->GetCount()) {
        throw ScriptException("Count arg is greater than maximum");
    }

    server->ItemMngr.MoveItem(item, count_, toCont, stackId, skipChecks);
}

///# ...
///# param items ...
///# param toCr ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_MoveItems(FOServer* server, const vector<Item*>& items, Critter* toCr)
{
    if (toCr == nullptr) {
        throw ScriptException("Critter arg is null");
    }

    for (auto* item : items) {
        if (item == nullptr || item->IsDestroyed()) {
            continue;
        }

        server->ItemMngr.MoveItem(item, item->GetCount(), toCr, false);
    }
}

///# ...
///# param items ...
///# param toCr ...
///# param skipChecks ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_MoveItems(FOServer* server, const vector<Item*>& items, Critter* toCr, bool skipChecks)
{
    if (toCr == nullptr) {
        throw ScriptException("Critter arg is null");
    }

    for (auto* item : items) {
        if (item == nullptr || item->IsDestroyed()) {
            continue;
        }

        server->ItemMngr.MoveItem(item, item->GetCount(), toCr, skipChecks);
    }
}

///# ...
///# param items ...
///# param toMap ...
///# param toHx ...
///# param toHy ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_MoveItems(FOServer* server, const vector<Item*>& items, Map* toMap, ushort toHx, ushort toHy)
{
    if (toMap == nullptr) {
        throw ScriptException("Map arg is null");
    }
    if (toHx >= toMap->GetWidth() || toHy >= toMap->GetHeight()) {
        throw ScriptException("Invalid hexex args");
    }

    for (auto* item : items) {
        if (item == nullptr || item->IsDestroyed()) {
            continue;
        }

        server->ItemMngr.MoveItem(item, item->GetCount(), toMap, toHx, toHy, false);
    }
}

///# ...
///# param items ...
///# param toMap ...
///# param toHx ...
///# param toHy ...
///# param skipChecks ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_MoveItems(FOServer* server, const vector<Item*>& items, Map* toMap, ushort toHx, ushort toHy, bool skipChecks)
{
    if (toMap == nullptr) {
        throw ScriptException("Map arg is null");
    }
    if (toHx >= toMap->GetWidth() || toHy >= toMap->GetHeight()) {
        throw ScriptException("Invalid hexex args");
    }

    for (auto* item : items) {
        if (item == nullptr || item->IsDestroyed()) {
            continue;
        }

        server->ItemMngr.MoveItem(item, item->GetCount(), toMap, toHx, toHy, skipChecks);
    }
}

///# ...
///# param items ...
///# param toCont ...
///# param stackId ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_MoveItems(FOServer* server, const vector<Item*>& items, Item* toCont, uint stackId)
{
    if (toCont == nullptr) {
        throw ScriptException("Container arg is null");
    }

    for (auto* item : items) {
        if (item == nullptr || item->IsDestroyed()) {
            continue;
        }

        server->ItemMngr.MoveItem(item, item->GetCount(), toCont, stackId, false);
    }
}

///# ...
///# param items ...
///# param toCont ...
///# param stackId ...
///# param skipChecks ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_MoveItems(FOServer* server, const vector<Item*>& items, Item* toCont, uint stackId, bool skipChecks)
{
    if (toCont == nullptr) {
        throw ScriptException("Container arg is null");
    }

    for (auto* item : items) {
        if (item == nullptr || item->IsDestroyed()) {
            continue;
        }

        server->ItemMngr.MoveItem(item, item->GetCount(), toCont, stackId, skipChecks);
    }
}

///# ...
///# param item ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_DeleteItem(FOServer* server, Item* item)
{
    if (item != nullptr) {
        server->ItemMngr.DeleteItem(item);
    }
}

///# ...
///# param item ...
///# param count ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_DeleteItem(FOServer* server, Item* item, uint count)
{
    if (item != nullptr && count > 0u) {
        const auto cur_count = item->GetCount();
        if (count >= cur_count) {
            server->ItemMngr.DeleteItem(item);
        }
        else {
            item->SetCount(cur_count - count);
        }
    }
}

///# ...
///# param itemId ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_DeleteItem(FOServer* server, uint itemId)
{
    if (auto* item = server->ItemMngr.GetItem(itemId); item != nullptr) {
        server->ItemMngr.DeleteItem(item);
    }
}

///# ...
///# param itemId ...
///# param count ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_DeleteItem(FOServer* server, uint itemId, uint count)
{
    if (auto* item = server->ItemMngr.GetItem(itemId); item != nullptr && count > 0u) {
        const auto cur_count = item->GetCount();
        if (count >= cur_count) {
            server->ItemMngr.DeleteItem(item);
        }
        else {
            item->SetCount(cur_count - count);
        }
    }
}

///# ...
///# param items ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_DeleteItems(FOServer* server, const vector<Item*>& items)
{
    for (auto* item : items) {
        if (item != nullptr) {
            server->ItemMngr.DeleteItem(item);
        }
    }
}

///# ...
///# param itemIds ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_DeleteItems(FOServer* server, const vector<uint>& itemIds)
{
    for (const auto item_id : itemIds) {
        if (item_id != 0u) {
            auto* item = server->ItemMngr.GetItem(item_id);
            if (item != nullptr) {
                server->ItemMngr.DeleteItem(item);
            }
        }
    }
}

///# ...
///# param cr ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_DeleteCritter(FOServer* server, Critter* cr)
{
    if (cr != nullptr && cr->IsNpc()) {
        server->CrMngr.DeleteNpc(cr);
    }
}

///# ...
///# param crId ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_DeleteCritter(FOServer* server, uint crId)
{
    if (crId != 0u) {
        if (Critter* cr = server->CrMngr.GetCritter(crId); cr != nullptr && cr->IsNpc()) {
            server->CrMngr.DeleteNpc(cr);
        }
    }
}

///# ...
///# param critters ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_DeleteCritters(FOServer* server, const vector<Critter*>& critters)
{
    for (auto* cr : critters) {
        if (cr != nullptr && cr->IsNpc()) {
            server->CrMngr.DeleteNpc(cr);
        }
    }
}

///# ...
///# param critterIds ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_DeleteCritters(FOServer* server, const vector<uint>& critterIds)
{
    for (const auto id : critterIds) {
        if (id != 0u) {
            if (Critter* cr = server->CrMngr.GetCritter(id); cr != nullptr && cr->IsNpc()) {
                server->CrMngr.DeleteNpc(cr);
            }
        }
    }
}

///# ...
///# param channel ...
///# param text ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_RadioMessage(FOServer* server, ushort channel, string_view text)
{
    if (!text.empty()) {
        server->ItemMngr.RadioSendTextEx(channel, RADIO_BROADCAST_FORCE_ALL, 0, 0, 0, text, false, 0, 0, "");
    }
}

///# ...
///# param channel ...
///# param textMsg ...
///# param numStr ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_RadioMessageMsg(FOServer* server, ushort channel, ushort textMsg, uint numStr)
{
    server->ItemMngr.RadioSendTextEx(channel, RADIO_BROADCAST_FORCE_ALL, 0, 0, 0, "", false, textMsg, numStr, "");
}

///# ...
///# param channel ...
///# param textMsg ...
///# param numStr ...
///# param lexems ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_RadioMessageMsg(FOServer* server, ushort channel, ushort textMsg, uint numStr, string_view lexems)
{
    server->ItemMngr.RadioSendTextEx(channel, RADIO_BROADCAST_FORCE_ALL, 0, 0, 0, "", false, textMsg, numStr, lexems);
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Server_Game_GetFullSecond(FOServer* server)
{
    return server->GameTime.GetFullSecond();
}

///# ...
///# param year ...
///# param month ...
///# param day ...
///# param hour ...
///# param minute ...
///# param second ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Server_Game_EvaluateFullSecond(FOServer* server, ushort year, ushort month, ushort day, ushort hour, ushort minute, ushort second)
{
    if (year && year < server->Settings.StartYear) {
        throw ScriptException("Invalid year", year);
    }
    if (year && year > server->Settings.StartYear + 100) {
        throw ScriptException("Invalid year", year);
    }
    if (month != 0u && month < 1) {
        throw ScriptException("Invalid month", month);
    }
    if (month != 0u && month > 12) {
        throw ScriptException("Invalid month", month);
    }

    auto year_ = year;
    auto month_ = month;
    auto day_ = day;

    if (year_ == 0u) {
        year_ = server->GetYear();
    }
    if (month_ == 0u) {
        month_ = server->GetMonth();
    }

    if (day_ != 0u) {
        const auto month_day = server->GameTime.GameTimeMonthDay(year, month_);
        if (day_ < month_day || day_ > month_day) {
            throw ScriptException("Invalid day", day_, month_day);
        }
    }

    if (day_ == 0u) {
        day_ = server->GetDay();
    }

    if (hour > 23) {
        throw ScriptException("Invalid hour", hour);
    }
    if (minute > 59) {
        throw ScriptException("Invalid minute", minute);
    }
    if (second > 59) {
        throw ScriptException("Invalid second", second);
    }

    return server->GameTime.EvaluateFullSecond(year_, month_, day_, hour, minute, second);
}

///# ...
///# param fullSecond ...
///# param year ...
///# param month ...
///# param day ...
///# param dayOfWeek ...
///# param hour ...
///# param minute ...
///# param second ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_GetGameTime(FOServer* server, uint fullSecond, ushort& year, ushort& month, ushort& day, ushort& dayOfWeek, ushort& hour, ushort& minute, ushort& second)
{
    const auto dt = server->GameTime.GetGameTime(fullSecond);

    year = dt.Year;
    month = dt.Month;
    dayOfWeek = dt.DayOfWeek;
    day = dt.Day;
    hour = dt.Hour;
    minute = dt.Minute;
    second = dt.Second;
}

///# ...
///# param locPid ...
///# param wx ...
///# param wy ...
///# param critters ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Location* Server_Game_CreateLocation(FOServer* server, hstring locPid, ushort wx, ushort wy, const vector<Critter*>& critters)
{
    // Create and generate location
    auto* loc = server->MapMngr.CreateLocation(locPid, wx, wy);
    if (!loc) {
        throw ScriptException("Unable to create location '{}'.", locPid);
    }

    // Add known locations to critters
    for (auto* cr : critters) {
        server->MapMngr.AddKnownLoc(cr, loc->GetId());

        if (cr->GetMapId() == 0u) {
            cr->Send_GlobalLocation(loc, true);
        }
        if (loc->IsNonEmptyAutomaps()) {
            cr->Send_AutomapsInfo(nullptr, loc);
        }

        const ushort zx = loc->GetWorldX() / server->Settings.GlobalMapZoneLength;
        const ushort zy = loc->GetWorldY() / server->Settings.GlobalMapZoneLength;

        auto gmap_fog = cr->GetGlobalMapFog();
        if (gmap_fog.size() != GM_ZONES_FOG_SIZE) {
            gmap_fog.resize(GM_ZONES_FOG_SIZE);
        }

        auto gmap_mask = TwoBitMask(GM_MAXZONEX, GM_MAXZONEY, gmap_fog.data());
        if (gmap_mask.Get2Bit(zx, zy) == GM_FOG_FULL) {
            gmap_mask.Set2Bit(zx, zy, GM_FOG_HALF);
            cr->SetGlobalMapFog(gmap_fog);
            if (cr->GetMapId() == 0u) {
                cr->Send_GlobalMapFog(zx, zy, GM_FOG_HALF);
            }
        }
    }

    return loc;
}

///# ...
///# param loc ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_DeleteLocation(FOServer* server, Location* loc)
{
    if (loc != nullptr) {
        server->MapMngr.DeleteLocation(loc, nullptr);
    }
}

///# ...
///# param locId ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_DeleteLocation(FOServer* server, uint locId)
{
    auto* loc = server->MapMngr.GetLocation(locId);
    if (loc) {
        server->MapMngr.DeleteLocation(loc, nullptr);
    }
}

///# ...
///# param crId ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Critter* Server_Game_GetCritter(FOServer* server, uint crId)
{
    if (crId == 0u) {
        return nullptr;
    }

    return server->CrMngr.GetCritter(crId);
}

///# ...
///# param name ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] Player* Server_Game_GetPlayer(FOServer* server, string_view name)
{
    // Check existence
    const auto id = server->MakePlayerId(name);
    const auto doc = server->DbStorage.Get("Players", id);
    if (doc.empty()) {
        return static_cast<Player*>(nullptr);
    }

    // Find online
    auto* player = server->CrMngr.GetPlayerById(id);
    if (player != nullptr) {
        player->AddRef();
        return player;
    }

    // Load from db
    const auto player_proto_id = server->ToHashedString("Player");
    const auto* player_proto = server->ProtoMngr.GetProtoCritter(player_proto_id);
    RUNTIME_ASSERT(player_proto);

    player = new Player(server, id, name, nullptr, player_proto);

    if (!PropertiesSerializator::LoadFromDocument(&player->GetPropertiesForEdit(), doc, *server)) {
        throw ScriptException("Player data db read failed");
    }

    return player;
}

///# ...
///# param wx ...
///# param wy ...
///# param radius ...
///# param findType ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Critter*> Server_Game_GetGlobalMapCritters(FOServer* server, ushort wx, ushort wy, uint radius, CritterFindType findType)
{
    return server->CrMngr.GetGlobalMapCritters(wx, wy, radius, findType);
}

///# ...
///# param mapId ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Map* Server_Game_GetMap(FOServer* server, uint mapId)
{
    if (mapId == 0u) {
        throw ScriptException("Map id arg is zero");
    }

    return server->MapMngr.GetMap(mapId);
}

///# ...
///# param mapPid ...
///# param skipCount ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Map* Server_Game_GetMapByPid(FOServer* server, hstring mapPid, uint skipCount)
{
    if (!mapPid) {
        throw ScriptException("Invalid zero map proto id arg");
    }

    return server->MapMngr.GetMapByPid(mapPid, skipCount);
}

///# ...
///# param locId ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Location* Server_Game_GetLocation(FOServer* server, uint locId)
{
    if (locId == 0u) {
        throw ScriptException("Location id arg is zero");
    }

    return server->MapMngr.GetLocation(locId);
}

///# ...
///# param locPid ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Location* Server_Game_GetLocationByPid(FOServer* server, hstring locPid)
{
    if (!locPid) {
        throw ScriptException("Invalid zero location proto id arg");
    }

    return server->MapMngr.GetLocationByPid(locPid, 0);
}

///# ...
///# param locPid ...
///# param skipCount ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Location* Server_Game_GetLocationByPid(FOServer* server, hstring locPid, uint skipCount)
{
    if (!locPid) {
        throw ScriptException("Invalid zero location proto id arg");
    }

    return server->MapMngr.GetLocationByPid(locPid, skipCount);
}

///# ...
///# param wx ...
///# param wy ...
///# param radius ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Location*> Server_Game_GetLocations(FOServer* server, ushort wx, ushort wy, uint radius)
{
    vector<Location*> locations;

    for (auto* loc : server->MapMngr.GetLocations()) {
        if (GenericUtils::DistSqrt(wx, wy, loc->GetWorldX(), loc->GetWorldY()) <= radius + loc->GetRadius()) {
            locations.push_back(loc);
        }
    }

    return locations;
}

///# ...
///# param wx ...
///# param wy ...
///# param radius ...
///# param cr ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Location*> Server_Game_GetVisibleLocations(FOServer* server, ushort wx, ushort wy, uint radius, Critter* cr)
{
    vector<Location*> locations;

    for (auto* loc : server->MapMngr.GetLocations()) {
        if (GenericUtils::DistSqrt(wx, wy, loc->GetWorldX(), loc->GetWorldY()) <= radius + loc->GetRadius() && (loc->IsLocVisible() || (cr && cr->IsPlayer() && server->MapMngr.CheckKnownLoc(cr, loc->GetId())))) {
            locations.push_back(loc);
        }
    }

    return locations;
}

///# ...
///# param zx ...
///# param zy ...
///# param zoneRadius ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Location*> Server_Game_GetZoneLocations(FOServer* server, ushort zx, ushort zy, uint zoneRadius)
{
    return server->MapMngr.GetZoneLocations(zx, zy, zoneRadius);
}

///# ...
///# param cr ...
///# param npc ...
///# param ignoreDistance ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Server_Game_RunDialog(FOServer* server, Critter* cr, Critter* npc, bool ignoreDistance)
{
    if (cr == nullptr) {
        throw ScriptException("Player arg is null");
    }
    if (!cr->IsPlayer()) {
        throw ScriptException("Player arg is not player");
    }
    if (npc == nullptr) {
        throw ScriptException("Npc arg is null");
    }
    if (!npc->IsNpc()) {
        throw ScriptException("Npc arg is not npc");
    }

    if (cr->Talk.Locked) {
        throw ScriptException("Can't open new dialog from demand, result or dialog functions");
    }

    server->BeginDialog(cr, npc, hstring(), 0, 0, ignoreDistance);

    return cr->Talk.Type == TalkType::Critter && cr->Talk.CritterId == npc->GetId();
}

///# ...
///# param cr ...
///# param npc ...
///# param dlgPack ...
///# param ignoreDistance ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Server_Game_RunDialog(FOServer* server, Critter* cr, Critter* npc, hstring dlgPack, bool ignoreDistance)
{
    if (cr == nullptr) {
        throw ScriptException("Player arg is null");
    }
    if (!cr->IsPlayer()) {
        throw ScriptException("Player arg is not player");
    }
    if (npc == nullptr) {
        throw ScriptException("Npc arg is null");
    }
    if (!npc->IsNpc()) {
        throw ScriptException("Npc arg is not npc");
    }

    if (cr->Talk.Locked) {
        throw ScriptException("Can't open new dialog from demand, result or dialog functions");
    }

    server->BeginDialog(cr, npc, dlgPack, 0, 0, ignoreDistance);

    return cr->Talk.Type == TalkType::Critter && cr->Talk.CritterId == npc->GetId();
}

///# ...
///# param cr ...
///# param dlgPack ...
///# param hx ...
///# param hy ...
///# param ignoreDistance ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Server_Game_RunDialog(FOServer* server, Critter* cr, hstring dlgPack, ushort hx, ushort hy, bool ignoreDistance)
{
    if (cr == nullptr) {
        throw ScriptException("Player arg is null");
    }
    if (!cr->IsPlayer()) {
        throw ScriptException("Player arg is not player");
    }
    if (!server->DlgMngr.GetDialog(dlgPack)) {
        throw ScriptException("Dialog not found");
    }

    if (cr->Talk.Locked) {
        throw ScriptException("Can't open new dialog from demand, result or dialog functions");
    }

    server->BeginDialog(cr, nullptr, dlgPack, hx, hy, ignoreDistance);

    return cr->Talk.Type == TalkType::Hex && cr->Talk.TalkHexX == hx && cr->Talk.TalkHexY == hy;
}

///# ...
///# param pid ...
///# return ...
///@ ExportMethod
[[maybe_unused]] int64 Server_Game_GetWorldItemCount(FOServer* server, hstring pid)
{
    if (!server->ProtoMngr.GetProtoItem(pid)) {
        throw ScriptException("Invalid protoId arg");
    }

    return server->ItemMngr.GetItemStatistics(pid);
}

///# ...
///# param sayType ...
///# param firstStr ...
///# param parameter ...
///# param func ...
///# return ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_AddTextListener(FOServer* server, int sayType, string_view firstStr, int parameter, ScriptFuncName<void, Critter, string> func)
{
    /*uint func_id = server->ScriptSys.BindByFunc(func, false);
    if (!func_id)
        throw ScriptException("Unable to bind script function");

    TextListener tl;
    tl.FuncId = func_id;
    tl.SayType = sayType;
    tl.FirstStr = firstStr;
    tl.Parameter = parameter;

    server->TextListeners.push_back(tl);*/
}

///# ...
///# param sayType ...
///# param firstStr ...
///# param parameter ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_EraseTextListener(FOServer* server, int sayType, string_view firstStr, int parameter)
{
    /*for (auto it = server->TextListeners.begin(), end = server->TextListeners.end(); it != end; ++it)
    {
        TextListener& tl = *it;
        if (sayType == tl.SayType && _str(firstStr).compareIgnoreCaseUtf8(tl.FirstStr) && tl.Parameter == parameter)
        {
            server->TextListeners.erase(it);
            return;
        }
    }*/
}

///# ...
///# param pid ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Item*> Server_Game_GetAllItems(FOServer* server, hstring pid)
{
    vector<Item*> items;
    for (auto* item : server->ItemMngr.GetItems()) {
        if (!item->IsDestroyed() && (!pid || pid == item->GetProtoId())) {
            items.push_back(item);
        }
    }

    return items;
}

///# ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] vector<Player*> Server_Game_GetOnlinePlayers(FOServer* server)
{
    return server->EntityMngr.GetPlayers();
}

///# ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] vector<uint> Server_Game_GetRegisteredPlayerIds(FOServer* server)
{
    return server->DbStorage.GetAllIds("Players");
}

///# ...
///# param pid ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Critter*> Server_Game_GetAllNpc(FOServer* server, hstring pid)
{
    vector<Critter*> npcs;

    for (auto* npc_ : server->CrMngr.GetAllNpc()) {
        if (!npc_->IsDestroyed() && (!pid || pid == npc_->GetProtoId())) {
            npcs.push_back(npc_);
        }
    }

    return npcs;
}

///# ...
///# param pid ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Map*> Server_Game_GetAllMaps(FOServer* server, hstring pid)
{
    vector<Map*> maps;

    for (auto* map : server->MapMngr.GetMaps()) {
        if (!pid || pid == map->GetProtoId()) {
            maps.push_back(map);
        }
    }

    return maps;
}

///# ...
///# param pid ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Location*> Server_Game_GetAllLocations(FOServer* server, hstring pid)
{
    vector<Location*> locations;

    for (auto* loc : server->MapMngr.GetLocations()) {
        if (!pid || pid == loc->GetProtoId()) {
            locations.push_back(loc);
        }
    }

    return locations;
}

///# ...
///# param year ...
///# param month ...
///# param day ...
///# param dayOfWeek ...
///# param hour ...
///# param minute ...
///# param second ...
///# param milliseconds ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_GetTime(FOServer* server, ushort& year, ushort& month, ushort& day, ushort& dayOfWeek, ushort& hour, ushort& minute, ushort& second, ushort& milliseconds)
{
    const auto cur_time = Timer::GetCurrentDateTime();
    year = cur_time.Year;
    month = cur_time.Month;
    dayOfWeek = cur_time.DayOfWeek;
    day = cur_time.Day;
    hour = cur_time.Hour;
    minute = cur_time.Minute;
    second = cur_time.Second;
    milliseconds = cur_time.Milliseconds;
}

///# ...
///# param multiplier ...
///# param year ...
///# param month ...
///# param day ...
///# param hour ...
///# param minute ...
///# param second ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_SetTime(FOServer* server, ushort multiplier, ushort year, ushort month, ushort day, ushort hour, ushort minute, ushort second)
{
    server->SetGameTime(multiplier, year, month, day, hour, minute, second);
}

///# ...
///# param cr ...
///# param staticItem ...
///# param param ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Server_Game_CallStaticItemFunction(FOServer* server, Critter* cr, StaticItem* staticItem, int param)
{
    if (!staticItem->SceneryScriptFunc) {
        return false;
    }

    return staticItem->SceneryScriptFunc(cr, staticItem, false, param) && staticItem->SceneryScriptFunc.GetResult();
}
