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

#include "WorkerPool.h"

FO_BEGIN_NAMESPACE

// Helper: wait for `predicate` to become true, polling every 1 ms up to `max_wait`.
// Used instead of sleep+CHECK to keep tests robust against scheduler jitter
static auto WaitFor(function<bool()> predicate, std::chrono::milliseconds max_wait = std::chrono::milliseconds {500}) -> bool
{
    auto deadline = std::chrono::steady_clock::now() + max_wait;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds {1});
    }
    return predicate();
}

// ============================================================================
// WorkerPool — anonymous submission
// ============================================================================

template<typename Pool>
static void CheckWorkerPoolTypedHolds()
{
    if constexpr (requires(Pool& pool, const_span<WorkerJobType> types) {
                      pool.HoldJobTypes(types);
                      pool.ReleaseHeldJobTypes();
                      pool.IsJobTypeHeld(WorkerJobType::TimeEvent);
                  }) {
        SECTION("UnrepresentableDeadlinesDoNotPublishJobs")
        {
            std::atomic<bool> shutdown {};
            Pool pool {"deadline-range", 1, &shutdown, true};
            auto body = []() -> optional<timespan> { return std::nullopt; };
            timespan maximum {std::numeric_limits<int64_t>::max()};
            CHECK_THROWS(pool.Submit(maximum, body));
            CHECK_THROWS(pool.Submit(WorkerJobKey {WorkerJobType::TimeEvent, 1}, maximum, body));
            CHECK(pool.GetDiagnostics().ScheduledJobs == 0);
            CHECK(pool.GetDiagnostics().QueuedKeys == 0);
        }

        SECTION("UnrepresentableRerunsFinishWithoutLosingTheirKey")
        {
            for (bool keyed : {false, true}) {
                CAPTURE(keyed);
                std::atomic<bool> shutdown {};
                std::atomic_uint32_t runs {};
                std::atomic_uint32_t reports {};
                std::atomic_bool report_has_context {};
                auto previous_callback = exceptions::get_callback();
                exceptions::set_callback([&](string_view, const stack_trace::catched_data&, bool) {
                    report_has_context.store(static_cast<bool>(SyncContext::GetCurrentOnThisThread()));
                    reports.fetch_add(1);
                });
                auto restore_callback = scope_exit([previous = std::move(previous_callback)]() mutable noexcept { exceptions::set_callback(std::move(previous)); });
                Pool pool {"rerun-range", 1, &shutdown};
                WorkerJobKey key {WorkerJobType::TimeEvent, 1};
                auto body = [&]() -> optional<timespan> { return runs.fetch_add(1) == 0 ? optional {timespan {std::numeric_limits<int64_t>::max()}} : std::nullopt; };
                if (keyed) {
                    pool.Submit(key, body);
                }
                else {
                    pool.Submit(body);
                }
                REQUIRE(pool.WaitIdle(timespan {std::chrono::seconds {2}}));
                CHECK(runs.load() == 1);
                CHECK(reports.load() == 1);
                CHECK(report_has_context.load());
                CHECK(pool.GetDiagnostics().QueuedKeys == 0);
                CHECK(pool.GetDiagnostics().ScheduledJobs == 0);
                pool.Submit(key, [&]() -> optional<timespan> {
                    runs.fetch_add(1);
                    return std::nullopt;
                });
                REQUIRE(pool.WaitIdle(timespan {std::chrono::seconds {2}}));
                CHECK(runs.load() == 2);
            }
        }

        SECTION("OnlySelectedTypesStop")
        {
            std::atomic<bool> shutdown {};
            std::atomic_uint32_t held_runs {};
            std::atomic_uint32_t service_runs {};
            Pool pool {"typed-hold", 2, &shutdown};
            array types {WorkerJobType::TimeEvent, WorkerJobType::None};
            pool.HoldJobTypes(types);
            CHECK(pool.IsJobTypeHeld(WorkerJobType::TimeEvent));
            CHECK_FALSE(pool.IsJobTypeHeld(WorkerJobType::None));

            pool.Submit(WorkerJobKey {WorkerJobType::TimeEvent, 1}, [&]() -> optional<timespan> {
                held_runs.fetch_add(1);
                return std::nullopt;
            });
            pool.Submit(WorkerJobKey {WorkerJobType::Player, 1}, [&]() -> optional<timespan> {
                service_runs.fetch_add(1);
                return std::nullopt;
            });
            pool.Submit([&]() -> optional<timespan> {
                service_runs.fetch_add(1);
                return std::nullopt;
            });

            REQUIRE(WaitFor([&] { return service_runs.load() == 2; }));
            CHECK(held_runs.load() == 0);
            CHECK(pool.WaitIdle(std::chrono::milliseconds {500}));
            CHECK(pool.GetDiagnostics().JobTypesHeld);
            CHECK(pool.GetDiagnostics().HeldJobs == 1);

            pool.ReleaseHeldJobTypes();
            REQUIRE(WaitFor([&] { return held_runs.load() == 1; }));
            CHECK_FALSE(pool.GetDiagnostics().JobTypesHeld);
        }

        SECTION("ReleasePreservesEqualDeadlineOrder")
        {
            std::atomic_bool shutdown {};
            vector<int32_t> order;
            Pool pool {"HoldOrderTest", 1, &shutdown};
            WorkerJobType types[] = {WorkerJobType::TimeEvent};
            pool.HoldJobTypes(types);

            for (int32_t i = 0; i < 3; i++) {
                pool.Submit({WorkerJobType::TimeEvent, numeric_cast<size_t>(i + 1)}, [&order, i]() -> optional<timespan> {
                    order.push_back(i);
                    return std::nullopt;
                });
            }

            pool.ReleaseHeldJobTypes();
            REQUIRE(pool.WaitIdle(std::chrono::seconds {2}));
            CHECK(order == vector<int32_t> {0, 1, 2});
        }

        SECTION("InvalidRequestLeavesNoHeldTypes")
        {
            std::atomic<bool> shutdown {};
            Pool pool {"typed-hold", 1, &shutdown};
            array invalid {WorkerJobType::TimeEvent, WorkerJobType {255}};
            CHECK_THROWS_AS(pool.HoldJobTypes(invalid), VerificationException);
            CHECK_FALSE(pool.GetDiagnostics().JobTypesHeld);
            CHECK_FALSE(pool.IsJobTypeHeld(WorkerJobType::TimeEvent));

            array none {WorkerJobType::None};
            CHECK_THROWS_AS(pool.HoldJobTypes(none), VerificationException);

            array valid {WorkerJobType::Player};
            pool.HoldJobTypes(valid);
            CHECK_FALSE(pool.IsJobTypeHeld(WorkerJobType::TimeEvent));
            CHECK_THROWS_AS(pool.HoldJobTypes(valid), VerificationException);
            CHECK(pool.IsJobTypeHeld(WorkerJobType::Player));
            pool.ReleaseHeldJobTypes();
            CHECK_THROWS_AS(pool.ReleaseHeldJobTypes(), VerificationException);
        }

        SECTION("MidHoldSubmissionKeepsOnlyItsRequestedDelay")
        {
            std::atomic<bool> shutdown {};
            std::atomic_bool fired {};
            std::atomic_int64_t elapsed_ms {};
            std::chrono::steady_clock::time_point released_at;
            Pool pool {"typed-hold", 1, &shutdown};
            array types {WorkerJobType::TimeEvent};
            pool.HoldJobTypes(types);
            coarse_sleep(std::chrono::milliseconds {500});
            pool.Submit(WorkerJobKey {WorkerJobType::TimeEvent, 1}, std::chrono::milliseconds {70}, [&]() -> optional<timespan> {
                elapsed_ms.store(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - released_at).count());
                fired.store(true);
                return std::nullopt;
            });
            coarse_sleep(std::chrono::milliseconds {100});
            CHECK_FALSE(fired.load());
            released_at = std::chrono::steady_clock::now();
            pool.ReleaseHeldJobTypes();
            REQUIRE(WaitFor([&] { return fired.load(); }, std::chrono::milliseconds {2000}));
            CHECK(elapsed_ms.load() >= 40);
            CHECK(elapsed_ms.load() < 350);
        }

        SECTION("WakingAHeldJobMakesItDueAtRelease")
        {
            std::atomic<bool> shutdown {};
            std::atomic_bool fired {};
            Pool pool {"typed-hold", 1, &shutdown};
            array types {WorkerJobType::TimeEvent};
            pool.HoldJobTypes(types);
            pool.Submit(WorkerJobKey {WorkerJobType::TimeEvent, 1}, std::chrono::seconds {5}, [&]() -> optional<timespan> {
                fired.store(true);
                return std::nullopt;
            });
            coarse_sleep(std::chrono::milliseconds {500});
            REQUIRE(pool.Wake(WorkerJobKey {WorkerJobType::TimeEvent, 1}));
            CHECK_FALSE(fired.load());
            pool.ReleaseHeldJobTypes();
            REQUIRE(WaitFor([&] { return fired.load(); }, std::chrono::milliseconds {350}));
        }

        SECTION("HeldPendingRerunKeepsItsMidHoldDelay")
        {
            std::atomic<bool> shutdown {};
            std::atomic_bool first_started {};
            std::atomic_bool first_finished {};
            std::atomic_int32_t first_gate {};
            std::atomic_uint32_t replacement_runs {};
            std::atomic_int64_t replacement_after_release_ms {-1};
            std::atomic_int64_t released_at_ms {};
            WorkerJobKey key {WorkerJobType::TimeEvent, 1};
            array types {WorkerJobType::TimeEvent};
            Pool pool {"typed-hold", 1, &shutdown};
            auto gate_guard = scope_exit([&]() noexcept { first_gate.store(1); });

            pool.Submit(key, [&]() -> optional<timespan> {
                first_started.store(true);

                while (first_gate.load() == 0) {
                    coarse_sleep(std::chrono::milliseconds {1});
                }

                first_finished.store(true);
                return std::nullopt;
            });
            REQUIRE(WaitFor([&] { return first_started.load(); }, std::chrono::milliseconds {1000}));

            pool.HoldJobTypes(types);
            coarse_sleep(std::chrono::milliseconds {500});

            // The replacement lands in the running key's pending slot halfway through the hold
            pool.Submit(key, std::chrono::milliseconds {70}, [&]() -> optional<timespan> {
                replacement_after_release_ms.store(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() - released_at_ms.load());
                replacement_runs.fetch_add(1);
                return std::nullopt;
            });

            first_gate.store(1);
            REQUIRE(WaitFor([&] { return first_finished.load(); }, std::chrono::milliseconds {1000}));
            REQUIRE(WaitFor(
                [&] {
                    auto diagnostics = pool.GetDiagnostics();
                    return diagnostics.RunningJobs == 0 && diagnostics.PendingReruns == 0;
                },
                std::chrono::milliseconds {1000}));

            // Completing the in-flight run must not dispatch its held replacement
            CHECK(replacement_runs.load() == 0);
            coarse_sleep(std::chrono::milliseconds {100});

            CHECK(replacement_runs.load() == 0);

            released_at_ms.store(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
            pool.ReleaseHeldJobTypes();

            REQUIRE(WaitFor([&] { return replacement_runs.load() == 1; }, std::chrono::milliseconds {2000}));
            CHECK(replacement_after_release_ms.load() >= 40);
            CHECK(replacement_after_release_ms.load() < 350);
            CHECK(pool.WaitIdle(std::chrono::milliseconds {1000}));
        }

        SECTION("GlobalFreezeKeepsTypedHoldIndependent")
        {
            std::atomic<bool> shutdown {};
            std::atomic_uint32_t held_runs {};
            std::atomic_uint32_t player_runs {};
            std::atomic_uint32_t anonymous_runs {};
            std::atomic_uint32_t frozen_service_runs {};
            std::atomic_uint32_t barrier_runs {};
            WorkerJobKey held_key {WorkerJobType::TimeEvent, 1};
            WorkerJobKey player_key {WorkerJobType::Player, 1};
            array types {WorkerJobType::TimeEvent};
            Pool pool {"typed-hold", 1, &shutdown};

            pool.HoldJobTypes(types);
            pool.Submit(held_key, [&]() -> optional<timespan> {
                held_runs.fetch_add(1);
                return std::nullopt;
            });
            pool.Submit(player_key, [&]() -> optional<timespan> {
                player_runs.fetch_add(1);
                return std::nullopt;
            });
            pool.Submit([&]() -> optional<timespan> {
                anonymous_runs.fetch_add(1);
                return std::nullopt;
            });

            // Unheld keyed and anonymous work must keep flowing while TimeEvent is held
            REQUIRE(WaitFor([&] { return player_runs.load() == 1 && anonymous_runs.load() == 1; }, std::chrono::milliseconds {1000}));
            CHECK(held_runs.load() == 0);

            pool.Pause();
            pool.FreezeSchedulingTime();

            pool.Submit([&]() -> optional<timespan> {
                frozen_service_runs.fetch_add(1);
                return std::nullopt;
            });
            coarse_sleep(std::chrono::milliseconds {250});

            CHECK(held_runs.load() == 0);
            CHECK(frozen_service_runs.load() == 0);

            pool.ResumeSchedulingTime();
            pool.Resume();

            REQUIRE(WaitFor([&] { return frozen_service_runs.load() == 1; }, std::chrono::milliseconds {1000}));
            CHECK(pool.IsJobTypeHeld(WorkerJobType::TimeEvent));
            CHECK(held_runs.load() == 0);

            pool.ReleaseHeldJobTypes();
            REQUIRE(WaitFor([&] { return held_runs.load() == 1; }, std::chrono::milliseconds {1000}));

            // A later service barrier proves the held run completed exactly once
            pool.Submit([&]() -> optional<timespan> {
                barrier_runs.fetch_add(1);
                return std::nullopt;
            });
            REQUIRE(WaitFor([&] { return barrier_runs.load() == 1; }, std::chrono::milliseconds {1000}));
            CHECK(pool.WaitIdle(std::chrono::milliseconds {1000}));

            CHECK(held_runs.load() == 1);
            CHECK_FALSE(pool.IsKeyActive(held_key));
        }

        SECTION("CancelledHeldJobLeavesKeyReusable")
        {
            std::atomic<bool> shutdown {};
            std::atomic_uint32_t cancelled_runs {};
            std::atomic_uint32_t reused_runs {};
            std::atomic_uint32_t barrier_runs {};
            WorkerJobKey key {WorkerJobType::TimeEvent, 1};
            array types {WorkerJobType::TimeEvent};
            Pool pool {"typed-hold", 1, &shutdown};

            pool.HoldJobTypes(types);
            pool.Submit(key, [&]() -> optional<timespan> {
                cancelled_runs.fetch_add(1);
                return std::nullopt;
            });
            CHECK(pool.IsKeyActive(key));

            // A service barrier orders the observation that the held closure did not run
            pool.Submit([&]() -> optional<timespan> {
                barrier_runs.fetch_add(1);
                return std::nullopt;
            });
            REQUIRE(WaitFor([&] { return barrier_runs.load() == 1; }, std::chrono::milliseconds {1000}));
            CHECK(cancelled_runs.load() == 0);

            CHECK(pool.Cancel(key));
            CHECK_FALSE(pool.IsKeyActive(key));
            pool.ReleaseHeldJobTypes();

            // Another service barrier proves the cancelled closure stayed gone after release
            pool.Submit([&]() -> optional<timespan> {
                barrier_runs.fetch_add(1);
                return std::nullopt;
            });
            REQUIRE(WaitFor([&] { return barrier_runs.load() == 2; }, std::chrono::milliseconds {1000}));
            CHECK(cancelled_runs.load() == 0);

            pool.Submit(key, [&]() -> optional<timespan> {
                reused_runs.fetch_add(1);
                return std::nullopt;
            });
            REQUIRE(WaitFor([&] { return reused_runs.load() == 1; }, std::chrono::milliseconds {1000}));

            pool.Submit([&]() -> optional<timespan> {
                barrier_runs.fetch_add(1);
                return std::nullopt;
            });
            REQUIRE(WaitFor([&] { return barrier_runs.load() == 3; }, std::chrono::milliseconds {1000}));
            CHECK(pool.WaitIdle(std::chrono::milliseconds {1000}));
            CHECK(reused_runs.load() == 1);
            CHECK(cancelled_runs.load() == 0);

            auto diagnostics = pool.GetDiagnostics();
            CHECK_FALSE(pool.IsKeyActive(key));
            CHECK(diagnostics.ScheduledJobs == 0);
            CHECK(diagnostics.QueuedKeys == 0);
            CHECK(diagnostics.PendingReruns == 0);
        }
    }
    else {
        FAIL("WorkerPool does not support selective job holds");
    }
}

