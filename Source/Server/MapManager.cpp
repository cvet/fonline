// FOnline Engine
// SPDX-License-Identifier: MIT

#include "MapManager.h"

#include "CritterManager.h"
#include "EntityManager.h"
#include "ItemManager.h"
#include "LineTracer.h"
#include "MapLoader.h"
#include "Player.h"
#include "ProtoManager.h"
#include "Server.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

CritterVisibilityMode CheckCritterVisibilityHook(ptr<const ServerEngine>, ptr<const Map>, ptr<const Critter>, ptr<const Critter>);

MapManager::MapManager(ptr<ServerEngine> engine) :
    _engine {engine}
{
}

void MapManager::LoadFromResources()
{
    FO_TRACE_ZONE(Map);

    for (const auto& [setting_name, grid_type] : array<pair<string_view, string_view>, 2> {{{SettingNames::Server_ProtoMapGridType, _engine->Settings->Server.ProtoMapGridType}, {SettingNames::Server_MapInstanceGridType, _engine->Settings->Server.MapInstanceGridType}}}) {
        if (grid_type != "Static" && grid_type != "Chunked" && grid_type != "Dynamic") {
            throw SettingsException("Unknown server map grid type; expected Static, Chunked or Dynamic", setting_name, grid_type);
        }
    }

    // Baked map content is read when a map first needs it (GetStaticMap), so unused proto maps cost no memory;
    // startup only checks that every baked map has its prototype
    for (const auto& map_file_header : _engine->Resources.FilterFiles("fomap-bin-server")) {
        hstring map_pid {_engine->Hashes.to_hashed_string(map_file_header.GetNameNoExt())};

        if (!_engine->GetProtoMap(map_pid)) {
            throw MapManagerException("Map proto not found for static map", map_pid);
        }
    }
}

void MapManager::BindStaticItemScripts(string_view map_name, ptr<StaticItem> item)
{
    if (auto static_script {item->GetStaticScript()}) {
        item->StaticScriptFunc = _engine->FindFunc<bool, ptr<Critter>, ptr<StaticItem>, nptr<Item>, any_t>(static_script);

        if (!item->StaticScriptFunc) {
            throw MapManagerException("Can't bind static item function", map_name, static_script);
        }
        if (!item->StaticScriptFunc.HasAttribute("ItemStatic")) {
            throw MapManagerException("Static item function missing [[ItemStatic]] attribute", map_name, static_script);
        }
    }

    if (auto trigger_script {item->GetTriggerScript()}) {
        item->TriggerScriptFunc = _engine->FindFunc<void, ptr<Critter>, ptr<StaticItem>, bool, mdir>(trigger_script);

        if (!item->TriggerScriptFunc) {
            throw MapManagerException("Can't bind static item trigger function", map_name, trigger_script);
        }
        if (!item->TriggerScriptFunc.HasAttribute("ItemTrigger")) {
            throw MapManagerException("Static item trigger function missing [[ItemTrigger]] attribute", map_name, trigger_script);
        }
    }
}

