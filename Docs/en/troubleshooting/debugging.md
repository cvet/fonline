---
layout: default
title: Native and AngelScript Debugging
locale: en
document_id: debugging
permalink: /Docs/en/troubleshooting/debugging.html
---

# Native and AngelScript Debugging

This is the Engine-owned route for diagnosing native failures, mixed native/script stack traces, fatal-process diagnostics, Visual Studio data inspection, and live AngelScript execution. It follows the current build configurations, platform helpers, exception and stack-trace implementation, AngelScript endpoint, bundled VS Code adapter source, Engine tests, and checked embedding-project evidence.

An embedding project owns concrete target names, executable paths, working directories, bake prerequisites, sub-configs, credentials, crash-storage policy, editor installation, and the scenario that reproduces its game bug.

## Fast route decision

- Choose a native debugger for crashes, native exceptions, memory, threads, or
  a mixed stack whose owning frame is C++.
- Choose the AngelScript debugger for live script stepping and variables. The
  current Engine contract requires `Script.DebuggerEnabled`, exposes a TCP
  endpoint on a process-selected port in `43000..44999`, and uses UDP port
  `43001` for discovery; the project owns editor wiring and remote-access policy.
- Choose a focused Engine or project test when the failure is deterministic and
  the changed contract can be observed without an interactive attach.

An attach is diagnostic evidence. Keep the original reproduction and add a
repeatable regression route after the cause is fixed.

## Contract status

This page describes the current reusable Engine contract. Engine source and Engine-owned tests are normative. Last Frontier and FOnline TLA are pinned workflow evidence only; their launch names, binary prefixes, ports beyond Engine defaults, test suites, and product policy do not extend Engine support.

Debugging has four separate evidence layers:

1. a reproducible failure and complete original log;
2. a matching binary, runtime libraries, and native symbols;
3. live debugger or AngelScript attach evidence from the failing execution;
4. a focused regression test or repeatable project scenario after the diagnosis.

A readable stack is not proof that the executable, symbols, and source came from the same build. A successful attach is not proof that the debugger control being displayed is implemented by the live Engine transport.

## Scope and authority

The Engine owns:

- build-configuration semantics, compiler/linker symbol flags, sanitizer variants, and generated application targets;
- `IsRunInDebugger`, `BreakIntoDebugger`, native stack capture/resolution, exception callbacks, crash handlers, and the diagnostic self-test;
- mixed AngelScript/native stack layers and the current runtime debugger endpoint;
- MSVC Natvis/NatJMC files attached to generated solutions;
- the `BuildTools/angelscript-debugger` adapter source and its declared VS Code configuration schema;
- focused native tests for stack-trace and exception behavior.

The embedding project owns:

- which application, configuration, resource set, database, account, and gameplay route to launch;
- `.vscode/launch.json`, task dependencies, editor-extension installation, and multi-process naming;
- native dump collection, retention, privacy, upload, symbol-store, and incident policy;
- gameplay/script regression suites and release-platform qualification.

Web and Android have additional runtime boundaries. Use [Web Build, Packaging, and Browser Debugging](../how-to/platforms/web-debugging.md) or [Android Build, Packaging, and Device Debugging](../how-to/platforms/android-debugging.md) after proving that the symptom is platform-specific.

## Source paths inspected

The current contract was re-derived from:

- `BuildTools/cmake/stages/Init.cmake`, `EngineSources.cmake`, and `ThirdParty.cmake`;
- `BuildTools/cmake/helpers/Build.cmake` and `BuildTools/cmake/helpers/State.cmake`;
- `BuildTools/natvis/essentials.natvis`, `unordered_dense.natvis`, and `fonline.natjmc`;
- the GLM, ImGui, small-vector, and ufbx visualizers under `ThirdParty/`;
- `Source/Essentials/BasicCore.cpp`, `StackTrace.*`, `BaseLogging.*`, `FatalError.*`, `ExceptionHandling.*`, and `Logging.cpp`;
- `Source/Common/DiagnosticSelfTest.cpp` and `Source/Frontend/ApplicationInit.cpp`;
- `Source/Scripting/AngelScript/AngelScriptBackend.cpp`, `AngelScriptContext.cpp`, and `AngelScriptDebugger.*`;
- `Source/Common/Settings.inc`;
- `Source/Tests/Test_StackTrace.cpp` and `Test_ExceptionHandling.cpp`;
- `BuildTools/angelscript-debugger/package.json` and its TypeScript sources;
- exact project snapshots in `BuildTools/ExternalProjectEvidence.json`.

## Evidence layers and support matrix

