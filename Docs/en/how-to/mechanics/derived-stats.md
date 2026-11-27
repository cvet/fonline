---
layout: default
title: Derived stats with getters and setters
locale: en
document_id: recipe-derived-stats
permalink: /Docs/en/how-to/mechanics/derived-stats.html
recipe_kind: primitive
---

# Derived stats with getters and setters

Strength and endurance are stored; carry weight and maximum life follow from them, and current life must never exceed the maximum. This recipe computes the derived numbers in property getters registered on both sides, keeps the stored life in range with a transforming setter, reacts to changes with post-store setters, and shows what an owner and an observer can compute from what the server replicates.

| Backend | Status |
| --- | --- |
| AngelScript | `runtime-verified` |
| Managed C# | `shape` |

Runtime-verified means that the in-process scenario `derived-stats`, the scenario `derived-stats-client` with a real headless client, and the two-phase restart scenarios `derived-stats-restart-write` and `derived-stats-restart-verify` passed on a built and baked sandbox on a Windows host. Not verified: a Managed C# compile or run (the `AddPropertyDeferredSetter` spelling comes from the Managed C# page), setters on a `Virtual` property, a client-side transforming setter, and the `Min` and `Max` tags named under extensions.

## Source paths inspected

- `Examples/MechanicsSandbox/Scripts/Recipes/DerivedStats.fos`
- `Examples/MechanicsSandbox/Scripts/Sandbox.fos`
- `Examples/MechanicsSandbox/Scripts/Tests/Test_DerivedStats.fos`
- `Examples/MechanicsSandbox/Managed/Recipes/DerivedStats.cs`
- `Examples/MechanicsSandbox/Content/DerivedStats.fopro`
- `Examples/MechanicsSandbox/FOnlineMechanicsSandbox.fomain`
- `Examples/MechanicsSandbox/mechanics-smoke.json`
- `Source/Common/Properties.h`
- `Source/Common/Properties.cpp`
- `Source/Scripting/AngelScript/AngelScriptEntity.cpp`
- `Docs/generated/api.json`

## Problem

A character has base values that are stored and replicated, and numbers computed from them. Storing the computed numbers as well gives two copies that drift apart; recomputing them by hand in every script gives as many formulas as call sites. The wanted shape: one formula per derived number, available wherever the inputs are, a stored value that stays inside its derived bound, and a reaction that runs once per real change. What the numbers mean and which formula is right is game policy.

## Engine primitives used

| Symbol | Side | Role in the recipe |
| --- | --- | --- |
| `script.method.server.Entity.IsPersistent` | server | Test check that the critter has a database record before the restart |
| `script.method.client.Game.Chosen` | client | The owner's critter in the client drill |
| `script.method.client.Game.GetCritter` | client | An observed critter in the client drill |

