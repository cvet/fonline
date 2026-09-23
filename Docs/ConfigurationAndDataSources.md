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
- Repeated sections: `GetSections(name)` returns every section of that name in file order (the
  multimap's equal range), and the single-section reads (`GetSection`, `GetAsStr`, `GetAsInt`,
  `HasKey`, `GetSectionKeyValues`, `GetSectionContent`) read the first declared one. Neither is
  built on `multimap::find`: the standard lets it return any element of a repeated key, and libc++
  returns whichever node its descent meets first, so counting on from it walks past the last section.
  MSVC STL and libstdc++ happen to return the first, which is why only a libc++ target (Web, Android,
  Apple) would show it.
- `ConfigFile` takes only the content: no file identity, no parse callbacks, no format tokens. For
  map files, `MapLoader` owns the interpretation - `[ProtoMap]` declares a map named by its `$Name`
  or by the file, and a nested `$Name/<Type>` prefix binds content to the anchor above it.

The parser stores owned strings internally and returns `string_view` values from parsed sections. Consumers must not assume those views outlive the `ConfigFile` instance.

## Runtime settings

For an unpackaged executable without `ApplyConfig`, `LoadAppSettings` searches the current directory
and its ancestors for `FO_MAIN_CONFIG`. The filesystem root is the final candidate; if no config is
found there, startup reports `Config file not found` instead of revisiting the root indefinitely.

`Source/Common/Settings.inc` is the central generated-like declaration file for setting groups and individual settings. A group is written as `SETTING_GROUP(<Group>, <virtual bases>)` ... `SETTING_GROUP_END(<Group>)`, which builds `struct <Group>Settings` and puts that group's settings into a nested aggregate member named after the group. Groups compose by virtual inheritance, so a narrow parameter type (`RenderSettings&`, `BakingSettings&`) reaches exactly the groups its bases declare and `GlobalSettings` reaches all of them — always through the group: `settings.Network.ServerPort`, `settings.Render.FixedFPS`. `Settings.h` exposes:

- `ResourcePackInfo` — name, input directories/files, include/exclude glob patterns, side flags, and baker list.
- `SubConfigInfo` — named config overlays and setting maps.
- `GlobalSettings` — combined client/server/baking/base settings with apply/save/custom-setting operations.

`GlobalSettings` applies input through:

- `ApplyConfigAtPath()` and `ApplyConfigFile()` for config files;
- `ApplyCommandLine()` for runtime/build-tool overrides;
- `ApplyInternalConfig()` for generated internal config;
- `ApplySubConfigSection()` for named overlays;
- `ApplyDefaultSettings()` and `ApplyAutoSettings()` for engine defaults/derived values.

`ConfigBaker` (`Source/Tools/ConfigBaker.cpp`) bakes the config by re-deriving each sub-config from the root and saving every registered setting; a setting that `GlobalSettings::Save()` does not emit is reported as `Uninitialized server/client setting <name>` and fails the bake. `Save()` only emits settings present in `_appliedSettings`, which is populated from the keys of every applied config plus a fixed **auto-settings** allow-list seeded in the baking-mode `GlobalSettings` constructor. Author-tunable settings reach `_appliedSettings` by being enumerated in the embedding project's config; settings that are resolved at runtime and never authored in any config (platform/build flags, monitor size, command-line/git/compatibility values, and `Common.UserWritablePath`, which is resolved before any config is read) must be added to that auto-settings allow-list, or baking fails. When adding such a runtime-only engine setting to `Settings.inc`, register it in the auto-settings list in the same change. Settings consumed only by `BuildTools/package.py` (the `AndroidSettings` and `PackagingSettings` groups in `Settings.inc`, e.g. `Android.Keystore`, `Packaging.AppIcon`, `Packaging.MsiUpgradeCode`, `Packaging.CodeSigningHook`) are registered as ordinary settings like everything else — the config baker validates them uniformly and has no packaging-specific allow-list, so an unregistered key in a config is always reported as `Unknown setting`. A setting is named by its group wherever it is addressed: `Group.Name` in a config file, on the command line and in `GetRuntimeSetting()` / `SetRuntimeSetting()`, and `Settings.Group.Name` in engine C++ and in managed scripts. A bare `Name` names no engine setting at all — it falls through to the custom-setting map, which the config baker then reports as `Unknown setting` — so two groups may declare the same short name without either one answering for the other.

