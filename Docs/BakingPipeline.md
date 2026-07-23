# Baking Pipeline

This document explains the engine resource baking pipeline: where it is wired, which source files own baker behavior, and how to validate changes. For the broader engine tool map, see [Tools.md](Tools.md).

Use this for reusable engine behavior. Game-specific content folder rules and product package policy belong in the embedding project's docs.

## Source paths inspected

- `BuildTools/cmake/stages/ScriptsAndBaking.cmake`
- `BuildTools/cmake/helpers/WriteBuildHash.cmake`
- `Source/Applications/BakerApp.cpp`
- `Source/Applications/BakerLib.cpp`
- `Source/Tools/Baker.h`
- `Source/Tools/Baker.cpp`
- `Source/Tools/BakingReport.h`
- `Source/Tools/BakingReport.cpp`
- `Source/Tools/MetadataBaker.h`
- `Source/Tools/MetadataBaker.cpp`
- `Source/Tools/ConfigBaker.h`
- `Source/Tools/ConfigBaker.cpp`
- `Source/Tools/RawCopyBaker.h`
- `Source/Tools/RawCopyBaker.cpp`
- `Source/Tools/ImageBaker.h`
- `Source/Tools/ImageBaker.cpp`
- `Source/Tools/SpriteMeshing.h`
- `Source/Tools/SpriteMeshing.cpp`
- `Source/Common/SpriteResource.h`
- `Source/Common/SpriteResource.cpp`
- `Source/Tools/EffectBaker.h`
- `Source/Tools/EffectBaker.cpp`
- `Source/Tools/ProtoBaker.h`
- `Source/Tools/ProtoBaker.cpp`
- `Source/Tools/MapBaker.h`
- `Source/Tools/MapBaker.cpp`
- `Source/Tools/TextBaker.h`
- `Source/Tools/TextBaker.cpp`
- `Source/Tools/ProtoTextBaker.h`
- `Source/Tools/ProtoTextBaker.cpp`
- `Source/Tools/ModelMeshBaker.h`
- `Source/Tools/ModelMeshBaker.cpp`
- `Source/Common/ModelMeshData.h`
- `Source/Common/ModelMeshData.cpp`
- `Source/Tools/ModelInfoBaker.h`
- `Source/Tools/ModelInfoBaker.cpp`
- `Source/Tools/ParticleBaker.h`
- `Source/Tools/ParticleBaker.cpp`
- `Source/Common/AnimationInfo.h`
- `Source/Common/AnimationInfo.cpp`
- `Source/Common/ModelBounds.cpp`
- `Source/Common/ModelBounds.h`
- `Source/Tools/ModelBoundsCalculator.h`
- `Source/Tools/ModelBoundsCalculator.cpp`
- `Source/Tools/ModelSourceLoader.h`
- `Source/Tools/ModelSourceLoader.cpp`
- `Source/Tools/ModelAnimationConverter.h`
- `Source/Tools/ModelAnimationConverter.cpp`
- `Source/Common/ModelAnimationData.h`
- `Source/Common/ModelAnimationData.cpp`
- `Source/Tools/AngelScriptBaker.h`
- `Source/Tools/AngelScriptBaker.cpp`
- `Source/Tests/Test_BakerSetup.cpp`
- `Source/Tests/Test_MetadataBaker.cpp`
- `Source/Tests/Test_ConfigBaker.cpp`
- `Source/Tests/Test_RawCopyBaker.cpp`
- `Source/Tests/Test_ImageBaker.cpp`
- `Source/Tests/Test_EffectBaker.cpp`
- `Source/Tests/Test_ProtoBaker.cpp`
- `Source/Tests/Test_ProtoTextBaker.cpp`
- `Source/Tests/Test_MapBaker.cpp`
- `Source/Tests/Test_TextBaker.cpp`
- `Source/Tests/Test_ModelBaker.cpp`
- `Source/Tests/Test_ParticleBaker.cpp`
- `Source/Tests/Test_ModelMeshData.cpp`
- `Source/Tests/Test_ModelAnimationData.cpp`
- `Source/Tests/Test_ModelAnimationConverter.cpp`
- `Source/Tests/Test_ModelSkeletonCompatibility.cpp`
- `Source/Tests/Test_ModelSourceLoader.cpp`
- `Source/Tests/Test_OzzAnimation.cpp`
- `Source/Tests/Test_AngelScriptBaker.cpp`

## What baking does

Baking turns project resources and configuration into runtime-ready output. The build pipeline creates `BakeResources` and `ForceBakeResources` command targets in `BuildTools/cmake/stages/ScriptsAndBaking.cmake`. Those targets run the project baker application with the embedding project's main config applied.

At runtime/source level, baking is owned by:

- `Source/Applications/BakerApp.cpp` — executable app wrapper that constructs `MasterBaker` and calls `BakeAll()`.
- `Source/Applications/BakerLib.cpp` — exported library entry point `FO_BakeResources()` for library-based baking flows. On Linux a linker export map makes this the shared library's only public symbol, and a post-build symbol check enforces that ABI. All allocator and engine implementation symbols bind locally, so loading a release baker into a sanitizer host cannot interpose on host allocation or global runtime state.
- `Source/Tools/Baker.h` / `Source/Tools/Baker.cpp` — shared baking context, baker setup, data source, output writing, and `MasterBaker`.
- `Source/Tools/BakingReport.h` / `Source/Tools/BakingReport.cpp` — report data contracts, thread-safe aggregation, JSON serialization, and report-path construction.

## CMake entry points

`BuildTools/cmake/stages/ScriptsAndBaking.cmake` creates baking commands after application targets are available.

Current target responsibilities:

- `BakeResources` runs the project baker with `-ForceBaking False`.
- `ForceBakeResources` runs the project baker with `-ForceBaking True`.
- Both apply the embedding project's main config through `-ApplyConfig <FO_MAIN_CONFIG>` and `-ApplySubConfig NONE`.
- Both work from `FO_OUTPUT_PATH`.
- Resource build-hash state is written through `BuildTools/cmake/helpers/WriteBuildHash.cmake` using `Baking/Resources.build-hash`.

The actual final target names that depend on these commands are project/preset-dependent. Do not document one embedding project's target names as universal engine behavior.

## Runtime classes

### `BakingContext`

Defined in `Source/Tools/Baker.h`. It carries shared bake state:

- `Settings` — `BakingSettings` for the current bake.
- `PackName` — current resource pack name.
- `BakeChecker` — callback used to decide whether existing baked data is still valid.
- `WriteData` — async output writer callback.
- `BakedFiles` — existing baked file data source.
- `ForceSyncMode` — optional override for synchronous execution.

For a `MasterBaker` pass, the context also carries the shared report collector
and the stable baker name used for attribution. `WriteData` returns a
`BakingWriteResult`, which distinguishes a content-changing write from an
unchanged file whose timestamp was refreshed.

### `BaseBaker`

The abstract base for individual baker implementations. Each baker provides:

- `GetName()` — stable baker name used by setup/config selection.
- `GetOrder()` — ordering key for deterministic bake ordering.
- `BakeFiles()` — the actual file transformation step.

`ModelInfoBaker` runs after `ParticleBaker`: model descriptions may reference
baked particle resources, so their link validation must not race particle
serialization during a clean or forced rebuild.

`BaseBaker::SetupBakers()` in `Source/Tools/Baker.cpp` creates requested bakers and then calls `SetupBakersHook()` so external/project code can extend the baker list.

