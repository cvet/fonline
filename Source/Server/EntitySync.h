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
[[nodiscard]] auto IsEntityAccessValid(const ServerEntity* entity) noexcept -> bool;

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

    // Marks this lock as shutting down and force-aborts every parked waiter. Each `WaitEntry`'s
    // state atomic CAS-flips from "waiting" to "aborted", then `notify_one` wakes its single
    // blocked thread (FIFO precision preserved — no thundering herd). The flag also rejects any
    // future `Acquire` so a tick that fires after shutdown started can't deadlock on the empty
    // owner field. `Server::Shutdown` walks every entity once and calls this on each lock.
    void AbortPendingWaiters() noexcept;

private:
    struct WaitEntry
    {
        // State machine:
        //   0 = parked on `wait(0)`, no decision yet
        //   1 = lock granted by Release; on wake the waiter removes itself and returns
        //   2 = aborted by AbortPendingWaiters; on wake the waiter removes itself and throws
        // Atomicity carries the cross-thread hand-off; CAS in Release / AbortPendingWaiters
        // disambiguates the abort-races-grant window.
        static constexpr int STATE_WAITING = 0;
        static constexpr int STATE_GRANTED = 1;
        static constexpr int STATE_ABORTED = 2;

        uint64_t Ticket {};
        std::thread::id Waiter {};
        bool Shared {};
        std::atomic<int> State {STATE_WAITING};
    };

    // Grants ownership to the next eligible waiter(s) after the lock becomes free. Called under
    // `_mutex` from `Release` / `ReleaseShared`. Exclusive waiters require zero shared holders;
    // a run of consecutive shared waiters at the queue front is granted together (readers batch),
    // stopping at the first exclusive waiter so writers are not starved by a stream of readers.
    void GrantWaiters() noexcept;

    mutable std::mutex _mutex {};
    std::atomic<std::thread::id> _ownerThread {};
    int32_t _recursionCount {};
    unordered_map<std::thread::id, int32_t> _sharedHolders {}; // Per-thread shared ("read") holders with their recursion counts.
    list<WaitEntry> _waitQueue {};
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

    [[nodiscard]] static auto GetCurrentOnThisThread() noexcept -> SyncContext*;
    [[nodiscard]] static auto GetOutermostOnThisThread() noexcept -> SyncContext*;

    [[nodiscard]] auto ValidateAccess(const ServerEntity* entity) const noexcept -> bool;
    [[nodiscard]] auto IsEmpty() const noexcept -> bool { return _heldLocks.empty() && _singletonLocks.empty(); }
    [[nodiscard]] auto GetHeldEntities() const -> vector<ServerEntity*>;

    void Activate() noexcept;
    void Deactivate() noexcept;
    void SyncEntities(span<ServerEntity*> entities);
    void SyncEntity(ServerEntity* entity);
    void EnsureEntitySynced(ServerEntity* entity);
    void Release() noexcept;

    // Singleton-lock acquisition (e.g. `Game.Lock()`). Acquires the supplied EntityLock and
    // records it in a SEPARATE bucket from the per-Sync-call `_heldLocks` set, so a subsequent
    // `Sync::Lock(...)` (which REPLACES `_heldLocks`) doesn't drop singletons under the caller's
    // feet. Recursion is supported per the EntityLock primitive — repeated LockSingleton calls
    // bump recursion, matched UnlockSingleton calls release. Job-exit (SyncContext destructor)
    // drains both buckets so leaked singleton locks can't outlive a worker job.
    void LockSingleton(EntityLock* lock);
    void UnlockSingleton(EntityLock* lock);

private:
    void AcquireLocks(vector<EntityLock*>& locks, vector<refcount_ptr<ServerEntity>>&& owners);
    void ReleaseLocks() noexcept;
    void ReleaseSingletonLocks() noexcept;

    vector<EntityLock*> _heldLocks {};
    // Parallel to `_heldLocks`. Each entry pins the ServerEntity whose `_ownedLock` (or whose
    // SetParent-chain reaches it) backs the corresponding `EntityLock*`. Without this, an entity
    // destroyed by the same thread that holds its lock (e.g. `Game.DestroyCritter(cr)` from inside
    // a `Sync::Lock(cr)` block) leaves a dangling `EntityLock*` here; the next SyncEntities call
    // would `ReleaseLocks` and crash. Keeping the owner alive via refcount until `ReleaseLocks`
    // runs guarantees the EntityLock storage outlives the held-lock list. May be null for the
    // engine singleton lock (the engine is not a ServerEntity).
    vector<refcount_ptr<ServerEntity>> _heldLockOwners {};
    // Singleton locks acquired via LockSingleton; survive SyncEntities replacement. Each entry
    // is one outstanding LockSingleton call (so recursion tracking matches paired Unlock calls).
    vector<EntityLock*> _singletonLocks {};
    // Saved on Activate, restored on Deactivate. Lets contexts stack per-thread so that an
    // event-callback's nested context can pop back to the dispatcher's primary context cleanly.
    SyncContext* _previousContext {nullptr};
};

void PropagateEntityLock(class Item* item, EntityLock* parent_lock);
void RevertEntityLock(class Item* item);

FO_END_NAMESPACE
