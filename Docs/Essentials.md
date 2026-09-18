# Essentials

> Engine-owned documentation. This page maps the low-level `Source/Essentials/` layer: platform/compiler prerequisites, process-wide lifecycle helpers, logging, memory, strings, serialization, filesystem, sockets, and utility types used by every higher engine layer.

## Purpose

Use this page when changing code that sits below `Source/Common/` or when you need to know whether a utility belongs in the reusable engine foundation instead of client, server, tools, or game-specific code.

For the memory model's exception contract — `safe_alloc` / `safe_allocator` terminate on OOM (so `std::bad_alloc` is not a recoverable error) and the `throw` / `FO_VERIFY_*` / `FO_STRONG_ASSERT` error tiers built on `ExceptionHandling.h` — see [ExceptionSafety.md](ExceptionSafety.md).

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
- `Source/Essentials/StringUtils.h`
- `Source/Essentials/StringUtils.cpp`
- `Source/Essentials/WinApi.h`
- `Source/Essentials/WinApi.cpp`
- `Source/Essentials/Posix.h`
- `Source/Essentials/Posix.cpp`
- `Source/Essentials/Platform.h`
- `Source/Essentials/Platform.cpp`
- `Source/Essentials/Posix.h`
- `Source/Essentials/Posix.cpp`
- `Source/Essentials/ExceptionHandling.h`
- `Source/Essentials/ExceptionHandling.cpp`
- `Source/Essentials/RandomGenerator.h`
- `Source/Essentials/RandomGenerator.cpp`
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

`BasicCore` → `GlobalData` → `StackTrace` → `BaseLogging` → `FatalError` → `FunctionObjects` → `SmartPointers` → `MemorySystem` → `StringObject` → `DequeObject` → `Containers` → `StringUtils` → `WinApi` → `Posix` → `Platform` → `ExceptionHandling` → `RandomGenerator` → `Threading` → `SafeArithmetics` → `DataSerialization` → `HashedString` → `StrongType` → `TimeRelated` → `ExtendedTypes` → `Compressor` → `WorkThread` → `Logging` → `DiskFileSystem` → `CommonHelpers` → `NetSockets`.

The order is both a compile-time and link-time rule. A module may include and call only modules to its left; declaring an API in an early header and defining it in a later `.cpp` is still a reverse dependency. Direct includes and namespace-level definition ownership are checked by `BuildTools/tests/test_essentials_layering.py`. Keep new essentials APIs free of dependencies on `Source/Common/`, `Source/Client/`, `Source/Server/`, `Source/Tools/`, or embedding-project headers.

`Threading` deliberately follows `ExceptionHandling`, its deepest dependency, while remaining early enough for value headers such as `HashedString` to guard their state with the shared synchronization primitives. See [ThreadSafetyAnalysis.md](ThreadSafetyAnalysis.md).

## Naming

Essentials is spelled in **snake_case**, and every layer above it in **PascalCase**. This is the layer's identity written into its names: Essentials is the engine's standard-library-shaped vocabulary, so `data_writer`, `safe_alloc::make_unique`, `logging::write`, `hash_storage::to_hashed_string` and the private `_read_pos` read the way `std::` reads, while `ServerEngine`, `ProtoManager` and `SpriteManager` above them do not. The spelling therefore tells a reader which side of the boundary a name lives on without looking it up.

Five kinds of name stay PascalCase inside the layer, in every case because the name is not Essentials' to choose:

| Kept PascalCase | Why |
|---|---|
| Template parameters — `CharT`, `Traits`, `Alloc`, `InlineCapacity`, `FormatContext` | The standard library's own convention; snake_case here would read as a type, not a parameter |
| Exception type names — `BaseEngineException` and the `FO_DECLARE_EXCEPTION(...)` types | `XException` is an engine-wide convention that Essentials only seeds; renaming its share would split one vocabulary in half |
| The ref-count protocol *as AngelScript spells it* — `AddRef`, `TryAddRef`, `Release` on `asIScriptFunction` and `asITypeInfo` | `refcountable` accepts either spelling and `refcount_ptr` dispatches on whichever the pointee declares, so `refcounted` uses `addref` / `release` / `get_refcount` while the library types keep their own |
| Foreign API names re-spelled in `WinApi.cpp` / `Posix.cpp` | `CloseHandle`, `RegCloseKey` and the rest are the operating system's names; only the wrappers around them are ours |
| The global crash hooks in `ExceptionHandling.cpp` — `GetCrashStream`, `SetCrashStackTrace`, `SetCrashSignalInfo`, `SetCrashSehInfo`, `SetCrashTerminationInfo` | The vendored `backward.hpp` declares them by name at global scope (`// (FOnline Patch)`); the spelling is part of that contract, not ours |

Module **file** names are PascalCase as well (`MemorySystem.h`, `BasicCore.h`): the `Essentials.h` include-order block and `BuildTools/tests/test_essentials_layering.py` are keyed on them, and file naming is an engine-wide convention rather than a per-layer one.

## Namespaces

A free function that has to carry its module's name gets that module's namespace instead, and the word comes out of the
name. `write_base_log` said "base log" in the identifier because nothing else could; `logging::write_base` says it in the
scope, which is where it belongs. The layer already read this way at its edges — `winapi::close_handle`,
`posix::sync_file`, `platform::get_exe_path`, `utf8::decode` — and now does so throughout:

| Namespace | Module | Reads as |
|---|---|---|
| `logging::` | `Logging.*` + `BaseLogging.*` | `logging::write(...)`, `logging::type::warning`, `logging::write_base(...)`, `logging::to_file(path)` |
| `stack_trace::` | `StackTrace.*` | `stack_trace::get()`, `stack_trace::format(st)`, `stack_trace::data`, `stack_trace::frame` |
| `exceptions::` | `ExceptionHandling.*` | `exceptions::report_and_continue(ex)`, `exceptions::set_callback(...)` |
| `fatal::` | `FatalError.*` | `fatal::report_and_exit(msg)` |
| `global_data::` | `GlobalData.*` | `global_data::create()`, `global_data::destroy()` |
| `memory::` | `MemorySystem.*` | `memory::copy(...)`, `memory::report_bad_alloc(...)` |
| `fs::` | `DiskFileSystem.*` | `fs::exists(path)`, `fs::read_file(path)`, `fs::iterate_dir(...)` |

Once a module takes a namespace its whole free surface moves in, types included, so it is never split across two
scopes — `stack_trace::data` sits beside `stack_trace::get()`, and `memory::bad_alloc_callback` beside
`memory::set_bad_alloc_callback`. The exception-type hierarchy stays in the engine namespace under its own convention,
and so do `safe_alloc` / `safe_allocator`, which are vocabulary rather than a module service.

The test is the name, not the module. A function that never needed a prefix is already the layer's vocabulary and stays
bare: `numeric_cast`, `iround`, `checked_add`, `safe_call`, `copy_hold_ref`, the `vec_*` helpers, `coarse_sleep`,
`precise_sleep`, `run_async`, `exit_app`, `make_stream_string`. Those read as language, and qualifying them would only
add noise.

One namespace name is not ours to pick: `log` collides with `::log` from `<cmath>`, which vendored headers call
unqualified, so the module is `logging::`.

## Subsystem map

### Platform and compiler gate

`BasicCore.h` enforces the selected OS macro (`FO_WINDOWS`, `FO_LINUX`, `FO_MAC`, `FO_ANDROID`, `FO_IOS`, or `FO_WEB`) and requires C++20. It also binds frequently used standard types into the engine namespace and declares core macros such as `FO_EXPORT_FUNC`, `FO_KEEP_DATA_SYMBOL`, and namespace helpers. Warning-suppression helpers also live here: `FO_DISABLE_WARNINGS_PUSH/POP` silence all warnings (for wrapping third-party header includes), while the per-compiler `FO_GCC_IGNORE_WARNINGS_PUSH/POP`, `FO_CLANG_IGNORE_WARNINGS_PUSH/POP`, and `FO_MSVC_IGNORE_WARNINGS_PUSH/POP` silence one named diagnostic and are active only on their matching compiler (so a single-toolchain false positive can be suppressed at one site without other toolchains rejecting an unknown `-W` name or warning number). Prefer fixing warnings at their root; reach for the per-compiler helpers only for documented compiler false positives.

`GlobalData.*` is the process-wide bootstrap registry: `FO_GLOBAL_DATA(Type, Instance)` registers a create and a delete callback, and `global_data::create()` / `global_data::destroy()` run them as a set. Four properties are contractual. **The set is whole or the process is gone**: the callbacks are `void (*)() noexcept`, so a constructor that throws terminates rather than leaving half the globals built, and nothing has to handle a partial set. **Both sweeps are serialized and symmetric**: they take one lock, a create over an existing set or a delete over a torn-down one does nothing, and `global_data::create()` answers whether this call built the set. An entry point that did not build the set must not tear it down: the baker library entry `FO_BakeResources` runs as its own module and inside an application alike. The lock is recursive on purpose — not to allow re-entry, but so that a callback reaching back into a sweep is met with a message and a deterministic exit rather than a deadlock inside the lock it already holds. **A library returning to a host that carries on tears its set down first**: the client runtime (`RunClientRuntimeAbi`) and the baker library join the threads their set owns — the async log writer and the global pools — before handing control back, so nothing they started is still running, or is killed holding a lock, when the host goes on to exit. Tearing down is not unloading: engine libraries are loaded with `platform::load_pinned_module` and are never unmapped, because the runtimes statically linked into them install process-wide hooks that cannot be withdrawn — Mono's vectored exception handler, rpmalloc's per-thread FLS callback, the backward-cpp unhandled-exception filter — and with the static CRT a running `std::thread` keeps its module mapped regardless. **Access is checked**: the instance is reached through `operator->` on a holder that ends the run when the data is not there, since reaching for a global outside its lifetime is a startup or teardown ordering defect and a null dereference would surface far from its cause. The one unchecked question is `is_created()`, which the paths that must work before the first create and after the last delete ask: logging, exception reporting (`exceptions::get_callback`), the out-of-memory path, and the error-message ignore list. Crash recording asks nothing, because a crash handler can fire at any moment of the process: the crash record and the stream that prints it are process-lifetime statics beside the handlers. The same holds for the rest of the deliberately process-lifetime state outside the set — the registry itself, stack-trace state and its resolver, `run_in_debugger`, `hstring::_zero_entry`, thread names, and data patched into the binary.


