# Scripting

> Engine-owned documentation. This page describes reusable scripting runtime behavior in `Source/Common/ScriptSystem.*` and `Source/Scripting/`; concrete game scripts, quests, rules, and content policy belong to the embedding project.

## Purpose

The scripting layer is the contract between the C++ engine runtime and game-authored behavior. It exposes engine entities, global services, events, remote calls, value types, collections, reflection helpers, and tool/frontend helpers to script code while keeping C++ ownership, metadata, nullability, persistence, networking, and validation in the engine.

Read this page together with:

- [GeneratedApiAndMetadata.md](GeneratedApiAndMetadata.md) for generated metadata, `///@` annotations, and codegen output.
- [Nullability.md](Nullability.md) for `T?` and `FO_NULLABLE` contracts across script/native boundaries.
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
For generic AngelScript calls, mutable script parameters must be read from the actual reference slot:
`out`/`inout` arguments use `asIScriptGeneric::GetArgAddress()`, while ordinary input arguments use
`GetAddressOfArg()`. This distinction is required for script-visible `int32&`, value-type refs such as `mpos&`,
and any exported native method that writes back through a C++ non-const lvalue reference.

`Yield(ms)` suspends the current AngelScript context and asks the active engine to resume it later through
`ScheduleDelayedCallback()`. On multithreaded servers, zero-delay or short-delay resumes may run on another worker
while the suspending context is still unwinding, or while another resume callback for the same context is already
starting. `AngelScriptContextManager::ResumeSpecificContext()` therefore treats `ExecutionActive` as an atomic
reservation before it leaves the context-pool lock: an already-active context is re-armed for a later retry, and
`ReturnContext()` leaves a context busy if a concurrent resume has already reserved it. This keeps suspended
coroutines from being either lost or re-entered concurrently. Client runtime callbacks are processed on the main
loop and do not have the same worker-pool race window.

### AngelScript backend shutdown

`~AngelScriptBackend()` tears the runtime down in a fixed order: stop the debugger endpoint, run the registered cleanup callbacks, reset the context manager, then reset the backend-owned `_engine` / `_meta` / `_scriptSys` / `_entityMngr` holders. It then calls `ReleaseScriptGlobalsAndReportGC()` **before** `asIScriptEngine::ShutDownAndRelease()` (which asserts the returned engine ref count is `0`), and finally runs the post-cleanup callbacks.

`ReleaseScriptGlobalsAndReportGC()` makes the runtime clean up after itself so embedding games do not need per-system reference cleanup at shutdown:

- It walks every module's global variables and drops the script-side reference each one holds: funcdef-typed globals (function handles / delegates) are `Release()`d, and `asOBJ_REF` object globals (handles, arrays, dictionaries, script classes) go through `ReleaseScriptObject()`; the slot is nulled so the standard module teardown skips it. Value-type object globals store their instance inline and are left to module discard (resetting them early would risk a double-free). AngelScript already releases globals during module discard inside `ShutDownAndRelease()`, but doing it explicitly here — while the engine is fully alive — lets the garbage collector collapse the now-unrooted object graphs in a normal multi-iteration cycle.
- It then runs `GarbageCollect(asGC_FULL_CYCLE)` repeatedly (up to 8 passes) until `GetGCStatistics()` reports a steady live count, because object destructors can release the last reference to further objects.
- Any GC objects still alive after that are kept by references the collector cannot reclaim (e.g. a cyclic graph). The method logs a concise `AngelScript shutdown: released N global var(s), M GC object(s) still alive:` line plus a per-type histogram so the surviving types point at the owning system. The subsequent `ShutDownAndRelease()` force-releases them; its AngelScript SDK-only "external reference" / "GC cannot destroy" diagnostics are filtered during this final shutdown phase because they duplicate the survivor summary. Runtime and compilation diagnostics are not filtered. A known residual is *shown* GUI screen graphs from the core scripts, whose cyclic refcount the collector cannot resolve — a separate, GUI-internal issue.

