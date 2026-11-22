// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE

// Signed map-coordinate position exposed to scripts as mutable x and y components; mcoord is 16-bit, or 32-bit with FO_MAP_WIDE_COORDS
// x: Signed X component of the map-cell coordinate
// y: Signed Y component of the map-cell coordinate
///@ ExportValueType Layout = mcoord-x+mcoord-y
struct mpos : ipos<mcoord_t>
{
    constexpr mpos() noexcept = default;
    constexpr mpos(mcoord_t x_, mcoord_t y_) noexcept :
        ipos(x_, y_)
    {
    }
};
static_assert(sizeof(mpos) == sizeof(mcoord_t) * 2 && std::is_standard_layout_v<mpos>);
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE mpos, "{} {}", value.x, value.y);
FO_DECLARE_TYPE_PARSER(FO_NAMESPACE mpos, value.x >> value.y);
FO_DECLARE_TYPE_HASHER(FO_NAMESPACE mpos);

// Signed map size exposed to scripts as mutable width and height components; mcoord is 16-bit, or 32-bit with FO_MAP_WIDE_COORDS
// width: Map width in addressable cells
// height: Map height in addressable cells
///@ ExportValueType Layout = mcoord-width+mcoord-height
struct msize : isize<mcoord_t>
{
    constexpr msize() noexcept = default;
    constexpr msize(mcoord_t width_, mcoord_t height_) noexcept :
        isize(width_, height_)
    {
    }

    [[nodiscard]] constexpr auto clamp_pos(std::integral auto x, std::integral auto y) const -> mpos
    {
        FO_VERIFY_AND_THROW(width > 0, "Width must be positive", width);
        FO_VERIFY_AND_THROW(height > 0, "Height must be positive", height);
        auto clamped_x {numeric_cast<mcoord_t>(std::clamp(numeric_cast<int32_t>(x), 0, width - 1))};
        auto clamped_y {numeric_cast<mcoord_t>(std::clamp(numeric_cast<int32_t>(y), 0, height - 1))};
        return {clamped_x, clamped_y};
    }

    [[nodiscard]] constexpr auto clamp_pos(pos_type auto pos) const -> mpos
    {
        FO_VERIFY_AND_THROW(width > 0, "Width must be positive", width);
        FO_VERIFY_AND_THROW(height > 0, "Height must be positive", height);
        auto clamped_x {numeric_cast<mcoord_t>(std::clamp(numeric_cast<int32_t>(pos.x), 0, width - 1))};
        auto clamped_y {numeric_cast<mcoord_t>(std::clamp(numeric_cast<int32_t>(pos.y), 0, height - 1))};
        return {clamped_x, clamped_y};
    }

    [[nodiscard]] constexpr auto from_raw_pos(std::integral auto x, std::integral auto y) const -> mpos
    {
        FO_VERIFY_AND_THROW(width > 0, "Width must be positive", width);
        FO_VERIFY_AND_THROW(height > 0, "Height must be positive", height);
        FO_VERIFY_AND_THROW(x >= 0, "X is negative", x);
        FO_VERIFY_AND_THROW(y >= 0, "Y is negative", y);
        FO_VERIFY_AND_THROW(x < width, "X is outside allowed range", x, width);
        FO_VERIFY_AND_THROW(y < height, "Y is outside allowed range", y, height);
        return {numeric_cast<mcoord_t>(x), numeric_cast<mcoord_t>(y)};
    }

    [[nodiscard]] constexpr auto from_raw_pos(pos_type auto pos) const -> mpos
    {
        FO_VERIFY_AND_THROW(width > 0, "Width must be positive", width);
        FO_VERIFY_AND_THROW(height > 0, "Height must be positive", height);
        FO_VERIFY_AND_THROW(pos.x >= 0, "Position x is negative", pos.x);
        FO_VERIFY_AND_THROW(pos.y >= 0, "Position y is negative", pos.y);
        FO_VERIFY_AND_THROW(pos.x < width, "Position x is outside allowed range", pos.x, width);
        FO_VERIFY_AND_THROW(pos.y < height, "Position y is outside allowed range", pos.y, height);
        return {numeric_cast<mcoord_t>(pos.x), numeric_cast<mcoord_t>(pos.y)};
    }
};
static_assert(sizeof(msize) == sizeof(mcoord_t) * 2 && std::is_standard_layout_v<msize>);
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE msize, "{} {}", value.width, value.height);
FO_DECLARE_TYPE_PARSER(FO_NAMESPACE msize, value.width >> value.height);
FO_DECLARE_TYPE_HASHER(FO_NAMESPACE msize);

// Discrete map direction normalized modulo the configured six-direction hex or eight-direction square geometry
// value: Direction index normalized to 0 through the configured six- or eight-direction count minus one
///@ ExportValueType Layout = int8-value
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
FO_DECLARE_TYPE_PARSER_EXT(FO_NAMESPACE hdir, int32_t v {}, v, FO_NAMESPACE hdir(v));
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