Each baker receives its own copy of the shared context. When a master-bake report
is active, `BaseBaker` wraps that copy's check and write callbacks with the
baker name. This keeps output attribution correct even when a baker performs
work on its own asynchronous tasks. Built-in and project-provided bakers can add
domain-specific counters and histograms through the protected report helpers;
the helpers are no-ops when no report collector is attached.

### `MasterBaker`

`MasterBaker` coordinates a configured bake pass through `BakeAll()`. It is the app-facing type used by both `BakerApp.cpp` and `BakerLib.cpp`.

`MasterBaker` also owns the one report collector for that bake attempt and
finalizes the report after either success or failure.

### `BakingReport`

`BakingReport` is defined in its own `Source/Tools/BakingReport.h` / `.cpp`
module. The header owns the common report DTOs and write-result contract; the
implementation owns aggregation, sprite-mesh analysis, JSON construction, and
the output report path. `Baker.cpp` only drives its lifecycle and forwards bake
events to it.

### `BakerDataSource`

`BakerDataSource` adapts resource inputs/outputs to the engine `DataSource` interface. It tracks input resource packs, output resources, cache checks, and output path construction. `Reindex()` reconstructs its input mounts, baker instances, file collections, and output index, returning whether the indexed paths or source write times changed. Long-running tools can therefore discover and on-demand bake added or changed resources without replacing cached directory lookup with repeated disk scans. Its output-discovery dry runs and later lazy, per-file baking do not attach the master-bake report collector and are therefore deliberately absent from the report. During lazy output discovery it walks resource packs in the same order as `MasterBaker`, so cross-pack dependencies such as `ManagedScriptBaker` reading `Metadata.fometa-*` see earlier pack outputs; runtime file lookup still searches pack outputs in reverse order for normal resource precedence.

## Master bake report

Every `MasterBaker::BakeAll()` attempt with a non-empty `BakeOutput` regenerates
`Baking.report.json` in the output directory. A successful complete rebuild also
updates `Baking.full.report.json`:

```text
BakeOutput = Baking
runtime resource directory = Baking/
report = Baking/Baking.report.json
last complete-corpus report = Baking/Baking.full.report.json
```

The previous report is removed before baking starts. After the bake finishes,
`MasterBaker` marks the new report `success` or `failed`, serializes all data
collected up to that point, and writes it even when resource preparation or a
baker failed. `failureMessage` carries the caught exception text, and baker
entries that were registered but never reached remain `not_run`. Failure to
write the report is itself a failed `BakeAll()` result, so every successful
bake pass has its matching report.

Incremental and failed passes never overwrite `Baking.full.report.json`. The
full snapshot therefore remains available for corpus analysis after ordinary
incremental development bakes. Both report names are excluded from outdated
runtime-resource cleanup.

The report is written directly into the `BakeOutput` root after runtime-resource
cleanup finishes. It is never mounted in the baked `FileSystem`, registered as
a baked output, or consumed as a runtime resource. It is removed before the next
bake attempt starts, so analysis data does not change the resource set delivered
to a client or server.

The top-level lifecycle fields are:

- `schemaVersion`, currently `1`;
- `status` and `failureMessage`;
- `buildHash`, `bakeOutput`, and total `durationMs`;
- `mode.forceRequested`, `mode.fullRebuild`, `mode.rebuildReason`, and
  `mode.singleThread`. Rebuild reasons are `incremental`, `requested`,
  `build_hash_changed`, or `missing_build_hash`.

`measurementScope` prevents incremental samples from being mistaken for corpus
statistics. Input counts always describe the complete configured input
collection. Output activity and baker-specific details describe work performed
in the current pass. In particular, `Image.details.spriteMesh` measures only
frames rebuilt in this pass. `completeCorpusDetails` is true only when
`mode.fullRebuild` is true; run a force bake before using form percentages as a
distribution over the complete art corpus.

`totals` summarizes packs, distinct baker types, invocations, pack input files
and bytes, output checks, scheduled and up-to-date artifacts, submitted files
and bytes, changed and unchanged files, and removed outdated files. The same
data is available in two analysis views:

- `bakers` aggregates every baker name across all resource packs;
- `packs` contains input/output statistics, duration, and a nested baker entry
  for each individual resource pack.

Every baker entry has its order, `success`/`failed`/`not_run` state, invocation
counts, elapsed time, failure messages, and
`availableInputFiles`/`availableInputBytes`. The latter
describe the complete input collection visible to that baker, not only files
that match its own extension filter. Output statistics distinguish:

- `checked`: unique artifact paths passed to the incremental checker;
- `scheduled`: checked paths selected for rebuilding;
- `upToDate`: checked paths satisfied by the existing output;
- `cacheHitPercent`: up-to-date check calls divided by all check calls;
- `submitted`: unique paths produced by the baker;
- `changed`: submitted content written because its bytes changed;
- `unchanged`: byte-identical submitted content whose timestamp was refreshed.

Raw `checkCalls`, `scheduledCheckCalls`, `upToDateCheckCalls`, `submitCalls`, and
`submittedBytesAcrossCalls` remain available because some bakers intentionally
check or submit a path more than once. File
and byte groups include distributions by extension and the 25 largest paths.
The `details.counters` and `details.histograms` objects hold optional
baker-specific measurements while preserving the same common schema for every
built-in or externally registered baker.

### Image and sprite-mesh statistics

`ImageBaker` adds collection, direction, frame-slot, unique-frame, and shared
frame-reference counters plus a source-format histogram. Its detailed geometry
analysis is stored under `details.spriteMesh` in both the aggregate `Image`
baker entry and each per-pack `Image` entry.

The `settings` object records the effective `Enabled`, `AlphaThreshold`,
`MaxTriangles`, and `AreaSavingsWeight` values together with the internal
base-dilation and maximum-padding policies used by that bake. Per-frame
diagnostics separately record the actual dilation and simplification tolerance
of the selected candidate, so expanded detailed candidates are not reported as
if they used only the base dilation.

`frames` separates unique serialized frames from shared animation references.
The `mesh`, `quad`, and `empty` percentages use **unique, non-shared frames** as
their denominator; shared references are reported in `slots` and
`sharedReferences` but never inflate form percentages. The
`triangleHistogram` likewise reports both `percentOfMeshes` and
`percentOfUniqueFrames`. The vertex histogram uses mesh frames as its
denominator. Source-alpha connected components are counted for every unique
frame; dilated components have their own histogram and measured denominator.
Selected-candidate tolerance and actual-dilation histograms make fallback usage
visible without inspecting individual resources.

`selectionOrigins` explains which winning candidate family produced each mesh:

- `greedy_whole`, `greedy_components`, and `clustered_components`;
- `enclosing_triangle` and `enclosing_quad`;
- `detailed_constrained`, `detailed_simplified`, and `detailed_expanded`.

`quadReasons` explains why a unique non-empty frame retained quad geometry:

- `disabled` or `zero_dimensions`;
- `dilation_fills_frame`;
- `contour_extraction_failed` or `no_valid_candidate`;
- `score_preferred_quad`, when valid candidates existed but did not beat the
  quad score.

