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
- `Source/Essentials/TextTypes.h`
- `Source/Essentials/TextTypes.cpp`
- `Source/Essentials/TextConversions.h`
- `Source/Essentials/TextConversions.cpp`
- `Source/Essentials/Containers.h`
- `Source/Essentials/Containers.cpp`
- `Source/Essentials/TextFormatting.h`
- `Source/Essentials/TextFormatting.cpp`
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
6. `TextTypes.h`, `TextConversions.h`, `Containers.h`, `TextFormatting.h`, `StringUtils.h`, `CommonHelpers.h` — strict text types, boundary conversions, formatting, and reusable container/string/utility helpers. The critical dependency chain is `MemorySystem.h` -> `TextTypes.h` -> `TextConversions.h` -> `Containers.h` -> `TextFormatting.h` -> `StringUtils.h`; conversions remain below the char-based container/string layer, while strict formatting may use the shared container aliases without depending on unbranded string algorithms.
7. `SafeArithmetics.h`, `DataSerialization.h`, `HashedString.h`, `StrongType.h`, `TimeRelated.h`, `ExtendedTypes.h`, `Compressor.h` — value, serialization, hashing, time, compression, and type helpers.
8. `WorkThread.h`, `DiskFileSystem.h`, `NetSockets.h`, `Platform.h` — the `WorkThread` job runner, disk access, socket, and host OS abstractions.

Embedding-project modules consume this surface through `Source/Common/Common.h`, which includes `Essentials.h`; they must not include individual Essentials headers such as `TextConversions.h`, `TextFormatting.h`, `Logging.h`, or `Platform.h`. Higher engine layers likewise use the umbrella-provided text conversion and formatting surface instead of adding direct `TextConversions.h` / `TextFormatting.h` dependencies. Granular includes are reserved for implementation and dependency ordering inside `Source/Essentials/` itself.

Keep new essentials APIs free of dependencies on `Source/Common/`, `Source/Client/`, `Source/Server/`, `Source/Tools/`, or embedding-project headers.

## Subsystem map

### Platform and compiler gate

`BasicCore.h` enforces the selected OS macro (`FO_WINDOWS`, `FO_LINUX`, `FO_MAC`, `FO_ANDROID`, `FO_IOS`, or `FO_WEB`) and requires C++20. It also binds frequently used standard types into the engine namespace and declares core macros such as `FO_EXPORT_FUNC`, `FO_KEEP_DATA_SYMBOL`, and namespace helpers. Warning-suppression helpers also live here: `FO_DISABLE_WARNINGS_PUSH/POP` silence all warnings (for wrapping third-party header includes), while the per-compiler `FO_GCC_IGNORE_WARNINGS_PUSH/POP`, `FO_CLANG_IGNORE_WARNINGS_PUSH/POP`, and `FO_MSVC_IGNORE_WARNINGS_PUSH/POP` silence one named diagnostic and are active only on their matching compiler (so a single-toolchain false positive can be suppressed at one site without other toolchains rejecting an unknown `-W` name or warning number). Prefer fixing warnings at their root; reach for the per-compiler helpers only for documented compiler false positives.

`Platform.h` / `.cpp` owns host-specific helpers that are deliberately small: informational logging, thread names, executable path lookup, strict environment lookup, per-user data directory lookup, process id formatting, fork support where available, process memory usage, CPU usage snapshots, and dynamic module loading. `Platform::GetEnvironmentUtf8()` accepts only a terminated ASCII variable name and returns an optional strict UTF-8 value: Windows reads the wide environment through `GetEnvironmentVariableW` and converts checked UTF-16, while other platforms validate the exact bytes returned by `getenv`; missing and empty values return `nullopt`. `Platform::GetUserDataBase()` builds on that API and remains shell/SDL-free: Windows reads `%LOCALAPPDATA%` (else `%APPDATA%`), macOS/iOS use `$HOME/Library/Application Support`, and Linux/Android/other use `$XDG_DATA_HOME` (else `$HOME/.local/share`). Malformed environment text throws before it reaches settings, filesystem, secure-storage, or diagnostic consumers. Higher layers append the application name and decide whether absence is fatal. `Platform::GetCpuUsageSnapshot()` returns cumulative per-core system counters plus the current process CPU time; callers compare two snapshots to compute percentages and keep any sampling/cache state outside the Platform layer. Platform-specific application/window/rendering behavior lives under `Source/Frontend/`, not here.

`Platform::GetExePath()` also returns strict UTF-8. Windows obtains a bounded wide path through `GetModuleFileNameW` and converts validated UTF-16; Linux grows the `readlink("/proc/self/exe")` buffer until the complete non-terminated byte sequence fits; macOS validates exactly the path length returned by `proc_pidpath`. Malformed POSIX path bytes throw instead of entering downstream path utilities. `strex` / `strvex` accept the strict UTF-8 view directly and keep their character representation private, while filesystem consumers pass the native `char8_t` view through `fs_make_path()` or directly to `std::filesystem` in boundary tests.