// Map direction angle stored as a signed 16-bit value with conversion and rotation operations for discrete map directions
// angle: Direction angle in degrees normalized to the inclusive range 0 through 359
///@ ExportValueType Layout = int16-angle
class mdir
{
public:
    constexpr mdir() noexcept = default;
    explicit mdir(int32_t angle) noexcept;
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
FO_DECLARE_TYPE_PARSER_EXT(FO_NAMESPACE mdir, int32_t angle {}, angle, FO_NAMESPACE mdir(angle));
FO_DECLARE_TYPE_HASHER(FO_NAMESPACE mdir);

namespace GeometryHelper
{
    auto GetYProj() -> float32_t;
    auto GetLineDirAngle(int32_t x1, int32_t y1, int32_t x2, int32_t y2) -> float32_t;
    auto GetHexPos(mpos hex) -> ipos32;
    auto GetHexPos(ipos32 raw_hex) -> ipos32;
    auto GetHexScreenRow(mpos hex) noexcept -> int32_t;
    auto GetHexWorldPos(mpos hex, ipos32 hex_offset, float32_t elevation = 0.0f) -> vec3;
    auto GetHexWorldPos(ipos32 raw_hex, ipos32 hex_offset, float32_t elevation = 0.0f) -> vec3;
    auto ProjectWorldToMap(vec3 world_pos) -> vec3;
    auto GetHexMapDepth(ipos32 raw_hex, ipos32 hex_offset, float32_t elevation, int32_t origin_y) -> float32_t;
    auto ProjectMapYToGroundDepth(float32_t map_y, float32_t elevation = 0.0f) -> float32_t;
    auto ProjectMapYToVerticalDepth(float32_t map_y, float32_t anchor_map_y, float32_t anchor_depth) -> float32_t;
    auto MakeMapCameraView(float32_t camera_angle_deg, float32_t yaw_deg, fpos32 scroll_offset, float32_t zoom) -> mat44;
    auto MakeMapAnchoredProj(const mat44& base_proj, const mat44& map_ortho, fpos32 anchor_pos, float32_t anchor_depth) -> mat44;
    auto GetHexAxialCoord(mpos hex) -> ipos32;
    auto GetHexAxialCoord(ipos32 raw_hex) -> ipos32;
    auto GetHexPosCoord(ipos32 pos, nptr<ipos32> hex_offset = nullptr) -> ipos32;
    auto NormalizeHexOffset(mpos& hex, ipos16& hex_offset, msize map_size) -> bool;
    auto NormalizeHexOffset(mpos& hex, ipos16& hex_offset, msize map_size, const function<bool(mpos)>& is_movable) -> bool;
    auto GetHexOffset(mpos from_hex, mpos to_hex) -> ipos32;
    auto GetHexOffset(ipos32 from_raw_hex, ipos32 to_raw_hex) -> ipos32;
    auto GetAxialHexes(mpos from_hex, mpos to_hex, msize map_size) -> vector<mpos>;
    auto GetDistance(int32_t x1, int32_t y1, int32_t x2, int32_t y2) -> int32_t;
    auto GetDistance(mpos hex1, mpos hex2) -> int32_t;
    auto GetDistance(ipos32 hex1, ipos32 hex2) -> int32_t;
    auto GetHexDir(int32_t x1, int32_t y1, int32_t x2, int32_t y2) -> hdir;
    auto GetHexDir(int32_t x1, int32_t y1, int32_t x2, int32_t y2, float32_t offset) -> hdir;
    auto GetHexDir(mpos from_hex, mpos to_hex) -> hdir;
    auto GetHexDir(mpos from_hex, mpos to_hex, float32_t offset) -> hdir;
    auto GetDirAngle(int32_t x1, int32_t y1, int32_t x2, int32_t y2) -> float32_t;
    auto GetDirAngle(mpos from_hex, mpos to_hex) -> float32_t;
    auto GetDirAngleDiff(float32_t a1, float32_t a2) -> float32_t;
    auto GetDirAngleDiffSided(float32_t a1, float32_t a2) -> float32_t;
    auto CheckDist(mpos hex1, mpos hex2, int32_t dist) -> bool;
    auto HexesInRadius(int32_t radius) noexcept -> int32_t;
    auto IntersectCircleLine(int32_t cx, int32_t cy, int32_t radius, int32_t x1, int32_t y1, int32_t x2, int32_t y2) noexcept -> bool;
    auto GetStepsCoords(ipos32 from_pos, ipos32 to_pos) noexcept -> fpos32;
    auto ChangeStepsCoords(fpos32 pos, float32_t deq) noexcept -> fpos32;

    auto MoveHexByDir(mpos& hex, mdir dir, msize map_size) -> bool;
    void MoveHexByDirUnsafe(ipos32& hex, mdir dir) noexcept;
    auto MoveHexAroundAway(mpos& hex, int32_t index, msize map_size) -> bool;
    void MoveHexAroundAwayUnsafe(ipos32& hex, int32_t index);
    void ForEachMultihexLines(const_span<uint8_t> dir_line, mpos hex, msize map_size, const function<void(mpos)>& callback);
}

FO_END_NAMESPACE