After that explicit report is emitted, teardown installs a shutdown-only AngelScript message callback before `ShutDownAndRelease()`. The callback suppresses the engine library's duplicate force-release diagnostics for the same remaining objects (`external reference`, `GC cannot destroy`, and the follow-up builtin-type line), while preserving all other AngelScript messages. The concise FOnline shutdown histogram remains the authoritative signal for leftover GC objects; repeated AngelScript force-release messages should not be treated as separate runtime warnings after this point.

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
- `FO_MANAGED_SCRIPTING` enables managed runtime loading, adds the `Managed` resource baker, wires `CompileManagedScripts` to the standalone `ManagedScriptBakerApp` (`<FO_DEV_NAME>_ManagedScriptBaker`), and uses `FO_MANAGED_ASSEMBLIES`, `FO_MANAGED_SOURCE`, `FO_MANAGED_REFERENCES`, and optional `FO_MANAGED_MSBUILD`. `AddManagedSource()` records scoped assembly/target entries in `FO_MANAGED_SOURCE` and real source paths in `FO_MANAGED_SOURCE_FILES` for codegen/build visibility. Script-level metadata tags such as `///@ Enum`, `///@ Property`, `///@ RefType`, and `///@ Setting` can live in C# source files; they are accepted by build-time codegen and consumed by `MetadataBaker` during resource baking.
- `BakeResources` and `ForceBakeResources` also depend on code generation and run the project baker app.

Script compilation and resource baking are adjacent but not identical. Script compilation produces bytecode/runtime inputs; baking packages resources and metadata for runtime consumption. See [BakingPipeline.md](BakingPipeline.md) for resource baking.

## Managed and native scripting roots

`Source/Scripting/Managed/CoreScripts/` contains only engine-owned stable C# runtime support such as `Attributes.cs`, `Initializator.cs`, and `Native.cs`. At bake time those core files are copied into the embedding project's managed script directory as local generated files such as `Attributes.gen.cs`, `Initializator.gen.cs`, and `Native.gen.cs`, so generated projects reference a single project-local folder instead of the engine source tree. Script-visible types are generated into the same directory, normally `Scripts/Managed/`: `hstring` and value/ref wrappers go to files such as `ServerTypes.gen.cs`, `ClientTypes.gen.cs`, and `MapperTypes.gen.cs`, while the managed `Entity` base and concrete entity wrappers go to `ServerEntities.gen.cs`, `ClientEntities.gen.cs`, and `MapperEntities.gen.cs`. Generated managed project files and MSBuild-generated assembly metadata also belong to the project managed directory; the engine source directory should not receive generated project `.csproj`, solution `.sln`, assembly info, or target API files. The managed baker only treats its own generated API files as stale cleanup candidates: `.gen.csproj` / `.gen.sln`, known generated managed API filenames, and `.gen.cs` files carrying the baker's auto-generated disclaimer. Project-owned generated C# files produced by other tools, such as GUI generator output, must stay intact. When `FO_MANAGED_SCRIPTING` is enabled, `Source/Tools/ManagedScriptBaker.*` builds server/client/mapper stub metadata, generates C# API files for enums, value/ref types, entity wrappers, events, settings, and content constants, writes one generated project plus a matching solution with `.gen` filenames (for example `<FO_NICE_NAME>.gen.csproj` and `<FO_NICE_NAME>.gen.sln`), adds an auto-generated disclaimer to generated files, deletes stale baker-owned generated artifacts from the managed project directory, then compiles the generated `.gen.cs` files together with project `.cs` sources directly into the current baking pack under `Baking/<Pack>/Assemblies/<Target>Assemblies/` as `<Pack>.<Target>.dll`. Generated settings accessors for scalar project settings and scalar/list engine `ExportSettings` call target-aware managed internal calls against the active backend `GlobalSettings`, so C# module init code can read feature flags, view settings, and other settings from the backend currently executing that target, including in-process parallel test workers. Mapper generated settings receive the Client/Common engine `ExportSettings` surface as well, matching mapper AngelScript visibility for editor rendering, input, geometry, and mapper helper settings. Generated entity wrappers also include generic `Entity.GetAsInt<TProp>()`, `Entity.SetAsInt<TProp>()`, `Entity.GetAsAny<TProp>()`, and `Entity.SetAsAny<TProp>()` property-index helpers backed by native internal calls, metadata-driven entity-holder helpers (`Add<X>`, `Has<X>s`, `Get<X>`, `Get<X>s`) backed by managed internal calls that mirror AngelScript `CustomEntity_*`, and server-side generic `Game.Destroy(...)` aliases over generated `DestroyEntity` calls. Engine CoreScripts also provide a managed `Game.Invoke(string, ...)` dispatcher for managed `Module::Func` callbacks, including generic ref-result overloads and managed exception counters for `GetGlobalExceptionCount()` / `GetContextExceptionCount()`; these counters cover managed `Invoke` failures, observed faulted tasks, swallowed managed event exceptions, and propagated managed callback/property/registered-func exceptions. `Game.Invoke` first resolves methods inside the managed assembly, then falls back to `Native.InvokeScriptFunc`, which asks backend-neutral C++ `ScriptSystem` for registered candidates with the same name, pre-checks candidate compatibility against the boxed C# argument shapes, and invokes the first signature whose exact engine argument descriptors can be populated. Generated wrappers also include `Game.GetPropertyInfo(<Type>Property, out ...)` overloads for entity and fixed-type property enums; these property-info overloads are emitted from bake-time metadata and mirror the AngelScript property-info surface without a runtime internal call. For virtual-property callbacks, generated `Game.AddPropertySetter(...)` overloads support both `PropertySetter<TEntity,TValue>` (`entity, ref value`) and `PropertySetterWithProperty<TEntity,TProperty,TValue>` (`entity, property, ref value`), matching AngelScript setters that are registered for a property group.

