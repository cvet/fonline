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

#include "Common.h"

#include "Application.h"
#include "Geometry.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

TEST_CASE("Matrix convention")
{
    SECTION("mat44 uses GLM column-major storage and column-vector multiplication")
    {
        mat44 matrix = glm::translate(mat44 {1.0f}, vec3 {2.0f, 3.0f, 4.0f});
        glm::vec<4, float32_t, glm::defaultp> pos {10.0f, 20.0f, 30.0f, 1.0f};
        glm::vec<4, float32_t, glm::defaultp> transformed = matrix * pos;
        const float32_t* matrix_data = glm::value_ptr(matrix);

        CHECK(matrix[3][0] == 2.0f);
        CHECK(matrix[3][1] == 3.0f);
        CHECK(matrix[3][2] == 4.0f);
        CHECK(matrix_data[12] == 2.0f);
        CHECK(matrix_data[13] == 3.0f);
        CHECK(matrix_data[14] == 4.0f);
        CHECK(transformed.x == 12.0f);
        CHECK(transformed.y == 23.0f);
        CHECK(transformed.z == 34.0f);
        CHECK(transformed.w == 1.0f);
    }
}

TEST_CASE("GeometryHelper")
{
    // GetDistance
    CHECK(GeometryHelper::GetDistance(0, 0, 0, 0) == 0);
    CHECK(GeometryHelper::GetDistance(0, 0, 1, 0) >= 0);
    CHECK(GeometryHelper::GetDistance(mpos {0, 0}, mpos {1, 1}) >= 0);
    CHECK(GeometryHelper::GetDistance(1, 2, 7, 9) == GeometryHelper::GetDistance(7, 9, 1, 2));

    // GetStepsCoords
    fpos32 zero_steps = GeometryHelper::GetStepsCoords({}, {});
    CHECK(is_float_equal(zero_steps.x, 0.0f));
    CHECK(is_float_equal(zero_steps.y, 0.0f));

    // GetHexDir
    CHECK(GeometryHelper::GetHexDir(0, 0, 1, 0).value() <= 7);
    CHECK(GeometryHelper::GetHexDir(mpos {0, 0}, mpos {1, 0}).value() <= 7);
    CHECK(GeometryHelper::GetHexDir(0, 0, 1, 0, 10.0f).value() <= 7);
    CHECK(GeometryHelper::GetHexDir(mpos {0, 0}, mpos {1, 0}, 10.0f).value() <= 7);

    // GetDirAngle
    CHECK(GeometryHelper::GetDirAngle(0, 0, 1, 0) >= 0.0f);
    CHECK(GeometryHelper::GetDirAngle(mpos {0, 0}, mpos {1, 0}) >= 0.0f);

    // GetDirAngleDiff
    CHECK(GeometryHelper::GetDirAngleDiff(30.0f, 60.0f) == 30.0f);
    CHECK(GeometryHelper::GetDirAngleDiff(350.0f, 10.0f) == 20.0f);

    // GetDirAngleDiffSided
    CHECK(is_float_equal(GeometryHelper::GetDirAngleDiffSided(30.0f, 60.0f), 30.0f));
    CHECK(is_float_equal(GeometryHelper::GetDirAngleDiffSided(60.0f, 30.0f), -30.0f));

    // CheckDist
    CHECK(GeometryHelper::CheckDist(mpos {0, 0}, mpos {0, 0}, 0));
    CHECK_FALSE(GeometryHelper::CheckDist(mpos {0, 0}, mpos {10, 10}, 5));

    // MoveHexByDir
    mpos hex {5, 5};
    msize map_size {10, 10};
    bool moved = GeometryHelper::MoveHexByDir(hex, hdir::SouthEast, map_size);
    CHECK(moved);
    CHECK(map_size.is_valid_pos(hex));

    // MoveHexByDirUnsafe
    ipos32 ihex {5, 5};
    GeometryHelper::MoveHexByDirUnsafe(ihex, hdir::SouthEast);
    CHECK(map_size.is_valid_pos(ihex));

    for (int32_t dir_value = 0; dir_value < GameSettings::MAP_DIR_COUNT; dir_value++) {
        hdir hex_dir = hdir(dir_value);
        mdir dir = hex_dir;
        mdir reverse_dir = dir.reverse();

        CHECK(dir.hex() == hex_dir);
        CHECK(reverse_dir.reverse().hex() == hex_dir);

        ipos32 roundtrip_hex {5, 5};
        GeometryHelper::MoveHexByDirUnsafe(roundtrip_hex, dir);
        GeometryHelper::MoveHexByDirUnsafe(roundtrip_hex, reverse_dir);
        CHECK(roundtrip_hex == ipos32 {5, 5});
    }

    // MoveHexAroundAway
    constexpr ipos32 ihex3 {5, 5};
    ipos32 ihex4 = ihex3;
    ipos32 ihex5 = ihex3;
    ipos32 ihex6 = ihex3;
    ipos32 ihex7 = ihex3;
    ipos32 ihex8 = ihex3;
    ipos32 ihex9 = ihex3;
    GeometryHelper::MoveHexAroundAwayUnsafe(ihex4, 0);
    CHECK(GeometryHelper::GetDistance(ihex3, ihex4) == 0);
    GeometryHelper::MoveHexAroundAwayUnsafe(ihex5, 1);
    CHECK(GeometryHelper::GetDistance(ihex3, ihex5) == 1);
    GeometryHelper::MoveHexAroundAwayUnsafe(ihex6, 6);
    CHECK(GeometryHelper::GetDistance(ihex3, ihex6) == 1);
    GeometryHelper::MoveHexAroundAwayUnsafe(ihex7, GeometryHelper::HexesInRadius(1));
    CHECK(GeometryHelper::GetDistance(ihex3, ihex7) == 2);
    GeometryHelper::MoveHexAroundAwayUnsafe(ihex8, GeometryHelper::HexesInRadius(2));
    CHECK(GeometryHelper::GetDistance(ihex3, ihex8) == 3);
    GeometryHelper::MoveHexAroundAwayUnsafe(ihex9, GeometryHelper::HexesInRadius(3));
    CHECK(GeometryHelper::GetDistance(ihex3, ihex9) == 4);

    // MoveHexAroundAway (safe)
    mpos safe_hex {5, 5};
    CHECK(GeometryHelper::MoveHexAroundAway(safe_hex, 1, map_size));
    CHECK(GeometryHelper::GetDistance(mpos {5, 5}, safe_hex) == 1);

    mpos border_hex {0, 0};
    mpos border_before = border_hex;
    CHECK_FALSE(GeometryHelper::MoveHexAroundAway(border_hex, 1, map_size));
    CHECK(border_hex == border_before);

    // ForEachMultihexLines
    vector<uint8_t> lines = {2, 2, 4, 1};
    mpos start {5, 5};
    int32_t count = 0;
    GeometryHelper::ForEachMultihexLines(lines, start, map_size, [&](mpos pos) {
        CHECK(map_size.is_valid_pos(pos));
        count++;
    });
    CHECK(count == 3);

    // ForEachMultihexLines edge cases
    vector<uint8_t> invalid_and_odd = {9, 5, 2};
    int32_t skipped_count = 0;
    GeometryHelper::ForEachMultihexLines(invalid_and_odd, start, map_size, [&](mpos) { skipped_count++; });
    CHECK(skipped_count == 4);

    uint8_t south_east_dir = numeric_cast<uint8_t>(hdir::SouthEast.value());
    uint8_t north_west_dir = numeric_cast<uint8_t>(mdir(hdir::SouthEast).reverse().hex().value());
    vector<uint8_t> reverse_path = {south_east_dir, 1, north_west_dir, 1};
    int32_t reverse_count = 0;
    GeometryHelper::ForEachMultihexLines(reverse_path, start, map_size, [&](mpos pos) {
        CHECK(pos != start);
        reverse_count++;
    });
    CHECK(reverse_count == 1);

    // HexesInRadius
    CHECK(GeometryHelper::HexesInRadius(0) == 1);
    CHECK(GeometryHelper::HexesInRadius(1) == 1 + GameSettings::MAP_DIR_COUNT);
    CHECK(GeometryHelper::HexesInRadius(2) == 1 + GameSettings::MAP_DIR_COUNT * 3);
}

