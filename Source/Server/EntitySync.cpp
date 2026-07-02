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

#include "EntitySync.h"
#include "Critter.h"
#include "Item.h"
#include "Location.h"
#include "Map.h"
#include "Server.h"
#include "ServerEntity.h"
#include "WorkerPool.h"

FO_BEGIN_NAMESPACE

static constexpr int32_t MAX_SYNC_RETRIES = 128;

static thread_local SyncContext* CurrentContext {};
static std::atomic<uint64_t> TicketCounter {};

EntityLock::EntityLock()
{
    FO_STACK_TRACE_ENTRY();
}

EntityLock::~EntityLock()
{
    FO_STACK_TRACE_ENTRY();
}

void EntityLock::Acquire(uint64_t ticket)
{
    FO_STACK_TRACE_ENTRY();

    const auto this_thread = std::this_thread::get_id();

    if (_ownerThread.load(std::memory_order_acquire) == this_thread) {
        scoped_lock locker {_mutex};

        _recursionCount++;
        return;
    }

    unique_lock locker {_mutex};

    // A thread that already holds this lock shared must not request it exclusively — that is a
    // read->write upgrade, which would self-deadlock (the exclusive grant below waits for all
    // shared holders to drain, including this very thread). The property paths never do this (a
    // getter releases its shared hold before any setter runs); assert so a future caller that
    // breaks the rule is caught immediately instead of hanging.
    FO_VERIFY_AND_THROW(!_sharedHolders.contains(this_thread), "Entity lock cannot be upgraded from shared to exclusive by the same thread", std::hash<std::thread::id> {}(this_thread), _sharedHolders.size());

    // Grant immediately only when the lock is fully free: no exclusive owner, no shared readers, and
    // no FOREIGN descendant-mark (another thread working inside this entity's subtree). Our own
    // descendant-marks never block — escalating up into a subtree we already hold is allowed.
    if (_ownerThread.load(std::memory_order_relaxed) == std::thread::id {} && _sharedHolders.empty() && !HasForeignDescendantHolder(this_thread)) {
        _ownerThread.store(this_thread, std::memory_order_release);
        _recursionCount = 1;
        TSanAcquire(this);
        return;
    }

    // Sorted insertion by ticket. `list` keeps the node address stable across other
    // insert/erase operations so the in-progress `wait(0)` below points at the right atomic.
    const auto insert_pos = std::ranges::find_if(_waitQueue, [ticket](const auto& e) { return e.Ticket > ticket; });
    const auto entry_it = _waitQueue.emplace(insert_pos);
    entry_it->Ticket = ticket;
    entry_it->Waiter = this_thread;
    entry_it->Kind = WaitKind::Exclusive;

    locker.unlock();

    // `atomic::wait` is C++20 — it parks on a futex-equivalent and is woken by `notify_one` from
    // either `Release` / `ReleaseShared` (state -> GRANTED) or `AbortPendingWaiters` (state ->
    // ABORTED). Loop on spurious wake-ups: the wake notification is best-effort and the standard
    // allows the wait to return before the value changes.
    int32_t state = WaitEntry::STATE_WAITING;

    while (state == WaitEntry::STATE_WAITING) {
        entry_it->State.wait(WaitEntry::STATE_WAITING, std::memory_order_acquire);
        state = entry_it->State.load(std::memory_order_acquire);
    }

    locker.lock();
    _waitQueue.erase(entry_it);
    locker.unlock();

    if (state == WaitEntry::STATE_ABORTED) {
        throw EntityLockWaitAbortedException("EntityLock::Acquire aborted: shutdown in progress");
    }

    FO_STRONG_ASSERT(state == WaitEntry::STATE_GRANTED, "Exclusive entity lock waiter woke up in a non-granted state", ticket, state);
    const auto owner_thread = _ownerThread.load(std::memory_order_acquire);
    FO_STRONG_ASSERT(owner_thread == this_thread, "Exclusive entity lock was granted but the current thread was not recorded as owner", ticket, std::hash<std::thread::id> {}(owner_thread), std::hash<std::thread::id> {}(this_thread));
    TSanAcquire(this);
}

void EntityLock::AcquireShared(uint64_t ticket)
{
    FO_STACK_TRACE_ENTRY();

    const auto this_thread = std::this_thread::get_id();

    // The exclusive owner trivially has read access: fold the shared request into the exclusive
    // recursion (a matched ReleaseShared unwinds it). This is what lets `Game.Lock()` (exclusive)
    // freely read Game properties from the same thread without self-deadlocking.
    if (_ownerThread.load(std::memory_order_acquire) == this_thread) {
        scoped_lock locker {_mutex};

        _recursionCount++;
        return;
    }

    unique_lock locker {_mutex};

    // Already a shared holder on this thread: bump recursion (nested reads).
    if (const auto it = _sharedHolders.find(this_thread); it != _sharedHolders.end()) {
        it->second++;
        return;
    }

    // Grant immediately when no exclusive owner holds AND no exclusive waiter is queued ahead. The
    // second condition keeps a waiting writer from being starved by a steady stream of new readers:
    // new readers queue behind the writer and are released as a batch once it finishes.
    if (_ownerThread.load(std::memory_order_relaxed) == std::thread::id {} && !HasWaitingExclusive()) {
        _sharedHolders.emplace(this_thread, 1);
        TSanAcquire(this);
        return;
    }

    const auto insert_pos = std::ranges::find_if(_waitQueue, [ticket](const auto& e) { return e.Ticket > ticket; });
    const auto entry_it = _waitQueue.emplace(insert_pos);
    entry_it->Ticket = ticket;
    entry_it->Waiter = this_thread;
    entry_it->Kind = WaitKind::Shared;

    locker.unlock();

    int32_t state = WaitEntry::STATE_WAITING;

    while (state == WaitEntry::STATE_WAITING) {
        entry_it->State.wait(WaitEntry::STATE_WAITING, std::memory_order_acquire);
        state = entry_it->State.load(std::memory_order_acquire);
    }

    locker.lock();
    _waitQueue.erase(entry_it);
    locker.unlock();

    if (state == WaitEntry::STATE_ABORTED) {
        throw EntityLockWaitAbortedException("EntityLock::AcquireShared aborted: shutdown in progress");
    }

    FO_VERIFY_AND_THROW(state == WaitEntry::STATE_GRANTED, "Shared entity lock waiter woke up in a non-granted state", ticket, state);
    // GrantWaiters recorded this thread in `_sharedHolders` before waking it.
    TSanAcquire(this);
}

void EntityLock::AbortPendingWaiters() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    scoped_lock locker {_mutex};

    for (auto& entry : _waitQueue) {
        int32_t expected = WaitEntry::STATE_WAITING;

        if (entry.State.compare_exchange_strong(expected, WaitEntry::STATE_ABORTED, std::memory_order_acq_rel)) {
            entry.State.notify_one();
        }

        // expected != STATE_WAITING means Release already CAS-flipped to STATE_GRANTED in a race —
        // the waiter is going to acquire the lock normally. We leave the grant in place; nothing
        // is lost (the holder will Release as usual and the next abort cascade catches any newer
        // waiters that show up in the meantime).
    }
}

void EntityLock::Release() noexcept
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(_ownerThread.load(std::memory_order_relaxed) == std::this_thread::get_id(), "Entity lock release called from non-owner thread");

    scoped_lock locker {_mutex};

    _recursionCount--;

    if (_recursionCount > 0) {
        return;
    }

    TSanRelease(this);
    _ownerThread.store(std::thread::id {}, std::memory_order_release);
    GrantWaiters();
}

