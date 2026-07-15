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
