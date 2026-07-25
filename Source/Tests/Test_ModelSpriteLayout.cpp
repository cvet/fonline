//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/

#include "catch_amalgamated.hpp"

#include "ModelSpriteLayout.h"

#if FO_ENABLE_3D

FO_BEGIN_NAMESPACE

TEST_CASE("ModelSpriteParticleFrameExpansionMovesPivotWithEnvelope", "[model][particle]")
{
    // Regression: a full-frame particle crop used to keep (16,65) while growing to 118x160, appending all new
    // space to the right and bottom and leaving the critter in the upper-left of the advertised particle frame.
    optional<ModelSpriteFramePlacement> placement = CalculateModelSpriteFramePlacement(-40.0f, -71.0f, 74.0f, 85.0f, {16, 65}, 2.0f, {32, 74});

    REQUIRE(placement);
    CHECK(placement->Size.width == 118);
    CHECK(placement->Size.height == 160);
    CHECK(placement->Pivot.x == 58);
    CHECK(placement->Pivot.y == 138);
}

TEST_CASE("ModelSpriteParticleFramePlacementPreservesLargerExistingSize", "[model][particle]")
{
    optional<ModelSpriteFramePlacement> placement = CalculateModelSpriteFramePlacement(-40.0f, -71.0f, 74.0f, 85.0f, {16, 65}, 2.0f, {140, 180});

    REQUIRE(placement);
    CHECK(placement->Size.width == 140);
    CHECK(placement->Size.height == 180);
    CHECK(placement->Pivot.x == 58);
    CHECK(placement->Pivot.y == 138);
}

TEST_CASE("ModelSpriteParticleFramePlacementClampsExtremeFiniteBounds", "[model][particle]")
{
    optional<ModelSpriteFramePlacement> placement = CalculateModelSpriteFramePlacement(-std::numeric_limits<float32_t>::max(), -std::numeric_limits<float32_t>::max(), -std::numeric_limits<float32_t>::max(), -std::numeric_limits<float32_t>::max(), {16, 65}, 2.0f, {32, 74});

    REQUIRE(placement);
    CHECK(placement->Size.width == 32);
    CHECK(placement->Size.height == 74);
    CHECK(placement->Pivot.x == 32);
    CHECK(placement->Pivot.y == 74);
}

FO_END_NAMESPACE

#endif
