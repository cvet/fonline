---
layout: default
title: BuildTools Pipeline
locale: en
document_id: buildtools-pipeline
permalink: /Docs/en/reference/cmake-and-buildtools/pipeline.html
---

# BuildTools Pipeline

This document explains the staged CMake pipeline under `BuildTools/cmake/`. It is a source-grounded companion to [Build Workflow](../../how-to/build/): use the workflow guide for how to approach builds as a user, this file for implementation ownership, [ProjectDependencies.md](../../../ProjectDependencies.md) for project-local targets and role linking, the [generated CMake reference](../cmake/index.md) for exact project-facing CMake declarations, the [generated helper CLI reference](../helper-cli/index.md) for executable helper syntax/ownership, the [generated native-extension reference](../../../generated/native-extension/index.md) for source roles and hooks, and the [generated package reference](../packages/index.md) for `DefinePackage` and payload contracts.

## Ownership model

FOnline is normally configured from an embedding game project. The engine supplies CMake stages and helpers; the game project supplies values such as product names, main config, enabled targets, output paths, packages, scripts, and platform choices.

## Interface decision

Use the existing owner instead of inventing a second interface. For a CMake
option, precedence is the matching `FO_` environment variable, then an existing
CMake cache or `-D` value, then project `SetOption` value, then the declared
interface default. The Engine supplies CMake stages and helpers; the game
project supplies values.

For commands, `BuildTools/buildtools.py create_parser()` owns the main CLI,
individual helper-script parsers own their command lines, and `package.py` plus
the package declaration own payload contracts. Helper CLIs are revision-pinned
implementation interfaces: automation must pin an Engine revision and consume
`BuildTools/HelperCliInterface.json` or its generated reference rather than
guessing cross-revision compatibility.


## Source paths inspected

- `BuildTools/Init.cmake`
- `BuildTools/cmake/ProjectInterface.json`
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
- `BuildTools/cmake/helpers/RunAndLog.cmake`
- `BuildTools/cmake/helpers/State.cmake`
- `BuildTools/cmake/helpers/WriteBuildHash.cmake`
- `BuildTools/codegen.py`
- `BuildTools/EffekseerEditor/build.ps1`
- `BuildTools/compile-mono-scripts.py`
- `BuildTools/codecoverage.py`
- `BuildTools/android_device.py`
- `BuildTools/web/simple-web-server.py`
- `BuildTools/HelperCliInterface.json`
- `BuildTools/docs_helper_cli.py`
- `BuildTools/docs_cmake.py`
- `BuildTools/PackageInterface.json`
- `BuildTools/docs_package.py`
- `BuildTools/tests/validate_package_interface.cmake`
- `BuildTools/tests/validate_project_interface.cmake`
- `BuildTools/package.py`
- `BuildTools/tests/test_package_include.py`
- `BuildTools/msicreator/createmsi.py`

Important consequences:

- Do not document one game's final target list as universal engine behavior.
- Prefer stage responsibilities and option names over hard-coded generated target names.
- Validate build changes through an embedding project preset whenever possible.

## Stage files

The staged pipeline lives in `BuildTools/cmake/stages/`. Canonical stage order, entrypoint names, and hook points are declared in `BuildTools/cmake/ProjectInterface.json`, loaded by `BuildTools/Init.cmake`, and rendered in the [stage reference](../cmake/stages.md). The loader rejects duplicate names/entrypoints, non-contiguous order, and unsupported hook points before an embedding project executes a stage.

### `Init.cmake`

Establishes baseline configuration. It declares every public project option from `BuildTools/cmake/ProjectInterface.json`, then checks required values and establishes the build hash and common generation context. The exact required inputs, cache types, defaults, allowed values, categories, and override precedence are generated in the [options reference](../cmake/options.md). Start in the manifest when a public option is added or changed; start in this stage when its configure-time behavior is wrong.

The manifest includes the independent `FO_SPARK_PARTICLES` and `FO_EFFEKSEER_PARTICLES` backends. Both default to `OFF`; an embedding project can enable either or both during a migration. Backend source files remain in stable engine source lists and guard their implementations with the corresponding macro. A disabled backend contributes no third-party target, compiled runtime or Mapper implementation, runtime resource extensions, or baker implementation.

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

Related doc: [GeneratedApiAndMetadata.md](../metadata/index.md).

### `CoreLibs.cmake`

Creates core static libraries from the source lists prepared by `EngineSources.cmake`. Current responsibilities include libraries such as Essentials, Common, frontend/headless app layers, scripting integration libraries, client/server libraries, baker libraries, and testing support depending on enabled options.

