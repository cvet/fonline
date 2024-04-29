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
    if (name.empty()) {
        throw ScriptException("Empty player name");
    }

    const auto player_id = server->MakePlayerId(name);
    if (!server->DbStorage.Get(server->PlayersCollectionName, player_id).empty()) {
        throw ScriptException("Player already registered", name);
    }

    server->DbStorage.Insert(server->PlayersCollectionName, player_id, {{"_Name", string(name)}, {"Password", string(password)}});

    return player_id;
}

///@ ExportMethod
[[maybe_unused]] Critter* Server_Game_CreateCritter(FOServer* server, hstring protoId, bool forPlayer)
{
    return server->CreateCritter(protoId, forPlayer);
}

///@ ExportMethod
[[maybe_unused]] Critter* Server_Game_LoadCritter(FOServer* server, ident_t crId, bool forPlayer)
{
    return server->LoadCritter(crId, forPlayer);
}

///@ ExportMethod
[[maybe_unused]] void Server_Game_UnloadCritter(FOServer* server, Critter* cr)
{
    server->UnloadCritter(cr);
}

///@ ExportMethod
[[maybe_unused]] void Server_Game_DestroyUnloadedCritter(FOServer* server, ident_t crId)
{
    server->DestroyUnloadedCritter(crId);
}

///@ ExportMethod
[[maybe_unused]] ident_t Server_Game_DeferredCall(FOServer* server, uint delay, ScriptFunc<void> func)
{
    return server->ServerDeferredCalls.AddDeferredCall(delay, false, func);
}

///@ ExportMethod
[[maybe_unused]] ident_t Server_Game_DeferredCall(FOServer* server, uint delay, ScriptFunc<void, any_t> func, any_t value)
{
    return server->ServerDeferredCalls.AddDeferredCall(delay, false, func, value);
}

///@ ExportMethod
[[maybe_unused]] ident_t Server_Game_DeferredCall(FOServer* server, uint delay, ScriptFunc<void, vector<any_t>> func, const vector<any_t>& values)
{
    return server->ServerDeferredCalls.AddDeferredCall(delay, false, func, values);
}

///@ ExportMethod
[[maybe_unused]] ident_t Server_Game_RepeatingDeferredCall(FOServer* client, uint delay, ScriptFunc<void> func)
{
    return client->ServerDeferredCalls.AddDeferredCall(delay, true, func);
}

///@ ExportMethod
[[maybe_unused]] ident_t Server_Game_RepeatingDeferredCall(FOServer* client, uint delay, ScriptFunc<void, any_t> func, any_t value)
{
    return client->ServerDeferredCalls.AddDeferredCall(delay, true, func, value);
}

///@ ExportMethod
[[maybe_unused]] ident_t Server_Game_RepeatingDeferredCall(FOServer* client, uint delay, ScriptFunc<void, vector<any_t>> func, const vector<any_t>& values)
{
    return client->ServerDeferredCalls.AddDeferredCall(delay, true, func, values);
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
[[maybe_unused]] ident_t Server_Game_SavedDeferredCall(FOServer* server, uint delay, ScriptFunc<void, any_t> func, any_t value)
{
    if (func.IsDelegate()) {
        throw ScriptException("Function must be global (not delegate)");
    }

    return server->ServerDeferredCalls.AddSavedDeferredCall(delay, func, value);
}

///@ ExportMethod
[[maybe_unused]] ident_t Server_Game_SavedDeferredCall(FOServer* server, uint delay, ScriptFunc<void, vector<any_t>> func, const vector<any_t>& values)
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
///# param cr1 ...
///# param cr2 ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Server_Game_GetDistance(FOServer* server, Critter* cr1, Critter* cr2)
{
    UNUSED_VARIABLE(server);

    if (cr1 == nullptr) {
        throw ScriptException("Critter1 arg is null");
    }
    if (cr2 == nullptr) {
        throw ScriptException("Critter2 arg is null");
    }
    if (cr1->GetMapId() != cr2->GetMapId()) {
        throw ScriptException("Differernt maps");
    }

    const auto dist = GeometryHelper::DistGame(cr1->GetHex(), cr2->GetHex());

    return static_cast<int>(dist);
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] uint Server_Game_GetTick(FOServer* server)
{
    return time_duration_to_ms<uint>(server->GameTime.FrameTime().time_since_epoch());
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
///# param toCr ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_MoveItem(FOServer* server, Item* item, Critter* toCr)
{
    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }
    if (toCr == nullptr) {
        throw ScriptException("Critter arg is null");
    }

    server->ItemMngr.MoveItem(item, item->GetCount(), toCr, false);
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

    if (count == 0) {
        return;
    }
    if (count > item->GetCount()) {
        throw ScriptException("Count arg is greater than maximum");
    }

    server->ItemMngr.MoveItem(item, count, toCr, false);
}

///# ...
///# param item ...
///# param toCr ...
///# param skipChecks ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_MoveItem(FOServer* server, Item* item, Critter* toCr, bool skipChecks)
{
    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }
    if (toCr == nullptr) {
        throw ScriptException("Critter arg is null");
    }

    server->ItemMngr.MoveItem(item, item->GetCount(), toCr, skipChecks);
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

    if (count == 0) {
        return;
    }
    if (count > item->GetCount()) {
        throw ScriptException("Count arg is greater than maximum");
    }

    server->ItemMngr.MoveItem(item, count, toCr, skipChecks);
}

