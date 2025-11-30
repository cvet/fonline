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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "LineTracer.h"

FO_BEGIN_NAMESPACE();

LineTracer::LineTracer(mpos start_hex, mpos target_hex, float32 dir_angle_offset, msize map_size)
{
    FO_STACK_TRACE_ENTRY();

    _mapSize = map_size;

    TraceInit(start_hex, target_hex, dir_angle_offset);
}

LineTracer::LineTracer(mpos start_hex, float32 dir_angle, int32 dist, msize map_size)
{
    FO_STACK_TRACE_ENTRY();

    _mapSize = map_size;

    auto angle = 360.0f - dir_angle + 60.0f;
    angle = angle < 0.0f ? 360.0f - std::fmod(-angle, 360.0f) : std::fmod(angle, 360.0f);

    auto dx = -std::cos(angle * DEG_TO_RAD_FLOAT);
    auto dy = -std::sin(angle * DEG_TO_RAD_FLOAT);

    if (std::abs(dx) > std::abs(dy)) {
        dy /= std::abs(dx);
        dx = dx > 0.0f ? 1.0f : -1.0f;
    }
    else {
        dx /= std::abs(dy);
        dy = dy > 0.0f ? 1.0f : -1.0f;
    }

    const auto sx = numeric_cast<float32>(start_hex.x) + 0.5f;
    const auto sy = numeric_cast<float32>(start_hex.y) + 0.5f;
    const auto tx = iround<int32>(std::floor(sx + dx * numeric_cast<float32>(dist)));
    const auto ty = iround<int32>(std::floor(sy + dy * numeric_cast<float32>(dist)));
    const auto target_hex = _mapSize.from_raw_pos(tx, ty);

    TraceInit(start_hex, target_hex, 0.0f);
}

void LineTracer::TraceInit(mpos start_hex, mpos target_hex, float32 dir_angle_offset)
{
    FO_STACK_TRACE_ENTRY();

    const auto sx = numeric_cast<float32>(start_hex.x);
    const auto sy = numeric_cast<float32>(start_hex.y);
    const auto tx = numeric_cast<float32>(target_hex.x);
    const auto ty = numeric_cast<float32>(target_hex.y);

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        constexpr auto bias = 0.02f;

        const auto sx_odd = numeric_cast<float32>(std::abs(start_hex.x % 2));
        const auto tx_odd = numeric_cast<float32>(std::abs(target_hex.x % 2));

        _xStart = sx * 3.0f + bias;
        _yStart = sy * SQRT3_X2_FLOAT - sx_odd * SQRT3_FLOAT + bias;

        auto x_end = tx * 3.0f + bias;
        auto y_end = ty * SQRT3_X2_FLOAT - tx_odd * SQRT3_FLOAT + bias;

        if (!is_float_equal(dir_angle_offset, 0.0f)) {
            x_end -= _xStart;
            y_end -= _yStart;

            const auto angle_offset_rad = -dir_angle_offset * DEG_TO_RAD_FLOAT;
            const auto ox = std::cos(angle_offset_rad) * x_end - std::sin(angle_offset_rad) * y_end;
            const auto oy = std::sin(angle_offset_rad) * x_end + std::cos(angle_offset_rad) * y_end;

            x_end = _xStart + ox;
            y_end = _yStart + oy;
        }

        _dx = x_end - _xStart;
        _dy = y_end - _yStart;

        const auto nx = (tx - sx) * 3.0f + bias;
        const auto ny = (ty - sy) * SQRT3_X2_FLOAT - (tx_odd - sx_odd) * SQRT3_FLOAT + bias;
        auto angle = 180.0f + std::atan2(ny, nx) * RAD_TO_DEG_FLOAT - dir_angle_offset;
        angle = angle < 0.0f ? 360.0f - std::fmod(-angle, 360.0f) : std::fmod(angle, 360.0f);

        _dirAngle = -60.0f + angle;
        _dirAngle = _dirAngle < 0.0f ? 360.0f - std::fmod(-_dirAngle, 360.0f) : std::fmod(_dirAngle, 360.0f);
        _dirAngle = 360.0f - _dirAngle;

        if (_dirAngle >= 30.0f && _dirAngle < 90.0f) {
            _dirLeft = 0;
            _dirRight = 1;
        }
        else if (_dirAngle >= 90.0f && _dirAngle < 150.0f) {
            _dirLeft = 1;
            _dirRight = 2;
        }
        else if (_dirAngle >= 150.0f && _dirAngle < 210.0f) {
            _dirLeft = 2;
            _dirRight = 3;
        }
        else if (_dirAngle >= 210.0f && _dirAngle < 270.0f) {
            _dirLeft = 3;
            _dirRight = 4;
        }
        else if (_dirAngle >= 270.0f && _dirAngle < 330.0f) {
            _dirLeft = 4;
            _dirRight = 5;
        }
        else {
            _dirLeft = 5;
            _dirRight = 0;
        }
    }
    else {
        const auto nx = tx - sx;
        const auto ny = ty - sy;
        auto angle = std::atan2(ny, nx) * RAD_TO_DEG_FLOAT;
        angle = angle < 0.0f ? 360.0f - std::fmod(-angle, 360.0f) : std::fmod(angle, 360.0f);

        _xStart = sx + 0.5f;
        _yStart = sy + 0.5f;

        _dx = std::cos(angle * DEG_TO_RAD_FLOAT);
        _dy = std::sin(angle * DEG_TO_RAD_FLOAT);

        if (std::abs(_dx) > std::abs(_dy)) {
            _dy /= std::abs(_dx);
            _dx = _dx > 0 ? 1.0f : -1.0f;
        }
        else {
            _dx /= std::abs(_dy);
            _dy = _dy > 0 ? 1.0f : -1.0f;
        }

        _dirAngle = -60.0f + angle;
        _dirAngle = _dirAngle < 0.0f ? 360.0f - std::fmod(-_dirAngle, 360.0f) : std::fmod(_dirAngle, 360.0f);
        _dirAngle = 360.0f - _dirAngle;
    }

    _x = _xStart;
    _y = _yStart;
}

auto LineTracer::GetNextHex(mpos& hex) const -> uint8
{
    FO_STACK_TRACE_ENTRY();

    auto left_hex = hex;
    auto right_hex = hex;
    GeometryHelper::MoveHexByDir(left_hex, _dirLeft, _mapSize);
    GeometryHelper::MoveHexByDir(right_hex, _dirRight, _mapSize);

    const auto left_hex_f = fpos32(left_hex);
    const auto right_hex_f = fpos32(right_hex);
    const auto dist_left = std::abs(_dx * (_yStart - (left_hex_f.y * SQRT3_X2_FLOAT - numeric_cast<float32>(std::abs(left_hex.x % 2)) * SQRT3_FLOAT)) - _dy * (_xStart - 3.0f * left_hex_f.x));
    const auto dist_right = std::abs(_dx * (_yStart - (right_hex_f.y * SQRT3_X2_FLOAT - numeric_cast<float32>(std::abs(right_hex.x % 2)) * SQRT3_FLOAT)) - _dy * (_xStart - 3.0f * right_hex_f.x));

    // Left hand biased
    if (dist_left <= dist_right) {
        hex = left_hex;
        return _dirLeft;
    }
    else {
        hex = right_hex;
        return _dirRight;
    }
}

void LineTracer::GetNextSquare(mpos& pos)
{
    FO_STACK_TRACE_ENTRY();

    _x += _dx;
    _y += _dy;

    pos = _mapSize.clamp_pos(iround<int32>(std::floor(_x)), iround<int32>(std::floor(_y)));
}

FO_END_NAMESPACE();
