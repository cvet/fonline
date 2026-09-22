---
layout: default
title: Managed C# Scripting
locale: en
document_id: managed-csharp-scripting
permalink: /Docs/en/how-to/scripting/managed-csharp.html
---

# Managed C# Scripting

> Engine-owned documentation. This guide describes the reusable Managed C# backend, its authoring contract, generated API, lifecycle, synchronization, build, delivery, and validation. Game modules and project-specific policy belong to the embedding project.

## Contract status

Managed C# is an implemented scripting backend for server, client, Mapper, bakers, and tests. It embeds Mono, compiles project scripts with the configured .NET SDK, and exposes the same backend-neutral Engine metadata used by AngelScript. Native scripting remains an unimplemented source-root placeholder and is not an equivalent third backend.

An embedding project selects its scripting backend at configure time. Enable `FO_MANAGED_SCRIPTING` and disable `FO_ANGELSCRIPT_SCRIPTING` when the project is fully managed. Do not enable a backend merely because its source directory exists; the project configuration, resource packs, generated metadata, script sources, packages, and tests must agree.

Use this guide beside:

- [Scripting Runtime](../../explanation/scripting-runtime/) for the backend-neutral facade and the AngelScript comparison;
- [Script Lifecycle and Concurrency](lifecycle-and-concurrency.md) for lifecycle rules shared by both implemented backends;
- [Remote Calls](../../reference/scripting/remote-calls.md) for the wire contract;
- [Generated API and Metadata](../../reference/metadata/) for the source-to-generated dependency graph;
- [Packaging](../release/packaging.md) and [Client Updater](../../explanation/runtime/client-updater.md) for delivery.

## Ownership and source layout

The Engine owns:

- `Source/Scripting/Managed/ManagedRuntime.*`: process-wide Mono startup and native-thread attachment;
- `ManagedScriptBackend.*`: one backend instance, assembly load context, marshalling, callbacks, events, remote calls, exceptions, and shutdown;
- `ManagedPInvokeTable.*` plus `BuildTools/generate_pinvoke_table.py`: native interop shim registration;
- `CoreScripts/*.cs`: the reusable `FOnline` namespace, attributes, synchronization helpers, async scheduler, invocation bridge, remote-call bridge, verification, and value-type helpers;
- `ManagedHost/ManagedLoadContextHost.cs`: per-backend assembly isolation;
- `Analyzers/FOnline.Analyzers.csproj`: compile-time entity-cover analysis;
- `Source/Tools/ManagedScriptBaker.*`: generated project/API and assembly production.

The embedding project owns game scripts, its namespace, project-only attributes and registrars, higher-level libraries such as a GUI object model, its resource-pack selection, target framework, analyzers, tests, and release qualification. The Engine no longer ships the former AngelScript high-level CoreScripts library; copying that library into Engine under C# would cross the same ownership boundary.

## Configure the backend

The CMake switch is `FO_MANAGED_SCRIPTING`. It adds the managed runtime, backend, baker, CoreScripts tests, and managed application wiring. `FO_NATIVE_SCRIPTING`, `FO_ANGELSCRIPT_SCRIPTING`, and `FO_MANAGED_SCRIPTING` are independent build options, but a production project should deliberately select one gameplay backend unless it is testing cross-backend invocation.

The immutable startup settings in the `ManagedScript` group define the generated project:

| Setting | Contract |
| --- | --- |
| `ManagedScript.Assemblies` | Logical entry assemblies to build. |
| `ManagedScript.ProjectName` | Base name for the generated solution and project. |
| `ManagedScript.TargetFramework` | Target framework passed to the generated SDK-style project. |
| `ManagedScript.MsBuild` | Command used to build the generated project. |
| `ManagedScript.Dirs` | Source roots scanned for top-level `.cs` files; normally Engine CoreScripts plus project scripts. |
| `ManagedScript.GeneratedDir` | Optional generated-project directory; empty selects the build tree's `GeneratedSource/Managed`. |
| `ManagedScript.ExtraSources` | Additional `assembly,target,path` inputs. |
| `ManagedScript.ExtraReferences` | Additional `assembly,target,reference` inputs. |
| `ManagedScript.Analyzers` | Roslyn analyzer projects included in the generated build. |
| `ManagedScript.AnalyzerPackages` | Roslyn analyzer NuGet packages as exact `name,version` pairs. |
| `ManagedScript.AdditionalFiles` | Analyzer configuration files exposed through MSBuild `AdditionalFiles`. |
| `ManagedScript.AnalysisLevel` / `AnalysisMode` | Optional SDK analysis-level and analysis-mode overrides. |
| `ManagedScript.BakerDryRun` | Structural baker mode for tests; it does not prove executable assemblies. |
| `ManagedScript.DeepTrackEntityWrappers` | Opt-in shutdown diagnostics that name still-live entity wrappers; ordinary live-wrapper counting is always enabled. |

