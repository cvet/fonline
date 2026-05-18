# Engine Documentation Expansion Plan

> **For Hermes:** Use this plan as the roadmap for iterative documentation work. Each documentation slice must start with repository research, cite exact source paths, update the owning `Engine/Docs/*.md` file, and finish with link/status verification. Do not commit or push unless Anton asks.

**Goal:** Build a complete, navigable, source-grounded documentation set for the FOnline engine that explains how the engine is structured, embedded, built, debugged, extended, and validated.

**Architecture:** Documentation is organized as a layered map: landing pages route readers to focused topic docs; each topic doc owns one engine subsystem or workflow; source-tree READMEs stay brief and link to deeper `Docs/` pages. Every new doc is written from repository evidence, not memory or guesses.

**Tech Stack:** Markdown, FOnline C++20 engine, CMake/BuildTools, AngelScript integration, platform workflows for native/Web/Android, git submodule embedding model.

---

## Documentation principles

- **Research first, write second.** For each topic, inspect source files, tests, BuildTools, existing docs, and embedding-project usage before writing.
- **One owner per topic.** Avoid duplicating long explanations across `README.md`, `AGENTS.md`, and `Docs/`; add short routes plus links.
- **Engine vs game separation.** Reusable engine behavior goes under `Engine/Docs/`; Last Frontier-specific commands/content stay in the game repo docs.
- **User-facing first.** Start every doc with what the reader needs to do or understand, then drill into implementation details.
- **Source-grounded references.** Include exact file/path references such as `Source/Common/Entity.cpp` or `BuildTools/cmake/stages/ScriptsAndBaking.cmake`.
- **Verification after every slice.** Run markdown link checks and `git diff --check`; confirm staged area is empty unless explicitly staging.

## Current baseline

Existing docs:

- `README.md` — engine landing page.
- `Docs/README.md` — documentation hub.
- `Docs/GettingStarted.md` — first route for new developers.
- `Docs/EmbeddingProject.md` — embedding-project ownership model.
- `Docs/BuildWorkflow.md` — build workflow overview.
- `Docs/BuildToolsPipeline.md` — BuildTools CMake stage map.
- `Docs/BakingPipeline.md` — resource baking and baker ownership.
- `Docs/GeneratedApiAndMetadata.md` — codegen and metadata registration.
- `Docs/EntityModel.md` — entity/property/prototype runtime model.
- `Docs/MapsMovementGeometry.md` — map coordinates, path finding, movement, and loading.
- `Docs/Networking.md` — network buffers, commands, transports, and UDP ordering.
- `Docs/Persistence.md` — database facade, backends, commit queue, and recovery.
- `Docs/ClientRuntime.md` — client lifecycle, connection, view entities, resources, sprites, and tests.
- `Docs/FrontendAndRendering.md` — application/window/input/audio abstraction, renderer backends, and render-target bridge.
- `Docs/ServerRuntime.md` — authoritative server lifecycle, managers, networking, persistence, movement, and updater backend.
- `Docs/ClientUpdater.md` — updater/runtime split.
- `Docs/Debugging.md` — native debugging.
- `Docs/Scripting.md` — scripting runtime, AngelScript backend, native method exports, core scripts, and compile flow.
- `Docs/ScriptMethodsMap.md` — native script method ownership by runtime side and receiver family.
- `Docs/Nullability.md` — script/native nullability.
- `Docs/Tools.md` — engine tools map: bakers, mapper, editor, asset explorer, particle editor, and application entry points.
- `Docs/MapperTools.md` — mapper lifecycle, native helper boundaries, and mapper automation.
- `Docs/WebDebugging.md` — Web debugging.
- `Docs/AndroidDebugging.md` — Android debugging.

High-value source areas to document:

- `Source/Applications/` — executable entry points.
- `Source/Common/` — shared runtime model, entities, properties, networking primitives, maps, configs.
- `Source/Client/` — client runtime, rendering-facing model, resources, network client.
- `Source/Server/` — server runtime, entity managers, database backends, networking, updater backend.
- `Source/Scripting/` — AngelScript/Mono/Native integration and script method registration.
- `Source/Tools/` — baker, mapper, editors, asset processors.
- `Source/Frontend/` — application abstraction and rendering backends.
- `Source/Essentials/` — low-level platform, logging, memory, filesystem, serialization, sockets, utility layer.
- `Source/Tests/` — executable knowledge base for engine behavior.
- `BuildTools/cmake/` — configure/generate/build/package stages.

