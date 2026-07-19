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
- `Source/Client/FontManager.h`
- `Source/Client/FontManager.cpp`
- `Source/Client/MapView.h`
- `Source/Client/MapView.cpp`
- `Source/Scripting/ClientMapScriptMethods.cpp`
- `Source/Client/CritterView.h`
- `Source/Client/CritterHexView.h`
- `Source/Client/ItemView.h`
- `Source/Client/ItemHexView.h`
- `Source/Client/LocationView.h`
- `Source/Client/PlayerView.h`
- `Source/Client/DefaultSprites.h`
- `Source/Client/DefaultSprites.cpp`
- `Source/Client/3dStuff.h`
- `Source/Client/3dStuff.cpp`
- `Source/Client/3dAnimation.h`
- `Source/Client/3dAnimation.cpp`
- `Source/Client/ModelSprites.h`
- `Source/Client/ModelSprites.cpp`
- `Source/Client/ModelSpriteLayout.h`
- `Source/Client/ModelSpriteLayout.cpp`
- `Source/Common/AnimInfo.h`
- `Source/Common/AnimInfo.cpp`
- `Source/Common/ModelBounds.h`
- `Source/Common/ModelBounds.cpp`
- `Source/Client/ParticleSprites.h`
- `Source/Client/ParticleSprites.cpp`
- `Source/Client/RenderTarget.h`
- `Source/Client/RenderTarget.cpp`
- `Source/Tests/Test_ClientEngine.cpp`
- `Source/Tests/Test_ClientRuntimeApi.cpp`
- `Source/Tests/Test_ClientDataValidation.cpp`
- `Source/Tests/Test_ClientServerIntegration.cpp`
- `Source/Tests/Test_ModelBaker.cpp`

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
2. **Resource filesystem selection** starts through `GetClientResources(GlobalSettings&)` in `Source/Client/Client.cpp`, which builds the client-side `FileSystem` view used by runtime managers. Installed clients may add a higher-priority writable resource overlay above the read-only base resources; see [ConfigurationAndDataSources.md](ConfigurationAndDataSources.md) and [ClientUpdater.md](ClientUpdater.md).
3. **Engine construction** wires settings, resources, the main application window, generated metadata, script modules, and client managers.
4. **`OnStart`/script initialization** gives scripts their first client-side entry point. `Source/Tests/Test_ClientEngine.cpp` validates that script module init and loop calls are callable on a self-contained client runtime.
5. **The main loop** processes frontend input, network packets, scripted loop callbacks, visual effects, video playback, map processing, and rendering-facing updates.
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

Client-side script continuations scheduled through `ScheduleDelayedCallback()` are processed once per main-loop pass from a snapshot of callbacks already due at the start of that pass. A callback that schedules another zero-delay callback, including `Yield(0)`, resumes on the next pass instead of re-entering immediately. This prevents script wait loops from starving the next network/input tick.

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

## Critter model animation

3D critter models use separate body/action and movement animation controllers. `ModelInstance::PlayAnim()` applies
animation-specific speed (`AnimSpeed`) to the body/action controller, while `RefreshMoveAnimation()` assigns gait and
movement-speed scaling to the movement controller's track. When both are active, the movement controller advances with
the model base/link/global speed only; it must not inherit the current body action's `AnimSpeed`, otherwise fast actions
such as use/pick-up make the leg cycle run too quickly while the critter is moving.

## `MapView`: map presentation and local spatial state

`MapView` is the largest client view class because it bridges several subsystems:

- map file/static-data loading through `LoadFromFile()` and `LoadStaticData()`;
- map processing through `Process()`;
- map rendering through `DrawMap()` and staged render events on `ClientEngine`;
- field indexes for items and critters;
- coordinate conversion, zoom, scrolling, screen-to-map and map-to-screen transformations;
- path finding and cut-path helpers built on `PathFinding::FindPath()`;
- line tracing for bullets and light fans; `MapView::ApplyLightFan` traces every light source out to its full `Distance` in hexes (kept at >= 1) and forwards per-light falloff metadata to primitive shaders via `PrimitivePoint::EggData` → vertex attribute slot 3 (`InTexEggCoord`): `LightFanToPrimitves` writes the traced radius (hexes) and the normalized center alpha so a shader can reconstruct each fragment's hex-distance-from-edge and fade over a fixed hex band independent of the light's total length. A critter's light fan follows the critter's sprite offset (`HexView::GetSpriteOffsetPtr()`) so the light stays exactly under the sprite — the two must never diverge. The offset is kept bounded at the source: `MovingContext::EvaluateRawProgress`'s `current_hex` can lag the smooth lerp, and client prediction reconciliation in `ReceiveCritterMoving` can fold an inter-hex delta into the offset during rapid step taps, but `CritterHexView::StopMoving` cashes any accumulated offset back into the hex (snapping to the hex the sprite actually reached and keeping only the sub-hex remainder, with `hex + offset` invariant so the sprite never jumps), so the offset cannot run away and drag the fan off the critter. Items use the same `GetSpriteOffsetPtr()`;
- fog-of-war layers;
- local lighting sources and render targets;
- mapper mode helpers used by engine tools.

