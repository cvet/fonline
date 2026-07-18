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

#include "LineTracer.h"

FO_BEGIN_NAMESPACE

LineTracer::LineTracer(mpos start_hex, mpos target_hex, float32_t dir_angle_offset, msize map_size, ipos16 start_offset, ipos16 target_offset)
{
    FO_STACK_TRACE_ENTRY();

    _mapSize = map_size;

    TraceInit(start_hex, target_hex, dir_angle_offset, start_offset, target_offset);
}

LineTracer::LineTracer(mpos start_hex, float32_t dir_angle, int32_t dist, msize map_size, ipos16 start_offset, ipos16 target_offset)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(dist >= 0, "Dist is negative");

    _mapSize = map_size;

    ipos32 start_pos = GeometryHelper::GetHexPos(start_hex);
    float32_t start_pos_x = numeric_cast<float32_t>(start_pos.x + start_offset.x);
    float32_t start_pos_y = numeric_cast<float32_t>(start_pos.y + start_offset.y);
    float32_t angle_rad = (dir_angle - 90.0f) * DEG_TO_RAD_FLOAT;
    float32_t ray_dx = std::cos(angle_rad);
    float32_t ray_dy = std::sin(angle_rad) / GeometryHelper::GetYProj();
    float32_t pixel_dist = numeric_cast<float32_t>(std::max(GameSettings::MAP_HEX_WIDTH, GameSettings::MAP_HEX_HEIGHT) * dist);
    ipos32 target_pos = ipos32 {iround<int32_t>(start_pos_x + ray_dx * pixel_dist), iround<int32_t>(start_pos_y + ray_dy * pixel_dist)};
    ipos32 raw_target_hex = GeometryHelper::GetHexPosCoord(target_pos);
    mpos target_hex = mpos {numeric_cast<int16_t>(raw_target_hex.x), numeric_cast<int16_t>(raw_target_hex.y)};

    TraceInit(start_hex, target_hex, 0.0f, start_offset, target_offset);
}

