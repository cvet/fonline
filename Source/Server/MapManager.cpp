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

#include "MapManager.h"
#include "CritterManager.h"
#include "EntityManager.h"
#include "GenericUtils.h"
#include "ItemManager.h"
#include "LineTracer.h"
#include "Log.h"
#include "MapLoader.h"
#include "ProtoManager.h"
#include "Server.h"
#include "Settings.h"
#include "StringUtils.h"

MapManager::MapManager(FOServer* engine) : _engine {engine}
{
}

void MapManager::LinkMaps()
{
    WriteLog("Link maps...\n");

    // Link maps to locations
    for (auto* map : GetMaps()) {
        const auto loc_id = map->GetLocId();
        const auto loc_map_index = map->GetLocMapIndex();

        auto* loc = GetLocation(loc_id);
        if (loc == nullptr) {
            throw EntitiesLoadException("Location for map not found", loc_id, map->GetName(), map->GetId());
        }

        auto& loc_maps = loc->GetMapsRaw();
        if (loc_map_index >= static_cast<uint>(loc_maps.size())) {
            loc_maps.resize(loc_map_index + 1);
        }

        loc_maps[loc_map_index] = map;
        map->SetLocation(loc);
    }

    // Verify linkage result
    for (auto* loc : GetLocations()) {
        auto& loc_maps = loc->GetMapsRaw();
        for (size_t i = 0; i < loc_maps.size(); i++) {
            if (loc_maps[i] == nullptr) {
                throw EntitiesLoadException("Location map is empty", loc->GetName(), loc->GetId(), i);
            }
        }
    }

    WriteLog("Link maps complete.\n");
}

void MapManager::LoadStaticMaps(FileManager& file_mngr)
{
    for (const auto& [pid, proto] : _engine->ProtoMngr.GetProtoMaps()) {
        LoadStaticMap(file_mngr, proto);
    }
}

void MapManager::LoadStaticMap(FileManager& file_mngr, const ProtoMap* pmap)
{
    StaticMap static_map {};

    MapLoader::Load(
        pmap->GetName(), file_mngr, _engine->ProtoMngr, *_engine, _engine->GetPropertyRegistrator(MapProperties::ENTITY_CLASS_NAME),
        [&static_map, this](uint id, const ProtoCritter* proto, const map<string, string>& kv) -> bool {
            auto* cr = new Critter(_engine, id, nullptr, proto);
            if (!cr->LoadFromText(kv)) {
                cr->Release();
                return false;
            }

            static_map.CrittersVec.push_back(cr);
            return true;
        },
        [&static_map, this](uint id, const ProtoItem* proto, const map<string, string>& kv) -> bool {
            auto* item = new Item(_engine, id, proto);
            if (!item->LoadFromText(kv)) {
                item->Release();
                return false;
            }

            static_map.AllItemsVec.push_back(item);
            return true;
        },
        [&static_map](MapTile&& tile) { static_map.Tiles.emplace_back(std::move(tile)); });

    // Bind scripts
    auto errors = 0;

    if (pmap->GetInitScript()) {
        /*hstring func_num =
            scriptSys.BindScriptFuncNumByFuncName(pmap->GetInitScript(), "void %s(Map, bool)");
        if (!func_num)
        {
            WriteLog(
                "Map '{}', can't bind map function '{}'.\n", pmap->GetName(), pmap->GetInitScript());
            errors++;
        }*/
    }

    /*for (auto& cr : static_map.CrittersVec) {
        if (cr->GetInitScript())
        {
            string func_name = cr->GetInitScript();
            hstring func_num = scriptSys.BindScriptFuncNumByFuncName(func_name, "void %s(Critter, bool)");
            if (!func_num)
            {
                WriteLog("Map '{}', can't bind critter function '{}'.\n", pmap->GetName(), func_name);
                errors++;
            }
        }
    }*/

    for (auto* item : static_map.AllItemsVec) {
        if (!item->IsStatic() && item->GetInitScript()) {
            const auto func_name = item->GetInitScript();
            const auto func = _engine->ScriptSys->FindFunc<void, Item*, bool>(func_name);
            if (!func) {
                WriteLog("Map '{}', can't bind item function '{}'.\n", pmap->GetName(), func_name);
                errors++;
            }
        }
        else if (item->IsStatic() && item->GetInitScript()) {
            const auto func_name = item->GetInitScript();
            ScriptFunc<bool, Critter*, StaticItem*, bool, int> scenery_func;
            ScriptFunc<void, Critter*, StaticItem*, bool, uchar> trigger_func;
            if (item->GetIsTrigger() || item->GetIsTrap()) {
                trigger_func = _engine->ScriptSys->FindFunc<void, Critter*, StaticItem*, bool, uchar>(func_name);
            }
            else {
                scenery_func = _engine->ScriptSys->FindFunc<bool, Critter*, StaticItem*, bool, int>(func_name);
            }

            if (!scenery_func && !trigger_func) {
                WriteLog("Map '{}', can't bind static item function '{}'.\n", pmap->GetName(), func_name);
                errors++;
            }

            item->SceneryScriptFunc = scenery_func;
            item->TriggerScriptFunc = trigger_func;
        }
    }

    if (errors != 0) {
        throw MapManagerException("Load map failed");
    }

    // Parse objects
    auto maxhx = pmap->GetWidth();
    auto maxhy = pmap->GetHeight();
    static_map.HexFlags = new uchar[maxhx * maxhy];
    std::memset(static_map.HexFlags, 0, maxhx * maxhy);

    uint scenery_count = 0;
    vector<uchar> scenery_data;
    for (auto* item : static_map.AllItemsVec) {
        if (!item->IsStatic()) {
            item->AddRef();
            if (item->GetOwnership() == ItemOwnership::MapHex) {
                static_map.HexItemsVec.push_back(item);
            }
            else {
                static_map.ChildItemsVec.push_back(item);
            }
            continue;
        }

        RUNTIME_ASSERT(item->GetOwnership() == ItemOwnership::MapHex);

        auto hx = item->GetHexX();
        auto hy = item->GetHexY();
        if (hx >= maxhx || hy >= maxhy) {
            WriteLog("Invalid item '{}' position on map '{}', hex x {}, hex y {}.\n", item->GetName(), pmap->GetName(), hx, hy);
            continue;
        }

        if (!item->GetIsHiddenInStatic()) {
            item->AddRef();
            static_map.StaticItemsVec.push_back(item);
        }

        if (!item->GetIsNoBlock()) {
            SetBit(static_map.HexFlags[hy * maxhx + hx], FH_BLOCK);
        }

        if (!item->GetIsShootThru()) {
            SetBit(static_map.HexFlags[hy * maxhx + hx], FH_BLOCK);
            SetBit(static_map.HexFlags[hy * maxhx + hx], FH_NOTRAKE);
        }

        // Block around scroll blocks
        if (item->GetIsScrollBlock()) {
            for (uchar k = 0; k < 6; k++) {
                auto hx_ = hx;
                auto hy_ = hy;
                if (_engine->GeomHelper.MoveHexByDir(hx_, hy_, k, maxhx, maxhy)) {
                    SetBit(static_map.HexFlags[hy_ * maxhx + hx_], FH_BLOCK);
                }
            }
        }

        if (item->GetIsTrigger() || item->GetIsTrap()) {
            item->AddRef();
            static_map.TriggerItemsVec.push_back(item);

            SetBit(static_map.HexFlags[hy * maxhx + hx], FH_STATIC_TRIGGER);
        }

        if (item->IsNonEmptyBlockLines()) {
            auto raked = item->GetIsShootThru();
            _engine->GeomHelper.ForEachBlockLines(item->GetBlockLines(), hx, hy, maxhx, maxhy, [&static_map, maxhx, raked](auto hx2, auto hy2) {
                SetBit(static_map.HexFlags[hy2 * maxhx + hx2], FH_BLOCK);
                if (!raked) {
                    SetBit(static_map.HexFlags[hy2 * maxhx + hx2], FH_NOTRAKE);
                }
            });
        }

        // Data for client
        if (!item->GetIsHidden()) {
            scenery_count++;
            WriteData(scenery_data, item->GetId());
            WriteData(scenery_data, item->GetProtoId());
            vector<uchar*>* all_data = nullptr;
            vector<uint>* all_data_sizes = nullptr;
            item->StoreData(false, &all_data, &all_data_sizes);
            WriteData(scenery_data, static_cast<uint>(all_data->size()));
            for (size_t i = 0; i < all_data->size(); i++) {
                WriteData(scenery_data, all_data_sizes->at(i));
                WriteDataArr(scenery_data, all_data->at(i), all_data_sizes->at(i));
            }
        }
    }

    static_map.SceneryData.clear();
    WriteData(static_map.SceneryData, scenery_count);
    if (!scenery_data.empty()) {
        WriteDataArr(static_map.SceneryData, &scenery_data[0], scenery_data.size());
    }

    // Generate hashes
    static_map.HashTiles = maxhx * maxhy;
    if (!static_map.Tiles.empty()) {
        static_map.HashTiles = Hashing::MurmurHash2(reinterpret_cast<uchar*>(&static_map.Tiles[0]), static_cast<uint>(static_map.Tiles.size()) * sizeof(MapTile));
    }
    static_map.HashScen = maxhx * maxhy;
    if (!static_map.SceneryData.empty()) {
        static_map.HashScen = Hashing::MurmurHash2(static_cast<uchar*>(&static_map.SceneryData[0]), static_cast<uint>(static_map.SceneryData.size()));
    }

    // Shrink the vector capacities to fit their contents and reduce memory use
    static_map.SceneryData.shrink_to_fit();
    static_map.CrittersVec.shrink_to_fit();
    static_map.AllItemsVec.shrink_to_fit();
    static_map.HexItemsVec.shrink_to_fit();
    static_map.ChildItemsVec.shrink_to_fit();
    static_map.StaticItemsVec.shrink_to_fit();
    static_map.TriggerItemsVec.shrink_to_fit();
    static_map.Tiles.shrink_to_fit();

    _staticMaps.emplace(pmap, std::move(static_map));
}