TEST_CASE("WorkerPoolTypedHolds")
{
    CheckWorkerPoolTypedHolds<WorkerPool>();
}

TEST_CASE("WorkerPoolAnonymous")
{
    SECTION("SubmitFireAndForget")
    {
        std::atomic<bool> shutdown_flag {false};
        std::atomic_int counter {0};
        WorkerPool pool {"test", 2, &shutdown_flag};

        for (int i = 0; i < 100; i++) {
            pool.Submit([&]() -> std::optional<timespan> {
                counter.fetch_add(1, std::memory_order_relaxed);
                return std::nullopt;
            });
        }

        pool.WaitIdle();
        CHECK(counter.load() == 100);
    }

    SECTION("DelayedSubmitNotRunBeforeDeadline")
    {
        std::atomic<bool> shutdown_flag {false};
        std::atomic_bool fired {false};
        std::atomic<int64_t> fired_after_ms {0};
        auto submit_time = std::chrono::steady_clock::now();
        WorkerPool pool {"test", 1, &shutdown_flag};

        pool.Submit(std::chrono::milliseconds {80}, [&]() -> std::optional<timespan> {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - submit_time).count();
            fired_after_ms.store(elapsed);
            fired.store(true);
            return std::nullopt;
        });

        REQUIRE(WaitFor([&] { return fired.load(); }, std::chrono::milliseconds {2000}));
        // Measuring the actual fire delay survives suite load, unlike a fixed sleep that can itself overshoot the
        // deadline; the slack covers timer granularity
        CHECK(fired_after_ms.load() >= 70);
        pool.WaitIdle();
    }

    SECTION("AnonymousSelfRescheduleRunsUntilNullopt")
    {
        std::atomic<bool> shutdown_flag {false};
        std::atomic_int runs {0};
        WorkerPool pool {"test", 1, &shutdown_flag};

        pool.Submit([&]() -> std::optional<timespan> {
            int32_t next = runs.fetch_add(1, std::memory_order_relaxed) + 1;
            return next < 3 ? std::optional<timespan> {std::chrono::milliseconds {1}} : std::nullopt;
        });

        REQUIRE(WaitFor([&] { return runs.load() == 3; }));
        pool.WaitIdle();
        CHECK(runs.load() == 3);
    }
}

