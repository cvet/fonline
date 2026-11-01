---
layout: default
title: Baking Pipeline
document_id: baking-pipeline
locale: en
permalink: /Docs/en/explanation/content-pipeline/baking.html
---

# Baking Pipeline

This page explains resource baking's wiring, source owners and validation. See [Tools](../../../Tools.md) for the broader engine tool map.

This page owns reusable engine behavior; game-specific content folders and package policy belong in project docs.

## Contract status

Baking is a revision-pinned `experimental` contract. The Engine owns resource-pack schema, built-in baker names/order, incremental/full rebuilds, private runtime payloads, report schema and extension hook. Projects own pack names, input layout, custom bakers, side assignment, content policy, generated sources and release acceptance.

Current Engine source and generated contract models are authoritative. A project config demonstrates one pipeline composition, without redefining the reusable contract.

Baking writes loose target-specific outputs. Packaging selects each logical pack's files from `Resources.usage.json`, applies its target filter and writes a deterministic `.fores` base; executable `Embedded` remains ZIP. Updates may pair a full `.fores` with one append-only writable `.patch.fores`; disposable `Resources.foindex` speeds merged lookup. [Resource Pack Format](../../../ResourcePackFormat.md) owns byte-level format, hash identities, validation and recovery; [Client Runtime Split and Updater](../runtime/client-updater.md) owns client synchronization. `BuildTools/measure_resource_packs.py` compares real baked-tree formats with optional readback verification; `analyze_resource_corpus.py` estimates source distribution and index cost without writing packs.

## Source paths inspected

- `BuildTools/cmake/stages/ScriptsAndBaking.cmake`
- `BuildTools/cmake/helpers/Build.cmake`
- `BuildTools/cmake/helpers/WriteBuildHash.cmake`
- `Source/Common/Settings.h`
- `Source/Common/Settings.cpp`
- `Source/Applications/BakerApp.cpp`
- `Source/Applications/BakerLib.cpp`
- `Source/Tools/Baker.h`
- `Source/Tools/Baker.cpp`
- `Source/Tools/BakingContext.h`
- `Source/Tools/BakingContext.cpp`
- `Source/Tools/BakingReport.h`
- `Source/Tools/BakingReport.cpp`
- `Source/Tools/ResourceUsage.h`
- `Source/Tools/ResourceUsage.cpp`
- `BuildTools/resource_usage.py`
- `Source/Tools/MetadataBaker.h`
- `Source/Tools/MetadataBaker.cpp`
- `Source/Tools/ConfigBaker.h`
- `Source/Tools/ConfigBaker.cpp`
- `Source/Tools/RawCopyBaker.h`
- `Source/Tools/RawCopyBaker.cpp`
- `Source/Tools/AudioBaker.h`
- `Source/Tools/AudioBaker.cpp`
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
- `Source/Tools/ManagedScriptBaker.h`
- `Source/Tools/ManagedScriptBaker.cpp`
- `Source/Tests/Test_BakerSetup.cpp`
- `Source/Tests/Test_MetadataBaker.cpp`
- `Source/Tests/Test_ConfigBaker.cpp`
- `Source/Tests/Test_RawCopyBaker.cpp`
- `Source/Tests/Test_AudioBaker.cpp`
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
- `Source/Tests/Test_ManagedScriptBaker.cpp`

## What baking does

Baking converts project resources/config into runtime output. Command targets `BakeResources` and `ForceBakeResources` in `BuildTools/cmake/stages/ScriptsAndBaking.cmake` run the project baker with the project's main config.

Runtime/source owners:

- `Source/Applications/BakerApp.cpp` — executable wrapper constructing `MasterBaker` and calling `BakeAll()`.
- `Source/Applications/BakerLib.cpp` — exported `FO_BakeResources()` entry for library baking. Unpackaged apps normally prebake on startup; `Baking.PrebakeOnStartup = False` skips this when a launcher already baked for several processes. On Linux, a linker export map and post-build symbol check make it the library's sole public symbol. Allocator/Engine implementation symbols bind locally, preventing a release baker loaded into a sanitizer host from interposing host allocation or global runtime state.
- `Source/Tools/Baker.h` / `Source/Tools/Baker.cpp` — shared context, baker setup, data source, output writing and `MasterBaker`.
- `Source/Tools/BakingReport.h` / `Source/Tools/BakingReport.cpp` — report contracts, thread-safe aggregation, JSON serialization and report paths.

### Output names are reconciled with the names bakers addressed

`MasterBaker::BakeAllInternal()` reconciles output names with those the bakers addressed, in two steps around the outdated-output sweep:

1. `ReconcileStaleCasedOutputDirs()` walks expected directories shallowest first and renames case-only differences, logging `Rename stale-cased dir <from> to <to>`. Directories precede files: a differently cased parent otherwise resolves to the existing directory, leaving its name unchanged. Shallowest-first puts each rename in an already corrected parent.
2. After the sweep, files receive the same comparison, logging `Rename stale-cased file <from> to <to>`.

On case-insensitive filesystems, case-only input renames otherwise disappear: output creation reuses old entries, the outdated sweep case-folds paths, and incremental baking may skip the apparently current artifact. Exact runtime lookup then cannot resolve its stale spelling.

`BakeChecker` supplies expected names, so bakers with different source/output names register output paths. `ImageBaker` registers lowercase Fallout critter `.frm` frames and a `.fr0`..`.fr5` set as one `.fofrm` for critters or `.frm` elsewhere. Reconciliation therefore keeps runtime spelling instead of restoring source spelling. `ScanModeRegistersTheResourceEachSourceBakesTo` compares registrations with written resources.

Reconciliation runs once per bake over produced outputs, including those skipped as up to date; it adds no per-write work and neither deletes nor recreates content. Case-sensitive filesystems need no reconciliation: the ordinary outdated sweep removes the old name. `BakerMasterRenamesStaleCasedOutputAfterCaseOnlyInputRename`, `BakerMasterRenamesStaleCasedOutputDirAfterCaseOnlyInputDirRename`, and `DiskFileSystemNameCase` pin pipeline/filesystem contracts.

## Output ownership and competing processes

`MasterBaker::BakeAll` acquires exclusive `<output-directory>.lock` before creating/deleting reports, success markers or outputs. Canonicalizing existing path components unifies relative, trailing-separator and directory-alias paths. Output must be a directory below a filesystem root. The lock covers error handling and both report writes; RAII releases it on return, including failure.

The sibling file survives a forced output rebuild. Leave it in place: its
existence does not mean a bake is running. Removing a held POSIX lock file can
let another process lock a new inode at the same name. A competing or otherwise
unavailable lock fails immediately with `Resource baking output lock is
unavailable` and leaves the previous reports, hash and resources unchanged,
including a repeat call on a previously used `MasterBaker`. Check the running
owner and directory permissions, then retry after it exits. Parent directories
and the empty sibling lock may be created even when acquisition fails.

Only the native baker writes `Resources.build-hash`, from compiled `FO_BUILD_HASH`, before releasing the lock. `AddBakingTarget` has no post-process marker writer: an unlocked CMake write could mark another process's incomplete rebuild successful. Native binary markers still use the configured hash helper. Reconfigure/rebuild for a new revision.

With `AS_DEBUG`, an AngelScript pack also acquires the working directory's `AS_DEBUG.lock` before output mutation. Same-directory baker processes refuse concurrent debug compilation even with different outputs. Role compilers and packs at the script-baking order run serially because bytecode diagnostic filenames are shared; other pack orders keep their configured mode. Release builds retain pack/role parallelism without this lock. Independent debug bakes need separate working directories; the lock does not govern arbitrary external AngelScript compilers. Both locks are process-owned handles released by the OS at exit, not PID sentinels.