auto MapManager::FindStaticMap(const ProtoMap* proto_map) const -> const StaticMap*
{
    const auto it = _staticMaps.find(proto_map);
    return it != _staticMaps.end() ? &it->second : nullptr;
}

void MapManager::GenerateMapContent(Map* map)
{
    NON_CONST_METHOD_HINT();

    std::map<uint, uint> id_map;

    // Generate npc
    for (const auto* base_cr : map->GetStaticMap()->CrittersVec) {
        auto* npc = _engine->CrMngr.CreateNpc(base_cr->GetProtoId(), &base_cr->GetProperties(), map, base_cr->GetHexX(), base_cr->GetHexY(), base_cr->GetDir(), true);
        if (npc == nullptr) {
            WriteLog("Create npc '{}' on map '{}' fail, continue generate.\n", base_cr->GetName(), map->GetName());
            continue;
        }

        id_map.insert(std::make_pair(base_cr->GetId(), npc->GetId()));

        // Check condition
        if (npc->GetCond() == CritterCondition::Dead) {
            npc->SetCurrentHp(_engine->Settings.DeadHitPoints - 1);

            const auto multihex = npc->GetMultihex();
            map->UnsetFlagCritter(npc->GetHexX(), npc->GetHexY(), multihex, false);
            map->SetFlagCritter(npc->GetHexX(), npc->GetHexY(), multihex, true);
        }
    }

    // Generate hex items
    for (const auto* base_item : map->GetStaticMap()->HexItemsVec) {
        // Create item
        auto* item = _engine->ItemMngr.CreateItem(base_item->GetProtoId(), 0, &base_item->GetProperties());
        if (item == nullptr) {
            WriteLog("Create item '{}' on map '{}' fail, continue generate.\n", base_item->GetName(), map->GetName());
            continue;
        }
        id_map.insert(std::make_pair(base_item->GetId(), item->GetId()));

        // Other values
        if (item->GetIsCanOpen() && item->GetOpened()) {
            item->SetIsLightThru(true);
        }

        if (!map->AddItem(item, item->GetHexX(), item->GetHexY())) {
            WriteLog("Add item '{}' to map '{}' failure, continue generate.\n", item->GetName(), map->GetName());
            _engine->ItemMngr.DeleteItem(item);
        }
    }

    // Add children items
    for (auto* base_item : map->GetStaticMap()->ChildItemsVec) {
        // Map id
        uint parent_id = 0;
        if (base_item->GetOwnership() == ItemOwnership::CritterInventory) {
            parent_id = base_item->GetCritId();
        }
        else if (base_item->GetOwnership() == ItemOwnership::ItemContainer) {
            parent_id = base_item->GetContainerId();
        }
        else {
            throw UnreachablePlaceException(LINE_STR);
        }

        if (id_map.count(parent_id) == 0u) {
            continue;
        }
        parent_id = id_map[parent_id];

        // Create item
        auto* item = _engine->ItemMngr.CreateItem(base_item->GetProtoId(), 0, &base_item->GetProperties());
        if (item == nullptr) {
            WriteLog("Create item '{}' on map '{}' fail, continue generate.\n", base_item->GetName(), map->GetName());
            continue;
        }

        // Add to parent
        if (base_item->GetOwnership() == ItemOwnership::CritterInventory) {
            auto* cr_cont = map->GetCritter(parent_id);
            RUNTIME_ASSERT(cr_cont);
            _engine->CrMngr.AddItemToCritter(cr_cont, item, false);
        }
        else if (base_item->GetOwnership() == ItemOwnership::ItemContainer) {
            auto* item_cont = map->GetItem(parent_id);
            RUNTIME_ASSERT(item_cont);
            _engine->ItemMngr.AddItemToContainer(item_cont, item, 0);
        }
        else {
            throw UnreachablePlaceException(LINE_STR);
        }
    }

    // Visible
    for (auto* npc : map->GetNpcs()) {
        ProcessVisibleCritters(npc);
        ProcessVisibleItems(npc);
    }

    // Map script
    if (!map->IsDestroyed()) {
        ScriptHelpers::CallInitScript(_engine->ScriptSys, map, map->GetInitScript(), true);
    }
}

void MapManager::DeleteMapContent(Map* map)
{
    while (!map->_mapCritters.empty() || !map->_mapItems.empty()) {
        // Transit players to global map
        KickPlayersToGlobalMap(map);

        // Delete npc
        auto del_npcs = map->_mapNonPlayerCritters;
        for (auto* del_npc : del_npcs) {
            _engine->CrMngr.DeleteNpc(del_npc);
        }

        // Delete items
        auto del_items = map->_mapItems;
        for (auto* del_item : del_items) {
            _engine->ItemMngr.DeleteItem(del_item);
        }
    }

    RUNTIME_ASSERT(map->_mapItemsById.empty());
    RUNTIME_ASSERT(map->_mapItemsByHex.empty());
    RUNTIME_ASSERT(map->_mapBlockLinesByHex.empty());
}

auto MapManager::GetLocationAndMapsStatistics() const -> string
{
    const auto locations = _engine->EntityMngr.GetLocations();
    const auto maps = _engine->EntityMngr.GetMaps();

    string result = _str("Locations count: {}\n", static_cast<uint>(locations.size()));
    result += _str("Maps count: {}\n", static_cast<uint>(maps.size()));
    result += "Location             Id           X     Y     Radius Color    Hidden  GeckVisible GeckCount AutoGarbage ToGarbage\n";
    result += "          Map                 Id          Time Rain Script\n";

    for (const auto* loc : locations) {
        result += _str("{:<20} {:<10}   {:<5} {:<5} {:<6} {:08X} {:<7} {:<11} {:<9} {:<11} {:<5}\n", loc->GetName(), loc->GetId(), loc->GetWorldX(), loc->GetWorldY(), loc->GetRadius(), loc->GetColor(), loc->GetHidden() ? "true" : "false", loc->GetGeckVisible() ? "true" : "false", loc->GeckCount, loc->GetAutoGarbage() ? "true" : "false", loc->GetToGarbage() ? "true" : "false");

        uint map_index = 0;
        for (const auto* map : loc->GetMaps()) {
            result += _str("     {:02}) {:<20} {:<9}   {:<4} {:<4} ", map_index, map->GetName(), map->GetId(), map->GetCurDayTime(), map->GetRainCapacity());
            result += map->GetInitScript();
            result += "\n";
            map_index++;
        }
    }

    return result;
}

auto MapManager::CreateLocation(hstring proto_id, ushort wx, ushort wy) -> Location*
{
    const auto* proto = _engine->ProtoMngr.GetProtoLocation(proto_id);
    if (proto == nullptr) {
        WriteLog("Location proto '{}' is not loaded.\n", proto_id);
        return nullptr;
    }

    if (wx >= GM_MAXZONEX * _engine->Settings.GlobalMapZoneLength || wy >= GM_MAXZONEY * _engine->Settings.GlobalMapZoneLength) {
        WriteLog("Invalid location '{}' coordinates.\n", proto_id);
        return nullptr;
    }

    auto* loc = new Location(_engine, 0, proto);
    loc->SetWorldX(wx);
    loc->SetWorldY(wy);

    for (const auto map_pid : loc->GetMapProtos()) {
        auto* map = CreateMap(map_pid, loc);
        if (map == nullptr) {
            WriteLog("Create map '{}' for location '{}' failed.\n", map_pid, proto_id);
            for (const auto* map2 : loc->GetMapsRaw()) {
                map2->Release();
            }
            loc->Release();
            return nullptr;
        }
    }

    loc->BindScript();

    _engine->EntityMngr.RegisterEntity(loc);

    // Generate location maps
    const auto maps = loc->GetMaps();

    for (auto* map : maps) {
        map->SetLocId(loc->GetId());
        GenerateMapContent(map);
    }

    // Init scripts
    for (auto* map : maps) {
        for (auto* item : map->GetItems()) {
            _engine->OnItemInit.Fire(item, true);
        }
        _engine->OnMapInit.Fire(map, true);
    }
    _engine->OnLocationInit.Fire(loc, true);

    return loc;
}

auto MapManager::CreateMap(hstring proto_id, Location* loc) -> Map*
{
    const auto* proto_map = _engine->ProtoMngr.GetProtoMap(proto_id);
    if (proto_map == nullptr) {
        WriteLog("Proto map '{}' is not loaded.\n", proto_id);
        return nullptr;
    }

    const auto it = _staticMaps.find(proto_map);
    if (it == _staticMaps.end()) {
        throw MapManagerException("Static map not found", proto_id);
    }

    auto* map = new Map(_engine, 0, proto_map, loc, &it->second);
    map->SetLocId(loc->GetId());

    auto& maps = loc->GetMapsRaw();
    map->SetLocMapIndex(static_cast<uint>(maps.size()));
    maps.push_back(map);

    _engine->EntityMngr.RegisterEntity(map);
    return map;
}

void MapManager::RegenerateMap(Map* map)
{
    _engine->OnMapFinish.Fire(map);

    DeleteMapContent(map);
    GenerateMapContent(map);

    for (auto* item : map->GetItems()) {
        _engine->OnItemInit.Fire(item, true);
    }

    _engine->OnMapInit.Fire(map, true);
}

