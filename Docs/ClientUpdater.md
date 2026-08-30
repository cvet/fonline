# Client Runtime Split and Updater

> Engine-owned documentation. Paths under `../` are relative to the FOnline engine root. Paths under `../../` point to an embedding game project such as Last Frontier when this engine is used as a submodule.

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


## Server-side updater backend

The client updater is served by the authoritative server runtime. `ServerEngine` wires an `UpdaterBackend` from `Source/Server/UpdaterBackend.*` during server startup when client packs/resources are prepared. The backend scans client resources and native runtime artifacts, builds target-specific update descriptors, and answers file-portion requests with `NetMessage::UpdateFileData`.

Runtime ownership is split deliberately:

- [ServerRuntime.md](ServerRuntime.md) documents where `UpdaterBackend` is hosted and how it fits into server startup/connection processing.
- This page documents the client host/runtime ABI, staging/promotion/restart flow, compatibility checks, and updater protocol behavior visible to the client.

Keep long protocol and host-runtime details here; keep server lifecycle and manager ownership in [ServerRuntime.md](ServerRuntime.md).

## Source paths inspected

- `Source/Applications/ClientApp.cpp`
- `Source/Applications/ClientLib.cpp`
- `Source/Client/ClientRuntimeApi.h`
- `Source/Client/ClientRuntimeApi.cpp`
- `Source/Client/Updater.h`
- `Source/Client/Updater.cpp`
- `Source/Frontend/ApplicationInit.cpp`
- `Source/Client/UpdaterFastClient.h`
- `Source/Client/UpdaterFastClient.cpp`
- `Source/Server/UpdaterBackend.h`
- `Source/Server/UpdaterBackend.cpp`
- `Source/Server/UpdaterFastServer.h`
- `Source/Server/UpdaterFastServer.cpp`
- `Source/Common/ContentUpdater.h`
- `Source/Common/ContentUpdater.cpp`
- `Source/Server/Server.cpp`
- `Source/Common/Common.h`
- `Source/Common/Settings.inc`
- `Source/Essentials/DiskFileSystem.h`
- `Source/Essentials/DiskFileSystem.cpp`
- `Source/Essentials/Platform.h`
- `Source/Essentials/Platform.cpp`
- `BuildTools/cmake/stages/Applications.cmake`
- `BuildTools/package.py`
- `BuildTools/msicreator/createmsi.py`
- `BuildTools/tests/test_package_wix_installer.py`
- `BuildTools/tests/test_package_zip_determinism.py`
- `Source/Tests/Test_ClientRuntimeApi.cpp`
- `Source/Tests/Test_ContentUpdater.cpp`
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
    â”‚  3. Platform::LoadModule(<runtime>) â†’ FO_QueryClientRuntimeExports(...)
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
the freshly downloaded runtime and incorrectly start the embedded updater. `--ClientLibCompatibilityVersion
<version>` is the opt-in strict mode for tests and explicit host/runtime probes: when it is passed and
differs from the host's compatibility, embedded fallback is refused rather than silently downgrading to
host code.

Startup/runtime handoff diagnostics go to the normal `<host>.log` through the regular `WriteLog` path.
The host brings up engine global data (`CreateGlobalData()` in `main`), resolves the log beside the exe
for a portable client or under the marker-derived writable root for an installed client, and opens it
fresh up front (`LogToFile(host_log_path, false)`) — the host runs first, so it truncates. It then keeps
its handle open across the loaded-DLL call instead of closing before the handoff: `LogToFile` opens the file without
an exclusive lock (the platform default — MSVC `std::ofstream` is deny-none, POSIX has no mandatory
open lock). The host EXE and the runtime DLL are two engine modules in one process, each carrying its
own copy of the engine global data, so they cannot share one `std::ofstream`. With shared access both
can hold the same file open; the host is idle while the DLL's `Run` export executes, and the DLL drains
and stops its asynchronous log writer before returning, so the host's post-handoff lines append only
after the DLL's whole session instead of racing or overwriting it. Client runtimes pass
`AppInitFlags::AppendLogFile` into
`InitApp` (which resolves the same `GetExeLogFileName()`), so each DLL/embedded `InitApp` appends to the
shared file instead of truncating the host's lines. The DLL's
`FO_QueryClientRuntimeExports` and the first pre-`InitApp` line of its `RunClientRuntime` run before the
DLL has its own global data, so those few lines go to stdout only; the host already records the full
load/accept/enter handoff to the file, and once the DLL's `InitApp` runs, its `WriteLog` appends to the
shared file too.

After a successful Case 1 binary update + restart request, the embedded host's `Application` instance
is destroyed (`App.reset()` in `RunClientRuntime`) before the host loads the freshly
downloaded DLL. This keeps a single SDL window alive at any one time â€” the host's window
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
   `Platform::UnloadModule` does not bring the previous module's OS refcount to zero (Windows
   `LoadLibrary` path dedup, glibc keeping a `.so` resident), the reload's `LoadModule` returns the
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
> small selector under `<Platform::GetUserDataBase()>/<FO_NICE_NAME>/ClientRuntimeHost/`. On the next
> launch an `INSTALLED` host reads and validates that selector before `InitApp`, then loads the writable
> DLL directly. The frozen install-dir DLL remains the fallback when the selector is absent, malformed,
> names a different runtime, or points to neither a live nor staged file. Portable clients never consult
> this selector.

> **Deployed hosts are frozen.** The host `.exe` is never delivered by the updater (only the runtime
> DLL is). A client built before this fix (one that attempted an in-process same-path reload) cannot be
> fixed in place by any server or DLL update — it needs a one-time manual reinstall of a client carrying
> the fix, after which self-updates work again. Updater protocol generation 2 and host/runtime ABI 3
> formed the original hard safety boundary; current protocol generation 3 preserves it. Generation-1
> clients are rejected before any native module transfer,
> and ABI-2 hosts cannot load an ABI-3 runtime. This prevents a frozen unsafe host from reaching a
> second `InitApp`. The frozen generation-1 runtime shows its existing base-client update instruction;
> generation-2 and newer runtimes use the explicit latest-full-package wording below.

## Host CLI surface

```text
LF_Client.exe                                                           # bundled runtime, default compatibility
LF_Client.exe --ClientLibPath <path>                                    # explicit runtime, default compatibility
LF_Client.exe --ClientLibPath <path> --ClientLibCompatibilityVersion <ver>  # explicit runtime, no embedded fallback if ver != built-in
```

The bundled runtime library name is **derived from the host executable name** at startup via
`GetCurrentClientRuntimeLibraryName()` (the exe basename without extension, with `FO_DEV_NAME` as the
low-level fallback). For a portable client, `GetClientRuntimeLivePath()` resolves
`<exe_dir>/<library_name><runtime_ext>`. Renamed/multi-instance hosts therefore each load their own
sibling module (`MyAlt.exe` â†” `MyAlt.dll`) instead of sharing one. For an installed client,
`ResolveBundledRuntimePath()` keeps the same host-derived library name but may place it under the
versioned marker's writable root, as described below. In the build tree, the `LF_ClientLib` target still
writes its canonical `LF_ClientLib.*` artifact and also copies a host-derived alias (`LF_Client.dll` /
`LF_Client.so` / `LF_Client.dylib`) so an unpackaged `LF_Client` exercises the portable loading path.

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

