---
layout: default
title: NPC patrol with awareness
locale: en
document_id: recipe-npc-patrol
permalink: /Docs/en/how-to/mechanics/npc-patrol.html
recipe_kind: primitive
---

# NPC patrol with awareness

This recipe builds a guard that walks a saved route from waypoint to waypoint, pauses when an intruder comes into view and resumes afterwards. Walking is asynchronous: `MoveToHex` returns at once and the walk takes real server time, so the recipe is a chain of short time events, one leg each, and the Engine's stop event drives the chain forward. The route and its index are persistent data; the timers and the event subscriptions are runtime state that the guard's `InitScript` rebuilds after every load.

| Backend | Status |
| --- | --- |
| AngelScript | `runtime-verified` |
| Managed C# | `shape` |

Runtime-verified means that the in-process scenario `npc-patrol` and the two-phase restart scenarios `npc-patrol-restart-write` and `npc-patrol-restart-verify` passed on a built and baked sandbox on a Windows host. Not verified: a Managed C# compile or run, the `TraceFailed`, `GagCritter` and `Deadlock` outcomes of a walk, a waypoint occupied by another critter, a distance-limited visibility rule (the sandbox has none, see [Awareness](#awareness)) and the server snapshot restore path.

## Source paths inspected

- `Examples/MechanicsSandbox/Scripts/Recipes/NpcPatrol.fos`
- `Examples/MechanicsSandbox/Scripts/Tests/Test_NpcPatrol.fos`
- `Examples/MechanicsSandbox/Managed/Recipes/NpcPatrol.cs`
- `Examples/MechanicsSandbox/Content/NpcPatrol.fopro`
- `Examples/MechanicsSandbox/FOnlineMechanicsSandbox.fomain`
- `Examples/MechanicsSandbox/mechanics-smoke.json`
- `Source/Scripting/ServerCritterScriptMethods.cpp`
- `Source/Server/Server.cpp`
- `Source/Server/MapManager.cpp`
- `Source/Common/TimeEvents.cpp`
- `BuildTools/validation-project/TestServerExtension.cpp`
- `Docs/en/reference/native-extension/hooks.md`
- `Docs/generated/api.json`

## Problem

A patrolling NPC must keep walking without a script waiting on it, survive blocked waypoints, react to what it sees and come back after a restart. The Engine gives a path-based walk, a stop notification and visibility events; the loop that joins them, the failure policy and the pause are the recipe. Routes, speeds, alert behavior and what a sighting means are game policy.

## Engine primitives used

| Symbol | Side | Role in the recipe |
| --- | --- | --- |
| `script.method.server.Critter.MoveToHex#59785332969dbd15` | server | `MoveToHex(mpos hex, int32 cut, int32 speed, callback(bool,Critter,Item) gagCallabck)` starts one leg and returns its context |
| `script.ref-type.common.MovingContext` | common | The context of that leg |
| `script.ref-method.common.MovingContext.IsCompleted` | common | True at once for a refused or already finished request |
| `script.ref-method.common.MovingContext.GetCompleteReason` | common | The reason of such an immediate completion |
| `script.method.server.Critter.GetMovingState` | server | The result of the walk that just stopped |
| `script.enum.MovingState` | common | The terminal or in-progress result |
| `script.enum-value.MovingState.Success` | common | The leg reached its hex |
| `script.enum-value.MovingState.Stopped` | common | A script or request stopped the walk |
| `script.enum-value.MovingState.HexBusy` | common | The waypoint hex is occupied or unavailable |
| `script.method.server.Critter.StopMoving` | server | Ends the walk when the guard sees an intruder |
| `script.event.server.Game.OnCritterStopMoving` | server | Fires after an active walk stops, whatever the reason |
| `script.event.server.Critter.OnCritterAppeared` | server | Per-critter event: another critter entered this one's view |
| `script.event.server.Critter.OnCritterDisappeared` | server | The other critter left the view |
| `script.property.Critter.LookDistance` | common | The look range that a project visibility rule may apply |
| `script.property.Critter.InitScript` | server | Names the function that runs for a new and for every restored guard |
| `script.method.server.Entity.StartTimeEvent#e59548a665ddb838` | server | `StartTimeEvent(timespan delay, callback(void,Entity) func)` schedules the next leg |
| `script.method.server.Entity.CountTimeEvent#7ed3553b71048a0d` | server | `CountTimeEvent(callback(void,Entity) func, bool excludeExecuting = false)` keeps one waiting leg |
| `script.method.server.Entity.StopTimeEvent#d792082ed73a8fc8` | server | Removes a waiting leg when the guard pauses |
| `script.method.server.Game.Sync#b42b6f6d6b684d2a` | server | `Sync(Entity entity1, Entity entity2)` covers the guard and its map |
| `script.method.server.Critter.GetMap` | server | The map the leg walks on |

