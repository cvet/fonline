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
- `Source/Common/Settings-Include.h`
- `Source/Common/DataSource.h`
- `Source/Common/DataSource.cpp`
- `Source/Common/FileSystem.h`
- `Source/Common/FileSystem.cpp`
- `Source/Common/CacheStorage.h`
- `Source/Common/CacheStorage.cpp`
- `Source/Essentials/DiskFileSystem.h`
- `Source/Essentials/DiskFileSystem.cpp`
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
2. **Settings model** — `Settings-Include.h` declares setting groups; `Settings.*` turns config files, command-line overrides, internal config, defaults, auto-settings, sub-configs, and resource-pack declarations into `GlobalSettings`.
3. **Data-source abstraction** — `DataSource` mounts disk directories and pack files behind a uniform file-list/open interface.
4. **File-system view** — `FileSystem` combines mounted data sources, exposes `FileHeader`, `File`, `FileReader`, and `FileCollection`, and resolves file reads by path/name.
5. **Cache storage** — `CacheStorage` persists named string/data entries for reusable cache consumers.
6. **Low-level disk access** — `DiskFileSystem` performs direct disk operations below mounted engine resources.

## Config parsing

`Source/Common/ConfigFile.*` owns syntax-level parsing. `ConfigFileOption` controls optional behavior:

- `CollectContent` preserves section content for consumers that need raw block text.
- `ReadFirstSection` supports workflows that only need the first section.

The parser stores owned strings internally and returns `string_view` values from parsed sections. Consumers must not assume those views outlive the `ConfigFile` instance.

## Runtime settings

`Source/Common/Settings-Include.h` is the central generated-like declaration file for setting groups and individual settings. `Settings.h` exposes:

- `ResourcePackInfo` — name, input directories/files, recursive flag, side flags, and baker list.
- `SubConfigInfo` — named config overlays and setting maps.
- `GlobalSettings` — combined client/server/baking/base settings with apply/save/custom-setting operations.

`GlobalSettings` applies input through:

- `ApplyConfigAtPath()` and `ApplyConfigFile()` for config files;
- `ApplyCommandLine()` for runtime/build-tool overrides;
- `ApplyInternalConfig()` for generated internal config;
- `ApplySubConfigSection()` for named overlays;
- `ApplyDefaultSettings()` and `ApplyAutoSettings()` for engine defaults/derived values.

Do not document one embedding project's `.fomain` contents as universal engine behavior. Use project docs for concrete values; use this page for the engine mechanics that consume them.

## Resource packs and data sources

`ResourcePackInfo` describes resource-pack inputs that bakers and runtimes consume. The bake side uses `BakingContext` / `BakerDataSource` in `Source/Tools/Baker.*`; the runtime side uses mounted `DataSource` and `FileSystem` abstractions.

`DataSource` provides two built-in mount shapes:

- `MountDir(dir, recursive, non_cached, maybe_not_available)` for disk directory resources;
- `MountPack(dir, name, maybe_not_available)` for packed resource data.

`FileSystem` then combines sources and offers:

- `AddDirSource()`, `AddPackSource()`, `AddPacksSource()`, and `AddCustomSource()`;
- `FilterFiles()`, `GetAllFiles()`, and existence checks;
- `ReadFile()`, `ReadFileText()`, and `ReadFileHeader()`;
- `FileReader` helpers for endian-aware binary reads.

Mount order matters for lookup behavior. When changing it, verify the runtime/tool path that owns the resource pack, not only the parser.

## Cache storage

`Source/Common/CacheStorage.*` stores named binary/string cache entries behind `HasEntry()`, `GetString()`, `GetData()`, `SetString()`, `SetData()`, and `RemoveEntry()`. It is separate from resource packs: cache entries are mutable runtime/tool artifacts, while baked resources are generated from configured inputs.

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
- Setting groups, defaults, command-line/config/sub-config application: `Source/Common/Settings.*` and `Settings-Include.h`.
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
