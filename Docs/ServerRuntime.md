# Server Runtime

> Engine-owned documentation. This page describes reusable server runtime behavior in `Source/Server/`; game rules, world content, concrete balance, quests, and project-specific deployment policy remain in the embedding project.

## Purpose

The server runtime owns authoritative game state. It loads resources, initializes scripts and metadata, accepts network connections, creates and persists entities, validates client input, processes player/critter/map/item state, broadcasts visible changes, and runs the game loop jobs that make the world advance.

Read this page together with:

- [EntityModel.md](EntityModel.md) for entity/property/prototype ownership.
- [MapsMovementGeometry.md](MapsMovementGeometry.md) for map positions, blockers, path finding, and movement contexts.
- [Networking.md](Networking.md) for command buffers, transports, and client/server message boundaries.
- [Persistence.md](Persistence.md) for database backends and entity serialization.
- [ClientRuntime.md](ClientRuntime.md) for the client-side view of server messages.
- [ClientUpdater.md](ClientUpdater.md) for the updater backend that is hosted by `ServerEngine`.

## Source paths inspected

- `Source/Server/Server.h`
- `Source/Server/Server.cpp`
- `Source/Server/EntityManager.h`
- `Source/Server/EntityManager.cpp`
- `Source/Server/MapManager.h`
- `Source/Server/MapManager.cpp`
- `Source/Server/CritterManager.h`
- `Source/Server/CritterManager.cpp`
- `Source/Server/ItemManager.h`
- `Source/Server/ItemManager.cpp`
- `Source/Server/Player.h`
- `Source/Server/Player.cpp`
- `Source/Server/Critter.h`
- `Source/Server/Critter.cpp`
- `Source/Server/Map.h`
- `Source/Server/Map.cpp`
- `Source/Server/Location.h`
- `Source/Server/Location.cpp`
- `Source/Server/Item.h`
- `Source/Server/Item.cpp`
- `Source/Server/ClientDataValidation.h`
- `Source/Server/ClientDataValidation.cpp`
- `Source/Server/UpdaterBackend.h`
- `Source/Server/UpdaterBackend.cpp`
- `Source/Tests/Test_ServerEngine.cpp`
- `Source/Tests/Test_ServerItems.cpp`
- `Source/Tests/Test_ServerMapOperations.cpp`
- `Source/Tests/Test_ServerAdvancedOps.cpp`
- `Source/Tests/Test_ServerScriptMethods.cpp`
- `Source/Tests/Test_ClientServerIntegration.cpp`
- `Source/Tests/Test_DataBase.cpp`

## Runtime owner: `ServerEngine`

`ServerEngine` in `Source/Server/Server.h` is the server-side composition root. It derives from `BaseEngine` and implements `EntityManagerApi`, so scripts and runtime systems can create, load, destroy, and query entities through one authoritative owner.

Major responsibilities:

- load server resources through `GetServerResources(GlobalSettings&)`;
- initialize storage, metadata, language packs, maps, client packs, scripts, networking, and game logic;
- run the server job loop and frame-time synchronization;
- accept network connections and create unlogged players;
- process handshake, ping, command, movement, direction, property, and remote-call messages;
- create, load, unload, destroy, and switch critters;
- move critters by paths and movement contexts;
- dispatch entity lifecycle and gameplay events to scripts;
- persist entity/property changes through `DataBase` and `PropertiesSerializator`;
- host `UpdaterBackend` for client resource/runtime updates;
- publish health information and optional health-file output.

`ServerEngine` is intentionally authoritative: client views can request movement, commands, property changes, and remote calls, but the server validates and applies the state that matters.

## Initialization and server jobs

`ServerEngine` startup is organized as scheduled jobs rather than one monolithic constructor. The private job list in `Source/Server/Server.h` shows the runtime phases:

- `InitScriptSystemJob()`
- `InitNetworkingJob()`
- `InitStorageJob()`
- `InitMetadataJob()`
- `InitLanguageJob()`
- `InitMapsJob()`
- `InitClientPacksJob()`
- `InitGameLogicJob()`
- `InitDoneJob()`
- `SyncPointJob()`
- `FrameTimeJob()`
- `ScriptSystemJob()`
- `UnloginedPlayersJob()`
- `PlayersJob()`
- `CrittersJob()`
- `TimeEventsJob()`
- `LogDispatchJob()`
- `LoopJob()`

The public `Lock()` / `Unlock()` pair is used by tests, tooling, and controlled operations that need a consistent view of server state. `Source/Tests/Test_ServerEngine.cpp` repeatedly waits for server startup, locks the server, performs entity/script checks, and unlocks on scope exit.

## Server events

`ServerEngine` declares script-facing events for lifecycle, players, critters, maps, locations, items, movement, and static-item triggers. Important event groups include:

- lifecycle: `OnInit`, `OnGenerateWorld`, `OnStart`, `OnFinish`;
- player flow: `OnPlayerAllowCommand`, `OnPlayerLogin`, `OnPlayerLogout`, `OnPlayerCritterSwitched`;
- player-controlled motion: `OnPlayerMoveCritter`, `OnPlayerDirCritter`;
- critter motion/lifecycle: `OnCritterMoved`, `OnCritterStartMoving`, `OnCritterStopMoving`, `OnCritterTransfer`, `OnCritterInit`, `OnCritterFinish`, `OnCritterLoad`, `OnCritterUnload`;
- map/location lifecycle: `OnLocationInit`, `OnLocationFinish`, `OnMapInit`, `OnMapFinish`;
- map presence: `OnMapCritterIn`, `OnMapCritterOut`, `OnGlobalMapCritterIn`, `OnGlobalMapCritterOut`;
- item lifecycle: `OnItemInit`, `OnItemFinish`, `OnCritterItemMoved`;
- static item trigger: `OnStaticItemWalk`.

These are engine extension points. The scripts that implement actual game rules belong to the embedding project.

## Entity ownership and persistence

`EntityManager` (`Source/Server/EntityManager.h`) is the central registry and persistence boundary for server entities.

It owns:

- loading persisted locations, maps, critters, items, custom entities, and inner entities;
- registering/unregistering players, locations, maps, critters, items, and custom entities;
- persistent/non-persistent state through `MakePersistent()` and recursive persistence helpers;
- entity destruction and inner-entity destruction;
- custom entity creation/loading/view enumeration;
- entity document storage through `StoreEntityDoc()` and `LoadEntityDoc()`.

Entity changes are persisted when relevant properties are saved by `ServerEngine::OnSaveEntityValue()` through `PropertiesSerializator`. The database facade and backends are documented in [Persistence.md](Persistence.md).

A persisted entity whose proto resolves through a `Proto <Type> <Name> Remove` migration rule is **dropped** on load: `LoadEntityDoc()` detects that the proto migration rule maps the saved proto to the `Remove` sentinel and returns an empty document **without** flagging a load error, so each loader (`LoadCritter` / `LoadItem` / `LoadLocation` / `LoadMap`) returns null and its owner removes the id from its child list while the rest of the load continues. A proto that is simply absent (covered by no migration rule) still surfaces as a fatal `proto not found` load error, so deliberate deletion and accidental content gaps stay distinct. A removed proto on a player's main critter is not silently dropped — `ServerEngine::LoadCritter()` raises so the login fails loudly rather than discarding the character.

## Player and connection flow

A newly accepted `NetworkServerConnection` enters the runtime through `ServerEngine::OnNewConnection()` and becomes an unlogged `Player` through `CreateUnloginedPlayer()`.

The server then processes the player in two broad states:

1. **Unlogged player** — `ProcessUnloginedPlayer()` reads initial protocol messages and runs handshake/login logic.
2. **Logged player** — `ProcessPlayer()` handles normal game messages for an attached player/critter session.

`Player` in `Source/Server/Player.h` owns the server-side per-client send surface:

- login success;
- movement/direction/speed;
- map load and view-map messages;
- property updates;
- add/remove critters and items;
- chosen inventory updates;
- teleport, time sync, info messages;
- critter actions and item moves;
- place-to-game-complete;
- custom entity add/remove;
- selected item batches through `Send_SomeItems()`.

`Player` also tracks its controlled critter, connection, ignored property-send pair, and optional view-map context.

## Network validation and inbound commands

The server receives client messages through `ServerConnection` and dispatches them from `ServerEngine` methods such as:

- `Process_Handshake()`
- `Process_Ping()`
- `Process_Move()`
- `Process_StopMove()`
- `Process_Dir()`
- `Process_Command()`
- `Process_Property()`
- `Process_RemoteCall()`

Inbound remote-call and property payload validation is centralized in `Source/Server/ClientDataValidation.h`:

- `ValidateInboundRemoteCallData()`
- `ValidateInboundPropertyData()`

`Source/Tests/Test_ClientDataValidation.cpp` exercises invalid UTF-8, invalid enum values, non-finite floats, unknown hashed strings, invalid bools, truncated payloads, and ref-type payload validation.

For the wire-level model, see [Networking.md](Networking.md). For client behavior, see [ClientRuntime.md](ClientRuntime.md).

## Managers

### `MapManager`

`MapManager` owns map/location creation, destruction, transfer, visibility, and map content generation:

- load map data from resources;
- create/destroy locations and maps;
- regenerate maps;
- add/remove critters to/from maps;
- transfer critters between maps or to global state;
- process visible critters and items;
- create temporary map views for players;
- calculate critter visibility modes;
- generate and destroy map content.

The reusable geometry, path finding, blockers, line tracing, and map-loading concepts are documented in [MapsMovementGeometry.md](MapsMovementGeometry.md). `MapManager` applies those concepts to authoritative server state.

### `CritterManager`

`CritterManager` owns critter creation/destruction and inventory-holder operations:

- create a critter on a map;
- destroy a critter;
- destroy a critter inventory;
- add and remove items from a critter.

