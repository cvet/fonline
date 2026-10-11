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

    SECTION("ChunkedGridKeepsCellAddressesAcrossChunkAllocations")
    {
        ChunkedTwoDimensionalGrid<int32_t, ipos32, isize32, 16> grid {{33, 35}};
        ptr<int32_t> first_cell = grid.GetCellForWriting({15, 15});
        *first_cell = 42;

        for (ipos32 pos : {ipos32 {16, 15}, ipos32 {15, 16}, ipos32 {16, 16}, ipos32 {32, 34}}) {
            CHECK(grid.GetCellForReading(pos) == 0);
            *grid.GetCellForWriting(pos) = 7;
            CHECK(grid.GetCellForReading(pos) == 7);
            CHECK(grid.GetCellForWriting({15, 15}) == first_cell);
        }

        CHECK(grid.GetCellForReading({15, 15}) == 42);
        CHECK(grid.GetCellForReading({14, 15}) == 0);
        CHECK(grid.GetCellForReading({-1, 0}) == 0);
        CHECK(grid.GetCellForReading({33, 34}) == 0);
        CHECK_THROWS(grid.GetCellForWriting({33, 34}));
    }

    SECTION("ChunkedGridResizePreservesCoordinatesAndDropsPartialChunkEdges")
    {
        ChunkedTwoDimensionalGrid<int32_t, ipos32, isize32, 16> grid {{33, 35}};
        *grid.GetCellForWriting({16, 16}) = 11;
        *grid.GetCellForWriting({17, 16}) = 12;
        *grid.GetCellForWriting({16, 17}) = 13;
        *grid.GetCellForWriting({32, 34}) = 14;

        grid.Resize({17, 17});
        grid.Resize({65, 65});

        CHECK(grid.GetSize() == isize32 {65, 65});
        CHECK(grid.GetCellForReading({16, 16}) == 11);
        CHECK(grid.GetCellForReading({17, 16}) == 0);
        CHECK(grid.GetCellForReading({16, 17}) == 0);
        CHECK(grid.GetCellForReading({32, 34}) == 0);
        CHECK_THROWS(grid.Resize({-1, 5}));
        CHECK(grid.GetSize() == isize32 {65, 65});

        grid.Resize({0, 0});
        grid.Resize({17, 17});
        CHECK(grid.GetCellForReading({16, 16}) == 0);
        *grid.GetCellForWriting({16, 16}) = 21;
        CHECK(grid.GetCellForReading({16, 16}) == 21);
    }

    SECTION("ChunkedGridResizeMovesCellOwnership")
    {
        ChunkedTwoDimensionalGrid<unique_nptr<int32_t>, ipos32, isize32, 16> grid {{17, 17}};
        *grid.GetCellForWriting({16, 16}) = safe_alloc::make_unique<int32_t>(42);
        grid.Resize({33, 33});
        REQUIRE(grid.GetCellForReading({16, 16}));
        CHECK(*grid.GetCellForReading({16, 16}) == 42);
    }

    SECTION("ChunkedGridUsesConfiguredChunkSideAcrossResize")
    {
        auto check_chunk_side = []<size_t ChunkSide>() {
            constexpr int32_t side = const_numeric_cast<int32_t>(ChunkSide);
            ChunkedTwoDimensionalGrid<int32_t, ipos32, isize32, ChunkSide> grid {{side * 2 + 1, side * 2 + 3}};
            ptr<int32_t> first_cell = grid.GetCellForWriting({side - 1, side - 1});
            *first_cell = 42;

            for (ipos32 pos : {ipos32 {side, side - 1}, ipos32 {side - 1, side}, ipos32 {side, side}, ipos32 {side * 2, side * 2 + 2}}) {
                CHECK(grid.GetCellForReading(pos) == 0);
                *grid.GetCellForWriting(pos) = 7;
                CHECK(grid.GetCellForReading(pos) == 7);
                CHECK(grid.GetCellForWriting({side - 1, side - 1}) == first_cell);
            }

            grid.Resize({side + 1, side + 1});
            grid.Resize({side * 3 + 3, side * 3 + 3});
            CHECK(grid.GetCellForReading({side - 1, side - 1}) == 42);
            CHECK(grid.GetCellForReading({side, side - 1}) == 7);
            CHECK(grid.GetCellForReading({side - 1, side}) == 7);
            CHECK(grid.GetCellForReading({side, side}) == 7);
            CHECK(grid.GetCellForReading({side * 2, side * 2 + 2}) == 0);
        };

        check_chunk_side.operator()<1>();
        check_chunk_side.operator()<8>();
        check_chunk_side.operator()<32>();
    }
}

FO_END_NAMESPACE
