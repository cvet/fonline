---
layout: default
title: Maps, Movement, and Geometry
locale: en
document_id: maps-movement-geometry
permalink: /Docs/en/explanation/maps-and-movement.html
---

# Maps, Movement, and Geometry

This document explains the reusable map-coordinate, movement, path-finding, line-tracing, and map-loading primitives used by client/server runtime and tools.

Use [Map Format](../how-to/content/map-format.md) for authored `.fomap` sections, placement identity and ownership, mapper round-trip behavior, side-specific baking, and runtime content materialization. This page owns geometry and movement semantics.

Use it when changing `Source/Common/Geometry.*`, `LineTracer.*`, `Movement.*`, `PathFinding.*`, `MapLoader.*`, map baker behavior, or map/movement tests.

## Ownership model

The engine owns coordinate types, path algorithms, movement interpolation, and map-file loading mechanics. An embedding game project owns concrete maps, blocker layout, encounter rules, and gameplay decisions around movement.

Keep project-specific map content and quest/navigation rules outside this document.

## Coordinate and direction types

`Source/Common/Geometry.h` defines the exported value types used across runtime, generated API, and scripts:

- `mpos` — map hex/tile position stored as `int16 x` + `int16 y`.
- `msize` — map dimensions stored as `int16 width` + `int16 height`.
- `hdir` — discrete hex/tile direction.
- `mdir` — movement direction angle; convertible to/from hex direction.

`msize` provides checked/clamped helpers:

- `clamp_pos()` clamps raw coordinates into map bounds;
- `from_raw_pos()` asserts that raw coordinates are already in bounds.

Do not replace these types with generic `ipos`/`isize` in public map APIs without checking generated metadata and script-visible value layouts.

### Sub-hex offset convention

Sub-hex pixel offsets are measured relative to the hex **visual center**, bounded by
`±{MAP_HEX_WIDTH/2, MAP_HEX_HEIGHT/2}`. This single convention is used by critter `HexOffset`,
`MovingContext` start/end offsets, move-target offsets (`MoveToHex`), transparent-egg offsets, and the
value returned by `MapView::GetHexAtScreen` / `Client_Map_GetHexAtScreenPos`. A click/cursor offset is
therefore directly usable as a move or critter offset with no conversion. Keep new offset
producers/consumers on this convention.

Implementation note — there are two anchor points that must not be confused:

- `GeometryHelper::GetHexPos` / `MapView::GetHexMapPos` (and the per-`Field` `Offset` grid) return the
  hex cell **top-left** draw origin, stepping by full `MAP_HEX_WIDTH` / `MAP_HEX_LINE_HEIGHT`.
