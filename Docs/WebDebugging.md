# Web Debugging

> Engine-owned documentation. Paths under `../` are relative to the FOnline engine root. Paths under `../../` point to an embedding game project such as Last Frontier when this engine is used as a submodule.

## Local web workflow

The current web-debug flow is task-driven, but the tasks are thin wrappers around the shared BuildTools layer.

- on Linux, `../BuildTools/prepare-workspace.sh web` delegates to `buildtools.py prepare-host-workspace linux web` and prepares only workspace-local web parts; use `web-packages` explicitly on a fresh host that still needs system packages
- on Windows, `../BuildTools/prepare-win-workspace.ps1 web` delegates to `buildtools.py prepare-host-workspace windows web`
- web client builds go through `buildtools.py build web client <Config>`
- browser packaging goes through `buildtools.py package-web-debug LF RemoteSceneLaunch LocalTest`

That last point matters: the current local/scene packaging task generates **both** debug package variants in one pass, not just the one you are about to launch. The staging task is separate and packages only the `Staging` variant.

### Version pinning

- Emscripten version is read from `../ThirdParty/emscripten`.
- The SDK is installed into `Workspace/emsdk`.
- Shared workspace preparation is handled by `../BuildTools/buildtools.py`.

### Key commands

Prepare the shared web workspace parts directly through BuildTools:

```powershell
py -3 ../BuildTools/buildtools.py prepare-host-workspace windows web
```

Linux wrapper equivalent on a provisioned host:

```bash
bash ../BuildTools/prepare-workspace.sh web
```

Fresh Linux host with web system packages:

```bash
bash ../BuildTools/prepare-workspace.sh web-packages web
```

Build the web client for the local debug workflow:

```powershell
py -3 ../BuildTools/buildtools.py build web client RelWithDebInfo
```

Build an optimized browser release:

```powershell
py -3 ../BuildTools/buildtools.py build web client Release
```

Create the browser debug packages used by the VS Code launch configurations:

```powershell
py -3 ../BuildTools/buildtools.py package-web-debug LF RemoteSceneLaunch LocalTest
```

Current package layout:
- `Workspace/web-debug/LF-Client-LocalTest-Web`
- `Workspace/web-debug/LF-Client-RemoteSceneLaunch-Web`
- `Workspace/web-debug/LF-Client-Staging-Web` for the staging task flow

`package-web-debug` currently packages with `-pack Raw+WebServer`, which is why each output directory includes a generated `web-server.py` used by the launch tasks.

### VS Code launch

Current `../../.vscode/launch.json` entries are:

- `Debugging :: Launch Web [windows]`
- `Debugging :: Launch Web [linux]`
- `Debugging :: Launch Web Scene [windows]`
- `Debugging :: Launch Web and Attach [windows]`
- `Debugging :: Launch Web and Attach [linux]`
- `Debugging :: Launch Web Scene and Attach [windows]`

These use `Workspace/web-debug/LF-Client-LocalTest-Web` for the local web client flow and `Workspace/web-debug/LF-Client-RemoteSceneLaunch-Web` for the remote-scene web flow.

The launch flow performs these steps:

1. Prepare the host workspace for web builds.
2. Bake resources.
3. Build the headless server for the current platform.
4. Build the Emscripten web client.
5. Run one packaging step that refreshes both `RemoteSceneLaunch` and `LocalTest` browser packages under `Workspace/web-debug/`.
6. Start the local headless game server.
7. Start the generated `web-server.py` HTTP server for the packaged browser build on port `7000`.
8. Open a dedicated Chrome instance pointing at the local package.

The hidden stop tasks for the web flow currently kill listeners on ports `7000`, `4025`, and `4026`, so stale browser-package or headless-server processes are cleaned up by port rather than by process identity.

The browser client uses the existing local server defaults from `../../LastFrontier.fomain`, including `ServerHost = localhost` and `ServerPort = 4025`.

The generated `index.html` (from `BuildTools/web/default-index.html`) parses the page URL's query string into engine command-line arguments before the runtime starts: each `key=value` becomes a `--key value` argument pair on `Module.arguments`. This makes the web client configurable per launch the same way native clients accept CLI flags — for example `index.html?ClientNetwork.ServerHost=127.0.0.1&Network.ServerPort=4025&Auth.AutoLoginName=...`. With no query string the client runs on its baked configuration unchanged. (Headless-browser automation relies on this to point the client at an ephemeral server port.)

