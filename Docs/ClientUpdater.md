# Client Runtime Split and Updater

> Engine-owned documentation. Source paths under `../` are relative to the FOnline engine root.

The native client ships as two artifacts:

- `LF_Client.exe` â€” thin host application built from [../Source/Applications/ClientApp.cpp](../Source/Applications/ClientApp.cpp). Stays compatible across runtime versions.
- sibling runtime library (`LF_Client.dll` in a Windows build tree, `.so` / `.dylib` on Linux / macOS) â€” loadable runtime built by the `LF_ClientLib` CMake target from [../Source/Applications/ClientLib.cpp](../Source/Applications/ClientLib.cpp). Contains the gameplay client engine.

The host loads the runtime through a stable C ABI ([../Source/Client/ClientRuntimeApi.h](../Source/Client/ClientRuntimeApi.h)) and falls back to the embedded client linked into `LF_Client.exe` if loading fails.

On native host/runtime platforms the runnable `LF_Client` target depends on
`LF_ClientLib`, and the runtime target's post-build step copies its output to
the host-derived sibling name. Building `LF_Client` therefore refreshes both
artifacts; `LF_ClientLib` remains independently buildable when only the runtime
module is needed. The headless host/runtime targets use the same dependency.

**Platform support.** The host + runtime split is built only on Windows, Linux and macOS â€” that is the set of platforms where `CanSelfUpdateNativeModules()` returns `true` and where the static_assert at the top of `ClientLib.cpp` accepts the build. Web, iOS and Android ship a single self-contained `LF_Client` binary instead: the engine code is statically linked into the executable and the runtime-loading branch in `RunEmbeddedOrLoadedClient()` is never taken. The CMake gate that enforces this lives in [../BuildTools/cmake/stages/Applications.cmake](../BuildTools/cmake/stages/Applications.cmake) (`if(FO_WINDOWS OR FO_LINUX OR FO_MAC)`), and Android additionally takes the `FO_BUILD_LIBRARY` branch which produces only the shared `LF_Client` artifact required by the SDL Android Java loader.

The updater protocol is the same machinery used to deliver gameplay resources, but versioned independently from gameplay compatibility so a host released today can ingest tomorrow's runtime module without a host-side rebuild.


## Managed runtime resource ownership

Mono and the native interop shims are linked into each managed application. The managed class-library payload, including `System.Private.CoreLib.dll`, is data: the Managed baker writes it under `ManagedRuntime/` in the managed resource pack, and normal resource packaging delivers it with the game assemblies. Only CoreLib and the class libraries the pack's assemblies reach by reference are written ([BakingPipeline.md](BakingPipeline.md#managed-runtime-payload-selection)). An embedding project can assign that baker to a pack such as `Scripts`, so the payload lives in `Scripts.fores`; it is not an installation-level companion directory. The payload is nevertheless target-platform-specific: CoreLib's Windows build imports Windows interop while Unix, Android, and browser builds select their respective implementations, and the OS-variant class libraries (`System.Net.Http`, `System.Console`, ...) are built for the target in the same way.

Before Mono initialization, the managed backend restores those resource files atomically into the writable content-addressed cache at `Cache/ManagedRuntime/<content-hash>/` and configures Mono from that directory. The resource payload wins whenever it exists. A filtered `ManagedRuntime/` beside an executable is only the fallback used by unpackaged applications and build tools. Because the payload wins, an unpackaged client of another architecture than the baker host runs the host's CoreLib: a Windows x86 client started against a bake made on x64 reads `IntPtr`-sized data with the 64-bit stride that CoreLib compiled in, and dies in its first reflection call with a null method handle. To run such a client unpackaged, give it a copy of the bake whose `Scripts/ManagedRuntime/` is replaced by the client's own `ManagedRuntime/`, which is what packaging does.

Updater targets therefore keep their ordinary platform/architecture names (`Windows-win64`, `Linux-x64`, `Web-wasm`, and so on). There is no managed-runtime target suffix or identity sidecar. Managed assembly directories use the ordinary resource-role suffixes (`Assemblies-server`, `Assemblies-client`, and `Assemblies-mapper`), and packaging applies those suffixes to directory components as well as filenames. During packaging, every Client resource pack therefore retains only `Assemblies/Assemblies-client/` and is rebuilt with the `ManagedRuntime` payload from its own binary target; Server and Mapper assemblies never enter the client artifact. A Server package stages those rebuilt packs at `PlatformBinaries/<target>/<pack>.fores`; when several native build variants share a target, the least-qualified entry (normally default Release) supplies this single target-wide pack, because independent builds can produce byte-distinct CoreLib files from the same source revision. `UpdaterBackend` tags configured pack names there as `ClientResources` and replaces the same-named common entry in that target's descriptor. Managed class-library changes still travel through the resource updater, while a native Mono or interop-shim change travels in the native runtime module and follows the existing native compatibility/restart protocol where native self-update is supported.

Binary output postfixes are parsed on full flag boundaries: a custom postfix such as `Debug_Profiling_Total` remains a postfix and does not change the profiling variant.

## Server-side updater backend

The client updater is served by the authoritative server runtime. `ServerEngine` wires an `UpdaterBackend` from `Source/Server/UpdaterBackend.*` during server startup when client packs/resources are prepared. The backend scans client resources and native runtime artifacts, builds target-specific update descriptors, and answers file-portion requests with `NetMessage::UpdateFileData`.

Runtime ownership is split deliberately:

- [ServerRuntime.md](ServerRuntime.md) documents where `UpdaterBackend` is hosted and how it fits into server startup/connection processing.
- This page documents the client host/runtime ABI, staging/reload flow, compatibility checks, and updater protocol behavior visible to the client.

Keep long protocol and host-runtime details here; keep server lifecycle and manager ownership in [ServerRuntime.md](ServerRuntime.md).

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
- `Source/Common/Common.h`
- `Source/Common/Settings.inc`
- `Source/Essentials/DiskFileSystem.h`
- `Source/Essentials/DiskFileSystem.cpp`
- `Source/Essentials/Platform.h`
- `Source/Essentials/Platform.cpp`
- `BuildTools/cmake/stages/Applications.cmake`
- `BuildTools/package.py`
- `BuildTools/managed_runtime_payload.py`
- `BuildTools/msicreator/createmsi.py`
- `BuildTools/tests/test_package_zip_helpers.py`
- `BuildTools/tests/test_managed_runtime_packaging.py`
- `BuildTools/tests/test_managed_runtime_payload.py`
- `BuildTools/tests/test_package_windows_arch_variants.py`
- `Source/Scripting/Managed/ManagedRuntime.h`
- `Source/Scripting/Managed/ManagedRuntime.cpp`
- `Source/Tests/Test_ClientRuntimeApi.cpp`
- `Source/Tests/Test_DiskFileSystem.cpp`
- `Source/Tests/Test_Platform.cpp`
- `Source/Tests/Test_Settings.cpp`

## Two-layer client startup

The host tries to load the bundled runtime DLL first on self-update platforms; the **embedded** engine
(statically linked into the host) is the fallback when no sibling DLL is present or it fails to load.
This is uniform across the regular and headless clients — a standalone headless client with no sibling
DLL simply lands on the embedded fallback. Setting `Client.ForceEmbeddedRuntime` (read from the command
line at host startup) forces the embedded path and skips the implicit bundled-DLL load; an explicit
`--ClientLibPath` still loads a DLL. Whichever module ends up running the game (loaded DLL or embedded
host) drives a uniform two-stage updater UI:

