---
layout: default
title: Essentials
locale: en
document_id: native-essentials
permalink: /Docs/en/reference/native/essentials.html
---

# Essentials

> Engine-owned documentation. This page maps the low-level `Source/Essentials/` layer: platform/compiler prerequisites, process-wide lifecycle helpers, logging, memory, strings, serialization, filesystem, sockets, and utility types used by every higher engine layer.

## Purpose

Use this page when changing code that sits below `Source/Common/` or when you need to know whether a utility belongs in the reusable engine foundation instead of client, server, tools, or game-specific code.

For the memory model's exception contract — `SafeAlloc` / `SafeAllocator` terminate on OOM (so `std::bad_alloc` is not a recoverable error) and the `throw` / `FO_VERIFY_*` / `FO_STRONG_ASSERT` error tiers built on `ExceptionHandling.h` — see [ExceptionSafety.md](../../contributing/coding-contracts/exception-safety.md).

The essentials layer should stay dependency-light. It is included by most of the engine through `Source/Essentials/Essentials.h`, so changes here can affect every application target.

## Cross-layer decision

Essentials follows a strict dependency DAG: a dependency must appear earlier in
the umbrella block, and a reverse dependency must be moved upward through
parameters or a higher owning layer. Register new implementation files in
`FO_ESSENTIALS_SOURCE`; `EssentialsLib` is the owning target, and consumers must
link at the correct dependency point rather than bypassing the layer.

The exact umbrella order is `BasicCore`, `GlobalData`, `StackTrace`,
`BaseLogging`, `FatalError`, `FunctionObjects`, `SmartPointers`, `MemorySystem`,
`StringObject`, `Containers`, `StringUtils`,
`Platform`, `ExceptionHandling`, `Threading`, `SafeArithmetics`,
`DataSerialization`, `HashedString`, `StrongType`, `TimeRelated`,
`ExtendedTypes`, `Compressor`, `WorkThread`, `Logging`, `DiskFileSystem`,
`CommonHelpers`, and `NetSockets`.
Do not reorder that list to repair a cycle. Move reverse dependency pressure
upward through a parameter or split the responsibility into the higher owning
layer. Every new Essentials `.h` / `.cpp` must enter the checked
`FO_ESSENTIALS_SOURCE` list, from which `CoreLibs.cmake` builds `EssentialsLib`;
a source-path inventory alone is not build wiring.
Only headers participate in the `Essentials.h` umbrella order; never add a
`.cpp` file to that umbrella. Register both headers and implementation files in
`FO_ESSENTIALS_SOURCE`, then verify that `EssentialsLib` owns them and that its
consumers link at the correct dependency point.

When the same change affects script-visible metadata, keep ownership separate.
The Engine owns the reusable metadata/codegen machinery; the embedding project
supplies project configuration, extra metadata sources, common headers, and
script/content inputs; generated files are build artifacts. Diff all eighteen
canonical generated models and require the reviewed exact domain-bound
disposition for every gated compatibility break; an Essentials unit test does
not bypass the contract-change gate.

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
- `Source/Essentials/FunctionObjects.h`
- `Source/Essentials/FunctionObjects.cpp`
- `Source/Essentials/SmartPointers.h`
- `Source/Essentials/SmartPointers.cpp`
- `Source/Essentials/MemorySystem.h`
- `Source/Essentials/MemorySystem.cpp`
- `Source/Essentials/StringObject.h`
- `Source/Essentials/StringObject.cpp`
- `Source/Essentials/Containers.h`
- `Source/Essentials/Containers.cpp`
- `ThirdParty/small_vector/README.md`
- `ThirdParty/small_vector/source/include/gch/small_vector.hpp`
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
- `Source/Essentials/UcsTables.inc`
- `Source/Essentials/WinApiUndef.inc`
- `BuildTools/natvis/essentials.natvis`
- `BuildTools/cmake/stages/EngineSources.cmake`
- `BuildTools/tests/test_essentials_layering.py`
- related tests under `Source/Tests/`

