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

#include "catch_amalgamated.hpp"

#include "ExtendedTypes.h"
#include "TwoDimensionalGrid.h"

FO_BEGIN_NAMESPACE

TEST_CASE("TwoDimensionalGrid")
{
    SECTION("DynamicGridCreatesCellsOnWriteAndReturnsEmptyForMissing")
    {
        DynamicTwoDimensionalGrid<int32_t, ipos32, isize32> grid {{3, 3}};

        CHECK(grid.GetSize() == isize32 {3, 3});
        CHECK(grid.GetCellForReading({1, 1}) == 0);

        *grid.GetCellForWriting({1, 1}) = 42;

        CHECK(grid.GetCellForReading({1, 1}) == 42);
        CHECK(grid.GetCellForReading({2, 2}) == 0);
        CHECK(grid.GetCellForReading({5, 5}) == 0);
    }

    SECTION("DynamicGridResizeDropsCellsOutsideNewBounds")
    {
        DynamicTwoDimensionalGrid<int32_t, ipos32, isize32> grid {{4, 4}};

        *grid.GetCellForWriting({3, 0}) = 30;
        *grid.GetCellForWriting({0, 3}) = 40;
        *grid.GetCellForWriting({1, 1}) = 11;

        grid.Resize({2, 4});
        grid.Resize({4, 4});

        CHECK(grid.GetCellForReading({1, 1}) == 11);
        CHECK(grid.GetCellForReading({3, 0}) == 0);
        CHECK(grid.GetCellForReading({0, 3}) == 40);

        grid.Resize({4, 2});
        grid.Resize({4, 4});

        CHECK(grid.GetCellForReading({0, 3}) == 0);
        CHECK(grid.GetCellForReading({1, 1}) == 11);
    }

    SECTION("StaticGridPreservesOverlapAcrossResize")
    {
        StaticTwoDimensionalGrid<int32_t, ipos32, isize32> grid {{3, 3}};

        *grid.GetCellForWriting({0, 0}) = 7;
        *grid.GetCellForWriting({2, 2}) = 9;

        grid.Resize({4, 4});
        CHECK(grid.GetCellForReading({0, 0}) == 7);
        CHECK(grid.GetCellForReading({2, 2}) == 9);
        CHECK(grid.GetCellForReading({3, 3}) == 0);

        grid.Resize({2, 2});
        grid.Resize({4, 4});

        CHECK(grid.GetCellForReading({0, 0}) == 7);
        CHECK(grid.GetCellForReading({2, 2}) == 0);
    }

    SECTION("StaticGridOutOfRangeReadReturnsEmptyCell")
    {
        StaticTwoDimensionalGrid<int32_t, ipos32, isize32> grid {{2, 2}};

        CHECK(grid.GetCellForReading({-1, 0}) == 0);
        CHECK(grid.GetCellForReading({2, 1}) == 0);
    }
}

FO_END_NAMESPACE