void EntityLock::ReleaseShared() noexcept
{
    FO_STACK_TRACE_ENTRY();

    const auto this_thread = std::this_thread::get_id();

    // Shared acquired by the exclusive owner was folded into the exclusive recursion — unwind it
    // through the exclusive Release path (which also re-grants once recursion hits zero).
    if (_ownerThread.load(std::memory_order_acquire) == this_thread) {
        Release();
        return;
    }

    scoped_lock locker {_mutex};

    const auto it = _sharedHolders.find(this_thread);
    FO_STRONG_ASSERT(it != _sharedHolders.end(), "Shared entity lock release without holder entry", _sharedHolders.size());

    if (--it->second == 0) {
        TSanRelease(this);
        _sharedHolders.erase(it);

        // A queued exclusive waiter can only proceed once the last reader has left.
        if (_sharedHolders.empty()) {
            GrantWaiters();
        }
    }
}

void EntityLock::RegisterDescendantHold(uint64_t ticket)
{
    FO_STACK_TRACE_ENTRY();

    const auto this_thread = std::this_thread::get_id();

    unique_lock locker {_mutex};

    // Re-entrant fast path: if this thread already owns the lock exclusively or already marks it, it
    // is already inside the subtree — no foreign exclusive owner can exist — so just bump the count.
    // (Locking a second descendant of the same ancestor, or escalating up into a subtree we hold.)
    if (_ownerThread.load(std::memory_order_acquire) == this_thread || _descendantHolders.contains(this_thread)) {
        _descendantHolders[this_thread]++;
        return;
    }

    // Register immediately only when no FOREIGN thread owns the lock exclusively AND no exclusive
    // writer is already queued ahead — yield to a parked writer so a map/location-exclusive op is not
    // starved by an endless stream of sibling marks. Shared holders are compatible (and a Game
    // singleton, the only shared user, is in no entity's parent chain, so they never coexist here).
    if (_ownerThread.load(std::memory_order_relaxed) == std::thread::id {} && !HasWaitingExclusive()) {
        _descendantHolders.emplace(this_thread, 1);
        return;
    }

    const auto insert_pos = std::ranges::find_if(_waitQueue, [ticket](const auto& e) { return e.Ticket > ticket; });
    const auto entry_it = _waitQueue.emplace(insert_pos);
    entry_it->Ticket = ticket;
    entry_it->Waiter = this_thread;
    entry_it->Kind = WaitKind::DescendantHold;

    locker.unlock();

    int32_t state = WaitEntry::STATE_WAITING;

    while (state == WaitEntry::STATE_WAITING) {
        entry_it->State.wait(WaitEntry::STATE_WAITING, std::memory_order_acquire);
        state = entry_it->State.load(std::memory_order_acquire);
    }

    locker.lock();
    _waitQueue.erase(entry_it);
    locker.unlock();

    if (state == WaitEntry::STATE_ABORTED) {
        throw EntityLockWaitAbortedException("EntityLock::RegisterDescendantHold aborted: shutdown in progress");
    }

    FO_VERIFY_AND_THROW(state == WaitEntry::STATE_GRANTED, "Descendant-hold waiter woke up in a non-granted state", ticket, state);
    // GrantWaiters recorded this thread in `_descendantHolders` before waking it.
}

auto EntityLock::TryRegisterDescendantHold() -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto this_thread = std::this_thread::get_id();

    scoped_lock locker {_mutex};

    if (_ownerThread.load(std::memory_order_acquire) == this_thread || _descendantHolders.contains(this_thread)) {
        _descendantHolders[this_thread]++;
        return true;
    }

    if (_ownerThread.load(std::memory_order_relaxed) != std::thread::id {} || HasWaitingExclusive()) {
        return false;
    }

    _descendantHolders.emplace(this_thread, 1);
    return true;
}

void EntityLock::UnregisterDescendantHold() noexcept
{
    FO_STACK_TRACE_ENTRY();

    const auto this_thread = std::this_thread::get_id();

    scoped_lock locker {_mutex};

    const auto it = _descendantHolders.find(this_thread);
    FO_STRONG_ASSERT(it != _descendantHolders.end(), "Descendant-hold release without holder entry", _descendantHolders.size());

    if (--it->second == 0) {
        _descendantHolders.erase(it);

        // Dropping the last foreign mark may unblock a queued exclusive writer (GrantWaiters re-checks
        // both readers and remaining foreign marks, and no-ops while we still own the lock exclusively).
        GrantWaiters();
    }
}

auto EntityLock::GetDescendantHoldCountForCurrentThread() const noexcept -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    scoped_lock locker {_mutex};

    const auto it = _descendantHolders.find(std::this_thread::get_id());
    return it != _descendantHolders.end() ? it->second : 0;
}

auto EntityLock::HasForeignDescendantHolder(std::thread::id self) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    for (const auto& [tid, count] : _descendantHolders) {
        if (tid != self && count > 0) {
            return true;
        }
    }

    return false;
}

auto EntityLock::HasWaitingExclusive() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return std::ranges::any_of(_waitQueue, [](const WaitEntry& e) { return e.Kind == WaitKind::Exclusive && e.State.load(std::memory_order_acquire) == WaitEntry::STATE_WAITING; });
}

void EntityLock::GrantWaiters() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    // Caller holds `_mutex`. Nothing can be granted while an exclusive owner still holds the lock.
    if (_ownerThread.load(std::memory_order_relaxed) != std::thread::id {}) {
        return;
    }

    // First still-WAITING entry (FIFO by ticket). Entries already flipped to GRANTED/ABORTED by a
    // prior grant or an interleaved AbortPendingWaiters linger until their waiter wakes and erases
    // them, so skip those.
    auto it = std::ranges::find_if(_waitQueue, [](const WaitEntry& e) { return e.State.load(std::memory_order_acquire) == WaitEntry::STATE_WAITING; });

    if (it == _waitQueue.end()) {
        return;
    }

    if (it->Kind == WaitKind::Exclusive) {
        // Exclusive waiter: grant only once every reader AND every foreign descendant-mark has drained
        // (another thread working inside this entity's subtree must finish before we take the ancestor).
        if (!_sharedHolders.empty() || HasForeignDescendantHolder(it->Waiter)) {
            return;
        }

        int32_t expected = WaitEntry::STATE_WAITING;

        if (it->State.compare_exchange_strong(expected, WaitEntry::STATE_GRANTED, std::memory_order_acq_rel)) {
            _ownerThread.store(it->Waiter, std::memory_order_release);
            _recursionCount = 1;
            it->State.notify_one();
        }

        return;
    }

    // Shared / DescendantHold waiter: grant it plus every consecutive waiter of the SAME kind, stopping
    // at the first exclusive waiter (so writers are not starved). Each granted waiter is recorded in its
    // holder map before being woken so a concurrent exclusive Acquire observes it as a live holder.
    // Shared and DescendantHold never interleave on one lock, so the homogeneous run is the whole batch.
    const WaitKind kind = it->Kind;

    for (; it != _waitQueue.end(); ++it) {
        if (it->State.load(std::memory_order_acquire) != WaitEntry::STATE_WAITING) {
            continue;
        }
        if (it->Kind != kind) {
            break;
        }

        int32_t expected = WaitEntry::STATE_WAITING;

        if (it->State.compare_exchange_strong(expected, WaitEntry::STATE_GRANTED, std::memory_order_acq_rel)) {
            if (kind == WaitKind::Shared) {
                _sharedHolders.emplace(it->Waiter, 1);
            }
            else {
                _descendantHolders.emplace(it->Waiter, 1);
            }

            it->State.notify_one();
        }
    }
}

auto EntityLock::TryAcquire() -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto this_thread = std::this_thread::get_id();

    if (_ownerThread.load(std::memory_order_acquire) == this_thread) {
        scoped_lock locker {_mutex};

        _recursionCount++;
        return true;
    }

    unique_lock locker {_mutex};

    if (_ownerThread.load(std::memory_order_relaxed) != std::thread::id {} || !_sharedHolders.empty() || HasForeignDescendantHolder(this_thread)) {
        return false;
    }

    _ownerThread.store(this_thread, std::memory_order_release);
    _recursionCount = 1;
    TSanAcquire(this);
    return true;
}

