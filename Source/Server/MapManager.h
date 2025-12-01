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

#pragma once

#include "Common.h"

#include "Critter.h"
#include "Geometry.h"
#include "Item.h"
#include "Location.h"
#include "Map.h"

FO_BEGIN_NAMESPACE();

FO_DECLARE_EXCEPTION(MapManagerException);

class FOServer;
class ProtoManager;
class EntityManager;
class ItemManager;
class CritterManager;

struct FindPathInput // Todo: make FindPathInput pointer fields raw_ptr
{
    raw_ptr<Map> TargetMap {};
    raw_ptr<Critter> FromCritter {};
    mpos FromHex {};
    mpos ToHex {};
    mpos NewToHex {};
    int32 Multihex {};
    int32 Cut {};
    int32 TraceDist {};
    bool CheckCritter {};
    bool CheckGagItems {};
    raw_ptr<Critter> TraceCr {};
};

struct FindPathOutput
{
    enum class ResultType : int8
    {
        Unknown = -1,
        Ok = 0,
        AlreadyHere = 2,
        MapNotFound = 5,
        HexBusy = 6,
        HexBusyRing = 7,
        TooFar = 8,
        NoWay = 9,
        InternalError = 10,
        InvalidHexes = 11,
        TraceFailed = 12,
        TraceTargetNullptr = 13,
    };

    ResultType Result {ResultType::Unknown};
    vector<uint8> Steps {};
    vector<uint16> ControlSteps {};
    mpos NewToHex {};
    ident_t GagCritterId {};
    ident_t GagItemId {};
};

struct TracePathInput // Todo: make TracePathInput pointer fields raw_ptr
{
    raw_ptr<Map> TraceMap {};
    mpos StartHex {};
    mpos TargetHex {};
    int32 MaxDist {};
    float32 Angle {};
    raw_ptr<const Critter> FindCr {};
    CritterFindType FindType {};
    bool CheckLastMovable {};
    bool CollectCritters {};
};

struct TracePathOutput
{
    bool IsFullTrace {};
    bool IsCritterFound {};
    bool HasLastMovable {};
    mpos PreBlock {};
    mpos Block {};
    mpos LastMovable {};
    vector<raw_ptr<Critter>> Critters {};
};

class MapManager final
{
public:
    MapManager() = delete;
    explicit MapManager(FOServer* engine);
    MapManager(const MapManager&) = delete;
    MapManager(MapManager&&) noexcept = delete;
    auto operator=(const MapManager&) = delete;
    auto operator=(MapManager&&) noexcept = delete;
    ~MapManager() = default;

    [[nodiscard]] auto GetStaticMap(const ProtoMap* proto) -> FO_NON_NULL StaticMap*;
    [[nodiscard]] auto GetLocationByPid(hstring loc_pid, int32 skip_count) noexcept -> Location*;
    [[nodiscard]] auto GetMapByPid(hstring map_pid, int32 skip_count) noexcept -> Map*;
    [[nodiscard]] auto FindPath(FindPathInput& input) const -> FindPathOutput;
    [[nodiscard]] auto TracePath(TracePathInput& input) const -> TracePathOutput;

    void LoadFromResources();
    auto CreateLocation(hstring proto_id, span<const hstring> map_pids = {}, const Properties* props = {}) -> FO_NON_NULL Location*;
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
    auto IsCritterSeeCritter(Map* map, Critter* cr, Critter* target, optional<bool>& trace_result) -> bool;

    void ProcessCritterLook(Map* map, Critter* cr, Critter* target, optional<bool>& trace_result);
    void Transfer(Critter* cr, Map* map, mpos hex, uint8 dir, optional<int32> safe_radius, ident_t global_cr_id);
    void GenerateMapContent(Map* map);
    void DestroyMapContent(Map* map);
    void DestroyMapInternal(Map* map);

    raw_ptr<FOServer> _engine;
    unordered_map<const ProtoMap*, unique_ptr<StaticMap>> _staticMaps {};
};

FO_END_NAMESPACE();