`[[TimeEvent]]`, `[[Async]]`, `[[Event]]`, `[[ModuleInit]]`, `Critter.OnCritterAppeared.Subscribe` and `Entity.IsDestroyed` are backend built-ins, and the four `///@ Property` lines are project metadata.

## Minimal example

Under `Examples/MechanicsSandbox/`: `Scripts/Recipes/NpcPatrol.fos` is the recipe, `Scripts/Tests/Test_NpcPatrol.fos` holds its tests, `Content/NpcPatrol.fopro` has the guard, an intruder and a blocking crate, and `Managed/Recipes/NpcPatrol.cs` is the Managed C# shape. [Test route](#test-route) has the commands.

## Step by step

The route and the index are persistent. The failure counter and the pause flag are `Mutable` but not `Persistent`: a restart is a clean start for both.

```angelscript
///@ Property Critter Server mpos[] PatrolRoute Mutable Persistent
///@ Property Critter Server int32 PatrolIndex Mutable Persistent
///@ Property Critter Server int32 PatrolFailures Mutable
///@ Property Critter Server bool PatrolPaused Mutable
```

The prototype names the `InitScript` that runs for a new guard (`firstTime == true`) and for every restored one (`firstTime == false`; [entity `InitScript` callbacks](../scripting/lifecycle-and-concurrency.md#entity-initscript-callbacks)). `LookDistance` is explained under [Awareness](#awareness):

```ini
LookDistance = 12
InitScript = NpcPatrol::InitPatroller
```

Two kinds of subscription exist. The stop event is global, so one handler serves every patroller and `InitNpcPatrol` subscribes it once at module initialization (`Game.OnCritterStopMoving.Subscribe(OnCritterStopMoving)`). The appearance events belong to each critter, and an entity's subscriptions are not part of its saved state, so the `InitScript` subscribes again on every load and, when a route is saved, schedules the next leg:

```angelscript
// The prototype names this function in its InitScript: it runs for a new patroller and again for every restored one
void InitPatroller(Critter cr, bool firstTime)
{
    // Subscriptions and walking are runtime state; the saved route is not, so a restored patroller starts over from it
    cr.OnCritterAppeared.Subscribe(OnSeen);
    cr.OnCritterDisappeared.Subscribe(OnLost);

    if (cr.PatrolRoute.length() != 0) {
        ScheduleLeg(cr, LegPauseMs);
    }
}
```

`StartPatrol` writes the route, resets the index, the failure counter and the pause flag, and schedules the first leg from a time event, never from the caller. `ScheduleLeg` keeps at most one waiting leg. A one-shot event stays in the entity's list, marked as executing, until its callback has completed and `TimeEventManager::PostFireTimeEvent` removes it (`Source/Common/TimeEvents.cpp`), so a leg that schedules its successor from inside itself must pass `excludeExecuting = true`; with the default it counts itself and no successor is ever scheduled:

```angelscript
// At most one waiting leg per critter; a running leg schedules its successor, so it does not count itself
void ScheduleLeg(Critter cr, int32 delayMs)
{
    if (cr.CountTimeEvent(PatrolStep, true) == 0) {
        cr.StartTimeEvent(Milliseconds(delayMs), PatrolStep);
    }
}
```

One leg is one `MoveToHex`. A critter time event covers only its critter, and `MoveToHex` also validates the critter's map (`StartCritterMoveToHex` in `Source/Scripting/ServerCritterScriptMethods.cpp`), so the leg is `[[Async]]` and covers both. A refused or already finished request completes at once and never raises the stop event, so the leg reads its result itself; a walk that is under way ends in the stop event:

```angelscript
// A critter time event covers only its critter; walking also changes the map, so the map joins the cover
[[TimeEvent]] [[Async]]
void PatrolStep(Critter cr)
{
    Map? map = cr.GetMap();
    if (map == null || cr.PatrolPaused || cr.PatrolRoute.length() == 0) {
        return;
    }

    Game.Sync(cr, map);

    // Other work may run while the cover changes hands
    if (cr.IsDestroyed || cr.MapId != map.Id || cr.PatrolPaused) {
        return;
    }

    mpos[] route = cr.PatrolRoute;
    MovingContext move = cr.MoveToHex(route[cr.PatrolIndex % route.length()], 0, WalkSpeed, NoGagPassage);

    // A leg that is walking ends in OnCritterStopMoving; a refused one never raised that event, so its result is read here
    if (!move.IsCompleted()) {
        cr.PatrolFailures = 0;
        return;
    }

    MovingState state = move.GetCompleteReason();
    if (state == MovingState::Success) {
        Advance(cr);
        ScheduleLeg(cr, LegPauseMs);
        return;
    }

    RetryOrSkip(cr, state);
}
```

The stop handler reads the state of the walk that just ended. Only `Success` moves the route on. `Stopped` is a decision of whoever stopped the walk, so the handler leaves it alone; any other state is a failed leg. A walk that ends by itself raises this event with `Success`, and an immediately completed one never does, so the index cannot advance twice for one leg:

```angelscript
// The engine reports every stop, including the ones this script causes; only a finished walk moves the patrol on
[[Event]]
void OnCritterStopMoving(Critter cr)
{
    if (cr.PatrolRoute.length() == 0 || cr.PatrolPaused) {
        return;
    }

    MovingState state = cr.GetMovingState();
    if (state == MovingState::Success) {
        Advance(cr);
        ScheduleLeg(cr, LegPauseMs);
        return;
    }

    // A script stop is a decision of its caller; any other reason is a failed leg
    if (state != MovingState::Stopped) {
        RetryOrSkip(cr, state);
    }
}
```

A failed leg is retried every `RetryMs` and the waypoint is skipped after `MaxFailures` failures in a row, so one blocked hex cannot hold the patrol forever (`RetryOrSkip` in the same file). The test puts a blocking crate on the second waypoint: the patrol walks to the first, skips the second after three refusals and reaches the third, and only the two finished walks raise a stop event.

### Awareness

The watcher's `OnCritterAppeared` runs when another critter enters its view. The first argument of an entity event is the owner. The pause is written first, so the stop event that the `StopMoving` call raises with `Stopped` finds the flag and leaves the patrol alone; a timer then resumes it:

```angelscript
// Runs on the watcher: a critter entered its view; the event owner is the first argument
[[Event]]
void OnSeen(Critter cr, Critter seen)
{
    if (seen.ProtoId != IntruderPid) {
        return;
    }

    // The pause comes first, so the stop event that follows sees it and leaves the patrol alone
    cr.PatrolPaused = true;
    cr.StopMoving();
    cr.StopTimeEvent(PatrolStep);
    Game.Log("npc_patrol_saw critter=" + cr.Id.value + " seen=" + seen.Id.value);

    if (cr.CountTimeEvent(ResumePatrol) == 0) {
        cr.StartTimeEvent(Milliseconds(SightPauseMs), ResumePatrol);
    }
}
```

`OnLost` only logs, and `ResumePatrol` clears the flag and schedules a leg. What "in view" means is not decided by the Engine's own code. `MapManager::IsCritterSeeCritter` calls the native hook `CheckCritterVisibilityHook`, whose default returns full visibility ([hook reference](../../reference/native-extension/hooks.md#checkcrittervisibilityhook)), so without a project hook every critter on a map sees every other one and `LookDistance` changes nothing for these events. A project that wants a range applies `LookDistance` in its hook, as the validation project's hook does with `GetDistance(...) <= LookDistance`; changing the property then re-runs visibility (`ServerEngine::OnSetCritterLookDistance`). The sandbox has no hook, so its `LookDistance = 12` is inert and the test places the intruder three hexes away: it verifies the event handling, not a range.

## Authority and sync

- The patrol is server-only NPC logic. Clients see the walk through the ordinary movement broadcast and own nothing.
- A critter time event covers that critter. `PatrolStep` adds the map with one `Game.Sync` and checks again afterwards, because acquiring a cover lets other jobs run ([covers do not survive suspension](../scripting/lifecycle-and-concurrency.md#covers-do-not-survive-suspension)).
- The stop and appearance handlers write only properties of the owner critter and start or stop its time events, so they need no cover beyond the one the Engine entered with. All path searches stay in `PatrolStep`, which keeps the notification cheap.
- Every leg is scheduled through `ScheduleLeg`, from the stop handler, from `PatrolStep` and from `ResumePatrol`, and it keeps the number of waiting legs at one.

## Persistence and migration

| Survives a restart | Does not survive |
| --- | --- |
| `PatrolRoute` and `PatrolIndex`, persistent properties of the guard | The time events `PatrolStep` and `ResumePatrol`: no database load restores them |
| The guard itself, when its location is persistent | The event subscriptions of the guard and the moving context of a walk in progress |
| | `PatrolFailures` and `PatrolPaused`, which are not persistent |

After a restart the `InitScript` runs with `firstTime == false`, subscribes again and schedules a leg from the saved index. The restart scenarios walk part of the route, stop the server, start it on the same JSON database and require: the route has three points, the index did not go back, a walk or a waiting leg exists, and an intruder placed later pauses the restored guard, which proves the subscription. A leg that was under way continues from where the guard stands, toward the saved index, because the index advances only on arrival.

The server snapshot restore path restores captured time events after the init events ([the same limit as in the timed effect recipe](timed-effect.md#persistence-and-migration)), so a captured `PatrolStep` can sit next to the one that init scheduled. This path is not run in the sandbox. Renaming `PatrolRoute` follows [Declare a rule for every rename, removal and move](../project/data-migrations.md#declare-a-rule-for-every-rename-removal-and-move).

## Pitfalls and anti-patterns

- Counting the callback's own timer. `CountTimeEvent` counts an executing one-shot callback unless `excludeExecuting` is true, so a leg that schedules its successor with the default guard schedules nothing and the patrol silently ends.
- Treating every `OnCritterStopMoving` as an arrival. It also fires for a script `StopMoving` (`Stopped`) and for failed walks, and a refused `MoveToHex` raises it only when the refusal interrupted a walk in progress, with state `Stopped`; a refusal on an idle critter raises nothing. Read `GetMovingState`, and read the immediate result of `MoveToHex` yourself.
- Calling `MoveToHex` from a time event without covering the map. The critter's own cover is not enough; the call fails with `Entity access without sync` for the map.
- Assuming a restored guard still has its subscriptions or timers. Per-critter subscriptions and time events are runtime state; only the `InitScript` (or `Game.OnCritterInit` with `firstTime == false`) brings them back.
- Assuming `LookDistance` limits awareness. The default visibility hook returns full visibility; the range exists only where a project hook applies it. Map queries such as `Map.GetCrittersWhoSeeHex` do use `LookDistance` directly.
- Assuming what happens when another critter stands on a waypoint. The sandbox observed a walk to such a hex ending with `Success`, and the recipe makes no claim about it; the tests block waypoints with an item instead.

## Test route

Run every sandbox scenario from `Examples/MechanicsSandbox` with `python validate.py` on Windows (PowerShell) or `python3 validate.py` on Linux (bash); its `README.md` lists the prerequisites. This recipe's scenarios require, among others:

```text
[Testing] PASS npc_patrol.blocked_waypoint_is_retried_and_skipped_without_a_stop_event
[Testing] PASS npc_patrol.script_stop_is_not_continued_by_the_stop_event
[Testing] PASS npc_patrol_restart_verify.restored_guard_walks_again_and_watches_again
npc_patrol_leg_failed critter=
npc_patrol_saw critter=
```

Walking takes real server time, so each test starts a patrol and a repeating poll judges its state. The restart pair uses the `RecipeRestart` sub-config; the runner overrides its database path with a directory it controls:

```ini
# Two-phase restart tests; the runner passes its own JSON directory with -Server.DbStorage
[SubConfig]
Name = RecipeRestart
Testing.Enabled = True
ServerNetwork.DisableNetworking = True
Server.DbStorage = JSON RecipeRestartDb
```

To run the in-process suite against an existing bake, from the build directory (PowerShell; on Linux use `./Binaries/Server-Linux-x64/FOMECH_ServerHeadless`):

```powershell
& ".\Binaries\Server-Windows-win64\FOMECH_ServerHeadless.exe" -ApplyConfig "<sandbox>\FOnlineMechanicsSandbox.fomain" -ApplySubConfig RecipeTests -Testing.Filter "npc_patrol."
```

For the restart pair use `-ApplySubConfig RecipeRestart -Server.DbStorage "JSON <empty-directory>"` with the filters `npc_patrol_restart_write.` and then `npc_patrol_restart_verify.` on the same directory.

## Managed C#

Status `shape`: an Engine test parses `Managed/Recipes/NpcPatrol.cs` and compares its `///@` lines with the AngelScript module; no Engine lane compiles or runs it. The flow is the same. The leg is an `async Task` time event, `internal` because the Engine dispatches it, and the cover is an awaited helper that reports a gone entity, so the call is a condition:

```csharp
        if (!await Sync.Lock(cr, map)) {
            return;
        }
```

## How a larger project extends it

- **Authored routes.** Store a route id and an index on the guard and keep the points in an authored catalog, for example a [FixedType catalog](../../explanation/entity-and-property-model/choosing-a-representation.md#fixedtype-catalog-row) row, so a route is edited as data. The `mpos[]` property fits routes that the game builds at run time.
- **Alertness policy.** Intruder rules, alarm states, chasing and what a sighting triggers replace `OnSeen`; the pause-first order and the single scheduler stay.
- **A real visibility rule.** Provide `CheckCritterVisibilityHook` in the project's native extension ([native extensions](../native-extensions.md)) and decide whether `LookDistance` limits sight, as the validation project does.
- **A project event with a producer.** Fire one project event where a sighting is decided, so other modules react without polling.
- **What stays in the game project.** Routes, speeds, pauses, retry limits, alert behavior and group rules. The Engine supplies the walk, the stop notification and the visibility events; the loop and the restore hook are the project's code.

## See also

- [Build Game Mechanics](index.md)
- [Timed effect that survives a restart](timed-effect.md)
- [Lock several entities for one action](cover-transactions.md)
- [Script Lifecycle and Concurrency: entity `InitScript` callbacks](../scripting/lifecycle-and-concurrency.md#entity-initscript-callbacks)
- [Maps, Movement, and Geometry: movement contexts](../../explanation/maps-and-movement.md#movement-contexts)
- [Server runtime: a refused move request ends the route it interrupts](../../explanation/runtime/server.md#a-refused-move-request-ends-the-route-it-interrupts)
- [Native extension hooks](../../reference/native-extension/hooks.md#checkcrittervisibilityhook)
- [Managed C# Scripting](../scripting/managed-csharp.md)
- [Script Test Harness Pattern](../project/script-test-harness.md)
