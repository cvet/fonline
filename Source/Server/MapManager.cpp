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
// Copyright (c) 2006 - 2024, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

auto MapManager::GridAt(int x, int y) -> int16&
{
    NO_STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    return _mapGrid[((FPATH_MAX_PATH + 1) + y - _mapGridOffsY) * (FPATH_MAX_PATH * 2 + 2) + ((FPATH_MAX_PATH + 1) + x - _mapGridOffsX)];
}

void MapManager::LoadFromResources()
{
    STACK_TRACE_ENTRY();

    auto map_files = _engine->Resources.FilterFiles("fomapb");

    while (map_files.MoveNext()) {
        auto map_file = map_files.GetCurFile();
        auto reader = DataReader({map_file.GetBuf(), map_file.GetSize()});

        const auto map_pid = _engine->ToHashedString(map_file.GetName());
        const auto* map_proto = _engine->ProtoMngr.GetProtoMap(map_pid);

        auto&& static_map = std::make_unique<StaticMap>();
        const auto map_width = map_proto->GetWidth();
        const auto map_height = map_proto->GetHeight();

        if (_engine->Settings.ProtoMapStaticGrid) {
            static_map->HexField = std::make_unique<StaticTwoDimensionalGrid<StaticMap::Field, uint16>>(map_width, map_height);
        }
        else {
            static_map->HexField = std::make_unique<DynamicTwoDimensionalGrid<StaticMap::Field, uint16>>(map_width, map_height);
        }

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
                    const auto hx = cr->GetHexX();
                    const auto hy = cr->GetHexY();

                    if (hx >= map_width || hy >= map_height) {
                        throw MapManagerException("Invalid critter position on map", map_proto->GetName(), cr->GetName(), hx, hy);
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
                        const auto hx = item->GetHexX();
                        const auto hy = item->GetHexY();

                        if (hx >= map_width || hy >= map_height) {
                            throw MapManagerException("Invalid item position on map", map_proto->GetName(), item->GetName(), hx, hy);
                        }
                    }

                    // Bind scripts
                    if (item->GetSceneryScript()) {
                        item->SceneryScriptFunc = _engine->ScriptSys->FindFunc<bool, Critter*, StaticItem*, Item*, int>(item->GetSceneryScript());

                        if (!item->SceneryScriptFunc) {
                            throw MapManagerException("Can't bind static item scenery function", map_proto->GetName(), item->GetSceneryScript());
                        }
                    }

                    if (item->GetTriggerScript()) {
                        item->TriggerScriptFunc = _engine->ScriptSys->FindFunc<void, Critter*, StaticItem*, bool, uint8>(item->GetTriggerScript());

                        if (!item->TriggerScriptFunc) {
                            throw MapManagerException("Can't bind static item trigger function", map_proto->GetName(), item->GetTriggerScript());
                        }
                    }

                    // Sort
                    if (item->GetIsStatic()) {
                        RUNTIME_ASSERT(item->GetOwnership() == ItemOwnership::MapHex);

                        const auto hx = item->GetHexX();
                        const auto hy = item->GetHexY();

                        if (!item->GetIsHiddenInStatic()) {
                            static_map->StaticItems.emplace_back(item);
                            static_map->StaticItemsById.emplace(item_id, item);

                            auto& static_field = static_map->HexField->GetCellForWriting(hx, hy);
                            static_field.StaticItems.emplace_back(item);
                        }

                        if (item->GetIsTrigger() || item->GetIsTrap()) {
                            auto& static_field = static_map->HexField->GetCellForWriting(hx, hy);
                            static_field.TriggerItems.emplace_back(item);
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

            const auto hx = item->GetHexX();
            const auto hy = item->GetHexY();

            if (!item->GetIsNoBlock()) {
                auto& static_field = static_map->HexField->GetCellForWriting(hx, hy);
                static_field.IsMoveBlocked = true;
            }

            if (!item->GetIsShootThru()) {
                auto& static_field = static_map->HexField->GetCellForWriting(hx, hy);
                static_field.IsShootBlocked = true;
                static_field.IsMoveBlocked = true;
            }

            // Block around scroll blocks
            if (item->GetIsScrollBlock()) {
                for (uint8 k = 0; k < GameSettings::MAP_DIR_COUNT; k++) {
                    auto hx2 = hx;
                    auto hy2 = hy;

                    if (GeometryHelper::MoveHexByDir(hx2, hy2, k, map_width, map_height)) {
                        auto& static_field = static_map->HexField->GetCellForWriting(hx2, hy2);
                        static_field.IsMoveBlocked = true;
                    }
                }
            }

            if (item->IsNonEmptyBlockLines()) {
                const auto shooted = item->GetIsShootThru();

                GeometryHelper::ForEachBlockLines(item->GetBlockLines(), hx, hy, map_width, map_height, [&static_map, shooted](uint16 hx2, uint16 hy2) {
                    auto& static_field2 = static_map->HexField->GetCellForWriting(hx2, hy2);
                    static_field2.IsMoveBlocked = true;

                    if (!shooted) {
                        static_field2.IsShootBlocked = true;
                    }
                });
            }
        }

        static_map->CritterBillets.shrink_to_fit();
        static_map->ItemBillets.shrink_to_fit();
        static_map->HexItemBillets.shrink_to_fit();
        static_map->ChildItemBillets.shrink_to_fit();
        static_map->StaticItems.shrink_to_fit();

        _staticMaps.emplace(map_proto, std::move(static_map));
    }

    RUNTIME_ASSERT(_engine->ProtoMngr.GetProtoMaps().size() == _staticMaps.size());
}

auto MapManager::GetStaticMap(const ProtoMap* proto) const -> const StaticMap*
{
    STACK_TRACE_ENTRY();

    const auto it = _staticMaps.find(proto);
    RUNTIME_ASSERT(it != _staticMaps.end());

    return it->second.get();
}

void MapManager::GenerateMapContent(Map* map)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    unordered_map<ident_t, ident_t> id_map;

    // Generate critters
    for (auto&& [base_cr_id, base_cr] : map->GetStaticMap()->CritterBillets) {
        const auto* cr = _engine->CrMngr.CreateCritterOnMap(base_cr->GetProtoId(), &base_cr->GetProperties(), map, base_cr->GetHexX(), base_cr->GetHexY(), base_cr->GetDir());

        id_map.emplace(base_cr_id, cr->GetId());
    }

    // Generate hex items
    for (auto&& [base_item_id, base_item] : map->GetStaticMap()->HexItemBillets) {
        auto* item = _engine->ItemMngr.CreateItem(base_item->GetProtoId(), 0, &base_item->GetProperties());

        id_map.emplace(base_item_id, item->GetId());

        if (item->GetIsCanOpen() && item->GetOpened()) {
            item->SetIsLightThru(true);
        }

        map->AddItem(item, base_item->GetHexX(), base_item->GetHexY(), nullptr);
    }

    // Add children items
    for (auto&& [base_item_id, base_item] : map->GetStaticMap()->ChildItemBillets) {
        // Map id to owner
        ident_t owner_id;

        if (base_item->GetOwnership() == ItemOwnership::CritterInventory) {
            owner_id = base_item->GetCritterId();
        }
        else if (base_item->GetOwnership() == ItemOwnership::ItemContainer) {
            owner_id = base_item->GetContainerId();
        }
        else {
            throw UnreachablePlaceException(LINE_STR);
        }

        if (id_map.count(owner_id) == 0) {
            continue;
        }

        owner_id = id_map[owner_id];

        // Create item
        auto* item = _engine->ItemMngr.CreateItem(base_item->GetProtoId(), 0, &base_item->GetProperties());

        // Add to parent
        if (base_item->GetOwnership() == ItemOwnership::CritterInventory) {
            auto* cr_cont = map->GetCritter(owner_id);
            RUNTIME_ASSERT(cr_cont);

            _engine->CrMngr.AddItemToCritter(cr_cont, item, false);
        }
        else if (base_item->GetOwnership() == ItemOwnership::ItemContainer) {
            auto* item_cont = map->GetItem(owner_id);
            RUNTIME_ASSERT(item_cont);

            item_cont->AddItemToContainer(item, ContainerItemStack::Root);
        }
        else {
            throw UnreachablePlaceException(LINE_STR);
        }
    }
}

