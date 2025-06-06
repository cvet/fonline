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

#pragma once

#include "Common.h"

#include "Settings.h"

FO_BEGIN_NAMESPACE();

///@ ExportValueType mpos mpos HardStrong Layout = uint16-x+uint16-y
struct mpos
{
    constexpr mpos() noexcept = default;
    constexpr mpos(uint16 x_, uint16 y_) noexcept :
        x {x_},
        y {y_}
    {
    }

    [[nodiscard]] constexpr auto operator==(const mpos& other) const noexcept -> bool { return x == other.x && y == other.y; }
    [[nodiscard]] constexpr auto operator!=(const mpos& other) const noexcept -> bool { return x != other.x || y != other.y; }
    [[nodiscard]] constexpr auto operator+(const mpos& other) const -> mpos { return {numeric_cast<uint16>(x + other.x), numeric_cast<uint16>(y + other.y)}; }
    [[nodiscard]] constexpr auto operator-(const mpos& other) const -> mpos { return {numeric_cast<uint16>(x - other.x), numeric_cast<uint16>(y - other.y)}; }
    [[nodiscard]] constexpr auto operator*(const mpos& other) const -> mpos { return {numeric_cast<uint16>(x * other.x), numeric_cast<uint16>(y * other.y)}; }
    [[nodiscard]] constexpr auto operator/(const mpos& other) const -> mpos { return {numeric_cast<uint16>(x / other.x), numeric_cast<uint16>(y / other.y)}; }

    // Todo: make hex position customizable, to allow to add third Z coordinate
    uint16 x {};
    uint16 y {};
};
static_assert(std::is_standard_layout_v<mpos>);
static_assert(sizeof(mpos) == 4);
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE mpos, "{} {}", value.x, value.y);
FO_DECLARE_TYPE_PARSER(FO_NAMESPACE mpos, value.x >> value.y);
FO_DECLARE_TYPE_HASHER(FO_NAMESPACE mpos);

///@ ExportValueType msize msize HardStrong Layout = uint16-x+uint16-y
struct msize
{
    constexpr msize() noexcept = default;
    constexpr msize(uint16 width_, uint16 height_) noexcept :
        width {width_},
        height {height_}
    {
    }

    [[nodiscard]] constexpr auto operator==(const msize& other) const noexcept -> bool { return width == other.width && height == other.height; }
    [[nodiscard]] constexpr auto operator!=(const msize& other) const noexcept -> bool { return width != other.width || height != other.height; }
    [[nodiscard]] constexpr auto GetSquare() const -> int32 { return numeric_cast<int32>(width * height); }
    template<typename T>
    [[nodiscard]] constexpr auto IsValidPos(T pos) const noexcept -> bool
    {
        return pos.x >= 0 && pos.y >= 0 && pos.x < width && pos.y < height;
    }
    template<typename T>
    [[nodiscard]] constexpr auto ClampPos(T pos) const -> mpos
    {
        FO_RUNTIME_ASSERT(width > 0);
        FO_RUNTIME_ASSERT(height > 0);
        return {numeric_cast<uint16>(std::clamp(numeric_cast<int32>(pos.x), 0, width - 1)), numeric_cast<uint16>(std::clamp(numeric_cast<int32>(pos.y), 0, height - 1))};
    }
    template<typename T>
    [[nodiscard]] constexpr auto FromRawPos(T pos) const -> mpos
    {
        FO_RUNTIME_ASSERT(width > 0);
        FO_RUNTIME_ASSERT(height > 0);
        FO_RUNTIME_ASSERT(pos.x >= 0);
        FO_RUNTIME_ASSERT(pos.y >= 0);
        FO_RUNTIME_ASSERT(pos.x < width);
        FO_RUNTIME_ASSERT(pos.y < height);
        return {numeric_cast<uint16>(pos.x), numeric_cast<uint16>(pos.y)};
    }

