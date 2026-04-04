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

#include "Geometry.h"
#include "LineTracer.h"

FO_BEGIN_NAMESPACE

enum class HexBlockResult : int8
{
    Passable = 0, // Hex is passable for movement
    Blocked = -1, // Permanently blocked
    DeferGag = 1, // Blocked by gag item — route through after distance gap
    DeferCritter = 2, // Blocked by critter — route through as last resort
};

struct FindPathInput
{
    mpos FromHex {};
    mpos ToHex {};
    msize MapSize {};
    int32 MaxLength {}; // Maximum BFS depth (from engine Settings.MaxPathFindLength)
    int32 Cut {}; // Stop BFS when within this distance of target; 0 = must reach exact target
    int32 Multihex {}; // Multihex radius; 0 = single hex; >0 = directional perimeter check in BFS
    bool FreeMovement {}; // Use LineTracer optimization for control steps
    function<HexBlockResult(mpos)> CheckHex {}; // Check if a single hex blocks movement
};

struct FindPathOutput
{
    enum class ResultType : int8
    {
        Unknown = -1,
        Ok = 0,
        AlreadyHere = 2,
        HexBusy = 6,
        TooFar = 8,
        NoWay = 9,
        BacktraceError = 10,
        InvalidHexes = 11,
        TraceFailed = 12,
    };

    ResultType Result {ResultType::Unknown};
    vector<uint8> Steps {};
    vector<uint16> ControlSteps {};
    mpos NewToHex {};
};

struct TraceLineInput
{
    mpos StartHex {};
    mpos TargetHex {};
    int32 MaxDist {}; // 0 means use distance (start, target)
    float32 Angle {};
    msize MapSize {};
    bool CheckLastMovable {};
    function<bool(mpos)> IsHexBlocked {}; // Return true if hex is blocked (stops trace)
    function<bool(mpos)> IsHexMovable {}; // Optional: return true if hex is movable
};

struct TraceLineOutput
{
    bool IsFullTrace {};
    bool HasLastMovable {};
    mpos PreBlock {};
    mpos Block {};
    mpos LastMovable {};
};

namespace PathFinding
{
    // Core pathfinding algorithm (BFS with deferred routing through gags/critters)
    [[nodiscard]] auto FindPath(const FindPathInput& input) -> FindPathOutput;

    // Core line trace from start toward target, stopping at blocked hexes
    [[nodiscard]] auto TraceLine(const TraceLineInput& input) -> TraceLineOutput;

}

FO_END_NAMESPACE