For `score_preferred_quad`, `bestRejectedCandidates` summarizes the best valid
candidate that lost to the quad: origin, triangle count, score, tolerance, and
actual-dilation distributions. The same candidate's vertices and area are
included in the affected top-list row. This is the direct diagnostic for
checking whether `AreaSavingsWeight` exchanges alpha area for triangles as
intended. Aggregate `geometry` and `area` blocks quantify the candidate
triangles, additional triangles over quads, recoverable frame area, and saved
pixels per additional triangle. Individual candidates also report the
`breakEvenAreaSavingsWeight` at which their area saving balances their triangle
cost.

`geometry` compares the actual submitted vertex and triangle counts (mesh
geometry plus four vertices/two triangles for every retained quad) with
all-quad baselines of four vertices and two triangles per unique frame. Empty
geometry submits no triangles, while the baseline intentionally represents how
the same frame slot behaved before
polygonal sprites. It also reports mesh-only vertices and triangles, the
triangle delta, and averages per mesh.

`area` keeps exact integer doubled areas as the canonical values, then adds
pixel-area and percentage views. It compares original unpadded quad area,
submitted geometry area, and visible pixels, and reports both total frame-area
savings and reduction of transparent overdraw. `cropping` reports how many
mesh frames serialize a smaller canvas than their source plus the saved RGBA
pixels and bytes. `padding` separately reports the serialized texture canvas,
frames that still expand beyond their source, added RGBA pixels and bytes,
padded-frame count, maximum padding, and the padding histogram. Expansion and
cropping are accumulated independently, so savings in one frame cannot hide
padding overhead in another.

The section also contains selection-score minimum/average/maximum values and
the fixed quad score, plus a per-resource classification (`mesh_only`,
`quad_only`, `empty_only`, or `mixed`). Five deterministic top-25 lists retain
the frame identity and all relevant geometry fields for direct investigation:

- `largestMissedSavings`: retained quads ranked by the absolute number of
  source-frame pixels that are not visible; this is a diagnostic opportunity
  estimate, not a claim that all of those pixels can be removed safely;
- `largestRejectedCandidateSavings`: score-rejected candidates ranked by the
  frame area they would actually save if selected;
- `mostComplexMeshes`: meshes ranked by triangle count, then vertex count;
- `largestCroppingSavings`: mesh frames ranked by serialized texture pixels
  removed by their exact geometry bounds;
- `largestPaddingOverhead`: frames ranked by added serialized canvas pixels.

Each top-list row includes separate source and actual baked-output paths,
direction, frame index, form, selection origin or quad reason, triangles,
vertices, source and dilated components, padding, chosen tolerance/dilation,
source and baked canvas pixels, visible pixels, submitted doubled area,
potential transparent pixels, padding overhead, cropping savings, and selection
score where one exists. Full tie-breaking includes both paths, direction, and
frame index, so the top lists remain deterministic under parallel baking.

During output discovery it visits resource packs in configured order so a later baker can read outputs declared by an earlier dependency (for example, model baking can load client metadata). When packs declare the same logical output path, the later registration replaces the earlier one; file resolution also searches packs from last to first, preserving the normal resource-overlay rule that later packs shadow earlier packs.

## Built-in baker types

`Source/Tools/Baker.cpp` registers built-in bakers when requested and enabled:

- `MetadataBaker` — `Source/Tools/MetadataBaker.*`
- `ConfigBaker` — `Source/Tools/ConfigBaker.*`, name `Config`, order `2`
- `RawCopyBaker` — `Source/Tools/RawCopyBaker.*`, name `RawCopy`, order `4`
- `ImageBaker` — `Source/Tools/ImageBaker.*`
- `EffectBaker` — `Source/Tools/EffectBaker.*`
- `ParticleBaker` — `Source/Tools/ParticleBaker.*`, name `Particle`, order `5`
- `ProtoBaker` — `Source/Tools/ProtoBaker.*`, name `Proto`, order `7`
- `MapBaker` — `Source/Tools/MapBaker.*`, name `Map`, order `8`
- `TextBaker` — `Source/Tools/TextBaker.*`, name `Text`, order `4`
- `ProtoTextBaker` — `Source/Tools/ProtoTextBaker.*`
- `ModelMeshBaker` — `Source/Tools/ModelMeshBaker.*`, enabled when `FO_ENABLE_3D` is active
- `ModelInfoBaker` — `Source/Tools/ModelInfoBaker.*`, order `6`, enabled when `FO_ENABLE_3D` is active
- `AngelScriptBaker` — `Source/Tools/AngelScriptBaker.*`, order `4`, enabled when `FO_ANGELSCRIPT_SCRIPTING` is active
- `ManagedScriptBaker` — `Source/Tools/ManagedScriptBaker.*`, name `Managed`, order `3`, enabled when `FO_MANAGED_SCRIPTING` is active. It runs before the `Proto` (7), `Map` (8), and dialog validators so the compiled managed assemblies exist when those bakers restore the managed script subsystem and resolve `[DialogDemand]`/`[DialogResult]` and other script funcs through `ScriptSystem::FindFunc`.

The particle/model/prototype/map stages intentionally form a strict dependency chain: particle outputs at order `5` are visible to model-info validation at order `6`, model descriptions are visible to prototype validation at order `7`, and baked prototypes are visible to map baking at order `8`. Bakers at the same order may run concurrently across resource packs and therefore must not consume one another's outputs.

When documenting a specific asset type, inspect the relevant baker class and its tests rather than inferring behavior from file extensions alone.

Shared animation metadata uses `AnimationInfo` as the aggregate record. The generic
record contains a `SpriteInfo` payload for 2D frame count, duration, directions,
and resolved per-frame bounds, plus a `ModelAnimationInfo` payload in
`FO_ENABLE_3D` builds for model and animation AABBs and typed animation
durations. `ReadSpriteResource` fills the sprite payload from the baked sprite
header and frame table. For metadata queries that must not load pixel payloads,
`ImageBaker` also writes one compact version 1
`SpriteInfo/<PackName>.foinfo` index per resource pack. The index is an
aggregate output over every image source in that pack; normal scan baking
merges changed entries with the existing complete index from the same pack's
output directory, while an explicit request for the index rebuilds every
entry. Pack-local previous outputs are mounted separately from the shared
cross-pack baked-file registry so a pack can read its own aggregate before its
first baker invocation. Introducing or losing the index is a full-rebake
condition rather than a reason to decode all sprite pixels at runtime. The
common `ReadAnimationInfo` path reads those 2D indexes in every
build, reads `ModelAnimationInfo.foinfo` when 3D is enabled, and merges both payloads
by resource name in `EngineMetadata`. Baker-local statistics follow the same
ownership names (`SpriteInfoBakingStats` and
`ModelAnimationInfoBakingStats`) instead of using the aggregate `AnimationInfo` name.

When `FO_ENABLE_3D` is active, `ModelInfoBaker` emits the regular baked model
descriptions together with a model-specific `ModelAnimationInfo.foinfo` companion. Bounds
schema version 2 adds three
required root-space contracts to every model section:

- `ModelBoundsMin*` / `ModelBoundsMax*` store the aggregate of all emitted
  animation envelopes, with exact static geometry used only when the model has
  no animation mappings;
- `ViewBoundsMin*` / `ViewBoundsMax*` store a deterministic reference envelope:
  `Unarmed + Idle` first, then any Idle, then the first valid animation or the
  static fallback;
- `BoundsStateAnimations` / `BoundsActionAnimations` and the parallel `BoundsMin*` /
  `BoundsMax*` arrays store the individual animation AABBs used by the runtime
  tight-crop predictor.

