# Exception Safety & Engine-Invariant Stability

How the engine stays in a consistent state when an exception is thrown, and the rules to keep it that way. The concern this doc answers: an exception thrown *partway through* a multi-step state mutation (entity create/register/destroy/invalidate, cross-entity links, persistence) must never leave the running process in a half-mutated state whose only recovery is a restart.

## 1. Out-of-memory is not a recoverable error

Engine memory comes from two paths that **terminate the process on exhaustion instead of throwing**:

- `SafeAlloc::MakeRefCounted` / `MakeRaw` / `MakeUnique` / `MakeRawArr` (`Source/Essentials/MemorySystem.h`): nothrow `new`, then drain a fixed backup pool and retry, then `ReportAndExit`. Every entity (`Item`/`Critter`/`Map`/`Location`/`CustomEntity`/…) is allocated this way.
- `SafeAllocator<T>` (`MemorySystem.h`) backs **every engine container alias** — `vector`, `small_vector`, `unordered_map`/`unordered_set`, `map`/`set`, `list`, `deque`, `string`/`stringstream` (`Source/Essentials/Containers.h`). Its `allocate()` is nothrow + backup-pool retry + `ReportAndExit`. So `emplace`, `emplace_back`, `insert`, `resize`, `reserve`, rehash, complex-property storage growth, and string growth on engine containers **cannot throw `std::bad_alloc` — they terminate at the allocation site.**

**Consequence — do not write rollbacks for allocation.** A code path whose only failure mode is an allocation cannot leave a half-mutated world: it either completes or the process dies deterministically at the failing allocation (which is the fail-fast outcome we want). Treat `std::bad_alloc` as "can't happen"; never add `scope_exit`/`scope_fail` undo logic justified solely by a possible container/string/refcount allocation between two mutations.

(The throwing global `operator new` still exists for a bare `new T` / a `std::allocator`; engine code should not rely on it for OOM behavior — prefer `SafeAlloc`/engine containers so OOM terminates rather than propagates.)

**The terminate-on-failure guarantee is memory allocation only — it does *not* extend to other resource acquisition.** Only heap allocation through `SafeAlloc` / the engine containers is "can't fail" (§1). OS-resource acquisition that can fail by exhaustion is a **genuinely recoverable throw** and must be handled, not assumed away. The canonical case is **thread creation**: constructing a `std::thread` can throw `std::system_error` when the OS is out of thread resources — that is *not* the allocation case, so `Threading.cpp`'s `spawn_pool_worker` is **not** `noexcept`, and `submit_impl` rolls back the just-queued task on that throw (otherwise the orphaned task would keep a dangling capture of an unwinding caller). So: only suppress a guard / avoid `ES: None` when the sole throw is a `SafeAlloc` memory allocation; a possible thread-creation, file-open, socket, or other OS-resource failure still needs correct handling.

## 2. What can actually be thrown, and where it is caught

After §1, the exceptions that can propagate through a running server are:

- `VerificationException` from `FO_VERIFY_AND_THROW(cond, …)` / the script `verify(…)` macro.
- Engine exceptions: `EntitySyncException`, `DataBaseException`, `GenericException`, manager exceptions, etc.
- Exceptions raised from an AngelScript callback that runs *directly* in native code (init scripts via `CallInit`), and the post-event re-validation `FO_VERIFY_AND_THROW` checks.

The server runs all gameplay work as jobs on a `WorkerPool`. `WorkerPool::WorkerEntry` (`Source/Server/WorkerPool.cpp`) wraps each job body in `try { job() } catch (const std::exception&) { ReportExceptionAndContinue (log only) } catch (...) { FO_UNKNOWN_EXCEPTION -> terminate }`. So a `std::exception` from a job **does not crash the server** — it is logged, the job's `SyncContext` is released (locks dropped), and the worker proceeds. Whatever half-state existed at the throw point stays live in the world. This is exactly why the rules below matter.

Scripted-event fan-out (`Game.OnX.Fire(...)`, `entity->OnX.Fire(...)`) is **noexcept**: `Entity::FireEvent` swallows per-callback exceptions and converts a throwing/`StopChain` callback into a chain stop. So `OnX.Fire` itself never propagates — but a handler's *side effects* (it may destroy or relocate entities) do persist, and the engine re-validates after firing.

