---
layout: default
title: Embedding FOnline in a Game Project
locale: en
document_id: embedding-project
permalink: /Docs/en/how-to/build/embedding-project.html
---

# Embedding FOnline in a Game Project

FOnline is designed to be embedded as a source submodule. The engine repository supplies reusable technology; the game repository supplies the concrete product. The [Project Lifecycle](../project/index.md) section covers creating and maintaining such a repository step by step.

## Source paths inspected

- `BuildTools/Init.cmake`
- `BuildTools/cmake/ProjectInterface.json`
- `BuildTools/cmake/helpers/Build.cmake`
- `BuildTools/cmake/stages/Applications.cmake`
- `BuildTools/cmake/stages/ScriptsAndBaking.cmake`
- `BuildTools/tests/test_cmake_baker_runtime_copy.py`
- `Examples/MinimalProject/CMakeLists.txt`
- `Examples/MinimalProject/FOnlineStarter.fomain`
- `Examples/MinimalProject/README.md`
- `Examples/MinimalMultiplayer/CMakeLists.txt`
- `Examples/MinimalMultiplayer/FOnlineMinimalMultiplayer.fomain`
- `Examples/PublicRepositories.json`
- `Source/Applications`
- `Source/Frontend/ApplicationInit.cpp`
- `Source/Tools`

The engine-owned [minimal project](../../../../Examples/MinimalProject/README.md) is the canonical executable example of this boundary. It is also the source for the [first headless project tutorial](../../tutorials/first-project.md) and the planned `fonline-project-template`. Ownership, exact-revision rules, CI lanes, and publication gates for that repository and later examples are defined in [Public Example Repositories](public-example-repositories.md).

## Expected repository shape

A typical game repository looks like this:

```text
GameProject/
├── Engine/                 # git submodule pointing to this repository
├── CMakeLists.txt          # project entry point that includes engine build logic
├── CMakePresets.json       # project presets and platform variants
├── GameName.fomain         # master project configuration
├── Scripts/                # game AngelScript (.fos) or Managed C# (.cs) modules
├── SourceExt/              # optional project-native C++ extensions
├── Critters/ Items/ Maps/  # game content and prototypes
├── ProjectDialogs/ Texts/  # optional project-defined dialogs and localization
└── Docs/                   # game-specific documentation
```

Folder names vary by project, but the ownership rule should stay stable: reusable engine machinery lives under `Engine/`, while concrete game content and project policy live in the parent repository.

The folder name above is deliberately generic. FOnline does not currently ship
a built-in dialog-tree schema, `.fodlg` parser, dialog baker, runtime, or visual
editor. A game may implement dialogs in scripts, through project-native
extensions and bakers, or through a separately versioned companion. Document
the chosen format and validation in the game repository, and do not assume that
another embedding project has the same dialog API or file layout.

## What belongs in the engine

Keep these in the engine repository:

- Runtime systems shared by multiple games.
- BuildTools and CMake stages used to compose projects.
- Platform package/workspace generation logic.
- Engine resources and reusable tools.
- Public/native API definitions and generated scripting API machinery.
- Documentation about engine behavior, platform mechanics, and reusable contracts.

## What belongs in the game project

Keep these in the embedding project:

- Game rules, content, maps, prototypes, dialogs, localization, and GUI definitions.
- Game-specific AngelScript or Managed C# modules, with exactly the baker/runtime backend(s) enabled and packaged by the project.
- Project-level native extension implementations and dependencies; [Native Extensions](../../../NativeExtensions.md) owns composition, hooks, and bindings, while [Project Dependencies](../../../ProjectDependencies.md) owns library/SDK selection, role-scoped linking, package delivery, and updates.
- Project-level AI observations, game actions, MCP tools, and listener shipping policy; [AiControl Protocol](../ai-control-protocol.md) owns only the reusable transport, command lifecycle, threat boundary, reference client, and protocol evidence.
- Project presets, product identifiers, package names, signing/deployment choices, and CI policy.
- Game design and content workflow documentation.

## Project-Owned Game-System Formats