```
LF_Client.exe (host)
    â”‚
    â”‚  1. Resolve runtime path (`GetClientRuntimeLivePath()` from current exe name; an installed
    â”‚     client may select a persisted per-user runtime bootstrap; --ClientLibPath overrides both)
    â”‚  2. ApplyStagedBinaryUpdate(<runtime>) â€” promote pending `<runtime>-staging` over `<runtime>`
    â”‚     (also recovers a crashed-mid-update install on first boot)
    â”‚  3. platform::load_module(<runtime>) â†’ FO_QueryClientRuntimeExports(...)
    â”‚  4. Validate ClientRuntimeExports.Metadata (ABI; compatibility only when explicitly requested)
    â”‚
    â–¼
<live runtime module>                 â”€â”€â”€ the running module (loaded DLL by default; host module on fallback / ForceEmbeddedRuntime)
    â”‚
    â”‚  RunClientRuntime: InitApp â†’ resource Updater (UI) â†’ ClientEngine â†’ MainLoop
    â”‚  If resource updater reports compat outdated and platform supports self-update:
    â”‚     stage 2: binary Updater (UI) writes or verifies the module at `<runtime>-staging` / `<runtime>`
    â”‚     return ClientRuntimeResult { ReloadRequested, RequestedRuntimePath = <runtime> }
    â”‚
    â””â”€â–º returns ClientRuntimeResult (Shutdown / ReloadRequested / FatalError)

No sibling DLL / LoadModule fails, or ForceEmbeddedRuntime is set:    â”€â”€â”€ embedded fallback
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
and headless clients. The bundled path itself (`GetClientRuntimeLivePath()`, beside the executable) is resolved under the same
`CanSelfUpdateNativeModules` condition: an Android or iOS app has no executable path, and resolving one there
aborted every mobile client with `Executable path could not be resolved` before its first frame. `Client.ForceEmbeddedRuntime` is honored from the command line
(`--Client.ForceEmbeddedRuntime`) because the host picks the runtime before settings are otherwise resolved;
a SubConfig/config-only value does not reach this pre-init decision, so launch profiles that must force
embedded on a standalone client pass it on the command line.

Because the regular client loads the `<live>` DLL into its own process, it cannot safely reload that
same path after staging a new module. The host promotes the file and exits so the next process performs
the only post-update load.

The implicit bundled-DLL load intentionally does **not** require the DLL's gameplay compatibility string
to match the host executable's built-in string. The executable is frozen in deployed installs, while the
runtime DLL is the self-updated module; tying the bundled DLL to the old host compatibility would reject
the freshly downloaded runtime and incorrectly start the embedded updater. `--ClientLibCompatibilityVersion
<version>` is the opt-in strict mode for tests and explicit host/runtime probes: when it is passed and
differs from the host's compatibility, embedded fallback is refused rather than silently downgrading to
host code.

Startup/runtime handoff diagnostics go to the normal `<host>.log` through the regular `logging::write` path.
The host brings up engine global data (`global_data::create()` in `main`) and opens that log fresh up front
(`logging::to_file(GetExeLogFileName(), false)`) — the host runs first, so it truncates. It then keeps its handle
open across the loaded-DLL call instead of closing before the handoff: `logging::to_file` opens the file without
an exclusive lock (the platform default —
MSVC `std::ofstream` is deny-none, POSIX has no mandatory open lock), and every log write seeks to end of
file first (`write_sync`). The host EXE and the runtime DLL are two engine
modules in one process, each carrying its own copy of the engine global data, so they cannot share one
`std::ofstream`, but with shared access both can hold the same file open and the seek-to-end keeps each
module's writes after whatever the other appended — so the host's post-handoff lines land *after* the
DLL's whole session rather than overwriting it. Client runtimes pass `AppInitFlags::AppendLogFile` into
`InitApp` (which resolves the same `GetExeLogFileName()`), so each DLL/embedded `InitApp` appends to the
shared file instead of truncating the host's lines. The DLL's
`FO_QueryClientRuntimeExports` and the first pre-`InitApp` line of its `RunClientRuntime` run before the
DLL has its own global data, so those few lines go to stdout only; the host already records the full
load/accept/enter handoff to the file, and once the DLL's `InitApp` runs, its `logging::write` appends to the
shared file too.

After a successful Case 1 binary update + restart request, the embedded host's `Application` instance
is destroyed (`App.reset()` in `RunClientRuntime`) before the host loads the freshly
downloaded DLL. This keeps a single SDL window alive at any one time â€” the host's window
disappears, then the DLL's `InitApp` creates a fresh one. Without this teardown the two
modules' independent `unique_ptr<Application> App` statics would briefly co-exist.

When the client runtime is running from a loaded DLL, `RunClientRuntime` also resets `App`
before returning to the host, and the application's destructor ends with `SDL_Quit()`, so SDL
windows, renderers, device threads and OS notifications are gone before control goes back. Both
embedded and DLL-backed runtime exits call `ApplicationShutdownHook()` before handing control back
to the host; embedding projects use that hook to stop process-global integrations such as
in-process crash handlers. `RunClientRuntimeAbi` then tears down the runtime's global data
(`global_data::destroy()`), which joins the async log writer and the global pools: the host carries on
in the same process, and nothing the runtime started may still be running, or be killed holding a
lock, when the host exits.

**The runtime library is never unloaded.** The host loads it with `platform::load_pinned_module`,
before it even queries the exports, and does not unload it when the runtime is rejected or returns.
The runtimes statically linked into it install process-wide hooks during the library's own static
initialization and first run that cannot be withdrawn — Mono's vectored exception handler and
unhandled-exception filter, rpmalloc's per-thread FLS cleanup callback, the backward-cpp crash
filter — so unmapping the library would leave them pointing at nothing. (With the static CRT a
running `std::thread` also keeps its module mapped, which is why an earlier `FreeLibrary` here never
actually unloaded anything.) Two consequences follow. The strings a `ClientRuntimeResult` points at
are published into storage the library owns (`CaptureClientRuntimeResultStrings`), because the
global data they were produced from is gone by the time the host reads them. And a runtime that ran
and returned an invalid result is treated as a fatal result rather than a reason to start the
embedded client: its own Mono and SDL are live in the process, and a second set beside them is not
safe. Embedded fallback remains for a library that is missing, fails to load, or is rejected before
`Run`.

### Self-update applies on the next launch (user restart)

A native self-update is **not** applied in the running process. When the updater stages the native
binaries it prints a "please restart" line **on the update screen** (`Updater::AddText` + `_restartPrompt`
in [../Source/Client/Updater.cpp](../Source/Client/Updater.cpp)) and holds that screen until the user
closes the client (Escape, which the updater already handles). The runtime then returns `ReloadRequested`
and the host (`PromoteStagedReloadForRestart` in
[../Source/Applications/ClientApp.cpp](../Source/Applications/ClientApp.cpp)) promotes the staged runtime
onto the live path (`ApplyStagedBinaryUpdate`) and **exits**. The next launch loads the promoted module
as its single, clean `InitApp`.

The message + hold are gated by `App->IsHeadless()` (a **runtime** check, since `FO_HEADLESS_APP` is an
app-target define that is not set when compiling `ClientLib` where the updater lives): a headless client
has no UI and no user to dismiss the prompt, so it skips the message/hold and the host promotes + exits
immediately.

An in-process reload is avoided because it is unsafe for two independent reasons:

1. **Stale module.** Reloading the **same** `<live>` path after staging the new module: if
   `platform::unload_module` does not bring the previous module's OS refcount to zero (Windows
   `LoadLibrary` path dedup, glibc keeping a `.so` resident), the reload's `platform::load_module` returns the
   **still-resident previous module** instead of the freshly-swapped file — so the runtime never
   actually updates. This is reliable, not occasional, on Windows.
2. **Second `InitApp`.** `InitApp`
   ([../Source/Frontend/ApplicationInit.cpp](../Source/Frontend/ApplicationInit.cpp)) is guarded by a
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
> small selector in the writable root itself, next to the log and the session marker
> (`MakeClientRuntimeBootstrapPath(<root>)` → `<root>/<runtime><ext>.path`). On the next launch a host
> with a writable root reads and validates that selector before `InitApp`, then loads the writable
> DLL directly. The frozen install-dir DLL remains the fallback when the selector is absent, malformed,
> names a different runtime, or points to neither a live nor staged file. Portable clients never consult
> this selector. Gameplay resources use the same overlay precedence: `GetClientResources()` mounts the
> read-only install packs first and then mounts any per-user resource packs on top. The updater reads its
> local metadata version through that same function rather than assembling a second pack set, so the
> version it validates is the one gameplay will read, and a repaired overlay pack cannot pass updater
> validation and then be bypassed in favour of a damaged install-dir copy. `Updater` layers the overlay
> over its splash pack too — the splash is drawn before this run downloads anything, so it would
> otherwise keep rendering an install-dir copy an earlier run already replaced. This precedence is also
> the recovery path after `METADATA_FILE_VERSION` changes: a new runtime treats an unreadable old install
> pack as having no local metadata version, downloads the current pack into the writable overlay, re-reads
> that overlay successfully, and only then constructs `ClientEngine`. The strict old-layout rejection is
> not relaxed. If the updater cannot complete that repair, the client exits with `Client update failed.
> Please install the latest full client package.` instead of surfacing `MetadataOutdatedException` from
> gameplay startup or misidentifying a resource failure as a native-module failure.

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
LF_Client.exe                                                           # bundled runtime, default compatibility
LF_Client.exe --ClientLibPath <path>                                    # explicit runtime, default compatibility
LF_Client.exe --ClientLibPath <path> --ClientLibCompatibilityVersion <ver>  # explicit runtime, no embedded fallback if ver != built-in
```

