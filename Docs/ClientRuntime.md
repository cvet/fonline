# Client Runtime

> Engine-owned documentation. This page describes reusable client runtime behavior in `Source/Client/`; game UI policy, gameplay rules, and concrete content remain in the embedding project.

## Purpose

The client runtime turns baked resources, server state, local input, and scripts into the player-facing game view. It does not own game design decisions. Instead, it provides the reusable engine pieces that a game project drives through scripts, configuration, resources, and server messages.

Read this page together with:

- [EntityModel.md](EntityModel.md) for the entity/property/prototype model that client views wrap.
- [MapsMovementGeometry.md](MapsMovementGeometry.md) for map positions, path finding, line tracing, and movement contexts.
- [Networking.md](Networking.md) for command buffers, transports, and property sync.
- [FrontendAndRendering.md](FrontendAndRendering.md) for platform windows, input, audio, and renderer backends.
- [WebDebugging.md](WebDebugging.md), [AndroidDebugging.md](AndroidDebugging.md), and [Debugging.md](Debugging.md) for platform-specific validation flows.

## Source paths inspected

- `Source/Client/Client.h`
- `Source/Client/Client.cpp`
- `Source/Client/ClientConnection.h`
- `Source/Client/ClientConnection.cpp`
- `Source/Client/ResourceManager.h`
- `Source/Client/ResourceManager.cpp`
- `Source/Client/MapView.h`
- `Source/Client/MapView.cpp`
- `Source/Client/CritterView.h`
- `Source/Client/CritterHexView.h`
- `Source/Client/ItemView.h`
- `Source/Client/ItemHexView.h`
- `Source/Client/LocationView.h`
- `Source/Client/PlayerView.h`
- `Source/Client/DefaultSprites.h`
- `Source/Client/DefaultSprites.cpp`
- `Source/Client/ModelSprites.h`
- `Source/Client/ModelSprites.cpp`
- `Source/Client/ParticleSprites.h`
- `Source/Client/ParticleSprites.cpp`
- `Source/Client/RenderTarget.h`
- `Source/Client/RenderTarget.cpp`
- `Source/Tests/Test_ClientEngine.cpp`
- `Source/Tests/Test_ClientRuntimeApi.cpp`
- `Source/Tests/Test_ClientDataValidation.cpp`
- `Source/Tests/Test_ClientServerIntegration.cpp`

## Runtime owner: `ClientEngine`

`ClientEngine` in `Source/Client/Client.h` is the central client-side engine object. It derives from `BaseEngine` and `AnimationResolver`, owns the high-level client lifecycle, and exposes event hooks used by client scripts.

Major responsibilities:

- initialize and shut down the client runtime;
- run the per-frame `MainLoop()`;
- process application input events;
- connect and disconnect from a server through `ClientConnection`;
- create, register, unregister, and look up client-side entities by id;
- receive and apply network messages for critters, items, maps, custom entities, time sync, movement, actions, and properties;
- own client-facing managers such as sprites, effects, fonts, sounds, video playback, resources, cache storage, and render targets;
- raise engine events such as `OnStart`, `OnLoop`, `OnConnected`, `OnDisconnected`, render-map stages, input events, entity in/out events, and map load/unload events.

`ClientEngine` is intentionally broad: it is the composition root where Common-layer data (`Entity`, properties, prototypes, networking buffers) meets Frontend-layer services (`Application`, render, input, audio) and game scripts.

## Client lifecycle

A typical client lifetime has these phases:

1. **Application initialization** happens in the frontend layer. `Application` owns the main window, renderer, input, and audio. See [FrontendAndRendering.md](FrontendAndRendering.md).
2. **Resource filesystem selection** starts through `GetClientResources(GlobalSettings&)` in `Source/Client/Client.cpp`, which builds the client-side `FileSystem` view used by runtime managers.
3. **Engine construction** wires settings, resources, the main application window, generated metadata, script modules, and client managers.
4. **`OnStart`/script initialization** gives scripts their first client-side entry point. `Source/Tests/Test_ClientEngine.cpp` validates that script module init and loop calls are callable on a self-contained client runtime.
5. **The main loop** processes frontend input, network packets, scripted loop callbacks, visual effects, video playback, screen fade/quake, map processing, and rendering-facing updates.
6. **Network connection** starts with `Connect()`, which delegates transport setup and handshake work to `ClientConnection`.
7. **Map and entity state** arrive through network messages, are represented as client view entities, and are updated through property sync and movement/action packets.
8. **Shutdown** disconnects networking, destroys inner entities, clears caches and render targets, and releases frontend resources.

When changing startup or shutdown behavior, keep script events, manager lifetime, entity registration, and network callbacks in sync; these paths are tightly coupled.

## Server connection and message dispatch

`ClientConnection` (`Source/Client/ClientConnection.h`, `Source/Client/ClientConnection.cpp`) owns the client-side transport state. It hides whether the current connection is interthread, TCP sockets, or UDP-capable sockets.

Important responsibilities:

- `Connect()` selects the connection mode from `ClientNetworkSettings` and creates a `NetworkClientConnection`;
- `CreateNetworkConnection(bool use_udp)` selects socket or UDP socket transport;
- `Process()` advances connect, receive, dispatch, and send work;
- `Disconnect()` tears down active transport and raises the configured disconnect handler;
- `TryFallbackToTcp()` provides a fallback path when UDP setup fails;
- `Net_SendHandshake()`, `Net_OnHandshakeAnswer()`, and `Net_OnPing()` handle connection-level protocol messages;
- `AddMessageHandler(NetMessage, MessageCallback)` binds protocol messages to `ClientEngine::Net_On...` handlers.

`ClientEngine` owns the semantic handlers. Examples include `Net_OnInitData`, `Net_OnAddCritter`, `Net_OnRemoveCritter`, `Net_OnProperty`, `Net_OnLoadMap`, `Net_OnSomeItems`, `Net_OnRemoteCall`, `Net_OnAddCustomEntity`, and `Net_OnRemoveCustomEntity`.

For protocol format details, use [Networking.md](Networking.md). For client/server handshake validation, see `Source/Tests/Test_ClientServerIntegration.cpp`, especially `ClientAndServerHandshakeOverInterthreadTransport`.

## Entity and view model

Client-side game objects are not raw server entities. They are view entities that combine Common-layer entity data with client-only rendering, input, and presentation state.

Primary view types:

- `PlayerView` — client representation of the current player entity.
- `LocationView` — client-side location entity.
- `MapView` — map entity plus map rendering, local spatial indexes, fog, lighting, scrolling, zoom, and hit testing.
- `CritterView` — critter entity visible outside the current map context.
- `CritterHexView` — critter placed on a map hex, with movement, direction, and map rendering behavior.
- `ItemView` — item entity visible in inventory or generic client contexts.
- `ItemHexView` — item placed on a map hex, with map flags, sprite placement, blockers, and lighting implications.
- custom client entities — created by `CreateCustomEntityView()` when a synced custom entity entry arrives.

`ClientEngine::RegisterEntity()` and `ClientEngine::UnregisterEntity()` maintain the id-to-entity lookup used by network handlers and scripts. `Source/Tests/Test_ClientEngine.cpp` validates that client entities can be registered and removed from the lookup.

## `MapView`: map presentation and local spatial state

`MapView` is the largest client view class because it bridges several subsystems:

- map file/static-data loading through `LoadFromFile()` and `LoadStaticData()`;
- map processing through `Process()`;
- map rendering through `DrawMap()` and staged render events on `ClientEngine`;
- field indexes for items and critters;
- coordinate conversion, zoom, scrolling, screen-to-map and map-to-screen transformations;
- path finding and cut-path helpers built on `PathFinding::FindPath()`;
- line tracing for bullets and light fans;
- fog-of-war layers;
- local lighting sources and render targets;
- mapper mode helpers used by engine tools.

