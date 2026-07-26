# Particle Authoring And Runtime

FOnline provides two optional particle backends behind one client runtime:
SPARK and Effekseer. They have different authoring tools and source formats,
but both are compiled by `ParticleBaker`, exposed as particle sprites, and
controlled through the same `ParticleSystem` facade.

Use this guide for reusable Engine behavior. Use the generated
[particle-format reference](generated/particle-format/index.md) and
[canonical JSON model](generated/particle-format.json) for the exact
source-backed contract. An embedding project must separately document its
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

### Paths and rendering

`SparkQuadRenderer` stores the FOnline effect and texture names, draw bounds,
atlas dimensions, orientation data, and the `DrawInScene` route. Texture paths
must:

- be relative;
- contain no tab or line-control characters;
- remain inside the resource source that owns the `.spark` file.

Use the atlas route for ordinary sprite particles. Use `DrawInScene` when the
effect must share map projection and depth behavior with the scene. If any
renderer in the system requests direct-scene drawing, the complete particle
sprite uses that route.

Set realistic draw bounds for atlas particles. Bounds that are too small clip
the effect; bounds that are too large consume atlas space and fill rate. Effect
state and image decoding follow [Effect Format](EffectFormat.md) and
[Image And Sprite Formats](ImageFormat.md).

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
Effekseer Core before publishing `.efk`. Unsupported project features fail the
bake rather than silently degrading. Current exclusions include unsupported
dynamic equations, procedural models, GPU particles, advanced renderer values,
custom renderer data, and material files.

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
[Mapper Tools](MapperTools.md#interactive-particle-preview) for launch options.

## Runtime contract

`ParticleRuntimeBackend` owns extension routing, resource creation, and cache
invalidation. `ParticleRuntimeSystem` owns backend-specific simulation and
drawing. `ParticleSystem` adds the shared timing and control facade:

- `Setup` applies projection, world transform, position/view offsets, look
  direction, scale, map camera angle, and projection tilt;
- `Respawn(seed)` supports deterministic playback;
- `Prewarm` advances an effect before ordinary playback;
- `SetScale` reapplies runtime setup without replacing the effect;
- `GetDrawSize` and `GetDrawInScene` select the sprite allocation and rendering
  route;
- optional live bounds support model and scene visibility calculations.

The same seed is deterministic only with the same resource, backend revision,
setup, and update sequence. Do not use seeded replay as a cross-version visual
compatibility promise.

Effekseer currently uses direct-scene rendering. SPARK may use either atlas or
direct-scene rendering. Both routes still depend on the embedding project's
effect state, image resources, camera settings, and draw order.

## Integration

`ParticleSpriteFactory` exposes all enabled runtime extensions to the generic
sprite loader. A map sprite or client script therefore references a baked
`.spk` or `.efk` path and receives normal particle routing.

Client script exports provide seeded playback, prewarm, and scale controls.
`Critter.RunParticle` starts a live particle on a model bone in 3D-enabled
builds.

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
transform from the owning joint. See [Model Format](ModelFormat.md) for the full
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
