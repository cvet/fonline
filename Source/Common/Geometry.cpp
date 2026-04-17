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

#include "Geometry.h"

FO_BEGIN_NAMESPACE

mdir::mdir(int32_t angle) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    const int32_t mod = angle % 360;
    _value = static_cast<int16_t>(mod < 0 ? mod + 360 : mod);
}

mdir::mdir(hdir dir) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        _value = static_cast<int16_t>(dir.value() * 60 + 30);
    }
    else {
        _value = static_cast<int16_t>(dir.value() * 45 + 45);
    }

    const int32_t mod = _value % 360;
    _value = static_cast<int16_t>(mod < 0 ? mod + 360 : mod);
}

auto mdir::hex() const noexcept -> hdir
{
    FO_NO_STACK_TRACE_ENTRY();

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        return hdir(_value / 60);
    }
    else {
        return hdir((_value - 45 / 2) / 45);
    }
}

auto mdir::incHex() const noexcept -> mdir
{
    FO_NO_STACK_TRACE_ENTRY();

    constexpr int32_t step = 360 / GameSettings::MAP_DIR_COUNT;
    return mdir(static_cast<int16_t>(_value + step));
}

auto mdir::decHex() const noexcept -> mdir
{
    FO_NO_STACK_TRACE_ENTRY();

    constexpr int32_t step = 360 / GameSettings::MAP_DIR_COUNT;
    return mdir(static_cast<int16_t>(_value - step));
}

auto mdir::rotateHex(int32_t steps) const noexcept -> mdir
{
    FO_NO_STACK_TRACE_ENTRY();

    constexpr int32_t step = 360 / GameSettings::MAP_DIR_COUNT;
    return mdir(static_cast<int16_t>(_value + steps * step));
}

auto mdir::reverse() const noexcept -> mdir
{
    FO_NO_STACK_TRACE_ENTRY();

    return mdir(static_cast<int16_t>(_value + 180));
}

auto GeometryHelper::GetDistance(int32_t x1, int32_t y1, int32_t x2, int32_t y2) -> int32_t
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

auto GeometryHelper::GetDistance(mpos hex1, mpos hex2) -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return GetDistance(hex1.x, hex1.y, hex2.x, hex2.y);
}

auto GeometryHelper::GetDistance(ipos32 hex1, ipos32 hex2) -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return GetDistance(hex1.x, hex1.y, hex2.x, hex2.y);
}

auto GeometryHelper::GetHexDir(int32_t x1, int32_t y1, int32_t x2, int32_t y2) -> hdir
{
    FO_NO_STACK_TRACE_ENTRY();

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        const auto hx = numeric_cast<float32_t>(x1);
        const auto hy = numeric_cast<float32_t>(y1);
        const auto tx = numeric_cast<float32_t>(x2);
        const auto ty = numeric_cast<float32_t>(y2);
        const auto nx = 3 * (tx - hx);
        const auto ny = (ty - hy) * SQRT3_X2_FLOAT - (numeric_cast<float32_t>(std::abs(x2 % 2)) - numeric_cast<float32_t>(std::abs(x1 % 2))) * SQRT3_FLOAT;
        const auto dir = 180.0f + RAD_TO_DEG_FLOAT * std::atan2(ny, nx);

        if (dir >= 60.0f && dir < 120.0f) {
            return hdir::NorthWest;
        }
        if (dir >= 120.0f && dir < 180.0f) {
            return hdir::West;
        }
        if (dir >= 180.0f && dir < 240.0f) {
            return hdir::SouthWest;
        }
        if (dir >= 240.0f && dir < 300.0f) {
            return hdir::SouthEast;
        }
        if (dir >= 300.0f) {
            return hdir::East;
        }

        return hdir::NorthEast;
    }