auto MapManager::ReadStaticMap(ptr<const ProtoMap> map_proto) -> shared_ptr<StaticMap>
{
    FO_TRACE_ZONE_NAMED(Map, "MapManager::LoadStaticMap");

    // The billets are entities without cover; a thread outside server logic gets a context for creating them
    optional<ScopedSyncContext> sync_ctx {};

    if (!SyncContext::GetCurrentOnThisThread()) {
        sync_ctx.emplace();
    }

    auto map_file {_engine->Resources.ReadFile(strex("{}.fomap-bin-server", map_proto->GetName()))};

    if (!map_file) {
        throw MapManagerException("Map file not found", map_proto->GetName());
    }

    data_reader reader {data_reader(map_file.GetDataSpan())};

    MapLoader::ReadBakedFileHeader(reader, map_proto->GetName());

    auto map_size {map_proto->GetSize()};
    FO_VERIFY_AND_THROW(map_size.width > 0 && map_size.height > 0, "Map size must be positive", map_proto->GetName(), map_size);
    FO_VERIFY_AND_THROW(map_size.width <= GameSettings::MAX_MAP_SIZE && map_size.height <= GameSettings::MAX_MAP_SIZE, "Map size exceeds the engine maximum", map_proto->GetName(), map_size, GameSettings::MAX_MAP_SIZE);

    auto static_map {safe_alloc::make_shared<StaticMap>(map_size, _engine->Settings->Server.ProtoMapGridType)};

    // Read hashes
    {
        auto hashes_count {reader.read<uint32_t>()};

        // Counts and sizes come from a resource file that may be stale or damaged, so every one of them
        // is preflighted against the buffer before it drives an allocation or a loop
        reader.verify_payload_count(hashes_count, sizeof(uint32_t));

        for (uint32_t i {0}; i < hashes_count; i++) {
            string str {reader.read_string()};
            hstring hstr {_engine->Hashes.to_hashed_string(str)};
            ignore_unused(hstr);
        }
    }

    // Read entities
    {
        vector<uint8_t> props_data {};

        // Read critters
        {
            auto cr_count {reader.read<uint32_t>()};

            reader.verify_payload_count(cr_count, sizeof(ident_t::underlying_type) + sizeof(hstring::hash_t) + sizeof(uint32_t));

            static_map->ReserveCritters(cr_count);

            for (uint32_t i {0}; i < cr_count; i++) {
                ident_t cr_id {ident_t {reader.read<ident_t::underlying_type>()}};

                auto cr_pid_hash {reader.read<hstring::hash_t>()};
                hstring cr_pid {_engine->Hashes.resolve_hash(cr_pid_hash)};
                auto cr_proto {_engine->GetProtoCritter(cr_pid)};

                if (!cr_proto) {
                    throw MapManagerException("Critter proto not found on map", map_proto->GetName(), cr_pid);
                }

                auto cr_props {Properties(cr_proto->GetProperties()->GetRegistrar())};
                auto props_data_size {reader.read<uint32_t>()};
                reader.verify_payload_count(props_data_size, sizeof(uint8_t));
                props_data.resize(props_data_size);
                span<uint8_t> props_data_span {props_data};
                reader.read_bytes(props_data_span);
                cr_props.RestoreAllData(props_data);

                auto cr_props_ptr {make_nptr(&cr_props)};
                auto cr {safe_alloc::make_refcounted<Critter>(_engine, ident_t {}, cr_proto, cr_props_ptr)};
                cr->SetEntityLock(nullptr);

                static_map->AddCritterBillet(cr_id, cr);

                // Checks
                if (auto hex {cr->GetHex()}; !map_size.is_valid_pos(hex)) {
                    throw MapManagerException("Invalid critter position on map", map_proto->GetName(), cr->GetName(), hex);
                }
            }
        }

        // Read items
        {
            auto item_count {reader.read<uint32_t>()};

            reader.verify_payload_count(item_count, sizeof(ident_t::underlying_type) + sizeof(hstring::hash_t) + sizeof(uint32_t));

            static_map->ReserveItems(item_count);

            for (uint32_t i {0}; i < item_count; i++) {
                ident_t item_id {ident_t {reader.read<ident_t::underlying_type>()}};

                auto item_pid_hash {reader.read<hstring::hash_t>()};
                hstring item_pid {_engine->Hashes.resolve_hash(item_pid_hash)};
                auto item_proto {_engine->GetProtoItem(item_pid)};

                if (!item_proto) {
                    throw MapManagerException("Item proto not found on map", map_proto->GetName(), item_pid);
                }

                auto item_props {Properties(item_proto->GetProperties()->GetRegistrar())};
                auto props_data_size {reader.read<uint32_t>()};
                reader.verify_payload_count(props_data_size, sizeof(uint8_t));
                props_data.resize(props_data_size);
                span<uint8_t> props_data_span {props_data};
                reader.read_bytes(props_data_span);
                item_props.RestoreAllData(props_data);

                auto item_props_ptr {make_nptr(&item_props)};
                auto item {safe_alloc::make_refcounted<StaticItem>(_engine, item_id, item_proto, item_props_ptr)};
                item->SetEntityLock(nullptr);
                static_map->AddOwnedItemBillet(item_id, item);

                // Checks
                if (item->GetOwnership() == ItemOwnership::MapHex) {
                    if (auto hex {item->GetHex()}; !map_size.is_valid_pos(hex)) {
                        throw MapManagerException("Invalid item position on map", map_proto->GetName(), item->GetName(), hex);
                    }
                }

                BindStaticItemScripts(map_proto->GetName(), item);

                // Sort
                if (item->GetStatic()) {
                    FO_VERIFY_AND_THROW(item->GetOwnership() == ItemOwnership::MapHex, "Item is not placed on map hex");
                    static_map->AddStaticItem(item_id, item);
                }
                else {
                    if (item->GetOwnership() == ItemOwnership::MapHex) {
                        static_map->AddHexItemBillet(item_id, item);
                    }
                    else {
                        FO_VERIFY_AND_THROW(item->GetOwnership() == ItemOwnership::CritterInventory || item->GetOwnership() == ItemOwnership::ItemContainer, "Map item load produced item with unsupported ownership");
                        static_map->AddChildItemBillet(item_id, item);
                    }
                }
            }
        }
    }

    reader.verify_end();

    // Scroll blocks
    irect32 scroll_area {map_proto->GetScrollAxialArea()};
    int32_t scroll_block_size {_engine->Settings->Hex.ScrollBlockSize};

    if (!scroll_area.is_zero()) {
        for (mcoord_t hx {0}; hx < map_size.width; hx++) {
            for (mcoord_t hy {0}; hy < map_size.height; hy++) {
                mpos hex {hx, hy};
                ipos32 axial_hex {GeometryHelper::GetHexAxialCoord(hex)};

                // Is hex on scroll block line
                if ((axial_hex.x >= scroll_area.x - scroll_block_size && axial_hex.x <= scroll_area.x + scroll_block_size) || //
                    (axial_hex.x >= scroll_area.x + scroll_area.width - scroll_block_size && axial_hex.x <= scroll_area.x + scroll_area.width + scroll_block_size) || //
                    (axial_hex.y >= scroll_area.y - scroll_block_size && axial_hex.y <= scroll_area.y + scroll_block_size) || //
                    (axial_hex.y >= scroll_area.y + scroll_area.height - scroll_block_size && axial_hex.y <= scroll_area.y + scroll_area.height + scroll_block_size)) {
                    static_map->MarkScrollBlocked(hex);
                }
            }
        }
    }

    static_map->ShrinkToFit();

    return static_map;
}

auto MapManager::IsStaticMapReady(const PendingStaticMap& static_map) const -> bool
{
    return static_map.wait_for(std::chrono::seconds {0}) == std::future_status::ready;
}

auto MapManager::RequestStaticMap(ptr<const ProtoMap> proto, bool in_background) -> PendingStaticMap
{
    auto read_promise {safe_alloc::make_shared<StaticMapRead>()};
    PendingStaticMap static_map {};

    {
        scoped_lock lock {_staticMapsLock};

        if (auto it {_staticMaps.find(proto)}; it != _staticMaps.end()) {
            return it->second.Data;
        }

        // The pending read is cached at once, so a concurrent request for the same map, from another logic thread or
        // a background load, waits for this read instead of reading the file again
        static_map = read_promise->get_future().share();
        _staticMaps.emplace(proto, StaticMapEntry {.Data = static_map, .Read = read_promise});
    }

    auto read {[this, proto, read_promise]() mutable FO_DEFERRED {
        try {
            read_promise->set_value(ReadStaticMap(proto));
        }
        catch (...) {
            // Dropped before the waiters see the error, so a later request reads the map again
            {
                scoped_lock lock {_staticMapsLock};

                if (auto it {_staticMaps.find(proto)}; it != _staticMaps.end() && it->second.Read == read_promise) {
                    _staticMaps.erase(it);
                }
            }

            read_promise->set_exception(std::current_exception());
        }
    }};

    if (in_background) {
        submit_async(strex("LoadStaticMap-{}", proto->GetName()), std::move(read));
    }
    else {
        read();
    }

    return static_map;
}

auto MapManager::AcquireStaticMap(ptr<const ProtoMap> proto) -> shared_ptr<StaticMap>
{
    return RequestStaticMap(proto, false).get();
}

auto MapManager::GetStaticMap(ptr<const ProtoMap> proto) -> ptr<StaticMap>
{
    return AcquireStaticMap(proto).get();
}

void MapManager::LoadStaticMap(ptr<const ProtoMap> proto, bool async_load)
{
    FO_TRACE_ZONE(Map);

    if (async_load) {
        (void)RequestStaticMap(proto, true);
    }
    else {
        (void)AcquireStaticMap(proto);
    }
}

