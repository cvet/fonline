# FOnline Engine

[![License](https://img.shields.io/github/license/cvet/fonline.svg)](https://github.com/cvet/fonline/blob/master/LICENSE)
[![GitHub](https://github.com/cvet/fonline/workflows/validate/badge.svg)](https://github.com/cvet/fonline/actions)
[![Codecov](https://codecov.io/gh/cvet/fonline/branch/master/graph/badge.svg)](https://codecov.io/gh/cvet/fonline)
[![Commit](https://img.shields.io/github/last-commit/cvet/fonline.svg)](https://github.com/cvet/fonline/commits/master)
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/cvet/fonline)

FOnline is a reusable C++20 engine for isometric Fallout / Arcanum-style multiplayer RPGs. It provides the runtime, rendering, networking, resource pipeline, scripting integration, tools, and cross-platform build system; an embedding game project provides the actual world, rules, scripts, assets, configuration, native extensions, and release policy.

## Start here

- **New engine user:** read [Docs/GettingStarted.md](Docs/GettingStarted.md).
- **Embedding the engine into a game:** read [Docs/EmbeddingProject.md](Docs/EmbeddingProject.md).
- **Building, presets, and platform packages:** read [Docs/BuildWorkflow.md](Docs/BuildWorkflow.md).
- **Engine documentation map:** read [Docs/README.md](Docs/README.md).
- **AI-maintainer instructions:** read [AGENTS.md](AGENTS.md) before changing engine code or docs.

## What the engine provides

- Isometric renderer for Fallout 1/2/Tactics and Arcanum-like games.
- Hexagonal and square map tiling.
- Sprite-based environments with support for 3D character models.
- Native C++20 engine core.
- AngelScript-based game scripting integration.
- Client/server runtime, tools, mapper support, updater support, tests, and build tooling.
- Cross-platform build targets: Windows, Linux, macOS, Android, WebAssembly, and related package layouts.
- Asset pipelines for classic Fallout-family formats, Arcanum-style data, FBX-based models, and common image formats such as PNG/TGA.

Not every historical or experimental feature is complete on every platform. Prefer the maintained docs and the embedding project's real presets over assumptions from older README sections.

## Repository layout

- [`Source/`](Source/) — engine source code: applications, runtime, scripting, resources, tools, essentials, and tests.
- [`BuildTools/`](BuildTools/) — CMake modules, code generation, package preparation, workspace generation, and platform helpers.
- [`Resources/`](Resources/) — engine-owned runtime and build resources.
- [`ThirdParty/`](ThirdParty/) — vendored third-party dependencies; update workflow is in [Docs/ThirdPartyMaintenance.md](Docs/ThirdPartyMaintenance.md).
- [`Docs/`](Docs/) — maintained user and maintainer documentation.
- [`PUBLIC_API.md`](PUBLIC_API.md) — public native API notes.
- [`TUTORIAL.md`](TUTORIAL.md) — tutorial material.

## How FOnline is normally used

A game repository embeds this repository as an `Engine/` submodule and keeps game-specific material outside the engine:

1. Add the engine repository as `Engine/`.
2. Add project-level `CMakeLists.txt`, `CMakePresets.json`, and a `.fomain` configuration.
3. Add game content folders such as `Critters/`, `Items/`, `Maps/`, `Dialogs/`, `Texts/`, and GUI/resources.
4. Add game logic scripts, usually under `Scripts/`.
5. Add optional project-native extensions under a project-owned source folder.
6. Build project targets through CMake presets and engine `BuildTools`.

See [Docs/EmbeddingProject.md](Docs/EmbeddingProject.md) for the expected project shape and responsibilities.

## Documentation index

- [Docs/GettingStarted.md](Docs/GettingStarted.md) — first tour for users who want to understand what to open and what to build.
- [Docs/EmbeddingProject.md](Docs/EmbeddingProject.md) — how a game project should own content, scripts, config, extensions, and release policy around the engine.
- [Docs/BuildWorkflow.md](Docs/BuildWorkflow.md) — build prerequisites, presets, validation, and platform workflow notes.
- [Docs/ThirdPartyMaintenance.md](Docs/ThirdPartyMaintenance.md) — engine vendored dependency update, pruning, and local patch workflow.
- [Docs/BuildToolsPipeline.md](Docs/BuildToolsPipeline.md) — staged CMake pipeline and BuildTools change routing.
- [Docs/BakingPipeline.md](Docs/BakingPipeline.md) — resource baking pipeline and baker validation.
- [Docs/GeneratedApiAndMetadata.md](Docs/GeneratedApiAndMetadata.md) — codegen and metadata registration flow.
- [Docs/ConfigurationAndDataSources.md](Docs/ConfigurationAndDataSources.md) — config parsing, settings, resource-pack data sources, file lookup, and caches.
- [Docs/EntityModel.md](Docs/EntityModel.md) — entity/property/prototype runtime model.
- [Docs/MapsMovementGeometry.md](Docs/MapsMovementGeometry.md) — map coordinates, path finding, line tracing, movement contexts, and map loading.
- [Docs/Networking.md](Docs/Networking.md) — network buffers, commands, transports, and ordered UDP.
- [Docs/Persistence.md](Docs/Persistence.md) — server database facade, backends, commit queue, and recovery logs.
- [Docs/ClientRuntime.md](Docs/ClientRuntime.md) — client lifecycle, connection, view entities, resources, sprites, and client tests.
- [Docs/FrontendAndRendering.md](Docs/FrontendAndRendering.md) — application/window/input/audio layer and renderer backends.
- [Docs/ServerRuntime.md](Docs/ServerRuntime.md) — authoritative server lifecycle, managers, networking, persistence, and updater backend.
- [Docs/Architecture.md](Docs/Architecture.md) — high-level engine architecture and layer ownership.
- [Docs/SourceTree.md](Docs/SourceTree.md) — source-tree navigation for maintainers.
- [Docs/Applications.md](Docs/Applications.md) — application entry points and target ownership notes.
- [Docs/Essentials.md](Docs/Essentials.md) — low-level platform, logging, memory, filesystem, serialization, sockets, and utility layer.
- [Docs/ClientUpdater.md](Docs/ClientUpdater.md) — client host/runtime split, runtime ABI, update manifests, and server-side update backend.
- [Docs/Debugging.md](Docs/Debugging.md) — native debugging, stack traces, Visual Studio helpers, and validation notes.
- [Docs/Scripting.md](Docs/Scripting.md) — script system lifecycle, AngelScript backend, native method exports, core scripts, and compile flow.
- [Docs/ScriptMethodsMap.md](Docs/ScriptMethodsMap.md) — native script method ownership map by runtime side and receiver family.
- [Docs/Nullability.md](Docs/Nullability.md) — `T?` / `FO_NULLABLE` script/native nullability contract and analyzer workflow.
- [Docs/Tools.md](Docs/Tools.md) — engine tool map: baker, AS compiler, mapper, editor, asset explorer, particle editor, and asset processors.
- [Docs/MapperTools.md](Docs/MapperTools.md) — mapper lifecycle, automation, native mapper helpers, and known headless-render workflows.
- [Docs/WebDebugging.md](Docs/WebDebugging.md) — WebAssembly target preparation and debug workflow.
- [Docs/AndroidDebugging.md](Docs/AndroidDebugging.md) — Android target preparation, package layout, ADB workflow, and remote-scene debugging.
- [Docs/Testing.md](Docs/Testing.md) — test-suite inventory, generated test targets, coverage, and validation routing.
- [Docs/DocumentationMaintenance.md](Docs/DocumentationMaintenance.md) — source-grounded docs maintenance workflow.

## Development and validation

Use a real embedding project to validate engine changes. The engine is designed to be composed with game content and project presets, so the most meaningful checks are usually project targets that exercise the engine in context.

A typical change should be validated with the smallest relevant scope:

- C++ runtime change: build and run the project unit-test target, for example the embedding project's unit-test/run-test target.
- BuildTools change: run the affected CMake configure/generate/package path.
- Client runtime change: check [Docs/ClientRuntime.md](Docs/ClientRuntime.md), [Docs/Networking.md](Docs/Networking.md), and the smallest relevant client tests.
- Server runtime change: check [Docs/ServerRuntime.md](Docs/ServerRuntime.md), [Docs/Persistence.md](Docs/Persistence.md), [Docs/Networking.md](Docs/Networking.md), and the smallest relevant server tests.
- Frontend/rendering change: check [Docs/FrontendAndRendering.md](Docs/FrontendAndRendering.md) and the affected backend.
- Web change: follow [Docs/WebDebugging.md](Docs/WebDebugging.md).
- Android change: follow [Docs/AndroidDebugging.md](Docs/AndroidDebugging.md).
- Mapper/tooling change: check [Docs/Tools.md](Docs/Tools.md), [Docs/MapperTools.md](Docs/MapperTools.md), and the affected app/tool source path.
- Script/native boundary change: check [Docs/Scripting.md](Docs/Scripting.md), [Docs/ScriptMethodsMap.md](Docs/ScriptMethodsMap.md), and [Docs/Nullability.md](Docs/Nullability.md), then run the relevant analyzer/runtime checks.

When behavior changes in a noticeable way, update the owning document in the same change.

## Project and community

- Site: <https://fonline.ru>
- GitHub: <https://github.com/cvet/fonline>
- License: [LICENSE](LICENSE)