| Surface | Current Engine capability | Evidence limit |
|---|---|---|
| Windows native | MSVC/clang-cl application targets, PDB emission outside `MinSizeRel`, debugger detection, `DebugBreak`, backward-cpp SEH diagnostics, generated MSVC visualizers | The Engine does not create or retain minidump files or operate a symbol server. |
| Linux native | Debug information outside `MinSizeRel`, `-rdynamic`, GDB/LLDB-compatible binaries, `/proc/self/status` debugger detection, signal/terminate diagnostics | Core-dump enablement, collection, symbol storage, container permissions, and retention are host/project policy. |
| macOS native | Debug information outside `MinSizeRel`, `-rdynamic`, `sysctl(P_TRACED)` detection, debug trap, backward-cpp signal diagnostics | No checked Engine LLDB launch profile, crash-report archive, or release qualification is supplied. |
| AngelScript runtime | Loopback-by-default TCP endpoint, UDP discovery, line breakpoints, pause/continue/step, script stack, read-only local values, stop/abort/error events | No authentication, encryption, published VSIX, pinned adapter dependency lock, live endpoint CI, global-value inspection, expression evaluation, or state mutation contract. |
| Mixed stack in logs | Script layers plus native frames, origin/catch distinction, safe crash-path output and process-local resolution cache | Native symbol quality depends on the exact binary, libraries, debug data, platform unwinder, and execution mode. MemorySanitizer disables native stack capture. |

`Source/Tests` validates stack and exception primitives. It does not currently exercise a real TCP/UDP AngelScript attach session. Project static checks and launch profiles prove integration shape, not the live protocol end to end.

## Fast route selection

| Symptom family | Start with | Proof boundary |
|---|---|---|
| Native assertion, C++ exception, signal, SEH failure, or lifecycle invariant | Matching native symbols, original log, then the smallest native target under a debugger | Focused `Source/Tests/Test_*.cpp` case when the boundary is reusable. |
| Script compile, binding, remote-call, or nullability failure | [Scripting Runtime](../explanation/scripting-runtime/) and [Testing](../contributing/testing/) before live attach | Minimal compile/bake fixture or owning test; use attach only for execution-state questions. |
| AngelScript breakpoint, stepping, script stack, or local value | Development config with `Script.DebuggerEnabled = True`, then a `fos` attach profile | Verified breakpoint/stop at the intended process and source revision. |
| Mixed script/native exception | Engine log's unified trace first, native debugger second | Preserve throw origin and catch site; isolate the reusable boundary in a native test. |
| Memory corruption, race, uninitialized read, or undefined behavior | The narrow supported sanitizer configuration before manual watch-window inspection | Reproducer under the owning sanitizer lane; debugger evidence supplements it. |
| Client host/runtime load failure | [Client Runtime Split and Updater](../explanation/runtime/client-updater.md) | Host/runtime ABI and selector tests before gameplay diagnosis. |
| Browser or Android failure | Platform guide after generic native/script behavior is ruled out | Browser/device evidence for the exact package. |

## Build configurations and symbols

`BuildTools/cmake/stages/Init.cmake` defines the reusable configuration contract. `expr_DebugInfo` is true for every native configuration except `MinSizeRel`:

- MSVC-compatible builds add `/Zi` and link with `/DEBUG:FULL` when debug information is enabled;
- Linux and macOS use `AddNativeOptimizationFlags`, which adds `-g` under the same condition;
- Linux and macOS add `-rdynamic` so executable symbols are available to runtime resolution;
- MSVC `Debug` and `RelWithDebInfo` also receive `/JMC`;
- MSVC-only `Release_Debugging` derives from `RelWithDebInfo` and adds `/dynamicdeopt` plus `/DYNAMICDEOPT`.

Do not use `MinSizeRel` for a diagnosis that requires source-level native frames. Do not mix a PDB, dSYM/DWARF file, executable, client runtime library, or native extension from different builds, even when names and commit labels look similar.

### Debug symbols are not debug semantics

`FO_DEBUG=1`, `DEBUG`, and `_DEBUG` are emitted only for `Debug`, `Debug_Profiling_Total`, `Debug_Profiling_OnDemand`, and `Debug_San_Address`. Other configurations receive `NDEBUG` and `FO_DEBUG=0`, even though most still carry debug information.

This distinction matters:

- `RelWithDebInfo` is normally the best first reproduction for release-like behavior with symbols;
- `Debug` changes assertions, CRT selection, optimization, and timing and can hide or expose a different failure;
- `Release_Ext` is the full-optimization/LTO route and still has native debug information, but stepping and local inspection may be degraded;
- `Release_Debugging` is an MSVC-specific dynamic-deoptimization route, not a cross-platform configuration name.

### Windows