void MapManager::LoadStaticMaps(const_span<ptr<const ProtoMap>> protos, bool async_load)
{
    FO_TRACE_ZONE(Map);

    for (ptr<const ProtoMap> proto : protos) {
        LoadStaticMap(proto, async_load);
    }
}

auto MapManager::UnloadStaticMap(ptr<const ProtoMap> proto) -> bool
{
    FO_TRACE_ZONE(Map);

    scoped_lock lock {_staticMapsLock};

    auto it {_staticMaps.find(proto)};

    if (it == _staticMaps.end()) {
        return true;
    }

    // A read in progress belongs to its waiters
    if (!IsStaticMapReady(it->second.Data)) {
        return false;
    }

    // A map instance reads its proto's content through a plain pointer for its whole life
    for (ptr<const Map> map : _engine->EntityMngr.GetMaps()) {
        if (map->GetProtoMap() == proto) {
            return false;
        }
    }

    _staticMaps.erase(it);
    return true;
}

auto MapManager::IsStaticMapLoaded(ptr<const ProtoMap> proto) const -> bool
{
    scoped_lock lock {_staticMapsLock};

    auto it {_staticMaps.find(proto)};

    // A failed read leaves the cache before it is ready, so a ready entry holds content
    return it != _staticMaps.end() && IsStaticMapReady(it->second.Data);
}

auto MapManager::LoadRuntimeStaticItems(ptr<Map> map, ptr<const ProtoMap> source_proto, mpos dest_hex, int64_t id_offset) -> vector<ident_t>
{
    return LoadRuntimeStaticItems(map, source_proto, dest_hex, mpos {}, source_proto->GetSize(), id_offset);
}

auto MapManager::LoadRuntimeStaticItems(ptr<Map> map, ptr<const ProtoMap> source_proto, mpos dest_hex, mpos source_hex, msize source_size, int64_t id_offset) -> vector<ident_t>
{
    FO_TRACE_ZONE(Map);

    ValidateEntityAccess(map);
    FO_VERIFY_AND_THROW(!map->IsDestroyed() && !map->IsDestroying(), "Cannot load static items into a map that is being destroyed", map->GetId());

    msize source_map_size {source_proto->GetSize()};

    if (source_size.width <= 0 || source_size.height <= 0 || !source_map_size.is_valid_pos(source_hex) || safe_numeric_cast<int64_t>(source_hex.x) + source_size.width > source_map_size.width || safe_numeric_cast<int64_t>(source_hex.y) + source_size.height > source_map_size.height) {
        throw MapManagerException("Source area is outside the source map", source_proto->GetName(), source_hex, source_size, source_map_size);
    }

    shared_ptr<StaticMap> source {AcquireStaticMap(source_proto)};
    msize map_size {map->GetSize()};

    // The source area's first hex lands at dest_hex
    auto move_hex {[dest_hex, source_hex, map_size, source_proto](mpos hex) -> mpos {
        int64_t x {safe_numeric_cast<int64_t>(hex.x) - source_hex.x + dest_hex.x};
        int64_t y {safe_numeric_cast<int64_t>(hex.y) - source_hex.y + dest_hex.y};

        if (x < 0 || y < 0 || x >= map_size.width || y >= map_size.height) {
            throw MapManagerException("Runtime static item lands outside the map", source_proto->GetName(), hex, dest_hex, map_size);
        }

        return mpos {numeric_cast<mcoord_t>(x), numeric_cast<mcoord_t>(y)};
    }};

    vector<ptr<StaticItem>> source_items {};
    vector<ident_t> item_ids {};
    unordered_set<ident_t> unique_ids {};

    for (ptr<StaticItem> source_item : source->GetStaticItems()) {
        mpos hex {source_item->GetHex()};

        if (hex.x >= source_hex.x && hex.y >= source_hex.y && hex.x - source_hex.x < source_size.width && hex.y - source_hex.y < source_size.height) {
            (void)move_hex(hex);
            ident_t item_id {checked_add(source_item->GetId().underlying_value(), id_offset)};

            if (!item_id) {
                throw MapManagerException("Runtime static item id is zero", source_proto->GetName(), source_item->GetId(), id_offset);
            }
            if (map->HasStaticItem(item_id)) {
                throw MapManagerException("Runtime static item id is already used on the map", source_proto->GetName(), item_id);
            }
            if (!unique_ids.emplace(item_id).second) {
                throw MapManagerException("Runtime static item id repeats", source_proto->GetName(), item_id);
            }

            source_items.emplace_back(source_item);
            item_ids.emplace_back(item_id);
        }
    }

    vector<refcount_ptr<StaticItem>> items {};
    items.reserve(source_items.size());

    for (size_t i {0}; i < source_items.size(); i++) {
        ptr<StaticItem> source_item {source_items[i]};
        auto item_proto {_engine->GetProtoItem(source_item->GetProtoId())};
        FO_VERIFY_AND_THROW(item_proto, "Missing required item prototype", source_item->GetProtoId());

        // Positions move on the plain property set before the item exists, so no entity setter observes them
        Properties item_props {source_item->GetProperties()->Copy()};
        ItemProperties item_view {item_props};
        item_view.SetHex(move_hex(item_view.GetHex()));

        if (item_view.IsNonEmptyMultihexMesh()) {
            vector<mpos> mesh {item_view.GetMultihexMesh()};

            for (mpos& mesh_hex : mesh) {
                mesh_hex = move_hex(mesh_hex);
            }

            item_view.SetMultihexMesh(mesh);
        }

        auto item {safe_alloc::make_refcounted<StaticItem>(_engine, item_ids[i], item_proto, make_nptr(&item_props))};
        item->SetEntityLock(nullptr);
        BindStaticItemScripts(source_proto->GetName(), item);
        items.emplace_back(std::move(item));
    }

    map->AddRuntimeStaticItems(items);

    return item_ids;
}

void MapManager::UnloadRuntimeStaticItems(ptr<Map> map, const_span<ident_t> runtime_static_item_ids)
{
    FO_TRACE_ZONE(Map);

    ValidateEntityAccess(map);

    unordered_set<ident_t> ids {};
    ids.reserve(runtime_static_item_ids.size());

    for (ident_t runtime_static_item_id : runtime_static_item_ids) {
        if (!map->IsRuntimeStaticItem(runtime_static_item_id)) {
            throw MapManagerException("Static item is not a runtime static item of this map", map->GetName(), runtime_static_item_id);
        }
        if (!ids.emplace(runtime_static_item_id).second) {
            throw MapManagerException("Runtime static item id to unload repeats", map->GetName(), runtime_static_item_id);
        }
    }

    map->RemoveRuntimeStaticItems(ids);
}

