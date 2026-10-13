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

#include "FogOfWar.h"

#include "Geometry.h"
#include "LineTracer.h"
#include "WorkScheduler.h"

FO_BEGIN_NAMESPACE

static auto ReadFogObstacle(const FogShape::ObstacleView& view, mpos hex) -> bool
{
    int32_t x = hex.x - view.Origin.x;
    int32_t y = hex.y - view.Origin.y;
    FO_VERIFY_AND_THROW(x >= 0 && y >= 0 && x < view.Size.width && y < view.Size.height, "Fog obstacle lookup is outside its snapshot", hex, view.Origin, view.Size);
    size_t index = numeric_cast<size_t>(y) * numeric_cast<size_t>(view.Size.width) + numeric_cast<size_t>(x);
    FO_VERIFY_AND_THROW(index < view.ShootBlocked.size(), "Fog obstacle snapshot is incomplete", index, view.ShootBlocked.size());
    return view.ShootBlocked[index] != 0;
}

template<typename Trace, typename Emit>
static void CalculateFogRange(const FogShape::CalculationInput& input, size_t first_sector, size_t sector_count, const Trace& trace, const Emit& emit)
{
    if (!input.Enabled || !input.FogOrigin.Valid) {
        return;
    }

    int32_t dist = std::max(input.Radius > 0 ? input.Radius : input.FogOrigin.LookDistance + input.FogExtraLength, 0);

    if (dist <= 0) {
        return;
    }

    constexpr size_t sectors = GameSettings::HEXAGONAL_GEOMETRY ? 6 : 4;
    FO_VERIFY_AND_THROW(first_sector < sectors && sector_count <= sectors - first_sector, "Fog sector range is invalid", first_sector, sector_count);
    bool is_traced_overlay = input.TraceMode == FogShape::TraceModeType::Overlay;
    mpos base_hex = input.FogOrigin.BaseHex;
    int32_t half_hw = input.MapHexWidth / 2;
    int32_t half_hh = input.MapHexHeight / 2;
    ucolor overlay_color = input.OverlayColor.comp.a != 0 ? input.OverlayColor : ucolor {96, 224, 96, 255};
    ucolor center_color = is_traced_overlay ? ucolor {overlay_color.comp.r, overlay_color.comp.g, overlay_color.comp.b, 255} : (input.CenterColor.comp.a != 0 ? input.CenterColor : ucolor {0, 0, 0, 0});
    FogShape::CalculatedPoint center {{half_hw, half_hh}, center_color, false};
    ipos32 raw {base_hex.x, base_hex.y};
    int32_t iterations = GameSettings::HEXAGONAL_GEOMETRY ? dist : dist * 2;
    auto direction = [](size_t sector) {
        if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
            return mdir {hdir(numeric_cast<int32_t>((sector + 2) % 6))};
        }
        else {
            return mdir {hdir(numeric_cast<int32_t>(((sector + 1) * 2) % 8))};
        }
    };

    for (int32_t i = 0; i < dist; i++) {
#if FO_GEOMETRY == 1
        GeometryHelper::MoveHexByDirUnsafe(raw, hdir::NorthEast);
#else
        GeometryHelper::MoveHexByDirUnsafe(raw, hdir::North);
#endif
    }

    for (size_t sector = 0; sector < first_sector; sector++) {
        for (int32_t i = 0; i < iterations; i++) {
            GeometryHelper::MoveHexByDirUnsafe(raw, direction(sector));
        }
    }

    size_t points_added = first_sector == 0 ? 0 : first_sector * numeric_cast<size_t>(iterations) + 1;

    for (size_t sector = first_sector; sector < first_sector + sector_count; sector++) {
        int32_t edge_count = iterations + (sector == 0 ? 1 : 0);

        for (int32_t edge = 0; edge < edge_count; edge++) {
            if (sector != 0 || edge != 0) {
                GeometryHelper::MoveHexByDirUnsafe(raw, direction(sector));
            }

            mpos target_hex = input.MapSize.clamp_pos(raw);
            int32_t target_dist = GeometryHelper::GetDistance(base_hex, target_hex);

            if (is_traced_overlay) {
                int32_t max_dist = std::max(std::min(target_dist, input.Distance), 0) + 1;
                mpos block_hex = trace(base_hex, target_hex, max_dist, input.CheckShootBlocks);
                ipos32 position = GeometryHelper::GetHexOffset(ipos32(base_hex), ipos32(block_hex));
                int32_t result_dist = GeometryHelper::GetDistance(base_hex, block_hex);
                uint8_t strength = numeric_cast<uint8_t>(std::max(max_dist - result_dist, 0) * 255 / max_dist);
                ucolor color {numeric_cast<uint8_t>(overlay_color.comp.r * strength / 255), numeric_cast<uint8_t>(overlay_color.comp.g * strength / 255), numeric_cast<uint8_t>(overlay_color.comp.b * strength / 255), 255};
                emit(FogShape::CalculatedPoint {{position.x + half_hw, position.y + half_hh}, color, result_dist < max_dist});
            }
            else {
                ipos32 position = GeometryHelper::GetHexOffset(ipos32(base_hex), ipos32(target_hex));
                ucolor color {255, numeric_cast<uint8_t>(target_dist * 255 / dist), 0, 0};
                emit(FogShape::CalculatedPoint {{position.x + half_hw, position.y + half_hh}, color, target_dist < dist});
            }

            if (++points_added % 2 == 0) {
                emit(center);
            }
        }
    }
}

