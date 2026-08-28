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
- `Source/Essentials/FatalError.h`
- `Source/Essentials/FatalError.cpp`
- `Source/Essentials/SmartPointers.h`
- `Source/Essentials/SmartPointers.cpp`
- `Source/Essentials/MemorySystem.h`
- `Source/Essentials/MemorySystem.cpp`
- `Source/Essentials/Containers.h`
- `Source/Essentials/Containers.cpp`
- `Source/Essentials/StringUtils.h`
- `Source/Essentials/StringUtils.cpp`
- `Source/Essentials/Platform.h`
- `Source/Essentials/Platform.cpp`
- `Source/Essentials/ExceptionHandling.h`
- `Source/Essentials/ExceptionHandling.cpp`
- `Source/Essentials/Threading.h`
- `Source/Essentials/Threading.cpp`
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
- `Source/Essentials/Logging.h`
- `Source/Essentials/Logging.cpp`
- `Source/Essentials/DiskFileSystem.h`
- `Source/Essentials/DiskFileSystem.cpp`
- `Source/Essentials/CommonHelpers.h`
- `Source/Essentials/CommonHelpers.cpp`
- `Source/Essentials/NetSockets.h`
- `Source/Essentials/NetSockets.cpp`
- `BuildTools/cmake/stages/EngineSources.cmake`
- `BuildTools/tests/test_essentials_layering.py`
- related tests under `Source/Tests/`

## Include and dependency model

`Source/Essentials/Essentials.h` is the umbrella include. Its exact include order is the dependency order for the foundation layer:

`BasicCore` → `GlobalData` → `StackTrace` → `BaseLogging` → `FatalError` → `SmartPointers` → `MemorySystem` → `Containers` → `StringUtils` → `Platform` → `ExceptionHandling` → `Threading` → `SafeArithmetics` → `DataSerialization` → `HashedString` → `StrongType` → `TimeRelated` → `ExtendedTypes` → `Compressor` → `WorkThread` → `Logging` → `DiskFileSystem` → `CommonHelpers` → `NetSockets`.

The order is both a compile-time and link-time rule. A module may include and call only modules to its left; declaring an API in an early header and defining it in a later `.cpp` is still a reverse dependency. Direct includes and namespace-level `extern` definition ownership are checked by `BuildTools/tests/test_essentials_layering.py`. Keep new essentials APIs free of dependencies on `Source/Common/`, `Source/Client/`, `Source/Server/`, `Source/Tools/`, or embedding-project headers.

`Threading` deliberately follows `ExceptionHandling`, its deepest dependency, while remaining early enough for value headers such as `HashedString` to guard their state with the shared synchronization primitives. See [ThreadSafetyAnalysis.md](ThreadSafetyAnalysis.md).

## Subsystem map

### Platform and compiler gate

`BasicCore.h` enforces the selected OS macro (`FO_WINDOWS`, `FO_LINUX`, `FO_MAC`, `FO_ANDROID`, `FO_IOS`, or `FO_WEB`) and requires C++20. It also binds frequently used standard types into the engine namespace and declares core macros such as `FO_EXPORT_FUNC`, `FO_KEEP_DATA_SYMBOL`, and namespace helpers. Warning-suppression helpers also live here: `FO_DISABLE_WARNINGS_PUSH/POP` silence all warnings (for wrapping third-party header includes), while the per-compiler `FO_GCC_IGNORE_WARNINGS_PUSH/POP`, `FO_CLANG_IGNORE_WARNINGS_PUSH/POP`, and `FO_MSVC_IGNORE_WARNINGS_PUSH/POP` silence one named diagnostic and are active only on their matching compiler (so a single-toolchain false positive can be suppressed at one site without other toolchains rejecting an unknown `-W` name or warning number). Prefer fixing warnings at their root; reach for the per-compiler helpers only for documented compiler false positives.

`Platform.h` / `.cpp` owns host-specific helpers that are deliberately small: informational logging, thread names, executable path lookup, per-user data directory lookup, process id formatting, fork support where available, process memory usage, CPU usage snapshots, and dynamic module loading. `Platform::GetUserDataBase()` is intentionally environment-only and shell/SDL-free: Windows uses `%LOCALAPPDATA%` (else `%APPDATA%`), macOS/iOS use `$HOME/Library/Application Support`, and Linux/Android/other use `$XDG_DATA_HOME` (else `$HOME/.local/share`). Higher layers append the application name and decide whether absence is fatal. `Platform::GetCpuUsageSnapshot()` returns cumulative per-core system counters plus the current process CPU time; callers compare two snapshots to compute percentages and keep any sampling/cache state outside the Platform layer. `Platform` stays above `ExceptionHandling` and uses the earlier `FO_BASIC_STRONG_ASSERT` for terminating host-API invariants rather than importing late exception macros. Platform-specific application/window/rendering behavior lives under `Source/Frontend/`, not here.

