# Server Runtime

> Engine-owned documentation. This page describes reusable server runtime behavior in `Source/Server/`; game rules, world content, concrete balance, quests, and project-specific deployment policy remain in the embedding project.

## Purpose

The server runtime owns authoritative game state. It loads resources, initializes scripts and metadata, accepts network connections, creates and persists entities, validates client input, processes player/critter/map/item state, broadcasts visible changes, and runs the game loop jobs that make the world advance.

For how this state stays internally consistent when an exception is thrown mid-operation — the WorkerPool catch-and-continue model, the entity-lifecycle throw-as-signal contract, and the `throw` / `FO_VERIFY_*` / `FO_STRONG_ASSERT` error tiers — see [ExceptionSafety.md](ExceptionSafety.md).

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
- `Source/Server/WorkerPool.h`
- `Source/Server/WorkerPool.cpp`
- `Source/Essentials/WorkThread.h`
- `Source/Essentials/WorkThread.cpp`
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

- `InitHealthFileJob()`
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

Startup runs on the `_starter` worker thread, so a failure surfaces asynchronously. If any mandatory init job throws — for example `InitStorageJob()` when the database is unreachable — the starter's exception handler sets `IsStartingError()` and clears the remaining jobs before global exception reporting runs: `IsStarted()` never becomes true, and the worker pool, database connection, and time synchronization are never established. This ordering keeps the host-visible startup-failure flag prompt even when stack-trace collection is slow. Host apps must observe this rather than block forever: `ServerHeadlessApp`, `ServerDaemonApp`, and `ServerServiceApp` wait on `IsQuitRequested() || IsStartingError()` and turn a start error into a non-success quit, instead of leaving the process listening but non-functional (the failure mode behind a Staging incident where a down MongoDB left the headless server half-initialized for hours). `Shutdown()` is correspondingly safe to call on a partially-initialized engine: the worker-pool drain and the database / sync-time flushes are gated on `reached_running_state` (the presence of `_workerPool`, which is created last in `InitMetadataJob` after the DB connect and time-sync), so an aborted startup tears down cleanly instead of dereferencing the null pool (`WorkerPool::Clear` locking a null pool's mutex) or tripping the connected/synchronized invariants. `Source/Tests/Test_ServerEngine.cpp` (`ServerEngineShutdownIsSafeAfterStartupFailure`) pins this by forcing an unrecognized `DbStorage`, asserting the start error, and requiring `Shutdown()` to complete without crashing.

The public `Lock()` / `Unlock()` pair is used by tests, tooling, and controlled operations that need a consistent view of server state. `Source/Tests/Test_ServerEngine.cpp` repeatedly waits for server startup, locks the server, performs entity/script checks, and unlocks on scope exit.

Script-exported map critter queries (`Map.GetCritters(...)`, "who sees" variants, and property-filtered lookups) rely on the map access validation performed by the script dispatch layer: callers must already hold map coverage, and concurrent map membership mutation under that cover is a bug to surface rather than mask by taking extra critter refs.

Property serialization through `Properties::StoreData()` returns pointer/size lists backed by the entity's live property storage plus the per-call send cache. Callers copy those chunks immediately into network or persistence buffers and must already hold the entity cover that protects property reads. ThreadSanitizer builds annotate the engine's custom `EntityLock` so this external cover is visible to the sanitizer without adding per-property snapshot copies.

Connected players are processed by keyed `WorkerPool` jobs. `OnPlayerConnected()` submits an `UnloginedPlayerJob()` for the temporary player object, and `OnPlayerLogined()` cancels the unlogged job key and submits the logged-in `PlayerJob()`. `Player.HardDisconnect()` and other hard-disconnect paths only mark the underlying connection as disconnected; logged-in player teardown (`OnPlayerLogout`, critter detach, view reset, destroyed mark, and unregister) belongs to the next `PlayerJob()` pass through `ProcessPlayer()`. Code that continues after a script-visible player event should validate possible connection/control changes, but it should not treat hard disconnect as an inline player-destruction path.

`SwitchPlayerCritter()` sends the new critter's initial info before `OnPlayerCritterSwitched`. `OnCritterSendInitialInfo` can re-enter scripts and detach or switch the player again, so the switch notification is emitted only if the player still controls the same critter after initial-info scripts return.

