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

    _mapGrid = new int16[static_cast<size_t>(FPATH_MAX_PATH * 2 + 2) * (FPATH_MAX_PATH * 2 + 2)];
}

MapManager::~MapManager()
{
    STACK_TRACE_ENTRY();

    delete[] _mapGrid;
}

auto MapManager::GridAt(mpos pos) -> int16&
{
    NO_STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    return _mapGrid[((FPATH_MAX_PATH + 1) + pos.y - _mapGridOffset.y) * (FPATH_MAX_PATH * 2 + 2) + ((FPATH_MAX_PATH + 1) + pos.x - _mapGridOffset.x)];
}

void MapManager::LoadFromResources()
{
    STACK_TRACE_ENTRY();

    auto map_files = _engine->Resources.FilterFiles("fomapb");

    while (map_files.MoveNext()) {
        auto map_file = map_files.GetCurFile();
        auto reader = DataReader({map_file.GetBuf(), map_file.GetSize()});

        const auto pid = _engine->ToHashedString(map_file.GetName());
        const auto* pmap = _engine->ProtoMngr.GetProtoMap(pid);
        RUNTIME_ASSERT(pmap);

        auto&& static_map = std::make_unique<StaticMap>();
        const auto map_size = pmap->GetSize();

        static_map->HexField.SetSize(map_size);

        // Read hashes
        {
            const auto hashes_count = reader.Read<uint>();

            string str;
            for (uint i = 0; i < hashes_count; i++) {
                const auto str_len = reader.Read<uint>();
                str.resize(str_len);
                reader.ReadPtr(str.data(), str.length());
                const auto hstr = _engine->ToHashedString(str);
                UNUSED_VARIABLE(hstr);
            }
        }

        // Read entities
        {
            vector<uint8> props_data;

            // Read critters
            {
                const auto cr_count = reader.Read<uint>();

                static_map->CritterBillets.reserve(cr_count);

                for (const auto i : xrange(cr_count)) {
                    UNUSED_VARIABLE(i);

                    const auto cr_id = reader.Read<uint>();

                    const auto cr_pid_hash = reader.Read<hstring::hash_t>();
                    const auto cr_pid = _engine->ResolveHash(cr_pid_hash);
                    const auto* cr_proto = _engine->ProtoMngr.GetProtoCritter(cr_pid);

                    auto cr_props = Properties(cr_proto->GetProperties().GetRegistrator());
                    const auto props_data_size = reader.Read<uint>();
                    props_data.resize(props_data_size);
                    reader.ReadPtr<uint8>(props_data.data(), props_data_size);
                    cr_props.RestoreAllData(props_data);

                    auto* cr = new Critter(_engine, ident_t {}, cr_proto, &cr_props);

                    static_map->CritterBillets.emplace_back(cr_id, cr);

                    // Checks
                    if (const auto hex = cr->GetMapHex(); !map_size.IsValidPos(hex)) {
                        throw MapManagerException("Invalid critter position on map", pmap->GetName(), cr->GetName(), hex);
                    }
                }
            }

            // Read items
            {
                const auto item_count = reader.Read<uint>();

                static_map->ItemBillets.reserve(item_count);
                static_map->HexItemBillets.reserve(item_count);
                static_map->ChildItemBillets.reserve(item_count);
                static_map->StaticItems.reserve(item_count);
                static_map->StaticItemsById.reserve(item_count);

                for (const auto i : xrange(item_count)) {
                    UNUSED_VARIABLE(i);

                    const auto item_id = reader.Read<uint>();

                    const auto item_pid_hash = reader.Read<hstring::hash_t>();
                    const auto item_pid = _engine->ResolveHash(item_pid_hash);
                    const auto* item_proto = _engine->ProtoMngr.GetProtoItem(item_pid);

                    auto item_props = Properties(item_proto->GetProperties().GetRegistrator());
                    const auto props_data_size = reader.Read<uint>();
                    props_data.resize(props_data_size);
                    reader.ReadPtr<uint8>(props_data.data(), props_data_size);
                    item_props.RestoreAllData(props_data);

                    auto* item = new Item(_engine, ident_t {}, item_proto, &item_props);

                    static_map->ItemBillets.emplace_back(item_id, item);

                    // Checks
                    if (item->GetOwnership() == ItemOwnership::MapHex) {
                        if (const auto hex = item->GetMapHex(); !map_size.IsValidPos(hex)) {
                            throw MapManagerException("Invalid item position on map", pmap->GetName(), item->GetName(), hex);
                        }
                    }

                    // Bind scripts
                    if (item->GetSceneryScript()) {
                        item->SceneryScriptFunc = _engine->ScriptSys->FindFunc<bool, Critter*, StaticItem*, Item*, int>(item->GetSceneryScript());
                        if (!item->SceneryScriptFunc) {
                            throw MapManagerException("Can't bind static item scenery function", pmap->GetName(), item->GetSceneryScript());
                        }
                    }

                    if (item->GetTriggerScript()) {
                        item->TriggerScriptFunc = _engine->ScriptSys->FindFunc<void, Critter*, StaticItem*, bool, uint8>(item->GetTriggerScript());
                        if (!item->TriggerScriptFunc) {
                            throw MapManagerException("Can't bind static item trigger function", pmap->GetName(), item->GetTriggerScript());
                        }
                    }

                    // Sort
                    if (item->GetIsStatic()) {
                        RUNTIME_ASSERT(item->GetOwnership() == ItemOwnership::MapHex);

                        const auto hex = item->GetMapHex();

                        if (!item->GetIsHiddenInStatic()) {
                            static_map->StaticItems.emplace_back(item);
                            static_map->StaticItemsById.emplace(item->GetId(), item);
                            static_map->HexField.GetCellForWriting(hex).StaticItems.emplace_back(item);
                        }
                        if (item->GetIsTrigger() || item->GetIsTrap()) {
                            static_map->HexField.GetCellForWriting(hex).TriggerItems.emplace_back(item);
                        }
                    }
                    else {
                        if (item->GetOwnership() == ItemOwnership::MapHex) {
                            static_map->HexItemBillets.emplace_back(item_id, item);
                        }
                        else {
                            RUNTIME_ASSERT(item->GetOwnership() == ItemOwnership::CritterInventory || item->GetOwnership() == ItemOwnership::ItemContainer);
                            static_map->ChildItemBillets.emplace_back(item_id, item);
                        }
                    }
                }
            }
        }

        reader.VerifyEnd();

        // Fill hex flags from static items on map
        for (auto&& [item_id, item] : static_map->ItemBillets) {
            if (item->GetOwnership() != ItemOwnership::MapHex || !item->GetIsStatic() || item->GetIsTile()) {
                continue;
            }

            const auto hex = item->GetMapHex();

            if (!item->GetIsNoBlock()) {
                auto& static_field = static_map->HexField.GetCellForWriting(hex);
                static_field.IsMoveBlocked = true;
            }

            if (!item->GetIsShootThru()) {
                auto& static_field = static_map->HexField.GetCellForWriting(hex);
                static_field.IsShootBlocked = true;
                static_field.IsMoveBlocked = true;
            }

            // Block around scroll blocks
            if (item->GetIsScrollBlock()) {
                for (uint8 k = 0; k < 6; k++) {
                    auto scroll_hex = hex;

                    if (GeometryHelper::MoveHexByDir(scroll_hex, k, map_size)) {
                        auto& static_field = static_map->HexField.GetCellForWriting(scroll_hex);
                        static_field.IsMoveBlocked = true;
                    }
                }
            }

            if (item->IsNonEmptyBlockLines()) {
                const auto shooted = item->GetIsShootThru();
                GeometryHelper::ForEachBlockLines(item->GetBlockLines(), hex, map_size, [&static_map, shooted](mpos block_hex) {
                    auto& block_static_field = static_map->HexField.GetCellForWriting(block_hex);
                    block_static_field.IsMoveBlocked = true;
                    if (!shooted) {
                        block_static_field.IsShootBlocked = true;
                    }
                });
            }
        }

        static_map->CritterBillets.shrink_to_fit();
        static_map->ItemBillets.shrink_to_fit();
        static_map->HexItemBillets.shrink_to_fit();
        static_map->ChildItemBillets.shrink_to_fit();
        static_map->StaticItems.shrink_to_fit();

        _staticMaps.emplace(pmap, std::move(static_map));
    }

    RUNTIME_ASSERT(_engine->ProtoMngr.GetProtoMaps().size() == _staticMaps.size());
}

