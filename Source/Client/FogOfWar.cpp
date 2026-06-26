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
//

#include "FogOfWar.h"

#include "Geometry.h"

FO_BEGIN_NAMESPACE

void FogShape::Clear()
{
    FO_STACK_TRACE_ENTRY();

    _rebuildFog = false;
    _lastEnabled = true;
    _transitionActive = false;
    _collapsingToOff = false;
    _transitionTime = {};
    _transitionDuration = 0;
    _lastOrigin = {};
    _lastDistance = {};
    _lastRadius = {};
    _lastOverlayColor = {};
    _lastCenterColor = {};
    _lastTraceMode = TraceModeType::None;
    _lastCheckShootBlocks = true;
    _points.clear();
    _startPoints.clear();
    _targetPoints.clear();
}

void FogShape::Prepare(const Input& input)
{
    FO_STACK_TRACE_ENTRY();

    if (input.Enabled != _lastEnabled || input.FogOrigin.Valid != _lastOrigin.Valid || //
        (input.FogOrigin.Valid && (input.FogOrigin.BaseHex != _lastOrigin.BaseHex || input.FogOrigin.LookDistance != _lastOrigin.LookDistance)) || //
        input.Distance != _lastDistance || input.Radius != _lastRadius || input.OverlayColor != _lastOverlayColor || //
        input.CenterColor != _lastCenterColor || input.TraceMode != _lastTraceMode || input.CheckShootBlocks != _lastCheckShootBlocks) {
        _rebuildFog = true;
    }

    _lastEnabled = input.Enabled;
    _lastOrigin = input.FogOrigin;
    _lastDistance = input.Distance;
    _lastRadius = input.Radius;
    _lastOverlayColor = input.OverlayColor;
    _lastCenterColor = input.CenterColor;
    _lastTraceMode = input.TraceMode;
    _lastCheckShootBlocks = input.CheckShootBlocks;

    if (_rebuildFog) {
        _rebuildFog = false;

        vector<PrimitivePoint> points;
        BuildPoints(input, points);
        StartTransition(std::move(points), input.FrameTime, input.FogTransitionDuration);
    }

    UpdateTransition(input.FrameTime);
}

void FogShape::BuildPoints(const Input& input, vector<PrimitivePoint>& fog_points) const
{
    FO_STACK_TRACE_ENTRY();

    fog_points.clear();

    if (!input.Enabled || !input.FogOrigin.Valid) {
        return;
    }

    const auto is_traced_overlay = input.TraceMode == TraceModeType::Overlay;
    const auto dist = std::max(input.Radius > 0 ? input.Radius : input.FogOrigin.LookDistance + input.FogExtraLength, 0);
    if (dist <= 0) {
        return;
    }

    if (is_traced_overlay) {
        FO_VERIFY_AND_THROW(input.TraceBulletToBlock, "Trace bullet to block callback is required in overlay trace mode");
    }

    const auto base_hex = input.FogOrigin.BaseHex;
    const auto half_hw = input.MapHexWidth / 2;
    const auto half_hh = input.MapHexHeight / 2;
    const auto has_custom_overlay_color = input.OverlayColor.comp.a != 0;
    const auto has_custom_center_color = input.CenterColor.comp.a != 0;
    const auto default_overlay_color = ucolor {96, 224, 96, 255};
    const auto overlay_color = has_custom_overlay_color ? input.OverlayColor : default_overlay_color;

    const ipos32 base_pos = GeometryHelper::GetHexOffset(ipos32(input.FogOrigin.BaseHex), ipos32(base_hex));
    const auto center_alpha = is_traced_overlay ? uint8_t {255} : uint8_t {0};
    const auto center_color = is_traced_overlay ? ucolor {overlay_color.comp.r, overlay_color.comp.g, overlay_color.comp.b, center_alpha} : (has_custom_center_color ? input.CenterColor : ucolor {0, 0, 0, center_alpha});
    const auto center_point = PrimitivePoint {.PointPos = {base_pos.x + half_hw, base_pos.y + half_hh}, .PointColor = center_color, .PointOffset = _drawOffset.as_ptr()};

    auto target_raw_hex = ipos32 {base_hex.x, base_hex.y};
    size_t points_added = 0;

    bool seek_start = true;

    for (auto i = 0; i < (GameSettings::HEXAGONAL_GEOMETRY ? 6 : 4); i++) {
        mdir dir;
        int32_t iterations;

        if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
            dir = hdir((i + 2) % 6);
            iterations = dist;
        }
        else {
            dir = hdir(((i + 1) * 2) % 8);
            iterations = dist * 2;
        }

        for (int32_t j = 0; j < iterations; j++) {
            if (seek_start) {
                for (int32_t l = 0; l < dist; l++) {
#if FO_GEOMETRY == 1
                    GeometryHelper::MoveHexByDirUnsafe(target_raw_hex, hdir::NorthEast);
#elif FO_GEOMETRY == 2
                    GeometryHelper::MoveHexByDirUnsafe(target_raw_hex, hdir::North);
#endif
                }
                seek_start = false;
                j = -1;
            }
            else {
                GeometryHelper::MoveHexByDirUnsafe(target_raw_hex, dir);
            }

            const auto target_hex = input.MapSize.clamp_pos(target_raw_hex);
            const auto target_dist = GeometryHelper::GetDistance(base_hex, target_hex);
            auto edge_offset = target_dist < dist ? _baseDrawOffset.as_ptr() : _drawOffset.as_ptr();

            if (is_traced_overlay) {
                const auto max_overlay_dist = std::max(std::min(target_dist, input.Distance), 0) + 1;
                const auto block_hex = input.TraceBulletToBlock(base_hex, target_hex, max_overlay_dist, input.CheckShootBlocks);
                const auto block_hex_pos = GeometryHelper::GetHexOffset(ipos32(base_hex), ipos32(block_hex));
                const auto result_overlay_dist = GeometryHelper::GetDistance(base_hex, block_hex);
                const auto overlay_strength = numeric_cast<uint8_t>(std::max(max_overlay_dist - result_overlay_dist, 0) * 255 / max_overlay_dist);
                const auto color_r = numeric_cast<uint8_t>(overlay_color.comp.r * overlay_strength / 255);
                const auto color_g = numeric_cast<uint8_t>(overlay_color.comp.g * overlay_strength / 255);
                const auto color_b = numeric_cast<uint8_t>(overlay_color.comp.b * overlay_strength / 255);
                const auto color = ucolor {color_r, color_g, color_b, 255};
                auto overlay_offset = result_overlay_dist < max_overlay_dist ? _baseDrawOffset.as_ptr() : _drawOffset.as_ptr();

                fog_points.emplace_back(PrimitivePoint {.PointPos = {block_hex_pos.x + half_hw, block_hex_pos.y + half_hh}, .PointColor = color, .PointOffset = overlay_offset});
            }
            else {
                const auto hex_pos = GeometryHelper::GetHexOffset(ipos32(base_hex), ipos32(target_hex));
                const auto color = ucolor {255, numeric_cast<uint8_t>(target_dist * 255 / dist), 0, 0};

                fog_points.emplace_back(PrimitivePoint {.PointPos = {hex_pos.x + half_hw, hex_pos.y + half_hh}, .PointColor = color, .PointOffset = edge_offset});
            }

            if (++points_added % 2 == 0) {
                fog_points.emplace_back(center_point);
            }
        }
    }
}

