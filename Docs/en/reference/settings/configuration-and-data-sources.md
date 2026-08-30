---
layout: default
title: Configuration and Data Sources
locale: en
document_id: configuration-data-sources
permalink: /Docs/en/reference/settings/configuration-and-data-sources.html
---

# Configuration and Data Sources

> Engine-owned documentation. This page explains reusable configuration parsing, runtime settings, mounted data sources, file lookup, and cache storage. Project-specific config values and content folder policy belong to the embedding project.

## Purpose

Use this page when changing how the engine reads `.fomain`/config data, applies command-line or sub-config overrides, mounts resource directories/packs, reads files, or stores cached resource data.

For the executable project-authoring route, use [Configure a Game Project](../../how-to/build/project-configuration.md). This page remains the reusable implementation/reference owner.

Read this together with:

- [Build Workflow](../../how-to/build/) for configure/build entry points.
- [Baking Pipeline](../../explanation/content-pipeline/baking.md) for resource-pack production.
- [Text and Localization](../../how-to/content/text-and-localization.md) for `Baking.BakeLanguages`, `Client.Language`, text-pack filename selection, and bake-time normalization.
- [Font Formats And Text Layout](../../how-to/content/font-format.md) for font descriptor raw-copy selection, referenced image lookup, client binding, and project-owned slot policy.
- [Generated API and Metadata](../metadata/index.md) for generated settings and metadata inputs.
- [Client Runtime](../../explanation/runtime/client.md), [Server Runtime](../../explanation/runtime/server.md), and [Tools](../tools/) for runtime/tool consumers.

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
- `Source/Common/SettingsStorage.h`
- `Source/Common/SettingsStorage.cpp`
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
6. **Settings store** — `SettingsStorage` persists per-user tool/editor preferences (registry on Windows, file store elsewhere), scoped by application name.
7. **Low-level disk access** — `DiskFileSystem` performs direct disk operations below mounted engine resources.

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

Ordinary application startup creates non-baking `GlobalSettings` and applies
Engine defaults before reading project input. The effective runtime order is:
defaults, project config (or packaged internal config), selected sub-configs,
the writable local-config cache, command-line overrides, then derived auto
settings. A project `.fomain` therefore records deliberate authored choices;
an omitted setting receives its declared Engine default rather than a
zero-initialized value. `Source/Tests/Test_Settings.cpp` protects both the
default baseline and the fact that a project override still wins.

`ConfigBaker` (`Source/Tools/ConfigBaker.cpp`) re-derives every sub-config from
the root. Metadata stores each game setting's configured root value and is the
runtime baseline: `BaseEngine` applies it only when config, sub-config, local
config, or command line did not set that name. The side-specific internal config
therefore carries only game-setting sub-config deltas; false and empty deltas are
written too, because they must override a non-empty metadata baseline. Built-in
server/client settings retain the existing non-empty compact-config rules.
`MetadataBaker` includes applied config timestamps in freshness and rejects a
game-setting declaration that has no configured value.

That metadata baseline is not applied until `BaseEngine` exists. A game setting
read by `ApplicationInitHook` or any earlier startup path must therefore travel
in the binary config itself. `Baking.BootstrapGameSettings` lists exactly those
exceptions: `ConfigBaker` writes each listed setting in full for every baked
sub-config instead of reducing it to a delta. Every name must resolve to a
declared game setting or baking fails. Keep this list narrow so the fixed
10000-byte internal-config patch remains bootstrap data rather than a second
copy of the metadata baseline.

`GlobalSettings::Save()` still emits only settings present in `_appliedSettings`,
which is populated from applied config keys plus the baking-mode
**auto-settings** allow-list. Runtime-only settings (platform/build flags,
monitor size, command-line/git/compatibility values, and
`Client.UserWritablePath`) must remain in that allow-list. Settings consumed only
by `BuildTools/package.py` are validated as ordinary settings. Setting lookup
accepts dotted (`Group.Name`) and bare (`Name`) spellings, so every bare name must
stay globally unique.

Custom settings have two read shapes. Use `FindCustomSetting()` when missing keys are normal and should stay in the nullable pointer vocabulary. Use `GetCustomSetting()` only for compatibility with the historical non-null sentinel behavior: it returns the stored value when present and `_emptySetting` when absent.

Do not document one embedding project's `.fomain` contents as universal engine behavior. Use project docs for concrete values; use this page for the engine mechanics that consume them.

`Baking.BakeLanguages` is an ordered content contract, not an unordered locale
allowlist: text baking uses the first value as the normalization base.
`Client.Language` selects the initial client text pack. Exact `.fotxt`, `$Text`,
fallback, and runtime lookup behavior belongs to
[Text and Localization](../../how-to/content/text-and-localization.md).

