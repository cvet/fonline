---
permalink: /Examples/MinimalMultiplayer/README.html
locale: en
document_id: minimal-multiplayer-readme
---

# FOnline Minimal Multiplayer

A standalone first playable project after
[`MinimalProject`](../MinimalProject/README.md), using only public Engine
interfaces and Engine-owned resources.

The final state demonstrates:

- a native client connected to a headless server;
- one generated location and one baked map;
- a player critter, an NPC, and a map item using Engine fallback sprites;
- a client-to-server item interaction;
- one `PublicSync Persistent` critter property;
- paired client/server remote calls and lifecycle events;
- distance-based critter visibility through one small public server Engine hook;
- English and Russian prototype text;
- a server-side content test and a complete client-visible smoke test;
- native client/server archives with inventory, hashes, and packaged runtime evidence;
- one Engine-owned SPARK source/texture fixture for Mapper, Particle Preview,
  Particle Viewer, and documentation capture.

## Prerequisites

- CMake 3.22 or newer;
- Python 3;
- Visual Studio with C++ support on Windows, or GCC plus Ninja Multi-Config on
  Linux;
- the Engine checkout at `Engine/`:

```bash
git clone --recursive https://github.com/cvet/fonline-minimal-multiplayer.git
```

When running from the Engine repository rather than the standalone example,
make `Engine` point back to that checkout before configuring:

```powershell
New-Item -ItemType Junction -Path Engine -Target ..\..
```

## Validate everything

Windows:

```powershell
python validate.py
```

Linux:

```bash
python3 validate.py
```

The command configures the project, bakes resources, builds the desktop client,
headless client, and headless server, runs the isolated content test, launches
both applications with the headless client, logs in, loads the map, collects
the item, and verifies baked metadata.

## Validate native packages

After configuring, build the package acceptance target for the current host:

```powershell
cmake --build --preset windows-package
```

```bash
cmake --build --preset linux-package
```

The target force-bakes the `TutorialSmoke` configuration, emits raw client and
server payloads plus ZIP or tar.gz archives, compares archive/payload
inventories, hashes every published archive and payload file, and runs the
packaged headless server/client interaction. Evidence is written under
`Build/<host>/FOMM-Tutorial/` as `tutorial-packaging-manifest.json` and
`tutorial-package-runtime-report.json`.

This qualifies an unsigned headless tutorial archive fixture only. It does not
claim installer, signing, store, public deployment, persistent database,
visible renderer/audio, upgrade, or rollback acceptance. A host becomes a
maintained package claim only after its required CI job retains the
commit-addressed archives and manifests.

## Run it visibly

Configure and build once:

```powershell
cmake --preset windows
cmake --build --preset windows-check
```

Then launch the server and client from two terminals without
`TutorialSmoke`:

```powershell
Set-Location Build\windows
.\Binaries\Server-Windows-win64\FOMM_ServerHeadless.exe -ApplyConfig ..\..\FOnlineMinimalMultiplayer.fomain
.\Binaries\Client-Windows-win64\FOMM_Client.exe -ApplyConfig ..\..\FOnlineMinimalMultiplayer.fomain
```

The exact platform directory can differ with the selected generator. The
desktop client target carries `FOMM_BakerLib` beside the executable, so source
changes are rebaked before a manual launch. The client enters the tutorial map
automatically. Press Space to collect the localized `TutorialSupply` item; the
synchronized counter in the top panel increments.

## Source map

