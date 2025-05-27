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

FO_BEGIN_NAMESPACE();

MapManager::MapManager(FOServer* engine) :
    _engine {engine}
{
    FO_STACK_TRACE_ENTRY();
}

auto MapManager::GridAt(mpos pos) -> int16&
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto max_path_find_len = _engine->Settings.MaxPathFindLength;
    return _mapGrid[((max_path_find_len + 1) + pos.y - _mapGridOffset.y) * (max_path_find_len * 2 + 2) + ((max_path_find_len + 1) + pos.x - _mapGridOffset.x)];
}

void MapManager::LoadFromResources()
{
    FO_STACK_TRACE_ENTRY();

    auto map_files = _engine->Resources.FilterFiles("fomapb-server");

    std::vector<pair<const ProtoMap*, std::future<unique_ptr<StaticMap>>>> static_map_loadings;

    while (map_files.MoveNext()) {
        auto map_file_0 = map_files.GetCurFile();
        const auto map_pid = _engine->Hashes.ToHashedString(map_file_0.GetName());
        const auto* map_proto = _engine->ProtoMngr.GetProtoMap(map_pid);

        std::launch async_flags;

        if constexpr (FO_DEBUG) {
            async_flags = std::launch::deferred;
        }
        else {
            async_flags = std::launch::async | std::launch::deferred;
        }

        static_map_loadings.emplace_back(map_proto, std::async(async_flags, [this, map_proto, map_file = std::move(map_file_0)]() {
            auto reader = DataReader({map_file.GetBuf(), map_file.GetSize()});

            auto static_map = SafeAlloc::MakeUnique<StaticMap>();
            const auto map_size = map_proto->GetSize();

            if (_engine->Settings.ProtoMapStaticGrid) {
                static_map->HexField = SafeAlloc::MakeUnique<StaticTwoDimensionalGrid<StaticMap::Field, mpos, msize>>(map_size);
            }
            else {
                static_map->HexField = SafeAlloc::MakeUnique<DynamicTwoDimensionalGrid<StaticMap::Field, mpos, msize>>(map_size);
            }

            // Read hashes
            {
                const auto hashes_count = reader.Read<uint32>();

                string str;

                for (uint32 i = 0; i < hashes_count; i++) {
                    const auto str_len = reader.Read<uint32>();
                    str.resize(str_len);
                    reader.ReadPtr(str.data(), str.length());
                    const auto hstr = _engine->Hashes.ToHashedString(str);
                    ignore_unused(hstr);
                }
            }

            // Read entities
            {
                vector<uint8> props_data;

                // Read critters
                {
                    const auto cr_count = reader.Read<uint32>();

                    static_map->CritterBillets.reserve(cr_count);

                    for (const auto i : xrange(cr_count)) {
                        ignore_unused(i);

                        const auto cr_id = ident_t {reader.Read<ident_t::underlying_type>()};

                        const auto cr_pid_hash = reader.Read<hstring::hash_t>();
                        const auto cr_pid = _engine->Hashes.ResolveHash(cr_pid_hash);
                        const auto* cr_proto = _engine->ProtoMngr.GetProtoCritter(cr_pid);

                        auto cr_props = Properties(cr_proto->GetProperties().GetRegistrator());
                        const auto props_data_size = reader.Read<uint32>();
                        props_data.resize(props_data_size);
                        reader.ReadPtr<uint8>(props_data.data(), props_data_size);
                        cr_props.RestoreAllData(props_data);

                        auto cr = SafeAlloc::MakeRefCounted<Critter>(_engine, ident_t {}, cr_proto, &cr_props);

                        static_map->CritterBillets.emplace_back(cr_id, cr);

                        // Checks
                        if (const auto hex = cr->GetHex(); !map_size.IsValidPos(hex)) {
                            throw MapManagerException("Invalid critter position on map", map_proto->GetName(), cr->GetName(), hex);
                        }
                    }
                }

                // Read items
                {
                    const auto item_count = reader.Read<uint32>();

                    static_map->ItemBillets.reserve(item_count);
                    static_map->HexItemBillets.reserve(item_count);
                    static_map->ChildItemBillets.reserve(item_count);
                    static_map->StaticItems.reserve(item_count);
                    static_map->StaticItemsById.reserve(item_count);

                    for (const auto i : xrange(item_count)) {
                        ignore_unused(i);

                        const auto item_id = ident_t {reader.Read<ident_t::underlying_type>()};

                        const auto item_pid_hash = reader.Read<hstring::hash_t>();
                        const auto item_pid = _engine->Hashes.ResolveHash(item_pid_hash);
                        const auto* item_proto = _engine->ProtoMngr.GetProtoItem(item_pid);

                        auto item_props = Properties(item_proto->GetProperties().GetRegistrator());
                        const auto props_data_size = reader.Read<uint32>();
                        props_data.resize(props_data_size);
                        reader.ReadPtr<uint8>(props_data.data(), props_data_size);
                        item_props.RestoreAllData(props_data);

                        auto item = SafeAlloc::MakeRefCounted<Item>(_engine, ident_t {}, item_proto, &item_props);

                        static_map->ItemBillets.emplace_back(item_id, item);

                        // Checks
                        if (item->GetOwnership() == ItemOwnership::MapHex) {
                            if (const auto hex = item->GetHex(); !map_size.IsValidPos(hex)) {
                                throw MapManagerException("Invalid item position on map", map_proto->GetName(), item->GetName(), hex);
                            }
                        }

                        // Bind scripts
                        if (const auto static_script = item->GetStaticScript()) {
                            item->StaticScriptFunc = _engine->ScriptSys.FindFunc<bool, Critter*, StaticItem*, Item*, any_t>(static_script);

                            if (!item->StaticScriptFunc) {
                                throw MapManagerException("Can't bind static item function", map_proto->GetName(), static_script);
                            }
                        }

                        if (const auto trigger_script = item->GetTriggerScript()) {
                            item->TriggerScriptFunc = _engine->ScriptSys.FindFunc<void, Critter*, StaticItem*, bool, uint8>(trigger_script);

                            if (!item->TriggerScriptFunc) {
                                throw MapManagerException("Can't bind static item trigger function", map_proto->GetName(), trigger_script);
                            }
                        }

                        // Sort
                        if (item->GetStatic()) {
                            FO_RUNTIME_ASSERT(item->GetOwnership() == ItemOwnership::MapHex);

                            const auto hex = item->GetHex();

                            if (!item->GetHiddenInStatic()) {
                                static_map->StaticItems.emplace_back(item.get());
                                static_map->StaticItemsById.emplace(item_id, item.get());
                                static_map->HexField->GetCellForWriting(hex).StaticItems.emplace_back(item.get());
                            }

                            if (item->GetIsTrigger() || item->GetIsTrap()) {
                                auto& static_field = static_map->HexField->GetCellForWriting(hex);
                                static_field.TriggerItems.emplace_back(item.get());
                            }
                        }
                        else {
                            if (item->GetOwnership() == ItemOwnership::MapHex) {
                                static_map->HexItemBillets.emplace_back(item_id, item.get());
                            }
                            else {
                                FO_RUNTIME_ASSERT(item->GetOwnership() == ItemOwnership::CritterInventory || item->GetOwnership() == ItemOwnership::ItemContainer);
                                static_map->ChildItemBillets.emplace_back(item_id, item.get());
                            }
                        }
                    }
                }
            }

            reader.VerifyEnd();

            // Fill hex flags from static items on map
            for (auto&& [item_id, item] : static_map->ItemBillets) {
                if (item->GetOwnership() != ItemOwnership::MapHex || !item->GetStatic() || item->GetIsTile()) {
                    continue;
                }

                const auto hex = item->GetHex();

                if (!item->GetNoBlock()) {
                    auto& static_field = static_map->HexField->GetCellForWriting(hex);
                    static_field.MoveBlocked = true;
                }

                if (!item->GetShootThru()) {
                    auto& static_field = static_map->HexField->GetCellForWriting(hex);
                    static_field.ShootBlocked = true;
                    static_field.MoveBlocked = true;
                }

                // Block around scroll blocks
                if (item->GetScrollBlock()) {
                    for (uint8 k = 0; k < GameSettings::MAP_DIR_COUNT; k++) {
                        auto scroll_hex = hex;

                        if (GeometryHelper::MoveHexByDir(scroll_hex, k, map_size)) {
                            auto& static_field = static_map->HexField->GetCellForWriting(scroll_hex);
                            static_field.MoveBlocked = true;
                        }
                    }
                }

                if (item->IsNonEmptyBlockLines()) {
                    const auto shooted = item->GetShootThru();

                    GeometryHelper::ForEachBlockLines(item->GetBlockLines(), hex, map_size, [&static_map, shooted](mpos block_hex) {
                        auto& block_static_field = static_map->HexField->GetCellForWriting(block_hex);
                        block_static_field.MoveBlocked = true;

                        if (!shooted) {
                            block_static_field.ShootBlocked = true;
                        }
                    });
                }
            }

            static_map->CritterBillets.shrink_to_fit();
            static_map->ItemBillets.shrink_to_fit();
            static_map->HexItemBillets.shrink_to_fit();
            static_map->ChildItemBillets.shrink_to_fit();
            static_map->StaticItems.shrink_to_fit();

            return static_map;
        }));
    }

    size_t errors = 0;

    for (auto&& static_map_loading : static_map_loadings) {
        try {
            auto static_map = static_map_loading.second.get();
            _staticMaps.emplace(static_map_loading.first, std::move(static_map));
        }
        catch (const std::exception& ex) {
            WriteLog("Failed to load map {}", static_map_loading.first->GetProtoId());
            ReportExceptionAndContinue(ex);
            errors++;
        }
    }

    if (errors != 0) {
        throw MapManagerException("Failed to load maps");
    }
}

