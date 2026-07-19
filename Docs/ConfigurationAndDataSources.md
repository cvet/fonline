# Configuration and Data Sources

> Engine-owned documentation. This page explains reusable configuration parsing, runtime settings, mounted data sources, file lookup, and cache storage. Project-specific config values and content folder policy belong to the embedding project.

## Purpose

Use this page when changing how the engine reads `.fomain`/config data, applies command-line or sub-config overrides, mounts resource directories/packs, reads files, or stores cached resource data.

Read this together with:

- [BuildWorkflow.md](BuildWorkflow.md) for configure/build entry points.
- [BakingPipeline.md](BakingPipeline.md) for resource-pack production.
- [GeneratedApiAndMetadata.md](GeneratedApiAndMetadata.md) for generated settings and metadata inputs.
- [ClientRuntime.md](ClientRuntime.md), [ServerRuntime.md](ServerRuntime.md), and [Tools.md](Tools.md) for runtime/tool consumers.

## Source paths inspected

- `Source/Common/ConfigFile.h`
- `Source/Common/ConfigFile.cpp`
- `Source/Common/Settings.h`
- `Source/Common/Settings.cpp`
- `Source/Common/Settings.inc`
- `Source/Common/DataSource.h`
- `Source/Common/DataSource.cpp`
- `Source/Common/FileSystem.h`
- `Source/Common/FileSystem.cpp`
- `Source/Common/CacheStorage.h`
- `Source/Common/CacheStorage.cpp`
- `Source/Essentials/DiskFileSystem.h`
- `Source/Essentials/DiskFileSystem.cpp`
- `Source/Essentials/Platform.h`
- `Source/Essentials/Platform.cpp`
- `Source/Frontend/ApplicationInit.cpp`
- `Source/Client/Client.cpp`
- `Source/Client/Updater.cpp`
- `Source/Client/ResourceManager.h`
- `Source/Client/ResourceManager.cpp`
- `Source/Tools/Baker.h`
- `Source/Tools/Baker.cpp`
- `Source/Tools/ConfigBaker.h`
- `Source/Tools/ConfigBaker.cpp`
- `BuildTools/cmake/stages/Codegen.cmake`
- `BuildTools/cmake/stages/ScriptsAndBaking.cmake`
- `BuildTools/cmake/stages/Packages.cmake`
- related tests under `Source/Tests/`

## Layer map

1. **Config text parser** — `ConfigFile` parses sections, keys, values, repeated sections, optional collected content, and first-section reads.
2. **Settings model** — `Settings.inc` declares setting groups; `Settings.*` turns config files, command-line overrides, internal config, defaults, auto-settings, sub-configs, and resource-pack declarations into `GlobalSettings`.
3. **Data-source abstraction** — `DataSource` mounts disk directories and pack files behind a uniform file-list/open interface.
4. **File-system view** — `FileSystem` combines mounted data sources, exposes `FileHeader`, `File`, `FileReader`, and `FileCollection`, and resolves file reads by path/name.
5. **Cache storage** — `CacheStorage` persists named string/data entries for reusable cache consumers.
6. **Low-level disk access** — `DiskFileSystem` performs direct disk operations below mounted engine resources.

## Config parsing

`Source/Common/ConfigFile.*` owns syntax-level parsing. `ConfigFileOption` controls optional behavior:

- `CollectContent` preserves section content for consumers that need raw block text.
- `SkipNestedSections` parses only anchor sections and skips nested (`/`-addressed) section bodies —
  cheap header enumeration on files with large nested payloads (map files).
- Nested sections: a section name containing `/` is nested. `ConfigFile` recognizes only the
  syntax - names are stored **verbatim** and no prefix is ever resolved, so what a prefix means
  belongs to the consuming format. `GetOrderedSections()` exposes sections in file order, which is
  what a consumer needs to bind a nested section to the section it follows (the by-name multimap
  cannot express that, since repeated names collapse). `SkipNestedSections` parses only non-nested
  sections and skips nested bodies.
- `ConfigFile` takes only the content: no file identity, no parse callbacks, no format tokens. For
  map files, `MapLoader` owns the interpretation - `[ProtoMap]` declares a map named by its `$Name`
  or by the file, and a nested `$Name/<Type>` prefix binds content to the anchor above it.

The parser stores owned strings internally and returns `string_view` values from parsed sections. Consumers must not assume those views outlive the `ConfigFile` instance.

## Runtime settings

`Source/Common/Settings.inc` is the central generated-like declaration file for setting groups and individual settings. `Settings.h` exposes:

- `ResourcePackInfo` — name, input directories/files, include/exclude glob patterns, side flags, and baker list.
- `SubConfigInfo` — named config overlays and setting maps.
- `GlobalSettings` — combined client/server/baking/base settings with apply/save/custom-setting operations.