Platform diagnostic output and OS thread descriptions are null-terminated UTF-8 boundaries: `Platform::InfoLog()` and `Platform::SetThreadName()` accept only `u8string_view_nt`, revalidate the complete backing storage immediately before the platform branch, and reject malformed UTF-8, embedded zeros, or a stale terminator even on platforms where the helper is otherwise a no-op. Windows converts through strict UTF-16 before `OutputDebugStringW` or `SetThreadDescription`; Android logging receives the checked UTF-8 C string. The staged log builder and thread-name storage cross this boundary through `utf8_from_char_span()`. The noexcept shared thread-name helper catches validation/platform failures, and Tracy receives the same checked null-terminated UTF-8 buffer rather than the original `char*`.

Dynamic-library symbol names are null-terminated technical identifiers, not display text: `Platform::GetFuncAddr()` therefore accepts `string_view_nt`. The ordinary narrow-string contract already classifies the identifier as ASCII; dynamically assembled storage crosses through `string_view_nt_from_span()`, which checks the bounded storage for a final terminator and embedded zeros before `GetProcAddress` or `dlsym`.

Dynamic-library paths are Unicode text and `Platform::LoadModule()` accepts only `u8string_view_nt`. It copies a freshly revalidated path before appending the platform extension, rejecting malformed UTF-8, embedded zeros, and a stale/missing terminator before any OS call. Windows converts that strict UTF-8 path through the portable UTF-16 adapter and calls `LoadLibraryW`; Linux and macOS pass the exact validated UTF-8 bytes to `dlopen`. Requested runtime paths and command-line arguments stay strict UTF-8 until the final terminated platform or client-runtime ABI handoff, as documented in [ConfigurationAndDataSources.md](ConfigurationAndDataSources.md).

Windows builds retain the `_WIN32_WINNT=0x0601` compile baseline. One Windows build-platform registry owns the CMake architecture, toolset, and canonical packaging architecture for the regular, `-clang`, and `-win7` variants. The Win7 pair pins MSVC 14.44, while `FO_BINARY_OUTPUT_POSTFIX` remains independent of the platform. In the package DSL the corresponding `BINARY` entry can select its own postfix, for example `BINARY Client Windows win32-win7 Raw+Zip+Wix POSTFIX Win7`, without affecting sibling binaries in the package. Compatibility checks are kept outside application targets.

### Diagnostics and failure handling