Entity-only managed property post-set callbacks are emitted as `Action<TEntity>` delegates. Runtime callback dispatch resolves `FOnline.Native` through the active managed backend, not through the delegate type's assembly image, so both generated `System.Action<TEntity>` delegates and custom `FOnline.*` delegates can invoke the native callback trampoline.

At runtime, `InitManagedScripting()` registers the managed backend after script type mapping, restores every baked dll under `Assemblies/<Target>Assemblies/` for the current side so helper assemblies are available to Mono probing, then loads only entry assemblies matching `Assemblies/<Target>Assemblies/*.<Target>.dll`. Each loaded assembly first receives `FOnline.Initializator.InitializeEarly()` immediately; this binds managed script funcs and remote-call metadata needed by resource/map loading before ordinary script module initialization. The backend then registers `FOnline.Initializator.Initialize()` as a normal script init function, so user `[ModuleInit]` code runs later through `ScriptSystem::InitModules()` after embedding-side native hooks have initialized their state. `Initializator` runs static constructors, finds `[ModuleInit]` methods returning either `void` or `Task`, waits task-returning initializers, and invokes them in priority order. During migration from AngelScript, it also skips managed `[ModuleInit]` and attributed script-func registration for managed types whose root module still has a coexisting `.fos` file, keeping the `.fos` implementation live until the source module is deleted. A coexisting `.fos` module can opt into managed `ModuleInit` ownership by containing the marker text `FO_MANAGED_MODULE_INIT_OWNER`; the AngelScript module must then guard its own runtime init path (typically with `#if !MANAGED`) so metadata/source can coexist while C# owns live initialization. The marker affects `[ModuleInit]` skip logic only; attributed script-function registration still skips coexisting AngelScript modules unless the caller explicitly opts into coexistence. The managed backend is implemented on top of the Mono embedding API and binds the native `FOnline.Native` internal calls used by generated `Game.Log()`, `hstring.ToString()`, `hstring.FromString()`, generated scalar/list `Settings.*` accessors, generated generic entity `GetAsInt`/`SetAsInt`/`GetAsAny`/`SetAsAny` property-index helpers, native-backed scalar/value/entity/dynamic-ref-type properties, native-backed array properties for supported scalar/value/entity-proto/dynamic-ref-type elements, native-backed scalar/value/entity/dynamic-ref-type methods, exported native ref-type methods, scalar/value/entity/dynamic-ref-type `ref`/mutable method arguments, `List<T>`/engine-array method and event arguments for scalar/value/entity/ref-type elements, `Dictionary<K,V>`/engine-dictionary method and event arguments for scalar/value/entity/ref-type keys and values, generated event subscriptions, native event firing, and managed delegate adapters for script callback parameters such as `Callback_void` / `Callback_bool_Critter_Item`. Scalar `any` crosses this bridge as the engine's string-backed `any_t`, with managed values converted through `ToString()` and native `any_t` boxed back as a managed string; managed C# signatures use `object` for metadata `any` when registering script functions. Dynamic `RefType` layouts are generated as C# DTO classes with settable properties; exported native ref types are generated as lightweight wrappers over the native ref pointer. Method wrappers pass the metadata method index to disambiguate overloads. Generated event wrappers do not keep local managed subscriber lists. `Subscribe()` and `Unsubscribe()` create native callback registrations keyed by `(Delegate, backend)` for the active backend, and generated `Fire()` builds an object-array payload and dispatches through `FOnline.Native.FireEvent`, which routes through the engine entity dispatcher and writes mutable arguments back from native call data. This keeps in-process parallel engines/test workers from sharing managed event state while preserving C# delegate invocation through native callbacks; handlers passed to generated `Subscribe()` methods must be marked `[Event]`, and missing markers are rejected before the native subscription is created. API members whose signatures still require unsupported bridges fail managed baking with `ManagedScriptBakerException` instead of generating runtime-only stubs. Runtime loading restores baked resource-pack assemblies to shared content-hashed subdirectories under `Cache/ManagedAssemblies/`, reusing existing files when bytes already match so parallel in-process test workers do not rewrite loaded assemblies while still keeping helper dlls next to the entry assembly; `FO_MANAGED_ASSEMBLIES_DIR` remains an explicit loose-file override for diagnostics. If no baked managed assemblies are present and no explicit managed assembly environment variable is set, the backend logs a skip and continues with zero managed assemblies, which keeps self-contained test rigs and tools that do not bake managed scripts usable.