Client startup has one extra resolution step for installed layouts: `ResolveUserWritablePath(settings)` in `Source/Frontend/ApplicationInit.cpp` resolves `Client.UserWritablePath` before the local-config cache is read. The writable-path knobs (`Client.UserWritablePath`, `Baking.CacheResources`) live in the config and sub-config, which are applied earlier, so the cache location is known without consulting the command line. The command line is then applied to the live settings exactly **once**, after the config, sub-config and local config, so it takes final precedence over all of them; a single pass also keeps `+`-append overrides (`-Setting +value`) from accumulating twice. That single pass logs each `Set <name> to <value>` override. In that one log path, settings whose name contains one of the masking tokens are printed as `Set <name> to ***`. The tokens are the `Common.SecretSettingTokens` setting (a case-insensitive substring list, default `secret token password apikey`), which `GlobalSettings::IsSecretSettingName()` reads. Command-line overrides are logged only on the final pass - after `ApplyDefaultSettings()` and the config file have run - so the list is already populated, and an embedding project can extend it for names the generic tokens miss. This is not general credential protection: the raw process arguments and setting value remain available, and other logs, settings UI, crash output, baked configs, and project code have independent exposure paths. Never pass credentials on the command line; use target-time provisioning and follow [Security and Secrets](../../how-to/release/security-and-secrets.md). Empty `Client.UserWritablePath` means portable unless an `INSTALLED` marker sits next to the executable; `*` resolves through `Platform::GetUserDataBase()` plus `Common.GameName`; an explicit path is resolved directly. If the target directory or required cache/resource subdirs cannot be created, the resolver logs a warning and reverts to portable layout.

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

Cached directory mounts snapshot their file index when mounted. Long-running tools can call `FileSystem::ReindexDataSources()` to ask every mounted source to refresh that snapshot; the method returns `true` when indexed paths, sizes, or write times changed. Sources that do not cache disk state keep the default no-op behavior. Custom sources can override `DataSource::Reindex()`; `BakerDataSource` uses it to rebuild input mounts and bake newly added or changed resources on demand.

Mount order matters for lookup behavior. When changing it, verify the runtime/tool path that owns the resource pack, not only the parser.

Installed clients keep the read-only base resources mounted from `ClientResources` and layer the writable resource overlay from `fs_make_writable_path(UserWritablePath, ClientResources)` on top in client/updater paths. The updater writes resource patches into that overlay, so current files win lookup/hash checks without modifying the install directory. Native runtime binary update paths are owned by [Client Runtime Split and Updater](../../explanation/runtime/client-updater.md).

## Low-level disk access

