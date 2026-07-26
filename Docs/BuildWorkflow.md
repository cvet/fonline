# Build Workflow

This document explains how to approach FOnline builds without hard-coding assumptions from one project into another.

## Source paths inspected

- `../CMakeLists.txt`
- `../BuildTools/README.md`
- `../BuildTools/Init.cmake`
- `../BuildTools/validate.sh`
- `../BuildTools/validate.cmd`
- `../BuildTools/buildtools.py`
- `../BuildTools/docs_cli.py`
- `generated/cli/index.md`
- `../BuildTools/PackageInterface.json`
- `../BuildTools/docs_package.py`
- `generated/package/index.md`
- `../Examples/MinimalProject/`
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

The repository includes one stable exception to project-specific target naming: [Examples/MinimalProject](../Examples/MinimalProject/README.md). It proves a clean headless embedding path without Last Frontier, TLA, or another game checkout.

From the engine root, use the host-specific validation target:

```powershell
python BuildTools\buildtools.py validate win64-starter-smoke
```

```bash
python3 BuildTools/buildtools.py validate linux-starter-smoke
```

Both routes configure and build the baker plus headless server, bake the minimal AngelScript project, run the server with networking disabled and an in-memory database, and require the lifecycle markers documented in [TUTORIAL.md](../TUTORIAL.md). The Windows route is locally verified; the Linux route is wired into GitHub Actions and remains pending until that job completes successfully.

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

### Windows 7 compatibility lane

The `win32-win7` and `win64-win7` build-platform keys are native-Windows MSVC lanes pinned to toolset `v143,version=14.44`; they fail early on a non-Windows host. `FO_BINARY_OUTPUT_POSTFIX` is an independent build identity, not an implication of the `-win7` platform name. When a project builds with a value such as `Win7`, its matching package declaration must use the same value on that one entry: `BINARY Client Windows win32-win7 Raw+Zip+Wix POSTFIX Win7`.

Before packaging or publishing that lane, inspect every linked EXE and DLL:

```powershell
python BuildTools/check_windows7_imports.py <client.exe> <client-runtime.dll>
```

The check parses PE imports and fails on `CreateFile2`, the currently forbidden Windows 8+ import. The embedding project owns the concrete toolset installation, binary paths, package matrix, and CI gate; [Testing.md](Testing.md) owns the reusable validation rule.

## Where build logic lives

Use the generated [BuildTools CLI reference](generated/cli/index.md) for the exact main commands, arguments, defaults, choices, and executable help output.

Use the generated [package interface reference](generated/package/index.md) for `DefinePackage` grammar, accepted targets/platforms/architectures, pack-token compatibility, payload layouts, and output artifacts. Keep a game's concrete package matrix in that embedding project's documentation.

- [../BuildTools/README.md](../BuildTools/README.md) — BuildTools overview.
- [BuildToolsPipeline.md](BuildToolsPipeline.md) — staged CMake pipeline and change routing.
- `../BuildTools/cmake/` — reusable CMake modules and staged generation/build/package logic.
- `../CMakeLists.txt` — engine-level CMake entry points.
- Embedding project root — product-level presets, configuration, and target selection.

## Validation by change type

When `BuildTools/buildtools.py::create_parser()` changes, regenerate and check the CLI model/pages before validating the affected command in an embedding project.

When package declarations or payload behavior change, update `BuildTools/PackageInterface.json`, regenerate/check its model/pages, run `validate_package_interface.cmake`, and exercise the narrowest affected package target.

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
6. Run the matching starter smoke when a BuildTools, baking, scripting, application-startup, or embedding-boundary change can affect the canonical minimal project.
7. Regenerate affected contract models and run the aggregate [generated contract diff](ApiChangeManagement.md) for project-facing API, CMake, CLI, or package changes.