`BaseLogging.*` and `Logging.*` provide the logging foundation. The upper layer carries invariant text: `WriteLogMessage()`, `WriteBaseLog()`, and `LogFunc` use `u8string_view`, callback registration keys use `string_view`, and the repeat cache/result builder own `u8string`. `WriteBaseLog()` also accepts a `const u8string&`, so an ASCII `string` or a `strex` result promotes once to the UTF-8 owner without caller-side adapters. `WriteLog()` accepts UTF-8 owners/views directly as raw messages and formatted arguments; callers must not unwrap them with `utf8_as_char_view()`. ASCII and UTF-8 format literals are checked at compile time, formatting produces a checked `u8string`, and only the private standard-library formatter adapter sees a `char` view. Ordinary narrow `string`, `string_view`, `std::string`, bounded `char` arrays, and non-null `const char*` formatted arguments are treated as UTF-8 candidates and validated as part of the complete formatted result, so native exception text is logged directly as `WriteLog("... {}", ex.what())` without a temporary `u8string`. `strex` remains the ASCII formatting proxy, while `u8strex` owns UTF-8 formatting and accepts both ASCII and UTF-8 inputs without caller-side adapters; neither proxy narrows UTF-8 implicitly. Wide and raw `char8_t` text arguments are rejected. `hstring` is an ASCII symbolic identity: it is accepted by both ASCII and UTF-8 formatting and is promoted only when the destination is UTF-8. Both borrowed message entrypoints revalidate their UTF-8 input before dispatch. `WriteLogMessage()` also collapses immediate duplicates by `LogType` and message text: repeated copies are skipped, and the next different log line first emits a summary such as `...and 25 more same messages`. Because logging is `noexcept`, malformed formatted bytes are replaced by the explicit diagnostic `Log message rejected: invalid UTF-8 or formatting arguments` instead of escaping or reaching callbacks. Engine exceptions accept narrow ASCII and strict UTF-8 context arguments directly, and `ExceptionCallback` carries `u8string_view`; callers must not unwrap paths, values, or messages before `throw` / `FO_VERIFY_*`. `BaseEngineException` retains a checked UTF-8 representation next to the mandatory `std::exception::what()` character ABI, while `exception_message_utf8()` validates arbitrary native exceptions and substitutes an explicit UTF-8 diagnostic for malformed external text. Crash handlers, native stack resolvers, and SDK callbacks may produce arbitrary diagnostic bytes, so the dependency-bottom `WriteBaseLogBytes(const_span<byte>)` sink preserves them exactly, including malformed UTF-8 and embedded zeros; its asynchronous queue owns byte buffers instead of strings. `LogToFile()` belongs to the logging modules and accepts either an ASCII `string_view` path or a strict UTF-8 `u8string_view` path. Owners and formatting proxies use their existing implicit view conversion, so calls such as `LogToFile(strex("{}_BakerLib.log", FO_DEV_NAME))` need no intermediate variables or character-view bridge. Frontend `GetExeLogFileName()` returns an owning `u8string`, so Unicode executable names and writable directories do not pass through a locale-dependent narrow path on Windows. `LogToFile()` opens the file without an exclusive lock (the platform default: MSVC `std::ofstream` opens deny-none, POSIX has no mandatory open lock) so two engine modules in one process — e.g. a runtime host EXE and the runtime DLL it loads, each with its own copy of the engine global data — can both hold the same file open at once, and every write seeks to end of file first (`WriteSync`) so neither handle overwrites content the other appended; the `append` parameter still selects truncate (default) vs append for the initial open. `WriteLog`/`WriteBaseLog` degrade safely when their global data is not yet created (falling back through the byte sink, then to `std::cout`), and a runtime host can open the log early as `LogToFile(GetExeLogFileName())` after `CreateGlobalData()`, so its pre-`InitApp` diagnostics reach the file. When `AsyncLogWrite` is enabled, the fatal-error handler (`ExceptionHandling`) calls `SuspendAsyncLogWriting()` first, which flips writes back to the synchronous path without joining the worker, so the crash reason and stack trace are flushed inline before the process exits instead of being lost in an undrained async queue. `StackTrace.*` captures and formats native/script stack information, including a capped global cache for resolved native frames, while `ExceptionHandling.*` owns exception-reporting helpers. For debugger-facing workflows, use [Debugging.md](Debugging.md).

### Memory, pointers, and lifetime utilities

`MemorySystem.*` owns backup-memory chunks, bad-allocation reporting, and `SafeAllocator`. `SmartPointers.*` contains pointer wrappers used to make ownership, nullability, and raw-reference intent explicit; see [SmartPointers.md](SmartPointers.md) for the native `ptr` / `nptr` vocabulary and migration rules. Use this layer for generic ownership utilities only; entity lifetime and holder semantics belong in [EntityModel.md](EntityModel.md).

#### Allocation vocabulary

Engine code allocates through one of two surfaces, and nothing else:

- **The `fo` container aliases** from `Containers.h` — `string`, `wstring`, `vector`, `map`, `unordered_map`, `set`, `list`, `deque`, `stringstream`, `small_vector` and friends. Each is the standard container instantiated on `SafeAllocator`. Use these, never the `std::` originals.
- **`SafeAlloc`** — `MakeUnique` / `MakeShared` / `MakeRefCounted` / `MakeRawArr` / `MakeUniqueArr` for typed objects, and the raw tier `MallocRaw` / `CallocRaw` / `ReallocRaw` / `FreeRaw` plus `MallocAlignedRaw` / `FreeAlignedRaw` for C-ABI boundaries.

The raw tier exists because third-party allocator hooks are C-shaped: they demand `realloc`, or an untyped byte block, or both, which a C++ allocator cannot express. It carries the same out-of-memory policy as `SafeAllocator` — report, drain the backup pool, retry, then exit deterministically — so wiring a library through it does not silently opt that library out of the contract. A zero-size request is passed through rather than treated as failure.

The underlying `rpmalloc` primitives are deliberately **not** exported from `MemorySystem.h`. They return null on failure and would be a second, equally reachable entry point that skips the contract; they live as file-local statics in `MemorySystem.cpp`. The `MemCopy` / `MemMove` / `MemFill` / `MemCompare` / `MemReadUnaligned` / `MemWriteUnaligned` block operations are unrelated to allocation and remain public.

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

The bson vtable is worth reading before copying its shape elsewhere: it supplies `aligned_alloc` but releases those blocks through the plain `free` member, never recording the alignment. That is sound only while both paths end in the same release function, which holds under rpmalloc and is pinned by a `static_assert(FO_HAVE_RPMALLOC, …)` in the shared database layer. The vtable is process-global, so every BSON-backed factory installs the same callbacks before its backend can allocate; changing it later could pair an old allocation with a new free callback. Dropping `aligned_alloc` is not a safer alternative — bson then substitutes an internal fallback that discards the requested alignment.