A setting is a knob: something a designer, operator or maintainer turns, which the engine then reads — and then only reads. `Settings.inc` declares every entry with one macro, `SETTING`, which makes the member `const`; there is no second, writable kind, and `SetRuntimeSetting()` answers a write to any engine setting with `SettingsException: Setting is read-only`. A value that one system writes and another reads in the same frame is therefore not expressible as a setting at all — it would be a global with no owner, and a config file could "set" it to a value the next frame overwrites. One tell that a value was never a knob is an entry no config authors, which therefore has to be listed in the baking-mode auto-settings allow-list: that list is the engine stating no config will ever write the value. Such a value belongs to the system that owns it, reached through that system's API — the way manual map scrolling moved from eight `Hex.Scroll*` flags to `MapView::SetManualScroll` (see [ClientRuntime.md](ClientRuntime.md)), and the measured round trip moved from `ClientNetwork.Ping` to `ClientConnection::GetPing()`, exported beside the other connection counters as `Game.GetPing()`. `ClientNetwork.PingPeriod` stayed a setting, because how often to ping is a knob while the answer is not. The same move retired the seven `Hex.Show*` layer flags, which only the mapper ever read, into `MapView::SetVisibleLayers(MapLayers)`; the live window geometry (`View.ScreenWidth` / `ScreenHeight`, `Render.Fullscreen`), which the platform decides and every resize changes, into `Application::ScreenState`, read back through `AppWindow::GetScreenSize()` / `IsFullscreen()`; the mixer volumes into `AudioManager::SetMusicVolume()` / `SetSoundVolume()`; the wireframe overlay into `SpriteManager::SetDrawWireframe()`; and the language in effect into `BaseEngine::GetCurLangName()`. In each case the configured entry keeps its one honest meaning — what to start with — and the system that owns the live value answers for it afterwards. The same reading retired `Baking.ServerResourceEntries` / `ClientResourceEntries` / `MapperResourceEntries`, which `AddResourcePacks` filled from the `[ResourcePack]` sections it had just parsed: the side flags on each pack already say which side loads it, so the three lists are derived on demand by `GetServerResourcePacks()` / `GetClientResourcePacks()` / `GetMapperResourcePacks()` instead of being mirrored into settings nobody authors.

A setting value is text read as written, from a config line or a command-line argument alike (`SetEntry()` in `Settings.cpp`). A `string` setting takes that text verbatim: a backslash is an ordinary character, so a Windows path such as `C:\work\none\Baking` or `\\server\share` arrives intact, and quotes are part of the value. A `vector<string>` setting splits the text at spaces and tabs and takes each element as written — the form `GlobalSettings::Save()` joins the list back into, so the `key=value` lines the config baker writes read back to the same value in a packaged application; an element therefore cannot hold whitespace itself. Only numeric, boolean and enum settings parse their text. The coded-string grammar of `AnyData` (`\n`, `\"`, `\\`, quoted tokens) belongs to property values, not to settings: applied to a setting it would turn the `\n` of `\none` into a line break, silently, since the `Set <name> to <value>` log line prints the text before it is parsed.