- Sprites (critters, items) anchor at the hex **visual center** = top-left `+ {MAP_HEX_WIDTH/2,
  MAP_HEX_HEIGHT/2}` (see `HexView::AddSprite`), and `MapView::GetHexScreenPos` returns that same
  visual center (where a centered critter's feet render).

Because the visual center is half a hex past the cell origin, any code that resolves or applies a
center-relative offset against `GetHexMapPos` must add `{MAP_HEX_WIDTH/2, MAP_HEX_HEIGHT/2}`:
`GetHexAtScreen` biases its lookup point by `-{half hex}` so `GetHexPosCoord` snaps to the hex whose
visual center is nearest and the returned offset is already center-relative and in range (no clamping);
`UpdateTransparentEgg` and the critter-based `SetTransparentEgg` add/subtract the same half-hex so a
stored center-relative egg offset renders at the intended point. `GetHexScreenPos(hex) + offset`
projects a `(hex, center-relative offset)` pair straight to screen with no further bias.

## Geometry modes

`hdir` is compiled differently depending on `FO_GEOMETRY`:

- `FO_GEOMETRY == 1` — six directions: north-east, east, south-east, south-west, west, north-west.
- `FO_GEOMETRY == 2` — eight directions: the six above plus south and north.

`GameSettings::MAP_DIR_COUNT` participates in direction normalization. When changing geometry, inspect compile-time geometry settings, generated value types, path-finding tests, and any rendering code that projects map positions.

`mdir` stores normalized angles and `hdir` stores discrete map directions. Use the shared conversion helpers when moving or reversing directions: square builds place the north direction at angle `0`, so hand-written angle bucketing must handle wraparound at `360`/`0`.

## Map camera projection

The map camera uses a world frame of `+X` right, `+Y` up, and `+Z` map-south, with one world unit equal to one pixel of hex spacing. Its parallel projection tilts the world around X by `MAP_CAMERA_ANGLE` (`arcsin(sqrt(3)/4)`): ground northing is foreshortened by `sin(angle)` and elevation by `cos(angle)`. Anchoring a ground point at `z = legacy_y / sin(angle)` makes `ProjectWorldToMap` reproduce the legacy `GetHexPos` position at elevation zero.

`ProjectWorldToMap` is the reference form without scroll or zoom. It returns map-space pixels in `.x/.y` and view depth in `.z`; larger depth is nearer the camera. Its `(.y, .z)` result is an orthonormal rotation of world `(Z, Y)`, not a shear.

`MakeMapCameraView` is the GPU form with scroll, zoom, and a `yaw_deg` orbit around the vertical axis. At zero yaw it reproduces `(ProjectWorldToMap(world).xy - scroll) * zoom` with unchanged depth. The backend ortho composes over this matrix, so sprites, 3D models, and particles share one world-to-clip transform. Map sprites also write world depth with `DepthFunc = LessEqual`; CPU painter order still handles blended layers while the shared depth buffer resolves cross-type occlusion. `Test_Geometry.cpp` pins both projection forms against each other and against `GetHexPos`. It also pins `GetHexOffset(from, to) == GetHexPos(to) - GetHexPos(from)`, which makes a view-origin change a uniform pixel translation; `MapView` relies on that identity when shifting cached light primitives during scroll.

## Geometry helper responsibilities

`GeometryHelper` is a static utility class. It owns coordinate projection and directional math such as:

- map hex/tile to projected screen/map coordinate conversion: `GetHexPos()`, `GetHexPosCoord()`, `GetHexOffset()`;
- hex/offset canonicalization: `NormalizeHexOffset()` rewrites a projected point to the nearest in-bounds hex plus a small local offset;
- axial-coordinate helpers: `GetHexAxialCoord()`, `GetAxialHexes()`;
- distance and direction: `GetDistance()`, `GetHexDir()`, `GetDirAngle()`, angle-difference helpers;
- radius and line/circle checks: `HexesInRadius()`, `IntersectCircleLine()`;
- movement stepping: `MoveHexByDir()`, `MoveHexByDirUnsafe()`, `MoveHexAroundAway()`;
- multihex traversal helpers: `ForEachMultihexLines()`.

Safe helpers check `msize` bounds; unsafe helpers are for internal algorithms that already proved bounds.

## Path finding

`Source/Common/PathFinding.h` exposes the core path-finding API:

- `FindPathInput`
- `FindPathOutput`
- `TraceLineInput`
- `TraceLineOutput`
- `PathFinding::CheckHexWithMultihex()`
- `PathFinding::FindPath()`
- `PathFinding::EvaluateFreeMovementEndOffset()`
- `PathFinding::TraceLine()`

`FindPathInput` is deliberately callback-driven. Runtime code supplies `CheckHex(mpos)` so map ownership, blockers, critters, gag items, and game-specific blocking policy stay outside the generic algorithm.

Important `FindPathInput` fields:

- `FromHex` / `ToHex` — requested route endpoints.
- `ToHexOffset` — the target's real sub-hex offset within `ToHex` (the continuous target position is `ToHex` center + `ToHexOffset`). Used only by the `FreeMovement` end-offset computation.
- `MapSize` — bounds for all checks.
- `MaxLength` — maximum BFS depth, normally derived from engine settings. The
  temporary search grid clamps its half-extent to the map dimensions, so a very
  large limit on a small map does not allocate a `(2 * MaxLength)^2` buffer or
  change the selected route.
- `Cut` — stop when route is within this distance of target; `0` requires exact target.
- `Multihex` — radius for multihex actors.
- `FreeMovement` — enables the line-tracer optimization for control steps and the continuous sub-hex end offset (see below).
- `CheckTarget` — optional exact-goal predicate for multi-target searches. When set, it replaces
  the single `ToHex` / `Cut` goal check; the first goal reached by BFS is returned in `NewToHex`.
- `CheckHex` — callback returning block/defer status.

`FindPathOutput` returns a result, direction steps, control steps, the (possibly cut-adjusted) `NewToHex`, and `EndHexOffset` (concrete `ipos16`, zero when FreeMovement is off).

`MapManager::FindPathToAny()` builds an indexed target set and supplies it through `CheckTarget`,
so a server caller can find the nearest reachable exact target with one BFS instead of launching a
full path search for every candidate. The server script `Map.FindPathToAny(...)` overloads expose
the same operation for a raw start hex or a critter and return both the selected target and route
length through output arguments.

Backtracking must enumerate `GameSettings::MAP_DIR_COUNT` through `GeometryHelper::MoveHexByDirUnsafe()` instead of hard-coding the six hex-neighbor offsets. Hexagonal builds compile six directions, while square builds compile eight; using the shared direction helpers keeps both BFS expansion and path reconstruction on the same geometry rules.

### FreeMovement end offset

When `FreeMovement` is set, the route is still cut to whole hexes by the BFS, but the final
standing position is refined to a sub-hex point instead of snapping to `NewToHex` center.
`PathFinding::EvaluateFreeMovementEndOffset()` computes `EndHexOffset` (relative to `NewToHex`
center) so the continuous end position sits exactly at the cut gap
`R = dist(NewToHex center, ToHex center)` from the target's real position
(`ToHex` center + `ToHexOffset`), on the `NewToHex` side of it. Distances use the camera `Y`
projection (`GeometryHelper::GetYProj()`), matching the metric `MovingContext` uses for segment
distances. Behavior:

- `EndHexOffset` is a plain `ipos16`: when `FreeMovement` is off (or the request short-circuits
  before the FreeMovement pass) it stays at its zero default, meaning the mover stops at the
  reached hex center.
- A centered target reached short of its hex (`ToHexOffset == 0`, `Cut > 0`) yields
  `EndHexOffset == 0` (hex center).
- `Cut == 0` onto an off-center target yields `EndHexOffset == ToHexOffset` (stop exactly on the
  target point).
- The offset magnitude is bounded by `|ToHexOffset|` and clamped to half a hex.
- Degenerate stop direction. Internally `EvaluateFreeMovementEndOffset` returns `optional<ipos16>`
  and yields `nullopt` when the real target essentially coincides with the reached hex center
  (e.g. `Cut == 0` onto a centered target, or `Cut > 0` with a target offset that exactly cancels
  the inter-hex vector). In that case `FindPath` substitutes `FindPathInput::FromHexOffset` — the
  mover's own current sub-hex offset — so the mover stays where it already is instead of being
  snapped to the reached hex center. `MapView::FindPath` and `MapManager::FindPath` fill
  `FromHexOffset` from the supplied critter automatically.

Callers supply `ToHexOffset` and consume `EndHexOffset` directly: on the client through
`MapView::FindPath` plus the cut-aware `Critter.MoveToHex(hex, cut, hexOffset, speed)` script
overload; on the server through `MapManager::FindPath` plus
`Critter.MoveToHex(hex, cut, endHexOffset, speed)`. The resolved offset travels in the existing
`SendCritterMove` end-offset field (a concrete `ipos16`), so client prediction and server
authority stop at the same continuous point and there is no protocol change.

## Blocking model

`HexBlockResult` expresses path-finding priority:

- `Passable` — hex can be used.
- `Blocked` — permanent blocker.
- `DeferGag` — blocked by a gag item; route through only after distance gap.
- `DeferCritter` — blocked by a critter; route through as last resort.

For multihex actors, `CheckHexWithMultihex()` checks the directional front arc and returns the worst blocker result across checked hexes.

When gameplay code changes blocker semantics, update the callback provider and tests; do not bake game-specific blocking rules into the generic path algorithm.

Server-side `Map::IsHexMovable()` / `IsHexShootable()` combine two grids: the map's own `Field`, recomputed by `RecacheHexFlags()` from dynamic items and manual blocks, and the static `StaticMap::Field` for the same hex. The static half is read through `Map::GetStaticField()`, which is where per-instance static item removal is applied — see below.

## Static item removal

Baked static items live in `StaticMap` (`Source/Server/StaticMap.h`), which `MapManager` keys by `ProtoMap` and shares across **every** live instance of that map. A map instance can still drop individual static items, and it does so without touching that shared data.

**Removal is one-way for the life of the map instance.** `RemovedStaticItemIds` only ever grows: taking an id back out is refused by a property *setter*, which runs before the value is stored, so the write fails and both the stored list and the overlay stand unchanged. Do not expect an item to reappear on maps players already have loaded — the live client path is only ever told to drop a static item, never to build one back, and a rejection after the store would have persisted a shrunk list that silently undid the removal on the next server start. `MapManager::RegenerateMap()` regenerates map *content* and leaves removals in place; a map that needs its static layer whole again is a new map instance.

The mechanics:

- `Map::RemovedStaticItemIds` (`Common Mutable PublicSync Persistent`) is the stored list. It is the whole contract: persistence, client sync, and script visibility all follow from the property.
- `Map` derives three caches from it in `RefreshRemovedStaticItems()` → `RebuildStaticOverlay()`: the removed-id set, a `vector` of the surviving static items, and a `StaticMap::Field` override for each hex a removed item covered. All three stay empty while the map keeps every baked item, so an untouched map reads the shared grid with no extra indirection.
- `Map::VerifyStaticItemRemovalsOnlyGrow()` is the append-only guard, reached through the `ServerEngine::OnSetMapRemovedStaticItems` setter. It mutates nothing and only throws, so a refused write leaves the map exactly as it was.
- `Map::GetStaticField()` returns the override when one exists and the shared cell otherwise. Every static query (`GetStaticItem`, `GetStaticItemOnHex`, `GetStaticItems`, `GetStaticItemsOnHex`, `GetStaticItemsInRadius`, `GetTriggerStaticItemsOnHex`, `IsTriggerStaticItemOnHex`) and both blocking queries go through it, so a removed item is gone from movement, shooting, triggers, and lookup alike.
- `StaticMap::ForEachItemHex()` and `StaticMap::ApplyItemToField()` are shared by the loader and by the overlay rebuild, so the two can never disagree about which hexes an item contributes to or what blocking it implies.
- `StaticMap::Field::ScrollBlocked` exists for this rebuild. The loader's scroll-block pass writes `MoveBlocked` with no owning item, so an override rebuilt purely from the surviving items would silently open the map border; the rebuild seeds `MoveBlocked` from `ScrollBlocked` first.
- Static items carry the `ident_t` their map file authored (`MapManager::LoadFromResources`), which is what `GetStaticItem()` looks up, what the client's `ItemHexView` carries, and what the removal list records.

`EntityManager::CallInit(Map, bool)` builds the overlay once per map — that hook covers both a freshly created map and one restored from the database. Runtime changes come through the `ServerEngine::OnPostSetMapRemovedStaticItems` property post-setter, so a script that writes the property directly gets the same rebuild as one that calls `RemoveStaticItem()`.

On the client there are exactly two paths, and the map view holds no state of its own for this. `LoadStaticData()` skips an id in the removed list outright, so the item is never constructed, fielded, drawn, or indexed — the record is still walked past, because the baked entries are variable length and the reader has no index to seek with. `ClientEngine::OnSetMapRemovedStaticItems` calls `MapView::ApplyStaticItemRemovals()`, which destroys the now-removed views through the ordinary `DestroyItems()` path. There is no third path: nothing on the client ever rebuilds a static item on a loaded map.

## Line tracing

`TraceLineInput` describes a trace from `StartHex` toward `TargetHex`:

- `MaxDist` limits trace length; `0` means use start/target distance.
- `Angle` can override/drive direction.
- `CheckLastMovable` asks the trace to report the last movable hex.
- `IsHexBlocked` stops the trace.
- `IsHexMovable` optionally records move-valid candidates.

`TraceLineOutput` reports whether the trace was full, whether a last movable hex exists, the pre-block hex, block hex, and last movable hex.

Line tracing is used by movement/path logic and by gameplay systems that need visibility, shooting, or straight-line movement checks.

## Movement contexts

`Source/Common/Movement.h` defines movement state and interpolation:

- `MovingState` — completion/error reason.
- `MovingMetrics` — end hex, whole time, whole distance.
- `MovingProgress` — current hex, offset, direction, completion flag.
- `MovingRawProgress` — internal segment/progress data.
- `MovingContext` — ref-counted movement plan and runtime evaluator.

`MovingContext` stores:

- map size;
- speed;
- direction steps;
- control steps;
- start time and offset time;
- start/end hex and offsets;
- computed whole time/distance;
- completion state and block/pre-block hexes.

Key operations:

- evaluate: `EvaluateMetrics()`, `EvaluateProjectedHex()`, `EvaluateNearestPathHex()`, `EvaluatePathHexes()`, `EvaluateProgress()`;
- advance time: `UpdateCurrentTime()`, `UpdateCurrentTimeToNextHex()`;
- mutate runtime state: `ChangeSpeed()`, `Complete()`, `SetBlockHexes()`;
- sanity check: `ValidateRuntimeState()`.

Movement is therefore a reusable time-based plan, not just a list of positions. Client prediction, server correction, and script-visible movement data should all preserve that distinction.

Server and client runtime processing keep `MovingContext` active regardless of `CritterCondition`. Game scripts own condition-based movement permissions, so a game can represent knockout falls, dead-body slides, or custom state movement while still relying on the same path, offset, and completion state machinery. Attached critters are still stopped by runtime processing because attachment is a transport/ownership relationship rather than a critter condition.

## Map loading

`Source/Common/MapLoader.h` exposes `MapLoader::Load()`:

```cpp
static void Load(
    string_view name,
    string_view file_name,
    const string& buf,
    const EngineMetadata& meta,
    HashResolver& hash_resolver,
    const CrLoadFunc& cr_load,
    const ItemLoadFunc& item_load);
```

The loader parses a map buffer, uses `file_name` in source diagnostics, and calls engine/runtime-provided callbacks for critters and items:

- `CrLoadFunc(ident_t id, ptr<const ProtoCritter> proto, ptr<const map<string_view, string_view>> kv)`
- `ItemLoadFunc(ident_t id, ptr<const ProtoItem> proto, ptr<const map<string_view, string_view>> kv)`

This keeps file parsing generic while letting server/tools decide how loaded critters/items become live entities or editor objects.

`MapLoader::EnumerateMaps(file_name, buf)` returns the map names declared by a source file without materializing their entities. Keep source-name diagnostics and multi-map enumeration covered when changing the parser.

Map resource production is adjacent to baking. For `MapBaker`, see [Baking Pipeline](content-pipeline/baking.md).

## Tests to inspect

Relevant tests include:

- `Source/Tests/Test_Geometry.cpp`
- `Source/Tests/Test_PathFinding.cpp`
- `Source/Tests/Test_Movement.cpp`
- `Source/Tests/Test_MapLoader.cpp`
- `Source/Tests/Test_MapBaker.cpp`
- `Source/Tests/Test_ServerMapOperations.cpp`

## Change routing

- Coordinate/value-type changes: `Source/Common/Geometry.*` and generated metadata docs.
- Line tracing: `Source/Common/LineTracer.*` and `Source/Common/PathFinding.*`.
- BFS/path blocking behavior: `Source/Common/PathFinding.*` plus caller-provided blocker callbacks.
- Movement interpolation/state: `Source/Common/Movement.*`.
- Client 2D walk/run presentation over movement interpolation: [Sprite Root Motion](../how-to/content/sprite-root-motion.md).
- Map file parsing: `Source/Common/MapLoader.*`.
- Map resource baking: `Source/Tools/MapBaker.*` and [Baking Pipeline](content-pipeline/baking.md).
- Runtime map entity behavior: [Server Runtime](runtime/server.md), [Client Runtime](runtime/client.md), and [Entity Model](entity-and-property-model/).

## Validation checklist

1. Run movement/path/map-loader tests relevant to the changed algorithm.
2. Test both direct and multihex movement if blocker logic changes.
3. Validate `FO_GEOMETRY` assumptions when changing directions/projection.
4. Validate map loading with real baked map resources from an embedding project when parser behavior changes.
5. If movement state is replicated, validate network and client/server behavior; update [Networking](authority-and-networking/) if packet/property flow changes.
6. If script-visible movement APIs change, update [GeneratedApiAndMetadata.md](../reference/metadata/index.md) and [Nullability.md](../../Nullability.md) if signatures/nullability changed.
