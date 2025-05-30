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

#include "GeometryHelper.h"

FO_BEGIN_NAMESPACE();

GeometryHelper::GeometryHelper(GeometrySettings& settings) :
    _settings {settings}
{
    FO_STACK_TRACE_ENTRY();
}

void GeometryHelper::InitializeHexOffsets() const
{
    FO_STACK_TRACE_ENTRY();

    constexpr auto size = (MAX_HEX_OFFSET * MAX_HEX_OFFSET / 2 + MAX_HEX_OFFSET / 2) * GameSettings::MAP_DIR_COUNT;

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        _sxEven = SafeAlloc::MakeUniqueArr<int16>(size);
        _syEven = SafeAlloc::MakeUniqueArr<int16>(size);
        _sxOdd = SafeAlloc::MakeUniqueArr<int16>(size);
        _syOdd = SafeAlloc::MakeUniqueArr<int16>(size);

        size_t iter = 0;
        ipos pos1 = {0, 0};
        ipos pos2 = {1, 0};

        for (auto i = 0; i < MAX_HEX_OFFSET; i++) {
            MoveHexByDirUnsafe(pos1, 0);
            MoveHexByDirUnsafe(pos2, 0);

            for (auto j = 0; j < 6; j++) {
                const uint8 dir = (j + 2) % 6;

                for (auto k = 0; k < i + 1; k++) {
                    _sxEven[iter] = numeric_cast<int16>(pos1.x);
                    _syEven[iter] = numeric_cast<int16>(pos1.y);
                    _sxOdd[iter] = numeric_cast<int16>(pos2.x - 1);
                    _syOdd[iter] = numeric_cast<int16>(pos2.y);

                    iter++;

                    MoveHexByDirUnsafe(pos1, dir);
                    MoveHexByDirUnsafe(pos2, dir);
                }
            }
        }
    }
    else {
        _sxEven = SafeAlloc::MakeUniqueArr<int16>(size);
        _syEven = SafeAlloc::MakeUniqueArr<int16>(size);
        _sxOdd = SafeAlloc::MakeUniqueArr<int16>(size);
        _syOdd = SafeAlloc::MakeUniqueArr<int16>(size);

        size_t iter = 0;
        ipos pos;

        for (auto i = 0; i < MAX_HEX_OFFSET; i++) {
            MoveHexByDirUnsafe(pos, 0);

            for (auto j = 0; j < 5; j++) {
                uint8 dir;
                int32 steps;

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
                    FO_UNREACHABLE_PLACE();
                }

                for (auto k = 0; k < steps; k++) {
                    _sxEven[iter] = numeric_cast<int16>(pos.x);
                    _syEven[iter] = numeric_cast<int16>(pos.y);

                    iter++;

                    MoveHexByDirUnsafe(pos, dir);
                }
            }
        }
    }
}

