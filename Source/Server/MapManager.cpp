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

#include "MapManager.h"
#include "CritterManager.h"
#include "EntityManager.h"
#include "ItemManager.h"
#include "LineTracer.h"
#include "Player.h"
#include "ProtoManager.h"
#include "Server.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

extern CritterVisibilityMode CheckCritterVisibilityHook(ptr<const ServerEngine>, ptr<const Map>, ptr<const Critter>, ptr<const Critter>);

MapManager::MapManager(ptr<ServerEngine> engine) :
    _engine {engine}
{
    FO_STACK_TRACE_ENTRY();
}

void MapManager::LoadFromResources()
{
    FO_STACK_TRACE_ENTRY();

    const auto map_files = _engine->Resources.FilterFiles("fomap-bin-server");
    std::vector<pair<ptr<const ProtoMap>, std::future<unique_ptr<StaticMap>>>> static_map_loadings;

    for (const auto& map_file_header : map_files) {
        const auto map_pid = _engine->Hashes.ToHashedString(map_file_header.GetNameNoExt());
        auto nullable_map_proto = _engine->GetProtoMap(map_pid);

        if (!nullable_map_proto) {
            throw MapManagerException("Map proto not found for static map", map_pid);
        }

        const auto map_proto = nullable_map_proto.as_ptr();

        static_map_loadings.emplace_back(map_proto, run_async(strex("LoadStaticMap-{}", nullable_map_proto->GetName()), [this, map_proto = map_proto, &map_file_header]() FO_DEFERRED {
            ScopedSyncContext sync_ctx;

            auto map_file = File::Load(map_file_header);
            auto reader = DataReader(map_file.GetDataSpan());

            const auto map_size = map_proto->GetSize();
            auto static_map = SafeAlloc::MakeUnique<StaticMap>(map_size, _engine->Settings->ProtoMapStaticGrid);

            // Read hashes
            {
                const auto hashes_count = reader.Read<uint32_t>();

                string str;

                for (uint32_t i = 0; i < hashes_count; i++) {
                    const auto str_len = reader.Read<uint32_t>();
                    str.resize(str_len);
                    reader.ReadStringBytes(str);
                    const auto hstr = _engine->Hashes.ToHashedString(str);
                    ignore_unused(hstr);
                }
            }

            // Read entities
            {
                vector<uint8_t> props_data;

                // Read critters
                {
                    const auto cr_count = reader.Read<uint32_t>();

                    static_map->CritterBillets.reserve(cr_count);

                    for (const auto i : iterate_range(cr_count)) {
                        ignore_unused(i);

                        const auto cr_id = ident_t {reader.Read<ident_t::underlying_type>()};

                        const auto cr_pid_hash = reader.Read<hstring::hash_t>();
                        const auto cr_pid = _engine->Hashes.ResolveHash(cr_pid_hash);
                        auto cr_proto = _engine->GetProtoCritter(cr_pid);

                        if (!cr_proto) {
                            throw MapManagerException("Critter proto not found on map", map_proto->GetName(), cr_pid);
                        }

                        auto cr_props = Properties(cr_proto->GetProperties()->GetRegistrator());
                        const auto props_data_size = reader.Read<uint32_t>();
                        props_data.resize(props_data_size);
                        span<uint8_t> props_data_span = props_data;
                        reader.ReadBytes(props_data_span);
                        cr_props.RestoreAllData(props_data);

                        nptr<const Properties> cr_props_ptr = &cr_props;
                        auto cr = SafeAlloc::MakeRefCounted<Critter>(_engine, ident_t {}, cr_proto.as_ptr(), cr_props_ptr);
                        cr->SetEntityLock(nullptr);

                        static_map->CritterBillets.emplace_back(cr_id, cr);

                        // Checks
                        if (const auto hex = cr->GetHex(); !map_size.is_valid_pos(hex)) {
                            throw MapManagerException("Invalid critter position on map", map_proto->GetName(), cr->GetName(), hex);
                        }
                    }
                }

                // Read items
                {
                    const auto item_count = reader.Read<uint32_t>();

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
                        auto item_proto = _engine->GetProtoItem(item_pid);

                        if (!item_proto) {
                            throw MapManagerException("Item proto not found on map", map_proto->GetName(), item_pid);
                        }

                        auto item_props = Properties(item_proto->GetProperties()->GetRegistrator());
                        const auto props_data_size = reader.Read<uint32_t>();
                        props_data.resize(props_data_size);
                        span<uint8_t> props_data_span = props_data;
                        reader.ReadBytes(props_data_span);
                        item_props.RestoreAllData(props_data);

                        nptr<const Properties> item_props_ptr = &item_props;
                        auto item = SafeAlloc::MakeRefCounted<StaticItem>(_engine, ident_t {}, item_proto.as_ptr(), item_props_ptr);
                        item->SetEntityLock(nullptr);
                        static_map->ItemBillets.emplace_back(item_id, item);

                        // Checks
                        if (item->GetOwnership() == ItemOwnership::MapHex) {
                            if (const auto hex = item->GetHex(); !map_size.is_valid_pos(hex)) {
                                throw MapManagerException("Invalid item position on map", map_proto->GetName(), item->GetName(), hex);
                            }
                        }

                        // Bind scripts
                        if (const auto static_script = item->GetStaticScript()) {
                            item->StaticScriptFunc = _engine->FindFunc<bool, Critter*, StaticItem*, Item*, any_t>(static_script);

                            if (!item->StaticScriptFunc) {
                                throw MapManagerException("Can't bind static item function", map_proto->GetName(), static_script);
                            }
                            if (!item->StaticScriptFunc.HasAttribute("ItemStatic")) {
                                throw MapManagerException("Static item function missing [[ItemStatic]] attribute", map_proto->GetName(), static_script);
                            }
                        }

                        if (const auto trigger_script = item->GetTriggerScript()) {
                            item->TriggerScriptFunc = _engine->FindFunc<void, Critter*, StaticItem*, bool, mdir>(trigger_script);

                            if (!item->TriggerScriptFunc) {
                                throw MapManagerException("Can't bind static item trigger function", map_proto->GetName(), trigger_script);
                            }
                            if (!item->TriggerScriptFunc.HasAttribute("ItemTrigger")) {
                                throw MapManagerException("Static item trigger function missing [[ItemTrigger]] attribute", map_proto->GetName(), trigger_script);
                            }
                        }

                        // Sort
                        if (item->GetStatic()) {
                            FO_VERIFY_AND_THROW(item->GetOwnership() == ItemOwnership::MapHex, "Item is not placed on map hex");
                            static_map->StaticItems.emplace_back(item);
                            static_map->StaticItemsById.emplace(item_id, item);

                            const auto add_item_to_field = [item_ = ptr<StaticItem> {item}](ptr<StaticMap::Field> static_field) {
                                if (!vec_exists(static_field->StaticItems, item_)) {
                                    static_field->StaticItems.reserve(static_field->StaticItems.size() + 1);
                                    static_field->StaticItems.emplace_back(item_);

                                    if (item_->GetIsTrigger()) {
                                        static_field->TriggerItems.reserve(static_field->TriggerItems.size() + 1);
                                        static_field->TriggerItems.emplace_back(item_);
                                    }

                                    if (!item_->GetNoBlock()) {
                                        static_field->MoveBlocked = true;
                                    }
                                    if (!item_->GetShootThru()) {
                                        static_field->ShootBlocked = true;
                                        static_field->MoveBlocked = true;
                                    }
                                }
                            };

                            const auto hex = item->GetHex();
                            ptr<StaticMap::Field> static_field = static_map->HexField->GetCellForWriting(hex);
                            add_item_to_field(static_field);

                            if (item->IsNonEmptyMultihexLines()) {
                                GeometryHelper::ForEachMultihexLines(item->GetMultihexLines(), hex, map_size, [&](mpos multihex) {
                                    ptr<StaticMap::Field> multihex_field = static_map->HexField->GetCellForWriting(multihex);
                                    add_item_to_field(multihex_field);
                                });
                            }
                            if (item->IsNonEmptyMultihexMesh()) {
                                for (const auto multihex : item->GetMultihexMesh()) {
                                    if (multihex != hex && map_size.is_valid_pos(multihex)) {
                                        ptr<StaticMap::Field> multihex_field = static_map->HexField->GetCellForWriting(multihex);
                                        add_item_to_field(multihex_field);
                                    }
                                }
                            }
                        }
                        else {
                            if (item->GetOwnership() == ItemOwnership::MapHex) {
                                static_map->HexItemBillets.emplace_back(item_id, item);
                            }
                            else {
                                FO_VERIFY_AND_THROW(item->GetOwnership() == ItemOwnership::CritterInventory || item->GetOwnership() == ItemOwnership::ItemContainer, "Map item load produced item with unsupported ownership");
                                static_map->ChildItemBillets.emplace_back(item_id, item);
                            }
                        }
                    }
                }
            }

            reader.VerifyEnd();

            // Scroll blocks
            const irect32 scroll_area = map_proto->GetScrollAxialArea();
            const int32_t scroll_block_size = _engine->Settings->ScrollBlockSize;

            if (!scroll_area.is_zero()) {
                for (const auto hx : iterate_range(map_size.width)) {
                    for (const auto hy : iterate_range(map_size.height)) {
                        const mpos hex = {hx, hy};
                        const ipos32 axial_hex = GeometryHelper::GetHexAxialCoord(hex);

                        // Is hex on scroll block line
                        if ((axial_hex.x >= scroll_area.x - scroll_block_size && axial_hex.x <= scroll_area.x + scroll_block_size) || //
                            (axial_hex.x >= scroll_area.x + scroll_area.width - scroll_block_size && axial_hex.x <= scroll_area.x + scroll_area.width + scroll_block_size) || //
                            (axial_hex.y >= scroll_area.y - scroll_block_size && axial_hex.y <= scroll_area.y + scroll_block_size) || //
                            (axial_hex.y >= scroll_area.y + scroll_area.height - scroll_block_size && axial_hex.y <= scroll_area.y + scroll_area.height + scroll_block_size)) {
                            ptr<StaticMap::Field> field = static_map->HexField->GetCellForWriting(hex);
                            field->MoveBlocked = true;
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

auto MapManager::GetStaticMap(ptr<const ProtoMap> proto) -> ptr<StaticMap>
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _staticMaps.find(proto);
    FO_VERIFY_AND_THROW(it != _staticMaps.end(), "Lookup failed in static maps");
    return it->second;
}

void MapManager::GenerateMapContent(ptr<Map> map)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Map is already destroyed");

    unordered_map<ident_t, ident_t> id_map;

    // Generate critters
    for (auto&& [base_cr_id, base_cr] : map->GetStaticMap()->CritterBillets) {
        auto cr = _engine->CrMngr.CreateCritterOnMap(base_cr->GetProtoId(), base_cr->GetProperties(), map, base_cr->GetHex(), base_cr->GetDir());
        id_map.emplace(base_cr_id, cr->GetId());
        FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Map is already destroyed");
    }

    // Generate hex items
    for (auto&& [base_item_id, base_item] : map->GetStaticMap()->HexItemBillets) {
        auto item = _engine->ItemMngr.CreateItem(base_item->GetProtoId(), 0, base_item->GetProperties());
        id_map.emplace(base_item_id, item->GetId());
        FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Map is already destroyed");
        map->AddItem(item, base_item->GetHex(), nullptr);
        FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Map is already destroyed");
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
        auto item = _engine->ItemMngr.CreateItem(base_item->GetProtoId(), 0, base_item->GetProperties());
        FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Map is already destroyed");

        // Add to parent
        if (base_item->GetOwnership() == ItemOwnership::CritterInventory) {
            auto nullable_cr_cont = map->GetCritter(owner_id);
            FO_VERIFY_AND_THROW(nullable_cr_cont, "Missing required critter container");

            auto cr_cont = nullable_cr_cont.as_ptr();
            _engine->CrMngr.AddItemToCritter(cr_cont, item, false);
            FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Map is already destroyed");
        }
        else if (base_item->GetOwnership() == ItemOwnership::ItemContainer) {
            auto nullable_item_cont = map->GetItem(owner_id);
            FO_VERIFY_AND_THROW(nullable_item_cont, "Missing required item container");

            auto item_cont = nullable_item_cont.as_ptr();
            item_cont->AddItemToContainer(item, {});
            FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Map is already destroyed");
        }
        else {
            FO_UNREACHABLE_PLACE();
        }
    }

    // Process visibility
    for (ptr<Critter> cr : copy_hold_ref(map->GetCritters())) {
        if (!cr->IsDestroyed()) {
            ProcessVisibleCritters(cr);
            FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Map is already destroyed");
        }
        if (!cr->IsDestroyed()) {
            ProcessVisibleItems(cr);
            FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Map is already destroyed");
        }
    }
}

void MapManager::DestroyMapContent(ptr<Map> map)
{
    FO_STACK_TRACE_ENTRY();

    ValidateEntityAccess(map);

    for (size_t prev_deps = std::numeric_limits<size_t>::max(); map->HasCritters() || map->HasItems();) {
        for (ptr<Critter> cr : copy_hold_ref(map->GetCritters())) {
            ValidateEntityAccess(cr);

            if (cr->GetControlledByPlayer()) {
                TransferToGlobal(cr, {});
            }
            else if (cr->IsDestroying()) {
                RemoveCritterFromMap(cr, map);
            }
            else {
                _engine->CrMngr.DestroyCritter(cr);
            }
        }

        for (ptr<Item> item : copy_hold_ref(map->GetItems())) {
            ValidateEntityAccess(item);
            _engine->ItemMngr.DestroyItem(item);
        }

        // Each pass must strictly reduce the map's remaining content; non-convergence is corruption.
        const size_t remaining_deps = map->GetCritters().size() + map->GetItems().size();
        FO_STRONG_ASSERT(remaining_deps < prev_deps, "Map content destruction made no progress", map->GetId(), remaining_deps, prev_deps);
        prev_deps = remaining_deps;
    }
}

auto MapManager::CreateLocation(hstring proto_id, const_span<hstring> map_pids, nptr<const Properties> props) -> ptr<Location>
{
    FO_STACK_TRACE_ENTRY();

    auto proto = _engine->GetProtoLocation(proto_id);

    if (!proto) {
        throw GenericException("Location proto not found", proto_id);
    }

    auto loc = SafeAlloc::MakeRefCounted<Location>(_engine, ident_t {}, proto.as_ptr(), props);

    _engine->EntityMngr.RegisterLocation(loc);

    {
        ScopedSyncContext create_scope;

        for (const auto map_pid : map_pids) {
            auto nullable_map_proto = _engine->GetProtoMap(map_pid);

            if (!nullable_map_proto) {
                throw GenericException("Map proto not found", map_pid);
            }

            auto map_proto = nullable_map_proto.as_ptr();
            auto static_map = GetStaticMap(map_proto);
            auto map = SafeAlloc::MakeRefCounted<Map>(_engine, ident_t {}, map_proto, loc, static_map);
            _engine->EntityMngr.RegisterMap(map);
            loc->AddMap(map);
            GenerateMapContent(map);
        }

        for (ptr<Map> map : copy_hold_ref(loc->GetMaps())) {
            _engine->EntityMngr.CallInit(map, true);
        }

        if (!loc->IsDestroyed()) {
            _engine->EntityMngr.CallInit(loc, true);
        }
    }

    if (loc->IsDestroyed()) {
        throw GenericException("Location destroyed during init");
    }

    return loc;
}

auto MapManager::CreateMap(hstring proto_id, ptr<Location> loc) -> ptr<Map>
{
    FO_STACK_TRACE_ENTRY();

    ValidateEntityAccess(loc);
    FO_VERIFY_AND_THROW(!loc->IsDestroyed(), "Location is already destroyed");
    FO_VERIFY_AND_THROW(!loc->IsDestroying(), "Location is already being destroyed");

    auto nullable_map_proto = _engine->GetProtoMap(proto_id);

    if (!nullable_map_proto) {
        throw GenericException("Map proto not found", proto_id);
    }

    auto map_proto = nullable_map_proto.as_ptr();
    auto static_map = GetStaticMap(map_proto);
    auto map = SafeAlloc::MakeRefCounted<Map>(_engine, ident_t {}, map_proto, loc, static_map);

    _engine->EntityMngr.RegisterMap(map);
    loc->AddMap(map);

    GenerateMapContent(map);

    if (!map->IsDestroyed()) {
        _engine->EntityMngr.CallInit(map, true);
    }

    if (!map->IsDestroyed()) {
        ValidateEntityAccess(loc);
        ValidateEntityAccess(map);
        loc->OnMapAdded.Fire(map);
    }

    if (map->IsDestroyed()) {
        throw GenericException("Map destroyed during init");
    }

    return map;
}

void MapManager::RegenerateMap(ptr<Map> map)
{
    FO_STACK_TRACE_ENTRY();

    ValidateEntityAccess(map.as_nptr());
    FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Map is already destroyed");

    auto map_holder = map.hold_ref();
    ignore_unused(map_holder);

    DestroyMapContent(map);

    if (!map->IsDestroyed()) {
        GenerateMapContent(map);
    }
}

auto MapManager::GetLocationByPid(hstring loc_pid, int32_t skip_count) noexcept -> refcount_nptr<Location>
{
    FO_STACK_TRACE_ENTRY();

    vector<refcount_ptr<Location>> locations = _engine->EntityMngr.GetLocations();

    for (size_t i = 0; i < locations.size(); i++) {
        if (locations[i]->GetProtoId() == loc_pid) {
            if (skip_count <= 0) {
                return std::move(locations[i]);
            }

            skip_count--;
        }
    }

    return nullptr;
}

auto MapManager::GetMapByPid(hstring map_pid, int32_t skip_count) noexcept -> refcount_nptr<Map>
{
    FO_STACK_TRACE_ENTRY();

    vector<refcount_ptr<Map>> maps = _engine->EntityMngr.GetMaps();

    for (size_t i = 0; i < maps.size(); i++) {
        if (maps[i]->GetProtoId() == map_pid) {
            if (skip_count <= 0) {
                return std::move(maps[i]);
            }

            skip_count--;
        }
    }

    return nullptr;
}

void MapManager::DestroyLocation(ptr<Location> loc)
{
    FO_STACK_TRACE_ENTRY();

    auto loc_holder = loc.hold_ref();
    ignore_unused(loc_holder);

    EnsureEntitySynced(loc);

    if (loc->IsDestroying() || loc->IsDestroyed()) {
        return;
    }

    for (ptr<Map> map : loc->GetMaps()) {
        ValidateEntityAccess(map);
        FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Map is already destroyed");
        FO_VERIFY_AND_THROW(!map->IsDestroying(), "Map is already being destroyed");
    }

    // Destroy location in couple with maps
    loc->MarkAsDestroying();

    for (ptr<Map> map : loc->GetMaps()) {
        map->MarkAsDestroying();
    }

    ValidateEntityAccess(loc);
    _engine->OnLocationFinish.Fire(loc);
    FO_VERIFY_AND_THROW(!loc->IsDestroyed(), "Location is already destroyed");

    for (ptr<Map> map : loc->GetMaps()) {
        ValidateEntityAccess(map);
        _engine->OnMapFinish.Fire(map);
        FO_VERIFY_AND_THROW(!loc->IsDestroyed(), "Location is already destroyed");
        FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Map is already destroyed");
        FO_VERIFY_AND_THROW(map->GetLocation() == loc, "Map is attached to a different location during location destruction", map->GetId(), loc->GetId(), map->GetLocation() ? map->GetLocation()->GetId() : ident_t {});
    }

    for (ptr<Map> map : loc->GetMaps()) {
        EnsureEntitySynced(map);
        DestroyMapInternal(map);
    }

    for (size_t prev_deps = std::numeric_limits<size_t>::max(); loc->HasInnerEntities();) {
        try {
            if (loc->HasInnerEntities()) {
                _engine->EntityMngr.DestroyInnerEntities(loc);
            }
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
        }

        // Each pass must strictly reduce the location's remaining inner entities; non-convergence is corruption.
        const size_t remaining_deps = loc->GetInnerEntitiesCount();
        FO_STRONG_ASSERT(remaining_deps < prev_deps, "Location inner-entity destruction made no progress", loc->GetId(), remaining_deps, prev_deps);
        prev_deps = remaining_deps;
    }

    _engine->TimeEventMngr.CancelAllForEntity(loc);
    loc->SetParent(nullptr);
    loc->MarkAsDestroyed();
    _engine->EntityMngr.UnregisterLocation(loc);
}

void MapManager::DestroyMap(ptr<Map> map)
{
    FO_STACK_TRACE_ENTRY();

    ValidateEntityAccess(map.as_nptr());
    auto map_holder = map.hold_ref();
    ignore_unused(map_holder);

    if (map->IsDestroying() || map->IsDestroyed()) {
        return;
    }

    map->MarkAsDestroying();
    ValidateEntityAccess(map);
    _engine->OnMapFinish.Fire(map);
    FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Map is already destroyed");

    auto nullable_loc = map->GetLocation();
    FO_VERIFY_AND_THROW(nullable_loc, "Missing location instance");
    auto loc = nullable_loc.as_ptr().hold_ref();
    ValidateEntityAccess(loc);
    ValidateEntityAccess(map.as_nptr());
    loc->OnMapRemoved.Fire(map);
    FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Map is already destroyed");
    FO_VERIFY_AND_THROW(!loc->IsDestroyed(), "Location is already destroyed");
    FO_VERIFY_AND_THROW(map->GetLocation() == nptr<Location> {loc}, "Map is attached to a different location during map destruction", map->GetId(), loc->GetId(), map->GetLocation() ? map->GetLocation()->GetId() : ident_t {});

    DestroyMapInternal(map);
}

void MapManager::DestroyMapInternal(ptr<Map> map)
{
    FO_STACK_TRACE_ENTRY();

    ValidateEntityAccess(map);

    // Eject spectators before destroying map content so each spectator gets a clean LoadMap(nullptr) and clears its ViewMap.
    while (map->HasSpectatorPlayers()) {
        ptr<Player> player = map->GetSpectatorPlayers().back();
        ValidateEntityAccess(player);
        player->ResetViewMap();
        safe_call([&] { player->Send_LoadMap(nullptr); });
    }

    for (size_t prev_deps = std::numeric_limits<size_t>::max(); map->HasCritters() || map->HasItems() || map->HasInnerEntities();) {
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

        // Each pass must strictly reduce the map's remaining content; non-convergence is corruption.
        const size_t remaining_deps = map->GetCritters().size() + map->GetItems().size() + map->GetInnerEntitiesCount();
        FO_STRONG_ASSERT(remaining_deps < prev_deps, "Map destruction made no progress", map->GetId(), remaining_deps, prev_deps);
        prev_deps = remaining_deps;
    }

    _engine->TimeEventMngr.CancelAllForEntity(map);

    auto loc = map->GetLocation();
    FO_VERIFY_AND_THROW(loc, "Missing location instance");
    ValidateEntityAccess(loc);
    loc->RemoveMap(map);

    map->SetParent(nullptr);
    map->MarkAsDestroyed();
    _engine->EntityMngr.UnregisterMap(map);
}

auto MapManager::TracePath(ptr<const Map> map, mpos start_hex, mpos target_hex, int32_t max_dist, float32_t angle, nptr<const Critter> nullable_find_cr, CritterFindType find_type, bool check_last_movable, bool collect_critters) const -> TraceResult
{
    FO_STACK_TRACE_ENTRY();

    ValidateEntityAccess(map);

    TraceResult output;
    output.FullyTraced = false;
    output.IsCritterFound = false;
    output.HasLastMovable = false;

    const auto map_size = map->GetSize();
    const auto dist = max_dist != 0 ? max_dist : GeometryHelper::GetDistance(start_hex, target_hex);

    auto tracer = LineTracer(start_hex, target_hex, angle, map_size);
    auto next_hex = start_hex;
    auto prev_hex = next_hex;
    bool last_passed_ok = false;

    for (int32_t i = 0;; i++) {
        if (i >= dist) {
            output.FullyTraced = true;
            break;
        }

        if (!tracer.GetNextHex(next_hex).has_value()) {
            break;
        }

        if (check_last_movable && !last_passed_ok) {
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

        if (collect_critters && map->IsCritterOnHex(next_hex, CritterFindType::Any)) {
            const auto critters = map->GetCrittersOnHex(next_hex, find_type);

            for (ptr<const Critter> cr : critters) {
                if (std::ranges::find(output.Critters, cr) == output.Critters.end()) {
                    output.Critters.emplace_back(cr);
                }
            }
        }

        if (nullable_find_cr) {
            auto find_cr = nullable_find_cr.as_ptr();

            if (map->IsCritterOnHex(next_hex, find_cr)) {
                output.IsCritterFound = true;
                break;
            }
        }

        prev_hex = next_hex;
    }

    output.PreBlock = prev_hex;
    output.Block = next_hex;

    return output;
}

auto MapManager::FindPath(ptr<const Map> map, nptr<const Critter> nullable_from_cr, mpos from_hex, mpos to_hex, int32_t multihex, int32_t cut, ipos16 to_hex_offset, function<bool(ptr<const Item>)> gag_callback) const -> FindPathOutput
{
    FO_STACK_TRACE_ENTRY();

    ValidateEntityAccess(map);

    // Pre-validate target hex (terrain/items only; critters are always passable)
    if (cut == 0) {
        const bool target_movable = multihex > 0 ? map->IsHexesMovable(to_hex, multihex) : map->IsHexMovable(to_hex);

        if (!target_movable && !map->CheckGagItem(to_hex, gag_callback)) {
            FindPathOutput output;
            output.Result = FindPathOutput::ResultType::HexBusy;
            return output;
        }
    }

    FindPathInput settings;
    settings.FromHex = from_hex;
    settings.FromHexOffset = {};

    if (nullable_from_cr) {
        auto from_cr = nullable_from_cr.as_ptr();
        settings.FromHexOffset = from_cr->GetHexOffset();
    }

    settings.ToHex = to_hex;
    settings.ToHexOffset = to_hex_offset;
    settings.MapSize = map->GetSize();
    settings.MaxLength = _engine->Settings->MaxPathFindLength;
    settings.Cut = cut;
    settings.Multihex = multihex;
    settings.FreeMovement = _engine->Settings->MapFreeMovement;

    settings.CheckHex = [&](mpos hex) -> HexBlockResult {
        if (!map->IsHexMovable(hex)) {
            if (map->CheckGagItem(hex, gag_callback)) {
                return HexBlockResult::DeferGag;
            }

            return HexBlockResult::Blocked;
        }

        if (map->HasLivingCritter(hex, nullable_from_cr)) {
            return HexBlockResult::DeferCritter;
        }

        return HexBlockResult::Passable;
    };

    return PathFinding::FindPath(settings);
}

void MapManager::TransferToMap(ptr<Critter> cr, ptr<Map> map, mpos hex, mdir dir, optional<int32_t> safe_radius)
{
    FO_STACK_TRACE_ENTRY();

    Transfer(cr, map, hex, dir, safe_radius, {});
}

void MapManager::TransferToGlobal(ptr<Critter> cr, ident_t global_cr_id)
{
    FO_STACK_TRACE_ENTRY();

    Transfer(cr, nullptr, {}, mdir {}, std::nullopt, global_cr_id);
}

void MapManager::Transfer(ptr<Critter> cr, nptr<Map> map, mpos hex, mdir dir, optional<int32_t> safe_radius, ident_t global_cr_id)
{
    FO_STACK_TRACE_ENTRY();

    EnsureEntitySynced(cr);
    ValidateEntityAccess(map);

    if (map != nullptr) {
        FO_VERIFY_AND_THROW(map->GetSize().is_valid_pos(hex), "Critter transfer target hex is outside target map bounds", cr->GetId(), map->GetId(), hex, map->GetSize());
    }

    if (cr->IsMapTransfersLocked()) {
        throw GenericException("Critter transfers locked");
    }

    cr->LockMapTransfers();
    auto restore_transfers = scope_exit([cr]() mutable noexcept { cr->UnlockMapTransfers(); });

    const auto prev_map_id = cr->GetMapId();
    refcount_nptr<Map> prev_map_ref = prev_map_id ? cr->GetParent<Map>() : refcount_nptr<Map> {};
    FO_VERIFY_AND_THROW(!prev_map_id || !!prev_map_ref, "Previous map id is set but previous map was not found");
    ValidateEntityAccess(prev_map_ref);

    if (map != nullptr && map != prev_map_ref) {
        auto loc = map->GetLocation();
        FO_VERIFY_AND_THROW(loc, "Missing location instance");
        ValidateEntityAccess(loc);
    }

    cr->StopMoving();

    if (cr->GetIsAttached()) {
        cr->DetachFromCritter();
    }

    vector<ptr<Critter>> attached_critters;

    if (cr->HasAttachedCritters()) {
        const auto attached_span = cr->GetAttachedCritters();
        attached_critters.assign(attached_span.begin(), attached_span.end());

        for (auto& attached_cr : attached_critters) {
            attached_cr->DetachFromCritter();
        }
    }

    if (prev_map_ref == map) {
        // Between one map
        if (map != nullptr) {
            FO_VERIFY_AND_THROW(map->GetSize().is_valid_pos(hex), "Critter intra-map transfer target hex is outside map bounds", cr->GetId(), map->GetId(), hex, map->GetSize());

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

        if (map) {
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

        RemoveCritterFromMap(cr, prev_map_ref);

        if (cr->IsDestroyed()) {
            return;
        }

        AddCritterToMap(cr, map, start_hex, dir, global_cr_id);

        if (cr->IsDestroyed()) {
            return;
        }
        if (map && map->IsDestroyed()) {
            return;
        }

        cr->Send_LoadMap(map);
        cr->Send_AddCritter(cr);

        if (!map) {
            for (ptr<Critter> group_cr : cr->GetGlobalMapGroup()) {
                if (group_cr != cr) {
                    cr->Send_AddCritter(group_cr);
                }
            }
        }

        if (map) {
            ValidateEntityAccess(map);
            ValidateEntityAccess(cr);
            _engine->OnMapCritterIn.Fire(map.as_ptr(), cr);

            if (cr->IsDestroyed() || map->IsDestroyed()) {
                return;
            }
            if (cr->GetMapId() != map->GetId()) {
                return;
            }

            ValidateEntityAccess(map);
            EnsureEntitySynced(cr);

            auto map_cr = map->GetCritter(cr->GetId());
            nptr<Critter> expected_cr = cr;
            FO_VERIFY_AND_THROW(map_cr == expected_cr, "Critter is not registered in the target map after map-enter event", map->GetId(), cr->GetId(), cr->GetMapId());
        }
        else {
            ValidateEntityAccess(cr);
            _engine->OnGlobalMapCritterIn.Fire(cr);

            if (cr->IsDestroyed()) {
                return;
            }
            if (cr->GetMapId()) {
                return;
            }

            EnsureEntitySynced(cr);

            FO_VERIFY_AND_THROW(cr->GetRawGlobalMapGroup(), "Critter has no global map group");
        }

        ValidateEntityAccess(map);
        EnsureEntitySynced(cr);

        ProcessVisibleCritters(cr);
        ProcessVisibleItems(cr);

        if (cr->IsDestroyed()) {
            return;
        }

        ValidateEntityAccess(map);
        EnsureEntitySynced(cr);
        _engine->OnCritterSendInitialInfo.Fire(cr);

        if (cr->IsDestroyed()) {
            return;
        }

        ValidateEntityAccess(map);
        EnsureEntitySynced(cr);

        if (map) {
            if (map->IsDestroyed() || cr->GetMapId() != map->GetId()) {
                return;
            }

            auto map_cr = map->GetCritter(cr->GetId());
            nptr<Critter> expected_cr = cr;
            if (!(map_cr == expected_cr)) {
                return;
            }
        }
        else {
            if (cr->GetMapId()) {
                return;
            }
        }

        cr->Send_PlaceToGameComplete();
    }

    // Transfer attached critters
    if (!attached_critters.empty()) {
        for (auto& attached_cr : attached_critters) {
            if (!cr->IsDestroyed() && !attached_cr->IsDestroyed() && !attached_cr->GetIsAttached() && attached_cr->GetMapId() == prev_map_id) {
                Transfer(attached_cr, map, cr->GetHex(), dir, std::nullopt, cr->GetId());
            }
        }

        if (!cr->IsDestroyed() && !cr->GetIsAttached()) {
            ptr<Critter> master_cr = cr;

            for (auto& attached_cr : attached_critters) {
                if (!attached_cr->IsDestroyed() && !attached_cr->GetIsAttached() && !attached_cr->HasAttachedCritters() && attached_cr->GetMapId() == cr->GetMapId()) {
                    attached_cr->AttachToCritter(master_cr);
                }
            }

            if (!cr->IsDestroyed()) {
                cr->MoveAttachedCritters();
                ProcessVisibleCritters(master_cr);
            }
        }
    }

    // ValidateEntityAccess and event dispatch tolerate destroyed entity arguments.
    ValidateEntityAccess(cr);
    ValidateEntityAccess(prev_map_ref);
    _engine->OnCritterTransfer.Fire(cr, prev_map_ref);
}

void MapManager::AddCritterToMap(ptr<Critter> cr, nptr<Map> map, mpos hex, mdir dir, ident_t global_cr_id)
{
    FO_STACK_TRACE_ENTRY();

    ValidateEntityAccess(cr);
    ValidateEntityAccess(map);

    FO_VERIFY_AND_THROW(!cr->IsDestroyed(), "Critter is already destroyed");
    cr->LockMapTransfers();
    auto restore_transfers = scope_exit([cr]() mutable noexcept { cr->UnlockMapTransfers(); });

    if (map != nullptr) {
        FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Cannot add a critter to an already destroyed map", map->GetId(), cr->GetId());
        FO_VERIFY_AND_THROW(!map->IsDestroying(), "Cannot add a critter to a map that is being destroyed", map->GetId(), cr->GetId());
        FO_VERIFY_AND_THROW(map->GetSize().is_valid_pos(hex), "Critter map placement target hex is outside map bounds", cr->GetId(), map->GetId(), hex, map->GetSize());

        cr->SetMapId(map->GetId());
        cr->SetHex(hex);
        cr->ChangeDir(dir);

        if (!cr->GetControlledByPlayer()) {
            auto cr_ids = map->GetCritterIds();
            vec_add_unique_value(cr_ids, cr->GetId());
            map->SetCritterIds(cr_ids);
        }

        map->AddCritter(cr);

        // Notify spectators
        if (map->HasSpectatorPlayers()) {
            for (ptr<Player> player : copy_hold_ref(map->GetSpectatorPlayers())) {
                player->Send_AddCritter(cr);
            }
        }
    }
    else {
        auto& cr_group = cr->GetRawGlobalMapGroup();
        FO_VERIFY_AND_THROW(!cr_group, "Critter group is already set");

        refcount_nptr<Critter> global_cr_ref {};
        if (global_cr_id && global_cr_id != cr->GetId()) {
            auto nullable_global_cr = _engine->EntityMngr.GetCritter(global_cr_id);

            if (nullable_global_cr) {
                global_cr_ref = std::move(nullable_global_cr);
            }
        }

        ValidateEntityAccess(global_cr_ref);

        cr->SetMapId({});
        cr->SetParent(nullptr);

        if (!global_cr_ref || global_cr_ref->GetMapId()) {
            // Tight lambda so the property lock is released (even on a throw) exactly when the
            // trip-id read/advance finishes, without widening the locked region over the work below.
            const auto trip_id = [this]() {
                _engine->LockForPropertyAccess();
                auto unlock_prop = scope_exit([this]() noexcept { _engine->UnlockForPropertyAccess(); });
                const auto next_trip_id = _engine->GetLastGlobalMapTripId() + 1;
                _engine->SetLastGlobalMapTripId(next_trip_id);
                return next_trip_id;
            }();

            cr->SetGlobalMapTripId(trip_id);

            cr_group = SafeAlloc::MakeShared<vector<ptr<Critter>>>();
        }
        else {
            auto& global_cr_group = global_cr_ref->GetRawGlobalMapGroup();
            FO_VERIFY_AND_THROW(global_cr_group, "Missing required global critter group");

            cr->SetGlobalMapTripId(global_cr_ref->GetGlobalMapTripId());

            for (ptr<Critter> group_cr : *global_cr_group) {
                group_cr->Send_AddCritter(cr);
            }

            cr_group = global_cr_group;
        }

        vec_add_unique_value(*cr_group, cr);
    }
}

void MapManager::RemoveCritterFromMap(ptr<Critter> cr, nptr<Map> nullable_map)
{
    FO_STACK_TRACE_ENTRY();

    ValidateEntityAccess(cr);
    ValidateEntityAccess(nullable_map);

    FO_VERIFY_AND_THROW(!cr->IsDestroyed(), "Critter is already destroyed");
    auto cr_holder = cr.hold_ref();
    ignore_unused(cr_holder);
    cr->LockMapTransfers();
    auto restore_transfers = scope_exit([cr]() mutable noexcept { cr->UnlockMapTransfers(); });

    if (nullable_map != nullptr) {
        auto map = nullable_map.as_ptr();
        FO_VERIFY_AND_THROW(cr->GetMapId() == map->GetId(), "Critter belongs to a different map");
        auto map_holder = map.hold_ref();
        ignore_unused(map_holder);
        ValidateEntityAccess(map);
        ValidateEntityAccess(cr);
        _engine->OnMapCritterOut.Fire(map, cr);

        if (cr->IsDestroyed() || map->IsDestroyed()) {
            return;
        }

        ValidateEntityAccess(map);
        EnsureEntitySynced(cr);

        FO_VERIFY_AND_THROW(cr->GetMapId() == map->GetId(), "Critter belongs to a different map");
        auto map_cr = map->GetCritter(cr->GetId());
        nptr<Critter> expected_cr = cr;
        FO_VERIFY_AND_THROW(map_cr == expected_cr, "Critter is not registered in its map before map-exit notification", map->GetId(), cr->GetId(), cr->GetMapId());

        for (ptr<Critter> other : copy_hold_ref(cr->GetCritters(CritterSeeType::WhoSeeMe, CritterFindType::Any))) {
            if (other->IsDestroyed()) {
                continue;
            }

            ValidateEntityAccess(other);
            ValidateEntityAccess(cr);
            other->OnCritterDisappeared.Fire(cr);

            if (cr->IsDestroyed() || map->IsDestroyed()) {
                return;
            }

            ValidateEntityAccess(map);
            EnsureEntitySynced(cr);

            FO_VERIFY_AND_THROW(cr->GetMapId() == map->GetId(), "Critter belongs to a different map");
            auto current_map_cr = map->GetCritter(cr->GetId());
            nptr<Critter> current_expected_cr = cr;
            FO_VERIFY_AND_THROW(current_map_cr == current_expected_cr, "Critter is not registered in its map after disappearance notification", map->GetId(), cr->GetId(), cr->GetMapId());
        }

        ValidateEntityAccess(map);
        EnsureEntitySynced(cr);

        cr->ClearVisibleEnitites();

        // Notify spectators
        if (map->HasSpectatorPlayers()) {
            for (ptr<Player> player : copy_hold_ref(map->GetSpectatorPlayers())) {
                player->Send_RemoveCritter(cr);
            }
        }

        map->RemoveCritter(cr);
        cr->SetMapId({});

        if (!cr->GetControlledByPlayer()) {
            auto cr_ids = map->GetCritterIds();
            vec_remove_unique_value(cr_ids, cr->GetId());
            map->SetCritterIds(cr_ids);
        }
    }
    else {
        FO_VERIFY_AND_THROW(!cr->IsDestroyed(), "Critter is already destroyed");
        FO_VERIFY_AND_THROW(!cr->GetMapId(), "Critter still has a map id before global map insertion");
        ValidateEntityAccess(cr);
        _engine->OnGlobalMapCritterOut.Fire(cr);

        if (cr->IsDestroyed()) {
            return;
        }

        EnsureEntitySynced(cr);

        FO_VERIFY_AND_THROW(!cr->GetMapId(), "Critter still has a map id before global map insertion");
        FO_VERIFY_AND_THROW(cr->GetRawGlobalMapGroup(), "Critter has no global map group");

        cr->SetGlobalMapTripId({});
        auto& cr_group = cr->GetRawGlobalMapGroup();
        vec_remove_unique_value(*cr_group, cr);

        for (ptr<Critter> group_cr : *cr_group) {
            group_cr->Send_RemoveCritter(cr);
        }

        cr_group.reset();
    }
}

void MapManager::ProcessVisibleCritters(ptr<Critter> cr)
{
    FO_STACK_TRACE_ENTRY();

    if (cr->IsDestroying() || cr->IsDestroyed()) {
        return;
    }

    ValidateEntityAccess(cr);

    if (cr->GetMapId()) {
        auto nullable_map = cr->GetParent<Map>();
        FO_VERIFY_AND_THROW(nullable_map, "Missing map instance");
        auto map = nullable_map.as_ptr();
        ValidateEntityAccess(map);

        for (ptr<Critter> target : copy_hold_ref(map->GetCritters())) {
            ProcessCritterLook(map, cr, target);
            ProcessCritterLook(map, target, cr);
        }
    }
    else {
        FO_VERIFY_AND_THROW(cr->GetRawGlobalMapGroup(), "Critter has no global map group");
    }
}

void MapManager::ProcessCritterLook(ptr<Map> map, ptr<Critter> cr, ptr<Critter> target)
{
    FO_STACK_TRACE_ENTRY();

    ValidateEntityAccess(map.as_nptr());

    auto map_holder = map.hold_ref();
    auto cr_holder = cr.hold_ref();
    auto target_holder = target.hold_ref();
    ignore_unused(map_holder);
    ignore_unused(cr_holder);
    ignore_unused(target_holder);

    const auto is_look_context_changed = [&map, &cr, &target]() noexcept -> bool {
        if (map->IsDestroying() || map->IsDestroyed() || cr->IsDestroying() || cr->IsDestroyed() || target->IsDestroying() || target->IsDestroyed()) {
            return true;
        }
        if (map->GetCritter(cr->GetId()) != cr) {
            return true;
        }
        if (map->GetCritter(target->GetId()) != target) {
            return true;
        }

        return false;
    };

    if (cr == target) {
        return;
    }
    if (is_look_context_changed()) {
        return;
    }

    ValidateEntityAccess(cr);
    ValidateEntityAccess(target);

    auto vis_mode = IsCritterSeeCritter(map, cr, target);

    if (vis_mode == CritterVisibilityMode::None && target->HasAttachedCritters()) {
        for (auto& attached_cr : target->GetAttachedCritters()) {
            FO_VERIFY_AND_THROW(attached_cr->GetMapId() == map->GetId(), "Attached critter is on a different map than its visibility target", target->GetId(), attached_cr->GetId(), attached_cr->GetMapId(), map->GetId());

            const auto attached_mode = IsCritterSeeCritter(map, cr, attached_cr);

            if (attached_mode != CritterVisibilityMode::None) {
                vis_mode = attached_mode;
                break;
            }
        }
    }

    const bool is_see = vis_mode != CritterVisibilityMode::None;

    if (is_see) {
        if (target->AddVisibleCritter(cr, vis_mode)) {
            cr->Send_AddCritter(target);
            ValidateEntityAccess(cr);
            ValidateEntityAccess(target);
            cr->OnCritterAppeared.Fire(target);

            if (is_look_context_changed()) {
                return;
            }
        }
        else {
            const auto prev_mode = cr->GetVisibleCritterMode(target->GetId());

            if (prev_mode != vis_mode) {
                cr->SetVisibleCritterMode(target->GetId(), vis_mode);
                cr->Send_CritterVisibilityMode(target, vis_mode);
                ValidateEntityAccess(cr);
                ValidateEntityAccess(target);
                cr->OnCritterVisibilityModeChanged.Fire(target, vis_mode);

                if (is_look_context_changed()) {
                    return;
                }
            }
        }
    }
    else {
        if (target->RemoveVisibleCritter(cr)) {
            cr->Send_RemoveCritter(target);
            ValidateEntityAccess(cr);
            ValidateEntityAccess(target);
            cr->OnCritterDisappeared.Fire(target);

            if (is_look_context_changed()) {
                return;
            }
        }
    }

    const int32_t dist = GeometryHelper::GetDistance(cr->GetHex(), target->GetHex());
    const int32_t show_cr_dist1 = cr->GetShowCritterDist1();
    const int32_t show_cr_dist2 = cr->GetShowCritterDist2();
    const int32_t show_cr_dist3 = cr->GetShowCritterDist3();

    if (show_cr_dist1 != 0) {
        if (show_cr_dist1 >= dist && is_see) {
            if (cr->AddCrIntoVisGroup1(target->GetId())) {
                ValidateEntityAccess(cr);
                ValidateEntityAccess(target);
                cr->OnCritterAppearedDist1.Fire(target);

                if (is_look_context_changed()) {
                    return;
                }
            }
        }
        else {
            if (cr->RemoveCrFromVisGroup1(target->GetId())) {
                ValidateEntityAccess(cr);
                ValidateEntityAccess(target);
                cr->OnCritterDisappearedDist1.Fire(target);

                if (is_look_context_changed()) {
                    return;
                }
            }
        }
    }

    if (show_cr_dist2 != 0) {
        if (show_cr_dist2 >= dist && is_see) {
            if (cr->AddCrIntoVisGroup2(target->GetId())) {
                ValidateEntityAccess(cr);
                ValidateEntityAccess(target);
                cr->OnCritterAppearedDist2.Fire(target);

                if (is_look_context_changed()) {
                    return;
                }
            }
        }
        else {
            if (cr->RemoveCrFromVisGroup2(target->GetId())) {
                ValidateEntityAccess(cr);
                ValidateEntityAccess(target);
                cr->OnCritterDisappearedDist2.Fire(target);

                if (is_look_context_changed()) {
                    return;
                }
            }
        }
    }

    if (show_cr_dist3 != 0) {
        if (show_cr_dist3 >= dist && is_see) {
            if (cr->AddCrIntoVisGroup3(target->GetId())) {
                ValidateEntityAccess(cr);
                ValidateEntityAccess(target);
                cr->OnCritterAppearedDist3.Fire(target);

                if (is_look_context_changed()) {
                    return;
                }
            }
        }
        else {
            if (cr->RemoveCrFromVisGroup3(target->GetId())) {
                ValidateEntityAccess(cr);
                ValidateEntityAccess(target);
                cr->OnCritterDisappearedDist3.Fire(target);

                if (is_look_context_changed()) {
                    return;
                }
            }
        }
    }
}

auto MapManager::IsCritterSeeCritter(ptr<const Map> map, ptr<const Critter> cr, ptr<const Critter> target) const -> CritterVisibilityMode
{
    FO_STACK_TRACE_ENTRY();

    ValidateEntityAccess(map);
    ValidateEntityAccess(cr);
    ValidateEntityAccess(target);
    FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Map is already destroyed");
    FO_VERIFY_AND_THROW(!cr->IsDestroyed(), "Critter is already destroyed");
    FO_VERIFY_AND_THROW(!target->IsDestroyed(), "Map transfer target is already destroyed");

    return CheckCritterVisibilityHook(_engine, map, cr, target);
}

void MapManager::ProcessVisibleItems(ptr<Critter> cr)
{
    FO_STACK_TRACE_ENTRY();

    if (cr->IsDestroying() || cr->IsDestroyed()) {
        return;
    }

    ValidateEntityAccess(cr);

    if (!cr->GetMapId()) {
        return;
    }

    auto nullable_map = cr->GetParent<Map>();
    FO_VERIFY_AND_THROW(nullable_map, "Missing map instance");
    auto map = nullable_map.as_ptr();
    ValidateEntityAccess(map);

    for (ptr<Item> item : copy_hold_ref(map->GetItems())) {
        if (item->IsDestroyed()) {
            continue;
        }

        if (cr->CanSeeItemOnMap(item)) {
            if (cr->AddVisibleItem(item->GetId())) {
                cr->Send_AddItemOnMap(item);
                ValidateEntityAccess(cr);
                ValidateEntityAccess(item);
                cr->OnItemOnMapAppeared.Fire(item, nullptr);
            }
        }
        else {
            if (cr->RemoveVisibleItem(item->GetId())) {
                cr->Send_RemoveItemFromMap(item);
                ValidateEntityAccess(cr);
                ValidateEntityAccess(item);
                cr->OnItemOnMapDisappeared.Fire(item, nullptr);
            }
        }
    }
}

void MapManager::ViewMap(ptr<Player> view_player, ptr<Map> map)
{
    FO_STACK_TRACE_ENTRY();

    ValidateEntityAccess(view_player);
    ValidateEntityAccess(map);

    // Critters: spectator sees every non-destroyed critter on the map
    for (ptr<const Critter> cr : copy_hold_ref(map->GetCritters())) {
        if (cr->IsDestroyed()) {
            continue;
        }

        view_player->Send_AddCritter(cr);
    }

    // Items: spectator sees every non-destroyed item on the map
    for (ptr<const Item> item : copy_hold_ref(map->GetItems())) {
        if (item->IsDestroyed()) {
            continue;
        }

        view_player->Send_AddItemOnMap(item);
    }
}

FO_END_NAMESPACE
