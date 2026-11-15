// FOnline Engine
// SPDX-License-Identifier: MIT

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
        constexpr bool wide {std::numeric_limits<mcoord_t>::max() >= 128000};

        if constexpr (!wide) {
            SKIP("Map coordinates are 16-bit in this build");
        }
        else {
            constexpr mcoord_t far_side {wide ? 128000 : 32000};
            constexpr msize near_map {400, 400};
            constexpr msize far_map {far_side, far_side};
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

    SECTION("VeryLongTracesReachTheirTargetAndStayInvariant")
    {
        // Lines tens of thousands of cells long close in on their target by one cell every step and arrive exactly, and
        // the same lines and rays far out on the map take the same steps; an even shift keeps hex columns aligned
        constexpr bool wide {std::numeric_limits<mcoord_t>::max() >= 128000};
        constexpr mcoord_t map_side {wide ? 128000 : 32000};
        constexpr msize map_size {map_side, map_side};
        constexpr int32_t span {wide ? 30000 : 7000};
        constexpr int32_t shift {wide ? 60000 : 16000};
        random_generator rnd {5112027};
        auto shifted {[](mpos hex) -> mpos { return mpos {numeric_cast<mcoord_t>(hex.x + shift), numeric_cast<mcoord_t>(hex.y + shift)}; }};

        for (int32_t round {0}; round < 24; round++) {
            mpos start {map_size.from_raw_pos(span + rnd.next_between(-50, 50), span + rnd.next_between(-50, 50))};
            mpos target {map_size.from_raw_pos(start.x + rnd.next_between(-span + 100, span - 100), start.y + rnd.next_between(-span + 100, span - 100))};
            int32_t distance {GeometryHelper::GetDistance(start, target)};

            LineTracer tracer {start, target, 0.0f, map_size};
            LineTracer far_tracer {shifted(start), shifted(target), 0.0f, map_size};
            mpos current {start};
            mpos far_current {shifted(start)};

            for (int32_t step {0}; step < distance; step++) {
                int32_t previous_distance {GeometryHelper::GetDistance(current, target)};
                auto dir {tracer.GetNextHex(current)};
                auto far_dir {far_tracer.GetNextHex(far_current)};

                INFO("round " << round << " step " << step << " of " << distance);
                REQUIRE(dir.has_value());
                REQUIRE(far_dir.has_value());
                REQUIRE(GeometryHelper::GetDistance(current, target) == previous_distance - 1);
                REQUIRE(far_current == shifted(current));
            }

            CHECK(current == target);

            float32_t dir_angle {numeric_cast<float32_t>(rnd.next_between(0, 3599)) / 10.0f};
            // A ray of n cells may cross up to 2n cells on an isometric square map
            int32_t ray_dist {rnd.next_between(span / 8, span / 4)};
            LineTracer ray {start, dir_angle, ray_dist, map_size};
            LineTracer far_ray {shifted(start), dir_angle, ray_dist, map_size};
            mpos ray_hex {start};
            mpos far_ray_hex {shifted(start)};

            for (int32_t step {0}; step < ray_dist; step++) {
                mpos previous {ray_hex};
                auto ray_dir {ray.GetNextHex(ray_hex)};
                auto far_ray_dir {far_ray.GetNextHex(far_ray_hex)};

                INFO("round " << round << " ray step " << step << " of " << ray_dist);
                REQUIRE(ray_dir.has_value() == far_ray_dir.has_value());

                if (!ray_dir.has_value()) {
                    break;
                }

                REQUIRE(GeometryHelper::GetDistance(previous, ray_hex) == 1);
                REQUIRE(far_ray_hex == shifted(ray_hex));
            }
        }
    }

    SECTION("VeryLongSquareTracesFollowTheExactLine")
    {
        // A square trace steps one cell along its main axis and rounds the other to the nearest cell of the exact line,
        // which integer arithmetic gives without error; only an exact tie may round either way
        if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
            SKIP("Square geometry only");
        }
        else {
            constexpr bool wide {std::numeric_limits<mcoord_t>::max() >= 128000};
            constexpr mcoord_t map_side {wide ? 128000 : 32000};
            constexpr msize map_size {map_side, map_side};
            constexpr int32_t span {wide ? 60000 : 15000};
            random_generator rnd {5112028};

            for (int32_t round {0}; round < 40; round++) {
                mpos start {map_size.from_raw_pos(span + rnd.next_between(-20, 20), span + rnd.next_between(-20, 20))};
                mpos target {map_size.from_raw_pos(start.x + rnd.next_between(-span + 50, span - 50), start.y + rnd.next_between(-span + 50, span - 50))};
                int64_t tx {target.x - start.x};
                int64_t ty {target.y - start.y};
                bool x_main {std::abs(tx) > std::abs(ty)};
                int64_t main_total {x_main ? std::abs(tx) : std::abs(ty)};
                int64_t main_sign {(x_main ? tx : ty) > 0 ? 1 : -1};
                int64_t side_total {x_main ? ty : tx};

                if (main_total == 0) {
                    continue;
                }

                LineTracer tracer {start, target, 0.0f, map_size};
                mpos current {start};

                for (int64_t step {1}; step <= main_total; step++) {
                    REQUIRE(tracer.GetNextHex(current).has_value());

                    // floor(0.5 + step * side_total / main_total), exactly: a tie lands on a whole number
                    int64_t numerator {main_total + 2 * step * side_total};
                    int64_t denominator {2 * main_total};
                    int64_t side {numerator >= 0 ? numerator / denominator : -((-numerator + denominator - 1) / denominator)};
                    bool tie {numerator % denominator == 0};
                    int64_t main {main_sign * step};
                    mpos expected {x_main ? map_size.from_raw_pos(numeric_cast<int32_t>(start.x + main), numeric_cast<int32_t>(start.y + side)) : map_size.from_raw_pos(numeric_cast<int32_t>(start.x + side), numeric_cast<int32_t>(start.y + main))};
                    mpos tie_low {x_main ? map_size.from_raw_pos(numeric_cast<int32_t>(start.x + main), numeric_cast<int32_t>(start.y + side - 1)) : map_size.from_raw_pos(numeric_cast<int32_t>(start.x + side - 1), numeric_cast<int32_t>(start.y + main))};

                    INFO("round " << round << " step " << step << " of " << main_total);
                    CHECK((current == expected || (tie && current == tie_low)));
                }

                CHECK(current == target);
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
