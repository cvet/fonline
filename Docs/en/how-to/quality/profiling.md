---
layout: default
title: Profiling
locale: en
document_id: profiling
permalink: /Docs/en/how-to/quality/profiling.html
---

# Profiling

> Engine-owned documentation for reusable Tracy instrumentation, capture
> boundaries, and comparable performance measurements. Workload scenes,
> launch orchestration, acceptance budgets, and retained reports belong to the
> embedding game project.

## Purpose

Use this guide when a client, server, tool, or baker is slow and a timing
capture is more useful than a debugger trace. The Engine supplies Tracy
instrumentation and dedicated build configurations. A game project supplies
the process topology and a deterministic workload.

The central rule is simple: instrument only the process being measured. A
profiled client should connect to a regular server; a profiled server should
be driven by a regular client. This keeps one Tracy endpoint, one process
timeline, and one attribution boundary per capture.

The Regular counterpart participates through the ordinary game client/server
connection. It neither opens nor owns the Tracy endpoint and never serves as a
placeholder holder for the Tracy port.

## Capture decision

1. Select `Profiling_OnDemand` for a bounded manual capture or
   `Profiling_Total` when startup must be included. On a single-config generator,
   choose the profiling configuration at configure time rather than expecting a
   later `--config` switch to replace it.
2. Run one Profiled process and a Regular counterpart so only the measured
   process owns the default Tracy port and timeline. The Regular counterpart
   neither opens nor owns a Tracy endpoint. For a client capture, start the
   Regular server and wait for readiness, reject a stale instrumented process
   immediately before launching the Profiled client, then launch that client.
   For a server capture, reject a stale instrumented process immediately before
   launching the Profiled server, wait for readiness, and only then drive it
   with the Regular client.
3. Fix revisions, configuration, workload, input, map/data state, duration, and
   warm-up condition. Record one capture at a time and repeat the same case at
   least three times before comparing a representative result.

The role-specific startup order is exact:

| Capture | Ordered startup |
|---|---|
| Client | Start the Regular server and wait for readiness; reject a stale process on the default Tracy port; launch the Profiled client; then start `tracy-capture`. |
| Server | Reject a stale process on the default Tracy port; launch the Profiled server and wait for readiness; start the Regular client workload driver; then start `tracy-capture`. |

The stale-port check occurs immediately before the Profiled process in both
routes. Never move it earlier than the Regular-server readiness gate in the
client route or after the Profiled process merely because the capture tool
itself starts later.

## Source paths inspected

