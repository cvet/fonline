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
- `ThirdParty/AngelScript/sdk/angelscript/source/as_compiler.cpp`
- `Source/Scripting/*ScriptMethods.cpp`
- `Source/Scripting/Managed/CoreScripts/*.cs`
- `Source/Tools/ManagedScriptBaker.*`
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
2. **Backend implementation** — `Source/Scripting/AngelScript/` provides the current production backend. `Source/Scripting/Managed/` is an implemented backend that embeds a Mono runtime with the marshalling, event-subscription, and settings bridges described later in this document; `Source/Scripting/Native/` is still a source-root stub. AngelScript owns the longest-running script compiler/runtime path in this tree.
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

`ScriptFunc<TRet, Args...>` normalizes native arguments into `FuncCallData` and catches script exceptions so callers can continue after a failed script callback. It retains return-value cleanup state only for non-void return types; void callbacks have no return storage to clean up when delayed callbacks are moved or destroyed during entity teardown. `NativeDataProvider` and `NativeDataCaller` adapt C++ arrays, dictionaries, entities, callbacks, value types, and mutable references to the generic call representation.

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

`Yield(ms)` suspends the current AngelScript context and asks the active engine to resume it later through
`ScheduleDelayedCallback()`. On multithreaded servers, zero-delay or short-delay resumes may run on another worker
while the suspending context is still unwinding, or while another resume callback for the same context is already
starting. `AngelScriptContextManager::ResumeSpecificContext()` therefore treats `ExecutionActive` as an atomic
reservation before it leaves the context-pool lock: an already-active context is re-armed for a later retry, and
`ReturnContext()` leaves a context busy if a concurrent resume has already reserved it. This keeps suspended
coroutines from being either lost or re-entered concurrently. Client runtime callbacks are processed on the main
loop and do not have the same worker-pool race window.

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

Client render helpers such as `Game.DrawSprite`, `Game.DrawSpritePattern`, and `Game.DrawSpriteRegion` are valid only during render-facing script callbacks (`RenderIface` / GUI draw callbacks). `Game.DrawSpriteRegion(sprId, uv0, uv1, pos, size, color)` draws a normalized `[0, 1]` sub-rectangle of the sprite's original logical image into a destination rectangle; polygon-cropped atlas frames are remapped through their source offset and transparent cropped margins remain transparent in the destination. `Game.DrawSpritePattern` follows the same logical-image contract for every complete or partial tile. Region drawing is intended for reusable GUI composition such as script-side 9-slice panels, and returns `false` when the sprite cannot provide atlas-region drawing.

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
- `FO_MANAGED_SCRIPTING` enables managed runtime loading, adds the `Managed` resource baker, and wires `CompileManagedScripts` to the standalone `ManagedScriptBakerApp` (`<FO_DEV_NAME>_ManagedScriptBaker`). Nothing else about managed scripting is configured at CMake level: the baker discovers `.cs` sources from the resource packs declared in the main config (the same ownership model as AngelScript sources) and reads everything else from settings with plain defaults — project name `FOnline`, assembly list `FOnline`, `dotnet msbuild`, `net10.0`, source dirs `Engine/Source/Scripting/Managed/CoreScripts` plus `Scripts` (relative to the working directory), generated output in the build `GeneratedSource/Managed` tree unless `Script.ManagedScriptGeneratedDir` points elsewhere; empty values are configuration errors, not fallbacks; the `Script.ManagedScriptAssemblies` / `ManagedScriptExtraSources` / `ManagedScriptExtraReferences` / `ManagedScriptMsBuild` / `ManagedScriptTargetFramework` / `ManagedScriptProjectName` / `ManagedScriptDirs` / `ManagedScriptGeneratedDir` / `ManagedScriptBakerDryRun` settings (`ScriptSettings` group) are the override channel for tests and special tooling; the baker reads no environment variables. Script-level metadata tags such as `///@ Enum`, `///@ Property`, `///@ RefType`, and `///@ Setting` can live in C# source files; build-time codegen skips them (`script_metadata_tags`) and `MetadataBaker` consumes them during resource baking.
- `BakeResources` and `ForceBakeResources` also depend on code generation and run the project baker app.