auto MapManager::GetMap(uint map_id) -> Map*
{
    NON_CONST_METHOD_HINT();

    if (map_id == 0u) {
        return nullptr;
    }

    return _engine->EntityMngr.GetMap(map_id);
}

auto MapManager::GetMap(uint map_id) const -> const Map*
{
    return const_cast<MapManager*>(this)->GetMap(map_id);
}

auto MapManager::GetMapByPid(hstring map_pid, uint skip_count) -> Map*
{
    NON_CONST_METHOD_HINT();

    if (!map_pid) {
        return nullptr;
    }

    return _engine->EntityMngr.GetMapByPid(map_pid, skip_count);
}

auto MapManager::GetMaps() -> vector<Map*>
{
    NON_CONST_METHOD_HINT();

    return _engine->EntityMngr.GetMaps();
}

auto MapManager::GetMapsCount() const -> uint
{
    return static_cast<uint>(_engine->EntityMngr.GetMaps().size());
}

auto MapManager::GetLocationByMap(uint map_id) -> Location*
{
    auto* map = GetMap(map_id);
    if (map == nullptr) {
        return nullptr;
    }
    return map->GetLocation();
}

auto MapManager::GetLocation(uint loc_id) -> Location*
{
    NON_CONST_METHOD_HINT();

    if (loc_id == 0u) {
        return nullptr;
    }

    return _engine->EntityMngr.GetLocation(loc_id);
}

auto MapManager::GetLocation(uint loc_id) const -> const Location*
{
    return const_cast<MapManager*>(this)->GetLocation(loc_id);
}

auto MapManager::GetLocationByPid(hstring loc_pid, uint skip_count) -> Location*
{
    NON_CONST_METHOD_HINT();

    if (!loc_pid) {
        return nullptr;
    }
    return _engine->EntityMngr.GetLocationByPid(loc_pid, skip_count);
}

auto MapManager::IsIntersectZone(int wx1, int wy1, int w1_radius, int wx2, int wy2, int w2_radius, int zones) const -> bool
{
    const int zl = _engine->Settings.GlobalMapZoneLength;
    const IRect r1((wx1 - w1_radius) / zl - zones, (wy1 - w1_radius) / zl - zones, (wx1 + w1_radius) / zl + zones, (wy1 + w1_radius) / zl + zones);
    const IRect r2((wx2 - w2_radius) / zl, (wy2 - w2_radius) / zl, (wx2 + w2_radius) / zl, (wy2 + w2_radius) / zl);
    return r1.Left <= r2.Right && r2.Left <= r1.Right && r1.Top <= r2.Bottom && r2.Top <= r1.Bottom;
}

auto MapManager::GetZoneLocations(int zx, int zy, int zone_radius) -> vector<Location*>
{
    NON_CONST_METHOD_HINT();

    const auto wx = zx * static_cast<int>(_engine->Settings.GlobalMapZoneLength);
    const auto wy = zy * static_cast<int>(_engine->Settings.GlobalMapZoneLength);

    vector<Location*> locs;
    for (auto* loc : _engine->EntityMngr.GetLocations()) {
        if (!loc->IsDestroyed() && loc->IsLocVisible() && IsIntersectZone(wx, wy, 0, loc->GetWorldX(), loc->GetWorldY(), loc->GetRadius(), zone_radius)) {
            locs.push_back(loc);
        }
    }

    return locs;
}

void MapManager::KickPlayersToGlobalMap(Map* map)
{
    for (auto* cl : map->GetPlayers()) {
        TransitToGlobal(cl, 0, true);
    }
}

auto MapManager::GetLocations() -> vector<Location*>
{
    NON_CONST_METHOD_HINT();

    return _engine->EntityMngr.GetLocations();
}

auto MapManager::GetLocationsCount() const -> uint
{
    return static_cast<uint>(_engine->EntityMngr.GetLocations().size());
}

void MapManager::LocationGarbager()
{
    if (_runGarbager) {
        _runGarbager = false;

        vector<Critter*>* gmap_players = nullptr;
        vector<Critter*> players;
        for (auto* loc : _engine->EntityMngr.GetLocations()) {
            if (loc->GetAutoGarbage() && loc->IsCanDelete()) {
                if (gmap_players == nullptr) {
                    players = _engine->CrMngr.GetPlayerCritters(true);
                    gmap_players = &players;
                }
                DeleteLocation(loc, gmap_players);
            }
        }
    }
}

void MapManager::DeleteLocation(Location* loc, vector<Critter*>* gmap_player_critters)
{
    // Start deleting
    auto&& maps = copy(loc->GetMaps());

    // Redundant calls
    if (loc->IsDestroying() || loc->IsDestroyed()) {
        return;
    }

    loc->MarkAsDestroying();
    for (auto* map : maps) {
        map->MarkAsDestroying();
    }

    // Finish events
    _engine->OnLocationFinish.Fire(loc);
    for (auto* map : maps) {
        _engine->OnMapFinish.Fire(map);
    }

    // Send players on global map about this
    vector<Critter*> players;
    if (gmap_player_critters == nullptr) {
        players = _engine->CrMngr.GetPlayerCritters(true);
        gmap_player_critters = &players;
    }

    for (auto* cl : *gmap_player_critters) {
        if (CheckKnownLoc(cl, loc->GetId())) {
            cl->Send_GlobalLocation(loc, false);
        }
    }

    // Delete maps
    for (auto* map : maps) {
        DeleteMapContent(map);
    }
    loc->GetMapsRaw().clear();

    // Erase from main collections
    _engine->EntityMngr.UnregisterEntity(loc);
    for (auto* map : maps) {
        _engine->EntityMngr.UnregisterEntity(map);
    }

    // Invalidate for use
    loc->MarkAsDestroyed();
    for (auto* map : maps) {
        map->MarkAsDestroyed();
    }

    // Release after some time
    for (auto* map : maps) {
        map->Release();
    }
    loc->Release();
}

void MapManager::TraceBullet(TraceData& trace)
{
    NON_CONST_METHOD_HINT();

    auto* map = trace.TraceMap;
    const auto maxhx = map->GetWidth();
    const auto maxhy = map->GetHeight();
    const auto hx = trace.BeginHx;
    const auto hy = trace.BeginHy;
    const auto tx = trace.EndHx;
    const auto ty = trace.EndHy;

    auto dist = trace.Dist;
    if (dist == 0u) {
        dist = _engine->GeomHelper.DistGame(hx, hy, tx, ty);
    }

    auto cx = hx;
    auto cy = hy;
    auto old_cx = cx;
    auto old_cy = cy;

    LineTracer line_tracer(_engine->Settings, hx, hy, tx, ty, maxhx, maxhy, trace.Angle);

    trace.IsFullTrace = false;
    trace.IsCritterFounded = false;
    trace.IsHaveLastPassed = false;
    auto last_passed_ok = false;
    for (uint i = 0;; i++) {
        if (i >= dist) {
            trace.IsFullTrace = true;
            break;
        }

        uchar dir;
        if (_engine->Settings.MapHexagonal) {
            dir = line_tracer.GetNextHex(cx, cy);
        }
        else {
            line_tracer.GetNextSquare(cx, cy);
            dir = _engine->GeomHelper.GetNearDir(old_cx, old_cy, cx, cy);
        }

        if (trace.HexCallback != nullptr) {
            trace.HexCallback(map, trace.FindCr, old_cx, old_cy, cx, cy, dir);
            old_cx = cx;
            old_cy = cy;
            continue;
        }

        if (trace.LastPassed != nullptr && !last_passed_ok) {
            if (map->IsHexPassed(cx, cy)) {
                (*trace.LastPassed).first = cx;
                (*trace.LastPassed).second = cy;
                trace.IsHaveLastPassed = true;
            }
            else if (!map->IsHexCritter(cx, cy) || !trace.LastPassedSkipCritters) {
                last_passed_ok = true;
            }
        }

        if (!map->IsHexRaked(cx, cy)) {
            break;
        }

        if (trace.Critters != nullptr && map->IsHexCritter(cx, cy)) {
            *trace.Critters = map->GetCrittersHex(cx, cy, 0, trace.FindType);
        }

        if (trace.FindCr != nullptr && map->IsFlagCritter(cx, cy, false)) {
            const auto* cr = map->GetHexCritter(cx, cy, false);
            if (cr == trace.FindCr) {
                trace.IsCritterFounded = true;
                break;
            }
        }

        old_cx = cx;
        old_cy = cy;
    }

    if (trace.PreBlock != nullptr) {
        (*trace.PreBlock).first = old_cx;
        (*trace.PreBlock).second = old_cy;
    }
    if (trace.Block != nullptr) {
        (*trace.Block).first = cx;
        (*trace.Block).second = cy;
    }
}

static thread_local int MapGridOffsX = 0;
static thread_local int MapGridOffsY = 0;
static thread_local short* Grid = nullptr;
#define GRID(x, y) Grid[((FPATH_MAX_PATH + 1) + (y)-MapGridOffsY) * (FPATH_MAX_PATH * 2 + 2) + ((FPATH_MAX_PATH + 1) + (x)-MapGridOffsX)]