auto MapManager::GetStaticMap(const ProtoMap* proto) const -> const StaticMap*
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _staticMaps.find(proto);
    FO_RUNTIME_ASSERT(it != _staticMaps.end());

    return it->second.get();
}

void MapManager::GenerateMapContent(Map* map)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    unordered_map<ident_t, ident_t> id_map;

    // Generate critters
    for (auto&& [base_cr_id, base_cr] : map->GetStaticMap()->CritterBillets) {
        const auto* cr = _engine->CrMngr.CreateCritterOnMap(base_cr->GetProtoId(), &base_cr->GetProperties(), map, base_cr->GetHex(), base_cr->GetDir());

        id_map.emplace(base_cr_id, cr->GetId());
    }

    // Generate hex items
    for (auto&& [base_item_id, base_item] : map->GetStaticMap()->HexItemBillets) {
        auto* item = _engine->ItemMngr.CreateItem(base_item->GetProtoId(), 0, &base_item->GetProperties());

        id_map.emplace(base_item_id, item->GetId());

        if (item->GetCanOpen() && item->GetOpened()) {
            item->SetLightThru(true);
        }

        map->AddItem(item, base_item->GetHex(), nullptr);
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
            FO_UNREACHABLE_PLACE();
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
            FO_RUNTIME_ASSERT(cr_cont);

            _engine->CrMngr.AddItemToCritter(cr_cont, item, false);
        }
        else if (base_item->GetOwnership() == ItemOwnership::ItemContainer) {
            auto* item_cont = map->GetItem(owner_id);
            FO_RUNTIME_ASSERT(item_cont);

            item_cont->AddItemToContainer(item, {});
        }
        else {
            FO_UNREACHABLE_PLACE();
        }
    }
}