---

## Phase 1 — Documentation map and research inventory

### Task 1.1: Create the documentation backlog index

**Objective:** Add a central page that tracks planned engine documentation topics and their research status.

**Files:**
- Create: `Docs/DocumentationBacklog.md`
- Modify: `Docs/README.md`

**Research:**
- List existing `Docs/*.md`.
- List top-level `Source/` and `BuildTools/` directories.
- Check `Source/Tests/README.md` for already documented test ownership.

**Write:**
- A status legend: `planned`, `researching`, `drafted`, `verified`.
- A topic table grouped by runtime, tools, build, platform, scripting, tests.
- For each topic, include owner doc path and source paths to inspect.

**Verify:**
- Link from `Docs/README.md` to `Docs/DocumentationBacklog.md`.
- Run markdown link check.

### Task 1.2: Create a reusable research template

**Objective:** Standardize how each future doc slice is researched.

**Files:**
- Create: `Docs/DocumentationResearchTemplate.md`
- Modify: `Docs/DocumentationBacklog.md`

**Research:**
- Inspect current docs for common section patterns.
- Inspect `AGENTS.md` for maintainer conventions.

**Write:**
- Template sections: purpose, audience, source paths inspected, tests inspected, build/preset references, current behavior, caveats, validation commands, doc links to update.
- Checklist: no game-specific leakage, links verified, stale claims avoided.

**Verify:**
- Link template from the backlog.

---

## Phase 2 — Architecture overview docs

### Task 2.1: Document engine architecture overview

**Objective:** Explain the major engine layers and how data flows between them.

**Files:**
- Create: `Docs/Architecture.md`
- Modify: `Docs/README.md`
- Optionally modify: `README.md` to link the new architecture doc.

**Research:**
- `Source/Applications/*.cpp`
- `Source/Common/EngineBase.*`
- `Source/Common/Entity.*`
- `Source/Common/ScriptSystem.*`
- `Source/Client/Client.*`
- `Source/Server/Server.*`
- `Source/Frontend/Application*.cpp`
- `BuildTools/cmake/stages/Applications.cmake`

**Write:**
- Layer map: Applications, Essentials, Common, Client, Server, Frontend, Scripting, Tools, BuildTools.
- Runtime flow: configure -> bake/generate -> launch app -> load resources -> connect client/server -> scripts drive game behavior.
- Ownership notes: what games extend vs what engine owns.

**Verify:**
- Every mentioned source path exists.
- Links added to docs index.

### Task 2.2: Document source tree guide

**Objective:** Expand source navigation beyond the short `Source/README.md`.

**Files:**
- Create: `Docs/SourceTree.md`
- Modify: `Source/README.md`
- Modify: `Docs/README.md`

**Research:**
- Directory contents under `Source/Applications`, `Source/Client`, `Source/Common`, `Source/Server`, `Source/Scripting`, `Source/Tools`, `Source/Frontend`, `Source/Essentials`, `Source/Tests`.

**Write:**
- One section per source directory.
- "Start here when changing X" routing list.
- Anti-patterns: do not put game policy into engine source; do not bypass generated metadata without documenting it.

**Verify:**
- Source README links to deeper doc.

### Task 2.3: Document application entry points

**Objective:** Explain the binaries/apps the engine builds and what each entry point does.

**Files:**
- Create: `Docs/Applications.md`
- Modify: `Docs/README.md`

**Research:**
- `Source/Applications/ClientApp.cpp`
- `Source/Applications/ServerApp.cpp`
- `Source/Applications/ServerDaemonApp.cpp`
- `Source/Applications/ServerHeadlessApp.cpp`
- `Source/Applications/ServerServiceApp.cpp`
- `Source/Applications/MapperApp.cpp`
- `Source/Applications/EditorApp.cpp`
- `Source/Applications/BakerApp.cpp`
- `Source/Applications/ASCompilerApp.cpp`
- `Source/Applications/TestingApp.cpp`
- `BuildTools/cmake/stages/Applications.cmake`

