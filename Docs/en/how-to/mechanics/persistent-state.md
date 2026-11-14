---
layout: default
title: Custom persistent character state
locale: en
document_id: recipe-persistent-state
permalink: /Docs/en/how-to/mechanics/persistent-state.html
recipe_kind: primitive
---

# Custom persistent character state

This recipe adds a value that the server owns, saves with the character and returns after a reconnect or a server restart: a visit counter stamped once at creation and changed through one writer. It shows which property flags make that happen, which Engine hook runs on which restore path, which records the Engine deletes without asking, and the two-phase test that proves the restart.

| Backend | Status |
| --- | --- |
| AngelScript | `runtime-verified` |
| Managed C# | `shape` |

Runtime-verified means that the in-process scenario `persistent-state` on a memory database and the two-phase restart scenarios `persistent-state-restart-write` and `persistent-state-restart-verify` against one JSON database directory passed on a built and baked sandbox on a Windows host. Not verified: a Managed C# compile or run, the server snapshot restore path, and the SQLite and MongoDB backends.

## Source paths inspected

- `Examples/MechanicsSandbox/Scripts/Recipes/PersistentState.fos`
- `Examples/MechanicsSandbox/Scripts/Sandbox.fos`
- `Examples/MechanicsSandbox/Scripts/Tests/Test_PersistentState.fos`
- `Examples/MechanicsSandbox/Managed/Recipes/PersistentState.cs`
- `Examples/MechanicsSandbox/Content/PersistentState.fopro`
- `Examples/MechanicsSandbox/FOnlineMechanicsSandbox.fomain`
- `Examples/MechanicsSandbox/mechanics-smoke.json`
- `Source/Server/Server.cpp`
- `Source/Server/EntityManager.cpp`
- `Source/Server/Map.cpp`
- `Source/Server/DataBase.cpp`
- `Source/Common/Settings.inc`
- `Docs/generated/api.json`

## Problem

A character needs state of its own: a counter, a first-seen time, a flag that a conversation has happened. The player expects it after a reconnect, and the operator expects it after a restart. The Engine stores persistent properties of persistent entities; the script decides the flags, initializes the value once, writes it from one place and proves the restart. What the value means, what counts as a visit and what it unlocks, is game policy and stays in the game project.

## Engine primitives used

| Symbol | Side | Role in the recipe |
| --- | --- | --- |
| `script.event.server.Game.OnCritterInit` | server | Stamps a new critter once (`firstTime == true`); runs with `firstTime == false` on every restore path |
| `script.event.server.Game.OnCritterLoad` | server | Fires only after `Game.LoadCritter`; the tests count it to show it is not a restore hook |
| `script.property.Game.SynchronizedTime` | common | The game clock that the server persists; the creation stamp reads it |
| `script.method.server.Entity.MakePersistent` | server | Gives an entity and its children a database record |
| `script.method.server.Entity.IsPersistent` | server | Test check that the record exists |
| `script.method.server.Game.UnloadCritter` | server | Removes a live critter; with `Game.LoadCritter` it replays a restore in one process |
| `script.method.server.Game.LoadCritter#f50433829d33781c` | server | `Critter Game.LoadCritter(ident crId, bool forPlayer, Map map)` restores a stored critter onto its map |
| `script.method.server.Game.GetLocations#8eb7a49e747de27a` | server | Finds the saved location after the restart |
| `setting.server.Server.DbStorage` | server setting | `Memory` for in-process tests, `JSON <directory>` for the restart |