void MapManager::DestroyMapContent(Map* map)
{
    FO_STACK_TRACE_ENTRY();

    for (InfinityLoopDetector detector; !map->_critters.empty() || !map->_items.empty(); detector.AddLoop()) {
        KickPlayersToGlobalMap(map);

        for (auto* del_npc : copy(map->_nonPlayerCritters)) {
            _engine->CrMngr.DestroyCritter(del_npc);
        }
        for (auto* del_item : copy(map->_items)) {
            _engine->ItemMngr.DestroyItem(del_item);
        }
    }

    FO_RUNTIME_ASSERT(map->_itemsMap.empty());
}

auto MapManager::GetLocationAndMapsStatistics() const -> string
{
    FO_STACK_TRACE_ENTRY();

    const auto& locations = _engine->EntityMngr.GetLocations();
    const auto& maps = _engine->EntityMngr.GetMaps();

    string result = strex("Locations count: {}\n", locations.size());
    result += strex("Maps count: {}\n", maps.size());
    result += "Location             Id\n";
    result += "          Map                 Id          Time Script\n";

    for (auto&& [id, loc] : locations) {
        result += strex("{:<20} {:<10}\n", loc->GetName(), loc->GetId());

        uint32 map_index = 0;

        for (const auto& map : loc->GetMaps()) {
            result += strex("     {:02}) {:<20} {:<9}   {:<4} {}\n", map_index, map->GetName(), map->GetId(), map->GetFixedDayTime(), map->GetInitScript());
            map_index++;
        }
    }

    return result;
}

