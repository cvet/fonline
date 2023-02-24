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

#include "Common.h"

#include "FileSystem.h"
#include "GenericUtils.h"
#include "Networking.h"
#include "PropertiesSerializator.h"
#include "ScriptSystem.h"
#include "Server.h"
#include "StringUtils.h"
#include "TwoBitMask.h"

// ReSharper disable CppInconsistentNaming

///@ ExportMethod
[[maybe_unused]] ident_t Server_Game_CreatePlayer(FOServer* server, string_view name, string_view password)
{
    const auto player_id = server->MakePlayerId(name);
    if (!server->DbStorage.Get("Players", player_id).empty()) {
        throw ScriptException("Player already registered", name);
    }

    server->DbStorage.Insert("Players", player_id, {{"_Name", string(name)}, {"Password", string(password)}});

    return player_id;
}

///@ ExportMethod
[[maybe_unused]] ident_t Server_Game_DeferredCall(FOServer* server, uint delay, ScriptFunc<void> func)
{
    return server->ServerDeferredCalls.AddDeferredCall(delay, func);
}

///@ ExportMethod
[[maybe_unused]] ident_t Server_Game_DeferredCall(FOServer* server, uint delay, ScriptFunc<void, int> func, int value)
{
    return server->ServerDeferredCalls.AddDeferredCall(delay, func, value);
}

///@ ExportMethod
[[maybe_unused]] ident_t Server_Game_DeferredCall(FOServer* server, uint delay, ScriptFunc<void, uint> func, uint value)
{
    return server->ServerDeferredCalls.AddDeferredCall(delay, func, value);
}

///@ ExportMethod
[[maybe_unused]] ident_t Server_Game_DeferredCall(FOServer* server, uint delay, ScriptFunc<void, vector<int>> func, const vector<int>& values)
{
    return server->ServerDeferredCalls.AddDeferredCall(delay, func, values);
}

///@ ExportMethod
[[maybe_unused]] ident_t Server_Game_DeferredCall(FOServer* server, uint delay, ScriptFunc<void, vector<uint>> func, const vector<uint>& values)
{
    return server->ServerDeferredCalls.AddDeferredCall(delay, func, values);
}

///@ ExportMethod
[[maybe_unused]] ident_t Server_Game_SavedDeferredCall(FOServer* server, uint delay, ScriptFunc<void> func)
{
    if (func.IsDelegate()) {
        throw ScriptException("Function must be global (not delegate)");
    }

    return server->ServerDeferredCalls.AddSavedDeferredCall(delay, func);
}

///@ ExportMethod
[[maybe_unused]] ident_t Server_Game_SavedDeferredCall(FOServer* server, uint delay, ScriptFunc<void, int> func, int value)
{
    if (func.IsDelegate()) {
        throw ScriptException("Function must be global (not delegate)");
    }

    return server->ServerDeferredCalls.AddSavedDeferredCall(delay, func, value);
}

///@ ExportMethod
[[maybe_unused]] ident_t Server_Game_SavedDeferredCall(FOServer* server, uint delay, ScriptFunc<void, uint> func, uint value)
{
    if (func.IsDelegate()) {
        throw ScriptException("Function must be global (not delegate)");
    }

    return server->ServerDeferredCalls.AddSavedDeferredCall(delay, func, value);
}

///@ ExportMethod
[[maybe_unused]] ident_t Server_Game_SavedDeferredCall(FOServer* server, uint delay, ScriptFunc<void, vector<int>> func, const vector<int>& values)
{
    if (func.IsDelegate()) {
        throw ScriptException("Function must be global (not delegate)");
    }

    return server->ServerDeferredCalls.AddSavedDeferredCall(delay, func, values);
}

///@ ExportMethod
[[maybe_unused]] ident_t Server_Game_SavedDeferredCall(FOServer* server, uint delay, ScriptFunc<void, vector<uint>> func, const vector<uint>& values)
{
    if (func.IsDelegate()) {
        throw ScriptException("Function must be global (not delegate)");
    }

    return server->ServerDeferredCalls.AddSavedDeferredCall(delay, func, values);
}

///@ ExportMethod
[[maybe_unused]] bool Server_Game_IsDeferredCallPending(FOServer* server, ident_t id)
{
    return server->ServerDeferredCalls.IsDeferredCallPending(id);
}

///@ ExportMethod
[[maybe_unused]] bool Server_Game_CancelDeferredCall(FOServer* server, ident_t id)
{
    return server->ServerDeferredCalls.CancelDeferredCall(id);
}