`Critter` itself owns visibility sets, attached player/critter relationships, moving state, map-transfer locking, visible item checks, broadcasting helpers, and critter-specific script events.

### `ItemManager`

`ItemManager` owns item creation, splitting, destruction, and movement between holders:

- create loose items and map items;
- add items to containers and critters;
- subtract/set critter item counts;
- split stacks;
- move items between critters, maps, and containers;
- remove item-holder relationships.

`Item` owns container membership and multihex entries. `StaticItem` is the static-map specialization used by map content.

## Map, location, item, and critter entities

Server entity classes combine Common-layer property/prototype behavior with server-only ownership rules:

- `Location` groups maps and raises `OnMapAdded` / `OnMapRemoved`.
- `Map` owns map fields, critter/item presence, spectators, item visibility, manual blocks, trigger verification, and `OnCheckLook` / `OnCheckTrapLook`.
- `Critter` owns visibility, current map/location/global state, inventory, moving state, player attachment, and broadcast helpers.
- `Item` owns holder/container relationships, stack/multihex behavior, and `OnCritterWalk`.
- `Player` owns connection/session state and the send surface to one client.

Do not duplicate the Common entity taxonomy here; [EntityModel.md](EntityModel.md) owns the base entity/property/prototype explanation.

## Movement and authoritative state

Client movement requests enter through `Process_Move()`, `Process_StopMove()`, and `Process_Dir()`. The server validates the request, applies script events such as `OnPlayerMoveCritter` and `OnPlayerDirCritter`, then updates the authoritative `Critter` and broadcasts the resulting state.

Server-side movement helpers include:

- `StartCritterMoving()` overloads for an existing `MovingContext` or raw path data;
- `StopCritterMoving()`;
- `ChangeCritterMovingSpeed()`;
- `ProcessCritterMoving()`;
- `ProcessCritterMovingBySteps()`.

`Source/Tests/Test_ServerEngine.cpp` includes overdue movement tests that verify route completion and blocked-hex stopping behavior. Coordinate/pathfinding mechanics remain documented in [MapsMovementGeometry.md](MapsMovementGeometry.md).

## Client update backend

During server construction, `ServerEngine` can create and load `UpdaterBackend` from client resources (`Source/Server/Server.cpp`, `Source/Server/UpdaterBackend.*`). This backend is responsible for describing and serving client resource/runtime update files to connecting clients.

`UpdaterBackend` responsibilities:

- scan client resources and binaries through `LoadFromClientResources(const GlobalSettings&)`;
- build an update descriptor grouped by update targets;
- serve requested file portions through `ProcessUpdateFile(ServerConnection*, int32_t)`;
- respond with `NetMessage::UpdateFileData` chunks;
- expose target-specific descriptors selected by binary target name.

The client host/runtime updater flow is documented in [ClientUpdater.md](ClientUpdater.md). Keep protocol-level details there and runtime hosting/ownership details here.

## Tests and validation map

Use the smallest relevant test scope when changing server behavior:

- `Source/Tests/Test_ServerEngine.cpp` — server startup, critter creation, player-controlled critter unload, script module init/events, admin remote-call allowlist, script marshalling, overdue movement.
- `Source/Tests/Test_ServerItems.cpp` — item creation/destruction, critter inventory, critter lifecycle, entity-manager queries.
- `Source/Tests/Test_ServerMapOperations.cpp` — map item/critter/hex/path/static-item/location/proto/property-filter operations.
- `Source/Tests/Test_ServerAdvancedOps.cpp` — location creation, entity-manager bulk operations, advanced critter/item operations, utility/database/string/array/dict/math/time/proto script operations.
- `Source/Tests/Test_ServerScriptMethods.cpp` — server script method surface for critter inventory/state, game queries, item operations, entity lifecycle, database/text/player ids.
- `Source/Tests/Test_ClientServerIntegration.cpp` — client/server handshake and connection event behavior.
- `Source/Tests/Test_DataBase.cpp` — persistence backend behavior used by server entity storage.

Exact test target names are generated by the embedding project's CMake/BuildTools configuration; do not hard-code one project's target names in engine docs.

## Change checklist

When changing server runtime behavior, verify:

- The changed entity type has a clear owner: `EntityManager`, `MapManager`, `CritterManager`, `ItemManager`, `Player`, or `ServerEngine`.
- Persistent state changes go through documented property/entity serialization boundaries from [Persistence.md](Persistence.md).
- Client-originated data is validated before mutating authoritative state.
- New or changed network messages are cross-linked in [Networking.md](Networking.md) and [ClientRuntime.md](ClientRuntime.md).
- Movement changes preserve [MapsMovementGeometry.md](MapsMovementGeometry.md) invariants and server blocked-hex behavior.
- Script-facing events and methods are covered by server tests or by the future scripting docs.
- Updater changes preserve the boundary between `UpdaterBackend` hosting here and client host/runtime behavior in [ClientUpdater.md](ClientUpdater.md).