auto MapManager::GetStaticMap(const ProtoMap* proto_map) const -> const StaticMap*
{
    STACK_TRACE_ENTRY();

    const auto it = _staticMaps.find(proto_map);
    RUNTIME_ASSERT(it != _staticMaps.end());
    return it->second.get();
}

void MapManager::GenerateMapContent(Map* map)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    unordered_map<ident_t, ident_t> id_map;

    // Generate npc
    for (auto&& [base_cr_id, base_cr] : map->GetStaticMap()->CritterBillets) {
        const auto* npc = _engine->CrMngr.CreateCritter(base_cr->GetProtoId(), &base_cr->GetProperties(), map, base_cr->GetMapHex(), base_cr->GetDir(), true);
        if (npc == nullptr) {
            WriteLog("Create npc '{}' on map '{}' failed, continue generate", base_cr->GetName(), map->GetName());
            continue;
        }

        id_map.emplace(base_cr_id, npc->GetId());
    }

    // Generate hex items
    for (auto&& [base_item_id, base_item] : map->GetStaticMap()->HexItemBillets) {
        // Create item
        auto* item = _engine->ItemMngr.CreateItem(base_item->GetProtoId(), 0, &base_item->GetProperties());
        if (item == nullptr) {
            WriteLog("Create item '{}' on map '{}' failed, continue generate", base_item->GetName(), map->GetName());
            continue;
        }
        id_map.emplace(base_item_id, item->GetId());

        // Other values
        if (item->GetIsCanOpen() && item->GetOpened()) {
            item->SetIsLightThru(true);
        }

        map->AddItem(item, base_item->GetMapHex(), nullptr);
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
            WriteLog("Create item '{}' on map '{}' failed, continue generate", base_item->GetName(), map->GetName());
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
            _engine->ItemMngr.AddItemToContainer(item_cont, item, ContainerItemStack::Root);
        }
        else {
            throw UnreachablePlaceException(LINE_STR);
        }
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
        result += _str("{:<20} {:<10}   {:<10} {:<6} {:08X} {:<7} {:<11} {:<9} {:<11} {:<5}\n", loc->GetName(), loc->GetId(), loc->GetWorldPos(), loc->GetRadius(), loc->GetColor(), loc->GetHidden() ? "true" : "false", loc->GetGeckVisible() ? "true" : "false", loc->GeckCount, loc->GetAutoGarbage() ? "true" : "false", loc->GetToGarbage() ? "true" : "false");

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

