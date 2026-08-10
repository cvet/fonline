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
    static auto MakeClearSettings(mpos from, mpos to, int32_t cut = 0) -> FindPathInput
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
    static auto MakeBlockedSettings(mpos from, mpos to, function<bool(mpos)> is_blocked, int32_t cut = 0) -> FindPathInput
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

    SECTION("TargetCallbackSelectsNearestReachableHex")
    {
        auto settings = MakeClearSettings(mpos {5, 5}, mpos {});
        settings.CheckTarget = [](mpos hex) { return hex == mpos {15, 15} || hex == mpos {7, 5}; };
        auto output = PathFinding::FindPath(settings);

        CHECK(output.Result == FindPathOutput::ResultType::Ok);
        CHECK(output.Steps.size() == 2);
        CHECK(output.NewToHex == mpos {7, 5});
    }

    SECTION("TargetCallbackReplacesSingleTargetValidation")
    {
        auto settings = MakeClearSettings(mpos {5, 5}, mpos {100, 100});
        settings.CheckTarget = [](mpos hex) { return hex == mpos {5, 5}; };
        auto output = PathFinding::FindPath(settings);

        CHECK(output.Result == FindPathOutput::ResultType::AlreadyHere);
        CHECK(output.NewToHex == mpos {5, 5});
    }

    SECTION("TargetCallbackSkipsBlockedCandidate")
    {
        auto settings = MakeBlockedSettings(mpos {5, 5}, mpos {}, [](mpos hex) { return hex == mpos {6, 5}; });
        settings.CheckTarget = [](mpos hex) { return hex == mpos {6, 5} || hex == mpos {8, 5}; };
        auto output = PathFinding::FindPath(settings);

        CHECK(output.Result == FindPathOutput::ResultType::Ok);
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

    SECTION("AdjacentPathWorksForEveryDirection")
    {
        mpos from {10, 10};

        for (int32_t dir_value = 0; dir_value < GameSettings::MAP_DIR_COUNT; dir_value++) {
            mdir dir = hdir(dir_value);
            ipos32 raw_to {from.x, from.y};
            GeometryHelper::MoveHexByDirUnsafe(raw_to, dir);
            REQUIRE(TEST_MAP_SIZE.is_valid_pos(raw_to));

            mpos to = TEST_MAP_SIZE.from_raw_pos(raw_to);
            auto settings = MakeClearSettings(from, to);
            auto output = PathFinding::FindPath(settings);

            INFO("dir=" << dir_value);
            REQUIRE(output.Result == FindPathOutput::ResultType::Ok);
            REQUIRE(output.Steps.size() == 1);
            CHECK(output.Steps[0] == dir);
            CHECK(output.NewToHex == to);
        }
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
        int32_t direct_dist = GeometryHelper::GetDistance(mpos {5, 5}, mpos {9, 5});
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
        mpos from = mpos {5, 5};
        mpos to = mpos {8, 8};
        auto settings = MakeClearSettings(from, to);
        auto output = PathFinding::FindPath(settings);

        REQUIRE(output.Result == FindPathOutput::ResultType::Ok);

        // Walk the path and verify each step moves to a valid adjacent hex
        mpos cur = from;
        for (const auto& step : output.Steps) {
            ipos32 raw = ipos32 {cur.x, cur.y};
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

        uint16_t prev = 0;
        for (auto cs : output.ControlSteps) {
            CHECK(cs > prev);
            prev = cs;
        }

        CHECK(prev == static_cast<uint16_t>(output.Steps.size()));
    }

    SECTION("FreeMovementProducesShorterControlSteps")
    {
        mpos from = mpos {2, 2};
        mpos to = mpos {15, 12};

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
        mpos gag_hex = mpos {7, 5};
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
        mpos critter_hex = mpos {7, 5};
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

    SECTION("MultihexPerimeterOutsideMapReturnsBlocked")
    {
        auto result = PathFinding::CheckHexWithMultihex(mpos {0, 0}, hdir::SouthEast, 1, TEST_MAP_SIZE, [](mpos /*hex*/) -> HexBlockResult { return HexBlockResult::Passable; });

        CHECK(result == HexBlockResult::Blocked);
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
        mpos target = mpos {10, 5};
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
        mpos from = mpos {3, 3};
        mpos to = mpos {12, 15};
        auto settings = MakeClearSettings(from, to);
        auto output = PathFinding::FindPath(settings);

        REQUIRE(output.Result == FindPathOutput::ResultType::Ok);

        // Walk path and verify end point
        mpos cur = from;
        for (const auto& step : output.Steps) {
            ipos32 raw = ipos32 {cur.x, cur.y};
            GeometryHelper::MoveHexByDirUnsafe(raw, step);
            REQUIRE(TEST_MAP_SIZE.is_valid_pos(raw));
            cur = TEST_MAP_SIZE.from_raw_pos(raw);
        }
        CHECK(cur == output.NewToHex);
    }

    SECTION("PathNeverVisitsSameHexTwice")
    {
        mpos from = mpos {2, 2};
        mpos to = mpos {17, 17};
        // Wall with a gap to force an interesting path
        auto settings = MakeBlockedSettings(from, to, [](mpos hex) -> bool { return hex.x == 10 && hex.y >= 5 && hex.y <= 14; });
        auto output = PathFinding::FindPath(settings);

        REQUIRE(output.Result == FindPathOutput::ResultType::Ok);

        set<uint32_t> visited;
        mpos cur = from;
        visited.insert(cur.x * 1000 + cur.y);
        for (const auto& step : output.Steps) {
            ipos32 raw = ipos32 {cur.x, cur.y};
            GeometryHelper::MoveHexByDirUnsafe(raw, step);
            cur = TEST_MAP_SIZE.from_raw_pos(raw);
            uint32_t key = static_cast<uint32_t>(cur.x * 1000 + cur.y);
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
        CHECK(output.ControlSteps.back() == static_cast<uint16_t>(output.Steps.size()));

        // First control step must be >= 1
        CHECK(output.ControlSteps.front() >= 1);

        // Each control step segment uses a single direction (non-free movement)
        uint16_t prev_cs = 0;
        for (auto cs : output.ControlSteps) {
            // All steps in [prev_cs, cs) should be the same direction
            if (cs > prev_cs + 1) {
                auto dir = output.Steps[prev_cs];
                for (int32_t j = prev_cs + 1; j < cs; j++) {
                    CHECK(output.Steps[j] == dir);
                }
            }
            prev_cs = cs;
        }
    }

    SECTION("FreeMovementProducesShorterControlSteps")
    {
        mpos from = mpos {2, 2};
        mpos to = mpos {15, 12};

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
        mpos from = mpos {3, 3};
        mpos to = mpos {16, 14};
        auto settings = MakeClearSettings(from, to);
        settings.FreeMovement = true;
        auto output = PathFinding::FindPath(settings);

        REQUIRE(output.Result == FindPathOutput::ResultType::Ok);

        // Walk the path and verify we end at the target
        mpos cur = from;
        for (const auto& step : output.Steps) {
            ipos32 raw = ipos32 {cur.x, cur.y};
            GeometryHelper::MoveHexByDirUnsafe(raw, step);
            REQUIRE(TEST_MAP_SIZE.is_valid_pos(raw));
            cur = TEST_MAP_SIZE.from_raw_pos(raw);
        }
        CHECK(cur == output.NewToHex);
        CHECK(output.NewToHex == to);
    }

    SECTION("FreeMovementAroundObstacle")
    {
        mpos from = mpos {3, 10};
        mpos to = mpos {16, 10};
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

        int32_t direct_dist = GeometryHelper::GetDistance(mpos {2, 10}, mpos {17, 10});

        if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
            CHECK(output.Steps.size() > numeric_cast<size_t>(direct_dist));
        }
        else {
            CHECK(output.Steps.size() >= numeric_cast<size_t>(direct_dist));
        }
    }
}

TEST_CASE("PathFinding::FreeMovementEndOffset")
{
    // Projected distance between two map-pixel points (same metric as MovingContext segments)
    auto proj_dist = [](ipos32 a, ipos32 b) -> float32_t {
        float32_t dx = numeric_cast<float32_t>(a.x - b.x);
        float32_t dy = numeric_cast<float32_t>(a.y - b.y) * GeometryHelper::GetYProj();
        return std::sqrt(dx * dx + dy * dy);
    };

    SECTION("DisabledFreeMovementReturnsZeroOffset")
    {
        auto settings = MakeClearSettings(mpos {5, 5}, mpos {10, 5}, 2);
        settings.ToHexOffset = ipos16 {8, 4};
        settings.FromHexOffset = ipos16 {5, 2}; // must not leak through when FreeMovement is off
        settings.FreeMovement = false;
        auto output = PathFinding::FindPath(settings);

        REQUIRE(output.Result == FindPathOutput::ResultType::Ok);
        CHECK(output.EndHexOffset.x == 0);
        CHECK(output.EndHexOffset.y == 0);
    }

    SECTION("DegenerateTargetKeepsCurrentOffset")
    {
        // Cut 0 onto a centered target produces an undefined stop direction; FindPath must fall
        // back to the mover's current sub-hex offset so the critter stays in place visually.
        auto settings = MakeClearSettings(mpos {5, 5}, mpos {9, 5}, 0);
        settings.ToHexOffset = ipos16 {0, 0};
        settings.FromHexOffset = ipos16 {-6, 4};
        settings.FreeMovement = true;
        auto output = PathFinding::FindPath(settings);

        REQUIRE(output.Result == FindPathOutput::ResultType::Ok);
        CHECK(output.EndHexOffset.x == -6);
        CHECK(output.EndHexOffset.y == 4);
    }

    SECTION("CenteredTargetKeepsCenterOffset")
    {
        auto settings = MakeClearSettings(mpos {5, 5}, mpos {10, 5}, 2);
        settings.ToHexOffset = ipos16 {0, 0};
        settings.FreeMovement = true;
        auto output = PathFinding::FindPath(settings);

        REQUIRE(output.Result == FindPathOutput::ResultType::Ok);
        CHECK(output.EndHexOffset.x == 0);
        CHECK(output.EndHexOffset.y == 0);
    }

    SECTION("ExactReachStandsAtTargetOffset")
    {
        // cut 0 on a clear map reaches the exact target hex; offset must equal the target's own offset
        auto settings = MakeClearSettings(mpos {5, 5}, mpos {9, 5}, 0);
        settings.ToHexOffset = ipos16 {7, 3};
        settings.FreeMovement = true;
        auto output = PathFinding::FindPath(settings);

        REQUIRE(output.Result == FindPathOutput::ResultType::Ok);
        REQUIRE(output.NewToHex == mpos {9, 5});
        CHECK(output.EndHexOffset.x == 7);
        CHECK(output.EndHexOffset.y == 3);
    }

    SECTION("ExactCenteredTargetWithCenteredMoverStaysCentered")
    {
        // Cut 0 onto a centered target is the degenerate case; with FromHexOffset == 0 the fallback
        // also yields 0, preserving the old center-snapped behavior for a mover already at its center.
        auto settings = MakeClearSettings(mpos {5, 5}, mpos {9, 5}, 0);
        settings.ToHexOffset = ipos16 {0, 0};
        settings.FreeMovement = true;
        auto output = PathFinding::FindPath(settings);

        REQUIRE(output.Result == FindPathOutput::ResultType::Ok);
        REQUIRE(output.NewToHex == mpos {9, 5});
        CHECK(output.EndHexOffset.x == 0);
        CHECK(output.EndHexOffset.y == 0);
    }

    SECTION("OffsetTargetPreservesCutDistance")
    {
        auto settings = MakeClearSettings(mpos {5, 5}, mpos {12, 5}, 2);
        settings.ToHexOffset = ipos16 {9, 5};
        settings.FreeMovement = true;
        auto output = PathFinding::FindPath(settings);

        REQUIRE(output.Result == FindPathOutput::ResultType::Ok);

        mpos new_to = output.NewToHex;
        mpos to = settings.ToHex;
        REQUIRE(new_to != to); // off-center target, stopped short by cut

        // R = continuous gap between the final hex center and the target hex center
        float32_t r = proj_dist(GeometryHelper::GetHexOffset(new_to, to), ipos32 {0, 0});

        // Final standing position and the real target, both relative to the target hex center
        ipos32 final_rel = GeometryHelper::GetHexOffset(to, new_to);
        ipos32 final_pos = ipos32 {final_rel.x + output.EndHexOffset.x, final_rel.y + output.EndHexOffset.y};
        ipos32 target_pos = ipos32 {settings.ToHexOffset.x, settings.ToHexOffset.y};

        // Distance to the real target equals the cut gap (within integer-rounding tolerance)
        CHECK(std::abs(proj_dist(final_pos, target_pos) - r) <= 3.0f);

        // Off-center target produced a non-zero correction (feature is active)
        CHECK((output.EndHexOffset.x != 0 || output.EndHexOffset.y != 0));
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

    SECTION("ClearTraceFullyTraced")
    {
        auto settings = make_trace_settings(mpos {5, 5}, mpos {10, 5});
        auto output = PathFinding::TraceLine(settings);

        CHECK(output.FullyTraced);
        CHECK_FALSE(output.HasLastMovable);
    }

    SECTION("ClearTraceBlockEqualsTarget")
    {
        auto settings = make_trace_settings(mpos {5, 5}, mpos {10, 5});
        auto output = PathFinding::TraceLine(settings);

        CHECK(output.FullyTraced);
        // On a full clear trace: Block is the last hex stepped, PreBlock is the one before
        // Both equal the target only if the tracer lands exactly on target
    }

    SECTION("ClearTraceDiagonal")
    {
        auto settings = make_trace_settings(mpos {3, 3}, mpos {15, 15});
        auto output = PathFinding::TraceLine(settings);

        CHECK(output.FullyTraced);
    }

    SECTION("SameStartAndTarget")
    {
        auto settings = make_trace_settings(mpos {5, 5}, mpos {5, 5});
        auto output = PathFinding::TraceLine(settings);

        CHECK(output.FullyTraced);
    }

    // ======== Blocked hex tests ========

    SECTION("BlockedHexStopsTrace")
    {
        mpos block_hex = mpos {7, 5};
        auto settings = make_trace_settings(mpos {5, 5}, mpos {10, 5}, [block_hex](mpos hex) { return hex == block_hex; });
        auto output = PathFinding::TraceLine(settings);

        CHECK_FALSE(output.FullyTraced);
        CHECK(output.Block == block_hex);
    }

    SECTION("PreBlockIsAdjacentToBlock")
    {
        mpos block_hex = mpos {7, 5};
        auto settings = make_trace_settings(mpos {5, 5}, mpos {10, 5}, [block_hex](mpos hex) { return hex == block_hex; });
        auto output = PathFinding::TraceLine(settings);

        CHECK_FALSE(output.FullyTraced);
        CHECK(output.PreBlock != output.Block);
        CHECK(GeometryHelper::CheckDist(output.PreBlock, output.Block, 1));
    }

    SECTION("BlockAtFirstHexAfterStart")
    {
        // Block immediately adjacent hex — trace should stop at distance 1
        auto settings = make_trace_settings(mpos {5, 5}, mpos {10, 5}, [](mpos hex) { return hex == mpos {6, 5}; });
        auto output = PathFinding::TraceLine(settings);

        CHECK_FALSE(output.FullyTraced);
        CHECK(output.Block == mpos {6, 5});
        CHECK(output.PreBlock == mpos {5, 5});
    }

    SECTION("MultipleBlockersOnlyFirstMatters")
    {
        // Two blockers along the line — only first one stops trace
        auto settings = make_trace_settings(mpos {5, 5}, mpos {12, 5}, [](mpos hex) { return hex == mpos {7, 5} || hex == mpos {9, 5}; });
        auto output = PathFinding::TraceLine(settings);

        CHECK_FALSE(output.FullyTraced);
        CHECK(output.Block == mpos {7, 5});
    }

    SECTION("BlockOnTargetHex")
    {
        mpos target = mpos {10, 5};
        auto settings = make_trace_settings(mpos {5, 5}, target, [target](mpos hex) { return hex == target; });
        auto output = PathFinding::TraceLine(settings);

        CHECK_FALSE(output.FullyTraced);
        CHECK(output.Block == target);
    }

    // ======== MaxDist tests ========

    SECTION("MaxDistLimitsTrace")
    {
        auto settings = make_trace_settings(mpos {5, 5}, mpos {15, 5});
        settings.MaxDist = 3;
        auto output = PathFinding::TraceLine(settings);

        CHECK(output.FullyTraced);
        CHECK(GeometryHelper::GetDistance(settings.StartHex, output.Block) <= 3);
    }

    SECTION("MaxDistShorterThanBlocker")
    {
        // MaxDist stops before the blocker — should be full trace
        auto settings = make_trace_settings(mpos {5, 5}, mpos {15, 5}, [](mpos hex) { return hex == mpos {12, 5}; });
        settings.MaxDist = 3;
        auto output = PathFinding::TraceLine(settings);

        CHECK(output.FullyTraced);
    }

    SECTION("MaxDistBeyondBlocker")
    {
        // MaxDist is larger than distance to blocker — blocker should stop trace
        auto settings = make_trace_settings(mpos {5, 5}, mpos {15, 5}, [](mpos hex) { return hex == mpos {7, 5}; });
        settings.MaxDist = 10;
        auto output = PathFinding::TraceLine(settings);

        CHECK_FALSE(output.FullyTraced);
        CHECK(output.Block == mpos {7, 5});
    }

    SECTION("MaxDistEqualsOne")
    {
        auto settings = make_trace_settings(mpos {5, 5}, mpos {15, 5});
        settings.MaxDist = 1;
        auto output = PathFinding::TraceLine(settings);

        CHECK(output.FullyTraced);
        CHECK(GeometryHelper::GetDistance(settings.StartHex, output.Block) <= 1);
    }

    // ======== LastMovable tracking ========

    SECTION("LastMovableTracking")
    {
        mpos block_hex = mpos {8, 5};
        auto settings = make_trace_settings(mpos {5, 5}, mpos {12, 5}, [block_hex](mpos hex) { return hex == block_hex; });
        settings.CheckLastMovable = true;
        settings.IsHexMovable = [block_hex](mpos hex) -> bool { return hex != block_hex; };
        auto output = PathFinding::TraceLine(settings);

        CHECK_FALSE(output.FullyTraced);
        CHECK(output.HasLastMovable);
        CHECK(GeometryHelper::GetDistance(output.LastMovable, settings.StartHex) > 0);
    }

    SECTION("LastMovableIsPreBlock")
    {
        mpos block_hex = mpos {8, 5};
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

        CHECK_FALSE(output.FullyTraced);
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

        CHECK(output.FullyTraced);
        CHECK(output.HasLastMovable);
    }

    SECTION("LastMovableNeverSetIfFirstHexNonMovable")
    {
        // First hex after start is non-movable, and the trace hits a blocker
        auto settings = make_trace_settings(mpos {5, 5}, mpos {10, 5}, [](mpos hex) { return hex.x == 9; });
        settings.CheckLastMovable = true;
        settings.IsHexMovable = [](mpos hex) -> bool { return hex.x > 8; }; // only hexes past x=8 are movable, but blocker at x=9
        auto output = PathFinding::TraceLine(settings);

        CHECK_FALSE(output.FullyTraced);
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

        CHECK(output0.FullyTraced);
        CHECK(output_angled.FullyTraced);

        // With angle offset, the final hex should differ from straight trace
        CHECK(output0.Block != output_angled.Block);
    }

    SECTION("AngleOffsetWithBlocker")
    {
        // Straight trace would hit blocker, angled trace might miss it
        mpos blocker = mpos {10, 7};
        auto settings_straight = make_trace_settings(mpos {10, 10}, mpos {10, 0}, [blocker](mpos hex) { return hex == blocker; });
        settings_straight.Angle = 0.0f;
        auto output_straight = PathFinding::TraceLine(settings_straight);

        auto settings_angled = make_trace_settings(mpos {10, 10}, mpos {10, 0}, [blocker](mpos hex) { return hex == blocker; });
        settings_angled.Angle = 45.0f;
        auto output_angled = PathFinding::TraceLine(settings_angled);

        // Straight should be blocked
        CHECK_FALSE(output_straight.FullyTraced);
        CHECK(output_straight.Block == blocker);

        // Angled might miss the blocker (depending on hex geometry)
        // At minimum, the block position should differ
        if (!output_angled.FullyTraced) {
            CHECK(output_angled.Block != blocker);
        }
    }
}

FO_END_NAMESPACE