**Linux module isolation.** The runtime `.so` must stay loadable with `dlopen` from an engine host executable that exports its own engine symbols (`-rdynamic` for stack-trace symbolization). Two build rules keep that true. First, engine runtime modules link with `-Wl,-Bsymbolic` (`AddSharedApplication` in [../BuildTools/cmake/helpers/Build.cmake](../BuildTools/cmake/helpers/Build.cmake)), so the module binds global references — the global-data registry, allocator, logging — to its own definitions instead of interposing on the host executable's exported copies; each module keeps private engine state, mirroring the Windows DLL model (without this, the module's `CreateGlobalData` resolves to the host's already-fired copy and the module crashes on its first global-data access). Second, vendored rpmalloc does not force initial-exec TLS on Linux (`(FOnline Patch)` in `ThirdParty/rpmalloc/rpmalloc/rpmalloc.c`): an IE-model TLS relocation makes glibc place the module's entire TLS segment into the limited static TLS surplus at `dlopen`, which fails with `cannot allocate memory in static TLS block`. The host/runtime C ABI keeps allocation ownership module-local (all strings are copied at the boundary), so per-module allocator state is safe.
When signature enforcement is enabled and the staged file is the runtime itself, `Updater` persists the
exact verified signed descriptor beside it as `<live>-staging.auth` before attempting promotion. An
immediate successful replacement removes that sidecar. If the loaded DLL is locked and promotion is
deferred to the thin host, `ApplyStagedBinaryUpdate` refuses to move `<live>-staging` unless the immutable
host can verify the `.auth` envelope with its compile-time
`FO_UPDATE_MANIFEST_TRUSTED_PUBLIC_KEYS` / `FO_UPDATE_MANIFEST_MIN_RELEASE_SEQUENCE` trust root and find
a signed `ClientBinaries` entry whose accepted name, exact size, and SHA-256 match the staged DLL. The
host removes the authorization sidecar only after successful promotion (or when no staged DLL exists),
so an unsigned, wrong-target, unknown-key, below-minimum-release, or byte-swapped staged runtime is never
loaded on the next launch. The descriptor/target/name/size/SHA decision is centralized in the filesystem-independent
`VerifyContentUpdateStagedBinaryAuthorization` helper so the immutable-host rule is covered directly by
focused unit tests in addition to packaged promotion tests. The normal updater/game-client path additionally enforces the persisted
highest-release rollback marker described below; the frozen host sidecar check deliberately uses its
compiled minimum and signature pins because it runs before replaceable settings/runtime code.

A matching PDB (Windows-only, named `<live>.pdb`, e.g. `LastFrontier.dll.pdb`) is staged side-by-side as `<live>.pdb-staging` and usually promotes immediately because PDBs are not held by the loaded runtime module; if it is locked by a debugger or another process, `ApplyStagedBinaryUpdate` retries after the main DLL swap succeeds. The PDB swap is best-effort â€” failure only degrades stack traces, so it never blocks the runtime swap, while the DLL swap remains backup-rename-rollback atomic. The client-side filter accepts a server file whose basename starts with `<runtime_name>.`, so the DLL (`LastFrontier.dll`) and its PDB sibling (`LastFrontier.dll.pdb`) both match and ride the same `UpdateFileTarget::ClientBinaries` channel. **The runtime DLL and its `<live>.pdb` are fetched only together, in binaries mode** (when the DLL is actually being updated) — a client whose DLL is already current does not pull `<live>.pdb` on its own. **The host PDB (`<host_name>.pdb`, e.g. `LastFrontier.pdb`) is also delivered, but the client fetches it only to recover a *missing* local copy and never overwrites a present one.** The host exe is frozen and its PDB is build-specific, so the server's host PDB matches only an up-to-date host: an up-to-date client re-downloads a matching PDB, while an older host's matching local PDB is never clobbered (a non-matching server-build PDB is written only when the local one is absent, where the debugger ignores it by GUID). `accept_binaries` is `_binariesMode || CanSelfUpdateNativeModules(...)`, so host-PDB recovery also works on a normal resource-sync connect.

## Updater protocol

Versioned by `FO_UPDATER_VERSION` ([../Source/Common/Common.h](../Source/Common/Common.h)). Bump it when
the wire format changes or an older updater/host lifecycle is unsafe to continue. Generation 2 first
rejected generation-1 clients before descriptor or binary transfer because their frozen hosts may attempt
an in-process runtime reload. Current generation 3 also rejects every older wire generation rather than
trying to interpret an incompatible descriptor or transport contract. Gameplay compatibility
(`Settings.CompatibilityVersion`) is separate and changes with every build.

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

### Init data and signed descriptor

Sent once after a non-outdated handshake. Init data contains an updater descriptor followed by initial
gameplay state (global properties, synchronized time). With `Network.UpdateManifestSignatureRequired`
enabled, that descriptor is an Ed25519-signed envelope; disabling the setting is an explicit legacy/test
mode in which the descriptor is the raw inner manifest. Both `Updater::Net_OnInitData` and the regular
game-client freshness gate (`ClientEngine::Net_OnInitData`) verify the envelope before deserializing or
trusting any manifest file metadata.

The signed-envelope v1 layout is:

| Field | Type | Notes |
|-------|------|-------|
| `signature` / `version` | `uint32` / `uint16` | `ContentUpdateSignedDescriptorSignature` (`FUSG`) / `ContentUpdateSignedDescriptorVersion = 1` |
| `key_id` | `uint32` | Non-zero id selecting one entry from the configured trusted-public-key set |
| `release_sequence` | `uint64` | Non-zero monotonic release identity; checked against the configured minimum and the highest locally accepted release |
| `binary_target` | `uint16` length + bytes | Exact handshake target such as `Windows-win64`; an empty value is a resource-only wildcard under the rule below; capped at 64 bytes |
| `manifest_size` / `manifest` | `uint32` + exact bytes | Bounded inner `ContentUpdateManifest` payload; its generation and every artifact/source field are covered by the signature |
| `ed25519_signature` | 64 raw bytes | Signature over every preceding envelope byte, including header, key id, release sequence, target, length, and exact manifest bytes |

The server parses `ServerNetwork.UpdateManifestSigningKey` as
`key-id:public-key-hex:private-seed-hex`, requires that the matching
`key-id:public-key-hex` entry is present in `Network.UpdateManifestTrustedPublicKeys`, and refuses to
publish a signed catalog when the key or positive release sequence is invalid. Clients reject an unknown
key id, invalid signature, target mismatch, undersized/oversized payload, or a release below
`Network.UpdateManifestMinimumReleaseSequence`.

Target verification is deliberately asymmetric. A signed envelope whose `binary_target` is empty may
match any client target only when the verified inner manifest contains no `ClientBinaries` entries; this
is the cached common/resource-only descriptor. As soon as a manifest contains any `ClientBinaries`, the
envelope target must exactly equal the client's current binary target. Staged-runtime authorization is
therefore never wildcarded: a valid `<live>-staging.auth` must carry the exact target and a matching
`ClientBinaries` entry before the immutable host will promote the DLL.

After verification, clients persist accepted release identities under the writable `UpdaterTrust/`
root. `AcceptContentUpdateReleaseSequence` serializes threads and processes on `acceptance.lock`, rejects
a sequence below the highest valid marker, and publishes a new marker through a unique pending file,
file flush/`fsync`, atomic rename, and directory sync. A crashed pending file is ignored; a recognizable
but corrupt final marker is quarantined before a later valid release repairs the state. Unreadable or
unpersistable trust state still fails closed. A trusted-key list may contain multiple key ids for rotation;
deployments keep the previous public key in both shipped runtime settings and the immutable host pin until
every base client that may encounter the new signing id has rolled out.

An embedding project may also compile `FO_UPDATE_MANIFEST_DEVELOPMENT_PUBLIC_KEYS` for a public,
non-secret test vector. The immutable host considers those keys only when `IsPackaged()` is false;
credential-free packaged E2E builds must opt in with
`FO_UPDATE_MANIFEST_ALLOW_PACKAGED_DEVELOPMENT_KEYS=1`. Production/package builds must leave that gate
off. This keeps an easy local signed-update path without making a publicly known test seed a production
trust root.

The verified inner manifest describes the files the server is offering for this binary target, their
authoritative SHA-256 digests, optional project-registered external sources, and, when enabled, the UDP
fast-updater mirrors/chunk hashes. The current serialized contract is
`ContentUpdateManifestVersion = 4` (`FO_UPDATER_VERSION = 3`).

Manifest v4 header:

| Field | Type | Notes |
|-------|------|-------|
| `signature` / `version` | `uint32` / `uint16` | `ContentUpdateManifestSignature`, `ContentUpdateManifestVersion` |
| `catalog_generation` | `uint64` | server catalog identity echoed by authenticated client source-health reports; stale reports are ignored |
| `fast_update_enabled` | `uint8` bool | server permits UDP fast transfer for this descriptor |
| `self_hosted_server_enabled` | `uint8` bool | server intends to serve UDP chunks itself; advertised endpoints remain authoritative |
| `session_id` | `uint32` | per-backend fast-updater session guard |
| `chunk_size` | `uint32` | bytes per fast-updater chunk |
| `endpoints` | repeated `{ host, port, priority }` | UDP mirrors parsed from `host:port[:priority]` settings entries |
| `files` | repeated file entries | common resources plus binary-target-specific entries |

Each manifest file entry is:

| Field | Type | Notes |
|-------|------|-------|
| `file_index` | `uint32` | server-assigned index for `GetUpdateFile` and UDP chunk requests |
| `name` | length-prefixed string | client-relative path |
| `size` | `uint64` | full file size |
| `hash` | `uint64` | FNV-1a 64-bit hash of the file content |
| `sha256` | 32 raw bytes | cryptographic final-content digest, authoritative for external-source acceptance |
| `target` | `UpdateFileTarget` (`uint8`) | `ClientResources` or `ClientBinaries` |
| `chunk_hashes` | repeated `uint64` | FNV-1a 64-bit hash for each UDP chunk; omitted when fast update is not advertised |
| `sources` | repeated source entries | optional project-registered candidates, already sorted by priority |

Each external source entry contains bounded length-prefixed `provider`, `source_key`, `transport`, and
opaque `locator` strings, followed by an `int32` priority, an `int64` expiry, and a 16-byte
`ContentUpdateSourceReportToken`. Expiry uses the server's synchronized game-time milliseconds; zero
means no declared expiry. `(provider, source_key)` is unique within one file. The server personalizes
the manifest for each updater connection: it derives every report ticket with HMAC-SHA-256 from a
server secret, the non-zero connection feedback-session id, catalog generation, file index, and the
complete source revision, then signs that personalized manifest. The ticket is opaque, exposes no
locator data, and is accepted at most once on that connection. The `fonline` provider namespace is
reserved for engine-owned delivery, and identifiers, per-file source counts/bytes, locators, and the
total manifest are capped at serialization and deserialization boundaries. Feedback-ticket keys,
Fast UDP cookie keys, and client nonces come from LibreSSL `RAND_bytes`, not implementation-defined
`std::random_device` state.

The inner manifest is capped at 64 MiB and the signed descriptor envelope at 64 MiB + 256 bytes. Both
the updater connection and the regular client reject the declared InitData descriptor length before
resizing their receive vector; the envelope verifier and manifest deserializer repeat their respective
caps for callers that already own a byte buffer. An oversized server value therefore cannot turn into
an attacker-directed client allocation before validation.

Common (gameplay-resource) entries are emitted for every binary target. Per-target binary entries (`UpdateFileTarget::ClientBinaries`) are emitted only for the matching `binary_target` from the handshake. The client then filters binary entries by the current host-derived runtime basename, so `LF_Client.exe` downloads `LF_Client.dll` while `LF_Client_OpenGL.exe` downloads `LF_Client_OpenGL.dll` even though both report the same CPU/OS target.

### External distribution transports

External delivery is an acceleration layer, never a replacement for the authoritative game channel. A project can attach a ready source to an immutable catalog artifact from server scripts. Until registration completes, manifests contain no such source and clients use the existing UDP/direct paths. Registration, refresh, removal, expiry pruning, or provider failure never removes the underlying `GetUpdateFile` artifact.

Client gameplay scripts cannot implement a transport: the updater runs before `ClientEngine` and before the resources containing those scripts are current. Instead, an embedding project's early native `SetupContentUpdateTransportsHook` registers factories into the registry owned by each `Updater` instance. There is no process-global transport registry, so embedded clients and parallel engine instances remain isolated. Unknown transport keys are skipped.

For each file, the client tries candidates in this order:

1. non-expired external sources in manifest priority order;
2. the existing fast UDP path, if enabled and usable;
3. reliable `GetUpdateFile` delivery over the game connection.

A transport downloads only to an engine-selected `~<filename>.__sha256.<digest>.__external.<source_index>` candidate. It reports progress and completion but cannot promote the file. The engine verifies exact size and streaming SHA-256, moves the accepted candidate into the digest-qualified `~<filename>.__sha256.<digest>` path, verifies FNV through the normal finalization contract without recomputing the just-verified SHA-256, and then uses the existing atomic resource replacement or native-binary staging path. The reliable-channel partial is kept separate while an external candidate runs.

Expired sources and unknown transports are skipped without disabling direct delivery. Once an active external download fails, is oversized/truncated, or fails its digest check, the updater opens a circuit for that `provider` for the rest of the updater session. Later files and later source entries from the same provider are skipped immediately, so a provider outage does not repeat a full timeout for every artifact; sources owned by other providers remain eligible. When no external candidate remains, selection continues through the existing UDP path and then reliable `GetUpdateFile`. Full locators are deliberately absent from engine logs because they may contain temporary signed query data.

After an attempted external transfer reaches a verified success or a terminal transport/integrity
failure, the client also sends a fixed-size `ReportUpdateSource` message containing only
`catalog_generation`, `file_index`, the 16-byte connection ticket, and a bounded result enum. It never
sends the locator, provider text, free-form errors, or local paths. The server first consumes that ticket
from the connection's bounded one-shot set, then validates the current generation, HMAC/session/source
binding, source expiry, and enum. A reconnect receives a new session-specific ticket, while the advisory
window still groups observations by distinct remote host so repeated reconnects from one address cannot
grow the distinct-host count.

Feedback is **advisory only**. With `ServerNetwork.UpdateSourceFeedbackEnabled`, the server keeps a
bounded per-source/distinct-host window and logs one redacted warning when the configured minimum report
count and failure percentage are reached. It does not mutate catalog sources, filter provider
descriptors, open a server-side circuit, or start a cooldown/half-open state from client reports. Each
client has already failed over immediately through its local provider-session circuit, then UDP, then
reliable `GetUpdateFile`; future clients continue receiving the currently registered source until the
project publisher removes/refreshes it or its declared expiry prunes it. This avoids turning
client-originated reachability observations into a global availability control plane.

Transport factories receive bounded immutable source/file metadata, the candidate path, cancellation, and progress responsibilities. Protocol-specific behavior such as HTTPS redirects/ranges or a future torrent/magnet implementation belongs to the registered transport, while the engine retains source ordering, final integrity, file promotion, and fallback policy.

### Resumable file transfer

The client drives a single transfer at a time:

```text
client â†’ server: GetUpdateFile  { file_index: uint32, start_offset: uint64 }
server â†’ client: UpdateFileData { update_portion: int32, raw bytes[update_portion] }
```

The server picks `update_portion` (capped by `Network.UpdateFileMaxPortionSize`, currently 5 MB in this project â€” see [LastFrontier.fomain](../../LastFrontier.fomain)). The client requests the next portion with `start_offset = bytes_already_written`, so a partial `~<filename>.__sha256.<digest>` transfer resumes from disk on reconnect without server-side state.

The updater connection also participates in the shared connection-stage protocol. After `InitData`, a
server may send `NetMessage::HashList` (message id 122) to teach clients strings that were previously
reported as unresolved runtime hashes. The updater consumes that message and records the strings in its
private hash storage before continuing resource or binary transfer; `HashList` is not an update-file
payload and does not change the `GetUpdateFile` / `UpdateFileData` state machine.

Server-side validation (in [../Source/Server/UpdaterBackend.cpp](../Source/Server/UpdaterBackend.cpp)):

- `file_index` out of range â†’ `LogType::Warning` + `HardDisconnect`.
- `start_offset > file_size` â†’ `LogType::Warning` + `HardDisconnect`.
- `update_file_max_portion_size <= 0` (misconfiguration) â†’ `LogType::Warning` + `HardDisconnect`.
- Disk-mode read failure â†’ `LogType::Warning` + `HardDisconnect`.
- Disk-mode size drift against the announced descriptor entry - `LogType::Warning` + `HardDisconnect`. With
  `ServerNetwork.UpdateFilesInMemory = False` the descriptor is a start-time snapshot while the bytes are read on
  demand, so a pack replaced under a live server would otherwise reach the client under the hash announced for the
  previous one.