SQLite's hook needs an `xSize` callback and hands the free/realloc/size functions only a pointer, so each block carries an 8-byte size header. Its configuration must also be installed *before* `sqlite3_initialize`, which is why the library is built with `SQLITE_OMIT_AUTOINIT` and every caller goes through one exported initializer.

Not hooked, with reasons: **LibreSSL** exports `CRYPTO_set_mem_functions` but its body is an inert `return 0;` — custom allocators were removed upstream, so calling it would be dead code that reads like coverage. **ogg / vorbis / theora** expose no allocator hook.

When vendoring or updating a library, check whether it has an allocator hook and either wire it or record why not — and read the hook's *implementation*, not just its declaration. Two of the entries above were initially misjudged from the call site or the symbol name alone.

### Serialization, values, strings, and hashes

`BasicCore.h` imports the standard C++20 `std::byte` type as `byte` into the engine namespace; it does not define a project-specific alias. Arbitrary-data APIs use `byte`, `vector<byte>`, `span<byte>`, and `const_span<byte>`; `uint8_t` remains a numeric 0...255 value for scalar fields and arithmetic. `make_byte_span` creates an explicitly byte-oriented view over contiguous trivially-copyable storage while preserving constness. Range overloads accept lvalues and borrowed rvalues only, and pointer-like owners must be lvalues, so the helper cannot return a view into a destroyed owning temporary. `make_span(ptr<T>, count)` is exclusively a typed-element view; the former `make_span(void*/T*, byte_size)` and `make_const_span` integer-byte overloads were removed, so raw storage must cross through the explicitly named byte helper.

`DataSerialization.*` contains binary read/write helpers used by network, persistence, resources, and tests. Raw buffers are exclusively `byte`: span helpers accept `span<byte>` / `const_span<byte>`, `DataReader` and `MutableDataReader` borrow byte spans, and `DataWriter` appends to `vector<byte>`. `ReadPtr<T>()` permits only zero-copy byte/text views (`byte`, `char`, or `void`); numeric arrays and C/SDK buffers cross through an explicit byte view at their immediate boundary. Length-prefixed UTF-8 fields use `ReadUtf8StringView()` and `WriteStringBytes(u8string_view)`: the reader validates the borrowed bytes before branding the zero-copy view, while the writer preserves the exact encoded bytes. `DataReader::Read<T>()` and `DataWriter::Write<T>()` still serialize numeric fixed-width scalar values, including numeric `uint8_t`, by copying their object bytes, so the on-disk and wire layout is unchanged and does not depend on buffer alignment. Typed values that need alignment must continue to use `Read<T>()` or `ReadPtr(destination, size)` rather than dereferencing an unaligned byte view. `StringUtils.*`, `HashedString.*`, `StrongType.*`, `ExtendedTypes.*`, `SafeArithmetics.*`, and `TimeRelated.*` provide the small reusable values that higher layers treat as primitives. `StringUtils` also owns the shared ASCII URI-scheme grammar helpers: `parse_uri_scheme()` validates `ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )` and returns a non-owning view without allocating. `HashStorage` accepts only ASCII `string_view` identities, validates that invariant before insertion, and stores only ASCII `string` values. `hstring::as_str()` exposes that immutable stored identity as `const string&`; callers that need a stable pointer at a concrete ABI boundary take its address directly, and `hstring` has no separate string-pointer accessor. Logical resource paths that become `hstring`, schema names, prototype ids, enum names, and script function names therefore share the same ASCII boundary. `ResolveHash()` consults only identities actually stored in that `HashStorage`; there is no alias or migration fallback in the hash layer. `DefaultHash()` remains the low-level byte hash primitive used to preserve existing wire and persistence layouts. `iround` rejects non-finite and out-of-int64-range floating-point input before rounding so no value undefined for `std::llround` can reach it. `HashStorage::SetResolveHashFailureHandler` lets higher layers observe failed hash resolution in both throwing and flagged no-throw lookup paths without teaching essentials about a specific recovery policy.

