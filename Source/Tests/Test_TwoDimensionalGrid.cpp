// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

#include "catch_amalgamated.hpp"

#include "ExtendedTypes.h"
#include "TwoDimensionalGrid.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE ipos<int64_t>, "{} {}", value.x, value.y);
FO_DECLARE_TYPE_FORMATTER(FO_NAMESPACE isize<int64_t>, "{} {}", value.width, value.height);

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
        ptr<int32_t> first_cell {grid.GetCellForWriting({15, 15})};
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
        auto check_chunk_side {[]<size_t ChunkSide>() {
            constexpr int32_t side {const_numeric_cast<int32_t>(ChunkSide)};
            ChunkedTwoDimensionalGrid<int32_t, ipos32, isize32, ChunkSide> grid {{side * 2 + 1, side * 2 + 3}};
            ptr<int32_t> first_cell {grid.GetCellForWriting({side - 1, side - 1})};
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
        }};

        check_chunk_side.operator()<1>();
        check_chunk_side.operator()<8>();
        check_chunk_side.operator()<32>();
    }

    SECTION("ChunkedGridIndexFollowsTouchedAreaOnContinentSizedMaps")
    {
        // 128,000 cells per side with 16-cell chunks would need a 512 MB flat chunk index
        ChunkedTwoDimensionalGrid<int32_t, ipos32, isize32, 16> grid {{128000, 128000}};
        CHECK(grid.GetAllocatedChunkCount() == 0);
        CHECK(grid.GetCellForReading({127999, 127999}) == 0);

        *grid.GetCellForWriting({0, 0}) = 1;
        *grid.GetCellForWriting({127999, 127999}) = 2;
        *grid.GetCellForWriting({64000, 64015}) = 3;

        CHECK(grid.GetAllocatedChunkCount() == 3);
        CHECK(grid.GetCellForReading({0, 0}) == 1);
        CHECK(grid.GetCellForReading({127999, 127999}) == 2);
        CHECK(grid.GetCellForReading({64000, 64015}) == 3);
        CHECK(grid.GetCellForReading({64000, 64016}) == 0);
    }

    SECTION("ChunkedGridClearAreaReleasesCoveredChunksAndResetsPartialOnes")
    {
        ChunkedTwoDimensionalGrid<unique_nptr<int32_t>, ipos32, isize32, 16> grid {{256, 256}};

        for (int32_t y {0}; y < 64; y++) {
            for (int32_t x {0}; x < 64; x++) {
                *grid.GetCellForWriting({64 + x, 64 + y}) = safe_alloc::make_unique<int32_t>(x + y);
            }
        }

        *grid.GetCellForWriting({140, 70}) = safe_alloc::make_unique<int32_t>(7);
        REQUIRE(grid.GetAllocatedChunkCount() == 17);

        // A sector-shaped area: the 16 chunks inside are released, the neighbouring chunk keeps its other cells
        grid.ClearArea({64, 64}, {64, 64});
        CHECK(grid.GetAllocatedChunkCount() == 1);
        CHECK_FALSE(grid.GetCellForReading({64, 64}));
        CHECK_FALSE(grid.GetCellForReading({127, 127}));
        REQUIRE(grid.GetCellForReading({140, 70}));
        CHECK(*grid.GetCellForReading({140, 70}) == 7);

        *grid.GetCellForWriting({139, 70}) = safe_alloc::make_unique<int32_t>(8);
        grid.ClearArea({136, 64}, {4, 16});
        CHECK(grid.GetAllocatedChunkCount() == 1);
        CHECK_FALSE(grid.GetCellForReading({139, 70}));
        REQUIRE(grid.GetCellForReading({140, 70}));

        // Areas are clipped to the grid; empty and outside areas change nothing
        grid.ClearArea({-100, -100}, {0, 50});
        grid.ClearArea({300, 300}, {10, 10});
        CHECK(grid.GetAllocatedChunkCount() == 1);
        grid.ClearArea({-1000, -1000}, {5000, 5000});
        CHECK(grid.GetAllocatedChunkCount() == 0);
        CHECK_FALSE(grid.GetCellForReading({140, 70}));
    }

    SECTION("ChunkedGridLoadUnloadCyclesKeepStorageBounded")
    {
        ChunkedTwoDimensionalGrid<int32_t, ipos32, isize32, 16> grid {{128000, 128000}};

        for (int32_t step {0}; step < 100; step++) {
            ipos32 origin {step * 64, 64000};

            for (int32_t y {0}; y < 64; y += 4) {
                for (int32_t x {0}; x < 64; x += 4) {
                    *grid.GetCellForWriting({origin.x + x, origin.y + y}) = step + 1;
                }
            }

            if (step >= 2) {
                grid.ClearArea({(step - 2) * 64, 64000}, {64, 64});
            }

            CHECK(grid.GetAllocatedChunkCount() <= 3 * 16);
        }

        CHECK(grid.GetCellForReading({99 * 64, 64000}) == 100);
        CHECK(grid.GetCellForReading({97 * 64, 64000}) == 0);
    }

    SECTION("DynamicAndStaticGridsClearArea")
    {
        DynamicTwoDimensionalGrid<int32_t, ipos32, isize32> dynamic_grid {{100, 100}};
        StaticTwoDimensionalGrid<int32_t, ipos32, isize32> static_grid {{100, 100}};

        for (ipos32 pos : {ipos32 {10, 10}, ipos32 {19, 19}, ipos32 {20, 20}, ipos32 {99, 99}}) {
            *dynamic_grid.GetCellForWriting(pos) = 5;
            *static_grid.GetCellForWriting(pos) = 5;
        }

        dynamic_grid.ClearArea({10, 10}, {10, 10});
        static_grid.ClearArea({10, 10}, {10, 10});
        dynamic_grid.ClearArea({90, 90}, {1, 1}); // Smaller than the stored cell count: per-cell erase path

        auto check_cleared {[](const auto& grid) {
            CHECK(grid.GetCellForReading({10, 10}) == 0);
            CHECK(grid.GetCellForReading({19, 19}) == 0);
            CHECK(grid.GetCellForReading({20, 20}) == 5);
            CHECK(grid.GetCellForReading({99, 99}) == 5);
        }};

        check_cleared(dynamic_grid);
        check_cleared(static_grid);
    }

    SECTION("EveryGridRejectsNegativeSizeOnConstruction")
    {
        using DynamicGrid = DynamicTwoDimensionalGrid<int32_t, ipos32, isize32>;
        using StaticGrid = StaticTwoDimensionalGrid<int32_t, ipos32, isize32>;
        using ChunkedGrid = ChunkedTwoDimensionalGrid<int32_t, ipos32, isize32, 16>;

        CHECK_THROWS(DynamicGrid(isize32 {-1, 3}));
        CHECK_THROWS(StaticGrid(isize32 {3, -1}));
        CHECK_THROWS(ChunkedGrid(isize32 {-1, -1}));
    }
}

TEST_CASE("StaticGridOverflowResizePreservesPublishedSize")
{
    StaticTwoDimensionalGrid<int32_t, ipos<int64_t>, isize<int64_t>> grid {{3, 3}};
    *grid.GetCellForWriting({1, 1}) = 42;

    CHECK_THROWS_AS(grid.Resize({std::numeric_limits<int64_t>::max(), 2}), OverflowException);
    CHECK(grid.GetSize() == isize<int64_t> {3, 3});

    if (grid.GetSize() == isize<int64_t> {3, 3}) {
        CHECK(grid.GetCellForReading({1, 1}) == 42);
    }
}

FO_END_NAMESPACE
