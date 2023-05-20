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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "GeometryHelper.h"

GeometryHelper::GeometryHelper(GeometrySettings& settings) :
    _settings {settings}
{
    STACK_TRACE_ENTRY();
}

GeometryHelper::~GeometryHelper()
{
    STACK_TRACE_ENTRY();

    if (_sxEven != nullptr) {
        if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
            delete[] _sxEven;
            delete[] _syEven;
            delete[] _sxOdd;
            delete[] _syOdd;
        }
        else {
            delete[] _sxEven;
            delete[] _syEven;
        }
    }
}

void GeometryHelper::InitializeHexOffsets() const
{
    STACK_TRACE_ENTRY();

    const auto size = (MAX_HEX_OFFSET * MAX_HEX_OFFSET / 2 + MAX_HEX_OFFSET / 2) * GameSettings::MAP_DIR_COUNT;

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        _sxEven = new int16[size];
        _syEven = new int16[size];
        _sxOdd = new int16[size];
        _syOdd = new int16[size];

        auto pos = 0;
        auto xe = 0;
        auto ye = 0;
        auto xo = 1;
        auto yo = 0;

        for (auto i = 0; i < MAX_HEX_OFFSET; i++) {
            MoveHexByDirUnsafe(xe, ye, 0u);
            MoveHexByDirUnsafe(xo, yo, 0u);

            for (auto j = 0; j < 6; j++) {
                const uint8 dir = (j + 2) % 6;

                for (auto k = 0; k < i + 1; k++) {
                    _sxEven[pos] = static_cast<int16>(xe);
                    _syEven[pos] = static_cast<int16>(ye);
                    _sxOdd[pos] = static_cast<int16>(xo - 1);
                    _syOdd[pos] = static_cast<int16>(yo);

                    pos++;

                    MoveHexByDirUnsafe(xe, ye, dir);
                    MoveHexByDirUnsafe(xo, yo, dir);
                }
            }
        }
    }
    else {
        _sxEven = _sxOdd = new int16[size];
        _syEven = _syOdd = new int16[size];

        auto pos = 0;
        auto hx = 0;
        auto hy = 0;

        for (auto i = 0; i < MAX_HEX_OFFSET; i++) {
            MoveHexByDirUnsafe(hx, hy, 0u);

            for (auto j = 0; j < 5; j++) {
                uint8 dir;
                int steps;

                switch (j) {
                case 0:
                    dir = 2;
                    steps = i + 1;
                    break;
                case 1:
                    dir = 4;
                    steps = (i + 1) * 2;
                    break;
                case 2:
                    dir = 6;
                    steps = (i + 1) * 2;
                    break;
                case 3:
                    dir = 0;
                    steps = (i + 1) * 2;
                    break;
                case 4:
                    dir = 2;
                    steps = i + 1;
                    break;
                default:
                    throw UnreachablePlaceException(LINE_STR);
                }

                for (auto k = 0; k < steps; k++) {
                    _sxEven[pos] = static_cast<int16>(hx);
                    _syEven[pos] = static_cast<int16>(hy);

                    pos++;

                    MoveHexByDirUnsafe(hx, hy, dir);
                }
            }
        }
    }
}

auto GeometryHelper::DistGame(int x1, int y1, int x2, int y2) -> uint
{
    STACK_TRACE_ENTRY();

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        const auto dx = x1 > x2 ? x1 - x2 : x2 - x1;

        if ((x1 & 1) == 0) {
            if (y2 <= y1) {
                const auto rx = y1 - y2 - dx / 2;
                return dx + (rx > 0 ? rx : 0);
            }

            const auto rx = y2 - y1 - (dx + 1) / 2;
            return dx + (rx > 0 ? rx : 0);
        }

        if (y2 >= y1) {
            const auto rx = y2 - y1 - dx / 2;
            return dx + (rx > 0 ? rx : 0);
        }

        const auto rx = y1 - y2 - (dx + 1) / 2;
        return dx + (rx > 0 ? rx : 0);
    }
    else {
        const auto dx = std::abs(x2 - x1);
        const auto dy = std::abs(y2 - y1);
        return std::max(dx, dy);
    }
}