#if FO_GEOMETRY == 2
    else {
        const auto dir = 180.0f + RAD_TO_DEG_FLOAT * std::atan2(numeric_cast<float32_t>(x2 - x1), numeric_cast<float32_t>(y2 - y1));

        if (dir >= 22.5f && dir < 67.5f) {
            return hdir::North;
        }
        if (dir >= 67.5f && dir < 112.5f) {
            return hdir::NorthEast;
        }
        if (dir >= 112.5f && dir < 157.5f) {
            return hdir::East;
        }
        if (dir >= 157.5f && dir < 202.5f) {
            return hdir::SouthEast;
        }
        if (dir >= 202.5f && dir < 247.5f) {
            return hdir::South;
        }
        if (dir >= 247.5f && dir < 292.5f) {
            return hdir::SouthWest;
        }
        if (dir >= 292.5f && dir < 337.5f) {
            return hdir::West;
        }

        return hdir::NorthWest;
    }
#endif
}

auto GeometryHelper::GetHexDir(int32_t x1, int32_t y1, int32_t x2, int32_t y2, float32_t offset) -> hdir
{
    FO_NO_STACK_TRACE_ENTRY();

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        const auto hx = numeric_cast<float32_t>(x1);
        const auto hy = numeric_cast<float32_t>(y1);
        const auto tx = numeric_cast<float32_t>(x2);
        const auto ty = numeric_cast<float32_t>(y2);
        const auto nx = 3 * (tx - hx);
        const auto ny = (ty - hy) * SQRT3_X2_FLOAT - (numeric_cast<float32_t>(std::abs(x2 % 2)) - numeric_cast<float32_t>(std::abs(x1 % 2))) * SQRT3_FLOAT;
        auto dir = 180.0f + RAD_TO_DEG_FLOAT * std::atan2(ny, nx) + offset;

        if (dir < 0.0f) {
            dir = 360.0f - std::fmod(-dir, 360.0f);
        }
        else if (dir >= 360.0f) {
            dir = std::fmod(dir, 360.0f);
        }

        if (dir >= 60.0f && dir < 120.0f) {
            return hdir::NorthWest;
        }
        if (dir >= 120.0f && dir < 180.0f) {
            return hdir::West;
        }
        if (dir >= 180.0f && dir < 240.0f) {
            return hdir::SouthWest;
        }
        if (dir >= 240.0f && dir < 300.0f) {
            return hdir::SouthEast;
        }
        if (dir >= 300.0f) {
            return hdir::East;
        }

        return hdir::NorthEast;
    }
#if FO_GEOMETRY == 2
    else {
        auto dir = 180.0f + RAD_TO_DEG_FLOAT * std::atan2(numeric_cast<float32_t>(x2 - x1), numeric_cast<float32_t>(y2 - y1)) + offset;

        if (dir < 0.0f) {
            dir = 360.0f - std::fmod(-dir, 360.0f);
        }
        else if (dir >= 360.0f) {
            dir = std::fmod(dir, 360.0f);
        }

        if (dir >= 22.5f && dir < 67.5f) {
            return hdir::North;
        }
        if (dir >= 67.5f && dir < 112.5f) {
            return hdir::NorthEast;
        }
        if (dir >= 112.5f && dir < 157.5f) {
            return hdir::East;
        }
        if (dir >= 157.5f && dir < 202.5f) {
            return hdir::SouthEast;
        }
        if (dir >= 202.5f && dir < 247.5f) {
            return hdir::South;
        }
        if (dir >= 247.5f && dir < 292.5f) {
            return hdir::SouthWest;
        }
        if (dir >= 292.5f && dir < 337.5f) {
            return hdir::West;
        }

        return hdir::NorthWest;
    }
#endif
}

auto GeometryHelper::GetHexDir(mpos from_hex, mpos to_hex) -> hdir
{
    FO_NO_STACK_TRACE_ENTRY();

    return GetHexDir(from_hex.x, from_hex.y, to_hex.x, to_hex.y);
}

