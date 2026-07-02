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

static auto RequireCurrentSyncContext(ptr<ServerEngine> server) -> ptr<SyncContext>
{
    FO_STACK_TRACE_ENTRY();

    auto nullable_ctx = server->GetCurrentSyncContext();
    FO_VERIFY_AND_THROW(nullable_ctx, "Missing script execution context");

    return nullable_ctx.as_ptr();
}

///@ ExportMethod
FO_SCRIPT_API ptr<Critter> Server_Game_CreateCritter(ptr<ServerEngine> server, hstring protoId, bool forPlayer)
{
    auto cr = server->CreateCritter(protoId, forPlayer);
    return cr;
}

///@ ExportMethod
FO_SCRIPT_API ptr<Critter> Server_Game_CreateCritter(ptr<ServerEngine> server, ptr<ProtoCritter> proto, bool forPlayer)
{
    auto cr = server->CreateCritter(proto->GetProtoId(), forPlayer);
    return cr;
}

///@ ExportMethod
FO_SCRIPT_API ptr<Critter> Server_Game_CreateCritter(ptr<ServerEngine> server, hstring protoId, bool forPlayer, readonly_map<CritterProperty, any_t> props)
{
    auto nullable_proto = server->GetProtoCritter(protoId);

    if (!nullable_proto) {
        throw ScriptException("Invalid critter proto id arg", protoId);
    }

    auto proto = nullable_proto.as_ptr();
    Properties props_ = proto->GetProperties().Copy();

    for (const auto& [key, value] : props) {
        props_.SetValueAsAnyProps(static_cast<int32_t>(key), value);
    }

    nptr<const Properties> props_ptr = &props_;
    auto cr = server->CreateCritter(protoId, forPlayer, props_ptr);
    return cr;
}

///@ ExportMethod
FO_SCRIPT_API ptr<Critter> Server_Game_CreateCritter(ptr<ServerEngine> server, ptr<ProtoCritter> proto, bool forPlayer, readonly_map<CritterProperty, any_t> props)
{
    Properties props_ = proto->GetProperties().Copy();

    for (const auto& [key, value] : props) {
        props_.SetValueAsAnyProps(static_cast<int32_t>(key), value);
    }

    nptr<const Properties> props_ptr = &props_;
    auto cr = server->CreateCritter(proto->GetProtoId(), forPlayer, props_ptr);
    return cr;
}

