# Build Workflow

This document explains how to approach FOnline builds without hard-coding assumptions from one project into another.

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

- **Runtime C++:** build and run the project unit-test target.
- **CMake/BuildTools:** reconfigure from a clean or relevant build directory and run the affected build/package target; use [BuildToolsPipeline.md](BuildToolsPipeline.md) for stage ownership.
- **Generated API:** rebuild generation targets, verify scripts compile, and consult [GeneratedApiAndMetadata.md](GeneratedApiAndMetadata.md).
- **Resource baking:** run the relevant normal/forced bake path and consult [BakingPipeline.md](BakingPipeline.md).
- **Updater:** follow [ClientUpdater.md](ClientUpdater.md).
- **Web:** follow [WebDebugging.md](WebDebugging.md).
- **Android:** follow [AndroidDebugging.md](AndroidDebugging.md).
- **Mapper/tooling:** follow [MapperTools.md](MapperTools.md).
- **Nullability/script boundary:** follow [Nullability.md](Nullability.md).

## Keep build docs maintainable

Do not copy a full preset list into engine docs. Presets change per game and per branch. Instead, explain ownership and link to the concrete project document that owns exact commands.
