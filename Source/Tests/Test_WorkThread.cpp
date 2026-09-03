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

    SECTION("ExceptionHandlerRunsBeforeGlobalExceptionReport")
    {
        auto prev_callback = exceptions::get_callback();
        auto restore_callback = scope_exit([prev = std::move(prev_callback)]() mutable noexcept { exceptions::set_callback(std::move(prev)); });

        work_thread worker {"ExceptionOrderWorker"};
        std::atomic_bool handler_called = false;
        std::atomic_bool report_called = false;
        std::atomic_bool report_saw_handler = false;

        exceptions::set_callback([&](string_view, const stack_trace::catched_data&, bool) {
            report_saw_handler = handler_called.load();
            report_called = true;
        });

        worker.set_exception_handler([&](const std::exception&) {
            handler_called = true;
            return true;
        });

        worker.add_job([]() -> optional<timespan> { throw std::runtime_error("boom"); });

        worker.wait();

        CHECK(handler_called.load());
        CHECK(report_called.load());
        CHECK(report_saw_handler.load());
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
}

FO_END_NAMESPACE