auto EntityLock::IsLockedByCurrentThread() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _ownerThread.load(std::memory_order_acquire) == std::this_thread::get_id();
}

auto EntityLock::WaiterCount() const noexcept -> size_t
{
    FO_NO_STACK_TRACE_ENTRY();

    scoped_lock locker {_mutex};

    return _waitQueue.size();
}

auto EntityLock::GetExclusiveRecursionForCurrentThread() const noexcept -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_ownerThread.load(std::memory_order_acquire) != std::this_thread::get_id()) {
        return 0;
    }

    scoped_lock locker {_mutex};

    // Re-check under the lock: a concurrent Release on this thread cannot happen (we are the owner
    // and single-threaded for our own lock state), so the owner check + recursion read are stable.
    return _ownerThread.load(std::memory_order_relaxed) == std::this_thread::get_id() ? _recursionCount : 0;
}

// Logs the access-without-sync diagnostic for a genuinely uncovered entity: its parent chain and its
// widen-coupled chain with each node's lock state, so the violation (the caller will throw) is debuggable.
// Pure logging — the coverage decision is already made by IsEntityAccessValid (this runs only when it is
// uncovered).
static void LogUncoveredEntity(nptr<const ServerEntity> entity) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto try_hold_entity = [](nptr<const ServerEntity> e) -> refcount_nptr<const ServerEntity> { return e.try_hold_ref(); };

    const auto lock_state = [](nptr<const EntityLock> lock) -> string_view {
        if (lock == nullptr) {
            return "none";
        }
        if (lock->IsLockedByCurrentThread()) {
            return "HELD-by-this-thread";
        }
        return "not-held";
    };

    WriteLog("SyncDiag access-without-sync: entity '{}' id={} destroyed={}", entity != nullptr ? entity->GetName() : string_view {}, entity != nullptr ? entity->GetId() : ident_t {}, entity != nullptr && entity->IsDestroyed());

    for (auto walk = try_hold_entity(entity); walk; walk = walk->GetParentRaw()) {
        WriteLog("SyncDiag   chain: '{}' id={} lock={}", walk->GetName(), walk->GetId(), lock_state(walk->GetEntityLock()));
    }

    auto widen = entity != nullptr ? entity->GetSyncWidenEntity() : nullptr;

    for (auto walk = try_hold_entity(widen); walk; walk = walk->GetParentRaw()) {
        WriteLog("SyncDiag   widen: '{}' id={} lock={}", walk->GetName(), walk->GetId(), lock_state(walk->GetEntityLock()));
    }
}

// Answers "is `entity` covered by a lock the current thread already holds?" in a single coverage pass.
// On an uncovered access-path check, diagnose=true dumps the parent/widen chain (LogUncoveredEntity)
// before returning false so a genuine violation is debuggable. Game.IsEntityLocked passes diagnose=false
// because false is the expected answer for a deliberate foreign-entity probe.
//
// One pass is authoritative because the reparent contract holds the entity's own lock during any reparent
// (MapManager::Transfer self-syncs the moving critter via EnsureEntitySynced), so while a thread holds a
// lock covering the entity no concurrent reparent can move it out of that cover — the cover cannot flap
// mid-walk. The former retry/verify loop (and the brief containment-sequence seqlock) existed only to
// tolerate lock-free reparenting, which is now out of contract. The per-step coverage rule: a
// held-by-this-thread or lock-free ancestor on the entity's own chain, or on its widen-coupled entity's
// chain (player<->critter symmetric auto-widen).
auto IsEntityAccessValid(nptr<const ServerEntity> entity, bool diagnose) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (entity == nullptr) {
        return true;
    }

    const auto* current_ctx = SyncContext::GetCurrentOnThisThread();
    FO_STRONG_ASSERT(current_ctx, "Entity access validation needs active sync context");

    if (current_ctx->IsEmpty()) {
        return true;
    }

    const auto try_hold_entity = [](nptr<const ServerEntity> e) -> refcount_nptr<const ServerEntity> { return e.try_hold_ref(); };

    const auto chain_covers = [&try_hold_entity](nptr<const ServerEntity> start) -> bool {
        for (auto current = try_hold_entity(start); current; current = current->GetParentRaw()) {
            const auto lock = current->GetEntityLock();

            if (lock == nullptr || lock->IsLockedByCurrentThread()) {
                return true;
            }
        }

        return false;
    };

    if (chain_covers(entity) || chain_covers(entity->GetSyncWidenEntity())) {
        return true;
    }

    if (diagnose) {
        LogUncoveredEntity(entity);
    }

    return false;
}

SyncContext::SyncContext()
{
    FO_STACK_TRACE_ENTRY();
}

SyncContext::~SyncContext()
{
    FO_STACK_TRACE_ENTRY();

    // Contract: holders MUST drain locks via explicit `Release()` before destruction.
    // Production tear-down paths (`WrapJobWithSync`, `FireEvent` per-callback nested,
    // `ServerEngine::Unlock`) wrap `Release()` in `safe_call` and run it before the
    // SyncContext goes out of scope. Tests construct SyncContexts that never acquire
    // locks. A non-empty bucket here means a code path created a SyncContext that
    // acquired locks and didn't call `Release()` — that path is buggy. `FO_STRONG_ASSERT`
    // logs and aborts the process so the violation is visible without the destructor
    // having to throw (which would `terminate` during unwind anyway).
    FO_STRONG_ASSERT(_heldLocks.empty(), "SyncContext destroyed with held entity locks", _heldLocks.size());
    FO_STRONG_ASSERT(_heldDescendantHolds.empty(), "SyncContext destroyed with held descendant-hold marks", _heldDescendantHolds.size());
    FO_STRONG_ASSERT(_singletonLocks.empty(), "SyncContext destroyed with held singleton locks", _singletonLocks.size());

    if (CurrentContext == this) {
        CurrentContext = _previousContext;
        _previousContext = nullptr;
    }
}

void SyncContext::Activate() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    // Save the previous current so a nested context (e.g. one created per event callback)
    // can pop back cleanly. Outermost Activate sees the slot empty and saves nullptr.
    _previousContext = CurrentContext;
    CurrentContext = this;
}

void SyncContext::Deactivate() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (CurrentContext == this) {
        CurrentContext = _previousContext;
        _previousContext = nullptr;
    }
}

// Returns the entity whose own `_ownedLock` backs `lock` — the topmost ancestor of `entity` still
// sharing `lock` through propagation (item -> container, custom entity -> holder). The held-lock set
// must pin THIS entity, not a propagated child: a child can be reverted/reparented out of the cover
// mid-hold (e.g. an item moved off a critter that is then destroyed, or the container destroyed and
// its items reverted), which drops the child's parent ref and lets the lock-owning ancestor — and
// its `_ownedLock` storage that `_heldLocks` still points at — be freed before `ReleaseLocks` runs.
// Pinning the owner keeps the lock storage alive for the whole hold regardless of later reparenting.
static auto FindLockOwner(ptr<ServerEntity> entity, nptr<EntityLock> lock) noexcept -> refcount_ptr<ServerEntity>
{
    FO_NO_STACK_TRACE_ENTRY();

    auto owner = entity.hold_ref();

    for (auto parent = entity->GetParentRaw(); parent && parent->GetEntityLock() == lock; parent = parent->GetParentRaw()) {
        owner = parent.as_ptr().hold_ref();
    }

    return owner;
}