Typed entity destruction has a single active owner once the target is marked `Destroying`. `OnItemFinish`, `OnCritterFinish`, and `OnLocationFinish` handlers may observe the entity and may issue redundant destroy calls, but they must not complete the same teardown inline; the native owner asserts that the entity still exists after the finish event. Map and location destruction apply the same rule across the owning pair. Once `DestroyMap()` marks a map as destroying, scripted events in that flow may not destroy the owning location to take over the same map; `DestroyLocation()` asserts that none of its maps is already in another destroy-flow before it marks them. `OnMapFinish` and `OnMapRemoved` handlers therefore run while the map still exists, but native continuation asserts that the same map and location were not destroyed behind the current owner. Map content destruction may still detach an already-`Destroying` non-player critter from the map without issuing another finish event; this only completes the map containment edge when the critter's own destroy owner is still active. For the same reason, removing an item from a critter that is already `Destroying` (inventory teardown inside `DestroyCritter`) does not fire `OnCritterItemMoved`: the item is being destroyed with its owner rather than relocated, and re-entering scripts there would let an item-movement handler attach a new inner entity (for example a modifier `StartEvent`) to the already-destroying critter, which the entity layer rejects. Normal item moves on a live critter still fire the event.

`WorkThread` and `WorkerPool` each expose a raw completed-job counter through a `GetDiagnostics()` snapshot. `ServerEngine` keeps a separate throughput counter for jobs that should be visible in server stats: the `_starter` initialization sequence is excluded, and recurring service jobs that mostly reflect scheduler cadence (`SyncPointJob`, `TimeEventJob`, `FrameTimeJob`, `HealthFileJob`, and `HealthFileWriteJob`) are excluded too. The always-open Info summary reports jobs per second, jobs per minute, total completed visible jobs, and CPU load for the machine and current process. The separate `Performance details` panel is closed by default and expands raw per-executor job counts, worker-pool internals, and per-core system CPU load. Job throughput is the live server cadence metric. The former loop-based metrics — per-loop time statistics (average/min/max/last loop time), the loops-per-second counter (and its Tracy plot), and the `Server.LoopAverageTimeInterval` setting — were all removed as the server moves from loop-based to event-based execution; only the `Tracy` "Server jobs per second" plot remains.

`FrameTimeJob` updates the engine `FrameTime` cache on a dedicated high-frequency `Server.FrameTimePeriodNs` cadence. Server movement uses this cached frame time for `MovingContext` start times, speed changes, step advancement, and outgoing movement snapshots instead of calling `nanotime::now()` in those hot paths.

CPU percentages come from `Platform::GetCpuUsageSnapshot()`: `ServerEngine` samples it about once per second and diffs consecutive snapshots. System load is the busy fraction of the whole machine (and per core); process load is this process's share normalized to one machine's worth of capacity, while `Performance details` also shows the un-normalized "process core load" (which can exceed 100% on multiple cores, like `top`).

The stat fields are updated on `_mainWorker` (inside `SyncPointJob`) and read only on `_mainWorker` itself (`GetHealthInfo()`) and by the visible server app's `DrawGui`, which reads them behind `Lock()` (serialized against the main worker by the sync point). Because nothing reads them from another thread, the fields are plain (no atomics needed).

## Server events

`ServerEngine` declares script-facing events for lifecycle, players, critters, maps, locations, items, movement, and static-item triggers. Important event groups include:

- lifecycle: `OnInit`, `OnGenerateWorld`, `OnStart`, `OnFinish`;
- player flow: `OnPlayerLogin`, `OnPlayerLogout`, `OnPlayerCritterSwitched`;
- player-controlled motion: `OnPlayerMoveCritter`, `OnPlayerDirCritter`;
- critter motion/lifecycle: `OnCritterMoved`, `OnCritterStartMoving`, `OnCritterStopMoving`, `OnCritterTransfer`, `OnCritterInit`, `OnCritterFinish`, `OnCritterLoad`, `OnCritterUnload`;
- map/location lifecycle: `OnLocationInit`, `OnLocationFinish`, `OnMapInit`, `OnMapFinish`;
- map presence: `OnMapCritterIn`, `OnMapCritterOut`, `OnGlobalMapCritterIn`, `OnGlobalMapCritterOut`;
- item lifecycle: `OnItemInit`, `OnItemFinish`, `OnCritterItemMoved`;
- static item trigger: `OnStaticItemWalk`.

These are engine extension points. The scripts that implement actual game rules belong to the embedding project.