Add a resource pack whose `Bakers` list contains `Managed`. The pack inputs must include Engine `CoreScripts`, the project script roots, and the `///@` metadata sources needed by those scripts. The assembly, target, pack, and metadata selection are one contract: compiling a loose project that differs from the baker input is not Engine validation.

## Generated project and assemblies

`ManagedScriptBaker` generates target API files, one SDK-style project, and a solution under the configured generated directory. Generated filenames carry `.gen`, an auto-generated banner, and their own nullable directive. Stale generated files not present in the new set are removed. Do not commit or hand-edit these outputs unless an embedding project explicitly treats a generated input as authored.

The generated project enables nullable analysis, warnings as errors, Engine code style, configured analyzers, and the sources/references selected for each assembly and runtime target. API generation covers:

- Engine settings, enums, value types, entities, prototypes, fixed types, and dynamic ref types;
- scalar, array, list, dictionary, and supported dict-of-list property forms;
- methods, overload identities, mutable arguments, callback delegates, and events;
- remote-call sender facades and inbound handler registration;
- native ref-type wrappers with explicit reference management where a borrow outlives the call.

Unsupported type or member shapes fail baking with `ManagedScriptBakerException`; the baker must not emit a placeholder that fails only when gameplay reaches it.

Compiled entry assemblies are target-specific, such as `<Pack>.Server.dll`, `<Pack>.Client.dll`, and `<Pack>.Mapper.dll`. They are written under the baked pack's `Assemblies/<Target>Assemblies/` tree. Helpers and dependencies remain next to the entry assembly.

## Authoring shape

Use the namespace owned by the embedding project and import `FOnline` for generated Engine types. Engine CoreScripts use file-scoped `namespace FOnline;`; a game should not put its own domain types there.

Treat nullable reference types as part of the script/native contract. Generated reference and entity APIs distinguish nullable and non-nullable values. Narrow expected absence with an ordinary branch; use `Game.Verify` or `Game.VerifyNotNull` for violated invariants. A managed reference to an Engine entity does not own persistence or lifetime.

Avoid mutable process-wide state. Each backend has an isolated load context, but gameplay state still belongs to a `Game`, entity, property, ref type, or another explicit per-engine owner. Static constants and immutable type metadata are fine; a mutable cache, registry, timer gate, or collection is not made correctly owned merely by being in a C# static field.

Use fixed exception messages with dynamic values as context through the Engine verification helpers. Do not hardcode player-visible text in exceptions or logs and then surface it to the player; localization stays project-owned.

## Initialization and attributes

`Initializator.InitializeEarly` runs before ordinary module initialization. It rejects every declared `async void` method, registers Engine attributed functions and remote calls, then runs project `[ScriptFuncRegistrar]` methods. A registrar must be static, parameterless, and return `void`; use it for project attributes that must be resolvable by bake-time reflection.

`Initializator.Initialize` runs static constructors, discovers `[ModuleInit(priority)]` methods, orders them by ascending priority, and invokes them. A module initializer must be static, parameterless, and return `void` or `Task`. A `Task` result is awaited in the initializer's private synchronous continuation context; a null task is an error.

Managed marker attributes mirror the Engine dispatch roles rather than ordinary direct calls:

- `[Event]` for event handlers;
- `[TimeEvent]` for time-event callbacks;
- `[PropertyGetter]` and `[PropertySetter]`;
- `[ServerRemoteCall]`, `[ClientRemoteCall]`, and `[AdminRemoteCall]`;
- `[ItemTrigger]`, `[ItemInit]`, `[ItemStatic]`, `[CritterInit]`, `[MapInit]`, and `[LocationInit]`;
- `[AnimCallback]`, `[ClassExtension]`, and project-registered marker attributes;
- `[CallableByName]` for functions intentionally exposed to named invocation.

Generated event wrappers reject a handler without `[Event]` before creating the native subscription. Named calls are likewise deny-by-default: `Game.Invoke` and administrative dispatch accept only explicitly marked functions and the applicable allowlist.

## Events, callbacks, timers, and named calls

Generated event wrappers support handlers returning `void`, `Task`, `EventResult`, or `Task<EventResult>`. Native dispatch awaits result-bearing tasks before deciding whether the subscriber chain continues. Use `Task<EventResult>` when an awaited handler may consume or destroy an argument that later subscribers must not receive.

Time-event APIs accept synchronous delegates and generated async delegates returning `Task`. The delegate identity is part of registration: stopping a time event with a separately constructed delegate does not identify the existing callback. Repetition or cancellation changes future scheduling and does not cancel a task that has already started.

Entity-only post-set reactions may return `Task`; value-transforming setters with `ref` arguments and property getters remain synchronous because the native caller needs their result before the call returns.

