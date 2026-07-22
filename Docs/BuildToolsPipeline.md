# BuildTools Pipeline

This document explains the staged CMake pipeline under `BuildTools/cmake/`. It is a source-grounded companion to [BuildWorkflow.md](BuildWorkflow.md): use `BuildWorkflow.md` for how to approach builds as a user, and this file for where the reusable build machinery lives.

## Ownership model

FOnline is normally configured from an embedding game project. The engine supplies CMake stages and helpers; the game project supplies values such as product names, main config, enabled targets, output paths, packages, scripts, and platform choices.


## Source paths inspected

- `BuildTools/Init.cmake`
- `BuildTools/cmake/stages/Init.cmake`
- `BuildTools/cmake/stages/ProjectOptions.cmake`
- `BuildTools/cmake/stages/ThirdParty.cmake`
- `BuildTools/cmake/stages/EngineSources.cmake`
- `BuildTools/cmake/stages/Codegen.cmake`
- `BuildTools/cmake/stages/CoreLibs.cmake`
- `BuildTools/cmake/stages/Applications.cmake`
- `BuildTools/cmake/stages/ScriptsAndBaking.cmake`
- `BuildTools/cmake/stages/Packages.cmake`
- `BuildTools/cmake/stages/Finalize.cmake`
- `BuildTools/cmake/helpers/Build.cmake`
- `BuildTools/cmake/helpers/Commands.cmake`
- `BuildTools/cmake/helpers/Options.cmake`
- `BuildTools/cmake/helpers/State.cmake`
- `BuildTools/cmake/helpers/WriteBuildHash.cmake`
- `BuildTools/codegen.py`
- `BuildTools/EffekseerEditor/build.ps1`
- `BuildTools/package.py`
- `BuildTools/tests/test_package_include.py`
- `BuildTools/msicreator/createmsi.py`

Important consequences:

- Do not document one game's final target list as universal engine behavior.
- Prefer stage responsibilities and option names over hard-coded generated target names.
- Validate build changes through an embedding project preset whenever possible.

## Stage files

The staged pipeline lives in `BuildTools/cmake/stages/`. Canonical stage order is defined by `BuildTools/Init.cmake`: `Init`, `ProjectOptions`, `ThirdParty`, `EngineSources`, `Codegen`, `CoreLibs`, `Applications`, `ScriptsAndBaking`, `Packages`, `Finalize`.

### `Init.cmake`

Establishes baseline configuration. It declares and checks core project options such as:

- `FO_MAIN_CONFIG`
- `FO_DEV_NAME`
- `FO_NICE_NAME`
- `FO_GEOMETRY`
- `FO_APP_ICON`
- `FO_OUTPUT_PATH`
- build feature toggles such as `FO_BUILD_CLIENT`, `FO_BUILD_SERVER`, `FO_BUILD_MAPPER`, `FO_BUILD_ASCOMPILER`, `FO_BUILD_BAKER`, `FO_UNIT_TESTS`, the scripting toggles, and the independent `FO_SPARK_PARTICLES` / `FO_EFFEKSEER_PARTICLES` particle backends.

Both particle backend options default to `OFF`. An embedding project explicitly
enables either backend or both during a migration. Backend source files remain
in the stable engine source lists and guard their implementation with the
corresponding `FO_*_PARTICLES` macro. A disabled backend contributes no
third-party target, compiled runtime or Mapper implementation, runtime resource
extensions, or baker implementation.

It also establishes build hash and common generation context. Start here when a build option is missing or validated too early/late.

### `ProjectOptions.cmake`

Normalizes and validates project-level option combinations. Examples from the current stage include checks around code coverage, build mode combinations, and scripting/tool compatibility such as `FO_BUILD_ASCOMPILER` requiring AngelScript support.

Start here when a combination of options should be rejected or derived before source lists and targets are created.

### `ThirdParty.cmake`

Adds bundled engine third-party libraries. The stage comment notes that it installs a `find_package()` interceptor before third-party `AddSubdirectory()` calls so vendored libraries cannot silently reach into the host system.