`TextTypes.*` defines the text-domain boundary. Ordinary `string` / `string_view` are the engine's ASCII domain by contract; they intentionally remain normal mutable character strings rather than duplicating the standard string API in an ASCII wrapper. Code using these types is built on the assumption that only bytes 0x00...0x7F are present. `u8string` / `u8string_view` are the distinct UTF-8 domain and use `char8_t`; UTF-8 views and raw external input are validated before becoming strict text, and the owner may contain embedded zero code units. A narrow string promotes implicitly and losslessly to `u8string` by copying every byte as the same `char8_t` code unit without a redundant ASCII scan; satisfying the source domain contract is the caller's responsibility. Bounded ASCII literals assign to the UTF-8 owner directly. A live `u8string` lvalue also promotes implicitly to `u8string_view`, while conversion from an rvalue owner is deleted so a temporary cannot leave a dangling view. The reverse direction is explicit through `utf8_to_string()` and throws `TextValidationException` when the UTF-8 value contains a non-ASCII code unit. ASCII literals bind directly to `string_view` APIs and `u8` literals bind directly to `u8string_view` APIs; UTF-8 owners/views also compare directly with validated `u8` literals, including through generic test/framework operators. Caller-side `.view()`, `string_view {"..."}`, and `u8string_view {u8"..."}` wrappers add no boundary information when the destination already accepts the matching view type and must not be used. Explicit views remain appropriate when a runtime raw pointer or an exact bounded length is the boundary being expressed.

`BuildTools/text_type_audit.py` is the lexical source gate for literal classification. It rejects non-ASCII source characters in ordinary narrow string, raw-string, and character literals while ignoring comments and explicitly prefixed UTF-8/UTF-16/UTF-32/wide literals. It also rejects high-byte hexadecimal/octal escapes and non-ASCII universal escapes in ordinary narrow literals: valid text must originate from an explicitly prefixed literal, while intentional malformed/code-unit fixtures use `byte` storage and cross a legacy `char` codec only through a named bounded adapter. Direct C++ reinterpret, pointer `static_cast`, and C-style pointer casts targeting `char` or `char8_t` storage are rejected as well; the controlled low-level conversions live in the pointer/span helpers and `TextConversions.*`, while standard-library and third-party C ABIs receive those explicitly bounded views. Embedding projects pass their first-party source scopes explicitly and exclude vendored trees. Run `--self-test` when changing the scanner.

`BuildTools/raw_uint8_audit.py` is the companion lexical gate for the binary/numeric distinction. First-party `uint8_t` sequence containers, spans, owning arrays, and C arrays must either migrate to `byte` or match `BuildTools/raw_uint8_allowlist.tsv`. The allowlist is deliberately strict: each reviewed numeric or fixed-ABI source file records the exact occurrence count plus a SHA-256 of its normalized declaration lines, so adding, removing, or editing a declaration requires a fresh semantic review rather than silently inheriting a path-wide exemption. Decoded color channels, palette indices, gameplay direction values, and fixed AngelScript/third-party `vector<uint8_t>` ABIs remain numeric; file payloads, compressed data, audio byte streams, object storage, and other uninterpreted buffers use `byte`. Run `--self-test` when changing this scanner too.

`string_view` is exactly `std::string_view`; it needs no factory or runtime branding. `u8string_view` is a non-owning validated UTF-8 view created from compile-time-checked literals, exact native views, or lvalue `basic_string` factories. Its literal constructor is implicit, so a UTF-8 view parameter accepts `u8"UTF-8"` directly while validating the literal at compile time. Raw pointers and temporary owning strings are rejected by its checked factories so length and lifetime stay explicit. As with every non-owning view, the backing storage must remain alive and unchanged while the view is used; `u8string` revalidates incoming UTF-8 views before copying.

`Containers.*` provides `u8istringstream` for token and line parsing over validated `char8_t` storage. UTF-8 tokens and `getline()` results stay `u8string`; extraction into `string` performs the explicit destination-domain check and throws for a non-ASCII token; arithmetic extraction accepts only ASCII numeric tokens and performs its checked narrowing internally. Ordinary `istringstream` remains the ASCII stream and must not receive UTF-8 through a character-view or temporary narrow-string adapter. `StringUtils.*` keeps the proxy families separate: `strvex` / `strex` operate only in the ASCII domain, while `u8strvex` / `u8strex` operate on validated UTF-8 and provide Unicode-aware case conversion, code-point length, trimming, splitting, tokenization, and UTF-8 path operations. Shared operation names such as `trim()` and `tokenize()` express the same algorithm in the proxy's own text domain; parallel `*_utf8` method names are not part of the API.

The `_nt` views additionally prove a trailing zero and the absence of embedded zeros. For ordinary strings, `try_string_view_nt_from_span()` and `string_view_nt_from_span()` validate bounded storage that includes the terminator. For UTF-8, `u8string_view_nt` additionally validates the text invariant, and `u8string::try_view_nt()` / `view_nt()` derive both guarantees from the owner. These views never transfer ownership; their backing storage must outlive them.

`validate_ascii_text()` and `validate_utf8_text()` validate exact character views, `char8_t` views, or explicit byte spans without allocation; raw pointer overloads are deleted so validation never performs an unbounded terminator search. They return a `TextValidationIssue` with the exact reason and code-unit/byte offset for non-ASCII input, invalid UTF-8 lead or continuation bytes, truncation, overlong encodings, surrogate scalars, and out-of-range scalars. Terminated-view factories add distinct `EmbeddedNull` and `MissingTerminator` failures. Throwing checked factories report the same information through `TextValidationException` (`encoding()`, `error()`, and `offset()`); `what()` uses a static reason-specific message. The low-level `utf8` codec remains the code-point decode/encode API, while strict UTF-8 validation is the invariant gate.