auto GeometryHelper::GetHexDir(mpos from_hex, mpos to_hex, float32_t offset) -> hdir
{
    FO_NO_STACK_TRACE_ENTRY();

    return GetHexDir(from_hex.x, from_hex.y, to_hex.x, to_hex.y, offset);
}

auto GeometryHelper::GetDirAngle(int32_t x1, int32_t y1, int32_t x2, int32_t y2) -> float32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto hx = numeric_cast<float32_t>(x1);
    const auto hy = numeric_cast<float32_t>(y1);
    const auto tx = numeric_cast<float32_t>(x2);
    const auto ty = numeric_cast<float32_t>(y2);
    const auto nx = 3 * (tx - hx);
    const auto ny = (ty - hy) * SQRT3_X2_FLOAT - (numeric_cast<float32_t>(std::abs(x2 % 2)) - numeric_cast<float32_t>(std::abs(x1 % 2))) * SQRT3_FLOAT;

    float32_t r = 180.0f + RAD_TO_DEG_FLOAT * std::atan2(ny, nx);
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

auto GeometryHelper::GetDirAngle(mpos from_hex, mpos to_hex) -> float32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return GetDirAngle(from_hex.x, from_hex.y, to_hex.x, to_hex.y);
}

auto GeometryHelper::GetDirAngleDiff(float32_t a1, float32_t a2) -> float32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto r = 180.0f - std::abs(std::abs(a1 - a2) - 180.0f);
    FO_RUNTIME_ASSERT(r >= 0.0f);
    FO_RUNTIME_ASSERT(r <= 180.0f);

    return r;
}

auto GeometryHelper::GetDirAngleDiffSided(float32_t a1, float32_t a2) -> float32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto a1_r = a1 * DEG_TO_RAD_FLOAT;
    const auto a2_r = a2 * DEG_TO_RAD_FLOAT;
    const auto r = std::atan2(std::sin(a2_r - a1_r), std::cos(a2_r - a1_r)) * RAD_TO_DEG_FLOAT;
    FO_RUNTIME_ASSERT(r >= -180.0f);
    FO_RUNTIME_ASSERT(r <= 180.0f);

    return r;
}

auto GeometryHelper::CheckDist(mpos hex1, mpos hex2, int32_t dist) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return GetDistance(hex1.x, hex1.y, hex2.x, hex2.y) <= dist;
}

auto GeometryHelper::HexesInRadius(int32_t radius) -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    const int32_t count = radius % 2 != 0 ? radius * (radius / 2 + 1) : radius * radius / 2 + radius / 2;
    return 1 + GameSettings::MAP_DIR_COUNT * count;
}

auto GeometryHelper::MoveHexByDir(mpos& hex, mdir dir, msize map_size) -> bool
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

void GeometryHelper::MoveHexByDirUnsafe(ipos32& hex, mdir dir) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto hex_dir = dir.hex();

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        if (hex_dir == hdir::NorthEast) {
            hex.x--;
            if ((hex.x % 2) == 0) {
                hex.y--;
            }
        }
        else if (hex_dir == hdir::East) {
            hex.x--;
            if ((hex.x % 2) != 0) {
                hex.y++;
            }
        }
        else if (hex_dir == hdir::SouthEast) {
            hex.y++;
        }
        else if (hex_dir == hdir::SouthWest) {
            hex.x++;
            if ((hex.x % 2) != 0) {
                hex.y++;
            }
        }
        else if (hex_dir == hdir::West) {
            hex.x++;
            if ((hex.x % 2) == 0) {
                hex.y--;
            }
        }
        else if (hex_dir == hdir::NorthWest) {
            hex.y--;
        }
    }
#if FO_GEOMETRY == 2
    else {
        if (hex_dir == hdir::NorthEast) {
            hex.x--;
        }
        else if (hex_dir == hdir::East) {
            hex.x--;
            hex.y++;
        }
        else if (hex_dir == hdir::SouthEast) {
            hex.y++;
        }
        else if (hex_dir == hdir::South) {
            hex.x++;
            hex.y++;
        }
        else if (hex_dir == hdir::SouthWest) {
            hex.x++;
        }
        else if (hex_dir == hdir::West) {
            hex.x++;
            hex.y--;
        }
        else if (hex_dir == hdir::NorthWest) {
            hex.y--;
        }
        else if (hex_dir == hdir::North) {
            hex.x--;
            hex.y--;
        }
    }
