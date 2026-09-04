---
layout: default
title: Build Workflow
locale: en
document_id: build-workflow
permalink: /Docs/en/how-to/build/
---

# Build Workflow

This document explains how to approach FOnline builds without hard-coding assumptions from one project into another.

## Source paths inspected

- `../BuildTools/README.md`
- `../BuildTools/Init.cmake`
- `../BuildTools/validate.sh`
- `../BuildTools/validate.cmd`
- `../BuildTools/buildtools.py`
- `../BuildTools/docs_cli.py`
- `Docs/en/reference/buildtools/index.md`
- `../BuildTools/PackageInterface.json`
- `../BuildTools/docs_package.py`
- `en/reference/packages/index.md`
- `../Examples/MinimalProject/`
- `../Examples/MinimalMultiplayer/`
- `../BuildTools/cmake/stages/Init.cmake`
- `../BuildTools/cmake/stages/ProjectOptions.cmake`
- `../BuildTools/cmake/stages/EngineSources.cmake`
- `../BuildTools/cmake/stages/Codegen.cmake`
- `../BuildTools/cmake/stages/ScriptsAndBaking.cmake`
- `../BuildTools/cmake/stages/Applications.cmake`
- `../BuildTools/cmake/stages/Packages.cmake`
- `../BuildTools/cmake/stages/Finalize.cmake`
- `../BuildTools/cmake/helpers/*.cmake`
- `../Source/Applications/TestingApp.cpp`
- `../Source/Tests/README.md`

## Use the embedding project as the build root

FOnline is normally built through a game repository that embeds the engine as `Engine/`. Configure and build from the game root unless a focused engine-only command explicitly says otherwise.

Reasons:

- Target names are project-defined.
- `.fomain` controls game-specific configuration.
- Generated scripting APIs are project-dependent.
- Package names, signing, resources, and deployment settings belong to the product.
- Platform presets usually live in the embedding project's `CMakePresets.json`.

## Typical workflow

1. Open the game repository root.
2. Inspect available presets with CMake or the IDE integration used by the project.
3. Configure the smallest preset that covers your change.
4. Build the narrowest relevant target.
5. Run the corresponding test, package, or launch target.
6. Update documentation if the workflow or behavior changed.

## Engine-owned first build

The repository includes one stable exception to project-specific target naming: [Examples/MinimalProject](../../../../Examples/MinimalProject/README.md). It proves a clean headless embedding path without Last Frontier, TLA, or another game checkout.

From the engine root, use the host-specific validation target:

```powershell
cd Examples\MinimalProject
python validate.py
```

```bash
cd Examples/MinimalProject
python3 validate.py
```

Both routes configure and build the baker plus headless server, bake the
minimal AngelScript project, run the server with networking disabled and an
in-memory database, and require the lifecycle markers documented in
[First FOnline Headless Project](../../tutorials/first-project.md). Pinned Windows and Linux CI lanes are verified.

The next Engine-owned route builds the desktop client, headless client,
headless server, and baker, then tests metadata, content, login, map loading,
localized text, remote calls, and replicated state:

```powershell
cd Examples\MinimalMultiplayer
python validate.py
```

```bash
cd Examples/MinimalMultiplayer
python3 validate.py
```

The source and manual launch path are documented in
[Minimal Multiplayer](../../../../Examples/MinimalMultiplayer/README.md) and
[First Playable Client](../../tutorials/first-client.md).

## Prerequisites

Use the [Support Matrix](../../reference/platforms/support-matrix.md) before turning a build profile into a release claim. The generated matrix distinguishes required compilation, executable smoke evidence, and source-only profiles; device, renderer, package, service, and store acceptance remain project-owned.

The exact list depends on host OS and target platform, but common tools include:

- Git
- CMake
- Python 3
- A C++20-capable compiler/toolchain
- Platform SDKs for the targets you build
- Visual Studio or Build Tools on Windows-oriented workflows
- Emscripten and Node.js for Web builds
- JDK and Android NDK for Android builds

