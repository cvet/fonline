---
layout: default
title: FOnline Engine
locale: en
document_id: repository-home
permalink: /
---

# FOnline Engine

[![License](https://img.shields.io/github/license/cvet/fonline.svg)](https://github.com/cvet/fonline/blob/master/LICENSE)
[![GitHub](https://github.com/cvet/fonline/workflows/validate/badge.svg)](https://github.com/cvet/fonline/actions)
[![Commit](https://img.shields.io/github/last-commit/cvet/fonline.svg)](https://github.com/cvet/fonline/commits/master)
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/cvet/fonline)

**FOnline** is an open-source (MIT) C++20 engine for building online multiplayer RPGs in the classic isometric style of Fallout 1/2/Tactics and Arcanum. One codebase gives you the authoritative server, the game client, the map editor, the content pipeline, and packaging for desktop, mobile, and the browser — you bring the game: content, scripts, and rules live in your own repository that embeds the engine.

In continuous development since 2006, the engine powers community multiplayer RPGs; a current example is [Last Frontier](https://lastfrontier.ru/), a post-apocalyptic MMO built on it.

## Why FOnline?

- **Multiplayer first.** Not a single-player engine with networking bolted on: an authoritative server, replicated entity state, and client/server separation are the core design, all the way down to the entity model.
- **Complete vertical.** Server, client, mapper, editor, resource baker, script compiler, test runner, auto-updater — all built from the same sources by one CMake pipeline.
- **Engine/game split that stays clean.** The engine is a reusable submodule; your game owns content, scripts, configuration, branding, and release policy. Engine updates don't drag game policy with them.
- **Data-driven content.** Prototypes, maps, dialogs, localization, and GUI are authored as plain-text assets and baked into runtime packs — friendly to diffs, reviews, and tooling.
- **Runs where players are.** Native Windows/Linux/macOS, Android and iOS, and a WebAssembly client that plays in the browser over WebSockets.

## Feature highlights

### Multiplayer core

- Authoritative server runtime with entity managers, client validation, and hardened parsing of untrusted client input.
- Shared entity/property/prototype model with generated type-safe property wrappers and automatic property replication to clients.
- Pluggable network transports: TCP sockets (including an Asio-based server), WebSockets for browser play, an ordered-UDP channel, and an in-process transport for tests and embedded clients.
- Pluggable persistence backends — JSON files, SQLite, MongoDB, or in-memory — behind one database facade with an async commit queue and recovery logs.
- Built-in client auto-updater: a thin client host plus a replaceable runtime, resumable file transfer, and a server-side update backend.

### Scripting

- AngelScript gameplay scripting over a backend-neutral script system.
- The native API is exported to scripts by code generation from `///@` annotations — methods, properties, events, remote calls, and enums stay in sync with the C++ source automatically.
- Nullability is enforced across the script/native boundary: script `T?` maps to native `ptr<T>`/`nptr<T>` contracts, checked by analyzers and runtime asserts.
- Script debugging support alongside native debugging.

### Rendering and presentation

- Renderer backends: OpenGL, Direct3D, Vulkan, and SDL_GPU, plus headless/null modes for servers and CI.
- Effects are written once in GLSL and compiled through glslang to SPIR-V, then translated to each backend via SPIRV-Cross.
- Sprite-based isometric worlds with 3D character models (FBX), particle effects, video playback, and audio in both modern (Ogg/Vorbis) and classic Fallout formats.
- Windowed, borderless-fullscreen, and multi-client virtual-window modes with a consistent resolution/letterbox model; ImGui-powered developer overlay.

### World and maps

- Hexagonal and square grid geometry modes with shared helpers for distance, direction, and neighborhoods.
- Path finding, line tracing, movement contexts, and a blocking model designed for multiplayer server authority.

### Content pipeline and tools

- A baking pipeline turns authored sources — prototypes, maps, dialogs, localized texts, effects, images, models, scripts — into versioned runtime resource packs.
- Imports classic 2D asset formats (Fallout FRM, Arcanum ART, and other legacy formats) alongside PNG/TGA.
- Interactive tools built on the engine itself: Mapper with map/content windows and SPARK editing, plus focused animation and baked-particle viewers.

### Engineering quality

- Unit tests (Catch2) with generated per-suite targets, sanitizer runs, and code coverage.
- Clang Thread Safety Analysis enforced as `-Werror` on every Clang toolchain; strict smart-pointer and nullability vocabularies audited across the codebase.
- Always-on stack traces, deterministic exception-safety rules, and a terminate-on-OOM allocation model instead of half-mutated states.
- Tracy profiler integration for client and server captures.

## Architecture at a glance

```text
Your game repository                      FOnline engine (this repo, embedded as Engine/)
────────────────────                      ────────────────────────────────────────────────
content: protos, maps,            ┌──►    Applications — client/server/tool entry points
dialogs, texts, GUI               │       Client & Server runtimes — views vs. authority
AngelScript game logic     embeds │       Common model — entities, properties, protos,
.fomain configuration      ───────┤                      maps, networking, config
native extensions                 │       Frontend — windows, input, audio, renderers
CMake presets, CI,                │       Scripting — AngelScript bridge + generated API
release policy                    └──►    Tools & BuildTools — bakers, mapper, editor,
                                                     CMake stages, codegen, packaging
```

The engine owns reusable technology; the game owns the product. A game repository adds the engine as an `Engine/` submodule, points the engine's staged CMake pipeline at its own configuration, and gets project-named build targets for every application. The full layer map is in [Engine Architecture](Docs/en/explanation/architecture/), and the embedding contract in [Embedding FOnline in a Game Project](Docs/en/how-to/build/embedding-project.md):

```text
GameProject/
├── Engine/                 # this repository as a git submodule
├── CMakeLists.txt          # project entry point that includes engine build logic
├── CMakePresets.json       # project presets and platform variants
├── GameName.fomain         # master project configuration
├── Scripts/                # game AngelScript modules
├── SourceExt/              # optional project-native C++ extensions
├── Critters/ Items/ Maps/  # game content and prototypes
└── Dialogs/ Texts/         # dialogs and localization
```

## Getting started

- **Run the first engine-owned project:** [First FOnline Headless Project](Docs/en/tutorials/first-project.md) - configure, build, bake, start, verify, and stop the minimal headless server.
- **Inspect the canonical scaffold:** [Examples/MinimalProject/README.md](Examples/MinimalProject/README.md) - the complete project and CI smoke contract.
- **Inspect the executable content gallery:** [Examples/ContentShowcase/README.md](Examples/ContentShowcase/README.md) - source assets, baking, native/Web checks, provenance, budgets, and reproducible capture evidence.
- **Build the first playable slice:** [first playable client](Docs/en/tutorials/first-client.md), [first content change](Docs/en/tutorials/first-content.md), and [first automated test](Docs/en/tutorials/first-test.md) - connect a client, change localized content, and extend executable checks.
- **Configure and maintain a project:** [Project Configuration](Docs/en/how-to/build/project-configuration.md), [Generated Content Workflow](Docs/en/how-to/build/generated-content.md), and [Engine Upgrade Guide](Docs/en/how-to/migration/engine-upgrade.md) - own `.fomain`, resource packs, generated outputs, migrations, and Engine updates.
- **Choose release targets truthfully:** [Support Matrix](Docs/en/reference/platforms/support-matrix.md) and its [generated matrix](Docs/en/reference/platforms/generated-matrix.md) - distinguish source capability, required builds, process smoke tests, and project/device qualification.
- **Plan and validate public examples:** [Public Example Repositories](Docs/en/how-to/build/public-example-repositories.md) and its [generated registry](Docs/en/reference/public-examples/index.md) - ownership, exact Engine pins, compatibility lanes, repository template, support, and asset provenance.
- **Browse the generated native API:** [Docs/en/reference/script-api/index.md](Docs/en/reference/script-api/index.md) - methods, properties, events, types, settings, migrations, and source links.
- **Author prototypes:** [Prototype Format](Docs/en/how-to/content/prototype-format.md) and its [generated reference](Docs/en/reference/prototype-format/index.md) - exact syntax, inheritance, built-in properties, references, migrations, and validation.
- **Author maps:** [Docs/en/how-to/content/map-format.md](Docs/en/how-to/content/map-format.md) and its [generated reference](Docs/en/reference/map-format/index.md) - `.fomap` sections, placement IDs, ownership, mapper normalization, side-specific baking, and runtime loading.
- **Author localized text:** [Text and Localization](Docs/en/how-to/content/text-and-localization.md) and its [generated reference](Docs/en/reference/text-format/index.md) - `.fotxt` syntax, language normalization, prototype `$Text`, runtime lookup, color tags, and the game-formatting boundary.
- **Author images and sprite sheets:** [Image and sprite formats](Docs/en/how-to/content/image-format.md) and the [generated reference](Docs/en/reference/image-format/index.md) - PNG/TGA and legacy import, FOFRM composition, baking, runtime factories, atlases, caches, and validation.
- **Author shader effects:** [Effect Format](Docs/en/how-to/content/effect-format.md) and its [generated reference](Docs/en/reference/effect-format/index.md) - `.fofx` sections, passes, render state, shader resources, backend outputs, runtime selection, and script values.
- **Author particles:** [Particle Format And Runtime](Docs/en/how-to/content/particle-format.md) and its [generated reference](Docs/en/reference/particle-format/index.md) - optional SPARK/Effekseer selection, `.spark`/`.efkproj` authoring, `.spk`/`.efk` baking, Mapper tools, runtime routes, and model/script integration.
- **Author bitmap fonts and lay out text:** [font formats and text layout](Docs/en/how-to/content/font-format.md) and its [generated reference](Docs/en/reference/font-format/index.md) - FOFNT/BMFont descriptors, slot binding, scaling, measurement, wrapping, rendering flags, colors, and validation.
- **Inspect project integration contracts:** [CMake reference](Docs/en/reference/cmake/index.md), [BuildTools CLI reference](Docs/en/reference/buildtools/index.md), [helper CLI reference](Docs/en/reference/helper-cli/index.md), [native-extension reference](Docs/en/reference/native-extension/index.md), [package reference](Docs/en/reference/packages/index.md), and [public-example registry](Docs/en/reference/public-examples/index.md) - exact CMake, main/helper BuildTools, native-extension, packaging, and example-program surfaces.
- **Add project-native C++ safely:** [Native Extensions](Docs/en/how-to/native-extensions.md) - source roles, hooks, script exports, state ownership, compatibility, and executable validation.
- **Browse the generated CMake interface:** [Docs/en/reference/cmake/index.md](Docs/en/reference/cmake/index.md) - project options, strict stages and hooks, selected helpers, defaults, and source links.
- **Measure client and server performance:** [Profiling](Docs/en/how-to/quality/profiling.md) - Tracy build modes, isolated capture boundaries, reproducible workloads, and comparable result analysis.
- **Publishing documentation:** [documentation site publication](Docs/en/contributing/documentation/site-publication.md) - generated navigation/search/route data, rolling version and locale policy, local Jekyll preview, rendered-route/accessibility validation, CI artifacts, and the existing `fonline.ru` GitHub Pages route.
- **AI and offline documentation:** [llms.txt](llms.txt), [llms-full.txt](llms-full.txt), and [docs-manifest.json](docs-manifest.json) - generated routes, bounded context, canonical/source URLs, provenance, and content hashes from the same Markdown manifest.
- **New to the engine:** [Getting Started](Docs/en/tutorials/getting-started.md) - the first route: what to read, what to build, what belongs where.
- **Starting or inspecting a game project:** [Embedding Project](Docs/en/how-to/build/embedding-project.md) — expected repository shape and ownership rules.
- **Building:** [Build Workflow](Docs/en/how-to/build/) — prerequisites, presets, and validation strategy. Builds are normally driven from the embedding game repository, not from the engine checkout.
- **AI-maintainer instructions:** [AGENTS.md](AGENTS.md) — read before changing engine code or docs.

FOnline has build profiles for Windows, Linux, macOS, Android, iOS, and WebAssembly, but support is evidence-scoped. Consult the [support matrix](Docs/en/reference/platforms/support-matrix.md); a cross-build or headless smoke does not qualify a renderer, device, package, service, or store route.

## Repository layout

- [`Source/`](Source/) — engine source: `Applications/` entry points, `Client/` and `Server/` runtimes, `Common/` shared model, `Frontend/` platform/render layer, `Scripting/` bridge, `Tools/` bakers and editors, `Essentials/` low-level core, `Tests/` unit tests.
- [`BuildTools/`](BuildTools/) — staged CMake pipeline, code generation, platform toolchains, workspace and package preparation.
- [`Resources/`](Resources/) — engine-owned runtime and build resources.
- [`ThirdParty/`](ThirdParty/) — vendored dependencies (SDL, AngelScript, Asio, ImGui, glslang, SPIRV-Cross, Tracy, and more); maintenance workflow in [ThirdParty Maintenance](Docs/en/contributing/third-party/).
- [`Docs/`](Docs/) — maintained engine documentation.

## Documentation

The maintained index is [Docs/en/index.md](Docs/en/index.md); the Russian mirror is [Docs/ru/index.md](Docs/ru/index.md). Deep dives by theme:

| Theme | Docs |
|-------|------|
| Architecture & navigation | [Architecture](Docs/en/explanation/architecture/) · [SourceTree](Docs/en/contributing/source-tree/) · [Applications](Docs/en/reference/applications.md) · [Essentials](Docs/en/reference/native/essentials.md) |
| Runtime model | [Entity Model](Docs/en/explanation/entity-and-property-model/) · [Maps and Movement](Docs/en/explanation/maps-and-movement.md) · [Networking](Docs/en/explanation/authority-and-networking/) · [Persistence](Docs/en/explanation/persistence/) |
| Client & server | [Client Runtime](Docs/en/explanation/runtime/client.md) · [Server Runtime](Docs/en/explanation/runtime/server.md) · [Frontend and Rendering](Docs/en/explanation/rendering/) · [Client Updater](Docs/en/explanation/runtime/client-updater.md) |
| Scripting | [Scripting](Docs/en/explanation/scripting-runtime/) · [LifecycleAndConcurrency](Docs/en/how-to/scripting/lifecycle-and-concurrency.md) · [RemoteCalls](Docs/en/reference/scripting/remote-calls.md) · [ScriptMethodsMap](Docs/en/reference/script-api/method-ownership.md) · [Nullability](Docs/en/contributing/coding-contracts/nullability.md) · [GeneratedApiAndMetadata](Docs/en/reference/metadata/index.md) · [ContractChangeManagement](Docs/en/contributing/contract-change-management.md) |
| Build & content pipeline | [BuildWorkflow](Docs/en/how-to/build/) · [ProjectConfiguration](Docs/en/how-to/build/project-configuration.md) · [GeneratedContentWorkflow](Docs/en/how-to/build/generated-content.md) · [EngineUpgradeGuide](Docs/en/how-to/migration/engine-upgrade.md) · [SupportMatrix](Docs/en/reference/platforms/support-matrix.md) · [BuildToolsPipeline](Docs/en/reference/cmake-and-buildtools/pipeline.md) · [BakingPipeline](Docs/en/explanation/content-pipeline/baking.md) · [ConfigurationAndDataSources](Docs/en/reference/settings/configuration-and-data-sources.md) |
| Tools | [Tools](Docs/en/reference/tools/) · [Mapper Tools](Docs/en/how-to/tools/mapper.md) · [Mapper Interactive Manual](Docs/en/how-to/tools/mapper-interactive.md) · [Animation and Particle Viewers](Docs/en/how-to/tools/animation-particle-viewers.md) |
| Quality & conventions | [Testing](Docs/en/contributing/testing/index.md) · [Profiling](Docs/en/how-to/quality/profiling.md) · [ExceptionSafety](Docs/en/contributing/coding-contracts/exception-safety.md) · [SmartPointers](Docs/en/contributing/coding-contracts/smart-pointers.md) · [ThreadSafetyAnalysis](Docs/en/contributing/coding-contracts/thread-safety-analysis.md) |
| Platform debugging | [Debugging](Docs/en/troubleshooting/debugging.md) · [Web](Docs/en/how-to/platforms/web-debugging.md) · [Android](Docs/en/how-to/platforms/android-debugging.md) |

When behavior changes in a noticeable way, the owning document is updated in the same change — the docs are maintained as a source-grounded reference, not an afterthought.

## Project and community

- Site: <https://fonline.ru>
- GitHub: <https://github.com/cvet/fonline>
- License: [MIT](LICENSE)