void MapManager::DestroyMapContent(Map* map)
{
    STACK_TRACE_ENTRY();

    for (InfinityLoopDetector detector; !map->_critters.empty() || !map->_items.empty(); detector.AddLoop()) {
        KickPlayersToGlobalMap(map);

        for (auto* del_npc : copy(map->_nonPlayerCritters)) {
            _engine->CrMngr.DestroyCritter(del_npc);
        }
        for (auto* del_item : copy(map->_items)) {
            _engine->ItemMngr.DestroyItem(del_item);
        }
    }

    RUNTIME_ASSERT(map->_itemsMap.empty());
}

auto MapManager::GetLocationAndMapsStatistics() const -> string
{
    STACK_TRACE_ENTRY();

    const auto& locations = _engine->EntityMngr.GetLocations();
    const auto& maps = _engine->EntityMngr.GetMaps();

    string result = strex("Locations count: {}\n", static_cast<uint>(locations.size()));
    result += strex("Maps count: {}\n", static_cast<uint>(maps.size()));
    result += "Location             Id           X     Y     Radius Color    Hidden  GeckVisible GeckCount AutoGarbage ToGarbage\n";
    result += "          Map                 Id          Time Rain Script\n";

    for (auto&& [id, loc] : locations) {
        result += strex("{:<20} {:<10}   {:<5} {:<5} {:<6} {:<10} {:<7} {:<11} {:<9} {:<11} {:<5}\n", loc->GetName(), loc->GetId(), loc->GetWorldX(), loc->GetWorldY(), loc->GetRadius(), loc->GetColor(), loc->GetHidden() ? "true" : "false", loc->GetGeckVisible() ? "true" : "false", loc->GeckCount, loc->GetAutoGarbage() ? "true" : "false", loc->GetToGarbage() ? "true" : "false");

        uint map_index = 0;

        for (const auto* map : loc->GetMaps()) {
            result += strex("     {:02}) {:<20} {:<9}   {:<4} {:<4} ", map_index, map->GetName(), map->GetId(), map->GetCurDayTime(), map->GetRainCapacity());
            result += map->GetInitScript();
            result += "\n";
            map_index++;
        }
    }

    return result;
}

