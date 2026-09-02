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

#include "EntitySync.h"
#include "Critter.h"
#include "Item.h"
#include "Location.h"
#include "Map.h"
#include "Server.h"
#include "ServerEntity.h"
#include "WorkerPool.h"

FO_BEGIN_NAMESPACE

// Gathered so the budgets can be reasoned about together. Every one bounds retries of non-blocking attempts;
// none bounds a parked wait, which is FIFO-fair and ends when the holder releases

// Sized far above realistic reparent churn, so exhausting it is a livelock tripwire rather than a normal
// give-up path
static constexpr int32_t MAX_SYNC_RETRIES = 128;

// Sized so a briefly contended set is taken without ever parking; past it, contention is genuine and FIFO
// fairness matters more than latency
static constexpr int32_t NON_PARKING_SPIN_BUDGET = 64;

// The first attempts only yield, since the contending operation usually finishes within a scheduler slice;
// later ones sleep with doubling so a longer window settles without busy-spin
static constexpr int32_t BACKOFF_YIELD_ONLY_ATTEMPTS = 8;
static constexpr int32_t BACKOFF_MAX_SHIFT = 6;

static thread_local nptr<SyncContext> CurrentContext {};
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

    auto this_thread = std::this_thread::get_id();

    if (_ownerThread.load(std::memory_order_acquire) == this_thread) {
        scoped_lock locker {_mutex};

        _recursionCount++;
        return;
    }

    unique_lock locker {_mutex};

    // A read->write upgrade would self-deadlock, because the exclusive grant waits for every shared holder to
    // drain, including this thread; asserted so a future caller is caught instead of hanging
    FO_VERIFY_AND_THROW(!_sharedHolders.contains(this_thread), "Entity lock cannot be upgraded from shared to exclusive by the same thread", std::hash<std::thread::id> {}(this_thread), _sharedHolders.size());

    // Granted only when nothing foreign holds the lock or its subtree; our own descendant-marks never block,
    // because escalating into a subtree we already hold is allowed
    if (_ownerThread.load(std::memory_order_relaxed) == std::thread::id {} && _sharedHolders.empty() && !HasForeignDescendantHolder(this_thread)) {
        _ownerThread.store(this_thread, std::memory_order_release);
        _recursionCount = 1;
        TSanAcquire(this);
        return;
    }

    // Sorted insertion by ticket. `list` keeps the node address stable across other
    // insert/erase operations so the in-progress `wait(0)` below points at the right atomic
    auto insert_pos = std::ranges::find_if(_waitQueue, [ticket](const auto& e) { return e.Ticket > ticket; });
    auto entry_it = _waitQueue.emplace(insert_pos);
    entry_it->Ticket = ticket;
    entry_it->Waiter = this_thread;
    entry_it->Kind = WaitKind::Exclusive;

    locker.unlock();

    // Looped because `atomic::wait` may return spuriously: the notification is best-effort and the standard
    // allows a wake before the value changes
    int32_t state = WaitEntry::STATE_WAITING;
    TimeMeter wait_time;

    while (state == WaitEntry::STATE_WAITING) {
        entry_it->State.wait(WaitEntry::STATE_WAITING, std::memory_order_acquire);
        state = entry_it->State.load(std::memory_order_acquire);
    }

    timespan lock_wait_duration = wait_time.GetDuration();

    locker.lock();
    _waitQueue.erase(entry_it);
    locker.unlock();

    if (state == WaitEntry::STATE_ABORTED) {
        throw EntityLockWaitAbortedException("EntityLock::Acquire aborted: shutdown in progress");
    }

    FO_STRONG_ASSERT(state == WaitEntry::STATE_GRANTED, "Exclusive entity lock waiter woke up in a non-granted state", ticket, state);
    auto owner_thread = _ownerThread.load(std::memory_order_acquire);
    FO_STRONG_ASSERT(owner_thread == this_thread, "Exclusive entity lock was granted but the current thread was not recorded as owner", ticket, std::hash<std::thread::id> {}(owner_thread), std::hash<std::thread::id> {}(this_thread));
    TSanAcquire(this);

    SyncContext::RecordLockWait(lock_wait_duration);
}