    uint16 width {};
    uint16 height {};
};
static_assert(std::is_standard_layout_v<msize>);
static_assert(sizeof(msize) == 4);
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE msize, "{} {}", value.width, value.height);
FO_DECLARE_TYPE_PARSER(FO_NAMESPACE msize, value.width >> value.height);
FO_DECLARE_TYPE_HASHER(FO_NAMESPACE msize);

constexpr auto MAX_HEX_OFFSET = 50; // Todo: remove hex offset limit

class GeometryHelper final
{
public:
    explicit GeometryHelper(GeometrySettings& settings);
    GeometryHelper(const GeometryHelper&) = delete;
    GeometryHelper(GeometryHelper&&) = default;
    auto operator=(const GeometryHelper&) -> GeometryHelper& = delete;
    auto operator=(GeometryHelper&&) -> GeometryHelper& = delete;
    ~GeometryHelper() = default;

    // Todo: move all geometry helper methods to static
    [[nodiscard]] auto GetYProj() const -> float32;
    [[nodiscard]] auto GetLineDirAngle(int32 x1, int32 y1, int32 x2, int32 y2) const -> float32;
    [[nodiscard]] auto GetHexOffsets(mpos hex) const -> tuple<const int16*, const int16*>;
    [[nodiscard]] auto GetHexInterval(mpos from_hex, mpos to_hex) const -> ipos;
    [[nodiscard]] auto GetHexInterval(ipos from_raw_hex, ipos to_raw_hex) const -> ipos;

    [[nodiscard]] static auto DistGame(int32 x1, int32 y1, int32 x2, int32 y2) -> int32;
    [[nodiscard]] static auto DistGame(mpos hex1, mpos hex2) -> int32;
    [[nodiscard]] static auto GetNearDir(int32 x1, int32 y1, int32 x2, int32 y2) -> uint8;
    [[nodiscard]] static auto GetNearDir(mpos from_hex, mpos to_hex) -> uint8;
    [[nodiscard]] static auto GetFarDir(int32 x1, int32 y1, int32 x2, int32 y2) -> uint8;
    [[nodiscard]] static auto GetFarDir(int32 x1, int32 y1, int32 x2, int32 y2, float32 offset) -> uint8;
    [[nodiscard]] static auto GetFarDir(mpos from_hex, mpos to_hex) -> uint8;
    [[nodiscard]] static auto GetFarDir(mpos from_hex, mpos to_hex, float32 offset) -> uint8;
    [[nodiscard]] static auto GetDirAngle(int32 x1, int32 y1, int32 x2, int32 y2) -> float32;
    [[nodiscard]] static auto GetDirAngle(mpos from_hex, mpos to_hex) -> float32;
    [[nodiscard]] static auto GetDirAngleDiff(float32 a1, float32 a2) -> float32;
    [[nodiscard]] static auto GetDirAngleDiffSided(float32 a1, float32 a2) -> float32;
    [[nodiscard]] static auto DirToAngle(uint8 dir) -> int16;
    [[nodiscard]] static auto AngleToDir(int16 dir_angle) -> uint8;
    [[nodiscard]] static auto NormalizeAngle(int16 dir_angle) -> int16;
    [[nodiscard]] static auto CheckDist(mpos hex1, mpos hex2, int32 dist) -> bool;
    [[nodiscard]] static auto ReverseDir(uint8 dir) -> uint8;

    static auto MoveHexByDir(mpos& hex, uint8 dir, msize map_size) -> bool;
    static auto MoveHexByDirUnsafe(ipos& hex, uint8 dir, msize map_size) -> bool;
    static void MoveHexByDirUnsafe(ipos& hex, uint8 dir);
    static void ForEachBlockLines(const vector<uint8>& lines, mpos hex, msize map_size, const function<void(mpos)>& callback);

private:
    void InitializeHexOffsets() const;

    GeometrySettings& _settings;
    mutable unique_ptr<int16[]> _sxEven {};
    mutable unique_ptr<int16[]> _syEven {};
    mutable unique_ptr<int16[]> _sxOdd {};
    mutable unique_ptr<int16[]> _syOdd {};
};

FO_END_NAMESPACE();
