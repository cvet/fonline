---
layout: default
title: Interruptible long action
locale: en
document_id: recipe-long-action
permalink: /Docs/en/how-to/mechanics/long-action.html
recipe_kind: primitive
---

# Interruptible long action

This recipe builds a gathering job that takes time: a player asks to gather from a node, the server starts one job for the critter, and when the time is up the node pays one result and loses one charge, unless something ended the job first. Walking away, a transfer, knockout, death and a disconnect each cancel the job through one `Cancel` function. A token separates the live completion from a stale one, because stopping the timer does not stop a completion that is already running; the sandbox keeps the timer-only version as a negative control and shows it granting its result after the stop.

| Backend | Status |
| --- | --- |
| AngelScript | `runtime-verified` |
| Managed C# | `shape` |

Runtime-verified means that the in-process scenario `long-action`, the real-client scenario `long-action-client` and the two-phase restart scenarios `long-action-restart-write` and `long-action-restart-verify` passed on a built and baked sandbox on a Windows host. Not verified: a Managed C# compile or run, a player switching its controlled critter, an interrupt that arrives while the completion is suspended inside its own `Game.Sync`, and the server snapshot restore path.

## Source paths inspected

- `Examples/MechanicsSandbox/Scripts/Recipes/LongAction.fos`
- `Examples/MechanicsSandbox/Scripts/Tests/Test_LongAction.fos`
- `Examples/MechanicsSandbox/Managed/Recipes/LongAction.cs`
- `Examples/MechanicsSandbox/Content/LongAction.fopro`
- `Examples/MechanicsSandbox/mechanics-smoke.json`
- `Source/Common/TimeEvents.cpp`
- `Source/Server/Server.cpp`
- `Source/Server/MapManager.cpp`
- `Source/Scripting/ServerCritterScriptMethods.cpp`
- `Docs/generated/api.json`

## Problem

A job that takes seconds must not complete after the world made it pointless. The player moved on, the critter was knocked out or killed, the player's connection dropped or the node ran out, and a timer that fires anyway would grant a result for work nobody did. The Engine gives time events and the events that signal each interrupt; keeping one job per critter, cancelling it from every signal and telling a live completion from a stale one is the recipe. Which actions are long, how long they take and what they pay are game policy.

## Engine primitives used

| Symbol | Side | Role in the recipe |
| --- | --- | --- |
| `script.method.server.Entity.StartTimeEvent#b494dfd4a4fa81ea` | server | `StartTimeEvent(timespan delay, callback(void,Entity,any[]) func, any[] data)` schedules the completion with the token as data |
| `script.method.server.Entity.StopTimeEvent#ddfbd34ed1764f92` | server | `StopTimeEvent(callback(void,Entity,any[]) func)` removes the waiting completion |
| `script.method.server.Entity.CountTimeEvent#3032339c32e34b9d` | server | `CountTimeEvent(callback(void,Entity,any[]) func, bool excludeExecuting = false)` counts only waiting completions when asked |
| `script.event.server.Game.OnCritterStartMoving` | server | Interrupt: a walk started |
| `script.event.server.Game.OnCritterTransfer` | server | Interrupt: the critter was transferred, also inside one map |
| `script.event.server.Game.OnPlayerLogout` | server | Interrupt: the controlling player's connection ended |
| `script.property.Critter.Condition` | common | The signal for knockout and death; the Engine has no death event |
| `script.enum.CritterCondition` | common | `Alive` is the only condition that may work |
| `script.method.server.Critter.SetCondition` | server | The tests knock out and kill critters with it |
| `script.method.server.Player.Disconnect` | server | The client drill ends with a graceful disconnect |
| `script.method.server.Player.GetControlledCritter` | server | The actor of a request and of a logout |
| `script.method.server.Map.GetItem` | server | Resolves the node by id on the critter's own map |
| `script.method.server.Game.Sync#b42b6f6d6b684d2a` | server | `Sync(Entity entity1, Entity entity2)` covers player and map at the request |
| `script.method.server.Game.Sync#a0c5737d3ba111b8` | server | `Sync(Entity[] entities)` covers critter and map at the completion |

`[[TimeEvent]]`, `[[Async]]`, `[[ServerRemoteCall]]`, `[[ClientRemoteCall]]`, `[[PropertySetter]]`, `Game.AddPropertySetter` and `Yield` are backend built-ins, and the `///@ Property` and `///@ RemoteCall` lines are project metadata.

## Minimal example