void EntityLock::AcquireShared(uint64_t ticket)
{
    FO_STACK_TRACE_ENTRY();

    auto this_thread = std::this_thread::get_id();

    // An exclusive owner trivially has read access, so folding the request into its recursion is what lets
    // `Game.Lock()` read Game properties without self-deadlocking
    if (_ownerThread.load(std::memory_order_acquire) == this_thread) {
        scoped_lock locker {_mutex};

        _recursionCount++;
        return;
    }

    unique_lock locker {_mutex};

    // Already a shared holder on this thread: bump recursion (nested reads)
    if (auto it = _sharedHolders.find(this_thread); it != _sharedHolders.end()) {
        it->second++;
        return;
    }

    // Queued behind an already-waiting writer, or a steady stream of new readers would starve it; the batch is
    // released together once the writer finishes
    if (_ownerThread.load(std::memory_order_relaxed) == std::thread::id {} && !HasWaitingExclusive()) {
        _sharedHolders.emplace(this_thread, 1);
        TSanAcquire(this);
        return;
    }

    auto insert_pos = std::ranges::find_if(_waitQueue, [ticket](const auto& e) { return e.Ticket > ticket; });
    auto entry_it = _waitQueue.emplace(insert_pos);
    entry_it->Ticket = ticket;
    entry_it->Waiter = this_thread;
    entry_it->Kind = WaitKind::Shared;

    locker.unlock();

    int32_t state = WaitEntry::STATE_WAITING;
    TimeMeter wait_time;

    while (state == WaitEntry::STATE_WAITING) {
        entry_it->State.wait(WaitEntry::STATE_WAITING, std::memory_order_acquire);
        state = entry_it->State.load(std::memory_order_acquire);
    }

    timespan lock_wait_duration = wait_time.GetDuration();

    locker.lock();
    _waitQueue.erase(entry_it);
    locker.unlock();

    if (state == WaitEntry::STATE_ABORTED) {
        throw EntityLockWaitAbortedException("EntityLock::AcquireShared aborted: shutdown in progress");
    }

    FO_VERIFY_AND_THROW(state == WaitEntry::STATE_GRANTED, "Shared entity lock waiter woke up in a non-granted state", ticket, state);
    // GrantWaiters recorded this thread in `_sharedHolders` before waking it
    TSanAcquire(this);
    SyncContext::RecordLockWait(lock_wait_duration);
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

        // Release won the race and already granted this waiter, so the grant stands: the holder releases as
        // usual and the next abort cascade catches any newer waiters
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

    auto this_thread = std::this_thread::get_id();

    // Shared acquired by the exclusive owner was folded into the exclusive recursion — unwind it
    // through the exclusive Release path (which also re-grants once recursion hits zero)
    if (_ownerThread.load(std::memory_order_acquire) == this_thread) {
        Release();
        return;
    }

    scoped_lock locker {_mutex};

    auto it = _sharedHolders.find(this_thread);
    FO_STRONG_ASSERT(it != _sharedHolders.end(), "Shared entity lock release without holder entry", _sharedHolders.size());

    if (--it->second == 0) {
        TSanRelease(this);
        _sharedHolders.erase(it);

        // A queued exclusive waiter can only proceed once the last reader has left
        if (_sharedHolders.empty()) {
            GrantWaiters();
        }
    }
}

void EntityLock::RegisterDescendantHold(uint64_t ticket)
{
    FO_STACK_TRACE_ENTRY();

    auto this_thread = std::this_thread::get_id();

    unique_lock locker {_mutex};

    // A thread that already owns or marks this lock is inside the subtree, so no foreign exclusive owner can
    // exist and the count simply grows
    if (_ownerThread.load(std::memory_order_acquire) == this_thread || _descendantHolders.contains(this_thread)) {
        _descendantHolders[this_thread]++;
        return;
    }

    // Yields to a parked writer, or an endless stream of sibling marks would starve a map- or
    // location-exclusive operation; shared holders are compatible and in practice never coexist here
    if (_ownerThread.load(std::memory_order_relaxed) == std::thread::id {} && !HasWaitingExclusive()) {
        _descendantHolders.emplace(this_thread, 1);
        return;
    }

    auto insert_pos = std::ranges::find_if(_waitQueue, [ticket](const auto& e) { return e.Ticket > ticket; });
    auto entry_it = _waitQueue.emplace(insert_pos);
    entry_it->Ticket = ticket;
    entry_it->Waiter = this_thread;
    entry_it->Kind = WaitKind::DescendantHold;

    locker.unlock();

    int32_t state = WaitEntry::STATE_WAITING;
    TimeMeter wait_time;

    while (state == WaitEntry::STATE_WAITING) {
        entry_it->State.wait(WaitEntry::STATE_WAITING, std::memory_order_acquire);
        state = entry_it->State.load(std::memory_order_acquire);
    }

    timespan lock_wait_duration = wait_time.GetDuration();

    locker.lock();
    _waitQueue.erase(entry_it);
    locker.unlock();

    if (state == WaitEntry::STATE_ABORTED) {
        throw EntityLockWaitAbortedException("EntityLock::RegisterDescendantHold aborted: shutdown in progress");
    }

    FO_VERIFY_AND_THROW(state == WaitEntry::STATE_GRANTED, "Descendant-hold waiter woke up in a non-granted state", ticket, state);
    // GrantWaiters recorded this thread in `_descendantHolders` before waking it
    SyncContext::RecordLockWait(lock_wait_duration);
}