// ============================================================================
// WorkerPool — keyed submission and coalescing
// ============================================================================

TEST_CASE("WorkerPoolKeyed")
{
    SECTION("KeyDedupWhileQueued")
    {
        // One thread, blocking job in flight; subsequent Submit(key, ...) calls with the same key
        // should coalesce into the queued slot rather than stack up
        std::atomic<bool> shutdown_flag {false};
        std::atomic_int blocker_acquired {0};
        std::atomic_int blocker_release {0};
        std::atomic_int submissions {0};
        WorkerJobKey gate_key {WorkerJobType::Player, 1};
        WorkerJobKey dup_key {WorkerJobType::Player, 2};
        WorkerPool pool {"test", 1, &shutdown_flag};

        // First job: blocks until release flag flips, occupying the worker
        pool.Submit(gate_key, [&]() -> std::optional<timespan> {
            blocker_acquired.fetch_add(1);
            while (blocker_release.load() == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds {1});
            }
            return std::nullopt;
        });
        REQUIRE(WaitFor([&] { return blocker_acquired.load() == 1; }));

        // Now another key, submitted many times — second and beyond should be dropped (queued dedup)
        for (int i = 0; i < 10; i++) {
            pool.Submit(dup_key, [&]() -> std::optional<timespan> {
                submissions.fetch_add(1);
                return std::nullopt;
            });
        }
        CHECK(pool.IsKeyActive(dup_key));

        blocker_release.store(1);
        pool.WaitIdle();
        CHECK(submissions.load() == 1);
    }

    SECTION("PendingRerunReplacesClosureWhileRunning")
    {
        // While a keyed job is running, a fresh Submit(key, newClosure) should make `newClosure`
        // execute exactly once after the current run, replacing any previously-stored closure
        std::atomic<bool> shutdown_flag {false};
        std::atomic_int latest_value {0};
        std::atomic_int run_count {0};
        std::atomic_int gate {0};
        WorkerJobKey key {WorkerJobType::Player, 7};
        WorkerPool pool {"test", 1, &shutdown_flag};

        pool.Submit(key, [&]() -> std::optional<timespan> {
            run_count.fetch_add(1);
            while (gate.load() == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds {1});
            }
            return std::nullopt;
        });

        // Wait until the first job is actually running before pushing replacements; otherwise the
        // replacements would hit the queued-dedup branch (no-op) instead of the running branch
        REQUIRE(WaitFor([&] { return run_count.load() == 1; }));

        // Now while it's running, push three closures with increasing values; only the last must run
        for (int v = 1; v <= 3; v++) {
            pool.Submit(key, [&, v]() -> std::optional<timespan> {
                latest_value.store(v);
                run_count.fetch_add(1);
                return std::nullopt;
            });
        }

        gate.store(1);
        pool.WaitIdle();
        CHECK(run_count.load() == 2); // Original + one rerun
        CHECK(latest_value.load() == 3);
    }

    SECTION("PendingRerunHonoursSubmittedDelayWhileRunning")
    {
        std::atomic<bool> shutdown_flag {false};
        std::atomic_int run_count {0};
        std::atomic_bool first_run_started {false};
        std::atomic_int first_run_release {0};
        std::atomic<int64_t> rerun_after_ms {0};
        WorkerJobKey key {WorkerJobType::Player, 8};
        std::chrono::steady_clock::time_point submit_time {};
        WorkerPool pool {"test", 1, &shutdown_flag};

        pool.Submit(key, [&]() -> std::optional<timespan> {
            run_count.fetch_add(1);
            first_run_started.store(true);
            while (first_run_release.load() == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds {1});
            }
            return std::nullopt;
        });

        REQUIRE(WaitFor([&] { return first_run_started.load(); }));

        submit_time = std::chrono::steady_clock::now();
        pool.Submit(key, std::chrono::milliseconds {80}, [&]() -> std::optional<timespan> {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - submit_time).count();
            rerun_after_ms.store(elapsed);
            run_count.fetch_add(1);
            return std::nullopt;
        });

        first_run_release.store(1);

        REQUIRE(WaitFor([&] { return run_count.load() == 2; }, std::chrono::milliseconds {2000}));
        CHECK(rerun_after_ms.load() >= 70);
        pool.WaitIdle();
    }

    SECTION("DistinctTypeSameIdAreSeparateKeys")
    {
        // {Player, 42} and {CritterMovement, 42} must NOT collide
        std::atomic<bool> shutdown_flag {false};
        std::atomic_int player_runs {0};
        std::atomic_int critter_runs {0};
        WorkerJobKey player_key {WorkerJobType::Player, 42};
        WorkerJobKey critter_key {WorkerJobType::CritterMovement, 42};
        WorkerPool pool {"test", 2, &shutdown_flag};

        pool.Submit(player_key, [&]() -> std::optional<timespan> {
            player_runs.fetch_add(1);
            return std::nullopt;
        });
        pool.Submit(critter_key, [&]() -> std::optional<timespan> {
            critter_runs.fetch_add(1);
            return std::nullopt;
        });

        pool.WaitIdle();
        CHECK(player_runs.load() == 1);
        CHECK(critter_runs.load() == 1);
    }

    SECTION("KeyedSelfRescheduleHonoursReturnedDelay")
    {
        std::atomic<bool> shutdown_flag {false};
        std::atomic_int runs {0};
        WorkerJobKey key {WorkerJobType::Player, 1};
        WorkerPool pool {"test", 1, &shutdown_flag};

        pool.Submit(key, [&]() -> std::optional<timespan> {
            int32_t next = runs.fetch_add(1, std::memory_order_relaxed) + 1;
            return next < 4 ? std::optional<timespan> {std::chrono::milliseconds {2}} : std::nullopt;
        });

        REQUIRE(WaitFor([&] { return runs.load() == 4; }));
        pool.WaitIdle();
        CHECK(runs.load() == 4);
    }
}