void FogShape::StartTransition(vector<PrimitivePoint>&& points, nanotime frame_time, int32_t duration)
{
    FO_STACK_TRACE_ENTRY();

    // One morph duration for every change: the oval grows from its center on appearance, glides between
    // shapes as the player moves, and shrinks back to center on disappearance. No separate slow reveal.
    _transitionDuration = std::max(duration, 0);

    if (points.empty()) {
        // Disappear: shrink the current fan to its center, then clear when the morph finishes. Nothing to
        // do if it is already gone or already collapsing.
        if (_collapsingToOff || _points.empty()) {
            return;
        }

        _collapsingToOff = true;

        if (_transitionDuration > 0) {
            _startPoints = _points;
            _targetPoints = MakeCollapsed(_points);
            _transitionTime = frame_time;
            _transitionActive = true;
        }
        else {
            _points.clear();
            _startPoints.clear();
            _targetPoints.clear();
            _transitionActive = false;
        }

        return;
    }

    _collapsingToOff = false;

    if (_transitionDuration == 0) {
        _points = std::move(points);
        _startPoints.clear();
        _targetPoints = _points;
        _transitionActive = false;
        return;
    }

    if (_points.empty()) {
        _points = MakeCollapsed(points);
    }

    _startPoints = _points;
    _targetPoints = std::move(points);
    _transitionTime = frame_time;
    _transitionActive = true;
}

void FogShape::UpdateTransition(nanotime frame_time)
{
    FO_STACK_TRACE_ENTRY();

    if (!_transitionActive) {
        return;
    }

    const auto clamped_duration = std::max(_transitionDuration, 0);

    if (clamped_duration == 0) {
        FinishTransition();
        return;
    }

    const auto progress = std::clamp((frame_time - _transitionTime).div<float32_t>(std::chrono::milliseconds {clamped_duration}), 0.0f, 1.0f);

    InterpolatePoints(_startPoints, _targetPoints, progress, _points);

    if (progress >= 1.0f) {
        FinishTransition();
    }
}

void FogShape::FinishTransition()
{
    FO_NO_STACK_TRACE_ENTRY();

    _points = _targetPoints;
    _transitionActive = false;

    // A collapse-to-off morph ends with the fog fully cleared.
    if (_collapsingToOff) {
        _points.clear();
        _startPoints.clear();
        _targetPoints.clear();
        _collapsingToOff = false;
    }
}

