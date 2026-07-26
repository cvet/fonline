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
- Interactive tools built on the engine itself: map editor (with headless automation), content editor, asset explorer, and particle editor.

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

The engine owns reusable technology; the game owns the product. A game repository adds the engine as an `Engine/` submodule, points the engine's staged CMake pipeline at its own configuration, and gets project-named build targets for every application. The full layer map is in [Docs/Architecture.md](Docs/Architecture.md), and the embedding contract in [Docs/EmbeddingProject.md](Docs/EmbeddingProject.md):

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

- **Run the first engine-owned project:** [TUTORIAL.md](TUTORIAL.md) - configure, build, bake, start, verify, and stop the minimal headless server.
- **Inspect the canonical scaffold:** [Examples/MinimalProject/README.md](Examples/MinimalProject/README.md) - the complete project and CI smoke contract.
- **Plan and validate public examples:** [Docs/PublicExampleRepositories.md](Docs/PublicExampleRepositories.md) and its [generated registry](Docs/generated/public-examples/index.md) - ownership, exact Engine pins, compatibility lanes, repository template, support, and asset provenance.
- **Browse the generated native API:** [Docs/generated/api/index.md](Docs/generated/api/index.md) - methods, properties, events, types, settings, migrations, and source links.
- **Author prototypes:** [Docs/PrototypeFormat.md](Docs/PrototypeFormat.md) and its [generated reference](Docs/generated/prototype-format/index.md) - exact syntax, inheritance, built-in properties, references, migrations, and validation.
- **Author maps:** [Docs/MapFormat.md](Docs/MapFormat.md) and its [generated reference](Docs/generated/map-format/index.md) - `.fomap` sections, placement IDs, ownership, mapper normalization, side-specific baking, and runtime loading.
- **Author localized text:** [Docs/TextAndLocalization.md](Docs/TextAndLocalization.md) and its [generated reference](Docs/generated/text-format/index.md) - `.fotxt` syntax, language normalization, prototype `$Text`, runtime lookup, color tags, and the game-formatting boundary.
- **Author images and sprite sheets:** [Docs/ImageFormat.md](Docs/ImageFormat.md) and its [generated reference](Docs/generated/image-format/index.md) - PNG/TGA and legacy import, FOFRM composition, baking, runtime factories, atlases, caches, and validation.
- **Author shader effects:** [Docs/EffectFormat.md](Docs/EffectFormat.md) and its [generated reference](Docs/generated/effect-format/index.md) - `.fofx` sections, passes, render state, shader resources, backend outputs, runtime selection, and script values.
- **Author particles:** [Docs/ParticleFormat.md](Docs/ParticleFormat.md) and its [generated reference](Docs/generated/particle-format/index.md) - optional SPARK/Effekseer selection, `.spark`/`.efkproj` authoring, `.spk`/`.efk` baking, Mapper tools, runtime routes, and model/script integration.
- **Author bitmap fonts and lay out text:** [Docs/FontFormat.md](Docs/FontFormat.md) and its [generated reference](Docs/generated/font-format/index.md) - FOFNT/BMFont descriptors, slot binding, scaling, measurement, wrapping, rendering flags, colors, and validation.
- **Inspect project integration contracts:** [Docs/generated/cmake/index.md](Docs/generated/cmake/index.md), [Docs/generated/cli/index.md](Docs/generated/cli/index.md), [Docs/generated/helper-cli/index.md](Docs/generated/helper-cli/index.md), [Docs/generated/native-extension/index.md](Docs/generated/native-extension/index.md), [Docs/generated/package/index.md](Docs/generated/package/index.md), and [Docs/generated/public-examples/index.md](Docs/generated/public-examples/index.md) - exact CMake, main/helper BuildTools, native-extension, packaging, and example-program surfaces.
- **Add project-native C++ safely:** [Docs/NativeExtensions.md](Docs/NativeExtensions.md) - source roles, hooks, script exports, state ownership, compatibility, and executable validation.
- **Browse the generated CMake interface:** [Docs/generated/cmake/index.md](Docs/generated/cmake/index.md) - project options, strict stages and hooks, selected helpers, defaults, and source links.
- **Publishing documentation:** [Docs/SitePublication.md](Docs/SitePublication.md) - generated navigation/search/route data, rolling version and locale policy, local Jekyll preview, rendered CI artifacts, and the existing `fonline.ru` GitHub Pages route.
- **AI and offline documentation:** [llms.txt](llms.txt), [llms-full.txt](llms-full.txt), and [docs-manifest.json](docs-manifest.json) - generated routes, bounded context, canonical/source URLs, provenance, and content hashes from the same Markdown manifest.
- **New to the engine:** [Docs/GettingStarted.md](Docs/GettingStarted.md) — the first route: what to read, what to build, what belongs where.
- **Starting or inspecting a game project:** [Docs/EmbeddingProject.md](Docs/EmbeddingProject.md) — expected repository shape and ownership rules.
- **Building:** [Docs/BuildWorkflow.md](Docs/BuildWorkflow.md) — prerequisites, presets, and validation strategy. Builds are normally driven from the embedding game repository, not from the engine checkout.
- **AI-maintainer instructions:** [AGENTS.md](AGENTS.md) — read before changing engine code or docs.

