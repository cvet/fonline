---
layout: default
title: Lock several entities for one action
locale: en
document_id: mechanics-cover-transactions
permalink: /Docs/en/how-to/mechanics/cover-transactions.html
---

# Lock several entities for one action

An action that reads one entity and changes another is correct only when its checks and its writes happen under one cover of every entity involved. This page works through one such transaction from `Examples/MechanicsSandbox`: a character hands an inventory item to the character next to it after a short delay. It starts from the acting entity, widens the cover to the partner, gives everything up across the delay, and commits under one fresh request.

| Backend | Status |
| --- | --- |
| AngelScript | `runtime-verified` |
| Managed C# | `shape` |

## Source paths inspected

- `Examples/MechanicsSandbox/Scripts/Recipes/CoverTransaction.fos`
- `Examples/MechanicsSandbox/Scripts/Tests/Test_CoverTransaction.fos`
- `Examples/MechanicsSandbox/Managed/Recipes/CoverTransaction.cs`
- `Examples/MechanicsSandbox/mechanics-smoke.json`
- `Examples/MechanicsSandbox/FOnlineMechanicsSandbox.fomain`
- `Source/Server/EntitySync.cpp`
- `Source/Scripting/ServerGlobalScriptMethods.cpp`
- `Source/Common/ScriptSystem.h`
- `Source/Tests/Test_ServerEntityLifetime.cpp`
- `Source/Scripting/Managed/CoreScripts/Sync.cs`
- `Source/Scripting/Managed/Analyzers/SyncCoverAnalyzer.cs`
- `Docs/generated/api.json`

## Problem

The giver must still hold the item, both characters must stand on the same map next to each other, and the item must end up with exactly one of them. Each fact lives on a different entity, the server runs jobs on several worker threads (`Server.WorkerThreads = 2` in the sandbox), and the hand-over takes 300 ms. Another job can move the item, move a character or destroy one while the action waits. Reach and delay are game policy; covering, waiting and checking again are what the Engine contract requires.

## Engine primitives used

| Symbol | Use |
| --- | --- |
| `script.method.server.Game.GetCritter` | Resolves a party by id; null when it is missing or being destroyed |
| `script.method.server.Game.Sync#3a28adb1be02af8c` | Covers the giver alone |
| `script.method.server.Game.SyncWiden` | Adds the receiver to the held cover |
| `script.method.server.Game.Sync#b42b6f6d6b684d2a` | Covers both parties with one request at commit |
| `script.property.Critter.MapId`, `script.method.server.Game.GetDistance#dacea0c00ba4c5f1` | The shared adjacency rule |
| `script.method.server.Critter.GetItem#3e4a8f273e418d3b` | Finds the item in the giver's inventory |
| `script.method.server.Game.MoveItem#c26f82c316789816` | Moves it to the receiver; returns null if the move removes it |
| `script.method.server.Game.DestroyCritter` | The test destroys the receiver during the wait |

`Yield`, `[[Async]]`, `[[TimeEvent]]`, `Entity.IsDestroyed` and `ZERO_IDENT` are backend built-ins.

## The cover model in practice