Use Visual Studio or a `cppvsdbg` profile with the exact generated executable, sibling runtime libraries, native extensions, and PDBs. Keep the working directory at the embedding-project root unless its generated config explicitly says otherwise. Break on thrown C++ exceptions only when the exception itself is unexpected; expected throw-as-signal paths can be diagnosed at their reporter or invariant boundary.

Generated MSVC projects include Engine Natvis and NatJMC inputs automatically. A copied executable without its matching PDB and libraries is not a complete diagnostic artifact.

### Linux

Use GDB or LLDB against the exact executable and shared objects. Preserve the original environment, working directory, config, resource paths, and allocator/sanitizer selection. The Engine adds `-rdynamic`; non-PIE is used for most normal executable routes, while baker/client-library and MemorySanitizer-related targets have relocation requirements that differ.

When a crash occurred outside the debugger, retain the Engine log before attempting a second run. An OS core is additional evidence only when the host was configured to produce and preserve it.

### macOS

Use LLDB with the matching executable, libraries, and debug data. `IsRunInDebugger` checks `P_TRACED` through `sysctl`, and `BreakIntoDebugger` uses `__builtin_debugtrap`. The Engine source is capable of native symbol/stack diagnostics, but the repository does not currently claim a checked macOS editor profile or crash-artifact lane.

### Sanitizer and platform limits

Use [Testing](../contributing/testing/) for the exact sanitizer matrix. The main debugging interactions are:

- MSVC supplies `San_Address` and `Debug_San_Address`;
- native Clang supplies Address, Memory, Memory-with-origins, Undefined, Thread, DataFlow, and Address+Undefined configurations where the toolchain supports them;
- AddressSanitizer, MemorySanitizer, and code-coverage builds switch AngelScript to `AS_MAX_PORTABILITY` so native call trampolines do not defeat instrumentation or terminate while an instrumented frame unwinds a registered-function exception;
- MemorySanitizer builds compile the stack/exception layer with `HAS_NATIVE_TRACE=0`; expect sanitizer diagnostics, not the normal native mixed-stack contract;
- sanitizer timing, allocation, stack size, and calling-convention behavior differ from a release build, so reproduce the original configuration as well.

## Native debugging

### Launch, attach, and reproduce

1. Record the exact Engine revision, embedding-project revision, target, configuration, config/sub-config, command line, working directory, and resource revision.
2. Preserve the first failing log and any OS diagnostic before adding logging or changing build mode.
3. Reproduce in `RelWithDebInfo` with matching symbols unless debug-only semantics are the subject of the bug.
4. Launch under the native debugger when the Engine's debugger-aware behavior matters. Late attach can observe process state, but it does not refresh the Engine's cached debugger-presence decision.
5. Stop at the narrow invariant, throw site, sanitizer report, or faulting instruction. Inspect the full thread set, not only the selected frame.
6. Reduce the failure to the smallest Engine test or project scenario that preserves it.
7. Re-run the original configuration after the fix; a Debug-only success is not release-like acceptance.

### Exceptions, assertions, and memory failures

`ReportExceptionAndContinue` records a non-fatal caught exception. `ReportExceptionAndExit` and strong assertions record diagnostics and terminate or break according to their contract. The exception-safety tier model and entity-lifecycle throw-as-signal rules live in [Exception Safety](../contributing/coding-contracts/exception-safety.md).

Use break-on-throw carefully. AngelScript bindings and engine lifecycle code can throw as part of an intentional reporting path. Start from the log's fixed message and context parameters, then place a focused breakpoint at the owning invariant or reporter. For memory corruption, prioritize ASan/MSan/UBSan/TSan evidence and the first invalid access over a later secondary assertion.

### Core and minidump boundary

The Engine writes crash diagnostics to its log. It does not currently create Windows minidumps, configure Linux core limits, collect macOS crash reports, upload dumps, or manage a symbol store.

An embedding project or operator may add those facilities, but must define:

- exact executable/library/symbol provenance;
- dump enablement and storage location;
- retention, access control, encryption, and deletion;
- treatment of credentials, player data, chat, network buffers, and memory-resident secrets;
- upload failure behavior and incident ownership;
- a restore/replay procedure that does not require production credentials.

Do not describe a platform's default crash reporter as an Engine-owned guarantee.

## Debugger detection and debugger breaks

`IsRunInDebugger()` is process-cached on its first call:

- Windows uses `IsDebuggerPresent()`;
- Linux reads `TracerPid` from `/proc/self/status`;
- macOS queries `KERN_PROC_PID` and tests `P_TRACED`.

`BreakIntoDebugger()` emits `DebugBreak`, `__builtin_debugtrap`, or `SIGTRAP` only when that cached result is true. Because exception handling asks this question during early process initialization, launching outside a native debugger and attaching later is not guaranteed to make Engine-triggered breaks active.