The baker samples animation keys, their midpoints, and a uniform timeline to
build deterministic envelopes independent of camera angle, projection factor,
model-sprite resolution, and renderer backend. Missing or invalid aggregate or
animation bounds are baking errors in the version 2 contract. In
`FO_ENABLE_3D` builds, the common `EngineMetadata` loader reads the companion
once at startup and strictly
validates its version, required bounds, and every parallel duration/bounds
array before publishing the parsed model records. Duration and animation-bounds
groups are optional and validated independently: durations use the alias-expanded
animation key domain, while bounds use the raw animation key domain, so either
group can legitimately appear without the other. A static section can carry
neither group; a present companion with no model sections is malformed.
Enabled animation bounds size
the logical scratch frame, the dedicated view bound seeds the stable body/name
rectangle, and aggregate bounds seed the horizontal-lighting reference. Runtime
layer/child-model envelopes extend both contracts, while exact weighted
current-pose geometry selects the atlas crop and expands/rerenders the scratch
frame when sampled bounds are insufficient.

`Source/Common/ModelBounds.h/.cpp`, guarded by `FO_ENABLE_3D`, owns the shared
root-space AABB contract used by the baker and client: finite/ordered validation, non-point extent checks,
point and bounds accumulation, eight-corner transformed accumulation, and the
common `max(0.01, maxAbs * 0.001)` guard. `ModelBoundsCalculator` is limited to
reading baked model data and sampling geometry; it does not maintain a parallel
bounds type or bounds-manipulation implementation. If the default link disables
every base mesh, the baker retries against the unfiltered source model as a
conservative layout envelope; genuinely empty or invalid geometry remains a
baking error.

`DrawSize` and `ViewSize` are no longer `.fo3d` grammar. `ModelInfoBaker`
rejects those removed tokens instead of serializing authored dimensions; frame
and view layout are runtime projections of the baked bounds. Because the
companion output is global, its incremental timestamp covers every input in the
pack, including animation FBX files referenced by `.fo3d` data. The `ModelInfo`
baking report records model sections, aggregate model bounds, duration entries,
animation bounds, view-bound idle/fallback selection, calculator/cache counts,
and a maximum-axis-extent histogram (`<1`, `1-2`, `2-3`, `3-5`, `5-10`, `10+`
model world units) for coverage and density analysis. Individual model
descriptions are validated and baked before the aggregate companion, so a
broken `.fo3d` is reported through the normal per-file diagnostic path and
cannot leave a newly written companion beside rejected model descriptions.

The companion also contains each model's effective `(state, action)` cycle
durations after authored `AnimSpeed` is applied. The duration arrays materialize
the same one-step `StateAnimEqual` / `ActionAnimEqual` resolution used by the
client model runtime, including alias priority over an exact entry with the
alias source key. Any pack can select `ModelInfo`; behavior never branches on
`PackName`. Put that pack on every runtime side that needs model metadata.
In `FO_ENABLE_3D` builds, `EngineMetadata` registers the complete parsed
model-animation records alongside prototypes at startup, including aggregate/view bounds, the alias-resolved
duration table, the raw-animation bounds table, and model names in hash storage.
The duration and bounds key sets intentionally remain separate because aliases
can produce duration-only keys while raw `.fo3d` entries can be bounds-only.
Client model code requests the parsed record instead of reopening the config;
common scripts query its duration table through
`Game.GetModelAnimDuration(modelName, stateAnim, actionAnim)`. The script method
returns a `timespan`, or zero when the resource, model, or resolved tuple is
absent. The config representation is an internal baker/runtime contract and
should not be parsed by embedding-project code.

## 3D model baking architecture

The model pipeline is split into source extraction, compatibility analysis,
conversion, native wire contracts, and the two final bakers. These are engine
responsibilities, not interchangeable animation-backend interfaces. The
animation-specific production code groups them into one same-named `.h`/`.cpp`
pair per owner: `ModelSourceLoader`, `ModelAnimationConverter`, and
`ModelAnimationData`.

| Module | Layer | Responsibility |
| --- | --- | --- |
| `ModelSourceLoader` | Tools | Backend-neutral source skeleton/clip/TRS data, `ufbx` import, complete source validation, and the per-bake single-flight `ModelSourceAssetCache`. |
| `ModelAnimationConverter` | Tools | Deterministic canonical hierarchy construction, compatibility diagnostics, contributed-joint/root-alias and parent analysis, and conversion into runtime rig artifacts using the pinned Ozz offline implementation internally. |
| `ModelAnimationData` | Common | Versioned little-endian LF animation envelopes and the native rig manifest: identity and signatures, canonical skeleton, base/clip remaps, clip payloads, presence/nearest data, and state/action bindings. |
| `ModelMeshData` | Common | Passive mesh-wire DTOs plus the versioned `LFMODMSH` reader, writer, and shared structural validation. It contains no runtime animation implementation. |
| `ModelMeshBaker` | Tools | Validates and writes mesh-only hierarchy, bind, vertex, index, influence, and drawable data. |
| `ModelInfoBaker` | Tools | Resolves `.fo3d` descriptions and dependencies, invokes source loading/compatibility/conversion, then writes `LFMODINF`, the required rig payload, and `ModelAnimationInfo.foinfo`. Compatibility failures remain bake errors; successful per-description compatibility reports are not emitted to the routine bake log. |

The main data flow is:

`FBX/.obj + .fo3d` -> `ModelSourceLoader` ->
`ModelAnimationConverter` -> `ModelAnimationData` -> `ModelInfoBaker` ->
`ModelInformation` / `ModelAnimation`.

Mesh data follows the parallel
`FBX/.obj` -> `ModelMeshBaker` -> `ModelMeshData` -> `ModelManager` /
`ModelHierarchy` path. The two streams meet in `ModelInformation`; clips and
mutable poses are never serialized into the shared mesh hierarchy.

The model-specific bounds and duration contract is described immediately above; the Ozz conversion changes its source representation, not the common `AnimationInfo` query surface.

`EffectBaker` compiles each `.fofx` pass once with glslang (Vulkan 1.0 client, SPIR-V 1.0) and emits, per stage, the native `-spv` (consumed by `Rendering-Vulkan`, and cross-compiled by SPIRV-Cross to `-glsl` / `-glsl_es` / `-hlsl`) plus, for the opt-in SDL_GPU backend, a `-spv_sdl` flavor and SDL-remapped `-msl_mac`/`-msl_ios`. The native SPIR-V follows the engine's 2-set descriptor convention (set 0 = uniform buffers, set 1 = combined image samplers, shared by both stages); `-spv_sdl` is that same SPIR-V with its descriptor decorations rewritten in place to SDL_GPU's per-stage convention (vertex samplers = set 0 / UBOs = set 1, fragment samplers = set 2 / UBOs = set 3, dense 0..N-1 slots). The per-pass `-info` artifact carries two sections: `[EffectInfo]` (program-wide bindings the GL/D3D/Vulkan backends consume, plus a `CHECK_BUF` size validation against the `RenderEffect` uniform structs) and `[EffectInfoSdl]` (per-stage SDL slot per resource plus the sampler/UBO counts `SDL_CreateGPUShader` needs). The baker hard-fails an effect that exceeds SDL_GPU per-stage limits (4 uniform buffers, 16 samplers), declares storage buffers/images, uses duplicate/missing explicit bindings, or declares a resource it never uses.

