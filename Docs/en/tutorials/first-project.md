---
layout: default
title: First FOnline Headless Project
locale: en
document_id: legacy-tutorial-entry
permalink: /Docs/en/tutorials/first-project.html
---

# First FOnline Headless Project

This tutorial runs the engine-owned minimal project without another game repository. At the end, FOnline has configured and built a baker and headless server, baked an AngelScript module, started the server with an in-memory database, executed the project start callback, and shut down cleanly.

This is the first supported milestone, not a playable game. The starter deliberately has no client, map, critter, item, dialog, or GUI.

## Prerequisites

Use a standalone FOnline checkout with submodules initialized. The validation wrapper requires:

- Git;
- Python 3;
- CMake 3.22 or newer;
- a C++20 toolchain supported by the host platform;
- the platform prerequisites described in [Build Workflow](../how-to/build/).

The canonical scaffold is [Examples/MinimalProject](../../../Examples/MinimalProject/README.md). Do not use an embedding game's files to complete this tutorial.

## Run the starter

From the engine repository root, run the command for the current host.

Windows x64:

```powershell
cd Examples\MinimalProject
python validate.py
```

Linux x64:

```bash
cd Examples/MinimalProject
python3 validate.py
```

BuildTools recreates `Workspace/validation-project`, copies the canonical scaffold into it, links that copy back to the current engine checkout, configures the server and baker, builds them, bakes resources, and runs the `StarterSmoke` sub-config. The smoke runner has a 60-second timeout.

## Confirm success

Near the end of the output, the server must report:

```text
starter_project=FOnline Starter
starter_native_extension_value=42
starter_server_started
starter_smoke_passed
[starter-smoke] required lifecycle markers found
```

The command must exit with code zero. The native value proves that the server-scoped C++ source entered `ServerLib`, codegen generated its script binding, and baked AngelScript called it. The lifecycle markers prove that the module subscribed to `Game.OnStart` and ran after server initialization; a native process that merely started and stopped is not sufficient.

Generated files stay under `Workspace/`:

- `Examples/MinimalProject/Build/windows/` is the Windows build/output tree;
- `Examples/MinimalProject/Build/linux/` is the Linux build/output tree.

## Read the complete project

The starter is small enough to inspect in one pass:

| File | Responsibility |
|---|---|
| `Examples/MinimalProject/CMakeLists.txt` | Composes the public FOnline CMake stages and defines `RunStarterSmoke`. |
| `Examples/MinimalProject/FOnlineStarter.fomain` | Declares the minimum runtime settings, resource packs, and deterministic smoke sub-config. |
| `Examples/MinimalProject/Scripts/Starter.fos` | Defines a project setting, subscribes to server start, logs lifecycle markers, and requests a successful smoke shutdown. |
| `Examples/MinimalProject/StarterServerExtension.cpp` | Demonstrates one server-scoped native script export and one optional engine hook. |
| `Examples/MinimalProject/run_starter_smoke.py` | Enforces timeout, process result, native/script lifecycle markers, and baked remote-call metadata. |

Two constraints are intentional:

1. The example does not enable the `Config` baker. That baker is for distributable runtime configuration and requires every engine setting to be initialized explicitly.
2. The example does not import all engine `CoreScripts`. Several client-facing core modules require project enums, settings, and generated GUI symbols. Add them only with the matching project contracts.

## Make and observe a script change

In `Examples/MinimalProject/Scripts/Starter.fos`, change the string returned by `GetProjectName()` while leaving the required smoke markers intact. Run the same validation command again.

The new value must appear in the `starter_project=...` line, followed by both required markers. This second run proves that the changed AngelScript source was copied and rebaked rather than served from stale workspace data.

## Troubleshooting

- `ConfigBakerException: Main config baking error`: a `Config` resource pack was added without a complete project configuration. Remove it for this milestone or initialize the complete settings contract before packaging.
- `Nothing was built in the module`: one generated runtime side became empty after preprocessing. Keep at least one valid common declaration or split the project modules according to their supported sides.
- Missing `WorldTime`, `GuiScreen`, or `GuiScreens` symbols: the full `CoreScripts` directory was imported without its project-owned metadata or generated GUI contracts.
- `[starter-smoke] server timed out`: inspect the preceding startup output. The script callback probably did not request shutdown, or initialization blocked before `Game.OnStart`.
- Missing lifecycle markers with exit code zero: treat the run as failed; `run_starter_smoke.py` rejects this case.

## Next steps

Continue with [First Playable Client](first-client.md),
[First Content Change](first-content.md), and
[First Automated Test](first-test.md). Use
[Getting Started](getting-started.md) for the wider documentation map,
[Embedding FOnline in a Game Project](../how-to/build/embedding-project.md) before turning
the scaffold into a separate repository, and
[Baking Pipeline](../explanation/content-pipeline/baking.md) before adding resource types.
