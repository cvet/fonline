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

// Tuning constants of the sync machinery, gathered here so their budgets can be reasoned about (and
// adjusted) together. All of them bound RETRIES of non-blocking attempts; none of them bounds a
// blocking (parked) wait — parked waits are FIFO-fair and end when the holder releases.

// SyncEntities verify-after-acquire retries: a failed verify means a concurrent reparent moved a
// requested entity out of the just-acquired cover, so the set is recomputed and re-acquired.
// Deliberately sized far above realistic reparent churn — exhausting it throws EntitySyncException
// and is treated as a livelock tripwire (a reparent storm), not a normal give-up path.
static constexpr int32_t MAX_SYNC_RETRIES = 128;

// AcquireLocks stage 1: whole-set non-blocking try-and-roll-back passes (with a plain yield between)
// before falling to the stage-2 fair acquire. Sized so briefly contended sets are taken promptly
// without ever parking; past it, contention is genuine and FIFO fairness matters more than latency.
static constexpr int32_t NON_PARKING_SPIN_BUDGET = 64;

// EnsureEntitySynced expansion retries. Under the no-camping protocol a covered entity's lock can be
// contended only by microsecond-scale try-windows of other threads' non-blocking passes, so a small
// budget with BackoffBeforeSyncRetry pacing absorbs every legitimate race; exhausting it throws and
// can only mean corruption (see the comment at the retry loop).
static constexpr int32_t MAX_CONTENDED_EXPANSION_ATTEMPTS = 16;

// BackoffBeforeSyncRetry pacing: the first attempts only yield (the contending operation usually
// completes within a scheduler slice), later attempts sleep 50us doubled per attempt and capped at
// 50us << BACKOFF_MAX_SHIFT (= 3.2ms), so a longer contention window can settle without busy-spin.
static constexpr int32_t BACKOFF_YIELD_ONLY_ATTEMPTS = 8;
static constexpr int32_t BACKOFF_MAX_SHIFT = 6;

static thread_local nptr<SyncContext> CurrentContext {};
static std::atomic<uint64_t> TicketCounter {};

EntityLock::EntityLock() noexcept
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
        if (!lock) {
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

    // The offending call site. An uncovered access is always a bug to fix, and script-side catch
    // handlers otherwise swallow the exception before its stack is ever reported.
    safe_call([] { WriteLog("SyncDiag   stack:\n{}", FormatStackTrace(GetStackTrace())); });
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

    if (!entity) {
        return true;
    }

    auto current_ctx = SyncContext::GetCurrentOnThisThread();
    FO_STRONG_ASSERT(current_ctx, "Entity access validation needs active sync context");
    ignore_unused(current_ctx);

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

SyncContext::SyncContext() noexcept
{
    FO_STACK_TRACE_ENTRY();
}

SyncContext::~SyncContext()
{
    FO_STACK_TRACE_ENTRY();

    // Contract: holders MUST drain locks via explicit `Release()` before destruction.
    // Production tear-down paths (`WrapJobWithSync`, `ServerEngine::RunScriptContext`,
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

    // Save the previous current so a nested context (e.g. one created for script execution)
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

    if (attempt < BACKOFF_YIELD_ONLY_ATTEMPTS) {
        std::this_thread::yield();
    }
    else {
        const int32_t shift = std::min(attempt - BACKOFF_YIELD_ONLY_ATTEMPTS, BACKOFF_MAX_SHIFT);
        std::this_thread::sleep_for(std::chrono::microseconds(int64_t {50} << shift));
    }
}

// The shared non-blocking acquisition pass and its rollback, used by every retrying acquire in this
// file (AcquireLocks stage 1, EnsureEntitySynced's expansion, AcquireLocksOrderedFair's no-camping
// loop). An op is one recursion unit of a lock: exclusive (`TryAcquire`) or a descendant-mark
// (`TryRegisterDescendantHold`). `skip_index` marks an op the caller already holds (a kept park
// grant); pass SIZE_MAX for none.

static auto TryAcquireOps(const vector<pair<ptr<EntityLock>, bool>>& ops, size_t skip_index) noexcept -> size_t
{
    FO_NO_STACK_TRACE_ENTRY();

    for (size_t i = 0; i < ops.size(); i++) {
        if (i == skip_index) {
            continue;
        }

        auto lock = ops[i].first;
        const bool is_exclusive = ops[i].second;

        if (!(is_exclusive ? lock->TryAcquire() : lock->TryRegisterDescendantHold())) {
            return i;
        }
    }

    return ops.size();
}

static void ReleaseOp(const pair<ptr<EntityLock>, bool>& op) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    auto lock = op.first;

    if (op.second) {
        lock->Release();
    }
    else {
        lock->UnregisterDescendantHold();
    }
}

