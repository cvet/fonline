---
layout: default
title: Scripting
locale: en
document_id: scripting-runtime
permalink: /Docs/en/explanation/scripting-runtime/
---

# Scripting

> Engine-owned documentation. This page describes reusable scripting runtime behavior in `Source/Common/ScriptSystem.*` and `Source/Scripting/`; concrete game scripts, quests, rules, and content policy belong to the embedding project.

## Purpose

The scripting layer is the contract between the C++ engine runtime and game-authored behavior. It exposes engine entities, global services, events, remote calls, value types, collections, reflection helpers, and tool/frontend helpers to script code while keeping C++ ownership, metadata, nullability, persistence, networking, and validation in the engine.

Read this page together with:

- [Managed C# Scripting](../../how-to/scripting/managed-csharp.md) for the complete managed authoring, generated-project, async, synchronization, runtime, packaging, platform, diagnostics, and migration contract.
- [AngelScript Style and Refactoring](../../how-to/scripting/style-and-refactoring.md) for module construction, source layout, formatter behavior, generated-file discipline, refactoring batches, and validation gates.
- [GeneratedApiAndMetadata.md](../../reference/metadata/index.md) for generated metadata, `///@` annotations, and codegen output.
- [Script Lifecycle and Concurrency](../../how-to/scripting/lifecycle-and-concurrency.md) for module initialization, callback ownership, `[[Async]]`, `Yield`, server synchronization covers, mutable-state ownership, and teardown rules.
- [Remote Calls](../../reference/scripting/remote-calls.md) for remote-call grammar, direction, handlers, authority, project catalog generation, and validation.
- [Nullability.md](../../../Nullability.md) for `T?` (script) and `ptr<T>`·`nptr<T>` (native) contracts across script/native boundaries.
- [Entity Model](../entity-and-property-model/) for entity, prototype, property, and holder concepts exposed to scripts.
- [Server Runtime](../runtime/server.md) and [Client Runtime](../runtime/client.md) for runtime events and script callback ownership.
- [Mapper Tools](../../how-to/tools/mapper.md) for mapper-specific script helpers.
- [Script Methods Map](../../reference/script-api/method-ownership.md) for the native script method file map.
- [Text and Localization](../../how-to/content/text-and-localization.md) for `TextPackKey`, `LanguageName`, `Game.GetText`, language switching, and the boundary from project-owned lexem formatting.

## Source paths inspected

- `Source/Common/ScriptSystem.h`
- `Source/Common/ScriptSystem.cpp`
- `Source/Scripting/AngelScript/AngelScriptScripting.h`
- `Source/Scripting/AngelScript/AngelScriptScripting.cpp`
- `Source/Scripting/AngelScript/AngelScriptBackend.h`
- `Source/Scripting/AngelScript/AngelScriptBackend.cpp`
- `Source/Scripting/AngelScript/AngelScriptAttributes.cpp`
- `Source/Scripting/AngelScript/AngelScriptCall.cpp`
- `Source/Scripting/AngelScript/AngelScriptEntity.cpp`
- `Source/Scripting/AngelScript/AngelScriptGlobals.cpp`
- `Source/Scripting/AngelScript/AngelScriptRemoteCalls.cpp`
- `Source/Scripting/AngelScript/AngelScriptReflection.cpp`
- `ThirdParty/AngelScript/sdk/angelscript/source/as_compiler.cpp`
- `Source/Scripting/*ScriptMethods.cpp`
- `Source/Scripting/Managed/CoreScripts/*.cs`
- `Source/Scripting/Managed/ManagedScripting.*`
- `Source/Scripting/Managed/ManagedScriptBackend.*`
- `Source/Scripting/Managed/ManagedInteropAbi.*`
- `Source/Scripting/Managed/ManagedRuntime.*`
- `Source/Scripting/Managed/ManagedHost/ManagedLoadContextHost.cs`
- `Source/Tools/ManagedScriptBaker.*`
- `Source/Scripting/Native/.keepalive`
- `BuildTools/cmake/stages/ScriptsAndBaking.cmake`
- `Source/Tests/Test_AngelScriptAttributes.cpp`
- `Source/Tests/Test_AngelScriptBaker.cpp`
- `Source/Tests/Test_AngelScriptBytecode.cpp`
- `Source/Tests/Test_AngelScriptCall.cpp`
- `Source/Tests/Test_ManagedScriptBaker.cpp`
- `Source/Tests/Test_ClientDataValidation.cpp`
- `Source/Tests/Test_CommonScriptMethods.cpp`
- `Source/Tests/Test_EntityLifecycle.cpp`
- `Source/Tests/Test_EntitySync.cpp`
- `Source/Tests/Test_MetadataBaker.cpp`
- `Source/Tests/Test_NetBuffer.cpp`
- `Source/Tests/Test_ScriptBuiltins.cpp`
- `Source/Tests/Test_ScriptEntityOps.cpp`
- `Source/Tests/Test_ServerScriptMethods.cpp`

## Layer map

The scripting subsystem has four layers:

1. **Common runtime facade** — `Source/Common/ScriptSystem.h` / `.cpp` define the backend-agnostic `ScriptSystem`, `ScriptFuncDesc`, `ScriptFunc`, `FuncCallData`, `DataAccessor`, native call adapters, init functions, loop callbacks, and type maps.
2. **Backend implementations** — `Source/Scripting/AngelScript/` provides the AngelScript compiler/runtime and `Source/Scripting/Managed/` provides the Managed C# compiler/runtime bridge hosted on embedded Mono. Both are implemented and tested backends. `Source/Scripting/Native/` is only a reserved source root and must not be presented as operational.
3. **Script-visible native methods** — `Source/Scripting/*ScriptMethods.cpp` files contain `///@ ExportMethod` functions grouped by runtime side and receiver type. Codegen reads these annotations and emits method descriptors/wrappers.
4. **Core library and game scripts** — `Source/Scripting/Managed/CoreScripts/*.cs` provides the reusable managed bridge library. High-level gameplay, GUI, and other project libraries belong to the embedding project for both languages; AngelScript no longer has an Engine-owned `CoreScripts` library. Projects select `.cs` and/or `.fos` sources through configuration and script/resource baking.

The engine owns the reusable bridge. The embedding project owns game scripts and chooses which features are enabled through project configuration, build presets, and `.fomain` inputs.

## `ScriptSystem`: backend-neutral dispatch

`ScriptSystem` is the C++ runtime facade used by client, server, mapper, tests, and script-aware tools. Its main jobs are:

- register one or more `ScriptSystemBackend` instances with `RegisterBackend()`;
- map C++ types to metadata descriptors with `MapScriptTypes()` and `MapEngineType()` / `MapEngineDictType()`;
- initialize modules with `InitModules()`;
- find and invoke global functions through `FindFunc()`, `CheckFunc()`, `CallFunc()`, and `CallAdminFunc()`;
- store `ScriptFuncDesc` entries from backends with `AddGlobalScriptFunc()`;
- run registered init functions and loop callbacks through `AddInitFunc()`, `AddLoopCallback()`, and `ProcessScriptEvents()`.

`ScriptFunc<TRet, Args...>` normalizes native arguments into `FuncCallData` and catches script exceptions so callers can continue after a failed script callback. It retains return-value cleanup state only for non-void return types; void callbacks have no return storage to clean up when delayed callbacks are moved or destroyed during entity teardown. `NativeDataProvider` and `NativeDataCaller` adapt C++ arrays, dictionaries, entities, callbacks, value types, and mutable references to the generic call representation.

This boundary is also where generated nullability checks are inserted. `NativeDataProvider::CheckArgNotNull()` and `CheckReturnNotNull()` are called by codegen-generated `MethodDesc::Call` lambdas, not only by the AngelScript adapter. See [Nullability.md](../../../Nullability.md) for the full contract.

## AngelScript runtime path

`InitAngelScriptScripting()` in `Source/Scripting/AngelScript/AngelScriptScripting.cpp` prepares the AngelScript runtime, creates an `AngelScriptBackend`, registers it at `ScriptSystemBackend::ANGELSCRIPT_BACKEND_INDEX`, and loads binary scripts from resources.

`CompileAngelScript()` is the compiler-side entry point used by tools/tests. It creates a standalone `ScriptSystem`, registers metadata, compiles text script files, and returns bytecode.

`AngelScriptBackend` owns the concrete engine instance and module lifecycle:

- `RegisterMetadata()` binds engine metadata and registers C++/script-visible types.
- `BindRequiredStuff()` registers arrays, dictionaries, strings, math/value types, globals, entity wrappers, remote callers, reflection helpers, and backend helpers.
- `CompileTextScripts()` preprocesses script source, adds script sections to a module, resolves includes, builds the module, and serializes bytecode.
- `LoadBinaryScripts()` loads compiled bytecode from resources at runtime.
- `SetMessageCallback()` / `SendMessage()` route compiler/runtime diagnostics to the caller. AngelScript diagnostic locations keep the original script line but format only the source file name, not the full source path, so logs remain stable across local and CI workspaces.
- cleanup callbacks and post-cleanup callbacks release backend-owned resources in a controlled order.

AngelScript is therefore used in two modes: compile-time tooling mode and runtime mode. The same metadata and type registration code must remain compatible with both.

Runtime overrun diagnostics use `Script.OverrunReportTime` as an independent threshold for two measurements. `Script execution overrun` reports wall time after subtracting the server synchronization context's accumulated entity-lock wait, while `Script lock wait overrun` reports the contention component itself. Both messages include execution, lock-wait, and total wall durations, so a compute-heavy function and a wait-heavy function stay separately searchable without losing the full latency picture. Non-server engines return zero lock wait. A value of zero still disables both diagnostics, and an attached debugger still suppresses them.

Managed entries use the matching `ManagedScript.OverrunReportTime` threshold and the same two message shapes. Both backends suppress these diagnostics while `BaseEngine::IsStartingUp()` is true. Every Engine starts in that state and calls `FinishStartingUp()` exactly once when it begins serving: the server at the end of `InitDoneJob`, the client at the end of `ClientEngine` construction, and Mapper at the end of `MapperEngine` construction because it continues initialization after the shared client constructor. There is no setter or transition back to start-up; a second finish is a duplicated start path and throws. One-time shader, font, GUI, model, or similar loading during construction is therefore not reported as a responsiveness failure before anything can be waiting on a frame.

Profiling builds also expose Managed C# method and JIT zones through Mono. See [Managed C# diagnostics](../../how-to/scripting/managed-csharp.md#diagnostics-and-debugging) for the instrumentation contract and [Profiling](../../how-to/quality/profiling.md#managed-script-zones) for capture interpretation.

AngelScript assigns registered object type IDs lazily. Separate script contexts may request the same fresh type concurrently, so the vendored runtime reads and initializes `asCTypeInfo::typeId` under the engine reader/writer lock and refreshes the value after acquiring exclusive access. `AngelScriptTypeIdsAreLazilyAssignedAcrossThreads` drives 16 native workers over 128 new types through public `asITypeInfo::GetTypeId()` and requires one identical valid ID per type.

Native methods registered through generated `MethodDesc` descriptors are invoked through `ScriptGenericCall()`.
The unified `FuncCallData` slot for a mutable simple argument is the **address of the caller's variable** — the
value itself for primitives/enums/value types (`int32&`, `mpos&`, `string&`), the handle cell for object handles
(`Critter@&`). Every AngelScript-side producer follows this contract: `ScriptGenericCall()` (classifying by the
registration-time `MethodDesc`/`EntityEventDesc` argument descriptors — the same data that emitted the `&`/`@&`
declaration) and the `Invoke` family resolve mutable arguments through `asIScriptGeneric::GetArgAddress()` (the
pointer held on the stack), while ordinary input arguments use `GetAddressOfArg()`. Consumers rely on it
symmetrically:
`NativeDataCaller::ConvertArg`/`ReturnArg` read and write back through the slot, and the AngelScript-to-
AngelScript branch of `ScriptFuncCall()` (script-fired events with by-ref args, `Invoke` targeting a script
function) passes the slot straight to `asIScriptContext::SetArgAddress()`. Regression coverage:
`Test_CommonScriptMethods.cpp` (`TimePackingOperations`, `GameInvokeOperations/ByNameWithRefArgs`) and
`Test_ScriptEntityOps.cpp` (`AdvancedServerOperations/CustomEntityEventRefArgs`).

When `asEP_ALLOW_UNSAFE_REFERENCES` is enabled, AngelScript may defer releasing method receivers and
arguments until an expression reaches a safe point. Short-circuit boolean compilation processes the
left operand's deferred parameters after materializing its primitive `bool` result and before merging
the branch bytecode. Otherwise the right operand can reuse a temporary object slot and overwrite the
retained receiver without releasing it. `ScriptBuiltinsDeferredReceiverTemporaryIsReleased` covers
the property-accessor plus method-call form that exposed this during GUI shutdown.

### AngelScript backend shutdown

`~AngelScriptBackend()` tears the runtime down in a fixed order: stop the debugger endpoint, run the registered cleanup callbacks, reset the context manager, then call `asIScriptEngine::ShutDownAndRelease()` while script modules, object types, behaviours, and backend links are still intact. The AngelScript shutdown path calls every module's `CallExit()`, uninitializes global variables, runs repeated full GC passes until the live set is empty or no longer makes progress, discards modules, and reports any object that still cannot be destroyed. There is no fixed pass limit: script destructors may create another finite collectable graph that needs a subsequent pass. After the engine is released, the backend resets `_meta` / `_scriptSys` / `_engine` / `_entityMngr` and runs post-cleanup callbacks.

Global variables, delegates, script object handles, arrays, dictionaries, and GUI object graphs must be cleaned by module shutdown, destructors, `ReleaseAllHandles`, and the AngelScript GC. Embedding-project scripts should not add `Game.OnFinish` / `EngineCallback_Finish` cleanup just to silence shutdown diagnostics; if a graph survives shutdown, fix the owning native release/GC enumeration bug.

Entity deletion/unload clears the entity's own event callbacks and time events from `Entity::MarkAsDestroyed()`, so embedding-project scripts should not keep central per-entity unsubscribe / `StopTimeEvent` registries for ordinary entity lifetime. Entity mutators and event/time-event entry points assert or verify when called after `MarkAsDestroyed()`, making accidental attempts to repopulate a destroyed entity show their stack trace at the offending call. During `ServerEngine::Shutdown` / `ClientEngine::Shutdown`, the engine also runs `UnsubscribeAllEvents()` + `ClearAllTimeEvents()` on the global engine entity and all live entities before `DestroyAllEntities()`. Embedding-project scripts should not hand-maintain unsubscribe / global-clear / `StopTimeEvent` cleanup in their `Game.OnFinish` handler purely to keep the GC quiet — only genuinely functional teardown belongs there.

Destroyed entities are rejected at the script-to-native boundary too. `NativeDataCaller::ConvertArg` validates access first and then rejects a destroyed entity for every ordinary `///@ ExportMethod`. On the server, an uncovered destroyed handle therefore reports the actionable missing-cover fault; a still-covered handle that the caller destroyed and reused reports the destroyed-argument fault. The client has no cover validation and sees only the latter.

The sole opt-out is `///@ ExportMethod ... AllowDestroyedEntityArgs`, emitted as a compile-time call-policy flag. It exists for explicit synchronization primitives such as `Game.Sync`: a concurrent destroy can always happen between a script liveness check and the synchronization call, and these wrappers must return `false` rather than fail at argument conversion. Do not apply the flag to ordinary exports. `ServerEngineDestroyedEntityArgumentReportsMissingCoverFirst` and `SyncAcceptsDestroyedEntity` pin the two contracts.

## Attributes, declarations, and metadata

`Source/Scripting/AngelScript/AngelScriptAttributes.cpp` parses engine-specific script attributes and declaration tags. Important contracts include:

- nullable `T?` suffix stripping and propagation into metadata;
- `///@ Event` declarations and matching `[[Event]]` handlers;
- `///@ RemoteCall` declarations, optional structural `MaxBytes N` / `MaxCollectionSize N` limits, and matching `[[ServerRemoteCall]]` or `[[ClientRemoteCall]]` implementations;
- the separate `[[AdminRemoteCall]]` command entry point;
- module/init-function priorities;
- callback attribute validation rules;
- `[[InvokeEntry]]` for functions dispatched only by name through the global `Invoke(...)` helper. It blocks ordinary direct calls while still allowing a function reference for `NameOf(...)` registration.

These attributes are source-level contracts. AngelScript sees normalized declarations after preprocessing, while engine metadata and analyzers retain the higher-level FOnline-specific meaning.

The complete authoring and runtime model for `[[ModuleInit]]`, callback-only attributes, transitive `[[Async]]`, `Yield`, server `Game.Sync` / `Game.Lock`, state ownership, and callback teardown is in [Script Lifecycle and Concurrency](../../how-to/scripting/lifecycle-and-concurrency.md). Keep this page focused on subsystem composition and use that guide for lifecycle-sensitive script design.

## Entities and properties in scripts

`Source/Scripting/AngelScript/AngelScriptEntity.cpp` registers script object types for engine entities, singleton-like components, property accessors, entity event types, and method dispatch. It bridges generated metadata with AngelScript registration calls so script code can work with engine entities through script-visible names such as critters, items, maps, locations, players, prototypes, abstracts, statics, holders, and property-backed components.

Entity lifetime is still owned by the engine runtime:

- server scripts work against authoritative entities owned by `ServerEngine` and managers;
- client scripts work against view/client entities owned by `ClientEngine`;
- mapper scripts work against mapper-owned editor state;
- script handles must not be treated as persistence ownership.

Use [Entity Model](../entity-and-property-model/) for entity/property/prototype ownership and [Persistence](../persistence/) for database boundaries.

## Remote calls and event callbacks

`Source/Scripting/AngelScript/AngelScriptRemoteCalls.cpp` registers remote caller object types such as `RemoteCaller` and `CritterRemoteCaller`. Remote-call declarations are metadata-backed, and runtime handling is split by side:

- server-side command processing validates client-originated remote calls before invoking server script handlers;
- client-side runtime receives server-originated remote calls and dispatches client script handlers;
- admin remote calls use the `CallAdminFunc()` path and require the `AdminRemoteCall` attribute.

For an untrusted client-to-server call, author `MaxBytes` as the largest legitimate serialized payload and `MaxCollectionSize` as the largest legitimate declared collection. The server resolves the descriptor before body allocation, and native validation plus AngelScript decoding enforce the collection limit before reserve or construction, including nested dictionary arrays. The server-wide `ServerNetwork.MaxRemoteCallPayloadSize` remains a separate hostile-input ceiling. See [Remote Calls](../../reference/scripting/remote-calls.md) for the declaration, baked-metadata, and compatibility contract.

Events and remote calls are intentionally separate concepts. Events describe engine/runtime lifecycle and gameplay notifications; remote calls describe network-addressable script entry points. Both rely on metadata signatures, nullability contracts, and generated descriptors.

The complete authoring, caller-surface, namespace, security, baked-catalog, and compatibility contract is in [Remote Calls](../../reference/scripting/remote-calls.md).

## Native script method exports

Native script APIs are grouped by file name:

- `Common*ScriptMethods.cpp` — APIs shared by multiple sides, including global helpers and ImGui wrappers.
- `Server*ScriptMethods.cpp` — authoritative server APIs for game creation, persistence, movement, entity mutation, and player/critter/map/item/location operations.
- `Client*ScriptMethods.cpp` — client/view APIs for UI, resources, rendering-facing map operations, visible critters/items, audio/video, input, and local state.
- `Mapper*ScriptMethods.cpp` — mapper/editor APIs for creating, moving, selecting, saving, and organizing map entities.

Each exported function is marked with `///@ ExportMethod` and normally starts with a side/type prefix such as `Server_Map_`, `Client_Game_`, `Common_ImGui_`, or `Mapper_Game_`. Codegen turns these declarations into script-visible method descriptors and backend call wrappers. Trailing C++ default parameters are preserved in metadata and restored in the AngelScript registration declarations, with C++ value-type defaults such as `fpos32 {}` normalized to script expressions such as `fpos()`. Prefer a single exported method with defaults over duplicate overloads that only append optional arguments. See [Script Methods Map](../../reference/script-api/method-ownership.md) for the per-file map and counts.

For entity instance methods, the AngelScript dispatch layer validates the receiver before entering the native method body. `Entity_MethodCall` calls `CheckScriptEntityAccessAndNonDestroyed`, which checks server sync coverage and destroyed state for the `self` entity. Do not add an entry-only `ValidateEntityAccess(self)` or repeat the receiver check before ordinary receiver reads. Later in the body, validate entities only at real access/assert boundaries such as event dispatch or post-reentry continuation. When a covered entity must keep its own lock across a detach or reparent, use the cover-retaining, idempotent `EnsureEntitySynced(...)`; it retains existing caller cover — never releasing or parking on it — and cannot acquire an omitted dependency.

When adding a method, route it to the side that owns the state it mutates. For example, authoritative item creation belongs under server methods, while sprite/UI helpers belong under client/common frontend methods.

AngelScript stores a `bool` value in one byte of a four-byte VM stack slot, whose upper bytes may retain earlier data. The patched native-call marshalling paths for x64 GCC, x64 MSVC, and ARM64 zero the destination argument slot and copy only the value type's in-memory bytes. Native callees may therefore rely on an incoming `bool` register being normalized to `0` or `1`; `AngelScriptNativeCallNormalizesBoolArgument` in `Source/Tests/Test_AngelScriptAlignment.cpp` pins this ABI boundary.

`Gui::RegisterScreen` precaches each screen inside a try/catch, so one window that
cannot be built no longer costs the registrations behind it: the failing screen keeps
its creator, the rest register normally, and `Gui::VerifyScreensInitialized()` then
raises a single `verify` naming every window that failed. `Gui::IsScreenRegistered`
answers whether a creator is stored, and the `verify` in `CreateScreen` carries the
screen's enum name as context. Note what an AngelScript `catch` does not give you: it
binds no exception object, so `GetExceptionInfo()` reports the message of the exception
the current catch block is handling, and that context is reset only when the script
context is reprepared.

Text lookup follows the same side ownership. Client/mapper scripts can retrieve
strings and change language; server scripts expose only text presence and
variant counts. The complete behavioral contract and missing-data semantics are
in [Text and Localization](../../how-to/content/text-and-localization.md).

Client render helpers such as `Game.DrawSprite`, `Game.DrawSpritePattern`, and `Game.DrawSpriteRegion` are valid only during render-facing script callbacks (`RenderIface` / GUI draw callbacks). `Game.DrawSpriteRegion(sprId, uv0, uv1, pos, size, color)` draws a normalized `[0, 1]` sub-rectangle of the sprite's original logical image into a destination rectangle; polygon-cropped atlas frames are remapped through their source offset and transparent cropped margins remain transparent in the destination. `Game.DrawSpritePattern` follows the same logical-image contract for every complete or partial tile. Region drawing is intended for reusable GUI composition such as script-side 9-slice panels, and returns `false` when the sprite cannot provide atlas-region drawing.

## Backend parity and ownership

The backend-neutral metadata and native export surface is shared, but language syntax and runtime mechanics are not interchangeable:

| Contract | AngelScript | Managed C# |
|---|---|---|
| Enablement | `FO_ANGELSCRIPT_SCRIPTING` | `FO_MANAGED_SCRIPTING` |
| Project source | project-owned `.fos` modules | project-owned `.cs` modules |
| Compile artifact | baked AngelScript bytecode | target assemblies plus generated `.gen.cs`, `.gen.csproj`, and `.gen.sln` |
| Initialization | `[[ModuleInit]] void` | `[ModuleInit]` static `void` or `Task` |
| Suspension | transitive `[[Async]]` and `Game.Yield` | `Task`, `await`, and `Game.YieldAsync` on the backend synchronization context |
| Synchronization proof | runtime cover operations and AngelScript attribute validation | `[RequiresCover]`, `[ProvidesCover]`, `[PreservesCover]`, `CoverReach`, runtime checks, and Roslyn `FOSYNC` diagnostics |
| Runtime ownership | AngelScript engine, modules, contexts, and GC | one process-wide Mono runtime plus backend-scoped load contexts, scheduler queues, handles, and managed GC roots |
| Native interop | registered AngelScript calls and VM contexts | one generated indexed ABI for scalar/value hot paths plus boxed fallback for complex values |

Managed C# is not a renamed version of the removed experimental `Source/Scripting/Mono/` path. It is a complete backend with its own baker, generated bindings, load-context host, analyzers, runtime payload, and platform wiring. See [Managed C# Scripting](../../how-to/scripting/managed-csharp.md) for its full contract. Native scripting remains a placeholder.

The Managed hot path is bound once from a shared `ManagedInteropAbi` manifest.
Methods, events, settings, and inner-entity operations use dense ids and packed
frames; entity-like arguments occupy nullable pointer slots, while plain fixed
values are copied by layout. Callback adapters, wrapper factories, reflection
lookups, and list factories are prepared at bind time. Complex strings and
collections retain the boxed path. Event subscriptions are owned by the native
entity, so an equal subscription is idempotent and can be removed through a
different wrapper for the same entity.

## Core script ownership

The Engine-owned script-side library now lives under `Source/Scripting/Managed/CoreScripts/` and contains the managed native bridge, attributes, initialization, invocation, remote-call, synchronization, async, verification, item-holder, enum, and value-type helpers required by the backend. It is infrastructure, not game policy.

The former Engine-owned AngelScript high-level library was removed. An embedding project that uses AngelScript owns its `.fos` helpers, GUI implementation, module order, and gameplay modules. Do not copy project GUI or gameplay contracts back into reusable Engine documentation.

## Build and baking flow

`BuildTools/cmake/stages/ScriptsAndBaking.cmake` wires script compilation into the project build:

- `FO_ANGELSCRIPT_SCRIPTING` enables the `CompileAngelScript` command target.
- The target runs the project AS compiler app (`${FO_DEV_NAME}_ASCompiler`) with the main config arguments.
- `CompileAngelScript` depends on `ForceCodeGeneration`, so script-visible generated metadata is current before compilation.
- `FO_MANAGED_SCRIPTING` creates the standalone `${FO_DEV_NAME}_ManagedScriptBaker`, `CompileManagedScripts`, `SetupManagedRuntime`, and `PrepareManagedRuntimePayload` integration. `CompileManagedScripts` depends on `ForceCodeGeneration` and compiles every configured resource-pack/target assembly from `ManagedScriptSourceDirs`, `ManagedScriptExtraSources`, references, and analyzers.
- `BakeResources` and `ForceBakeResources` also depend on code generation and run the project baker app.

Script compilation and resource baking are adjacent but not identical. Script compilation produces bytecode/runtime inputs; baking packages resources and metadata for runtime consumption. See [Baking Pipeline](../content-pipeline/baking.md) for resource baking.

## Managed and native scripting roots

`Source/Scripting/Managed/` is the implemented C# backend; its complete operational contract is in [Managed C# Scripting](../../how-to/scripting/managed-csharp.md). The obsolete `Source/Scripting/Mono/`, `FO_MONO_SCRIPTING`, `CompileMonoScripts`, and `BuildTools/compile-mono-scripts.py` interfaces no longer exist.

`Source/Scripting/Native/` currently contains only `.keepalive`, marking the reserved source-root location for future native scripting integration. Do not document it as implemented until runtime, build, tests, and an authoring contract exist.

## Tests to inspect

Script behavior is covered by focused tests:

- `Source/Tests/Test_AngelScriptAttributes.cpp` — attribute parsing, nullable suffix handling, events, remote calls, and callback rules.
- `Source/Tests/Test_AngelScriptBaker.cpp` — AngelScript bytecode/resource baking path.
- `Source/Tests/Test_AngelScriptBytecode.cpp` — bytecode compilation/loading behavior.
- `Source/Tests/Test_AngelScriptCall.cpp` — native/script call ABI and object-return lifetime.
- `Source/Tests/Test_ManagedScriptBaker.cpp` — generated C# API and indexed ABI, packed frames/adapters, project/assembly construction, attributes, values, properties, remotes, and diagnostics.
- `Source/Scripting/Managed/Tests/` — managed core-library bootstrap and generated-API fixtures.
- `Source/Scripting/Managed/Analyzers/Tests/` — synchronization-cover analyzer diagnostics.
- `Source/Tests/Test_ClientDataValidation.cpp` and `Test_NetBuffer.cpp` — inbound remote-call payload validation and framing.
- `Source/Tests/Test_CommonScriptMethods.cpp` — common exported methods.
- `Source/Tests/Test_EntityLifecycle.cpp` and `Test_EntitySync.cpp` — lifecycle and server synchronization boundaries.
- `Source/Tests/Test_MetadataBaker.cpp` — baked event and remote-call metadata grammar.
- `Source/Tests/Test_ServerScriptMethods.cpp` — server exported methods.
- `Source/Tests/Test_ScriptBuiltins.cpp` — built-in script helpers/types.
- `Source/Tests/Test_ScriptEntityOps.cpp` — script/entity interactions.

Use these tests as executable documentation when changing script registration, generated wrappers, method signatures, nullability, event declarations, or remote-call dispatch.

## Change routing

- Backend-neutral call ABI: `Source/Common/ScriptSystem.*`.
- AngelScript compiler/runtime lifecycle: `Source/Scripting/AngelScript/AngelScriptScripting.*` and `AngelScriptBackend.*`.
- Managed compiler/runtime/lifecycle: `Source/Tools/ManagedScriptBaker.*`, `Source/Scripting/Managed/`, and [Managed C# Scripting](../../how-to/scripting/managed-csharp.md).
- Script module construction, source conventions, formatting, generated-file ownership, and refactoring gates: [AngelScript Style and Refactoring](../../how-to/scripting/style-and-refactoring.md).
- Attribute syntax and nullable preprocessing: `Source/Scripting/AngelScript/AngelScriptAttributes.*` and [Nullability.md](../../../Nullability.md).
- Script entity/property registration: `Source/Scripting/AngelScript/AngelScriptEntity.*` plus [Entity Model](../entity-and-property-model/).
- Remote caller registration/dispatch support: `Source/Scripting/AngelScript/AngelScriptRemoteCalls.*`, [Remote Calls](../../reference/scripting/remote-calls.md), and [Networking](../authority-and-networking/).
- Reflection helpers: `Source/Scripting/AngelScript/AngelScriptReflection.*`.
- Native exported methods: `Source/Scripting/*ScriptMethods.cpp` and [Script Methods Map](../../reference/script-api/method-ownership.md).
- Build target wiring: `BuildTools/cmake/stages/ScriptsAndBaking.cmake` and [BuildTools Pipeline](../../reference/cmake-and-buildtools/pipeline.md).
- Generated metadata/codegen: [GeneratedApiAndMetadata.md](../../reference/metadata/index.md).

## Validation checklist

1. If signatures or annotations changed, regenerate code and inspect generated metadata/wrapper diffs.
2. Compile every enabled backend: `CompileAngelScript` for AngelScript and `CompileManagedScripts` for Managed C#.
3. Run the smallest affected script tests. For AngelScript start with `Test_AngelScriptAttributes`, `Test_AngelScriptBaker`, and `Test_AngelScriptCall`; for Managed C# start with `Test_ManagedScriptBaker`, managed core/analyzer tests, and affected `BuildTools/tests/test_managed_*.py`. Run shared method/entity tests for backend-neutral changes.
4. For nullable changes, run the nullability analyzers described in [Nullability.md](../../../Nullability.md).
5. For server/client/mapper method changes, validate the owning runtime path; do not rely only on compilation.
6. Update [Script Methods Map](../../reference/script-api/method-ownership.md) when exported method files are added, removed, or materially regrouped.