auto MapManager::FindPath(const FindPathInput& pfd) -> FindPathOutput
{
    // Allocate temporary grid
    if (Grid == nullptr) {
        Grid = new short[(FPATH_MAX_PATH * 2 + 2) * (FPATH_MAX_PATH * 2 + 2)];
    }

    // Data
    auto map_id = pfd.MapId;
    auto from_hx = pfd.FromX;
    auto from_hy = pfd.FromY;
    auto to_hx = pfd.ToX;
    auto to_hy = pfd.ToY;
    auto multihex = pfd.Multihex;
    auto cut = pfd.Cut;
    auto trace = pfd.Trace;
    auto is_run = pfd.IsRun;
    auto check_cr = pfd.CheckCrit;
    auto check_gag_items = pfd.CheckGagItems;
    auto dirs_count = _engine->Settings.MapDirCount;

    FindPathOutput output;

    // Checks
    if (trace != 0u && pfd.TraceCr == nullptr) {
        output.Result = FindPathResult::TraceTargetNullptr;
        return output;
    }

    auto* map = GetMap(map_id);
    if (map == nullptr) {
        output.Result = FindPathResult::MapNotFound;
        return output;
    }

    const auto maxhx = map->GetWidth();
    const auto maxhy = map->GetHeight();

    if (from_hx >= maxhx || from_hy >= maxhy || to_hx >= maxhx || to_hy >= maxhy) {
        output.Result = FindPathResult::InvalidHexes;
        return output;
    }
    if (_engine->GeomHelper.CheckDist(from_hx, from_hy, to_hx, to_hy, cut)) {
        output.Result = FindPathResult::AlreadyHere;
        return output;
    }
    if (cut == 0u && IsBitSet(map->GetHexFlags(to_hx, to_hy), FH_NOWAY)) {
        output.Result = FindPathResult::HexBusy;
        return output;
    }

    // Ring check
    if (cut <= 1u && multihex == 0u) {
        auto [rsx, rsy] = _engine->GeomHelper.GetHexOffsets((to_hx % 2) != 0);

        auto i = 0;
        for (; i < dirs_count; i++, rsx++, rsy++) {
            const auto xx = static_cast<int>(to_hx + *rsx);
            const auto yy = static_cast<int>(to_hy + *rsy);
            if (xx >= 0 && xx < maxhx && yy >= 0 && yy < maxhy) {
                const auto flags = map->GetHexFlags(static_cast<ushort>(xx), static_cast<ushort>(yy));
                if (IsBitSet(flags, static_cast<ushort>(FH_GAG_ITEM << 8))) {
                    break;
                }
                if (!IsBitSet(flags, FH_NOWAY)) {
                    break;
                }
            }
        }

        if (i == dirs_count) {
            output.Result = FindPathResult::HexBusyRing;
            return output;
        }
    }

    // Parse previous move params
    /*UShortPairVec first_steps;
       uchar first_dir=pfd.MoveParams&7;
       if(first_dir<settings.MapDirCount)
       {
            ushort hx_=from_hx;
            ushort hy_=from_hy;
            MoveHexByDir(hx_,hy_,first_dir);
            if(map->IsHexPassed(hx_,hy_))
            {
                    first_steps.push_back(PAIR(hx_,hy_));
            }
       }
       for(int i=0;i<4;i++)
       {

       }*/

    // Prepare
    MapGridOffsX = from_hx;
    MapGridOffsY = from_hy;

    short numindex = 1;
    std::memset(Grid, 0, (FPATH_MAX_PATH * 2 + 2) * (FPATH_MAX_PATH * 2 + 2) * sizeof(short));
    GRID(from_hx, from_hy) = numindex;

    vector<pair<ushort, ushort>> coords;
    vector<pair<ushort, ushort>> cr_coords;
    vector<pair<ushort, ushort>> gag_coords;
    coords.reserve(10000);
    cr_coords.reserve(100);
    gag_coords.reserve(100);

    // First point
    coords.emplace_back(from_hx, from_hy);

    // Begin search
    auto p = 0;
    auto p_togo = 1;
    ushort cx = 0;
    ushort cy = 0;
    while (true) {
        for (auto i = 0; i < p_togo; i++, p++) {
            cx = coords[p].first;
            cy = coords[p].second;
            numindex = GRID(cx, cy);

            if (_engine->GeomHelper.CheckDist(cx, cy, to_hx, to_hy, cut)) {
                goto label_FindOk;
            }
            if (++numindex > FPATH_MAX_PATH) {
                output.Result = FindPathResult::TooFar;
                return output;
            }

            const auto [sx, sy] = _engine->GeomHelper.GetHexOffsets((cx & 1) != 0);

            for (auto j = 0; j < dirs_count; j++) {
                const auto nxi = cx + sx[j];
                const auto nyi = cy + sy[j];
                if (nxi < 0 || nyi < 0 || nxi >= maxhx || nyi >= maxhy) {
                    continue;
                }

                const auto nx = static_cast<ushort>(nxi);
                const auto ny = static_cast<ushort>(nyi);
                auto& grid_cell = GRID(nx, ny);
                if (grid_cell != 0) {
                    continue;
                }

                if (multihex == 0u) {
                    auto flags = map->GetHexFlags(nx, ny);
                    if (!IsBitSet(flags, FH_NOWAY)) {
                        coords.emplace_back(nx, ny);
                        grid_cell = numindex;
                    }
                    else if (check_gag_items && IsBitSet(flags, static_cast<ushort>(FH_GAG_ITEM << 8))) {
                        gag_coords.emplace_back(nx, ny);
                        grid_cell = numindex | 0x4000;
                    }
                    else if (check_cr && IsBitSet(flags, static_cast<ushort>(FH_CRITTER << 8))) {
                        cr_coords.emplace_back(nx, ny);
                        grid_cell = numindex | 0x8000;
                    }
                    else {
                        grid_cell = -1;
                    }
                }
                else {
                    /*
                       // Multihex
                       // Base hex
                       int hx_=nx,hy_=ny;
                       for(uint k=0;k<multihex;k++) MoveHexByDirUnsafe(hx_,hy_,j);
                       if(hx_<0 || hy_<0 || hx_>=maxhx || hy_>=maxhy) continue;
                       //if(!IsHexPassed(hx_,hy_)) return false;

                       // Clock wise hexes
                       bool is_square_corner=(!settings.MapHexagonal && (j % 2) != 0);
                       uint steps_count=(is_square_corner?multihex*2:multihex);
                       int dir_=(settings.MapHexagonal?((j+2)%6):((j+2)%8));
                       if(is_square_corner) dir_=(dir_+1)%8;
                       int hx__=hx_,hy__=hy_;
                       for(uint k=0;k<steps_count;k++)
                       {
                            MoveHexByDirUnsafe(hx__,hy__,dir_);
                            //if(!IsHexPassed(hx__,hy__)) return false;
                       }

                       // Counter clock wise hexes
                       dir_=(settings.MapHexagonal?((j+4)%6):((j+6)%8));
                       if(is_square_corner) dir_=(dir_+7)%8;
                       hx__=hx_,hy__=hy_;
                       for(uint k=0;k<steps_count;k++)
                       {
                            MoveHexByDirUnsafe(hx__,hy__,dir_);
                            //if(!IsHexPassed(hx__,hy__)) return false;
                       }
                     */

                    if (map->IsMovePassed(nx, ny, static_cast<uchar>(j), multihex)) {
                        coords.emplace_back(nx, ny);
                        grid_cell = numindex;
                    }
                    else {
                        grid_cell = -1;
                    }
                }
            }
        }

        // Add gag hex after some distance
        if (!gag_coords.empty()) {
            auto last_index = GRID(coords.back().first, coords.back().second);
            auto& xy = gag_coords.front();
            short gag_index = GRID(xy.first, xy.second) ^ 0x4000;
            if (gag_index + 10 < last_index) // Todo: if path finding not be reworked than migrate magic number to scripts
            {
                GRID(xy.first, xy.second) = gag_index;
                coords.push_back(xy);
                gag_coords.erase(gag_coords.begin());
            }
        }

        // Add gag and critters hexes
        p_togo = static_cast<int>(coords.size()) - p;
        if (p_togo == 0) {
            if (!gag_coords.empty()) {
                auto& xy = gag_coords.front();
                GRID(xy.first, xy.second) ^= 0x4000;
                coords.push_back(xy);
                gag_coords.erase(gag_coords.begin());
                p_togo++;
            }
            else if (!cr_coords.empty()) {
                auto& xy = cr_coords.front();
                GRID(xy.first, xy.second) ^= 0x8000;
                coords.push_back(xy);
                cr_coords.erase(cr_coords.begin());
                p_togo++;
            }
        }

        if (p_togo == 0) {
            output.Result = FindPathResult::DeadLock;
            return output;
        }
    }

label_FindOk:
    auto& steps = output.Steps;
    steps.resize(static_cast<size_t>(numindex - 1));

    // Smooth data
    if (!_engine->Settings.MapSmoothPath) {
        _smoothSwitcher = false;
    }

    auto smooth_count = 0;
    auto smooth_iteration = 0;
    if (_engine->Settings.MapSmoothPath && !_engine->Settings.MapHexagonal) {
        int x1 = cx;
        int y1 = cy;
        int x2 = from_hx;
        int y2 = from_hy;
        auto dx = abs(x1 - x2);
        auto dy = abs(y1 - y2);
        auto d = std::max(dx, dy);
        auto h1 = abs(dx - dy);
        auto h2 = d - h1;
        if (dy < dx) {
            std::swap(h1, h2);
        }
        smooth_count = h1 != 0 && h2 != 0 ? h1 / h2 + 1 : 3;
        if (smooth_count < 3) {
            smooth_count = 3;
        }

        smooth_count = h1 != 0 && h2 != 0 ? std::max(h1, h2) / std::min(h1, h2) + 1 : 0;
        if (h1 != 0 && h2 != 0 && smooth_count < 2) {
            smooth_count = 2;
        }
        smooth_iteration = h1 != 0 && h2 != 0 ? std::min(h1, h2) % std::max(h1, h2) : 0;
    }

    while (numindex > 1) {
        if (_engine->Settings.MapSmoothPath) {
            if (_engine->Settings.MapHexagonal) {
                if ((numindex & 1) != 0) {
                    _smoothSwitcher = !_smoothSwitcher;
                }
            }
            else {
                _smoothSwitcher = smooth_count < 2 || smooth_iteration % smooth_count != 0;
            }
        }

        numindex--;
        auto& ps = steps[numindex - 1];
        ps.HexX = cx;
        ps.HexY = cy;
        const auto dir = FindPathGrid(cx, cy, numindex, _smoothSwitcher);
        if (dir == std::numeric_limits<uchar>::max()) {
            output.Result = FindPathResult::InternalError;
            return output;
        }
        ps.Dir = dir;

        smooth_iteration++;
    }

    // Check for closed door and critter
    if (check_cr || check_gag_items) {
        for (auto i = 0, j = static_cast<int>(steps.size()); i < j; i++) {
            auto& ps = steps[i];
            if (map->IsHexPassed(ps.HexX, ps.HexY)) {
                continue;
            }

            if (check_gag_items && map->IsHexGag(ps.HexX, ps.HexY)) {
                auto* item = map->GetItemGag(ps.HexX, ps.HexY);
                if (item == nullptr) {
                    continue;
                }
                output.GagItem = item;
                steps.resize(i);
                break;
            }

            if (check_cr && map->IsFlagCritter(ps.HexX, ps.HexY, false)) {
                auto* cr = map->GetHexCritter(ps.HexX, ps.HexY, false);
                if (cr == nullptr || cr == pfd.FromCritter) {
                    continue;
                }
                output.GagCritter = cr;
                steps.resize(i);
                break;
            }
        }
    }

    // Trace
    if (trace != 0u) {
        vector<int> trace_seq;
        auto targ_hx = pfd.TraceCr->GetHexX();
        auto targ_hy = pfd.TraceCr->GetHexY();
        auto trace_ok = false;

        trace_seq.resize(steps.size() + 4);
        for (auto i = 0, j = static_cast<int>(steps.size()); i < j; i++) {
            auto& ps = steps[i];
            if (map->IsHexGag(ps.HexX, ps.HexY)) {
                trace_seq[i + 2 - 2] += 1;
                trace_seq[i + 2 - 1] += 2;
                trace_seq[i + 2 - 0] += 3;
                trace_seq[i + 2 + 1] += 2;
                trace_seq[i + 2 + 2] += 1;
            }
        }

        TraceData trace_;
        trace_.TraceMap = map;
        trace_.EndHx = targ_hx;
        trace_.EndHy = targ_hy;
        trace_.FindCr = pfd.TraceCr;
        for (auto k = 0; k < 5; k++) {
            for (auto i = 0, j = static_cast<int>(steps.size()); i < j; i++) {
                if (k < 4 && trace_seq[i + 2] != k) {
                    continue;
                }
                if (k == 4 && trace_seq[i + 2] < 4) {
                    continue;
                }

                auto& ps = steps[i];

                if (!_engine->GeomHelper.CheckDist(ps.HexX, ps.HexY, targ_hx, targ_hy, trace)) {
                    continue;
                }

                trace_.BeginHx = ps.HexX;
                trace_.BeginHy = ps.HexY;
                TraceBullet(trace_);
                if (trace_.IsCritterFounded) {
                    trace_ok = true;
                    steps.resize(i + 1);
                    goto label_TraceOk;
                }
            }
        }

        if (!trace_ok && output.GagItem == nullptr && output.GagCritter == nullptr) {
            output.Result = FindPathResult::TraceFailed;
            return output;
        }

    label_TraceOk:
        if (trace_ok) {
            output.GagItem = nullptr;
            output.GagCritter = nullptr;
        }
    }

    // Parse move params
    PathSetMoveParams(steps, is_run);

    // Number of path
    if (steps.empty()) {
        output.Result = FindPathResult::AlreadyHere;
        return output;
    }

    // New X,Y
    auto& ps = steps[steps.size() - 1];
    output.NewToX = ps.HexX;
    output.NewToY = ps.HexY;
    output.Result = FindPathResult::Ok;
    return output;
}