`ImageBaker` imports PNG/TGA plus classic frame/image formats such as FRM,
FRx, FOFRM, ART, SPR, ZAR, MOS, BAM, and TIL. FOFRM nested frame references
forward `$` options to the referenced image loader. ART options accept palette
selection (`0`..`3`), transparent-alpha derivation (`t`/`T`), horizontal and
vertical mirroring (`h`/`H`, `v`/`V`), and frame selection/ranges (`f`/`F`,
for example `f5` or `f7-5`). SPR options accept zero or more `[part,r,g,b]`
color-offset entries, using either commas or whitespace as separators, followed
by the sequence name. BAM options accept a cycle index and optional cycle-frame
selector separated by `-` (for example `$1` or `$1-3`); out-of-range cycle and
frame selectors fall back to the first available cycle/frame.

`ImageBaker` can also bake an indexed silhouette mesh for every unique RGBA
frame. The controls live in the dedicated `SpriteMesh.*` setting group inherited
by `BakingSettings`:

- `Enabled` opts the embedding project into mesh generation;
- `AlphaThreshold` defines the binary source mask (`alpha > threshold`);
- `MaxTriangles` is the search and retention budget for enclosing and detailed
  candidates;
- `AreaSavingsWeight` converts the fraction of original quad area removed into
  selection-score points; one point compensates one submitted triangle.

The polygonization algorithm is isolated in `SpriteMeshing`. It accepts an
RGBA frame, its dimensions, and the resolved mesh settings, then owns mask
construction, component and contour analysis, candidate generation,
triangulation, validation, scoring, and the diagnostic result. `ImageBaker`
owns image-format decoding, frame sequences and shared frames, adaptive
per-frame padding and cropping, mesh serialization, and baking-report
integration. This
keeps polygonization independent of `ImageBaker::FrameShot` and allows the
geometry builder to be exercised without an image-resource container.

Geometric safety is internal policy rather than project tuning. The baker uses
a one-pixel mask guard band and may probe up to 20 pixels of temporary padding
when a candidate needs room beyond the source bounds. After selecting a mesh,
it takes the exact bounds of the final vertices, crops the RGBA payload to those
bounds, and translates the vertices to the cropped frame origin. The serialized
per-frame offset is adjusted on both axes so the logical root remains at the
same screen position even when different animation frames have different
bounds. At runtime `AtlasSprite` reverses that storage adjustment for its public
logical size and offset, maps mesh positions through the stored source origin,
and keeps UVs local to the cropped atlas allocation. A quad or empty result is
not cropped or padded. Candidate profitability
is always compared against the original unpadded frame, so increasing the
search area cannot manufacture an artificial saving. Each unique frame is
searched on the maximum temporary canvas, then the retained mesh is translated
into its minimum required canvas without repeating candidate generation. A
maximum-canvas quad is re-evaluated on the unpadded frame, because the bounded
contour search can produce a useful border-sensitive candidate there.

Mesh generation builds enclosing candidates for every reachable triangle count
from one through `MaxTriangles`. It starts with exact convex-hull support lines.
A greedy removal path quickly reaches the configured range, then a deterministic
bounded beam explores alternate support-line removals and retains the smallest
area found for every triangle count. The exhaustive minimum-area triangle and
parallelogram candidates remain additional optimized cases. Candidates that do
not contain the complete hull are rejected before triangulation.

If the mask has multiple disconnected outer contours, the same candidate ladder
is built for every component. Nearby components are also merged into a
deterministic hierarchy. Every partition that can fit the triangle budget is
evaluated, so the search compares one global primitive, all-independent
primitives, and intermediate groupings such as two close door details plus a
separate leaf. A bounded dynamic program keeps several low-area alternatives per
triangle count while distributing the budget. The final validator rejects local
primitives that overlap each other or miss any guarded visible pixel. Selection
scores each valid candidate as
`savedFrameAreaRatio * AreaSavingsWeight - triangleCount` and compares it with
the ordinary quad score of `-2`. Equal scores prefer the smaller covered area.
This lets projects explicitly trade submission complexity for fill-rate savings:
for example, a weight of 64 makes each additional 1.5625% frame-area saving worth
one extra triangle. A closed double door can still use one common primitive,
while sufficiently separated leaves use independent primitives without covering
the gap.

The baker also traces exact pixel-cell boundaries, preserves disconnected opaque
islands and transparent holes, and automatically tries a deterministic ladder
of closed-contour simplification tolerances. Clipper2 normalizes touching paths,
offsets simplified geometry by the internal guard band, unions it with the exact
source polygon so no visible area can be clipped, and intersects it with the
exact raster-dilated region so simplification cannot bridge arbitrary
transparent pockets. A second constrained cleanup removes long pixel
staircases. Every simplified outer-ring edge is moved to the supporting line
that encloses its complete replaced source chain before adjacent lines are
intersected; hole rings retain ordinary inward simplification. This keeps the
cleanup from cutting across visible stair-step corners. Earcut only triangulates
the resulting outer rings and holes.

Every valid detailed result up to `MaxTriangles` competes directly with the
enclosing and per-component candidates under the same score. At every tolerance,
the constrained offset, guard-band simplified, and tolerance-expanded cover
families are generated independently; finding a merely valid candidate in one
family never suppresses another family. The tolerance ladder likewise runs to
its deterministic bound instead of stopping after several early valid results.

The simplified families use a bounded removal beam rather than probing only one
vertex. Each step tries replacing an exterior corner with a supporting chord
through that corner; adjacent edge intersections preserve coverage. The beam
keeps the lowest-area distinct states and records the best result for every
reachable triangle count. Tolerance-expanded covers convert the
Douglas-Peucker error bound into an enclosure instead of clipping thin visible
features. They still undergo exact pixel-cell coverage and topology validation,
but their transparent overdraw competes through `AreaSavingsWeight` rather than
a separate tolerance mask.

Generated triangles must cover the complete unit-square area of every original
visible pixel; the validator clips triangles against pixel cells and compares
their accumulated area instead of relying on a finite set of sample points.
For the constrained detailed-contour path, additional transparent coverage is
limited to the internal one-pixel guard band plus one pixel for final staircase
cleanup and integer vertex rounding. Expanded-cover and enclosing candidates are
not controlled by a separate overdraw gate: their actual covered area is
accounted for directly by the score.
Failure reasons remain distinct internally
(contour extraction/offset, vertex limit, triangulation, area, coverage, and
tolerance-mask failures), so retry policy does not conflate complexity with
geometric invalidity. An empty
mask is recorded explicitly as empty geometry; a fully opaque, unprofitable,
oversized, or otherwise unsafe result is recorded as the ordinary quad. The
optional Earcut Delaunay refinement pass is deliberately not used: it changes
neither triangle count nor covered area and does not promise bit-identical
output across compilers.

