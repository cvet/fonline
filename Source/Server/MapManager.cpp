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

extern CritterVisibilityMode CheckCritterVisibilityHook(const ServerEngine*, const Map*, const Critter*, const Critter*);

MapManager::MapManager(ServerEngine* engine) :
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
        const auto* map_proto = _engine->GetProtoMap(map_pid);

        if (map_proto == nullptr) {
            throw MapManagerException("Map proto not found for static map", map_pid);
        }

        static_map_loadings.emplace_back(map_proto, run_async(strex("LoadStaticMap-{}", map_proto->GetName()), [this, map_proto, &map_file_header]() FO_DEFERRED {
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
                const auto hashes_count = reader.Read<uint32_t>();

                string str;

                for (uint32_t i = 0; i < hashes_count; i++) {
                    const auto str_len = reader.Read<uint32_t>();
                    str.resize(str_len);
                    reader.ReadPtr(str.data(), str.length());
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
                        const auto* cr_proto = _engine->GetProtoCritter(cr_pid);

                        if (cr_proto == nullptr) {
                            throw MapManagerException("Critter proto not found on map", map_proto->GetName(), cr_pid);
                        }

                        auto cr_props = Properties(cr_proto->GetProperties().GetRegistrator());
                        const auto props_data_size = reader.Read<uint32_t>();
                        props_data.resize(props_data_size);
                        reader.ReadPtr<uint8_t>(props_data.data(), props_data_size);
                        cr_props.RestoreAllData(props_data);

                        auto cr = SafeAlloc::MakeRefCounted<Critter>(_engine.get(), ident_t {}, cr_proto, &cr_props);
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
                        const auto* item_proto = _engine->GetProtoItem(item_pid);

                        if (item_proto == nullptr) {
                            throw MapManagerException("Item proto not found on map", map_proto->GetName(), item_pid);
                        }

                        auto item_props = Properties(item_proto->GetProperties().GetRegistrator());
                        const auto props_data_size = reader.Read<uint32_t>();
                        props_data.resize(props_data_size);
                        reader.ReadPtr<uint8_t>(props_data.data(), props_data_size);
                        item_props.RestoreAllData(props_data);

                        auto item = SafeAlloc::MakeRefCounted<StaticItem>(_engine.get(), ident_t {}, item_proto, &item_props);
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
                            static_map->StaticItems.emplace_back(item.get());
                            static_map->StaticItemsById.emplace(item_id, item.get());

                            const auto add_item_to_field = [item_ = item.get()](StaticMap::Field& static_field) {
                                if (!vec_exists(static_field.StaticItems, item_)) {
                                    static_field.StaticItems.reserve(static_field.StaticItems.size() + 1);
                                    static_field.StaticItems.emplace_back(item_);

                                    if (item_->GetIsTrigger()) {
                                        static_field.TriggerItems.reserve(static_field.TriggerItems.size() + 1);
                                        static_field.TriggerItems.emplace_back(item_);
                                    }

                                    if (!item_->GetNoBlock()) {
                                        static_field.MoveBlocked = true;
                                    }
                                    if (!item_->GetShootThru()) {
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
                                FO_VERIFY_AND_THROW(item->GetOwnership() == ItemOwnership::CritterInventory || item->GetOwnership() == ItemOwnership::ItemContainer, "Map item load produced item with unsupported ownership");
                                static_map->ChildItemBillets.emplace_back(item_id, item.get());
                            }
                        }
                    }
                }
            }

            reader.VerifyEnd();

            // Scroll blocks
            const irect32 scroll_area = map_proto->GetScrollAxialArea();
            const int32_t scroll_block_size = _engine->Settings.ScrollBlockSize;

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
    FO_VERIFY_AND_THROW(it != _staticMaps.end(), "Lookup failed in static maps");
    return it->second.get();
}

void MapManager::GenerateMapContent(Map* map)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(map, "Missing map instance");
    FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Map is already destroyed");

    unordered_map<ident_t, ident_t> id_map;

    // Generate critters
    for (auto&& [base_cr_id, base_cr] : map->GetStaticMap()->CritterBillets) {
        const auto* cr = _engine->CrMngr.CreateCritterOnMap(base_cr->GetProtoId(), &base_cr->GetProperties(), map, base_cr->GetHex(), base_cr->GetDir());
        id_map.emplace(base_cr_id, cr->GetId());
        FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Map is already destroyed");
    }

    // Generate hex items
    for (auto&& [base_item_id, base_item] : map->GetStaticMap()->HexItemBillets) {
        auto* item = _engine->ItemMngr.CreateItem(base_item->GetProtoId(), 0, &base_item->GetProperties());
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
        auto* item = _engine->ItemMngr.CreateItem(base_item->GetProtoId(), 0, &base_item->GetProperties());
        FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Map is already destroyed");

        // Add to parent
        if (base_item->GetOwnership() == ItemOwnership::CritterInventory) {
            auto* cr_cont = map->GetCritter(owner_id);
            FO_VERIFY_AND_THROW(cr_cont, "Missing required critter cont");

            _engine->CrMngr.AddItemToCritter(cr_cont, item, false);
            FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Map is already destroyed");
        }
        else if (base_item->GetOwnership() == ItemOwnership::ItemContainer) {
            auto* item_cont = map->GetItem(owner_id);
            FO_VERIFY_AND_THROW(item_cont, "Missing required item cont");

            item_cont->AddItemToContainer(item, {});
            FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Map is already destroyed");
        }
        else {
            FO_UNREACHABLE_PLACE();
        }
    }

    // Process visibility
    for (auto* cr : copy_hold_ref(map->GetCritters())) {
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

void MapManager::DestroyMapContent(Map* map)
{
    FO_STACK_TRACE_ENTRY();

    // Each child critter / item has its own EntityLock; the parent walk doesn't reach the map's
    // (or location's) lock once the child is detached during destruction. Pull each into the
    // current SyncContext before handing it to the destruction path so its validator passes.
    auto* ctx = _engine->GetCurrentSyncContext();
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");

    for (InfinityLoopDetector detector; map->HasCritters() || map->HasItems(); detector.AddLoop()) {
        for (auto* cr : copy_hold_ref(map->GetPlayerCritters())) {
            ctx->EnsureEntitySynced(cr);
            TransferToGlobal(cr, {});
        }
        for (auto* cr : copy_hold_ref(map->GetNonPlayerCritters())) {
            ctx->EnsureEntitySynced(cr);
            _engine->CrMngr.DestroyCritter(cr);
        }
        for (auto* item : copy_hold_ref(map->GetItems())) {
            ctx->EnsureEntitySynced(item);
            _engine->ItemMngr.DestroyItem(item);
        }
    }
}

auto MapManager::CreateLocation(hstring proto_id, const_span<hstring> map_pids, const Properties* props) -> Location*
{
    FO_STACK_TRACE_ENTRY();

    const auto* proto = _engine->GetProtoLocation(proto_id);

    if (proto == nullptr) {
        throw GenericException("Location proto not found", proto_id);
    }

    auto loc = SafeAlloc::MakeRefCounted<Location>(_engine.get(), ident_t {}, proto, props);

    _engine->EntityMngr.RegisterLocation(loc.get());

    auto* ctx = _engine->GetCurrentSyncContext();
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");

    bool release_empty_sync_context = false;

    if (ctx->IsEmpty()) {
        ctx->SyncEntity(loc.get());
        release_empty_sync_context = true;
    }
    else {
        ctx->EnsureEntitySynced(loc.get());
    }

    auto release_empty_sync = scope_exit([ctx, release_empty_sync_context]() noexcept {
        if (release_empty_sync_context) {
            ctx->Release();
        }
    });

    for (const auto map_pid : map_pids) {
        const auto* map_proto = _engine->GetProtoMap(map_pid);

        if (map_proto == nullptr) {
            throw GenericException("Map proto not found", map_pid);
        }

        auto* static_map = GetStaticMap(map_proto);
        auto map = SafeAlloc::MakeRefCounted<Map>(_engine.get(), ident_t {}, map_proto, loc.get(), static_map);
        _engine->EntityMngr.RegisterMap(map.get());
        loc->AddMap(map.get());
        GenerateMapContent(map.get());
    }

    for (auto* map : copy_hold_ref(loc->GetMaps())) {
        _engine->EntityMngr.CallInit(map, true);
    }

    if (!loc->IsDestroyed()) {
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

    ValidateEntityAccess(loc);
    FO_VERIFY_AND_THROW(loc, "Missing location instance");
    FO_VERIFY_AND_THROW(!loc->IsDestroyed(), "Location is already destroyed");
    FO_VERIFY_AND_THROW(!loc->IsDestroying(), "Location is already being destroyed");

    const auto* map_proto = _engine->GetProtoMap(proto_id);

    if (map_proto == nullptr) {
        throw GenericException("Map proto not found", proto_id);
    }

    auto* static_map = GetStaticMap(map_proto);
    auto map = SafeAlloc::MakeRefCounted<Map>(_engine.get(), ident_t {}, map_proto, loc, static_map);

    _engine->EntityMngr.RegisterMap(map.get());
    loc->AddMap(map.get());

    GenerateMapContent(map.get());

    if (!map->IsDestroyed()) {
        _engine->EntityMngr.CallInit(map.get(), true);
    }

    if (!map->IsDestroyed()) {
        ValidateEntityAccess(loc);
        ValidateEntityAccess(map.get());
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

    ValidateEntityAccess(map);
    FO_VERIFY_AND_THROW(map, "Missing map instance");
    FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Map is already destroyed");

    refcount_ptr map_holder = map;

    DestroyMapContent(map);

    if (!map->IsDestroyed()) {
        GenerateMapContent(map);
    }
}

auto MapManager::GetLocationByPid(hstring loc_pid, int32_t skip_count) noexcept -> refcount_ptr<Location>
{
    FO_STACK_TRACE_ENTRY();

    for (auto& loc : _engine->EntityMngr.GetLocations()) {
        if (loc->GetProtoId() == loc_pid) {
            if (skip_count <= 0) {
                return loc.get();
            }

            skip_count--;
        }
    }

    return nullptr;
}

auto MapManager::GetMapByPid(hstring map_pid, int32_t skip_count) noexcept -> refcount_ptr<Map>
{
    FO_STACK_TRACE_ENTRY();

    for (auto& map : _engine->EntityMngr.GetMaps()) {
        if (map->GetProtoId() == map_pid) {
            if (skip_count <= 0) {
                return map.get();
            }

            skip_count--;
        }
    }

    return nullptr;
}

void MapManager::DestroyLocation(Location* loc)
{
    FO_STACK_TRACE_ENTRY();

    ValidateEntityAccess(loc);

    if (loc->IsDestroying() || loc->IsDestroyed()) {
        return;
    }

    for (auto* map : loc->GetMaps()) {
        ValidateEntityAccess(map);
        FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Map is already destroyed");
        FO_VERIFY_AND_THROW(!map->IsDestroying(), "Map is already being destroyed");
    }

    // Destroy location in couple with maps
    loc->MarkAsDestroying();

    for (auto* map : loc->GetMaps()) {
        map->MarkAsDestroying();
    }

    ValidateEntityAccess(loc);
    _engine->OnLocationFinish.Fire(loc);
    FO_VERIFY_AND_THROW(!loc->IsDestroyed(), "Location is already destroyed");

    for (auto* map : loc->GetMaps()) {
        ValidateEntityAccess(map);
        _engine->OnMapFinish.Fire(map);
        FO_VERIFY_AND_THROW(!loc->IsDestroyed(), "Location is already destroyed");
        FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Map is already destroyed");
        FO_VERIFY_AND_THROW(map->GetLocation() == loc, "Map is attached to a different location during location destruction", map->GetId(), loc->GetId(), map->GetLocation() != nullptr ? map->GetLocation()->GetId() : ident_t {});
    }

    for (auto* map : loc->GetMaps()) {
        auto* ctx = _engine->GetCurrentSyncContext();
        FO_VERIFY_AND_THROW(ctx, "Missing script execution context");
        ctx->EnsureEntitySynced(map);

        DestroyMapInternal(map);
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

    _engine->TimeEventMngr.CancelAllForEntity(loc);
    loc->SetParent(nullptr);
    loc->MarkAsDestroyed();
    _engine->EntityMngr.UnregisterLocation(loc);
}

void MapManager::DestroyMap(Map* map)
{
    FO_STACK_TRACE_ENTRY();

    ValidateEntityAccess(map);
    refcount_ptr map_holder = map;
    ignore_unused(map_holder);

    if (map->IsDestroying() || map->IsDestroyed()) {
        return;
    }

    map->MarkAsDestroying();
    ValidateEntityAccess(map);
    _engine->OnMapFinish.Fire(map);
    FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Map is already destroyed");

    refcount_ptr loc = map->GetLocation();
    ValidateEntityAccess(loc.get());
    ValidateEntityAccess(map);
    loc->OnMapRemoved.Fire(map);
    FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Map is already destroyed");
    FO_VERIFY_AND_THROW(!loc->IsDestroyed(), "Location is already destroyed");
    FO_VERIFY_AND_THROW(map->GetLocation() == loc.get(), "Map is attached to a different location during map destruction", map->GetId(), loc->GetId(), map->GetLocation() != nullptr ? map->GetLocation()->GetId() : ident_t {});

    DestroyMapInternal(map);
}

void MapManager::DestroyMapInternal(Map* map)
{
    FO_STACK_TRACE_ENTRY();

    ValidateEntityAccess(map);

    // Eject spectators before destroying map content so each spectator gets a clean LoadMap(nullptr) and clears its ViewMap.
    while (map->HasSpectatorPlayers()) {
        auto* player = map->GetSpectatorPlayers().back().get();
        player->ResetViewMap();
        player->Send_LoadMap(nullptr);
    }

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

    _engine->TimeEventMngr.CancelAllForEntity(map);

    auto loc = map->GetLocation();
    FO_VERIFY_AND_THROW(loc, "Missing location instance");
    ValidateEntityAccess(loc);
    loc->RemoveMap(map);

    map->SetParent(nullptr);
    map->MarkAsDestroyed();
    _engine->EntityMngr.UnregisterMap(map);
}

auto MapManager::TracePath(const Map* map, mpos start_hex, mpos target_hex, int32_t max_dist, float32_t angle, const Critter* find_cr, CritterFindType find_type, bool check_last_movable, bool collect_critters) const -> TraceResult
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

            for (const auto* cr : critters) {
                if (std::ranges::find(output.Critters, cr) == output.Critters.end()) {
                    output.Critters.emplace_back(cr);
                }
            }
        }

        if (find_cr != nullptr && map->IsCritterOnHex(next_hex, find_cr)) {
            output.IsCritterFound = true;
            break;
        }

        prev_hex = next_hex;
    }

    output.PreBlock = prev_hex;
    output.Block = next_hex;

    return output;
}

auto MapManager::FindPath(const Map* map, const Critter* from_cr, mpos from_hex, mpos to_hex, int32_t multihex, int32_t cut, ipos16 to_hex_offset, function<bool(const Item*)> gag_callback) const -> FindPathOutput
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(map, "Missing map instance");
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
    settings.FromHexOffset = from_cr != nullptr ? from_cr->GetHexOffset() : ipos16 {};
    settings.ToHex = to_hex;
    settings.ToHexOffset = to_hex_offset;
    settings.MapSize = map->GetSize();
    settings.MaxLength = _engine->Settings.MaxPathFindLength;
    settings.Cut = cut;
    settings.Multihex = multihex;
    settings.FreeMovement = _engine->Settings.MapFreeMovement;

    settings.CheckHex = [&](mpos hex) -> HexBlockResult {
        if (!map->IsHexMovable(hex)) {
            if (map->CheckGagItem(hex, gag_callback)) {
                return HexBlockResult::DeferGag;
            }

            return HexBlockResult::Blocked;
        }

        if (map->HasLivingCritter(hex, from_cr)) {
            return HexBlockResult::DeferCritter;
        }

        return HexBlockResult::Passable;
    };

    return PathFinding::FindPath(settings);
}

void MapManager::TransferToMap(Critter* cr, Map* map, mpos hex, mdir dir, optional<int32_t> safe_radius)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(map, "Missing map instance");

    Transfer(cr, map, hex, dir, safe_radius, {});
}

