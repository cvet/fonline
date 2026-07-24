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

#include "ModelSpriteLayout.h"

#if FO_ENABLE_3D

FO_BEGIN_NAMESPACE

static constexpr float32_t MODEL_SPRITE_LAYOUT_GUARD = 2.0f;
// Keep in sync with the default 3D_Skinned shadow pass.
static constexpr float32_t SHADOW_CAMERA_ANGLE_COS = 0.9010770213221f;
static constexpr float32_t SHADOW_CAMERA_ANGLE_SIN = 0.4336590845875f;
static constexpr float32_t SHADOW_ANGLE_TAN = 0.2548968037538f;

struct ProjectedLayoutBounds
{
    float32_t MinX {};
    float32_t MinY {};
    float32_t MaxX {};
    float32_t MaxY {};
    bool Initialized {};
};

static auto IsFinite(const mat44& value) -> bool;
static auto CalculateHarmonicRange(float32_t value_0, float32_t value_90, float32_t value_180) -> optional<pair<float32_t, float32_t>>;
static void IncludeProjectedRange(ProjectedLayoutBounds& bounds, const pair<float32_t, float32_t>& x_range, const pair<float32_t, float32_t>& y_range);
static auto IncludeProjectedCorner(const vec3& point, const mat44& post_direction_transform, float32_t projection_factor, bool include_shadow, const vec3& ground_pos, ProjectedLayoutBounds& body_bounds, ProjectedLayoutBounds& draw_bounds) -> bool;
static auto RoundFrameDimension(uint64_t value) -> optional<int32_t>;

auto CalculateModelSpriteFrameSize(float32_t min_x, float32_t min_y, float32_t max_x, float32_t max_y) -> optional<isize32>
{
    FO_STACK_TRACE_ENTRY();

    if (!std::isfinite(min_x) || !std::isfinite(min_y) || !std::isfinite(max_x) || !std::isfinite(max_y) || min_x > max_x || min_y > max_y) {
        return std::nullopt;
    }

    // Exact projected extent: the frame is the tight bounding box of the projected model, with the origin placed
    // at its real position inside it (see CalculateModelSpriteLayout's DrawRect). No legacy quarter/anchor
    // assumption - the baked bounds already carry the precise geometry, so a low/centred-origin creature no
    // longer inflates the frame by reserving three quarters of it above a fixed anchor.
    const float64_t required_width = std::ceil(numeric_cast<float64_t>(max_x) - numeric_cast<float64_t>(min_x));
    const float64_t required_height = std::ceil(numeric_cast<float64_t>(max_y) - numeric_cast<float64_t>(min_y));

    if (required_width > numeric_cast<float64_t>(std::numeric_limits<uint32_t>::max()) || required_height > numeric_cast<float64_t>(std::numeric_limits<uint32_t>::max())) {
        return std::nullopt;
    }

    optional<int32_t> width = RoundFrameDimension(std::max<uint64_t>(4, iround<uint64_t>(required_width)));
    optional<int32_t> height = RoundFrameDimension(std::max<uint64_t>(4, iround<uint64_t>(required_height)));

    if (!width || !height) {
        return std::nullopt;
    }

    return isize32 {*width, *height};
}

