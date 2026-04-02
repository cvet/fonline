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

void FogOfWar::Clear()
{
    FO_STACK_TRACE_ENTRY();

    _rebuildFog = false;
    _lastEnabled = true;
    _transitionActive = false;
    _collapsingToOff = false;
    _transitionTime = {};
    _lastOrigin = {};
    _lastDistance = {};
    _points.clear();
    _startPoints.clear();
    _targetPoints.clear();
}

void FogOfWar::Prepare(const Input& input)
{
    FO_STACK_TRACE_ENTRY();

    if (input.Enabled != _lastEnabled || input.FogOrigin.Valid != _lastOrigin.Valid || (input.FogOrigin.Valid && (input.FogOrigin.BaseHex != _lastOrigin.BaseHex || input.FogOrigin.LookDistance != _lastOrigin.LookDistance)) || input.Distance != _lastDistance) {
        _rebuildFog = true;
    }

    _lastEnabled = input.Enabled;
    _lastOrigin = input.FogOrigin;
    _lastDistance = input.Distance;

    if (_rebuildFog) {
        _rebuildFog = false;

        vector<PrimitivePoint> points;
        BuildPoints(input, points);
        StartTransition(std::move(points), input.FrameTime, input.FogTransitionDuration);
    }

    UpdateTransition(input.FrameTime, input.FogTransitionDuration);
}

void FogOfWar::BuildPoints(const Input& input, vector<PrimitivePoint>& fog_points) const
{
    FO_STACK_TRACE_ENTRY();

    fog_points.clear();

    if (!input.Enabled || !input.FogOrigin.Valid) {
        return;
    }

    FO_RUNTIME_ASSERT(input.TraceBulletToBlock);

    const auto is_shoot = _kind == Kind::Shoot;
    const auto dist = input.FogOrigin.LookDistance + input.FogExtraLength;
    const auto base_hex = input.FogOrigin.BaseHex;
    const auto chosen_dir = input.FogOrigin.Dir;
    const auto half_hw = input.MapHexWidth / 2;
    const auto half_hh = input.MapHexHeight / 2;

    const ipos32 base_pos = GeometryHelper::GetHexOffset(ipos32(input.FogOrigin.BaseHex), ipos32(base_hex));
    const auto center_alpha = is_shoot ? uint8 {255} : uint8 {0};
    const auto center_point = PrimitivePoint {.PointPos = {base_pos.x + half_hw, base_pos.y + half_hh}, .PointColor = ucolor {0, 0, 0, center_alpha}, .PointOffset = &_drawOffset};

    auto target_raw_hex = ipos32 {base_hex.x, base_hex.y};
    size_t points_added = 0;

    bool seek_start = true;

    for (auto i = 0; i < (GameSettings::HEXAGONAL_GEOMETRY ? 6 : 4); i++) {
        uint8 dir;
        int32 iterations;

        if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
            dir = numeric_cast<uint8>((i + 2) % 6);
            iterations = dist;
        }
        else {
            dir = numeric_cast<uint8>(((i + 1) * 2) % 8);
            iterations = dist * 2;
        }

        for (int32 j = 0; j < iterations; j++) {
            if (seek_start) {
                for (int32 l = 0; l < dist; l++) {
                    GeometryHelper::MoveHexByDirUnsafe(target_raw_hex, GameSettings::HEXAGONAL_GEOMETRY ? 0 : 7);
                }
                seek_start = false;
                j = -1;
            }
            else {
                GeometryHelper::MoveHexByDirUnsafe(target_raw_hex, numeric_cast<uint8>(dir));
            }

            auto target_hex = input.MapSize.clamp_pos(target_raw_hex);

            if (IsBitSet(input.LookChecks, LOOK_CHECK_DIR)) {
                FO_RUNTIME_ASSERT(input.LookDir != nullptr);

                const int32 dir_ = GeometryHelper::GetDir(base_hex, target_hex);
                auto ii = (chosen_dir > dir_ ? chosen_dir - dir_ : dir_ - chosen_dir);

                if (ii > GameSettings::MAP_DIR_COUNT / 2) {
                    ii = GameSettings::MAP_DIR_COUNT - ii;
                }

                const auto look_dist = dist - dist * (*input.LookDir)[ii] / 100;
                target_hex = input.TraceBulletToBlock(base_hex, target_hex, look_dist, false);
            }

            if (IsBitSet(input.LookChecks, LOOK_CHECK_TRACE_CLIENT)) {
                target_hex = input.TraceBulletToBlock(base_hex, target_hex, 0, true);
            }

            const auto dist_look = GeometryHelper::GetDistance(base_hex, target_hex);
            const auto* edge_offset = dist_look < dist ? &_baseDrawOffset : &_drawOffset;

            if (is_shoot) {
                const auto max_shoot_dist = std::max(std::min(dist_look, input.Distance), 0) + 1;
                const auto block_hex = input.TraceBulletToBlock(base_hex, target_hex, max_shoot_dist, true);
                const auto block_hex_pos = GeometryHelper::GetHexOffset(ipos32(base_hex), ipos32(block_hex));
                const auto result_shoot_dist = GeometryHelper::GetDistance(base_hex, block_hex);
                const auto color = ucolor {255, numeric_cast<uint8>(result_shoot_dist * 255 / max_shoot_dist), 0, 255};
                const auto* shoot_offset = result_shoot_dist < max_shoot_dist ? &_baseDrawOffset : &_drawOffset;

                fog_points.emplace_back(PrimitivePoint {.PointPos = {block_hex_pos.x + half_hw, block_hex_pos.y + half_hh}, .PointColor = color, .PointOffset = shoot_offset});
            }
            else {
                const auto hex_pos = GeometryHelper::GetHexOffset(ipos32(base_hex), ipos32(target_hex));
                const auto color = ucolor {255, numeric_cast<uint8>(dist_look * 255 / dist), 0, 0};

                fog_points.emplace_back(PrimitivePoint {.PointPos = {hex_pos.x + half_hw, hex_pos.y + half_hh}, .PointColor = color, .PointOffset = edge_offset});
            }

            if (++points_added % 2 == 0) {
                fog_points.emplace_back(center_point);
            }
        }
    }
}

