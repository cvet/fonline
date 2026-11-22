---
layout: default
title: Client Runtime
locale: en
document_id: client-runtime
permalink: /Docs/en/explanation/runtime/client.html
---

# Client Runtime

> Engine-owned documentation. This page describes reusable client runtime behavior in `Source/Client/`; game UI policy, gameplay rules, and concrete content remain in the embedding project.

## Purpose

The client runtime combines baked resources, server state, input, and scripts into the game view. Projects own game design and drive the reusable runtime through scripts, configuration, resources, and server messages.

Read this page together with:

- [Entity Model](../entity-and-property-model/) for the entity/property/prototype model that client views wrap.
- [Maps, Movement, and Geometry](../maps-and-movement.md) for map positions, path finding, line tracing, and movement contexts.
- [Image And Sprite Formats](../../how-to/content/image-format.md) for image import, FOFRM composition, baked sprite loading, atlases, and cache behavior.
- [Font Formats And Text Layout](../../how-to/content/font-format.md) for bitmap-font descriptors, slot binding, bind-time scaling, text layout, rendering flags, and inline colors.
- [Sprite Root Motion](../../how-to/content/sprite-root-motion.md) for 2D per-frame offsets and movement-driven walk/run cycle alignment.
- [Video.md](../../../Video.md) for experimental Ogg/Theora resources, fullscreen and embedded playback, memory, and visible validation.
- [Networking](../authority-and-networking/) for command buffers, transports, and property sync.
- [Frontend and Rendering](../rendering/) for platform windows, input, audio, and renderer backends.
- [Web Build, Packaging, and Browser Debugging](../../how-to/platforms/web-debugging.md), [Android Build, Packaging, and Device Debugging](../../how-to/platforms/android-debugging.md), and [Native, AngelScript, and Managed C# Debugging](../../troubleshooting/debugging.md) for platform-specific validation flows.

## Source paths inspected

- `Source/Client/Client.h`
- `Source/Client/Client.cpp`
- `Source/Client/ClientConnection.h`
- `Source/Client/ClientConnection.cpp`
- `Source/Client/ResourceManager.h`
- `Source/Client/ResourceManager.cpp`
- `Source/Client/FontManager.h`
- `Source/Client/FontManager.cpp`
- `Source/Client/MapView.h`
- `Source/Client/MapView.cpp`
- `Source/Scripting/ClientMapScriptMethods.cpp`
- `Source/Scripting/ClientGlobalScriptMethods.cpp`
- `Source/Scripting/Managed/ManagedScriptBackend.cpp`
- `Source/Client/CritterView.h`
- `Source/Client/CritterHexView.h`
- `Source/Client/ItemView.h`
- `Source/Client/ItemHexView.h`
- `Source/Client/LocationView.h`
- `Source/Client/PlayerView.h`
- `Source/Client/DefaultSprites.h`
- `Source/Client/DefaultSprites.cpp`
- `Source/Client/SpriteManager.h`
- `Source/Client/SpriteManager.cpp`
- `Source/Client/TextureAtlas.h`
- `Source/Client/TextureAtlas.cpp`
- `Source/Client/ModelAnimation.h`
- `Source/Client/ModelAnimation.cpp`
- `Source/Client/ModelBakedData.h`
- `Source/Client/ModelBakedData.cpp`
- `Source/Client/ModelManager.h`
- `Source/Client/ModelManager.cpp`
- `Source/Client/ModelInstance.h`
- `Source/Client/ModelInstance.cpp`
- `Source/Client/ModelInformation.h`
- `Source/Client/ModelInformation.cpp`
- `Source/Client/ModelHierarchy.h`
- `Source/Client/ModelHierarchy.cpp`
- `Source/Client/ModelSprites.h`
- `Source/Client/ModelSprites.cpp`
- `Source/Client/ModelSpriteLayout.h`
- `Source/Client/ModelSpriteLayout.cpp`
- `Source/Common/AnimationInfo.h`
- `Source/Common/AnimationInfo.cpp`
- `Source/Common/ModelBounds.h`
- `Source/Common/ModelBounds.cpp`
- `Source/Client/ParticleSprites.h`
- `Source/Client/ParticleSprites.cpp`
- `Source/Client/RenderTarget.h`
- `Source/Client/RenderTarget.cpp`
- `Source/Tests/Test_ClientEngine.cpp`
- `Source/Tests/Test_AngelScriptClientEngine.cpp`
- `Source/Tests/Test_ClientRuntimeApi.cpp`
- `Source/Tests/Test_ClientDataValidation.cpp`
- `Source/Tests/Test_ClientServerIntegration.cpp`
- `Source/Tests/Test_ModelBaker.cpp`
- `Source/Tests/Test_ModelAnimationRuntime.cpp`

## Runtime owner: `ClientEngine`

`ClientEngine` in `Source/Client/Client.h` derives from `BaseEngine` and `AnimationResolver`, owns the client lifecycle, and exposes client-script event hooks.

Major responsibilities:

- initialize and shut down the client runtime;
- run the per-frame `MainLoop()`;
- process application input events;
- connect and disconnect from a server through `ClientConnection`;
- create, register, unregister, and look up client-side entities by id;
- receive and apply network messages for critters, items, maps, custom entities, time sync, movement, actions, and properties;
- own client-facing managers such as sprites, effects, fonts, sounds, video playback, resources, cache storage, and render targets;
- raise engine events such as `OnStart`, `OnLoop`, `OnConnected`, `OnDisconnected`, render-map stages, input events, entity in/out events, and map load/unload events.

`ClientEngine` composes Common-layer data (`Entity`, properties, prototypes, networking buffers), Frontend services (`Application`, render, input, audio), and game scripts. [Audio.md](../../../Audio.md) and [Video.md](../../../Video.md) own game-facing media contracts.

## Client lifecycle

A typical client lifetime has these phases:

1. **Application initialization** happens in the frontend layer. `Application` owns the main window, renderer, input, and audio. See [Frontend and Rendering](../rendering/).
2. **Resource filesystem selection** starts through `GetClientResources(const ClientSettings&)` in `Source/Client/Client.cpp`, which builds the client-side `FileSystem` view used by runtime managers and by the updater's local metadata-version check. Installed clients add a higher-priority writable resource overlay above the read-only base resources; see [Configuration and Data Sources](../../reference/settings/configuration-and-data-sources.md) and [Client Updater](client-updater.md).
3. **Engine construction** wires settings, resources, the main application window, generated metadata, script modules, and client managers.
4. **`OnStart`/script initialization** gives scripts their first client-side entry point. `Source/Tests/Test_AngelScriptClientEngine.cpp` validates that script module init and loop calls are callable on a self-contained client runtime.
5. **The main loop** processes frontend input, network packets, scripted loop callbacks, visual effects, video playback, map processing, and rendering-facing updates.
6. **Network connection** starts with `Connect()`, which delegates transport setup and handshake work to `ClientConnection`.
7. **Map and entity state** arrive through network messages, are represented as client view entities, and are updated through property sync and movement/action packets.
8. **Shutdown** first closes and drains resource preparation, then disconnects networking, destroys inner entities, clears caches and render targets, and releases frontend resources.

Startup/shutdown changes must keep script events, manager lifetime, entity registration, and network callbacks consistent.

`ClientSessionMarker` is a best-effort diagnostic in the writable client root, shared by host and runtime. It records build/start time, process identity (`Pid` plus `ProcessStart`), and the last stage: `MainLoopExited`, `ClientStopped`, `ApplicationReset`, `ShutdownHookDone`, `GlobalDataTeardown` (including the active global-data set), `RuntimeReturned`, then `ExitRequested`. Numeric stage meanings are historical, not this sequence. The marker survives process exit because DLL detach/runtime callbacks may hang. The next launch silently consumes `ExitRequested` only after the exact process has gone; otherwise it reports a running predecessor or unclean exit. Unknown/malformed stages or missing identity cannot prove clean exit. Writes verify identity so an older instance cannot complete a newer marker. This is no registry or cross-process lock. Windows polls the process handle; exit code `259` does not prove liveness.

## Server connection and message dispatch

`ClientConnection` (`Source/Client/ClientConnection.h`, `Source/Client/ClientConnection.cpp`) owns transport state, abstracting interthread, TCP, and UDP-capable sockets.

`ClientNetwork.RequireInterthreadTransport = True` requires an in-process listener on `Network.ServerPort`: absence immediately fails `Connect()` with `ConnectResult::Failed` without trying TCP, UDP, or WebSocket. An available listener wins even with `ClientNetwork.UseUdp` enabled. Default false retains ordinary selection/fallback. This client opt-in serves embedded clients; server admission/authentication remain independent.

Important responsibilities:

- `Connect()` selects the connection mode from `ClientNetworkSettings` and creates a `NetworkClientConnection`;
- `CreateNetworkConnection(bool use_udp)` selects socket or UDP socket transport;
- `Process()` advances connect, receive, dispatch, and send work;
- `Disconnect()` tears down active transport and raises the configured disconnect handler;
- `TryFallbackToTcp()` provides a fallback path while the connection is still being established, from the UDP connect until the server answers the handshake. A transport failure after that answer ends the session through the disconnect handler: a TCP connection would be a new server session, and the state built on the first one cannot continue on it;
- `Net_SendHandshake()`, `Net_OnHandshakeAnswer()`, and `Net_OnPing()` handle connection-level protocol messages;
- `AddMessageHandler(NetMessage, MessageCallback)` binds protocol messages to `ClientEngine::Net_On...` handlers.

`ClientEngine` owns the semantic handlers. Examples include `Net_OnInitData`, `Net_OnAddCritter`, `Net_OnRemoveCritter`, `Net_OnProperty`, `Net_OnLoadMap`, `Net_OnSomeItems`, `Net_OnRemoteCall`, `Net_OnAddCustomEntity`, and `Net_OnRemoveCustomEntity`.

On ordinary connection failure, `Net_OnConnect()` clears the pending request before `OnConnectingFailed`. There is no per-frame retry: a script/user action must explicitly request `Game.Connect()` again.

For protocol format details, use [Networking](../authority-and-networking/). For client/server handshake validation, see `Source/Tests/Test_ClientServerIntegration.cpp`, especially `ClientAndServerHandshakeOverInterthreadTransport`.

Each main-loop pass processes a snapshot of `ScheduleDelayedCallback()` continuations already due at its start. Newly scheduled zero-delay callbacks, including `Yield(0)`, resume next pass instead of re-entering, so script wait loops cannot starve network/input ticks.

## Entity and view model

Client objects are views combining Common entity data with client-only rendering, input, and presentation state, rather than raw server entities.

Primary view types:

- `PlayerView` — client representation of the current player entity.
- `LocationView` — client-side location entity.
- `MapView` — map entity plus map rendering, local spatial indexes, fog, lighting, scrolling, zoom, and hit testing.
- `CritterView` — critter entity visible outside the current map context.
- `CritterHexView` — critter placed on a map hex, with movement, direction, and map rendering behavior.
- `ItemView` — item entity visible in inventory or generic client contexts.
- `ItemHexView` — item placed on a map hex, with map flags, sprite placement, blockers, and lighting implications.
- custom client entities — created by `CreateCustomEntityView()` when a synced custom entity entry arrives.

`ClientEngine::RegisterEntity()` and `ClientEngine::UnregisterEntity()` maintain network/script id lookup; `Source/Tests/Test_ClientEngine.cpp` checks registration and removal.

`Item.Ownership` is unsynchronized. `ClientEngine::ReceiveDetachedItem()` restores snapshots outside an owned holder and assigns `ItemOwnership::Nowhere` for item lists, action context, and slot moves. Snapshots must never retain the zero default (`MapHex`): it would make presentation/actions treat another inventory's item as lying on the current map. Lists require nonzero entity ids because callers retain/address their items. Action-context snapshots may represent immutable static map items with empty ids; `ReceiveDetachedItem()` accepts that generic state, while the list handler enforces its narrower identity requirement.

Authoritative inventory slot reconciliation updates the moved item and exchanges
the occupied slot with a peer when the destination is exclusive.
Ordinary moves, prediction echoes and multi-item destinations preserve their
existing semantics (`Test_ClientEntityLifetime.cpp`).

Script GUI `ItemView` widgets cache the item handles bound to their cells. `Resort()` drops destroyed handles returned by a stale supplier, keeps a cell only when the source returns the same live handle instance, and rebinds a replacement clone with the same entity id so item draw callbacks observe its current count and other projected properties.

## 3D model runtime architecture

The removed `3dAnimation` / `3dStuff` umbrellas are replaced in the client 3D path by same-named `Model*.h` / `Model*.cpp` pairs, each owning one boundary. No `ModelMesh` utility exists: mesh types belong to `ModelHierarchy`. No separate `ModelPoseRuntime` exists: renderer-independent pose/link operations belong to `ModelAnimation`.

| Module | Responsibility |
| --- | --- |
| `ModelManager` | Runtime entry point, model-description and hierarchy caches, baked mesh loading, and construction of model instances. |
| `ModelHierarchy` | Shared loaded `ModelBone` topology, mesh/bind data, model textures/effects, and the mesh representation types used by hierarchy and instances. It does not own mutable animation pose output. |
| `ModelInformation` | One resolved baked `.fo3d` description: hierarchy reference, canonical/runtime joint identities, animation lookup tables, cuts/links, draw metadata, and one immutable `ModelAnimationRuntimeRig`. |
| `ModelInstance` | All mutable per-instance state: controller timelines, runtime pose, world-matrix snapshots, mesh state and batching, cuts, attachments, particles, procedural transforms, projection, and draw submission. |
| `ModelAnimation` | Engine animation-controller timeline, transition, callback, reverse/freeze/play-once, and binding semantics together with the engine-owned clip/rig/pose contract, Ozz-backed sampling and blending behind PImpl, canonical-joint mapping, linked-pose resolution, and validated rest-pose matrix construction for direct raw models. |
| `ModelBakedData` | Small defensive reader helpers shared by client model loaders, especially count-versus-unread-data preflight before allocation. |
| `ModelSprites` | Adapter from `ModelManager` / `ModelInstance` to atlas-backed and direct-scene sprite rendering. |

The ownership direction is deliberate:

- `ModelManager` caches shared `ModelHierarchy` and `ModelInformation` objects;
- every created `ModelInstance` references one `ModelInformation` but owns its
  mutable controllers, pose buffers, matrices, mesh overrides, and children;
- shared `ModelBone` nodes retain topology, rest/bind, and drawable data only;
  pose results never flow back into the shared hierarchy;
- canonical animation joints may exist without a physical `ModelBone`, so
  canonical indexing and cross-model pose links belong to
  `ModelAnimation`, not `ModelHierarchy`.

Callers include only the manager, information, hierarchy, instance, animation, or baked-data contract they consume. Passive types remain with their owner rather than creating empty translation units; the include-everything dependency is gone.

`ModelAnimationRuntimeClip`, `ModelAnimationRuntimeRig`, and `ModelAnimationRuntimePose` hide backend state behind PImpl. Public headers expose engine runtime values/spans; `ModelAnimation.cpp` holds Ozz headers, archives, sampling contexts, and matrices. `ModelAnimationData` owns the versioned native wire contract shared with the baker; `ModelAnimationConverter` owns offline conversion. Ozz implements the Engine's native runtime/baked format, not a public interchangeable-backend API.

## Critter model animation

[Model Format](../../how-to/content/model-format.md) owns the complete `.fo3d` loading/composition contract: source meshes, layers, root modifiers, child models, particles, transforms, materials, cuts, and rendering controls.

3D critters have separate body/action and movement controllers. `ModelInstance::PlayAnim()` applies `AnimSpeed` to body/action; `RefreshMoveAnimation()` assigns gait/movement-speed scaling to the movement track. With both active, movement advances at model base/link/global speed, without the body's `AnimSpeed`; otherwise fast use/pick-up actions would accelerate moving legs.

The `~` animation-name prefix reverses source playback: time `t` samples `duration - fmod(t, duration)`, restarting exact loop boundaries at the clip end. Without interpolation, nearest-key selection uses that reversed source time.

Every baked `.fo3d` requires a versioned `LFMODINF` schema-5 header (`MODEL_DESCRIPTION_SCHEMA_VERSION`) and schema-1 `LFOZZRIG` payload for pinned `ozz-animation` 0.16.0. `ModelInformation` strictly owns/loads one immutable `ModelAnimationRuntimeRig`, whose PImpl holds the canonical Ozz skeleton, unique clips, base/clip remaps, presence masks, nearest timelines, and resolved state/action bindings. Old unversioned, partial, or inconsistent rigs fail; Ozz errors never fall back to other payloads. The historical mesh-only wire transition used compatibility marker `0.0.30` and requires a full rebake.

Ozz evaluates production animated poses; body/movement controllers advance only timelines/events. Registered animations hold direct Ozz clip index/duration/reverse metadata and immutable bound-joint sets derived from clip presence masks. Binding requires matching canonical/exact runtime names, excluding a resource-renamed model root from source-root animation. Per-track allowed-joint and transition-suppression masks further filter bindings before tracks feed the instance pose. Ozz samples clips, blends bodies, replaces movement-only joints, applies procedural body/head pre-rotation, and evaluates local-to-model transforms. Each `ModelInstance` snapshots world matrices for skin palettes, linked children, particles, and bone queries. Link-all attachments override matched joints after evaluation without recomputing descendants, preserving attachment order.

Canonical names, exact runtime names, and name/index bindings are independent of physical `ModelBone` topology. Base joints retain read-only physical bones for meshes/cuts; animation-contributed joints have no `ModelBone` and never enter the shared hierarchy. Base roots retain resource-path runtime aliases; contributed joints use canonical names, preserving legacy exact lookup/root-animation suppression. Particles, attachment resolution, link-all matching, and bone-position queries use canonical indices, so link-all supports contributed joints without physical bones. Authored one-bone links remain base-hierarchy-only in the current baking schema.

Static `.fo3d` instances evaluate empty Ozz track sets, sharing animated-instance canonical rest/procedural body/head transforms. Only direct raw models bypass Ozz and build parent-ordered world matrices through `ModelAnimation` validation helpers.

`Game.DrawCritter3d(...)` and `Game.GetDrawCritter3dBounds(...)` support GUI model layout. After a draw, the query returns two draw-anchor-relative rectangles, or `false` if no valid model sprite exists. `drawRect` covers the selected animation's full cycle/continuous facing range and projected shadow; `viewRect` is the stable logical model/layers rectangle for names and coarse picking. GUI previews fit/centre it; world overlays anchor to it without duplicating 3D projection or following atlas crops. The old custom evaluator/shared mutable matrix table are gone. Baked meshes require `LFMODMSH` schema-1 headers and only recursive hierarchy/bind/drawable mesh data. Clients consume exactly that payload, rejecting headerless, mismatched, truncated, or trailing data; no serialized TRS tail or legacy fallback remains. Only the baked Ozz rig supplies clip identity, duration, joint presence, and sampling.

The nested LF archive hash detects corruption, not authenticity. Ozz deserialization trusts baked packs; deployments allowing attacker-rewritable packs must authenticate them before loading.

## Sprite loading and atlases

Stock clients do not decode source PNG/TGA or legacy image bytes. `ResourceManager` lowercases baked extensions, dispatches registered `SpriteFactory` objects, and caches copyable sprites by path/`AtlasType`. `DefaultSpriteFactory` reads the private `ImageBaker` container into static `AtlasSprite` or animated/directional `SpriteSheet` objects. RGBA frames upload with duplicated one-pixel filter borders and alpha hit masks. Missing paths and unknown/failing factories are separately memoized across routine cleanup; ForgetFailedSprite or InvalidateSpriteResource permits retry.

[Image And Sprite Formats](../../how-to/content/image-format.md) owns source formats, FOFRM, baked records, default extensions, atlases, and caching. Projects must use the pinned baker for supported sources rather than parse private baked streams.

Particles use a separate neutral factory. `ParticleManager` chooses enabled SPARK/Effekseer backends by baked `.spk` / `.efk` extensions, caches immutable backend data/failed loads by exact path, and creates independent instance simulations. `ParticleSpriteFactory` frames sprites from mandatory baked bounds, choosing atlas-backed or direct map-scene rendering; bone attachments use 3D composition. [Particle Format And Runtime](../../how-to/content/particle-format.md) owns authoring, baking, rendering, caching, tools, scripts, and validation.

## 2D sprite root motion

For moving 2D critters, `MovingContext` owns logical path/hex progress. `CritterHexView` projects integer-pixel displacement onto accumulated walk/run `Motion` cycles, deriving frames and `_offsAnim` without changing authoritative movement. Direction-specific sheets preserve wrapped phase; start/stop resets the anchor. [Sprite Root Motion](../../how-to/content/sprite-root-motion.md) owns the data/math contract.

## `MapView`: map presentation and local spatial state

`MapView`, the largest client view, bridges these subsystems:

- map/static-data loading through `LoadFromFile()` and `LoadStaticData()`;
- one-way per-instance static item removal: `LoadStaticData()` skips removals, `ApplyStaticItemRemovals()` applies them live from `RemovedStaticItemIds`; loaded maps never reverse removal — see [Maps and Movement](../maps-and-movement.md#static-item-removal);
- processing through `Process()`;
- drawing through `DrawMap()` and `ClientEngine` render stages;
- item/critter field indexes;
- picking active map/indoor-mask sprites: drawables retain their `ItemHexView` owners, hidden primaries are skipped, eligible extra/multihex sprites remain selectable. Move/destruction updates the active set directly, excluding stale padded viewport cells;
- coordinates, zoom, scrolling, screen/map conversions;
- pathfinding/cutting through `PathFinding::FindPath()`;
- bullet/light tracing: `MapView::ApplyLightFan` traces each light's full hex `Distance` (>= 1), passing falloff through `PrimitivePoint::EggData` to attribute slot 3 (`InTexEggCoord`). `LightFanToPrimitves` writes traced hex radius/normalized centre alpha for per-fragment hex-distance-from-edge and fixed-hex-band fading independent of light length. Critter fans must follow sprite offsets (`HexView::GetSpriteOffsetPtr()`) exactly. `MovingContext::EvaluateRawProgress`'s `current_hex` may lag smooth interpolation; rapid-tap reconciliation in `ReceiveCritterMoving` may add inter-hex deltas. `CritterHexView::StopMoving` folds accumulated offsets into the reached hex, retaining sub-hex remainders and invariant `hex + offset` to avoid jumps. Normalization retains the original offset if a different target hex is movement-blocked, avoiding reporting a hex the server cannot path to. Bounded offsets keep fans under critters. Items also use `GetSpriteOffsetPtr()`;
- fog layers;
- local lights/render targets;
- engine-tool mapper helpers.

`map.GetEntityAtScreenPos(pos, ignoreTransparentEgg = false)` returns the picked entity, or null for empty points. Default picking prefers non-egg items over egg-faded sprites. `true` removes only that preference, retaining alpha checks/draw order so tools can target faded structures; neither render mask changes. Native `MapView::GetItemAtScreen` / `GetEntityAtScreen` share this default-false option. AngelScript and Managed C# expose the same default; projects choose which input modes opt in.

`map.GetVisibleItemOnHex(itemId, hex)` resolves an exact item ID only when a
valid, nonhidden sprite represents it at the requested occupied hex. This
includes drawn multihex copies whose primary sprite is hidden or outside the
viewport. The requested hex must be inside the bounded view area; the item's
origin may be outside it. Zero IDs and invalid hexes throw; unknown IDs, hidden
or invalid copies, physical-only mesh cells and removed items return null.
`item.GetSpriteMapAnchor(hex, localOffset, mapOffset, depthOffset)` uses the
item's owning map and the same exact-copy visibility rule. It returns a logical
pixel offset and depth correction for an independent map holder on that bitmap
plane; see [particle anchors](../../how-to/content/particle-format.md#integration).

This query checks render entries, not pixel alpha or frontmost screen picking.

Existing `GetItem`, `GetItemOnHex`, `GetItems` and `GetItemsOnHex` retain their
origin-based view-area filtering. `Item.IsVisible()` still checks the primary
sprite; it does not report visibility of every additional copy.

`MapView` is still a client-side view over the Common map model. Reusable coordinate/pathfinding rules belong in [Maps, Movement, and Geometry](../maps-and-movement.md); presentation details such as render targets, light textures, transparent eggs, map scrolling, and hit testing belong here and in [Frontend and Rendering](../rendering/).

`MapView::SetTransparentEgg` configures either screen-space egg slot; the critter overload uses sprite size. The rectangle script overload places `TransparentEggTarget` after `eggSize`: `TransparentEggTarget::AnyOccluder` cuts all occluders; `Structure` only walls/roofs, excluding props. `MapSprite::SetEggStructure` stores prototype-derived classification; roofs use `EggAppearenceType::Always`. `CheckEggAppearence` includes the egg's own line, cutting complete wall runs without parity adjustment. `IsCutByTransparentEgg` owns eligibility (`Test_TransparentEgg.cpp`).

Authored light intensity is percentage magnitude (`0..100`; negative values retain magnitude but select constant/personal capacity semantics). `MapView` clamps animated percentages, converts to raw falloff (`0..10000`), then uses day-light capacity percentage to scale light-map RGB to `0..200` and primitive alpha to `0..255`. `SetDayColors()` must invalidate cached fans when either day color or light-capacity percentage changes; both affect cached per-hex lighting.

`GetHexOffset(from, to)` equals `GetHexPos(to) - GetHexPos(from)`. `RebuildMapOffset()` moves every light vertex by the same scrolling pixel delta; `MapView` translates cached `_lightPoints` instead of rebuilding fans. Hiding a light's last visible hex still rebuilds primitives to remove stale triangles; entering lights reapply/rebuild normally. `Test_Geometry.cpp` pins uniform translation.

`SetExtraScrollOffset()` applies script-owned transient camera offsets to the map view. Project scripts own quake/shake timing and fade overlays.

## Resources, sprites, effects, and render targets

### Client map field storage

`MapView` stores fields in `ChunkedTwoDimensionalGrid`: a two-level, directly
indexed table of nullable owners of dense blocks. The grid's `ChunkSide` template
parameter must be a nonzero power of two. `MapView` supplies
`GameSettings::CLIENT_MAP_CHUNK_SIDE`, currently 16, so its blocks are 16x16.
Changing this compile-time setting requires rebuilding native consumers;
it is not a runtime configuration key. The top level holds one owner per
32x32 block slots; a slot table is allocated on the first write inside it.
The first write to a block constructs its `ChunkSide * ChunkSide` default
fields. Reading a missing block returns the shared empty field without
allocating. Access uses coordinate division/remainder by compile-time sides and
two array lookups; it does not hash coordinates or allocate per-field nodes.
Existing field addresses survive allocations of other blocks.
`ClearArea(origin, size)` resets every field of a clipped rectangle, releases
blocks it covers completely, and releases a slot table when its last block goes;
callers remove the views those fields own first. Mapper resize preserves
overlapping coordinates, discards removed fields, and may invalidate field
addresses. Unload releases the tables and every block.

Each field carries its own `Light`, the strongest colour of the light sources
applied to it. `MapView::SetupSpriteLight()` points a map sprite at the light of
its hex and the two neighbours its corner type reads, writing those fields so
that later light changes reach the stored addresses. Light storage therefore
follows the touched fields instead of the map area.

Only sprites drawn after the light pass take this per-hex light. Sprites below
`DrawOrderType::PreLight` (tiles and flat static items) are composited under the
light render target and are lit by it alone, so `ItemHexView::SetupSprite()`
gives them no vertex light.

No map-sized allocation or whole-map pass remains on construction, scrolling or
lighting. Roof groups are numbered from the tracked set of roof fields, in the
former x-major, y-minor order, so group numbers are unchanged. A map without a
scroll axial area clears only the fields it blocked before; a map with one still
visits every cell to mark its lines. Mapper resize destroys the views outside the
new size from the view lists. Populated blocks consume their full capacity, so a
map with writes in every block can still approach dense field storage.

`ClientLargeMapFieldsStayWithinMemoryBudget` and
`ClientLargeMapScrollBoundsStayWithinMemoryBudget` measure the real constructor
on an empty 1200x1200 map, including its view buffers and scroll bounds.
`ClientMaximumSizeMapLoadsWithoutMapSizedWork` constructs a map at the configured
`FO_MAP_MAX_SIZE` (up to 128,000 cells per side), scrolls across it and checks
time and private memory.
The hidden `ClientMapFieldAccessCost` compares actual field reads/writes with
dense and hash-based storage. Its timings qualify only those workloads, not
whole-game FPS or a fragmented 32-bit long session; see [Testing](../../contributing/testing/).

### Map unload and native storage lifetime

`ClientEngine::UnloadMap()` destroys the old `MapView` before constructing another. Script handles can retain its destroyed shell: Managed references survive until finalization, and either backend supports explicit handles. Unload therefore releases the grid, view/light buffers, entity collections, deferred item owners, fog, and scratch capacity without awaiting destruction. `MapSpriteList::InvalidateAll()` retains camera-rebuild pools; permanent `Clear()` detaches holders/frees pools. Preparation cancels the old generation before `OnMapUnload`; active CPU requests own inputs until quiescence and cannot publish into the next generation.

Unload retires all three manager-owned map targets; failed construction frees targets already created. Queued draws flush before teardown, and matching cached indoor-mask borrows clear before texture deletion; see [render-target ownership](../rendering/#render-targets-and-client-bridge). Sprite eviction also frees empty atlas pages/targets while preserving live allocations. Model mesh/animation definitions remain resource-identity caches, not retired-map owners.

`CritterView::OnDestroySelf()` clears this view’s attachment IDs and inventory without destroying passengers or a successor sharing its entity ID. Map/global removal use it; `ClientCritterDestroyClearsAttachmentsBeforeDelayedFinalRelease` tests delayed release and successor registration.

`Test_ClientEntityLifetime.cpp` covers retained handles, pending item owners, construction failure and empty/live atlas pages. Its retained-storage bound runs only when debug/profiling allocator statistics are available. Process private bytes and working set also include allocator, managed-runtime and renderer/driver caches; a remaining high-water mark alone is not proof of a loaded map, and headless target counts do not qualify hardware memory behavior.

The client resource path starts with a `FileSystem` from `GetClientResources()` and is organized by runtime managers:

- `ResourceManager` owns factories, extension dispatch, template/missing caches, invalidation, and defaults. `CritterFramesCache` caches project-resolved directional sheets; Mapper owns mutable preview models. `AudioManager` owns audio paths; scripts own project sound names.
- `SpriteManager` owns atlases, live updates, primitives, ordering, scissor stack, window/screen sizing, and target drawing.
- `DefaultSpriteFactory` loads default image/animation atlas sprites/sheets, including `ImageBaker`'s optional per-frame silhouette mesh. Decoders fill the `Sprite` payload of common `AnimationInfo` with frame count, duration, directions, and resolved frame bounds, including non-3D builds. Startup `EngineMetadata` reads compact version 1 `SpriteInfo/<PackName>.foinfo` indexes, avoiding pixel decoding for metadata queries. Ordinary full-image draws use source-backed meshes; explicit empty meshes skip drawing, missing/quad meshes retain the four-vertex path used by runtime-generated sprites.
- With `FO_ENABLE_3D`, `ModelSpriteFactory` creates atlas-backed models. `EngineMetadata` supplies parsed version 2 aggregates, project-selected preview views, and per-animation bounds from `ModelAnimationInfo.foinfo`; clients never reopen/reparse that companion, and authored `.fo3d` `DrawSize` / `ViewSize` no longer exist. Active transforms project enabled body/movement envelopes over every facing into grid-aligned logical frames containing body/projected shadow. The separate configured `Animation.PreviewStateAnim` / `Animation.PreviewActionAnim` envelope, with deterministic fallback, seeds body `ViewRect`; aggregate `ModelBounds` sizes horizontal lighting. GPU-free layout uses portable `AppRender::MIN_ATLAS_SIZE / FRAME_SCALE` ceiling (`MODEL_SPRITE_MAX_LOGICAL_FRAME_DIMENSION`), not the baker host's atlas limit. Runtime scratch textures are capped by `Render.ModelSpriteMaxTextureWidth` / `Height` and device atlases; logical frame size is texture size divided by `FRAME_SCALE`. Oversize pose/lighting envelopes crop, including otherwise-valid models enlarged by authored scale plus GUI preview `SetScale`. Invalid/non-finite bounds assert, reporting six coordinates/model file. Layer/child bounds extend view/name and aggregate lighting envelopes; only mesh-composition changes reset them, otherwise they grow. Names, coarse picking, eggs, flying text, and attachments thus fit derived bounds without authored sizes. Animation switches may refresh scratch frames but must retain the accumulated configuration view envelope, avoiding idle-root fallback that shifts equipped critters' name/flying-text anchors during turns.

  Models render into reusable 2x automatic-frame scratch targets. Animation predictions unite with selected geometry-link baked AABBs; sizing projects envelope corners rather than skinning combined vertices. When live particles need expansion, the factory unions current/required placements as signed root-relative intervals before rerendering. This absorbs adjacent pixel-rounded pivot alternation and permits frames wholly on one side of the root; bounded retries still reject unbounded/clipped frames. Cropped offsets preserve fixed roots, picking coordinates, and stable horizontal lighting. Scratch setup reserves no atlas space: allocation follows final crop. Changed placements prepare/render locally and publish only after successful atlas copy; failures retain old allocations/request immediate redraw. Slots only expand while animation/mesh/shadow envelope identity is stable, then may shrink once as transitions settle or composition changes. Emitting model attachments contribute mandatory bake bounds transformed by their current attachments, without per-frame live-AABB walks. Frame changes rebase emitted atlas-space particles to avoid movement; particles require full-frame crops. Full-frame cropping still derives origin from complete geometry/particle envelopes: pivot-only changes trigger sizing poses, expanding toward the effect rather than accumulating space right/down. Non-default model effects also disable tight crops. Shader-displacing effects need separate conservative contracts: bounds schema version 2 omits shader displacement.
- `ParticleSpriteFactory` does the same for particles.
- `EffectManager` loads default/minimal effects, resolves script choices, and updates per-frame buffers.
- `FontManager` loads fonts and formats/draws text with inline colors.
- `RenderTargetManager` creates, resizes, pushes, pops, reads, clears, dumps, and deletes offscreen targets.

3D critter alive-state idles restart at the beginning; dead idles freeze at the final frame, other non-alive idles at the first. Projects must author that first frame as the condition's resting pose.

These managers use `IAppRender` / `Renderer` abstractions rather than a specific renderer, supporting OpenGL, Direct3D, or null rendering according to platform/build configuration.

`ParticleManager` and `ParticleSystem` are neutral facades for sprites/model attachments. `ParticleRuntime.cpp` creates enabled `ParticleRuntimeBackend` implementations; the manager selects by extension. Each particle owns one `ParticleRuntimeSystem`. `ParticleSystem` owns common timing, scale, and render scheduling; virtual runtime operations own simulation/backend rendering.

Model attachments lack a `ParticleSprite`; `ModelInstance::ProcessAnimation` advances them after posing the full parent/child hierarchy. Child-joint effects require that order: stale poses make automatic sizing chase old bounds without converging. Particles receive logical model-frame deltas; zero-delta sizing re-poses update placement without advancing emission twice. Prewarm delays model-clock reset until the next real animation advance, preventing off-screen waits becoming a large first update that destroys warmed particle ages.

`ResourceManager -> ParticleSpriteFactory -> ParticleManager` invalidation calls every `ParticleRuntimeBackend::InvalidateResource()` because changed files may be dependencies rather than roots. SPARK drops matching `.spk` graphs and clears graph caches on texture/render-effect changes, reloading both next creation. Failed loads are uncached; backends without parsed-asset caches do nothing.

`VisualParticles.h` / `.cpp` contain no backend names/guards; extension files own concrete types/capabilities. SPARK editor checked typed access follows `GetRuntimeSystem()`. New runtimes therefore add no branches, enum values, or vendor types to the common facade.

Independent `FO_SPARK_PARTICLES` / `FO_EFFEKSEER_PARTICLES` default to `OFF`. Projects may enable either/both. The factory advertises only enabled extensions; disabled upstream runtimes neither compile nor link.

SPARK `.spark` sources bake to `.spk`; `SparkParticleRuntimeBackend` accepts only `.spk` through binary `loadFromBuffer`, never runtime XML. Binary/stream loaders must behave equivalently: truncated/oversized data, unknown types, descriptor-signature mismatches, zero/out-of-range references, and incompatible referenced classes invalidate graphs. Baker/editor/client share thread-safe `EnsureSparkParticleObjectsRegistered()`. `SparkQuadRenderer::Setup()` resolves effects/textures before returning particles, turning missing dependencies into ordinary load failure rather than first-draw exceptions. New editor renderers bind to their owner before preview initialization and draw nothing until effect/texture assignment, avoiding incomplete-state dereferences.

Baked raw `.efk` selects `EffekseerParticleRuntimeBackend`; clients never load `.efkproj` XML or run compilers, including on Web, where the host must bake `.efk` before packaging. Each `ClientEngine`/Mapper owns a core manager; each `Create` parses a fresh effect, while the shared factory caches successful atlas textures. Effekseer advances hierarchy/emission/lifetimes; custom callbacks copy evaluated snapshots into FOnline packets without an Effekseer graphics backend. Initial CPU Sprite/Ring-only support rejects unsupported renderer/material families before creation. Dynamic checks retire existing handles on non-finite data, invalid UV ranges, or atlas filter mismatches. Simulation is independent of draw, so off-viewport direct-scene lifetimes continue; stopped handles drain deferred removal queues before wrapper release.

The exception is an explicit direct-scene prewarm request: it remains pending
until the first `DrawInScene` can provide the current transform, and scheduled
updates pause meanwhile. Effekseer prewarm advances exactly one second, then
resynchronizes the update clock before ordinary simulation resumes so the
offscreen wait is not double-counted.
Sprite mesh geometry is independent of the pixel-exact hit mask. `FillAtlas`
still derives hit testing directly from source alpha and `Render.SpriteHitValue`;
contour simplification/dilation only changes which triangles are submitted for
drawing. Cropped regions, repeated patterns, fonts, render-target blits, and
padded custom-effect/contour draws intentionally construct quads because their
sampling rectangle is not the source sprite silhouette.

### Fonts and Inline Color Tags

[Font Formats And Text Layout](../../how-to/content/font-format.md) owns font authoring/runtime contracts; its [generated reference](../../reference/font-format/index.md) specifies fields, enums, limits, and validation. This page places `FontManager` in the client lifecycle.

`FontManager::FormatText()` strips `@color:0xBBGGRR@` / `@color:0xAABBGGRR@`, recording parsed `ucolor` per glyph during draw formatting. `@color@` restores the previous inline color, or base draw color if none is active. `FontFlag::NoColorize` strips tags but renders with the caller's base color.

`Game.BindFont(font, path, defaultScale = 1.0)` downscales bound slots once at bind time: it re-rasterizes glyphs in their atlas region with area-average filtering and rounds advances, offsets, line height, and space width to target integers. `Game.GetTextInfo(...)`, `Game.GetTextLines(...)`, and `Game.DrawText(...)` then use integer pixels, like a smaller authored font, without fractional glyph positions. Scale must be `(0..1]`; bitmap upscaling fails, so author larger assets for larger text.

## Input and script-facing hooks

`ClientEngine::ProcessInputEvent()` converts frontend `InputEvent` values into script events:

- `OnMouseDown`, `OnMouseUp`, `OnMouseMove`;
- `OnTouchDown`, `OnTouchMove`, `OnTouchUp` for raw per-finger touch streams, plus `OnTouchTap`, `OnTouchDoubleTap`, `OnTouchScroll`, `OnTouchZoom` for aggregated gestures;
- `OnKeyDown`, `OnKeyUp`, `OnInputLost`;
- `OnScreenScroll` and `OnScreenSizeChanged`;
- map render-stage events such as `OnRenderMap_BeforeTiles`, `OnRenderMap_AfterSprites`, and `OnRenderMap_AfterFlushMap`.

`Source/Frontend/Application.h` owns input semantics; projects own game UI in scripts/GUI resources.

**Client runtime filters typed text.** `ProcessInputEvent()` removes C0 controls and `DEL` from `KeyDown.Text` before `OnKeyDown`, dropping control-only `KeyCode::Text` events. Windows Alt+numpad delivers ordinary OS text: below `0x20` these are bare controls, rendered as CP437 junk pseudographics by bitmap fonts. Character filtering preserves AltGr, ordinary typing, and printable Alt codes (`Alt+0169`) rather than rejecting Alt modifiers. Byte filtering is UTF-8-safe: multi-byte sequence bytes are `>= 0x80`.

`ProcessInputEvent()` receives every source: SDL through `ProcessInputEvents()`, scripted `Game.Simulate*`, and project automation bridges. Frontend/SDL filtering would cover only OS input, missing simulated-input tests.

Client scripts can synthesize local input through the same runtime path for automation and embedded-client probes. `Game.SimulateMouseMove(pos)`, `Game.SimulateMouseDown(pos, button)`, and `Game.SimulateMouseUp(pos, button)` preserve held-button state across a raw mouse gesture, including positions outside the render window; `Game.SimulateMouseClick(pos, button)` sends a complete mouse click or wheel event. `Game.SimulateTouchDown(fingerId, pos)`, `Game.SimulateTouchMove(fingerId, pos, offsetPos)`, and `Game.SimulateTouchUp(fingerId, pos)` send raw touch streams, `Game.SimulateTouchTap(pos)` sends a completed tap event, `Game.SimulateTouchScroll(pos, offsetPos)` sends an aggregated scroll gesture with signed pixel offsets, `Game.SimulateKeyPress(key, text)` sends one key down/up pair, and `Game.SimulateKeyboardPress(key1, key2, key1Text, key2Text)` remains available for two-key sequences. Two network notifications are synthesizable the same way: `Game.SimulateDisconnect()` delivers the `OnDisconnected` notification a real disconnect ends with, and `Game.SimulateInfoMessage(infoMessage, extraText)` delivers `OnInfoMessage`. Both leave the connection itself untouched — a probe testing the reaction to a dropped session must not end the session it reports through.

`ClientEngine::CritterMoveTo()` brings active `MovingContext` state to the current frame before starting movement/stopping, then normalizes hex/offset pairs before sending requests. Rapid start/stop therefore sends neither stale nor overlarge offsets.

### Held-direction movement

`Critter.MoveToDir(dir, speed)` is a held direction — keyboard, D-pad or stick — not a path to a point ahead. It sets or steers the critter's *direct-move intent*, which lasts until `StopMove()`, a path order (`MoveToHex()`), or the critter leaving the current map (`UnloadMap()` drops it). A stop still sends `SendStopCritterMove` when local playback has already ended but the direct intent remains active; the server may still hold its lease.

The client materialises the intent itself:

- `Client.DirectMoveTraceSteps` bounds each trace. `Client.DirectMoveSlide` allows forward side steps around blockers. Extensions retain the ray and authoritative speed without rounding drift. Only full traces extend when `Client.DirectMoveExtendAheadSteps` remain.
- Numbered plans lease projected distance. Server and observers advance at most `Client.DirectMoveLeaseSteps` ahead of confirmed input. `SendCritterMoveLease` renews when `Client.DirectMoveLeaseRenewSteps` remain. Uplink stalls stop them at the lease regardless of speed corrections or usual transit delay.
- `Client.DirectMoveRetargetImmediateAngle` turns, speed changes, starts and stops apply immediately. Smaller turns (45° with the default 60° threshold) wait for `Client.DirectMoveRetargetMinMs`; the latest input wins. Blocked directions retry at that interval.
- Held input resumes after server corrections. Truncation makes a correction finite and retires its lease and renewals.

Observers match `CritterMoveLease` by server movement revision and shift its boundary by joined path distance at current speed. `MovingContext::SetLeaseTime` pauses the clock there until renewal; `CritterHexView` draws the wait standing. See [direction traces](../maps-and-movement.md#direction-traces) and [tracing](../authority-and-networking/index.md#movement-synchronization-trace).

### Late plans catch up

Movement and ping messages carry sender-clock stamps (see [networking](../authority-and-networking/)). Each connection's `LinkDelayEstimator` measures excess over median receive-minus-send offset within `Network.LinkDelayWindowMs`, without shared clocks. Usual samples survive silence. Excluding lateness from `Network.MoveLateCatchUpMinMs` keeps stall bursts visible. Lateness persisting for `Network.LinkDelayRebaseMs` rebases usual transit independently of the bounded queue. The median keeps ordinary jitter from moving the server ahead of later stops.

Plans and renewals catch up from that minimum to `Network.MoveLateCatchUpMaxMs`. The server fast-forwards; observers play up to `Client.MoveCatchUpSmoothMaxMs` at `Client.MoveCatchUpRate` and skip the excess. Observers at the plan start play server-ahead progress; newly visible copies start at current progress. Chosen input is not caught up.

### Joining a plan already under way

`FindPathPrefixSteps` / `DropPathPrefix` join at the current hex within `Network.MovePlanJoinMaxSteps`, without backward replay. The server first advances its previous plan to now. Zero disables joining.

## Client-side validation tests

Choose the smallest relevant client-runtime test scope:

- `Source/Tests/Test_ClientEngine.cpp` — native client construction, entity registration and rendering resources with either scripting backend.
- `Source/Tests/Test_AngelScriptClientEngine.cpp` — AngelScript initialization, loop callbacks and client bindings.
- `Source/Tests/Test_ManagedScriptRuntime.cpp` — real managed client assembly loading and module initialization.
- `Source/Tests/Test_ClientServerIntegration.cpp` — native transport, malformed payload and connection behavior.
- `Source/Tests/Test_AngelScriptClientServerIntegration.cpp` — client/server handshake, login, shared maps and connection events through AngelScript.
- `Source/Tests/Test_ClientDataValidation.cpp` — inbound remote-call payload validation for UTF-8, enums, floats, hashed strings, bools, truncation, and ref-type payloads.
- `Source/Tests/Test_ClientRuntimeApi.cpp` — client runtime ABI/export/result validation for the host/runtime split described in [Client Updater](client-updater.md).

Embedding-project CMake/BuildTools generates target names; Engine docs must not hard-code a project's names.

## Change checklist

When changing client runtime code, verify:

- `ClientEngine` startup/shutdown still leaves no stale registered entities or render targets.
- New network messages are documented in [Networking](../authority-and-networking/) and bound through `ClientConnection::AddMessageHandler()`.
- New synced state has a clear ownership boundary between Common entities/properties and client-only view state.
- Map presentation changes do not duplicate coordinate/pathfinding rules already owned by [Maps, Movement, and Geometry](../maps-and-movement.md).
- Resource changes describe whether they affect `ResourceManager`, `SpriteManager`, a sprite factory, `EffectManager`, or `RenderTargetManager`.
- Platform-specific rendering/input implications are reflected in [Frontend and Rendering](../rendering/), [Web Build, Packaging, and Browser Debugging](../../how-to/platforms/web-debugging.md), or [Android Build, Packaging, and Device Debugging](../../how-to/platforms/android-debugging.md) as appropriate.

Optional CPU workers: [client multithreading](client-multithreading.md) owns worker selection, model pose phases, and validation.

## Map light and fog calculation ownership

Dirty lights/large fog shapes may calculate passive marks/fan geometry within the client worker budget. Source lifecycle, map-field publication, offset borrows, transitions, and GPU submission remain on the application thread. Direct/small paths do not snapshot obstacles. [light and fog batches](client-multithreading.md#light-and-fog-batches) owns thresholds, aggregation/trace rules, exceptions, and native migration.