Windows builds retain the `_WIN32_WINNT=0x0601` compile baseline. One Windows build-platform registry owns the CMake architecture, toolset, and canonical packaging architecture for the regular, `-clang`, and `-win7` variants. The Win7 pair pins MSVC 14.44, while `FO_BINARY_OUTPUT_POSTFIX` remains independent of the platform. In the package DSL the corresponding `BINARY` entry can select its own postfix, for example `BINARY Client Windows win32-win7 Raw+Zip+Wix POSTFIX Win7`, without affecting sibling binaries in the package. Compatibility checks are kept outside application targets.

### Diagnostics and failure handling

`BaseLogging.*` and `Logging.*` provide the logging foundation. `WriteLogMessage()` collapses immediate duplicates by `LogType` and message text: repeated copies are skipped, then the next different log line first emits a summary such as `...and 25 more same messages`. `LogToFile()` opens the log file without an exclusive lock (the platform default: MSVC `std::ofstream` opens deny-none, POSIX has no mandatory open lock) so two engine modules in one process — e.g. a runtime host EXE and the runtime DLL it loads, each with its own copy of the engine global data — can both hold the same file open at once, and every write seeks to end of file first (`WriteSync`) so neither handle overwrites content the other appended; the `append` parameter still selects truncate (default) vs append for the initial open. `WriteLog`/`WriteBaseLog` degrade safely when their global data is not yet created (falling back to the base log, then to `std::cout`), and a runtime host can open the log early — after `CreateGlobalData()`, `LogToFile(GetExeLogFileName(), false)` (Frontend) — so its pre-`InitApp` diagnostics reach the file.

`FatalError.*` is the early, native-only fatal layer. It follows `StackTrace` and `BaseLogging`, suspends asynchronous writes, emits one synchronous message plus native trace, and then delegates only the mechanical process termination to `BasicCore::ExitApp(false)`. It owns `ReportFatalAndExit`, `ReportStrongAssertAndExit`, and `FO_BASIC_STRONG_ASSERT`; it deliberately does not construct exception objects or depend on the later `ExceptionHandling` module. `ExitApp(false)` itself remains status-only because its callers include both controlled command failures and fatal invariant failures.

`StackTrace.*` captures and formats native/script stack information, including a capped global cache for resolved native frames, while `ExceptionHandling.*` owns the later exception-object reporting helpers. For debugger-facing workflows, use [Debugging.md](Debugging.md).

### Memory, pointers, and lifetime utilities

`MemorySystem.*` owns backup-memory chunks, bad-allocation reporting, and `SafeAllocator`. `SmartPointers.*` contains pointer wrappers used to make ownership, nullability, and raw-reference intent explicit; see [SmartPointers.md](SmartPointers.md) for the native `ptr` / `nptr` vocabulary and migration rules. Use this layer for generic ownership utilities only; entity lifetime and holder semantics belong in [EntityModel.md](EntityModel.md).

#### Allocation vocabulary

Engine code allocates through one of two surfaces, and nothing else:

- **The `fo` container aliases** from `Containers.h` — `string`, `wstring`, `vector`, `map`, `unordered_map`, `set`, `list`, `deque`, `stringstream`, `small_vector` and friends. Each is the standard container instantiated on `SafeAllocator`. Use these, never the `std::` originals.
- **`SafeAlloc`** — `MakeUnique` / `MakeShared` / `MakeRefCounted` / `MakeRawArr` / `MakeUniqueArr` for typed objects, and the raw tier `MallocRaw` / `CallocRaw` / `ReallocRaw` / `FreeRaw` plus `MallocAlignedRaw` / `FreeAlignedRaw` for C-ABI boundaries.

The raw tier exists because third-party allocator hooks are C-shaped: they demand `realloc`, or an untyped byte block, or both, which a C++ allocator cannot express. It carries the same out-of-memory policy as `SafeAllocator` — report, drain the backup pool, retry, then exit deterministically — so wiring a library through it does not silently opt that library out of the contract. A zero-size request is passed through rather than treated as failure.

The underlying `rpmalloc` primitives are deliberately **not** exported from `MemorySystem.h`. They return null on failure and would be a second, equally reachable entry point that skips the contract; they live as file-local statics in `MemorySystem.cpp`. The `MemCopy` / `MemMove` / `MemFill` / `MemCompare` / `MemReadUnaligned` / `MemWriteUnaligned` block operations are unrelated to allocation and remain public.