auto MapManager::FindPathGrid(ushort& hx, ushort& hy, int index, bool smooth_switcher) const -> uchar
{
    // Hexagonal
    if (_engine->Settings.MapHexagonal) {
        if (smooth_switcher) {
            if ((hx & 1) != 0) {
                if (GRID(hx - 1, hy - 1) == index) {
                    hx--;
                    hy--;
                    return 3u;
                } // 0
                if (GRID(hx, hy - 1) == index) {
                    hy--;
                    return 2u;
                } // 5
                if (GRID(hx, hy + 1) == index) {
                    hy++;
                    return 5u;
                } // 2
                if (GRID(hx + 1, hy) == index) {
                    hx++;
                    return 0u;
                } // 3
                if (GRID(hx - 1, hy) == index) {
                    hx--;
                    return 4u;
                } // 1
                if (GRID(hx + 1, hy - 1) == index) {
                    hx++;
                    hy--;
                    return 1u;
                } // 4
            }
            else {
                if (GRID(hx - 1, hy) == index) {
                    hx--;
                    return 3u;
                } // 0
                if (GRID(hx, hy - 1) == index) {
                    hy--;
                    return 2u;
                } // 5
                if (GRID(hx, hy + 1) == index) {
                    hy++;
                    return 5u;
                } // 2
                if (GRID(hx + 1, hy + 1) == index) {
                    hx++;
                    hy++;
                    return 0u;
                } // 3
                if (GRID(hx - 1, hy + 1) == index) {
                    hx--;
                    hy++;
                    return 4u;
                } // 1
                if (GRID(hx + 1, hy) == index) {
                    hx++;
                    return 1u;
                } // 4
            }
        }
        else {
            if ((hx & 1) != 0) {
                if (GRID(hx - 1, hy) == index) {
                    hx--;
                    return 4u;
                } // 1
                if (GRID(hx + 1, hy - 1) == index) {
                    hx++;
                    hy--;
                    return 1u;
                } // 4
                if (GRID(hx, hy - 1) == index) {
                    hy--;
                    return 2u;
                } // 5
                if (GRID(hx - 1, hy - 1) == index) {
                    hx--;
                    hy--;
                    return 3u;
                } // 0
                if (GRID(hx + 1, hy) == index) {
                    hx++;
                    return 0u;
                } // 3
                if (GRID(hx, hy + 1) == index) {
                    hy++;
                    return 5u;
                } // 2
            }
            else {
                if (GRID(hx - 1, hy + 1) == index) {
                    hx--;
                    hy++;
                    return 4u;
                } // 1
                if (GRID(hx + 1, hy) == index) {
                    hx++;
                    return 1u;
                } // 4
                if (GRID(hx, hy - 1) == index) {
                    hy--;
                    return 2u;
                } // 5
                if (GRID(hx - 1, hy) == index) {
                    hx--;
                    return 3u;
                } // 0
                if (GRID(hx + 1, hy + 1) == index) {
                    hx++;
                    hy++;
                    return 0u;
                } // 3
                if (GRID(hx, hy + 1) == index) {
                    hy++;
                    return 5u;
                } // 2
            }
        }
    }
    // Square
    else {
        // Without smoothing
        if (!_engine->Settings.MapSmoothPath) {
            if (GRID(hx - 1, hy) == index) {
                hx--;
                return 0u;
            } // 0
            if (GRID(hx, hy - 1) == index) {
                hy--;
                return 6u;
            } // 6
            if (GRID(hx, hy + 1) == index) {
                hy++;
                return 2u;
            } // 2
            if (GRID(hx + 1, hy) == index) {
                hx++;
                return 4u;
            } // 4
            if (GRID(hx - 1, hy + 1) == index) {
                hx--;
                hy++;
                return 1u;
            } // 1
            if (GRID(hx + 1, hy - 1) == index) {
                hx++;
                hy--;
                return 5u;
            } // 5
            if (GRID(hx + 1, hy + 1) == index) {
                hx++;
                hy++;
                return 3u;
            } // 3
            if (GRID(hx - 1, hy - 1) == index) {
                hx--;
                hy--;
                return 7u;
            } // 7
        }
        // With smoothing
        else {
            if (smooth_switcher) {
                if (GRID(hx - 1, hy) == index) {
                    hx--;
                    return 0u;
                } // 0
                if (GRID(hx, hy + 1) == index) {
                    hy++;
                    return 2u;
                } // 2
                if (GRID(hx + 1, hy) == index) {
                    hx++;
                    return 4u;
                } // 4
                if (GRID(hx, hy - 1) == index) {
                    hy--;
                    return 6u;
                } // 6
                if (GRID(hx + 1, hy + 1) == index) {
                    hx++;
                    hy++;
                    return 3u;
                } // 3
                if (GRID(hx - 1, hy - 1) == index) {
                    hx--;
                    hy--;
                    return 7u;
                } // 7
                if (GRID(hx - 1, hy + 1) == index) {
                    hx--;
                    hy++;
                    return 1u;
                } // 1
                if (GRID(hx + 1, hy - 1) == index) {
                    hx++;
                    hy--;
                    return 5u;
                } // 5
            }
            else {
                if (GRID(hx + 1, hy + 1) == index) {
                    hx++;
                    hy++;
                    return 3u;
                } // 3
                if (GRID(hx - 1, hy - 1) == index) {
                    hx--;
                    hy--;
                    return 7u;
                } // 7
                if (GRID(hx - 1, hy) == index) {
                    hx--;
                    return 0u;
                } // 0
                if (GRID(hx, hy + 1) == index) {
                    hy++;
                    return 2u;
                } // 2
                if (GRID(hx + 1, hy) == index) {
                    hx++;
                    return 4u;
                } // 4
                if (GRID(hx, hy - 1) == index) {
                    hy--;
                    return 6u;
                } // 6
                if (GRID(hx - 1, hy + 1) == index) {
                    hx--;
                    hy++;
                    return 1u;
                } // 1
                if (GRID(hx + 1, hy - 1) == index) {
                    hx++;
                    hy--;
                    return 5u;
                } // 5
            }
        }
    }

    return std::numeric_limits<uchar>::max();
}

