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

#include "Movement.h"

FO_BEGIN_NAMESPACE

static constexpr size_t NEXT_HEX_TIME_SEARCH_ITERATIONS = 20;

MovingContext::MovingContext(msize map_size, uint16 speed, vector<uint8> steps, vector<uint16> control_steps, nanotime start_time, timespan offset_time, mpos start_hex, ipos16 start_hex_offset, ipos16 end_hex_offset) :
    _mapSize {map_size},
    _speed {speed},
    _steps {std::move(steps)},
    _controlSteps {std::move(control_steps)},
    _startTime {start_time},
    _offsetTime {offset_time},
    _startHex {start_hex},
    _startHexOffset {start_hex_offset},
    _endHexOffset {end_hex_offset}
{
    FO_STACK_TRACE_ENTRY();

    _elapsedTime = std::max(offset_time.to_ms<float32>(), 0.0f);
    RecalculateMetrics();
}

MovingContext::MovingContext(msize map_size, uint16 speed, vector<uint8> steps, vector<uint16> control_steps, nanotime start_time, timespan offset_time, mpos start_hex, ipos16 start_hex_offset, ipos16 end_hex_offset, float32 whole_time) :
    MovingContext(map_size, speed, std::move(steps), std::move(control_steps), start_time, offset_time, start_hex, start_hex_offset, end_hex_offset)
{
    FO_STACK_TRACE_ENTRY();

    _wholeTime = std::max(whole_time, 0.0001f);
}

void MovingContext::RecalculateMetrics()
{
    FO_STACK_TRACE_ENTRY();

    const auto metrics = EvaluateMetrics();

    _endHex = metrics.EndHex;
    _wholeTime = metrics.WholeTime;
    _wholeDist = metrics.WholeDist;
}

auto MovingContext::GetRuntimeElapsedTime(nanotime current_time) const noexcept -> float32
{
    return std::max((current_time - _startTime + _offsetTime).to_ms<float32>(), 0.0f);
}

void MovingContext::EvaluateSegment(uint16 control_step_begin, uint16 control_step_end, mpos segment_start_hex, bool is_last, mpos& segment_end_hex, ipos32& offset, float32& dist) const
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(control_step_begin <= control_step_end);
    FO_RUNTIME_ASSERT(control_step_end <= _steps.size());

    segment_end_hex = segment_start_hex;

    for (uint16 j = control_step_begin; j < control_step_end; j++) {
        const auto move_ok = GeometryHelper::MoveHexByDir(segment_end_hex, _steps[j], _mapSize);
        FO_RUNTIME_ASSERT(move_ok);
    }

    auto&& [ox, oy] = GeometryHelper::GetHexOffset(segment_start_hex, segment_end_hex);

    if (control_step_begin == 0) {
        ox -= _startHexOffset.x;
        oy -= _startHexOffset.y;
    }
    if (is_last) {
        ox += _endHexOffset.x;
        oy += _endHexOffset.y;
    }

    const float32 proj_oy = numeric_cast<float32>(oy) * GeometryHelper::GetYProj();
    offset = {ox, oy};
    dist = std::sqrt(numeric_cast<float32>(ox * ox) + proj_oy * proj_oy);
}

auto MovingContext::EvaluateRawProgress(float32 elapsed_time_ms) const -> MovingRawProgress
{
    FO_STACK_TRACE_ENTRY();

    MovingRawProgress raw_progress;
    raw_progress.Hex = _startHex;

    if (_steps.empty() || _controlSteps.empty() || _wholeTime <= 0.0f || _wholeDist <= 0.0f) {
        return raw_progress;
    }

    const float32 normalized_time = std::clamp(elapsed_time_ms / _wholeTime, 0.0f, 1.0f);
    const float32 dist_pos = _wholeDist * normalized_time;
    float32 cur_dist = 0.0f;
    mpos next_start_hex = _startHex;
    uint16 control_step_begin = 0;

    for (size_t i = 0; i < _controlSteps.size(); i++) {
        mpos segment_end_hex;
        ipos32 segment_offset;
        float32 dist = 0.0f;
        EvaluateSegment(control_step_begin, _controlSteps[i], next_start_hex, i == _controlSteps.size() - 1, segment_end_hex, segment_offset, dist);
        const float32 clamped_dist = std::max(dist, 0.0001f);

        if ((normalized_time < 1.0f && dist_pos >= cur_dist && dist_pos <= cur_dist + clamped_dist) || (normalized_time == 1.0f && i == _controlSteps.size() - 1)) {
            const auto normalized_dist = std::clamp((dist_pos - cur_dist) / clamped_dist, 0.0f, 1.0f);

            const auto step_index = control_step_begin + iround<int32>(normalized_dist * numeric_cast<float32>(_controlSteps[i] - control_step_begin));
            FO_RUNTIME_ASSERT(step_index >= numeric_cast<int32>(control_step_begin));
            FO_RUNTIME_ASSERT(step_index <= numeric_cast<int32>(_controlSteps[i]));

            auto current_hex = next_start_hex;

            for (int32 j = control_step_begin; j < step_index; j++) {
                const auto move_ok = GeometryHelper::MoveHexByDir(current_hex, _steps[j], _mapSize);
                FO_RUNTIME_ASSERT(move_ok);
            }

            raw_progress.Hex = current_hex;
            raw_progress.SegmentStartHex = next_start_hex;
            raw_progress.SegmentOffset = segment_offset;
            raw_progress.NormalizedDist = normalized_dist;
            raw_progress.Completed = normalized_time == 1.0f;
            raw_progress.Found = true;
            return raw_progress;
        }

        cur_dist += clamped_dist;
        raw_progress.Hex = segment_end_hex;
        control_step_begin = _controlSteps[i];
        next_start_hex = segment_end_hex;
    }

    return raw_progress;
}

