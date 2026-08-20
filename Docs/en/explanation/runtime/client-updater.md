---
layout: default
title: Client Runtime Split and Updater
locale: en
document_id: client-updater
permalink: /Docs/en/explanation/runtime/client-updater.html
---

# Client Runtime Split and Updater

> Engine-owned documentation for the reusable client host/runtime ABI, update protocol, packaging contract, and recovery behavior. Binary names shown below are illustrative; config overrides, package profiles, and distribution channels belong in the embedding project.

The native client ships as two artifacts:

- `<client-host>.exe` - thin host application built from [ClientApp.cpp](../../../../Source/Applications/ClientApp.cpp). It stays compatible across runtime versions.
- sibling runtime library (`<client-host>.dll` in a Windows build tree, `.so` / `.dylib` on Linux / macOS) - loadable runtime built by the generated `<ProjectDevName>_ClientLib` CMake target from [ClientLib.cpp](../../../../Source/Applications/ClientLib.cpp). It contains the gameplay client engine.

The host loads the runtime through a stable C ABI ([ClientRuntimeApi.h](../../../../Source/Client/ClientRuntimeApi.h)) and falls back to the embedded client linked into `<client-host>.exe` if loading fails.

On native host/runtime platforms the generated `<ProjectDevName>_Client` target depends on
`<ProjectDevName>_ClientLib`, and the runtime target's post-build step copies its output to
the host-derived sibling name. Building the generated host target therefore refreshes both
artifacts; the generated runtime target remains independently buildable when only the runtime
module is needed. The headless host/runtime targets use the same dependency.

**Platform support.** The host + runtime split is built only on Windows, Linux and macOS - that is the set of platforms where `CanSelfUpdateNativeModules()` returns `true` and where the `static_assert` at the top of `ClientLib.cpp` accepts the build. Web, iOS and Android ship a single self-contained generated client binary instead: the engine code is statically linked into the executable and the runtime-loading branch in `RunEmbeddedOrLoadedClient()` is never taken. The CMake gate that enforces this lives in [Applications.cmake](../../../../BuildTools/cmake/stages/Applications.cmake) (`if(FO_WINDOWS OR FO_LINUX OR FO_MAC)`), and Android additionally takes the `FO_BUILD_LIBRARY` branch required by the SDL Android Java loader.

The updater protocol is the same machinery used to deliver gameplay resources, but versioned independently from gameplay compatibility so a host released today can ingest tomorrow's runtime module without a host-side rebuild.

## Contract status

This page is a source-backed explanation of the current reusable host/runtime and
updater behavior. The C ABI version and updater protocol generation are explicit
compatibility contracts. Settings, generated package metadata, and documented
command-line switches keep the stability assigned by their owning references.
Native classes, helper functions, cache-key layout, and staging implementation are
engine internals unless the [Public Contract Index](../../reference/public-contract/index.md) labels them
otherwise. A project must pin an exact Engine revision and revalidate its packaged
payloads whenever any of those internals change.


## Server-side updater backend

The client updater is served by the authoritative server runtime. `ServerEngine` wires an `UpdaterBackend` from `Source/Server/UpdaterBackend.*` during server startup when client packs/resources are prepared. The backend scans client resources and native runtime artifacts, builds target-specific update descriptors, and answers file-portion requests with `NetMessage::UpdateFileData`.

Runtime ownership is split deliberately:

- [Server Runtime](server.md) documents where `UpdaterBackend` is hosted and how it fits into server startup/connection processing.
- This page documents the client host/runtime ABI, staging/reload flow, compatibility checks, and updater protocol behavior visible to the client.

Keep long protocol and host-runtime details here; keep server lifecycle and manager ownership in [Server Runtime](server.md).

## Source paths inspected

- `Source/Applications/ClientApp.cpp`
- `Source/Applications/ClientLib.cpp`
- `Source/Client/ClientRuntimeApi.h`
- `Source/Client/ClientRuntimeApi.cpp`
- `Source/Client/Updater.h`
- `Source/Client/Updater.cpp`
- `Source/Frontend/ApplicationInit.cpp`
- `Source/Server/UpdaterBackend.h`
- `Source/Server/UpdaterBackend.cpp`
- `Source/Server/Server.cpp`
- `Source/Server/Player.h`
- `Source/Server/Player.cpp`
- `Source/Server/ServerConnection.h`
- `Source/Server/ServerConnection.cpp`
- `Source/Common/Common.h`
- `Source/Common/Settings.inc`
- `Source/Essentials/DiskFileSystem.h`
- `Source/Essentials/DiskFileSystem.cpp`
- `Source/Essentials/Platform.h`
- `Source/Essentials/Platform.cpp`
- `BuildTools/cmake/stages/Applications.cmake`
- `BuildTools/cmake/helpers/Build.cmake`
- `BuildTools/package.py`
- `BuildTools/msicreator/createmsi.py`
- `BuildTools/tests/test_package_zip_determinism.py`
- `Source/Tests/Test_ClientRuntimeApi.cpp`
- `Source/Tests/Test_DiskFileSystem.cpp`
- `Source/Tests/Test_Platform.cpp`
- `Source/Tests/Test_Settings.cpp`
- `ThirdParty/rpmalloc/rpmalloc/rpmalloc.c`

## Two-layer client startup

The host tries to load the bundled runtime DLL first on self-update platforms; the **embedded** engine
(statically linked into the host) is the fallback when no sibling DLL is present or it fails to load.
This is uniform across the regular and headless clients — a standalone headless client with no sibling
DLL simply lands on the embedded fallback. Setting `Client.ForceEmbeddedRuntime` (read from the command
line at host startup) forces the embedded path and skips the implicit bundled-DLL load; an explicit
`--ClientLibPath` still loads a DLL. Whichever module ends up running the game (loaded DLL or embedded
host) drives a uniform two-stage updater UI:

```text
<client-host> (host)
    │
    │  1. Resolve runtime path (`GetClientRuntimeLivePath()` from current exe name; an installed
    │     client may select a persisted per-user runtime bootstrap; --ClientLibPath overrides both)
    │  2. ApplyStagedBinaryUpdate(<runtime>) — promote pending `<runtime>-staging` over `<runtime>`
    │     (also recovers a crashed-mid-update install on first boot)
    │  3. Platform::LoadModule(<runtime>) → FO_QueryClientRuntimeExports(...)
    │  4. Validate ClientRuntimeExports.Metadata (ABI; compatibility only when explicitly requested)
    │
    ▼
<live runtime module>                 ─── the running module (loaded DLL by default; host module on fallback / ForceEmbeddedRuntime)
    │
    │  RunClientRuntime: InitApp → resource Updater (UI) → ClientEngine → MainLoop
    │  If resource updater reports compat outdated and platform supports self-update:
    │     stage 2: binary Updater (UI) writes or verifies the module at `<runtime>-staging` / `<runtime>`
    │     return ClientRuntimeResult { ReloadRequested, RequestedRuntimePath = <runtime> }
    │
    └─► returns ClientRuntimeResult (Shutdown / ReloadRequested / FatalError)

No sibling DLL / LoadModule fails, or ForceEmbeddedRuntime is set:    ─── embedded fallback
    Embedded client runs the same RunClientRuntime in the host module. After it
    signals ReloadRequested, the host tears down its own Application instance and goes
    to the restart step below.

Restart step (taken on either Case after ReloadRequested) — PromoteStagedReloadForRestart:
    The runtime already asked the user to restart (ShowUpdaterRestartRequired). The host runs
    ApplyStagedBinaryUpdate(RequestedRuntimePath), renaming `<runtime>-staging` over `<runtime>`
    when a staged file exists (atomic .bak rollback). For an installed client it then persists that
    absolute runtime path in the per-user bootstrap selector and EXITS. The update is applied on the
    next launch, which resolves the selector before settings and loads the promoted runtime as its
    single InitApp. The update is not applied in-process. See "Self-update applies on the next launch"
    below for why.
```

`ApplyStagedBinaryUpdate` is idempotent: if no `<live>-staging` file exists it returns
`true` and does nothing. That makes startup-time recovery (host crash mid-update) and the
exit-time promotion use the same code.

The embedded client (host module hosts the game and the updater itself) runs when:
- the bundled runtime DLL could not be loaded (cold install / missing or invalid DLL); if
  `--ClientLibCompatibilityVersion` was explicitly passed, embedded fallback is allowed only when that
  requested compatibility version equals the host's built-in `FO_COMPATIBILITY_VERSION`, **or**
- `Client.ForceEmbeddedRuntime` is set and no explicit `--ClientLibPath` was given - the host skips the
  implicit bundled-DLL load and goes straight to embedded.

`RunEmbeddedOrLoadedClient` gates the bundled-DLL-first path on `requested_runtime.ExplicitPath ||
(!ForceEmbedded && CanSelfUpdateNativeModules(GetCurrentUpdatePlatform()))`, identically for the regular
and headless clients. `Client.ForceEmbeddedRuntime` is honored from the command line
(`--ForceEmbeddedRuntime`) because the host picks the runtime before settings are otherwise resolved;
a SubConfig/config-only value does not reach this pre-init decision, so launch profiles that must force
embedded on a standalone client pass it on the command line.

Because the regular client loads the `<live>` DLL into its own process, it cannot safely reload that
same path after staging a new module. The host promotes the file and exits so the next process performs
the only post-update load.

The implicit bundled-DLL load intentionally does **not** require the DLL's gameplay compatibility string
to match the host executable's built-in string. The executable is frozen in deployed installs, while the
runtime DLL is the self-updated module; tying the bundled DLL to the old host compatibility would reject
the freshly downloaded runtime and incorrectly start the embedded updater.
`--ClientLibCompatibilityVersion <version>` is the opt-in strict mode for tests and explicit host/runtime probes: when it is passed and
differs from the host's compatibility, embedded fallback is refused rather than silently downgrading to
host code.

Startup/runtime handoff diagnostics go to the normal `<host>.log` through the regular `WriteLog` path.
The host brings up engine global data (`CreateGlobalData()` in `main`) and opens that log fresh up front
(`LogToFile(GetExeLogFileName(), false)`) — the host runs first, so it truncates. It then keeps its handle
open across the loaded-DLL call instead of closing before the handoff: `LogToFile` opens the file without
an exclusive lock (the platform default —
MSVC `std::ofstream` is deny-none, POSIX has no mandatory open lock), and every log write seeks to end of
file first (`WriteSync`). The host EXE and the runtime DLL are two engine
modules in one process, each carrying its own copy of the engine global data, so they cannot share one
`std::ofstream`, but with shared access both can hold the same file open and the seek-to-end keeps each
module's writes after whatever the other appended — so the host's post-handoff lines land *after* the
DLL's whole session rather than overwriting it. Client runtimes pass `AppInitFlags::AppendLogFile` into
`InitApp` (which resolves the same `GetExeLogFileName()`), so each DLL/embedded `InitApp` appends to the
shared file instead of truncating the host's lines. The DLL's
`FO_QueryClientRuntimeExports` and the first pre-`InitApp` line of its `RunClientRuntime` run before the
DLL has its own global data, so those few lines go to stdout only; the host already records the full
load/accept/enter handoff to the file, and once the DLL's `InitApp` runs, its `WriteLog` appends to the
shared file too.

After a successful Case 1 binary update + restart request, the embedded host's `Application` instance
is destroyed (`App.reset()` in `RunClientRuntime`) before the host loads the freshly
downloaded DLL. This keeps a single SDL window alive at any one time — the host's window
disappears, then the DLL's `InitApp` creates a fresh one. Without this teardown the two
modules' independent `unique_ptr<Application> App` statics would briefly co-exist.

When the client runtime is running from a loaded DLL, `RunClientRuntime` also resets `App`
before returning to the host so SDL windows, renderers, and other frontend resources are
released before `Platform::UnloadModule`. Both embedded and DLL-backed runtime exits call
`ApplicationShutdownHook()` before handing control back to the host; embedding projects use
that hook to stop process-global integrations such as in-process crash handlers before a
runtime module can be unloaded.

### Self-update applies on the next launch (user restart)

A native self-update is **not** applied in the running process. When the updater stages the native
binaries it prints a "please restart" line **on the update screen** (`Updater::AddText` + `_restartPrompt`
in [Client/Updater.cpp](../../../../Source/Client/Updater.cpp)) and holds that screen until the user
closes the client (Escape, which the updater already handles). The runtime then returns `ReloadRequested`
and the host (`PromoteStagedReloadForRestart` in
[Applications/ClientApp.cpp](../../../../Source/Applications/ClientApp.cpp)) promotes the staged runtime
onto the live path (`ApplyStagedBinaryUpdate`) and **exits**. The next launch loads the promoted module
as its single, clean `InitApp`.

The message + hold are gated by `App->IsHeadless()` (a **runtime** check, since `FO_HEADLESS_APP` is an
app-target define that is not set when compiling `ClientLib` where the updater lives): a headless client
has no UI and no user to dismiss the prompt, so it skips the message/hold and the host promotes + exits
immediately.

An in-process reload is avoided because it is unsafe for two independent reasons:

1. **Stale module.** Reloading the **same** `<live>` path after staging the new module: if
   `Platform::UnloadModule` does not bring the previous module's OS refcount to zero (Windows
   `LoadLibrary` path dedup, glibc keeping a `.so` resident), the reload's `LoadModule` returns the
   **still-resident previous module** instead of the freshly-swapped file — so the runtime never
   actually updates. This is reliable, not occasional, on Windows.