When a debugger is detected at startup, the Engine does not install backward-cpp fatal signal/SEH handling. This lets the native debugger receive the fault directly, but it also means the normal out-of-debugger fatal crash-to-log path is not the evidence to expect from that run. Preserve one non-debugger crash run when the crash-log contract itself is under test.

The AngelScript debugger is independent of `IsRunInDebugger`; attaching the `fos` adapter does not make the process native-debugger-aware.

## Visual Studio Visualizers

Generated MSVC solutions attach these visualizers without a manual Visual Studio install step:

- `BuildTools/natvis/essentials.natvis`: Engine borrow/owner pointers, `propagate_const`, stack data, engine exceptions, hashed strings, colors, positions, and time values;
- `BuildTools/natvis/unordered_dense.natvis`: `ankerl::unordered_dense` tables and segmented vectors;
- `BuildTools/natvis/fonline.natjmc`: Engine Just My Code classification;
- vendored visualizers for GLM, ImGui, `gch::small_vector`, and ufbx.

`BuildTools/cmake/stages/EngineSources.cmake` attaches the Engine visualizers, and `ThirdParty.cmake` attaches supported dependency visualizers only for MSVC-generated projects. Natvis improves inspection; it does not change object lifetime, pointer validity, or optimizer behavior.

## Visual Studio Solution Folders

For MSVC CMake generators, a target should be created while its intended `CMAKE_FOLDER` is active. Repository helpers and the final regrouping pass place application, command, core-library, and third-party targets in generated solution folders. Folder placement is navigation only and has no effect on symbols or linkage.

## Quick Validation

1. Build a narrow native target outside `MinSizeRel` and confirm its matching symbol artifact exists.
2. Launch it under the native debugger from the embedding-project root.
3. Inspect an Engine pointer and `StackTraceData`; confirm the appropriate visualizer is loaded on MSVC.
4. Trigger or stop at a controlled exception/assertion path and compare the debugger location with the Engine log.
5. Run a second out-of-debugger diagnostic self-test only in an isolated workspace when the crash-log route itself must be proved.

## Stack Trace Architecture

The Engine captures a bounded native return-address array and optional pre-resolved script layers in `StackTraceData`. Native symbol resolution is deferred until formatting or explicit resolution. Resolved native frames are cached process-wide by instruction address under a bounded cache so repeated reports do not reload the same symbol information unnecessarily.

`FO_STACK_TRACE_ENTRY()` is not a manual thread-local call stack. Outside Tracy configurations it contributes no stack frame; under Tracy it expands to a profiling zone. Native call stacks come from platform capture at the moment `GetStackTrace()` runs.

### AngelScript bridge

`AngelScriptContext.cpp` registers a script stack provider without making Essentials depend on AngelScript headers. The provider walks the active context and parent context chain, resolves each function declaration and original `.fos` file/line through the preprocessor translator, and preserves the native birth anchors used to splice nested script re-entry into the native stack.

Script frames are captured eagerly because an AngelScript context can be reused or changed after capture. The captured layers live behind immutable shared storage so copying an Engine exception remains noexcept.

### Unified frame ordering

The formatted trace is most-recent first and can interleave native bridges with nested script layers:

```text
[Native] code below the active script/native bridge
[Script] active child context
[Native] bridge between child and parent contexts
[Script] parent context
[Native] caller and process entry
```

Simple traces with no native birth anchors place script frames before the native tail. `FormatStackTrace` marks every frame `[Script]` or `[Native]`; safe crash output falls back to hexadecimal addresses when full resolution is unavailable.

### API surface

| Function | Purpose |
|---|---|
| `GetStackTrace()` | Capture native addresses and currently available script layers. |
| `GetStackTraceEntry(deep)` | Resolve one unified frame by zero-based depth. |
| `ResolveStackTrace(st)` | Resolve and interleave all captured frames. |
| `FormatStackTrace(st)` | Produce the human-readable mixed trace. |
| `SafeWriteStackTrace(st)` | Write through the low-allocation crash/log path with address fallback. |
| `ClearResolvedStackTraceCache()` | Clear process-wide resolved native entries. |
| `GetResolvedStackTraceCacheSize()` | Inspect the current cache size for tests/diagnostics. |
| `SetScriptStackTraceProvider(provider)` | Install or clear the higher-layer script provider. |
| `HasScriptStackTraceProvider()` | Observe provider registration in tests. |

`BaseEngineException` captures its origin trace at construction. This is why a later catch/report can retain the throw site instead of replacing it with only the reporter's stack.

### Exception reporting and deferred formatting

`MakeErrorStackTrace()` produces `CatchedStackTraceData`: an optional origin from `BaseEngineException` plus a fresh catch-site trace. Formatting uses the origin when present and marks the catch location; non-Engine exceptions have only the catch-site trace.

