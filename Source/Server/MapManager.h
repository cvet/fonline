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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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

#pragma once

#include "Common.h"

#include "Critter.h"
#include "Geometry.h"
#include "Item.h"
#include "Location.h"
#include "Map.h"
#include "PathFinding.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(MapManagerException);

class ServerEngine;
class ProtoManager;
class EntityManager;
class ItemManager;
class CritterManager;
class Player;

struct TraceResult
{
    bool FullyTraced {};
    bool IsCritterFound {};
    bool HasLastMovable {};
    mpos PreBlock {};
    mpos Block {};
    mpos LastMovable {};
    vector<ptr<const Critter>> Critters {};
};

class MapManager final
{
public:
    MapManager() = delete;
    explicit MapManager(ptr<ServerEngine> engine);
    MapManager(const MapManager&) = delete;
    MapManager(MapManager&&) noexcept = delete;
    auto operator=(const MapManager&) = delete;
    auto operator=(MapManager&&) noexcept = delete;
    ~MapManager() = default;

    [[nodiscard]] auto GetStaticMap(ptr<const ProtoMap> proto) -> ptr<StaticMap>;
    [[nodiscard]] auto IsStaticMapLoaded(ptr<const ProtoMap> proto) const -> bool;
    [[nodiscard]] auto GetLocationByPid(hstring loc_pid, int32_t skip_count) noexcept -> refcount_nptr<Location>;
    [[nodiscard]] auto GetMapByPid(hstring map_pid, int32_t skip_count) noexcept -> refcount_nptr<Map>;

    auto FindPath(ptr<const Map> map, nptr<const Critter> from_cr, mpos from_hex, mpos to_hex, int32_t multihex, int32_t cut, ipos16 to_hex_offset = {}, function<bool(ptr<const Item>)> gag_callback = {}, int32_t max_length = 0) const -> FindPathOutput;
    auto FindPathToAny(ptr<const Map> map, nptr<const Critter> from_cr, mpos from_hex, const_span<mpos> target_hexes, int32_t multihex, function<bool(ptr<const Item>)> gag_callback = {}) const -> FindPathOutput;
    auto FindReachableHexes(ptr<const Map> map, mpos from_hex, const_span<mpos> target_hexes, function<bool(ptr<const Item>)> gag_callback = {}) const -> vector<mpos>;
    auto TracePath(ptr<const Map> map, mpos start_hex, mpos target_hex, int32_t max_dist = 0, float32_t angle = 0.0f, nptr<const Critter> find_cr = nullptr, CritterFindType find_type = CritterFindType::Any, bool check_last_movable = false, bool collect_critters = false) const -> TraceResult;

    void LoadFromResources();
    auto CreateLocation(hstring proto_id, const_span<hstring> map_pids = {}, nptr<const Properties> props = {}) -> ptr<Location>;
    void DestroyLocation(ptr<Location> loc);
    auto CreateMap(hstring proto_id, ptr<Location> loc) -> ptr<Map>;
    void DestroyMap(ptr<Map> map);
    void RegenerateMap(ptr<Map> map);
    void AddCritterToMap(ptr<Critter> cr, nptr<Map> map, mpos hex, mdir dir, ident_t global_cr_id);
    void AddCritterToMap(ptr<Critter> cr, nptr<Map> map, mpos hex, hdir dir, ident_t global_cr_id) { AddCritterToMap(cr, map, hex, mdir(dir), global_cr_id); }
    void RemoveCritterFromMap(ptr<Critter> cr, nptr<Map> map);
    void TransferToMap(ptr<Critter> cr, ptr<Map> map, mpos hex, mdir dir, optional<int32_t> safe_radius);
    void TransferToMap(ptr<Critter> cr, ptr<Map> map, mpos hex, hdir dir, optional<int32_t> safe_radius) { TransferToMap(cr, map, hex, mdir(dir), safe_radius); }
    void TransferToGlobal(ptr<Critter> cr, ident_t global_cr_id);
    void ProcessVisibleCritters(ptr<Critter> cr);
    void ProcessVisibleItems(ptr<Critter> cr);
    void ViewMap(ptr<Player> view_player, ptr<Map> map);
    void LoadStaticMap(ptr<const ProtoMap> proto, bool async_load);
    void LoadStaticMaps(const_span<ptr<const ProtoMap>> protos, bool async_load);
    auto UnloadStaticMap(ptr<const ProtoMap> proto) -> bool;
    void ClearStaticMaps() noexcept;
    auto LoadRuntimeStaticItems(ptr<Map> map, ptr<const ProtoMap> source_proto, mpos dest_hex, int64_t id_offset) -> vector<ident_t>;
    auto LoadRuntimeStaticItems(ptr<Map> map, ptr<const ProtoMap> source_proto, mpos dest_hex, mpos source_hex, msize source_size, int64_t id_offset) -> vector<ident_t>;
    void UnloadRuntimeStaticItems(ptr<Map> map, const_span<ident_t> runtime_static_item_ids);
    auto GenerateContent(ptr<Map> map, ptr<const ProtoMap> source_proto, mpos dest_hex) -> vector<ident_t>;

private:
    using PendingStaticMap = std::shared_future<shared_ptr<StaticMap>>;

    struct StaticMapEntry
    {
        PendingStaticMap Data {};
        uintptr_t ReadToken {};
    };

    auto IsCritterSeeCritter(ptr<const Map> map, ptr<const Critter> cr, ptr<const Critter> target) const -> CritterVisibilityMode;
    void ProcessCritterLook(ptr<Map> map, ptr<Critter> cr, ptr<Critter> target);
    void Transfer(ptr<Critter> cr, nptr<Map> map, mpos hex, mdir dir, optional<int32_t> safe_radius, ident_t global_cr_id);
    void GenerateMapContent(ptr<Map> map);
    auto GenerateContent(ptr<Map> map, ptr<const StaticMap> source, mpos dest_hex, bool fresh_map) -> vector<ident_t>;
    auto IsStaticMapReady(const PendingStaticMap& static_map) const -> bool;
    auto ReadStaticMap(ptr<const ProtoMap> map_proto) -> shared_ptr<StaticMap>;
    void BindStaticItemScripts(string_view map_name, ptr<StaticItem> item);
    auto RequestStaticMap(ptr<const ProtoMap> proto, bool in_background) -> PendingStaticMap;
    auto AcquireStaticMap(ptr<const ProtoMap> proto) -> shared_ptr<StaticMap>;
    void DestroyMapContent(ptr<Map> map);
    void DestroyMapInternal(ptr<Map> map);

    ptr<ServerEngine> _engine;
    mutable mutex _staticMapsLock {};
    unordered_map<ptr<const ProtoMap>, StaticMapEntry> _staticMaps FO_TSA_GUARDED_BY(_staticMapsLock) {};
};

FO_END_NAMESPACE