The bundled runtime library name is **derived from the host executable name** at startup via `GetCurrentClientRuntimeLibraryName()` (returns the exe basename without extension; falls back to `FO_DEV_NAME` when `platform::get_exe_path()` cannot resolve). The resolved live path is `GetClientRuntimeLivePath() = <exe_dir>/<library_name>` (extension is appended by `platform::load_module`). Renamed/multi-instance hosts therefore each load their own sibling module (`MyAlt.exe` â†” `MyAlt.dll`) instead of sharing one â€” no settings or packaging-time config patching needed. In the build tree, the `LF_ClientLib` target still writes its canonical `LF_ClientLib.*` artifact and also copies a host-derived alias (`LF_Client.dll` / `LF_Client.so` / `LF_Client.dylib`) so an unpackaged `LF_Client` can exercise the same loading path as a packaged client.

## Runtime ABI

[../Source/Client/ClientRuntimeApi.h](../Source/Client/ClientRuntimeApi.h) is the only contract between host and runtime. Both sides agree on:

- `FO_CLIENT_RUNTIME_HOST_ABI_VERSION` â€” bumped when the structs change shape or the required host
  lifecycle behavior changes. ABI 3 requires the promote-and-exit policy and rejects ABI-2 hosts that
  may attempt an in-process runtime reload.
- `ClientRuntimeMetadata` â€” runtime name, build hash, gameplay compatibility version.
- `ClientRuntimeExports` â€” entry table returned by `FO_QueryClientRuntimeExports(host_abi_version, *exports)`.
- `ClientRuntimeResult` â€” how the runtime communicates back to the host (`Shutdown`, `ReloadRequested`, `FatalError`).

A runtime that has staged a self-update sets `ResultKind = ReloadRequested` and fills
`RequestedRuntimePath`. Despite the ABI name, the host does not load that module again in the running
process: it promotes the staged file, exits, and lets the next user launch load the updated runtime as
its only `InitApp`.

The runtime stages a new module as `<live>-staging` next to the live module, where `<live>` is the updater's binary output path `Updater::GetRuntimeLivePath()` = `<Updater::_binaryDir>/<runtime_name><ext>` (the full live path including the platform runtime extension, e.g. `<exe_dir>/LastFrontier.dll` for a portable client, or `<UserWritablePath>/LastFrontier.dll` for an installed one). After each binary payload is fully downloaded and hash-validated, the updater also makes a best-effort attempt to promote that staged file to the live path immediately; if the live file is locked, the `-staging` file is left in place for the host's startup/exit-time promotion pass. The host promotes via `MakeClientRuntimeStagingPath(runtime_live_path)` â†’ `runtime_live_path` rename: at startup this is the path selected from the exe-dir default, installed-client bootstrap, or explicit CLI; after `ReloadRequested` it is the runtime-supplied `RequestedRuntimePath`. `RequestedRuntimePath` is the post-swap path (`<live>`), not the staging path. The host promotes it and exits; `LoadModule` happens only in the next process.

**Linux module isolation.** The runtime `.so` must stay loadable with `dlopen` from an engine host executable that exports its own engine symbols (`-rdynamic` for stack-trace symbolization). Two build rules keep that true. First, engine runtime modules link with `-Wl,-Bsymbolic` (`AddSharedApplication` in [../BuildTools/cmake/helpers/Build.cmake](../BuildTools/cmake/helpers/Build.cmake)), so the module binds global references — the global-data registry, allocator, logging — to its own definitions instead of interposing on the host executable's exported copies; each module keeps private engine state, mirroring the Windows DLL model (without this, the module's `global_data::create` resolves to the host's already-fired copy and the module crashes on its first global-data access). Second, vendored rpmalloc does not force initial-exec TLS on Linux (`(FOnline Patch)` in `ThirdParty/rpmalloc/rpmalloc/rpmalloc.c`): an IE-model TLS relocation makes glibc place the module's entire TLS segment into the limited static TLS surplus at `dlopen`, which fails with `cannot allocate memory in static TLS block`. The host/runtime C ABI keeps allocation ownership module-local (all strings are copied at the boundary), so per-module allocator state is safe.

A matching PDB (Windows-only, named `<live>.pdb`, e.g. `LastFrontier.dll.pdb`) is staged side-by-side as `<live>.pdb-staging` and usually promotes immediately because PDBs are not held by the loaded runtime module; if it is locked by a debugger or another process, `ApplyStagedBinaryUpdate` retries after the main DLL swap succeeds. The PDB swap is best-effort â€” failure only degrades stack traces, so it never blocks the runtime swap, while the DLL swap remains backup-rename-rollback atomic. The client-side filter accepts a server file whose basename starts with `<runtime_name>.`, so the DLL (`LastFrontier.dll`) and its PDB sibling (`LastFrontier.dll.pdb`) both match and ride the same `UpdateFileTarget::ClientBinaries` channel. **The runtime DLL and its `<live>.pdb` are fetched only together, in binaries mode** (when the DLL is actually being updated) — a client whose DLL is already current does not pull `<live>.pdb` on its own. **The host PDB (`<host_name>.pdb`, e.g. `LastFrontier.pdb`) is also delivered, but the client fetches it only to recover a *missing* local copy and never overwrites a present one.** The host exe is frozen and its PDB is build-specific, so the server's host PDB matches only an up-to-date host: an up-to-date client re-downloads a matching PDB, while an older host's matching local PDB is never clobbered (a non-matching server-build PDB is written only when the local one is absent, where the debugger ignores it by GUID). `accept_binaries` is `_binariesMode || CanSelfUpdateNativeModules(...)`, so host-PDB recovery also works on a normal resource-sync connect.

