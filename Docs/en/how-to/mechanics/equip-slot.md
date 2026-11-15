---
layout: default
title: Equip into a slot with one shared rule
locale: en
document_id: recipe-equip-slot
permalink: /Docs/en/how-to/mechanics/equip-slot.html
recipe_kind: primitive
---

# Equip into a slot with one shared rule

A player drags armor onto the armor slot. The interface must know at once whether that is allowed, and the server must decide for real. This recipe writes the rule once, compiles it for both sides, enforces it in the one server request handler, and repairs any move that bypassed that handler. It also shows the Engine fact that shapes the design: there is no event that can veto an item move before it happens.

| Backend | Status |
| --- | --- |
| AngelScript | `runtime-verified` |
| Managed C# | `shape` |

Runtime-verified means that the in-process scenario `equip-slot` and the scenario `equip-slot-client`, in which a real headless client connected to the server sends honest and forged requests, passed on a built and baked sandbox on a Windows host. Not verified: a Managed C# compile or run, a client interface that predicts the move locally, slots beyond the third one, and persistence of the slot across a restart.

## Source paths inspected

- `Examples/MechanicsSandbox/Scripts/Recipes/EquipSlot.fos`
- `Examples/MechanicsSandbox/Scripts/Sandbox.fos`
- `Examples/MechanicsSandbox/Scripts/Tests/Test_EquipSlot.fos`
- `Examples/MechanicsSandbox/Managed/Recipes/EquipSlot.cs`
- `Examples/MechanicsSandbox/Content/EquipSlot.fopro`
- `Examples/MechanicsSandbox/FOnlineMechanicsSandbox.fomain`
- `Examples/MechanicsSandbox/mechanics-smoke.json`
- `Source/Scripting/ServerCritterScriptMethods.cpp`
- `Source/Server/CritterManager.cpp`
- `Source/Server/Item.cpp`
- `Source/Common/Settings.inc`
- `Source/Common/Common.h`
- `Docs/generated/api.json`

## Problem

Armor fits the armor slot, a sword fits the main slot, a rock fits no equipment slot, and nobody may move another character's item. The client wants to grey out a wrong drop without a round trip, and the server must not trust that the client did. Which slots exist, what fits where and what equipping does to stats is game policy. What the Engine fixes is smaller: three built-in slot values, a swap on single-item slots, and a post-commit event.

## Engine primitives used

| Symbol | Side | Role in the recipe |
| --- | --- | --- |
| `script.enum.CritterItemSlot` | common | The slot type; the recipe extends it with one script-declared value |
| `script.enum-value.CritterItemSlot.Inventory` | common | The unequipped inventory, value `0`; the always-allowed destination |
| `script.enum-value.CritterItemSlot.Main` | common | The built-in equipped slot, value `1` |
| `script.property.Item.CritterSlot` | common | The slot an inventory item occupies; the audit reads it after a move |
| `script.property.Item.CritterId` | common | The owning critter; the rule refuses an item that is not the actor's |
| `script.method.server.Critter.ChangeItemSlot` | server | Moves an inventory item to an enabled slot and swaps the occupant of a single-item slot |
| `script.event.server.Game.OnCritterItemMoved` | server | Fires after an item enters, leaves or changes slot; the audit hook |
| `script.method.server.Player.GetControlledCritter` | server | The actor comes from the calling player |
| `script.method.server.Critter.GetItem#3e4a8f273e418d3b` | server | Resolves the claimed id only inside the actor's inventory |
| `script.method.client.Game.Chosen` | client | The client's own critter for the shared rule |
| `setting.server.Critter.CritterSlotEnabled` | server setting | Enables slots by index; the script-declared slot needs its entry |

