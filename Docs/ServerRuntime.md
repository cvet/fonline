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

**Sync-free server→client broadcasts (the "rassylka").** A broadcast to observers/spectators is the one place where a sender legitimately cannot hold the recipient's cover — the fan-out runs under the **subject** critter's (or map's) cover only, then must reach every recipient's player. The whole broadcast surface is sync-free: property, movement (`Send_Moving`/`Send_MovingSpeed`), facing (`Send_Dir`), action (`Send_Action`), inventory move (`Send_MoveItem`), teleport (`Send_Teleport`), and attachments (`Send_Attachments`), plus their serialization helpers `SendItem`/`SendInnerEntities`/`SendCritterMoving`. The pattern:

- **Recipient sends validate the SUBJECT, not the recipient — and `this` always carries its own explicit marker.** Two independent decisions, both stated at the top of every send: (1) the **`this`-marker** declares how the method treats its own entity — a recipient send writes only the recipient's connection and never reads recipient state, so its `this`-marker is `FO_NO_VALIDATE_ENTITY_ACCESS()`. This marker is **mandatory and is not implied by the value check** — `FO_VALIDATE_ENTITY_ACCESS_VALUE(x)` validates `x`, it is *not* a `this`-decision, so every send pairs the two: `FO_NO_VALIDATE_ENTITY_ACCESS();` then `FO_VALIDATE_ENTITY_ACCESS_VALUE(subject);`. (2) The **subject validation**: every send is handed an entity it serializes or reads — even just its id — and validates it via `FO_VALIDATE_ENTITY_ACCESS_VALUE(subject)` (= the null-tolerant throwing `ValidateEntityAccess(subject)`). The subject *must* be in sync, and the broadcaster holds its cover for the whole fan-out so the check passes — an uncovered subject is caught (a `ScriptException` reported at the job/script frontier, which continues; escaping a `noexcept` send still terminates the process). This is **intentionally aggressive diagnostic validation**: validate every sent entity to surface every desync immediately (this validation layer is temporary and will be removed after the multithreaded logic system stabilizes; see the TODO below). The recipient connection is guarded by a per-player `_connectionLock` so a concurrent reconnect `SwapConnection` cannot swap `_connection` mid-write. It is a **plain `mutex`, not a `shared_mutex`**: same-player sends already serialize on the connection's own single output-buffer lock (`ServerConnection::_outBufLocker`, held by the `OutBufAccessor` for the whole `WriteMsg`), so a shared "many concurrent send readers" lock would buy nothing — and `mutex::lock()` is cheaper on the hot path than `shared_mutex::lock_shared()`. Cross-player concurrency (the actual win) comes from each player owning its own lock; sends and `SwapConnection` both take it exclusively, and no send re-enters it (the `SendItem`/`SendInnerEntities`/`SendCritterMoving` helpers take the already-opened buffer as a parameter, so a non-recursive mutex cannot self-deadlock). `is_chosen` is a lock-free atomic identity compare against `Player::_controlledCr` (no deref). Only sends that are handed **no entity at all** — `Send_TimeSync`/`Send_InfoMessage`/`Send_PlaceToGameComplete` (no entity), `Send_HashList` (bare strings; used by the reported-hash broadcast fan-out and the handshake-time full-list push), `Send_RemoteCall` (a name + opaque payload; the outbound remote-call channel — the recipient may be uncovered and mid-reconnect, so the send pins the live connection under `_connectionLock`), `Send_Ping`/`Send_HandshakeAnswer`/`Send_InitData`/`Send_UpdateFileData` (connection-stage protocol replies, isolated in `Player` so no code outside `Player` writes a player-directed `NetMessage`; `Send_HandshakeAnswer` also installs the out-buffer encrypt key under the same lock hold), `Send_RemoveCustomEntity` (a bare `ident_t`), and `Send_SomeItems` (a span — each item is validated downstream in `SendItem`) — are pure `FO_NO_VALIDATE` with no value check. (The validation gap the subject check closes: `StoreData`/`GetRawData` do **not** validate entity access, so a send serializing a subject through them must validate the subject explicitly.) The `Critter::Send_*` forwarders (per-critter "send to my own player") follow the same two-marker shape: `FO_NO_VALIDATE_ENTITY_ACCESS()` for the *recipient* critter (`this`) plus `FO_VALIDATE_ENTITY_ACCESS_VALUE(subject)` for the forwarded subject. They must **not** validate the recipient critter (the old `this`-check fired spuriously on an uncovered NPC group member with no player during `DestroyCritter` cleanup — the subject being removed is covered, only the recipient was not), and they read `_player` atomically before forwarding.
- **Fan-outs resolve a refcount-pinned recipient set.** `Critter::Broadcast_*` / `SendAndBroadcast_*` and the generic `SendAndBroadcast(ignore_player, player_callback)` build `Critter::GetBroadcastRecipients(ignore_player)` under the subject cover: each observer's player via `Critter::GetPlayerForSend()` (the no-validate `TryAddRef`-pinned accessor mirroring `ServerEntity::GetParentRaw`) plus map spectators via `Map::GetSpectatorPlayersForSend()` (a `FO_NO_VALIDATE` snapshot guarded by the map's `_spectatorLock` `shared_mutex`, so the broadcaster needs neither the observer's nor the **map's** cover). The pinned `vector<refcount_ptr<Player>>` keeps every recipient alive through the lock-free dispatch (`GetBroadcastRecipients`/`GetMapSpectators`/`GetSpectatorPlayersForSend` return an owning `refcount_ptr` vector; `ref_hold_vector` is reserved for the transient `copy_hold_ref(...)` loop-helper use).
- **Property broadcasts read the subject live.** Every property broadcast trigger fans out the ordinary `Player::Send_Property(type, prop, subject)` to each pinned recipient: it validates the **subject** (`FO_VALIDATE_ENTITY_ACCESS_VALUE`), reads the subject's serialized bytes live via `Properties::GetRawData` (safe because the broadcaster holds the subject's cover for the whole fan-out), and writes only the recipient's connection under `_connectionLock`. The triggers: critter/critter-item (`Critter::Broadcast_Property`), global (`OnSendGlobalValue` → all players), map (`Map::SendProperty` Map case → map critters + spectators), location (`OnSendLocationValue` → `Map::SendProperty` Location case for each map in the location → map critters + spectators), and custom entity (`OnSendCustomEntityValue` → viewers resolved by the covered `ForEachCustomEntityView`, then dispatched lock-free). (A byte-snapshot optimization — capturing the payload once under the subject's cover and blitting it lock-free — was prototyped and reverted; the live-read fan-out is the current shape.)
- **TSA-guarded.** Both fine-grained locks are Clang [Thread Safety Analysis](ThreadSafetyAnalysis.md) capabilities (`fo::mutex` / `fo::shared_mutex`), and the state they guard carries `FO_TSA_GUARDED_BY`: `Player::_connection FO_TSA_GUARDED_BY(_connectionLock)` and `Map::_spectatorPlayers FO_TSA_GUARDED_BY(_spectatorLock)` (each lock declared before the field it guards). Every lock-free path holds the lock via `scoped_lock`/`shared_lock`, so TSA statically enforces the guard on exactly the threads that lack the entity cover. The accessors that legitimately reach the guarded state under the **entity cover** instead — the cooperative scheme TSA cannot model, and which also excludes the swap/mutation — are `FO_TSA_NO_ANALYSIS` with a comment: `Player::GetConnection` (hands the pointer to entity-cover callers), `Player::SwapConnection` (cross-object `other->_connection` swap), `Map::HasSpectatorPlayers` / `Map::GetSpectatorPlayers` (leak a span), and the single-threaded `~Map` teardown invariant. The `Map::AddItem` / `RemoveItem` / `SendProperty` (MapItem) spectator legs route through `GetSpectatorPlayersForSend()` (which takes the shared lock) rather than touching `_spectatorPlayers` directly, so they stay TSA-clean without an escape hatch.

