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

#include "SpriteManager.h"

FO_BEGIN_NAMESPACE

// East-west wall runs share y and answer ByY; north-south runs share x and answer ByX
TEST_CASE("TransparentEggCutsEveryPieceOfARunStandingOnItsOwnLine")
{
    constexpr int16_t line = 10;

    // Both column parities must cut the entire wall run when the egg sits on its base line
    constexpr int16_t egg_columns[] = {8, 9};

    for (int16_t egg_along : egg_columns) {
        for (int16_t along = 4; along <= 13; along++) {
            CAPTURE(egg_along, along);

            for (TransparentEggTarget target : {TransparentEggTarget::AnyOccluder, TransparentEggTarget::Structure}) {
                CHECK(IsCutByTransparentEgg(target, mpos {egg_along, line}, EggAppearenceType::ByY, true, mpos {along, line}));
                CHECK(IsCutByTransparentEgg(target, mpos {line, egg_along}, EggAppearenceType::ByX, true, mpos {line, along}));
            }
        }
    }

    // Corner pieces on the egg's line follow the same rule
    CHECK(IsCutByTransparentEgg(TransparentEggTarget::Structure, mpos {8, line}, EggAppearenceType::ByXOrY, true, mpos {9, line}));
    CHECK(IsCutByTransparentEgg(TransparentEggTarget::Structure, mpos {8, line}, EggAppearenceType::ByXAndY, true, mpos {7, line}));
}

TEST_CASE("TransparentEggLeavesARunBehindItsPointStanding")
{
    constexpr int16_t line = 10;

    for (int16_t along = 4; along <= 13; along++) {
        CAPTURE(along);
        CHECK_FALSE(IsCutByTransparentEgg(TransparentEggTarget::AnyOccluder, mpos {8, line + 2}, EggAppearenceType::ByY, true, mpos {along, line}));
        CHECK_FALSE(IsCutByTransparentEgg(TransparentEggTarget::AnyOccluder, mpos {line + 2, 8}, EggAppearenceType::ByX, true, mpos {line, along}));
        CHECK(IsCutByTransparentEgg(TransparentEggTarget::AnyOccluder, mpos {8, line - 2}, EggAppearenceType::ByY, true, mpos {along, line}));
        CHECK(IsCutByTransparentEgg(TransparentEggTarget::AnyOccluder, mpos {line - 2, 8}, EggAppearenceType::ByX, true, mpos {line, along}));
    }
}

TEST_CASE("TransparentEggStructureTargetSparesPropsInFrontOfAWall")
{
    // The point under a cursor raised over a shelf lies behind the wall the shelf stands against, so both are in front
    // of it: a critter's egg fades both, a structure egg only the wall and the roof
    mpos egg_hex = {8, 9};
    mpos wall_hex = {8, 10};
    mpos shelf_hex = {8, 11};

    CHECK(IsCutByTransparentEgg(TransparentEggTarget::AnyOccluder, egg_hex, EggAppearenceType::ByY, false, shelf_hex));
    CHECK_FALSE(IsCutByTransparentEgg(TransparentEggTarget::Structure, egg_hex, EggAppearenceType::ByY, false, shelf_hex));
    CHECK(IsCutByTransparentEgg(TransparentEggTarget::Structure, egg_hex, EggAppearenceType::ByY, true, wall_hex));
    CHECK(IsCutByTransparentEgg(TransparentEggTarget::Structure, egg_hex, EggAppearenceType::Always, true, mpos {8, 6}));
    CHECK_FALSE(IsCutByTransparentEgg(TransparentEggTarget::Structure, egg_hex, EggAppearenceType::Always, false, mpos {8, 6}));

    for (TransparentEggTarget target : {TransparentEggTarget::AnyOccluder, TransparentEggTarget::Structure}) {
        CHECK_FALSE(IsCutByTransparentEgg(target, egg_hex, EggAppearenceType::None, true, wall_hex));
    }
}

FO_END_NAMESPACE