`TextConversions.*` is the explicit boundary-adapter layer immediately above `TextTypes.*`. Bounded character and raw-byte UTF-8 input enters through `utf8_from_char_span()` or `utf8_from_byte_span()`; ASCII bytes copy through `string_from_byte_span()`. These functions never infer a length from a raw pointer. `utf8_from_terminated_char_span()` accepts a bounded character span whose final element must be the sole terminator, then reports an empty/missing final zero as `MissingTerminator` and an earlier zero as `EmbeddedNull`, with the exact offset.

The reverse adapters `utf8_to_char_span()`, `utf8_to_byte_span()`, and `string_to_byte_span()` are zero-copy read-only views. Their result is valid only while the source storage remains alive and unchanged. A UTF-8 raw C ABI pointer is available through `utf8_to_c_str(u8string_view_nt)`; C callback implementations that must return the raw pointer use `return_utf8_c_str(u8string_view_nt)` instead of unwrapping the pointer facade locally. Ordinary `string_view_nt::c_str()` serves the ASCII C ABI. Requiring an `_nt` view proves the trailing zero and absence of embedded zeros; the backing owner must outlive the C call or returned-pointer use.

`utf16_string` is the portable UTF-16 boundary container: `std::basic_string<char16_t, std::char_traits<char16_t>, SafeAllocator<char16_t>>`. It is used for APIs whose contract is UTF-16 code units instead of a platform-dependent `wchar_t` width. `validate_utf16_text()` returns an optional issue for unpaired high and low surrogates, and `utf8_to_utf16()` / `utf16_to_utf8()` convert only Unicode scalar sequences. UTF-16 input is an explicitly bounded `std::u16string_view`; raw `char16_t` pointer overloads are deleted. On Windows, `wide_string` is the corresponding SafeAllocator-owned `wchar_t` container; `utf16_to_wide()` and `wide_to_utf16()` are its shared checked code-unit adapters for OS APIs. Both reject malformed surrogate sequences before returning, so platform code does not duplicate unchecked `bit_cast` loops or cross allocator domains. Throwing adapters and conversions report invalid UTF-8, malformed UTF-16, and terminated-span failures through the shared `TextValidationException`; UTF-16 failures use `TextEncoding::Utf16`, `UnpairedHighSurrogate` or `UnpairedLowSurrogate`, and the exact `char16_t` code-unit offset.

`TextFormatting.*` provides the checked formatting machinery shared by the string proxies, logging, exceptions, and `FormatUtf8()`. `format_string<Args...>` is constructed from a compile-time ASCII format literal, and `u8format_string<Args...>` from a compile-time `char8_t` literal; both validate the standard format grammar and argument types during compilation. `strex` accepts only the ASCII format/argument domain and produces `string`. `u8strex` accepts ASCII or UTF-8 format literals, promotes ASCII arguments without a caller-side adapter, and produces `u8string`. `FormatUtf8()` remains the direct owning UTF-8 entry point and accepts either format domain. There is no `FormatAscii` API.

UTF-8 owners, views, and terminated views are accepted as formatting arguments. `strex`, `u8strex`, and `FormatUtf8()` accept ordinary narrow `string`, `string_view`, `std::string`, bounded `char` arrays, non-null `const char*`, and the matching proxy arguments. `u8strex` and `FormatUtf8()` additionally accept UTF-8 owners/views and UTF-8 proxies. Owner temporaries remain alive for the complete formatting call, and proxy arguments are adapted as bounded views for the same duration, so callers do not extract temporary strings or spell `.view()`. UTF-8 formatting privately adapts and validates the complete result. Raw wide text, raw `char8_t` pointers, null character pointers, and dynamic format strings remain rejected.

Formatting preserves embedded zeros as ordinary code units and validates the complete result before constructing its strict owner. Consequently, even output produced by a custom formatter cannot escape the ASCII or UTF-8 invariant: malformed output throws `TextValidationException` with the output encoding, exact validation reason, and exact output offset. The implementation keeps any standard-library `char` formatting adapters private; it does not add public `std::formatter` specializations for strict UTF-8 types.

`strvex` / `strex` never accept or expose UTF-8 storage, and they have no `*_utf8` methods. `u8strvex` / `u8strex` never narrow their results to `string`; any UTF-8-to-ASCII transition remains an explicit `utf8_to_string()` operation that can fail. APIs whose semantic domain is UTF-8 should accept `u8string_view`, allowing `u8strex(...)` temporaries and `u8string` lvalues to bind directly without `.view()`. When such an API also naturally accepts ASCII, provide an ASCII overload that promotes internally instead of forcing callers through character-storage adapters.