///# ...
///# param pid ...
///# param props ...
///# return ...
///@ ExportMethod
[[maybe_unused]] ProtoItem* Server_Game_GetProtoItem(FOServer* server, hstring pid)
{
    return const_cast<ProtoItem*>(server->ProtoMngr.GetProtoItem(pid));
}

///# ...
///# param cr1 ...
///# param cr2 ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Server_Game_GetDistance(FOServer* server, Critter* cr1, Critter* cr2)
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

    const auto dist = server->Geometry.DistGame(cr1->GetHexX(), cr1->GetHexY(), cr2->GetHexX(), cr2->GetHexY());
    return static_cast<int>(dist);
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
[[maybe_unused]] Item* Server_Game_GetItem(FOServer* server, ident_t itemId)
{
    if (!itemId) {
        throw ScriptException("Item id arg is zero");
    }

    auto* item = server->ItemMngr.GetItem(itemId);
    if (item == nullptr || item->IsDestroyed()) {
        return nullptr;
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
[[maybe_unused]] void Server_Game_MoveItem(FOServer* server, Item* item, uint count, Map* toMap, uint16 toHx, uint16 toHy)
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
[[maybe_unused]] void Server_Game_MoveItem(FOServer* server, Item* item, uint count, Map* toMap, uint16 toHx, uint16 toHy, bool skipChecks)
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
[[maybe_unused]] void Server_Game_MoveItems(FOServer* server, const vector<Item*>& items, Map* toMap, uint16 toHx, uint16 toHy)
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
[[maybe_unused]] void Server_Game_MoveItems(FOServer* server, const vector<Item*>& items, Map* toMap, uint16 toHx, uint16 toHy, bool skipChecks)
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
[[maybe_unused]] void Server_Game_DeleteItem(FOServer* server, ident_t itemId)
{
    if (auto* item = server->ItemMngr.GetItem(itemId); item != nullptr) {
        server->ItemMngr.DeleteItem(item);
    }
}

///# ...
///# param itemId ...
///# param count ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_DeleteItem(FOServer* server, ident_t itemId, uint count)
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
[[maybe_unused]] void Server_Game_DeleteItems(FOServer* server, const vector<ident_t>& itemIds)
{
    for (const auto item_id : itemIds) {
        if (item_id) {
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
        server->CrMngr.DeleteCritter(cr);
    }
}

///# ...
///# param crId ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_DeleteCritter(FOServer* server, ident_t crId)
{
    if (crId) {
        if (Critter* cr = server->CrMngr.GetCritter(crId); cr != nullptr && cr->IsNpc()) {
            server->CrMngr.DeleteCritter(cr);
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
            server->CrMngr.DeleteCritter(cr);
        }
    }
}

///# ...
///# param critterIds ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_DeleteCritters(FOServer* server, const vector<ident_t>& critterIds)
{
    for (const auto id : critterIds) {
        if (id) {
            if (Critter* cr = server->CrMngr.GetCritter(id); cr != nullptr && cr->IsNpc()) {
                server->CrMngr.DeleteCritter(cr);
            }
        }
    }
}

///# ...
///# param channel ...
///# param text ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_RadioMessage(FOServer* server, uint16 channel, string_view text)
{
    if (!text.empty()) {
        server->ItemMngr.RadioSendTextEx(channel, RADIO_BROADCAST_FORCE_ALL, ident_t {}, 0, 0, text, false, 0, 0, "");
    }
}

///# ...
///# param channel ...
///# param textMsg ...
///# param numStr ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_RadioMessageMsg(FOServer* server, uint16 channel, uint16 textMsg, uint numStr)
{
    server->ItemMngr.RadioSendTextEx(channel, RADIO_BROADCAST_FORCE_ALL, ident_t {}, 0, 0, "", false, textMsg, numStr, "");
}

///# ...
///# param channel ...
///# param textMsg ...
///# param numStr ...
///# param lexems ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_RadioMessageMsg(FOServer* server, uint16 channel, uint16 textMsg, uint numStr, string_view lexems)
{
    server->ItemMngr.RadioSendTextEx(channel, RADIO_BROADCAST_FORCE_ALL, ident_t {}, 0, 0, "", false, textMsg, numStr, lexems);
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
[[maybe_unused]] uint Server_Game_EvaluateFullSecond(FOServer* server, uint16 year, uint16 month, uint16 day, uint16 hour, uint16 minute, uint16 second)
{
    if (year != 0u && year < server->Settings.StartYear) {
        throw ScriptException("Invalid year", year);
    }
    if (year != 0u && year > server->Settings.StartYear + 100) {
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
        if (day_ > month_day) {
            throw ScriptException("Invalid day", day_, month_day);
        }
    }

    if (day_ == 0u) {
        day_ = server->GetDay();
    }

    if (hour > 23) {
        if (minute > 0 || second > 0) {
            throw ScriptException("Invalid hour", hour);
        }
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
[[maybe_unused]] void Server_Game_GetGameTime(FOServer* server, uint fullSecond, uint16& year, uint16& month, uint16& day, uint16& dayOfWeek, uint16& hour, uint16& minute, uint16& second)
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
///# return ...
///@ ExportMethod
[[maybe_unused]] Location* Server_Game_CreateLocation(FOServer* server, hstring locPid, uint16 wx, uint16 wy)
{
    // Create and generate location
    auto* loc = server->MapMngr.CreateLocation(locPid, wx, wy);
    if (loc == nullptr) {
        throw ScriptException("Unable to create location '{}'.", locPid);
    }

    return loc;
}

///# ...
///# param locPid ...
///# param wx ...
///# param wy ...
///# param critters ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Location* Server_Game_CreateLocation(FOServer* server, hstring locPid, uint16 wx, uint16 wy, const vector<Critter*>& critters)
{
    // Create and generate location
    auto* loc = server->MapMngr.CreateLocation(locPid, wx, wy);
    if (loc == nullptr) {
        throw ScriptException("Unable to create location {}", locPid);
    }

    // Add known locations to critters
    for (auto* cr : critters) {
        server->MapMngr.AddKnownLoc(cr, loc->GetId());

        if (!cr->GetMapId()) {
            cr->Send_GlobalLocation(loc, true);
        }
        if (loc->IsNonEmptyAutomaps()) {
            cr->Send_AutomapsInfo(nullptr, loc);
        }

        const uint16 zx = loc->GetWorldX() / server->Settings.GlobalMapZoneLength;
        const uint16 zy = loc->GetWorldY() / server->Settings.GlobalMapZoneLength;

        auto gmap_fog = cr->GetGlobalMapFog();
        if (gmap_fog.size() != GM_ZONES_FOG_SIZE) {
            gmap_fog.resize(GM_ZONES_FOG_SIZE);
        }

        auto gmap_mask = TwoBitMask(GM_MAXZONEX, GM_MAXZONEY, gmap_fog.data());
        if (gmap_mask.Get2Bit(zx, zy) == GM_FOG_FULL) {
            gmap_mask.Set2Bit(zx, zy, GM_FOG_HALF);
            cr->SetGlobalMapFog(gmap_fog);
            if (!cr->GetMapId()) {
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
        server->MapMngr.DeleteLocation(loc);
    }
}

///# ...
///# param locId ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_DeleteLocation(FOServer* server, ident_t locId)
{
    auto* loc = server->MapMngr.GetLocation(locId);
    if (loc != nullptr) {
        server->MapMngr.DeleteLocation(loc);
    }
}

///# ...
///# param crId ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Critter* Server_Game_GetCritter(FOServer* server, ident_t crId)
{
    if (!crId) {
        return nullptr;
    }

    return server->CrMngr.GetCritter(crId);
}

///# ...
///# param name ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer PassOwnership
[[maybe_unused]] Player* Server_Game_GetPlayer(FOServer* server, string_view name)
{
    // Check existence
    const auto id = server->MakePlayerId(name);
    const auto doc = server->DbStorage.Get("Players", id);
    if (doc.empty()) {
        return nullptr;
    }

    // Find online
    auto* player = server->CrMngr.GetPlayerById(id);
    if (player != nullptr) {
        player->AddRef();
        return player;
    }

    // Load from db
    auto* dummy_net_conn = new DummyNetConnection(server->Settings);

    player = new Player(server, id, new ClientConnection(dummy_net_conn));
    player->SetName(name);

    dummy_net_conn->Release();

    if (!PropertiesSerializator::LoadFromDocument(&player->GetPropertiesForEdit(), doc, *server)) {
        player->MarkAsDestroyed();
        player->Release();
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
[[maybe_unused]] vector<Critter*> Server_Game_GetGlobalMapCritters(FOServer* server, uint16 wx, uint16 wy, uint radius, CritterFindType findType)
{
    return server->CrMngr.GetGlobalMapCritters(wx, wy, radius, findType);
}

///# ...
///# param mapId ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Map* Server_Game_GetMap(FOServer* server, ident_t mapId)
{
    if (!mapId) {
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
[[maybe_unused]] Location* Server_Game_GetLocation(FOServer* server, ident_t locId)
{
    if (!locId) {
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
[[maybe_unused]] vector<Location*> Server_Game_GetLocations(FOServer* server, uint16 wx, uint16 wy, uint radius)
{
    vector<Location*> locations;
    locations.reserve(server->MapMngr.GetLocationsCount());

    for (auto&& [id, loc] : server->MapMngr.GetLocations()) {
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
[[maybe_unused]] vector<Location*> Server_Game_GetVisibleLocations(FOServer* server, uint16 wx, uint16 wy, uint radius, Critter* cr)
{
    vector<Location*> locations;
    locations.reserve(server->MapMngr.GetLocationsCount());

    for (auto&& [id, loc] : server->MapMngr.GetLocations()) {
        if (GenericUtils::DistSqrt(wx, wy, loc->GetWorldX(), loc->GetWorldY()) <= radius + loc->GetRadius() && //
            (loc->IsLocVisible() || (cr != nullptr && cr->IsOwnedByPlayer() && server->MapMngr.CheckKnownLoc(cr, loc->GetId())))) {
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
[[maybe_unused]] vector<Location*> Server_Game_GetZoneLocations(FOServer* server, uint16 zx, uint16 zy, uint zoneRadius)
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
    if (!cr->IsOwnedByPlayer()) {
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
    if (!cr->IsOwnedByPlayer()) {
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
[[maybe_unused]] bool Server_Game_RunDialog(FOServer* server, Critter* cr, hstring dlgPack, uint16 hx, uint16 hy, bool ignoreDistance)
{
    if (cr == nullptr) {
        throw ScriptException("Player arg is null");
    }
    if (!cr->IsOwnedByPlayer()) {
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
[[maybe_unused]] void Server_Game_AddTextListener(FOServer* server, int sayType, string_view firstStr, int parameter, ScriptFunc<void, Critter*, string> func)
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

    if (!pid) {
        items.reserve(server->ItemMngr.GetItemsCount());
    }

    for (auto&& [id, item] : server->ItemMngr.GetItems()) {
        RUNTIME_ASSERT(!item->IsDestroyed());
        if (!pid || pid == item->GetProtoId()) {
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
    const auto& players = server->EntityMngr.GetPlayers();

    vector<Player*> result;
    result.reserve(players.size());

    for (auto&& [id, player] : players) {
        result.push_back(player);
    }

    return result;
}

///# ...
///# return ...
///@ ExportMethod ExcludeInSingleplayer
[[maybe_unused]] vector<ident_t> Server_Game_GetRegisteredPlayerIds(FOServer* server)
{
    return server->DbStorage.GetAllIds("Players");
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Critter*> Server_Game_GetAllNpc(FOServer* server)
{
    return server->CrMngr.GetNonPlayerCritters();
}

///# ...
///# param pid ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Critter*> Server_Game_GetAllNpc(FOServer* server, hstring pid)
{
    vector<Critter*> npcs;

    for (auto* npc_ : server->CrMngr.GetNonPlayerCritters()) {
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

    if (!pid) {
        maps.reserve(server->MapMngr.GetLocationsCount());
    }

    for (auto&& [id, map] : server->MapMngr.GetMaps()) {
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

    if (!pid) {
        locations.reserve(server->MapMngr.GetLocationsCount());
    }

    for (auto&& [id, loc] : server->MapMngr.GetLocations()) {
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
[[maybe_unused]] void Server_Game_GetTime(FOServer* server, uint16& year, uint16& month, uint16& day, uint16& dayOfWeek, uint16& hour, uint16& minute, uint16& second, uint16& milliseconds)
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
[[maybe_unused]] void Server_Game_SetTime(FOServer* server, uint16 multiplier, uint16 year, uint16 month, uint16 day, uint16 hour, uint16 minute, uint16 second)
{
    server->SetGameTime(multiplier, year, month, day, hour, minute, second);
}

///# ...
///# param cr ...
///# param staticItem ...
///# param usedItem ...
///# param param ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Server_Game_CallStaticItemFunction(FOServer* server, Critter* cr, StaticItem* staticItem, Item* usedItem, int param)
{
    if (!staticItem->SceneryScriptFunc) {
        return false;
    }

    return staticItem->SceneryScriptFunc(cr, staticItem, usedItem, param) && staticItem->SceneryScriptFunc.GetResult();
}