Supported target platforms: Windows, Linux, macOS, Android, iOS, and Web (WebAssembly). Not every feature is equally mature on every platform — prefer the maintained docs and a real embedding project's presets over assumptions.

## Repository layout

- [`Source/`](Source/) — engine source: `Applications/` entry points, `Client/` and `Server/` runtimes, `Common/` shared model, `Frontend/` platform/render layer, `Scripting/` bridge, `Tools/` bakers and editors, `Essentials/` low-level core, `Tests/` unit tests.
- [`BuildTools/`](BuildTools/) — staged CMake pipeline, code generation, platform toolchains, workspace and package preparation.
- [`Resources/`](Resources/) — engine-owned runtime and build resources.
- [`ThirdParty/`](ThirdParty/) — vendored dependencies (SDL, AngelScript, Asio, ImGui, glslang, SPIRV-Cross, Tracy, and more); maintenance workflow in [Docs/ThirdPartyMaintenance.md](Docs/ThirdPartyMaintenance.md).
- [`Docs/`](Docs/) — maintained engine documentation.

## Documentation

The maintained index is [Docs/README.md](Docs/README.md). Deep dives by theme:

| Theme | Docs |
|-------|------|
| Architecture & navigation | [Architecture](Docs/Architecture.md) · [SourceTree](Docs/SourceTree.md) · [Applications](Docs/Applications.md) · [Essentials](Docs/Essentials.md) |
| Runtime model | [EntityModel](Docs/EntityModel.md) · [MapsMovementGeometry](Docs/MapsMovementGeometry.md) · [Networking](Docs/Networking.md) · [Persistence](Docs/Persistence.md) |
| Client & server | [ClientRuntime](Docs/ClientRuntime.md) · [ServerRuntime](Docs/ServerRuntime.md) · [FrontendAndRendering](Docs/FrontendAndRendering.md) · [ClientUpdater](Docs/ClientUpdater.md) |
| Scripting | [Scripting](Docs/Scripting.md) · [LifecycleAndConcurrency](Docs/ScriptLifecycleAndConcurrency.md) · [RemoteCalls](Docs/RemoteCalls.md) · [ScriptMethodsMap](Docs/ScriptMethodsMap.md) · [Nullability](Docs/Nullability.md) · [GeneratedApiAndMetadata](Docs/GeneratedApiAndMetadata.md) · [ApiChangeManagement](Docs/ApiChangeManagement.md) |
| Build & content pipeline | [BuildWorkflow](Docs/BuildWorkflow.md) · [BuildToolsPipeline](Docs/BuildToolsPipeline.md) · [BakingPipeline](Docs/BakingPipeline.md) · [ConfigurationAndDataSources](Docs/ConfigurationAndDataSources.md) |
| Tools | [Tools](Docs/Tools.md) · [MapperTools](Docs/MapperTools.md) |
| Quality & conventions | [Testing](Docs/Testing.md) · [ExceptionSafety](Docs/ExceptionSafety.md) · [SmartPointers](Docs/SmartPointers.md) · [ThreadSafetyAnalysis](Docs/ThreadSafetyAnalysis.md) |
| Platform debugging | [Debugging](Docs/Debugging.md) · [WebDebugging](Docs/WebDebugging.md) · [AndroidDebugging](Docs/AndroidDebugging.md) |

When behavior changes in a noticeable way, the owning document is updated in the same change — the docs are maintained as a source-grounded reference, not an afterthought.

## Project and community

- Site: <https://fonline.ru>
- GitHub: <https://github.com/cvet/fonline>
- License: [MIT](LICENSE)
