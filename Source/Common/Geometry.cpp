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

#include "Geometry.h"

FO_BEGIN_NAMESPACE();

GeometryHelper::GeometryHelper(GeometrySettings& settings) :
    _settings {&settings}
{
    FO_STACK_TRACE_ENTRY();
}

auto GeometryHelper::GetDistance(int32 x1, int32 y1, int32 x2, int32 y2) -> int32
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

auto GeometryHelper::GetDistance(mpos hex1, mpos hex2) -> int32
{
    FO_NO_STACK_TRACE_ENTRY();

    return GetDistance(hex1.x, hex1.y, hex2.x, hex2.y);
}

auto GeometryHelper::GetDistance(ipos32 hex1, ipos32 hex2) -> int32
{
    FO_NO_STACK_TRACE_ENTRY();

    return GetDistance(hex1.x, hex1.y, hex2.x, hex2.y);
}

auto GeometryHelper::GetDir(int32 x1, int32 y1, int32 x2, int32 y2) -> uint8
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

auto GeometryHelper::GetDir(int32 x1, int32 y1, int32 x2, int32 y2, float32 offset) -> uint8
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

auto GeometryHelper::GetDir(mpos from_hex, mpos to_hex) -> uint8
{
    FO_NO_STACK_TRACE_ENTRY();

    return GetDir(from_hex.x, from_hex.y, to_hex.x, to_hex.y);
}

auto GeometryHelper::GetDir(mpos from_hex, mpos to_hex, float32 offset) -> uint8
{
    FO_NO_STACK_TRACE_ENTRY();

    return GetDir(from_hex.x, from_hex.y, to_hex.x, to_hex.y, offset);
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

    return numeric_cast<int16>(dir_angle < 0 ? 360 - (-dir_angle % 360) : dir_angle % 360);
}

auto GeometryHelper::CheckDist(mpos hex1, mpos hex2, int32 dist) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return GetDistance(hex1.x, hex1.y, hex2.x, hex2.y) <= dist;
}

auto GeometryHelper::ReverseDir(uint8 dir) -> uint8
{
    FO_NO_STACK_TRACE_ENTRY();

    return numeric_cast<uint8>((dir + GameSettings::MAP_DIR_COUNT / 2) % GameSettings::MAP_DIR_COUNT);
}

auto GeometryHelper::HexesInRadius(int32 radius) -> int32
{
    FO_NO_STACK_TRACE_ENTRY();

    const int32 count = radius % 2 != 0 ? radius * (radius / 2 + 1) : radius * radius / 2 + radius / 2;
    return 1 + GameSettings::MAP_DIR_COUNT * count;
}

auto GeometryHelper::MoveHexByDir(mpos& hex, uint8 dir, msize map_size) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    auto raw_pos = ipos32 {hex.x, hex.y};
    MoveHexByDirUnsafe(raw_pos, dir);

    if (map_size.is_valid_pos(raw_pos)) {
        hex = map_size.from_raw_pos(raw_pos);
        return true;
    }

    return false;
}

void GeometryHelper::MoveHexByDirUnsafe(ipos32& hex, uint8 dir) noexcept
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
            break;
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
            break;
        }
    }
}

auto GeometryHelper::MoveHexAroundAway(mpos& hex, int32 index, msize map_size) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    ipos32 raw_hex = {hex.x, hex.y};
    MoveHexAroundAwayUnsafe(raw_hex, index);

    if (map_size.is_valid_pos(raw_hex)) {
        hex = map_size.from_raw_pos(raw_hex);
        return true;
    }
    else {
        return false;
    }
}

void GeometryHelper::MoveHexAroundAwayUnsafe(ipos32& hex, int32 index)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (index <= 0) {
        return;
    }

    // Move to first hex around
    constexpr uint8 init_dir = GameSettings::HEXAGONAL_GEOMETRY ? 0 : 7;
    MoveHexByDirUnsafe(hex, init_dir);

    uint8 dir = 2;
    int32 round = 1;
    int32 round_count = HexesInRadius(1) - 1;

    // Move around in a spiral away from the start position
    for (int32 i = 1; i < index; i++) {
        MoveHexByDirUnsafe(hex, dir);

        if (i >= round_count) {
            round++;
            round_count = HexesInRadius(round) - 1;
            MoveHexByDirUnsafe(hex, init_dir);
            FO_RUNTIME_ASSERT(dir == 1);
        }

        if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
            if (i % round == 0) {
                dir = numeric_cast<uint8>((dir + 1) % GameSettings::MAP_DIR_COUNT);
            }
        }
        else {
            if (i % (round * 2) == 0) {
                dir = numeric_cast<uint8>((dir + 2) % GameSettings::MAP_DIR_COUNT);
            }
        }
    }
}