#endif
}

auto GeometryHelper::MoveHexAroundAway(mpos& hex, int32_t index, msize map_size) -> bool
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

void GeometryHelper::MoveHexAroundAwayUnsafe(ipos32& hex, int32_t index)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (index <= 0) {
        return;
    }

    // Move to first hex around
#if FO_GEOMETRY == 1
    constexpr hdir init_dir = hdir::NorthEast;
#elif FO_GEOMETRY == 2
    constexpr hdir init_dir = hdir::North;
#endif
    MoveHexByDirUnsafe(hex, mdir(init_dir));

    auto dir = hdir::SouthEast;
    int32_t round = 1;
    int32_t round_count = HexesInRadius(1) - 1;

    // Move around in a spiral away from the start position
    for (int32_t i = 1; i < index; i++) {
        MoveHexByDirUnsafe(hex, dir);

        if (i >= round_count) {
            round++;
            round_count = HexesInRadius(round) - 1;
            MoveHexByDirUnsafe(hex, mdir(init_dir));
            FO_RUNTIME_ASSERT(dir == hdir::East);
        }

        if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
            if (i % round == 0) {
                dir = hdir(dir.value() + 1);
            }
        }
        else {
            if (i % (round * 2) == 0) {
                dir = hdir(dir.value() + 2);
            }
        }
    }
}

auto GeometryHelper::GetYProj() -> float32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return 1.0f / std::sin(GameSettings::MAP_CAMERA_ANGLE * DEG_TO_RAD_FLOAT);
}

auto GeometryHelper::GetLineDirAngle(int32_t x1, int32_t y1, int32_t x2, int32_t y2) -> float32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto x1_f = numeric_cast<float32_t>(x1);
    const auto y1_f = numeric_cast<float32_t>(y1) * GetYProj();
    const auto x2_f = numeric_cast<float32_t>(x2);
    const auto y2_f = numeric_cast<float32_t>(y2) * GetYProj();

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

auto GeometryHelper::GetHexPos(mpos hex) -> ipos32
{
    FO_NO_STACK_TRACE_ENTRY();

    return GetHexPos(ipos32(hex));
}

auto GeometryHelper::GetHexPos(ipos32 raw_hex) -> ipos32
{
    FO_NO_STACK_TRACE_ENTRY();

    constexpr int32_t w = GameSettings::MAP_HEX_WIDTH;
    constexpr int32_t h = GameSettings::MAP_HEX_LINE_HEIGHT;

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        const int32_t hx = (raw_hex.x < 0 ? raw_hex.x - 1 : raw_hex.x) / 2;
        const int32_t x = raw_hex.y * (w / 2) - raw_hex.x * w + w / 2 * hx;
        const int32_t y = raw_hex.y * h + h * hx;

        return {x, y};
    }
    else {
        const int32_t x = (raw_hex.y - raw_hex.x) * w / 2;
        const int32_t y = (raw_hex.y + raw_hex.x) * h;

        return {x, y};
    }
}

auto GeometryHelper::GetHexAxialCoord(mpos hex) -> ipos32
{
    FO_NO_STACK_TRACE_ENTRY();

    return GetHexAxialCoord(ipos32(hex));
}

auto GeometryHelper::GetHexAxialCoord(ipos32 raw_hex) -> ipos32
{
    FO_NO_STACK_TRACE_ENTRY();

    constexpr int32_t w = GameSettings::MAP_HEX_WIDTH;
    constexpr int32_t h = GameSettings::MAP_HEX_LINE_HEIGHT;
    const ipos32 hex_pos = GetHexPos(raw_hex);
    FO_RUNTIME_ASSERT(hex_pos.x % (w / 2) == 0);
    FO_RUNTIME_ASSERT(hex_pos.y % h == 0);

    return {hex_pos.x / (w / 2), hex_pos.y / h};
}