The exception callback receives the message, already-captured `CatchedStackTraceData`, and fatal flag. Integrations that forward diagnostics must resolve/copy the data while its provenance is still known and must preserve script/native frame identity.

### Logging and crash-path primitives

Normal exception callbacks use the structured logging path. Immediate repeated exception messages are collapsed into a later count. Fatal and low-memory paths use synchronous base logging and `SafeWriteStackTrace`; if formatting or symbol resolution fails, raw addresses are retained instead of suppressing the report.

`Common.AsyncLogWrite` controls normal asynchronous log delivery. Fatal crash output suspends it and flushes synchronously so a headless process does not depend on `stderr` or an unfinished writer thread.

Explicit low-level fatal exits use `ReportFatalAndExit` or `ReportStrongAssertAndExit` from `FatalError.cpp`. This early layer follows `StackTrace` and `BaseLogging`, writes one synchronous native report, and delegates only process termination to `ExitApp(false)`, avoiding a reverse dependency on `ExceptionHandling`. Raw `ExitApp(false)` remains status-only: controlled compiler/input failures can return a non-zero status without being mislabeled as crashes, while true fatal callers report before exiting.

### Crash-to-log guarantee and self-test

Outside a native debugger, backward-cpp handles supported Windows SEH failures and POSIX fatal signals/termination on Windows, Linux, and macOS. The Engine adds a crash reason, captures a stack, switches to synchronous log writes, and exits through the crash path. Long-lived Engine worker threads install a POSIX alternate signal stack so stack-overflow diagnostics have space to run. Third-party-created threads need the same setup before executing deeply recursive Engine work.

`FO_SELFTEST_CRASH` is an environment-only destructive diagnostic hook run during application initialization after logging and exception callbacks are ready. Supported base modes are `main_null_read`, `main_null_write`, `main_wild_write`, `main_stack_overflow`, `main_fpe`, `main_abort`, `main_noexcept_throw`, `main_throw`, `main_strong_assert`, `main_basic_strong_assert`, `main_fatal_exit`, and `main_failure_exit`; replace `main_` with `thread_` to run the corresponding worker-style thread route.

Run it only against an isolated disposable process and workspace. It intentionally crashes or terminates the process. An unknown mode logs a warning and continues. The Engine repository does not itself provide a subprocess acceptance runner; checked Last Frontier evidence exercises the Linux headless route, but that project test is not normative Engine proof.

### Coverage

`Source/Tests/Test_StackTrace.cpp` covers provider registration, script-layer order, nested native/script interleaving, truncation, formatting, cache reuse/eviction behavior, individual entry lookup, safe writing, and throwing-provider containment. `Test_ExceptionHandling.cpp` covers Engine exception payloads, origin/catch behavior, callback replacement, and fatal/non-fatal reporter inputs.

A recoverable ImGui assertion carries only the stringified expression. `ImGuiExt::Init` therefore installs an error callback that logs `ImGui error in window '<name>': <message>` immediately before the assertion fires. For an unbalanced `Begin`/`End` in a headless client or mapper test, use that line to identify the owning window; ImGui's own debug log is unavailable because `IMGUI_DISABLE_DEBUG_TOOLS` is enabled.

The current Engine suite does not open the AngelScript TCP/UDP endpoint, attach the VS Code adapter, validate Natvis in Visual Studio, or execute every crash mode as a subprocess. Those are explicit integration gaps, not implied by the native unit-test result.

## Network latency emulation

`Network.ArtificalLags` (milliseconds, `0` disables) delays both inbound and outbound client batches in `ClientConnection::ProcessConnection`. Each batch independently samples `ArtificalLags / 2 .. ArtificalLags`; `Network.ArtificalLagsJitter` adds another `0 .. jitter` milliseconds. The emulation delays delivery but does not throttle the network pump.

Both directions are required to reproduce authority divergence. Inbound delay makes the client learn server state late; outbound delay makes the server learn client actions late. Because related messages receive independent samples, their delay difference produces divergence even though a truly fixed equal delay would cancel. Use a modest base and larger jitter to model occasional stalls.

Server-to-client movement includes an `offset_time`, allowing the client to fast-forward a late movement. Client-to-server movement has no elapsed-time field; `Process_Move` starts it from the server's current frame time, so the authoritative critter trails the client by roughly one-way delay while walking. Settings participate in the generated compatibility hash; adding or renaming one does not require a manual compatibility-marker edit.

## AngelScript debugger

### Enablement and runtime cost

Set `Script.DebuggerEnabled = True` only in a development config or command-line override. The default is `False`. When enabled, `AngelScriptBackend` retains line cues, disables bytecode optimization, creates the endpoint, and installs a line callback on script contexts.