Script compilation and resource baking are adjacent but not identical. Script compilation produces bytecode/runtime inputs; baking packages resources and metadata for runtime consumption. See [BakingPipeline.md](BakingPipeline.md) for resource baking.

## Managed and native scripting roots

`Source/Scripting/Managed/CoreScripts/` contains only engine-owned stable C# runtime support such as `Attributes.cs`, `Initializator.cs`, and `Native.cs`. Those core files are compiled in place: every directory listed in `Script.ManagedScriptDirs` contributes its top-level `.cs` files to the generated project, engine and project sources alike. Script-visible types are generated into the `Script.ManagedScriptGeneratedDir` directory: `hstring` and value/ref wrappers go to files such as `ServerTypes.gen.cs`, `ClientTypes.gen.cs`, and `MapperTypes.gen.cs`, while the managed `Entity` base and concrete entity wrappers go to `ServerEntities.gen.cs`, `ClientEntities.gen.cs`, and `MapperEntities.gen.cs`. Generated managed project files and MSBuild-generated assembly metadata also belong to the project managed directory; the engine source directory should not receive generated project `.csproj`, solution `.sln`, assembly info, or target API files. The managed baker only treats its own generated API files as stale cleanup candidates: `.gen.csproj` / `.gen.sln`, known generated managed API filenames, and `.gen.cs` files carrying the baker's auto-generated disclaimer. Project-owned generated C# files produced by other tools, such as GUI generator output, must stay intact. When `FO_MANAGED_SCRIPTING` is enabled, `Source/Tools/ManagedScriptBaker.*` builds server/client/mapper stub metadata, generates C# API files for enums, value/ref types, entity wrappers, events, settings, and content constants, writes one generated project plus a matching solution with `.gen` filenames (for example `<FO_NICE_NAME>.gen.csproj` and `<FO_NICE_NAME>.gen.sln`), adds an auto-generated disclaimer to generated files, deletes stale baker-owned generated artifacts from the managed project directory, then compiles the generated `.gen.cs` files together with project `.cs` sources directly into the current baking pack under `Baking/<Pack>/Assemblies/<Target>Assemblies/` as `<Pack>.<Target>.dll`. Generated settings accessors for scalar project settings and scalar/list engine `ExportSettings` call target-aware managed internal calls against the active backend `GlobalSettings`, so C# module init code can read feature flags, view settings, and other settings from the backend currently executing that target, including in-process parallel test workers. Mapper generated settings receive the Client/Common engine `ExportSettings` surface as well, matching mapper AngelScript visibility for editor rendering, input, geometry, and mapper helper settings. Generated entity wrappers also include generic `Entity.GetAsInt<TProp>()`, `Entity.SetAsInt<TProp>()`, `Entity.GetAsAny<TProp>()`, and `Entity.SetAsAny<TProp>()` property-index helpers backed by native internal calls, metadata-driven entity-holder helpers (`Add<X>`, `Has<X>s`, `Get<X>`, `Get<X>s`) backed by managed internal calls that mirror AngelScript `CustomEntity_*`, and server-side generic `Game.Destroy(...)` aliases over generated `DestroyEntity` calls. Engine CoreScripts also provide a managed `Game.Invoke(string, ...)` dispatcher for managed `Module::Func` callbacks, including generic ref-result overloads and managed exception counters for `GetGlobalExceptionCount()` / `GetContextExceptionCount()`; these counters cover managed `Invoke` failures, observed faulted tasks, swallowed managed event exceptions, and propagated managed callback/property/registered-func exceptions. `Game.Invoke` first resolves methods inside the managed assembly, then falls back to `Native.InvokeScriptFunc`, which asks backend-neutral C++ `ScriptSystem` for registered candidates with the same name, pre-checks candidate compatibility against the boxed C# argument shapes, and invokes the first signature whose exact engine argument descriptors can be populated. Generated wrappers also include `Game.GetPropertyInfo(<Type>Property, out ...)` overloads for entity and fixed-type property enums; these property-info overloads are emitted from bake-time metadata and mirror the AngelScript property-info surface without a runtime internal call. For virtual-property callbacks, generated `Game.AddPropertySetter(...)` overloads support both `PropertySetter<TEntity,TValue>` (`entity, ref value`) and `PropertySetterWithProperty<TEntity,TProperty,TValue>` (`entity, property, ref value`), matching AngelScript setters that are registered for a property group.

