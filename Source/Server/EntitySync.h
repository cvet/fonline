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
//

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(EntitySyncException);
FO_DECLARE_EXCEPTION_EXT(EntityLockWaitAbortedException, EntitySyncException);

class ServerEngine;
class ServerEntity;

[[nodiscard]] auto NextSyncTicket() noexcept -> uint64_t;

// Single-pass coverage check: does the current thread hold a lock covering `entity` (its own lock or a
// lock-free ancestor on its parent / widen chain)? The access path keeps diagnostics enabled so uncovered
// entities dump their parent chain before the caller throws; Game.IsEntityLocked passes diagnose=false
// because it is a deliberate non-throwing probe. NOTE: this is the read-path notion; mutating an entity
// (SetParent reparent) requires holding its OWN lock directly, a stricter check made inline there.
[[nodiscard]] auto IsEntityAccessValid(nptr<const ServerEntity> entity, bool diagnose = true) noexcept -> bool;

class EntityLock final
{
public:
    EntityLock();
    EntityLock(const EntityLock&) = delete;
    EntityLock(EntityLock&&) noexcept = delete;
    auto operator=(const EntityLock&) = delete;
    auto operator=(EntityLock&&) noexcept = delete;
    ~EntityLock();

    [[nodiscard]] auto IsLockedByCurrentThread() const noexcept -> bool;
    [[nodiscard]] auto WaiterCount() const noexcept -> size_t;

    // Number of times the calling thread currently holds this lock EXCLUSIVELY (its recursion
    // count), or 0 if it is not the exclusive owner. Used by the globally-ordered escalation in
    // `SyncContext::AcquireLocks` to release a parent-held lock down to zero and re-acquire it the
    // same number of times, restoring the parent context's recursion bookkeeping exactly.
    auto GetExclusiveRecursionForCurrentThread() const noexcept -> int32_t;

    // Exclusive ("write") acquisition. One owner thread at a time; re-entrant on that thread via
    // `_recursionCount`. Blocks until no shared holders and no other exclusive owner remain.
    void Acquire(uint64_t ticket);
    auto TryAcquire() -> bool;
    void Release() noexcept;

    // Shared ("read") acquisition — multiple threads may hold it concurrently, but it is mutually
    // exclusive with `Acquire`. Re-entrant per thread. If the calling thread already owns the lock
    // exclusively, the shared request is subsumed into the exclusive recursion (a writer trivially
    // has read access). Used for Game-singleton property reads so concurrent readers don't serialize
    // on the engine-global lock; writes (property setters) and `Game.Lock()` stay exclusive.
    void AcquireShared(uint64_t ticket);
    void ReleaseShared() noexcept;

    // Descendant-busy ("intention") marking — the hierarchical-exclusion mechanism. A thread that
    // takes an entity's lock also Register's a descendant-hold on every SEPARATE-lock ancestor (a
    // critter on its map and location, etc.), recording "this thread holds a descendant of you".
    // This is NOT a shared *access* mode: nobody reads the entity through it; it is pure bookkeeping
    // so an ancestor's exclusive `Acquire` knows a descendant is busy and queues. Properties:
    //   - Compatible among threads: two siblings each mark their shared parent → parallel.
    //   - Conflicts with a foreign exclusive owner BOTH ways: registering blocks while another thread
    //     holds this lock exclusively (a descendant can't be taken under a foreign-held ancestor), and
    //     `Acquire` blocks while a FOREIGN thread holds a descendant-mark here (an ancestor can't be
    //     taken while a descendant is busy elsewhere).
    //   - Re-entrant per thread (locking two descendants of the same ancestor marks it twice); the
    //     same thread may also escalate up into an ancestor it already marks (own marks never block).
    //   - Registration yields to an already-waiting exclusive writer (queues behind it) so a
    //     map/location-exclusive op cannot be starved forever by a stream of sibling locks.
    void RegisterDescendantHold(uint64_t ticket);
    auto TryRegisterDescendantHold() -> bool;
    void UnregisterDescendantHold() noexcept;

    // Number of descendant-holds the calling thread currently has on this lock (0 if none). Mirrors
    // GetExclusiveRecursionForCurrentThread: lets the globally-ordered escalation release a marked
    // ancestor down to zero and re-mark it the same number of times, restoring intention bookkeeping.
    auto GetDescendantHoldCountForCurrentThread() const noexcept -> int32_t;

    // Marks this lock as shutting down and force-aborts every parked waiter. Each `WaitEntry`'s
    // state atomic CAS-flips from "waiting" to "aborted", then `notify_one` wakes its single
    // blocked thread (FIFO precision preserved — no thundering herd). The flag also rejects any
    // future `Acquire` so a tick that fires after shutdown started can't deadlock on the empty
    // owner field. `Server::Shutdown` walks every entity once and calls this on each lock.
    void AbortPendingWaiters() noexcept;