auto EntityLock::TryRegisterDescendantHold() -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto this_thread = std::this_thread::get_id();

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

    auto this_thread = std::this_thread::get_id();

    scoped_lock locker {_mutex};

    auto it = _descendantHolders.find(this_thread);
    FO_STRONG_ASSERT(it != _descendantHolders.end(), "Descendant-hold release without holder entry", _descendantHolders.size());

    if (--it->second == 0) {
        _descendantHolders.erase(it);

        // Dropping the last foreign mark may unblock a queued exclusive writer (GrantWaiters re-checks
        // both readers and remaining foreign marks, and no-ops while we still own the lock exclusively)
        GrantWaiters();
    }
}

auto EntityLock::GetDescendantHoldCountForCurrentThread() const noexcept -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    scoped_lock locker {_mutex};

    auto it = _descendantHolders.find(std::this_thread::get_id());
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

    // Caller holds `_mutex`. Nothing can be granted while an exclusive owner still holds the lock
    if (_ownerThread.load(std::memory_order_relaxed) != std::thread::id {}) {
        return;
    }

    // FIFO by ticket, skipping entries already flipped to GRANTED or ABORTED, which linger until their waiter
    // wakes and erases them
    auto it = std::ranges::find_if(_waitQueue, [](const WaitEntry& e) { return e.State.load(std::memory_order_acquire) == WaitEntry::STATE_WAITING; });

    if (it == _waitQueue.end()) {
        return;
    }

    if (it->Kind == WaitKind::Exclusive) {
        // Exclusive waiter: grant only once every reader AND every foreign descendant-mark has drained
        // (another thread working inside this entity's subtree must finish before we take the ancestor)
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

    // The run stops at the first exclusive waiter so writers are not starved, and each grant is recorded in
    // the holder map before waking, so a concurrent Acquire sees it as a live holder
    WaitKind kind = it->Kind;

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

    auto this_thread = std::this_thread::get_id();

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

void EntityLock::LockStateMutex()
{
    FO_NO_STACK_TRACE_ENTRY();

    _mutex.lock();
}

bool EntityLock::TryLockStateMutex()
{
    FO_NO_STACK_TRACE_ENTRY();

    return _mutex.try_lock();
}

void EntityLock::UnlockStateMutex() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    _mutex.unlock();
}

bool EntityLock::IsEnsureOpCompatible(bool is_exclusive) const noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    auto this_thread = std::this_thread::get_id();
    auto owner_thread = _ownerThread.load(std::memory_order_acquire);

    if (is_exclusive) {
        return owner_thread == this_thread || (owner_thread == std::thread::id {} && _sharedHolders.empty() && !HasForeignDescendantHolder(this_thread));
    }

    return owner_thread == this_thread || _descendantHolders.contains(this_thread) || (owner_thread == std::thread::id {} && !HasWaitingExclusive());
}

void EntityLock::CommitEnsureOp(bool is_exclusive) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    auto this_thread = std::this_thread::get_id();
    auto owner_thread = _ownerThread.load(std::memory_order_relaxed);

    if (is_exclusive) {
        if (owner_thread == this_thread) {
            _recursionCount++;
        }
        else {
            _ownerThread.store(this_thread, std::memory_order_release);
            _recursionCount = 1;
            TSanAcquire(this);
        }
    }
    else if (owner_thread == this_thread || _descendantHolders.contains(this_thread)) {
        _descendantHolders[this_thread]++;
    }
    else {
        _descendantHolders.emplace(this_thread, 1);
    }
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
    // and single-threaded for our own lock state), so the owner check + recursion read are stable
    return _ownerThread.load(std::memory_order_relaxed) == std::this_thread::get_id() ? _recursionCount : 0;
}

// Pure logging of the parent and widen chains with their lock states, so the violation the caller is about to
// throw is debuggable; the coverage decision was already made
static void LogUncoveredEntity(nptr<const ServerEntity> entity) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    auto try_hold_entity = [](nptr<const ServerEntity> e) -> refcount_nptr<const ServerEntity> { return e.try_hold_ref(); };

    auto lock_state = [](nptr<const EntityLock> lock) -> string_view {
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
    // handlers otherwise swallow the exception before its stack is ever reported
    safe_call([] { WriteLog("SyncDiag   stack:\n{}", FormatStackTrace(GetStackTrace())); });
}

// Single-threaded logic runs every job on one worker, so no cover can ever be contended and the whole
// acquire/validate layer is bypassed — the mode is an engine setting, read through the entity at hand
auto IsSingleThreadedLogic(nptr<const ServerEntity> entity) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return entity && entity->GetEngine()->Settings->SingleThreadedLogic;
}

