// FOnline Engine
// SPDX-License-Identifier: MIT

#include "TwoDimensionalGrid.h"

FO_BEGIN_NAMESPACE

[[maybe_unused]] static void TestSpecializations()
{
    if constexpr (FO_DEBUG) {
        [[maybe_unused]] auto grid1 {StaticTwoDimensionalGrid<int32_t, ipos32, isize32>({100, 100})};
        [[maybe_unused]] auto grid2 {DynamicTwoDimensionalGrid<int32_t, ipos32, isize32>({100, 100})};
        [[maybe_unused]] auto grid3 {ChunkedTwoDimensionalGrid<int32_t, ipos32, isize32, 64>({100, 100})};
    }
}

FO_END_NAMESPACE
