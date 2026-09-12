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

The `Script.ManagedScript*` settings define the generated project:

| Setting | Contract |
| --- | --- |
| `ManagedScriptAssemblies` | Logical entry assemblies to build. |
| `ManagedScriptProjectName` | Base name for the generated solution and project. |
| `ManagedScriptTargetFramework` | Target framework passed to the generated SDK-style project. |
| `ManagedScriptMsBuild` | Command used to build the generated project. |
| `ManagedScriptDirs` | Source roots scanned for top-level `.cs` files; normally Engine CoreScripts plus project scripts. |
| `ManagedScriptGeneratedDir` | Optional generated-project directory; empty selects the build tree's `GeneratedSource/Managed`. |
| `ManagedScriptExtraSources` | Additional `assembly,target,path` inputs. |
| `ManagedScriptExtraReferences` | Additional `assembly,target,reference` inputs. |
| `ManagedScriptAnalyzers` | Roslyn analyzer projects included in the generated build. |
| `ManagedScriptBakerDryRun` | Structural baker mode for tests; it does not prove executable assemblies. |

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

## Async and continuation scheduling

`Game.YieldAsync(milliseconds)` is the managed equivalent of a script suspension. It completes from an Engine time event and resumes through the backend-owned `ScriptSynchronizationContext`. Do not use `async void`; return `Task` or `Task<T>` so the Engine can observe completion and faults.

Each backend owns a separate continuation queue. `BaseEngine::FrameAdvance` pumps only its backends after releasing the frame-property lock. Every resumed continuation re-enters the owning Engine through `RunScriptContext`; on the server this creates a fresh synchronization context. A newly posted continuation waits for a later frame, so a yielding loop cannot monopolize one frame.

Synchronous native-result callbacks and module initialization use a private continuation queue and drain only their own awaited continuations. `Game.YieldAsync` is rejected in that context because the blocked caller cannot advance the timer pump. Completed tasks remain valid.

`ConfigureAwait(false)`, `Task.Run`, and manually dispatched ThreadPool work deliberately bypass the Engine synchronization context. They may perform isolated computation, but they must not call Engine APIs. Return to the captured Engine context before touching Engine state.

## Server entity synchronization

The server cover contract is backend-neutral: script callers must cover every existing entity the native call graph can read or mutate. An `await` ends the old cover; re-resolve or revalidate retained entities and reacquire cover before reuse.

Managed scripts declare and prove this contract with:

- `[RequiresCover]` on a parameter or method receiver;
- `[ProvidesCover]` on a parameter or return value that establishes cover;
- `[PreservesCover]` on an awaitable helper that restores the caller's cover;
- `CoverReach.Parent`, `Ancestors`, and `DestroyGraph` for transitive requirements;
- the `Sync` CoreScript helpers as the only normal wrappers around raw `Game.Sync`, `SyncRelease`, `Lock`, and `Unlock`.

The Roslyn analyzer reports invalid annotations (`FOSYNC001`), unsatisfied transitive cover (`FOSYNC002`), missing entry-point declarations (`FOSYNC003`), cover probing instead of acquisition (`FOSYNC004`), raw synchronization calls outside the helper (`FOSYNC005`), leaked singleton locks (`FOSYNC006`), locks held across `await` (`FOSYNC007`), and cover use not re-proved after `await` (`FOSYNC009`). Configure the analyzer through `ManagedScriptAnalyzers` and treat its warnings as build failures.

Attributes state a proof; they do not lock anything. Entry points annotate the entity the Engine already synchronized. Ordinary helpers either acquire the required cover or propagate `[RequiresCover]` to their callers.

## Values, collections, properties, and lifetime

The bridge converts supported primitives, enums, strings, `hstring`, value types, entities, ref types, lists, dictionaries, delegates, mutable arguments, and return values through Engine metadata. Value-type storage is constructed field by field; it is not a raw byte cast of a C++ aggregate.

Generated entity properties are native-backed. Dynamic ref types are managed DTOs whose values are materialized from or assigned to native property storage. A getter returns detached structured state; persist a mutation with read-modify-reassign unless the generated member itself is a live wrapper.

Native ref types are explicit borrowed wrappers. If a project keeps one beyond the call/frame that returned it, follow the generated `__AddRef()`/`__Release()` contract. Factory-backed wrappers start with a reference that must be released after ownership is transferred or detached.

`hstring` literals are interned through the active backend's Engine metadata. Static managed fields initialize separately in every load context; there is no process-wide hash fallback shared by engine instances.

## Runtime loading, isolation, and shutdown