// One pass is authoritative because a reparent holds the entity's own lock, so a cover cannot flap mid-walk.
// A step passes on a held or lock-free ancestor of the entity's own chain or its widen-coupled one
auto IsEntityAccessValid(nptr<const ServerEntity> entity, bool diagnose) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!entity) {
        return true;
    }

    if (IsSingleThreadedLogic(entity)) {
        return true;
    }

    auto current_ctx = SyncContext::GetCurrentOnThisThread();
    FO_STRONG_ASSERT(current_ctx, "Entity access validation needs active sync context");
    ignore_unused(current_ctx);

    auto try_hold_entity = [](nptr<const ServerEntity> e) -> refcount_nptr<const ServerEntity> { return e.try_hold_ref(); };

    auto chain_covers = [&try_hold_entity](nptr<const ServerEntity> start) -> bool {
        for (auto current = try_hold_entity(start); current; current = current->GetParentRaw()) {
            auto lock = current->GetEntityLock();

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

    // Holders must drain their locks with an explicit Release() before destruction, so a non-empty bucket here
    // is a buggy path; asserting beats throwing from a destructor, which would terminate anyway
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
    // can pop back cleanly. Outermost Activate sees the slot empty and saves nullptr
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

void SyncContext::RecordLockWait(timespan duration) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    for (nptr<SyncContext> context = CurrentContext; context; context = context->_previousContext) {
        context->_lockWaitDuration += duration;
    }
}

// The held-lock set must pin the lock's owning ancestor, not a propagated child: a child reparented out of the
// cover mid-hold would let the owner, and the lock storage `_heldLocks` points at, be freed
static auto FindLockOwner(ptr<ServerEntity> entity, nptr<EntityLock> lock) noexcept -> refcount_ptr<ServerEntity>
{
    FO_NO_STACK_TRACE_ENTRY();

    auto owner = entity.hold_ref();

    for (auto parent = entity->GetParentRaw(); parent && parent->GetEntityLock() == lock; parent = parent->GetParentRaw()) {
        owner = require_refcount_ptr(parent);
    }

    return owner;
}

// Recomputing immediately would re-race the same in-flight reparent, so the first attempts yield and later
// ones sleep briefly, letting the transfer settle. Called with no locks held
static void BackoffBeforeSyncRetry(int32_t attempt) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (attempt < BACKOFF_YIELD_ONLY_ATTEMPTS) {
        std::this_thread::yield();
    }
    else {
        int32_t shift = std::min(attempt - BACKOFF_YIELD_ONLY_ATTEMPTS, BACKOFF_MAX_SHIFT);
        precise_sleep(std::chrono::microseconds(int64_t {50} << shift));
    }
}

// An op is one recursion unit of a lock, exclusive or a descendant-mark; `skip_index` marks one the caller
// already holds from a kept park grant, and SIZE_MAX means none