The vendored rpmalloc keeps its upstream 256 MiB spans on 64-bit targets. On 32-bit targets it uses one `LARGE_PAGE_SIZE` (16 MiB) per span: pre-`VirtualAlloc2` Windows has to reserve `size + alignment`, so an upstream 256 MiB aligned span can require a contiguous 512 MiB reservation inside the process's 2 GiB address space and make even the first tiny allocation fail. The smaller x86 span preserves every built-in page class while avoiding that startup dependency on a single huge address-space hole.

Three distinct things are at stake when code bypasses this vocabulary, and they are not equally severe:

| | What actually happens |
|---|---|
| **Separate heap** | Global `operator new`/`delete` are replaced with rpmalloc, so every `new` and every `std::allocator` already lands in the engine heap. But rpmalloc is built with `ENABLE_OVERRIDE=0`, so C `malloc`/`free` is **not** intercepted — anything allocating through it lives in the CRT heap, outside rpmalloc, invisible to `AllocatorGetInUseBytes()` and to Tracy allocation tracking. |
| **Wrong out-of-memory policy** | `std::allocator` throws `std::bad_alloc` instead of following the terminate-on-OOM model in [ExceptionSafety.md](ExceptionSafety.md) §1. |
| **Alignment** | `SafeAllocator` routes over-aligned element types through the aligned `operator new`/`delete` overloads. Note that the over-alignment test must stay a member *function*: `alignof(T)` needs a complete `T`, while the allocator has to remain usable with an incomplete one, since `std::vector<T>` may be declared before `T` is defined. |

Known and accepted limits: `std::function`, `std::future`/`std::promise`/`std::packaged_task`, `std::thread`, `std::filesystem::path` and the file streams have no allocator parameter at all, so they reach the engine heap through global `new` but throw on exhaustion. Separately, `BasicCore`, `StackTrace` and `BaseLogging` sit above `MemorySystem` in the `Essentials.h` include order and therefore use `std::` containers by design — `MemorySystem.cpp` calls `GetStackTrace()` from `ReportBadAlloc`, so the reporting path must not depend on the allocator that just failed.

#### Third-party allocators

| Library | Routed to | Where |
|---|---|---|
| ImGui | `SafeAllocator` | `Common/ImGuiExt/ImGuiStuff.cpp` |
| AngelScript | `SafeAllocator` | `Scripting/AngelScript/AngelScriptScripting.cpp` |
| zlib | `SafeAllocator` | `Essentials/Compressor.cpp` |
| ozz-animation | `SafeAlloc` aligned tier | `Common/ModelAnimationData.cpp` |
| meshoptimizer | `SafeAllocator` | `Tools/ModelMeshBaker.cpp` |
| ufbx | `SafeAllocator` | compile-time `UFBX_EXTERNAL_MALLOC` plus `extern "C" ufbx_malloc/realloc/free` in `Tools/ModelMeshBaker.cpp` |
| SDL | `SafeAlloc::*Raw` | `Frontend/Application.cpp` |
| Effekseer | `SafeAlloc::*Raw` + aligned | `Client/EffekseerExtension.cpp`, declared in its header; both owners (client runtime and `Tools/ParticleBaker.cpp`) install through that one definition |
| libpng | `SafeAlloc::*Raw` | `Tools/ImageBaker.cpp`, via `png_create_read_struct_2` |
| libbson / mongo-c | `SafeAlloc::*Raw` + aligned | shared `Server/DataBase.cpp`; every BSON-backed factory (JSON, SQLite, Mongo) installs the process-global vtable before constructing its backend |
| SQLite | `SafeAlloc::*Raw` | `Server/DataBase-SQLite.cpp`, via `sqlite3_config(SQLITE_CONFIG_MALLOC)` before `sqlite3_initialize()` |

The bson vtable is worth reading before copying its shape elsewhere: it supplies `aligned_alloc` but releases those blocks through the plain `free` member, never recording the alignment. That is sound only while both paths end in the same release function. Under rpmalloc they do (`rpaligned_alloc` and `rpmalloc` both end in `rpfree`), and so do they on POSIX without it (`posix_memalign` blocks are `free()`-able by definition). The one combination that breaks is Windows without rpmalloc — the sanitizer configs, where `expr_RpmallocEnabled` turns the allocator off so the sanitizer can interpose — because there the aligned path is `_aligned_malloc`/`_aligned_free`. `BsonAlignedAlloc` therefore falls back to plain `SafeAlloc::MallocRaw` in exactly that case, which is what bson's own default vtable does on MSVC and for the same stated reason (`_aligned_alloc_impl` in libbson `memory.c` deliberately does not use `_aligned_malloc`); every aligned request in mongoc is a `BSON_ALIGNOF` of an ordinary C struct, so malloc's fundamental alignment covers them. The vtable is process-global, so every BSON-backed factory installs the same callbacks before its backend can allocate; changing it later could pair an old allocation with a new free callback. Dropping `aligned_alloc` from the vtable is not an alternative — bson then substitutes an internal fallback that discards the requested alignment on every platform, not just the one that needs it.