`GlobalSettings` applies input through:

- `ApplyConfigAtPath()` and `ApplyConfigFile()` for config files;
- `ApplyCommandLine()` for runtime/build-tool overrides;
- `ApplyInternalConfig()` for generated internal config;
- `ApplySubConfigSection()` for named overlays;
- `ApplyDefaultSettings()` and `ApplyAutoSettings()` for engine defaults/derived values.

`ConfigBaker` (`Source/Tools/ConfigBaker.cpp`) bakes the config by re-deriving each sub-config from the root and saving every registered setting; a setting that `GlobalSettings::Save()` does not emit is reported as `Uninitialized server/client setting <name>` and fails the bake. `Save()` only emits settings present in `_appliedSettings`, which is populated from the keys of every applied config plus a fixed **auto-settings** allow-list seeded in the baking-mode `GlobalSettings` constructor. Author-tunable settings reach `_appliedSettings` by being enumerated in the embedding project's config; settings that are resolved at runtime and never authored in any config (platform/build flags, monitor size, command-line/git/compatibility values, and `Client.UserWritablePath`) must be added to that auto-settings allow-list, or baking fails. When adding such a runtime-only engine setting to `Settings.inc`, register it in the auto-settings list in the same change. Settings consumed only by `BuildTools/package.py` (the `AndroidSettings` and `PackagingSettings` groups in `Settings.inc`, e.g. `Android.Keystore`, `Packaging.AppIcon`, `Packaging.MsiUpgradeCode`, `Packaging.CodeSigningHook`) are registered as ordinary settings like everything else — the config baker validates them uniformly and has no packaging-specific allow-list, so an unregistered key in a config is always reported as `Unknown setting`. Note that setting lookup accepts both the dotted (`Group.Name`) and bare (`Name`) spellings, so the bare part of every setting name must stay globally unique across all groups (e.g. `Android.Icon` already claims `Icon`, which is why the packaging icon lives under its own `Packaging.AppIcon` name).

Custom settings have two read shapes. Use `FindCustomSetting()` when missing keys are normal and should stay in the nullable pointer vocabulary. Use `GetCustomSetting()` only for compatibility with the historical non-null sentinel behavior: it returns the stored value when present and `_emptySetting` when absent.

Do not document one embedding project's `.fomain` contents as universal engine behavior. Use project docs for concrete values; use this page for the engine mechanics that consume them.

Client startup has one extra resolution step for installed layouts: `ResolveUserWritablePath(settings)` in `Source/Frontend/ApplicationInit.cpp` resolves `Client.UserWritablePath` before the local-config cache is read. The writable-path knobs (`Client.UserWritablePath`, `Baking.CacheResources`) live in the config and sub-config, which are applied earlier, so the cache location is known without consulting the command line. The command line is then applied to the live settings exactly **once**, after the config, sub-config and local config, so it takes final precedence over all of them; a single pass also keeps `+`-append overrides (`-Setting +value`) from accumulating twice. That single pass logs each `Set <name> to <value>` override. In that log, settings whose name contains one of the masking tokens are printed as `Set <name> to ***`, so a credential such as `Auth.WebTokenVerifySecret` never appears in plaintext (server logs may be shared). The tokens are the `Common.SecretSettingTokens` setting (a case-insensitive substring list, default `secret token password apikey`), which `GlobalSettings::IsSecretSettingName()` reads. Command-line overrides are logged only on the final pass — after `ApplyDefaultSettings()` and the config file have run — so the list is already populated, and an embedding project extends it through config to cover credentials the generic tokens miss (Last Frontier sets `Common.SecretSettingTokens = secret token password apikey dsn` so `Sentry.Dsn` is masked). Empty means portable unless an `INSTALLED` marker sits next to the executable; `*` resolves through `Platform::GetUserDataBase()` plus `Common.GameName`; an explicit path is resolved directly. If the target directory or required cache/resource subdirs cannot be created, the resolver logs a warning and reverts to portable layout.

## Resource packs and data sources

`ResourcePackInfo` describes resource-pack inputs that bakers and runtimes consume. The bake side uses `BakingContext` / `BakerDataSource` in `Source/Tools/Baker.*`; the runtime side uses mounted `DataSource` and `FileSystem` abstractions.

Resource-pack input directories are mounted recursively. `IncludePatterns` and `ExcludePatterns` are optional space-separated glob lists applied to normalized resource-relative paths before any baker runs. An empty include list accepts every path; exclusion is evaluated after inclusion and wins. Patterns are case-sensitive and support:

- `*` — zero or more characters other than `/`;
- `?` — exactly one character other than `/`;
- `**` — zero or more characters including `/`; `**/` also matches zero directory levels.

Both `/` and `\` are accepted as pattern separators and normalized to `/`. For example, `IncludePatterns = **/*.fomap` selects maps at any depth, while `ExcludePatterns = **/_*.fomap` removes scratch maps such as `Generated/_compose.fomap`. Multiple packs may mount the same `InputDirs` and select disjoint resources with different pattern lists. Use `IncludePatterns = *` to reproduce the former top-level-only input behavior.

`DataSource` provides two built-in mount shapes:

- `MountDir(dir, recursive, non_cached, maybe_not_available)` for disk directory resources;
- `MountPack(dir, name, maybe_not_available)` for packed resource data.

`FileSystem` then combines sources and offers:

- `AddDirSource()`, `AddPackSource()`, `AddPacksSource()`, and `AddCustomSource()`;
- `FilterFiles()`, `GetAllFiles()`, and existence checks;
- `ReadFile()`, `ReadFileText()`, and `ReadFileHeader()`;
- `FileReader` helpers for endian-aware binary reads.

Mount order matters for lookup behavior. When changing it, verify the runtime/tool path that owns the resource pack, not only the parser.

Installed clients keep the read-only base resources mounted from `ClientResources` and layer the writable resource overlay from `fs_make_writable_path(UserWritablePath, ClientResources)` on top in client/updater paths. The updater writes resource patches into that overlay, so current files win lookup/hash checks without modifying the install directory. Native runtime binary update paths are owned by [ClientUpdater.md](ClientUpdater.md).

## Cache storage

`Source/Common/CacheStorage.*` stores named binary/string cache entries behind `HasEntry()`, `GetString()`, `GetData()`, `SetString()`, `SetData()`, and `RemoveEntry()`. It is separate from resource packs: cache entries are mutable runtime/tool artifacts, while baked resources are generated from configured inputs. Client-side cache consumers resolve relative cache paths through `fs_make_writable_path(UserWritablePath, CacheResources)`, so portable clients keep cache next to the executable and installed clients write under the per-user root.

## Build and package routing

- `BuildTools/cmake/stages/Codegen.cmake` generates internal config inputs used by runtime settings.
- `BuildTools/cmake/stages/ScriptsAndBaking.cmake` wires resource baking/script compilation that consume `ResourcePackInfo` and baking settings.
- `BuildTools/cmake/stages/Packages.cmake` packages resources for runtime targets.
- `Source/Tools/ConfigBaker.*` bakes config resources; full bake orchestration is in [BakingPipeline.md](BakingPipeline.md).

## Tests to inspect

Focused tests for this area:

- `Source/Tests/Test_CacheStorage.cpp`
- `Source/Tests/Test_ConfigFile.cpp`
- `Source/Tests/Test_DataSource.cpp`
- `Source/Tests/Test_DiskFileSystem.cpp`
- `Source/Tests/Test_FileSystem.cpp`
- `Source/Tests/Test_Settings.cpp`
- `Source/Tests/Test_ConfigBaker.cpp`

Related consumers are covered by resource, client, server, script, and baker tests listed in [Testing.md](Testing.md).

## Change routing

- Config grammar and parsed section/key behavior: `Source/Common/ConfigFile.*`.
- Setting groups, defaults, command-line/config/sub-config application: `Source/Common/Settings.*` and `Settings.inc`.
- Installed-client writable-root resolution: `Source/Frontend/ApplicationInit.cpp`, `Source/Essentials/Platform.*`, and `Source/Essentials/DiskFileSystem.*`.
- Mounted resource lookup: `Source/Common/DataSource.*` and `FileSystem.*`.
- Raw disk operations: `Source/Essentials/DiskFileSystem.*`.
- Runtime resource consumption: `Source/Client/ResourceManager.*` plus owning runtime docs.
- Resource-pack generation: [BakingPipeline.md](BakingPipeline.md) and `Source/Tools/*Baker.*`.

## Validation checklist

1. Run the focused parser/settings/filesystem/cache tests for the changed area.
2. If resource-pack shape or mount order changes, run at least one bake path and one runtime/tool consumer path.
3. If command-line or sub-config behavior changes, verify the embedding project config that exercises it, but keep project-specific values in project docs.
4. If packaging/resource staging changes, re-check [WebDebugging.md](WebDebugging.md), [AndroidDebugging.md](AndroidDebugging.md), and [ClientUpdater.md](ClientUpdater.md) as applicable.
5. Update [BakingPipeline.md](BakingPipeline.md) or [BuildToolsPipeline.md](BuildToolsPipeline.md) when build-stage ownership changes.