static void RollbackOps(const vector<pair<ptr<EntityLock>, bool>>& ops, size_t count, size_t skip_index) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    for (size_t i = 0; i < count; i++) {
        if (i != skip_index) {
            ReleaseOp(ops[i]);
        }
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
        unordered_map<ptr<EntityLock>, ptr<ServerEntity>> lock_to_entity;

        for (auto entity : entities) {
            if (!entity) {
                continue;
            }

            auto lock = entity->GetEntityLock();

            if (!lock) {
                continue;
            }

            lock_to_entity.emplace(lock, entity);
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

                vector<ptr<ServerEntity>> snapshot;
                snapshot.reserve(lock_to_entity.size() + entities.size());

                for (auto entity : lock_to_entity | std::views::values) {
                    snapshot.emplace_back(entity);
                }

                for (auto entity : entities) {
                    if (entity) {
                        snapshot.emplace_back(entity);
                    }
                }

                for (auto entity : snapshot) {
                    auto widen = entity->GetSyncWidenEntity();

                    if (widen == nullptr) {
                        continue;
                    }

                    auto widen_lock = widen->GetEntityLock();

                    if (widen_lock == nullptr) {
                        continue;
                    }

                    if (lock_to_entity.emplace(widen_lock, widen).second) {
                        widened = true;
                    }
                }
            }
        }

        // We lock EXACTLY the requested entities (plus each one's sync-widen partner added above), and
        // NOTHING else. There is deliberately NO sibling-to-parent escalation: if the caller asks to
        // Sync {cr1, cr2}, we hold cr1's and cr2's OWN locks — we do NOT collapse two siblings onto their
        // shared parent (map/location). Escalation used to do that (`Sync(cr1, cr2)` → `{map}`), but it
        // left each sibling only *covered*, not exclusively held: a concurrent worker could then transiently
        // hold that sibling's own lock (racing an `EnsureEntitySynced` expansion into it), and a per-item
        // operation ended up running under a coarse map lock the caller never requested — coupling
        // unrelated work and manufacturing contention out of thin air. Locking precisely the requested set
        // keeps each entity exclusively held and genuinely uncontended, and is still deadlock-free (the
        // acquire below takes the whole set in ascending-address order via the two-stage `AcquireLocks`).
        // Holding N sibling locks instead of one ancestor lock is the intended trade — honour the caller's
        // request and keep exclusivity, over minimising lock count.
        //
        // (Parent-cover *reduction* was already removed for a related reason: `Sync::Lock(cr, map)` holds
        // BOTH cr's and the map's own lock, because `MapManager::Transfer` severs cr→map mid-call and
        // re-validates cr, which needs cr's own lock — not merely the parent map's.)

        vector<ptr<EntityLock>> new_locks;
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
        vector<ptr<EntityLock>> new_holds;
        vector<refcount_ptr<ServerEntity>> new_hold_owners;

        for (auto& owner : new_owners) {
            for (auto parent = owner->GetParentRaw(); parent; parent = parent->GetParentRaw()) {
                auto parent_lock = parent->GetEntityLock();

                if (parent_lock == nullptr) {
                    continue;
                }
                if (std::ranges::find(new_locks, parent_lock) != new_locks.end()) {
                    continue; // covered exclusively
                }
                if (std::ranges::find(new_holds, parent_lock) != new_holds.end()) {
                    continue; // already marked
                }

                new_holds.emplace_back(parent_lock);
                new_hold_owners.emplace_back(parent.as_ptr().hold_ref());
            }
        }

        ReleaseLocks();

        AcquireLocks(new_locks, std::move(new_owners), new_holds, std::move(new_hold_owners));

        // Verify: after locks are acquired, every requested entity must still be covered by
        // a lock in our held set. If a requested entity's parent chain no longer passes
        // through any held lock, it escaped during AcquireLocks (concurrent SetParent moved
        // it out of the cover) — release everything and recompute.
        const auto held_contains = [this](nptr<EntityLock> lock) noexcept { return lock && std::ranges::find(_heldLocks, lock) != _heldLocks.end(); };

        bool all_covered = true;

        for (auto entity : entities) {
            if (!entity) {
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

    if (!entity) {
        return;
    }

    nptr<ServerEntity> arr[] = {entity};
    SyncEntities(arr);
}

void SyncContext::EnsureEntitySynced(nptr<ServerEntity> entity)
{
    FO_STACK_TRACE_ENTRY();

    if (!entity) {
        return;
    }

    // The entity's own lock is ALWAYS taken here, including under a still-empty context. A freshly
    // registered/created entity is a real, uncovered lock the moment it exists; leaving it unlocked
    // let another worker legitimately Sync it (its content's time events, a peer job) and mutate its
    // properties while the creating thread was still initializing it uncovered — the CreateLocation
    // Properties data race. Taking the lock makes the creator hold its fresh subtree exclusively for
    // the rest of the job, so those concurrent jobs queue until it is done. Consequence: a job that
    // creates and registers an entity thereby becomes restricted (its later accesses are validated
    // against the held set), so a former empty-context path that touches an *unrelated* entity after
    // a create must Sync that entity explicitly.
    auto lock = entity->GetEntityLock();

    if (!lock) {
        return;
    }

    // No-op if the entity's OWN lock is already held: EnsureEntitySynced is idempotent across a
    // transfer/cascade that pulls the same entity in more than once.
    if (lock->IsLockedByCurrentThread()) {
        return;
    }

    // EnsureEntitySynced is a pure NON-BLOCKING lock EXPANSION — never a release-and-reacquire. It pulls
    // an entity that is already covered by a lock this thread holds (a locked ancestor on its parent
    // chain, or a locked widen-linked entity) — or a freshly created, still-parentless entity — into our
    // own held set, so we may then reparent or mutate it while our exclusive hold keeps every other thread
    // out. We take its own lock plus a descendant-mark on each separate-lock ancestor between it and the
    // cover, all NON-BLOCKING (`TryAcquire` / `TryRegisterDescendantHold`) in ascending-address order, so
    // no wait-for cycle forms even though the new lock may sit *below* locks we already hold. We never
    // release what we already hold, so a covered entity stays covered the whole time and cannot be moved
    // out from under us.
    vector<ptr<EntityLock>> add_marks;
    vector<refcount_ptr<ServerEntity>> add_mark_owners;

    for (auto parent = entity->GetParentRaw(); parent; parent = parent->GetParentRaw()) {
        auto parent_lock = parent->GetEntityLock();

        if (parent_lock == nullptr || parent_lock == lock) {
            continue; // no lock, or shares `entity`'s lock — not a separate ancestor
        }
        if (std::ranges::find(_heldLocks, parent_lock) != _heldLocks.end()) {
            continue; // already covered exclusively
        }
        if (std::ranges::find(_heldDescendantHolds, parent_lock) != _heldDescendantHolds.end()) {
            continue; // already marked
        }
        if (std::ranges::find(add_marks, parent_lock) != add_marks.end()) {
            continue;
        }

        add_marks.emplace_back(parent_lock);
        add_mark_owners.emplace_back(parent.as_ptr().hold_ref());
    }

    vector<pair<ptr<EntityLock>, bool>> ops; // bool = is-exclusive
    ops.reserve(add_marks.size() + 1);
    ops.emplace_back(lock, true);

    for (auto mark : add_marks) {
        ops.emplace_back(mark, false);
    }

    std::ranges::sort(ops, [](const auto& a, const auto& b) { return a.first < b.first; });

    // NON-BLOCKING attempts with bounded back-off — no release, no block-acquire. Because `SyncEntities`
    // locks exactly the requested set (there is no sibling-to-parent escalation), the entity is covered by
    // an ancestor this thread holds EXCLUSIVELY, which excludes every other thread from the whole subtree —
    // so the entity's own lock and each intermediate ancestor mark are ours to take uncontended, and this
    // lands on the first try. The exclusion is STRICT because parked acquisitions hold nothing (the
    // no-camping stage-2 protocol in `AcquireLocksOrderedFair`): a completed foreign acquisition inside
    // the subtree would have blocked our exclusive ancestor via its root mark, an in-flight one either
    // rolls back its transient try-pass or is parked empty-handed — so a miss here can only mean the
    // entity is not actually covered (a contract violation — the caller must `Game.Sync` its subtree in
    // advance) or a microsecond-scale try-window race. The uncovered case throws immediately; the
    // transient case is retried in place (roll the prefix back, back off, try again) because many callers
    // are script-initiated engine operations (DestroyCritter, Transfer) with no outer retry loop.
    // Exhausting the budget throws rather than block-acquiring — waiting while holding the ancestor would
    // be hold-and-wait outside the global order — and by the protocol above can only fire on corruption.
    for (int32_t attempt = 0;; attempt++) {
        const size_t acquired = TryAcquireOps(ops, std::numeric_limits<size_t>::max());

        if (acquired == ops.size()) {
            // Pin the lock's OWNER (whose `_ownedLock` it is), not necessarily `entity`: for a shared/
            // propagated lock `entity` is a child that could be reparented out mid-hold, which would free
            // the owning ancestor's `_ownedLock` storage while `_heldLocks` still references it.
            _heldLocks.emplace_back(lock);
            _heldLockOwners.emplace_back(FindLockOwner(entity, lock));

            for (size_t i = 0; i < add_marks.size(); i++) {
                _heldDescendantHolds.emplace_back(add_marks[i]);
                _heldDescendantHoldOwners.emplace_back(std::move(add_mark_owners[i]));
            }

            return;
        }

        RollbackOps(ops, acquired, std::numeric_limits<size_t>::max());

        if (!IsEntityAccessValid(entity, true)) {
            throw EntitySyncException("EnsureEntitySynced: entity is neither locked nor covered — its scope must be Sync'd in advance", entity->GetName(), entity->GetId());
        }

        if (attempt >= MAX_CONTENDED_EXPANSION_ATTEMPTS) {
            break;
        }

        BackoffBeforeSyncRetry(attempt);
    }

    throw EntitySyncException("EnsureEntitySynced: covered entity is persistently contended", entity->GetName(), entity->GetId());
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
        if (owner->GetId()) {
            entities.emplace_back(owner);
        }
    }

    return entities;
}

