//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
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

#include "Common.h"

#include "ModelBounds.h"

FO_BEGIN_NAMESPACE

// The whole suite exercises the 3D bounds API, which `ModelBounds.h` declares only for 3D-enabled builds.
// The source inventory stays unconditional, so the guard lives here.
#if FO_ENABLE_3D

static constexpr float32_t NOT_A_NUMBER = std::numeric_limits<float32_t>::quiet_NaN();
static constexpr float32_t POSITIVE_INFINITY = std::numeric_limits<float32_t>::infinity();

TEST_CASE("ModelBoundsValidity")
{
    SECTION("OrderedFiniteBoundsAreValid")
    {
        CHECK(IsValidModelBounds(ModelBounds3D {.Min = {-1.0f, -2.0f, -3.0f}, .Max = {1.0f, 2.0f, 3.0f}}));

        // A degenerate box is still valid, it simply has no extent
        ModelBounds3D point_bounds {.Min = {1.0f, 2.0f, 3.0f}, .Max = {1.0f, 2.0f, 3.0f}};
        CHECK(IsValidModelBounds(point_bounds));
        CHECK_FALSE(HasModelBoundsExtent(point_bounds));
    }

    SECTION("InvertedBoundsAreRejectedOnEveryAxis")
    {
        CHECK_FALSE(IsValidModelBounds(ModelBounds3D {.Min = {2.0f, 0.0f, 0.0f}, .Max = {1.0f, 0.0f, 0.0f}}));
        CHECK_FALSE(IsValidModelBounds(ModelBounds3D {.Min = {0.0f, 2.0f, 0.0f}, .Max = {0.0f, 1.0f, 0.0f}}));
        CHECK_FALSE(IsValidModelBounds(ModelBounds3D {.Min = {0.0f, 0.0f, 2.0f}, .Max = {0.0f, 0.0f, 1.0f}}));
    }

    SECTION("NonFiniteComponentsAreRejected")
    {
        CHECK_FALSE(IsValidModelBounds(ModelBounds3D {.Min = {NOT_A_NUMBER, 0.0f, 0.0f}, .Max = {1.0f, 1.0f, 1.0f}}));
        CHECK_FALSE(IsValidModelBounds(ModelBounds3D {.Min = {0.0f, 0.0f, 0.0f}, .Max = {POSITIVE_INFINITY, 1.0f, 1.0f}}));
        CHECK_FALSE(HasModelBoundsExtent(ModelBounds3D {.Min = {NOT_A_NUMBER, 0.0f, 0.0f}, .Max = {1.0f, 1.0f, 1.0f}}));
    }

    SECTION("ExtentIsReportedPerAxis")
    {
        CHECK(HasModelBoundsExtent(ModelBounds3D {.Min = {0.0f, 0.0f, 0.0f}, .Max = {1.0f, 0.0f, 0.0f}}));
        CHECK(HasModelBoundsExtent(ModelBounds3D {.Min = {0.0f, 0.0f, 0.0f}, .Max = {0.0f, 1.0f, 0.0f}}));
        CHECK(HasModelBoundsExtent(ModelBounds3D {.Min = {0.0f, 0.0f, 0.0f}, .Max = {0.0f, 0.0f, 1.0f}}));
    }
}

TEST_CASE("ModelBoundsAccumulation")
{
    SECTION("PointsSeedAndExpandTheTarget")
    {
        optional<ModelBounds3D> target;

        REQUIRE(IncludeModelBoundsPoint(target, vec3 {1.0f, 2.0f, 3.0f}));
        REQUIRE(target.has_value());
        CHECK(target->Min == vec3 {1.0f, 2.0f, 3.0f});
        CHECK(target->Max == vec3 {1.0f, 2.0f, 3.0f});

        REQUIRE(IncludeModelBoundsPoint(target, vec3 {-1.0f, 5.0f, 0.0f}));
        CHECK(target->Min == vec3 {-1.0f, 2.0f, 0.0f});
        CHECK(target->Max == vec3 {1.0f, 5.0f, 3.0f});

        // A point already inside the box leaves it untouched
        ModelBounds3D before = *target;
        REQUIRE(IncludeModelBoundsPoint(target, vec3 {0.0f, 3.0f, 1.0f}));
        CHECK(target->Min == before.Min);
        CHECK(target->Max == before.Max);
    }

    SECTION("NonFinitePointsAndCorruptTargetsAreRejected")
    {
        optional<ModelBounds3D> target;
        CHECK_FALSE(IncludeModelBoundsPoint(target, vec3 {NOT_A_NUMBER, 0.0f, 0.0f}));
        CHECK_FALSE(target.has_value());

        optional<ModelBounds3D> corrupt = ModelBounds3D {.Min = {2.0f, 0.0f, 0.0f}, .Max = {1.0f, 0.0f, 0.0f}};
        CHECK_FALSE(IncludeModelBoundsPoint(corrupt, vec3 {0.0f, 0.0f, 0.0f}));
        CHECK_FALSE(IncludeModelBounds(corrupt, ModelBounds3D {.Min = {0.0f, 0.0f, 0.0f}, .Max = {1.0f, 1.0f, 1.0f}}));
    }

    SECTION("BoxesSeedAndExpandTheTarget")
    {
        optional<ModelBounds3D> target;
        ModelBounds3D first {.Min = {0.0f, 0.0f, 0.0f}, .Max = {1.0f, 1.0f, 1.0f}};

        REQUIRE(IncludeModelBounds(target, first));
        REQUIRE(target.has_value());
        CHECK(target->Min == first.Min);
        CHECK(target->Max == first.Max);

        REQUIRE(IncludeModelBounds(target, ModelBounds3D {.Min = {-2.0f, 0.5f, 0.5f}, .Max = {0.5f, 3.0f, 0.5f}}));
        CHECK(target->Min == vec3 {-2.0f, 0.0f, 0.0f});
        CHECK(target->Max == vec3 {1.0f, 3.0f, 1.0f});

        // An invalid source is refused without disturbing what was already accumulated
        ModelBounds3D accumulated = *target;
        CHECK_FALSE(IncludeModelBounds(target, ModelBounds3D {.Min = {1.0f, 0.0f, 0.0f}, .Max = {0.0f, 0.0f, 0.0f}}));
        CHECK(target->Min == accumulated.Min);
        CHECK(target->Max == accumulated.Max);
    }
}

