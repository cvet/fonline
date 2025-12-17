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

// Todo: make hex position customizable, to allow to add third Z coordinate
///@ ExportValueType mpos mpos Layout = int16-x+int16-y
struct mpos : ipos<int16>
{
    constexpr mpos() noexcept = default;
    constexpr mpos(int16 x_, int16 y_) noexcept :
        ipos(x_, y_)
    {
    }
};
static_assert(sizeof(mpos) == 4 && std::is_standard_layout_v<mpos>);
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE mpos, "{} {}", value.x, value.y);
FO_DECLARE_TYPE_PARSER(FO_NAMESPACE mpos, value.x >> value.y);
FO_DECLARE_TYPE_HASHER(FO_NAMESPACE mpos);

///@ ExportValueType msize msize Layout = int16-width+int16-height
struct msize : isize<int16>
{
    constexpr msize() noexcept = default;
    constexpr msize(int16 width_, int16 height_) noexcept :
        isize(width_, height_)
    {
    }

    [[nodiscard]] constexpr auto clamp_pos(std::integral auto x, std::integral auto y) const -> mpos
    {
        FO_RUNTIME_ASSERT(width > 0);
        FO_RUNTIME_ASSERT(height > 0);
        const auto clamped_x = numeric_cast<int16>(std::clamp(numeric_cast<int32>(x), 0, width - 1));
        const auto clamped_y = numeric_cast<int16>(std::clamp(numeric_cast<int32>(y), 0, height - 1));
        return {clamped_x, clamped_y};
    }
    [[nodiscard]] constexpr auto clamp_pos(pos_type auto pos) const -> mpos
    {
        FO_RUNTIME_ASSERT(width > 0);
        FO_RUNTIME_ASSERT(height > 0);
        const auto clamped_x = numeric_cast<int16>(std::clamp(numeric_cast<int32>(pos.x), 0, width - 1));
        const auto clamped_y = numeric_cast<int16>(std::clamp(numeric_cast<int32>(pos.y), 0, height - 1));
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
        return {numeric_cast<int16>(x), numeric_cast<int16>(y)};
    }
    [[nodiscard]] constexpr auto from_raw_pos(pos_type auto pos) const -> mpos
    {
        FO_RUNTIME_ASSERT(width > 0);
        FO_RUNTIME_ASSERT(height > 0);
        FO_RUNTIME_ASSERT(pos.x >= 0);
        FO_RUNTIME_ASSERT(pos.y >= 0);
        FO_RUNTIME_ASSERT(pos.x < width);
        FO_RUNTIME_ASSERT(pos.y < height);
        return {numeric_cast<int16>(pos.x), numeric_cast<int16>(pos.y)};
    }
};
static_assert(sizeof(msize) == 4 && std ::is_standard_layout_v<msize>);
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE msize, "{} {}", value.width, value.height);
FO_DECLARE_TYPE_PARSER(FO_NAMESPACE msize, value.width >> value.height);
FO_DECLARE_TYPE_HASHER(FO_NAMESPACE msize);

class GeometryHelper final
{
public:
    explicit GeometryHelper(GeometrySettings& settings);
    GeometryHelper(const GeometryHelper&) = delete;
    GeometryHelper(GeometryHelper&&) = default;
    auto operator=(const GeometryHelper&) -> GeometryHelper& = delete;
    auto operator=(GeometryHelper&&) -> GeometryHelper& = delete;
    ~GeometryHelper() = default;

    [[nodiscard]] auto GetYProj() const -> float32;
    [[nodiscard]] auto GetLineDirAngle(int32 x1, int32 y1, int32 x2, int32 y2) const -> float32;
    [[nodiscard]] auto GetHexPos(mpos hex) const -> ipos32;
    [[nodiscard]] auto GetHexPos(ipos32 raw_hex) const -> ipos32;
    [[nodiscard]] auto GetHexAxialCoord(mpos hex) const -> ipos32;
    [[nodiscard]] auto GetHexAxialCoord(ipos32 raw_hex) const -> ipos32;
    [[nodiscard]] auto GetHexPosCoord(ipos32 pos, ipos32* hex_offset = nullptr) const -> ipos32;
    [[nodiscard]] auto GetHexOffset(mpos from_hex, mpos to_hex) const -> ipos32;
    [[nodiscard]] auto GetHexOffset(ipos32 from_raw_hex, ipos32 to_raw_hex) const -> ipos32;
    [[nodiscard]] auto GetAxialHexes(mpos from_hex, mpos to_hex, msize map_size) -> vector<mpos>;

    [[nodiscard]] static auto GetDistance(int32 x1, int32 y1, int32 x2, int32 y2) -> int32;
    [[nodiscard]] static auto GetDistance(mpos hex1, mpos hex2) -> int32;
    [[nodiscard]] static auto GetDistance(ipos32 hex1, ipos32 hex2) -> int32;
    [[nodiscard]] static auto GetDir(int32 x1, int32 y1, int32 x2, int32 y2) -> uint8;
    [[nodiscard]] static auto GetDir(int32 x1, int32 y1, int32 x2, int32 y2, float32 offset) -> uint8;
    [[nodiscard]] static auto GetDir(mpos from_hex, mpos to_hex) -> uint8;
    [[nodiscard]] static auto GetDir(mpos from_hex, mpos to_hex, float32 offset) -> uint8;
    [[nodiscard]] static auto GetDirAngle(int32 x1, int32 y1, int32 x2, int32 y2) -> float32;
    [[nodiscard]] static auto GetDirAngle(mpos from_hex, mpos to_hex) -> float32;
    [[nodiscard]] static auto GetDirAngleDiff(float32 a1, float32 a2) -> float32;
    [[nodiscard]] static auto GetDirAngleDiffSided(float32 a1, float32 a2) -> float32;
    [[nodiscard]] static auto DirToAngle(uint8 dir) -> int16;
    [[nodiscard]] static auto AngleToDir(int16 dir_angle) -> uint8;
    [[nodiscard]] static auto NormalizeAngle(int16 dir_angle) -> int16;
    [[nodiscard]] static auto CheckDist(mpos hex1, mpos hex2, int32 dist) -> bool;
    [[nodiscard]] static auto ReverseDir(uint8 dir) -> uint8;
    [[nodiscard]] static auto HexesInRadius(int32 radius) -> int32;

    static auto MoveHexByDir(mpos& hex, uint8 dir, msize map_size) -> bool;
    static void MoveHexByDirUnsafe(ipos32& hex, uint8 dir) noexcept;
    static auto MoveHexAroundAway(mpos& hex, int32 index, msize map_size) -> bool;
    static void MoveHexAroundAwayUnsafe(ipos32& hex, int32 index);
    static void ForEachMultihexLines(span<const uint8> dir_line, mpos hex, msize map_size, const function<void(mpos)>& callback);

private:
    raw_ptr<GeometrySettings> _settings;
};

FO_END_NAMESPACE();
