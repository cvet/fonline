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
#include "Geometry.h"
#include "NetworkServer.h"
#include "PropertiesSerializator.h"
#include "ScriptSystem.h"
#include "Server.h"
#include "WinApi-Include.h"

FO_BEGIN_NAMESPACE();

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
FO_SCRIPT_API int32 Server_Game_GetDistance(FOServer* server, Critter* cr1, Critter* cr2)
{
    ignore_unused(server);

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

    const auto dist = GeometryHelper::GetDistance(cr1->GetHex(), cr2->GetHex());
    const auto multihex = cr1->GetMultihex() + cr2->GetMultihex();
    return multihex < dist ? dist - multihex : 0;
}

///@ ExportMethod
FO_SCRIPT_API int32 Server_Game_GetDistance(FOServer* server, Item* item1, Item* item2)
{
    ignore_unused(server);

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

    const auto dist = GeometryHelper::GetDistance(item1->GetHex(), item2->GetHex());
    return dist;
}

///@ ExportMethod
FO_SCRIPT_API int32 Server_Game_GetDistance(FOServer* server, Critter* cr, Item* item)
{
    ignore_unused(server);

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

    const auto dist = GeometryHelper::GetDistance(cr->GetHex(), item->GetHex());
    const auto multihex = cr->GetMultihex();
    return multihex < dist ? dist - multihex : 0;
}

///@ ExportMethod
FO_SCRIPT_API int32 Server_Game_GetDistance(FOServer* server, Item* item, Critter* cr)
{
    ignore_unused(server);

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

    const auto dist = GeometryHelper::GetDistance(cr->GetHex(), item->GetHex());
    const auto multihex = cr->GetMultihex();
    return multihex < dist ? dist - multihex : 0;
}

///@ ExportMethod
FO_SCRIPT_API int32 Server_Game_GetDistance(FOServer* server, Critter* cr, mpos hex)
{
    ignore_unused(server);

    if (cr == nullptr) {
        throw ScriptException("Critter arg is null");
    }
    if (!cr->GetMapId()) {
        throw ScriptException("Critter not on map");
    }

    const auto dist = GeometryHelper::GetDistance(cr->GetHex(), hex);
    const auto multihex = cr->GetMultihex();
    return multihex < dist ? dist - multihex : 0;
}

///@ ExportMethod
FO_SCRIPT_API int32 Server_Game_GetDistance(FOServer* server, mpos hex, Critter* cr)
{
    ignore_unused(server);

    if (cr == nullptr) {
        throw ScriptException("Critter arg is null");
    }
    if (!cr->GetMapId()) {
        throw ScriptException("Critter not on map");
    }

    const auto dist = GeometryHelper::GetDistance(cr->GetHex(), hex);
    const auto multihex = cr->GetMultihex();
    return multihex < dist ? dist - multihex : 0;
}

///@ ExportMethod
FO_SCRIPT_API int32 Server_Game_GetDistance(FOServer* server, Item* item, mpos hex)
{
    ignore_unused(server);

    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }
    if (!item->GetMapId()) {
        throw ScriptException("Item not on map");
    }

    const auto dist = GeometryHelper::GetDistance(item->GetHex(), hex);
    return dist;
}

///@ ExportMethod
FO_SCRIPT_API int32 Server_Game_GetDistance(FOServer* server, mpos hex, Item* item)
{
    ignore_unused(server);

    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }
    if (!item->GetMapId()) {
        throw ScriptException("Item not on map");
    }

    const auto dist = GeometryHelper::GetDistance(item->GetHex(), hex);
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
FO_SCRIPT_API Item* Server_Game_MoveItem(FOServer* server, Item* item, int32 count, Critter* toCr)
{
    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }
    if (toCr == nullptr) {
        throw ScriptException("Critter arg is null");
    }

    if (count <= 0) {
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
    if (!toMap->GetSize().is_valid_pos(toHex)) {
        throw ScriptException("Invalid hexex args");
    }

    return server->ItemMngr.MoveItem(item, item->GetCount(), toMap, toHex);
}

