# FOnline Engine

FOnline is a reusable C++20 engine for isometric Fallout/Arcanum-style games. It is normally embedded as a git submodule inside a game project; the game project provides content, scripts, configuration, native extensions, presets, and packaging policy.

## Start Here

- [Docs/README.md](Docs/README.md) - maintained engine documentation map.
- [AGENTS.md](AGENTS.md) - AI-maintainer entry point and repository conventions.
- [PUBLIC_API.md](PUBLIC_API.md) - public API notes.
- [TUTORIAL.md](TUTORIAL.md) - tutorial for using the engine.
- [Source/README.md](Source/README.md) - source-tree overview.
- [Source/Tests/README.md](Source/Tests/README.md) - engine unit-test suites.
- [BuildTools/README.md](BuildTools/README.md) - build tooling notes.

## Repository Layout

- `Source/` - engine source code: applications, client/server runtime, scripting, resource pipeline, tools, essentials, and tests.
- `BuildTools/` - CMake/tooling scripts for building, code generation, packaging, and platform workspace preparation.
- `Resources/` - engine-owned runtime/build resources.
- `ThirdParty/` - vendored third-party dependencies.
- `Docs/` - maintained engine docs that were split out of game-project documentation.

## Using the Engine From a Game Project

A game project typically:

1. Adds this repository as `Engine/` submodule.
2. Provides a root `CMakeLists.txt`, `CMakePresets.json`, and `.fomain` configuration.
3. Provides game scripts (`Scripts/`), content (`Critters/`, `Items/`, `Maps/`, `Dialogs/`, `Texts/`, etc.), and optional native extensions (`SourceExt/`).
4. Builds final targets through the engine `BuildTools` and project-specific target definitions.

Last Frontier is one such embedding project. Engine docs may use `../../...` paths when they intentionally point from `Engine/Docs/` back into the embedding project.

## Key Maintained Docs

- [Docs/ClientUpdater.md](Docs/ClientUpdater.md) - client host/runtime split, runtime ABI, updater protocol, and server-side update backend.
- [Docs/Debugging.md](Docs/Debugging.md) - native debugging, stack traces, Visual Studio helpers, and validation notes.
- [Docs/Nullability.md](Docs/Nullability.md) - script/native nullability contract and analyzer workflow.
- [Docs/MapperTools.md](Docs/MapperTools.md) - mapper automation and native mapper helper integration points.
- [Docs/WebDebugging.md](Docs/WebDebugging.md) - web target build/debug workflow.
- [Docs/AndroidDebugging.md](Docs/AndroidDebugging.md) - Android target build/debug workflow.

## Validation

Use the embedding project to exercise real targets. In Last Frontier, the common engine baseline is the `LF_UnitTests` / `RunUnitTests` target, with platform-specific package/build validation for BuildTools and runtime changes.

## About

- Site: <https://fonline.ru>
- GitHub: <https://github.com/cvet/fonline>