Inbound remote-call handlers may return `void`, `Task`, or `Task<T>`. Remote calls have no wire result, so incomplete tasks are observed without blocking the network/client pump; a `Task<T>` value is ignored. A named script function returning non-generic `Task` follows the same asynchronous boundary. A `Task<T>` named function remains synchronous when native code requires `T`.

Callback and remote-call arguments keep their metadata shape: native `any[]` arrives as `List<any>` and `string=>any` as `Dictionary<string, any>`. The bridge does not adapt these into `List<object>` or string dictionaries; declare the exact generated types.

## Async and continuation scheduling

`Game.YieldAsync(milliseconds)` is the managed equivalent of a script suspension. It completes from an Engine time event and resumes through the backend-owned `ScriptSynchronizationContext`. Do not use `async void`; return `Task` or `Task<T>` so the Engine can observe completion and faults.

Each backend owns a separate continuation queue. `BaseEngine::FrameAdvance` pumps only its backends after releasing the frame-property lock. Every resumed continuation re-enters the owning Engine through `RunScriptContext`; on the server this creates a fresh synchronization context. A newly posted continuation waits for a later frame, so a yielding loop cannot monopolize one frame.

`Post` raises an atomic backend-ready flag while the scheduler is open; an idle frame does not enter managed code just to find an empty queue. A partly failed pump signals remaining work for the next frame. Shutdown closes the scheduler before unbinding the backend, so a late post cannot signal released native state. `Native.GetAndResetContinuationPumps` exposes pump counts to interop tests.

Synchronous native-result callbacks and module initialization use a private continuation queue and drain only their own awaited continuations. `Game.YieldAsync` is rejected in that context because the blocked caller cannot advance the timer pump. Completed tasks remain valid.

`ConfigureAwait(false)`, `Task.Run`, `Task.Factory`, `Parallel`, and manually dispatched ThreadPool work deliberately bypass the Engine synchronization context. They may perform isolated computation, but they must not call Engine APIs. The entry assembly's backend binding can still identify an Engine from such a thread, so this misuse is not guaranteed to fail at every native call; only synchronization-sensitive routes such as server entity access reliably reject it. Embedding projects should ban these escape APIs statically and return to the captured Engine context before touching Engine state.

## Server entity synchronization

The server cover contract is backend-neutral: script callers must cover every existing entity the native call graph can read or mutate. An `await` ends the old cover; re-resolve or revalidate retained entities and reacquire cover before reuse.

Managed scripts declare and prove this contract with:

- `[RequiresCover]` on a parameter or method receiver;
- `[ProvidesCover]` on a parameter or return value that establishes cover;
- `[PreservesCover]` on an awaitable helper that restores the caller's cover;
- `CoverReach.Parent`, `Ancestors`, and `DestroyGraph` for transitive requirements;
- the `Sync` CoreScript helpers as the only normal wrappers around raw `Game.Sync`, `SyncRelease`, `Lock`, and `Unlock`.

The Roslyn analyzer reports invalid annotations (`FOSYNC001`), unsatisfied transitive cover (`FOSYNC002`), missing entry-point declarations (`FOSYNC003`), cover probing instead of acquisition (`FOSYNC004`), raw synchronization calls outside the helper (`FOSYNC005`), and cover use not re-proved after `await` (`FOSYNC009`). `FOSYNC006` and `FOSYNC007` are retired: use `using GameLock scope = GameLock.Acquire();`, whose `ref struct` scope releases on every path and cannot survive an `await`. Configure the analyzer through `ManagedScript.Analyzers` or `ManagedScript.AnalyzerPackages` and treat its warnings as build failures.

Provider inference first proves that a candidate call executes on every returning
path and only then traverses its callees. A call hidden in a conditional branch
cannot establish cover; this order also prevents dense conditional call cycles
from expanding exponentially. Analyzer self-tests time-bound that graph and still
require `FOSYNC009` for an uncovered use after `await`.

`Sync.Acquire` expands linked cover in place through `Game.SyncWiden`; it does not release and reacquire the already-held entities. This keeps the native cover continuous while following `[SyncWiden]` relationships and avoids a race window between the two sets.

`FOSYNC010` rejects discarding a boolean acquisition answer, including a bare call or assignment to `_`: failure must influence control flow. `FOSYNC011` requires a `Sync` helper that changes held cover, directly or through another effectful helper, to declare its own `[CoverEffect]`. The analyzer treats these as build verdicts, not advisory warnings; the proposed redundancy diagnostics `FOSYNC012`–`FOSYNC014` were withdrawn.

Attributes state a proof; they do not lock anything. Entry points annotate the entity the Engine already synchronized. Ordinary helpers either acquire the required cover or propagate `[RequiresCover]` to their callers.

