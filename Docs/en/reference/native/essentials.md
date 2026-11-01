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

Use this page for code below `Source/Common/` or to decide whether a utility belongs in the reusable foundation rather than client, server, tools, or game-specific code.

For the exception contract — `safe_alloc` / `safe_allocator` terminate on OOM, making `std::bad_alloc` unrecoverable, and `throw` / `FO_VERIFY_*` / `FO_STRONG_ASSERT` define the `ExceptionHandling.h` error tiers — see [ExceptionSafety.md](../../contributing/coding-contracts/exception-safety.md).

Keep essentials dependency-light: most engine code includes `Source/Essentials/Essentials.h`, so changes can affect every application target.

## Cross-layer decision

Essentials has a strict dependency DAG: dependencies precede consumers in the umbrella; reverse dependencies move upward through parameters or a higher owner. Register implementations in `FO_ESSENTIALS_SOURCE`, owned by `EssentialsLib`; consumers must link at the correct dependency point without bypassing the layer.

The exact umbrella order is `BasicCore`, `GlobalData`, `StackTrace`, `BaseLogging`, `FatalError`, `FunctionObjects`, `SmartPointers`, `MemorySystem`, `StringObject`, `DequeObject`, `Containers`, `StringUtils`, `WinApi`, `Posix`, `Platform`, `ExceptionHandling`, `RandomGenerator`, `Threading`, `SafeArithmetics`, `DataSerialization`, `HashedString`, `StrongType`, `TimeRelated`, `ExtendedTypes`, `Compressor`, `Cryptography`, `WorkThread`, `Logging`, `DiskFileSystem`, `CommonHelpers`, and `NetSockets`.
Never reorder it to fix a cycle: pass reverse dependencies upward through parameters or split responsibility into the higher owner. Every new Essentials `.h` / `.cpp` must enter checked `FO_ESSENTIALS_SOURCE`, which `CoreLibs.cmake` builds into `EssentialsLib`; a path inventory alone is not build wiring. Only headers enter the `Essentials.h` umbrella, never a `.cpp`. Register headers and implementations in `FO_ESSENTIALS_SOURCE`, then verify `EssentialsLib` ownership and correct consumer links.

For changes affecting script-visible metadata, separate ownership: Engine owns reusable metadata/codegen machinery; the embedding project supplies configuration, extra metadata sources, common headers, and script/content inputs. Generated files are build artifacts. Diff all 17 canonical generated models and require the reviewed exact domain-bound disposition for every gated compatibility break; Essentials unit tests do not bypass this gate.

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
- `Source/Essentials/DequeObject.h`
- `Source/Essentials/DequeObject.cpp`
- `Source/Essentials/Containers.h`
- `Source/Essentials/Containers.cpp`
- `ThirdParty/small_vector/README.md`
- `ThirdParty/small_vector/source/include/gch/small_vector.hpp`
- `Source/Essentials/StringUtils.h`
- `Source/Essentials/StringUtils.cpp`
- `Source/Essentials/WinApi.h`
- `Source/Essentials/WinApi.cpp`
- `Source/Essentials/Posix.h`
- `Source/Essentials/Posix.cpp`
- `Source/Essentials/Platform.h`
- `Source/Essentials/Platform.cpp`
- `Source/Essentials/ExceptionHandling.h`
- `Source/Essentials/ExceptionHandling.cpp`
- `Source/Essentials/RandomGenerator.h`
- `Source/Essentials/RandomGenerator.cpp`
- `Source/Essentials/Cryptography.h`
- `Source/Essentials/Cryptography.cpp`
- `ThirdParty/Monocypher/src/monocypher.h`
- `ThirdParty/Monocypher/src/monocypher.c`
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

### Naming and module namespaces

Essentials types, functions, methods and fields use snake_case; higher layers and project-native code use PascalCase. Module filenames stay PascalCase for umbrella-order and layering checks. Template parameters (`CharT`, `Traits`, `Alloc`, `InlineCapacity`), exception types, foreign APIs and the AngelScript ref-count protocol (`AddRef`, `TryAddRef`, `Release`) retain their owners' conventions. Engine `refcounted` uses `addref`, `release` and `get_refcount`; `refcountable` / `refcount_ptr` support the appropriate spelling.

Replace static-only utility classes with namespaces unless class identity is required, as for `friend class safe_alloc`. Drop member `static`, which would give a namespace function internal linkage. Namespaces follow declaration order and cannot be friends; inline callers may need forward declarations.

Module-prefixed free surfaces belong wholly in their module namespace, types included: `logging::write_base`, `memory::copy`, `stack_trace::get`, `exceptions::report_and_continue`, `winapi::`, `posix::`, `platform::`, `utf8::`. Unprefixed vocabulary such as `numeric_cast`, `safe_call`, `copy_hold_ref`, `coarse_sleep`, `run_async`, `exit_app` and `vec_*` stays bare. Use `logging`: `log` collides with the global math function called unqualified by vendored headers.

### Dependency ordering

The umbrella `Source/Essentials/Essentials.h` defines the foundation's exact dependency order:

`BasicCore` → `GlobalData` → `StackTrace` → `BaseLogging` → `FatalError` → `FunctionObjects` → `SmartPointers` → `MemorySystem` → `StringObject` → `DequeObject` → `Containers` → `StringUtils` → `WinApi` → `Posix` → `Platform` → `ExceptionHandling` → `RandomGenerator` → `Threading` → `SafeArithmetics` → `DataSerialization` → `HashedString` → `StrongType` → `TimeRelated` → `ExtendedTypes` → `Compressor` → `Cryptography` → `WorkThread` → `Logging` → `DiskFileSystem` → `CommonHelpers` → `NetSockets`.