- `BuildTools/cmake/stages/Init.cmake`
- `BuildTools/cmake/stages/ThirdParty.cmake`
- `Source/Essentials/BasicCore.h`
- `Source/Essentials/StackTrace.h`
- `Source/Essentials/Logging.cpp`
- `Source/Essentials/MemorySystem.cpp`
- `Source/Essentials/Threading.h`
- `Source/Frontend/ApplicationInit.cpp`
- `Source/Frontend/Application.cpp`
- `Source/Frontend/ApplicationHeadless.cpp`
- `Source/Client/Client.cpp`
- `Source/Server/Server.cpp`
- `Source/Scripting/AngelScript/AngelScriptContext.cpp`
- `Source/Applications/BakerLib.cpp`
- `ThirdParty/tracy/CMakeLists.txt`
- `ThirdParty/tracy/NEWS`
- `ThirdParty/tracy/public/common/TracyVersion.hpp`
- `Examples/MinimalMultiplayer/CMakeLists.txt`
- `Examples/MinimalMultiplayer/CMakePresets.json`
- `Examples/MinimalMultiplayer/FOnlineMinimalMultiplayer.fomain`
- `Examples/MinimalMultiplayer/README.md`
- `Examples/MinimalMultiplayer/run_tutorial_smoke.py`
- upstream Tracy v0.13.1 [`capture.cpp`](https://github.com/wolfpld/tracy/blob/v0.13.1/capture/src/capture.cpp) and [`csvexport.cpp`](https://github.com/wolfpld/tracy/blob/v0.13.1/csvexport/src/csvexport.cpp)

## What the Engine instruments

Profiling configurations compile `TracyClient` into the Engine essentials
layer and define `FO_TRACY=1`. Non-profiling configurations define
`FO_TRACY=0`; the Engine profiling macros then compile away.

| Signal | Current Engine behavior |
|---|---|
| Process identity | `ApplicationInit` sends `FO_NICE_NAME` to Tracy. |
| Native CPU zones | `FO_STACK_TRACE_ENTRY()` becomes `ZoneScoped`; the named form becomes `ZoneScopedN`. |
| AngelScript CPU zones | Executed script calls emit zones using the original file, line, and declaration. Suspended contexts restore their script zone stack when execution resumes. |
| Frames | Visible and headless `Application::EndFrame()` paths emit `FrameMark`. |
| Client plot | `Client FPS` is emitted from `ClientEngine::MainLoop()`. |
| Server plot | `Server jobs per second` is emitted from the server statistics job. |
| Log messages | Engine log records are also sent as Tracy messages. |
| Threads | Engine thread names are visible in Tracy and in tagged log lines. |
| Memory | Engine allocations routed through the Tracy-aware rpmalloc path emit allocation/free events. |

The current first-party integration does not add renderer GPU zones or
Tracy lock wrappers. A CPU capture therefore must not be reported as GPU
timing or lock-contention proof. Use renderer-specific tools or deliberately
add scoped instrumentation when that evidence is required.

Plain C-library allocations made outside the Engine allocation wrappers are
not automatically attributed to the Engine heap. See
[Essentials](../../reference/native/essentials.md#memory-pointers-and-lifetime-utilities) before
interpreting an allocation capture as complete process memory accounting.

## Build configurations

`BuildTools/cmake/stages/Init.cmake` adds four configurations:

| Configuration | Base | Tracy mode | Use |
|---|---|---|---|
| `Profiling_OnDemand` | `RelWithDebInfo` | on demand | Normal captures after startup and warm-up. |
| `Profiling_Total` | `RelWithDebInfo` | continuous | Startup, initialization, and first-frame captures. |
| `Debug_Profiling_OnDemand` | `Debug` | on demand | Diagnose instrumentation or debug-only behavior, not a performance baseline. |
| `Debug_Profiling_Total` | `Debug` | continuous | Diagnose debug startup with full tracing. |

On-demand mode defines `TRACY_ON_DEMAND`, so tracing starts only after a
profiler connection. It is the default choice for repeatable steady-state
measurements. Total mode records from process startup and is appropriate only
when startup is part of the question. Restart a total-mode process before a
new capture.

For a multi-config generator, select the profile with `cmake --build
--config`. For a single-config generator, set `CMAKE_BUILD_TYPE` to the
profiling configuration at configure time. Passing `--config` to a
single-config build does not turn an already configured release binary into a
Tracy binary.

## Prepare matching Tracy tools

The Engine currently vendors Tracy `0.13.1`. The checkout is intentionally
pruned to the client library under `ThirdParty/tracy`; it does not contain the
upstream `capture`, `csvexport`, or GUI profiler sources. The capture protocol
must match, so use the
[Tracy v0.13.1 release](https://github.com/wolfpld/tracy/releases/tag/v0.13.1)
or build tools from that exact tag.

The documented capture and export flags are pinned to the same tag's
[`capture.cpp`](https://github.com/wolfpld/tracy/blob/v0.13.1/capture/src/capture.cpp)
and [`csvexport.cpp`](https://github.com/wolfpld/tracy/blob/v0.13.1/csvexport/src/csvexport.cpp),
not inferred from a newer local installation.

The following creates headless capture and CSV-export tools outside authored
source:

```bash
cmake -E make_directory Workspace/Tracy
git clone --depth 1 --branch v0.13.1 https://github.com/wolfpld/tracy.git Workspace/Tracy/src
cmake -S Workspace/Tracy/src/capture -B Workspace/Tracy/capture -DCMAKE_BUILD_TYPE=Release -DNO_FILESELECTOR=ON
cmake --build Workspace/Tracy/capture --config Release
cmake -S Workspace/Tracy/src/csvexport -B Workspace/Tracy/csvexport -DCMAKE_BUILD_TYPE=Release -DNO_FILESELECTOR=ON
cmake --build Workspace/Tracy/csvexport --config Release
```

`NO_FILESELECTOR=ON` avoids pulling GUI file-dialog dependencies into the two
headless command-line tools. The output directory differs between
single-config and multi-config generators; locate `tracy-capture` and
`tracy-csvexport` after the build and put them on `PATH`. Use the matching
release's GUI profiler to inspect saved `.tracy` files.

Do not silently use a different tool release. Tracy rejects an incompatible
handshake, and a tool that can open an older file is not proof that its live
capture protocol matches the instrumented process.

## Choose one measurement boundary

| Question | Profiled process | Regular counterpart |
|---|---|---|
| Rendering, visibility, UI, input, client scripts, or client networking | desktop client | headless or desktop server |
| Authority, AI, simulation, persistence, server scripts, or server networking | headless or desktop server | standalone client workload driver |
| Resource-baking throughput | baker application | no runtime counterpart |
| Startup and first frame | affected process in `Profiling_Total` | only the minimum dependencies needed to reach startup |

Do not profile an embedded server and client in the same process when the
question requires client/server attribution. Do not build both standalone
processes with Tracy on the default port and then guess which one accepted the
capture connection.

Build and bake before the measurement window. An unexpected in-process rebake,
shader warm-up, cache population, or updater operation is part of the capture
only when that operation is the stated workload.

## Build a profiled sample

The engine-owned minimal multiplayer project gives the commands concrete
target names without depending on a private game. On Windows:

```powershell
Set-Location Examples\MinimalMultiplayer
cmake --preset windows
cmake --build Build\windows --config RelWithDebInfo --target BakeResources FOMM_ServerHeadless
cmake --build Build\windows --config Profiling_OnDemand --target FOMM_Client
```

On Linux:

```bash
cd Examples/MinimalMultiplayer
cmake --preset linux
cmake --build Build/linux --config RelWithDebInfo --target BakeResources FOMM_ServerHeadless
cmake --build Build/linux --config Profiling_OnDemand --target FOMM_Client
```

This prepares a profiled client and a regular server. To profile the server,
reverse the configurations:

```bash
cmake --build Build/linux --config RelWithDebInfo --target BakeResources FOMM_Client
cmake --build Build/linux --config Profiling_OnDemand --target FOMM_ServerHeadless
```

Project target names are derived from `FO_DEV_NAME`; `FOMM_*` names are valid
only for this engine-owned sample.

## Capture a client

Use separate terminals and make `Examples/MinimalMultiplayer/Build/windows`
the runtime working directory, matching the generated target and smoke-test
contract. Start the regular server first:

```powershell
Set-Location Examples\MinimalMultiplayer\Build\windows
.\Binaries\Server-Windows-win64\FOMM_ServerHeadless.exe -ApplyConfig ..\..\FOnlineMinimalMultiplayer.fomain
```

Start the profiled client:

```powershell
Set-Location Examples\MinimalMultiplayer\Build\windows
.\Binaries\Client-Windows-win64-Profiling_OnDemand\FOMM_Client.exe -ApplyConfig ..\..\FOnlineMinimalMultiplayer.fomain
```

From the example project root, reach a stable workload and capture a fixed
window:

```powershell
New-Item -ItemType Directory -Force -Path Workspace\Profiling | Out-Null
tracy-capture -o Workspace\Profiling\client.tracy -f -s 30 -m 80 -p 8086
```

For Tracy 0.13.1, `-f` permits replacing the named output, `-s 30` captures for
30 seconds, `-m 80` caps capture memory at 80 percent of physical memory, and
`-p 8086` selects the default Tracy port. Keep capture stdout with the result;
it records the frame count, time span, zone count, and trace size.

The Linux binary layout follows the same rule:
`Binaries/Server-Linux-x64/` for the regular server and
`Binaries/Client-Linux-x64-Profiling_OnDemand/` for the profiled client.

## Capture a server

Build `FOMM_ServerHeadless` as `Profiling_OnDemand` and `FOMM_Client` as
`RelWithDebInfo`. Start the profiled server, connect the regular client, wait
until the selected workload is stable, and run the same `tracy-capture`
command.

The profiled Windows server is emitted below
`Binaries/Server-Windows-win64-Profiling_OnDemand/`; the regular client remains
below `Binaries/Client-Windows-win64/`.

For a startup capture, build the target as `Profiling_Total`, start
`tracy-capture` before the process, and retain the complete startup log. Do not
compare a total-mode startup trace with an on-demand steady-state trace.

## Design a reproducible workload

A performance result needs a workload contract, not only a `.tracy` file.
Record:

- exact Engine and game revisions, dirty-worktree state, target, and profile;
- compiler, host CPU, operating system, renderer, driver, resolution, and
  frame cap;
- map/scene or server workload ID, content revision, seed, actor/client count,
  and account/database setup;
- warm-up condition, capture duration, frame or server-job count, and all
  input automation;
- other processes, host CPU load, thermal/power mode, and any discarded
  attempt;
- raw capture, capture stdout, runtime logs, exported tables, and the
  interpretation.

Prefer a project-owned scripted scene or workload driver. It should start from
a known state, suppress unrelated human input, announce readiness in a log,
run the same actions for every attempt, and stop cleanly. Keep visual debug
overlays, admin panels, verbose diagnostics, and unrelated telemetry out of
the profile unless they are the subject.

Run one capture at a time. Check that the Tracy port is free and that no stale
instrumented process can accept the connection. Wait for an idle host, reject
captures with unexpected frame/job counts, and retain at least three
comparable attempts when making an optimization claim. Report the selected
statistic and selection rule instead of keeping only the fastest trace.

## Analyze a capture

Start with the timeline and call tree:

1. Confirm program name, process, build configuration, capture duration, and
   workload marker.
2. Confirm frame or server-job counts are comparable to the baseline.
3. Find long frames, scheduling gaps, and dominant threads before sorting
   functions.
4. Compare inclusive time with self time. A large parent zone can merely own
   expensive children.
5. Inspect AngelScript zones in the same timeline as their native callers.
6. Correlate log messages, FPS/job plots, allocations, and source locations
   with the time window.
7. Change one variable, repeat the same workload, and preserve both raw
   captures.

The matching CSV exporter can produce a source-linked self-time table:

```bash
tracy-csvexport -e --truncated_mean=95 Workspace/Profiling/client.tracy > Workspace/Profiling/hotspots-self.csv
```

`-e` selects self time. The 95-percent truncated mean reduces the influence of
the slowest tail while retaining a separate percentile column; it does not
replace inspection of long frames or tail latency. Use the same export options
for baseline and candidate captures.

Client captures expose the `Client FPS` plot. Server captures expose `Server
jobs per second`; the current event-driven server no longer publishes the
removed loop-time/loops-per-second metrics. See
[Server Runtime](../../explanation/runtime/server.md#initialization-and-server-jobs) for the current
server statistics boundary.

## Add focused instrumentation

Use existing zones before adding new ones. Most native Engine functions already
call `FO_STACK_TRACE_ENTRY()`, and AngelScript calls are emitted
automatically. When a broad function needs a stable semantic label, use
`FO_STACK_TRACE_ENTRY_NAMED` at the owning native scope. Guard direct
`TracyPlot`, allocation, or message macros with `#if FO_TRACY`.

Instrumentation must:

- compile out in non-profiling configurations;
- use stable, low-cardinality names;
- avoid secrets, account data, chat text, or unbounded content IDs;
- avoid changing ownership, locking, allocation, or scheduling semantics;
- include a focused validation route and an update to this guide when it
  changes the public interpretation of a capture.

Do not add a zone solely to make a report look more detailed. Each zone should
answer a named performance question and have an owner who can interpret it.

## Common failure modes

| Symptom | Check |
|---|---|
| Capture tool cannot connect | Confirm the process was built in one of the four profiling configurations, the port is correct, and a firewall or stale process is not owning it. |
| Protocol mismatch | Use tools from the exact version in `TracyVersion.hpp`. |
| Trace starts too late | Use `Profiling_Total` only when startup is the intended workload; otherwise add an explicit readiness marker and warm-up. |
| Near-empty trace | Confirm the capture attached to the intended process and that the workload continued for the full window. |
| Client and server zones are mixed or ambiguous | Profile one standalone process and rebuild the counterpart as `RelWithDebInfo`. |
| Profile is dominated by baking or cache creation | Pre-bake and warm up, or rename the workload as a startup/baking measurement. |
| Debug build looks much slower | Use `Profiling_OnDemand` or `Profiling_Total` for performance comparisons; debug profiles are diagnostic. |
| Headless client result is used as renderer evidence | A null/headless renderer does not validate visible rendering performance. |
| Software rendering dominates Linux capture | Record the renderer and driver; do not compare software and hardware renderer captures. |
| Allocation totals appear incomplete | Check for third-party/plain C allocations outside the Engine allocator boundary. |

## Project-owned automation

An embedding project should automate the repeatable parts without changing the
Engine contract. A production runner should:

- choose a declared workload ID and exact client/server measurement side;
- build only the measured process with Tracy;
- pre-bake and stage binaries/resources;
- serialize access to runtime logs and the Tracy port;
- wait for process and workload readiness;
- enforce quiet-host and minimum-work checks;
- run `tracy-capture`, export a stable table, and copy all logs;
- retain provenance and reject noisy or incomplete attempts.

Scene IDs, config names, executable names, expected FPS/job thresholds,
database fixtures, renderer policy, and report destinations remain
project-owned. They must not be copied into this Engine guide as defaults.

## Validation workflow

After changing profiling configurations, instrumentation, or this guide, run:

```bash
python BuildTools/tests/test_docs_profiling.py
python BuildTools/docs_snippets.py --check --external
python BuildTools/docs_validate.py
```

Then build one on-demand target and verify a short capture contains:

- the expected program name and process;
- native and AngelScript zones;
- frame marks;
- the applicable client/server plot;
- log messages and named threads;
- a nonzero frame/job count and a readable saved trace.

Memory changes also require a focused allocation capture. Script-context
changes require a capture that executes, suspends, resumes, and completes an
AngelScript call chain. Frontend changes require both visible and headless
frame-mark paths when both remain supported.

## Maintenance

Update this page in the same change when:

- profiling configuration names or their base configurations change;
- `FO_TRACY`, `TRACY_ENABLE`, or `TRACY_ON_DEMAND` wiring changes;
- the vendored Tracy version or pruned payload changes;
- frame marks, program/thread naming, logs, plots, allocation tracking, or
  AngelScript zones change;
- a first-party GPU/lock instrumentation boundary is added;
- the engine-owned sample target/config/output layout changes.

When updating Tracy, verify the client and matching upstream capture/export
tools together. Re-run one client and one server capture before changing the
documented version or protocol claim.

## See also

- [Testing](../../contributing/testing/) for test-boundary selection, sanitizers, and coverage.
- [Native and AngelScript Debugging](../../troubleshooting/debugging.md) for native and AngelScript debugger workflows.
- [Build Workflow](../build/) for embedding-project build ownership.
- [Frontend and Rendering](../../explanation/rendering/) for renderer/runtime
  boundaries.
- [Server Runtime](../../explanation/runtime/server.md) for current job-throughput diagnostics.
- [ThirdParty Maintenance](../../../ThirdPartyMaintenance.md) for vendored version and
  pruning updates.