// ============================================================================
// WorkerPool — Wake semantics
// ============================================================================

TEST_CASE("WorkerPoolWake")
{
    SECTION("WakeQueuedRunsImmediatelyInsteadOfDelay")
    {
        std::atomic<bool> shutdown_flag {false};
        std::atomic_int runs {0};
        WorkerJobKey key {WorkerJobType::Player, 1};
        WorkerPool pool {"test", 1, &shutdown_flag};

        // Submit with a 5-second delay — it would normally not run during the test
        pool.Submit(key, std::chrono::seconds {5}, [&]() -> std::optional<timespan> {
            runs.fetch_add(1);
            return std::nullopt;
        });

        // Wake should advance FireTime to now
        bool waked = pool.Wake(key);
        CHECK(waked);

        REQUIRE(WaitFor([&] { return runs.load() == 1; }));
        pool.WaitIdle();
    }

    SECTION("WakeMissingKeyReturnsFalse")
    {
        std::atomic<bool> shutdown_flag {false};
        WorkerJobKey key {WorkerJobType::Player, 999};
        WorkerPool pool {"test", 1, &shutdown_flag};
        CHECK_FALSE(pool.Wake(key));
    }

    SECTION("WakeRunningJobOverridesSelfRescheduleDelay")
    {
        // Body returns a 5-second delay between runs; Wake during execution must collapse the
        // wait so the next run starts immediately
        std::atomic<bool> shutdown_flag {false};
        std::atomic_int runs {0};
        std::atomic_bool first_run_started {false};
        std::atomic_int first_run_release {0};
        WorkerJobKey key {WorkerJobType::Player, 1};
        WorkerPool pool {"test", 1, &shutdown_flag};

        pool.Submit(key, [&]() -> std::optional<timespan> {
            int32_t n = runs.fetch_add(1, std::memory_order_relaxed) + 1;
            if (n == 1) {
                first_run_started.store(true);
                while (first_run_release.load() == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds {1});
                }
                return std::optional<timespan> {std::chrono::seconds {5}};
            }
            return std::nullopt;
        });

        REQUIRE(WaitFor([&] { return first_run_started.load(); }));
        bool waked = pool.Wake(key);
        CHECK(waked);

        first_run_release.store(1);
        REQUIRE(WaitFor([&] { return runs.load() == 2; }, std::chrono::milliseconds {1000}));
        pool.WaitIdle();
    }
}

