---
layout: default
title: First Automated Test
locale: en
document_id: first-test-tutorial
permalink: /Docs/en/tutorials/first-test.html
---

# First Automated Test

Extend the three-layer executable checks in
[Minimal Multiplayer](../../../Examples/MinimalMultiplayer/README.md).

## Test layers

`RunTutorialChecks` runs these layers in order:

1. **Baked metadata check.** `run_tutorial_smoke.py` decodes server and client
   metadata, requires symmetric remote-call declarations, and confirms that
   `SuppliesCollected` exists on both sides.
2. **Server content test.** The `TutorialContentTest` sub-config starts an
   isolated in-memory server with networking disabled. Script assertions create
   the location/map, add the NPC and item, and validate map invariants.
3. **Client-visible smoke.** A headless client connects to a real listening
   server, logs in, loads the map, collects the item, observes the synchronized
   count, and requests clean shutdown.

The headless client proves connection, baking, map load, localization, remote
calls, and replication. It does not prove pixels, input ergonomics, audio, or
GPU behavior; keep a visible pass for those claims.

## Run all layers

From a standalone example checkout:

```powershell
cmake --preset windows
cmake --build --preset windows-check
```

Use `linux` and `linux-check` on Linux. `python validate.py` selects these
commands for the current host.

Engine CI uses the equivalent reusable route:

```powershell
cd Examples\MinimalMultiplayer
python validate.py
```

## Add one content invariant

Suppose the project adds an item with stable name `TutorialBeacon`. Add its
prototype first, then extend `RunContentTest()` in
[`Scripts/Tutorial.fos`](../../../Examples/MinimalMultiplayer/Scripts/Tutorial.fos):

```angelscript
const hstring BeaconPid = "TutorialBeacon".hstr();

void RunContentTest()
{
    verify(Game.CheckProtoItem(BeaconPid), "Tutorial beacon prototype is missing");
    // Keep the existing assertions and clean shutdown.
}
```

Keep the constant with the other stable IDs and preserve the existing body;
the abbreviated function above only shows the new assertion. Run the complete
check. Then temporarily misspell the ID and confirm the content-test layer
fails before the multiplayer client starts. Restore the valid ID and rerun.

This fail-then-pass replay proves the assertion is active. A test that has only
ever been observed green is weaker evidence.

## Marker and timeout policy

Markers are a small public contract between the example scripts and runner:

- use stable, machine-searchable lowercase names;
- emit a marker only after the asserted state exists;
- require process exit code zero and every marker;
- bound every process wait;
- terminate both processes on failure;
- label captured server/client output and reject every missing marker.

Do not replace state assertions with log assertions. Script `verify(...)`
checks the world invariant; the marker tells the external runner which verified
milestone completed.

## Persistence scope

`SuppliesCollected` and `TutorialAccount` are declared `Persistent`, and the
baked metadata test protects those declarations. The tutorial uses
`Server.DbStorage = Memory`, so it does not claim persistence across server
process restarts. A game that claims restart durability must run an additional
test against its supported database backend and verify the restored entity
state.

## Recovery

- Metadata fails before startup: compare both baked sides and remote-call
  signatures; do not weaken the decoder.
- Client connects but never loads the map: inspect server world creation and
  player/critter switching before adding longer sleeps.
- A test is flaky: wait on semantic milestones with a bounded timeout; avoid
  fixed delays as success criteria.

For reusable fixture, marker, timeout, cleanup, and process-report semantics,
continue with [Gameplay and Integration Testing](../../GameplayTesting.md). For
native Catch2 ownership and coverage, use [Testing](../../Testing.md).