## Include and dependency model

`Source/Essentials/Essentials.h` is the umbrella include. Its exact include order is the dependency order for the foundation layer:

`BasicCore` → `GlobalData` → `StackTrace` → `BaseLogging` → `FatalError` → `FunctionObjects` → `SmartPointers` → `MemorySystem` → `StringObject` → `Containers` → `StringUtils` → `Platform` → `ExceptionHandling` → `Threading` → `SafeArithmetics` → `DataSerialization` → `HashedString` → `StrongType` → `TimeRelated` → `ExtendedTypes` → `Compressor` → `WorkThread` → `Logging` → `DiskFileSystem` → `CommonHelpers` → `NetSockets`.

This list is intentionally exact rather than thematic. `Essentials.h` defines a strict dependency DAG: every Essentials header and its `.cpp` may include and call only modules that appear earlier in the umbrella block. Declaring an API early but defining it in a later `.cpp` is still a reverse link dependency. `BuildTools/tests/test_essentials_layering.py` checks direct includes and namespace-level external-definition ownership. Do not reorder the list to hide a cycle; move data through parameters or split the responsibility at the correct layer.

Keep new essentials APIs free of dependencies on `Source/Common/`, `Source/Client/`, `Source/Server/`, `Source/Tools/`, or embedding-project headers.

## Subsystem map

### Platform and compiler gate

`BasicCore.h` enforces the selected OS macro (`FO_WINDOWS`, `FO_LINUX`, `FO_MAC`, `FO_ANDROID`, `FO_IOS`, or `FO_WEB`) and requires C++20. It also binds frequently used standard types into the engine namespace and declares core macros such as `FO_EXPORT_FUNC`, `FO_KEEP_DATA_SYMBOL`, and namespace helpers. Warning-suppression helpers also live here: `FO_DISABLE_WARNINGS_PUSH/POP` silence all warnings (for wrapping third-party header includes), while the per-compiler `FO_GCC_IGNORE_WARNINGS_PUSH/POP`, `FO_CLANG_IGNORE_WARNINGS_PUSH/POP`, and `FO_MSVC_IGNORE_WARNINGS_PUSH/POP` silence one named diagnostic and are active only on their matching compiler (so a single-toolchain false positive can be suppressed at one site without other toolchains rejecting an unknown `-W` name or warning number). Prefer fixing warnings at their root; reach for the per-compiler helpers only for documented compiler false positives.

`Platform.h` / `.cpp` owns host-specific helpers that are deliberately small: informational logging, thread names, executable path lookup, per-user data directory lookup, process id formatting, fork support where available, process memory usage, CPU usage snapshots, and dynamic module loading. `Platform::GetUserDataBase()` is intentionally environment-only and shell/SDL-free: Windows uses `%LOCALAPPDATA%` (else `%APPDATA%`), macOS/iOS use `$HOME/Library/Application Support`, and Linux/Android/other use `$XDG_DATA_HOME` (else `$HOME/.local/share`). Higher layers append the application name and decide whether absence is fatal. `Platform::GetCpuUsageSnapshot()` returns cumulative per-core system counters plus the current process CPU time; callers compare two snapshots to compute percentages and keep any sampling/cache state outside the Platform layer. `Platform` stays above `ExceptionHandling` and uses the earlier `FO_BASIC_STRONG_ASSERT` for terminating host-API invariants rather than importing later exception macros. Platform-specific application/window/rendering behavior lives under `Source/Frontend/`, not here.

Windows builds retain the `_WIN32_WINNT=0x0601` compile baseline. One Windows build-platform registry owns the CMake architecture, toolset, and canonical packaging architecture for the regular, `-clang`, and `-win7` variants. The Win7 pair pins MSVC 14.44, while `FO_BINARY_OUTPUT_POSTFIX` remains independent of the platform. In the package DSL the corresponding `BINARY` entry can select its own postfix, for example `BINARY Client Windows win32-win7 Raw+Zip+Wix POSTFIX Win7`, without affecting sibling binaries in the package. Compatibility checks are kept outside application targets.