void MapManager::TransferToGlobal(Critter* cr, ident_t global_cr_id)
{
    FO_STACK_TRACE_ENTRY();

    Transfer(cr, nullptr, {}, mdir {}, std::nullopt, global_cr_id);
}

void MapManager::Transfer(Critter* cr, Map* map, mpos hex, mdir dir, optional<int32_t> safe_radius, ident_t global_cr_id)
{
    FO_STACK_TRACE_ENTRY();

    auto* sync_ctx = _engine->GetCurrentSyncContext();
    FO_VERIFY_AND_THROW(sync_ctx, "Missing required sync context");

    bool release_empty_sync_context = false;

    if (sync_ctx->IsEmpty()) {
        ServerEntity* sync_entities[] = {cr, map};
        sync_ctx->SyncEntities(span<ServerEntity*> {sync_entities});
        release_empty_sync_context = true;
    }

    sync_ctx->EnsureEntitySynced(cr);
    sync_ctx->EnsureEntitySynced(map);

    auto release_empty_sync = scope_exit([sync_ctx, release_empty_sync_context]() noexcept {
        if (release_empty_sync_context) {
            sync_ctx->Release();
        }
    });

    ValidateEntityAccess(cr);
    ValidateEntityAccess(map);

    if (map != nullptr) {
        FO_VERIFY_AND_THROW(map->GetSize().is_valid_pos(hex), "Critter transfer target hex is outside target map bounds", cr->GetId(), map->GetId(), hex, map->GetSize());
    }

    if (cr->IsMapTransfersLocked()) {
        throw GenericException("Critter transfers locked");
    }

    cr->LockMapTransfers();
    auto restore_transfers = scope_exit([cr]() noexcept { cr->UnlockMapTransfers(); });

    const auto prev_map_id = cr->GetMapId();
    auto prev_map_ref = prev_map_id ? cr->GetParent<Map>() : refcount_ptr<Map> {};
    Map* prev_map = prev_map_ref.get();
    FO_VERIFY_AND_THROW(!prev_map_id || !!prev_map, "Previous map id is set but previous map was not found");

    sync_ctx->EnsureEntitySynced(prev_map);
    ValidateEntityAccess(prev_map);

    cr->StopMoving();

    if (cr->GetIsAttached()) {
        cr->DetachFromCritter();
    }

    vector<raw_ptr<Critter>> attached_critters;

    if (cr->HasAttachedCritters()) {
        const auto attached_span = cr->GetAttachedCritters();
        attached_critters.assign(attached_span.begin(), attached_span.end());

        for (auto& attached_cr : attached_critters) {
            attached_cr->DetachFromCritter();
        }
    }

    if (map == prev_map) {
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

        if (cr->IsDestroyed()) {
            return;
        }

        AddCritterToMap(cr, map, start_hex, dir, global_cr_id);

        if (cr->IsDestroyed()) {
            return;
        }
        if (map != nullptr && map->IsDestroyed()) {
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

        if (map != nullptr) {
            ValidateEntityAccess(map);
            ValidateEntityAccess(cr);
            _engine->OnMapCritterIn.Fire(map, cr);

            if (cr->IsDestroyed() || map->IsDestroyed()) {
                return;
            }
            if (cr->GetMapId() != map->GetId()) {
                return;
            }

            FO_VERIFY_AND_THROW(map->GetCritter(cr->GetId()) == cr, "Critter is not registered in the target map after map-enter event", map->GetId(), cr->GetId(), cr->GetMapId());
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

            FO_VERIFY_AND_THROW(cr->GetRawGlobalMapGroup(), "Critter has no global map group");
        }

        ProcessVisibleCritters(cr);
        ProcessVisibleItems(cr);

        if (cr->IsDestroyed()) {
            return;
        }

        ValidateEntityAccess(cr);
        _engine->OnCritterSendInitialInfo.Fire(cr);

        if (cr->IsDestroyed()) {
            return;
        }

        if (map != nullptr) {
            if (map->IsDestroyed() || cr->GetMapId() != map->GetId() || map->GetCritter(cr->GetId()) != cr) {
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
                Transfer(attached_cr.get(), map, cr->GetHex(), dir, std::nullopt, cr->GetId());
            }
        }

        if (!cr->IsDestroyed() && !cr->GetIsAttached()) {
            for (auto& attached_cr : attached_critters) {
                if (!attached_cr->IsDestroyed() && !attached_cr->GetIsAttached() && !attached_cr->HasAttachedCritters() && attached_cr->GetMapId() == cr->GetMapId()) {
                    attached_cr->AttachToCritter(cr);
                }
            }

            if (!cr->IsDestroyed()) {
                cr->MoveAttachedCritters();
                ProcessVisibleCritters(cr);
            }
        }
    }

    // ValidateEntityAccess and event dispatch tolerate destroyed entity arguments.
    ValidateEntityAccess(cr);
    ValidateEntityAccess(prev_map);
    _engine->OnCritterTransfer.Fire(cr, prev_map);
}

void MapManager::AddCritterToMap(Critter* cr, Map* map, mpos hex, mdir dir, ident_t global_cr_id)
{
    FO_STACK_TRACE_ENTRY();

    ValidateEntityAccess(cr);
    ValidateEntityAccess(map);

    FO_VERIFY_AND_THROW(!cr->IsDestroyed(), "Critter is already destroyed");
    cr->LockMapTransfers();
    auto restore_transfers = scope_exit([cr]() noexcept { cr->UnlockMapTransfers(); });

    if (map != nullptr) {
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
            for (auto* player : copy_hold_ref(map->GetSpectatorPlayers())) {
                player->Send_AddCritter(cr);
            }
        }
    }
    else {
        auto& cr_group = cr->GetRawGlobalMapGroup();
        FO_VERIFY_AND_THROW(!cr_group, "Critter group is already set");

        auto global_cr_ref = global_cr_id && global_cr_id != cr->GetId() ? _engine->EntityMngr.GetCritter(global_cr_id) : refcount_ptr<Critter> {};
        Critter* global_cr = global_cr_ref.get();

        cr->SetMapId({});
        cr->SetParent(nullptr);

        if (global_cr == nullptr || global_cr->GetMapId()) {
            _engine->LockForPropertyAccess();
            const auto trip_id = _engine->GetLastGlobalMapTripId() + 1;
            _engine->SetLastGlobalMapTripId(trip_id);
            _engine->UnlockForPropertyAccess();

            cr->SetGlobalMapTripId(trip_id);

            cr_group = SafeAlloc::MakeShared<vector<raw_ptr<Critter>>>();
        }
        else {
            auto& global_cr_group = global_cr->GetRawGlobalMapGroup();
            FO_VERIFY_AND_THROW(global_cr_group, "Missing required global critter group");

            cr->SetGlobalMapTripId(global_cr->GetGlobalMapTripId());

            for (auto& group_cr : *global_cr_group) {
                group_cr->Send_AddCritter(cr);
            }

            cr_group = global_cr_group;
        }

        vec_add_unique_value(*cr_group, cr);
    }
}

