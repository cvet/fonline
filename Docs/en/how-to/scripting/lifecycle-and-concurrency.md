---
layout: default
title: Script Lifecycle and Concurrency
locale: en
document_id: script-lifecycle-concurrency
permalink: /Docs/en/how-to/scripting/lifecycle-and-concurrency.html
---

# Script Lifecycle And Concurrency

> Engine-owned documentation. This guide describes reusable AngelScript lifecycle and concurrency behavior. Project modules, gameplay policies, and project-specific synchronization helpers belong to the embedding game.

## Purpose

Use this guide when a script needs to initialize a module, subscribe a callback, wait with `Yield`, mutate server entities, own runtime state, or shut down cleanly. It answers four questions that should be explicit before writing the code:

1. Who invokes this function, and during which runtime phase?
2. Can this call chain suspend or resume on another server worker?
3. Which object owns the mutable state and its lifetime?
4. Which entity synchronization cover is valid at this exact point?

Keep examples inside the same contract. When a documented lookup can return no
entity, store it as `T?` and narrow it before use. Call `Yield` only from a
transitively `[[Async]]` chain, then re-resolve, narrow, and reacquire cover
after resumption. If the exact lookup or callback signature is not present in
the owning reference, explain the lifecycle without inventing shorthand code.
Native entry cover is dispatcher-specific: the inbound remote-call rule below
does not prove that another event, setter, callback, or direct script entry
starts empty or carries the same cover. Inspect its owning native dispatcher
before stating an initial cover.

Use this decision for every server entry point that accesses an entity:

`ServerEngine::RunScriptContext()` creates the nested `SyncContext` that can
hold an entity cover; it does not itself cover any entity. The owning native
dispatcher may establish an initial entity cover inside that context.

1. Rely on an initial synchronization cover only when that entry point's
   owning native dispatcher proves the exact covered set.
2. If the required entity is outside that proven set, or the initial set is
   not documented, call `Game.Sync(...)` with the complete required set before
   reading or mutating it. Do not fill the evidence gap by saying an unrelated
   event, setter, callback, or direct entry starts empty.
3. After `Yield`, reacquire the complete cover before entity access; a cover
   from the previous execution does not survive suspension.

Do not paraphrase this as "events, setters, or callbacks create no additional
sync scope." That is the same unsupported cross-dispatcher generalization in a
different form. State only the proven dispatcher cover and the explicit
`Game.Sync(...)` cover used by the script.

Read it together with:

- [Scripting](../../explanation/scripting-runtime/) for the complete scripting subsystem and native binding path.
- [Entity Model](../../explanation/entity-and-property-model/) for entity, property, holder, and destruction ownership.
- [Server Runtime](../../explanation/runtime/server.md) and [Client Runtime](../../explanation/runtime/client.md) for side-specific loops and managers.
- [Remote Calls](../../reference/scripting/remote-calls.md) for network entry points and authority boundaries.
- [Nullability.md](../../../Nullability.md) for handles that can disappear before a continuation resumes.
- [generated API reference](../../../generated/api/index.md) for current method signatures, attributes, settings, and source links.

## Source paths inspected