///# ...
///# param item ...
///# param toMap ...
///# param toHex ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_MoveItem(FOServer* server, Item* item, Map* toMap, mpos toHex)
{
    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }
    if (toMap == nullptr) {
        throw ScriptException("Map arg is null");
    }
    if (!toMap->GetSize().IsValidPos(toHex)) {
        throw ScriptException("Invalid hexex args");
    }

    server->ItemMngr.MoveItem(item, item->GetCount(), toMap, toHex, false);
}

///# ...
///# param item ...
///# param count ...
///# param toMap ...
///# param toHex ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_MoveItem(FOServer* server, Item* item, uint count, Map* toMap, mpos toHex)
{
    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }
    if (toMap == nullptr) {
        throw ScriptException("Map arg is null");
    }
    if (!toMap->GetSize().IsValidPos(toHex)) {
        throw ScriptException("Invalid hexex args");
    }

    if (count == 0) {
        return;
    }
    if (count > item->GetCount()) {
        throw ScriptException("Count arg is greater than maximum");
    }

    server->ItemMngr.MoveItem(item, count, toMap, toHex, false);
}

///# ...
///# param item ...
///# param toMap ...
///# param toHex ...
///# param skipChecks ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_MoveItem(FOServer* server, Item* item, Map* toMap, mpos toHex, bool skipChecks)
{
    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }
    if (toMap == nullptr) {
        throw ScriptException("Map arg is null");
    }
    if (!toMap->GetSize().IsValidPos(toHex)) {
        throw ScriptException("Invalid hexex args");
    }

    server->ItemMngr.MoveItem(item, item->GetCount(), toMap, toHex, skipChecks);
}

///# ...
///# param item ...
///# param count ...
///# param toMap ...
///# param toHex ...
///# param skipChecks ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_MoveItem(FOServer* server, Item* item, uint count, Map* toMap, mpos toHex, bool skipChecks)
{
    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }
    if (toMap == nullptr) {
        throw ScriptException("Map arg is null");
    }
    if (!toMap->GetSize().IsValidPos(toHex)) {
        throw ScriptException("Invalid hexex args");
    }

    if (count == 0) {
        return;
    }
    if (count > item->GetCount()) {
        throw ScriptException("Count arg is greater than maximum");
    }

    server->ItemMngr.MoveItem(item, count, toMap, toHex, skipChecks);
}

///# ...
///# param item ...
///# param toCont ...
///# param stackId ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_MoveItem(FOServer* server, Item* item, Item* toCont, ContainerItemStack stackId)
{
    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }
    if (toCont == nullptr) {
        throw ScriptException("Container arg is null");
    }

    server->ItemMngr.MoveItem(item, item->GetCount(), toCont, stackId, false);
}

