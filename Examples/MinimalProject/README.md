---
permalink: /Examples/MinimalProject/README.html
locale: en
document_id: minimal-project-readme
---

# FOnline Minimal Project

This is the engine-owned executable starter and opt-in validation project. It is intentionally small enough to explain completely:

- `CMakeLists.txt` composes FOnline through the public stage helpers and routes
  one server-only `INTERFACE` dependency through the current revision-pinned
  `FO_SERVER_LIBS` integration list;
- `CMakePresets.json` provides standalone Windows x64 and Linux GCC configure/smoke presets;
- `FOnlineStarter.fomain` defines one smoke sub-config and the minimum reusable resource packs;
- `Scripts/Starter.fos` subscribes to the server start event and declares one remote call in each direction;
- `StarterServerExtension.cpp` demonstrates a server-scoped native script export and an optional engine hook;
- `run_starter_smoke.py` enforces a timeout, native/script lifecycle markers, and paired baked remote-call metadata;
- `validate.py` selects the supported host preset and runs the complete configure/build/smoke route;
- `Engine/` is supplied as a link/submodule by the validation wrapper or by a game-project checkout.

The project has no maps, critters, items, dialogs, GUI, authentication, or product policy. Those belong in later tutorials and public showcase repositories.

## Automated smoke

From the example root with `Engine/` initialized:

```bash
cd Examples/MinimalProject
python validate.py
```

The local validator configures/builds the headless server and baker, bakes resources, then runs `RunStarterSmoke`. The current required Engine workflow does not execute this route.

Success requires the server log to reach every marker and exit with code zero:

```text
starter_native_extension_value=42
starter_server_started
starter_smoke_passed
```

The native marker proves that a `SERVER` source was compiled into `ServerLib`, its `ExportMethod` declaration entered codegen, and baked AngelScript called the generated `Game.NativeStarterValue()` binding at runtime. The same translation unit requires `FO_STARTER_PROJECT_DEPENDENCY=1`, so compilation also proves that the server-role `INTERFACE` target propagated through `FO_SERVER_LIBS`. The visibility hook proves optional hook discovery and fallback suppression during code generation. The self-contained runner also rejects a process that hangs for more than 60 seconds, exits without every marker, or produces inconsistent `Metadata.fometa-server` / `Metadata.fometa-client` contracts. The baked outputs must contain `script.remote-call.server.StarterPing` and `script.remote-call.client.StarterNotice`; this exercises the same `MetadataBaker` format consumed by the richer Engine-side `BuildTools/docs_metadata.py` catalog generator without making the standalone example depend on that documentation tool. The Windows x64 route was run successfully on 2026-07-31. These standalone build routes are opt-in local evidence; the current Engine validation workflow does not execute them.

`FO_SERVER_LIBS` is current revision-pinned integration state, not a helper declared by `BuildTools/cmake/ProjectInterface.json`. Re-audit it whenever the Engine pin changes.

## Deliberate limits

The starter does not enable the `Config` baker because distributable configuration requires a complete explicit settings set. It also does not import all engine `CoreScripts`; client-facing core modules depend on project-owned settings, enums, and generated GUI symbols. These are integration contracts for later examples, not hidden starter prerequisites.

## Manual project checkout

To use the scaffold outside the engine validation wrapper, copy this directory as a new repository and add FOnline at `Engine/`. Configure from the project root with the platform/toolchain appropriate for that host, build `BakeResources` and `FOSTART_ServerHeadless`, then run the generated headless server with:

```text
-ApplyConfig <project-root>/FOnlineStarter.fomain -ApplySubConfig StarterSmoke
```

With `Engine/` initialized, the standalone preset route is:

```bash
# Debian/Ubuntu host prerequisites (first Linux run only)
Engine/BuildTools/prepare-workspace.sh linux-packages linux

# Cross-platform wrapper (Windows or Linux)
python validate.py

# Equivalent explicit commands
# Windows
cmake --preset windows
cmake --build --preset windows-smoke

# Linux
cmake --preset linux
cmake --build --preset linux-smoke
```

The repository workflows run the same Engine-owned Linux preparation command before validation, so their system package set stays aligned with the pinned revision rather than being duplicated in example YAML.

The Windows preset deliberately leaves generator selection to CMake, which chooses the newest installed Visual Studio; pinning `Visual Studio 17 2022` would reject otherwise compatible newer installations. The presets build only the headless server and baker required by `RunStarterSmoke`; project-specific client, mapper, editor, packaging, and deployment presets belong in later examples. Do not rename source/config identifiers one at a time. Change `FO_DEV_NAME`, `FO_NICE_NAME`, the `.fomain` filename, and any package/application identity together as one project-bootstrap operation.

The tested first lesson is
[First FOnline Headless Project](../../Docs/en/tutorials/first-project.md).