#undef GRID

void MapManager::PathSetMoveParams(vector<PathStep>& path, bool is_run)
{
    uint move_params = 0; // Base parameters
    for (auto i = static_cast<int>(path.size()) - 1; i >= 0; i--) // From end to beginning
    {
        auto& ps = path[i];

        // Walk flags
        if (is_run) {
            SetBit(move_params, MOVE_PARAM_RUN);
        }
        else {
            UnsetBit(move_params, MOVE_PARAM_RUN);
        }

        // Store
        ps.MoveParams = move_params;

        // Add dir to sequence
        move_params = move_params << MOVE_PARAM_STEP_BITS | ps.Dir | MOVE_PARAM_STEP_ALLOW;
    }
}

auto MapManager::TransitToGlobal(Critter* cr, uint leader_id, bool force) -> bool
{
    if (cr->LockMapTransfers != 0) {
        WriteLog("Transfers locked, critter '{}'.\n", cr->GetName());
        return false;
    }

    return Transit(cr, nullptr, 0, 0, 0, 0, leader_id, force);
}

auto MapManager::Transit(Critter* cr, Map* map, ushort hx, ushort hy, uchar dir, uint radius, uint leader_id, bool force) -> bool
{
    // Check location deletion
    auto* loc = map != nullptr ? map->GetLocation() : nullptr;
    if (loc != nullptr && loc->GetToGarbage()) {
        WriteLog("Transfer to deleted location, critter '{}'.\n", cr->GetName());
        return false;
    }

    // Maybe critter already in transfer
    if (cr->LockMapTransfers != 0) {
        WriteLog("Transfers locked, critter '{}'.\n", cr->GetName());
        return false;
    }

    // Check force
    if (!force) {
        if (cr->GetTimeoutTransfer() > _engine->GameTime.GetFullSecond() || cr->GetTimeoutBattle() > _engine->GameTime.GetFullSecond()) {
            return false;
        }
        if (cr->IsDead()) {
            return false;
        }
        if (cr->IsKnockout()) {
            return false;
        }
        if (loc != nullptr && !loc->IsCanEnter(1)) {
            return false;
        }
    }

    const auto map_id = map != nullptr ? map->GetId() : 0;
    const auto old_map_id = cr->GetMapId();
    auto* old_map = GetMap(old_map_id);

    // Recheck after synchronization
    if (cr->GetMapId() != old_map_id) {
        return false;
    }

    if (old_map_id == map_id) {
        // One map
        if (map_id == 0u) {
            // Todo: check group
            return true;
        }

        if (map == nullptr || hx >= map->GetWidth() || hy >= map->GetHeight()) {
            return false;
        }

        const auto multihex = cr->GetMultihex();
        if (!map->FindStartHex(hx, hy, multihex, radius, true) && !map->FindStartHex(hx, hy, multihex, radius, false)) {
            return false;
        }

        cr->LockMapTransfers++;

        cr->SetDir(dir >= _engine->Settings.MapDirCount ? 0 : dir);
        map->UnsetFlagCritter(cr->GetHexX(), cr->GetHexY(), multihex, cr->IsDead());
        cr->SetHexX(hx);
        cr->SetHexY(hy);
        map->SetFlagCritter(hx, hy, multihex, cr->IsDead());
        cr->Send_CustomCommand(cr, OTHER_TELEPORT, cr->GetHexX() << 16 | cr->GetHexY());
        cr->ClearVisible();
        ProcessVisibleCritters(cr);
        ProcessVisibleItems(cr);
        cr->Send_Position(cr);

        cr->LockMapTransfers--;
    }
    else {
        // Different maps
        if (map != nullptr) {
            const auto multihex = cr->GetMultihex();
            if (!map->FindStartHex(hx, hy, multihex, radius, true) && !map->FindStartHex(hx, hy, multihex, radius, false)) {
                return false;
            }
        }
        if (!CanAddCrToMap(cr, map, hx, hy, leader_id)) {
            return false;
        }

        cr->LockMapTransfers++;

        if (old_map_id == 0u || old_map != nullptr) {
            EraseCrFromMap(cr, old_map);
        }

        cr->SetLastMapHexX(cr->GetHexX());
        cr->SetLastMapHexY(cr->GetHexY());

        AddCrToMap(cr, map, hx, hy, dir, leader_id);

        cr->Send_LoadMap(nullptr, *this);

        // Visible critters / items
        if (auto* owner = cr->GetOwner(); owner != nullptr) {
            owner->DisableSend++;
        }

        ProcessVisibleCritters(cr);
        ProcessVisibleItems(cr);

        if (auto* owner = cr->GetOwner(); owner != nullptr) {
            owner->DisableSend--;
        }

        cr->LockMapTransfers--;
    }
    return true;
}

auto MapManager::CanAddCrToMap(Critter* cr, Map* map, ushort hx, ushort hy, uint leader_id) const -> bool
{
    if (map != nullptr) {
        if (hx >= map->GetWidth() || hy >= map->GetHeight()) {
            return false;
        }
        if (!map->IsHexesPassed(hx, hy, cr->GetMultihex())) {
            return false;
        }
    }
    else {
        if (leader_id != 0u && leader_id != cr->GetId()) {
            auto* leader = _engine->CrMngr.GetCritter(leader_id);
            if (leader == nullptr || leader->GetMapId() != 0u) {
                return false;
            }
        }
    }
    return true;
}

void MapManager::AddCrToMap(Critter* cr, Map* map, ushort hx, ushort hy, uchar dir, uint leader_id)
{
    NON_CONST_METHOD_HINT();

    cr->LockMapTransfers++;

    cr->SetTimeoutBattle(0);
    cr->SetTimeoutTransfer(_engine->GameTime.GetFullSecond() + _engine->Settings.TimeoutTransfer);

    if (map != nullptr) {
        RUNTIME_ASSERT(hx < map->GetWidth() && hy < map->GetHeight());

        cr->SetMapId(map->GetId());
        cr->SetRefMapId(map->GetId());
        cr->SetRefMapPid(map->GetProtoId());
        cr->SetRefLocationId(map->GetLocation() != nullptr ? map->GetLocation()->GetId() : 0u);
        cr->SetRefLocationPid(map->GetLocation() != nullptr ? map->GetLocation()->GetProtoId() : hstring());
        cr->SetHexX(hx);
        cr->SetHexY(hy);
        cr->SetDir(dir);

        map->AddCritter(cr);

        _engine->OnMapCritterIn.Fire(map, cr);
    }
    else {
        RUNTIME_ASSERT(!cr->GlobalMapGroup);

        if (leader_id == 0u || leader_id == cr->GetId()) {
            cr->SetGlobalMapLeaderId(cr->GetId());
            cr->SetRefGlobalMapLeaderId(cr->GetId());
            cr->SetGlobalMapTripId(cr->GetGlobalMapTripId() + 1);
            cr->SetRefGlobalMapTripId(cr->GetGlobalMapTripId());

            cr->GlobalMapGroup = new vector<Critter*>();
            cr->GlobalMapGroup->push_back(cr);
        }
        else {
            auto* leader = _engine->CrMngr.GetCritter(leader_id);
            RUNTIME_ASSERT(leader);
            RUNTIME_ASSERT(!leader->GetMapId());

            cr->SetWorldX(leader->GetWorldX());
            cr->SetWorldY(leader->GetWorldY());
            cr->SetGlobalMapLeaderId(leader_id);
            cr->SetRefGlobalMapLeaderId(leader_id);
            cr->SetGlobalMapTripId(leader->GetGlobalMapTripId());

            for (auto& it : *leader->GlobalMapGroup) {
                it->Send_AddCritter(cr);
            }

            cr->GlobalMapGroup = leader->GlobalMapGroup;
            cr->GlobalMapGroup->push_back(cr);
        }

        _engine->OnGlobalMapCritterIn.Fire(cr);
    }

    cr->LockMapTransfers--;
}

void MapManager::EraseCrFromMap(Critter* cr, Map* map)
{
    cr->LockMapTransfers++;

    if (map != nullptr) {
        _engine->OnMapCritterOut.Fire(map, cr);

        for (auto* other : copy(cr->VisCr)) {
            other->OnCritterDisappeared.Fire(cr);
        }

        cr->ClearVisible();
        map->EraseCritter(cr);
        map->UnsetFlagCritter(cr->GetHexX(), cr->GetHexY(), cr->GetMultihex(), cr->IsDead());

        cr->SetMapId(0);

        _runGarbager = true;
    }
    else {
        RUNTIME_ASSERT(cr->GlobalMapGroup);

        _engine->OnGlobalMapCritterOut.Fire(cr);

        const auto it = std::find(cr->GlobalMapGroup->begin(), cr->GlobalMapGroup->end(), cr);
        cr->GlobalMapGroup->erase(it);

        if (!cr->GlobalMapGroup->empty()) {
            auto* new_leader = *cr->GlobalMapGroup->begin();
            for (auto* group_cr : *cr->GlobalMapGroup) {
                group_cr->SetGlobalMapLeaderId(new_leader->GetId());
                group_cr->SetRefGlobalMapLeaderId(new_leader->GetId());
                group_cr->Send_RemoveCritter(cr);
            }
            cr->GlobalMapGroup = nullptr;
        }
        else {
            delete cr->GlobalMapGroup;
            cr->GlobalMapGroup = nullptr;
        }

        cr->SetGlobalMapLeaderId(0);
    }

    cr->LockMapTransfers--;
}