void MapManager::ClearStaticMaps() noexcept
{
    vector<PendingStaticMap> reads {};

    {
        scoped_lock lock {_staticMapsLock};

        for (const auto& entry : _staticMaps | std::views::values) {
            reads.emplace_back(entry.Data);
        }
    }

    // A background read uses this manager until its result is set
    for (const auto& read : reads) {
        read.wait();
    }

    scoped_lock lock {_staticMapsLock};

    _staticMaps.clear();
}

void MapManager::GenerateMapContent(ptr<Map> map)
{
    FO_TRACE_ZONE(Map);

    (void)GenerateContent(map, map->GetStaticMap(), mpos {}, true);
}

auto MapManager::GenerateContent(ptr<Map> map, ptr<const ProtoMap> source_proto, mpos dest_hex) -> vector<ident_t>
{
    FO_TRACE_ZONE(Map);

    ValidateEntityAccess(map);
    FO_VERIFY_AND_THROW(!map->IsDestroying(), "Cannot generate content on a map that is being destroyed", map->GetId());

    shared_ptr<StaticMap> source {AcquireStaticMap(source_proto)};
    return GenerateContent(map, make_ptr(source.get()), dest_hex, false);
}

auto MapManager::GenerateContent(ptr<Map> map, ptr<const StaticMap> source, mpos dest_hex, bool fresh_map) -> vector<ident_t>
{
    FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Map is already destroyed");

    msize map_size {map->GetSize()};

    auto move_hex {[dest_hex, map_size](mpos hex) -> mpos {
        int64_t x {safe_numeric_cast<int64_t>(hex.x) + dest_hex.x};
        int64_t y {safe_numeric_cast<int64_t>(hex.y) + dest_hex.y};

        if (x < 0 || y < 0 || x >= map_size.width || y >= map_size.height) {
            throw MapManagerException("Generated map content lands outside the map", hex, dest_hex, map_size);
        }

        return mpos {numeric_cast<mcoord_t>(x), numeric_cast<mcoord_t>(y)};
    }};

    // Every position is checked before the first entity exists
    for (const auto& base_cr : source->GetCritterBillets() | std::views::values) {
        (void)move_hex(base_cr->GetHex());
    }
    for (const auto& base_item : source->GetHexItemBillets() | std::views::values) {
        (void)move_hex(base_item->GetHex());
    }

    unordered_map<ident_t, ident_t> id_map {};
    vector<ident_t> created_ids {};
    vector<refcount_ptr<Critter>> created_critters {};

    // Generate critters
    for (auto&& [base_cr_id, base_cr] : source->GetCritterBillets()) {
        auto cr {_engine->CrMngr.CreateCritterOnMap(base_cr->GetProtoId(), base_cr->GetProperties(), map, move_hex(base_cr->GetHex()), base_cr->GetDir())};
        id_map.emplace(base_cr_id, cr->GetId());
        created_ids.emplace_back(cr->GetId());
        created_critters.emplace_back(cr.hold_ref());
        FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Map is already destroyed");
    }

    // Generate hex items
    for (auto&& [base_item_id, base_item] : source->GetHexItemBillets()) {
        nptr<const Properties> item_props {base_item->GetProperties()};
        optional<Properties> moved_props {};

        // A mesh lists map hexes, so it moves with the item
        if (dest_hex != mpos {} && base_item->IsNonEmptyMultihexMesh()) {
            moved_props.emplace(base_item->GetProperties()->Copy());
            ItemProperties item_view {*moved_props};
            vector<mpos> mesh {item_view.GetMultihexMesh()};

            for (mpos& mesh_hex : mesh) {
                mesh_hex = move_hex(mesh_hex);
            }

            item_view.SetMultihexMesh(mesh);
            item_props = make_nptr(&*moved_props);
        }

        auto item {_engine->ItemMngr.CreateItem(base_item->GetProtoId(), item_props)};
        id_map.emplace(base_item_id, item->GetId());
        created_ids.emplace_back(item->GetId());
        FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Map is already destroyed");
        map->AddItem(item, move_hex(base_item->GetHex()), nullptr);
        FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Map is already destroyed");
    }

    // Add children items
    for (const auto& base_item : source->GetChildItemBillets() | std::views::values) {
        // Map id to owner
        ident_t owner_id {};

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
        auto item {_engine->ItemMngr.CreateItem(base_item->GetProtoId(), base_item->GetProperties())};
        FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Map is already destroyed");

        // Add to parent
        if (base_item->GetOwnership() == ItemOwnership::CritterInventory) {
            auto cr_cont {map->GetCritter(owner_id)};
            FO_VERIFY_AND_THROW(cr_cont, "Missing required critter container");

            _engine->CrMngr.AddItemToCritter(cr_cont, item, false);
            FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Map is already destroyed");
        }
        else if (base_item->GetOwnership() == ItemOwnership::ItemContainer) {
            auto item_cont {map->GetItem(owner_id)};
            FO_VERIFY_AND_THROW(item_cont, "Missing required item container");

            item_cont->AddItemToContainer(item, any_t {string {base_item->GetContainerStack()}});
            FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Map is already destroyed");
        }
        else {
            FO_UNREACHABLE_PLACE();
        }
    }

    // Process visibility: every critter of a fresh map, the new ones of a live map, whose other critters already saw
    // each new critter and item arrive
    auto process_visibility {[this, map](ptr<Critter> cr) {
        if (!cr->IsDestroyed()) {
            ProcessVisibleCritters(cr);
            FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Map is already destroyed");
        }
        if (!cr->IsDestroyed()) {
            ProcessVisibleItems(cr);
            FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Map is already destroyed");
        }
    }};

    if (fresh_map) {
        for (ptr<Critter> cr : copy_hold_ref(map->GetCritters())) {
            process_visibility(cr);
        }
    }
    else {
        for (auto& cr : created_critters) {
            process_visibility(cr);
        }
    }

    return created_ids;
}

