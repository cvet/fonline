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

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(EntitySyncException);
FO_DECLARE_EXCEPTION_EXT(EntityLockWaitAbortedException, EntitySyncException);

class ServerEngine;
class ServerEntity;
class EntityManager;

[[nodiscard]] auto NextSyncTicket() noexcept -> uint64_t;

// The read-path coverage check — reparenting is stricter and needs the entity's own lock. `diagnose` dumps the
// parent chain before the caller throws, which the non-throwing Game.IsEntityLocked probe turns off
[[nodiscard]] auto IsEntityAccessValid(nptr<const ServerEntity> entity, bool diagnose = true) noexcept -> bool;
// True when the owning engine runs its logic on a single worker, which is when the cover model is bypassed
[[nodiscard]] auto IsSingleThreadedLogic(nptr<const ServerEntity> entity) noexcept -> bool;

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

    // Escalation releases a parent-held lock to zero and re-takes it this many times, restoring the parent
    // context's bookkeeping exactly
    auto GetExclusiveRecursionForCurrentThread() const noexcept -> int32_t;

    // Exclusive ("write") acquisition. One owner thread at a time; re-entrant on that thread via
    // `_recursionCount`. Blocks until no shared holders and no other exclusive owner remain
    void Acquire(uint64_t ticket);
    auto TryAcquire() -> bool;
    void Release() noexcept;

    // Used for Game-singleton property reads so concurrent readers do not serialize on the engine-global lock;
    // an exclusive owner subsumes it into its own recursion
    void AcquireShared(uint64_t ticket);
    void ReleaseShared() noexcept;

    // Bookkeeping, not access: nobody reads through the mark, it only tells an ancestor's exclusive Acquire that
    // a descendant is busy
    void RegisterDescendantHold(uint64_t ticket);
    auto TryRegisterDescendantHold() -> bool;
    void UnregisterDescendantHold() noexcept;

    // Mirrors GetExclusiveRecursionForCurrentThread so escalation can restore an ancestor's intention count
    // exactly
    auto GetDescendantHoldCountForCurrentThread() const noexcept -> int32_t;

    // Force-aborts every parked waiter and rejects later acquisitions, so a tick firing after shutdown started
    // cannot deadlock on the empty owner field
    void AbortPendingWaiters() noexcept;

    // Ensure is split into a read-only compatibility check and a no-throw commit so SyncContext can make a
    // multi-lock batch all-or-nothing; never hold the state mutex across a blocking operation
    void LockStateMutex() FO_TSA_ACQUIRE(_mutex);
    [[nodiscard]] bool TryLockStateMutex() FO_TSA_TRY_ACQUIRE(true, _mutex);
    void UnlockStateMutex() noexcept FO_TSA_RELEASE(_mutex);
    [[nodiscard]] bool IsEnsureOpCompatible(bool is_exclusive) const noexcept FO_TSA_REQUIRES(_mutex);
    void CommitEnsureOp(bool is_exclusive) noexcept FO_TSA_REQUIRES(_mutex);

private:
    // Shared and DescendantHold never coexist on one lock, because a Game singleton is in no entity's parent
    // chain, so their grant logic stays disjoint
    enum class WaitKind : uint8_t
    {
        Exclusive,
        Shared,
        DescendantHold,
    };

    struct WaitEntry
    {
        // Atomic because it carries the cross-thread hand-off; the CAS in Release / AbortPendingWaiters is what
        // disambiguates the abort-races-grant window
        static constexpr int32_t STATE_WAITING = 0;
        static constexpr int32_t STATE_GRANTED = 1;
        static constexpr int32_t STATE_ABORTED = 2;

        uint64_t Ticket {};
        std::thread::id Waiter {};
        WaitKind Kind {WaitKind::Exclusive};
        std::atomic<int32_t> State {STATE_WAITING};
    };

    // A linear inline vector, not a hash map: holders per lock stay in the low single digits and an idle lock
    // must allocate nothing across millions of entities. Emplace assumes the key is absent
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

    // Called under `_mutex`. Consecutive shared waiters are granted together but the run stops at the first
    // exclusive one, so readers batch without starving writers
    void GrantWaiters() noexcept FO_TSA_REQUIRES(_mutex);

    // A foreign mark means another thread is working inside this subtree, so an exclusive Acquire must wait;
    // own marks never block. Caller holds `_mutex`
    bool HasForeignDescendantHolder(std::thread::id self) const noexcept FO_TSA_REQUIRES(_mutex);
    // True if an exclusive waiter is already parked ahead — a new Shared/DescendantHold request queues
    // behind it so a writer is not starved by a stream of readers/sibling marks. Caller holds `_mutex`
    bool HasWaitingExclusive() const noexcept FO_TSA_REQUIRES(_mutex);

    mutable mutex _mutex {};
    std::atomic<std::thread::id> _ownerThread {};
    int32_t _recursionCount FO_TSA_GUARDED_BY(_mutex) {};
    ThreadHoldCounts _sharedHolders FO_TSA_GUARDED_BY(_mutex) {}; // Per-thread shared ("read") holders with their recursion counts
    // How many descendants of this entity each thread holds; a foreign entry blocks this lock's exclusive
    // Acquire while the owner's own entry does not
    ThreadHoldCounts _descendantHolders FO_TSA_GUARDED_BY(_mutex) {};
    list<WaitEntry> _waitQueue FO_TSA_GUARDED_BY(_mutex) {};
};