Client-side, the `Updater` writes each portion to `~<filename>.__sha256.<digest>`, verifies both streamed FNV (`fs_hash_file`) and SHA-256 (`fs_sha256_file`, [../Source/Essentials/DiskFileSystem.cpp](../Source/Essentials/DiskFileSystem.cpp)) once complete, then atomically renames over the live file (`ReplaceFileSafely`). The digest-qualified name prevents a partial from an older manifest from being resumed as bytes for a different artifact. Before selecting a source, the updater removes the legacy `~<filename>` family, legacy external/fast sidecars, and temp files for prior digests; current-digest external candidates remain isolated from the reliable channel and are validated before promotion.

If a completed reliable-channel transfer fails FNV or SHA-256 validation, the updater deletes the partial and requests that file exactly once more from offset zero. A second integrity mismatch aborts the update instead of looping. FNV-1a remains the fast corruption/cache/chunk vocabulary (separate from the engine's wyhash-backed `hashing_ex::hash`, which is reserved for hash-tables and `hstring`); SHA-256 is the final trust boundary for accepted updater bytes. Both are streamed, so even multi-GB resource packs never get fully buffered in RAM on either side.

To avoid rehashing existing packs on every startup (the hashing cost dominates the updater's "is this file already current?" pass for multi-GB resource packs), the disk-side hash check goes through `Updater::IsDiskFileHashMatch`, which caches the result in `CacheStorage` ([Settings.CacheResources](../../LastFrontier.fomain)) under the key `<basename>.hash` (so a pack at `<ClientResources>/Embedded.zip` lands as `<CacheResources>/Embedded.zip.hash`). The cached entry stores `(size, mtime, hash)`; the cache lookup is invalidated automatically when either size or mtime changes, so a refreshed pack is always rehashed exactly once. Deleting a `<basename>.hash` file from the cache directory transparently triggers re-hashing on the next updater pass — earlier revisions used the full absolute path as the key, which produced filenames containing the drive-letter colon on Windows and silently failed to write, so the cache never persisted.

There are no backward-compatible fallback paths. The previous "session-state file index + portion counter" protocol was removed when `FO_UPDATER_VERSION` was introduced; clients and servers must agree on the version.

### Fast UDP transfer

The fast updater is an opt-in acceleration path layered over the TCP/game-channel transfer above. The client uses it only when all of these are true:

- `Network.FastUpdateEnabled` is true on the client.
- The manifest has `fast_update_enabled = true`.
- The manifest has at least one endpoint, a non-zero `chunk_size`, and chunk hashes for the file.

Manifest boolean flags must be encoded as `0` or `1`, file targets must be known updater targets, and endpoint ports must be non-zero. When a manifest advertises `fast_update_enabled = true`, deserialization also rejects a `chunk_size` of `0` or one larger than the supported UDP payload range. Endpoint hosts and file names are length-prefixed with `uint16`, and server-side serialization rejects values that do not fit that field before writing the descriptor. Manifest file names must be relative normalized paths: no absolute roots, drive prefixes, backslashes, empty path components, `.`, or `..`. A malformed descriptor is reported as an updater failure instead of escaping the network message handler.

Files whose chunk count does not fit the 32-bit wire field are treated as unsupported by the fast path and continue through the reliable updater. `Network.FastUpdateChunkSize` must be in the supported UDP payload range; oversized or non-positive values disable fast-update advertisement so clients use the reliable updater. Every chunk's initial request goes to the highest-priority endpoint. On timeout or send failure, retries rotate through the ordered endpoint list; the budget permits the configured additional retry count on every endpoint before falling back. Extremely large retry budgets are saturated internally so the attempt counter does not overflow.

Fast UDP uses `ContentUpdateDatagramVersion = 2`. Every socket has a random non-zero client nonce.
Its first chunk request carries no valid cookie; the mirror answers only with a same-size-or-smaller
`CookieChallenge` echoing the session/file/chunk/nonce plus a short-lived 16-byte cookie. The cookie is
the truncated HMAC-SHA-256 of the remote host and UDP source port, manifest session id, client nonce, and
expiry under a process-random server secret. The client validates the echoed fields, caches the cookie
for that socket, and reissues the request. The server sends chunk data only after constant-time cookie
validation and rejects expired or implausibly far-future cookies. Each client tick drains queued socket
replies before applying request timeouts, so a valid challenge that arrived near the deadline is not
discarded merely because the polling thread ran late.

`UpdaterFastClient` downloads authenticated chunks into sidecar files named
`<digest-qualified-temp>.__fastupd.<chunk_index>`. Each received data datagram is checked against the
manifest `session_id`, `file_index`, `chunk_index`, payload size, and 64-bit FNV chunk hash before it is
marked complete. Existing sidecar chunks are size/hash-checked before reuse and rechecked before assembly
or TCP fallback resume. When every chunk is verified, the chunks are assembled into the digest-qualified
temp file, the assembled file is checked against the manifest size/hash, and the existing atomic
rename/staging path finalizes the update.

If UDP setup, send, timeout, receive, write, assembly, or validation fails, the client falls back to `GetUpdateFile` on the reliable game channel. Fully contiguous chunks are rechecked and written into the temp file first, and the TCP request resumes at the byte offset actually written. Non-contiguous or stale sidecar chunks are deleted during fallback cleanup. Client logs include `Fast updater started for ...`, `Fast updater completed for ...`, and `Fast updater failed for ...` markers so packaged headless smoke tests can distinguish the UDP path from reliable fallback.

`UpdaterFastServer` is polled by `ServerEngine` on the main worker when `Network.FastUpdateEnabled &&
ServerNetwork.FastUpdateServerEnabled`, the backend generated fast-update chunk data from at least one
valid advertised endpoint, and a bind port is configured. It validates the session id, enforces the
cookie challenge above, and delegates authenticated chunk reads to
`UpdaterBackend::ReadFastUpdateChunk`, so memory-mode and disk-mode update storage share the same hashes
and file indices as the regular updater.

Unauthenticated traffic can receive only a globally rate-limited cookie challenge, and a strong assert
keeps that challenge no larger than the triggering request: no unauthenticated request amplifies bytes.
Authenticated chunk responses consume both per-address and global token buckets before file I/O; rates
and bursts are independently configured. Address-bucket state is capped at 4096 entries and stale entries
are reclaimed. The mirror remains opt-in/disabled by default, and normal firewall/traffic monitoring is
still required for public operation, but the v2 cookie+budget contract closes the old reflection path in
the in-process server itself.

## Server-side: `UpdaterBackend`

[../Source/Server/UpdaterBackend.h](../Source/Server/UpdaterBackend.h) is owned in-place by
`ServerEngine` as `optional<UpdaterBackend>`; the self-hosted UDP mirror is independently held as
`optional<UpdaterFastServer>`. When `_updaterBackend` is disengaged (an unpackaged development server),
the server rejects `GetUpdateFile` with `HardDisconnect` because there is nothing to serve.

Public API:

```cpp
auto GetUpdateDescriptor(string_view binary_target_name, int64_t current_synchronized_time_ms,
    uint64_t feedback_session_id = 0) -> shared_ptr<const vector<uint8_t>>;
auto BeginContentUpdateFeedbackSession() -> uint64_t;
auto GetFastUpdateSessionId() const noexcept -> uint32_t;
auto IsFastUpdateEnabled() const noexcept -> bool;

auto GetContentUpdateCatalogGeneration() const noexcept -> uint64_t;
auto GetContentUpdateCatalog() const -> vector<refcount_ptr<ContentUpdateArtifact>>;
auto AcquireContentUpdateArtifact(uint64_t generation, uint32_t file_id, const Sha256Digest& expected_sha256) const -> shared_ptr<ContentUpdateArtifactLease>;
auto UpsertContentUpdateSource(uint64_t generation, uint32_t file_id, const Sha256Digest& expected_sha256, ContentUpdateSource source) -> bool;
auto RemoveContentUpdateSource(uint64_t generation, uint32_t file_id, const Sha256Digest& expected_sha256, string_view provider, string_view source_key) -> bool;
auto ClearContentUpdateSources(uint64_t generation, string_view provider) -> bool;
auto ReportContentUpdateSourceResult(uint64_t generation, uint32_t file_id,
    const ContentUpdateSourceReportToken& report_token, uint64_t feedback_session_id,
    ContentUpdateSourceResult result, uint64_t reporter_id, int64_t current_synchronized_time_ms,
    const ContentUpdateSourceFeedbackPolicy& policy) -> ContentUpdateSourceFeedbackDecision;

void LoadFromClientResources(const GlobalSettings& settings, string_view server_metadata_version);
void ProcessUpdateFile(ptr<Player> player, int32_t update_file_max_portion_size);
void ProcessContentUpdateSourceReport(ptr<Player> player, int64_t current_synchronized_time_ms,
    const ServerSettings& settings);
auto ReadFastUpdateChunk(uint32_t file_index, uint32_t chunk_index,
    vector<uint8_t>& data, uint64_t& chunk_hash) const -> bool;
```

- `LoadFromClientResources` walks `Settings.ClientResources`, picks every pack listed in `Settings.ClientResourceEntries` (excluding `Embedded`), then enumerates `Settings.PlatformBinaries/<target>/` for per-target binaries (default `PlatformBinaries/`, sibling of `Resources/` in the package layout).
- Entries are stored as immutable storage owners plus `MemoryData` or one opened disk-storage object,
  `Size`, FNV `Hash`, `Sha256`, and optional UDP `ChunkHashes`. Memory mode keeps the whole pack in
  `MemoryData` for the lifetime of the server. Disk mode copies each source pack into a private temporary
  disk snapshot while constructing the catalog, opens that snapshot once, computes every hash through the
  opened object, and retains it for all direct portions, UDP chunks, and publisher leases. All disk-backed
  files in one catalog share one snapshot root and one `ContentUpdateSnapshotOwner`; every storage object
  retains that owner, while publisher leases retain their storage. In-place source writes and atomic
  same-path deployment replacements therefore cannot switch bytes underneath an existing
  catalog; a later catalog load snapshots and identifies the replacement. Disk mode avoids pack-sized RAM
  residency but requires temporary disk space for one full catalog (and briefly both catalogs during reload).
  That shared snapshot root carries one versioned `owner.lock` marker held under an exclusive OS lock until
  the last catalog storage/publisher lease releases the owner. Normal owner destruction removes the whole
  root. Before building a disk-backed catalog, the bounded scavenger scans only recognizable marker-bearing
  directories: a lock that can be acquired proves the former owner is gone, so that orphan is removed;
  locked snapshots and unknown/prefix-colliding directories are preserved. This reclaims crash leftovers
  without deleting snapshots still served by another engine/server process.
- `VerifyClientResourcesMetadata` then mounts the client packs and compares their metadata version against the
  one the server itself loaded. The server runs on `Settings.ServerResources` and hands out
  `Settings.ClientResources`, so a deploy that refreshed only one of them would leave every synced client with a
  property layout the server cannot talk to; startup fails with `UpdaterException` naming both versions instead.
- Each successful catalog load receives a non-zero, monotonically increasing generation. Artifact ids
  are opaque within that generation; asynchronous publishers must bind completion to generation, file id,
  and expected SHA-256. A stale generation, unknown id, or digest mismatch returns `false` and cannot attach
  a locator to newer bytes.
- `ContentUpdateArtifactLease` is the native publisher boundary. It retains immutable memory storage or an
  open disk handle and exposes exact, bounds-checked `Read(offset, span)` calls, so project extensions never
  need server filesystem paths and use the same API in memory and disk modes.
- External sources are provider-neutral descriptors. Upsert identity is `(provider, sourceKey)`; remove is
  additionally digest-bound, while provider-wide clear is generation-bound. The common validator enforces
  identifier, locator, count, byte, expiry, and reserved-namespace limits before publication. Exact upserts
  are idempotent. A provider must serialize changes for the same `(provider, sourceKey)`; generation and
  expected SHA-256 reject results from an older artifact catalog. Material upsert builds and validates the
  replacement descriptors before committing the catalog or feedback reset, so a
  failed rebuild preserves the previously published source and its health state.
- Descriptor updates use copy-on-write catalog state. `GetUpdateDescriptor` returns a shared immutable byte
  snapshot, and the handshake retains that owner through `Send_InitData`; a concurrent source refresh cannot
  invalidate a span or mix two descriptor versions. Common-resource entries are merged into every target,
  while unknown targets receive the common-only descriptor. Per-target source changes clone only that file's
  source list and rebuild only affected target descriptors; common-file changes necessarily rebuild every
  descriptor because common files are present in every target.
- Every updater handshake starts a non-zero feedback session on its `ServerConnection`.
  When the catalog has no external sources, `GetUpdateDescriptor(..., feedback_session_id)` returns the
  cached raw or already-signed common/per-target descriptor directly; it does not deserialize, personalize,
  or sign per connection. With sources present, the backend captures the immutable catalog/descriptor
  owners while holding `_catalogLocker`, releases the lock, then clones the raw manifest to insert
  per-source 16-byte HMAC tickets and signs the personalized descriptor outside the lock. The
  connection consumes each ticket at most once and caps remembered tickets; replay on the same connection
  or reuse after reconnect is ignored.
- `expiresAt` is expressed in `Game.SynchronizedTime` milliseconds, with zero meaning no declared expiry.
  A handshake passes the current synchronized time to the backend; expired sources are pruned before its
  descriptor is returned. Feedback arriving at or after that expiry is ignored even if no subsequent
  handshake has yet triggered physical pruning.
- The server script surface is `Game.GetContentUpdateCatalog`, `Game.UpsertContentUpdateSource`,
  `Game.RemoveContentUpdateSource`, and `Game.ClearContentUpdateSources`. `Game.OnContentUpdateCatalogReady`
  fires after module initialization for packaged servers, so a handler can queue background publication.
  The event result is deliberately not a server-readiness gate.
- Script/native source registration only changes advertised acceleration routes. It never removes
  `GetUpdateFile`: the reliable game-channel transfer remains available while publishers are preparing data
  and after any external transport failure.

## Settings

| Setting | Where | Purpose |
|---------|-------|---------|
| `Network.UpdateFileMaxPortionSize` | top-level | Maximum bytes per `UpdateFileData` response. Drives both transfer throughput and per-message memory pressure. Default 1 MB (engine) / 5 MB (this project). |
| `Network.UpdateManifestSignatureRequired` | top-level | Requires the InitData updater descriptor to use the Ed25519 envelope. When true, the server also requires a valid signing key/release sequence and clients reject raw manifests. |
| `Network.UpdateManifestTrustedPublicKeys` | top-level | Trusted `key-id:public-key-hex` entries. Multiple ids support key rotation; the active server signing key must match one entry exactly. |
| `Network.UpdateManifestMinimumReleaseSequence` | top-level | Positive floor for signed releases. The updater/game client also persist the highest accepted sequence and reject rollback below it. |
| `Network.FastUpdateEnabled` | top-level | Enables advertising/using the UDP fast updater. Both client and server-side manifest generation check this before using UDP. Last Frontier keeps this explicit and disabled in the base config until UDP mirror ports/endpoints are provisioned. |
| `Network.FastUpdateChunkSize` | top-level | UDP payload chunk size. Default 1024 bytes; supported range is 1..60 KiB. Values outside this range disable fast-update advertisement. |
| `Network.FastUpdateRequestTimeout` | top-level | Client timeout in milliseconds before retrying a requested UDP chunk. |
| `Network.FastUpdateMaxRetries` | top-level | Additional retry count per endpoint. Initial requests use the highest-priority endpoint; later attempts rotate endpoints until each has received its configured retry budget, then the client falls back to reliable transfer. |
| `Network.FastUpdateMaxSockets` | top-level | Requested concurrent UDP socket count, clamped to `1..64` and then limited by the file's chunk count. Concurrency is independent of endpoint count: even one advertised mirror can serve many chunks in parallel, and every socket initially uses the highest-priority endpoint. |
| `ServerNetwork.FastUpdateServerEnabled` | top-level | Starts the in-process UDP chunk mirror when packaged updater data is available. |
| `ServerNetwork.FastUpdateBindHost` / `ServerNetwork.FastUpdateBindPort` | top-level | UDP bind address for the self-hosted fast updater. Values outside `1..65535` keep the in-process mirror stopped and the manifest `self_hosted_server_enabled` flag false, while valid external `FastUpdateEndpoints` may still be advertised. |
| `ServerNetwork.FastUpdateEndpoints` | top-level | Advertised UDP endpoints in `host:port[:priority]` format. Clients sort higher priority first. |
| `ServerNetwork.FastUpdateCookieLifetimeSeconds` | top-level | Address/port-bound v2 cookie lifetime, clamped to `5..300` seconds. |
| `ServerNetwork.FastUpdateChallengeRateLimit` | top-level | Global maximum same-size unauthenticated cookie challenges per second, clamped to `1..1,000,000`. |
| `ServerNetwork.FastUpdateGlobalRateLimit` / `FastUpdateGlobalBurst` | top-level | Global authenticated UDP response token-bucket rate and burst in bytes. |
| `ServerNetwork.FastUpdatePerAddressRateLimit` / `FastUpdatePerAddressBurst` | top-level | Per-remote-host authenticated UDP response token-bucket rate and burst in bytes. |
| `ServerNetwork.UpdateManifestSigningKey` | top-level | Active `key-id:public-key-hex:private-seed-hex` Ed25519 signing key. Treat as a deployment secret; it must match the trusted public-key set. |
| `ServerNetwork.UpdateManifestReleaseSequence` | top-level | Positive monotonic release sequence written into every signed descriptor; bump for every published client release. |
| `ServerNetwork.UpdateSourceFeedbackEnabled` | top-level | Enables authenticated one-shot client reachability reports and advisory aggregation/logging. Disabling it does not affect immediate client-local fallback. Feedback never suppresses sources. |
| `ServerNetwork.UpdateSourceFeedbackMinReports` | top-level | Minimum distinct client hosts before the advisory failure threshold can be logged; runtime-clamped to `2..64` (Last Frontier default: `5`). |
| `ServerNetwork.UpdateSourceFeedbackFailurePercent` | top-level | Failure percentage required for the advisory threshold log; runtime-clamped to `1..100` (Last Frontier default: `60`). |
| `ServerNetwork.UpdateSourceFeedbackWindowSeconds` | top-level | Advisory aggregation window, clamped to `1..86400` seconds (Last Frontier default: `300`). |
| `ServerNetwork.UpdateFilesInMemory` | top-level + `[SubConfig]` | `True` keeps every packaged update file in RAM (low CPU under load). `False` serves from disk on demand (low RAM, more I/O). Public `[SubConfig]`s in this project: `PublicGame = True`, `DailyTest = True`, `Staging = True`. |
| `Network.ForceMetadataVersion` | top-level | Testing only: overrides the layout version the client reports, so a divergence can be simulated without a second bake. Empty in every shipped config. |
| `Baking.PlatformBinaries` | top-level | Directory the server reads per-target client runtime libraries from, and the packager writes them to. Default `PlatformBinaries`, resolved relative to the server's working directory / package root. |
| `Client.UserWritablePath` | client | Writable data root for cache/log/update overlays. A valid versioned `INSTALLED` marker is authoritative and selects `<GetUserDataBase>/<marker GameName>`. Without one, empty = portable, `*` = the per-OS user data dir plus `Common.GameName`, and another value is an explicit path. Resolution runs again after local-config/command-line overrides. See below. |

There is no auto-detection of memory vs disk mode in C++. Choose explicitly per environment.

## Installed vs portable writable data

A **portable** build writes its cache, log, and self-update files next to the exe — fine for a zip the
user unpacks anywhere. An **installed** build (MSI in `Program Files`, a package under `/usr/...`) sits
in a read-only directory, so those writes must go to a per-user writable location instead.

`Client.UserWritablePath` selects the model, resolved at startup by `ResolveUserWritablePath(settings)` (`Source/Frontend/ApplicationInit.cpp`, called from `LoadAppSettings`):

- **empty → portable** (default): writable paths stay relative to the exe / working dir (unchanged behaviour).
- **`*` → per-OS user data dir** (`Platform::GetUserDataBase()` via env, no SDL/shell32 dependency): Windows `%LOCALAPPDATA%`, macOS `~/Library/Application Support`, Linux `$XDG_DATA_HOME` or `~/.local/share`, then `/<Common.GameName>`.
- **explicit path** → that absolute writable root.

The pre-settings host selection described below is defined for the versioned installed-marker layout.
An arbitrary explicit `Client.UserWritablePath` remains valid for runtime data, logs, and resource
overlays, but the thin host cannot discover that setting before loading a DLL; use an explicit
`--ClientLibPath` when probing a native runtime at such a custom location.

Resolution is idempotent, creates the directory + the `Cache`/`<ClientResources>` subdirs, and is
**fail-safe**: if the dir can't be determined or created it logs a warning and reverts to portable, so a
bad install config never bricks startup.

What moves to the writable root (via the free path helper `fs_make_writable_path(UserWritablePath, relative)`
in `DiskFileSystem.cpp`): the **cache** (`CacheStorage` in `ApplicationInit`/`Client`/`Updater` — login keys, native
secure storage, local config), the **log** file (resolved from the marker before host/runtime startup, then
confirmed after settings load), **self-update resource
patches** — the updater writes them under `<root>/<ClientResources>` and layers that dir on top of the
read-only install-dir base as a higher-priority resource source (`Updater.cpp`, `Client.cpp`), so the base
resources are read from the install dir and patches override from the user dir — and the **self-updated native
runtime** (see below).

The installed package carries a versioned `INSTALLED` marker next to the executable:

```text
FONLINE_INSTALLED_CLIENT_V1
<Common.GameName>
```

`package.py::make_wix_installer` writes exactly those two lines plus the final newline. The shared marker
parser caps the file at 256 bytes and the UTF-8 directory component at 128 bytes; it rejects `.`, `..`,
Windows-invalid/control characters, reserved device names (`CON`, `NUL`, `COM1`, `COM`/`LPT` plus Unicode superscript 1, 2, or 3, and peers), trailing
dots/spaces, path separators, drive-colons, and line breaks. The packager applies the same validation.
Both `ResolveUserWritablePath` and the thin pre-settings host consume that one parsed root, so a later
display `Common.GameName` change cannot split resource/update writes from runtime selection. A valid
installed marker is authoritative even after local-config/command-line application; malformed and legacy
presence-only markers do not activate the installed layout and require reinstall. The host and runtime
open their startup log under that root before loading a DLL, so promotion/selection diagnostics do not
depend on write access beside an executable installed under Program Files.

**Native binary self-update for installed builds writes the runtime into the writable root**
(`Updater.cpp`). The pre-settings host derives `<root>/<runtime_name><ext>` from the versioned marker and
passes that full live path through the runtime ABI; portable mode passes the exe sibling instead. A
self-updated runtime therefore lands at `<root>/<runtime_name><ext>` (mirroring
the install-dir layout, `<exe_dir>/<runtime_name><ext>`) alongside its `-staging` and `<...>.pdb` siblings. It
is **not** gated off — both portable and installed clients self-update on every platform where
`CanSelfUpdateNativeModules()` is true.

Because the host resolves and loads the runtime DLL *before* settings (so it cannot compute `<root>` itself —
`Common.GameName` is only known after `InitApp`), the runtime returns the writable live path through
`ClientRuntimeResult::RequestedRuntimePath`. The host promotes that path, validates that it is absolute and
has the current executable-derived runtime filename, writes it to the installed-client bootstrap selector,
and exits; it never loads it again in the same process. On the next launch the `INSTALLED` host reads the
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

[../BuildTools/package.py](../BuildTools/package.py) does both halves:

- **Client packages** include the host exe (e.g. `LF_Client.exe`) and the matching runtime library renamed to the same basename next to it (`LF_Client.dll`). The host derives the library name from its own exe basename at startup, so no config patching is required to point one at the other.
- **Server packages** also stage every available client runtime library under `<Settings.PlatformBinaries>/<binary_target>/<output_name><runtime_ext>` (default `PlatformBinaries/`, sibling of the client-resources dir in the package layout) so a different-platform client connecting to this server can self-update its native modules.
- **Windows Client packages with the `Wix` pack** build an additive MSI from the already-staged Raw client payload. `package.py::make_wix_installer` writes a temporary WiX JSON config, adds the `INSTALLED` marker only while the MSI payload is generated, and registers the product URI scheme through HKCU registry entries. If WiX/wixl or the generator is missing, this step logs a warning and leaves the Raw/Zip artifacts intact.
- **PDBs for Windows runtime DLLs** are shipped under `<runtime_dll>.pdb` (e.g. `LastFrontier.dll.pdb`) â€” both next to the bundled client DLL and inside every server-staged `PlatformBinaries/Windows-*` payload. The host exe keeps its own `<host_name>.pdb` so the two namespaces never collide. `package.py` patches the CodeView (`RSDS`) record in place to point at the new PDB filename — for the renamed runtime DLL (`copy_runtime_pdb`) **and** for the host exe (`<name>.pdb`, patched at the `copy_pdb` call site) — so DbgHelp / `backward-cpp` resolve symbols automatically without relying on the build-machine path baked into the binary. Missing PDB inputs or failed RSDS patches `assert` immediately during packaging â€” symbol gaps are never silently tolerated.
- **The host PDB is delivered for missing-copy recovery only.** `package_all_client_runtime_update_payloads` stages the host's own `<name>.pdb` alongside the runtime DLL and its `<name>.dll.pdb` under `PlatformBinaries/<target>/`. The host exe is frozen and never delivered, so its PDB is build-specific and the server only carries its *current* build's host PDB. The client therefore fetches the host PDB **only when its local copy is missing** and **never overwrites a present one** (`Updater.cpp` skips the `<runtime_local_prefix>.pdb` entry when the file already exists, in either resource-sync or binaries mode). An up-to-date host re-downloads a matching PDB; an older host's matching local PDB stays untouched (and only if the player deleted it does the client write the current, non-matching one, which the debugger ignores by GUID). This recovers a deleted host PDB without ever clobbering a good one — the clobber that an unconditional host-PDB delivery used to cause for self-updated clients (frozen old host + newer server host PDB).

Both the bundled runtime library in client packages and the runtime libraries staged for server-side binary updates go through the same package-time patching as ordinary executables: embedded resources, internal config, and packaged mark are written by `package.py`. Variant-specific config is applied to the runtime payload that actually runs the game; for example the Windows OpenGL runtime receives `ForceOpenGL=1`. The embedded-resource zip is produced with pinned entry timestamps and permissions (`make_embedded_pack`), so the bundled-client copy of a runtime and the matching `<Baking.PlatformBinaries>/<target>/<output_name><ext>` payload remain byte-identical across separate Server/Client package runs.

Client resource zips are written with the same stable entry metadata and sorted normalized paths. This matters because the baker touches unchanged output files during incremental runs; package output must ignore those mtimes so a content-identical repack keeps the same FNV hash in the updater descriptor and does not force clients to redownload every pack. `../BuildTools/tests/test_package_zip_determinism.py` covers the mtime/order invariant.

The internal config patch area has a fixed engine-owned capacity of 10000 bytes; embedding projects cannot resize it. `package.py` discovers the reserved size from the generated binary markers before writing bootstrap config data.

Naming convention from `build_runtime_update_target_name` in `BuildTools/package.py`:
- `Windows-win64`, `Linux-x64`, `Linux-arm64`, `macOS-arm64`, `Android-arm64`, etc.
- Profiling variants get the `_Profiling` suffix in the staged file name.
- The Windows OpenGL variant (`OGL`) is staged separately and patches `ForceOpenGL=1`.
- Entries tagged with a `FO_BINARY_OUTPUT_POSTFIX` (e.g. `Client-Linux-x64-Steam`, `Client-Windows-win64-Steam`) are staged under the same `PlatformBinaries/<target>/` directory as the default variant, but `package_all_client_runtime_update_payloads` appends `_<postfix>` to every staged payload name (`LastFrontier_Steam.so`, `LastFrontier_Headless_Steam.so`, …) so the variants don't clobber each other. `extract_binary_entry_postfix` parses the postfix out of `Client-<platform>-<arch>[-Profiling_X][-Debug][-<postfix>]`. The matching Client package builds with the same `FO_BINARY_OUTPUT_POSTFIX` and the client-side packager mirrors the suffix in `bin_out_name` so the patched `PACKAGED_BUILD_NAME` lines up with the server-side payload name — that's what `Updater.cpp::remap_runtime_name` keys on (`runtime_server_prefix = GetPackagedRuntimeName()`).

## Lifecycle

```
LF_Client.exe main
    â”œâ”€â”€ ResolveRequestedClientRuntime(argc, argv)        # CLI path, installed writable path, or exe sibling
    â”‚
    â”œâ”€â”€ RunClientFromLibrary(argc, argv, requested, *)   # CASE 2: bundled runtime exists
    â”‚     â”œâ”€â”€ ApplyStagedBinaryUpdate(requested.Path)    # promote <requested>-staging (no-op when missing)
    â”‚     â”œâ”€â”€ Platform::LoadModule + FO_QueryClientRuntimeExports
    â”‚     â”œâ”€â”€ Validate exports + metadata
    â”‚     â”œâ”€â”€ exports.Run(argc, argv, &result)           # DLL drives RunClientRuntime:
    â”‚     â”‚     â”œâ”€â”€ single Updater (UI) connects to the server. The connect result picks the mode:
    â”‚     â”‚     â”‚     â”œâ”€â”€ Success         â†’ resources mode â†’ sync ClientResources, finish ResourcesReady
    â”‚     â”‚     â”‚     â””â”€â”€ CompatibilityOutdated:
    â”‚     â”‚     â”‚             â”œâ”€â”€ if !CanSelfUpdate    â†’ finish PlatformUnsupported, caller shows store msg
    â”‚     â”‚     â”‚             â””â”€â”€ else                  â†’ binaries mode â†’ write ClientBinaries to
    â”‚     â”‚     â”‚                                          `<live>-staging`, try immediate promote, or verify `<live>`,
    â”‚     â”‚     â”‚                                          finish BinariesStaged
    â”‚     â”‚     â”œâ”€â”€ On BinariesStaged: preserve Updater::GetRuntimeLivePath exactly;
    â”‚     â”‚     â”‚     GUI holds the restart prompt until quit, headless continues immediately;
    â”‚     â”‚     â”‚     set ResultKind = ReloadRequested and RequestedRuntimePath
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
| Signed descriptor is invalid, untrusted, or for another target | client log `Invalid content update descriptor: ...`; updater aborts before manifest metadata or external locators are used |
| Signed release is below the configured/persisted trust floor | client log reports release rollback or inability to persist trust state; the descriptor is rejected before file selection |
| Gameplay version mismatch on Web / iOS / Android | message box `Client outdated, please update via your app store`, then quit (no in-process self-update on these platforms) |
| Wrong file index / offset | server log `Wrong file index N, from host '...'` / `Wrong update file offset O, file index N, client host '...'` (both at `LogType::Warning`), client gets disconnected |
| Client data does not match the server data | server log `Connected client X runs metadata version A while the server runs B`; updater log `synced resources run metadata version A while the server runs B, resources <dir>`. Both name the two versions - find which resource directory came from a different bake |
| Server distributing resources it does not run on | server startup fails with `Distributed client resources were baked apart from the server resources`, naming both resource directories and both layout versions |
| Baked resources predate the current metadata format | startup fails at the metadata header: `does not start with the metadata file marker`, `file version does not match the engine`, or `carries no version` - run a full rebake |
| Server has no native update for this target | message box `Server doesn't provide a native client update for binary target <target>` |
| Stale staging file | `<live>-staging` survived a previous failed swap; the next `LF_Client.exe` startup promotes it via `ApplyStagedBinaryUpdate` before loading the runtime |
| Linux host logs `LoadModule failed` for a present, valid runtime `.so`, then `trying embedded fallback` on every launch | `dlopen` rejected the module. Two engine build rules must hold (see "Linux module isolation" above): the module is linked with `-Wl,-Bsymbolic` (`AddSharedApplication`), and no vendored code forces initial-exec TLS on Linux — an IE-model TLS relocation fails `dlopen` with `cannot allocate memory in static TLS block` (diagnose with a standalone `dlopen` of the `.so`, e.g. via `python3 -c "import ctypes; ctypes.CDLL('./<runtime>.so')"`). A silently-engaged embedded fallback makes a native self-update loop: the downloaded `.so` is promoted on disk but never executed |
| Self-update downloads and then waits on the update screen | This is the expected native flow. Close the client after the restart prompt; the host promotes the staged runtime and exits, and the next user launch starts the updated runtime with one clean `InitApp`. Hosts predating this policy were cut off by updater generation 2 / runtime ABI 3; current generation 3 rejects all older updater wire contracts and requires the latest full client package instead of attempting an unsafe or incompatible continuation |
| Host rejects a staged runtime authorization | inspect `<live>-staging.auth`, host compile-time key ids/minimum release, descriptor target, and the staged DLL size/SHA-256/name. The host logs `staged DLL authorization rejected` and leaves the live DLL untouched. |
| Stack trace shows raw addresses for the new runtime DLL | After a binary self-update the renamed `<live>.dll`'s CodeView entry must reference its sibling `<live>.dll.pdb`. If `package.py` skipped the RSDS patch (it will assert when this happens), `dbghelp`/`backward-cpp` cannot find the PDB and frames in the runtime resolve to addresses only |
| Stack trace shows raw addresses for **host** (`<host>.exe`) frames after a self-update, while runtime-DLL frames resolve | The on-disk `<host_name>.pdb` doesn't match the frozen exe (CodeView GUID differs) — typically a leftover from an old updater build that clobbered the matching host PDB with a newer server-build one. The current updater never overwrites a present host PDB and fetches one only when the local copy is missing, so the fix is to delete the mismatched `<host_name>.pdb`: an up-to-date host then re-downloads the matching one; otherwise restore the host PDB shipped with that exe build (matching CodeView GUID). A mis-walked stack through unsymbolized host frames can also surface bogus top frames (e.g. attributing the fault to an unrelated system DLL) |

Local validation steps:

1. Build `LF_UnitTests` and run it. [../Source/Tests/Test_ClientRuntimeApi.cpp](../Source/Tests/Test_ClientRuntimeApi.cpp) exercises the ABI surface plus installed-runtime selector round-trip, validation, live selection, staged recovery, and fallback; [../Source/Tests/Test_ContentUpdater.cpp](../Source/Tests/Test_ContentUpdater.cpp) covers SHA-256, manifest v4, signed descriptors, release rollback, feedback tickets, source mutation, artifact leases, snapshot ownership, staged authorization, Fast UDP v2, and malformed inputs; [../Source/Tests/Test_ContentUpdateTransport.cpp](../Source/Tests/Test_ContentUpdateTransport.cpp) covers per-updater transport registration, factory isolation, and failure containment; [../Source/Tests/Test_DiskFileSystem.cpp](../Source/Tests/Test_DiskFileSystem.cpp) covers hashes, file SHA-256, and writable paths; [../Source/Tests/Test_Platform.cpp](../Source/Tests/Test_Platform.cpp) covers `Platform::GetUserDataBase`; [../Source/Tests/Test_Settings.cpp](../Source/Tests/Test_Settings.cpp) covers `UpdateFilesInMemory` inheritance and `ResolveUserWritablePath` fail-safe behavior.
2. Build `LF_Client`; its native target dependency also builds `LF_ClientLib`. Confirm the client output directory contains the host plus the host-derived runtime alias (`LF_Client.exe` + `LF_Client.dll` on Windows, `LF_Client` + `LF_Client.so` on Linux). Build `LF_ClientLib` explicitly when validating the runtime target in isolation.
3. Launch `LF_Client.exe` with the bundled runtime present â†’ normal startup (Case 2 happy path: load DLL, resource updater finishes, game starts).
4. Launch `LF_Client.exe --ClientLibPath <path>` with a valid alternate runtime â†’ host routes through the loaded library.
5. Launch `LF_Client.exe --ClientLibPath <path> --ClientLibCompatibilityVersion <other>` and remove the runtime â†’ host fails (no fallback).
6. Point `--ClientLibPath` to an invalid path, no `--ClientLibCompatibilityVersion` â†’ host falls back to embedded client (Case 1).
7. Build a packaged server (e.g. `Daily`) and confirm `<Settings.PlatformBinaries>/<target>/<name><ext>` (default `PlatformBinaries/`, sibling of the client-resources dir in the package layout) contains the per-target runtime libraries and that `ClientResources` pack list contains the resource zips.
8. Interrupt a client mid-download (kill the network) and reconnect â€” the next `GetUpdateFile` resumes from the matching digest-qualified temp-file size, with no full re-download. Change the manifest digest and confirm the old partial is removed rather than resumed.
9. Force a Case 2 binary update: package an actually older client or use a compatibility override for the first run only. The GUI updater must hold the restart message without loading another module; close it and confirm the host promotes the live destination and exits. Relaunch normally without the override: the host must load the promoted DLL as its only `InitApp` and reach the game.
10. Crash recovery: kill the host while the binary updater UI is mid-download. Restart `LF_Client.exe`. `ApplyStagedBinaryUpdate` runs at the start of `RunClientFromLibrary`; if `<live>-staging` is fully written it gets promoted, otherwise the runtime's resume logic completes the download in a normal updater session.
11. Installed-layout smoke: build an installed package with its install directory treated as read-only. The first outdated launch must write/promote the DLL under the per-user `<GetUserDataBase>/<GameName>` root while leaving the install-dir DLL untouched. Two subsequent ordinary launches must both select the writable DLL and enter the game without another binary-update loop.
12. Fast updater: set `Network.FastUpdateEnabled = True`, `ServerNetwork.FastUpdateServerEnabled = True`, a non-zero `ServerNetwork.FastUpdateBindPort`, and a matching `ServerNetwork.FastUpdateEndpoints` entry. Confirm the v2 client first receives a same-size cookie challenge, only an authenticated retry receives chunk data, global/per-address budgets cap throughput, the server log reports the UDP mirror startup, the client logs `Fast updater started for ...` / `Fast updater completed for ...`, and a forced endpoint failure logs `Fast updater failed for ...` before falling back to `GetUpdateFile` without corrupting the temp file.
13. Staged-runtime authorization: force the loaded-DLL path to leave `<live>-staging` locked for host promotion and confirm `<live>-staging.auth` contains the exact signed descriptor. Tamper the descriptor, target, signing id, release floor, or staged DLL bytes in isolated copies and confirm the host rejects promotion and leaves the current live DLL untouched; restore the valid pair and confirm promotion consumes the sidecar.

## See Also

- [BuildAndLaunch.md](../../Docs/BuildAndLaunch.md) â€” build / package commands and launch profiles.
- [Architecture.md](../../Docs/Architecture.md) â€” engine + game build layout, target table.
- [Debugging.md](Debugging.md) â€” debugger setup; the host vs runtime split affects which binary the debugger should attach to.
- [SteamIntegration.md](../../Docs/SteamIntegration.md) â€” alternative distribution channel that bypasses the in-game updater.
