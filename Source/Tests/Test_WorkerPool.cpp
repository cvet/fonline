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

#include "WorkerPool.h"

FO_BEGIN_NAMESPACE

// Helper: wait for `predicate` to become true, polling every 1 ms up to `max_wait`.
// Used instead of sleep+CHECK to keep tests robust against scheduler jitter.
static auto WaitFor(std::function<bool()> predicate, std::chrono::milliseconds max_wait = std::chrono::milliseconds {500}) -> bool
{
    const auto deadline = std::chrono::steady_clock::now() + max_wait;
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
        const auto submit_time = std::chrono::steady_clock::now();
        WorkerPool pool {"test", 1, &shutdown_flag};

        pool.Submit(std::chrono::milliseconds {80}, [&]() -> std::optional<timespan> {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - submit_time).count();
            fired_after_ms.store(elapsed);
            fired.store(true);
            return std::nullopt;
        });

        REQUIRE(WaitFor([&] { return fired.load(); }, std::chrono::milliseconds {2000}));
        // The delayed task must not run before (close to) its scheduled deadline. Measuring the actual
        // fire delay is robust under suite load, unlike asserting "not fired" after a fixed wall-clock
        // sleep that can itself overshoot the deadline. Small slack accounts for timer granularity.
        CHECK(fired_after_ms.load() >= 70);
        pool.WaitIdle();
    }

    SECTION("AnonymousSelfRescheduleRunsUntilNullopt")
    {
        std::atomic<bool> shutdown_flag {false};
        std::atomic_int runs {0};
        WorkerPool pool {"test", 1, &shutdown_flag};

        pool.Submit([&]() -> std::optional<timespan> {
            const int next = runs.fetch_add(1, std::memory_order_relaxed) + 1;
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
        // should coalesce into the queued slot rather than stack up.
        std::atomic<bool> shutdown_flag {false};
        std::atomic_int blocker_acquired {0};
        std::atomic_int blocker_release {0};
        std::atomic_int submissions {0};
        const WorkerJobKey gate_key {WorkerJobType::Player, 1};
        const WorkerJobKey dup_key {WorkerJobType::Player, 2};
        WorkerPool pool {"test", 1, &shutdown_flag};

        // First job: blocks until release flag flips, occupying the worker.
        pool.Submit(gate_key, [&]() -> std::optional<timespan> {
            blocker_acquired.fetch_add(1);
            while (blocker_release.load() == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds {1});
            }
            return std::nullopt;
        });
        REQUIRE(WaitFor([&] { return blocker_acquired.load() == 1; }));

        // Now another key, submitted many times — second and beyond should be dropped (queued dedup).
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
        // execute exactly once after the current run, replacing any previously-stored closure.
        std::atomic<bool> shutdown_flag {false};
        std::atomic_int latest_value {0};
        std::atomic_int run_count {0};
        std::atomic_int gate {0};
        const WorkerJobKey key {WorkerJobType::Player, 7};
        WorkerPool pool {"test", 1, &shutdown_flag};

        pool.Submit(key, [&]() -> std::optional<timespan> {
            run_count.fetch_add(1);
            while (gate.load() == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds {1});
            }
            return std::nullopt;
        });

        // Wait until the first job is actually running before pushing replacements; otherwise the
        // replacements would hit the queued-dedup branch (no-op) instead of the running branch.
        REQUIRE(WaitFor([&] { return run_count.load() == 1; }));

        // Now while it's running, push three closures with increasing values; only the last must run.
        for (int v = 1; v <= 3; v++) {
            pool.Submit(key, [&, v]() -> std::optional<timespan> {
                latest_value.store(v);
                run_count.fetch_add(1);
                return std::nullopt;
            });
        }

        gate.store(1);
        pool.WaitIdle();
        CHECK(run_count.load() == 2); // Original + one rerun.
        CHECK(latest_value.load() == 3);
    }

    SECTION("PendingRerunHonoursSubmittedDelayWhileRunning")
    {
        std::atomic<bool> shutdown_flag {false};
        std::atomic_int run_count {0};
        std::atomic_bool first_run_started {false};
        std::atomic_int first_run_release {0};
        std::atomic<int64_t> rerun_after_ms {0};
        const WorkerJobKey key {WorkerJobType::Player, 8};
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
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - submit_time).count();
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
        // {Player, 42} and {CritterMovement, 42} must NOT collide.
        std::atomic<bool> shutdown_flag {false};
        std::atomic_int player_runs {0};
        std::atomic_int critter_runs {0};
        const WorkerJobKey player_key {WorkerJobType::Player, 42};
        const WorkerJobKey critter_key {WorkerJobType::CritterMovement, 42};
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
        const WorkerJobKey key {WorkerJobType::Player, 1};
        WorkerPool pool {"test", 1, &shutdown_flag};

        pool.Submit(key, [&]() -> std::optional<timespan> {
            const int next = runs.fetch_add(1, std::memory_order_relaxed) + 1;
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
        const WorkerJobKey key {WorkerJobType::Player, 1};
        WorkerPool pool {"test", 1, &shutdown_flag};

        // Submit with a 5-second delay — it would normally not run during the test.
        pool.Submit(key, std::chrono::seconds {5}, [&]() -> std::optional<timespan> {
            runs.fetch_add(1);
            return std::nullopt;
        });

        // Wake should advance FireTime to now.
        const bool waked = pool.Wake(key);
        CHECK(waked);

        REQUIRE(WaitFor([&] { return runs.load() == 1; }));
        pool.WaitIdle();
    }

    SECTION("WakeMissingKeyReturnsFalse")
    {
        std::atomic<bool> shutdown_flag {false};
        const WorkerJobKey key {WorkerJobType::Player, 999};
        WorkerPool pool {"test", 1, &shutdown_flag};
        CHECK_FALSE(pool.Wake(key));
    }

    SECTION("WakeRunningJobOverridesSelfRescheduleDelay")
    {
        // Body returns a 5-second delay between runs; Wake during execution must collapse the
        // wait so the next run starts immediately.
        std::atomic<bool> shutdown_flag {false};
        std::atomic_int runs {0};
        std::atomic_bool first_run_started {false};
        std::atomic_int first_run_release {0};
        const WorkerJobKey key {WorkerJobType::Player, 1};
        WorkerPool pool {"test", 1, &shutdown_flag};

        pool.Submit(key, [&]() -> std::optional<timespan> {
            const int n = runs.fetch_add(1, std::memory_order_relaxed) + 1;
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
        const bool waked = pool.Wake(key);
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
        const WorkerJobKey gate_key {WorkerJobType::Player, 1};
        const WorkerJobKey target_key {WorkerJobType::Player, 2};
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

        const bool cancelled = pool.Cancel(target_key);
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
        const WorkerJobKey key {WorkerJobType::Player, 1};
        WorkerPool pool {"test", 1, &shutdown_flag};

        pool.Submit(key, [&]() -> std::optional<timespan> {
            runs.fetch_add(1);
            first_started.store(true);
            while (first_release.load() == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds {1});
            }
            // Body asks for a self-reschedule, but Cancel should suppress it.
            return std::optional<timespan> {std::chrono::milliseconds {1}};
        });

        REQUIRE(WaitFor([&] { return first_started.load(); }));
        CHECK(pool.Cancel(key));

        first_release.store(1);
        // Wait for the in-flight run to finalize. After that, no rerun should happen.
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
        const WorkerJobKey key {WorkerJobType::Player, 1};
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
        // the cancel-on-finish flag must be cleared so the rerun isn't itself silently dropped.
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
        const WorkerJobKey gate_key {WorkerJobType::Player, 1};
        WorkerPool pool {"test", 1, &shutdown_flag};

        pool.Submit(gate_key, [&]() -> std::optional<timespan> {
            const int n = gate_started.fetch_add(1) + 1;
            if (n == 1) {
                while (gate_release.load() == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds {1});
                }
                // Self-reschedule — but Clear() should suppress this rerun.
                return std::optional<timespan> {std::chrono::milliseconds {1}};
            }
            gate_reruns.fetch_add(1);
            return std::nullopt;
        });
        REQUIRE(WaitFor([&] { return gate_started.load() == 1; }));

        for (int i = 0; i < 20; i++) {
            const WorkerJobKey k {WorkerJobType::Player, static_cast<size_t>(100 + i)};
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
    // No isolated test exercises the keyed bookkeeping (_pendingRerun / _wakeRequests /
    // _cancelOnFinish / _queuedKeys / _runningKeys) under concurrent external mutation. Several driver
    // threads hammer Submit / Wake / Cancel / Clear over a small, deliberately overlapping key space
    // while the worker threads run the bodies — the interleaving that actually stresses every
    // transition in WorkerEntry's finalize block. Bodies are non-deterministic, so only structural
    // invariants are asserted: the pool must stay race-free (TSan), never touch a freed closure (ASan),
    // and drain to a fully idle, self-consistent state.
    SECTION("DriversHammerSubmitWakeCancelClear")
    {
        std::atomic<bool> shutdown_flag {false};
        WorkerPool pool {"chaos", 4, &shutdown_flag};

        std::atomic_int body_runs {0};

        constexpr int driver_count = 6;
        constexpr int ops_per_driver = 3000;
        constexpr size_t key_space = 8; // small so keys overlap and contend across drivers and workers

        const auto make_body = [&body_runs]() -> WorkerPool::Job {
            return [&body_runs]() -> std::optional<timespan> {
                const int n = body_runs.fetch_add(1, std::memory_order_relaxed) + 1;
                // Every fourth run self-reschedules with a tiny delay, exercising the reschedule path
                // and the wake-override / cancel-on-finish interplay when an external op lands mid-run.
                return n % 4 == 0 ? std::optional<timespan> {std::chrono::microseconds {50}} : std::nullopt;
            };
        };

        std::vector<std::thread> drivers;
        drivers.reserve(driver_count);

        for (int d = 0; d < driver_count; d++) {
            drivers.emplace_back([&, d]() {
                // Per-driver deterministic LCG — no shared RNG state, no <random> dependency.
                uint32_t rng = 0xC0FFEEU + static_cast<uint32_t>(d) * 2654435761U;
                const auto next = [&rng]() {
                    rng = rng * 1664525U + 1013904223U;
                    return rng;
                };

                for (int i = 0; i < ops_per_driver; i++) {
                    const WorkerJobKey key {WorkerJobType::Player, static_cast<size_t>(next() % key_space) + 1};

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
        // job remains and every in-flight body is marked cancel-on-finish, so the pool must reach idle.
        pool.Clear();
        REQUIRE(pool.WaitIdle(std::chrono::seconds {10}));

        const auto diag = pool.GetDiagnostics();
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
    // report "not idle" once the deadline passes, and reach idle once the job drains.
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

        // Job is parked on the gate → the pool is not idle, so a short timed wait must give up.
        CHECK_FALSE(pool.WaitIdle(std::chrono::milliseconds {50}));

        // Release the gate; the timed wait now reaches idle well within its budget.
        release.store(1);
        CHECK(pool.WaitIdle(std::chrono::seconds {5}));

        pool.WaitIdle();
    }
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

        const WorkerPool::Diagnostics diagnostics = pool.GetDiagnostics();
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
        const WorkerJobKey key {WorkerJobType::Player, 1};
        WorkerPool pool {"test", 1, &shutdown_flag};

        pool.Submit(key, [&]() -> std::optional<timespan> {
            const int next = runs.fetch_add(1, std::memory_order_relaxed) + 1;
            return next < 3 ? std::optional<timespan> {std::chrono::milliseconds {1}} : std::nullopt;
        });

        REQUIRE(WaitFor([&] { return runs.load() == 3; }));
        pool.WaitIdle();
        CHECK(pool.GetDiagnostics().CompletedJobs == 3);
    }
}

FO_END_NAMESPACE