auto GeometryHelper::GetYProj() const -> float32
{
    FO_NO_STACK_TRACE_ENTRY();

    return 1.0f / std::sin(_settings->MapCameraAngle * DEG_TO_RAD_FLOAT);
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

auto GeometryHelper::GetHexPos(mpos hex) const -> ipos32
{
    FO_NO_STACK_TRACE_ENTRY();

    return GetHexPos(ipos32(hex));
}

auto GeometryHelper::GetHexPos(ipos32 raw_hex) const -> ipos32
{
    FO_NO_STACK_TRACE_ENTRY();

    const int32 w = _settings->MapHexWidth;
    const int32 h = _settings->MapHexLineHeight;

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        const int32 hx = (raw_hex.x < 0 ? raw_hex.x - 1 : raw_hex.x) / 2;
        const int32 x = raw_hex.y * (w / 2) - raw_hex.x * w + w / 2 * hx;
        const int32 y = raw_hex.y * h + h * hx;

        return {x, y};
    }
    else {
        const int32 x = (raw_hex.y - raw_hex.x) * w / 2;
        const int32 y = (raw_hex.y + raw_hex.x) * h;

        return {x, y};
    }
}

auto GeometryHelper::GetHexAxialCoord(mpos hex) const -> ipos32
{
    FO_NO_STACK_TRACE_ENTRY();

    return GetHexAxialCoord(ipos32(hex));
}

auto GeometryHelper::GetHexAxialCoord(ipos32 raw_hex) const -> ipos32
{
    FO_NO_STACK_TRACE_ENTRY();

    const int32 w = _settings->MapHexWidth;
    const int32 h = _settings->MapHexLineHeight;
    const ipos32 hex_pos = GetHexPos(raw_hex);
    FO_RUNTIME_ASSERT(hex_pos.x % (w / 2) == 0);
    FO_RUNTIME_ASSERT(hex_pos.y % h == 0);

    return {hex_pos.x / (w / 2), hex_pos.y / h};
}

auto GeometryHelper::GetHexPosCoord(ipos32 pos, ipos32* hex_offset) const -> ipos32
{
    FO_NO_STACK_TRACE_ENTRY();

    const int32 w = _settings->MapHexWidth;
    const int32 half_w = w / 2;
    const int32 h = _settings->MapHexLineHeight;

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        const int32 ty = pos.y / h;
        const int32 num = ty * half_w - pos.x;
        const int32 rx = -(-num / w - (-num % w != 0 && (-num < 0) != (w < 0) ? 1 : 0));
        const int32 ry = ty - (rx < 0 ? rx - 1 : rx) / 2;
        const ipos32 raw_hex = {rx, ry};

        if (hex_offset != nullptr) {
            const int32 hx_adj = (raw_hex.x < 0 ? raw_hex.x - 1 : raw_hex.x) / 2;
            const int32 base_x = raw_hex.y * half_w - raw_hex.x * w + half_w * hx_adj;
            const int32 base_y = raw_hex.y * h + h * hx_adj;
            *hex_offset = {pos.x - base_x, pos.y - base_y};
        }

        return raw_hex;
    }
    else {
        const int32 ty = pos.y / h;
        const int32 tx = pos.x / half_w;
        const int32 rx = (ty - tx) / 2;
        const int32 ry = (ty + tx) / 2;
        const ipos32 raw_hex = {rx, ry};

        if (hex_offset != nullptr) {
            const int32 base_x = (raw_hex.y - raw_hex.x) * half_w;
            const int32 base_y = (raw_hex.y + raw_hex.x) * h;
            *hex_offset = {pos.x - base_x, pos.y - base_y};
        }

        return raw_hex;
    }
}

auto GeometryHelper::GetHexOffset(mpos from_hex, mpos to_hex) const -> ipos32
{
    FO_NO_STACK_TRACE_ENTRY();

    return GetHexOffset(ipos32(from_hex), ipos32(to_hex));
}

auto GeometryHelper::GetHexOffset(ipos32 from_raw_hex, ipos32 to_raw_hex) const -> ipos32
{
    FO_NO_STACK_TRACE_ENTRY();

    const int32 w = _settings->MapHexWidth;
    const int32 h = _settings->MapHexLineHeight;

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        const int32 dx = to_raw_hex.x - from_raw_hex.x;
        const int32 dy = to_raw_hex.y - from_raw_hex.y;
        const int32 dx2 = ((from_raw_hex.x % 2) != 0 ? (dx > 0 ? dx + 1 : dx) : (dx < 0 ? dx - 1 : dx)) / 2;
        const int32 x = dy * (w / 2) - dx * w + w / 2 * dx2;
        const int32 y = dy * h + h * dx2;

        return {x, y};
    }
    else {
        const int32 dx = to_raw_hex.x - from_raw_hex.x;
        const int32 dy = to_raw_hex.y - from_raw_hex.y;
        const int32 x = (dy - dx) * w / 2;
        const int32 y = (dy + dx) * h;

        return {x, y};
    }
}

