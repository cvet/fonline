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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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

#include "Geometry.h"

FO_BEGIN_NAMESPACE

struct MapLightObstacle
{
    bool LightBlocked {};
    bool HasWall {};
    bool HasTransparentWall {};
    CornerType Corner {};
};

struct MapLightObstacleView
{
    ipos32 Origin {};
    isize32 Size {};
    const_span<MapLightObstacle> Cells {};
};

struct MapLightCalculationInput
{
    msize MapSize {};
    mpos Hex {};
    ucolor Color {};
    int32_t Distance {};
    int32_t Intensity {};
    int32_t CurIntensity {};
    bool Global {};
    bool Inverse {};
    uint16_t StopDirections {};
    int32_t MapCapacity {};
    int32_t GlobalCapacity {};
};

struct MapLightCalculationResult
{
    struct Mark
    {
        mpos Hex {};
        ucolor Color {};
    };

    int32_t Capacity {};
    ucolor CenterColor {};
    vector<Mark> Marks {};
    vector<tuple<mpos, uint8_t, bool>> FanHexes {};
};

// Worker entry: only values and a bounded immutable obstacle view
auto CalculateMapLight(const MapLightCalculationInput& input, const MapLightObstacleView& obstacles) -> MapLightCalculationResult;

// Owner-only entry: reads live obstacles without building a snapshot
auto CalculateMapLightDirect(const MapLightCalculationInput& input, const function<MapLightObstacle(mpos)>& read_obstacle, const function<void(mpos, ucolor)>& mark) -> MapLightCalculationResult;

FO_END_NAMESPACE

