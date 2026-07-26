# Tools

> Engine-owned documentation. This page maps reusable engine tools in `Source/Tools/` and their application entry points. Concrete game content pipelines, project-specific command lines, and release automation belong in the embedding project's docs unless they are explicitly marked as examples.

## Purpose

Use this page when you need to find which tool owns a workflow, where an application enters the tool layer, or which deeper document owns the implementation details.

The FOnline tool layer centers interactive editing in Mapper. A separately
built upstream Effekseer Editor is available as an optional companion authoring
tool. Together these paths contain:

- command-line resource/script/metadata bakers;
- Mapper and its integrated particle editor;
- asset-type-specific processors for images, effects, models, maps, protos, text, configs, and raw copies;
- application wrappers under `Source/Applications/` that initialize the engine platform layer and run a tool.

## Source paths inspected

- `Source/Tools/Baker.h`
- `Source/Tools/Baker.cpp`
- `Source/Tools/*Baker.h`
- `Source/Tools/*Baker.cpp`
- `Source/Tools/Mapper.h`
- `Source/Tools/Mapper.cpp`
- `Source/Tools/SparkParticleEditor.h`
- `Source/Tools/SparkParticleEditor.cpp`
- `Source/Applications/BakerApp.cpp`
- `Source/Applications/ASCompilerApp.cpp`
- `Source/Applications/MapperApp.cpp`
- `Source/Applications/TestingApp.cpp`
- `BuildTools/EffekseerEditor/build.ps1`
- `BuildTools/buildtools.py`
- `ThirdParty/Effekseer/Dev/Editor/`
- `ThirdParty/Effekseer/Dev/Cpp/Viewer/`
- relevant tool/baker tests under `Source/Tests/`

## Tool layer map

The tool layer has three main shapes:

1. **Batch command tools** — `BakerApp.cpp` and `ASCompilerApp.cpp` run deterministic file transformations from project settings and resource packs.
2. **Interactive runtime tool** — `MapperApp.cpp` creates the frontend window and drives Mapper's per-frame map/content editing loop.
3. **Reusable processors** — `Source/Tools/*Baker.*`, `Mapper.*`, and the particle-subeditor implementations provide the reusable work behind the apps.

For CMake target creation and package wiring, see [Applications.md](Applications.md) and [BuildToolsPipeline.md](BuildToolsPipeline.md). For resource bake internals, see [BakingPipeline.md](BakingPipeline.md).

## Application entry points

### Baker app

`Source/Applications/BakerApp.cpp` initializes the application layer with `DisableLogTags`, constructs `MasterBaker`, calls `BakeAll()`, and exits with the bake result.

Use it for full resource baking. `BuildTools/cmake/stages/ScriptsAndBaking.cmake` wires the `BakeResources` and `ForceBakeResources` command targets to the project baker app.

### AngelScript compiler app

`Source/Applications/ASCompilerApp.cpp` prepares metadata and compiles AngelScript resource packs. It runs metadata baking first for resource packs that include `MetadataBaker`, then compiles packs that include `AngelScriptBaker`.

Use it for the `CompileAngelScript` build path. The broader script runtime is documented in [Scripting.md](Scripting.md).

### Mapper app

`Source/Applications/MapperApp.cpp` initializes the frontend/application layer, waits for persistent data readiness on Web builds, constructs `MapperEngine`, locks input when running headless, calls `MapperMainLoop()` every frame, and shuts the mapper down on exit.

Use it for map editing, mapper-side automation, and headless mapper-based workflows. See [MapperTools.md](MapperTools.md) for lifecycle and mapper-specific helper details.

### Testing app

`Source/Applications/TestingApp.cpp` is the test runner entry point. Use [../Source/Tests/README.md](../Source/Tests/README.md) and future testing docs for suite ownership.

## Baking tools

The baking family is the most mature batch tool group. `Source/Tools/Baker.h` / `.cpp` define the common infrastructure:

- `BakingContext` carries settings, pack name, write callback, existing baked files, and sync/async mode hints.
- `BaseBaker` defines `GetName()`, `GetOrder()`, and `BakeFiles()`.
- `BaseBaker::SetupBakers()` constructs requested built-in bakers and exposes `SetupBakersHook()` for project/external extension.
- `MasterBaker::BakeAll()` coordinates full resource-pack baking.
- `BakerDataSource` adapts baked/input files to the engine data-source interface.

Built-in baker implementations:

- `Source/Tools/MetadataBaker.*` — parses metadata tags and produces metadata resources used by runtime registration.
- `Source/Tools/ConfigBaker.*` — bakes configuration resources.
- `Source/Tools/RawCopyBaker.*` — copies selected resources without transformation.
- `Source/Tools/ImageBaker.*` — imports image/sprite/frame formats including classic Fallout-family formats and PNG/TGA.
- `Source/Tools/EffectBaker.*` — bakes shader/effect sources and shader stages.
- `Source/Tools/ParticleBaker.*` — converts native SPARK `.spark` XML to
  memory-loadable `.spk` and compiles Effekseer `.efkproj` XML to validated raw
  `.efk` (`SKFE`) resources. Authored `.spk`/`.efk` runtime binaries are
  rejected.
- `Source/Tools/ProtoBaker.*` — bakes prototype files with metadata/script-aware validation.
- `Source/Tools/MapBaker.*` — bakes map files and validates map/proto relationships.
- `Source/Tools/TextBaker.*` — bakes text packs.
- `Source/Tools/ProtoTextBaker.*` — bakes prototype text data.
- `Source/Tools/ModelMeshBaker.*` — bakes model mesh data when 3D support is enabled.
- `Source/Tools/ModelInfoBaker.*` — bakes model descriptions and the common model-animation duration table when 3D support is enabled.
- `Source/Tools/AngelScriptBaker.*` — compiles/bakes AngelScript bytecode resources when AngelScript support is enabled.

Detailed bake ordering, settings, output writing, and validation live in [BakingPipeline.md](BakingPipeline.md).

## Interactive developer tools

### Mapper

`Source/Tools/Mapper.h` / `.cpp` implement `MapperEngine`, a mapper-specific client-like runtime for editing maps and map entities. It derives through the client/view side of the engine, registers mapper metadata, sets up sprite factories, processes input, draws map/editor UI frames, loads/saves maps, and exposes mapper automation helpers to scripts.

Main areas inside `Mapper.cpp` include:

- `MapperMainLoop()` / `DrawMapperFrame()` frame lifecycle;
- input handling and cursor/hex helpers;
- ImGui panels for workspace, content, map list, map window, inspector, history, settings, console, script calls, and neutral particle-subeditor dispatch;
- map loading/showing/saving through `LoadMapFromText()`, `LoadMap()`, `ShowMap()`, `SaveCurrentMap()`, and `SaveMap()`;
- mapper script system integration through mapper metadata and `MapperGlobalScriptMethods.cpp`.

See [MapperTools.md](MapperTools.md) for the mapper lifecycle, extension points, and known headless-render workflow.

### SPARK particle editor

