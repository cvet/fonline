---
layout: default
title: Item behavior selected by data
locale: en
document_id: recipe-data-driven-item
permalink: /Docs/en/how-to/mechanics/data-driven-item.html
recipe_kind: primitive
---

# Item behavior selected by data

An item should be a remedy because its prototype says so, not because a script lists prototype ids. This recipe makes the kind a property component on the item prototype, seeds the per-item state once through the prototype's `InitScript`, reads a tuning cap from a setting at use time, and adds the content check and the rename rule that keep the data honest. The game policy (what a remedy is worth, which kinds exist) stays in content and settings.

| Backend | Status |
| --- | --- |
| AngelScript | `runtime-verified` |
| Managed C# | `shape` |

Runtime-verified means that the in-process scenario `data-driven-item` on a memory database and the two-phase restart scenarios `data-driven-item-restart-write` and `data-driven-item-restart-verify` against one JSON database directory passed on a built and baked sandbox on a Windows host. Not verified: a Managed C# compile or run, and migrating a persisted database document that still carries the old property key (only the authored prototype key was exercised).

## Source paths inspected

- `Examples/MechanicsSandbox/Scripts/Recipes/DataDrivenItem.fos`
- `Examples/MechanicsSandbox/Scripts/Sandbox.fos`
- `Examples/MechanicsSandbox/Scripts/Tests/Test_DataDrivenItem.fos`
- `Examples/MechanicsSandbox/Managed/Recipes/DataDrivenItem.cs`
- `Examples/MechanicsSandbox/Content/DataDrivenItem.fopro`
- `Examples/MechanicsSandbox/FOnlineMechanicsSandbox.fomain`
- `Examples/MechanicsSandbox/mechanics-smoke.json`
- `Source/Scripting/AngelScript/AngelScriptEntity.cpp`
- `Source/Scripting/CommonGlobalScriptMethods.cpp`
- `Source/Common/Properties.cpp`
- `Source/Server/EntityManager.cpp`
- `Docs/generated/api.json`

## Problem

A game ships dozens of consumables. If the script decides what an item does by comparing prototype ids, every new item needs a code change, content cannot be validated without running the game, and a designer cannot tune a value. The wanted shape is the opposite: content declares the kind and its numbers, one script path reads them, and the only code per kind is the behavior itself. A tonic restores some vigor, has a few charges, and is consumed with the last one; a stone must pass through the same path untouched.

## Engine primitives used

| Symbol | Side | Role in the recipe |
| --- | --- | --- |
| `script.property.Item.InitScript` | server | Names the function that seeds per-item state; runs for created and restored items |
| `script.property.Item.CritterId` | common | Proves the item is in the acting critter's inventory |
| `script.method.common.Game.GetProtoItem` | common | Reads a prototype for the content check; throws for an unknown id |
| `script.method.common.Game.CheckProtoItem` | common | Probes whether a prototype id exists before reading it |
| `script.method.server.Game.DestroyItem` | server | Consumes the item with its last charge |
| `script.method.server.Critter.AddItem#e8b1d5e540061d17` | server | Creates a tonic in a critter's inventory; the test calls it, and it runs the init script |
| `script.method.server.Game.UnloadCritter` | server | Removes a live critter so the test can replay a restore in one process |
| `script.method.server.Game.LoadCritter#f50433829d33781c` | server | Restores the unloaded critter and its inventory |
| `script.method.server.Entity.IsPersistent` | server | Test check that an inventory item has its own database record |

