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

#include "MapManager.h"
#include "CritterManager.h"
#include "EntityManager.h"
#include "GenericUtils.h"
#include "ItemManager.h"
#include "LineTracer.h"
#include "Log.h"
#include "ProtoManager.h"
#include "Server.h"
#include "Settings.h"
#include "StringUtils.h"

MapManager::MapManager(FOServer* engine) :
    _engine {engine}
{
    STACK_TRACE_ENTRY();

    _mapGrid = new short[(FPATH_MAX_PATH * 2 + 2) * (FPATH_MAX_PATH * 2 + 2)];
}

MapManager::~MapManager()
{
    STACK_TRACE_ENTRY();

    delete[] _mapGrid;
}

void MapManager::LinkMaps()
{
    STACK_TRACE_ENTRY();

    WriteLog("Link maps");

    int errors = 0;

    // Link maps to locations
    for (auto&& [id, map] : GetMaps()) {
        const auto loc_id = map->GetLocId();
        const auto loc_map_index = map->GetLocMapIndex();

        auto* loc = GetLocation(loc_id);
        if (loc == nullptr) {
            WriteLog("Location {} for map {} not found", loc_id, map->GetName());
            errors++;
            continue;
        }

        auto& loc_maps = loc->GetMapsRaw();
        if (loc_map_index >= static_cast<uint>(loc_maps.size())) {
            loc_maps.resize(loc_map_index + 1);
        }

        loc_maps[loc_map_index] = map;
        map->SetLocation(loc);
    }

    // Verify linkage result
    for (auto&& [id, loc] : GetLocations()) {
        auto& loc_maps = loc->GetMapsRaw();
        for (size_t i = 0; i < loc_maps.size(); i++) {
            if (loc_maps[i] == nullptr) {
                WriteLog("Location {} map index {} is empty", loc->GetName(), i);
                errors++;
                continue;
            }
        }
    }

    if (errors != 0) {
        throw ServerInitException("Link maps failed");
    }
}

void MapManager::LoadFromResources()
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_staticMaps.empty());

    auto map_files = _engine->Resources.FilterFiles("fomapb");
    while (map_files.MoveNext()) {
        auto map_file = map_files.GetCurFile();
        auto reader = DataReader({map_file.GetBuf(), map_file.GetSize()});

        const auto pid = _engine->ToHashedString(map_file.GetName());
        const auto* pmap = _engine->ProtoMngr.GetProtoMap(pid);
        RUNTIME_ASSERT(pmap);

        StaticMap static_map;

        const auto maxhx = pmap->GetWidth();
        const auto maxhy = pmap->GetHeight();

        // Read entities
        {
            vector<uchar> props_data;

            // Read critters
            {
                const auto cr_count = reader.Read<uint>();
                static_map.CritterBillets.reserve(cr_count);

                for (const auto i : xrange(cr_count)) {
                    UNUSED_VARIABLE(i);

                    const auto cr_id = reader.Read<uint>();

                    const auto cr_pid_hash = reader.Read<hstring::hash_t>();
                    const auto cr_pid = _engine->ResolveHash(cr_pid_hash);
                    const auto* cr_proto = _engine->ProtoMngr.GetProtoCritter(cr_pid);

                    auto cr_props = Properties(cr_proto->GetProperties().GetRegistrator());
                    const auto props_data_size = reader.Read<uint>();
                    props_data.resize(props_data_size);
                    reader.ReadPtr<uchar>(props_data.data(), props_data_size);
                    cr_props.RestoreAllData(props_data);

                    auto* cr = new Critter(_engine, ident_t {}, nullptr, cr_proto);
                    cr->SetProperties(cr_props);

                    static_map.CritterBillets.emplace_back(cr_id, cr);

                    // Checks
                    const auto hx = cr->GetHexX();
                    const auto hy = cr->GetHexY();
                    if (hx >= maxhx || hy >= maxhy) {
                        throw MapManagerException("Invalid critter position on map", pmap->GetName(), cr->GetName(), hx, hy);
                    }
                }
            }

            // Read items
            {
                const auto item_count = reader.Read<uint>();
                static_map.ItemBillets.reserve(item_count);

                for (const auto i : xrange(item_count)) {
                    UNUSED_VARIABLE(i);

                    const auto item_id = reader.Read<uint>();

                    const auto item_pid_hash = reader.Read<hstring::hash_t>();
                    const auto item_pid = _engine->ResolveHash(item_pid_hash);
                    const auto* item_proto = _engine->ProtoMngr.GetProtoItem(item_pid);

                    auto item_props = Properties(item_proto->GetProperties().GetRegistrator());
                    const auto props_data_size = reader.Read<uint>();
                    props_data.resize(props_data_size);
                    reader.ReadPtr<uchar>(props_data.data(), props_data_size);
                    item_props.RestoreAllData(props_data);

                    auto* item = new Item(_engine, ident_t {}, item_proto);
                    item->SetProperties(item_props);

                    static_map.ItemBillets.emplace_back(item_id, item);

                    // Checks
                    if (item->GetOwnership() == ItemOwnership::MapHex) {
                        const auto hx = item->GetHexX();
                        const auto hy = item->GetHexY();
                        if (hx >= maxhx || hy >= maxhy) {
                            throw MapManagerException("Invalid item position on map", pmap->GetName(), item->GetName(), hx, hy);
                        }
                    }

                    // Bind scripts
                    if (item->IsStatic()) {
                        if (item->GetSceneryScript()) {
                            item->SceneryScriptFunc = _engine->ScriptSys->FindFunc<bool, Critter*, StaticItem*, Item*, int>(item->GetSceneryScript());
                            if (!item->SceneryScriptFunc) {
                                throw MapManagerException("Can't bind static item scenery function", pmap->GetName(), item->GetSceneryScript());
                            }
                        }

                        if (item->GetTriggerScript()) {
                            item->TriggerScriptFunc = _engine->ScriptSys->FindFunc<void, Critter*, StaticItem*, bool, uchar>(item->GetTriggerScript());
                            if (!item->TriggerScriptFunc) {
                                throw MapManagerException("Can't bind static item trigger function", pmap->GetName(), item->GetTriggerScript());
                            }
                        }
                    }

                    // Sort
                    if (item->IsStatic()) {
                        RUNTIME_ASSERT(item->GetOwnership() == ItemOwnership::MapHex);

                        if (!item->GetIsHiddenInStatic()) {
                            static_map.StaticItems.emplace_back(item);
                        }
                        if (item->GetIsTrigger() || item->GetIsTrap()) {
                            static_map.TriggerItems.emplace_back(item);
                        }
                    }
                    else {
                        if (item->GetOwnership() == ItemOwnership::MapHex) {
                            static_map.HexItemBillets.emplace_back(item_id, item);
                        }
                        else {
                            static_map.ChildItemBillets.emplace_back(item_id, item);
                        }
                    }
                }
            }
        }

        reader.VerifyEnd();

        // Fill hex flags
        static_map.HexFlags.resize(static_cast<size_t>(maxhx * maxhy), 0);

        for (auto&& [item_id, item] : static_map.ItemBillets) {
            if (!item->IsStatic()) {
                continue;
            }

            const auto hx = item->GetHexX();
            const auto hy = item->GetHexY();

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
                    if (_engine->Geometry.MoveHexByDir(hx_, hy_, k, maxhx, maxhy)) {
                        SetBit(static_map.HexFlags[hy_ * maxhx + hx_], FH_BLOCK);
                    }
                }
            }

            if (item->GetIsTrigger() || item->GetIsTrap()) {
                SetBit(static_map.HexFlags[hy * maxhx + hx], FH_STATIC_TRIGGER);
            }

            if (item->IsNonEmptyBlockLines()) {
                const auto shooted = item->GetIsShootThru();
                _engine->Geometry.ForEachBlockLines(item->GetBlockLines(), hx, hy, maxhx, maxhy, [&static_map, maxhx, shooted](auto hx2, auto hy2) {
                    SetBit(static_map.HexFlags[hy2 * maxhx + hx2], FH_BLOCK);
                    if (!shooted) {
                        SetBit(static_map.HexFlags[hy2 * maxhx + hx2], FH_NOTRAKE);
                    }
                });
            }
        }

        static_map.CritterBillets.shrink_to_fit();
        static_map.ItemBillets.shrink_to_fit();
        static_map.HexItemBillets.shrink_to_fit();
        static_map.ChildItemBillets.shrink_to_fit();
        static_map.StaticItems.shrink_to_fit();
        static_map.TriggerItems.shrink_to_fit();

        _staticMaps.emplace(pmap, std::move(static_map));
    }

    RUNTIME_ASSERT(_engine->ProtoMngr.GetProtoMaps().size() == _staticMaps.size());
}