`EngineSources.cmake` includes the native `EffekseerCompiler.h/.cpp` module in
`BakerLib`. With Effekseer particles enabled, `ParticleBaker` calls it directly
to compile fixed Editor-1.80.5 `.efkproj` XML and obtain each project's
referenced-resource list for the per-effect path/size/write-time snapshot under
`BakeOutput/.baker-cache`. Runtime libraries and Web clients do not depend on a
compiler target or host process; they consume pre-baked `.efk`. A server-only
build no longer enables BakerLib merely because `FO_BUILD_SERVER` is set.

Start here when source grouping, library dependencies, or runtime layer boundaries change.

### `Applications.cmake`

Creates executable and shared-library applications from `Source/Applications/*.cpp`. It uses helpers such as `AddExecutableApplication` and `AddSharedApplication` and project variables such as `FO_DEV_NAME`, output paths, platform flags, and enabled build modes.

Examples of entry points wired here include client, client runtime library, client headless variants, server variants, mapper, animation and particle viewers, baker, AngelScript compiler, and testing app depending on options. There is no generic Editor application or validation target.

Effekseer Editor is intentionally absent from this stage and from the
application target graph. Its standalone `BuildTools/EffekseerEditor/build.ps1`
entry point configures and builds upstream sources independently of an
embedding project's FOnline CMake configuration.

For Visual Studio/MSBuild test targets, the stage invokes the test executable through `BuildTools/cmake/helpers/RunAndLog.cmake`. The helper captures stdout and stderr in `<build-dir>/<target>.log` and fails the CMake command from the real process exit code. This preserves expected negative-test diagnostics without letting MSBuild reinterpret lines containing words such as `error` as build failures. Other generators run the executable directly.

See [Applications](../applications.md).

### `ScriptsAndBaking.cmake`

Creates custom targets for script compilation and resource baking. Current responsibilities include:

- AngelScript compilation through the project AS compiler target when AngelScript scripting is enabled.
- Mono script compilation through `BuildTools/compile-mono-scripts.py` when Mono scripting is enabled. CMake passes `FO_OUTPUT_PATH` explicitly as the required scripts/project directory and appends each `FO_MONO_ASSEMBLIES` entry.
- Resource baking through the project baker target.
- Build-hash/write-hash support for baked resources.
- Normal and forced bake targets.
- The public `AddBakingTarget(<target> [SUB_CONFIG <name>] [FORCE] [COMMENT <text>])` helper for project-owned bake variants. Call it after `SetupScriptsAndBaking()` so the project baker exists; every added target reuses the standard codegen dependency, output working directory, config application, and resource build-hash update.

Related docs: [Baking Pipeline](../../explanation/content-pipeline/baking.md) and [Scripting](../../explanation/scripting-runtime/).

### `Packages.cmake`

Creates package targets from `FO_PACKAGES` and calls `BuildTools/package.py` with project context such as main config, build hash, developer name, nice name, input/output paths, platform/architecture/config data, and the current `BINARY` entry's optional output postfix.

`DefinePackage` declaration clauses, accepted runtime targets/platforms/architectures, pack tokens, support status, and payload effects are versioned in `BuildTools/PackageInterface.json` and rendered in the [generated package reference](../packages/index.md). `package.py` consumes the same manifest to reject unknown, duplicate, placeholder, unsupported-platform, target-incompatible, modifier-only, and invalid-architecture package requests before staging output. The embedding project still owns which valid combinations it declares.

`POSTFIX <value>` follows a single `BINARY` clause and is never inherited by sibling entries. It must match the `FO_BINARY_OUTPUT_POSTFIX` used when that binary was built, because both sides participate in the input-directory name and packaged runtime identity. The `win32-win7` and `win64-win7` package architecture keys resolve to canonical `win32` and `win64` binary architectures; their legacy toolset choice comes from `buildtools.py`, while an explicit postfix such as `POSTFIX Win7` keeps the produced Raw/Zip/Wix names distinct. Run `BuildTools/check_windows7_imports.py` against every linked Win7 PE before packaging or publication.

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

Start in `Packages.cmake` when package target wiring changes. Start in `BuildTools/PackageInterface.json` plus `package.py` when declaration vocabulary, supported combinations, payload layout, artifact behavior, packager arguments, or package-time installer metadata changes.

### `Finalize.cmake`

Performs final solution/project organization and late reporting. Current responsibilities include target folder grouping, optional ReSharper settings copy, third-party dummy grouping, and verbose cache-variable reporting when `FO_VERBOSE_BUILD` is enabled.

Start here for final target organization or post-generation diagnostics, not for source ownership or build feature validation.

## Helper files

Reusable helpers live in `BuildTools/cmake/helpers/`:

- `Build.cmake` — build/target creation helpers.
- `Commands.cmake` — command target helpers.
- `Options.cmake` — option/value helpers.
- `RunAndLog.cmake` — internal script-mode process runner that captures test output and propagates the exit code.
- `State.cmake` — staged pipeline state/hook support.
- `WriteBuildHash.cmake` — writes build-hash state used by generation/baking flows.

When a stage needs reusable behavior, prefer adding a helper here instead of copy-pasting logic between stages.

Helper location does not make a command public. Only the selected commands declared in `BuildTools/cmake/ProjectInterface.json` and rendered in the [helper reference](../cmake/helpers.md) are the documented embedding-project surface; all other helper commands remain internal implementation details.

## Stage hooks

Stage comments reference the hook convention:

```cmake
AddStageHook(<StageName> Pre|Post <macro-name>)
```

Use hooks when an embedding project or a later refactor needs to extend stage behavior without editing the middle of a stage body. Keep hook behavior documented near the owning stage or in the project docs if it is game-specific.

The generated [stage and hook reference](../cmake/stages.md) is authoritative for supported stage names, entrypoints, and hook positions.

## Change routing

- New project option: `BuildTools/cmake/ProjectInterface.json`; option application/validation: `Init.cmake` / `ProjectOptions.cmake`.
- New vendored dependency: `ThirdParty.cmake`.
- New project-local dependency or role link: [ProjectDependencies.md](../../../ProjectDependencies.md), the pinned revision's consumed `FO_*_LIBS` list, and the embedding-project target/package matrix.
- New engine source file: `EngineSources.cmake` and maybe `CoreLibs.cmake`.
- New generated metadata/API behavior: `Codegen.cmake` and [GeneratedApiAndMetadata.md](../metadata/index.md).
- New helper command or argument: the executable `create_parser()`, `BuildTools/HelperCliInterface.json`, [helper CLI reference](../helper-cli/index.md), and `BuildTools/docs_helper_cli.py`.
- New project-native source role, hook, or binding rule: `BuildTools/NativeExtensionInterface.json`, [NativeExtensions.md](../../../NativeExtensions.md), [generated/native-extension/index.md](../../../generated/native-extension/index.md), and `BuildTools/docs_native_extension.py`.
- New script compile or resource bake behavior: `ScriptsAndBaking.cmake`, [Baking Pipeline](../../explanation/content-pipeline/baking.md), and [Scripting](../../explanation/scripting-runtime/).
- New executable/tool entry point: `Applications.cmake` and [Applications](../applications.md).
- Auxiliary-tool build recipes: `BuildTools/buildtools.py build-auxiliary`,
  `BuildTools/EffekseerEditor/build.ps1`, and [Tools.md](../../../Tools.md).
- New package declaration, support combination, layout, or artifact: `BuildTools/PackageInterface.json`, `Packages.cmake`, `BuildTools/package.py`, [generated package reference](../packages/index.md), `BuildTools/msicreator/createmsi.py` when relevant, plus platform docs.
- Final target organization or verbose diagnostics: `Finalize.cmake`.

## Validation checklist

For BuildTools changes:

1. Run `cmake -P BuildTools/tests/validate_project_interface.cmake` for project-interface changes.
2. Run `python BuildTools/tests/test_docs_cmake.py` and `python BuildTools/docs_cmake.py --check` after regenerating the CMake reference.
3. Configure from a real embedding project root.
4. Use the narrowest preset that exercises the changed stage.
5. For source-list changes, verify the affected target builds.
6. For codegen changes, verify generated files and script API consumers.
7. For baking changes, run normal and forced bake paths when relevant.
8. For Effekseer Editor changes, run `buildtools.py build-auxiliary
   effekseer-editor Release` on Windows win64 and inspect the staged
   managed/native/resources payload; exercise the package `INCLUDE` when the
   developer-package layout changes.
9. For package changes, run the affected package target and inspect output layout; for WiX/MSI changes, also verify the generated installer config/registry values or run the installer build on a host with WiX/wixl.
10. For package-interface changes, run `python BuildTools/tests/test_docs_package.py`, `cmake -P BuildTools/tests/validate_package_interface.cmake`, and `python BuildTools/docs_package.py --check` after regeneration.
11. For native-extension interface changes, run `python BuildTools/tests/test_docs_native_extension.py`, `cmake -P BuildTools/tests/validate_native_extension_interface.cmake`, and `python BuildTools/docs_native_extension.py --check` after regeneration.
12. Compare all regenerated API/CMake/main-CLI/package/helper-CLI/native-extension models with the intended base through `BuildTools/docs_contract_diff.py`; complete any required [contract disposition](../../contributing/contract-change-management.md).
13. Run documentation link checks if docs changed.
14. Run `git diff --check` before reporting completion.