The contract is owned by [Script Lifecycle and Concurrency](../scripting/lifecycle-and-concurrency.md#server-entity-synchronization) and [Server Runtime](../../explanation/runtime/server.md#entity-synchronization-and-locking). For one action it comes down to five rules:

- **One request names everything.** `Game.Sync` replaces the cover with the listed live entities and their auto-widen partners (a player and its controlled critter); it never adds to the previous cover. The set is acquired in one address-sorted pass, which is what keeps it deadlock-free (`SyncContext::AcquireLocks` in `Source/Server/EntitySync.cpp`).
- **Holding an entity covers what it contains.** Covering the giver covers the items it carries; covering a map covers the critters and items on it. An operation that changes a container needs the container: removing a critter changes its map, so the test covers the location before `Game.DestroyCritter`.
- **An acquisition is a point where other jobs run.** When the request is not already held, the context releases its locks before it acquires the new set (`SyncContext::SyncEntities`). Under contention the thread waits holding nothing, outer contexts included, and gets everything back before the call returns. Whatever was read before the call must be read again after it.
- **No cover survives a suspension.** `Yield` ends the script execution and its synchronization context; the continuation starts with no cover ([covers do not survive suspension](../scripting/lifecycle-and-concurrency.md#covers-do-not-survive-suspension)).
- **A destroyed entity is dropped, not reported.** The two-entity, three-entity and array overloads of `Game.Sync`, and `Game.SyncWiden`, skip an entity destroyed since it was resolved; the generated reference marks them `AllowDestroyedEntityArgs`. The one-entity overload `script.method.server.Game.Sync#3a28adb1be02af8c` lacks that flag, so a destroyed argument fails at the script boundary instead (`ConvertArg` in `Source/Common/ScriptSystem.h`). Either way, test `IsDestroyed` under the new cover.

## The worked transaction

`HandOverItem` is a plain `[[Async]]` function: a time event, a remote-call handler or a test calls it directly with ids, and every caller carries `[[Async]]` too. Only ids cross the delay:

```angelscript
// Ids, not handles, cross the suspension: every step resolves and covers its entities again
[[Async]]
bool HandOverItem(ident giverId, ident receiverId, ident itemId)
{
    if (!PrepareHandOver(giverId, receiverId, itemId)) {
        Game.Log("cover_transaction_refused step=prepare");
        return false;
    }

    // The hand-over takes time; no cover and no decision survive this suspension
    Yield(HandOverDelayMs);

    return CommitHandOver(giverId, receiverId, itemId);
}
```

### Prepare: cover the actor, then widen

```angelscript
[[Async]]
bool PrepareHandOver(ident giverId, ident receiverId, ident itemId)
{
    Critter? giver = Game.GetCritter(giverId);
    if (giver == null) {
        return false;
    }

    // Start from the acting critter, then widen the cover to the partner it names
    Game.Sync(giver);

    Critter? receiver = Game.GetCritter(receiverId);
    if (receiver == null) {
        return false;
    }

    Game.SyncWiden(array<Entity> = {receiver});

    // Widening can hand the held locks over before it returns, so the checks follow it
    return CanHandOver(giver, receiver) && giver.GetItem(itemId) != null;
}
```

The prepare step refuses early, before the player waits for nothing. Its answer decides nothing for the commit.

### Commit: resolve again, cover with one request

```angelscript
[[Async]]
bool CommitHandOver(ident giverId, ident receiverId, ident itemId)
{
    // A later observation: resolve both parties again and cover them with one request
    Critter? giver = Game.GetCritter(giverId);
    Critter? receiver = Game.GetCritter(receiverId);
    if (giver == null || receiver == null) {
        Game.Log("cover_transaction_refused step=commit reason=party_gone");
        return false;
    }

    Game.Sync(giver, receiver);

    if (!CanHandOver(giver, receiver)) {
        Game.Log("cover_transaction_refused step=commit reason=parties_changed");
        return false;
    }

    Item? item = giver.GetItem(itemId);
    if (item == null) {
        Game.Log("cover_transaction_refused step=commit reason=item_gone");
        return false;
    }

    // All checks ran under the same cover as the move, so nothing can change in between
    Item? moved = Game.MoveItem(item, receiver);
    return moved != null;
}
```

### The rule both steps share

```angelscript
// The rules that both parties must satisfy, read from the currently covered state
bool CanHandOver(Critter giver, Critter receiver)
{
    // Game.Sync skips an entity destroyed after it was resolved instead of failing
    if (giver.IsDestroyed || receiver.IsDestroyed) {
        return false;
    }

    if (giver.MapId == ZERO_IDENT || giver.MapId != receiver.MapId) {
        return false;
    }

    return Game.GetDistance(giver, receiver) <= HandOverDistance;
}
```

## Widening and revalidation after suspension

What `Game.SyncWiden` does, read from `Source/Server/EntitySync.cpp`:

1. `SyncContext::WidenEntities` builds one request from the entities the context already holds, except destroyed ones, plus the new entities, and passes it to `SyncContext::SyncEntities`.
2. `TryRetainCoveredRequest` keeps the held locks in place only when every requested entity is already held or covered through its own parent chain, such as an item inside a held critter. `ServerSyncWidenOfCoveredEntityKeepsHeldLocks` in `Source/Tests/Test_ServerEntityLifetime.cpp` pins that case.
3. Otherwise `SyncEntities` calls `ReleaseLocks()` and then `AcquireLocks(...)` for the whole union. Between the two calls another job can lock the giver and change it.

In the hand-over the receiver is another critter on the map, not something inside the giver, so the widening takes the release path. When it returns, both parties are covered, but the giver may have moved or lost the item since `Game.Sync(giver)` returned. That is why `PrepareHandOver` runs `CanHandOver` and looks up the item after the widening, never between the two calls. Treat a widening like a fresh `Game.Sync` of the union: covered afterwards, not unchanged.

After the `Yield`, the commit holds no cover and no trusted handle. It resolves both parties from their ids, covers them with one `Game.Sync(giver, receiver)`, repeats every check, resolves the item again and only then moves it. Every decision and the write happen under that one cover, so no other job can change the parties between the last check and the move.

## Managed C# equivalent

Status `shape`: `Managed/Recipes/CoverTransaction.cs` follows the same steps, but no Engine lane compiles it, so the analyzer verdicts below have not been observed on this file.

```csharp
    private static async Task<bool> PrepareHandOver(ident giverId, ident receiverId, ident itemId)
    {
        Critter? giver = Game.GetCritter(giverId);
        if (giver == null || !await Sync.Lock(giver)) {
            return false;
        }

        // Start from the acting critter, then widen the cover to the partner it names
        Critter? receiver = Game.GetCritter(receiverId);
        if (receiver == null || !await Sync.Widen(receiver)) {
            return false;
        }

        // Widening can hand the held locks over before it returns, so the checks follow it
        return CanHandOver(giver, receiver) && giver.GetItem(itemId) != null;
    }
```

- `Sync.Lock` and `Sync.Widen` from `CoreScripts/Sync.cs` wrap `Game.Sync` and `Game.SyncWiden`, which `FOSYNC005` rejects outside the helpers. They return `false` when an entity is destroyed, or being destroyed by another thread, before or after the acquisition; `FOSYNC010` rejects discarding that answer, so the shape folds it into each condition.
- `CanHandOver` declares `[RequiresCover]` on both parameters; `FOSYNC002` reports a caller that neither acquires that cover nor declares it in turn.
- `await ScriptTask.Delay(HandOverDelayMs)` replaces `Yield`. An `await` ends the cover, and `FOSYNC009` reports a covered value used after an `await` that nothing re-proved; `CommitHandOver` resolves fresh values instead.
- The analyzer models `Sync.Widen` as extending the held cover (`[CoverEffect(CoverEffectKind.Extend)]`), so `FOSYNC009` does not ask to re-prove the giver after it. That is a statement about cover, not about state: the native widening above still releases and reacquires, so the checks still follow the widening. `FOSYNC015` reports a widening that names only entities the caller already covers.

The rules and their configuration are in [server entity synchronization](../scripting/managed-csharp.md#server-entity-synchronization) and [Engine-contract analyzers](../project/engine-contract-analyzers.md).

## Pitfalls

- Calling `Game.Sync(a)` and then `Game.Sync(b)` to add `b`. The second call replaces the first; request both at once or widen.
- Checking before a widening and acting after it. The widening can release the held cover first; check after it.
- Keeping `Item` or `Critter` handles across a `Yield` and acting on them. Carry ids and resolve them again.
- Moving several items under separate covers, or validating after the first move. Check everything under one cover, then write.
- Treating a returned `Game.Sync` as proof that the entities live. It skips destroyed entities; test `IsDestroyed`, as `CanHandOver` does.
- Calling `Game.Sync` while `Game.Lock()` is held. The Engine throws, because that order would close a lock cycle ([`Game.Lock` is separate](../scripting/lifecycle-and-concurrency.md#gamelock-is-separate)).
- Covering only the critters when the operation changes their map. Destroying or transferring a critter changes its map; cover the map or its location.

## Test route

Scenario `cover-transactions` in `mechanics-smoke.json` uses the `RecipeTests` sub-config (in-memory database, networking off) and `-Testing.Filter cover_transaction.`. Each test starts `HandOverItem` from a time event:

| Test | Another job during the 300 ms wait | Expected result |
| --- | --- | --- |
| `hand_over_moves_the_item` | nothing | the receiver holds the parcel |
| `item_taken_during_the_wait_is_not_moved` | moves the parcel to a bystander after 50 ms | the commit refuses with `item_gone`; the bystander keeps the parcel |
| `receiver_destroyed_during_the_wait_is_resolved_again` | destroys the receiver after 50 ms | the commit refuses with `party_gone`; the giver keeps the parcel |

The interfering jobs are ordinary time events in `Test_CoverTransaction.fos`. `DestroyReceiver` covers the receiver's location with `Game.Sync(location)` before `Game.DestroyCritter`, because removing the receiver changes its map. Besides one `[Testing] PASS cover_transaction.<test>` marker per test and `[Testing] COMPLETE status=passed`, the runner requires:

```text
cover_transaction_interference=parcel_moved
cover_transaction_refused step=commit reason=item_gone
cover_transaction_interference=receiver_destroyed
cover_transaction_refused step=commit reason=party_gone
```

Run the sandbox from `Examples/MechanicsSandbox` with `python validate.py` (Windows) or `python3 validate.py` (Linux). Against an existing bake, `Examples/MechanicsSandbox/README.md` shows the single-suite server command; pass `-Testing.Filter "cover_transaction."`.

Runtime verification: the three tests passed on a Windows host with two worker threads, and both interference markers and both refusals appeared in the server log. Not verified: the release window inside a widening, which no deterministic test opens (the statement above comes from `EntitySync.cpp` and the unit test it names); the prepare-step refusals; a Linux run; the Managed C# backend.

## See also

- [Build Game Mechanics](index.md)
- [Validated action request](validated-action.md)
- [Script Lifecycle and Concurrency](../scripting/lifecycle-and-concurrency.md)
- [Server Runtime](../../explanation/runtime/server.md#entity-synchronization-and-locking)
- [Managed C# Scripting](../scripting/managed-csharp.md#server-entity-synchronization)
- [Generated method reference](../../reference/script-api/methods.md)