## Updater protocol

Versioned by `FO_UPDATER_VERSION` ([../Source/Common/Common.h](../Source/Common/Common.h)). Bump it when
the wire format changes or an older updater/host lifecycle is unsafe to continue. Generation 2 rejects
generation-1 clients before descriptor or binary transfer because their frozen hosts may attempt an
in-process runtime reload. Generation 3 changes what `hash` means for a resource pack entry - the header
`PackHash` rather than the whole-file digest - which a generation-2 client would compare against a digest it
computes itself and re-download for ever, so it is refused the same way. Gameplay compatibility (`Settings.Network.CompatibilityVersion`) is separate and
changes with every build.

### Handshake

| Direction | Field | Type | Purpose |
|-----------|-------|------|---------|
| client â†’ server | `CompatibilityVersion` | `string` | gameplay compatibility |
| client â†’ server | `MetadataVersion` | `string` | baked metadata version, empty while the updater has no resources of its own |
| client â†’ server | `updater_version` | `uint32` | `FO_UPDATER_VERSION` |
| client â†’ server | `binary_target` | `string` | e.g. `Windows-win64`, `Android-arm64` (from `GetCurrentBinaryUpdateTargetName()`) |
| client â†’ server | `in_encrypt_key` | `uint32` | session keys |
| server â†’ client | `compatibility_outdated` | `bool` | gameplay version mismatch |
| server â†’ client | `updater_outdated` | `bool` | `FO_UPDATER_VERSION` mismatch â€” protocol is unusable |
| server â†’ client | `metadata_outdated` | `bool` | client resources were baked from another revision |
| server â†’ client | `MetadataVersion` | `string` | the metadata version the server itself runs on |
| server â†’ client | `out_encrypt_key` | `uint32` | session keys |

`updater_outdated == true` is fatal to the connection â€” the protocol contract has changed and no further messages are valid. `compatibility_outdated == true` only blocks gameplay; the updater can still deliver resources / native modules to bring the client back to current compatibility.

`metadata_outdated == true` means the binaries match but the baked data does not. **Server and client must run
on metadata from one bake** — the property index space carried by entity payloads is that metadata's registration
order (see [GeneratedApiAndMetadata.md](GeneratedApiAndMetadata.md), metadata version). Reaching this verdict is
therefore a defect report, not a supported mode: the updater is supposed to have made it impossible.

Order of operations that keeps it impossible:

1. The updater connects first, sends whatever layout version its current packs carry (empty on a fresh install),
   and syncs every file the server announces.
2. After the sync it re-reads the version from the local packs. Unless it now equals the server's, the result is
   `UpdaterResult::MetadataMismatch` and **no `ClientEngine` is created** — the client never talks to a server
   whose data it does not share. A server that distributes no resources at all (unpackaged dev server) has nothing
   to verify and is skipped.
3. Only then is the client constructed, and it sends its own version in its own handshake.

If the verdict still arrives at that point, the server changed its resources between the sync and the connect: the
client throws `ResourcesOutdatedException` and the host syncs again — as often as that happens, since a server can
be redeployed any number of times while a client is running. This cannot spin: what stops a real divergence is step
2, where the updater refuses to report the resources ready at all, and every further round runs against the server's
new state. An unpackaged client is the exception — it has no updater and plays off `Baking.BakeOutput`, so its
verdict goes through the normal exception path instead.

### Init data

Sent once after a non-outdated handshake. Contains the descriptor of files the server is offering for this binary target plus initial gameplay state (global properties, synchronized time).

Each descriptor entry is:

| Field | Type | Notes |
|-------|------|-------|
| `name_len` | `int16` (`-1` terminates the list) | client-relative path length |
| `name` | `char[name_len]` | client-relative path |
| `size` | `uint64` | full file size |
| `hash` | `uint64` | FNV-1a 64-bit digest: for a resource pack (`.fores`) the `PackHash` its header carries, for anything else the whole file content |
| `target` | `UpdateFileTarget` (`uint8`) | `ClientResources` or `ClientBinaries` |
| `file_index` | `uint32` | server-assigned index for `GetUpdateFile` |
| `pack_header_size` | `uint32` | 80 for a resource base, 0 for native files |
| `pack_header` | bytes | Full version 2.0 base header, including physical and logical identity |

Common (gameplay-resource) entries are emitted for every binary target. Per-target binary entries (`UpdateFileTarget::ClientBinaries`) are emitted only for the matching `binary_target` from the handshake. The client then filters binary entries by the current host-derived runtime basename, so `LF_Client.exe` downloads `LF_Client.dll` while `LF_Client_OpenGL.exe` downloads `LF_Client_OpenGL.dll` even though both report the same CPU/OS target.

### Resource synchronization and resumable transfer

The updater protocol version is 4. It requests one bounded range at a time:

```text
GetUpdateFile  { file_index: uint32, start_offset: uint64, requested_size: uint64, expected_hash: uint64 }
UpdateFileData { update_portion: int32, raw bytes[update_portion] }
```

The backend caps each portion by `Network.UpdateFileMaxPortionSize`. The client advances within the requested
range and requests the remainder, using the existing temp-file size to resume a complete-file transfer after
reconnect without server-side state. It rejects negative, oversized or stalled replies. The server rejects an
unknown file index, mismatched expected artifact hash, out-of-bounds range, invalid portion setting or failed
read. Disk mode retains opened file descriptors for the advertised artifacts; memory mode retains their bytes.
Deployments must replace published artifacts, never modify the bytes of an opened artifact in place. An opened
old inode remains the matching source after a POSIX rename; Windows can reject replacement until it is closed.
Native files continue to use complete-file transfer. Resource ranges select catalogs or whole encoded resources,
not per-resource binary deltas.

The server advertises complete current `.fores` bases. For every resource target the updater compares the
selected pair's `ContentHash`, independently of its physical layout. If changed and patching is allowed, it
fetches the server catalog, reuses local resources with matching decoded hash and size, and plans a complete
patch catalog. Only absent encoded resources are requested. Renames/deletions can therefore commit without
payload transfer. No server-generated patch chain, set manifest or segment files are needed.

The writable pair consists of `Pack.patch.fores` and a selected full `Pack.fores`. The latter comes from the
writable resource directory when present, otherwise the installed directory or APK. The patch's header binds
it to the exact selected base hash. A patch is never an independent overriding source. See
[ResourcePackFormat.md](ResourcePackFormat.md) for byte layouts, hash encodings and recovery validation.

Patch growth has no configured size limit. Obsolete payloads and previous catalogs/footers remain in the
file, and its size never triggers a full-base download. An already current patch is retained unchanged.
Before appending, the updater checks available disk space for the new payloads, catalog and footer.
Missing or unusable base/pair data can still require a complete-base download for repair.

Patch publication appends verified payloads and the complete catalog, flushes them, then appends and flushes
the commit footer. New directory entries are persisted on POSIX. A failed update leaves the previous commit
readable. Restart scans backward only when EOF lacks a valid footer, then truncates the uncommitted suffix
before another append. It may redownload the interrupted addition; no persistent resource resume journal is
stored. The writable directory is locked during mutations using platform locks, with no lock/selector file.

Complete bases download into `~<filename>` under the writable resource directory. Space admission counts all
remaining bytes; the file is not preallocated because its actual length is the resume position. A completed
file must reproduce the advertised physical hash and have a valid catalog with the target logical hash.
`ReplaceFileSafely` moves the prior writable base to `<name>-backup`, promotes the verified file with checked
durability ordering, then discards the backup. Only after promotion succeeds is `Pack.patch.fores` removed.
The read-only installation remains intact; subsequent reads select the writable base. A stale leftover patch
cannot apply to its replacement because its base binding differs.

