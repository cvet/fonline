# Debugging

> Engine-owned documentation. Paths under `../` are relative to the FOnline engine root. Paths under `../../` point to an embedding game project such as Last Frontier when this engine is used as a submodule.

Diagnosing a server that logged a handled invariant violation, deterministically terminated (`FO_STRONG_ASSERT` / `ReportExceptionAndExit`), or left a "stuck-destroying" / un-syncable entity? The error-tier model and the entity-lifecycle exception contracts are in [ExceptionSafety.md](ExceptionSafety.md).

## Visual Studio Visualizers

For MSVC-generated solutions, natvis files from `../BuildTools/natvis` are included in the generated project automatically.

`essentials.natvis` covers Essentials smart pointers, stack traces, exceptions, hashed strings, and compact helper value types.

`unordered_dense.natvis` covers `ankerl::unordered_dense` containers.

## Source paths inspected

- `../BuildTools/natvis/essentials.natvis`
- `../BuildTools/natvis/unordered_dense.natvis`
- `../BuildTools/cmake/stages/Finalize.cmake`
- `../BuildTools/cmake/helpers/Build.cmake`
- `../Source/Essentials/StackTrace.h`
- `../Source/Essentials/StackTrace.cpp`
- `../Source/Essentials/BaseLogging.h`
- `../Source/Essentials/BaseLogging.cpp`
- `../Source/Essentials/ExceptionHandling.h`
- `../Source/Essentials/ExceptionHandling.cpp`
- `../Source/Scripting/AngelScript/AngelScriptContext.cpp`
- `../Source/Frontend/ApplicationInit.cpp`
- `../Source/Tests/Test_StackTrace.cpp`
- `../Source/Tests/Test_ExceptionHandling.cpp`
- `../../.vscode/launch.json`
- `../../.vscode/tasks.json`

## Stack Trace Architecture

The engine no longer maintains a thread-local manual call stack. The `FO_STACK_TRACE_ENTRY()` macro is now empty outside Tracy builds (under `FO_TRACY` it expands to `ZoneScoped` only), and stack traces are constructed on demand from two independent sources at the moment a `StackTraceData` is captured:

1. **Native frames.** [../Source/Essentials/StackTrace.cpp](../Source/Essentials/StackTrace.cpp) calls `backward::StackTrace::load_here(...)` to capture raw return addresses. Symbol resolution is deferred â€” `ResolveStackTrace`, `FormatStackTrace`, `SafeWriteStackTrace`, and `GetStackTraceEntry` resolve via `backward::TraceResolver` only when frames are actually needed. Resolved native frames are cached globally by instruction pointer in a capped process-local cache (`STACK_TRACE_RESOLVE_CACHE_MAX_ENTRIES`) so repeated exception formatting and script/native anchor matching reuse symbol data. The capture path is allocation-free aside from the storage on the `StackTraceData` itself.
2. **Script frames.** Higher layers register a `ScriptStackTraceProvider` via `SetScriptStackTraceProvider(...)`. The provider is called synchronously during capture and pre-resolves frames eagerly because script execution state is ephemeral (the active context's call stack changes after we leave the capture site).

Pre-resolved script frames live behind a `shared_ptr<const vector<StackTraceFrame>>` so copying a `StackTraceData` (notably during `BaseEngineException` propagation) remains noexcept.

### AngelScript bridge

[../Source/Scripting/AngelScript/AngelScriptContext.cpp](../Source/Scripting/AngelScript/AngelScriptContext.cpp) installs `CollectScriptStackLayers` through the AngelScript stack-trace installer. The provider walks `AngelScript::asGetActiveContext()` first, then follows `AngelScriptContextExtendedData::Parent` up the parent-context chain. For each context, it iterates `asIScriptContext::GetCallstackSize()` levels in order (deepest call first) and emits a `StackTraceFrame` per level by resolving the function declaration plus the original `.fos` file/line through `Preprocessor::ResolveOriginalFile / ResolveOriginalLine` (the line-number translator is stashed at engine user-data slot `5`).

The provider handles the multi-context case naturally: if a script function called a native function that re-entered scripting on a fresh context, the active (child) context's frames are emitted first, then the parent context's frames are appended. The two sub-stacks read continuously in the formatted output, with native bridging frames showing up after all script frames once symbols are resolved.

`AngelScriptBackend` mutes the AngelScript message callback during final script-engine teardown. Runtime and compilation messages still go through the normal callback before teardown begins, but shutdown-only GC survivor messages are kept out of normal logs.

When `ServerEntity::ValidateAccess()` reports `Entity access without sync`, the server log now includes the entity parent/widen chain, the script/native stack, and the current `SyncContext`'s recent cover-transition ring buffer. The transition trace records `SyncEntities` requests/acquisitions and `Release()` calls with the previous/requested/held entity cover so a post-`Yield`, cover-replacement, or missing-widen failure can be matched against the last lock transition rather than inferred from stack frames alone.

The Essentials module never depends on AngelScript directly; the bridge is one-way through the function pointer registered at runtime. This keeps the `Essentials` layer reusable and avoids forcing the whole engine to compile against AngelScript headers.

### Unified frame ordering

The unified ordering produced by `ResolveStackTrace` and `FormatStackTrace` is, most-recent first:

```
[Script] active context, top frame
[Script] active context, ..., bottom frame
[Script] parent context, top frame
[Script] ..., bottom frame
[Script] ..., root context, bottom frame
[Native] caller of root context Execute()
[Native] ...
[Native] main
```

`FormatStackTrace` prefixes lines with `[Script]` or `[Native]` so the boundary between sub-stacks is obvious in logs. `SafeWriteStackTrace` uses the same format, with an allocation-free fallback that writes raw `0x...` addresses when symbol resolution fails (used for OOM and crash paths).

### API surface

| Function | Purpose |
|----------|---------|
| `GetStackTrace()` | Capture native PCs + query script provider. Returns a `StackTraceData` snapshot. |
| `GetStackTraceEntry(deep)` | Resolve a single frame at depth `deep` (0 = topmost). Script frames first, native frames after. |
| `ResolveStackTrace(st)` | Resolve every frame into a `vector<StackTraceFrame>` (full symbol resolution). |
| `FormatStackTrace(st)` | Human-readable multi-line string with `[Script]` / `[Native]` prefixes. |
| `SafeWriteStackTrace(st)` | Writes the trace to the base log; tolerant of OOM (falls back to hex addresses). |
| `ClearResolvedStackTraceCache()` | Clear the process-wide native-frame resolution cache. |
| `GetResolvedStackTraceCacheSize()` | Return the current native-frame resolution cache size. |
| `SetScriptStackTraceProvider(p)` | Install the script-frame provider. Pass an empty function to clear. |
| `HasScriptStackTraceProvider()` | Test hook to confirm a provider is registered. |

`BaseEngineException` captures `GetStackTrace()` at construction so the trace stored on the exception object reflects the throw site. The crash printer in `ExceptionHandling.cpp` writes `FATAL ERROR!`, a `Crash reason:` line with the native SEH exception / signal / runtime termination code captured by `backward.hpp`, then calls `SafeWriteStackTrace` with the trace captured by `SetCrashStackTrace`.

### Exception reporting and deferred formatting

The reporters (`ReportExceptionAndExit`, `ReportExceptionAndContinue`) create a `CatchedStackTraceData` value with `MakeErrorStackTrace()`. That value contains the origin trace from `BaseEngineException::stack_trace()` when the exception type carries one, plus a fresh catch-site trace from `GetStackTrace()`. `FormatStackTrace(const CatchedStackTraceData&)` formats the origin trace when present, otherwise it prefixes the catch-site trace with `Catched at:`.

The exception callback receives the already-captured `CatchedStackTraceData` and the fatal flag directly. There is no separate context object in the current source; if callback behavior changes, update `ExceptionCallback` in [../Source/Essentials/ExceptionHandling.h](../Source/Essentials/ExceptionHandling.h), `ReportExceptionAndExit` / `ReportExceptionAndContinue` in [../Source/Essentials/ExceptionHandling.cpp](../Source/Essentials/ExceptionHandling.cpp), and the default callback in [../Source/Frontend/ApplicationInit.cpp](../Source/Frontend/ApplicationInit.cpp) together.

### Logging and crash-path primitives

[../Source/Essentials/BaseLogging.h](../Source/Essentials/BaseLogging.h) and [../Source/Essentials/BaseLogging.cpp](../Source/Essentials/BaseLogging.cpp) own `SafeWriteStackTrace(const StackTraceData&)`, which is used by crash and low-memory paths where normal formatting/logging may be unsafe. Regular exception callbacks use `WriteLogMessage` with the captured `CatchedStackTraceData`; immediate duplicate exception messages are collapsed into a later `...and N more same messages` summary by `Logging.cpp`. Async file writing is still controlled by `SetAsyncLogWriting(true)` once `settings.AsyncLogWrite` is known.

### Crash-to-log guarantee and self-test

Every abnormal death must leave usable diagnostics in the log file, not only on `stderr` (which is discarded for a headless/service process). The paths:

- **Fatal signals** (`SIGSEGV`, `SIGABRT`, `SIGFPE`, `SIGBUS`, `SIGILL`, …) are caught by backward-cpp's signal handler ([../ThirdParty/backward-cpp/backward.hpp](../ThirdParty/backward-cpp/backward.hpp)), which writes `FATAL ERROR!`, a `Crash reason:` line, and a symbolised stack trace through `GetCrashStream()` → `BackwardOStreamBuffer` → `WriteBaseLog`. The header first calls `SuspendAsyncLogWriting()` and everything after is written with `WriteSync` (immediate `flush`), so the report survives even with `Common.AsyncLogWrite` on.
- **`std::terminate`** (an exception escaping a `noexcept` function or a thread, a rethrow with no handler, a pure-virtual call) is routed through `SignalHandling::terminator()` — an **FOnline patch** that installs `std::set_terminate` on POSIX too (it was Windows-only upstream). It records the failing exception's type + `what()` via `SetCrashTerminationInfo("std::terminate")` (`FormatRuntimeCrashInfo`), prints the report, and `_Exit`s without re-entering the `SIGABRT` handler. Without it, the default POSIX terminate handler prints the exception text to `stderr` only and the log gets a bare `Signal 6 (SIGABRT)`.
- **Stack overflow** is a `SIGSEGV` on the guard page; the handler needs an **alternate signal stack** (`SA_ONSTACK`) because the thread's own stack is exhausted. backward installs one only on the thread that constructs it (the main thread), so every long-lived worker thread calls `InstallCrashHandlerStackForThisThread()` ([../Source/Essentials/ExceptionHandling.cpp](../Source/Essentials/ExceptionHandling.cpp)) at entry (see `WorkThread::ThreadEntry`) to keep worker-thread overflows diagnosable. Threads created outside the engine (e.g. third-party Asio/SDL threads) do not get one; add the call at their entry if they run engine logic that can recurse deeply.
- **Caught exceptions** reported through `ReportExceptionAndExit` / `ReportExceptionAndContinue` take the graceful path instead: the exception callback logs the message + `CatchedStackTraceData` via `WriteLogMessage`, plus `Shutdown!` for the fatal variant. No `FATAL ERROR!` header.

`InstallCrashHandlerStackForThisThread()` allocates a per-thread 2 MiB signal stack (lazily committed; touched only during a crash) and is a no-op on non-POSIX targets and under a debugger (where backward does not install its handlers).

**Self-test.** [../Source/Common/DiagnosticSelfTest.cpp](../Source/Common/DiagnosticSelfTest.cpp) deliberately induces a chosen crash class to verify the above end-to-end. It is driven by the `FO_SELFTEST_CRASH` environment variable (not a setting, so it is inert in production and invisible to the config/script surface) and fires once in `InitApp`, after logging + the exception callback + the async-log mode are live. Modes: `main_null_read` / `main_null_write` / `main_wild_write` (SIGSEGV), `main_fpe`, `main_abort`, `main_stack_overflow`, `main_noexcept_throw`, `main_throw`, `main_strong_assert`, and `thread_*` counterparts that run the same crash on a worker-style `std::thread`. The embedding project's `Tools/PipelineTests/test_crash_diagnostics_linux.py` exercises these against the Linux headless server.

### Coverage

`../Source/Tests/Test_StackTrace.cpp` exercises the new API:

- Provider registration / unregistration is observable via `HasScriptStackTraceProvider`.
- Script frames captured by the provider preserve the most-recent-first ordering.
- Multi-context concatenation (top-most context's frames first, then parent) renders in the expected order.
- `[Script]` / `[Native]` prefixes are present in `FormatStackTrace`.
- Resolved unified order places script frames before native frames.
- Native frame resolution populates the global cache once per unique instruction pointer and reuses entries on repeated resolution.
- `GetStackTraceEntry(deep)` returns the depth-th frame and `nullopt` for out-of-range depths.
- An empty `StackTraceData` formats to header-only.
- `SafeWriteStackTrace` writes both sections.
- A throwing provider (despite the noexcept contract) does not propagate from capture.

`../Source/Tests/Test_ExceptionHandling.cpp` continues to exercise `BaseEngineException` capture, `FormatStackTrace` ordering, and exception callbacks against the new layout.

## Visual Studio Solution Folders

For the MSVC CMake generators, solution-folder grouping is only reliable when a target is created with `CMAKE_FOLDER` already set. Keep the late regrouping pass in `../BuildTools/cmake/stages/Finalize.cmake`, but make sure the helper macros in `../BuildTools/cmake/helpers/Build.cmake` set `CMAKE_FOLDER` while creating `Applications`, `Commands`, `CoreLibs`, and `ThirdParty` targets. For external packages added through `AddSubdirectory(...)`, pass `FOLDER "..."` to the repository-owned wrapper so the subproject targets are created inside the intended solution folder without editing vendor `../../CMakeLists.txt`.

## Quick Validation

1. Regenerate or open the MSVC solution.
2. Start a debugger session and inspect `fo::ptr`, `fo::nptr`, `fo::unique_ptr`, or `fo::refcount_ptr` values in Watch or Locals.
3. Confirm that expanding the smart pointer opens the pointed object directly.
4. Capture a stack trace by stepping into `fo::GetStackTrace()` and inspect the resulting `StackTraceData`. Native frames render as raw addresses until symbol resolution runs (via `FormatStackTrace` / `ResolveStackTrace`); pre-resolved script frames are reachable through `ScriptStackTraceLayer::ScriptFrames` in the `ScriptLayers` shared pointer.
5. Break on `fo::BaseEngineException` and verify that the message, parameters, and embedded stack trace are visible.

## VS Code Debug Configurations

Current `../../.vscode/launch.json` entries use:

- `Debugging :: Launch [windows]` for native Windows server debugging (`cppvsdbg`)
- `Debugging :: Launch [linux]` for native Linux server debugging (`cppdbg` with `gdb`)
- `Debugging :: Attach` for the AngelScript debugger over UDP discovery on port `43001`
- compound launchers such as `Debugging :: Launch and Attach [windows]` and `Debugging :: Launch and Attach [linux]`

These native launch configurations depend on `Prepare :: Launch (Debug)`, which currently bakes resources and builds the debug `LF_Server` binary before attaching the C++ debugger.

The AngelScript debugger requires `AngelScript.DebuggerEnabled = True`. The current `LocalTest` subconfig enables it, while `GameplayTests` disables it.

## Fast Route Selection

Before choosing a debugger, identify the smallest boundary that can prove the symptom:

| Symptom family | First doc route | Validation route |
|----------------|-----------------|------------------|
| Gameplay rule, player-state, AI, combat, survival, inventory, or world traversal | [GameSystems.md](../../Docs/GameSystems.md) and the owning domain doc | [Testing.md#validation-boundary-test-routing](../../Docs/Testing.md#validation-boundary-test-routing) with the narrowest `Testing.Filter` |
| Auth, login, account lookup, or platform-only runtime behavior | [AuthLoginFlow.md](../../Docs/AuthLoginFlow.md) first; add [SteamIntegration.md](../../Docs/SteamIntegration.md) for Steam client/server runtime issues | `Testing.Filter = authentication` for script flow; manual `Steam :: Launch Login` only when Steam runtime state is involved |
| Achievement, stat, analytics, or platform mirror mismatch | [Achievements.md](../../Docs/Achievements.md), [Analytics.md](../../Docs/Analytics.md), [SteamIntegration.md](../../Docs/SteamIntegration.md) | prove the gameplay event first, then inspect analytics transport or Steam mirror queues |
| Client-visible GUI/text/input issue | [GuiSystem.md](../../Docs/GuiSystem.md), [Localization.md](../../Docs/Localization.md), and the platform-specific debug doc if needed | GUI generation, `Testing.Filter = client`, then web or Android launch paths for platform-only failures |
| Native crash, script API binding, or engine/unit behavior | [NativeExtensions.md](../../Docs/NativeExtensions.md), [Scripts.md](../../Docs/Scripts.md), or engine tests | `RunUnitTests` / `validate.sh unit-tests`, then `CompileAngelScript` and the smallest consuming gameplay suite |

Use this table as the bridge between the entry-index fast routes and the concrete launch profiles below. If the boundary can be captured by a deterministic test, add or narrow that test before opening an interactive debugger. If the symptom depends on renderer, browser, Android packaging, Steam client state, or live script stepping, move to the matching launch/debug profile.

## Choosing The Right Debug Path

Use the debug path that matches the bug boundary instead of starting with the heaviest interactive session:

| Symptom | Start here | Why |
|---------|------------|-----|
| Script, proto, dialog, scene state, gameplay rule regression | `Launch Tests [linux]` / `Launch Tests [windows]` with `GameplayTests` after selecting the boundary in [Testing.md#validation-boundary-test-routing](../../Docs/Testing.md#validation-boundary-test-routing) | fastest repeatable server-side signal after resource baking |
| Native crash or engine assertion before gameplay state matters | `Debugging :: Launch [linux]` or `[windows]` | attaches C++ debugger to `LF_Server` under `LocalTest` |
| AngelScript breakpoint or call-stack inspection | compound `Debugging :: Launch and Attach [...]` | starts native server and attaches the FOS debugger on discovery port `43001` |
| Browser package, web client, or JavaScript-side failure | `Debugging :: Launch Web [...]` / Web Scene profiles | launches Chrome against the web-debug workspace and pairs with web service tasks |
| Android APK, Wi-Fi ADB, or device-to-host scene failure | `Android :: Launch Remote Scene [linux]` / `Docs/AndroidDebugging.md` | validates the external-device client path, APK install, and `ClientNetwork.ServerHost` override |
| Startup-scene-only bug | scene launch, Web Scene, or Android remote-scene profile first, then headless test only after isolating the rule | preserves intro/personal-room scene flow that regular gameplay suites may bypass |

A practical rule: if the repro can be expressed as a deterministic gameplay assertion, add or narrow a headless suite before opening an interactive debugger. If the repro depends on renderer, input, browser or Android packaging, external-device networking, or script stepping, use the launch profiles.

## Unit Test Validation

Use the deterministic engine test target for Common and metadata regressions before moving to wider gameplay checks.

1. Build the suite with `cmake --build Build/MSVC2026 --config RelWithDebInfo --target LF_UnitTests`.
2. Run it with `cmake --build Build/MSVC2026 --config RelWithDebInfo --target RunUnitTests`.
3. Prefer this path for migration-rule, serialization, and other engine-only regressions that do not require resource baking or a live server-client session.
4. Self-contained client-engine tests run through `NullRenderer`. They may still log missing `.fofx` files from the minimal in-memory test resources, but the headless renderer now synthesizes the required effect metadata instead of treating those missing shader assets as fatal.

## Gameplay Bug Triage

Use the headless workflow first for script, proto, content, and scene-runtime regressions. It is more deterministic than starting the regular server with an embedded client and keeps reproduction focused on gameplay state.

1. Reproduce the bug first, then rebake resources after any changes under `../../Scripts/`, `../../Scripts/Tests/`, `../../Scripts/Scenes/`, `Modifiers/`, `Items/`, `Critters/`, `Dialogs/`, `Maps/`, or `../../LastFrontier.fomain`.
2. Run `Prepare :: Gameplay Tests Launch`, then the platform launch task (`Launch Tests [windows]` or `Launch Tests [linux]`). This starts `LF_ServerHeadless` with `--ApplySubConfig GameplayTests`.
3. `GameplayTests` now uses suite-level multi-instance execution by default: matched gameplay suites run in dedicated in-process server+client worker threads, with `Testing.RunSuitesInParallel` enabling overlap and `Testing.MaxParallelInstances` capping how many worker instances may stay active at once. Worker servers are started one by one to avoid startup fan-out on busy machines, then continue running in parallel after startup succeeds. When `Testing.MaxParallelInstances = 0`, the controller uses `std::thread::hardware_concurrency()` and logs the resolved value at startup. Narrow validation with `Testing.Filter` when a bug maps to an existing gameplay suite or tag. New gameplay test files are only discovered after `Bake Resources` rebakes scripts.
4. Watch `TEST` log lines for suite progress, per-suite completion summaries, and the final parallel aggregate, plus `SCENE` log lines for startup-scene and runtime-context issues. Engine and extension threads created through the shared thread helper now inherit the suite thread namespace in logs, for example `TestSuite-Combat::ServerWorker`, which makes parallel output easier to separate even when the code uses direct thread creation instead of `WorkThread`. The default log file for this flow is `LF_ServerHeadless.log` in the workspace root.
5. Use the regular launch or scene-launch profiles only when the bug depends on the embedded client, rendering, direct input, AngelScript stepping, or startup scene UX.
6. For engine-side regressions that may also affect gameplay, run `LF_UnitTests` first, then move to the headless gameplay pass if the failure path crosses scripting, baking, or network replication.

## Key Files and Integration Points

If you need to trace the current debugging flow through the live repository, start with these files:

- `../../.vscode/launch.json` - live native, AngelScript-attach, and web-debug launch entries such as `Debugging :: Launch [linux]`, `Debugging :: Attach`, and the compound launch-and-attach profiles
- `../../.vscode/tasks.json` - task wiring behind `Prepare :: Launch (Debug)`, gameplay-test preparation, Win32 variants, and the web debug service lifecycle tasks paired with launch profiles
- `../../LastFrontier.fomain` - base config plus `LocalTest`, `GameplayTests`, and scene-launch subconfigs that control debugger availability and startup behavior
- `../BuildTools/natvis/essentials.natvis` and `../BuildTools/natvis/unordered_dense.natvis` - debugger visualizers for MSVC sessions
- `../BuildTools/cmake/stages/Finalize.cmake` and `../BuildTools/cmake/helpers/Build.cmake` - current solution-folder and generated-project wiring mentioned by the Visual Studio guidance in this doc
- `../../Scripts/Tests/Test_ClientControl.fos`, `../../Scripts/Tests/Test_ClientGui.fos`, and `../../Scripts/Tests/Test_ClientUiText.fos` - embedded-client and client-visible gameplay probes that are often the fastest debugger-adjacent validation targets
- `Docs/Testing.md` - current headless gameplay-test triage flow and validation-boundary test routing used before falling back to regular embedded-client debugging
- `Docs/GameSystems.md` - cross-system debugging clusters that help choose the owning gameplay/content doc before choosing a launch profile
- `Docs/Scenes.md` - startup-scene runtime details that matter when a bug only reproduces through intro, personal-room, or other scene-driven entry paths
- `Docs/WebDebugging.md` - companion reference for browser-side and web packaging debug flows that branch away from the native server debugger path
- `Docs/AndroidDebugging.md` - companion reference for Android APK, Wi-Fi ADB, external-device networking, and remote-scene debug flows

## Validation and Tests

Current checks worth running when debugger launch flow, attach assumptions, or troubleshooting guidance changes:

- verify native, AngelScript, and web debugging entries against `../../.vscode/launch.json`, including the AngelScript discovery port `43001`; keep this guide focused on debugger route selection rather than duplicating every launch profile
- `../../.vscode/tasks.json` and `../../LastFrontier.fomain` confirm the documented split between `LocalTest` debug launches with `AngelScript.DebuggerEnabled = True` and `GameplayTests` runs with `AngelScript.DebuggerEnabled = False`
- `Docs/Testing.md` remains the reference for the current `LF_ServerHeadless --ApplySubConfig GameplayTests` workflow and `Validation Boundary Test Routing` table used during gameplay bug triage
- `../../Scripts/Tests/Test_ClientControl.fos`, `../../Scripts/Tests/Test_ClientGui.fos`, and `../../Scripts/Tests/Test_ClientUiText.fos` cover embedded-client interaction, GUI, and UI-text paths that are commonly rechecked when debugging workflows depend on client-visible behavior
- `Docs/WebDebugging.md` and `Docs/AndroidDebugging.md` confirm the browser and external-device branches of the general debug-path selection table

## Client Host and Runtime Validation

The client ships as `LF_Client.exe` (host) plus a sibling loadable runtime library built by the `LF_ClientLib` target. The host loads the runtime through a stable C ABI and falls back to the embedded client only when the requested compatibility version matches its built-in one. See [ClientUpdater.md](ClientUpdater.md) for the full architecture, ABI surface, updater protocol, and packaging behavior.

Quick validation when touching either side:

1. Build `LF_Client`; on native host/runtime platforms it depends on `LF_ClientLib`. Confirm the host-derived runtime alias lands next to the host (`LF_Client.exe` + `LF_Client.dll` on Windows, `LF_Client` + `LF_Client.so` on Linux). Build `LF_ClientLib` explicitly only when the host is not needed.
2. Launch `LF_Client.exe` with the bundled runtime present â†’ normal startup.
3. Launch `LF_Client.exe --ClientLibPath <path>` with a valid alternate runtime â†’ host routes through the loaded library.
4. Launch `LF_Client.exe --ClientLibPath <path> --ClientLibCompatibilityVersion <other>` and remove the runtime â†’ host fails (no embedded fallback when compatibility differs).
5. Point `--ClientLibPath` to an invalid path without `--ClientLibCompatibilityVersion` â†’ host falls back to the embedded client.
6. Re-run `LF_UnitTests` after ABI changes; `Test_ClientRuntimeApi.cpp` covers exports validation and compatibility helpers.
7. Build a packaged server target and confirm `<Settings.PlatformBinaries>/<target>/` (default `PlatformBinaries/`, sibling of the client-resources dir in the package layout) contains the runtime libraries the client will pull during startup binary sync.