ASCII `string` / `string_view` values promote to `u8string` directly; there is no named string-to-UTF-8 conversion helper and callers must not manufacture one. APIs that genuinely expose UTF-8 through bounded `char` storage enter through `utf8_from_char_span()`. The reverse character-ABI crossings use the global `snake_case` helpers `utf8_as_char_view()` and `utf8_to_char_string()`, while checked UTF-8-to-ASCII narrowing uses `utf8_to_string()`. UTF-16 boundaries use the shared `utf8_to_utf16()` / `utf16_to_utf8()` pair for Unicode and `string_to_utf16()` / `utf16_to_string()` for the checked ASCII domain; translation units do not duplicate those conversions in subject-specific helpers. Every production `utf8_to_char_string()` call is an audit point: it must terminate at a concrete character-storage ABI or remain explicitly tracked as an unfinished migration boundary, never silently redefine an engine path or text value as ASCII. `utf8_to_char_string()` preserves validated UTF-8 bytes for a `char` ABI; it is not the checked narrowing operation. The container-layer `utf8_map_as_char_views()` helper lives in `Containers.*`, after both text conversions and the engine map aliases are available. Windows callers additionally share `string_to_wide_string()` and `utf8_to_wide_string()`. These helpers belong only at an actual OS, C, SDK, parser, standard-library formatter, or not-yet-migrated API boundary: never wrap an API such as `WriteLog`, `strex`, `AnyData`, an engine exception, or `FO_VERIFY_*` with `utf8_as_char_view()` merely to make types line up. Translation units must not add subject-named conversion copies or reconstruct `string`/`string_view` through a local span pair. A runtime raw character pointer must first be paired with its exact length before entering the bounded span adapter. UTF-8-to-character helpers revalidate borrowed views, ASCII narrowing rejects the first non-ASCII code unit, and zero-copy views remain valid only while their backing owners are alive and unchanged. AngelScript exposes one UTF-8 `string` type, but native binding signatures still carry the invariant: user text, physical filesystem paths, and other Unicode-capable values use `u8string` / `u8string_view`; logical resource paths and other deliberately ASCII-only identifiers or grammar tokens use `string` / `string_view` and are checked at the generated boundary.

`AnyData` is a strict UTF-8 value domain. String values and dictionary keys are owned as `u8string`; `AsString()`, lookup, parsing, and serialization use `u8string_view` / `u8string` without an intermediate `char` string. Integer, floating-point, and boolean tokens are ASCII subdomains and are checked before reaching the existing numeric parser. JSON, BSON, ImGui, AngelScript, diagnostics, and similar interfaces may use a global text adapter only at their actual external or not-yet-migrated `char` boundary; `AnyData` itself never stores or exposes unclassified character text.

Formatting width, precision, and fill retain the standard library's `char` formatting semantics; they are not a grapheme-cluster or terminal display-width contract, and multibyte Unicode fill characters are not part of this API guarantee. Complete-result validation turns any formatter operation that splits a UTF-8 sequence into a checked failure instead of allowing malformed text into `u8string`.

The low-level `utf8` codec treats Unicode scalar values as U+0000...U+10FFFF excluding the surrogate range U+D800...U+DFFF. `utf8::Decode()` and `utf8::Encode()` report malformed input through an empty `optional`; U+FFFD is an ordinary valid scalar and is never used as an error sentinel. A successful decode writes the consumed one-to-four-byte length back through its length argument. A structural decoding error consumes one available byte so recovery walkers can always progress, while a structurally complete encoded surrogate reports its full three-byte length together with failure.

### Filesystem, compression, sockets, and work threads

`DiskFileSystem.*` is the low-level disk abstraction. Every filesystem path parameter is `u8string_view`, and every owned path result is `u8string`; the ASCII `string_view` domain has no path overload. `fs_make_path()` is the single UTF-8-to-`std::filesystem` entry point, while `fs_path_to_u8string()` converts native paths back through `generic_u8string()` and validates the result. Directory visitors expose relative names as `u8string_view`. Callers must keep an owning `u8string` alive before borrowing `.view()`; borrowing from a temporary owner is rejected by the UTF-8 text types.

Whole-file access is separated by semantics: `fs_read_file_bytes()` / `fs_write_file_bytes()` use `vector<byte>` and `const_span<byte>`, while `fs_read_file_text()` returns `optional<u8string>` and `fs_write_file_text()` accepts only `u8string_view`. Text reads validate the complete file and throw `TextValidationException` for malformed UTF-8; the byte API still exposes the same file unchanged. `fs_compare_file_bytes()` and `fs_hash_bytes()` likewise accept only explicit byte views, and `stream_read_exact()` fills a `span<byte>`. Thus neither arbitrary binary payloads nor unvalidated legacy path/text strings can enter the disk API implicitly.

