---
layout: default
title: Validated action request
locale: en
document_id: recipe-validated-action
permalink: /Docs/en/how-to/mechanics/validated-action.html
recipe_kind: primitive
---

# Validated action request

A player clicks a device and the client asks the server to activate it. This recipe treats that remote call as an intent, not as a command: the server derives who acts from the connection, resolves the named target only where the actor can reach it, runs every check in a fixed order, and only then charges the cost and applies the effect. Forged and stale requests are refused with a logged reason and change nothing.

| Backend | Status |
| --- | --- |
| AngelScript | `runtime-verified` |
| Managed C# | `shape` |

## Source paths inspected

- `Examples/MechanicsSandbox/Scripts/Recipes/ValidatedAction.fos`
- `Examples/MechanicsSandbox/Scripts/Tests/Test_ValidatedAction.fos`
- `Examples/MechanicsSandbox/Scripts/Sandbox.fos`
- `Examples/MechanicsSandbox/Content/ValidatedAction.fopro`
- `Examples/MechanicsSandbox/Managed/Recipes/ValidatedAction.cs`
- `Examples/MechanicsSandbox/mechanics-smoke.json`
- `Examples/MechanicsSandbox/FOnlineMechanicsSandbox.fomain`
- `Source/Server/Server.cpp`
- `Source/Server/ClientDataValidation.cpp`
- `Source/Server/EntitySync.cpp`
- `Source/Scripting/AngelScript/AngelScriptAttributes.cpp`
- `Source/Scripting/AngelScript/AngelScriptRemoteCalls.cpp`
- `Source/Scripting/Managed/Analyzers/SyncCoverAnalyzer.cs`
- `Docs/generated/api.json`

## Problem

A device on the map belongs to one character, who may activate it from at most two hexes away for one point of action energy. The client knows which device the player clicked; it does not decide whether the activation happens. A modified client can send any id, and an honest client can send an id that went stale while the request was in flight. Which devices exist, their reach and their price are game policy. What this recipe teaches is the part the Engine leaves to every project: where each fact comes from, the order of the checks, and the point where the cost is charged.

## Engine primitives used

| Symbol | Side | Why |
| --- | --- | --- |
| `script.method.server.Player.GetControlledCritter` | server | The actor comes from the calling player, never from the payload |
| `script.method.server.Critter.GetMap` | server | The actor's map bounds where a target may be |
| `script.method.server.Game.Sync#b42b6f6d6b684d2a` | server | Covers the player (with its critter) and the map in one request |
| `script.property.Critter.MapId` | server | Proves the actor is still on the covered map after the acquisition |
| `script.method.server.Map.GetItem` | server | Resolves the claimed id only among the items of that map |
| `script.method.server.Game.GetDistance#3dade8869ec8c0ac` | server | Reach between the actor and the device |
| `script.method.common.Game.Log` | both | Refusal reasons on the server, the reply on the client |
| `script.method.server.Map.AddItem#93da72a5ec69c371` | server | The test places the devices |
| `script.method.server.Game.LoginPlayerToNewRecord`, `script.method.server.Player.SwitchCritter` | server | The sandbox login gives the headless client a controlled critter |
| `setting.server.ServerNetwork.MaxRemoteCallPayloadSize` | server | Server-wide payload ceiling under every per-call `MaxBytes` |