// Inline storage keeps the hottest server path off the heap, with capacity sized to the measured cover; the
// parallel owner lists stay `vector` because their element needs a complete ServerEntity this header hides
using SyncLockList = small_vector<ptr<EntityLock>, 4>;

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
    [[nodiscard]] auto GetLockWaitDuration() const noexcept -> timespan { return _lockWaitDuration; }

    void Activate() noexcept;
    void Deactivate() noexcept;
    void SyncEntities(const_span<ptr<ServerEntity>> entities);
    void SyncEntity(nptr<ServerEntity> entity);
    void EnsureEntitySynced(nptr<ServerEntity> entity);
    void Release() noexcept;

    // Recorded in a separate bucket from `_heldLocks`, which a later `Sync::Lock(...)` replaces wholesale and
    // would otherwise drop singletons under the caller's feet; job exit drains both
    void LockSingleton(ptr<EntityLock> lock);
    void UnlockSingleton(ptr<EntityLock> lock);

private:
    friend class EntityLock;
    friend class EntityManager;
    friend class ServerEngine;

    // A parked wait belongs to every active script context in the synchronous call chain: the outer
    // context's wall time includes waits performed by a nested script callback too
    static void RecordLockWait(timespan duration) noexcept;

    // The trusted creation boundary, and the only path that may capture an unpublished parentless entity with
    // no caller cover, so its C++ surface stays restricted to the two audited owners
    void EnsureFreshEntitySynced(nptr<ServerEntity> entity);
    // One all-or-nothing state-mutex transaction, retried until it lands; TSA cannot follow the
    // try-lock/roll-back batch, hence FO_TSA_NO_ANALYSIS
    void FO_TSA_NO_ANALYSIS EnsureEntitySyncedImpl(ptr<ServerEntity> entity);

    // Cover and intention marks are taken in one ascending-address pass, because a single total order over
    // their union is what makes the mixed acquire cycle-free; replaces the whole held set
    void AcquireLocks(SyncLockList& locks, vector<refcount_ptr<ServerEntity>>&& owners, SyncLockList& holds, vector<refcount_ptr<ServerEntity>>&& hold_owners);
    // The deadlock breaker when back-off cannot progress: the whole thread-held union is released and re-taken
    // in address order with counts restored exactly, and a shutdown abort rolls it back to zero
    void AcquireLocksOrderedFair(const_span<ptr<EntityLock>> locks, const_span<ptr<EntityLock>> holds);
    void ReleaseLocks() noexcept;
    void ReleaseSingletonLocks() noexcept;

    SyncLockList _heldLocks {};
    // Pins the entity backing each held lock, because a thread destroying an entity it holds would otherwise
    // leave a dangling borrow that the next ReleaseLocks dereferences. Entries are never null
    vector<refcount_ptr<ServerEntity>> _heldLockOwners {};
    // The separate-lock ancestors of every cover owner, marked so no other thread takes them exclusively while
    // we hold a descendant; reset together with `_heldLocks`, which they are derived from
    SyncLockList _heldDescendantHolds {};
    vector<refcount_ptr<ServerEntity>> _heldDescendantHoldOwners {};
    // Singleton locks acquired via LockSingleton; survive SyncEntities replacement. Each entry
    // is one outstanding LockSingleton call (so recursion tracking matches paired Unlock calls)
    SyncLockList _singletonLocks {};
    // Saved on Activate, restored on Deactivate. Lets contexts stack per-thread so that an
    // event-callback's nested context can pop back to the dispatcher's primary context cleanly
    nptr<SyncContext> _previousContext {};
    timespan _lockWaitDuration {};
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

// Retains the own lock of an already-covered entity without ever releasing the caller's cover or parking on a
// lock; the bounded retry absorbs in-flight acquire/release windows
void EnsureEntitySynced(nptr<ServerEntity> entity);

FO_END_NAMESPACE
