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
- `Source/Scripting/AngelScript/CoreScripts/Gui.fos`
- `Source/Scripting/Managed/CoreScripts/Gui.cs`
- `Source/Client/CritterView.h`
- `Source/Client/CritterHexView.h`
- `Source/Client/ItemView.h`
- `Source/Client/ItemHexView.h`
- `Source/Client/LocationView.h`
- `Source/Client/PlayerView.h`
- `Source/Client/DefaultSprites.h`
- `Source/Client/DefaultSprites.cpp`
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
- `Source/Tests/Test_ClientRuntimeApi.cpp`
- `Source/Tests/Test_ClientDataValidation.cpp`
- `Source/Tests/Test_ClientServerIntegration.cpp`
- `Source/Tests/Test_ModelBaker.cpp`
- `Source/Tests/Test_ModelAnimationRuntime.cpp`

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

Script GUI `ItemView` widgets cache the item handles bound to their cells. `Resort()` keeps a cell only when the source returns the same handle instance; a replacement clone with the same entity id is rebound so item draw callbacks observe its current count and other projected properties.

## 3D model runtime architecture

The former `3dAnimation` and `3dStuff` umbrella modules have been removed. The
client 3D path now uses same-named `Model*.h` / `Model*.cpp` module pairs with
one ownership boundary per module. There is no client-side `ModelMesh` utility
module: mesh representation types live with `ModelHierarchy`. There is also no
separate `ModelPoseRuntime` module: renderer-independent pose/link operations
are part of `ModelAnimation`.

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

This split removes the old include-everything dependency. A caller includes the
manager, information, hierarchy, instance, animation, or baked-data contract it
actually consumes. Small passive types stay with their owning module instead of
forming empty translation units.

`ModelAnimationRuntimeClip`, `ModelAnimationRuntimeRig`, and
`ModelAnimationRuntimePose` keep their backend state behind PImpl. The public
header exposes only engine-owned runtime values and spans; Ozz headers, archive
objects, sampling contexts, and matrix buffers stay in
`ModelAnimation.cpp`. `ModelAnimationData` defines the native versioned wire
contract shared with the baker, while
`ModelAnimationConverter` owns offline conversion. The engine API does not name
or model interchangeable animation backends: Ozz is an implementation detail of
the engine's native model-animation runtime and baked format.

## Critter model animation

3D critter models use separate body/action and movement animation controllers. `ModelInstance::PlayAnim()` applies
animation-specific speed (`AnimSpeed`) to the body/action controller, while `RefreshMoveAnimation()` assigns gait and
movement-speed scaling to the movement controller's track. When both are active, the movement controller advances with
the model base/link/global speed only; it must not inherit the current body action's `AnimSpeed`, otherwise fast actions
such as use/pick-up make the leg cycle run too quickly while the critter is moving.

An animation name prefixed with `~` plays the source clip in reverse: playback time `t` samples the clip at
`duration - fmod(t, duration)`, so an exact loop boundary restarts at the clip end. If interpolation is disabled, the
same nearest-key rule is applied at that reversed source time.

Every baked `.fo3d` has the required versioned `LFMODINF` schema-1 header and a
required `LFOZZRIG` payload generated for pinned `ozz-animation` 0.16.0.
`ModelInformation` strictly loads and owns one immutable
`ModelAnimationRuntimeRig`. Its PImpl contains the canonical Ozz skeleton,
unique clips, base/clip remaps, presence masks, nearest timelines, and resolved
state/action binding table. The loader rejects old unversioned files and any
partial or inconsistent rig; it never falls back to another payload after an
Ozz load error. The final mesh-only wire transition uses compatibility marker
`0.0.30` and requires a full resource rebake.

Ozz is the production animated-pose path. The body and movement controllers
advance timeline/event state only. Each registered animation stores direct Ozz
clip index/duration/reverse metadata and an immutable bound-joint set derived
from the clip presence mask. A joint is bound only when its canonical and exact
runtime names match, so this gate deliberately keeps a resource-renamed model
root out of a source-root animation. Per-track allowed-joint and transition-
suppression masks further filter those bindings before the track state feeds the
per-instance Ozz pose.
Ozz performs clip sampling, body blending,
movement-only joint replacement, procedural body/head pre-rotation, and
local-to-model evaluation. Each `ModelInstance` snapshots the resulting world
matrices, and skin palettes, linked children, particles, and bone queries read
that owning instance snapshot. Link-all attachments override only the matched
joint after evaluation and deliberately do not recompute its descendants,
preserving the established attachment order.

Canonical joint names, exact runtime lookup names, and name-to-index bindings
are independent of the physical `ModelBone` hierarchy. Base joints retain a
read-only physical bone for meshes and cuts; animation-contributed joints have
no `ModelBone` and are never materialized into the shared cached hierarchy.
The base root deliberately keeps its resource-path runtime alias, while
contributed joints use their canonical names, preserving exact legacy lookup
and root-animation suppression semantics. Runtime particles, attachment
resolution, link-all matching, and bone-position queries operate on canonical
indices; link-all can therefore bind contributed joints without physical
bones. Authored one-bone link validation remains base-hierarchy-only in the
current baking schema.

Static `.fo3d` instances evaluate an empty Ozz track set so canonical rest and
procedural body/head transforms follow the same runtime as animated instances.
Only direct raw-model instances remain outside Ozz and build parent-ordered
world matrices through the validation helpers owned by
`ModelAnimation`.
The former custom pose evaluator and shared mutable matrix-output table have
been removed. Baked model meshes now begin with the mandatory `LFMODMSH`
schema-1 header and contain only the recursive hierarchy/bind/drawable mesh
payload. The client consumes it exactly and rejects headerless, mismatched,
truncated, or trailing data; no serialized TRS tail and no legacy mesh fallback
remain. Runtime clip identity, duration, joint presence, and sampling come
exclusively from the baked Ozz rig.

The nested LF archive hash detects accidental corruption but is not an
authentication mechanism. Ozz deserialization assumes the baked resource pack
is trusted; deployments that permit attacker-rewritable packs must authenticate
the pack before this loader runs.

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
  payload of the common `AnimationInfo` record with frame count, duration,
  directions, and resolved per-frame bounds; this payload remains available in
  builds without 3D support. `EngineMetadata` reads the matching compact
  version 1 `SpriteInfo/<PackName>.foinfo` indexes at startup, so common sprite metadata
  queries do not decode pixel payloads. A source-backed sprite uses its mesh for ordinary
  full-image draws; an explicit empty mesh skips the draw, while a missing/quad
  mesh keeps the four-vertex path used by runtime-generated atlas sprites.
- With `FO_ENABLE_3D`, `ModelSpriteFactory` turns model resources into
  atlas-backed sprites. It asks
  `EngineMetadata` for the already parsed version 2 aggregate, idle-priority
  view, and per-animation bounds from `ModelAnimationInfo.foinfo`; the client model
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