Custom dispatchers can declare their marker attribute with `EntryPointMarker`, so the analyzer treats their handlers as entry points. `FOSYNC009` requires a current cover after `await`: locking an earlier `map = cr.GetMap()` does not re-prove `cr`; use the current direct alias, a whole covered collection, or an explicit `PassesCover`/`RestoreCallerCover` relationship. The withdrawn redundancy rules `FOSYNC012`–`FOSYNC014` are not part of the active contract. Failed `Sync` acquisitions may be observed through `Sync.OnFailure`; subscribers receive immutable caller, reason, entity, and stack snapshots without changing the helper's `false` result. With no subscribers, the diagnostic snapshot is not allocated.

## Values, collections, properties, and lifetime

The bridge converts supported primitives, enums, strings, `hstring`, value types, entities, ref types, lists, dictionaries, delegates, mutable arguments, and return values through Engine metadata. A registered value type is plain packed data: every field is a primitive, enum, `hstring`, or single-field value type; every offset is aligned to the field size; the total size has no tail padding; and a native twin is trivially copyable with the same size. Metadata registration rejects every other shape. Generated C# structs use sequential layout, and the backend checks the Mono value size before copying their bytes.

Managed `FOnline.any` is a value type holding the Engine's textual `any_t`, not `object` or `string`. Conversion into it is implicit for supported primitives, strings, enums, and generated value structs; conversion out is explicit and rejects invalid or out-of-range data. Empty text reads as zero/false/empty text, numeric enum reads validate the underlying range, and lowercase float suffixes are accepted for numeric reads. `ToEnum<T>()` accepts a qualified member, bare member, or numeric value. Equality compares the stored text; `IsEmpty` tests empty text. Use the explicit operators, not `System.Convert`/`IConvertible`. Collections use `List<any>` and `Dictionary<K, any>`, and generated `GetAsAny`/`SetAsAny` bridge properties. The managed ABI and baked assemblies must move together when this representation changes.

Managed script property writes, including unboxed, fixed-list, and converting paths, use `Properties::SetValue`: validation/clamping happens first, unchanged stored bytes stop the write, and only a changed value runs setters and post-setters (persistence and client sync). `SetValueFromData` is for applying received network data, never a script assignment.

Generated entity properties are native-backed. Dynamic ref types are managed DTOs whose values are materialized from or assigned to native property storage. A getter returns detached structured state; persist a mutation with read-modify-reassign unless the generated member itself is a live wrapper.

Native ref types are explicit borrowed wrappers. If a project keeps one beyond the call/frame that returned it, follow the generated `__AddRef()`/`__Release()` contract. Factory-backed wrappers start with a reference that must be released after ownership is transferred or detached.

`hstring` is an eight-byte blittable value containing the native intern-entry pointer. Frames and value types copy that pointer unchanged; only property and RPC storage uses the 64-bit hash and converts at the storage boundary. Values are interned through the Engine metadata bound to the entry assembly, resolve their text from that exact entry, and do not fall back to a process-wide hash table shared by engine instances. Static managed fields still initialize separately in every load context.

Arrays of primitives, enums, `hstring`, and registered value types use `GetPropertyList<T>` / `SetPropertyList<T>` and cross as raw bytes. Longer reads retry directly into the final list storage while the same cover remains held. Strings, dictionaries, dynamic ref types, nullable proto/fixed-type values, and other structured forms keep the converting bridge, but generated access selects the property by registrar index rather than repeating owner and property names.

### Indexed native interop ABI

`ManagedScriptBaker` and the native backend share `ManagedInteropAbi`: one manifest of dense method, event, setting, and inner-entity ids plus a content hash. Generated `*Abi.gen.cs` bind stubs call `Native.BindAbi` during `Initializator.InitializeEarly`; a hash or count mismatch fails loading before script execution. Generated ABI files participate in the incremental bake stamp, so a generator-only change cannot publish new wrappers with an old assembly.

The indexed path covers primitives, enums, `hstring`, registered value types, and by-value entity/proto/fixed/ref-type handles. Methods use `CallMethodIndexed`, eligible events use `FireEventIndexed`, numeric/bool settings use `GetSettingValue<T>`, and inner entities are collected by one `FillInnerEntities` snapshot instead of `Count` plus repeated indexed lookups. Complex signatures use the corresponding boxed path with the same dense id. Nullability belongs to the manifest: a non-nullable handle slot rejects zero, nullable handles may carry zero, and dynamic ref types, by-ref handles, and abstract/base entity results remain boxed where their runtime type is required.

Managed frames are compact packed buffers, but native code never dereferences an unaligned slot. `BuildManagedAbiNativeFrame` copies inputs and result slots into aligned stack storage, native dispatch works on that storage, and `CopyBackManagedAbiNativeFrame` returns only mutable arguments and the result. Event adapters return `EventResult` through a trailing `ref int`, copy by-ref arguments back after invocation, and avoid boxing the result.

