#include "catch_amalgamated.hpp"

#include "ExtendedTypes.h"
#include "TwoDimensionalGrid.h"

FO_BEGIN_NAMESPACE

TEST_CASE("TwoDimensionalGrid")
{
    SECTION("DynamicGridCreatesCellsOnWriteAndReturnsEmptyForMissing")
    {
        DynamicTwoDimensionalGrid<int32, ipos32, isize32> grid {{3, 3}};

        CHECK(grid.GetSize() == isize32 {3, 3});
        CHECK(grid.GetCellForReading({1, 1}) == 0);

        grid.GetCellForWriting({1, 1}) = 42;

        CHECK(grid.GetCellForReading({1, 1}) == 42);
        CHECK(grid.GetCellForReading({2, 2}) == 0);
        CHECK(grid.GetCellForReading({5, 5}) == 0);
    }

    SECTION("DynamicGridResizeDropsCellsOutsideNewBounds")
    {
        DynamicTwoDimensionalGrid<int32, ipos32, isize32> grid {{4, 4}};

        grid.GetCellForWriting({3, 0}) = 30;
        grid.GetCellForWriting({0, 3}) = 40;
        grid.GetCellForWriting({1, 1}) = 11;

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
        StaticTwoDimensionalGrid<int32, ipos32, isize32> grid {{3, 3}};

        grid.GetCellForWriting({0, 0}) = 7;
        grid.GetCellForWriting({2, 2}) = 9;

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
        StaticTwoDimensionalGrid<int32, ipos32, isize32> grid {{2, 2}};

        CHECK(grid.GetCellForReading({-1, 0}) == 0);
        CHECK(grid.GetCellForReading({2, 1}) == 0);
    }
}

FO_END_NAMESPACE