`Platform.h` / `.cpp` owns host-specific helpers that are deliberately small: informational logging, thread names, executable path lookup, per-user data directory lookup, process id formatting, fork support where available, process memory usage, CPU usage snapshots, and dynamic module loading (`load_module`, and `load_pinned_module` for a module that must stay mapped until the process exits). `platform::get_user_data_base()` is shell-free by preference, not by rule: it reads the environment first — Windows `%LOCALAPPDATA%` (else `%APPDATA%`), macOS/iOS `$HOME/Library/Application Support`, Linux/Android/other `$XDG_DATA_HOME` (else `$HOME/.local/share`) — and only when the environment says nothing does it ask the OS itself, through `winapi::get_local_app_data_path()` (`SHGetKnownFolderPath`) or `posix::get_home_dir()` (`getpwuid_r` on hosts that have a user database; the web target has none and answers none). Returning nothing sends a client back to its install directory, which the writable root exists to avoid. Higher layers append the application name and decide whether absence is fatal. `platform::get_cpu_usage_snapshot()` returns cumulative per-core system counters plus the current process CPU time; callers compare two snapshots to compute percentages and keep any sampling/cache state outside the Platform layer. `Platform` stays above `ExceptionHandling` and uses the earlier `FO_BASIC_STRONG_ASSERT` for terminating host-API invariants rather than importing late exception macros. Platform-specific application/window/rendering behavior lives under `Source/Frontend/`, not here.

`WinApi.*` and `Posix.*` own the operating-system calls themselves, under the `winapi::` and `posix::` namespaces, and they are the only place `<Windows.h>` and the POSIX headers are included. That confinement is the point: `<Windows.h>` defines macros over hundreds of ordinary words, which is why every file that pulls it in also has to include `WinApiUndef.inc` to take them back. Both headers keep OS types off their boundary — engine `string`, `optional`, fixed-width integers and `nptr<void>` in and out — so a caller never learns what a `HANDLE` or a `pid_t` is. `Platform` is now a dispatcher over the two: each of its functions is a `#if FO_WINDOWS` choosing `winapi::` or `posix::`, and the OS-specific bodies live in the modules. Where a caller uses one whole family, a `namespace osfile = winapi;` alias (as the database recovery oplog does) keeps the platform branch out of every step. Where a platform genuinely lacks an API, the declaration is compiled out rather than kept as a stub that ignores its arguments and returns a failure sentinel: `posix::open_exclusive_file` and `posix::run_process_capturing_output` are absent on the web build, which has no advisory lock and spawns no process, and the `dlopen` trio exists only on Linux and macOS — `platform::load_module` answers for Android and web, where the runtime is linked in rather than loaded. A caller that would need one of those on a platform without it is a build error, which is the honest outcome.


File offsets are 64-bit on every platform the engine builds, and that is proved rather than assumed: `Posix.cpp` asserts at compile time that `off_t` is eight bytes wide, and the one target where it is not - a 32-bit Android ABI, where the NDK keeps `off_t` narrow below `_FILE_OFFSET_BITS=64` - names the wide calls instead (`lseek64`, `pread64`, `ftruncate64`, `posix_fallocate64`) and adds `O_LARGEFILE` at open. A platform whose `off_t` cannot carry a pack offset past 2 GB therefore fails to build rather than truncating one silently. Preallocation is a ladder, not a call: Linux and Android try `posix_fallocate` to reserve the blocks and fall back to `ftruncate` where the filesystem will not, Windows uses `_chsize_s`, which moves the end of file without writing zeroes, and everywhere else the truncate step is the whole of it. Only the resulting size is contractual - the reserved blocks are best effort, so a caller still has to handle a later write that fails for space.
Three implementations below the modules in the `Essentials.h` order keep their own OS calls, and that is structural rather than an oversight: `BasicCore.cpp` (debugger detection and the debugger break) is the first header in the order, so nothing can move out of it without inverting the layering; `BaseLogging.cpp` holds a single `SetConsoleOutputCP`; and `StringUtils.cpp` owns the UTF-8/UTF-16 conversion, which is the string layer's own primitive. Two more stay outside for a different reason — they are OS wrappers themselves rather than consumers: `NetSockets.*` is the engine's BSD socket layer, and `ServerServiceApp.cpp` is a Windows service host whose contract with the OS is a set of callbacks, not a call list.

Windows builds retain the `_WIN32_WINNT=0x0601` compile baseline. One Windows build-platform registry owns the CMake architecture, toolset, and canonical packaging architecture for the regular, `-clang`, and `-win7` variants. The Win7 pair pins MSVC 14.44, while `FO_BINARY_OUTPUT_POSTFIX` remains independent of the platform. In the package DSL the corresponding `BINARY` entry can select its own postfix, for example `BINARY Client Windows win32-win7 Raw+Zip+Wix POSTFIX Win7`, without affecting sibling binaries in the package. Compatibility checks are kept outside application targets.

### Diagnostics and failure handling

`BaseLogging.*` and `Logging.*` provide the logging foundation. `logging::write_message()` collapses immediate duplicates by `logging::type` and message text: repeated copies are skipped, then the next different log line first emits a summary such as `...and 25 more same messages`. `logging::to_file()` opens the log file without an exclusive lock (the platform default: MSVC `std::ofstream` opens deny-none, POSIX has no mandatory open lock) so two engine modules in one process — e.g. a runtime host EXE and the runtime DLL it loads, each with its own copy of the engine global data — can both hold the same file open at once, and every write seeks to end of file first (`write_sync`) so neither handle overwrites content the other appended; the `append` parameter still selects truncate (default) vs append for the initial open. `logging::write`/`logging::write_base` degrade safely when their global data is not yet created (falling back to the base log, then to `std::cout`), and an application opens the file as its second statement: the writable root it belongs in is resolved from the command line and the installer marker alone (`ResolveWritableRoot`, Frontend), so nothing has to be read from disk first and no line has to be buffered or moved afterwards. `logging::get_file_path()` answers with the file currently open, which is how a crash reporter attaches the log the run is actually writing.

