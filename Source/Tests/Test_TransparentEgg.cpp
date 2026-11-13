// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

#include "catch_amalgamated.hpp"

#include "SpriteManager.h"

FO_BEGIN_NAMESPACE

// East-west wall runs share y and answer ByY; north-south runs share x and answer ByX
TEST_CASE("TransparentEggCutsEveryPieceOfARunStandingOnItsOwnLine")
{
    constexpr int16_t line {10};

    // Both column parities must cut the entire wall run when the egg sits on its base line
    constexpr int16_t egg_columns[] {8, 9};

    for (int16_t egg_along : egg_columns) {
        for (int16_t along {4}; along <= 13; along++) {
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
    constexpr int16_t line {10};

    for (int16_t along {4}; along <= 13; along++) {
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
    mpos egg_hex {8, 9};
    mpos wall_hex {8, 10};
    mpos shelf_hex {8, 11};

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