Prefer the embedding project's documented setup because it may pin specific SDK/tool versions.

### Windows 7 compatibility lane

The `win32-win7` and `win64-win7` build-platform keys are native-Windows MSVC lanes pinned to toolset `v143,version=14.44`; they fail early on a non-Windows host. `FO_BINARY_OUTPUT_POSTFIX` is an independent build identity, not an implication of the `-win7` platform name. When a project builds with a value such as `Win7`, its matching package declaration must use the same value on that one entry: `BINARY Client Windows win32-win7 Raw+Zip+Wix POSTFIX Win7`.

Before packaging or publishing that lane, inspect every linked EXE and DLL:

```powershell
python BuildTools/check_windows7_imports.py <client.exe> <client-runtime.dll>
```

The check parses PE imports and fails on `CreateFile2`, the currently forbidden Windows 8+ import. The embedding project owns the concrete toolset installation, binary paths, package matrix, and CI gate; [Testing](../../contributing/testing/) owns the reusable validation rule.

## Fetching through a mirror of your own

`prepare-workspace` downloads the toolset, the Android SDK/NDK, the MSVC SDK and the LLVM sources from
whoever publishes them. Each of those is a machine you do not run, and a dropped connection costs the
job that is waiting on it. An embedding project may put a host of its own in front of them; the engine
only needs to be told where it is, so nothing about that host is compiled in and everything travels in
the environment:

| variable | what it configures |
|---|---|
| `FO_DOWNLOAD_MIRROR` | Base URL of a pull-through mirror. `https://host/path` is fetched as `<mirror>/host/path` instead. |
| `FO_WORKSPACE_CACHE` | Base URL for prepared workspaces. The MSVC SDK tree is built once, stored under `xwin-<version>-<arches>.tar.gz`, and downloaded whole afterwards. |
| `FO_CI_TOKEN` | Bearer token for the two addresses above. It is sent **only** to their own scheme and host, never to an upstream one. |
| `FO_CI_CA` | Extra trust anchors, added to the system store rather than replacing it, for a machine whose root store cannot be repaired. |

Unset, every one of them leaves the download path exactly as it was.

Two behaviours are deliberate. A download is checked against the upstream `Content-Length`, because a
dropped connection ends the read instead of raising and an archive cut in half unpacks into a failure
far from its cause. And a workspace cache that is empty, unreachable or refusing is only a **miss**:
it exists to make the build faster and independent of other people's servers, not to become another
way for it to fail.

`xwin` fetches the Microsoft packages itself, so mirroring the engine's own downloads does not cover
it — which is why its *result* is what the workspace cache holds.

## Where build logic lives

Use the generated [BuildTools CLI reference](../../reference/buildtools/index.md) for the exact main commands, arguments, defaults, choices, and executable help output.

Use the generated [package interface reference](../../reference/packages/index.md) for `DefinePackage` grammar, accepted targets/platforms/architectures, pack-token compatibility, payload layouts, and output artifacts. Follow [Packaging and Release](../release/packaging.md) for the build/bake/package order, platform procedures, artifact evidence, signing, acceptance, and recovery boundaries. Keep a game's concrete package matrix in that embedding project's documentation.

For a library, SDK, framework, or runtime payload owned by the game repository, follow [Project-Local Dependencies](../../../ProjectDependencies.md). Create a project CMake target, append it to the narrowest consumed `FO_*_LIBS` list supported by the pinned revision, and validate both its compiled feature state and packaged runtime state.

- [BuildTools overview](../../../../BuildTools/README.md).
- [BuildTools Pipeline](../../reference/cmake-and-buildtools/pipeline.md) — staged CMake pipeline and change routing.
- `../BuildTools/cmake/` — reusable CMake modules and staged generation/build/package logic.
- `../BuildTools/Init.cmake` — project-facing CMake entry point and strict stage dispatcher.
- Embedding project root — product-level presets, configuration, and target selection.

## Validation by change type