2. **Second `InitApp`.** `InitApp`
   ([Frontend/ApplicationInit.cpp](../../../../Source/Frontend/ApplicationInit.cpp)) is guarded by a
   module-static `std::once_flag` + `FO_STRONG_ASSERT(first_call)` and brings up SDL (video device,
   window, audio device + thread). Even if a fresh *module* is mapped (e.g. via a renamed copy),
   running `InitApp` a **second time in the same process** crashes during SDL re-initialization —
   `CreateInternalWindow` fails (`EXCEPTION_ACCESS_VIOLATION`, "window creation failed") and the prior
   App's audio thread faults touching torn-down state. The historical build-hash reload guard *masked*
   this by aborting on the stale module before the second `InitApp` ran.

A fresh launch sidesteps both: the new process loads the promoted runtime as its first and only
`InitApp` in a clean address space, and its compatibility now matches the server, so it syncs resources
and enters the game without staging another update.

> **Installed (writable-root) clients.** After promotion, the host records the writable live DLL in a
> small selector under `<Platform::GetUserDataBase()>/<FO_NICE_NAME>/ClientRuntimeHost/`. On the next
> launch an `INSTALLED` host reads and validates that selector before `InitApp`, then loads the writable
> DLL directly. The frozen install-dir DLL remains the fallback when the selector is absent, malformed,
> names a different runtime, or points to neither a live nor staged file. Portable clients never consult
> this selector.

> **Deployed hosts are frozen.** The host `.exe` is never delivered by the updater (only the runtime
> DLL is). A client built before this fix (one that attempted an in-process same-path reload) cannot be
> fixed in place by any server or DLL update — it needs a one-time manual reinstall of a client carrying
> the fix, after which self-updates work again. Updater protocol generation 2 and host/runtime ABI 3
> form the hard safety boundary: generation-1 clients are rejected before any native module transfer,
> and ABI-2 hosts cannot load an ABI-3 runtime. This prevents a frozen unsafe host from reaching a
> second `InitApp`. The frozen generation-1 runtime shows its existing base-client update instruction;
> generation-2 and newer runtimes use the explicit latest-full-package wording below.

## Host CLI surface

```text
<client-host>                                                           # bundled runtime, default compatibility
<client-host> --ClientLibPath <path>                                    # explicit runtime, default compatibility
<client-host> --ClientLibPath <path> --ClientLibCompatibilityVersion <ver>  # explicit runtime, no embedded fallback if ver != built-in
```

The bundled runtime library name is **derived from the host executable name** at startup via `GetCurrentClientRuntimeLibraryName()` (returns the exe basename without extension; falls back to `FO_DEV_NAME` when `Platform::GetExePath()` cannot resolve). The resolved live path is `GetClientRuntimeLivePath() = <exe_dir>/<library_name>` (extension is appended by `Platform::LoadModule`). Renamed/multi-instance hosts therefore each load their own sibling module (`MyAlt.exe` ↔ `MyAlt.dll`) instead of sharing one — no settings or packaging-time config patching needed. In the build tree, the generated `<ProjectDevName>_ClientLib` target writes its canonical artifact and also copies a host-derived alias (`<client-host>.dll` / `.so` / `.dylib`) so an unpackaged client host can exercise the same loading path as a packaged client.

## Runtime ABI

[Client/ClientRuntimeApi.h](../../../../Source/Client/ClientRuntimeApi.h) is the only contract between host and runtime. Both sides agree on:

- `FO_CLIENT_RUNTIME_HOST_ABI_VERSION = 3` — bumped when the structs change shape or the required host
  lifecycle behavior changes. ABI 3 requires the promote-and-exit policy and rejects ABI-2 hosts that
  may attempt an in-process runtime reload.
- `ClientRuntimeMetadata` — runtime name, build hash, gameplay compatibility version.
- `ClientRuntimeExports` — entry table returned by `FO_QueryClientRuntimeExports(host_abi_version, *exports)`.
- `ClientRuntimeResult` — how the runtime communicates back to the host (`Shutdown`, `ReloadRequested`, `FatalError`).

A runtime that has staged a self-update sets `ResultKind = ReloadRequested` and fills
`RequestedRuntimePath`. Despite the ABI name, the host does not load that module again in the running
process: it promotes the staged file, exits, and lets the next user launch load the updated runtime as
its only `InitApp`.

The runtime stages a new module as `<live>-staging` next to the live module, where `<live>` is the updater's binary output path `Updater::GetRuntimeLivePath()` = `<Updater::_binaryDir>/<runtime_name><ext>` (the full live path including the platform runtime extension, e.g. `<exe_dir>/<runtime-name>.dll` for a portable client, or `<UserWritablePath>/<runtime-name>.dll` for an installed one). After each binary payload is fully downloaded and hash-validated, the updater also makes a best-effort attempt to promote that staged file to the live path immediately; if the live file is locked, the `-staging` file is left in place for the host's startup/exit-time promotion pass. The host promotes via `MakeClientRuntimeStagingPath(runtime_live_path)` → `runtime_live_path` rename: at startup this is the path selected from the exe-dir default, installed-client bootstrap, or explicit CLI; after `ReloadRequested` it is the runtime-supplied `RequestedRuntimePath`. `RequestedRuntimePath` is the post-swap path (`<live>`), not the staging path. The host promotes it and exits; `LoadModule` happens only in the next process.

**Linux module isolation.** The runtime `.so` must stay loadable with `dlopen` from an engine host executable that exports its own engine symbols (`-rdynamic` for stack-trace symbolization). Two build rules keep that true. First, engine runtime modules link with `-Wl,-Bsymbolic` (`AddSharedApplication` in [cmake/helpers/Build.cmake](../../../../BuildTools/cmake/helpers/Build.cmake)), so the module binds global references — the global-data registry, allocator, logging — to its own definitions instead of interposing on the host executable's exported copies; each module keeps private engine state, mirroring the Windows DLL model (without this, the module's `CreateGlobalData` resolves to the host's already-fired copy and the module crashes on its first global-data access). Second, vendored rpmalloc does not force initial-exec TLS on Linux (`(FOnline Patch)` in `ThirdParty/rpmalloc/rpmalloc/rpmalloc.c`): an IE-model TLS relocation makes glibc place the module's entire TLS segment into the limited static TLS surplus at `dlopen`, which fails with `cannot allocate memory in static TLS block`. The host/runtime C ABI keeps allocation ownership module-local (all strings are copied at the boundary), so per-module allocator state is safe.