void SyncContext::LockSingleton(ptr<EntityLock> lock)
{
    FO_STACK_TRACE_ENTRY();

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

void SyncContext::UnlockSingleton(ptr<EntityLock> lock)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(lock->IsLockedByCurrentThread(), "Entity lock is not held by current thread");

    // Pop the most recent matching entry — paired LIFO with LockSingleton. If the script side
    // mismatches Lock/Unlock counts the leftover entries get drained at job exit.
    const auto rit = std::ranges::find(std::ranges::reverse_view(_singletonLocks), lock);
    FO_VERIFY_AND_THROW(rit != _singletonLocks.rend(), "Singleton entity lock release does not match any lock held by this sync context", lock.void_cast(), _singletonLocks.size());
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

    if (!entity) {
        return false;
    }

    const auto lock = entity->GetEntityLock();

    if (!lock) {
        return true;
    }

    return lock->IsLockedByCurrentThread();
}

// Dedup a lock list while keeping its parallel owner list aligned. A simple sort-unique on `locks`
// alone would lose the lock↔owner correspondence, so pair them, sort+unique by lock, and rebuild both.
static void DedupLockOwners(vector<ptr<EntityLock>>& locks, vector<refcount_ptr<ServerEntity>>& owners) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    vector<pair<ptr<EntityLock>, refcount_ptr<ServerEntity>>> paired;
    paired.reserve(locks.size());

    for (size_t i = 0; i < locks.size(); ++i) {
        paired.emplace_back(locks[i], std::move(owners[i]));
    }

    std::ranges::sort(paired, [](const auto& a, const auto& b) { return a.first < b.first; });
    auto last = std::ranges::unique(paired, [](const auto& a, const auto& b) { return a.first == b.first; });
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