auto MapManager::CreateLocation(hstring proto_id, const Properties* props) -> Location*
{
    FO_STACK_TRACE_ENTRY();

    const auto* proto = _engine->ProtoMngr.GetProtoLocation(proto_id);
    auto loc = SafeAlloc::MakeRefCounted<Location>(_engine, ident_t {}, proto, props);

    vector<ident_t> map_ids;

    for (const auto map_pid : loc->GetMapProtos()) {
        const auto* map = CreateMap(map_pid, loc.get());

        if (map == nullptr) {
            throw MapManagerException("Create map for location failed", proto_id, map_pid);
        }

        vec_add_unique_value(map_ids, map->GetId());
    }

    loc->SetMapIds(map_ids);

    _engine->EntityMngr.RegisterLocation(loc.get());

    for (auto& map : copy_hold_ref(loc->GetMaps())) {
        GenerateMapContent(map.get());
    }

    _engine->EntityMngr.CallInit(loc.get(), true);

    if (!loc->IsDestroyed()) {
        for (auto& map : copy_hold_ref(loc->GetMaps())) {
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

    if (loc->IsDestroyed()) {
        throw GenericException("Location destroyed during init");
    }

    return loc.get();
}

auto MapManager::CreateMap(hstring proto_id, Location* loc) -> Map*
{
    FO_STACK_TRACE_ENTRY();

    const auto* proto = _engine->ProtoMngr.GetProtoMap(proto_id);
    const auto* static_map = GetStaticMap(proto);

    auto map = SafeAlloc::MakeRefCounted<Map>(_engine, ident_t {}, proto, loc, static_map);

    loc->AddMap(map.get());

    _engine->EntityMngr.RegisterMap(map.get());

    return map.get();
}

void MapManager::RegenerateMap(Map* map)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!map->IsDestroyed());

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

auto MapManager::GetMapByPid(hstring map_pid, uint32 skip_count) noexcept -> Map*
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

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

auto MapManager::GetLocationByPid(hstring loc_pid, uint32 skip_count) noexcept -> Location*
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

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

void MapManager::KickPlayersToGlobalMap(Map* map)
{
    FO_STACK_TRACE_ENTRY();

    for (auto* cl : map->GetPlayerCritters()) {
        TransitToGlobal(cl, {});
    }
}

void MapManager::DestroyLocation(Location* loc)
{
    FO_STACK_TRACE_ENTRY();

    // Start deleting
    auto loc_holder = RefCountHolder(loc);
    auto maps = copy_hold_ref(loc->GetMaps());

    // Redundant calls
    if (loc->IsDestroying() || loc->IsDestroyed()) {
        return;
    }

    loc->MarkAsDestroying();

    for (auto& map : maps) {
        map->MarkAsDestroying();
    }

    // Finish events
    _engine->OnLocationFinish.Fire(loc);

    for (auto& map : maps) {
        _engine->OnMapFinish.Fire(map.get());
    }

    // Inner entites
    for (InfinityLoopDetector detector;; detector.AddLoop()) {
        bool has_entities = false;

        if (loc->HasInnerEntities()) {
            _engine->EntityMngr.DestroyInnerEntities(loc);
            has_entities = true;
        }

        for (auto& map : maps) {
            if (map->HasInnerEntities()) {
                _engine->EntityMngr.DestroyInnerEntities(map.get());
                has_entities = true;
            }
        }

        if (!has_entities) {
            break;
        }
    }

    // Destroy maps
    for (auto& map : maps) {
        DestroyMapContent(map.get());
    }

    // Invalidate
    for (auto& map : maps) {
        map->MarkAsDestroyed();
    }

    loc->MarkAsDestroyed();

    // Erase from main collections
    _engine->EntityMngr.UnregisterLocation(loc);

    for (auto& map : maps) {
        _engine->EntityMngr.UnregisterMap(map.get());
    }
}

void MapManager::TraceBullet(TraceData& trace)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    auto* map = trace.TraceMap;
    const auto map_size = map->GetSize();
    const auto start_hex = trace.StartHex;
    const auto target_hex = trace.TargetHex;
    const auto dist = trace.MaxDist != 0 ? trace.MaxDist : GeometryHelper::DistGame(start_hex, target_hex);

    auto next_hex = start_hex;
    auto prev_hex = next_hex;

    LineTracer line_tracer(start_hex, target_hex, map_size, trace.Angle);

    trace.IsFullTrace = false;
    trace.IsCritterFound = false;
    trace.HasLastMovable = false;
    bool last_passed_ok = false;

    for (uint32 i = 0;; i++) {
        if (i >= dist) {
            trace.IsFullTrace = true;
            break;
        }

        if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
            line_tracer.GetNextHex(next_hex);
        }
        else {
            line_tracer.GetNextSquare(next_hex);
        }

        if (trace.LastMovable != nullptr && !last_passed_ok) {
            if (map->IsHexMovable(next_hex)) {
                *trace.LastMovable = next_hex;
                trace.HasLastMovable = true;
            }
            else {
                last_passed_ok = true;
            }
        }

        if (!map->IsHexShootable(next_hex)) {
            break;
        }

        if (trace.Critters != nullptr && map->IsCritter(next_hex, CritterFindType::Any)) {
            const auto critters = map->GetCritters(next_hex, trace.FindType);

            for (auto* cr : critters) {
                if (std::ranges::find(*trace.Critters, cr) == trace.Critters->end()) {
                    trace.Critters->emplace_back(cr);
                }
            }
        }

        if (trace.FindCr != nullptr && map->IsCritter(next_hex, trace.FindCr)) {
            trace.IsCritterFound = true;
            break;
        }

        prev_hex = next_hex;
    }

    if (trace.PreBlock != nullptr) {
        *trace.PreBlock = prev_hex;
    }
    if (trace.Block != nullptr) {
        *trace.Block = next_hex;
    }
}

