# Client Runtime Split and Updater

> Engine-owned documentation. Paths under `../` are relative to the FOnline engine root. Paths under `../../` point to an embedding game project such as Last Frontier when this engine is used as a submodule.

The native client ships as two artifacts:

- `LF_Client.exe` â€” thin host application built from [../Source/Applications/ClientApp.cpp](../Source/Applications/ClientApp.cpp). Stays compatible across runtime versions.
- sibling runtime library (`LF_Client.dll` in a Windows build tree, `.so` / `.dylib` on Linux / macOS) â€” loadable runtime built by the `LF_ClientLib` CMake target from [../Source/Applications/ClientLib.cpp](../Source/Applications/ClientLib.cpp). Contains the gameplay client engine.

The host loads the runtime through a stable C ABI ([../Source/Client/ClientRuntimeApi.h](../Source/Client/ClientRuntimeApi.h)) and falls back to the embedded client linked into `LF_Client.exe` if loading fails.

**Platform support.** The host + runtime split is built only on Windows, Linux and macOS â€” that is the set of platforms where `CanSelfUpdateNativeModules()` returns `true` and where the static_assert at the top of `ClientLib.cpp` accepts the build. Web, iOS and Android ship a single self-contained `LF_Client` binary instead: the engine code is statically linked into the executable and the runtime-loading branch in `RunEmbeddedOrLoadedClient()` is never taken. The CMake gate that enforces this lives in [../BuildTools/cmake/stages/Applications.cmake](../BuildTools/cmake/stages/Applications.cmake) (`if(FO_WINDOWS OR FO_LINUX OR FO_MAC)`), and Android additionally takes the `FO_BUILD_LIBRARY` branch which produces only the shared `LF_Client` artifact required by the SDL Android Java loader.

The updater protocol is the same machinery used to deliver gameplay resources, but versioned independently from gameplay compatibility so a host released today can ingest tomorrow's runtime module without a host-side rebuild.

## Two-layer client startup

The host always tries to load the bundled runtime first; whichever module ends up running
the game (DLL or embedded host) drives a uniform two-stage updater UI:

```
LF_Client.exe (host)
    â”‚
    â”‚  1. Resolve runtime path (`GetClientRuntimeLivePath()` from current exe name, or --ClientLibPath)
    â”‚  2. ApplyStagedBinaryUpdate() â€” promote pending `<live>-staging` over `<live>`
    â”‚     (also recovers a crashed-mid-update install on first boot)
    â”‚  3. Platform::LoadModule(<runtime>) â†’ FO_QueryClientRuntimeExports(...)
    â”‚  4. Validate ClientRuntimeExports.Metadata (ABI, compatibility version)
    â”‚
    â–¼
<live runtime module>                 â”€â”€â”€ Case 2 (optimistic, common path)
    â”‚
    â”‚  RunClientRuntime: InitApp â†’ resource Updater (UI) â†’ ClientEngine â†’ MainLoop
    â”‚  If resource updater reports compat outdated and platform supports self-update:
    â”‚     stage 2: binary Updater (UI) writes or verifies the module at `<live>-staging` / `<live>`
    â”‚     return ClientRuntimeResult { ReloadRequested, RequestedRuntimePath = <live> }
    â”‚
    â””â”€â–º returns ClientRuntimeResult (Shutdown / ReloadRequested / FatalError)

If LoadModule failed (no DLL on disk yet):    â”€â”€â”€ Case 1 (cold install / fallback)
    Embedded client runs the same RunClientRuntime in the host module. After it
    signals ReloadRequested, the host tears down its own Application instance and goes
    to the reload step below.

Reload step (taken on either Case after ReloadRequested):
    ApplyStagedBinaryUpdate() renames `<live>-staging` over `<live>` (atomic .bak rollback),
    then RunClientFromLibrary loads the freshly-renamed runtime; compat is not re-checked
    against the host's built-in version because the new module by definition carries a newer
    compatibility string.
```

`ApplyStagedBinaryUpdate` is idempotent: if no `<live>-staging` file exists it returns
`true` and does nothing. That makes startup-time recovery (host crash mid-update) and the
in-process reload path use the same code.

The embedded client (host module hosts the game and the updater itself) runs when:
- the requested runtime path could not be loaded **and**
- the requested compatibility version equals the host's built-in `FO_COMPATIBILITY_VERSION`.

