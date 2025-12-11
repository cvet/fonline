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
#include "ItemManager.h"
#include "LineTracer.h"
#include "ProtoManager.h"
#include "Server.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE();

MapManager::MapManager(FOServer* engine) :
    _engine {engine}
{
    FO_STACK_TRACE_ENTRY();
}

void MapManager::LoadFromResources()
{
    FO_STACK_TRACE_ENTRY();

    const auto map_files = _engine->Resources.FilterFiles("fomap-bin-server");
    std::vector<pair<const ProtoMap*, std::future<unique_ptr<StaticMap>>>> static_map_loadings;

    for (const auto& map_file_header : map_files) {
        const auto map_pid = _engine->Hashes.ToHashedString(map_file_header.GetNameNoExt());
        const auto* map_proto = _engine->ProtoMngr.GetProtoMap(map_pid);

        constexpr std::launch async_flags = std::launch::async | std::launch::deferred;
        static_map_loadings.emplace_back(map_proto, std::async(async_flags, [this, map_proto, &map_file_header]() {
            auto map_file = File::Load(map_file_header);
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

                    for (const auto i : iterate_range(cr_count)) {
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

                        auto cr = SafeAlloc::MakeRefCounted<Critter>(_engine.get(), ident_t {}, cr_proto, &cr_props);

                        static_map->CritterBillets.emplace_back(cr_id, cr);

                        // Checks
                        if (const auto hex = cr->GetHex(); !map_size.is_valid_pos(hex)) {
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

                    for (const auto i : iterate_range(item_count)) {
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

                        auto item = SafeAlloc::MakeRefCounted<Item>(_engine.get(), ident_t {}, item_proto, &item_props);
                        static_map->ItemBillets.emplace_back(item_id, item);

                        // Checks
                        if (item->GetOwnership() == ItemOwnership::MapHex) {
                            if (const auto hex = item->GetHex(); !map_size.is_valid_pos(hex)) {
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
                            static_map->StaticItems.emplace_back(item.get());
                            static_map->StaticItemsById.emplace(item_id, item.get());

                            const auto add_item_to_field = [item = item.get()](StaticMap::Field& static_field) {
                                if (!vec_exists(static_field.StaticItems, item)) {
                                    static_field.StaticItems.reserve(static_field.StaticItems.size() + 1);
                                    static_field.StaticItems.emplace_back(item);

                                    if (item->GetIsTrigger() || item->GetIsTrap()) {
                                        static_field.TriggerItems.reserve(static_field.TriggerItems.size() + 1);
                                        static_field.TriggerItems.emplace_back(item);
                                    }

                                    if (!item->GetNoBlock()) {
                                        static_field.MoveBlocked = true;
                                    }
                                    if (!item->GetShootThru()) {
                                        static_field.ShootBlocked = true;
                                        static_field.MoveBlocked = true;
                                    }
                                }
                            };

                            const auto hex = item->GetHex();
                            auto& static_field = static_map->HexField->GetCellForWriting(hex);
                            add_item_to_field(static_field);

                            if (item->IsNonEmptyMultihexLines()) {
                                GeometryHelper::ForEachMultihexLines(item->GetMultihexLines(), hex, map_size, [&](mpos multihex) {
                                    auto& multihex_field = static_map->HexField->GetCellForWriting(multihex);
                                    add_item_to_field(multihex_field);
                                });
                            }
                            if (item->IsNonEmptyMultihexMesh()) {
                                for (const auto multihex : item->GetMultihexMesh()) {
                                    if (multihex != hex && map_size.is_valid_pos(multihex)) {
                                        auto& multihex_field = static_map->HexField->GetCellForWriting(multihex);
                                        add_item_to_field(multihex_field);
                                    }
                                }
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

            // Scroll blocks
            const irect32 scroll_area = map_proto->GetScrollAxialArea();
            const int32 scroll_block_size = _engine->Settings.ScrollBlockSize;

            if (!scroll_area.is_zero()) {
                for (const auto hx : iterate_range(map_size.width)) {
                    for (const auto hy : iterate_range(map_size.height)) {
                        const mpos hex = {hx, hy};
                        const ipos32 axial_hex = _engine->Geometry.GetHexAxialCoord(hex);

                        // Is hex on scroll block line
                        if ((axial_hex.x >= scroll_area.x - scroll_block_size && axial_hex.x <= scroll_area.x + scroll_block_size) || //
                            (axial_hex.x >= scroll_area.x + scroll_area.width - scroll_block_size && axial_hex.x <= scroll_area.x + scroll_area.width + scroll_block_size) || //
                            (axial_hex.y >= scroll_area.y - scroll_block_size && axial_hex.y <= scroll_area.y + scroll_block_size) || //
                            (axial_hex.y >= scroll_area.y + scroll_area.height - scroll_block_size && axial_hex.y <= scroll_area.y + scroll_area.height + scroll_block_size)) {
                            auto& field = static_map->HexField->GetCellForWriting(hex);
                            field.MoveBlocked = true;
                        }
                    }
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

auto MapManager::GetStaticMap(const ProtoMap* proto) -> StaticMap*
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _staticMaps.find(proto);
    FO_RUNTIME_ASSERT(it != _staticMaps.end());
    return it->second.get();
}

void MapManager::GenerateMapContent(Map* map)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(map);
    FO_RUNTIME_ASSERT(!map->IsDestroyed());

    unordered_map<ident_t, ident_t> id_map;

    // Generate critters
    for (auto&& [base_cr_id, base_cr] : map->GetStaticMap()->CritterBillets) {
        const auto* cr = _engine->CrMngr.CreateCritterOnMap(base_cr->GetProtoId(), &base_cr->GetProperties(), map, base_cr->GetHex(), base_cr->GetDir());
        id_map.emplace(base_cr_id, cr->GetId());
        FO_RUNTIME_ASSERT(!map->IsDestroyed());
    }

    // Generate hex items
    for (auto&& [base_item_id, base_item] : map->GetStaticMap()->HexItemBillets) {
        auto* item = _engine->ItemMngr.CreateItem(base_item->GetProtoId(), 0, &base_item->GetProperties());
        id_map.emplace(base_item_id, item->GetId());
        FO_RUNTIME_ASSERT(!map->IsDestroyed());
        map->AddItem(item, base_item->GetHex(), nullptr);
        FO_RUNTIME_ASSERT(!map->IsDestroyed());
    }

    // Add children items
    for (const auto& base_item : map->GetStaticMap()->ChildItemBillets | std::views::values) {
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
        FO_RUNTIME_ASSERT(!map->IsDestroyed());

        // Add to parent
        if (base_item->GetOwnership() == ItemOwnership::CritterInventory) {
            auto* cr_cont = map->GetCritter(owner_id);
            FO_RUNTIME_ASSERT(cr_cont);

            _engine->CrMngr.AddItemToCritter(cr_cont, item, false);
            FO_RUNTIME_ASSERT(!map->IsDestroyed());
        }
        else if (base_item->GetOwnership() == ItemOwnership::ItemContainer) {
            auto* item_cont = map->GetItem(owner_id);
            FO_RUNTIME_ASSERT(item_cont);

            item_cont->AddItemToContainer(item, {});
            FO_RUNTIME_ASSERT(!map->IsDestroyed());
        }
        else {
            FO_UNREACHABLE_PLACE();
        }
    }

    // Process visibility
    for (auto* cr : copy_hold_ref(map->GetCritters())) {
        if (!cr->IsDestroyed()) {
            ProcessVisibleCritters(cr);
            FO_RUNTIME_ASSERT(!map->IsDestroyed());
        }
        if (!cr->IsDestroyed()) {
            ProcessVisibleItems(cr);
            FO_RUNTIME_ASSERT(!map->IsDestroyed());
        }
    }
}

void MapManager::DestroyMapContent(Map* map)
{
    FO_STACK_TRACE_ENTRY();

    for (InfinityLoopDetector detector; map->HasCritters() || map->HasItems(); detector.AddLoop()) {
        for (auto* cr : copy_hold_ref(map->GetPlayerCritters())) {
            TransferToGlobal(cr, {});
        }
        for (auto* cr : copy_hold_ref(map->GetNonPlayerCritters())) {
            _engine->CrMngr.DestroyCritter(cr);
        }
        for (auto* item : copy_hold_ref(map->GetItems())) {
            _engine->ItemMngr.DestroyItem(item);
        }
    }
}

auto MapManager::CreateLocation(hstring proto_id, span<const hstring> map_pids, const Properties* props) -> Location*
{
    FO_STACK_TRACE_ENTRY();

    const auto* proto = _engine->ProtoMngr.GetProtoLocation(proto_id);
    auto loc = SafeAlloc::MakeRefCounted<Location>(_engine.get(), ident_t {}, proto, props);

    _engine->EntityMngr.RegisterLocation(loc.get());

    for (const auto map_pid : map_pids) {
        const auto* map_proto = _engine->ProtoMngr.GetProtoMap(map_pid);
        auto* static_map = GetStaticMap(map_proto);
        auto map = SafeAlloc::MakeRefCounted<Map>(_engine.get(), ident_t {}, map_proto, loc.get(), static_map);
        _engine->EntityMngr.RegisterMap(map.get());
        loc->AddMap(map.get());
        GenerateMapContent(map.get());
    }

    for (auto& map : copy_hold_ref(loc->GetMaps())) {
        _engine->EntityMngr.CallInit(map.get(), true);
    }

    if (loc->IsDestroyed()) {
        _engine->EntityMngr.CallInit(loc.get(), true);
    }

    if (loc->IsDestroyed()) {
        throw GenericException("Location destroyed during init");
    }

    return loc.get();
}

auto MapManager::CreateMap(hstring proto_id, Location* loc) -> Map*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(loc);
    FO_RUNTIME_ASSERT(!loc->IsDestroyed());
    FO_RUNTIME_ASSERT(!loc->IsDestroying());

    const auto* map_proto = _engine->ProtoMngr.GetProtoMap(proto_id);
    auto* static_map = GetStaticMap(map_proto);
    auto map = SafeAlloc::MakeRefCounted<Map>(_engine.get(), ident_t {}, map_proto, loc, static_map);

    _engine->EntityMngr.RegisterMap(map.get());
    loc->AddMap(map.get());

    GenerateMapContent(map.get());

    if (!map->IsDestroyed()) {
        _engine->EntityMngr.CallInit(map.get(), true);
    }

    if (!map->IsDestroyed()) {
        loc->OnMapAdded.Fire(map.get());
    }

    if (map->IsDestroyed()) {
        throw GenericException("Map destroyed during init");
    }

    return map.get();
}

void MapManager::RegenerateMap(Map* map)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(map);
    FO_RUNTIME_ASSERT(!map->IsDestroyed());

    refcount_ptr map_holder = map;

    DestroyMapContent(map);

    if (!map->IsDestroyed()) {
        GenerateMapContent(map);
    }
}

auto MapManager::GetMapByPid(hstring map_pid, int32 skip_count) noexcept -> Map*
{
    FO_STACK_TRACE_ENTRY();

    for (auto& map : _engine->EntityMngr.GetMaps() | std::views::values) {
        if (map->GetProtoId() == map_pid) {
            if (skip_count <= 0) {
                return map.get();
            }

            skip_count--;
        }
    }

    return nullptr;
}

auto MapManager::GetLocationByPid(hstring loc_pid, int32 skip_count) noexcept -> Location*
{
    FO_STACK_TRACE_ENTRY();

    for (auto& loc : _engine->EntityMngr.GetLocations() | std::views::values) {
        if (loc->GetProtoId() == loc_pid) {
            if (skip_count <= 0) {
                return loc.get();
            }

            skip_count--;
        }
    }

    return nullptr;
}

void MapManager::DestroyLocation(Location* loc)
{
    FO_STACK_TRACE_ENTRY();

    if (loc->IsDestroying() || loc->IsDestroyed()) {
        return;
    }

    // Destroy location in couple with maps
    loc->MarkAsDestroying();

    for (auto& map : loc->GetMaps()) {
        map->MarkAsDestroying();
    }

    _engine->OnLocationFinish.Fire(loc);

    for (auto& map : loc->GetMaps()) {
        _engine->OnMapFinish.Fire(map.get());
    }

    for (auto& map : copy_hold_ref(loc->GetMaps())) {
        loc->RemoveMap(map.get());
        DestroyMapInternal(map.get());
    }

    for (InfinityLoopDetector detector; loc->HasInnerEntities(); detector.AddLoop()) {
        try {
            if (loc->HasInnerEntities()) {
                _engine->EntityMngr.DestroyInnerEntities(loc);
            }
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
        }
    }

    loc->MarkAsDestroyed();
    _engine->EntityMngr.UnregisterLocation(loc);
}

void MapManager::DestroyMap(Map* map)
{
    FO_STACK_TRACE_ENTRY();

    if (map->IsDestroying() || map->IsDestroyed()) {
        return;
    }

    map->MarkAsDestroying();
    _engine->OnMapFinish.Fire(map);

    refcount_ptr loc = map->GetLocation();
    loc->OnMapRemoved.Fire(map);
    loc->RemoveMap(map);

    DestroyMapInternal(map);
}

void MapManager::DestroyMapInternal(Map* map)
{
    FO_STACK_TRACE_ENTRY();

    for (InfinityLoopDetector detector; map->HasCritters() || map->HasItems() || map->HasInnerEntities(); detector.AddLoop()) {
        try {
            if (map->HasCritters() || map->HasItems()) {
                DestroyMapContent(map);
            }

            if (map->HasInnerEntities()) {
                _engine->EntityMngr.DestroyInnerEntities(map);
            }
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
        }
    }

    map->MarkAsDestroyed();
    _engine->EntityMngr.UnregisterMap(map);
}

auto MapManager::TracePath(TracePathInput& input) const -> TracePathOutput
{
    FO_STACK_TRACE_ENTRY();

    TracePathOutput output;
    output.IsFullTrace = false;
    output.IsCritterFound = false;
    output.HasLastMovable = false;

    auto& map = input.TraceMap;
    const auto map_size = map->GetSize();
    const auto start_hex = input.StartHex;
    const auto target_hex = input.TargetHex;
    const auto dist = input.MaxDist != 0 ? input.MaxDist : GeometryHelper::GetDistance(start_hex, target_hex);

    auto tracer = LineTracer(start_hex, target_hex, input.Angle, map_size);
    auto next_hex = start_hex;
    auto prev_hex = next_hex;
    bool last_passed_ok = false;

    for (int32 i = 0;; i++) {
        if (i >= dist) {
            output.IsFullTrace = true;
            break;
        }

        if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
            tracer.GetNextHex(next_hex);
        }
        else {
            tracer.GetNextSquare(next_hex);
        }

        if (input.CheckLastMovable && !last_passed_ok) {
            if (map->IsHexMovable(next_hex)) {
                output.LastMovable = next_hex;
                output.HasLastMovable = true;
            }
            else {
                last_passed_ok = true;
            }
        }

        if (!map->IsHexShootable(next_hex)) {
            break;
        }

        if (input.CollectCritters && map->IsCritterOnHex(next_hex, CritterFindType::Any)) {
            const auto critters = map->GetCrittersOnHex(next_hex, input.FindType);

            for (auto* cr : critters) {
                if (std::ranges::find(output.Critters, cr) == output.Critters.end()) {
                    output.Critters.emplace_back(cr);
                }
            }
        }

        if (input.FindCr != nullptr && map->IsCritterOnHex(next_hex, input.FindCr.get())) {
            output.IsCritterFound = true;
            break;
        }

        prev_hex = next_hex;
    }

    output.PreBlock = prev_hex;
    output.Block = next_hex;

    return output;
}

auto MapManager::FindPath(FindPathInput& input) const -> FindPathOutput
{
    FO_STACK_TRACE_ENTRY();

    FindPathOutput output;

    // Checks
    if (input.TraceDist > 0 && input.TraceCr == nullptr) {
        output.Result = FindPathOutput::ResultType::TraceTargetNullptr;
        return output;
    }

    auto& map = input.TargetMap;

    if (map == nullptr) {
        output.Result = FindPathOutput::ResultType::MapNotFound;
        return output;
    }

    const auto map_size = map->GetSize();

    if (!map_size.is_valid_pos(input.FromHex) || !map_size.is_valid_pos(input.ToHex)) {
        output.Result = FindPathOutput::ResultType::InvalidHexes;
        return output;
    }
    if (GeometryHelper::CheckDist(input.FromHex, input.ToHex, input.Cut)) {
        output.Result = FindPathOutput::ResultType::AlreadyHere;
        return output;
    }
    if (input.Cut == 0 && !map->IsHexesMovable(input.ToHex, input.Multihex, input.TraceCr.get())) {
        output.Result = FindPathOutput::ResultType::HexBusy;
        return output;
    }

    // Ring check
    if (input.Cut <= 1 && input.Multihex == 0) {
        const int32 hexes_around = GeometryHelper::HexesInRadius(1);
        int32 i = 1;

        for (; i < hexes_around; i++) {
            if (auto ring_hex = input.ToHex; GeometryHelper::MoveHexAroundAway(ring_hex, i, map_size)) {
                if (map->IsGagItemOnHex(ring_hex)) {
                    break;
                }
                if (map->IsHexMovable(ring_hex)) {
                    break;
                }
            }
        }

        if (i >= hexes_around) {
            output.Result = FindPathOutput::ResultType::HexBusyRing;
            return output;
        }
    }

    // Prepare grid
    const auto max_path_find_len = _engine->Settings.MaxPathFindLength;
    auto path_find_grid = vector<int16>(numeric_cast<size_t>(max_path_find_len * 2 + 2) * (max_path_find_len * 2 + 2));
    const auto grid_offset = input.FromHex;
    const auto grid_at = [&](mpos hex) -> int16& { return path_find_grid[((max_path_find_len + 1) + hex.y - grid_offset.y) * (max_path_find_len * 2 + 2) + ((max_path_find_len + 1) + hex.x - grid_offset.x)]; };

    vector<mpos> next_hexes;
    vector<mpos> cr_hexes;
    vector<mpos> gag_hexes;
    next_hexes.reserve(1024);
    cr_hexes.reserve(64);
    gag_hexes.reserve(64);

    // Begin search
    auto to_hex = input.ToHex;
    grid_at(input.FromHex) = 1;
    next_hexes.emplace_back(input.FromHex);

    while (true) {
        bool find_ok = false;
        const auto next_hexes_round = next_hexes.size();
        FO_RUNTIME_ASSERT(next_hexes_round != 0);

        for (size_t i = 0; i < next_hexes_round; i++) {
            const auto cur_hex = next_hexes[i];

            if (GeometryHelper::CheckDist(cur_hex, to_hex, input.Cut)) {
                to_hex = cur_hex;
                find_ok = true;
                break;
            }

            const auto next_hex_index = numeric_cast<int16>(grid_at(cur_hex) + 1);

            if (next_hex_index > _engine->Settings.MaxPathFindLength) {
                output.Result = FindPathOutput::ResultType::TooFar;
                return output;
            }

            for (int32 j = 0; j < GameSettings::MAP_DIR_COUNT; j++) {
                auto raw_next_hex = ipos32 {cur_hex.x, cur_hex.y};
                GeometryHelper::MoveHexByDirUnsafe(raw_next_hex, static_cast<uint8>(j));

                if (!map_size.is_valid_pos(raw_next_hex)) {
                    continue;
                }

                const auto next_hex = map_size.from_raw_pos(raw_next_hex);
                auto& grid_cell = grid_at(next_hex);

                if (grid_cell != 0) {
                    continue;
                }

                if (map->IsHexesMovable(next_hex, input.Multihex, input.FromCritter.get())) {
                    next_hexes.emplace_back(next_hex);
                    grid_cell = next_hex_index;
                }
                else if (input.CheckGagItems && map->IsGagItemOnHex(next_hex)) {
                    gag_hexes.emplace_back(next_hex);
                    grid_cell = numeric_cast<int16>(next_hex_index | 0x4000);
                }
                else if (input.CheckCritter && map->IsCritterOnHex(next_hex, CritterFindType::NonDead)) {
                    cr_hexes.emplace_back(next_hex);
                    grid_cell = numeric_cast<int16>(next_hex_index | 0x4000);
                }
                else {
                    grid_cell = -1;
                }
            }
        }

        if (find_ok) {
            break;
        }

        next_hexes.erase(next_hexes.begin(), next_hexes.begin() + static_cast<ptrdiff_t>(next_hexes_round));

        // Add gag hex after some distance
        if (!gag_hexes.empty()) {
            const auto last_index = grid_at(next_hexes.back());
            const auto& gag_hex = gag_hexes.front();
            const auto gag_index = numeric_cast<int16>(grid_at(gag_hex) ^ 0x4000);

            if (gag_index + 10 < last_index) { // Todo: if path finding not be reworked than migrate magic number to scripts
                grid_at(gag_hex) = gag_index;
                next_hexes.emplace_back(gag_hex);
                gag_hexes.erase(gag_hexes.begin());
            }
        }

        // If no way then route through gag/critter
        if (next_hexes.empty()) {
            if (!gag_hexes.empty()) {
                auto& gag_hex = gag_hexes.front();
                grid_at(gag_hex) ^= 0x4000;
                next_hexes.emplace_back(gag_hex);
                gag_hexes.erase(gag_hexes.begin());
            }
            else if (!cr_hexes.empty()) {
                auto& cr_hex = cr_hexes.front();
                grid_at(cr_hex) ^= 0x4000;
                next_hexes.emplace_back(cr_hex);
                cr_hexes.erase(cr_hexes.begin());
            }
        }

        if (next_hexes.empty()) {
            output.Result = FindPathOutput::ResultType::NoWay;
            return output;
        }
    }

    vector<uint8> raw_steps;
    auto hex_index = grid_at(to_hex);
    auto cur_hex = to_hex;
    raw_steps.resize(hex_index - 1);
    float32 base_angle = GeometryHelper::GetDirAngle(to_hex, input.FromHex);

    while (hex_index > 1) {
        hex_index--;

        const auto find_path_grid = [&](mpos& hex) -> bool {
            int32 best_step_dir = -1;
            float32 best_step_angle_diff = 0.0f;

            const auto check_hex = [&](int32 dir, ipos32 step_raw_hex) {
                if (!map_size.is_valid_pos(step_raw_hex)) {
                    return;
                }

                const auto step_hex = map_size.from_raw_pos(step_raw_hex);

                if (grid_at(step_hex) != hex_index) {
                    return;
                }

                const float32 angle = GeometryHelper::GetDirAngle(step_hex, input.FromHex);
                const float32 angle_diff = GeometryHelper::GetDirAngleDiff(base_angle, angle);

                if (best_step_dir == -1 || hex_index == 0) {
                    best_step_dir = dir;
                    best_step_angle_diff = GeometryHelper::GetDirAngleDiff(base_angle, angle);
                }
                else if (angle_diff < best_step_angle_diff) {
                    best_step_dir = dir;
                    best_step_angle_diff = angle_diff;
                }
            };

            if ((hex.x % 2) != 0) {
                check_hex(3, ipos32 {hex.x - 1, hex.y - 1});
                check_hex(2, ipos32 {hex.x, hex.y - 1});
                check_hex(5, ipos32 {hex.x, hex.y + 1});
                check_hex(0, ipos32 {hex.x + 1, hex.y});
                check_hex(4, ipos32 {hex.x - 1, hex.y});
                check_hex(1, ipos32 {hex.x + 1, hex.y - 1});

                if (best_step_dir == 3) {
                    raw_steps[hex_index - 1] = 3;
                    hex.x--;
                    hex.y--;
                    return true;
                }
                if (best_step_dir == 2) {
                    raw_steps[hex_index - 1] = 2;
                    hex.y--;
                    return true;
                }
                if (best_step_dir == 5) {
                    raw_steps[hex_index - 1] = 5;
                    hex.y++;
                    return true;
                }
                if (best_step_dir == 0) {
                    raw_steps[hex_index - 1] = 0;
                    hex.x++;
                    return true;
                }
                if (best_step_dir == 4) {
                    raw_steps[hex_index - 1] = 4;
                    hex.x--;
                    return true;
                }
                if (best_step_dir == 1) {
                    raw_steps[hex_index - 1] = 1;
                    hex.x++;
                    hex.y--;
                    return true;
                }
            }
            else {
                check_hex(3, ipos32 {hex.x - 1, hex.y});
                check_hex(2, ipos32 {hex.x, hex.y - 1});
                check_hex(5, ipos32 {hex.x, hex.y + 1});
                check_hex(0, ipos32 {hex.x + 1, hex.y + 1});
                check_hex(4, ipos32 {hex.x - 1, hex.y + 1});
                check_hex(1, ipos32 {hex.x + 1, hex.y});

                if (best_step_dir == 3) {
                    raw_steps[hex_index - 1] = 3;
                    hex.x--;
                    return true;
                }
                if (best_step_dir == 2) {
                    raw_steps[hex_index - 1] = 2;
                    hex.y--;
                    return true;
                }
                if (best_step_dir == 5) {
                    raw_steps[hex_index - 1] = 5;
                    hex.y++;
                    return true;
                }
                if (best_step_dir == 0) {
                    raw_steps[hex_index - 1] = 0;
                    hex.x++;
                    hex.y++;
                    return true;
                }
                if (best_step_dir == 4) {
                    raw_steps[hex_index - 1] = 4;
                    hex.x--;
                    hex.y++;
                    return true;
                }
                if (best_step_dir == 1) {
                    raw_steps[hex_index - 1] = 1;
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

            if (input.CheckGagItems && map->IsGagItemOnHex(check_hex)) {
                const Item* item = map->GetGagItemOnHex(check_hex);

                if (item == nullptr) {
                    continue;
                }

                output.GagItemId = item->GetId();
                to_hex = prev_check_hex;
                raw_steps.resize(i);
                break;
            }

            if (input.CheckCritter && map->IsCritterOnHex(check_hex, CritterFindType::NonDead)) {
                const Critter* cr = map->GetCritterOnHex(check_hex, CritterFindType::NonDead);

                if (cr == nullptr || cr == input.FromCritter) {
                    continue;
                }

                output.GagCritterId = cr->GetId();
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

            if (map->IsGagItemOnHex(check_hex)) {
                trace_seq[i + 2 - 2] += 1;
                trace_seq[i + 2 - 1] += 2;
                trace_seq[i + 2 - 0] += 3;
                trace_seq[i + 2 + 1] += 2;
                trace_seq[i + 2 + 2] += 1;
            }
        }

        TracePathInput trace;
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
                const auto treace_output = TracePath(trace);

                if (treace_output.IsCritterFound) {
                    raw_steps.resize(i + 1);
                    const auto move_ok3 = GeometryHelper::MoveHexByDir(check_hex2, raw_steps[i + 1], map_size);
                    FO_RUNTIME_ASSERT(move_ok3);
                    to_hex = check_hex2;
                    trace_ok = true;
                    break;
                }
            }

            if (trace_ok) {
                break;
            }
        }

        if (!trace_ok && !output.GagItemId && !output.GagCritterId) {
            output.Result = FindPathOutput::ResultType::TraceFailed;
            return output;
        }

        if (trace_ok) {
            output.GagItemId = {};
            output.GagCritterId = {};
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

            for (auto i = numeric_cast<int32>(raw_steps.size()) - 1; i >= 0; i--) {
                LineTracer tracer(trace_hex, trace_hex2, 0.0f, map_size);
                mpos next_hex = trace_hex;
                vector<uint8> direct_steps;
                bool failed = false;

                while (true) {
                    uint8 dir = tracer.GetNextHex(next_hex);
                    direct_steps.emplace_back(dir);

                    if (next_hex == trace_hex2) {
                        break;
                    }

                    if (grid_at(next_hex) <= 0) {
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

                output.ControlSteps.emplace_back(numeric_cast<uint16>(output.Steps.size()));

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

            output.ControlSteps.emplace_back(numeric_cast<uint16>(output.Steps.size()));
        }
    }

    FO_RUNTIME_ASSERT(!output.Steps.empty());
    FO_RUNTIME_ASSERT(!output.ControlSteps.empty());

    output.Result = FindPathOutput::ResultType::Ok;
    output.NewToHex = to_hex;

    return output;
}

void MapManager::TransferToMap(Critter* cr, Map* map, mpos hex, uint8 dir, optional<int32> safe_radius)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(map);
    FO_RUNTIME_ASSERT(map->GetSize().is_valid_pos(hex));

    Transfer(cr, map, hex, dir, safe_radius, {});
}

void MapManager::TransferToGlobal(Critter* cr, ident_t global_cr_id)
{
    FO_STACK_TRACE_ENTRY();

    Transfer(cr, nullptr, {}, 0, std::nullopt, global_cr_id);
}

void MapManager::Transfer(Critter* cr, Map* map, mpos hex, uint8 dir, optional<int32> safe_radius, ident_t global_cr_id)
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

    vector<raw_ptr<Critter>> attached_critters;

    if (!cr->AttachedCritters.empty()) {
        attached_critters = cr->AttachedCritters;

        for (auto& attached_cr : attached_critters) {
            attached_cr->DetachFromCritter();
        }
    }

    if (map == prev_map) {
        // Between one map
        if (map != nullptr) {
            FO_RUNTIME_ASSERT(map->GetSize().is_valid_pos(hex));

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
            cr->ClearVisibleEnitites();
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
            for (const auto& group_cr : cr->GetGlobalMapGroup()) {
                if (group_cr != cr) {
                    cr->Send_AddCritter(group_cr.get());
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

    // Transfer attached critters
    if (!attached_critters.empty()) {
        for (auto& attached_cr : attached_critters) {
            if (!cr->IsDestroyed() && !attached_cr->IsDestroyed() && !attached_cr->GetIsAttached() && attached_cr->GetMapId() == prev_map_id) {
                Transfer(attached_cr.get(), map, cr->GetHex(), dir, std::nullopt, cr->GetId());
            }
        }

        if (!cr->IsDestroyed() && !cr->GetIsAttached()) {
            for (auto& attached_cr : attached_critters) {
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

    _engine->OnCritterTransfer.Fire(cr, prev_map);
}

void MapManager::AddCritterToMap(Critter* cr, Map* map, mpos hex, uint8 dir, ident_t global_cr_id)
{
    FO_STACK_TRACE_ENTRY();

    cr->LockMapTransfers++;
    auto restore_transfers = ScopeCallback([cr]() noexcept { cr->LockMapTransfers--; });

    if (map != nullptr) {
        FO_RUNTIME_ASSERT(map->GetSize().is_valid_pos(hex));

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
        auto& cr_group = cr->GetRawGlobalMapGroup();
        FO_RUNTIME_ASSERT(!cr_group);

        auto* global_cr = global_cr_id && global_cr_id != cr->GetId() ? _engine->EntityMngr.GetCritter(global_cr_id) : nullptr;

        cr->SetMapId({});

        if (global_cr == nullptr || global_cr->GetMapId()) {
            const auto trip_id = _engine->GetLastGlobalMapTripId() + 1;
            _engine->SetLastGlobalMapTripId(trip_id);
            cr->SetGlobalMapTripId(trip_id);

            cr_group = SafeAlloc::MakeShared<vector<raw_ptr<Critter>>>();
        }
        else {
            auto& global_cr_group = global_cr->GetRawGlobalMapGroup();
            FO_RUNTIME_ASSERT(global_cr_group);

            cr->SetGlobalMapTripId(global_cr->GetGlobalMapTripId());

            for (auto& group_cr : *global_cr_group) {
                group_cr->Send_AddCritter(cr);
            }

            cr_group = global_cr_group;
        }

        vec_add_unique_value(*cr_group, cr);

        _engine->OnGlobalMapCritterIn.Fire(cr);
    }
}

void MapManager::RemoveCritterFromMap(Critter* cr, Map* map)
{
    FO_STACK_TRACE_ENTRY();

    cr->LockMapTransfers++;
    auto restore_transfers = ScopeCallback([cr]() noexcept { cr->LockMapTransfers--; });

    if (map != nullptr) {
        FO_RUNTIME_ASSERT(cr->GetMapId() == map->GetId());

        _engine->OnMapCritterOut.Fire(map, cr);

        for (auto* other : copy_hold_ref(cr->GetCritters(CritterSeeType::WhoSeeMe, CritterFindType::Any))) {
            other->OnCritterDisappeared.Fire(cr);
        }

        cr->ClearVisibleEnitites();
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
        auto& cr_group = cr->GetRawGlobalMapGroup();

        _engine->OnGlobalMapCritterOut.Fire(cr);

        cr->SetGlobalMapTripId({});

        vec_remove_unique_value(*cr_group, cr);

        for (auto& group_cr : *cr_group) {
            group_cr->Send_RemoveCritter(cr);
        }

        cr_group.reset();
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
        FO_RUNTIME_ASSERT(cr->GetRawGlobalMapGroup());
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
        for (auto& attached_cr : target->AttachedCritters) {
            FO_RUNTIME_ASSERT(attached_cr->GetMapId() == map->GetId());

            optional<bool> dummy_trace_result;
            is_see = IsCritterSeeCritter(map, cr, attached_cr.get(), dummy_trace_result);

            if (is_see) {
                break;
            }
        }
    }

    if (is_see) {
        if (target->AddVisibleCritter(cr)) {
            cr->Send_AddCritter(target);
            cr->OnCritterAppeared.Fire(target);
        }
    }
    else {
        if (target->RemoveVisibleCritter(cr)) {
            cr->Send_RemoveCritter(target);
            cr->OnCritterDisappeared.Fire(target);
        }
    }

    const int32 dist = GeometryHelper::GetDistance(cr->GetHex(), target->GetHex());
    const int32 show_cr_dist1 = cr->GetShowCritterDist1();
    const int32 show_cr_dist2 = cr->GetShowCritterDist2();
    const int32 show_cr_dist3 = cr->GetShowCritterDist3();

    if (show_cr_dist1 != 0) {
        if (show_cr_dist1 >= dist && is_see) {
            if (cr->AddCrIntoVisGroup1(target->GetId())) {
                cr->OnCritterAppearedDist1.Fire(target);
            }
        }
        else {
            if (cr->RemoveCrFromVisGroup1(target->GetId())) {
                cr->OnCritterDisappearedDist1.Fire(target);
            }
        }
    }

    if (show_cr_dist2 != 0) {
        if (show_cr_dist2 >= dist && is_see) {
            if (cr->AddCrIntoVisGroup2(target->GetId())) {
                cr->OnCritterAppearedDist2.Fire(target);
            }
        }
        else {
            if (cr->RemoveCrFromVisGroup2(target->GetId())) {
                cr->OnCritterDisappearedDist2.Fire(target);
            }
        }
    }

    if (show_cr_dist3 != 0) {
        if (show_cr_dist3 >= dist && is_see) {
            if (cr->AddCrIntoVisGroup3(target->GetId())) {
                cr->OnCritterAppearedDist3.Fire(target);
            }
        }
        else {
            if (cr->RemoveCrFromVisGroup3(target->GetId())) {
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
        int32 dist = GeometryHelper::GetDistance(cr->GetHex(), target->GetHex());
        int32 look_dist = cr->GetLookDistance();
        const auto cr_dir = cr->GetDir();

        // Dir modifier
        if (IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_DIR)) {
            const auto real_dir = GeometryHelper::GetDir(cr->GetHex(), target->GetHex());
            auto dir_index = cr_dir > real_dir ? cr_dir - real_dir : real_dir - cr_dir;

            if (dir_index > numeric_cast<int32>(GameSettings::MAP_DIR_COUNT / 2)) {
                dir_index = numeric_cast<int32>(GameSettings::MAP_DIR_COUNT) - dir_index;
            }

            look_dist -= look_dist * _engine->Settings.LookDir[dir_index] / 100;
        }

        if (dist > look_dist) {
            dist = -1;
        }

        // Trace
        if (IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_TRACE) && dist != -1) {
            if (!trace_result.has_value()) {
                TracePathInput trace;
                trace.TraceMap = map;
                trace.StartHex = cr->GetHex();
                trace.TargetHex = target->GetHex();

                const auto trace_output = TracePath(trace);

                if (!trace_output.IsFullTrace) {
                    dist = -1;
                }

                trace_result = trace_output.IsFullTrace;
            }
            else {
                if (!trace_result.value()) {
                    dist = -1;
                }
            }
        }

        // Sneak
        if (target->GetInSneakMode() && dist != -1) {
            auto sneak_opp = target->GetSneakCoefficient();

            if (IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_SNEAK_DIR)) {
                const auto real_dir = GeometryHelper::GetDir(cr->GetHex(), target->GetHex());
                auto dir_index = (cr_dir > real_dir ? cr_dir - real_dir : real_dir - cr_dir);

                if (dir_index > numeric_cast<int32>(GameSettings::MAP_DIR_COUNT / 2)) {
                    dir_index = numeric_cast<int32>(GameSettings::MAP_DIR_COUNT) - dir_index;
                }

                sneak_opp -= sneak_opp * _engine->Settings.LookSneakDir[dir_index] / 100;
            }

            sneak_opp /= _engine->Settings.SneakDivider;
            look_dist = look_dist > sneak_opp ? look_dist - sneak_opp : 0;
        }

        look_dist = std::max(look_dist, _engine->Settings.LookMinimum);

        is_see = dist != -1 && look_dist >= dist;
    }

    return is_see;
}

void MapManager::ProcessVisibleItems(Critter* cr)
{
    FO_STACK_TRACE_ENTRY();

    if (cr->IsDestroyed()) {
        return;
    }

    const auto map_id = cr->GetMapId();

    if (!map_id) {
        return;
    }

    auto* map = _engine->EntityMngr.GetMap(map_id);
    FO_RUNTIME_ASSERT(map);

    const auto look = cr->GetLookDistance();

    for (auto* item : copy_hold_ref(map->GetItems())) {
        if (item->IsDestroyed()) {
            continue;
        }
        if (item->GetHidden()) {
            continue;
        }

        if (item->GetAlwaysView()) {
            if (cr->AddVisibleItem(item->GetId())) {
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
                auto dist = GeometryHelper::GetDistance(cr->GetHex(), item->GetHex());

                if (item->GetIsTrap()) {
                    dist += item->GetTrapValue();
                }

                allowed = look >= dist;
            }

            if (allowed) {
                if (cr->AddVisibleItem(item->GetId())) {
                    cr->Send_AddItemOnMap(item);
                    cr->OnItemOnMapAppeared.Fire(item, nullptr);
                }
            }
            else {
                if (cr->RemoveVisibleItem(item->GetId())) {
                    cr->Send_RemoveItemFromMap(item);
                    cr->OnItemOnMapDisappeared.Fire(item, nullptr);
                }
            }
        }
    }
}

void MapManager::ViewMap(Critter* view_cr, Map* map, int32 look, mpos hex, uint8 dir)
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

        const auto dist = GeometryHelper::GetDistance(hex, cr->GetHex());
        auto look_self = look;

        // Dir modifier
        if (IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_DIR)) {
            const auto real_dir = GeometryHelper::GetDir(hex, cr->GetHex());
            auto i = dir > real_dir ? dir - real_dir : real_dir - dir;

            if (i > dirs_count / 2) {
                i = dirs_count - i;
            }

            look_self -= look_self * _engine->Settings.LookDir[i] / 100;
        }

        if (dist > look_self) {
            continue;
        }

        // Trace
        if (IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_TRACE) && dist != -1) {
            TracePathInput trace;
            trace.TraceMap = map;
            trace.StartHex = hex;
            trace.TargetHex = cr->GetHex();

            const auto trace_output = TracePath(trace);

            if (!trace_output.IsFullTrace) {
                continue;
            }
        }

        // Hide modifier
        int32 vis;

        if (cr->GetInSneakMode()) {
            auto sneak_opp = cr->GetSneakCoefficient();

            if (IsBitSet(_engine->Settings.LookChecks, LOOK_CHECK_SNEAK_DIR)) {
                const auto real_dir = GeometryHelper::GetDir(hex, cr->GetHex());
                auto i = dir > real_dir ? dir - real_dir : real_dir - dir;

                if (i > numeric_cast<int32>(dirs_count / 2u)) {
                    i = numeric_cast<int32>(dirs_count) - i;
                }

                sneak_opp -= sneak_opp * _engine->Settings.LookSneakDir[i] / 100;
            }

            sneak_opp /= _engine->Settings.SneakDivider;
            vis = look_self > sneak_opp ? look_self - sneak_opp : 0;
        }
        else {
            vis = look_self;
        }

        vis = std::max(vis, _engine->Settings.LookMinimum);

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
                auto dist = GeometryHelper::GetDistance(hex, item->GetHex());

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