The `///@ Enum`, `///@ Property` and `///@ RemoteCall` lines are project metadata, and `[[ServerRemoteCall]]`, `[[ClientRemoteCall]]`, `[[Event]]`, `[[ModuleInit]]`, `Player.ServerCall`, `Player.ClientCall` and the client getter `CurPlayer` are backend built-ins; they are outside the API model. The remote-call grammar is in [Remote Calls](../../reference/scripting/remote-calls.md#declaration-grammar), the request pattern in the [validated action recipe](validated-action.md).

## Minimal example

Under `Examples/MechanicsSandbox/`: `Scripts/Recipes/EquipSlot.fos` holds the declarations, the shared rule, the server request path, the audit and the client call; `Content/EquipSlot.fopro` the item prototypes; `FOnlineMechanicsSandbox.fomain` the slot setting; `Scripts/Tests/Test_EquipSlot.fos` the scenarios and the client drill; `Managed/Recipes/EquipSlot.cs` the Managed C# shape. [Test route](#test-route) has the commands.

## Step by step

### 1. Declare the slot, the item data and the calls

The engine enum has `Inventory = 0`, `Main = 1` and `Outside = 255`. A script may add values to it; `Armor = 2` is the third slot. `EquipSlot` is an authored `Common` property, so both sides read the slot an item fits. The remote calls carry the item id and the wanted slot, nothing else, and `MaxBytes 9` is one `ident` plus one enum byte:

```angelscript
///@ Enum CritterItemSlot Armor = 2

///@ Property Item Common CritterItemSlot EquipSlot

///@ RemoteCall Server EquipItem(ident itemId, CritterItemSlot slot) MaxBytes 9
///@ RemoteCall Client EquipResult(ident itemId, bool accepted) MaxBytes 9
```

### 2. Enable the slot in configuration

`ChangeItemSlot` refuses a slot that `Critter.CritterSlotEnabled` does not enable. The default setting lists two entries, so the third needs the line below:

```ini
Critter.CritterSlotEnabled = true true true
```

Two sibling settings are indexed the same way and have no entry for slot `2`: `Critter.CritterSlotMultiItem` (default `true false`) then treats the new slot as single-item, so a second item swaps the first one out, and `Critter.CritterSlotSendData` (default `false true`) keeps its contents away from observers, who receive only the slots listed as `true` there (`Item::CanSendItem` in `Source/Server/Item.cpp`).

### 3. Author which slot an item fits

```ini
# Armor goes to the Armor slot, a sword to Main, a rock to no equipment slot at all
[ProtoItem]
$Name = EquipSlot_Armor
$Text engl = "Leather armor"
$Text russ = "Кожаная броня"
EquipSlot = Armor
```

The sword prototype sets `EquipSlot = Main`; the rock sets nothing, and its value is the default `0`, the plain inventory.

### 4. Write the rule once

The rule has no side-specific data: `Item.CritterId` and `EquipSlot` exist on both sides, so one function serves the interface and the server:

```angelscript
// The one rule, compiled for both sides: the UI predicts exactly what the server will decide
bool CanEquip(Critter cr, Item item, CritterItemSlot slot)
{
    if (item.CritterId != cr.Id) {
        return false;
    }

    // Putting an item back in the plain inventory is always allowed
    if (slot == CritterItemSlot::Inventory) {
        return true;
    }

    return item.EquipSlot == slot;
}
```

### 5. Enforce it in the request handler

The server derives the actor from the player, resolves the item only in that actor's inventory, runs the rule, and only then changes the slot. A refusal logs its reason and replies `false` without telling the client more:

```angelscript
// The only request path: the engine supplies the player, the critter and its inventory are derived from it
[[ServerRemoteCall]]
void EquipItem(Player player, ident itemId, CritterItemSlot slot)
{
    Critter? cr = player.GetControlledCritter();
    if (cr == null) {
        Refuse(player, itemId, "no_actor");
        return;
    }

    // Only the actor's own inventory resolves; any other id is not an item the actor may move
    Item? item = cr.GetItem(itemId);
    if (item == null) {
        Refuse(player, itemId, "not_owned");
        return;
    }

    if (!CanEquip(cr, item, slot)) {
        Refuse(player, itemId, "slot_rule");
        return;
    }

    cr.ChangeItemSlot(itemId, slot);
    Game.Log("equip_slot_accepted item=" + itemId.value + " slot=" + int(slot));
    player.ClientCall.EquipResult(itemId, true);
}

void Refuse(Player player, ident itemId, string reason)
{
    Game.Log("equip_slot_refused reason=" + reason + " player=" + player.Id.value + " item=" + itemId.value);
    player.ClientCall.EquipResult(itemId, false);
}
```

### 6. Audit after the commit

The Engine cannot ask the script before a move, so the audit subscribes to the event that fires after it. It repairs only moves that bypassed the handler, for example a project script that calls `ChangeItemSlot` directly:

```angelscript
// The engine fires this after the move is committed, so it cannot veto: it repairs a move that bypassed the request path
[[Event]]
void OnCritterItemMoved(Critter cr, Item item, CritterItemSlot fromSlot)
{
    // An item that left the inventory has no slot to audit
    if (item.CritterId != cr.Id || CanEquip(cr, item, item.CritterSlot)) {
        return;
    }

    Game.Log("equip_slot_repaired item=" + item.Id.value + " slot=" + int(item.CritterSlot));
    cr.ChangeItemSlot(item.Id, CritterItemSlot::Inventory);
}
```

### 7. Predict on the client

The client runs the same rule against its chosen critter before it sends anything, then sends the request and waits for the reply; it does not move the item itself:

```angelscript
// The client asks only for what the shared rule allows; the server repeats the check
void RequestEquip(Item item, CritterItemSlot slot)
{
    if (!CanEquip(Chosen, item, slot)) {
        Game.Log("equip_slot_client_refused item=" + item.Id.value);
        return;
    }

    CurPlayer.ServerCall.EquipItem(item.Id, slot);
}

[[ClientRemoteCall]]
void EquipResult(ident itemId, bool accepted)
{
    Game.Log("equip_slot_result item=" + itemId.value + " accepted=" + (accepted ? "true" : "false"));
}
```

## Authority and sync

- The server decides. The actor is `Player.GetControlledCritter()`, and the item is resolved with `Critter.GetItem(itemId)`, which finds only the actor's own inventory: another critter's item is not reachable by id, so `not_owned` costs no extra check.
- The handler enters with the player and its controlled critter covered, which covers the critter's inventory, and `ChangeItemSlot` touches only that critter and its items. No `Game.Sync` is needed; a rule that reads another entity needs one ([Lock several entities for one action](cover-transactions.md)).
- The client's check is a convenience. It runs on data the client owns, and a modified client can skip it, which is why the server repeats it on its own data.
- Observers see an equipped item only when `Critter.CritterSlotSendData` has `true` at its index; the owner receives every enabled slot.

## Persistence and migration

`Item.CritterSlot` is a persistent core property, so an equipped item keeps its slot in its record; the sandbox does not restart-test that, because the recipe claim is the rule, not the storage. `EquipSlot` is authored and not saved.

A script-declared enum value is a saved-data identity: its number and name are what a stored slot means. Never reuse number `2` for another meaning, and rename a value with an `Enum` rule ([Declare a rule for every rename, removal and move](../project/data-migrations.md#declare-a-rule-for-every-rename-removal-and-move)). Turning a slot off in `Critter.CritterSlotEnabled` later does not move the items stored in it: `Item::CanSendItem` stops sending them, so migrate them to `Inventory` first.

## Pitfalls and anti-patterns

- Waiting for a veto event. `api.json` has no event that runs before an item move; `Game.OnCritterItemMoved` fires after the slot is committed. The sandbox registers a highest-priority probe, and it already sees the rule-breaking slot when a script moves the armor to `Main`. The rule belongs in the request handler, with the audit as a safety net.
- Trusting the client rule. The sandbox client skips the rule and sends a rock to Armor, armor to Main and a bystander's armor; the server refuses them with `slot_rule`, `slot_rule` and `not_owned`, and accepts the two honest requests.
- Adding an enum value without enabling the slot. `ChangeItemSlot` then throws `Slot is not allowed` and changes nothing (the test uses slot `3`, which no setting enables); the Engine does not infer slots from script enums.
- An audit that does not terminate. Moving an item from inside `Game.OnCritterItemMoved` fires the event again, and a swap on a single-item slot fires it for both items. The audit above ends because the repair target, `Inventory`, always passes the rule.
- A rule that reads server-only data. The client would compute a different answer or fail to read it; keep the shared function to properties both sides receive.
- Putting display text in the reply. Reply with a code or a `TextPackKey`, as the [validated action recipe](validated-action.md#how-a-larger-project-extends-it) says.

## Test route

The scenario `equip-slot` runs `-Testing.Filter equip_slot.` on `RecipeTests`. Its negative control moves armor to `Main` with a direct `ChangeItemSlot` call and shows that the Engine accepts it and the audit repairs it:

```angelscript
// Negative control: the engine runs no rule of its own, so a script that skips the request path moves anything anywhere
void EngineAcceptsAScriptMoveAndTheAuditRepairsIt(Testing::TestContext ctx)
{
    Map map = Sandbox::CreateMap();
    Critter cr = map.AddCritter(Sandbox::NpcPid, NpcHex, mdir(0));
    Item armor = cr.AddItem(ArmorPid);

    cr.TestProbeSlot = -1;
    cr.ChangeItemSlot(armor.Id, CritterItemSlot::Main);

    Testing::ExpectEq(ctx, cr.TestProbeSlot, int(CritterItemSlot::Main), "The engine committed the rule-breaking move before any audit ran");
    Testing::Expect(ctx, armor.CritterSlot == CritterItemSlot::Inventory, "The post-commit audit put the item back");

    Game.DestroyLocation(map.GetLocation());
}
```

The scenario `equip-slot-client` starts the server with `RecipeNetworkTests` and `-Testing.Filter equip_slot_client.`, waits for `equip_slot_waiting_for_client`, then starts a headless client with `RecipeClient`, as in the [validated action test route](validated-action.md#test-route). The client first applies the shared rule locally to the rock, then sends three forged requests that skip the rule, two honest ones and a final `EquipDrillSent`; one connection's handlers run in arrival order, so that last call is processed after the five requests. Run every sandbox scenario from `Examples/MechanicsSandbox` with `python validate.py` on Windows (PowerShell) or `python3 validate.py` on Linux (bash); the run passes when `BuildTools/gameplay_test_runner.py` reports `status=passed` for every scenario and exits with `0`. Required markers include:

```text
equip_slot_repaired item=
equip_slot_client_refused item=
equip_slot_refused reason=slot_rule
equip_slot_refused reason=not_owned
equip_slot_accepted item=
[Testing] PASS equip_slot_client.client_requests_are_enforced_on_the_server
```

## Managed C#

Status `shape`: an Engine test parses `Managed/Recipes/EquipSlot.cs` and compares its `///@` lines with the AngelScript module, which are identical; no Engine lane compiles or runs it. The audit keeps its structure with `[RequiresCover]` on its entity parameters, and the handler is `internal` ([Managed C# authoring shape](../scripting/managed-csharp.md#authoring-shape)):

```csharp
    // The engine fires this after the move is committed, so it cannot veto: it repairs a move that bypassed the request path
    [Event]
    internal static void OnCritterItemMoved([RequiresCover] Critter cr, [RequiresCover] Item item, CritterItemSlot fromSlot)
    {
        // An item that left the inventory has no slot to audit
        if (item.CritterId != cr.Id || CanEquip(cr, item, item.CritterSlot)) {
            return;
        }

        Game.Log("equip_slot_repaired item=" + item.Id + " slot=" + (int)item.CritterSlot);
        cr.ChangeItemSlot(item.Id, CritterItemSlot.Inventory);
    }
```

The client reads `Game.Chosen` and `Game.CurPlayer` as properties. A project on the managed backend must run the same client drill before relying on this shape.

## How a larger project extends it

- **More slots.** Add one enum value and one entry per slot to the three settings, and migrate saved slots when a number changes meaning. No slot beyond index `2` was run.
- **Rule as data.** Replace the single `EquipSlot` value with a component or catalog row that lists allowed slots and requirements, and keep `CanEquip` as the one reader ([property components](../content/prototype-format.md#property-components)).
- **Prediction.** A client that moves the item before the reply arrives must undo it on a refusal; the recipe leaves the interface in its old state until the reply.
- **Consequences.** Stats from equipment are [derived values](derived-stats.md) recomputed from the equipped items, not a number added at equip time.
- **What stays in the game project.** Slot names, what fits where, equip cost and time, and the item data.

## See also

- [Build Game Mechanics](index.md)
- [Validated action request](validated-action.md)
- [Derived stats with getters and setters](derived-stats.md)
- [Remote Calls](../../reference/scripting/remote-calls.md)
- [Script Lifecycle and Concurrency](../scripting/lifecycle-and-concurrency.md)
- [Managed C# Scripting](../scripting/managed-csharp.md)
- [Migrate Saved Data and Content in a Game Project](../project/data-migrations.md)