    // Internal state-mutex handles plus a single Ensure operation split into a read-only
    // compatibility check and a no-throw ownership commit. SyncContext's one-shot multi-op Ensure
    // transaction and lock-contention tests drive these; hold the state mutex only across the
    // paired check/commit, never across a blocking operation.
    void LockStateMutex() FO_TSA_ACQUIRE(_mutex);
    [[nodiscard]] bool TryLockStateMutex() FO_TSA_TRY_ACQUIRE(true, _mutex);
    void UnlockStateMutex() noexcept FO_TSA_RELEASE(_mutex);
    [[nodiscard]] bool IsEnsureOpCompatible(bool is_exclusive) const noexcept FO_TSA_REQUIRES(_mutex);
    void CommitEnsureOp(bool is_exclusive) noexcept FO_TSA_REQUIRES(_mutex);

private:
    // Which kind of acquisition a queued waiter wants. Exclusive and Shared are the classic
    // reader/writer modes (Shared used only for Game-singleton property reads). DescendantHold is the
    // hierarchical intention mark — it conflicts with a foreign Exclusive both ways but is compatible
    // with other DescendantHolds (sibling parallelism). Shared and DescendantHold never coexist on the
    // same lock (a Game singleton is in no entity's parent chain), so their grant logic stays disjoint.
    enum class WaitKind : uint8_t
    {
        Exclusive,
        Shared,
        DescendantHold,
    };

    struct WaitEntry
    {
        // State machine:
        //   0 = parked on `wait(0)`, no decision yet
        //   1 = lock granted by Release; on wake the waiter removes itself and returns
        //   2 = aborted by AbortPendingWaiters; on wake the waiter removes itself and throws
        // Atomicity carries the cross-thread hand-off; CAS in Release / AbortPendingWaiters
        // disambiguates the abort-races-grant window.
        static constexpr int32_t STATE_WAITING = 0;
        static constexpr int32_t STATE_GRANTED = 1;
        static constexpr int32_t STATE_ABORTED = 2;

        uint64_t Ticket {};
        std::thread::id Waiter {};
        WaitKind Kind {WaitKind::Exclusive};
        std::atomic<int32_t> State {STATE_WAITING};
    };

    // Per-thread hold counters. Entities number in the millions while concurrent holders per lock
    // number in the low single digits, so a linear inline vector replaces a hash map here: an idle
    // lock allocates nothing, whereas a per-entity hash map eagerly allocates 4 KB of bucket storage
    // — gigabytes across a loaded world (and past ThreadSanitizer's per-size-class allocator cap).
    // Exposes the unordered_map subset EntityLock uses; emplace assumes the key is absent.
    class ThreadHoldCounts final
    {
    public:
        using value_type = pair<std::thread::id, int32_t>;
        using storage_type = small_vector<value_type, 2>;

        [[nodiscard]] auto find(std::thread::id id) noexcept { return std::ranges::find(_entries, id, &value_type::first); }
        [[nodiscard]] auto find(std::thread::id id) const noexcept { return std::ranges::find(_entries, id, &value_type::first); }
        [[nodiscard]] auto contains(std::thread::id id) const noexcept -> bool { return find(id) != _entries.end(); }
        [[nodiscard]] auto empty() const noexcept -> bool { return _entries.empty(); }
        [[nodiscard]] auto size() const noexcept -> size_t { return _entries.size(); }
        [[nodiscard]] auto begin() noexcept { return _entries.begin(); }
        [[nodiscard]] auto begin() const noexcept { return _entries.begin(); }
        [[nodiscard]] auto end() noexcept { return _entries.end(); }
        [[nodiscard]] auto end() const noexcept { return _entries.end(); }

        [[nodiscard]] auto operator[](std::thread::id id) -> int32_t&
        {
            if (auto it = find(id); it != _entries.end()) {
                return it->second;
            }

            return _entries.emplace_back(id, 0).second;
        }

        void emplace(std::thread::id id, int32_t count) { _entries.emplace_back(id, count); }
        void erase(storage_type::const_iterator it) { _entries.erase(it); }

    private:
        storage_type _entries {};
    };

    // Grants ownership to the next eligible waiter(s) after the lock becomes free. Called under
    // `_mutex` from `Release` / `ReleaseShared`. Exclusive waiters require zero shared holders;
    // a run of consecutive shared waiters at the queue front is granted together (readers batch),
    // stopping at the first exclusive waiter so writers are not starved by a stream of readers.
    void GrantWaiters() noexcept FO_TSA_REQUIRES(_mutex);

