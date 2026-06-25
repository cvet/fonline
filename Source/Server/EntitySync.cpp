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

static constexpr int32_t MAX_VALIDATE_ATTEMPTS = 32;
static constexpr int32_t MAX_SYNC_RETRIES = 128;

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

    // Grant immediately only when the lock is fully free: no exclusive owner and no shared readers.
    if (_ownerThread.load(std::memory_order_relaxed) == std::thread::id {} && _sharedHolders.empty()) {
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
    entry_it->Shared = false;

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
    const bool exclusive_waiter_ahead = std::ranges::any_of(_waitQueue, //
        [](const WaitEntry& e) { return !e.Shared && e.State.load(std::memory_order_acquire) == WaitEntry::STATE_WAITING; });

    if (_ownerThread.load(std::memory_order_relaxed) == std::thread::id {} && !exclusive_waiter_ahead) {
        _sharedHolders.emplace(this_thread, 1);
        TSanAcquire(this);
        return;
    }

    const auto insert_pos = std::ranges::find_if(_waitQueue, [ticket](const auto& e) { return e.Ticket > ticket; });
    const auto entry_it = _waitQueue.emplace(insert_pos);
    entry_it->Ticket = ticket;
    entry_it->Waiter = this_thread;
    entry_it->Shared = true;

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

    if (!it->Shared) {
        // Exclusive waiter: grant only once every reader has drained.
        if (!_sharedHolders.empty()) {
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

    // Shared waiter: grant it plus every consecutive shared waiter, stopping at the first exclusive
    // waiter (so writers are not starved). Each granted reader is recorded before being woken so a
    // concurrent exclusive Acquire observes it as a live holder.
    for (; it != _waitQueue.end(); ++it) {
        if (it->State.load(std::memory_order_acquire) != WaitEntry::STATE_WAITING) {
            continue;
        }
        if (!it->Shared) {
            break;
        }

        int32_t expected = WaitEntry::STATE_WAITING;

        if (it->State.compare_exchange_strong(expected, WaitEntry::STATE_GRANTED, std::memory_order_acq_rel)) {
            _sharedHolders.emplace(it->Waiter, 1);
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

    if (_ownerThread.load(std::memory_order_relaxed) != std::thread::id {} || !_sharedHolders.empty()) {
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

auto IsEntityAccessValid(nptr<const ServerEntity> entity) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!entity) {
        return true;
    }

    const nptr<SyncContext> current_ctx = SyncContext::GetCurrentOnThisThread();
    FO_STRONG_ASSERT(current_ctx, "Entity access validation needs active sync context");

    if (current_ctx->IsEmpty()) {
        return true;
    }

    // Try to find a covering lock, verify it still covers after acquire, retry if it doesn't.
    const auto try_hold_entity = [](nptr<const ServerEntity> maybe_entity) noexcept -> refcount_nptr<const ServerEntity> { return maybe_entity.try_hold_ref(); };

    const auto dump_uncovered = [&try_hold_entity](const ServerEntity* e, string_view where) -> bool {
        struct DiagEntry
        {
            string_view Kind {};
            string Name {};
            ident_t Id {};
            string_view Lock {};
        };

        vector<DiagEntry> entries;
        bool covered = false;

        const auto lock_state = [&covered](nptr<EntityLock> lock) -> string_view {
            if (!lock) {
                covered = true;
                return "none";
            }

            if (lock->IsLockedByCurrentThread()) {
                covered = true;
                return "HELD-by-this-thread";
            }

            return "not-held";
        };

        for (auto walk = try_hold_entity(e); walk; walk = walk->GetParentRaw()) {
            const auto walk_lock = walk->GetEntityLock();
            entries.emplace_back(DiagEntry {string_view {"chain"}, string {walk->GetName()}, walk->GetId(), lock_state(walk_lock)});
        }

        nptr<const ServerEntity> widen;

        if (e != nullptr) {
            widen = e->GetSyncWidenEntity();
        }

        for (auto walk = try_hold_entity(widen); walk; walk = walk->GetParentRaw()) {
            const auto walk_lock = walk->GetEntityLock();
            entries.emplace_back(DiagEntry {string_view {"widen"}, string {walk->GetName()}, walk->GetId(), lock_state(walk_lock)});
        }

        if (covered) {
            return true;
        }

        WriteLog("SyncDiag access-without-sync [{}]: entity '{}' id={} destroyed={}", where, e != nullptr ? e->GetName() : string_view {}, e != nullptr ? e->GetId() : ident_t {}, e != nullptr && e->IsDestroyed());

        for (const auto& entry : entries) {
            WriteLog("SyncDiag   {}: '{}' id={} lock={}", entry.Kind, entry.Name, entry.Id, entry.Lock);
        }

        return false;
    };

    for (int32_t attempt = 0; attempt < MAX_VALIDATE_ATTEMPTS; attempt++) {
        nptr<EntityLock> found_lock;

        for (refcount_nptr<const ServerEntity> nullable_current = try_hold_entity(entity); nullable_current; nullable_current = nullable_current.as_ptr()->GetParentRaw()) {
            auto current = nullable_current.as_ptr();
            auto lock = current->GetEntityLock();

            if (!lock) {
                return true;
            }

            if (lock->IsLockedByCurrentThread()) {
                found_lock = lock;
                break;
            }
        }

        if (!found_lock) {
            // A player-controlled critter and its Player are coupled through GetSyncWidenEntity (not
            // parent containment), and SyncEntities co-locks them (symmetric auto-widen). Accessing
            // one is therefore valid whenever the other is covered; mirror the widen-aware cover
            // check SyncEntities performs at verify time (search the widen-linked entity's own +
            // ancestor locks). This only relaxes access; the entity's own chain failed above.
            const auto widen = entity->GetSyncWidenEntity();

            if (widen) {
                refcount_nptr<const ServerEntity> nullable_current = try_hold_entity(widen);

                while (nullable_current) {
                    auto current = nullable_current.as_ptr();
                    auto lock = current->GetEntityLock();

                    if (!lock || lock->IsLockedByCurrentThread()) {
                        return true;
                    }

                    nullable_current = current->GetParentRaw();
                }
            }

            // The lock-free parent walk can observe a transient no-parent/no-held-ancestor snapshot
            // while another thread is exchanging containment. Retry before declaring that a held map
            // or location ancestor does not cover the descendant.
            if (attempt + 1 < MAX_VALIDATE_ATTEMPTS) {
                continue;
            }

            if (dump_uncovered(entity.get(), "no-lock-found")) {
                return true;
            }

            return false;
        }

        // Verify: re-walk and confirm the chain still leads through the recorded lock.
        bool still_covers = false;

        for (refcount_nptr<const ServerEntity> nullable_verify = try_hold_entity(entity); nullable_verify; nullable_verify = nullable_verify.as_ptr()->GetParentRaw()) {
            auto verify = nullable_verify.as_ptr();

            if (verify->GetEntityLock() == found_lock) {
                still_covers = true;
                break;
            }
        }

        if (still_covers) {
            return true;
        }

        // Chain shifted between walk and verify: concurrent reparenting moved the entity out
        // of the held lock's cover. Loop and re-walk in case the entity is now covered by a
        // different held lock (e.g. caller holds multiple ancestors and the new chain hits one).
    }

    if (dump_uncovered(entity.get(), "retry-exhausted")) {
        return true;
    }

    FO_VERIFY_AND_CONTINUE(false, "Unable to prove entity is covered by a held sync lock", entity ? entity->GetId() : ident_t {}, MAX_VALIDATE_ATTEMPTS);

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
static auto EntityUsesLock(ptr<const ServerEntity> entity, ptr<const EntityLock> lock) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    nptr<const EntityLock> entity_lock = entity->GetEntityLock();
    return entity_lock == lock;
}

static auto FindLockOwner(ptr<ServerEntity> entity, ptr<const EntityLock> lock) noexcept -> refcount_ptr<ServerEntity>
{
    FO_NO_STACK_TRACE_ENTRY();

    auto owner = entity.hold_ref();

    auto nullable_parent = entity->GetParentRaw();

    while (nullable_parent) {
        auto parent = nullable_parent.as_ptr();

        if (!EntityUsesLock(parent, lock)) {
            break;
        }
        owner = parent.hold_ref();
        nullable_parent = parent->GetParentRaw();
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

void SyncContext::SyncEntities(const_span<nptr<ServerEntity>> entities)
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
        unordered_map<nptr<EntityLock>, ptr<ServerEntity>> lock_to_entity;

        for (nptr<ServerEntity> requested_entity : entities) {
            if (!requested_entity) {
                continue;
            }

            auto lock = requested_entity->GetEntityLock();

            if (!lock) {
                continue;
            }

            lock_to_entity.emplace(lock, requested_entity.as_ptr());
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

                for (ptr<ServerEntity> covered_entity : lock_to_entity | std::views::values) {
                    snapshot.emplace_back(covered_entity);
                }

                for (nptr<ServerEntity> requested_entity : entities) {
                    if (requested_entity) {
                        snapshot.emplace_back(requested_entity.as_ptr());
                    }
                }

                for (ptr<ServerEntity> snapshot_entity : snapshot) {
                    auto widen = snapshot_entity->GetSyncWidenEntity();

                    if (!widen) {
                        continue;
                    }

                    auto widen_lock = widen->GetEntityLock();

                    if (!widen_lock) {
                        continue;
                    }

                    if (lock_to_entity.emplace(widen_lock, widen.as_ptr()).second) {
                        widened = true;
                    }
                }
            }
        }

        // Try to escalate: for each explicitly requested / widened entity, check if its parent's
        // lock covers another entity in the set. Keep the entity list separate from
        // lock_to_entity: inventory items can share a propagated lock with their holder, but the
        // holder still has to participate in sibling escalation (item + holder + recipient).
        vector<ptr<ServerEntity>> escalation_entities;
        escalation_entities.reserve(lock_to_entity.size() + entities.size());

        const auto add_escalation_entity = [&escalation_entities](nptr<ServerEntity> nullable_entity) {
            if (nullable_entity) {
                auto entity = nullable_entity.as_ptr();

                if (std::ranges::find(escalation_entities, entity) == escalation_entities.end()) {
                    escalation_entities.emplace_back(entity);
                }
            }
        };

        for (nptr<ServerEntity> requested_entity : entities) {
            add_escalation_entity(requested_entity);
        }
        for (auto& [lock, lock_entity] : lock_to_entity) {
            ignore_unused(lock);
            add_escalation_entity(lock_entity);
        }

        bool changed = true;

        while (changed) {
            changed = false;

            // Gather entity pointers that still have a lock representative in the current cover.
            vector<ptr<ServerEntity>> entries;
            entries.reserve(escalation_entities.size());

            for (ptr<ServerEntity> entry_entity : escalation_entities) {
                auto lock = entry_entity->GetEntityLock();

                if (lock && lock_to_entity.count(lock) != 0) {
                    entries.emplace_back(entry_entity);
                }
            }

            for (size_t i = 0; i < entries.size(); i++) {
                for (size_t j = i + 1; j < entries.size(); j++) {
                    // GetParentRaw — escalation runs while computing which locks to acquire, the
                    // entries are not yet covered by any held lock so the validating GetParent
                    // would throw. Snapshot here may be stale; the verify step below catches it.
                    refcount_nptr<ServerEntity> parent_i = entries[i]->GetParentRaw();
                    refcount_nptr<ServerEntity> parent_j = entries[j]->GetParentRaw();

                    if (parent_i && parent_j && parent_i.as_ptr() == parent_j.as_ptr()) {
                        auto parent = parent_i.as_ptr();
                        auto parent_lock = parent->GetEntityLock();

                        if (parent_lock) {
                            nptr<EntityLock> lock_i = entries[i]->GetEntityLock();
                            nptr<EntityLock> lock_j = entries[j]->GetEntityLock();

                            if (parent_lock == lock_i && parent_lock == lock_j) {
                                continue;
                            }

                            // Both entities share the same parent — escalate to parent lock
                            lock_to_entity.erase(lock_i);
                            lock_to_entity.erase(lock_j);
                            lock_to_entity.emplace(parent_lock, parent);
                            add_escalation_entity(parent_i.as_nptr());
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

        vector<ptr<EntityLock>> new_locks;
        vector<refcount_ptr<ServerEntity>> new_owners;
        new_locks.reserve(lock_to_entity.size());
        new_owners.reserve(lock_to_entity.size());

        for (auto& [nullable_lock, entity] : lock_to_entity) {
            auto lock = nullable_lock.as_ptr();
            new_locks.emplace_back(lock);
            // Pin the entity that OWNS this lock (whose `_ownedLock` it is), not necessarily the
            // requested entity. For a propagated lock the requested entity is an inner child using
            // its parent's `_ownedLock`; pinning only the child is unsafe because the child can be
            // reverted/reparented out of the cover mid-hold, dropping its parent ref and letting the
            // owning ancestor (and the `_ownedLock` storage `_heldLocks` points at) be freed before
            // `ReleaseLocks` runs. `FindLockOwner` walks up to the owner so the lock storage lives.
            new_owners.emplace_back(FindLockOwner(entity, lock));
        }

        ReleaseLocks();

        AcquireLocks(new_locks, std::move(new_owners));

        // Verify: after locks are acquired, every requested entity must still be covered by
        // a lock in our held set. If a requested entity's parent chain no longer passes
        // through any held lock, it escaped during AcquireLocks (concurrent SetParent moved
        // it out of the cover) — release everything and recompute.
        const auto held_contains = [this](nptr<EntityLock> lock) noexcept { return lock && std::ranges::find(_heldLocks, lock) != _heldLocks.end(); };

        bool all_covered = true;

        for (nptr<ServerEntity> requested_entity : entities) {
            if (!requested_entity) {
                continue;
            }

            auto own_lock = requested_entity->GetEntityLock();

            if (!own_lock) {
                continue;
            }

            // Walk the entity's CURRENT parent chain and look for any held lock.
            bool covered = held_contains(own_lock);

            if (!covered) {
                auto nullable_current = requested_entity->GetParentRaw();

                while (nullable_current) {
                    auto current = nullable_current.as_ptr();

                    if (held_contains(current->GetEntityLock())) {
                        covered = true;
                        break;
                    }

                    nullable_current = current->GetParentRaw();
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
            auto widen = requested_entity->GetSyncWidenEntity();

            if (widen) {
                auto widen_lock = widen->GetEntityLock();

                if (widen_lock) {
                    bool widen_covered = held_contains(widen_lock);

                    if (!widen_covered) {
                        auto nullable_current = widen->GetParentRaw();

                        while (nullable_current) {
                            auto current = nullable_current.as_ptr();

                            if (held_contains(current->GetEntityLock())) {
                                widen_covered = true;
                                break;
                            };
                            nullable_current = current->GetParentRaw();
                        }
                    }

                    if (!widen_covered) {
                        all_covered = false;
                        break;
                    }
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

    const array<nptr<ServerEntity>, 1> entities {entity};
    SyncEntities(entities);
}

void SyncContext::EnsureEntitySynced(nptr<ServerEntity> nullable_entity)
{
    FO_STACK_TRACE_ENTRY();

    if (!nullable_entity) {
        return;
    }

    auto entity = nullable_entity.as_ptr();

    // A fully empty sync context is the legacy/unrestricted access mode. Registering a freshly
    // created entity inside that mode must not turn the scope into a partial lock set, otherwise the
    // very next access to an unrelated entity starts failing spuriously. Singleton locks (Game.Lock)
    // still make the context restricted, so fresh entities created under them must be added.
    if (_heldLocks.empty() && _singletonLocks.empty()) {
        return;
    }

    auto nullable_lock = entity->GetEntityLock();

    if (!nullable_lock) {
        return;
    }

    auto lock = nullable_lock.as_ptr();

    if (nullable_lock->IsLockedByCurrentThread()) {
        return;
    }

    // This lock may sit *below* locks we already hold, so we cannot block-acquire it in place — that
    // would invert the global ascending-address order. `AcquireLocks` is non-parking and immune to
    // such inversions, but it replaces the whole held set; widening one lock onto the existing set is
    // worth a cheaper fast path first.
    //
    // Fast path: if the lock is uncontended, take it immediately. No other thread holds it, so
    // adding it out of order cannot create a wait-for cycle (we never *wait* out of order here).
    if (nullable_lock->TryAcquire()) {
        _heldLocks.emplace_back(lock);
        // Pin the lock's OWNER (whose `_ownedLock` it is), not necessarily `entity`: for a
        // propagated lock `entity` is a child that can be reverted/reparented out mid-hold, which
        // would let the owning ancestor and its `_ownedLock` storage be freed while `_heldLocks`
        // still references it. `FindLockOwner` walks up to the owner so the storage outlives the hold.
        _heldLockOwners.emplace_back(FindLockOwner(entity, lock));
        return;
    }

    // Contended: release our current set and re-acquire the whole union through `AcquireLocks`, which
    // takes it with the non-parking try-and-back-off (no inversion, no cycle) and sorts + dedups.
    vector<ptr<EntityLock>> union_locks = _heldLocks;
    vector<refcount_ptr<ServerEntity>> union_owners = _heldLockOwners;
    union_locks.emplace_back(lock);
    union_owners.emplace_back(FindLockOwner(entity, lock));

    ReleaseLocks();

    AcquireLocks(union_locks, std::move(union_owners));
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
            entities.emplace_back(owner.as_ptr());
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
    FO_VERIFY_AND_THROW(rit != _singletonLocks.rend(), "Singleton entity lock release does not match any lock held by this sync context", lock.cast<const void>().get(), _singletonLocks.size());
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

    auto lock = entity->GetEntityLock();

    if (!lock) {
        return true;
    }

    return lock->IsLockedByCurrentThread();
}

void SyncContext::AcquireLocks(vector<ptr<EntityLock>>& locks, vector<refcount_ptr<ServerEntity>>&& owners)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(locks.size() == owners.size(), "Entity lock list and owner list have different sizes before lock acquisition", locks.size(), owners.size());

    // Dedup `locks` while keeping `owners` aligned. A simple sort-unique on `locks` would lose
    // the lock↔owner correspondence, so we do an explicit two-pass dedup that mirrors the
    // operation on the owners vector.
    {
        vector<pair<ptr<EntityLock>, refcount_ptr<ServerEntity>>> paired;
        paired.reserve(locks.size());

        for (size_t i = 0; i < locks.size(); ++i) {
            paired.emplace_back(locks[i], std::move(owners[i]));
        }

        std::ranges::sort(paired, [](const auto& a, const auto& b) noexcept { return a.first < b.first; });
        auto last = std::ranges::unique(paired, {}, [](const auto& entry) noexcept { return entry.first; });
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

    // Acquisition strategy (two stages):
    //
    // Stage 1 — non-parking try-and-back-off (the fast path for transient contention). Try-acquire
    // the whole (sorted, deduped) set; on the first contended lock, release the prefix we just took
    // and spin-yield. This never holds-and-waits *for the duration of one attempt*, so a briefly
    // contended lock is taken promptly without parking. Locks already owned by this thread (the
    // parent cover) succeed immediately via TryAcquire's re-entrant fast path.
    //
    // Stage 2 — globally-ordered fair acquire (the deadlock-breaker). Stage 1 alone is NOT
    // sufficient for NESTED contexts: a per-event-callback context runs while its parent job/event
    // context already holds locks at arbitrary addresses (e.g. a per-player job covers {player,
    // controlled critter}; `OnPlayerLogout` fires and a handler calls `Sync(cr, leader)` on the
    // nested context). Those parent locks are held for the WHOLE nested attempt — that is real
    // hold-and-wait. Two concurrent group-logout cleanups form a genuine 2-cycle: thread 1's parent
    // holds cr_a and the nested acquire wants {cr_a, cr_b}; thread 2's parent holds cr_b and wants
    // {cr_b, cr_a}. The contended lock is permanently held by the OTHER thread's parent, so no
    // amount of non-parking back-off in stage 1 can ever break it (each thread spins forever holding
    // the lock the other needs). The escalation therefore releases the ENTIRE thread-held lock set
    // (this context's prefix + every ancestor SyncContext's locks, since `Sync()` observes no entity
    // state during a lock-set transition) and re-acquires the full union with BLOCKING `Acquire` in
    // strict ascending-address order. Total resource ordering makes that fair acquire deadlock-free
    // for any context, nested or not: no thread can hold a higher-addressed lock while blocking on a
    // lower-addressed one, so no wait-for cycle can form. Each ancestor lock's recursion count is
    // restored exactly, so parent bookkeeping is intact when control returns to the parent.
    constexpr int32_t non_parking_spin_budget = 64;

    bool acquired_all = false;

    for (int32_t spins = 0; spins < non_parking_spin_budget; spins++) {
        size_t acquired = 0;

        while (acquired < locks.size() && locks[acquired]->TryAcquire()) {
            acquired++;
        }

        if (acquired == locks.size()) {
            acquired_all = true;
            break;
        }

        for (size_t i = 0; i < acquired; i++) {
            locks[i]->Release();
        }

        std::this_thread::yield();
    }

    if (!acquired_all) {
        AcquireLocksOrderedFair(locks);
    }

    _heldLocks = std::move(locks);
    _heldLockOwners = std::move(owners);
}

void SyncContext::AcquireLocksOrderedFair(const vector<ptr<EntityLock>>& locks)
{
    FO_STACK_TRACE_ENTRY();

    // Collect every lock the calling thread currently holds across the ancestor SyncContext chain
    // (each context's per-Sync `_heldLocks` plus its singleton `_singletonLocks`) together with this
    // call's target `locks`, recording each lock's current exclusive recursion so it can be released
    // to zero and re-acquired the same number of times. `this` context's own prefix is already
    // released by the stage-1 back-off, so it contributes only via `locks` (recursion 0 here).
    unordered_map<ptr<EntityLock>, int32_t> reacquire_count;

    const auto add_held = [&reacquire_count](ptr<EntityLock> lock) {
        if (!reacquire_count.contains(lock)) {
            reacquire_count.emplace(lock, lock->GetExclusiveRecursionForCurrentThread());
        }
    };

    for (nptr<SyncContext> ancestor = _previousContext; ancestor; ancestor = ancestor->_previousContext) {
        for (ptr<EntityLock> lock : ancestor->_heldLocks) {
            add_held(lock);
        }
        for (ptr<EntityLock> lock : ancestor->_singletonLocks) {
            add_held(lock);
        }
    }

    // The target locks of THIS acquire each need exactly one extra hold on top of whatever ancestors
    // already hold them.
    for (ptr<EntityLock> lock : locks) {
        reacquire_count[lock] += 1;
    }

    // Release every currently-held lock down to zero so the whole union becomes orderable. After this
    // the calling thread holds none of these locks; another thread that was waiting on one can take
    // it, which is exactly what breaks a cross-hold 2-cycle. No entity state is observed between here
    // and the ordered re-acquire below, so dropping the parent cover across the transition is safe —
    // the parent cover is fully restored (as a superset) before control returns to any script.
    for (auto& [lock, count] : reacquire_count) {
        const int32_t held = lock->GetExclusiveRecursionForCurrentThread();

        for (int32_t i = 0; i < held; i++) {
            lock->Release();
        }
    }

    // Re-acquire the entire union in strict ascending-address order with the FIFO-fair blocking
    // `Acquire`. Ascending order across all threads prevents any wait-for cycle (resource ordering);
    // the FIFO ticket queue prevents starvation.
    vector<pair<ptr<EntityLock>, int32_t>> ordered {reacquire_count.begin(), reacquire_count.end()};
    std::ranges::sort(ordered, [](const ptr<EntityLock>& a, const ptr<EntityLock>& b) noexcept { return a < b; }, &pair<ptr<EntityLock>, int32_t>::first);

    // `Acquire` blocks and throws `EntityLockWaitAbortedException` if the server shuts down while it
    // is parked. Reaching that here would corrupt the lock bookkeeping unless we recover: we have
    // already released the whole union to zero and would only partially restore it, so the ancestor
    // SyncContexts still list locks this thread no longer owns — their teardown `Release()` would
    // strong-assert on an unowned lock. Re-acquiring is impossible (the abort keeps firing throughout
    // shutdown), so on failure bring the entire union back to zero and drop it from every ancestor's
    // bookkeeping, leaving a consistent "this thread holds nothing" state that unwinds cleanly. The
    // guard is released after a normal acquire, so it fires only on the abort (or any other) throw.
    auto restore_on_abort = scope_fail([this, &reacquire_count]() noexcept {
        for (auto& [lock, count] : reacquire_count) {
            const int32_t held = lock->GetExclusiveRecursionForCurrentThread();

            for (int32_t i = 0; i < held; i++) {
                lock->Release();
            }
        }

        for (nptr<SyncContext> ancestor = _previousContext; ancestor; ancestor = ancestor->_previousContext) {
            ancestor->_heldLocks.clear();
            ancestor->_heldLockOwners.clear();
            ancestor->_singletonLocks.clear();
        }
    });

    for (auto& [lock, count] : ordered) {
        for (int32_t i = 0; i < count; i++) {
            lock->Acquire(NextSyncTicket());
        }
    }

    restore_on_abort.release();
}

void SyncContext::ReleaseLocks() noexcept
{
    FO_STACK_TRACE_ENTRY();

    for (auto it = _heldLocks.rbegin(); it != _heldLocks.rend(); ++it) {
        (*it)->Release();
    }

    _heldLocks.clear();
    // Releasing the owner refs may delete the owning entities; their `_ownedLock` storage is
    // gone after this point, but `_heldLocks` is already empty, so dangling pointers can't be
    // re-released by a subsequent SyncEntities call.
    _heldLockOwners.clear();
}

auto SyncContext::GetCurrentOnThisThread() noexcept -> nptr<SyncContext>
{
    FO_NO_STACK_TRACE_ENTRY();

    return CurrentContext;
}

auto SyncContext::GetOutermostOnThisThread() noexcept -> nptr<SyncContext>
{
    FO_NO_STACK_TRACE_ENTRY();

    nptr<SyncContext> ctx = CurrentContext;

    if (!ctx) {
        return nullptr;
    }

    while (ctx->_previousContext) {
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
            PropagateEntityLock(inner.as_ptr(), parent_lock);
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
            PropagateEntityLock(inner.as_ptr(), owned_lock);
        }
    }
}

FO_END_NAMESPACE
