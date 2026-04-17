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

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE

// Todo: make hex position customizable, to allow to add third Z coordinate
///@ ExportValueType mpos mpos Layout = int16-x+int16-y
struct mpos : ipos<int16_t>
{
    constexpr mpos() noexcept = default;
    constexpr mpos(int16_t x_, int16_t y_) noexcept :
        ipos(x_, y_)
    {
    }
};
static_assert(sizeof(mpos) == 4 && std::is_standard_layout_v<mpos>);
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE mpos, "{} {}", value.x, value.y);
FO_DECLARE_TYPE_PARSER(FO_NAMESPACE mpos, value.x >> value.y);
FO_DECLARE_TYPE_HASHER(FO_NAMESPACE mpos);

///@ ExportValueType msize msize Layout = int16-width+int16-height
struct msize : isize<int16_t>
{
    constexpr msize() noexcept = default;
    constexpr msize(int16_t width_, int16_t height_) noexcept :
        isize(width_, height_)
    {
    }

    [[nodiscard]] constexpr auto clamp_pos(std::integral auto x, std::integral auto y) const -> mpos
    {
        FO_RUNTIME_ASSERT(width > 0);
        FO_RUNTIME_ASSERT(height > 0);
        const auto clamped_x = numeric_cast<int16_t>(std::clamp(numeric_cast<int32_t>(x), 0, width - 1));
        const auto clamped_y = numeric_cast<int16_t>(std::clamp(numeric_cast<int32_t>(y), 0, height - 1));
        return {clamped_x, clamped_y};
    }
    [[nodiscard]] constexpr auto clamp_pos(pos_type auto pos) const -> mpos
    {
        FO_RUNTIME_ASSERT(width > 0);
        FO_RUNTIME_ASSERT(height > 0);
        const auto clamped_x = numeric_cast<int16_t>(std::clamp(numeric_cast<int32_t>(pos.x), 0, width - 1));
        const auto clamped_y = numeric_cast<int16_t>(std::clamp(numeric_cast<int32_t>(pos.y), 0, height - 1));
        return {clamped_x, clamped_y};
    }
    [[nodiscard]] constexpr auto from_raw_pos(std::integral auto x, std::integral auto y) const -> mpos
    {
        FO_RUNTIME_ASSERT(width > 0);
        FO_RUNTIME_ASSERT(height > 0);
        FO_RUNTIME_ASSERT(x >= 0);
        FO_RUNTIME_ASSERT(y >= 0);
        FO_RUNTIME_ASSERT(x < width);
        FO_RUNTIME_ASSERT(y < height);
        return {numeric_cast<int16_t>(x), numeric_cast<int16_t>(y)};
    }
    [[nodiscard]] constexpr auto from_raw_pos(pos_type auto pos) const -> mpos
    {
        FO_RUNTIME_ASSERT(width > 0);
        FO_RUNTIME_ASSERT(height > 0);
        FO_RUNTIME_ASSERT(pos.x >= 0);
        FO_RUNTIME_ASSERT(pos.y >= 0);
        FO_RUNTIME_ASSERT(pos.x < width);
        FO_RUNTIME_ASSERT(pos.y < height);
        return {numeric_cast<int16_t>(pos.x), numeric_cast<int16_t>(pos.y)};
    }
};
static_assert(sizeof(msize) == 4 && std ::is_standard_layout_v<msize>);
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE msize, "{} {}", value.width, value.height);
FO_DECLARE_TYPE_PARSER(FO_NAMESPACE msize, value.width >> value.height);
FO_DECLARE_TYPE_HASHER(FO_NAMESPACE msize);

///@ ExportValueType hdir hdir Layout = int8-value
class hdir
{
public:
    constexpr hdir() noexcept = default;
    constexpr explicit hdir(int32_t value) noexcept :
        _value(static_cast<int8_t>((value % GameSettings::MAP_DIR_COUNT + GameSettings::MAP_DIR_COUNT) % GameSettings::MAP_DIR_COUNT))
    {
    }

    [[nodiscard]] constexpr auto operator==(const hdir& other) const noexcept -> bool { return _value == other._value; }
    [[nodiscard]] constexpr auto operator!=(const hdir& other) const noexcept -> bool { return _value != other._value; }
    [[nodiscard]] constexpr auto value() const noexcept -> int8_t { return _value; }

    static const hdir NorthEast;
    static const hdir East;
    static const hdir SouthEast;
    static const hdir SouthWest;
    static const hdir West;
    static const hdir NorthWest;
#if FO_GEOMETRY == 2
    static const hdir South;
    static const hdir North;
#endif

private:
    int8_t _value {};
};
static_assert(sizeof(hdir) == 1 && std::is_standard_layout_v<hdir>);
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE hdir, "{}", value.value());
FO_DECLARE_TYPE_PARSER_EXT(FO_NAMESPACE hdir, int32_t v, v, FO_NAMESPACE hdir(v));
FO_DECLARE_TYPE_HASHER(FO_NAMESPACE hdir);

#if FO_GEOMETRY == 1
inline constexpr hdir hdir::NorthEast {0};
inline constexpr hdir hdir::East {1};
inline constexpr hdir hdir::SouthEast {2};
inline constexpr hdir hdir::SouthWest {3};
inline constexpr hdir hdir::West {4};
inline constexpr hdir hdir::NorthWest {5};
#elif FO_GEOMETRY == 2
inline constexpr hdir hdir::NorthEast {0};
inline constexpr hdir hdir::East {1};
inline constexpr hdir hdir::SouthEast {2};
inline constexpr hdir hdir::South {3};
inline constexpr hdir hdir::SouthWest {4};
inline constexpr hdir hdir::West {5};
inline constexpr hdir hdir::NorthWest {6};
inline constexpr hdir hdir::North {7};
#endif