auto MapManager::GetStaticMap(const ProtoMap* proto_map) const -> const StaticMap*
{
    STACK_TRACE_ENTRY();

    const auto it = _staticMaps.find(proto_map);
    RUNTIME_ASSERT(it != _staticMaps.end());
    return &it->second;
}

void MapManager::GenerateMapContent(Map* map)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    unordered_map<ident_t, ident_t> id_map;

    // Generate npc
    for (auto&& [base_cr_id, base_cr] : map->GetStaticMap()->CritterBillets) {
        const auto* npc = _engine->CrMngr.CreateCritter(base_cr->GetProtoId(), &base_cr->GetProperties(), map, base_cr->GetHexX(), base_cr->GetHexY(), base_cr->GetDir(), true);
        if (npc == nullptr) {
            WriteLog("Create npc '{}' on map '{}' fail, continue generate", base_cr->GetName(), map->GetName());
            continue;
        }

        id_map.emplace(base_cr_id, npc->GetId());
    }

    // Generate hex items
    for (auto&& [base_item_id, base_item] : map->GetStaticMap()->HexItemBillets) {
        // Create item
        auto* item = _engine->ItemMngr.CreateItem(base_item->GetProtoId(), 0, &base_item->GetProperties());
        if (item == nullptr) {
            WriteLog("Create item '{}' on map '{}' fail, continue generate", base_item->GetName(), map->GetName());
            continue;
        }
        id_map.emplace(base_item_id, item->GetId());

        // Other values
        if (item->GetIsCanOpen() && item->GetOpened()) {
            item->SetIsLightThru(true);
        }

        if (!map->AddItem(item, base_item->GetHexX(), base_item->GetHexY())) {
            WriteLog("Add item '{}' to map '{}' failure, continue generate", item->GetName(), map->GetName());
            _engine->ItemMngr.DeleteItem(item);
        }
    }

    // Add children items
    for (auto&& [base_item_id, base_item] : map->GetStaticMap()->ChildItemBillets) {
        // Map id
        ident_t parent_id;
        if (base_item->GetOwnership() == ItemOwnership::CritterInventory) {
            parent_id = base_item->GetCritterId();
        }
        else if (base_item->GetOwnership() == ItemOwnership::ItemContainer) {
            parent_id = base_item->GetContainerId();
        }
        else {
            throw UnreachablePlaceException(LINE_STR);
        }

        if (id_map.count(parent_id) == 0) {
            continue;
        }
        parent_id = id_map[parent_id];

        // Create item
        auto* item = _engine->ItemMngr.CreateItem(base_item->GetProtoId(), 0, &base_item->GetProperties());
        if (item == nullptr) {
            WriteLog("Create item '{}' on map '{}' fail, continue generate", base_item->GetName(), map->GetName());
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
    STACK_TRACE_ENTRY();

    while (!map->_critters.empty() || !map->_items.empty()) {
        // Transit players to global map
        KickPlayersToGlobalMap(map);

        // Delete npc
        auto del_npcs = map->_nonPlayerCritters;
        for (auto* del_npc : del_npcs) {
            _engine->CrMngr.DeleteCritter(del_npc);
        }

        // Delete items
        auto del_items = map->_items;
        for (auto* del_item : del_items) {
            _engine->ItemMngr.DeleteItem(del_item);
        }
    }

    RUNTIME_ASSERT(map->_itemsMap.empty());
    RUNTIME_ASSERT(map->_itemsByHex.empty());
    RUNTIME_ASSERT(map->_blockLinesByHex.empty());
}

auto MapManager::GetLocationAndMapsStatistics() const -> string
{
    STACK_TRACE_ENTRY();

    const auto& locations = _engine->EntityMngr.GetLocations();
    const auto& maps = _engine->EntityMngr.GetMaps();

    string result = _str("Locations count: {}\n", static_cast<uint>(locations.size()));
    result += _str("Maps count: {}\n", static_cast<uint>(maps.size()));
    result += "Location             Id           X     Y     Radius Color    Hidden  GeckVisible GeckCount AutoGarbage ToGarbage\n";
    result += "          Map                 Id          Time Rain Script\n";

    for (auto&& [id, loc] : locations) {
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
    STACK_TRACE_ENTRY();

    const auto* proto = _engine->ProtoMngr.GetProtoLocation(proto_id);
    if (proto == nullptr) {
        throw MapManagerException("Location proto is not loaded", proto_id);
    }

    if (wx >= GM_MAXZONEX * _engine->Settings.GlobalMapZoneLength || wy >= GM_MAXZONEY * _engine->Settings.GlobalMapZoneLength) {
        throw MapManagerException("Invalid location coordinates", proto_id);
    }

    auto* loc = new Location(_engine, ident_t {}, proto);

    loc->SetWorldX(wx);
    loc->SetWorldY(wy);

    vector<ident_t> map_ids;

    for (const auto map_pid : loc->GetMapProtos()) {
        const auto* map = CreateMap(map_pid, loc);
        if (map == nullptr) {
            for (const auto* map2 : loc->GetMapsRaw()) {
                map2->Release();
            }
            loc->Release();

            throw MapManagerException("Create map for location failed", proto_id, map_pid);
        }

        RUNTIME_ASSERT(std::find(map_ids.begin(), map_ids.end(), map->GetId()) == map_ids.end());
        map_ids.emplace_back(map->GetId());
    }

    loc->SetMapIds(map_ids);
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
    STACK_TRACE_ENTRY();

    const auto* proto_map = _engine->ProtoMngr.GetProtoMap(proto_id);
    if (proto_map == nullptr) {
        throw MapManagerException("Proto map is not loaded", proto_id);
    }

    const auto it = _staticMaps.find(proto_map);
    if (it == _staticMaps.end()) {
        throw MapManagerException("Static map not found", proto_id);
    }

    auto* map = new Map(_engine, ident_t {}, proto_map, loc, &it->second);
    map->SetLocId(loc->GetId());

    auto& maps = loc->GetMapsRaw();
    map->SetLocMapIndex(static_cast<uint>(maps.size()));
    maps.push_back(map);

    _engine->EntityMngr.RegisterEntity(map);
    return map;
}

void MapManager::RegenerateMap(Map* map)
{
    STACK_TRACE_ENTRY();

    _engine->OnMapFinish.Fire(map);

    DeleteMapContent(map);
    GenerateMapContent(map);

    for (auto* item : map->GetItems()) {
        _engine->OnItemInit.Fire(item, true);
    }

    _engine->OnMapInit.Fire(map, true);
}

auto MapManager::GetMap(ident_t map_id) -> Map*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (!map_id) {
        return nullptr;
    }

    return _engine->EntityMngr.GetMap(map_id);
}

auto MapManager::GetMap(ident_t map_id) const -> const Map*
{
    STACK_TRACE_ENTRY();

    return const_cast<MapManager*>(this)->GetMap(map_id);
}

auto MapManager::GetMapByPid(hstring map_pid, uint skip_count) -> Map*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (!map_pid) {
        return nullptr;
    }

    return _engine->EntityMngr.GetMapByPid(map_pid, skip_count);
}

