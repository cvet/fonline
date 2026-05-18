# Documentation Backlog

This backlog tracks the planned engine documentation set. It exists so documentation work stays source-grounded: every topic lists the source areas to inspect before writing.

## Status legend

- `planned` — topic is identified but not researched yet.
- `researching` — source/tests/build files are being inspected.
- `drafted` — a first doc exists, but it may need deeper validation.
- `verified` — links and formatting were checked after the latest edit.

## Navigation and maintenance

- `verified` — [README.md](../README.md), [Docs/README.md](README.md), [GettingStarted.md](GettingStarted.md), [EmbeddingProject.md](EmbeddingProject.md), [BuildWorkflow.md](BuildWorkflow.md), [DocumentationExpansionPlan.md](DocumentationExpansionPlan.md), this backlog, and [DocumentationResearchTemplate.md](DocumentationResearchTemplate.md).
- `verified` — [SourceTree.md](SourceTree.md).
- `verified` — [Architecture.md](Architecture.md), [Applications.md](Applications.md)
  - Verified against `Source/Applications/`, common engine base/entity/script-system owners, client/server/frontend entry points, and application CMake wiring on 2026-05-18.

## Architecture and source navigation

- `verified` — [Architecture.md](Architecture.md)
  - Verified against `Source/Applications/`, `Source/Common/EngineBase.*`, `Source/Common/Entity.*`, `Source/Common/ScriptSystem.*`, client/server/frontend owners, and application CMake wiring on 2026-05-18.
- `verified` — [SourceTree.md](SourceTree.md)
  - Verified against current top-level `Source/` areas, `BuildTools/cmake/stages/Applications.cmake`, and `Source/Tests/` on 2026-05-18.
- `verified` — [Applications.md](Applications.md)
  - Verified against all current `Source/Applications/*.cpp` entry points plus `BuildTools/cmake/stages/Applications.cmake` and `BuildTools/cmake/helpers/Build.cmake` on 2026-05-18.

## Build, generation, and resources

- `drafted` — [BuildWorkflow.md](BuildWorkflow.md)
  - Inspect: `BuildTools/cmake/`, embedding project presets, platform package folders.
- `drafted` — [BuildToolsPipeline.md](BuildToolsPipeline.md)
  - Inspect: `BuildTools/cmake/stages/*.cmake`, `BuildTools/cmake/helpers/*.cmake`.
- `drafted` — [BakingPipeline.md](BakingPipeline.md)
  - Inspect: `Source/Tools/Baker.*`, `Source/Tools/*Baker.*`, `Source/Applications/BakerApp.cpp`, `BuildTools/cmake/stages/ScriptsAndBaking.cmake`, `Source/Tests/Test_*Baker*.cpp`.
- `drafted` — [GeneratedApiAndMetadata.md](GeneratedApiAndMetadata.md)
  - Inspect: `Source/Common/MetadataRegistration.*`, `Source/Common/GenericCode-Template.cpp`, `Source/Common/Properties.*`, `BuildTools/cmake/stages/Codegen.cmake`, `BuildTools/codegen.py`, `PUBLIC_API.md`.

## Runtime model

- `verified` — [EntityModel.md](EntityModel.md)
  - Verified against `Source/Common/Entity.*`, `EntityProperties.*`, `EntityProtos.*`, `Properties.*`, `PropertiesSerializator.*`, `ProtoManager.*`, and entity/property tests on 2026-05-18.
- `verified` — [MapsMovementGeometry.md](MapsMovementGeometry.md)
  - Verified against `Source/Common/Geometry.*`, `LineTracer.*`, `Movement.*`, `PathFinding.*`, `MapLoader.*`, `Source/Tools/MapBaker.*`, and map/movement tests on 2026-05-18.
- `verified` — [Networking.md](Networking.md)
  - Verified against `Source/Common/NetBuffer.*`, `NetCommand.*`, `NetworkUdp.*`, `Source/Client/NetworkClient*`, `Source/Server/NetworkServer*`, network tests, and client/server integration tests on 2026-05-18.
- `verified` — [Persistence.md](Persistence.md)
  - Verified against `Source/Server/DataBase.*`, `Source/Server/DataBase-*.cpp`, and `Source/Tests/Test_DataBase.cpp` on 2026-05-18.