`MapView` is still a client-side view over the Common map model. Reusable coordinate/pathfinding rules belong in [MapsMovementGeometry.md](MapsMovementGeometry.md); presentation details such as render targets, light textures, transparent eggs, map scrolling, and hit testing belong here and in [FrontendAndRendering.md](FrontendAndRendering.md).

Map light source intensity is authored as a percentage magnitude (`0..100`, with negative values keeping the same magnitude but opting into constant/personal capacity semantics). `MapView` clamps the current animated percentage, converts it to an internal raw falloff scale (`0..10000`), and then scales light-map RGB to the engine light range (`0..200`) and primitive alpha to `0..255` through the source's day-light capacity percentage. `SetDayColors()` must invalidate applied light fans when either the day color or the light-capacity percentage changes, because both feed cached per-hex lighting.

The reusable map presentation API includes `SetExtraScrollOffset()` for script-owned transient camera offsets. The engine applies the offset to the map view, but game-specific screen effects such as quake/shake timing and fade overlays are owned by embedding-project scripts.

## Resources, sprites, effects, and render targets

The client resource path starts with a `FileSystem` from `GetClientResources()` and is organized by runtime managers:

- `ResourceManager` indexes resource files, resolves item default sprites, loads and caches critter animation frames, handles Fallout-style animation frame mapping, and exposes sound-name mappings.
- `SpriteManager` owns sprite factories, atlases, primitive drawing, draw ordering, scissor stack, window/screen sizing, and render-target drawing.
- `DefaultSpriteFactory` loads atlas sprites and sprite sheets from default
  image/animation resources, including the optional per-frame silhouette mesh
  produced by `ImageBaker`. The resource decoder also fills the `Sprite`
  payload of the common `AnimInfo` record with frame count, duration,
  directions, and resolved per-frame bounds; this payload remains available in
  builds without 3D support. `EngineMetadata` reads the matching compact
  version 1 `SpriteInfo/<PackName>.foinfo` indexes at startup, so common sprite metadata
  queries do not decode pixel payloads. A source-backed sprite uses its mesh for ordinary
  full-image draws; an explicit empty mesh skips the draw, while a missing/quad
  mesh keeps the four-vertex path used by runtime-generated atlas sprites.
- With `FO_ENABLE_3D`, `ModelSpriteFactory` turns model resources into
  atlas-backed sprites. It asks
  `EngineMetadata` for the already parsed version 2 aggregate, idle-priority
  view, and per-animation bounds from `ModelAnimInfo.foinfo`; the client model
  layer never reopens or reparses that companion, and no authored `.fo3d`
  `DrawSize` or `ViewSize` remains.
  Enabled body/movement animation envelopes are projected through the active
  model transform across every facing direction to derive a power-of-two
  logical scratch frame large enough for the body and projected shadow. The
  separate view envelope (`Unarmed + Idle`, any Idle, then deterministic
  fallback) seeds the body `ViewRect`. Runtime layer and child-model bounds
  extend both the view/name rectangle and the aggregate horizontal-lighting
  frame; the envelope resets when mesh composition changes and otherwise only
  grows. Names, coarse picking, transparent eggs, flying text, and attachments
  therefore stay inside automatically derived bounds without authored sizes.

  The model is rendered into a reusable 2x scratch target for the automatic
  frame. Per-animation prediction and exact weighted skinning of referenced
  combined-mesh vertices choose the atlas crop. If the evaluated pose requires a larger scratch frame,
  the factory expands it and rerenders before copying; the bounded retry loop
  fails rather than accepting a clipped frame that does not converge. The
  cropped sprite offset preserves the fixed model root, hit-test coordinates,
  and stable horizontal lighting gradient. Scratch-frame setup does not reserve
  atlas space; allocation happens only after the final crop is known. A changed
  placement is prepared locally, rendered, and published only after the atlas
  copy succeeds, while failures request an immediate redraw and retain the old
  allocation. An atlas
  slot only expands while its active animation/mesh/shadow envelope identity is
  unchanged, then may shrink once when a transition settles or mesh composition
  changes. Model-attached particles enable SPARK live-AABB computation; emitted
  quads and trails drive bounded frame expansion after their first update, with
  the advertised canvas retained as the pre-update fallback. Frame changes
  rebase already emitted atlas-space particles so expansion does not move them,
  and particles force a full-frame crop. A non-default model effect also disables the tight
  crop; effects that displace vertices beyond ordinary skinned geometry need a
  separate conservative rendering contract because bounds schema version 2 does
  not encode shader displacement.
