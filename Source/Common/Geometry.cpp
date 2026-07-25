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

    int32_t mod = angle % 360;
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

    int32_t mod = _value % 360;
    _value = static_cast<int16_t>(mod < 0 ? mod + 360 : mod);
}

auto mdir::hex() const noexcept -> hdir
{
    FO_NO_STACK_TRACE_ENTRY();

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        return hdir(_value / 60);
    }
    else {
        constexpr int32_t step = 360 / GameSettings::MAP_DIR_COUNT;
        constexpr int32_t half_step = step / 2;
        int32_t shifted_angle = _value + 360 - half_step;
        return hdir(shifted_angle / step);
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
        int32_t dx = x1 > x2 ? x1 - x2 : x2 - x1;

        if ((x1 % 2) == 0) {
            if (y2 <= y1) {
                int32_t rx = y1 - y2 - dx / 2;

                return dx + (rx > 0 ? rx : 0);
            }

            int32_t rx = y2 - y1 - (dx + 1) / 2;

            return dx + (rx > 0 ? rx : 0);
        }

        if (y2 >= y1) {
            int32_t rx = y2 - y1 - dx / 2;

            return dx + (rx > 0 ? rx : 0);
        }

        int32_t rx = y1 - y2 - (dx + 1) / 2;

        return dx + (rx > 0 ? rx : 0);
    }
    else {
        int32_t dx = std::abs(x2 - x1);
        int32_t dy = std::abs(y2 - y1);

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
        float32_t hx = numeric_cast<float32_t>(x1);
        float32_t hy = numeric_cast<float32_t>(y1);
        float32_t tx = numeric_cast<float32_t>(x2);
        float32_t ty = numeric_cast<float32_t>(y2);
        float32_t nx = 3 * (tx - hx);
        float32_t ny = (ty - hy) * SQRT3_X2_FLOAT - (numeric_cast<float32_t>(std::abs(x2 % 2)) - numeric_cast<float32_t>(std::abs(x1 % 2))) * SQRT3_FLOAT;
        float32_t dir = 180.0f + RAD_TO_DEG_FLOAT * std::atan2(ny, nx);

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
        float32_t hx = numeric_cast<float32_t>(x1);
        float32_t hy = numeric_cast<float32_t>(y1);
        float32_t tx = numeric_cast<float32_t>(x2);
        float32_t ty = numeric_cast<float32_t>(y2);
        float32_t nx = 3 * (tx - hx);
        float32_t ny = (ty - hy) * SQRT3_X2_FLOAT - (numeric_cast<float32_t>(std::abs(x2 % 2)) - numeric_cast<float32_t>(std::abs(x1 % 2))) * SQRT3_FLOAT;
        float32_t dir = 180.0f + RAD_TO_DEG_FLOAT * std::atan2(ny, nx) + offset;

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

    float32_t hx = numeric_cast<float32_t>(x1);
    float32_t hy = numeric_cast<float32_t>(y1);
    float32_t tx = numeric_cast<float32_t>(x2);
    float32_t ty = numeric_cast<float32_t>(y2);
    float32_t nx = 3 * (tx - hx);
    float32_t ny = (ty - hy) * SQRT3_X2_FLOAT - (numeric_cast<float32_t>(std::abs(x2 % 2)) - numeric_cast<float32_t>(std::abs(x1 % 2))) * SQRT3_FLOAT;

    float32_t r = 180.0f + RAD_TO_DEG_FLOAT * std::atan2(ny, nx);
    FO_VERIFY_AND_THROW(r >= 0.0f, "Hex direction angle calculation produced a negative raw angle", x1, y1, x2, y2, r);
    FO_VERIFY_AND_THROW(r <= 360.0f, "Hex direction angle calculation produced a value above 360 degrees before normalization", x1, y1, x2, y2, r);

    r = -r + 60.0f;

    if (r < 0.0f) {
        r += 360.0f;
    }
    if (r >= 360.0f) {
        r -= 360.0f;
    }

    FO_VERIFY_AND_THROW(r >= 0.0f, "Normalized hex direction angle calculation produced a negative value", x1, y1, x2, y2, r);
    FO_VERIFY_AND_THROW(r < 360.0f, "Computed hex direction angle is outside the normalized degree range", x1, y1, x2, y2, r);

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

    float32_t r = 180.0f - std::abs(std::abs(a1 - a2) - 180.0f);
    FO_VERIFY_AND_THROW(r >= 0.0f, "Unsigned direction angle difference calculation produced a negative value", a1, a2, r);
    FO_VERIFY_AND_THROW(r <= 180.0f, "Unsigned direction angle difference exceeded 180 degrees", a1, a2, r);

    return r;
}

