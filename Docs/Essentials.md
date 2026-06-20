# Essentials

> Engine-owned documentation. This page maps the low-level `Source/Essentials/` layer: platform/compiler prerequisites, process-wide lifecycle helpers, logging, memory, strings, serialization, filesystem, sockets, and utility types used by every higher engine layer.

## Purpose

Use this page when changing code that sits below `Source/Common/` or when you need to know whether a utility belongs in the reusable engine foundation instead of client, server, tools, or game-specific code.

For the memory model's exception contract — `SafeAlloc` / `SafeAllocator` terminate on OOM (so `std::bad_alloc` is not a recoverable error) and the `throw` / `FO_VERIFY_*` / `FO_STRONG_ASSERT` error tiers built on `ExceptionHandling.h` — see [ExceptionSafety.md](ExceptionSafety.md).

The essentials layer should stay dependency-light. It is included by most of the engine through `Source/Essentials/Essentials.h`, so changes here can affect every application target.

## Source paths inspected

- `Source/Essentials/Essentials.h`
- `Source/Essentials/Essentials.cpp`
- `Source/Essentials/BasicCore.h`
- `Source/Essentials/BasicCore.cpp`
- `Source/Essentials/GlobalData.h`
- `Source/Essentials/GlobalData.cpp`
- `Source/Essentials/StackTrace.h`
- `Source/Essentials/StackTrace.cpp`
- `Source/Essentials/BaseLogging.h`
- `Source/Essentials/BaseLogging.cpp`
- `Source/Essentials/Logging.h`
- `Source/Essentials/Logging.cpp`
- `Source/Essentials/ExceptionHandling.h`
- `Source/Essentials/ExceptionHandling.cpp`
- `Source/Essentials/MemorySystem.h`
- `Source/Essentials/MemorySystem.cpp`
- `Source/Essentials/SmartPointers.h`
- `Source/Essentials/SmartPointers.cpp`
- `Source/Essentials/Containers.h`
- `Source/Essentials/Containers.cpp`
- `Source/Essentials/StringUtils.h`
- `Source/Essentials/StringUtils.cpp`
- `Source/Essentials/SafeArithmetics.h`
- `Source/Essentials/SafeArithmetics.cpp`
- `Source/Essentials/DataSerialization.h`
- `Source/Essentials/DataSerialization.cpp`
- `Source/Essentials/HashedString.h`
- `Source/Essentials/HashedString.cpp`
- `Source/Essentials/StrongType.h`
- `Source/Essentials/StrongType.cpp`
- `Source/Essentials/TimeRelated.h`
- `Source/Essentials/TimeRelated.cpp`
- `Source/Essentials/ExtendedTypes.h`
- `Source/Essentials/ExtendedTypes.cpp`
- `Source/Essentials/Compressor.h`
- `Source/Essentials/Compressor.cpp`
- `Source/Essentials/WorkThread.h`
- `Source/Essentials/WorkThread.cpp`
- `Source/Essentials/DiskFileSystem.h`
- `Source/Essentials/DiskFileSystem.cpp`
- `Source/Essentials/CommonHelpers.h`
- `Source/Essentials/CommonHelpers.cpp`
- `Source/Essentials/NetSockets.h`
- `Source/Essentials/NetSockets.cpp`
- `Source/Essentials/Platform.h`
- `Source/Essentials/Platform.cpp`
- `BuildTools/cmake/stages/EngineSources.cmake`
- related tests under `Source/Tests/`

## Include and dependency model

`Source/Essentials/Essentials.h` is the umbrella include. Its include order is the dependency order for the foundation layer:

1. `BasicCore.h` — compiler/OS gates, standard library surface, namespace macros, base aliases, exception declaration helpers, and compile-time constants.
2. `GlobalData.h` — process-wide create/delete callback registration for engine global data.
3. `StackTrace.h`, `BaseLogging.h`, `ExceptionHandling.h`, `Logging.h` — diagnostic and failure-reporting foundation.
4. `Threading.h` — Clang Thread Safety Analysis macros (`FO_TSA_*`), the snake_case synchronization primitives (`fo::mutex` / `fo::shared_mutex` / `fo::scoped_lock` / `fo::shared_lock` / `fo::unique_lock`), the `fo::thread` pool-task handle, and the `run_thread` / `run_async` worker pools. Deliberately positioned right after `ExceptionHandling.h` (its deepest dependency) so even low-layer value headers such as `HashedString.h` can guard their state with the primitives. See [ThreadSafetyAnalysis.md](ThreadSafetyAnalysis.md).
5. `SmartPointers.h`, `MemorySystem.h` — pointer and allocation helpers.
6. `Containers.h`, `StringUtils.h`, `CommonHelpers.h` — reusable container/string/utility helpers.
7. `SafeArithmetics.h`, `DataSerialization.h`, `HashedString.h`, `StrongType.h`, `TimeRelated.h`, `ExtendedTypes.h`, `Compressor.h` — value, serialization, hashing, time, compression, and type helpers.
8. `WorkThread.h`, `DiskFileSystem.h`, `NetSockets.h`, `Platform.h` — the `WorkThread` job runner, disk access, socket, and host OS abstractions.

Keep new essentials APIs free of dependencies on `Source/Common/`, `Source/Client/`, `Source/Server/`, `Source/Tools/`, or embedding-project headers.

## Subsystem map

### Platform and compiler gate

`BasicCore.h` enforces the selected OS macro (`FO_WINDOWS`, `FO_LINUX`, `FO_MAC`, `FO_ANDROID`, `FO_IOS`, or `FO_WEB`) and requires C++20. It also binds frequently used standard types into the engine namespace and declares core macros such as `FO_EXPORT_FUNC`, `FO_KEEP_DATA_SYMBOL`, and namespace helpers.

`Platform.h` / `.cpp` owns host-specific helpers that are deliberately small: informational logging, thread names, executable path lookup, per-user data directory lookup, process id formatting, fork support where available, process memory usage, CPU usage snapshots, and dynamic module loading. `Platform::GetCpuUsageSnapshot()` returns cumulative per-core system counters plus the current process CPU time; callers compare two snapshots to compute percentages and keep any sampling/cache state outside the Platform layer. Platform-specific application/window/rendering behavior lives under `Source/Frontend/`, not here.

### Diagnostics and failure handling

`BaseLogging.*` and `Logging.*` provide the logging foundation. `WriteLogMessage()` collapses immediate duplicates by `LogType` and message text: repeated copies are skipped, then the next different log line first emits a summary such as `...and 25 more same messages`. `LogToFile()` opens the log file without an exclusive lock (the platform default: MSVC `std::ofstream` opens deny-none, POSIX has no mandatory open lock) so two engine modules in one process — e.g. a runtime host EXE and the runtime DLL it loads, each with its own copy of the engine global data — can both hold the same file open at once, and every write seeks to end of file first (`WriteSync`) so neither handle overwrites content the other appended; the `append` parameter still selects truncate (default) vs append for the initial open. `WriteLog`/`WriteBaseLog` degrade safely when their global data is not yet created (falling back to the base log, then to `std::cout`), and a runtime host can open the log early — after `CreateGlobalData()`, `LogToFile(GetExeLogFileName(), false)` (Frontend) — so its pre-`InitApp` diagnostics reach the file. When `AsyncLogWrite` is enabled, the fatal-error handler (`ExceptionHandling`) calls `SuspendAsyncLogWriting()` first, which flips writes back to the synchronous path without joining the worker, so the crash reason and stack trace are flushed inline before the process exits instead of being lost in an undrained async queue. `StackTrace.*` captures and formats native/script stack information, including a capped global cache for resolved native frames, while `ExceptionHandling.*` owns exception-reporting helpers. For debugger-facing workflows, use [Debugging.md](Debugging.md).

### Memory, pointers, and lifetime utilities

`MemorySystem.*` owns backup-memory chunks, bad-allocation reporting, and `SafeAllocator`. `SmartPointers.*` contains pointer wrappers used to make ownership and raw-reference intent explicit. Use this layer for generic ownership utilities only; entity lifetime and holder semantics belong in [EntityModel.md](EntityModel.md).

### Serialization, values, strings, and hashes

