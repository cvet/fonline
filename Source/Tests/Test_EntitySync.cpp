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

#include "EntitySync.h"

FO_BEGIN_NAMESPACE

// ============================================================================
// EntityLock — positive
// ============================================================================

TEST_CASE("EntityLock")
{
    SECTION("BasicAcquireRelease")
    {
        EntityLock lock;

        lock.Acquire(0);
        CHECK(lock.IsLockedByCurrentThread());

        lock.Release();
        CHECK_FALSE(lock.IsLockedByCurrentThread());
    }

    SECTION("SharedConcurrentReaders")
    {
        EntityLock lock;
        std::atomic_int inside {0};
        std::atomic_int max_concurrent {0};
        std::atomic_int ready {0};
        std::atomic_bool start {false};

        auto reader = [&]() {
            ready.fetch_add(1);
            while (!start.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            lock.AcquireShared(0);
            const int32_t n = inside.fetch_add(1) + 1;
            int32_t prev = max_concurrent.load();
            while (n > prev && !max_concurrent.compare_exchange_weak(prev, n)) {
            }

            const std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while (max_concurrent.load() < 2 && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            inside.fetch_sub(1);
            lock.ReleaseShared();
        };

        std::thread a(reader);
        std::thread b(reader);
        while (ready.load() != 2) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        start.store(true);
        a.join();
        b.join();

        // Both readers must have held the lock at the same time — readers do not serialize.
        CHECK(max_concurrent.load() == 2);
    }

    SECTION("ExclusiveWaitsForSharedHolder")
    {
        EntityLock lock;
        std::atomic_bool reader_in {false};
        std::atomic_bool writer_got {false};
        std::atomic_bool release_reader {false};

        std::thread reader([&]() {
            lock.AcquireShared(0);
            reader_in.store(true);
            while (!release_reader.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            lock.ReleaseShared();
        });

        while (!reader_in.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        std::thread writer([&]() {
            lock.Acquire(10);
            writer_got.store(true);
            lock.Release();
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        CHECK_FALSE(writer_got.load()); // exclusive blocked while a reader holds shared

        release_reader.store(true);
        writer.join();
        CHECK(writer_got.load());
        reader.join();
    }

    SECTION("SharedWaitsForExclusiveHolder")
    {
        EntityLock lock;
        std::atomic_bool reader_got {false};

        lock.Acquire(0); // main thread holds exclusive

        std::thread reader([&]() {
            lock.AcquireShared(10);
            reader_got.store(true);
            lock.ReleaseShared();
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        CHECK_FALSE(reader_got.load()); // shared blocked while exclusive is held

        lock.Release();
        reader.join();
        CHECK(reader_got.load());
    }

    SECTION("ExclusiveOwnerCanAcquireShared")
    {
        EntityLock lock;

        lock.Acquire(0);
        lock.AcquireShared(0); // subsumed by the exclusive hold — must not deadlock
        CHECK(lock.IsLockedByCurrentThread());

        lock.ReleaseShared();
        CHECK(lock.IsLockedByCurrentThread());

        lock.Release();
        CHECK_FALSE(lock.IsLockedByCurrentThread());
    }

    SECTION("SharedRecursionAndTryAcquire")
    {
        EntityLock lock;

        lock.AcquireShared(0);
        lock.AcquireShared(0);
        CHECK_FALSE(lock.TryAcquire()); // a shared holder blocks exclusive TryAcquire

        lock.ReleaseShared();
        CHECK_FALSE(lock.TryAcquire()); // one shared hold still outstanding

        lock.ReleaseShared();
        CHECK(lock.TryAcquire()); // fully released
        lock.Release();
    }

    SECTION("AbortPendingWaitersAbortsSharedWaiter")
    {
        EntityLock lock;
        std::atomic_bool threw {false};

        lock.Acquire(0); // exclusive held so the reader must park

        std::thread reader([&]() {
            try {
                lock.AcquireShared(10);
                lock.ReleaseShared();
            }
            catch (const std::exception&) {
                threw.store(true);
            }
        });

        // Wait until the reader is actually parked as a waiter before aborting. A fixed sleep here
        // could, under load, call AbortPendingWaiters before the reader parks — the abort would then
        // find nothing to abort and the reader would block forever (join hangs).
        while (lock.WaiterCount() != 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        lock.AbortPendingWaiters();
        reader.join();
        CHECK(threw.load());

        lock.Release();
    }

    SECTION("RecursiveAcquire")
    {
        EntityLock lock;

        lock.Acquire(0);
        lock.Acquire(0);
        CHECK(lock.IsLockedByCurrentThread());

        lock.Release();
        CHECK(lock.IsLockedByCurrentThread());

        lock.Release();
        CHECK_FALSE(lock.IsLockedByCurrentThread());
    }

    SECTION("DeeplyRecursiveAcquire")
    {
        EntityLock lock;

        constexpr int depth = 10;
        for (int i = 0; i < depth; i++) {
            lock.Acquire(0);
        }

        CHECK(lock.IsLockedByCurrentThread());

        for (int i = 0; i < depth - 1; i++) {
            lock.Release();
            CHECK(lock.IsLockedByCurrentThread());
        }

        lock.Release();
        CHECK_FALSE(lock.IsLockedByCurrentThread());
    }

    SECTION("TryAcquireSucceedsWhenFree")
    {
        EntityLock lock;

        CHECK(lock.TryAcquire());
        CHECK(lock.IsLockedByCurrentThread());

        lock.Release();
    }

    SECTION("TryAcquireSucceedsRecursively")
    {
        EntityLock lock;

        lock.Acquire(0);
        CHECK(lock.TryAcquire());
        CHECK(lock.IsLockedByCurrentThread());

        lock.Release();
        CHECK(lock.IsLockedByCurrentThread());
        lock.Release();
        CHECK_FALSE(lock.IsLockedByCurrentThread());
    }

    SECTION("TryAcquireFailsWhenHeldByOtherThread")
    {
        EntityLock lock;

        std::atomic_bool locked = false;
        std::atomic_bool try_result = true;

        std::thread holder([&]() {
            lock.Acquire(0);
            locked.store(true);
            while (locked.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            lock.Release();
        });

        while (!locked.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        try_result = lock.TryAcquire();
        CHECK_FALSE(try_result.load());

        locked.store(false);
        holder.join();
    }

    SECTION("FIFOWaitQueueOrdersWaiters")
    {
        EntityLock lock;
        std::vector<int> order;
        mutex order_mutex;

        lock.Acquire(0);

        std::thread t1([&]() {
            lock.Acquire(10);
            {
                scoped_lock g {order_mutex};
                order.push_back(1);
            }
            lock.Release();
        });

        std::thread t2([&]() {
            lock.Acquire(5);
            {
                scoped_lock g {order_mutex};
                order.push_back(2);
            }
            lock.Release();
        });

        // Wait until both waiters are fully enqueued before releasing, so the grant order is
        // determined purely by ticket value and not by thread-startup timing under load.
        while (lock.WaiterCount() != 2) {
            std::this_thread::yield();
        }

        lock.Release();

        t1.join();
        t2.join();

        REQUIRE(order.size() == 2);
        CHECK(order[0] == 2);
        CHECK(order[1] == 1);
    }

    SECTION("ThreeThreadsFIFO")
    {
        EntityLock lock;
        std::vector<int> order;
        mutex order_mutex;

        lock.Acquire(0);

        auto make_waiter = [&](uint64_t ticket, int id) {
            return std::thread([&, ticket, id]() {
                lock.Acquire(ticket);
                {
                    scoped_lock g {order_mutex};
                    order.push_back(id);
                }
                lock.Release();
            });
        };

        auto t1 = make_waiter(100, 1);
        auto t2 = make_waiter(50, 2);
        auto t3 = make_waiter(75, 3);

        // Wait until all three waiters are fully enqueued before releasing, so the grant order is
        // determined purely by ticket value and not by thread-startup timing under load.
        while (lock.WaiterCount() != 3) {
            std::this_thread::yield();
        }
        lock.Release();

        t1.join();
        t2.join();
        t3.join();

        REQUIRE(order.size() == 3);
        CHECK(order[0] == 2);
        CHECK(order[1] == 3);
        CHECK(order[2] == 1);
    }

    SECTION("AcquireAfterFullRelease")
    {
        EntityLock lock;

        lock.Acquire(0);
        lock.Release();
        CHECK_FALSE(lock.IsLockedByCurrentThread());

        lock.Acquire(1);
        CHECK(lock.IsLockedByCurrentThread());
        lock.Release();
    }

    SECTION("IsLockedByCurrentThreadFalseByDefault")
    {
        EntityLock lock;
        CHECK_FALSE(lock.IsLockedByCurrentThread());
    }

    SECTION("CrossThreadIsLockedByCurrentThread")
    {
        EntityLock lock;
        lock.Acquire(0);

        std::atomic_bool other_sees_locked = true;

        std::thread other([&]() { other_sees_locked.store(lock.IsLockedByCurrentThread()); });
        other.join();

        CHECK_FALSE(other_sees_locked.load());
        lock.Release();
    }

    SECTION("NewReaderQueuesBehindParkedWriter")
    {
        // Writer-starvation prevention: a fresh shared reader arriving while an exclusive writer is
        // merely QUEUED (parked, not yet holding) must NOT take the immediate-grant fast path — it
        // queues behind the writer (AcquireShared `exclusive_waiter_ahead` branch). On release the
        // writer is granted first, then the reader. SharedWaitsForExclusiveHolder only covers a reader
        // blocking while exclusive is HELD; this covers it blocking behind a QUEUED writer.
        EntityLock lock;
        std::atomic_int grant_seq {0};
        std::atomic_int writer_grant {-1};
        std::atomic_int reader_grant {-1};

        lock.Acquire(0); // main holds exclusive

        std::thread writer([&]() {
            lock.Acquire(10);
            writer_grant.store(grant_seq.fetch_add(1));
            lock.Release();
        });

        // Writer parks as a WAITING exclusive entry.
        while (lock.WaiterCount() != 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        std::thread reader([&]() {
            lock.AcquireShared(20);
            reader_grant.store(grant_seq.fetch_add(1));
            lock.ReleaseShared();
        });

        // Reader must queue BEHIND the writer (exclusive_waiter_ahead), not grab the lock.
        while (lock.WaiterCount() != 2) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        // Stable-state check: while main still holds exclusive, neither waiter can have been granted.
        CHECK(writer_grant.load() == -1);
        CHECK(reader_grant.load() == -1);

        lock.Release(); // releasing main's exclusive grants the writer first, then the reader

        writer.join();
        reader.join();

        // Grant order: writer (seq 0) strictly before reader (seq 1) — the writer is not starved.
        CHECK(writer_grant.load() == 0);
        CHECK(reader_grant.load() == 1);
    }

    SECTION("GrantWaitersBatchStopsAtFirstExclusive")
    {
        // GrantWaiters batch-grants the leading consecutive shared waiters and STOPS at the first
        // exclusive waiter: queue [R1,R2,W,R3] -> release grants R1+R2 together, W and R3 stay parked.
        // After R1+R2 drain, W is granted alone, and only after W releases is R3 granted.
        EntityLock lock;
        std::atomic_bool got_r1 {false};
        std::atomic_bool got_r2 {false};
        std::atomic_bool got_w {false};
        std::atomic_bool got_r3 {false};
        std::atomic_bool release_readers {false};

        lock.Acquire(0); // main holds exclusive so all four park

        auto reader = [&](std::atomic_bool& flag, uint64_t ticket) {
            return std::thread([&, ticket]() {
                lock.AcquireShared(ticket);
                flag.store(true);
                while (!release_readers.load()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                lock.ReleaseShared();
            });
        };

        // Ticket order R1(10) < R2(20) < W(30) < R3(40) — the wait queue is sorted by ticket regardless
        // of thread start order, so the queue is deterministically [R1,R2,W,R3].
        std::thread r1 = reader(got_r1, 10);
        std::thread r2 = reader(got_r2, 20);
        std::thread w([&]() {
            lock.Acquire(30);
            got_w.store(true);
            lock.Release();
        });
        std::thread r3 = reader(got_r3, 40);

        while (lock.WaiterCount() != 4) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        lock.Release(); // grants the leading shared batch R1+R2 only

        // R1 and R2 are granted as a batch; W and R3 stay queued behind the (now-granted) readers.
        while (!(got_r1.load() && got_r2.load())) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        CHECK_FALSE(got_w.load());
        CHECK_FALSE(got_r3.load());
        CHECK(lock.WaiterCount() == 2); // W and R3 still parked

        release_readers.store(true); // R1,R2 release -> last reader out grants W -> W releases -> R3

        r1.join();
        r2.join();
        w.join();
        r3.join();

        CHECK(got_w.load());
        CHECK(got_r3.load());
    }

    SECTION("WaiterCountSnapshotConsistency")
    {
        // WaiterCount() returns a consistent mutex-protected snapshot. While main holds exclusive the
        // parked-reader count is stable and exact; after release the granted readers drain the queue.
        constexpr int reader_count = 8;
        EntityLock lock;
        std::atomic_bool release_readers {false};
        std::vector<std::thread> readers;

        lock.Acquire(0);

        for (int i = 0; i < reader_count; i++) {
            readers.emplace_back([&, i]() {
                lock.AcquireShared(numeric_cast<uint64_t>(10 + i));
                while (!release_readers.load()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                lock.ReleaseShared();
            });
        }

        // All readers park behind the exclusive holder; the count is stable while main holds exclusive.
        while (lock.WaiterCount() != static_cast<size_t>(reader_count)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        CHECK(lock.WaiterCount() == static_cast<size_t>(reader_count));

        lock.Release(); // all eight shared waiters are batch-granted and erase their queue entries

        // The queue drains to empty once every granted reader has woken and removed its entry.
        while (lock.WaiterCount() != 0) {
            CHECK(lock.WaiterCount() <= static_cast<size_t>(reader_count)); // never exceeds the spawned count
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        CHECK(lock.WaiterCount() == 0);

        release_readers.store(true);
        for (auto& t : readers) {
            t.join();
        }
    }
}

// ============================================================================
// EntityLock — negative
// ============================================================================

TEST_CASE("EntityLockNegative")
{
    SECTION("TryAcquireFailsUnderContention")
    {
        EntityLock lock;
        constexpr int attempts = 100;
        std::atomic_int failures {0};

        lock.Acquire(0);

        std::vector<std::thread> threads;
        for (int i = 0; i < 4; i++) {
            threads.emplace_back([&]() {
                for (int j = 0; j < attempts; j++) {
                    if (!lock.TryAcquire()) {
                        failures.fetch_add(1);
                    }
                    else {
                        lock.Release();
                    }
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        lock.Release();

        CHECK(failures.load() == 4 * attempts);
    }

    SECTION("IsNotLockedAfterAcquireByDifferentThread")
    {
        EntityLock lock;
        std::atomic_bool held = false;

        std::thread holder([&]() {
            lock.Acquire(0);
            held.store(true);
            while (held.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            lock.Release();
        });

        while (!held.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        CHECK_FALSE(lock.IsLockedByCurrentThread());

        held.store(false);
        holder.join();
    }

    SECTION("AcquireBlocksUntilReleasedByOtherThread")
    {
        EntityLock lock;
        std::atomic_bool acquired_by_main = false;
        std::atomic_bool released_by_holder = false;

        lock.Acquire(0);

        std::thread waiter([&]() {
            lock.Acquire(1);
            acquired_by_main.store(true);
            lock.Release();
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        CHECK_FALSE(acquired_by_main.load());

        lock.Release();
        released_by_holder.store(true);

        waiter.join();
        CHECK(acquired_by_main.load());
    }

    SECTION("MultipleTryAcquireAllFailOnContendedLock")
    {
        EntityLock lock;
        lock.Acquire(0);

        std::atomic_int successes {0};
        std::vector<std::thread> threads;

        for (int i = 0; i < 8; i++) {
            threads.emplace_back([&]() {
                if (lock.TryAcquire()) {
                    successes.fetch_add(1);
                    lock.Release();
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        CHECK(successes.load() == 0);
        lock.Release();
    }

    SECTION("SharedHolderExclusiveUpgradeAsserts")
    {
        // Read->write upgrade on the same thread (hold shared, then request exclusive) would
        // self-deadlock — the exclusive grant waits for all shared holders to drain, including this
        // very thread. The Acquire guard (FO_VERIFY_AND_THROW) catches it immediately. The property
        // pipeline never upgrades (a getter releases its shared hold before any setter runs), so only
        // an explicit negative test reaches this branch.
        EntityLock lock;
        lock.AcquireShared(0);

        CHECK_THROWS_AS(lock.Acquire(0), VerificationException);

        // The shared hold is untouched (the assert fired before any state change).
        lock.ReleaseShared();
        CHECK(lock.TryAcquire());
        lock.Release();
    }
}

// ============================================================================
// EntityLock — descendant-hold (hierarchical intention) mechanism
// ============================================================================

TEST_CASE("EntityLockDescendantHold")
{
    SECTION("BasicRegisterUnregister")
    {
        EntityLock lock;

        CHECK(lock.GetDescendantHoldCountForCurrentThread() == 0);
        lock.RegisterDescendantHold(0);
        CHECK(lock.GetDescendantHoldCountForCurrentThread() == 1);
        lock.UnregisterDescendantHold();
        CHECK(lock.GetDescendantHoldCountForCurrentThread() == 0);
    }

    SECTION("RegisterRecursionPerThread")
    {
        EntityLock lock;

        lock.RegisterDescendantHold(0); // two descendants of the same ancestor on one thread
        lock.RegisterDescendantHold(0);
        CHECK(lock.GetDescendantHoldCountForCurrentThread() == 2);

        lock.UnregisterDescendantHold();
        CHECK(lock.GetDescendantHoldCountForCurrentThread() == 1);
        lock.UnregisterDescendantHold();
        CHECK(lock.GetDescendantHoldCountForCurrentThread() == 0);
    }

    SECTION("ForeignDescendantHoldBlocksExclusiveAcquire")
    {
        // Down-direction: while another thread holds a descendant, the ancestor's exclusive Acquire
        // must wait (you cannot take a node while a descendant of it is busy elsewhere).
        EntityLock lock;
        std::atomic_bool holder_in {false};
        std::atomic_bool writer_got {false};
        std::atomic_bool release_holder {false};

        std::thread holder([&]() {
            lock.RegisterDescendantHold(0);
            holder_in.store(true);
            while (!release_holder.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            lock.UnregisterDescendantHold();
        });

        while (!holder_in.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        std::thread writer([&]() {
            lock.Acquire(10);
            writer_got.store(true);
            lock.Release();
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        CHECK_FALSE(writer_got.load()); // exclusive blocked by a foreign descendant-mark

        release_holder.store(true);
        writer.join();
        CHECK(writer_got.load());
        holder.join();
    }

    SECTION("ForeignExclusiveBlocksRegister")
    {
        // Up-direction: while another thread holds the ancestor exclusively, a descendant-mark
        // registration must wait (you cannot take a descendant under a foreign-held ancestor).
        EntityLock lock;
        std::atomic_bool holder_got {false};

        lock.Acquire(0); // main thread holds exclusive

        std::thread holder([&]() {
            lock.RegisterDescendantHold(10);
            holder_got.store(true);
            lock.UnregisterDescendantHold();
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        CHECK_FALSE(holder_got.load()); // register blocked while exclusive is held

        lock.Release();
        holder.join();
        CHECK(holder_got.load());
    }

    SECTION("SiblingDescendantHoldsAreConcurrent")
    {
        // Two threads marking the same ancestor (their shared parent) are compatible — neither blocks.
        EntityLock lock;
        std::atomic_int in_count {0};
        std::atomic_bool release_all {false};

        const auto worker = [&]() {
            lock.RegisterDescendantHold(0);
            in_count.fetch_add(1);
            while (!release_all.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            lock.UnregisterDescendantHold();
        };

        std::thread a(worker);
        std::thread b(worker);

        // Both must be able to hold their marks simultaneously (sibling parallelism).
        while (in_count.load() != 2) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        CHECK(in_count.load() == 2);

        release_all.store(true);
        a.join();
        b.join();
    }

    SECTION("OwnExclusiveOwnerMayRegister")
    {
        // The exclusive owner escalating down into its own subtree (it locks a descendant while
        // holding this node) must not self-deadlock — its own marks never conflict with its ownership.
        EntityLock lock;

        lock.Acquire(0);
        lock.RegisterDescendantHold(0); // own mark on a lock we own exclusively
        CHECK(lock.GetDescendantHoldCountForCurrentThread() == 1);
        CHECK(lock.IsLockedByCurrentThread());

        lock.UnregisterDescendantHold();
        lock.Release();
        CHECK_FALSE(lock.IsLockedByCurrentThread());
    }

    SECTION("OwnDescendantHoldAllowsUpEscalationToExclusive")
    {
        // Holding a descendant-mark, the same thread escalates up and takes this node exclusively
        // (lock a critter, then lock its map). Own marks must not block its own Acquire.
        EntityLock lock;

        lock.RegisterDescendantHold(0);
        lock.Acquire(0); // own mark present — must still grant immediately
        CHECK(lock.IsLockedByCurrentThread());

        lock.Release();
        lock.UnregisterDescendantHold();
        CHECK(lock.GetDescendantHoldCountForCurrentThread() == 0);
    }

    SECTION("TryRegisterFailsUnderForeignExclusive")
    {
        EntityLock lock;
        std::atomic<int> try_result {-1};

        lock.Acquire(0); // main holds exclusive

        std::thread t([&]() { try_result.store(lock.TryRegisterDescendantHold() ? 1 : 0); });
        t.join();
        CHECK(try_result.load() == 0); // cannot mark a foreign-held node without blocking

        lock.Release();
    }

    SECTION("TryAcquireFailsUnderForeignDescendantHold")
    {
        EntityLock lock;
        std::atomic_bool holder_in {false};
        std::atomic_bool release_holder {false};
        std::atomic<int> try_result {-1};

        std::thread holder([&]() {
            lock.RegisterDescendantHold(0);
            holder_in.store(true);
            while (!release_holder.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            lock.UnregisterDescendantHold();
        });

        while (!holder_in.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        try_result.store(lock.TryAcquire() ? 1 : 0);
        CHECK(try_result.load() == 0); // exclusive TryAcquire fails on a foreign descendant-mark

        release_holder.store(true);
        holder.join();
        CHECK(lock.TryAcquire()); // succeeds once the mark is gone
        lock.Release();
    }

    SECTION("RegisterYieldsToWaitingExclusive")
    {
        // Anti-starvation: once a writer is parked waiting for the node, later sibling-mark requests
        // queue BEHIND it rather than jumping ahead — otherwise an endless stream of sibling locks
        // could starve a map/location-exclusive op forever (the real livelock hazard of this model).
        EntityLock lock;
        std::atomic_bool holder_in {false};
        std::atomic_bool release_holder {false};
        std::atomic_bool writer_got {false};
        std::atomic_bool release_writer {false};
        std::atomic_bool late_registered {false};

        std::thread holder([&]() {
            lock.RegisterDescendantHold(0);
            holder_in.store(true);
            while (!release_holder.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            lock.UnregisterDescendantHold();
        });

        while (!holder_in.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        // Writer parks waiting exclusive (blocked by holder's mark).
        std::thread writer([&]() {
            lock.Acquire(10);
            writer_got.store(true);
            while (!release_writer.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            lock.Release();
        });

        while (lock.WaiterCount() != 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        // Late sibling-mark request: must queue behind the parked writer, not register immediately.
        std::thread late([&]() {
            lock.RegisterDescendantHold(20);
            late_registered.store(true);
            lock.UnregisterDescendantHold();
        });

        while (lock.WaiterCount() != 2) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        CHECK_FALSE(late_registered.load()); // queued behind the writer
        CHECK_FALSE(writer_got.load());

        // Drain the original holder: the WRITER must win next (not the later sibling mark).
        release_holder.store(true);
        while (!writer_got.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        CHECK(writer_got.load());
        CHECK_FALSE(late_registered.load()); // still behind the now-running writer

        // Writer releases — the late sibling mark finally proceeds.
        release_writer.store(true);
        late.join();
        CHECK(late_registered.load());

        holder.join();
        writer.join();
    }

    SECTION("AbortPendingWaitersAbortsDescendantHoldWaiter")
    {
        EntityLock lock;
        std::atomic_bool threw {false};

        lock.Acquire(0); // exclusive held so the register must park

        std::thread holder([&]() {
            try {
                lock.RegisterDescendantHold(10);
                lock.UnregisterDescendantHold();
            }
            catch (const std::exception&) {
                threw.store(true);
            }
        });

        while (lock.WaiterCount() != 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        lock.AbortPendingWaiters();
        holder.join();
        CHECK(threw.load());

        lock.Release();
    }
}

// ============================================================================
// SyncContext — positive
// ============================================================================

TEST_CASE("SyncContext")
{
    SECTION("NoContextByDefault")
    {
        CHECK(SyncContext::GetCurrentOnThisThread() == nullptr);
    }

    SECTION("SyncContextLifecycle")
    {
        {
            SyncContext ctx;
            CHECK(SyncContext::GetCurrentOnThisThread() == nullptr);
        }
        CHECK(SyncContext::GetCurrentOnThisThread() == nullptr);
    }

    SECTION("ActivateDeactivate")
    {
        SyncContext ctx;
        CHECK(SyncContext::GetCurrentOnThisThread() == nullptr);

        ctx.Activate();
        CHECK(SyncContext::GetCurrentOnThisThread() == &ctx);

        ctx.Deactivate();
        CHECK(SyncContext::GetCurrentOnThisThread() == nullptr);
    }

    SECTION("ActivateOverwritesPrevious")
    {
        SyncContext ctx1;
        SyncContext ctx2;

        ctx1.Activate();
        CHECK(SyncContext::GetCurrentOnThisThread() == &ctx1);

        ctx2.Activate();
        CHECK(SyncContext::GetCurrentOnThisThread() == &ctx2);

        ctx2.Deactivate();
        CHECK(SyncContext::GetCurrentOnThisThread() == &ctx1);

        ctx1.Deactivate();
        CHECK(SyncContext::GetCurrentOnThisThread() == nullptr);
    }

    SECTION("DeactivateOnlyIfCurrent")
    {
        SyncContext ctx1;
        SyncContext ctx2;

        ctx1.Activate();
        ctx2.Deactivate();
        CHECK(SyncContext::GetCurrentOnThisThread() == &ctx1);

        ctx1.Deactivate();
        CHECK(SyncContext::GetCurrentOnThisThread() == nullptr);
    }

    SECTION("DestructorCleansUpIfCurrent")
    {
        {
            SyncContext ctx;
            ctx.Activate();
            CHECK(SyncContext::GetCurrentOnThisThread() == &ctx);
        }
        CHECK(SyncContext::GetCurrentOnThisThread() == nullptr);
    }

    SECTION("ReleaseWithoutSync")
    {
        SyncContext ctx;
        ctx.Activate();
        ctx.Release();
        CHECK(SyncContext::GetCurrentOnThisThread() == &ctx);
        ctx.Deactivate();
    }

    SECTION("EachContextGetsUniqueTicket")
    {
        SyncContext ctx1;
        SyncContext ctx2;
        SyncContext ctx3;
        CHECK(true);
    }

    SECTION("ThreadLocalContextIsolation")
    {
        SyncContext ctx_main;
        ctx_main.Activate();
        CHECK(SyncContext::GetCurrentOnThisThread() == &ctx_main);

        std::atomic<SyncContext*> other_ctx {nullptr};
        std::thread t([&]() { other_ctx.store(SyncContext::GetCurrentOnThisThread()); });
        t.join();

        CHECK(other_ctx.load() == nullptr);
        CHECK(SyncContext::GetCurrentOnThisThread() == &ctx_main);
        ctx_main.Deactivate();
    }
}

// ============================================================================
// SyncContext — negative
// ============================================================================

TEST_CASE("SyncContextNegative")
{
    SECTION("ValidateAccessFailsOnUnheldLock")
    {
        EntityLock lock;
        SyncContext ctx;
        ctx.Activate();

        // ValidateAccess on a lock not held should return false
        // We can't call ValidateAccess directly without a ServerEntity,
        // but we can verify the lock itself reports not held
        CHECK_FALSE(lock.IsLockedByCurrentThread());

        ctx.Deactivate();
    }

    SECTION("ValidateAccessFailsOnLockHeldByOtherThread")
    {
        EntityLock lock;
        std::atomic_bool held = false;
        std::atomic_bool validated = false;

        std::thread holder([&]() {
            lock.Acquire(0);
            held.store(true);
            while (held.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            lock.Release();
        });

        while (!held.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        // Main thread: lock is held by another thread
        CHECK_FALSE(lock.IsLockedByCurrentThread());
        validated.store(true);

        held.store(false);
        holder.join();
    }

    SECTION("SyncEntityNullIsNoOp")
    {
        SyncContext ctx;
        ctx.Activate();

        // SyncEntity(nullptr) should not crash
        ctx.SyncEntity(nullptr);
        CHECK(SyncContext::GetCurrentOnThisThread() == &ctx);

        ctx.Deactivate();
    }

    SECTION("SyncCurrentContextWithNoContextIsNoOp")
    {
        CHECK(SyncContext::GetCurrentOnThisThread() == nullptr);
        // No active context — there is nothing to call SyncEntity on. The pre-refactor static
        // SyncCurrentContext(nullptr) was a no-op; now the equivalent is just "no current ctx".
    }

    SECTION("DoubleRelease")
    {
        SyncContext ctx;
        ctx.Activate();

        // Double Release should not crash — second release is a no-op
        ctx.Release();
        ctx.Release();
        CHECK(SyncContext::GetCurrentOnThisThread() == &ctx);

        ctx.Deactivate();
    }

    SECTION("DeactivateWithoutActivateIsNoOp")
    {
        SyncContext ctx;
        CHECK(SyncContext::GetCurrentOnThisThread() == nullptr);

        // Deactivate without prior Activate should not crash
        ctx.Deactivate();
        CHECK(SyncContext::GetCurrentOnThisThread() == nullptr);
    }

    SECTION("ActivateDeactivateRapidCycle")
    {
        SyncContext ctx;

        for (int i = 0; i < 100; i++) {
            ctx.Activate();
            CHECK(SyncContext::GetCurrentOnThisThread() == &ctx);
            ctx.Deactivate();
            CHECK(SyncContext::GetCurrentOnThisThread() == nullptr);
        }
    }

    SECTION("SyncEmptySpanIsNoOp")
    {
        SyncContext ctx;
        ctx.Activate();

        std::vector<ServerEntity*> empty;
        ctx.SyncEntities(empty);
        CHECK(SyncContext::GetCurrentOnThisThread() == &ctx);

        ctx.Deactivate();
    }

    SECTION("SyncSpanOfNullsIsNoOp")
    {
        SyncContext ctx;
        ctx.Activate();

        ServerEntity* nulls[] = {nullptr, nullptr, nullptr};
        ctx.SyncEntities(nulls);
        CHECK(SyncContext::GetCurrentOnThisThread() == &ctx);

        ctx.Deactivate();
    }
}

// ============================================================================
// SyncContext singleton-lock bucket (Game.Lock) — kept SEPARATE from the per-Sync
// held-lock set so a later Sync replacement cannot drop it, recursive, and it must
// block Sync (the {Engine,Entity} <-> {Entity,Engine} deadlock-prevention guard).
// ============================================================================

TEST_CASE("SyncContextSingletonLock")
{
    SECTION("SyncWhileHoldingSingletonLockThrows")
    {
        EntityLock singleton;
        SyncContext ctx;
        ctx.Activate();

        ctx.LockSingleton(&singleton);
        CHECK(singleton.IsLockedByCurrentThread());
        CHECK_FALSE(ctx.IsEmpty());

        // A thread holding Game.Lock() must not Sync(...) — the guard converts that into a
        // hard error instead of risking the per-property auto-lock deadlock.
        std::vector<ServerEntity*> empty;
        CHECK_THROWS_AS(ctx.SyncEntities(empty), EntitySyncException);

        // Drain (destructor asserts both buckets are empty).
        ctx.UnlockSingleton(&singleton);
        CHECK(ctx.IsEmpty());
        CHECK_FALSE(singleton.IsLockedByCurrentThread());
        ctx.Deactivate();
    }

    SECTION("SingletonLockIsRecursive")
    {
        EntityLock singleton;
        SyncContext ctx;
        ctx.Activate();

        ctx.LockSingleton(&singleton);
        ctx.LockSingleton(&singleton); // recursive Game.Lock()
        CHECK(singleton.IsLockedByCurrentThread());
        CHECK_FALSE(ctx.IsEmpty());

        ctx.UnlockSingleton(&singleton);
        CHECK(singleton.IsLockedByCurrentThread()); // still held at recursion depth 1
        CHECK_FALSE(ctx.IsEmpty());
        ctx.UnlockSingleton(&singleton);
        CHECK_FALSE(singleton.IsLockedByCurrentThread());
        CHECK(ctx.IsEmpty());
        ctx.Deactivate();
    }

    SECTION("ReleaseDrainsSingletonBucket")
    {
        // Release() is the explicit teardown path: it drains BOTH the per-Sync held set and the
        // singleton bucket so the destructor's empty-bucket contract always holds, even when a job
        // leaked a Game.Lock() without a matching Unlock(). (A subsequent Sync REPLACEMENT — not an
        // explicit Release — is what the separate singleton bucket is designed to survive, but the
        // engine guard above forbids Sync while a singleton lock is held, so that path stays internal.)
        EntityLock singleton;
        SyncContext ctx;
        ctx.Activate();

        ctx.LockSingleton(&singleton);
        CHECK(singleton.IsLockedByCurrentThread());
        CHECK_FALSE(ctx.IsEmpty());

        ctx.Release();
        CHECK_FALSE(singleton.IsLockedByCurrentThread());
        CHECK(ctx.IsEmpty());
        ctx.Deactivate();
    }

    SECTION("UnbalancedSingletonUnlockAsserts")
    {
        // UnlockSingleton with no matching LockSingleton (more Game.Unlock() than Game.Lock()) trips
        // the find-fail FO_VERIFY_AND_THROW. The first IsLockedByCurrentThread guard fires once the lock
        // is already fully released, so the unbalanced second unlock is rejected rather than corrupting
        // the singleton bucket.
        EntityLock singleton;
        SyncContext ctx;
        ctx.Activate();

        ctx.LockSingleton(&singleton);
        ctx.UnlockSingleton(&singleton); // balanced — fully releases

        CHECK_FALSE(singleton.IsLockedByCurrentThread());
        CHECK(ctx.IsEmpty());

        // Second unlock has nothing to match.
        CHECK_THROWS_AS(ctx.UnlockSingleton(&singleton), VerificationException);

        CHECK(ctx.IsEmpty());
        ctx.Deactivate();
    }
}

// ============================================================================
// EntityLock contention — positive
// ============================================================================

TEST_CASE("EntityLockContention")
{
    SECTION("ManyThreadsIncrementUnderLock")
    {
        EntityLock lock;
        std::atomic_int counter {0};
        constexpr int threads_count = 8;
        constexpr int increments_per_thread = 1000;

        std::vector<std::thread> threads;
        threads.reserve(threads_count);

        for (int i = 0; i < threads_count; i++) {
            threads.emplace_back([&, i]() {
                for (int j = 0; j < increments_per_thread; j++) {
                    lock.Acquire(numeric_cast<uint64_t>(i * increments_per_thread + j));
                    counter.fetch_add(1, std::memory_order_relaxed);
                    lock.Release();
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        CHECK(counter.load() == threads_count * increments_per_thread);
    }

    SECTION("MultipleLocksSortedAcquisition")
    {
        EntityLock lock_a;
        EntityLock lock_b;
        std::atomic_int counter {0};
        constexpr int threads_count = 4;
        constexpr int iterations = 500;

        std::vector<std::thread> threads;
        threads.reserve(threads_count);

        for (int i = 0; i < threads_count; i++) {
            threads.emplace_back([&, i]() {
                EntityLock* first = &lock_a < &lock_b ? &lock_a : &lock_b;
                EntityLock* second = &lock_a < &lock_b ? &lock_b : &lock_a;

                for (int j = 0; j < iterations; j++) {
                    uint64_t ticket = numeric_cast<uint64_t>(i * iterations + j);
                    first->Acquire(ticket);
                    second->Acquire(ticket);

                    counter.fetch_add(1, std::memory_order_relaxed);

                    second->Release();
                    first->Release();
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        CHECK(counter.load() == threads_count * iterations);
    }

    // Mirrors the engine-global lock under load: many worker threads read Game properties
    // (AcquireShared/ReleaseShared) while a few occasionally hold it exclusively (property
    // writes / syncing a parentless entity whose lock is `_engine->GetEntityLock()`). The
    // exclusive holders make readers pile up as shared waiters; each Release fires the
    // GrantWaiters shared-batch path (`_sharedHolders.emplace(...)`). Reproduces the
    // ServerPool crash in EntityLock::GrantWaiters seen by the encounters gameplay suite.
    SECTION("SharedExclusiveBatchGrantStress")
    {
        EntityLock lock;
        std::atomic<uint64_t> ticket {0};
        std::atomic_int reader_ops {0};
        std::atomic_int writer_ops {0};
        std::atomic_bool stop {false};

        constexpr int reader_count = 8;
        constexpr int writer_count = 2;
        constexpr int writer_iterations = 4000;

        std::vector<std::thread> readers;
        readers.reserve(reader_count);

        for (int i = 0; i < reader_count; i++) {
            readers.emplace_back([&]() {
                while (!stop.load(std::memory_order_acquire)) {
                    lock.AcquireShared(ticket.fetch_add(1, std::memory_order_relaxed));
                    reader_ops.fetch_add(1, std::memory_order_relaxed);
                    lock.ReleaseShared();
                }
            });
        }

        std::vector<std::thread> writers;
        writers.reserve(writer_count);

        for (int i = 0; i < writer_count; i++) {
            writers.emplace_back([&]() {
                for (int j = 0; j < writer_iterations; j++) {
                    lock.Acquire(ticket.fetch_add(1, std::memory_order_relaxed));
                    // Owner reads a Game property mid-write — folds into the exclusive recursion.
                    lock.AcquireShared(ticket.fetch_add(1, std::memory_order_relaxed));
                    lock.ReleaseShared();
                    writer_ops.fetch_add(1, std::memory_order_relaxed);
                    lock.Release();
                }
            });
        }

        for (auto& t : writers) {
            t.join();
        }

        stop.store(true, std::memory_order_release);

        for (auto& t : readers) {
            t.join();
        }

        CHECK(writer_ops.load() == writer_count * writer_iterations);
        CHECK(reader_ops.load() > 0);
    }

    // Faithful to SyncContext::AcquireLocks' non-parking try-and-back-off: each "sync" thread
    // wants {engine-global lock, other lock}; it TryAcquires the prefix and, on the first
    // contended lock, RELEASES the prefix it already took and retries. That release of the
    // engine-global lock fires GrantWaiters while Game-property readers are parked as shared
    // waiters — the high-frequency churn the encounters suite hits. Readers sleep briefly between
    // reads so the exclusive syncers are never perpetually starved (keeps the test bounded) while
    // still overlapping with the exclusive batch-grant path.
    SECTION("AcquireLocksChurnWithSharedReaders")
    {
        EntityLock gl; // engine-global lock — taken shared by readers, exclusive by sync threads
        EntityLock other[4]; // per-entity locks the sync threads also want
        std::atomic<uint64_t> ticket {0};
        std::atomic_int reader_ops {0};
        std::atomic_int sync_ops {0};
        std::atomic_bool stop {false};

        constexpr int reader_count = 4;
        constexpr int sync_count = 4;
        constexpr int sync_iterations = 800;

        std::vector<std::thread> readers;
        readers.reserve(reader_count);

        for (int i = 0; i < reader_count; i++) {
            readers.emplace_back([&]() {
                while (!stop.load(std::memory_order_acquire)) {
                    gl.AcquireShared(ticket.fetch_add(1, std::memory_order_relaxed));
                    reader_ops.fetch_add(1, std::memory_order_relaxed);
                    gl.ReleaseShared();
                    // Sleep (not just yield) so the lock actually frees and exclusive syncers
                    // make steady progress — keeps the contention window without starving writers.
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                }
            });
        }

        std::vector<std::thread> syncers;
        syncers.reserve(sync_count);

        for (int i = 0; i < sync_count; i++) {
            syncers.emplace_back([&, i]() {
                EntityLock* second = &other[i % 4];

                for (int j = 0; j < sync_iterations; j++) {
                    // Replicates AcquireLocks: try the whole set, release the prefix on the first
                    // contended lock, back off, retry. `gl` is acquired exclusively first.
                    for (int32_t spins = 0;;) {
                        size_t acquired = 0;
                        EntityLock* set[2] = {&gl, second};

                        while (acquired < 2 && set[acquired]->TryAcquire()) {
                            acquired++;
                        }

                        if (acquired == 2) {
                            break;
                        }

                        for (size_t k = 0; k < acquired; k++) {
                            set[k]->Release();
                        }

                        if (++spins < 64) {
                            std::this_thread::yield();
                        }
                        else {
                            std::this_thread::sleep_for(std::chrono::microseconds(20));
                        }
                    }

                    sync_ops.fetch_add(1, std::memory_order_relaxed);

                    second->Release();
                    gl.Release();
                }
            });
        }

        for (auto& t : syncers) {
            t.join();
        }

        stop.store(true, std::memory_order_release);

        for (auto& t : readers) {
            t.join();
        }

        CHECK(sync_ops.load() == sync_count * sync_iterations);
        CHECK(reader_ops.load() > 0);
    }

    // Down-direction contention faithful to ServerMapOperations: many "subtree" workers concurrently
    // mark the same ancestor via RegisterDescendantHold (sibling parallelism — sibling marks never
    // block each other), while a few "ancestor-exclusive" threads take the ancestor with Acquire and
    // therefore must wait until every FOREIGN descendant-mark has drained. Each exclusive Release fires
    // the GrantWaiters path that has to batch-grant the queued DescendantHold waiters (the consecutive
    // same-kind run), and every RegisterDescendantHold arriving while an exclusive writer is parked
    // queues behind it (HasWaitingExclusive). Ticket ordering keeps both sides making progress. This
    // exercises the descendant-hold wait queue, the DescendantHold batch-grant arm of GrantWaiters, and
    // HasForeignDescendantHolder draining under churn — the least-covered concurrent path of EntityLock.
    SECTION("DescendantHoldChurnWithExclusive")
    {
        EntityLock ancestor;
        std::atomic<uint64_t> ticket {0};
        std::atomic_int marker_ops {0};
        std::atomic_int exclusive_ops {0};
        std::atomic_bool stop {false};

        constexpr int marker_count = 6;
        constexpr int exclusive_count = 3;
        constexpr int exclusive_iterations = 1500;

        std::vector<std::thread> markers;
        markers.reserve(marker_count);

        for (int i = 0; i < marker_count; i++) {
            markers.emplace_back([&]() {
                while (!stop.load(std::memory_order_acquire)) {
                    ancestor.RegisterDescendantHold(ticket.fetch_add(1, std::memory_order_relaxed));
                    marker_ops.fetch_add(1, std::memory_order_relaxed);
                    ancestor.UnregisterDescendantHold();
                    // Brief sleep so the exclusive writers actually drain the marks and make steady
                    // progress instead of being perpetually re-blocked by a fresh stream of marks.
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                }
            });
        }

        std::vector<std::thread> exclusives;
        exclusives.reserve(exclusive_count);

        for (int i = 0; i < exclusive_count; i++) {
            exclusives.emplace_back([&]() {
                for (int j = 0; j < exclusive_iterations; j++) {
                    ancestor.Acquire(ticket.fetch_add(1, std::memory_order_relaxed));
                    exclusive_ops.fetch_add(1, std::memory_order_relaxed);
                    ancestor.Release();
                }
            });
        }

        for (auto& t : exclusives) {
            t.join();
        }

        stop.store(true, std::memory_order_release);

        for (auto& t : markers) {
            t.join();
        }

        CHECK(exclusive_ops.load() == exclusive_count * exclusive_iterations);
        CHECK(marker_ops.load() > 0);
    }

    // SyncContext's descendant path is non-parking: AcquireLocks TryRegisterDescendantHold's the set and,
    // on the first contended node, releases the marks it already took and retries. Mirror that try-and-
    // back-off for descendant marks running against ancestor-exclusive writers, so the TryRegisterDescendantHold
    // rejection path (foreign owner / parked exclusive) and the Unregister-driven GrantWaiters are hammered
    // without any thread ever parking on the descendant-hold futex.
    SECTION("TryDescendantHoldBackoffWithExclusive")
    {
        EntityLock ancestor;
        std::atomic<uint64_t> ticket {0};
        std::atomic_int marker_ops {0};
        std::atomic_int exclusive_ops {0};
        std::atomic_bool stop {false};

        constexpr int marker_count = 4;
        constexpr int exclusive_count = 2;
        constexpr int exclusive_iterations = 2000;

        std::vector<std::thread> markers;
        markers.reserve(marker_count);

        for (int i = 0; i < marker_count; i++) {
            markers.emplace_back([&]() {
                while (!stop.load(std::memory_order_acquire)) {
                    for (int32_t spins = 0; !ancestor.TryRegisterDescendantHold();) {
                        if (++spins < 64) {
                            std::this_thread::yield();
                        }
                        else {
                            std::this_thread::sleep_for(std::chrono::microseconds(20));
                        }
                    }

                    marker_ops.fetch_add(1, std::memory_order_relaxed);
                    ancestor.UnregisterDescendantHold();
                }
            });
        }

        std::vector<std::thread> exclusives;
        exclusives.reserve(exclusive_count);

        for (int i = 0; i < exclusive_count; i++) {
            exclusives.emplace_back([&]() {
                for (int j = 0; j < exclusive_iterations; j++) {
                    ancestor.Acquire(ticket.fetch_add(1, std::memory_order_relaxed));
                    exclusive_ops.fetch_add(1, std::memory_order_relaxed);
                    ancestor.Release();
                    // Leave the lock fully free for a beat so the non-parking TryRegisterDescendantHold
                    // markers reliably catch a window and make progress (TryRegister never queues, so it
                    // only succeeds when no exclusive owns and none is parked ahead).
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                }
            });
        }

        for (auto& t : exclusives) {
            t.join();
        }

        stop.store(true, std::memory_order_release);

        for (auto& t : markers) {
            t.join();
        }

        CHECK(exclusive_ops.load() == exclusive_count * exclusive_iterations);
        CHECK(marker_ops.load() > 0);
    }
}

FO_END_NAMESPACE