| Path | Owns |
|---|---|
| `generate_config.py` | complete Engine-setting defaults plus reviewed tutorial overrides and sections |
| `FOnlineMinimalMultiplayer.fomain` | generated package-ready runtime settings, resource packs, and test subconfigs |
| `Content/StarterContent.fopro` | player, guide, supply, and location prototypes |
| `Maps/TutorialMap.fomap` | the one-map world |
| `Scripts/Tutorial.fos` | lifecycle, login, interaction, rendering, metadata, and content test |
| `Scripts/MapperCapture.fos` | delayed full-window Mapper documentation capture |
| `SourceExt/ServerExtension.cpp` | distance-based `CheckCritterVisibilityHook` used by the tutorial server |
| `Particles/Documentation.spark` | minimal looping SPARK authoring fixture |
| `assets/provenance.json` | machine-readable license and exact source hash for the Engine-owned particle texture |
| `package-smoke.json` | packaged server/client runtime scenario and semantic evidence |
| `tutorial-smoke.json` | process order, readiness, markers, deadlines, and failure signatures |
| `run_tutorial_smoke.py` | baked metadata checks plus the shared Engine gameplay runner entry point |
| `verify_tutorial_package.py` | archive parity, SHA-256 inventory, packaged runtime, and evidence manifest |
| `validate.py` | host-aware configure/build entry point |

The repository has no project-owned gameplay image or audio catalog. Engine
embedded fonts and built-in fallback sprites keep the playable route runnable.
The single `Radiation.png` particle texture is copied from the MIT-licensed
Engine resources and pinned with its origin revision, path, license, and
SHA-256 in `assets/provenance.json`.

The smoke manifest is executed by
`Engine/BuildTools/gameplay_test_runner.py`. The example owns its process
commands and semantic `tutorial_*` markers; the Engine runner owns manifest
validation, ordered launch, output capture, common deadlines, cleanup, and the
JSON report written under `Workspace/`. See
[Gameplay and Integration Testing](../../Docs/en/how-to/testing/gameplay-and-integration.md) for the
reusable contract.

The package verifier uses the same runner after checking payload/archive
parity. Package grammar and the release evidence boundary are documented in
[Packaging and Release](../../Docs/en/how-to/release/packaging.md).

## Capture the Mapper particle workflow

Build the source fixture, baked resources, Mapper, and the focused viewer:

```powershell
cmake --build Build\windows --config Release --target ForceBakeResources FOMM_Mapper FOMM_ParticleViewer
```

Launch either deterministic documentation profile:

```powershell
Build\windows\Binaries\Mapper-Windows-win64\FOMM_Mapper.exe `
  -ApplyConfig FOnlineMinimalMultiplayer.fomain `
  -ApplySubConfig MapperDocumentationCapture

Build\windows\Binaries\Mapper-Windows-win64\FOMM_Mapper.exe `
  -ApplyConfig FOnlineMinimalMultiplayer.fomain `
  -ApplySubConfig MapperDocumentationSparkEditorCapture
```

Both profiles open `TutorialMap`, fix a `1280x800` viewport, wait for the UI to
settle, request the full Mapper frame, and exit after the deferred capture.
The first selects `Documentation.spk` with fixed seed, scale, and prewarm; the
second opens the authored `Documentation.spark` source directly through
`Mapper.SparkEditorSource` without covering it with Particle Preview.
The TGA output is `MapperDocumentationCapture.tga` beside the launched
process. The checked-in documentation PNGs, exact interaction steps, source
hashes, and recapture triggers live in
[`BuildTools/DocumentationScreenshots.json`](../../BuildTools/DocumentationScreenshots.json).

This profile is evidence for reusable tool documentation. It is not part of
the multiplayer smoke route and does not make SPARK a required backend for
downstream games.

## Lesson route

Use the source in this order:

1. [First Playable Client](../../Docs/en/tutorials/first-client.md)
2. [First Content Change](../../Docs/en/tutorials/first-content.md)
3. [First Automated Test](../../Docs/en/tutorials/first-test.md)

Ordinary runtime startup applies Engine defaults before the project config,
sub-configs, local cache, command-line overrides, and derived auto settings.
The `.fomain` therefore records deliberate project choices instead of copying
every runtime default. The distribution-oriented `Config` baker is stricter and
requires its complete saved settings contract.
