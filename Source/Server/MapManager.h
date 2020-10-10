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
#include "DataBase.h"
#include "Entity.h"
#include "FileSystem.h"
#include "GeometryHelper.h"
#include "Item.h"
#include "Location.h"
#include "Map.h"
#include "ServerScripting.h"
#include "Settings.h"
#include "Timer.h"

static constexpr auto FPATH_DATA_SIZE = 10000;
static constexpr auto FPATH_MAX_PATH = 400;

DECLARE_EXCEPTION(MapManagerException);

class ProtoManager;
class EntityManager;
class ItemManager;
class CritterManager;

enum class FindPathResult
{
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

struct TraceData
{
    // Input
    Map* TraceMap {};
    ushort BeginHx {};
    ushort BeginHy {};
    ushort EndHx {};
    ushort EndHy {};
    uint Dist {};
    float Angle {};
    Critter* FindCr {};
    int FindType {};
    bool LastPassedSkipCritters {};
    void (*HexCallback)(Map*, Critter*, ushort, ushort, ushort, ushort, uchar) {};

    // Output
    CritterVec* Critters {};
    UShortPair* PreBlock {};
    UShortPair* Block {};
    UShortPair* LastPassed {};
    bool IsFullTrace {};
    bool IsCritterFounded {};
    bool IsHaveLastPassed {};
};

struct PathFindData
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
    uint PathNum {};
    uint Trace {};
    bool IsRun {};
    bool CheckCrit {};
    bool CheckGagItems {};
    Critter* TraceCr {};
    Critter* GagCritter {};
    Item* GagItem {};
};

struct PathStep
{
    ushort HexX {};
    ushort HexY {};
    uint MoveParams {};
    uchar Dir {};
};

class MapManager final
{
public:
    MapManager() = delete;
    MapManager(ServerSettings& settings, ProtoManager& proto_mngr, EntityManager& entity_mngr, CritterManager& cr_mngr, ItemManager& item_mngr, ServerScriptSystem& script_sys, GameTimer& game_time);
    MapManager(const MapManager&) = delete;
    MapManager(MapManager&&) noexcept = delete;
    auto operator=(const MapManager&) = delete;
    auto operator=(MapManager&&) noexcept = delete;
    ~MapManager() = default;

    void LoadStaticMaps(FileManager& file_mngr);
    [[nodiscard]] auto FindStaticMap(const ProtoMap* proto_map) -> const StaticMap*;

    // Maps
    auto CanAddCrToMap(Critter* cr, Map* map, ushort hx, ushort hy, uint leader_id) const -> bool;
    void AddCrToMap(Critter* cr, Map* map, ushort hx, ushort hy, uchar dir, uint leader_id);
    void EraseCrFromMap(Critter* cr, Map* map);
    auto TransitToGlobal(Critter* cr, uint leader_id, bool force) -> bool;
    auto Transit(Critter* cr, Map* map, ushort hx, ushort hy, uchar dir, uint radius, uint leader_id, bool force) -> bool;
    auto IsIntersectZone(int wx1, int wy1, int wx1_radius, int wx2, int wy2, int wx2_radius, int zones) const -> bool;
    void GetZoneLocations(int zx, int zy, int zone_radius, UIntVec& loc_ids);
    void KickPlayersToGlobalMap(Map* map);

    // Locations
    auto CreateLocation(hash proto_id, ushort wx, ushort wy) -> Location*;
    auto RestoreLocation(uint id, hash proto_id, const DataBase::Document& doc) -> bool;
    auto GetLocationByMap(uint map_id) -> Location*;
    auto GetLocation(uint loc_id) -> Location*;
    auto GetLocationByPid(hash loc_pid, uint skip_count) -> Location*;
    void GetLocations(LocationVec& locs);
    auto GetLocationsCount() const -> uint;
    void LocationGarbager();
    void DeleteLocation(Location* loc, ClientVec* gmap_players);

    // Maps
    auto CreateMap(hash proto_id, Location* loc) -> Map*;
    auto RestoreMap(uint id, hash proto_id, const DataBase::Document& doc) -> bool;
    void RegenerateMap(Map* map);
    auto GetMap(uint map_id) -> Map*;
    auto GetMapByPid(hash map_pid, uint skip_count) -> Map*;
    void GetMaps(MapVec& maps);
    auto GetMapsCount() const -> uint;
    void TraceBullet(TraceData& trace) const;
    auto FindPath(PathFindData& pfd) -> FindPathResult;
    auto FindPathGrid(ushort& hx, ushort& hy, int index, bool smooth_switcher) -> int;
    auto GetPath(uint num) -> vector<PathStep>& { return _pathesPool[num]; }
    static void PathSetMoveParams(vector<PathStep>& path, bool is_run);

    void ProcessVisibleCritters(Critter* view_cr);
    void ProcessVisibleItems(Critter* view_cr);
    void ViewMap(Critter* view_cr, Map* map, int look, ushort hx, ushort hy, int dir);

    auto CheckKnownLocById(Critter* cr, uint loc_id) const -> bool;
    auto CheckKnownLocByPid(Critter* cr, hash loc_pid) const -> bool;
    void AddKnownLoc(Critter* cr, uint loc_id);
    void EraseKnownLoc(Critter* cr, uint loc_id);

    auto GetLocationsMapsStatistics() const -> string;

private:
    void LoadStaticMap(FileManager& file_mngr, ProtoMap* pmap);
    void GenerateMapContent(Map* map);
    void DeleteMapContent(Map* map);

    ServerSettings& _settings;
    GeometryHelper _geomHelper;
    ProtoManager& _protoMngr;
    EntityManager& _entityMngr;
    CritterManager& _crMngr;
    ItemManager& _itemMngr;
    ServerScriptSystem& _scriptSys;
    GameTimer& _gameTime;
    bool _runGarbager {true};
    vector<PathStep> _pathesPool[FPATH_DATA_SIZE] {};
    uint _pathNumCur {};
    bool _smoothSwitcher {};
    map<const ProtoMap*, StaticMap> _staticMaps {};
};
