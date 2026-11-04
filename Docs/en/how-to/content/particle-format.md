---
layout: default
title: Particle Authoring And Runtime
locale: en
document_id: particle-format-guide
permalink: /Docs/en/how-to/content/particle-format.html
---

# Particle Authoring And Runtime

FOnline provides two optional particle backends behind one client runtime:
SPARK and Effekseer. They have different authoring tools and source formats,
but both are compiled by `ParticleBaker`, exposed as particle sprites, and
controlled through the same `ParticleSystem` facade.

Use this guide for reusable Engine behavior. Use the generated
[particle-format reference](../../reference/particle-format/index.md) and
[canonical JSON model](../../../generated/particle-format.json) for the exact
source-backed contract. Use
[Particle Authoring Tools](../tools/particle-authoring.md) for the operational
Mapper/SPARK/Effekseer/Viewer workflow and versioned screenshots. An embedding
project must separately document its
enabled backend, particle catalog, visual policy, resource provenance,
performance budgets, and acceptance scenes.

## Build-time selection

Both backends default to `OFF`. Select them explicitly before project
generation:

```cmake
SetOptionValues(
    FO_SPARK_PARTICLES ON
    FO_EFFEKSEER_PARTICLES OFF
)
```

The options are independent:

- `FO_SPARK_PARTICLES` enables `.spark` baking, `.spk` runtime loading, and the
  Mapper SPARK authoring subeditor.
- `FO_EFFEKSEER_PARTICLES` enables `.efkproj` compilation and `.efk` runtime
  loading.

`CreateParticleRuntimeBackends` is the feature-aware composition point. The
backend-neutral `ParticleManager`, `ParticleSystem`, `ParticleSprite`, and
Mapper preview do not choose a backend themselves; they discover the runtime
extensions reported by the enabled backends.

Do not enable a backend merely because the Engine can compile it. A production
project should ship only backends covered by its content, platform, packaging,
performance, and visible-rendering tests.

## Resource pipeline

The authored and runtime forms are deliberately different:

| Backend | Authored source | Baked runtime | Runtime reference |
|---|---|---|---|
| SPARK | `.spark` XML | `.spark` -> `.spk` | `.spk` |
| Effekseer | `.efkproj` XML | `.efkproj` -> `.efk` | `.efk` |

Authored `.spk` and `.efk` files are rejected. Generated binaries belong only
in baking output and packages; all editable changes return to `.spark` or
`.efkproj`.

`ParticleBaker` participates in ordinary full and target-specific baking. A
runtime request never falls back to an authoring file. When a backend is
disabled, its source format is not baked and its runtime extension is not
advertised by `ParticleSpriteFactory`.

Both formats store simulated position bounds and maximum camera-facing billboard
radius (`.spk` or an Engine `.efk` trailer) for sprite framing and model visibility.
SPARK seeds emitter copying. Effekseer uses a nonnegative authored random seed
or a fixed fallback, local to the bounds simulation. Bounds do not depend
on process random state or effect order. One simulation is a sample, not a proof
that every random trajectory fits. Fully rebake existing Effekseer resources after
updating the baker; `.efk` without a valid trailer fails.

### Incremental Effekseer baking

Effekseer projects may reference textures, models, curves, and other files.
After a successful compile, the baker stores a project-and-dependency snapshot
under the baking cache. A changed, removed, or renamed dependency invalidates
each project that references it without forcing unrelated effects to rebuild.

The snapshot follows the physical directory source selected for the project.
Effekseer project sources therefore must come from a directory-backed resource
source, and dependency paths must remain relative and inside that source.
After changing `EffekseerCompiler` behavior, force a full resource bake so all
generated `.efk` files are refreshed.

## SPARK authoring

A `.spark` file is a SPARK object graph serialized as XML. The normal graph has
one `System`, one or more `Group` objects, emitters and modifiers, and the
Engine-owned `SparkQuadRenderer`. The baker:

1. loads the source through the vendored SPARK XML loader;
2. validates renderer texture paths;
3. serializes the graph to deterministic `.spk` binary data.

Use only object types registered by the Engine build and exposed by the Mapper
SPARK editor. An upstream SPARK type is not an FOnline authoring contract merely
because its implementation exists in the dependency.

The editor decodes texture previews through the same versioned
`SpriteResource` reader as the runtime-facing image path. It requires exactly
one direction and one frame, rejects shared-frame records, and reconstructs
the logical image when mesh cropping is present. Do not duplicate baked sprite
magic values or private byte offsets in a tool parser.

### Paths and rendering

`SparkQuadRenderer` stores the FOnline effect and texture names, atlas
dimensions, orientation data, and the `DrawInScene` route. Texture paths must:

- be relative;
- contain no tab or line-control characters;
- remain inside the resource source that owns the `.spark` file.

Use the atlas route for ordinary sprite particles. Use `DrawInScene` when the
effect must share map projection and depth behavior with the scene. If any
renderer in the system requests direct-scene drawing, the complete particle
sprite uses that route.

There is no manual `draw size` attribute. Baking runs a throwaway copy across
its bounded simulated lifetime and rejects a SPARK system that never produces a
visible particle. The measured position box and billboard radius determine the
atlas frame automatically. Effect state and image decoding follow
[Effect Format](effect-format.md) and [Image And Sprite Formats](image-format.md).

## Effekseer authoring

Author Effekseer effects as text `.efkproj` files with the bundled standalone
Effekseer Editor. Its Windows payload is built outside the normal Engine target
graph through:

```powershell
$env:FO_OUTPUT = (Get-Location).Path
python BuildTools\buildtools.py build-auxiliary effekseer-editor Release
```

The exact arguments are part of the BuildTools CLI and may be wrapped by an
embedding project's tasks. The authoring editor is not a runtime dependency and
must not be included in production game packages.

`EffekseerCompiler` consumes the project XML through a fixed supported profile,
emits raw `SKFE` data, and `ParticleBaker` verifies the result with the vendored
Effekseer Core before publishing `.efk`. Supported drawing nodes are `Sprite`,
`Ring`, `Ribbon`, `Track`, and `Model`; `Root` and `None` may organize the
graph. The callback renderers support:

- Normal, Add, and Sub blending;
- nearest or linear filtering and Clamp or Repeat wrapping;
- per-node depth test/write, with Engine effect depth variants;
- stable camera-depth sorting for Sprite and Ring;
- static Model resources and per-model face culling;
- scene distortion on Sprite nodes through a deferred background snapshot.

Unsupported features fail closed instead of silently degrading. The runtime
rejects GPU particles, normal textures, sounds, custom materials, external
curves, procedural models, multiply blending, mirrored wrapping, advanced
texture/material slots, soft particles, falloff, flipbook interpolation,
Z-sorted strips or models, and distortion on non-Sprite nodes. A referenced
static model must also pass Engine validation.

The editor's preview proves authoring-tool behavior only. The FOnline runtime
uses Effekseer CPU simulation with Engine-owned callback geometry, effects,
textures, depth, and graphics backends. Always repeat the check in Mapper and a
real client scene.

## Mapper workflow

Mapper has two distinct particle tools:

- **Particle preview** is backend-neutral. It lists baked `.spk` and `.efk`
  resources reported by enabled backends and runs them through the same
  `ParticleSystem` and renderer used by the client.
- **SPARK particle editor** browses raw `.spark` sources, edits their graph,
  saves XML, rebakes the matching `.spk`, invalidates cached data, and recreates
  the preview.

The preview supports resource filtering, placement at the mouse hex or map
center, restart, removal, explicit seed, transient scale and offset, and
optional prewarm. Its temporary `MapSprite` is not serialized and must not dirty
the map.