`MapManager::Transfer()` emits `OnCritterTransfer` only after the critter transfer, attached-critter transfers, and final visibility refresh finish. Nested event paths may destroy the transferred critter or previous-map argument before that final notification attempt; `ValidateEntityAccess()` accepts that state and event dispatch suppresses script callbacks whose entity arguments are already destroyed. While the transfer lock is held, the critter's target map/global ownership remains an asserted invariant rather than a recoverable branch.

Script event handlers may re-enter item movement while an item is already in its committed add state. Native helpers that report a completed move therefore validate the final ownership after firing the event: `AddItemToCritter()` throws if the committed item no longer belongs to the target critter, `CreateItemOnHex()` / script `Map.AddItem()` throw if the created item no longer belongs to the target map hex, and `MoveItem(..., Map*)` returns it only if it still belongs to the target map. A partial-stack `MoveItem()` splits the source before delivery, and the split's init event can re-enter scripts and destroy the destination; if it does, the helper folds the split count back into the surviving source stack and destroys the undeliverable split item, so a failed split move is lossless rather than leaving an orphaned `Nowhere` item. `ChangeItemSlot()` swap notification still attempts the second `OnCritterItemMoved` after the displaced-item event, even if that handler moves or destroys the original moving item; redundant or stale notifications are handled by the event path and final item ownership. Map-item add, visibility, and property broadcasts snapshot the item's map/hex context; if `OnItemOnMapAppeared`, `OnItemOnMapDisappeared`, or `OnItemOnMapChanged` moves, destroys, or otherwise detaches the item from that context, the outer broadcast stops before notifying more observers or spectators. Removing an item from a holder fires events after the item has already been detached, so handlers can destroy that detached item, but ordinary script movement APIs require a current holder and do not move `Nowhere` items.

Walk trigger processing is scoped to the critter's current trigger context. If `OnStaticItemWalk` or an item's `OnCritterWalk` moves, transfers, destroys, or otherwise detaches the critter from that context, `VerifyTrigger()` stops processing the remaining triggers from the old map/hex.

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

## Network validation and inbound messages

The server receives client messages through `ServerConnection` and dispatches them from `ServerEngine` methods such as:

- `Process_Handshake()`
- `Process_Ping()`
- `Process_Move()`
- `Process_StopMove()`
- `Process_Dir()`
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

Client movement requests enter through `Process_Move()`, `Process_StopMove()`, and `Process_Dir()`. The server validates the request, applies script events such as `OnPlayerMoveCritter` and `OnPlayerDirCritter`, then updates the authoritative `Critter` and broadcasts the resulting state. Stop-move packets include the client's current hex and hex offset; the server normalizes that pair to a canonical in-bounds hex/offset, reconciles positions that lie on the critter's current authoritative `MovingContext` path, and allows a small pathfinding-validated correction for rapid start/stop input that stopped between path centers. This lets client and server converge without accepting arbitrary stop teleports.

`Process_StopMove()` also fires `OnPlayerDirCritter` during stop reconciliation, before it can stop the active `MovingContext`. Scripts may hard-disconnect the connection, detach or switch the player's controlled critter, or move the critter to another map; the native continuation revalidates those possible outcomes before applying the final stop to avoid completing a stale client command.

Server scripts can call `Player.RefreshCritterMoving(cr)` to resend the authoritative movement snapshot for a critter on the player's current map. Moving critters are sent as `CritterMove`; stationary critters are sent as `CritterPos`, which lets the client stop prediction and apply the server hex, hex offset, and direction without inventing a project-specific correction packet.

Runtime movement is independent of `CritterCondition`: alive, knockout, dead, and any future condition use the same `MovingContext` processing. Game scripts own gameplay-level movement permissions and must stop or reject movement when a creature state should forbid it. Attached critters still stop their active `MovingContext` because attachment is a transport/ownership relationship rather than a condition.

Server-side movement helpers include:

- `StartCritterMoving()` overloads for an existing `MovingContext` or raw path data;
- `StopCritterMoving()`;
- `ChangeCritterMovingSpeed()`;
- `CritterMovingJob()` as the self-rescheduling worker-pool body for active movement;
- `ProcessCritterMovingBySteps()`.

`Source/Tests/Test_ServerEngine.cpp` includes overdue movement tests that verify route completion, condition-independent movement processing, and blocked-hex stopping behavior. Coordinate/pathfinding mechanics remain documented in [MapsMovementGeometry.md](MapsMovementGeometry.md).

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