If `--ClientLibCompatibilityVersion <version>` is passed and differs from the host's compatibility, the host **does not** fall back â€” failure means "wrong runtime, refuse to run" rather than "silently downgrade to host code".

After a successful Case 1 binary update + reload, the embedded host's `Application` instance
is destroyed (`App.reset()` in `RunClientRuntime`) before the host loads the freshly
downloaded DLL. This keeps a single SDL window alive at any one time â€” the host's window
disappears, then the DLL's `InitApp` creates a fresh one. Without this teardown the two
modules' independent `unique_ptr<Application> App` statics would briefly co-exist.

## Host CLI surface

```text
LF_Client.exe                                                           # bundled runtime, default compatibility
LF_Client.exe --ClientLibPath <path>                                    # explicit runtime, default compatibility
LF_Client.exe --ClientLibPath <path> --ClientLibCompatibilityVersion <ver>  # explicit runtime, no embedded fallback if ver != built-in
```

The bundled runtime library name is **derived from the host executable name** at startup via `GetCurrentClientRuntimeLibraryName()` (returns the exe basename without extension; falls back to `FO_DEV_NAME` when `Platform::GetExePath()` cannot resolve). The resolved live path is `GetClientRuntimeLivePath() = <exe_dir>/<library_name>` (extension is appended by `Platform::LoadModule`). Renamed/multi-instance hosts therefore each load their own sibling module (`MyAlt.exe` â†” `MyAlt.dll`) instead of sharing one â€” no settings or packaging-time config patching needed. In the build tree, the `LF_ClientLib` target still writes its canonical `LF_ClientLib.*` artifact and also copies a host-derived alias (`LF_Client.dll` / `LF_Client.so` / `LF_Client.dylib`) so an unpackaged `LF_Client` can exercise the same loading path as a packaged client.

## Runtime ABI

[../Source/Client/ClientRuntimeApi.h](../Source/Client/ClientRuntimeApi.h) is the only contract between host and runtime. Both sides agree on:

- `FO_CLIENT_RUNTIME_HOST_ABI_VERSION` â€” bumped when the structs in this header change shape.
- `ClientRuntimeMetadata` â€” runtime name, build hash, gameplay compatibility version.
- `ClientRuntimeExports` â€” entry table returned by `FO_QueryClientRuntimeExports(host_abi_version, *exports)`.
- `ClientRuntimeResult` â€” how the runtime communicates back to the host (`Shutdown`, `ReloadRequested`, `FatalError`).

A runtime that wants to hand control back for reload sets `ResultKind = ReloadRequested` and fills `RequestedRuntimePath`; the host then re-loads with that path. The host does **not** validate the new module's compatibility version against its own built-in `FO_COMPATIBILITY_VERSION` on a reload â€” by definition the just-staged DLL is the carrier of a *new* compat version, so the new module's metadata is the authority.

The runtime stages a new module as `<live>-staging` next to the live module, where `<live>` is `GetClientRuntimeLivePath()` (the full live path including the platform runtime extension, e.g. `<exe_dir>/LastFrontier.dll`). The host promotes via `GetClientRuntimeStagingPath()` â†’ `GetClientRuntimeLivePath()` rename. `RequestedRuntimePath` in the returned `ClientRuntimeResult` is the post-swap path (`<live>`), not the staging path, because `LoadModule` is called *after* the host applies the swap.

A matching PDB (Windows-only, named `<live>.pdb`, e.g. `LastFrontier.dll.pdb`) is staged side-by-side as `<live>.pdb-staging` and promoted by `ApplyStagedBinaryUpdate` after the main DLL swap succeeds. The PDB swap is best-effort â€” failure only degrades stack traces, so it never blocks the runtime swap, while the DLL swap remains backup-rename-rollback atomic. The client-side filter accepts any server file whose basename starts with `<runtime_name>.`, so the DLL (`LastFrontier.dll`) and its PDB sibling (`LastFrontier.dll.pdb`) both match and ride the same `UpdateFileTarget::ClientBinaries` channel.

## Updater protocol

Versioned by `FO_UPDATER_VERSION` ([../Source/Common/Common.h](../Source/Common/Common.h)). Bump it when the wire format changes. Gameplay compatibility (`Settings.CompatibilityVersion`) is separate and changes with every build.

### Handshake