auto GeometryHelper::GetHexPosCoord(ipos32 pos, ipos32* hex_offset) -> ipos32
{
    FO_NO_STACK_TRACE_ENTRY();

    constexpr int32_t w = GameSettings::MAP_HEX_WIDTH;
    constexpr int32_t half_w = w / 2;
    constexpr int32_t h = GameSettings::MAP_HEX_LINE_HEIGHT;

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        // Hex centers form a lattice with basis vectors:
        //   v1 = (half_w, h)  — direction of ry++
        //   v2 = (w, 0)       — direction of rx-- (horizontal neighbor)
        // Pixel position = a * v1 + b * v2
        // Solving: a = py / h, b = (px - a * half_w) / w
        const float32_t fh = numeric_cast<float32_t>(h);
        const float32_t fw = numeric_cast<float32_t>(w);
        const float32_t fhw = numeric_cast<float32_t>(half_w);

        const float32_t fa = numeric_cast<float32_t>(pos.y) / fh;
        const float32_t fb = (numeric_cast<float32_t>(pos.x) - fa * fhw) / fw;
        const float32_t fc = -(fa + fb);

        // Cube coordinate rounding
        int32_t ra = iround<int32_t>(fa);
        int32_t rb = iround<int32_t>(fb);
        int32_t rc = iround<int32_t>(fc);

        if (ra + rb + rc != 0) {
            const float32_t da = std::abs(numeric_cast<float32_t>(ra) - fa);
            const float32_t db = std::abs(numeric_cast<float32_t>(rb) - fb);
            const float32_t dc = std::abs(numeric_cast<float32_t>(rc) - fc);

            if (da > db && da > dc) {
                ra = -(rb + rc);
            }
            else if (db > dc) {
                rb = -(ra + rc);
            }
            else {
                rc = -(ra + rb);
            }
        }

        // Hex center from lattice coordinates
        int32_t cx = ra * half_w + rb * w;
        int32_t cy = ra * h;
        int32_t dx = pos.x - cx;
        int32_t dy = pos.y - cy;

        // Verify point is inside the hex using edge constraints
        // Pointy-top hex vertices relative to center:
        //   top (0, -H/2), upper-right (half_w, -H/4), lower-right (half_w, H/4),
        //   bottom (0, H/2), lower-left (-half_w, H/4), upper-left (-half_w, -H/4)
        // Three symmetric constraint pairs:
        //   |dx| <= half_w
        //   |dx * hq - dy * half_w| <= limit
        //   |dx * hq + dy * half_w| <= limit
        // where hq = MAP_HEX_HEIGHT / 4, limit = 2 * half_w * hq
        constexpr int32_t hq = GameSettings::MAP_HEX_HEIGHT / 4;
        const int32_t limit = 2 * half_w * hq;

        const auto is_inside_hex = [half_w, hq, limit](int32_t lx, int32_t ly) -> bool { return std::abs(lx) <= half_w && std::abs(lx * hq - ly * half_w) <= limit && std::abs(lx * hq + ly * half_w) <= limit; };

        if (!is_inside_hex(dx, dy)) {
            // Check 6 neighbors in lattice space: (±1,0), (0,±1), (+1,-1), (-1,+1)
            static constexpr ipos32 neighbor_offsets[] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, -1}, {-1, 1}};

            for (const auto& off : neighbor_offsets) {
                const int32_t na = ra + off.x;
                const int32_t nb = rb + off.y;
                const int32_t ncx = na * half_w + nb * w;
                const int32_t ncy = na * h;
                const int32_t ndx = pos.x - ncx;
                const int32_t ndy = pos.y - ncy;

                if (is_inside_hex(ndx, ndy)) {
                    ra = na;
                    rb = nb;
                    cx = ncx;
                    cy = ncy;
                    dx = ndx;
                    dy = ndy;
                    break;
                }
            }
        }

        // Convert lattice (a, b) back to engine hex coordinates (rx, ry)
        // From: a = ry + floor(rx/2), b = -rx
        const int32_t rx = -rb;
        const int32_t ry = ra - (rx < 0 ? rx - 1 : rx) / 2;
        const ipos32 raw_hex = {rx, ry};

        if (hex_offset != nullptr) {
            *hex_offset = {dx, dy};
        }

        return raw_hex;
    }
    else {
        // Rhomboid/diamond grid
        // Cell (rx, ry) center: cx = (ry - rx) * half_w, cy = (ry + rx) * h
        // Oblique coordinate transform:
        //   u = px / half_w + py / h = 2 * ry (at center)
        //   v = -px / half_w + py / h = 2 * rx (at center)
        const float32_t u = numeric_cast<float32_t>(pos.x) / numeric_cast<float32_t>(half_w) + numeric_cast<float32_t>(pos.y) / numeric_cast<float32_t>(h);
        const float32_t v = -numeric_cast<float32_t>(pos.x) / numeric_cast<float32_t>(half_w) + numeric_cast<float32_t>(pos.y) / numeric_cast<float32_t>(h);

        const int32_t ry = iround<int32_t>(u * 0.5f);
        const int32_t rx = iround<int32_t>(v * 0.5f);
        const ipos32 raw_hex = {rx, ry};

        if (hex_offset != nullptr) {
            const int32_t base_x = (ry - rx) * half_w;
            const int32_t base_y = (ry + rx) * h;
            *hex_offset = {pos.x - base_x, pos.y - base_y};
        }

        return raw_hex;
    }
}