auto FogShape::GetCollapsePoint(const vector<PrimitivePoint>& points) -> PrimitivePoint
{
    FO_NO_STACK_TRACE_ENTRY();

    if (points.empty()) {
        return {};
    }
    if (points.size() >= 3) {
        return points[2];
    }

    return points.front();
}

auto FogShape::MakeCollapsed(const vector<PrimitivePoint>& points) const -> vector<PrimitivePoint>
{
    FO_NO_STACK_TRACE_ENTRY();

    if (points.empty()) {
        return {};
    }

    const auto center = GetCollapsePoint(points);

    auto collapsed = points;

    // Collapse only the position to the origin; keep each point's own color so a growing/shrinking
    // fan keeps its soft faded edges throughout the transition instead of flashing the center color.
    for (auto& p : collapsed) {
        p.PointPos = center.PointPos;
        p.PointOffset = _drawOffset.as_ptr();
    }

    return collapsed;
}

auto FogShape::SampleEdgePoint(const vector<PrimitivePoint>& points, size_t edge_count, size_t sample_edge_idx, size_t sample_edge_count, const PrimitivePoint& fallback) -> PrimitivePoint
{
    FO_NO_STACK_TRACE_ENTRY();

    if (edge_count == 0) {
        return fallback;
    }
    if (sample_edge_count <= 1) {
        return points[0];
    }

    const auto src_edge = std::min((sample_edge_idx * (edge_count - 1) + (sample_edge_count - 1) / 2) / (sample_edge_count - 1), edge_count - 1);
    // Edge index to array index: skip every third slot (center)
    // edge 0 → arr 0, edge 1 → arr 1, edge 2 → arr 3, edge 3 → arr 4, ...
    const auto arr_idx = 3 * (src_edge / 2) + (src_edge % 2);
    return arr_idx < points.size() ? points[arr_idx] : fallback;
}

auto FogShape::LerpFogColor(ucolor from, ucolor to, float32_t t) -> ucolor
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto lerp_channel = [t](uint8_t from_value, uint8_t to_value) -> uint8_t { return numeric_cast<uint8_t>(std::clamp(iround<int32_t>(lerp(numeric_cast<float32_t>(from_value), numeric_cast<float32_t>(to_value), t)), 0, 255)); };

    return ucolor {lerp_channel(from.comp.r, to.comp.r), lerp_channel(from.comp.g, to.comp.g), lerp_channel(from.comp.b, to.comp.b), lerp_channel(from.comp.a, to.comp.a)};
}

auto FogShape::LerpFogPoint(const PrimitivePoint& from, const PrimitivePoint& to, float32_t t) -> PrimitivePoint
{
    FO_NO_STACK_TRACE_ENTRY();

    PrimitivePoint result;
    result.PointPos.x = iround<int32_t>(lerp(numeric_cast<float32_t>(from.PointPos.x), numeric_cast<float32_t>(to.PointPos.x), t));
    result.PointPos.y = iround<int32_t>(lerp(numeric_cast<float32_t>(from.PointPos.y), numeric_cast<float32_t>(to.PointPos.y), t));
    result.PointColor = LerpFogColor(from.PointColor, to.PointColor, t);
    result.PointOffset = to.PointOffset ? to.PointOffset : from.PointOffset;
    result.PPointColor = to.PPointColor ? to.PPointColor : from.PPointColor;
    return result;
}

void FogShape::InterpolatePoints(const vector<PrimitivePoint>& from_points, const vector<PrimitivePoint>& to_points, float32_t t, vector<PrimitivePoint>& result_points)
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto result_count = std::max(from_points.size(), to_points.size());

    if (result_count == 0) {
        result_points.clear();
        return;
    }

    const auto from_center = GetCollapsePoint(from_points);
    const auto to_center = GetCollapsePoint(to_points);
    const auto fallback_center = !to_points.empty() ? to_center : from_center;

    const auto from_edge_count = from_points.size() - from_points.size() / 3;
    const auto to_edge_count = to_points.size() - to_points.size() / 3;
    const auto result_edge_count = result_count - result_count / 3;

    result_points.resize(result_count);

    size_t edge_idx = 0;

    for (size_t i = 0; i < result_count; i++) {
        if (i % 3 == 2) {
            result_points[i] = LerpFogPoint(from_center, to_center, t);
        }
        else {
            const auto from_edge = SampleEdgePoint(from_points, from_edge_count, edge_idx, result_edge_count, fallback_center);
            const auto to_edge = SampleEdgePoint(to_points, to_edge_count, edge_idx, result_edge_count, fallback_center);
            result_points[i] = LerpFogPoint(from_edge, to_edge, t);
            edge_idx++;
        }
    }
}

FO_END_NAMESPACE
