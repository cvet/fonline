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

#include "LinkDelay.h"

FO_BEGIN_NAMESPACE

namespace
{
    static auto At(int64_t milliseconds) -> nanotime
    {
        return nanotime {timespan {std::chrono::milliseconds {milliseconds}}};
    }
}

TEST_CASE("LinkDelayEstimator")
{
    LinkDelayEstimator estimator {std::chrono::milliseconds {30000}, std::chrono::milliseconds {150}, std::chrono::milliseconds {3000}};

    SECTION("RejectsUnrepresentableSenderTimesBeforeChangingSamples")
    {
        CHECK_THROWS(estimator.AddSample(std::numeric_limits<int64_t>::min(), At(1000)));
        CHECK_THROWS(estimator.AddSample(std::numeric_limits<int64_t>::max(), At(1000)));
        CHECK(estimator.GetSampleCount() == 0);

        CHECK(estimator.AddSample(0, At(1000)) == timespan::zero);
        CHECK_THROWS(estimator.AddSample(std::numeric_limits<int64_t>::min(), At(1001)));
        CHECK_THROWS(estimator.AddSample(std::numeric_limits<int64_t>::max(), At(1001)));
        CHECK(estimator.GetSampleCount() == 1);
        CHECK(estimator.AddSample(1, At(1001)) == timespan::zero);
    }

    SECTION("ExtremeNativeClockOffsetsKeepLatenessRepresentable")
    {
        constexpr int64_t max_stamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::nanoseconds::max()).count();
        constexpr int64_t min_stamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::nanoseconds::min()).count();

        CHECK(estimator.AddSample(max_stamp_ms, At(min_stamp_ms)) == timespan::zero);
        CHECK(estimator.AddSample(min_stamp_ms, At(min_stamp_ms + 1)) == timespan {std::chrono::milliseconds {max_stamp_ms}});
    }

    SECTION("LatenessIsMeasuredAgainstTheUsualTransitWhateverTheClockOffset")
    {
        // The sender's clock runs 5000 ms behind the receiver's; only the differences between samples matter
        CHECK(estimator.AddSample(1000, At(6040)) == timespan::zero);
        CHECK(estimator.AddSample(2000, At(7030)) == timespan::zero);
        CHECK(estimator.AddSample(3000, At(8055)) == timespan {std::chrono::milliseconds {15}});
        // Held by a stall: sent at 4000, delivered about three seconds later than the link usually takes
        CHECK(estimator.AddSample(4000, At(12030)) == timespan {std::chrono::milliseconds {2990}});
        CHECK(estimator.GetSampleCount() == 4);
    }

    SECTION("AStallBurstIsMeasuredAgainstTheLinkBeforeIt")
    {
        (void)estimator.AddSample(0, At(5000));
        (void)estimator.AddSample(500, At(5500));
        (void)estimator.AddSample(1000, At(6000));

        // Everything sent during the stall arrives at once; each message is as late as it was held
        CHECK(estimator.AddSample(4000, At(12000)) == timespan {std::chrono::milliseconds {3000}});
        CHECK(estimator.AddSample(4400, At(12000)) == timespan {std::chrono::milliseconds {2600}});
        CHECK(estimator.AddSample(4800, At(12000)) == timespan {std::chrono::milliseconds {2200}});
        CHECK(estimator.AddSample(12100, At(17100)) == timespan::zero);
    }

    SECTION("ARouteThatGotSlowerBecomesUsualAfterTheRebaseTime")
    {
        (void)estimator.AddSample(0, At(5000));
        (void)estimator.AddSample(500, At(5500));
        (void)estimator.AddSample(1000, At(6000));

        // Three hundred milliseconds slower from now on: late until the rebase time has passed, then usual
        CHECK(estimator.AddSample(10000, At(15300)) == timespan {std::chrono::milliseconds {300}});
        CHECK(estimator.AddSample(11500, At(16800)) == timespan {std::chrono::milliseconds {300}});
        CHECK(estimator.AddSample(13000, At(18300)) == timespan::zero);
        CHECK(estimator.AddSample(14000, At(19300)) == timespan::zero);
    }

    SECTION("SilenceDoesNotForgetTheTransit")
    {
        (void)estimator.AddSample(0, At(5000));
        (void)estimator.AddSample(500, At(5500));

        // A link quiet for longer than the window still takes what it took before
        CHECK(estimator.AddSample(60000, At(68000)) == timespan {std::chrono::milliseconds {3000}});
    }

    SECTION("BusySlowerLinkStillRebasesWithABoundedSampleQueue")
    {
        (void)estimator.AddSample(0, At(5000));

        for (int64_t sent = 10000; sent <= 15000; sent += 5) {
            timespan late = estimator.AddSample(sent, At(sent + 5300));

            if (sent >= 13000) {
                CHECK(late == timespan::zero);
            }

            CHECK(estimator.GetSampleCount() <= 512);
        }
    }

    SECTION("FirstSampleOfALinkIsNeverLate")
    {
        CHECK(estimator.AddSample(123456, At(999)) == timespan::zero);
    }

    SECTION("ResetStartsTheLinkAfresh")
    {
        (void)estimator.AddSample(0, At(10));
        estimator.Reset();

        CHECK(estimator.GetSampleCount() == 0);
        CHECK(estimator.AddSample(0, At(900)) == timespan::zero);
    }
}

TEST_CASE("LateCatchUp")
{
    timespan min_time = std::chrono::milliseconds {150};
    timespan max_time = std::chrono::milliseconds {10000};

    CHECK(EvaluateLateCatchUp(std::chrono::milliseconds {149}, min_time, max_time) == timespan::zero);
    CHECK(EvaluateLateCatchUp(std::chrono::milliseconds {150}, min_time, max_time) == timespan {std::chrono::milliseconds {150}});
    CHECK(EvaluateLateCatchUp(std::chrono::milliseconds {3000}, min_time, max_time) == timespan {std::chrono::milliseconds {3000}});
    CHECK(EvaluateLateCatchUp(std::chrono::milliseconds {60000}, min_time, max_time) == max_time);
}

FO_END_NAMESPACE