// Bounded back-off between SyncEntities verify-after-acquire retries. A failed verify means a
// concurrent reparent (e.g. MapManager::Transfer) moved a requested entity out of the cover we
// just acquired; recomputing immediately re-races the same in-flight transfer, the busy-spin that
// exhausts the retry budget under sustained map-transfer churn. The first attempts only yield (the
// transfer usually completes within a scheduler slice), later attempts add a short, capped sleep so
// a longer transfer window can settle. Mirrors the stage-1 non-parking back-off in AcquireLocks and
// keeps the common single-retry case latency-free. Called with no locks held.
static void BackoffBeforeSyncRetry(int32_t attempt) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    constexpr int32_t yield_only_attempts = 8;
    constexpr int32_t max_backoff_shift = 6;

    if (attempt < yield_only_attempts) {
        std::this_thread::yield();
    }
    else {
        const int32_t shift = std::min(attempt - yield_only_attempts, max_backoff_shift);
        std::this_thread::sleep_for(std::chrono::microseconds(int64_t {50} << shift));
    }
}

void SyncContext::SyncEntities(span<const nptr<ServerEntity>> entities)
{
    FO_STACK_TRACE_ENTRY();

    // Deadlock prevention: a thread holding the engine's singleton lock (`Game.Lock()`) must
    // not call Sync(...) — combined with the per-property auto-lock on `Game.X` from another
    // thread, this would form a {Engine,EntityX} <-> {EntityX,Engine} cycle. Scripts must
    // `Game.Unlock()` before any `Sync::Lock(...)` call.
    if (!_singletonLocks.empty()) {
        throw EntitySyncException("Cannot call Sync() while holding a singleton lock (e.g. Game.Lock()) — Unlock first to avoid the per-property auto-lock deadlock");
    }

    CurrentContext = this;

    // Lock acquisition with verify-after-acquire / retry-if-escaped.
    //
    // The minimal-cover and covered-by-parent passes read parents lock-free (GetParentRaw),
    // which gives a snapshot that can be invalidated by concurrent SetParent racing through
    // the wait time of AcquireLocks. After we hold the computed lock set we walk each
    // requested entity's current parent chain and verify that at least one held lock still
    // covers it; if anyone escaped (was reparented out of the cover during the wait), we
    // release, recompute the cover from the now-current parent layout, and try again. A failed
    // verify backs off (BackoffBeforeSyncRetry) before recomputing so the racing reparent can
    // complete instead of being re-raced immediately; the budget is generous because each retry
    // is cheap (recompute the cover) and, with back-off, far more likely to make progress. The
    // give-up budget (MAX_SYNC_RETRIES) remains a livelock safety valve.
    for (int32_t attempt = 0;; attempt++) {
        // Collect all entity locks, then reduce to minimal covering set:
        // if two entities share a common ancestor, use the ancestor's lock
        unordered_map<EntityLock*, ServerEntity*> lock_to_entity;

        for (nptr<ServerEntity> entity : entities) {
            if (entity == nullptr) {
                continue;
            }

            nptr<EntityLock> lock = entity->GetEntityLock();

            if (lock == nullptr) {
                continue;
            }

            lock_to_entity.emplace(lock.get(), entity.get());
        }

        // Symmetric auto-widening for entity pairs that don't share an ownership parent:
        // Critter ↔ Player are linked via Critter::_player / Player::_controlledCr (not via
        // SetParent), so the parent-chain walk does not cover both halves. Expand the cover
        // by calling GetSyncWidenEntity on every requested entity and add the returned linked
        // entity's lock. Iterate to a fixed point so widening can chain (in practice it never
        // does today — at most one hop — but the loop keeps the rule robust if more linked
        // pairs are added later). Reads are lock-free; the verify-after-acquire step below
        // re-checks the linkage with locks held and triggers a retry if the linkage moved.
        {
            bool widened = true;

            while (widened) {
                widened = false;

                vector<ServerEntity*> snapshot;
                snapshot.reserve(lock_to_entity.size() + entities.size());

                for (auto* entity : lock_to_entity | std::views::values) {
                    snapshot.emplace_back(entity);
                }

                for (nptr<ServerEntity> entity : entities) {
                    if (entity != nullptr) {
                        snapshot.emplace_back(entity.get());
                    }
                }

                for (auto* entity : snapshot) {
                    auto widen = entity->GetSyncWidenEntity();

                    if (widen == nullptr) {
                        continue;
                    }

                    auto widen_lock = widen->GetEntityLock();

                    if (widen_lock == nullptr) {
                        continue;
                    }

                    if (lock_to_entity.emplace(widen_lock.get(), widen.get()).second) {
                        widened = true;
                    }
                }
            }
        }

        // Try to escalate: for each explicitly requested / widened entity, check if its parent's
        // lock covers another entity in the set. Keep the entity list separate from
        // lock_to_entity: inventory items can share a propagated lock with their holder, but the
        // holder still has to participate in sibling escalation (item + holder + recipient).
        vector<ServerEntity*> escalation_entities;
        escalation_entities.reserve(lock_to_entity.size() + entities.size());

        const auto add_escalation_entity = [&escalation_entities](ServerEntity* entity) {
            if (entity != nullptr && std::ranges::find(escalation_entities, entity) == escalation_entities.end()) {
                escalation_entities.emplace_back(entity);
            }
        };

        for (nptr<ServerEntity> entity : entities) {
            add_escalation_entity(entity.get());
        }
        for (auto& [lock, entity] : lock_to_entity) {
            ignore_unused(lock);
            add_escalation_entity(entity);
        }

        bool changed = true;

        while (changed) {
            changed = false;

            // Gather entity pointers that still have a lock representative in the current cover.
            vector<ServerEntity*> entries;
            entries.reserve(escalation_entities.size());

            for (auto* entity : escalation_entities) {
                if (entity == nullptr) {
                    continue;
                }

                auto lock = entity->GetEntityLock();

                if (lock != nullptr && lock_to_entity.count(lock.get()) != 0) {
                    entries.emplace_back(entity);
                }
            }

            for (size_t i = 0; i < entries.size(); i++) {
                for (size_t j = i + 1; j < entries.size(); j++) {
                    // GetParentRaw — escalation runs while computing which locks to acquire, the
                    // entries are not yet covered by any held lock so the validating GetParent
                    // would throw. Snapshot here may be stale; the verify step below catches it.
                    auto parent_i = entries[i]->GetParentRaw();
                    auto parent_j = entries[j]->GetParentRaw();

                    if (parent_i && parent_j && parent_i == parent_j) {
                        auto parent_lock = parent_i->GetEntityLock();

                        if (parent_lock != nullptr) {
                            auto lock_i = entries[i]->GetEntityLock();
                            auto lock_j = entries[j]->GetEntityLock();

                            if (parent_lock == lock_i && parent_lock == lock_j) {
                                continue;
                            }

                            // Both entities share the same parent — escalate to parent lock
                            lock_to_entity.erase(lock_i.get());
                            lock_to_entity.erase(lock_j.get());
                            lock_to_entity.emplace(parent_lock.get(), parent_i.get());
                            add_escalation_entity(parent_i.get());
                            changed = true;
                            break;
                        }
                    }
                }

                if (changed) {
                    break;
                }
            }
        }

        // Parent-cover reduction removed. When the caller asked for `Sync::Lock(cr, map)` — i.e.
        // an entity and its own parent — we hold BOTH locks. Reducing cr's lock "because the
        // parent map is already in the set" is unsafe under reparenting: MapManager::Transfer
        // severs cr→map mid-call (RemoveCritterFromMap sets cr.MapId = {}) and then re-validates
        // cr inside AddCritterToMap. With only the parent's lock held, that walk goes cr → null
        // and fails. Holding cr's own lock survives the reparenting.
        //
        // The earlier sibling-to-parent escalation (Sync::Lock(cr1, cr2) → {map}) stays in place:
        // there's no explicit child request to preserve, the caller asked for a generic "cover
        // these siblings" and the escalation IS that cover. Code paths that Transfer one of those
        // siblings from inside that context must explicitly include the moving entity in their
        // own Sync::Lock call so its own lock is kept (matches the "if you Transfer it, lock it"
        // convention already used by MapTransfer.fos).

        vector<EntityLock*> new_locks;
        vector<refcount_ptr<ServerEntity>> new_owners;
        new_locks.reserve(lock_to_entity.size());
        new_owners.reserve(lock_to_entity.size());

        for (auto& [lock, entity] : lock_to_entity) {
            new_locks.emplace_back(lock);
            // Pin the entity that OWNS this lock (whose `_ownedLock` it is), not necessarily the
            // requested entity. For a propagated lock the requested entity is an inner child using
            // its parent's `_ownedLock`; pinning only the child is unsafe because the child can be
            // reverted/reparented out of the cover mid-hold, dropping its parent ref and letting the
            // owning ancestor (and the `_ownedLock` storage `_heldLocks` points at) be freed before
            // `ReleaseLocks` runs. `FindLockOwner` walks up to the owner so the lock storage lives.
            new_owners.emplace_back(FindLockOwner(entity, lock));
        }

        // Hierarchical intention marks: a descendant-mark on every SEPARATE-lock ancestor of each cover
        // owner (a critter's map and location, a map's location, …), so no other thread can take any of
        // those ancestors exclusively while we hold a descendant under it. Computed from the owners'
        // CURRENT parent chains (lock-free, like the cover); the verify step below re-checks that every
        // ancestor is still marked and retries on a reparent. `AcquireLocks` drops any ancestor that is
        // itself in the cover (an exclusive hold already excludes its descendants).
        vector<EntityLock*> new_holds;
        vector<refcount_ptr<ServerEntity>> new_hold_owners;

        for (auto& owner : new_owners) {
            for (auto parent = owner->GetParentRaw(); parent; parent = parent->GetParentRaw()) {
                auto parent_lock = parent->GetEntityLock();

                if (parent_lock == nullptr) {
                    continue;
                }
                if (std::ranges::find(new_locks, parent_lock.get()) != new_locks.end()) {
                    continue; // covered exclusively
                }
                if (std::ranges::find(new_holds, parent_lock.get()) != new_holds.end()) {
                    continue; // already marked
                }

                new_holds.emplace_back(parent_lock.get());
                new_hold_owners.emplace_back(parent.as_ptr().hold_ref());
            }
        }

        ReleaseLocks();

        AcquireLocks(new_locks, std::move(new_owners), new_holds, std::move(new_hold_owners));

        // Verify: after locks are acquired, every requested entity must still be covered by
        // a lock in our held set. If a requested entity's parent chain no longer passes
        // through any held lock, it escaped during AcquireLocks (concurrent SetParent moved
        // it out of the cover) — release everything and recompute.
        const auto held_contains = [this](nptr<EntityLock> lock) noexcept { return lock != nullptr && std::ranges::find(_heldLocks, lock.get()) != _heldLocks.end(); };

        bool all_covered = true;

        for (nptr<ServerEntity> entity : entities) {
            if (entity == nullptr) {
                continue;
            }

            auto own_lock = entity->GetEntityLock();

            if (own_lock == nullptr) {
                continue;
            }

            // Walk the entity's CURRENT parent chain and look for any held lock.
            bool covered = held_contains(own_lock);

            if (!covered) {
                auto current = entity->GetParentRaw();

                while (current) {
                    if (held_contains(current->GetEntityLock())) {
                        covered = true;
                        break;
                    }

                    current = current->GetParentRaw();
                }
            }

            if (!covered) {
                all_covered = false;
                break;
            }

            // Re-check the sync-widen linkage with locks held. If a concurrent attach/detach
            // changed which entity should be paired during Phase 1, the held set won't include
            // the new pairing — fail verify and retry against the current linkage. The widen
            // target counts as covered when its OWN lock is held or — exactly like the entity
            // cover check above — when an ancestor's lock is held. Sibling-to-parent escalation
            // (e.g. Sync::Lock(cr1, cr2) collapsing both critters onto their shared map lock)
            // legitimately drops the widen target's own lock in favour of its parent's, and the
            // parent lock still covers it. Checking only to widens own lock here would fail to
            // verify on every retry and exhaust the budget whenever two player-controlled critters
            // (each widening to its Player) are locked together and escalated onto their map.
            auto widen = entity->GetSyncWidenEntity();

            if (widen != nullptr) {
                auto widen_lock = widen->GetEntityLock();

                if (widen_lock != nullptr) {
                    bool widen_covered = held_contains(widen_lock);

                    if (!widen_covered) {
                        auto current = widen->GetParentRaw();

                        while (current) {
                            if (held_contains(current->GetEntityLock())) {
                                widen_covered = true;
                                break;
                            };
                            current = current->GetParentRaw();
                        }
                    }

                    if (!widen_covered) {
                        all_covered = false;
                        break;
                    }
                }
            }
        }

        // Hierarchical verify: every SEPARATE-lock ancestor of every cover owner must be marked (held
        // in the cover or as an intention). If an ancestor reparented out from under us during
        // AcquireLocks (e.g. its parent link changed), a mark is now stale and the
        // ancestor/descendant exclusion would have a hole — recompute against the current layout.
        if (all_covered) {
            const auto marked = [this](nptr<EntityLock> lock) noexcept { //
                return std::ranges::find(_heldLocks, lock.get()) != _heldLocks.end() || std::ranges::find(_heldDescendantHolds, lock.get()) != _heldDescendantHolds.end();
            };

            for (auto& owner : _heldLockOwners) {
                if (owner.get() == nullptr) {
                    continue;
                }

                for (auto parent = owner->GetParentRaw(); parent; parent = parent->GetParentRaw()) {
                    auto parent_lock = parent->GetEntityLock();

                    if (parent_lock != nullptr && !marked(parent_lock)) {
                        all_covered = false;
                        break;
                    }
                }

                if (!all_covered) {
                    break;
                }
            }
        }

        if (all_covered) {
            return;
        }

        if (attempt + 1 >= MAX_SYNC_RETRIES) {
            ReleaseLocks();
            throw EntitySyncException("SyncEntities retry budget exhausted — entity reparenting raced lock acquisition repeatedly");
        }

        // Some entity escaped the cover during AcquireLocks: a concurrent reparent is in flight.
        // Release the stale cover (so we don't block other threads while waiting) and back off
        // briefly before recomputing against the current parent layout — without this the
        // immediate recompute re-races the same transfer and exhausts the budget under churn.
        ReleaseLocks();
        BackoffBeforeSyncRetry(attempt);
    }
}