This order is exact, not thematic. `Essentials.h` imposes a strict DAG: headers and their `.cpp` implementations may include/call only earlier modules. An early API declaration defined in a later `.cpp` is still a reverse link dependency. `BuildTools/tests/test_essentials_layering.py` checks direct includes and namespace-level external-definition owners. Never reorder to hide a cycle; move data through parameters or split the responsibility at the correct layer.

New essentials APIs must not depend on `Source/Common/`, `Source/Client/`, `Source/Server/`, `Source/Tools/`, or project headers.

`global_data::destroy(observer, context)` sends each registered set name to the observer immediately before that set's delete callback. This identifies stuck teardown without making the observer depend on globals already destroyed. Hosts/runtime modules destroy only sets they created and join their own workers before returning to a continuing host. Process-lifetime crash records stay outside this sweep.

## Subsystem map

### Platform and compiler gate

`BasicCore.h` declares `FO_TRACE_COLOR_<Category>`, `FO_TRACE_ZONE(Category)` / `FO_TRACE_ZONE_NAMED(Category, name)`. Profiling builds select zones through generated `TraceCategories.gen.h`; other builds validate categories but emit no zones. See the [profiling guide](../../how-to/quality/profiling.md#placing-zones).

`BasicCore.h` enforces the selected OS (`FO_WINDOWS`, `FO_LINUX`, `FO_MAC`, `FO_ANDROID`, `FO_IOS`, or `FO_WEB`) and C++20, binds common standard types into the engine namespace, and declares `FO_EXPORT_FUNC`, `FO_KEEP_DATA_SYMBOL` and namespace helpers. `FO_DISABLE_WARNINGS_PUSH/POP` silences all warnings around third-party headers; `FO_GCC_IGNORE_WARNINGS_PUSH/POP`, `FO_CLANG_IGNORE_WARNINGS_PUSH/POP` and `FO_MSVC_IGNORE_WARNINGS_PUSH/POP` silence one named diagnostic only on their matching compiler, avoiding unknown `-W` names/numbers on other toolchains. Fix root causes first; use per-compiler helpers only for documented compiler false positives.

`Platform.h` / `.cpp` owns small host helpers: informational logging, thread names, executable paths, per-user data directories, process id formatting, supported fork operations, process memory usage, CPU snapshots and dynamic module loading. Shell/SDL-free `platform::get_user_data_base()` prefers environment paths: Windows `%LOCALAPPDATA%` then `%APPDATA%`; macOS/iOS `$HOME/Library/Application Support`; Linux/Android/other `$XDG_DATA_HOME` then `$HOME/.local/share`. Without those paths, Windows tries `SHGetKnownFolderPath`, supported POSIX hosts `getpwuid_r`; no source means no path. Higher layers append the application name and decide whether absence is fatal. `platform::get_cpu_usage_snapshot()` returns cumulative per-core system counters and current-process CPU time; callers compare two snapshots for percentages and own sampling/cache state. `Platform` precedes `ExceptionHandling` and uses earlier `FO_BASIC_STRONG_ASSERT` for terminating host-API invariants. Application/window/rendering platform behavior belongs in `Source/Frontend/`.

Immediately after failed module load/symbol lookup, `platform::get_last_module_error()` captures Windows loader text or POSIX `dlerror()`. Client diagnostics use `platform::get_os_version()`: `RtlGetVersion` on Windows, `uname` elsewhere.

`WinApi.*` and `Posix.*` own OS calls under `winapi::` and `posix::`. Public boundaries use engine strings, optionals, fixed-width integers and `nptr<void>`, hiding `HANDLE`, `pid_t` and other OS types. `Platform` dispatches to them; ordinary consumers extend/call wrappers rather than include `<Windows.h>` or POSIX headers. Exceptions are lower Essentials implementations unable to depend on these modules (`BasicCore.cpp`, `BaseLogging.cpp`, `StringUtils.cpp`), plus OS wrappers `NetSockets.*` and `ServerServiceApp.cpp`.

Darwin SDK signal-set operations may be function-like macros. In `Posix.cpp`, call `sigfillset`, `sigdelset`, `sigemptyset` and `sigaddset` without `::` so macros expand; Linux still resolves global functions. Crash-handler masks, flags, alternate stack and default-signal re-raise are unchanged. Ordinary consumers use `posix::` wrappers, not OS headers/calls.

Windows retains `_WIN32_WINNT=0x0601`. One build-platform registry owns CMake architecture, toolset and canonical packaging architecture for regular, `-clang` and `-win7` variants. Win7 pins MSVC 14.44; `FO_BINARY_OUTPUT_POSTFIX` remains independent. Each package-DSL `BINARY` may choose its own postfix, e.g. `BINARY Client Windows win32-win7 Raw+Zip+Wix POSTFIX Win7`, without affecting siblings. Compatibility checks stay outside application targets.

`platform::process_identity` pairs PID and start time because PIDs can be reused; client-session diagnostics match both. Windows liveness polls the handle with zero timeout rather than exit code `259` (`STILL_ACTIVE`), which a terminated process can retain while another process holds its handle. `BuildTools/tests/test_process_identity.py` covers live and terminated retained-handle cases when `clang++` is available.

#### Temporary compatibility

Temporary old-build/data handling uses native `FO_TEMPORARY_COMPAT(Id, "YYYY-MM-DD");` (`BasicCore.h`) or managed `[TemporaryCompat("Id", "YYYY-MM-DD")]` (`CoreScripts/Attributes.cs`). Share one id/date across implementations, members and tests. Native `static_assert` checks date shape at namespace/class/block scope.

CMake's `TemporaryCompat(Id "YYYY-MM-DD")` in `BuildTools/cmake/helpers/Commands.cmake` marks temporary refusals/dependency exclusions and checks argument/date shape. Permanent gates need no date. After validating platform support, remove only the temporary exclusion.

`BuildTools/temporary_compat.py` and its CI tests scan `Source/` and `BuildTools/cmake/` by default (`.cmake`, `CMakeLists.txt`, inactive branches); projects supply files/directories. Comments/CMake strings are ignored. Malformed/inconsistent dates or dates over 366 days ahead fail; expiry fails the next day at every location. Remove expired code/constraints or review an extension, preserving reproducible builds.

### Diagnostics and failure handling

`BaseLogging.*`/`Logging.*` own logging. `logging::write_message()` collapses consecutive equal messages per `logging::type`; the next different line first prints a summary, e.g. `...and 25 more same messages`. `logging::to_file()` opens non-exclusively; each `WriteSync` seeks to the end so separately linked Engine modules can append in one process. Before global data exists, `logging::write`/`logging::write_base` fall back to base logging, then `std::cout`.

The early native-only fatal layer `FatalError.*` follows `StackTrace`/`BaseLogging`. It suspends async writes, synchronously emits a message/native trace, then delegates termination to status-only `exit_app(false)`, also used for controlled command failures. It owns `fatal::report_and_exit`, `fatal::report_strong_assert_and_exit` and `FO_BASIC_STRONG_ASSERT`, without exception objects or later `ExceptionHandling`. Android `exit_app` uses `quick_exit` with both logging flushes, skipping TLS/global destructors; normal shutdown explicitly cleans up the runtime first.

`StackTrace.*` captures/formats native/script stacks; later `ExceptionHandling.*` owns exception-object reporting. For debugger workflows, see [Native, AngelScript, and Managed Debugging](../../troubleshooting/debugging.md).

### Memory, pointers, and lifetime utilities

`MemorySystem.*` owns backup-memory chunks, bad-allocation reporting and `safe_allocator`. `SmartPointers.*` makes ownership, nullability and raw-reference intent explicit; [SmartPointers.md](../../contributing/coding-contracts/smart-pointers.md) owns native `ptr` / `nptr` vocabulary and migration. Keep generic ownership here; entity lifetime and holder semantics belong in [Entity Model](../../explanation/entity-and-property-model/).

#### Callable vocabulary

`FunctionObjects.*` provides two Engine alternatives to `std::function`: default `function<Signature>` aliases move-only `move_only_function<Signature>`; use `copyable_function<Signature>` only when copying the target is the ownership contract, e.g. a callback snapshot distributed to several owners. Both inline small nothrow-movable targets and fail-fast allocate larger ones; construction adds no recoverable `std::bad_alloc`. If migration exposes a copy, first decide whether its owner should move instead. The `StackTrace.h` script-provider hook is the sole retained `std::function`, because it precedes `FunctionObjects` in dependency order.

#### Allocation vocabulary

Engine code allocates through exactly two surfaces:

- **The `fo` aliases** in `Containers.h`: `string`, `wstring`, `vector`, `map`, `unordered_map`, `set`, `list`, `deque`, `stringstream`, `small_vector` and friends. `string` / `wstring` use engine `basic_string` from `StringObject.h`; `deque` uses `basic_deque` from `DequeObject.h`; other allocator-aware aliases use `safe_allocator`. Never substitute `std::` originals.
- **`safe_alloc`** provides `make_raw` / `make_unique` / `make_shared` / `make_refcounted` / `make_raw_arr` / `make_unique_arr` for typed objects, plus raw `malloc_raw` / `calloc_raw` / `realloc_raw` / `free_raw` and `malloc_aligned_raw` / `free_aligned_raw` for C-ABI boundaries.

Third-party C-shaped hooks may require `realloc`, untyped bytes, or both, which a C++ allocator cannot express. The raw tier keeps `safe_allocator`'s OOM policy: report, drain the backup pool, retry, then exit deterministically. Library integration therefore retains the contract. Zero-size requests pass through and are not treated as failures.

Underlying `rpmalloc` primitives are not exported from `MemorySystem.h`: they return null on failure, so exporting them would bypass the policy. They remain file-local statics in `MemorySystem.cpp`. The vendored allocator returns null for both on-demand commit failure in a reserved span and failed recommit of a decommitted free page. It recommits before publishing the page as available; failure restores it to the free list, preventing backup-pool retry from handing out reserve-only memory. `Test_MemorySystem.cpp` injects both failures. Threads serialize bad-allocation reports; if trace generation cannot allocate, the nested report writes only its header without recursion. Cross-thread backup-chunk releases return to the original thread heap, without guaranteeing retry headroom for every thread. Mono's lock-free allocator is outside this contract and may abort on OOM. Public block operations `memory::copy` / `memory::move` / `memory::fill` / `memory::compare` / `memory::read_unaligned` / `memory::write_unaligned` are unrelated to allocation.

Vendored rpmalloc retains upstream 256 MiB spans on 64-bit targets; on 32-bit targets spans shrink to `LARGE_PAGE_SIZE` (16 MiB). Older Windows APIs reserve `size + alignment`, so an aligned 256 MiB span may need a contiguous 512 MiB hole in a 2 GiB x86 address space, failing the first small allocation. The 16 MiB x86 span preserves every built-in page class without that enormous contiguous reservation.

Bypassing this vocabulary has three distinct, unequally severe effects:

| | What actually happens |
|---|---|
| **Separate heap** | Global `operator new`/`delete` are replaced with rpmalloc, so every `new` and every `std::allocator` already lands in the engine heap. But rpmalloc is built with `ENABLE_OVERRIDE=0`, so C `malloc`/`free` is **not** intercepted — anything allocating through it lives in the CRT heap, outside rpmalloc, invisible to `memory::get_in_use_bytes()` and to Tracy allocation tracking. |
| **Wrong out-of-memory policy** | `std::allocator` throws `std::bad_alloc` instead of following the terminate-on-OOM model in [ExceptionSafety.md](../../contributing/coding-contracts/exception-safety.md) §1. |
| **Alignment** | `safe_allocator` routes over-aligned element types through the aligned `operator new`/`delete` overloads. Note that the over-alignment test must stay a member *function*: `alignof(T)` needs a complete `T`, while the allocator has to remain usable with an incomplete one, since `std::vector<T>` may be declared before `T` is defined. |

Accepted limits: `std::future`/`std::promise`/`std::packaged_task`, `std::thread`, `std::filesystem::path` and file streams have no allocator parameter; global `new` reaches the engine heap but throws on exhaustion. The sole `std::function` in `StackTrace.h` also precedes the engine callable module. Separately, `BasicCore`, `StackTrace` and `BaseLogging` precede `MemorySystem` in `Essentials.h` and deliberately use `std::` containers: `MemorySystem.cpp` calls `stack_trace::get()` from `memory::report_bad_alloc`, so reporting cannot depend on the failed allocator.

#### Allocator occupancy diagnostics

`memory::get_allocator_statistics()` returns an allocation-free snapshot. With rpmalloc, Debug/Tracy builds or regular builds with `FO_MEMORY_DIAGNOSTICS=ON` enable it; otherwise `available` is false. Shared AngelScript/Managed C# export `Game.GetAllocatorStatistics()` returns an empty dictionary when unavailable. The script dictionary itself allocates; the native snapshot does not.

- `mappedBytes`, `committedBytes`, `hugeAllocatedBytes` and `heapCount` describe the calling native module's rpmalloc instance. Commitment is `global.active`, not cumulative commit traffic.
- `threadSizeClassAllocatedBytes` measures occupied block capacity in the calling thread, including rounding and deferred cross-thread releases, excluding huge allocations. It is neither requested payload size nor a process-wide live-byte count.
- `threadReusableBlockBytes` counts immediately reusable slots, including unused page tails; `threadFreeCommittedPageBytes` separately counts cached committed pages.
- `class<block-size>AllocatedBlocks` and `class<block-size>ReusableBlocks` preserve size-class distribution; a slot generally cannot satisfy larger allocations.

The snapshot never walks another live thread's heap, drains deferred frees, forces GC or trims caches. Global atomic counters are not one atomic multi-field snapshot. CRT allocations, GC-object storage, drivers, padding and GPU fragmentation are outside its scope. Do not subtract thread occupancy from global commitment to infer fragmentation; reusable capacity alone does not prove harmful fragmentation. `Test_MemorySystem.cpp` checks owning-thread holes/reuse without cache flushing.

#### Third-party allocators

Vendored vkd3d-shader has no allocator hook. Bake-scoped calls use plain `malloc` and release returned objects through vkd3d's own free functions; errors fail the effect bake. These temporary allocations are not Engine-heap or Tracy allocation events.

| Library | Routed to | Where |
|---|---|---|
| ImGui | `safe_allocator` | `Common/ImGuiExt/ImGuiStuff.cpp` |
| AngelScript | `safe_allocator` | `Scripting/AngelScript/AngelScriptScripting.cpp` |
| zlib | `safe_allocator` | `Essentials/Compressor.cpp` |
| ozz-animation | `safe_alloc` aligned tier | `Common/ModelAnimationData.cpp` |
| meshoptimizer | `safe_allocator` | `Tools/ModelMeshBaker.cpp` |
| ufbx | `safe_allocator` | compile-time `UFBX_EXTERNAL_MALLOC` plus `extern "C" ufbx_malloc/realloc/free` in `Tools/ModelMeshBaker.cpp` |
| SDL | `safe_alloc::*_raw` | `Frontend/Application.cpp` |
| Effekseer | `safe_alloc::*_raw` + aligned | `Client/EffekseerExtension.cpp`, declared in its header; both owners (client runtime and `Tools/ParticleBaker.cpp`) install through that one definition |
| libpng | `safe_alloc::*_raw` | `Tools/ImageBaker-Png.cpp`, via `png_create_read_struct_2` |
| libbson / mongo-c | `safe_alloc::*_raw` + aligned | `Server/DataBase-Mongo.cpp`; only the Mongo factory installs the process-global vtable before constructing its backend |
| SQLite | `safe_alloc::*_raw` | `Server/DataBase-SQLite.cpp`, via `sqlite3_config(SQLITE_CONFIG_MALLOC)` before `sqlite3_initialize()` |

Read the bson vtable before copying it: it supplies `aligned_alloc` but frees through plain `free` without recording alignment. Both routes must share a release function. With rpmalloc, `rpaligned_alloc` and `rpmalloc` end in `rpfree`; on POSIX without it, `posix_memalign` blocks are `free()`-able. Windows without rpmalloc breaks this: sanitizer configs disable it through `expr_RpmallocEnabled` for interposition, leaving `_aligned_malloc`/`_aligned_free`. Only there, `BsonAlignedAlloc` uses plain `safe_alloc::malloc_raw`, matching bson's MSVC default and its rationale (`_aligned_alloc_impl` in libbson `memory.c` deliberately avoids `_aligned_malloc`). Mongoc aligned requests are all `BSON_ALIGNOF` of ordinary C structs, covered by malloc's fundamental alignment. The Mongo factory installs identical process-global callbacks before backend allocation; later changes could pair old allocations with new free callbacks. Omitting `aligned_alloc` is invalid: bson's fallback discards requested alignment on every platform, not only Windows.

SQLite needs `xSize` but passes only a pointer to free/realloc/size callbacks, so blocks carry an 8-byte size header. Install configuration *before* `sqlite3_initialize`: the library uses `SQLITE_OMIT_AUTOINIT`, and all callers use one exported initializer.

Unhooked: **Monocypher** allocates nothing; callers own all buffers. **LibreSSL** declares `CRYPTO_set_mem_functions` but implements inert `return 0;`: upstream removed custom allocators, so calling it would imply nonexistent coverage. **ogg / vorbis / theora** have no allocator hook.

When vendoring/updating, check for an allocator hook, wire it or document why not, and inspect its *implementation*, not just the declaration. Two entries above were initially misjudged from call sites/symbol names alone.

#### Vector containers and inline storage

`Containers.h` provides two `safe_allocator<T>`-backed sequence aliases:

- `vector<T>` remains the default dynamically allocated sequence for unbounded data, persistent collections amortizing allocation, move-heavy pipelines and exact engine interfaces.
- `small_vector<T, InlineCapacity>` embeds up to `InlineCapacity` elements, spilling to `safe_allocator` above that limit. Engine aliases require explicit capacity; vendored `GCH_SMALL_VECTOR_DEFAULT_SIZE` is configuration, not capacity-selection policy.

Use `small_vector` only when measurements or a hard protocol limit show a frequently constructed collection is usually small. Choose capacity from observed typical cardinality, preserve heap spill for rare large cases, and count inline bytes in every instance. Per-call scratch lists can fit; several inline buffers in every dense-map cell may cost more than the avoided allocation. `inlined()` reports current storage mode for focused tests/profiling.

Representation affects correctness:

1. Moving an inline `small_vector` relocates elements into the destination's inline buffer. Source pointers/references/iterators do not follow as they commonly do when heap-backed `vector` transfers its allocation. Audit every address retained across a move.
2. Inline moves/swaps perform element work and are only conditionally `noexcept`; re-derive the function's exception guarantee rather than inheriting `vector` assumptions.
3. A `small_vector<T, N>` member instantiates inline element destruction at its containing class boundary, requiring complete `T` there. It cannot simply replace a `vector` member with a forward-declared element type.
4. With this vendor, a member of nested element type with default member initializers can make default-inserting operations ill-formed before the enclosing class is complete, notably on Clang. Shrink with `erase(begin() + new_size, end())`; otherwise move the type outside or make construction requirements explicit.
5. Heap spill retains terminate-on-OOM through `safe_allocator`. Element construction, conversion and moves may still throw; see [ExceptionSafety.md](../../contributing/coding-contracts/exception-safety.md).

Vector-like operations do not justify substituting `small_vector` at exact-type boundaries:

- Script export/codegen signatures and `ScriptSystem` registration require exact `vector<T>` / `readonly_vector<T>` spellings and identities.
- Property writes, serialized stores, `data_reader` / `data_writer`, `NetBuffer` and `CScriptArray` require exact `vector` contracts at selected boundaries.
- Span-taking internal helpers can accept both representations without exposing concrete types: the preferred seam when both are valid.
- `FO_ENTITY_PROPERTY` cannot directly accept `small_vector<T, N>` because its comma also separates macro arguments.

`vector_collection` admits both aliases to generic readers. `vec_filter`, `vec_transform` and `vec_sorted` preserve `vector` versus `small_vector` and inline capacity via `rebind_vector_t`; non-vector ranges become `vector`. `to_vector` always materializes `vector`; `copy_hold_ref` returns an opaque ref-held snapshot, not a concrete sequence contract. Generic numeric formatting accepts both, but string/bool special cases match exact `vector<string>` and `vector<bool>`: extend/test the formatter before assuming equivalent `small_vector<string, N>` or `small_vector<bool, N>` behavior.

For adoption, record measured distribution and object count; audit address lifetime and move/swap sites, complete types and exact interfaces. Add focused coverage for inline operation, spill and generic-helper result types. Run the full native unit suite, project exception-safety/smart-pointer audits when present, and representative bake/gameplay/profiling paths for the affected subsystem.

#### Deque containers

`DequeObject.*` owns `basic_deque<T, BlockBytes>`, aliased by `Containers.h` as `deque<T, BlockBytes = DEQUE_BLOCK_BYTES>`. Default blocks hold 512 bytes, never fewer than four elements; measured call sites may choose another size. Growth at either end preserves existing elements/references, as required by engine message, packet, task, input and database queues. Never substitute `std::deque`: its fixed block policy is outside Engine allocator/performance contracts.

#### Random generation

`RandomGenerator.*` owns `random_generator`, the Engine's xoshiro256++ source.
Default construction seeds it from the OS; explicit seeding expands the seed
through SplitMix64. Use `next()` for raw bits, `next_below(bound)` for
`[0, bound)`, `next_between(min, max)` for an inclusive signed range, and
`next_normalized()` for `[0, 1)`. These bounded mappings are Engine-owned and
therefore deterministic across supported standard libraries. Do not use
`std::mt19937` or `std::uniform_int_distribution` for engine behavior.

#### Cryptography

`Cryptography.*` owns the `crypto::` primitives used by the [secure network channel](../../explanation/authority-and-networking/#secure-channel): X25519, BLAKE2b-512 and HMAC-BLAKE2b, and RFC 8439 ChaCha20-Poly1305. The implementation uses the same vendored Monocypher on native, Web and Android targets. `crypto::fill_random` obtains key material from the operating system through `platform::fill_system_random` (`BCryptGenRandom` on Windows, `getentropy` on Linux/Web, `arc4random_buf` on Apple and Android); it throws if the platform cannot provide it. Never use `random_generator` to create a channel key. AEAD sealing/opening creates a fresh Monocypher context per message; the library's streaming context rekeys after its first message. `crypto::is_equal` compares in constant time and `crypto::wipe` clears secret buffers; owners of plain key arrays wipe them during teardown.

### Serialization, values, strings, and hashes

Use existing `strex` / `strvex` formatting/parsing operations. Repeated missing operations belong in the reusable utility owner, not new ad hoc parsers/formatters at each call site.

`StringObject.*` owns engine `basic_string`, following `std::basic_string`. `FO_STRING_INLINE_CAPACITY` sets both `string` and `wstring` inline buffers: a build-wide ABI choice, not per-call optimization. Three standard boundaries need adapters: copy text into standard streams via `make_stream_string`, build `std::filesystem::path` with `fs::make_path`, and call `getline` unqualified for ADL to select the engine overload. String growth follows deterministic terminate-on-OOM like other engine storage.

`DataSerialization.*` supplies binary I/O for networking, persistence, resources and tests. `data_reader::read<T>()` / `data_writer::write<T>()` byte-copy standard-layout values, independent of buffer alignment. Zero-copy `read_ptr<T>(size)` accepts only raw byte/string views (`uint8_t`, `char`, `void`); aligned typed values use `read<T>()` or `read_ptr(destination, size)`. `StringUtils.*`, `HashedString.*`, `StrongType.*`, `ExtendedTypes.*`, `SafeArithmetics.*` and `TimeRelated.*` supply reusable primitive-like values. `iround` rejects non-finite/out-of-int64-range floats before rounding, protecting `std::llround` from undefined inputs. `hash_storage::set_resolve_hash_failure_handler` exposes failed resolution in throwing and flagged no-throw lookups without imposing recovery policy on essentials.

`timespan::to_ms/to_sec` preserve fractions for floating T; integers truncate toward zero. Use `div<T>` for ratios and explicit adapters for numeric file/config/wire units. Explicit zero durations use existing `timespan::zero`; normal field/local initialization may keep `{}`.

### Filesystem, compression, sockets, and work threads

`DiskFileSystem.*` abstracts low-level disk I/O. `fs::disk_read_file` captures a descriptor/length; `read_at` makes bounded positional reads, and a region constructor opens uncompressed APK `.fores` without copying. `fs::disk_write_file` excludes competing writers, appends or truncates an uncommitted suffix, and flushes with `fsync`/`_commit`. `fs::disk_directory_lock` serializes resource mutation without a lock file (`flock` on POSIX, named mutex on Windows); unlike named mutexes, another flock on the same directory is non-reentrant even within one process. `fs::rename_durable` persists renames; `fs::sync_parent` persists directory entries; `fs::available_space` permits admission without preallocating resumable downloads. `fs::is_contained_relative_path` rejects rooted, traversal, colon, NUL and malformed UTF-8 paths before joining a writable root. `fs::make_writable_path` composes a relative path under the user root. Use `fs::make_path` for UTF-8 to `std::filesystem::path`, and `fs::path_to_string` back: Windows `path.string()` uses ANSI and may fail on Cyrillic profiles. Higher mounted resources belong to `Source/Common/FileSystem.*`; see [Configuration and Data Sources](../settings/configuration-and-data-sources.md). `Compressor.*` owns generic compression, `NetSockets.*` raw sockets, and `WorkThread.*` background workers.

`fs::disk_write_mode::create_new` creates exclusively, failing without truncation if the path exists. `fs::write_file` creates a temporary sibling this way, flushes/closes it, then replaces the destination; existing readers retain old descriptors/bytes. Write/publish failure removes the sibling and preserves the destination. It does not call `fs::sync_parent`: atomic visibility does not guarantee directory durability across power loss.

String and byte-span `fs::write_file` overloads publish complete files atomically. Strings preserve embedded NUL; empty input publishes an empty file. Replacement requires a writable parent and creates a new file object, preserving neither symbolic-link targets, hard-link identity nor destination-specific metadata. File-identity-sensitive consumers use lower-level `fs::disk_write_file`. Atomic publication is neither a multi-file transaction nor a writer lock.

`utf8::is_valid` rejects out-of-range code points, surrogates (`U+D800–U+DFFF`) and U+FFFD, reserved as the decoder error sentinel. `strvex::is_valid_utf8` (also on `strex`) decodes all bytes and applies that check, also rejecting malformed, overlong and truncated encodings. Even well-formed UTF-8 U+FFFD is rejected. Embedded NUL is valid text; paths reject it separately.

Every `fs` path entry rejects embedded NUL before conversion, parent extraction or I/O, preventing truncated-prefix operations. String paths require valid UTF-8 on all platforms: non-UTF-8 POSIX names are refused, literal POSIX backslashes remain valid. Native-path-to-string results are validated too, so traversal cannot return malformed names. Pure converters (`make_path`, `path_to_string`, `resolve_path`, `make_io_path`, `make_writable_path`) report invalid paths with `std::filesystem::filesystem_error`; `make_io_path` propagates conversion failures, never ignores their error code. Other operations use existing failure results/closed handles. `iterate_dir` throws before visiting invalid paths. Refused `open_ifstream` is closed with `failbit`, preventing hashing from mistaking conversion failure for an empty file. Pure writable-path composition propagates conversion/allocation errors rather than treating failed classification as a relative path.

Result-based file operations are all `noexcept`: queries, whole/bounded reads, comparisons, hashes, atomic/exclusive writes, removal, rename/sync, locks and directory-name listing. Failure uses existing `false`, empty optional, zero, closed handle or accumulated name list. Standard-path, owned-stream and iterator allocation/conversion failures are handled at individual boundaries. Known exceptions keep their error codes; other C++ exceptions there return `io_error` or the existing failure result. Ready-native-path queries and descriptor operations need no wrapper. No whole-operation catches or generic no-throw execution wrappers are used. Engine allocation still terminates on OOM.

Converters (`make_path`, `path_to_string`, `resolve_path`, `make_io_path`, `make_writable_path`) and by-value standard-stream `open_ifstream` may throw. `iterate_dir` propagates filesystem/visitor exceptions; four `stream_*` helpers propagate caller stream exceptions. Never silently convert caller exceptions to filesystem failure.

Whole-file reads reject sizes above the requested bound or engine string limit before allocation and require the exact previously observed byte count. Hashing reads bounded chunks to EOF, including the final partial chunk. Owned streams construct in place, preventing throwing moves across `noexcept`. Listing retains names accumulated before later OS/allocation/conversion failure; startup failure returns an empty list. Failed writes may leave parent directories: cleanup attempts only the operation's own file. Preserve empty-path conventions, including `create_directories("")`, `is_relative_path("")` and current-directory locks. Native parent extraction preserves POSIX backslash components.

`fs::write_file_durable_new(path, bytes)` exclusively creates, writes and flushes a new file before closing. Existing names, including directories, fail without replacement. Failed setup/write/flush closes the handle and attempts removal of only the created file; the OS may refuse removal. Empty bytes are valid; empty/NUL paths fail. Missing parents are created through the native path adapter. The new name is visible during writing: staged-publication callers use a private stage, then `fs::replace_file`. Content sync neither syncs the parent nor promises a multi-file/power-loss transaction.

Join stages through the native filesystem adapter, not `strex::combine_path`: preserve leading spaces, POSIX backslashes and native parent semantics instead of normalizing as resource paths. Successful publication releases cleanup ownership before the temporary name can be reused.

Windows publication depends on OS/filesystem capabilities and handle sharing. `fs::disk_read_file` allows delete sharing; CRT streams without it may block replacement, as may loaded native DLL images even on modern hosts. Then `false` preserves the previous complete destination and removes the stage; close consumers before retrying when needed. Atomic publication does not make every in-use file replaceable. Native tests verify refusal, unchanged old bytes and retry after releasing consumers.

Windows open-reader replacement reserves a terminating UTF-16 NUL for [`FILE_RENAME_INFO`](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-file_rename_info); `FileNameLength` counts only name bytes. Tests vary destination-name alignment, retain the old reader, and verify complete replacement through a new read.

Windows `fs::make_io_path` gives standard filesystem/file operations a literal extended-length path without changing logical resource paths. Disk tests cover Unicode names past 320 native characters and trailing-name behavior. Convert at the native I/O boundary; never truncate/shorten project paths.

`BuildTools/probe_windows_file_io.py` independently checks canonical path-adapter and directory/open/rename/remove bodies with static and dynamic MSVC runtimes. It extracts canonical validation/conversion dependencies and the UTF-8 decoder; a minimal borrow shim supplies decoder pointer operations, leaving Engine pointer checks/allocator behavior to linked tests. Standard-stream fixture writes exercise that adapter; linked native DiskFileSystem tests validate production atomic `fs::write_file`.

`Threading.h` exposes `coarse_sleep` and `precise_sleep`, replacing `std::this_thread::sleep_for` in Engine code. `coarse_sleep` parks without CPU consumption for polling/approximate waits. `precise_sleep` uses a high-resolution timer, spinning the final short interval; reserve it for deliberate sub-millisecond deadlines such as sync back-off/frame pacing. Both are `noexcept`; choose by latency intent, never interchange mechanically.

The final interval polls the steady clock without yielding the OS time slice. Yielding could give another ready thread a full scheduling quantum. Active waiting occupies only the final millisecond. OS preemption may delay either wait; neither guarantees hard real-time behavior.

The worker acknowledges `work_thread::clear()` under the state lock: queued jobs are discarded, running work finishes before acknowledgement, and `clear()` returns after the worker observes the request. Every queue shape must wake `clear`, including empty/future-dated queues and pause/resume transitions; a request dropped without waking `clear` leaves the caller asleep after the worker moves on.

A registered exception handler owns reporting of thrown `work_thread` jobs: it interprets the failure, chooses when to report it, and can update worker policy, e.g. clear queued jobs. Only without a handler does the thread use the global non-fatal exception reporter. Reporting both ways would repeat the exception behind the handler's initiated actions.

## Build integration

`BuildTools/cmake/stages/EngineSources.cmake` lists all authored Essentials `.h` / `.cpp` pairs, two `.inc` files and debugger visualization in `FO_ESSENTIALS_SOURCE`; `BuildTools/cmake/stages/CoreLibs.cmake` builds `EssentialsLib` from it. Applications, tools, tests and generated-code consumers use this core dependency chain. Place a new module's header at the correct `Essentials.h` dependency point, register its files in `FO_ESSENTIALS_SOURCE`, and add focused coverage where possible.

## Tests to inspect

Direct essentials tests include:

- `Source/Tests/Test_BaseLogging.cpp`
- `Source/Tests/Test_BasicCore.cpp`
- `Source/Tests/Test_CommonHelpers.cpp`
- `Source/Tests/Test_Compressor.cpp`
- `Source/Tests/Test_Containers.cpp`
- `Source/Tests/Test_Cryptography.cpp`
- `Source/Tests/Test_DequeObject.cpp`
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
- `Source/Tests/Test_RandomGenerator.cpp`
- `Source/Tests/Test_SafeArithmetics.cpp`
- `Source/Tests/Test_SmartPointers.cpp`
- `Source/Tests/Test_StackTrace.cpp`
- `Source/Tests/Test_StringObject.cpp`
- `Source/Tests/Test_StringUtils.cpp`
- `Source/Tests/Test_StrongType.cpp`
- `Source/Tests/Test_TimeRelated.cpp`
- `Source/Tests/Test_Threading.cpp`
- `Source/Tests/Test_WorkThread.cpp`

`Test_Containers.cpp` pins aliases, allocators, inline-to-heap transitions, moves, swaps and formatting. `Test_DequeObject.cpp` covers block growth, both-ended mutation, iterators, stable references, copy/move and destruction. `Test_RandomGenerator.cpp` pins seeded cross-platform sequences/bounded ranges. `Test_Threading.cpp` checks sleep primitives and sub-millisecond behavior. `Test_CommonHelpers.cpp` pins container-kind preservation through `rebind_vector_t` and producing `vec_*` helpers.

See [Testing](../../contributing/testing/) for complete suite maps and target wiring.

## Change routing

- Compiler/OS gates, namespace, base aliases, and low-level macros: `Source/Essentials/BasicCore.*`.
- Global create/delete callback registration: `Source/Essentials/GlobalData.*`.
- Stack traces, logging, and exception reporting: `Source/Essentials/StackTrace.*`, `BaseLogging.*`, `Logging.*`, `ExceptionHandling.*`, and [Native, AngelScript, and Managed Debugging](../../troubleshooting/debugging.md).
- Generic memory/pointer utilities: `Source/Essentials/MemorySystem.*`, `SmartPointers.*`, and [SmartPointers.md](../../contributing/coding-contracts/smart-pointers.md).
- Callable ownership and inline targets: `Source/Essentials/FunctionObjects.*`.
- Engine strings and the build-wide inline-capacity contract: `Source/Essentials/StringObject.*`; deque storage: `DequeObject.*`; aliases and stream interop: `Containers.h`.
- OS-call confinement and dispatch: `Source/Essentials/WinApi.*`, `Posix.*`, and `Platform.*`.
- Cross-platform random sequences: `Source/Essentials/RandomGenerator.*`; coarse and precise waits: `Threading.*`.
- File bytes and low-level writable-path composition on disk: `Source/Essentials/DiskFileSystem.*`; mounted engine resources and installed-client overlays: [Configuration and Data Sources](../settings/configuration-and-data-sources.md).
- Socket primitives: `Source/Essentials/NetSockets.*`; protocol/command/network runtime: [Networking](../../explanation/authority-and-networking/).

`fs::try_lock_file(path)` returns move-only `fs::exclusive_file_lock` with `owns_lock()` and idempotent `reset()`. It opens/creates without truncation or waiting; busy/invalid/unavailable paths and Web builds return closed owners. Parents must exist. Windows uses wide paths, long-path normalization and denied sharing; POSIX uses advisory exclusive `flock`. Moves transfer ownership, replacement releases the previous lock, and destruction/process exit releases it. Child executions do not inherit descriptors. The file remains: never unlink while a participant may hold it. See [the baking guide](../../explanation/content-pipeline/baking.md#output-ownership-and-competing-processes).

## Validation checklist

1. Exclude dependencies from essentials back to higher layers.
2. Update `BuildTools/cmake/stages/EngineSources.cmake` for added/removed files.
3. Run the smallest matching essentials test, then broader `RunUnitTests` when behavior crosses utility boundaries.
4. For diagnostic changes, keep [Native, AngelScript, and Managed Debugging](../../troubleshooting/debugging.md) accurate.
5. For filesystem/socket/threading contract changes, validate at least one higher-level consumer.
6. For `small_vector` adoption, prove capacity/object counts with data, audit move/address lifetime/exact types, and rerun exception-safety/pointer-ownership gates.