void SyncContext::AcquireLocks(vector<ptr<EntityLock>>& locks, vector<refcount_ptr<ServerEntity>>&& owners, vector<ptr<EntityLock>>& holds, vector<refcount_ptr<ServerEntity>>&& hold_owners)
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
        const auto in_cover = [&locks](ptr<EntityLock> l) { return std::ranges::find(locks, l) != locks.end(); };

        vector<ptr<EntityLock>> filtered_holds;
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
    vector<pair<ptr<EntityLock>, bool>> ops; // bool = is-exclusive
    ops.reserve(locks.size() + holds.size());

    for (auto lock : locks) {
        ops.emplace_back(lock, true);
    }
    for (auto hold : holds) {
        ops.emplace_back(hold, false);
    }

    std::ranges::sort(ops, [](const auto& a, const auto& b) { return a.first < b.first; });

    // Acquisition strategy (two stages), unchanged in shape from the exclusive-only version but each
    // op now dispatches on its kind (Acquire/Release for cover, RegisterDescendantHold/Unregister for
    // intention):
    //
    // Stage 1 — non-parking try-and-back-off. Try the whole sorted set; on the first contended op,
    // release the prefix we just took and spin-yield. Briefly contended locks are taken promptly
    // without parking; locks/marks already owned by this thread succeed immediately (re-entrant).
    //
    // Stage 2 — fair no-camping acquire (the deadlock-breaker for NESTED contexts that genuinely
    // hold-and-wait — see AcquireLocksOrderedFair). Releases the whole thread-held union (cover +
    // intentions, this context's prefix already released by the caller, plus ancestor contexts')
    // and re-acquires it via non-blocking passes that PARK HOLDING NOTHING on the contended op —
    // so a parked thread never camps a lock another operation needs.
    bool acquired_all = false;

    for (int32_t spins = 0; spins < NON_PARKING_SPIN_BUDGET; spins++) {
        const size_t acquired = TryAcquireOps(ops, std::numeric_limits<size_t>::max());

        if (acquired == ops.size()) {
            acquired_all = true;
            break;
        }

        RollbackOps(ops, acquired, std::numeric_limits<size_t>::max());

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

void SyncContext::AcquireLocksOrderedFair(const vector<ptr<EntityLock>>& locks, const vector<ptr<EntityLock>>& holds)
{
    FO_STACK_TRACE_ENTRY();

    // Collect, across the ancestor SyncContext chain plus this call's targets, how many times the
    // calling thread holds each lock EXCLUSIVELY (`reacquire_count`) and how many descendant-marks it
    // has on each lock (`reregister_count`), so both can be released to zero and restored exactly. A
    // single lock may carry both (it is owned exclusively in one context and marked as an ancestor in
    // another — e.g. up-escalation), so the two maps are independent. `this` context's own prefix is
    // already released by the stage-1 back-off, so it contributes only via `locks`/`holds`.
    unordered_map<ptr<EntityLock>, int32_t> reacquire_count;
    unordered_map<ptr<EntityLock>, int32_t> reregister_count;

    const auto add_excl = [&reacquire_count](ptr<EntityLock> lock) {
        if (!reacquire_count.contains(lock)) {
            reacquire_count.emplace(lock, lock->GetExclusiveRecursionForCurrentThread());
        }
    };
    const auto add_hold = [&reregister_count](ptr<EntityLock> lock) {
        if (!reregister_count.contains(lock)) {
            reregister_count.emplace(lock, lock->GetDescendantHoldCountForCurrentThread());
        }
    };

    for (auto ancestor = _previousContext; ancestor; ancestor = ancestor->_previousContext) {
        for (auto lock : ancestor->_heldLocks) {
            add_excl(lock);
        }
        for (auto lock : ancestor->_singletonLocks) {
            add_excl(lock);
        }
        for (auto lock : ancestor->_heldDescendantHolds) {
            add_hold(lock);
        }
    }

    // The targets of THIS acquire each need one extra hold on top of whatever ancestors already hold.
    for (auto lock : locks) {
        reacquire_count[lock] += 1;
    }
    for (auto lock : holds) {
        reregister_count[lock] += 1;
    }

    // Release every currently-held exclusive lock AND descendant-mark down to zero so the whole union
    // becomes orderable. After this the calling thread holds none of them; another thread waiting on
    // one can take it, which is exactly what breaks a cross-hold cycle. Release exclusives first, then
    // marks — a mark must outlive the descendant it represents, so we never drop a mark while still
    // owning that descendant exclusively. No entity state is observed across the transition, so
    // dropping the parent cover here is safe — it is fully restored (as a superset) before any script.
    for (auto& [lock, count] : reacquire_count) {
        auto lock_ref = lock;
        const int32_t held = lock_ref->GetExclusiveRecursionForCurrentThread();

        for (int32_t i = 0; i < held; i++) {
            lock_ref->Release();
        }
    }
    for (auto& [lock, count] : reregister_count) {
        auto lock_ref = lock;
        const int32_t held = lock_ref->GetDescendantHoldCountForCurrentThread();

        for (int32_t i = 0; i < held; i++) {
            lock_ref->UnregisterDescendantHold();
        }
    }

    // Re-acquire the entire union with NO hold-and-wait while parked. Repeatedly try the whole
    // ordered set non-blocking; on the first contended op release the partial pass back to zero and
    // park on that op ALONE (blocking, FIFO ticket) — holding nothing — then keep its grant and
    // restart the pass. A parked thread therefore never owns any other sync lock, which gives two
    // guarantees the former blocking-sequential re-acquire (park while holding all lower-addressed
    // ops) could not:
    //   - no wait-for cycle can pass through a parked thread (it holds nothing while parked), and
    //   - no thread can CAMP an in-subtree own lock while parked on a mark for an exclusively held
    //     ancestor. Ancestor marks are acquired by address, not hierarchy, so an in-flight acquire
    //     may own a descendant's lock before its root mark is registered; under the old protocol it
    //     then parked HOLDING that descendant until the root was released — livelocking any root-
    //     exclusive operation (location destroy) that tried to expand onto the descendant. Now the
    //     exclusive-root invariant is strict: whoever holds an entity's lock exclusively owns the
    //     whole subtree outright, and `EnsureEntitySynced` expansions meet only bounded transient
    //     try-windows (see its retry loop).
    // Progress: parked waiters are FIFO per lock and every re-park re-uses the operation's original
    // ticket (allocated once below), so a waiter that loses a re-try race re-enters each queue ahead
    // of later-arriving operations.
    //
    // Flat per-recursion op list, sorted by ascending lock address with exclusive recursions before
    // marks per lock (an exclusively owned lock re-marks via the re-entrant owner fast path without
    // blocking).
    vector<pair<ptr<EntityLock>, bool>> ops; // bool = is-exclusive
    ops.reserve(reacquire_count.size() + reregister_count.size());

    for (auto& [lock, count] : reacquire_count) {
        auto lock_ref = lock;

        for (int32_t i = 0; i < count; i++) {
            ops.emplace_back(lock_ref, true);
        }
    }
    for (auto& [lock, count] : reregister_count) {
        auto lock_ref = lock;

        for (int32_t i = 0; i < count; i++) {
            ops.emplace_back(lock_ref, false);
        }
    }

    std::ranges::sort(ops, [](const pair<ptr<EntityLock>, bool>& a, const pair<ptr<EntityLock>, bool>& b) { //
        return !(a.first == b.first) ? a.first < b.first : a.second && !b.second;
    });

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
            auto lock_ref = lock;
            const int32_t held = lock_ref->GetExclusiveRecursionForCurrentThread();

            for (int32_t i = 0; i < held; i++) {
                lock_ref->Release();
            }
        }
        for (auto& [lock, count] : reregister_count) {
            auto lock_ref = lock;
            const int32_t held = lock_ref->GetDescendantHoldCountForCurrentThread();

            for (int32_t i = 0; i < held; i++) {
                lock_ref->UnregisterDescendantHold();
            }
        }

        for (auto ancestor = _previousContext; ancestor; ancestor = ancestor->_previousContext) {
            ancestor->_heldLocks.clear();
            ancestor->_heldLockOwners.clear();
            ancestor->_heldDescendantHolds.clear();
            ancestor->_heldDescendantHoldOwners.clear();
            ancestor->_singletonLocks.clear();
        }
    });

    // One ticket for the WHOLE re-acquire: every re-park re-queues with the operation's original
    // (old, hence front-of-queue) ticket, giving wound-wait-style seniority — a waiter that loses a
    // re-try race re-enters every queue ahead of later-arriving operations, which is the progress
    // argument of this loop. The loop is unbounded by design (like the blocking acquire it replaced),
    // but a periodic diagnostic makes a pathological spin observable instead of a silent stall.
    const uint64_t park_ticket = NextSyncTicket();
    size_t parked_index = std::numeric_limits<size_t>::max(); // op held over from the last park's grant

    for (int64_t round = 1;; round++) {
        const size_t acquired = TryAcquireOps(ops, parked_index);

        if (acquired == ops.size()) {
            break; // full union held
        }

        // Roll the partial pass back to zero — including the kept park grant (skipped by the
        // rollback regardless of its position, so exactly one release) — the park below must hold
        // NOTHING.
        RollbackOps(ops, acquired, parked_index);

        if (parked_index != std::numeric_limits<size_t>::max()) {
            ReleaseOp(ops[parked_index]);
        }

        if (round % 10000 == 0) {
            WriteLog("Fair lock re-acquire is spinning: round {} over {} ops", round, ops.size());
        }

        // Park on the contended op alone (blocking, FIFO ticket), holding nothing.
        auto contended_lock = ops[acquired].first;
        const bool contended_exclusive = ops[acquired].second;

        if (contended_exclusive) {
            contended_lock->Acquire(park_ticket);
        }
        else {
            contended_lock->RegisterDescendantHold(park_ticket);
        }

        parked_index = acquired;
    }

    restore_on_abort.release();
}