void MapManager::DestroyMapContent(ptr<Map> map)
{
    FO_TRACE_ZONE(Map);

    ValidateEntityAccess(map);

    for (size_t prev_deps {std::numeric_limits<size_t>::max()}; map->HasCritters() || map->HasItems();) {
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

        // Each pass must strictly reduce the map's remaining content; non-convergence is corruption
        size_t remaining_deps {map->GetCritters().size() + map->GetItems().size()};
        FO_STRONG_ASSERT(remaining_deps < prev_deps, "Map content destruction made no progress", map->GetId(), remaining_deps, prev_deps);
        prev_deps = remaining_deps;
    }
}

auto MapManager::CreateLocation(hstring proto_id, const_span<hstring> map_pids, nptr<const Properties> props) -> ptr<Location>
{
    FO_TRACE_ZONE(Map);

    auto proto {_engine->GetProtoLocation(proto_id)};

    if (!proto) {
        throw GenericException("Location proto not found", proto_id);
    }

    auto loc {safe_alloc::make_refcounted<Location>(_engine, ident_t {}, proto, props)};

    _engine->EntityMngr.RegisterLocation(loc);

    {
        ScopedSyncContext create_scope {};

        for (auto map_pid : map_pids) {
            auto map_proto {_engine->GetProtoMap(map_pid)};

            if (!map_proto) {
                throw GenericException("Map proto not found", map_pid);
            }

            auto static_map {GetStaticMap(map_proto)};
            auto map {safe_alloc::make_refcounted<Map>(_engine, ident_t {}, map_proto, loc, static_map)};
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
    FO_TRACE_ZONE(Map);

    ValidateEntityAccess(loc);
    FO_VERIFY_AND_THROW(!loc->IsDestroyed(), "Location is already destroyed");
    FO_VERIFY_AND_THROW(!loc->IsDestroying(), "Location is already being destroyed");

    auto map_proto {_engine->GetProtoMap(proto_id)};

    if (!map_proto) {
        throw GenericException("Map proto not found", proto_id);
    }

    auto static_map {GetStaticMap(map_proto)};
    auto map {safe_alloc::make_refcounted<Map>(_engine, ident_t {}, map_proto, loc, static_map)};

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
    FO_TRACE_ZONE(Map);

    ValidateEntityAccess(map);
    FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Map is already destroyed");

    auto map_holder {map.hold_ref()};
    ignore_unused(map_holder);

    DestroyMapContent(map);

    if (!map->IsDestroyed()) {
        GenerateMapContent(map);
    }
}

auto MapManager::GetLocationByPid(hstring loc_pid, int32_t skip_count) noexcept -> refcount_nptr<Location>
{
    vector<refcount_ptr<Location>> locations {_engine->EntityMngr.GetLocations()};

    for (size_t i {0}; i < locations.size(); i++) {
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
    vector<refcount_ptr<Map>> maps {_engine->EntityMngr.GetMaps()};

    for (size_t i {0}; i < maps.size(); i++) {
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
    FO_TRACE_ZONE(Map);

    auto loc_holder {loc.hold_ref()};
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

    for (size_t prev_deps {std::numeric_limits<size_t>::max()}; loc->HasInnerEntities();) {
        try {
            if (loc->HasInnerEntities()) {
                _engine->EntityMngr.DestroyInnerEntities(loc);
            }
        }
        catch (const std::exception& ex) {
            exceptions::report_and_continue(ex);
        }

        // Each pass must strictly reduce the location's remaining inner entities; non-convergence is corruption
        size_t remaining_deps {loc->GetInnerEntitiesCount()};
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
    FO_TRACE_ZONE(Map);

    auto map_holder {map.hold_ref()};
    ignore_unused(map_holder);
    EnsureEntitySynced(map);

    if (map->IsDestroying() || map->IsDestroyed()) {
        return;
    }

    map->MarkAsDestroying();

    _engine->OnMapFinish.Fire(map);
    FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Map is already destroyed");

    auto loc {map->GetLocation()};
    FO_VERIFY_AND_THROW(loc, "Missing location instance");
    auto loc_ptr {loc.hold_ref()};
    ValidateEntityAccess(loc_ptr);
    ValidateEntityAccess(map);
    loc_ptr->OnMapRemoved.Fire(map);
    FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Map is already destroyed");
    FO_VERIFY_AND_THROW(!loc_ptr->IsDestroyed(), "Location is already destroyed");
    FO_VERIFY_AND_THROW(map->GetLocation() == nptr<Location> {loc_ptr}, "Map is attached to a different location during map destruction", map->GetId(), loc_ptr->GetId(), map->GetLocation() ? map->GetLocation()->GetId() : ident_t {});

    DestroyMapInternal(map);
}

void MapManager::DestroyMapInternal(ptr<Map> map)
{
    FO_TRACE_ZONE(Map);

    EnsureEntitySynced(map);

    // Eject spectators before destroying map content so each spectator gets a clean LoadMap(nullptr) and clears its ViewMap
    while (map->HasSpectatorPlayers()) {
        auto player {map->GetSpectatorPlayers().back()};
        ValidateEntityAccess(player);
        player->ResetViewMap();
        safe_call([&] { player->Send_LoadMap(nullptr); });
    }

    for (size_t prev_deps {std::numeric_limits<size_t>::max()}; map->HasCritters() || map->HasItems() || map->HasInnerEntities();) {
        try {
            if (map->HasCritters() || map->HasItems()) {
                DestroyMapContent(map);
            }

            if (map->HasInnerEntities()) {
                _engine->EntityMngr.DestroyInnerEntities(map);
            }
        }
        catch (const std::exception& ex) {
            exceptions::report_and_continue(ex);
        }

        // Each pass must strictly reduce the map's remaining content; non-convergence is corruption
        size_t remaining_deps {map->GetCritters().size() + map->GetItems().size() + map->GetInnerEntitiesCount()};
        FO_STRONG_ASSERT(remaining_deps < prev_deps, "Map destruction made no progress", map->GetId(), remaining_deps, prev_deps);
        prev_deps = remaining_deps;
    }

    _engine->TimeEventMngr.CancelAllForEntity(map);

    auto loc {map->GetLocation()};
    FO_VERIFY_AND_THROW(loc, "Missing location instance");
    ValidateEntityAccess(loc);
    loc->RemoveMap(map);

    map->SetParent(nullptr);
    map->MarkAsDestroyed();
    _engine->EntityMngr.UnregisterMap(map);
}

auto MapManager::TracePath(ptr<const Map> map, mpos start_hex, mpos target_hex, int32_t max_dist, float32_t angle, nptr<const Critter> find_cr, CritterFindType find_type, bool check_last_movable, bool collect_critters) const -> TraceResult
{
    FO_TRACE_ZONE(Map);

    ValidateEntityAccess(map);

    TraceResult output {};
    output.FullyTraced = false;
    output.IsCritterFound = false;
    output.HasLastMovable = false;

    auto map_size {map->GetSize()};
    int32_t dist {max_dist != 0 ? max_dist : GeometryHelper::GetDistance(start_hex, target_hex)};

    auto tracer {LineTracer(start_hex, target_hex, angle, map_size)};
    mpos next_hex {start_hex};
    mpos prev_hex {next_hex};
    bool last_passed_ok {false};

    for (int32_t i {0};; i++) {
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
            auto critters {map->GetCrittersOnHex(next_hex, find_type)};

            for (ptr<const Critter> cr : critters) {
                if (std::ranges::find(output.Critters, cr) == output.Critters.end()) {
                    output.Critters.emplace_back(cr);
                }
            }
        }

        if (find_cr) {
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

auto MapManager::FindPath(ptr<const Map> map, nptr<const Critter> from_cr, mpos from_hex, mpos to_hex, int32_t multihex, int32_t cut, ipos16 to_hex_offset, function<bool(ptr<const Item>)> gag_callback, int32_t max_length) const -> FindPathOutput
{
    ValidateEntityAccess(map);
    FO_VERIFY_AND_THROW(max_length >= 0, "Path search length limit must not be negative", max_length);

    // Pre-validate target hex (terrain/items only; critters are always passable)
    if (cut == 0) {
        bool target_movable {multihex > 0 ? map->IsHexesMovable(to_hex, multihex) : map->IsHexMovable(to_hex)};

        if (!target_movable && !map->CheckGagItem(to_hex, gag_callback)) {
            FindPathOutput output {};
            output.Result = FindPathOutput::ResultType::HexBusy;
            return output;
        }
    }

    FindPathInput settings {};
    settings.FromHex = from_hex;
    settings.FromHexOffset = {};

    if (from_cr) {
        settings.FromHexOffset = from_cr->GetHexOffset();
    }

    settings.ToHex = to_hex;
    settings.ToHexOffset = to_hex_offset;
    settings.MapSize = map->GetSize();
    settings.MaxLength = max_length != 0 ? std::min(max_length, _engine->Settings->Geometry.MaxPathFindLength) : _engine->Settings->Geometry.MaxPathFindLength;
    settings.EnclosureProbeLimit = _engine->Settings->Geometry.PathFindEnclosureProbe;
    settings.CritterDetour = _engine->Settings->Geometry.PathFindCritterDetour;
    settings.Cut = cut;
    settings.Multihex = multihex;
    settings.FreeMovement = _engine->Settings->Geometry.MapFreeMovement;

    settings.CheckHex = [&](mpos hex) -> HexBlockResult {
        if (!map->IsHexMovable(hex)) {
            if (map->CheckGagItem(hex, gag_callback)) {
                return HexBlockResult::DeferGag;
            }

            return HexBlockResult::Blocked;
        }

        if (map->HasPathfindingObstacle(hex, from_cr)) {
            return HexBlockResult::DeferCritter;
        }

        return HexBlockResult::Passable;
    };

    return PathFinding::FindPath(settings);
}

auto MapManager::FindPathToAny(ptr<const Map> map, nptr<const Critter> from_cr, mpos from_hex, const_span<mpos> target_hexes, int32_t multihex, function<bool(ptr<const Item>)> gag_callback) const -> FindPathOutput
{
    ValidateEntityAccess(map);

    auto map_size {map->GetSize()};
    unordered_set<mpos> target_hex_set {};
    target_hex_set.reserve(target_hexes.size());

    for (mpos target_hex : target_hexes) {
        if (map_size.is_valid_pos(target_hex)) {
            target_hex_set.emplace(target_hex);
        }
    }

    if (target_hex_set.empty()) {
        FindPathOutput output {};
        output.Result = FindPathOutput::ResultType::InvalidHexes;
        return output;
    }

    FindPathInput settings {};
    settings.FromHex = from_hex;
    settings.FromHexOffset = from_cr ? from_cr->GetHexOffset() : ipos16 {};
    settings.MapSize = map_size;
    settings.MaxLength = _engine->Settings->Geometry.MaxPathFindLength;
    settings.CritterDetour = _engine->Settings->Geometry.PathFindCritterDetour;
    settings.Multihex = multihex;
    settings.FreeMovement = _engine->Settings->Geometry.MapFreeMovement;
    settings.CheckTarget = [&target_hex_set](mpos hex) { return target_hex_set.contains(hex); };
    settings.CheckHex = [&](mpos hex) -> HexBlockResult {
        if (!map->IsHexMovable(hex)) {
            if (map->CheckGagItem(hex, gag_callback)) {
                return HexBlockResult::DeferGag;
            }

            return HexBlockResult::Blocked;
        }

        if (map->HasPathfindingObstacle(hex, from_cr)) {
            return HexBlockResult::DeferCritter;
        }

        return HexBlockResult::Passable;
    };

    return PathFinding::FindPath(settings);
}

auto MapManager::FindReachableHexes(ptr<const Map> map, mpos from_hex, const_span<mpos> target_hexes, function<bool(ptr<const Item>)> gag_callback) const -> vector<mpos>
{
    ValidateEntityAccess(map);

    FindReachableInput settings {};
    settings.FromHex = from_hex;
    settings.MapSize = map->GetSize();
    settings.MaxLength = _engine->Settings->Geometry.MaxPathFindLength;
    settings.TargetHexes = target_hexes;

    // A critter or a gag the callback lets through makes a route dearer, never impossible, so only blockers are asked
    settings.CheckHex = [&](mpos hex) -> HexBlockResult {
        if (map->IsHexMovable(hex) || map->CheckGagItem(hex, gag_callback)) {
            return HexBlockResult::Passable;
        }

        return HexBlockResult::Blocked;
    };

    return PathFinding::FindReachable(settings);
}

void MapManager::TransferToMap(ptr<Critter> cr, ptr<Map> map, mpos hex, mdir dir, optional<int32_t> safe_radius)
{
    Transfer(cr, map, hex, dir, safe_radius, {});
}

void MapManager::TransferToGlobal(ptr<Critter> cr, ident_t global_cr_id)
{
    Transfer(cr, nullptr, {}, mdir {}, std::nullopt, global_cr_id);
}

void MapManager::Transfer(ptr<Critter> cr, nptr<Map> map, mpos hex, mdir dir, optional<int32_t> safe_radius, ident_t global_cr_id)
{
    FO_TRACE_ZONE(Map);

    EnsureEntitySynced(cr);
    ValidateEntityAccess(map);

    if (map) {
        FO_VERIFY_AND_THROW(map->GetSize().is_valid_pos(hex), "Critter transfer target hex is outside target map bounds", cr->GetId(), map->GetId(), hex, map->GetSize());
    }

    if (cr->IsMapTransfersLocked()) {
        throw GenericException("Critter transfers locked");
    }

    cr->LockMapTransfers();
    auto restore_transfers {scope_exit([cr]() mutable noexcept { cr->UnlockMapTransfers(); })};

    auto prev_map_id {cr->GetMapId()};
    auto prev_map_ref {prev_map_id ? cr->GetParent<Map>() : refcount_nptr<Map> {}};
    FO_VERIFY_AND_THROW(!prev_map_id || prev_map_ref, "Previous map id is set but previous map was not found");
    ValidateEntityAccess(prev_map_ref);

    if (map && map != prev_map_ref) {
        auto loc {map->GetLocation()};
        FO_VERIFY_AND_THROW(loc, "Missing location instance");
        ValidateEntityAccess(loc);
    }

    cr->StopMoving();

    // A transfer places the critter on a hex; the sub-hex offset an interrupted step left belongs to the old position
    cr->SetHexOffset({});

    if (cr->GetIsAttached()) {
        cr->DetachFromCritter();
    }

    vector<ptr<Critter>> attached_critters {};

    if (cr->HasAttachedCritters()) {
        auto attached_span {cr->GetAttachedCritters()};
        attached_critters.assign(attached_span.begin(), attached_span.end());

        for (auto& attached_cr : attached_critters) {
            attached_cr->DetachFromCritter();
        }
    }

    if (prev_map_ref == map) {
        // Between one map
        if (map) {
            FO_VERIFY_AND_THROW(map->GetSize().is_valid_pos(hex), "Critter intra-map transfer target hex is outside map bounds", cr->GetId(), map->GetId(), hex, map->GetSize());

            int32_t multihex {cr->GetMultihex()};

            mpos start_hex {hex};

            if (safe_radius.has_value()) {
                auto safe_start_hex {map->FindStartHex(hex, multihex, safe_radius.value(), true)};
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
        mpos start_hex {hex};

        if (map) {
            int32_t multihex {cr->GetMultihex()};

            if (safe_radius.has_value()) {
                auto safe_start_hex {map->FindStartHex(hex, multihex, safe_radius.value(), true)};
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
            _engine->OnMapCritterIn.Fire(map, cr);

            if (cr->IsDestroyed() || map->IsDestroyed()) {
                return;
            }
            if (cr->GetMapId() != map->GetId()) {
                return;
            }

            ValidateEntityAccess(map);
            EnsureEntitySynced(cr);

            auto map_cr {map->GetCritter(cr->GetId())};
            nptr<Critter> expected_cr {cr};
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

            auto map_cr {map->GetCritter(cr->GetId())};
            nptr<Critter> expected_cr {cr};
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
            auto master_cr {cr};

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

    // ValidateEntityAccess and event dispatch tolerate destroyed entity arguments
    ValidateEntityAccess(cr);
    ValidateEntityAccess(prev_map_ref);
    _engine->OnCritterTransfer.Fire(cr, prev_map_ref);
}

void MapManager::AddCritterToMap(ptr<Critter> cr, nptr<Map> map, mpos hex, mdir dir, ident_t global_cr_id)
{
    FO_TRACE_ZONE(Map);

    ValidateEntityAccess(cr);
    ValidateEntityAccess(map);

    FO_VERIFY_AND_THROW(!cr->IsDestroyed(), "Critter is already destroyed");
    cr->LockMapTransfers();
    auto restore_transfers {scope_exit([cr]() mutable noexcept { cr->UnlockMapTransfers(); })};

    if (map) {
        FO_VERIFY_AND_THROW(!map->IsDestroyed(), "Cannot add a critter to an already destroyed map", map->GetId(), cr->GetId());
        FO_VERIFY_AND_THROW(!map->IsDestroying(), "Cannot add a critter to a map that is being destroyed", map->GetId(), cr->GetId());
        FO_VERIFY_AND_THROW(map->GetSize().is_valid_pos(hex), "Critter map placement target hex is outside map bounds", cr->GetId(), map->GetId(), hex, map->GetSize());

        cr->SetMapId(map->GetId());
        cr->SetHex(hex);
        cr->ChangeDir(dir);

        // The restore index lists critters that have their own records; promotion lists a temporary one
        if (!cr->GetControlledByPlayer() && cr->IsPersistent()) {
            auto cr_ids {map->GetCritterIds()};
            vec_add_unique_value(cr_ids, cr->GetId());
            map->SetCritterIds(cr_ids);
        }

        map->AddCritter(cr);

        // Notify spectators
        map->RefreshAreaSpectators(cr);
    }
    else {
        auto& cr_group {cr->GetRawGlobalMapGroup()};
        FO_VERIFY_AND_THROW(!cr_group, "Critter group is already set");

        refcount_nptr<Critter> global_cr {};

        if (global_cr_id && global_cr_id != cr->GetId()) {
            global_cr = _engine->EntityMngr.GetCritter(global_cr_id);
        }

        ValidateEntityAccess(global_cr);

        cr->SetMapId({});
        cr->SetParent(nullptr);

        if (global_cr && !global_cr->GetMapId()) {
            auto& global_cr_group {global_cr->GetRawGlobalMapGroup()};
            FO_VERIFY_AND_THROW(global_cr_group, "Missing required global critter group");

            cr->SetGlobalMapTripId(global_cr->GetGlobalMapTripId());

            for (ptr<Critter> group_cr : global_cr_group->GetMembers()) {
                group_cr->Send_AddCritter(cr);
            }

            cr_group = global_cr_group;
        }
        else {
            // Tight lambda so the property lock is released (even on a throw) exactly when the
            // trip-id read/advance finishes, without widening the locked region over the work below
            auto trip_id {[this]() {
                _engine->LockForPropertyAccess();
                auto unlock_prop {scope_exit([this]() noexcept { _engine->UnlockForPropertyAccess(); })};
                auto next_trip_id {_engine->GetLastGlobalMapTripId() + 1};
                _engine->SetLastGlobalMapTripId(next_trip_id);
                return next_trip_id;
            }()};

            cr->SetGlobalMapTripId(trip_id);

            cr_group = safe_alloc::make_shared<GlobalMapGroup>();
        }

        cr_group->AddMember(cr);
    }
}

void MapManager::RemoveCritterFromMap(ptr<Critter> cr, nptr<Map> map)
{
    FO_TRACE_ZONE(Map);

    ValidateEntityAccess(cr);
    ValidateEntityAccess(map);

    FO_VERIFY_AND_THROW(!cr->IsDestroyed(), "Critter is already destroyed");
    auto cr_holder {cr.hold_ref()};
    ignore_unused(cr_holder);
    cr->LockMapTransfers();
    auto restore_transfers {scope_exit([cr]() mutable noexcept { cr->UnlockMapTransfers(); })};

    if (map) {
        FO_VERIFY_AND_THROW(cr->GetMapId() == map->GetId(), "Critter belongs to a different map");
        auto map_holder {map.hold_ref()};
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
        auto map_cr {map->GetCritter(cr->GetId())};
        nptr<Critter> expected_cr {cr};
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
            auto current_map_cr {map->GetCritter(cr->GetId())};
            nptr<Critter> current_expected_cr {cr};
            FO_VERIFY_AND_THROW(current_map_cr == current_expected_cr, "Critter is not registered in its map after disappearance notification", map->GetId(), cr->GetId(), cr->GetMapId());
        }

        ValidateEntityAccess(map);
        EnsureEntitySynced(cr);

        cr->ClearVisibleEnitites();

        // Notify spectators
        map->RefreshAreaSpectators(cr, true);

        // Leaving the map may demote the critter, so its listing follows the persistence it had on the map
        bool listed {cr->IsPersistent()};

        map->RemoveCritter(cr);
        cr->SetMapId({});

        if (listed && !cr->GetControlledByPlayer()) {
            auto cr_ids {map->GetCritterIds()};
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
        auto& cr_group {cr->GetRawGlobalMapGroup()};
        cr_group->RemoveMember(cr);

        for (ptr<Critter> group_cr : cr_group->GetMembers()) {
            group_cr->Send_RemoveCritter(cr);
        }

        cr_group.reset();
    }
}

void MapManager::ProcessVisibleCritters(ptr<Critter> cr)
{
    FO_TRACE_ZONE(Map);

    if (cr->IsDestroying() || cr->IsDestroyed()) {
        return;
    }

    ValidateEntityAccess(cr);

    if (cr->GetMapId()) {
        auto map {cr->GetParent<Map>()};
        FO_VERIFY_AND_THROW(map, "Missing map instance");
        ValidateEntityAccess(map);
        map->RefreshAreaSpectators(cr);

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
    ValidateEntityAccess(map);

    auto map_holder {map.hold_ref()};
    auto cr_holder {cr.hold_ref()};
    auto target_holder {target.hold_ref()};
    ignore_unused(map_holder);
    ignore_unused(cr_holder);
    ignore_unused(target_holder);

    auto is_look_context_changed {[&map, &cr, &target]() noexcept -> bool {
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
    }};

    if (cr == target) {
        return;
    }
    if (is_look_context_changed()) {
        return;
    }

    ValidateEntityAccess(cr);
    ValidateEntityAccess(target);

    auto vis_mode {IsCritterSeeCritter(map, cr, target)};

    if (vis_mode == CritterVisibilityMode::None && target->HasAttachedCritters()) {
        for (auto& attached_cr : target->GetAttachedCritters()) {
            FO_VERIFY_AND_THROW(attached_cr->GetMapId() == map->GetId(), "Attached critter is on a different map than its visibility target", target->GetId(), attached_cr->GetId(), attached_cr->GetMapId(), map->GetId());

            auto attached_mode {IsCritterSeeCritter(map, cr, attached_cr)};

            if (attached_mode != CritterVisibilityMode::None) {
                vis_mode = attached_mode;
                break;
            }
        }
    }

    bool is_see {vis_mode != CritterVisibilityMode::None};

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
            auto prev_mode {cr->GetVisibleCritterMode(target->GetId())};

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

    int32_t dist {GeometryHelper::GetDistance(cr->GetHex(), target->GetHex())};
    int32_t show_cr_dist1 {cr->GetShowCritterDist1()};
    int32_t show_cr_dist2 {cr->GetShowCritterDist2()};
    int32_t show_cr_dist3 {cr->GetShowCritterDist3()};

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
    FO_TRACE_ZONE(Map);

    if (cr->IsDestroying() || cr->IsDestroyed()) {
        return;
    }

    ValidateEntityAccess(cr);

    if (!cr->GetMapId()) {
        return;
    }

    auto map {cr->GetParent<Map>()};
    FO_VERIFY_AND_THROW(map, "Missing map instance");
    ValidateEntityAccess(map);

    auto cr_holder {cr.hold_ref()};
    ignore_unused(cr_holder);

    for (ptr<Item> item : copy_hold_ref(map->GetItems())) {
        if (cr->IsDestroying() || cr->IsDestroyed() || map->IsDestroying() || map->IsDestroyed()) {
            return;
        }
        if (cr->GetMapId() != map->GetId()) {
            return;
        }
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
    FO_TRACE_ZONE(Map);

    ValidateEntityAccess(view_player);
    ValidateEntityAccess(map);

    for (ptr<const Critter> cr : copy_hold_ref(map->GetCritters())) {
        CritterVisibilityMode mode {map->GetSpectatorCritterVisibility(view_player->GetId(), cr->GetId())};

        if (cr->IsDestroyed() || mode == CritterVisibilityMode::None) {
            continue;
        }

        view_player->Send_AddObservedCritter(cr, mode);
    }

    for (ptr<const Item> item : copy_hold_ref(map->GetItems())) {
        if (item->IsDestroyed() || !map->IsSpectatorEntityVisible(view_player->GetId(), item->GetId())) {
            continue;
        }

        view_player->Send_AddItemOnMap(item);
    }
}

FO_END_NAMESPACE
