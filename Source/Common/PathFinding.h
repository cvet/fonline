// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "Geometry.h"
#include "LineTracer.h"

FO_BEGIN_NAMESPACE

enum class HexBlockResult : int8_t
{
    Passable = 0, // Hex is passable for movement
    Blocked = -1, // Permanently blocked
    DeferGag = 1, // Blocked by gag item — route through after distance gap
    DeferCritter = 2, // Blocked by critter — route through as last resort
};

struct FindPathInput
{
    mpos FromHex {};
    ipos16 FromHexOffset {};
    mpos ToHex {};
    ipos16 ToHexOffset {};
    msize MapSize {};
    int32_t MaxLength {}; // Longest route allowed, in steps (from engine Settings.Geometry.MaxPathFindLength)
    int32_t EnclosureProbeLimit {}; // Forward hexes after which the target side is flooded back within the same budget; 0 = never
    int32_t CritterDetour {}; // A project-marked critter obstacle costs as much as a detour this many steps long
    int32_t Cut {}; // Stop the search when within this distance of target; 0 = must reach exact target
    int32_t Multihex {}; // Multihex radius; 0 = single hex; >0 = directional perimeter check per step
    bool FreeMovement {}; // Use LineTracer optimization for control steps and continuous end offset
    function<bool(mpos)> CheckTarget {}; // Optional exact multi-target predicate; replaces ToHex/Cut when set
    function<HexBlockResult(mpos)> CheckHex {}; // Check if a single hex blocks movement
};

struct FindPathOutput
{
    enum class ResultType : int8_t
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
    vector<mdir> Steps {};
    vector<uint16_t> ControlSteps {};
    mpos NewToHex {};
    ipos16 EndHexOffset {}; // FreeMovement sub-hex stop offset relative to NewToHex center; zero when FreeMovement is off or the real target coincides with NewToHex center
};

struct FindReachableInput
{
    mpos FromHex {};
    msize MapSize {};
    int32_t MaxLength {}; // Longest route allowed, in steps (from engine Settings.Geometry.MaxPathFindLength)
    const_span<mpos> TargetHexes {}; // Hexes to answer for; the flood stops once every one of them is reached
    function<HexBlockResult(mpos)> CheckHex {}; // Check if a single hex blocks movement
};

struct TraceLineInput
{
    mpos StartHex {};
    mpos TargetHex {};
    int32_t MaxDist {}; // 0 means use distance (start, target)
    float32_t Angle {};
    msize MapSize {};
    bool CheckLastMovable {};
    function<bool(mpos)> IsHexBlocked {}; // Return true if hex is blocked (stops trace)
    function<bool(mpos)> IsHexMovable {}; // Optional: return true if hex is movable
};

struct TraceLineOutput
{
    bool FullyTraced {};
    bool HasLastMovable {};
    mpos PreBlock {};
    mpos Block {};
    mpos LastMovable {};
};

struct TraceDirectionInput
{
    mpos StartHex {};
    ipos16 StartHexOffset {}; // The ray starts where the mover is drawn, not at its hex centre
    // A point the ray passes through, onto which the end is projected back: the start of a fresh direction, or where
    // an extended one was first held, so rounding never accumulates along a chain of traces
    mpos RayHex {};
    ipos16 RayHexOffset {};
    mdir Dir {};
    int32_t MaxSteps {};
    int32_t Multihex {};
    bool Slide {}; // Step round a blocked hex toward the direction instead of stopping at it
    msize MapSize {};
    function<HexBlockResult(mpos)> CheckHex {}; // Anything but Passable blocks: a held direction does not route through gags or critters
};

struct TraceDirectionOutput
{
    vector<mdir> Steps {};
    vector<uint16_t> ControlSteps {};
    ipos16 EndHexOffset {}; // Puts the end of a trace that never left the ray back on the ray; zero once it slid
    bool Slid {};
};

namespace PathFinding
{
    // Check the movement-direction front arc for a single hex or multihex perimeter.
    // Return the worst result in Blocked > DeferCritter > DeferGag > Passable order
    auto CheckHexWithMultihex(mpos hex, mdir dir, int32_t multihex, msize map_size, const function<HexBlockResult(mpos)>& check_hex) -> HexBlockResult;

    // Core pathfinding algorithm (A* with deferred routing through gags/critters)
    auto FindPath(const FindPathInput& input) -> FindPathOutput;

    // Single-hex flood from FromHex over every hex CheckHex does not block, at most MaxLength steps out.
    // Return the target hexes it reaches, in the order given
    auto FindReachable(const FindReachableInput& input) -> vector<mpos>;

    // Compute the half-hex-clamped FreeMovement endpoint relative to the target's real offset.
    // Return nullopt for an undefined stop direction so callers preserve the mover's offset
    auto EvaluateFreeMovementEndOffset(mpos new_to_hex, mpos to_hex, ipos16 to_hex_offset) -> optional<ipos16>;

    // Core line trace from start toward target, stopping at blocked hexes
    auto TraceLine(const TraceLineInput& input) -> TraceLineOutput;

    // Trace a held direction along its ray, optionally sliding past what blocks it. A trace that stays on the ray
    // ends on it, so consecutive traces draw one straight line; a side step never goes against the direction
    auto TraceDirection(const TraceDirectionInput& input) -> TraceDirectionOutput;
}

FO_END_NAMESPACE
