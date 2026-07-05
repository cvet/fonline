# Scripting

> Engine-owned documentation. This page describes reusable scripting runtime behavior in `Source/Common/ScriptSystem.*` and `Source/Scripting/`; concrete game scripts, quests, rules, and content policy belong to the embedding project.

## Purpose

The scripting layer is the contract between the C++ engine runtime and game-authored behavior. It exposes engine entities, global services, events, remote calls, value types, collections, reflection helpers, and tool/frontend helpers to script code while keeping C++ ownership, metadata, nullability, persistence, networking, and validation in the engine.

Read this page together with:

- [GeneratedApiAndMetadata.md](GeneratedApiAndMetadata.md) for generated metadata, `///@` annotations, and codegen output.
- [Nullability.md](Nullability.md) for `T?` (script) and `ptr<T>`·`nptr<T>` (native) contracts across script/native boundaries.
- [EntityModel.md](EntityModel.md) for entity, prototype, property, and holder concepts exposed to scripts.
- [ServerRuntime.md](ServerRuntime.md) and [ClientRuntime.md](ClientRuntime.md) for runtime events and script callback ownership.
- [MapperTools.md](MapperTools.md) for mapper-specific script helpers.
- [ScriptMethodsMap.md](ScriptMethodsMap.md) for the native script method file map.

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
- `Source/Scripting/AngelScript/CoreScripts/*.fos`
- `Source/Scripting/*ScriptMethods.cpp`
- `Source/Scripting/Mono/*.cs`
- `Source/Scripting/Native/.keepalive`
- `BuildTools/cmake/stages/ScriptsAndBaking.cmake`
- `Source/Tests/Test_AngelScriptAttributes.cpp`
- `Source/Tests/Test_AngelScriptBaker.cpp`
- `Source/Tests/Test_AngelScriptBytecode.cpp`
- `Source/Tests/Test_CommonScriptMethods.cpp`
- `Source/Tests/Test_ScriptBuiltins.cpp`
- `Source/Tests/Test_ScriptEntityOps.cpp`
- `Source/Tests/Test_ServerScriptMethods.cpp`

## Layer map

The scripting subsystem has four layers:

1. **Common runtime facade** — `Source/Common/ScriptSystem.h` / `.cpp` define the backend-agnostic `ScriptSystem`, `ScriptFuncDesc`, `ScriptFunc`, `FuncCallData`, `DataAccessor`, native call adapters, init functions, loop callbacks, and type maps.
2. **Backend implementation** — `Source/Scripting/AngelScript/` provides the current production backend. Mono and native scripting have placeholder/source roots, but AngelScript owns the implemented script compiler/runtime path in this tree.
3. **Script-visible native methods** — `Source/Scripting/*ScriptMethods.cpp` files contain `///@ ExportMethod` functions grouped by runtime side and receiver type. Codegen reads these annotations and emits method descriptors/wrappers.
4. **Core script library and game scripts** — `Source/Scripting/AngelScript/CoreScripts/*.fos` provides engine-owned reusable script-side helpers. Embedding projects add their own `.fos` files and metadata through project configuration and resource/script baking.

The engine owns the reusable bridge. The embedding project owns game scripts and chooses which features are enabled through project configuration, build presets, and `.fomain` inputs.

## `ScriptSystem`: backend-neutral dispatch

`ScriptSystem` is the C++ runtime facade used by client, server, mapper, tests, and script-aware tools. Its main jobs are:

- register one or more `ScriptSystemBackend` instances with `RegisterBackend()`;
- map C++ types to metadata descriptors with `MapScriptTypes()` and `MapEngineType()` / `MapEngineDictType()`;
- initialize modules with `InitModules()`;
- find and invoke global functions through `FindFunc()`, `CheckFunc()`, `CallFunc()`, and `CallAdminFunc()`;
- store `ScriptFuncDesc` entries from backends with `AddGlobalScriptFunc()`;
- run registered init functions and loop callbacks through `AddInitFunc()`, `AddLoopCallback()`, and `ProcessScriptEvents()`.

`ScriptFunc<TRet, Args...>` normalizes native arguments into `FuncCallData` and catches script exceptions so callers can continue after a failed script callback. `NativeDataProvider` and `NativeDataCaller` adapt C++ arrays, dictionaries, entities, callbacks, value types, and mutable references to the generic call representation.

This boundary is also where generated nullability checks are inserted. `NativeDataProvider::CheckArgNotNull()` and `CheckReturnNotNull()` are called by codegen-generated `MethodDesc::Call` lambdas, not only by the AngelScript adapter. See [Nullability.md](Nullability.md) for the full contract.

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

