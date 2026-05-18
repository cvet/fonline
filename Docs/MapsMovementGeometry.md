# Maps, Movement, and Geometry

This document explains the reusable map-coordinate, movement, path-finding, line-tracing, and map-loading primitives used by client/server runtime and tools.

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

## Geometry modes

`hdir` is compiled differently depending on `FO_GEOMETRY`:

- `FO_GEOMETRY == 1` — six directions: north-east, east, south-east, south-west, west, north-west.
- `FO_GEOMETRY == 2` — eight directions: the six above plus south and north.

`GameSettings::MAP_DIR_COUNT` participates in direction normalization. When changing geometry, inspect compile-time geometry settings, generated value types, path-finding tests, and any rendering code that projects map positions.

## Geometry helper responsibilities

`GeometryHelper` is a static utility class. It owns coordinate projection and directional math such as:

- map hex/tile to projected screen/map coordinate conversion: `GetHexPos()`, `GetHexPosCoord()`, `GetHexOffset()`;
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
- `PathFinding::TraceLine()`

`FindPathInput` is deliberately callback-driven. Runtime code supplies `CheckHex(mpos)` so map ownership, blockers, critters, gag items, and game-specific blocking policy stay outside the generic algorithm.

Important `FindPathInput` fields:

- `FromHex` / `ToHex` — requested route endpoints.
- `MapSize` — bounds for all checks.
- `MaxLength` — maximum BFS depth, normally derived from engine settings.
- `Cut` — stop when route is within this distance of target; `0` requires exact target.
- `Multihex` — radius for multihex actors.
- `FreeMovement` — allows line-tracer optimization for control steps.
- `CheckHex` — callback returning block/defer status.

`FindPathOutput` returns a result, direction steps, control steps, and possibly adjusted `NewToHex`.

## Blocking model

`HexBlockResult` expresses path-finding priority:

- `Passable` — hex can be used.
- `Blocked` — permanent blocker.
- `DeferGag` — blocked by a gag item; route through only after distance gap.
- `DeferCritter` — blocked by a critter; route through as last resort.

For multihex actors, `CheckHexWithMultihex()` checks the directional front arc and returns the worst blocker result across checked hexes.

When gameplay code changes blocker semantics, update the callback provider and tests; do not bake game-specific blocking rules into the generic path algorithm.

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

## Map loading

`Source/Common/MapLoader.h` exposes `MapLoader::Load()`:

```cpp
static void Load(
    string_view name,
    const string& buf,
    const EngineMetadata& meta,
    HashResolver& hash_resolver,
    const CrLoadFunc& cr_load,
    const ItemLoadFunc& item_load);
```

The loader parses a map buffer and calls engine/runtime-provided callbacks for critters and items:

- `CrLoadFunc(ident_t id, const ProtoCritter* proto, const map<string_view, string_view>& kv)`
- `ItemLoadFunc(ident_t id, const ProtoItem* proto, const map<string_view, string_view>& kv)`

This keeps file parsing generic while letting server/tools decide how loaded critters/items become live entities or editor objects.

Map resource production is adjacent to baking. For `MapBaker`, see [BakingPipeline.md](BakingPipeline.md).

## Tests to inspect

Relevant tests include:

- `Source/Tests/Test_Movement.cpp`
- `Source/Tests/Test_MapLoader.cpp`
- `Source/Tests/Test_MapBaker.cpp`
- `Source/Tests/Test_ServerMapOperations.cpp`
- geometry/path/line-tracing tests when present in the checkout.

## Change routing

- Coordinate/value-type changes: `Source/Common/Geometry.*` and generated metadata docs.
- Line tracing: `Source/Common/LineTracer.*` and `Source/Common/PathFinding.*`.
- BFS/path blocking behavior: `Source/Common/PathFinding.*` plus caller-provided blocker callbacks.
- Movement interpolation/state: `Source/Common/Movement.*`.
- Map file parsing: `Source/Common/MapLoader.*`.
- Map resource baking: `Source/Tools/MapBaker.*` and [BakingPipeline.md](BakingPipeline.md).
- Runtime map entity behavior: [ServerRuntime.md](ServerRuntime.md), [ClientRuntime.md](ClientRuntime.md), and [EntityModel.md](EntityModel.md).

## Validation checklist

1. Run movement/path/map-loader tests relevant to the changed algorithm.
2. Test both direct and multihex movement if blocker logic changes.
3. Validate `FO_GEOMETRY` assumptions when changing directions/projection.
4. Validate map loading with real baked map resources from an embedding project when parser behavior changes.
5. If movement state is replicated, validate network and client/server behavior; update [Networking.md](Networking.md) if packet/property flow changes.
6. If script-visible movement APIs change, update [GeneratedApiAndMetadata.md](GeneratedApiAndMetadata.md) and [Nullability.md](Nullability.md) if signatures/nullability changed.