Native-to-managed callbacks whose signatures contain only fixed values and entity/ref-type handles use generated `CallbackAdapters.Adapt_<key>` methods. One `ManagedCallbackPlan` resolves the adapter during registration; wrapper factories and native wrapper classes are also registered/cached during ABI binding, so dispatch does not repeat reflection or constructor lookup. Unsupported callback shapes retain the boxed `MonoArray`/`DynamicInvoke` path. Entity event subscriptions belong to the native entity rather than to one wrapper: an equal handler is idempotent, any wrapper of that entity can unsubscribe it, and destruction removes the subscriptions.

Each generated entity-wrapper type belongs to one backend load context. Its native pointer is therefore sufficient for equality and hashing within that type; wrappers from different Engine instances are different runtime types. A live wrapper checks `Native.IsBackendAlive` before exposing its pointer. Once shutdown unbinds the entry assembly, later access throws `ObjectDisposedException`, and a late finalizer deliberately keeps its native reference instead of calling released Engine state.

Backend-owned caches are built before hot-path use: managed helper methods, metadata-named classes, dynamic ref-type accessors, wrapper constructors, callback adapters, list factories, and per-event adapters. Typed custom settings keep a parsed cell behind `GlobalSettings::GetCustomSettingsGeneration()`; every custom-setting writer advances the generation, while a warmed read is a generation comparison plus a value copy. `ScriptSynchronizationContext` likewise allocates its continuation queue only on the first post.

## Runtime loading, isolation, and shutdown

Mono is initialized once for the process. The first managed entry on a native Engine worker attaches that thread to the root domain and caches the attachment for the thread's lifetime. Later entries only switch the attachment GC-unsafe around managed execution and park it GC-safe while native work or locks run; reentrant entries inherit the attachment. The thread that initializes Mono is the exception because `mono_jit_init_version` attaches it implicitly and the initialization scope releases that adopted attachment. A backend then creates its own non-collectible `AssemblyLoadContext`; this is the per-engine isolation boundary because the embedded runtime does not provide usable classic AppDomain unload.

Before any code or type initializer runs from an entry assembly, the backend calls `Native.BindBackend` with its own pointer. Every internal call that needs Engine state passes that bound pointer explicitly, so static constructors, marshalling constructors, callbacks, and continuations identify the correct Engine without thread-local caller state. Binding identifies ownership only; it does not create a script synchronization context or server entity cover.

At startup, baked assemblies are restored into content-hashed subdirectories under the writable `Cache/ManagedAssemblies/` root. Existing byte-identical files are reused, so concurrent in-process engine instances do not rewrite an assembly Mono already loaded. Missing managed assemblies are a supported empty-backend state for tests/tools that do not bake scripts; a configured gameplay project should treat that as a packaging or resource-selection failure.

`DynamicAssemblies.Load(image, symbols)` loads a post-bake PE image into this backend's non-collectible load context. Its assembly name must be a fresh `FOnline.Dynamic.*` name; loaded code shares the backend's script types and statics and remains loaded for the process lifetime. `RunEntryAsync(MethodInfo)` accepts a static parameterless method returning a value, `Task`, or `Task<T>` and runs it as its own script entry and server synchronization context; it rejects `async void` and unwraps invocation exceptions. `ScriptsVersionId` is the entry assembly MVID. On a server, `ReadClientScriptsImage()` returns the client entry image used by the updater (or local bake), allowing a fragment compiler to target the matching client version. Dynamic assemblies participate in script-static cleanup.

The optional engine-owned `FOnline.ScriptCompiler` library compiles live fragments with Roslyn. Projects add its `.csproj` through `ManagedScript.ExtraReferences` only for targets that compile fragments; it and its dependencies are then packed alongside those entry assemblies. `DynamicScriptCompiler.CompileAsync` works off the Engine thread, uses a unique `FOnline.Dynamic.*` name, accepts a statement body or expression plus usings/symbols, and maps compile diagnostics to fragment line/column. It can compile against the running scripts or a supplied other-target image. Compilation has access to private/internal script members, so authorization to submit code is entirely an embedding-project responsibility. The emitted image has no PDB because the embedded Mono runtime may lack cryptography; compile diagnostics retain source lines, runtime frames do not. These assemblies are not hot-unloadable.

Shutdown first calls `BeginManagedTeardown`, which invokes `Native.BeginBackendTeardown` before any other cleanup and makes `Native.IsBackendTearingDown` true while the backend is still bound. This distinguishes a wrapper finalized during ordinary runtime from one made unreachable by teardown itself. A thread-affine wrapper that cannot release its native resource from the finalizer thread may suppress its leak report in the latter case because the owning Engine subsystem is about to be destroyed. `Native.IsBackendAlive` cannot make that distinction: unbinding deliberately remains later so entity wrappers collected during shutdown can still return their native references.