| Direction | Field | Type | Purpose |
|-----------|-------|------|---------|
| client â†’ server | `CompatibilityVersion` | `string` | gameplay compatibility |
| client â†’ server | `updater_version` | `uint32` | `FO_UPDATER_VERSION` |
| client â†’ server | `binary_target` | `string` | e.g. `Windows-win64`, `Android-arm64` (from `GetCurrentBinaryUpdateTargetName()`) |
| client â†’ server | `in_encrypt_key` | `uint32` | session keys |
| server â†’ client | `compatibility_outdated` | `bool` | gameplay version mismatch |
| server â†’ client | `updater_outdated` | `bool` | `FO_UPDATER_VERSION` mismatch â€” protocol is unusable |
| server â†’ client | `out_encrypt_key` | `uint32` | session keys |

`updater_outdated == true` is fatal to the connection â€” the protocol contract has changed and no further messages are valid. `compatibility_outdated == true` only blocks gameplay; the updater can still deliver resources / native modules to bring the client back to current compatibility.

### Init data

Sent once after a non-outdated handshake. Contains the descriptor of files the server is offering for this binary target plus initial gameplay state (global properties, synchronized time).

Each descriptor entry is:

| Field | Type | Notes |
|-------|------|-------|
| `name_len` | `int16` (`-1` terminates the list) | client-relative path length |
| `name` | `char[name_len]` | client-relative path |
| `size` | `uint64` | full file size |
| `hash` | `uint64` | FNV-1a 64-bit hash of the file content |
| `target` | `UpdateFileTarget` (`uint8`) | `ClientResources` or `ClientBinaries` |
| `file_index` | `uint32` | server-assigned index for `GetUpdateFile` |

Common (gameplay-resource) entries are emitted for every binary target. Per-target binary entries (`UpdateFileTarget::ClientBinaries`) are emitted only for the matching `binary_target` from the handshake. The client then filters binary entries by the current host-derived runtime basename, so `LF_Client.exe` downloads `LF_Client.dll` while `LF_Client_OpenGL.exe` downloads `LF_Client_OpenGL.dll` even though both report the same CPU/OS target.

### Resumable file transfer

The client drives a single transfer at a time:

```text
client â†’ server: GetUpdateFile  { file_index: uint32, start_offset: uint64 }
server â†’ client: UpdateFileData { update_portion: int32, raw bytes[update_portion] }
```

The server picks `update_portion` (capped by `Network.UpdateFileMaxPortionSize`, currently 5 MB in this project â€” see [LastFrontier.fomain](../../LastFrontier.fomain)). The client requests the next portion with `start_offset = bytes_already_written`, so partial transfers resume from disk on reconnect without server-side state.

Server-side validation (in [../Source/Server/UpdaterBackend.cpp](../Source/Server/UpdaterBackend.cpp)):

- `file_index` out of range â†’ `LogType::Warning` + `HardDisconnect`.
- `start_offset > file_size` â†’ `LogType::Warning` + `HardDisconnect`.
- `update_file_max_portion_size <= 0` (misconfiguration) â†’ `LogType::Warning` + `HardDisconnect`.
- Disk-mode read failure â†’ `LogType::Warning` + `HardDisconnect`.