## 3. The entity-lifecycle contract (create / destroy)

Entity lifecycle is deliberately **not** "transactional rollback". The contract, encoded by the tests in `Source/Tests/Test_EntityLifecycle.cpp` and `Source/Tests/Test_ServerMapOperations.cpp`, is:

**Create (`CritterManager::CreateCritterOnMap`, `ItemManager::CreateItem`, `MapManager::CreateLocation`/`CreateMap`, `ServerEngine::CreateCritter`/`LoadCritter`):**
- The entity is allocated and registered first, then placed, then its init script and entry events run.
- A lifecycle event (or init script) may legitimately **destroy** the new entity, **relocate** it (e.g. an `OnMapCritterIn` handler attaches and transfers the critter to another map), or leave it where created.
- The create function **throws as a signal** that the requested operation did not complete nominally — it does **not** roll the entity back. The entity is left in whatever valid state the events produced:
  - event destroyed it → the entity is gone and the registry is back to baseline (`ItemInitEventMayDestroyItem`, `CritterInitEventMayDestroyCritter`, `LocationInitEventMayDestroyLocation`);
  - event relocated it → the entity **survives** at its new location (`MapAddCritterEventMayMoveCritterAwayThrows`, `MapAddCritterInitEventMayMoveCritterAwayThrows` assert the critter is alive on the destination map after the throw);
  - load event destroyed a loaded critter → `LoadCritter` throws and the critter is gone (`CritterLoadEventMayDestroyLoadedCritterThrows`).

  Because "relocated and surviving" is indistinguishable from "leaked" at the throw site, **a blanket create-time rollback is incorrect** — it would destroy entities the design intends to keep. Do not add one.

**Destroy (`DestroyCritter`/`DestroyItem`/`DestroyLocation`/`DestroyMap`/`DestroyCustomEntity`):**
- `MarkAsDestroying()` latches first; a redundant call early-returns.
- `IsDestroying()` / `IsDestroyed()` are cross-worker lifecycle latches backed by acquire/release atomics. This lets a deferred script context observe teardown without racing the player/entity job that publishes it; the entity lock still protects the mutable entity contents.
- The finish event fires, then the environment tear-off runs inside a `for (size_t prev_deps = std::numeric_limits<size_t>::max(); …) { try { … } catch (ReportExceptionAndContinue); /* progress guard */ }` loop so a misbehaving teardown step is retried until the entity is fully detached (or the inline progress guard, §3.1, terminates on non-convergence), and a finish-event handler **cannot take over or block** the destruction (`ItemFinishEventCannotTakeOverItemDestruction`, and the critter/location variants).
- Snapshot iteration sets with `copy_hold_ref(...)` and re-check `IsDestroyed()` after each event before continuing to use a retained reference.

## 3.1 Destruction-loop convergence (why a teardown can't spin forever)

Each `DestroyX` runs a teardown loop `for (size_t prev_deps = std::numeric_limits<size_t>::max(); <holder still has children/links>;) { try { detach steps } catch { ReportExceptionAndContinue } /* inline progress guard, §3.1 */ }`, converging by emptying the holder's child collections / links. Eight such loops exist: `ItemManager::DestroyItem`, `CritterManager::DestroyCritter` + `DestroyInventory`, `MapManager::DestroyMapContent` + `DestroyMapInternal` + the `DestroyLocation` inner-entity loop, and `EntityManager::DestroyInnerEntities` + the `DestroyCustomEntity` inner-entity loop.

A loop can fail to converge for three reasons:

1. **Re-entrant re-add (the realistic cause).** The teardown fires Finish events and recursively destroys children (more events) and transfers critters (more events). A script handler reacting to a child's destruction can *add a new child to the holder being destroyed* — e.g. an `OnItemFinish` handler that puts a fresh item back on the same critter/map/container, or an inner-entity-destroy handler that creates a new inner entity. If it re-adds on every child-destroy, the collection refills as fast as it drains.
2. **Persistent-throw detach step.** A detach step throws on every iteration (caught, logged, retried), so the collection never shrinks — a genuine bug in that step.
3. **No-progress logic bug.** A detach step succeeds but does not reduce the loop condition.