`DiskFileSystem` lock/move/Unicode tests, `BakerMasterRawCopy` failure/forced-rebuild cases and `BakerMasterAngelScriptMultiplePacks` cover these boundaries. Process integration must also verify a refused second owner, byte-identical protected outputs and reuse after normal/abrupt owner exit. Locks protect cooperating bakers, without making the resource set transactional.

## Resource usage manifest

`MasterBaker::BakeAllInternal()` registers every configured pack in a `ResourceUsage` collector (`Source/Tools/ResourceUsage.h` / `.cpp`), then publishes `Resources.usage.json` in the output directory after all pack bakers finish. Packaging exports only that selection: `BuildTools/resource_usage.py` validates/loads the manifest; `BuildTools/package.py` packages only listed files.

The manifest contains `Version` (integer `1`), `BuildHash`, `BakingConfigHash` and `Packs`: every configured pack maps to a sorted, exact pack-relative file list, including empty arrays. Diagnostic `Reasons`, keyed by `<pack>/<path>`, explains selections; packaging ignores it.

Pack names, output paths, references and dependency edges use canonical relative paths. Native registration and packaging reject repeated or trailing `/`. An empty optional reference adds no root. One physical output key must identify one pack/path pair and baker. Aliases such as `Art` + `Subpack/Image.png` and `Art/Subpack` + `Image.png` fail before registry mutation; identical repeats and `Resource` to `Required` promotion remain valid.

### Manifest lifecycle

`BakeAll()` deletes the previous manifest after acquiring the output lock and before any output mutation. Refused locks leave manifest, markers, reports and resources unchanged; interrupted bakes cannot leave a usable old manifest. Success writes `Resources.usage.json.tmp`, renames it into place, then writes `Resources.baking-config-hash` and `Resources.build-hash`. Success markers therefore require that run's published manifest. Packaging requires the manifest and both markers to match.

The manifest selects from a normal incremental/forced bake's complete loose output; it neither accelerates baking nor deletes source art. Baking still produces the full local tree and image cache, and the outdated sweep still removes outputs no baker addressed. Only packaging narrows: disk presence alone does not select an export.

### Root references

`ResourceUsage::CollectReferences()` re-reads baked root references every run, including cache hits, never sourcing a previous manifest. Roots:

- the `Render.ImGuiDefaultEffect`, `Render.CritterStubSpriteName` and `Render.ItemStubSpriteName` settings, from the applied config and the baked client config;
- every resource-typed property of every typed baked prototype — server and client metadata, including `FixedType` rows from `.foinfo` files — through `ResourceUsage::AddProperties()`;
- the same resource-typed properties of placed map entities (critters on the server side, items on both sides), restored from the baked map blobs.

`AddProperties` reads typed properties, not arbitrary strings: only resource base types with nonempty scalar `hstring` or nonempty `hstring[]` arrays qualify; other resource-typed shapes fail baking. Each root must name a real baked output. Missing providers fail manifest creation with `Referenced resource has no baked output`, binding roots to names actually written.

Each metadata pass skips properties disabled for its role; active-role reads of server-only/client-only `Resource` fields contribute to the shared selection.

### Output kinds and registration

Every baker output is registered as `Required` or `Resource`:

- `BakingContext::CheckOutput()` registers every incremental query, including up-to-date answers; `WriteData()` registers writes. Cache hits and fresh writes both count. Registration under a second baker fails; later `Required` upgrades `Resource`.
- Custom bakers default to `Required`: per-baker `BakingContext` copies default the output kind to `Required`. `Embedded` outputs are always `Required`, regardless of requested kind. Built-in image, audio, effect, particle, model and raw-copy outputs are `Resource` unless `KeepOutput()` explicitly marks them `Required`. This preserves always-shipped structural outputs: `SpriteInfo`/`ModelAnimationInfo` indexes, default effects and embedded content. Default-effect retention also applies when no incremental checker is supplied.

### Selection and dependency closure

`ResourceUsage::CreateManifest()` first requires every `Required` output in its pack directory, failing with `Required baked output is missing` otherwise. Each root then selects all available matching providers across packs, preserving pack identity. Finally, selected outputs add owning-baker `CollectResourceDependencies()` edges transitively to a fixed point.

Dependency edges describe runtime companions. Custom bakers owning them must override the empty default. Built-in closure covers font pages, effect current-pass companions, model meshes/attachments/textures/effects/cuts and particle externals. FOFRM inputs are bake-time-only dependencies: flattening exports the composed `.fofrm`, without exporting each source image merely read to compose it.

A complete, up-to-date cache does not imply export. Removing the last reference drops a file from the next bake's manifest/package while keeping its local cache; flattening an FOFRM alone never ships its input PNG. An independent `Resource` reference or model-texture dependency can still export that source. Unreferenced cache copies stay off package lists.

Particle image dependencies resolve image filename options. Effekseer non-image dependencies (models, materials, curves and waves) keep the complete relative resource name, including `$`.

Model/particle descriptors come from their own physical pack. `ResourceProviderCallback` exposes all current available providers of each dependency path; model collectors combine meshes, child descriptions and relative texture overrides into a conservative shared allowlist. Unclaimed stale files cannot resolve. Map inclusion reasons group by source map, entity type and property, not placement ID.

### Forced inclusion for opaque runtime names

Typed-property collection cannot see roots used only through project JSON/INI, Spine data or script-built names. The Engine neither guesses exports nor expands wildcards, recurses directories, scans arbitrary strings, intercepts script constructors or falls back to exporting everything. Use a project `FixedType`, e.g. `ResourceSet` with an `hstring[] Resource` `Files` property, ordinary `///@ FixedType` metadata and `.foinfo` rows. The same collector reads its resource properties as roots. That name has no special parser or script-constructor execution: its typed references resolve like other roots.

For example, declare:

```text
///@ FixedType Common ResourceSet
///@ Property ResourceSet Common hstring[] Files Resource
```

Then author:

```ini
[ResourceSet]
$Name = Startup
Files = "Interface/Example With Space.png" \
 "Music/Example.ogg"
```

## CMake entry points

`BuildTools/cmake/stages/ScriptsAndBaking.cmake` defines `AddBakingTarget` as a validated project-interface helper and uses it to create the standard baking commands after application targets are available.

Current target responsibilities:

- `BakeResources` is created through `AddBakingTarget(BakeResources)` and runs the project baker with `-Baking.ForceBaking False`.
- `ForceBakeResources` is created through `AddBakingTarget(ForceBakeResources FORCE)` and runs it with `-Baking.ForceBaking True`.
- Both standard targets apply the embedding project's main config through `-ApplyConfig <FO_MAIN_CONFIG>` and use the default subconfig `NONE`.
- Every target created by `AddBakingTarget` works from `FO_OUTPUT_PATH`, depends on `ForceCodeGeneration`, and lets the native baker write `Baking/Resources.build-hash` while holding its output lock. The marker uses the same configured `FO_BUILD_HASH` as the native baker and applications; a later Git HEAD change cannot relabel the completed bake. Reconfigure, rebuild and rebake when the source revision changes.
- `CompileAngelScript` and `CompileManagedScripts` depend on `ForceCodeGeneration`, so metadata and generated code cannot lag behind either script backend or a bake invocation.

After `SetupScriptsAndBaking()` has run, an embedding project can add a target for a project-owned subconfig without copying the baker command:

```cmake
AddBakingTarget(Game_PublicResources
    SUB_CONFIG PublicGame
    COMMENT "Bake public resources")
```

The full signature is `AddBakingTarget(<target> [SUB_CONFIG <name>] [FORCE] [COMMENT <text>])`. `SUB_CONFIG` defaults to `NONE`, `COMMENT` defaults to `Bake resources`, and `FORCE` changes `-Baking.ForceBaking` from `False` to `True`. Unknown arguments and keywords without values fail at configure time. The embedding project owns the additional target name and the referenced subconfig.

The actual final target names that depend on these commands are project/preset-dependent. Do not document one embedding project's target names as universal engine behavior.

## Runtime classes

### `BakingContext`

`Source/Tools/BakingContext.h/.cpp` encapsulates state initialized from passive
`BakingContextOptions`. Settings/files stay borrowed; report and const graph stay
shared. Use `Get*` accessors and `GetAsyncMode()`; configure modes/graph before tasks.

Declare `BakingTaskGuard` after captured inputs: it waits valid, non-deferred
futures on every exit without consuming results. Proto client tasks finish before
the server shared-future handle is destroyed.

`CheckOutput()` calls the checker once if present, applies `required` (including
`false`), then reports the result once. `HasBakeChecker()` detects absence.
`WriteData()` returns the writer result and reports only successful submissions.

`IsImageOutputOwned(path)` recognizes consumed sources and this pack's image roots;
without a graph it returns `false`. Report methods are no-ops without a collector.

Changed outputs use `fs::write_file`: an exclusively created temporary file
beside the destination is written, flushed and closed before `fs::replace_file`
publishes it. Existing readers retain the previous bytes; new opens receive the
complete replacement. A failed write or publication preserves the destination and
removes the temporary file. Unchanged outputs keep their existing comparison and
timestamp behavior.

Both `MasterBaker` outputs and `BakerDataSource::WriteData` use this publication path. Baking reports, the successful `Resources.build-hash` marker and Effekseer dependency-cache snapshots also publish complete files atomically; their contents, successful-bake ordering and unchanged-resource timestamp rules are preserved.

Windows attempts an OS/filesystem-specific POSIX rename path when ordinary replacement fails. This does not guarantee replacement of every loaded image or CRT-held file: an open consumer may still refuse publication, and the Baker must report failure while retaining the previous output. Engine disk readers permit delete sharing and retain their old bytes after successful replacement. Validate managed assembly publication through the actual backend/bake route; a shared reader test alone does not prove every mapping mode. The helper flushes the stage without adding directory synchronization or promising power-loss durability for the rename.