Under `Examples/MechanicsSandbox/`: `Scripts/Recipes/LongAction.fos` is the recipe, `Scripts/Tests/Test_LongAction.fos` holds its tests and the naive completion, `Content/LongAction.fopro` has the node prototypes (`NodeCharges = 2`, `NodeWorkMs = 800`, and a slow node of 60000 ms for the restart test), the ore and the worker, and `Managed/Recipes/LongAction.cs` is the Managed C# shape. [Test route](#test-route) has the commands.

## Step by step

The registry of the running job lives on the critter and is runtime-only: no `Persistent` flag, so a restart leaves every critter idle. The node's charges are persistent and public; its work time is authored data on the prototype:

```angelscript
///@ Property Critter Server bool ActionActive Mutable
///@ Property Critter Server int32 ActionToken Mutable
///@ Property Critter Server ident ActionTarget Mutable
///@ Property Item Common int32 NodeCharges Mutable Persistent PublicSync
///@ Property Item Common int32 NodeWorkMs

///@ RemoteCall Server BeginGather(ident nodeId) MaxBytes 8
///@ RemoteCall Client GatherResult(ident nodeId, bool accepted) MaxBytes 9
```

`BeginGather` is the request handler, built as in [Validated action request](validated-action.md): it derives the actor from the player, covers player and map with `Game.Sync`, derives the actor again, calls `Start` and answers with `GatherResult`. `Start` allows one job per critter. A second request while one runs is refused as `busy`, and a refusal starts nothing. Only the token travels in the timer data; the map and the node are resolved again at the completion:

```angelscript
// Starts the one action a critter may have; returns the refusal reason or an empty string. The caller covers the critter and its map
string Start(Critter cr, Map map, ident nodeId)
{
    if (cr.Condition != CritterCondition::Alive) {
        return "not_alive";
    }

    // One job per critter; a cancelled completion may still be running with a stale token, so only a waiting one counts
    if (cr.ActionActive || cr.CountTimeEvent(FinishAction, true) != 0) {
        return "busy";
    }

    // The node is resolved on the critter's own map; its work time is data on the item
    Item? node = map.GetItem(nodeId);
    if (node == null || node.NodeWorkMs <= 0 || node.NodeCharges <= 0) {
        return "no_node";
    }

    if (Game.GetDistance(cr, node) > WorkDistance) {
        return "too_far";
    }

    cr.ActionToken = cr.ActionToken + 1;
    cr.ActionActive = true;
    cr.ActionTarget = nodeId;

    // Only an id and a token travel in the timer: no handle to the critter's map or node survives the wait
    any[] data = {any(cr.ActionToken)};
    cr.StartTimeEvent(Milliseconds(node.NodeWorkMs), FinishAction, data);
    return "";
}
```

`Cancel` is the one way out for every interrupt and is safe to call when nothing runs. It clears the flag and advances the token, so a completion that already started also finds a token that no longer matches, then removes the waiting timer. `IsCurrent(cr, token)` is `cr.ActionActive && cr.ActionToken == token`:

```angelscript
// The one way out for every interrupt; it is safe to call when nothing is running
void Cancel(Critter cr, string reason)
{
    if (!cr.ActionActive) {
        return;
    }

    cr.ActionActive = false;

    // A completion that is already running cannot be stopped, so it also finds a token that no longer matches
    cr.ActionToken = cr.ActionToken + 1;
    cr.StopTimeEvent(FinishAction);
    Game.Log("long_action_cancelled critter=" + cr.Id.value + " reason=" + reason);
}
```

Each interrupt is one engine fact wired to `Cancel` in `InitLongAction`:

```angelscript
[[ModuleInit]]
void InitLongAction()
{
    // The interrupts: each one is an engine fact that makes the work pointless
    Game.OnCritterStartMoving.Subscribe(OnCritterStartMoving);
    Game.OnCritterTransfer.Subscribe(OnCritterTransfer);
    Game.OnPlayerLogout.Subscribe(OnPlayerLogout);

    // The engine has no death event; the condition property is the signal
    Game.AddPropertySetter(CritterProperty::Condition, OnConditionChanged);
}
```

| Interrupt | Engine signal | Logged reason |
| --- | --- | --- |
| The critter starts walking | `Game.OnCritterStartMoving`, which also fires when the speed of a walk in progress changes | `moved` |
| The critter is transferred | `Game.OnCritterTransfer`, also for a `TransferToMap` inside one map | `transferred` |
| Knockout or death | A post-set callback on `Critter.Condition` | `incapacitated` |
| The player disconnects | `Game.OnPlayerLogout`, which `Player.Disconnect()` leads to | `disconnected` |

The completion runs long after the decision to start, so it checks again. The token is tested before the cover, which costs nothing for a stale call, and again after it, because acquiring the cover lets other jobs run. The map and the node are resolved by id under the new cover; the work changes a map item, so the map joins the cover:

```angelscript
// The completion step: it runs long after the decision to start, so it resolves and checks everything again
[[Async]]
void Complete(Critter cr, int32 token)
{
    if (!IsCurrent(cr, token)) {
        return;
    }

    Map? map = cr.GetMap();
    if (map == null) {
        Cancel(cr, "no_map");
        return;
    }

    // The work changes a map item, so the map joins the cover; getting it may let other work run first
    ident nodeId = cr.ActionTarget;
    Game.Sync(array<Entity> = {cr, map});

    if (cr.IsDestroyed || cr.MapId != map.Id || !IsCurrent(cr, token)) {
        return;
    }

    Item? node = map.GetItem(nodeId);
    if (node == null || node.NodeCharges <= 0) {
        Cancel(cr, "node_gone");
        return;
    }

    cr.ActionActive = false;
    node.NodeCharges = node.NodeCharges - 1;
    cr.AddItem(OrePid);
    Game.Log("long_action_done critter=" + cr.Id.value + " node=" + nodeId.value);
}
```

`FinishAction` is the `[[TimeEvent]]` entry that reads the token from the data array and calls `Complete`; the call is split out so a test can deliver a stale token directly.

### The naive variant fails

`Scripts/Tests/Test_LongAction.fos` keeps the version that cancels with `StopTimeEvent` alone. Its completion suspends like any completion that waits for its cover:

```angelscript
[[TimeEvent]] [[Async]]
void NaiveFinish(Critter cr, any[] data)
{
    // The naive completion suspends like any completion that waits for its cover
    Yield(300);
    Game.Sync(cr);
    cr.AddItem(OrePid);
    Game.Log("long_action_naive_granted critter=" + cr.Id.value);
}
```

A cancel that arrives while it is suspended only removes the timer record:

```angelscript
    Game.Log("long_action_naive_cancel pending=" + worker.CountTimeEvent(NaiveFinish) + " pending_not_running=" + worker.CountTimeEvent(NaiveFinish, true));
    worker.StopTimeEvent(NaiveFinish);
```

The test requires the marker `long_action_naive_granted critter=` and one ore for the cancelled worker. At the stop the log reads `pending=1 pending_not_running=0`: the running event is still counted, and it is excluded when `excludeExecuting` is true. In the source, `TimeEventManager::StopTimeEvent` (`Source/Common/TimeEvents.cpp`) erases the event record and `ServerEngine::OnTimeEventCancel` cancels the scheduled worker job; the run shows that a script that already started is not interrupted. Only a state the completion re-reads, the token, can cancel it.

## Authority and sync