`FatalError.*` is the early, native-only fatal layer. It follows `StackTrace` and `BaseLogging`, suspends asynchronous writes, emits one synchronous message plus native trace, and then delegates only the mechanical process termination to `BasicCore::ExitApp(false)`. It owns `fatal::report_and_exit`, `fatal::report_strong_assert_and_exit`, and `FO_BASIC_STRONG_ASSERT`; it deliberately does not construct exception objects or depend on the later `ExceptionHandling` module. `exit_app(false)` itself remains status-only because its callers include both controlled command failures and fatal invariant failures.

`exit_app` never returns: desktop Windows/Linux use `std::quick_exit`, while web, Apple and Android use `std::exit`. The selected CRT function owns termination and its registered cleanup callbacks; neither path unwinds automatic local objects. There is no fallback work after either terminal call. `BuildTools/tests/test_process_termination.py` compiles the exact engine declaration and body with unreachable-code diagnostics enabled, then checks success/failure status, the selected CRT callbacks and destructor behavior in separate processes.

`StackTrace.*` captures and formats native/script stack information, including a capped global cache for resolved native frames, while `ExceptionHandling.*` owns the later exception-object reporting helpers. For debugger-facing workflows, use [Debugging.md](Debugging.md).

### Memory, pointers, and lifetime utilities

`MemorySystem.*` owns backup-memory chunks, bad-allocation reporting, and `safe_allocator`. The backup chunks live in the module's global data: once the set is torn down there is no reserve left to release, and the out-of-memory path reports and exits without one. `SmartPointers.*` contains pointer wrappers used to make ownership, nullability, and raw-reference intent explicit; see [SmartPointers.md](SmartPointers.md) for the native `ptr` / `nptr` vocabulary and migration rules. Use this layer for generic ownership utilities only; entity lifetime and holder semantics belong in [EntityModel.md](EntityModel.md).

#### Callable vocabulary

`FunctionObjects.*` owns the engine callable wrappers, which replace `std::function` throughout the engine. The two names carry the standard-library contracts of `std::move_only_function` and `std::copyable_function`, so a reader who knows those knows these:

- **`move_only_function<Signature>`** — the target belongs to exactly one wrapper, so a closure may capture `unique_ptr`, `refcount_ptr`, or any other move-only owner.
- **`copyable_function<Signature>`** — copying duplicates the target rather than sharing it, so the two wrappers stay independent; the target must be copy-constructible. Use it only where a stored callable is genuinely handed to more than one owner, such as a descriptor a runtime hands out repeatedly.
- **`function<Signature>`** — alias of `move_only_function`. This is the default: reach for `copyable_function` only when a copy is actually required.

A target of at most `FUNCTION_INLINE_TARGET_SIZE` bytes that is nothrow-move-constructible lives inside the wrapper; anything larger, over-aligned, or throwing-move goes to the heap. That covers a closure capturing up to six pointers or holding one `string` by value, which is nearly every engine callback, so the common case allocates nothing and the wrapper stays one cache line wide on a 64-bit target. `is_heap_allocated()` reports which path a wrapper took and is what the module's tests assert against. A throwing move is pushed to the heap on purpose: moving the wrapper is `noexcept`, and only a pointer steal can guarantee that.

The 48-byte storage union obtains fundamental alignment from an inactive
`std::max_align_t` member. Its pointer and byte-buffer offsets remain zero;
using natural alignment avoids MSVC's Win32 diagnostic for an explicitly
aligned union member without changing the inline budget or heap boundary.

Function references bind directly as nonempty targets. Null function pointers and null member pointers create empty wrappers; both move-only and copyable wrappers preserve that distinction.

A `copyable_function` narrows to `move_only_function` by adopting or copying its target in place, never by wrapping it in a second indirection. The reverse conversion does not exist — a move-only target cannot become copyable.

Calling an empty wrapper is a defect, not a recoverable condition: it hits `FO_BASIC_STRONG_ASSERT` instead of throwing `std::bad_function_call`. Check with `operator bool` where absence is legitimate. The module sits above `SmartPointers` and `MemorySystem` in the include order, so its heap tier uses the globally replaced `operator new` directly and exits through `fatal::report_and_exit` on exhaustion rather than through `safe_alloc`.

#### String vocabulary

`StringObject.*` owns `basic_string<CharT, InlineCapacity>`, the engine string that `Containers.h` aliases as `string` and `wstring`. It behaves as `std::basic_string` — same constructors, same member and free operators, same `constexpr` support for targets that stay inline, same `npos`/`max_size`/`out_of_range`/`length_error` contracts — with one difference: the small-string buffer is a template parameter instead of a fixed property of the standard library.