`fs_make_writable_path(user_writable_path, relative)` is the small path-policy helper used by higher layers for installed-client writable overlays: empty root or absolute input returns the input unchanged, while a relative path is layered under the writable root. The higher-level mounted resource view is `Source/Common/FileSystem.*` and is documented in [ConfigurationAndDataSources.md](ConfigurationAndDataSources.md). `Compressor.*` accepts and returns only `byte` buffers; reinterpretation to zlib's `Bytef` is confined to its implementation. Network compression now keeps the same byte type through `NetBuffer` and the transport callbacks, while legacy numeric parser/SDK buffers use explicit views only at their immediate boundaries. Fixed decoder fixtures protect the zlib stream format without assuming byte-for-byte encoder output across zlib versions. `NetSockets.*` accepts only `span<byte>` / `const_span<byte>` payloads below the higher-level network command/connection model in [Networking.md](Networking.md), and `WorkThread.*` owns simple background-worker infrastructure.

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
- `Source/Tests/Test_TextConversions.cpp`
- `Source/Tests/Test_TextFormatting.cpp`
- `Source/Tests/Test_TextTypes.cpp`
- `Source/Tests/Test_TimeRelated.cpp`
- `Source/Tests/Test_WorkThread.cpp`

`Test_TextConversions.cpp` pins mixed Unicode and embedded-zero round trips through exact character and byte spans, malformed UTF-8 diagnostics, bounded terminated-character handling, zero-copy and C ABI adapter compile gates, character-bridge ownership and ASCII narrowing, raw-pointer rejection, UTF-16 scalar boundaries and lone-surrogate offsets, checked Windows `wchar_t`/UTF-16 code-unit preservation, and revalidation when a previously branded external UTF-8 view has gone stale.

`Test_TextFormatting.cpp` pins format-spec behavior, UTF-8 literals across Cyrillic, combining, and non-BMP text, ASCII-to-UTF-8 promotion, direct narrow-character promotion, owner-temporary lifetime, embedded-zero preservation, and whole-result validation with exact failure diagnostics. Its compile-time gates accept strict owners and branded views with exact strict return types, accept bounded narrow-character values only at the UTF-8 entry point, and reject wide/raw `char8_t` text, dynamic formats, and every unclassified or UTF-8 argument at the ASCII entry point.

See [Testing.md](Testing.md) for the complete test-suite map and target wiring.

## Change routing

- Compiler/OS gates, namespace, base aliases, and low-level macros: `Source/Essentials/BasicCore.*`.
- Global create/delete callback registration: `Source/Essentials/GlobalData.*`.
- Stack traces, logging, and exception reporting: `Source/Essentials/StackTrace.*`, `BaseLogging.*`, `Logging.*`, `ExceptionHandling.*`, and [Debugging.md](Debugging.md).
- Generic memory/pointer utilities: `Source/Essentials/MemorySystem.*`, `SmartPointers.*`, and [SmartPointers.md](SmartPointers.md).
- ASCII/UTF-8 invariants, checked text construction, and terminated text views: `Source/Essentials/TextTypes.*`; bounded character/byte/C ABI adapters and UTF-8/UTF-16 boundaries: `Source/Essentials/TextConversions.*`; strict ASCII/UTF-8 formatting: `Source/Essentials/TextFormatting.*`; code-point decode/encode and legacy string algorithms: `Source/Essentials/StringUtils.*`.
- File bytes and low-level writable-path composition on disk: `Source/Essentials/DiskFileSystem.*`; mounted engine resources and installed-client overlays: [ConfigurationAndDataSources.md](ConfigurationAndDataSources.md).
- Socket primitives: `Source/Essentials/NetSockets.*`; protocol/command/network runtime: [Networking.md](Networking.md).

## Validation checklist

1. Confirm the change does not introduce a dependency from essentials back into higher engine layers.
2. Update `BuildTools/cmake/stages/EngineSources.cmake` when adding/removing essentials files.
3. Run the smallest matching essentials test and then the broader `RunUnitTests` target when behavior crosses utility boundaries.
   For strict text changes, run the focused `TextTypes`, `TextConversions`, `TextFormatting`, and `StringUtils` cases together so validator/codec agreement, bounded boundary adapters, UTF-16 error offsets, compile-time formatting gates, and complete formatted-output validation stay covered.
4. For diagnostics changes, also verify [Debugging.md](Debugging.md) stays accurate.
5. For filesystem/socket/threading changes, validate at least one higher-level consumer if the low-level contract changed.