auto GeometryHelper::DistGame(int32 x1, int32 y1, int32 x2, int32 y2) -> int32
{
    FO_NO_STACK_TRACE_ENTRY();

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        const auto dx = x1 > x2 ? x1 - x2 : x2 - x1;

        if ((x1 % 2) == 0) {
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

auto GeometryHelper::DistGame(mpos hex1, mpos hex2) -> int32
{
    FO_NO_STACK_TRACE_ENTRY();

    return DistGame(hex1.x, hex1.y, hex2.x, hex2.y);
}

auto GeometryHelper::GetNearDir(int32 x1, int32 y1, int32 x2, int32 y2) -> uint8
{
    FO_NO_STACK_TRACE_ENTRY();

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        if ((x1 % 2) != 0) {
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

auto GeometryHelper::GetNearDir(mpos from_hex, mpos to_hex) -> uint8
{
    FO_NO_STACK_TRACE_ENTRY();

    return GetNearDir(from_hex.x, from_hex.y, to_hex.x, to_hex.y);
}

auto GeometryHelper::GetFarDir(int32 x1, int32 y1, int32 x2, int32 y2) -> uint8
{
    FO_NO_STACK_TRACE_ENTRY();

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        const auto hx = numeric_cast<float32>(x1);
        const auto hy = numeric_cast<float32>(y1);
        const auto tx = numeric_cast<float32>(x2);
        const auto ty = numeric_cast<float32>(y2);
        const auto nx = 3 * (tx - hx);
        const auto ny = (ty - hy) * SQRT3_X2_FLOAT - (numeric_cast<float32>(std::abs(x2 % 2)) - numeric_cast<float32>(std::abs(x1 % 2))) * SQRT3_FLOAT;
        const auto dir = 180.0f + RAD_TO_DEG_FLOAT * std::atan2(ny, nx);

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
        const auto dir = 180.0f + RAD_TO_DEG_FLOAT * std::atan2(numeric_cast<float32>(x2 - x1), numeric_cast<float32>(y2 - y1));

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

auto GeometryHelper::GetFarDir(int32 x1, int32 y1, int32 x2, int32 y2, float32 offset) -> uint8
{
    FO_NO_STACK_TRACE_ENTRY();

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        const auto hx = numeric_cast<float32>(x1);
        const auto hy = numeric_cast<float32>(y1);
        const auto tx = numeric_cast<float32>(x2);
        const auto ty = numeric_cast<float32>(y2);
        const auto nx = 3 * (tx - hx);
        const auto ny = (ty - hy) * SQRT3_X2_FLOAT - (numeric_cast<float32>(std::abs(x2 % 2)) - numeric_cast<float32>(std::abs(x1 % 2))) * SQRT3_FLOAT;
        auto dir = 180.0f + RAD_TO_DEG_FLOAT * std::atan2(ny, nx) + offset;

        if (dir < 0.0f) {
            dir = 360.0f - std::fmod(-dir, 360.0f);
        }
        else if (dir >= 360.0f) {
            dir = std::fmod(dir, 360.0f);
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
        auto dir = 180.0f + RAD_TO_DEG_FLOAT * std::atan2(numeric_cast<float32>(x2 - x1), numeric_cast<float32>(y2 - y1)) + offset;

        if (dir < 0.0f) {
            dir = 360.0f - std::fmod(-dir, 360.0f);
        }
        else if (dir >= 360.0f) {
            dir = std::fmod(dir, 360.0f);
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

auto GeometryHelper::GetFarDir(mpos from_hex, mpos to_hex) -> uint8
{
    FO_NO_STACK_TRACE_ENTRY();

    return GetFarDir(from_hex.x, from_hex.y, to_hex.x, to_hex.y);
}

auto GeometryHelper::GetFarDir(mpos from_hex, mpos to_hex, float32 offset) -> uint8
{
    FO_NO_STACK_TRACE_ENTRY();

    return GetFarDir(from_hex.x, from_hex.y, to_hex.x, to_hex.y, offset);
}

auto GeometryHelper::GetDirAngle(int32 x1, int32 y1, int32 x2, int32 y2) -> float32
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto hx = numeric_cast<float32>(x1);
    const auto hy = numeric_cast<float32>(y1);
    const auto tx = numeric_cast<float32>(x2);
    const auto ty = numeric_cast<float32>(y2);
    const auto nx = 3 * (tx - hx);
    const auto ny = (ty - hy) * SQRT3_X2_FLOAT - (numeric_cast<float32>(std::abs(x2 % 2)) - numeric_cast<float32>(std::abs(x1 % 2))) * SQRT3_FLOAT;

    float32 r = 180.0f + RAD_TO_DEG_FLOAT * std::atan2(ny, nx);
    FO_RUNTIME_ASSERT(r >= 0.0f);
    FO_RUNTIME_ASSERT(r <= 360.0f);

    r = -r + 60.0f;
    if (r < 0.0f) {
        r += 360.0f;
    }
    if (r >= 360.0f) {
        r -= 360.0f;
    }

    FO_RUNTIME_ASSERT(r >= 0.0f);
    FO_RUNTIME_ASSERT(r < 360.0f);

    return r;
}

auto GeometryHelper::GetDirAngle(mpos from_hex, mpos to_hex) -> float32
{
    FO_NO_STACK_TRACE_ENTRY();

    return GetDirAngle(from_hex.x, from_hex.y, to_hex.x, to_hex.y);
}

auto GeometryHelper::GetDirAngleDiff(float32 a1, float32 a2) -> float32
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto r = 180.0f - std::abs(std::abs(a1 - a2) - 180.0f);
    FO_RUNTIME_ASSERT(r >= 0.0f);
    FO_RUNTIME_ASSERT(r <= 180.0f);

    return r;
}

auto GeometryHelper::GetDirAngleDiffSided(float32 a1, float32 a2) -> float32
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto a1_r = a1 * DEG_TO_RAD_FLOAT;
    const auto a2_r = a2 * DEG_TO_RAD_FLOAT;
    const auto r = std::atan2(std::sin(a2_r - a1_r), std::cos(a2_r - a1_r)) * RAD_TO_DEG_FLOAT;
    FO_RUNTIME_ASSERT(r >= -180.0f);
    FO_RUNTIME_ASSERT(r <= 180.0f);

    return r;
}

auto GeometryHelper::DirToAngle(uint8 dir) -> int16
{
    FO_NO_STACK_TRACE_ENTRY();

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        return numeric_cast<int16>(dir * 60 + 30);
    }
    else {
        return numeric_cast<int16>(dir * 45 + 45);
    }
}

auto GeometryHelper::AngleToDir(int16 dir_angle) -> uint8
{
    FO_NO_STACK_TRACE_ENTRY();

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        return numeric_cast<uint8>(NormalizeAngle(dir_angle) / 60);
    }
    else {
        return numeric_cast<uint8>(NormalizeAngle(numeric_cast<int16>(dir_angle - 45 / 2)) / 45);
    }
}

auto GeometryHelper::NormalizeAngle(int16 dir_angle) -> int16
{
    FO_NO_STACK_TRACE_ENTRY();

    while (dir_angle < 0) {
        dir_angle += 360;
    }

    return numeric_cast<int16>(dir_angle % 360);
}

auto GeometryHelper::CheckDist(mpos hex1, mpos hex2, int32 dist) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return DistGame(hex1.x, hex1.y, hex2.x, hex2.y) <= dist;
}

auto GeometryHelper::ReverseDir(uint8 dir) -> uint8
{
    FO_NO_STACK_TRACE_ENTRY();

    return numeric_cast<uint8>((dir + GameSettings::MAP_DIR_COUNT / 2) % GameSettings::MAP_DIR_COUNT);
}

auto GeometryHelper::MoveHexByDir(mpos& hex, uint8 dir, msize map_size) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    auto raw_pos = ipos {hex.x, hex.y};

    if (MoveHexByDirUnsafe(raw_pos, dir, map_size)) {
        hex.x = numeric_cast<uint16>(raw_pos.x);
        hex.y = numeric_cast<uint16>(raw_pos.y);
        return true;
    }

    return false;
}

auto GeometryHelper::MoveHexByDirUnsafe(ipos& hex, uint8 dir, msize map_size) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    MoveHexByDirUnsafe(hex, dir);

    return map_size.IsValidPos(hex);
}

