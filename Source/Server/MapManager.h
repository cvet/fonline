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

struct TraceResult
{
    bool IsFullTrace {};
    bool IsCritterFound {};
    bool HasLastMovable {};
    mpos PreBlock {};
    mpos Block {};
    mpos LastMovable {};
    vector<raw_ptr<const Critter>> Critters {};
};

class MapManager final
{
public:
    MapManager() = delete;
    explicit MapManager(ServerEngine* engine);
    MapManager(const MapManager&) = delete;
    MapManager(MapManager&&) noexcept = delete;
    auto operator=(const MapManager&) = delete;
    auto operator=(MapManager&&) noexcept = delete;
    ~MapManager() = default;

    [[nodiscard]] auto GetStaticMap(const ProtoMap* proto) -> FO_NON_NULL StaticMap*;
    [[nodiscard]] auto GetLocationByPid(hstring loc_pid, int32 skip_count) noexcept -> Location*;
    [[nodiscard]] auto GetMapByPid(hstring map_pid, int32 skip_count) noexcept -> Map*;
    [[nodiscard]] auto FindPath(const Map* map, const Critter* from_cr, mpos from_hex, mpos to_hex, int32 multihex, int32 cut, function<bool(const Item*)> gag_callback = {}) const -> FindPathOutput;
    [[nodiscard]] auto TracePath(const Map* map, mpos start_hex, mpos target_hex, int32 max_dist = 0, float32 angle = 0.0f, const Critter* find_cr = nullptr, CritterFindType find_type = CritterFindType::Any, bool check_last_movable = false, bool collect_critters = false) const -> TraceResult;

    void LoadFromResources();
    auto CreateLocation(hstring proto_id, const_span<hstring> map_pids = {}, const Properties* props = {}) -> FO_NON_NULL Location*;
    void DestroyLocation(Location* loc);
    auto CreateMap(hstring proto_id, Location* loc) -> FO_NON_NULL Map*;
    void DestroyMap(Map* map);
    void RegenerateMap(Map* map);
    void AddCritterToMap(Critter* cr, Map* map, mpos hex, uint8 dir, ident_t global_cr_id);
    void RemoveCritterFromMap(Critter* cr, Map* map);
    void TransferToMap(Critter* cr, Map* map, mpos hex, uint8 dir, optional<int32> safe_radius);
    void TransferToGlobal(Critter* cr, ident_t global_cr_id);
    void ProcessVisibleCritters(Critter* cr);
    void ProcessVisibleItems(Critter* cr);
    void ViewMap(Critter* view_cr, Map* map, int32 look, mpos hex, uint8 dir);

private:
    auto IsCritterSeeCritter(const Map* map, const Critter* cr, const Critter* target) const -> bool;

    void ProcessCritterLook(Map* map, Critter* cr, Critter* target);
    void Transfer(Critter* cr, Map* map, mpos hex, uint8 dir, optional<int32> safe_radius, ident_t global_cr_id);
    void GenerateMapContent(Map* map);
    void DestroyMapContent(Map* map);
    void DestroyMapInternal(Map* map);

    raw_ptr<ServerEngine> _engine;
    unordered_map<const ProtoMap*, unique_ptr<StaticMap>> _staticMaps {};
    bool _nonConstHelper {};
};

FO_END_NAMESPACE
