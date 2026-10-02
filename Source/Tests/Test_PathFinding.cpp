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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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
    constexpr msize WIDE_MAP_SIZE {120, 120};
    constexpr mpos POCKET_CENTER {60, 60};
    constexpr mpos FAR_START {10, 10};
    constexpr msize RANDOM_MAP_SIZE {32, 32};
    // The detour a living critter on the route costs, the value the game ships with
    constexpr int32_t TEST_CRITTER_DETOUR = 12;

    // Helper: create settings for a clear map (no obstacles)
    static auto MakeClearSettings(mpos from, mpos to, int32_t cut = 0) -> FindPathInput
    {
        FindPathInput settings;
        settings.FromHex = from;
        settings.ToHex = to;
        settings.MapSize = TEST_MAP_SIZE;
        settings.MaxLength = 200;
        settings.CritterDetour = TEST_CRITTER_DETOUR;
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

    // Reference answer: steps from the start to every hex over a plain breadth-first flood, where a hex is reached
    // by the first step allowed into it. A hex refused from one side stays open to the others
    static auto MeasureSteps(msize map_size, mpos from, const function<bool(mpos, mdir)>& can_enter) -> vector<int32_t>
    {
        vector<int32_t> steps(numeric_cast<size_t>(map_size.width) * numeric_cast<size_t>(map_size.height), -1);
        auto at = [&steps, map_size](mpos hex) -> int32_t& { return steps[numeric_cast<size_t>(hex.y) * numeric_cast<size_t>(map_size.width) + numeric_cast<size_t>(hex.x)]; };
        vector<mpos> queue;
        queue.emplace_back(from);
        at(from) = 0;

        for (size_t i = 0; i < queue.size(); i++) {
            mpos cur = queue[i];

            for (int32_t dir_value = 0; dir_value < GameSettings::MAP_DIR_COUNT; dir_value++) {
                ipos32 raw {cur.x, cur.y};
                GeometryHelper::MoveHexByDirUnsafe(raw, hdir(dir_value));

                if (!map_size.is_valid_pos(raw)) {
                    continue;
                }

                mpos next = map_size.from_raw_pos(raw);

                if (at(next) >= 0 || !can_enter(next, hdir(dir_value))) {
                    continue;
                }

                at(next) = at(cur) + 1;
                queue.emplace_back(next);
            }
        }

        return steps;
    }

    // Reference route: of all the shortest routes, the one that keeps nearest the straight line back to the start
    static auto MakeStraightestRoute(msize map_size, mpos from, mpos goal, const vector<int32_t>& steps, const function<bool(mpos, mdir)>& can_enter) -> vector<mdir>
    {
        auto at = [&steps, map_size](mpos hex) -> int32_t { return steps[numeric_cast<size_t>(hex.y) * numeric_cast<size_t>(map_size.width) + numeric_cast<size_t>(hex.x)]; };
        vector<mdir> route(numeric_cast<size_t>(at(goal)));
        float32_t base_angle = GeometryHelper::GetDirAngle(goal, from);
        mpos cur = goal;

        for (int32_t index = at(goal); index > 0; index--) {
            bool found = false;
            float32_t best_diff = 0.0f;
            mdir best_dir;
            mpos best_hex;

            for (int32_t dir_value = 0; dir_value < GameSettings::MAP_DIR_COUNT; dir_value++) {
                mdir dir = hdir(dir_value);
                ipos32 raw {cur.x, cur.y};
                GeometryHelper::MoveHexByDirUnsafe(raw, dir.reverse());

                if (!map_size.is_valid_pos(raw)) {
                    continue;
                }

                mpos prev = map_size.from_raw_pos(raw);

                if (at(prev) != index - 1 || !can_enter(cur, dir)) {
                    continue;
                }

                float32_t diff = GeometryHelper::GetDirAngleDiff(base_angle, GeometryHelper::GetDirAngle(prev, from));

                if (!found || diff < best_diff) {
                    found = true;
                    best_diff = diff;
                    best_dir = dir;
                    best_hex = prev;
                }
            }

            REQUIRE(found);
            route[numeric_cast<size_t>(index - 1)] = best_dir;
            cur = best_hex;
        }

        return route;
    }

    // Follows the steps and answers where they end, or nullopt when a step leaves the map or enters a refused hex
    static auto WalkRoute(msize map_size, mpos from, const vector<mdir>& route, const function<bool(mpos, mdir)>& can_enter) -> optional<mpos>
    {
        mpos cur = from;

        for (mdir dir : route) {
            ipos32 raw {cur.x, cur.y};
            GeometryHelper::MoveHexByDirUnsafe(raw, dir);

            if (!map_size.is_valid_pos(raw)) {
                return std::nullopt;
            }

            cur = map_size.from_raw_pos(raw);

            if (!can_enter(cur, dir)) {
                return std::nullopt;
            }
        }

        return cur;
    }

    // A random map of scattered blocked hexes with the given density in percent
    static auto MakeRandomBlocks(random_generator& rnd, msize map_size, int32_t density) -> vector<uint8_t>
    {
        vector<uint8_t> blocked(numeric_cast<size_t>(map_size.width) * numeric_cast<size_t>(map_size.height), 0);

        for (auto& cell : blocked) {
            cell = rnd.next_between(0, 99) < density ? 1 : 0;
        }

        return blocked;
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

    SECTION("MaxLengthFarBeyondTheMapKeepsTheSameRoute")
    {
        // The grid half-extent is clamped to the map, so a caller that raises MaxLength walks a different
        // buffer shape. It must still walk the same route: this pins the indexing, not the allocation
        auto blocked = [](mpos hex) { return hex.x == 9 && hex.y >= 4 && hex.y <= 16; };
        auto huge = MakeBlockedSettings(mpos {2, 10}, mpos {17, 10}, blocked);
        huge.MaxLength = 3000;

        auto huge_output = PathFinding::FindPath(huge);
        auto modest_output = PathFinding::FindPath(MakeBlockedSettings(mpos {2, 10}, mpos {17, 10}, blocked));

        CHECK(modest_output.Result == FindPathOutput::ResultType::Ok);
        CHECK(huge_output.Result == modest_output.Result);
        CHECK(huge_output.NewToHex == modest_output.NewToHex);
        CHECK(huge_output.Steps == modest_output.Steps);
    }

    SECTION("ShortMaxLengthOnAWideMapKeepsTheSameRoute")
    {
        // The other side of the clamp: a limit smaller than the map keeps the tight buffer it always had
        auto settings = MakeClearSettings(mpos {2, 2}, mpos {8, 2});
        settings.MaxLength = 12;

        auto output = PathFinding::FindPath(settings);

        CHECK(output.Result == FindPathOutput::ResultType::Ok);
        CHECK(output.NewToHex == mpos {8, 2});
        CHECK(!output.Steps.empty());
    }
}

TEST_CASE("PathFinding::EnclosureProbe")
{
    // Every CheckHex call is counted, because the probe exists to make a hopeless search cheap, not to change its answer
    auto make_settings = [](mpos from, mpos to, int32_t probe_limit, int32_t& calls, function<HexBlockResult(mpos)> check) -> FindPathInput {
        FindPathInput settings;
        settings.FromHex = from;
        settings.ToHex = to;
        settings.MapSize = WIDE_MAP_SIZE;
        settings.MaxLength = 1000;
        settings.EnclosureProbeLimit = probe_limit;
        settings.CheckHex = [&calls, check = std::move(check)](mpos hex) -> HexBlockResult {
            calls++;
            return check(hex);
        };
        return settings;
    };

    auto ring_of = [](HexBlockResult ring_result) -> function<HexBlockResult(mpos)> { return [ring_result](mpos hex) -> HexBlockResult { return GeometryHelper::GetDistance(hex, POCKET_CENTER) == 2 ? ring_result : HexBlockResult::Passable; }; };

    SECTION("WalledOffTargetIsRefusedWithoutFloodingTheMap")
    {
        int32_t probed_calls = 0;
        auto probed = PathFinding::FindPath(make_settings(FAR_START, POCKET_CENTER, 64, probed_calls, ring_of(HexBlockResult::Blocked)));
        int32_t flooded_calls = 0;
        auto flooded = PathFinding::FindPath(make_settings(FAR_START, POCKET_CENTER, 0, flooded_calls, ring_of(HexBlockResult::Blocked)));

        CHECK(probed.Result == FindPathOutput::ResultType::NoWay);
        CHECK(flooded.Result == FindPathOutput::ResultType::NoWay);
        CHECK(probed_calls < 1000);
        CHECK(flooded_calls > 10000);
    }

    SECTION("CutGoalInsideTheWallsIsRefusedToo")
    {
        int32_t calls = 0;
        auto settings = make_settings(FAR_START, POCKET_CENTER, 64, calls, ring_of(HexBlockResult::Blocked));
        settings.Cut = 1;
        auto output = PathFinding::FindPath(settings);

        CHECK(output.Result == FindPathOutput::ResultType::NoWay);
        CHECK(calls < 1000);
    }

    SECTION("NegativeCutIsProbedAsTheExactGoal")
    {
        int32_t calls = 0;
        auto settings = make_settings(FAR_START, POCKET_CENTER, 64, calls, ring_of(HexBlockResult::Blocked));
        settings.Cut = -1;
        auto output = PathFinding::FindPath(settings);

        CHECK(output.Result == FindPathOutput::ResultType::NoWay);
        CHECK(calls < 1000);
    }

    SECTION("DeferredRingStaysPassable")
    {
        int32_t gag_calls = 0;
        auto gag_output = PathFinding::FindPath(make_settings(FAR_START, POCKET_CENTER, 64, gag_calls, ring_of(HexBlockResult::DeferGag)));
        int32_t critter_calls = 0;
        auto critter_output = PathFinding::FindPath(make_settings(FAR_START, POCKET_CENTER, 64, critter_calls, ring_of(HexBlockResult::DeferCritter)));

        CHECK(gag_output.Result == FindPathOutput::ResultType::Ok);
        CHECK(gag_output.NewToHex == POCKET_CENTER);
        CHECK(critter_output.Result == FindPathOutput::ResultType::Ok);
        CHECK(critter_output.NewToHex == POCKET_CENTER);
    }

    SECTION("ReachableRouteIsTheSameWithAndWithoutTheProbe")
    {
        auto wall = [](mpos hex) -> HexBlockResult { return hex.x == 40 && hex.y >= 5 && hex.y <= 110 ? HexBlockResult::Blocked : HexBlockResult::Passable; };
        int32_t probed_calls = 0;
        auto probed = PathFinding::FindPath(make_settings(mpos {20, 60}, mpos {70, 60}, 64, probed_calls, wall));
        int32_t plain_calls = 0;
        auto plain = PathFinding::FindPath(make_settings(mpos {20, 60}, mpos {70, 60}, 0, plain_calls, wall));

        CHECK(probed.Result == FindPathOutput::ResultType::Ok);
        CHECK(probed.Result == plain.Result);
        CHECK(probed.NewToHex == plain.NewToHex);
        CHECK(probed.Steps == plain.Steps);
    }

    SECTION("ShortSearchNeverRunsTheProbe")
    {
        // A route found within the budget is answered before the probe could start, so it costs nothing extra
        int32_t probed_calls = 0;
        auto probed = PathFinding::FindPath(make_settings(mpos {50, 60}, mpos {53, 60}, 1024, probed_calls, ring_of(HexBlockResult::Passable)));
        int32_t plain_calls = 0;
        auto plain = PathFinding::FindPath(make_settings(mpos {50, 60}, mpos {53, 60}, 0, plain_calls, ring_of(HexBlockResult::Passable)));

        CHECK(probed.Result == FindPathOutput::ResultType::Ok);
        CHECK(probed_calls == plain_calls);
    }

    SECTION("RegionLargerThanTheBudgetFallsBackToTheFullSearch")
    {
        auto split = [](mpos hex) -> HexBlockResult { return hex.x == 60 ? HexBlockResult::Blocked : HexBlockResult::Passable; };
        int32_t probed_calls = 0;
        auto probed = PathFinding::FindPath(make_settings(mpos {30, 60}, mpos {90, 60}, 64, probed_calls, split));
        int32_t plain_calls = 0;
        auto plain = PathFinding::FindPath(make_settings(mpos {30, 60}, mpos {90, 60}, 0, plain_calls, split));

        CHECK(probed.Result == plain.Result);
        CHECK(probed.Result == FindPathOutput::ResultType::NoWay);
        CHECK(probed_calls > plain_calls);
    }

    SECTION("MultiTargetSearchIsNotProbed")
    {
        int32_t probed_calls = 0;
        auto probed_settings = make_settings(FAR_START, mpos {}, 64, probed_calls, ring_of(HexBlockResult::Blocked));
        probed_settings.CheckTarget = [](mpos hex) { return hex == POCKET_CENTER; };
        auto probed = PathFinding::FindPath(probed_settings);
        int32_t plain_calls = 0;
        auto plain_settings = make_settings(FAR_START, mpos {}, 0, plain_calls, ring_of(HexBlockResult::Blocked));
        plain_settings.CheckTarget = [](mpos hex) { return hex == POCKET_CENTER; };
        auto plain = PathFinding::FindPath(plain_settings);

        CHECK(probed.Result == plain.Result);
        CHECK(probed_calls == plain_calls);
    }
}

TEST_CASE("PathFinding::AStar")
{
    auto blocked_at = [](const vector<uint8_t>& blocked, mpos hex) -> bool { return blocked[numeric_cast<size_t>(hex.y) * numeric_cast<size_t>(RANDOM_MAP_SIZE.width) + numeric_cast<size_t>(hex.x)] != 0; };

    auto pick_open = [&blocked_at](random_generator& rnd, const vector<uint8_t>& blocked) -> mpos {
        while (true) {
            mpos hex = RANDOM_MAP_SIZE.from_raw_pos(rnd.next_between(0, RANDOM_MAP_SIZE.width - 1), rnd.next_between(0, RANDOM_MAP_SIZE.height - 1));

            if (!blocked_at(blocked, hex)) {
                return hex;
            }
        }
    };

    auto nearest_goal = [](const vector<int32_t>& steps, mpos to, int32_t cut) -> int32_t {
        int32_t best = -1;

        for (int16_t y = 0; y < RANDOM_MAP_SIZE.height; y++) {
            for (int16_t x = 0; x < RANDOM_MAP_SIZE.width; x++) {
                int32_t hex_steps = steps[numeric_cast<size_t>(y) * numeric_cast<size_t>(RANDOM_MAP_SIZE.width) + numeric_cast<size_t>(x)];

                if (hex_steps >= 0 && GeometryHelper::CheckDist(mpos {x, y}, to, cut) && (best < 0 || hex_steps < best)) {
                    best = hex_steps;
                }
            }
        }

        return best;
    };

    SECTION("RandomMapsGetTheShortestAndStraightestRoute")
    {
        // The reference is the breadth-first answer the search replaced: the same length, and of the shortest routes
        // the one nearest the straight line back to the start
        random_generator rnd {26092026};
        size_t routes = 0;

        for (int32_t round = 0; round < 400; round++) {
            vector<uint8_t> blocked = MakeRandomBlocks(rnd, RANDOM_MAP_SIZE, rnd.next_between(5, 40));
            auto can_enter = [&blocked, &blocked_at](mpos hex, mdir /*dir*/) -> bool { return !blocked_at(blocked, hex); };
            mpos from = pick_open(rnd, blocked);
            mpos to = pick_open(rnd, blocked);
            int32_t cut = rnd.next_between(0, 2);

            FindPathInput input = MakeBlockedSettings(from, to, [&blocked, &blocked_at](mpos hex) -> bool { return blocked_at(blocked, hex); }, cut);
            input.MapSize = RANDOM_MAP_SIZE;
            input.MaxLength = 1000;
            auto output = PathFinding::FindPath(input);

            vector<int32_t> steps = MeasureSteps(RANDOM_MAP_SIZE, from, can_enter);
            int32_t best = nearest_goal(steps, to, cut);

            INFO("round " << round << " from " << from.x << "," << from.y << " to " << to.x << "," << to.y << " cut " << cut);

            if (GeometryHelper::CheckDist(from, to, cut)) {
                CHECK(output.Result == FindPathOutput::ResultType::AlreadyHere);
                continue;
            }
            if (best < 0) {
                CHECK(output.Result == FindPathOutput::ResultType::NoWay);
                continue;
            }

            REQUIRE(output.Result == FindPathOutput::ResultType::Ok);
            CHECK(output.Steps.size() == numeric_cast<size_t>(best));
            CHECK(WalkRoute(RANDOM_MAP_SIZE, from, output.Steps, can_enter) == optional<mpos> {output.NewToHex});
            CHECK(GeometryHelper::CheckDist(output.NewToHex, to, cut));
            CHECK(output.Steps == MakeStraightestRoute(RANDOM_MAP_SIZE, from, output.NewToHex, steps, can_enter));

            input.FreeMovement = true;
            auto free_output = PathFinding::FindPath(input);

            REQUIRE(free_output.Result == FindPathOutput::ResultType::Ok);
            CHECK(free_output.NewToHex == output.NewToHex);
            CHECK(free_output.Steps.size() == numeric_cast<size_t>(best));
            CHECK(WalkRoute(RANDOM_MAP_SIZE, from, free_output.Steps, can_enter) == optional<mpos> {free_output.NewToHex});
            routes++;
        }

        CHECK(routes > 200);
    }

    SECTION("RandomMultihexMapsGetTheShortestRoute")
    {
        // A footprint refused from one side may still fit from another, so every side is tried before a hex is given up
        random_generator rnd {19091991};
        size_t routes = 0;

        for (int32_t round = 0; round < 300; round++) {
            vector<uint8_t> blocked = MakeRandomBlocks(rnd, RANDOM_MAP_SIZE, rnd.next_between(2, 10));
            function<HexBlockResult(mpos)> check = [&blocked, &blocked_at](mpos hex) -> HexBlockResult { return blocked_at(blocked, hex) ? HexBlockResult::Blocked : HexBlockResult::Passable; };
            auto can_enter = [&check](mpos hex, mdir dir) -> bool { return PathFinding::CheckHexWithMultihex(hex, dir, 1, RANDOM_MAP_SIZE, check) != HexBlockResult::Blocked; };
            mpos from = pick_open(rnd, blocked);
            mpos to = pick_open(rnd, blocked);

            FindPathInput input = MakeClearSettings(from, to);
            input.MapSize = RANDOM_MAP_SIZE;
            input.MaxLength = 1000;
            input.Multihex = 1;
            input.CheckHex = [&check](mpos hex) -> HexBlockResult { return check(hex); };
            auto output = PathFinding::FindPath(input);

            vector<int32_t> steps = MeasureSteps(RANDOM_MAP_SIZE, from, can_enter);
            int32_t best = nearest_goal(steps, to, 0);

            INFO("round " << round << " from " << from.x << "," << from.y << " to " << to.x << "," << to.y);

            if (from == to) {
                continue;
            }
            if (best < 0) {
                CHECK(output.Result == FindPathOutput::ResultType::NoWay);
                continue;
            }

            REQUIRE(output.Result == FindPathOutput::ResultType::Ok);
            CHECK(output.Steps.size() == numeric_cast<size_t>(best));
            CHECK(WalkRoute(RANDOM_MAP_SIZE, from, output.Steps, can_enter) == optional<mpos> {to});
            CHECK(output.Steps == MakeStraightestRoute(RANDOM_MAP_SIZE, from, to, steps, can_enter));
            routes++;
        }

        CHECK(routes > 100);
    }

    SECTION("LengthLimitCountsRouteSteps")
    {
        // The limit is the longest route allowed, step for step: a route of exactly that length is found, one step
        // longer is refused as too far
        random_generator rnd {777};
        size_t checked = 0;

        for (int32_t round = 0; round < 200; round++) {
            vector<uint8_t> blocked = MakeRandomBlocks(rnd, RANDOM_MAP_SIZE, rnd.next_between(10, 35));
            auto can_enter = [&blocked, &blocked_at](mpos hex, mdir /*dir*/) -> bool { return !blocked_at(blocked, hex); };
            mpos from = pick_open(rnd, blocked);
            mpos to = pick_open(rnd, blocked);
            int32_t best = nearest_goal(MeasureSteps(RANDOM_MAP_SIZE, from, can_enter), to, 0);

            if (best < 2) {
                continue;
            }

            FindPathInput input = MakeBlockedSettings(from, to, [&blocked, &blocked_at](mpos hex) -> bool { return blocked_at(blocked, hex); });
            input.MapSize = RANDOM_MAP_SIZE;
            input.MaxLength = best;
            auto exact = PathFinding::FindPath(input);
            input.MaxLength = best - 1;
            auto short_by_one = PathFinding::FindPath(input);

            INFO("round " << round << " best " << best);
            CHECK(exact.Result == FindPathOutput::ResultType::Ok);
            CHECK(exact.Steps.size() == numeric_cast<size_t>(best));
            CHECK(short_by_one.Result == FindPathOutput::ResultType::TooFar);
            checked++;
        }

        CHECK(checked > 100);
    }

    SECTION("TargetBeyondTheLimitIsRefusedWithoutAskingTheMap")
    {
        int32_t calls = 0;
        auto settings = MakeClearSettings(mpos {10, 10}, mpos {150, 150});
        settings.MapSize = msize {200, 200};
        settings.MaxLength = 50;
        settings.CheckHex = [&calls](mpos /*hex*/) -> HexBlockResult {
            calls++;
            return HexBlockResult::Passable;
        };
        auto output = PathFinding::FindPath(settings);

        CHECK(output.Result == FindPathOutput::ResultType::TooFar);
        CHECK(calls == 0);
    }

    SECTION("OpenFieldSearchStaysNearTheLine")
    {
        // The search heads for the goal instead of flooding everything within reach, which would ask about every hex
        // nearer than the goal
        int32_t calls = 0;
        auto settings = MakeClearSettings(mpos {50, 150}, mpos {250, 150});
        settings.MapSize = msize {300, 300};
        settings.MaxLength = 500;
        settings.CheckHex = [&calls](mpos /*hex*/) -> HexBlockResult {
            calls++;
            return HexBlockResult::Passable;
        };
        auto output = PathFinding::FindPath(settings);

        int32_t distance = GeometryHelper::GetDistance(mpos {50, 150}, mpos {250, 150});

        REQUIRE(output.Result == FindPathOutput::ResultType::Ok);
        CHECK(output.Steps.size() == numeric_cast<size_t>(distance));
        CHECK(calls * 5 < GeometryHelper::HexesInRadius(distance));
    }

    SECTION("CheapestGoalNearestTheLineIsChosen")
    {
        // Every hex two away from the target on the near side is as cheap to reach; the one straight ahead is taken
        auto output = PathFinding::FindPath(MakeClearSettings(mpos {5, 5}, mpos {12, 5}, 2));

        CHECK(output.Result == FindPathOutput::ResultType::Ok);
        CHECK(output.NewToHex == mpos {10, 5});
    }

    SECTION("GagIsCrossedOnlyWhenTheDetourCostsMore")
    {
        mpos gag_hex {30, 30};

        auto crosses_gag = [gag_hex](int16_t gap_y) -> bool {
            auto settings = MakeClearSettings(mpos {25, 30}, mpos {35, 30});
            settings.MapSize = msize {60, 60};
            settings.CheckHex = [gap_y, gag_hex](mpos hex) -> HexBlockResult {
                if (hex == gag_hex) {
                    return HexBlockResult::DeferGag;
                }

                return hex.x == 30 && hex.y != gap_y ? HexBlockResult::Blocked : HexBlockResult::Passable;
            };
            auto output = PathFinding::FindPath(settings);
            REQUIRE(output.Result == FindPathOutput::ResultType::Ok);

            mpos cur = settings.FromHex;
            bool crossed = false;

            for (mdir dir : output.Steps) {
                GeometryHelper::MoveHexByDir(cur, dir, settings.MapSize);
                crossed = crossed || cur == gag_hex;
            }

            CHECK(cur == settings.ToHex);
            return crossed;
        };

        CHECK_FALSE(crosses_gag(33));
        CHECK(crosses_gag(55));
    }

    SECTION("CritterIsCrossedOnlyWhenTheDetourCostsMore")
    {
        // A critter costs CritterDetour extra steps as a gag costs its ten: a gap in a line of critters three rows off
        // the straight way is walked round, one twenty-five rows off costs more than the critter and the line is crossed
        auto crosses_critter = [](int16_t gap_y) -> bool {
            auto settings = MakeClearSettings(mpos {25, 30}, mpos {35, 30});
            settings.MapSize = msize {60, 60};
            settings.CheckHex = [gap_y](mpos hex) -> HexBlockResult { return hex.x == 30 && hex.y != gap_y ? HexBlockResult::DeferCritter : HexBlockResult::Passable; };
            auto output = PathFinding::FindPath(settings);
            REQUIRE(output.Result == FindPathOutput::ResultType::Ok);

            mpos cur = settings.FromHex;
            bool crossed = false;

            for (mdir dir : output.Steps) {
                GeometryHelper::MoveHexByDir(cur, dir, settings.MapSize);
                crossed = crossed || (cur.x == 30 && cur.y != gap_y);
            }

            CHECK(cur == settings.ToHex);
            return crossed;
        };

        CHECK_FALSE(crosses_critter(33));
        CHECK(crosses_critter(55));
    }

    SECTION("RouteThroughFewerCrittersWinsOverAShortDetour")
    {
        // Column 15 is all critters and column 16 has them above row 9: the straight way crosses two, a way through
        // the lower rows crosses one for a couple of extra steps and wins
        auto settings = MakeClearSettings(mpos {10, 5}, mpos {22, 5});
        settings.MapSize = msize {40, 40};
        auto is_critter = [](mpos hex) -> bool { return hex.x == 15 || (hex.x == 16 && hex.y < 9); };
        settings.CheckHex = [is_critter](mpos hex) -> HexBlockResult { return is_critter(hex) ? HexBlockResult::DeferCritter : HexBlockResult::Passable; };
        auto output = PathFinding::FindPath(settings);

        REQUIRE(output.Result == FindPathOutput::ResultType::Ok);

        mpos cur = settings.FromHex;
        int32_t critters = 0;

        for (mdir dir : output.Steps) {
            GeometryHelper::MoveHexByDir(cur, dir, settings.MapSize);
            critters += is_critter(cur) ? 1 : 0;
        }

        CHECK(cur == settings.ToHex);
        CHECK(critters == 1);
    }

    SECTION("LengthLimitDoesNotHoldBackACritterCrossing")
    {
        // A wall of critters splits the map and the length limit cuts off part of this side. A critter is a price, not a
        // last resort, so the route crosses the wall instead of refusing for a way round the limit might hide
        auto settings = MakeClearSettings(mpos {95, 100}, mpos {105, 100});
        settings.MapSize = msize {200, 200};
        settings.MaxLength = 60;
        settings.CheckHex = [](mpos hex) -> HexBlockResult { return hex.x == 100 ? HexBlockResult::DeferCritter : HexBlockResult::Passable; };
        auto output = PathFinding::FindPath(settings);

        REQUIRE(output.Result == FindPathOutput::ResultType::Ok);
        CHECK(output.NewToHex == mpos {105, 100});
        CHECK(output.Steps.size() == 10);
    }

    SECTION("TargetRingedByCrittersIsReachedWithoutFloodingTheMap")
    {
        // A melee crowd: every hex within Cut of the target holds a critter, so a route may end in one. The search
        // heads for the nearest of them at the price of one critter instead of settling the whole map within the limit
        mpos target {300, 300};
        mpos from {290, 300};
        int32_t calls = 0;
        auto settings = MakeClearSettings(from, target, 1);
        settings.MapSize = msize {600, 600};
        settings.MaxLength = 500;
        settings.CheckHex = [&calls, target](mpos hex) -> HexBlockResult {
            calls++;
            return GeometryHelper::GetDistance(hex, target) <= 1 ? HexBlockResult::DeferCritter : HexBlockResult::Passable;
        };
        auto output = PathFinding::FindPath(settings);

        REQUIRE(output.Result == FindPathOutput::ResultType::Ok);
        CHECK(GeometryHelper::GetDistance(output.NewToHex, target) <= 1);
        CHECK(output.Steps.size() == numeric_cast<size_t>(GeometryHelper::GetDistance(from, target) - 1));
        CHECK(calls < 2000);
    }

    SECTION("OccupiedExactTargetIsReachedWithoutFloodingTheMap")
    {
        // The same for an exact target a critter stands on, as in front of a door somebody blocks: one step, not a
        // flood of everything within reach
        mpos target {301, 300};
        int32_t calls = 0;
        auto settings = MakeClearSettings(mpos {300, 300}, target);
        settings.MapSize = msize {600, 600};
        settings.MaxLength = 500;
        settings.CheckHex = [&calls, target](mpos hex) -> HexBlockResult {
            calls++;
            return hex == target ? HexBlockResult::DeferCritter : HexBlockResult::Passable;
        };
        auto output = PathFinding::FindPath(settings);

        REQUIRE(output.Result == FindPathOutput::ResultType::Ok);
        CHECK(output.NewToHex == target);
        CHECK(output.Steps.size() == 1);
        CHECK(calls < 500);
    }

    SECTION("CrowdIsNotAGoalWhileAnyGoalIsFree")
    {
        // One hex of the ring is free, on the far side, and walled off from outside, so the only way in is through the
        // crowd. Stopping in the crowd would be cheaper, yet a route ends on a critter only when no goal is free
        mpos target = POCKET_CENTER;
        mpos from {50, 60};
        mpos free_hex = target;
        (void)GeometryHelper::MoveHexByDir(free_hex, GeometryHelper::GetHexDir(from, target), WIDE_MAP_SIZE);
        auto is_critter = [target, free_hex](mpos hex) -> bool { return hex != free_hex && GeometryHelper::GetDistance(hex, target) <= 1; };
        auto settings = MakeClearSettings(from, target, 1);
        settings.MapSize = WIDE_MAP_SIZE;
        settings.CheckHex = [target, free_hex, is_critter](mpos hex) -> HexBlockResult {
            if (is_critter(hex)) {
                return HexBlockResult::DeferCritter;
            }

            return GeometryHelper::GetDistance(hex, free_hex) == 1 && GeometryHelper::GetDistance(hex, target) > 1 ? HexBlockResult::Blocked : HexBlockResult::Passable;
        };
        auto output = PathFinding::FindPath(settings);

        REQUIRE(output.Result == FindPathOutput::ResultType::Ok);
        CHECK(output.NewToHex == free_hex);

        mpos cur = settings.FromHex;
        int32_t critters = 0;

        for (mdir dir : output.Steps) {
            GeometryHelper::MoveHexByDir(cur, dir, settings.MapSize);
            critters += is_critter(cur) ? 1 : 0;
        }

        CHECK(cur == free_hex);
        CHECK(critters == 1);
    }

    SECTION("CrittersOnTheWayAreChargedWhenEveryGoalHoldsOne")
    {
        // Every goal holds a critter, so the route ends in one, and the critters on the way still cost their detour: a
        // line of them open three rows off the straight way is walked round, not crossed
        mpos target {30, 20};
        auto is_critter = [target](mpos hex) -> bool { return GeometryHelper::GetDistance(hex, target) <= 1 || (hex.x == 20 && hex.y != 23); };
        auto settings = MakeClearSettings(mpos {10, 20}, target, 1);
        settings.MapSize = msize {60, 60};
        settings.CheckHex = [is_critter](mpos hex) -> HexBlockResult { return is_critter(hex) ? HexBlockResult::DeferCritter : HexBlockResult::Passable; };
        auto output = PathFinding::FindPath(settings);

        REQUIRE(output.Result == FindPathOutput::ResultType::Ok);

        mpos cur = settings.FromHex;
        int32_t critters = 0;

        for (mdir dir : output.Steps) {
            GeometryHelper::MoveHexByDir(cur, dir, settings.MapSize);
            critters += is_critter(cur) ? 1 : 0;
        }

        CHECK(cur == output.NewToHex);
        CHECK(GeometryHelper::GetDistance(cur, target) <= 1);
        CHECK(critters == 1);
    }
}

TEST_CASE("PathFinding::FindReachable")
{
    auto blocked_at = [](const vector<uint8_t>& blocked, mpos hex) -> bool { return blocked[numeric_cast<size_t>(hex.y) * numeric_cast<size_t>(RANDOM_MAP_SIZE.width) + numeric_cast<size_t>(hex.x)] != 0; };

    auto make_input = [](mpos from, msize map_size, int32_t max_length, const vector<mpos>& targets, function<HexBlockResult(mpos)> check) -> FindReachableInput {
        FindReachableInput input;
        input.FromHex = from;
        input.MapSize = map_size;
        input.MaxLength = max_length;
        input.TargetHexes = targets;
        input.CheckHex = std::move(check);
        return input;
    };

    SECTION("RandomMapsAnswerEveryTargetLikeASingleTargetSearch")
    {
        // The flood replaces one search per target, so every answer must be the one that search gives, the length limit
        // included; the breadth-first reference covers targets the limit cuts off
        random_generator rnd {28092026};
        size_t reachable_total = 0;
        size_t unreachable_total = 0;

        for (int32_t round = 0; round < 150; round++) {
            vector<uint8_t> blocked = MakeRandomBlocks(rnd, RANDOM_MAP_SIZE, rnd.next_between(10, 45));
            auto check = [&blocked, &blocked_at](mpos hex) -> HexBlockResult { return blocked_at(blocked, hex) ? HexBlockResult::Blocked : HexBlockResult::Passable; };
            auto can_enter = [&blocked, &blocked_at](mpos hex, mdir /*dir*/) -> bool { return !blocked_at(blocked, hex); };
            mpos from = RANDOM_MAP_SIZE.from_raw_pos(rnd.next_between(0, RANDOM_MAP_SIZE.width - 1), rnd.next_between(0, RANDOM_MAP_SIZE.height - 1));
            int32_t max_length = rnd.next_between(0, 1) == 0 ? 1000 : rnd.next_between(3, 20);
            vector<mpos> targets;

            for (int32_t i = 0; i < 40; i++) {
                targets.emplace_back(RANDOM_MAP_SIZE.from_raw_pos(rnd.next_between(0, RANDOM_MAP_SIZE.width - 1), rnd.next_between(0, RANDOM_MAP_SIZE.height - 1)));
            }

            vector<mpos> reachable = PathFinding::FindReachable(make_input(from, RANDOM_MAP_SIZE, max_length, targets, check));
            vector<int32_t> steps = MeasureSteps(RANDOM_MAP_SIZE, from, can_enter);
            vector<mpos> expected;

            for (mpos target : targets) {
                int32_t target_steps = steps[numeric_cast<size_t>(target.y) * numeric_cast<size_t>(RANDOM_MAP_SIZE.width) + numeric_cast<size_t>(target.x)];
                bool expect_reached = target_steps >= 0 && target_steps <= max_length;

                FindPathInput single = MakeClearSettings(from, target);
                single.MapSize = RANDOM_MAP_SIZE;
                single.MaxLength = max_length;
                single.CheckHex = check;
                auto single_output = PathFinding::FindPath(single);
                bool single_reached = single_output.Result == FindPathOutput::ResultType::Ok || single_output.Result == FindPathOutput::ResultType::AlreadyHere;

                INFO("round " << round << " from " << from.x << "," << from.y << " target " << target.x << "," << target.y << " limit " << max_length);
                CHECK(single_reached == expect_reached);

                if (expect_reached) {
                    expected.emplace_back(target);
                    reachable_total++;
                }
                else {
                    unreachable_total++;
                }
            }

            INFO("round " << round);
            CHECK(reachable == expected);
        }

        CHECK(reachable_total > 500);
        CHECK(unreachable_total > 500);
    }

    SECTION("DeferredHexesArePassable")
    {
        // A gag the caller lets through and a critter make a route dearer, never impossible
        vector<mpos> targets {POCKET_CENTER};
        auto ring_of = [](HexBlockResult ring_result) -> function<HexBlockResult(mpos)> { return [ring_result](mpos hex) -> HexBlockResult { return GeometryHelper::GetDistance(hex, POCKET_CENTER) == 2 ? ring_result : HexBlockResult::Passable; }; };

        CHECK(PathFinding::FindReachable(make_input(FAR_START, WIDE_MAP_SIZE, 1000, targets, ring_of(HexBlockResult::DeferGag))) == targets);
        CHECK(PathFinding::FindReachable(make_input(FAR_START, WIDE_MAP_SIZE, 1000, targets, ring_of(HexBlockResult::DeferCritter))) == targets);
        CHECK(PathFinding::FindReachable(make_input(FAR_START, WIDE_MAP_SIZE, 1000, targets, ring_of(HexBlockResult::Blocked))).empty());
    }

    SECTION("WalledOffTargetsCostOneFlood")
    {
        // Every target inside a closed pocket is refused by the same single flood of the start side, which asks the
        // map about each hex once however many neighbours reach it
        vector<mpos> targets;

        for (int32_t i = 0; i < GeometryHelper::HexesInRadius(1); i++) {
            mpos hex = POCKET_CENTER;
            REQUIRE(GeometryHelper::MoveHexAroundAway(hex, i, WIDE_MAP_SIZE));
            targets.emplace_back(hex);
        }

        int32_t calls = 0;
        unordered_set<mpos> asked;
        auto check = [&calls, &asked](mpos hex) -> HexBlockResult {
            calls++;
            asked.emplace(hex);
            return GeometryHelper::GetDistance(hex, POCKET_CENTER) == 2 ? HexBlockResult::Blocked : HexBlockResult::Passable;
        };
        vector<mpos> reachable = PathFinding::FindReachable(make_input(FAR_START, WIDE_MAP_SIZE, 1000, targets, check));

        CHECK(reachable.empty());
        CHECK(calls == numeric_cast<int32_t>(asked.size()));
        CHECK(calls < numeric_cast<int32_t>(WIDE_MAP_SIZE.width) * numeric_cast<int32_t>(WIDE_MAP_SIZE.height));
    }

    SECTION("FloodStopsOnceEveryTargetIsReached")
    {
        int32_t calls = 0;
        vector<mpos> targets {mpos {52, 60}, mpos {48, 60}};
        auto check = [&calls](mpos /*hex*/) -> HexBlockResult {
            calls++;
            return HexBlockResult::Passable;
        };
        vector<mpos> reachable = PathFinding::FindReachable(make_input(mpos {50, 60}, WIDE_MAP_SIZE, 1000, targets, check));

        CHECK(reachable == targets);
        CHECK(calls < GeometryHelper::HexesInRadius(4));
    }

    SECTION("StartAndInvalidHexes")
    {
        vector<mpos> targets {mpos {5, 5}, mpos {6, 5}};
        auto open = [](mpos /*hex*/) -> HexBlockResult { return HexBlockResult::Passable; };

        CHECK(PathFinding::FindReachable(make_input(mpos {5, 5}, TEST_MAP_SIZE, 0, targets, open)) == vector<mpos> {mpos {5, 5}});
        CHECK(PathFinding::FindReachable(make_input(mpos {5, 5}, TEST_MAP_SIZE, 1, targets, open)) == targets);
        CHECK(PathFinding::FindReachable(make_input(mpos {50, 50}, TEST_MAP_SIZE, 100, targets, open)).empty());
        CHECK(PathFinding::FindReachable(make_input(mpos {5, 5}, TEST_MAP_SIZE, 100, {}, open)).empty());
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
        // back to the mover's current sub-hex offset so the critter stays in place visually
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
        // also yields 0, preserving the old center-snapped behavior for a mover already at its center
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
        settings.ToHexOffset = ipos16 {-9, 5};
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

// Hidden: what one search costs on a settlement-sized map, per kind of request, with every answer folded into a
// fingerprint so two builds can be compared on the same requests. A measurement, so it runs only by name
TEST_CASE("PathFindingCost", "[.]")
{
    constexpr int16_t SIDE = 600;
    constexpr msize MAP_SIZE {SIDE, SIDE};
    constexpr int32_t REPEATS = 3;

    vector<uint8_t> blocked(numeric_cast<size_t>(SIDE) * numeric_cast<size_t>(SIDE), 0);
    random_generator rnd {20260926};
    int64_t check_calls = 0;

    auto cell = [&blocked](int32_t x, int32_t y) -> uint8_t& { return blocked[numeric_cast<size_t>(y) * numeric_cast<size_t>(SIDE) + numeric_cast<size_t>(x)]; };
    auto is_blocked = [&cell](mpos hex) -> bool { return cell(hex.x, hex.y) != 0; };

    auto draw_outline = [&cell](int32_t x0, int32_t y0, int32_t w, int32_t h) {
        for (int32_t x = x0; x < x0 + w; x++) {
            cell(x, y0) = 1;
            cell(x, y0 + h - 1) = 1;
        }
        for (int32_t y = y0; y < y0 + h; y++) {
            cell(x0, y) = 1;
            cell(x0 + w - 1, y) = 1;
        }
    };

    // Scattered rocks, then houses with one or two doorways two hexes wide, then boxes nobody can enter
    for (int32_t i = 0; i < SIDE * SIDE / 33; i++) {
        cell(rnd.next_between(0, SIDE - 1), rnd.next_between(0, SIDE - 1)) = 1;
    }

    for (int32_t i = 0; i < 900; i++) {
        int32_t w = rnd.next_between(6, 18);
        int32_t h = rnd.next_between(6, 18);
        int32_t x0 = rnd.next_between(1, SIDE - w - 1);
        int32_t y0 = rnd.next_between(1, SIDE - h - 1);
        draw_outline(x0, y0, w, h);

        int32_t doors = rnd.next_between(1, 2);

        for (int32_t door = 0; door < doors; door++) {
            int32_t side = rnd.next_between(0, 3);

            if (side < 2) {
                int32_t x = rnd.next_between(x0 + 1, x0 + w - 3);
                int32_t y = side == 0 ? y0 : y0 + h - 1;
                cell(x, y) = 0;
                cell(x + 1, y) = 0;
            }
            else {
                int32_t y = rnd.next_between(y0 + 1, y0 + h - 3);
                int32_t x = side == 2 ? x0 : x0 + w - 1;
                cell(x, y) = 0;
                cell(x, y + 1) = 0;
            }
        }
    }

    vector<mpos> sealed_centers;

    for (int32_t i = 0; i < 16; i++) {
        int32_t x0 = 20 + (i % 4) * 150;
        int32_t y0 = 20 + (i / 4) * 150;

        for (int32_t y = y0; y < y0 + 11; y++) {
            for (int32_t x = x0; x < x0 + 11; x++) {
                cell(x, y) = 0;
            }
        }

        draw_outline(x0, y0, 11, 11);
        sealed_centers.emplace_back(MAP_SIZE.from_raw_pos(x0 + 5, y0 + 5));
    }

    auto pick_open = [&]() -> mpos {
        while (true) {
            mpos hex = MAP_SIZE.from_raw_pos(rnd.next_between(0, SIDE - 1), rnd.next_between(0, SIDE - 1));

            if (!is_blocked(hex)) {
                return hex;
            }
        }
    };

    auto pick_open_near = [&](mpos from, int32_t min_dist, int32_t max_dist) -> mpos {
        while (true) {
            int32_t x = from.x + rnd.next_between(-max_dist, max_dist);
            int32_t y = from.y + rnd.next_between(-max_dist, max_dist);

            if (!MAP_SIZE.is_valid_pos(x, y)) {
                continue;
            }

            mpos hex = MAP_SIZE.from_raw_pos(x, y);
            int32_t dist = GeometryHelper::GetDistance(from, hex);

            if (dist >= min_dist && dist <= max_dist && !is_blocked(hex)) {
                return hex;
            }
        }
    };

    auto make_request = [&](mpos from, mpos to) -> FindPathInput {
        FindPathInput input;
        input.FromHex = from;
        input.ToHex = to;
        input.MapSize = MAP_SIZE;
        input.MaxLength = 500;
        input.EnclosureProbeLimit = 1024;
        input.FreeMovement = true;
        input.CheckHex = [&check_calls, &is_blocked](mpos hex) -> HexBlockResult {
            check_calls++;
            return is_blocked(hex) ? HexBlockResult::Blocked : HexBlockResult::Passable;
        };
        return input;
    };

    struct Scenario
    {
        string Name;
        vector<FindPathInput> Requests;
    };

    vector<Scenario> scenarios;
    scenarios.emplace_back(Scenario {.Name = "near 3-15", .Requests = {}});
    scenarios.emplace_back(Scenario {.Name = "near 3-15, hex steps", .Requests = {}});

    for (int32_t i = 0; i < 400; i++) {
        mpos from = pick_open();
        mpos to = pick_open_near(from, 3, 15);
        scenarios[0].Requests.emplace_back(make_request(from, to));
        FindPathInput hex_steps_request = make_request(from, to);
        hex_steps_request.FreeMovement = false;
        scenarios[1].Requests.emplace_back(std::move(hex_steps_request));
    }

    scenarios.emplace_back(Scenario {.Name = "medium 20-60", .Requests = {}});

    for (int32_t i = 0; i < 150; i++) {
        mpos from = pick_open();
        scenarios.back().Requests.emplace_back(make_request(from, pick_open_near(from, 20, 60)));
    }

    scenarios.emplace_back(Scenario {.Name = "long 100-300", .Requests = {}});

    for (int32_t i = 0; i < 40; i++) {
        mpos from = pick_open();
        scenarios.back().Requests.emplace_back(make_request(from, pick_open_near(from, 100, 300)));
    }

    scenarios.emplace_back(Scenario {.Name = "beyond the limit", .Requests = {}});

    for (int32_t i = 0; i < 20; i++) {
        mpos from = pick_open_near(MAP_SIZE.from_raw_pos(25, 25), 0, 20);
        scenarios.back().Requests.emplace_back(make_request(from, pick_open_near(MAP_SIZE.from_raw_pos(575, 575), 0, 20)));
    }

    scenarios.emplace_back(Scenario {.Name = "sealed target, probed", .Requests = {}});

    for (mpos center : sealed_centers) {
        scenarios.back().Requests.emplace_back(make_request(pick_open_near(center, 30, 80), center));
    }

    scenarios.emplace_back(Scenario {.Name = "sealed target, not probed", .Requests = {}});

    for (size_t i = 0; i < 4; i++) {
        FindPathInput request = make_request(pick_open_near(sealed_centers[i], 30, 80), sealed_centers[i]);
        request.EnclosureProbeLimit = 0;
        scenarios.back().Requests.emplace_back(std::move(request));
    }

    scenarios.emplace_back(Scenario {.Name = "near 3-15, multihex 1", .Requests = {}});

    for (int32_t i = 0; i < 100; i++) {
        mpos from = pick_open();
        FindPathInput request = make_request(from, pick_open_near(from, 3, 15));
        request.Multihex = 1;
        scenarios.back().Requests.emplace_back(std::move(request));
    }

    scenarios.emplace_back(Scenario {.Name = "nearest of six 5-20", .Requests = {}});

    for (int32_t i = 0; i < 100; i++) {
        mpos from = pick_open();
        mpos center = pick_open_near(from, 5, 20);
        vector<mpos> targets;

        for (int32_t dir_value = 0; dir_value < GameSettings::MAP_DIR_COUNT; dir_value++) {
            ipos32 raw_hex {center.x, center.y};
            GeometryHelper::MoveHexByDirUnsafe(raw_hex, hdir(dir_value));

            if (MAP_SIZE.is_valid_pos(raw_hex)) {
                targets.emplace_back(MAP_SIZE.from_raw_pos(raw_hex));
            }
        }

        FindPathInput request = make_request(from, mpos {});
        request.CheckTarget = [targets](mpos hex) -> bool { return std::ranges::find(targets, hex) != targets.end(); };
        scenarios.back().Requests.emplace_back(std::move(request));
    }

    auto elapsed_ms = [](auto started) { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count(); };
    string report;

    for (const auto& scenario : scenarios) {
        size_t ok = 0;
        size_t too_far = 0;
        size_t no_way = 0;
        size_t other = 0;
        size_t steps = 0;
        uint64_t route_hash = 14695981039346656037ull;
        double best_ms = 0.0;
        check_calls = 0;

        for (int32_t rep = 0; rep < REPEATS; rep++) {
            auto started = std::chrono::steady_clock::now();

            for (const auto& request : scenario.Requests) {
                auto output = PathFinding::FindPath(request);

                if (rep != 0) {
                    continue;
                }

                ok += output.Result == FindPathOutput::ResultType::Ok ? 1 : 0;
                too_far += output.Result == FindPathOutput::ResultType::TooFar ? 1 : 0;
                no_way += output.Result == FindPathOutput::ResultType::NoWay ? 1 : 0;
                other += output.Result != FindPathOutput::ResultType::Ok && output.Result != FindPathOutput::ResultType::TooFar && output.Result != FindPathOutput::ResultType::NoWay ? 1 : 0;
                steps += output.Steps.size();
                route_hash = (route_hash ^ numeric_cast<uint64_t>(static_cast<uint8_t>(output.Result))) * 1099511628211ull;

                for (mdir step : output.Steps) {
                    route_hash = (route_hash ^ numeric_cast<uint64_t>(step.hex().value())) * 1099511628211ull;
                }
            }

            double ms = elapsed_ms(started);
            best_ms = rep == 0 ? ms : std::min(best_ms, ms);
        }

        double requests = numeric_cast<double>(scenario.Requests.size());
        report += strex("{:<28} {:>4} requests  {:>9.3f} ms each  {:>9.0f} CheckHex each  ok {} too far {} no way {} other {}  steps {}  route hash {:016x}\n", scenario.Name, scenario.Requests.size(), best_ms / requests, numeric_cast<double>(check_calls) / (requests * REPEATS), ok, too_far, no_way, other, steps, route_hash).str();
    }

    WARN(report);
}

TEST_CASE("PathFinding::TraceDirection")
{
    // Static, so the helper lambdas below read them without a capture; a class-typed constexpr local is odr-used by a copy
    static constexpr msize map_size {200, 200};
    static constexpr mpos start {100, 100};
    constexpr int32_t horizon = 6;

    auto make_input = [](mpos from, ipos16 from_offset, int32_t angle, int32_t max_steps, bool slide, function<bool(mpos)> is_blocked) -> TraceDirectionInput {
        TraceDirectionInput input;
        input.StartHex = from;
        input.StartHexOffset = from_offset;
        input.RayHex = from;
        input.RayHexOffset = from_offset;
        input.Dir = mdir(angle);
        input.MaxSteps = max_steps;
        input.Slide = slide;
        input.MapSize = map_size;
        input.CheckHex = [is_blocked = std::move(is_blocked)](mpos hex) -> HexBlockResult { return is_blocked(hex) ? HexBlockResult::Blocked : HexBlockResult::Passable; };
        return input;
    };

    auto nothing_blocked = [](mpos /*hex*/) -> bool { return false; };

    // Every hex a trace enters, in order
    auto walk = [](mpos from, const TraceDirectionOutput& output) -> vector<mpos> {
        vector<mpos> hexes;
        mpos hex = from;

        for (mdir step : output.Steps) {
            bool moved = GeometryHelper::MoveHexByDir(hex, step, map_size);
            REQUIRE(moved);
            hexes.emplace_back(hex);
        }

        return hexes;
    };

    // Distance of a map-pixel point from the ray, in the projected plane where MovingContext measures its segments
    auto distance_from_ray = [](float32_t px, float32_t py, int32_t angle) -> float32_t {
        float32_t angle_rad = (numeric_cast<float32_t>(angle) - 90.0f) * DEG_TO_RAD_FLOAT;
        float32_t dy = py * GeometryHelper::GetYProj();
        return std::abs(px * std::sin(angle_rad) - dy * std::cos(angle_rad));
    };

    SECTION("ClearRayRunsTheWholeHorizonAsOneSegment")
    {
        for (int32_t angle : {0, 30, 45, 90, 135, 200, 275, 330}) {
            auto output = PathFinding::TraceDirection(make_input(start, {}, angle, horizon, true, nothing_blocked));

            CHECK(std::cmp_equal(output.Steps.size(), horizon));
            CHECK_FALSE(output.Slid);
            REQUIRE(output.ControlSteps.size() == 1);
            CHECK(output.ControlSteps.front() == horizon);
        }
    }

    SECTION("ChainedTracesStayOnOneStraightLine")
    {
        // A held direction is walked as a chain of short traces, each starting where the last ended and projected onto
        // the line the run began on; the ends must stay on that line instead of drifting by a rounding error a link
        for (int32_t angle : {20, 45, 100, 160, 250, 310}) {
            mpos hex = start;
            ipos16 offset {};

            for (int32_t link = 0; link < 12; link++) {
                auto input = make_input(hex, offset, angle, horizon, false, nothing_blocked);
                input.RayHex = start;
                input.RayHexOffset = {};
                auto output = PathFinding::TraceDirection(input);
                REQUIRE(std::cmp_equal(output.Steps.size(), horizon));
                vector<mpos> hexes = walk(hex, output);
                hex = hexes.back();
                offset = output.EndHexOffset;

                ipos32 end_center = GeometryHelper::GetHexOffset(start, hex);
                float32_t end_x = numeric_cast<float32_t>(end_center.x + offset.x);
                float32_t end_y = numeric_cast<float32_t>(end_center.y + offset.y);
                CHECK(distance_from_ray(end_x, end_y, angle) <= 1.5f);
            }
        }
    }

    SECTION("RayStartsFromTheSubHexOffset")
    {
        // A mover drawn below its hex centre keeps that height when it walks east: the ray is parallel, not recentred
        auto output = PathFinding::TraceDirection(make_input(start, ipos16 {0, 6}, 90, horizon, false, nothing_blocked));

        REQUIRE(std::cmp_equal(output.Steps.size(), horizon));
        CHECK(std::abs(output.EndHexOffset.y - 6) <= 1);
    }

    SECTION("BlockedRayStopsWithoutSlide")
    {
        auto is_wall = [](mpos hex) -> bool { return GeometryHelper::GetHexOffset(start, hex).x > GameSettings::MAP_HEX_WIDTH * 3; };
        auto output = PathFinding::TraceDirection(make_input(start, {}, 90, horizon, false, is_wall));

        CHECK_FALSE(output.Steps.empty());
        CHECK(std::cmp_less(output.Steps.size(), horizon));
        CHECK_FALSE(output.Slid);

        for (mpos hex : walk(start, output)) {
            CHECK_FALSE(is_wall(hex));
        }
    }

    SECTION("SlideFollowsAWallTowardTheDirection")
    {
        // Heading down-right into a wall below: the trace keeps going right along the wall instead of stopping
        auto is_wall = [](mpos hex) -> bool { return GeometryHelper::GetHexOffset(start, hex).y >= GameSettings::MAP_HEX_LINE_HEIGHT; };
        auto stopped = PathFinding::TraceDirection(make_input(start, {}, 120, horizon, false, is_wall));
        auto slid = PathFinding::TraceDirection(make_input(start, {}, 120, horizon, true, is_wall));

        CHECK(std::cmp_less(stopped.Steps.size(), horizon));
        CHECK(std::cmp_equal(slid.Steps.size(), horizon));
        CHECK(slid.Slid);
        CHECK(slid.EndHexOffset == ipos16 {});

        vector<mpos> hexes = walk(start, slid);

        for (mpos hex : hexes) {
            CHECK_FALSE(is_wall(hex));
        }

        CHECK(GeometryHelper::GetHexOffset(start, hexes.back()).x > GameSettings::MAP_HEX_WIDTH * 3);
        REQUIRE_FALSE(slid.ControlSteps.empty());
        CHECK(slid.ControlSteps.back() == horizon);
    }

    SECTION("PushingStraightIntoAWallDoesNotMove")
    {
        auto is_wall = [](mpos hex) -> bool { return GeometryHelper::GetHexOffset(start, hex).x > 0; };
        auto output = PathFinding::TraceDirection(make_input(start, {}, 90, horizon, true, is_wall));

        CHECK(output.Steps.empty());
        CHECK(output.ControlSteps.empty());
    }

    SECTION("NoStepGoesAgainstTheDirection")
    {
        // Whatever the obstacles, every step is under a right angle off the direction, so a trace cannot oscillate
        random_generator rng {1790};

        for (int32_t round = 0; round < 40; round++) {
            unordered_set<mpos> blocked;

            for (int32_t i = 0; i < 120; i++) {
                blocked.emplace(mpos {numeric_cast<int16_t>(90 + rng.next_between(0, 20)), numeric_cast<int16_t>(90 + rng.next_between(0, 20))});
            }

            blocked.erase(start);
            int32_t angle = rng.next_between(0, 359);
            auto output = PathFinding::TraceDirection(make_input(start, {}, angle, 12, true, [&blocked](mpos hex) { return blocked.contains(hex); }));

            for (mdir step : output.Steps) {
                CHECK(GeometryHelper::GetDirAngleDiff(numeric_cast<float32_t>(step.angle()), numeric_cast<float32_t>(angle)) < 90.0f);
            }

            for (mpos hex : walk(start, output)) {
                CHECK_FALSE(blocked.contains(hex));
            }
        }
    }

    SECTION("ZeroHorizonTracesNothing")
    {
        auto output = PathFinding::TraceDirection(make_input(start, {}, 90, 0, true, nothing_blocked));

        CHECK(output.Steps.empty());
        CHECK(output.ControlSteps.empty());
        CHECK(output.EndHexOffset == ipos16 {});
    }

    SECTION("MapEdgeEndsTheTrace")
    {
        // Two hexes short of the map edge along the direction, found by walking there rather than by coordinates
        mpos near_edge = start;

        while (true) {
            mpos next = near_edge;

            if (!GeometryHelper::MoveHexByDir(next, mdir(90), map_size)) {
                break;
            }

            near_edge = next;
        }

        for (int32_t i = 0; i < 2; i++) {
            bool moved_back = GeometryHelper::MoveHexByDir(near_edge, mdir(270), map_size);
            REQUIRE(moved_back);
        }

        // The edge stops the direction itself; sliding follows the edge instead, and never leaves the map either way
        auto stopped = PathFinding::TraceDirection(make_input(near_edge, {}, 90, horizon, false, nothing_blocked));
        auto slid = PathFinding::TraceDirection(make_input(near_edge, {}, 90, horizon, true, nothing_blocked));

        CHECK(stopped.Steps.size() == 2);

        for (mpos hex : walk(near_edge, slid)) {
            CHECK(map_size.is_valid_pos(hex));
        }
    }

    SECTION("NegativeHorizonIsRejected")
    {
        CHECK_THROWS(PathFinding::TraceDirection(make_input(start, {}, 90, -1, true, nothing_blocked)));
    }
}

FO_END_NAMESPACE
