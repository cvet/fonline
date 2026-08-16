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

#include "ModelSpriteLayout.h"

#if FO_ENABLE_3D

FO_BEGIN_NAMESPACE

TEST_CASE("ModelSpriteParticleFrameExpansionMovesPivotWithEnvelope", "[model][particle]")
{
    // Regression: a full-frame particle crop used to keep (16,65) while growing to 118x160, appending all new
    // space to the right and bottom and leaving the critter in the upper-left of the advertised particle frame
    optional<ModelSpriteFramePlacement> placement = CalculateModelSpriteFramePlacement(-40.0f, -71.0f, 74.0f, 85.0f, {16, 65}, 2.0f, {32, 74});

    REQUIRE(placement);
    // 118 tight pixels quantize up to the frame grid; the height is already a multiple of it
    CHECK(placement->Size.width == 128);
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

TEST_CASE("ModelSpriteFramePlacementMergeContainsAlternatingEqualSizePivots", "[model][particle]")
{
    // Regression: a live attack-frame envelope alternated between adjacent pivots while retaining its 58x86 size.
    // Replacing the placement never converged; their root-relative union is only one pixel larger on each affected axis
    optional<ModelSpriteFramePlacement> merged = MergeModelSpriteFramePlacements(ModelSpriteFramePlacement {.Size = {58, 86}, .Pivot = {16, 65}}, ModelSpriteFramePlacement {.Size = {58, 86}, .Pivot = {17, 64}});

    REQUIRE(merged);
    CHECK(merged->Size.width == 80);
    CHECK(merged->Size.height == 112);
    CHECK(merged->Pivot.x == 32);
    CHECK(merged->Pivot.y == 80);

    optional<ModelSpriteFramePlacement> contains_first = MergeModelSpriteFramePlacements(*merged, ModelSpriteFramePlacement {.Size = {58, 86}, .Pivot = {16, 65}});
    optional<ModelSpriteFramePlacement> contains_second = MergeModelSpriteFramePlacements(*merged, ModelSpriteFramePlacement {.Size = {58, 86}, .Pivot = {17, 64}});

    REQUIRE(contains_first);
    REQUIRE(contains_second);
    CHECK(contains_first->Size == merged->Size);
    CHECK(contains_first->Pivot == merged->Pivot);
    CHECK(contains_second->Size == merged->Size);
    CHECK(contains_second->Pivot == merged->Pivot);
}

TEST_CASE("ModelSpriteFramePlacementMergeAllowsRootOutsideTightFrame", "[model][particle]")
{
    // Regression: a preview model's tight envelope was entirely left of its root, so the current X pivot was five
    // pixels beyond the frame. That is a valid placement and must merge with a wider particle/animation envelope
    optional<ModelSpriteFramePlacement> merged = MergeModelSpriteFramePlacements(ModelSpriteFramePlacement {.Size = {130, 266}, .Pivot = {135, 210}}, ModelSpriteFramePlacement {.Size = {270, 274}, .Pivot = {98, 191}});

    REQUIRE(merged);
    CHECK(merged->Size.width == 320);
    CHECK(merged->Size.height == 320);
    CHECK(merged->Pivot.x == 144);
    CHECK(merged->Pivot.y == 224);

    optional<ModelSpriteFramePlacement> contains_current = MergeModelSpriteFramePlacements(*merged, ModelSpriteFramePlacement {.Size = {130, 266}, .Pivot = {135, 210}});
    optional<ModelSpriteFramePlacement> contains_required = MergeModelSpriteFramePlacements(*merged, ModelSpriteFramePlacement {.Size = {270, 274}, .Pivot = {98, 191}});

    REQUIRE(contains_current);
    REQUIRE(contains_required);
    CHECK(contains_current->Size == merged->Size);
    CHECK(contains_current->Pivot == merged->Pivot);
    CHECK(contains_required->Size == merged->Size);
    CHECK(contains_required->Pivot == merged->Pivot);
}

TEST_CASE("ModelSpriteFrameSizesQuantizeSoAnimatedPosesReuseOneFrame", "[model][particle]")
{
    // A breathing pose missed the render-target cache and created a GPU texture per frame, so neighbouring
    // envelopes must share one frame and that frame must still contain the envelope
    optional<isize32> small = CalculateModelSpriteFrameSize(0.0f, 0.0f, 57.0f, 85.0f);
    optional<isize32> grown = CalculateModelSpriteFrameSize(0.0f, 0.0f, 58.0f, 86.0f);

    REQUIRE(small);
    REQUIRE(grown);
    CHECK(small->width == grown->width);
    CHECK(small->height == grown->height);
    CHECK(small->width % MODEL_SPRITE_FRAME_ALIGNMENT == 0);
    CHECK(small->height % MODEL_SPRITE_FRAME_ALIGNMENT == 0);
    CHECK(grown->width >= 58);
    CHECK(grown->height >= 86);

    // Crossing a grid step still grows the frame, so a pose that needs more room is never clipped
    optional<isize32> beyond_step = CalculateModelSpriteFrameSize(0.0f, 0.0f, 65.0f, 86.0f);

    REQUIRE(beyond_step);
    CHECK(beyond_step->width > grown->width);
    CHECK(beyond_step->width >= 65);
}

TEST_CASE("ModelSpriteFramePlacementMergeStaysOnTheFrameGrid", "[model][particle]")
{
    // The atlas draw loop converges on the merge, so its output must be quantized too
    optional<ModelSpriteFramePlacement> merged = MergeModelSpriteFramePlacements(ModelSpriteFramePlacement {.Size = {58, 86}, .Pivot = {16, 65}}, ModelSpriteFramePlacement {.Size = {58, 86}, .Pivot = {17, 64}});

    REQUIRE(merged);
    CHECK(merged->Size.width % MODEL_SPRITE_FRAME_ALIGNMENT == 0);
    CHECK(merged->Size.height % MODEL_SPRITE_FRAME_ALIGNMENT == 0);
    CHECK(merged->Pivot.x % MODEL_SPRITE_FRAME_ALIGNMENT == 0);
    CHECK(merged->Pivot.y % MODEL_SPRITE_FRAME_ALIGNMENT == 0);

    // Idempotent, otherwise the draw loop never settles and allocates a frame every pass
    optional<ModelSpriteFramePlacement> remerged = MergeModelSpriteFramePlacements(*merged, *merged);

    REQUIRE(remerged);
    CHECK(remerged->Size == merged->Size);
    CHECK(remerged->Pivot == merged->Pivot);
}

TEST_CASE("ModelSpriteViewRectStaysInsideTheDrawRectOfWiderBounds", "[model]")
{
    // The view rect anchors the critter name, so it must stay the idle-pose subset of the model bounds; a box grown
    // from live poses and attachments dwarfs the frame and drifts the name up
    ModelBounds3D model_bounds = {.Min = {-0.9f, 0.0f, -0.7f}, .Max = {0.9f, 2.4f, 0.7f}};
    ModelBounds3D view_bounds = {.Min = {-0.3f, 0.0f, -0.25f}, .Max = {0.3f, 1.8f, 0.25f}};
    mat44 identity {1.0f};

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
    // replaces the idle one. The opposite must not happen: a raised weapon or an overhead swing may not lift the name
    ModelBounds3D idle_bounds = {.Min = {-0.3f, 0.0f, -0.25f}, .Max = {0.3f, 1.8f, 0.25f}};
    ModelBounds3D lying_bounds = {.Min = {-0.9f, 0.0f, -0.3f}, .Max = {0.9f, 0.4f, 0.3f}};
    ModelBounds3D overhead_bounds = {.Min = {-0.6f, 0.0f, -0.4f}, .Max = {0.6f, 2.6f, 0.4f}};
    mat44 identity {1.0f};
    constexpr float32_t projection_factor = 32.0f;

    CHECK(SelectModelViewBounds(idle_bounds, lying_bounds, identity, identity, projection_factor).Max.y == lying_bounds.Max.y);
    CHECK(SelectModelViewBounds(idle_bounds, overhead_bounds, identity, identity, projection_factor).Max.y == idle_bounds.Max.y);
    CHECK(SelectModelViewBounds(idle_bounds, std::nullopt, identity, identity, projection_factor).Max.y == idle_bounds.Max.y);

    // The lying pose is wider than the standing one, so the whole box - not just its top - has to come from it
    CHECK(SelectModelViewBounds(idle_bounds, lying_bounds, identity, identity, projection_factor).Min.x == lying_bounds.Min.x);
}

TEST_CASE("ModelSpriteViewBoundsCompareHeightAfterTheModelBaseRotation", "[model]")
{
    // Several production models import with RotX +/-90, making source Z - not source Y - the screen-up axis. The
    // active box deliberately has a taller raw Y than idle, so a raw Max.y comparison would keep idle in both cases
    mat44 identity {1.0f};
    constexpr float32_t projection_factor = 32.0f;

    SECTION("Positive quarter turn")
    {
        ModelBounds3D idle_bounds = {.Min = {-0.3f, -0.25f, -1.8f}, .Max = {0.3f, 0.25f, 0.0f}};
        ModelBounds3D lying_bounds = {.Min = {-0.9f, -0.3f, -0.4f}, .Max = {0.9f, 0.3f, 0.0f}};
        mat44 base_rotation = glm::rotate(mat44 {1.0f}, 90.0f * DEG_TO_RAD_FLOAT, vec3 {1.0f, 0.0f, 0.0f});

        CHECK(SelectModelViewBounds(idle_bounds, lying_bounds, identity, base_rotation, projection_factor).Min.x == lying_bounds.Min.x);
    }

    SECTION("Negative quarter turn")
    {
        ModelBounds3D idle_bounds = {.Min = {-0.3f, -0.25f, 0.0f}, .Max = {0.3f, 0.25f, 1.8f}};
        ModelBounds3D lying_bounds = {.Min = {-0.9f, -0.3f, 0.0f}, .Max = {0.9f, 0.3f, 0.4f}};
        mat44 base_rotation = glm::rotate(mat44 {1.0f}, -90.0f * DEG_TO_RAD_FLOAT, vec3 {1.0f, 0.0f, 0.0f});

        CHECK(SelectModelViewBounds(idle_bounds, lying_bounds, identity, base_rotation, projection_factor).Min.x == lying_bounds.Min.x);
    }
}

FO_END_NAMESPACE

#endif