- **`FO_STRING_INLINE_CAPACITY`** is the build option that sets it, defaulting to 31 (a 48-byte object) and reaching the code as `STRING_INLINE_CAPACITY` through `EngineConfig.gen.h`. Only 7, 15, 23, 31, 39, 47, ... are worth setting — one less than a multiple of 8. The object rounds the inline array up to the pointer size, so 19 produces the same 40-byte object as 23 and 27 the same 48-byte one as 31, holding fewer characters for the same memory. `WSTRING_INLINE_CAPACITY` derives from it so a wide string costs the same bytes rather than the same characters. Codegen tracks its argument file as an input, so reconfiguring with a new capacity regenerates the header on the next build, as with every other `-enginedefine` value.
- The object is a union of the inline array and the heap pointer plus the `size` and `capacity` words. `capacity == InlineCapacity` is the discriminator, so heap growth never lands on that value and no flag or pointer tagging is needed; `is_inlined()` reports which tier a string is on.
- Growth is geometric and rounds the whole buffer, terminator included, up to the allocator bucket, so a block's tail is spent on characters rather than padding.
- Allocation goes through `safe_allocator`, so the string carries the same terminate-on-OOM contract as every other engine container.

The 31 default is not a guess: it is the value a peak-size census produced in an embedding project (Last Frontier), measured over roughly 30 million destroyed strings on its performance scenes. The client is what the choice serves — 88-94% of its strings stay inside the object against 71-82% at the 15 a standard library typically ships, and its string allocations drop by about 60% — while a 48-byte object still fits inside a cache line. Servers tend to look different: that project's server barely moved (64% to 69%) because its distribution is bimodal, with a quarter of strings past 66 characters and a long tail beyond 1024 that no sane capacity inlines. Re-measure per project rather than assuming these numbers transfer.

Two lookup rules are load-bearing and easy to break. `operator<<` lives in the engine namespace because Catch2 is included before the engine headers and can only reach it through ADL. `operator>>` lives in the global namespace instead, beside the `FO_DECLARE_TYPE_PARSER` operators: an `operator>>` inside the engine namespace would hide every one of those from unqualified lookup in engine code. `getline` is an engine-namespace overload, so calls must be unqualified — `std::getline` cannot find it.

The standard string streams are specified on `std::basic_string`, so they keep the `stream_string` alias and text handed to one is copied through `make_stream_string`. `std::filesystem::path` likewise only recognises the standard string shapes; build a path from an engine string with `fs::make_path`, which is the correct engine idiom anyway because it carries UTF-8 rather than the native narrow encoding.

#### Deque vocabulary

`DequeObject.*` owns `basic_deque<T, BlockBytes>`, which `Containers.h` aliases as `deque`. It exists because `std::deque` fixes its block at 16 bytes: for any element wider than a pointer that is one heap block per element, which a message or task queue pays on every push. Measured on the toolchain this engine targets, a thousand `push_back` calls of a 64-byte element cost 1008 allocations from the standard container.

- The block size is the template parameter, defaulting to `DEQUE_BLOCK_BYTES` (512 bytes of elements) and never dropping below `DEQUE_MIN_BLOCK_ELEMENTS`, so a wide element still amortises its allocations. Override it per use site as with `small_vector<T, N>`; there is no build option, because unlike the string's inline capacity a block size has no single global optimum.
- Storage is a growable array of fixed blocks plus the index of the first element, so `push_back` and `push_front` never move an existing element. That matches `std::deque`'s reference-stability guarantee, which engine queues lean on.
- The surface is the subset the engine uses — the push/pop/emplace pairs at both ends, `front`/`back`, indexing, `size`/`empty`/`clear`/`swap`, forward and reverse iterators, and single-element `erase`. `erase` shifts toward the nearer end and invalidates from there, as the standard container does. A missing operation is a compile error, so add it when a caller needs it.

#### Random vocabulary

`RandomGenerator.*` owns `random_generator`, a xoshiro256++ engine that replaces `std::mt19937` everywhere in the engine. Two reasons, and the second is the important one:

- **State.** 32 bytes against the 5000 the standard Mersenne engine occupies on this toolchain, and no seeding pass — that engine costs about 3.6 us to construct.
- **Reproducibility.** `std::uniform_int_distribution` is implementation-defined, so the same seed maps to different values on a Windows client and a Linux server, and across standard-library versions. `next_below(bound)` and `next_between(min, max)` are ours, using Lemire's multiply-shift, so a seed produces one sequence everywhere. `Test_RandomGenerator` pins that sequence against fixed values.

A default-constructed generator seeds itself from `std::random_device`; the `uint64_t` constructor and `seed()` expand a caller's seed through SplitMix64, so a zero seed is not the state's fixed point. `next()` returns a raw 64-bit draw, `next_normalized()` a double in `[0, 1)`. The bounded draws validate their arguments with `FO_VERIFY_AND_THROW` rather than returning a wrong answer.

#### Allocation vocabulary

Engine code allocates through one of two surfaces, and nothing else:

- **The `fo` container aliases** from `Containers.h` — `string`, `wstring`, `vector`, `map`, `unordered_map`, `set`, `list`, `deque`, `stringstream`, `small_vector` and friends. Use these, never the `std::` originals. `string` and `wstring` are the engine `basic_string` and `deque` the engine `basic_deque`, both described above; the rest are the standard containers instantiated on `safe_allocator`.
- **`safe_alloc`** — `make_unique` / `make_shared` / `make_refcounted` / `make_raw_arr` / `make_unique_arr` for typed objects, and the raw tier `malloc_raw` / `calloc_raw` / `realloc_raw` / `free_raw` plus `malloc_aligned_raw` / `free_aligned_raw` for C-ABI boundaries.