// ============================================================================
// WorkerPool — Cancel semantics
// ============================================================================

TEST_CASE("WorkerPoolCancel")
{
    SECTION("CancelQueuedDropsTheJob")
    {
        std::atomic<bool> shutdown_flag {false};
        std::atomic_int gate_run {0};
        std::atomic_int gate_release {0};
        std::atomic_int target_runs {0};
        WorkerJobKey gate_key {WorkerJobType::Player, 1};
        WorkerJobKey target_key {WorkerJobType::Player, 2};
        WorkerPool pool {"test", 1, &shutdown_flag};

        pool.Submit(gate_key, [&]() -> std::optional<timespan> {
            gate_run.fetch_add(1);
            while (gate_release.load() == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds {1});
            }
            return std::nullopt;
        });
        REQUIRE(WaitFor([&] { return gate_run.load() == 1; }));

        pool.Submit(target_key, [&]() -> std::optional<timespan> {
            target_runs.fetch_add(1);
            return std::nullopt;
        });
        CHECK(pool.IsKeyActive(target_key));

        bool cancelled = pool.Cancel(target_key);
        CHECK(cancelled);
        CHECK_FALSE(pool.IsKeyActive(target_key));

        gate_release.store(1);
        pool.WaitIdle();
        CHECK(target_runs.load() == 0);
    }

    SECTION("CancelRunningArmsCancelOnFinishToDropSelfReschedule")
    {
        std::atomic<bool> shutdown_flag {false};
        std::atomic_int runs {0};
        std::atomic_bool first_started {false};
        std::atomic_int first_release {0};
        WorkerJobKey key {WorkerJobType::Player, 1};
        WorkerPool pool {"test", 1, &shutdown_flag};

        pool.Submit(key, [&]() -> std::optional<timespan> {
            runs.fetch_add(1);
            first_started.store(true);
            while (first_release.load() == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds {1});
            }
            // Body asks for a self-reschedule, but Cancel should suppress it
            return std::optional<timespan> {std::chrono::milliseconds {1}};
        });

        REQUIRE(WaitFor([&] { return first_started.load(); }));
        CHECK(pool.Cancel(key));

        first_release.store(1);
        // Wait for the in-flight run to finalize. After that, no rerun should happen
        std::this_thread::sleep_for(std::chrono::milliseconds {50});
        pool.WaitIdle();
        CHECK(runs.load() == 1);
    }

    SECTION("SubmitAfterCancelRunningOverridesCancelOnFinish")
    {
        std::atomic<bool> shutdown_flag {false};
        std::atomic_int runs {0};
        std::atomic_bool first_started {false};
        std::atomic_int first_release {0};
        WorkerJobKey key {WorkerJobType::Player, 1};
        WorkerPool pool {"test", 1, &shutdown_flag};

        pool.Submit(key, [&]() -> std::optional<timespan> {
            runs.fetch_add(1);
            first_started.store(true);
            while (first_release.load() == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds {1});
            }
            return std::nullopt;
        });
        REQUIRE(WaitFor([&] { return first_started.load(); }));

        CHECK(pool.Cancel(key));

        // Submit after Cancel-while-running: the new closure should run as a pending rerun, and
        // the cancel-on-finish flag must be cleared so the rerun isn't itself silently dropped
        pool.Submit(key, [&]() -> std::optional<timespan> {
            runs.fetch_add(1);
            return std::nullopt;
        });

        first_release.store(1);
        REQUIRE(WaitFor([&] { return runs.load() == 2; }));
        pool.WaitIdle();
    }
}