auto GeometryHelper::GetDirAngleDiffSided(float32_t a1, float32_t a2) -> float32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    float32_t a1_r = a1 * DEG_TO_RAD_FLOAT;
    float32_t a2_r = a2 * DEG_TO_RAD_FLOAT;
    float32_t r = std::atan2(std::sin(a2_r - a1_r), std::cos(a2_r - a1_r)) * RAD_TO_DEG_FLOAT;
    FO_VERIFY_AND_THROW(r >= -180.0f, "Signed direction angle difference is below -180 degrees", a1, a2, r);
    FO_VERIFY_AND_THROW(r <= 180.0f, "Signed direction angle difference exceeded 180 degrees", a1, a2, r);

    return r;
}

auto GeometryHelper::CheckDist(mpos hex1, mpos hex2, int32_t dist) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return GetDistance(hex1.x, hex1.y, hex2.x, hex2.y) <= dist;
}

auto GeometryHelper::HexesInRadius(int32_t radius) noexcept -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    int32_t count = radius % 2 != 0 ? radius * (radius / 2 + 1) : radius * radius / 2 + radius / 2;
    return 1 + GameSettings::MAP_DIR_COUNT * count;
}

auto GeometryHelper::MoveHexByDir(mpos& hex, mdir dir, msize map_size) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    ipos32 raw_pos = ipos32 {hex.x, hex.y};
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

    hdir hex_dir = dir.hex();

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

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        // Hex grid: ring R has 6R cells; HexesInRadius(R) = 1 + 3R(R+1).
        // R = smallest integer with 3R(R+1) >= index.
        float64_t fi = static_cast<float64_t>(index);
        int32_t round = static_cast<int32_t>(std::ceil((std::sqrt(12.0 * fi + 9.0) - 3.0) / 6.0 - 1e-9));

        if (round < 1) {
            round = 1;
        }
        while (round > 1 && 3 * (round - 1) * round >= index) {
            round--;
        }
        while (3 * round * (round + 1) < index) {
            round++;
        }

        int32_t r = index - (1 + 3 * (round - 1) * round); // [0, 6*round - 1]

        // Axial position relative to hex (engine's doubled-axial: E=(+2,0), SE=(+1,+1), ...).
        // 6 sides of length `round` walked in order SE, SW, W, NW, NE, E. Ranges below
        // overlap at corners by design - both side formulas agree there.
        int32_t dax;
        int32_t day;

        if (r <= round) {
            dax = round + r;
            day = -round + r;
        }
        else if (r <= 2 * round) {
            dax = 3 * round - r;
            day = r - round;
        }
        else if (r <= 3 * round) {
            dax = 5 * round - 2 * r;
            day = round;
        }
        else if (r <= 4 * round) {
            dax = 2 * round - r;
            day = 4 * round - r;
        }
        else if (r <= 5 * round) {
            dax = r - 6 * round;
            day = 4 * round - r;
        }
        else {
            dax = 2 * r - 11 * round;
            day = -round;
        }

        // Convert axial delta back to offset delta. Hex directions always produce
        // an even (day - dax), so dx is integer; dy then depends on hex.x parity
        // via the same row-shift rule used by GetHexPos.
        int32_t dx = (day - dax) / 2;
        int32_t cx_parity = hex.x & 1;
        int32_t shift = dx + cx_parity;
        int32_t adj = (shift < 0 ? shift - 1 : shift) / 2; // floor(shift / 2)
        int32_t dy = day - adj;

        hex = ipos32 {hex.x + dx, hex.y + dy};
    }
    else {
        // Square grid: ring R has 8R cells; HexesInRadius(R) = 1 + 4R(R+1).
        // R = smallest integer with 4R(R+1) >= index.
        float64_t fi = static_cast<float64_t>(index);
        int32_t round = static_cast<int32_t>(std::ceil((std::sqrt(1.0 + fi) - 1.0) / 2.0 - 1e-9));

        if (round < 1) {
            round = 1;
        }
        while (round > 1 && 4 * (round - 1) * round >= index) {
            round--;
        }
        while (4 * round * (round + 1) < index) {
            round++;
        }

        int32_t r = index - (1 + 4 * (round - 1) * round); // [0, 8*round - 1]

        // 4 sides of length 2R, walked SE, SW, NW, NE (raw-coord directions).
        // Square geometry has no parity quirk: raw coords ARE the lattice.
        int32_t drx;
        int32_t dry;

        if (r <= 2 * round) {
            drx = -round;
            dry = -round + r;
        }
        else if (r <= 4 * round) {
            drx = r - 3 * round;
            dry = round;
        }
        else if (r <= 6 * round) {
            drx = round;
            dry = 5 * round - r;
        }
        else {
            drx = 7 * round - r;
            dry = -round;
        }

        hex = ipos32 {hex.x + drx, hex.y + dry};
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

    float32_t x1_f = numeric_cast<float32_t>(x1);
    float32_t y1_f = numeric_cast<float32_t>(y1) * GetYProj();
    float32_t x2_f = numeric_cast<float32_t>(x2);
    float32_t y2_f = numeric_cast<float32_t>(y2) * GetYProj();

    float32_t angle = 90.0f + RAD_TO_DEG_FLOAT * std::atan2(y2_f - y1_f, x2_f - x1_f);

    if (angle < 0.0f) {
        angle += 360.0f;
    }
    if (angle >= 360.0f) {
        angle -= 360.0f;
    }

    FO_VERIFY_AND_THROW(angle >= 0.0f, "Angle is negative");
    FO_VERIFY_AND_THROW(angle < 360.0f, "Computed direction angle is outside the normalized degree range", angle, x1, y1, x2, y2);

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
        int32_t hx = (raw_hex.x < 0 ? raw_hex.x - 1 : raw_hex.x) / 2;
        int32_t x = raw_hex.y * (w / 2) - raw_hex.x * w + w / 2 * hx;
        int32_t y = raw_hex.y * h + h * hx;

        return {x, y};
    }
    else {
        int32_t x = (raw_hex.y - raw_hex.x) * w / 2;
        int32_t y = (raw_hex.y + raw_hex.x) * h;

        return {x, y};
    }
}