///# ...
///# param item ...
///# param count ...
///# param toCont ...
///# param stackId ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_MoveItem(FOServer* server, Item* item, uint count, Item* toCont, ContainerItemStack stackId)
{
    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }
    if (toCont == nullptr) {
        throw ScriptException("Container arg is null");
    }

    if (count == 0) {
        return;
    }
    if (count > item->GetCount()) {
        throw ScriptException("Count arg is greater than maximum");
    }

    server->ItemMngr.MoveItem(item, count, toCont, stackId, false);
}

///# ...
///# param item ...
///# param count ...
///# param toCont ...
///# param stackId ...
///# param skipChecks ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_MoveItem(FOServer* server, Item* item, Item* toCont, ContainerItemStack stackId, bool skipChecks)
{
    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }
    if (toCont == nullptr) {
        throw ScriptException("Container arg is null");
    }

    server->ItemMngr.MoveItem(item, item->GetCount(), toCont, stackId, skipChecks);
}

///# ...
///# param item ...
///# param count ...
///# param toCont ...
///# param stackId ...
///# param skipChecks ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_MoveItem(FOServer* server, Item* item, uint count, Item* toCont, ContainerItemStack stackId, bool skipChecks)
{
    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }
    if (toCont == nullptr) {
        throw ScriptException("Container arg is null");
    }

    if (count == 0) {
        return;
    }
    if (count > item->GetCount()) {
        throw ScriptException("Count arg is greater than maximum");
    }

    server->ItemMngr.MoveItem(item, count, toCont, stackId, skipChecks);
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
///# param toHex ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_MoveItems(FOServer* server, const vector<Item*>& items, Map* toMap, mpos toHex)
{
    if (toMap == nullptr) {
        throw ScriptException("Map arg is null");
    }
    if (!toMap->GetSize().IsValidPos(toHex)) {
        throw ScriptException("Invalid hexex args");
    }

    for (auto* item : items) {
        if (item == nullptr || item->IsDestroyed()) {
            continue;
        }

        server->ItemMngr.MoveItem(item, item->GetCount(), toMap, toHex, false);
    }
}

///# ...
///# param items ...
///# param toMap ...
///# param toHex ...
///# param skipChecks ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_MoveItems(FOServer* server, const vector<Item*>& items, Map* toMap, mpos toHex, bool skipChecks)
{
    if (toMap == nullptr) {
        throw ScriptException("Map arg is null");
    }
    if (!toMap->GetSize().IsValidPos(toHex)) {
        throw ScriptException("Invalid hexex args");
    }

    for (auto* item : items) {
        if (item == nullptr || item->IsDestroyed()) {
            continue;
        }

        server->ItemMngr.MoveItem(item, item->GetCount(), toMap, toHex, skipChecks);
    }
}

///# ...
///# param items ...
///# param toCont ...
///# param stackId ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_MoveItems(FOServer* server, const vector<Item*>& items, Item* toCont, ContainerItemStack stackId)
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
[[maybe_unused]] void Server_Game_MoveItems(FOServer* server, const vector<Item*>& items, Item* toCont, ContainerItemStack stackId, bool skipChecks)
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
    if (item != nullptr && count > 0) {
        const auto cur_count = item->GetCount();

        if (count >= cur_count) {
            server->ItemMngr.DeleteItem(item);
        }
        else {
            item->SetCount(cur_count - count);
            server->OnItemStackChanged.Fire(item, -static_cast<int>(count));
        }
    }
}

///# ...
///# param itemId ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_DeleteItem(FOServer* server, ident_t itemId)
{
    auto* item = server->ItemMngr.GetItem(itemId);

    if (item != nullptr) {
        server->ItemMngr.DeleteItem(item);
    }
}

