---
layout: default
title: First Playable Client
locale: en
document_id: first-client-tutorial
permalink: /Docs/en/tutorials/first-client.html
---

# First Playable Client

Build and run a desktop client connected to the engine-owned
[Minimal Multiplayer](../../../Examples/MinimalMultiplayer/README.md) server.

## Prerequisites

Complete the headless tutorial first. A standalone checkout keeps FOnline as
the `Engine/` submodule beside the example's `CMakeLists.txt`.

From the standalone example root, run:

```powershell
python validate.py
```

On Linux, use `python3 validate.py`. The command configures and builds the
desktop client, headless client, headless server, and baker, then runs the full
automated lesson.

Engine maintainers can exercise the checked-in source without materializing a
separate repository:

```powershell
python BuildTools/buildtools.py validate win64-tutorial-smoke
```

Use `linux-tutorial-smoke` on Linux.

## Start the visible client

After `validate.py` succeeds, open two terminals in the standalone example
root. Start the server first:

```powershell
Build\windows\Binaries\Server-Windows-win64\FOMM_ServerHeadless.exe -ApplyConfig FOnlineMinimalMultiplayer.fomain
```

Then start the desktop client:

```powershell
Build\windows\Binaries\Client-Windows-win64\FOMM_Client.exe -ApplyConfig FOnlineMinimalMultiplayer.fomain
```

The generated platform directory may differ with the host and generator. The
client connects to `127.0.0.1:4010`, logs in through
`Tutorial::EnterWorld()`, and loads `TutorialMap`. Press Space after the map
appears. The client calls `CollectSupply()`, the server increments
`SuppliesCollected`, and the client displays the synchronized value.

Expected log milestones include:

```text
tutorial_client_connected
tutorial_server_world_ready
tutorial_client_map_loaded
tutorial_server_supply_collected=1
tutorial_client_supply_collected=1
```

## Read the client/server boundary

The complete behavior is in
[`Scripts/Tutorial.fos`](../../../Examples/MinimalMultiplayer/Scripts/Tutorial.fos):

1. Client `Game.OnStart` binds the Engine font and calls `Game.Connect()`.
2. Client `Game.OnConnected` invokes the server remote call `EnterWorld()`.
3. The server creates the location, map, critters, and item, logs in the player,
   then calls `WorldReady()`.
4. Client `Game.OnMapLoaded` enables input and renders the map plus a small
   interface overlay.
5. Space sends only an intent. The server owns item creation and the replicated
   property mutation.

The client owns presentation and intent; the server owns authoritative world
and persistent state.

## Configuration contract

[`FOnlineMinimalMultiplayer.fomain`](../../../Examples/MinimalMultiplayer/FOnlineMinimalMultiplayer.fomain)
is generated from current Engine setting declarations because distribution
config baking requires a complete server/client setting set. Project choices
remain explicit in `generate_config.py`; edit its `OVERRIDES` or
`PROJECT_SECTIONS`, regenerate the `.fomain`, and keep `--check` green.
Ordinary runtime startup applies settings in this order:

1. Engine defaults;
2. project config or packaged internal config;
3. selected sub-configs;
4. the writable local-config cache;
5. command-line overrides;
6. derived auto settings.

The desktop target stages `FOMM_BakerLib` beside the host so an unpackaged
launch can prebake changed sources. The stricter distribution `Config` baker
requires every saved project setting to be initialized; `CheckTutorialConfig`
rejects drift before baking or packaging.

## Recovery

- `Connection refused`: start the server first and confirm port `4010` is free.
- `Config file not found`: run from the example root or pass the full path to
  `-ApplyConfig`.
- Missing `FOMM_ClientLib.dll` or `FOMM_BakerLib.dll`: rebuild the
  `windows-check` preset; do not copy a DLL from another project.
- A map or script change is ignored: run `cmake --build --preset windows-check`
  to bake and replay the complete route.

Continue with [First Content Change](first-content.md).
