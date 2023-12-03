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
// Copyright (c) 2006 - 2023, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "GeometryHelper.h"

constexpr auto BIAS_FLOAT = 0.02f;

LineTracer::LineTracer(uint16 hx, uint16 hy, uint16 tx, uint16 ty, uint16 maxhx, uint16 maxhy, float angle)
{
    STACK_TRACE_ENTRY();

    _maxHx = maxhx;
    _maxHy = maxhy;

    if constexpr (GameSettings::SQUARE_GEOMETRY) {
        _dir = std::atan2(static_cast<float>(ty - hy), static_cast<float>(tx - hx)) + angle;
        _dx = std::cos(_dir);
        _dy = std::sin(_dir);

        if (std::fabs(_dx) > std::fabs(_dy)) {
            _dy /= std::fabs(_dx);
            _dx = _dx > 0 ? 1.0f : -1.0f;
        }
        else {
            _dx /= std::fabs(_dy);
            _dy = _dy > 0 ? 1.0f : -1.0f;
        }

        _x1 = static_cast<float>(hx) + 0.5f;
        _y1 = static_cast<float>(hy) + 0.5f;
    }
    else {
        const auto nx = 3.0f * (static_cast<float>(tx) - static_cast<float>(hx));
        const auto ny = (static_cast<float>(ty) - static_cast<float>(hy)) * SQRT3_X2_FLOAT - (static_cast<float>(std::abs(tx % 2)) - static_cast<float>(std::abs(hx % 2))) * SQRT3_FLOAT;

        _dir = 180.0f + RAD_TO_DEG_FLOAT * std::atan2(ny, nx);

        if (angle != 0.0f) {
            _dir += angle;
            NormalizeDir();
        }

        if (_dir >= 30.0f && _dir < 90.0f) {
            _dir1 = 5;
            _dir2 = 0;
        }
        else if (_dir >= 90.0f && _dir < 150.0f) {
            _dir1 = 4;
            _dir2 = 5;
        }
        else if (_dir >= 150.0f && _dir < 210.0f) {
            _dir1 = 3;
            _dir2 = 4;
        }
        else if (_dir >= 210.0f && _dir < 270.0f) {
            _dir1 = 2;
            _dir2 = 3;
        }
        else if (_dir >= 270.0f && _dir < 330.0f) {
            _dir1 = 1;
            _dir2 = 2;
        }
        else {
            _dir1 = 0;
            _dir2 = 1;
        }

        _x1 = 3.0f * static_cast<float>(hx) + BIAS_FLOAT;
        _y1 = SQRT3_X2_FLOAT * static_cast<float>(hy) - SQRT3_FLOAT * static_cast<float>(std::abs(hx % 2)) + BIAS_FLOAT;
        _x2 = 3.0f * static_cast<float>(tx) + BIAS_FLOAT + BIAS_FLOAT;
        _y2 = SQRT3_X2_FLOAT * static_cast<float>(ty) - SQRT3_FLOAT * static_cast<float>(std::abs(tx % 2)) + BIAS_FLOAT;

        if (angle != 0.0f) {
            _x2 -= _x1;
            _y2 -= _y1;

            const auto xp = std::cos(angle / RAD_TO_DEG_FLOAT) * _x2 - std::sin(angle / RAD_TO_DEG_FLOAT) * _y2;
            const auto yp = std::sin(angle / RAD_TO_DEG_FLOAT) * _x2 + std::cos(angle / RAD_TO_DEG_FLOAT) * _y2;

            _x2 = _x1 + xp;
            _y2 = _y1 + yp;
        }

        _dx = _x2 - _x1;
        _dy = _y2 - _y1;
    }
}

auto LineTracer::GetNextHex(uint16& cx, uint16& cy) const -> uint8
{
    STACK_TRACE_ENTRY();

    auto t1_x = cx;
    auto t2_x = cx;
    auto t1_y = cy;
    auto t2_y = cy;

    GeometryHelper::MoveHexByDir(t1_x, t1_y, _dir1, _maxHx, _maxHy);
    GeometryHelper::MoveHexByDir(t2_x, t2_y, _dir2, _maxHx, _maxHy);

    auto dist1 = _dx * (_y1 - (SQRT3_X2_FLOAT * static_cast<float>(t1_y) - static_cast<float>(std::abs(t1_x % 2)) * SQRT3_FLOAT)) - _dy * (_x1 - 3 * static_cast<float>(t1_x));
    auto dist2 = _dx * (_y1 - (SQRT3_X2_FLOAT * static_cast<float>(t2_y) - static_cast<float>(std::abs(t2_x % 2)) * SQRT3_FLOAT)) - _dy * (_x1 - 3 * static_cast<float>(t2_x));

    dist1 = dist1 > 0 ? dist1 : -dist1;
    dist2 = dist2 > 0 ? dist2 : -dist2;

    // Left hand biased
    if (dist1 <= dist2) {
        cx = t1_x;
        cy = t1_y;
        return _dir1;
    }

    cx = t2_x;
    cy = t2_y;
    return _dir2;
}

void LineTracer::GetNextSquare(uint16& cx, uint16& cy)
{
    STACK_TRACE_ENTRY();

    _x1 += _dx;
    _y1 += _dy;

    cx = static_cast<uint16>(floor(_x1));
    cy = static_cast<uint16>(floor(_y1));

    if (cx >= _maxHx) {
        cx = _maxHx - 1;
    }
    if (cy >= _maxHy) {
        cy = _maxHy - 1;
    }
}

void LineTracer::NormalizeDir()
{
    STACK_TRACE_ENTRY();

    if (_dir <= 0.0f) {
        _dir = 360.0f - std::fmod(-_dir, 360.0f);
    }
    else if (_dir >= 0.0f) {
        _dir = std::fmod(_dir, 360.0f);
    }
}