void FogOfWar::StartTransition(vector<PrimitivePoint>&& points, nanotime frame_time, int32 duration)
{
    FO_STACK_TRACE_ENTRY();

    const auto clamped_duration = std::max(duration, 0);

    if (points.empty()) {
        if (_collapsingToOff || _points.empty()) {
            return;
        }

        _collapsingToOff = true;

        if (clamped_duration > 0) {
            _startPoints = _points;
            _targetPoints = MakeCollapsed(_points);
            _transitionTime = frame_time;
            _transitionActive = true;
        }
        else {
            _points = MakeCollapsed(_points);
            _targetPoints = _points;
            _startPoints.clear();
            _transitionActive = false;
        }

        return;
    }

    if (_collapsingToOff) {
        _collapsingToOff = false;

        if (clamped_duration > 0) {
            if (_points.empty()) {
                _points = MakeCollapsed(points);
            }
            _startPoints = _points;
            _targetPoints = std::move(points);
            _transitionTime = frame_time;
            _transitionActive = true;
        }
        else {
            _points = std::move(points);
            _targetPoints = _points;
            _startPoints.clear();
            _transitionActive = false;
        }

        return;
    }

    if (clamped_duration == 0) {
        _points = std::move(points);
        _startPoints.clear();
        _targetPoints = _points;
        _transitionActive = false;
    }
    else {
        if (!_transitionActive && _points.empty()) {
            _points = MakeCollapsed(points);
        }

        _startPoints = _points;
        _targetPoints = std::move(points);
        _transitionTime = frame_time;
        _transitionActive = true;
    }
}

void FogOfWar::UpdateTransition(nanotime frame_time, int32 duration)
{
    FO_STACK_TRACE_ENTRY();

    if (!_transitionActive) {
        return;
    }

    const auto clamped_duration = std::max(duration, 0);

    if (clamped_duration == 0) {
        _points = _targetPoints;
        _transitionActive = false;
        return;
    }

    const auto progress = std::clamp((frame_time - _transitionTime).div<float32>(std::chrono::milliseconds {clamped_duration}), 0.0f, 1.0f);

    InterpolatePoints(_startPoints, _targetPoints, progress, _points);

    if (progress >= 1.0f) {
        _points = _targetPoints;
        _transitionActive = false;

        if (_collapsingToOff) {
            _points.clear();
            _startPoints.clear();
            _targetPoints.clear();
        }
    }
}