auto GeometryHelper::GetNearDir(int x1, int y1, int x2, int y2) -> uint8
{
    STACK_TRACE_ENTRY();

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        if ((x1 & 1) != 0) {
            if (x1 > x2 && y1 > y2) {
                return 0;
            }
            else if (x1 > x2 && y1 == y2) {
                return 1;
            }
            else if (x1 == x2 && y1 < y2) {
                return 2;
            }
            else if (x1 < x2 && y1 == y2) {
                return 3;
            }
            else if (x1 < x2 && y1 > y2) {
                return 4;
            }
            else if (x1 == x2 && y1 > y2) {
                return 5;
            }
        }
        else {
            if (x1 > x2 && y1 == y2) {
                return 0;
            }
            else if (x1 > x2 && y1 < y2) {
                return 1;
            }
            else if (x1 == x2 && y1 < y2) {
                return 2;
            }
            else if (x1 < x2 && y1 < y2) {
                return 3;
            }
            else if (x1 < x2 && y1 == y2) {
                return 4;
            }
            else if (x1 == x2 && y1 > y2) {
                return 5;
            }
        }
    }
    else {
        if (x1 > x2 && y1 == y2) {
            return 0;
        }
        else if (x1 > x2 && y1 < y2) {
            return 1;
        }
        else if (x1 == x2 && y1 < y2) {
            return 2;
        }
        else if (x1 < x2 && y1 < y2) {
            return 3;
        }
        else if (x1 < x2 && y1 == y2) {
            return 4;
        }
        else if (x1 < x2 && y1 > y2) {
            return 5;
        }
        else if (x1 == x2 && y1 > y2) {
            return 6;
        }
        else if (x1 > x2 && y1 > y2) {
            return 7;
        }
    }
    return 0;
}

auto GeometryHelper::GetFarDir(int x1, int y1, int x2, int y2) -> uint8
{
    STACK_TRACE_ENTRY();

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        const auto hx = static_cast<float>(x1);
        const auto hy = static_cast<float>(y1);
        const auto tx = static_cast<float>(x2);
        const auto ty = static_cast<float>(y2);
        const auto nx = 3 * (tx - hx);
        const auto ny = (ty - hy) * SQRT3_X2_FLOAT - (static_cast<float>(x2 & 1) - static_cast<float>(x1 & 1)) * SQRT3_FLOAT;

        const auto dir = 180.0f + RAD_TO_DEG_FLOAT * atan2f(ny, nx);

        if (dir >= 60.0f && dir < 120.0f) {
            return 5;
        }
        if (dir >= 120.0f && dir < 180.0f) {
            return 4;
        }
        if (dir >= 180.0f && dir < 240.0f) {
            return 3;
        }
        if (dir >= 240.0f && dir < 300.0f) {
            return 2;
        }
        if (dir >= 300.0f) {
            return 1;
        }

        return 0;
    }
    else {
        const auto dir = 180.0f + RAD_TO_DEG_FLOAT * atan2(static_cast<float>(x2 - x1), static_cast<float>(y2 - y1));

        if (dir >= 22.5f && dir < 67.5f) {
            return 7;
        }
        if (dir >= 67.5f && dir < 112.5f) {
            return 0;
        }
        if (dir >= 112.5f && dir < 157.5f) {
            return 1;
        }
        if (dir >= 157.5f && dir < 202.5f) {
            return 2;
        }
        if (dir >= 202.5f && dir < 247.5f) {
            return 3;
        }
        if (dir >= 247.5f && dir < 292.5f) {
            return 4;
        }
        if (dir >= 292.5f && dir < 337.5f) {
            return 5;
        }

        return 6;
    }
}

auto GeometryHelper::GetFarDir(int x1, int y1, int x2, int y2, float offset) -> uint8
{
    STACK_TRACE_ENTRY();

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        const auto hx = static_cast<float>(x1);
        const auto hy = static_cast<float>(y1);
        const auto tx = static_cast<float>(x2);
        const auto ty = static_cast<float>(y2);
        const auto nx = 3 * (tx - hx);
        const auto ny = (ty - hy) * SQRT3_X2_FLOAT - (static_cast<float>(x2 & 1) - static_cast<float>(x1 & 1)) * SQRT3_FLOAT;
        auto dir = 180.0f + RAD_TO_DEG_FLOAT * atan2f(ny, nx) + offset;

        if (dir < 0.0f) {
            dir = 360.0f - fmod(-dir, 360.0f);
        }
        else if (dir >= 360.0f) {
            dir = fmod(dir, 360.0f);
        }

        if (dir >= 60.0f && dir < 120.0f) {
            return 5;
        }
        if (dir >= 120.0f && dir < 180.0f) {
            return 4;
        }
        if (dir >= 180.0f && dir < 240.0f) {
            return 3;
        }
        if (dir >= 240.0f && dir < 300.0f) {
            return 2;
        }
        if (dir >= 300.0f) {
            return 1;
        }

        return 0;
    }
    else {
        auto dir = 180.0f + RAD_TO_DEG_FLOAT * atan2(static_cast<float>(x2 - x1), static_cast<float>(y2 - y1)) + offset;

        if (dir < 0.0f) {
            dir = 360.0f - fmod(-dir, 360.0f);
        }
        else if (dir >= 360.0f) {
            dir = fmod(dir, 360.0f);
        }

        if (dir >= 22.5f && dir < 67.5f) {
            return 7;
        }
        if (dir >= 67.5f && dir < 112.5f) {
            return 0;
        }
        if (dir >= 112.5f && dir < 157.5f) {
            return 1;
        }
        if (dir >= 157.5f && dir < 202.5f) {
            return 2;
        }
        if (dir >= 202.5f && dir < 247.5f) {
            return 3;
        }
        if (dir >= 247.5f && dir < 292.5f) {
            return 4;
        }
        if (dir >= 292.5f && dir < 337.5f) {
            return 5;
        }

        return 6;
    }
}