- `ParticleSpriteFactory` does the same for particle resources.
- `EffectManager` loads default/minimal effects, resolves script-selected effects, and updates per-frame effect buffers.
- `FontManager` loads fonts and formats/draws text, including inline color tags.
- `RenderTargetManager` creates, resizes, pushes, pops, reads, clears, dumps, and deletes offscreen render targets.

For 3D critter views, idle refresh plays alive-state animations from the beginning. Dead condition idles freeze on their final frame. Other non-alive condition idles freeze on their first frame, so embedding projects should author that first frame as the intended resting pose for the condition.

These managers are renderer-facing but not renderer-specific. They talk through `IAppRender` / `Renderer` abstractions, so the same client logic can run against OpenGL, Direct3D, or the null renderer depending on platform/build configuration.

Sprite mesh geometry is independent of the pixel-exact hit mask. `FillAtlas`
still derives hit testing directly from source alpha and `Render.SpriteHitValue`;
contour simplification/dilation only changes which triangles are submitted for
drawing. Cropped regions, repeated patterns, fonts, render-target blits, and
padded custom-effect/contour draws intentionally construct quads because their
sampling rectangle is not the source sprite silhouette.

### Fonts and Inline Color Tags

`FontManager::FormatText()` strips `@color:0xBBGGRR@` / `@color:0xAABBGGRR@` tags and records the parsed `ucolor` value in the formatted text's per-glyph color buffer during draw formatting. The reset tag is `@color@`; it restores the previous inline color, or the base draw color when no inline color is active. `FontFlag::NoColorize` still strips these tags, but keeps rendering with the caller-provided base color.

`Game.BindFont(font, path, defaultScale = 1.0)` can downscale the bound font slot. The scale is applied once at bind time: glyph bitmaps are re-rasterized in place inside the font's atlas region with an area-average filter, and every metric (advances, offsets, line height, space width) is rounded to integers at the target size. The runtime text pipeline (`Game.GetTextInfo(...)`, `Game.GetTextLines(...)`, `Game.DrawText(...)`) therefore always works in plain integer pixel coordinates — a scaled font behaves exactly like a font authored at the smaller size, with no fractional glyph positions. The scale must be in `(0..1]`; upscaling a bitmap font is rejected — author a bigger font asset for larger text.

## Input and script-facing hooks

`ClientEngine::ProcessInputEvent()` receives frontend `InputEvent` values and raises higher-level script events such as:

- `OnMouseDown`, `OnMouseUp`, `OnMouseMove`;
- `OnTouchDown`, `OnTouchMove`, `OnTouchUp` for raw per-finger touch streams, plus `OnTouchTap`, `OnTouchDoubleTap`, `OnTouchScroll`, `OnTouchZoom` for aggregated gestures;
- `OnKeyDown`, `OnKeyUp`, `OnInputLost`;
- `OnScreenScroll` and `OnScreenSizeChanged`;
- map render-stage events such as `OnRenderMap_BeforeTiles`, `OnRenderMap_AfterSprites`, and `OnRenderMap_AfterFlushMap`.

Input semantics originate in `Source/Frontend/Application.h`; game-specific UI behavior should stay in scripts and GUI resources owned by the embedding project.

Client scripts can synthesize local input through the same runtime path for automation and embedded-client probes. `Game.SimulateMouseClick(pos, button)` sends mouse move/click or wheel events, `Game.SimulateTouchDown(fingerId, pos)`, `Game.SimulateTouchMove(fingerId, pos, offsetPos)`, and `Game.SimulateTouchUp(fingerId, pos)` send raw touch streams, `Game.SimulateTouchTap(pos)` sends a completed tap event, `Game.SimulateKeyPress(key, text)` sends one key down/up pair, and `Game.SimulateKeyboardPress(key1, key2, key1Text, key2Text)` remains available for two-key sequences.

For local critter movement prediction, `ClientEngine::CritterMoveTo()` synchronizes any active `MovingContext` to the current client frame before starting a new movement or sending a stop request. It then normalizes the local hex/offset pair before the next request is sent, so rapid start/stop input does not report one-frame-stale or overlarge offsets to the server.

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
