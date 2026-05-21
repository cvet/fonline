# Build Workflow

This document explains how to approach FOnline builds without hard-coding assumptions from one project into another.

## Source paths inspected

- `../CMakeLists.txt`
- `../BuildTools/README.md`
- `../BuildTools/Init.cmake`
- `../BuildTools/validate.sh`
- `../BuildTools/validate.cmd`
- `../BuildTools/buildtools.py`
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

## Prerequisites

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

## Where build logic lives

- [../BuildTools/README.md](../BuildTools/README.md) — BuildTools overview.
- [BuildToolsPipeline.md](BuildToolsPipeline.md) — staged CMake pipeline and change routing.
- `../BuildTools/cmake/` — reusable CMake modules and staged generation/build/package logic.
- `../CMakeLists.txt` — engine-level CMake entry points.
- Embedding project root — product-level presets, configuration, and target selection.

## Validation by change type

- **Runtime C++:** build and run the project unit-test target; use [Testing.md](Testing.md) to choose focused suites and understand generated test targets.
- **CMake/BuildTools:** reconfigure from a clean or relevant build directory and run the affected build/package target; use [BuildToolsPipeline.md](BuildToolsPipeline.md) for stage ownership.
- **Generated API:** rebuild generation targets, verify scripts compile, and consult [GeneratedApiAndMetadata.md](GeneratedApiAndMetadata.md).
- **Resource baking:** run the relevant normal/forced bake path and consult [BakingPipeline.md](BakingPipeline.md).
- **Updater:** follow [ClientUpdater.md](ClientUpdater.md).
- **Web:** follow [WebDebugging.md](WebDebugging.md).
- **Android:** follow [AndroidDebugging.md](AndroidDebugging.md).
- **Mapper/tooling:** follow [Tools.md](Tools.md) and [MapperTools.md](MapperTools.md).
- **Nullability/script boundary:** follow [Scripting.md](Scripting.md), [ScriptMethodsMap.md](ScriptMethodsMap.md), and [Nullability.md](Nullability.md).
- **Configuration/resources:** follow [ConfigurationAndDataSources.md](ConfigurationAndDataSources.md) and [BakingPipeline.md](BakingPipeline.md).
- **Essentials/low-level utilities:** follow [Essentials.md](Essentials.md) and run the matching essentials tests from [Testing.md](Testing.md).

## Keep build docs maintainable

Do not copy a full preset list into engine docs. Presets change per game and per branch. Instead, explain ownership and link to the concrete project document that owns exact commands.

## Validation checklist

1. Confirm the command or preset belongs to the embedding project before documenting exact names in engine docs.
2. For BuildTools changes, reconfigure the smallest affected preset and run the generated target that exercises the changed stage.
3. For runtime changes, run focused tests first and then the project `RunUnitTests` target when practical.
4. For package/platform changes, validate the owning package/debug doc in the same change.
5. Update [BuildToolsPipeline.md](BuildToolsPipeline.md), [Testing.md](Testing.md), or platform docs when the build workflow itself changes.
