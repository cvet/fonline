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

        auto current = start;
        const auto initial_distance = GeometryHelper::GetDistance(start, target);
        REQUIRE(initial_distance > 0);

        for (int32_t step = 0; step < initial_distance; step++) {
            const auto previous = current;
            const auto previous_distance = GeometryHelper::GetDistance(previous, target);
            const auto dir = tracer.GetNextHex(current);

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

        auto current = start;
        const auto distance = GeometryHelper::GetDistance(start, target);
        REQUIRE(distance > 0);

        for (int32_t step = 0; step < distance; step++) {
            const auto previous_distance = GeometryHelper::GetDistance(current, target);
            const auto dir = tracer.GetNextHex(current);
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

        auto current = start;
        const auto dir = tracer.GetNextHex(current);

        CHECK(!dir.has_value());
        CHECK(current == start);
    }

    SECTION("TraceRemainsWithinBoundsWhenContinuingPastTarget")
    {
        constexpr msize map_size {4, 4};
        constexpr mpos start {0, 0};
        constexpr mpos target {3, 3};
        LineTracer tracer {start, target, 0.0f, map_size};

        auto pos = start;
        bool moved = false;

        for (size_t i = 0; i < 8; i++) {
            const auto previous = pos;
            const auto dir = tracer.GetNextHex(pos);
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
