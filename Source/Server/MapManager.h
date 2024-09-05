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
// Copyright (c) 2006 - 2023, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
    HexCallbackFunc HexCallback {};

    // Output
    vector<Critter*>* Critters {};
    pair<uint16, uint16>* PreBlock {};
    pair<uint16, uint16>* Block {};
    pair<uint16, uint16>* LastMovable {};
    bool IsFullTrace {};
    bool IsCritterFound {};
    bool IsHaveLastMovable {};
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
    [[nodiscard]] auto GetLocationByMap(ident_t map_id) noexcept -> Location*;
    [[nodiscard]] auto GetLocationByPid(hstring loc_pid, uint skip_count) noexcept -> Location*;
    [[nodiscard]] auto IsIntersectZone(int wx1, int wy1, int w1_radius, int wx2, int wy2, int w2_radius, int zones) const noexcept -> bool;
    [[nodiscard]] auto GetZoneLocations(int zx, int zy, int zone_radius) -> vector<Location*>;
    [[nodiscard]] auto GetMapByPid(hstring map_pid, uint skip_count) noexcept -> Map*;
    [[nodiscard]] auto CheckKnownLoc(Critter* cr, ident_t loc_id) const -> bool;
    [[nodiscard]] auto FindPath(const FindPathInput& input) -> FindPathOutput;
    [[nodiscard]] auto GetLocationAndMapsStatistics() const -> string;

    void LoadFromResources();
    auto CreateLocation(hstring proto_id, uint16 wx, uint16 wy) -> Location*;
    auto CreateMap(hstring proto_id, Location* loc) -> Map*;
    void DestroyLocation(Location* loc);
    void LocationGarbager();
    void RegenerateMap(Map* map);
    void TraceBullet(TraceData& trace);
    void AddCritterToMap(Critter* cr, Map* map, uint16 hx, uint16 hy, uint8 dir, ident_t global_cr_id);
    void RemoveCritterFromMap(Critter* cr, Map* map);
    void TransitToMap(Critter* cr, Map* map, uint16 hx, uint16 hy, uint8 dir, optional<uint> safe_radius);
    void TransitToGlobal(Critter* cr, ident_t global_cr_id);
    void KickPlayersToGlobalMap(Map* map);
    void ProcessVisibleCritters(Critter* cr);
    void ProcessVisibleItems(Critter* cr);
    void ViewMap(Critter* view_cr, Map* map, uint look, uint16 hx, uint16 hy, int dir);
    void AddKnownLoc(Critter* cr, ident_t loc_id);
    void RemoveKnownLoc(Critter* cr, ident_t loc_id);

private:
    [[nodiscard]] FORCE_INLINE auto GridAt(int x, int y) -> int16&;
    [[nodiscard]] auto IsCritterSeeCritter(Map* map, Critter* cr, Critter* target, optional<bool>& trace_result) -> bool;

    void ProcessCritterLook(Map* map, Critter* cr, Critter* target, optional<bool>& trace_result);
    void Transit(Critter* cr, Map* map, uint16 hx, uint16 hy, uint8 dir, optional<uint> safe_radius, ident_t global_cr_id);
    void GenerateMapContent(Map* map);
    void DestroyMapContent(Map* map);

    FOServer* _engine;
    bool _runGarbager {true};
    unordered_map<const ProtoMap*, unique_ptr<StaticMap>> _staticMaps {};
    int _mapGridOffsX {};
    int _mapGridOffsY {};
    int16* _mapGrid {};
    bool _nonConstHelper {};
};