The web scene workflow uses the `RemoteSceneLaunch` subconfig and starts `LF_ServerHeadless` with `--Scene.Startup <SceneId>`, so the server stays headless while its output remains visible in the terminal.

The Linux staging web workflow is task-only rather than a Chrome debug launch entry. `Web :: Prepare Staging Launch [linux]` prepares the web workspace, bakes resources, builds `LF_ServerHeadless`, builds the web client, removes `Workspace/output/Baking`, packages `LF Staging`, starts `LF_ServerHeadless --ApplySubConfig Staging`, and serves `Workspace/web-debug/LF-Client-Staging-Web/web-server.py` on port `8001`. `Web :: Launch Staging Client [linux]` then opens `https://dev.lastfrontier.ru/play-staging/`, so staging browser behavior depends on both the local staging package/server and the public staging URL route.

On the client side, scene transitions are surfaced through `PrepareToLoadScene()` / `SceneLoaded()` remotes in `../../Scripts/Scenes.fos`, which fire `Game.OnPrepareToLoadScene` / `Game.OnSceneLoaded`. `../../Scripts/GameState.fos` uses those events to keep the `Wait` screen visible during scene loads and only restore `GuiScreen::Game` after the scene is reported as loaded.

`Workspace/web-debug/...` output directories are generated by the packaging tasks and may not exist in a fresh checkout until the first web package build completes.

An explicit in-game quit is finalized on the next browser frame. The Web client calls `ClientEngine::Shutdown()` (including script `Game.OnFinish` subscribers) and then cancels the Emscripten main loop; `emscripten_set_main_loop_arg(..., simulate_infinite_loop=1)` does not return to the ordinary post-loop native cleanup path, so `RunClientRuntime()` never reaches its own cleanup, result reporting, or `ApplicationShutdownHook()`. Browser/tab termination remains best-effort and uses page lifecycle hooks owned by the project when needed.

## Source paths inspected

- `../BuildTools/buildtools.py`
- `../BuildTools/prepare-workspace.sh`
- `../BuildTools/prepare-win-workspace.ps1`
- `../ThirdParty/emscripten`
- `../../.vscode/tasks.json`
- `../../.vscode/launch.json`
- `../../CMakePresets.json`
- `../../LastFrontier.fomain`
- `../../Scripts/Scenes.fos`
- `../../Scripts/GameState.fos`

## Practical Debugging Notes

When a web-debug run fails, separate browser packaging, HTTP serving, and game-server startup before rebuilding everything:

- **`Workspace/web-debug/...` is missing** -> run `Web :: Build Debug Package` / `package-web-debug`; a fresh checkout has no generated browser package until packaging completes.
- **browser opens but assets or `.wasm` fail to load** -> inspect the generated package directory and `web-server.py`; web debug packages use `Raw+WebServer`, so serving from a different directory can hide packaged files.
- **port `7000`, `4025`, or `4026` is already in use** -> run the hidden web stop/prepare tasks or kill the stale listener; web launch cleanup is port-based, not tied to a remembered process id.
- **local web client cannot connect to server** -> check `../../LastFrontier.fomain` base `ClientNetwork.ServerHost = localhost`, `ServerPort = 4025`, and whether the local headless server actually started.
- **remote-scene web launch connects but loads the wrong scene** -> inspect `RemoteSceneLaunch`, `--Scene.Startup <SceneId>`, and the selected VS Code scene task rather than changing scene scripts first.
- **browser hangs on the wait/loading screen** -> trace `../../Scripts/Scenes.fos` `PrepareToLoadScene()` / `SceneLoaded()` remotes and `../../Scripts/GameState.fos` wait-screen restore logic.
- **a web fix appears to have no effect** -> the client has three independently rebuilt layers, and skipping one silently leaves the old code running. Engine C++ needs a rebuild with **both** `LF_Client.wasm` and `LF_Client.js` deleted first (CMake treats the target as up to date if either survives); a swapped Mono archive needs the same forced relink, because the runtime libraries are linked by name (`-lmono-ee-interp`) and CMake tracks no file dependency on them; and engine or game C# needs `BakeResources`. Verify the change reached the binary (grep the `.wasm`, or the rebuilt assembly) before concluding the fix did not work.
- **an edit to the Mono runtime source changes nothing** -> `SetupManagedRuntime` is guarded by a `READY_<triplet>_...` marker file in the build tree's `dotnet/` directory, so editing (or reverting) anything under `dotnet/runtime/src/` rebuilds nothing at all. Rebuild the affected archive in the runtime's own ninja tree (`dotnet/runtime/artifacts/obj/mono/<triplet>/`, e.g. target `mono/mini/libmono-ee-interp.a`), copy it over `dotnet/output/mono/<triplet>/lib/`, and force the relink as above. Diagnostic tracing added this way survives every ordinary rebuild - check the packaged `.wasm` for your marker string before shipping or timing anything.
- **web link fails with `undefined symbol: __syscall_*` or `SystemNative_*`** -> do not reach for `-sALLOW_UNIMPLEMENTED_SYSCALLS=1`; find what pulls the symbol in. Under LTO the reference lives in the combined bitcode, so scanning archives with `llvm-nm` can report "nobody" while the object is plainly linked - scan the archive *members* (`llvm-nm --undefined-only --print-file-name`) and check which entry points of the owning object are still in the generated table. See "Managed Runtime On Wasm" below.
- **web build fails before CMake config** -> verify the pinned Emscripten version from `../ThirdParty/emscripten` and that `Workspace/emsdk` was prepared by the BuildTools web workspace step.
- **`RelWithDebInfo` links with DWARF** -> the Web flags intentionally retain `-g3`; `Init.cmake` suppresses only Emscripten's informational `limited-postlink-optimizations` warning for those debug-info configurations. Do not suppress other compiler or linker warnings when adjusting this path.
- **official web package fails in `patch_data` (`Space for embedded data not found`, `Internal config marker not found`, or `assert pos != -1` on `###NOT_PACKAGED###`)** -> the three patch markers (`EMBEDDED_RESOURCES` in the generated `EmbeddedResources.gen.inc`, `INTERNAL_CONFIG` in the generated `InternalConfig.gen.inc` included by [`../Source/Common/Settings.cpp`](../Source/Common/Settings.cpp), `PACKAGED_MARK` in [`../Source/Common/Common.cpp`](../Source/Common/Common.cpp)) must survive emcc `-O3 -flto`; each definition is annotated with `FO_KEEP_DATA_SYMBOL` from [`../Source/Essentials/BasicCore.h`](../Source/Essentials/BasicCore.h) (which expands to `[[gnu::used]] static alignas(uint32_t) volatile` on GCC/Clang). `FO_EMBEDDED_DATA_CAPACITY` controls the embedded-resource buffer; the internal-config buffer is fixed by the engine at 10000 bytes. When adding a new package-time marker, declare the storage through the same macro - do not hand-roll `static volatile` or LTO may DCE the bytes and break `package.py`.
- **LocalTest and RemoteSceneLaunch appear cross-contaminated** -> remember one `package-web-debug LF RemoteSceneLaunch LocalTest` pass refreshes both package directories, so compare the generated package path used by the current launch entry.
- **staging web launch serves old resources or the wrong backend** -> inspect `Web :: Clean Output Baking [hidden]`, the single-variant `package-web-debug LF Staging` task, `LF_ServerHeadless --ApplySubConfig Staging`, port `8001`, and the `https://dev.lastfrontier.ru/play-staging/` route before changing LocalTest or RemoteSceneLaunch docs.

## Managed Runtime On Wasm

The web client embeds the same managed script backend as the native builds, so the `browser.wasm` Mono
runtime is linked into `LF_Client.wasm` and the assemblies are copied beside it. Four things about that
link are specific to the browser and easy to break by "simplifying" them:

- **The browser subset carries an extra piece.** `setup_mono` builds the runtime with
  `mono.runtime+mono.corelib+libs.native`, and for `browser` it appends `mono.wasmruntime`. That last
  subset builds the TypeScript glue (`src/mono/browser/runtime/`) that supplies the imports Mono expects
  JavaScript to provide - the scheduler, the entropy source and the startup helpers. Without it the link
  fails on `mono_wasm_schedule_timer`, `mono_wasm_browser_entropy` and their neighbours. The glue is
  published beside the runtime as `dotnet.es6.*.js` and passed to the link as `--pre-js`, `--js-library`
  and `--extern-post-js`. Only the browser runtime marker carries the extra suffix, so changing the
  browser subset does not invalidate the verified Linux and Android runtimes.
- **`System.Globalization.Native` is deliberately not linked on wasm.** Statically it needs ICU at link
  time; the game ships none and runs invariant, so the library is dropped on the merits rather than
  stubbed. On every other platform it is still linked.
- **The generated P/Invoke table is filtered for the browser.** Taking an entry point's address keeps its
  whole object alive, so naming a call a browser cannot honour drags a POSIX dependency into the link.
  `FO_MANAGED_WEB_EXCLUDED_ENTRY_POINTS` in the CMake third-party stage passes those names to
  `generate_pinvoke_table.py --exclude`: process control, the user/group database, dynamic loading, mmap
  and file locking. The generator logs the count it dropped (`System.Native: 212 entry points (39
  excluded)`). Excluding a name means a managed call to it fails at runtime instead of at link time, so
  only exclude what a browser genuinely cannot do.
- **The browser runtime splits itself across archives, and the embedder initializes each one.** The
  runtime is built with `ENABLE_INTERP_LIB=1`, `DISABLE_INTERPRETER` and `DISABLE_ICALL_TABLES` (read the
  runtime's own generated `config.h`, not the CMake cache, which disagrees). So `mini_init` initializes
  none of them and `ConfigureManagedRuntime` does it instead, in dotnet's own order (see
  `browser/runtime/runtime.c`): `mono_jit_set_aot_mode(MONO_AOT_MODE_INTERP_ONLY)`, then
  `mono_icall_table_init()`, then `mono_ee_interp_init("")`, then the three IL generators
  (`mono_marshal_ilgen_init`, `mono_method_builder_ilgen_init`, `mono_sgen_mono_ilgen_init`). Miss the
  icall table and every icall resolves to the niladic `no_icall_table` stub, which a four-argument call
  site cannot invoke - and because wasm rejects the call on type rather than entering the stub, its
  `g_assert_not_reached` never runs and you get a bare `function signature mismatch` instead of the
  diagnostic. `libmono-ee-interp.a` must precede `monosgen` in the link, and the interpreter SIMD table
  archive (`mono-wasm-simd` / `mono-wasm-nosimd`) must match how the runtime was built.
- **No preemptive thread suspension.** The engine forces `MONO_THREADS_SUSPEND=preemptive` for the
  multi-engine server model, which implements suspension with signals - WebAssembly has none, and the
  browser client is single-threaded anyway. Left on, SGen walks off the end of the card table during
  `collect_nursery`. The setting is skipped on web.
- **Nothing on the managed side may memory-map a file.** `AssemblyName.GetAssemblyName` does, so
  `ManagedLoadContextHost` reads the assembly's simple name through a `PEReader` over a `FileStream`
  instead. Excluding `SystemNative_MMap` from the table is what surfaces this, as an ordinary
  `EntryPointNotFoundException`.

- **`ALLOW_UNIMPLEMENTED_SYSCALLS` stays `0`, and two syscalls are implemented instead.** Emscripten
  provides weak stubs for the syscalls it does not implement, but `tools/system_libs.py` links that
  `libstubs` library under exactly one condition - `if settings.ALLOW_UNIMPLEMENTED_SYSCALLS` - so with
  the strict default the stubs never arrive. Two of them are genuinely reachable and cannot be filtered
  away: `uname` is reached through `gethostname()`, which SDL itself calls alongside the managed
  networking layer, and `wait4` comes from the System.Native process object, which stays live because
  `SystemNative_GetCwd`, `GetPid`, `GetProcessPath` and `PathConf` share it and are needed. Both are
  defined by the engine in [`../Source/Common/WebRelated.cpp`](../Source/Common/WebRelated.cpp), mirroring
  Emscripten's own semantics: a fixed identity for `uname`, `-ECHILD` for `wait4`. Add a definition there
  rather than relaxing the flag - the flag would also silence every syscall a future change drags in.