auto GeometryHelper::GetDirAngle(int x1, int y1, int x2, int y2) -> float
{
    STACK_TRACE_ENTRY();

    const auto hx = static_cast<float>(x1);
    const auto hy = static_cast<float>(y1);
    const auto tx = static_cast<float>(x2);
    const auto ty = static_cast<float>(y2);
    const auto nx = 3 * (tx - hx);
    const auto ny = (ty - hy) * SQRT3_X2_FLOAT - (static_cast<float>(x2 & 1) - static_cast<float>(x1 & 1)) * SQRT3_FLOAT;

    float r = 180.0f + RAD_TO_DEG_FLOAT * std::atan2(ny, nx);
    RUNTIME_ASSERT(r >= 0.0f);
    RUNTIME_ASSERT(r <= 360.0f);

    r = -r + 60.0f;
    if (r < 0.0f) {
        r += 360.0f;
    }
    if (r >= 360.0f) {
        r -= 360.0f;
    }

    RUNTIME_ASSERT(r >= 0.0f);
    RUNTIME_ASSERT(r < 360.0f);
    return r;
}

auto GeometryHelper::GetDirAngleDiff(float a1, float a2) -> float
{
    STACK_TRACE_ENTRY();

    const auto r = 180.0f - std::abs(std::abs(a1 - a2) - 180.0f);
    RUNTIME_ASSERT(r >= 0.0f);
    RUNTIME_ASSERT(r <= 180.0f);
    return r;
}

auto GeometryHelper::GetDirAngleDiffSided(float a1, float a2) -> float
{
    STACK_TRACE_ENTRY();

    const auto a1_r = a1 * DEG_TO_RAD_FLOAT;
    const auto a2_r = a2 * DEG_TO_RAD_FLOAT;
    const auto r = std::atan2(std::sin(a2_r - a1_r), std::cos(a2_r - a1_r)) * RAD_TO_DEG_FLOAT;
    RUNTIME_ASSERT(r >= -180.0f);
    RUNTIME_ASSERT(r <= 180.0f);
    return r;
}

auto GeometryHelper::DirToAngle(uint8 dir) -> int16
{
    STACK_TRACE_ENTRY();

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        return static_cast<int16>(dir * 60 + 30);
    }
    else {
        return static_cast<int16>(dir * 45 + 45);
    }
}

auto GeometryHelper::AngleToDir(int16 dir_angle) -> uint8
{
    STACK_TRACE_ENTRY();

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        return static_cast<uint8>(NormalizeAngle(dir_angle) / 60);
    }
    else {
        return static_cast<uint8>(NormalizeAngle(static_cast<int16>(dir_angle - 45 / 2)) / 45);
    }
}

auto GeometryHelper::NormalizeAngle(int16 dir_angle) -> int16
{
    STACK_TRACE_ENTRY();

    while (dir_angle < 0) {
        dir_angle += 360;
    }
    return static_cast<int16>(dir_angle % 360);
}

auto GeometryHelper::CheckDist(uint16 x1, uint16 y1, uint16 x2, uint16 y2, uint dist) -> bool
{
    STACK_TRACE_ENTRY();

    return DistGame(x1, y1, x2, y2) <= dist;
}

auto GeometryHelper::ReverseDir(uint8 dir) -> uint8
{
    STACK_TRACE_ENTRY();

    return static_cast<uint8>((dir + GameSettings::MAP_DIR_COUNT / 2) % GameSettings::MAP_DIR_COUNT);
}

auto GeometryHelper::MoveHexByDir(uint16& hx, uint16& hy, uint8 dir, uint16 maxhx, uint16 maxhy) -> bool
{
    STACK_TRACE_ENTRY();

    int hx_ = hx;
    int hy_ = hy;

    if (MoveHexByDirUnsafe(hx_, hy_, dir, maxhx, maxhy)) {
        hx = static_cast<uint16>(hx_);
        hy = static_cast<uint16>(hy_);
        return true;
    }
    return false;
}

