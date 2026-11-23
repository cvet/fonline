---
layout: default
title: Timed effect that survives a restart
locale: en
document_id: recipe-timed-effect
permalink: /Docs/en/how-to/mechanics/timed-effect.html
recipe_kind: primitive
---

# Timed effect that survives a restart

This recipe builds a boost that ends at its deadline even when the server restarts or the critter is unloaded and loaded in between. The obvious version, a time event started when the effect starts, silently loses its timer on every reload; the sandbox keeps that version as a negative control and proves the loss. The working version stores the deadline in a persistent property, re-arms the timer from the restore hook, and lets only one timer exist.

| Backend | Status |
| --- | --- |
| AngelScript | `runtime-verified` |
| Managed C# | `shape` |

Runtime-verified means that the in-process scenario `timed-effect` and the two-phase restart scenarios `timed-effect-restart-write` and `timed-effect-restart-verify` against one JSON database directory passed on a built and baked sandbox on a Windows host, with the naive variant failing as expected. Not verified by the sandbox: a Managed C# compile or run. The server snapshot restore order is pinned by an Engine unit test, not by the sandbox.

## Source paths inspected

- `Examples/MechanicsSandbox/Scripts/Recipes/TimedEffect.fos`
- `Examples/MechanicsSandbox/Scripts/Tests/Test_TimedEffect.fos`
- `Examples/MechanicsSandbox/Managed/Recipes/TimedEffect.cs`
- `Examples/MechanicsSandbox/Content/TimedEffect.fopro`
- `Examples/MechanicsSandbox/mechanics-smoke.json`
- `Source/Common/TimeEvents.cpp`
- `Source/Server/Server.cpp`
- `Source/Server/Server.h`
- `Source/Server/EntityManager.cpp`
- `Docs/generated/api.json`

## Problem