auto MapManager::GetMaps() -> const unordered_map<ident_t, Map*>&
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    return _engine->EntityMngr.GetMaps();
}

auto MapManager::GetMapsCount() const -> uint
{
    STACK_TRACE_ENTRY();

    return static_cast<uint>(_engine->EntityMngr.GetMaps().size());
}

auto MapManager::GetLocationByMap(ident_t map_id) -> Location*
{
    STACK_TRACE_ENTRY();

    auto* map = GetMap(map_id);
    if (map == nullptr) {
        return nullptr;
    }
    return map->GetLocation();
}

auto MapManager::GetLocation(ident_t loc_id) -> Location*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (!loc_id) {
        return nullptr;
    }

    return _engine->EntityMngr.GetLocation(loc_id);
}

auto MapManager::GetLocation(ident_t loc_id) const -> const Location*
{
    STACK_TRACE_ENTRY();

    return const_cast<MapManager*>(this)->GetLocation(loc_id);
}

auto MapManager::GetLocationByPid(hstring loc_pid, uint skip_count) -> Location*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (!loc_pid) {
        return nullptr;
    }
    return _engine->EntityMngr.GetLocationByPid(loc_pid, skip_count);
}

auto MapManager::IsIntersectZone(int wx1, int wy1, int w1_radius, int wx2, int wy2, int w2_radius, int zones) const -> bool
{
    STACK_TRACE_ENTRY();

    const int zl = _engine->Settings.GlobalMapZoneLength;
    const IRect r1((wx1 - w1_radius) / zl - zones, (wy1 - w1_radius) / zl - zones, (wx1 + w1_radius) / zl + zones, (wy1 + w1_radius) / zl + zones);
    const IRect r2((wx2 - w2_radius) / zl, (wy2 - w2_radius) / zl, (wx2 + w2_radius) / zl, (wy2 + w2_radius) / zl);
    return r1.Left <= r2.Right && r2.Left <= r1.Right && r1.Top <= r2.Bottom && r2.Top <= r1.Bottom;
}

auto MapManager::GetZoneLocations(int zx, int zy, int zone_radius) -> vector<Location*>
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto wx = zx * static_cast<int>(_engine->Settings.GlobalMapZoneLength);
    const auto wy = zy * static_cast<int>(_engine->Settings.GlobalMapZoneLength);

    vector<Location*> locs;
    for (auto&& [id, loc] : _engine->EntityMngr.GetLocations()) {
        if (!loc->IsDestroyed() && loc->IsLocVisible() && IsIntersectZone(wx, wy, 0, loc->GetWorldX(), loc->GetWorldY(), loc->GetRadius(), zone_radius)) {
            locs.push_back(loc);
        }
    }

    return locs;
}

void MapManager::KickPlayersToGlobalMap(Map* map)
{
    STACK_TRACE_ENTRY();

    for (auto* cl : map->GetPlayers()) {
        TransitToGlobal(cl, ident_t {});
    }
}

auto MapManager::GetLocations() -> const unordered_map<ident_t, Location*>&
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    return _engine->EntityMngr.GetLocations();
}

auto MapManager::GetLocationsCount() const -> uint
{
    STACK_TRACE_ENTRY();

    return static_cast<uint>(_engine->EntityMngr.GetLocations().size());
}

void MapManager::LocationGarbager()
{
    STACK_TRACE_ENTRY();

    if (_runGarbager) {
        _runGarbager = false;

        for (auto&& [id, loc] : copy(_engine->EntityMngr.GetLocations())) {
            if (!loc->IsDestroyed() && loc->GetAutoGarbage() && loc->IsCanDelete()) {
                DeleteLocation(loc);
            }
        }
    }
}