The `///@ Property` and `///@ RemoteCall` declarations, `[[ServerRemoteCall]]`, `[[ClientRemoteCall]]`, `[[Async]]`, `Player.ServerCall`, `Player.ClientCall`, `Entity.Id` and `Entity.ProtoId` are project metadata or backend built-ins, outside the generated API model; their grammar is in [Remote Calls](../../reference/scripting/remote-calls.md#declaration-grammar). The Engine has no "use" event or action pipeline: the remote call and its handler are that pipeline.

## Minimal example

| File in `Examples/MechanicsSandbox` | Role |
| --- | --- |
| `Scripts/Recipes/ValidatedAction.fos` | Declarations, the server handler and the client reply handler |
| `Content/ValidatedAction.fopro` | The `ValidatedAction_Device` item prototype |
| `Scripts/Tests/Test_ValidatedAction.fos` | Builds the targets, drives a real headless client, checks what changed |
| `Scripts/Sandbox.fos` | Shared map creation and the headless-client login |
| `Managed/Recipes/ValidatedAction.cs` | The same declarations and flow in Managed C# |

## Step by step

### 1. Declare the request and the reply

```angelscript
///@ Property Critter Common int32 ActionEnergy Mutable Persistent OwnerSync
///@ Property Item Server ident DeviceOwnerId Mutable Persistent
///@ Property Item Common int32 DeviceActivations Mutable Persistent PublicSync

///@ RemoteCall Server ActivateDevice(ident deviceId) MaxBytes 8
///@ RemoteCall Client DeviceActivationResult(ident deviceId, bool accepted) MaxBytes 9
```

The request carries one `ident`, and `MaxBytes 8` is exactly its size; the reply is an `ident` and a `bool`. Ownership is a `Server` property, so no client ever receives it. Energy is `OwnerSync` so the owner's client can grey out a button, but the server reads it again before acting.

### 2. Derive the actor from the calling player

```angelscript
    // The engine supplies the calling player; nothing in the payload names the actor
    Critter? actor = player.GetControlledCritter();
    if (actor == null) {
        Refuse(player, deviceId, "no_actor");
        return;
    }

    Map? map = actor.GetMap();
    if (map == null) {
        Refuse(player, deviceId, "actor_not_on_map");
        return;
    }
```

The Engine passes the `Player` of the connection that sent the call as the first handler argument; the declaration has no actor argument at all. A player without a controlled critter, or with a critter on the global map, is refused before any payload id is looked at.

### 3. Cover the map, then derive again

```angelscript
    // The entry cover holds the player and its critter; map items need the map, and Sync replaces the set
    Game.Sync(player, map);

    // Other work may run while the cover changes hands, so the actor and its map are derived again
    actor = player.GetControlledCritter();
    if (actor == null || actor.MapId != map.Id) {
        Refuse(player, deviceId, "actor_moved");
        return;
    }
```

The dispatcher enters the handler with the player and its controlled critter covered ([native entry covers](../scripting/lifecycle-and-concurrency.md#native-entry-covers)). Reading map items needs the map, and `Game.Sync` replaces the cover with the complete set. When the map is busy, the acquisition releases what the thread holds, the entry cover included, waits, and takes everything back before it returns (`SyncContext::SyncEntities` and `SyncContext::AcquireLocks` in `Source/Server/EntitySync.cpp`). Another job can move the actor during that wait, so the handler derives the actor again and checks that it is still on the covered map.

### 4. Resolve the target, then check it

```angelscript
    // Only items on the actor's own map resolve; an id from anywhere else is not a target
    Item? device = map.GetItem(deviceId);
    if (device == null || device.ProtoId != DevicePid) {
        Refuse(player, deviceId, "unknown_target");
        return;
    }

    if (device.DeviceOwnerId != actor.Id) {
        Refuse(player, deviceId, "not_owner");
        return;
    }

    if (Game.GetDistance(actor, device) > ActivationDistance) {
        Refuse(player, deviceId, "too_far");
        return;
    }

    if (actor.ActionEnergy < ActivationCost) {
        Refuse(player, deviceId, "no_energy");
        return;
    }
```

The order is deliberate:

1. Identity and kind. `Map.GetItem` resolves only items of the actor's map, so an id of a device on another map is `unknown_target` even when the actor owns it; a global lookup such as `Game.GetItem` would find it and leave the map check to be remembered. The prototype check stops the action from applying to any other item.
2. Ownership, read from the server-only property.
3. Reach. It changes with movement, so honest clients hit it too.
4. Affordability, last, because it is the value about to be charged.

### 5. Charge the cost, then apply the effect

```angelscript
    // Every check that can refuse has passed: charge the cost, then apply the effect
    actor.ActionEnergy = actor.ActionEnergy - ActivationCost;
    device.DeviceActivations = device.DeviceActivations + 1;

    Game.Log("validated_action_accepted device=" + deviceId.value);
    player.ClientCall.DeviceActivationResult(deviceId, true);
```

Nothing after the charge can refuse, and no suspension separates the last check from the write, so a refused request leaves no trace and nothing needs a refund. The charge and the effect run under the same cover as the checks.

### 6. Refuse with a logged reason

```angelscript
// Logs the caller and the claimed id for diagnosis; the reply tells the client only that it failed
void Refuse(Player player, ident deviceId, string reason)
{
    Game.Log("validated_action_refused reason=" + reason + " player=" + player.Id.value + " device=" + deviceId.value);
    player.ClientCall.DeviceActivationResult(deviceId, false);
}
```

The log line names the caller and the claimed id, not the whole payload. The reply says only `false`: `not_owner` versus `unknown_target` would tell a probing client whether a device with that id exists somewhere.

### 7. Present the reply on the client

```angelscript
[[ClientRemoteCall]]
void DeviceActivationResult(ident deviceId, bool accepted)
{
    Game.Log("validated_action_result device=" + deviceId.value + " accepted=" + (accepted ? "true" : "false"));
}
```

The client handler only presents. A real interface would update a widget; it never changes authoritative state.

## Authority and sync

| Fact | Where it comes from |
| --- | --- |
| Who acts | The Engine-supplied `Player`, then `GetControlledCritter()` |
| Where | The actor's `GetMap()`; the client never names a map |
| Which device | The client's `deviceId`, resolved only on that map |
| Owner, reach, energy | Server properties and `Game.GetDistance`, read under the new cover |
| Result | The server's reply; the client only displays it |

Transport checks run before the handler. `ServerEngine::Process_RemoteCall` in `Source/Server/Server.cpp` resolves the call by name and throws when the payload is larger than the call's `MaxBytes` or `ServerNetwork.MaxRemoteCallPayloadSize` (`setting.server.ServerNetwork.MaxRemoteCallPayloadSize`, default 1 MiB), before the body is copied. `ValidateInboundRemoteCallData` in `Source/Server/ClientDataValidation.cpp` then checks the declared shape and collection counts, and only after that is the player covered and the handler invoked; the AngelScript decoder repeats the size check for every inbound call (`AngelScriptRemoteCalls.cpp`). A call that fails these checks never reaches the handler. A well-formed request with a forged id is a domain question, and the handler answers it with a refusal ([authority and failure handling](../../reference/scripting/remote-calls.md#authority-and-failure-handling)).

The player job of one connection processes its messages in order (`ServerEngine::ProcessPlayer`), so its handlers start in arrival order. A handler that suspends with `Yield` returns to that loop, and the next request of the same client can run before it resumes. If an action needs a delay, keep ids across it and repeat steps 2 to 5 afterwards, as in [Lock several entities for one action](cover-transactions.md#widening-and-revalidation-after-suspension); never charge before a suspension and check after it.

## Persistence and migration

`ActionEnergy`, `DeviceOwnerId` and `DeviceActivations` are `Mutable Persistent`, so they are saved with their entities. Whether a device itself is restored depends on whether it and the map that lists it are persistent ([entity ownership and persistence](../../explanation/runtime/server.md#entity-ownership-and-persistence)); the test runs on the `Memory` database and does not restart. A renamed property needs `///@ MigrationRule Property <Entity> Rename <Old> <New>` ([data migrations](../project/data-migrations.md)). Changing the remote-call name, arguments or limits is a network-contract change: client and server must come from the same bake ([compatibility and release practice](../../reference/scripting/remote-calls.md#compatibility-and-release-practice)).

## Pitfalls and anti-patterns

- Accepting an actor, owner or map id from the payload. The calling `Player` is the only identity the Engine vouches for ([authority and failure handling](../../reference/scripting/remote-calls.md#authority-and-failure-handling)).
- Resolving the target with a global lookup. `Game.GetItem` finds a device anywhere in the world; `Map.GetItem` on the actor's own map makes "reachable" part of the lookup.
- Charging the cost before a check that can still refuse. The player then pays for a refusal, or the code grows a refund path that a forged request can exploit; charge after the last refusing check, as in step 5.
- Keeping a decision across an acquisition or a `Yield`. `Game.Sync` can release and reacquire the thread's cover, and no cover survives a suspension ([the suspension rule](../scripting/lifecycle-and-concurrency.md#the-suspension-rule)); derive and check again under the new cover.
- Omitting `MaxBytes` and `MaxCollectionSize`. An omitted limit is recorded as `0`, an unlimited structural value; only the 1 MiB server-wide ceiling remains ([arguments and serialization](../../reference/scripting/remote-calls.md#arguments-and-serialization)).
- Building display text on the server. The server has no `Game.GetText`; send a code or a text key and let the client resolve it in its language ([runtime script API](../content/text-and-localization.md#runtime-script-api)).
- Testing the handler with a direct call. AngelScript rejects a direct call to a `[[ServerRemoteCall]]` function when it compiles or loads the module (`DIRECT_CALL_BLOCKING_ATTRIBUTES` in `AngelScriptAttributes.cpp`; only namespaces listed in `AngelScript.AttributedFunctionDirectCallAllowedNamespaces` are exempt, and the sandbox lists none), and Managed C# reports it as `FOENTRY001`. A direct call would also skip the payload limits, the engine-supplied `Player` and the entry cover, which are what this recipe is about.

## Test route

The `validated-action` scenario in `mechanics-smoke.json` starts a server with the `RecipeNetworkTests` sub-config and `-Testing.Filter validated_action.`, waits for its ready marker `validated_action_waiting_for_client`, and then starts a headless client with `RecipeClient`:

```ini
# Server side of tests that need a connected headless client
[SubConfig]
Name = RecipeNetworkTests
Testing.Enabled = True
Testing.TestTimeout = 60000
Server.DbStorage = Memory
```

`RecipeClient` turns on `Sandbox.ClientAutomation` with a null renderer and no audio. The client connects, enters the sandbox map through `Sandbox::EnterSandbox`, and the server test sends it one drill: a device the actor owns on another map, a device owned by nobody, an owned device eight hexes away, the valid device, and the valid device again with no energy left.

```angelscript
    // Energy for exactly one activation, so the repeated valid request must be refused for its cost
    actor.ActionEnergy = ValidatedAction::ActivationCost;
    actor.TestValidTargetId = own.Id;

    ident[] drill = {elsewhere.Id, foreign.Id, far.Id, own.Id, own.Id};
    player.ClientCall.RunDeviceDrill(ctx.Id, drill);
    Game.Log("validated_action_drill_sent");
```

The client sends the five requests through the generated caller and then `DeviceDrillSent`; when that last call arrives, the test checks that the energy is `0` and that only the valid device has one activation. The drill call itself is declared with `MaxBytes 64 MaxCollectionSize 8`. The runner requires these server markers, in addition to `[Testing] PASS validated_action.client_requests_are_validated_on_the_server` and `[Testing] COMPLETE status=passed`:

```text
validated_action_refused reason=unknown_target
validated_action_refused reason=not_owner
validated_action_refused reason=too_far
validated_action_accepted device=
validated_action_refused reason=no_energy
```

Run the whole sandbox from `Examples/MechanicsSandbox` with `python validate.py` (Windows) or `python3 validate.py` (Linux); the runner has no per-scenario filter. With an existing build and bake, on Windows from the build directory:

```powershell
py -3 "<sandbox>\run_mechanics_smoke.py" .\Binaries\Server-Windows-win64\FOMECH_ServerHeadless.exe .\Binaries\Client-Windows-win64\FOMECH_ClientHeadless.exe "<sandbox>\FOnlineMechanicsSandbox.fomain"
```

Runtime verification: the scenario passed on a Windows host with a real headless client connected to the server; the server logged the four refusals with their reasons in the drill order and one acceptance, and the client logged five replies, one of them `accepted=true`. Not verified: a Linux run, the Managed C# backend, the `no_actor`, `actor_not_on_map` and `actor_moved` refusals, oversized or malformed payloads (the generated caller cannot produce them), concurrent requests from several clients, and persistence across a restart.

## Managed C#

Status: `shape`. `Managed/Recipes/ValidatedAction.cs` declares the same `///@` lines and follows the same steps, but no Engine lane compiles it, so neither the compiler nor the analyzers have judged it.

```csharp
        // The entry cover holds the player and its critter; map items need the map, and Lock replaces the set
        if (!await Sync.Lock(player, map)) {
            return;
        }

        // Other work may run while the cover changes hands, so the actor and its map are derived again
        actor = player.GetControlledCritter();
        if (actor == null || actor.MapId != map.Id) {
            Refuse(player, deviceId, "actor_moved");
            return;
        }
```

- The handler is `[ServerRemoteCall] internal static async Task ActivateDevice([RequiresCover] Player player, ident deviceId)`: it is `internal` (`FOENTRY002`) and declares `[RequiresCover]` on the engine-synchronized `Player` (`FOSYNC003`); see [Engine-contract analyzers](../project/engine-contract-analyzers.md) and [server entity synchronization](../scripting/managed-csharp.md#server-entity-synchronization).
- `Sync.Lock` replaces `Game.Sync`, which `FOSYNC005` rejects outside the `Sync` helpers. It answers whether the entities are still available, and `FOSYNC010` forbids discarding that answer. This shape returns without a reply when the answer is `false`; send a refusal there if the client waits for one.
- An `await` ends the cover; `FOSYNC009` reports a covered value used after an `await` that nothing re-proved.

## How a larger project extends it

- **Data instead of constants.** Device kinds become prototype property components or a FixedType catalog, and reach and cost become `///@ Setting` values; see [Choose How to Represent Game Data](../../explanation/entity-and-property-model/choosing-a-representation.md).
- **Honest races versus forgery.** A stale target, a moved actor or an exhausted resource can come from an honest client: refuse, log at a low level and tell the player. A well-formed value no honest client can produce, such as a negative count or an option the interface never offers, can be treated as a protocol violation that the project logs loudly or disconnects for.
- **Localized refusal feedback.** Reply with an integer-backed enum reason or a `TextPackKey` instead of `false`, and let the client resolve it with `Game.GetText`, which exists only on the client and mapper. Keep the reasons coarse where a precise one would reveal hidden state.
- **Shared rules.** A pure rule function compiled for both sides can grey out the button on the client; the server still runs it on its own data.
- **Costs and limits.** Cooldowns and request rate limits are project policy. `ServerNetwork.MaxPendingRemoteCalls` and the runtime age limits bound unfinished handlers, not request frequency ([runtime monitoring](../../reference/scripting/remote-calls.md#runtime-monitoring-and-disconnection)).
- **Project events.** Fire a project event such as "device activated" after the state change is complete, so subscribers see a consistent world.
- **Catalog review.** Generate the project remote-call catalog from the bake and review its limits ([generate a project catalog](../../reference/scripting/remote-calls.md#generate-a-project-catalog)).

## See also

- [Build Game Mechanics](index.md)
- [Lock several entities for one action](cover-transactions.md)
- [Remote Calls](../../reference/scripting/remote-calls.md)
- [Script Lifecycle and Concurrency](../scripting/lifecycle-and-concurrency.md#server-entity-synchronization)
- [Managed C# Scripting](../scripting/managed-csharp.md)
- [Text and Localization](../content/text-and-localization.md)
- [Generated method reference](../../reference/script-api/methods.md)
