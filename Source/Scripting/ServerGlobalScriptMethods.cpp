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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "EntitySync.h"
#include "FileSystem.h"
#include "Geometry.h"
#include "NetworkServer.h"
#include "Platform.h"
#include "PropertiesSerializator.h"
#include "ScriptSystem.h"
#include "Server.h"

// SystemCall spawns a subprocess (CreateProcessW). This file is server-only, so the process-spawning code is
// never compiled into the client binary — keep it that way to avoid antivirus heuristics flagging the client.
#if FO_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif
#include "WinApiUndef.inc"

FO_BEGIN_NAMESPACE

///@ ExportMethod
FO_SCRIPT_API Critter* Server_Game_CreateCritter(ServerEngine* server, hstring protoId, bool forPlayer)
{
    return server->CreateCritter(protoId, forPlayer);
}

///@ ExportMethod
FO_SCRIPT_API Critter* Server_Game_CreateCritter(ServerEngine* server, ProtoCritter* proto, bool forPlayer)
{
    return server->CreateCritter(proto->GetProtoId(), forPlayer);
}

///@ ExportMethod
FO_SCRIPT_API Critter* Server_Game_CreateCritter(ServerEngine* server, hstring protoId, bool forPlayer, readonly_map<CritterProperty, any_t> props)
{
    const auto* proto = server->GetProtoCritter(protoId);

    if (proto == nullptr) {
        throw ScriptException("Invalid critter proto id arg", protoId);
    }

    Properties props_ = proto->GetProperties().Copy();

    for (const auto& [key, value] : props) {
        props_.SetValueAsAnyProps(static_cast<int32_t>(key), value);
    }

    return server->CreateCritter(protoId, forPlayer, &props_);
}

///@ ExportMethod
FO_SCRIPT_API Critter* Server_Game_CreateCritter(ServerEngine* server, ProtoCritter* proto, bool forPlayer, readonly_map<CritterProperty, any_t> props)
{
    Properties props_ = proto->GetProperties().Copy();

    for (const auto& [key, value] : props) {
        props_.SetValueAsAnyProps(static_cast<int32_t>(key), value);
    }

    return server->CreateCritter(proto->GetProtoId(), forPlayer, &props_);
}

