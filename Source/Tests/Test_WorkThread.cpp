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

#include "ExceptionHandling.h"
#include "WorkThread.h"

FO_BEGIN_NAMESPACE

// A helper that stops completing work means clear() was stranded. Record the failure, then keep waking the
// worker through the public queue until the outstanding calls leave, so every join below still completes
static void JoinHelpersWithStallWatchdog(work_thread& worker, vector<thread>& helpers, int32_t helper_count, std::atomic_int32_t& helpers_done, std::atomic_int64_t& progress, std::atomic_bool& stop, std::atomic_bool& tripped)
{
    constexpr auto stall_limit = std::chrono::seconds {5};

    int64_t seen_progress = progress.load(std::memory_order_relaxed);
    auto stalled_since = std::chrono::steady_clock::now();

    while (helpers_done.load(std::memory_order_acquire) < helper_count) {
        int64_t now_progress = progress.load(std::memory_order_relaxed);

        if (now_progress != seen_progress) {
            seen_progress = now_progress;
            stalled_since = std::chrono::steady_clock::now();
        }
        else if (std::chrono::steady_clock::now() - stalled_since > stall_limit) {
            tripped.store(true);
            stop.store(true, std::memory_order_release);
        }

        if (tripped.load()) {
            // Keep waking until every in-flight clear leaves; ending rescue before join can strand a late request
            worker.add_job([]() -> optional<timespan> { return std::nullopt; });
        }

        coarse_sleep(std::chrono::milliseconds {1});
    }

    for (thread& helper : helpers) {
        helper.join();
    }
}

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
        work_thread worker {"TestWorker"};
        std::atomic_int32_t counter = 0;

        worker.add_job([&]() -> optional<timespan> {
            counter.fetch_add(1);
            return std::nullopt;
        });
        worker.add_job([&]() -> optional<timespan> {
            counter.fetch_add(2);
            return std::nullopt;
        });

        worker.wait();

        CHECK(counter.load() == 3);
        CHECK(worker.get_jobs_count() == 0);
    }

    SECTION("PauseBlocksJobsUntilResume")
    {
        work_thread worker {"PauseWorker"};
        std::atomic_bool executed = false;

        worker.pause();
        worker.add_job([&]() -> optional<timespan> {
            executed = true;
            return std::nullopt;
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        CHECK_FALSE(executed.load());

        worker.resume();
        worker.wait();

        CHECK(executed.load());
    }

    SECTION("RepeatedJobReschedulesUntilItStops")
    {
        work_thread worker {"RepeatWorker"};
        std::atomic_int32_t runs = 0;

        worker.add_job([&]() -> optional<timespan> {
            int32_t next_run = ++runs;
            return next_run < 3 ? optional<timespan> {std::chrono::milliseconds {1}} : std::nullopt;
        });

        worker.wait();

        CHECK(runs.load() == 3);
    }

    SECTION("ExceptionHandlerCanClearRemainingJobs")
    {
        work_thread worker {"ExceptionWorker"};
        std::atomic_bool handler_called = false;
        std::atomic_bool second_job_called = false;
        std::atomic_bool jobs_enqueued = false;

        worker.set_exception_handler([&](const std::exception&) {
            handler_called = true;
            return true;
        });

        // Queue the second job before releasing the thrower so exception cleanup must clear it
        worker.add_job([&]() -> optional<timespan> {
            jobs_enqueued.wait(false);
            throw std::runtime_error("boom");
        });
        worker.add_job([&]() -> optional<timespan> {
            second_job_called = true;
            return std::nullopt;
        });
        jobs_enqueued = true;
        jobs_enqueued.notify_one();

        worker.wait();

        CHECK(handler_called.load());
        CHECK_FALSE(second_job_called.load());
        CHECK(worker.get_jobs_count() == 0);
    }

    SECTION("AnExceptionHandlerTakesOverTheReporting")
    {
        // The handler knows what the failure means to its owner and when to say so. Reporting here as
        // well would print the same exception twice, the second copy behind everything the handler started
        auto prev_callback = exceptions::get_callback();
        auto restore_callback = scope_exit([prev = std::move(prev_callback)]() mutable noexcept { exceptions::set_callback(std::move(prev)); });

        work_thread worker {"ExceptionHandlerWorker"};
        std::atomic_bool handler_called = false;
        std::atomic_bool report_called = false;

        exceptions::set_callback([&](string_view, const stack_trace::catched_data&, bool) { report_called = true; });

        worker.set_exception_handler([&](const std::exception&) {
            handler_called = true;
            return true;
        });

        worker.add_job([]() -> optional<timespan> { throw std::runtime_error("boom"); });

        worker.wait();

        CHECK(handler_called.load());
        CHECK_FALSE(report_called.load());
    }

    SECTION("WithoutAHandlerTheExceptionIsStillReported")
    {
        auto prev_callback = exceptions::get_callback();
        auto restore_callback = scope_exit([prev = std::move(prev_callback)]() mutable noexcept { exceptions::set_callback(std::move(prev)); });

        work_thread worker {"ExceptionReportWorker"};
        std::atomic_bool report_called = false;

        exceptions::set_callback([&](string_view, const stack_trace::catched_data&, bool) { report_called = true; });

        worker.add_job([]() -> optional<timespan> { throw std::runtime_error("boom"); });

        worker.wait();

        CHECK(report_called.load());
    }

    SECTION("ClearRemovesQueuedJobsWhilePaused")
    {
        work_thread worker {"ClearWorker"};
        std::atomic_bool executed = false;

        worker.pause();
        worker.add_job([&]() -> optional<timespan> {
            executed = true;
            return std::nullopt;
        });

        CHECK(worker.get_jobs_count() == 1);

        worker.clear();
        CHECK(worker.get_jobs_count() == 0);

        worker.resume();
        worker.wait();

        CHECK_FALSE(executed.load());
    }

    SECTION("DiagnosticsTrackCompletedJobs")
    {
        work_thread worker {"DiagnosticsWorker"};

        worker.add_job([]() -> optional<timespan> { return std::nullopt; });
        worker.add_job([]() -> optional<timespan> { return std::nullopt; });

        worker.wait();

        work_thread::diagnostics diagnostics = worker.get_diagnostics();
        CHECK(diagnostics.completed_jobs == 2);
        CHECK(diagnostics.queued_jobs == 0);
        CHECK_FALSE(diagnostics.job_active);
    }

    SECTION("DiagnosticsCountEveryRescheduledRun")
    {
        work_thread worker {"RepeatDiagnosticsWorker"};
        std::atomic_int32_t runs = 0;

        worker.add_job([&]() -> optional<timespan> {
            int32_t next_run = ++runs;
            return next_run < 3 ? optional<timespan> {std::chrono::milliseconds {1}} : std::nullopt;
        });

        worker.wait();

        // Each execution of the self-rescheduling job is counted, so the server's job-throughput
        // stats reflect every body run rather than the number of distinct submissions
        CHECK(worker.get_diagnostics().completed_jobs == 3);
    }

    // Pause must wake when the active job finishes even while later jobs remain queued
    SECTION("PauseReturnsWhileJobsRemainQueued")
    {
        work_thread worker {"PauseDrainWorker"};
        std::atomic_bool gate {false};
        std::atomic_bool a_started {false};

        worker.add_job([&]() -> optional<timespan> {
            a_started.store(true);
            while (!gate.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds {1});
            }
            return std::nullopt;
        });
        worker.add_job([&]() -> optional<timespan> { return std::nullopt; }); // stays queued behind the gated job

        while (!a_started.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds {1});
        }

        // Pause from a helper thread while the first job is in flight and the second is still queued
        thread pauser = run_thread("Pauser", [&]() { worker.pause(); });
        gate.store(true); // let the in-flight job finish

        pauser.join(); // pre-fix: deadlocks here (the in-flight job cleared _jobActive but never signalled)
        CHECK_FALSE(worker.get_diagnostics().job_active);

        worker.resume();
        worker.wait();
        CHECK(worker.get_jobs_count() == 0);
    }

    // Race concurrent producers against Pause, Resume, and Clear across the guarded state machine.
    // Bodies never self-reschedule, so Pause must still converge
    SECTION("ConcurrentProducersHammerAddJobClearPauseResume")
    {
        work_thread worker {"ChaosWorker"};
        std::atomic_int64_t body_runs {0};
        std::atomic_bool stop {false};

        constexpr int producer_count = 5;
        constexpr int jobs_per_producer = 3000;

        vector<thread> producers;
        producers.reserve(producer_count);

        for (int p = 0; p < producer_count; p++) {
            producers.emplace_back(run_thread("ChaosProducer", [&]() {
                for (int i = 0; i < jobs_per_producer; i++) {
                    worker.add_job([&body_runs]() -> optional<timespan> {
                        body_runs.fetch_add(1, std::memory_order_relaxed);
                        return std::nullopt;
                    });
                }
            }));
        }

        thread controller = run_thread("ChaosController", [&]() {
            uint32_t rng = 0xC0FFEEU;
            auto next = [&rng]() {
                rng = rng * 1664525U + 1013904223U;
                return rng;
            };

            while (!stop.load(std::memory_order_acquire)) {
                switch (next() % 3) {
                case 0:
                    worker.pause();
                    worker.resume();
                    break;
                case 1:
                    worker.clear();
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

        // Ensure the worker is not left paused, then drain whatever survived the Clear churn
        worker.resume();
        worker.wait();
        CHECK(worker.get_jobs_count() == 0);

        // The worker must still be fully functional after the chaos — a fresh job runs to completion
        std::atomic_bool final_ran {false};
        worker.add_job([&final_ran]() -> optional<timespan> {
            final_ran.store(true);
            return std::nullopt;
        });
        worker.wait();
        CHECK(final_ran.load());
    }

    // Hammer clear on an idle empty worker to race the worker's unlocked acknowledgement gap. The second helper
    // keeps clearing after the first stops, so a dropped request strands alone and the watchdog reports it
    SECTION("ConcurrentClearOnIdleEmptyWorkerIsAcknowledged")
    {
        work_thread worker {"ClearLivenessEmptyWorker"};

        constexpr int32_t concurrent_clears = 1000;
        constexpr int32_t solo_tail_clears = 5000;
        constexpr int32_t helper_count = 2;

        std::atomic_int32_t helpers_done {0};
        std::atomic_int64_t progress {0};
        std::atomic_bool start {false};
        std::atomic_bool stop {false};
        std::atomic_bool tripped {false};

        vector<thread> helpers;
        helpers.reserve(helper_count);

        helpers.emplace_back(run_thread("ClearLivenessConcurrentHelper", [&]() {
            start.wait(false);

            for (int32_t i = 0; i < concurrent_clears && !stop.load(std::memory_order_acquire); i++) {
                worker.clear();
                progress.fetch_add(1, std::memory_order_relaxed);
            }

            helpers_done.fetch_add(1, std::memory_order_release);
        }));

        helpers.emplace_back(run_thread("ClearLivenessSoloTailHelper", [&]() {
            start.wait(false);

            constexpr int32_t total_clears = concurrent_clears + solo_tail_clears;

            for (int32_t i = 0; i < total_clears && !stop.load(std::memory_order_acquire); i++) {
                worker.clear();
                progress.fetch_add(1, std::memory_order_relaxed);
            }

            helpers_done.fetch_add(1, std::memory_order_release);
        }));

        start.store(true);
        start.notify_all();

        JoinHelpersWithStallWatchdog(worker, helpers, helper_count, helpers_done, progress, stop, tripped);

        INFO("clear() stalled and required watchdog rescue wakeups");
        CHECK_FALSE(tripped.load());
        CHECK(helpers_done.load() == helper_count);
        CHECK(progress.load() == concurrent_clears * 2 + solo_tail_clears);
        CHECK(worker.get_jobs_count() == 0);
        CHECK_FALSE(worker.get_diagnostics().job_active);

        if (!tripped.load()) {
            // The worker must stay usable after the concurrent clears
            std::atomic_bool final_ran {false};

            worker.add_job([&final_ran]() -> optional<timespan> {
                final_ran.store(true);
                return std::nullopt;
            });
            worker.wait();

            CHECK(final_ran.load());
        }
    }

    // Add a future-dated non-rescheduling job and clear it in the same helper loop, concurrently in the shared
    // phase and alone in the tail. Successful clears discard every job before its deadline and no body may run
    SECTION("ConcurrentClearDiscardsFutureDatedJobsWithoutRunningThem")
    {
        // Declared before the worker so its destructor can still touch the counter while draining any leftover jobs
        std::atomic_int32_t bodies_ran {0};
        work_thread worker {"ClearLivenessFutureWorker"};

        constexpr int32_t concurrent_rounds = 500;
        constexpr int32_t solo_tail_rounds = 2500;
        constexpr int32_t helper_count = 2;
        // Past the watchdog stall limit on purpose: a clear delayed to the job deadline must trip first
        constexpr auto job_delay = std::chrono::seconds {10};

        std::atomic_int32_t helpers_done {0};
        std::atomic_int64_t progress {0};
        std::atomic_bool start {false};
        std::atomic_bool stop {false};
        std::atomic_bool tripped {false};

        auto add_then_clear = [&]() {
            worker.add_job(job_delay, [&bodies_ran]() -> optional<timespan> {
                bodies_ran.fetch_add(1, std::memory_order_relaxed);
                return std::nullopt;
            });

            worker.clear();
        };

        vector<thread> helpers;
        helpers.reserve(helper_count);

        helpers.emplace_back(run_thread("FutureClearConcurrentHelper", [&]() {
            start.wait(false);

            for (int32_t i = 0; i < concurrent_rounds && !stop.load(std::memory_order_acquire); i++) {
                add_then_clear();
                progress.fetch_add(1, std::memory_order_relaxed);
            }

            helpers_done.fetch_add(1, std::memory_order_release);
        }));

        helpers.emplace_back(run_thread("FutureClearSoloTailHelper", [&]() {
            start.wait(false);

            constexpr int32_t total_rounds = concurrent_rounds + solo_tail_rounds;

            for (int32_t i = 0; i < total_rounds && !stop.load(std::memory_order_acquire); i++) {
                add_then_clear();
                progress.fetch_add(1, std::memory_order_relaxed);
            }

            helpers_done.fetch_add(1, std::memory_order_release);
        }));

        start.store(true);
        start.notify_all();

        JoinHelpersWithStallWatchdog(worker, helpers, helper_count, helpers_done, progress, stop, tripped);

        INFO("clear() stalled and required watchdog rescue wakeups");
        CHECK_FALSE(tripped.load());
        CHECK(helpers_done.load() == helper_count);
        CHECK(progress.load() == concurrent_rounds * 2 + solo_tail_rounds);
        CHECK(bodies_ran.load() == 0);
        CHECK(worker.get_jobs_count() == 0);
        CHECK_FALSE(worker.get_diagnostics().job_active);

        if (!tripped.load()) {
            // The worker must stay usable after the discarded future queue
            std::atomic_bool final_ran {false};

            worker.add_job([&final_ran]() -> optional<timespan> {
                final_ran.store(true);
                return std::nullopt;
            });
            worker.wait();

            CHECK(final_ran.load());
        }
    }
}

FO_END_NAMESPACE
