//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "FileSystem.h"
#include "GeometryHelper.h"
#include "Item.h"
#include "Location.h"
#include "Map.h"
#include "Settings.h"
#include "Timer.h"

static constexpr auto FPATH_MAX_PATH = 400;

DECLARE_EXCEPTION(MapManagerException);

class FOServer;
class ProtoManager;
class EntityManager;
class ItemManager;
class CritterManager;

struct TraceData
{
    using HexCallbackFunc = std::function<void(Map*, Critter*, ushort, ushort, ushort, ushort, uchar)>;

    // Input
    Map* TraceMap {};
    ushort BeginHx {};
    ushort BeginHy {};
    ushort EndHx {};
    ushort EndHy {};
    uint Dist {};
    float Angle {};
    Critter* FindCr {};
    CritterFindType FindType {};
    bool LastPassedSkipCritters {};
    HexCallbackFunc HexCallback {};

    // Output
    vector<Critter*>* Critters {};
    pair<ushort, ushort>* PreBlock {};
    pair<ushort, ushort>* Block {};
    pair<ushort, ushort>* LastPassed {};
    bool IsFullTrace {};
    bool IsCritterFounded {};
    bool IsHaveLastPassed {};
};

struct FindPathInput
{
    uint MapId {};
    ushort MoveParams {};
    Critter* FromCritter {};
    ushort FromX {};
    ushort FromY {};
    ushort ToX {};
    ushort ToY {};
    ushort NewToX {};
    ushort NewToY {};
    uint Multihex {};
    uint Cut {};
    uint Trace {};
    bool IsRun {};
    bool CheckCrit {};
    bool CheckGagItems {};
    Critter* TraceCr {};
};

enum class FindPathResult
{
    Unknown = -1,
    Ok = 0,
    AlreadyHere = 2,
    MapNotFound = 5,
    HexBusy = 6,
    HexBusyRing = 7,
    TooFar = 8,
    DeadLock = 9,
    InternalError = 10,
    InvalidHexes = 11,
    TraceFailed = 12,
    TraceTargetNullptr = 13,
};

struct FindPathOutput
{
    FindPathResult Result {FindPathResult::Unknown};
    vector<PathStep> Steps {};
    ushort NewToX {};
    ushort NewToY {};
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
    ~MapManager() = default;

    [[nodiscard]] auto FindStaticMap(const ProtoMap* proto_map) const -> const StaticMap*;
    [[nodiscard]] auto GetLocation(uint loc_id) -> Location*;
    [[nodiscard]] auto GetLocation(uint loc_id) const -> const Location*;
    [[nodiscard]] auto GetLocationByMap(uint map_id) -> Location*;
    [[nodiscard]] auto GetLocationByPid(hstring loc_pid, uint skip_count) -> Location*;
    [[nodiscard]] auto GetLocations() -> vector<Location*>;
    [[nodiscard]] auto GetLocationsCount() const -> uint;
    [[nodiscard]] auto IsIntersectZone(int wx1, int wy1, int w1_radius, int wx2, int wy2, int w2_radius, int zones) const -> bool;
    [[nodiscard]] auto GetZoneLocations(int zx, int zy, int zone_radius) -> vector<Location*>;
    [[nodiscard]] auto GetMap(uint map_id) -> Map*;
    [[nodiscard]] auto GetMap(uint map_id) const -> const Map*;
    [[nodiscard]] auto GetMapByPid(hstring map_pid, uint skip_count) -> Map*;
    [[nodiscard]] auto GetMaps() -> vector<Map*>;
    [[nodiscard]] auto GetMapsCount() const -> uint;
    [[nodiscard]] auto CheckKnownLoc(Critter* cr, uint loc_id) const -> bool;
    [[nodiscard]] auto CanAddCrToMap(Critter* cr, Map* map, ushort hx, ushort hy, uint leader_id) const -> bool;
    [[nodiscard]] auto FindPath(const FindPathInput& pfd) -> FindPathOutput;
    [[nodiscard]] auto GetLocationAndMapsStatistics() const -> string;

    [[nodiscard]] auto CreateLocation(hstring proto_id, ushort wx, ushort wy) -> Location*;
    [[nodiscard]] auto CreateMap(hstring proto_id, Location* loc) -> Map*;

    void LinkMaps();
    void LoadStaticMaps(FileManager& file_mngr);
    void DeleteLocation(Location* loc, vector<Critter*>* gmap_player_critters);
    void LocationGarbager();
    void RegenerateMap(Map* map);
    void TraceBullet(TraceData& trace);
    void AddCrToMap(Critter* cr, Map* map, ushort hx, ushort hy, uchar dir, uint leader_id);
    void EraseCrFromMap(Critter* cr, Map* map);
    auto TransitToGlobal(Critter* cr, uint leader_id, bool force) -> bool;
    auto Transit(Critter* cr, Map* map, ushort hx, ushort hy, uchar dir, uint radius, uint leader_id, bool force) -> bool;
    void KickPlayersToGlobalMap(Map* map);
    void PathSetMoveParams(vector<PathStep>& path, bool is_run);
    void ProcessVisibleCritters(Critter* view_cr);
    void ProcessVisibleItems(Critter* view_cr);
    void ViewMap(Critter* view_cr, Map* map, uint look, ushort hx, ushort hy, int dir);
    void AddKnownLoc(Critter* cr, uint loc_id);
    void EraseKnownLoc(Critter* cr, uint loc_id);

private:
    [[nodiscard]] auto FindPathGrid(ushort& hx, ushort& hy, int index, bool smooth_switcher) const -> uchar;

    void LoadStaticMap(FileManager& file_mngr, const ProtoMap* pmap);
    void GenerateMapContent(Map* map);
    void DeleteMapContent(Map* map);

    FOServer* _engine;
    bool _runGarbager {true};
    bool _smoothSwitcher {};
    map<const ProtoMap*, StaticMap> _staticMaps {};
    bool _nonConstHelper {};
};
