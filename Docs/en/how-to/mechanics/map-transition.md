---
layout: default
title: A trigger that sends a critter to another map
locale: en
document_id: recipe-map-transition
permalink: /Docs/en/how-to/mechanics/map-transition.html
recipe_kind: primitive
---

# A trigger that sends a critter to another map

This recipe builds an exit pad: a static item on the floor of one map that sends every critter that steps onto it to a hex on another map. The pad carries its destination as data, and a key can lock it. A transfer cannot run inside the trigger callback, because the Engine calls the trigger with only the walking map covered; the recipe starts a zero-delay time event that covers the critter, both maps and the destination location and then calls `TransferToMap`. The sandbox keeps the transfer-inside-the-trigger version as a negative control and shows its failure.

| Backend | Status |
| --- | --- |
| AngelScript | `runtime-verified` |
| Managed C# | `shape` |

Runtime-verified means that the in-process scenario `map-transition`, the real-client scenario `map-transition-client` and the two-phase restart scenarios `map-transition-restart-write` and `map-transition-restart-verify` passed on a built and baked sandbox on a Windows host. Not verified: a Managed C# compile or run, dynamic (non-static) trigger items, global-map transfers and a destination chosen among several instances of one map prototype.

## Source paths inspected

- `Examples/MechanicsSandbox/Scripts/Recipes/MapTransition.fos`
- `Examples/MechanicsSandbox/Scripts/Tests/Test_MapTransition.fos`
- `Examples/MechanicsSandbox/Managed/Recipes/MapTransition.cs`
- `Examples/MechanicsSandbox/Content/MapTransition.fopro`
- `Examples/MechanicsSandbox/Maps/MapTransition_Yard.fomap`
- `Examples/MechanicsSandbox/FOnlineMechanicsSandbox.fomain`
- `Examples/MechanicsSandbox/mechanics-smoke.json`
- `Source/Server/Map.cpp`
- `Source/Server/MapManager.cpp`
- `Source/Server/StaticMap.cpp`
- `Source/Server/Server.cpp`
- `Source/Common/TimeEvents.cpp`
- `Docs/generated/api.json`

## Problem

Walking onto a hex must start a map change, and the Engine's hook for "a critter stepped here" is a static trigger item. The transfer is the hard part: it changes the source map, the destination map and the destination location, while the trigger runs inside the walking step. Which pads exist, where they lead, what opens them and what the player sees during the change are game policy; the cover, the order of checks and the persistence consequences are what the Engine contract decides.

## Engine primitives used

| Symbol | Side | Role in the recipe |
| --- | --- | --- |
| `script.property.Item.IsTrigger` | server | Includes the pad in trigger lookup and movement-trigger processing |
| `script.property.Item.TriggerScript` | server | Names the `[[ItemTrigger]]` function the Engine calls for each entry and exit |
| `script.property.Item.NoBlock` | server | Must be true on the pad, or the walk cannot end on its hex |
| `script.property.Item.ShootThru` | server | Must be true too; a pad that stops shots also blocks movement |
| `script.event.server.Game.OnStaticItemWalk` | server | Fires for the same crossing after the trigger script; the test counts it |
| `script.method.server.Entity.StartTimeEvent#b494dfd4a4fa81ea` | server | `StartTimeEvent(timespan delay, callback(void,Entity,any[]) func, any[] data)` carries the destination to the deferred half |
| `script.method.server.Game.GetMap#28a9744f2e2f208e` | server | `GetMap(hstring mapPid, int32 skipCount = 0)` resolves the destination by prototype id |
| `script.method.server.Critter.GetMap` | server | Names the source map |
| `script.method.server.Game.Sync#a0c5737d3ba111b8` | server | `Sync(Entity[] entities)` covers critter, source and destination |
| `script.method.server.Game.SyncWiden` | server | Adds the destination location to the held cover |
| `script.method.server.Map.GetLocation` | server | Names the location a covered map belongs to |
| `script.method.server.Critter.TransferToMap#c9880f573836f54f` | server | `TransferToMap(Map map, mpos hex)` moves the critter |
| `script.method.server.Critter.MapTransfersLocked` | server | True while a transfer of the critter runs |
| `script.event.server.Game.OnCritterTransfer` | server | Fires once after a completed transfer with the previous map |
| `script.method.server.Entity.MakePersistent` | server | Gives a test critter or location its own record |
| `script.method.server.Entity.IsPersistent` | server | The test reads whether a critter kept its record |
| `script.method.server.Game.LoadCritter#e4b9bdc5bdeb7919` | server | `LoadCritter(ident crId, bool forPlayer)` restores a record onto the global map |
| `script.method.server.Game.LoadCritter#f50433829d33781c` | server | `LoadCritter(ident crId, bool forPlayer, Map map)` restores it onto a map |