///@ ExportMethod
FO_SCRIPT_API Item* Server_Game_MoveItem(FOServer* server, Item* item, int32 count, Map* toMap, mpos toHex)
{
    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }
    if (toMap == nullptr) {
        throw ScriptException("Map arg is null");
    }
    if (!toMap->GetSize().is_valid_pos(toHex)) {
        throw ScriptException("Invalid hexex args");
    }

    if (count <= 0) {
        return nullptr;
    }

    return server->ItemMngr.MoveItem(item, count, toMap, toHex);
}

///@ ExportMethod
FO_SCRIPT_API Item* Server_Game_MoveItem(FOServer* server, Item* item, Item* toCont)
{
    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }
    if (toCont == nullptr) {
        throw ScriptException("Container arg is null");
    }

    return server->ItemMngr.MoveItem(item, item->GetCount(), toCont, {});
}

///@ ExportMethod
FO_SCRIPT_API Item* Server_Game_MoveItem(FOServer* server, Item* item, Item* toCont, any_t stackId)
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
FO_SCRIPT_API Item* Server_Game_MoveItem(FOServer* server, Item* item, int32 count, Item* toCont)
{
    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }
    if (toCont == nullptr) {
        throw ScriptException("Container arg is null");
    }

    if (count <= 0) {
        return nullptr;
    }

    return server->ItemMngr.MoveItem(item, count, toCont, {});
}