void SyncContext::SyncEntity(nptr<ServerEntity> entity)
{
    FO_STACK_TRACE_ENTRY();

    if (entity == nullptr) {
        return;
    }

    nptr<ServerEntity> arr[] = {entity};
    SyncEntities(arr);
}

void SyncContext::EnsureEntitySynced(nptr<ServerEntity> entity)
{
    FO_STACK_TRACE_ENTRY();

    if (entity == nullptr) {
        return;
    }

    // A fully empty sync context is the legacy/unrestricted access mode. Registering a freshly
    // created entity inside that mode must not turn the scope into a partial lock set, otherwise the
    // very next access to an unrelated entity starts failing spuriously. Singleton locks (Game.Lock)
    // still make the context restricted, so fresh entities created under them must be added.
    if (_heldLocks.empty() && _singletonLocks.empty()) {
        return;
    }

    auto lock = entity->GetEntityLock();

    if (lock == nullptr) {
        return;
    }

    // No-op only if the entity's OWN lock is already held. Ancestor coverage is deliberately NOT enough
    // here: callers use EnsureEntitySynced specifically before reparenting/detaching the entity, and a
    // parent-chain lock stops covering it the instant it detaches — so the entity needs its OWN lock to
    // remain accessible across the sever. The contract is that the entity is already covered or locked: when
    // the caller has `Game.Sync`'d the enclosing subtree (the ancestor map/location), that ancestor's
    // exclusive lock EXCLUDES every other thread from this entity, so the own-lock acquire below is
    // uncontended and lands on the non-blocking fast path. If it is neither covered nor locked, the acquire
    // below throws — an entity must be brought into scope in advance, not block-acquired here.
    if (lock->IsLockedByCurrentThread()) {
        return;
    }

    // Collect this entity's separate-lock ancestors that we don't already hold exclusively or mark —
    // taking the entity's lock under the hierarchical model also requires marking each such ancestor.
    vector<EntityLock*> add_marks;
    vector<refcount_ptr<ServerEntity>> add_mark_owners;

    for (auto parent = entity->GetParentRaw(); parent; parent = parent->GetParentRaw()) {
        auto parent_lock = parent->GetEntityLock();

        if (parent_lock == nullptr || parent_lock == lock) {
            continue; // no lock, or shares `entity`'s lock via propagation — not a separate ancestor
        }
        if (std::ranges::find(_heldLocks, parent_lock.get()) != _heldLocks.end()) {
            continue; // already covered exclusively
        }
        if (std::ranges::find(_heldDescendantHolds, parent_lock.get()) != _heldDescendantHolds.end()) {
            continue; // already marked
        }
        if (std::ranges::find(add_marks, parent_lock.get()) != add_marks.end()) {
            continue;
        }

        add_marks.emplace_back(parent_lock.get());
        add_mark_owners.emplace_back(parent.as_ptr().hold_ref());
    }

    // Fast path: take the new lock + its new ancestor marks WITHOUT releasing the held set. The new ops
    // are TRIED (never blocked) in ascending-address order, so even though the new lock may sit *below*
    // locks we already hold, we never WAIT out of order — no wait-for cycle can form. On any contention
    // roll the prefix back and fall to the ordered union re-acquire below. This keeps repeated
    // EnsureEntitySynced during a transfer/reparent cheap: without it, each call would release and
    // re-acquire (and re-mark) the whole growing cover — O(n) churn that, under load, blows latency.
    {
        vector<pair<EntityLock*, bool>> ops; // bool = is-exclusive
        ops.reserve(add_marks.size() + 1);
        ops.emplace_back(lock.get(), true);

        for (auto* mark : add_marks) {
            ops.emplace_back(mark, false);
        }

        std::ranges::sort(ops, {}, &pair<EntityLock*, bool>::first);

        size_t acquired = 0;

        for (; acquired < ops.size(); acquired++) {
            auto& [op_lock, is_exclusive] = ops[acquired];
            const bool ok = is_exclusive ? op_lock->TryAcquire() : op_lock->TryRegisterDescendantHold();

            if (!ok) {
                break;
            }
        }

        if (acquired == ops.size()) {
            // Pin the lock's OWNER (whose `_ownedLock` it is), not necessarily `entity`: for a
            // propagated lock `entity` is a child that can be reverted/reparented out mid-hold, which
            // would free the owning ancestor's `_ownedLock` storage while `_heldLocks` still references it.
            _heldLocks.emplace_back(lock.get());
            _heldLockOwners.emplace_back(FindLockOwner(entity.as_ptr(), lock));

            for (size_t i = 0; i < add_marks.size(); i++) {
                _heldDescendantHolds.emplace_back(add_marks[i]);
                _heldDescendantHoldOwners.emplace_back(std::move(add_mark_owners[i]));
            }

            return;
        }

        for (size_t i = 0; i < acquired; i++) {
            if (ops[i].second) {
                ops[i].first->Release();
            }
            else {
                ops[i].first->UnregisterDescendantHold();
            }
        }
    }

    // Fast path failed: the entity (or one of its ancestors) is contended. The contract is that an entity
    // must already be **covered** (a held exclusive ancestor) or directly locked before EnsureEntitySynced
    // is called — the caller's `Game.Sync` establishes the scope in advance. So decide by coverage:
    //
    //  - Covered by a held exclusive ancestor → we already own the subtree; the contention is a transient
    //    concurrent acquisition (another thread grabbed this descendant before it blocks registering the
    //    ancestor mark we hold). Resolve it with the ordered-fair re-acquire below (release the union and
    //    re-take it in one ascending-address pass, which lets the other thread's mark through). This is the
    //    only blocking acquisition besides `Game.Sync`, and it is bounded.
    //  - NOT covered → the entity is not in our synced scope at all (the caller did not Sync it, or another
    //    thread owns an ancestor). That is a contract violation; throw rather than silently block-acquire a
    //    foreign entity.
    bool covered_by_ancestor = false;

    for (auto parent = entity->GetParentRaw(); parent; parent = parent->GetParentRaw()) {
        const auto parent_lock = parent->GetEntityLock();

        if (parent_lock != nullptr && parent_lock->IsLockedByCurrentThread()) {
            covered_by_ancestor = true;
            break;
        }
    }

    if (!covered_by_ancestor) {
        throw EntitySyncException("EnsureEntitySynced: entity is neither locked nor covered — its scope must be Sync'd in advance", entity->GetName(), entity->GetId());
    }

    // Covered → resolve the transient contention. Rebuild the whole union and route it through
    // `AcquireLocks`, whose stage-2 ordered-fair re-acquires it in one ascending-address pass without a
    // cycle. The add_mark_owners are intact (the fast path moved them only in its committing branch).
    vector<EntityLock*> union_locks = _heldLocks;
    vector<refcount_ptr<ServerEntity>> union_owners = _heldLockOwners;
    union_locks.emplace_back(lock.get());
    union_owners.emplace_back(FindLockOwner(entity.as_ptr(), lock));

    vector<EntityLock*> union_holds = _heldDescendantHolds;
    vector<refcount_ptr<ServerEntity>> union_hold_owners = _heldDescendantHoldOwners;

    for (size_t i = 0; i < add_marks.size(); i++) {
        union_holds.emplace_back(add_marks[i]);
        union_hold_owners.emplace_back(std::move(add_mark_owners[i]));
    }

    ReleaseLocks();

    AcquireLocks(union_locks, std::move(union_owners), union_holds, std::move(union_hold_owners));
}

