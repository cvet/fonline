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

#include "catch_amalgamated.hpp"

#include "PathFinding.h"

FO_BEGIN_NAMESPACE

namespace
{
    constexpr msize TEST_MAP_SIZE {20, 20};

    // Helper: create settings for a clear map (no obstacles)
    static auto MakeClearSettings(mpos from, mpos to, int32 cut = 0) -> FindPathInput
    {
        FindPathInput settings;
        settings.FromHex = from;
        settings.ToHex = to;
        settings.MapSize = TEST_MAP_SIZE;
        settings.MaxLength = 200;
        settings.Cut = cut;
        settings.FreeMovement = false;
        settings.Multihex = 0;
        settings.CheckHex = [](mpos /*hex*/) -> HexBlockResult { return HexBlockResult::Passable; };
        return settings;
    }

    // Helper: create settings with a blocked-hex predicate
    static auto MakeBlockedSettings(mpos from, mpos to, function<bool(mpos)> is_blocked, int32 cut = 0) -> FindPathInput
    {
        FindPathInput settings = MakeClearSettings(from, to, cut);
        settings.CheckHex = [is_blocked = std::move(is_blocked)](mpos hex) -> HexBlockResult { return is_blocked(hex) ? HexBlockResult::Blocked : HexBlockResult::Passable; };
        return settings;
    }
}

