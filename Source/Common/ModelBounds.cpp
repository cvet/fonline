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

#include "ModelBounds.h"

#if FO_ENABLE_3D

FO_BEGIN_NAMESPACE

constexpr float64_t MODEL_BOUNDS_RELATIVE_GUARD = 0.001;
constexpr float64_t MODEL_BOUNDS_ABSOLUTE_GUARD = 0.01;

static auto IsFinite(const vec3& value) -> bool;
static auto IsFinite(const mat44& value) -> bool;

auto IsValidModelBounds(const ModelBounds3D& bounds) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return IsFinite(bounds.Min) && IsFinite(bounds.Max) && bounds.Min.x <= bounds.Max.x && bounds.Min.y <= bounds.Max.y && bounds.Min.z <= bounds.Max.z;
}

auto HasModelBoundsExtent(const ModelBounds3D& bounds) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return IsValidModelBounds(bounds) && (bounds.Min.x < bounds.Max.x || bounds.Min.y < bounds.Max.y || bounds.Min.z < bounds.Max.z);
}

auto IncludeModelBoundsPoint(optional<ModelBounds3D>& target, const vec3& point) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!IsFinite(point) || (target && !IsValidModelBounds(*target))) {
        return false;
    }

    if (!target) {
        target = ModelBounds3D {.Min = point, .Max = point};
        return true;
    }

    ModelBounds3D expanded = *target;
    expanded.Min = glm::min(expanded.Min, point);
    expanded.Max = glm::max(expanded.Max, point);
    target = expanded;
    return true;
}

auto IncludeModelBounds(optional<ModelBounds3D>& target, const ModelBounds3D& bounds) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!IsValidModelBounds(bounds) || (target && !IsValidModelBounds(*target))) {
        return false;
    }

    if (!target) {
        target = bounds;
        return true;
    }

    ModelBounds3D expanded = *target;
    expanded.Min = glm::min(expanded.Min, bounds.Min);
    expanded.Max = glm::max(expanded.Max, bounds.Max);
    target = expanded;
    return true;
}

auto IncludeTransformedModelBounds(optional<ModelBounds3D>& target, const ModelBounds3D& bounds, const mat44& transform) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!IsValidModelBounds(bounds) || !IsFinite(transform)) {
        return false;
    }

    optional<ModelBounds3D> transformed_bounds;

    for (uint32_t corner_index = 0; corner_index < 8; corner_index++) {
        const vec3 corner {
            (corner_index & 1U) != 0 ? bounds.Max.x : bounds.Min.x,
            (corner_index & 2U) != 0 ? bounds.Max.y : bounds.Min.y,
            (corner_index & 4U) != 0 ? bounds.Max.z : bounds.Min.z,
        };
        const glm::vec4 transformed = transform * glm::vec4 {corner, 1.0f};

        if (!IsFinite(vec3 {transformed}) || !is_float_equal(transformed.w, 1.0f) || !IncludeModelBoundsPoint(transformed_bounds, vec3 {transformed})) {
            return false;
        }
    }

    return transformed_bounds && IncludeModelBounds(target, *transformed_bounds);
}

auto CalculateGuardedModelBounds(const ModelBounds3D& bounds) -> optional<ModelBounds3D>
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!IsValidModelBounds(bounds)) {
        return std::nullopt;
    }

    const float64_t max_abs = std::max({
        std::abs(numeric_cast<float64_t>(bounds.Min.x)),
        std::abs(numeric_cast<float64_t>(bounds.Min.y)),
        std::abs(numeric_cast<float64_t>(bounds.Min.z)),
        std::abs(numeric_cast<float64_t>(bounds.Max.x)),
        std::abs(numeric_cast<float64_t>(bounds.Max.y)),
        std::abs(numeric_cast<float64_t>(bounds.Max.z)),
    });
    const float32_t guard = numeric_cast<float32_t>(std::max(MODEL_BOUNDS_ABSOLUTE_GUARD, max_abs * MODEL_BOUNDS_RELATIVE_GUARD));
    ModelBounds3D guarded = bounds;
    guarded.Min -= vec3 {guard};
    guarded.Max += vec3 {guard};

    if (!IsValidModelBounds(guarded)) {
        return std::nullopt;
    }

    return guarded;
}

static auto IsFinite(const vec3& value) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

static auto IsFinite(const mat44& value) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    const ptr<const float32_t> values = glm::value_ptr(value);

    for (size_t i = 0; i < 16; i++) {
        if (!std::isfinite(values[i])) {
            return false;
        }
    }

    return true;
}

FO_END_NAMESPACE

#endif
