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

TEST_CASE("GeometryHelper")
{
    // GetDistance
    CHECK(GeometryHelper::GetDistance(0, 0, 0, 0) == 0);
    CHECK(GeometryHelper::GetDistance(0, 0, 1, 0) >= 0);
    CHECK(GeometryHelper::GetDistance(mpos {0, 0}, mpos {1, 1}) >= 0);
    CHECK(GeometryHelper::GetDistance(1, 2, 7, 9) == GeometryHelper::GetDistance(7, 9, 1, 2));

    // GetDir
    CHECK(GeometryHelper::GetDir(0, 0, 1, 0) <= 7);
    CHECK(GeometryHelper::GetDir(mpos {0, 0}, mpos {1, 0}) <= 7);
    CHECK(GeometryHelper::GetDir(0, 0, 1, 0, 10.0f) <= 7);
    CHECK(GeometryHelper::GetDir(mpos {0, 0}, mpos {1, 0}, 10.0f) <= 7);

    // GetDirAngle
    CHECK(GeometryHelper::GetDirAngle(0, 0, 1, 0) >= 0.0f);
    CHECK(GeometryHelper::GetDirAngle(mpos {0, 0}, mpos {1, 0}) >= 0.0f);

    // GetDirAngleDiff
    CHECK(GeometryHelper::GetDirAngleDiff(30.0f, 60.0f) == 30.0f);
    CHECK(GeometryHelper::GetDirAngleDiff(350.0f, 10.0f) == 20.0f);

    // GetDirAngleDiffSided
    CHECK(is_float_equal(GeometryHelper::GetDirAngleDiffSided(30.0f, 60.0f), 30.0f));
    CHECK(is_float_equal(GeometryHelper::GetDirAngleDiffSided(60.0f, 30.0f), -30.0f));

    // DirToAngle / AngleToDir / NormalizeAngle
    for (uint8 d = 0; d < 6; d++) {
        int16 angle = GeometryHelper::DirToAngle(d);
        uint8 dir = GeometryHelper::AngleToDir(angle);
        CHECK(dir == d);
        CHECK(GeometryHelper::NormalizeAngle(angle + 360 * 2) == angle);
    }
    CHECK(GeometryHelper::AngleToDir(-360) == 0);
    CHECK(GeometryHelper::AngleToDir(360) == 0);
    CHECK(GeometryHelper::AngleToDir(719) == GeometryHelper::AngleToDir(-1));
    CHECK(GeometryHelper::NormalizeAngle(721) == 1);
    CHECK(GeometryHelper::NormalizeAngle(-1) == 359);
    CHECK(GeometryHelper::NormalizeAngle(-360) == 0);
    CHECK(GeometryHelper::NormalizeAngle(-720) == 0);
    CHECK(GeometryHelper::NormalizeAngle(-721) == 359);

    // CheckDist
    CHECK(GeometryHelper::CheckDist(mpos {0, 0}, mpos {0, 0}, 0));
    CHECK_FALSE(GeometryHelper::CheckDist(mpos {0, 0}, mpos {10, 10}, 5));

    // ReverseDir
    for (uint8 d = 0; d < GameSettings::MAP_DIR_COUNT; d++) {
        uint8 rev = GeometryHelper::ReverseDir(d);
        CHECK(GeometryHelper::ReverseDir(rev) == d);
    }

    // MoveHexByDir
    mpos hex {5, 5};
    msize map_size {10, 10};
    bool moved = GeometryHelper::MoveHexByDir(hex, 2, map_size);
    CHECK(moved);
    CHECK(map_size.is_valid_pos(hex));

    // MoveHexByDirUnsafe
    ipos32 ihex {5, 5};
    GeometryHelper::MoveHexByDirUnsafe(ihex, 2);
    CHECK(map_size.is_valid_pos(ihex));

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
    const mpos border_before = border_hex;
    CHECK_FALSE(GeometryHelper::MoveHexAroundAway(border_hex, 1, map_size));
    CHECK(border_hex == border_before);

    // ForEachMultihexLines
    vector<uint8> lines = {2, 2, 4, 1};
    mpos start {5, 5};
    int32 count = 0;
    GeometryHelper::ForEachMultihexLines(lines, start, map_size, [&](mpos pos) {
        CHECK(map_size.is_valid_pos(pos));
        count++;
    });
    CHECK(count == 3);

    // ForEachMultihexLines edge cases
    vector<uint8> invalid_and_odd = {9, 5, 2};
    int32 skipped_count = 0;
    GeometryHelper::ForEachMultihexLines(invalid_and_odd, start, map_size, [&](mpos) { skipped_count++; });
    CHECK(skipped_count == 0);

    vector<uint8> reverse_path = {2, 1, 5, 1};
    int32 reverse_count = 0;
    GeometryHelper::ForEachMultihexLines(reverse_path, start, map_size, [&](mpos pos) {
        CHECK(pos != start);
        reverse_count++;
    });
    CHECK(reverse_count == 1);

    // HexesInRadius
    CHECK(GeometryHelper::HexesInRadius(0) == 1);
    CHECK(GeometryHelper::HexesInRadius(1) == 7);
    CHECK(GeometryHelper::HexesInRadius(2) == 19);
}

