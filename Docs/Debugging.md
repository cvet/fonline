# Debugging

> Engine-owned documentation for reusable native, AngelScript, stack-trace, and client-runtime debugging. Concrete launch profiles, game tests, binary names, and editor tasks belong in the embedding project.

Diagnosing a server that logged a handled invariant violation, deterministically terminated (`FO_STRONG_ASSERT` / `ReportExceptionAndExit`), or left a "stuck-destroying" / un-syncable entity? The error-tier model and the entity-lifecycle exception contracts are in [ExceptionSafety.md](ExceptionSafety.md).

## Visual Studio Visualizers

For MSVC-generated solutions, engine and supported third-party natvis files are included in the generated project automatically.

`essentials.natvis` covers Essentials smart pointers, stack traces, exceptions, hashed strings, and compact helper value types.

`unordered_dense.natvis` covers `ankerl::unordered_dense` containers.

The vendored [`small_vector.natvis`](../ThirdParty/small_vector/source/support/visualstudio/small_vector.natvis) displays `gch::small_vector` size, capacity, inline/heap storage, and elements. `BuildTools/cmake/stages/ThirdParty.cmake` attaches it to the generated MSVC project; no manual Visual Studio installation is required.

## Source paths inspected

- `../BuildTools/natvis/essentials.natvis`
- `../BuildTools/natvis/unordered_dense.natvis`
- `../ThirdParty/small_vector/source/support/visualstudio/small_vector.natvis`
- `../BuildTools/cmake/stages/ThirdParty.cmake`
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
- `../BuildTools/angelscript-debugger/`
- `../Source/Scripting/AngelScript/AngelScriptDebugger.cpp`
- `../Source/Common/Settings.inc`

## Stack Trace Architecture

The engine no longer maintains a thread-local manual call stack. The `FO_STACK_TRACE_ENTRY()` macro is now empty outside Tracy builds (under `FO_TRACY` it expands to `ZoneScoped` only), and stack traces are constructed on demand from two independent sources at the moment a `StackTraceData` is captured:

1. **Native frames.** [../Source/Essentials/StackTrace.cpp](../Source/Essentials/StackTrace.cpp) calls `backward::StackTrace::load_here(...)` to capture raw return addresses. Symbol resolution is deferred â€” `ResolveStackTrace`, `FormatStackTrace`, `SafeWriteStackTrace`, and `GetStackTraceEntry` resolve via `backward::TraceResolver` only when frames are actually needed. Resolved native frames are cached globally by instruction pointer in a capped process-local cache (`STACK_TRACE_RESOLVE_CACHE_MAX_ENTRIES`) so repeated exception formatting and script/native anchor matching reuse symbol data. The capture path is allocation-free aside from the storage on the `StackTraceData` itself.
2. **Script frames.** Higher layers register a `ScriptStackTraceProvider` via `SetScriptStackTraceProvider(...)`. The provider is called synchronously during capture and pre-resolves frames eagerly because script execution state is ephemeral (the active context's call stack changes after we leave the capture site).

Pre-resolved script frames live behind a `shared_ptr<const vector<StackTraceFrame>>` so copying a `StackTraceData` (notably during `BaseEngineException` propagation) remains noexcept.

### AngelScript bridge

[../Source/Scripting/AngelScript/AngelScriptContext.cpp](../Source/Scripting/AngelScript/AngelScriptContext.cpp) installs `CollectScriptStackLayers` through the AngelScript stack-trace installer. The provider walks `AngelScript::asGetActiveContext()` first, then follows `AngelScriptContextExtendedData::Parent` up the parent-context chain. For each context, it iterates `asIScriptContext::GetCallstackSize()` levels in order (deepest call first) and emits a `StackTraceFrame` per level by resolving the function declaration plus the original `.fos` file/line through `Preprocessor::ResolveOriginalFile / ResolveOriginalLine` (the line-number translator is stashed at engine user-data slot `5`).

The provider handles the multi-context case naturally: if a script function called a native function that re-entered scripting on a fresh context, the active (child) context's frames are emitted first, then the parent context's frames are appended. The two sub-stacks read continuously in the formatted output, with native bridging frames showing up after all script frames once symbols are resolved.

`AngelScriptBackend` mutes the AngelScript message callback during final script-engine teardown. Runtime and compilation messages still go through the normal callback before teardown begins, but shutdown-only GC survivor messages are kept out of normal logs.

When `ServerEntity::ValidateAccess()` reports `Entity access without sync`, the server log includes the entity parent/widen chain and the script/native stack. This identifies the uncovered entity path and the access site; the engine does not currently retain a `SyncContext` transition history, so earlier cover replacement or `Release()` activity must still be reconstructed from the surrounding execution path.

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

For the MSVC CMake generators, solution-folder grouping is only reliable when a target is created with `CMAKE_FOLDER` already set. Keep the late regrouping pass in `../BuildTools/cmake/stages/Finalize.cmake`, but make sure the helper macros in `../BuildTools/cmake/helpers/Build.cmake` set `CMAKE_FOLDER` while creating `Applications`, `Commands`, `CoreLibs`, and `ThirdParty` targets. For external packages added through `AddSubdirectory(...)`, pass `FOLDER "..."` to the repository-owned wrapper so subproject targets are created inside the intended solution folder without editing vendor `CMakeLists.txt` files.

## Quick Validation

1. Regenerate or open the MSVC solution.
2. Start a debugger session and inspect `fo::ptr`, `fo::nptr`, `fo::unique_ptr`, or `fo::refcount_ptr` values in Watch or Locals.
3. Confirm that expanding the smart pointer opens the pointed object directly.
4. Capture a stack trace by stepping into `fo::GetStackTrace()` and inspect the resulting `StackTraceData`. Native frames render as raw addresses until symbol resolution runs (via `FormatStackTrace` / `ResolveStackTrace`); pre-resolved script frames are reachable through `ScriptStackTraceLayer::ScriptFrames` in the `ScriptLayers` shared pointer.
5. Break on `fo::BaseEngineException` and verify that the message, parameters, and embedded stack trace are visible.

## Debugger integration in an embedding project

The engine supplies debuggable applications, symbols, stack traces, and the AngelScript debugger endpoint. An embedding project owns the launch configuration that selects a generated binary, working directory, project config, and any bake/build prerequisites.

A project debugger setup should expose three independent routes:

- native launch or attach for the generated client, server, mapper, editor, or baker;
- AngelScript attach through the bundled debug adapter;
- platform-specific browser or Android launch when the bug requires those runtimes.

The AngelScript debugger requires `Script.DebuggerEnabled = True`. Its TCP endpoint
binds to `Script.DebuggerBindHost = 127.0.0.1` by default. Remote binding must be an
explicit command-line or subconfig override on a trusted network.

Keep project task names and binary prefixes in the project documentation. Engine
guidance should remain valid when `<ProjectDevName>` and output directories change.

## Fast route selection

Choose the smallest engine boundary that can prove the symptom:

| Symptom family | Start with | Validation boundary |
|---|---|---|
| Native assertion, exception, crash, or lifecycle failure | This page plus [ExceptionSafety.md](ExceptionSafety.md) | Reproduce in the smallest engine unit test and inspect the captured native/script stack. |
| Script compile, binding, event, remote-call, or nullability failure | [Scripting.md](Scripting.md), [GeneratedApiAndMetadata.md](GeneratedApiAndMetadata.md), and [Nullability.md](Nullability.md) | Compile a minimal script fixture or run the owning engine test before using an interactive debugger. |
| Client host/runtime load or update failure | [ClientRuntime.md](ClientRuntime.md) and [ClientUpdater.md](ClientUpdater.md) | Validate host/runtime ABI and compatibility behavior independently from gameplay. |
| Server runtime, entity, persistence, or map-lifecycle failure | [ServerRuntime.md](ServerRuntime.md), [EntityModel.md](EntityModel.md), and [Persistence.md](Persistence.md) | Use the narrow owning test under `Source/Tests/`. |
| Transport, handshake, replication, or connection failure | [Networking.md](Networking.md) | Prove protocol/connection behavior before debugging game authentication policy. |
| Rendering or frontend failure | [FrontendAndRendering.md](FrontendAndRendering.md) | Use a minimal visible client/mapper case; branch to Web or Android only for platform-specific failures. |
| Browser packaging/runtime failure | [WebDebugging.md](WebDebugging.md) | Reproduce in the generated web package and browser console. |
| Android package/device failure | [AndroidDebugging.md](AndroidDebugging.md) | Isolate workspace, native build, Gradle, ADB, and host-connect layers in that order. |

Gameplay rules, quests, UI composition, authentication policy, analytics, and product SDK integration are embedding-project domains. Route those symptoms through the project's owning docs and tests after the engine boundary is identified.

## Native debugging

Build the narrow generated application or unit-test target with debug information. The exact target name and prefix are project-derived.

On Windows, use the Visual Studio debugger or `cppvsdbg` with the matching PDB files. On Linux, use GDB or LLDB with the executable and shared-library symbols from the same build. For either platform:

1. reproduce with the smallest config that still loads the failing engine surface;
2. break on thrown C++ exceptions or the reported assertion path as appropriate;
3. retain the original log and stack trace before adding extra logging;
4. verify that executable, runtime library, and symbols come from one build;
5. convert a deterministic failure into an engine unit test when the boundary is reusable.

The stack-trace layer described above complements a debugger; it does not replace one when process state, registers, or memory corruption must be inspected.

## AngelScript debugger

Set `Script.DebuggerEnabled = True` in a development-only config. When enabled, the AngelScript backend retains line cues, disables bytecode optimization needed for stepping, starts the debugger endpoint, and advertises it over UDP discovery port `43001`.

The bundled adapter lives under `BuildTools/angelscript-debugger/`. An embedding project's editor configuration should launch or attach that adapter and may override discovery settings when several development instances share a host.

Do not enable the debugger in performance measurements or production configs. Its line-cue and optimization settings intentionally change script execution characteristics.

When attach fails:

1. confirm the selected config resolves `Script.DebuggerEnabled = True`;
2. inspect the engine log for the TCP endpoint and UDP discovery lines;
3. verify UDP `43001` is reachable or configure the adapter with an explicit endpoint;
4. confirm script sources correspond to the loaded baked bytecode;
5. reduce multi-instance launches so the intended endpoint is unambiguous.

## Engine test validation

[Testing.md](Testing.md) owns the current unit-test inventory and commands. For a reusable engine regression:

1. select or add the smallest `Source/Tests/Test_*.cpp` case;
2. build the embedding project's generated unit-test target;
3. run the exact test binary or filtered Catch2 case;
4. confirm the failure reproduces before the fix and passes after it;
5. run the broader unit-test target when shared runtime or Essentials behavior changed.

Game script tests and bake commands remain project-owned. Engine docs may describe the required boundary but must not cite a project test suite as normative proof.

## Client host and runtime validation

Generated native clients can use a small host executable plus a sibling runtime library. [ClientUpdater.md](ClientUpdater.md) owns the ABI, compatibility, and update protocol.

Use project-derived names in commands. The reusable validation sequence is:

1. build the generated client host target; on native platforms its dependency also builds the runtime library, which remains independently buildable for isolated checks;
2. confirm the runtime alias is adjacent to the host in the output directory;
3. launch with the bundled runtime and verify normal startup;
4. launch with `--ClientLibPath <path>` and a compatible alternate runtime;
5. request an incompatible version with `--ClientLibCompatibilityVersion <other>` and verify that missing/incompatible runtime fails rather than silently loading the wrong code;
6. provide an invalid alternate path without an explicit incompatible version and verify the documented embedded fallback;
7. run `Source/Tests/Test_ClientRuntimeApi.cpp` coverage after ABI changes.

A packaged server's platform-binary layout is a build/release concern; validate it through the selected embedding-project package after the engine ABI tests pass.

## Project launch-profile checklist

A project-owned native launch profile should record:

- generated target and configuration;
- executable path and working directory;
- selected project config/sub-config;
- resource bake/build prerequisite;
- debugger type and symbol path;
- environment variables and secrets policy;
- whether `Script.DebuggerEnabled` is expected;
- the narrow test or manual scenario that proves the profile.

Keep these values in project docs or editor configuration. Do not copy them back into this engine page as universal commands.

## Validation checklist

1. Run `Source/Tests/Test_StackTrace.cpp` and `Source/Tests/Test_ExceptionHandling.cpp` after stack-trace or crash-path changes.
2. Verify MSVC visualizers load from `BuildTools/natvis/` in a generated solution.
3. Verify AngelScript discovery defaults against `Source/Scripting/AngelScript/AngelScriptDebugger.cpp` and the bundled adapter.
4. Test one native launch with symbols and one AngelScript attach using an embedding-project configuration.
5. Run client host/runtime tests after changing the loadable client ABI.
6. Confirm this page contains no required path, binary name, launch task, or test owned only by an embedding project.

## See also

- [Testing.md](Testing.md) for engine-owned regression routing.
- [Scripting.md](Scripting.md) for AngelScript runtime ownership.
- [ExceptionSafety.md](ExceptionSafety.md) for invariant and termination policy.
- [ClientUpdater.md](ClientUpdater.md) for host/runtime and updater diagnostics.
- [WebDebugging.md](WebDebugging.md) and [AndroidDebugging.md](AndroidDebugging.md) for platform-specific paths.