SQLite's hook needs an `xSize` callback and hands the free/realloc/size functions only a pointer, so each block carries an 8-byte size header. Its configuration must also be installed *before* `sqlite3_initialize`, which is why the library is built with `SQLITE_OMIT_AUTOINIT` and every caller goes through one exported initializer.

Not hooked, with reasons: **LibreSSL** exports `CRYPTO_set_mem_functions` but its body is an inert `return 0;` — custom allocators were removed upstream, so calling it would be dead code that reads like coverage. **ogg / vorbis / theora** expose no allocator hook.

When vendoring or updating a library, check whether it has an allocator hook and either wire it or record why not — and read the hook's *implementation*, not just its declaration. Two of the entries above were initially misjudged from the call site or the symbol name alone.

### Serialization, values, strings, and hashes

`DataSerialization.*` contains binary read/write helpers used by network, persistence, resources, and tests. `DataReader::Read<T>()` and `DataWriter::Write<T>()` copy standard-layout values through byte copies so serialized streams do not depend on buffer alignment. The zero-copy `ReadPtr<T>(size)` overload is only for raw byte/string views (`uint8_t`, `char`, or `void`); typed values that need alignment must use `Read<T>()` or `ReadPtr(destination, size)`. `StringUtils.*`, `HashedString.*`, `StrongType.*`, `ExtendedTypes.*`, `SafeArithmetics.*`, and `TimeRelated.*` provide the small reusable values that higher layers treat as primitives. `iround` rejects non-finite and out-of-int64-range floating-point input before rounding so no value undefined for `std::llround` can reach it. `HashStorage::SetResolveHashFailureHandler` lets higher layers observe failed hash resolution in both throwing and flagged no-throw lookup paths without teaching essentials about a specific recovery policy.

### Filesystem, compression, sockets, and work threads

`DiskFileSystem.*` is the low-level disk abstraction. `fs_make_writable_path(user_writable_path, relative)` is the small path-policy helper used by higher layers for installed-client writable overlays: empty root or absolute input returns the input unchanged, while a relative path is layered under the writable root. The higher-level mounted resource view is `Source/Common/FileSystem.*` and is documented in [ConfigurationAndDataSources.md](ConfigurationAndDataSources.md). `Compressor.*` owns generic compression round-trips, `NetSockets.*` owns raw socket helpers below the higher-level network command/connection model in [Networking.md](Networking.md), and `WorkThread.*` owns simple background-worker infrastructure.

When a `WorkThread` job throws, the thread runs its local exception handler first so it can update worker-owned policy such as clearing queued jobs; the original exception is then reported through the global non-fatal exception reporter outside the worker lock.

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

- Compiler/OS gates, namespace, base aliases, and raw process termination: `Source/Essentials/BasicCore.*`.
- Global create/delete callback registration: `Source/Essentials/GlobalData.*`.
- Stack traces, early fatal reporting, logging, and exception reporting: `Source/Essentials/StackTrace.*`, `BaseLogging.*`, `FatalError.*`, `Logging.*`, `ExceptionHandling.*`, and [Debugging.md](Debugging.md).
- Generic memory/pointer utilities: `Source/Essentials/MemorySystem.*`, `SmartPointers.*`, and [SmartPointers.md](SmartPointers.md).
- File bytes and low-level writable-path composition on disk: `Source/Essentials/DiskFileSystem.*`; mounted engine resources and installed-client overlays: [ConfigurationAndDataSources.md](ConfigurationAndDataSources.md).
- Socket primitives: `Source/Essentials/NetSockets.*`; protocol/command/network runtime: [Networking.md](Networking.md).

## Validation checklist

1. Confirm the change does not introduce either an include-time or link-time dependency from an Essentials module to a later or higher engine layer; run `python -m pytest -q BuildTools/tests/test_essentials_layering.py`.
2. Update `BuildTools/cmake/stages/EngineSources.cmake` when adding/removing essentials files.
3. Run the smallest matching essentials test and then the broader `RunUnitTests` target when behavior crosses utility boundaries.
4. For diagnostics changes, also verify [Debugging.md](Debugging.md) stays accurate.
5. For filesystem/socket/threading changes, validate at least one higher-level consumer if the low-level contract changed.