The raw tier exists because third-party allocator hooks are C-shaped: they demand `realloc`, or an untyped byte block, or both, which a C++ allocator cannot express. It carries the same out-of-memory policy as `safe_allocator` — report, drain the backup pool, retry, then exit deterministically — so wiring a library through it does not silently opt that library out of the contract. A zero-size request is passed through rather than treated as failure.

The underlying `rpmalloc` primitives are deliberately **not** exported from `MemorySystem.h`. They return null on failure and would be a second, equally reachable entry point that skips the contract; they live as file-local statics in `MemorySystem.cpp`. The vendored allocator propagates an on-demand page-commit failure as null too, so the same report, backup-pool retry, and controlled termination apply when Windows exhausts its commit limit after reserving a span. The `memory::copy` / `memory::move` / `memory::fill` / `memory::compare` / `memory::read_unaligned` / `memory::write_unaligned` block operations are unrelated to allocation and remain public.

The vendored rpmalloc keeps its upstream 256 MiB spans on 64-bit targets. On 32-bit targets it uses one `LARGE_PAGE_SIZE` (16 MiB) per span: pre-`VirtualAlloc2` Windows has to reserve `size + alignment`, so an upstream 256 MiB aligned span can require a contiguous 512 MiB reservation inside the process's 2 GiB address space and make even the first tiny allocation fail. The smaller x86 span preserves every built-in page class while avoiding that startup dependency on a single huge address-space hole.

Three distinct things are at stake when code bypasses this vocabulary, and they are not equally severe:

| | What actually happens |
|---|---|
| **Separate heap** | Global `operator new`/`delete` are replaced with rpmalloc, so every `new` and every `std::allocator` already lands in the engine heap. But rpmalloc is built with `ENABLE_OVERRIDE=0`, so C `malloc`/`free` is **not** intercepted — anything allocating through it lives in the CRT heap, outside rpmalloc, invisible to `memory::get_in_use_bytes()` and to Tracy allocation tracking. |
| **Wrong out-of-memory policy** | `std::allocator` throws `std::bad_alloc` instead of following the terminate-on-OOM model in [ExceptionSafety.md](ExceptionSafety.md) §1. |
| **Alignment** | `safe_allocator` routes over-aligned element types through the aligned `operator new`/`delete` overloads. Note that the over-alignment test must stay a member *function*: `alignof(T)` needs a complete `T`, while the allocator has to remain usable with an incomplete one, since `std::vector<T>` may be declared before `T` is defined. |

Known and accepted limits: `std::future`/`std::promise`/`std::packaged_task`, `std::thread`, `std::filesystem::path` and the file streams have no allocator parameter at all, so they reach the engine heap through global `new` but throw on exhaustion. `std::function` is no longer among them — engine code uses `move_only_function` / `copyable_function`, whose heap tier terminates like the rest of the engine; the one remaining `std::function` is the `StackTrace.h` script-provider hook, which sits above the callable module in the include order. Separately, `BasicCore`, `StackTrace` and `BaseLogging` sit above `MemorySystem` in the `Essentials.h` include order and therefore use `std::` containers by design — `MemorySystem.cpp` calls `stack_trace::get()` from `memory::report_bad_alloc`, so the reporting path must not depend on the allocator that just failed.

#### Third-party allocators

| Library | Routed to | Where |
|---|---|---|
| ImGui | `safe_allocator` | `Common/ImGuiExt/ImGuiStuff.cpp` |
| AngelScript | `safe_allocator` | `Scripting/AngelScript/AngelScriptScripting.cpp` |
| zlib | `safe_allocator` | `Essentials/Compressor.cpp` |
| ozz-animation | `safe_alloc` aligned tier | `Common/ModelAnimationData.cpp` |
| meshoptimizer | `safe_allocator` | `Tools/ModelMeshBaker.cpp` |
| ufbx | `safe_allocator` | compile-time `UFBX_EXTERNAL_MALLOC` plus `extern "C" ufbx_malloc/realloc/free` in `Tools/ModelMeshBaker.cpp` |
| SDL | `safe_alloc::*Raw` | `Frontend/Application.cpp` |
| Effekseer | `safe_alloc::*Raw` + aligned | `Client/EffekseerExtension.cpp`, declared in its header; both owners (client runtime and `Tools/ParticleBaker.cpp`) install through that one definition |
| libpng | `safe_alloc::*Raw` | `Tools/ImageBaker.cpp`, via `png_create_read_struct_2` |
| libbson / mongo-c | `safe_alloc::*Raw` + aligned | shared `Server/DataBase.cpp`; every BSON-backed factory (JSON, SQLite, Mongo) installs the process-global vtable before constructing its backend |
| SQLite | `safe_alloc::*Raw` | `Server/DataBase-SQLite.cpp`, via `sqlite3_config(SQLITE_CONFIG_MALLOC)` before `sqlite3_initialize()` |
| Mono | `safe_alloc::*Raw` | `Scripting/Managed/ManagedScriptBackend.cpp`, via `mono_set_allocator_vtable` before any other Mono call |