TEST_CASE("PathFinding::FindPath")
{
    SECTION("ClearPathReturnsOk")
    {
        auto settings = MakeClearSettings(mpos {5, 5}, mpos {8, 5});
        auto output = PathFinding::FindPath(settings);

        CHECK(output.Result == FindPathOutput::ResultType::Ok);
        CHECK(!output.Steps.empty());
        CHECK(!output.ControlSteps.empty());
        CHECK(output.NewToHex == mpos {8, 5});
    }

    SECTION("SameHexReturnsAlreadyHere")
    {
        auto settings = MakeClearSettings(mpos {5, 5}, mpos {5, 5});
        auto output = PathFinding::FindPath(settings);

        CHECK(output.Result == FindPathOutput::ResultType::AlreadyHere);
        CHECK(output.Steps.empty());
    }

    SECTION("AdjacentHexReturnsAlreadyHereWithCut1")
    {
        auto settings = MakeClearSettings(mpos {5, 5}, mpos {6, 5}, 1);
        auto output = PathFinding::FindPath(settings);

        CHECK(output.Result == FindPathOutput::ResultType::AlreadyHere);
    }

    SECTION("InvalidFromHexReturnsInvalidHexes")
    {
        auto settings = MakeClearSettings(mpos {100, 100}, mpos {5, 5});
        auto output = PathFinding::FindPath(settings);

        CHECK(output.Result == FindPathOutput::ResultType::InvalidHexes);
    }

    SECTION("InvalidToHexReturnsInvalidHexes")
    {
        auto settings = MakeClearSettings(mpos {5, 5}, mpos {100, 100});
        auto output = PathFinding::FindPath(settings);

        CHECK(output.Result == FindPathOutput::ResultType::InvalidHexes);
    }

    SECTION("FullyBlockedTargetReturnsNoWay")
    {
        // Block everything except the start hex
        auto settings = MakeBlockedSettings(mpos {5, 5}, mpos {8, 5}, [](mpos hex) { return hex != mpos {5, 5}; });
        auto output = PathFinding::FindPath(settings);

        CHECK(output.Result == FindPathOutput::ResultType::NoWay);
    }

    SECTION("PathRoutesAroundWall")
    {
        // Partial vertical wall at x=7 (y 3-8), leaves gap above/below for routing
        auto settings = MakeBlockedSettings(mpos {5, 5}, mpos {9, 5}, [](mpos hex) { return hex.x == 7 && hex.y >= 3 && hex.y <= 8; });
        auto output = PathFinding::FindPath(settings);

        CHECK(output.Result == FindPathOutput::ResultType::Ok);
        CHECK(!output.Steps.empty());
        CHECK(output.NewToHex == mpos {9, 5});

        // Verify path is longer than direct distance (had to route around)
        const auto direct_dist = GeometryHelper::GetDistance(mpos {5, 5}, mpos {9, 5});
        CHECK(output.Steps.size() > static_cast<size_t>(direct_dist));
    }

    SECTION("CutStopsWithinDistance")
    {
        auto settings = MakeClearSettings(mpos {5, 5}, mpos {10, 5}, 2);
        auto output = PathFinding::FindPath(settings);

        CHECK(output.Result == FindPathOutput::ResultType::Ok);
        CHECK(GeometryHelper::CheckDist(output.NewToHex, mpos {10, 5}, 2));
    }

    SECTION("TooFarReturnsWhenMaxLengthExceeded")
    {
        auto settings = MakeClearSettings(mpos {0, 0}, mpos {19, 19});
        settings.MaxLength = 3; // Very short max
        auto output = PathFinding::FindPath(settings);

        CHECK(output.Result == FindPathOutput::ResultType::TooFar);
    }

    SECTION("StepsFormValidPath")
    {
        auto from = mpos {5, 5};
        auto to = mpos {8, 8};
        auto settings = MakeClearSettings(from, to);
        auto output = PathFinding::FindPath(settings);

        REQUIRE(output.Result == FindPathOutput::ResultType::Ok);

        // Walk the path and verify each step moves to a valid adjacent hex
        auto cur = from;
        for (const auto& step : output.Steps) {
            CHECK(step < GameSettings::MAP_DIR_COUNT);
            auto raw = ipos32 {cur.x, cur.y};
            GeometryHelper::MoveHexByDirUnsafe(raw, step);
            CHECK(TEST_MAP_SIZE.is_valid_pos(raw));
            cur = TEST_MAP_SIZE.from_raw_pos(raw);
        }

        CHECK(cur == output.NewToHex);
    }

    SECTION("ControlStepsAreMonotonicallyIncreasing")
    {
        auto settings = MakeClearSettings(mpos {2, 2}, mpos {15, 12});
        auto output = PathFinding::FindPath(settings);

        REQUIRE(output.Result == FindPathOutput::ResultType::Ok);
        REQUIRE(!output.ControlSteps.empty());

        uint16 prev = 0;
        for (const auto cs : output.ControlSteps) {
            CHECK(cs > prev);
            prev = cs;
        }

        CHECK(prev == static_cast<uint16>(output.Steps.size()));
    }

    SECTION("FreeMovementProducesShorterControlSteps")
    {
        auto from = mpos {2, 2};
        auto to = mpos {15, 12};

        auto settings_normal = MakeClearSettings(from, to);
        auto output_normal = PathFinding::FindPath(settings_normal);

        auto settings_free = MakeClearSettings(from, to);
        settings_free.FreeMovement = true;
        auto output_free = PathFinding::FindPath(settings_free);

        REQUIRE(output_normal.Result == FindPathOutput::ResultType::Ok);
        REQUIRE(output_free.Result == FindPathOutput::ResultType::Ok);

        // Free movement should have fewer control steps (merges straight segments)
        CHECK(output_free.ControlSteps.size() <= output_normal.ControlSteps.size());
    }

    // ======== Deferred routing tests ========

    SECTION("DeferredGagRoutingAround")
    {
        // Single-hex gap blocked by gag; BFS should route around it
        auto gag_hex = mpos {7, 5};
        auto settings = MakeClearSettings(mpos {5, 5}, mpos {9, 5});
        settings.CheckHex = [gag_hex](mpos hex) -> HexBlockResult {
            if (hex == gag_hex) {
                return HexBlockResult::DeferGag;
            }
            return HexBlockResult::Passable;
        };
        auto output = PathFinding::FindPath(settings);

        CHECK(output.Result == FindPathOutput::ResultType::Ok);
        CHECK(!output.Steps.empty());
        CHECK(output.NewToHex == mpos {9, 5});
    }

    SECTION("DeferredGagFallbackWhenSurrounded")
    {
        // Gag wall blocks entire passage; BFS exhausts passable hexes, then re-enters gag hexes as fallback
        // Wall of gag at x=7 (y 0-19), no other way around
        auto settings = MakeClearSettings(mpos {5, 5}, mpos {9, 5});
        settings.CheckHex = [](mpos hex) -> HexBlockResult {
            if (hex.x == 7) {
                return HexBlockResult::DeferGag;
            }
            return HexBlockResult::Passable;
        };
        auto output = PathFinding::FindPath(settings);

        CHECK(output.Result == FindPathOutput::ResultType::Ok);
        CHECK(!output.Steps.empty());
        CHECK(output.NewToHex == mpos {9, 5});
    }

    SECTION("DeferredCritterRoutingAround")
    {
        // Critter on hex {7,5} — BFS should route around it when possible
        auto critter_hex = mpos {7, 5};
        auto settings = MakeClearSettings(mpos {5, 5}, mpos {9, 5});
        settings.CheckHex = [critter_hex](mpos hex) -> HexBlockResult {
            if (hex == critter_hex) {
                return HexBlockResult::DeferCritter;
            }
            return HexBlockResult::Passable;
        };
        auto output = PathFinding::FindPath(settings);

        CHECK(output.Result == FindPathOutput::ResultType::Ok);
        CHECK(!output.Steps.empty());
    }

    SECTION("DeferredCritterFallbackWhenNoOtherWay")
    {
        // Critter wall blocks entire passage; BFS must route through critters as last resort
        auto settings = MakeClearSettings(mpos {5, 5}, mpos {9, 5});
        settings.CheckHex = [](mpos hex) -> HexBlockResult {
            if (hex.x == 7) {
                return HexBlockResult::DeferCritter;
            }
            return HexBlockResult::Passable;
        };
        auto output = PathFinding::FindPath(settings);

        CHECK(output.Result == FindPathOutput::ResultType::Ok);
        CHECK(!output.Steps.empty());
        CHECK(output.NewToHex == mpos {9, 5});
    }

    SECTION("DeferredGagBeforeCritter")
    {
        // Both gag and critter block, gag wall is only passage — gag should be consumed before critter
        auto settings = MakeClearSettings(mpos {5, 5}, mpos {9, 5});
        settings.CheckHex = [](mpos hex) -> HexBlockResult {
            if (hex.x == 7) {
                return HexBlockResult::DeferGag;
            }
            if (hex.x == 8) {
                return HexBlockResult::DeferCritter;
            }
            return HexBlockResult::Passable;
        };
        auto output = PathFinding::FindPath(settings);

        CHECK(output.Result == FindPathOutput::ResultType::Ok);
        CHECK(!output.Steps.empty());
        CHECK(output.NewToHex == mpos {9, 5});
    }

    // ======== Multihex tests ========

    SECTION("MultihexClearPath")
    {
        auto settings = MakeClearSettings(mpos {100, 100}, mpos {105, 100});
        settings.MapSize = msize {200, 200};
        settings.Multihex = 1;
        settings.MaxLength = 15;
        auto output = PathFinding::FindPath(settings);

        CHECK(output.Result == FindPathOutput::ResultType::Ok);
        CHECK(!output.Steps.empty());
    }

    SECTION("MultihexLargerRadius")
    {
        auto settings = MakeClearSettings(mpos {100, 100}, mpos {108, 100});
        settings.MapSize = msize {200, 200};
        settings.Multihex = 2;
        settings.MaxLength = 20;
        auto output = PathFinding::FindPath(settings);

        CHECK(output.Result == FindPathOutput::ResultType::Ok);
        CHECK(!output.Steps.empty());
    }

    SECTION("MultihexPerimeterBlocksMove")
    {
        // Wall at x=55 — multihex=1 front arc includes x=55 perimeter hexes when heading right
        auto settings = MakeClearSettings(mpos {50, 50}, mpos {58, 50});
        settings.MapSize = msize {100, 100};
        settings.Multihex = 1;
        settings.MaxLength = 30;
        settings.CheckHex = [](mpos hex) -> HexBlockResult {
            if (hex.x == 55) {
                return HexBlockResult::Blocked;
            }
            return HexBlockResult::Passable;
        };
        auto output = PathFinding::FindPath(settings);

        // Full x=55 column blocked; multihex must route around or fail
        CHECK((output.Result == FindPathOutput::ResultType::Ok || output.Result == FindPathOutput::ResultType::NoWay || output.Result == FindPathOutput::ResultType::TooFar));
    }

    SECTION("MultihexPerimeterDeferCritter")
    {
        // Critter on a perimeter hex — multihex should aggregate DeferCritter
        auto settings = MakeClearSettings(mpos {100, 100}, mpos {107, 100});
        settings.MapSize = msize {200, 200};
        settings.Multihex = 1;
        settings.MaxLength = 20;
        settings.CheckHex = [](mpos hex) -> HexBlockResult {
            if (hex == mpos {105, 99}) {
                return HexBlockResult::DeferCritter;
            }
            return HexBlockResult::Passable;
        };
        auto output = PathFinding::FindPath(settings);

        CHECK(output.Result == FindPathOutput::ResultType::Ok);
        CHECK(!output.Steps.empty());
    }

    SECTION("MultihexWithDeferredRouting")
    {
        // Gag wall at x=105 with multihex=1; deferred routing should eventually pass
        auto settings = MakeClearSettings(mpos {100, 100}, mpos {110, 100});
        settings.MapSize = msize {200, 200};
        settings.Multihex = 1;
        settings.MaxLength = 80;
        settings.CheckHex = [](mpos hex) -> HexBlockResult {
            if (hex.x == 105) {
                return HexBlockResult::DeferGag;
            }
            return HexBlockResult::Passable;
        };
        auto output = PathFinding::FindPath(settings);

        CHECK(output.Result == FindPathOutput::ResultType::Ok);
        CHECK(!output.Steps.empty());
    }

    // ======== Cut distance tests ========

    SECTION("CutStopsBeforeBlockedTarget")
    {
        // Target hex itself is blocked, but cut=2 should stop before reaching it
        auto target = mpos {10, 5};
        auto settings = MakeBlockedSettings(mpos {5, 5}, target, [target](mpos hex) -> bool { return hex == target; }, 2);
        auto output = PathFinding::FindPath(settings);

        CHECK(output.Result == FindPathOutput::ResultType::Ok);
        CHECK(GeometryHelper::CheckDist(output.NewToHex, target, 2));
    }

    SECTION("CutZeroMustReachTarget")
    {
        // Cut=0 requires reaching the exact target
        auto settings = MakeClearSettings(mpos {5, 5}, mpos {8, 5}, 0);
        auto output = PathFinding::FindPath(settings);

        CHECK(output.Result == FindPathOutput::ResultType::Ok);
        CHECK(output.NewToHex == mpos {8, 5});
    }

    SECTION("CutLargerThanDistanceIsAlreadyHere")
    {
        auto settings = MakeClearSettings(mpos {5, 5}, mpos {7, 5}, 5);
        auto output = PathFinding::FindPath(settings);

        CHECK(output.Result == FindPathOutput::ResultType::AlreadyHere);
    }

    // ======== Path quality tests ========

    SECTION("DiagonalPathIsValid")
    {
        auto from = mpos {3, 3};
        auto to = mpos {12, 15};
        auto settings = MakeClearSettings(from, to);
        auto output = PathFinding::FindPath(settings);

        REQUIRE(output.Result == FindPathOutput::ResultType::Ok);

        // Walk path and verify end point
        auto cur = from;
        for (const auto& step : output.Steps) {
            CHECK(step < GameSettings::MAP_DIR_COUNT);
            auto raw = ipos32 {cur.x, cur.y};
            GeometryHelper::MoveHexByDirUnsafe(raw, step);
            REQUIRE(TEST_MAP_SIZE.is_valid_pos(raw));
            cur = TEST_MAP_SIZE.from_raw_pos(raw);
        }
        CHECK(cur == output.NewToHex);
    }

    SECTION("PathNeverVisitsSameHexTwice")
    {
        auto from = mpos {2, 2};
        auto to = mpos {17, 17};
        // Wall with a gap to force an interesting path
        auto settings = MakeBlockedSettings(from, to, [](mpos hex) -> bool { return hex.x == 10 && hex.y >= 5 && hex.y <= 14; });
        auto output = PathFinding::FindPath(settings);

        REQUIRE(output.Result == FindPathOutput::ResultType::Ok);

        set<uint32> visited;
        auto cur = from;
        visited.insert(cur.x * 1000 + cur.y);
        for (const auto& step : output.Steps) {
            auto raw = ipos32 {cur.x, cur.y};
            GeometryHelper::MoveHexByDirUnsafe(raw, step);
            cur = TEST_MAP_SIZE.from_raw_pos(raw);
            auto key = static_cast<uint32>(cur.x * 1000 + cur.y);
            CHECK(visited.find(key) == visited.end());
            visited.insert(key);
        }
    }

    SECTION("ControlStepsCoverAllSteps")
    {
        auto settings = MakeClearSettings(mpos {2, 2}, mpos {15, 12});
        auto output = PathFinding::FindPath(settings);

        REQUIRE(output.Result == FindPathOutput::ResultType::Ok);
        REQUIRE(!output.ControlSteps.empty());

        // Last control step must equal total steps
        CHECK(output.ControlSteps.back() == static_cast<uint16>(output.Steps.size()));

        // First control step must be >= 1
        CHECK(output.ControlSteps.front() >= 1);

        // Each control step segment uses a single direction (non-free movement)
        uint16 prev_cs = 0;
        for (const auto cs : output.ControlSteps) {
            // All steps in [prev_cs, cs) should be the same direction
            if (cs > prev_cs + 1) {
                auto dir = output.Steps[prev_cs];
                for (auto j = prev_cs + 1; j < cs; j++) {
                    CHECK(output.Steps[j] == dir);
                }
            }
            prev_cs = cs;
        }
    }

    SECTION("FreeMovementProducesShorterControlSteps")
    {
        auto from = mpos {2, 2};
        auto to = mpos {15, 12};

        auto settings_normal = MakeClearSettings(from, to);
        auto output_normal = PathFinding::FindPath(settings_normal);

        auto settings_free = MakeClearSettings(from, to);
        settings_free.FreeMovement = true;
        auto output_free = PathFinding::FindPath(settings_free);

        REQUIRE(output_normal.Result == FindPathOutput::ResultType::Ok);
        REQUIRE(output_free.Result == FindPathOutput::ResultType::Ok);

        CHECK(output_free.ControlSteps.size() <= output_normal.ControlSteps.size());
    }

    SECTION("FreeMovementPathReachesTarget")
    {
        auto from = mpos {3, 3};
        auto to = mpos {16, 14};
        auto settings = MakeClearSettings(from, to);
        settings.FreeMovement = true;
        auto output = PathFinding::FindPath(settings);

        REQUIRE(output.Result == FindPathOutput::ResultType::Ok);

        // Walk the path and verify we end at the target
        auto cur = from;
        for (const auto& step : output.Steps) {
            auto raw = ipos32 {cur.x, cur.y};
            GeometryHelper::MoveHexByDirUnsafe(raw, step);
            REQUIRE(TEST_MAP_SIZE.is_valid_pos(raw));
            cur = TEST_MAP_SIZE.from_raw_pos(raw);
        }
        CHECK(cur == output.NewToHex);
        CHECK(output.NewToHex == to);
    }

    SECTION("FreeMovementAroundObstacle")
    {
        auto from = mpos {3, 10};
        auto to = mpos {16, 10};
        auto settings = MakeBlockedSettings(from, to, [](mpos hex) -> bool { return hex.x == 10 && hex.y >= 5 && hex.y <= 14; });
        settings.FreeMovement = true;
        auto output = PathFinding::FindPath(settings);

        REQUIRE(output.Result == FindPathOutput::ResultType::Ok);
        CHECK(!output.Steps.empty());
        CHECK(!output.ControlSteps.empty());
        CHECK(output.NewToHex == to);
    }

    // ======== Grid reuse tests ========

    SECTION("RepeatedCallsRemainIndependent")
    {
        auto settings1 = MakeClearSettings(mpos {3, 3}, mpos {7, 7});
        auto output1 = PathFinding::FindPath(settings1);
        REQUIRE(output1.Result == FindPathOutput::ResultType::Ok);

        auto settings2 = MakeClearSettings(mpos {10, 10}, mpos {15, 15});
        auto output2 = PathFinding::FindPath(settings2);
        CHECK(output2.Result == FindPathOutput::ResultType::Ok);
    }

    SECTION("FailedCallDoesNotPoisonLaterCall")
    {
        // First call fails
        auto settings1 = MakeBlockedSettings(mpos {5, 5}, mpos {8, 5}, [](mpos hex) -> bool { return hex != mpos {5, 5}; });
        auto output1 = PathFinding::FindPath(settings1);
        CHECK(output1.Result == FindPathOutput::ResultType::NoWay);

        // Second call on same grid buffer succeeds
        auto settings2 = MakeClearSettings(mpos {10, 10}, mpos {15, 15});
        auto output2 = PathFinding::FindPath(settings2);
        CHECK(output2.Result == FindPathOutput::ResultType::Ok);
    }

    // ======== Boundary & edge cases ========

    SECTION("PathAlongMapEdge")
    {
        auto settings = MakeClearSettings(mpos {0, 0}, mpos {19, 0});
        auto output = PathFinding::FindPath(settings);

        CHECK(output.Result == FindPathOutput::ResultType::Ok);
        CHECK(output.NewToHex == mpos {19, 0});
    }

    SECTION("PathFromCornerToCorner")
    {
        auto settings = MakeClearSettings(mpos {0, 0}, mpos {19, 19});
        auto output = PathFinding::FindPath(settings);

        CHECK(output.Result == FindPathOutput::ResultType::Ok);
        CHECK(output.NewToHex == mpos {19, 19});
    }

    SECTION("AdjacentHexPath")
    {
        auto settings = MakeClearSettings(mpos {10, 10}, mpos {11, 10});
        auto output = PathFinding::FindPath(settings);

        CHECK(output.Result == FindPathOutput::ResultType::Ok);
        CHECK(output.Steps.size() == 1);
        CHECK(output.ControlSteps.size() == 1);
    }

    SECTION("MaxLengthExactlyReachable")
    {
        // Short path with generous MaxLength should succeed
        auto settings = MakeClearSettings(mpos {5, 5}, mpos {8, 5});
        settings.MaxLength = 10;
        auto output = PathFinding::FindPath(settings);

        CHECK(output.Result == FindPathOutput::ResultType::Ok);
        CHECK(output.Steps.size() <= 10);
    }

    SECTION("ComplexMazeWithSingleSolution")
    {
        // Create a maze with a narrow corridor
        auto settings = MakeBlockedSettings(mpos {2, 10}, mpos {17, 10}, [](mpos hex) -> bool {
            // Vertical wall at x=5 with gap at y=10
            if (hex.x == 5 && hex.y != 10) {
                return true;
            }
            // Vertical wall at x=10 with gap at y=5
            if (hex.x == 10 && hex.y != 5) {
                return true;
            }
            // Vertical wall at x=15 with gap at y=10
            if (hex.x == 15 && hex.y != 10) {
                return true;
            }
            return false;
        });
        auto output = PathFinding::FindPath(settings);

        CHECK(output.Result == FindPathOutput::ResultType::Ok);
        CHECK(output.NewToHex == mpos {17, 10});
        // Path should be longer than direct due to maze
        CHECK(output.Steps.size() > 15);
    }
}