TEST_CASE("ModelBoundsTransform")
{
    ModelBounds3D unit {.Min = {0.0f, 0.0f, 0.0f}, .Max = {1.0f, 1.0f, 1.0f}};

    SECTION("IdentityKeepsTheBox")
    {
        optional<ModelBounds3D> target;

        REQUIRE(IncludeTransformedModelBounds(target, unit, mat44 {1.0f}));
        REQUIRE(target.has_value());
        CHECK(target->Min == unit.Min);
        CHECK(target->Max == unit.Max);
    }

    SECTION("TranslationMovesEveryCorner")
    {
        optional<ModelBounds3D> target;
        mat44 transform = glm::translate(mat44 {1.0f}, vec3 {10.0f, -5.0f, 2.0f});

        REQUIRE(IncludeTransformedModelBounds(target, unit, transform));
        REQUIRE(target.has_value());
        CHECK(target->Min == vec3 {10.0f, -5.0f, 2.0f});
        CHECK(target->Max == vec3 {11.0f, -4.0f, 3.0f});
    }

    SECTION("RotationGrowsTheAxisAlignedBox")
    {
        optional<ModelBounds3D> target;
        mat44 transform = glm::rotate(mat44 {1.0f}, glm::radians(45.0f), vec3 {0.0f, 0.0f, 1.0f});

        REQUIRE(IncludeTransformedModelBounds(target, unit, transform));
        REQUIRE(target.has_value());
        // The rotated unit square no longer fits inside its original footprint
        CHECK(target->Max.x - target->Min.x > 1.0f);
    }

    SECTION("InvalidInputsAreRejected")
    {
        optional<ModelBounds3D> target;

        CHECK_FALSE(IncludeTransformedModelBounds(target, ModelBounds3D {.Min = {1.0f, 0.0f, 0.0f}, .Max = {0.0f, 0.0f, 0.0f}}, mat44 {1.0f}));

        mat44 broken {1.0f};
        broken[0][0] = NOT_A_NUMBER;
        CHECK_FALSE(IncludeTransformedModelBounds(target, unit, broken));

        // A projective transform breaks the w == 1 assumption the corner walk relies on
        mat44 projective {1.0f};
        projective[3][3] = 2.0f;
        CHECK_FALSE(IncludeTransformedModelBounds(target, unit, projective));

        CHECK_FALSE(target.has_value());
    }
}

TEST_CASE("ModelBoundsGuard")
{
    SECTION("GuardExpandsInEveryDirection")
    {
        ModelBounds3D bounds {.Min = {-1.0f, -1.0f, -1.0f}, .Max = {1.0f, 1.0f, 1.0f}};
        auto guarded = CalculateGuardedModelBounds(bounds);

        REQUIRE(guarded.has_value());
        CHECK(guarded->Min.x < bounds.Min.x);
        CHECK(guarded->Min.y < bounds.Min.y);
        CHECK(guarded->Min.z < bounds.Min.z);
        CHECK(guarded->Max.x > bounds.Max.x);
        CHECK(guarded->Max.y > bounds.Max.y);
        CHECK(guarded->Max.z > bounds.Max.z);
    }

    SECTION("LargeBoundsUseTheRelativeGuard")
    {
        auto small = CalculateGuardedModelBounds(ModelBounds3D {.Min = {0.0f, 0.0f, 0.0f}, .Max = {0.0f, 0.0f, 0.0f}});
        auto large = CalculateGuardedModelBounds(ModelBounds3D {.Min = {-1.0e6f, 0.0f, 0.0f}, .Max = {1.0e6f, 0.0f, 0.0f}});

        REQUIRE(small.has_value());
        REQUIRE(large.has_value());

        // The absolute guard floors tiny boxes while the relative guard scales with big ones
        float32_t small_guard = small->Max.y - 0.0f;
        float32_t large_guard = large->Max.y - 0.0f;
        CHECK(large_guard > small_guard);
    }

    SECTION("InvalidBoundsYieldNothing")
    {
        CHECK_FALSE(CalculateGuardedModelBounds(ModelBounds3D {.Min = {1.0f, 0.0f, 0.0f}, .Max = {0.0f, 0.0f, 0.0f}}).has_value());
        CHECK_FALSE(CalculateGuardedModelBounds(ModelBounds3D {.Min = {NOT_A_NUMBER, 0.0f, 0.0f}, .Max = {1.0f, 1.0f, 1.0f}}).has_value());
    }
}

#endif

FO_END_NAMESPACE