auto MapManager::CreateLocation(hstring proto_id, upos16 wpos) -> Location*
{
    STACK_TRACE_ENTRY();

    const auto* proto = _engine->ProtoMngr.GetProtoLocation(proto_id);
    if (proto == nullptr) {
        throw MapManagerException("Location proto is not loaded", proto_id);
    }

    if (wpos.x >= GM_MAXZONEX * _engine->Settings.GlobalMapZoneLength || wpos.y >= GM_MAXZONEY * _engine->Settings.GlobalMapZoneLength) {
        throw MapManagerException("Invalid location coordinates", proto_id);
    }

    auto* loc = new Location(_engine, ident_t {}, proto);

    auto loc_holder = RefCountHolder(loc);

    loc->SetWorldPos(wpos);

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

    for (auto* map : copy_hold_ref(loc->GetMaps())) {
        map->SetLocId(loc->GetId());
        GenerateMapContent(map);
    }

    _engine->EntityMngr.CallInit(loc, true);

    if (!loc->IsDestroyed()) {
        for (auto* map : copy_hold_ref(loc->GetMaps())) {
            if (!map->IsDestroyed()) {
                for (auto* cr : copy_hold_ref(map->GetCritters())) {
                    if (!cr->IsDestroyed()) {
                        ProcessVisibleCritters(cr);
                    }
                    if (!cr->IsDestroyed()) {
                        ProcessVisibleItems(cr);
                    }
                }
            }
        }
    }

    return !loc->IsDestroyed() ? loc : nullptr;
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

    auto* map = new Map(_engine, ident_t {}, proto_map, loc, it->second.get());
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

    RUNTIME_ASSERT(!map->IsDestroyed());

    auto map_holder = RefCountHolder(map);

    DeleteMapContent(map);

    if (!map->IsDestroyed()) {
        GenerateMapContent(map);
    }

    if (!map->IsDestroyed()) {
        for (auto* cr : copy_hold_ref(map->GetCritters())) {
            if (!cr->IsDestroyed()) {
                _engine->EntityMngr.CallInit(cr, true);
            }
        }
    }

    if (!map->IsDestroyed()) {
        for (auto* item : copy_hold_ref(map->GetItems())) {
            if (!item->IsDestroyed()) {
                _engine->EntityMngr.CallInit(item, true);
            }
        }
    }

    if (!map->IsDestroyed()) {
        for (auto* cr : copy_hold_ref(map->GetCritters())) {
            if (!cr->IsDestroyed()) {
                ProcessVisibleCritters(cr);
            }
            if (!cr->IsDestroyed()) {
                ProcessVisibleItems(cr);
            }
        }
    }
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

auto MapManager::GetMapsCount() const -> size_t
{
    STACK_TRACE_ENTRY();

    return _engine->EntityMngr.GetMaps().size();
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

    const int zl = static_cast<int>(_engine->Settings.GlobalMapZoneLength);
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
        if (!loc->IsDestroyed() && loc->IsLocVisible() && IsIntersectZone(wx, wy, 0, loc->GetWorldPos().x, loc->GetWorldPos().y, loc->GetRadius(), zone_radius)) {
            locs.push_back(loc);
        }
    }

    return locs;
}

void MapManager::KickPlayersToGlobalMap(Map* map)
{
    STACK_TRACE_ENTRY();

    for (auto* cl : map->GetPlayerCritters()) {
        TransitToGlobal(cl, {});
    }
}

auto MapManager::GetLocations() -> const unordered_map<ident_t, Location*>&
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    return _engine->EntityMngr.GetLocations();
}

auto MapManager::GetLocationsCount() const -> size_t
{
    STACK_TRACE_ENTRY();

    return _engine->EntityMngr.GetLocations().size();
}