**Write:**
- App list, purpose, expected context, build target discovery notes.
- Which apps are user-facing, developer-facing, CI/test-facing, or platform-specific.

**Verify:**
- Avoid inventing target names; if target names are project-generated, say so.

---

## Phase 3 — BuildTools and generation docs

### Task 3.1: Document BuildTools CMake pipeline

**Objective:** Explain CMake stages and how the project build is assembled.

**Files:**
- Create: `Docs/BuildToolsPipeline.md`
- Modify: `BuildTools/README.md`
- Modify: `Docs/BuildWorkflow.md`
- Modify: `Docs/README.md`

**Research:**
- `BuildTools/cmake/stages/Init.cmake`
- `BuildTools/cmake/stages/ProjectOptions.cmake`
- `BuildTools/cmake/stages/CoreLibs.cmake`
- `BuildTools/cmake/stages/ThirdParty.cmake`
- `BuildTools/cmake/stages/EngineSources.cmake`
- `BuildTools/cmake/stages/Codegen.cmake`
- `BuildTools/cmake/stages/ScriptsAndBaking.cmake`
- `BuildTools/cmake/stages/Applications.cmake`
- `BuildTools/cmake/stages/Packages.cmake`
- `BuildTools/cmake/stages/Finalize.cmake`
- `BuildTools/cmake/helpers/*.cmake`

**Write:**
- Stage order and responsibility.
- What is configured, generated, baked, compiled, packaged.
- Where embedding projects plug in.

**Verify:**
- Link from BuildTools README.

### Task 3.2: Document baking/resource pipeline

**Objective:** Explain how source assets and configs become runtime resources.

**Files:**
- Create: `Docs/BakingPipeline.md`
- Modify: `Docs/BuildToolsPipeline.md`
- Modify: `Docs/README.md`

**Research:**
- `Source/Tools/Baker.*`
- `Source/Tools/*Baker.*`
- `Source/Applications/BakerApp.cpp`
- `BuildTools/cmake/stages/ScriptsAndBaking.cmake`
- `Source/Tests/Test_*Baker*.cpp`

**Write:**
- Baker responsibilities by asset type.
- Input/output concepts without hard-coding one game's exact folders unless used as an example.
- Common failure modes and validation targets.

**Verify:**
- Cross-link to game content docs only as embedding-project examples.

### Task 3.3: Document generated API and metadata

**Objective:** Explain generated public/script API files and metadata registration.

**Files:**
- Create: `Docs/GeneratedApiAndMetadata.md`
- Modify: `Docs/Nullability.md`
- Modify: `Docs/README.md`

**Research:**
- `Source/Common/MetadataRegistration.*`
- `Source/Common/MetadataRegistration-Template.cpp`
- `Source/Common/GenericCode-Template.cpp`
- `Source/Common/Properties.*`
- `BuildTools/cmake/stages/Codegen.cmake`
- `PUBLIC_API.md`
- generated API references in embedding projects if needed.

**Write:**
- What is hand-authored vs generated.
- Where annotations/contracts live.
- How to validate generated API changes.

**Verify:**
- Avoid documenting generated output as if it were hand-authored source.

---

## Phase 4 — Runtime model docs

### Task 4.1: Document entity/property/prototype model

**Objective:** Explain the core runtime data model shared by client and server.

**Files:**
- Create: `Docs/EntityModel.md`
- Modify: `Docs/README.md`

**Research:**
- `Source/Common/Entity.*`
- `Source/Common/EntityProperties.*`
- `Source/Common/EntityProtos.*`
- `Source/Common/Properties.*`
- `Source/Common/PropertiesSerializator.*`
- `Source/Common/ProtoManager.*`
- `Source/Server/*Entity*.h`
- `Source/Client/*Entity*.h`
- `Source/Tests/Test_Entity*.cpp`
- `Source/Tests/Test_Properties*.cpp`

**Write:**
- Entity/proto/property concepts.
- Serialization and replication boundaries.
- Client/server differences.
- Testing map.

**Verify:**
- Cross-link to scripting docs once available.

### Task 4.2: Document map, movement, and geometry systems

**Objective:** Explain reusable map/hex/pathfinding mechanics.

**Files:**
- Create: `Docs/MapsMovementGeometry.md`
- Modify: `Docs/README.md`