- `Source/Common/ScriptSystem.h`
- `Source/Common/ScriptSystem.cpp`
- `Source/Common/EntityProperties.h`
- `Source/Common/Entity.h`
- `Source/Common/Entity.cpp`
- `Source/Client/Client.cpp`
- `Source/Server/EntityManager.cpp`
- `Source/Server/Server.cpp`
- `Source/Server/WorkerPool.cpp`
- `Source/Tools/Baker.cpp`
- `Source/Server/EntitySync.h`
- `Source/Server/EntitySync.cpp`
- `Source/Scripting/ServerGlobalScriptMethods.cpp`
- `Source/Scripting/ServerCritterScriptMethods.cpp`
- `Source/Scripting/ServerItemScriptMethods.cpp`
- `Source/Scripting/ServerLocationScriptMethods.cpp`
- `Source/Scripting/ServerMapScriptMethods.cpp`
- `Source/Scripting/AngelScript/AngelScriptAttributes.cpp`
- `Source/Scripting/AngelScript/AngelScriptBackend.cpp`
- `Source/Scripting/AngelScript/AngelScriptCall.cpp`
- `Source/Scripting/AngelScript/AngelScriptContext.cpp`
- `Source/Scripting/AngelScript/AngelScriptEntity.cpp`
- `Source/Scripting/AngelScript/AngelScriptGlobals.cpp`
- `Source/Scripting/AngelScript/AngelScriptRemoteCalls.cpp`
- `Source/Scripting/AngelScript/CoreScripts/Input.fos`
- `ThirdParty/AngelScript/sdk/angelscript/source/as_compiler.cpp`
- `ThirdParty/AngelScript/sdk/angelscript/source/as_scriptengine.cpp`
- `Source/Tests/Test_AngelScriptCall.cpp`
- `Source/Tests/Test_AngelScriptAttributes.cpp`
- `Source/Tests/Test_AngelScriptBaker.cpp`
- `Source/Tests/Test_EntityLifecycle.cpp`
- `Source/Tests/Test_EntitySync.cpp`
- `Source/Tests/Test_ServerMapOperations.cpp`
- `Source/Tests/Test_ScriptEntityOps.cpp`
- `Source/Tests/Test_ScriptBuiltins.cpp`

## Runtime mental model

Script execution is a sequence of bounded entries, not one serialized game-wide thread:

| Phase | Owner | Important boundary |
|---|---|---|
| Compile and bake | AngelScript compiler and bakers | Attributes, callback usage, nullable handles, and mutable globals are validated before runtime. |
| Module initialization | `ScriptSystem::InitModules()` | Init functions run in ascending priority while global assignment is temporarily enabled. |
| Entity initialization | `EntityManager::CallInit()` | The entity is marked initialized, its `On*Init` event fires, then its optional persisted `InitScript` callback runs. |
| Callback dispatch | Client loop or server worker job | Events, time events, remote calls, and native re-entry invoke attributed functions through their owning API. |
| Suspension | AngelScript context manager | `Yield` preserves the script continuation, but the current native execution scope returns. |
| Resumption | Client scheduled-callback pass or server worker pool | The continuation runs later; on the server it may run on another worker under a fresh synchronization context. |
| Entity destruction | Entity/manager owner | Event callbacks and time-event storage are cleared; server dispatch jobs are cancelled by the owning manager. |
| Runtime shutdown | Client/server and scripting backend | Global events/time events, entities, script globals, contexts, and the backend are drained in owner-defined order. |

This model has two practical consequences:

- a live script handle is not proof that its entity is still valid or synchronized;
- code after any suspension point is a new observation of mutable world state, not a continuation of an atomic transaction.

## Module initialization

Declare a global, no-argument, `void` function with `[[ModuleInit]]`. The attribute accepts an optional signed integer priority:

```angelscript
[[ModuleInit(100)]]
void InitializeInventoryRules()
{
    Game.OnSomeEvent.Subscribe(OnSomeEvent);
}
```

The event name above is illustrative; use a generated event from the current API reference.

The runtime indexes valid init functions, stores their priorities, and uses a stable ascending sort. `[[ModuleInit]]` without an argument has priority `0`. Lower values run first. Equal priorities retain registration order, but cross-module dependencies should still use explicit priorities rather than relying on file or bytecode discovery order.

`ScriptSystem::InitModules()` performs this sequence:

1. unfreeze script globals;
2. invoke every registered init function;
3. abort startup if any invocation fails;
4. freeze globals after all functions complete.

`Game.SetConstGlobalVar(...)` is therefore an initialization-only operation. It throws after the freeze boundary. The baker also rejects mutable module-level globals unless their namespace is explicitly allowed by `Script.MutableGlobalsAllowedNamespaces`.

Prefer module init for registration and immutable lookup construction. Do not start gameplay work that assumes the world, current player, map, or network session already exists. The client fires `Game.OnStart` after module initialization; the server fires its initialization lifecycle events after `InitModules()` in the server startup sequence.