static auto TryAcquireOps(const_span<pair<ptr<EntityLock>, bool>> ops, size_t skip_index) noexcept -> size_t
{
    FO_NO_STACK_TRACE_ENTRY();

    for (size_t i = 0; i < ops.size(); i++) {
        if (i == skip_index) {
            continue;
        }

        auto lock = ops[i].first;
        bool is_exclusive = ops[i].second;

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

static void RollbackOps(const_span<pair<ptr<EntityLock>, bool>> ops, size_t count, size_t skip_index) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    for (size_t i = 0; i < count; i++) {
        if (i != skip_index && ops[i].second) {
            ReleaseOp(ops[i]);
        }
    }

    // Marks must outlive every exclusive descendant of the partial pass, or a single address-ordered release
    // could expose an ancestor while this thread still owns a descendant sorting after it
    for (size_t i = 0; i < count; i++) {
        if (i != skip_index && !ops[i].second) {
            ReleaseOp(ops[i]);
        }
    }
}

void SyncContext::SyncEntities(const_span<ptr<ServerEntity>> entities)
{
    FO_STACK_TRACE_ENTRY();

    // Nothing is ever acquired in single-threaded logic, so there is no cover to replace and the cycle the
    // singleton guard below rejects needs a second thread to exist at all
    if (!entities.empty() && IsSingleThreadedLogic(entities.front())) {
        return;
    }

    // Holding the engine singleton and then syncing would close a {Engine,Entity} <-> {Entity,Engine} cycle
    // against another thread's per-property auto-lock, so scripts must Game.Unlock() first
    if (!_singletonLocks.empty()) {
        throw EntitySyncException("Cannot call Sync() while holding a singleton lock (e.g. Game.Lock()) — Unlock first to avoid the per-property auto-lock deadlock");
    }

    CurrentContext = this;

    // Callers hand over borrowed handles, and the retry path below drops the whole cover before recomputing it,
    // so every request is pinned once for the transition rather than trusting the caller across that window
    small_vector<refcount_ptr<ServerEntity>, 8> requested;
    requested.reserve(entities.size());

    for (auto entity : entities) {
        requested.emplace_back(entity.hold_ref());
    }

    // The cover is computed from lock-free parent reads, which a concurrent reparent can invalidate while
    // AcquireLocks waits, so the held set is verified afterwards and recomputed if anything escaped
    for (int32_t attempt = 0;; attempt++) {
        // Collect all entity locks, then reduce to minimal covering set:
        // if two entities share a common ancestor, use the ancestor's lock
        unordered_map<ptr<EntityLock>, refcount_ptr<ServerEntity>> lock_to_entity;

        for (auto& entity : requested) {
            auto lock = entity->GetEntityLock();

            if (!lock) {
                continue;
            }

            lock_to_entity.emplace(lock, entity);
        }

        // Critter and Player are linked outside SetParent, so the parent walk covers only one half; the loop
        // runs to a fixed point in case a future pair chains, and the verify below re-checks the linkage
        {
            bool widened = true;

            while (widened) {
                widened = false;

                vector<ptr<ServerEntity>> snapshot;
                snapshot.reserve(lock_to_entity.size() + requested.size());

                for (auto& entity : lock_to_entity | std::views::values) {
                    snapshot.emplace_back(entity);
                }

                for (auto& entity : requested) {
                    snapshot.emplace_back(entity);
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

                    if (lock_to_entity.emplace(widen_lock, widen.take_not_null()).second) {
                        widened = true;
                    }
                }
            }
        }

        // Exactly the requested entities and their widen partners, with no escalation onto a shared parent:
        // that would leave a sibling merely covered rather than exclusively held, manufacturing contention

        SyncLockList new_locks;
        vector<refcount_ptr<ServerEntity>> new_owners;
        new_locks.reserve(lock_to_entity.size());
        new_owners.reserve(lock_to_entity.size());

        for (auto& [lock, entity] : lock_to_entity) {
            new_locks.emplace_back(lock);
            // The lock's owner is pinned rather than the requested entity, because an inner child reparented
            // out of the cover mid-hold would let the owning ancestor and its lock storage be freed
            new_owners.emplace_back(FindLockOwner(entity, lock));
        }

        // Marks every separate-lock ancestor so no other thread takes one exclusively above a descendant we
        // hold; an ancestor already in the cover is dropped, since an exclusive hold excludes its descendants
        SyncLockList new_holds;
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
                new_hold_owners.emplace_back(require_refcount_ptr(parent));
            }
        }

        ReleaseLocks();

        AcquireLocks(new_locks, std::move(new_owners), new_holds, std::move(new_hold_owners));

        // An entity whose parent chain no longer passes through a held lock escaped during AcquireLocks, so
        // everything is released and the cover recomputed
        auto held_contains = [this](nptr<EntityLock> lock) noexcept { return lock && std::ranges::find(_heldLocks, lock) != _heldLocks.end(); };

        bool all_covered = true;

        for (auto& entity : requested) {
            auto own_lock = entity->GetEntityLock();

            if (own_lock == nullptr) {
                continue;
            }

            // Walk the entity's CURRENT parent chain and look for any held lock
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

            // A widen target counts as covered through an ancestor too, not only its own lock: escalation may
            // legitimately have replaced it with the parent, and demanding the own lock would exhaust the budget
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

        // A stale mark left by an ancestor that reparented during AcquireLocks would leave a hole in the
        // ancestor/descendant exclusion, so the layout is recomputed instead
        if (all_covered) {
            auto marked = [this](nptr<EntityLock> lock) noexcept { //
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
            throw EntitySyncException("SyncEntities retry budget exhausted — entity reparenting or widen relinking raced lock acquisition repeatedly");
        }

        // The stale cover is released so other threads are not blocked while waiting, and the back-off keeps
        // the recompute from re-racing the same in-flight transfer
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

    small_vector<ptr<ServerEntity>, 1> sync_entities {entity};
    SyncEntities(sync_entities);
}

void SyncContext::EnsureEntitySynced(nptr<ServerEntity> entity)
{
    FO_STACK_TRACE_ENTRY();

    if (!entity) {
        return;
    }

    if (IsSingleThreadedLogic(entity)) {
        return;
    }

    if (!IsEntityAccessValid(entity, true)) {
        throw EntitySyncException("EnsureEntitySynced: entity is neither locked nor covered — its scope must be Sync'd in advance", entity->GetName(), entity->GetId());
    }

    EnsureEntitySyncedImpl(entity);
}

void SyncContext::EnsureFreshEntitySynced(nptr<ServerEntity> entity)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(entity, "Fresh entity capture requires a non-null entity");
    FO_VERIFY_AND_THROW(!entity->IsDestroying(), "Fresh entity capture cannot retain an entity that is being destroyed", entity->GetName(), entity->GetId());
    FO_VERIFY_AND_THROW(!entity->IsDestroyed(), "Fresh entity capture cannot retain a destroyed entity", entity->GetName(), entity->GetId());
    FO_VERIFY_AND_THROW(!entity->GetParentRaw(), "Fresh entity capture requires an unpublished parentless entity", entity->GetName(), entity->GetId());

    if (IsSingleThreadedLogic(entity)) {
        return;
    }

    EnsureEntitySyncedImpl(entity);
}

void FO_TSA_NO_ANALYSIS SyncContext::EnsureEntitySyncedImpl(ptr<ServerEntity> entity)
{
    FO_STACK_TRACE_ENTRY();

    // The trusted entry point may take an unpublished entity's lock under an empty context, while the ordinary
    // one first proves cover; both retain it here so publication cannot unprotect the native call
    auto lock = entity->GetEntityLock();

    if (!lock) {
        return;
    }

    // Ownership by another active context is not enough, because this one must keep the lock alive when that
    // context releases; it also makes repeated retention idempotent
    if (std::ranges::find(_heldLocks, lock) != _heldLocks.end() || std::ranges::find(_singletonLocks, lock) != _singletonLocks.end()) {
        return;
    }

    // Retention only, never a release-and-reacquire: the own lock and the marks up to the cover are taken as
    // one ascending-address transaction, so nothing cycles even though the new lock sits below held ones
    SyncLockList add_marks;
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
        add_mark_owners.emplace_back(require_refcount_ptr(parent));
    }

    // Every allocating step runs before the lock is touched, so recording the retention is a no-throw commit
    // and no acquired lock can end up missing from this context's release lists
    auto lock_owner = FindLockOwner(entity, lock);
    _heldLocks.reserve(_heldLocks.size() + 1);
    _heldLockOwners.reserve(_heldLockOwners.size() + 1);
    _heldDescendantHolds.reserve(_heldDescendantHolds.size() + add_marks.size());
    _heldDescendantHoldOwners.reserve(_heldDescendantHoldOwners.size() + add_mark_owners.size());

    small_vector<pair<ptr<EntityLock>, bool>, 8> ops; // bool = is-exclusive
    ops.reserve(add_marks.size() + 1);
    ops.emplace_back(lock, true);

    for (auto mark : add_marks) {
        ops.emplace_back(mark, false);
    }

    std::ranges::sort(ops, [](const auto& a, const auto& b) { return a.first < b.first; });

    // Only ever adds to the held set, and the exclusively held ancestor already excludes the whole subtree, so
    // the acquisition is mandatory and throws only on a broken cover contract, never on ordinary contention
    {
        for (size_t i = 1; i < ops.size(); i++) {
            FO_VERIFY_AND_THROW(ops[i - 1].first < ops[i].first, "Ensure acquisition operations must be sorted and unique", i);
        }

        vector<ptr<EntityLock>> state_locked;
        state_locked.reserve(ops.size());

        auto unlock_state = scope_exit([&state_locked]() FO_TSA_NO_ANALYSIS noexcept {
            for (auto it = state_locked.rbegin(); it != state_locked.rend(); ++it) {
                (*it)->UnlockStateMutex();
            }
        });

        // Every miss under a covered entity is a foreign thread mid-flight through its own non-blocking pass,
        // which clears in microseconds; nothing is retained across the back-off, so retrying always progresses
        for (int32_t attempt = 0;; attempt++) {
            bool acquired = true;

            for (const auto& [const_lock, is_exclusive] : ops) {
                ignore_unused(is_exclusive);
                auto op_lock = const_lock;

                if (!op_lock->TryLockStateMutex()) {
                    acquired = false;
                    break;
                }

                state_locked.emplace_back(op_lock);
            }

            // Preflight the complete batch before changing a recursion counter, owner, or descendant mark
            if (acquired) {
                for (const auto& [op_lock, is_exclusive] : ops) {
                    if (!op_lock->IsEnsureOpCompatible(is_exclusive)) {
                        acquired = false;
                        break;
                    }
                }
            }

            if (acquired) {
                break;
            }

            // Roll the partial batch back before backing off: parking on a prefix would block the very
            // foreign pass whose completion we are waiting for
            for (auto it = state_locked.rbegin(); it != state_locked.rend(); ++it) {
                (*it)->UnlockStateMutex();
            }

            state_locked.clear();

            if (attempt + 1 >= MAX_SYNC_RETRIES) {
                throw EntitySyncException("EnsureEntitySynced: covered entity lock is contended", entity->GetName(), entity->GetId());
            }

            BackoffBeforeSyncRetry(attempt);
        }

        // Every state mutex is still held and all operations are compatible, and terminate-on-OOM allocation
        // means nothing recoverable can split this commit
        for (const auto& [const_lock, is_exclusive] : ops) {
            auto op_lock = const_lock;
            op_lock->CommitEnsureOp(is_exclusive);
        }
    }

    // The lock's owner is pinned rather than `entity`, which for a propagated lock is a child that could be
    // reparented out mid-hold and free the storage `_heldLocks` still references
    _heldLocks.emplace_back(lock);
    _heldLockOwners.emplace_back(std::move(lock_owner));

    for (size_t i = 0; i < add_marks.size(); i++) {
        _heldDescendantHolds.emplace_back(add_marks[i]);
        _heldDescendantHoldOwners.emplace_back(std::move(add_mark_owners[i]));
    }
}