## Client, server, and platform runtime

- `verified` — [ClientRuntime.md](ClientRuntime.md)
  - Verified against `Source/Client/Client.*`, `ClientConnection.*`, `ResourceManager.*`, client view/sprite/render-target managers, and client tests on 2026-05-18.
- `verified` — [FrontendAndRendering.md](FrontendAndRendering.md)
  - Verified against `Source/Frontend/Application*.cpp`, `Rendering*.cpp`, `Source/Client/RenderTarget.*`, `SpriteManager.*`, `EffectManager.*`, `BuildTools/cmake/stages/Packages.cmake`, and `Source/Tests/Test_Rendering.cpp` on 2026-05-18.
- `verified` — [ServerRuntime.md](ServerRuntime.md)
  - Verified against `Source/Server/Server.*`, `Source/Server/*Manager.*`, `Source/Server/Player.*`, `Source/Server/Critter.*`, `Source/Server/Map.*`, `Source/Server/Location.*`, `Source/Server/Item.*`, `Source/Server/ClientDataValidation.*`, `Source/Server/UpdaterBackend.*`, server tests, and integration tests on 2026-05-18.
- `verified` — [ClientUpdater.md](ClientUpdater.md)
  - Verified against `Source/Applications/ClientApp.cpp`, `ClientLib.cpp`, `Source/Client/ClientRuntimeApi.*`, `Source/Client/Updater.*`, `Source/Server/UpdaterBackend.*`, `Source/Server/Server.cpp`, updater settings, packaging code, runtime API tests, and updater pipeline tests on 2026-05-18.
- `verified` — [WebDebugging.md](WebDebugging.md), [AndroidDebugging.md](AndroidDebugging.md), [Debugging.md](Debugging.md)
  - Verified against platform BuildTools flows, web/Android package helpers, VS Code task/launch files, frontend/application debugging hooks, stack-trace/exception handling code, natvis files, and stack/exception tests on 2026-05-18.

## Scripting and tools

- `drafted` — [Scripting.md](Scripting.md)
  - Inspect: `Source/Common/ScriptSystem.*`, `Source/Scripting/AngelScript/`, `Source/Scripting/Native/`, `Source/Scripting/Mono/`, `Source/Scripting/*ScriptMethods.cpp`, AngelScript tests.
- `drafted` — [ScriptMethodsMap.md](ScriptMethodsMap.md)
  - Inspect: all `Source/Scripting/*ScriptMethods.cpp` files.
- `drafted` — [Nullability.md](Nullability.md)
  - Inspect: nullable annotations in native/script boundary code and analyzers.
- `drafted` — [Tools.md](Tools.md)
  - Inspect: `Source/Tools/*.h`, `Source/Tools/*.cpp`, tool app entry points.
- `drafted` — [MapperTools.md](MapperTools.md)
  - Inspect: `Source/Tools/Mapper.*`, `Source/Applications/MapperApp.cpp`, mapper script methods.

## Essentials, infrastructure, and tests

- `planned` — Docs/Essentials.md
  - Inspect: `Source/Essentials/*.h`, `Source/Essentials/*.cpp`, essentials tests.
- `planned` — Docs/ConfigurationAndDataSources.md
  - Inspect: `Source/Common/ConfigFile.*`, `Source/Common/DataSource.*`, `Source/Common/FileSystem.*`, `Source/Common/CacheStorage.*`, `Source/Essentials/DiskFileSystem.*`.
- `planned` — Docs/Testing.md
  - Inspect: `Source/Tests/Test_*.cpp`, `Source/Applications/TestingApp.cpp`, BuildTools test target generation.
- `planned` — Docs/DocumentationMaintenance.md
  - Inspect: [AGENTS.md](../AGENTS.md), this backlog, [DocumentationExpansionPlan.md](DocumentationExpansionPlan.md).

## Next recommended slice

The next high-value verification slice is `Docs/BuildToolsPipeline.md`, `Docs/BakingPipeline.md`, and `Docs/GeneratedApiAndMetadata.md`, because the architecture/application overview is now source-checked and the remaining drafted build/generation slice shares CMake/codegen/baker ownership.