### Diagnostics and failure handling

`BaseLogging.*` and `Logging.*` provide the logging foundation. `WriteLogMessage()` collapses immediate duplicates by `LogType` and message text: repeated copies are skipped, then the next different log line first emits a summary such as `...and 25 more same messages`. `LogToFile()` opens the file without an exclusive lock, and every `WriteSync` seeks to the end, so separately linked Engine modules in one process can append safely. `WriteLog`/`WriteBaseLog` degrade to the base log and then `std::cout` before full global data exists.

`FatalError.*` is the early native-only fatal layer. It follows `StackTrace` and `BaseLogging`, suspends asynchronous writes, emits one synchronous message plus native trace, and delegates only mechanical termination to `BasicCore::ExitApp(false)`. It owns `ReportFatalAndExit`, `ReportStrongAssertAndExit`, and `FO_BASIC_STRONG_ASSERT` without constructing exception objects or depending on the later `ExceptionHandling` module. `ExitApp(false)` itself remains status-only because controlled command failures and fatal invariant failures both use it.

`StackTrace.*` captures and formats native/script stacks, while `ExceptionHandling.*` owns the later exception-object reporting helpers. For debugger-facing workflows, use [Native and AngelScript Debugging](../../troubleshooting/debugging.md).

### Memory, pointers, and lifetime utilities

`MemorySystem.*` owns backup-memory chunks, bad-allocation reporting, and `SafeAllocator`. `SmartPointers.*` contains pointer wrappers used to make ownership, nullability, and raw-reference intent explicit; see [SmartPointers.md](../../contributing/coding-contracts/smart-pointers.md) for the native `ptr` / `nptr` vocabulary and migration rules. Use this layer for generic ownership utilities only; entity lifetime and holder semantics belong in [Entity Model](../../explanation/entity-and-property-model/).

#### Callable vocabulary

`FunctionObjects.*` replaces `std::function` with two engine-owned wrappers.
`function<Signature>` is an alias of move-only `move_only_function<Signature>`
and is the default. Use `copyable_function<Signature>` only when copying the
stored target is the real ownership contract, such as a callback snapshot
distributed to more than one owner. Both keep a small nothrow-movable target
inline and allocate larger targets through a fail-fast path, so construction
does not introduce a recoverable `std::bad_alloc`. If migration exposes a copy,
first decide whether the owner should move instead. The `StackTrace.h`
script-provider hook is the sole retained `std::function`, because it precedes
`FunctionObjects` in the dependency order.

#### Allocation vocabulary

Engine code allocates through one of two surfaces, and nothing else:

- **The `fo` container aliases** from `Containers.h` — `string`, `wstring`, `vector`, `map`, `unordered_map`, `set`, `list`, `deque`, `stringstream`, `small_vector` and friends. `string` and `wstring` use the engine `basic_string` from `StringObject.h`; the remaining allocator-aware aliases use `SafeAllocator`. Use these, never the `std::` originals.
- **`SafeAlloc`** — `MakeUnique` / `MakeShared` / `MakeRefCounted` / `MakeRawArr` / `MakeUniqueArr` for typed objects, and the raw tier `MallocRaw` / `CallocRaw` / `ReallocRaw` / `FreeRaw` plus `MallocAlignedRaw` / `FreeAlignedRaw` for C-ABI boundaries.

The raw tier exists because third-party allocator hooks are C-shaped: they demand `realloc`, or an untyped byte block, or both, which a C++ allocator cannot express. It carries the same out-of-memory policy as `SafeAllocator` — report, drain the backup pool, retry, then exit deterministically — so wiring a library through it does not silently opt that library out of the contract. A zero-size request is passed through rather than treated as failure.

The underlying `rpmalloc` primitives are deliberately **not** exported from `MemorySystem.h`. They return null on failure and would be a second, equally reachable entry point that skips the contract; they live as file-local statics in `MemorySystem.cpp`. The `MemCopy` / `MemMove` / `MemFill` / `MemCompare` / `MemReadUnaligned` / `MemWriteUnaligned` block operations are unrelated to allocation and remain public.

