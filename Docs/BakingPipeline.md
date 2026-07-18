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
- `Source/Tools/MetadataBaker.h`
- `Source/Tools/MetadataBaker.cpp`
- `Source/Tools/ConfigBaker.h`
- `Source/Tools/ConfigBaker.cpp`
- `Source/Tools/RawCopyBaker.h`
- `Source/Tools/RawCopyBaker.cpp`
- `Source/Tools/ImageBaker.h`
- `Source/Tools/ImageBaker.cpp`
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

### `BaseBaker`

The abstract base for individual baker implementations. Each baker provides:

- `GetName()` — stable baker name used by setup/config selection.
- `GetOrder()` — ordering key for deterministic bake ordering.
- `BakeFiles()` — the actual file transformation step.

`BaseBaker::SetupBakers()` in `Source/Tools/Baker.cpp` creates requested bakers and then calls `SetupBakersHook()` so external/project code can extend the baker list.

### `MasterBaker`

`MasterBaker` coordinates a full bake through `BakeAll()`. It is the app-facing type used by both `BakerApp.cpp` and `BakerLib.cpp`.

### `BakerDataSource`

`BakerDataSource` adapts resource inputs/outputs to the engine `DataSource` interface. It tracks input resource packs, output resources, cache checks, and output path construction.

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
- `ModelInfoBaker` — `Source/Tools/ModelInfoBaker.*`, order `5`, enabled when `FO_ENABLE_3D` is active
- `AngelScriptBaker` — `Source/Tools/AngelScriptBaker.*`, enabled when `FO_ANGELSCRIPT_SCRIPTING` is active

When documenting a specific asset type, inspect the relevant baker class and its tests rather than inferring behavior from file extensions alone.

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
| `ModelMeshData` | Common | Versioned `LFMODMSH` mesh-wire header shared by baker and client. It contains no runtime animation implementation. |
| `ModelMeshBaker` | Tools | Validates and writes mesh-only hierarchy, bind, vertex, index, influence, and drawable data. |
| `ModelInfoBaker` | Tools | Resolves `.fo3d` descriptions and dependencies, invokes source loading/compatibility/conversion, then writes `LFMODINF`, the required rig payload, and `ModelAnimInfo.foinfo`. |

The main data flow is:

`FBX/.obj + .fo3d` -> `ModelSourceLoader` ->
`ModelAnimationConverter` -> `ModelAnimationData` -> `ModelInfoBaker` ->
`ModelInformation` / `ModelAnimation`.

Mesh data follows the parallel
`FBX/.obj` -> `ModelMeshBaker` -> `ModelMeshData` -> `ModelManager` /
`ModelHierarchy` path. The two streams meet in `ModelInformation`; clips and
mutable poses are never serialized into the shared mesh hierarchy.

`ModelInfoBaker` emits the regular baked model descriptions together with a common `ModelAnimInfo.foinfo` table containing each model's effective `(state, action)` cycle durations after authored `AnimSpeed` is applied. The table materializes the same one-step `StateAnimEqual` / `ActionAnimEqual` resolution used by the client model runtime, including alias priority over an exact entry with the alias source key. Any pack can select `ModelInfo`; behavior never branches on `PackName`. Put that pack on every runtime side that needs model metadata. `EngineMetadata` registers the duration table alongside prototypes at startup, including model names in its hash storage; common scripts query it through `Game.GetModelAnimDuration(modelName, stateAnim, actionAnim)`. The method returns a `timespan`, or zero when the resource, model, or resolved tuple is absent. The config representation is an internal baker/runtime contract and should not be parsed by embedding-project scripts.

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

`MapBaker` writes separate server and client map blobs. The client blob serializes visible static items, and its hash dictionary is also accumulated from client-side properties of hidden static items so `Common` hstring values can resolve later without exposing the hidden item entities.

`ModelMeshBaker` writes a versioned mesh-only payload. Every baked model mesh
starts with `LFMODMSH`, schema `1`, and zero flags, followed by the recursive
bone, bind, and drawable-mesh data. The writer does not serialize clips or a TRS
tail. Both `ModelInfoBaker` and the client require this header, consume the mesh
payload exactly, and reject old headerless files, unknown schemas or flags,
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
canonical Ozz rig and `ModelAnimInfo` data.

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
