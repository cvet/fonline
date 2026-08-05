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

TEST_CASE("ModelSpriteViewRectStaysInsideTheDrawRectOfWiderBounds", "[model]")
{
    // The view rect anchors the critter name, so it must never advertise more space than the model actually draws in.
    // Its bounds are the idle-pose subset of the model bounds, and the layout must keep that containment: the regression
    // this pins is a view box grown from live poses and attachments until it dwarfed the frame and the name drifted up.
    const ModelBounds3D model_bounds = {.Min = {-0.9f, 0.0f, -0.7f}, .Max = {0.9f, 2.4f, 0.7f}};
    const ModelBounds3D view_bounds = {.Min = {-0.3f, 0.0f, -0.25f}, .Max = {0.3f, 1.8f, 0.25f}};
    const mat44 identity {1.0f};

    optional<ModelSpriteLayout> model_layout = CalculateModelSpriteLayout(model_bounds, identity, identity, 32.0f, false);
    optional<ModelSpriteLayout> view_layout = CalculateModelSpriteLayout(view_bounds, identity, identity, 32.0f, false);

    REQUIRE(model_layout);
    REQUIRE(view_layout);
    CHECK(view_layout->ViewRect.width <= model_layout->DrawRect.width);
    CHECK(view_layout->ViewRect.height <= model_layout->DrawRect.height);
    CHECK(view_layout->ViewRect.x >= model_layout->DrawRect.x);
    CHECK(view_layout->ViewRect.y >= model_layout->DrawRect.y);
    CHECK(view_layout->ViewRect.x + view_layout->ViewRect.width <= model_layout->DrawRect.x + model_layout->DrawRect.width);
    CHECK(view_layout->ViewRect.y + view_layout->ViewRect.height <= model_layout->DrawRect.y + model_layout->DrawRect.height);
}

TEST_CASE("ModelSpriteViewBoundsFollowALowerPoseButNeverAHigherOne", "[model]")
{
    // A corpse or a prone body must wear its name at its own height, not at standing height, so a lower animation box
    // replaces the idle one. The opposite must not happen: a raised weapon or an overhead swing may not lift the name.
    const ModelBounds3D idle_bounds = {.Min = {-0.3f, 0.0f, -0.25f}, .Max = {0.3f, 1.8f, 0.25f}};
    const ModelBounds3D lying_bounds = {.Min = {-0.9f, 0.0f, -0.3f}, .Max = {0.9f, 0.4f, 0.3f}};
    const ModelBounds3D overhead_bounds = {.Min = {-0.6f, 0.0f, -0.4f}, .Max = {0.6f, 2.6f, 0.4f}};
    const mat44 identity {1.0f};
    constexpr float32_t projection_factor = 32.0f;

    CHECK(SelectModelViewBounds(idle_bounds, lying_bounds, identity, identity, projection_factor).Max.y == lying_bounds.Max.y);
    CHECK(SelectModelViewBounds(idle_bounds, overhead_bounds, identity, identity, projection_factor).Max.y == idle_bounds.Max.y);
    CHECK(SelectModelViewBounds(idle_bounds, std::nullopt, identity, identity, projection_factor).Max.y == idle_bounds.Max.y);

    // The lying pose is wider than the standing one, so the whole box - not just its top - has to come from it.
    CHECK(SelectModelViewBounds(idle_bounds, lying_bounds, identity, identity, projection_factor).Min.x == lying_bounds.Min.x);
}

TEST_CASE("ModelSpriteViewBoundsCompareHeightAfterTheModelBaseRotation", "[model]")
{
    // Several production models import with RotX +/-90, making source Z - not source Y - the screen-up axis. The
    // active box deliberately has a taller raw Y than idle, so a raw Max.y comparison would keep idle in both cases.
    const mat44 identity {1.0f};
    constexpr float32_t projection_factor = 32.0f;

    SECTION("Positive quarter turn")
    {
        const ModelBounds3D idle_bounds = {.Min = {-0.3f, -0.25f, -1.8f}, .Max = {0.3f, 0.25f, 0.0f}};
        const ModelBounds3D lying_bounds = {.Min = {-0.9f, -0.3f, -0.4f}, .Max = {0.9f, 0.3f, 0.0f}};
        const mat44 base_rotation = glm::rotate(mat44 {1.0f}, 90.0f * DEG_TO_RAD_FLOAT, vec3 {1.0f, 0.0f, 0.0f});

        CHECK(SelectModelViewBounds(idle_bounds, lying_bounds, identity, base_rotation, projection_factor).Min.x == lying_bounds.Min.x);
    }

    SECTION("Negative quarter turn")
    {
        const ModelBounds3D idle_bounds = {.Min = {-0.3f, -0.25f, 0.0f}, .Max = {0.3f, 0.25f, 1.8f}};
        const ModelBounds3D lying_bounds = {.Min = {-0.9f, -0.3f, 0.0f}, .Max = {0.9f, 0.3f, 0.4f}};
        const mat44 base_rotation = glm::rotate(mat44 {1.0f}, -90.0f * DEG_TO_RAD_FLOAT, vec3 {1.0f, 0.0f, 0.0f});

        CHECK(SelectModelViewBounds(idle_bounds, lying_bounds, identity, base_rotation, projection_factor).Min.x == lying_bounds.Min.x);
    }
}

FO_END_NAMESPACE

#endif