void SyncContext::Release() noexcept
{
    FO_STACK_TRACE_ENTRY();

    // Drain both buckets so the dtor's "empty buckets" contract holds. A misbehaving job
    // may have leaked a singleton lock (script `Game.Lock()` without matching `Unlock()`) —
    // both ReleaseLocks and ReleaseSingletonLocks are noexcept and tolerate leftover entries
    // by recursion-LIFO unwinding, so calling here on the explicit teardown path is safe
    // even when paired calls were balanced (no-op on empty buckets).
    ReleaseLocks();
    ReleaseSingletonLocks();
}

auto SyncContext::GetHeldEntities() -> vector<ptr<ServerEntity>>
{
    FO_STACK_TRACE_ENTRY();

    vector<ptr<ServerEntity>> entities;
    entities.reserve(_heldLockOwners.size());

    for (auto& owner : _heldLockOwners) {
        if (owner.get() != nullptr && owner->GetId()) {
            entities.emplace_back(owner);
        }
    }

    return entities;
}

void SyncContext::LockSingleton(EntityLock* lock)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(lock, "Missing required lock");

    // Allocate a fresh ticket so this call participates in FIFO fairness exactly like a normal
    // Sync acquisition. EntityLock handles the recursion-on-same-thread bookkeeping internally,
    // so repeated LockSingleton calls from the same thread bump recursion without re-queuing.
    const auto ticket = NextSyncTicket();
    lock->Acquire(ticket);

    // Singleton bucket is intentionally separate from `_heldLocks` — `SyncEntities` replaces
    // `_heldLocks` wholesale every call, but a singleton acquired via `Game.Lock()` must survive
    // any number of subsequent `Sync::Lock(...)` calls inside the same job.
    _singletonLocks.emplace_back(lock);
}

