// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

#include "catch_amalgamated.hpp"

#include "Timer.h"

FO_BEGIN_NAMESPACE

TEST_CASE("GameTimer")
{
    GlobalSettings settings {false};

    SECTION("SynchronizedTimeThrowsBeforeInitialization")
    {
        GameTimer timer {&settings};

        CHECK_FALSE(timer.IsTimeSynchronized());
        CHECK_THROWS_AS(timer.GetSynchronizedTime(), TimeNotSyncException);
    }

    SECTION("FrameAdvanceUpdatesTimeAndDelta")
    {
        GameTimer timer {&settings};
        nanotime initial_time {timer.GetFrameTime()};

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        timer.FrameAdvance(false);

        CHECK(timer.GetFrameTime() >= initial_time);
        CHECK(timer.GetFrameDeltaTime() >= timespan::zero);
    }

    SECTION("SynchronizedTimeMovesForwardAfterAdvance")
    {
        GameTimer timer {&settings};
        synctime sync_base {123456};

        timer.SetSynchronizedTime(sync_base);
        CHECK(timer.IsTimeSynchronized());
        CHECK(timer.GetSynchronizedTime() == sync_base);

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        timer.FrameAdvance(false);

        CHECK(timer.GetSynchronizedTime() >= sync_base);
    }

    SECTION("PauseFreezesFrameAndSynchronizedTime")
    {
        GameTimer timer {&settings};
        synctime sync_base {654321};

        timer.SetSynchronizedTime(sync_base);
        timer.FrameAdvance(false);
        timer.Pause();

        CHECK(timer.IsPaused());
        CHECK_THROWS_AS(timer.Pause(), VerificationException);

        nanotime paused_frame_time {timer.GetFrameTime()};
        synctime paused_sync_time {timer.GetSynchronizedTime()};

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        timer.FrameAdvance(false);

        CHECK(timer.GetFrameTime() == paused_frame_time);
        CHECK(timer.GetFrameDeltaTime() == timespan::zero);
        CHECK(timer.GetSynchronizedTime() == paused_sync_time);

        timer.Resume();
        CHECK_FALSE(timer.IsPaused());
        CHECK_THROWS_AS(timer.Resume(), VerificationException);

        timer.FrameAdvance(false);
        CHECK(timer.GetFrameTime() >= paused_frame_time);
        CHECK(timer.GetSynchronizedTime() >= paused_sync_time);
    }

    SECTION("MonotonicSynchronizedTimeDoesNotRollbackAndCatchesUp")
    {
        GameTimer timer {&settings};
        synctime sync_base {123456};

        timer.SetSynchronizedTime(sync_base);

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        timer.FrameAdvance(false);

        synctime current_time {timer.GetSynchronizedTime()};
        synctime older_time {current_time.milliseconds() - 100};

        timer.SetSynchronizedTimeMonotonic(older_time);
        CHECK(timer.GetSynchronizedTime() == current_time);

        std::this_thread::sleep_for(std::chrono::milliseconds(120));
        timer.FrameAdvance(false);
        CHECK(timer.GetSynchronizedTime() > current_time);
    }

    SECTION("FramesPerSecondBecomesAvailableAfterOneSecondWindow")
    {
        GameTimer timer {&settings};

        timer.FrameAdvance(false);
        CHECK(timer.GetFramesPerSecond() == 0);

        std::this_thread::sleep_for(std::chrono::milliseconds(1100));
        timer.FrameAdvance(false);

        CHECK(timer.GetFramesPerSecond() >= 1);
    }
}

FO_END_NAMESPACE