void LineTracer::TraceInit(mpos start_hex, mpos target_hex, float32_t dir_angle_offset, ipos16 start_offset, ipos16 target_offset)
{
    FO_STACK_TRACE_ENTRY();

    float32_t sx = numeric_cast<float32_t>(start_hex.x);
    float32_t sy = numeric_cast<float32_t>(start_hex.y);
    float32_t tx = numeric_cast<float32_t>(target_hex.x);
    float32_t ty = numeric_cast<float32_t>(target_hex.y);

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        constexpr float32_t bias = 0.02f;

        // Convert pixel sub-hex offset to internal trace coordinates.
        // Internal hex bounding box is 4 wide (vertex-to-vertex) and SQRT3_X2 tall (edge-to-edge),
        // visual hex bounding box is MAP_HEX_WIDTH x MAP_HEX_HEIGHT.
        constexpr float32_t offset_scale_x = 4.0f / numeric_cast<float32_t>(GameSettings::MAP_HEX_WIDTH);
        constexpr float32_t offset_scale_y = SQRT3_X2_FLOAT / numeric_cast<float32_t>(GameSettings::MAP_HEX_HEIGHT);

        float32_t so_x = numeric_cast<float32_t>(start_offset.x) * offset_scale_x;
        float32_t so_y = numeric_cast<float32_t>(start_offset.y) * offset_scale_y;
        float32_t to_x = numeric_cast<float32_t>(target_offset.x) * offset_scale_x;
        float32_t to_y = numeric_cast<float32_t>(target_offset.y) * offset_scale_y;

        float32_t sx_odd = numeric_cast<float32_t>(std::abs(start_hex.x % 2));
        float32_t tx_odd = numeric_cast<float32_t>(std::abs(target_hex.x % 2));

        _xStart = sx * 3.0f + bias + so_x;
        _yStart = sy * SQRT3_X2_FLOAT - sx_odd * SQRT3_FLOAT + bias + so_y;

        float32_t x_end = tx * 3.0f + bias + to_x;
        float32_t y_end = ty * SQRT3_X2_FLOAT - tx_odd * SQRT3_FLOAT + bias + to_y;

        if (!is_float_equal(dir_angle_offset, 0.0f)) {
            x_end -= _xStart;
            y_end -= _yStart;

            float32_t angle_offset_rad = -dir_angle_offset * DEG_TO_RAD_FLOAT;
            float32_t ox = std::cos(angle_offset_rad) * x_end - std::sin(angle_offset_rad) * y_end;
            float32_t oy = std::sin(angle_offset_rad) * x_end + std::cos(angle_offset_rad) * y_end;

            x_end = _xStart + ox;
            y_end = _yStart + oy;
        }

        _dx = x_end - _xStart;
        _dy = y_end - _yStart;

        float32_t nx = (tx - sx) * 3.0f + (to_x - so_x) + bias;
        float32_t ny = (ty - sy) * SQRT3_X2_FLOAT - (tx_odd - sx_odd) * SQRT3_FLOAT + (to_y - so_y) + bias;
        float32_t angle = 180.0f + std::atan2(ny, nx) * RAD_TO_DEG_FLOAT - dir_angle_offset;
        angle = angle < 0.0f ? 360.0f - std::fmod(-angle, 360.0f) : std::fmod(angle, 360.0f);

        _dirAngle = -60.0f + angle;
        _dirAngle = _dirAngle < 0.0f ? 360.0f - std::fmod(-_dirAngle, 360.0f) : std::fmod(_dirAngle, 360.0f);
        _dirAngle = 360.0f - _dirAngle;

        if (_dirAngle >= 30.0f && _dirAngle < 90.0f) {
            _dirLeft = hdir::NorthEast;
            _dirRight = hdir::East;
        }
        else if (_dirAngle >= 90.0f && _dirAngle < 150.0f) {
            _dirLeft = hdir::East;
            _dirRight = hdir::SouthEast;
        }
        else if (_dirAngle >= 150.0f && _dirAngle < 210.0f) {
            _dirLeft = hdir::SouthEast;
            _dirRight = hdir::SouthWest;
        }
        else if (_dirAngle >= 210.0f && _dirAngle < 270.0f) {
            _dirLeft = hdir::SouthWest;
            _dirRight = hdir::West;
        }
        else if (_dirAngle >= 270.0f && _dirAngle < 330.0f) {
            _dirLeft = hdir::West;
            _dirRight = hdir::NorthWest;
        }
        else {
            _dirLeft = hdir::NorthWest;
            _dirRight = hdir::NorthEast;
        }
    }
    else {
        // Square geometry: internal hex size is 1x1, visual is MAP_HEX_WIDTH x MAP_HEX_HEIGHT.
        constexpr float32_t offset_scale_x = 1.0f / numeric_cast<float32_t>(GameSettings::MAP_HEX_WIDTH);
        constexpr float32_t offset_scale_y = 1.0f / numeric_cast<float32_t>(GameSettings::MAP_HEX_HEIGHT);

        float32_t so_x = numeric_cast<float32_t>(start_offset.x) * offset_scale_x;
        float32_t so_y = numeric_cast<float32_t>(start_offset.y) * offset_scale_y;
        float32_t to_x = numeric_cast<float32_t>(target_offset.x) * offset_scale_x;
        float32_t to_y = numeric_cast<float32_t>(target_offset.y) * offset_scale_y;

        float32_t nx = (tx - sx) + (to_x - so_x);
        float32_t ny = (ty - sy) + (to_y - so_y);
        float32_t angle = std::atan2(ny, nx) * RAD_TO_DEG_FLOAT;
        angle = angle < 0.0f ? 360.0f - std::fmod(-angle, 360.0f) : std::fmod(angle, 360.0f);

        _xStart = sx + 0.5f + so_x;
        _yStart = sy + 0.5f + so_y;

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

auto LineTracer::GetNextHex(mpos& hex) -> optional<mdir>
{
    FO_STACK_TRACE_ENTRY();

    mpos cur_hex = hex;

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        mpos left_hex = hex;
        mpos right_hex = hex;

        bool left_changed = GeometryHelper::MoveHexByDir(left_hex, mdir(_dirLeft), _mapSize);
        bool right_changed = GeometryHelper::MoveHexByDir(right_hex, mdir(_dirRight), _mapSize);
        FO_VERIFY_AND_THROW(left_changed == (left_hex != cur_hex), "Left-side line-trace move result does not match the resulting hex", cur_hex, left_hex, left_changed);
        FO_VERIFY_AND_THROW(right_changed == (right_hex != cur_hex), "Right-side line-trace move result does not match the resulting hex", cur_hex, right_hex, right_changed);

        if (!left_changed && !right_changed) {
            return std::nullopt;
        }
        if (!left_changed) {
            hex = right_hex;
            return mdir(_dirRight);
        }
        if (!right_changed) {
            hex = left_hex;
            return mdir(_dirLeft);
        }

        fpos32 left_hex_f = fpos32(left_hex);
        fpos32 right_hex_f = fpos32(right_hex);
        float32_t dist_left = std::abs(_dx * (_yStart - (left_hex_f.y * SQRT3_X2_FLOAT - numeric_cast<float32_t>(std::abs(left_hex.x % 2)) * SQRT3_FLOAT)) - _dy * (_xStart - 3.0f * left_hex_f.x));
        float32_t dist_right = std::abs(_dx * (_yStart - (right_hex_f.y * SQRT3_X2_FLOAT - numeric_cast<float32_t>(std::abs(right_hex.x % 2)) * SQRT3_FLOAT)) - _dy * (_xStart - 3.0f * right_hex_f.x));

        // Left hand biased
        if (dist_left <= dist_right) {
            hex = left_hex;
            return mdir(_dirLeft);
        }
        else {
            hex = right_hex;
            return mdir(_dirRight);
        }
    }
    else {
        _x += _dx;
        _y += _dy;

        mpos next_hex = _mapSize.clamp_pos(iround<int32_t>(std::floor(_x)), iround<int32_t>(std::floor(_y)));

        if (next_hex == cur_hex) {
            return std::nullopt;
        }

        hex = next_hex;
        return mdir(GeometryHelper::GetHexDir(cur_hex, next_hex));
    }
}

FO_END_NAMESPACE