A boost, a cooldown or a temporary penalty lasts a fixed time. The player expects it to end on time, not to stay forever because the server restarted while it was active. A time event alone cannot carry that promise: time events are runtime state, and neither the startup world restore nor `Game.LoadCritter` restores them ([Time events after a database reload](../scripting/lifecycle-and-concurrency.md#time-events-after-a-database-reload)). The duration, the size of the boost and how boosts stack are game policy and stay in the game project.

## Engine primitives used

| Symbol | Side | Role in the recipe |
| --- | --- | --- |
| `script.event.server.Game.OnCritterInit` | server | The re-arm hook: `firstTime == false` on both restore paths |
| `script.method.server.Entity.StartTimeEvent#1e937a7976df63ac` | server | `StartTimeEvent(timespan delay, callback(void,Entity,TimeEventContext) func)` arms the recipe timer |
| `script.method.server.Entity.StartTimeEvent#e59548a665ddb838` | server | `StartTimeEvent(timespan delay, callback(void,Entity) func)` arms the naive control timer |
| `script.method.server.Entity.CountTimeEvent#417e7527b8152349` | server | The guard; `excludeExecuting = false` by default also counts a callback that is running |
| `script.ref-type.common.TimeEventContext` | common | The firing event, passed to the callback |
| `script.ref-method.common.TimeEventContext.Repeat` | common | Waits again for a deadline that moved later |
| `script.ref-method.common.TimeEventContext.Stop` | common | Removes the event after the callback completes |
| `script.property.Game.SynchronizedTime` | common | The clock of the deadline; the server persists and resumes it |
| `script.method.server.Entity.MakePersistent` | server | Gives the test critter its own record |
| `script.method.server.Game.UnloadCritter` | server | Removes the critter and clears its time events |
| `script.method.server.Game.LoadCritter#f50433829d33781c` | server | Restores it and runs `OnCritterInit(cr, false)` |

`[[TimeEvent]]`, `[[Event]]`, `[[PropertySetter]]`, `Game.AddPropertySetter`, `ZERO_SYNCTIME` and `ZERO_TIMESPAN` are backend built-ins, and the `///@ Property` line is project metadata. Deliberately not used: `Game.OnCritterLoad`, which never runs for critters restored with their maps at startup.

## Minimal example

Under `Examples/MechanicsSandbox/`: `Scripts/Recipes/TimedEffect.fos` is the recipe, `Scripts/Tests/Test_TimedEffect.fos` holds its tests and the naive variant, `Content/TimedEffect.fopro` the `TimedEffect_Npc` prototype, and `Managed/Recipes/TimedEffect.cs` the Managed C# shape. [Test route](#test-route) has the commands.

## Step by step

Store the absolute deadline, not a remaining duration. `synctime` is compared with `Game.SynchronizedTime`; `PublicSync` lets clients that see the critter show the remaining time without a timer of their own:

```angelscript
///@ Property Critter Common synctime BoostEndTime Mutable Persistent PublicSync

#if SERVER

[[ModuleInit]]
void InitTimedEffect()
{
    Game.OnCritterInit.Subscribe(OnCritterInit);
}
```

The prototype needs nothing for the effect; the deadline starts at its declaration default, `ZERO_SYNCTIME`, which means no boost:

```ini
# A character that carries a timed boost
[ProtoCritter]
$Name = TimedEffect_Npc
$Text engl = "Boosted runner"
$Text russ = "Ускоренный бегун"
```

Starting and extending the boost are one operation: write the deadline, then make sure a timer exists. The deadline is the truth; the timer only delivers the expiry:

```angelscript
// Starts the boost or moves its deadline; an armed timer reads the new deadline when it fires
void ApplyBoost(Critter cr, timespan duration)
{
    cr.BoostEndTime = Game.SynchronizedTime + duration;
    ArmBoostTimer(cr);
}

bool IsBoosted(Critter cr)
{
    return cr.BoostEndTime != ZERO_SYNCTIME;
}
```

Re-arm on restore in `Game.OnCritterInit` with `firstTime == false`. The sandbox counts the events: a map critter restored at server start receives `OnCritterInit(cr, false)` once and `OnCritterLoad` never, while `Game.LoadCritter` fires each once. `OnCritterLoad` would therefore miss every restart:

```angelscript
// Time events are not restored from the database: a restored critter re-arms from its saved deadline
[[Event]]
void OnCritterInit(Critter cr, bool firstTime)
{
    if (!firstTime && IsBoosted(cr)) {
        ArmBoostTimer(cr);
    }
}
```

Guard every start with `CountTimeEvent`, so a refresh, a repeated init or any later start path cannot schedule a second timer. A deadline that already passed arms a zero delay, which the Engine raises to its 1-millisecond minimum (`TimeEventManager::StartTimeEventWithId` in `Source/Common/TimeEvents.cpp`), so the boost expires on the next dispatch:

```angelscript
// At most one timer per critter, whichever path asks first
void ArmBoostTimer(Critter cr)
{
    if (cr.CountTimeEvent(OnBoostTimer) != 0) {
        return;
    }

    cr.StartTimeEvent(RemainingTime(cr), OnBoostTimer);
}

timespan RemainingTime(Critter cr)
{
    timespan remaining = cr.BoostEndTime - Game.SynchronizedTime;
    return remaining > ZERO_TIMESPAN ? remaining : ZERO_TIMESPAN;
}
```

The callback re-reads the deadline. `Repeat` keeps the same event waiting for an extended deadline; it also turns the one-shot event into a repeating one, so every final path calls `Stop` ([TimeEventContext completion](../scripting/lifecycle-and-concurrency.md#time-event-completion)):

```angelscript
[[TimeEvent]]
void OnBoostTimer(Critter cr, TimeEventContext timer)
{
    // A second timer, for example from a start path that skipped the guard, finds the boost over and stops
    if (!IsBoosted(cr)) {
        timer.Stop();
        return;
    }

    // A deadline moved later keeps the same timer waiting for the rest of the time
    timespan remaining = RemainingTime(cr);
    if (remaining > ZERO_TIMESPAN) {
        timer.Repeat(remaining);
        return;
    }

    // Repeat made the event periodic, so the last firing stops it explicitly
    timer.Stop();
    cr.BoostEndTime = ZERO_SYNCTIME;
    Game.Log("timed_effect_boost_expired critter=" + cr.Id.value);
}
```

### The naive variant fails

`Scripts/Tests/Test_TimedEffect.fos` keeps the shape that looks right and is wrong. Its timer starts only in a post-set property callback, and a database load assigns nothing, so the callback never runs again:

```angelscript
// Naive variant: the timer starts only when the deadline is assigned, and a database load assigns nothing
[[PropertySetter]]
void OnNaiveBoostEndTimeChanged(Critter cr)
{
    if (cr.NaiveBoostEndTime != ZERO_SYNCTIME) {
        cr.StartTimeEvent(cr.NaiveBoostEndTime - Game.SynchronizedTime, OnNaiveBoostExpired);
    }
}

[[TimeEvent]]
void OnNaiveBoostExpired(Critter cr)
{
    cr.NaiveBoostEndTime = ZERO_SYNCTIME;
}
```

The same critter carries both variants through an unload and a load; both deadlines come back, only the recipe timer does:

```angelscript
    Critter restored = Game.LoadCritter(crId, false, map);
    Testing::Expect(ctx, TimedEffect::IsBoosted(restored), "The deadline comes back from the database");
    Testing::ExpectEq(ctx, restored.CountTimeEvent(TimedEffect::OnBoostTimer), 1, "OnCritterInit re-armed the recipe timer");
    Testing::Expect(ctx, restored.NaiveBoostEndTime != ZERO_SYNCTIME, "The naive deadline comes back too");
    Testing::ExpectEq(ctx, restored.CountTimeEvent(OnNaiveBoostExpired), 0, "Nothing re-armed the naive timer");
```

After a real restart the naive boost is still active past its deadline, while the recipe boost expires. The naive setter has a second defect visible in its code: it starts another timer on every new deadline without a guard, and the older timer then clears an extended boost early.

## Authority and sync

- The server owns the deadline and the timer. Clients read `BoostEndTime` through `PublicSync` and compare it with their copy of `Game.SynchronizedTime`; they never arm gameplay timers.
- A time event belongs to its entity. The server time-event job covers that entity before it calls the callback (`ServerEngine::TimeEventJob` in `Source/Server/Server.cpp`), so `OnBoostTimer` may touch `cr`; touching another entity needs a complete `Game.Sync` cover.
- `OnCritterInit` enters with its critter covered, as every init callback does ([entity `InitScript` callbacks](../scripting/lifecycle-and-concurrency.md#entity-initscript-callbacks)).
- `StartTimeEvent` rejects destroyed and destroying entities ([events and time events](../../explanation/entity-and-property-model/index.md#events-and-time-events)), so arming from a destroy or unload path is an error, not a deferred start.

## Persistence and migration

| Survives a restart | Does not survive |
| --- | --- |
| `BoostEndTime`, a persistent property of a persistent critter | The time event: shutdown, `Game.UnloadCritter` and destruction clear it, and no database load restores it |
| `Game.SynchronizedTime`, persisted by the server and resumed at startup, so downtime does not consume the boost | A deadline kept only in a script variable or in a temporary property |

The restart scenarios check exactly this: after phase 2 starts on phase 1's JSON database, the remaining time is above zero and at most the full duration, one timer exists, and the boost then expires. A graceful shutdown writes the exact clock (`ServerEngine::FlushExactSyncTime` in `Source/Server/Server.cpp`). While running, the server stores the clock with a lead of `SyncTimePersistLead`, 10 seconds (`Source/Server/Server.h`), so after a crash the clock resumes up to 10 seconds ahead and a deadline arrives that much earlier; this case is not tested.

A server snapshot restore is the one path that brings time events back. `ServerEngine::RestoreSnapshotTimeEvents` (`Source/Server/Server.cpp`) restarts an entity's captured timers after the world is loaded and before that entity's init events, so the `CountTimeEvent` guard at init sees the captured timer and the re-arm does nothing; an Engine unit test pins exactly one timer for that case. `OnBoostTimer` still stops a second timer that finds no boost, so a start path that skipped the guard cannot leave a stale timer behind. A snapshot also records the callback by name and signature and fails to restore when it no longer resolves (`TimeEventManager::RestartTimeEvent` in `Source/Common/TimeEvents.cpp`), so renaming `OnBoostTimer` invalidates captured snapshots. The sandbox does not run a snapshot restore.

Renaming the persisted deadline follows [Declare a rule for every rename, removal and move](../project/data-migrations.md#declare-a-rule-for-every-rename-removal-and-move):

```text
///@ MigrationRule Property Critter Rename BoostEndTime BoostDeadline
```

Changing its type, for example from a stored `timespan` to a `synctime` deadline, is a replacement, not a rename: a new property name, a `Remove` tombstone for the old one and a value carry-over tested on isolated data ([Never reuse a retired identity](../project/data-migrations.md#never-reuse-a-retired-identity)).

## Pitfalls and anti-patterns

- Starting the timer only where the effect starts, in a setter or in the action, loses it on every reload. The negative control shows it after `Game.LoadCritter` and after a real restart ([Time events after a database reload](../scripting/lifecycle-and-concurrency.md#time-events-after-a-database-reload)).
- Re-arming in `Game.OnCritterLoad` misses every startup restore. Its only fire site is `ServerEngine::LoadCritter` (`Source/Server/Server.cpp`), and the sandbox counts zero calls for a restored map critter. Use `Game.OnCritterInit` with `firstTime == false`.
- A start without the `CountTimeEvent` guard schedules duplicates as soon as a second path starts the same effect. A snapshot restore does not break the guard: it restarts captured entity timers before the init events (the startup sequence in `Source/Server/Server.cpp` calls `RestoreSnapshotTimeEvents` between `EntityMngr.LoadEntities()` and `InitLoadedEntities()`), so the callback needs to tolerate a second timer only when some other start path skips the guard.
- `Repeat` without `Stop` on the final firing leaves a periodic event. Once `Repeat` changed the interval, a successful call schedules the next firing (`TimeEventManager` completion in `Source/Common/TimeEvents.cpp`, the `IsRepeatChanged` and `RepeatDuration` branch), so a callback that only returns keeps firing.
- Storing a remaining duration instead of an absolute deadline needs a rewrite on every tick and drifts across restarts. An absolute `synctime` deadline stays correct because the server resumes synchronized time.
- `ApplyBoost` can only move a deadline later. A shorter new deadline keeps the existing timer, which fires at the old time and only then expires the boost; a project that shortens effects stops the timer with `StopTimeEvent` and arms it again.

## Test route

Run every sandbox scenario from `Examples/MechanicsSandbox` with `python validate.py` on Windows (PowerShell) or `python3 validate.py` on Linux (bash); its `README.md` lists the prerequisites. `run_mechanics_smoke.py` deletes the restart database directory first, so phase 1 always starts empty. The run passes when `BuildTools/gameplay_test_runner.py` reports `status=passed` for every scenario and exits with `0`. This recipe's scenarios require, among others:

```text
[Testing] PASS timed_effect.unload_and_load_rearm_only_the_guarded_timer
[Testing] PASS timed_effect_restart_write.start_boosts_before_restart
[Testing] PASS timed_effect_restart_verify.rearmed_timer_expires_after_restart
[Testing] PASS timed_effect_restart_verify.naive_variant_loses_timer_after_restart
[Testing] COMPLETE status=passed
timed_effect_boost_expired critter=
```

To run the in-process suite against an existing bake, from the build directory (PowerShell; on Linux use `./Binaries/Server-Linux-x64/FOMECH_ServerHeadless`):

```powershell
& ".\Binaries\Server-Windows-win64\FOMECH_ServerHeadless.exe" -ApplyConfig "<sandbox>\FOnlineMechanicsSandbox.fomain" -ApplySubConfig RecipeTests -Testing.Filter "timed_effect."
```

For the restart pair use `-ApplySubConfig RecipeRestart -Server.DbStorage "JSON <empty-directory>"` with the filters `timed_effect_restart_write.` and then `timed_effect_restart_verify.` on the same directory.

## Managed C#

Status `shape`: an Engine test parses `Managed/Recipes/TimedEffect.cs` and compares its `///@` line with the AngelScript module; no Engine lane compiles or runs it. The flow is the same; `default(synctime)` and `default(timespan)` replace the AngelScript zero constants. The guard depends on callback identity:

```csharp
    // At most one timer per critter, whichever path asks first; the handler is passed by method group,
    // so counting and starting name the same callback
    private static void ArmBoostTimer([RequiresCover] Critter cr)
    {
        if (cr.CountTimeEvent(OnBoostTimer) != 0) {
            return;
        }

        cr.StartTimeEvent(RemainingTime(cr), OnBoostTimer);
    }
```

A delegate is part of the time-event registration, and a separately constructed delegate, such as a lambda or a closure, does not identify the existing callback ([timers in Managed C#](../scripting/managed-csharp.md#events-callbacks-timers-and-named-calls)). A delegate-bound time event also blocks a server snapshot capture ([backend-consistent snapshots](../../explanation/persistence/index.md#backend-consistent-snapshots)). Keep the callback a static `[TimeEvent]` method, `internal` because the Engine dispatches it, and pass it by method group.

## How a larger project extends it

- **Data-driven catalogs.** Durations, magnitudes and stacking rules of many effects are authored rows, for example a [FixedType catalog](../../explanation/entity-and-property-model/choosing-a-representation.md#fixedtype-catalog-row) keyed by effect id. The critter stores only what changes: the deadline per active effect, in one property per effect or in a RefType list written with read, change and assign back, and one guarded timer that wakes for the earliest deadline.
- **Components.** When only some kinds of critter can carry an effect, group its fields under a [property component](../content/prototype-format.md#property-components) instead of adding them to every critter.
- **Project events with the producer rule.** Fire one project event where the expiry happens, in the timer callback, so every declared event has a producer and other modules react without polling. The declaration shape, not part of the sandbox:

```text
///@ Event Server Game OnBoostExpired(Critter cr)
```

- **What stays in the game project.** Effect kinds, durations, magnitudes, how effects combine, their texts and icons. The Engine supplies persistent properties, synchronized time and time events; the restore hook, the guard and the deadline check are the project's code.

## See also

- [Build Game Mechanics](index.md)
- [Custom persistent character state](persistent-state.md)
- [Script Lifecycle and Concurrency: time events after a database reload](../scripting/lifecycle-and-concurrency.md#time-events-after-a-database-reload)
- [Server runtime: initialization and server jobs](../../explanation/runtime/server.md#initialization-and-server-jobs)
- [Persistence](../../explanation/persistence/index.md)
- [Migrate Saved Data and Content in a Game Project](../project/data-migrations.md)
- [Managed C# Scripting](../scripting/managed-csharp.md)
- [Script Test Harness Pattern](../project/script-test-harness.md)
