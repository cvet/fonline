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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

FO_BEGIN_NAMESPACE();

TEST_CASE("GeometryHelper")
{
    // GetDistance
    CHECK(GeometryHelper::GetDistance(0, 0, 0, 0) == 0);
    CHECK(GeometryHelper::GetDistance(0, 0, 1, 0) >= 0);
    CHECK(GeometryHelper::GetDistance(mpos {0, 0}, mpos {1, 1}) >= 0);

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

    // ForEachMultihexLines
    vector<uint8> lines = {2, 2, 4, 1};
    mpos start {5, 5};
    int32 count = 0;
    GeometryHelper::ForEachMultihexLines(lines, start, map_size, [&](mpos pos) {
        CHECK(map_size.is_valid_pos(pos));
        count++;
    });
    CHECK(count == 3);
}

FO_END_NAMESPACE();