void MovingContext::ChangeSpeed(uint16 speed, nanotime current_time)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_speed != 0);

    const auto diff = numeric_cast<float32>(speed) / numeric_cast<float32>(_speed);
    const auto elapsed_time = _elapsedTime;

    _wholeTime /= diff;
    _wholeTime = std::max(_wholeTime, 0.0001f);
    _startTime = current_time;
    _offsetTime = std::chrono::milliseconds {iround<int32>(elapsed_time / diff)};
    _elapsedTime = std::max(_offsetTime.to_ms<float32>(), 0.0f);
    _speed = speed;
}

void MovingContext::Complete(MovingState reason) noexcept
{
    if (reason == MovingState::Success) {
        _elapsedTime = std::max(_wholeTime, _elapsedTime);
    }

    _completeReason = reason;
    _completed = true;
}

void MovingContext::SetBlockHexes(mpos pre_block_hex, mpos block_hex) noexcept
{
    _preBlockHex = pre_block_hex;
    _blockHex = block_hex;
}

void MovingContext::ValidateRuntimeState() const
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!_steps.empty());
    FO_RUNTIME_ASSERT(!_controlSteps.empty());
    FO_RUNTIME_ASSERT(_wholeTime > 0.0f);
    FO_RUNTIME_ASSERT(_wholeDist > 0.0f);
}

auto MovingContext::EvaluateMetrics() const -> MovingMetrics
{
    FO_STACK_TRACE_ENTRY();

    MovingMetrics metrics;
    metrics.EndHex = _startHex;

    const auto base_move_speed = numeric_cast<float32>(_speed);
    mpos next_start_hex = _startHex;
    uint16 control_step_begin = 0;

    for (size_t i = 0; i < _controlSteps.size(); i++) {
        mpos segment_end_hex;
        ipos32 segment_offset;
        float32 dist = 0.0f;

        EvaluateSegment(control_step_begin, _controlSteps[i], next_start_hex, i == _controlSteps.size() - 1, segment_end_hex, segment_offset, dist);
        ignore_unused(segment_offset);

        metrics.WholeDist += dist;

        if (_speed != 0) {
            metrics.WholeTime += dist / base_move_speed * 1000.0f;
        }

        metrics.EndHex = segment_end_hex;
        control_step_begin = _controlSteps[i];
        next_start_hex = segment_end_hex;
    }

    metrics.WholeDist = std::max(metrics.WholeDist, 0.0001f);
    
    if (_speed != 0) {
        metrics.WholeTime = std::max(metrics.WholeTime, 0.0001f);
    }

    return metrics;
}

auto MovingContext::EvaluateProjectedHex(float32 look_ahead_ms) const -> mpos
{
    FO_STACK_TRACE_ENTRY();

    if (_steps.empty() || _controlSteps.empty() || _wholeTime <= 0.0f || _wholeDist <= 0.0f) {
        return _endHex;
    }

    return EvaluateRawProgress(_elapsedTime + std::max(look_ahead_ms, 0.0f)).Hex;
}

auto MovingContext::EvaluateNearestPathHex(mpos current_hex, mpos from_hex, mpos fallback_hex) const -> mpos
{
    FO_STACK_TRACE_ENTRY();

    auto best_hex = fallback_hex;
    auto best_dist = GeometryHelper::GetDistance(from_hex, best_hex);

    if (_steps.empty()) {
        const auto end_dist = GeometryHelper::GetDistance(from_hex, _endHex);

        if (end_dist < best_dist) {
            best_hex = _endHex;
            best_dist = end_dist;
        }

        const auto current_dist = GeometryHelper::GetDistance(from_hex, current_hex);

        if (current_dist < best_dist) {
            best_hex = current_hex;
        }

        return best_hex;
    }

    auto hex = _startHex;
    bool use_path_point = hex == current_hex;

    for (const auto step : _steps) {
        if (!GeometryHelper::MoveHexByDir(hex, step, _mapSize)) {
            break;
        }

        if (!use_path_point) {
            use_path_point = hex == current_hex;
            continue;
        }

        const auto dist = GeometryHelper::GetDistance(from_hex, hex);

        if (dist < best_dist) {
            best_dist = dist;
            best_hex = hex;

            if (best_dist == 0) {
                break;
            }
        }
    }

    return best_hex;
}

