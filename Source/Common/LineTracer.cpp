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

#include "LineTracer.h"

FO_BEGIN_NAMESPACE

LineTracer::LineTracer(mpos start_hex, mpos target_hex, float32_t dir_angle_offset, msize map_size, ipos16 start_offset, ipos16 target_offset)
{
    _mapSize = map_size;

    TraceInit(start_hex, target_hex, dir_angle_offset, start_offset, target_offset);
}

LineTracer::LineTracer(mpos start_hex, float32_t dir_angle, int32_t dist, msize map_size, ipos16 start_offset, ipos16 target_offset)
{
    FO_VERIFY_AND_THROW(dist >= 0, "Dist is negative");

    _mapSize = map_size;

    // The ray is cast from a hex at the map origin with the start's column parity, whose pixel lattice is the start's
    // moved by whole cells, so float rounding does not depend on where the start is
    ipos32 local_start {GameSettings::HEXAGONAL_GEOMETRY ? start_hex.x % 2 : 0, 0};
    ipos32 shift {start_hex.x - local_start.x, start_hex.y - local_start.y};
    ipos32 start_pos {GeometryHelper::GetHexPos(local_start)};
    float64_t start_pos_x {numeric_cast<float64_t>(start_pos.x + start_offset.x)};
    float64_t start_pos_y {numeric_cast<float64_t>(start_pos.y + start_offset.y)};
    float64_t angle_rad {(numeric_cast<float64_t>(dir_angle) - 90.0) * DEG_TO_RAD_FLOAT64};
    float64_t ray_dx {std::cos(angle_rad)};
    float64_t ray_dy {std::sin(angle_rad) / numeric_cast<float64_t>(GeometryHelper::GetYProj())};
    float64_t pixel_dist {numeric_cast<float64_t>(std::max(GameSettings::MAP_HEX_WIDTH, GameSettings::MAP_HEX_HEIGHT)) * numeric_cast<float64_t>(dist)};
    ipos32 target_pos {ipos32 {iround<int32_t>(start_pos_x + ray_dx * pixel_dist), iround<int32_t>(start_pos_y + ray_dy * pixel_dist)}};
    ipos32 raw_target_hex {GeometryHelper::GetHexPosCoord(target_pos)};
    mpos target_hex {mpos {numeric_cast<mcoord_t>(checked_add(raw_target_hex.x, shift.x)), numeric_cast<mcoord_t>(checked_add(raw_target_hex.y, shift.y))}};

    TraceInit(start_hex, target_hex, 0.0f, start_offset, target_offset);
}