void SyncContext::UnlockSingleton(EntityLock* lock)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(lock, "Missing required lock");
    FO_VERIFY_AND_THROW(lock->IsLockedByCurrentThread(), "Entity lock is not held by current thread");

    // Pop the most recent matching entry — paired LIFO with LockSingleton. If the script side
    // mismatches Lock/Unlock counts the leftover entries get drained at job exit.
    const auto rit = std::ranges::find(std::ranges::reverse_view(_singletonLocks), lock);
    FO_VERIFY_AND_THROW(rit != _singletonLocks.rend(), "Singleton entity lock release does not match any lock held by this sync context", static_cast<const void*>(lock), _singletonLocks.size());
    _singletonLocks.erase(std::next(rit).base());

    lock->Release();
}

void SyncContext::ReleaseSingletonLocks() noexcept
{
    FO_STACK_TRACE_ENTRY();

    // Drain in reverse so recursion counts unwind in LIFO order (matches Acquire/Release pairing).
    for (auto it = _singletonLocks.rbegin(); it != _singletonLocks.rend(); ++it) {
        (*it)->Release();
    }

    _singletonLocks.clear();
}

auto SyncContext::ValidateAccess(nptr<const ServerEntity> entity) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (entity == nullptr) {
        return false;
    }

    const auto lock = entity->GetEntityLock();

    if (lock == nullptr) {
        return true;
    }

    return lock->IsLockedByCurrentThread();
}

// Dedup a lock list while keeping its parallel owner list aligned. A simple sort-unique on `locks`
// alone would lose the lock↔owner correspondence, so pair them, sort+unique by lock, and rebuild both.
static void DedupLockOwners(vector<EntityLock*>& locks, vector<refcount_ptr<ServerEntity>>& owners)
{
    FO_NO_STACK_TRACE_ENTRY();

    vector<pair<EntityLock*, refcount_ptr<ServerEntity>>> paired;
    paired.reserve(locks.size());

    for (size_t i = 0; i < locks.size(); ++i) {
        paired.emplace_back(locks[i], std::move(owners[i]));
    }

    std::ranges::sort(paired, {}, &pair<EntityLock*, refcount_ptr<ServerEntity>>::first);
    auto last = std::ranges::unique(paired, {}, &pair<EntityLock*, refcount_ptr<ServerEntity>>::first);
    paired.erase(last.begin(), last.end());

    locks.clear();
    owners.clear();
    locks.reserve(paired.size());
    owners.reserve(paired.size());

    for (auto& [lock, owner] : paired) {
        locks.emplace_back(lock);
        owners.emplace_back(std::move(owner));
    }
}

void SyncContext::AcquireLocks(vector<EntityLock*>& locks, vector<refcount_ptr<ServerEntity>>&& owners, vector<EntityLock*>& holds, vector<refcount_ptr<ServerEntity>>&& hold_owners)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(locks.size() == owners.size(), "Entity lock list and owner list have different sizes before lock acquisition", locks.size(), owners.size());
    FO_VERIFY_AND_THROW(holds.size() == hold_owners.size(), "Intention lock list and owner list have different sizes before lock acquisition", holds.size(), hold_owners.size());

    DedupLockOwners(locks, owners);
    DedupLockOwners(holds, hold_owners);

    // Drop any intention target that is also an exclusive cover lock: an exclusive hold already
    // excludes that node's descendants, so a separate descendant-mark on it would be redundant (and
    // would double-acquire the same lock in the combined pass). Cover wins.
    {
        const auto in_cover = [&locks](EntityLock* l) { return std::ranges::find(locks, l) != locks.end(); };

        vector<EntityLock*> filtered_holds;
        vector<refcount_ptr<ServerEntity>> filtered_hold_owners;
        filtered_holds.reserve(holds.size());
        filtered_hold_owners.reserve(holds.size());

        for (size_t i = 0; i < holds.size(); i++) {
            if (!in_cover(holds[i])) {
                filtered_holds.emplace_back(holds[i]);
                filtered_hold_owners.emplace_back(std::move(hold_owners[i]));
            }
        }

        holds = std::move(filtered_holds);
        hold_owners = std::move(filtered_hold_owners);
    }

    // Combined ascending-address op list over the cover (exclusive) and the intention marks. A SINGLE
    // total order across BOTH kinds is what makes the mixed acquire deadlock-free: a thread only ever
    // blocks on its lowest not-yet-taken lock while holding strictly lower-addressed ones, so no
    // wait-for cycle can form regardless of which ops are exclusive vs intention. The two sets are
    // disjoint (filter above), so each lock appears once.
    vector<pair<EntityLock*, bool>> ops; // bool = is-exclusive
    ops.reserve(locks.size() + holds.size());

    for (auto* lock : locks) {
        ops.emplace_back(lock, true);
    }
    for (auto* hold : holds) {
        ops.emplace_back(hold, false);
    }

    std::ranges::sort(ops, {}, &pair<EntityLock*, bool>::first);

    // Acquisition strategy (two stages), unchanged in shape from the exclusive-only version but each
    // op now dispatches on its kind (Acquire/Release for cover, RegisterDescendantHold/Unregister for
    // intention):
    //
    // Stage 1 — non-parking try-and-back-off. Try the whole sorted set; on the first contended op,
    // release the prefix we just took and spin-yield. Briefly contended locks are taken promptly
    // without parking; locks/marks already owned by this thread succeed immediately (re-entrant).
    //
    // Stage 2 — globally-ordered fair acquire (the deadlock-breaker for NESTED contexts that
    // genuinely hold-and-wait — see AcquireLocksOrderedFair). Releases the whole thread-held union
    // (cover + intentions, this context's prefix already released by the caller, plus ancestor
    // contexts') and re-acquires it in strict ascending-address order with blocking ops.
    constexpr int32_t non_parking_spin_budget = 64;

    bool acquired_all = false;

    for (int32_t spins = 0; spins < non_parking_spin_budget; spins++) {
        size_t acquired = 0;

        while (acquired < ops.size()) {
            auto& [lock, is_exclusive] = ops[acquired];
            const bool ok = is_exclusive ? lock->TryAcquire() : lock->TryRegisterDescendantHold();

            if (!ok) {
                break;
            }

            acquired++;
        }

        if (acquired == ops.size()) {
            acquired_all = true;
            break;
        }

        for (size_t i = 0; i < acquired; i++) {
            if (ops[i].second) {
                ops[i].first->Release();
            }
            else {
                ops[i].first->UnregisterDescendantHold();
            }
        }

        std::this_thread::yield();
    }

    if (!acquired_all) {
        AcquireLocksOrderedFair(locks, holds);
    }

    _heldLocks = std::move(locks);
    _heldLockOwners = std::move(owners);
    _heldDescendantHolds = std::move(holds);
    _heldDescendantHoldOwners = std::move(hold_owners);
}