The `///@ Property`, `///@ Setting` and `///@ MigrationRule` lines are project metadata, and the generated accessors `Item.HasRemedy`, `Item.Remedy`, `ProtoItem.HasRemedy`, `Settings.DataDrivenItem.MaxVigor` and `Entity.IsDestroyed` are backend built-ins; none of them is in the API model. The component mechanism is owned by [Prototype Format: property components](../content/prototype-format.md#property-components), the init script contract by [Prototype Format: init scripts](../content/prototype-format.md#init-scripts).

## Minimal example

Under `Examples/MechanicsSandbox/`: `Scripts/Recipes/DataDrivenItem.fos` holds the declarations, the init script, the use path and the content check; `Content/DataDrivenItem.fopro` four item prototypes and a critter; `FOnlineMechanicsSandbox.fomain` the setting value; `Scripts/Tests/Test_DataDrivenItem.fos` the scenarios; `Managed/Recipes/DataDrivenItem.cs` the Managed C# shape. [Test route](#test-route) has the commands.

## Step by step

### 1. Declare the component, the instance state and the setting

The first line is the marker: a `Common bool` with `Component`. The qualified lines below it are its fields. Marker and fields are authored, so they carry no `Mutable`; only `Remedy.ChargesLeft`, the per-item state, is `Mutable Persistent`. The critter's `Vigor` is plain persistent state, and the cap is a `Setting`, which is configuration and not state ([choosing a representation](../../explanation/entity-and-property-model/choosing-a-representation.md#setting)):

```angelscript
///@ Property Item Common bool Remedy Component
///@ Property Item Common int32 Remedy.Restore
///@ Property Item Common int32 Remedy.Charges
///@ Property Item Common int32 Remedy.ChargesLeft Mutable Persistent PublicSync
///@ Property Critter Common int32 Vigor Mutable Persistent OwnerSync
///@ Setting Server int32 DataDrivenItem.MaxVigor
///@ MigrationRule Property Item Rename RemedyRestore Remedy.Restore
```

The setting needs a value in the project configuration, or the metadata baker refuses the declaration:

```ini
DataDrivenItem.MaxVigor = 100
```

### 2. Author the kind in content

The tonic enables the marker, supplies the values and names the init script. Qualified fields alone do not enable the marker ([property components](../content/prototype-format.md#property-components)):

```ini
# The item kind is the Remedy component: values and the init script come from the prototype
[ProtoItem]
$Name = DataDrivenItem_Tonic
$Text engl = "Tonic"
$Text russ = "Тоник"
Remedy = True
Remedy.Restore = 30
Remedy.Charges = 3
InitScript = DataDrivenItem::InitRemedy
```

A stone has no component at all, and a blank tonic is the content mistake that step 5 catches:

```ini
# No component: the same use path must leave it alone
[ProtoItem]
$Name = DataDrivenItem_Stone
$Text engl = "Stone"
$Text russ = "Камень"

# A marker without values: the content check must catch it, because the defaults are zero
[ProtoItem]
$Name = DataDrivenItem_Blank
$Text engl = "Blank tonic"
$Text russ = "Пустой тоник"
Remedy = True
InitScript = DataDrivenItem::InitRemedy
```

### 3. Seed instance state once

`InitScript` is a persistent property of every item, so a function named there runs for a created item with `firstTime == true` and for a restored one with `false` ([init scripts](../content/prototype-format.md#init-scripts), [entity `InitScript` callbacks](../scripting/lifecycle-and-concurrency.md#entity-initscript-callbacks)). The baker resolves the name and checks the signature, so a typo fails the bake. Seeding only on creation is what keeps spent charges across a restore:

```angelscript
// The prototype names this function in its InitScript: it runs for each created item and for each restored one
void InitRemedy(Item item, bool firstTime)
{
    // Creation seeds the instance state once; a restored item already carries what the last run left in it
    if (firstTime && item.HasRemedy) {
        item.Remedy.ChargesLeft = item.Remedy.Charges;
    }
}
```

### 4. Use the item through the marker

The kind is the marker, the owner is `Item.CritterId`, and the cap is read from the setting at the moment of use. A refused use returns zero and changes nothing; the charge is spent only after the effect is applied:

```angelscript
// Uses an inventory item and returns the vigor it restored; zero means nothing happened
int32 UseItem(Critter cr, Item item)
{
    // The kind comes from the component marker, never from the prototype id or from a nonempty field
    if (item.CritterId != cr.Id || !item.HasRemedy) {
        return 0;
    }

    // A marker without authored values is a content mistake; the default zero must not read as a valid effect
    if (item.Remedy.Restore <= 0 || item.Remedy.ChargesLeft <= 0) {
        return 0;
    }

    int32 room = Settings.DataDrivenItem.MaxVigor - cr.Vigor;
    int32 restored = item.Remedy.Restore < room ? item.Remedy.Restore : room;
    if (restored <= 0) {
        return 0;
    }

    cr.Vigor = cr.Vigor + restored;
    item.Remedy.ChargesLeft = item.Remedy.ChargesLeft - 1;

    // A spent item leaves the world with its last charge
    if (item.Remedy.ChargesLeft == 0) {
        Game.DestroyItem(item);
    }

    return restored;
}
```

### 5. Check content before play

A marker without values reads as zero everywhere, so the use path and a content check both require positive values. The check probes with `Game.CheckProtoItem` because `Game.GetProtoItem` throws for an unknown id:

```angelscript
// Content check for one prototype: a kind marker needs values that make the kind usable
bool IsValidRemedyProto(hstring protoId)
{
    if (!Game.CheckProtoItem(protoId)) {
        return false;
    }

    ProtoItem proto = Game.GetProtoItem(protoId);
    return proto.HasRemedy && proto.Remedy.Restore > 0 && proto.Remedy.Charges > 0;
}
```

## Authority and sync

- The use path is server-only (`#if SERVER`). It trusts nothing from a client: the caller resolves the critter and the item on the server, and the path re-checks that the item sits in that critter's inventory. A remote call in front of it follows the [validated action recipe](validated-action.md).
- The callers hold a cover of the critter, which covers the items it carries ([Lock several entities for one action](cover-transactions.md)). The init script enters with its item covered.
- `Remedy.Restore` and `Remedy.Charges` are immutable `Common` values from the baked prototype: both sides read the same number and nothing can change it at runtime. `Remedy.ChargesLeft` is the only mutable item field and the only one that needs a sync decision; a sync flag belongs to each property separately, and the component grants none.
- The setting is `Server`: only the server reads it, and the recipe treats it as read-only configuration, never as state.

## Persistence and migration

`Remedy.ChargesLeft` and `Vigor` are `Mutable Persistent`, so they have a database record when the item and the critter do. The prototype fields and the setting are not saved: they come from the next bake and the next configuration, so changing `Remedy.Restore` or `DataDrivenItem.MaxVigor` changes every existing item at its next use. Changing `Remedy.Charges` does not refill existing items, because the charges were seeded once. If a rebalance must reach them, write a migration or reseed deliberately.

The component field replaced a flat `RemedyRestore` property. The rule keeps authored text and saves that still use the old key loadable, and gameplay code reads the new name only ([Declare a rule for every rename, removal and move](../project/data-migrations.md#declare-a-rule-for-every-rename-removal-and-move)):

```text
///@ MigrationRule Property Item Rename RemedyRestore Remedy.Restore
```

Authored `.fopro` keys resolve through the same rule table as stored keys (`PropertyRegistrar::FindPersistedProperty` in `Source/Common/Properties.cpp`). The sandbox bakes a prototype that still uses the old key without a warning, and `Remedy.Restore` reads `15` on it:

```ini
# Authored with the flat property name that the component field replaced
[ProtoItem]
$Name = DataDrivenItem_Legacy
$Text engl = "Old tonic"
$Text russ = "Старый тоник"
Remedy = True
RemedyRestore = 15
Remedy.Charges = 1
InitScript = DataDrivenItem::InitRemedy
```

`Remedy.Restore` is immutable, so a saved item never stores it; the rule matters for authored text, and for any `Mutable Persistent` field that moves into a component the same rule needs a restart test that loads an old save ([Test a migration](../project/data-migrations.md#test-a-migration)).

## Pitfalls and anti-patterns

- Deciding the kind by prototype id or by a nonempty field. Test the bool marker (`HasRemedy`): a child prototype can switch an inherited component off with `Remedy = False` and keep its field values, so a nonempty field proves nothing.
- Reading a component field without testing the marker first. In AngelScript the accessor throws `Component is not present on entity (check the Has-accessor first)` (`Entity_GetComponent` in `Source/Scripting/AngelScript/AngelScriptEntity.cpp`); the sandbox test `component_read_without_the_marker_throws` reads `stone.Remedy.Restore` and expects the throw. Managed C# documents `InvalidOperationException` for the same read; that was not run.
- Trusting a marker that has no values. The blank tonic has `Remedy = True` and nothing else, so every field reads `0` and the item looks like a valid remedy that does nothing. The use path refuses non-positive values, and `IsValidRemedyProto` is the check a content test runs over every prototype that carries the marker.
- Seeding without the `firstTime` guard. An unconditional `ChargesLeft = Charges` in the init script refills a restored item on every load. The sandbox test shows the guarded script keeps `2` of `3` charges through an unload and load and through a real restart; no unguarded variant is run.
- Reading a setting once at init and caching it in a script global. A setting is configuration, so read `Settings.DataDrivenItem.MaxVigor` where it is used, as the sandbox test does when it lowers the vigor to five below the maximum and the restore is capped to `5`.
- Calling `Game.GetProtoItem` for an id that may not exist. It throws; guard with `Game.CheckProtoItem` when absence is a legitimate answer.

## Test route

The scenarios `data-driven-item`, `data-driven-item-restart-write` and `data-driven-item-restart-verify` in `mechanics-smoke.json` use the `RecipeTests` and `RecipeRestart` sub-configs described in the [persistent state recipe](persistent-state.md#test-route), with `-Testing.Filter` set to `data_driven_item.`, `data_driven_item_restart_write.` and `data_driven_item_restart_verify.`. The in-process restore test creates a critter with a tonic, uses one charge, unloads the critter and loads it back:

```angelscript
    Map map = Sandbox::CreateMap();
    Location location = map.GetLocation();
    Critter cr = MakePatient(map);
    Item tonic = cr.AddItem(TonicPid);
    DataDrivenItem::UseItem(cr, tonic);
    location.MakePersistent(true);

    ident crId = cr.Id;
    Game.UnloadCritter(cr);

    Critter restored = Game.LoadCritter(crId, false, map);
    Item? restoredTonic = restored.GetItem(TonicPid);
    if (restoredTonic == null) {
        Testing::Fail(ctx, "The inventory item did not come back with its critter");
        return;
    }

    Testing::Expect(ctx, restoredTonic.HasRemedy, "The restored item still carries the component");
    Testing::ExpectEq(ctx, restoredTonic.Remedy.ChargesLeft, 2, "The init script did not reseed the restored item");
    Testing::ExpectEq(ctx, restored.Vigor, 30, "The critter vigor came back too");
```

The verify phase is a new server process on the same JSON directory; it finds the saved critter through its location, then expects `2` charges left, the authored `Remedy.Restore` of `30` and a still usable item.

Run every sandbox scenario from `Examples/MechanicsSandbox` with `python validate.py` on Windows (PowerShell) or `python3 validate.py` on Linux (bash); the run passes when `BuildTools/gameplay_test_runner.py` reports `status=passed` for every scenario and exits with `0`. Required markers include:

```text
[Testing] PASS data_driven_item.kind_comes_from_the_component
[Testing] PASS data_driven_item.legacy_flat_property_resolves_into_the_component
[Testing] PASS data_driven_item_restart_verify.charges_survive_restart
[Testing] COMPLETE status=passed
```

## Managed C#

Status `shape`: an Engine test parses `Managed/Recipes/DataDrivenItem.cs` and compares its `///@` lines with the AngelScript module, which are identical; no Engine lane compiles or runs it. The init callback and the use path keep the same structure; the entity parameters carry `[RequiresCover]` ([server entity synchronization](../scripting/managed-csharp.md#server-entity-synchronization)):

```csharp
    // The prototype names this method in its InitScript: it runs for each created item and for each restored one
    public static void InitRemedy([RequiresCover] Item item, bool firstTime)
    {
        // Creation seeds the instance state once; a restored item already carries what the last run left in it
        if (firstTime && item.HasRemedy) {
            item.Remedy.ChargesLeft = item.Remedy.Charges;
        }
    }
```

The generated accessors are `item.HasRemedy` and `item.Remedy.Restore`; reading `Remedy` while `HasRemedy` is false throws `InvalidOperationException` ([property components](../content/prototype-format.md#property-components)). A project on the managed backend must run the same restart test before relying on this shape.

## How a larger project extends it

- **More kinds.** Each kind is another component with its own marker (`Weapon`, `Armor`, `Food`), not another script switch. An item may carry several markers; every consumer tests only the marker it needs.
- **Structured tables.** When a kind needs a table of named rows, such as effect definitions shared by many items, move it to a [FixedType catalog](../../explanation/entity-and-property-model/choosing-a-representation.md#fixedtype-catalog-row) and reference the row from the component.
- **Content audit.** Run the content check over every prototype in a project test, not at module initialization, and fail the build on a marker without values.
- **Effects.** The restore here is one number. Declarative effects, timed buffs and stacking are project policy; a timed effect that survives a restart needs the [timed effect recipe](timed-effect.md).
- **What stays in the game project.** The kinds, their numbers, the item texts and the economy around them.

## See also

- [Build Game Mechanics](index.md)
- [Custom persistent character state](persistent-state.md)
- [Validated action request](validated-action.md)
- [Prototype Format](../content/prototype-format.md)
- [Choose How to Represent Game Data](../../explanation/entity-and-property-model/choosing-a-representation.md)
- [Script Lifecycle and Concurrency](../scripting/lifecycle-and-concurrency.md)
- [Migrate Saved Data and Content in a Game Project](../project/data-migrations.md)