///@ ExportMethod
FO_SCRIPT_API Item* Server_Game_MoveItem(FOServer* server, Item* item, int32 count, Item* toCont, any_t stackId)
{
    if (item == nullptr) {
        throw ScriptException("Item arg is null");
    }
    if (toCont == nullptr) {
        throw ScriptException("Container arg is null");
    }

    if (count <= 0) {
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
FO_SCRIPT_API void Server_Game_MoveItems(FOServer* server, const vector<Item*>& items, Item* toCont)
{
    if (toCont == nullptr) {
        throw ScriptException("Container arg is null");
    }

    for (auto* item : items) {
        if (item == nullptr || item->IsDestroyed()) {
            continue;
        }

        server->ItemMngr.MoveItem(item, item->GetCount(), toCont, {});
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_MoveItems(FOServer* server, const vector<Item*>& items, Item* toCont, any_t stackId)
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
FO_SCRIPT_API void Server_Game_DestroyItem(FOServer* server, Item* item, int32 count)
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
FO_SCRIPT_API void Server_Game_DestroyItem(FOServer* server, ident_t itemId, int32 count)
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
FO_SCRIPT_API Location* Server_Game_CreateLocation(FOServer* server, hstring protoId)
{
    auto* loc = server->MapMngr.CreateLocation(protoId);
    FO_RUNTIME_ASSERT(loc);
    return loc;
}

///@ ExportMethod
FO_SCRIPT_API Location* Server_Game_CreateLocation(FOServer* server, hstring protoId, const vector<hstring>& map_pids)
{
    auto* loc = server->MapMngr.CreateLocation(protoId, map_pids);
    FO_RUNTIME_ASSERT(loc);
    return loc;
}

///@ ExportMethod
FO_SCRIPT_API Location* Server_Game_CreateLocation(FOServer* server, hstring protoId, const map<LocationProperty, any_t>& props)
{
    const auto* proto = server->ProtoMngr.GetProtoLocation(protoId);
    auto props_ = proto->GetProperties().Copy();

    for (const auto& [key, value] : props) {
        props_.SetValueAsAnyProps(static_cast<int32>(key), value);
    }

    auto* loc = server->MapMngr.CreateLocation(protoId, {}, &props_);
    FO_RUNTIME_ASSERT(loc);
    return loc;
}

///@ ExportMethod
FO_SCRIPT_API Location* Server_Game_CreateLocation(FOServer* server, hstring protoId, const vector<hstring>& map_pids, const map<LocationProperty, any_t>& props)
{
    const auto* proto = server->ProtoMngr.GetProtoLocation(protoId);
    auto props_ = proto->GetProperties().Copy();

    for (const auto& [key, value] : props) {
        props_.SetValueAsAnyProps(static_cast<int32>(key), value);
    }

    auto* loc = server->MapMngr.CreateLocation(protoId, map_pids, &props_);
    FO_RUNTIME_ASSERT(loc);
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
FO_SCRIPT_API void Server_Game_DestroyMap(FOServer* server, Map* map)
{
    if (map != nullptr) {
        server->MapMngr.DestroyMap(map);
    }
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_DestroyMap(FOServer* server, ident_t mapId)
{
    auto* map = server->EntityMngr.GetMap(mapId);

    if (map != nullptr) {
        server->MapMngr.DestroyMap(map);
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

///@ ExportMethod PassOwnership
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
    refcount_ptr<Player> player = server->EntityMngr.GetPlayer(id);

    if (player) {
        player->AddRef();
        return player.get();
    }

    // Load from db
    auto dummy_net_conn = NetworkServer::CreateDummyConnection(server->Settings);
    player = SafeAlloc::MakeRefCounted<Player>(server, id, SafeAlloc::MakeUnique<ServerConnection>(server->Settings, dummy_net_conn));
    player->SetName(name);

    if (!PropertiesSerializator::LoadFromDocument(&player->GetPropertiesForEdit(), doc, server->Hashes, *server)) {
        throw ScriptException("Player data db read failed");
    }

    player->AddRef();
    return player.get();
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
FO_SCRIPT_API Map* Server_Game_GetMap(FOServer* server, hstring mapPid, int32 skipCount)
{
    return server->MapMngr.GetMapByPid(mapPid, skipCount);
}

///@ ExportMethod
FO_SCRIPT_API vector<Map*> Server_Game_GetMaps(FOServer* server)
{
    vector<Map*> maps;
    maps.reserve(server->EntityMngr.GetLocationsCount());

    for (auto& map : server->EntityMngr.GetMaps() | std::views::values) {
        maps.emplace_back(map.get());
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

    for (auto& map : server->EntityMngr.GetMaps() | std::views::values) {
        if (!pid || pid == map->GetProtoId()) {
            maps.emplace_back(map.get());
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
FO_SCRIPT_API Location* Server_Game_GetLocation(FOServer* server, hstring locPid, int32 skipCount)
{
    return server->MapMngr.GetLocationByPid(locPid, skipCount);
}

///@ ExportMethod
FO_SCRIPT_API Location* Server_Game_GetLocation(FOServer* server, LocationComponent component)
{
    for (auto& loc : server->EntityMngr.GetLocations() | std::views::values) {
        if (loc->GetProto()->HasComponent(static_cast<hstring::hash_t>(component))) {
            return loc.get();
        }
    }

    return nullptr;
}

///@ ExportMethod
FO_SCRIPT_API Location* Server_Game_GetLocation(FOServer* server, LocationProperty property, int32 propertyValue)
{
    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<Location>(server, property);

    for (auto& loc : server->EntityMngr.GetLocations() | std::views::values) {
        if (loc->GetValueAsInt(prop) == propertyValue) {
            return loc.get();
        }
    }

    return nullptr;
}

///@ ExportMethod
FO_SCRIPT_API vector<Location*> Server_Game_GetLocations(FOServer* server)
{
    auto& locs = server->EntityMngr.GetLocations();

    vector<Location*> result;
    result.reserve(locs.size());

    for (auto& loc : locs | std::views::values) {
        result.emplace_back(loc.get());
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<Location*> Server_Game_GetLocations(FOServer* server, hstring pid)
{
    auto& locs = server->EntityMngr.GetLocations();
    vector<Location*> result;

    if (!pid) {
        result.reserve(locs.size());
    }

    for (auto& loc : locs | std::views::values) {
        if (!pid || pid == loc->GetProtoId()) {
            result.emplace_back(loc.get());
        }
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<Location*> Server_Game_GetLocations(FOServer* server, LocationComponent component)
{
    auto& locs = server->EntityMngr.GetLocations();

    vector<Location*> result;
    result.reserve(locs.size());

    for (auto& loc : locs | std::views::values) {
        if (loc->GetProto()->HasComponent(static_cast<hstring::hash_t>(component))) {
            result.emplace_back(loc.get());
        }
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<Location*> Server_Game_GetLocations(FOServer* server, LocationProperty property, int32 propertyValue)
{
    const auto* prop = ScriptHelpers::GetIntConvertibleEntityProperty<Location>(server, property);
    auto& locs = server->EntityMngr.GetLocations();

    vector<Location*> result;
    result.reserve(locs.size());

    for (auto& loc : locs | std::views::values) {
        if (loc->GetValueAsInt(prop) == propertyValue) {
            result.emplace_back(loc.get());
        }
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<Item*> Server_Game_GetAllItems(FOServer* server, hstring pid)
{
    auto& items = server->EntityMngr.GetItems();
    vector<Item*> result;

    if (!pid) {
        result.reserve(items.size());
    }

    for (auto& item : items | std::views::values) {
        if (!pid || pid == item->GetProtoId()) {
            result.emplace_back(item.get());
        }
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API vector<Player*> Server_Game_GetOnlinePlayers(FOServer* server)
{
    auto& players = server->EntityMngr.GetPlayers();

    vector<Player*> result;
    result.reserve(players.size());

    for (auto& player : players | std::views::values) {
        result.emplace_back(player.get());
    }

    return result;
}

///@ ExportMethod
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
    vector<Critter*> result;

    for (auto* cr : server->CrMngr.GetNonPlayerCritters()) {
        if (!cr->IsDestroyed() && (!pid || pid == cr->GetProtoId())) {
            result.emplace_back(cr);
        }
    }

    return result;
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_SetSynchronizedTime(FOServer* server, synctime time)
{
    server->GameTime.SetSynchronizedTime(time);
    server->SetSynchronizedTime(time);
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Game_CallStaticItemFunction(FOServer* server, Critter* cr, StaticItem* staticItem, Item* usedItem, any_t param)
{
    ignore_unused(server);

    if (!staticItem->StaticScriptFunc) {
        return false;
    }

    return staticItem->StaticScriptFunc(cr, staticItem, usedItem, param) && staticItem->StaticScriptFunc.GetResult();
}

///@ ExportMethod
FO_SCRIPT_API vector<StaticItem*> Server_Game_GetStaticItemsForProtoMap(FOServer* server, ProtoMap* proto)
{
    auto* static_map = server->MapMngr.GetStaticMap(proto);
    return vec_transform(static_map->StaticItems, [](auto&& item) -> StaticItem* { return item.get(); });
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Game_IsTextPresent(FOServer* server, TextPackName textPack, uint32 strNum)
{
    return server->GetLangPack().GetTextPack(textPack).GetStrCount(strNum) != 0;
}

static auto SystemCall(string_view command, const function<void(string_view)>& log_callback) -> int32
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

    return std::bit_cast<int32>(retval);

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
FO_SCRIPT_API int32 Server_Game_SystemCall(FOServer* server, string_view command)
{
    ignore_unused(server);

    auto prefix = command.substr(0, command.find(' '));
    return SystemCall(command, [&prefix](string_view line) { WriteLog("{} : {}\n", prefix, line); });
}

///@ ExportMethod
FO_SCRIPT_API int32 Server_Game_SystemCall(FOServer* server, string_view command, string& output)
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

FO_END_NAMESPACE();