///# ...
///# param itemId ...
///# param count ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_DeleteItem(FOServer* server, ident_t itemId, uint count)
{
    auto* item = server->ItemMngr.GetItem(itemId);

    if (item != nullptr && count > 0) {
        const auto cur_count = item->GetCount();

        if (count >= cur_count) {
            server->ItemMngr.DeleteItem(item);
        }
        else {
            item->SetCount(cur_count - count);
            server->OnItemStackChanged.Fire(item, -static_cast<int>(count));
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
    if (cr != nullptr && !cr->GetIsControlledByPlayer()) {
        server->CrMngr.DeleteCritter(cr);
    }
}

///# ...
///# param crId ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_DeleteCritter(FOServer* server, ident_t crId)
{
    if (crId) {
        if (Critter* cr = server->CrMngr.GetCritter(crId); cr != nullptr && !cr->GetIsControlledByPlayer()) {
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
        if (cr != nullptr && !cr->GetIsControlledByPlayer()) {
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
            if (Critter* cr = server->CrMngr.GetCritter(id); cr != nullptr && !cr->GetIsControlledByPlayer()) {
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
        server->ItemMngr.RadioSendTextEx(channel, RADIO_BROADCAST_FORCE_ALL, ident_t {}, {}, text, false, TextPackName::None, 0, "");
    }
}

///# ...
///# param channel ...
///# param textMsg ...
///# param numStr ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_RadioMessageMsg(FOServer* server, uint16 channel, TextPackName textPack, uint numStr)
{
    server->ItemMngr.RadioSendTextEx(channel, RADIO_BROADCAST_FORCE_ALL, ident_t {}, {}, "", false, textPack, numStr, "");
}

///# ...
///# param channel ...
///# param textMsg ...
///# param numStr ...
///# param lexems ...
///@ ExportMethod
[[maybe_unused]] void Server_Game_RadioMessageMsg(FOServer* server, uint16 channel, TextPackName textPack, uint numStr, string_view lexems)
{
    server->ItemMngr.RadioSendTextEx(channel, RADIO_BROADCAST_FORCE_ALL, ident_t {}, {}, "", false, textPack, numStr, lexems);
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] tick_t Server_Game_GetFullSecond(FOServer* server)
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
[[maybe_unused]] tick_t Server_Game_EvaluateFullSecond(FOServer* server, uint16 year, uint16 month, uint16 day, uint16 hour, uint16 minute, uint16 second)
{
    if (year != 0 && year < server->Settings.StartYear) {
        throw ScriptException("Invalid year", year);
    }
    if (year != 0 && year > server->Settings.StartYear + 100) {
        throw ScriptException("Invalid year", year);
    }
    if (month != 0 && month < 1) {
        throw ScriptException("Invalid month", month);
    }
    if (month != 0 && month > 12) {
        throw ScriptException("Invalid month", month);
    }

    auto year_ = year;
    auto month_ = month;
    auto day_ = day;

    if (year_ == 0) {
        year_ = server->GetYear();
    }
    if (month_ == 0) {
        month_ = server->GetMonth();
    }

    if (day_ != 0) {
        const auto month_day = server->GameTime.GameTimeMonthDays(year, month_);
        if (day_ > month_day) {
            throw ScriptException("Invalid day", day_, month_day);
        }
    }

    if (day_ == 0) {
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
[[maybe_unused]] void Server_Game_EvaluateGameTime(FOServer* server, tick_t fullSecond, uint16& year, uint16& month, uint16& day, uint16& dayOfWeek, uint16& hour, uint16& minute, uint16& second)
{
    const auto dt = server->GameTime.EvaluateGameTime(fullSecond);

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
///# param wpos ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Location* Server_Game_CreateLocation(FOServer* server, hstring locPid, upos16 wpos)
{
    // Create and generate location
    auto* loc = server->MapMngr.CreateLocation(locPid, wpos);
    if (loc == nullptr) {
        throw ScriptException("Unable to create location {}", locPid);
    }

    return loc;
}

///# ...
///# param locPid ...
///# param wpos ...
///# param critters ...
///# return ...
///@ ExportMethod
[[maybe_unused]] Location* Server_Game_CreateLocation(FOServer* server, hstring locPid, upos16 wpos, const vector<Critter*>& critters)
{
    // Create and generate location
    auto* loc = server->MapMngr.CreateLocation(locPid, wpos);
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

        const auto zx = static_cast<uint16>(loc->GetWorldPos().x / server->Settings.GlobalMapZoneLength);
        const auto zy = static_cast<uint16>(loc->GetWorldPos().y / server->Settings.GlobalMapZoneLength);

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
    if (name.empty()) {
        throw ScriptException("Empty player name");
    }

    // Check existence
    const auto id = server->MakePlayerId(name);
    const auto doc = server->DbStorage.Get(server->PlayersCollectionName, id);
    if (doc.empty()) {
        return nullptr;
    }

    // Find online
    auto* player = server->EntityMngr.GetPlayer(id);
    if (player != nullptr) {
        player->AddRef();
        return player;
    }

    // Load from db
    auto* dummy_net_conn = new DummyNetConnection(server->Settings);

    player = new Player(server, id, new ClientConnection(dummy_net_conn));
    player->SetName(name);

    dummy_net_conn->Release();

    if (!PropertiesSerializator::LoadFromDocument(&player->GetPropertiesForEdit(), doc, *server, *server)) {
        player->MarkAsDestroyed();
        player->Release();
        throw ScriptException("Player data db read failed");
    }

    return player;
}

///# ...
///# param wpos ...
///# param radius ...
///# param findType ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Critter*> Server_Game_GetGlobalMapCritters(FOServer* server, upos16 wpos, uint radius, CritterFindType findType)
{
    return server->CrMngr.GetGlobalMapCritters(wpos, radius, findType);
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
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Map*> Server_Game_GetMaps(FOServer* server)
{
    vector<Map*> maps;
    maps.reserve(server->MapMngr.GetLocationsCount());

    for (auto&& [id, map] : server->MapMngr.GetMaps()) {
        maps.push_back(map);
    }

    return maps;
}

///# ...
///# param pid ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Map*> Server_Game_GetMaps(FOServer* server, hstring pid)
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
[[maybe_unused]] Location* Server_Game_GetLocation(FOServer* server, hstring locPid)
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
[[maybe_unused]] Location* Server_Game_GetLocation(FOServer* server, hstring locPid, uint skipCount)
{
    if (!locPid) {
        throw ScriptException("Invalid zero location proto id arg");
    }

    return server->MapMngr.GetLocationByPid(locPid, skipCount);
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Location*> Server_Game_GetLocations(FOServer* server)
{
    vector<Location*> locations;
    locations.reserve(server->MapMngr.GetLocationsCount());

    for (auto&& [id, loc] : server->MapMngr.GetLocations()) {
        locations.push_back(loc);
    }

    return locations;
}

///# ...
///# param pid ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Location*> Server_Game_GetLocations(FOServer* server, hstring pid)
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
///# param wpos ...
///# param radius ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Location*> Server_Game_GetLocations(FOServer* server, upos16 wpos, uint radius)
{
    vector<Location*> locations;
    locations.reserve(server->MapMngr.GetLocationsCount());

    for (auto&& [id, loc] : server->MapMngr.GetLocations()) {
        if (GenericUtils::DistSqrt({wpos.x, wpos.y}, {loc->GetWorldPos().x, loc->GetWorldPos().y}) <= radius + loc->GetRadius()) {
            locations.push_back(loc);
        }
    }

    return locations;
}

///# ...
///# param wpos ...
///# param radius ...
///# param cr ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<Location*> Server_Game_GetVisibleLocations(FOServer* server, upos16 wpos, uint radius, Critter* cr)
{
    vector<Location*> locations;
    locations.reserve(server->MapMngr.GetLocationsCount());

    for (auto&& [id, loc] : server->MapMngr.GetLocations()) {
        if (GenericUtils::DistSqrt({wpos.x, wpos.y}, {loc->GetWorldPos().x, loc->GetWorldPos().y}) <= radius + loc->GetRadius() && //
            (loc->IsLocVisible() || (cr != nullptr && cr->GetIsControlledByPlayer() && server->MapMngr.CheckKnownLoc(cr, loc->GetId())))) {
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
    return server->MapMngr.GetZoneLocations(zx, zy, static_cast<int>(zoneRadius));
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
    if (!cr->GetIsControlledByPlayer()) {
        throw ScriptException("Player arg is not player");
    }
    if (npc == nullptr) {
        throw ScriptException("Npc arg is null");
    }
    if (npc->GetIsControlledByPlayer()) {
        throw ScriptException("Npc arg is not npc");
    }

    if (cr->Talk.Locked) {
        throw ScriptException("Can't open new dialog from demand, result or dialog functions");
    }

    server->BeginDialog(cr, npc, {}, {}, ignoreDistance);

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
    if (!cr->GetIsControlledByPlayer()) {
        throw ScriptException("Player arg is not player");
    }
    if (npc == nullptr) {
        throw ScriptException("Npc arg is null");
    }
    if (npc->GetIsControlledByPlayer()) {
        throw ScriptException("Npc arg is not npc");
    }

    if (cr->Talk.Locked) {
        throw ScriptException("Can't open new dialog from demand, result or dialog functions");
    }

    server->BeginDialog(cr, npc, dlgPack, {}, ignoreDistance);

    return cr->Talk.Type == TalkType::Critter && cr->Talk.CritterId == npc->GetId();
}

///# ...
///# param cr ...
///# param dlgPack ...
///# param hex ...
///# param ignoreDistance ...
///# return ...
///@ ExportMethod
[[maybe_unused]] bool Server_Game_RunDialog(FOServer* server, Critter* cr, hstring dlgPack, mpos hex, bool ignoreDistance)
{
    if (cr == nullptr) {
        throw ScriptException("Player arg is null");
    }
    if (!cr->GetIsControlledByPlayer()) {
        throw ScriptException("Player arg is not player");
    }
    if (server->DlgMngr.GetDialog(dlgPack) == nullptr) {
        throw ScriptException("Dialog not found");
    }

    if (cr->Talk.Locked) {
        throw ScriptException("Can't open new dialog from demand, result or dialog functions");
    }

    server->BeginDialog(cr, nullptr, dlgPack, hex, ignoreDistance);

    return cr->Talk.Type == TalkType::Hex && cr->Talk.TalkHex == hex;
}

///# ...
///# param pid ...
///# return ...
///@ ExportMethod
[[maybe_unused]] int64 Server_Game_GetWorldItemCount(FOServer* server, hstring pid)
{
    if (server->ProtoMngr.GetProtoItem(pid) == nullptr) {
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
    UNUSED_VARIABLE(server);
    UNUSED_VARIABLE(sayType);
    UNUSED_VARIABLE(firstStr);
    UNUSED_VARIABLE(parameter);
    UNUSED_VARIABLE(func);

    throw NotImplementedException(LINE_STR);

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
    UNUSED_VARIABLE(server);
    UNUSED_VARIABLE(sayType);
    UNUSED_VARIABLE(firstStr);
    UNUSED_VARIABLE(parameter);

    throw NotImplementedException(LINE_STR);

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
    return server->DbStorage.GetAllIds(server->PlayersCollectionName);
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
    UNUSED_VARIABLE(server);

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
    UNUSED_VARIABLE(server);

    if (!staticItem->SceneryScriptFunc) {
        return false;
    }

    return staticItem->SceneryScriptFunc(cr, staticItem, usedItem, param) && staticItem->SceneryScriptFunc.GetResult();
}

///# ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<hstring> Server_Game_GetDialogs(FOServer* server)
{
    const auto& dlg_packs = server->DlgMngr.GetDialogs();

    vector<hstring> result;
    result.reserve(dlg_packs.size());

    for (const auto* dlg_pack : dlg_packs) {
        result.emplace_back(dlg_pack->PackId);
    }

    return result;
}

///# ...
///# param proto ...
///# return ...
///@ ExportMethod
[[maybe_unused]] vector<StaticItem*> Server_Game_GetStaticItemsForProtoMap(FOServer* server, ProtoMap* proto)
{
    const auto* static_map = server->MapMngr.GetStaticMap(proto);

    return static_map->StaticItems;
}