void MapManager::LocationGarbager()
{
    STACK_TRACE_ENTRY();

    if (_runGarbager) {
        _runGarbager = false;

        for (auto* loc : copy_hold_ref(_engine->EntityMngr.GetLocations())) {
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
    auto loc_holder = RefCountHolder(loc);
    auto&& maps = copy_hold_ref(loc->GetMaps());

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
    const auto map_size = map->GetSize();
    const auto start_hex = trace.StartHex;
    const auto target_hex = trace.TargetHex;

    auto dist = trace.Dist;
    if (dist == 0) {
        dist = GeometryHelper::DistGame(start_hex, target_hex);
    }

    auto cur_hex = start_hex;
    auto old_hex = cur_hex;

    auto line_tracer = LineTracer(start_hex, target_hex, map_size, trace.Angle);

    trace.IsFullTrace = false;
    trace.IsCritterFound = false;
    trace.IsHaveLastMovable = false;
    auto last_passed_ok = false;

    for (uint i = 0;; i++) {
        if (i >= dist) {
            trace.IsFullTrace = true;
            break;
        }

        uint8 dir;
        if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
            dir = line_tracer.GetNextHex(cur_hex);
        }
        else {
            line_tracer.GetNextSquare(cur_hex);
            dir = GeometryHelper::GetNearDir(old_hex, cur_hex);
        }

        if (trace.HexCallback != nullptr) {
            trace.HexCallback(map, trace.FindCr, old_hex, cur_hex, dir);
            old_hex = cur_hex;
            continue;
        }

        if (trace.LastMovable != nullptr && !last_passed_ok) {
            if (map->IsHexMovable(cur_hex)) {
                *trace.LastMovable = cur_hex;
                trace.IsHaveLastMovable = true;
            }
            else {
                last_passed_ok = true;
            }
        }

        if (!map->IsHexShootable(cur_hex)) {
            break;
        }

        if (trace.Critters != nullptr && map->IsAnyCritter(cur_hex)) {
            const auto critters = map->GetCritters(cur_hex, 0, trace.FindType);
            trace.Critters->insert(trace.Critters->end(), critters.begin(), critters.end());
        }

        if (trace.FindCr != nullptr && map->IsCritter(cur_hex, trace.FindCr)) {
            trace.IsCritterFound = true;
            break;
        }

        old_hex = cur_hex;
    }

    if (trace.PreBlock != nullptr) {
        *trace.PreBlock = old_hex;
    }
    if (trace.Block != nullptr) {
        *trace.Block = cur_hex;
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

    const auto map_size = map->GetSize();

    if (!map_size.IsValidPos(input.FromHex) || !map_size.IsValidPos(input.ToHex)) {
        output.Result = FindPathOutput::ResultType::InvalidHexes;
        return output;
    }
    if (GeometryHelper::CheckDist(input.FromHex, input.ToHex, input.Cut)) {
        output.Result = FindPathOutput::ResultType::AlreadyHere;
        return output;
    }
    if (input.Cut == 0 && !map->IsHexesMovable(input.ToHex, input.Multihex, input.TraceCr)) {
        output.Result = FindPathOutput::ResultType::HexBusy;
        return output;
    }

    // Ring check
    if (input.Cut <= 1 && input.Multihex == 0) {
        auto&& [rsx, rsy] = _engine->Geometry.GetHexOffsets(input.ToHex);

        uint i = 0;

        for (; i < GameSettings::MAP_DIR_COUNT; i++, rsx++, rsy++) {
            const auto ring_raw_hex = ipos {input.ToHex.x + *rsx, input.ToHex.y + *rsy};

            if (map_size.IsValidPos(ring_raw_hex)) {
                const auto ring_hex = map_size.FromRawPos(ring_raw_hex);

                if (map->IsItemGag(ring_hex)) {
                    break;
                }
                if (map->IsHexMovable(ring_hex)) {
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
    _mapGridOffset = input.FromHex;

    int16 numindex = 1;
    std::memset(_mapGrid, 0, static_cast<size_t>(FPATH_MAX_PATH * 2 + 2) * (FPATH_MAX_PATH * 2 + 2) * sizeof(int16));
    GridAt(input.FromHex) = numindex;

    auto to_hex = input.ToHex;

    vector<mpos> coords;
    vector<mpos> cr_coords;
    vector<mpos> gag_coords;
    coords.reserve(10000);
    cr_coords.reserve(100);
    gag_coords.reserve(100);

    // First point
    coords.emplace_back(input.FromHex);

    // Begin search
    auto p = 0;
    auto p_togo = 1;
    mpos cur_hex;

    while (true) {
        for (auto i = 0; i < p_togo; i++, p++) {
            cur_hex = coords[p];
            numindex = GridAt(cur_hex);

            if (GeometryHelper::CheckDist(cur_hex, to_hex, input.Cut)) {
                to_hex = cur_hex;
                goto label_FindOk;
            }

            if (++numindex > FPATH_MAX_PATH) {
                output.Result = FindPathOutput::ResultType::TooFar;
                return output;
            }

            auto&& [sx, sy] = _engine->Geometry.GetHexOffsets(cur_hex);

            for (uint j = 0; j < GameSettings::MAP_DIR_COUNT; j++) {
                const auto raw_next_hex = ipos {cur_hex.x + sx[j], cur_hex.y + sy[j]};

                if (!map_size.IsValidPos(raw_next_hex)) {
                    continue;
                }

                const auto next_hex = map_size.FromRawPos(raw_next_hex);
                auto& grid_cell = GridAt(next_hex);

                if (grid_cell != 0) {
                    continue;
                }

                if (map->IsHexesMovable(next_hex, input.Multihex, input.FromCritter)) {
                    coords.emplace_back(next_hex);
                    grid_cell = numindex;
                }
                else if (input.CheckGagItems && map->IsItemGag(next_hex)) {
                    gag_coords.emplace_back(next_hex);
                    grid_cell = static_cast<int16>(numindex | 0x4000);
                }
                else if (input.CheckCritter && map->IsNonDeadCritter(next_hex)) {
                    cr_coords.emplace_back(next_hex);
                    grid_cell = static_cast<int16>(numindex | 0x8000);
                }
                else {
                    grid_cell = -1;
                }
            }
        }

        // Add gag hex after some distance
        if (!gag_coords.empty()) {
            auto last_index = GridAt(coords.back());
            auto& gag_hex = gag_coords.front();
            auto gag_index = static_cast<int16>(GridAt(gag_hex) ^ static_cast<int16>(0x4000));

            if (gag_index + 10 < last_index) // Todo: if path finding not be reworked than migrate magic number to scripts
            {
                GridAt(gag_hex) = gag_index;
                coords.push_back(gag_hex);
                gag_coords.erase(gag_coords.begin());
            }
        }

        // Add gag and critters hexes
        p_togo = static_cast<int>(coords.size()) - p;

        if (p_togo == 0) {
            if (!gag_coords.empty()) {
                auto& gag_hex = gag_coords.front();
                GridAt(gag_hex) ^= 0x4000;
                coords.push_back(gag_hex);
                gag_coords.erase(gag_coords.begin());
                p_togo++;
            }
            else if (!cr_coords.empty()) {
                auto& gag_hex = gag_coords.front();
                GridAt(gag_hex) ^= static_cast<int16>(0x8000);
                coords.push_back(gag_hex);
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
    vector<uint8> raw_steps;
    raw_steps.resize(numindex - 1);

    // Smooth data
    float base_angle = GeometryHelper::GetDirAngle(to_hex, input.FromHex);

    while (numindex > 1) {
        numindex--;

        const auto find_path_grid = [this, &map_size, &input, base_angle, &raw_steps, &numindex](mpos& hex) -> bool {
            int best_step_dir = -1;
            float best_step_angle_diff = 0.0f;

            const auto check_hex = [this, &map_size, &best_step_dir, &best_step_angle_diff, &input, numindex, base_angle](int dir, ipos step_raw_hex) {
                if (!map_size.IsValidPos(step_raw_hex)) {
                    return;
                }

                const auto step_hex = map_size.FromRawPos(step_raw_hex);

                if (GridAt(step_hex) != numindex) {
                    return;
                }

                const float angle = GeometryHelper::GetDirAngle(step_hex, input.FromHex);
                const float angle_diff = GeometryHelper::GetDirAngleDiff(base_angle, angle);

                if (best_step_dir == -1 || numindex == 0) {
                    best_step_dir = dir;
                    best_step_angle_diff = GeometryHelper::GetDirAngleDiff(base_angle, angle);
                }
                else {
                    if (best_step_dir == -1 || angle_diff < best_step_angle_diff) {
                        best_step_dir = dir;
                        best_step_angle_diff = angle_diff;
                    }
                }
            };

            if ((hex.x % 2) != 0) {
                check_hex(3, ipos {hex.x - 1, hex.y - 1});
                check_hex(2, ipos {hex.x, hex.y - 1});
                check_hex(5, ipos {hex.x, hex.y + 1});
                check_hex(0, ipos {hex.x + 1, hex.y});
                check_hex(4, ipos {hex.x - 1, hex.y});
                check_hex(1, ipos {hex.x + 1, hex.y - 1});

                if (best_step_dir == 3) {
                    raw_steps[numindex - 1] = 3;
                    hex.x--;
                    hex.y--;
                    return true;
                }
                if (best_step_dir == 2) {
                    raw_steps[numindex - 1] = 2;
                    hex.y--;
                    return true;
                }
                if (best_step_dir == 5) {
                    raw_steps[numindex - 1] = 5;
                    hex.y++;
                    return true;
                }
                if (best_step_dir == 0) {
                    raw_steps[numindex - 1] = 0;
                    hex.x++;
                    return true;
                }
                if (best_step_dir == 4) {
                    raw_steps[numindex - 1] = 4;
                    hex.x--;
                    return true;
                }
                if (best_step_dir == 1) {
                    raw_steps[numindex - 1] = 1;
                    hex.x++;
                    hex.y--;
                    return true;
                }
            }
            else {
                check_hex(3, ipos {hex.x - 1, hex.y});
                check_hex(2, ipos {hex.x, hex.y - 1});
                check_hex(5, ipos {hex.x, hex.y + 1});
                check_hex(0, ipos {hex.x + 1, hex.y + 1});
                check_hex(4, ipos {hex.x - 1, hex.y + 1});
                check_hex(1, ipos {hex.x + 1, hex.y});

                if (best_step_dir == 3) {
                    raw_steps[numindex - 1] = 3;
                    hex.x--;
                    return true;
                }
                if (best_step_dir == 2) {
                    raw_steps[numindex - 1] = 2;
                    hex.y--;
                    return true;
                }
                if (best_step_dir == 5) {
                    raw_steps[numindex - 1] = 5;
                    hex.y++;
                    return true;
                }
                if (best_step_dir == 0) {
                    raw_steps[numindex - 1] = 0;
                    hex.x++;
                    hex.y++;
                    return true;
                }
                if (best_step_dir == 4) {
                    raw_steps[numindex - 1] = 4;
                    hex.x--;
                    hex.y++;
                    return true;
                }
                if (best_step_dir == 1) {
                    raw_steps[numindex - 1] = 1;
                    hex.x++;
                    return true;
                }
            }

            return false;
        };

        if (!find_path_grid(cur_hex)) {
            output.Result = FindPathOutput::ResultType::InternalError;
            return output;
        }
    }

    // Check for closed door and critter
    if (input.CheckCritter || input.CheckGagItems) {
        mpos check_hex = input.FromHex;

        for (size_t i = 0; i < raw_steps.size(); i++) {
            const auto prev_check_hex = check_hex;

            const auto move_ok = GeometryHelper::MoveHexByDir(check_hex, raw_steps[i], map_size);
            RUNTIME_ASSERT(move_ok);

            if (input.CheckGagItems && map->IsItemGag(check_hex)) {
                Item* item = map->GetItemGag(check_hex);

                if (item == nullptr) {
                    continue;
                }

                output.GagItem = item;
                to_hex = prev_check_hex;
                raw_steps.resize(i);
                break;
            }

            if (input.CheckCritter && map->IsNonDeadCritter(check_hex)) {
                Critter* cr = map->GetNonDeadCritter(check_hex);

                if (cr == nullptr || cr == input.FromCritter) {
                    continue;
                }

                output.GagCritter = cr;
                to_hex = prev_check_hex;
                raw_steps.resize(i);
                break;
            }
        }
    }

    // Trace
    if (input.TraceDist != 0) {
        vector<int> trace_seq;
        const mpos targ_hex = input.TraceCr->GetMapHex();
        bool trace_ok = false;

        trace_seq.resize(raw_steps.size() + 4);

        mpos check_hex = input.FromHex;

        for (size_t i = 0; i < raw_steps.size(); i++) {
            const auto move_ok = GeometryHelper::MoveHexByDir(check_hex, raw_steps[i], map_size);
            RUNTIME_ASSERT(move_ok);

            if (map->IsItemGag(check_hex)) {
                trace_seq[i + 2 - 2] += 1;
                trace_seq[i + 2 - 1] += 2;
                trace_seq[i + 2 - 0] += 3;
                trace_seq[i + 2 + 1] += 2;
                trace_seq[i + 2 + 2] += 1;
            }
        }

        TraceData trace;
        trace.TraceMap = map;
        trace.TargetHex = targ_hex;
        trace.FindCr = input.TraceCr;

        for (int k = 0; k < 5; k++) {
            mpos check_hex2 = input.FromHex;

            for (size_t i = 0; i < raw_steps.size() - 1; i++) {
                const auto move_ok2 = GeometryHelper::MoveHexByDir(check_hex2, raw_steps[i], map_size);
                RUNTIME_ASSERT(move_ok2);

                if (k < 4 && trace_seq[i + 2] != k) {
                    continue;
                }
                if (k == 4 && trace_seq[i + 2] < 4) {
                    continue;
                }

                if (!GeometryHelper::CheckDist(check_hex2, targ_hex, input.TraceDist)) {
                    continue;
                }

                trace.StartHex = check_hex2;

                TraceBullet(trace);

                if (trace.IsCritterFound) {
                    trace_ok = true;
                    raw_steps.resize(i + 1);
                    const auto move_ok3 = GeometryHelper::MoveHexByDir(check_hex2, raw_steps[i + 1], map_size);
                    RUNTIME_ASSERT(move_ok3);
                    to_hex = check_hex2;

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
        mpos trace_hex = input.FromHex;

        while (true) {
            mpos trace_hex2 = to_hex;

            for (auto i = static_cast<int>(raw_steps.size()) - 1; i >= 0; i--) {
                LineTracer tracer(trace_hex, trace_hex2, map_size, 0.0f);
                mpos next_hex = trace_hex;
                vector<uint8> direct_steps;
                bool failed = false;

                while (true) {
                    uint8 dir = tracer.GetNextHex(next_hex);
                    direct_steps.push_back(dir);

                    if (next_hex == trace_hex2) {
                        break;
                    }

                    if (GridAt(next_hex) <= 0) {
                        failed = true;
                        break;
                    }
                }

                if (failed) {
                    RUNTIME_ASSERT(i > 0);
                    GeometryHelper::MoveHexByDir(trace_hex2, GeometryHelper::ReverseDir(raw_steps[i]), map_size);
                    continue;
                }

                for (const auto& ds : direct_steps) {
                    output.Steps.push_back(ds);
                }

                output.ControlSteps.emplace_back(static_cast<uint16>(output.Steps.size()));

                trace_hex = trace_hex2;
                break;
            }

            if (trace_hex2 == to_hex) {
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

            output.ControlSteps.emplace_back(static_cast<uint16>(output.Steps.size()));
        }
    }

    RUNTIME_ASSERT(!output.Steps.empty());
    RUNTIME_ASSERT(!output.ControlSteps.empty());

    output.Result = FindPathOutput::ResultType::Ok;
    output.NewToHex = to_hex;

    return output;
}

void MapManager::TransitToMap(Critter* cr, Map* map, mpos hex, uint8 dir, optional<uint> safe_radius)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(map);
    RUNTIME_ASSERT(map->GetSize().IsValidPos(hex));

    Transit(cr, map, hex, dir, safe_radius, {});
}

void MapManager::TransitToGlobal(Critter* cr, ident_t global_cr_id)
{
    STACK_TRACE_ENTRY();

    Transit(cr, nullptr, {}, 0, std::nullopt, global_cr_id);
}

void MapManager::Transit(Critter* cr, Map* map, mpos hex, uint8 dir, optional<uint> safe_radius, ident_t global_cr_id)
{
    STACK_TRACE_ENTRY();

    if (cr->LockMapTransfers != 0) {
        throw GenericException("Critter transfers locked");
    }

    cr->LockMapTransfers++;
    auto restore_transfers = ScopeCallback([cr]() noexcept { cr->LockMapTransfers--; });

    const auto prev_map_id = cr->GetMapId();
    auto* prev_map = prev_map_id ? GetMap(prev_map_id) : nullptr;
    RUNTIME_ASSERT(!prev_map_id || !!prev_map);

    cr->ClearMove();

    if (cr->GetIsAttached()) {
        cr->DetachFromCritter();
    }

    vector<Critter*> attached_critters;

    if (!cr->AttachedCritters.empty()) {
        attached_critters = cr->AttachedCritters;

        for (auto* attached_cr : attached_critters) {
            attached_cr->DetachFromCritter();
        }
    }

    if (map == prev_map) {
        // Between one map
        if (map != nullptr) {
            RUNTIME_ASSERT(map->GetSize().IsValidPos(hex));

            const auto multihex = cr->GetMultihex();

            mpos start_hex = hex;

            if (safe_radius.has_value()) {
                auto safe_start_hex = map->FindStartHex(hex, multihex, safe_radius.value(), true);
                if (!safe_start_hex.has_value()) {
                    safe_start_hex = map->FindStartHex(hex, multihex, safe_radius.value(), false);
                }
                if (safe_start_hex.has_value()) {
                    start_hex = safe_start_hex.value();
                }
            }

            cr->ChangeDir(dir);
            map->RemoveCritterFromField(cr);
            cr->SetMapHex(start_hex);
            map->AddCritterToField(cr);
            cr->Send_Teleport(cr, start_hex);
            cr->Broadcast_Teleport(start_hex);
            cr->ClearVisible();
            cr->Send_Moving(cr);

            ProcessVisibleCritters(cr);
            ProcessVisibleItems(cr);

            if (cr->IsDestroyed()) {
                return;
            }
        }
    }
    else {
        // Between different maps
        mpos start_hex = hex;

        if (map != nullptr) {
            const auto multihex = cr->GetMultihex();

            if (safe_radius.has_value()) {
                auto safe_start_hex = map->FindStartHex(hex, multihex, safe_radius.value(), true);
                if (!safe_start_hex.has_value()) {
                    safe_start_hex = map->FindStartHex(hex, multihex, safe_radius.value(), false);
                }
                if (safe_start_hex.has_value()) {
                    start_hex = safe_start_hex.value();
                }
            }
        }

        cr->SetMapLeaveHex(cr->GetMapHex());

        EraseCrFromMap(cr, prev_map);
        AddCrToMap(cr, map, start_hex, dir, global_cr_id);

        if (cr->IsDestroyed()) {
            return;
        }

        cr->Send_LoadMap(nullptr);
        cr->Send_AddCritter(cr);
        cr->Send_AddAllItems();

        ProcessVisibleCritters(cr);
        ProcessVisibleItems(cr);

        if (cr->IsDestroyed()) {
            return;
        }

        cr->Send_PlaceToGameComplete();
    }

    // Transit attached critters
    if (!attached_critters.empty()) {
        for (auto* attached_cr : attached_critters) {
            if (!cr->IsDestroyed() && !attached_cr->IsDestroyed() && !attached_cr->GetIsAttached() && attached_cr->GetMapId() == prev_map_id) {
                Transit(attached_cr, map, cr->GetMapHex(), dir, std::nullopt, cr->GetId());
            }
        }

        if (!cr->IsDestroyed() && !cr->GetIsAttached()) {
            for (auto* attached_cr : attached_critters) {
                if (!attached_cr->IsDestroyed() && !attached_cr->GetIsAttached() && attached_cr->AttachedCritters.empty() && attached_cr->GetMapId() == cr->GetMapId()) {
                    attached_cr->AttachToCritter(cr);
                }
            }

            if (!cr->IsDestroyed()) {
                cr->MoveAttachedCritters();
                ProcessVisibleCritters(cr);
            }
        }
    }
}

void MapManager::AddCrToMap(Critter* cr, Map* map, mpos hex, uint8 dir, ident_t global_cr_id)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    cr->LockMapTransfers++;
    auto restore_transfers = ScopeCallback([cr]() noexcept { cr->LockMapTransfers--; });

    if (map != nullptr) {
        RUNTIME_ASSERT(map->GetSize().IsValidPos(hex));

        cr->SetMapId(map->GetId());
        cr->SetMapHex(hex);
        cr->ChangeDir(dir);

        cr->SetLastLocationId(map->GetLocation()->GetId());
        cr->SetLastLocationPid(map->GetLocation()->GetProtoId());
        cr->SetLastMapId(map->GetId());
        cr->SetLastMapPid(map->GetProtoId());

        if (!cr->GetIsControlledByPlayer()) {
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

        const auto* global_cr = global_cr_id && global_cr_id != cr->GetId() ? _engine->CrMngr.GetCritter(global_cr_id) : nullptr;

        if (global_cr == nullptr || global_cr->GetMapId()) {
            cr->SetGlobalMapLeaderId(cr->GetId());
            cr->SetGlobalMapTripId(cr->GetGlobalMapTripId() + 1);

            cr->SetLastGlobalMapLeaderId(cr->GetId());

            cr->GlobalMapGroup = new vector<Critter*>();
            cr->GlobalMapGroup->push_back(cr);
        }
        else {
            RUNTIME_ASSERT(global_cr->GlobalMapGroup);

            cr->SetWorldPos(global_cr->GetWorldPos());
            cr->SetGlobalMapLeaderId(global_cr_id);
            cr->SetGlobalMapTripId(global_cr->GetGlobalMapTripId());

            cr->SetLastGlobalMapLeaderId(global_cr_id);

            for (auto* group_cr : *global_cr->GlobalMapGroup) {
                group_cr->Send_AddCritter(cr);
            }

            cr->GlobalMapGroup = global_cr->GlobalMapGroup;
            cr->GlobalMapGroup->push_back(cr);
        }

        _engine->OnGlobalMapCritterIn.Fire(cr);
    }
}

void MapManager::EraseCrFromMap(Critter* cr, Map* map)
{
    STACK_TRACE_ENTRY();

    cr->LockMapTransfers++;
    auto restore_transfers = ScopeCallback([cr]() noexcept { cr->LockMapTransfers--; });

    if (map != nullptr) {
        RUNTIME_ASSERT(cr->GetMapId() == map->GetId());

        _engine->OnMapCritterOut.Fire(map, cr);

        for (auto* other : copy_hold_ref(cr->VisCr)) {
            other->OnCritterDisappeared.Fire(cr);
        }

        cr->ClearVisible();
        map->EraseCritter(cr);

        cr->SetMapId({});

        if (!cr->GetIsControlledByPlayer()) {
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

void MapManager::ProcessVisibleCritters(Critter* cr)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (cr->IsDestroyed()) {
        return;
    }

    if (!cr->GetMapId()) {
        // Global map
        RUNTIME_ASSERT(cr->GlobalMapGroup);

        if (cr->GetIsControlledByPlayer()) {
            for (const auto* group_cr : *cr->GlobalMapGroup) {
                if (cr == group_cr) {
                    cr->Send_AddCritter(cr);
                    cr->Send_AddAllItems();
                }
                else {
                    cr->Send_AddCritter(group_cr);
                }
            }
        }
    }
    else {
        // Local map
        auto* map = GetMap(cr->GetMapId());
        RUNTIME_ASSERT(map);

        for (auto* target : copy_hold_ref(map->GetCritters())) {
            optional<bool> trace_result;
            ProcessCritterLook(map, cr, target, trace_result);
            ProcessCritterLook(map, target, cr, trace_result);
        }
    }
}

void MapManager::ProcessCritterLook(Map* map, Critter* cr, Critter* target, optional<bool>& trace_result)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(map && cr && target);

    if (cr == target) {
        return;
    }
    if (map->IsDestroyed() || cr->IsDestroyed() || target->IsDestroyed()) {
        return;
    }
    if (cr->GetMapId() != map->GetId()) {
        return;
    }
    if (target->GetMapId() != map->GetId()) {
        return;
    }

    bool is_see = IsCritterSeeCritter(map, cr, target, trace_result);

    if (!is_see && !target->AttachedCritters.empty()) {
        for (auto* attached_cr : target->AttachedCritters) {
            RUNTIME_ASSERT(attached_cr->GetMapId() == map->GetId());

            optional<bool> dummy_trace_result;
            is_see = IsCritterSeeCritter(map, cr, attached_cr, dummy_trace_result);

            if (is_see) {
                break;
            }
        }
    }

    if (is_see) {
        if (target->AddCrIntoVisVec(cr)) {
            cr->Send_AddCritter(target);
            cr->OnCritterAppeared.Fire(target);
        }
    }
    else {
        if (target->DelCrFromVisVec(cr)) {
            cr->Send_RemoveCritter(target);
            cr->OnCritterDisappeared.Fire(target);
        }
    }

    const uint dist = GeometryHelper::DistGame(cr->GetMapHex(), target->GetMapHex());
    const uint show_cr_dist1 = cr->GetShowCritterDist1();
    const uint show_cr_dist2 = cr->GetShowCritterDist2();
    const uint show_cr_dist3 = cr->GetShowCritterDist3();

    if (show_cr_dist1 != 0) {
        if (show_cr_dist1 >= dist && is_see) {
            if (cr->AddCrIntoVisSet1(target->GetId())) {
                cr->OnCritterAppearedDist1.Fire(target);
            }
        }
        else {
            if (cr->DelCrFromVisSet1(target->GetId())) {
                cr->OnCritterDisappearedDist1.Fire(target);
            }
        }
    }

    if (show_cr_dist2 != 0) {
        if (show_cr_dist2 >= dist && is_see) {
            if (cr->AddCrIntoVisSet2(target->GetId())) {
                cr->OnCritterAppearedDist2.Fire(target);
            }
        }
        else {
            if (cr->DelCrFromVisSet2(target->GetId())) {
                cr->OnCritterDisappearedDist2.Fire(target);
            }
        }
    }

    if (show_cr_dist3 != 0) {
        if (show_cr_dist3 >= dist && is_see) {
            if (cr->AddCrIntoVisSet3(target->GetId())) {
                cr->OnCritterAppearedDist3.Fire(target);
            }
        }
        else {
            if (cr->DelCrFromVisSet3(target->GetId())) {
                cr->OnCritterDisappearedDist3.Fire(target);
            }
        }
    }
}

auto MapManager::IsCritterSeeCritter(Map* map, Critter* cr, Critter* target, optional<bool>& trace_result) -> bool
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!map->IsDestroyed());
    RUNTIME_ASSERT(!cr->IsDestroyed());
    RUNTIME_ASSERT(!target->IsDestroyed());

    bool is_see;

    if (IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_SCRIPT)) {
        is_see = _engine->OnMapCheckLook.Fire(map, cr, target);
    }
    else {
        uint dist = GeometryHelper::DistGame(cr->GetMapHex(), target->GetMapHex());
        uint look_dist = cr->GetLookDistance();
        const int cr_dir = cr->GetDir();

        // Dir modifier
        if (IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_DIR)) {
            const auto real_dir = GeometryHelper::GetFarDir(cr->GetMapHex(), target->GetMapHex());
            auto dir_index = cr_dir > real_dir ? cr_dir - real_dir : real_dir - cr_dir;

            if (dir_index > static_cast<int>(GameSettings::MAP_DIR_COUNT / 2)) {
                dir_index = static_cast<int>(GameSettings::MAP_DIR_COUNT) - dir_index;
            }

            look_dist -= look_dist * _engine->Settings.LookDir[dir_index] / 100;
        }

        if (dist > look_dist) {
            dist = std::numeric_limits<uint>::max();
        }

        // Trace
        if (IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_TRACE) && dist != std::numeric_limits<uint>::max()) {
            if (!trace_result.has_value()) {
                TraceData trace;
                trace.TraceMap = map;
                trace.StartHex = cr->GetMapHex();
                trace.TargetHex = target->GetMapHex();

                TraceBullet(trace);

                if (!trace.IsFullTrace) {
                    dist = std::numeric_limits<uint>::max();
                }

                trace_result = trace.IsFullTrace;
            }
            else {
                if (!trace_result.value()) {
                    dist = std::numeric_limits<uint>::max();
                }
            }
        }

        // Sneak
        if (target->GetIsHide() && dist != std::numeric_limits<uint>::max()) {
            auto sneak_opp = target->GetSneakCoefficient();

            if (IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_SNEAK_DIR)) {
                const auto real_dir = GeometryHelper::GetFarDir(cr->GetMapHex(), target->GetMapHex());
                auto dir_index = (cr_dir > real_dir ? cr_dir - real_dir : real_dir - cr_dir);

                if (dir_index > static_cast<int>(GameSettings::MAP_DIR_COUNT / 2)) {
                    dir_index = static_cast<int>(GameSettings::MAP_DIR_COUNT) - dir_index;
                }

                sneak_opp -= sneak_opp * _engine->Settings.LookSneakDir[dir_index] / 100;
            }

            sneak_opp /= _engine->Settings.SneakDivider;
            look_dist = look_dist > sneak_opp ? look_dist - sneak_opp : 0;
        }

        if (look_dist < _engine->Settings.LookMinimum) {
            look_dist = _engine->Settings.LookMinimum;
        }

        is_see = look_dist >= dist;
    }

    return is_see;
}

void MapManager::ProcessVisibleItems(Critter* cr)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (cr->IsDestroyed()) {
        return;
    }

    const auto map_id = cr->GetMapId();

    if (!map_id) {
        return;
    }

    auto* map = GetMap(map_id);
    RUNTIME_ASSERT(map);

    const int look = static_cast<int>(cr->GetLookDistance());

    for (auto* item : copy_hold_ref(map->GetItems())) {
        if (item->IsDestroyed()) {
            continue;
        }
        if (item->GetIsHidden()) {
            continue;
        }

        if (item->GetIsAlwaysView()) {
            if (cr->AddIdVisItem(item->GetId())) {
                cr->Send_AddItemOnMap(item);
                cr->OnItemOnMapAppeared.Fire(item, nullptr);
            }
        }
        else {
            bool allowed;

            if (item->GetIsTrap() && IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_ITEM_SCRIPT)) {
                allowed = _engine->OnMapCheckTrapLook.Fire(map, cr, item);
            }
            else {
                int dist = static_cast<int>(GeometryHelper::DistGame(cr->GetMapHex(), item->GetMapHex()));
                if (item->GetIsTrap()) {
                    dist += item->GetTrapValue();
                }

                allowed = look >= dist;
            }

            if (allowed) {
                if (cr->AddIdVisItem(item->GetId())) {
                    cr->Send_AddItemOnMap(item);
                    cr->OnItemOnMapAppeared.Fire(item, nullptr);
                }
            }
            else {
                if (cr->DelIdVisItem(item->GetId())) {
                    cr->Send_EraseItemFromMap(item);
                    cr->OnItemOnMapDisappeared.Fire(item, nullptr);
                }
            }
        }
    }
}

void MapManager::ViewMap(Critter* view_cr, Map* map, uint look, mpos hex, int dir)
{
    STACK_TRACE_ENTRY();

    // Critters
    constexpr auto dirs_count = GameSettings::MAP_DIR_COUNT;

    for (auto* cr : copy_hold_ref(map->GetCritters())) {
        if (cr->IsDestroyed()) {
            continue;
        }
        if (cr == view_cr) {
            continue;
        }

        if (IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_SCRIPT)) {
            if (_engine->OnMapCheckLook.Fire(map, view_cr, cr)) {
                view_cr->Send_AddCritter(cr);
            }

            continue;
        }

        const auto dist = GeometryHelper::DistGame(hex, cr->GetMapHex());
        auto look_self = look;

        // Dir modifier
        if (IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_DIR)) {
            const auto real_dir = GeometryHelper::GetFarDir(hex, cr->GetMapHex());
            auto i = (dir > real_dir ? dir - real_dir : real_dir - dir);

            if (i > static_cast<int>(dirs_count / 2u)) {
                i = static_cast<int>(dirs_count) - i;
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
            trace.StartHex = hex;
            trace.TargetHex = cr->GetMapHex();

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
                const auto real_dir = GeometryHelper::GetFarDir(hex, cr->GetMapHex());
                auto i = dir > real_dir ? dir - real_dir : real_dir - dir;

                if (i > static_cast<int>(dirs_count / 2u)) {
                    i = static_cast<int>(dirs_count) - i;
                }

                sneak_opp -= sneak_opp * _engine->Settings.LookSneakDir[i] / 100;
            }

            sneak_opp /= _engine->Settings.SneakDivider;
            vis = look_self > sneak_opp ? look_self - sneak_opp : 0;
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
    for (auto* item : copy_hold_ref(map->GetItems())) {
        if (item->IsDestroyed()) {
            continue;
        }
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
                auto dist = GeometryHelper::DistGame(hex, item->GetMapHex());

                if (item->GetIsTrap()) {
                    dist += item->GetTrapValue();
                }

                allowed = look >= dist;
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
            known_locs.erase(known_locs.begin() + static_cast<ptrdiff_t>(i));
            cr->SetKnownLocations(known_locs);
            break;
        }
    }
}