A matching PDB (Windows-only, named `<live>.pdb`, e.g. `<runtime-name>.dll.pdb`) is staged side-by-side as `<live>.pdb-staging` and usually promotes immediately because PDBs are not held by the loaded runtime module; if it is locked by a debugger or another process, `ApplyStagedBinaryUpdate` retries after the main DLL swap succeeds. The PDB swap is best-effort — failure only degrades stack traces, so it never blocks the runtime swap, while the DLL swap remains backup-rename-rollback atomic. The client-side filter accepts a server file whose basename starts with `<runtime_name>.`, so the DLL (`<runtime-name>.dll`) and its PDB sibling (`<runtime-name>.dll.pdb`) both match and ride the same `UpdateFileTarget::ClientBinaries` channel. **The runtime DLL and its `<live>.pdb` are fetched only together, in binaries mode** (when the DLL is actually being updated) — a client whose DLL is already current does not pull `<live>.pdb` on its own. **The host PDB (`<host_name>.pdb`, e.g. `<host-name>.pdb`) is also delivered, but the client fetches it only to recover a *missing* local copy and never overwrites a present one.** The host exe is frozen and its PDB is build-specific, so the server's host PDB matches only an up-to-date host: an up-to-date client re-downloads a matching PDB, while an older host's matching local PDB is never clobbered (a non-matching server-build PDB is written only when the local one is absent, where the debugger ignores it by GUID). `accept_binaries` is `_binariesMode || CanSelfUpdateNativeModules(...)`, so host-PDB recovery also works on a normal resource-sync connect.

## Updater protocol

Versioned by `FO_UPDATER_VERSION = 2` ([Common/Common.h](../../../../Source/Common/Common.h)). Bump it when
the wire format changes or an older updater/host lifecycle is unsafe to continue. Generation 2 rejects
generation-1 clients before descriptor or binary transfer because their frozen hosts may attempt an
in-process runtime reload. Gameplay compatibility (`Settings.CompatibilityVersion`) is separate and
changes with every build.

### Handshake

| Direction | Field | Type | Purpose |
|-----------|-------|------|---------|
| client → server | `CompatibilityVersion` | `string` | gameplay compatibility |
| client → server | `MetadataVersion` | `string` | baked metadata version; empty while the updater has no resources of its own |
| client → server | `updater_version` | `uint32` | `FO_UPDATER_VERSION` |
| client → server | `binary_target` | `string` | e.g. `Windows-win64`, `Android-arm64` (from `GetCurrentBinaryUpdateTargetName()`) |
| client → server | `in_encrypt_key` | `uint32` | session keys |
| server → client | `compatibility_outdated` | `bool` | gameplay version mismatch |
| server → client | `updater_outdated` | `bool` | `FO_UPDATER_VERSION` mismatch — protocol is unusable |
| server → client | `metadata_outdated` | `bool` | client resources were baked from another revision |
| server → client | `MetadataVersion` | `string` | metadata version currently used by the server |
| server → client | `out_encrypt_key` | `uint32` | session keys |

`updater_outdated == true` is fatal to the connection — the protocol contract has changed and no further messages are valid. `compatibility_outdated == true` only blocks gameplay; the updater can still deliver resources / native modules to bring the client back to current compatibility.

`metadata_outdated == true` means the binaries match but the baked data does not. The server and client must run metadata from one bake because entity payloads address properties by that metadata's registration order. A mismatch is a build or deployment defect, not a supported compatibility mode; see [Metadata version](../../reference/metadata/#metadata-version).

The updater prevents a mismatched client engine from starting:

1. It connects first, reports the version carried by its current packs (empty on a fresh install), and synchronizes every announced file.
2. It re-reads the version from the local packs. Unless it matches the server, the updater returns `UpdaterResult::MetadataMismatch` and no `ClientEngine` is created. An unpackaged development server that distributes no resources has nothing to verify and is skipped.
3. Only after that check is the client constructed and allowed to send its own handshake.

If the server is redeployed between synchronization and the client handshake, the server reports the new mismatch, the client throws `ResourcesOutdatedException`, and the host synchronizes again. An unpackaged client has no updater and therefore reports the mismatch through the normal exception path.

Malformed pre-handshake payloads that fail buffer decoding are treated as invalid handshake data: the server logs a warning with the remote endpoint and hard-disconnects without reporting an exception stack trace. Post-handshake decode failures still go through the normal exception reporting path.

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

Common (gameplay-resource) entries are emitted for every binary target. Per-target binary entries (`UpdateFileTarget::ClientBinaries`) are emitted only for the matching `binary_target` from the handshake. The client then filters binary entries by the current host-derived runtime basename, so `<client-host>.exe` downloads `<client-host>.dll` while `<alternate-host>.exe` downloads `<alternate-host>.dll` even when both report the same CPU/OS target.

### Resumable file transfer

The client drives a single transfer at a time:

```text
client → server: GetUpdateFile  { file_index: uint32, start_offset: uint64 }
server → client: UpdateFileData { update_portion: int32, raw bytes[update_portion] }
```

The server picks `update_portion`, capped by `Network.UpdateFileMaxPortionSize`. The engine default is 1,000,000 bytes in [Settings.inc](../../../../Source/Common/Settings.inc); an embedding project may override it after measuring throughput and per-message memory pressure. The client requests the next portion with `start_offset = bytes_already_written`, so partial transfers resume from disk on reconnect without server-side state.

The updater connection also participates in the shared connection-stage protocol. After `InitData`, a
server may send `NetMessage::HashList` (message id 122) to teach clients strings that were previously
reported as unresolved runtime hashes. The updater consumes that message and records the strings in its
private hash storage before continuing resource or binary transfer; `HashList` is not an update-file
payload and does not change the `GetUpdateFile` / `UpdateFileData` state machine.

Server-side validation (in [Server/UpdaterBackend.cpp](../../../../Source/Server/UpdaterBackend.cpp)):

- `file_index` out of range → `LogType::Warning` + `HardDisconnect`.
- `start_offset > file_size` → `LogType::Warning` + `HardDisconnect`.
- `update_file_max_portion_size <= 0` (misconfiguration) → `LogType::Warning` + `HardDisconnect`.
- Disk-mode read failure → `LogType::Warning` + `HardDisconnect`.
- Disk-mode size drift against the announced descriptor entry → `LogType::Warning` + `HardDisconnect`. With `ServerNetwork.UpdateFilesInMemory = False`, the descriptor is a start-time snapshot while bytes are read on demand; replacing a pack under a live server must not deliver new bytes under the old hash.