void GeometryHelper::MoveHexByDirUnsafe(ipos& hex, uint8 dir)
{
    FO_NO_STACK_TRACE_ENTRY();

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        switch (dir) {
        case 0:
            hex.x--;
            if ((hex.x % 2) == 0) {
                hex.y--;
            }
            break;
        case 1:
            hex.x--;
            if ((hex.x % 2) != 0) {
                hex.y++;
            }
            break;
        case 2:
            hex.y++;
            break;
        case 3:
            hex.x++;
            if ((hex.x % 2) != 0) {
                hex.y++;
            }
            break;
        case 4:
            hex.x++;
            if ((hex.x % 2) == 0) {
                hex.y--;
            }
            break;
        case 5:
            hex.y--;
            break;
        default:
            throw GenericException("Invalid dir", dir);
        }
    }
    else {
        switch (dir) {
        case 0:
            hex.x--;
            break;
        case 1:
            hex.x--;
            hex.y++;
            break;
        case 2:
            hex.y++;
            break;
        case 3:
            hex.x++;
            hex.y++;
            break;
        case 4:
            hex.x++;
            break;
        case 5:
            hex.x++;
            hex.y--;
            break;
        case 6:
            hex.y--;
            break;
        case 7:
            hex.x--;
            hex.y--;
            break;
        default:
            throw GenericException("Invalid dir", dir);
        }
    }
}