`[[ItemTrigger]]`, `[[TimeEvent]]`, `[[Async]]`, `StaticItem`, `Entity.IsDestroyed` and `ZERO_TIMESPAN` are backend built-ins, and the `///@ Property` lines are project metadata.

## Minimal example

Under `Examples/MechanicsSandbox/`: `Scripts/Recipes/MapTransition.fos` is the recipe, `Scripts/Tests/Test_MapTransition.fos` holds its tests and the naive pad, `Content/MapTransition.fopro` has the pad, the naive pad, the key and a courier prototype, `Maps/MapTransition_Yard.fomap` places six static pads, and `Maps/MapTransition_Hall.fomap` and `Maps/MapTransition_Cellar.fomap` are the destinations. `Managed/Recipes/MapTransition.cs` is the Managed C# shape. [Test route](#test-route) has the commands.

## Step by step

Declare the exit data as server properties of the pad item. They are set per placement, so one prototype serves every exit:

```angelscript
///@ Property Item Server hstring ExitMap
///@ Property Item Server mpos ExitHex
///@ Property Item Server bool ExitNeedsKey
```

The prototype names the trigger function and turns the item into a trigger. `NoBlock` and `ShootThru` are required: the Engine marks a hex as move-blocked when a static item lacks `NoBlock` and as both shoot- and move-blocked when it lacks `ShootThru` (`StaticMap::ApplyItemToField` in `Source/Server/StaticMap.cpp`), so a walk to such a pad hex ends at once as `HexBusy`:

```ini
[ProtoItem]
$Name = MapTransition_Pad
$Text engl = "Exit pad"
$Text russ = "Площадка выхода"
NoBlock = True
ShootThru = True
IsTrigger = True
TriggerScript = MapTransition::OnExitPad
```

A placement in the map file supplies the destination, and `ExitNeedsKey` locks the pad:

```ini
[MapTransition_Yard/Item]
$Id = 3
$Proto = MapTransition_Pad
Hex = 30 29
Static = True
ExitMap = MapTransition_Hall
ExitHex = 10 10
ExitNeedsKey = True
```

The trigger is called for every entry and every exit of the hex, in the walking step, after the critter's hex was changed (`Map::VerifyTrigger` in `Source/Server/Map.cpp`, called from `ServerEngine::ProcessCritterMovingBySteps`). Ignore exits, apply the rule that needs only the critter and the pad, and hand the rest to a time event. The destination travels as timer data, so nothing but values crosses into the deferred half:

```angelscript
// The pad names this function in its TriggerScript; the engine calls it once for each entry and each exit of a hex
[[ItemTrigger]]
void OnExitPad(Critter cr, StaticItem pad, bool entered, mdir dir)
{
    if (!entered) {
        return;
    }

    // The rule reads only the critter and the immutable pad data, so it needs no further cover
    if (pad.ExitNeedsKey && cr.GetItem(KeyPid) == null) {
        Game.Log("map_transition_refused reason=no_key critter=" + cr.Id.value);
        return;
    }

    // A trigger runs inside the walking step with only the walking map covered; the transfer needs more, so a time event does it
    mpos exitHex = pad.ExitHex;
    any[] data = {any(pad.ExitMap), any(exitHex.x), any(exitHex.y)};
    cr.StartTimeEvent(ZERO_TIMESPAN, CompleteExit, data);
}
```

