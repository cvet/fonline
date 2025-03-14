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

#include "Common.h"

#include "FileSystem.h"
#include "GenericUtils.h"
#include "Networking.h"
#include "PropertiesSerializator.h"
#include "ScriptSystem.h"
#include "Server.h"
#include "StringUtils.h"

///@ ExportMethod
FO_SCRIPT_API void Server_Game_StartPersistentTimeEvent(FOServer* server, tick_t delay, ScriptFuncName<void> func)
{
    server->TimeEventMngr.StartTimeEvent(server, true, func, delay, {}, {});
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_StartPersistentTimeEvent(FOServer* server, tick_t delay, ScriptFuncName<void, any_t> func, any_t data)
{
    server->TimeEventMngr.StartTimeEvent(server, true, func, delay, {}, vector<any_t> {std::move(data)});
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_StartPersistentTimeEvent(FOServer* server, tick_t delay, tick_t repeat, ScriptFuncName<void> func)
{
    server->TimeEventMngr.StartTimeEvent(server, true, func, delay, repeat, {});
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_StartPersistentTimeEvent(FOServer* server, tick_t delay, tick_t repeat, ScriptFuncName<void, any_t> func, any_t data)
{
    server->TimeEventMngr.StartTimeEvent(server, true, func, delay, repeat, vector<any_t> {std::move(data)});
}

///@ ExportMethod
FO_SCRIPT_API ident_t Server_Game_CreatePlayer(FOServer* server, string_view name, string_view password)
{
    if (name.empty()) {
        throw ScriptException("Empty player name");
    }

    const auto player_id = server->MakePlayerId(name);
    if (!server->DbStorage.Get(server->PlayersCollectionName, player_id).Empty()) {
        throw ScriptException("Player already registered", name);
    }

    AnyData::Document player_data;
    player_data.Emplace("_Name", string(name));
    player_data.Emplace("Password", string(password));

    server->DbStorage.Insert(server->PlayersCollectionName, player_id, player_data);

    return player_id;
}

///@ ExportMethod
FO_SCRIPT_API Critter* Server_Game_CreateCritter(FOServer* server, hstring protoId, bool forPlayer)
{
    return server->CreateCritter(protoId, forPlayer);
}

///@ ExportMethod
FO_SCRIPT_API Critter* Server_Game_LoadCritter(FOServer* server, ident_t crId, bool forPlayer)
{
    return server->LoadCritter(crId, forPlayer);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_UnloadCritter(FOServer* server, Critter* cr)
{
    server->UnloadCritter(cr);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyUnloadedCritter(FOServer* server, ident_t crId)
{
    server->DestroyUnloadedCritter(crId);
}

///@ ExportMethod
FO_SCRIPT_API uint Server_Game_GetDistance(FOServer* server, Critter* cr1, Critter* cr2)
{
    UNUSED_VARIABLE(server);

    if (cr1 == nullptr) {
        throw ScriptException("Critter 1 arg is null");
    }
    if (cr2 == nullptr) {
        throw ScriptException("Critter 2 arg is null");
    }
    if (cr1->GetMapId() != cr2->GetMapId()) {
        throw ScriptException("Critters different maps");
    }
    if (!cr1->GetMapId()) {
        throw ScriptException("Critters not on map");
    }

    const auto dist = GeometryHelper::DistGame(cr1->GetHex(), cr2->GetHex());
    const auto multihex = cr1->GetMultihex() + cr2->GetMultihex();
    return multihex < dist ? dist - multihex : 0;
}

///@ ExportMethod
FO_SCRIPT_API uint Server_Game_GetDistance(FOServer* server, Item* item1, Item* item2)
{
    UNUSED_VARIABLE(server);

    if (item1 == nullptr) {
        throw ScriptException("Item 1 arg is null");
    }
    if (item2 == nullptr) {
        throw ScriptException("Item 2 arg is null");
    }
    if (item1->GetMapId() != item2->GetMapId()) {
        throw ScriptException("Items different maps");
    }
    if (!item1->GetMapId()) {
        throw ScriptException("Items not on map");
    }

    const auto dist = GeometryHelper::DistGame(item1->GetHex(), item2->GetHex());
    return dist;
}

///@ ExportMethod
FO_SCRIPT_API uint Server_Game_GetDistance(FOServer* server, Critter* cr, Item* item)
{
    UNUSED_VARIABLE(server);

    if (cr == nullptr) {
        throw ScriptException("Critter arg is null");
    }
    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }
    if (cr->GetMapId() != item->GetMapId()) {
        throw ScriptException("Critter/Item different maps");
    }
    if (!cr->GetMapId()) {
        throw ScriptException("Critter/Item not on map");
    }

    const auto dist = GeometryHelper::DistGame(cr->GetHex(), item->GetHex());
    const auto multihex = cr->GetMultihex();
    return multihex < dist ? dist - multihex : 0;
}

///@ ExportMethod
FO_SCRIPT_API uint Server_Game_GetDistance(FOServer* server, Item* item, Critter* cr)
{
    UNUSED_VARIABLE(server);

    if (cr == nullptr) {
        throw ScriptException("Critter arg is null");
    }
    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }
    if (cr->GetMapId() != item->GetMapId()) {
        throw ScriptException("Item/Critter different maps");
    }
    if (!cr->GetMapId()) {
        throw ScriptException("Item/Critter not on map");
    }

    const auto dist = GeometryHelper::DistGame(cr->GetHex(), item->GetHex());
    const auto multihex = cr->GetMultihex();
    return multihex < dist ? dist - multihex : 0;
}

///@ ExportMethod
FO_SCRIPT_API uint Server_Game_GetDistance(FOServer* server, Critter* cr, mpos hex)
{
    UNUSED_VARIABLE(server);

    if (cr == nullptr) {
        throw ScriptException("Critter arg is null");
    }
    if (!cr->GetMapId()) {
        throw ScriptException("Critter not on map");
    }

    const auto dist = GeometryHelper::DistGame(cr->GetHex(), hex);
    const auto multihex = cr->GetMultihex();
    return multihex < dist ? dist - multihex : 0;
}

///@ ExportMethod
FO_SCRIPT_API uint Server_Game_GetDistance(FOServer* server, mpos hex, Critter* cr)
{
    UNUSED_VARIABLE(server);

    if (cr == nullptr) {
        throw ScriptException("Critter arg is null");
    }
    if (!cr->GetMapId()) {
        throw ScriptException("Critter not on map");
    }

    const auto dist = GeometryHelper::DistGame(cr->GetHex(), hex);
    const auto multihex = cr->GetMultihex();
    return multihex < dist ? dist - multihex : 0;
}

///@ ExportMethod
FO_SCRIPT_API uint Server_Game_GetDistance(FOServer* server, Item* item, mpos hex)
{
    UNUSED_VARIABLE(server);

    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }
    if (!item->GetMapId()) {
        throw ScriptException("Item not on map");
    }

    const auto dist = GeometryHelper::DistGame(item->GetHex(), hex);
    return dist;
}

///@ ExportMethod
FO_SCRIPT_API uint Server_Game_GetDistance(FOServer* server, mpos hex, Item* item)
{
    UNUSED_VARIABLE(server);

    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }
    if (!item->GetMapId()) {
        throw ScriptException("Item not on map");
    }

    const auto dist = GeometryHelper::DistGame(item->GetHex(), hex);
    return dist;
}

///@ ExportMethod
FO_SCRIPT_API Item* Server_Game_GetItem(FOServer* server, ident_t itemId)
{
    if (!itemId) {
        throw ScriptException("Item id arg is zero");
    }

    auto* item = server->EntityMngr.GetItem(itemId);
    if (item == nullptr || item->IsDestroyed()) {
        return nullptr;
    }

    return item;
}

///@ ExportMethod
FO_SCRIPT_API Item* Server_Game_MoveItem(FOServer* server, Item* item, Critter* toCr)
{
    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }
    if (toCr == nullptr) {
        throw ScriptException("Critter arg is null");
    }

    return server->ItemMngr.MoveItem(item, item->GetCount(), toCr);
}