Native methods registered through generated `MethodDesc` descriptors are invoked through `ScriptGenericCall()`.
The unified `FuncCallData` slot for a mutable simple argument is the **address of the caller's variable** — the
value itself for primitives/enums/value types (`int32&`, `mpos&`, `string&`), the handle cell for object handles
(`Critter@&`). Every AngelScript-side producer follows this contract: `ScriptGenericCall()` and the `Invoke`
family resolve `out`/`inout` arguments through `asIScriptGeneric::GetArgAddress()` (the pointer held on the
stack), while ordinary input arguments use `GetAddressOfArg()`; only references to non-handle reference-object
types keep the raw stack slot, which their accessor paths expect. Consumers rely on it symmetrically:
`NativeDataCaller::ConvertArg`/`ReturnArg` read and write back through the slot, and the AngelScript-to-
AngelScript branch of `ScriptFuncCall()` (script-fired events with by-ref args, `Invoke` targeting a script
function) passes the slot straight to `asIScriptContext::SetArgAddress()`. Regression coverage:
`Test_CommonScriptMethods.cpp` (`TimePackingOperations`, `GameInvokeOperations/ByNameWithRefArgs`) and
`Test_ScriptEntityOps.cpp` (`AdvancedServerOperations/CustomEntityEventRefArgs`).

### AngelScript backend shutdown

`~AngelScriptBackend()` tears the runtime down in a fixed order: stop the debugger endpoint, run the registered cleanup callbacks, reset the context manager, then reset the backend-owned `_engine` / `_meta` / `_scriptSys` / `_entityMngr` holders. It then calls `ReleaseScriptGlobalsAndReportGC()` **before** `asIScriptEngine::ShutDownAndRelease()` (which asserts the returned engine ref count is `0`), and finally runs the post-cleanup callbacks.

`ReleaseScriptGlobalsAndReportGC()` makes the runtime clean up after itself so embedding games do not need per-system reference cleanup at shutdown:

- It walks every module's global variables and drops the script-side reference each one holds: funcdef-typed globals (function handles / delegates) are `Release()`d, and `asOBJ_REF` object globals (handles, arrays, dictionaries, script classes) go through `ReleaseScriptObject()`; the slot is nulled so the standard module teardown skips it. Value-type object globals store their instance inline and are left to module discard (resetting them early would risk a double-free). AngelScript already releases globals during module discard inside `ShutDownAndRelease()`, but doing it explicitly here — while the engine is fully alive — lets the garbage collector collapse the now-unrooted object graphs in a normal multi-iteration cycle.
- It then runs `GarbageCollect(asGC_FULL_CYCLE)` repeatedly (up to 8 passes) until `GetGCStatistics()` reports a steady live count, because object destructors can release the last reference to further objects.
- Any GC objects still alive after that are kept by references the collector cannot reclaim (e.g. a cyclic graph). The method logs a concise `AngelScript shutdown: released N global var(s), M GC object(s) still alive:` line plus a per-type histogram so the surviving types point at the owning system. The subsequent `ShutDownAndRelease()` force-releases them; its AngelScript SDK-only "external reference" / "GC cannot destroy" diagnostics are filtered during this final shutdown phase because they duplicate the survivor summary. Runtime and compilation diagnostics are not filtered. A known residual is *shown* GUI screen graphs from the core scripts, whose cyclic refcount the collector cannot resolve — a separate, GUI-internal issue.

Entity deletion/unload clears the entity's own event callbacks and time events from `Entity::MarkAsDestroyed()`, so embedding-project scripts should not keep central per-entity unsubscribe / `StopTimeEvent` registries for ordinary entity lifetime. Entity mutators and event/time-event entry points assert or verify when called after `MarkAsDestroyed()`, making accidental attempts to repopulate a destroyed entity show their stack trace at the offending call. During `ServerEngine::Shutdown` / `ClientEngine::Shutdown`, the engine also runs `UnsubscribeAllEvents()` + `ClearAllTimeEvents()` on the global engine entity and all live entities before `DestroyAllEntities()`. Embedding-project scripts should not hand-maintain unsubscribe / global-clear / `StopTimeEvent` cleanup in their `Game.OnFinish` handler purely to keep the GC quiet — only genuinely functional teardown belongs there.

## Attributes, declarations, and metadata

`Source/Scripting/AngelScript/AngelScriptAttributes.cpp` parses engine-specific script attributes and declaration tags. Important contracts include:

- nullable `T?` suffix stripping and propagation into metadata;
- `///@ Event` declarations and matching `[[Event]]` handlers;
- `///@ RemoteCall` declarations and matching `[[ServerRemoteCall]]`, `[[ClientRemoteCall]]`, or `[[AdminRemoteCall]]` implementations;
- module/init-function priorities;
- callback attribute validation rules.