///@ ExportMethod
FO_SCRIPT_API ptr<Critter> Server_Game_LoadCritter(ptr<ServerEngine> server, ident_t crId, bool forPlayer)
{
    auto cr = server->LoadCritter(crId, forPlayer);
    return cr;
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_UnloadCritter(ptr<ServerEngine> server, ptr<Critter> cr)
{
    server->UnloadCritter(cr);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyUnloadedCritter(ptr<ServerEngine> server, ident_t crId)
{
    server->DestroyUnloadedCritter(crId);
}

///@ ExportMethod
FO_SCRIPT_API int32_t Server_Game_GetDistance(ptr<ServerEngine> server, ptr<Critter> cr1, ptr<Critter> cr2)
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
FO_SCRIPT_API int32_t Server_Game_GetDistance(ptr<ServerEngine> server, ptr<Item> item1, ptr<Item> item2)
{
    ignore_unused(server);

    if (item1->GetMapId() != item2->GetMapId()) {
        throw ScriptException("Items different maps");
    }
    if (!item1->GetMapId()) {
        throw ScriptException("Items not on map");
    }

    return GeometryHelper::GetDistance(item1->GetHex(), item2->GetHex());
}

///@ ExportMethod
FO_SCRIPT_API int32_t Server_Game_GetDistance(ptr<ServerEngine> server, ptr<Critter> cr, ptr<Item> item)
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
FO_SCRIPT_API int32_t Server_Game_GetDistance(ptr<ServerEngine> server, ptr<Item> item, ptr<Critter> cr)
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
FO_SCRIPT_API int32_t Server_Game_GetDistance(ptr<ServerEngine> server, ptr<Critter> cr, mpos hex)
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
FO_SCRIPT_API int32_t Server_Game_GetDistance(ptr<ServerEngine> server, mpos hex, ptr<Critter> cr)
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
FO_SCRIPT_API int32_t Server_Game_GetDistance(ptr<ServerEngine> server, ptr<Item> item, mpos hex)
{
    ignore_unused(server);

    if (!item->GetMapId()) {
        throw ScriptException("Item not on map");
    }

    return GeometryHelper::GetDistance(item->GetHex(), hex);
}

///@ ExportMethod
FO_SCRIPT_API int32_t Server_Game_GetDistance(ptr<ServerEngine> server, mpos hex, ptr<Item> item)
{
    ignore_unused(server);

    if (!item->GetMapId()) {
        throw ScriptException("Item not on map");
    }

    return GeometryHelper::GetDistance(item->GetHex(), hex);
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API nptr<Item> Server_Game_GetItem(ptr<ServerEngine> server, ident_t itemId)
{
    if (!itemId) {
        throw ScriptException("Item id arg is zero");
    }

    auto item = server->EntityMngr.GetItem(itemId);
    return ReleaseNullableScriptOwnership(std::move(item));
}

///@ ExportMethod
FO_SCRIPT_API nptr<Item> Server_Game_MoveItem(ptr<ServerEngine> server, ptr<Item> item, ptr<Critter> toCr)
{
    auto moved_item = server->ItemMngr.MoveItem(item, item->GetCount(), toCr);
    return moved_item;
}

///@ ExportMethod
FO_SCRIPT_API nptr<Item> Server_Game_MoveItem(ptr<ServerEngine> server, ptr<Item> item, int32_t count, ptr<Critter> toCr)
{
    if (count <= 0) {
        return nullptr;
    }

    auto moved_item = server->ItemMngr.MoveItem(item, count, toCr);
    return moved_item;
}

///@ ExportMethod
FO_SCRIPT_API nptr<Item> Server_Game_MoveItem(ptr<ServerEngine> server, ptr<Item> item, ptr<Map> toMap, mpos toHex)
{
    if (!toMap->GetSize().is_valid_pos(toHex)) {
        throw ScriptException("Invalid hexex args");
    }

    auto moved_item = server->ItemMngr.MoveItem(item, item->GetCount(), toMap, toHex);
    return moved_item;
}

///@ ExportMethod
FO_SCRIPT_API nptr<Item> Server_Game_MoveItem(ptr<ServerEngine> server, ptr<Item> item, int32_t count, ptr<Map> toMap, mpos toHex)
{
    if (!toMap->GetSize().is_valid_pos(toHex)) {
        throw ScriptException("Invalid hexex args");
    }

    if (count <= 0) {
        return nullptr;
    }

    auto moved_item = server->ItemMngr.MoveItem(item, count, toMap, toHex);
    return moved_item;
}

///@ ExportMethod
FO_SCRIPT_API nptr<Item> Server_Game_MoveItem(ptr<ServerEngine> server, ptr<Item> item, ptr<Item> toCont, any_t stackId = any_t {})
{
    auto moved_item = server->ItemMngr.MoveItem(item, item->GetCount(), toCont, stackId);
    return moved_item;
}

///@ ExportMethod
FO_SCRIPT_API nptr<Item> Server_Game_MoveItem(ptr<ServerEngine> server, ptr<Item> item, int32_t count, ptr<Item> toCont, any_t stackId = any_t {})
{
    if (count <= 0) {
        return nullptr;
    }

    auto moved_item = server->ItemMngr.MoveItem(item, count, toCont, stackId);
    return moved_item;
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_MoveItems(ptr<ServerEngine> server, readonly_vector<Item*> items, ptr<Critter> toCr)
{
    for (nptr<Item> nullable_item : items) {
        if (!nullable_item) {
            continue;
        }

        auto item = nullable_item.as_ptr();
        if (item->IsDestroyed()) {
            continue;
        }

        server->ItemMngr.MoveItem(item, item->GetCount(), toCr);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_MoveItems(ptr<ServerEngine> server, readonly_vector<Item*> items, ptr<Map> toMap, mpos toHex)
{
    if (!toMap->GetSize().is_valid_pos(toHex)) {
        throw ScriptException("Invalid hexex args");
    }

    for (nptr<Item> nullable_item : items) {
        if (!nullable_item) {
            continue;
        }

        auto item = nullable_item.as_ptr();
        if (item->IsDestroyed()) {
            continue;
        }

        server->ItemMngr.MoveItem(item, item->GetCount(), toMap, toHex);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_MoveItems(ptr<ServerEngine> server, readonly_vector<Item*> items, ptr<Item> toCont, any_t stackId = any_t {})
{
    for (nptr<Item> nullable_item : items) {
        if (!nullable_item) {
            continue;
        }

        auto item = nullable_item.as_ptr();
        if (item->IsDestroyed()) {
            continue;
        }

        server->ItemMngr.MoveItem(item, item->GetCount(), toCont, stackId);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyEntity(ptr<ServerEngine> server, ident_t id)
{
    auto entity = server->EntityMngr.GetEntity(id);

    if (entity) {
        server->EntityMngr.DestroyEntity(entity.as_ptr());
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyEntity(ptr<ServerEngine> server, nptr<ServerEntity> entity)
{
    if (entity) {
        server->EntityMngr.DestroyEntity(entity.as_ptr());
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyEntities(ptr<ServerEngine> server, readonly_vector<ident_t> ids)
{
    for (const ident_t id : ids) {
        auto entity = server->EntityMngr.GetEntity(id);

        if (entity) {
            server->EntityMngr.DestroyEntity(entity.as_ptr());
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyEntities(ptr<ServerEngine> server, readonly_vector<ServerEntity*> entities)
{
    for (nptr<ServerEntity> entity : entities) {
        if (entity) {
            server->EntityMngr.DestroyEntity(entity.as_ptr());
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyItem(ptr<ServerEngine> server, nptr<Item> item)
{
    if (item) {
        server->ItemMngr.DestroyItem(item.as_ptr());
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyItem(ptr<ServerEngine> server, nptr<Item> item, int32_t count)
{
    if (item && count > 0) {
        auto item_ref = item.as_ptr();
        const auto cur_count = item_ref->GetCount();

        if (count >= cur_count) {
            server->ItemMngr.DestroyItem(item_ref);
        }
        else {
            item_ref->SetCount(cur_count - count);
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyItem(ptr<ServerEngine> server, ident_t itemId)
{
    auto item = server->EntityMngr.GetItem(itemId);

    if (item) {
        server->ItemMngr.DestroyItem(item.as_ptr());
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyItem(ptr<ServerEngine> server, ident_t itemId, int32_t count)
{
    auto nullable_item = server->EntityMngr.GetItem(itemId);

    if (nullable_item && count > 0) {
        auto item = nullable_item.as_ptr();
        const int32_t cur_count = item->GetCount();

        if (count >= cur_count) {
            server->ItemMngr.DestroyItem(item);
        }
        else {
            item->SetCount(cur_count - count);
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyItems(ptr<ServerEngine> server, readonly_vector<Item*> items)
{
    for (nptr<Item> item : items) {
        if (item) {
            server->ItemMngr.DestroyItem(item.as_ptr());
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyItems(ptr<ServerEngine> server, readonly_vector<ident_t> itemIds)
{
    for (const ident_t item_id : itemIds) {
        if (item_id) {
            auto item = server->EntityMngr.GetItem(item_id);

            if (item) {
                server->ItemMngr.DestroyItem(item.as_ptr());
            }
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyCritter(ptr<ServerEngine> server, nptr<Critter> cr)
{
    nptr<Critter> cr_lookup = cr;

    if (!cr_lookup) {
        return;
    }

    auto cr_ptr = cr_lookup.as_ptr();
    if (!cr_ptr->GetControlledByPlayer()) {
        server->CrMngr.DestroyCritter(cr_ptr);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyCritter(ptr<ServerEngine> server, ident_t crId)
{
    if (crId) {
        auto nullable_cr = server->EntityMngr.GetCritter(crId);

        if (nullable_cr) {
            auto cr = nullable_cr.as_ptr();

            if (!cr->GetControlledByPlayer()) {
                server->CrMngr.DestroyCritter(cr);
            }
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyCritters(ptr<ServerEngine> server, readonly_vector<Critter*> critters)
{
    for (nptr<Critter> nullable_cr : critters) {
        if (nullable_cr) {
            auto cr = nullable_cr.as_ptr();

            if (!cr->GetControlledByPlayer()) {
                server->CrMngr.DestroyCritter(cr);
            }
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyCritters(ptr<ServerEngine> server, readonly_vector<ident_t> critterIds)
{
    for (const ident_t id : critterIds) {
        if (id) {
            auto nullable_cr = server->EntityMngr.GetCritter(id);

            if (nullable_cr) {
                auto cr = nullable_cr.as_ptr();

                if (!cr->GetControlledByPlayer()) {
                    server->CrMngr.DestroyCritter(cr);
                }
            }
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API ptr<Location> Server_Game_CreateLocation(ptr<ServerEngine> server, hstring protoId)
{
    auto loc = server->MapMngr.CreateLocation(protoId);
    return loc;
}

///@ ExportMethod
FO_SCRIPT_API ptr<Location> Server_Game_CreateLocation(ptr<ServerEngine> server, ptr<ProtoLocation> proto)
{
    auto loc = server->MapMngr.CreateLocation(proto->GetProtoId());
    return loc;
}

///@ ExportMethod
FO_SCRIPT_API ptr<Location> Server_Game_CreateLocation(ptr<ServerEngine> server, hstring protoId, readonly_vector<hstring> map_pids)
{
    auto loc = server->MapMngr.CreateLocation(protoId, map_pids);
    return loc;
}

///@ ExportMethod
FO_SCRIPT_API ptr<Location> Server_Game_CreateLocation(ptr<ServerEngine> server, hstring protoId, readonly_map<LocationProperty, any_t> props)
{
    auto nullable_proto = server->GetProtoLocation(protoId);

    if (!nullable_proto) {
        throw ScriptException("Invalid location proto id arg", protoId);
    }

    auto proto = nullable_proto.as_ptr();
    Properties props_ = proto->GetProperties().Copy();

    for (const auto& [key, value] : props) {
        props_.SetValueAsAnyProps(static_cast<int32_t>(key), value);
    }

    nptr<const Properties> props_ptr = &props_;
    auto loc = server->MapMngr.CreateLocation(protoId, {}, props_ptr);
    return loc;
}

///@ ExportMethod
FO_SCRIPT_API ptr<Location> Server_Game_CreateLocation(ptr<ServerEngine> server, ptr<ProtoLocation> proto, readonly_map<LocationProperty, any_t> props)
{
    Properties props_ = proto->GetProperties().Copy();

    for (const auto& [key, value] : props) {
        props_.SetValueAsAnyProps(static_cast<int32_t>(key), value);
    }

    nptr<const Properties> props_ptr = &props_;
    auto loc = server->MapMngr.CreateLocation(proto->GetProtoId(), {}, props_ptr);
    return loc;
}

///@ ExportMethod
FO_SCRIPT_API ptr<Location> Server_Game_CreateLocation(ptr<ServerEngine> server, hstring protoId, readonly_vector<hstring> map_pids, readonly_map<LocationProperty, any_t> props)
{
    auto nullable_proto = server->GetProtoLocation(protoId);

    if (!nullable_proto) {
        throw ScriptException("Invalid location proto id arg", protoId);
    }

    auto proto = nullable_proto.as_ptr();
    Properties props_ = proto->GetProperties().Copy();

    for (const auto& [key, value] : props) {
        props_.SetValueAsAnyProps(static_cast<int32_t>(key), value);
    }

    nptr<const Properties> props_ptr = &props_;
    auto loc = server->MapMngr.CreateLocation(protoId, map_pids, props_ptr);
    return loc;
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyLocation(ptr<ServerEngine> server, nptr<Location> loc)
{
    if (loc) {
        server->MapMngr.DestroyLocation(loc.as_ptr());
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyLocation(ptr<ServerEngine> server, ident_t locId)
{
    auto loc = server->EntityMngr.GetLocation(locId);

    if (loc) {
        server->MapMngr.DestroyLocation(loc.as_ptr());
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyMap(ptr<ServerEngine> server, nptr<Map> map)
{
    if (map) {
        server->MapMngr.DestroyMap(map.as_ptr());
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyMap(ptr<ServerEngine> server, ident_t mapId)
{
    auto map = server->EntityMngr.GetMap(mapId);

    if (map) {
        server->MapMngr.DestroyMap(map.as_ptr());
    }
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API nptr<Critter> Server_Game_GetCritter(ptr<ServerEngine> server, ident_t crId)
{
    if (!crId) {
        return nullptr;
    }

    auto cr = server->EntityMngr.GetCritter(crId);
    return ReleaseNullableScriptOwnership(std::move(cr));
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API nptr<ServerEntity> Server_Game_GetEntity(ptr<ServerEngine> server, ident_t entityId)
{
    if (!entityId) {
        return nullptr;
    }

    auto entity = server->EntityMngr.GetEntity(entityId);
    return ReleaseNullableScriptOwnership(std::move(entity));
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API vector<Critter*> Server_Game_GetCritters(ptr<ServerEngine> server, CritterFindType findType)
{
    vector<refcount_ptr<Critter>> critters = server->EntityMngr.GetCritters();
    vector<refcount_ptr<Critter>> result;
    result.reserve(critters.size());

    for (size_t i = 0; i != critters.size(); i++) {
        if (critters[i]->CheckFind(findType)) {
            result.emplace_back(std::move(critters[i]));
        }
    }

    return ReleaseScriptOwnershipVector(std::move(result));
}

///@ ExportMethod
FO_SCRIPT_API ptr<Player> Server_Game_CreateUnloginedPlayer(ptr<ServerEngine> server)
{
    shared_ptr<NetworkServerConnection> dummy_net_conn = NetworkServer::CreateDummyConnection(server->Settings, NetworkServer::DummyConnectionState::Connected);
    auto player = server->CreateUnloginedPlayer(std::move(dummy_net_conn));
    return player;
}

///@ ExportMethod
FO_SCRIPT_API ptr<Player> Server_Game_LoginPlayerToNewRecord(ptr<ServerEngine> server, ptr<Player> unloginedPlayer)
{
    if (unloginedPlayer->GetLogined()) {
        throw ScriptException("Player is already logined");
    }

    auto player = server->LoginPlayerToNewRecord(unloginedPlayer);
    return player.as_ptr();
}

///@ ExportMethod
FO_SCRIPT_API ptr<Player> Server_Game_LoginPlayerToTempSession(ptr<ServerEngine> server, ptr<Player> unloginedPlayer)
{
    if (unloginedPlayer->GetLogined()) {
        throw ScriptException("Player is already logined");
    }

    auto player = server->LoginPlayerToTempSession(unloginedPlayer);
    return player.as_ptr();
}

///@ ExportMethod
FO_SCRIPT_API ptr<Player> Server_Game_LoginPlayerToExistentRecord(ptr<ServerEngine> server, ptr<Player> unloginedPlayer, ident_t playerId)
{
    if (unloginedPlayer->GetLogined()) {
        throw ScriptException("Player is already logined");
    }
    if (!playerId) {
        throw ScriptException("Player id arg is zero");
    }

    auto player = server->LoginPlayerToExistentRecord(unloginedPlayer, playerId);
    return player.as_ptr();
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API nptr<Player> Server_Game_GetPlayer(ptr<ServerEngine> server, ident_t playerId)
{
    if (!playerId) {
        return nullptr;
    }

    auto player = server->EntityMngr.GetPlayer(playerId);
    return ReleaseNullableScriptOwnership(std::move(player));
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API nptr<Map> Server_Game_GetMap(ptr<ServerEngine> server, ident_t mapId)
{
    auto map = server->EntityMngr.GetMap(mapId);
    return ReleaseNullableScriptOwnership(std::move(map));
}

///@ ExportMethod
FO_SCRIPT_API nptr<Map> Server_Game_GetMap(ptr<ServerEngine> server, hstring mapPid, int32_t skipCount = 0)
{
    auto map = server->MapMngr.GetMapByPid(mapPid, skipCount);
    return ReleaseNullableScriptOwnership(std::move(map));
}

///@ ExportMethod
FO_SCRIPT_API nptr<Map> Server_Game_GetMap(ptr<ServerEngine> server, ptr<ProtoMap> mapProto, int32_t skipCount = 0)
{
    ptr<const ProtoMap> map_proto = mapProto;
    auto map = server->MapMngr.GetMapByPid(map_proto->GetProtoId(), skipCount);
    return ReleaseNullableScriptOwnership(std::move(map));
}

///@ ExportMethod
FO_SCRIPT_API vector<Map*> Server_Game_GetMaps(ptr<ServerEngine> server)
{
    vector<refcount_ptr<Map>> maps = server->EntityMngr.GetMaps();
    return MakeScriptRefHandleVectorAs<Map, Map>(maps);
}

///@ ExportMethod
FO_SCRIPT_API vector<Map*> Server_Game_GetMaps(ptr<ServerEngine> server, hstring pid)
{
    vector<refcount_ptr<Map>> maps = server->EntityMngr.GetMaps();
    vector<ptr<Map>> result;

    if (!pid) {
        result.reserve(maps.size());
    }

    for (size_t i = 0; i != maps.size(); i++) {
        auto map = maps[i].as_ptr();

        if (!pid || pid == map->GetProtoId()) {
            result.emplace_back(map);
        }
    }

    return MakeScriptHandleVector<Map>(result);
}

///@ ExportMethod
FO_SCRIPT_API vector<Map*> Server_Game_GetMaps(ptr<ServerEngine> server, nptr<ProtoMap> proto)
{
    nptr<const ProtoMap> proto_lookup = proto;
    vector<refcount_ptr<Map>> maps = server->EntityMngr.GetMaps();
    vector<ptr<Map>> result;

    if (!proto_lookup) {
        result.reserve(maps.size());

        for (size_t i = 0; i != maps.size(); i++) {
            result.emplace_back(maps[i]);
        }

        return MakeScriptHandleVector<Map>(result);
    }

    auto proto_ptr = proto_lookup.as_ptr();

    for (size_t i = 0; i != maps.size(); i++) {
        auto map = maps[i].as_ptr();

        if (proto_ptr->GetProtoId() == map->GetProtoId()) {
            result.emplace_back(map);
        }
    }

    return MakeScriptHandleVector<Map>(result);
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API nptr<Location> Server_Game_GetLocation(ptr<ServerEngine> server, ident_t locId)
{
    auto loc = server->EntityMngr.GetLocation(locId);
    return ReleaseNullableScriptOwnership(std::move(loc));
}

///@ ExportMethod
FO_SCRIPT_API nptr<Location> Server_Game_GetLocation(ptr<ServerEngine> server, hstring locPid, int32_t skipCount = 0)
{
    auto loc = server->MapMngr.GetLocationByPid(locPid, skipCount);
    return ReleaseNullableScriptOwnership(std::move(loc));
}

///@ ExportMethod
FO_SCRIPT_API nptr<Location> Server_Game_GetLocation(ptr<ServerEngine> server, ptr<ProtoLocation> locProto, int32_t skipCount = 0)
{
    ptr<const ProtoLocation> loc_proto = locProto;
    auto loc = server->MapMngr.GetLocationByPid(loc_proto->GetProtoId(), skipCount);
    return ReleaseNullableScriptOwnership(std::move(loc));
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API nptr<Location> Server_Game_GetLocation(ptr<ServerEngine> server, LocationProperty property, int32_t propertyValue)
{
    ptr<const Property> prop = ScriptHelpers::GetIntConvertibleEntityProperty<Location>(server, property);
    vector<refcount_ptr<Location>> locs = server->EntityMngr.GetLocations();

    for (size_t i = 0; i != locs.size(); i++) {
        if (locs[i]->GetValueAsInt(prop) == propertyValue) {
            return ReleaseScriptOwnership(std::move(locs[i]));
        }
    }

    return nullptr;
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API vector<Location*> Server_Game_GetLocations(ptr<ServerEngine> server)
{
    vector<refcount_ptr<Location>> locs = server->EntityMngr.GetLocations();
    vector<refcount_ptr<Location>> result;
    result.reserve(locs.size());

    for (size_t i = 0; i != locs.size(); i++) {
        result.emplace_back(std::move(locs[i]));
    }

    return ReleaseScriptOwnershipVector(std::move(result));
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API vector<Location*> Server_Game_GetLocations(ptr<ServerEngine> server, hstring pid)
{
    vector<refcount_ptr<Location>> locs = server->EntityMngr.GetLocations();
    vector<refcount_ptr<Location>> result;

    if (!pid) {
        result.reserve(locs.size());
    }

    for (size_t i = 0; i != locs.size(); i++) {
        if (!pid || pid == locs[i]->GetProtoId()) {
            result.emplace_back(std::move(locs[i]));
        }
    }

    return ReleaseScriptOwnershipVector(std::move(result));
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API vector<Location*> Server_Game_GetLocations(ptr<ServerEngine> server, nptr<ProtoLocation> proto)
{
    nptr<const ProtoLocation> proto_lookup = proto;
    vector<refcount_ptr<Location>> locs = server->EntityMngr.GetLocations();
    vector<refcount_ptr<Location>> result;

    if (!proto_lookup) {
        result.reserve(locs.size());

        for (size_t i = 0; i != locs.size(); i++) {
            result.emplace_back(std::move(locs[i]));
        }

        return ReleaseScriptOwnershipVector(std::move(result));
    }

    auto proto_ptr = proto_lookup.as_ptr();

    for (size_t i = 0; i != locs.size(); i++) {
        if (proto_ptr->GetProtoId() == locs[i]->GetProtoId()) {
            result.emplace_back(std::move(locs[i]));
        }
    }

    return ReleaseScriptOwnershipVector(std::move(result));
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API vector<Location*> Server_Game_GetLocations(ptr<ServerEngine> server, LocationProperty property, int32_t propertyValue)
{
    ptr<const Property> prop = ScriptHelpers::GetIntConvertibleEntityProperty<Location>(server, property);
    vector<refcount_ptr<Location>> locs = server->EntityMngr.GetLocations();
    vector<refcount_ptr<Location>> result;
    result.reserve(locs.size());

    for (size_t i = 0; i != locs.size(); i++) {
        if (locs[i]->GetValueAsInt(prop) == propertyValue) {
            result.emplace_back(std::move(locs[i]));
        }
    }

    return ReleaseScriptOwnershipVector(std::move(result));
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API vector<Item*> Server_Game_GetAllItems(ptr<ServerEngine> server, hstring pid)
{
    vector<refcount_ptr<Item>> items = server->EntityMngr.GetItems();
    vector<refcount_ptr<Item>> result;

    if (!pid) {
        result.reserve(items.size());
    }

    for (size_t i = 0; i != items.size(); i++) {
        if (!pid || pid == items[i]->GetProtoId()) {
            result.emplace_back(std::move(items[i]));
        }
    }

    return ReleaseScriptOwnershipVector(std::move(result));
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API vector<Item*> Server_Game_GetAllItems(ptr<ServerEngine> server, nptr<ProtoItem> proto)
{
    nptr<const ProtoItem> proto_lookup = proto;
    vector<refcount_ptr<Item>> items = server->EntityMngr.GetItems();
    vector<refcount_ptr<Item>> result;

    if (!proto_lookup) {
        result.reserve(items.size());

        for (size_t i = 0; i != items.size(); i++) {
            result.emplace_back(std::move(items[i]));
        }

        return ReleaseScriptOwnershipVector(std::move(result));
    }

    auto proto_ptr = proto_lookup.as_ptr();

    for (size_t i = 0; i != items.size(); i++) {
        if (proto_ptr->GetProtoId() == items[i]->GetProtoId()) {
            result.emplace_back(std::move(items[i]));
        }
    }

    return ReleaseScriptOwnershipVector(std::move(result));
}

///@ ExportMethod PassOwnership
FO_SCRIPT_API vector<Player*> Server_Game_GetOnlinePlayers(ptr<ServerEngine> server)
{
    vector<refcount_ptr<Player>> players = server->EntityMngr.GetPlayers();
    vector<refcount_ptr<Player>> result;
    result.reserve(players.size());

    for (size_t i = 0; i != players.size(); i++) {
        result.emplace_back(std::move(players[i]));
    }

    return ReleaseScriptOwnershipVector(std::move(result));
}

///@ ExportMethod
FO_SCRIPT_API vector<ident_t> Server_Game_GetRegisteredPlayerIds(ptr<ServerEngine> server)
{
    return server->DbStorage.GetAllIntIds(server->PlayersCollectionName);
}

///@ ExportMethod
FO_SCRIPT_API vector<ident_t> Server_Game_DbGetAllRecordIds(ptr<ServerEngine> server, hstring collectionName)
{
    if (!collectionName) {
        throw ScriptException("Collection name arg is empty");
    }

    return server->DbStorage.GetAllIntIds(collectionName);
}

///@ ExportMethod
FO_SCRIPT_API vector<string> Server_Game_DbGetAllRecordKeys(ptr<ServerEngine> server, hstring collectionName)
{
    if (!collectionName) {
        throw ScriptException("Collection name arg is empty");
    }

    return server->DbStorage.GetAllStringIds(collectionName);
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Game_DbHasEntity(ptr<ServerEngine> server, ptr<ServerEntity> entity)
{
    return server->DbStorage.Valid(entity->GetTypeNamePlural(), entity->GetId());
}

///@ ExportMethod
FO_SCRIPT_API map<string, string> Server_Game_DbGetPlayerData(ptr<ServerEngine> server, ident_t playerId)
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
FO_SCRIPT_API bool Server_Game_DbHasRecord(ptr<ServerEngine> server, hstring collectionName, ident_t id)
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
FO_SCRIPT_API bool Server_Game_DbHasRecord(ptr<ServerEngine> server, hstring collectionName, string_view id)
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
FO_SCRIPT_API map<string, string> Server_Game_DbGetRecord(ptr<ServerEngine> server, hstring collectionName, ident_t id)
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
FO_SCRIPT_API map<string, string> Server_Game_DbGetRecord(ptr<ServerEngine> server, hstring collectionName, string_view id)
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
FO_SCRIPT_API void Server_Game_DbInsertRecord(ptr<ServerEngine> server, hstring collectionName, ident_t id, readonly_map<string, string> keyValues)
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
FO_SCRIPT_API void Server_Game_DbInsertRecord(ptr<ServerEngine> server, hstring collectionName, string_view id, readonly_map<string, string> keyValues)
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
    static void ValidateAndUpdateRecord(ptr<ServerEngine> server_ptr, hstring collectionName, const auto& id, string_view key, const T& value)
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

        if (!server_ptr->DbStorage.Valid(collectionName, id)) {
            throw ScriptException("Record not found");
        }

        if constexpr (std::is_same_v<T, string_view>) {
            server_ptr->DbStorage.Update(collectionName, id, key, string(value));
        }
        else {
            server_ptr->DbStorage.Update(collectionName, id, key, value);
        }
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DbUpdateRecordString(ptr<ServerEngine> server, hstring collectionName, ident_t id, string_view key, string_view value)
{
    ValidateAndUpdateRecord(server, collectionName, id, key, value);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DbUpdateRecordString(ptr<ServerEngine> server, hstring collectionName, string_view id, string_view key, string_view value)
{
    ValidateAndUpdateRecord(server, collectionName, string(id), key, value);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DbUpdateRecordInt64(ptr<ServerEngine> server, hstring collectionName, ident_t id, string_view key, int64_t value)
{
    ValidateAndUpdateRecord(server, collectionName, id, key, value);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DbUpdateRecordInt64(ptr<ServerEngine> server, hstring collectionName, string_view id, string_view key, int64_t value)
{
    ValidateAndUpdateRecord(server, collectionName, string(id), key, value);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DbUpdateRecordFloat64(ptr<ServerEngine> server, hstring collectionName, ident_t id, string_view key, float64_t value)
{
    ValidateAndUpdateRecord(server, collectionName, id, key, value);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DbUpdateRecordFloat64(ptr<ServerEngine> server, hstring collectionName, string_view id, string_view key, float64_t value)
{
    ValidateAndUpdateRecord(server, collectionName, string(id), key, value);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DbUpdateRecordBool(ptr<ServerEngine> server, hstring collectionName, ident_t id, string_view key, bool value)
{
    ValidateAndUpdateRecord(server, collectionName, id, key, value);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DbUpdateRecordBool(ptr<ServerEngine> server, hstring collectionName, string_view id, string_view key, bool value)
{
    ValidateAndUpdateRecord(server, collectionName, string(id), key, value);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DbRemoveRecord(ptr<ServerEngine> server, hstring collectionName, ident_t id)
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
FO_SCRIPT_API void Server_Game_DbRemoveRecord(ptr<ServerEngine> server, hstring collectionName, string_view id)
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
FO_SCRIPT_API vector<Critter*> Server_Game_GetAllNpc(ptr<ServerEngine> server)
{
    vector<refcount_ptr<Critter>> npcs = server->CrMngr.GetNonPlayerCritters();
    return MakeScriptRefHandleVectorAs<Critter, Critter>(npcs);
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Game_GetAllNpc(ptr<ServerEngine> server, hstring pid)
{
    vector<refcount_ptr<Critter>> npcs = server->CrMngr.GetNonPlayerCritters();
    vector<ptr<Critter>> result;
    result.reserve(npcs.size());

    for (size_t i = 0; i != npcs.size(); i++) {
        auto cr = npcs[i].as_ptr();

        if (!pid || pid == cr->GetProtoId()) {
            result.emplace_back(cr);
        }
    }

    return MakeScriptHandleVector<Critter>(result);
}

///@ ExportMethod
FO_SCRIPT_API vector<Critter*> Server_Game_GetAllNpc(ptr<ServerEngine> server, ptr<ProtoCritter> proto)
{
    vector<ptr<Critter>> result;
    vector<refcount_ptr<Critter>> npcs = server->CrMngr.GetNonPlayerCritters();
    result.reserve(npcs.size());

    for (size_t i = 0; i != npcs.size(); i++) {
        auto cr = npcs[i].as_ptr();

        if (proto->GetProtoId() == cr->GetProtoId()) {
            result.emplace_back(cr);
        }
    }

    return MakeScriptHandleVector<Critter>(result);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_SetSynchronizedTime(ptr<ServerEngine> server, synctime time)
{
    server->GameTime.SetSynchronizedTime(time);
    server->SetSynchronizedTime(time);
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Game_CallStaticItemFunction(ptr<ServerEngine> server, nptr<Critter> cr, ptr<StaticItem> staticItem, nptr<Item> usedItem, any_t param)
{
    ignore_unused(server);

    nptr<Item> used_item = usedItem;

    if (!staticItem->StaticScriptFunc) {
        return false;
    }

    return staticItem->StaticScriptFunc.Call(cr.get(), staticItem.get(), used_item.get(), param) && staticItem->StaticScriptFunc.GetResult();
}

///@ ExportMethod
FO_SCRIPT_API vector<StaticItem*> Server_Game_GetStaticItemsForProtoMap(ptr<ServerEngine> server, ptr<ProtoMap> proto)
{
    auto static_map = server->MapMngr.GetStaticMap(proto);
    const vector<ptr<StaticItem>>& static_items = static_map->StaticItems;
    return MakeScriptHandleVector<StaticItem>(static_items);
}

///@ ExportMethod
FO_SCRIPT_API vector<ProtoCritter*> Server_Game_GetProtoCrittersForProtoMap(ptr<ServerEngine> server, ptr<ProtoMap> proto)
{
    auto static_map = server->MapMngr.GetStaticMap(proto);
    vector<ptr<const ProtoCritter>> proto_critters;
    proto_critters.reserve(static_map->CritterBillets.size());

    for (const pair<ident_t, refcount_ptr<Critter>>& billet : static_map->CritterBillets) {
        auto proto_cr = billet.second->GetProto().dyn_cast<const ProtoCritter>();
        FO_VERIFY_AND_THROW(proto_cr, "Missing required prototype critter");
        proto_critters.emplace_back(proto_cr.as_ptr());
    }

    return MakeMutableScriptHandleVector<ProtoCritter>(proto_critters);
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Game_IsTextPresent(ptr<ServerEngine> server, TextPackKey textKey)
{
    return server->GetLangPack().IsTextPresent(textKey);
}

///@ ExportMethod
FO_SCRIPT_API int32_t Server_Game_GetTextCount(ptr<ServerEngine> server, TextPackKey textKey)
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
    const string command_str = string(command);
    ptr<const char> command_cstr = command_str.c_str();
    nptr<FILE> in = popen(command_cstr.get(), "r");

    if (!in) {
        return -1;
    }

    string log;
    char buf[4096];

    while (fgets(buf, sizeof(buf), in.get())) {
        log += buf;
        print_log(log, false);
    }

    print_log(log, true);

    return pclose(in.get());

#else
    return 1;
#endif
}

///@ ExportMethod
FO_SCRIPT_API int32_t Server_Game_SystemCall(ptr<ServerEngine> server, string_view command)
{
    ignore_unused(server);

    auto prefix = command.substr(0, command.find(' '));
    return SystemCall(command, [&prefix](string_view line) { WriteLog("{} : {}\n", prefix, line); });
}

///@ ExportMethod
FO_SCRIPT_API int32_t Server_Game_SystemCall(ptr<ServerEngine> server, string_view command, string& output)
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
FO_SCRIPT_API void Server_Game_Sync(ptr<ServerEngine> server, ptr<ServerEntity> entity)
{
    auto ctx = RequireCurrentSyncContext(server);
    const array<nptr<ServerEntity>, 1> entities {entity};
    ctx->SyncEntities(entities);
}

///@ ExportMethod Async
FO_SCRIPT_API void Server_Game_Sync(ptr<ServerEngine> server, ptr<ServerEntity> entity1, ptr<ServerEntity> entity2)
{
    auto ctx = RequireCurrentSyncContext(server);
    const array<nptr<ServerEntity>, 2> entities {entity1, entity2};
    ctx->SyncEntities(entities);
}

///@ ExportMethod Async
FO_SCRIPT_API void Server_Game_Sync(ptr<ServerEngine> server, ptr<ServerEntity> entity1, ptr<ServerEntity> entity2, ptr<ServerEntity> entity3)
{
    auto ctx = RequireCurrentSyncContext(server);
    const array<nptr<ServerEntity>, 3> entities {entity1, entity2, entity3};
    ctx->SyncEntities(entities);
}

///@ ExportMethod Async
FO_SCRIPT_API void Server_Game_Sync(ptr<ServerEngine> server, readonly_vector<ServerEntity*> entities)
{
    vector<nptr<ServerEntity>> non_null;
    non_null.reserve(entities.size());

    for (nptr<ServerEntity> entity : entities) {
        if (!entity) {
            throw ScriptException("Entity in array arg is null");
        }

        non_null.emplace_back(entity);
    }

    auto ctx = RequireCurrentSyncContext(server);
    ctx->SyncEntities(non_null);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_SyncEnsure(ptr<ServerEngine> server, ptr<ServerEntity> entity)
{
    auto ctx = RequireCurrentSyncContext(server);
    ctx->EnsureEntitySynced(entity);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_SyncRelease(ptr<ServerEngine> server)
{
    auto ctx = RequireCurrentSyncContext(server);
    ctx->Release();
}

///@ ExportMethod
FO_SCRIPT_API vector<ServerEntity*> Server_Game_GetHeldSyncEntities(ptr<ServerEngine> server)
{
    auto ctx = RequireCurrentSyncContext(server);
    vector<ptr<ServerEntity>> held_entities = ctx->GetHeldEntities();
    return MakeScriptHandleVector<ServerEntity>(held_entities);
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Game_IsEntityLocked(ptr<ServerEngine> server, nptr<ServerEntity> entity)
{
    auto ctx = RequireCurrentSyncContext(server);
    ignore_unused(ctx);
    return IsEntityAccessValid(entity, false);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_Lock(ptr<ServerEngine> server)
{
    auto ctx = RequireCurrentSyncContext(server);
    ctx->LockSingleton(server->GetEntityLock().get());
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_Unlock(ptr<ServerEngine> server)
{
    auto ctx = RequireCurrentSyncContext(server);
    ctx->UnlockSingleton(server->GetEntityLock().get());
}

///@ ExportMethod
FO_SCRIPT_API int64_t Server_Game_GetProcessMemoryUsage(ptr<ServerEngine> server)
{
    ignore_unused(server);

    return static_cast<int64_t>(Platform::GetProcessMemoryUsage());
}

///@ ExportMethod
FO_SCRIPT_API int64_t Server_Game_GetAllocatorMemoryUsage(ptr<ServerEngine> server)
{
    ignore_unused(server);

    return static_cast<int64_t>(AllocatorGetInUseBytes());
}

///@ ExportMethod
FO_SCRIPT_API int32_t Server_Game_GetEntityRegistryCount(ptr<ServerEngine> server)
{
    return static_cast<int32_t>(server->EntityMngr.GetEntitiesCount());
}

///@ ExportMethod
FO_SCRIPT_API int32_t Server_Game_GetPlayerRegistryCount(ptr<ServerEngine> server)
{
    return static_cast<int32_t>(server->EntityMngr.GetPlayersCount());
}

///@ ExportMethod
FO_SCRIPT_API int32_t Server_Game_GetLocationRegistryCount(ptr<ServerEngine> server)
{
    return static_cast<int32_t>(server->EntityMngr.GetLocationsCount());
}

///@ ExportMethod
FO_SCRIPT_API int32_t Server_Game_GetMapRegistryCount(ptr<ServerEngine> server)
{
    return static_cast<int32_t>(server->EntityMngr.GetMapsCount());
}

///@ ExportMethod
FO_SCRIPT_API int32_t Server_Game_GetCritterRegistryCount(ptr<ServerEngine> server)
{
    return static_cast<int32_t>(server->EntityMngr.GetCrittersCount());
}

///@ ExportMethod
FO_SCRIPT_API int32_t Server_Game_GetItemRegistryCount(ptr<ServerEngine> server)
{
    return static_cast<int32_t>(server->EntityMngr.GetItemsCount());
}

FO_END_NAMESPACE