// ============================================================================
// WorkerPool — exception handling
// ============================================================================

TEST_CASE("WorkerPoolExceptionHandling")
{
    SECTION("ExceptionInJobDoesNotKillPool")
    {
        std::atomic<bool> shutdown_flag {false};
        std::atomic_int exception_seen {0};
        std::atomic_int success {0};
        WorkerPool pool {"test", 1, &shutdown_flag};

        pool.Submit([&]() -> std::optional<timespan> {
            exception_seen.fetch_add(1);
            shutdown_flag.store(true, std::memory_order_release);
            throw std::runtime_error("boom");
        });

        REQUIRE(WaitFor([&] { return exception_seen.load() == 1; }));
        pool.WaitIdle();
        shutdown_flag.store(false, std::memory_order_release);

        for (int i = 0; i < 5; i++) {
            pool.Submit([&]() -> std::optional<timespan> {
                success.fetch_add(1);
                return std::nullopt;
            });
        }

        pool.WaitIdle();
        CHECK(success.load() == 5);
    }
}

// ============================================================================
// WorkerPool — Clear and parallelism
// ============================================================================

TEST_CASE("WorkerPoolClearAndParallelism")
{
    SECTION("ClearRemovesQueuedAndArmsCancelOnRunning")
    {
        std::atomic<bool> shutdown_flag {false};
        std::atomic_int gate_started {0};
        std::atomic_int gate_release {0};
        std::atomic_int gate_reruns {0};
        std::atomic_int dropped_runs {0};
        WorkerJobKey gate_key {WorkerJobType::Player, 1};
        WorkerPool pool {"test", 1, &shutdown_flag};

        pool.Submit(gate_key, [&]() -> std::optional<timespan> {
            int32_t n = gate_started.fetch_add(1) + 1;
            if (n == 1) {
                while (gate_release.load() == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds {1});
                }
                // Self-reschedule — but Clear() should suppress this rerun
                return std::optional<timespan> {std::chrono::milliseconds {1}};
            }
            gate_reruns.fetch_add(1);
            return std::nullopt;
        });
        REQUIRE(WaitFor([&] { return gate_started.load() == 1; }));

        for (int i = 0; i < 20; i++) {
            WorkerJobKey k {WorkerJobType::Player, static_cast<size_t>(100 + i)};
            pool.Submit(k, [&]() -> std::optional<timespan> {
                dropped_runs.fetch_add(1);
                return std::nullopt;
            });
        }

        pool.Clear();
        gate_release.store(1);
        std::this_thread::sleep_for(std::chrono::milliseconds {50});
        pool.WaitIdle();

        CHECK(dropped_runs.load() == 0);
        CHECK(gate_reruns.load() == 0);
    }

    SECTION("MultipleWorkersRunIndependentJobsConcurrently")
    {
        std::atomic<bool> shutdown_flag {false};
        std::atomic_int counter {0};
        WorkerPool pool {"test", 4, &shutdown_flag};
        constexpr int total = 200;

        for (int i = 0; i < total; i++) {
            pool.Submit([&]() -> std::optional<timespan> {
                counter.fetch_add(1, std::memory_order_relaxed);
                return std::nullopt;
            });
        }

        pool.WaitIdle();
        CHECK(counter.load() == total);
        CHECK(pool.GetThreadCount() == 4);
    }
}