This changes script build/execution characteristics and adds line-processing overhead. Do not enable it in production, benchmarks, or acceptance runs that claim normal script performance. The AngelScript compile-time `AS_DEBUG` define follows native Debug configurations and is separate from the runtime `Script.DebuggerEnabled` setting.

### Endpoint and discovery contract

The runtime:

- binds TCP to `Script.DebuggerBindHost`, whose Engine default is `127.0.0.1`;
- selects one port in `43000..44999`, starting from `process_id % 2000`;
- advertises a newline-delimited JSON protocol version `1`;
- answers UDP probe `fos-debug-discover-v1` on port `43001`;
- advertises process id as `<pid>:<tcp-port>` and target role as `server`, `client`, or `mapper`;
- accepts one active TCP debug session at a time.

The VS Code attach configuration accepts `processId`, a direct `endpoint` such as `tcp://127.0.0.1:43042`, `discoveryPort` (default `43001`), and `discoveryTimeoutMs` (default `800`). Desktop discovery requires Node.js UDP support. The current Engine endpoint is TCP only even though the adapter parser also recognizes pipe and Unix-socket endpoint strings for other transports.

### Attach capability matrix

| VS Code action | Live Engine attach status | Notes |
|---|---|---|
| Discover/select server, client, or mapper | Supported | Use the advertised `<pid>:<port>` when several instances exist. |
| Line breakpoint | Supported | The Engine keys breakpoints by source file basename, so duplicate `.fos` filenames are ambiguous. |
| Pause / continue | Supported | Pause takes effect at the next AngelScript line callback, not while no script line is executing. |
| Step in / over / out | Supported | Operates on script context depth and original preprocessor-resolved lines. |
| Script stack trace | Supported while stopped | The attach response exposes script frames; use the Engine log/native debugger for the unified native stack. |
| Local variables | Read-only, supported while stopped | Values are formatted snapshots per script frame. |
| Script globals | Not implemented | The adapter's Globals scope contains attach metadata, not live AngelScript globals. |
| Hover/evaluate/expression | Not a live Engine contract | Current attach mode can fall back to adapter-local/mock behavior. Do not use it as process evidence. |
| Set variable/expression, memory read/write, data/instruction/function breakpoints, reverse execution | Not implemented by live attach | Some controls are advertised by the shared adapter because its mock launch runtime supports them; attach-mode errors or placeholder behavior do not extend Engine capability. |
| Exception/abort/error stop | Supported as runtime events | Inspect the Engine log for the complete exception and mixed trace. |

The source editor uses normal one-based lines; the adapter and endpoint translate internally to zero-based protocol lines. Breakpoint verification currently confirms accepted line numbers, not that a given source basename is unique or executable in the active module.

### Security boundary

The debugger protocol has no authentication, authorization, confidentiality, or integrity protection. Discovery also reveals a process role and attach endpoint. Keep `Script.DebuggerBindHost = 127.0.0.1` unless an explicit, temporary, trusted-network review permits a different bind.

Never expose TCP `43000..44999` or UDP `43001` to the public Internet, an untrusted LAN, a production pod/service, or a shared CI runner. For remote work, keep the Engine bound to loopback and use an authenticated transport owned by the operator, then configure an explicit local endpoint. Do not pass credentials in debugger config or log evidence.

### Adapter delivery status

`BuildTools/angelscript-debugger` is currently source-capable tooling, not a production-distributed editor product:

- `package.json` is private, version `0.1.0`, and supplies typecheck/build/package scripts;
- the repository has no adapter dependency lock file, checked VSIX, marketplace publication record, or required adapter build job;
- the TypeScript test file exercises the adapter's sample/mock runtime, not the live Engine endpoint;
- attach transport requires the desktop Node.js debug-adapter runtime.

An embedding project may build and review a local VSIX, but must own the selected Node/npm versions, resolved dependency lock, extension artifact hash, install/upgrade path, and editor compatibility. Until the Engine adds those artifacts and a live attach gate, do not call the adapter installation reproducible or release-qualified.

### Multi-process selection

Client, server, and mapper instances share UDP discovery port `43001` and choose different TCP ports from the process-derived range. Prefer a process-specific selection rather than attaching to the first response. For deterministic automation, read the runtime log's `AngelScript debugger TCP endpoint` line and use an explicit endpoint.

Use unique script filenames across debugger-relevant source roots. Because the Engine's breakpoint table uses only the extracted filename, paths such as `Scripts/Admin/State.fos` and `Scripts/Client/State.fos` cannot be independently targeted by the current transport.

### Attach troubleshooting