void SyncContext::Release() noexcept
{
    FO_STACK_TRACE_ENTRY();

    // Both buckets are drained so the destructor's empty-bucket contract holds even when a job leaked a
    // singleton lock; the releases are noexcept and no-op on empty buckets
    ReleaseLocks();
    ReleaseSingletonLocks();
}

auto SyncContext::GetHeldEntities() -> vector<ptr<ServerEntity>>
{
    FO_STACK_TRACE_ENTRY();

    vector<ptr<ServerEntity>> entities;
    entities.reserve(_heldLockOwners.size());

    for (auto& owner : _heldLockOwners) {
        entities.emplace_back(owner);
    }

    return entities;
}

void SyncContext::LockSingleton(ptr<EntityLock> lock)
{
    FO_STACK_TRACE_ENTRY();

    // A fresh ticket keeps this call in the same FIFO fairness as a normal Sync acquisition; recursion on the
    // same thread is handled inside EntityLock without re-queuing
    uint64_t ticket = NextSyncTicket();
    lock->Acquire(ticket);

    // Separate from `_heldLocks`, which `SyncEntities` replaces wholesale, because a `Game.Lock()` singleton
    // must survive every later `Sync::Lock(...)` in the same job
    _singletonLocks.emplace_back(lock);
}

void SyncContext::UnlockSingleton(ptr<EntityLock> lock)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(lock->IsLockedByCurrentThread(), "Entity lock is not held by current thread");

    // Pop the most recent matching entry — paired LIFO with LockSingleton. If the script side
    // mismatches Lock/Unlock counts the leftover entries get drained at job exit
    auto rit = std::ranges::find(std::ranges::reverse_view(_singletonLocks), lock);
    FO_VERIFY_AND_THROW(rit != _singletonLocks.rend(), "Singleton entity lock release does not match any lock held by this sync context", lock.void_cast(), _singletonLocks.size());
    _singletonLocks.erase(std::next(rit).base());

    lock->Release();
}

