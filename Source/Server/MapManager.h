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
#include "ScriptSystem.h"
#include "Settings.h"

DECLARE_EXCEPTION(MapManagerException);

class ProtoManager;
class EntityManager;
class ItemManager;
class CritterManager;

#define FPATH_DATA_SIZE (10000)
#define FPATH_MAX_PATH (400)
#define FPATH_OK (0)
#define FPATH_ALREADY_HERE (2)
#define FPATH_MAP_NOT_FOUND (5)
#define FPATH_HEX_BUSY (6)
#define FPATH_HEX_BUSY_RING (7)
#define FPATH_TOOFAR (8)
#define FPATH_DEADLOCK (9)
#define FPATH_ERROR (10)
#define FPATH_INVALID_HEXES (11)
#define FPATH_TRACE_FAIL (12)
#define FPATH_TRACE_TARG_NULL_PTR (13)

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
using PathStepVec = vector<PathStep>;

class MapManager
{
public:
    MapManager(ServerSettings& sett, ProtoManager& proto_mngr, EntityManager& entity_mngr, CritterManager& cr_mngr,
        ItemManager& item_mngr, ScriptSystem& script_sys);

    void LoadStaticMaps(FileManager& file_mngr);
    StaticMap* FindStaticMap(ProtoMap* pmap);

    // Maps
    bool FindPlaceOnMap(Critter* cr, Map* map, ushort& hx, ushort& hy, uint radius);
    bool CanAddCrToMap(Critter* cr, Map* map, ushort hx, ushort hy, uint leader_id);
    void AddCrToMap(Critter* cr, Map* map, ushort hx, ushort hy, uchar dir, uint leader_id);
    void EraseCrFromMap(Critter* cr, Map* map);
    bool TransitToGlobal(Critter* cr, uint leader_id, bool force);
    bool Transit(Critter* cr, Map* map, ushort hx, ushort hy, uchar dir, uint radius, uint leader_id, bool force);
    bool IsIntersectZone(int wx1, int wy1, int wx1_radius, int wx2, int wy2, int wx2_radius, int zones);
    void GetZoneLocations(int zx, int zy, int zone_radius, UIntVec& loc_ids);
    void KickPlayersToGlobalMap(Map* map);

    // Locations
    Location* CreateLocation(hash proto_id, ushort wx, ushort wy);
    bool RestoreLocation(uint id, hash proto_id, const DataBase::Document& doc);
    Location* GetLocationByMap(uint map_id);
    Location* GetLocation(uint loc_id);
    Location* GetLocationByPid(hash loc_pid, uint skip_count);
    void GetLocations(LocationVec& locs);
    uint GetLocationsCount();
    void LocationGarbager();
    void DeleteLocation(Location* loc, ClientVec* gmap_players);

    // Maps
    Map* CreateMap(hash proto_id, Location* loc);
    bool RestoreMap(uint id, hash proto_id, const DataBase::Document& doc);
    void RegenerateMap(Map* map);
    Map* GetMap(uint map_id);
    Map* GetMapByPid(hash map_pid, uint skip_count);
    void GetMaps(MapVec& maps);
    uint GetMapsCount();
    void TraceBullet(TraceData& trace);
    int FindPath(PathFindData& pfd);
    int FindPathGrid(ushort& hx, ushort& hy, int index, bool smooth_switcher);
    PathStepVec& GetPath(uint num) { return pathesPool[num]; }
    void PathSetMoveParams(PathStepVec& path, bool is_run);

    void ProcessVisibleCritters(Critter* view_cr);
    void ProcessVisibleItems(Critter* view_cr);
    void ViewMap(Critter* view_cr, Map* map, int look, ushort hx, ushort hy, int dir);

    bool CheckKnownLocById(Critter* cr, uint loc_id);
    bool CheckKnownLocByPid(Critter* cr, hash loc_pid);
    void AddKnownLoc(Critter* cr, uint loc_id);
    void EraseKnownLoc(Critter* cr, uint loc_id);

    string GetLocationsMapsStatistics();

private:
    void LoadStaticMap(FileManager& file_mngr, ProtoMap* pmap);
    void GenerateMapContent(Map* map);
    void DeleteMapContent(Map* map);

    ServerSettings& settings;
    GeometryHelper geomHelper;
    ProtoManager& protoMngr;
    EntityManager& entityMngr;
    CritterManager& crMngr;
    ItemManager& itemMngr;
    ScriptSystem& scriptSys;
    bool runGarbager {true};
    PathStepVec pathesPool[FPATH_DATA_SIZE] {};
    uint pathNumCur {};
    bool smoothSwitcher {};
    map<ProtoMap*, StaticMap> staticMaps {};
};
