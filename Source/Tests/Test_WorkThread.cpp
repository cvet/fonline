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

#include "WorkThread.h"

FO_BEGIN_NAMESPACE

TEST_CASE("WorkThread")
{
    SECTION("ExecutesQueuedJobsAndWaitsForCompletion")
    {
        WorkThread worker {"TestWorker"};
        std::atomic_int32_t counter = 0;

        worker.AddJob([&]() -> optional<timespan> {
            counter.fetch_add(1);
            return std::nullopt;
        });
        worker.AddJob([&]() -> optional<timespan> {
            counter.fetch_add(2);
            return std::nullopt;
        });

        worker.Wait();

        CHECK(counter.load() == 3);
        CHECK(worker.GetJobsCount() == 0);
    }

    SECTION("PauseBlocksJobsUntilResume")
    {
        WorkThread worker {"PauseWorker"};
        std::atomic_bool executed = false;

        worker.Pause();
        worker.AddJob([&]() -> optional<timespan> {
            executed = true;
            return std::nullopt;
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        CHECK_FALSE(executed.load());

        worker.Resume();
        worker.Wait();

        CHECK(executed.load());
    }

    SECTION("RepeatedJobReschedulesUntilItStops")
    {
        WorkThread worker {"RepeatWorker"};
        std::atomic_int32_t runs = 0;

        worker.AddJob([&]() -> optional<timespan> {
            const int32 next_run = ++runs;
            return next_run < 3 ? optional<timespan> {std::chrono::milliseconds {1}} : std::nullopt;
        });

        worker.Wait();

        CHECK(runs.load() == 3);
    }

    SECTION("ExceptionHandlerCanClearRemainingJobs")
    {
        WorkThread worker {"ExceptionWorker"};
        std::atomic_bool handler_called = false;
        std::atomic_bool second_job_called = false;

        worker.SetExceptionHandler([&](const std::exception&) {
            handler_called = true;
            return true;
        });

        worker.AddJob([&]() -> optional<timespan> { throw std::runtime_error("boom"); });
        worker.AddJob([&]() -> optional<timespan> {
            second_job_called = true;
            return std::nullopt;
        });

        worker.Wait();

        CHECK(handler_called.load());
        CHECK_FALSE(second_job_called.load());
        CHECK(worker.GetJobsCount() == 0);
    }
}

FO_END_NAMESPACE