## Key Files and Integration Points

If you need to trace the current web-debug flow through the live repository, start with these files:

- `../../.vscode/tasks.json` - live web task graph for `Web :: Prepare Workspace`, `Web :: Build Client`, `Web :: Build Debug Package`, `Web :: Prepare Launch [linux]`, `Web :: Prepare Scene Launch [linux]`, `Web :: Prepare Staging Launch [linux]`, hidden web-service stop tasks, and the generated `web-server.py` launch paths under `Workspace/web-debug/`
- `../../.vscode/launch.json` - active browser-debug and web-scene launch entries, including the current `webRoot` and Chrome profile paths for local and remote-scene flows
- `README.md` - repo-root web build commands and the shared BuildTools-based workflow this doc summarizes
- `../../CMakePresets.json` — Emscripten configure/build preset family behind the documented web-client build flow
- `../../LastFrontier.fomain` - base networking defaults plus `RemoteSceneLaunch` and `Staging` subconfig-sensitive startup behavior used by the local browser client, remote-scene server, and staging server flows
- `../BuildTools/buildtools.py` - shared web workspace preparation, Emscripten build, and `package-web-debug` entry point used by current Windows and Linux workflows
- `../BuildTools/prepare-workspace.sh` and `../BuildTools/prepare-win-workspace.ps1` - platform wrapper scripts used by the VS Code web tasks before they delegate into BuildTools
- `../ThirdParty/emscripten` - pinned Emscripten version source referenced by the web workspace setup flow
- `Docs/Debugging.md` - native and attach-debug companion flow when a browser-side issue must be correlated with server or AngelScript stepping
- `../../Scripts/Scenes.fos` - client scene-load remotes/events and server-side scene startup wiring used by remote-scene browser launches
- `../../Scripts/GameState.fos` - client wait-screen behavior during `OnPrepareToLoadScene` / `OnSceneLoaded`
- `Docs/Scenes.md` - remote-scene startup details relevant to `RemoteSceneLaunch` web packages and `--Scene.Startup <SceneId>` behavior
- `../../Scripts/Tests/Test_ClientGui.fos`, `../../Scripts/Tests/Test_ClientUiText.fos`, and `../../Scripts/Tests/Test_ClientControl.fos` - client-visible gameplay probes that are useful smoke checks after web packaging or browser startup changes
- `Docs/GuiSystem.md` and `Docs/Localization.md` - companion references when a browser-only failure turns out to be generated-screen or text-formatting behavior rather than packaging

## Validation and Tests

Current checks worth running when web launch flow, packaging paths, or browser-debug assumptions change:

- verify web task names, generated `Workspace/web-debug/.../web-server.py` paths, `--Scene.Startup <SceneId>` handling, and staging port `8001` against `../../.vscode/tasks.json`; keep this guide canonical for choosing and debugging the web/local/remote-scene/staging routes
- `../../.vscode/launch.json` defines the active browser-debug entries, current `webRoot` locations under `Workspace/web-debug/LF-Client-LocalTest-Web` and `Workspace/web-debug/LF-Client-RemoteSceneLaunch-Web`, and the local browser URL `http://localhost:7000/index.html`
- `../../CMakePresets.json` confirms the current Emscripten configure/build preset family used by the documented web workflow, including the `emscripten` preset targeting `Build/Web`
- `../../Scripts/Tests/Test_ClientGui.fos` covers gameplay and GUI screen transitions that are commonly rechecked when browser client packaging or UI startup behavior changes
- `../../Scripts/Tests/Test_ClientUiText.fos` covers embedded-client UI text and login-screen behavior that often regresses alongside web-client presentation changes
- `../../Scripts/Tests/Test_ClientControl.fos` covers client control and interaction probes that help verify browser-client behavior after web debug flow changes
- `../../Scripts/Scenes.fos` plus `../../Scripts/GameState.fos` are worth rechecking when scene-debug launches start hanging on the wait screen or fail to hand control back to gameplay after a remote-scene transition