auto GeometryHelper::GetHexScreenRow(mpos hex) noexcept -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    // Row index of GetHexPos().y in MAP_HEX_LINE_HEIGHT units: hexes sharing it project to the same
    // screen row and therefore the same ground view depth (+2X/-1Y walks along one such row)
    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        return hex.y + hex.x / 2;
    }
    else {
        return hex.y + hex.x;
    }
}

auto GeometryHelper::GetHexWorldPos(mpos hex, ipos32 hex_offset, float32_t elevation) -> vec3
{
    FO_NO_STACK_TRACE_ENTRY();

    return GetHexWorldPos(ipos32(hex), hex_offset, elevation);
}

auto GeometryHelper::GetHexWorldPos(ipos32 raw_hex, ipos32 hex_offset, float32_t elevation) -> vec3
{
    FO_NO_STACK_TRACE_ENTRY();

    // Real-3D map-camera world frame: +X right, +Y up (elevation), +Z map-south, one world unit == one
    // pixel of hex spacing. The fixed map camera is a parallel (orthographic) projection that rigidly tilts
    // the world about X by MAP_CAMERA_ANGLE; ground northing is foreshortened by sin(angle) (== 1 /
    // GetYProj()) and elevation by cos(angle). Anchoring the ground point at z = legacy_y / sin(angle) makes
    // ProjectWorldToMap reproduce the legacy GetHexPos screen position exactly at elevation 0. Models render
    // in this same frame, so they need no extra transform (see
    // Docs/Plans/2026-05-29-sprites-real-3d-coordinates.md).
    ipos32 hex_pos = GetHexPos(raw_hex);
    float32_t sin_a = std::sin(GameSettings::MAP_CAMERA_ANGLE * DEG_TO_RAD_FLOAT);

    // Hex offsets are authored in legacy map-screen pixels. Treat them as movement along the ground plane:
    // +X stays +X, while +screenY is the camera-foreshortened projection of +worldZ.
    vec3 world_pos {numeric_cast<float32_t>(hex_pos.x), elevation, numeric_cast<float32_t>(hex_pos.y) / sin_a};
    world_pos.x += numeric_cast<float32_t>(hex_offset.x);
    world_pos.z += numeric_cast<float32_t>(hex_offset.y) / sin_a;
    return world_pos;
}

