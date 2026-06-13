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
    FO_RUNTIME_ASSERT(!_sharedHolders.contains(this_thread));

    // Grant immediately only when the lock is fully free: no exclusive owner and no shared readers.
    if (_ownerThread.load(std::memory_order_relaxed) == std::thread::id {} && _sharedHolders.empty()) {
        _ownerThread.store(this_thread, std::memory_order_release);
        _recursionCount = 1;
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

    FO_RUNTIME_ASSERT(state == WaitEntry::STATE_GRANTED);
    FO_RUNTIME_ASSERT(_ownerThread.load(std::memory_order_acquire) == this_thread);
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

    FO_RUNTIME_ASSERT(state == WaitEntry::STATE_GRANTED);
    // GrantWaiters recorded this thread in `_sharedHolders` before waking it.
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

    FO_STRONG_ASSERT(_ownerThread.load(std::memory_order_relaxed) == std::this_thread::get_id());

    scoped_lock locker {_mutex};

    _recursionCount--;

    if (_recursionCount > 0) {
        return;
    }

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
    FO_STRONG_ASSERT(it != _sharedHolders.end());

    if (--it->second == 0) {
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

auto IsEntityAccessValid(const ServerEntity* entity) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (entity == nullptr) {
        return true;
    }

    const auto* current_ctx = SyncContext::GetCurrentOnThisThread();
    FO_STRONG_ASSERT(current_ctx);

    if (current_ctx->IsEmpty()) {
        return true;
    }

    // Try to find a covering lock, verify it still covers after acquire, retry if it doesn't.
    const auto try_hold_entity = [](const ServerEntity* e) -> refcount_ptr<const ServerEntity> {
        if (e != nullptr && e->TryAddRef()) {
            return refcount_ptr<const ServerEntity>(refcount_ptr<const ServerEntity>::adopt, e);
        }
        else {
            return {};
        }
    };

    for (int32_t attempt = 0; attempt < MAX_VALIDATE_ATTEMPTS; attempt++) {
        const EntityLock* found_lock = nullptr;

        for (auto current = try_hold_entity(entity); current; current = current->GetParentRaw()) {
            const auto* lock = current->GetEntityLock();

            if (lock == nullptr) {
                return true;
            }

            if (lock->IsLockedByCurrentThread()) {
                found_lock = lock;
                break;
            }
        }

        if (found_lock == nullptr) {
            // A player-controlled critter and its Player are coupled through GetSyncWidenEntity (not
            // parent containment), and SyncEntities co-locks them (symmetric auto-widen). Accessing
            // one is therefore valid whenever the other is covered — mirror the widen-aware cover
            // check SyncEntities performs at verify time (search the widen-linked entity's own +
            // ancestor locks). This only relaxes access; the entity's own chain failed above.
            const auto* widen = const_cast<ServerEntity*>(entity)->GetSyncWidenEntity();

            if (widen != nullptr) {
                auto current = try_hold_entity(widen);

                while (current) {
                    const auto* lock = current->GetEntityLock();

                    if (lock == nullptr || lock->IsLockedByCurrentThread()) {
                        return true;
                    }

                    current = current->GetParentRaw();
                }
            }

            return false;
        }

        // Verify: re-walk and confirm the chain still leads through found_lock.
        bool still_covers = false;

        for (auto verify = try_hold_entity(entity); verify; verify = verify->GetParentRaw()) {
            if (verify->GetEntityLock() == found_lock) {
                still_covers = true;
                break;
            }
        }

        if (still_covers) {
            return true;
        }

        // Chain shifted between walk and verify — concurrent reparenting moved the entity out
        // of the held lock's cover. Loop and re-walk in case the entity is now covered by a
        // different held lock (e.g. caller holds multiple ancestors and the new chain hits one).
    }

    FO_RUNTIME_VERIFY(false);
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
    FO_STRONG_ASSERT(_heldLocks.empty());
    FO_STRONG_ASSERT(_singletonLocks.empty());

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
static auto FindLockOwner(ServerEntity* entity, const EntityLock* lock) noexcept -> refcount_ptr<ServerEntity>
{
    FO_NO_STACK_TRACE_ENTRY();

    refcount_ptr<ServerEntity> owner = entity;

    for (auto parent = entity->GetParentRaw(); parent && parent->GetEntityLock() == lock; parent = parent->GetParentRaw()) {
        owner = parent;
    }

    return owner;
}

void SyncContext::SyncEntities(span<ServerEntity*> entities)
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
    // release, recompute the cover from the now-current parent layout, and try again.
    constexpr int32_t max_sync_retries = 16;

    for (int32_t attempt = 0;; attempt++) {
        // Collect all entity locks, then reduce to minimal covering set:
        // if two entities share a common ancestor, use the ancestor's lock
        unordered_map<EntityLock*, ServerEntity*> lock_to_entity;

        for (auto* entity : entities) {
            if (entity == nullptr) {
                continue;
            }

            auto* lock = entity->GetEntityLock();

            if (lock == nullptr) {
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

                vector<ServerEntity*> snapshot;
                snapshot.reserve(lock_to_entity.size() + entities.size());

                for (auto* entity : lock_to_entity | std::views::values) {
                    snapshot.emplace_back(entity);
                }

                for (auto* entity : entities) {
                    if (entity != nullptr) {
                        snapshot.emplace_back(entity);
                    }
                }

                for (auto* entity : snapshot) {
                    auto* widen = entity->GetSyncWidenEntity();

                    if (widen == nullptr) {
                        continue;
                    }

                    auto* widen_lock = widen->GetEntityLock();

                    if (widen_lock == nullptr) {
                        continue;
                    }

                    if (lock_to_entity.emplace(widen_lock, widen).second) {
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

        for (auto* entity : entities) {
            add_escalation_entity(entity);
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

                auto* lock = entity->GetEntityLock();

                if (lock != nullptr && lock_to_entity.count(lock) != 0) {
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
                        auto* parent_lock = parent_i->GetEntityLock();

                        if (parent_lock != nullptr) {
                            auto* lock_i = entries[i]->GetEntityLock();
                            auto* lock_j = entries[j]->GetEntityLock();

                            if (parent_lock == lock_i && parent_lock == lock_j) {
                                continue;
                            }

                            // Both entities share the same parent — escalate to parent lock
                            lock_to_entity.erase(lock_i);
                            lock_to_entity.erase(lock_j);
                            lock_to_entity.emplace(parent_lock, parent_i.get());
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

        ReleaseLocks();

        AcquireLocks(new_locks, std::move(new_owners));

        // Verify: after locks are acquired, every requested entity must still be covered by
        // a lock in our held set. If a requested entity's parent chain no longer passes
        // through any held lock, it escaped during AcquireLocks (concurrent SetParent moved
        // it out of the cover) — release everything and recompute.
        const auto held_contains = [this](EntityLock* lock) noexcept { return lock != nullptr && std::ranges::find(_heldLocks, lock) != _heldLocks.end(); };

        bool all_covered = true;

        for (auto* entity : entities) {
            if (entity == nullptr) {
                continue;
            }

            auto* own_lock = entity->GetEntityLock();

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
            auto* widen = entity->GetSyncWidenEntity();

            if (widen != nullptr) {
                auto* widen_lock = widen->GetEntityLock();

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

        if (all_covered) {
            return;
        }

        if (attempt + 1 >= max_sync_retries) {
            ReleaseLocks();
            throw EntitySyncException("SyncEntities retry budget exhausted — entity reparenting raced lock acquisition repeatedly");
        }

        // Some entity escaped the cover; loop and recompute against the current parent layout.
    }
}

void SyncContext::SyncEntity(ServerEntity* entity)
{
    FO_STACK_TRACE_ENTRY();

    if (entity == nullptr) {
        return;
    }

    ServerEntity* arr[] = {entity};
    SyncEntities(arr);
}

void SyncContext::EnsureEntitySynced(ServerEntity* entity)
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

    auto* lock = entity->GetEntityLock();

    if (lock == nullptr) {
        return;
    }

    if (lock->IsLockedByCurrentThread()) {
        return;
    }

    // This lock may sit *below* locks we already hold, so we cannot block-acquire it in place — that
    // would invert the global ascending-address order. `AcquireLocks` is non-parking and immune to
    // such inversions, but it replaces the whole held set; widening one lock onto the existing set is
    // worth a cheaper fast path first.
    //
    // Fast path: if the lock is uncontended, take it immediately. No other thread holds it, so
    // adding it out of order cannot create a wait-for cycle (we never *wait* out of order here).
    if (lock->TryAcquire()) {
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
    vector<EntityLock*> union_locks = _heldLocks;
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

auto SyncContext::GetHeldEntities() -> vector<ServerEntity*>
{
    FO_STACK_TRACE_ENTRY();

    vector<ServerEntity*> entities;
    entities.reserve(_heldLockOwners.size());

    for (auto& owner : _heldLockOwners) {
        if (owner && owner->GetId()) {
            entities.emplace_back(owner.get());
        }
    }

    return entities;
}

void SyncContext::LockSingleton(EntityLock* lock)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(lock);

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

    FO_RUNTIME_ASSERT(lock);
    FO_RUNTIME_ASSERT(lock->IsLockedByCurrentThread());

    // Pop the most recent matching entry — paired LIFO with LockSingleton. If the script side
    // mismatches Lock/Unlock counts the leftover entries get drained at job exit.
    const auto rit = std::ranges::find(std::ranges::reverse_view(_singletonLocks), lock);
    FO_RUNTIME_ASSERT(rit != _singletonLocks.rend());
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

auto SyncContext::ValidateAccess(const ServerEntity* entity) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (entity == nullptr) {
        return false;
    }

    const auto* lock = entity->GetEntityLock();

    if (lock == nullptr) {
        return true;
    }

    return lock->IsLockedByCurrentThread();
}

void SyncContext::AcquireLocks(vector<EntityLock*>& locks, vector<refcount_ptr<ServerEntity>>&& owners)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(locks.size() == owners.size());

    // Dedup `locks` while keeping `owners` aligned. A simple sort-unique on `locks` would lose
    // the lock↔owner correspondence, so we do an explicit two-pass dedup that mirrors the
    // operation on the owners vector.
    {
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

void SyncContext::AcquireLocksOrderedFair(const vector<EntityLock*>& locks)
{
    FO_STACK_TRACE_ENTRY();

    // Collect every lock the calling thread currently holds across the ancestor SyncContext chain
    // (each context's per-Sync `_heldLocks` plus its singleton `_singletonLocks`) together with this
    // call's target `locks`, recording each lock's current exclusive recursion so it can be released
    // to zero and re-acquired the same number of times. `this` context's own prefix is already
    // released by the stage-1 back-off, so it contributes only via `locks` (recursion 0 here).
    unordered_map<EntityLock*, int32_t> reacquire_count;

    const auto add_held = [&reacquire_count](EntityLock* lock) {
        if (lock != nullptr && !reacquire_count.contains(lock)) {
            reacquire_count.emplace(lock, lock->GetExclusiveRecursionForCurrentThread());
        }
    };

    for (auto* ancestor = _previousContext; ancestor != nullptr; ancestor = ancestor->_previousContext) {
        for (auto* lock : ancestor->_heldLocks) {
            add_held(lock);
        }
        for (auto* lock : ancestor->_singletonLocks) {
            add_held(lock);
        }
    }

    // The target locks of THIS acquire each need exactly one extra hold on top of whatever ancestors
    // already hold them.
    for (auto* lock : locks) {
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
    vector<pair<EntityLock*, int32_t>> ordered {reacquire_count.begin(), reacquire_count.end()};
    std::ranges::sort(ordered, {}, &pair<EntityLock*, int32_t>::first);

    for (auto& [lock, count] : ordered) {
        for (int32_t i = 0; i < count; i++) {
            lock->Acquire(NextSyncTicket());
        }
    }
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

void PropagateEntityLock(Item* item, EntityLock* parent_lock)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(item);
    FO_RUNTIME_ASSERT(parent_lock);

    item->SetEntityLock(parent_lock);

    if (item->HasInnerItems()) {
        for (auto* inner : item->GetAllInnerItems()) {
            PropagateEntityLock(inner, parent_lock);
        }
    }
}

void RevertEntityLock(Item* item)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(item);

    item->SetEntityLock(&item->GetOwnedLock());

    if (item->HasInnerItems()) {
        for (auto* inner : item->GetAllInnerItems()) {
            PropagateEntityLock(inner, &item->GetOwnedLock());
        }
    }
}

FO_END_NAMESPACE