The bson vtable is worth reading before copying its shape elsewhere: it supplies `aligned_alloc` but releases those blocks through the plain `free` member, never recording the alignment. That is sound only while both paths end in the same release function. Under rpmalloc they do (`rpaligned_alloc` and `rpmalloc` both end in `rpfree`), and so do they on POSIX without it (`posix_memalign` blocks are `free()`-able by definition). The one combination that breaks is Windows without rpmalloc — the sanitizer configs, where `expr_RpmallocEnabled` turns the allocator off so the sanitizer can interpose — because there the aligned path is `_aligned_malloc`/`_aligned_free`. `BsonAlignedAlloc` therefore falls back to plain `safe_alloc::malloc_raw` in exactly that case, which is what bson's own default vtable does on MSVC and for the same stated reason (`_aligned_alloc_impl` in libbson `memory.c` deliberately does not use `_aligned_malloc`); every aligned request in mongoc is a `BSON_ALIGNOF` of an ordinary C struct, so malloc's fundamental alignment covers them. The vtable is process-global, so every BSON-backed factory installs the same callbacks before its backend can allocate; changing it later could pair an old allocation with a new free callback. Dropping `aligned_alloc` from the vtable is not an alternative — bson then substitutes an internal fallback that discards the requested alignment on every platform, not just the one that needs it.

SQLite's hook needs an `xSize` callback and hands the free/realloc/size functions only a pointer, so each block carries an 8-byte size header. Its configuration must also be installed *before* `sqlite3_initialize`, which is why the library is built with `SQLITE_OMIT_AUTOINIT` and every caller goes through one exported initializer.

Mono's public hook is `mono_set_allocator_vtable`. It redirects eglib (`g_malloc` / `g_realloc` / `g_calloc` / `g_free`: metadata, hashtables, runtime strings) onto `safe_alloc`'s raw tier. The runtime is configured with `ENABLE_OVERRIDABLE_ALLOCATORS`; without that flag the setter is an empty function that still returns TRUE, so the host reads eglib's `g_mem_get_vtable` after install and rejects a no-op. The GC heap (SGen nursery, major, LOS) and executable code pages are mapped with `mono_valloc` and stay there: they need protection changes, decommit, and executable pages, which a general-purpose heap cannot provide.

Not hooked, with reasons: **LibreSSL** exports `CRYPTO_set_mem_functions` but its body is an inert `return 0;` — custom allocators were removed upstream, so calling it would be dead code that reads like coverage. **ogg / vorbis / theora** expose no allocator hook.

When vendoring or updating a library, check whether it has an allocator hook and either wire it or record why not — and read the hook's *implementation*, not just its declaration. Two of the entries above were initially misjudged from the call site or the symbol name alone.

### Serialization, values, strings, and hashes

`DataSerialization.*` contains binary read/write helpers used by network, persistence, resources, and tests. `data_reader::read<T>()` and `data_writer::write<T>()` copy standard-layout values through byte copies so serialized streams do not depend on buffer alignment. The zero-copy `read_ptr<T>(size)` overload is only for raw byte/string views (`uint8_t`, `char`, or `void`); typed values that need alignment must use `read<T>()` or `read_ptr(destination, size)`. `StringUtils.*`, `HashedString.*`, `StrongType.*`, `ExtendedTypes.*`, `SafeArithmetics.*`, and `TimeRelated.*` provide the small reusable values that higher layers treat as primitives. `iround` rejects non-finite and out-of-int64-range floating-point input before rounding so no value undefined for `std::llround` can reach it. `hash_storage::set_resolve_hash_failure_handler` lets higher layers observe failed hash resolution in both throwing and flagged no-throw lookup paths without teaching essentials about a specific recovery policy.

Duration formatting retains 64-bit hour and day counts and keeps the existing minute/day display boundaries. `Test_TimeRelated` covers these boundaries and the largest nanosecond duration.

### Filesystem, compression, sockets, and work threads

`DiskFileSystem.*` owns low-level disk access. `fs::disk_read_file` retains a descriptor and captured length;
`read_at` performs exact positional reads within that bound. Its `(path, offset, size)` constructor restricts
reads to a region of a backing file, used for uncompressed APK assets. Moving or closing a reader while a read
is in flight is invalid. `fs::disk_write_file` either creates/truncates or appends, acquires writer exclusion before
mutation, and supports truncating an uncommitted tail. Writes own one cursor; `flush` uses `fsync`/`_commit`.
`fs::disk_directory_lock` serializes resource mutations without a stored lock file (POSIX directory flock,
Windows named mutex). POSIX readers can retain old inodes across replacement; Windows sharing can reject
replacement/deletion while readers are open. `fs::sync_parent` persists POSIX directory entries;
`fs::rename_durable` uses that ordering on POSIX and write-through MoveFileEx on Windows.

`fs::available_space` supports admission without preallocating a resumable download. `fs::is_contained_relative_path`
rejects empty/rooted paths or `..`; `fs::list_dir_file_names` includes names hidden by the ordinary iteration
filter, including updater temporaries. `fs::make_writable_path` layers relative paths under a user root while
leaving absolute inputs unchanged; resource-specific installed-root selection belongs to `FileSystem.h`.
The mounted view is documented in [ConfigurationAndDataSources.md](ConfigurationAndDataSources.md).
`Compressor.*` owns compression, `NetSockets.*` owns low-level sockets, and `WorkThread.*` owns background workers.


TCP and UDP transfer calls retain signed 32-bit byte counts. The requested buffer size is checked before the OS call; a successful result cannot exceed that size. Checked return conversion preserves `-1` errors (including would-block), zero-length results and TCP end-of-stream. `Test_NetSockets` exercises these outcomes over real loopback sockets.