The recipe rests on property machinery that the API model does not list as symbols: the `Virtual` property flag, `[[PropertyGetter]]`, `[[PropertySetter]]`, `Game.SetPropertyGetter`, `Game.AddPropertySetter` and `[[ModuleInit]]` are backend built-ins, and the `///@ Property` lines are project metadata. The ordering rule between transforming and post-store setters is owned by [property transformation versus post-set reactions](../scripting/lifecycle-and-concurrency.md#property-transformation-versus-post-set-reactions). A `Virtual` property cannot be `Persistent` or synchronized, which the registrar enforces when the declaration is loaded (`Source/Common/Properties.cpp`).

## Minimal example

Under `Examples/MechanicsSandbox/`: `Scripts/Recipes/DerivedStats.fos` holds the declarations, the getters and the setters; `Content/DerivedStats.fopro` the `DerivedStats_Npc` prototype; `Scripts/Tests/Test_DerivedStats.fos` the scenarios, including a headless-client drill that reads both critter views; `Managed/Recipes/DerivedStats.cs` the Managed C# shape. [Test route](#test-route) has the commands.

## Step by step

### 1. Declare inputs, derived values and constants

The stored inputs are `Mutable Persistent` with a sync flag. The derived values are `Virtual`: they have no storage, no sync flag and no record. `MaxLifeShown` is the exception that step 5 explains:

```angelscript
///@ Property Critter Common int32 Strength Mutable Persistent PublicSync
///@ Property Critter Common int32 Endurance Mutable Persistent OwnerSync
///@ Property Critter Common int32 CurLife Mutable Persistent PublicSync
///@ Property Critter Common bool Collapsed Mutable Persistent PublicSync
///@ Property Critter Common int32 CarryWeight Virtual
///@ Property Critter Common int32 MaxLife Virtual
///@ Property Critter Common int32 MaxLifeShown Mutable Persistent PublicSync

const int32 BaseCarryWeight = 20;
const int32 CarryPerStrength = 10;
const int32 BaseLife = 20;
const int32 LifePerEndurance = 5;
```

### 2. Register the getters on every side that reads the value

A getter is a function of the entity, registered once per property and side. It runs on every read and returns the value for that moment. Keep it a pure function of properties that the reading side has:

```angelscript
#if SERVER || CLIENT

// Getters are registered on every side that reads the property: a side without one cannot read it
[[ModuleInit]]
void InitDerivedStats()
{
    Game.SetPropertyGetter(CritterProperty::CarryWeight, GetCarryWeight);
    Game.SetPropertyGetter(CritterProperty::MaxLife, GetMaxLife);
}

// Pure functions of replicated inputs: no random, no clock, no other entity
[[PropertyGetter]]
int32 GetCarryWeight(Critter cr)
{
    return BaseCarryWeight + cr.Strength * CarryPerStrength;
}

[[PropertyGetter]]
int32 GetMaxLife(Critter cr)
{
    return BaseLife + cr.Endurance * LifePerEndurance;
}

#endif
```

### 3. Keep a stored value in range with a transforming setter

A setter that takes the value by reference runs before storage and may rewrite it; reading the entity inside it still returns the previous value. Setters are server-only here, because the server owns every write:

```angelscript
#if SERVER

[[ModuleInit]]
void InitDerivedStatsServer()
{
    Game.AddPropertySetter(CritterProperty::CurLife, ClampCurLife);
    Game.AddPropertySetter(CritterProperty::CurLife, OnCurLifeChanged);
    Game.AddPropertySetter(CritterProperty::Endurance, OnEnduranceChanged);
}

// The transforming setter runs before storage and rewrites the proposed value; the entity still holds the old one
[[PropertySetter]]
void ClampCurLife(Critter cr, int32& value)
{
    int32 maxLife = cr.MaxLife;
    if (value > maxLife) {
        value = maxLife;
    }

    if (value < 0) {
        value = 0;
    }
}
```

### 4. React after the store, and mind cascades

A setter that takes only the entity runs after storage and reads the committed value. Both handlers below assign another property; that assignment goes through the other property's own setters, so lowering endurance lowers `CurLife` through the clamp above:

```angelscript
// The reaction setter runs after storage and sees the committed value; a request clamped back to the stored value still runs it
[[PropertySetter]]
void OnCurLifeChanged(Critter cr)
{
    bool collapsed = cr.CurLife == 0;
    if (cr.Collapsed != collapsed) {
        cr.Collapsed = collapsed;
        Game.Log("derived_stats_collapsed_changed critter=" + cr.Id.value + " collapsed=" + (collapsed ? "true" : "false"));
    }
}

// Endurance is owner-only, so observers cannot compute MaxLife; a public mirror carries the result to them
[[PropertySetter]]
void OnEnduranceChanged(Critter cr)
{
    cr.MaxLifeShown = cr.MaxLife;

    // A smaller cap also lowers the stored life; the assignment goes through the clamping setter
    if (cr.CurLife > cr.MaxLife) {
        cr.CurLife = cr.MaxLife;
    }
}
```

### 5. Give observers what they cannot compute

The sync flags decide which side can evaluate a getter. `Strength` is `PublicSync`, so an observer computes `CarryWeight` itself. `Endurance` is `OwnerSync`, so an observer receives no endurance and its `MaxLife` getter sees `0`; the server therefore writes the public `MaxLifeShown` in the endurance setter, and that property carries the server's value to everyone.

## Authority and sync

- The server writes the stored values and registers the setters. A client has no `ModifiableByClient` flag on any of them, so it cannot assign them.
- A getter must be registered on every side that reads the property. The sandbox registers its two under `#if SERVER || CLIENT`; the control property `TestServerOnlyBonus` has a getter only on the server, and reading it on the client throws in AngelScript (`Getter not set`, `Entity_GetPropertyValue` in `Source/Scripting/AngelScript/AngelScriptEntity.cpp`). The native typed read of a `Virtual` property without a getter returns a default instead (`Properties::GetValue`); the Managed C# result was not run.
- Replication result, from the client drill: the owner, with strength `2` and endurance `3`, computes `CarryWeight` and `MaxLife` as `40` and `35`, the same as the server. An observer of another critter with strength `5` and endurance `6` computes carry weight `70`, reads endurance `0`, computes a base maximum life of `20`, and reads `50` from the mirror.
- Getters and setters run with the entity covered. A setter that touches another entity needs that entity under cover as well ([Lock several entities for one action](cover-transactions.md)).

## Persistence and migration

The inputs, `CurLife`, `Collapsed` and the mirror are saved; `CarryWeight` and `MaxLife` are not stored and are recomputed from the restored inputs. Restoring an entity runs no property setter: after a real restart the sandbox's post-setter counter reads `0`, yet `CarryWeight` is `50` and `MaxLife` is `40`. A value that a setter maintains must therefore be persisted, like `MaxLifeShown`, or rebuilt at load in `Game.OnCritterInit` with `firstTime == false` ([Custom persistent character state](persistent-state.md#persistence-and-migration)).

Changing a formula changes every derived value at the next read, but not a stored value that was clamped under the old formula: a saved `CurLife` can exceed a new, smaller `MaxLife` until the next assignment. Recompute at load or migrate. A renamed input needs a `Property Rename` rule ([data migrations](../project/data-migrations.md#declare-a-rule-for-every-rename-removal-and-move)).

## Pitfalls and anti-patterns

- Assuming a reaction setter runs only when the stored value changed. The Engine compares the request with the stored value before any transforming setter runs: assigning the stored value again runs nothing, but a different request that the clamp turns back into the stored value still runs the post setters (`Properties::SetValue` in `Source/Common/Properties.h`). The sandbox assigns `900` to a critter at its cap of `40`: the value stays `40` and the reaction runs once more. Write reactions that compare with state, as `OnCurLifeChanged` does with `Collapsed`.
- Reading the new value in a transforming setter. It sees the previous stored value and the proposed one in its parameter; the sandbox records `10` there before an assignment of `25`, and the post setter records `25`.
- Registering a getter on one side only. In AngelScript the other side throws on read, so register under `#if SERVER || CLIENT` and keep the function free of server-only data.
- Computing from data the reading side does not get. An observer's `MaxLife` was `20`, not `50`, because the `OwnerSync` input reads `0` there. Replicate a result through a public mirror, or compute it only on the owner's side.
- Letting setters form a cycle. A setter that assigns another property runs that property's setters, so two setters that write each other's property never settle.
- Putting randomness, the clock or another entity in a getter. It runs on every read, on both sides, and the results must match.
- Expecting a setter to rebuild state at load. Restoring runs none, as the restart test shows.

## Test route

The scenario `derived-stats` runs `-Testing.Filter derived_stats.` on `RecipeTests`. Its sharpest check proves the equal-request rule above:

```angelscript
    cr.CurLife = 12;
    int32 calls = cr.TestPostCount;
    cr.CurLife = 12;
    Testing::ExpectEq(ctx, cr.TestPostCount, calls, "Assigning the stored value again runs no setter");

    // A different request that the clamp turns into the stored value is not equal on entry, so reactions still run
    cr.CurLife = 40;
    calls = cr.TestPostCount;
    bool collapsed = cr.Collapsed;
    cr.CurLife = 900;
    Testing::ExpectEq(ctx, cr.CurLife, 40, "The clamp kept the stored value");
    Testing::ExpectEq(ctx, cr.TestPostCount, calls + 1, "The reaction setter ran although the stored value did not change");
    Testing::Expect(ctx, cr.Collapsed == collapsed, "The idempotent reaction changed nothing");
```

The scenario `derived-stats-client` starts the server with `RecipeNetworkTests` and `-Testing.Filter derived_stats_client.`, waits for `derived_stats_waiting_for_client`, and starts a headless client with `RecipeClient`, as in the [validated action test route](validated-action.md#test-route). The client reads its own critter and an observed one and reports the numbers; its read of the server-only control property is the throw check:

```angelscript
    bool bonusThrew = false;

    try {
        int32 bonus = own.TestServerOnlyBonus;
        Game.Log("derived_stats_client_unexpected_bonus value=" + bonus);
    }
    catch {
        bonusThrew = true;
    }

    CurPlayer.ServerCall.StatsDrillReport(testId, own.CarryWeight, own.MaxLife, npc.CarryWeight, npc.MaxLife, npc.MaxLifeShown, npc.Endurance, bonusThrew);
```

The restart pair uses `RecipeRestart` with a JSON directory, as in the [persistent state test route](persistent-state.md#test-route). The verify phase reads the restored critter:

```angelscript
    Critter cr = critters[0];
    Testing::ExpectEq(ctx, cr.Strength, 3, "The stored strength survived");
    Testing::ExpectEq(ctx, cr.CurLife, 33, "The stored life survived");
    Testing::ExpectEq(ctx, cr.MaxLifeShown, 40, "The persisted mirror survived");
    Testing::ExpectEq(ctx, cr.CarryWeight, 50, "The virtual carry weight is recomputed from stored values");
    Testing::ExpectEq(ctx, cr.MaxLife, 40, "The virtual maximum life is recomputed from stored values");
    Testing::ExpectEq(ctx, cr.TestPostCount, 0, "Restoring a critter runs no property setter");
    Testing::Pass(ctx);
```

Run every sandbox scenario from `Examples/MechanicsSandbox` with `python validate.py` on Windows (PowerShell) or `python3 validate.py` on Linux (bash); the run passes when `BuildTools/gameplay_test_runner.py` reports `status=passed` for every scenario and exits with `0`. The client scenario requires this server marker:

```text
derived_stats_report own=40/35 npc=70/20/50 npc_endurance=0
```

## Managed C#

Status `shape`: an Engine test parses `Managed/Recipes/DerivedStats.cs` and compares its `///@` lines with the AngelScript module, which are identical; no Engine lane compiles or runs it. The generated `Game.AddPropertyDeferredSetter` selects the post-store path explicitly, `ref int` marks the transforming setter, and handlers are `internal` with `[RequiresCover]` ([Managed C# authoring shape](../scripting/managed-csharp.md#authoring-shape)):

```csharp
    [ModuleInit]
    internal static void InitDerivedStatsServer()
    {
        Game.AddPropertySetter(CritterProperty.CurLife, ClampCurLife);
        Game.AddPropertyDeferredSetter(CritterProperty.CurLife, OnCurLifeChanged);
        Game.AddPropertyDeferredSetter(CritterProperty.Endurance, OnEnduranceChanged);
    }

    // The transforming setter runs before storage and rewrites the proposed value; the entity still holds the old one
    [PropertySetter]
    internal static void ClampCurLife([RequiresCover] Critter cr, ref int value)
    {
        int maxLife = cr.MaxLife;
        if (value > maxLife) {
            value = maxLife;
        }

        if (value < 0) {
            value = 0;
        }
    }
```

A project on the managed backend must run the same client drill before relying on this shape.

## How a larger project extends it

- **Formulas as data.** The four constants become `///@ Setting` values, or a [FixedType catalog](../../explanation/entity-and-property-model/choosing-a-representation.md#fixedtype-catalog-row) row per archetype, read inside the getter. A setting is read-only configuration, so the getter stays a pure function of it.
- **Constant limits without a setter.** A property declaration accepts `Min = <number>` and `Max = <number>` tags, and the Engine clamps an assigned value to them (`ValidateAndClampRawData` in `Source/Common/Properties.h`); this was not run in the sandbox. A bound that depends on another property, like `MaxLife`, still needs the transforming setter.
- **Many inputs.** Equipment, effects and status bonuses become further inputs of the same getter, each with its own sync decision; a structured value held in one [RefType property](../../explanation/entity-and-property-model/choosing-a-representation.md#reftype) is read, changed and assigned back as a whole.
- **Project events.** Fire a project event from the reaction setter when other modules need the change, so there is one producer.
- **What stays in the game project.** The stat set, the formulas, the bounds and what a collapsed character means.

## See also

- [Build Game Mechanics](index.md)
- [Custom persistent character state](persistent-state.md)
- [Equip into a slot with one shared rule](equip-slot.md)
- [Script Lifecycle and Concurrency](../scripting/lifecycle-and-concurrency.md#property-transformation-versus-post-set-reactions)
- [Choose How to Represent Game Data](../../explanation/entity-and-property-model/choosing-a-representation.md)
- [Managed C# Scripting](../scripting/managed-csharp.md)
- [Migrate Saved Data and Content in a Game Project](../project/data-migrations.md)