auto GeometryHelper::GetHexOffset(mpos from_hex, mpos to_hex) -> ipos32
{
    FO_NO_STACK_TRACE_ENTRY();

    return GetHexOffset(ipos32(from_hex), ipos32(to_hex));
}

auto GeometryHelper::GetHexOffset(ipos32 from_raw_hex, ipos32 to_raw_hex) -> ipos32
{
    FO_NO_STACK_TRACE_ENTRY();

    constexpr int32_t w = GameSettings::MAP_HEX_WIDTH;
    constexpr int32_t h = GameSettings::MAP_HEX_LINE_HEIGHT;

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        const int32_t dx = to_raw_hex.x - from_raw_hex.x;
        const int32_t dy = to_raw_hex.y - from_raw_hex.y;
        const int32_t dx2 = ((from_raw_hex.x % 2) != 0 ? (dx > 0 ? dx + 1 : dx) : (dx < 0 ? dx - 1 : dx)) / 2;
        const int32_t x = dy * (w / 2) - dx * w + w / 2 * dx2;
        const int32_t y = dy * h + h * dx2;

        return {x, y};
    }
    else {
        const int32_t dx = to_raw_hex.x - from_raw_hex.x;
        const int32_t dy = to_raw_hex.y - from_raw_hex.y;
        const int32_t x = (dy - dx) * w / 2;
        const int32_t y = (dy + dx) * h;

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

        const int32_t dx = x / GameSettings::MAP_HEX_WIDTH;
        const int32_t dy = y / GameSettings::MAP_HEX_LINE_HEIGHT;
        const int32_t adx = std::abs(dx);
        const int32_t ady = std::abs(dy);

        int32_t hx;
        int32_t hy;

        for (int32_t j = 1; j <= ady; j++) {
            if (dy >= 0) {
                hx = from_hex.x + j / 2 + ((j % 2) != 0 ? 1 : 0);
                hy = from_hex.y + (j - (hx - from_hex.x - ((from_hex.x % 2) != 0 ? 1 : 0)) / 2);
            }
            else {
                hx = from_hex.x - j / 2 - ((j % 2) != 0 ? 1 : 0);
                hy = from_hex.y - (j - (from_hex.x - hx - ((from_hex.x % 2) != 0 ? 0 : 1)) / 2);
            }

            for (int32_t i = 0; i <= adx; i++) {
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

        const int32_t hw = std::abs(rw / (GameSettings::MAP_HEX_WIDTH / 2)) + ((rw % (GameSettings::MAP_HEX_WIDTH / 2)) != 0 ? 1 : 0) + (std::abs(rw) >= GameSettings::MAP_HEX_WIDTH / 2 ? 1 : 0); // Hexes width
        const int32_t hh = std::abs(rh / GameSettings::MAP_HEX_LINE_HEIGHT) + ((rh % GameSettings::MAP_HEX_LINE_HEIGHT) != 0 ? 1 : 0) + (std::abs(rh) >= GameSettings::MAP_HEX_LINE_HEIGHT ? 1 : 0); // Hexes height
        int32_t shx = numeric_cast<int32_t>(from_hex.x);
        int32_t shy = numeric_cast<int32_t>(from_hex.y);

        for (int32_t i = 0; i < hh; i++) {
            int32_t hx = shx;
            int32_t hy = shy;

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

            for (int32_t j = (i % 2) != 0 ? 1 : 0; j < hw; j += 2) {
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

void GeometryHelper::ForEachMultihexLines(const_span<uint8_t> dir_line, mpos hex, msize map_size, const function<void(mpos)>& callback)
{
    FO_STACK_TRACE_ENTRY();

    auto step_raw_hex = ipos32 {hex.x, hex.y};

    for (size_t i = 0; i < dir_line.size() / 2; i++) {
        const auto dir = hdir(dir_line[i * 2]);
        const auto steps = dir_line[i * 2 + 1];

        for (uint8_t j = 0; j < steps; j++) {
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

auto GeometryHelper::IntersectCircleLine(int32_t cx, int32_t cy, int32_t radius, int32_t x1, int32_t y1, int32_t x2, int32_t y2) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    const int32_t x01 = x1 - cx;
    const int32_t y01 = y1 - cy;
    const int32_t x02 = x2 - cx;
    const int32_t y02 = y2 - cy;
    const int32_t dx = x02 - x01;
    const int32_t dy = y02 - y01;
    const int32_t a = dx * dx + dy * dy;
    const int32_t b = 2 * (x01 * dx + y01 * dy);
    const int32_t c = x01 * x01 + y01 * y01 - radius * radius;

    if (-b < 0) {
        return c < 0;
    }
    if (-b < 2 * a) {
        return 4 * a * c - b * b < 0;
    }

    return a + b + c < 0;
}

auto GeometryHelper::GetStepsCoords(ipos32 from_pos, ipos32 to_pos) noexcept -> fpos32
{
    FO_NO_STACK_TRACE_ENTRY();

    if (from_pos == to_pos) {
        return {};
    }

    const float32_t dx = numeric_cast<float32_t>(std::abs(to_pos.x - from_pos.x));
    const float32_t dy = numeric_cast<float32_t>(std::abs(to_pos.y - from_pos.y));

    float32_t sx = 1.0f;
    float32_t sy = 1.0f;

    if (dx < dy) {
        sx = dx / dy;
    }
    else {
        sy = dy / dx;
    }

    if (to_pos.x < from_pos.x) {
        sx = -sx;
    }
    if (to_pos.y < from_pos.y) {
        sy = -sy;
    }

    return {sx, sy};
}

auto GeometryHelper::ChangeStepsCoords(fpos32 pos, float32_t deq) noexcept -> fpos32
{
    FO_NO_STACK_TRACE_ENTRY();

    const float32_t rad = deq * DEG_TO_RAD_FLOAT;
    const float32_t x = pos.x * std::cos(rad) - pos.y * std::sin(rad);
    const float32_t y = pos.x * std::sin(rad) + pos.y * std::cos(rad);

    return {x, y};
}

FO_END_NAMESPACE
