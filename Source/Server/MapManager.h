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

#pragma once

#include "Common.h"

#include "Critter.h"
#include "Entity.h"
#include "Item.h"
#include "Location.h"
#include "Map.h"

// Todo: make dynamic path growth and move max value to settings
static constexpr auto FPATH_MAX_PATH = 400;

DECLARE_EXCEPTION(MapManagerException);

class FOServer;
class ProtoManager;
class EntityManager;
class ItemManager;
class CritterManager;

struct TraceData
{
    // Input
    Map* TraceMap {};
    mpos StartHex {};
    mpos TargetHex {};
    uint MaxDist {};
    float Angle {};
    Critter* FindCr {};
    CritterFindType FindType {};

    // Output
    vector<Critter*>* Critters {};
    mpos* PreBlock {};
    mpos* Block {};
    mpos* LastMovable {};
    bool IsFullTrace {};
    bool IsCritterFound {};
    bool HasLastMovable {};
};

struct FindPathInput
{
    ident_t MapId {};
    Critter* FromCritter {};
    mpos FromHex {};
    mpos ToHex {};
    mpos NewToHex {};
    uint Multihex {};
    uint Cut {};
    uint TraceDist {};
    bool CheckCritter {};
    bool CheckGagItems {};
    Critter* TraceCr {};
};

struct FindPathOutput
{
    enum class ResultType
    {
        Unknown = -1,
        Ok = 0,
        AlreadyHere = 2,
        MapNotFound = 5,
        HexBusy = 6,
        HexBusyRing = 7,
        TooFar = 8,
        Deadlock = 9,
        InternalError = 10,
        InvalidHexes = 11,
        TraceFailed = 12,
        TraceTargetNullptr = 13,
    };

    ResultType Result {ResultType::Unknown};
    vector<uint8> Steps {};
    vector<uint16> ControlSteps {};
    mpos NewToHex {};
    Critter* GagCritter {};
    Item* GagItem {};
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
    ~MapManager();

    [[nodiscard]] auto GetStaticMap(const ProtoMap* proto) const -> NON_NULL const StaticMap*;
    [[nodiscard]] auto GetLocationByPid(hstring loc_pid, uint skip_count) noexcept -> Location*;
    [[nodiscard]] auto IsIntersectZone(int wx1, int wy1, int w1_radius, int wx2, int wy2, int w2_radius, int zones) const noexcept -> bool;
    [[nodiscard]] auto GetZoneLocations(int zx, int zy, int zone_radius) -> vector<Location*>;
    [[nodiscard]] auto GetMapByPid(hstring map_pid, uint skip_count) noexcept -> Map*;
    [[nodiscard]] auto FindPath(const FindPathInput& input) -> FindPathOutput;
    [[nodiscard]] auto GetLocationAndMapsStatistics() const -> string;

    void LoadFromResources();
    auto CreateLocation(hstring proto_id, const Properties* props) -> NON_NULL Location*;
    void DestroyLocation(Location* loc);
    void RegenerateMap(Map* map);
    void TraceBullet(TraceData& trace);
    void AddCritterToMap(Critter* cr, Map* map, mpos hex, uint8 dir, ident_t global_cr_id);
    void RemoveCritterFromMap(Critter* cr, Map* map);
    void TransitToMap(Critter* cr, Map* map, mpos hex, uint8 dir, optional<uint> safe_radius);
    void TransitToGlobal(Critter* cr, ident_t global_cr_id);
    void KickPlayersToGlobalMap(Map* map);
    void ProcessVisibleCritters(Critter* cr);
    void ProcessVisibleItems(Critter* cr);
    void ViewMap(Critter* view_cr, Map* map, uint look, mpos hex, int dir);

private:
    [[nodiscard]] FORCE_INLINE auto GridAt(mpos pos) -> int16&;
    [[nodiscard]] auto IsCritterSeeCritter(Map* map, Critter* cr, Critter* target, optional<bool>& trace_result) -> bool;

    auto CreateMap(hstring proto_id, Location* loc) -> NON_NULL Map*;
    void ProcessCritterLook(Map* map, Critter* cr, Critter* target, optional<bool>& trace_result);
    void Transit(Critter* cr, Map* map, mpos hex, uint8 dir, optional<uint> safe_radius, ident_t global_cr_id);
    void GenerateMapContent(Map* map);
    void DestroyMapContent(Map* map);

    FOServer* _engine;
    unordered_map<const ProtoMap*, unique_ptr<StaticMap>> _staticMaps {};
    mpos _mapGridOffset {};
    int16* _mapGrid {};
    bool _nonConstHelper {};
};