auto CalculateModelSpriteLayout(const ModelBounds3D& bounds, const mat44& post_direction_transform, const mat44& pre_direction_transform, float32_t projection_factor, bool include_shadow) -> optional<ModelSpriteLayout>
{
    FO_STACK_TRACE_ENTRY();

    if (!std::isfinite(projection_factor) || projection_factor <= 0.0f || !IsFinite(post_direction_transform) || !IsFinite(pre_direction_transform) || !IsValidModelBounds(bounds)) {
        return std::nullopt;
    }

    glm::vec4 ground = post_direction_transform * glm::vec4 {0.0f, 0.0f, 0.0f, 1.0f};

    if (!std::isfinite(ground.x) || !std::isfinite(ground.y) || !std::isfinite(ground.z) || ground.w != 1.0f) {
        return std::nullopt;
    }

    vec3 ground_pos {ground};
    ProjectedLayoutBounds body_bounds;
    ProjectedLayoutBounds draw_bounds;

    for (uint32_t corner_index = 0; corner_index < 8; corner_index++) {
        vec3 corner {
            (corner_index & 1U) != 0 ? bounds.Max.x : bounds.Min.x,
            (corner_index & 2U) != 0 ? bounds.Max.y : bounds.Min.y,
            (corner_index & 4U) != 0 ? bounds.Max.z : bounds.Min.z,
        };
        glm::vec4 transformed = pre_direction_transform * glm::vec4 {corner, 1.0f};

        if (!std::isfinite(transformed.x) || !std::isfinite(transformed.y) || !std::isfinite(transformed.z) || transformed.w != 1.0f) {
            return std::nullopt;
        }

        if (!IncludeProjectedCorner(vec3 {transformed}, post_direction_transform, projection_factor, include_shadow, ground_pos, body_bounds, draw_bounds)) {
            return std::nullopt;
        }
    }

    if (!body_bounds.Initialized || !draw_bounds.Initialized) {
        return std::nullopt;
    }

    body_bounds.MinX -= MODEL_SPRITE_LAYOUT_GUARD;
    body_bounds.MinY -= MODEL_SPRITE_LAYOUT_GUARD;
    body_bounds.MaxX += MODEL_SPRITE_LAYOUT_GUARD;
    body_bounds.MaxY += MODEL_SPRITE_LAYOUT_GUARD;
    draw_bounds.MinX -= MODEL_SPRITE_LAYOUT_GUARD;
    draw_bounds.MinY -= MODEL_SPRITE_LAYOUT_GUARD;
    draw_bounds.MaxX += MODEL_SPRITE_LAYOUT_GUARD;
    draw_bounds.MaxY += MODEL_SPRITE_LAYOUT_GUARD;

    optional<isize32> draw_size = CalculateModelSpriteFrameSize(draw_bounds.MinX, draw_bounds.MinY, draw_bounds.MaxX, draw_bounds.MaxY);

    if (!draw_size) {
        return std::nullopt;
    }

    float64_t view_left = std::floor(numeric_cast<float64_t>(body_bounds.MinX));
    float64_t view_top = std::floor(numeric_cast<float64_t>(body_bounds.MinY));
    float64_t view_right = std::ceil(numeric_cast<float64_t>(body_bounds.MaxX));
    float64_t view_bottom = std::ceil(numeric_cast<float64_t>(body_bounds.MaxY));
    float64_t draw_left = std::floor(numeric_cast<float64_t>(draw_bounds.MinX));
    float64_t draw_top = std::floor(numeric_cast<float64_t>(draw_bounds.MinY));
    float64_t draw_right = std::ceil(numeric_cast<float64_t>(draw_bounds.MaxX));
    float64_t draw_bottom = std::ceil(numeric_cast<float64_t>(draw_bounds.MaxY));

    if (view_left < numeric_cast<float64_t>(std::numeric_limits<int32_t>::min()) || view_top < numeric_cast<float64_t>(std::numeric_limits<int32_t>::min()) || view_right > numeric_cast<float64_t>(std::numeric_limits<int32_t>::max()) || view_bottom > numeric_cast<float64_t>(std::numeric_limits<int32_t>::max()) || draw_left < numeric_cast<float64_t>(std::numeric_limits<int32_t>::min()) || draw_top < numeric_cast<float64_t>(std::numeric_limits<int32_t>::min()) || draw_right > numeric_cast<float64_t>(std::numeric_limits<int32_t>::max()) || draw_bottom > numeric_cast<float64_t>(std::numeric_limits<int32_t>::max())) {
        return std::nullopt;
    }

    int32_t view_rect_left = iround<int32_t>(view_left);
    int32_t view_rect_top = iround<int32_t>(view_top);
    int32_t view_rect_right = iround<int32_t>(view_right);
    int32_t view_rect_bottom = iround<int32_t>(view_bottom);
    int32_t draw_rect_left = iround<int32_t>(draw_left);
    int32_t draw_rect_top = iround<int32_t>(draw_top);
    int32_t draw_rect_right = iround<int32_t>(draw_right);
    int32_t draw_rect_bottom = iround<int32_t>(draw_bottom);

    if (view_rect_right <= view_rect_left || view_rect_bottom <= view_rect_top || draw_rect_right <= draw_rect_left || draw_rect_bottom <= draw_rect_top) {
        return std::nullopt;
    }

    return ModelSpriteLayout {
        .DrawSize = *draw_size,
        .DrawRect = {draw_rect_left, draw_rect_top, draw_rect_right - draw_rect_left, draw_rect_bottom - draw_rect_top},
        .ViewRect = {view_rect_left, view_rect_top, view_rect_right - view_rect_left, view_rect_bottom - view_rect_top},
    };
}

static auto IsFinite(const mat44& value) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    ptr<const float32_t> values = glm::value_ptr(value);

    for (size_t i = 0; i < 16; i++) {
        if (!std::isfinite(values[i])) {
            return false;
        }
    }

    return true;
}

static auto CalculateHarmonicRange(float32_t value_0, float32_t value_90, float32_t value_180) -> optional<pair<float32_t, float32_t>>
{
    FO_STACK_TRACE_ENTRY();

    float64_t center = (numeric_cast<float64_t>(value_0) + numeric_cast<float64_t>(value_180)) * 0.5;
    float64_t cosine = (numeric_cast<float64_t>(value_0) - numeric_cast<float64_t>(value_180)) * 0.5;
    float64_t sine = numeric_cast<float64_t>(value_90) - center;
    float64_t radius = std::hypot(cosine, sine);
    float64_t range_min = center - radius;
    float64_t range_max = center + radius;

    if (!std::isfinite(range_min) || !std::isfinite(range_max) || range_min < numeric_cast<float64_t>(std::numeric_limits<float32_t>::lowest()) || range_max > numeric_cast<float64_t>(std::numeric_limits<float32_t>::max())) {
        return std::nullopt;
    }

    return pair<float32_t, float32_t> {numeric_cast<float32_t>(range_min), numeric_cast<float32_t>(range_max)};
}