auto GeometryHelper::ProjectWorldToMap(vec3 world_pos) -> vec3
{
    FO_NO_STACK_TRACE_ENTRY();

    // Reference map-camera projection (no scroll/zoom): rigid tilt about X by MAP_CAMERA_ANGLE, parallel
    // projection. Returns map-space pixels in .x/.y (legacy convention, Y down) and view depth in .z (larger
    // == nearer the camera == drawn on top). The (.y, .z) pair is an orthonormal rotation of the world
    // (Z, Y) pair, so this is a true rigid camera tilt rather than a shear. The Phase 1 GPU view-projection
    // matrix must agree with this contract; it is pinned by Test_Geometry.cpp.
    float32_t angle_rad = GameSettings::MAP_CAMERA_ANGLE * DEG_TO_RAD_FLOAT;
    float32_t sin_a = std::sin(angle_rad);
    float32_t cos_a = std::cos(angle_rad);

    float32_t screen_x = world_pos.x;
    float32_t screen_y = sin_a * world_pos.z - cos_a * world_pos.y;
    float32_t depth = cos_a * world_pos.z + sin_a * world_pos.y;

    return vec3 {screen_x, screen_y, depth};
}

auto GeometryHelper::ProjectMapYToGroundDepth(float32_t map_y, float32_t elevation) -> float32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    float32_t angle_rad = GameSettings::MAP_CAMERA_ANGLE * DEG_TO_RAD_FLOAT;
    float32_t sin_a = std::sin(angle_rad);
    float32_t cos_a = std::cos(angle_rad);

    // Inverse of ProjectWorldToMap for a horizontal ground plane:
    // screen_y = sin(a) * world_z - cos(a) * elevation
    // depth = cos(a) * world_z + sin(a) * elevation
    return map_y * cos_a / sin_a + elevation / sin_a;
}

auto GeometryHelper::ProjectMapYToVerticalDepth(float32_t map_y, float32_t anchor_map_y, float32_t anchor_depth) -> float32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    float32_t angle_rad = GameSettings::MAP_CAMERA_ANGLE * DEG_TO_RAD_FLOAT;
    float32_t sin_a = std::sin(angle_rad);
    float32_t cos_a = std::cos(angle_rad);

    // Inverse of ProjectWorldToMap for a vertical plane at the anchor's ground Z:
    // screen_y - anchor_y = -cos(a) * height_delta
    // depth - anchor_depth = sin(a) * height_delta
    return anchor_depth - (map_y - anchor_map_y) * sin_a / cos_a;
}

auto GeometryHelper::MakeMapCameraView(float32_t camera_angle_deg, float32_t yaw_deg, fpos32 scroll_offset, float32_t zoom) -> mat44
{
    FO_NO_STACK_TRACE_ENTRY();

    // World -> map-screen pixels (.x/.y, Y down) + view depth (.z): the GPU form of ProjectWorldToMap with the
    // map camera's scroll (translate) and zoom (scale) folded in. `camera_angle_deg` is the fixed elevation
    // (pitch) above the ground (arcsin(sqrt(3)/4) == MAP_CAMERA_ANGLE keeps hexes metric-regular); `yaw_deg`
    // orbits the camera about the vertical (up) axis so a real 3D camera can rotate the scene around the same
    // elevation/roll. yaw == 0 reproduces the legacy fixed isometric view: result = (ProjectWorldToMap(world).xy
    // - scroll) * zoom on screen, depth unchanged. The renderer composes the backend ortho on top
    // (MapViewProj = CreateOrthoMatrix(0, w, h, 0, near, far) * MakeMapCameraView), giving one world->clip
    // matrix shared by sprites, 3D models and particles. 2D map sprites write per-vertex world depth and test it
    // with DepthFunc = LessEqual (the CPU painter sort still orders blended layers), so the shared GPU depth
    // buffer resolves occlusion across sprites, 3D models and particles alike. Pinned against
    // ProjectWorldToMap / GetHexPos by Test_Geometry.cpp.
    float32_t angle_rad = camera_angle_deg * DEG_TO_RAD_FLOAT;
    float32_t sin_a = std::sin(angle_rad);
    float32_t cos_a = std::cos(angle_rad);

    // Rigid tilt about X (map Y is down): screen_y = sin*z - cos*y, depth = cos*z + sin*y; screen_x = x.
    mat44 tilt {1.0f};
    tilt[1][1] = -cos_a;
    tilt[2][1] = sin_a;
    tilt[1][2] = sin_a;
    tilt[2][2] = cos_a;

    // Yaw about the world up axis (Y), applied before the tilt so the ground orbits under the fixed-elevation
    // camera. The vertical axis is invariant under yaw, so walls/models stay vertical on screen.
    mat44 yaw_mat = glm::rotate(mat44 {1.0f}, yaw_deg * DEG_TO_RAD_FLOAT, vec3 {0.0f, 1.0f, 0.0f});

    mat44 scroll_zoom = glm::scale(mat44 {1.0f}, vec3 {zoom, zoom, 1.0f}) * //
        glm::translate(mat44 {1.0f}, vec3 {-scroll_offset.x, -scroll_offset.y, 0.0f});

    return scroll_zoom * tilt * yaw_mat;
}