void SyncContext::AcquireLocksOrderedFair(const vector<EntityLock*>& locks, const vector<EntityLock*>& holds)
{
    FO_STACK_TRACE_ENTRY();

    // Collect, across the ancestor SyncContext chain plus this call's targets, how many times the
    // calling thread holds each lock EXCLUSIVELY (`reacquire_count`) and how many descendant-marks it
    // has on each lock (`reregister_count`), so both can be released to zero and restored exactly. A
    // single lock may carry both (it is owned exclusively in one context and marked as an ancestor in
    // another — e.g. up-escalation), so the two maps are independent. `this` context's own prefix is
    // already released by the stage-1 back-off, so it contributes only via `locks`/`holds`.
    unordered_map<EntityLock*, int32_t> reacquire_count;
    unordered_map<EntityLock*, int32_t> reregister_count;

    const auto add_excl = [&reacquire_count](EntityLock* lock) {
        if (lock != nullptr && !reacquire_count.contains(lock)) {
            reacquire_count.emplace(lock, lock->GetExclusiveRecursionForCurrentThread());
        }
    };
    const auto add_hold = [&reregister_count](EntityLock* lock) {
        if (lock != nullptr && !reregister_count.contains(lock)) {
            reregister_count.emplace(lock, lock->GetDescendantHoldCountForCurrentThread());
        }
    };

    for (auto* ancestor = _previousContext; ancestor != nullptr; ancestor = ancestor->_previousContext) {
        for (auto* lock : ancestor->_heldLocks) {
            add_excl(lock);
        }
        for (auto* lock : ancestor->_singletonLocks) {
            add_excl(lock);
        }
        for (auto* lock : ancestor->_heldDescendantHolds) {
            add_hold(lock);
        }
    }

    // The targets of THIS acquire each need one extra hold on top of whatever ancestors already hold.
    for (auto* lock : locks) {
        reacquire_count[lock] += 1;
    }
    for (auto* lock : holds) {
        reregister_count[lock] += 1;
    }

    // Release every currently-held exclusive lock AND descendant-mark down to zero so the whole union
    // becomes orderable. After this the calling thread holds none of them; another thread waiting on
    // one can take it, which is exactly what breaks a cross-hold cycle. Release exclusives first, then
    // marks — a mark must outlive the descendant it represents, so we never drop a mark while still
    // owning that descendant exclusively. No entity state is observed across the transition, so
    // dropping the parent cover here is safe — it is fully restored (as a superset) before any script.
    for (auto& [lock, count] : reacquire_count) {
        const int32_t held = lock->GetExclusiveRecursionForCurrentThread();

        for (int32_t i = 0; i < held; i++) {
            lock->Release();
        }
    }
    for (auto& [lock, count] : reregister_count) {
        const int32_t held = lock->GetDescendantHoldCountForCurrentThread();

        for (int32_t i = 0; i < held; i++) {
            lock->UnregisterDescendantHold();
        }
    }

    // Re-acquire the entire union in strict ascending-address order. Ascending order across all
    // threads prevents any wait-for cycle (resource ordering); the FIFO ticket queue prevents
    // starvation. For each lock take its exclusive recursion first (blocking `Acquire`) then its
    // descendant-marks (blocking `RegisterDescendantHold`): a lock we just took exclusively re-marks
    // via the re-entrant owner fast path without blocking, so doing exclusive-then-mark never stalls.
    vector<EntityLock*> ordered;
    ordered.reserve(reacquire_count.size() + reregister_count.size());

    for (auto& [lock, count] : reacquire_count) {
        ordered.emplace_back(lock);
    }
    for (auto& [lock, count] : reregister_count) {
        if (!reacquire_count.contains(lock)) {
            ordered.emplace_back(lock);
        }
    }

    std::ranges::sort(ordered);

    // `Acquire` / `RegisterDescendantHold` block and throw `EntityLockWaitAbortedException` if the
    // server shuts down while parked. Reaching that here would corrupt bookkeeping unless we recover:
    // we have already released the whole union to zero and would only partially restore it, so ancestor
    // SyncContexts still list locks/marks this thread no longer holds — their teardown would strong-
    // assert. Re-acquiring is impossible (the abort keeps firing throughout shutdown), so on failure
    // bring the entire union back to zero and drop it from every ancestor's bookkeeping, leaving a
    // consistent "this thread holds nothing" state that unwinds cleanly. The guard is released after a
    // normal acquire, so it fires only on the abort (or any other) throw.
    auto restore_on_abort = scope_fail([this, &reacquire_count, &reregister_count]() noexcept {
        for (auto& [lock, count] : reacquire_count) {
            const int32_t held = lock->GetExclusiveRecursionForCurrentThread();

            for (int32_t i = 0; i < held; i++) {
                lock->Release();
            }
        }
        for (auto& [lock, count] : reregister_count) {
            const int32_t held = lock->GetDescendantHoldCountForCurrentThread();

            for (int32_t i = 0; i < held; i++) {
                lock->UnregisterDescendantHold();
            }
        }

        for (auto* ancestor = _previousContext; ancestor != nullptr; ancestor = ancestor->_previousContext) {
            ancestor->_heldLocks.clear();
            ancestor->_heldLockOwners.clear();
            ancestor->_heldDescendantHolds.clear();
            ancestor->_heldDescendantHoldOwners.clear();
            ancestor->_singletonLocks.clear();
        }
    });

    for (auto* lock : ordered) {
        if (const auto it = reacquire_count.find(lock); it != reacquire_count.end()) {
            for (int32_t i = 0; i < it->second; i++) {
                lock->Acquire(NextSyncTicket());
            }
        }
        if (const auto it = reregister_count.find(lock); it != reregister_count.end()) {
            for (int32_t i = 0; i < it->second; i++) {
                lock->RegisterDescendantHold(NextSyncTicket());
            }
        }
    }

    restore_on_abort.release();
}

void SyncContext::ReleaseLocks() noexcept
{
    FO_STACK_TRACE_ENTRY();

    // Release the exclusive cover FIRST, then the ancestor intention marks. A mark ("this thread holds
    // a descendant of you") must outlive the descendant's exclusive hold: dropping a mark lets a parked
    // thread take that ancestor exclusively, which is only safe once we no longer hold any descendant
    // under it. Reversing the order would briefly let another thread own an ancestor while we still
    // hold its descendant — the exact ancestor/descendant overlap the model forbids.
    for (auto it = _heldLocks.rbegin(); it != _heldLocks.rend(); ++it) {
        (*it)->Release();
    }

    _heldLocks.clear();
    // Releasing the owner refs may delete the owning entities; their `_ownedLock` storage is
    // gone after this point, but `_heldLocks` is already empty, so dangling pointers can't be
    // re-released by a subsequent SyncEntities call.
    _heldLockOwners.clear();

    for (auto it = _heldDescendantHolds.rbegin(); it != _heldDescendantHolds.rend(); ++it) {
        (*it)->UnregisterDescendantHold();
    }

    _heldDescendantHolds.clear();
    _heldDescendantHoldOwners.clear();
}

auto SyncContext::GetCurrentOnThisThread() noexcept -> SyncContext*
{
    FO_NO_STACK_TRACE_ENTRY();

    return CurrentContext;
}

auto SyncContext::GetOutermostOnThisThread() noexcept -> SyncContext*
{
    FO_NO_STACK_TRACE_ENTRY();

    auto* ctx = CurrentContext;

    if (ctx == nullptr) {
        return nullptr;
    }

    while (ctx->_previousContext != nullptr) {
        ctx = ctx->_previousContext;
    }

    return ctx;
}

auto NextSyncTicket() noexcept -> uint64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return TicketCounter.fetch_add(1, std::memory_order_relaxed);
}

void PropagateEntityLock(ptr<Item> item, ptr<EntityLock> parent_lock)
{
    FO_STACK_TRACE_ENTRY();

    item->SetEntityLock(parent_lock);

    if (item->_innerItems) {
        for (auto& inner : *item->_innerItems) {
            PropagateEntityLock(inner, parent_lock);
        }
    }
}

void RevertEntityLock(ptr<Item> item)
{
    FO_STACK_TRACE_ENTRY();

    auto owned_lock = item->GetOwnedLock();

    item->SetEntityLock(owned_lock);

    if (item->_innerItems) {
        for (auto& inner : *item->_innerItems) {
            PropagateEntityLock(inner, owned_lock);
        }
    }
}

void EnsureEntitySynced(nptr<ServerEntity> entity)
{
    FO_STACK_TRACE_ENTRY();

    SyncContext* ctx = SyncContext::GetCurrentOnThisThread();
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");
    ctx->EnsureEntitySynced(entity);
}

FO_END_NAMESPACE
