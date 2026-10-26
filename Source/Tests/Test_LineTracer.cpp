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

        mpos current = start;
        int32_t initial_distance = GeometryHelper::GetDistance(start, target);
        REQUIRE(initial_distance > 0);

        for (int32_t step = 0; step < initial_distance; step++) {
            mpos previous = current;
            int32_t previous_distance = GeometryHelper::GetDistance(previous, target);
            auto dir = tracer.GetNextHex(current);

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

        mpos current = start;
        int32_t distance = GeometryHelper::GetDistance(start, target);
        REQUIRE(distance > 0);

        for (int32_t step = 0; step < distance; step++) {
            int32_t previous_distance = GeometryHelper::GetDistance(current, target);
            auto dir = tracer.GetNextHex(current);
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

        mpos current = start;
        auto dir = tracer.GetNextHex(current);

        CHECK(!dir.has_value());
        CHECK(current == start);
    }

    SECTION("DirectionTraceFollowsExpectedStraightSteps")
    {
        constexpr msize map_size {20, 20};
        constexpr mpos start {10, 10};
        constexpr int32_t expected_steps = 4;
        mdir expected_dir = mdir(hdir::SouthEast);

        mpos target = start;
        for (int32_t step = 0; step < expected_steps; step++) {
            REQUIRE(GeometryHelper::MoveHexByDir(target, expected_dir, map_size));
        }

        float32_t dir_angle = GeometryHelper::GetDirAngle(start, target);
        LineTracer tracer {start, dir_angle, expected_steps, map_size};

        mpos current = start;
        mpos expected = start;

        for (int32_t step = 0; step < expected_steps; step++) {
            REQUIRE(GeometryHelper::MoveHexByDir(expected, expected_dir, map_size));

            auto dir = tracer.GetNextHex(current);

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
        mdir east_dir = mdir(hdir::East);
        mdir south_east_dir = mdir(hdir::SouthEast);

        mpos base_target = start;
        mpos offset_target = start;
        REQUIRE(GeometryHelper::MoveHexByDir(base_target, east_dir, map_size));
        REQUIRE(GeometryHelper::MoveHexByDir(offset_target, south_east_dir, map_size));

        ipos16 target_offset {};

        if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
            float32_t delta_internal_x = numeric_cast<float32_t>(offset_target.x - base_target.x) * 3.0f;
            float32_t base_odd = numeric_cast<float32_t>(std::abs(base_target.x % 2));
            float32_t offset_odd = numeric_cast<float32_t>(std::abs(offset_target.x % 2));
            float32_t delta_internal_y = numeric_cast<float32_t>(offset_target.y - base_target.y) * SQRT3_X2_FLOAT - (offset_odd - base_odd) * SQRT3_FLOAT;

            target_offset = {
                numeric_cast<int16_t>(iround<int32_t>(delta_internal_x * numeric_cast<float32_t>(GameSettings::MAP_HEX_WIDTH) / 4.0f)),
                numeric_cast<int16_t>(iround<int32_t>(delta_internal_y * numeric_cast<float32_t>(GameSettings::MAP_HEX_HEIGHT) / SQRT3_X2_FLOAT)),
            };
        }
        else {
            ipos32 pixel_delta = GeometryHelper::GetHexOffset(base_target, offset_target);
            target_offset = {numeric_cast<int16_t>(pixel_delta.x), numeric_cast<int16_t>(pixel_delta.y)};

            if constexpr (GameSettings::MAP_HEX_WIDTH == 32 && GameSettings::MAP_HEX_LINE_HEIGHT == 8) {
                CHECK(target_offset == ipos16 {-16, 8});
            }
        }

        LineTracer base_tracer {start, base_target, 0.0f, map_size};
        LineTracer offset_tracer {start, base_target, 0.0f, map_size, {}, target_offset};
        LineTracer reference_tracer {start, offset_target, 0.0f, map_size};

        mpos base_pos = start;
        mpos offset_pos = start;
        mpos reference_pos = start;

        auto base_step = base_tracer.GetNextHex(base_pos);
        auto offset_step = offset_tracer.GetNextHex(offset_pos);
        auto reference_step = reference_tracer.GetNextHex(reference_pos);

        REQUIRE(base_step.has_value());
        REQUIRE(offset_step.has_value());
        REQUIRE(reference_step.has_value());
        CHECK(map_size.is_valid_pos(base_pos));
        CHECK(map_size.is_valid_pos(offset_pos));
        CHECK(offset_pos == reference_pos);
        CHECK(base_pos != offset_pos);

        if constexpr (!GameSettings::HEXAGONAL_GEOMETRY) {
            CHECK(base_pos == mpos {9, 11});
            CHECK(offset_pos == mpos {10, 11});
            CHECK(reference_pos == mpos {10, 11});
        }
    }

    SECTION("TraceRemainsWithinBoundsWhenContinuingPastTarget")
    {
        constexpr msize map_size {4, 4};
        constexpr mpos start {0, 0};
        constexpr mpos target {3, 3};
        LineTracer tracer {start, target, 0.0f, map_size};

        mpos pos = start;
        bool moved = false;

        for (size_t i = 0; i < 8; i++) {
            mpos previous = pos;
            auto dir = tracer.GetNextHex(pos);
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

TEST_CASE("LineTracer projected angular and pixel offsets")
{
    if constexpr (!GameSettings::HEXAGONAL_GEOMETRY) {
        constexpr msize map_size {30, 30};
        constexpr mpos start {10, 10};

        auto trace_path = [&](LineTracer& tracer, mpos from, int32_t steps) -> vector<mpos> {
            vector<mpos> path;
            mpos current = from;

            for (int32_t step = 0; step < steps; step++) {
                REQUIRE(tracer.GetNextHex(current).has_value());
                path.emplace_back(current);
            }

            return path;
        };

        SECTION("Quarter turns rotate the physical ray")
        {
            constexpr array<mpos, 4> zero_path {{{9, 11}, {8, 12}, {7, 13}, {6, 14}}};
            constexpr array<mpos, 4> positive_path {{{11, 11}, {12, 12}, {13, 13}, {14, 14}}};
            constexpr array<mpos, 4> negative_path {{{9, 9}, {8, 8}, {7, 7}, {6, 6}}};
            constexpr array<mpos, 4> reverse_path {{{11, 9}, {12, 8}, {13, 7}, {14, 6}}};
            constexpr array<mpos, 4> diagonal_positive_path {{{10, 11}, {10, 12}, {10, 13}, {10, 14}}};
            constexpr array<mpos, 4> diagonal_negative_path {{{9, 10}, {8, 10}, {7, 10}, {6, 10}}};

            auto check_angle = [&](float32_t offset, const array<mpos, 4>& expected) {
                LineTracer tracer {start, mpos {6, 14}, offset, map_size};
                vector<mpos> path = trace_path(tracer, start, 4);

                REQUIRE(path.size() == expected.size());

                for (size_t index = 0; index < expected.size(); index++) {
                    INFO(offset << " step " << index);
                    CHECK(path[index] == expected[index]);
                }
            };

            check_angle(0.0f, zero_path);
            check_angle(360.0f, zero_path);
            check_angle(90.0f, positive_path);
            check_angle(450.0f, positive_path);
            check_angle(-270.0f, positive_path);
            check_angle(-90.0f, negative_path);
            check_angle(270.0f, negative_path);
            check_angle(-450.0f, negative_path);
            check_angle(180.0f, reverse_path);
            check_angle(-180.0f, reverse_path);

            bool default_fixture = GameSettings::MAP_HEX_WIDTH == 32 && GameSettings::MAP_HEX_HEIGHT == 16 && std::abs(GameSettings::MAP_CAMERA_ANGLE - 25.6589f) <= 0.00001f;

            if (default_fixture) {
                check_angle(45.0f, diagonal_positive_path);
                check_angle(-45.0f, diagonal_negative_path);
            }
        }

        SECTION("Target pixel offset equals the raw lattice target")
        {
            constexpr mpos base_target {14, 10};
            constexpr mpos shifted_target {14, 11};
            constexpr array<mpos, 4> expected {{{11, 10}, {12, 11}, {13, 11}, {14, 11}}};
            ipos32 pixel_delta = GeometryHelper::GetHexOffset(base_target, shifted_target);
            ipos16 pixel_offset {numeric_cast<int16_t>(pixel_delta.x), numeric_cast<int16_t>(pixel_delta.y)};
            LineTracer offset_tracer {start, base_target, 0.0f, map_size, {}, pixel_offset};
            LineTracer reference_tracer {start, shifted_target, 0.0f, map_size};
            vector<mpos> offset_path = trace_path(offset_tracer, start, 4);
            vector<mpos> reference_path = trace_path(reference_tracer, start, 4);

            REQUIRE(offset_path.size() == expected.size());
            REQUIRE(reference_path.size() == expected.size());
            CHECK(offset_path == reference_path);

            for (size_t index = 0; index < expected.size(); index++) {
                CHECK(offset_path[index] == expected[index]);
            }
        }

        SECTION("Start pixel offset equals the shifted raw start")
        {
            constexpr mpos shifted_start {10, 11};
            constexpr mpos target {14, 11};
            constexpr array<mpos, 4> expected {{{11, 11}, {12, 11}, {13, 11}, {14, 11}}};
            ipos32 pixel_delta = GeometryHelper::GetHexOffset(start, shifted_start);
            ipos16 pixel_offset {numeric_cast<int16_t>(pixel_delta.x), numeric_cast<int16_t>(pixel_delta.y)};
            LineTracer offset_tracer {start, target, 0.0f, map_size, pixel_offset, {}};
            LineTracer reference_tracer {shifted_start, target, 0.0f, map_size};
            vector<mpos> offset_path = trace_path(offset_tracer, start, 4);
            vector<mpos> reference_path = trace_path(reference_tracer, shifted_start, 4);

            REQUIRE(offset_path.size() == expected.size());
            REQUIRE(reference_path.size() == expected.size());
            CHECK(offset_path == reference_path);

            for (size_t index = 0; index < expected.size(); index++) {
                CHECK(offset_path[index] == expected[index]);
            }
        }

        SECTION("Rotated traces stay within map bounds")
        {
            constexpr msize small_map {8, 8};
            constexpr mpos small_start {3, 3};
            constexpr mpos small_target {6, 3};

            for (float32_t offset : {90.0f, -90.0f, 180.0f, 450.0f}) {
                LineTracer tracer {small_start, small_target, offset, small_map};
                mpos current = small_start;

                for (int32_t step = 0; step < 12; step++) {
                    mpos previous = current;
                    auto dir = tracer.GetNextHex(current);

                    CHECK(small_map.is_valid_pos(current));

                    if (!dir.has_value()) {
                        CHECK(current == previous);
                        break;
                    }
                }
            }
        }
    }
    else {
        SECTION("Hex traces keep their existing offset frame")
        {
            constexpr msize map_size {20, 20};
            constexpr mpos start {10, 10};
            constexpr mpos target {14, 10};
            LineTracer tracer {start, target, 0.0f, map_size};
            mpos current = start;

            REQUIRE(tracer.GetNextHex(current).has_value());
            CHECK(map_size.is_valid_pos(current));
            CHECK(current != start);
        }
    }
}

FO_END_NAMESPACE