TEST_CASE("GetHexPos and GetHexPosCoord")
{
    GlobalSettings settings {false};
    settings.ApplyDefaultSettings();
    GeometryHelper geo {settings.Geometry};

    const int32 w = settings.Geometry.MapHexWidth;
    const int32 half_w = w / 2;
    const int32 h = settings.Geometry.MapHexLineHeight;
    const int32 hex_h = settings.Geometry.MapHexHeight;

    SECTION("GetHexPos origin")
    {
        const ipos32 pos = geo.GetHexPos(ipos32 {0, 0});
        CHECK(pos.x == 0);
        CHECK(pos.y == 0);
    }

    SECTION("GetHexPos mpos overload matches ipos32 overload")
    {
        for (int16 rx = 0; rx < 10; rx++) {
            for (int16 ry = 0; ry < 10; ry++) {
                CHECK(geo.GetHexPos(mpos {rx, ry}) == geo.GetHexPos(ipos32 {rx, ry}));
            }
        }
    }

    SECTION("GetHexPosCoord at hex center returns exact hex")
    {
        for (int32 rx = -5; rx <= 5; rx++) {
            for (int32 ry = -5; ry <= 5; ry++) {
                const ipos32 center = geo.GetHexPos(ipos32 {rx, ry});
                ipos32 offset;
                const ipos32 result = geo.GetHexPosCoord(center, &offset);
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
        for (int32 rx = -3; rx <= 3; rx++) {
            for (int32 ry = -3; ry <= 3; ry++) {
                const ipos32 center = geo.GetHexPos(ipos32 {rx, ry});

                // Small offsets within hex interior
                for (int32 dx = -2; dx <= 2; dx++) {
                    for (int32 dy = -2; dy <= 2; dy++) {
                        ipos32 offset;
                        const ipos32 result = geo.GetHexPosCoord({center.x + dx, center.y + dy}, &offset);
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
        constexpr int32 test_range = 4;

        for (int32 rx = -test_range; rx <= test_range; rx++) {
            for (int32 ry = -test_range; ry <= test_range; ry++) {
                const ipos32 center = geo.GetHexPos(ipos32 {rx, ry});

                // Scan full hex bounding box area
                for (int32 px = center.x - half_w; px <= center.x + half_w; px++) {
                    for (int32 py = center.y - hex_h / 2; py <= center.y + hex_h / 2; py++) {
                        ipos32 offset;
                        const ipos32 result = geo.GetHexPosCoord({px, py}, &offset);

                        // Verify the result is self-consistent: center + offset == input pixel
                        const ipos32 result_center = geo.GetHexPos(result);
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
        constexpr int32 scan_min = -100;
        constexpr int32 scan_max = 100;

        for (int32 px = scan_min; px < scan_max; px++) {
            for (int32 py = scan_min; py < scan_max; py++) {
                const ipos32 hex_here = geo.GetHexPosCoord({px, py});
                const ipos32 hex_right = geo.GetHexPosCoord({px + 1, py});
                const ipos32 hex_down = geo.GetHexPosCoord({px, py + 1});

                // Moving one pixel should change hex by at most 1
                const int32 dist_right = GeometryHelper::GetDistance(hex_here, hex_right);
                const int32 dist_down = GeometryHelper::GetDistance(hex_here, hex_down);
                INFO("px=" << px << " py=" << py);
                CHECK(dist_right <= 1);
                CHECK(dist_down <= 1);
            }
        }
    }

    SECTION("GetHexPosCoord without hex_offset (nullptr)")
    {
        const ipos32 center = geo.GetHexPos(ipos32 {3, 4});
        const ipos32 result = geo.GetHexPosCoord(center, nullptr);
        CHECK(result.x == 3);
        CHECK(result.y == 4);

        // Also via default argument
        const ipos32 result2 = geo.GetHexPosCoord(center);
        CHECK(result2.x == 3);
        CHECK(result2.y == 4);
    }

    SECTION("GetHexPosCoord negative coordinates")
    {
        for (int32 rx = -10; rx <= 0; rx++) {
            for (int32 ry = -10; ry <= 0; ry++) {
                const ipos32 center = geo.GetHexPos(ipos32 {rx, ry});
                const ipos32 result = geo.GetHexPosCoord(center);
                INFO("rx=" << rx << " ry=" << ry);
                CHECK(result.x == rx);
                CHECK(result.y == ry);
            }
        }
    }

    SECTION("GetHexPosCoord large coordinates")
    {
        const int32 coords[] = {-100, -50, 0, 50, 100};
        for (int32 rx : coords) {
            for (int32 ry : coords) {
                const ipos32 center = geo.GetHexPos(ipos32 {rx, ry});
                ipos32 offset;
                const ipos32 result = geo.GetHexPosCoord(center, &offset);
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
        for (int32 ax = 0; ax < 5; ax++) {
            for (int32 ay = 0; ay < 5; ay++) {
                for (int32 bx = 0; bx < 5; bx++) {
                    for (int32 by = 0; by < 5; by++) {
                        const ipos32 from_raw {ax, ay};
                        const ipos32 to_raw {bx, by};
                        const ipos32 pixel_from = geo.GetHexPos(from_raw);
                        const ipos32 pixel_to = geo.GetHexPos(to_raw);
                        const ipos32 hex_offset = geo.GetHexOffset(from_raw, to_raw);
                        CHECK(hex_offset.x == pixel_to.x - pixel_from.x);
                        CHECK(hex_offset.y == pixel_to.y - pixel_from.y);
                    }
                }
            }
        }
    }

    SECTION("GetHexOffset mpos overload matches ipos32 overload")
    {
        for (int16 ax = 0; ax < 5; ax++) {
            for (int16 ay = 0; ay < 5; ay++) {
                for (int16 bx = 0; bx < 5; bx++) {
                    for (int16 by = 0; by < 5; by++) {
                        CHECK(geo.GetHexOffset(mpos {ax, ay}, mpos {bx, by}) == geo.GetHexOffset(ipos32 {ax, ay}, ipos32 {bx, by}));
                    }
                }
            }
        }
    }

    SECTION("GetHexAxialCoord roundtrip")
    {
        for (int32 rx = -5; rx <= 5; rx++) {
            for (int32 ry = -5; ry <= 5; ry++) {
                const ipos32 axial = geo.GetHexAxialCoord(ipos32 {rx, ry});
                const ipos32 pixel = geo.GetHexPos(ipos32 {rx, ry});
                INFO("rx=" << rx << " ry=" << ry);
                CHECK(axial.x == pixel.x / half_w);
                CHECK(axial.y == pixel.y / h);
            }
        }
    }

    SECTION("GetHexAxialCoord mpos overload matches ipos32 overload")
    {
        for (int16 rx = 0; rx < 10; rx++) {
            for (int16 ry = 0; ry < 10; ry++) {
                CHECK(geo.GetHexAxialCoord(mpos {rx, ry}) == geo.GetHexAxialCoord(ipos32 {rx, ry}));
            }
        }
    }

    SECTION("GetHexPosCoord at hex vertices resolves correctly")
    {
        // Test points at hex vertices - should resolve to valid hexes
        for (int32 rx = -3; rx <= 3; rx++) {
            for (int32 ry = -3; ry <= 3; ry++) {
                const ipos32 center = geo.GetHexPos(ipos32 {rx, ry});

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
                        const ipos32 result = geo.GetHexPosCoord(v, &offset);
                        const ipos32 result_center = geo.GetHexPos(result);
                        INFO("rx=" << rx << " ry=" << ry << " vertex=" << v.x << "," << v.y);
                        CHECK(result_center.x + offset.x == v.x);
                        CHECK(result_center.y + offset.y == v.y);
                    }
                }
            }
        }
    }
}

FO_END_NAMESPACE