Start here when a bundled dependency is added, removed, or needs build isolation rules.

### `EngineSources.cmake`

Builds source lists and generated resource files used by later stages. It appends source lists for engine layers such as Essentials, Common, Frontend, Client, Server, Tools, Scripting, and tests. It also prepares app icon/resource data such as the generated Windows `.rc` file.

Start here when a new hand-authored source file must become part of a core engine library.

### `Codegen.cmake`

Constructs the code-generation command and output set. It passes project and engine metadata to `BuildTools/codegen.py`, including main config, build hash, generated output path, project names, embedded data capacity, metadata source files, and added common headers.

It creates codegen targets such as normal and forced code generation. Start here when generated C++/script API metadata changes.

Related doc: [GeneratedApiAndMetadata.md](GeneratedApiAndMetadata.md).

### `CoreLibs.cmake`

Creates core static libraries from the source lists prepared by `EngineSources.cmake`. Current responsibilities include libraries such as Essentials, Common, frontend/headless app layers, scripting integration libraries, client/server libraries, baker libraries, and testing support depending on enabled options.

Start here when source grouping, library dependencies, or runtime layer boundaries change.

### `ScriptsAndBaking.cmake`

Creates custom targets for script compilation and resource baking. Current responsibilities include:

- AngelScript compilation through the project AS compiler target when AngelScript scripting is enabled.
- Mono script compilation through `BuildTools/compile-mono-scripts.py` when Mono scripting is enabled.
- Resource baking through the project baker target.
- Build-hash/write-hash support for baked resources.
- Normal and forced bake targets.

Related docs: [BakingPipeline.md](BakingPipeline.md) and [Scripting.md](Scripting.md).

### `Applications.cmake`

Creates executable and shared-library applications from `Source/Applications/*.cpp`. It uses helpers such as `AddExecutableApplication` and `AddSharedApplication` and project variables such as `FO_DEV_NAME`, output paths, platform flags, and enabled build modes.

Examples of entry points wired here include client, client runtime library, client headless variants, server variants, mapper/editor/tool apps, baker, AngelScript compiler, and testing app depending on options.

Effekseer Editor is intentionally absent from this stage and from the
application target graph. Its standalone `BuildTools/EffekseerEditor/build.ps1`
entry point configures and builds upstream sources independently of an
embedding project's FOnline CMake configuration.

See [Applications.md](Applications.md).

### `Packages.cmake`

Creates package targets from `FO_PACKAGES` and calls `BuildTools/package.py` with project context such as main config, build hash, developer name, nice name, input/output paths, platform/architecture/config data, and binary-output postfix.

`package.py` owns the reusable package payload layout and optional post-processing. For a Windows Client package that includes the `Wix` pack, it invokes `msicreator/createmsi.py` to build an MSI after the Raw payload is staged: the MSI gets the temporary `INSTALLED` marker used by installed-client writable-path resolution, registers the deep-link URI scheme, and creates Start Menu + Desktop shortcuts and an Add/Remove Programs icon. The MSI is a **required** artifact when the `Wix` pack is requested — a missing toolset (`wixl` on POSIX hosts — on Debian/Ubuntu it ships in its own `wixl` apt package, not in `msitools`; WiX `candle`/`light` on Windows) or a generator/build error fails the package (it is not a silent best-effort step). All installer values are read from the embedding project's config, so the packager stays game-agnostic:

- product/manufacturer/comments name ← `Common.GameName` (falls back to the package nice name)
- `ProductVersion` ← `Common.GameVersion`, with `$FILE{...}` indirection resolved relative to the main config directory (so a `$FILE{VERSION}` setting yields the real numeric version, not a `0.0.0` fallback)
- deep-link URI scheme ← `Auth.UriScheme`
- stable WiX `UpgradeCode` ← `Packaging.MsiUpgradeCode` (required; must never change once an MSI has shipped)
- Add/Remove Programs icon ← `Packaging.AppIcon` (optional)
- install directory and MSI base name ← the package nice name