When `BuildTools/buildtools.py::create_parser()` changes, regenerate and check the CLI model/pages before validating the affected command in an embedding project.

When package declarations or payload behavior change, update `BuildTools/PackageInterface.json`, regenerate/check its model/pages, run `validate_package_interface.cmake` and `test_packaging_matrix.py`, then build `RunPackagingChecks`, `RunTutorialPackageChecks`, or the narrower affected product package target from the owning example/project. These example targets are opt-in and are not part of the required Engine validation registry. The Engine fixtures prove native raw/archive/config/updater mechanics; they do not replace a game's signing, install, deployment, or rollback lane.

- **Runtime C++:** build and run the project unit-test target; use [Testing](../../contributing/testing/) to choose focused suites and understand generated test targets.
- **CMake/BuildTools:** reconfigure from a clean or relevant build directory and run the affected build/package target; use [BuildTools Pipeline](../../reference/cmake-and-buildtools/pipeline.md) for stage ownership.
- **Generated API:** rebuild generation targets, verify scripts compile, and consult [Generated API and Metadata](../../reference/metadata/index.md).
- **Project config/resource packs:** follow [Configure a Game Project](project-configuration.md), compile scripts, bake normally, force-bake when the input graph changed, and execute the consuming sub-config.
- **Generated outputs:** follow [Generated Content Workflow](generated-content.md) in dependency order instead of editing build-tree, baked, or documentation artifacts.
- **Engine pin:** follow the [Engine Upgrade Guide](../migration/engine-upgrade.md), including complete-range audit, generated contract comparison, persistence/network/updater review, and documentation reconciliation.
- **Resource baking:** run the relevant normal/forced bake path and consult [Baking Pipeline](../../explanation/content-pipeline/baking.md).
- **Updater:** follow [Client Updater](../../explanation/runtime/client-updater.md).
- **Web:** follow [Web Build, Packaging, and Browser Debugging](../platforms/web-debugging.md).
- **Android:** follow [Android Build, Packaging, and Device Debugging](../platforms/android-debugging.md).
- **Mapper/tooling:** follow [Tools](../../../Tools.md) and [Mapper Tools](../tools/mapper.md).
- **AngelScript source/refactor:** follow [AngelScript Style and Refactoring](../scripting/style-and-refactoring.md), run the Engine or project formatter wrapper, compile every affected side warning-free, and execute the narrowest behavior or contract test.
- **Nullability/script boundary:** follow [Scripting](../../explanation/scripting-runtime/), [Script Methods Map](../../reference/script-api/method-ownership.md), and [Nullability](../../contributing/coding-contracts/nullability.md).
- **Configuration/resources:** follow [Configuration and Data Sources](../../reference/settings/configuration-and-data-sources.md) and [Baking Pipeline](../../explanation/content-pipeline/baking.md).
- **Essentials/low-level utilities:** follow [Essentials](../../reference/native/essentials.md) and run the matching essentials tests from [Testing](../../contributing/testing/).

## Keep build docs maintainable

Do not copy a full preset list into engine docs. Presets change per game and per branch. Instead, explain ownership and link to the concrete project document that owns exact commands.

## Validation checklist

1. Confirm the command or preset belongs to the embedding project before documenting exact names in engine docs.
2. For BuildTools changes, reconfigure the smallest affected preset and run the generated target that exercises the changed stage.
3. For runtime changes, run focused tests first and then the project `RunUnitTests` target when practical.
4. For package/platform changes, validate the owning package/debug doc in the same change.
5. Update [BuildTools Pipeline](../../reference/cmake-and-buildtools/pipeline.md), [Testing](../../contributing/testing/), or platform docs when the build workflow itself changes.
6. Run the matching starter smoke when a BuildTools, baking, scripting, application-startup, or embedding-boundary change can affect the canonical minimal project.
7. Regenerate affected contract models and run the aggregate [generated contract diff](../../contributing/contract-change-management.md) for project-facing API, CMake, CLI, or package changes.