///@ ExportMethod
FO_SCRIPT_API Item* Server_Game_MoveItem(FOServer* server, Item* item, uint count, Critter* toCr)
{
    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }
    if (toCr == nullptr) {
        throw ScriptException("Critter arg is null");
    }

    if (count == 0) {
        return nullptr;
    }

    return server->ItemMngr.MoveItem(item, count, toCr);
}

///@ ExportMethod
FO_SCRIPT_API Item* Server_Game_MoveItem(FOServer* server, Item* item, Map* toMap, mpos toHex)
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

    return server->ItemMngr.MoveItem(item, item->GetCount(), toMap, toHex);
}

///@ ExportMethod
FO_SCRIPT_API Item* Server_Game_MoveItem(FOServer* server, Item* item, uint count, Map* toMap, mpos toHex)
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
        return nullptr;
    }

    return server->ItemMngr.MoveItem(item, count, toMap, toHex);
}

///@ ExportMethod
FO_SCRIPT_API Item* Server_Game_MoveItem(FOServer* server, Item* item, Item* toCont, ContainerItemStack stackId)
{
    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }
    if (toCont == nullptr) {
        throw ScriptException("Container arg is null");
    }

    return server->ItemMngr.MoveItem(item, item->GetCount(), toCont, stackId);
}