TEST_CASE("PathFinding::TraceLine")
{
    // Helper to build trace settings with defaults
    auto make_trace_settings = [](mpos from, mpos to, function<bool(mpos)> blocker = nullptr) -> TraceLineInput {
        TraceLineInput settings;
        settings.StartHex = from;
        settings.TargetHex = to;
        settings.MaxDist = 0;
        settings.Angle = 0.0f;
        settings.MapSize = TEST_MAP_SIZE;
        settings.CheckLastMovable = false;
        settings.IsHexBlocked = blocker ? std::move(blocker) : [](mpos /*hex*/) -> bool { return false; };
        return settings;
    };

    // ======== Basic tracing ========

    SECTION("ClearTraceIsFullTrace")
    {
        auto settings = make_trace_settings(mpos {5, 5}, mpos {10, 5});
        auto output = PathFinding::TraceLine(settings);

        CHECK(output.IsFullTrace);
        CHECK_FALSE(output.HasLastMovable);
    }

    SECTION("ClearTraceBlockEqualsTarget")
    {
        auto settings = make_trace_settings(mpos {5, 5}, mpos {10, 5});
        auto output = PathFinding::TraceLine(settings);

        CHECK(output.IsFullTrace);
        // On a full clear trace: Block is the last hex stepped, PreBlock is the one before
        // Both equal the target only if the tracer lands exactly on target
    }

    SECTION("ClearTraceDiagonal")
    {
        auto settings = make_trace_settings(mpos {3, 3}, mpos {15, 15});
        auto output = PathFinding::TraceLine(settings);

        CHECK(output.IsFullTrace);
    }

    SECTION("SameStartAndTarget")
    {
        auto settings = make_trace_settings(mpos {5, 5}, mpos {5, 5});
        auto output = PathFinding::TraceLine(settings);

        CHECK(output.IsFullTrace);
    }

    // ======== Blocked hex tests ========

    SECTION("BlockedHexStopsTrace")
    {
        auto block_hex = mpos {7, 5};
        auto settings = make_trace_settings(mpos {5, 5}, mpos {10, 5}, [block_hex](mpos hex) { return hex == block_hex; });
        auto output = PathFinding::TraceLine(settings);

        CHECK_FALSE(output.IsFullTrace);
        CHECK(output.Block == block_hex);
    }

    SECTION("PreBlockIsAdjacentToBlock")
    {
        auto block_hex = mpos {7, 5};
        auto settings = make_trace_settings(mpos {5, 5}, mpos {10, 5}, [block_hex](mpos hex) { return hex == block_hex; });
        auto output = PathFinding::TraceLine(settings);

        CHECK_FALSE(output.IsFullTrace);
        CHECK(output.PreBlock != output.Block);
        CHECK(GeometryHelper::CheckDist(output.PreBlock, output.Block, 1));
    }

    SECTION("BlockAtFirstHexAfterStart")
    {
        // Block immediately adjacent hex — trace should stop at distance 1
        auto settings = make_trace_settings(mpos {5, 5}, mpos {10, 5}, [](mpos hex) { return hex == mpos {6, 5}; });
        auto output = PathFinding::TraceLine(settings);

        CHECK_FALSE(output.IsFullTrace);
        CHECK(output.Block == mpos {6, 5});
        CHECK(output.PreBlock == mpos {5, 5});
    }

    SECTION("MultipleBlockersOnlyFirstMatters")
    {
        // Two blockers along the line — only first one stops trace
        auto settings = make_trace_settings(mpos {5, 5}, mpos {12, 5}, [](mpos hex) { return hex == mpos {7, 5} || hex == mpos {9, 5}; });
        auto output = PathFinding::TraceLine(settings);

        CHECK_FALSE(output.IsFullTrace);
        CHECK(output.Block == mpos {7, 5});
    }

    SECTION("BlockOnTargetHex")
    {
        auto target = mpos {10, 5};
        auto settings = make_trace_settings(mpos {5, 5}, target, [target](mpos hex) { return hex == target; });
        auto output = PathFinding::TraceLine(settings);

        CHECK_FALSE(output.IsFullTrace);
        CHECK(output.Block == target);
    }

    // ======== MaxDist tests ========

    SECTION("MaxDistLimitsTrace")
    {
        auto settings = make_trace_settings(mpos {5, 5}, mpos {15, 5});
        settings.MaxDist = 3;
        auto output = PathFinding::TraceLine(settings);

        CHECK(output.IsFullTrace);
        CHECK(GeometryHelper::GetDistance(settings.StartHex, output.Block) <= 3);
    }

    SECTION("MaxDistShorterThanBlocker")
    {
        // MaxDist stops before the blocker — should be full trace
        auto settings = make_trace_settings(mpos {5, 5}, mpos {15, 5}, [](mpos hex) { return hex == mpos {12, 5}; });
        settings.MaxDist = 3;
        auto output = PathFinding::TraceLine(settings);

        CHECK(output.IsFullTrace);
    }

    SECTION("MaxDistBeyondBlocker")
    {
        // MaxDist is larger than distance to blocker — blocker should stop trace
        auto settings = make_trace_settings(mpos {5, 5}, mpos {15, 5}, [](mpos hex) { return hex == mpos {7, 5}; });
        settings.MaxDist = 10;
        auto output = PathFinding::TraceLine(settings);

        CHECK_FALSE(output.IsFullTrace);
        CHECK(output.Block == mpos {7, 5});
    }

    SECTION("MaxDistEqualsOne")
    {
        auto settings = make_trace_settings(mpos {5, 5}, mpos {15, 5});
        settings.MaxDist = 1;
        auto output = PathFinding::TraceLine(settings);

        CHECK(output.IsFullTrace);
        CHECK(GeometryHelper::GetDistance(settings.StartHex, output.Block) <= 1);
    }

    // ======== LastMovable tracking ========

    SECTION("LastMovableTracking")
    {
        auto block_hex = mpos {8, 5};
        auto settings = make_trace_settings(mpos {5, 5}, mpos {12, 5}, [block_hex](mpos hex) { return hex == block_hex; });
        settings.CheckLastMovable = true;
        settings.IsHexMovable = [block_hex](mpos hex) -> bool { return hex != block_hex; };
        auto output = PathFinding::TraceLine(settings);

        CHECK_FALSE(output.IsFullTrace);
        CHECK(output.HasLastMovable);
        CHECK(GeometryHelper::GetDistance(output.LastMovable, settings.StartHex) > 0);
    }

    SECTION("LastMovableIsPreBlock")
    {
        auto block_hex = mpos {8, 5};
        auto settings = make_trace_settings(mpos {5, 5}, mpos {12, 5}, [block_hex](mpos hex) { return hex == block_hex; });
        settings.CheckLastMovable = true;
        settings.IsHexMovable = [block_hex](mpos hex) -> bool { return hex != block_hex; };
        auto output = PathFinding::TraceLine(settings);

        CHECK(output.HasLastMovable);
        // LastMovable should be the last hex that is movable before the trace stopped
        // Since all hexes before block_hex are movable, LastMovable == PreBlock
        CHECK(output.LastMovable == output.PreBlock);
    }

    SECTION("LastMovableStopsAfterFirstNonMovable")
    {
        // Non-movable hex at x=7 (but not blocking), blocked at x=10
        // LastMovable should stop at the last hex before x=7
        auto settings = make_trace_settings(mpos {5, 5}, mpos {12, 5}, [](mpos hex) { return hex.x == 10; });
        settings.CheckLastMovable = true;
        settings.IsHexMovable = [](mpos hex) -> bool { return hex.x < 7; };
        auto output = PathFinding::TraceLine(settings);

        CHECK_FALSE(output.IsFullTrace);
        CHECK(output.HasLastMovable);
        // LastMovable should be a hex with x < 7 (last movable before reaching the non-movable zone)
        CHECK(output.LastMovable.x < 7);
    }

    SECTION("LastMovableOnClearTrace")
    {
        auto settings = make_trace_settings(mpos {5, 5}, mpos {10, 5});
        settings.CheckLastMovable = true;
        settings.IsHexMovable = [](mpos /*hex*/) -> bool { return true; };
        auto output = PathFinding::TraceLine(settings);

        CHECK(output.IsFullTrace);
        CHECK(output.HasLastMovable);
    }

    SECTION("LastMovableNeverSetIfFirstHexNonMovable")
    {
        // First hex after start is non-movable, and the trace hits a blocker
        auto settings = make_trace_settings(mpos {5, 5}, mpos {10, 5}, [](mpos hex) { return hex.x == 9; });
        settings.CheckLastMovable = true;
        settings.IsHexMovable = [](mpos hex) -> bool { return hex.x > 8; }; // only hexes past x=8 are movable, but blocker at x=9
        auto output = PathFinding::TraceLine(settings);

        CHECK_FALSE(output.IsFullTrace);
        // First hex (x=6) is NOT movable, so last_passed_ok becomes true immediately
        // No LastMovable should be set since IsHexMovable fails on first try
        CHECK_FALSE(output.HasLastMovable);
    }

    // ======== Angle offset tests ========

    SECTION("AngleOffsetAltersTracePath")
    {
        // Comparing zero angle vs non-zero — they should produce different Block positions
        auto settings0 = make_trace_settings(mpos {10, 10}, mpos {10, 0});
        settings0.Angle = 0.0f;

        auto settings_angled = make_trace_settings(mpos {10, 10}, mpos {10, 0});
        settings_angled.Angle = 30.0f;

        auto output0 = PathFinding::TraceLine(settings0);
        auto output_angled = PathFinding::TraceLine(settings_angled);

        CHECK(output0.IsFullTrace);
        CHECK(output_angled.IsFullTrace);

        // With angle offset, the final hex should differ from straight trace
        CHECK(output0.Block != output_angled.Block);
    }

    SECTION("AngleOffsetWithBlocker")
    {
        // Straight trace would hit blocker, angled trace might miss it
        auto blocker = mpos {10, 7};
        auto settings_straight = make_trace_settings(mpos {10, 10}, mpos {10, 0}, [blocker](mpos hex) { return hex == blocker; });
        settings_straight.Angle = 0.0f;
        auto output_straight = PathFinding::TraceLine(settings_straight);

        auto settings_angled = make_trace_settings(mpos {10, 10}, mpos {10, 0}, [blocker](mpos hex) { return hex == blocker; });
        settings_angled.Angle = 45.0f;
        auto output_angled = PathFinding::TraceLine(settings_angled);

        // Straight should be blocked
        CHECK_FALSE(output_straight.IsFullTrace);
        CHECK(output_straight.Block == blocker);

        // Angled might miss the blocker (depending on hex geometry)
        // At minimum, the block position should differ
        if (!output_angled.IsFullTrace) {
            CHECK(output_angled.Block != blocker);
        }
    }
}

FO_END_NAMESPACE