1. Confirm the selected process actually received `Script.DebuggerEnabled = True`; a compound launch name alone does not enable it.
2. Confirm the log contains both the TCP endpoint and UDP discovery port lines.
3. Verify the bind remains loopback unless remote exposure was explicitly reviewed.
4. When discovery finds nothing, use the logged direct TCP endpoint and check local firewall/extension-host UDP behavior.
5. When several targets appear, select the advertised role and `<pid>:<port>` deliberately.
6. Confirm editor sources match the baked script revision loaded by the process.
7. Rename duplicate `.fos` basenames before trusting line breakpoints.
8. Treat unavailable globals, mutation, memory, hover/evaluate, and advanced DAP controls as current transport limits.
9. If stepping changes behavior, reproduce again with the debugger disabled because line cues and bytecode optimization differ.

## Debugger integration in an embedding project

The project should expose independent routes for:

- native launch under a debugger with the exact generated executable and symbols;
- native attach when process startup cannot be debugger-owned, with the cached-detection limitation documented;
- AngelScript attach to an already running development process;
- a compound native launch plus `fos` attach when both views are needed;
- Web/Android launch only for platform-specific symptoms;
- isolated unit-test launch and destructive crash-diagnostic subprocesses.

Keep binary prefixes, paths, tasks, databases, accounts, ports, and game sub-configs in project-owned files. The reusable requirement is the field/validation contract, not a particular `.vscode` name.

## Project launch-profile checklist

A maintained native profile records:

- target, configuration, executable, runtime libraries, symbol source, and working directory;
- config/sub-config and every command-line override;
- configure/build/bake prerequisites and whether they can create a clean build tree;
- debugger type (`cppvsdbg`, GDB/LLDB through `cppdbg`, or another reviewed frontend);
- environment variables, with secrets excluded from source and reports;
- launch-versus-attach behavior and the late-attach limitation;
- a narrow scenario that proves the profile.

A maintained AngelScript profile additionally records:

- how `Script.DebuggerEnabled = True` is applied to the intended process;
- loopback `Script.DebuggerBindHost` policy;
- discovery port/timeout or explicit endpoint selection;
- multi-instance selection and duplicate-filename policy;
- adapter version, dependency/artifact provenance, and installation route;
- supported attach controls and a live breakpoint/stack/local-value acceptance check.

Static validation should reject missing task/compound references, stale setting names, a non-loopback default, and profiles that offer `fos` attach without enabling the endpoint.

## Engine test validation

For a reusable native regression:

1. select or add the smallest `Source/Tests/Test_*.cpp` case;
2. build the embedding project's generated unit-test target with matching symbols;
3. run the exact Catch2 case that reproduced the failure;
4. run the broader Engine unit-test target when Essentials, scripting, threading, or shared runtime behavior changed;
5. run the appropriate sanitizer lane for memory/concurrency/undefined-behavior defects;
6. repeat the original application scenario after the test is green.

Game scripts, content, bake commands, process names, and gameplay fixtures remain project-owned. A project test can demonstrate compatibility but cannot be the sole normative proof for Engine behavior.

## Client host and runtime validation

Native clients can use a small host executable plus a sibling client runtime library. Diagnose host/runtime loading independently from gameplay:

1. build the host and runtime from one revision/configuration;
2. confirm the expected runtime alias and matching symbols are adjacent to the host;
3. launch the bundled pair;
4. test an explicit compatible `--ClientLibPath`;
5. test an incompatible `--ClientLibCompatibilityVersion` and verify failure rather than silent wrong-library loading;
6. test an invalid alternate path and the documented embedded fallback;
7. run `Source/Tests/Test_ClientRuntimeApi.cpp` after ABI or selector changes.

Package layout and updater rollout are owned by [Packaging and Release](../how-to/release/packaging.md) and [Client Runtime Split and Updater](../explanation/runtime/client-updater.md).

## Project evidence and extraction rules

`BuildTools/ExternalProjectEvidence.json` pins both project snapshots. Current evidence shows:

- Last Frontier keeps Windows/Linux native launch profiles, an explicit `fos` attach profile, compounds that launch with `--Script.DebuggerEnabled True`, a loopback base bind, and a checked static workflow validator. Its Linux pipeline also exercises Engine crash self-test modes. These are strong project practices but remain project-owned.
- FOnline TLA independently carries Windows/Linux native profiles and `fos` compounds. At the pinned revision, the compounds do not themselves enable `Script.DebuggerEnabled`, while its base config disables the debugger and binds it to `0.0.0.0`. This is useful negative compatibility evidence, not a template to promote.

Reusable rules were re-derived from Engine source. Never copy Last Frontier target names into Engine docs, never promote TLA's wildcard bind, and never infer live attach coverage from a static launch file. A project revision change requires re-verifying the complete cited files before updating the evidence decision.