These attributes are source-level contracts. AngelScript sees normalized declarations after preprocessing, while engine metadata and analyzers retain the higher-level FOnline-specific meaning.

## Entities and properties in scripts

`Source/Scripting/AngelScript/AngelScriptEntity.cpp` registers script object types for engine entities, singleton-like components, property accessors, entity event types, and method dispatch. It bridges generated metadata with AngelScript registration calls so script code can work with engine entities through script-visible names such as critters, items, maps, locations, players, prototypes, abstracts, statics, holders, and property-backed components.

Entity lifetime is still owned by the engine runtime:

- server scripts work against authoritative entities owned by `ServerEngine` and managers;
- client scripts work against view/client entities owned by `ClientEngine`;
- mapper scripts work against mapper-owned editor state;
- script handles must not be treated as persistence ownership.

Use [EntityModel.md](EntityModel.md) for entity/property/prototype ownership and [Persistence.md](Persistence.md) for database boundaries.

## Remote calls and event callbacks

`Source/Scripting/AngelScript/AngelScriptRemoteCalls.cpp` registers remote caller object types such as `RemoteCaller` and `CritterRemoteCaller`. Remote-call declarations are metadata-backed, and runtime handling is split by side:

- server-side command processing validates client-originated remote calls before invoking server script handlers;
- client-side runtime receives server-originated remote calls and dispatches client script handlers;
- admin remote calls use the `CallAdminFunc()` path and require the `AdminRemoteCall` attribute.

Events and remote calls are intentionally separate concepts. Events describe engine/runtime lifecycle and gameplay notifications; remote calls describe network-addressable script entry points. Both rely on metadata signatures, nullability contracts, and generated descriptors.

## Native script method exports

Native script APIs are grouped by file name:

- `Common*ScriptMethods.cpp` — APIs shared by multiple sides, including global helpers and ImGui wrappers.
- `Server*ScriptMethods.cpp` — authoritative server APIs for game creation, persistence, movement, entity mutation, and player/critter/map/item/location operations.
- `Client*ScriptMethods.cpp` — client/view APIs for UI, resources, rendering-facing map operations, visible critters/items, audio/video, input, and local state.
- `Mapper*ScriptMethods.cpp` — mapper/editor APIs for creating, moving, selecting, saving, and organizing map entities.

Each exported function is marked with `///@ ExportMethod` and normally starts with a side/type prefix such as `Server_Map_`, `Client_Game_`, `Common_ImGui_`, or `Mapper_Game_`. Codegen turns these declarations into script-visible method descriptors and backend call wrappers. Trailing C++ default parameters are preserved in metadata and restored in the AngelScript registration declarations, with C++ value-type defaults such as `fpos32 {}` normalized to script expressions such as `fpos()`. Prefer a single exported method with defaults over duplicate overloads that only append optional arguments. See [ScriptMethodsMap.md](ScriptMethodsMap.md) for the per-file map and counts.

For entity instance methods, the AngelScript dispatch layer validates the receiver before entering the native method body. `Entity_MethodCall` calls `CheckScriptEntityAccessAndNonDestroyed`, which checks server sync coverage and destroyed state for the `self` entity. Do not add an entry-only `ValidateEntityAccess(self)` or repeat the receiver check before ordinary receiver reads. Later in the body, validate entities only at real access/assert boundaries such as event dispatch or post-reentry continuation. When native code first needs to extend the current cover, use `EnsureEntitySynced(...)` rather than pairing it with a redundant immediate `ValidateEntityAccess(...)`.

When adding a method, route it to the side that owns the state it mutates. For example, authoritative item creation belongs under server methods, while sprite/UI helpers belong under client/common frontend methods.

Client render helpers such as `Game.DrawSprite`, `Game.DrawSpritePattern`, and `Game.DrawSpriteRegion` are valid only during render-facing script callbacks (`RenderIface` / GUI draw callbacks). `Game.DrawSpriteRegion(sprId, uv0, uv1, pos, size, color)` draws a normalized `[0, 1]` sub-rectangle of an atlas-backed sprite into a destination rectangle; it is intended for reusable GUI composition such as script-side 9-slice panels, and returns `false` when the sprite cannot provide atlas-region drawing.

## Core scripts

The engine-owned AngelScript core library lives in `Source/Scripting/AngelScript/CoreScripts/` and includes reusable modules such as:

- `Core.fos`
- `Math.fos`
- `Time.fos`
- `Color.fos` (`namespace Color`, `Color::Text`, `Color::Neutral`)
- `Input.fos`
- `Gui.fos`
- `Sprite.fos`
- `LineTracer.fos`
- `Serializator.fos`
- `MapperCore.fos`
- `FixedDropMenu.fos`
- `Tween.fos`