Shutdown then closes the continuation scheduler and discards queued work before releasing backend state. It clears project static references and persistent callback roots, runs bounded collect/finalizer passes while the Engine and assembly images still exist, reports remaining entity wrappers (and names them when deep tracking is enabled), then calls `Native.UnbindBackend` for every entry assembly before releasing the load scope and native global data. A non-browser finalizer wait runs on an Engine-requested pool task with a separate five-second budget, so a blocked finalizer cannot park the teardown thread indefinitely. A timeout or remaining-wrapper report is diagnostic and teardown continues; a wrapper that finalizes after unbinding must not release through dead native state.

The single-threaded browser runtime has neither a usable managed thread pool nor a finalizer thread. Its shutdown therefore performs one inline collect/wait pass; `GC.WaitForPendingFinalizers()` returns immediately, finalizers run later as main-thread jobs, and the interim wrapper count is reported but not verified. Later posts cannot run against a disposed Engine. Managed exceptions are counted and logged through the common script exception path; deferred task faults are observed once. Managed frames and nested managed causes are spliced into the common native stack trace, while native exceptions crossing managed code keep identity through GC handles.

## Build and bake workflow

The generated CMake target `CompileManagedScripts` runs the standalone `<ProjectDevName>_ManagedScriptBaker`. It depends on `ForceCodeGeneration`, loads the project configuration, prepares metadata, generates the managed API/project including `*Abi.gen.cs`, and compiles target assemblies without a full resource bake. The generated API files are part of the assembly stamp.

A `.csproj` in `ManagedScript.ExtraReferences` is built as a project reference and its package dependencies are copied for that target. Every packed helper assembly is claimed as a bake output, including on an up-to-date target; freshness checks use the pack output, not a temporary MSBuild output directory that the outdated sweep may remove.

`BakeResources` and `ForceBakeResources` run the `Managed` baker as part of the selected resource pack. Use the compile target for a fast source/API check and the bake target for the real resource, assembly, runtime-payload, and metadata contract. After a force bake, run an ordinary incremental bake and require it to settle cleanly.

The runtime toolchain is prepared by `SetupManagedRuntime`; `PrepareManagedRuntimePayload` produces the deployable subset and a `runtime.manifest`. Toolchain setup runs with an isolated environment so a workstation's `DOTNET_*`, NuGet, or SDK state does not silently redefine the published runtime. Runtime source builds disable the live NuGet advisory audit: the pinned source revision, not a later feed update, defines the reproducible dependency set. Before each runtime build, BuildTools removes dotnet's target-dependent repo-local tasks semaphore so switching from a desktop build to Android cannot reuse an incomplete task set. A configured workspace cache stores only a verified published runtime tree under a target/toolchain-specific key; local runtime source checkouts are never shared, incomplete cache hits are rebuilt, and a stale SDK bootstrap that lacks its matching shared runtime is removed before setup retries.

## Packaging and updating

The prepared runtime contains only managed class libraries actually referenced by the target assemblies, including `System.Private.CoreLib.dll`; native runtime libraries, JIT binaries, headers, import libraries, and symbols are excluded from the resource payload. Mono and the generated native interop table remain linked into the application. Target-specific class libraries are resolved from that target's published runtime rather than copied from the host SDK.

The Managed baker places the prepared runtime under `ManagedRuntime/` in the same resource pack as the game assemblies. Client packaging rebuilds that pack from the runtime payload belonging to the exact application target. Server packaging stages one target-specific copy for every distributed client target under `PlatformBinaries/<target>/`; the updater substitutes that copy for the common pack when serving that target. Several native variants may share this updater target while their independently built equivalent CoreLib payloads differ byte-for-byte, so packaging deterministically chooses the least-qualified matching binary entry, normally the default Release build, instead of requiring those payloads to be identical.

Runtime startup restores the selected payload atomically to `<CacheDir>/ManagedRuntime/<content-hash>/`, adds its class-library directory to Mono's search path, and uses the cached payload as the source of truth. Unpackaged native development binaries also receive the prepared payload beside the executable; packaged native, Web, and Android applications use the resource-pack copy.

The embedded payload defaults to invariant globalization because `System.Globalization.Native` is not shipped. A project that supplies and qualifies its own globalization native library may override the environment before runtime startup.

## Platforms and sanitizers

Managed scripting is wired for Windows, Linux, Android, WebAssembly, macOS, and iOS build paths, but an Engine source-capable path is not a project release claim. Qualify every shipped target with the exact project resource pack, assemblies, runtime payload, startup, callbacks, async work, shutdown, packaging, and update route.