## Troubleshooting by layer

| Observation | Likely layer | Next action |
|---|---|---|
| Breakpoints are hollow and no endpoint lines exist | Endpoint not enabled or startup failed | Verify effective `Script.DebuggerEnabled`, then inspect startup logs and port availability. |
| Discovery is empty but TCP endpoint is logged | UDP/firewall/extension-host issue | Attach to the exact logged `tcp://127.0.0.1:<port>` endpoint. |
| Wrong client/server/mapper stops | Multi-instance selection | Select the advertised role and `<pid>:<port>`; avoid first-response automation. |
| Breakpoint stops in another file with the same name | Basename collision | Rename one `.fos` file; current Engine breakpoints are basename-keyed. |
| Globals or hover values look synthetic | Adapter feature exceeds live attach transport | Use read-only locals, logs, or native inspection; do not treat the value as Engine evidence. |
| Native frames are addresses only | Missing/mismatched symbols or resolver limitation | Match binary/libraries/debug data and inspect platform unwinder availability. |
| Crash appears in debugger but no `FATAL ERROR!` log | Process started under native debugger | Expected debugger-aware route; reproduce once outside the debugger to test crash logging. |
| Engine-triggered break does not fire after attach | Debugger presence was cached before late attach | Relaunch under the native debugger. |
| MemorySanitizer trace lacks native frames | Intentional `HAS_NATIVE_TRACE=0` configuration | Use MSan report and a matching non-MSan symbolized reproduction. |
| Debug build passes but release-like build fails | Semantic/optimization/timing difference | Reproduce in `RelWithDebInfo`, then sanitizer or `Release_Debugging` where supported. |
| Dump/core is missing | Host/project collection not configured | Configure the OS/operator-owned dump route; Engine only guarantees its documented log path. |

## Maintenance triggers

Re-audit this page in the same change when modifying:

- configuration names, `expr_DebugInfo`, `expr_DebugBuild`, symbol/linker flags, sanitizer wiring, PIE/LTO, or output layout;
- `IsRunInDebugger`, `BreakIntoDebugger`, stack capture/resolution/cache, exception callbacks, crash handlers, logging flush, alternate signal stacks, or `FO_SELFTEST_CRASH` modes;
- Engine or third-party Natvis/NatJMC files and their CMake attachment;
- `Script.DebuggerEnabled`, `Script.DebuggerBindHost`, AngelScript line cues/optimization, context setup, endpoint ports/protocol/commands/events, breakpoint keys, stack/locals, or security boundary;
- adapter schema, discovery/transport, DAP capability mapping, dependency/toolchain delivery, tests, or publication;
- project launch/evidence files cited by `ExternalProjectEvidence.json`.

Update the canonical English and Russian pages together, refresh the normalized translation source hash, regenerate external evidence, snippets, locale/site/search/routes, and AI delivery, then run the focused debugging gate plus aggregate documentation validation. Runtime changes additionally require the owning native, TypeScript/adapter, process, and project integration tests.

## Validation checklist

1. Run `BuildTools/tests/test_docs_debugging.py` and aggregate documentation tests.
2. Run `Source/Tests/Test_StackTrace.cpp` and `Test_ExceptionHandling.cpp` after native stack/exception changes.
3. Run the relevant sanitizer and `Test_ClientRuntimeApi.cpp` lanes when those boundaries changed.
4. Confirm PDB/DWARF artifacts and MSVC visualizers from a freshly generated project.
5. Prove one native launch under a debugger and one out-of-debugger crash-log route on every platform whose behavior changed.
6. Prove one live AngelScript attach: endpoint log, deliberate process selection, breakpoint, pause/step, script stack, and read-only locals.
7. Verify advanced adapter controls remain described according to the live Engine transport, not the mock runtime.
8. Confirm debugger bind is loopback, no credentials are present, and dump/log evidence follows project privacy policy.
9. Re-run exact checked project evidence and keep project-specific names out of the Engine procedure.

## See also

- [Testing](../contributing/testing/) for unit, sanitizer, coverage, and integration boundaries.
- [Profiling](../how-to/quality/profiling.md) for Tracy capture after the correctness boundary is understood.
- [Scripting Runtime](../explanation/scripting-runtime/) for AngelScript ownership and execution.
- [Exception Safety](../contributing/coding-contracts/exception-safety.md) for invariant and termination policy.
- [Client Runtime Split and Updater](../explanation/runtime/client-updater.md) for host/runtime diagnostics.
- [Web Build, Packaging, and Browser Debugging](../how-to/platforms/web-debugging.md) and [Android Build, Packaging, and Device Debugging](../how-to/platforms/android-debugging.md) for platform-specific routes.
