# FOnline Engine Documentation

This directory contains maintained documentation for reusable engine behavior. It is the user-facing documentation hub for developers who embed or modify FOnline.

## Start here

- [GettingStarted.md](GettingStarted.md) — first route through the engine docs.
- [EmbeddingProject.md](EmbeddingProject.md) — how a game repository should embed and own the engine.
- [BuildWorkflow.md](BuildWorkflow.md) — build, presets, platform prerequisites, and validation strategy.
- [ThirdPartyMaintenance.md](ThirdPartyMaintenance.md) — engine vendored dependency update, pruning, version pin, and local patch workflow.
- [DocumentationExpansionPlan.md](DocumentationExpansionPlan.md) — roadmap for researching the repository and expanding engine documentation.
- [DocumentationBacklog.md](DocumentationBacklog.md) — planned and verified documentation slices.
- [DocumentationVerificationReport.md](DocumentationVerificationReport.md) — source-check reports for recently verified slices.
- [DocumentationResearchTemplate.md](DocumentationResearchTemplate.md) — checklist/template for source-grounded doc slices.
- [DocumentationMaintenance.md](DocumentationMaintenance.md) — maintainer workflow for source-grounded docs, backlog/report updates, and validation.
- [../README.md](../README.md) — polished repository landing page.
- [../AGENTS.md](../AGENTS.md) — AI-maintainer entry point and working conventions.

## Architecture and source navigation

- [Architecture.md](Architecture.md) — engine layer map and runtime/build composition overview.
- [SourceTree.md](SourceTree.md) — source-directory guide for maintainers.
- [Applications.md](Applications.md) — executable/library entry points and CMake target ownership notes.
- [Essentials.md](Essentials.md) — low-level platform, logging, memory, filesystem, serialization, sockets, and utility layer.
- [SmartPointers.md](SmartPointers.md) — native C++ pointer ownership/nullability vocabulary and migration rules.

## Build and generation

- [BuildWorkflow.md](BuildWorkflow.md) — build, presets, platform prerequisites, and validation strategy.
- [ThirdPartyMaintenance.md](ThirdPartyMaintenance.md) — engine vendored dependency update, pruning, version pin, and local patch workflow.
- [BuildToolsPipeline.md](BuildToolsPipeline.md) — staged CMake pipeline, hooks, helpers, and change routing.
- [BakingPipeline.md](BakingPipeline.md) — resource baking, baker classes, script compile adjacency, and validation.
- [GeneratedApiAndMetadata.md](GeneratedApiAndMetadata.md) — codegen, metadata registration, generated files, and property contracts.
- [ConfigurationAndDataSources.md](ConfigurationAndDataSources.md) — config parsing, settings application, resource-pack data sources, file lookup, and caches.

## Runtime model

- [EntityModel.md](EntityModel.md) — entity/property/prototype runtime model, holders, events, and serialization relationships.
- [ExceptionSafety.md](ExceptionSafety.md) — engine-invariant stability under exceptions: terminate-on-OOM allocation model, the entity-lifecycle throw-as-signal contract, post-mutation `FO_STRONG_ASSERT` policy, and the disposition decision rules.
- [MapsMovementGeometry.md](MapsMovementGeometry.md) — map coordinates, geometry modes, path finding, line tracing, movement contexts, and map loading.
- [Networking.md](Networking.md) — network buffers, command packing, client/server transports, and ordered UDP.
- [Persistence.md](Persistence.md) — server database facade, backend implementations, commit queue, and recovery logs.

## Client, frontend, and platform runtime

- [ClientRuntime.md](ClientRuntime.md) — client lifecycle, server connection, view entities, resources, sprites, render-target bridge, and client tests.
- [ServerRuntime.md](ServerRuntime.md) — authoritative server lifecycle, managers, entity ownership, networking, persistence, movement, and updater backend.
- [FrontendAndRendering.md](FrontendAndRendering.md) — application abstraction, windows/input/audio, headless/stub modes, renderers, the screen-size/resolution + letterbox model (windowed, fullscreen, multi-client virtual windows), effects, and platform package boundaries.
- [ClientUpdater.md](ClientUpdater.md) — client host/runtime split, runtime ABI, updater protocol, and server-side updater backend.
- [Debugging.md](Debugging.md) — native debugger support, stack traces, Visual Studio helpers, and client host/runtime validation.
- [Scripting.md](Scripting.md) — script system lifecycle, AngelScript backend, native method exports, core scripts, and compile flow.
- [ScriptMethodsMap.md](ScriptMethodsMap.md) — native `///@ ExportMethod` file map by runtime side and receiver family.
- [Nullability.md](Nullability.md) — `T?` script / `ptr<T>`·`nptr<T>` native boundary contract and analyzers.
- [SmartPointers.md](SmartPointers.md) — C++ `ptr` / `nptr` / owning-pointer nullability contracts.
- [Tools.md](Tools.md) — engine tool map: baker, AS compiler, mapper, editor, asset explorer, particle editor, and asset processors.
- [MapperTools.md](MapperTools.md) — mapper lifecycle, automation, native mapper helpers, and known headless-render workflows.
- [WebDebugging.md](WebDebugging.md) — WebAssembly target preparation, package layout, and debug workflow.
- [AndroidDebugging.md](AndroidDebugging.md) — Android target preparation, package layout, ADB workflow, and remote-scene debugging.

## Source and tooling references

- [../Source/README.md](../Source/README.md) — source-tree overview.
- [Testing.md](Testing.md) — maintained full test-suite map, generated test targets, coverage routing, and validation ownership.
- [../Source/Tests/README.md](../Source/Tests/README.md) — short source-tree entry point for engine unit-test suites.
- [../BuildTools/README.md](../BuildTools/README.md) — build tooling notes.
- [../PUBLIC_API.md](../PUBLIC_API.md) — public native API notes.
- [../TUTORIAL.md](../TUTORIAL.md) — tutorial material.

## Documentation ownership

Use engine docs for reusable engine mechanics: runtime behavior, tool contracts, platform build/debug flows, generated API mechanics, updater protocol, mapper tooling, and script/native conventions.

Use the embedding project's docs for concrete game content, product rules, quests, balance, text, maps, release policy, and project-specific commands.

Engine docs must not depend on embedding-project files, scripts, tests, CI, or generated artifacts as the proof of engine behavior. If a reusable helper or regression test is cited from an engine doc, it belongs in the engine repository.

Some docs may mention `../../...` paths only as explicitly labelled embedding-project examples, for example Last Frontier. The owning procedure, validation, and source-of-truth documentation stay in the repository that owns the files.