auto GeometryHelper::GetAxialHexes(mpos from_hex, mpos to_hex, msize map_size) -> vector<mpos>
{
    FO_STACK_TRACE_ENTRY();

    vector<mpos> hexes;

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        auto [x, y] = GetHexOffset(from_hex, to_hex);
        x = -x;

        const int32 dx = x / _settings->MapHexWidth;
        const int32 dy = y / _settings->MapHexLineHeight;
        const int32 adx = std::abs(dx);
        const int32 ady = std::abs(dy);

        int32 hx;
        int32 hy;

        for (int32 j = 1; j <= ady; j++) {
            if (dy >= 0) {
                hx = from_hex.x + j / 2 + ((j % 2) != 0 ? 1 : 0);
                hy = from_hex.y + (j - (hx - from_hex.x - ((from_hex.x % 2) != 0 ? 1 : 0)) / 2);
            }
            else {
                hx = from_hex.x - j / 2 - ((j % 2) != 0 ? 1 : 0);
                hy = from_hex.y - (j - (from_hex.x - hx - ((from_hex.x % 2) != 0 ? 0 : 1)) / 2);
            }

            for (int32 i = 0; i <= adx; i++) {
                if (map_size.is_valid_pos(hx, hy)) {
                    hexes.emplace_back(map_size.from_raw_pos(hx, hy));
                }

                if (dx >= 0) {
                    if ((hx % 2) != 0) {
                        hy--;
                    }

                    hx++;
                }
                else {
                    hx--;

                    if ((hx % 2) != 0) {
                        hy++;
                    }
                }
            }
        }
    }
    else {
        auto [rw, rh] = GetHexOffset(from_hex, to_hex);

        if (rw == 0) {
            rw = 1;
        }
        if (rh == 0) {
            rh = 1;
        }

        const int32 hw = std::abs(rw / (_settings->MapHexWidth / 2)) + ((rw % (_settings->MapHexWidth / 2)) != 0 ? 1 : 0) + (std::abs(rw) >= _settings->MapHexWidth / 2 ? 1 : 0); // Hexes width
        const int32 hh = std::abs(rh / _settings->MapHexLineHeight) + ((rh % _settings->MapHexLineHeight) != 0 ? 1 : 0) + (std::abs(rh) >= _settings->MapHexLineHeight ? 1 : 0); // Hexes height
        int32 shx = numeric_cast<int32>(from_hex.x);
        int32 shy = numeric_cast<int32>(from_hex.y);

        for (int32 i = 0; i < hh; i++) {
            int32 hx = shx;
            int32 hy = shy;

            if (rh > 0) {
                if (rw > 0) {
                    if ((i % 2) != 0) {
                        shx++;
                    }
                    else {
                        shy++;
                    }
                }
                else {
                    if ((i % 2) != 0) {
                        shy++;
                    }
                    else {
                        shx++;
                    }
                }
            }
            else {
                if (rw > 0) {
                    if ((i % 2) != 0) {
                        shy--;
                    }
                    else {
                        shx--;
                    }
                }
                else {
                    if ((i % 2) != 0) {
                        shx--;
                    }
                    else {
                        shy--;
                    }
                }
            }

            for (int32 j = (i % 2) != 0 ? 1 : 0; j < hw; j += 2) {
                if (map_size.is_valid_pos(hx, hy)) {
                    hexes.emplace_back(map_size.from_raw_pos(hx, hy));
                }

                if (rw > 0) {
                    hx--;
                    hy++;
                }
                else {
                    hx++;
                    hy--;
                }
            }
        }
    }

    return hexes;
}

void GeometryHelper::ForEachMultihexLines(span<const uint8> dir_line, mpos hex, msize map_size, const function<void(mpos)>& callback)
{
    FO_STACK_TRACE_ENTRY();

    auto step_raw_hex = ipos32 {hex.x, hex.y};

    for (size_t i = 0; i < dir_line.size() / 2; i++) {
        const auto dir = dir_line[i * 2];
        const auto steps = dir_line[i * 2 + 1];

        if (dir >= GameSettings::MAP_DIR_COUNT) {
            continue;
        }

        for (uint8 j = 0; j < steps; j++) {
            MoveHexByDirUnsafe(step_raw_hex, dir);

            if (map_size.is_valid_pos(step_raw_hex)) {
                const auto step_hex = map_size.from_raw_pos(step_raw_hex);

                if (step_hex != hex) {
                    callback(step_hex);
                }
            }
        }
    }
}

FO_END_NAMESPACE();