`Source/Scripting/Managed/ManagedHost/` owns the stateless bootstrap used before project code can be loaded. The baker emits `FOnline.ManagedHost.gen.csproj`, references it from the generated project, and packages `FOnline.ManagedHost.dll` beside every target entry assembly. Its source timestamp and output path participate in the managed bake check, so incremental baking rebuilds a missing or changed host and does not delete an unchanged host as stale output.

Native exported ref types are lightweight borrowed wrappers. When a ref type exports `__Factory`, the managed baker emits that factory as a static C# method and the backend invokes it without a receiver. The returned initial native reference belongs to the managed caller and must be balanced with `__Release()` after the object is detached from native users; borrowed wrappers retained across frames still require paired `__AddRef()` / `__Release()` calls.

Entity-only managed property post-set callbacks are emitted as `Action<TEntity>` delegates. Runtime callback dispatch resolves `FOnline.Native` through the active managed backend, not through the delegate type's assembly image, so both generated `System.Action<TEntity>` delegates and custom `FOnline.*` delegates can invoke the native callback trampoline.

At runtime, all managed backends share one Mono VM and root domain, but each `ManagedScriptBackend` owns a dedicated non-collectible `System.Runtime.Loader.AssemblyLoadContext`. `FOnline.ManagedHost.dll` is the only engine bootstrap loaded into the default context; it loads the current target's entry and helper assemblies into the backend context. Consequently C# static fields, type initializers, module state, event-wrapper dictionaries, and dependency identity are isolated per engine instance even when several server/client/mapper engines coexist in one process. Managed objects and delegates must not cross backend boundaries; generated entity wrappers retain the backend identity and reject such use. Backend shutdown removes native callback roots and managed function descriptors, clears the host scope, and releases its GC handle. The context itself remains owned by Mono until process shutdown: collectible-context unload corrupts the embedded Mono runtime once native callbacks and concurrent engine threads have exercised the assembly. Context execution remains parallel, while assembly loading is serialized process-wide because embedded Mono does not safely overlap context loads. Mono VM shutdown remains process-owned.