void SyncContext::ReleaseSingletonLocks() noexcept
{
    FO_STACK_TRACE_ENTRY();

    // Drain in reverse so recursion counts unwind in LIFO order (matches Acquire/Release pairing)
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

    if (IsSingleThreadedLogic(entity)) {
        return true;
    }

    auto lock = entity->GetEntityLock();

    if (!lock) {
        return true;
    }

    return lock->IsLockedByCurrentThread();
}

// Dedup a lock list while keeping its parallel owner list aligned. A simple sort-unique on `locks`
// alone would lose the lock↔owner correspondence, so pair them, sort+unique by lock, and rebuild both
static void DedupLockOwners(SyncLockList& locks, vector<refcount_ptr<ServerEntity>>& owners)
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

void SyncContext::AcquireLocks(SyncLockList& locks, vector<refcount_ptr<ServerEntity>>&& owners, SyncLockList& holds, vector<refcount_ptr<ServerEntity>>&& hold_owners)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(locks.size() == owners.size(), "Entity lock list and owner list have different sizes before lock acquisition", locks.size(), owners.size());
    FO_VERIFY_AND_THROW(holds.size() == hold_owners.size(), "Intention lock list and owner list have different sizes before lock acquisition", holds.size(), hold_owners.size());

    DedupLockOwners(locks, owners);
    DedupLockOwners(holds, hold_owners);

    // An exclusive hold already excludes that node's descendants, so a mark on it would be redundant and
    // would double-acquire the same lock in the combined pass
    {
        auto in_cover = [&locks](ptr<EntityLock> l) { return std::ranges::find(locks, l) != locks.end(); };

        SyncLockList filtered_holds;
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

    // One total order across both kinds is what makes the mixed acquire deadlock-free: a thread blocks only on
    // its lowest not-yet-taken lock while holding strictly lower ones
    small_vector<pair<ptr<EntityLock>, bool>, 8> ops; // bool = is-exclusive
    ops.reserve(locks.size() + holds.size());

    for (auto lock : locks) {
        ops.emplace_back(lock, true);
    }
    for (auto hold : holds) {
        ops.emplace_back(hold, false);
    }

    std::ranges::sort(ops, [](const auto& a, const auto& b) { return a.first < b.first; });

    // Stage 1 tries the whole sorted set without parking, rolling the prefix back on contention; stage 2 is the
    // deadlock breaker for nested contexts and parks holding nothing (Docs/ServerRuntime.md)
    bool acquired_all = false;

    for (int32_t spins = 0; spins < NON_PARKING_SPIN_BUDGET; spins++) {
        size_t acquired = TryAcquireOps(ops, std::numeric_limits<size_t>::max());

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

void SyncContext::AcquireLocksOrderedFair(const_span<ptr<EntityLock>> locks, const_span<ptr<EntityLock>> holds)
{
    FO_STACK_TRACE_ENTRY();

    // Counted so the union can be released to zero and restored exactly; one lock may be exclusive in one
    // context and marked in another, which is why the two maps stay independent
    unordered_map<ptr<EntityLock>, int32_t> reacquire_count;
    unordered_map<ptr<EntityLock>, int32_t> reregister_count;

    auto add_excl = [&reacquire_count](ptr<EntityLock> lock) {
        if (!reacquire_count.contains(lock)) {
            reacquire_count.emplace(lock, lock->GetExclusiveRecursionForCurrentThread());
        }
    };
    auto add_hold = [&reregister_count](ptr<EntityLock> lock) {
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

    // The targets of THIS acquire each need one extra hold on top of whatever ancestors already hold
    for (auto lock : locks) {
        reacquire_count[lock] += 1;
    }
    for (auto lock : holds) {
        reregister_count[lock] += 1;
    }

    // Dropping the whole union to zero is what breaks a cross-hold cycle; exclusives go first because a mark
    // must outlive the descendant it represents, and no entity state is observed across the transition
    for (auto& [lock, count] : reacquire_count) {
        auto lock_ref = lock;
        int32_t held = lock_ref->GetExclusiveRecursionForCurrentThread();

        for (int32_t i = 0; i < held; i++) {
            lock_ref->Release();
        }
    }
    for (auto& [lock, count] : reregister_count) {
        auto lock_ref = lock;
        int32_t held = lock_ref->GetDescendantHoldCountForCurrentThread();

        for (int32_t i = 0; i < held; i++) {
            lock_ref->UnregisterDescendantHold();
        }
    }

    // Parking holds nothing, so no wait-for cycle passes through a parked thread and nobody camps an in-subtree
    // lock while waiting on a mark; re-parks reuse the original ticket, which keeps FIFO seniority
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

    // A shutdown abort while parked would leave the union half-restored and ancestor contexts listing locks
    // this thread no longer holds, so the recovery drops everything to a clean holds-nothing state
    auto restore_on_abort = scope_fail([this, &reacquire_count, &reregister_count]() noexcept {
        for (auto& [lock, count] : reacquire_count) {
            auto lock_ref = lock;
            int32_t held = lock_ref->GetExclusiveRecursionForCurrentThread();

            for (int32_t i = 0; i < held; i++) {
                lock_ref->Release();
            }
        }
        for (auto& [lock, count] : reregister_count) {
            auto lock_ref = lock;
            int32_t held = lock_ref->GetDescendantHoldCountForCurrentThread();

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

    // One ticket for the whole re-acquire gives wound-wait seniority, which is this unbounded loop's progress
    // argument; the periodic diagnostic keeps a pathological spin from stalling silently
    uint64_t park_ticket = NextSyncTicket();
    size_t parked_index = std::numeric_limits<size_t>::max(); // op held over from the last park's grant

    for (int64_t round = 1;; round++) {
        size_t acquired = TryAcquireOps(ops, parked_index);

        if (acquired == ops.size()) {
            break; // full union held
        }

        // Rolled back to zero including the kept park grant, because the park below must hold nothing; exclusive
        // holds always go before any descendant mark
        if (parked_index != std::numeric_limits<size_t>::max() && ops[parked_index].second) {
            ReleaseOp(ops[parked_index]);
        }

        RollbackOps(ops, acquired, parked_index);

        if (parked_index != std::numeric_limits<size_t>::max() && !ops[parked_index].second) {
            ReleaseOp(ops[parked_index]);
        }

        if (round % 10000 == 0) {
            WriteLog("Fair lock re-acquire is spinning: round {} over {} ops", round, ops.size());
        }

        // Park on the contended op alone (blocking, FIFO ticket), holding nothing
        auto contended_lock = ops[acquired].first;
        bool contended_exclusive = ops[acquired].second;

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

    // While we hold an entity's own lock its refcount is stable, so a sole remaining pin proves it unreachable
    // and it is destroyed under the lock; children go before parents, and marks are released last
    FO_STRONG_ASSERT(_heldLocks.size() == _heldLockOwners.size(), "Held lock/owner arrays desynchronized", _heldLocks.size(), _heldLockOwners.size());

    for (size_t i = _heldLocks.size(); i-- > 0;) {
        auto lock = _heldLocks[i];
        // Steal the owner pin so it drops at the end of this iteration, after any lock release
        auto owner = std::move(_heldLockOwners[i]);

        if (owner->GetEntityLock() == lock && owner->GetRefCount() == 1) {
            // Destroyed under the lock as the stolen pin dies, so its `_ownedLock` storage goes with it and must
            // not be released afterwards
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

void SyncContext::RetainEntityPairInCurrentChain(nptr<ServerEntity> first, nptr<ServerEntity> second)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(first && second, "Missing entity in sync pair");

    auto first_lock = first->GetEntityLock();
    auto second_lock = second->GetEntityLock();

    FO_VERIFY_AND_THROW(first_lock && second_lock, "Missing lock in sync pair");

    // A nested operation publishing a new non-parented link retains both locks in every context owning either
    // half, so the outer cover stays continuous once the temporary context releases
    for (auto ctx = CurrentContext; ctx; ctx = ctx->_previousContext) {
        bool owns_first = std::ranges::find(ctx->_heldLocks, first_lock) != ctx->_heldLocks.end();
        bool owns_second = std::ranges::find(ctx->_heldLocks, second_lock) != ctx->_heldLocks.end();

        if (owns_first || owns_second) {
            ctx->EnsureEntitySynced(first);
            ctx->EnsureEntitySynced(second);
        }
    }
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
