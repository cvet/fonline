// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

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
        auto best {std::chrono::nanoseconds::max()};

        for (size_t i {0}; i < attempts; i++) {
            auto started {std::chrono::steady_clock::now()};
            sleeper();
            auto elapsed {std::chrono::steady_clock::now() - started};

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

            auto best {BestElapsed(20, [requested] { precise_sleep(requested); })};

            CHECK(best >= requested);
            CHECK(best < std::chrono::milliseconds {5});
        }
    }

    SECTION("PreciseSleepReachesItsDeadlineForLongerWaits")
    {
        auto best {BestElapsed(5, [] { precise_sleep(std::chrono::milliseconds {4}); })};

        CHECK(best >= std::chrono::milliseconds {4});
        CHECK(best < std::chrono::milliseconds {8});
    }

    SECTION("CoarseSleepLandsWithinHalfAMillisecond")
    {
        auto best {BestElapsed(10, [] { coarse_sleep(std::chrono::milliseconds {2}); })};

        CHECK(best >= std::chrono::microseconds {1500});
        CHECK(best < std::chrono::milliseconds {6});
    }

    SECTION("NonPositiveDurationReturnsImmediately")
    {
        auto started {std::chrono::steady_clock::now()};

        precise_sleep(std::chrono::nanoseconds::zero());
        coarse_sleep(std::chrono::nanoseconds::zero());
        precise_sleep(std::chrono::milliseconds {-5});
        coarse_sleep(std::chrono::milliseconds {-5});

        CHECK(std::chrono::steady_clock::now() - started < std::chrono::milliseconds {5});
    }
}

FO_END_NAMESPACE