void MapManager::ProcessVisibleCritters(Critter* view_cr)
{
    NON_CONST_METHOD_HINT();

    if (view_cr->IsDestroyed()) {
        return;
    }

    // Global map
    if (view_cr->GetMapId() == 0u) {
        RUNTIME_ASSERT(view_cr->GlobalMapGroup);

        if (view_cr->IsPlayer()) {
            for (auto* cr : *view_cr->GlobalMapGroup) {
                if (view_cr == cr) {
                    SetBit(view_cr->Flags, FCRIT_CHOSEN);
                    view_cr->Send_AddCritter(view_cr);
                    UnsetBit(view_cr->Flags, FCRIT_CHOSEN);
                    view_cr->Send_AddAllItems();
                }
                else {
                    view_cr->Send_AddCritter(cr);
                }
            }
        }

        return;
    }

    // Local map
    auto* map = GetMap(view_cr->GetMapId());
    RUNTIME_ASSERT(map);

    uint vis;
    const auto look_base_self = view_cr->GetLookDistance();
    const int dir_self = view_cr->GetDir();
    const auto dirs_count = _engine->Settings.MapDirCount;
    const auto show_cr_dist1 = view_cr->GetShowCritterDist1();
    const auto show_cr_dist2 = view_cr->GetShowCritterDist2();
    const auto show_cr_dist3 = view_cr->GetShowCritterDist3();
    const auto show_cr1 = show_cr_dist1 > 0;
    const auto show_cr2 = show_cr_dist2 > 0;
    const auto show_cr3 = show_cr_dist3 > 0;
    const auto show_cr = (show_cr1 || show_cr2 || show_cr3);
    const auto sneak_base_self = view_cr->GetSneakCoefficient();

    for (auto* cr : map->GetCritters()) {
        if (cr == view_cr || cr->IsDestroyed()) {
            continue;
        }

        auto dist = _engine->GeomHelper.DistGame(view_cr->GetHexX(), view_cr->GetHexY(), cr->GetHexX(), cr->GetHexY());

        if (IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_SCRIPT)) {
            const auto allow_self = _engine->OnMapCheckLook.Fire(map, view_cr, cr);
            const auto allow_opp = _engine->OnMapCheckLook.Fire(map, cr, view_cr);

            if (allow_self) {
                if (cr->AddCrIntoVisVec(view_cr)) {
                    view_cr->Send_AddCritter(cr);
                    view_cr->OnCritterAppeared.Fire(cr);
                }
            }
            else {
                if (cr->DelCrFromVisVec(view_cr)) {
                    view_cr->Send_RemoveCritter(cr);
                    view_cr->OnCritterDisappeared.Fire(cr);
                }
            }

            if (allow_opp) {
                if (view_cr->AddCrIntoVisVec(cr)) {
                    cr->Send_AddCritter(view_cr);
                    cr->OnCritterAppeared.Fire(view_cr);
                }
            }
            else {
                if (view_cr->DelCrFromVisVec(cr)) {
                    cr->Send_RemoveCritter(view_cr);
                    cr->OnCritterDisappeared.Fire(view_cr);
                }
            }

            if (show_cr) {
                if (show_cr1) {
                    if (show_cr_dist1 >= dist) {
                        if (view_cr->AddCrIntoVisSet1(cr->GetId())) {
                            view_cr->OnCritterAppearedDist1.Fire(cr);
                        }
                    }
                    else {
                        if (view_cr->DelCrFromVisSet1(cr->GetId())) {
                            view_cr->OnCritterDisappearedDist1.Fire(cr);
                        }
                    }
                }
                if (show_cr2) {
                    if (show_cr_dist2 >= dist) {
                        if (view_cr->AddCrIntoVisSet2(cr->GetId())) {
                            view_cr->OnCritterAppearedDist2.Fire(cr);
                        }
                    }
                    else {
                        if (view_cr->DelCrFromVisSet2(cr->GetId())) {
                            view_cr->OnCritterDisappearedDist2.Fire(cr);
                        }
                    }
                }
                if (show_cr3) {
                    if (show_cr_dist3 >= dist) {
                        if (view_cr->AddCrIntoVisSet3(cr->GetId())) {
                            view_cr->OnCritterAppearedDist3.Fire(cr);
                        }
                    }
                    else {
                        if (view_cr->DelCrFromVisSet3(cr->GetId())) {
                            view_cr->OnCritterDisappearedDist3.Fire(cr);
                        }
                    }
                }
            }

            auto cr_dist = cr->GetShowCritterDist1();
            if (cr_dist != 0u) {
                if (cr_dist >= dist) {
                    if (cr->AddCrIntoVisSet1(view_cr->GetId())) {
                        cr->OnCritterAppearedDist1.Fire(view_cr);
                    }
                }
                else {
                    if (cr->DelCrFromVisSet1(view_cr->GetId())) {
                        cr->OnCritterDisappearedDist1.Fire(view_cr);
                    }
                }
            }
            cr_dist = cr->GetShowCritterDist2();
            if (cr_dist != 0u) {
                if (cr_dist >= dist) {
                    if (cr->AddCrIntoVisSet2(view_cr->GetId())) {
                        cr->OnCritterAppearedDist2.Fire(view_cr);
                    }
                }
                else {
                    if (cr->DelCrFromVisSet2(view_cr->GetId())) {
                        cr->OnCritterDisappearedDist2.Fire(view_cr);
                    }
                }
            }
            cr_dist = cr->GetShowCritterDist3();
            if (cr_dist != 0u) {
                if (cr_dist >= dist) {
                    if (cr->AddCrIntoVisSet3(view_cr->GetId())) {
                        cr->OnCritterAppearedDist1.Fire(view_cr);
                    }
                }
                else {
                    if (cr->DelCrFromVisSet3(view_cr->GetId())) {
                        cr->OnCritterDisappearedDist3.Fire(view_cr);
                    }
                }
            }
            continue;
        }

        auto look_self = look_base_self;
        auto look_opp = cr->GetLookDistance();

        // Dir modifier
        if (IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_DIR)) {
            // Self
            auto real_dir = _engine->GeomHelper.GetFarDir(view_cr->GetHexX(), view_cr->GetHexY(), cr->GetHexX(), cr->GetHexY());
            auto i = (dir_self > real_dir ? dir_self - real_dir : real_dir - dir_self);
            if (i > static_cast<int>(dirs_count / 2u)) {
                i = dirs_count - i;
            }
            look_self -= look_self * _engine->Settings.LookDir[i] / 100u;
            // Opponent
            const int dir_opp = cr->GetDir();
            real_dir = static_cast<uchar>((real_dir + dirs_count / 2u) % dirs_count);
            i = (dir_opp > real_dir ? dir_opp - real_dir : real_dir - dir_opp);
            if (i > static_cast<int>(dirs_count / 2u)) {
                i = dirs_count - i;
            }
            look_opp -= look_opp * _engine->Settings.LookDir[i] / 100u;
        }

        if (dist > look_self && dist > look_opp) {
            dist = std::numeric_limits<uint>::max();
        }

        // Trace
        if (IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_TRACE) && dist != std::numeric_limits<uint>::max()) {
            TraceData trace;
            trace.TraceMap = map;
            trace.BeginHx = view_cr->GetHexX();
            trace.BeginHy = view_cr->GetHexY();
            trace.EndHx = cr->GetHexX();
            trace.EndHy = cr->GetHexY();
            TraceBullet(trace);
            if (!trace.IsFullTrace) {
                dist = std::numeric_limits<uint>::max();
            }
        }

        // Self
        if (cr->GetIsHide() && dist != std::numeric_limits<uint>::max()) {
            auto sneak_opp = cr->GetSneakCoefficient();
            if (IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_SNEAK_DIR)) {
                const auto real_dir = _engine->GeomHelper.GetFarDir(view_cr->GetHexX(), view_cr->GetHexY(), cr->GetHexX(), cr->GetHexY());
                auto i = (dir_self > real_dir ? dir_self - real_dir : real_dir - dir_self);
                if (i > static_cast<int>(dirs_count / 2u)) {
                    i = dirs_count - i;
                }
                sneak_opp -= sneak_opp * _engine->Settings.LookSneakDir[i] / 100;
            }
            sneak_opp /= _engine->Settings.SneakDivider;
            vis = (look_self > sneak_opp ? look_self - sneak_opp : 0u);
        }
        else {
            vis = look_self;
        }

        if (vis < _engine->Settings.LookMinimum) {
            vis = _engine->Settings.LookMinimum;
        }

        if (vis >= dist) {
            if (cr->AddCrIntoVisVec(view_cr)) {
                view_cr->Send_AddCritter(cr);
                view_cr->OnCritterAppeared.Fire(cr);
            }
        }
        else {
            if (cr->DelCrFromVisVec(view_cr)) {
                view_cr->Send_RemoveCritter(cr);
                view_cr->OnCritterDisappeared.Fire(cr);
            }
        }

        if (show_cr1) {
            if (show_cr_dist1 >= dist) {
                if (view_cr->AddCrIntoVisSet1(cr->GetId())) {
                    view_cr->OnCritterAppearedDist1.Fire(cr);
                }
            }
            else {
                if (view_cr->DelCrFromVisSet1(cr->GetId())) {
                    view_cr->OnCritterDisappearedDist1.Fire(cr);
                }
            }
        }
        if (show_cr2) {
            if (show_cr_dist2 >= dist) {
                if (view_cr->AddCrIntoVisSet2(cr->GetId())) {
                    view_cr->OnCritterAppearedDist2.Fire(cr);
                }
            }
            else {
                if (view_cr->DelCrFromVisSet2(cr->GetId())) {
                    view_cr->OnCritterDisappearedDist2.Fire(cr);
                }
            }
        }
        if (show_cr3) {
            if (show_cr_dist3 >= dist) {
                if (view_cr->AddCrIntoVisSet3(cr->GetId())) {
                    view_cr->OnCritterAppearedDist3.Fire(cr);
                }
            }
            else {
                if (view_cr->DelCrFromVisSet3(cr->GetId())) {
                    view_cr->OnCritterDisappearedDist3.Fire(cr);
                }
            }
        }

        // Opponent
        if (view_cr->GetIsHide() && dist != std::numeric_limits<uint>::max()) {
            auto sneak_self = sneak_base_self;
            if (IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_SNEAK_DIR)) {
                const auto dir_opp = cr->GetDir();
                const auto real_dir = _engine->GeomHelper.GetFarDir(cr->GetHexX(), cr->GetHexY(), view_cr->GetHexX(), view_cr->GetHexY());
                auto i = (dir_opp > real_dir ? dir_opp - real_dir : real_dir - dir_opp);
                if (i > static_cast<int>(dirs_count / 2u)) {
                    i = dirs_count - i;
                }
                sneak_self -= sneak_self * _engine->Settings.LookSneakDir[i] / 100;
            }
            sneak_self /= _engine->Settings.SneakDivider;
            vis = (look_opp > sneak_self ? look_opp - sneak_self : 0u);
        }
        else {
            vis = look_opp;
        }
        if (vis < _engine->Settings.LookMinimum) {
            vis = _engine->Settings.LookMinimum;
        }

        if (vis >= dist) {
            if (view_cr->AddCrIntoVisVec(cr)) {
                cr->Send_AddCritter(view_cr);
                cr->OnCritterAppeared.Fire(view_cr);
            }
        }
        else {
            if (view_cr->DelCrFromVisVec(cr)) {
                cr->Send_RemoveCritter(view_cr);
                cr->OnCritterDisappeared.Fire(view_cr);
            }
        }

        auto cr_dist = cr->GetShowCritterDist1();
        if (cr_dist != 0u) {
            if (cr_dist >= dist) {
                if (cr->AddCrIntoVisSet1(view_cr->GetId())) {
                    cr->OnCritterAppearedDist1.Fire(view_cr);
                }
            }
            else {
                if (cr->DelCrFromVisSet1(view_cr->GetId())) {
                    cr->OnCritterDisappearedDist1.Fire(view_cr);
                }
            }
        }
        cr_dist = cr->GetShowCritterDist2();
        if (cr_dist != 0u) {
            if (cr_dist >= dist) {
                if (cr->AddCrIntoVisSet2(view_cr->GetId())) {
                    cr->OnCritterAppearedDist2.Fire(view_cr);
                }
            }
            else {
                if (cr->DelCrFromVisSet2(view_cr->GetId())) {
                    cr->OnCritterDisappearedDist2.Fire(view_cr);
                }
            }
        }
        cr_dist = cr->GetShowCritterDist3();
        if (cr_dist != 0u) {
            if (cr_dist >= dist) {
                if (cr->AddCrIntoVisSet3(view_cr->GetId())) {
                    cr->OnCritterAppearedDist3.Fire(view_cr);
                }
            }
            else {
                if (cr->DelCrFromVisSet3(view_cr->GetId())) {
                    cr->OnCritterDisappearedDist3.Fire(view_cr);
                }
            }
        }
    }
}