A project may define authored formats for game systems that are not part of the
Engine contract. Dialog trees are a common example. Keep such a system
project-owned until its reusable implementation, tests, fixtures, compatibility
policy, and documentation have moved into Engine or a versioned companion.

A complete project-owned format should identify:

1. the parser and authoritative grammar;
2. the baker or other generated outputs;
3. runtime consumers and authority boundaries;
4. editor/formatter behavior and round-trip expectations;
5. source-level, compiled, and runtime validation;
6. compatibility and migration policy across Engine pins;
7. the exact ownership label in project documentation.

Engine documentation may describe the native-extension and baking primitives
used to implement the format. It must not publish the project format as a stock
FOnline capability.

## Build composition

The game repository should drive the build. In practice this means:

1. Configure from the game repository root, not from inside `Engine/`, unless a specific engine-only workflow says otherwise.
2. Use the game repository's `CMakePresets.json` and tasks so generated paths, target names, and package metadata match the product.
3. Let engine `BuildTools` provide reusable stages and helper functions.
4. Keep generated files out of hand-authored docs unless the generation process is part of the topic.

Use `Examples/MinimalProject/CMakeLists.txt` as the smallest current composition example. It also proves a server-only `INTERFACE` dependency through the revision-pinned `FO_SERVER_LIBS` list; expand it by adding project-owned modules and targets without copying unrelated wiring from a large game.

### Add a project-specific baking target

The standard pipeline creates `BakeResources` and `ForceBakeResources` with subconfig `NONE`. If the game defines another configuration slice for a public, test, or release resource set, create its target immediately after the scripts-and-baking stage:

```cmake
SetupScriptsAndBaking()

AddBakingTarget(Game_PublicResources
    SUB_CONFIG PublicGame
    COMMENT "Bake public resources")

BuildPackages()
```

Add `FORCE` only when that target must always request a full bake. The helper retains the standard `ForceCodeGeneration` dependency, `FO_OUTPUT_PATH` working directory, main-config argument, and resource build-hash update. Keep the target name, subconfig contents, and downstream CI/package policy in the game repository.

### Baker library next to prebaking applications

An unpackaged client or server bakes changed sources before it starts. `Source/Frontend/ApplicationInit.cpp` looks for `FO_BakeResources` in the running process and otherwise loads `<FO_DEV_NAME>_BakerLib` from the directory of the executable. When the library is missing, the application shows `Warning! <FO_DEV_NAME>_BakerLib not found. Resources may be out of date` and starts on the existing bake output, or stops with `Baker not found. Unable to bake resources` when there is no bake output. `Baking.PrebakeOnStartup = False` skips the step deliberately.

`BuildApplications()` owns the copy. When the baker library target exists (`FO_BUILD_BAKER` is ON and the platform is not Web), the Applications stage makes each of these executables depend on it and copies it next to the executable after every build of that executable:

| Executable | Built when |
|---|---|
| `<FO_DEV_NAME>_Client` | `FO_BUILD_CLIENT`, except library client builds such as Android |
| `<FO_DEV_NAME>_ClientHeadless` | `FO_BUILD_CLIENT` on Windows, Linux and macOS |
| `<FO_DEV_NAME>_Server`, `<FO_DEV_NAME>_ServerHeadless` | `FO_BUILD_SERVER` |
| `<FO_DEV_NAME>_ServerService` | `FO_BUILD_SERVER` on Windows |
| `<FO_DEV_NAME>_ServerDaemon` | `FO_BUILD_SERVER` on other platforms |

These are the executables whose entry point passes `AppInitFlags::PrebakeResources` to `InitApp`. The client runtime module `<FO_DEV_NAME>_ClientLib` prebakes inside its host executable and finds the library beside that host. Mapper, the viewers, ASCompiler, Baker and ManagedScriptBaker link the baker code statically and never load the library, so they get no copy. A library client build gets none either, because its code runs inside a host process that the project does not build. `BuildTools/tests/test_cmake_baker_runtime_copy.py` derives the set from the entry points and fails when the stage drifts from them.

Do not copy the baker library from the project. `CopyTargetRuntimeToTarget` records every consumer and producer pair, and a second request for a pair aborts configure. For the baker library the message names the stage:

```text
CMake Error at Engine/BuildTools/cmake/helpers/Build.cmake:<line> (_message):
  CopyTargetRuntimeToTarget: the Applications stage already copies
  MYGAME_BakerLib next to MYGAME_Server; remove the project-side copy
Call Stack (most recent call first):
  Engine/BuildTools/cmake/helpers/Build.cmake:<line> (AbortMessage)
  CMakeLists.txt:<line> (CopyTargetRuntimeToTarget)
```

The failure is deliberate. A second identical copy would build correctly, but it would keep a second list of applications in the project, and that list drifts from the Engine one with every new application. The call stack names the project line to delete.

### Migrate a project that copies the baker itself

Projects written against an Engine revision without this stage step usually carry a `Finalize Pre` hook with `CopyTargetRuntimeToTarget(<app> ${FO_DEV_NAME}_BakerLib)` lines. Find them from the game repository root; the command is the same in PowerShell and bash:

```bash
git grep -n -E 'CopyTargetRuntimeToTarget\([^)]*_BakerLib' -- '*CMakeLists.txt' '*.cmake'
```

`git grep` does not search the Engine submodule, so the stage's own call is not listed.

1. Delete every listed call, including calls for Mapper, ASCompiler or other tools. Those applications never load the library, so their copy only placed an unused file.
2. If the hook function becomes empty, delete the function and its `AddStageHook(Finalize Pre <macro-name>)` line. Keep the hook when it still does other work, such as third-party runtime copies or link options.
3. Reconfigure and rebuild the client and server targets. Configure must finish without the `already copies` error, and each executable directory from the table above must contain `<FO_DEV_NAME>_BakerLib` with the platform's shared-library suffix, for example `.dll` on Windows and `.so` on Linux.

### Copy project runtime files next to applications

Project-owned runtime files, such as a vendor SDK library or a project plugin target, are copied from a `Finalize Pre` hook. Every application target exists at that point. Use the documented helpers from [CMake Project Helpers](../../reference/cmake/helpers.md): `CopyRuntimeToTarget` for a file and `CopyTargetRuntimeToTarget` for a CMake target. In this example `MYGAME_VENDOR_RUNTIME` holds the absolute path of a vendor runtime file for the configured platform, and `MyGamePlugin` is a shared-library target that the project defines:

```cmake
function(_mygame_copy_runtimes)
    CopyRuntimeToTarget(${FO_DEV_NAME}_Client "${MYGAME_VENDOR_RUNTIME}")
    CopyTargetRuntimeToTarget(${FO_DEV_NAME}_Server MyGamePlugin)
endfunction()

AddStageHook(Finalize Pre _mygame_copy_runtimes)
```

- Register the hook before `FinalizeProjectGeneration()`; `AddStageHook` aborts configure for a stage that has already run.
- Both helpers skip a missing target silently, so one hook serves presets with different application roles.
- `CopyRuntimeToTarget` also skips an empty path and a file that does not exist at configure time. When a copy is missing, check the path in the configure environment first.
- `CopyTargetRuntimeToTarget` builds the producer before the consumer, and a repeated pair aborts configure as described above.

`BuildTools/tests/test_cmake_baker_runtime_copy.py` configures this hook in a scratch project and checks both copies.

## Documentation composition

Use this routing:

- Link from game docs into `Engine/Docs/...` for reusable mechanics such as Web/Android debugging, nullability, updater protocol, mapper automation, and native debugging.
- Keep local links in engine docs inside the engine repository. Cross-project examples must use stable HTTPS links to tagged public revisions.
- Use only examples whose generated registry status is `published`; a planned repository name is not a valid public source link.
- Avoid duplicating long engine explanations in the game docs. Prefer a short project-specific note plus a link to the owning engine document.

## Validation principle

Validate engine changes through a real embedding project whenever possible. The engine-owned minimal project provides the baseline host-aware `Examples/MinimalProject/validate.py` route; larger projects remain necessary for client, content, packaging, and gameplay contracts. The example validator is opt-in and is not a required Engine workflow lane. A reusable engine change may compile in isolation but still break generated APIs, project packaging, scripts, or content baking. Choose the narrowest project target that exercises the changed layer.