// ============================================================================
// WorkerPool — concurrent chaos (keyed bookkeeping under external mutation)
// ============================================================================

TEST_CASE("WorkerPoolConcurrentChaos")
{
    // Drivers hammer a deliberately overlapping key space while workers run, which is the interleaving that
    // stresses every finalize transition; only structural invariants are asserted, since the bodies race
    SECTION("DriversHammerSubmitWakeCancelClear")
    {
        std::atomic<bool> shutdown_flag {false};
        WorkerPool pool {"chaos", 4, &shutdown_flag};

        std::atomic_int body_runs {0};

        constexpr int driver_count = 6;
        constexpr int ops_per_driver = 3000;
        constexpr size_t key_space = 8; // small so keys overlap and contend across drivers and workers

        auto make_body = [&body_runs]() -> WorkerPool::Job {
            return [&body_runs]() -> std::optional<timespan> {
                int32_t n = body_runs.fetch_add(1, std::memory_order_relaxed) + 1;
                // Every fourth run self-reschedules with a tiny delay, exercising the reschedule path
                // and the wake-override / cancel-on-finish interplay when an external op lands mid-run
                return n % 4 == 0 ? std::optional<timespan> {std::chrono::microseconds {50}} : std::nullopt;
            };
        };

        std::vector<std::thread> drivers;
        drivers.reserve(driver_count);

        for (int d = 0; d < driver_count; d++) {
            drivers.emplace_back([&, d]() {
                // Per-driver deterministic LCG — no shared RNG state, no <random> dependency
                uint32_t rng = 0xC0FFEEU + static_cast<uint32_t>(d) * 2654435761U;
                auto next = [&rng]() {
                    rng = rng * 1664525U + 1013904223U;
                    return rng;
                };

                for (int i = 0; i < ops_per_driver; i++) {
                    WorkerJobKey key {WorkerJobType::Player, static_cast<size_t>(next() % key_space) + 1};

                    switch (next() % 6) {
                    case 0:
                    case 1:
                        pool.Submit(key, make_body());
                        break;
                    case 2:
                        pool.Submit(key, std::chrono::microseconds {static_cast<int64_t>(next() % 200)}, make_body());
                        break;
                    case 3:
                        (void)pool.Wake(key);
                        break;
                    case 4:
                        (void)pool.Cancel(key);
                        break;
                    case 5:
                        if (i % 512 == 511) {
                            pool.Clear();
                        }
                        break;
                    default:
                        break;
                    }
                }
            });
        }

        for (auto& t : drivers) {
            t.join();
        }

        // Stop the self-reschedule churn and drain everything to a clean barrier. After Clear() no queued
        // job remains and every in-flight body is marked cancel-on-finish, so the pool must reach idle
        pool.Clear();
        REQUIRE(pool.WaitIdle(std::chrono::seconds {10}));

        auto diag = pool.GetDiagnostics();
        CHECK(diag.ScheduledJobs == 0);
        CHECK(diag.QueuedKeys == 0);
        CHECK(diag.RunningJobs == 0);
        CHECK(diag.PendingReruns == 0);
        CHECK(diag.ActiveWorkers == 0);
        CHECK(body_runs.load() > 0);
    }
}

TEST_CASE("WorkerPoolWaitIdleTimeout")
{
    // The timed WaitIdle backs the shutdown grace window: while a job is still in flight it must
    // report "not idle" once the deadline passes, and reach idle once the job drains
    SECTION("ReturnsFalseWhileBusyThenTrueWhenDrained")
    {
        std::atomic<bool> shutdown_flag {false};
        std::atomic_int started {0};
        std::atomic_int release {0};
        WorkerPool pool {"test", 1, &shutdown_flag};

        pool.Submit([&]() -> std::optional<timespan> {
            started.fetch_add(1);
            while (release.load() == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds {1});
            }
            return std::nullopt;
        });

        REQUIRE(WaitFor([&] { return started.load() == 1; }));

        // Job is parked on the gate → the pool is not idle, so a short timed wait must give up
        CHECK_FALSE(pool.WaitIdle(std::chrono::milliseconds {50}));

        // Release the gate; the timed wait now reaches idle well within its budget
        release.store(1);
        CHECK(pool.WaitIdle(std::chrono::seconds {5}));

        pool.WaitIdle();
    }
}