This is why `Critter::_player`, `Player::_controlledCr`, and `Player::_sendIgnoreEntity/_sendIgnoreProperty` are atomics published under their owner's cover — the broadcaster reads them without the recipient's cover. Message ordering survives because recipient resolution stays under the subject cover (the visibility grant `MapManager::ProcessVisibleCritters` inserts an observer into the reverse-visible set and the broadcast reads that set both under the subject cover, so a delta can never be enqueued ahead of its AddCritter; AddCritter ships a full snapshot, so even a reordered client message self-heals — the client decoder drops an unknown-entity message rather than faulting).

**Covered-by-design exceptions (intentionally NOT sync-free).** Sends that read the **recipient's own** state stay validated, because that state needs the recipient's cover by construction — they are single-recipient sends issued under that cover, not broadcast distribution: `Send_LoginSuccess` (serializes the recipient itself), `Send_ViewMap` (reads the recipient's own `_viewMap`), `Send_AddCritter` (reads the recipient's controlled-critter visibility mode; sent from the visibility grant / map-load / transfer, which hold the recipient's cover — convertible only by threading the grant-computed vis-mode through its call sites). Every other `Player::Send_*` is **recipient-lock-free** (`this`-marker `FO_NO_VALIDATE_ENTITY_ACCESS()` — the recipient is never validated): every one that is handed an entity validates that **subject** via `FO_VALIDATE_ENTITY_ACCESS_VALUE`, including the ones that read only the subject's id (`Send_RemoveCritter`/`Send_CritterVisibilityMode`/`Send_RemoveItemFromMap`/`Send_ChosenRemoveItem`/`Send_Teleport`) alongside the ones that serialize it (`Send_Property`/`Send_Moving`/`Send_MovingSpeed`/`Send_Dir`/`Send_Action`/`Send_MoveItem`/`Send_Attachments`/`Send_LoadMap`/`Send_AddItemOnMap`/`Send_ChosenAddItem`/`Send_AddCustomEntity` and the `SendItem`/`SendInnerEntities`/`SendCritterMoving` helpers). Only the sends handed **no entity** are pure `FO_NO_VALIDATE` with no value check: `Send_RemoveCustomEntity` (a bare `ident_t`), `Send_InfoMessage`, `Send_PlaceToGameComplete`, `Send_TimeSync`, and `Send_SomeItems` (a span — each item is validated in `SendItem`). `Map::SendProperty`'s **MapItem** case and the `Map::AddItem`/`RemoveItem` item-appearance loops are also covered-by-design: not pure fan-outs but per-critter notify-and-react loops (`AddVisibleItem`/`RemoveVisibleItem` + a re-entrant `OnItemOnMap*` event with an early-return on item-context change), which require each critter covered; their spectator legs are lock-free. The critter destructor's teardown-invariant diagnostics likewise read only raw members (no validating accessor) so a refcount-driven `~Critter` on a worker thread outside the critter's cover cannot fault.

Manual server-side entity methods that read or mutate their own entity state declare the LOCKED precondition with `FO_VALIDATE_ENTITY(LOCKED, ...)` at method entry. It expands to the regular throwing validator (`ValidateEntityAccess(this)`), which **throws a recoverable `ScriptException`** on an uncovered access — at the job/script frontier the violation is reported and the job continues, so a sync-scope bug no longer takes the whole server down. An exception escaping a `noexcept`-declared method still terminates the process, so violations inside noexcept accessors remain fatal; the frontier-reachable throwing surface recovers. Generated C++ property accessors (`GetX()`, `SetX()`, `IsNonEmptyX()`) validate the owning entity through `FO_VALIDATE_ENTITY_ACCESS_VALUE(entity)` (the same throwing form), which currently resolves the entity from `Properties::GetEntity()` before touching property storage; low-level raw `Properties` access remains reserved for serialization, loading, tooling, and other paths that already establish their own storage-access contract. These validation macros live in `Common.h` so the temporary stabilization checks can be removed or compiled out from one location later. The check is intentionally always-on while the multithreaded logic system is being stabilized. Methods used by the validator, persistence callbacks, and lock machinery itself are marked with `FO_NO_VALIDATE_ENTITY_ACCESS()` as explicit unchecked escape hatches: constructors/destructors, `GetId()`, `GetName()`, `IsPersistent()`, `GetEntityLock()`, raw parent access, `GetSyncWidenEntity()`, and low-level parent/lock lifecycle setters must not recursively validate while the access checker is trying to produce diagnostics, handle persistence hooks, or prove the current lock cover.

Two ordering contracts follow from this model. **(1) Script-export functions validate an entity argument before its first property read.** Property accessors are `noexcept`, so the throwing validator inside one cannot be recovered — an uncovered read there terminates the process. Every `FO_SCRIPT_API` function that reads a property of an entity argument therefore calls `ValidateEntityAccess(arg)` first, converting a caller's sync-scope violation into the recoverable frontier `ScriptException`; the `Server_Game_DestroyCritter(s)` family is the reference pattern. **(2) Manager create/destroy entry points self-sync the container/holder they touch.** Entity registration pulls the fresh entity into the current context (`EnsureEntitySynced`), which restricts a previously-empty context — and a child's lock never covers its parent. Any manager path that then touches the container would fail its own validation, so the container is pulled in up front: `CreateCritterOnMap()` / `ItemManager::CreateItemOnHex()` sync the destination map before creating, `CritterManager::DestroyCritter()` syncs the source map, and `ItemManager::DestroyItem()` syncs the item's holder before tearing ownership off. Callers already holding the proper cover make these a no-op; callers under a foreign restricted cover get the recoverable contract throw.

TODO: remove the entire entity-access validation system after multithreaded logic stabilization; it is a high-overhead diagnostic layer, not a permanent runtime safety mechanism. If any `FO_VALIDATE_ENTITY_ACCESS*` check fires, fix the owning top-level path (job dispatch, script entry, sync widening, entity creation/registration, or holder transfer) so the entity cover is acquired before the code can reach the checked access at all. Do not treat the checked method or property accessor as the synchronization boundary unless that method is itself the top-level entry point.

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

`ProcessPlayer()` drains up to `MaxMessagesPerProcessPass` buffered messages per job pass in a loop, with the player synced once by `PlayerJob()` before the loop. Script reached from a message handler never runs in the job's **primary** SyncContext: event callbacks are dispatched under per-callback nested contexts, and `Process_RemoteCall()` likewise wraps the script handler in its own nested `ScopedSyncContext`. A script `Sync::Lock(...)` replaces the held lock set of the *current* context with exactly the requested entities, so under the nested layer it can no longer drop the primary's player cover — the player stays locked across every buffered message. The engine-side handlers that re-sync the primary themselves (`Process_Move()` / `Process_StopMove()` / `Process_Dir()`) always include the player in the requested set, and `Process_Property()` does not touch the held set at all. The drain loop still re-checks `player->IsDestroyed()` each iteration because handler script may legitimately destroy the player. The invariant matters because a later message's first player access goes through a `noexcept` accessor (e.g. `Player::GetConnection()` in `Process_RemoteCall()`): an uncovered access there would trip the access-without-sync invariant and terminate the process, since a throw cannot escape the `noexcept` frontier to be recovered at the job boundary.

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