`InitManagedScripting()` registers the managed backend after script type mapping, restores every baked dll under `Assemblies/<Target>Assemblies/` for the current side, and asks the backend context to load only entry assemblies matching `Assemblies/<Target>Assemblies/*.<Target>.dll`. Each loaded assembly first receives `FOnline.Initializator.InitializeEarly()` immediately; this binds managed script funcs and remote-call metadata needed by resource/map loading before ordinary script module initialization, and then invokes every static parameterless `[ScriptFuncRegistrar]` method found in the assembly — the extension point embedding projects use to register their own attributed script functions (e.g. dialog demand/result markers) in the registration phase. `InitializeEarly()` rejects a second invocation in the same load context, making accidental default-context loading a runtime error instead of silently sharing static state. Because registration lives in `InitializeEarly`, bake-time validation engines can restore the managed script subsystem from the baked compiled assembly with `InitManagedScripting()` — the managed twin of the AngelScript bytecode restore — and resolve managed functions through the ordinary `ScriptSystem::FindFunc`/`CheckFunc` reflection, with no side manifests. The backend then registers `FOnline.Initializator.Initialize()` as a normal script init function, so user `[ModuleInit]` code runs later through `ScriptSystem::InitModules()` after embedding-side native hooks have initialized their state. `Initializator` runs static constructors, finds `[ModuleInit]` methods returning either `void` or `Task`, waits task-returning initializers, and invokes them in priority order. During migration from AngelScript, it also skips managed `[ModuleInit]` and attributed script-func registration for managed types whose root module still has a coexisting `.fos` file, keeping the `.fos` implementation live until the source module is deleted. A coexisting `.fos` module can opt into managed `ModuleInit` ownership by containing the marker text `FO_MANAGED_MODULE_INIT_OWNER`; the AngelScript module must then guard its own runtime init path (typically with `#if !MANAGED`) so metadata/source can coexist while C# owns live initialization. The marker affects `[ModuleInit]` skip logic only; attributed script-function registration still skips coexisting AngelScript modules unless the caller explicitly opts into coexistence. The managed backend is implemented on top of the Mono embedding API and binds the native `FOnline.Native` internal calls used by generated `Game.Log()`, `hstring.ToString()`, `hstring.FromString()`, generated scalar/list `Settings.*` accessors, generated generic entity `GetAsInt`/`SetAsInt`/`GetAsAny`/`SetAsAny` property-index helpers, native-backed scalar/value/entity/dynamic-ref-type properties, native-backed array properties for supported scalar/value/entity-proto/dynamic-ref-type elements, native-backed scalar/value/entity/dynamic-ref-type methods, exported native ref-type methods, scalar/value/entity/dynamic-ref-type `ref`/mutable method arguments, `List<T>`/engine-array method and event arguments for scalar/value/entity/ref-type elements, `Dictionary<K,V>`/engine-dictionary method and event arguments for scalar/value/entity/ref-type keys and values, generated event subscriptions, native event firing, and managed delegate adapters for script callback parameters such as `Callback_void` / `Callback_bool_Critter_Item`. Scalar `any` crosses this bridge as the engine's string-backed `any_t`, with managed values converted through `ToString()` and native `any_t` boxed back as a managed string; managed C# signatures use `object` for metadata `any` when registering script functions. Dynamic `RefType` layouts are generated as C# DTO classes with settable properties; exported native ref types are generated as lightweight wrappers over the native ref pointer. Method wrappers pass the metadata method index to disambiguate overloads. Generated event wrappers do not keep local managed subscriber lists. `Subscribe()` and `Unsubscribe()` create native callback registrations keyed by `(Delegate, backend)` for the active backend, and generated `Fire()` builds an object-array payload and dispatches through `FOnline.Native.FireEvent`, which routes through the engine entity dispatcher and writes mutable arguments back from native call data. This preserves C# delegate invocation through native callbacks; handlers passed to generated `Subscribe()` methods must be marked `[Event]`, and missing markers are rejected before the native subscription is created. API members whose signatures still require unsupported bridges fail managed baking with `ManagedScriptBakerException` instead of generating runtime-only stubs. Runtime loading restores baked resource-pack assemblies to shared content-hashed subdirectories under `Cache/ManagedAssemblies/`, reusing existing files when bytes already match so parallel in-process test workers do not rewrite loaded assemblies while still keeping helper dlls next to the entry assembly. If no baked managed assemblies are present, the backend logs a skip and continues with zero managed assemblies, which keeps self-contained test rigs and tools that do not bake managed scripts usable.

Managed `List<T>` settings for engine `vector<T>` `ExportSettings` use the same whitespace-separated string format as `GlobalSettings::Save()` and `GlobalSettings::SetValue()`. The generated getter formats the native vector through the engine setting bridge and the C# core helper parses it into `List<T>`; the setter joins values with spaces and routes them back through `SetValue()`, so project custom settings and built-in engine settings share one path.

Managed remote-call registration accepts `void`, `Task`, and `Task<T>` handlers for attributed remote-call methods. Native callback invocation adapts engine collection payloads to the handler's declared C# shape before `DynamicInvoke`: dictionary-like payloads can feed `Dictionary<string, string>` handlers, while the string-backed engine `any[]` surface is boxed as `List<string>` for generated APIs and can also feed registered `List<object>` handlers through an explicit list adaptation. Returned tasks are observed with `GetAwaiter().GetResult()` and `Task<T>.Result` is propagated back through the same managed-to-native return path, so async managed handlers do not silently detach failures from the script call stack.