///@ ExportMethod
FO_SCRIPT_API Item* Server_Game_MoveItem(FOServer* server, Item* item, uint count, Item* toCont, ContainerItemStack stackId)
{
    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }
    if (toCont == nullptr) {
        throw ScriptException("Container arg is null");
    }

    if (count == 0) {
        return nullptr;
    }

    return server->ItemMngr.MoveItem(item, count, toCont, stackId);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_MoveItems(FOServer* server, const vector<Item*>& items, Critter* toCr)
{
    if (toCr == nullptr) {
        throw ScriptException("Critter arg is null");
    }

    for (auto* item : items) {
        if (item == nullptr || item->IsDestroyed()) {
            continue;
        }

        server->ItemMngr.MoveItem(item, item->GetCount(), toCr);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_MoveItems(FOServer* server, const vector<Item*>& items, Map* toMap, mpos toHex)
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

        server->ItemMngr.MoveItem(item, item->GetCount(), toMap, toHex);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_MoveItems(FOServer* server, const vector<Item*>& items, Item* toCont, ContainerItemStack stackId)
{
    if (toCont == nullptr) {
        throw ScriptException("Container arg is null");
    }

    for (auto* item : items) {
        if (item == nullptr || item->IsDestroyed()) {
            continue;
        }

        server->ItemMngr.MoveItem(item, item->GetCount(), toCont, stackId);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyEntity(FOServer* server, ident_t id)
{
    if (auto* entity = server->EntityMngr.GetEntity(id); entity != nullptr) {
        server->EntityMngr.DestroyEntity(entity);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyEntity(FOServer* server, ServerEntity* entity)
{
    if (entity != nullptr) {
        server->EntityMngr.DestroyEntity(entity);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyEntities(FOServer* server, const vector<ident_t>& ids)
{
    for (const auto id : ids) {
        if (auto* entity = server->EntityMngr.GetEntity(id); entity != nullptr) {
            server->EntityMngr.DestroyEntity(entity);
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyEntities(FOServer* server, const vector<ServerEntity*>& entities)
{
    for (auto* entity : entities) {
        if (entity != nullptr) {
            server->EntityMngr.DestroyEntity(entity);
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyItem(FOServer* server, Item* item)
{
    if (item != nullptr) {
        server->ItemMngr.DestroyItem(item);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyItem(FOServer* server, Item* item, uint count)
{
    if (item != nullptr && count > 0) {
        const auto cur_count = item->GetCount();

        if (count >= cur_count) {
            server->ItemMngr.DestroyItem(item);
        }
        else {
            item->SetCount(cur_count - count);
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyItem(FOServer* server, ident_t itemId)
{
    auto* item = server->EntityMngr.GetItem(itemId);

    if (item != nullptr) {
        server->ItemMngr.DestroyItem(item);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyItem(FOServer* server, ident_t itemId, uint count)
{
    auto* item = server->EntityMngr.GetItem(itemId);

    if (item != nullptr && count > 0) {
        const auto cur_count = item->GetCount();

        if (count >= cur_count) {
            server->ItemMngr.DestroyItem(item);
        }
        else {
            item->SetCount(cur_count - count);
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyItems(FOServer* server, const vector<Item*>& items)
{
    for (auto* item : items) {
        if (item != nullptr) {
            server->ItemMngr.DestroyItem(item);
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyItems(FOServer* server, const vector<ident_t>& itemIds)
{
    for (const auto item_id : itemIds) {
        if (item_id) {
            auto* item = server->EntityMngr.GetItem(item_id);

            if (item != nullptr) {
                server->ItemMngr.DestroyItem(item);
            }
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyCritter(FOServer* server, Critter* cr)
{
    if (cr != nullptr && !cr->GetControlledByPlayer()) {
        server->CrMngr.DestroyCritter(cr);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyCritter(FOServer* server, ident_t crId)
{
    if (crId) {
        if (Critter* cr = server->EntityMngr.GetCritter(crId); cr != nullptr && !cr->GetControlledByPlayer()) {
            server->CrMngr.DestroyCritter(cr);
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyCritters(FOServer* server, const vector<Critter*>& critters)
{
    for (auto* cr : critters) {
        if (cr != nullptr && !cr->GetControlledByPlayer()) {
            server->CrMngr.DestroyCritter(cr);
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyCritters(FOServer* server, const vector<ident_t>& critterIds)
{
    for (const auto id : critterIds) {
        if (id) {
            if (Critter* cr = server->EntityMngr.GetCritter(id); cr != nullptr && !cr->GetControlledByPlayer()) {
                server->CrMngr.DestroyCritter(cr);
            }
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_RadioMessage(FOServer* server, uint16 channel, string_view text)
{
    if (!text.empty()) {
        server->ItemMngr.RadioSendTextEx(channel, RADIO_BROADCAST_FORCE_ALL, ident_t {}, text, false, TextPackName::None, 0, "");
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_RadioMessageMsg(FOServer* server, uint16 channel, TextPackName textPack, uint numStr)
{
    server->ItemMngr.RadioSendTextEx(channel, RADIO_BROADCAST_FORCE_ALL, ident_t {}, "", false, textPack, numStr, "");
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_RadioMessageMsg(FOServer* server, uint16 channel, TextPackName textPack, uint numStr, string_view lexems)
{
    server->ItemMngr.RadioSendTextEx(channel, RADIO_BROADCAST_FORCE_ALL, ident_t {}, "", false, textPack, numStr, lexems);
}

///@ ExportMethod
FO_SCRIPT_API Location* Server_Game_CreateLocation(FOServer* server, hstring protoId)
{
    auto* loc = server->MapMngr.CreateLocation(protoId, nullptr);
    RUNTIME_ASSERT(loc);

    return loc;
}

///@ ExportMethod
FO_SCRIPT_API Location* Server_Game_CreateLocation(FOServer* server, hstring protoId, const map<LocationProperty, any_t>& props)
{
    const auto* proto = server->ProtoMngr.GetProtoLocation(protoId);
    auto props_ = proto->GetProperties().Copy();

    for (const auto& [key, value] : props) {
        props_.SetValueAsAnyProps(static_cast<int>(key), value);
    }

    auto* loc = server->MapMngr.CreateLocation(protoId, &props_);
    RUNTIME_ASSERT(loc);

    return loc;
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyLocation(FOServer* server, Location* loc)
{
    if (loc != nullptr) {
        server->MapMngr.DestroyLocation(loc);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyLocation(FOServer* server, ident_t locId)
{
    auto* loc = server->EntityMngr.GetLocation(locId);
    if (loc != nullptr) {
        server->MapMngr.DestroyLocation(loc);
    }
}

///@ ExportMethod
FO_SCRIPT_API Critter* Server_Game_GetCritter(FOServer* server, ident_t crId)
{
    if (!crId) {
        return nullptr;
    }

    return server->EntityMngr.GetCritter(crId);
}

///@ ExportMethod ExcludeInSingleplayer PassOwnership
FO_SCRIPT_API Player* Server_Game_GetPlayer(FOServer* server, string_view name)
{
    if (name.empty()) {
        throw ScriptException("Empty player name");
    }

    // Check existence
    const auto id = server->MakePlayerId(name);
    const auto doc = server->DbStorage.Get(server->PlayersCollectionName, id);

    if (doc.Empty()) {
        return nullptr;
    }

    // Find online
    auto* player = server->EntityMngr.GetPlayer(id);

    if (player != nullptr) {
        player->AddRef();
        return player;
    }

    // Load from db
    auto&& dummy_net_conn = SafeAlloc::MakeShared<DummyNetConnection>(server->Settings);
    player = SafeAlloc::MakeRaw<Player>(server, id, SafeAlloc::MakeUnique<ClientConnection>(std::move(dummy_net_conn)));
    player->SetName(name);

    if (!PropertiesSerializator::LoadFromDocument(&player->GetPropertiesForEdit(), doc, *server, *server)) {
        player->MarkAsDestroying();
        player->MarkAsDestroyed();
        player->Release();
        throw ScriptException("Player data db read failed");
    }

    return player;
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Game_GetGlobalMapCritters(FOServer* server, CritterFindType findType)
{
    return server->CrMngr.GetGlobalMapCritters(findType);
}

///@ ExportMethod
FO_SCRIPT_API Map* Server_Game_GetMap(FOServer* server, ident_t mapId)
{
    return server->EntityMngr.GetMap(mapId);
}

///@ ExportMethod
FO_SCRIPT_API Map* Server_Game_GetMap(FOServer* server, hstring mapPid)
{
    return server->MapMngr.GetMapByPid(mapPid, 0);
}

///@ ExportMethod
FO_SCRIPT_API Map* Server_Game_GetMap(FOServer* server, hstring mapPid, uint skipCount)
{
    return server->MapMngr.GetMapByPid(mapPid, skipCount);
}

///@ ExportMethod
FO_SCRIPT_API vector<Map*> Server_Game_GetMaps(FOServer* server)
{
    vector<Map*> maps;
    maps.reserve(server->EntityMngr.GetLocationsCount());

    for (auto&& [id, map] : server->EntityMngr.GetMaps()) {
        maps.emplace_back(map);
    }

    return maps;
}

///@ ExportMethod
FO_SCRIPT_API vector<Map*> Server_Game_GetMaps(FOServer* server, hstring pid)
{
    vector<Map*> maps;

    if (!pid) {
        maps.reserve(server->EntityMngr.GetLocationsCount());
    }

    for (auto&& [id, map] : server->EntityMngr.GetMaps()) {
        if (!pid || pid == map->GetProtoId()) {
            maps.emplace_back(map);
        }
    }

    return maps;
}

///@ ExportMethod
FO_SCRIPT_API Location* Server_Game_GetLocation(FOServer* server, ident_t locId)
{
    return server->EntityMngr.GetLocation(locId);
}

///@ ExportMethod
FO_SCRIPT_API Location* Server_Game_GetLocation(FOServer* server, hstring locPid)
{
    return server->MapMngr.GetLocationByPid(locPid, 0);
}

///@ ExportMethod
FO_SCRIPT_API Location* Server_Game_GetLocation(FOServer* server, hstring locPid, uint skipCount)
{
    return server->MapMngr.GetLocationByPid(locPid, skipCount);
}

///@ ExportMethod
FO_SCRIPT_API Location* Server_Game_GetLocation(FOServer* server, LocationComponent component)
{
    for (auto&& [id, loc] : server->EntityMngr.GetLocations()) {
        if (loc->GetProto()->HasComponent(static_cast<hstring::hash_t>(component))) {
            return loc;
        }
    }

    return nullptr;
}

///@ ExportMethod
FO_SCRIPT_API Location* Server_Game_GetLocation(FOServer* server, LocationProperty property, int propertyValue)
{
    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<Location>(server, property);

    for (auto&& [id, loc] : server->EntityMngr.GetLocations()) {
        if (loc->GetValueAsInt(prop) == propertyValue) {
            return loc;
        }
    }

    return nullptr;
}

///@ ExportMethod
FO_SCRIPT_API vector<Location*> Server_Game_GetLocations(FOServer* server)
{
    vector<Location*> locs;
    locs.reserve(server->EntityMngr.GetLocationsCount());

    for (auto&& [id, loc] : server->EntityMngr.GetLocations()) {
        locs.emplace_back(loc);
    }

    return locs;
}

///@ ExportMethod
FO_SCRIPT_API vector<Location*> Server_Game_GetLocations(FOServer* server, hstring pid)
{
    vector<Location*> locs;

    if (!pid) {
        locs.reserve(server->EntityMngr.GetLocationsCount());
    }

    for (auto&& [id, loc] : server->EntityMngr.GetLocations()) {
        if (!pid || pid == loc->GetProtoId()) {
            locs.emplace_back(loc);
        }
    }

    return locs;
}

///@ ExportMethod
FO_SCRIPT_API vector<Location*> Server_Game_GetLocations(FOServer* server, LocationComponent component)
{
    vector<Location*> locs;
    locs.reserve(server->EntityMngr.GetLocationsCount());

    for (auto&& [id, loc] : server->EntityMngr.GetLocations()) {
        if (loc->GetProto()->HasComponent(static_cast<hstring::hash_t>(component))) {
            locs.emplace_back(loc);
        }
    }

    return locs;
}

///@ ExportMethod
FO_SCRIPT_API vector<Location*> Server_Game_GetLocations(FOServer* server, LocationProperty property, int propertyValue)
{
    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<Location>(server, property);

    vector<Location*> locs;
    locs.reserve(server->EntityMngr.GetLocationsCount());

    for (auto&& [id, loc] : server->EntityMngr.GetLocations()) {
        if (loc->GetValueAsInt(prop) == propertyValue) {
            locs.emplace_back(loc);
        }
    }

    return locs;
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Game_RunDialog(FOServer* server, Critter* cr, Critter* npc, bool ignoreDistance)
{
    if (cr == nullptr) {
        throw ScriptException("Player arg is null");
    }
    if (!cr->GetControlledByPlayer()) {
        throw ScriptException("Player arg is not player");
    }
    if (npc == nullptr) {
        throw ScriptException("Npc arg is null");
    }
    if (npc->GetControlledByPlayer()) {
        throw ScriptException("Npc arg is not npc");
    }

    if (cr->Talk.Locked) {
        throw ScriptException("Can't open new dialog from demand, result or dialog functions");
    }

    server->BeginDialog(cr, npc, {}, {}, ignoreDistance);

    return cr->Talk.Type == TalkType::Critter && cr->Talk.CritterId == npc->GetId();
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Game_RunDialog(FOServer* server, Critter* cr, Critter* npc, hstring dlgPack, bool ignoreDistance)
{
    if (cr == nullptr) {
        throw ScriptException("Player arg is null");
    }
    if (!cr->GetControlledByPlayer()) {
        throw ScriptException("Player arg is not player");
    }
    if (npc == nullptr) {
        throw ScriptException("Npc arg is null");
    }
    if (npc->GetControlledByPlayer()) {
        throw ScriptException("Npc arg is not npc");
    }

    if (cr->Talk.Locked) {
        throw ScriptException("Can't open new dialog from demand, result or dialog functions");
    }

    server->BeginDialog(cr, npc, dlgPack, {}, ignoreDistance);

    return cr->Talk.Type == TalkType::Critter && cr->Talk.CritterId == npc->GetId();
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Game_RunDialog(FOServer* server, Critter* cr, hstring dlgPack, mpos hex, bool ignoreDistance)
{
    if (cr == nullptr) {
        throw ScriptException("Player arg is null");
    }
    if (!cr->GetControlledByPlayer()) {
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

///@ ExportMethod
FO_SCRIPT_API int64 Server_Game_GetWorldItemCount(FOServer* server, hstring pid)
{
    return server->ItemMngr.GetItemStatistics(pid);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_AddTextListener(FOServer* server, int sayType, string_view firstStr, int parameter, ScriptFunc<void, Critter*, string> func)
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

    server->TextListeners.emplace_back(tl);*/
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_RemoveTextListener(FOServer* server, int sayType, string_view firstStr, int parameter)
{
    UNUSED_VARIABLE(server);
    UNUSED_VARIABLE(sayType);
    UNUSED_VARIABLE(firstStr);
    UNUSED_VARIABLE(parameter);

    throw NotImplementedException(LINE_STR);

    /*for (auto it = server->TextListeners.begin(), end = server->TextListeners.end(); it != end; ++it)
    {
        TextListener& tl = *it;
        if (sayType == tl.SayType && strex(firstStr).compareIgnoreCaseUtf8(tl.FirstStr) && tl.Parameter == parameter)
        {
            server->TextListeners.erase(it);
            return;
        }
    }*/
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Game_GetAllItems(FOServer* server, hstring pid)
{
    vector<Item*> items;

    if (!pid) {
        items.reserve(server->EntityMngr.GetItemsCount());
    }

    for (auto&& [id, item] : server->EntityMngr.GetItems()) {
        RUNTIME_ASSERT(!item->IsDestroyed());
        if (!pid || pid == item->GetProtoId()) {
            items.emplace_back(item);
        }
    }

    return items;
}

///@ ExportMethod ExcludeInSingleplayer
FO_SCRIPT_API vector<Player*> Server_Game_GetOnlinePlayers(FOServer* server)
{
    const auto& players = server->EntityMngr.GetPlayers();

    vector<Player*> result;
    result.reserve(players.size());

    for (auto&& [id, player] : players) {
        result.emplace_back(player);
    }

    return result;
}

///@ ExportMethod ExcludeInSingleplayer
FO_SCRIPT_API vector<ident_t> Server_Game_GetRegisteredPlayerIds(FOServer* server)
{
    return server->DbStorage.GetAllIds(server->PlayersCollectionName);
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Game_GetAllNpc(FOServer* server)
{
    return server->CrMngr.GetNonPlayerCritters();
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Game_GetAllNpc(FOServer* server, hstring pid)
{
    vector<Critter*> npcs;

    for (auto* npc_ : server->CrMngr.GetNonPlayerCritters()) {
        if (!npc_->IsDestroyed() && (!pid || pid == npc_->GetProtoId())) {
            npcs.emplace_back(npc_);
        }
    }

    return npcs;
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_SetServerTime(FOServer* server, uint16 multiplier, uint16 year, uint16 month, uint16 day, uint16 hour, uint16 minute, uint16 second)
{
    server->SetServerTime(multiplier, year, month, day, hour, minute, second);
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Game_CallStaticItemFunction(FOServer* server, Critter* cr, StaticItem* staticItem, Item* usedItem, any_t param)
{
    UNUSED_VARIABLE(server);

    if (!staticItem->StaticScriptFunc) {
        return false;
    }

    return staticItem->StaticScriptFunc(cr, staticItem, usedItem, param) && staticItem->StaticScriptFunc.GetResult();
}

///@ ExportMethod
FO_SCRIPT_API vector<hstring> Server_Game_GetDialogs(FOServer* server)
{
    const auto& dlg_packs = server->DlgMngr.GetDialogs();

    vector<hstring> result;
    result.reserve(dlg_packs.size());

    for (const auto* dlg_pack : dlg_packs) {
        result.emplace_back(dlg_pack->PackId);
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<StaticItem*> Server_Game_GetStaticItemsForProtoMap(FOServer* server, ProtoMap* proto)
{
    const auto* static_map = server->MapMngr.GetStaticMap(proto);

    return static_map->StaticItems;
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Game_IsTextPresent(FOServer* server, TextPackName textPack, uint strNum)
{
    return server->GetLangPack().GetTextPack(textPack).GetStrCount(strNum) != 0;
}
