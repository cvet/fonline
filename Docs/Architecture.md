# Engine Architecture

This document gives a source-grounded map of the FOnline engine layers. Use it when deciding where a behavior belongs before opening subsystem-specific docs.

## Big picture

FOnline is organized around a reusable engine embedded by a game project. The game project owns content, scripts, product configuration, and release policy; the engine owns reusable runtime systems, tools, generated API infrastructure, platform frontends, and build composition.

The main layers are:

- **Applications** — executable and library entry points in `Source/Applications/`.
- **Essentials** — low-level platform, memory, filesystem, logging, serialization, sockets, and utilities in `Source/Essentials/`.
- **Common runtime** — shared engine model in `Source/Common/`: entities, properties, prototypes, maps, networking primitives, config, scripts, and engine base services.
- **Client runtime** — presentation/resource/network-client side in `Source/Client/`.
- **Server runtime** — authoritative world, managers, database backends, network-server side, and updater backend in `Source/Server/`.
- **Frontend** — application/window/rendering abstraction in `Source/Frontend/`.
- **Scripting** — AngelScript, Native, Managed, and script method registration in `Source/Scripting/`.
- **Tools** — baker, mapper, editors, asset processors, and related developer tooling in `Source/Tools/`.
- **BuildTools** — CMake stages, helpers, toolchains, platform project generation, package layout, and validation support in `BuildTools/`.

## Application layer

`Source/Applications/` is the practical entry-point directory. It contains app wrappers such as:

- `ClientApp.cpp` and `ClientLib.cpp` for client host/runtime flows.
- `ServerApp.cpp`, `ServerDaemonApp.cpp`, `ServerHeadlessApp.cpp`, and `ServerServiceApp.cpp` for server variants.
- `MapperApp.cpp` and `EditorApp.cpp` for developer-facing tools.
- `BakerApp.cpp`, `ASCompilerApp.cpp`, and `ManagedScriptBakerApp.cpp` for generation/build support.
- `TestingApp.cpp` for test execution.

`BuildTools/cmake/stages/Applications.cmake` wires these files into project-specific targets based on build options such as client/server/tool/platform/library modes. Avoid hard-coding target names in engine docs: target names are often derived from the embedding project's `FO_DEV_NAME` and presets.

See [Applications.md](Applications.md) for the application map.

## Source paths inspected

- `Source/Applications/`
- `Source/Common/EngineBase.h`
- `Source/Common/EngineBase.cpp`
- `Source/Common/Entity.h`
- `Source/Common/Entity.cpp`
- `Source/Common/ScriptSystem.h`
- `Source/Common/ScriptSystem.cpp`
- `Source/Client/Client.h`
- `Source/Server/Server.h`
- `Source/Frontend/Application.h`
- `Source/Frontend/ApplicationInit.cpp`
- `BuildTools/cmake/stages/Applications.cmake`

## Common runtime layer

`Source/Common/` holds shared concepts used by client, server, tools, and scripts. Important entry points include:

- `EngineBase.h` / `EngineBase.cpp` — base engine services and shared runtime state.
- `Entity.h` / `Entity.cpp` — exported entity concepts shared across runtime sides.
- `Properties.h`, `EntityProperties.h`, `EntityProtos.h`, `ProtoManager.h` — property/prototype model.
- `ScriptSystem.h` / `ScriptSystem.cpp` — script engine abstraction used by runtime sides and tools.
- `Geometry.h`, `Movement.h`, `PathFinding.h`, `MapLoader.h` — reusable map and movement primitives.
- `NetBuffer.h`, `NetworkUdp.h` — common networking primitives.
- `ConfigFile.h`, `DataSource.h`, `FileSystem.h`, `CacheStorage.h` — config and data access support.

This layer should stay reusable. Game rules should generally be expressed through content/scripts or project-native extensions, not by embedding one project's policy into common engine code.

## Client and server layers

`Source/Client/Client.h` includes the client-side composition points: application integration, resource/cache access, views for critters/items/locations/maps, effects, rendering-facing structures, and client connection code.

`Source/Server/Server.h` includes authoritative runtime pieces: entities, managers, database, geometry, scripting-facing server objects, client validation, networking, and updater backend support.

Treat client and server docs as separate because their ownership differs:

- The **client** presents local views, resources, UI-facing objects, and network-client behavior.
- The **server** owns authoritative world state, persistence, entity managers, validation, and network-server behavior.

## Frontend layer

`Source/Frontend/Application.h` and the related `Application*.cpp` / `Rendering*.cpp` files abstract platform app startup and rendering backends. This layer is where headless/stub/native frontend differences belong, not in game docs.

Platform workflow docs:

- [WebDebugging.md](WebDebugging.md)
- [AndroidDebugging.md](AndroidDebugging.md)
- [Debugging.md](Debugging.md)

## Scripting layer

`Source/Common/ScriptSystem.*` defines the common script-system abstraction. `Source/Scripting/` provides runtime-specific method registration and integration folders:

- `Source/Scripting/AngelScript/`
- `Source/Scripting/Native/`
- `Source/Scripting/Managed/`
- `Source/Scripting/*ScriptMethods.cpp`

The engine owns the reusable script/native bridge. A game project owns concrete game script modules and gameplay logic.

## Build and generation layer

`BuildTools/cmake/stages/` is the staged CMake pipeline. Current stage files include:

- `Init.cmake`
- `ProjectOptions.cmake`
- `CoreLibs.cmake`
- `ThirdParty.cmake`
- `EngineSources.cmake`
- `Codegen.cmake`
- `Applications.cmake`
- `ScriptsAndBaking.cmake`
- `Packages.cmake`
- `Finalize.cmake`

These stages compose engine code with embedding-project configuration. Read [BuildWorkflow.md](BuildWorkflow.md) before changing build behavior.

## Typical runtime flow

A normal embedding-project workflow looks like this:

1. The game repository configures CMake from the project root.
2. BuildTools loads project options and engine sources.
3. Codegen and baking steps prepare generated API/resources/scripts.
4. Applications are built from `Source/Applications/` entry points.
5. Runtime starts through the selected app: client, server, mapper, baker, test app, or platform package.
6. Client/server/tools use common runtime services and call into game-owned scripts/content where appropriate.

## Where to document changes

- Architecture-wide behavior: this file.
- Source navigation: [SourceTree.md](SourceTree.md).
- App entry points: [Applications.md](Applications.md).
- Build workflow: [BuildWorkflow.md](BuildWorkflow.md) and [BuildToolsPipeline.md](BuildToolsPipeline.md).
- Script/native boundary: [Nullability.md](Nullability.md) and [Scripting.md](Scripting.md).
- Platform debugging: [WebDebugging.md](WebDebugging.md), [AndroidDebugging.md](AndroidDebugging.md), [Debugging.md](Debugging.md).