Managed `hstring` values are exposed to C# as value-type hashes, while construction, `hstring.ToString()`, and `Game.GetHashStr()` use the active backend's ordinary `EngineMetadata::Hashes`. Static C# `hstring` fields initialize independently in each backend load context, so their literals are interned directly into that engine instance; there is no process-wide managed literal table or cross-engine fallback. Value structs with `hstring` fields, such as `LanguageName`, convert native `hstring` entries to managed hashes and back explicitly instead of copying raw native `hstring` bytes.

Managed `StringExtensions` mirror selected AngelScript string helpers in C#. UTF-8 byte-buffer helpers such as `rawLength()`, `rawGet()`, `rawResize()`, and `rawSet()` intentionally preserve the AngelScript raw-string contract; because C# strings are immutable, `rawResize()` and `rawSet()` return the rebuilt string and the port tool rewrites statement-form calls into receiver assignments. Parsing helpers such as `tryToInt()` and `toInt()` accept nullable managed receivers, so nullable callback payloads and `object.ToString()` results follow the normal failed-parse/default path. `hstr()` also accepts nullable managed receivers and hashes the empty string when the receiver is null.

Managed `ident` has a CoreScript string constructor and `ToString()` override so string-backed `any_t` property access and GUI parameter conversion can round-trip entity identifiers without generated value-struct special cases.

Managed value-struct CoreScript extensions mirror AngelScript helper constructors and conversions that the raw struct baker cannot derive from fields alone. `TextPackKey.ToString()` mirrors the C++ formatter's `{Collection}{Key1}{Key2}{Key3}` structured tuple. For map directions, `mdir` exposes the AngelScript-compatible `mdir(hdir)` conversion and the `hex` getter; the conversion uses `Settings.Geometry_MapHexagonal` for the same geometry-dependent angle mapping, while `hex` routes through the native `MdirHex` helper.

The CMake `SetupManagedRuntime` target runs `BuildTools/setup-mono.*` with a build-local `FO_WORKSPACE`, builds `mono.runtime+mono.corelib`, publishes the Mono runtime into `dotnet/output/mono/<triplet>`, and stages the managed .NET runtime assemblies under `lib/netcoreapp` with Mono's `System.Private.CoreLib.dll`. All application targets copy that layout to `ManagedRuntime/` next to the executable while also copying native shared runtime libraries to the executable directory. Native packages include that `ManagedRuntime/` directory as a runtime companion, so target machines do not need a separately installed Mono/.NET runtime. Runtime startup checks the current working directory and the executable directory for `ManagedRuntime`, adds `lib/netcoreapp` to Mono's assembly search path, enables `DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1` by default because the embedded runtime is published without `System.Globalization.Native`, and attaches each native engine thread to the process-wide Mono root domain. Per-engine isolation is provided by the backend-owned `AssemblyLoadContext`, not by classic Mono AppDomains, whose creation/unload embedding APIs are unavailable in this runtime. Projects that stage their own globalization native library can override the environment variable before startup.

`Source/Scripting/Native/` currently contains `.keepalive`, marking the source-root location for native scripting integration; do not document native scripting as equivalent to the AngelScript runtime. The Managed backend is implemented (see above), with engine-side coverage concentrated in `Source/Tests/Test_ManagedScriptBaker.cpp` for project/resource generation, host/helper assembly packaging and incremental tracking, path handling, and stale generated artifact cleanup; embedding projects should still carry end-to-end managed runtime suites for their live gameplay modules.

## Tests to inspect

Script behavior is covered by focused tests:

- `Source/Tests/Test_AngelScriptAttributes.cpp` — attribute parsing, nullable suffix handling, events, remote calls, and callback rules.
- `Source/Tests/Test_AngelScriptBaker.cpp` — AngelScript bytecode/resource baking path.
- `Source/Tests/Test_AngelScriptBytecode.cpp` — bytecode compilation/loading behavior.
- `Source/Tests/Test_ManagedScriptBaker.cpp` — managed project/resource generation in dry-run mode.
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