Baked sprite blobs have an explicit engine-owned magic/version and store each
unique frame's individual draw offset before its cropped RGBA payload, followed
by the mesh kind. `SpriteResource` owns this shared format contract, mesh data
type, and strict whole-resource decoder used by the client, particle editor,
and project-side server image loader. Its public entry point accepts the exact
resource byte span and creates its own reader, so decoding is independent of
any caller-owned stream position while footer and trailing-data checks remain
scoped to that resource slice. The decoder returns animation timing,
directions, per-frame offsets, shared-frame references, RGBA pixels, and
optional mesh geometry. Mesh records use fixed-width cropped-frame coordinates
and indices plus the original logical source size and the cropped-frame origin
within that source. The origin may be negative when selected geometry uses the
internal safety border. The decoder requires mesh vertices to occupy the exact
serialized frame bounds. Shared animation frames continue to refer to the
original frame and do not duplicate its offset, pixels, or geometry. The
runtime rejects legacy or malformed blobs rather than guessing their layout.
Drawable atlas sprites retain the original logical size, anchor, scaling, and
hit-test coordinate space while using the cropped pixels and mesh for texture
storage and rendering. Thus transparent-canvas cropping is an internal storage
optimization rather than a change to the authored sprite contract.
Consumers that sample the image as a plain rectangular texture rather than a
sprite (`FontManager`, `ParticleEditor`, and project-side server image sampling)
restore the cropped payload into the original logical canvas so their size and
pixel-coordinate contracts do not change. In particular, font glyph positions
are authored against the original sheet dimensions; `SpriteManager::LoadSpriteAsQuad`
must be used for font sheets so mesh padding/cropping cannot shift glyph UVs.
After this format changes, or when `SpriteMesh.*` values change without a new
build hash, run `ForceBakeResources`; source-file timestamps alone cannot prove
that an existing image output was baked with the same mesh settings.

`MapBaker` writes separate server and client map blobs. The client blob serializes visible static items, and its hash dictionary is also accumulated from client-side properties of hidden static items so `Common` hstring values can resolve later without exposing the hidden item entities.

`ParticleBaker` exposes only the formats whose backend is enabled at build time.
`FO_SPARK_PARTICLES` enables text `.spark` input and generated `.spk` output;
`FO_EFFEKSEER_PARTICLES` enables text `.efkproj` input and generated `.efk`
output. Both options default to `OFF`, in which case the registered baker has no
particle formats to process.

For SPARK, `ParticleBaker` loads native `.spark` XML with the engine's registered
`SparkQuadRenderer` type and writes deterministic SPARK binary to `.spk` with
the same path stem. Renderer texture paths are resolved relative to the
`.spark` resource; absolute paths and paths which escape the resource source
are hard errors. Unknown object types, malformed XML, authored `.spk`, and
binary-save failures are also hard errors; publishing a graph after silently
omitting an object is forbidden.
The client and baker use the same `SparkExtension.cpp` implementation through
`ClientLib`; do not compile a layout-changing headless copy into `BakerLib`.
The binary loader enforces exact memory payload length, bounded object/attribute
counts, bounds-checked reads, descriptor signatures, and valid typed object
references, so changing a custom serialized descriptor requires rebaking its
resources.

For Effekseer, `.efkproj` must be an on-disk XML project normalized by Editor
1.80.5 with project version 3. `ParticleBaker` calls the native C++
`EffekseerCompiler` module directly. The fixed-profile exporter produces raw
`SKFE` bytes and a dependency list; the baker validates each result with the
pinned C++ Effekseer Core and publishes it under the same path stem with the
`.efk` extension. The compiler is part of `BakerLib` and is never linked into
production clients. Native Mapper's on-demand baker uses the same path when a
tracked source edit is detected. Web targets consume `.efk` resources baked
before packaging.

The compiler also exposes the referenced resource paths for each project.
`ParticleBaker` resolves them relative to that project, rejects paths which
escape the project's physical directory resource source, and stores a
per-output snapshot containing the project path, size, and write time plus each
dependency's path, size, and write time under
`<BakeOutput>/.baker-cache/Effekseer/<pack>/<output>.deps`. On the next
incremental check it stats those same physical files which the native compiler
reads. This remains correct when a resource pack has multiple overlaid input
directories: the snapshot follows the selected project's disk source rather
than a same-named virtual file from another source.
A changed, deleted, or renamed dependency dirties every effect that references
it, while an unrelated file does not trigger a corpus-wide recompile. A missing
or stale snapshot makes the compiler inspect the project's dependency list
before the normal single `BakeChecker` call. `ParticleBaker` then removes only
that effect's stale physical `.efk` output, so the ordinary missing-output path
schedules the compile without a special timestamp or a callback in the common
baker infrastructure.

An `.efkmodel` is not an effect container: it is a binary runtime
dependency requested by Model nodes and remains subject to the embedding
project's ordinary resource-pack/raw-copy policy.

Authored `.efk` files are hard errors rather than copy-through inputs. Project
resource sources retain reproducible `.efkproj` XML, while runtime code accepts
only the generated `.efk`. Baking proves the compiler/Core parser boundary
only: it does not load FOnline atlas textures or enforce the client/Mapper CPU
Sprite/Ring renderer capability policy. A compiler implementation change
requires a forced bake so all generated `.efk` files are refreshed.
`ModelMeshBaker` builds the passive `ModelMeshData` tree and its common codec
writes a versioned mesh-only payload. Every baked model mesh
starts with `LFMODMSH`, schema `1`, and zero flags, followed by the recursive
bone, bind, and drawable-mesh data. The writer does not serialize clips or a TRS
tail. `ModelInfoBaker` traverses the decoded DTO for metadata, while the client
adapts it into hashed runtime hierarchy and render types; neither owns a second
byte parser. Both require this header, consume the mesh payload exactly, and
reject old headerless files, unknown schemas or flags,
truncation, and trailing data. There is no legacy mesh-format fallback.
Runtime readers preflight every serialized length/count against the unread byte
span before allocation. Mesh vertices are limited by `vindex_t`
addressability, each index must address a serialized vertex, skin palettes may
not exceed `MODEL_MAX_BONES`, and offset counts must match. Every serialized
blend weight/index must be finite, weights stay in `[0, 1]`, indices are integral
and address that palette, and each vertex's weights sum to one. The hierarchy
has at most 1024 joints and at most 128 nodes on one parent chain; the mesh
baker, `ModelInfoBaker`, source loader, and client all enforce the applicable
limits. Malformed resources therefore fail with contextual
`DataReadingException` instead of allocation, out-of-bounds palette access, or
recursive-stack failure.
Schema 1 keeps the existing `DataWriter` native-endian mesh payload; all current
engine targets are little-endian. Unlike the explicitly little-endian Ozz
envelopes below, a future big-endian mesh consumer requires a converted wire
format and a new schema rather than interpreting schema 1 in place.

Before writing the mesh payload, `ModelMeshBaker` validates node matrices,
vertex attributes, normalized colors, skin weights, skin offsets, the 1024-node
total limit, and the same 128-node safe hierarchy-depth limit. Values must be
finite and representable as `float32`. Every skinned vertex must retain at least
one positive influence; cluster indices must resolve, weights must be
non-negative, and the retained top four are normalized and rechecked to sum to
one. The final serialized vertex is checked again after UV flipping and
skin-weight normalization.
Diagnostics name the source asset, node, field, element, and component. Invalid
values are never clamped or replaced with identity.

After `ufbx_generate_indices` has removed byte-identical duplicate vertices,
the baker runs the pinned `meshoptimizer` v1.2 release (tag commit
`9d9890c73011d75920af614485296d1e03e95448`) on each drawable mesh. Vertex-cache
optimization first reorders whole triangles to improve transformed-vertex
reuse; vertex-fetch optimization then reorders the interleaved `Vertex3D`
buffer to first-use order and rewrites its 32-bit working indices. Both passes
write temporary buffers. The baker verifies triangle-list shape and index ranges
before the library call, requires fetch optimization to retain every vertex
produced by `ufbx`, validates the resulting indices again, and only then commits
the buffers and narrows indices to `vindex_t`. The library's temporary allocator
is installed once before parallel mesh jobs and uses `SafeAllocator`, preserving
the engine rpmalloc, backup-pool retry, and fail-fast OOM policy.