void MapManager::RemoveCritterFromMap(Critter* cr, Map* map)
{
    FO_STACK_TRACE_ENTRY();

    ValidateEntityAccess(cr);
    ValidateEntityAccess(map);

    FO_VERIFY_AND_THROW(!cr->IsDestroyed(), "Critter is already destroyed");
    refcount_ptr cr_holder = cr;
    ignore_unused(cr_holder);
    cr->LockMapTransfers();
    auto restore_transfers = scope_exit([cr]() noexcept { cr->UnlockMapTransfers(); });

    if (map != nullptr) {
        FO_VERIFY_AND_THROW(cr->GetMapId() == map->GetId(), "Critter belongs to a different map");
        refcount_ptr map_holder = map;
        ignore_unused(map_holder);
        ValidateEntityAccess(map);
        ValidateEntityAccess(cr);
        _engine->OnMapCritterOut.Fire(map, cr);

        if (cr->IsDestroyed() || map->IsDestroyed()) {
            return;
        }

        FO_VERIFY_AND_THROW(cr->GetMapId() == map->GetId(), "Critter belongs to a different map");
        FO_VERIFY_AND_THROW(map->GetCritter(cr->GetId()) == cr, "Critter is not registered in its map before map-exit notification", map->GetId(), cr->GetId(), cr->GetMapId());

        for (auto* other : copy_hold_ref(cr->GetCritters(CritterSeeType::WhoSeeMe, CritterFindType::Any))) {
            if (other->IsDestroyed()) {
                continue;
            }

            ValidateEntityAccess(other);
            ValidateEntityAccess(cr);
            other->OnCritterDisappeared.Fire(cr);

            if (cr->IsDestroyed() || map->IsDestroyed()) {
                return;
            }

            FO_VERIFY_AND_THROW(cr->GetMapId() == map->GetId(), "Critter belongs to a different map");
            FO_VERIFY_AND_THROW(map->GetCritter(cr->GetId()) == cr, "Critter is not registered in its map after disappearance notification", map->GetId(), cr->GetId(), cr->GetMapId());
        }

        cr->ClearVisibleEnitites();

        // Notify spectators
        if (map->HasSpectatorPlayers()) {
            for (auto* player : copy_hold_ref(map->GetSpectatorPlayers())) {
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

        FO_VERIFY_AND_THROW(!cr->GetMapId(), "Critter still has a map id before global map insertion");
        FO_VERIFY_AND_THROW(cr->GetRawGlobalMapGroup(), "Critter has no global map group");

        cr->SetGlobalMapTripId({});
        auto& cr_group = cr->GetRawGlobalMapGroup();
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

    ValidateEntityAccess(cr);

    if (cr->IsDestroyed()) {
        return;
    }

    if (cr->GetMapId()) {
        auto map = cr->GetParent<Map>();
        FO_VERIFY_AND_THROW(map, "Missing map instance");
        ValidateEntityAccess(map.get());

        for (auto* target : copy_hold_ref(map->GetCritters())) {
            ProcessCritterLook(map.get(), cr, target);
            ProcessCritterLook(map.get(), target, cr);
        }
    }
    else {
        FO_VERIFY_AND_THROW(cr->GetRawGlobalMapGroup(), "Critter has no global map group");
    }
}

void MapManager::ProcessCritterLook(Map* map, Critter* cr, Critter* target)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    FO_VERIFY_AND_THROW(map && cr && target, "Map transfer is missing map, critter, or target hex");
    ValidateEntityAccess(map);
    ValidateEntityAccess(cr);
    ValidateEntityAccess(target);
    refcount_ptr map_holder = map;
    refcount_ptr cr_holder = cr;
    refcount_ptr target_holder = target;
    ignore_unused(map_holder);
    ignore_unused(cr_holder);
    ignore_unused(target_holder);
    const auto is_look_context_changed = [map, cr, target]() noexcept -> bool {
        FO_NO_STACK_TRACE_ENTRY();

        if (map->IsDestroyed() || cr->IsDestroyed() || target->IsDestroyed()) {
            return true;
        }
        if (cr->GetMapId() != map->GetId()) {
            return true;
        }
        if (target->GetMapId() != map->GetId()) {
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

    auto vis_mode = IsCritterSeeCritter(map, cr, target);

    if (vis_mode == CritterVisibilityMode::None && target->HasAttachedCritters()) {
        for (auto& attached_cr : target->GetAttachedCritters()) {
            FO_VERIFY_AND_THROW(attached_cr->GetMapId() == map->GetId(), "Attached critter is on a different map than its visibility target", target->GetId(), attached_cr->GetId(), attached_cr->GetMapId(), map->GetId());

            const auto attached_mode = IsCritterSeeCritter(map, cr, attached_cr.get());

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

auto MapManager::IsCritterSeeCritter(const Map* map, const Critter* cr, const Critter* target) const -> CritterVisibilityMode
{
    FO_STACK_TRACE_ENTRY();

    ValidateEntityAccess(map);
    ValidateEntityAccess(cr);
    ValidateEntityAccess(target);
    FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Map is already destroyed");
    FO_VERIFY_AND_THROW(!cr->IsDestroyed(), "Critter is already destroyed");
    FO_VERIFY_AND_THROW(!target->IsDestroyed(), "Map transfer target is already destroyed");

    return CheckCritterVisibilityHook(_engine.get(), map, cr, target);
}

void MapManager::ProcessVisibleItems(Critter* cr)
{
    FO_STACK_TRACE_ENTRY();

    ValidateEntityAccess(cr);

    if (cr->IsDestroyed()) {
        return;
    }

    if (!cr->GetMapId()) {
        return;
    }

    auto map = cr->GetParent<Map>();
    FO_VERIFY_AND_THROW(map, "Missing map instance");
    ValidateEntityAccess(map.get());

    for (auto* item : copy_hold_ref(map->GetItems())) {
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

void MapManager::ViewMap(Player* view_player, Map* map)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(view_player, "Missing required view player");
    FO_VERIFY_AND_THROW(map, "Missing map instance");

    ValidateEntityAccess(view_player);
    ValidateEntityAccess(map);

    // Critters: spectator sees every non-destroyed critter on the map
    for (const auto* cr : copy_hold_ref(map->GetCritters())) {
        if (cr->IsDestroyed()) {
            continue;
        }

        view_player->Send_AddCritter(cr);
    }

    // Items: spectator sees every non-destroyed item on the map
    for (const auto* item : copy_hold_ref(map->GetItems())) {
        if (item->IsDestroyed()) {
            continue;
        }

        view_player->Send_AddItemOnMap(item);
    }
}

FO_END_NAMESPACE