auto MapManager::FindPath(const FindPathInput& input) -> FindPathOutput
{
    FO_STACK_TRACE_ENTRY();

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

        uint32 i = 0;

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
    _mapGrid.resize(static_cast<size_t>(_engine->Settings.MaxPathFindLength * 2 + 2) * (_engine->Settings.MaxPathFindLength * 2 + 2));
    MemFill(_mapGrid.data(), 0, _mapGrid.size() * sizeof(int16));
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

            if (++numindex > _engine->Settings.MaxPathFindLength) {
                output.Result = FindPathOutput::ResultType::TooFar;
                return output;
            }

            auto&& [sx, sy] = _engine->Geometry.GetHexOffsets(cur_hex);

            for (uint32 j = 0; j < GameSettings::MAP_DIR_COUNT; j++) {
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
                else if (input.CheckCritter && map->IsCritter(next_hex, CritterFindType::NonDead)) {
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
                coords.emplace_back(gag_hex);
                gag_coords.erase(gag_coords.begin());
            }
        }

        // Add gag and critters hexes
        p_togo = static_cast<int32>(coords.size()) - p;

        if (p_togo == 0) {
            if (!gag_coords.empty()) {
                auto& gag_hex = gag_coords.front();
                GridAt(gag_hex) ^= 0x4000;
                coords.emplace_back(gag_hex);
                gag_coords.erase(gag_coords.begin());
                p_togo++;
            }
            else if (!cr_coords.empty()) {
                auto& gag_hex = gag_coords.front();
                GridAt(gag_hex) ^= static_cast<int16>(0x8000);
                coords.emplace_back(gag_hex);
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
    float32 base_angle = GeometryHelper::GetDirAngle(to_hex, input.FromHex);

    while (numindex > 1) {
        numindex--;

        const auto find_path_grid = [this, &map_size, &input, base_angle, &raw_steps, &numindex](mpos& hex) -> bool {
            int32 best_step_dir = -1;
            float32 best_step_angle_diff = 0.0f;

            const auto check_hex = [this, &map_size, &best_step_dir, &best_step_angle_diff, &input, numindex, base_angle](int32 dir, ipos step_raw_hex) {
                if (!map_size.IsValidPos(step_raw_hex)) {
                    return;
                }

                const auto step_hex = map_size.FromRawPos(step_raw_hex);

                if (GridAt(step_hex) != numindex) {
                    return;
                }

                const float32 angle = GeometryHelper::GetDirAngle(step_hex, input.FromHex);
                const float32 angle_diff = GeometryHelper::GetDirAngleDiff(base_angle, angle);

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
            FO_RUNTIME_ASSERT(move_ok);

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

            if (input.CheckCritter && map->IsCritter(check_hex, CritterFindType::NonDead)) {
                Critter* cr = map->GetCritter(check_hex, CritterFindType::NonDead);

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
        vector<int32> trace_seq;
        const mpos targ_hex = input.TraceCr->GetHex();
        bool trace_ok = false;

        trace_seq.resize(raw_steps.size() + 4);

        mpos check_hex = input.FromHex;

        for (size_t i = 0; i < raw_steps.size(); i++) {
            const auto move_ok = GeometryHelper::MoveHexByDir(check_hex, raw_steps[i], map_size);
            FO_RUNTIME_ASSERT(move_ok);

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

        for (int32 k = 0; k < 5; k++) {
            mpos check_hex2 = input.FromHex;

            for (size_t i = 0; i < raw_steps.size() - 1; i++) {
                const auto move_ok2 = GeometryHelper::MoveHexByDir(check_hex2, raw_steps[i], map_size);
                FO_RUNTIME_ASSERT(move_ok2);

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
                    FO_RUNTIME_ASSERT(move_ok3);
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

            for (auto i = static_cast<int32>(raw_steps.size()) - 1; i >= 0; i--) {
                LineTracer tracer(trace_hex, trace_hex2, map_size, 0.0f);
                mpos next_hex = trace_hex;
                vector<uint8> direct_steps;
                bool failed = false;

                while (true) {
                    uint8 dir = tracer.GetNextHex(next_hex);
                    direct_steps.emplace_back(dir);

                    if (next_hex == trace_hex2) {
                        break;
                    }

                    if (GridAt(next_hex) <= 0) {
                        failed = true;
                        break;
                    }
                }

                if (failed) {
                    FO_RUNTIME_ASSERT(i > 0);
                    GeometryHelper::MoveHexByDir(trace_hex2, GeometryHelper::ReverseDir(raw_steps[i]), map_size);
                    continue;
                }

                for (const auto& ds : direct_steps) {
                    output.Steps.emplace_back(ds);
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

    FO_RUNTIME_ASSERT(!output.Steps.empty());
    FO_RUNTIME_ASSERT(!output.ControlSteps.empty());

    output.Result = FindPathOutput::ResultType::Ok;
    output.NewToHex = to_hex;

    return output;
}

void MapManager::TransitToMap(Critter* cr, Map* map, mpos hex, uint8 dir, optional<uint32> safe_radius)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(map);
    FO_RUNTIME_ASSERT(map->GetSize().IsValidPos(hex));

    Transit(cr, map, hex, dir, safe_radius, {});
}

void MapManager::TransitToGlobal(Critter* cr, ident_t global_cr_id)
{
    FO_STACK_TRACE_ENTRY();

    Transit(cr, nullptr, {}, 0, std::nullopt, global_cr_id);
}

void MapManager::Transit(Critter* cr, Map* map, mpos hex, uint8 dir, optional<uint32> safe_radius, ident_t global_cr_id)
{
    FO_STACK_TRACE_ENTRY();

    if (cr->LockMapTransfers != 0) {
        throw GenericException("Critter transfers locked");
    }

    cr->LockMapTransfers++;
    auto restore_transfers = ScopeCallback([cr]() noexcept { cr->LockMapTransfers--; });

    const auto prev_map_id = cr->GetMapId();
    auto* prev_map = prev_map_id ? _engine->EntityMngr.GetMap(prev_map_id) : nullptr;
    FO_RUNTIME_ASSERT(!prev_map_id || !!prev_map);

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
            FO_RUNTIME_ASSERT(map->GetSize().IsValidPos(hex));

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
            cr->SetHex(start_hex);
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

        RemoveCritterFromMap(cr, prev_map);
        AddCritterToMap(cr, map, start_hex, dir, global_cr_id);

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

        _engine->OnCritterSendInitialInfo.Fire(cr);

        if (cr->IsDestroyed()) {
            return;
        }

        cr->Send_PlaceToGameComplete();
    }

    // Transit attached critters
    if (!attached_critters.empty()) {
        for (auto* attached_cr : attached_critters) {
            if (!cr->IsDestroyed() && !attached_cr->IsDestroyed() && !attached_cr->GetIsAttached() && attached_cr->GetMapId() == prev_map_id) {
                Transit(attached_cr, map, cr->GetHex(), dir, std::nullopt, cr->GetId());
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

    _engine->OnCritterTransit.Fire(cr, prev_map);
}

void MapManager::AddCritterToMap(Critter* cr, Map* map, mpos hex, uint8 dir, ident_t global_cr_id)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    cr->LockMapTransfers++;
    auto restore_transfers = ScopeCallback([cr]() noexcept { cr->LockMapTransfers--; });

    if (map != nullptr) {
        FO_RUNTIME_ASSERT(map->GetSize().IsValidPos(hex));

        cr->SetMapId(map->GetId());
        cr->SetHex(hex);
        cr->ChangeDir(dir);

        if (!cr->GetControlledByPlayer()) {
            auto cr_ids = map->GetCritterIds();
            vec_add_unique_value(cr_ids, cr->GetId());
            map->SetCritterIds(cr_ids);
        }

        map->AddCritter(cr);

        _engine->OnMapCritterIn.Fire(map, cr);
    }
    else {
        FO_RUNTIME_ASSERT(!cr->GlobalMapGroup);

        const auto* global_cr = global_cr_id && global_cr_id != cr->GetId() ? _engine->EntityMngr.GetCritter(global_cr_id) : nullptr;

        cr->SetMapId({});

        if (global_cr == nullptr || global_cr->GetMapId()) {
            const auto trip_id = _engine->GetLastGlobalMapTripId() + 1;
            _engine->SetLastGlobalMapTripId(trip_id);
            cr->SetGlobalMapTripId(trip_id);

            cr->GlobalMapGroup = SafeAlloc::MakeShared<vector<Critter*>>();
        }
        else {
            FO_RUNTIME_ASSERT(global_cr->GlobalMapGroup);

            cr->SetGlobalMapTripId(global_cr->GetGlobalMapTripId());

            for (auto* group_cr : *global_cr->GlobalMapGroup) {
                group_cr->Send_AddCritter(cr);
            }

            cr->GlobalMapGroup = global_cr->GlobalMapGroup;
        }

        vec_add_unique_value(*cr->GlobalMapGroup, cr);

        _engine->OnGlobalMapCritterIn.Fire(cr);
    }
}

void MapManager::RemoveCritterFromMap(Critter* cr, Map* map)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    cr->LockMapTransfers++;
    auto restore_transfers = ScopeCallback([cr]() noexcept { cr->LockMapTransfers--; });

    if (map != nullptr) {
        FO_RUNTIME_ASSERT(cr->GetMapId() == map->GetId());

        _engine->OnMapCritterOut.Fire(map, cr);

        for (auto* other : copy_hold_ref(cr->VisCr)) {
            other->OnCritterDisappeared.Fire(cr);
        }

        cr->ClearVisible();
        map->RemoveCritter(cr);
        cr->SetMapId({});

        if (!cr->GetControlledByPlayer()) {
            auto cr_ids = map->GetCritterIds();
            vec_remove_unique_value(cr_ids, cr->GetId());
            map->SetCritterIds(cr_ids);
        }
    }
    else {
        FO_RUNTIME_ASSERT(!cr->GetMapId());
        FO_RUNTIME_ASSERT(cr->GlobalMapGroup);

        _engine->OnGlobalMapCritterOut.Fire(cr);

        cr->SetGlobalMapTripId({});

        vec_remove_unique_value(*cr->GlobalMapGroup, cr);

        if (!cr->GlobalMapGroup->empty()) {
            for (auto* group_cr : *cr->GlobalMapGroup) {
                group_cr->Send_RemoveCritter(cr);
            }
        }

        cr->GlobalMapGroup.reset();
    }
}

void MapManager::ProcessVisibleCritters(Critter* cr)
{
    FO_STACK_TRACE_ENTRY();

    if (cr->IsDestroyed()) {
        return;
    }

    if (const auto map_id = cr->GetMapId()) {
        auto* map = _engine->EntityMngr.GetMap(map_id);
        FO_RUNTIME_ASSERT(map);

        for (auto* target : copy_hold_ref(map->GetCritters())) {
            optional<bool> trace_result;
            ProcessCritterLook(map, cr, target, trace_result);
            ProcessCritterLook(map, target, cr, trace_result);
        }
    }
    else {
        FO_RUNTIME_ASSERT(cr->GlobalMapGroup);
    }
}

void MapManager::ProcessCritterLook(Map* map, Critter* cr, Critter* target, optional<bool>& trace_result)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(map && cr && target);

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
            FO_RUNTIME_ASSERT(attached_cr->GetMapId() == map->GetId());

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

    const uint32 dist = GeometryHelper::DistGame(cr->GetHex(), target->GetHex());
    const uint32 show_cr_dist1 = cr->GetShowCritterDist1();
    const uint32 show_cr_dist2 = cr->GetShowCritterDist2();
    const uint32 show_cr_dist3 = cr->GetShowCritterDist3();

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
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!map->IsDestroyed());
    FO_RUNTIME_ASSERT(!cr->IsDestroyed());
    FO_RUNTIME_ASSERT(!target->IsDestroyed());

    bool is_see;

    if (IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_SCRIPT)) {
        is_see = _engine->OnMapCheckLook.Fire(map, cr, target);
    }
    else {
        uint32 dist = GeometryHelper::DistGame(cr->GetHex(), target->GetHex());
        uint32 look_dist = cr->GetLookDistance();
        const int32 cr_dir = cr->GetDir();

        // Dir modifier
        if (IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_DIR)) {
            const auto real_dir = GeometryHelper::GetFarDir(cr->GetHex(), target->GetHex());
            auto dir_index = cr_dir > real_dir ? cr_dir - real_dir : real_dir - cr_dir;

            if (dir_index > static_cast<int32>(GameSettings::MAP_DIR_COUNT / 2)) {
                dir_index = static_cast<int32>(GameSettings::MAP_DIR_COUNT) - dir_index;
            }

            look_dist -= look_dist * _engine->Settings.LookDir[dir_index] / 100;
        }

        if (dist > look_dist) {
            dist = std::numeric_limits<uint32>::max();
        }

        // Trace
        if (IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_TRACE) && dist != std::numeric_limits<uint32>::max()) {
            if (!trace_result.has_value()) {
                TraceData trace;
                trace.TraceMap = map;
                trace.StartHex = cr->GetHex();
                trace.TargetHex = target->GetHex();

                TraceBullet(trace);

                if (!trace.IsFullTrace) {
                    dist = std::numeric_limits<uint32>::max();
                }

                trace_result = trace.IsFullTrace;
            }
            else {
                if (!trace_result.value()) {
                    dist = std::numeric_limits<uint32>::max();
                }
            }
        }

        // Sneak
        if (target->GetInSneakMode() && dist != std::numeric_limits<uint32>::max()) {
            auto sneak_opp = target->GetSneakCoefficient();

            if (IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_SNEAK_DIR)) {
                const auto real_dir = GeometryHelper::GetFarDir(cr->GetHex(), target->GetHex());
                auto dir_index = (cr_dir > real_dir ? cr_dir - real_dir : real_dir - cr_dir);

                if (dir_index > static_cast<int32>(GameSettings::MAP_DIR_COUNT / 2)) {
                    dir_index = static_cast<int32>(GameSettings::MAP_DIR_COUNT) - dir_index;
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
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    if (cr->IsDestroyed()) {
        return;
    }

    const auto map_id = cr->GetMapId();

    if (!map_id) {
        return;
    }

    auto* map = _engine->EntityMngr.GetMap(map_id);
    FO_RUNTIME_ASSERT(map);

    const int32 look = static_cast<int32>(cr->GetLookDistance());

    for (auto* item : copy_hold_ref(map->GetItems())) {
        if (item->IsDestroyed()) {
            continue;
        }
        if (item->GetHidden()) {
            continue;
        }

        if (item->GetAlwaysView()) {
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
                int32 dist = static_cast<int32>(GeometryHelper::DistGame(cr->GetHex(), item->GetHex()));

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

void MapManager::ViewMap(Critter* view_cr, Map* map, uint32 look, mpos hex, int32 dir)
{
    FO_STACK_TRACE_ENTRY();

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

        const auto dist = GeometryHelper::DistGame(hex, cr->GetHex());
        auto look_self = look;

        // Dir modifier
        if (IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_DIR)) {
            const auto real_dir = GeometryHelper::GetFarDir(hex, cr->GetHex());
            auto i = dir > real_dir ? dir - real_dir : real_dir - dir;

            if (i > static_cast<int32>(dirs_count / 2u)) {
                i = static_cast<int32>(dirs_count) - i;
            }

            look_self -= look_self * _engine->Settings.LookDir[i] / 100;
        }

        if (dist > look_self) {
            continue;
        }

        // Trace
        if (IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_TRACE) && dist != std::numeric_limits<uint32>::max()) {
            TraceData trace;
            trace.TraceMap = map;
            trace.StartHex = hex;
            trace.TargetHex = cr->GetHex();

            TraceBullet(trace);

            if (!trace.IsFullTrace) {
                continue;
            }
        }

        // Hide modifier
        uint32 vis;

        if (cr->GetInSneakMode()) {
            auto sneak_opp = cr->GetSneakCoefficient();

            if (IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_SNEAK_DIR)) {
                const auto real_dir = GeometryHelper::GetFarDir(hex, cr->GetHex());
                auto i = dir > real_dir ? dir - real_dir : real_dir - dir;

                if (i > static_cast<int32>(dirs_count / 2u)) {
                    i = static_cast<int32>(dirs_count) - i;
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
        if (item->GetHidden()) {
            continue;
        }

        if (item->GetAlwaysView()) {
            view_cr->Send_AddItemOnMap(item);
        }
        else {
            bool allowed;

            if (item->GetIsTrap() && IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_ITEM_SCRIPT)) {
                allowed = _engine->OnMapCheckTrapLook.Fire(map, view_cr, item);
            }
            else {
                auto dist = GeometryHelper::DistGame(hex, item->GetHex());

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

FO_END_NAMESPACE();