Windows disk I/O resolves ordinary paths through the native full-path operation before adding the extended namespace at the Win32 directory-length boundary. Reads, writes, metadata queries, renames, removal and directory iteration share this conversion, including relative paths beneath a long current directory. Recursive removal adds the namespace even for a short root: the standard library constructs descendant paths internally, and those descendants can cross the length boundary. Ordinary drive and UNC paths retain Windows dot/space and separator normalization; already extended or device paths remain literal inputs. Resolution failures follow each API's existing error-result contract. Lexical helpers (`fs::make_path`, `fs::path_to_string`, `fs::resolve_path`, and writable-path policy) do not add the I/O prefix, and directory visitors still receive relative resource names. `Test_DiskFileSystem.cpp` exercises Unicode paths beyond 320 native characters, removal of their remaining directory tree and Windows ordinary-versus-literal trailing-name behavior.

For a native Windows path-length investigation, run
`python BuildTools/probe_windows_file_io.py --output Workspace/file-io.json`.
The diagnostic builds small MSVC programs with static and dynamic CRTs, records
ordinary absolute, relative and extended-path `ifstream`/directory-enumeration
results at 259, 260, 262 and 320 UTF-16 units, and checks file contents against
a short-path control. It retains executables, compiler logs and available
embedded manifests beside the JSON report. `--unc-root` optionally tests an
existing writable share; otherwise UNC I/O is explicitly untested. This probes
the native library boundary without changing the engine's lexical path APIs or
requiring a game build. The same run compiles the exact path-conversion and
create/write/open/rename/remove functions extracted from `DiskFileSystem.cpp`,
checks long Unicode file operations and recursive removal beneath a short root,
records the unprefixed `remove_all` control separately, and distinguishes ordinary trailing
dot/space normalization from literal extended names. Its manifest records each
function hash and the harness aliases; full engine linkage and directory-visitor
dispatch remain covered by the native unit suite. UNC conversion is checked
lexically even when no writable share was supplied.

When a `work_thread` job throws, the thread runs its local exception handler first so it can update worker-owned policy such as clearing queued jobs; the original exception is then reported through the global non-fatal exception reporter outside the worker lock.

#### Waiting

`Threading.h` exposes `coarse_sleep` and `precise_sleep`; engine code uses those and never `std::this_thread::sleep_for`. The standard call rounds the request up to the OS timer tick, so on a default Windows configuration a 50 us request parks for **15.4 ms** — three hundred times over, which silently defeats every sub-millisecond back-off and every frame pacer. Nothing in the engine raises the process timer resolution, so this is the shipped behaviour, not a misconfiguration.

- **`coarse_sleep`** parks on a high-resolution waitable timer and lands within about half a millisecond, spending no CPU. It is the right call for a polling loop that merely wants to yield the core for a while.
- **`precise_sleep`** hits its deadline to within microseconds by spinning the last millisecond out on `yield()`. Use it where the number was chosen on purpose — lock back-off, frame pacing — and remember it burns at most one spin budget of CPU per call.

A wait at or below the spin budget is spun in full, because the timer cannot be asked for less than roughly 600 us. Above it, the timer takes `duration - budget` and the tail is spun; the timer overshoots its own deadline by 0.3-0.5 ms, so in practice the tail is short or empty. The timer handle is created per call, which measured at 4.5 us and only applies to waits over a millisecond, so no thread-local handle cache is needed. `Test_Threading` pins that a sub-millisecond request returns in well under a millisecond, which the tick-rounded call cannot do.

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

See [Testing.md](Testing.md) for the complete test-suite map and target wiring.

## Change routing

- Compiler/OS gates, namespace, base aliases, and raw process termination: `Source/Essentials/BasicCore.*`.
- Global create/delete callback registration: `Source/Essentials/GlobalData.*`.
- Stack traces, early fatal reporting, logging, and exception reporting: `Source/Essentials/StackTrace.*`, `BaseLogging.*`, `FatalError.*`, `Logging.*`, `ExceptionHandling.*`, and [Debugging.md](Debugging.md).
- Generic memory/pointer utilities: `Source/Essentials/MemorySystem.*`, `SmartPointers.*`, and [SmartPointers.md](SmartPointers.md).
- Callable wrappers and their inline-storage budget: `Source/Essentials/FunctionObjects.*`.
- The string type and its inline-capacity budget: `Source/Essentials/StringObject.*`; the aliases and stream interop: `Containers.h`.
- File bytes and low-level writable-path composition on disk: `Source/Essentials/DiskFileSystem.*`; mounted engine resources and installed-client overlays: [ConfigurationAndDataSources.md](ConfigurationAndDataSources.md).
- Socket primitives: `Source/Essentials/NetSockets.*`; protocol/command/network runtime: [Networking.md](Networking.md).

## Validation checklist

1. Confirm the change does not introduce either an include-time or link-time dependency from an Essentials module to a later or higher engine layer; run `python -m pytest -q BuildTools/tests/test_essentials_layering.py`.
2. Update `BuildTools/cmake/stages/EngineSources.cmake` when adding/removing essentials files.
3. Run the smallest matching essentials test and then the broader `RunUnitTests` target when behavior crosses utility boundaries.
4. For diagnostics changes, also verify [Debugging.md](Debugging.md) stays accurate.
5. For filesystem/socket/threading changes, validate at least one higher-level consumer if the low-level contract changed.

`fs::iterate_dir` treats a missing directory as an empty source. Other path lookup and traversal errors propagate to the caller; it never reports an inaccessible or malformed path as a complete empty directory.