Client-side, the `Updater` writes each portion to a `~<filename>` temp file, hashes via streamed `fs_hash_file` ([Essentials/DiskFileSystem.cpp](../../../../Source/Essentials/DiskFileSystem.cpp)) once complete, then atomically renames over the live file (`ReplaceFileSafely`). The updater hash is FNV-1a 64-bit (separate from the engine's wyhash-backed `hashing_ex::hash`, which is reserved for hash-tables and `hstring`); streaming a chunked file produces the same digest as `fs_hash_data` over the full buffer, so server in-memory hashing and client streaming hashing agree by construction. Streaming the hash means even multi-GB resource packs never get fully buffered in RAM on either side.

To avoid rehashing existing packs on every startup (the hashing cost dominates the updater's "is this file already current?" pass for multi-GB resource packs), the disk-side hash check goes through `Updater::IsDiskFileHashMatch`, which caches the result in `CacheStorage` under the configured `Baking.CacheResources` directory from [Settings.inc](../../../../Source/Common/Settings.inc). The key is `<basename>-<path-hash>.hash`, where `<path-hash>` is the 16-digit lowercase hexadecimal result of `hashing::hash<string_view>` over the full path string passed to the check. For example, an `Embedded.zip` entry becomes `Embedded.zip-0123456789abcdef.hash`; the exact suffix depends on its path. Including only the digest, rather than the absolute path text, keeps the filename valid on Windows while preventing same-basename resources in different directories from sharing a cache entry. The cached value stores `(size, mtime, hash)`; a size or mtime change invalidates it, and deleting the cache entry causes a transparent re-hash on the next updater pass.

There are no backward-compatible fallback paths. The previous "session-state file index + portion counter" protocol was removed when `FO_UPDATER_VERSION` was introduced; clients and servers must agree on the version.

## Server-side: `UpdaterBackend`

[Server/UpdaterBackend.h](../../../../Source/Server/UpdaterBackend.h) is owned by `ServerEngine` as a `unique_ptr`. When `_updaterBackend` is null (unpackaged dev server) the server rejects `GetUpdateFile` with `HardDisconnect` — there is nothing to serve.

Current native interface (an internal engine surface, not a stable public API):

```cpp
void LoadFromClientResources(const GlobalSettings& settings);
void ProcessUpdateFile(ptr<Player> player, int32_t update_file_max_portion_size);
auto GetUpdateDescriptor(string_view binary_target_name) const -> const_span<uint8_t>;
```

The descriptor is exposed as a borrowed `const_span<uint8_t>` view over storage
owned by the backend; callers must not retain it beyond that storage's lifetime.

- `LoadFromClientResources` walks `Settings.ClientResources`, picks every pack listed in `Settings.ClientResourceEntries` (excluding `Embedded`), then enumerates `Settings.PlatformBinaries/<target>/` for per-target binaries (default `PlatformBinaries/`, sibling of `Resources/` in the package layout).
- Entries are stored as `UpdateFileData { InMemory, MemoryData?, DiskPath?, Size, Hash }`. Memory mode keeps the whole pack in RAM for the lifetime of the server. Disk mode keeps only `DiskPath`, `Size`, and the streamed `Hash`; portions are read on demand by `ReadUpdateFilePortion(...)`.
- Descriptors are cached per `binary_target_name`. Common-resource entries are merged into every per-target descriptor; targets without specific binaries fall back to the common-only descriptor.
- `VerifyClientResourcesMetadata` mounts the client packs and compares their metadata version with the version loaded by the server. Because the server runs `Settings.ServerResources` but distributes `Settings.ClientResources`, deploying only one side now fails startup with `UpdaterException` that names both versions.

## Settings

| Setting | Where | Purpose |
|---------|-------|---------|
| `Network.UpdateFileMaxPortionSize` | top-level | Maximum bytes per `UpdateFileData` response. Drives both transfer throughput and per-message memory pressure. Engine default: 1,000,000 bytes. |
| `ServerNetwork.UpdateFilesInMemory` | top-level + `[SubConfig]` | `True` keeps every packaged update file in RAM (low CPU under load). `False` serves from disk on demand (low RAM, more I/O). Engine default: `False`; each project chooses overrides for its deployment profiles. |
| `Network.ForceMetadataVersion` | top-level | Testing only: overrides the metadata version reported by the client, allowing a mismatch to be simulated without a second bake. Keep empty in shipped configurations. |
| `Baking.PlatformBinaries` | top-level | Directory the server reads per-target client runtime libraries from, and the packager writes them to. Default `PlatformBinaries`, resolved relative to the server's working directory / package root. |
| `Client.UserWritablePath` | client | Writable data root for an **installed** client whose install dir is read-only. Empty (default) = **portable** (cache/logs/updates next to the exe). `*` = the per-OS user data dir. Otherwise an explicit absolute path. See the section below. |

There is no auto-detection of memory vs disk mode in C++. Choose explicitly per environment.

## Installed vs portable writable data

A **portable** build writes its cache, log, and self-update files next to the exe — fine for a zip the
user unpacks anywhere. An **installed** build (MSI in `Program Files`, a package under `/usr/...`) sits
in a read-only directory, so those writes must go to a per-user writable location instead.

`Client.UserWritablePath` selects the model, resolved at startup by `ResolveUserWritablePath(settings)` (`Source/Frontend/ApplicationInit.cpp`, called from `LoadAppSettings`):

- **empty → portable** (default): writable paths stay relative to the exe / working dir (unchanged behaviour).
- **`*` → per-OS user data dir** (`Platform::GetUserDataBase()` via env, no SDL/shell32 dependency): Windows `%LOCALAPPDATA%`, macOS `~/Library/Application Support`, Linux `$XDG_DATA_HOME` or `~/.local/share`, then `/<Common.GameName>`.
- **explicit path** → that absolute writable root.

Resolution is idempotent, creates the directory + the `Cache`/`<ClientResources>` subdirs, and is
**fail-safe**: if the dir can't be determined or created it logs a warning and reverts to portable, so a
bad install config never bricks startup.

What moves to the writable root (via the free path helper `fs_make_writable_path(UserWritablePath, relative)`
in `DiskFileSystem.cpp`): the **cache** (`CacheStorage` in `ApplicationInit`/`Client`/`Updater` — login keys, native
secure storage, local config), the **log** file (re-pointed after settings load), **self-update resource
patches** — the updater writes them under `<root>/<ClientResources>` and layers that dir on top of the
read-only install-dir base as a higher-priority resource source (`Updater.cpp`, `Client.cpp`), so the base
resources are read from the install dir and patches override from the user dir — and the **self-updated native
runtime** (see below).

**Native binary self-update for installed builds writes the runtime into the writable root**
(`Updater.cpp`). The updater's binary output dir (`Updater::_binaryDir`) is `<root>` for an installed client
and the exe dir for a portable one, so a self-updated runtime lands at `<root>/<runtime_name><ext>` (mirroring
the install-dir layout, `<exe_dir>/<runtime_name><ext>`) alongside its `-staging` and `<...>.pdb` siblings. It
is **not** gated off — both portable and installed clients self-update on every platform where
`CanSelfUpdateNativeModules()` is true.

Because the host resolves and loads the runtime DLL *before* settings (so it cannot compute `<root>` itself —
`Common.GameName` is only known after `InitApp`), the runtime returns the writable live path through
`ClientRuntimeResult::RequestedRuntimePath`. The host promotes that path, validates that it is absolute and
has the current executable-derived runtime filename, writes it to the installed-client bootstrap selector,
through `GetInstalledClientRuntimeBootstrapPath()` and `WriteClientRuntimeBootstrapTarget()`, and exits; it never loads it again in the same process. On the next launch the `INSTALLED` host reads the
selector from `<Platform::GetUserDataBase()>/<FO_NICE_NAME>/ClientRuntimeHost/<runtime><ext>.path` before
settings, accepts it only when the live file or its `-staging` sibling exists, and loads that runtime directly.
Missing, oversized, relative, newline-containing, wrong-basename, and stale selectors fall back to the frozen
install-dir runtime. `--ClientLibPath` remains the final explicit override. Portable clients update their
exe-dir sibling runtime and neither write nor read the installed selector.

**Trigger:** the installer drops an `INSTALLED` file next to the exe; when `Client.UserWritablePath`
is empty and that marker is present, the client switches to `*` automatically. The portable zip has no
marker. The MSI packager adds the marker to the MSI payload only (`package.py::make_wix_installer`, added
then removed around `createmsi` so the sibling Raw/Zip portable artifacts stay portable).

## Packaging

[package.py](../../../../BuildTools/package.py) does both halves:

- **Client packages** include the host executable and the matching runtime library renamed to the same basename next to it (for example, `<client-host>.exe` and `<client-host>.dll`). The host derives the library name from its own executable basename at startup, so no config patching is required to point one at the other.
- **Server packages** also stage every available client runtime library under `<Settings.PlatformBinaries>/<binary_target>/<output_name><runtime_ext>` (default `PlatformBinaries/`, sibling of the client-resources dir in the package layout) so a different-platform client connecting to this server can self-update its native modules.
- **Windows Client packages with the `Wix` pack** build a required MSI from the already-staged Raw client payload. `package.py::make_wix_installer` writes a temporary WiX JSON config, adds the `INSTALLED` marker only while the MSI payload is generated, and registers the product URI scheme through HKCU registry entries. A missing WiX/wixl toolset or a generator failure fails the package instead of silently publishing only the Raw/Zip siblings.
- **PDBs for Windows runtime DLLs** are shipped under `<runtime_dll>.pdb` (for example, `<client-host>.dll.pdb`) — both next to the bundled client DLL and inside every server-staged `PlatformBinaries/Windows-*` payload. The host exe keeps its own `<host_name>.pdb` so the two namespaces never collide. `package.py` patches the CodeView (`RSDS`) record in place to point at the new PDB filename — for the renamed runtime DLL (`copy_runtime_pdb`) **and** for the host exe (`<name>.pdb`, patched at the `copy_pdb` call site) — so DbgHelp / `backward-cpp` resolve symbols automatically without relying on the build-machine path baked into the binary. Missing PDB inputs or failed RSDS patches `assert` immediately during packaging — symbol gaps are never silently tolerated.
- **The host PDB is delivered for missing-copy recovery only.** `package_all_client_runtime_update_payloads` stages the host's own `<name>.pdb` alongside the runtime DLL and its `<name>.dll.pdb` under `PlatformBinaries/<target>/`. The host exe is frozen and never delivered, so its PDB is build-specific and the server only carries its *current* build's host PDB. The client therefore fetches the host PDB **only when its local copy is missing** and **never overwrites a present one** (`Updater.cpp` skips the `<runtime_local_prefix>.pdb` entry when the file already exists, in either resource-sync or binaries mode). An up-to-date host re-downloads a matching PDB; an older host's matching local PDB stays untouched (and only if the player deleted it does the client write the current, non-matching one, which the debugger ignores by GUID). This recovers a deleted host PDB without ever clobbering a good one — the clobber that an unconditional host-PDB delivery used to cause for self-updated clients (frozen old host + newer server host PDB).

Both the bundled runtime library in client packages and the runtime libraries staged for server-side binary updates go through the same package-time patching as ordinary executables: embedded resources, internal config, and packaged mark are written by `package.py`. Variant-specific config is applied to the runtime payload that actually runs the game; for example the Windows OpenGL runtime receives `ForceOpenGL=1`. The embedded-resource zip is produced with pinned entry timestamps and permissions (`make_embedded_pack`), so the bundled-client copy of a runtime and the matching `<Baking.PlatformBinaries>/<target>/<output_name><ext>` payload remain byte-identical across separate Server/Client package runs.

Client resource zips are written with the same stable entry metadata and sorted normalized paths. This matters because the baker touches unchanged output files during incremental runs; package output must ignore those mtimes so a content-identical repack keeps the same FNV hash in the updater descriptor and does not force clients to redownload every pack. [test_package_zip_determinism.py](../../../../BuildTools/tests/test_package_zip_determinism.py) covers the mtime/order invariant.

The internal config patch area is generated from the CMake `FO_INTERNAL_CONFIG_CAPACITY` option, next to `FO_EMBEDDED_DATA_CAPACITY`; `package.py` discovers the actual reserved size from the generated binary markers before writing config data.

Naming convention from `build_runtime_update_target_name` in `BuildTools/package.py`:
- `Windows-win64`, `Linux-x64`, `Linux-arm64`, `macOS-arm64`, `Android-arm64`, etc.
- Profiling variants get the `_Profiling` suffix in the staged file name.
- The Windows OpenGL variant (`OGL`) is staged separately and patches `ForceOpenGL=1`.
- Binary outputs tagged by `FO_BINARY_OUTPUT_POSTFIX` (e.g. `Client-Linux-x64-Steam`, `Client-Windows-win64-Steam`) are staged under the same `PlatformBinaries/<target>/` directory as the default variant, but `package_all_client_runtime_update_payloads` appends `_<postfix>` to every staged payload name (`<runtime-name>_Steam.so`, `<runtime-name>_Headless_Steam.so`, etc.) so variants do not clobber each other. `extract_binary_entry_postfix` strips either the packager order (`-Profiling_X-Debug`) or the actual CMake multi-config name (`-Debug_Profiling_X`), as well as the named release and sanitizer configurations, before reading an optional postfix. Unrelated or malformed `Client-*` directories are skipped instead of aborting an otherwise valid package. The matching package declaration must put the same value on that `BINARY` entry with `POSTFIX <value>`; there is no package-wide fallback from `FO_BINARY_OUTPUT_POSTFIX`. The client-side packager mirrors the suffix in `bin_out_name`, so the patched `PACKAGED_BUILD_NAME` lines up with the server-side payload name used by `Updater.cpp::remap_runtime_name` (`runtime_server_prefix = GetPackagedRuntimeName()`). `win32-win7` and `win64-win7` normalize to updater targets `Windows-win32` and `Windows-win64`; an explicit postfix is what keeps a legacy payload distinct from the regular build.

## Embedding-project practices

Treat updater configuration as a measured release decision, not a set of values
to copy from another game. Keep `Network.UpdateFileMaxPortionSize` and
`ServerNetwork.UpdateFilesInMemory` explicit in each deployment profile. Measure
transfer throughput, peak resident memory, concurrent update load, disk latency,
and reconnect behavior before changing either value. A production profile may
choose an in-memory payload while development and staging remain disk-backed, but
the release record must state which path was exercised.

Keep portable and installed clients as separate acceptance lanes. An installed
lane must cover the first native update, selector persistence, next-launch loading,
corrupt and stale selector fallback, and a second update while the current runtime
is locked. A portable lane must prove that no selector is read or written and that
the sibling runtime remains self-contained. Settings present in a project config
are not evidence that these paths work; executable package and updater tests are.

Variant identity must remain aligned across the package declaration, generated
binary name, `POSTFIX`, `PACKAGED_BUILD_NAME`, server payload name, and client-side
runtime remapping. Exercise every distributed postfix independently and verify
that packaging one variant cannot overwrite another. Keep Web, Android, and iOS
store or manual replacement workflows separate from the Windows, Linux, and
macOS native-module self-update lane.

Project-owned release tests should cover at least:

- an outdated runtime reaching the restart prompt, promotion, exit, and successful next launch;
- resource and native-binary corruption, a missing resource pack, and a server missing the requested platform payload;
- interrupted transfer resume, stale staging recovery, repeated outdated launches, and an invalid installed selector;
- distinct postfix variants and Windows PDB staging when those outputs are distributed;
- the exact package profiles and server payload directories used for release, not only a development build tree.

The [internal external-project evidence ledger](https://github.com/cvet/fonline/blob/master/Docs/generated/external-project-evidence/index.md)
records which of these practices are observed in maintained embedding projects.
That evidence is comparative and non-normative: this page and Engine source remain
the reusable contract.

### Signing and trust boundary

A downloaded native runtime is executable code. Transport encryption, descriptor
hashes, and atomic promotion detect transfer damage and support recovery; they do
not establish publisher identity. The embedding project owns authenticated
distribution, code signing, timestamping, key protection, revocation, and incident
response. Follow [Security and Secrets](../../how-to/release/security-and-secrets.md)
and [Packaging and Release](../../how-to/release/packaging.md) for that boundary.

Sign the final patched host and runtime artifacts before archive or installer
creation. Verify signatures and timestamps after signing, then bind descriptor
hashes to those exact bytes. Acceptance must start a packaged server and prove
that its `PlatformBinaries/<target>/` payload is byte-for-byte the artifact that
was approved. Never sign one file and serve a later repackaged or config-patched
copy under the same release identity.

### Release acceptance matrix

| Lane | Required evidence |
|------|-------------------|
| Native portable client | sibling runtime load, resource update, native update, restart, next-launch execution, resume, rollback/staging recovery |
| Native installed client | read-only install, writable resource overlay, selector validation and persistence, native update, corrupt-selector fallback, second update |
| Variant/postfix | unique packaged and server-staged names, matching runtime remap, no cross-variant clobber |
| Windows symbols | host PDB recovery, runtime PDB update, patched CodeView names, signature verification after final patching |
| Web / Android / iOS | resource update where supported, explicit unsupported-native-update result, store/manual replacement instructions |
| Packaged server | exact release profile, complete resource list, every supported target payload, memory/disk mode, missing-payload failure |
| Compatibility break | updater generation and runtime ABI rejection text, full-client reinstall path, rollback and support instructions |

## Lifecycle

```text
<client-host> main
    ├── ResolveRequestedClientRuntime(argc, argv)        # Path + CompatibilityVersion + ExplicitPath
    │
    ├── RunClientFromLibrary(argc, argv, requested, *)   # CASE 2: bundled runtime exists
    │     ├── ApplyStagedBinaryUpdate(requested.Path)    # promote <requested>-staging (no-op when missing)
    │     ├── Platform::LoadModule + FO_QueryClientRuntimeExports
    │     ├── Validate exports + metadata
    │     ├── exports.Run(argc, argv, &result)           # DLL drives RunClientRuntime:
    │     │     ├── single Updater (UI) connects to the server. The connect result picks the mode:
    │     │     │     ├── Success         → resources mode → sync ClientResources, finish ResourcesReady
    │     │     │     └── CompatibilityOutdated:
    │     │     │             ├── if !CanSelfUpdate    → finish PlatformUnsupported, caller shows store msg
    │     │     │             └── else                  → binaries mode → write ClientBinaries to
    │     │     │                                          `<live>-staging`, try immediate promote, or verify `<live>`,
    │     │     │                                          finish BinariesStaged
    │     │     ├── On BinariesStaged: set ResultKind = ReloadRequested, RequestedRuntimePath
    │     │     ├── On any other non-success result: ShowUpdaterFailure(result) and quit
    │     │     └── unload of DLL (scope_exit) frees the loaded module
    │     └── If ResultKind == ReloadRequested: PromoteStagedReloadForRestart
    │           └── ApplyStagedBinaryUpdate(requested path), then exit
    │
    └── If LoadModule failed (CASE 1: no DLL yet, packaged install):
          if !CanFallbackToEmbeddedClient(requested): return false
          RunEmbeddedClient(argc, argv, *)               # host-module RunClientRuntime
          (same single-Updater flow as the DLL; host module's App.reset() runs after
           ReloadRequested before the host promotes the runtime and exits)
          if ResultKind == ReloadRequested → promote staged runtime, then exit
```

A single `Updater` instance handles both gameplay-resources and native-binaries syncs.
It picks the mode internally based on the server's compatibility verdict on connect — no
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
| Updater protocol mismatch | server log `Connected client X has outdated updater version Y`; generation-1 client message box `Client updater outdated, please update the base client`; generation-2+ wording `Client updater is incompatible with this server. Please install the latest full client package.` |
| Gameplay version mismatch on a self-update platform | resource updater finishes silently with `WasCompatibilityOutdated() == true`; the runtime opens the binary updater UI, stages the current module, shows the restart prompt, and returns `ReloadRequested`; the host promotes the staged runtime and exits |
| Gameplay version mismatch on Web / iOS / Android | message box `Client outdated, please update via your app store`, then quit (no in-process self-update on these platforms) |
| Wrong file index / offset | server log `Wrong file index N, from host '...'` / `Wrong update file offset O, file index N, client host '...'` (both at `LogType::Warning`), client gets disconnected |
| Client data does not match server data | server log `Connected client X runs metadata version A while the server runs B`; updater log names the local version, server version, and resource directory. Find which directory came from a different bake |
| Server distributes resources it does not run | server startup fails with `Distributed client resources were baked apart from the server resources`, naming both resource directories and versions |
| Resources predate the current metadata format | metadata-header startup failure: `does not start with the metadata file marker`, `file version does not match the engine`, or `carries no version`; run a full rebake |
| Server has no native update for this target | message box `Server doesn't provide a native client update for binary target <target>` |
| Stale staging file | `<live>-staging` survived a previous failed swap; the next client-host startup promotes it via `ApplyStagedBinaryUpdate` before loading the runtime |
| Linux host logs `LoadModule failed` for a present, valid runtime `.so`, then `trying embedded fallback` on every launch | `dlopen` rejected the module. Two engine build rules must hold (see "Linux module isolation" above): the module is linked with `-Wl,-Bsymbolic` (`AddSharedApplication`), and no vendored code forces initial-exec TLS on Linux — an IE-model TLS relocation fails `dlopen` with `cannot allocate memory in static TLS block` (diagnose with a standalone `dlopen` of the `.so`, e.g. via `python3 -c "import ctypes; ctypes.CDLL('./<runtime>.so')"`). A silently-engaged embedded fallback makes a native self-update loop: the downloaded `.so` is promoted on disk but never executed |
| Self-update downloads and then waits on the update screen | This is the expected native flow. Close the client after the restart prompt; the host promotes the staged runtime and exits, and the next user launch starts the updated runtime with one clean `InitApp`. Hosts predating this policy are rejected by updater generation 2 / runtime ABI 3 and require the latest full client package instead of attempting the unsafe second initialization |
| Stack trace shows raw addresses for the new runtime DLL | After a binary self-update the renamed `<live>.dll`'s CodeView entry must reference its sibling `<live>.dll.pdb`. If `package.py` skipped the RSDS patch (it will assert when this happens), `dbghelp`/`backward-cpp` cannot find the PDB and frames in the runtime resolve to addresses only |
| Stack trace shows raw addresses for **host** (`<host>.exe`) frames after a self-update, while runtime-DLL frames resolve | The on-disk `<host_name>.pdb` doesn't match the frozen exe (CodeView GUID differs) — typically a leftover from an old updater build that clobbered the matching host PDB with a newer server-build one. The current updater never overwrites a present host PDB and fetches one only when the local copy is missing, so the fix is to delete the mismatched `<host_name>.pdb`: an up-to-date host then re-downloads the matching one; otherwise restore the host PDB shipped with that exe build (matching CodeView GUID). A mis-walked stack through unsymbolized host frames can also surface bogus top frames (e.g. attributing the fault to an unrelated system DLL) |

Local validation steps:

1. Build the embedding project's generated unit-test target and run it. [Tests/Test_ClientRuntimeApi.cpp](../../../../Source/Tests/Test_ClientRuntimeApi.cpp) exercises the ABI surface plus installed-runtime selector round-trip, validation, live selection, staged recovery, and fallback; [Tests/Test_DiskFileSystem.cpp](../../../../Source/Tests/Test_DiskFileSystem.cpp) covers `fs_hash_file` parity with `fs_hash_data` and `fs_make_writable_path`; [Tests/Test_Platform.cpp](../../../../Source/Tests/Test_Platform.cpp) covers `Platform::GetUserDataBase`; [Tests/Test_Settings.cpp](../../../../Source/Tests/Test_Settings.cpp) covers `UpdateFilesInMemory` sub-config inheritance and `ResolveUserWritablePath` fail-safe/creation behavior.
2. Build the generated client-host target; on native host/runtime platforms its dependency also builds the generated client-runtime target. Confirm the client output directory contains the host plus the host-derived runtime alias (`<client-host>.exe` + `<client-host>.dll` on Windows, `<client-host>` + `<client-host>.so` on Linux). Build the runtime target explicitly when validating it in isolation.
3. Launch `<client-host>` with the bundled runtime present → normal startup (Case 2 happy path: load DLL, resource updater finishes, game starts).
4. Launch `<client-host> --ClientLibPath <path>` with a valid alternate runtime → host routes through the loaded library.
5. Launch `<client-host> --ClientLibPath <path> --ClientLibCompatibilityVersion <other>` and remove the runtime → host fails (no fallback).
6. Point `--ClientLibPath` to an invalid path, no `--ClientLibCompatibilityVersion` → host falls back to embedded client (Case 1).
7. Build one project-owned packaged-server profile and confirm `<Settings.PlatformBinaries>/<target>/<name><ext>` (default `PlatformBinaries/`, sibling of the client-resources dir in the package layout) contains the per-target runtime libraries and that the `ClientResources` pack list contains the resource zips.
8. Interrupt a client mid-download (kill the network) and reconnect — the next `GetUpdateFile` resumes from the temp-file size, no full re-download.
9. Force a Case 2 → restart: package a client against an older `FO_COMPATIBILITY_VERSION`, point it at a server with a newer one, run. The resource updater UI should appear briefly, then the binary updater UI takes over (UI/SplashPic identical). Close the client after the restart prompt; the host renames `<live>-staging` over `<live>` and exits without loading it. The next launch must load the promoted runtime in a fresh process and reach the game.
10. Crash recovery: kill the host while the binary updater UI is mid-download. Restart `<client-host>`. `ApplyStagedBinaryUpdate` runs at the start of `RunClientFromLibrary`; if `<live>-staging` is fully written it gets promoted, otherwise the runtime's resume logic completes the download in a normal updater session.
11. Installed-layout smoke: place an `INSTALLED` marker next to the client executable (or build the Windows `Wix` package), leave `Client.UserWritablePath` empty, and launch. The resolved writable root should be the per-OS user-data dir plus `Common.GameName`; cache/log/resource overlay writes should go there, while the install-dir resources remain read-only inputs. Force a native update, close at the restart prompt, and launch again: the host should log `selected installed runtime ... from bootstrap ...`, load the writable-root runtime directly, and not show the same update prompt again. Delete or corrupt the selector and confirm the host safely falls back to the install-dir runtime.

## See Also

- [Packaging and Release](../../how-to/release/packaging.md) for release payload, signing, compatibility, rollout, and rollback gates.
- [Build Workflow](../../how-to/build/) for reusable build and package entry points.
- [Engine Architecture](../architecture/) for engine, application, and build ownership.
- [Native and AngelScript Debugging](../../troubleshooting/debugging.md) for debugger setup; the host vs runtime split affects which binary the debugger should attach to.
- The embedding project's release documentation for product-specific package profiles and alternative distribution channels.