Complete native-file hash checks use `Updater::IsDiskFileHashMatch`, which caches `(size, mtime, hash)` in
`CacheStorage` under a basename plus a hash of the full path. A changed size or modification time invalidates
the entry. Resource freshness uses the pair's catalog `ContentHash`; a fully downloaded replacement base is
instead verified against its header `PackHash` before promotion.

Interrupted replacement recovery scans both writable resource and binary trees recursively before updater
reads, including names hidden by ordinary resource enumeration. It snapshots file names before renaming or
removing backups and holds the writable-resource lock through both scans, since the binary root may contain
the resource tree. Shared base resolution restores a missing writable base from its backup before bootstrap/Core
or cache selection can fall back to the installed copy.
A backup with a present live counterpart is obsolete. The updater releases its mounted sources before mutation;
Windows file sharing can reject a reset while another reader still holds the old pair. POSIX readers retain
valid old file descriptors across replacement rather than following the new path.

Every advertised resource is checked again before `ResourcesReady`, followed by metadata compatibility.
Packs commit independently: interruption can leave different packs at different versions, and synchronization
must finish before gameplay. Matching only the metadata pack is insufficient. There is no global atomic
release switch or rollback of a partially synchronized set.

A successful resource sync rebuilds the disposable `Resources.foindex` over effective pairs in the configured
suffix after Embedded. Earlier packs and Embedded retain their positions. Cache freshness includes the selected
base and patch commit identities; cached/direct mounts agree on lookup, deletions, timestamps and enumeration.
A failed cache build is logged and authoritative pairs remain usable. Web skips cache creation because its
filesystem does not survive a page reload.

Native whole-file hashes use the existing `(size, mtime, hash)` cache in `CacheStorage`; full resource body
hashing is reserved for verifying completed base downloads. Normal resource mounts validate catalogs and
verify individual payload hashes when resources are read. All descriptor paths are checked before joining
writable paths. The updater also consumes connection-stage `HashList` messages normally.

Obsolete temporary full downloads are swept after the desired file list arrives. Format and protocol readers
require the current versions; they contain no previous-format parsing or migration fallback.

## Server-side: `UpdaterBackend`

[../Source/Server/UpdaterBackend.h](../Source/Server/UpdaterBackend.h) is owned by `ServerEngine` as an `optional`. When `_updaterBackend` is empty (unpackaged dev server) the server rejects `GetUpdateFile` with `HardDisconnect` â€” there is nothing to serve.

Public API:

```cpp
void LoadFromClientResources(const GlobalSettings& settings, string_view server_metadata_version);
void ProcessUpdateFile(ptr<Player> player, int32_t update_file_max_portion_size);
auto GetUpdateDescriptor(string_view binary_target_name) const -> const_span<uint8_t>;
```

- `LoadFromClientResources` walks `Settings.Baking.ClientResources`, picks every pack `Settings.GetClientResourcePacks()` derives from the declared resource packs (excluding `Embedded`; a packaged server reads those declarations from the `[ResourcePack]` sections its baked config closes with), then enumerates `Settings.Baking.PlatformBinaries/<target>/` for per-target binaries (default `PlatformBinaries/`, sibling of `Resources/` in the package layout).
- Entries retain size, hash and the resource header. Memory mode retains all bytes; disk mode retains an opened positional reader. Both modes serve the artifact the descriptor identifies.
- Descriptors are cached per `binary_target_name`. Common-resource entries are merged into every per-target descriptor; targets without specific binaries fall back to the common-only descriptor.
- `VerifyClientResourcesMetadata` then mounts the client packs and compares their metadata version against the one
  the server itself loaded. The server runs on `Settings.Baking.ServerResources` and hands out `Settings.Baking.ClientResources`, so
  a deploy that refreshed only one of them would leave every synced client with a property layout the server cannot
  talk to; startup fails with `UpdaterException` naming both versions instead.

## Settings

| Setting | Where | Purpose |
|---------|-------|---------|
| `Network.UpdateFileMaxPortionSize` | top-level | Maximum bytes per `UpdateFileData` response. Drives both transfer throughput and per-message memory pressure. Default 1 MB (engine) / 5 MB (this project). |
| `ServerNetwork.UpdateFilesInMemory` | top-level + `[SubConfig]` | `True` keeps every packaged update file in RAM (low CPU under load). `False` serves from disk on demand (low RAM, more I/O). Public `[SubConfig]`s in this project: `PublicGame = True`, `DailyTest = True`, `Staging = True`. |
| `Network.ForceMetadataVersion` | top-level | Testing only: overrides the layout version the client reports, so a divergence can be simulated without a second bake. Empty in every shipped config. |
| `Baking.PlatformBinaries` | top-level | Directory the server reads per-target client runtime libraries from, and the packager writes them to. Default `PlatformBinaries`, resolved relative to the server's working directory / package root. |
| `Common.UserWritablePath` | common | **Read-only**: the writable data root for everything written at runtime — log, cache, resource overlay, self-updated binaries, and on the server the database. Resolved at startup before any config is read, so it is not authorable: `--Common.UserWritablePath <path>` names it, otherwise an `INSTALLED` marker beside the executable selects the per-OS user data dir plus the project name, otherwise it stays empty and everything is relative to the working directory. See the section below. |

There is no auto-detection of memory vs disk mode in C++. Choose explicitly per environment.

## Installed vs portable writable data

A **portable** build writes its cache, log, and self-update files next to the exe — fine for a zip the
user unpacks anywhere. The Windows MSI defaults to `%LOCALAPPDATA%`, but an **installed** build may
still sit in a read-only directory after an explicit `Program Files` choice (or under `/usr/...`), so
its writes must go to a per-user writable location instead.

`ResolveWritableRoot(args)` (`Source/Frontend/ApplicationInit.cpp`) answers it, and it is **settings-free
by design**: the log, the cache and the local-config cache all live under this root, so nothing read from
disk may decide where it is. It runs before the config is even located, which is why it is also the first
thing `main` does — the log file opens at its final location instead of being moved there later. In order:

1. **`--Common.UserWritablePath <path>` on the command line**, scanned by hand rather than through the
   settings parser. This is how Android passes the directory the platform hands it
   (`FOnlineActivity.getArguments`), and how a test isolates a run.
   A config file **cannot** set it: a value that lives inside the root cannot name the root. The value `*`
   asks for the same per-user directory the marker selects, for a launcher that wants it without knowing
   the per-OS path.
2. **an `INSTALLED` marker beside the executable** → the per-OS user data dir from
   `platform::get_user_data_base()` (environment first, the OS itself as fallback): Windows
   `%LOCALAPPDATA%`, macOS/iOS `~/Library/Application Support`, Linux `$XDG_DATA_HOME` or
   `~/.local/share` — plus `FO_NICE_NAME`. Android never reaches this lookup: `FOnlineActivity` always
   passes its `getFilesDir()` through `--Common.UserWritablePath`. The **project** name, not `Common.GameName`, because the name
   has to be known before any config is read; the Windows MSI installs into the same directory name, so a
   default install keeps one folder rather than two.
3. **otherwise portable**: every writable path stays relative and therefore resolves against the **working
   directory**. That is the anchor of the whole portable layout — the main config is found by walking up
   from `std::filesystem::current_path()`, and `ClientResources`, `CacheResources` and `BakeOutput` are
   relative names read from the same place — so writes cannot be anchored to the executable's directory
   without splitting them from the reads they pair with. A player launching the exe from Explorer, Steam
   or a shortcut gets a working directory equal to the install directory; a launcher that sets a foreign
   one breaks resource loading first.

