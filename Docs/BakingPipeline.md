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
- `Source/Tools/ModelInfoBaker.h`
- `Source/Tools/ModelInfoBaker.cpp`
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

`BakerDataSource` adapts resource inputs/outputs to the engine `DataSource` interface. It tracks input resource packs, output resources, cache checks, and output path construction. Its output-discovery dry runs and later lazy, per-file baking do not attach the master-bake report collector and are therefore deliberately absent from the report.

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
savings and reduction of transparent overdraw. `padding` separately reports
the serialized texture canvas, added RGBA pixels and bytes, padded-frame count,
maximum padding, and the padding histogram; padding therefore cannot be hidden
inside a favorable geometry-area result.

The section also contains selection-score minimum/average/maximum values and
the fixed quad score, plus a per-resource classification (`mesh_only`,
`quad_only`, `empty_only`, or `mixed`). Four deterministic top-25 lists retain
the frame identity and all relevant geometry fields for direct investigation:

- `largestMissedSavings`: retained quads ranked by the absolute number of
  source-frame pixels that are not visible; this is a diagnostic opportunity
  estimate, not a claim that all of those pixels can be removed safely;
- `largestRejectedCandidateSavings`: score-rejected candidates ranked by the
  frame area they would actually save if selected;
- `mostComplexMeshes`: meshes ranked by triangle count, then vertex count;
- `largestPaddingOverhead`: frames ranked by added serialized canvas pixels.

Each top-list row includes separate source and actual baked-output paths,
direction, frame index, form, selection origin or quad reason, triangles,
vertices, source and dilated components, padding, chosen tolerance/dilation,
source and baked canvas pixels, visible pixels, submitted doubled area,
potential transparent pixels, padding overhead, and selection score where one
exists. Full tie-breaking includes both paths, direction, and frame index, so
the top lists remain deterministic under parallel baking.

## Built-in baker types

`Source/Tools/Baker.cpp` registers built-in bakers when requested and enabled:

- `MetadataBaker` — `Source/Tools/MetadataBaker.*`
- `ConfigBaker` — `Source/Tools/ConfigBaker.*`, name `Config`, order `2`
- `RawCopyBaker` — `Source/Tools/RawCopyBaker.*`, name `RawCopy`, order `4`
- `ImageBaker` — `Source/Tools/ImageBaker.*`
- `EffectBaker` — `Source/Tools/EffectBaker.*`
- `ProtoBaker` — `Source/Tools/ProtoBaker.*`, name `Proto`, order `6`
- `MapBaker` — `Source/Tools/MapBaker.*`, name `Map`, order `7`
- `TextBaker` — `Source/Tools/TextBaker.*`, name `Text`, order `4`
- `ProtoTextBaker` — `Source/Tools/ProtoTextBaker.*`
- `ModelMeshBaker` — `Source/Tools/ModelMeshBaker.*`, enabled when `FO_ENABLE_3D` is active
- `ModelInfoBaker` — `Source/Tools/ModelInfoBaker.*`, enabled when `FO_ENABLE_3D` is active
- `AngelScriptBaker` — `Source/Tools/AngelScriptBaker.*`, enabled when `FO_ANGELSCRIPT_SCRIPTING` is active

When documenting a specific asset type, inspect the relevant baker class and its tests rather than inferring behavior from file extensions alone.

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
sequence padding, mesh serialization, and baking-report integration. This
keeps polygonization independent of `ImageBaker::FrameShot` and allows the
geometry builder to be exercised without an image-resource container.

Geometric safety is internal policy rather than project tuning. The baker uses
a one-pixel mask guard band and may probe up to 20 pixels of temporary padding
when a candidate needs room beyond the source bounds. It serializes only the
smallest symmetric border required by the selected vertices and compensates the
vertical sprite offset, preserving the original on-screen anchor. A quad or
empty result does not acquire adaptive padding. Candidate profitability is
always compared against the original unpadded frame, so increasing the search
area cannot manufacture an artificial saving. Each unique frame is first
searched on the maximum temporary canvas. A retained non-quad mesh is then
translated into the final smaller sequence canvas without repeating candidate
generation. A maximum-canvas quad is re-evaluated on that final canvas, because
the bounded contour search can produce a useful border-sensitive candidate at
the actual serialized size.

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

Baked sprite blobs have an explicit engine-owned magic/version and store the
mesh kind after each unique frame's RGBA payload. `SpriteResource` owns this
shared format contract, mesh data type, and strict whole-resource decoder used
by the client, particle editor, and project-side server image loader. The
decoder returns animation timing, directions, frame offsets, shared-frame
references, RGBA pixels, and optional mesh geometry. Mesh records use fixed-width
local pixel coordinates and indices; shared animation frames continue to refer
to the original frame and do not duplicate either pixels or geometry. The
runtime rejects legacy or malformed blobs rather than guessing
their layout.
After this format changes, or when `SpriteMesh.*` values change without a new
build hash, run `ForceBakeResources`; source-file timestamps alone cannot prove
that an existing image output was baked with the same mesh settings.

`MapBaker` writes separate server and client map blobs. The client blob serializes visible static items, and its hash dictionary is also accumulated from client-side properties of hidden static items so `Common` hstring values can resolve later without exposing the hidden item entities.

## Script compilation relationship

`ScriptsAndBaking.cmake` also creates script compilation commands:

- `CompileAngelScript` runs the project AS compiler target when `FO_ANGELSCRIPT_SCRIPTING` is enabled.
- `CompileMonoScripts` runs `BuildTools/compile-mono-scripts.py` when `FO_MONO_SCRIPTING` is enabled.

These are separate command targets from resource baking, but they share the same stage because generated/baked runtime inputs are part of the same build preparation workflow.

## Tests to inspect

Baker behavior is covered by focused tests in `Source/Tests/`:

- `Test_BakerSetup.cpp`
- `Test_ConfigBaker.cpp`
- `Test_MetadataBaker.cpp`
- `Test_RawCopyBaker.cpp`
- `Test_ImageBaker.cpp`
- `Test_EffectBaker.cpp`
- `Test_ProtoBaker.cpp`
- `Test_ProtoTextBaker.cpp`
- `Test_MapBaker.cpp`
- `Test_TextBaker.cpp`
- `Test_ModelBaker.cpp`
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
5. Run `ForceBakeResources` when forced rebuild behavior matters.
6. Inspect generated output only as output, not as hand-authored source.
7. Update this document and [BuildToolsPipeline.md](BuildToolsPipeline.md) if stage responsibilities change.
