// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#if FO_ENABLE_3D

FO_BEGIN_NAMESPACE

constexpr uint32_t MODEL_BOUNDS_VERSION {2};

struct ModelBounds3D
{
    vec3 Min {};
    vec3 Max {};
};

auto IsValidModelBounds(const ModelBounds3D& bounds) -> bool;
auto HasModelBoundsExtent(const ModelBounds3D& bounds) -> bool;
auto IncludeModelBoundsPoint(optional<ModelBounds3D>& target, const vec3& point) -> bool;
auto IncludeModelBounds(optional<ModelBounds3D>& target, const ModelBounds3D& bounds) -> bool;
auto IncludeTransformedModelBounds(optional<ModelBounds3D>& target, const ModelBounds3D& bounds, const mat44& transform) -> bool;
auto CalculateGuardedModelBounds(const ModelBounds3D& bounds) -> optional<ModelBounds3D>;

FO_END_NAMESPACE

#endif