These passes are lossless reorderings, so `LFMODMSH` remains schema `1`; existing
runtime readers need no meshoptimizer dependency. Overdraw reordering is not
enabled because the same assets target desktop and tiled mobile GPUs, where its
benefit is workload-dependent. Quantization, mesh codecs, LODs, meshlets, and a
packed vertex layout remain a separate measured schema-2 design rather than an
implicit extension of this wire format.

Animation extraction is owned by `ModelSourceLoader`, not by the baked mesh
format. For each source FBX selected through a resolved `.fo3d`, the loader uses
`ufbx` to extract the source skeleton and every clip, validates the complete
source asset, and returns immutable source data to `ModelInfoBaker`. Clip names
must be unique case-insensitively. Source/clip identity, positive finite
duration, hierarchy/output pairing, every S/R/T time and value, joint/clip/key
limits, the shared 128-node source-hierarchy depth, and quaternion normalization
are validated before Ozz conversion.
Individual S/R/T channels must be non-empty and key times must be finite,
strictly ascending, and have finite intervals. The `ModelSourceAssetCache` is
created once per `ModelInfoBaker::BakeFiles()` call and implements single-flight
loading with one shared future per source path: parallel descriptions share one
successful parse or the same exception, while FBX parsing occurs outside the
cache mutex.

`ModelInfoBaker` audits the base skeleton and every selected external animation
clip before writing a `.fo3d` description. `ModelAnimationConverter` builds a
deterministic, parent-first canonical joint map as part of its compatibility
analysis, and the baker emits
per-description reports in path order. Animation-only joints are permitted and
reported as contributions. An FBX's unique technical scene root may have an
empty name; empty non-root joint names remain invalid. Duplicate joint names or
full hierarchies and incompatible parents fail baking because they cannot be
remapped to an index-based runtime skeleton without ambiguity. Different
source-root names are normalized to the base root and reported explicitly.

Rest-pose divergence between the base model and external clip FBXs is
report-only: selected clip outputs are authored absolute local TRS, so existing
assets do not require one shared rest pose. Reports retain every divergent
clip/joint mapping and log deterministic counts, the maximum matrix-component
delta, and its source. The canonical converter keeps base-model local rest
transforms and gives animation-only hierarchy nodes identity rest; a clip's
different rest pose does not replace either fallback.

Source key times are deliberately not constrained to `[0, duration]`: ufbx
playback ranges and key ranges are independent, and `trim_start_time` shifts
shared curves without cropping them. `ModelAnimationConverter` evaluates the source
curve at `0` and `duration`, keeps only keys strictly inside that playback
window, and preserves the original duration. It never stretches the take,
rescales all key times, or clamps each out-of-window key. Smooth quaternion
segments are adaptively subdivided at SLERP values with a `0.05`-degree
conversion budget, leaving room for Ozz runtime quantization under the tested
`0.1`-degree parity limit. Descriptions with disabled interpolation must use one
shared cropped S/R/T timeline per clip and contain exact keys at playback times
`0` and `duration` in every authored track. This preserves nearest-key midpoint
thresholds when source tracks also contain keys outside the playback window.
The converter serializes the shared timeline so the runtime sampler can select
the exact nearest key before sampling. Smooth multi-key tracks may start at or
before time `0`, or wholly after the playback window; a first key inside
`(0, duration]` is rejected because the required discontinuity cannot be
represented by continuous Ozz interpolation.

Direct FBX attachments are validated as rest-only resources. If an
`Attach ...fbx` target contains embedded animation clips, baking fails and
requires an `Attach ...fo3d` description with explicit animation mappings; the
direct runtime path has neither a controller nor a clip-binding table. This
check reads the attachment's validated source asset through the same per-call
single-flight loader.

External animation FBXs should contain transform hierarchy and animation only.
Baking rejects drawable geometry in an external `Anim` source unless the
resolved model description names that exact selected file with one
validation-only `AllowAnimationGeometry <file>` token. Duplicate resolved
exceptions, exceptions for non-selected sources, and exceptions whose target no
longer contains geometry are hard errors, preventing broad or stale allowlists.
When duplicate drawable geometry shares transforms with linked child models,
source repair must preserve the helper names, parents, and tracks as
geometry-free helper/bone nodes; it must not prune the complete branches.
Remove each exception with its repaired export. The embedding project's content
workflow owns the exact temporary allowlist and source-repair inventory;
reusable engine documentation deliberately does not enumerate project assets.

Incremental `.fo3d` invalidation includes the maximum write time of the resolved
description/include graph and every replacement-expanded `Model`, external
`Anim`, direct `Attach`, and `Cut` source dependency. A missing referenced source
is a hard error. This is required because an animation-only FBX change does not
change the mesh-only `LFMODMSH` output by itself but must still rebuild the
canonical Ozz rig and `ModelAnimationInfo` data.

The native model-animation format is implemented with the `ozz-animation`
0.16.0 release tag, whose exact tagged commit is
`6cbdc790123aa4731d82e255df187b3a8a808256`.
`ModelAnimationData` defines the versioned LF envelope for generated skeleton,
animation, and joint-remap payloads together with the containing rig manifest.
The envelope writes its magic,
schema, payload kind/flags, rig/source/cache signatures, exact pinned ozz
revision, source/object identity, payload length, and FNV-1a payload hash
field-by-field in little-endian order. Schema 1 reserves all flag bits and
requires zero. Source and object identities are non-empty valid UTF-8. The
envelope stores, but does not derive, three caller-owned deterministic content
signatures: canonical rig data, the exact cropped payload source, and converter
policy/settings. `ModelAnimationConverter` hashes fields in explicit little-endian
order and includes retained hierarchy/rest/fallback data, cropped TRS plus
presence/remap data, the interpolation policy/error threshold, optimizer state,
and pinned ozz revision. These signatures are deterministic invalidation inputs,
not security identities. The production payload stores them, but a persisted or
shared conversion cache additionally requires canonical signed-zero/quaternion
handling (or source-bit signatures) and Windows/Linux golden-signature tests
before these derived floating-point hashes can be treated as cross-platform
cache keys.

Before any runtime or offline codec object is constructed,
`InitializeModelAnimationMemory()` installs a private engine allocator for the
statically linked Ozz state in that module. It provides arbitrary power-of-two
alignment over `SafeAllocator<uint8_t>`, so codec allocations use the same
rpmalloc backend, backup-memory retry, OOM diagnostics, and fail-fast policy as
engine containers. No Ozz allocator type is exposed through a public model API,
and the vendored allocator source remains byte-identical to upstream 0.16.0.

The reader requires the caller's expected identity and rejects mismatches,
unknown kinds, reserved flags, truncation, trailing bytes, and payload hash
mismatches before the animation codec reads the opaque payload. FNV-1a detects
accidental corruption only; it is non-cryptographic and does not authenticate
the archive. Ozz's binary deserializer is therefore used only after LF framing
validation and only for trusted baked resources. A deployment that accepts
attacker-rewritable resource packs must authenticate the pack before model
loading; recomputing the non-cryptographic LF hash is not a security boundary.