**Research:**
- `Source/Common/Geometry.*`
- `Source/Common/LineTracer.*`
- `Source/Common/Movement.*`
- `Source/Common/PathFinding.*`
- `Source/Common/MapLoader.*`
- `Source/Server/Map.*`
- `Source/Client/MapView.*`
- `Source/Tests/Test_Geometry.cpp`
- `Source/Tests/Test_LineTracer.cpp`
- `Source/Tests/Test_MapLoader.cpp`
- `Source/Tests/Test_Movement.cpp`
- `Source/Tests/Test_PathFinding.cpp`

**Write:**
- Coordinate systems, map loading, movement validation, pathfinding responsibilities.
- What the engine defines vs what scripts/game content define.

**Verify:**
- Do not duplicate project-specific map format docs; link to game docs where relevant.

### Task 4.3: Document networking and protocol basics

**Objective:** Explain client/server networking layers and command serialization.

**Files:**
- Create: `Docs/Networking.md`
- Modify: `Docs/ClientUpdater.md` if updater network notes need cross-links.
- Modify: `Docs/README.md`

**Research:**
- `Source/Common/NetBuffer.*`
- `Source/Common/NetCommand.*`
- `Source/Common/NetworkUdp.*`
- `Source/Client/NetworkClient*.cpp`
- `Source/Server/NetworkServer*.cpp`
- `Source/Server/ServerConnection.*`
- `Source/Client/ClientConnection.*`
- `Source/Tests/Test_Net*.cpp`

**Write:**
- Transport variants, command buffers, connection roles, validation boundaries.
- What not to rely on as stable public protocol unless explicitly documented.

**Verify:**
- Cross-link updater only where protocol layers overlap.

### Task 4.4: Document persistence and database backends

**Objective:** Explain server-side persistence options and data ownership.

**Files:**
- Create: `Docs/Persistence.md`
- Modify: `Docs/README.md`

**Research:**
- `Source/Server/DataBase.*`
- `Source/Server/DataBase-Json.cpp`
- `Source/Server/DataBase-Memory.cpp`
- `Source/Server/DataBase-Mongo.cpp`
- `Source/Server/DataBase-UnQLite.cpp`
- `Source/Tests/Test_DataBase.cpp`

**Write:**
- Backend list, intended use, constraints, validation.
- Relationship to entity serialization.

**Verify:**
- Avoid recommending a backend without source/test evidence.

---

## Phase 5 — Client, rendering, frontend docs

### Task 5.1: Document client runtime overview — completed

**Objective:** Explain major client systems without going into every renderer detail.

**Files:**
- Create: `Docs/ClientRuntime.md`
- Modify: `Docs/README.md`

**Research:**
- `Source/Client/Client.*`
- `Source/Client/ClientConnection.*`
- `Source/Client/ResourceManager.*`
- `Source/Client/*View.*`
- `Source/Client/DefaultSprites.*`
- `Source/Client/ModelSprites.*`
- `Source/Client/ParticleSprites.*`
- `Source/Tests/Test_Client*.cpp`

**Write:**
- Client lifecycle, resource loading, view model, server connection, script-facing hooks.

**Verify:**
- Cross-link to Web/Android debugging and frontend docs.

### Task 5.2: Document frontend/application/rendering abstraction — completed

**Objective:** Explain how platform application layers and rendering backends are selected.

**Files:**
- Create: `Docs/FrontendAndRendering.md`
- Modify: `Docs/README.md`

**Research:**
- `Source/Frontend/Application*.cpp`
- `Source/Frontend/Rendering*.cpp`
- `Source/Client/RenderTarget.*`
- `BuildTools/cmake/stages/Packages.cmake`
- Web/Android BuildTools folders.

**Write:**
- Application abstraction, headless/stub modes, OpenGL/Direct3D/null rendering roles.
- Platform package relationship.

**Verify:**
- Cross-link to platform debugging docs.

---

## Phase 6 — Server docs

### Task 6.1: Document server runtime overview — completed

**Objective:** Explain server lifecycle, managers, entities, networking, and scripts.

**Files:**
- Create: `Docs/ServerRuntime.md`
- Modify: `Docs/README.md`