auto GeometryHelper::MoveHexByDirUnsafe(int& hx, int& hy, uint8 dir, uint16 maxhx, uint16 maxhy) -> bool
{
    STACK_TRACE_ENTRY();

    MoveHexByDirUnsafe(hx, hy, dir);
    return hx >= 0 && hx < maxhx && hy >= 0 && hy < maxhy;
}

void GeometryHelper::MoveHexByDirUnsafe(int& hx, int& hy, uint8 dir)
{
    STACK_TRACE_ENTRY();

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        switch (dir) {
        case 0:
            hx--;
            if ((hx % 2) == 0) {
                hy--;
            }
            break;
        case 1:
            hx--;
            if ((hx % 2) != 0) {
                hy++;
            }
            break;
        case 2:
            hy++;
            break;
        case 3:
            hx++;
            if ((hx % 2) != 0) {
                hy++;
            }
            break;
        case 4:
            hx++;
            if ((hx % 2) == 0) {
                hy--;
            }
            break;
        case 5:
            hy--;
            break;
        default:
            break;
        }
    }
    else {
        switch (dir) {
        case 0:
            hx--;
            break;
        case 1:
            hx--;
            hy++;
            break;
        case 2:
            hy++;
            break;
        case 3:
            hx++;
            hy++;
            break;
        case 4:
            hx++;
            break;
        case 5:
            hx++;
            hy--;
            break;
        case 6:
            hy--;
            break;
        case 7:
            hx--;
            hy--;
            break;
        default:
            break;
        }
    }
}

auto GeometryHelper::GetYProj() const -> float
{
    STACK_TRACE_ENTRY();

    return 1.0f / std::sin(_settings.MapCameraAngle * DEG_TO_RAD_FLOAT);
}

auto GeometryHelper::GetLineDirAngle(int x1, int y1, int x2, int y2) const -> float
{
    STACK_TRACE_ENTRY();

    const auto x1_f = static_cast<float>(x1);
    const auto y1_f = static_cast<float>(y1) * GetYProj();
    const auto x2_f = static_cast<float>(x2);
    const auto y2_f = static_cast<float>(y2) * GetYProj();

    auto angle = 90.0f + RAD_TO_DEG_FLOAT * std::atan2(y2_f - y1_f, x2_f - x1_f);
    if (angle < 0.0f) {
        angle += 360.0f;
    }
    if (angle >= 360.0f) {
        angle -= 360.0f;
    }

    RUNTIME_ASSERT(angle >= 0.0f);
    RUNTIME_ASSERT(angle < 360.0f);
    return angle;
}

auto GeometryHelper::GetHexOffsets(bool odd) const -> tuple<const int16*, const int16*>
{
    STACK_TRACE_ENTRY();

    if (_sxEven == nullptr) {
        InitializeHexOffsets();
    }

    const auto* sx = (odd ? _sxOdd : _sxEven);
    const auto* sy = (odd ? _syOdd : _syEven);
    return {sx, sy};
}

auto GeometryHelper::GetHexInterval(int from_hx, int from_hy, int to_hx, int to_hy) const -> tuple<int, int>
{
    STACK_TRACE_ENTRY();

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        auto dx = to_hx - from_hx;
        const auto dy = to_hy - from_hy;

        auto x = dy * (_settings.MapHexWidth / 2) - dx * _settings.MapHexWidth;
        auto y = dy * _settings.MapHexLineHeight;

        if ((from_hx & 1) != 0) {
            if (dx > 0) {
                dx++;
            }
        }
        else if (dx < 0) {
            dx--;
        }

        dx /= 2;

        x += _settings.MapHexWidth / 2 * dx;
        y += _settings.MapHexLineHeight * dx;
        return {x, y};
    }
    else {
        const auto dx = to_hx - from_hx;
        const auto dy = to_hy - from_hy;

        auto x = (dy - dx) * _settings.MapHexWidth / 2;
        auto y = (dy + dx) * _settings.MapHexLineHeight;
        return {x, y};
    }
}

void GeometryHelper::ForEachBlockLines(const vector<uint8>& lines, uint16 hx, uint16 hy, uint16 maxhx, uint16 maxhy, const std::function<void(uint16, uint16)>& work)
{
    STACK_TRACE_ENTRY();

    int hx_ = hx;
    int hy_ = hy;

    for (size_t i = 0; i < lines.size() / 2; i++) {
        const auto dir = lines[i * 2];
        const auto steps = lines[i * 2 + 1];

        if (dir >= GameSettings::MAP_DIR_COUNT || steps == 0u || steps > 9u) {
            continue;
        }

        for (uint8 k = 0; k < steps; k++) {
            if (MoveHexByDirUnsafe(hx_, hy_, dir, maxhx, maxhy)) {
                work(static_cast<uint16>(hx_), static_cast<uint16>(hy_));
            }
        }
    }
}