auto MovingContext::EvaluatePathHexes(mpos current_hex) const -> vector<mpos>
{
    FO_STACK_TRACE_ENTRY();

    vector<mpos> path_hexes;

    if (_steps.empty()) {
        if (current_hex != _endHex) {
            path_hexes.emplace_back(current_hex);
            path_hexes.emplace_back(_endHex);
        }

        return path_hexes;
    }

    mpos hex = _startHex;
    bool include_hex = hex == current_hex;

    if (include_hex) {
        path_hexes.emplace_back(hex);
    }

    for (const uint8 step : _steps) {
        if (!GeometryHelper::MoveHexByDir(hex, step, _mapSize)) {
            break;
        }

        if (!include_hex) {
            include_hex = hex == current_hex;
        }

        if (include_hex) {
            path_hexes.emplace_back(hex);
        }
    }

    return path_hexes;
}

auto MovingContext::EvaluateProgress() const -> MovingProgress
{
    FO_STACK_TRACE_ENTRY();

    const auto raw_progress = EvaluateRawProgress(_elapsedTime);
    return BuildProgress(raw_progress, raw_progress.Hex);
}

auto MovingContext::EvaluateProgress(mpos current_hex) const -> MovingProgress
{
    FO_STACK_TRACE_ENTRY();

    return BuildProgress(EvaluateRawProgress(_elapsedTime), current_hex);
}

auto MovingContext::BuildProgress(const MovingRawProgress& raw_progress, mpos current_hex) const -> MovingProgress
{
    FO_STACK_TRACE_ENTRY();

    MovingProgress progress;
    progress.Hex = raw_progress.Hex;
    progress.Completed = raw_progress.Completed;

    if (!raw_progress.Found) {
        return progress;
    }

    auto&& [current_ox, current_oy] = GeometryHelper::GetHexOffset(raw_progress.SegmentStartHex, current_hex);

    if (raw_progress.SegmentStartHex == _startHex) {
        current_ox -= _startHexOffset.x;
        current_oy -= _startHexOffset.y;
    }

    const float32 lerp = [](int32 a, int32 b, float32 t) -> float32 { return numeric_cast<float32>(a) * (1.0f - t) + numeric_cast<float32>(b) * t; };
    const float32 offset_x = lerp(0, raw_progress.SegmentOffset.x, raw_progress.NormalizedDist) - numeric_cast<float32>(current_ox);
    const float32 offset_y = lerp(0, raw_progress.SegmentOffset.y, raw_progress.NormalizedDist) - numeric_cast<float32>(current_oy);

    progress.HexOffset = ipos16 {numeric_cast<int16>(iround<int32>(offset_x)), numeric_cast<int16>(iround<int32>(offset_y))};
    progress.DirAngle = iround<int16>(GeometryHelper::GetLineDirAngle(0, 0, raw_progress.SegmentOffset.x, raw_progress.SegmentOffset.y));
    return progress;
}

void MovingContext::UpdateCurrentTime(nanotime current_time)
{
    FO_STACK_TRACE_ENTRY();

    _elapsedTime = GetRuntimeElapsedTime(current_time);
}

void MovingContext::UpdateCurrentTimeToNextHex(nanotime current_time, mpos current_hex)
{
    FO_STACK_TRACE_ENTRY();

    const float32 runtime_elapsed = GetRuntimeElapsedTime(current_time);

    if (runtime_elapsed <= _elapsedTime) {
        _elapsedTime = runtime_elapsed;
        return;
    }

    if (EvaluateRawProgress(_elapsedTime).Hex != current_hex) {
        _elapsedTime = runtime_elapsed;
        return;
    }

    if (EvaluateRawProgress(runtime_elapsed).Hex == current_hex) {
        _elapsedTime = runtime_elapsed;
        return;
    }

    float32 low = _elapsedTime;
    float32 high = runtime_elapsed;

    for (size_t i = 0; i < NEXT_HEX_TIME_SEARCH_ITERATIONS; i++) {
        const float32 middle = (low + high) * 0.5f;

        if (EvaluateRawProgress(middle).Hex == current_hex) {
            low = middle;
        }
        else {
            high = middle;
        }
    }

    _elapsedTime = std::min(high, runtime_elapsed);
}

FO_END_NAMESPACE
