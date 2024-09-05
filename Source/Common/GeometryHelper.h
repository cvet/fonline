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

#pragma once

#include "Common.h"

#include "Settings.h"

constexpr auto MAX_HEX_OFFSET = 50; // Todo: remove hex offset limit

class GeometryHelper final
{
public:
    GeometryHelper() = delete;
    explicit GeometryHelper(GeometrySettings& settings);
    GeometryHelper(const GeometryHelper&) = delete;
    GeometryHelper(GeometryHelper&&) = default;
    auto operator=(const GeometryHelper&) -> GeometryHelper& = delete;
    auto operator=(GeometryHelper&&) -> GeometryHelper& = delete;
    ~GeometryHelper();

    // Todo: move all geometry helper methods to static
    [[nodiscard]] auto GetYProj() const -> float;
    [[nodiscard]] auto GetLineDirAngle(int x1, int y1, int x2, int y2) const -> float;
    [[nodiscard]] auto GetHexOffsets(bool odd) const -> tuple<const int16*, const int16*>;
    [[nodiscard]] auto GetHexInterval(int from_hx, int from_hy, int to_hx, int to_hy) const -> tuple<int, int>;

    [[nodiscard]] static auto DistGame(int x1, int y1, int x2, int y2) -> uint;
    [[nodiscard]] static auto GetNearDir(int x1, int y1, int x2, int y2) -> uint8;
    [[nodiscard]] static auto GetFarDir(int x1, int y1, int x2, int y2) -> uint8;
    [[nodiscard]] static auto GetFarDir(int x1, int y1, int x2, int y2, float offset) -> uint8;
    [[nodiscard]] static auto GetDirAngle(int x1, int y1, int x2, int y2) -> float;
    [[nodiscard]] static auto GetDirAngleDiff(float a1, float a2) -> float;
    [[nodiscard]] static auto GetDirAngleDiffSided(float a1, float a2) -> float;
    [[nodiscard]] static auto DirToAngle(uint8 dir) -> int16;
    [[nodiscard]] static auto AngleToDir(int16 dir_angle) -> uint8;
    [[nodiscard]] static auto NormalizeAngle(int16 dir_angle) -> int16;
    [[nodiscard]] static auto CheckDist(uint16 x1, uint16 y1, uint16 x2, uint16 y2, uint dist) -> bool;
    [[nodiscard]] static auto ReverseDir(uint8 dir) -> uint8;

    static auto MoveHexByDir(uint16& hx, uint16& hy, uint8 dir, uint16 maxhx, uint16 maxhy) noexcept -> bool;
    static auto MoveHexByDirUnsafe(int& hx, int& hy, uint8 dir, uint16 maxhx, uint16 maxhy) noexcept -> bool;
    static void MoveHexByDirUnsafe(int& hx, int& hy, uint8 dir) noexcept;
    static void ForEachBlockLines(const vector<uint8>& lines, uint16 hx, uint16 hy, uint16 maxhx, uint16 maxhy, const std::function<void(uint16, uint16)>& work);

private:
    void InitializeHexOffsets() const;

    GeometrySettings& _settings;
    mutable int16* _sxEven {};
    mutable int16* _syEven {};
    mutable int16* _sxOdd {};
    mutable int16* _syOdd {};
};
