//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

constexpr auto MAX_HEX_OFFSET = 50; // Must be not an odd

class GeometryHelper final
{
public:
    GeometryHelper() = delete;
    explicit GeometryHelper(GeometrySettings& settings);
    GeometryHelper(const GeometryHelper&) = delete;
    GeometryHelper(GeometryHelper&&) = default;
    auto operator=(const GeometryHelper&) -> GeometryHelper& = delete;
    auto operator=(GeometryHelper &&) -> GeometryHelper& = delete;
    ~GeometryHelper();

    [[nodiscard]] auto DistGame(int x1, int y1, int x2, int y2) const -> uint;
    [[nodiscard]] auto GetNearDir(int x1, int y1, int x2, int y2) const -> uchar;
    [[nodiscard]] auto GetFarDir(int x1, int y1, int x2, int y2) const -> uchar;
    [[nodiscard]] auto GetFarDir(int x1, int y1, int x2, int y2, float offset) const -> uchar;
    [[nodiscard]] auto CheckDist(ushort x1, ushort y1, ushort x2, ushort y2, uint dist) const -> bool;
    [[nodiscard]] auto ReverseDir(uchar dir) const -> uchar;
    [[nodiscard]] auto GetHexOffsets(bool odd) const -> tuple<const short*, const short*>;
    [[nodiscard]] auto GetHexInterval(int from_hx, int from_hy, int to_hx, int to_hy) const -> tuple<int, int>;

    auto MoveHexByDir(ushort& hx, ushort& hy, uchar dir, ushort maxhx, ushort maxhy) const -> bool;
    auto MoveHexByDirUnsafe(int& hx, int& hy, uchar dir, ushort maxhx, ushort maxhy) const -> bool;
    void MoveHexByDirUnsafe(int& hx, int& hy, uchar dir) const;
    void ForEachBlockLines(const vector<uchar>& lines, ushort hx, ushort hy, ushort maxhx, ushort maxhy, const std::function<void(ushort, ushort)>& work) const;

private:
    void InitializeHexOffsets() const;

    GeometrySettings& _settings;
    mutable short* _sxEven {};
    mutable short* _syEven {};
    mutable short* _sxOdd {};
    mutable short* _syOdd {};
};