Use the standalone Effekseer Editor for `.efkproj`; Mapper does not edit those
sources. Mapper is nevertheless the required final authoring preview because it
exercises the generated `.efk` and the real FOnline rendering bridge. See
[Particle Authoring Tools](../tools/particle-authoring.md) for the complete
operator workflow and [Mapper Tools](../tools/mapper.md#interactive-particle-preview)
for automation/integration details.

## Runtime contract

`ParticleRuntimeBackend` owns extension routing, resource creation, and cache
invalidation. `ParticleRuntimeSystem` owns backend-specific simulation and
drawing. `ParticleSystem` adds the shared timing and control facade. Its explicit `Update(timespan)` and `GetElapsedTime()` preserve nanoseconds; backend update/prewarm adapters use seconds:

- `Setup` applies projection, world transform, position/view offsets, look
  direction, scale, map camera angle, and projection tilt;
- `Respawn(seed)` supports deterministic playback;
- `Prewarm` advances an effect before ordinary playback;
- `SetScale` reapplies runtime setup without replacing the effect;
- `GetBakedBounds` exposes the validated position box and billboard radius;
- `GetLiveBounds` retains transformed baked bounds while instances live and
  extends them with current Effekseer instance positions and shape radii;
- `ComputeSpriteFrame` projects baked bounds through the map camera and derives
  sprite allocation, emitter offset, and world transform;
- `GetDrawInScene` selects the atlas or direct-scene rendering route.

Standalone scene sprites start with identity World before their first simulation
tick, including a switch from atlas rendering. `DrawInScene` supplies the map
projection later. Birth placement therefore excludes atlas-frame translation;
atlas sprites retain their computed frame transform. Seeded respawn uses the
selected placement even before a draw.

`ParticleBounds3D` deliberately separates two quantities. The position box
follows the complete emitter placement. The billboard radius follows placement
scale but remains camera-facing, so callers add it as view-plane padding and do
not rotate it as another world-space point. Attached model particles expand the
model frame only while their backend reports live particles or instances.

`ModelInstance::RunParticle` requests a redraw and queues the effect until a
complete bone pose is available. Its first placement uses `Setup` followed by
`Update(0)`, so the pose interval before creation does not age the newborn.
Animation-callback effects receive this initialization before the current draw,
including requests on already posed children. A newly created child or changed
atlas layout waits for its next valid pose. Pending effects neither draw nor
expand the frame at identity transforms. Later positive pose deltas advance
normally; zero-time frame-sizing re-poses do not age the effect.

Effekseer model placement treats `ParticleRuntimeSetup.World` as the complete
bone pose. A finite `LookDirectionAngle` is accepted as a legacy hint without
applying a second yaw; a non-finite hint rejects the effect. Orient the effect
through `World` or its authored nodes.

For emitters bound at particle creation, moving or shrinking the emitter does
not move or shrink older particles. Live bounds therefore union the current
baked envelope with Sprite/Ribbon/Ring/Track/Model instance geometry. A cached
CPU callback pass measures those instances without GPU packets, scene-background
capture, simulation advancement or random generation. Expired particles stop
extending the frame; the existing model texture-size cap still applies.

`RebaseWorldParticles(delta)` changes the internal atlas origin, separately from
ordinary emitter movement. The model pairs it with its new absolute `Setup`
and zero-time `RefreshRenderTransform`; old particles translate once while a
compensating simulation root keeps future births at the current emitter.
Non-finite deltas or overflowing accumulated coordinates are ignored before
mutation. Respawn and handle retirement clear the old translation base.

Every active Effekseer render refresh still runs `UpdateHandle(0)`. The shared
Manager reuses its rendering snapshot while placement and handles are unchanged;
creation or placement changes from any handle require the next refresh to `Flip`.
Deferred rebase remains pending on its own handle until its SDK matrices are
applied; another handle cannot consume that publication. The refresh decides
whether to `Flip` after applying those matrices.
Root-only or empty handles after simulation or prewarm also require that next
`Flip`, because it checks effect completion before retiring the snapshot.
Ordinary `Update`, prewarm and the three retirement flushes always `Flip`.
Projection-only changes use the current draw parameters without advancing time.

The same seed is deterministic only with the same resource, backend revision,
setup, and update sequence. Do not use seeded replay as a cross-version visual
compatibility promise.

Authored SRT transforms may collapse individual scale axes to zero. Effekseer's
local `Mat43f` patch completes their rotation basis without dividing by zero:
surviving axes retain their signed direction, one missing axis uses their cross
product, and a lone survivor receives deterministic perpendicular axes. An
all-zero basis uses identity. The erased orientation is underdetermined; the
completion is canonical. Noncollapsed decomposition retains the upstream path.
This prevents invalid inherited Track transforms during zero-scale animation.
Non-finite callback geometry still fails runtime validation.

Standalone Effekseer particle sprites use direct-scene rendering. SPARK sprites
may use either atlas or direct-scene rendering. Model-bone particles follow the
model atlas/direct-scene route, including GUI atlas previews. Both routes still depend on the embedding project's
effect state, image resources, camera settings, and draw order.

## Integration

`ParticleSpriteFactory` exposes all enabled runtime extensions to the generic
sprite loader. A map sprite or client script therefore references a baked
`.spk` or `.efk` path and receives normal particle routing.

Client script exports provide seeded playback, prewarm, and scale controls.
`Critter.RunParticle` starts a live particle on a model bone in 3D-enabled
builds.

`Critter.RunUnlitParticle` uses the same bone, model-unit offset, first-pose
placement and lifetime, selecting the model's unlit atlas layer instead.
The model and ordinary particles retain map lighting; the extra layer keeps
the same crop, alpha and model depth with its authored node depth settings.
Both exports do nothing without a loaded model or 3D support, and throw off-map.
Direct scene rendering retains its existing all-particle route. See
[sprite overlays](../../explanation/rendering/index.md#optional-unlit-sprite-overlay).

`MapSpriteHolder.DepthOffset` is a finite logical-pixel view-depth correction:
zero default, positive nearer, unchanged screen XY in both render paths.
`Map.DrawMapSprite` copies it on attachment; field changes/rebuilds require
reattachment. NaN/infinity throw before any holder processing, even invalid/offscreen.

`Critter.GetBoneMapAnchor` projects authored bone-local units to integer offset
and `DepthOffset` for the critter's current hex. Both outputs use one retained
pose, actual carrier elevation/root depth and pixel rounding across atlas/direct.
Missing models/bones, detached sprites and invalid/unrepresentable points return
`false` with sprite offset/zero depth; off-map throws. Existing 2D methods stay
unchanged. Independent sprites follow later movement only if updated.

This query always preserves the bone point's physical view depth. A cached atlas
model is drawn as a bitmap plane, whose submitted depth can differ at the same XY.
Use `Critter.GetBoneSpriteMapAnchor` when an independent particle must meet the
displayed model: it uses the same bone pose, follows the submitted bitmap point
and plane in atlas mode and the physical point in direct scene mode. It returns
`false` with sprite offset/zero depth for a hidden carrier as well as unavailable
or invalid anchors. The physical query retains its existing meaning. Both queries
use authored bone-local units and require reattachment after movement or rebuild.

Update a following holder in `Game.OnRenderMap_BeforeSprites` when it must meet
the pose drawn in that map pass. Visible direct-scene critters are prepared before
this event; atlas poses have already been updated. Earlier loop callbacks can read
the preceding retained pose. Bone queries do not advance animation. Replayed direct
draws keep the prepared pose and only rebind particle projection, without advancing
particle time. Hidden or offscreen critters are not advanced by this preparation pass.

`Item.GetSpriteMapAnchor(hex, localOffset, mapOffset, depthOffset)` resolves the
visible bitmap copy at `hex`. `localOffset` is in logical sprite pixels relative
to its draw root before rotation and map projection. Set a holder's `Hex` to that
same hex and use both outputs as `Offset` and `DepthOffset`. The query follows the
submitted bitmap plane, including flat layers, authored roots and rounded elevation.
A valid out-of-view hex, unavailable copy, direct scene sprite, empty bitmap or
invalid/unrepresentable result returns `false` with zero outputs. Off-map items
and invalid hexes throw before changing the outputs. Pixel alpha and frontmost
picking remain caller-owned; refresh and reattach independent holders as required.

Model descriptions attach a baked particle with `AttachParticles`:

```text
Layer 8
Value 1
AttachParticles Particles/Jet.spk Link Backpack
MoveY 0.15
RotY 90
```

The model baker validates that the baked particle exists and that the target
bone is valid. The client creates an independent runtime system and updates its
transform from the owning joint. See [Model Format](model-format.md) for the full
attachment grammar.

## Production practices

- Pick one backend unless a migration or comparison has an explicit end date.
  Every additional backend multiplies package, platform, test, and support
  work.
- Keep authored sources and dependencies together with stable relative paths.
  Never hand-edit or version generated `.spk` and `.efk` files.
- Use explicit seeds in previews and regression scenes so visual comparisons
  are reproducible.
- Keep fast authoring preview separate from acceptance. Validate gameplay
  lifetime, transforms, depth, clipping, visibility, and frame cost in a
  representative client scene.
- Record asset provenance and licenses in the embedding project. Vendored
  runtimes do not grant rights to third-party particle samples.
- Define budgets for active systems, emitted particles, callback geometry,
  overdraw, atlas area, texture memory, and prewarm time.
- Treat warnings, missing textures, unsupported capabilities, non-finite
  geometry, and stale dependency output as release blockers.

## Validation

After changing the particle contract or documentation:

```powershell
python BuildTools\docs_particle_format.py --write
python -m unittest BuildTools.tests.test_docs_particle_format
python BuildTools\docs_contract_diff.py --base-ref <base>
```

After changing native particle code, configure both relevant feature lanes and
run the focused native tests, including `Test_ParticleBaker.cpp` and, for
Effekseer runtime changes, `Test_EffekseerParticleRuntime.cpp`.

Run each hidden `EffekseerManagerFailurePolicy` section in a separate native test
process. `ordinary_respawn` and `wrong_alignment_control` must exit 0;
`throw_bad_alloc` and `throw_unexpected` must exit 1 with their Engine fatal
diagnostic. These controls replace a process-wide allocator callback and must
not run concurrently in one process. Internal manager operations have no rollback
contract; resource validation, loaders and render callbacks retain their existing
failure behavior outside that fatal boundary.

An embedding project must then:

1. rebake the affected resources;
2. run its particle/content validation;
3. inspect the baked resource in Mapper;
4. inspect every affected sprite, map, script, and model-bone route in a visible
   client scene;
5. compare performance against its documented production budget.

A clean bake proves source conversion and static validation. It does not prove
that an effect looks correct, integrates with gameplay timing, or fits a
production frame budget.