## Events and callback ownership

An event handler must carry `[[Event]]` and must be passed through `Subscribe` or `Unsubscribe`. Direct calls to event handlers are blocked by attribute validation. The same ownership rule applies to other callback attributes:

- `[[TimeEvent]]` functions belong to `StartTimeEvent`, `StopTimeEvent`, `CountTimeEvent`, `RepeatTimeEvent`, and `SetTimeEventData` APIs;
- remote-call handlers belong to the remote-call dispatcher described in [Remote Calls](../../reference/scripting/remote-calls.md);
- animation and property callbacks belong to their corresponding registration APIs.

This separation makes invocation context visible. Put reusable logic in an ordinary helper and let the attributed callback adapt event arguments to it.

```angelscript
[[Event]]
void OnSomeEvent(Critter critter)
{
    ApplyImmediateRule(critter);
}

void ApplyImmediateRule(Critter critter)
{
    // Ordinary logic that may also be called from another ordinary function.
}
```

Subscriptions are stored on the entity that owns the event. `Entity::MarkAsDestroyed()` clears all of that entity's event callbacks and time-event records before marking it destroyed. Server managers additionally cancel scheduled time-event jobs before final entity destruction.

Explicitly unsubscribe when behavior must stop before the owner is destroyed, when replacing a callback, or when a long-lived global owner should release a project object. Do not maintain a second project-wide registry solely to unsubscribe ordinary entity callbacks during destruction; that duplicates engine lifetime ownership and creates another source of stale handles.

Event dispatch may run user callbacks that alter subscriptions, so the engine iterates a callback snapshot. Do not rely on a subscription added during a dispatch being called in that same dispatch.

### Persistence preload boundary

`Game.OnCritterPreLoad` is the server-side migration hook for an existing persisted critter. It fires once after the critter properties, inventory, and inner entities have been restored and the critter has been registered, but before map or global-map entry, visibility processing, `OnCritterInit(critter, false)`, and `OnCritterLoad`. Newly created critters do not receive this event.

The callback runs with map transfers locked. Limit it to normalizing the critter's own persisted properties and inventory: the rest of the world may still be only partially restored, and resolving, loading, or relocating other persisted entities is not supported at this phase. A handler may explicitly destroy the critter to drop a stale persisted graph. Throwing or stopping the event chain instead marks the load as failed and leaves the persisted record available for diagnosis. See [Server Runtime](../../explanation/runtime/server.md) for the full load order and player-bound behavior.

## Entity `InitScript` callbacks