Web uses the Mono interpreter plus Engine JavaScript scheduling/entropy glue; keep its interpreter thread attached until teardown. Because the interpreter compiles no native entry points, managed callbacks use `mono_runtime_invoke`; thunk and `UnmanagedCallersOnly` probe modes are skipped when `RuntimeFeature.IsDynamicCodeCompiled` is false. Script PDB resources are loaded there when present so managed stack traces retain source information. Android and Apple targets use target-specific runtime archives and class libraries. Never reuse one target's prepared payload for another target or architecture.

MemorySanitizer and ThreadSanitizer configurations are rejected with `FO_MANAGED_SCRIPTING`: embedded Mono and generated/JIT code cannot satisfy those instruments and otherwise report false failures. AddressSanitizer and the supported undefined/data-flow combinations still require the project's actual managed build and runtime checks.

## Diagnostics and debugging

The managed backend reports fixed native context plus managed exception text and stack information through the common script error path. A build that merely produces assemblies does not prove startup or callback dispatch. Set `ManagedScript.InteropProbeOnStart = True` for a client/device/browser qualification run that cannot host the native test suite; startup logs one `INTEROP-TRANSPORT` line per condition and a final summary.

Both script backends retain at most 32 distinct overrun entry names per Engine, counting repeats while preserving independent maximum execution and lock-wait times. `TakeScriptOverruns()` drains that buffer. The client drains before `OnLoop` and dispatches `OnScriptOverrun(entry, execution, lockWait, count)` outside the buffer lock; server and mapper do not publish the event in their loops. An overrun caused by a subscriber waits for the next drain. The usual threshold/debugger suppression still applies.

`InteropProbe` compares runtime invoke, classic thunk, and `UnmanagedCallersOnly` transports where the runtime supplies them, then measures production dispatch and its synchronization, attachment, and overrun-report components. Each series verifies delivery and arguments and reports GC handles, metadata lookups, managed objects, wrapper construction, and—under Tracy—native allocations per call. Counters are thread-local and disabled outside a measured stretch. Latency is evidence for a quiet-host comparison, not a shared-CI threshold; allocation and delivery counts are hard assertions.

When `FO_TRACY` is enabled, the backend installs a Mono profiler immediately after runtime initialization and before any entry assembly executes. Method-call instrumentation is restricted to registered game assembly images and methods with metadata tokens; runtime plumbing and generated wrappers stay out of the call tree. JIT zones are not image-filtered because a handler's first invocation pays for every method it reaches. The hook requests `ENTER | LEAVE | EXCEPTION_LEAVE`, deliberately not `TAIL_CALL`: Mono can eliminate a self tail call without a matching enter event, so treating that notification as an ordinary leave would close the caller's zone. An exceptional or inlined leave closes the per-thread zone stack down to the method Mono names.

Method names and source locations are resolved once into a process-wide table shared by all managed backends and then read under a shared lock. Zone names omit parameter lists and commas because Tracy's CSV hotspot exporter does not quote that field. Mono profiler callbacks are `noexcept` C-ABI boundaries and must never unwind through JIT-generated code. See [Profiling](../quality/profiling.md#managed-script-zones) for how to read these zones beneath a `Script execution overrun` entry.

Use the generated solution/project for IDE navigation and Roslyn diagnostics. Debug native startup and P/Invoke at the host process boundary; debug managed behavior with runtime logs and focused callbacks unless the embedding project provides a qualified managed debugger attachment workflow. The AngelScript UDP debugger does not debug C# and its settings should not be presented as a managed debugger.

First diagnosis routes:

| Symptom | Inspect first |
| --- | --- |
| Generated type or member is missing | Metadata input, target selection, and `ManagedScriptBaker` diagnostic. |
| Build sees stale API | Generated directory selection and `CompileManagedScripts` dependency. |
| Assembly builds but runtime loads none | Baked pack selection and `Assemblies/<Target>Assemblies/`. |
| Works natively but not on Web/Android | Target-specific runtime payload and platform build, not the host SDK output. |
| Continuation never resumes | Captured `ScriptSynchronizationContext`, frame pump, and forbidden ThreadPool escape. |
| Native API fails after `await` | Entity liveness and reacquired synchronization cover. |
| Callback cannot be registered | Required marker attribute and exact generated delegate signature. |
| Package starts with missing framework type | `ManagedRuntime/runtime.manifest` and target-specific pack replacement. |
| ABI bind fails before module initialization | Stale generated `*Abi.gen.cs`, native manifest/hash mismatch, or a skipped managed rebuild. |
| A wrapper unsubscribe leaves the callback active | Subscription ownership on the native entity and delegate equality; do not keep wrapper-local event state. |

## Validation matrix