The deferred half is an `[[Async]]` time event of the critter, so the critter is covered when it starts. It resolves the maps, widens the cover in two requests and checks again after each one:

```angelscript
// The deferred half: the critter is covered at entry, the maps and the destination location are not
[[TimeEvent]] [[Async]]
void CompleteExit(Critter cr, any[] data)
{
    hstring mapPid = hstring(data[0]);
    mpos exitHex = mpos(int(data[1]), int(data[2]));

    Map? destination = Game.GetMap(mapPid);
    Map? source = cr.GetMap();
    if (destination == null || source == null) {
        Game.Log("map_transition_refused reason=no_map critter=" + cr.Id.value);
        return;
    }

    // A transfer changes both maps, so both maps and the critter are covered by one request
    Game.Sync(array<Entity> = {cr, source, destination});

    if (cr.IsDestroyed || destination.IsDestroyed) {
        Game.Log("map_transition_refused reason=gone critter=" + cr.Id.value);
        return;
    }

    // Entering a map also enters its location, which only a covered map can name; widening may hand the cover over
    Game.SyncWiden(array<Entity> = {destination.GetLocation()});

    // The world may have changed while the cover changed hands
    if (cr.IsDestroyed || destination.IsDestroyed || cr.MapId != source.Id) {
        Game.Log("map_transition_refused reason=moved critter=" + cr.Id.value);
        return;
    }

    // Another transfer of the same critter is running; it owns the critter until it finishes
    if (cr.MapTransfersLocked) {
        Game.Log("map_transition_refused reason=locked critter=" + cr.Id.value);
        return;
    }

    cr.TransferToMap(destination, exitHex);
    Game.Log("map_transition_done critter=" + cr.Id.value);
}
```

Why each step is there:

1. `Game.Sync` of critter, source and destination: `MapManager::Transfer` validates the critter and both maps (`Source/Server/MapManager.cpp`), and a transfer changes both of them.
2. `Game.SyncWiden` of the destination location: when the maps differ, `Transfer` also validates the destination's location, and `Map.GetLocation` can be read only from a covered map, so the location cannot join the first request.
3. The checks after each acquisition: both acquisitions are points where other jobs run, and a widening that adds an entity the context does not already cover releases and reacquires the whole held set ([widening and revalidation after suspension](cover-transactions.md#widening-and-revalidation-after-suspension)). The critter can be gone, the destination destroyed or the critter on another map by then.
4. The `MapTransfersLocked` check: `Transfer` throws `Critter transfers locked` when another transfer of the critter is running, so the recipe refuses instead.

### The naive variant fails

`MapTransition_NaivePad` in `Content/MapTransition.fopro` names a trigger script from `Scripts/Tests/Test_MapTransition.fos` that calls `TransferToMap` directly inside the walking step. The walking step covers the walking critter and its map, so the destination map is uncovered:

```angelscript
    try {
        cr.TransferToMap(hall, ExitHex);
    }
    catch {
        error = GetExceptionInfo();
    }
```

The test requires the server marker `locked=false uncovered=true`: the transfer threw `Entity access without sync`, the critter was not locked at that moment, so the failure comes from the missing cover and not from the lock. The critter stays on its pad on the source map and no `OnCritterTransfer` fires.

## Authority and sync

- The client never asks for a transition. It walks through the ordinary movement protocol, the server walks the critter onto the pad, and the pad's destination properties are `Server` properties that no client reads. After the transfer the client reloads the map and reports it; the real-client scenario waits for that report.
- The trigger runs under the walking step's cover: the movement job covers the critter and its parent map (`ServerEngine::CritterMovingJob` in `Source/Server/Server.cpp`). The recipe changes none of that in the trigger; it reads the critter and the pad and starts a time event.
- The time event covers its critter and nothing else, so the recipe acquires the rest itself. The order is fixed by what is readable: the destination map first, then the location that only that map can name.
- `OnCritterTransfer` fires once per completed transfer; for a player it fires twice in the sandbox, once for the login placement and once for the pad, and the second call reports the yard as `prevMap`.

## Persistence and migration

A critter's record follows its destination map. `Map::AddCritter` makes a critter persistent when its map is, and `Map::RemoveCritter` removes persistence again unless the critter was made persistent explicitly (`Source/Server/Map.cpp`). The restart scenarios put three couriers on a persistent yard and send them through three pads:

| Courier | After the transfer | After a restart |
| --- | --- | --- |
| Entered the persistent hall | `IsPersistent()` is true | Restored with the hall |
| Entered the temporary cellar | `IsPersistent()` is false | No record: `Game.LoadCritter(id, false)` throws `Critter data base loading error` |
| Made persistent explicitly, then entered the cellar | `IsPersistent()` is true | Not placed by the world restore, because its map was not saved; loading it onto another map is refused as `Cannot restore critter on stored map`, and `Game.LoadCritter(id, false)` loads it onto the global map |

The assertions behind the first two rows:

```angelscript
    Testing::Expect(ctx, a.IsPersistent(), "A courier that entered a persistent map keeps its record");
    Testing::Expect(ctx, !b.IsPersistent(), "A courier that entered a temporary map lost its record");
    Testing::Expect(ctx, c.IsPersistent(), "An explicitly persistent courier keeps its record in a temporary map");
```

A project that sends critters into temporary maps chooses one policy per case: let the record go, or pin the critter with `MakePersistent(true)` and own the recovery path for the case where the temporary map is gone ([restoring a critter on its stored map](../../explanation/runtime/server.md#restoring-a-critter-on-its-stored-map)).

The pad data is authored map content, so a rebake applies a change and no saved record holds it. Renaming a declared property such as `ExitMap` follows [Declare a rule for every rename, removal and move](../project/data-migrations.md#declare-a-rule-for-every-rename-removal-and-move); the sandbox did not exercise a rename of a map-authored key.

## Pitfalls and anti-patterns

- Calling `TransferToMap` inside the trigger throws for the uncovered destination, and acquiring a wider cover inside a native walking step is not something the recipe tries. Defer to a time event, as the negative control shows.
- A pad that blocks movement or shots never fires: the path finder refuses its hex before the critter arrives. Set both `NoBlock` and `ShootThru` on every trigger item.
- Covering the destination map and forgetting its location fails inside `MapManager::Transfer`, and the generated description of `TransferToMap` does not say so. The location is named from the covered map, so the cover grows in two requests and every check repeats after each.
- `Game.GetMap(mapPid)` returns the first live map with that prototype. A project with several instances of one map prototype resolves the destination from the instance, for example the location id stored on the pad, or passes `skipCount`; the sandbox runs one instance per prototype.
- The trigger runs for every entry, and the recipe starts one time event per entry without a guard. Two entries before the first event runs would give two transfer attempts, the second from wherever the critter then is. This follows from the code and was not run; a project that cares guards the start with `CountTimeEvent`.
- A critter whose record exists only because its map is persistent loses that record when it enters a temporary map, and nothing reports it. Decide the policy before the first temporary destination exists.

## Test route

Run every sandbox scenario from `Examples/MechanicsSandbox` with `python validate.py` on Windows (PowerShell) or `python3 validate.py` on Linux (bash); its `README.md` lists the prerequisites. The four scenarios of this recipe require, among others:

```text
[Testing] PASS map_transition.walking_onto_a_pad_transfers_the_critter
[Testing] PASS map_transition.locked_pads_need_the_key
[Testing] PASS map_transition.transfer_inside_the_walking_step_is_the_naive_variant
[Testing] PASS map_transition.persistence_follows_the_destination_map
[Testing] PASS map_transition_client.player_walks_onto_the_pad_and_the_client_reloads
[Testing] PASS map_transition_restart_verify.persistence_decided_by_the_destination_survives_restart
map_transition_refused reason=no_key
locked=false uncovered=true
error=GenericException: Critter data base loading error
error=GenericException: Cannot restore critter on stored map
```

The client scenario starts the server with the `RecipeNetworkTests` sub-config (`Testing.Enabled`, `Testing.TestTimeout = 60000`, `Server.DbStorage = Memory`), waits for `map_transition_waiting_for_client` and starts a headless client with the `RecipeClient` sub-config. The test waits for the player's critter on the yard, walks it to the hall pad and requires two transfers, the second from the yard, and the client's `map_transition_client_map_loaded` report.

To run the in-process suite against an existing bake, from the build directory (PowerShell; on Linux use `./Binaries/Server-Linux-x64/FOMECH_ServerHeadless`):

```powershell
& ".\Binaries\Server-Windows-win64\FOMECH_ServerHeadless.exe" -ApplyConfig "<sandbox>\FOnlineMechanicsSandbox.fomain" -ApplySubConfig RecipeTests -Testing.Filter "map_transition."
```

The restart pair uses `-ApplySubConfig RecipeRestart -Server.DbStorage "JSON <empty-directory>"` with the filters `map_transition_restart_write.` and then `map_transition_restart_verify.` on the same directory.

## Managed C#

Status `shape`: an Engine test parses `Managed/Recipes/MapTransition.cs` and compares its `///@` lines with the AngelScript module; no Engine lane compiles or runs it. The trigger is an `[ItemTrigger]` method and the deferred half a `[TimeEvent]` method, both `internal` because the Engine dispatches them. The two covers become awaited helpers that return `false` when an entity is gone, so each is a condition of its own; the widening reads:

```csharp
        // Entering a map also enters its location, which only a covered map can name; widening may hand the cover over
        if (!await Sync.Widen(destination.GetLocation())) {
            return;
        }
```

`Sync.Lock` and `Sync.Widen` wrap `Game.Sync` and `Game.SyncWiden`; the analyzer rules for them are in [Lock several entities for one action](cover-transactions.md#managed-c-equivalent).

## How a larger project extends it

- **Data-driven exits.** Replace the three pad properties with a key into an authored catalog, for example a [FixedType catalog](../../explanation/entity-and-property-model/choosing-a-representation.md#fixedtype-catalog-row) row holding destination, hex, key and cost, so exits are edited as data and the pad stores one id.
- **Instanced destinations.** Create the destination location when the first critter arrives and transfer into its map; the tests create their locations the same way. Which instance a party enters and when it is destroyed are project policy that this recipe does not model.
- **Followers.** `MapManager::Transfer` re-attaches and moves attached critters after the leader; party and follower rules are project policy and were not exercised here.
- **A project event with a producer.** Fire one project event where the transfer happens, so other modules react without polling. The declaration shape, not part of the sandbox:

```text
///@ Event Server Game OnMapTransition(Critter cr, Map? from, Map to)
```

- **What stays in the game project.** Which pads exist, their keys and costs, what the player sees during the change, and the rule for temporary destinations. The Engine supplies the trigger call, the transfer and the persistence behavior; the cover sequence is the project's code.

## See also

- [Build Game Mechanics](index.md)
- [Lock several entities for one action](cover-transactions.md)
- [Timed effect that survives a restart](timed-effect.md)
- [Script Lifecycle and Concurrency: server entity synchronization](../scripting/lifecycle-and-concurrency.md#server-entity-synchronization)
- [Maps, Movement, and Geometry: blocking model](../../explanation/maps-and-movement.md#blocking-model)
- [Server runtime: restoring a critter on its stored map](../../explanation/runtime/server.md#restoring-a-critter-on-its-stored-map)
- [Persistence](../../explanation/persistence/index.md)
- [Managed C# Scripting](../scripting/managed-csharp.md)
- [Script Test Harness Pattern](../project/script-test-harness.md)