Resolution is idempotent, creates the directory (and `LoadAppSettings` then pre-creates the
`Cache`/`<ClientResources>` subdirs once their names are known), and is **fail-safe**: if the directory
cannot be determined or created it logs a warning and falls back to the working directory, so a bad
install never bricks startup.

Android supplies its app-private writable root explicitly and reads installed full bases directly from
uncompressed APK asset regions. Web uses its preloaded writable in-memory filesystem without persistence.
See [ResourcePackFormat.md](ResourcePackFormat.md).

What moves to the writable root (via the free path helper `fs::make_writable_path(UserWritablePath, relative)`
in `DiskFileSystem.cpp`): the **cache** (`CacheStorage` in `ApplicationInit`/`Client`/`Updater` — login keys, native
secure storage, local config), the **log** file (re-pointed after settings load), **self-update resource
patches** — the updater writes them under `<root>/<ClientResources>` for relative resource paths, or `<root>/Resources` for an absolute installed/APK path, while both the updater's post-sync
metadata check and `ClientEngine` obtain their identically ordered pack view from `GetClientResources()`.
That view selects one base and optional patch per logical pack, preserving configured pack order, so the files the updater
validated are exactly the files gameplay opens — and the **self-updated native runtime** (see below).
The updater's packaged-mode gates and resource-root choices use the already loaded read-only
`Common.Packaged` snapshot; direct executable-marker checks are limited to the pre-settings bootstrap and
the filesystem's physical archive-versus-directory selection.

**Native binary self-update for installed builds writes the runtime into the writable root**
(`Updater.cpp`). The updater's binary output dir (`Updater::_binaryDir`) comes from
`GetClientBinaryDir(UserWritablePath)` — the one rule for where a client keeps the binaries it may replace:
`<root>` when it has a writable root and the exe dir when it does not — so a self-updated runtime lands at `<root>/<runtime_name><ext>` (mirroring
the install-dir layout, `<exe_dir>/<runtime_name><ext>`) alongside its `-staging` and `<...>.pdb` siblings. It
is **not** gated off — both portable and installed clients self-update on every platform where
`CanSelfUpdateNativeModules()` is true.

The runtime returns the writable live path through `ClientRuntimeResult::RequestedRuntimePath`. The host
promotes that path, validates that it is absolute and has the current executable-derived runtime filename,
writes it to the bootstrap selector, and exits; it never loads it again in the same process. The host
resolves the writable root once at startup through `ResolveWritableRoot` and `GlobalSettings::ApplyWritableRoot`,
and derives the selector path and the session marker from it — so both halves of the
client answer "where does this client write" identically, and an explicitly configured `UserWritablePath`
is honoured by the host as well. On the next launch the host reads the selector from
`<root>/<runtime><ext>.path` before `InitApp`, accepts it only when the live file or its `-staging` sibling
exists, and loads that runtime directly. Missing, oversized, relative, newline-containing, wrong-basename,
and stale selectors fall back to the frozen install-dir runtime. `--ClientLibPath` remains the final
explicit override. A client with no writable root updates its exe-dir sibling runtime in place and neither
writes nor reads a selector — there is nothing for one to point at.

**Trigger:** the installer drops an `INSTALLED` file next to the exe, and its presence alone selects the
per-user layout. The portable zip has no marker. Android needs none either — its launcher passes the
platform's own directory on the command line; when macOS/iOS bundle packaging lands, the marker belongs in
`Contents/MacOS/` beside the executable, since a bundle's contents are read-only. The MSI packager adds the marker to the MSI payload only (`package.py::make_wix_installer`, added
then removed around `createmsi` so the sibling Raw/Zip portable artifacts stay portable).

## Packaging

[../BuildTools/package.py](../BuildTools/package.py) does both halves:

- **Client packages** include the host exe (e.g. `LF_Client.exe`) and the matching runtime library renamed to the same basename next to it (`LF_Client.dll`). The host derives the library name from its own exe basename at startup, so no config patching is required to point one at the other.
- **Sibling client variants are not runtime companions.** Native GUI and headless hosts/runtimes share a build-output directory, so `package.py` excludes all engine-owned `Client`/`ClientLib` and `ClientHeadless`/`ClientLibHeadless` library names from the generic DLL/DSO companion pass. Only variants requested by the package are copied explicitly under their packaged basenames. This keeps stale headless build outputs out of ordinary portable/installer payloads while preserving explicit `Headless` test packages.
- **Server packages** also stage every available client runtime library under `<Settings.Baking.PlatformBinaries>/<binary_target>/<output_name><runtime_ext>` (default `PlatformBinaries/`, sibling of the client-resources dir in the package layout) so a different-platform client connecting to this server can self-update its native modules.
- **Managed class libraries are platform-specific resource data.** The Managed baker places the filtered payload under `ManagedRuntime/` in its resource pack and tags each managed assembly directory with the standard resource-role suffix. `package.py` filters those directory components, keeps only `Assemblies/Assemblies-client/` in each Client copy, rebuilds it with that target's side-by-side clean payload, and stages corresponding Server updater copies under `PlatformBinaries/<target>/<pack>.fores`; Server and Mapper assemblies are not delivered to clients, the side-by-side directory is not shipped, and native Mono files are not hoisted into package roots. Web and Android use the same resource path but receive their own target contents.
- **Windows Client packages with the `Wix` pack** build an additive MSI from the already-staged Raw client payload. `package.py::make_wix_installer` writes a temporary WiX JSON config and adds the `INSTALLED` marker only while the MSI payload is generated; `createmsi.py` defaults `INSTALLDIR` to `%LOCALAPPDATA%\<Common.GameName>` and registers the selected path plus the product URI scheme through HKCU registry entries. A remembered path or explicit command-line/UI choice still overrides that default. WiX/wixl and the generated MSI are required when the pack requests `Wix`; a missing toolset or generator failure aborts the package instead of silently publishing only Raw/Zip.
- **PDBs for Windows runtime DLLs** are shipped under `<runtime_dll>.pdb` (e.g. `LastFrontier.dll.pdb`) â€” both next to the bundled client DLL and inside every server-staged `PlatformBinaries/Windows-*` payload. The host exe keeps its own `<host_name>.pdb` so the two namespaces never collide. `package.py` patches the CodeView (`RSDS`) record in place to point at the new PDB filename — for the renamed runtime DLL (`copy_runtime_pdb`) **and** for the host exe (`<name>.pdb`, patched at the `copy_pdb` call site) — so DbgHelp / `backward-cpp` resolve symbols automatically without relying on the build-machine path baked into the binary. Missing PDB inputs or failed RSDS patches `assert` immediately during packaging â€” symbol gaps are never silently tolerated.
- **The host PDB is delivered for missing-copy recovery only.** `package_all_client_runtime_update_payloads` stages the host's own `<name>.pdb` alongside the runtime DLL and its `<name>.dll.pdb` under `PlatformBinaries/<target>/`. The host exe is frozen and never delivered, so its PDB is build-specific and the server only carries its *current* build's host PDB. The client therefore fetches the host PDB **only when its local copy is missing** and **never overwrites a present one** (`Updater.cpp` skips the `<runtime_local_prefix>.pdb` entry when the file already exists, in either resource-sync or binaries mode). An up-to-date host re-downloads a matching PDB; an older host's matching local PDB stays untouched (and only if the player deleted it does the client write the current, non-matching one, which the debugger ignores by GUID). This recovers a deleted host PDB without ever clobbering a good one — the clobber that an unconditional host-PDB delivery used to cause for self-updated clients (frozen old host + newer server host PDB).

