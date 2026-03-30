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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "catch_amalgamated.hpp"

#include "Timer.h"

FO_BEGIN_NAMESPACE

TEST_CASE("GameTimer")
{
    GlobalSettings settings {false};

    SECTION("SynchronizedTimeThrowsBeforeInitialization")
    {
        GameTimer timer {settings};

        CHECK_FALSE(timer.IsTimeSynchronized());
        CHECK_THROWS_AS(timer.GetSynchronizedTime(), TimeNotSyncException);
    }

    SECTION("FrameAdvanceUpdatesTimeAndDelta")
    {
        GameTimer timer {settings};
        const nanotime initial_time = timer.GetFrameTime();

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        timer.FrameAdvance();

        CHECK(timer.GetFrameTime() >= initial_time);
        CHECK(timer.GetFrameDeltaTime() >= timespan {});
    }

    SECTION("SynchronizedTimeMovesForwardAfterAdvance")
    {
        GameTimer timer {settings};
        const synctime sync_base {123456};

        timer.SetSynchronizedTime(sync_base);
        CHECK(timer.IsTimeSynchronized());
        CHECK(timer.GetSynchronizedTime() == sync_base);

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        timer.FrameAdvance();

        CHECK(timer.GetSynchronizedTime() >= sync_base);
    }

    SECTION("FramesPerSecondBecomesAvailableAfterOneSecondWindow")
    {
        GameTimer timer {settings};

        timer.FrameAdvance();
        CHECK(timer.GetFramesPerSecond() == 0);

        std::this_thread::sleep_for(std::chrono::milliseconds(1100));
        timer.FrameAdvance();

        CHECK(timer.GetFramesPerSecond() >= 1);
    }
}

FO_END_NAMESPACE
