---
layout: default
title: Put and take between a critter and a container
locale: en
document_id: recipe-container-transfer
permalink: /Docs/en/how-to/mechanics/container-transfer.html
recipe_kind: primitive
---

# Put and take between a critter and a container

A player puts an item into a chest on the map or into a bag it carries, and takes one out again. This recipe moves items between two holders under one cover, checks reach for a chest and ownership for the item, groups contents by a section key, sends the client a snapshot of the contents, and proves with a restart that both holders and the sections come back. Capacity, sections and what a container is are project policy; the Engine stores an inner item with a grouping key and nothing more.

| Backend | Status |
| --- | --- |
| AngelScript | `runtime-verified` |
| Managed C# | `shape` |

Runtime-verified means that the in-process scenario `container-transfer`, the scenario `container-transfer-client` with a real headless client that sends honest and forged requests, and the two-phase restart scenarios `container-transfer-restart-write` and `container-transfer-restart-verify` passed on a built and baked sandbox on a Windows host. Not verified: a Managed C# compile or run, `withInnerEntities = true` in `Critter.SendItems`, and item counts on a stackable property (stacking is project policy and is not modelled).

## Source paths inspected

- `Examples/MechanicsSandbox/Scripts/Recipes/ContainerTransfer.fos`
- `Examples/MechanicsSandbox/Scripts/Sandbox.fos`
- `Examples/MechanicsSandbox/Scripts/Tests/Test_ContainerTransfer.fos`
- `Examples/MechanicsSandbox/Managed/Recipes/ContainerTransfer.cs`
- `Examples/MechanicsSandbox/Content/ContainerTransfer.fopro`
- `Examples/MechanicsSandbox/FOnlineMechanicsSandbox.fomain`
- `Examples/MechanicsSandbox/mechanics-smoke.json`
- `Source/Server/Item.cpp`
- `Source/Server/ItemManager.cpp`
- `Source/Scripting/ServerGlobalScriptMethods.cpp`
- `Source/Scripting/ServerItemScriptMethods.cpp`
- `Source/Server/EntitySync.cpp`
- `Source/Client/Client.cpp`
- `Docs/generated/api.json`

## Problem

Two holders take part in every transfer: the actor's inventory and a container that is either a chest standing on the map or a bag in the same inventory. The player names the container and the item; both ids are claims. The server must find the container only where the actor can reach it, find the item only where the actor owns it, apply the capacity rule, and move the item with every entity that the move touches covered. Which items are containers, how many fit and what the sections mean is game policy.

## Engine primitives used

| Symbol | Side | Role in the recipe |
| --- | --- | --- |
| `script.method.server.Player.GetControlledCritter` | server | The actor comes from the calling player |
| `script.method.server.Game.Sync#b42b6f6d6b684d2a` | server | Covers the player with its critter and the map in one request |
| `script.method.server.Critter.GetItem#3e4a8f273e418d3b` | server | Resolves an id inside the actor's inventory only |
| `script.method.server.Map.GetItem` | server | Resolves a chest id among the items of the actor's map |
| `script.method.server.Game.GetDistance#3dade8869ec8c0ac` | server | Reach between the actor and a chest |
| `script.property.Item.Ownership` | common | Tells a map chest from a carried bag; also shows where an item is after a move |
| `script.enum-value.ItemOwnership.MapHex` | common | The placement of a chest that stands on the map |
| `script.method.server.Item.GetItems` | server | The contents of a container, optionally one section |
| `script.method.server.Game.MoveItem#2ed0d82a6f587a26` | server | `Game.MoveItem(item, toCont, stackId)` puts an item into a container |
| `script.method.server.Game.MoveItem#c26f82c316789816` | server | `Game.MoveItem(item, toCr)` takes it back to a critter |
| `script.property.Item.ContainerId` | common | The parent container while the item is inside one |
| `script.property.Item.ContainerStack` | common | The grouping key; the recipe stores the section there |
| `script.method.server.Item.AddItem#c6fbcb98959420b1` | server | The test creates an item directly inside a chest |
| `script.method.server.Critter.SendItems` | server | Sends a snapshot of the contents to the actor's client |
| `script.event.client.Game.OnReceiveItems` | client | Receives the snapshot with the context value |
| `script.method.server.Game.DestroyItem` | server | The negative control destroys a container with its contents |