**Research:**
- `Source/Server/Server.*`
- `Source/Server/*Manager.*`
- `Source/Server/Player.*`
- `Source/Server/Critter.*`
- `Source/Server/Map.*`
- `Source/Server/Location.*`
- `Source/Server/Item.*`
- `Source/Server/ClientDataValidation.*`
- `Source/Tests/Test_Server*.cpp`
- `Source/Tests/Test_ClientServerIntegration.cpp`

**Write:**
- Server app flow, manager ownership, data validation, client/server integration testing.

**Verify:**
- Cross-link persistence and networking docs.

### Task 6.2: Document updater backend integration — completed

**Objective:** Ensure updater docs connect cleanly to server runtime docs.

**Files:**
- Modify: `Docs/ClientUpdater.md`
- Modify: `Docs/ServerRuntime.md`

**Research:**
- `Source/Server/UpdaterBackend.*`
- tests covering updater/client runtime API.

**Write:**
- Add cross-links and clarify where updater backend sits in server runtime.

**Verify:**
- No duplicate long protocol sections.

---

## Phase 7 — Scripting docs

### Task 7.1: Document scripting system overview

**Status:** completed — `Docs/Scripting.md` now documents `ScriptSystem`, AngelScript backend lifecycle, attributes, entity/remote-call bridges, native exports, core scripts, Mono/native roots, tests, and validation.

**Objective:** Explain how AngelScript/Native/Mono integration is structured.

**Files:**
- Create: `Docs/Scripting.md`
- Modify: `Docs/Nullability.md`
- Modify: `Docs/GeneratedApiAndMetadata.md`
- Modify: `Docs/README.md`

**Research:**
- `Source/Common/ScriptSystem.*`
- `Source/Scripting/AngelScript/`
- `Source/Scripting/Native/`
- `Source/Scripting/Mono/`
- `Source/Scripting/*ScriptMethods.cpp`
- `Source/Tools/AngelScriptBaker.*`
- `Source/Applications/ASCompilerApp.cpp`
- `Source/Tests/Test_AngelScript*.cpp`
- `Source/Tests/Test_CommonScriptMethods.cpp`

**Write:**
- Script module lifecycle, method registration, generated API, compile/bake flow, nullability relationship.

**Verify:**
- Keep game-specific script module maps in the game docs.

### Task 7.2: Document script method ownership map

**Status:** completed — `Docs/ScriptMethodsMap.md` now maps all current `Source/Scripting/*ScriptMethods.cpp` export groups and counts by side/receiver.

**Objective:** Make script method files discoverable by runtime side and entity type.

**Files:**
- Create: `Docs/ScriptMethodsMap.md`
- Modify: `Docs/Scripting.md`

**Research:**
- All `Source/Scripting/*ScriptMethods.cpp` files.

**Write:**
- Map common/client/server/mapper methods to files and intended audience.
- Explain how to decide where a new method belongs.

**Verify:**
- Cross-link `Docs/Nullability.md` for nullable signatures.

---

## Phase 8 — Tools docs

### Task 8.1: Document tools overview

**Status:** completed — `Docs/Tools.md` now maps tool application entry points, baker processors, mapper/editor/asset/particle tools, tests, change routing, and validation.

**Objective:** Explain engine tools and which docs own their workflows.

**Files:**
- Create: `Docs/Tools.md`
- Modify: `Docs/MapperTools.md`
- Modify: `Docs/README.md`

**Research:**
- `Source/Tools/*.h`
- `Source/Tools/*.cpp`
- `Source/Applications/*App.cpp` for tool entry points.

**Write:**
- Tool list: baker, mapper, editor, asset explorer, particle editor, model/effect/image/map/proto/text bakers.
- Routing to specific docs for mapper and baking pipeline.

**Verify:**
- Avoid promising UI workflows not supported by current code.

### Task 8.2: Expand mapper documentation from source

**Status:** completed — `Docs/MapperTools.md` now starts with mapper app lifecycle, `MapperEngine` source ownership, key UI/map lifecycle methods, and extension boundaries before the project-specific headless render workflows.

**Objective:** Improve mapper docs with source-grounded lifecycle and extension points.

**Files:**
- Modify: `Docs/MapperTools.md`

**Research:**
- `Source/Tools/Mapper.*`
- `Source/Applications/MapperApp.cpp`
- mapper script methods under `Source/Scripting/`.

