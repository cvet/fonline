# Source Tree Guide

This guide explains where to start when navigating `Source/`. It complements the shorter [../Source/README.md](../Source/README.md) file.

## Quick routing

- Changing executable startup or target entry points: `Source/Applications/` and [Applications.md](Applications.md).
- Changing low-level platform/utilities: `Source/Essentials/`.
- Changing shared entity/property/map/config/network primitives: `Source/Common/`.
- Changing client-side runtime or views: `Source/Client/`.
- Changing authoritative world/server behavior: `Source/Server/`.
- Changing scripting integration or script-visible native methods: `Source/Scripting/`.
- Changing developer tools, baking, mapper, or editors: `Source/Tools/`.
- Changing application/window/rendering abstraction: `Source/Frontend/`.
- Looking for behavior examples or regression coverage: `Source/Tests/`.

## `Source/Applications/`

Contains app and library entry points. Examples include client, server variants, mapper, editor, baker, AngelScript compiler, and testing app wrappers. Build target wiring is in `BuildTools/cmake/stages/Applications.cmake`.

See [Applications.md](Applications.md).

## `Source/Essentials/`

Low-level reusable primitives. Current files include logging, core helpers, compression, containers, serialization, filesystem, exception handling, memory system, sockets, platform helpers, stack traces, string utilities, strong types, time helpers, and worker threads.

This layer should not grow game-specific rules. It should remain usable by every runtime side.

## `Source/Common/`

Shared runtime code used by client/server/tools/scripts. Key areas include:

- Engine base and shared setup: `EngineBase.*`, `Common.*`.
- Entities/properties/prototypes: `Entity.*`, `EntityProperties.*`, `EntityProtos.*`, `Properties.*`, `ProtoManager.*`.
- Maps and movement: `MapLoader.*`, `Geometry.*`, `Movement.*`, `PathFinding.*`, `LineTracer.*`.
- Networking primitives: `NetBuffer.*`, `NetworkUdp.*`.
- Config/data access: `ConfigFile.*`, `DataSource.*`, `FileSystem.*`, `CacheStorage.*`.
- Script bridge: `ScriptSystem.*`.

If a change is reusable and shared by both client and server, it likely starts here.

## `Source/Client/`

Client-side runtime and presentation-facing state. It includes client startup/composition, connection handling, resource management, views for critters/items/maps/locations/player state, sprite/model/effect managers, render targets, and network-client transport variants.

Keep authoritative game-state decisions out of the client unless the server contract and validation are documented.

## `Source/Server/`

Authoritative runtime. It includes server startup/composition, players, critters, items, maps, locations, entity managers, data validation, database backends, network-server transport variants, server connections, and updater backend support.

Server behavior is usually where persistence, validation, and authoritative entity lifecycle questions start.

## `Source/Scripting/`

Script integration and script-visible native method registration. It is split into integration folders (`AngelScript`, `Native`, `Managed`) and method registration files grouped by runtime side and entity type, such as common/client/server global methods and critter/item/map/player methods.

Use [Nullability.md](Nullability.md) when changing nullable script/native signatures.

## `Source/Tools/`

Developer and build-time tools. Current tool files include baker classes, config/effect/image/map/model/proto/text bakers, mapper, editor, asset explorer, and particle editor.

Build/resource pipeline docs should cite these files and the CMake stage that invokes them rather than guessing from app names.

## `Source/Frontend/`

Application and rendering abstraction. It contains `Application*.cpp` variants and rendering backends such as Direct3D, OpenGL, null rendering, and shared rendering interfaces.

This layer is relevant for native client startup, headless modes, testing, Web, and Android platform notes.

## `Source/Tests/`

The tests are the executable knowledge base for many engine subsystems. File names are grouped by subsystem (`Test_Geometry.cpp`, `Test_NetBuffer.cpp`, `Test_DataBase.cpp`, `Test_AngelScript*.cpp`, etc.). Expand [../Source/Tests/README.md](../Source/Tests/README.md) when adding new test categories; add a dedicated testing page only after it is researched against the current test runner and generated targets.

## Navigation anti-patterns

- Do not infer target names from one embedding project and document them as universal engine names.
- Do not put game-specific behavior into engine docs unless clearly labelled as an example.
- Do not document generated output as hand-authored source.
- Do not change source-tree READMEs into large manuals; use focused `Docs/` pages and link to them.