The `///@ Property` lines are project metadata, and `[[ModuleInit]]`, `[[Event]]`, `Game.OnCritterInit.Subscribe`, `Entity.Id` and `ZERO_SYNCTIME` are backend built-ins; neither is in the API model. There is no save event: a persistent property of a persistent entity is queued for the database when its value changes (`ServerEngine::OnSaveEntityValue` in `Source/Server/Server.cpp`). `Game.OnCritterPreLoad` exists, but it is the [migration hook for persisted critters](../scripting/lifecycle-and-concurrency.md#persistence-preload-boundary), not a place for gameplay initialization.

## Minimal example

Under `Examples/MechanicsSandbox/`: `Scripts/Recipes/PersistentState.fos` holds the declarations, the init stamp and the single writer; `Scripts/Sandbox.fos` the shared location, map and headless-client login; `Scripts/Tests/Test_PersistentState.fos` the in-process tests and both restart phases; `Content/PersistentState.fopro` the `PersistentState_Npc` prototype; `FOnlineMechanicsSandbox.fomain` the `RecipeTests` and `RecipeRestart` sub-configs; `Managed/Recipes/PersistentState.cs` the Managed C# shape. [Test route](#test-route) has the commands.

## Step by step

Declare the state. `VisitCount` is visible to the controlling client; `FirstVisitTime` never leaves the server. Both are `Mutable Persistent`, which the baker requires for a value saved per entity ([property flags](../../explanation/entity-and-property-model/choosing-a-representation.md#prototype-property-and-property-component)):

```angelscript
///@ Property Critter Common int32 VisitCount Mutable Persistent OwnerSync
///@ Property Critter Server synctime FirstVisitTime Mutable Persistent
```

Initialize in `Game.OnCritterInit`, and only when `firstTime` is true. A restored critter already carries what the database returned, so an unconditional assignment here would overwrite saved data on every load:

```angelscript
[[ModuleInit]]
void InitPersistentState()
{
    Game.OnCritterInit.Subscribe(OnCritterInit);
}

// firstTime is true only for a new critter; a restored critter keeps what the database returned
[[Event]]
void OnCritterInit(Critter cr, bool firstTime)
{
    if (firstTime) {
        cr.FirstVisitTime = Game.SynchronizedTime;
    }
}
```

Write the value from one function. A property read returns a copy; the change becomes state, and is queued for the database, only when it is assigned back:

```angelscript
// The only writer of VisitCount: read the stored value, change the copy, assign it back
int32 RecordVisit(Critter cr)
{
    int32 count = cr.VisitCount + 1;
    cr.VisitCount = count;
    return count;
}
```

The content side is an ordinary prototype; the state needs no authored value because new critters start from the declaration default:

```ini
# A character whose visit counter is saved with it
[ProtoCritter]
$Name = PersistentState_Npc
$Text engl = "Regular visitor"
$Text russ = "Постоянный посетитель"
```

A created critter is temporary. It gets a record when it, or the location that holds it, is made persistent. The in-process test then unloads it and loads it back. `Game.UnloadCritter` keeps the record of a critter that is persistent only through its location by making that persistence explicit, see [Pitfalls](#pitfalls-and-anti-patterns):

```angelscript
    location.MakePersistent(true);

    ident crId = cr.Id;
    synctime stamp = cr.FirstVisitTime;
    Game.UnloadCritter(cr);

    Critter restored = Game.LoadCritter(crId, false, map);
    Testing::ExpectEq(ctx, restored.VisitCount, 2, "The count comes back from the database");
    Testing::Expect(ctx, restored.FirstVisitTime == stamp, "A restore keeps the creation stamp");
    Testing::ExpectEq(ctx, restored.TestRestoreInitCount, 1, "Game.LoadCritter runs OnCritterInit(cr, false)");
    Testing::ExpectEq(ctx, restored.TestLoadEventCount, 1, "Game.LoadCritter fires OnCritterLoad");
```

A player record needs no recipe-specific preparation. `Game.LoginPlayerToNewRecord` stores the player's name with its persistent properties, so a new player whose properties all keep their defaults still gets a record:

```angelscript
    unloginedPlayer.SetName("SandboxPlayer");
    Player player = Game.LoginPlayerToNewRecord(unloginedPlayer);
```

## Authority and sync

- The server is the only writer. `RecordVisit` is the one place that changes `VisitCount`; a client that wants a visit recorded asks through a remote call that the server validates ([Remote Calls: authority and failure handling](../../reference/scripting/remote-calls.md#authority-and-failure-handling) and the [validated action recipe](validated-action.md)).
- `OwnerSync` sends the value to the client of the player that controls the critter, and `PublicSync` would also broadcast it to observers (`ServerEngine::OnSendCritterValue` in `Source/Server/Server.cpp`). Without `ModifiableByClient` the client cannot change it ([property runtime](../../explanation/entity-and-property-model/index.md#property-runtime)).
- A `Server` property is never sent and takes no sync flag. Keep facts that the client must not see, such as `FirstVisitTime` here, on the server side.
- `OnCritterInit` enters with its critter covered ([entity `InitScript` callbacks](../scripting/lifecycle-and-concurrency.md#entity-initscript-callbacks)). `RecordVisit` touches only `cr`, so its caller must hold a cover of that critter; the Managed C# shape states this with `[RequiresCover]`. The restart test calls `Game.Sync` on the restored location before it reads the critter.

## Persistence and migration

What comes back after a restart: the persistent properties of entities that had a record, and `Game.SynchronizedTime`, which the server persists and resumes. What does not: temporary entities, temporary properties and time events; a deadline needs the [timed effect recipe](timed-effect.md).

Which hook runs on which path:

| Path | `Game.OnCritterPreLoad` | `Game.OnCritterInit` | `Game.OnCritterLoad` |
| --- | --- | --- | --- |
| New critter (`Map.AddCritter`, `Game.CreateCritter`) | no | once, `firstTime == true` | no |
| Map critter restored at server start | once (source) | once, `firstTime == false` (verified) | never (verified) |
| `Game.LoadCritter` | once (source) | once, `firstTime == false` (verified) | once (verified) |

"Verified" rows are counted by the sandbox tests; "source" means `EntityManager::LoadCritter` in `Source/Server/EntityManager.cpp`, which both restore paths call. The only fire site of `OnCritterLoad` is `ServerEngine::LoadCritter` in `Source/Server/Server.cpp`. Restore code therefore belongs in `OnCritterInit` with `firstTime == false`, or in the entity's `InitScript`.

A rename of a persisted property is a compatibility change. Declare the rule in the same change and keep gameplay code on the new name only ([Declare a rule for every rename, removal and move](../project/data-migrations.md#declare-a-rule-for-every-rename-removal-and-move)):

```text
///@ MigrationRule Property Critter Rename VisitCount VisitTotal
```

A removed property keeps a permanent `Remove` tombstone, and a type change is a new name, not a rename. Prove a migration the same way as this recipe: a save written by the old declarations, a restart, and assertions on loaded values ([Test a migration](../project/data-migrations.md#test-a-migration)).

## Pitfalls and anti-patterns

- A memory database proves nothing about a restart. With `Server.DbStorage = Memory` records live as long as the process; only a second process on the same `JSON <directory>` or database proves persistence, as [Test a migration](../project/data-migrations.md#test-a-migration) says for any persisted change.
- A persistent property of a temporary entity is not written: `ServerEngine::OnSaveEntityValue` (`Source/Server/Server.cpp`) skips entities without a record. The current values reach the database only when the entity gets its record (`EntityManager::MakePersistentRecursive` stores the whole document) and are lost if it never does. A critter created by `Map.AddCritter` stays temporary until it, or its location, is made persistent; the test `new_critter_is_temporary_until_made_persistent` checks both states.
- Persistence inherited from a map ends when the critter leaves it. `Map::AddCritter` promotes a critter on a persistent map, and `Map::RemoveCritter` demotes one that is not explicitly persistent, which deletes its record (`Source/Server/Map.cpp`, `EntityManager::MakeNonPersistentRecursive` in `Source/Server/EntityManager.cpp`). A move to a temporary map therefore deletes the record, and a move to another persistent map deletes it and writes it again. `Game.UnloadCritter` is the exception: it makes such persistence explicit before it removes the critter from its map (`ServerEngine::UnloadCritter` in `Source/Server/Server.cpp`), so the record survives and the loaded critter keeps it on its own. Call `MakePersistent(true)` on a critter that must keep its record across moves.
- `Game.OnCritterLoad` is not a restore hook. It never runs for critters restored with their maps at startup ([time events after a database reload](../scripting/lifecycle-and-concurrency.md#time-events-after-a-database-reload) states the same rule), so state rebuilt only there is missing after every restart.
- `Baking.PrebakeOnStartup` defaults to `true` (`Source/Common/Settings.inc`), so every process of a multi-process test runs the baker before it starts. The sandbox bakes once and sets `Baking.PrebakeOnStartup = False`, so every phase starts on that one bake.
- A script-global dictionary keyed by entity id instead of a property separates the data from the entity's lifetime and persistence ([anti-patterns](../../explanation/entity-and-property-model/choosing-a-representation.md#anti-patterns)).

## Test route

The sub-configs select the database. In-process tests use memory; the restart pair uses a JSON directory that the runner replaces with its own through `-Server.DbStorage`:

```ini
# In-process recipe tests on an in-memory database
[SubConfig]
Name = RecipeTests
Testing.Enabled = True
ServerNetwork.DisableNetworking = True
Server.DbStorage = Memory

# Two-phase restart tests; the runner passes its own JSON directory with -Server.DbStorage
[SubConfig]
Name = RecipeRestart
Testing.Enabled = True
ServerNetwork.DisableNetworking = True
Server.DbStorage = JSON RecipeRestartDb
```

Phase 1 requires an empty database, creates the critter, records three visits, makes the location persistent and checks `cr.IsPersistent()`. Phase 2 is a new server process on the same directory. It reads what the world restore brought back and counts the hooks:

```angelscript
    Location[] locations = Game.GetLocations(Sandbox::LocationPid);
    if (!Testing::ExpectEq(ctx, locations.length(), 1, "The saved location was loaded")) {
        return;
    }

    Game.Sync(locations[0]);
    Critter[] critters = locations[0].GetMapByIndex(0).GetCritters(NpcPid, CritterFindType::Any);
    if (!Testing::ExpectEq(ctx, critters.length(), 1, "The saved critter was loaded with its map")) {
        return;
    }

    Critter cr = critters[0];
    Testing::ExpectEq(ctx, cr.VisitCount, 3, "The count survives the restart");
    Testing::Expect(ctx, cr.FirstVisitTime == cr.TestExpectedFirstVisitTime, "The restore keeps the creation stamp");
    Testing::ExpectEq(ctx, cr.TestRestoreInitCount, 1, "World loading runs OnCritterInit(cr, false) once");
    Testing::ExpectEq(ctx, cr.TestLoadEventCount, 0, "World loading does not fire OnCritterLoad for a map critter");
```

Run every sandbox scenario from `Examples/MechanicsSandbox` with `python validate.py` on Windows (PowerShell) or `python3 validate.py` on Linux (bash); its `README.md` lists the prerequisites. The run passes when `BuildTools/gameplay_test_runner.py` reports `status=passed` for every scenario and exits with `0`. The scenarios of this recipe require these markers, among others:

```text
[Testing] PASS persistent_state.unload_and_load_keep_the_state
[Testing] PASS persistent_state_restart_write.save_state_before_restart
[Testing] PASS persistent_state_restart_verify.state_survives_restart
[Testing] COMPLETE status=passed
```

To repeat the restart by hand against an existing bake, run phase 1 and then phase 2 with the same empty directory, from the build directory (PowerShell; on Linux use `./Binaries/Server-Linux-x64/FOMECH_ServerHeadless`):

```powershell
& ".\Binaries\Server-Windows-win64\FOMECH_ServerHeadless.exe" -ApplyConfig "<sandbox>\FOnlineMechanicsSandbox.fomain" -ApplySubConfig RecipeRestart -Server.DbStorage "JSON <empty-directory>" -Testing.Filter "persistent_state_restart_write."
& ".\Binaries\Server-Windows-win64\FOMECH_ServerHeadless.exe" -ApplyConfig "<sandbox>\FOnlineMechanicsSandbox.fomain" -ApplySubConfig RecipeRestart -Server.DbStorage "JSON <empty-directory>" -Testing.Filter "persistent_state_restart_verify."
```

## Managed C#

Status `shape`: an Engine test parses `Managed/Recipes/PersistentState.cs` and compares its `///@` lines with the AngelScript module, which are identical; no Engine lane compiles or runs it. The hook and the writer keep the same structure. Engine-dispatched methods are `internal` ([Managed C# authoring shape](../scripting/managed-csharp.md#authoring-shape)), and `[RequiresCover]` states the cover that the caller must hold ([server entity synchronization](../scripting/managed-csharp.md#server-entity-synchronization)):

```csharp
    [ModuleInit]
    internal static void InitPersistentState()
    {
        Game.OnCritterInit.Subscribe(OnCritterInit);
    }

    // firstTime is true only for a new critter; a restored critter keeps what the database returned
    [Event]
    internal static void OnCritterInit([RequiresCover] Critter cr, bool firstTime)
    {
        if (firstTime) {
            cr.FirstVisitTime = Game.SynchronizedTime;
        }
    }

    // The only writer of VisitCount: read the stored value, change the copy, assign it back
    public static int RecordVisit([RequiresCover] Critter cr)
    {
        int count = cr.VisitCount + 1;
        cr.VisitCount = count;
        return count;
    }
```

The persistence rules, hooks and pitfalls above are backend-neutral. A project on the managed backend must run the same two-phase restart test with its own harness before relying on this shape.

## How a larger project extends it

- **Data-driven catalogs.** Thresholds and rewards that depend on the value are authored data: one tunable scalar is a `///@ Setting`, a table of named rows is a [FixedType catalog](../../explanation/entity-and-property-model/choosing-a-representation.md#fixedtype-catalog-row). The per-character value stays a property.
- **Components.** When only some kinds of critter carry the state, group its fields under a property component with a presence marker instead of flat properties on every critter ([property components](../content/prototype-format.md#property-components)). Several values that always change together become a RefType held by one property, written with the same read, change and assign-back step as `RecordVisit`.
- **Project events with the producer rule.** When other modules react to a change, declare a project event and fire it from the single writer, so every declared event has exactly one producer and no subscriber waits for an event nothing fires. The declaration shape, not part of the sandbox:

```text
///@ Event Server Game OnVisitRecorded(Critter cr, int32 count)
```

- **What stays in the game project.** What a visit is, every number derived from the count, texts and content, and the migration rules for the project's own names. The Engine supplies the flags, the hooks, the database and the restore order.

## See also

- [Build Game Mechanics](index.md)
- [Timed effect that survives a restart](timed-effect.md)
- [Script Lifecycle and Concurrency](../scripting/lifecycle-and-concurrency.md)
- [Choose How to Represent Game Data](../../explanation/entity-and-property-model/choosing-a-representation.md)
- [Entity Model](../../explanation/entity-and-property-model/index.md)
- [Persistence](../../explanation/persistence/index.md#relationship-to-entity-state)
- [Migrate Saved Data and Content in a Game Project](../project/data-migrations.md)
- [Remote Calls](../../reference/scripting/remote-calls.md)
- [Script Test Harness Pattern](../project/script-test-harness.md)