`Source/Tools/ParticleEditor.h` / `.cpp` define the virtual Mapper particle-subeditor boundary and the backend-neutral **Windows -> Particle preview** window. `Source/Tools/SparkParticleEditor.h` / `.cpp` define the SPARK-specific subeditor and asset editor. Its browser enumerates raw `.spark` sources and opens one editor window per selected asset. Each window previews the memory-backed resource, exposes native object parameters (including FOnline's `SparkQuadRenderer`), saves XML back through Mapper's raw resource filesystem, and confirms Save / Discard / Cancel when closing with changes. Effekseer authoring uses the separately built upstream Editor described below; native Mapper previews the `.efk` emitted by its baking data source and remains the final check against FOnline's own renderer.

`Source/Tools/EffekseerCompiler.h/.cpp` is the native C++ fixed-profile compiler
used directly by `ParticleBaker`. It accepts Editor-1.80.5, project-version-3
`.efkproj` XML and returns raw `SKFE` bytes plus referenced resource paths.
Runtime targets consume only the generated binary and do not carry the compiler;
Web consumes pre-baked `.efk`.

The SPARK editor is compiled only with `FO_SPARK_PARTICLES`. The neutral preview
is always part of Mapper, asks the registered particle sprite factory which
extensions are available, and hides its menu entry when no runtime is enabled.
Both runtime options default to `OFF`, so an embedding project must opt into the
particle support it needs.

### Effekseer Editor developer bundle

`BuildTools/EffekseerEditor/build.ps1` builds the pinned source-only upstream
English-only Editor as an isolated Windows win64 developer tool. The script
publishes a self-contained .NET 10 managed UI alongside the native Viewer,
Direct3D 11/OpenGL material compilers, material editor, English localization,
icons, meshes, and tool license. Both Editor
processes use a 17.4 KiB English-only Source Han subset instead of the upstream
17.4 MiB CJK font collection. It is not an FOnline application entry point and
none of these UI/viewer components is linked into a runtime library.
The isolated native build consumes the engine-owned zlib, libpng, Ogg, and
Vorbis source trees instead of retaining duplicates below Effekseer. Its local
docking-imgui remains required because the Viewer backends use the
multi-viewport ABI absent from the engine's standard imgui branch.

The Editor has no engine CMake option or target and is not a universal
`buildtools.py build` selection. Invoke it through `buildtools.py
build-auxiliary effekseer-editor Release` on Windows; the registered recipe
configures only the isolated upstream native build. The reusable packager has
no `EffekseerEditor` binary role. An embedding project uses the generic package
`INCLUDE` declaration to copy a prebuilt payload into its developer package and
update an existing `SingleZip`.

The FOnline adaptation makes authoring source-first. Editor **Save** and
**Save As** write normalized `.efkproj` XML. The Editor's stock preview remains
available for authoring iteration. GIF recording is disabled in the FOnline
bundle; Mapper preview is still required to verify the baked effect through
FOnline's renderer and supported-capability gate.

There is no separate generic Editor application or `EditorLib`; Mapper is the engine's central interactive editing tool.

## Ownership boundaries

Use engine docs for:

- app/tool entry points and their reusable lifecycle;
- baker types and their engine-owned transformations;
- Mapper and particle-editor extension points;
- validation checklists and tests;
- links to CMake/app wiring.

Use embedding-project docs for:

- concrete content folder policy;
- game-specific map/proto/text conventions;
- product-specific generated outputs;
- exact preset names or binary names such as `LF_Mapper` unless clearly marked as an example;
- downstream pipelines that consume tool output for a specific game feature.

## Tests to inspect

Baker/tool behavior is covered by focused tests in `Source/Tests/`:

- `Test_BakerSetup.cpp`
- `Test_ConfigBaker.cpp`
- `Test_MetadataBaker.cpp`
- `Test_RawCopyBaker.cpp`
- `Test_ImageBaker.cpp`
- `Test_EffectBaker.cpp`
- `Test_ParticleBaker.cpp`
- `Test_ProtoBaker.cpp`
- `Test_ProtoTextBaker.cpp`
- `Test_MapBaker.cpp`
- `Test_TextBaker.cpp`
- `Test_ModelBaker.cpp`
- `Test_AngelScriptBaker.cpp`

Mapper UI behavior is less directly covered by focused unit tests. Validate those paths through Mapper itself.

## Change routing

- Full resource bake orchestration: `Source/Tools/Baker.*`, `Source/Applications/BakerApp.cpp`, [BakingPipeline.md](BakingPipeline.md).
- Script bytecode compilation: `Source/Tools/AngelScriptBaker.*`, `Source/Applications/ASCompilerApp.cpp`, [Scripting.md](Scripting.md).
- Metadata tags and generated metadata resources: `Source/Tools/MetadataBaker.*`, [GeneratedApiAndMetadata.md](GeneratedApiAndMetadata.md).
- Map editing and headless mapper automation: `Source/Tools/Mapper.*`, `Source/Applications/MapperApp.cpp`, [MapperTools.md](MapperTools.md).
- Particle editor boundary and preview: `Source/Tools/ParticleEditor.*`; Mapper only invokes its neutral lifecycle and drawing hooks.
- SPARK particle editor implementation: `Source/Tools/SparkParticleEditor.*`, composed behind the neutral particle-editor boundary.
- Effekseer authoring-tool build/staging and optional generic package include: `BuildTools/buildtools.py`, `BuildTools/EffekseerEditor/build.ps1`, `ThirdParty/Effekseer/Dev/Editor/`, and [BuildToolsPipeline.md](BuildToolsPipeline.md).
- Application target wiring: [Applications.md](Applications.md) and [BuildToolsPipeline.md](BuildToolsPipeline.md).

## Validation checklist

1. For baker changes, run the smallest affected baker test and then the project bake target when behavior crosses resource-pack boundaries.
2. For script compile changes, run AngelScript baker/compiler tests and the project `CompileAngelScript` path.
3. For mapper changes, launch the mapper path that owns the change; for headless flows, verify the generated output rather than only process exit.
4. For particle UI changes, validate the interactive Mapper path on the affected platform/backend.
5. For Effekseer Editor changes, run `buildtools.py build-auxiliary
   effekseer-editor Release` on Windows win64, launch its managed UI, save a
   text `.efkproj`, then bake and preview that project in Mapper. Exercise the
   generic package `INCLUDE` when its staged layout changes.
6. If tool target wiring changes, inspect [Applications.md](Applications.md), [BuildToolsPipeline.md](BuildToolsPipeline.md), and the generated CMake targets from an embedding project.
7. Keep game-specific tool pipelines in the embedding project's docs and link to engine docs for reusable mechanics.