auto GeometryHelper::GetYProj() const -> float32
{
    FO_NO_STACK_TRACE_ENTRY();

    return 1.0f / std::sin(_settings.MapCameraAngle * DEG_TO_RAD_FLOAT);
}

auto GeometryHelper::GetLineDirAngle(int32 x1, int32 y1, int32 x2, int32 y2) const -> float32
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto x1_f = numeric_cast<float32>(x1);
    const auto y1_f = numeric_cast<float32>(y1) * GetYProj();
    const auto x2_f = numeric_cast<float32>(x2);
    const auto y2_f = numeric_cast<float32>(y2) * GetYProj();

    auto angle = 90.0f + RAD_TO_DEG_FLOAT * std::atan2(y2_f - y1_f, x2_f - x1_f);

    if (angle < 0.0f) {
        angle += 360.0f;
    }
    if (angle >= 360.0f) {
        angle -= 360.0f;
    }

    FO_RUNTIME_ASSERT(angle >= 0.0f);
    FO_RUNTIME_ASSERT(angle < 360.0f);

    return angle;
}

auto GeometryHelper::GetHexOffsets(mpos hex) const -> tuple<const int16*, const int16*>
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!_sxEven) {
        InitializeHexOffsets();
    }

    const auto odd = (hex.x % 2) != 0;
    const auto* sx = odd ? _sxOdd.get() : _sxEven.get();
    const auto* sy = odd ? _syOdd.get() : _syEven.get();

    return {sx, sy};
}

auto GeometryHelper::GetHexInterval(mpos from_hex, mpos to_hex) const -> ipos
{
    FO_NO_STACK_TRACE_ENTRY();

    return GetHexInterval(ipos {from_hex.x, from_hex.y}, ipos {to_hex.x, to_hex.y});
}

auto GeometryHelper::GetHexInterval(ipos from_raw_hex, ipos to_raw_hex) const -> ipos
{
    FO_NO_STACK_TRACE_ENTRY();

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        auto dx = to_raw_hex.x - from_raw_hex.x;
        const auto dy = to_raw_hex.y - from_raw_hex.y;

        auto x = dy * (_settings.MapHexWidth / 2) - dx * _settings.MapHexWidth;
        auto y = dy * _settings.MapHexLineHeight;

        if ((from_raw_hex.x % 2) != 0) {
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
        const auto dx = to_raw_hex.x - from_raw_hex.x;
        const auto dy = to_raw_hex.y - from_raw_hex.y;

        auto x = (dy - dx) * _settings.MapHexWidth / 2;
        auto y = (dy + dx) * _settings.MapHexLineHeight;

        return {x, y};
    }
}

void GeometryHelper::ForEachBlockLines(const vector<uint8>& lines, mpos hex, msize map_size, const std::function<void(mpos)>& callback)
{
    FO_STACK_TRACE_ENTRY();

    auto raw_pos = ipos {hex.x, hex.y};

    for (size_t i = 0; i < lines.size() / 2; i++) {
        const auto dir = lines[i * 2];
        const auto steps = lines[i * 2 + 1];

        if (dir >= GameSettings::MAP_DIR_COUNT || steps == 0 || steps > 9) {
            continue;
        }

        for (uint8 k = 0; k < steps; k++) {
            if (MoveHexByDirUnsafe(raw_pos, dir, map_size)) {
                callback({numeric_cast<uint16>(raw_pos.x), numeric_cast<uint16>(raw_pos.y)});
            }
        }
    }
}

FO_END_NAMESPACE();