The vendored rpmalloc keeps its upstream 256 MiB spans on 64-bit targets. On
32-bit targets one span is reduced to `LARGE_PAGE_SIZE` (16 MiB). Older Windows
allocation APIs reserve `size + alignment`; an aligned 256 MiB span can
therefore require a contiguous 512 MiB hole inside a 2 GiB x86 address space and
make the first small allocation fail. The 16 MiB x86 span preserves all built-in
page classes while removing that startup dependency on one enormous contiguous
reservation.

Three distinct things are at stake when code bypasses this vocabulary, and they are not equally severe:

| | What actually happens |
|---|---|
| **Separate heap** | Global `operator new`/`delete` are replaced with rpmalloc, so every `new` and every `std::allocator` already lands in the engine heap. But rpmalloc is built with `ENABLE_OVERRIDE=0`, so C `malloc`/`free` is **not** intercepted — anything allocating through it lives in the CRT heap, outside rpmalloc, invisible to `AllocatorGetInUseBytes()` and to Tracy allocation tracking. |
| **Wrong out-of-memory policy** | `std::allocator` throws `std::bad_alloc` instead of following the terminate-on-OOM model in [ExceptionSafety.md](../../contributing/coding-contracts/exception-safety.md) §1. |
| **Alignment** | `SafeAllocator` routes over-aligned element types through the aligned `operator new`/`delete` overloads. Note that the over-alignment test must stay a member *function*: `alignof(T)` needs a complete `T`, while the allocator has to remain usable with an incomplete one, since `std::vector<T>` may be declared before `T` is defined. |

Known and accepted limits: `std::future`/`std::promise`/`std::packaged_task`, `std::thread`, `std::filesystem::path` and the file streams have no allocator parameter at all, so they reach the engine heap through global `new` but throw on exhaustion. The sole `std::function` in `StackTrace.h` is also above the engine callable module. Separately, `BasicCore`, `StackTrace` and `BaseLogging` sit above `MemorySystem` in the `Essentials.h` include order and therefore use `std::` containers by design — `MemorySystem.cpp` calls `GetStackTrace()` from `ReportBadAlloc`, so the reporting path must not depend on the allocator that just failed.

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

#### Vector containers and inline storage

`Containers.h` exposes two sequence aliases backed by `SafeAllocator<T>`:

- `vector<T>` is the normal dynamically allocated sequence and remains the default for unbounded data, persistent collections that amortize their allocation, move-heavy pipelines, and exact engine interfaces.
- `small_vector<T, InlineCapacity>` stores up to `InlineCapacity` elements inside the object and spills to `SafeAllocator` storage above that limit. The engine alias requires an explicit capacity; `GCH_SMALL_VECTOR_DEFAULT_SIZE` configures the vendored implementation and is not a capacity-selection policy.

Use `small_vector` only when measurements or a hard protocol limit show that a frequently constructed collection is usually small. Choose the capacity from observed typical cardinality, keep rare large cases correct through heap spill, and account for the inline bytes in every instance. A per-call scratch list can be a strong candidate; adding several inline buffers to every cell in a dense map can cost more memory than the avoided first allocation. `inlined()` reports the current storage mode and is useful in focused tests and profiling instrumentation.

The representation has several correctness consequences:

1. Moving an inline `small_vector` relocates its elements into the destination object's inline buffer. Pointers, references, and iterators into the source do not follow the move as they commonly do when a heap-backed `vector` transfers its allocation. Audit every address that survives a container move.
2. Inline moves and swaps perform element work and are only conditionally `noexcept`; re-derive the touched function's exception-safety guarantee instead of inheriting assumptions from `vector`.
3. A `small_vector<T, N>` data member instantiates inline element destruction at the containing class boundary. `T` must therefore be complete there; it is not a drop-in replacement for a `vector` member whose element type is only forward-declared.
4. With the vendored implementation, a member whose element is a nested type with default member initializers can make default-inserting operations ill-formed while the enclosing class is incomplete, notably under Clang. For a shrink, prefer `erase(begin() + new_size, end())`; otherwise move the element type out of the enclosing class or make construction requirements explicit.
5. Heap spill still follows the engine's terminate-on-OOM policy because the alias uses `SafeAllocator`. Element construction, conversion, and move operations may still throw; see [ExceptionSafety.md](../../contributing/coding-contracts/exception-safety.md).

Do not substitute `small_vector` across an exact-type boundary merely because the operations look vector-like:

- script export/codegen signatures and `ScriptSystem` registration use exact `vector<T>` / `readonly_vector<T>` spellings and type identities;
- property writes, serialized backing stores, `DataReader` / `DataWriter`, `NetBuffer`, and `CScriptArray` have exact `vector` contracts at selected boundaries;
- a span-taking internal helper may accept either representation without exposing the concrete container type, which is the preferred seam when both are valid;
- `FO_ENTITY_PROPERTY` cannot take `small_vector<T, N>` directly because the comma also separates macro arguments.

`vector_collection` admits both engine aliases for generic readers. The producing helpers `vec_filter`, `vec_transform`, and `vec_sorted` preserve `vector` versus `small_vector` and retain the inline capacity through `rebind_vector_t`; non-vector ranges materialize as `vector`. `to_vector` intentionally always materializes a `vector`, while `copy_hold_ref` exposes an opaque ref-held snapshot rather than a concrete sequence contract. The generic formatter accepts both aliases for ordinary numeric elements, but its string and boolean special cases currently match exact `vector<string>` and `vector<bool>` types; do not assume equivalent formatting for `small_vector<string, N>` or `small_vector<bool, N>` without extending and testing that formatter.

For an adoption, record the measured distribution and object count, audit address lifetime plus move/swap sites, verify complete-type and exact-interface constraints, and add focused coverage for inline operation, spill, and any generic-helper result type. Then run the complete native unit suite, the embedding project's exception-safety and smart-pointer audits when present, and representative bake/gameplay/profiling paths for the changed subsystem.

### Serialization, values, strings, and hashes

`StringObject.*` owns the engine `basic_string` implementation. Its API follows
`std::basic_string`, while `FO_STRING_INLINE_CAPACITY` selects the compiled
small-string buffer for both `string` and `wstring`; this is a build-wide ABI
choice, not a per-call optimization. Three standard-library boundaries require
explicit adapters: copy text into a standard stream with `make_stream_string`,
construct `std::filesystem::path` through `fs_make_path`, and call `getline`
unqualified so argument-dependent lookup selects the engine overload. String
growth uses the same deterministic terminate-on-OOM policy as other engine
storage.

`DataSerialization.*` contains binary read/write helpers used by network, persistence, resources, and tests. `DataReader::Read<T>()` and `DataWriter::Write<T>()` copy standard-layout values through byte copies so serialized streams do not depend on buffer alignment. The zero-copy `ReadPtr<T>(size)` overload is only for raw byte/string views (`uint8_t`, `char`, or `void`); typed values that need alignment must use `Read<T>()` or `ReadPtr(destination, size)`. `StringUtils.*`, `HashedString.*`, `StrongType.*`, `ExtendedTypes.*`, `SafeArithmetics.*`, and `TimeRelated.*` provide the small reusable values that higher layers treat as primitives. `iround` rejects non-finite and out-of-int64-range floating-point input before rounding so no value undefined for `std::llround` can reach it. `HashStorage::SetResolveHashFailureHandler` lets higher layers observe failed hash resolution in both throwing and flagged no-throw lookup paths without teaching essentials about a specific recovery policy.

### Filesystem, compression, sockets, and work threads