`Item`, `Critter`, `Map`, and `Location` have a server-side persistent `InitScript` property. It names a global callback with the corresponding entity plus `bool firstTime`; authored values are signature-checked during server baking. The exact signatures and prototype-authoring rules are in [Prototype Format](../content/prototype-format.md#init-scripts).

For normal entity initialization, `EntityManager::CallInit()`:

1. validates the entity and returns if initialization already ran;
2. holds the entity alive and sets its initialized flag;
3. fires the matching `Game.OnItemInit`, `OnCritterInit`, `OnMapInit`, or `OnLocationInit` event;
4. invokes the named `InitScript` only if the event did not destroy the entity;
5. recursively initializes owned children that remain alive.

`firstTime` is `true` for newly created entities and `false` for restored world state. A newly created location initializes its child maps before the location callback, while world loading begins at the location and cascades down through maps, critters, and items. Do not encode cross-entity ordering assumptions in an init callback.

An unresolved name or mismatched signature throws `ScriptException`. Because the initialized flag and event dispatch precede resolution, this is a fail-loud `Basic` lifecycle guarantee, not transactional rollback. A script-body exception follows a different path: `ScriptFunc::Call()` is `noexcept`, reports the exception, and returns `false`. Normal `CallInit()` treats that as already reported; runtime `SetupScript` / `SetupScriptEx` convert it to `ScriptException("Call init failed", ...)`.

`SetupScript(typedFunction)` and `SetupScriptEx(name)` run the callback immediately with `firstTime = true`, then persist the function name only after success. The typed overload rejects delegates because a persisted callback must be globally resolvable. An empty property means no entity-specific callback; projects may use the corresponding global `Game.On*Init` event when one subscriber should own behavior across many prototypes.

The callback enters with its entity covered. Touching another entity requires the normal complete cover or a checked widening operation. The callback is not implicitly async, and no cover survives a `Yield`.

## Async propagation and `Yield`

`Yield(int durationMs)` is registered with the `Async` function attribute. `Async` is a transitive marker: every script function that directly calls an `[[Async]]` function must also carry `[[Async]]`. A callback may carry both its invocation attribute and the marker:

```angelscript
[[TimeEvent]] [[Async]]
void RefreshLater(Critter critter)
{
    Yield(10);
    // This is a later observation. Revalidate before accessing mutable state.
}
```

When `Yield` runs, AngelScript suspends the active context and schedules `ResumeSpecificContext()` for the requested game-frame time. The script stack and local handles remain in that context. The native call that was executing the context returns as suspended.

The two runtime sides resume differently:

- **Client:** delayed callbacks are processed from a snapshot of callbacks that were already due at the start of the current main-loop pass. `Yield(0)` therefore resumes on the next pass, after the loop can process network and input work.
- **Server:** delayed callbacks are submitted to the worker pool. Every worker job has a synchronization context, and every script execution creates a nested synchronization context. A continuation may resume on a different worker thread.

The context manager prevents two workers from executing the same suspended AngelScript context concurrently. This protects the continuation object itself; it does not serialize unrelated callbacks or make project state thread-safe.

### The suspension rule

Treat every `Yield` and every helper that can suspend as a transaction boundary:

1. Finish or abandon the current mutation before suspending.
2. Do not retain a `Game.Lock` expectation across suspension.
3. After resumption, reacquire the required entity cover.
4. Resolve or revalidate entities that may have been destroyed, detached, or reparented.
5. Re-read properties, collections, parent links, and other mutable decisions before writing.

Identifiers are often safer continuation state than an assumed-valid entity snapshot. Resolving an identifier after resumption still requires null/destroyed handling and synchronization before access.

## Server entity synchronization

Server callbacks can run concurrently. The engine validates authoritative entity access against the current thread's `SyncContext`; an uncovered read or write is a contract error, not a benign race to ignore.

`Game.Sync(...)` is the script-visible acquisition boundary:

- it replaces the current entity cover with the supplied non-null entities and engine-defined auto-widen partners;
- overloads accept one, two, three, or an array of entities;
- the array overload rejects null entries;
- a later `Game.Sync(...)` does not extend the previous cover, so request the complete set needed by the next operation;
- `Game.GetHeldSyncEntities()` reports the current entity-cover owners for diagnostics;
- `Game.IsEntityLocked(entity)` probes coverage without emitting the uncovered-access diagnostic;
- `Game.SyncRelease()` releases both the entity cover and any singleton lock entries held by the current script execution scope.

`Game.Sync(...)` carries `[[Async]]`, so the marker propagates through its direct script callers even when acquisition completes without suspending the AngelScript context.

### Native entry covers

Every server-side AngelScript execution enters through `ServerEngine::RunScriptContext()` and receives its own nested `SyncContext`; an event, setter, or remote-call dispatcher does not create another script scope around the handler. Native entry points may establish the initial cover inside that active context before dispatch.

For an inbound server remote call, `Process_RemoteCall()` syncs its `Player` argument immediately before invoking the script handler. Entity widening includes the player's currently controlled `Critter` and the reverse critter-to-player link, so the handler may read that linked pair without an additional `Game.Sync(...)`. Independently resolved entities still require an explicit complete cover. A later `Game.Sync(...)` replaces the entry cover, so include the player or controlled critter again when the following operation still needs them.

### Existing-player reconnect

`Game.LoginPlayerToExistentRecord(unloginedPlayer, playerId)` preserves the caller's current cover. For a live reconnect, the caller must acquire the complete graph before calling it:

- the incoming unlogined `Player`;
- the existing live `Player`;
- its controlled `Critter`, when present;
- the current `Map` and `Location` when the critter is mapped;
- every current global-map group member when the critter is on the global map.

The Player/Critter auto-widen pair does not cover the critter's parent map or the map's parent location. Likewise, covering the group leader does not cover sibling global-map members. The complete graph is needed because reconnect dispatches `OnPlayerLogin` and sends critter initial information without narrowing the caller's cover; local-map initial info reads the map/location and global-map initial info serializes the group members.

A typical project flow first resolves the optional live player with `Game.GetPlayer(playerId)`, synchronizes the incoming and live players, discovers the covered controlled critter, then acquires and revalidates the relevant map/location or global-group graph. Because every later `Game.Sync(...)` replaces the previous set, all required entities must be present in the final request. The helper and every direct caller must carry `[[Async]]`.

Offline stored-record login has no existing runtime player to add. It enters with the unlogined player covered and builds the restored character graph through the normal project `OnPlayerLogin` bootstrap.

### `Game.Lock` is separate

`Game.Lock()` acquires the global `Game` singleton lock in a separate, recursive bucket. Pair it with `Game.Unlock()` and keep the critical section small. The entity cover is unchanged by `Game.Unlock()`.

Do not call `Game.Sync(...)` while `Game.Lock()` is held. The engine rejects that order to prevent a singleton/entity lock cycle. `Game.SyncRelease()` drains both buckets, including unmatched recursive singleton entries, but that teardown behavior is a safety boundary rather than a recommended substitute for balanced `Lock`/`Unlock` calls.

### Covers do not survive suspension

`ServerEngine::RunScriptContext()` creates a nested `ScopedSyncContext` around each `ctx->Execute()` call. The scoped destructor releases all entity and singleton locks when `ctx->Execute()` returns, including when it returns because of `Yield`. Resumption calls `ctx->Execute()` again under a new nested context.

Therefore this is invalid reasoning:

```text
Game.Sync(entity) -> read state -> Yield(...) -> write using the old cover and decision
```

The correct shape is:

```text
resolve entity -> Game.Sync(entity) -> read/mutate -> Yield(...)
resolve or revalidate entity -> Game.Sync(entity) -> re-read -> decide/mutate
```

Native extension code has an additional `EnsureEntitySynced(...)` expansion helper for engine-owned operations that pull a covered descendant or freshly created entity into the current context. It is not a replacement for the script-visible `Game.Sync(...)` contract and must not be exposed as a project workaround for an incorrectly scoped operation.

## Mutable state ownership

Choose the narrowest owner whose lifetime matches the state:

| State | Preferred owner |
|---|---|
| Per entity, persisted or replicated | A declared entity property with the correct persistence/sync flags. |
| Per entity, runtime-only | A non-persistent entity property or engine/project component owned by that entity. |
| Per engine instance | A manager or component reachable from `ServerEngine`, `ClientEngine`, or another explicit owner. |
| Immutable module data | A `const` global, initialized directly or through `Game.SetConstGlobalVar` during module init. |
| Short continuation state | Local values in an `[[Async]]` function, with mutable world data revalidated after suspension. |

Avoid mutable script-global dictionaries keyed by entity id for ordinary entity state. They separate data from its lifetime, can leak across id reuse or tests, require independent cleanup, and become a shared concurrency boundary. If state belongs to an entity, storing it on that entity lets synchronization, destruction, persistence, and replication rules remain explicit.

An allowlisted mutable-global namespace is an escape hatch for a reviewed subsystem, not the default architecture. Its owner must define synchronization, reset behavior, multi-instance isolation, and shutdown cleanup.

## Destruction and shutdown

Entity destruction and runtime shutdown are related but distinct:

- `Entity::MarkAsDestroyed()` clears entity callbacks and time-event records.
- Server entity managers cancel dispatcher jobs before final destruction.
- Client and server shutdown clear global events/time events and destroy owned entities in an ordered runtime sequence.
- `AngelScriptBackend` destroys its context manager, then calls `asIScriptEngine::ShutDownAndRelease()` while modules, types, behaviours, and backend links are still available. The patched AngelScript shutdown calls module exits, releases globals, runs full garbage-collection passes until the live set is empty or stable, discards modules, repeats collection, and reports unreclaimable survivors before the backend links are reset.

The garbage-collection stop condition is **empty or stable**, not always empty. A stable live set can contain unreclaimable survivors, which shutdown reports for diagnosis before continuing its ordered teardown.

These bullets do not define one total order that interleaves client/server entity cleanup with the backend-internal module and garbage-collection stages. Follow each owning shutdown sequence independently unless the runtime source establishes a cross-owner ordering edge.

Do not use `Game.OnFinish` only to reproduce those owner actions. Use it for functional project teardown such as flushing a project service, ending an external session, or cancelling work owned outside normal entity lifetime.

Script expression temporaries, returned object handles, delegates, arrays, and dictionaries are runtime-owned according to their registered AngelScript behaviours. Unsafe-reference expressions may defer receiver and argument cleanup until the expression reaches a safe point; native/script call bridges retain the copied result and release replaced or exceptional-path objects. Project scripts must not add manual reference-count or shutdown registries to compensate for those internals. A surviving object graph indicates a missing owner release or GC enumeration contract in the owning type.

A suspended continuation can outlive the gameplay assumptions under which it started. On resumption, a destroyed handle must be treated as invalid even if the local variable is still non-null at the script-language level; the normal entity access guard is expected to reject destroyed access. Prefer explicit resolution and nullable narrowing where destruction is a normal outcome.

## Recommended workflow

When adding or changing a callback:

1. Identify its owning API and apply the required callback attribute.
2. Add `[[Async]]` to the complete direct call chain if it reaches `Yield`, `Game.Sync`, or another async-marked function.
3. List every authoritative entity read or written between suspension points.
4. Acquire one complete cover for that operation and do not hold `Game.Lock` while acquiring it.
5. Put mutable state on its lifecycle owner; document any mutable-global exception.
6. Revalidate and reacquire after suspension or re-entry.
7. Add a focused script/compiler/runtime test and run the embedding project's script bake.

## Validation routes

Choose the narrowest gate that proves the changed contract:

| Change | Minimum validation |
|---|---|
| Callback attribute or direct-call rule | `Test_AngelScriptAttributes` and project script compilation/bake. |
| Mutable global policy | `Test_AngelScriptBaker` and project script compilation/bake. |
| `Yield` or context scheduling | AngelScript context tests plus the affected client/server runtime test. |
| Server cover, singleton lock, or access validation | `Test_EntitySync`, affected script-method/entity tests, and a project runtime path. |
| Inbound server remote-call cover | Server runtime/remote-call tests plus a handler that reads the caller's controlled critter. |
| Persisted critter preload migration | `Test_EntityLifecycle` plus the embedding project's migration and bake tests. |
| `InitScript` authoring or runtime resolution | `Test_ServerMapOperations`, focused baker tests, and an embedding-project bake. |
| Entity callback/time-event lifetime | Entity/time-event tests and a destruction or shutdown smoke path. |
| Script object lifetime or shutdown GC | `Test_AngelScriptCall`, `Test_ScriptBuiltins`, and an engine shutdown smoke path. |
| Project script behavior only | Embedding-project bake plus the narrowest gameplay/scene test. |

For all engine documentation changes, also run the standalone documentation gate from [Documentation maintenance](../../contributing/documentation/).

## Review checklist

- The callback is entered through its owning API, not called directly.
- Every async caller carries `[[Async]]`.
- No entity cover or state snapshot is assumed to survive `Yield`.
- `Game.Lock` is balanced and released before `Game.Sync`.
- One `Game.Sync` call names the complete entity set for the next operation.
- A preload migration touches only the restored critter state supported at that phase.
- Mutable state lives on an explicit lifecycle owner.
- Destruction cleanup is not duplicated in a project-global registry without functional need.
- The selected test exercises the actual compile, scheduling, synchronization, or teardown boundary.
