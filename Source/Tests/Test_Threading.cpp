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

#include "Threading.h"

FO_BEGIN_NAMESPACE

namespace
{
    // A preempted sample says nothing about the sleep mechanism, so the shortest of several attempts is what
    // gets asserted; the wrong mechanism (timer-tick rounding) cannot produce a short sample at all
    template<typename Func>
    auto BestElapsed(size_t attempts, Func&& sleeper) -> std::chrono::nanoseconds
    {
        auto best = std::chrono::nanoseconds::max();

        for (size_t i = 0; i < attempts; i++) {
            auto started = std::chrono::steady_clock::now();
            sleeper();
            auto elapsed = std::chrono::steady_clock::now() - started;

            best = std::min(best, std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed));
        }

        return best;
    }
}

TEST_CASE("Threading")
{
    SECTION("PreciseSleepDoesNotRoundUpToTheTimerTick")
    {
        // std::this_thread::sleep_for turns any of these into a full tick, 15 ms on a default Windows setup
        for (std::chrono::microseconds requested : {std::chrono::microseconds {100}, std::chrono::microseconds {250}, std::chrono::microseconds {500}}) {
            CAPTURE(requested.count());

            auto best = BestElapsed(20, [requested] { precise_sleep(requested); });

            CHECK(best >= requested);
            CHECK(best < std::chrono::milliseconds {5});
        }
    }

    SECTION("PreciseSleepReachesItsDeadlineForLongerWaits")
    {
        auto best = BestElapsed(5, [] { precise_sleep(std::chrono::milliseconds {4}); });

        CHECK(best >= std::chrono::milliseconds {4});
        CHECK(best < std::chrono::milliseconds {8});
    }

    SECTION("CoarseSleepLandsWithinHalfAMillisecond")
    {
        auto best = BestElapsed(10, [] { coarse_sleep(std::chrono::milliseconds {2}); });

        CHECK(best >= std::chrono::microseconds {1500});
        CHECK(best < std::chrono::milliseconds {6});
    }

    SECTION("NonPositiveDurationReturnsImmediately")
    {
        auto started = std::chrono::steady_clock::now();

        precise_sleep(std::chrono::nanoseconds::zero());
        coarse_sleep(std::chrono::nanoseconds::zero());
        precise_sleep(std::chrono::milliseconds {-5});
        coarse_sleep(std::chrono::milliseconds {-5});

        CHECK(std::chrono::steady_clock::now() - started < std::chrono::milliseconds {5});
    }
}

FO_END_NAMESPACE
