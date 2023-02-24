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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "Item.h"
#include "Location.h"
#include "Map.h"
#include "Settings.h"
#include "Timer.h"

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
    using HexCallbackFunc = std::function<void(Map*, Critter*, uint16, uint16, uint16, uint16, uint8)>;

    // Input
    Map* TraceMap {};
    uint16 BeginHx {};
    uint16 BeginHy {};
    uint16 EndHx {};
    uint16 EndHy {};
    uint Dist {};
    float Angle {};
    Critter* FindCr {};
    CritterFindType FindType {};
    bool LastPassedSkipCritters {};
    HexCallbackFunc HexCallback {};

    // Output
    vector<Critter*>* Critters {};
    pair<uint16, uint16>* PreBlock {};
    pair<uint16, uint16>* Block {};
    pair<uint16, uint16>* LastPassed {};
    bool IsFullTrace {};
    bool IsCritterFounded {};
    bool IsHaveLastPassed {};
};

struct FindPathInput
{
    ident_t MapId {};
    uint16 MoveParams {};
    Critter* FromCritter {};
    uint16 FromHexX {};
    uint16 FromHexY {};
    uint16 ToHexX {};
    uint16 ToHexY {};
    uint16 NewToX {};
    uint16 NewToY {};
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
    uint16 NewToX {};
    uint16 NewToY {};
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

    [[nodiscard]] auto GetStaticMap(const ProtoMap* proto_map) const -> const StaticMap*;
    [[nodiscard]] auto GetLocation(ident_t loc_id) -> Location*;
    [[nodiscard]] auto GetLocation(ident_t loc_id) const -> const Location*;
    [[nodiscard]] auto GetLocationByMap(ident_t map_id) -> Location*;
    [[nodiscard]] auto GetLocationByPid(hstring loc_pid, uint skip_count) -> Location*;
    [[nodiscard]] auto GetLocations() -> const unordered_map<ident_t, Location*>&;
    [[nodiscard]] auto GetLocationsCount() const -> size_t;
    [[nodiscard]] auto IsIntersectZone(int wx1, int wy1, int w1_radius, int wx2, int wy2, int w2_radius, int zones) const -> bool;
    [[nodiscard]] auto GetZoneLocations(int zx, int zy, int zone_radius) -> vector<Location*>;
    [[nodiscard]] auto GetMap(ident_t map_id) -> Map*;
    [[nodiscard]] auto GetMap(ident_t map_id) const -> const Map*;
    [[nodiscard]] auto GetMapByPid(hstring map_pid, uint skip_count) -> Map*;
    [[nodiscard]] auto GetMaps() -> const unordered_map<ident_t, Map*>&;
    [[nodiscard]] auto GetMapsCount() const -> size_t;
    [[nodiscard]] auto CheckKnownLoc(Critter* cr, ident_t loc_id) const -> bool;
    [[nodiscard]] auto CanAddCrToMap(const Critter* cr, const Map* map, uint16 hx, uint16 hy, ident_t leader_id) const -> bool;
    [[nodiscard]] auto FindPath(const FindPathInput& input) -> FindPathOutput;
    [[nodiscard]] auto GetLocationAndMapsStatistics() const -> string;

    void LoadFromResources();
    auto CreateLocation(hstring proto_id, uint16 wx, uint16 wy) -> Location*;
    auto CreateMap(hstring proto_id, Location* loc) -> Map*;
    void DeleteLocation(Location* loc);
    void LocationGarbager();
    void RegenerateMap(Map* map);
    void TraceBullet(TraceData& trace);
    void AddCrToMap(Critter* cr, Map* map, uint16 hx, uint16 hy, uint8 dir, ident_t leader_id);
    void EraseCrFromMap(Critter* cr, Map* map);
    auto TransitToGlobal(Critter* cr, ident_t leader_id) -> bool;
    auto Transit(Critter* cr, Map* map, uint16 hx, uint16 hy, uint8 dir, uint radius, ident_t leader_id) -> bool;
    void KickPlayersToGlobalMap(Map* map);
    void ProcessVisibleCritters(Critter* view_cr);
    void ProcessVisibleItems(Critter* view_cr);
    void ViewMap(Critter* view_cr, Map* map, uint look, uint16 hx, uint16 hy, int dir);
    void AddKnownLoc(Critter* cr, ident_t loc_id);
    void EraseKnownLoc(Critter* cr, ident_t loc_id);

private:
    [[nodiscard]] auto GridAt(int x, int y) -> int16& { return _mapGrid[((FPATH_MAX_PATH + 1) + y - _mapGridOffsY) * (FPATH_MAX_PATH * 2 + 2) + ((FPATH_MAX_PATH + 1) + x - _mapGridOffsX)]; }

    void GenerateMapContent(Map* map);
    void DeleteMapContent(Map* map);

    FOServer* _engine;
    bool _runGarbager {true};
    unordered_map<const ProtoMap*, StaticMap> _staticMaps {};
    int _mapGridOffsX {};
    int _mapGridOffsY {};
    int16* _mapGrid {};
    bool _nonConstHelper {};
};
