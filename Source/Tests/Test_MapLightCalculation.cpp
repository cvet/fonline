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

#include "MapLightCalculation.h"
#include "WorkScheduler.h"

FO_BEGIN_NAMESPACE

TEST_CASE("MapLightCalculationFullIntensityCenter")
{
    MapLightCalculationInput input {.MapSize = {20, 20}, .Hex = {10, 10}, .Color = {255, 128, 64, 0}, .Distance = 4, .Intensity = -100, .CurIntensity = 100};
    vector<MapLightObstacle> cells(400);
    auto result = CalculateMapLight(input, {{0, 0}, {20, 20}, cells});
    REQUIRE_FALSE(result.Marks.empty());
    CHECK(result.Capacity == 100);
    CHECK(result.CenterColor == ucolor {255, 128, 64, 255});
    CHECK(result.Marks.front().Hex == input.Hex);
    CHECK(result.Marks.front().Color == ucolor {200, 100, 50, 0});
    CHECK_FALSE(result.FanHexes.empty());
}

TEST_CASE("MapLightCalculationDirectStreamsWithoutMarkQueue")
{
    MapLightCalculationInput input {.MapSize = {20, 20}, .Hex = {10, 10}, .Color = {255, 128, 64, 0}, .Distance = 4, .Intensity = -100, .CurIntensity = 100};
    size_t marks = 0;
    ucolor center;
    auto result = CalculateMapLightDirect(
        input, [](mpos) { return MapLightObstacle {}; },
        [&marks, &center, &input](mpos hex, ucolor color) {
            marks++;

            if (hex == input.Hex) {
                center = color;
            }
        });
    CHECK(marks > 1);
    CHECK(center == ucolor {200, 100, 50, 0});
    CHECK(result.Marks.empty());
    CHECK_FALSE(result.FanHexes.empty());
}

TEST_CASE("MapLightCalculationWorkerFailureDrainsAndRecovers")
{
    if constexpr (FO_WEB) {
        SKIP("This build has no worker threads");
    }

    WorkScheduler scheduler("LightFailure", 2);
    MapLightCalculationInput input {.MapSize = {20, 20}, .Hex = {10, 10}, .Color = {255, 128, 64, 0}, .Distance = 4, .Intensity = -100, .CurIntensity = 100};
    vector<MapLightObstacle> obstacles(400);
    vector<MapLightCalculationResult> results(16);
    std::atomic<int32_t> active {};
    auto calculate = [&](size_t index, bool fail) {
        active.fetch_add(1);
        auto leave = scope_exit([&active]() noexcept { active.fetch_sub(1); });
        results[index] = CalculateMapLight(input, {{0, 0}, {20, 20}, fail && index == 0 ? const_span<MapLightObstacle> {} : const_span<MapLightObstacle> {obstacles}});
    };
    CHECK_THROWS(scheduler.RunBatch("LightFailure", results.size(), 2, [&](size_t index) { calculate(index, true); }));
    CHECK(active.load() == 0);
    REQUIRE_NOTHROW(scheduler.RunBatch("LightRecovery", results.size(), 2, [&](size_t index) { calculate(index, false); }));
    CHECK(active.load() == 0);
    CHECK(scheduler.GetDiagnostics().ParallelBatches == 2);

    for (const auto& result : results) {
        REQUIRE_FALSE(result.Marks.empty());
        CHECK(result.Marks.front().Color == ucolor {200, 100, 50, 0});
    }
}

FO_END_NAMESPACE