    // True if a thread OTHER than `self` currently holds a descendant-mark on this lock — i.e. some
    // other thread is working inside this entity's subtree, so an exclusive Acquire here must wait.
    // Caller holds `_mutex`. Own marks (escalating up into a subtree you already hold) never block.
    bool HasForeignDescendantHolder(std::thread::id self) const noexcept FO_TSA_REQUIRES(_mutex);
    // True if an exclusive waiter is already parked ahead — a new Shared/DescendantHold request queues
    // behind it so a writer is not starved by a stream of readers/sibling marks. Caller holds `_mutex`.
    bool HasWaitingExclusive() const noexcept FO_TSA_REQUIRES(_mutex);

    mutable mutex _mutex {};
    std::atomic<std::thread::id> _ownerThread {};
    int32_t _recursionCount FO_TSA_GUARDED_BY(_mutex) {};
    ThreadHoldCounts _sharedHolders FO_TSA_GUARDED_BY(_mutex) {}; // Per-thread shared ("read") holders with their recursion counts.
    // Per-thread descendant-hold ("intention") counts: how many descendants of this lock's entity the
    // thread currently holds. A foreign entry blocks this lock's exclusive Acquire; the owner's own
    // entry does not. Bumped by Register*, dropped by Unregister. Never coexists with `_sharedHolders`.
    ThreadHoldCounts _descendantHolders FO_TSA_GUARDED_BY(_mutex) {};
    list<WaitEntry> _waitQueue FO_TSA_GUARDED_BY(_mutex) {};
};

class SyncContext final
{
public:
    SyncContext();
    SyncContext(const SyncContext&) = delete;
    SyncContext(SyncContext&&) noexcept = delete;
    auto operator=(const SyncContext&) = delete;
    auto operator=(SyncContext&&) noexcept = delete;
    ~SyncContext();

    [[nodiscard]] static auto GetCurrentOnThisThread() noexcept -> nptr<SyncContext>;
    [[nodiscard]] static auto GetOutermostOnThisThread() noexcept -> nptr<SyncContext>;
    static void RetainEntityPairInCurrentChain(nptr<ServerEntity> first, nptr<ServerEntity> second);

    [[nodiscard]] auto ValidateAccess(nptr<const ServerEntity> entity) const noexcept -> bool;
    [[nodiscard]] auto IsEmpty() const noexcept -> bool { return _heldLocks.empty() && _singletonLocks.empty(); }
    [[nodiscard]] auto GetHeldEntities() -> vector<ptr<ServerEntity>>;

    void Activate() noexcept;
    void Deactivate() noexcept;
    void SyncEntities(span<const nptr<ServerEntity>> entities);
    void SyncEntity(nptr<ServerEntity> entity);
    void EnsureEntitySynced(nptr<ServerEntity> entity);
    // Trusted creation/registration boundary. Unlike ordinary EnsureEntitySynced, this may capture
    // an unpublished parentless entity that has no existing caller cover; the native sync boundary
    // audit pins its exact allowed callers.
    void EnsureFreshEntitySynced(nptr<ServerEntity> entity);
    void Release() noexcept;

    // Singleton-lock acquisition (e.g. `Game.Lock()`). Acquires the supplied EntityLock and
    // records it in a SEPARATE bucket from the per-Sync-call `_heldLocks` set, so a subsequent
    // `Sync::Lock(...)` (which REPLACES `_heldLocks`) doesn't drop singletons under the caller's
    // feet. Recursion is supported per the EntityLock primitive — repeated LockSingleton calls
    // bump recursion, matched UnlockSingleton calls release. Job-exit (SyncContext destructor)
    // drains both buckets so leaked singleton locks can't outlive a worker job.
    void LockSingleton(ptr<EntityLock> lock);
    void UnlockSingleton(ptr<EntityLock> lock);

private:
    void EnsureEntitySyncedImpl(ptr<ServerEntity> entity);

    // EnsureEntitySynced's one-shot transaction over the sorted-unique op batch: every state mutex
    // is try-locked before compatibility is checked or any ownership state is committed, so failure
    // is genuinely non-blocking and mutation-free.
    static auto FO_TSA_NO_ANALYSIS TryAcquireEnsureOpsAtomically(span<const pair<ptr<EntityLock>, bool>> ops) -> bool;