### Exit mechanism

**Primary — block re-add during destruction (eliminates cause 1), echeloned across tiers (§5).** A re-entrant re-add originates from a **script** handler (Finish/destroy events run scripts), so the same "you may not add a child to a destroying entity" rule is enforced at two depths — caught as early as possible, and re-checked deeper as a backstop:

- **Top — `throw` (expected script misuse):** every `FO_SCRIPT_API` add-method (`Server_Map_AddItem` / `Server_Map_AddCritter`, `Server_Critter_AddItem` / `Server_Critter_AttachToCritter`, `Server_Item_AddItem`, `Server_Location_AddMap`) rejects an add to an `IsDestroying` entity with a `ScriptException`.
- **Below — `FO_VERIFY_AND_THROW` (invariant re-check):** the internal mutation methods (`Entity::AddInnerEntity`, `CritterManager::AddItemToCritter`, `Map::AddCritter` / `Map::SetItem`, `Item::SetItemToContainer`, `Critter::AttachToCritter`, `Location::AddMap`) re-check the same `IsDestroying` / `IsDestroyed` invariant in case a path slipped past the top guard.

With these, a destroying holder cannot gain new children, so the teardown loop is monotone. Note: only *adding children* is forbidden — **modifying** a destroying entity (writing its properties) stays allowed, because a Finish/destroy handler legitimately mutates the entity it is cleaning up (and the engine passes the destroying entity into its own handlers).

**Reads on a destroying map are allowed under the lock (owner directive — "option B"; supersedes the earlier read-side strictness for queries).** An earlier version of this doc claimed the map's hex-field grids (`_hexField` / `_staticMap->HexField`) are torn down *before* the destructor, making a spatial read on an `IsDestroying` map a use-after-free — **that was wrong.** `_hexField` is a `unique_ptr` member destroyed only by `~Map` (there is no pre-destructor reset anywhere in teardown), and the shared static `HexField` outlives the instance; so throughout `IsDestroying` the grid *structure* is alive and only its *contents* drain — a hex-keyed read mid-drain returns empty cells, not freed memory. The real protection against the concurrency hazard (the sibling-parallel grid race) is the **lock**: a grid-*mutating* op holds the map exclusively and serializes against readers — not an `IsDestroying` gate, which never addressed concurrency and only ever returned a degraded value or threw at a legitimate under-lock reader (e.g. a deferred map-analysis job hitting `Cannot query a map that is being destroyed`).

So map read methods take **`LOCKED, NOT_DESTROYED`** and do **not** gate on `IsDestroying`: script `Server_Map_Get*` / `Server_Item_GetItems`, and engine `Map::Get*` / `Is*` / `Has*` / `Check*` (the 14 read macros dropped `NOT_DESTROYING`; the 8 `noexcept` hand-written `FO_VERIFY_AND_RETURN_VALUE(!IsDestroying(), …)` read-gates were removed). A consumer that does not want to act on a dying map checks `IsDestroying` / a failed `Sync::Lock` *itself* — that is the consumer's call, not a hard engine gate. The single unified rule: **grow a dying entity → forbidden (`NOT_DESTROYING`, expansion only); read → allowed under the lock (`LOCKED, NOT_DESTROYED`); fully dead → never (`NOT_DESTROYED`); drain → *is* the destruction (allowed).** The `IsDestroyed` (`FO_STRONG_ASSERT`, corrupt) vs `IsDestroying` (`FO_VERIFY_AND_THROW`, recoverable) tiers stay distinct for the methods that still gate (the expansion `Add*` / `SetItem` add-guards).