TEST_CASE("GetHexPos and GetHexPosCoord")
{
    constexpr int32_t w = GameSettings::MAP_HEX_WIDTH;
    constexpr int32_t half_w = w / 2;
    constexpr int32_t h = GameSettings::MAP_HEX_LINE_HEIGHT;
    constexpr int32_t hex_h = GameSettings::MAP_HEX_HEIGHT;

    SECTION("GetHexPos origin")
    {
        ipos32 pos = GeometryHelper::GetHexPos(ipos32 {0, 0});
        CHECK(pos.x == 0);
        CHECK(pos.y == 0);
    }

    SECTION("GetHexPos mpos overload matches ipos32 overload")
    {
        for (int16_t rx = 0; rx < 10; rx++) {
            for (int16_t ry = 0; ry < 10; ry++) {
                CHECK(GeometryHelper::GetHexPos(mpos {rx, ry}) == GeometryHelper::GetHexPos(ipos32 {rx, ry}));
            }
        }
    }

    SECTION("GetHexPosCoord at hex center returns exact hex")
    {
        for (int32_t rx = -5; rx <= 5; rx++) {
            for (int32_t ry = -5; ry <= 5; ry++) {
                ipos32 center = GeometryHelper::GetHexPos(ipos32 {rx, ry});
                ipos32 offset;
                ipos32 result = GeometryHelper::GetHexPosCoord(center, &offset);
                INFO("rx=" << rx << " ry=" << ry << " center=" << center.x << "," << center.y);
                CHECK(result.x == rx);
                CHECK(result.y == ry);
                CHECK(offset.x == 0);
                CHECK(offset.y == 0);
            }
        }
    }

    SECTION("GetHexPosCoord roundtrip with small offsets")
    {
        for (int32_t rx = -3; rx <= 3; rx++) {
            for (int32_t ry = -3; ry <= 3; ry++) {
                ipos32 center = GeometryHelper::GetHexPos(ipos32 {rx, ry});

                // Small offsets within hex interior
                for (int32_t dx = -2; dx <= 2; dx++) {
                    for (int32_t dy = -2; dy <= 2; dy++) {
                        ipos32 offset;
                        ipos32 result = GeometryHelper::GetHexPosCoord({center.x + dx, center.y + dy}, &offset);
                        INFO("rx=" << rx << " ry=" << ry << " dx=" << dx << " dy=" << dy);
                        CHECK(result.x == rx);
                        CHECK(result.y == ry);
                        CHECK(offset.x == dx);
                        CHECK(offset.y == dy);
                    }
                }
            }
        }
    }

    SECTION("GetHexPosCoord covers every pixel in grid area")
    {
        // For a block of hexes, verify that every pixel resolves to a valid hex
        // and the offset is within hex bounds
        constexpr int32_t test_range = 4;

        for (int32_t rx = -test_range; rx <= test_range; rx++) {
            for (int32_t ry = -test_range; ry <= test_range; ry++) {
                ipos32 center = GeometryHelper::GetHexPos(ipos32 {rx, ry});

                // Scan full hex bounding box area
                for (int32_t px = center.x - half_w; px <= center.x + half_w; px++) {
                    for (int32_t py = center.y - hex_h / 2; py <= center.y + hex_h / 2; py++) {
                        ipos32 offset;
                        ipos32 result = GeometryHelper::GetHexPosCoord({px, py}, &offset);

                        // Verify the result is self-consistent: center + offset == input pixel
                        ipos32 result_center = GeometryHelper::GetHexPos(result);
                        INFO("px=" << px << " py=" << py << " result=" << result.x << "," << result.y);
                        CHECK(result_center.x + offset.x == px);
                        CHECK(result_center.y + offset.y == py);

                        // Offset must be within hex bounds
                        CHECK(std::abs(offset.x) <= half_w);
                        CHECK(std::abs(offset.y) <= hex_h / 2);
                    }
                }
            }
        }
    }

    SECTION("GetHexPosCoord hex boundary continuity")
    {
        // Adjacent pixels should resolve to the same or neighboring hexes
        constexpr int32_t scan_min = -100;
        constexpr int32_t scan_max = 100;

        for (int32_t px = scan_min; px < scan_max; px++) {
            for (int32_t py = scan_min; py < scan_max; py++) {
                ipos32 hex_here = GeometryHelper::GetHexPosCoord({px, py});
                ipos32 hex_right = GeometryHelper::GetHexPosCoord({px + 1, py});
                ipos32 hex_down = GeometryHelper::GetHexPosCoord({px, py + 1});

                // Moving one pixel should change hex by at most 1
                int32_t dist_right = GeometryHelper::GetDistance(hex_here, hex_right);
                int32_t dist_down = GeometryHelper::GetDistance(hex_here, hex_down);
                INFO("px=" << px << " py=" << py);
                CHECK(dist_right <= 1);
                CHECK(dist_down <= 1);
            }
        }
    }

    SECTION("GetHexPosCoord without hex_offset (nullptr)")
    {
        ipos32 center = GeometryHelper::GetHexPos(ipos32 {3, 4});
        ipos32 result = GeometryHelper::GetHexPosCoord(center, nullptr);
        CHECK(result.x == 3);
        CHECK(result.y == 4);

        // Also via default argument
        ipos32 result2 = GeometryHelper::GetHexPosCoord(center);
        CHECK(result2.x == 3);
        CHECK(result2.y == 4);
    }

    SECTION("GetHexPosCoord negative coordinates")
    {
        for (int32_t rx = -10; rx <= 0; rx++) {
            for (int32_t ry = -10; ry <= 0; ry++) {
                ipos32 center = GeometryHelper::GetHexPos(ipos32 {rx, ry});
                ipos32 result = GeometryHelper::GetHexPosCoord(center);
                INFO("rx=" << rx << " ry=" << ry);
                CHECK(result.x == rx);
                CHECK(result.y == ry);
            }
        }
    }

    SECTION("GetHexPosCoord large coordinates")
    {
        const int32_t coords[] = {-100, -50, 0, 50, 100};
        for (int32_t rx : coords) {
            for (int32_t ry : coords) {
                ipos32 center = GeometryHelper::GetHexPos(ipos32 {rx, ry});
                ipos32 offset;
                ipos32 result = GeometryHelper::GetHexPosCoord(center, &offset);
                INFO("rx=" << rx << " ry=" << ry);
                CHECK(result.x == rx);
                CHECK(result.y == ry);
                CHECK(offset.x == 0);
                CHECK(offset.y == 0);
            }
        }
    }

    SECTION("GetHexOffset consistency")
    {
        for (int32_t ax = 0; ax < 5; ax++) {
            for (int32_t ay = 0; ay < 5; ay++) {
                for (int32_t bx = 0; bx < 5; bx++) {
                    for (int32_t by = 0; by < 5; by++) {
                        ipos32 from_raw {ax, ay};
                        ipos32 to_raw {bx, by};
                        ipos32 pixel_from = GeometryHelper::GetHexPos(from_raw);
                        ipos32 pixel_to = GeometryHelper::GetHexPos(to_raw);
                        ipos32 hex_offset = GeometryHelper::GetHexOffset(from_raw, to_raw);
                        CHECK(hex_offset.x == pixel_to.x - pixel_from.x);
                        CHECK(hex_offset.y == pixel_to.y - pixel_from.y);
                    }
                }
            }
        }
    }

    SECTION("GetHexOffset mpos overload matches ipos32 overload")
    {
        for (int16_t ax = 0; ax < 5; ax++) {
            for (int16_t ay = 0; ay < 5; ay++) {
                for (int16_t bx = 0; bx < 5; bx++) {
                    for (int16_t by = 0; by < 5; by++) {
                        CHECK(GeometryHelper::GetHexOffset(mpos {ax, ay}, mpos {bx, by}) == GeometryHelper::GetHexOffset(ipos32 {ax, ay}, ipos32 {bx, by}));
                    }
                }
            }
        }
    }

    SECTION("GetHexAxialCoord roundtrip")
    {
        for (int32_t rx = -5; rx <= 5; rx++) {
            for (int32_t ry = -5; ry <= 5; ry++) {
                ipos32 axial = GeometryHelper::GetHexAxialCoord(ipos32 {rx, ry});
                ipos32 pixel = GeometryHelper::GetHexPos(ipos32 {rx, ry});
                INFO("rx=" << rx << " ry=" << ry);
                CHECK(axial.x == pixel.x / half_w);
                CHECK(axial.y == pixel.y / h);
            }
        }
    }

    SECTION("GetHexAxialCoord mpos overload matches ipos32 overload")
    {
        for (int16_t rx = 0; rx < 10; rx++) {
            for (int16_t ry = 0; ry < 10; ry++) {
                CHECK(GeometryHelper::GetHexAxialCoord(mpos {rx, ry}) == GeometryHelper::GetHexAxialCoord(ipos32 {rx, ry}));
            }
        }
    }

    SECTION("GetHexPosCoord at hex vertices resolves correctly")
    {
        // Test points at hex vertices - should resolve to valid hexes
        for (int32_t rx = -3; rx <= 3; rx++) {
            for (int32_t ry = -3; ry <= 3; ry++) {
                ipos32 center = GeometryHelper::GetHexPos(ipos32 {rx, ry});

                if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
                    // Pointy-top hex vertices
                    const ipos32 vertices[] = {
                        {center.x, center.y - hex_h / 2}, // top
                        {center.x + half_w, center.y - hex_h / 4}, // upper-right
                        {center.x + half_w, center.y + hex_h / 4}, // lower-right
                        {center.x, center.y + hex_h / 2}, // bottom
                        {center.x - half_w, center.y + hex_h / 4}, // lower-left
                        {center.x - half_w, center.y - hex_h / 4}, // upper-left
                    };

                    for (const auto& v : vertices) {
                        ipos32 offset;
                        ipos32 result = GeometryHelper::GetHexPosCoord(v, &offset);
                        ipos32 result_center = GeometryHelper::GetHexPos(result);
                        INFO("rx=" << rx << " ry=" << ry << " vertex=" << v.x << "," << v.y);
                        CHECK(result_center.x + offset.x == v.x);
                        CHECK(result_center.y + offset.y == v.y);
                    }
                }
            }
        }
    }
}