`DataSerialization.*` contains binary read/write helpers used by network, persistence, resources, and tests. `StringUtils.*`, `HashedString.*`, `StrongType.*`, `ExtendedTypes.*`, `SafeArithmetics.*`, and `TimeRelated.*` provide the small reusable values that higher layers treat as primitives. `HashStorage::SetResolveHashFailureHandler` lets higher layers observe failed hash resolution in both throwing and flagged no-throw lookup paths without teaching essentials about a specific recovery policy.

### Filesystem, compression, sockets, and work threads

`DiskFileSystem.*` is the low-level disk abstraction. The higher-level mounted resource view is `Source/Common/FileSystem.*` and is documented in [ConfigurationAndDataSources.md](ConfigurationAndDataSources.md). `Compressor.*` owns generic compression round-trips, `NetSockets.*` owns raw socket helpers below the higher-level network command/connection model in [Networking.md](Networking.md), and `WorkThread.*` owns simple background-worker infrastructure.

## Build integration

`BuildTools/cmake/stages/EngineSources.cmake` lists essentials files in `FO_ESSENTIALS_SOURCE`. The essentials target is part of the core libraries used by applications, tools, tests, and generated-code consumers. If a new essentials file is added, wire it through this stage and add a focused test where possible.

## Tests to inspect

The essentials layer has direct test coverage in:

- `Source/Tests/Test_BaseLogging.cpp`
- `Source/Tests/Test_BasicCore.cpp`
- `Source/Tests/Test_CommonHelpers.cpp`
- `Source/Tests/Test_Compressor.cpp`
- `Source/Tests/Test_Containers.cpp`
- `Source/Tests/Test_DataSerialization.cpp`
- `Source/Tests/Test_DiskFileSystem.cpp`
- `Source/Tests/Test_ExceptionHandling.cpp`
- `Source/Tests/Test_ExtendedTypes.cpp`
- `Source/Tests/Test_GenericUtils.cpp`
- `Source/Tests/Test_GlobalData.cpp`
- `Source/Tests/Test_HashedString.cpp`
- `Source/Tests/Test_Logging.cpp`
- `Source/Tests/Test_MemorySystem.cpp`
- `Source/Tests/Test_NetSockets.cpp`
- `Source/Tests/Test_Platform.cpp`
- `Source/Tests/Test_SafeArithmetics.cpp`
- `Source/Tests/Test_SmartPointers.cpp`
- `Source/Tests/Test_StackTrace.cpp`
- `Source/Tests/Test_StringUtils.cpp`
- `Source/Tests/Test_StrongType.cpp`
- `Source/Tests/Test_TimeRelated.cpp`
- `Source/Tests/Test_WorkThread.cpp`

See [Testing.md](Testing.md) for the complete test-suite map and target wiring.

## Change routing

- Compiler/OS gates, namespace, base aliases, and low-level macros: `Source/Essentials/BasicCore.*`.
- Global create/delete callback registration: `Source/Essentials/GlobalData.*`.
- Stack traces, logging, and exception reporting: `Source/Essentials/StackTrace.*`, `BaseLogging.*`, `Logging.*`, `ExceptionHandling.*`, and [Debugging.md](Debugging.md).
- Generic memory/pointer utilities: `Source/Essentials/MemorySystem.*` and `SmartPointers.*`.
- File bytes on disk: `Source/Essentials/DiskFileSystem.*`; mounted engine resources: [ConfigurationAndDataSources.md](ConfigurationAndDataSources.md).
- Socket primitives: `Source/Essentials/NetSockets.*`; protocol/command/network runtime: [Networking.md](Networking.md).

## Validation checklist

1. Confirm the change does not introduce a dependency from essentials back into higher engine layers.
2. Update `BuildTools/cmake/stages/EngineSources.cmake` when adding/removing essentials files.
3. Run the smallest matching essentials test and then the broader `RunUnitTests` target when behavior crosses utility boundaries.
4. For diagnostics changes, also verify [Debugging.md](Debugging.md) stays accurate.
5. For filesystem/socket/threading changes, validate at least one higher-level consumer if the low-level contract changed.
