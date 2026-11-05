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

#include "Common.h"

#include "catch_amalgamated.hpp"

#include "LineTracer.h"

FO_BEGIN_NAMESPACE

TEST_CASE("LineTracer")
{
    SECTION("HexTraceReachesTargetWithMonotonicDistanceDrop")
    {
        constexpr msize map_size {20, 20};
        constexpr mpos start {5, 5};
        constexpr mpos target {8, 5};
        LineTracer tracer {start, target, 0.0f, map_size};

        mpos current {start};
        int32_t initial_distance {GeometryHelper::GetDistance(start, target)};
        REQUIRE(initial_distance > 0);

        for (int32_t step {0}; step < initial_distance; step++) {
            mpos previous {current};
            int32_t previous_distance {GeometryHelper::GetDistance(previous, target)};
            auto dir {tracer.GetNextHex(current)};

            REQUIRE(dir.has_value());
            CHECK(map_size.is_valid_pos(current));
            CHECK(GeometryHelper::GetDistance(previous, current) == 1);
            CHECK(GeometryHelper::GetDistance(current, target) == previous_distance - 1);
        }

        CHECK(current == target);
    }

    SECTION("AngledHexTraceReachesDiagonalTarget")
    {
        constexpr msize map_size {20, 20};
        constexpr mpos start {6, 6};
        constexpr mpos target {8, 8};
        LineTracer tracer {start, target, 0.0f, map_size};

        mpos current {start};
        int32_t distance {GeometryHelper::GetDistance(start, target)};
        REQUIRE(distance > 0);

        for (int32_t step {0}; step < distance; step++) {
            int32_t previous_distance {GeometryHelper::GetDistance(current, target)};
            auto dir {tracer.GetNextHex(current)};
            REQUIRE(dir.has_value());
            CHECK(map_size.is_valid_pos(current));
            CHECK(GeometryHelper::GetDistance(current, target) == previous_distance - 1);
        }

        CHECK(current == target);
    }

    SECTION("HexTraceReturnsNulloptWhenNoBorderStepExists")
    {
        constexpr msize map_size {1, 1};
        constexpr mpos start {0, 0};
        LineTracer tracer {start, 0.0f, 1, map_size};

        mpos current {start};
        auto dir {tracer.GetNextHex(current)};

        CHECK(!dir.has_value());
        CHECK(current == start);
    }

    SECTION("DirectionTraceFollowsExpectedStraightSteps")
    {
        constexpr msize map_size {20, 20};
        constexpr mpos start {10, 10};
        constexpr int32_t expected_steps {4};
        mdir expected_dir {mdir(hdir::SouthEast)};

        mpos target {start};

        for (int32_t step {0}; step < expected_steps; step++) {
            REQUIRE(GeometryHelper::MoveHexByDir(target, expected_dir, map_size));
        }

        float32_t dir_angle {GeometryHelper::GetDirAngle(start, target)};
        LineTracer tracer {start, dir_angle, expected_steps, map_size};

        mpos current {start};
        mpos expected {start};

        for (int32_t step {0}; step < expected_steps; step++) {
            REQUIRE(GeometryHelper::MoveHexByDir(expected, expected_dir, map_size));

            auto dir {tracer.GetNextHex(current)};

            REQUIRE(dir.has_value());
            CHECK(map_size.is_valid_pos(current));
            CHECK(current == expected);
        }

        CHECK(current == target);
    }

    SECTION("TargetOffsetChangesFirstChosenStep")
    {
        constexpr msize map_size {20, 20};
        constexpr mpos start {10, 10};
        mdir east_dir {mdir(hdir::East)};
        mdir south_east_dir {mdir(hdir::SouthEast)};

        mpos base_target {start};
        mpos offset_target {start};
        REQUIRE(GeometryHelper::MoveHexByDir(base_target, east_dir, map_size));
        REQUIRE(GeometryHelper::MoveHexByDir(offset_target, south_east_dir, map_size));

        ipos16 target_offset {};

        if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
            float32_t delta_internal_x {numeric_cast<float32_t>(offset_target.x - base_target.x) * 3.0f};
            float32_t base_odd {numeric_cast<float32_t>(std::abs(base_target.x % 2))};
            float32_t offset_odd {numeric_cast<float32_t>(std::abs(offset_target.x % 2))};
            float32_t delta_internal_y {numeric_cast<float32_t>(offset_target.y - base_target.y) * SQRT3_X2_FLOAT - (offset_odd - base_odd) * SQRT3_FLOAT};

            target_offset = {
                numeric_cast<int16_t>(iround<int32_t>(delta_internal_x * numeric_cast<float32_t>(GameSettings::MAP_HEX_WIDTH) / 4.0f)),
                numeric_cast<int16_t>(iround<int32_t>(delta_internal_y * numeric_cast<float32_t>(GameSettings::MAP_HEX_HEIGHT) / SQRT3_X2_FLOAT)),
            };
        }
        else {
            target_offset = {
                numeric_cast<int16_t>((offset_target.x - base_target.x) * GameSettings::MAP_HEX_WIDTH),
                numeric_cast<int16_t>((offset_target.y - base_target.y) * GameSettings::MAP_HEX_HEIGHT),
            };
        }

        LineTracer base_tracer {start, base_target, 0.0f, map_size};
        LineTracer offset_tracer {start, base_target, 0.0f, map_size, {}, target_offset};
        LineTracer reference_tracer {start, offset_target, 0.0f, map_size};

        mpos base_pos {start};
        mpos offset_pos {start};
        mpos reference_pos {start};

        auto base_step {base_tracer.GetNextHex(base_pos)};
        auto offset_step {offset_tracer.GetNextHex(offset_pos)};
        auto reference_step {reference_tracer.GetNextHex(reference_pos)};

        REQUIRE(base_step.has_value());
        REQUIRE(offset_step.has_value());
        REQUIRE(reference_step.has_value());
        CHECK(map_size.is_valid_pos(base_pos));
        CHECK(map_size.is_valid_pos(offset_pos));
        CHECK(offset_pos == reference_pos);
        CHECK(base_pos != offset_pos);
    }

    SECTION("TraceIsTheSameAnywhereOnTheMap")
    {
        // A line is traced relative to its start hex, so the same line far out on a continent-sized map takes the same
        // steps as near the origin; an even shift keeps hex columns of the same parity
        if constexpr (std::numeric_limits<mcoord_t>::max() < 128000) {
            SKIP("Map coordinates are 16-bit in this build");
        }
        else {
            constexpr msize near_map {400, 400};
            constexpr msize far_map {128000, 128000};
            constexpr int32_t shift {120000};
            random_generator rnd {5112026};
            auto shifted {[](mpos hex) -> mpos { return mpos {numeric_cast<mcoord_t>(hex.x + shift), numeric_cast<mcoord_t>(hex.y + shift)}; }};

            for (int32_t round {0}; round < 300; round++) {
                mpos start {near_map.from_raw_pos(rnd.next_between(150, 250), rnd.next_between(150, 250))};
                mpos target {near_map.from_raw_pos(start.x + rnd.next_between(-80, 80), start.y + rnd.next_between(-80, 80))};
                float32_t angle_offset {round % 3 == 0 ? numeric_cast<float32_t>(rnd.next_between(-30, 30)) : 0.0f};
                ipos16 start_offset {numeric_cast<int16_t>(rnd.next_between(-8, 8)), numeric_cast<int16_t>(rnd.next_between(-8, 8))};
                ipos16 target_offset {numeric_cast<int16_t>(rnd.next_between(-8, 8)), numeric_cast<int16_t>(rnd.next_between(-8, 8))};
                float32_t dir_angle {numeric_cast<float32_t>(rnd.next_between(0, 359))};
                int32_t dist {rnd.next_between(1, 90)};

                LineTracer near_tracer {start, target, angle_offset, near_map, start_offset, target_offset};
                LineTracer far_tracer {shifted(start), shifted(target), angle_offset, far_map, start_offset, target_offset};
                LineTracer near_ray {start, dir_angle, dist, near_map, start_offset};
                LineTracer far_ray {shifted(start), dir_angle, dist, far_map, start_offset};

                mpos near_hex {start};
                mpos far_hex {shifted(start)};
                mpos near_ray_hex {start};
                mpos far_ray_hex {shifted(start)};

                for (int32_t step {0}; step < 120; step++) {
                    INFO("round " << round << " step " << step);
                    auto near_dir {near_tracer.GetNextHex(near_hex)};
                    auto far_dir {far_tracer.GetNextHex(far_hex)};
                    REQUIRE(near_dir.has_value() == far_dir.has_value());
                    CHECK(far_hex == shifted(near_hex));

                    auto near_ray_dir {near_ray.GetNextHex(near_ray_hex)};
                    auto far_ray_dir {far_ray.GetNextHex(far_ray_hex)};
                    REQUIRE(near_ray_dir.has_value() == far_ray_dir.has_value());
                    CHECK(far_ray_hex == shifted(near_ray_hex));
                }
            }
        }
    }

    SECTION("TraceRemainsWithinBoundsWhenContinuingPastTarget")
    {
        constexpr msize map_size {4, 4};
        constexpr mpos start {0, 0};
        constexpr mpos target {3, 3};
        LineTracer tracer {start, target, 0.0f, map_size};

        mpos pos {start};
        bool moved {false};

        for (size_t i {0}; i < 8; i++) {
            mpos previous {pos};
            auto dir {tracer.GetNextHex(pos)};
            CHECK(map_size.is_valid_pos(pos));

            if (dir.has_value()) {
                moved = moved || pos != previous;
            }
            else {
                CHECK(pos == previous);
            }
        }

        CHECK(moved);
    }
}

FO_END_NAMESPACE