Managed `List<T>` settings for engine `vector<T>` `ExportSettings` use the same whitespace-separated string format as `GlobalSettings::Save()` and `GlobalSettings::SetValue()`. The generated getter formats the native vector through the engine setting bridge and the C# core helper parses it into `List<T>`; the setter joins values with spaces and routes them back through `SetValue()`, so project custom settings and built-in engine settings share one path.

Managed remote-call registration accepts `void`, `Task`, and `Task<T>` handlers for attributed remote-call methods. Native callback invocation adapts engine collection payloads to the handler's declared C# shape before `DynamicInvoke`: dictionary-like payloads can feed `Dictionary<string, string>` handlers, and `any[]` / string-list payloads can feed `List<string>` handlers even when the native boxed argument arrives as an object list. Returned tasks are observed with `GetAwaiter().GetResult()` and `Task<T>.Result` is propagated back through the same managed-to-native return path, so async managed handlers do not silently detach failures from the script call stack.

Managed `hstring` values are exposed to C# as value-type hashes, while `hstring.ToString()` and `Game.GetHashStr()` resolve through the active backend hash storage. Value structs with `hstring` fields, such as `LanguageName`, convert native `hstring` entries to managed hashes and back explicitly instead of copying raw native `hstring` bytes.

Managed `StringExtensions` mirror selected AngelScript string helpers in C#. UTF-8 byte-buffer helpers such as `rawLength()`, `rawGet()`, `rawResize()`, and `rawSet()` intentionally preserve the AngelScript raw-string contract; because C# strings are immutable, `rawResize()` and `rawSet()` return the rebuilt string and the port tool rewrites statement-form calls into receiver assignments. Parsing helpers such as `tryToInt()` and `toInt()` accept nullable managed receivers, so nullable callback payloads and `object.ToString()` results follow the normal failed-parse/default path. `hstr()` also accepts nullable managed receivers and hashes the empty string when the receiver is null.

Managed `ident` has a CoreScript string constructor and `ToString()` override so string-backed `any_t` property access and GUI parameter conversion can round-trip entity identifiers without generated value-struct special cases.

Managed value-struct CoreScript extensions mirror AngelScript helper constructors and conversions that the raw struct baker cannot derive from fields alone. `TextPackKey.ToString()` mirrors the C++ formatter's `{Collection}{Key1}{Key2}{Key3}` structured tuple. For map directions, `mdir` exposes the AngelScript-compatible `mdir(hdir)` conversion and the `hex` getter; the conversion uses `Settings.Geometry_MapHexagonal` for the same geometry-dependent angle mapping, while `hex` routes through the native `MdirHex` helper.

The CMake `SetupManagedRuntime` target runs `BuildTools/setup-mono.*` with a build-local `FO_WORKSPACE`, builds `mono.runtime+mono.corelib`, publishes the Mono runtime into `dotnet/output/mono/<triplet>`, and stages the managed .NET runtime assemblies under `lib/netcoreapp` with Mono's `System.Private.CoreLib.dll`. All application targets copy that layout to `ManagedRuntime/` next to the executable while also copying native shared runtime libraries to the executable directory. Native packages include that `ManagedRuntime/` directory as a runtime companion, so target machines do not need a separately installed Mono/.NET runtime. Runtime startup checks `FO_MANAGED_RUNTIME_DIR`, the current working directory, and the executable directory for `ManagedRuntime`, adds `lib/netcoreapp` to Mono's assembly search path, enables `DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1` by default because the embedded runtime is published without `System.Globalization.Native`, and attaches each native engine thread that loads managed assemblies to the process-wide Mono domain. Projects that stage their own globalization native library can override the environment variable before startup.

`Source/Scripting/Native/` currently contains `.keepalive`, marking the source-root location for native scripting integration; do not document native scripting as equivalent to the AngelScript runtime. The Managed backend is implemented (see above), with engine-side coverage concentrated in `Source/Tests/Test_ManagedScriptBaker.cpp` for project/resource generation, assembly packaging, path handling, and stale generated artifact cleanup; embedding projects should still carry end-to-end managed runtime suites for their live gameplay modules.

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
