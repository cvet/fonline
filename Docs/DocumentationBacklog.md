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

- `verified` — [BuildWorkflow.md](BuildWorkflow.md)
  - Verified against engine CMake/BuildTools entry points, staged CMake build/test/package wiring, validators, `Source/Applications/TestingApp.cpp`, and test docs on 2026-05-18.
- `verified` — [BuildToolsPipeline.md](BuildToolsPipeline.md)
  - Verified against `BuildTools/Init.cmake`, current staged CMake files, helpers, codegen/package scripts, and build-generation ownership boundaries on 2026-05-18.
- `verified` — [BakingPipeline.md](BakingPipeline.md)
  - Verified against `BuildTools/cmake/stages/ScriptsAndBaking.cmake`, build-hash helper, baker apps/library entry points, all current `Source/Tools/*Baker.*` implementations, and baker tests on 2026-05-18.
- `verified` — [GeneratedApiAndMetadata.md](GeneratedApiAndMetadata.md)
  - Verified against `BuildTools/cmake/stages/Codegen.cmake`, `BuildTools/codegen.py`, metadata registration templates/runtime, property/entity model owners, `MetadataBaker`, and metadata/property tests on 2026-05-18.

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

- `verified` — [Scripting.md](Scripting.md)
  - Verified against `Source/Common/ScriptSystem.*`, AngelScript backend/runtime/compiler files, core scripts, native script method files, build script-compilation wiring, and scripting tests on 2026-05-18.
- `verified` — [ScriptMethodsMap.md](ScriptMethodsMap.md)
  - Verified against all 18 current `Source/Scripting/*ScriptMethods.cpp` files and 874 current `///@ ExportMethod` declarations; count refreshed on 2026-05-21 after default-argument overload collapse.
- `verified` — [Nullability.md](Nullability.md)
  - Verified against AngelScript nullable suffix parsing, `FO_NULLABLE`, codegen runtime checks, nullable analyzer tools, parent VS Code/CI task wiring, and nullable/script tests on 2026-05-18.
- `verified` — [Tools.md](Tools.md)
  - Verified against current `Source/Tools/*.h`, `Source/Tools/*.cpp`, tool application entry points, baker tests, mapper/editor/asset/particle owners, and tool/build routing docs on 2026-05-18.
- `verified` — [MapperTools.md](MapperTools.md)
  - Verified against `Source/Applications/MapperApp.cpp`, `Source/Tools/Mapper.*`, mapper/common script methods, client map-view helpers, geometry transforms, and the current embedding-project mapper-render/map-preview scripts on 2026-05-18.

## Essentials, infrastructure, and tests

- `verified` — [Essentials.md](Essentials.md)
  - Created and verified against `Source/Essentials/*.h`, `Source/Essentials/*.cpp`, `BuildTools/cmake/stages/EngineSources.cmake`, and essentials tests on 2026-05-18.
- `verified` — [ConfigurationAndDataSources.md](ConfigurationAndDataSources.md)
  - Created and verified against `Source/Common/ConfigFile.*`, `Settings.*`, `DataSource.*`, `FileSystem.*`, `CacheStorage.*`, `Source/Essentials/DiskFileSystem.*`, resource/baker consumers, and focused tests on 2026-05-18.
- `verified` — [Testing.md](Testing.md)
  - Created and verified against `Source/Applications/TestingApp.cpp`, all 79 current `Source/Tests/Test_*.cpp` files, `FO_TESTS_SOURCE`, generated test/coverage target wiring, and `Source/Tests/README.md` on 2026-05-18.
- `verified` — [DocumentationMaintenance.md](DocumentationMaintenance.md)
  - Created and verified against `../AGENTS.md`, the docs hub, backlog, expansion plan, research template, verification report, and current source-grounded doc conventions on 2026-05-18.

## Next recommended slice

The documentation backlog plan is complete as of the 2026-05-18 planned-docs slice. Future work should start from new source changes, stale findings, or explicitly requested docs rather than this initial backlog.