Both the bundled runtime library in client packages and the runtime libraries staged for server-side binary updates go through the same package-time patching as ordinary executables: embedded resources, internal config, and packaged mark are written by `package.py`. Variant-specific config is applied to the runtime payload that actually runs the game; for example the Windows OpenGL runtime receives `ForceOpenGL=1`. The embedded-resource zip is produced with pinned entry timestamps and permissions (`make_embedded_pack`), so the bundled-client copy of a runtime and the matching `<Baking.PlatformBinaries>/<target>/<output_name><ext>` payload remain byte-identical across separate Server/Client package runs.

Client resource packs are written in the engine pack format ([ResourcePackFormat.md](ResourcePackFormat.md)), from sorted normalized paths and with no timestamp anywhere in the file. This matters because the baker touches unchanged output files during incremental runs; package output must ignore those mtimes so a content-identical repack keeps the same hash in the updater descriptor and does not force clients to redownload every pack. The `Embedded` pack is the one that stays a zip, since it is patched into the executable rather than shipped as a file: the packager reopens its in-memory buffer, verifies the exact entry list and streams every entry through Python's CRC-checking reader before embedding it, where a corrupt archive would otherwise be undetectable until a player's client failed to read it. `../BuildTools/tests/test_package_zip_helpers.py` covers the mtime/order and post-build validation invariants.