static void IncludeProjectedRange(ProjectedLayoutBounds& bounds, const pair<float32_t, float32_t>& x_range, const pair<float32_t, float32_t>& y_range)
{
    FO_STACK_TRACE_ENTRY();

    if (!bounds.Initialized) {
        bounds.MinX = x_range.first;
        bounds.MinY = y_range.first;
        bounds.MaxX = x_range.second;
        bounds.MaxY = y_range.second;
        bounds.Initialized = true;
    }
    else {
        bounds.MinX = std::min(bounds.MinX, x_range.first);
        bounds.MinY = std::min(bounds.MinY, y_range.first);
        bounds.MaxX = std::max(bounds.MaxX, x_range.second);
        bounds.MaxY = std::max(bounds.MaxY, y_range.second);
    }
}

static auto IncludeProjectedCorner(const vec3& point, const mat44& post_direction_transform, float32_t projection_factor, bool include_shadow, const vec3& ground_pos, ProjectedLayoutBounds& body_bounds, ProjectedLayoutBounds& draw_bounds) -> bool
{
    FO_STACK_TRACE_ENTRY();

    array<vec3, 3> world_points;
    constexpr array<float32_t, 3> angles {0.0f, 90.0f, 180.0f};

    for (size_t i = 0; i < angles.size(); i++) {
        mat44 direction_transform = glm::rotate(mat44 {1.0f}, angles[i] * DEG_TO_RAD_FLOAT, vec3 {0.0f, 1.0f, 0.0f});
        glm::vec4 world = post_direction_transform * direction_transform * glm::vec4 {point, 1.0f};

        if (!std::isfinite(world.x) || !std::isfinite(world.y) || !std::isfinite(world.z) || world.w != 1.0f) {
            return false;
        }

        world_points[i] = vec3 {world};
    }

    auto screen_x = [projection_factor](const vec3& value) { return value.x * projection_factor; };
    auto screen_y = [projection_factor](const vec3& value) { return -value.y * projection_factor; };
    optional<pair<float32_t, float32_t>> body_x_range = CalculateHarmonicRange(screen_x(world_points[0]), screen_x(world_points[1]), screen_x(world_points[2]));
    optional<pair<float32_t, float32_t>> body_y_range = CalculateHarmonicRange(screen_y(world_points[0]), screen_y(world_points[1]), screen_y(world_points[2]));

    if (!body_x_range || !body_y_range) {
        return false;
    }

    IncludeProjectedRange(body_bounds, *body_x_range, *body_y_range);
    IncludeProjectedRange(draw_bounds, *body_x_range, *body_y_range);

    if (!include_shadow) {
        return true;
    }

    array<vec3, 3> shadow_points = world_points;

    for (vec3& shadow_pos : shadow_points) {
        float32_t shadow_distance = (shadow_pos.y - ground_pos.y) * SHADOW_CAMERA_ANGLE_COS;
        shadow_distance -= (ground_pos.z - shadow_pos.z) * SHADOW_CAMERA_ANGLE_SIN;
        shadow_pos.y -= shadow_distance * SHADOW_CAMERA_ANGLE_COS;
        shadow_distance *= SHADOW_ANGLE_TAN;
        shadow_pos.y += shadow_distance * SHADOW_CAMERA_ANGLE_SIN;
        shadow_pos.z -= 10.0f;
    }

    optional<pair<float32_t, float32_t>> shadow_x_range = CalculateHarmonicRange(screen_x(shadow_points[0]), screen_x(shadow_points[1]), screen_x(shadow_points[2]));
    optional<pair<float32_t, float32_t>> shadow_y_range = CalculateHarmonicRange(screen_y(shadow_points[0]), screen_y(shadow_points[1]), screen_y(shadow_points[2]));

    if (!shadow_x_range || !shadow_y_range) {
        return false;
    }

    IncludeProjectedRange(draw_bounds, *shadow_x_range, *shadow_y_range);
    return true;
}

static auto RoundFrameDimension(uint64_t value) -> optional<int32_t>
{
    FO_STACK_TRACE_ENTRY();

    constexpr uint32_t max_logical_frame_dimension = numeric_cast<uint32_t>(std::numeric_limits<int32_t>::max() / MODEL_SPRITE_FRAME_SCALE);

    if (value > numeric_cast<uint64_t>(max_logical_frame_dimension)) {
        return std::nullopt;
    }

    // Round up to the frame scale only - the frame is the tight bounding box of the projected model, not a
    // power-of-two atlas page (the atlas packs the cropped sprite, not this frame), so no extra slack is reserved.
    constexpr uint64_t alignment = MODEL_SPRITE_FRAME_SCALE;
    const uint64_t rounded = (std::max<uint64_t>(value, 1) + alignment - 1) / alignment * alignment;

    if (rounded > numeric_cast<uint64_t>(max_logical_frame_dimension)) {
        return std::nullopt;
    }

    return numeric_cast<int32_t>(rounded);
}

FO_END_NAMESPACE

#endif