Mono is initialized once for the process. Native Engine threads attach to the root domain through bounded attachment records, including long-lived frame workers and the Web interpreter thread. A backend then creates its own non-collectible `AssemblyLoadContext`; this is the per-engine isolation boundary because the embedded runtime does not provide usable classic AppDomain unload.

At startup, baked assemblies are restored into content-hashed subdirectories under the writable `Cache/ManagedAssemblies/` root. Existing byte-identical files are reused, so concurrent in-process engine instances do not rewrite an assembly Mono already loaded. Missing managed assemblies are a supported empty-backend state for tests/tools that do not bake scripts; a configured gameplay project should treat that as a packaging or resource-selection failure.

Shutdown closes the continuation scheduler and discards queued work before releasing backend state. Later posts cannot run against a disposed engine. Managed exceptions are counted and logged through the common script exception path; deferred task faults are observed once.

## Build and bake workflow

The generated CMake target `CompileManagedScripts` runs the standalone `<ProjectDevName>_ManagedScriptBaker`. It depends on `ForceCodeGeneration`, loads the project configuration, prepares metadata, generates the managed API/project, and compiles target assemblies without a full resource bake.

`BakeResources` and `ForceBakeResources` run the `Managed` baker as part of the selected resource pack. Use the compile target for a fast source/API check and the bake target for the real resource, assembly, runtime-payload, and metadata contract. After a force bake, run an ordinary incremental bake and require it to settle cleanly.

The runtime toolchain is prepared by `SetupManagedRuntime`; `PrepareManagedRuntimePayload` produces the deployable subset and a `runtime.manifest`. Toolchain setup runs with an isolated environment so a workstation's `DOTNET_*`, NuGet, or SDK state does not silently redefine the published runtime.

## Packaging and updating

The prepared runtime contains managed class libraries required by the target, including `System.Private.CoreLib.dll`; native runtime libraries, JIT binaries, headers, import libraries, and symbols are excluded from the resource payload. Mono and the generated native interop table remain linked into the application.

The Managed baker places the prepared runtime under `ManagedRuntime/` in the same resource pack as the game assemblies. Client packaging rebuilds that pack from the runtime payload belonging to the exact application target. Server packaging stages a target-specific copy for every distributed client target under `PlatformBinaries/<target>/`; the updater substitutes that copy for the common pack when serving that target.

Runtime startup restores the selected payload atomically to `<CacheDir>/ManagedRuntime/<content-hash>/`, adds its class-library directory to Mono's search path, and uses the cached payload as the source of truth. Unpackaged native development binaries also receive the prepared payload beside the executable; packaged native, Web, and Android applications use the resource-pack copy.

The embedded payload defaults to invariant globalization because `System.Globalization.Native` is not shipped. A project that supplies and qualifies its own globalization native library may override the environment before runtime startup.

## Platforms and sanitizers

Managed scripting is wired for Windows, Linux, Android, WebAssembly, macOS, and iOS build paths, but an Engine source-capable path is not a project release claim. Qualify every shipped target with the exact project resource pack, assemblies, runtime payload, startup, callbacks, async work, shutdown, packaging, and update route.

Web uses the Mono interpreter plus Engine JavaScript scheduling/entropy glue; keep its interpreter thread attached until teardown. Android and Apple targets use target-specific runtime archives and class libraries. Never reuse one target's prepared payload for another target or architecture.

MemorySanitizer and ThreadSanitizer configurations are rejected with `FO_MANAGED_SCRIPTING`: embedded Mono and generated/JIT code cannot satisfy those instruments and otherwise report false failures. AddressSanitizer and the supported undefined/data-flow combinations still require the project's actual managed build and runtime checks.

## Diagnostics and debugging

The managed backend reports fixed native context plus managed exception text and stack information through the common script error path. A build that merely produces assemblies does not prove startup or callback dispatch.

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

## Validation matrix

| Change | Required evidence |
| --- | --- |
| Managed CoreScripts or backend | C# format/style checks, CoreScripts tests, generated project build, focused native unit tests. |
| Generated API shape or native export | Codegen, managed baker, generated diff, API contract diff, both backend tests where the contract is shared. |
| Attribute, event, callback, timer, or named call | Managed reflection/registration test plus the owning native/runtime dispatch. |
| Async scheduler | `test_managed_async_callbacks.py`, backend-isolation/frame-pump tests, and an embedding-project awaited gameplay path. |
| Entity-cover contract | Roslyn analyzer tests, warning-free managed build, and the owning synchronized server behavior. |
| Runtime/cache/thread attachment | Managed baker/backend native tests plus repeated multi-instance startup/shutdown. |
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
- `Script.ManagedScript*` settings or `ManagedScriptBaker` discovery/output;
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