TEST_CASE("Map camera world projection")
{
    const int32_t coords[] = {-50, -5, 0, 1, 7, 50};

    SECTION("ProjectWorldToMap of GetHexWorldPos reproduces GetHexPos at ground level")
    {
        for (int32_t rx : coords) {
            for (int32_t ry : coords) {
                ipos32 legacy = GeometryHelper::GetHexPos(ipos32 {rx, ry});
                vec3 world = GeometryHelper::GetHexWorldPos(ipos32 {rx, ry}, ipos32 {});
                vec3 projected = GeometryHelper::ProjectWorldToMap(world);
                INFO("rx=" << rx << " ry=" << ry);
                CHECK(is_float_equal(projected.x, numeric_cast<float32_t>(legacy.x)));
                CHECK(is_float_equal(projected.y, numeric_cast<float32_t>(legacy.y)));
            }
        }
    }

    SECTION("mpos overload matches ipos32 overload")
    {
        for (int16_t rx = 0; rx < 10; rx++) {
            for (int16_t ry = 0; ry < 10; ry++) {
                vec3 from_mpos = GeometryHelper::GetHexWorldPos(mpos {rx, ry}, ipos32 {});
                vec3 from_ipos = GeometryHelper::GetHexWorldPos(ipos32 {rx, ry}, ipos32 {});
                CHECK(is_float_equal(from_mpos.x, from_ipos.x));
                CHECK(is_float_equal(from_mpos.y, from_ipos.y));
                CHECK(is_float_equal(from_mpos.z, from_ipos.z));
            }
        }
    }

    SECTION("Elevation raises the sprite on screen and brings it nearer the camera")
    {
        vec3 ground = GeometryHelper::ProjectWorldToMap(GeometryHelper::GetHexWorldPos(ipos32 {3, 3}, ipos32 {}, 0.0f));
        vec3 raised = GeometryHelper::ProjectWorldToMap(GeometryHelper::GetHexWorldPos(ipos32 {3, 3}, ipos32 {}, 100.0f));
        // Higher elevation appears higher on screen (smaller Y in the Y-down map convention).
        CHECK(raised.y < ground.y);
        // Higher elevation is nearer the camera (larger depth), so it draws on top.
        CHECK(raised.z > ground.z);
        // X is unaffected by elevation.
        CHECK(is_float_equal(raised.x, ground.x));
    }

    SECTION("Hex offset moves the projected world position along the ground plane")
    {
        ipos32 offset {17, 23};
        vec3 base = GeometryHelper::ProjectWorldToMap(GeometryHelper::GetHexWorldPos(ipos32 {3, 3}, ipos32 {}, 0.0f));
        vec3 moved = GeometryHelper::ProjectWorldToMap(GeometryHelper::GetHexWorldPos(ipos32 {3, 3}, offset, 0.0f));
        float32_t angle_rad = GameSettings::MAP_CAMERA_ANGLE * DEG_TO_RAD_FLOAT;
        float32_t expected_depth_delta = numeric_cast<float32_t>(offset.y) * std::cos(angle_rad) / std::sin(angle_rad);

        CHECK(is_float_equal(moved.x, base.x + numeric_cast<float32_t>(offset.x)));
        CHECK(is_float_equal(moved.y, base.y + numeric_cast<float32_t>(offset.y)));
        CHECK(is_float_equal(moved.z, base.z + expected_depth_delta));
    }

    SECTION("ProjectMapYToGroundDepth matches projected horizontal ground")
    {
        const ipos32 offsets[] = {{0, 0}, {17, 23}, {-11, 7}};
        const float32_t elevations[] = {0.0f, 12.0f, 100.0f};

        for (int32_t rx : coords) {
            for (int32_t ry : coords) {
                for (ipos32 offset : offsets) {
                    for (float32_t elevation : elevations) {
                        vec3 projected = GeometryHelper::ProjectWorldToMap(GeometryHelper::GetHexWorldPos(ipos32 {rx, ry}, offset, elevation));
                        float32_t ground_depth = GeometryHelper::ProjectMapYToGroundDepth(projected.y, elevation);
                        INFO("rx=" << rx << " ry=" << ry << " ox=" << offset.x << " oy=" << offset.y << " elevation=" << elevation);
                        CHECK(is_float_equal(ground_depth, projected.z));
                    }
                }
            }
        }
    }

    SECTION("ProjectMapYToVerticalDepth matches projected standing plane")
    {
        const ipos32 offsets[] = {{0, 0}, {17, 23}, {-11, 7}};
        const float32_t elevations[] = {0.0f, 12.0f, 100.0f};
        const float32_t screen_offsets_y[] = {-150.0f, -32.0f, 0.0f, 12.0f};
        float32_t angle_rad = GameSettings::MAP_CAMERA_ANGLE * DEG_TO_RAD_FLOAT;
        float32_t cos_a = std::cos(angle_rad);

        for (int32_t rx : coords) {
            for (int32_t ry : coords) {
                for (ipos32 offset : offsets) {
                    for (float32_t elevation : elevations) {
                        vec3 anchor_world = GeometryHelper::GetHexWorldPos(ipos32 {rx, ry}, offset, elevation);
                        vec3 anchor_proj = GeometryHelper::ProjectWorldToMap(anchor_world);

                        for (float32_t screen_offset_y : screen_offsets_y) {
                            vec3 world = {anchor_world.x, anchor_world.y - screen_offset_y / cos_a, anchor_world.z};
                            vec3 projected = GeometryHelper::ProjectWorldToMap(world);
                            float32_t vertical_depth = GeometryHelper::ProjectMapYToVerticalDepth(projected.y, anchor_proj.y, anchor_proj.z);
                            INFO("rx=" << rx << " ry=" << ry << " ox=" << offset.x << " oy=" << offset.y << " elevation=" << elevation << " sy=" << screen_offset_y);
                            CHECK(is_float_equal(projected.y, anchor_proj.y + screen_offset_y));
                            CHECK(is_float_equal(vertical_depth, projected.z));
                        }
                    }
                }
            }
        }
    }

    SECTION("Southward hexes are nearer the camera than northward hexes (painter order)")
    {
        vec3 north = GeometryHelper::ProjectWorldToMap(GeometryHelper::GetHexWorldPos(ipos32 {0, 0}, ipos32 {}));
        vec3 south = GeometryHelper::ProjectWorldToMap(GeometryHelper::GetHexWorldPos(ipos32 {0, 10}, ipos32 {}));
        // Larger hex.y maps further down-screen and nearer the camera, matching the legacy painter's order.
        CHECK(south.y > north.y);
        CHECK(south.z > north.z);
    }

    SECTION("MakeMapCameraView reproduces ProjectWorldToMap and GetHexPos (scroll 0, zoom 1)")
    {
        mat44 view = GeometryHelper::MakeMapCameraView(GameSettings::MAP_CAMERA_ANGLE, 0.0f, fpos32 {0.0f, 0.0f}, 1.0f);

        for (int32_t rx : coords) {
            for (int32_t ry : coords) {
                vec3 world = GeometryHelper::GetHexWorldPos(ipos32 {rx, ry}, ipos32 {});
                vec3 ref = GeometryHelper::ProjectWorldToMap(world);
                ipos32 legacy = GeometryHelper::GetHexPos(ipos32 {rx, ry});
                glm::vec4 clip = view * glm::vec4 {world.x, world.y, world.z, 1.0f};
                INFO("rx=" << rx << " ry=" << ry);
                // The matrix agrees with the reference projection (and so with legacy GetHexPos) pixel-for-pixel.
                CHECK(is_float_equal(clip.x, ref.x));
                CHECK(is_float_equal(clip.y, ref.y));
                CHECK(is_float_equal(clip.z, ref.z));
                CHECK(is_float_equal(clip.x, numeric_cast<float32_t>(legacy.x)));
                CHECK(is_float_equal(clip.y, numeric_cast<float32_t>(legacy.y)));
            }
        }
    }

    SECTION("MakeMapCameraView folds in scroll (translate) then zoom (scale); depth unchanged")
    {
        fpos32 scroll {123.0f, -45.0f};
        float32_t zoom = 1.5f;
        mat44 view = GeometryHelper::MakeMapCameraView(GameSettings::MAP_CAMERA_ANGLE, 0.0f, scroll, zoom);
        vec3 world = GeometryHelper::GetHexWorldPos(ipos32 {7, 3}, ipos32 {});
        vec3 ref = GeometryHelper::ProjectWorldToMap(world);
        glm::vec4 clip = view * glm::vec4 {world.x, world.y, world.z, 1.0f};
        // Matches MapView::MapPosToScreenPos: screen = (mapPixel - scroll) * zoom; depth is independent.
        CHECK(is_float_equal(clip.x, (ref.x - scroll.x) * zoom));
        CHECK(is_float_equal(clip.y, (ref.y - scroll.y) * zoom));
        CHECK(is_float_equal(clip.z, ref.z));
    }

    SECTION("MakeMapAnchoredProj moves the local origin to the requested map anchor")
    {
        mat44 anchored = GeometryHelper::MakeMapAnchoredProj(mat44 {1.0f}, mat44 {1.0f}, fpos32 {12.0f, 34.0f}, 56.0f);
        glm::vec4 origin = anchored * glm::vec4 {0.0f, 0.0f, 0.0f, 1.0f};

        CHECK(is_float_equal(origin.x, 12.0f));
        CHECK(is_float_equal(origin.y, 34.0f));
        CHECK(is_float_equal(origin.z, 56.0f));
    }

    SECTION("MakeMapCameraView yaw leaves the vertical (up) axis projection invariant")
    {
        // Yaw rotates the world about the vertical, so a point on the up axis projects to the same screen point
        // and depth at any yaw (walls/models stay vertical on screen while the ground orbits beneath the camera).
        glm::vec4 up {0.0f, 100.0f, 0.0f, 1.0f};
        mat44 view0 = GeometryHelper::MakeMapCameraView(GameSettings::MAP_CAMERA_ANGLE, 0.0f, fpos32 {0.0f, 0.0f}, 1.0f);
        glm::vec4 ref = view0 * up;

        for (float32_t yaw : {30.0f, 90.0f, 137.0f, 270.0f}) {
            mat44 view = GeometryHelper::MakeMapCameraView(GameSettings::MAP_CAMERA_ANGLE, yaw, fpos32 {0.0f, 0.0f}, 1.0f);
            glm::vec4 p = view * up;
            INFO("yaw=" << yaw);
            CHECK(is_float_equal(p.x, ref.x));
            CHECK(is_float_equal(p.y, ref.y));
            CHECK(is_float_equal(p.z, ref.z));
        }
    }

    SECTION("MakeMapCameraView yaw 90 deg maps the +X ground axis onto the -Z ground axis")
    {
        // Orbiting 90 deg about the vertical sends the +X ground direction to -Z, so projecting +X at yaw 90
        // equals projecting -Z at yaw 0 — i.e. the camera really rotated around the scene.
        mat44 view0 = GeometryHelper::MakeMapCameraView(GameSettings::MAP_CAMERA_ANGLE, 0.0f, fpos32 {0.0f, 0.0f}, 1.0f);
        mat44 view90 = GeometryHelper::MakeMapCameraView(GameSettings::MAP_CAMERA_ANGLE, 90.0f, fpos32 {0.0f, 0.0f}, 1.0f);
        glm::vec4 a = view90 * glm::vec4 {50.0f, 0.0f, 0.0f, 1.0f};
        glm::vec4 b = view0 * glm::vec4 {0.0f, 0.0f, -50.0f, 1.0f};
        CHECK(is_float_equal(a.x, b.x));
        CHECK(is_float_equal(a.y, b.y));
        CHECK(is_float_equal(a.z, b.z));
    }
}

FO_END_NAMESPACE