`Source/Tests/Test_DiskFileSystem.cpp` covers replacement with an existing reader,
exclusive creation without truncation, destination preservation and temporary-file
cleanup after publication failure. The low-level contract is owned by
[Essentials](../../reference/native/essentials.md#filesystem-compression-sockets-and-work-threads).

### `BaseBaker`

The abstract base for individual baker implementations. Each baker provides:

- `GetName()` — stable baker name used by setup/config selection.
- `GetOrder()` — ordering key for deterministic bake ordering.
- `BakeFiles()` — the actual file transformation step.

Cross-pack dependencies use distinct stages: `ParticleBaker` runs before
`ModelInfoBaker`, which runs before `ProtoBaker`, which runs before `MapBaker`.
Model descriptions may reference baked particle resources, prototypes may
reference the baked model descriptions, and maps consume baked prototypes;
none of those validation steps may race publication by its dependency during a
clean or forced rebuild.

`BaseBaker::SetupBakers()` in `Source/Tools/Baker.cpp` creates requested bakers and then calls `SetupBakersHook()` so external/project code can extend the baker list.

Each baker receives its own named copy of `BakingContext`, sharing the collector
and read-only image graph while retaining its own baker identity. Context methods
handle output attribution directly, including asynchronous work, without wrapping
callbacks on each copy. The protected `BaseBaker` helpers forward to those methods
so existing subclasses can keep using them.

### `MasterBaker`

`MasterBaker` coordinates a configured bake pass through `BakeAll()`. It is the app-facing type used by both `BakerApp.cpp` and `BakerLib.cpp`.

`MasterBaker` also owns the one report collector for that bake attempt and
finalizes the report after either success or failure.

Before parallel pack preparation, `MountSharedInputDirs()` mounts each distinct configured `InputDirs` path once. Every pack context borrows the resulting read-only source through `DataSourceRef`, and the owning registry lives for exactly that `BakeAllInternal()` pass. Packs still apply their own include/exclude filters and preserve configured output semantics, but packs sharing a large authored tree no longer walk and cache that directory independently. Source-local name filtering preserves first-seen order while using a set of already-added names, so duplicate suppression is linear rather than repeatedly scanning the accumulated result.

Before preparing packs, the master checks `Resources.baking-config-hash` against `FO_BAKING_CONFIG_HASH`. Missing/changed baking-impact options clear the output tree and force a complete rebuild; unchanged options permit incremental reuse. The [configuration fingerprint](../../how-to/build/project-configuration.md#baking-configuration-fingerprint) owns option registration and scope.

### `BakingReport`

`BakingReport` is defined in its own `Source/Tools/BakingReport.h` / `.cpp`
module. The header owns the common report DTOs and write-result contract; the
implementation owns aggregation, sprite-mesh analysis, JSON construction, and
the output report path. `Baker.cpp` only drives its lifecycle and forwards bake
events to it.

### `BakerDataSource`

`BakerDataSource` adapts resource inputs/outputs to the engine `DataSource` interface. It tracks input resource packs, output resources, cache checks, and output path construction. `Reindex()` reconstructs its input mounts, baker instances, file collections, and output index, returning whether the indexed paths or source write times changed. Long-running tools can therefore discover and on-demand bake added or changed resources without replacing cached directory lookup with repeated disk scans. Output-discovery dry runs and later lazy, per-file baking do not attach the master-bake report collector and are therefore deliberately absent from the report.

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
incremental development bakes.

Outdated runtime-resource cleanup skips any `*.report.json` file directly in
the `BakeOutput` root (`REPORT_FILE_SUFFIX` in `Baker.h`). The suffix covers the
two standard reports and project-provided diagnostic artifacts written beside
them. Runtime resources always live below a pack directory, so the root-only
rule cannot preserve a genuinely stale baked resource.

The report is written directly into the `BakeOutput` root after runtime-resource
cleanup finishes. It is never mounted in the baked `FileSystem`, registered as
a baked output, or consumed as a runtime resource. It is removed before the next
bake attempt starts, so analysis data does not change the resource set delivered
to a client or server.

The top-level lifecycle fields are:

- `schemaVersion`, currently `1`;
- `status` and `failureMessage`;
- `buildHash`, `bakingConfigHash`, `bakeOutput`, and total `durationMs`;
- `mode.forceRequested`, `mode.fullRebuild`, `mode.rebuildReason`, and
  `mode.singleThread`. Rebuild reasons are `incremental`, `requested`,
  `build_hash_changed`, `baking_config_changed`, or `missing_build_hash`.

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

Model-info discovery registers output paths without validating cached model payloads:
linked descriptions from the same baker may not have entered the index yet. Normal
incremental and lazy baking still validate cached texture dependencies and reject
missing linked descriptions or animated model textures.

## Embedding-project pack design

`Source/Common/Settings.h` and `Settings.cpp` define the reusable `[ResourcePack]` state: authored `Name`, `InputDirs`, `InputFiles`, `IncludePatterns`, `ExcludePatterns`, `ServerOnly`, `ClientOnly`, `MapperOnly`, and `Bakers`, plus the derived `ConfigDir` of the declaring file. A name is mandatory, at most one side-only flag may be true, relative inputs resolve from `ConfigDir`, and the configured section order is preserved.

Mature projects and the independent TLA codebase converge on a few useful practices. They are recommendations, not extra Engine syntax:

- give each pack one clear responsibility and a stable name, such as metadata, scripts, text, prototypes, maps, visual resources, or sounds;
- list only the bakers that can transform that pack, instead of applying every baker to one broad input tree;
- place producer packs before consumers and use distinct baker orders for hard dependencies; bakers at the same order may run concurrently and must not consume one another's outputs;
- use `IncludePatterns` and `ExcludePatterns` to keep templates, editor-only inputs, and intentionally private sources out of runtime output;
- use one side-only flag when a pack truly belongs only to server, client, or mapper; an unflagged pack is delivered to server and client;
- prefer authored directories for normal source and `InputFiles` for mounted archives or data packs; do not edit baked output as source;
- run a force bake after an Engine revision, baker contract, global metadata, or dependency-graph change that invalidates more than timestamp-based incremental detection can prove.

For the exact project-configuration grammar and precedence rules, see [Configure a Game Project](../../how-to/build/project-configuration.md). For generated-source ownership and recovery order, see [Generated Content Workflow](../../how-to/build/generated-content.md).

## Built-in baker types

`Source/Tools/Baker.cpp` registers built-in bakers when requested and enabled:

| Config name | Class | Order | Availability |
| --- | --- | ---: | --- |
| `Metadata` | `MetadataBaker` | 1 | always |
| `Config` | `ConfigBaker` | 2 | always |
| `RawCopy` | `RawCopyBaker` | 4 | always |
| `Audio` | `AudioBaker` | 4 | always |
| `Image` | `ImageBaker` | 4 | always |
| `Effect` | `EffectBaker` | 4 | always |
| `Text` | `TextBaker` | 4 | always |
| `ModelMesh` | `ModelMeshBaker` | 4 | `FO_ENABLE_3D` |
| `Managed` | `ManagedScriptBaker` | 3 | `FO_MANAGED_SCRIPTING` |
| `AngelScript` | `AngelScriptBaker` | 4 | `FO_ANGELSCRIPT_SCRIPTING` |
| `Particle` | `ParticleBaker` | 5 | always; selected backend still depends on build options |
| `ProtoText` | `ProtoTextBaker` | 6 | always |
| `ModelInfo` | `ModelInfoBaker` | 6 | `FO_ENABLE_3D` |
| `Proto` | `ProtoBaker` | 7 | always |
| `Map` | `MapBaker` | 8 | always |

The particle/model/prototype/map stages intentionally form a strict dependency chain: particle outputs at order `5` are visible to model-info validation at order `6`, model descriptions are visible to prototype validation at order `7`, and baked prototypes are visible to map baking at order `8`. Bakers at the same order may run concurrently across resource packs and therefore must not consume one another's outputs.

When documenting a specific asset type, inspect the relevant baker class and its tests rather than inferring behavior from file extensions alone.

### Image conversion failures

When an image conversion fails, the diagnostic names its source resource path
alongside the underlying error. All scheduled conversions are still awaited;
any conversion failure fails the pack rather than accepting incomplete output.

### Shared animation metadata

Shared animation metadata uses `AnimationInfo` as the aggregate record. The generic
record contains a `SpriteInfo` payload for 2D frame count, duration, directions,
and resolved per-frame bounds, plus a `ModelAnimationInfo` payload in
`FO_ENABLE_3D` builds for model and animation AABBs and typed animation
durations. `ReadSpriteResource` fills the sprite payload from the baked sprite
header and frame table. For metadata queries that must not load pixel payloads,
`ImageBaker` also writes one compact version 2
`SpriteInfo/<PackName>.foinfo` index per resource pack. The index is an
aggregate over published roots, with independent direction counts and per-frame
durations. Packs without roots publish an empty index before dependent bakers,
preventing stale metadata after last-root removal. Successful snapshots retain entries: full/lazy baking
can restore a missing index without decoding unchanged pixels. Consumed inputs and
intermediate FOFRM are absent. Pack-local outputs remain distinct from cross-pack
resource winners. Sprite schema changes require matching binaries and a full rebake. The
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
  the configured `Animation.PreviewStateAnim` / `Animation.PreviewActionAnim`
  pair first, then the first valid animation or the static fallback;
- `BoundsStateAnimations` / `BoundsActionAnimations` and the parallel `BoundsMin*` /
  `BoundsMax*` arrays store the individual animation AABBs used by the runtime
  tight-crop predictor.

The baker samples every animation key, the midpoint between neighbouring keys,
and a 60 Hz grid. That common timeline prevents a fast arc from being missed
and keeps deterministic envelopes independent of camera angle, projection
factor, model-sprite resolution, and renderer backend.

`Baking.PreciseModelBounds` selects the geometry measurement at each sample:

- `true` uses `ModelBoundsMeasurement::PerVertex` and transforms every skinned
  vertex. `PublicGame` enables this exact mode for shipped resources;
- `false` (the default) uses `PerBoneEnvelope` and transforms one prepared
  envelope box per bone slot. Because blended vertices are convex combinations
  of their transformed bone positions, the union can over-size an envelope but
  cannot clip the exact geometry. This is the conservative fast path for local
  working bakes.

`ModelBoundsSampler` prepares a model hierarchy, mesh selection, and per-bone
envelopes once, then answers every clip from that immutable plan. Attachment
bounds reuse one sampled bone track per clip and bone instead of re-walking the
parent hierarchy for every attachment. Model-description sections are produced
independently and concatenated in sorted source order.

Each binary description link also writes an explicit geometry discriminator:
non-particle child links carry a required aggregate root-space AABB followed by
the `(state, action, AABB)` bounds of every mapped parent clip; the default link
and particle links carry no geometry payload. Multiple state/action pairs that
reuse one sampled clip all reference that clip's box. Invalid flags, missing
child bounds, unknown animation pairs, and degenerate AABBs are rejected.
Missing or invalid aggregate or animation bounds are baking errors in the
version 2 contract. In
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
layer/child-model bounds extend both contracts. For a direct child, the client
selects link boxes matching the active clips of the parent rig and uses the
aggregate link envelope when no matching clip is active. Nested links retain
their own aggregate envelope because their clip indexes belong to another rig.
The client projects those corners instead of walking or skinning combined-mesh
vertices; live particle bounds can still expand and rerender the scratch frame.

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

The model pipeline separates source extraction, compatibility analysis, conversion, native wire contracts and two final bakers. These are engine owners, not interchangeable animation-backend interfaces. Animation production code uses one same-named `.h`/`.cpp` pair per owner: `ModelSourceLoader`, `ModelAnimationConverter`, and `ModelAnimationData`.

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

Mesh data follows `FBX/.obj` -> `ModelMeshBaker` -> `ModelMeshData` -> `ModelManager` / `ModelHierarchy`. Both streams meet in `ModelInformation`; clips and mutable poses never serialize into shared mesh hierarchy.

Ozz conversion changes the source representation, retaining the model bounds/duration contract above and common `AnimationInfo` query surface.

[Prototype Format](../../how-to/content/prototype-format.md) and the generated [prototype-format reference](../../reference/prototype-format/index.md) own `ProtoBaker` section resolution, identity, inheritance, side-specific property applicability, strict values, references and migrations. Project extensions, entity metadata, IDs and semantic field combinations remain project-owned.

[Map Format](../../how-to/content/map-format.md) and the generated [map-format reference](../../reference/map-format/index.md) own `.fomap` section order, placement IDs, ownership references, static/dynamic handling, mapper round-trip and coupled server/client `MapBaker` payloads. Map catalogs, composition rules, quests, encounters and gameplay validation remain project-owned.

`ModelMeshBaker` imports current `.fbx` / `.obj` before `ModelInfoBaker` validates/emits concrete `.fo3d` descriptions. [Model Format](../../how-to/content/model-format.md) and its generated reference own syntax, includes, layers, attachments, transforms, materials, cuts, runtime composition and validation. [Model Animation](../../how-to/content/model-animation.md) owns animation tuples, `AnimSpeed`, one-step `StateAnimEqual` / `ActionAnimEqual` semantics, `ModelAnimationInfo.foinfo`, runtime distribution and missing-data behavior. Mesh/description/config layouts are private baker/runtime contracts; project scripts must not parse them.

`TextBaker` parses `<TextPack>.<Language>.fotxt`, completes changed pack language sets and normalizes against the first `Baking.BakeLanguages` entry. `ProtoTextBaker` merges inherited `$Text` fields into five Engine-generated prototype text packs. [Text and Localization](../../how-to/content/text-and-localization.md) and the generated [text-format reference](../../reference/text-format/index.md) own syntax, fallback, lookup, color-tag and project-formatting boundaries.

`ImageBaker` imports PNG/TGA and ten legacy/descriptor extensions, composes FOFRM sequences/directions/options and writes the stock client's private RGBA sprite container. [Image And Sprite Formats](../../how-to/content/image-format.md) and its generated [image-format reference](../../reference/image-format/index.md) own source support, FOFRM grammar, legacy selectors, output naming, runtime factories, atlases/caches and validation. Signed per-frame `Motion` feeds the narrower movement-driven [Sprite Root Motion](../../how-to/content/sprite-root-motion.md) contract.

`ParticleBaker` owns both optional backends: it validates SPARK `.spark` XML into deterministic `.spk` and compiles Effekseer `.efkproj` sources/dependencies into `.efk`. Both require measured bounds; authored `.spk`/`.efk` inputs fail. [Particle Format And Runtime](../../how-to/content/particle-format.md) and its generated [particle-format reference](../../reference/particle-format/index.md) own backend options, formats, supported objects/render capabilities, incremental baking, framing, integration routes and mandatory visible validation.

Fonts have no dedicated baker. `RawCopyBaker` copies runtime `.fofnt` and AngelCode text `.fnt`; `.bmfc` is an authoring sidecar shippable for provenance. `ImageBaker` separately bakes referenced PNG/TGA. `FontManager` validates/binds descriptors at client startup. [Font Formats And Text Layout](../../how-to/content/font-format.md) and its generated [font-format reference](../../reference/font-format/index.md) own syntax, resource relationships, layout/rendering, diagnostics and visible validation.

Audio has a dedicated baker: PCM/IEEE-float `.wav` becomes signed 16-bit PCM encoded as Vorbis; verified `.ogg` passes through at its logical path. Runtime uses one Vorbis decoder; legacy ACM is unsupported. [Audio](../../how-to/content/audio.md) and the generated [audio reference](../../reference/audio/index.md) own accepted profiles, handles, spatial updates, mixing, diagnostics and audible validation.

Video has no dedicated baker/transcoder. Runtime `.ogv` bytes copy unchanged and load by exact client resource path into the experimental Ogg/Theora decoder. [Video](../../how-to/content/video.md) and the generated [video reference](../../reference/video/index.md) own whole-resource memory, fullscreen/embedded behavior, separate audio, diagnostics and mandatory visible-client validation.

`EffectBaker` compiles each `.fofx` pass once with glslang (Vulkan 1.0 client, SPIR-V 1.0), emitting per-stage native `-spv` for `Rendering-Vulkan`, SPIRV-Cross outputs `-glsl` / `-glsl_es` / `-hlsl`, and opt-in SDL_GPU `-spv_sdl` plus SDL-remapped `-msl_mac`/`-msl_ios`. Native SPIR-V uses the engine's 2-set convention (set 0 uniform buffers, set 1 combined image samplers, shared across stages); `-spv_sdl` rewrites its descriptor decorations in place to SDL_GPU per-stage sets (vertex samplers 0 / UBOs 1, fragment samplers 2 / UBOs 3, dense 0..N-1 slots). Per-pass `-info` contains `[EffectInfo]` (program-wide GL/D3D/Vulkan bindings and `CHECK_BUF` sizes against `RenderEffect` structs) and `[EffectInfoSdl]` (per-resource stage slots and sampler/UBO counts for `SDL_CreateGPUShader`). Baking fails on exceeding SDL_GPU per-stage limits (4 uniform buffers, 16 samplers), storage buffers/images, duplicate/missing explicit bindings or unused declared resources. [Effect Format](../../how-to/content/effect-format.md) and the generated [effect-format reference](../../reference/effect-format/index.md) own authoring, resources, runtime caches, script values and validation.

Direct3D consumes baked `-dxbc` from SPIRV-Cross HLSL via vendored vkd3d-shader, without runtime HLSL compilation. Optional `Baking.Direct3DLevel9Shaders` invokes `Direct3DLevel9.cpp` to add an `Aon9` level-9.3 chunk only to non-model effects; unsupported 9.3 constructs fail baking. A 3D-enabled renderer cannot use 9.3; 9.1/9.2 are unsupported. See [Effect Format](../../how-to/content/effect-format.md#baking-outputs).

`ImageBaker` can also bake an indexed silhouette mesh for every unique RGBA
frame. The controls live in the dedicated `SpriteMesh.*` setting group inherited
by `BakingSettings`:

- `Enabled` opts the embedding project into mesh generation;
- `AlphaThreshold` defines the binary source mask (`alpha > threshold`);
- `MaxTriangles` is the search and retention budget for enclosing and detailed
  candidates;
- `AreaSavingsWeight` converts the fraction of original quad area removed into
  selection-score points; one point compensates one submitted triangle.

Declare all four `.fomain` values even with `Enabled = False`. Image baking resolves/validates the full group before frames: `AlphaThreshold` in `0..254`, positive `MaxTriangles`, finite non-negative `AreaSavingsWeight`. The canonical minimal project explicitly records Engine defaults, avoiding dependence on project-generated/inherited settings during standalone baking.

`SpriteMeshing` takes RGBA, dimensions and resolved settings, owning mask construction, components/contours, candidates, triangulation, validation, scoring and diagnostics. `ImageBaker` owns decoding, sequences/shared frames, adaptive padding/cropping, serialization and report integration. Polygonization thus stays independent of `ImageImport::FrameShot` and testable without an image-resource container.

Geometric safety is internal policy. A one-pixel mask guard band and up to 20 pixels of temporary padding accommodate candidates beyond source bounds. Selected final vertex bounds determine RGBA cropping; vertices translate to that origin. Both serialized offset axes adjust to preserve the logical root across differently bounded frames. `AtlasSprite` reverses this adjustment for public size/offset, maps mesh positions through the stored source origin and keeps UVs local to the cropped allocation. Quad/empty results receive no cropping/padding. Profitability compares against the original unpadded frame, so larger search areas cannot invent savings. Each unique frame is searched on the maximum temporary canvas, then retained geometry translates to its minimum required canvas without regenerating candidates. A maximum-canvas quad is re-evaluated unpadded, where bounded contour search may find a useful border-sensitive candidate.

Mesh generation builds enclosing candidates for every reachable triangle count from one through `MaxTriangles`, starting with exact convex-hull support lines. Greedy removal reaches the configured range quickly; a deterministic bounded beam explores alternate support-line removals, keeping the smallest area per count. Exhaustive minimum-area triangle and parallelogram candidates remain additional optimized cases. Candidates missing any hull area fail before triangulation.

Disconnected outer contours each receive this candidate ladder; nearby components also merge into a deterministic hierarchy. Every budget-fitting partition is evaluated, comparing a global primitive, independent primitives and intermediate groups such as two close door details plus a separate leaf. Bounded dynamic programming keeps several low-area alternatives per triangle count while distributing the budget. Validation rejects overlapping primitives or missed guarded visible pixels. Each valid candidate scores `savedFrameAreaRatio * AreaSavingsWeight - triangleCount` against the quad's `-2`; ties prefer smaller covered area. This trades submission cost for fill rate: weight 64 values each 1.5625% area saving as one extra triangle. A closed double door can share a primitive; separated leaves can use independent primitives without covering the gap.

Exact pixel-cell tracing preserves disconnected islands and transparent holes and tries a deterministic closed-contour simplification ladder. Clipper2 normalizes touching paths, offsets simplified geometry by the guard band, unions it with the exact source polygon to prevent visible clipping, and intersects the exact raster-dilated region to prevent arbitrary transparent bridges. A second constrained cleanup removes long pixel staircases: each simplified outer edge moves to the support line enclosing its entire replaced source chain before adjacent-line intersections; holes keep ordinary inward simplification. Thus stair-step corners remain covered. Earcut only triangulates resulting outer rings/holes.

Every valid detailed result up to `MaxTriangles` competes with enclosing/component candidates under the same score. Each tolerance independently generates constrained-offset, guard-band-simplified and tolerance-expanded cover families; a valid result never suppresses another family. The ladder runs to its deterministic bound, without stopping after early valid results.

Simplified families use a bounded removal beam: each step tries replacing exterior corners with supporting chords through them, preserving coverage at adjacent-edge intersections. The beam keeps lowest-area distinct states and the best result per reachable triangle count. Tolerance-expanded covers turn Douglas-Peucker error bounds into enclosure, preserving thin features. Exact pixel-cell/topology validation still applies; transparent overdraw competes through `AreaSavingsWeight`, without a separate tolerance mask.

Validation clips triangles against original visible pixel cells and compares accumulated area, requiring complete unit-square coverage rather than finite point samples. Constrained detailed contours allow only the internal one-pixel guard plus one pixel for staircase cleanup/integer rounding as additional transparent coverage. Expanded/enclosing candidates have no separate overdraw gate; scoring accounts for their actual area. Distinct contour/offset, vertex-limit, triangulation, area, coverage and tolerance-mask failures keep complexity retries separate from invalid geometry. Empty masks explicitly yield empty geometry; fully opaque, unprofitable, oversized or unsafe results yield an ordinary quad. Optional Earcut Delaunay refinement is unused: it changes neither triangle count nor area and promises no bit-identical cross-compiler output.

Baked spritev3 stores direction count/total and logical-frame duration, int32 placement and int16 movement. Shared records reference pixels/geometry only; concrete records carry RGBA and mesh kind. `SpriteResource` owns mesh types and the strict whole-resource decoder for client, particle editor and project server image loader. Its entry point takes exact resource bytes and creates its reader, independent of caller stream position; footer/trailing checks stay within that slice. It returns timing, directions, offsets, shared references, RGBA and optional geometry. Fixed-width cropped-frame coordinates/indices accompany original source size and cropped origin, which may be negative for safety-border geometry. Vertices must occupy exact serialized frame bounds. Shared frames reference the original without duplicating offset/pixels/geometry. Legacy/malformed blobs fail without layout guessing. Atlas sprites retain logical size, anchor, scaling and hit-test space while storing/rendering cropped pixels/mesh; cropping changes storage, not authored semantics. Rectangular-texture consumers (`FontManager`, `ParticleEditor`, project server sampling) restore the original logical canvas, preserving dimensions/pixel coordinates. Font glyphs use original sheet dimensions: use `ResourceManager::LoadSpriteAsQuad` to prevent mesh padding/cropping from shifting UVs. Format changes, or `SpriteMesh.*` changes without a new build hash, require `ForceBakeResources`; timestamps cannot prove matching mesh settings.

`MapBaker` writes separate server and client map blobs. The client blob serializes visible static items; its hash dictionary includes their client properties, hidden static-item client properties, and every string collected for the server blob. Thus authored `Server` values and critter/dynamic-item overrides can resolve after that map loads without putting those entities or server property records in the client blob.

`ProtoBaker` similarly carries the server prototype pack's strings in `fopro-bin-client`, while `Server` property data and applicability remain server-only. When only the client pack is stale, it still parses the server side to collect strings, without rerunning script validation or writing the current server pack. A shared future orders client writing after server-string collection; errors remain fatal. The native `ClientPackCarriesStringsOfServerOnlyProperties` section and `MapBaker`'s `BakesCrittersAndStaticItems` checks bind these boundaries to real output tables. Regenerate affected prototype/map outputs after updating the baker; unchanged source timestamps do not establish that old outputs carry the enlarged dictionaries.

Both blobs begin with `BAKED_MAP_FILE_MAGIC` and `BAKED_MAP_FILE_VERSION` from `Source/Common/MapLoader.h`. `MapLoader::ReadBakedFileHeader` validates before `MapManager::LoadStaticMaps` or `MapView::LoadStaticData` reads payloads. Hash strings pair `DataWriter::WriteString` / `DataReader::ReadString`; `DataReader::VerifyPayloadCount` preflights every other count/byte size before allocation or iteration. Missing, stale, truncated or damaged data raises `DataReadingException` rather than becoming counts. Layout changes require `BAKED_MAP_FILE_VERSION` and `ForceBakeResources` in the same change; timestamps cannot prove current layout.

`ParticleBaker` registers only enabled backends: `FO_SPARK_PARTICLES` enables text `.spark` and generated `.spk`; `FO_EFFEKSEER_PARTICLES` enables text `.efkproj` and generated `.efk`. Both default `OFF`, leaving the registered baker no particle formats.

`ParticleBaker` loads SPARK `.spark` XML with registered `SparkQuadRenderer`, writing deterministic `.spk` at the same stem. Texture paths resolve relative to `.spark`; absolute/escaping paths, unknown types, malformed XML, authored `.spk` and binary-save failures are hard errors. Never publish a graph with silently omitted objects. Client and baker share `SparkExtension.cpp` through `ClientLib`; no layout-changing headless copy belongs in `BakerLib`. Binary loading enforces exact payload length, bounded object/attribute counts, checked reads, descriptor signatures and valid typed references; custom descriptor changes require rebaking.

Effekseer `.efkproj` must be on-disk XML normalized by Editor 1.80.5, project version 3. `ParticleBaker` directly calls native C++ `EffekseerCompiler`; its fixed-profile exporter returns raw `SKFE` and dependencies. The baker validates each result with pinned C++ Effekseer Core and publishes same-stem `.efk`. The compiler belongs to `BakerLib`, never production clients. Mapper's on-demand baker follows this path for tracked edits; Web targets consume `.efk` baked before packaging.

The compiler lists referenced resources. `ParticleBaker` resolves them relative to the project, rejecting escapes from its physical directory resource source. `<BakeOutput>/.baker-cache/Effekseer/<pack>/<output>.deps` stores project/dependency paths, sizes and write times per output. Incremental checks stat the same physical files the compiler reads, following the selected project's disk source even with overlaid input directories. Changed/deleted/renamed dependencies dirty all referencing effects; unrelated files cause no corpus-wide compile. Missing/stale snapshots trigger dependency inspection before the normal single `BakeChecker` call. `ParticleBaker` removes only that effect's stale physical `.efk`; ordinary missing-output scheduling compiles it, without special timestamps or common-infrastructure callbacks.

An `.efkmodel` is not an effect container: it is a binary runtime
dependency requested by Model nodes and remains subject to the embedding
project's ordinary resource-pack/raw-copy policy.

Authored `.efk` inputs fail; sources retain reproducible `.efkproj` XML and runtime accepts generated `.efk` only. Baking proves compiler/Core parsing, without loading FOnline atlases or enforcing client/Mapper CPU Sprite/Ring capabilities. Compiler changes require forced rebaking of every `.efk`. `ModelMeshBaker` builds passive `ModelMeshData`; its common codec writes mesh-only `LFMODMSH`, schema `1`, zero flags, then recursive bone/bind/drawable data without clips/TRS tail. `ModelInfoBaker` traverses the decoded DTO; client adapts it to hashed hierarchy/render types. Neither adds a parser. Both require the header and exact consumption, rejecting headerless/unknown-schema/unknown-flag/truncated/trailing payloads with no legacy fallback. Readers preflight all lengths/counts against unread bytes before allocation. `vindex_t` bounds vertices; indices must address serialized vertices, palettes cannot exceed `MODEL_MAX_BONES`, and offset counts must match. Blend weights/indices must be finite; weights in `[0, 1]` sum to one per vertex, and integral indices address the palette. Hierarchies allow at most 1024 joints and 128 nodes per parent chain; mesh baker, `ModelInfoBaker`, source loader and client enforce applicable limits. Malformed data raises contextual `DataReadingException`, avoiding allocation, palette-bound and stack failures.

`ModelMeshBaker` rejects negative `geometry_to_world` determinants: exported negative scale reflects surfaces, making normals/winding inconsistent with lighting/culling. It never hides this by flipping normals/winding; freeze mirrored objects to positive scale in the authoring tool.

`ModelInfoBaker` enforces `Baking.ModelAttachmentMinExtent` .. `Baking.ModelAttachmentMaxExtent`. A direct `Attach` to bare `.fbx` uses static maximum-axis extent; each `.fo3d` section uses aggregate `ModelBounds` (animation-envelope union, or static geometry without mappings). Full baking checks while writing `ModelAnimationInfo.foinfo`; targeted `.fo3d` baking checks before description output. `.fo3d` `Scale` does not change the envelope or exempt centimetre-space roots. Errors name file, measured extent and limit.

Baking never treats host `AppRender::MAX_ATLAS_WIDTH` / `HEIGHT` as device contracts; portable math uses `AppRender::MIN_ATLAS_SIZE / FRAME_SCALE`. Preview zoom may exceed `Render.ModelSpriteMaxTextureWidth` / `Height` even for valid envelopes: `RefreshFrameLayout` clamps scratch textures and crops instead of terminating.

Schema 1 keeps the existing `DataWriter` native-endian mesh payload; all current
engine targets are little-endian. Unlike the explicitly little-endian Ozz
envelopes below, a future big-endian mesh consumer requires a converted wire
format and a new schema rather than interpreting schema 1 in place.

`ModelMeshBaker` validates matrices, vertex attributes, normalized colors, skin weights/offsets, 1024 total nodes and 128-node depth before writing. Values must be finite/representable as `float32`. Skinned vertices need a positive influence, resolving cluster indices and non-negative weights; retained top four normalize and recheck to sum one. Serialized vertices recheck after UV flipping/weight normalization. Errors name asset, node, field, element and component; invalid values never clamp or default to identity.

After `ufbx_generate_indices` removes byte-identical vertices, pinned `meshoptimizer` v1.2 (tag commit `9d9890c73011d75920af614485296d1e03e95448`) optimizes each drawable mesh: vertex-cache ordering reorders whole triangles for reuse; vertex-fetch ordering moves interleaved `Vertex3D` to first use and rewrites 32-bit working indices. Both use temporary buffers. Shape/ranges validate before library calls; fetch must retain every `ufbx` vertex. Indices revalidate before committing buffers/narrowing to `vindex_t`. The temporary allocator installs once before parallel jobs and uses `SafeAllocator`, preserving rpmalloc, backup-pool retry and fail-fast OOM.

Lossless reordering retains `LFMODMSH` schema `1`; runtime needs no meshoptimizer dependency. Overdraw reordering stays disabled: desktop/tiled-mobile benefits depend on workload. Quantization, codecs, LODs, meshlets and packed vertices require a separate measured schema-2 design, never an implicit wire extension.

`ModelSourceLoader` owns animation extraction, outside baked meshes. For each source FBX selected by resolved `.fo3d`, `ufbx` extracts skeleton/all clips, validates the whole asset and returns immutable data to `ModelInfoBaker`. Case-insensitively unique clip names, source/clip identity, positive finite duration, hierarchy/output pairing, all S/R/T times/values, joint/clip/key limits, shared 128-node depth and quaternion normalization validate before Ozz conversion. Each channel needs keys with finite strictly ascending times and finite intervals. One `ModelSourceAssetCache` per `ModelInfoBaker::BakeFiles()` provides single-flight shared futures by source path: parallel descriptions share a parse or exception, with FBX parsing outside the mutex.

`ModelInfoBaker` audits base skeleton/all selected external clips before `.fo3d` description output. `ModelAnimationConverter` compatibility analysis builds deterministic parent-first canonical joints; baker emits per-description reports in path order. Animation-only joints are allowed/reported as contributions. Only the unique technical FBX scene root may have an empty name. Duplicate joint names/full hierarchies or incompatible parents fail because index remapping would be ambiguous. Different source-root names normalize to the base root and are explicitly reported.

Base/external-clip rest divergence is report-only: clips author absolute local TRS, requiring no common rest pose. Reports retain every divergent clip/joint mapping and deterministic counts, maximum matrix-component delta and source. Canonical conversion keeps base local rest and identity rest for animation-only nodes; differing clip rest replaces neither fallback.

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

Direct FBX attachments are rest-only: clips in `Attach ...fbx` fail baking and require `Attach ...fo3d` with explicit mappings, since direct runtime lacks controllers/clip bindings. Validation uses the same per-call single-flight source loader.

External animation FBXs should contain only transform hierarchy/animation. Drawable external `Anim` geometry fails unless the resolved description names that exact selected file in one validation-only `AllowAnimationGeometry <file>`. Duplicate resolved exceptions, unselected targets and targets no longer containing geometry fail, preventing broad/stale allowlists. Repair duplicate geometry shared with child-model transforms by preserving helper names, parents and tracks as geometry-free helper/bone nodes; never prune whole branches. Remove exceptions with repaired exports. Project workflow owns temporary allowlists/repair inventories; reusable engine documentation lists no project assets.

Incremental `.fo3d` invalidation takes the maximum write time of the resolved description/include graph and replacement-expanded `Model`, external `Anim`, direct `Attach` and `Cut` dependencies. Missing references fail. Animation-only FBX edits leave mesh-only `LFMODMSH` unchanged but must rebuild canonical Ozz rigs and `ModelAnimationInfo`.

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

Before runtime/offline codec construction, `InitializeModelAnimationMemory()` installs a private allocator for that module's statically linked Ozz state. Arbitrary power-of-two alignment over `SafeAllocator<uint8_t>` preserves rpmalloc, backup-memory retry, OOM diagnostics and fail-fast policy. Public model APIs expose no Ozz allocator type; vendored allocator source stays byte-identical to upstream 0.16.0.

The reader requires expected caller identity, rejecting mismatches, unknown kinds, reserved flags, truncation, trailing bytes and bad payload hashes before opaque codec parsing. Non-cryptographic FNV-1a detects corruption, not authenticity. Ozz deserialization follows LF framing validation for trusted baked resources only. Authenticate attacker-rewritable packs before loading; recomputed LF hashes provide no security boundary.

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
with `LFMODINF`, schema `5`, and zero flags, followed by the existing positional
description and one required length-prefixed `LFOZZRIG` schema-1 payload. Schema 5 stores static texture name, mesh and slot without an animation flag; earlier description schemas are rejected. Rebuild matching baker/runtime and force-rebake descriptions and metadata using the current runtime marker in `Source/Common/Common.h`. Schema 2
appended the union AABB to every geometry link and schema 3 appended the counted,
`(StateAnim, ActionAnim)`-keyed per-animation boxes described above, so the client
uses the active clip's envelope instead of the union for every pose. Each step is an
incompatible binary layout: any further `LFMODINF` field bumps
`MODEL_DESCRIPTION_SCHEMA_VERSION` in the same change, and the schema-3 transition
used compatibility marker `0.0.46` so a new runtime and a full resource bake travel
together. The rig
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

The generated Ozz rig alone supplies production clips/poses; client loads no parallel TRS. `ModelAnimationController` stores LF timelines/events and direct Ozz clip index, duration, reverse and bound-joint metadata only. Sampling, joint body blending, movement replacement, procedural pre-rotations and local-to-model evaluation remain per-instance Ozz work. Cross-model clip sharing/persisted conversion caches require measured pack/runtime-memory justification.

## Script compilation relationship

`ScriptsAndBaking.cmake` also creates script compilation commands:

- `CompileAngelScript` runs the project AS compiler target when `FO_ANGELSCRIPT_SCRIPTING` is enabled.
- `CompileManagedScripts` runs the standalone project `ManagedScriptBaker` after `ForceCodeGeneration` when `FO_MANAGED_SCRIPTING` is enabled. It compiles configured `.cs` inputs into resource-pack/target assemblies and generates the `.gen.cs` surface (including the target `*Abi.gen.cs` binding manifest), `.gen.csproj`, and `.gen.sln`. The generated API files are included in the assembly freshness stamp after generation, so generator-only changes cannot leave a previously compiled DLL current; runtime payload preparation remains a separate packaging stage.

Every packed managed assembly, including copied package/project-reference DLLs, is claimed as an output of the bake. An up-to-date target reclaims the assemblies it previously shipped from the pack output; the temporary MSBuild output directory inside that pack may be removed by the outdated sweep and is not a freshness source. Dependency closure reads assembly identity and references through `Source/Scripting/Managed/ManagedAssemblyReferences.*`.

These are separate command targets from resource baking, but they share the same stage because generated/baked runtime inputs are part of the same build preparation workflow.

## Tests to inspect

Baker behavior is covered by focused tests in `Source/Tests/`:

- `Test_BakerSetup.cpp`
- `Test_ConfigBaker.cpp`
- `Test_MetadataBaker.cpp`
- `Test_RawCopyBaker.cpp`
- `Test_AudioBaker.cpp`
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
- `Test_ManagedScriptBaker.cpp`

Use the smallest test that matches the baker you changed. If CMake target names are generated by the embedding project, discover them from that project's presets/build files instead of hard-coding them here.

## Change routing

- New baker type: add/modify `Source/Tools/*Baker.*`, register it in `BaseBaker::SetupBakers()` or through `SetupBakersHook()`, and add focused tests.
- Baking command arguments: update `BuildTools/cmake/stages/ScriptsAndBaking.cmake`.
- Resource build hash behavior: update `MasterBaker::BakeAllInternal` in `Source/Tools/Baker.cpp` and related tests/build validation; the marker must remain inside the output lock.
- Metadata baking: update `Source/Tools/MetadataBaker.*` and [Generated API and Metadata](../../reference/metadata/index.md).
- Prototype format/property loading: update `BuildTools/PrototypeFormatInterface.json`, [Prototype Format](../../how-to/content/prototype-format.md), regenerate its model/reference, run `test_docs_prototype_format.py`, and include it in the aggregate contract diff.
- Map parsing/baking/materialization: update `BuildTools/MapFormatInterface.json`, [Map Format](../../how-to/content/map-format.md), regenerate its model/reference, run `test_docs_map_format.py`, include it in the aggregate contract diff, run the affected map unit tests, and rebake an embedding project.
- Image import, FOFRM composition, baked sprite records, or stock client loading/atlas/cache behavior: update `BuildTools/ImageFormatInterface.json`, [Image And Sprite Formats](../../how-to/content/image-format.md), regenerate its model/reference, run `test_docs_image_format.py`, include it in the aggregate contract diff, run focused image/atlas tests, and rebake plus visibly inspect an affected embedding project.
- Font descriptor delivery, `Baking.RawCopyFileExtensions`, FOFNT/BMFont parsing, binding, layout, or rendering: update `BuildTools/FontFormatInterface.json`, [Font Formats And Text Layout](../../how-to/content/font-format.md), regenerate its model/reference, run `test_docs_font_format.py`, include it in the aggregate contract diff, run native tests, and rebake plus visibly inspect an affected embedding project.
- Audio baking, WAV/Ogg validation and conversion, `AudioManager`, sound-name indexing, playback handles, spatial updates, settings, or frontend mixing: update `BuildTools/AudioInterface.json`, [Audio](../../how-to/content/audio.md), regenerate its model/reference, run `test_docs_audio.py`, include it in the aggregate contract diff, run native tests, and rebake plus audibly inspect an affected embedding project on every claimed platform.
- Video raw-copy delivery, Ogg/Theora decoding, `VideoClip`, fullscreen queue/input/music/drawing, or script-owned playback: update `BuildTools/VideoInterface.json`, [Video](../../how-to/content/video.md), regenerate its model/reference, run `test_docs_video.py`, include it in the aggregate contract diff, run native tests, and rebake plus visibly inspect an affected embedding project on every claimed platform.
- Effect parsing/baking/resources/runtime selection: update `BuildTools/EffectFormatInterface.json`, [Effect Format](../../how-to/content/effect-format.md), regenerate its model/reference, run `test_docs_effect_format.py`, include it in the aggregate contract diff, run focused effect-baker tests, and validate affected backends in a visible embedding-project scene.
- AngelScript bake behavior: update `Source/Tools/AngelScriptBaker.*`, the AngelScript tests, and [Scripting](../scripting-runtime/).
- Managed C# bake, generated-project, assembly, or runtime-payload behavior: update `Source/Tools/ManagedScriptBaker.*`, [Managed C# Scripting](../../how-to/scripting/managed-csharp.md), the native/Python managed tests, and the affected package/platform route.

## Validation checklist

1. Configure from an embedding project root.
2. Build the project baker application/library target if the changed path affects app/library code.
3. Run the relevant baker test(s) under `Source/Tests/`.
4. Run `BakeResources` for incremental behavior when cache/build-hash logic matters.
5. Run `ForceBakeResources` after model-source or Ozz conversion changes. This
   is the positive integration gate that parses and extracts animations from the
   project's real FBX files instead of only synthetic unit-test assets.
6. Inspect generated output only as output, not as hand-authored source.
7. Update this document and [BuildTools Pipeline](../../reference/cmake-and-buildtools/pipeline.md) if stage responsibilities change.