The `///@ Property` and `///@ RemoteCall` lines are project metadata, and `[[ServerRemoteCall]]`, `[[ClientRemoteCall]]`, `[[Async]]`, `[[Event]]`, `[[ModuleInit]]`, `Player.ClientCall` and `Player.ServerCall` are backend built-ins; they are outside the API model. The request pattern is the [validated action recipe](validated-action.md), the cover rules are in [Lock several entities for one action](cover-transactions.md).

## Minimal example

Under `Examples/MechanicsSandbox/`: `Scripts/Recipes/ContainerTransfer.fos` holds the declarations, the transfer path, the snapshot call and the client handlers; `Content/ContainerTransfer.fopro` a chest, a bag and two kinds of item; `Scripts/Tests/Test_ContainerTransfer.fos` the scenarios and the client drill; `Managed/Recipes/ContainerTransfer.cs` the Managed C# shape. [Test route](#test-route) has the commands.

## Step by step

### 1. Declare the policy data and the calls

`ContainerCapacity` makes an item a container, and `StorageSection` is the grouping key an item carries. Both are authored properties; the Engine has no container flag, and `Item::AddItemToContainer` checks only that the move does not create a cycle. The remote calls name the container and the item, and `put` chooses the direction:

```angelscript
///@ Property Item Common int32 ContainerCapacity
///@ Property Item Common int32 StorageSection

///@ RemoteCall Server TransferItem(ident containerId, ident itemId, bool put) MaxBytes 17
///@ RemoteCall Server OpenContainer(ident containerId) MaxBytes 8
///@ RemoteCall Client ContainerResult(ident containerId, bool accepted) MaxBytes 9
```

A chest and a bag differ only in data:

```ini
# A chest that stands on the map and holds three items, and a bag that holds two and travels with its owner
[ProtoItem]
$Name = ContainerTransfer_Chest
$Text engl = "Storage chest"
$Text russ = "Сундук"
ContainerCapacity = 3
```

### 2. Cover both holders with one request

The handler derives the actor and its map, then asks for the player and the map in one `Game.Sync`. Covering the player covers its critter and everything the critter carries, and covering the map covers its items, a chest and the items inside it. The actor and its map are derived again after the acquisition because other jobs may have run:

```angelscript
[[ServerRemoteCall]] [[Async]]
void TransferItem(Player player, ident containerId, ident itemId, bool put)
{
    // The engine supplies the player; the actor and its map are derived, and one cover holds both holders
    Critter? actor = player.GetControlledCritter();
    Map? map = actor != null ? actor.GetMap() : null;
    if (actor == null || map == null) {
        Refuse(player, containerId, "no_actor");
        return;
    }

    Game.Sync(player, map);

    actor = player.GetControlledCritter();
    if (actor == null || actor.MapId != map.Id) {
        Refuse(player, containerId, "actor_moved");
        return;
    }

    string reason = TryTransfer(actor, map, containerId, itemId, put);
    if (!reason.isEmpty()) {
        Refuse(player, containerId, reason);
        return;
    }

    player.ClientCall.ContainerResult(containerId, true);
}
```

### 3. Resolve the container where the actor can reach it

A carried bag is found in the inventory and needs no reach; a map chest must be within one hex. Anything else, including an item without capacity and a zero id, resolves to nothing:

```angelscript
// A chest stands on the map and must be near; a bag in the actor's own inventory is always in reach
Item? FindReachableContainer(Critter actor, Map map, ident containerId)
{
    Item? container = actor.GetItem(containerId);
    if (container == null) {
        container = map.GetItem(containerId);
    }

    if (container == null || container.ContainerCapacity <= 0) {
        return null;
    }

    if (container.Ownership == ItemOwnership::MapHex && Game.GetDistance(actor, container) > ReachDistance) {
        return null;
    }

    return container;
}
```

### 4. Put and take

A put resolves the item only in the actor's inventory, applies the project rules, and passes the section as the stack id. A take finds the item through the covered container instead of a global id lookup:

```angelscript
string TryPut(Critter actor, Item container, ident itemId)
{
    // Only the actor's own inventory resolves, so a foreign or invented id is not an item to move
    Item? item = actor.GetItem(itemId);
    if (item == null) {
        return "not_carried";
    }

    // Policy: containers do not nest, which also keeps a container out of its own subtree
    if (item.Id == container.Id || item.ContainerCapacity > 0) {
        return "no_nesting";
    }

    if (container.GetItems().length() >= container.ContainerCapacity) {
        return "full";
    }

    // The section is the grouping key; the engine merges nothing, so equal sections stay separate items
    Item? moved = Game.MoveItem(item, container, any(item.StorageSection));
    return moved != null ? "" : "move_failed";
}

string TryTake(Critter actor, Item container, ident itemId)
{
    // The item is found through the covered container; a global id lookup would hand back an uncovered entity
    Item[] contents = container.GetItems();

    for (int i = 0; i < contents.length(); i++) {
        if (contents[i].Id == itemId) {
            Item? moved = Game.MoveItem(contents[i], actor);
            return moved != null ? "" : "move_failed";
        }
    }

    return "not_inside";
}
```

### 5. Send a snapshot, not a guess

`OpenContainer` repeats steps 2 and 3, then sends the contents with a context value that names the container (`actor.SendItems(container.GetItems(), false, false, any(containerId.value))`, `ContainerTransfer.fos` lines 70-71). The client receives detached item views and the same context:

```angelscript
// Received items are detached views: read them now, they are not part of the client's inventory
[[Event]]
void OnReceiveItems(Item[] items, any contextParam)
{
    int64 containerId = contextParam;
    Game.Log("container_snapshot container=" + containerId + " count=" + items.length());
}
```

## Authority and sync