void SyncContext::ReleaseLocks() noexcept
{
    FO_STACK_TRACE_ENTRY();

    // Release the exclusive cover, dropping each lock's owner pin in the SAME step and BEFORE handing
    // the lock back, so no other thread can acquire an entity between our release of its lock and its
    // destruction. Dropping a pin may be the final reference; the entity must not be destroyed while
    // another thread holds/acquires its lock (that would free a `_mutex` under a concurrent `TryAcquire`).
    //
    // Key invariant: while we still hold the entity's OWN exclusive lock, its refcount is STABLE — no
    // other thread can acquire that lock, reach the entity to look it up, or remove it from its
    // container, since each of those needs the very lock we hold. So if our pin is the sole reference
    // (`GetRefCount() == 1`) the entity is provably unreachable: destroy it now, still under the lock,
    // and do NOT `Release()` the now-freed lock storage. Otherwise the entity outlives us (its container
    // or an outer scope holds a ref) — hand its lock back and drop our pin. Process in reverse (children
    // before parents) so destroying a child never dangles a still-listed parent.
    //
    // Marks are released afterwards, still last: a mark ("this thread holds a descendant of you") must
    // outlive the descendant's exclusive hold, else a parked thread could take the ancestor exclusively
    // while we still hold a descendant under it — the ancestor/descendant overlap the model forbids.
    FO_STRONG_ASSERT(_heldLocks.size() == _heldLockOwners.size(), "Held lock/owner arrays desynchronized", _heldLocks.size(), _heldLockOwners.size());

    for (size_t i = _heldLocks.size(); i-- > 0;) {
        auto lock = _heldLocks[i];
        // Steal the owner pin so it drops at the end of this iteration, after any lock release.
        auto owner = std::move(_heldLockOwners[i]);

        if (owner->GetEntityLock() == lock && owner->GetRefCount() == 1) {
            // Sole reference to an unreachable entity we still hold exclusively: destroy it under the
            // lock (when the stolen pin dies below). Its `_ownedLock` storage dies with it, so it must
            // not be Release()d afterwards.
        }
        else {
            lock->Release();
        }
    }

    _heldLocks.clear();
    _heldLockOwners.clear();

    for (auto it = _heldDescendantHolds.rbegin(); it != _heldDescendantHolds.rend(); ++it) {
        (*it)->UnregisterDescendantHold();
    }

    _heldDescendantHolds.clear();
    _heldDescendantHoldOwners.clear();
}

auto SyncContext::GetCurrentOnThisThread() noexcept -> nptr<SyncContext>
{
    FO_NO_STACK_TRACE_ENTRY();

    return CurrentContext;
}

auto SyncContext::GetOutermostOnThisThread() noexcept -> nptr<SyncContext>
{
    FO_NO_STACK_TRACE_ENTRY();

    auto ctx = CurrentContext;

    if (!ctx) {
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

void EnsureEntitySynced(nptr<ServerEntity> entity)
{
    FO_STACK_TRACE_ENTRY();

    auto ctx = SyncContext::GetCurrentOnThisThread();
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");
    ctx->EnsureEntitySynced(entity);
}

FO_END_NAMESPACE