auto FogOfWar::GetCollapsePoint(const vector<PrimitivePoint>& points) -> PrimitivePoint
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

auto FogOfWar::MakeCollapsed(const vector<PrimitivePoint>& points) const -> vector<PrimitivePoint>
{
    FO_NO_STACK_TRACE_ENTRY();

    if (points.empty()) {
        return {};
    }

    const auto center = GetCollapsePoint(points);

    auto collapsed = points;

    for (auto& p : collapsed) {
        p.PointPos = center.PointPos;
        p.PointColor = center.PointColor;
        p.PointOffset = &_drawOffset;
    }

    return collapsed;
}

auto FogOfWar::SampleEdgePoint(const vector<PrimitivePoint>& points, size_t edge_count, size_t sample_edge_idx, size_t sample_edge_count, const PrimitivePoint& fallback) -> PrimitivePoint
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

auto FogOfWar::LerpFogColor(ucolor from, ucolor to, float32 t) -> ucolor
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto lerp_channel = [t](uint8 from_value, uint8 to_value) -> uint8 { return numeric_cast<uint8>(std::clamp(iround<int32>(lerp(numeric_cast<float32>(from_value), numeric_cast<float32>(to_value), t)), 0, 255)); };

    return ucolor {lerp_channel(from.comp.r, to.comp.r), lerp_channel(from.comp.g, to.comp.g), lerp_channel(from.comp.b, to.comp.b), lerp_channel(from.comp.a, to.comp.a)};
}

auto FogOfWar::LerpFogPoint(const PrimitivePoint& from, const PrimitivePoint& to, float32 t) -> PrimitivePoint
{
    FO_NO_STACK_TRACE_ENTRY();

    PrimitivePoint result;
    result.PointPos.x = iround<int32>(lerp(numeric_cast<float32>(from.PointPos.x), numeric_cast<float32>(to.PointPos.x), t));
    result.PointPos.y = iround<int32>(lerp(numeric_cast<float32>(from.PointPos.y), numeric_cast<float32>(to.PointPos.y), t));
    result.PointColor = LerpFogColor(from.PointColor, to.PointColor, t);
    result.PointOffset = to.PointOffset ? to.PointOffset : from.PointOffset;
    result.PPointColor = to.PPointColor ? to.PPointColor : from.PPointColor;
    return result;
}

void FogOfWar::InterpolatePoints(const vector<PrimitivePoint>& from_points, const vector<PrimitivePoint>& to_points, float32 t, vector<PrimitivePoint>& result_points)
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto result_count = std::max(from_points.size(), to_points.size());

    if (result_count == 0) {
        result_points.clear();
        return;
    }

    // Triangle strip: edge, edge, center repeating (center at every i % 3 == 2).
    // Resample edge and center points independently so that changing point counts
    // (e.g. from different LookDistance) never mix edge and center positions.

    const auto center_point = GetCollapsePoint(!to_points.empty() ? to_points : from_points);

    const auto from_edge_count = from_points.size() - from_points.size() / 3;
    const auto to_edge_count = to_points.size() - to_points.size() / 3;
    const auto result_edge_count = result_count - result_count / 3;

    result_points.resize(result_count);

    size_t edge_idx = 0;

    for (size_t i = 0; i < result_count; i++) {
        if (i % 3 == 2) {
            result_points[i] = center_point;
        }
        else {
            const auto from_edge = SampleEdgePoint(from_points, from_edge_count, edge_idx, result_edge_count, center_point);
            const auto to_edge = SampleEdgePoint(to_points, to_edge_count, edge_idx, result_edge_count, center_point);
            result_points[i] = LerpFogPoint(from_edge, to_edge, t);
            edge_idx++;
        }
    }
}

FO_END_NAMESPACE