After resolving a `.fo3d`, `ModelInfoBaker` now builds the complete canonical
runtime skeleton, every unique selected runtime animation, the base/clip remaps,
canonical presence masks, and nearest timelines in memory. Runtime ozz objects
are serialized with explicit little-endian `OArchive`, wrapped in the LF
envelope, read back, and checked for exact type/tag/length plus canonical
name/parent/count/rest/identity invariants. Signed TRS decomposition supports
mirrored rest matrices by placing reflection in X scale and requires `T*R*S` matrix
round-trip within a relative `1e-4` tolerance; shear, non-affine/zero-scale
transforms, non-unit quaternions, animation translation/scale outside ozz FP16
range, non-positive durations or durations whose reciprocal is not finite,
effective `(state, action)` cycle durations that round below one millisecond,
collapsed or non-finite normalized runtime timepoints, more than 1024 joints,
more than 65535
time points, and animated aliased roots fail baking.
Absent base tracks are explicitly filled from canonical rest; absent
animation-only tracks are identity, while a separate presence byte remains
zero.

The baked `.fo3d` contract is now explicitly versioned. Every description starts
with `LFMODINF`, schema `1`, and zero flags, followed by the existing positional
description and one required length-prefixed `LFOZZRIG` schema-1 payload. The rig
payload stores the canonical rig/cache signatures, canonical skeleton, base
remap, each unique resolved animation/remap pair, and a sorted
`(StateAnim, ActionAnim) -> (clip index, reversed)` table. Clip identities are
the actual baker-resolved source/name, so `Base`, case-insensitive authored
names, relative paths, and multiple animation pairs sharing one clip do not need
to be resolved again by the client.

Each rig archive manifest repeats the caller-owned source signature and
source/object identity before its nested `LFOZZARC`. The reader constructs its
expected metadata from that outer manifest and strictly compares the inner
archive instead of trusting metadata read from the same envelope. Counts,
lengths, ordering, duplicates, binding references, remap semantics, exact
consumption, type tags, skeleton topology/rest transforms, track counts, and
durations are validated before the immutable rig is published to
`ModelInformation`. Old unversioned descriptions are rejected; there is no
runtime fallback. The final mesh-only wire transition uses compatibility marker
`0.0.30` and requires a full resource rebake.

The generated Ozz rig is the only production clip/pose payload. The client does
not load a parallel TRS representation, and `ModelAnimationController` stores
only LF timeline/event state plus direct Ozz clip index, duration, reverse, and
bound-joint metadata. Sampling, per-joint body blending, movement replacement,
procedural pre-rotations, and local-to-model evaluation are per-instance Ozz
runtime work. Cross-model clip sharing or a persisted conversion cache remains
an optional optimization that must be justified by measured pack and runtime
memory results.

## Script compilation relationship

`ScriptsAndBaking.cmake` also creates script compilation commands:

- `CompileAngelScript` runs the project AS compiler target when `FO_ANGELSCRIPT_SCRIPTING` is enabled.
- `CompileManagedScripts` runs the standalone `ManagedScriptBakerApp` (built as `<FO_DEV_NAME>_ManagedScriptBaker`) when `FO_MANAGED_SCRIPTING` is enabled, so the managed project environment can be regenerated and rebuilt without a full resource bake. The `Managed` baker compiles `.cs` sources in place from every directory listed in `Script.ManagedScriptDirs` (engine core scripts and project scripts alike) and writes target API C# files as `*.gen.cs`, one generated `.gen.csproj`, and a matching `.gen.sln` under the `Script.ManagedScriptGeneratedDir` directory (empty targets the build `GeneratedSource/Managed` tree). Generated files include an auto-generated disclaimer, are rewritten only when their content changes, and stale `.gen.*` files in that directory are removed when they are no longer produced. The project and solution are named from the `Script.ManagedScriptProjectName` setting (default `FOnline`; embedding projects set their own name in the main config), so they get readable names such as `Scripts/<FO_NICE_NAME>.gen.csproj` and `Scripts/<FO_NICE_NAME>.gen.sln`. Managed assemblies are built into the baking pack under `Baking/<Pack>/Assemblies/<Target>Assemblies/` so packaging consumes them through the regular resource path; for a `Scripts` pack the entry assemblies are `Scripts.Server.dll`, `Scripts.Client.dll`, and `Scripts.Mapper.dll`, and any copied helper dlls from the MSBuild output are packed beside them for runtime dependency resolution. Managed builds run MSBuild with node reuse disabled to avoid stale project-file locks during later prebake passes. Managed builds also depend on `SetupManagedRuntime`, which publishes the Mono runtime plus Mono corelib into the CMake build tree, deploys it next to generated application binaries, and native package outputs carry that `ManagedRuntime/` directory as a runtime companion.

These are separate command targets from resource baking, but they share the same stage because generated/baked runtime inputs are part of the same build preparation workflow.

## Tests to inspect

Baker behavior is covered by focused tests in `Source/Tests/`:

- `Test_BakerSetup.cpp`
- `Test_ConfigBaker.cpp`
- `Test_MetadataBaker.cpp`
- `Test_ManagedScriptBaker.cpp`
- `Test_RawCopyBaker.cpp`
- `Test_ImageBaker.cpp`
- `Test_EffectBaker.cpp`
- `Test_ProtoBaker.cpp`
- `Test_ProtoTextBaker.cpp`
- `Test_MapBaker.cpp`
- `Test_TextBaker.cpp`
- `Test_ModelBaker.cpp`
- `Test_ParticleBaker.cpp`
- `Test_ModelMeshData.cpp`
- `Test_ModelAnimationData.cpp`
- `Test_ModelAnimationConverter.cpp`
- `Test_ModelSkeletonCompatibility.cpp`
- `Test_ModelSourceLoader.cpp`
- `Test_OzzAnimation.cpp`
- `Test_AngelScriptBaker.cpp`

Use the smallest test that matches the baker you changed. If CMake target names are generated by the embedding project, discover them from that project's presets/build files instead of hard-coding them here.

## Change routing

- New baker type: add/modify `Source/Tools/*Baker.*`, register it in `BaseBaker::SetupBakers()` or through `SetupBakersHook()`, and add focused tests.
- Baking command arguments: update `BuildTools/cmake/stages/ScriptsAndBaking.cmake`.
- Resource build hash behavior: update `BuildTools/cmake/helpers/WriteBuildHash.cmake` and related tests/build validation.
- Metadata baking: update `Source/Tools/MetadataBaker.*` and [GeneratedApiAndMetadata.md](GeneratedApiAndMetadata.md).
- Script-specific bake behavior: update `Source/Tools/AngelScriptBaker.*` and [Scripting.md](Scripting.md).

## Validation checklist

1. Configure from an embedding project root.
2. Build the project baker application/library target if the changed path affects app/library code.
3. Run the relevant baker test(s) under `Source/Tests/`.
4. Run `BakeResources` for incremental behavior when cache/build-hash logic matters.
5. Run `ForceBakeResources` after model-source or Ozz conversion changes. This
   is the positive integration gate that parses and extracts animations from the
   project's real FBX files instead of only synthetic unit-test assets.
6. Inspect generated output only as output, not as hand-authored source.
7. Update this document and [BuildToolsPipeline.md](BuildToolsPipeline.md) if stage responsibilities change.