void LineTracer::TraceInit(mpos start_hex, mpos target_hex, float32_t dir_angle_offset, ipos16 start_offset, ipos16 target_offset)
{
    // The start hex is the origin, so the target and every traced hex are offsets from it
    _origin = start_hex;
    _step = 0;

    float64_t tx {numeric_cast<float64_t>(target_hex.x - _origin.x)};
    float64_t ty {numeric_cast<float64_t>(target_hex.y - _origin.y)};
    float64_t angle_offset {numeric_cast<float64_t>(dir_angle_offset)};

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        constexpr float64_t bias {0.02};

        // Trace coordinates use a hex box 4 wide (vertex-to-vertex) by SQRT3_X2 tall, while the visual one
        // is MAP_HEX_WIDTH x MAP_HEX_HEIGHT
        constexpr float64_t offset_scale_x {4.0 / numeric_cast<float64_t>(GameSettings::MAP_HEX_WIDTH)};
        constexpr float64_t offset_scale_y {SQRT3_X2_FLOAT64 / numeric_cast<float64_t>(GameSettings::MAP_HEX_HEIGHT)};

        float64_t so_x {numeric_cast<float64_t>(start_offset.x) * offset_scale_x};
        float64_t so_y {numeric_cast<float64_t>(start_offset.y) * offset_scale_y};
        float64_t to_x {numeric_cast<float64_t>(target_offset.x) * offset_scale_x};
        float64_t to_y {numeric_cast<float64_t>(target_offset.y) * offset_scale_y};

        // Column parity shifts a hex by half a row, counted relative to the start column like GetLineDistance does
        float64_t parity {numeric_cast<float64_t>(std::abs(target_hex.x % 2) - std::abs(start_hex.x % 2))};

        _xStart = bias + so_x;
        _yStart = bias + so_y;

        float64_t x_end {tx * 3.0 + bias + to_x};
        float64_t y_end {ty * SQRT3_X2_FLOAT64 - parity * SQRT3_FLOAT64 + bias + to_y};

        if (!is_float_equal(dir_angle_offset, 0.0f)) {
            x_end -= _xStart;
            y_end -= _yStart;

            float64_t angle_offset_rad {-angle_offset * DEG_TO_RAD_FLOAT64};
            float64_t ox {std::cos(angle_offset_rad) * x_end - std::sin(angle_offset_rad) * y_end};
            float64_t oy {std::sin(angle_offset_rad) * x_end + std::cos(angle_offset_rad) * y_end};

            x_end = _xStart + ox;
            y_end = _yStart + oy;
        }

        _dx = x_end - _xStart;
        _dy = y_end - _yStart;

        float64_t nx {tx * 3.0 + (to_x - so_x) + bias};
        float64_t ny {ty * SQRT3_X2_FLOAT64 - parity * SQRT3_FLOAT64 + (to_y - so_y) + bias};
        float64_t angle {wrap_angle(180.0 + std::atan2(ny, nx) * RAD_TO_DEG_FLOAT64 - angle_offset)};
        float64_t dir_angle {360.0 - wrap_angle(-60.0 + angle)};

        if (dir_angle >= 30.0 && dir_angle < 90.0) {
            _dirLeft = hdir::NorthEast;
            _dirRight = hdir::East;
        }
        else if (dir_angle >= 90.0 && dir_angle < 150.0) {
            _dirLeft = hdir::East;
            _dirRight = hdir::SouthEast;
        }
        else if (dir_angle >= 150.0 && dir_angle < 210.0) {
            _dirLeft = hdir::SouthEast;
            _dirRight = hdir::SouthWest;
        }
        else if (dir_angle >= 210.0 && dir_angle < 270.0) {
            _dirLeft = hdir::SouthWest;
            _dirRight = hdir::West;
        }
        else if (dir_angle >= 270.0 && dir_angle < 330.0) {
            _dirLeft = hdir::West;
            _dirRight = hdir::NorthWest;
        }
        else {
            _dirLeft = hdir::NorthWest;
            _dirRight = hdir::NorthEast;
        }
    }
    else {
        // Square geometry: internal hex size is 1x1, visual is MAP_HEX_WIDTH x MAP_HEX_HEIGHT
        constexpr float64_t offset_scale_x {1.0 / numeric_cast<float64_t>(GameSettings::MAP_HEX_WIDTH)};
        constexpr float64_t offset_scale_y {1.0 / numeric_cast<float64_t>(GameSettings::MAP_HEX_HEIGHT)};

        float64_t so_x {numeric_cast<float64_t>(start_offset.x) * offset_scale_x};
        float64_t so_y {numeric_cast<float64_t>(start_offset.y) * offset_scale_y};
        float64_t to_x {numeric_cast<float64_t>(target_offset.x) * offset_scale_x};
        float64_t to_y {numeric_cast<float64_t>(target_offset.y) * offset_scale_y};

        float64_t nx {tx + (to_x - so_x)};
        float64_t ny {ty + (to_y - so_y)};
        float64_t angle {wrap_angle(std::atan2(ny, nx) * RAD_TO_DEG_FLOAT64)};

        _xStart = 0.5 + so_x;
        _yStart = 0.5 + so_y;
        _dx = std::cos(angle * DEG_TO_RAD_FLOAT64);
        _dy = std::sin(angle * DEG_TO_RAD_FLOAT64);

        // One coordinate advances by a whole cell per step
        if (std::abs(_dx) > std::abs(_dy)) {
            _dy /= std::abs(_dx);
            _dx = _dx > 0.0 ? 1.0 : -1.0;
        }
        else {
            _dx /= std::abs(_dy);
            _dy = _dy > 0.0 ? 1.0 : -1.0;
        }
    }
}

// Distance of a hex centre from the traced line, scaled by its length; an odd column sits half a row up
auto LineTracer::GetLineDistance(mpos hex) const -> float64_t
{
    float64_t rel_x {numeric_cast<float64_t>(hex.x - _origin.x) * 3.0};
    float64_t parity {numeric_cast<float64_t>(std::abs(hex.x % 2) - std::abs(_origin.x % 2))};
    float64_t rel_y {numeric_cast<float64_t>(hex.y - _origin.y) * SQRT3_X2_FLOAT64 - parity * SQRT3_FLOAT64};

    return std::abs(_dx * (_yStart - rel_y) - _dy * (_xStart - rel_x));
}

auto LineTracer::GetNextHex(mpos& hex) -> optional<mdir>
{
    mpos cur_hex {hex};

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        mpos left_hex {hex};
        mpos right_hex {hex};

        bool left_changed {GeometryHelper::MoveHexByDir(left_hex, mdir(_dirLeft), _mapSize)};
        bool right_changed {GeometryHelper::MoveHexByDir(right_hex, mdir(_dirRight), _mapSize)};
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

        // Left hand biased
        if (GetLineDistance(left_hex) <= GetLineDistance(right_hex)) {
            hex = left_hex;
            return mdir(_dirLeft);
        }
        else {
            hex = right_hex;
            return mdir(_dirRight);
        }
    }
    else {
        // The position follows from the step count, so no rounding accumulates along the line
        _step++;

        float64_t step {numeric_cast<float64_t>(_step)};
        float64_t x {_xStart + _dx * step};
        float64_t y {_yStart + _dy * step};
        mpos next_hex {_mapSize.clamp_pos(_origin.x + iround<int32_t>(std::floor(x)), _origin.y + iround<int32_t>(std::floor(y)))};

        if (next_hex == cur_hex) {
            return std::nullopt;
        }

        hex = next_hex;
        return mdir(GeometryHelper::GetHexDir(cur_hex, next_hex));
    }
}

FO_END_NAMESPACE