**Declaring method preconditions: `FO_VALIDATE_ENTITY(<flags>)`.** *(Temporary scaffolding — owner directive 2026-07-11: the macro exists to debug the lock system and is slated for removal once it is validated; per-method ES tags classify bodies as if it were absent, see §8.)* Every server entity method opens by declaring what it requires, via the macro in `Common.h`: `LOCKED` (the caller's sync context covers `this`; the noexcept report-and-exit form), `NOT_DESTROYED` (`FO_STRONG_ASSERT(!IsDestroyed())` — the corrupt-state tier above), `NOT_DESTROYING` (`FO_VERIFY_AND_THROW(!IsDestroying())`; it *throws*, so a `noexcept` method instead takes `LOCKED[, NOT_DESTROYED]` and hand-writes a `FO_VERIFY_AND_RETURN_VALUE(!IsDestroying(), …)`), or `NONE`. This makes "may this be called now?" explicit and greppable per method, and replaces the older `FO_VALIDATE_ENTITY_ACCESS()` / `FO_NO_VALIDATE_ENTITY_ACCESS()` markers. **The `noexcept`⇒no-`NOT_DESTROYING` rule is *not* compiler-enforced:** `FO_VERIFY_AND_THROW` raises through a function call, so MSVC does **not** emit `C4297` for a `NOT_DESTROYING` placed in a `noexcept` body — it builds clean and degrades to a latent `std::terminate`-on-teardown. Enforce it by review (verify the signature has no `noexcept` before adding `NOT_DESTROYING`), not by relying on the build.

**Strictness policy (owner directive 2026-06-29, amended by the option-B read rule above): make every method's `FO_VALIDATE_ENTITY` as strict as it can bear — when in doubt, forbid and relax later.** The default for a non-`noexcept` **mutation** is the maximal `LOCKED, NOT_DESTROYED, NOT_DESTROYING`. A **read/query** instead defaults to `LOCKED, NOT_DESTROYED` only: a read does not grow the entity and the grid/collections it touches are valid throughout the drain, so `NOT_DESTROYING` on a read buys no safety and only rejects a legitimate under-lock reader (option B). `NOT_DESTROYED` stays broadly on both (it tests `IsDestroyed`, which is *false* throughout the drain, so it never breaks teardown while still catching a stale/freed pointer). The hard part is the **exclusions** — *mutation* methods the strict flags would break because a legitimate teardown/destroy path reaches them. Static analysis under-finds these; the **gameplay-test sweep + engine unit tests are the authoritative net** — every exclusion below built clean and was caught only by the behavioral gate. The exclusion classes:

1. **`noexcept` ⇒ never `NOT_DESTROYING`** (it throws; `C4297` does *not* fire, so the build is no net — see above). A `noexcept` method that needs the guard hand-writes `FO_VERIFY_AND_RETURN_VALUE(!IsDestroying(), …)`.
2. **Drain-reached accessors ⇒ no `NOT_DESTROYING`.** Reading an owned collection (inventory, inner items/entities, visibility, `Location` maps) during `IsDestroying` is safe (valid until the destructor) and the destruction drain loops *call exactly those accessors* (`HasItems`, `HasInnerItems`, `GetInvItems`, `GetRawInnerItems`, … — the loop conditions in the convergence backstop below) to empty the entity; guarding `Item::HasInnerItems` with `IsDestroying` makes it report "nothing left" mid-drain and the loop abandons an un-drained entity. Also here: a property push fired *during* the drain (`Critter::Broadcast_Property`). Same at the **script frontier** — the `Server_Location_Get*` map readers must not reject `IsDestroying`, because `OnLocationFinish` reads the location's still-alive maps to detach guard data.
3. **Destroy/transfer-cascade-reached ⇒ no `NOT_DESTROYING`.** `DestroyCritter` → `MapManager::Transfer`/`TransferToGlobal` unhooks a **destroying** critter onto the global map, and the path's guards check `IsDestroyed` *only* — so an `IsDestroying` critter flows through and the cascade calls `StopMoving`, `GetGlobalMapGroup`, `MoveAttachedCritters`, `ChangeDir` on it. These must tolerate `IsDestroying`: the cascade *is* the destroy, so an `IsDestroying` guard there would abort teardown, not protect it.
4. **Event-destroy re-validation ⇒ no `NOT_DESTROYED` either.** A few methods run on an *already-`IsDestroyed`* (but still ref-held) self after an event destroyed it: a `scope_exit{ cr->UnlockMapTransfers() }` that must rebalance the counter the destructor asserts is zero; and the post-event re-validations whose first body statement is `if (IsDestroyed()) return X` (`Map::IsMapItemContextChanged`, `Critter::CanSeeItemOnMap`). For these even `NOT_DESTROYED` is wrong (it asserts before the graceful return) — they take `LOCKED` only, and the `if (IsDestroyed())` early-out *is* the contract.

The **genuine positive grounds** for `NOT_DESTROYING` are unchanged: (a) a reader that dereferences a sub-structure torn down *before* the destructor — the map *grid* (`_hexField` / `_staticMap->HexField`) hex-keyed queries; and (b) an add-to-holder method (`AddItem`, `AddCritter`, `AddMap`, `SetItemToContainer`, …) as the "can't add a child to a dying holder" invariant (§3). Everything else is strict-by-default minus the four exclusion classes.

**Backstop — terminate on genuine non-convergence (causes 2 and 3).** Each teardown loop carries a local `size_t prev_deps` (seeded to `std::numeric_limits<size_t>::max()`) and, at the end of every pass, recomputes the entity's *current* remaining-dependency count (e.g. `cr->GetInvItems().size() + cr->GetInnerEntitiesCount() + …`) and asserts a **strict monotonic decrease**:

```cpp
for (size_t prev_deps = std::numeric_limits<size_t>::max(); cr->HasItems() || cr->HasInnerEntities() || …;) {
    try { /* tear off one layer */ } catch (const std::exception& ex) { ReportExceptionAndContinue(ex); }

    const size_t remaining_deps = cr->GetInvItems().size() + cr->GetInnerEntitiesCount() + …;
    FO_STRONG_ASSERT(remaining_deps < prev_deps, "Critter destruction made no progress", cr->GetId(), remaining_deps, prev_deps);
    prev_deps = remaining_deps;
}
```

If a pass does not reduce the count (the loop stalled, or a re-add grew it), `FO_STRONG_ASSERT` terminates. The loop's boolean condition is exactly equivalent to `remaining_deps != 0`, so a real drain ends the loop *before* the next check — a slow-but-progressing teardown is never falsely killed; only a stall is. This is written inline at each destruction loop (no shared helper class) so the dependency count is defined right next to the loop it guards. The earlier `throw`-on-overflow escaped to the WorkerPool catch and left a half-destroyed, un-finalized "undead" entity in the registry until restart; terminating surfaces the underlying bug deterministically. (Every such progress-guarded loop is a destruction loop, where non-progress is always corruption.)

The reaction is `FO_STRONG_ASSERT` (terminate), not stop-and-throw (`SetDestroying(false)` + throw): by the time the loop spins, the entity's `Finish` event has already fired, so a "recovered" entity would be alive-but-finalized — a worse corruption than a deterministic crash on the real bug.

Net guarantee: a destruction either completes, or the process terminates deterministically on a real bug — it never silently leaves an undead entity.

## 4. Post-mutation invariants → `FO_STRONG_ASSERT`

This is the **unexpected-and-unhandled** tier (§5). A verify placed **after** an irreversible mutation that checks an invariant which "can only be false if the world is already corrupt" is `FO_STRONG_ASSERT` (deterministic `ReportExceptionAndExit`), **not** a throwable verify: there is nothing left to handle, and a throwable verify would just be caught by the WorkerPool and keep running on corrupt state. (The same invariant may still be re-checked *earlier*, where it is recoverable, as a `throw` or `FO_VERIFY_AND_THROW` — that is the echeloned model in §5, not a contradiction.)

Such invariants must abort in **every** build. `FO_STRONG_ASSERT` (`Source/Essentials/ExceptionHandling.h`) is **unconditional** — it always evaluates its condition and calls `ReportExceptionAndExit` on failure, regardless of build profile — so it is the correct tool for load-bearing post-mutation invariants.

Examples converted to `FO_STRONG_ASSERT` (all post-mutation, can't-happen-with-correct-code):
- `EntityManager` typed-registry duplicate checks and `Unregister*`/global lookup checks (forward registration implies the typed/global maps agree).
- `Critter` visibility-graph symmetry (`AddVisibleCritter` reverse insert, `RemoveVisibleCritter`/`ClearVisibleEnitites` reverse lookups: a forward link implies its reverse).
- `EntitySync` post-grant lock invariants (`EntityLock::Acquire`: a non-aborted waiter must wake `GRANTED` and own the lock).
- `AngelScriptContextManager::ResumeSpecificContext` pre-resume invariants (a suspended context must have its extended data / scheduler).

The legitimate **non**-invariant throws are left as `FO_VERIFY_AND_THROW`: e.g. `EntityManager::RegisterEntity`'s global-registry insert (a duplicate id from a corrupt persisted record is handled gracefully on the load path via `is_error`), and `EntityLock::Acquire`'s `EntityLockWaitAbortedException` (the legitimate shutdown-abort path).

## 5. Error tiers & choosing the disposition

Three tiers, classified by whether an error is *expected* and whether it is *handled*. **All three stay active in every build, including release** — we accept that our own bugs can surface only under real, loaded production conditions, beyond what development and testing catch, so we never compile out the lower tiers.

| Tier | When | Mechanism |
|------|------|-----------|
| **Expected error** | A condition we anticipate and design around — bad script arguments, malformed/untrusted input, a target legitimately full/absent/forbidden. *Not* an invariant violation. | **`throw`** a domain exception (`ScriptException`, `DataBaseException`, …). Caught upstream and turned into a normal failure result; no side effect when it fires before any mutation. |
| **Unexpected but handled** | An **invariant violation** — "can't happen if the code is correct" — that we nonetheless re-check and catch rather than trust blindly. **Assert semantics, not expectation.** | The **`FO_VERIFY_*` family** (`FO_VERIFY_AND_THROW`, `FO_VERIFY_AND_CONTINUE`, `FO_VERIFY_AND_RETURN`, `FO_VERIFY_AND_RETURN_VALUE`). All **report** the violation (log + stack trace) and are **never compiled out**; the variant only chooses the post-report control flow — see "Picking the variant" below. |
| **Unexpected and unhandled** | An invariant violation reached where it is **too late to recover** — continuing would run on already-corrupt state and no caller can sensibly handle it. | **`FO_STRONG_ASSERT`** — deterministic `ReportExceptionAndExit` (terminate). Also always-on. |

**Picking the `FO_VERIFY_*` variant.** The tier is the same for all of them — an invariant violation, always reported — only the *continuation after the report* differs, and it is dictated by the **control-flow context**, not preference. In every case you get the report; what matters is how execution proceeds:

- **`FO_VERIFY_AND_THROW`** — throws `VerificationException`. Use **only where an exception may legally propagate** and be caught by some upper layer (a normal exception-permitting flow: a WorkerPool job, a script-call boundary, a destroy loop's `try`). The current unit of work unwinds.
- **`FO_VERIFY_AND_CONTINUE`** — reports and **keeps executing** at the same point. Use in a **`noexcept` context** (a destructor, a `scope_exit`/`scope_fail` body, a `noexcept` callback, any function on a `noexcept` path — throwing there would `std::terminate`), or in a loop where you want to skip a bad element and keep iterating.
- **`FO_VERIFY_AND_RETURN`** — reports and **returns from a `void` function**. Use in a `noexcept`/no-throw `void` function that must bail out cleanly.
- **`FO_VERIFY_AND_RETURN_VALUE(expr, fallback, …)`** — reports and **returns a safe fallback**. Use in a `noexcept`/no-throw value-returning function (e.g. a getter that must yield *something* defined).

Rule of thumb: if you are (or might be) inside a `noexcept` region you **cannot** use `FO_VERIFY_AND_THROW` — pick `CONTINUE` / `RETURN` / `RETURN_VALUE` so the report is emitted *and* the caller is left in a defined state. (`FO_STRONG_ASSERT` is also valid from a `noexcept` region, when the right response is to terminate rather than continue.)

**Echeloned (defense-in-depth) detection.** The *same* underlying error may be caught by more than one tier, at different depths — and that is **intentional**: the path from a high-level entry point to the low-level mutation varies, so we catch it as early as possible *without* removing the deeper backstops. Catching an error earlier is always better than later, and there can never be "too much" error protection — so the tiers are **additive, not either/or**. Canonical example, "a script adds a child to an entity that is being destroyed":

1. **`throw` at the top** — the `FO_SCRIPT_API` add-method rejects with `ScriptException` (expected script misuse, caught early).
2. **`FO_VERIFY_AND_THROW` below** — the internal mutation method (`CritterManager::AddItemToCritter`, `Map::AddCritter`, `Entity::AddInnerEntity`, …) re-checks the same `IsDestroying` invariant, in case a call slipped past the top guard.
3. **`FO_STRONG_ASSERT` at the bottom** — if a re-add nonetheless stalled a teardown loop (a pass that removes no dependency), the loop's inline strict-monotonic-decrease check (§3.1) terminates (too late to undo).

Per-situation guidance:

- **Expected runtime failure** (bad args/input, target full, forbidden call) → **`throw`** the domain exception (e.g. `DbStorage.Insert`'s empty-document guard, the `NetOutBuffer` outgoing-message verifies, the `FO_SCRIPT_API` argument and `IsDestroying` checks). Never `FO_STRONG_ASSERT` — it has no side effect yet and crashing all players for a recoverable mistake (or one connection) is wrong.
- **Validate top-level (script / RPC / client-writable-property) arguments at the boundary, before the value can reach a deep `numeric_cast` or a low-level `FO_VERIFY_*`.** A `FO_SCRIPT_API` method's args and a `///@ RemoteCall` payload are *script/client-controlled* (a tampered client can send anything an `int`/`ipos`/`mpos` can hold). Range-check them in the export method and `throw ScriptException` (range, bounds, non-negative) so the failure surfaces at the right altitude with a clear message — rather than a deep `numeric_cast<unsigned/smaller>` throwing an opaque `OverflowException`/`UnderflowException`, or a `std::string::resize`/container op throwing `length_error`, far from the call site. The deep cast/verify stays as the echeloned backstop. (Audited 2026-06-16: no script-reachable `FO_STRONG_ASSERT` exists — the engine never *terminates* on bad script input — but several args reached deep wrong-tier throws; those boundary guards were added. The property get/set boundary was confirmed fully guarded.)
- **Invariant re-check you can still reject** → **`FO_VERIFY_AND_THROW`** (the internal add-to-destroying guards; any "this should already hold" check *before* a mutation that a caller can abort).
- **Invariant reached too late to recover** → **`FO_STRONG_ASSERT`** (post-mutation registry/graph/lock corruption; non-convergent teardown).
- **Throw-as-signal (no rollback)** — the operation may legitimately not complete because an event/script changed the world; throw to inform the caller and leave the entity in the valid state the events produced (the lifecycle contract, §3).
- **Commit-or-rollback (`scope_fail`/`scope_exit`)** — only when a step that genuinely can fail at runtime would otherwise leave **two representations out of sync** with no design intent to keep the partial state, and there is no allocation-only excuse (§1). The rollback body must be `noexcept` (`safe_call`). Confirm with a failing test first.
- **Reorder (validate-first, mutate-last)** — do all fallible/validating work before the first irreversible mutation; insert into the authoritative store (with its duplicate guard) before populating any derived cache. (Example: `ProtoManager::AddProto`.)

Persistence note: DB writes (`DbStorage.Insert/Update/Delete`) are **enqueue-only** and never perform synchronous backend I/O; a backend failure is handled asynchronously (recovery op-log + reconnect + panic-shutdown with replay on restart). So a DB write cannot throw a backend error mid-mutation — no write-through rollback is needed. See `Source/Server/DataBase.cpp`.

## 6. Primitives

- `scope_exit` / `scope_fail` / `scope_success` (`Source/Essentials/BasicCore.h`): RAII guards. `scope_fail` runs only on stack unwinding (an in-flight exception) and static-asserts its callback is `noexcept`.
- `safe_call(callable, …)` (`Source/Essentials/CommonHelpers.h`): invoke a callable, swallowing any exception — use to make a rollback/teardown body `noexcept`.
- `copy_hold_ref(container)`: snapshot a server-entity collection by ref-count so elements survive a loop that fires re-entrant events.
- Inline teardown-progress guard (no shared class): seed `size_t prev_deps = std::numeric_limits<size_t>::max()`, and at the end of each destruction-loop pass `FO_STRONG_ASSERT(remaining_deps < prev_deps, …)` then `prev_deps = remaining_deps`. Detects non-convergence by *progress*, not by an iteration cap, so a slow-but-progressing teardown is never falsely killed (§3.1).

## 7. Tests

The contracts above are pinned by `Source/Tests/Test_EntityLifecycle.cpp` (init/finish-event destruction, load/unload events, registry collections) and `Source/Tests/Test_ServerMapOperations.cpp` (the full map critter in/out/init × may-destroy / may-move-away / may-destroy-map matrix). When changing any lifecycle or invariant behavior, run `LF_UnitTests` and extend these suites rather than weakening an assertion.

## 8. Per-function exception-safety classification (ES levels)

Every function/method definition in `Source/**/*.cpp` (excluding `Source/Tests/` and the codegen `*.template.cpp` inputs) has a classified exception-safety level stating what its body **currently guarantees** — documentation of fact, not aspiration.

| Level | Guarantee |
|-------|-----------|
| `NoThrow` | No exception propagates to the caller: `noexcept` signature, a catch-all that swallows, or a body that provably cannot throw. Remember the engine model: `SafeAlloc`/engine-container allocation **terminates** on OOM (§1), `FO_STRONG_ASSERT`/`ReportAndExit` **terminate**, event `Fire()` is noexcept, and `FO_VERIFY_AND_CONTINUE`/`RETURN`/`RETURN_VALUE` report without throwing — none of those demote a method from NoThrow. |
| `Strong` | May throw, but on throw the observable engine state is exactly as before the call — validate-first bodies (all throws precede the first observable mutation), read-only queries, or genuinely rolled-back mutations (`scope_fail`). |
| `Basic` | May throw after an observable mutation, but every invariant holds and every touched entity/object stays valid and usable. The §3 entity-lifecycle throw-as-signal contract is *by design* this level: the throw reports "did not complete nominally" while the world remains valid. |
| `None (<reason>)` | A throw can escape with a broken invariant / half-mutated state (two representations out of sync, a partially rebuilt structure, an orphaned link). Always carries a short reason naming the broken invariant. Per the §5 policy this is a **defect candidate to triage**, not an accepted level. |

The classification is **not** kept in the source. It lives in an external per-function baseline maintained by the embedding project's audit tool (Last Frontier: `Tools/ExceptionSafetyAudit/` — one row per function with level, verification status, and a comment-insensitive body hash; the audit fails when a new or changed function lacks a valid classification and warns on entries that need manual review). This section is the engine-side contract an auditor — human or agent — uses to **derive** a level; when a change alters what a body guarantees, re-derive its level and update the baseline in the same change.

Derivation, in order: a `noexcept` definition, a catch-all-swallowing body, or a body with no real throw points is `NoThrow`; when throw points exist but all fire before the first observable mutation (or the mutations are rolled back), it is `Strong`; when a throw can escape after a mutation with every invariant intact — including the §3 throw-as-signal lifecycle paths — it is `Basic`; only when a concrete broken invariant can be named is it `None`. Judge each function by its own body given its callees' behavior: a throwing callee is a throw point at the call site. Lambdas, `= default`/`= delete`, declarations, header-inline bodies, and test code are not classified.

**`FO_VALIDATE_ENTITY(...)` is ignored for ES classification (owner directive 2026-07-11).** The precondition macro (§3.1) is *temporary* lock-system-debugging scaffolding, slated for removal once the lock system is validated. ES levels document the method's *permanent* body semantics, so classify **as if the macro were absent**: its `LOCKED` throw (`ValidateEntityAccess` → `ScriptException`) and its `NOT_DESTROYING` `FO_VERIFY_AND_THROW` are **not** counted as throw points, and its `NOT_DESTROYED` `FO_STRONG_ASSERT` terminates (never a throw point anyway). A read-only method whose only potential throw comes from `FO_VALIDATE_ENTITY` is therefore `NoThrow`. Hand-written guards in the body (`FO_VERIFY_AND_THROW(!IsDestroying())`, `vec_add_unique_value`'s duplicate verify, explicit `throw`) are permanent code and always count.