`Source/Essentials/DiskFileSystem.*` performs direct disk operations below mounted engine resources. `fs_write_file()` writes content at the requested path but, on a case-insensitive filesystem, an existing entry that differs only by letter case is reused with its old spelling. Callers that rewrite an exact-name tree they own must reconcile such entries explicitly. The baker does this once per pass; see [Output names are reconciled with the names bakers addressed](../../explanation/content-pipeline/baking.md#output-names-are-reconciled-with-the-names-bakers-addressed).

## Cache storage

`Source/Common/CacheStorage.*` stores named binary/string cache entries behind `HasEntry()`, `GetString()`, `GetData()`, `SetString()`, `SetData()`, and `RemoveEntry()`. Bounded consumers use `GetDataBounded(name, max_size)`, which checks the file size before allocating and distinguishes `Success`, `Missing`, `TooLarge`, and `Failed`, plus `SetDataChecked(...)`, which reports whether the complete write succeeded. The underlying disk helper `fs_read_file_bounded()` applies the same pre-allocation cap and returns no data for an oversized file instead of reading it. It is separate from resource packs: cache entries are mutable runtime/tool artifacts, while baked resources are generated from configured inputs. Client-side cache consumers resolve relative cache paths through `fs_make_writable_path(UserWritablePath, CacheResources)`, so portable clients keep cache next to the executable and installed clients write under the per-user root.

An entry is stored as one plain file named after the entry, with path separators folded to `_`, so the cache directory stays readable and inspectable. Two entry names that differ only in those separators therefore map to the same file — acceptable because an entry is only ever a cache, where a miss is always recoverable, but it means a caller that needs distinct entries must not rely on directory structure alone to separate them. The cache is not a confidentiality boundary: anything that must not be readable at rest has to be protected by its owner before it is handed over (the embedding project's secure-storage bridge does exactly that).

## Settings store

`Source/Common/SettingsStorage.*` persists small per-user tool/editor preferences (ImGui window layout, view options, last selection) behind `GetString()`/`SetString()`, typed `GetInt`/`SetInt`, `GetBool`/`SetBool`, `GetFloat`/`SetFloat`, `HasKey()`, and `Remove()`. It is scoped by an application name passed to the constructor so different tools never collide. The backend is platform-specific through a pimpl: on `FO_WINDOWS` the values are `REG_SZ` entries under `HKCU\Software\FOnline\<app_name>` (Win32 headers are confined to the `.cpp` behind `WIN32_LEAN_AND_MEAN` + `WinApiUndef.inc`, using the explicit `*A` registry entry points); on other platforms it is a per-application `CacheStorage` under `Platform::GetUserDataBase()/FOnline/<app_name>`. Every value is stored as a string (the typed accessors serialize through it), so both backends behave identically, and the multi-line ImGui `imgui.ini` blob round-trips verbatim. Persistence is **best-effort**: a backend failure is logged, never thrown, so a tool never dies because its settings could not be written. It differs from `CacheStorage` in intent (durable user preferences vs. regenerable cache artifacts) and, on Windows, in medium (registry vs. files).

Only the GUI tools reference it (Mapper `MapperEngine::_uiSettings`, migrated from the resource `Cache`; standalone AnimationViewer / ParticleViewer, each loading in its constructor and saving on shutdown). It lives in `CommonLib` for simplicity, but because the client and server reference no `SettingsStorage` symbol, the linker (`/OPT:REF` plus on-demand static-library inclusion) drops the object from the shipped client/server binaries — so the Windows registry calls never land where antivirus heuristics might flag them. ImGui's own `imgui.ini` autosave stays disabled (`Application.cpp`), so all layout persistence flows through this store.

## Build and package routing

- `BuildTools/cmake/stages/Codegen.cmake` generates internal config inputs used by runtime settings.
- `BuildTools/cmake/stages/ScriptsAndBaking.cmake` wires resource baking/script compilation that consume `ResourcePackInfo` and baking settings.
- `BuildTools/cmake/stages/Packages.cmake` packages resources for runtime targets.
- `Source/Tools/ConfigBaker.*` bakes config resources; full bake orchestration is in [Baking Pipeline](../../explanation/content-pipeline/baking.md).

## Tests to inspect

Focused tests for this area:

- `Source/Tests/Test_CacheStorage.cpp`
- `Source/Tests/Test_SettingsStorage.cpp`
- `Source/Tests/Test_ConfigFile.cpp`
- `Source/Tests/Test_DataSource.cpp`
- `Source/Tests/Test_DiskFileSystem.cpp`
- `Source/Tests/Test_FileSystem.cpp`
- `Source/Tests/Test_Settings.cpp`
- `Source/Tests/Test_ConfigBaker.cpp`

Related consumers are covered by resource, client, server, script, and baker tests listed in [Testing](../../contributing/testing/).

## Change routing

- Config grammar and parsed section/key behavior: `Source/Common/ConfigFile.*`.
- Setting groups, defaults, command-line/config/sub-config application: `Source/Common/Settings.*` and `Settings.inc`.
- Installed-client writable-root resolution: `Source/Frontend/ApplicationInit.cpp`, `Source/Essentials/Platform.*`, and `Source/Essentials/DiskFileSystem.*`.
- Mounted resource lookup: `Source/Common/DataSource.*` and `FileSystem.*`.
- Raw disk operations: `Source/Essentials/DiskFileSystem.*`.
- Runtime resource consumption: `Source/Client/ResourceManager.*` plus owning runtime docs.
- Particle source selection, `.spark`/`.efkproj` compilation, and `.spk`/`.efk` runtime consumption: `Source/Tools/ParticleBaker.*`, `Source/Client/ParticleRuntime.*`, `Source/Client/VisualParticles.*`, and [Particle Format And Runtime](../../how-to/content/particle-format.md).
- Font descriptor raw-copy selection and runtime consumption: `Baking.RawCopyFileExtensions`, `Source/Tools/RawCopyBaker.*`, `Source/Client/FontManager.*`, and [Font Formats And Text Layout](../../how-to/content/font-format.md).
- Audio raw-copy selection, sound indexing, decoder dispatch, and client playback: `Baking.RawCopyFileExtensions`, `Audio.*`, `Source/Tools/RawCopyBaker.*`, `Source/Client/ResourceManager.cpp`, `Source/Client/SoundManager.*`, and [Audio](../../how-to/content/audio.md).
- Video raw-copy selection, exact-path loading, Ogg/Theora decode, fullscreen/embedded client playback, and memory ownership: `Baking.RawCopyFileExtensions`, `Source/Tools/RawCopyBaker.*`, `Source/Client/VideoClip.*`, `Source/Client/Client.*`, and [Video](../../how-to/content/video.md).
- Resource-pack generation: [Baking Pipeline](../../explanation/content-pipeline/baking.md) and `Source/Tools/*Baker.*`.

## Validation checklist

1. Run the focused parser/settings/filesystem/cache tests for the changed area.
2. If resource-pack shape or mount order changes, run at least one bake path and one runtime/tool consumer path.
3. If command-line or sub-config behavior changes, verify the embedding project config that exercises it, but keep project-specific values in project docs.
4. If packaging/resource staging changes, re-check [Web Build, Packaging, and Browser Debugging](../../how-to/platforms/web-debugging.md), [Android Build, Packaging, and Device Debugging](../../how-to/platforms/android-debugging.md), and [Client Runtime Split and Updater](../../explanation/runtime/client-updater.md) as applicable.
5. Update [Baking Pipeline](../../explanation/content-pipeline/baking.md) or [BuildTools Pipeline](../cmake-and-buildtools/pipeline.md) when build-stage ownership changes.