void MapManager::ProcessVisibleItems(Critter* view_cr)
{
    NON_CONST_METHOD_HINT();

    if (view_cr->IsDestroyed()) {
        return;
    }

    const auto map_id = view_cr->GetMapId();
    if (map_id == 0u) {
        return;
    }

    auto* map = GetMap(map_id);
    RUNTIME_ASSERT(map);

    const int look = view_cr->GetLookDistance();
    for (auto* item : map->GetItems()) {
        if (item->GetIsHidden()) {
            continue;
        }
        if (item->GetIsAlwaysView()) {
            if (view_cr->AddIdVisItem(item->GetId())) {
                view_cr->Send_AddItemOnMap(item);
                view_cr->OnItemOnMapAppeared.Fire(item, item->ViewPlaceOnMap, item->ViewByCritter);
            }
        }
        else {
            bool allowed;
            if (item->GetIsTrap() && IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_ITEM_SCRIPT)) {
                allowed = _engine->OnMapCheckTrapLook.Fire(map, view_cr, item);
            }
            else {
                int dist = _engine->GeomHelper.DistGame(view_cr->GetHexX(), view_cr->GetHexY(), item->GetHexX(), item->GetHexY());
                if (item->GetIsTrap()) {
                    dist += item->GetTrapValue();
                }
                allowed = look >= dist;
            }

            if (allowed) {
                if (view_cr->AddIdVisItem(item->GetId())) {
                    view_cr->Send_AddItemOnMap(item);
                    view_cr->OnItemOnMapAppeared.Fire(item, item->ViewPlaceOnMap, item->ViewByCritter);
                }
            }
            else {
                if (view_cr->DelIdVisItem(item->GetId())) {
                    view_cr->Send_EraseItemFromMap(item);
                    view_cr->OnItemOnMapDisappeared.Fire(item, item->ViewPlaceOnMap, item->ViewByCritter);
                }
            }
        }
    }
}

void MapManager::ViewMap(Critter* view_cr, Map* map, uint look, ushort hx, ushort hy, int dir)
{
    NON_CONST_METHOD_HINT();

    if (view_cr->IsDestroyed()) {
        return;
    }

    view_cr->Send_GameInfo(map);

    // Critters
    const auto dirs_count = _engine->Settings.MapDirCount;
    for (auto* cr : map->GetCritters()) {
        if (cr == view_cr || cr->IsDestroyed()) {
            continue;
        }

        if (IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_SCRIPT)) {
            if (_engine->OnMapCheckLook.Fire(map, view_cr, cr)) {
                view_cr->Send_AddCritter(cr);
            }
            continue;
        }

        const auto dist = _engine->GeomHelper.DistGame(hx, hy, cr->GetHexX(), cr->GetHexY());
        auto look_self = look;

        // Dir modifier
        if (IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_DIR)) {
            const auto real_dir = _engine->GeomHelper.GetFarDir(hx, hy, cr->GetHexX(), cr->GetHexY());
            auto i = (dir > real_dir ? dir - real_dir : real_dir - dir);
            if (i > static_cast<int>(dirs_count / 2u)) {
                i = dirs_count - i;
            }
            look_self -= look_self * _engine->Settings.LookDir[i] / 100;
        }

        if (dist > look_self) {
            continue;
        }

        // Trace
        if (IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_TRACE) && dist != std::numeric_limits<uint>::max()) {
            TraceData trace;
            trace.TraceMap = map;
            trace.BeginHx = hx;
            trace.BeginHy = hy;
            trace.EndHx = cr->GetHexX();
            trace.EndHy = cr->GetHexY();
            TraceBullet(trace);
            if (!trace.IsFullTrace) {
                continue;
            }
        }

        // Hide modifier
        uint vis;
        if (cr->GetIsHide()) {
            auto sneak_opp = cr->GetSneakCoefficient();
            if (IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_SNEAK_DIR)) {
                const auto real_dir = _engine->GeomHelper.GetFarDir(hx, hy, cr->GetHexX(), cr->GetHexY());
                auto i = (dir > real_dir ? dir - real_dir : real_dir - dir);
                if (i > static_cast<int>(dirs_count / 2u)) {
                    i = dirs_count - i;
                }
                sneak_opp -= sneak_opp * _engine->Settings.LookSneakDir[i] / 100u;
            }
            sneak_opp /= _engine->Settings.SneakDivider;
            vis = (look_self > sneak_opp ? look_self - sneak_opp : 0u);
        }
        else {
            vis = look_self;
        }

        if (vis < _engine->Settings.LookMinimum) {
            vis = _engine->Settings.LookMinimum;
        }
        if (vis >= dist) {
            view_cr->Send_AddCritter(cr);
        }
    }

    // Items
    for (auto* item : map->GetItems()) {
        if (item->GetIsHidden()) {
            continue;
        }
        if (item->GetIsAlwaysView()) {
            view_cr->Send_AddItemOnMap(item);
        }
        else {
            bool allowed;
            if (item->GetIsTrap() && IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_ITEM_SCRIPT)) {
                allowed = _engine->OnMapCheckTrapLook.Fire(map, view_cr, item);
            }
            else {
                auto dist = _engine->GeomHelper.DistGame(hx, hy, item->GetHexX(), item->GetHexY());
                if (item->GetIsTrap()) {
                    dist += item->GetTrapValue();
                }
                allowed = (look >= dist);
            }

            if (allowed) {
                view_cr->Send_AddItemOnMap(item);
            }
        }
    }
}

auto MapManager::CheckKnownLoc(Critter* cr, uint loc_id) const -> bool
{
    for (const auto known_loc_id : cr->GetKnownLocations()) {
        if (known_loc_id == loc_id) {
            return true;
        }
    }
    return false;
}

void MapManager::AddKnownLoc(Critter* cr, uint loc_id)
{
    NON_CONST_METHOD_HINT();

    if (CheckKnownLoc(cr, loc_id)) {
        return;
    }

    auto known_locs = cr->GetKnownLocations();
    known_locs.push_back(loc_id);
    cr->SetKnownLocations(known_locs);
}

void MapManager::EraseKnownLoc(Critter* cr, uint loc_id)
{
    auto known_locs = cr->GetKnownLocations();
    for (size_t i = 0; i < known_locs.size(); i++) {
        if (known_locs[i] == loc_id) {
            known_locs.erase(known_locs.begin() + i);
            cr->SetKnownLocations(known_locs);
            break;
        }
    }
}