    // Acquire the exclusive cover (`locks`/`owners`) AND register the hierarchical intention marks
    // (`holds`/`hold_owners` — the cover owners' separate-lock ancestors) in ONE deadlock-free
    // ascending-address pass: exclusive locks via Acquire/TryAcquire, intention marks via
    // RegisterDescendantHold/TryRegisterDescendantHold. A single total order over the union of both
    // makes the mixed acquire cycle-free. Replaces the whole held set (cover + intentions).
    void AcquireLocks(vector<ptr<EntityLock>>& locks, vector<refcount_ptr<ServerEntity>>&& owners, vector<ptr<EntityLock>>& holds, vector<refcount_ptr<ServerEntity>>&& hold_owners);
    // Deadlock-breaking escalation used by AcquireLocks when the non-parking back-off cannot make
    // progress (typically a nested context cross-holding locks against another thread's parent
    // cover). Releases the full thread-held union (this context's prefix is already released by the
    // caller, plus every ancestor SyncContext's per-Sync exclusive locks, intention marks, and
    // singleton locks) and re-acquires it in strict ascending-address order via the FIFO-fair blocking
    // `EntityLock::Acquire` (exclusive) and `RegisterDescendantHold` (intention), restoring each lock's
    // exclusive-recursion and descendant-hold counts exactly. Total resource ordering over the mixed
    // union makes this fair acquire deadlock-free regardless of what the thread (or its ancestors) held.
    // If the blocking re-acquire is aborted by shutdown, it rolls the whole union back to zero and
    // clears the ancestor bookkeeping so the unwinding contexts don't release locks they no longer hold.
    void AcquireLocksOrderedFair(const vector<ptr<EntityLock>>& locks, const vector<ptr<EntityLock>>& holds);
    void ReleaseLocks() noexcept;
    void ReleaseSingletonLocks() noexcept;

    vector<ptr<EntityLock>> _heldLocks {};
    // Parallel to `_heldLocks`. Each entry pins the ServerEntity whose `_ownedLock` (or whose
    // SetParent-chain reaches it) backs the corresponding lock borrow. Without this, an entity
    // destroyed by the same thread that holds its lock (e.g. `Game.DestroyCritter(cr)` from inside
    // a `Sync::Lock(cr)` block) leaves a dangling lock borrow here; the next SyncEntities call
    // would `ReleaseLocks` and crash. Keeping the owner alive via refcount until `ReleaseLocks`
    // runs guarantees the EntityLock storage outlives the held-lock list. Entries are never null:
    // the engine singleton lock lives in the separate `_singletonLocks` bucket, not here.
    vector<refcount_ptr<ServerEntity>> _heldLockOwners {};
    // Hierarchical intention marks: the SEPARATE-lock ancestors of every cover owner (a critter's map
    // and location, an item's map/location, …). Registered as descendant-holds so no other thread can
    // take any of those ancestors exclusively while we hold their descendant; compatible among siblings
    // marking the same ancestor. The parallel owner vector pins the ancestor entities so their
    // `_ownedLock` storage outlives the hold. Reset together with `_heldLocks` (derived from the cover).
    vector<ptr<EntityLock>> _heldDescendantHolds {};
    vector<refcount_ptr<ServerEntity>> _heldDescendantHoldOwners {};
    // Singleton locks acquired via LockSingleton; survive SyncEntities replacement. Each entry
    // is one outstanding LockSingleton call (so recursion tracking matches paired Unlock calls).
    vector<ptr<EntityLock>> _singletonLocks {};
    // Saved on Activate, restored on Deactivate. Lets contexts stack per-thread so that an
    // event-callback's nested context can pop back to the dispatcher's primary context cleanly.
    nptr<SyncContext> _previousContext {};
};

class ScopedSyncContext final
{
public:
    ScopedSyncContext() noexcept { _ctx.Activate(); }
    ~ScopedSyncContext() noexcept
    {
        _ctx.Release();
        _ctx.Deactivate();
    }

    ScopedSyncContext(const ScopedSyncContext&) = delete;
    ScopedSyncContext(ScopedSyncContext&&) = delete;
    auto operator=(const ScopedSyncContext&) -> ScopedSyncContext& = delete;
    auto operator=(ScopedSyncContext&&) -> ScopedSyncContext& = delete;

    [[nodiscard]] auto GetContext() noexcept -> SyncContext& { return _ctx; }

    void Sync(nptr<ServerEntity> entity) { _ctx.SyncEntity(entity); }

private:
    SyncContext _ctx {};
};

// Null-tolerant own-lock retention for an entity already covered by the active script/job context.
// Makes one non-blocking attempt and never releases/reacquires, blocks, waits, retries, yields, sleeps,
// or obtains missing caller cover; throws when there is no active context, the entity is uncovered,
// or the one try is contended.
void EnsureEntitySynced(nptr<ServerEntity> entity);

FO_END_NAMESPACE