A setting value may pull text from outside the config (`GlobalSettings::SetValue()`): `$ENV{NAME}` reads an environment variable and `$FILE{path}` a file, trimmed, with a relative path taken from the config's own directory. `$TARGET_ENV{NAME}` and `$TARGET_FILE{path}` read the same way but are left as written while baking, so they resolve on the host that runs the package and never enter a baked config. That is how a credential such as the secure channel's `ServerNetwork.ChannelSecretKey` ([Networking.md](Networking.md#keys)) stays out of the repository and out of every package. A variable or file that is missing is logged as a warning and the reference text is kept as the value, so a setting that must hold a well-formed value fails where it is consumed rather than silently turning empty.

Custom settings have two read shapes. Use `FindCustomSetting()` when missing keys are normal and should stay in the nullable pointer vocabulary. Use `GetCustomSetting()` only for compatibility with the historical non-null sentinel behavior: it returns the stored value when present and `_emptySetting` when absent.

For `///@ Setting` declarations, `MetadataBaker` requires and writes the configured textual value after the setting name and type for both generated engine settings and custom game settings; a declaration without a value in the applied configuration is a bake error. The declaration names the setting in full, `Group.Name` (`Common . DebugBuild` spelled across tokens is the same name); one that names no group is a bake error too, since resolving a bare name into whichever group happens to own it is exactly what made a short name global. Runtime metadata registration requires the same three fields. Those values are the **baseline** for game settings: `BaseEngine` applies a registered metadata value only to a setting the applied configuration never mentioned (`BaseSettings::FindSettingValue()` returns nothing), routing a known setting through its generated typed field and an unknown one to custom storage. A setting the config, sub-config, local config or command line did set keeps that value — those layers are the operator's explicit override and run before the engine exists, so the baseline must not win over them.

`ConfigBaker` keeps the `.fomain-*` data patched into packaged binaries small by writing a game-only setting only when the package sub-config value differs from the active configuration used to bake the metadata resource; a setting that is also a native engine setting is always written, because startup may consume it before metadata is available. Comparing against the active bake matters when one public bake feeds several packages: a staging child that restores a production parent setting to the authored root value must still carry that reset. A game-only delta is written verbatim, bypassing the empty/`false` skip that applies to the rest — an override that turns something off still has to beat a baseline that says otherwise. This keeps the internal config as bootstrap plus per-package deltas while the metadata resource owns the script-setting baseline. The internal-config patch area is fixed by the engine at 10000 bytes and is not project-configurable. A packaged application reads nothing but that config, so it is also where the pack list travels: every baked config closes with the `[ResourcePack]` sections that `BaseSettings::GetResourcePackDeclarations()` writes, each with the pack name and its `ServerOnly` / `ClientOnly` / `MapperOnly` side and nothing about its inputs, which is what `GetServerResourcePacks()`, `GetClientResourcePacks()` and `GetMapperResourcePacks()` answer from. The sections come last because every line after a section header belongs to that section; for the same reason `package.py` puts variant config such as `Render.ForceOpenGL=1` in front of the baked text rather than after it. Without the sections a packaged server fails at startup with `Baked metadata file is not present in the resources`, since it mounts no pack at all. The applied root config write time is a metadata-bake dependency, so changing a configured setting refreshes the metadata resource even when no script declaration changed.

That baseline arrives with `BaseEngine`, so it is not yet applied while `InitApp` runs — settings read from `ApplicationInitHook` or any earlier point see only what the binary config carries. `Baking.BootstrapGameSettings` names the game settings an embedding project consumes there: `ConfigBaker` writes each of them in full, exactly as it writes a native engine setting, instead of reducing it to a sub-config delta. Every listed name must be a declared game setting or the bake fails, so a typo cannot quietly restore the delta form. Keep the list to settings that are genuinely read before the engine exists; everything else belongs in the metadata baseline, which is what keeps the patched config inside its fixed 10000-byte area.

Managed runtime script reads use `GlobalSettings::GetRuntimeSetting()`, and the generated `Settings.Group.Name` surface is **get-only**: `ManagedScriptBaker` emits no setter for a setting, so a script cannot write one by accident. `SetRuntimeSetting()` still exists for the paths that legitimately set a configured value before or outside play — the config layers, `SetSettingValue()`, and a test pinning a value — and it rejects every engine setting with `SettingsException: Setting is read-only`. Unknown names remain project custom settings and are stored as strings, which is how a `///@ Setting` declared by an embedding project can still be pinned by that project's tests. Both lookup paths check the complete short or group-qualified name after hash dispatch; a custom name with a colliding hash remains a custom setting on reads and writes. The managed settings bridge follows this path; it must not shadow a built-in setting in the custom map. Generated numeric, bool and enum accessors whose ABI manifest entry is typed read through indexed `Native.GetSettingValue<T>`: a builtin entry through its typed `GlobalSettings` field, a project `///@ Setting` through the custom-setting map; string and list settings keep the name-based helpers. A custom value is parsed once into a typed cell of the ABI setting entry and re-read only after `GlobalSettings::GetCustomSettingsGeneration()` moved — every writer of the custom map (`SetRuntimeSetting`, `SetCustomSetting`, `SetValue`, `CopyFrom`) bumps it — so a warmed read is a generation compare and a copy.

Do not document one embedding project's `.fomain` contents as universal engine behavior. Use project docs for concrete values; use this page for the engine mechanics that consume them.

Startup resolves one thing before all of this: `ResolveWritableRoot(args)` in `Source/Frontend/ApplicationInit.cpp` answers where the process may write, from the command line and the `INSTALLED` marker alone. It runs before the config is located, because the log file, the local-config cache and everything else land under that root — so `Common.UserWritablePath` is **not settable from a config file** and is registered as an ordinary read-only `SETTING` beside the other engine-filled values such as `Common.Packaged`: `GlobalSettings::ApplyWritableRoot()` stores the resolved answer and every consumer reads it back from there. That is the only reason it is a setting at all — a dozen consumers across client, server, scripting and `SourceExt` hold settings and nothing else, so the setting is how a value resolved by a manual argv scan reaches them. The log file is then opened straight at its final location, so an application whose own directory is read-only never writes there at all. The command line is then applied to the live settings exactly **once**, after the config, sub-config and local config, so it takes final precedence over all of them; a single pass also keeps `+`-append overrides (`-Setting +value`) from accumulating twice. That single pass logs each `Set <name> to <value>` override. In that log, settings whose name contains one of the masking tokens are printed as `Set <name> to ***`, so a credential such as `Auth.WebTokenVerifySecret` never appears in plaintext (server logs may be shared). The tokens are the `Common.SecretSettingTokens` setting (a case-insensitive substring list, default `secret token password apikey`), which `GlobalSettings::IsSecretSettingName()` reads. Command-line overrides are logged only on the final pass — after `ApplyDefaultSettings()` and the config file have run — so the list is already populated, and an embedding project extends it through config to cover credentials the generic tokens miss (Last Frontier sets `Common.SecretSettingTokens = secret token password apikey dsn` so `Sentry.Dsn` is masked). Empty means portable unless an `INSTALLED` marker sits next to the executable; `*` resolves through `platform::get_user_data_base()` plus `Common.GameName`; an explicit path is resolved directly. If the target directory or required cache/resource subdirs cannot be created, the resolver logs a warning and reverts to portable layout.

## Resource packs and data sources

`ResourcePackInfo` describes resource-pack inputs that bakers and runtimes consume. The bake side uses `BakingContext` / `BakerDataSource` in `Source/Tools/Baker.*`; the runtime side uses mounted `DataSource` and `FileSystem` abstractions.

Resource-pack input directories are mounted recursively. `IncludePatterns` and `ExcludePatterns` are optional space-separated glob lists applied to normalized resource-relative paths before any baker runs. An empty include list accepts every path; exclusion is evaluated after inclusion and wins. Patterns are case-sensitive and support:

- `*` — zero or more characters other than `/`;
- `?` — exactly one character other than `/`;
- `**` — zero or more characters including `/`; `**/` also matches zero directory levels.

Both `/` and `\` are accepted as pattern separators and normalized to `/`. For example, `IncludePatterns = **/*.fomap` selects maps at any depth, while `ExcludePatterns = **/_*.fomap` removes scratch maps such as `Generated/_compose.fomap`. Multiple packs may mount the same `InputDirs` and select disjoint resources with different pattern lists. Use `IncludePatterns = *` to reproduce the former top-level-only input behavior.

`Baking.IgnoreInputDirs` (default empty) leaves directories out of every pack's `InputDirs`. Entries are resolved against the directory of the config that declares each pack, exactly as `InputDirs` are, and removed from every pack that lists them, so every baker and tool that walks `InputDirs` stops seeing them. An entry that is not an input directory of any declared pack is a settings error, because a misspelled entry would otherwise leave the directory baked with nothing reporting it; the check is skipped when no declared pack has inputs, as in a packaged application whose baked config carries the setting and declares its packs by name and side only. Resource packs are parsed with the config, before a sub-config or the command line can set this, so `ApplyAutoSettings()` rebuilds the packs in effect from the declared ones each time it runs. A shipping bake names test directories here in its sub-config and bakes neither the test sources nor anything declared in them. That matters for metadata as much as for code: a test file's `///@ RemoteCall` registered without its handler would stop the runtime at startup, because every registered inbound remote call needs one. The match is by directory, not by path prefix: a pack that mounts a parent directory with patterns reaching into the ignored one still bakes those files.

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

### Shared index over mounted sources

A point lookup (`ReadFile()`, `ReadFileHeader()`, `IsFileExists()`) is answered from one shared index when every
mounted source could hand its content over; otherwise the sources are probed in mount order as before.
`DataSource::GetIndexSnapshot()` is that hand-over: a source whose content is fixed until it is remounted returns
all of it, and a source whose answer depends on the world at call time returns `nullopt`. The default is `nullopt`,
so a source that says nothing keeps being probed - a missed override costs a lookup, a wrong one serves a file that
has since moved.

Every pack-backed source offers a snapshot - `ZipFile`, `EmbeddedFile`, `FalloutDat`, `FilesList` - and so does the
empty stand-in a `maybe_not_available` mount produces when its pack is absent. That last one is not a detail:
`GetClientResources()` mounts every pack name a second time against the writable overlay so a downloaded pack wins
over the installed copy, and on a client that has downloaded nothing yet every one of those is absent. If an absent
pack withheld a snapshot, the file system the game actually plays on would be off the index by default.

Directory sources offer none, cached or otherwise. `CachedDir` could - its file tree is already a snapshot refreshed
only by `Reindex`, so indexing it would add no staleness of its own - and it is withheld by decision rather than by
capability: unpacked mounts go through `CachedDir`, and development runs are meant to keep the probe loop. A file
system mounted entirely from packs - what a packaged client, server, mapper and viewer use - therefore resolves a
path with a single hash lookup, while a development run over directories, the baker's live input dirs and the
on-demand baker data source keep probing. Mixing needs no configuration: one source without a snapshot
disables the index for that file system. The decision is per instance rather than per build, because a packaged
client also builds mixed file systems: the updater's own resources, and the file system that checks a pushed file
list, both mount the resource directory as a non-cached dir to size the pack files while the updater is rewriting
them, and that directory must not be answered from a snapshot.

The index is filled as sources are mounted. A new source goes in front of the others and claims every path it holds
away from them, which is the shadowing the probe loop already produced; `ReindexDataSources()` rebuilds it. It is
never populated from a lookup: the read path takes no lock, because the source list is only mutated during setup,
and filling an index lazily from a `const` lookup would either race or put a mutex on every read.

`FilterFiles()` and `GetAllFiles()` stay on the source loop even where the index exists. Their output order is
source by source, and consumers depend on it - script module load order, prototype registration - which a hash
container does not preserve.

`Common.Packaged` is a fixed auto-setting populated from the executable's packaged marker by `GlobalSettings::ApplyAutoSettings()`. After settings are loaded, runtime policy must read that snapshot (`settings.Common.Packaged`) so copied or injected settings remain internally consistent and testable. Direct `IsPackaged()` checks are reserved for pre-settings bootstrap decisions and `FileSystem::AddPackSource()`, where the physical executable marker deliberately selects archive-versus-directory mounting; tests may also inspect that marker when choosing compatible fixtures.

Installed clients keep the read-only base resources mounted from `ClientResources` and layer the writable resource overlay from `fs::make_writable_path(UserWritablePath, ClientResources)` on top. `GetClientResources()` owns that ordering for both the updater's post-sync metadata check and the gameplay `ClientEngine`; do not reconstruct the pack view independently in either path. The updater writes resource patches into that overlay, so the exact current files that pass validation also win runtime lookup and hash checks without modifying the install directory. A ZIP entry read failure identifies the archive path and the resource-relative entry in `DataSourceException` context; short reads also record the expected byte count, actual read result, and close result. Native runtime binary update paths are owned by [ClientUpdater.md](ClientUpdater.md).

## Low-level disk access

`Source/Essentials/DiskFileSystem.*` performs direct disk operations below mounted engine resources. `fs::write_file()` writes content at the given path and does **not** guarantee the resulting directory entry carries that name verbatim: on a case-insensitive filesystem (Windows, default macOS) an existing entry differing only by letter case is reused and keeps its own name. Callers that address files by exact name and rewrite a tree they do not own — the baker being the one in-engine case — reconcile names themselves rather than paying for a check on every write; see [BakingPipeline.md](BakingPipeline.md#output-names-are-reconciled-with-the-names-bakers-addressed).

## Cache storage

`Source/Common/CacheStorage.*` stores named binary/string cache entries behind `HasEntry()`, `GetString()`, `GetData()`, `SetString()`, `SetData()`, and `RemoveEntry()`. Bounded consumers use `GetDataBounded(name, max_size)`, which checks the file size before allocating and distinguishes `Success`, `Missing`, `TooLarge`, and `Failed`, plus `SetDataChecked(...)`, which reports whether the complete write succeeded. The underlying disk helper `fs::read_file_bounded` applies the same pre-allocation cap and answers an oversized file with an empty result instead of raising. It is separate from resource packs: cache entries are mutable runtime/tool artifacts, while baked resources are generated from configured inputs. Client-side cache consumers resolve relative cache paths through `fs::make_writable_path(UserWritablePath, CacheResources)`, so portable clients keep cache next to the executable and installed clients write under the per-user root.

Managed class-library resources deliberately cross that boundary at startup. If the mounted resources contain `ManagedRuntime/`, `ManagedScriptBackend` hashes their normalized paths and bytes, restores the complete tree through a temporary directory, validates it, and atomically publishes it at `<CacheDir>/ManagedRuntime/<content-hash>/` before Mono initialization. A matching cache is reused; a partial or damaged one is rebuilt. Every write and check of that tree goes through the `fs_*` helpers, which pass UTF-8 paths and switch to extended-length paths on Windows: the staging directory adds a content hash and a process id under the cache root, so a deep profile or temp directory otherwise crosses `MAX_PATH` and the freshly written copy fails its own validation. This is a derived cache of the current resource pack, not a second source of truth. A clean side-by-side `ManagedRuntime/` is consulted only when resources do not contain the payload, for unpackaged applications and build tools.

An entry is stored as one plain file named after the entry, with path separators folded to `_`, so the cache directory stays readable and inspectable. Two entry names that differ only in those separators therefore map to the same file — acceptable because an entry is only ever a cache, where a miss is always recoverable, but it means a caller that needs distinct entries must not rely on directory structure alone to separate them. The cache is not a confidentiality boundary: anything that must not be readable at rest has to be protected by its owner before it is handed over (the embedding project's secure-storage bridge does exactly that).

## Settings store

`Source/Common/SettingsStorage.*` persists small per-user tool/editor preferences (ImGui window layout, view options, last selection) behind `GetString()`/`SetString()`, typed `GetInt`/`SetInt`, `GetBool`/`SetBool`, `GetFloat`/`SetFloat`, `HasKey()`, and `Remove()`. It is scoped by an application name passed to the constructor so different tools never collide. The backend is platform-specific through a pimpl: on `FO_WINDOWS` the values are `REG_SZ` entries under `HKCU\Software\<FO_NICE_NAME>\<app_name>` (Win32 headers are confined to the `.cpp` behind `WIN32_LEAN_AND_MEAN` + `WinApiUndef.inc`, using the explicit `*A` registry entry points); on other platforms it is a per-application `CacheStorage` under `platform::get_user_data_base()/<FO_NICE_NAME>/<app_name>`. It is the one writable location that does not come from `Common.UserWritablePath`, deliberately: it sits in `Common`, below settings, and a tool's preferences belong to the user rather than to one game install, so they must not move with a client's writable root (on Windows the backend is a registry key, not a directory at all). Every value is stored as a string (the typed accessors serialize through it), so both backends behave identically, and the multi-line ImGui `imgui.ini` blob round-trips verbatim. Persistence is **best-effort**: a backend failure is logged, never thrown, so a tool never dies because its settings could not be written. It differs from `CacheStorage` in intent (durable user preferences vs. regenerable cache artifacts) and, on Windows, in medium (registry vs. files).

Only the GUI tools reference it (Mapper `MapperEngine::_uiSettings`, migrated from the resource `Cache`; standalone AnimationViewer / ParticleViewer, each loading in its constructor and saving on shutdown). It lives in `CommonLib` for simplicity, but because the client and server reference no `SettingsStorage` symbol, the linker (`/OPT:REF` plus on-demand static-library inclusion) drops the object from the shipped client/server binaries — so the Windows registry calls never land where antivirus heuristics might flag them. ImGui's own `imgui.ini` autosave stays disabled (`Application.cpp`), so all layout persistence flows through this store.

## Build and package routing

- `BuildTools/cmake/stages/Codegen.cmake` generates internal config inputs used by runtime settings.
- `BuildTools/cmake/stages/ScriptsAndBaking.cmake` wires resource baking/script compilation that consume `ResourcePackInfo` and baking settings.
- `BuildTools/cmake/stages/Packages.cmake` packages resources for runtime targets.
- `Source/Tools/ConfigBaker.*` bakes config resources; full bake orchestration is in [BakingPipeline.md](BakingPipeline.md).

## Tests to inspect

Focused tests for this area:

- `Source/Tests/Test_CacheStorage.cpp`
- `Source/Tests/Test_SettingsStorage.cpp`
- `Source/Tests/Test_ConfigFile.cpp`
- `Source/Tests/Test_DataSource.cpp`
- `Source/Tests/Test_DiskFileSystem.cpp`
- `Source/Tests/Test_FileSystem.cpp`
- `Source/Tests/Test_ManagedScriptBaker.cpp`
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