- The client only asks. `BeginGather` takes the node id and nothing else; the actor, the map, the reach rule, the charges and the work time come from the server. The client drill sends a far node, an item that is not a node, the valid node and the valid node again, and the server answers `too_far`, `no_node`, an acceptance and `busy`.
- The completion is a time event of the critter, which covers that critter. The map joins by one `Game.Sync` of critter and map; the node is a map item and is covered through its map ([cover model](cover-transactions.md#the-cover-model-in-practice)).
- The interrupt handlers write only the critter's properties and stop its timers, so the cover they enter with is enough. `OnPlayerLogout` reaches the critter through the player.
- Only ids and a token cross the wait. A handle to the map or the node held across the delay can point at a destroyed entity, and no cover survives a suspension ([covers do not survive suspension](../scripting/lifecycle-and-concurrency.md#covers-do-not-survive-suspension)).

## Persistence and migration

| Survives a restart | Does not survive |
| --- | --- |
| `NodeCharges`, a persistent property of the node | The registry (`ActionActive`, `ActionToken`, `ActionTarget`) and the completion timer |

The restart scenarios start a 60-second job on a persistent location and stop the server. After the restart the worker is idle, no completion timer exists, nothing was granted, the node still has both charges and the worker can start the job again. The recipe abandons work on restart on purpose, and it spends the charge at completion, so abandoned work costs nothing. A project that resumes the job persists a deadline and re-arms the timer from the restore hook, as in [Timed effect that survives a restart](timed-effect.md), and then also decides what a job that finished while the server was down pays.

Renaming `NodeCharges` follows [Declare a rule for every rename, removal and move](../project/data-migrations.md#declare-a-rule-for-every-rename-removal-and-move).

## Pitfalls and anti-patterns

- Cancelling with `StopTimeEvent` alone. It removes a waiting timer but not a completion that already started, as the negative control shows. Advance a token in `Cancel` and test it after the last cover acquisition.
- Counting the job through its own running timer. `CountTimeEvent` counts an executing callback unless `excludeExecuting` is true, so the `busy` check in `Start` passes `true` and counts only a waiting completion; a completion that has finished its decision but still runs does not block the next start.
- Waiting for a death event. The Engine has none: the condition property is the signal, and `SetCondition` to the condition the critter already has changes nothing and runs no setter. Wire knockout and death to that setter.
- Keeping handles across the wait. Store ids in the timer data and resolve them under the new cover; a destroyed map or node is then a refusal reason, not a crash.
- Assuming the list of interrupts is complete. Every extra reason, such as taking damage or dropping the tool, needs its own call to `Cancel`; the Engine's events cover only the four above. A player switching its controlled critter is not among them and was not run.
- Starting the job without checking the alive condition, reach and node state on the server. Those are validation rules, and the client's copy of them is a convenience.

## Test route

Run every sandbox scenario from `Examples/MechanicsSandbox` with `python validate.py` on Windows (PowerShell) or `python3 validate.py` on Linux (bash); its `README.md` lists the prerequisites. This recipe's scenarios require, among others:

```text
[Testing] PASS long_action.stale_completion_is_ignored
[Testing] PASS long_action.stop_time_event_does_not_stop_a_running_completion
[Testing] PASS long_action_client.client_requests_are_validated_and_disconnect_cancels
[Testing] PASS long_action_restart_verify.restart_abandons_the_work
long_action_refused reason=too_far
long_action_refused reason=no_node
long_action_refused reason=busy
long_action_naive_granted critter=
reason=disconnected
```

The client scenario starts the server with the `RecipeNetworkTests` sub-config, waits for `long_action_waiting_for_client` and starts a headless client with `RecipeClient`. The client sends four requests in one burst and then `GatherDrillSent`; the handlers of one connection run in arrival order, so the server checks that only the valid request started a job and then disconnects the player in the middle of the work. To run the in-process suite against an existing bake, from the build directory (PowerShell; on Linux use `./Binaries/Server-Linux-x64/FOMECH_ServerHeadless`):

```powershell
& ".\Binaries\Server-Windows-win64\FOMECH_ServerHeadless.exe" -ApplyConfig "<sandbox>\FOnlineMechanicsSandbox.fomain" -ApplySubConfig RecipeTests -Testing.Filter "long_action."
```

For the restart pair use `-ApplySubConfig RecipeRestart -Server.DbStorage "JSON <empty-directory>"` with the filters `long_action_restart_write.` and then `long_action_restart_verify.` on the same directory.

## Managed C#

Status `shape`: an Engine test parses `Managed/Recipes/LongAction.cs` and compares its `///@` lines with the AngelScript module; no Engine lane compiles or runs it. The flow is the same. The completion is an `async Task` time event, `internal` because the Engine dispatches it, and the cover is an awaited helper that reports a gone entity, so the call is a condition. The shape registers its condition setter through the Managed spelling `Game.AddPropertyDeferredSetter`, which no lane compiled:

```csharp
        if (!await Sync.Lock(cr, map)) {
            return;
        }
```

## How a larger project extends it

- **Many actions.** One registry per critter serves every long action: add a kind to the registry, give each kind its own completion function and keep the single `Cancel`. Actions that may run side by side need a registry per kind.
- **Data-driven actions.** Durations, yields and requirements are authored rows, for example a [FixedType catalog](../../explanation/entity-and-property-model/choosing-a-representation.md#fixedtype-catalog-row) keyed by action id; the item keeps only what changes, its charges.
- **A project event with a producer.** Fire one project event where the job completes or cancels, so quest and statistics modules react without polling.
- **Resumable work.** Store the deadline in a persistent property and re-arm from the restore hook ([timed effect](timed-effect.md)); keep the token, because a cancelled completion can still be running.
- **What stays in the game project.** Which actions are long, durations, yields, what interrupts them besides the four engine signals, and the client's progress display. The Engine supplies time events and the interrupt events; the registry and the token are the project's code.

## See also

- [Build Game Mechanics](index.md)
- [Validated action request](validated-action.md)
- [Timed effect that survives a restart](timed-effect.md)
- [Lock several entities for one action](cover-transactions.md)
- [Script Lifecycle and Concurrency: time-event completion](../scripting/lifecycle-and-concurrency.md#time-event-completion)
- [Script Lifecycle and Concurrency: time events after a database reload](../scripting/lifecycle-and-concurrency.md#time-events-after-a-database-reload)
- [Managed C# Scripting](../scripting/managed-csharp.md)
- [Script Test Harness Pattern](../project/script-test-harness.md)