auto GeometryHelper::MakeMapAnchoredProj(const mat44& base_proj, const mat44& map_ortho, fpos32 anchor_pos, float32_t anchor_depth) -> mat44
{
    FO_NO_STACK_TRACE_ENTRY();

    // Shift `base_proj` in clip space so its local origin lands at the map-space anchor encoded by
    // `map_ortho`. This keeps direct-draw models and in-scene particle systems on the same root/depth formula.
    glm::vec4 origin_clip = base_proj * glm::vec4 {0.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4 anchor_clip = map_ortho * glm::vec4 {anchor_pos.x, anchor_pos.y, anchor_depth, 1.0f};
    vec3 clip_offset = vec3 {anchor_clip.x / anchor_clip.w - origin_clip.x / origin_clip.w, //
        anchor_clip.y / anchor_clip.w - origin_clip.y / origin_clip.w, //
        anchor_clip.z / anchor_clip.w - origin_clip.z / origin_clip.w};

    return glm::translate(mat44 {1.0f}, clip_offset) * base_proj;
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
    ipos32 hex_pos = GetHexPos(raw_hex);
    FO_VERIFY_AND_THROW(hex_pos.x % (w / 2) == 0, "Raw hex position X is not aligned to the axial coordinate grid", raw_hex, hex_pos, w / 2);
    FO_VERIFY_AND_THROW(hex_pos.y % h == 0, "Raw hex position Y is not aligned to the axial coordinate grid", raw_hex, hex_pos, h);

    return {hex_pos.x / (w / 2), hex_pos.y / h};
}

auto GeometryHelper::GetHexPosCoord(ipos32 pos, nptr<ipos32> hex_offset) -> ipos32
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
        float32_t fh = numeric_cast<float32_t>(h);
        float32_t fw = numeric_cast<float32_t>(w);
        float32_t fhw = numeric_cast<float32_t>(half_w);

        float32_t fa = numeric_cast<float32_t>(pos.y) / fh;
        float32_t fb = (numeric_cast<float32_t>(pos.x) - fa * fhw) / fw;
        float32_t fc = -(fa + fb);

        // Cube coordinate rounding
        int32_t ra = iround<int32_t>(fa);
        int32_t rb = iround<int32_t>(fb);
        int32_t rc = iround<int32_t>(fc);

        if (ra + rb + rc != 0) {
            float32_t da = std::abs(numeric_cast<float32_t>(ra) - fa);
            float32_t db = std::abs(numeric_cast<float32_t>(rb) - fb);
            float32_t dc = std::abs(numeric_cast<float32_t>(rc) - fc);

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
        constexpr int32_t limit = 2 * half_w * hq;

        auto is_inside_hex = [](int32_t lx, int32_t ly) -> bool { return std::abs(lx) <= half_w && std::abs(lx * hq - ly * half_w) <= limit && std::abs(lx * hq + ly * half_w) <= limit; };

        if (!is_inside_hex(dx, dy)) {
            // Check 6 neighbors in lattice space: (±1,0), (0,±1), (+1,-1), (-1,+1)
            static constexpr ipos32 neighbor_offsets[] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, -1}, {-1, 1}};

            for (const auto& off : neighbor_offsets) {
                int32_t na = ra + off.x;
                int32_t nb = rb + off.y;
                int32_t ncx = na * half_w + nb * w;
                int32_t ncy = na * h;
                int32_t ndx = pos.x - ncx;
                int32_t ndy = pos.y - ncy;

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
        int32_t rx = -rb;
        int32_t ry = ra - (rx < 0 ? rx - 1 : rx) / 2;
        ipos32 raw_hex = {rx, ry};

        if (hex_offset) {
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
        float32_t u = numeric_cast<float32_t>(pos.x) / numeric_cast<float32_t>(half_w) + numeric_cast<float32_t>(pos.y) / numeric_cast<float32_t>(h);
        float32_t v = -numeric_cast<float32_t>(pos.x) / numeric_cast<float32_t>(half_w) + numeric_cast<float32_t>(pos.y) / numeric_cast<float32_t>(h);

        int32_t ry = iround<int32_t>(u * 0.5f);
        int32_t rx = iround<int32_t>(v * 0.5f);
        ipos32 raw_hex = {rx, ry};

        if (hex_offset) {
            int32_t base_x = (ry - rx) * half_w;
            int32_t base_y = (ry + rx) * h;
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

auto GeometryHelper::NormalizeHexOffset(mpos& hex, ipos16& hex_offset, msize map_size) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    ipos32 world_pos = GetHexPos(hex) + ipos32(hex_offset);
    ipos32 normalized_offset;
    ipos32 normalized_raw_hex = GetHexPosCoord(world_pos, &normalized_offset);

    if (!map_size.is_valid_pos(normalized_raw_hex)) {
        return false;
    }

    hex = map_size.from_raw_pos(normalized_raw_hex);
    hex_offset = {numeric_cast<int16_t>(normalized_offset.x), numeric_cast<int16_t>(normalized_offset.y)};
    return true;
}

auto GeometryHelper::GetHexOffset(ipos32 from_raw_hex, ipos32 to_raw_hex) -> ipos32
{
    FO_NO_STACK_TRACE_ENTRY();

    constexpr int32_t w = GameSettings::MAP_HEX_WIDTH;
    constexpr int32_t h = GameSettings::MAP_HEX_LINE_HEIGHT;

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        int32_t dx = to_raw_hex.x - from_raw_hex.x;
        int32_t dy = to_raw_hex.y - from_raw_hex.y;
        int32_t dx2 = ((from_raw_hex.x % 2) != 0 ? (dx > 0 ? dx + 1 : dx) : (dx < 0 ? dx - 1 : dx)) / 2;
        int32_t x = dy * (w / 2) - dx * w + w / 2 * dx2;
        int32_t y = dy * h + h * dx2;

        return {x, y};
    }
    else {
        int32_t dx = to_raw_hex.x - from_raw_hex.x;
        int32_t dy = to_raw_hex.y - from_raw_hex.y;
        int32_t x = (dy - dx) * w / 2;
        int32_t y = (dy + dx) * h;

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

        int32_t dx = x / GameSettings::MAP_HEX_WIDTH;
        int32_t dy = y / GameSettings::MAP_HEX_LINE_HEIGHT;
        int32_t adx = std::abs(dx);
        int32_t ady = std::abs(dy);

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

        int32_t hw = std::abs(rw / (GameSettings::MAP_HEX_WIDTH / 2)) + ((rw % (GameSettings::MAP_HEX_WIDTH / 2)) != 0 ? 1 : 0) + (std::abs(rw) >= GameSettings::MAP_HEX_WIDTH / 2 ? 1 : 0); // Hexes width
        int32_t hh = std::abs(rh / GameSettings::MAP_HEX_LINE_HEIGHT) + ((rh % GameSettings::MAP_HEX_LINE_HEIGHT) != 0 ? 1 : 0) + (std::abs(rh) >= GameSettings::MAP_HEX_LINE_HEIGHT ? 1 : 0); // Hexes height
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

    ipos32 step_raw_hex = ipos32 {hex.x, hex.y};

    for (size_t i = 0; i < dir_line.size() / 2; i++) {
        hdir dir = hdir(dir_line[i * 2]);
        auto steps = dir_line[i * 2 + 1];

        for (uint8_t j = 0; j < steps; j++) {
            MoveHexByDirUnsafe(step_raw_hex, dir);

            if (map_size.is_valid_pos(step_raw_hex)) {
                mpos step_hex = map_size.from_raw_pos(step_raw_hex);

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

    int32_t x01 = x1 - cx;
    int32_t y01 = y1 - cy;
    int32_t x02 = x2 - cx;
    int32_t y02 = y2 - cy;
    int32_t dx = x02 - x01;
    int32_t dy = y02 - y01;
    int32_t a = dx * dx + dy * dy;
    int32_t b = 2 * (x01 * dx + y01 * dy);
    int32_t c = x01 * x01 + y01 * y01 - radius * radius;

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

    float32_t dx = numeric_cast<float32_t>(std::abs(to_pos.x - from_pos.x));
    float32_t dy = numeric_cast<float32_t>(std::abs(to_pos.y - from_pos.y));

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

    float32_t rad = deq * DEG_TO_RAD_FLOAT;
    float32_t x = pos.x * std::cos(rad) - pos.y * std::sin(rad);
    float32_t y = pos.x * std::sin(rad) + pos.y * std::cos(rad);

    return {x, y};
}

FO_END_NAMESPACE