`MapView` is still a client-side view over the Common map model. Reusable coordinate/pathfinding rules belong in [MapsMovementGeometry.md](MapsMovementGeometry.md); presentation details such as render targets, light textures, transparent eggs, map scrolling, and hit testing belong here and in [FrontendAndRendering.md](FrontendAndRendering.md).

## Resources, sprites, effects, and render targets

The client resource path starts with a `FileSystem` from `GetClientResources()` and is organized by runtime managers:

- `ResourceManager` indexes resource files, resolves item default sprites, loads and caches critter animation frames, handles Fallout-style animation frame mapping, and exposes sound-name mappings.
- `SpriteManager` owns sprite factories, atlases, primitive drawing, draw ordering, scissor stack, window/screen sizing, and render-target drawing.
- `DefaultSpriteFactory` loads atlas sprites and sprite sheets from default image/animation resources.
- `ModelSpriteFactory` turns model resources into atlas-backed sprites by rendering model frames into a render target.
- `ParticleSpriteFactory` does the same for particle resources.
- `EffectManager` loads default/minimal effects, resolves script-selected effects, and updates per-frame effect buffers.
- `RenderTargetManager` creates, resizes, pushes, pops, reads, clears, dumps, and deletes offscreen render targets.

These managers are renderer-facing but not renderer-specific. They talk through `IAppRender` / `Renderer` abstractions, so the same client logic can run against OpenGL, Direct3D, or the null renderer depending on platform/build configuration.

## Input and script-facing hooks

`ClientEngine::ProcessInputEvent()` receives frontend `InputEvent` values and raises higher-level script events such as:

- `OnMouseDown`, `OnMouseUp`, `OnMouseMove`;
- `OnTouchTap`, `OnTouchDoubleTap`, `OnTouchScroll`, `OnTouchZoom`;
- `OnKeyDown`, `OnKeyUp`, `OnInputLost`;
- `OnScreenScroll` and `OnScreenSizeChanged`;
- map render-stage events such as `OnRenderMap_BeforeTiles`, `OnRenderMap_AfterSprites`, and `OnRenderMap_AfterFlushMap`.

Input semantics originate in `Source/Frontend/Application.h`; game-specific UI behavior should stay in scripts and GUI resources owned by the embedding project.

## Client-side validation tests

Use the smallest relevant test scope when changing client runtime behavior:

- `Source/Tests/Test_ClientEngine.cpp` — self-contained client engine startup, entity registration, script module initialization, and loop callbacks.
- `Source/Tests/Test_ClientServerIntegration.cpp` — client/server handshake over interthread transport and observable client connection events.
- `Source/Tests/Test_ClientDataValidation.cpp` — inbound remote-call payload validation for UTF-8, enums, floats, hashed strings, bools, truncation, and ref-type payloads.
- `Source/Tests/Test_ClientRuntimeApi.cpp` — client runtime ABI/export/result validation for the host/runtime split described in [ClientUpdater.md](ClientUpdater.md).

Exact test target names are generated by the embedding project's CMake/BuildTools configuration; do not hard-code one project's target names in engine docs.

## Change checklist

When changing client runtime code, verify:

- `ClientEngine` startup/shutdown still leaves no stale registered entities or render targets.
- New network messages are documented in [Networking.md](Networking.md) and bound through `ClientConnection::AddMessageHandler()`.
- New synced state has a clear ownership boundary between Common entities/properties and client-only view state.
- Map presentation changes do not duplicate coordinate/pathfinding rules already owned by [MapsMovementGeometry.md](MapsMovementGeometry.md).
- Resource changes describe whether they affect `ResourceManager`, `SpriteManager`, a sprite factory, `EffectManager`, or `RenderTargetManager`.
- Platform-specific rendering/input implications are reflected in [FrontendAndRendering.md](FrontendAndRendering.md), [WebDebugging.md](WebDebugging.md), or [AndroidDebugging.md](AndroidDebugging.md) as appropriate.