- The server decides. The actor is `Player.GetControlledCritter()`, the map is the actor's map, and the container and the item are resolved only inside those two holders. A forged id from another critter's inventory or from a far chest resolves to nothing and is refused.
- `Game.MoveItem` checks cover for the item, for the holder it leaves and for the destination (`Server_Game_MoveItem` in `Source/Scripting/ServerGlobalScriptMethods.cpp`). `Game.Sync(player, map)` supplies all three for both directions: holding an entity covers every entity below it, so a bag in the inventory, a chest and every inner item are covered ([cover model](cover-transactions.md#the-cover-model-in-practice)).
- The handlers carry `[[Async]]` because `Game.Sync` does; after the acquisition no step suspends, so the checks and the move run under one cover.

## Persistence and migration

Items inside a persistent container get a database record when they enter it (`Item::AddItemToContainer` in `Source/Server/Item.cpp`), and a critter's inventory is stored with the critter. The restart pair puts items into a map chest and keeps one in the critter, then a new server process finds the chest through its location: the chest still holds both items, each in its section, the critter still carries its own item, and a take then works on the restored pair. A critter unloaded and loaded in one process keeps a bag with its contents too.

`Item.ContainerStack` is saved with the item, so the section numbers are saved data. Renumbering a section is a data migration, and a renamed `StorageSection` or `ContainerCapacity` needs a `Property Rename` rule ([Declare a rule for every rename, removal and move](../project/data-migrations.md#declare-a-rule-for-every-rename-removal-and-move)).

## Pitfalls and anti-patterns

- Treating the stack id as stacking. The `Item.ContainerStack` description says the key is used to select and merge items, but no Engine code merges: two items with equal prototype and equal key stay two items, and `Item.GetItems` with a key only filters by it. The sandbox adds two ores and a herb, finds three items, finds two in the ore section, and counts any quantity in project data.
- Moving a container into itself. `Game.MoveItem(bag, bag)` throws, but leaves the bag with `Ownership == Nowhere` (value `3`), no holder and not destroyed: `ItemManager::MoveItem` detaches the source before `Item::AddItemToContainer` refuses the cycle. The sandbox prints `container_transfer_self_move_state ownership=3 destroyed=false`. The recipe's no-nesting rule keeps a container out of every container, so the path is never reached; a project that allows nesting must check the cycle before the call.
- Resolving the container or the item by a global id lookup. It can return an entity outside the cover and one the actor has no claim to. Resolve through `Critter.GetItem`, `Map.GetItem` and the covered container's `Item.GetItems`.
- Destroying a container that still holds items. The contents are owned by the container and go with it; the negative control `destroying_the_container_destroys_its_contents` shows it. Take or move them first when they must survive.
- Splitting the capacity check from the move by a cover change. Both belong under the one cover from step 2; a `Game.Sync` between them lets another job fill the chest first.
- Trusting the snapshot afterwards. The client's items are a copy of one moment; ask again with `OpenContainer` after any change.

## Test route

The scenario `container-transfer` runs `-Testing.Filter container_transfer.` on `RecipeTests`. Its stacking control, `sections_group_items_and_nothing_is_merged`, puts two ores and a herb into a chest and expects three items. The engine trap is a test too: the move throws, and the bag's state is logged:

```angelscript
    bool threw = false;

    try {
        Game.MoveItem(bag, bag);
    }
    catch {
        threw = true;
    }

    Testing::Expect(ctx, threw, "The engine refuses a container inside itself");
    Game.Log("container_transfer_self_move_state ownership=" + int(bag.Ownership) + " destroyed=" + (bag.IsDestroyed ? "true" : "false"));
```

The scenario `container-transfer-client` starts the server with `RecipeNetworkTests` and `-Testing.Filter container_transfer_client.`, waits for `container_transfer_waiting_for_client`, and starts a headless client with `RecipeClient`, as in the [validated action test route](validated-action.md#test-route). The client opens the chest, puts a herb, tries another critter's item, tries a far chest, takes the stored ore, and sends `ContainerDrillSent`; the server then checks that only the near chest holds the herb and the far one is empty. The restart pair uses `RecipeRestart`, as in the [persistent state test route](persistent-state.md#test-route). Run every sandbox scenario from `Examples/MechanicsSandbox` with `python validate.py` on Windows (PowerShell) or `python3 validate.py` on Linux (bash); the run passes when `BuildTools/gameplay_test_runner.py` reports `status=passed` for every scenario and exits with `0`. Required markers include:

```text
container_transfer_refused reason=not_carried
container_transfer_refused reason=unknown_container
container_snapshot container=
[Testing] PASS container_transfer_client.client_requests_are_validated_and_snapshots_arrive
[Testing] PASS container_transfer_restart_verify.both_holders_survive_restart
```

## Managed C#

Status `shape`: an Engine test parses `Managed/Recipes/ContainerTransfer.cs` and compares its `///@` lines with the AngelScript module, which are identical; no Engine lane compiles or runs it. The handler is `async Task` and `internal`, and `Sync.Lock` replaces `Game.Sync` ([Engine-contract analyzers](../project/engine-contract-analyzers.md), [server entity synchronization](../scripting/managed-csharp.md#server-entity-synchronization)):

```csharp
    [ServerRemoteCall]
    internal static async Task TransferItem([RequiresCover] Player player, ident containerId, ident itemId, bool put)
    {
        // The engine supplies the player; the actor and its map are derived, and one cover holds both holders
        Critter? actor = player.GetControlledCritter();
        Map? map = actor?.GetMap();
        if (actor == null || map == null) {
            Refuse(player, containerId, "no_actor");
            return;
        }

        if (!await Sync.Lock(player, map)) {
            return;
        }
```

A project on the managed backend must run the same client drill and restart pair before relying on this shape.

## How a larger project extends it

- **Stacking and counts.** Put the quantity in a mutable property on the item and merge in project code under the same cover; the Engine will not.
- **Nested containers.** Allow them only with an explicit cycle check before every move, and decide whether contents count against the parent's capacity.
- **Shared and locked containers.** Ownership, keys and who may open a chest are policy data on the item; the recipe's reach rule is the place to add them.
- **What stays in the game project.** What a container is, capacity, sections, transfer cost and time, and the texts of every refusal.

## See also

- [Build Game Mechanics](index.md)
- [Lock several entities for one action](cover-transactions.md)
- [Validated action request](validated-action.md)
- [Custom persistent character state](persistent-state.md)
- [Remote Calls](../../reference/scripting/remote-calls.md)
- [Script Lifecycle and Concurrency](../scripting/lifecycle-and-concurrency.md)
- [Managed C# Scripting](../scripting/managed-csharp.md)
- [Migrate Saved Data and Content in a Game Project](../project/data-migrations.md)