Treat these files as engine library code. Game-specific script modules should live in the embedding project instead of expanding the engine core script library with project policy.

## Build and baking flow

`BuildTools/cmake/stages/ScriptsAndBaking.cmake` wires script compilation into the project build:

- `FO_ANGELSCRIPT_SCRIPTING` enables the `CompileAngelScript` command target.
- The target runs the project AS compiler app (`${FO_DEV_NAME}_ASCompiler`) with the main config arguments.
- `CompileAngelScript` depends on `ForceCodeGeneration`, so script-visible generated metadata is current before compilation.
- `FO_MONO_SCRIPTING` wires `CompileMonoScripts` through `BuildTools/compile-mono-scripts.py` and `FO_MONO_ASSEMBLIES` / `FO_MONO_SOURCE`.
- `BakeResources` and `ForceBakeResources` also depend on code generation and run the project baker app.

Script compilation and resource baking are adjacent but not identical. Script compilation produces bytecode/runtime inputs; baking packages resources and metadata for runtime consumption. See [BakingPipeline.md](BakingPipeline.md) for resource baking.

## Mono and native scripting roots

`Source/Scripting/Mono/` contains C# support files such as `AssemblyInfo.cs`, `BasicTypes.cs`, `Entity.cs`, `Initializator.cs`, `MapSprite.cs`, and `Link.xml`. BuildTools can wire Mono compilation when `FO_MONO_SCRIPTING` is enabled.

`Source/Scripting/Native/` currently contains `.keepalive`, marking the source-root location for native scripting integration. Do not document Native or Mono as equivalent to the AngelScript runtime unless the implementation and tests are expanded.

## Tests to inspect

Script behavior is covered by focused tests:

- `Source/Tests/Test_AngelScriptAttributes.cpp` — attribute parsing, nullable suffix handling, events, remote calls, and callback rules.
- `Source/Tests/Test_AngelScriptBaker.cpp` — AngelScript bytecode/resource baking path.
- `Source/Tests/Test_AngelScriptBytecode.cpp` — bytecode compilation/loading behavior.
- `Source/Tests/Test_CommonScriptMethods.cpp` — common exported methods.
- `Source/Tests/Test_ServerScriptMethods.cpp` — server exported methods.
- `Source/Tests/Test_ScriptBuiltins.cpp` — built-in script helpers/types.
- `Source/Tests/Test_ScriptEntityOps.cpp` — script/entity interactions.

Use these tests as executable documentation when changing script registration, generated wrappers, method signatures, nullability, event declarations, or remote-call dispatch.

## Change routing

- Backend-neutral call ABI: `Source/Common/ScriptSystem.*`.
- AngelScript compiler/runtime lifecycle: `Source/Scripting/AngelScript/AngelScriptScripting.*` and `AngelScriptBackend.*`.
- Attribute syntax and nullable preprocessing: `Source/Scripting/AngelScript/AngelScriptAttributes.*` and [Nullability.md](Nullability.md).
- Script entity/property registration: `Source/Scripting/AngelScript/AngelScriptEntity.*` plus [EntityModel.md](EntityModel.md).
- Remote caller registration/dispatch support: `Source/Scripting/AngelScript/AngelScriptRemoteCalls.*` plus [Networking.md](Networking.md).
- Reflection helpers: `Source/Scripting/AngelScript/AngelScriptReflection.*`.
- Native exported methods: `Source/Scripting/*ScriptMethods.cpp` and [ScriptMethodsMap.md](ScriptMethodsMap.md).
- Build target wiring: `BuildTools/cmake/stages/ScriptsAndBaking.cmake` and [BuildToolsPipeline.md](BuildToolsPipeline.md).
- Generated metadata/codegen: [GeneratedApiAndMetadata.md](GeneratedApiAndMetadata.md).

## Validation checklist

1. If signatures or annotations changed, regenerate code and inspect generated metadata/wrapper diffs.
2. Compile AngelScript through the embedding project's `CompileAngelScript` target or equivalent AS compiler app.
3. Run the smallest affected script tests, starting with `Test_AngelScriptAttributes`, `Test_CommonScriptMethods`, `Test_ServerScriptMethods`, `Test_ScriptBuiltins`, and `Test_ScriptEntityOps` as applicable.
4. For nullable changes, run the nullability analyzers described in [Nullability.md](Nullability.md).
5. For server/client/mapper method changes, validate the owning runtime path; do not rely only on compilation.
6. Update [ScriptMethodsMap.md](ScriptMethodsMap.md) when exported method files are added, removed, or materially regrouped.
