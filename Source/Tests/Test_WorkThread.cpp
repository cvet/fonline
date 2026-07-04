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

#include "ExceptionHandling.h"
#include "WorkThread.h"

FO_BEGIN_NAMESPACE

TEST_CASE("WorkThread")
{
    SECTION("RunThreadJoinConsumesHandle")
    {
        std::atomic_bool ran = false;
        thread worker = run_thread("JoinHandleWorker", [&] { ran = true; });

        REQUIRE(worker.joinable());

        worker.join();

        CHECK(ran.load());
        CHECK_FALSE(worker.joinable());
    }

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
            const int32_t next_run = ++runs;
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
        std::atomic_bool jobs_enqueued = false;

        worker.SetExceptionHandler([&](const std::exception&) {
            handler_called = true;
            return true;
        });

        // The throwing job must not run until the second job is queued. Otherwise the worker can pick
        // up the first job, throw, and let the handler clear an empty queue before the second job is
        // even added - the second job is then enqueued after the clear and runs normally (flaky).
        worker.AddJob([&]() -> optional<timespan> {
            jobs_enqueued.wait(false);
            throw std::runtime_error("boom");
        });
        worker.AddJob([&]() -> optional<timespan> {
            second_job_called = true;
            return std::nullopt;
        });
        jobs_enqueued = true;
        jobs_enqueued.notify_one();

        worker.Wait();

        CHECK(handler_called.load());
        CHECK_FALSE(second_job_called.load());
        CHECK(worker.GetJobsCount() == 0);
    }

    SECTION("ExceptionHandlerRunsBeforeGlobalExceptionReport")
    {
        const auto prev_callback = GetExceptionCallback();
        auto restore_callback = scope_exit([prev = std::move(prev_callback)]() mutable noexcept { SetExceptionCallback(std::move(prev)); });

        WorkThread worker {"ExceptionOrderWorker"};
        std::atomic_bool handler_called = false;
        std::atomic_bool report_called = false;
        std::atomic_bool report_saw_handler = false;

        SetExceptionCallback([&](string_view, const CatchedStackTraceData&, bool) {
            report_saw_handler = handler_called.load();
            report_called = true;
        });

        worker.SetExceptionHandler([&](const std::exception&) {
            handler_called = true;
            return true;
        });

        worker.AddJob([]() -> optional<timespan> { throw std::runtime_error("boom"); });

        worker.Wait();

        CHECK(handler_called.load());
        CHECK(report_called.load());
        CHECK(report_saw_handler.load());
    }

    SECTION("ClearRemovesQueuedJobsWhilePaused")
    {
        WorkThread worker {"ClearWorker"};
        std::atomic_bool executed = false;

        worker.Pause();
        worker.AddJob([&]() -> optional<timespan> {
            executed = true;
            return std::nullopt;
        });

        CHECK(worker.GetJobsCount() == 1);

        worker.Clear();
        CHECK(worker.GetJobsCount() == 0);

        worker.Resume();
        worker.Wait();

        CHECK_FALSE(executed.load());
    }

    SECTION("DiagnosticsTrackCompletedJobs")
    {
        WorkThread worker {"DiagnosticsWorker"};

        worker.AddJob([]() -> optional<timespan> { return std::nullopt; });
        worker.AddJob([]() -> optional<timespan> { return std::nullopt; });

        worker.Wait();

        const WorkThread::Diagnostics diagnostics = worker.GetDiagnostics();
        CHECK(diagnostics.CompletedJobs == 2);
        CHECK(diagnostics.QueuedJobs == 0);
        CHECK_FALSE(diagnostics.JobActive);
    }

    SECTION("DiagnosticsCountEveryRescheduledRun")
    {
        WorkThread worker {"RepeatDiagnosticsWorker"};
        std::atomic_int32_t runs = 0;

        worker.AddJob([&]() -> optional<timespan> {
            const int32_t next_run = ++runs;
            return next_run < 3 ? optional<timespan> {std::chrono::milliseconds {1}} : std::nullopt;
        });

        worker.Wait();

        // Each execution of the self-rescheduling job is counted, so the server's job-throughput
        // stats reflect every body run rather than the number of distinct submissions.
        CHECK(worker.GetDiagnostics().CompletedJobs == 3);
    }

    // Regression for a Pause() deadlock: the worker only signalled _doneSignal when its queue drained, so
    // a Pause() waiting on _jobActive was never woken while jobs were still queued behind the in-flight
    // one — hanging the pauser forever. Pause() must return as soon as the active job finishes, regardless
    // of how many jobs remain queued.
    SECTION("PauseReturnsWhileJobsRemainQueued")
    {
        WorkThread worker {"PauseDrainWorker"};
        std::atomic_bool gate {false};
        std::atomic_bool a_started {false};

        worker.AddJob([&]() -> optional<timespan> {
            a_started.store(true);
            while (!gate.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds {1});
            }
            return std::nullopt;
        });
        worker.AddJob([&]() -> optional<timespan> { return std::nullopt; }); // stays queued behind the gated job

        while (!a_started.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds {1});
        }

        // Pause from a helper thread while the first job is in flight and the second is still queued.
        thread pauser = run_thread("Pauser", [&]() { worker.Pause(); });
        gate.store(true); // let the in-flight job finish

        pauser.join(); // pre-fix: deadlocks here (the in-flight job cleared _jobActive but never signalled)
        CHECK_FALSE(worker.GetDiagnostics().JobActive);

        worker.Resume();
        worker.Wait();
        CHECK(worker.GetJobsCount() == 0);
    }

    // Faithful to production cross-thread use (e.g. `_healthWriter.AddJob` is called from the
    // `_mainWorker` thread while the control thread also drives the worker): several producer threads
    // hammer AddJob concurrently while a controller churns Pause/Resume/Clear and the worker runs the
    // bodies. The `_dataLocker`-guarded state machine must stay race-free (TSan), never use freed job
    // state (ASan), never deadlock, and remain functional afterwards. Bodies don't self-reschedule, so
    // Pause (which waits for the active job) can never block forever.
    SECTION("ConcurrentProducersHammerAddJobClearPauseResume")
    {
        WorkThread worker {"ChaosWorker"};
        std::atomic_int64_t body_runs {0};
        std::atomic_bool stop {false};

        constexpr int producer_count = 5;
        constexpr int jobs_per_producer = 3000;

        vector<thread> producers;
        producers.reserve(producer_count);

        for (int p = 0; p < producer_count; p++) {
            producers.emplace_back(run_thread("ChaosProducer", [&]() {
                for (int i = 0; i < jobs_per_producer; i++) {
                    worker.AddJob([&body_runs]() -> optional<timespan> {
                        body_runs.fetch_add(1, std::memory_order_relaxed);
                        return std::nullopt;
                    });
                }
            }));
        }

        thread controller = run_thread("ChaosController", [&]() {
            uint32_t rng = 0xC0FFEEU;
            const auto next = [&rng]() {
                rng = rng * 1664525U + 1013904223U;
                return rng;
            };

            while (!stop.load(std::memory_order_acquire)) {
                switch (next() % 3) {
                case 0:
                    worker.Pause();
                    worker.Resume();
                    break;
                case 1:
                    worker.Clear();
                    break;
                default:
                    std::this_thread::sleep_for(std::chrono::microseconds {50});
                    break;
                }
            }
        });

        for (auto& t : producers) {
            t.join();
        }

        stop.store(true, std::memory_order_release);
        controller.join();

        // Ensure the worker is not left paused, then drain whatever survived the Clear churn.
        worker.Resume();
        worker.Wait();
        CHECK(worker.GetJobsCount() == 0);

        // The worker must still be fully functional after the chaos — a fresh job runs to completion.
        std::atomic_bool final_ran {false};
        worker.AddJob([&final_ran]() -> optional<timespan> {
            final_ran.store(true);
            return std::nullopt;
        });
        worker.Wait();
        CHECK(final_ran.load());
    }
}

FO_END_NAMESPACE