`DiskFileSystem.*` is the low-level disk abstraction. `fs_make_writable_path(user_writable_path, relative)` is the small path-policy helper used by higher layers for installed-client writable overlays: empty root or absolute input returns the input unchanged, while a relative path is layered under the writable root. The higher-level mounted resource view is `Source/Common/FileSystem.*` and is documented in [Configuration and Data Sources](../settings/configuration-and-data-sources.md). `Compressor.*` owns generic compression round-trips, `NetSockets.*` owns raw socket helpers below the higher-level network command/connection model in [Networking](../../explanation/authority-and-networking/), and `WorkThread.*` owns simple background-worker infrastructure.

When a `WorkThread` job throws, the thread runs its local exception handler first so it can update worker-owned policy such as clearing queued jobs; the original exception is then reported through the global non-fatal exception reporter outside the worker lock.

## Build integration

`BuildTools/cmake/stages/EngineSources.cmake` lists every authored Essentials `.h` / `.cpp` pair plus the two `.inc` files and debugger visualization in `FO_ESSENTIALS_SOURCE`. `BuildTools/cmake/stages/CoreLibs.cmake` then creates `EssentialsLib` from that list. The library is part of the core dependency chain used by applications, tools, tests, and generated-code consumers. If a new Essentials file is added, place it at the correct dependency point in `Essentials.h`, wire it through `FO_ESSENTIALS_SOURCE`, and add focused coverage where possible.

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
- `Source/Tests/Test_FunctionObjects.cpp`
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
- `Source/Tests/Test_StringObject.cpp`
- `Source/Tests/Test_StringUtils.cpp`
- `Source/Tests/Test_StrongType.cpp`
- `Source/Tests/Test_TimeRelated.cpp`
- `Source/Tests/Test_WorkThread.cpp`

`Test_Containers.cpp` pins the engine alias, allocator, inline-to-heap transition, move, swap, and formatting behavior. `Test_CommonHelpers.cpp` pins container-kind preservation through `rebind_vector_t` and the producing `vec_*` helpers. Keep both focused suites current when changing vector aliases or generic sequence helpers.

See [Testing](../../contributing/testing/) for the complete test-suite map and target wiring.

## Change routing

- Compiler/OS gates, namespace, base aliases, and low-level macros: `Source/Essentials/BasicCore.*`.
- Global create/delete callback registration: `Source/Essentials/GlobalData.*`.
- Stack traces, logging, and exception reporting: `Source/Essentials/StackTrace.*`, `BaseLogging.*`, `Logging.*`, `ExceptionHandling.*`, and [Native and AngelScript Debugging](../../troubleshooting/debugging.md).
- Generic memory/pointer utilities: `Source/Essentials/MemorySystem.*`, `SmartPointers.*`, and [SmartPointers.md](../../contributing/coding-contracts/smart-pointers.md).
- Callable ownership and inline targets: `Source/Essentials/FunctionObjects.*`.
- Engine strings and the build-wide inline-capacity contract: `Source/Essentials/StringObject.*`; aliases and stream interop: `Containers.h`.
- File bytes and low-level writable-path composition on disk: `Source/Essentials/DiskFileSystem.*`; mounted engine resources and installed-client overlays: [Configuration and Data Sources](../settings/configuration-and-data-sources.md).
- Socket primitives: `Source/Essentials/NetSockets.*`; protocol/command/network runtime: [Networking](../../explanation/authority-and-networking/).

## Validation checklist

1. Confirm the change does not introduce a dependency from essentials back into higher engine layers.
2. Update `BuildTools/cmake/stages/EngineSources.cmake` when adding/removing essentials files.
3. Run the smallest matching essentials test and then the broader `RunUnitTests` target when behavior crosses utility boundaries.
4. For diagnostics changes, also verify [Native and AngelScript Debugging](../../troubleshooting/debugging.md) stays accurate.
5. For filesystem/socket/threading changes, validate at least one higher-level consumer if the low-level contract changed.
6. For a `small_vector` adoption, prove capacity and object-count assumptions with data, audit move/address lifetime and exact-type boundaries, and re-run exception-safety plus pointer-ownership gates.