///@ ExportValueType mdir mdir Layout = int16-angle
class mdir
{
public:
    constexpr mdir() noexcept = default;
    explicit mdir(int32_t angle) noexcept;
    // ReSharper disable once CppNonExplicitConvertingConstructor
    mdir(hdir dir) noexcept;

    [[nodiscard]] constexpr auto operator==(const mdir& other) const noexcept -> bool { return _value == other._value; }
    [[nodiscard]] constexpr auto operator!=(const mdir& other) const noexcept -> bool { return _value != other._value; }

    [[nodiscard]] constexpr auto angle() const noexcept -> int16_t { return _value; }
    [[nodiscard]] auto hex() const noexcept -> hdir;
    [[nodiscard]] auto incHex() const noexcept -> mdir;
    [[nodiscard]] auto decHex() const noexcept -> mdir;
    [[nodiscard]] auto rotateHex(int32_t steps) const noexcept -> mdir;
    [[nodiscard]] auto reverse() const noexcept -> mdir;

private:
    int16_t _value {};
};
static_assert(sizeof(mdir) == 2 && std::is_standard_layout_v<mdir>);
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE mdir, "{}", value.angle());
FO_DECLARE_TYPE_PARSER_EXT(FO_NAMESPACE mdir, int32_t angle, angle, FO_NAMESPACE mdir(angle));
FO_DECLARE_TYPE_HASHER(FO_NAMESPACE mdir);

class GeometryHelper final
{
public:
    GeometryHelper() = delete;
    GeometryHelper(const GeometryHelper&) = delete;
    GeometryHelper(GeometryHelper&&) = delete;
    auto operator=(const GeometryHelper&) -> GeometryHelper& = delete;
    auto operator=(GeometryHelper&&) -> GeometryHelper& = delete;
    ~GeometryHelper() = delete;

    [[nodiscard]] static auto GetYProj() -> float32_t;
    [[nodiscard]] static auto GetLineDirAngle(int32_t x1, int32_t y1, int32_t x2, int32_t y2) -> float32_t;
    [[nodiscard]] static auto GetHexPos(mpos hex) -> ipos32;
    [[nodiscard]] static auto GetHexPos(ipos32 raw_hex) -> ipos32;
    [[nodiscard]] static auto GetHexAxialCoord(mpos hex) -> ipos32;
    [[nodiscard]] static auto GetHexAxialCoord(ipos32 raw_hex) -> ipos32;
    [[nodiscard]] static auto GetHexPosCoord(ipos32 pos, ipos32* hex_offset = nullptr) -> ipos32;
    [[nodiscard]] static auto GetHexOffset(mpos from_hex, mpos to_hex) -> ipos32;
    [[nodiscard]] static auto GetHexOffset(ipos32 from_raw_hex, ipos32 to_raw_hex) -> ipos32;
    [[nodiscard]] static auto GetAxialHexes(mpos from_hex, mpos to_hex, msize map_size) -> vector<mpos>;
    [[nodiscard]] static auto GetDistance(int32_t x1, int32_t y1, int32_t x2, int32_t y2) -> int32_t;
    [[nodiscard]] static auto GetDistance(mpos hex1, mpos hex2) -> int32_t;
    [[nodiscard]] static auto GetDistance(ipos32 hex1, ipos32 hex2) -> int32_t;
    [[nodiscard]] static auto GetHexDir(int32_t x1, int32_t y1, int32_t x2, int32_t y2) -> hdir;
    [[nodiscard]] static auto GetHexDir(int32_t x1, int32_t y1, int32_t x2, int32_t y2, float32_t offset) -> hdir;
    [[nodiscard]] static auto GetHexDir(mpos from_hex, mpos to_hex) -> hdir;
    [[nodiscard]] static auto GetHexDir(mpos from_hex, mpos to_hex, float32_t offset) -> hdir;
    [[nodiscard]] static auto GetDirAngle(int32_t x1, int32_t y1, int32_t x2, int32_t y2) -> float32_t;
    [[nodiscard]] static auto GetDirAngle(mpos from_hex, mpos to_hex) -> float32_t;
    [[nodiscard]] static auto GetDirAngleDiff(float32_t a1, float32_t a2) -> float32_t;
    [[nodiscard]] static auto GetDirAngleDiffSided(float32_t a1, float32_t a2) -> float32_t;
    [[nodiscard]] static auto CheckDist(mpos hex1, mpos hex2, int32_t dist) -> bool;
    [[nodiscard]] static auto HexesInRadius(int32_t radius) -> int32_t;
    [[nodiscard]] static auto IntersectCircleLine(int32_t cx, int32_t cy, int32_t radius, int32_t x1, int32_t y1, int32_t x2, int32_t y2) noexcept -> bool;
    [[nodiscard]] static auto GetStepsCoords(ipos32 from_pos, ipos32 to_pos) noexcept -> fpos32;
    [[nodiscard]] static auto ChangeStepsCoords(fpos32 pos, float32_t deq) noexcept -> fpos32;

    static auto MoveHexByDir(mpos& hex, mdir dir, msize map_size) -> bool;
    static void MoveHexByDirUnsafe(ipos32& hex, mdir dir) noexcept;
    static auto MoveHexAroundAway(mpos& hex, int32_t index, msize map_size) -> bool;
    static void MoveHexAroundAwayUnsafe(ipos32& hex, int32_t index);
    static void ForEachMultihexLines(const_span<uint8_t> dir_line, mpos hex, msize map_size, const function<void(mpos)>& callback);
};

FO_END_NAMESPACE