auto FogShape::CalculateSector(const CalculationInput& input, const ObstacleView& obstacles, size_t sector) -> vector<CalculatedPoint>
{
    FO_TRACE_ZONE(Map);

    vector<CalculatedPoint> points;
    auto trace = [&input, &obstacles](mpos start, mpos target, int32_t distance, bool check) {
        mpos next = start;
        LineTracer tracer(start, target, 0.0f, input.MapSize);

        for (int32_t i = 0; i < distance; i++) {
            if (!tracer.GetNextHex(next).has_value()) {
                break;
            }
            if (check && ReadFogObstacle(obstacles, next)) {
                break;
            }
        }

        return next;
    };
    CalculateFogRange(input, sector, 1, trace, [&points](CalculatedPoint point) { points.emplace_back(point); });
    return points;
}

void FogShape::Clear()
{
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

void FogShape::Prepare(const Input& input, nptr<WorkScheduler> scheduler, const function<bool(mpos)>& read_shoot_block)
{
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
        vector<PrimitivePoint> points;
        BuildPoints(input, points, scheduler, read_shoot_block);
        _rebuildFog = false;
        StartTransition(std::move(points), input.FrameTime, input.FogTransitionDuration);
    }

    UpdateTransition(input.FrameTime);
}

void FogShape::BuildPoints(const Input& input, vector<PrimitivePoint>& fog_points, nptr<WorkScheduler> scheduler, const function<bool(mpos)>& read_shoot_block) const
{
    FO_TRACE_ZONE(Map);

    constexpr size_t sectors = GameSettings::HEXAGONAL_GEOMETRY ? 6 : 4;
    int32_t dist = std::max(input.Radius > 0 ? input.Radius : input.FogOrigin.LookDistance + input.FogExtraLength, 0);
    auto publish = [this, &fog_points](CalculatedPoint point) { fog_points.emplace_back(PrimitivePoint {.PointPos = point.Position, .PointColor = point.Color, .PointOffset = point.BaseOffset ? ptr<const ipos32> {_baseDrawOffset} : ptr<const ipos32> {_drawOffset}}); };

    // No request list, obstacle snapshot or jobs for serial, clean, disabled or small shapes
    if (!input.Enabled || !input.FogOrigin.Valid || dist < 32 || !scheduler || !scheduler->ShouldRunParallel(sectors, 2)) {
        if (input.Enabled && input.FogOrigin.Valid && dist > 0 && input.TraceMode == TraceModeType::Overlay) {
            FO_VERIFY_AND_THROW(input.TraceBulletToBlock, "Trace bullet to block callback is required in overlay trace mode");
        }

        CalculateFogRange(input, 0, sectors, input.TraceBulletToBlock, publish);
        return;
    }

    CalculationInput calculation = input;
    ObstacleView view;
    vector<uint8_t> obstacles;

    if (input.TraceMode == TraceModeType::Overlay && input.CheckShootBlocks) {
        FO_VERIFY_AND_THROW(read_shoot_block, "Fog obstacle reader is required for parallel overlay calculation");
        int32_t margin = std::min(dist, numeric_cast<int32_t>(std::max(input.MapSize.width, input.MapSize.height))) + 2;
        view.Origin = {std::max(input.FogOrigin.BaseHex.x - margin, 0), std::max(input.FogOrigin.BaseHex.y - margin, 0)};
        view.Size = {std::min(input.FogOrigin.BaseHex.x + margin + 1, numeric_cast<int32_t>(input.MapSize.width)) - view.Origin.x, std::min(input.FogOrigin.BaseHex.y + margin + 1, numeric_cast<int32_t>(input.MapSize.height)) - view.Origin.y};
        obstacles.resize(numeric_cast<size_t>(view.Size.width) * numeric_cast<size_t>(view.Size.height));

        for (int32_t y = 0; y < view.Size.height; y++) {
            for (int32_t x = 0; x < view.Size.width; x++) {
                obstacles[numeric_cast<size_t>(y * view.Size.width + x)] = read_shoot_block(input.MapSize.from_raw_pos(view.Origin.x + x, view.Origin.y + y)) ? 1 : 0;
            }
        }

        view.ShootBlocked = obstacles;
    }

    array<vector<CalculatedPoint>, sectors> results;
    scheduler->RunBatch("FogGeometry", sectors, 1, [&calculation, &view, &results](size_t sector) { results[sector] = CalculateSector(calculation, view, sector); });

    // Bind owner offsets only after every sector has completed, preserving the global edge/center order
    for (const auto& result : results) {
        for (CalculatedPoint point : result) {
            publish(point);
        }
    }
}