The internal config patch area has a fixed engine-owned capacity of 10000 bytes; embedding projects cannot resize it. `package.py` discovers the reserved size from the generated binary markers before writing bootstrap config data. The baked config ends with the pack declarations every packaged application mounts its packs from, so variant config is written in front of it; see [ConfigurationAndDataSources.md](ConfigurationAndDataSources.md#runtime-settings).

Naming convention from `build_runtime_update_target_name` in `BuildTools/package.py`:
- `Windows-win64`, `Linux-x64`, `Linux-arm64`, `macOS-arm64`, `Android-arm64`, etc.
- Profiling variants get the `_Profiling` suffix in the staged file name.
- The Windows OpenGL variant (`OGL`) is staged separately and patches `ForceOpenGL=1`.
- Entries tagged with a `FO_BINARY_OUTPUT_POSTFIX` (e.g. `Client-Linux-x64-Steam`, `Client-Windows-win64-Steam`) are staged under the same `PlatformBinaries/<target>/` directory as the default variant, but `package_all_client_runtime_update_payloads` appends `_<postfix>` to every staged payload name (`LastFrontier_Steam.so`, `LastFrontier_Headless_Steam.so`, …) so the variants don't clobber each other. `extract_binary_entry_postfix` parses the postfix out of `Client-<platform>-<arch>[-Profiling_X][-Debug][-<postfix>]`. The matching Client package builds with the same `FO_BINARY_OUTPUT_POSTFIX` and the client-side packager mirrors the suffix in `bin_out_name` so the patched `PACKAGED_BUILD_NAME` lines up with the server-side payload name — that's what `Updater.cpp::remap_runtime_name` keys on (`runtime_server_prefix = GetPackagedRuntimeName()`).

## Lifecycle

```
LF_Client.exe main
    â”œâ”€â”€ ResolveRequestedClientRuntime(argc, argv)        # Path + CompatibilityVersion + ExplicitPath
    â”‚
    â”œâ”€â”€ RunClientFromLibrary(argc, argv, requested, *)   # CASE 2: bundled runtime exists
    â”‚     â”œâ”€â”€ ApplyStagedBinaryUpdate(requested.Path)    # promote <requested>-staging (no-op when missing)
    â”‚     â”œâ”€â”€ platform::load_module + FO_QueryClientRuntimeExports
    â”‚     â”œâ”€â”€ Validate exports + metadata
    â”‚     â”œâ”€â”€ exports.Run(argc, argv, &result)           # DLL drives RunClientRuntime:
    â”‚     â”‚     â”œâ”€â”€ single Updater (UI) connects to the server. The connect result picks the mode:
    â”‚     â”‚     â”‚     â”œâ”€â”€ Success         â†’ resources mode â†’ sync ClientResources, finish ResourcesReady
    â”‚     â”‚     â”‚     â””â”€â”€ CompatibilityOutdated:
    â”‚     â”‚     â”‚             â”œâ”€â”€ if !CanSelfUpdate    â†’ finish PlatformUnsupported, caller shows store msg
    â”‚     â”‚     â”‚             â””â”€â”€ else                  â†’ binaries mode â†’ write ClientBinaries to
    â”‚     â”‚     â”‚                                          `<live>-staging`, try immediate promote, or verify `<live>`,
    â”‚     â”‚     â”‚                                          finish BinariesStaged
    â”‚     â”‚     â”œâ”€â”€ On BinariesStaged: set ResultKind = ReloadRequested, RequestedRuntimePath
    â”‚     â”‚     â”œâ”€â”€ On any other non-success result: ShowUpdaterFailure(result) and quit
    â”‚     â”‚     â””â”€â”€ unload of DLL (scope_exit) frees the loaded module
    â”‚     â””â”€â”€ If ResultKind == ReloadRequested: PromoteStagedReloadForRestart
    â”‚           â””â”€â”€ ApplyStagedBinaryUpdate(requested path), then exit
    â”‚
    â””â”€â”€ If LoadModule failed (CASE 1: no DLL yet, packaged install):
          if !CanFallbackToEmbeddedClient(requested): return false
          RunEmbeddedClient(argc, argv, *)               # host-module RunClientRuntime
          (same single-Updater flow as the DLL; host module's App.reset() runs after
           ReloadRequested before the host promotes the runtime and exits)
          if ResultKind == ReloadRequested â†’ promote staged runtime, then exit
```

A single `Updater` instance handles both gameplay-resources and native-binaries syncs.
It picks the mode internally based on the server's compatibility verdict on connect â€” no
per-stage construction, no caller-side mode parameter, no separate "BinaryUpdater" type or
headless variant. The splash UI (`Application::MainWindow`) is shared throughout, so the
user always sees indication of what is happening. The terminal state is exposed via
`Updater::GetResult()` returning `UpdaterResult` (see header).

`UpdaterResult::ConnectionFailed` separates "the server was not reachable" from "this client could not
update itself". Both connection aborts land on it - the connect that never succeeded, and a drop while
files were still in flight - and `IsUpdaterFailureReportable()` is what keeps it out of the crash
reporter: a server that is down, restarting or unreachable from the player's network is an environment
state, so filing it would cost one report per player per restart and carry nothing the server side does
not already know. The failure is still visible - `ShowUpdaterFailure` writes the terminal result to the
log unconditionally, before deciding whether to report it - and the player is told the server may be
offline instead of being advised to reinstall a client that is not at fault. Every other result keeps
reporting, `MetadataMismatch` included: its player-facing advice is also "try again later", but it names
a server distributing resources it does not run on, which is a deployment defect worth a report.

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
| Host can't find runtime, no fallback possible, or resource repair cannot complete | client message box `Client update failed. Please install the latest full client package.` |
| Client started while the server is down, restarting, or unreachable | client message box `Can't connect to the server. It may be offline or restarting, please try again later.`, client log `Client updater: connection failed` then `Client updater: terminal result ConnectionFailed`. Deliberately files **no** crash report - an offline server is not a client defect, and reporting it would flood the crash reporter on every restart |
| Updater protocol mismatch | server log `Connected client X has outdated updater version Y`; generation-1 client message box `Client updater outdated, please update the base client`; generation-2+ wording `Client updater is incompatible with this server. Please install the latest full client package.` |
| Gameplay version mismatch on a self-update platform | resource updater finishes silently with `WasCompatibilityOutdated() == true`; the runtime opens the binary updater UI, stages the current module, shows the restart prompt, and returns `ReloadRequested`; the host promotes the staged runtime and exits |
| Gameplay version mismatch on Web / iOS / Android | message box `Client outdated, please update via your app store`, then quit (no in-process self-update on these platforms) |
| Wrong file index / offset | server log `Wrong file index N, from host '...'` / `Wrong update file offset O, file index N, client host '...'` (both at `logging::type::warning`), client gets disconnected |
| Client data does not match the server data | server log `Connected client X runs metadata version A while the server runs B`; updater log `synced resources run metadata version A while the server runs B, resources <dir>`. Both name the two versions - find which resource directory came from a different bake |
| Server distributing resources it does not run on | server startup fails with `Distributed client resources were baked apart from the server resources`, naming both resource directories and both layout versions |
| Server or unpackaged development resources predate the current metadata format | startup fails at the metadata header: `does not start with the metadata file marker`, `file version does not match the engine`, or `carries no version` - run a full rebake. A packaged client with an old install pack recovers through the writable updater overlay before gameplay startup; if that repair fails, install the latest full client package |
| Server has no native update for this target | message box `Server doesn't provide a native client update for binary target <target>` |
| Stale staging file | `<live>-staging` survived a previous failed swap; the next `LF_Client.exe` startup promotes it via `ApplyStagedBinaryUpdate` before loading the runtime |
| Linux host logs `LoadModule failed` for a present, valid runtime `.so`, then `trying embedded fallback` on every launch | `dlopen` rejected the module. Two engine build rules must hold (see "Linux module isolation" above): the module is linked with `-Wl,-Bsymbolic` (`AddSharedApplication`), and no vendored code forces initial-exec TLS on Linux — an IE-model TLS relocation fails `dlopen` with `cannot allocate memory in static TLS block` (diagnose with a standalone `dlopen` of the `.so`, e.g. via `python3 -c "import ctypes; ctypes.CDLL('./<runtime>.so')"`). A silently-engaged embedded fallback makes a native self-update loop: the downloaded `.so` is promoted on disk but never executed |
| Self-update downloads and then waits on the update screen | This is the expected native flow. Close the client after the restart prompt; the host promotes the staged runtime and exits, and the next user launch starts the updated runtime with one clean `InitApp`. Hosts predating this policy are rejected by updater generation 2 / runtime ABI 3 and require the latest full client package instead of attempting the unsafe second initialization |
| Stack trace shows raw addresses for the new runtime DLL | After a binary self-update the renamed `<live>.dll`'s CodeView entry must reference its sibling `<live>.dll.pdb`. If `package.py` skipped the RSDS patch (it will assert when this happens), `dbghelp`/`backward-cpp` cannot find the PDB and frames in the runtime resolve to addresses only |
| Stack trace shows raw addresses for **host** (`<host>.exe`) frames after a self-update, while runtime-DLL frames resolve | The on-disk `<host_name>.pdb` doesn't match the frozen exe (CodeView GUID differs) — typically a leftover from an old updater build that clobbered the matching host PDB with a newer server-build one. The current updater never overwrites a present host PDB and fetches one only when the local copy is missing, so the fix is to delete the mismatched `<host_name>.pdb`: an up-to-date host then re-downloads the matching one; otherwise restore the host PDB shipped with that exe build (matching CodeView GUID). A mis-walked stack through unsymbolized host frames can also surface bogus top frames (e.g. attributing the fault to an unrelated system DLL) |

Local validation steps:

1. Build `LF_UnitTests` and run it. [../Source/Tests/Test_ClientRuntimeApi.cpp](../Source/Tests/Test_ClientRuntimeApi.cpp) exercises the ABI surface plus installed-runtime selector round-trip, validation, live selection, staged recovery, and fallback; [../Source/Tests/Test_DiskFileSystem.cpp](../Source/Tests/Test_DiskFileSystem.cpp) covers `fs::hash_file` parity with `fs::hash_data` and `fs::make_writable_path`; [../Source/Tests/Test_Platform.cpp](../Source/Tests/Test_Platform.cpp) covers `platform::get_user_data_base`; [../Source/Tests/Test_Settings.cpp](../Source/Tests/Test_Settings.cpp) covers `UpdateFilesInMemory` sub-config inheritance and writable-root fail-safe/creation behavior.
2. Build `LF_Client`; its native target dependency also builds `LF_ClientLib`. Confirm the client output directory contains the host plus the host-derived runtime alias (`LF_Client.exe` + `LF_Client.dll` on Windows, `LF_Client` + `LF_Client.so` on Linux). Build `LF_ClientLib` explicitly when validating the runtime target in isolation.
3. Launch `LF_Client.exe` with the bundled runtime present â†’ normal startup (Case 2 happy path: load DLL, resource updater finishes, game starts).
4. Launch `LF_Client.exe --ClientLibPath <path>` with a valid alternate runtime â†’ host routes through the loaded library.
5. Launch `LF_Client.exe --ClientLibPath <path> --ClientLibCompatibilityVersion <other>` and remove the runtime â†’ host fails (no fallback).
6. Point `--ClientLibPath` to an invalid path, no `--ClientLibCompatibilityVersion` â†’ host falls back to embedded client (Case 1).
7. Build a packaged server (e.g. `Daily`) and confirm `<Settings.Baking.PlatformBinaries>/<target>/<name><ext>` (default `PlatformBinaries/`, sibling of the client-resources dir in the package layout) contains the per-target runtime libraries and that `ClientResources` pack list contains the `.fores` resource packs.
8. Interrupt a client mid-download (kill the network) and reconnect â€” the next `GetUpdateFile` resumes from the temp-file size, no full re-download.
9. Force a Case 2 â†’ restart: package a client against an older `FO_COMPATIBILITY_VERSION`, point it at a server with a newer one, run. The resource updater UI should appear briefly, then the binary updater UI takes over (UI/SplashPic identical). Close the client after the restart prompt; the host renames `<live>-staging` over `<live>` and exits without loading it. The next launch must load the promoted runtime in a fresh process and reach the game.
10. Crash recovery: kill the host while the binary updater UI is mid-download. Restart `LF_Client.exe`. `ApplyStagedBinaryUpdate` runs at the start of `RunClientFromLibrary`; if `<live>-staging` is fully written it gets promoted, otherwise the runtime's resume logic completes the download in a normal updater session.
11. Installed-layout smoke: place an `INSTALLED` marker next to the client executable (or build the Windows `Wix` package), leave `Common.UserWritablePath` empty, and launch. The resolved writable root should be the per-OS user-data dir plus `Common.GameName`; cache/log/resource overlay writes should go there, while the install-dir resources remain read-only inputs. Force both a resource-pack update and a native update, close at the restart prompt, and launch again: the host should log `selected runtime ... from bootstrap ...`, load the writable-root runtime directly, and gameplay must read the updated writable pack rather than its frozen install-dir counterpart. Delete or corrupt the selector and confirm the host safely falls back to the install-dir runtime.

## See Also

- [BuildWorkflow.md](BuildWorkflow.md) — configure, build and validation workflow.
- [BuildToolsPipeline.md](BuildToolsPipeline.md) — packaging and generated-source stages.
- [Architecture.md](Architecture.md) â€” engine + game build layout, target table.
- [Debugging.md](Debugging.md) â€” debugger setup; the host vs runtime split affects which binary the debugger should attach to.
- [ResourcePackFormat.md](ResourcePackFormat.md) â€” the `.fores` pack and the derived `.foindex` tree the sync moves and rebuilds.