TEST_CASE("WorkerPoolCountsAnonymousScheduledJobsIncrementally")
{
    std::atomic<bool> shutdown_flag {false};
    std::atomic<int32_t> ran {0};
    WorkerPool pool {"test", 1, &shutdown_flag, /*start_paused*/ true};
    WorkerJobKey key {WorkerJobType::Player, 1};

    CHECK(pool.GetDiagnostics().AnonymousScheduledJobs == 0);

    pool.Submit(std::chrono::milliseconds {5000}, [&]() -> std::optional<timespan> { return std::nullopt; });
    pool.Submit(std::chrono::milliseconds {5000}, [&]() -> std::optional<timespan> { return std::nullopt; });
    CHECK(pool.GetDiagnostics().AnonymousScheduledJobs == 2);

    // A keyed job is never counted as an anonymous one
    pool.Submit(key, std::chrono::milliseconds {5000}, [&]() -> std::optional<timespan> { return std::nullopt; });
    CHECK(pool.GetDiagnostics().AnonymousScheduledJobs == 2);
    CHECK(pool.GetDiagnostics().ScheduledJobs == 3);

    CHECK(pool.Cancel(key));
    CHECK(pool.GetDiagnostics().AnonymousScheduledJobs == 2);

    // Running the queue empties the counter as jobs leave it
    pool.Submit([&]() -> std::optional<timespan> {
        ran++;
        return std::nullopt;
    });
    pool.Resume();
    REQUIRE(WaitFor([&] { return ran.load() == 1; }, std::chrono::milliseconds {2000}));
    pool.WaitIdle();
    CHECK(pool.GetDiagnostics().AnonymousScheduledJobs == 2);

    pool.Clear();
    CHECK(pool.GetDiagnostics().AnonymousScheduledJobs == 0);
    CHECK(pool.GetDiagnostics().ScheduledJobs == 0);
}

TEST_CASE("WorkerPoolSchedulingTimeFreezePreservesDelays")
{
    std::atomic<bool> shutdown_flag {false};
    std::atomic<int64_t> before_freeze_after_resume_ms {-1};
    std::atomic<int64_t> during_freeze_after_resume_ms {-1};
    std::chrono::steady_clock::time_point resume_time {};
    WorkerPool pool {"test", 1, &shutdown_flag, /*start_paused*/ true};

    pool.Submit(std::chrono::milliseconds {200}, [&]() -> std::optional<timespan> {
        before_freeze_after_resume_ms.store(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - resume_time).count());
        return std::nullopt;
    });

    pool.FreezeSchedulingTime();
    CHECK(pool.GetDiagnostics().SchedulingTimeFrozen);

    pool.Submit(std::chrono::milliseconds {200}, [&]() -> std::optional<timespan> {
        during_freeze_after_resume_ms.store(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - resume_time).count());
        return std::nullopt;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds {250});
    CHECK(before_freeze_after_resume_ms.load() == -1);
    CHECK(during_freeze_after_resume_ms.load() == -1);

    resume_time = std::chrono::steady_clock::now();
    pool.ResumeSchedulingTime();
    CHECK_FALSE(pool.GetDiagnostics().SchedulingTimeFrozen);
    pool.Resume();

    REQUIRE(WaitFor([&] { return before_freeze_after_resume_ms.load() >= 0 && during_freeze_after_resume_ms.load() >= 0; }, std::chrono::milliseconds {2000}));
    CHECK(before_freeze_after_resume_ms.load() >= 100);
    CHECK(during_freeze_after_resume_ms.load() >= 100);
    pool.WaitIdle();
}

// ============================================================================
// WorkerPool — diagnostics
// ============================================================================

TEST_CASE("WorkerPoolDiagnostics")
{
    SECTION("CompletedJobsCountsEveryRun")
    {
        std::atomic<bool> shutdown_flag {false};
        std::atomic_int counter {0};
        WorkerPool pool {"test", 2, &shutdown_flag};

        for (int i = 0; i < 100; i++) {
            pool.Submit([&]() -> std::optional<timespan> {
                counter.fetch_add(1, std::memory_order_relaxed);
                return std::nullopt;
            });
        }

        pool.WaitIdle();

        WorkerPool::Diagnostics diagnostics = pool.GetDiagnostics();
        CHECK(diagnostics.CompletedJobs == 100);
        CHECK(diagnostics.ThreadCount == 2);
        CHECK(diagnostics.ScheduledJobs == 0);
        CHECK(diagnostics.RunningJobs == 0);
        CHECK(diagnostics.ActiveWorkers == 0);
    }

    SECTION("SelfRescheduleCountsEachRun")
    {
        std::atomic<bool> shutdown_flag {false};
        std::atomic_int runs {0};
        WorkerJobKey key {WorkerJobType::Player, 1};
        WorkerPool pool {"test", 1, &shutdown_flag};

        pool.Submit(key, [&]() -> std::optional<timespan> {
            int32_t next = runs.fetch_add(1, std::memory_order_relaxed) + 1;
            return next < 3 ? std::optional<timespan> {std::chrono::milliseconds {1}} : std::nullopt;
        });

        REQUIRE(WaitFor([&] { return runs.load() == 3; }));
        pool.WaitIdle();
        CHECK(pool.GetDiagnostics().CompletedJobs == 3);
    }
}

FO_END_NAMESPACE