void MapManager::DeleteLocation(Location* loc)
{
    STACK_TRACE_ENTRY();

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

    // Inform players on global map about this
    for (auto* cr : _engine->CrMngr.GetPlayerCritters(true)) {
        if (CheckKnownLoc(cr, loc->GetId())) {
            cr->Send_GlobalLocation(loc, false);
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

    // Invalidate
    for (auto* map : maps) {
        map->MarkAsDestroyed();
    }
    loc->MarkAsDestroyed();

    // Release
    for (const auto* map : maps) {
        map->Release();
    }
    loc->Release();
}

void MapManager::TraceBullet(TraceData& trace)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    auto* map = trace.TraceMap;
    const auto maxhx = map->GetWidth();
    const auto maxhy = map->GetHeight();
    const auto hx = trace.BeginHx;
    const auto hy = trace.BeginHy;
    const auto tx = trace.EndHx;
    const auto ty = trace.EndHy;

    auto dist = trace.Dist;
    if (dist == 0) {
        dist = _engine->Geometry.DistGame(hx, hy, tx, ty);
    }

    auto cx = hx;
    auto cy = hy;
    auto old_cx = cx;
    auto old_cy = cy;

    LineTracer line_tracer(_engine->Geometry, hx, hy, tx, ty, maxhx, maxhy, trace.Angle);

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
        if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
            dir = line_tracer.GetNextHex(cx, cy);
        }
        else {
            line_tracer.GetNextSquare(cx, cy);
            dir = _engine->Geometry.GetNearDir(old_cx, old_cy, cx, cy);
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

auto MapManager::FindPath(const FindPathInput& input) -> FindPathOutput
{
    STACK_TRACE_ENTRY();

    FindPathOutput output;

    // Checks
    if (input.TraceDist > 0 && input.TraceCr == nullptr) {
        output.Result = FindPathOutput::ResultType::TraceTargetNullptr;
        return output;
    }

    auto* map = GetMap(input.MapId);
    if (map == nullptr) {
        output.Result = FindPathOutput::ResultType::MapNotFound;
        return output;
    }

    const auto maxhx = map->GetWidth();
    const auto maxhy = map->GetHeight();

    if (input.FromHexX >= maxhx || input.FromHexY >= maxhy || input.ToHexX >= maxhx || input.ToHexY >= maxhy) {
        output.Result = FindPathOutput::ResultType::InvalidHexes;
        return output;
    }
    if (_engine->Geometry.CheckDist(input.FromHexX, input.FromHexY, input.ToHexX, input.ToHexY, input.Cut)) {
        output.Result = FindPathOutput::ResultType::AlreadyHere;
        return output;
    }
    if (input.Cut == 0 && IsBitSet(map->GetHexFlags(input.ToHexX, input.ToHexY), FH_NOWAY)) {
        output.Result = FindPathOutput::ResultType::HexBusy;
        return output;
    }

    // Ring check
    if (input.Cut <= 1u && input.Multihex == 0) {
        auto [rsx, rsy] = _engine->Geometry.GetHexOffsets((input.ToHexX % 2) != 0);

        auto i = 0;
        for (; i < GameSettings::MAP_DIR_COUNT; i++, rsx++, rsy++) {
            const auto xx = static_cast<int>(input.ToHexX + *rsx);
            const auto yy = static_cast<int>(input.ToHexY + *rsy);
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

        if (i == GameSettings::MAP_DIR_COUNT) {
            output.Result = FindPathOutput::ResultType::HexBusyRing;
            return output;
        }
    }

    // Prepare
    _mapGridOffsX = input.FromHexX;
    _mapGridOffsY = input.FromHexY;

    short numindex = 1;
    std::memset(_mapGrid, 0, (FPATH_MAX_PATH * 2 + 2) * (FPATH_MAX_PATH * 2 + 2) * sizeof(short));
    GridAt(input.FromHexX, input.FromHexY) = numindex;

    auto to_hx = input.ToHexX;
    auto to_hy = input.ToHexY;

    vector<pair<ushort, ushort>> coords;
    vector<pair<ushort, ushort>> cr_coords;
    vector<pair<ushort, ushort>> gag_coords;
    coords.reserve(10000);
    cr_coords.reserve(100);
    gag_coords.reserve(100);

    // First point
    coords.emplace_back(input.FromHexX, input.FromHexY);

    // Begin search
    auto p = 0;
    auto p_togo = 1;
    ushort cx = 0;
    ushort cy = 0;
    while (true) {
        for (auto i = 0; i < p_togo; i++, p++) {
            cx = coords[p].first;
            cy = coords[p].second;
            numindex = GridAt(cx, cy);

            if (_engine->Geometry.CheckDist(cx, cy, to_hx, to_hy, input.Cut)) {
                to_hx = cx;
                to_hy = cy;
                goto label_FindOk;
            }
            if (++numindex > FPATH_MAX_PATH) {
                output.Result = FindPathOutput::ResultType::TooFar;
                return output;
            }

            const auto [sx, sy] = _engine->Geometry.GetHexOffsets((cx & 1) != 0);

            for (auto j = 0; j < GameSettings::MAP_DIR_COUNT; j++) {
                const auto nxi = cx + sx[j];
                const auto nyi = cy + sy[j];
                if (nxi < 0 || nyi < 0 || nxi >= maxhx || nyi >= maxhy) {
                    continue;
                }

                const auto nx = static_cast<ushort>(nxi);
                const auto ny = static_cast<ushort>(nyi);
                auto& grid_cell = GridAt(nx, ny);
                if (grid_cell != 0) {
                    continue;
                }

                if (input.Multihex == 0) {
                    auto flags = map->GetHexFlags(nx, ny);
                    if (!IsBitSet(flags, FH_NOWAY)) {
                        coords.emplace_back(nx, ny);
                        grid_cell = numindex;
                    }
                    else if (input.CheckGagItems && IsBitSet(flags, static_cast<ushort>(FH_GAG_ITEM << 8))) {
                        gag_coords.emplace_back(nx, ny);
                        grid_cell = static_cast<short>(numindex | 0x4000);
                    }
                    else if (input.CheckCritter && IsBitSet(flags, static_cast<ushort>(FH_CRITTER << 8))) {
                        cr_coords.emplace_back(nx, ny);
                        grid_cell = static_cast<short>(numindex | 0x8000);
                    }
                    else {
                        grid_cell = -1;
                    }
                }
                else {
                    if (map->IsMovePassed(input.FromCritter, nx, ny, input.Multihex)) {
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
            auto last_index = GridAt(coords.back().first, coords.back().second);
            auto& xy = gag_coords.front();
            auto gag_index = static_cast<short>(GridAt(xy.first, xy.second) ^ static_cast<short>(0x4000));
            if (gag_index + 10 < last_index) // Todo: if path finding not be reworked than migrate magic number to scripts
            {
                GridAt(xy.first, xy.second) = gag_index;
                coords.push_back(xy);
                gag_coords.erase(gag_coords.begin());
            }
        }

        // Add gag and critters hexes
        p_togo = static_cast<int>(coords.size()) - p;
        if (p_togo == 0) {
            if (!gag_coords.empty()) {
                auto& xy = gag_coords.front();
                GridAt(xy.first, xy.second) ^= 0x4000;
                coords.push_back(xy);
                gag_coords.erase(gag_coords.begin());
                p_togo++;
            }
            else if (!cr_coords.empty()) {
                auto& xy = cr_coords.front();
                GridAt(xy.first, xy.second) ^= static_cast<short>(0x8000);
                coords.push_back(xy);
                cr_coords.erase(cr_coords.begin());
                p_togo++;
            }
        }

        if (p_togo == 0) {
            output.Result = FindPathOutput::ResultType::Deadlock;
            return output;
        }
    }

label_FindOk:
    vector<uchar> raw_steps;
    raw_steps.resize(numindex - 1);

    // Smooth data
    float base_angle = _engine->Geometry.GetDirAngle(to_hx, to_hy, input.FromHexX, input.FromHexY);

    while (numindex > 1) {
        numindex--;

        const auto find_path_grid = [this, &input, base_angle, &raw_steps, &numindex](ushort& x1, ushort& y1) -> bool {
            int best_step_dir = -1;
            float best_step_angle_diff = 0.0f;

            const auto check_hex = [&best_step_dir, &best_step_angle_diff, &input, numindex, base_angle, this](int dir, int step_hx, int step_hy) {
                if (GridAt(step_hx, step_hy) == numindex) {
                    const float angle = _engine->Geometry.GetDirAngle(step_hx, step_hy, input.FromHexX, input.FromHexY);
                    const float angle_diff = _engine->Geometry.GetDirAngleDiff(base_angle, angle);
                    if (best_step_dir == -1 || numindex == 0) {
                        best_step_dir = dir;
                        best_step_angle_diff = _engine->Geometry.GetDirAngleDiff(base_angle, angle);
                    }
                    else {
                        if (best_step_dir == -1 || angle_diff < best_step_angle_diff) {
                            best_step_dir = dir;
                            best_step_angle_diff = angle_diff;
                        }
                    }
                }
            };

            if ((x1 % 2) != 0) {
                check_hex(3, x1 - 1, y1 - 1);
                check_hex(2, x1, y1 - 1);
                check_hex(5, x1, y1 + 1);
                check_hex(0, x1 + 1, y1);
                check_hex(4, x1 - 1, y1);
                check_hex(1, x1 + 1, y1 - 1);

                if (best_step_dir == 3) {
                    raw_steps[numindex - 1] = 3;
                    x1--;
                    y1--;
                    return true;
                }
                if (best_step_dir == 2) {
                    raw_steps[numindex - 1] = 2;
                    y1--;
                    return true;
                }
                if (best_step_dir == 5) {
                    raw_steps[numindex - 1] = 5;
                    y1++;
                    return true;
                }
                if (best_step_dir == 0) {
                    raw_steps[numindex - 1] = 0;
                    x1++;
                    return true;
                }
                if (best_step_dir == 4) {
                    raw_steps[numindex - 1] = 4;
                    x1--;
                    return true;
                }
                if (best_step_dir == 1) {
                    raw_steps[numindex - 1] = 1;
                    x1++;
                    y1--;
                    return true;
                }
            }
            else {
                check_hex(3, x1 - 1, y1);
                check_hex(2, x1, y1 - 1);
                check_hex(5, x1, y1 + 1);
                check_hex(0, x1 + 1, y1 + 1);
                check_hex(4, x1 - 1, y1 + 1);
                check_hex(1, x1 + 1, y1);

                if (best_step_dir == 3) {
                    raw_steps[numindex - 1] = 3;
                    x1--;
                    return true;
                }
                if (best_step_dir == 2) {
                    raw_steps[numindex - 1] = 2;
                    y1--;
                    return true;
                }
                if (best_step_dir == 5) {
                    raw_steps[numindex - 1] = 5;
                    y1++;
                    return true;
                }
                if (best_step_dir == 0) {
                    raw_steps[numindex - 1] = 0;
                    x1++;
                    y1++;
                    return true;
                }
                if (best_step_dir == 4) {
                    raw_steps[numindex - 1] = 4;
                    x1--;
                    y1++;
                    return true;
                }
                if (best_step_dir == 1) {
                    raw_steps[numindex - 1] = 1;
                    x1++;
                    return true;
                }
            }

            return false;
        };

        if (!find_path_grid(cx, cy)) {
            output.Result = FindPathOutput::ResultType::InternalError;
            return output;
        }
    }

    // Check for closed door and critter
    if (input.CheckCritter || input.CheckGagItems) {
        ushort check_hx = input.FromHexX;
        ushort check_hy = input.FromHexY;

        for (size_t i = 0; i < raw_steps.size(); i++) {
            const auto prev_check_hx = check_hx;
            const auto prev_check_hy = check_hy;

            const auto move_ok = _engine->Geometry.MoveHexByDir(check_hx, check_hy, raw_steps[i], maxhx, maxhy);
            RUNTIME_ASSERT(move_ok);

            if (map->IsHexPassed(check_hx, check_hy)) {
                continue;
            }

            if (input.CheckGagItems && map->IsHexGag(check_hx, check_hy)) {
                Item* item = map->GetItemGag(check_hx, check_hy);
                if (item == nullptr) {
                    continue;
                }

                output.GagItem = item;
                to_hx = prev_check_hx;
                to_hy = prev_check_hy;
                raw_steps.resize(i);
                break;
            }

            if (input.CheckCritter && map->IsFlagCritter(check_hx, check_hy, false)) {
                Critter* cr = map->GetHexCritter(check_hx, check_hy, false);
                if (cr == nullptr || cr == input.FromCritter) {
                    continue;
                }

                output.GagCritter = cr;
                to_hx = prev_check_hx;
                to_hy = prev_check_hy;
                raw_steps.resize(i);
                break;
            }
        }
    }

    // Trace
    if (input.TraceDist > 0) {
        vector<int> trace_seq;
        ushort targ_hx = input.TraceCr->GetHexX();
        ushort targ_hy = input.TraceCr->GetHexY();
        bool trace_ok = false;

        trace_seq.resize(raw_steps.size() + 4);

        ushort check_hx = input.FromHexX;
        ushort check_hy = input.FromHexY;

        for (size_t i = 0; i < raw_steps.size(); i++) {
            const auto move_ok = _engine->Geometry.MoveHexByDir(check_hx, check_hy, raw_steps[i], maxhx, maxhy);
            RUNTIME_ASSERT(move_ok);

            if (map->IsHexGag(check_hx, check_hy)) {
                trace_seq[i + 2 - 2] += 1;
                trace_seq[i + 2 - 1] += 2;
                trace_seq[i + 2 - 0] += 3;
                trace_seq[i + 2 + 1] += 2;
                trace_seq[i + 2 + 2] += 1;
            }
        }

        TraceData trace;
        trace.TraceMap = map;
        trace.EndHx = targ_hx;
        trace.EndHy = targ_hy;
        trace.FindCr = input.TraceCr;
        for (int k = 0; k < 5; k++) {
            ushort check_hx2 = input.FromHexX;
            ushort check_hy2 = input.FromHexY;

            for (size_t i = 0; i < raw_steps.size() - 1; i++) {
                const auto move_ok2 = _engine->Geometry.MoveHexByDir(check_hx2, check_hy2, raw_steps[i], maxhx, maxhy);
                RUNTIME_ASSERT(move_ok2);

                if (k < 4 && trace_seq[i + 2] != k) {
                    continue;
                }
                if (k == 4 && trace_seq[i + 2] < 4) {
                    continue;
                }

                if (!_engine->Geometry.CheckDist(check_hx2, check_hy2, targ_hx, targ_hy, input.TraceDist)) {
                    continue;
                }

                trace.BeginHx = check_hx2;
                trace.BeginHy = check_hy2;

                TraceBullet(trace);

                if (trace.IsCritterFounded) {
                    trace_ok = true;
                    raw_steps.resize(i + 1);
                    const auto move_ok3 = _engine->Geometry.MoveHexByDir(check_hx2, check_hy2, raw_steps[i + 1], maxhx, maxhy);
                    RUNTIME_ASSERT(move_ok3);
                    to_hx = check_hx2;
                    to_hy = check_hy2;
                    goto label_TraceOk;
                }
            }
        }

        if (!trace_ok && output.GagItem == nullptr && output.GagCritter == nullptr) {
            output.Result = FindPathOutput::ResultType::TraceFailed;
            return output;
        }

    label_TraceOk:
        if (trace_ok) {
            output.GagItem = nullptr;
            output.GagCritter = nullptr;
        }
    }

    if (raw_steps.empty()) {
        output.Result = FindPathOutput::ResultType::AlreadyHere;
        return output;
    }

    if (_engine->Settings.MapFreeMovement) {
        ushort trace_hx = input.FromHexX;
        ushort trace_hy = input.FromHexY;

        while (true) {
            ushort trace_tx = to_hx;
            ushort trace_ty = to_hy;

            for (auto i = static_cast<int>(raw_steps.size()) - 1; i >= 0; i--) {
                LineTracer tracer(_engine->Geometry, trace_hx, trace_hy, trace_tx, trace_ty, maxhx, maxhy, 0.0f);
                ushort next_hx = trace_hx;
                ushort next_hy = trace_hy;
                vector<uchar> direct_steps;
                bool failed = false;

                while (true) {
                    uchar dir = tracer.GetNextHex(next_hx, next_hy);
                    direct_steps.push_back(dir);

                    if (next_hx == trace_tx && next_hy == trace_ty) {
                        break;
                    }

                    if (GridAt(next_hx, next_hy) <= 0) {
                        failed = true;
                        break;
                    }
                }

                if (failed) {
                    RUNTIME_ASSERT(i > 0);
                    _engine->Geometry.MoveHexByDir(trace_tx, trace_ty, _engine->Geometry.ReverseDir(raw_steps[i]), maxhx, maxhy);
                    continue;
                }

                for (const auto& ds : direct_steps) {
                    output.Steps.push_back(ds);
                }

                output.ControlSteps.emplace_back(static_cast<ushort>(output.Steps.size()));

                trace_hx = trace_tx;
                trace_hy = trace_ty;
                break;
            }

            if (trace_tx == to_hx && trace_ty == to_hy) {
                break;
            }
        }
    }
    else {
        for (size_t i = 0; i < raw_steps.size(); i++) {
            const auto cur_dir = raw_steps[i];
            output.Steps.push_back(cur_dir);

            for (size_t j = i + 1; j < raw_steps.size(); j++) {
                if (raw_steps[j] == cur_dir) {
                    output.Steps.push_back(cur_dir);
                    i++;
                }
                else {
                    break;
                }
            }

            output.ControlSteps.emplace_back(static_cast<ushort>(output.Steps.size()));
        }
    }

    RUNTIME_ASSERT(!output.Steps.empty());
    RUNTIME_ASSERT(!output.ControlSteps.empty());

    output.Result = FindPathOutput::ResultType::Ok;
    output.NewToX = to_hx;
    output.NewToY = to_hy;
    return output;
}

auto MapManager::TransitToGlobal(Critter* cr, ident_t leader_id) -> bool
{
    STACK_TRACE_ENTRY();

    if (cr->LockMapTransfers != 0) {
        WriteLog("Transfers locked, critter '{}'", cr->GetName());
        return false;
    }

    return Transit(cr, nullptr, 0, 0, 0, 0, leader_id);
}

auto MapManager::Transit(Critter* cr, Map* map, ushort hx, ushort hy, uchar dir, uint radius, ident_t leader_id) -> bool
{
    STACK_TRACE_ENTRY();

    // Check location deletion
    const auto* loc = map != nullptr ? map->GetLocation() : nullptr;
    if (loc != nullptr && loc->GetToGarbage()) {
        WriteLog("Transfer to deleted location, critter '{}'", cr->GetName());
        return false;
    }

    // Maybe critter already in transfer
    if (cr->LockMapTransfers != 0) {
        WriteLog("Transfers locked, critter '{}'", cr->GetName());
        return false;
    }

    const auto map_id = map != nullptr ? map->GetId() : ident_t {};

    const auto cur_map_id = cr->GetMapId();
    auto* cur_map = cur_map_id ? GetMap(cur_map_id) : nullptr;
    RUNTIME_ASSERT(!cur_map_id || !!cur_map);

    cr->ClearMove();

    if (cur_map_id == map_id) {
        // One map
        if (!map_id) {
            // Todo: check group
            return true;
        }

        if (hx >= map->GetWidth() || hy >= map->GetHeight()) {
            return false;
        }

        const auto multihex = cr->GetMultihex();

        auto start_hex = map->FindStartHex(hx, hy, multihex, radius, true);
        if (!start_hex.has_value()) {
            start_hex = map->FindStartHex(hx, hy, multihex, radius, false);
        }
        if (!start_hex.has_value()) {
            return false;
        }

        const auto fixed_hx = std::get<0>(start_hex.value());
        const auto fixed_hy = std::get<1>(start_hex.value());

        cr->LockMapTransfers++;
        auto enable_transfers = ScopeCallback([cr]() noexcept { cr->LockMapTransfers--; });

        cr->ChangeDir(dir);
        map->UnsetFlagCritter(cr->GetHexX(), cr->GetHexY(), multihex, cr->IsDead());
        cr->SetHexX(fixed_hx);
        cr->SetHexY(fixed_hy);
        map->SetFlagCritter(fixed_hx, fixed_hy, multihex, cr->IsDead());
        cr->Send_Teleport(cr, cr->GetHexX(), cr->GetHexY());
        cr->Broadcast_Teleport(cr->GetHexX(), cr->GetHexY());
        cr->ClearVisible();
        ProcessVisibleCritters(cr);
        ProcessVisibleItems(cr);
        cr->Send_Position(cr);
    }
    else {
        // Different maps
        ushort fixed_hx = 0;
        ushort fixed_hy = 0;

        if (map != nullptr) {
            const auto multihex = cr->GetMultihex();

            auto start_hex = map->FindStartHex(hx, hy, multihex, radius, true);
            if (!start_hex.has_value()) {
                start_hex = map->FindStartHex(hx, hy, multihex, radius, false);
            }
            if (!start_hex.has_value()) {
                return false;
            }

            fixed_hx = std::get<0>(start_hex.value());
            fixed_hy = std::get<1>(start_hex.value());
        }

        if (!CanAddCrToMap(cr, map, fixed_hx, fixed_hy, leader_id)) {
            return false;
        }

        cr->LockMapTransfers++;
        auto enable_transfers = ScopeCallback([cr]() noexcept { cr->LockMapTransfers--; });

        cr->SetMapLeaveHexX(cr->GetHexX());
        cr->SetMapLeaveHexY(cr->GetHexY());

        EraseCrFromMap(cr, cur_map);

        AddCrToMap(cr, map, fixed_hx, fixed_hy, dir, leader_id);

        cr->Send_LoadMap(nullptr);
        cr->Send_AddCritter(cr);
        cr->Send_AddAllItems();

        ProcessVisibleCritters(cr);
        ProcessVisibleItems(cr);

        cr->Send_PlaceToGameComplete();
    }

    return true;
}

auto MapManager::CanAddCrToMap(Critter* cr, Map* map, ushort hx, ushort hy, ident_t leader_id) const -> bool
{
    STACK_TRACE_ENTRY();

    if (map != nullptr) {
        if (hx >= map->GetWidth() || hy >= map->GetHeight()) {
            return false;
        }
        if (!map->IsHexesPassed(hx, hy, cr->GetMultihex())) {
            return false;
        }
        if (!map->GetLocation()->IsCanEnter(1)) {
            return false;
        }
    }
    else {
        if (leader_id && leader_id != cr->GetId()) {
            const auto* leader = _engine->CrMngr.GetCritter(leader_id);
            if (leader == nullptr || leader->GetMapId()) {
                return false;
            }
        }
    }
    return true;
}

void MapManager::AddCrToMap(Critter* cr, Map* map, ushort hx, ushort hy, uchar dir, ident_t leader_id)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    cr->LockMapTransfers++;
    auto decrease_transfers = ScopeCallback([cr]() noexcept { cr->LockMapTransfers--; });

    if (map != nullptr) {
        RUNTIME_ASSERT(hx < map->GetWidth() && hy < map->GetHeight());

        cr->SetMapId(map->GetId());
        cr->SetHexX(hx);
        cr->SetHexY(hy);
        cr->ChangeDir(dir);

        cr->SetLastLocationId(map->GetLocation()->GetId());
        cr->SetLastLocationPid(map->GetLocation()->GetProtoId());
        cr->SetLastMapId(map->GetId());
        cr->SetLastMapPid(map->GetProtoId());

        if (!cr->IsOwnedByPlayer()) {
            auto cr_ids = map->GetCritterIds();
            RUNTIME_ASSERT(std::find(cr_ids.begin(), cr_ids.end(), cr->GetId()) == cr_ids.end());
            cr_ids.emplace_back(cr->GetId());
            map->SetCritterIds(cr_ids);
        }

        map->AddCritter(cr);

        _engine->OnMapCritterIn.Fire(map, cr);
    }
    else {
        RUNTIME_ASSERT(!cr->GlobalMapGroup);

        if (!leader_id || leader_id == cr->GetId()) {
            cr->SetGlobalMapLeaderId(cr->GetId());
            cr->SetGlobalMapTripId(cr->GetGlobalMapTripId() + 1);

            cr->SetLastGlobalMapLeaderId(cr->GetId());

            cr->GlobalMapGroup = new vector<Critter*>();
            cr->GlobalMapGroup->push_back(cr);
        }
        else {
            const auto* leader = _engine->CrMngr.GetCritter(leader_id);
            RUNTIME_ASSERT(leader);
            RUNTIME_ASSERT(!leader->GetMapId());
            RUNTIME_ASSERT(leader->GlobalMapGroup);

            cr->SetWorldX(leader->GetWorldX());
            cr->SetWorldY(leader->GetWorldY());
            cr->SetGlobalMapLeaderId(leader_id);
            cr->SetGlobalMapTripId(leader->GetGlobalMapTripId());

            cr->SetLastGlobalMapLeaderId(leader_id);

            for (auto* group_cr : *leader->GlobalMapGroup) {
                group_cr->Send_AddCritter(cr);
            }

            cr->GlobalMapGroup = leader->GlobalMapGroup;
            cr->GlobalMapGroup->push_back(cr);
        }

        _engine->OnGlobalMapCritterIn.Fire(cr);
    }
}

void MapManager::EraseCrFromMap(Critter* cr, Map* map)
{
    STACK_TRACE_ENTRY();

    cr->LockMapTransfers++;
    auto decrease_transfers = ScopeCallback([cr]() noexcept { cr->LockMapTransfers--; });

    if (map != nullptr) {
        RUNTIME_ASSERT(cr->GetMapId() == map->GetId());

        _engine->OnMapCritterOut.Fire(map, cr);

        for (auto* other : copy(cr->VisCr)) {
            other->OnCritterDisappeared.Fire(cr);
        }

        cr->ClearVisible();
        map->EraseCritter(cr);
        map->UnsetFlagCritter(cr->GetHexX(), cr->GetHexY(), cr->GetMultihex(), cr->IsDead());

        cr->SetMapId(ident_t {});

        if (!cr->IsOwnedByPlayer()) {
            auto cr_ids = map->GetCritterIds();
            const auto cr_id_it = std::find(cr_ids.begin(), cr_ids.end(), cr->GetId());
            RUNTIME_ASSERT(cr_id_it != cr_ids.end());
            cr_ids.erase(cr_id_it);
            map->SetCritterIds(std::move(cr_ids));
        }

        _runGarbager = true;
    }
    else {
        RUNTIME_ASSERT(!cr->GetMapId());
        RUNTIME_ASSERT(cr->GlobalMapGroup);

        _engine->OnGlobalMapCritterOut.Fire(cr);

        const auto it = std::find(cr->GlobalMapGroup->begin(), cr->GlobalMapGroup->end(), cr);
        RUNTIME_ASSERT(it != cr->GlobalMapGroup->end());
        cr->GlobalMapGroup->erase(it);

        if (!cr->GlobalMapGroup->empty()) {
            const auto* new_leader = *cr->GlobalMapGroup->begin();
            for (auto* group_cr : *cr->GlobalMapGroup) {
                group_cr->SetGlobalMapLeaderId(new_leader->GetId());
                group_cr->SetLastGlobalMapLeaderId(new_leader->GetId());
                group_cr->Send_RemoveCritter(cr);
            }
        }
        else {
            delete cr->GlobalMapGroup;
        }

        cr->GlobalMapGroup = nullptr;
        cr->SetGlobalMapLeaderId(ident_t {});
        cr->SetLastGlobalMapLeaderId(ident_t {});
    }
}

void MapManager::ProcessVisibleCritters(Critter* view_cr)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (view_cr->IsDestroyed()) {
        return;
    }

    // Global map
    if (!view_cr->GetMapId()) {
        RUNTIME_ASSERT(view_cr->GlobalMapGroup);

        if (view_cr->IsOwnedByPlayer()) {
            for (auto* cr : *view_cr->GlobalMapGroup) {
                if (view_cr == cr) {
                    view_cr->Send_AddCritter(view_cr);
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
    const auto dirs_count = GameSettings::MAP_DIR_COUNT;
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

        auto dist = _engine->Geometry.DistGame(view_cr->GetHexX(), view_cr->GetHexY(), cr->GetHexX(), cr->GetHexY());

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
            if (cr_dist != 0) {
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
            if (cr_dist != 0) {
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
            if (cr_dist != 0) {
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
            auto real_dir = _engine->Geometry.GetFarDir(view_cr->GetHexX(), view_cr->GetHexY(), cr->GetHexX(), cr->GetHexY());
            auto i = (dir_self > real_dir ? dir_self - real_dir : real_dir - dir_self);
            if (i > static_cast<int>(dirs_count / 2u)) {
                i = dirs_count - i;
            }
            look_self -= look_self * _engine->Settings.LookDir[i] / 100;
            // Opponent
            const int dir_opp = cr->GetDir();
            real_dir = static_cast<uchar>((real_dir + dirs_count / 2u) % dirs_count);
            i = (dir_opp > real_dir ? dir_opp - real_dir : real_dir - dir_opp);
            if (i > static_cast<int>(dirs_count / 2u)) {
                i = dirs_count - i;
            }
            look_opp -= look_opp * _engine->Settings.LookDir[i] / 100;
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
                const auto real_dir = _engine->Geometry.GetFarDir(view_cr->GetHexX(), view_cr->GetHexY(), cr->GetHexX(), cr->GetHexY());
                auto i = (dir_self > real_dir ? dir_self - real_dir : real_dir - dir_self);
                if (i > static_cast<int>(dirs_count / 2u)) {
                    i = dirs_count - i;
                }
                sneak_opp -= sneak_opp * _engine->Settings.LookSneakDir[i] / 100;
            }
            sneak_opp /= _engine->Settings.SneakDivider;
            vis = (look_self > sneak_opp ? look_self - sneak_opp : 0);
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
                const auto real_dir = _engine->Geometry.GetFarDir(cr->GetHexX(), cr->GetHexY(), view_cr->GetHexX(), view_cr->GetHexY());
                auto i = (dir_opp > real_dir ? dir_opp - real_dir : real_dir - dir_opp);
                if (i > static_cast<int>(dirs_count / 2u)) {
                    i = dirs_count - i;
                }
                sneak_self -= sneak_self * _engine->Settings.LookSneakDir[i] / 100;
            }
            sneak_self /= _engine->Settings.SneakDivider;
            vis = (look_opp > sneak_self ? look_opp - sneak_self : 0);
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
        if (cr_dist != 0) {
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
        if (cr_dist != 0) {
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
        if (cr_dist != 0) {
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
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (view_cr->IsDestroyed()) {
        return;
    }

    const auto map_id = view_cr->GetMapId();
    if (!map_id) {
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
                int dist = _engine->Geometry.DistGame(view_cr->GetHexX(), view_cr->GetHexY(), item->GetHexX(), item->GetHexY());
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
    STACK_TRACE_ENTRY();

    // Critters
    const auto dirs_count = GameSettings::MAP_DIR_COUNT;
    for (auto* cr : map->GetCritters()) {
        if (cr == view_cr) {
            continue;
        }

        if (IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_SCRIPT)) {
            if (_engine->OnMapCheckLook.Fire(map, view_cr, cr)) {
                view_cr->Send_AddCritter(cr);
            }
            continue;
        }

        const auto dist = _engine->Geometry.DistGame(hx, hy, cr->GetHexX(), cr->GetHexY());
        auto look_self = look;

        // Dir modifier
        if (IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_DIR)) {
            const auto real_dir = _engine->Geometry.GetFarDir(hx, hy, cr->GetHexX(), cr->GetHexY());
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
                const auto real_dir = _engine->Geometry.GetFarDir(hx, hy, cr->GetHexX(), cr->GetHexY());
                auto i = (dir > real_dir ? dir - real_dir : real_dir - dir);
                if (i > static_cast<int>(dirs_count / 2u)) {
                    i = dirs_count - i;
                }
                sneak_opp -= sneak_opp * _engine->Settings.LookSneakDir[i] / 100;
            }
            sneak_opp /= _engine->Settings.SneakDivider;
            vis = (look_self > sneak_opp ? look_self - sneak_opp : 0);
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
                auto dist = _engine->Geometry.DistGame(hx, hy, item->GetHexX(), item->GetHexY());
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

auto MapManager::CheckKnownLoc(Critter* cr, ident_t loc_id) const -> bool
{
    STACK_TRACE_ENTRY();

    for (const auto known_loc_id : cr->GetKnownLocations()) {
        if (known_loc_id == loc_id) {
            return true;
        }
    }
    return false;
}

void MapManager::AddKnownLoc(Critter* cr, ident_t loc_id)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (CheckKnownLoc(cr, loc_id)) {
        return;
    }

    auto known_locs = cr->GetKnownLocations();
    known_locs.push_back(loc_id);
    cr->SetKnownLocations(known_locs);
}

void MapManager::EraseKnownLoc(Critter* cr, ident_t loc_id)
{
    STACK_TRACE_ENTRY();

    auto known_locs = cr->GetKnownLocations();
    for (size_t i = 0; i < known_locs.size(); i++) {
        if (known_locs[i] == loc_id) {
            known_locs.erase(known_locs.begin() + i);
            cr->SetKnownLocations(known_locs);
            break;
        }
    }
}