**Write:**
- Mapper app lifecycle, automation hooks, native helper boundaries.

**Verify:**
- Existing links still pass.

---

## Phase 9 — Essentials and infrastructure docs

### Task 9.1: Document Essentials layer

**Objective:** Explain low-level primitives shared across the engine.

**Files:**
- Create: `Docs/Essentials.md`
- Modify: `Docs/README.md`

**Research:**
- `Source/Essentials/BaseLogging.*`
- `Source/Essentials/BasicCore.*`
- `Source/Essentials/DiskFileSystem.*`
- `Source/Essentials/ExceptionHandling.*`
- `Source/Essentials/MemorySystem.*`
- `Source/Essentials/NetSockets.*`
- `Source/Essentials/Platform.*`
- `Source/Essentials/StackTrace.*`
- `Source/Essentials/WorkThread.*`
- corresponding tests in `Source/Tests/`.

**Write:**
- What belongs in Essentials, common pitfalls, where not to place higher-level game/runtime logic.

**Verify:**
- Cross-link debugging docs for stack traces/logging.

### Task 9.2: Document configuration and filesystem/data sources

**Objective:** Explain config loading and abstract data access.

**Files:**
- Create: `Docs/ConfigurationAndDataSources.md`
- Modify: `Docs/BuildWorkflow.md`
- Modify: `Docs/README.md`

**Research:**
- `Source/Common/ConfigFile.*`
- `Source/Common/DataSource.*`
- `Source/Common/FileSystem.*`
- `Source/Common/CacheStorage.*`
- `Source/Essentials/DiskFileSystem.*`
- tests for config/data/file/cache.

**Write:**
- Config file role, data sources, cache/storage behavior, embedding-project interaction.

**Verify:**
- Do not copy Last Frontier `.fomain` details into engine docs except as an example link.

---

## Phase 10 — Testing and maintenance docs

### Task 10.1: Expand test suite documentation

**Objective:** Turn `Source/Tests/README.md` into a useful test map and connect it to docs.

**Files:**
- Modify: `Source/Tests/README.md`
- Create: `Docs/Testing.md`
- Modify: `Docs/README.md`

**Research:**
- All `Source/Tests/Test_*.cpp` filenames.
- Test app entry point in `Source/Applications/TestingApp.cpp`.
- Build target generation in BuildTools.

**Write:**
- Test categories by subsystem.
- How to choose minimal relevant tests for a doc/code change.
- Where exact project target names come from.

**Verify:**
- Link from BuildWorkflow validation section.

### Task 10.2: Document documentation maintenance workflow

**Objective:** Define how we keep engine docs fresh while continuing repo research.

**Files:**
- Create: `Docs/DocumentationMaintenance.md`
- Modify: `Docs/README.md`
- Modify: `AGENTS.md` if it should route AI maintainers to this workflow.

**Research:**
- Existing `AGENTS.md` rules.
- This plan and backlog docs.

**Write:**
- Doc ownership rules.
- Required checks: link check, `git diff --check`, source path existence, no staged files unless requested.
- Review checklist for stale claims.

**Verify:**
- Link from `AGENTS.md` only if it improves AI routing.

---

## Standard per-task execution checklist

For every task above:

1. Read the owning existing docs.
2. Inspect listed source files and adjacent tests.
3. Search for names/classes/functions referenced in the topic.
4. Draft or update the doc with exact source-path references.
5. Update `Docs/README.md` and related cross-links.
6. Run markdown link check over `README.md`, `AGENTS.md`, `Docs/**/*.md`, `Source/README.md`, `Source/Tests/README.md`, and `BuildTools/README.md`.
7. Run `git diff --check`.
8. Run `git status --short` and confirm no accidental staging.
9. Report changed files and remaining questions.

## Suggested order for the next work session

1. `Docs/Essentials.md`
2. `Docs/ConfigurationAndDataSources.md`
3. `Docs/Testing.md`
4. `Docs/DocumentationMaintenance.md`
6. `Docs/ConfigurationAndDataSources.md`
7. `Docs/Testing.md`
8. `Docs/DocumentationMaintenance.md`

This order documents scripting/tooling next, then low-level infrastructure, tests, and maintenance.