///@ ExportMethod
FO_SCRIPT_API Critter* Server_Game_LoadCritter(ServerEngine* server, ident_t crId, bool forPlayer)
{
    return server->LoadCritter(crId, forPlayer);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_UnloadCritter(ServerEngine* server, Critter* cr)
{
    server->UnloadCritter(cr);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyUnloadedCritter(ServerEngine* server, ident_t crId)
{
    server->DestroyUnloadedCritter(crId);
}

///@ ExportMethod
FO_SCRIPT_API int32_t Server_Game_GetDistance(ServerEngine* server, Critter* cr1, Critter* cr2)
{
    ignore_unused(server);

    if (cr1->GetMapId() != cr2->GetMapId()) {
        throw ScriptException("Critters different maps");
    }
    if (!cr1->GetMapId()) {
        throw ScriptException("Critters not on map");
    }

    const auto dist = GeometryHelper::GetDistance(cr1->GetHex(), cr2->GetHex());
    const auto multihex = cr1->GetMultihex() + cr2->GetMultihex();
    return multihex < dist ? dist - multihex : 0;
}

///@ ExportMethod
FO_SCRIPT_API int32_t Server_Game_GetDistance(ServerEngine* server, Item* item1, Item* item2)
{
    ignore_unused(server);

    if (item1->GetMapId() != item2->GetMapId()) {
        throw ScriptException("Items different maps");
    }
    if (!item1->GetMapId()) {
        throw ScriptException("Items not on map");
    }

    const auto dist = GeometryHelper::GetDistance(item1->GetHex(), item2->GetHex());
    return dist;
}

///@ ExportMethod
FO_SCRIPT_API int32_t Server_Game_GetDistance(ServerEngine* server, Critter* cr, Item* item)
{
    ignore_unused(server);

    if (cr->GetMapId() != item->GetMapId()) {
        throw ScriptException("Critter/Item different maps");
    }
    if (!cr->GetMapId()) {
        throw ScriptException("Critter/Item not on map");
    }

    const auto dist = GeometryHelper::GetDistance(cr->GetHex(), item->GetHex());
    const auto multihex = cr->GetMultihex();
    return multihex < dist ? dist - multihex : 0;
}

///@ ExportMethod
FO_SCRIPT_API int32_t Server_Game_GetDistance(ServerEngine* server, Item* item, Critter* cr)
{
    ignore_unused(server);

    if (cr->GetMapId() != item->GetMapId()) {
        throw ScriptException("Item/Critter different maps");
    }
    if (!cr->GetMapId()) {
        throw ScriptException("Item/Critter not on map");
    }

    const auto dist = GeometryHelper::GetDistance(cr->GetHex(), item->GetHex());
    const auto multihex = cr->GetMultihex();
    return multihex < dist ? dist - multihex : 0;
}

///@ ExportMethod
FO_SCRIPT_API int32_t Server_Game_GetDistance(ServerEngine* server, Critter* cr, mpos hex)
{
    ignore_unused(server);

    if (!cr->GetMapId()) {
        throw ScriptException("Critter not on map");
    }

    const auto dist = GeometryHelper::GetDistance(cr->GetHex(), hex);
    const auto multihex = cr->GetMultihex();
    return multihex < dist ? dist - multihex : 0;
}

///@ ExportMethod
FO_SCRIPT_API int32_t Server_Game_GetDistance(ServerEngine* server, mpos hex, Critter* cr)
{
    ignore_unused(server);

    if (!cr->GetMapId()) {
        throw ScriptException("Critter not on map");
    }

    const auto dist = GeometryHelper::GetDistance(cr->GetHex(), hex);
    const auto multihex = cr->GetMultihex();
    return multihex < dist ? dist - multihex : 0;
}

///@ ExportMethod
FO_SCRIPT_API int32_t Server_Game_GetDistance(ServerEngine* server, Item* item, mpos hex)
{
    ignore_unused(server);

    if (!item->GetMapId()) {
        throw ScriptException("Item not on map");
    }

    const auto dist = GeometryHelper::GetDistance(item->GetHex(), hex);
    return dist;
}

///@ ExportMethod
FO_SCRIPT_API int32_t Server_Game_GetDistance(ServerEngine* server, mpos hex, Item* item)
{
    ignore_unused(server);

    if (!item->GetMapId()) {
        throw ScriptException("Item not on map");
    }

    const auto dist = GeometryHelper::GetDistance(item->GetHex(), hex);
    return dist;
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API FO_NULLABLE Item* Server_Game_GetItem(ServerEngine* server, ident_t itemId)
{
    if (!itemId) {
        throw ScriptException("Item id arg is zero");
    }

    auto item = server->EntityMngr.GetItem(itemId);
    return item.release_ownership();
}

///@ ExportMethod
FO_SCRIPT_API FO_NULLABLE Item* Server_Game_MoveItem(ServerEngine* server, Item* item, Critter* toCr)
{
    return server->ItemMngr.MoveItem(item, item->GetCount(), toCr);
}

///@ ExportMethod
FO_SCRIPT_API FO_NULLABLE Item* Server_Game_MoveItem(ServerEngine* server, Item* item, int32_t count, Critter* toCr)
{
    if (count <= 0) {
        return nullptr;
    }

    return server->ItemMngr.MoveItem(item, count, toCr);
}

///@ ExportMethod
FO_SCRIPT_API FO_NULLABLE Item* Server_Game_MoveItem(ServerEngine* server, Item* item, Map* toMap, mpos toHex)
{
    if (!toMap->GetSize().is_valid_pos(toHex)) {
        throw ScriptException("Invalid hexex args");
    }

    return server->ItemMngr.MoveItem(item, item->GetCount(), toMap, toHex);
}

///@ ExportMethod
FO_SCRIPT_API FO_NULLABLE Item* Server_Game_MoveItem(ServerEngine* server, Item* item, int32_t count, Map* toMap, mpos toHex)
{
    if (!toMap->GetSize().is_valid_pos(toHex)) {
        throw ScriptException("Invalid hexex args");
    }

    if (count <= 0) {
        return nullptr;
    }

    return server->ItemMngr.MoveItem(item, count, toMap, toHex);
}

///@ ExportMethod
FO_SCRIPT_API FO_NULLABLE Item* Server_Game_MoveItem(ServerEngine* server, Item* item, Item* toCont, any_t stackId = any_t {})
{
    return server->ItemMngr.MoveItem(item, item->GetCount(), toCont, stackId);
}

///@ ExportMethod
FO_SCRIPT_API FO_NULLABLE Item* Server_Game_MoveItem(ServerEngine* server, Item* item, int32_t count, Item* toCont, any_t stackId = any_t {})
{
    if (count <= 0) {
        return nullptr;
    }

    return server->ItemMngr.MoveItem(item, count, toCont, stackId);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_MoveItems(ServerEngine* server, readonly_vector<Item*> items, Critter* toCr)
{
    for (auto* item : items) {
        if (item == nullptr || item->IsDestroyed()) {
            continue;
        }

        server->ItemMngr.MoveItem(item, item->GetCount(), toCr);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_MoveItems(ServerEngine* server, readonly_vector<Item*> items, Map* toMap, mpos toHex)
{
    if (!toMap->GetSize().is_valid_pos(toHex)) {
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
FO_SCRIPT_API void Server_Game_MoveItems(ServerEngine* server, readonly_vector<Item*> items, Item* toCont, any_t stackId = any_t {})
{
    for (auto* item : items) {
        if (item == nullptr || item->IsDestroyed()) {
            continue;
        }

        server->ItemMngr.MoveItem(item, item->GetCount(), toCont, stackId);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyEntity(ServerEngine* server, ident_t id)
{
    if (auto entity = server->EntityMngr.GetEntity(id)) {
        server->EntityMngr.DestroyEntity(entity.get());
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyEntity(ServerEngine* server, FO_NULLABLE ServerEntity* entity)
{
    if (entity != nullptr) {
        server->EntityMngr.DestroyEntity(entity);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyEntities(ServerEngine* server, readonly_vector<ident_t> ids)
{
    for (const auto id : ids) {
        if (auto entity = server->EntityMngr.GetEntity(id)) {
            server->EntityMngr.DestroyEntity(entity.get());
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyEntities(ServerEngine* server, readonly_vector<ServerEntity*> entities)
{
    for (auto* entity : entities) {
        if (entity != nullptr) {
            server->EntityMngr.DestroyEntity(entity);
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyItem(ServerEngine* server, FO_NULLABLE Item* item)
{
    if (item != nullptr) {
        server->ItemMngr.DestroyItem(item);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyItem(ServerEngine* server, Item* item, int32_t count)
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
FO_SCRIPT_API void Server_Game_DestroyItem(ServerEngine* server, ident_t itemId)
{
    auto item = server->EntityMngr.GetItem(itemId);

    if (item != nullptr) {
        server->ItemMngr.DestroyItem(item.get());
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyItem(ServerEngine* server, ident_t itemId, int32_t count)
{
    auto item = server->EntityMngr.GetItem(itemId);

    if (item != nullptr && count > 0) {
        const auto cur_count = item->GetCount();

        if (count >= cur_count) {
            server->ItemMngr.DestroyItem(item.get());
        }
        else {
            item->SetCount(cur_count - count);
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyItems(ServerEngine* server, readonly_vector<Item*> items)
{
    for (auto* item : items) {
        if (item != nullptr) {
            server->ItemMngr.DestroyItem(item);
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyItems(ServerEngine* server, readonly_vector<ident_t> itemIds)
{
    for (const auto item_id : itemIds) {
        if (item_id) {
            auto item = server->EntityMngr.GetItem(item_id);

            if (item != nullptr) {
                server->ItemMngr.DestroyItem(item.get());
            }
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyCritter(ServerEngine* server, Critter* cr)
{
    if (cr != nullptr && !cr->GetControlledByPlayer()) {
        server->CrMngr.DestroyCritter(cr);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyCritter(ServerEngine* server, ident_t crId)
{
    if (crId) {
        if (auto cr = server->EntityMngr.GetCritter(crId); cr != nullptr && !cr->GetControlledByPlayer()) {
            server->CrMngr.DestroyCritter(cr.get());
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyCritters(ServerEngine* server, readonly_vector<Critter*> critters)
{
    for (auto* cr : critters) {
        if (cr != nullptr && !cr->GetControlledByPlayer()) {
            server->CrMngr.DestroyCritter(cr);
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyCritters(ServerEngine* server, readonly_vector<ident_t> critterIds)
{
    for (const auto id : critterIds) {
        if (id) {
            if (auto cr = server->EntityMngr.GetCritter(id); cr != nullptr && !cr->GetControlledByPlayer()) {
                server->CrMngr.DestroyCritter(cr.get());
            }
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API Location* Server_Game_CreateLocation(ServerEngine* server, hstring protoId)
{
    auto* loc = server->MapMngr.CreateLocation(protoId);
    FO_VERIFY_AND_THROW(loc, "Missing location instance");
    return loc;
}

///@ ExportMethod
FO_SCRIPT_API Location* Server_Game_CreateLocation(ServerEngine* server, ProtoLocation* proto)
{
    auto* loc = server->MapMngr.CreateLocation(proto->GetProtoId());
    FO_VERIFY_AND_THROW(loc, "Missing location instance");
    return loc;
}

///@ ExportMethod
FO_SCRIPT_API Location* Server_Game_CreateLocation(ServerEngine* server, hstring protoId, readonly_vector<hstring> map_pids)
{
    auto* loc = server->MapMngr.CreateLocation(protoId, map_pids);
    FO_VERIFY_AND_THROW(loc, "Missing location instance");
    return loc;
}

///@ ExportMethod
FO_SCRIPT_API Location* Server_Game_CreateLocation(ServerEngine* server, hstring protoId, readonly_map<LocationProperty, any_t> props)
{
    const auto* proto = server->GetProtoLocation(protoId);

    if (proto == nullptr) {
        throw ScriptException("Invalid location proto id arg", protoId);
    }

    auto props_ = proto->GetProperties().Copy();

    for (const auto& [key, value] : props) {
        props_.SetValueAsAnyProps(static_cast<int32_t>(key), value);
    }

    auto* loc = server->MapMngr.CreateLocation(protoId, {}, &props_);
    FO_VERIFY_AND_THROW(loc, "Missing location instance");
    return loc;
}

///@ ExportMethod
FO_SCRIPT_API Location* Server_Game_CreateLocation(ServerEngine* server, ProtoLocation* proto, readonly_map<LocationProperty, any_t> props)
{
    auto props_ = proto->GetProperties().Copy();

    for (const auto& [key, value] : props) {
        props_.SetValueAsAnyProps(static_cast<int32_t>(key), value);
    }

    auto* loc = server->MapMngr.CreateLocation(proto->GetProtoId(), {}, &props_);
    FO_VERIFY_AND_THROW(loc, "Missing location instance");
    return loc;
}

///@ ExportMethod
FO_SCRIPT_API Location* Server_Game_CreateLocation(ServerEngine* server, hstring protoId, readonly_vector<hstring> map_pids, readonly_map<LocationProperty, any_t> props)
{
    const auto* proto = server->GetProtoLocation(protoId);

    if (proto == nullptr) {
        throw ScriptException("Invalid location proto id arg", protoId);
    }

    auto props_ = proto->GetProperties().Copy();

    for (const auto& [key, value] : props) {
        props_.SetValueAsAnyProps(static_cast<int32_t>(key), value);
    }

    auto* loc = server->MapMngr.CreateLocation(protoId, map_pids, &props_);
    FO_VERIFY_AND_THROW(loc, "Missing location instance");
    return loc;
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyLocation(ServerEngine* server, FO_NULLABLE Location* loc)
{
    if (loc != nullptr) {
        server->MapMngr.DestroyLocation(loc);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyLocation(ServerEngine* server, ident_t locId)
{
    auto loc = server->EntityMngr.GetLocation(locId);

    if (loc != nullptr) {
        server->MapMngr.DestroyLocation(loc.get());
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyMap(ServerEngine* server, FO_NULLABLE Map* map)
{
    if (map != nullptr) {
        server->MapMngr.DestroyMap(map);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyMap(ServerEngine* server, ident_t mapId)
{
    auto map = server->EntityMngr.GetMap(mapId);

    if (map != nullptr) {
        server->MapMngr.DestroyMap(map.get());
    }
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API FO_NULLABLE Critter* Server_Game_GetCritter(ServerEngine* server, ident_t crId)
{
    if (!crId) {
        return nullptr;
    }

    auto cr = server->EntityMngr.GetCritter(crId);
    return cr.release_ownership();
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API FO_NULLABLE ServerEntity* Server_Game_GetEntity(ServerEngine* server, ident_t entityId)
{
    if (!entityId) {
        return nullptr;
    }

    auto entity = server->EntityMngr.GetEntity(entityId);
    return entity.release_ownership();
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API vector<Critter*> Server_Game_GetCritters(ServerEngine* server, CritterFindType findType)
{
    auto critters = server->EntityMngr.GetCritters();
    vector<Critter*> result;
    result.reserve(critters.size());

    for (auto& cr : critters) {
        if (cr->CheckFind(findType)) {
            result.emplace_back(cr.get());
        }
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API Player* Server_Game_CreateUnloginedPlayer(ServerEngine* server)
{
    auto dummy_net_conn = NetworkServer::CreateDummyConnection(server->Settings, NetworkServer::DummyConnectionState::Connected);
    return server->CreateUnloginedPlayer(std::move(dummy_net_conn));
}

///@ ExportMethod
FO_SCRIPT_API Player* Server_Game_LoginPlayerToNewRecord(ServerEngine* server, Player* unloginedPlayer)
{
    if (unloginedPlayer->GetLogined()) {
        throw ScriptException("Player is already logined");
    }

    return server->LoginPlayerToNewRecord(unloginedPlayer);
}

///@ ExportMethod
FO_SCRIPT_API Player* Server_Game_LoginPlayerToTempSession(ServerEngine* server, Player* unloginedPlayer)
{
    if (unloginedPlayer->GetLogined()) {
        throw ScriptException("Player is already logined");
    }

    return server->LoginPlayerToTempSession(unloginedPlayer);
}

///@ ExportMethod
FO_SCRIPT_API Player* Server_Game_LoginPlayerToExistentRecord(ServerEngine* server, Player* unloginedPlayer, ident_t playerId)
{
    if (unloginedPlayer->GetLogined()) {
        throw ScriptException("Player is already logined");
    }
    if (!playerId) {
        throw ScriptException("Player id arg is zero");
    }

    return server->LoginPlayerToExistentRecord(unloginedPlayer, playerId);
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API FO_NULLABLE Player* Server_Game_GetPlayer(ServerEngine* server, ident_t playerId)
{
    if (!playerId) {
        return nullptr;
    }

    auto player = server->EntityMngr.GetPlayer(playerId);
    return player.release_ownership();
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API FO_NULLABLE Map* Server_Game_GetMap(ServerEngine* server, ident_t mapId)
{
    auto map = server->EntityMngr.GetMap(mapId);
    return map.release_ownership();
}

///@ ExportMethod
FO_SCRIPT_API FO_NULLABLE Map* Server_Game_GetMap(ServerEngine* server, hstring mapPid, int32_t skipCount = 0)
{
    auto map = server->MapMngr.GetMapByPid(mapPid, skipCount);
    return map.get();
}

///@ ExportMethod
FO_SCRIPT_API FO_NULLABLE Map* Server_Game_GetMap(ServerEngine* server, ProtoMap* mapProto, int32_t skipCount = 0)
{
    auto map = server->MapMngr.GetMapByPid(mapProto->GetProtoId(), skipCount);
    return map.get();
}

///@ ExportMethod
FO_SCRIPT_API vector<Map*> Server_Game_GetMaps(ServerEngine* server)
{
    vector<Map*> result;
    result.reserve(server->EntityMngr.GetMapsCount());

    for (auto& map : server->EntityMngr.GetMaps()) {
        result.emplace_back(map.get());
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<Map*> Server_Game_GetMaps(ServerEngine* server, hstring pid)
{
    vector<Map*> result;

    if (!pid) {
        result.reserve(server->EntityMngr.GetMapsCount());
    }

    for (auto& map : server->EntityMngr.GetMaps()) {
        if (!pid || pid == map->GetProtoId()) {
            result.emplace_back(map.get());
        }
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<Map*> Server_Game_GetMaps(ServerEngine* server, ProtoMap* proto)
{
    vector<Map*> result;

    if (proto == nullptr) {
        result.reserve(server->EntityMngr.GetMapsCount());
    }

    for (auto& map : server->EntityMngr.GetMaps()) {
        if (proto == nullptr || proto->GetProtoId() == map->GetProtoId()) {
            result.emplace_back(map.get());
        }
    }

    return result;
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API FO_NULLABLE Location* Server_Game_GetLocation(ServerEngine* server, ident_t locId)
{
    auto loc = server->EntityMngr.GetLocation(locId);
    return loc.release_ownership();
}

///@ ExportMethod
FO_SCRIPT_API FO_NULLABLE Location* Server_Game_GetLocation(ServerEngine* server, hstring locPid, int32_t skipCount = 0)
{
    auto loc = server->MapMngr.GetLocationByPid(locPid, skipCount);
    return loc.get();
}

///@ ExportMethod
FO_SCRIPT_API FO_NULLABLE Location* Server_Game_GetLocation(ServerEngine* server, ProtoLocation* locProto, int32_t skipCount = 0)
{
    auto loc = server->MapMngr.GetLocationByPid(locProto->GetProtoId(), skipCount);
    return loc.get();
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API FO_NULLABLE Location* Server_Game_GetLocation(ServerEngine* server, LocationProperty property, int32_t propertyValue)
{
    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<Location>(server, property);
    auto locs = server->EntityMngr.GetLocations();

    for (auto& loc : locs) {
        if (loc->GetValueAsInt(prop) == propertyValue) {
            return loc.release_ownership();
        }
    }

    return nullptr;
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API vector<Location*> Server_Game_GetLocations(ServerEngine* server)
{
    auto locs = server->EntityMngr.GetLocations();
    vector<Location*> result;
    result.reserve(locs.size());

    for (auto& loc : locs) {
        result.emplace_back(loc.get());
    }

    return result;
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API vector<Location*> Server_Game_GetLocations(ServerEngine* server, hstring pid)
{
    auto locs = server->EntityMngr.GetLocations();
    vector<Location*> result;

    if (!pid) {
        result.reserve(locs.size());
    }

    for (auto& loc : locs) {
        if (!pid || pid == loc->GetProtoId()) {
            result.emplace_back(loc.get());
        }
    }

    return result;
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API vector<Location*> Server_Game_GetLocations(ServerEngine* server, ProtoLocation* proto)
{
    auto locs = server->EntityMngr.GetLocations();
    vector<Location*> result;

    if (proto == nullptr) {
        result.reserve(locs.size());
    }

    for (auto& loc : locs) {
        if (proto == nullptr || proto->GetProtoId() == loc->GetProtoId()) {
            result.emplace_back(loc.get());
        }
    }

    return result;
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API vector<Location*> Server_Game_GetLocations(ServerEngine* server, LocationProperty property, int32_t propertyValue)
{
    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<Location>(server, property);
    auto locs = server->EntityMngr.GetLocations();
    vector<Location*> result;
    result.reserve(locs.size());

    for (auto& loc : locs) {
        if (loc->GetValueAsInt(prop) == propertyValue) {
            result.emplace_back(loc.get());
        }
    }

    return result;
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API vector<Item*> Server_Game_GetAllItems(ServerEngine* server, hstring pid)
{
    auto items = server->EntityMngr.GetItems();
    vector<Item*> result;

    if (!pid) {
        result.reserve(items.size());
    }

    for (auto& item : items) {
        if (!pid || pid == item->GetProtoId()) {
            result.emplace_back(item.get());
        }
    }

    return result;
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API vector<Item*> Server_Game_GetAllItems(ServerEngine* server, ProtoItem* proto)
{
    auto items = server->EntityMngr.GetItems();
    vector<Item*> result;

    if (proto == nullptr) {
        result.reserve(items.size());
    }

    for (auto& item : items) {
        if (proto == nullptr || proto->GetProtoId() == item->GetProtoId()) {
            result.emplace_back(item.get());
        }
    }

    return result;
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API vector<Player*> Server_Game_GetOnlinePlayers(ServerEngine* server)
{
    auto players = server->EntityMngr.GetPlayers();
    vector<Player*> result;
    result.reserve(players.size());

    for (auto& player : players) {
        result.emplace_back(player.get());
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<ident_t> Server_Game_GetRegisteredPlayerIds(ServerEngine* server)
{
    return server->DbStorage.GetAllIntIds(server->PlayersCollectionName);
}

///@ ExportMethod
FO_SCRIPT_API vector<ident_t> Server_Game_DbGetAllRecordIds(ServerEngine* server, hstring collectionName)
{
    if (!collectionName) {
        throw ScriptException("Collection name arg is empty");
    }

    return server->DbStorage.GetAllIntIds(collectionName);
}

///@ ExportMethod
FO_SCRIPT_API vector<string> Server_Game_DbGetAllRecordKeys(ServerEngine* server, hstring collectionName)
{
    if (!collectionName) {
        throw ScriptException("Collection name arg is empty");
    }

    return server->DbStorage.GetAllStringIds(collectionName);
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Game_DbHasEntity(ServerEngine* server, ServerEntity* entity)
{
    return server->DbStorage.Valid(entity->GetTypeNamePlural(), entity->GetId());
}

///@ ExportMethod
FO_SCRIPT_API map<string, string> Server_Game_DbGetPlayerData(ServerEngine* server, ident_t playerId)
{
    if (!playerId) {
        throw ScriptException("Player id arg is zero");
    }

    const auto doc = server->DbStorage.Get(server->PlayersCollectionName, playerId);
    map<string, string> result;

    for (const auto& [key, value] : doc) {
        result.emplace(key, AnyData::ValueToString(value));
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Game_DbHasRecord(ServerEngine* server, hstring collectionName, ident_t id)
{
    if (!collectionName) {
        throw ScriptException("Collection name arg is empty");
    }
    if (!id) {
        throw ScriptException("Record id arg is zero");
    }

    return server->DbStorage.Valid(collectionName, id);
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Game_DbHasRecord(ServerEngine* server, hstring collectionName, string_view id)
{
    if (!collectionName) {
        throw ScriptException("Collection name arg is empty");
    }
    if (id.empty()) {
        throw ScriptException("Record id arg is empty");
    }

    return server->DbStorage.Valid(collectionName, string(id));
}

///@ ExportMethod
FO_SCRIPT_API map<string, string> Server_Game_DbGetRecord(ServerEngine* server, hstring collectionName, ident_t id)
{
    if (!collectionName) {
        throw ScriptException("Collection name arg is empty");
    }
    if (!id) {
        throw ScriptException("Record id arg is zero");
    }

    const auto doc = server->DbStorage.Get(collectionName, id);
    map<string, string> result;

    for (const auto& [key, value] : doc) {
        result.emplace(key, AnyData::ValueToString(value));
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API map<string, string> Server_Game_DbGetRecord(ServerEngine* server, hstring collectionName, string_view id)
{
    if (!collectionName) {
        throw ScriptException("Collection name arg is empty");
    }
    if (id.empty()) {
        throw ScriptException("Record id arg is empty");
    }

    const auto doc = server->DbStorage.Get(collectionName, string(id));
    map<string, string> result;

    for (const auto& [key, value] : doc) {
        result.emplace(key, AnyData::ValueToString(value));
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DbInsertRecord(ServerEngine* server, hstring collectionName, ident_t id, readonly_map<string, string> keyValues)
{
    if (!collectionName) {
        throw ScriptException("Collection name arg is empty");
    }
    if (!id) {
        throw ScriptException("Record id arg is zero");
    }
    if (keyValues.empty()) {
        throw ScriptException("Record values are empty");
    }

    if (server->DbStorage.Valid(collectionName, id)) {
        throw ScriptException("Record already exists");
    }

    AnyData::Document doc;

    for (const auto& [key, value] : keyValues) {
        if (key.empty()) {
            throw ScriptException("Record key is empty");
        }
        if (!strvex(key).is_valid_utf8()) {
            throw ScriptException("Record key has invalid encoding");
        }
        if (!strvex(value).is_valid_utf8()) {
            throw ScriptException("Record value has invalid encoding");
        }

        doc.Emplace(key, string(value));
    }

    server->DbStorage.Insert(collectionName, id, doc);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DbInsertRecord(ServerEngine* server, hstring collectionName, string_view id, readonly_map<string, string> keyValues)
{
    if (!collectionName) {
        throw ScriptException("Collection name arg is empty");
    }
    if (id.empty()) {
        throw ScriptException("Record id arg is empty");
    }
    if (keyValues.empty()) {
        throw ScriptException("Record values are empty");
    }

    if (server->DbStorage.Valid(collectionName, string(id))) {
        throw ScriptException("Record already exists");
    }

    AnyData::Document doc;

    for (const auto& [key, value] : keyValues) {
        if (key.empty()) {
            throw ScriptException("Record key is empty");
        }
        if (!strvex(key).is_valid_utf8()) {
            throw ScriptException("Record key has invalid encoding");
        }
        if (!strvex(value).is_valid_utf8()) {
            throw ScriptException("Record value has invalid encoding");
        }

        doc.Emplace(key, string(value));
    }

    server->DbStorage.Insert(collectionName, string(id), doc);
}

namespace
{
    template<typename T>
    static void ValidateAndUpdateRecord(ServerEngine* server, hstring collectionName, const auto& id, string_view key, const T& value)
    {
        if (!collectionName) {
            throw ScriptException("Collection name arg is empty");
        }
        if constexpr (std::is_same_v<std::decay_t<decltype(id)>, ident_t>) {
            if (!id) {
                throw ScriptException("Record id arg is zero");
            }
        }
        else {
            if (id.empty()) {
                throw ScriptException("Record id arg is empty");
            }
        }
        if (key.empty()) {
            throw ScriptException("Record key is empty");
        }
        if (!strvex(key).is_valid_utf8()) {
            throw ScriptException("Record key has invalid encoding");
        }
        if constexpr (std::is_same_v<T, string_view>) {
            if (!strvex(value).is_valid_utf8()) {
                throw ScriptException("Record value has invalid encoding");
            }
        }

        if (!server->DbStorage.Valid(collectionName, id)) {
            throw ScriptException("Record not found");
        }

        if constexpr (std::is_same_v<T, string_view>) {
            server->DbStorage.Update(collectionName, id, key, string(value));
        }
        else {
            server->DbStorage.Update(collectionName, id, key, value);
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DbUpdateRecordString(ServerEngine* server, hstring collectionName, ident_t id, string_view key, string_view value)
{
    ValidateAndUpdateRecord(server, collectionName, id, key, value);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DbUpdateRecordString(ServerEngine* server, hstring collectionName, string_view id, string_view key, string_view value)
{
    ValidateAndUpdateRecord(server, collectionName, string(id), key, value);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DbUpdateRecordInt64(ServerEngine* server, hstring collectionName, ident_t id, string_view key, int64_t value)
{
    ValidateAndUpdateRecord(server, collectionName, id, key, value);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DbUpdateRecordInt64(ServerEngine* server, hstring collectionName, string_view id, string_view key, int64_t value)
{
    ValidateAndUpdateRecord(server, collectionName, string(id), key, value);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DbUpdateRecordFloat64(ServerEngine* server, hstring collectionName, ident_t id, string_view key, float64_t value)
{
    ValidateAndUpdateRecord(server, collectionName, id, key, value);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DbUpdateRecordFloat64(ServerEngine* server, hstring collectionName, string_view id, string_view key, float64_t value)
{
    ValidateAndUpdateRecord(server, collectionName, string(id), key, value);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DbUpdateRecordBool(ServerEngine* server, hstring collectionName, ident_t id, string_view key, bool value)
{
    ValidateAndUpdateRecord(server, collectionName, id, key, value);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DbUpdateRecordBool(ServerEngine* server, hstring collectionName, string_view id, string_view key, bool value)
{
    ValidateAndUpdateRecord(server, collectionName, string(id), key, value);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DbRemoveRecord(ServerEngine* server, hstring collectionName, ident_t id)
{
    if (!collectionName) {
        throw ScriptException("Collection name arg is empty");
    }
    if (!id) {
        throw ScriptException("Record id arg is zero");
    }

    if (server->DbStorage.Valid(collectionName, id)) {
        server->DbStorage.Delete(collectionName, id);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DbRemoveRecord(ServerEngine* server, hstring collectionName, string_view id)
{
    if (!collectionName) {
        throw ScriptException("Collection name arg is empty");
    }
    if (id.empty()) {
        throw ScriptException("Record id arg is empty");
    }

    if (server->DbStorage.Valid(collectionName, string(id))) {
        server->DbStorage.Delete(collectionName, string(id));
    }
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Game_GetAllNpc(ServerEngine* server)
{
    auto npcs = server->CrMngr.GetNonPlayerCritters();
    vector<Critter*> result;
    result.reserve(npcs.size());

    for (auto& cr : npcs) {
        result.emplace_back(cr.get());
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Game_GetAllNpc(ServerEngine* server, hstring pid)
{
    vector<Critter*> result;

    for (auto& cr : server->CrMngr.GetNonPlayerCritters()) {
        if (!pid || pid == cr->GetProtoId()) {
            result.emplace_back(cr.get());
        }
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Game_GetAllNpc(ServerEngine* server, ProtoCritter* proto)
{
    FO_VERIFY_AND_THROW(proto, "Missing prototype instance");

    vector<Critter*> result;

    for (auto& cr : server->CrMngr.GetNonPlayerCritters()) {
        if (proto->GetProtoId() == cr->GetProtoId()) {
            result.emplace_back(cr.get());
        }
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_SetSynchronizedTime(ServerEngine* server, synctime time)
{
    server->GameTime.SetSynchronizedTime(time);
    server->SetSynchronizedTime(time);
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Game_CallStaticItemFunction(ServerEngine* server, FO_NULLABLE Critter* cr, StaticItem* staticItem, FO_NULLABLE Item* usedItem, any_t param)
{
    ignore_unused(server);

    if (!staticItem->StaticScriptFunc) {
        return false;
    }

    return staticItem->StaticScriptFunc.Call(cr, staticItem, usedItem, param) && staticItem->StaticScriptFunc.GetResult();
}

///@ ExportMethod
FO_SCRIPT_API vector<StaticItem*> Server_Game_GetStaticItemsForProtoMap(ServerEngine* server, ProtoMap* proto)
{
    auto static_map = server->MapMngr.GetStaticMap(proto);
    return vec_transform(static_map->StaticItems, [](auto&& item) -> StaticItem* { return item.get(); });
}

///@ ExportMethod
FO_SCRIPT_API vector<ProtoCritter*> Server_Game_GetProtoCrittersForProtoMap(ServerEngine* server, ProtoMap* proto)
{
    FO_VERIFY_AND_THROW(proto, "Missing prototype instance");

    const auto* static_map = server->MapMngr.GetStaticMap(proto);
    vector<ProtoCritter*> proto_critters;
    proto_critters.reserve(static_map->CritterBillets.size());

    for (const pair<ident_t, refcount_ptr<Critter>>& billet : static_map->CritterBillets) {
        FO_VERIFY_AND_THROW(billet.second, "Missing required billet second");
        const auto* proto_cr = dynamic_cast<const ProtoCritter*>(billet.second->GetProto());
        FO_VERIFY_AND_THROW(proto_cr, "Missing required prototype critter");
        proto_critters.emplace_back(const_cast<ProtoCritter*>(proto_cr));
    }

    return proto_critters;
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Game_IsTextPresent(ServerEngine* server, TextPackKey textKey)
{
    return server->GetLangPack().IsTextPresent(textKey);
}

///@ ExportMethod
FO_SCRIPT_API int32_t Server_Game_GetTextCount(ServerEngine* server, TextPackKey textKey)
{
    return numeric_cast<int32_t>(server->GetLangPack().GetTextCount(textKey));
}

// Server-only on purpose: the subprocess-spawning Win32 path (CreateProcessW with a hidden window + pipe
// capture) must not land in the client binary, where antivirus heuristics would flag it.
static auto SystemCall(string_view command, const function<void(string_view)>& log_callback) -> int32_t
{
    const auto print_log = [&log_callback](string& log, bool last_call) {
        log = strex(log).replace('\r', '\n', '\n').erase('\r');

        while (true) {
            auto pos = log.find('\n');

            if (pos == string::npos && last_call && !log.empty()) {
                pos = log.size();
            }

            if (pos != string::npos) {
                log_callback(log.substr(0, pos));
                log.erase(0, pos + 1);
            }
            else {
                break;
            }
        }
    };

#if FO_WINDOWS
    HANDLE out_read = nullptr;
    HANDLE out_write = nullptr;

    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    if (::CreatePipe(&out_read, &out_write, &sa, 0) == 0) {
        return -1;
    }

    if (::SetHandleInformation(out_read, HANDLE_FLAG_INHERIT, 0) == 0) {
        ::CloseHandle(out_read);
        ::CloseHandle(out_write);
        return -1;
    }

    STARTUPINFOW si = {};
    si.cb = sizeof(STARTUPINFO);
    si.hStdError = out_write;
    si.hStdOutput = out_write;
    si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};

    auto wcommand = strex(command).to_wide_char();
    const auto result = ::CreateProcessW(nullptr, wcommand.data(), nullptr, //
        nullptr, TRUE, 0, nullptr, nullptr, &si, &pi);

    if (result == 0) {
        ::CloseHandle(out_read);
        ::CloseHandle(out_write);
        return -1;
    }

    string log;

    while (true) {
        while (true) {
            DWORD bytes = 0;

            if (::PeekNamedPipe(out_read, nullptr, 0, nullptr, &bytes, nullptr) == 0) {
                break;
            }
            if (bytes == 0) {
                break;
            }

            char buf[4096] = {};

            if (::ReadFile(out_read, buf, sizeof(buf), &bytes, nullptr) != 0) {
                log.append(buf, bytes);
                print_log(log, false);
            }
        }

        if (::WaitForSingleObject(pi.hProcess, 1) != WAIT_TIMEOUT) {
            break;
        }
    }

    print_log(log, true);

    DWORD retval = 0;
    ::GetExitCodeProcess(pi.hProcess, &retval);

    ::CloseHandle(out_read);
    ::CloseHandle(out_write);
    ::CloseHandle(pi.hProcess);
    ::CloseHandle(pi.hThread);

    return std::bit_cast<int32_t>(retval);

#elif !FO_WINDOWS && !FO_WEB
    FILE* in = popen(string(command).c_str(), "r");

    if (in == nullptr) {
        return -1;
    }

    string log;
    char buf[4096];

    while (fgets(buf, sizeof(buf), in)) {
        log += buf;
        print_log(log, false);
    }

    print_log(log, true);

    return pclose(in);

#else
    return 1;
#endif
}

///@ ExportMethod
FO_SCRIPT_API int32_t Server_Game_SystemCall(ServerEngine* server, string_view command)
{
    ignore_unused(server);

    auto prefix = command.substr(0, command.find(' '));
    return SystemCall(command, [&prefix](string_view line) { WriteLog("{} : {}\n", prefix, line); });
}

///@ ExportMethod
FO_SCRIPT_API int32_t Server_Game_SystemCall(ServerEngine* server, string_view command, string& output)
{
    ignore_unused(server);

    output = "";

    return SystemCall(command, [&output](string_view line) {
        if (!output.empty()) {
            output += "\n";
        }
        output += line;
    });
}

///@ ExportMethod Async
FO_SCRIPT_API void Server_Game_Sync(ServerEngine* server, ServerEntity* entity)
{
    auto* ctx = server->GetCurrentSyncContext();
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");
    ServerEntity* entities[] = {entity};
    ctx->SyncEntities(entities);
}

///@ ExportMethod Async
FO_SCRIPT_API void Server_Game_Sync(ServerEngine* server, ServerEntity* entity1, ServerEntity* entity2)
{
    auto* ctx = server->GetCurrentSyncContext();
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");
    ServerEntity* entities[] = {entity1, entity2};
    ctx->SyncEntities(entities);
}

///@ ExportMethod Async
FO_SCRIPT_API void Server_Game_Sync(ServerEngine* server, ServerEntity* entity1, ServerEntity* entity2, ServerEntity* entity3)
{
    auto* ctx = server->GetCurrentSyncContext();
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");
    ServerEntity* entities[] = {entity1, entity2, entity3};
    ctx->SyncEntities(entities);
}

///@ ExportMethod Async
FO_SCRIPT_API void Server_Game_Sync(ServerEngine* server, readonly_vector<ServerEntity*> entities)
{
    vector<ServerEntity*> non_null;
    non_null.reserve(entities.size());

    for (auto* entity : entities) {
        if (entity == nullptr) {
            throw ScriptException("Entity in array arg is null");
        }

        non_null.emplace_back(entity);
    }

    auto* ctx = server->GetCurrentSyncContext();
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");
    ctx->SyncEntities(non_null);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_SyncEnsure(ServerEngine* server, ServerEntity* entity)
{
    auto* ctx = server->GetCurrentSyncContext();
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");
    ctx->EnsureEntitySynced(entity);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_SyncRelease(ServerEngine* server)
{
    auto* ctx = server->GetCurrentSyncContext();
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");
    ctx->Release();
}

///@ ExportMethod
FO_SCRIPT_API vector<ServerEntity*> Server_Game_GetHeldSyncEntities(ServerEngine* server)
{
    auto* ctx = server->GetCurrentSyncContext();
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");
    return ctx->GetHeldEntities();
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Game_IsEntityLocked(ServerEngine* server, FO_NULLABLE ServerEntity* entity)
{
    auto* ctx = server->GetCurrentSyncContext();
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");
    return IsEntityAccessValid(entity, false);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_Lock(ServerEngine* server)
{
    auto* ctx = server->GetCurrentSyncContext();
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");
    ctx->LockSingleton(server->GetEntityLock());
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_Unlock(ServerEngine* server)
{
    auto* ctx = server->GetCurrentSyncContext();
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");
    ctx->UnlockSingleton(server->GetEntityLock());
}

///@ ExportMethod
FO_SCRIPT_API int64_t Server_Game_GetProcessMemoryUsage(ServerEngine* server)
{
    ignore_unused(server);
    return static_cast<int64_t>(Platform::GetProcessMemoryUsage());
}

///@ ExportMethod
FO_SCRIPT_API int64_t Server_Game_GetAllocatorMemoryUsage(ServerEngine* server)
{
    ignore_unused(server);
    return static_cast<int64_t>(AllocatorGetInUseBytes());
}

///@ ExportMethod
FO_SCRIPT_API int32_t Server_Game_GetEntityRegistryCount(ServerEngine* server)
{
    return static_cast<int32_t>(server->EntityMngr.GetEntitiesCount());
}

///@ ExportMethod
FO_SCRIPT_API int32_t Server_Game_GetPlayerRegistryCount(ServerEngine* server)
{
    return static_cast<int32_t>(server->EntityMngr.GetPlayersCount());
}

///@ ExportMethod
FO_SCRIPT_API int32_t Server_Game_GetLocationRegistryCount(ServerEngine* server)
{
    return static_cast<int32_t>(server->EntityMngr.GetLocationsCount());
}

///@ ExportMethod
FO_SCRIPT_API int32_t Server_Game_GetMapRegistryCount(ServerEngine* server)
{
    return static_cast<int32_t>(server->EntityMngr.GetMapsCount());
}

///@ ExportMethod
FO_SCRIPT_API int32_t Server_Game_GetCritterRegistryCount(ServerEngine* server)
{
    return static_cast<int32_t>(server->EntityMngr.GetCrittersCount());
}

///@ ExportMethod
FO_SCRIPT_API int32_t Server_Game_GetItemRegistryCount(ServerEngine* server)
{
    return static_cast<int32_t>(server->EntityMngr.GetItemsCount());
}

FO_END_NAMESPACE