The portable Raw/Zip artifacts are finalized before the MSI step and never carry the `INSTALLED` marker, so they stay portable.

When several package parts append to one `SingleZip`, byte-identical files at
the same archive path are coalesced into one entry. Different contents at the
same path are a packaging error; the packager never emits ambiguous duplicate
ZIP names.

The universal package schema has no `EffekseerEditor` binary role. Separately
built tools are declared alongside `BINARY` parts with
`INCLUDE <source-path-glob> <target-path-in-pack>`. The source glob is relative
to `FO_OUTPUT_PATH`. After the ordinary binary parts are assembled, the generic
packager replaces the included target tree and updates an existing `SingleZip`
without duplicate or stale entries. This path is covered by
`BuildTools/tests/test_package_include.py`.

Start here when platform package layout, package target naming, package script arguments, or package-time installer metadata changes.

### `Finalize.cmake`

Performs final solution/project organization and late reporting. Current responsibilities include target folder grouping, optional ReSharper settings copy, third-party dummy grouping, and verbose cache-variable reporting when `FO_VERBOSE_BUILD` is enabled.

Start here for final target organization or post-generation diagnostics, not for source ownership or build feature validation.

## Helper files

Reusable helpers live in `BuildTools/cmake/helpers/`:

- `Build.cmake` — build/target creation helpers.
- `Commands.cmake` — command target helpers.
- `Options.cmake` — option/value helpers.
- `State.cmake` — staged pipeline state/hook support.
- `WriteBuildHash.cmake` — writes build-hash state used by generation/baking flows.

When a stage needs reusable behavior, prefer adding a helper here instead of copy-pasting logic between stages.

## Stage hooks

Stage comments reference the hook convention:

```cmake
AddStageHook(<StageName> Pre|Post <macro-name>)
```

Use hooks when an embedding project or a later refactor needs to extend stage behavior without editing the middle of a stage body. Keep hook behavior documented near the owning stage or in the project docs if it is game-specific.

## Change routing

- New project option or option validation: `Init.cmake` / `ProjectOptions.cmake`.
- New vendored dependency: `ThirdParty.cmake`.
- New engine source file: `EngineSources.cmake` and maybe `CoreLibs.cmake`.
- New generated metadata/API behavior: `Codegen.cmake` and [GeneratedApiAndMetadata.md](GeneratedApiAndMetadata.md).
- New script compile or resource bake behavior: `ScriptsAndBaking.cmake`, [BakingPipeline.md](BakingPipeline.md), and [Scripting.md](Scripting.md).
- New executable/tool entry point: `Applications.cmake` and [Applications.md](Applications.md).
- Auxiliary-tool build recipes: `BuildTools/buildtools.py build-auxiliary`,
  `BuildTools/EffekseerEditor/build.ps1`, and [Tools.md](Tools.md).
- New package layout or installer metadata: `Packages.cmake`, `BuildTools/package.py`, `BuildTools/msicreator/createmsi.py`, plus platform docs.
- Final target organization or verbose diagnostics: `Finalize.cmake`.

## Validation checklist

For BuildTools changes:

1. Configure from a real embedding project root.
2. Use the narrowest preset that exercises the changed stage.
3. For source-list changes, verify the affected target builds.
4. For codegen changes, verify generated files and script API consumers.
5. For baking changes, run normal and forced bake paths when relevant.
6. For Effekseer Editor changes, run `buildtools.py build-auxiliary
   effekseer-editor Release` on Windows win64 and inspect the staged
   managed/native/resources payload; exercise the package `INCLUDE` when the
   developer-package layout changes.
7. For package changes, run the affected package target and inspect output layout; for WiX/MSI changes, also verify the generated installer config/registry values or run the installer build on a host with WiX/wixl.
8. Run documentation link checks if docs changed.
9. Run `git diff --check` before reporting completion.