void FogShape::StartTransition(vector<PrimitivePoint>&& points, nanotime frame_time, int32_t duration)
{
    // One morph duration for every change: the oval grows from its center on appearance, glides between
    // shapes as the player moves, and shrinks back to center on disappearance. No separate slow reveal
    _transitionDuration = std::max(duration, 0);

    if (points.empty()) {
        // Disappear: shrink the current fan to its center, then clear when the morph finishes. Nothing to
        // do if it is already gone or already collapsing
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
    if (!_transitionActive) {
        return;
    }

    int32_t clamped_duration = std::max(_transitionDuration, 0);

    if (clamped_duration == 0) {
        FinishTransition();
        return;
    }

    float32_t progress = std::clamp((frame_time - _transitionTime).div<float32_t>(std::chrono::milliseconds {clamped_duration}), 0.0f, 1.0f);

    InterpolatePoints(_startPoints, _targetPoints, progress, _points);

    if (progress >= 1.0f) {
        FinishTransition();
    }
}

void FogShape::FinishTransition()
{
    _points = _targetPoints;
    _transitionActive = false;

    // A collapse-to-off morph ends with the fog fully cleared
    if (_collapsingToOff) {
        _points.clear();
        _startPoints.clear();
        _targetPoints.clear();
        _collapsingToOff = false;
    }
}

auto FogShape::GetCollapsePoint(const vector<PrimitivePoint>& points) -> PrimitivePoint
{
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
    if (points.empty()) {
        return {};
    }

    auto center = GetCollapsePoint(points);

    auto collapsed = points;

    // Collapse only the position to the origin; keep each point's own color so a growing/shrinking
    // fan keeps its soft faded edges throughout the transition instead of flashing the center color
    for (auto& p : collapsed) {
        p.PointPos = center.PointPos;
        p.PointOffset = _drawOffset;
    }

    return collapsed;
}

auto FogShape::SampleEdgePoint(const vector<PrimitivePoint>& points, size_t edge_count, size_t sample_edge_idx, size_t sample_edge_count, const PrimitivePoint& fallback) -> PrimitivePoint
{
    if (edge_count == 0) {
        return fallback;
    }
    if (sample_edge_count <= 1) {
        return points[0];
    }

    auto src_edge = std::min((sample_edge_idx * (edge_count - 1) + (sample_edge_count - 1) / 2) / (sample_edge_count - 1), edge_count - 1);
    // Edge index to array index: skip every third slot (center)
    // edge 0 → arr 0, edge 1 → arr 1, edge 2 → arr 3, edge 3 → arr 4, ...
    auto arr_idx = 3 * (src_edge / 2) + (src_edge % 2);
    return arr_idx < points.size() ? points[arr_idx] : fallback;
}

auto FogShape::LerpFogColor(ucolor from, ucolor to, float32_t t) -> ucolor
{
    auto lerp_channel = [t](uint8_t from_value, uint8_t to_value) -> uint8_t { return numeric_cast<uint8_t>(std::clamp(iround<int32_t>(lerp(numeric_cast<float32_t>(from_value), numeric_cast<float32_t>(to_value), t)), 0, 255)); };

    return ucolor {lerp_channel(from.comp.r, to.comp.r), lerp_channel(from.comp.g, to.comp.g), lerp_channel(from.comp.b, to.comp.b), lerp_channel(from.comp.a, to.comp.a)};
}

auto FogShape::LerpFogPoint(const PrimitivePoint& from, const PrimitivePoint& to, float32_t t) -> PrimitivePoint
{
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
    auto result_count = std::max(from_points.size(), to_points.size());

    if (result_count == 0) {
        result_points.clear();
        return;
    }

    auto from_center = GetCollapsePoint(from_points);
    auto to_center = GetCollapsePoint(to_points);
    auto fallback_center = !to_points.empty() ? to_center : from_center;

    auto from_edge_count = from_points.size() - from_points.size() / 3;
    auto to_edge_count = to_points.size() - to_points.size() / 3;
    auto result_edge_count = result_count - result_count / 3;

    result_points.resize(result_count);

    size_t edge_idx = 0;

    for (size_t i = 0; i < result_count; i++) {
        if (i % 3 == 2) {
            result_points[i] = LerpFogPoint(from_center, to_center, t);
        }
        else {
            auto from_edge = SampleEdgePoint(from_points, from_edge_count, edge_idx, result_edge_count, fallback_center);
            auto to_edge = SampleEdgePoint(to_points, to_edge_count, edge_idx, result_edge_count, fallback_center);
            result_points[i] = LerpFogPoint(from_edge, to_edge, t);
            edge_idx++;
        }
    }
}

FO_END_NAMESPACE