Client-side, the `Updater` writes each portion to a `~<filename>` temp file, hashes via streamed `fs_hash_file` ([../Source/Essentials/DiskFileSystem.cpp](../Source/Essentials/DiskFileSystem.cpp)) once complete, then atomically renames over the live file (`ReplaceFileSafely`). The updater hash is FNV-1a 64-bit (separate from the engine's wyhash-backed `hashing_ex::hash`, which is reserved for hash-tables and `hstring`); streaming a chunked file produces the same digest as `fs_hash_data` over the full buffer, so server in-memory hashing and client streaming hashing agree by construction. Streaming the hash means even multi-GB resource packs never get fully buffered in RAM on either side.

To avoid rehashing existing packs on every startup (the hashing cost dominates the updater's "is this file already current?" pass for multi-GB resource packs), the disk-side hash check goes through `Updater::IsDiskFileHashMatch`, which caches the result in `CacheStorage` ([Settings.CacheResources](../../LastFrontier.fomain)) keyed on the file path. The cached entry stores `(size, mtime, hash)`; the cache lookup is invalidated automatically when either size or mtime changes, so a refreshed pack is always rehashed exactly once.

There are no backward-compatible fallback paths. The previous "session-state file index + portion counter" protocol was removed when `FO_UPDATER_VERSION` was introduced; clients and servers must agree on the version.

## Server-side: `UpdaterBackend`

[../Source/Server/UpdaterBackend.h](../Source/Server/UpdaterBackend.h) is owned by `ServerEngine` as a `unique_ptr`. When `_updaterBackend` is null (unpackaged dev server) the server rejects `GetUpdateFile` with `HardDisconnect` â€” there is nothing to serve.

Public API:

```cpp
void LoadFromClientResources(const GlobalSettings& settings);
void ProcessUpdateFile(ServerConnection* connection, int32_t update_file_max_portion_size);
auto GetUpdateDescriptor(string_view binary_target_name) const -> const vector<uint8_t>&;
```

- `LoadFromClientResources` walks `Settings.ClientResources`, picks every pack listed in `Settings.ClientResourceEntries` (excluding `Embedded`), then enumerates `Settings.PlatformBinaries/<target>/` for per-target binaries (default `PlatformBinaries/`, sibling of `Resources/` in the package layout).
- Entries are stored as `UpdateFileData { InMemory, MemoryData?, DiskPath?, Size, Hash }`. Memory mode keeps the whole pack in RAM for the lifetime of the server. Disk mode keeps only `DiskPath`, `Size`, and the streamed `Hash`; portions are read on demand by `ReadUpdateFilePortion(...)`.
- Descriptors are cached per `binary_target_name`. Common-resource entries are merged into every per-target descriptor; targets without specific binaries fall back to the common-only descriptor.

## Settings

| Setting | Where | Purpose |
|---------|-------|---------|
| `Network.UpdateFileMaxPortionSize` | top-level | Maximum bytes per `UpdateFileData` response. Drives both transfer throughput and per-message memory pressure. Default 1 MB (engine) / 5 MB (this project). |
| `ServerNetwork.UpdateFilesInMemory` | top-level + `[SubConfig]` | `True` keeps every packaged update file in RAM (low CPU under load). `False` serves from disk on demand (low RAM, more I/O). Public `[SubConfig]`s in this project: `PublicGame = True`, `DailyTest = True`, `Staging = True`. |
| `Baking.PlatformBinaries` | top-level | Directory the server reads per-target client runtime libraries from, and the packager writes them to. Default `PlatformBinaries`, resolved relative to the server's working directory / package root. |

There is no auto-detection of memory vs disk mode in C++. Choose explicitly per environment.

## Packaging

[../BuildTools/package.py](../BuildTools/package.py) does both halves:

- **Client packages** include the host exe (e.g. `LF_Client.exe`) and the matching runtime library renamed to the same basename next to it (`LF_Client.dll`). The host derives the library name from its own exe basename at startup, so no config patching is required to point one at the other.
- **Server packages** also stage every available client runtime library under `<Settings.PlatformBinaries>/<binary_target>/<output_name><runtime_ext>` (default `PlatformBinaries/`, sibling of the client-resources dir in the package layout) so a different-platform client connecting to this server can self-update its native modules.
- **PDBs for Windows runtime DLLs** are shipped under `<runtime_dll>.pdb` (e.g. `LastFrontier.dll.pdb`) â€” both next to the bundled client DLL and inside every server-staged `PlatformBinaries/Windows-*` payload. The host exe keeps its own `<host_name>.pdb` so the two namespaces never collide. `package.py` patches the renamed DLL's CodeView (`RSDS`) record in place to point at the new PDB filename, so DbgHelp / `backward-cpp` resolve symbols automatically when the runtime is loaded. Missing PDB inputs or failed RSDS patches `assert` immediately during packaging â€” symbol gaps are never silently tolerated.

Both the bundled runtime library in client packages and the runtime libraries staged for server-side binary updates go through the same package-time patching as ordinary executables: embedded resources, internal config, and packaged mark are written by `package.py`. Variant-specific config is applied to the runtime payload that actually runs the game; for example the Windows OpenGL runtime receives `ForceOpenGL=1`. The embedded-resource zip is produced with pinned entry timestamps and permissions (`make_embedded_pack`), so the bundled-client copy of a runtime and the matching `<Baking.PlatformBinaries>/<target>/<output_name><ext>` payload remain byte-identical across the two separate package runs (this is what `../../Tools/PipelineTests/test_updater_binary_update.py` asserts before corrupting the client copy).

The internal config patch area is generated from the CMake `FO_INTERNAL_CONFIG_CAPACITY` option, next to `FO_EMBEDDED_DATA_CAPACITY`; `package.py` discovers the actual reserved size from the generated binary markers before writing config data.

Naming convention from `BuildBinaryEntry` / `build_runtime_update_target_name`:
- `Windows-win64`, `Linux-x64`, `Linux-arm64`, `macOS-arm64`, `Android-arm64`, etc.
- Profiling variants get the `_Profiling` suffix in the staged file name.
- The Windows OpenGL variant (`OGL`) is staged separately and patches `ForceOpenGL=1`.

## Lifecycle

```
LF_Client.exe main
    â”œâ”€â”€ ResolveRequestedClientRuntime(argc, argv)        # Path + CompatibilityVersion + ExplicitPath
    â”‚
    â”œâ”€â”€ RunClientFromLibrary(argc, argv, requested, *)   # CASE 2: bundled runtime exists
    â”‚     â”œâ”€â”€ ApplyStagedBinaryUpdate()                  # promote <live>-staging (no-op when missing)
    â”‚     â”œâ”€â”€ Platform::LoadModule + FO_QueryClientRuntimeExports
    â”‚     â”œâ”€â”€ Validate exports + metadata
    â”‚     â”œâ”€â”€ exports.Run(argc, argv, &result)           # DLL drives RunClientRuntime:
    â”‚     â”‚     â”œâ”€â”€ single Updater (UI) connects to the server. The connect result picks the mode:
    â”‚     â”‚     â”‚     â”œâ”€â”€ Success         â†’ resources mode â†’ sync ClientResources, finish ResourcesReady
    â”‚     â”‚     â”‚     â””â”€â”€ CompatibilityOutdated:
    â”‚     â”‚     â”‚             â”œâ”€â”€ if !CanSelfUpdate    â†’ finish PlatformUnsupported, caller shows store msg
    â”‚     â”‚     â”‚             â””â”€â”€ else                  â†’ binaries mode â†’ write ClientBinaries to
    â”‚     â”‚     â”‚                                          `<live>-staging` or verify `<live>`, finish BinariesStaged
    â”‚     â”‚     â”œâ”€â”€ On BinariesStaged: set ResultKind = ReloadRequested, RequestedRuntimePath
    â”‚     â”‚     â”œâ”€â”€ On any other non-success result: ShowUpdaterFailure(result) and quit
    â”‚     â”‚     â””â”€â”€ unload of DLL (scope_exit) frees the loaded module
    â”‚     â””â”€â”€ If ResultKind == ReloadRequested: RunReloadedRuntime
    â”‚           â””â”€â”€ RunClientFromLibrary again â€” second ApplyStagedBinaryUpdate moves
    â”‚             the just-downloaded DLL on top of the previous one before LoadModule
    â”‚
    â””â”€â”€ If LoadModule failed (CASE 1: no DLL yet, packaged install):
          if !CanFallbackToEmbeddedClient(requested): return false
          RunEmbeddedClient(argc, argv, *)               # host-module RunClientRuntime
          (same single-Updater flow as the DLL; host module's App.reset() runs after
           ReloadRequested so SDL state is gone before the next LoadModule)
          if ResultKind == ReloadRequested â†’ RunReloadedRuntime
```

A single `Updater` instance handles both gameplay-resources and native-binaries syncs.
It picks the mode internally based on the server's compatibility verdict on connect â€” no
per-stage construction, no caller-side mode parameter, no separate "BinaryUpdater" type or
headless variant. The splash UI (`Application::MainWindow`) is shared throughout, so the
user always sees indication of what is happening. The terminal state is exposed via
`Updater::GetResult()` returning `UpdaterResult` (see header).

`CanSelfUpdateNativeModules(GetCurrentUpdatePlatform())` decides whether the binary
self-update step is even attempted: Windows / Linux / macOS are eligible; Web / iOS / Android
currently require manual client updates because the platform either bundles the runtime
inside an APK (Android), forbids dlopen of arbitrary code (iOS), or has no comparable
mechanism (Web). On those platforms the resource updater detects compat outdated and the
host shows a "Client outdated, please update via your app store" message before quitting,
instead of looping back to the game which would only reject the connection again.

## Validation

| Symptom | First signal |
|---------|--------------|
| Host can't find runtime, no fallback possible | embedded host's resource updater fails to download anything; client message box `Failed to update native client modules for binary target <target>` |
| Updater protocol mismatch | server log `Connected client X has outdated updater version Y`; client message box `Client updater outdated, please update the base client` |
| Gameplay version mismatch on a self-update platform | resource updater finishes silently with `WasCompatibilityOutdated() == true`; the runtime then opens the binary updater UI and downloads the current host's module to `<live>-staging` next to the live one, or reloads immediately if `<live>` already matches the server payload |
| Gameplay version mismatch on Web / iOS / Android | message box `Client outdated, please update via your app store`, then quit (no in-process self-update on these platforms) |
| Wrong file index / offset | server log `Wrong file index N, from host '...'` / `Wrong update file offset O, file index N, client host '...'` (both at `LogType::Warning`), client gets disconnected |
| Server has no native update for this target | message box `Server doesn't provide a native client update for binary target <target>` |
| Stale staging file | `<live>-staging` survived a previous failed swap; the next `LF_Client.exe` startup promotes it via `ApplyStagedBinaryUpdate` before loading the runtime |
| Stack trace shows raw addresses for the new runtime DLL | After a binary self-update the renamed `<live>.dll`'s CodeView entry must reference its sibling `<live>.dll.pdb`. If `package.py` skipped the RSDS patch (it will assert when this happens), `dbghelp`/`backward-cpp` cannot find the PDB and frames in the runtime resolve to addresses only |

Local validation steps:

1. Build `LF_UnitTests` and run it. [../Source/Tests/Test_ClientRuntimeApi.cpp](../Source/Tests/Test_ClientRuntimeApi.cpp) exercises the ABI surface; [../Source/Tests/Test_DiskFileSystem.cpp](../Source/Tests/Test_DiskFileSystem.cpp) covers `fs_hash_file` parity with `fs_hash_data` for various sizes including 70000 bytes; [../Source/Tests/Test_Settings.cpp](../Source/Tests/Test_Settings.cpp) covers `UpdateFilesInMemory` sub-config inheritance.
2. Build both `LF_Client` and `LF_ClientLib`. Confirm the client output directory contains the host plus the host-derived runtime alias (`LF_Client.exe` + `LF_Client.dll` on Windows, `LF_Client` + `LF_Client.so` on Linux).
3. Launch `LF_Client.exe` with the bundled runtime present â†’ normal startup (Case 2 happy path: load DLL, resource updater finishes, game starts).
4. Launch `LF_Client.exe --ClientLibPath <path>` with a valid alternate runtime â†’ host routes through the loaded library.
5. Launch `LF_Client.exe --ClientLibPath <path> --ClientLibCompatibilityVersion <other>` and remove the runtime â†’ host fails (no fallback).
6. Point `--ClientLibPath` to an invalid path, no `--ClientLibCompatibilityVersion` â†’ host falls back to embedded client (Case 1).
7. Build a packaged server (e.g. `Daily`) and confirm `<Settings.PlatformBinaries>/<target>/<name><ext>` (default `PlatformBinaries/`, sibling of the client-resources dir in the package layout) contains the per-target runtime libraries and that `ClientResources` pack list contains the resource zips.
8. Interrupt a client mid-download (kill the network) and reconnect â€” the next `GetUpdateFile` resumes from the temp-file size, no full re-download.
9. Force a Case 2 â†’ reload: package a client against an older `FO_COMPATIBILITY_VERSION`, point it at a server with a newer one, run. The resource updater UI should appear briefly, then the binary updater UI takes over (UI/SplashPic identical), the host renames `<live>-staging` over `<live>`, and the new runtime module loads and reaches the game. No process restart involved.
10. Crash recovery: kill the host while the binary updater UI is mid-download. Restart `LF_Client.exe`. `ApplyStagedBinaryUpdate` runs at the start of `RunClientFromLibrary`; if `<live>-staging` is fully written it gets promoted, otherwise the runtime's resume logic completes the download in a normal updater session.

## See Also

- [BuildAndLaunch.md](../../Docs/BuildAndLaunch.md) â€” build / package commands and launch profiles.
- [Architecture.md](../../Docs/Architecture.md) â€” engine + game build layout, target table.
- [Debugging.md](Debugging.md) â€” debugger setup; the host vs runtime split affects which binary the debugger should attach to.
- [SteamIntegration.md](../../Docs/SteamIntegration.md) â€” alternative distribution channel that bypasses the in-game updater.