auto MapManager::CreateLocation(hstring proto_id, uint16 wx, uint16 wy) -> Location*
{
    STACK_TRACE_ENTRY();

    const auto* proto = _engine->ProtoMngr.GetProtoLocation(proto_id);

    if (wx >= GM_MAXZONEX * _engine->Settings.GlobalMapZoneLength || wy >= GM_MAXZONEY * _engine->Settings.GlobalMapZoneLength) {
        throw MapManagerException("Invalid location coordinates", proto_id);
    }

    auto* loc = new Location(_engine, ident_t {}, proto);
    auto loc_holder = RefCountHolder(loc);

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

        vec_add_unique_value(map_ids, map->GetId());
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

    return loc;
}

auto MapManager::CreateMap(hstring proto_id, Location* loc) -> Map*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto* proto = _engine->ProtoMngr.GetProtoMap(proto_id);
    const auto* static_map = GetStaticMap(proto);

    auto* map = new Map(_engine, ident_t {}, proto, loc, static_map);
    map->SetLocId(loc->GetId());

    auto& maps = loc->GetMapsRaw();
    map->SetLocMapIndex(static_cast<uint>(maps.size()));
    maps.emplace_back(map);

    _engine->EntityMngr.RegisterEntity(map);

    return map;
}

void MapManager::RegenerateMap(Map* map)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!map->IsDestroyed());

    auto map_holder = RefCountHolder(map);

    DestroyMapContent(map);

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

auto MapManager::GetMapByPid(hstring map_pid, uint skip_count) noexcept -> Map*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    for (auto&& [id, map] : _engine->EntityMngr.GetMaps()) {
        if (map->GetProtoId() == map_pid) {
            if (skip_count == 0) {
                return map;
            }

            skip_count--;
        }
    }

    return nullptr;
}

auto MapManager::GetLocationByPid(hstring loc_pid, uint skip_count) noexcept -> Location*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    for (auto&& [id, loc] : _engine->EntityMngr.GetLocations()) {
        if (loc->GetProtoId() == loc_pid) {
            if (skip_count == 0) {
                return loc;
            }

            skip_count--;
        }
    }

    return nullptr;
}