| Change | Required evidence |
| --- | --- |
| Managed CoreScripts or backend | C# format/style checks, CoreScripts tests, generated project build, focused native unit tests. |
| Dynamic assemblies or live compiler | `test_managed_dynamic_assemblies.py`, `test_managed_script_compiler.py`, target package/closure check, and an embedding-project runtime authorization and execution test. |
| Synchronization failures | `FOnline.Sync.Tests.csproj`, analyzer tests, and the embedding-project subscriber/log behavior. |
| Generated API shape or native export | Codegen, managed baker, generated diff, API contract diff, both backend tests where the contract is shared. |
| Attribute, event, callback, timer, or named call | Managed reflection/registration test plus the owning native/runtime dispatch. |
| Async scheduler | `test_managed_async_callbacks.py`, backend-isolation/frame-pump tests, and an embedding-project awaited gameplay path. |
| Entity-cover contract | Roslyn analyzer tests, warning-free managed build, and the owning synchronized server behavior. |
| Runtime/cache/thread attachment | Managed baker/backend native tests plus repeated multi-instance startup/shutdown. |
| Indexed ABI, callback adapters, or wrapper caches | `Test_ManagedScriptBaker`, aligned-frame/native backend tests, `InteropProbe.VerifyTransports`, allocation counters, and the exact target runtime. |
| Package or updater | Runtime-payload and packaging tests, exact target package inspection, startup from the packaged artifact, and update replacement. |
| Platform claim | Configure/build, target payload, process/device/browser smoke, and project acceptance for that platform. |

At minimum, run the focused Python managed suites under `BuildTools/tests/test_managed_*.py`, the analyzer test project, CoreScripts tests, `Test_ManagedScriptBaker.cpp`, the generated embedding-project unit-test target, `CompileManagedScripts`, and the affected bake/package/runtime path. A dry-run baker marker, a successful `dotnet build`, and a native process-start smoke prove different layers and must be reported separately.

## Migration from AngelScript

A source port is complete only when declarations, generated API use, lifecycle, callbacks, named calls, synchronization, tests, resource inputs, packages, and runtime evidence have all moved. Deleting `.fos` files without removing the AngelScript baker or adding the Managed pack leaves a project with no active gameplay scripts.

Preserve public metadata names, remote-call wire declarations, property layouts, persisted identifiers, and behavior unless the migration deliberately changes them. Backend-neutral metadata should not change merely because syntax changed. Run both sides during a controlled comparison only when the project has explicitly designed that lane; do not ship two handlers for the same event or remote call accidentally.

Replace AngelScript `[[ModuleInit]]`, `[[Event]]`, `[[Async]]`/`Yield`, and dynamic synchronization comments with the managed `[ModuleInit]`, `[Event]`, `Task`/`YieldAsync`, and cover-attribute/analyzer contracts. Port Engine-provided helpers by semantics, not by transliterating syntax or retaining no-op compatibility shims.

## Project documentation boundary

Every managed embedding project should document:

- selected backend options, target framework, SDK/runtime pin, configured source roots, assemblies, analyzers, and generated directory;
- project namespace and source layout, generated inputs, formatter/style gates, and how to open the generated project;
- module catalog, authority boundaries, project attributes, higher-level libraries, and mutable-state owners;
- exact compile, bake, test, package, launch, browser/device, and update commands;
- migration status, intentionally unsupported shapes, platform qualification, and operational rollback.

The Engine guide defines reusable behavior; a project's documentation must say how that behavior is configured and proven in that repository.

## Maintenance triggers

Reconcile this page and its Russian mirror when any of these change:

- `FO_MANAGED_SCRIPTING`, managed CMake targets, toolchain setup, or generated project structure;
- `ManagedScript.*` settings or `ManagedScriptBaker` discovery/output;
- CoreScript attributes, marshalling shapes, generated wrappers, events, remote calls, or named invocation;
- continuation scheduling, thread attachment, load-context isolation, exception accounting, or shutdown;
- synchronization-cover attributes or analyzer diagnostics;
- runtime payload composition, cache restoration, packaging, updater substitution, or platform support.

Also regenerate affected CMake, helper-CLI, package, API, public-contract, translation, site, search, and agent-delivery artifacts in their documented dependency order.

## Source paths inspected

- `Source/Common/ScriptSystem.*`
- `Source/Common/Settings.inc`
- `Source/Scripting/Managed/`
- `Source/Tools/ManagedScriptBaker.*`
- `Source/Applications/ManagedScriptBakerApp.cpp`
- `BuildTools/cmake/stages/Init.cmake`
- `BuildTools/cmake/stages/Applications.cmake`
- `BuildTools/cmake/stages/ScriptsAndBaking.cmake`
- `BuildTools/cmake/stages/ThirdParty.cmake`
- `BuildTools/cmake/stages/Packages.cmake`
- `BuildTools/managed_runtime_payload.py`
- `BuildTools/package.py`
- `BuildTools/tests/test_managed_*.py`
- `Source/Tests/Test_ManagedScriptBaker.cpp`