auto MapManager::IsIntersectZone(int wx1, int wy1, int w1_radius, int wx2, int wy2, int w2_radius, int zones) const noexcept -> bool
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
        if (!loc->IsDestroyed() && loc->IsLocVisible() && IsIntersectZone(wx, wy, 0, loc->GetWorldX(), loc->GetWorldY(), loc->GetRadius(), zone_radius)) {
            locs.emplace_back(loc);
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

void MapManager::LocationGarbager()
{
    STACK_TRACE_ENTRY();

    if (_runGarbager) {
        _runGarbager = false;

        for (auto* loc : copy_hold_ref(_engine->EntityMngr.GetLocations())) {
            if (!loc->IsDestroyed() && loc->GetAutoGarbage() && loc->IsCanDelete()) {
                DestroyLocation(loc);
            }
        }
    }
}

void MapManager::DestroyLocation(Location* loc)
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

    // Inner entites
    for (InfinityLoopDetector detector;; detector.AddLoop()) {
        bool has_entities = false;

        if (loc->HasInnerEntities()) {
            _engine->EntityMngr.DestroyInnerEntities(loc);
            has_entities = true;
        }

        for (auto* map : maps) {
            if (map->HasInnerEntities()) {
                _engine->EntityMngr.DestroyInnerEntities(map);
                has_entities = true;
            }
        }

        if (!has_entities) {
            break;
        }
    }

    // Inform players on global map about this
    for (auto* cr : _engine->CrMngr.GetPlayerCritters(true)) {
        if (CheckKnownLoc(cr, loc->GetId())) {
            cr->Send_GlobalLocation(loc, false);
        }
    }

    // Destroy maps
    for (auto* map : maps) {
        DestroyMapContent(map);
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
        dist = GeometryHelper::DistGame(hx, hy, tx, ty);
    }

    auto cx = hx;
    auto cy = hy;
    auto old_cx = cx;
    auto old_cy = cy;

    LineTracer line_tracer(hx, hy, tx, ty, maxhx, maxhy, trace.Angle);

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
            dir = line_tracer.GetNextHex(cx, cy);
        }
        else {
            line_tracer.GetNextSquare(cx, cy);
            dir = GeometryHelper::GetNearDir(old_cx, old_cy, cx, cy);
        }

        if (trace.HexCallback != nullptr) {
            trace.HexCallback(map, trace.FindCr, old_cx, old_cy, cx, cy, dir);
            old_cx = cx;
            old_cy = cy;
            continue;
        }

        if (trace.LastMovable != nullptr && !last_passed_ok) {
            if (map->IsHexMovable(cx, cy)) {
                trace.LastMovable->first = cx;
                trace.LastMovable->second = cy;
                trace.IsHaveLastMovable = true;
            }
            else {
                last_passed_ok = true;
            }
        }

        if (!map->IsHexShootable(cx, cy)) {
            break;
        }

        if (trace.Critters != nullptr && map->IsAnyCritter(cx, cy)) {
            const auto critters = map->GetCritters(cx, cy, 0, trace.FindType);
            trace.Critters->insert(trace.Critters->end(), critters.begin(), critters.end());
        }

        if (trace.FindCr != nullptr && map->IsCritter(cx, cy, trace.FindCr)) {
            trace.IsCritterFound = true;
            break;
        }

        old_cx = cx;
        old_cy = cy;
    }

    if (trace.PreBlock != nullptr) {
        trace.PreBlock->first = old_cx;
        trace.PreBlock->second = old_cy;
    }
    if (trace.Block != nullptr) {
        trace.Block->first = cx;
        trace.Block->second = cy;
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

    auto* map = _engine->EntityMngr.GetMap(input.MapId);

    if (map == nullptr) {
        output.Result = FindPathOutput::ResultType::MapNotFound;
        return output;
    }

    const auto map_width = map->GetWidth();
    const auto map_height = map->GetHeight();

    if (input.FromHexX >= map_width || input.FromHexY >= map_height || input.ToHexX >= map_width || input.ToHexY >= map_height) {
        output.Result = FindPathOutput::ResultType::InvalidHexes;
        return output;
    }
    if (GeometryHelper::CheckDist(input.FromHexX, input.FromHexY, input.ToHexX, input.ToHexY, input.Cut)) {
        output.Result = FindPathOutput::ResultType::AlreadyHere;
        return output;
    }
    if (input.Cut == 0 && !map->IsHexesMovable(input.ToHexX, input.ToHexY, input.Multihex, input.TraceCr)) {
        output.Result = FindPathOutput::ResultType::HexBusy;
        return output;
    }

    // Ring check
    if (input.Cut <= 1 && input.Multihex == 0) {
        auto&& [rsx, rsy] = _engine->Geometry.GetHexOffsets((input.ToHexX % 2) != 0);

        uint i = 0;

        for (; i < GameSettings::MAP_DIR_COUNT; i++, rsx++, rsy++) {
            const auto xx = static_cast<int>(input.ToHexX + *rsx);
            const auto yy = static_cast<int>(input.ToHexY + *rsy);

            if (xx >= 0 && xx < map_width && yy >= 0 && yy < map_height) {
                if (map->IsItemGag(static_cast<uint16>(xx), static_cast<uint16>(yy))) {
                    break;
                }
                if (map->IsHexMovable(static_cast<uint16>(xx), static_cast<uint16>(yy))) {
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

    int16 numindex = 1;
    std::memset(_mapGrid, 0, static_cast<size_t>(FPATH_MAX_PATH * 2 + 2) * (FPATH_MAX_PATH * 2 + 2) * sizeof(int16));
    GridAt(input.FromHexX, input.FromHexY) = numindex;

    auto to_hx = input.ToHexX;
    auto to_hy = input.ToHexY;

    vector<pair<uint16, uint16>> coords;
    vector<pair<uint16, uint16>> cr_coords;
    vector<pair<uint16, uint16>> gag_coords;
    coords.reserve(10000);
    cr_coords.reserve(100);
    gag_coords.reserve(100);

    // First point
    coords.emplace_back(input.FromHexX, input.FromHexY);

    // Begin search
    auto p = 0;
    auto p_togo = 1;
    uint16 cx = 0;
    uint16 cy = 0;

    while (true) {
        for (auto i = 0; i < p_togo; i++, p++) {
            cx = coords[p].first;
            cy = coords[p].second;
            numindex = GridAt(cx, cy);

            if (GeometryHelper::CheckDist(cx, cy, to_hx, to_hy, input.Cut)) {
                to_hx = cx;
                to_hy = cy;
                goto label_FindOk;
            }
            if (++numindex > FPATH_MAX_PATH) {
                output.Result = FindPathOutput::ResultType::TooFar;
                return output;
            }

            auto&& [sx, sy] = _engine->Geometry.GetHexOffsets((cx % 2) != 0);

            for (uint j = 0; j < GameSettings::MAP_DIR_COUNT; j++) {
                const auto nxi = cx + sx[j];
                const auto nyi = cy + sy[j];
                if (nxi < 0 || nyi < 0 || nxi >= map_width || nyi >= map_height) {
                    continue;
                }

                const auto nx = static_cast<uint16>(nxi);
                const auto ny = static_cast<uint16>(nyi);
                auto& grid_cell = GridAt(nx, ny);
                if (grid_cell != 0) {
                    continue;
                }

                if (map->IsHexesMovable(nx, ny, input.Multihex, input.FromCritter)) {
                    coords.emplace_back(nx, ny);
                    grid_cell = numindex;
                }
                else if (input.CheckGagItems && map->IsItemGag(nx, ny)) {
                    gag_coords.emplace_back(nx, ny);
                    grid_cell = static_cast<int16>(numindex | 0x4000);
                }
                else if (input.CheckCritter && map->IsNonDeadCritter(nx, ny)) {
                    cr_coords.emplace_back(nx, ny);
                    grid_cell = static_cast<int16>(numindex | 0x8000);
                }
                else {
                    grid_cell = -1;
                }
            }
        }

        // Add gag hex after some distance
        if (!gag_coords.empty()) {
            auto last_index = GridAt(coords.back().first, coords.back().second);
            auto& xy = gag_coords.front();
            auto gag_index = static_cast<int16>(GridAt(xy.first, xy.second) ^ static_cast<int16>(0x4000));
            if (gag_index + 10 < last_index) // Todo: if path finding not be reworked than migrate magic number to scripts
            {
                GridAt(xy.first, xy.second) = gag_index;
                coords.emplace_back(xy);
                gag_coords.erase(gag_coords.begin());
            }
        }

        // Add gag and critters hexes
        p_togo = static_cast<int>(coords.size()) - p;
        if (p_togo == 0) {
            if (!gag_coords.empty()) {
                auto& xy = gag_coords.front();
                GridAt(xy.first, xy.second) ^= 0x4000;
                coords.emplace_back(xy);
                gag_coords.erase(gag_coords.begin());
                p_togo++;
            }
            else if (!cr_coords.empty()) {
                auto& xy = cr_coords.front();
                GridAt(xy.first, xy.second) ^= static_cast<int16>(0x8000);
                coords.emplace_back(xy);
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
    float base_angle = GeometryHelper::GetDirAngle(to_hx, to_hy, input.FromHexX, input.FromHexY);

    while (numindex > 1) {
        numindex--;

        const auto find_path_grid = [this, &input, base_angle, &raw_steps, &numindex](uint16& x1, uint16& y1) -> bool {
            int best_step_dir = -1;
            float best_step_angle_diff = 0.0f;

            const auto check_hex = [&best_step_dir, &best_step_angle_diff, &input, numindex, base_angle, this](int dir, int step_hx, int step_hy) {
                if (GridAt(step_hx, step_hy) == numindex) {
                    const float angle = GeometryHelper::GetDirAngle(step_hx, step_hy, input.FromHexX, input.FromHexY);
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
        uint16 check_hx = input.FromHexX;
        uint16 check_hy = input.FromHexY;

        for (size_t i = 0; i < raw_steps.size(); i++) {
            const auto prev_check_hx = check_hx;
            const auto prev_check_hy = check_hy;

            const auto move_ok = GeometryHelper::MoveHexByDir(check_hx, check_hy, raw_steps[i], map_width, map_height);
            RUNTIME_ASSERT(move_ok);

            if (input.CheckGagItems && map->IsItemGag(check_hx, check_hy)) {
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

            if (input.CheckCritter && map->IsNonDeadCritter(check_hx, check_hy)) {
                Critter* cr = map->GetNonDeadCritter(check_hx, check_hy);
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
    if (input.TraceDist != 0) {
        vector<int> trace_seq;
        uint16 targ_hx = input.TraceCr->GetHexX();
        uint16 targ_hy = input.TraceCr->GetHexY();
        bool trace_ok = false;

        trace_seq.resize(raw_steps.size() + 4);

        uint16 check_hx = input.FromHexX;
        uint16 check_hy = input.FromHexY;

        for (size_t i = 0; i < raw_steps.size(); i++) {
            const auto move_ok = GeometryHelper::MoveHexByDir(check_hx, check_hy, raw_steps[i], map_width, map_height);
            RUNTIME_ASSERT(move_ok);

            if (map->IsItemGag(check_hx, check_hy)) {
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
            uint16 check_hx2 = input.FromHexX;
            uint16 check_hy2 = input.FromHexY;

            for (size_t i = 0; i < raw_steps.size() - 1; i++) {
                const auto move_ok2 = GeometryHelper::MoveHexByDir(check_hx2, check_hy2, raw_steps[i], map_width, map_height);
                RUNTIME_ASSERT(move_ok2);

                if (k < 4 && trace_seq[i + 2] != k) {
                    continue;
                }
                if (k == 4 && trace_seq[i + 2] < 4) {
                    continue;
                }

                if (!GeometryHelper::CheckDist(check_hx2, check_hy2, targ_hx, targ_hy, input.TraceDist)) {
                    continue;
                }

                trace.BeginHx = check_hx2;
                trace.BeginHy = check_hy2;

                TraceBullet(trace);

                if (trace.IsCritterFound) {
                    trace_ok = true;
                    raw_steps.resize(i + 1);
                    const auto move_ok3 = GeometryHelper::MoveHexByDir(check_hx2, check_hy2, raw_steps[i + 1], map_width, map_height);
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
        uint16 trace_hx = input.FromHexX;
        uint16 trace_hy = input.FromHexY;

        while (true) {
            uint16 trace_tx = to_hx;
            uint16 trace_ty = to_hy;

            for (auto i = static_cast<int>(raw_steps.size()) - 1; i >= 0; i--) {
                LineTracer tracer(trace_hx, trace_hy, trace_tx, trace_ty, map_width, map_height, 0.0f);
                uint16 next_hx = trace_hx;
                uint16 next_hy = trace_hy;
                vector<uint8> direct_steps;
                bool failed = false;

                while (true) {
                    uint8 dir = tracer.GetNextHex(next_hx, next_hy);
                    direct_steps.emplace_back(dir);

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
                    GeometryHelper::MoveHexByDir(trace_tx, trace_ty, GeometryHelper::ReverseDir(raw_steps[i]), map_width, map_height);
                    continue;
                }

                for (const auto& ds : direct_steps) {
                    output.Steps.emplace_back(ds);
                }

                output.ControlSteps.emplace_back(static_cast<uint16>(output.Steps.size()));

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
            output.Steps.emplace_back(cur_dir);

            for (size_t j = i + 1; j < raw_steps.size(); j++) {
                if (raw_steps[j] == cur_dir) {
                    output.Steps.emplace_back(cur_dir);
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
    output.NewToX = to_hx;
    output.NewToY = to_hy;
    return output;
}

void MapManager::TransitToMap(Critter* cr, Map* map, uint16 hx, uint16 hy, uint8 dir, optional<uint> safe_radius)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(map);
    RUNTIME_ASSERT(hx < map->GetWidth());
    RUNTIME_ASSERT(hy < map->GetHeight());

    Transit(cr, map, hx, hy, dir, safe_radius, {});
}

void MapManager::TransitToGlobal(Critter* cr, ident_t global_cr_id)
{
    STACK_TRACE_ENTRY();

    Transit(cr, nullptr, 0, 0, 0, std::nullopt, global_cr_id);
}

void MapManager::Transit(Critter* cr, Map* map, uint16 hx, uint16 hy, uint8 dir, optional<uint> safe_radius, ident_t global_cr_id)
{
    STACK_TRACE_ENTRY();

    if (cr->LockMapTransfers != 0) {
        throw GenericException("Critter transfers locked");
    }

    cr->LockMapTransfers++;
    auto restore_transfers = ScopeCallback([cr]() noexcept { cr->LockMapTransfers--; });

    const auto prev_map_id = cr->GetMapId();
    auto* prev_map = prev_map_id ? _engine->EntityMngr.GetMap(prev_map_id) : nullptr;
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
            RUNTIME_ASSERT(hx < map->GetWidth());
            RUNTIME_ASSERT(hy < map->GetHeight());

            const auto multihex = cr->GetMultihex();

            uint16 start_hx = hx;
            uint16 start_hy = hy;

            if (safe_radius.has_value()) {
                auto start_hex = map->FindStartHex(hx, hy, multihex, safe_radius.value(), true);
                if (!start_hex.has_value()) {
                    start_hex = map->FindStartHex(hx, hy, multihex, safe_radius.value(), false);
                }
                if (start_hex.has_value()) {
                    start_hx = std::get<0>(start_hex.value());
                    start_hy = std::get<1>(start_hex.value());
                }
            }

            cr->ChangeDir(dir);
            map->RemoveCritterFromField(cr);
            cr->SetHexX(start_hx);
            cr->SetHexY(start_hy);
            map->AddCritterToField(cr);
            cr->Send_Teleport(cr, start_hx, start_hy);
            cr->Broadcast_Teleport(start_hx, start_hy);
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
        uint16 start_hx = hx;
        uint16 start_hy = hy;

        if (map != nullptr) {
            const auto multihex = cr->GetMultihex();

            if (safe_radius.has_value()) {
                auto start_hex = map->FindStartHex(hx, hy, multihex, safe_radius.value(), true);
                if (!start_hex.has_value()) {
                    start_hex = map->FindStartHex(hx, hy, multihex, safe_radius.value(), false);
                }
                if (start_hex.has_value()) {
                    start_hx = std::get<0>(start_hex.value());
                    start_hy = std::get<1>(start_hex.value());
                }
            }
        }

        RemoveCritterFromMap(cr, prev_map);
        AddCritterToMap(cr, map, start_hx, start_hy, dir, global_cr_id);

        if (cr->IsDestroyed()) {
            return;
        }

        cr->Send_LoadMap(map);
        cr->Send_AddCritter(cr);

        if (map == nullptr) {
            for (const auto* group_cr : *cr->GlobalMapGroup) {
                if (group_cr != cr) {
                    cr->Send_AddCritter(group_cr);
                }
            }
        }

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
                Transit(attached_cr, map, cr->GetHexX(), cr->GetHexY(), dir, std::nullopt, cr->GetId());
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

void MapManager::AddCritterToMap(Critter* cr, Map* map, uint16 hx, uint16 hy, uint8 dir, ident_t global_cr_id)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    cr->LockMapTransfers++;
    auto restore_transfers = ScopeCallback([cr]() noexcept { cr->LockMapTransfers--; });

    if (map != nullptr) {
        RUNTIME_ASSERT(hx < map->GetWidth() && hy < map->GetHeight());

        cr->SetMapId(map->GetId());
        cr->SetHexX(hx);
        cr->SetHexY(hy);
        cr->ChangeDir(dir);

        if (!cr->GetIsControlledByPlayer()) {
            auto cr_ids = map->GetCritterIds();
            vec_add_unique_value(cr_ids, cr->GetId());
            map->SetCritterIds(cr_ids);
        }

        map->AddCritter(cr);

        _engine->OnMapCritterIn.Fire(map, cr);
    }
    else {
        RUNTIME_ASSERT(!cr->GlobalMapGroup);

        const auto* global_cr = global_cr_id && global_cr_id != cr->GetId() ? _engine->EntityMngr.GetCritter(global_cr_id) : nullptr;

        cr->SetMapId({});

        if (global_cr == nullptr || global_cr->GetMapId()) {
            const auto trip_id = _engine->GetLastGlobalMapTripId() + 1;
            _engine->SetLastGlobalMapTripId(trip_id);
            cr->SetGlobalMapTripId(trip_id);

            cr->GlobalMapGroup = new vector<Critter*>();
            cr->GlobalMapGroup->emplace_back(cr);
        }
        else {
            RUNTIME_ASSERT(global_cr->GlobalMapGroup);

            cr->SetWorldX(global_cr->GetWorldX());
            cr->SetWorldY(global_cr->GetWorldY());
            cr->SetGlobalMapTripId(global_cr->GetGlobalMapTripId());

            for (auto* group_cr : *global_cr->GlobalMapGroup) {
                group_cr->Send_AddCritter(cr);
            }

            cr->GlobalMapGroup = global_cr->GlobalMapGroup;
            cr->GlobalMapGroup->emplace_back(cr);
        }

        _engine->OnGlobalMapCritterIn.Fire(cr);
    }
}

void MapManager::RemoveCritterFromMap(Critter* cr, Map* map)
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
        map->RemoveCritter(cr);
        cr->SetMapId({});

        if (!cr->GetIsControlledByPlayer()) {
            auto cr_ids = map->GetCritterIds();
            vec_remove_unique_value(cr_ids, cr->GetId());
            map->SetCritterIds(cr_ids);
        }

        _runGarbager = true;
    }
    else {
        RUNTIME_ASSERT(!cr->GetMapId());
        RUNTIME_ASSERT(cr->GlobalMapGroup);

        _engine->OnGlobalMapCritterOut.Fire(cr);

        cr->SetGlobalMapTripId({});

        vec_remove_unique_value(*cr->GlobalMapGroup, cr);

        if (!cr->GlobalMapGroup->empty()) {
            for (auto* group_cr : *cr->GlobalMapGroup) {
                group_cr->Send_RemoveCritter(cr);
            }
        }
        else {
            delete cr->GlobalMapGroup;
        }

        cr->GlobalMapGroup = nullptr;
    }
}

void MapManager::ProcessVisibleCritters(Critter* cr)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (cr->IsDestroyed()) {
        return;
    }

    if (const auto map_id = cr->GetMapId()) {
        auto* map = _engine->EntityMngr.GetMap(map_id);
        RUNTIME_ASSERT(map);

        for (auto* target : copy_hold_ref(map->GetCritters())) {
            optional<bool> trace_result;
            ProcessCritterLook(map, cr, target, trace_result);
            ProcessCritterLook(map, target, cr, trace_result);
        }
    }
    else {
        RUNTIME_ASSERT(cr->GlobalMapGroup);
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

    const uint dist = GeometryHelper::DistGame(cr->GetHexX(), cr->GetHexY(), target->GetHexX(), target->GetHexY());
    const auto show_cr_dist1 = cr->GetShowCritterDist1();
    const auto show_cr_dist2 = cr->GetShowCritterDist2();
    const auto show_cr_dist3 = cr->GetShowCritterDist3();

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
        uint dist = GeometryHelper::DistGame(cr->GetHexX(), cr->GetHexY(), target->GetHexX(), target->GetHexY());
        uint look_dist = cr->GetLookDistance();
        const int cr_dir = cr->GetDir();

        // Dir modifier
        if (IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_DIR)) {
            const auto real_dir = GeometryHelper::GetFarDir(cr->GetHexX(), cr->GetHexY(), target->GetHexX(), target->GetHexY());
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
                trace.BeginHx = cr->GetHexX();
                trace.BeginHy = cr->GetHexY();
                trace.EndHx = target->GetHexX();
                trace.EndHy = target->GetHexY();

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
        if (target->GetInSneakMode() && dist != std::numeric_limits<uint>::max()) {
            auto sneak_opp = target->GetSneakCoefficient();

            if (IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_SNEAK_DIR)) {
                const auto real_dir = GeometryHelper::GetFarDir(cr->GetHexX(), cr->GetHexY(), target->GetHexX(), target->GetHexY());
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

    auto* map = _engine->EntityMngr.GetMap(map_id);
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
                int dist = static_cast<int>(GeometryHelper::DistGame(cr->GetHexX(), cr->GetHexY(), item->GetHexX(), item->GetHexY()));

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
                    cr->Send_RemoveItemFromMap(item);
                    cr->OnItemOnMapDisappeared.Fire(item, nullptr);
                }
            }
        }
    }
}

void MapManager::ViewMap(Critter* view_cr, Map* map, uint look, uint16 hx, uint16 hy, int dir)
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

        const auto dist = GeometryHelper::DistGame(hx, hy, cr->GetHexX(), cr->GetHexY());
        auto look_self = look;

        // Dir modifier
        if (IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_DIR)) {
            const auto real_dir = GeometryHelper::GetFarDir(hx, hy, cr->GetHexX(), cr->GetHexY());
            auto i = dir > real_dir ? dir - real_dir : real_dir - dir;

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

        if (cr->GetInSneakMode()) {
            auto sneak_opp = cr->GetSneakCoefficient();

            if (IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_SNEAK_DIR)) {
                const auto real_dir = GeometryHelper::GetFarDir(hx, hy, cr->GetHexX(), cr->GetHexY());
                auto i = (dir > real_dir ? dir - real_dir : real_dir - dir);

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
                auto dist = GeometryHelper::DistGame(hx, hy, item->GetHexX(), item->GetHexY());

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
    known_locs.emplace_back(loc_id);
    cr->SetKnownLocations(known_locs);
}

void MapManager::RemoveKnownLoc(Critter* cr, ident_t loc_id)
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
