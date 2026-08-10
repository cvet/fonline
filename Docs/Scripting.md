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
- `Source/Common/EngineBase.h`
- `Source/Common/EngineBase.cpp`
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
- `Source/Scripting/Wasm/WasmImports.h`
- `Source/Scripting/Wasm/WasmImports.cpp`
- `Source/Scripting/Wasm/WasmScripting.h`
- `Source/Scripting/Wasm/WasmScripting.cpp`
- `Source/Scripting/Wasm/WasmApiBridge.h`
- `Source/Scripting/Wasm/WasmApiBridge.cpp`
- `Source/Scripting/Wasm/WasmBackend.h`
- `Source/Scripting/Wasm/WasmBackend.cpp`
- `Source/Scripting/Wasm/WebWasmBackend.h`
- `Source/Scripting/Wasm/WebWasmBackend.cpp`
- `Source/Scripting/*ScriptMethods.cpp`
- `Source/Scripting/Mono/*.cs`
- `Source/Scripting/Native/.keepalive`
- `Source/Tools/WasmBaker.h`
- `Source/Tools/WasmBaker.cpp`
- `Source/Tools/WasmAssemblyScriptBaker.h`
- `Source/Tools/WasmAssemblyScriptBaker.cpp`
- `BuildTools/package.py`
- `BuildTools/tests/test_package_zip_determinism.py`
- `BuildTools/tests/test_wasm_host.js`
- `BuildTools/web/default-index.html`
- `BuildTools/web/wasm-host.js`
- `BuildTools/cmake/stages/ThirdParty.cmake`
- `BuildTools/cmake/stages/CoreLibs.cmake`
- `BuildTools/cmake/stages/ScriptsAndBaking.cmake`
- `Source/Tests/Test_AngelScriptAttributes.cpp`
- `Source/Tests/Test_AngelScriptBaker.cpp`
- `Source/Tests/Test_AngelScriptBytecode.cpp`
- `Source/Tests/Test_CommonScriptMethods.cpp`
- `Source/Tests/Test_ScriptBuiltins.cpp`
- `Source/Tests/Test_ScriptEntityOps.cpp`
- `Source/Tests/Test_ServerScriptMethods.cpp`
- `Source/Tests/Test_WasmBaker.cpp`

## Layer map

The scripting subsystem has four layers:

1. **Common runtime facade** — `Source/Common/ScriptSystem.h` / `.cpp` define the backend-agnostic `ScriptSystem`, `ScriptFuncDesc`, `ScriptFunc`, `FuncCallData`, `DataAccessor`, native call adapters, init functions, loop callbacks, and type maps.
2. **Backend implementation** — `Source/Scripting/AngelScript/` provides the current production backend. `Source/Scripting/Wasm/` provides an optional experimental WAMR/browser runtime backend for baked WebAssembly modules. Mono and native scripting have placeholder/source roots, but AngelScript owns the complete script compiler/runtime path in this tree.
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

## WASM runtime path

The WebAssembly backend is optional and disabled by default. `FO_WASM_SCRIPTING` enables it. Native and Android builds link the bundled WAMR tree from `Engine/ThirdParty/wamr` through WAMR's `build-scripts/runtime_lib.cmake`; a versioned `Engine/ThirdParty/wamr-2.4.4` folder is also accepted if the bundled dependency is kept under that name. Web builds do not link WAMR. They use the browser's `WebAssembly` host through `BuildTools/web/wasm-host.js` and `Source/Scripting/Wasm/WebWasmBackend.*`. Web packages include `wasm-host.js` and `WasmScripts/` only when CMake passes the package-level `-wasm-scripting` flag from `FO_WASM_SCRIPTING`.

`InitWasmScripting()` in `Source/Scripting/Wasm/WasmScripting.cpp` creates the platform backend, registers it at `ScriptSystemBackend::WASM_BACKEND_INDEX`, registers metadata, and loads script functions.

`WasmBackend` is intentionally a narrow first runtime bridge:

- Native WAMR loads resource files ending in `.wasm`. Files ending in `.server.wasm`, `.client.wasm`, or `.mapper.wasm` are only loaded on that runtime side; plain `.wasm` files are visible to every side that has the backend enabled.
- Web packages copy client-visible `.wasm` resources into `WasmScripts/`, generate `WasmScripts/manifest.json`, and load those modules before the Emscripten `_main` runs. `WebWasmBackend` reads that manifest from JavaScript and registers the exported functions in `ScriptSystem`.
- The script module name is the file name without `.wasm` and without the optional side suffix. Exported functions are registered as `ModuleName::ExportName` in `ScriptSystem`. A raw WASM export name registers directly; a metadata-typed export can use the suffix form `Export__ArgType_ArgType__RetType` and is registered as `ModuleName::Export` after the suffix is validated and stripped. Use `void` for an empty argument list or return type when a suffixed name needs to express it.
- Exported WASM script functions still use a physical scalar/buffer ABI: `i32`, `i64`, `f32`, and `f64` parameters, plus zero or one scalar result. The metadata suffix form allows primitive/bool values, enums, `hstring`, runtime entity handles, proto/fixed-type handles, borrowed ref-type handles, simple one-field value types, and compact packed structs up to 8 bytes when the metadata type maps to one physical scalar. Read-only `string`/`any` and callback input arguments are also supported; each logical text or callback argument expands to two physical `i32` parameters, `ptr` and `len`, containing UTF-8 bytes in the callee module's memory. Callback export arguments carry the full registered script function name or a temporary callback token and use the suffix type form `callback_<ret>_<args>_callback`; nested callback arguments repeat that same form inside the outer callback suffix. Temporary callback tokens are call-scoped by default; a WASM module that needs to store one after the current export returns must call `fonline.callback_retain(ptr,len)` while the token is still active and later balance it with `fonline.callback_release(ptr,len)`. Exported `string`/`any` returns use one physical `i64` result that packs the module pointer in the low 32 bits and the byte length in the high 32 bits. Simple ref-type export arguments and returns are borrowed opaque `i64` pointers; the export bridge does not retain or release non-mutable values. Non-mutable `T[]`, `string[]`, `any[]`, `dict<K,V>`, and `dict<K,V[]>` export arguments use physical `ptr,byte_len` pairs, and array/dictionary returns use the same packed `i64` `ptr,byte_len` result shape; supported key/value families include borrowed ref-type handles. Mutable scalar/value-type, runtime entity/proto/fixed-type handle, and direct ref-type `T&` export arguments use `ptr,byte_len` and copy the modified bytes back after the export returns; entity/proto/fixed refs accept the same 8-byte value as non-mutable handle arguments, while mutable ref-type values may select only `null` or an exact-type handle already borrowed into the current export call. Mutable `string&`/`any&`, `T[]&`, `string[]&`, `any[]&`, `dict<K,V>&`, and `dict<K,V[]>&` export arguments use `ptr,byte_len,capacity_byte_len,required_byte_len_ptr`; the current backend capacity is the initial serialized byte length, so in-place edits and shrink copy back, while growth reports the required byte length and fails without a partial copy. Mutable arrays and method-independent dictionary shapes may contain ref-type keys or values when the ref type exposes `__AddRef` and `__Release`. Before any destination is cleared or replaced, the backend validates every non-null output ref against the call-scoped exact-type borrowed-handle set and temporarily retains all validated occurrences; it releases those temporary retains after all copy-back operations finish. Mutable text uses UTF-8 bytes, mutable arrays use their serialized array blob, mutable dictionaries reuse the counted key/value blob used by non-mutable dictionary arguments, and mutable dict-of-array values use counted nested array blobs. WASM-exported script entrypoints still cannot take callback return values, remote calls, or global entity objects. Metadata-derived engine imports under `fonline.api` have their own ABI described below.
- Native function calls use WAMR's interpreter path. Stack and heap sizes come from `Script.WasmStackSize` and `Script.WasmHeapSize`. Web function calls go through `wasm-host.js` and the browser `WebAssembly.Instance` exports.
- Native WAMR validates every function import against `Source/Scripting/Wasm/WasmImports.*` immediately after loading a module and before instantiation. Web performs the same validation from the package manifest before registering exports. Non-function imports, unknown import names, unsupported value types, excess arity, and signature mismatches fail module loading instead of becoming late call-site surprises.
- The only backend-specific attribute accepted today is `Wasm`; it is a marker for future metadata use and does not change call behavior.

The first shared import module is `fonline`. `Source/Scripting/Wasm/WasmImports.*` owns the C++ import registry and signatures. Both WAMR and the Web host provide logging imports `fonline.log_i32`, `fonline.log_i64`, `fonline.log_f32`, `fonline.log_f64`, and `fonline.log_utf8(ptr: i32, len: i32)`, callback-token lifecycle imports `fonline.callback_retain(ptr: i32, len: i32) -> i32` and `fonline.callback_release(ptr: i32, len: i32) -> i32`, plus read-only runtime context imports: `fonline.get_side() -> i32`, `fonline.get_frame_time_ms() -> i64`, `fonline.get_frame_delta_time_ms() -> i64`, `fonline.is_time_synchronized() -> i32`, and `fonline.get_synchronized_time_ms() -> i64`. `get_side()` returns `0` for server, `1` for client, and `2` for mapper. `get_synchronized_time_ms()` returns `0` when synchronized time is not available; callers should check `is_time_synchronized()` first.

`log_utf8` and the callback lifecycle imports are pointer-style imports. The WASM signatures are ordinary `(i32, i32) -> void` or `(i32, i32) -> i32` in module manifests, while WAMR uses native `(*~)` and `(*~)i` signatures so the runtime validates and converts the pointer/length pair before entering engine code. The engine treats the buffer as read-only UTF-8 bytes and does not retain it after the import returns. Retain/release return `1` on success; registered global callback names are accepted as no-op lifecycle operations, while temporary delegate tokens require a live `ScriptSystem` call context. On Web, `wasm-host.js` reads the bytes from the currently executing module's exported `memory`; modules that use pointer imports must export their linear memory until a fuller memory-import contract exists.

Runtime context imports read a per-call snapshot prepared by the backend immediately before invoking a WASM export. Native WAMR stores that snapshot as module-instance custom data for the duration of the call; the Web backend passes the same values into `wasm-host.js`, which makes them visible only while the browser export is executing. Web packages include all function imports in `WasmScripts/manifest.json`, and `WebWasmBackend` validates them against the registry before registering module exports. `wasm-host.js` also stores import signatures, checks them while building the browser import object, and installs throwing stubs for missing imports so unresolved host calls fail at the exact call site.

Engine API imports live under the separate `fonline.api` module and are generated from `EngineMetadata`, not from the bootstrap `WasmImports.*` list. `EngineMetadata` records a script API inventory while method metadata and property registrators are registered, including dynamic metadata/property paths. `Source/Scripting/Wasm/WasmApiBridge.*` materializes a side-aware `WasmApiImportTable` from that inventory by resolving entries back to the same `MethodDesc` and `Property` objects that AngelScript consumes; native WAMR and the Web backend both validate/register imports from that table for the owning engine side. The currently supported ABI subset is numeric/enum/`hstring`/`ident` scalar methods, mutable scalar/value-type `&` arguments, mutable `string&`/`any&` method arguments, direct fixed-size object-shaped value-struct method inputs/returns/mutable refs and property get/set imports, scalar-compatible, fixed-size object-shaped value-struct, `string[]`/`any[]`, and borrowed ref-type method arrays including mutable array `&` arguments, scalar-compatible/fixed-size value-struct/`string`/`any` `dict<K,V>` method inputs/returns and mutable `dict<K,V>&` arguments, borrowed ref-type method dictionary keys and values for `dict<K,V>` inputs/returns and mutable `dict<K,V>&` arguments, scalar-compatible/fixed-size value-struct/`string`/`any` `dict<K,V[]>` method inputs/returns and mutable arguments, borrowed ref-type method dictionary keys and nested values for `dict<K,V[]>` inputs/returns and mutable `dict<K,V[]>&` arguments, `string`/`string_view`/`any` method input and return values, callback arguments carried as registered script function names, simple value types, compact packed value structs, runtime entity-handle method arguments and returns, proto/fixed-type handle method arguments and returns, borrowed ref-type receiver methods, ref-type `__Factory` construction imports, ref-type `PassOwnership` returns with explicit retain/release imports, simple ref-type argument/return handles, fixed-type property getters, dynamic ref-type layout property get/set imports, plus scalar-compatible, fixed-size value-struct, and `string`/`any` property getters/setters, property array getters/setters, property dictionary getters/setters, and property dict-of-array getters/setters on global entities and runtime entity receivers. WASM treats `any_t` as the engine's string-backed `any` type and passes it with the same UTF-8 ABI as `string`; it is not a structured variant ABI. Numeric values use the obvious WASM scalar type; enums use their underlying numeric type; `ident`, `hstring`, proto references, fixed-type references, and borrowed ref-type references use `i64`, with `hstring`, proto ids, and fixed-type ids resolved through engine hash storage for method arguments. A simple one-field value type uses the WASM scalar kind of that field, so wrappers such as `timespan`, `ucolor`, and `TextPackName` pass as `i64`, `i32`, and `i64` respectively. A small complex value struct of 1, 2, 4, or 8 bytes whose fields are primitive, enum, or simple one-field value types is passed as raw packed bits in `i32` for sizes up to 4 bytes or `i64` for 8 bytes; for example `mpos` is an `i32` import value that preserves the engine struct layout bytes. Larger fixed-size value structs up to the fixed object-value storage limit use raw bytes in import direct-value buffers, import collection buffers, and metadata-suffixed export direct/collection buffers when all fields are primitive, enum, `hstring`, or supported nested value structs; for example `irect` can be a `fonline.api` method argument/return/ref or property value, a metadata-suffixed export argument/return/ref, and a byte-buffer collection element in `irect[]` or `dict<string, irect[]>`.

Non-global entity receivers are passed as a leading `ident`/`i64` argument and are resolved by `BaseEngine::ResolveScriptEntityHandle()` before dispatch, so a WASM import such as `Critter_IsAlive__ident__bool(cr_id: i64) -> i32` routes through the same generated native method wrapper as AngelScript `Critter.IsAlive()`. Runtime entity method arguments and entity returns are also projected as `ident`/`i64`; nullable entity arguments accept `0`, and nullable entity returns produce `0`. Proto and fixed-type arguments/properties use the proto/fixed id hash as `i64`, resolve through `GetProtoEntity()`, and nullable proto/fixed returns/properties use `0`. Fixed-type property receivers use a leading `hstring`/`i64` id and are read-only, matching AngelScript's fixed-type property surface; for example `Rule_get_Score__hstring_int32(rule_pid: i64) -> i32`. Global property imports keep the compact names `<Entity>_get_<Property>__<Type>` and `<Entity>_set_<Property>__<Type>`; entity receiver property imports add the leading handle to the suffix, for example `Critter_get_Alive__ident_bool(cr_id: i64) -> i32` and `Critter_set_Alive__ident_bool(cr_id: i64, value: i32)`. Property arrays use the same suffix style as method arrays, for example `Game_get_Scores__int32_array(out_ptr: i32, out_len: i32) -> i32` and `Game_set_Tags__string_array(ptr: i32, byte_len: i32) -> void`. Property dictionaries use method dictionary suffixes, for example `Game_get_Config__string_string_dict(out_ptr: i32, out_len: i32) -> i32` and `Game_set_Groups__string_int32_array_dict(ptr: i32, byte_len: i32) -> void`. Setters are supported only for mutable entity/global properties.

Ref-type receiver imports use a leading opaque `i64` pointer supplied by the engine. Visible ref methods are imported with the ref type name in the suffix, for example `MovingContext_GetSpeed__MovingContext__uint16(ctx: i64) -> i32`. Refcounted lifecycle methods `__AddRef` and `__Release` are imported with the same receiver shape, for example `MovingContext___Release__MovingContext__void(ctx: i64) -> void`. Ref factories are imported without a receiver, for example `MovingContext___Factory__void__MovingContext() -> i64`, and return one owned reference. Dynamic ref layout fields are exposed as property imports, for example `RouteSnapshot_get_Note__RouteSnapshot_string(snapshot: i64, out_ptr: i32, out_len: i32) -> i32` and `RouteSnapshot_set_Note__RouteSnapshot_string(snapshot: i64, ptr: i32, len: i32) -> void`. Handles passed into ordinary ref receiver imports are borrowed unless the method documentation says otherwise; WASM code must not forge them. A `__Factory` or another `PassOwnership` method returning a ref type, such as `Critter_MoveToHex__ident_mpos_int32_int32_callback__MovingContext`, transfers one reference to the WASM side, and the module must eventually call the matching `__Release` import. A module that keeps a borrowed ref beyond the current engine call must call `__AddRef` first and later balance it with `__Release`. Non-mutable `RefType[]`, `dict<K,RefType>`, `dict<RefType,V>`, `dict<K,RefType[]>`, and `dict<RefType,V[]>` method imports use the same borrowed `i64` handles for supported key/value positions. Mutable `RefType[]&`, `dict<K,RefType>&`, `dict<RefType,V>&`, `dict<K,RefType[]>&`, and `dict<RefType,V[]>&` method arguments use the same borrowed handles with the existing no-partial-copy collection copy-back contract. Property dictionaries keep ref keys unsupported because the shared property raw-data stream, text/value serializators, and AngelScript property helpers do not define a fixed key-size or persistence policy for borrowed ref handles.

Method input strings/`any` values and property setters are passed as read-only UTF-8 buffers: one metadata `string`, `string_view`, or `any` argument expands to two WASM `i32` parameters, `ptr` and `len`, while native WAMR registration uses `*~` so WAMR validates the linear-memory range and converts it before entering engine code. On Web, `wasm-host.js` decodes the same `ptr,len` from the currently executing module's exported `memory`, copies the bytes into temporary engine memory, and the C++ bridge then presents an owned `string` or `any_t` to the generated native wrapper. Method string/`any` returns and property getters append output `ptr,len` parameters and return the required byte length as `i32`; callers may pass `0,0` to query the size, then allocate in module memory and retry. The engine copies up to the provided capacity and never retains the output buffer.

Direct fixed-size object-shaped method and property values such as `irect` use the same raw layout bytes as their metadata-suffixed export counterparts. Non-mutable method arguments and property setters expand to `ptr, byte_len`; method returns and property getters append output `ptr, byte_len` and return the required fixed byte length as `i32`. If the output buffer is too small, the bridge leaves it untouched and returns the required size. Mutable method refs such as `irect&` use `ptr, byte_len`, copy the initial value into local native storage, call the generated wrapper with an ordinary `T&`, and copy the fixed-size value bytes back into module memory after the wrapper returns.

Mutable method `string&` and `any&` arguments expand to `ptr, byte_len, capacity_byte_len, required_byte_len_ptr`. The bridge copies the input UTF-8 bytes into an owned `string` or string-backed `any_t`, calls the generated wrapper with an ordinary mutable reference, writes the required byte length, and copies the modified bytes back only when they fit in `capacity_byte_len`. Growth attempts leave the caller buffer untouched apart from the required-length write.

Method callback arguments (`ScriptFunc<...>` in metadata) use a read-only UTF-8 `ptr,len` pair containing the full registered script function name, for example `CombatWasm::CanStep`, or an engine-generated temporary callback token. The target function must already be registered in `ScriptSystem`, unless the name is a temporary token produced for a callback delegate during the currently executing WASM export call or retained from an earlier call through `fonline.callback_retain`. Native and Web WASM backends register module exports as `Module::Export`, so passing that full name lets the bridge resolve the callback through the same `ScriptFuncDesc` path used by AngelScript. The callback import ABI is independent from the callback target backend: it can name any registered function whose metadata signature matches, including AngelScript functions with object, string, array, dictionary, mutable argument shapes, or nested callback arguments. A callback target implemented as a WASM export may use the raw scalar export ABI or the metadata suffix form for scalar-compatible primitive, enum, `hstring`, runtime entity handle, proto/fixed-type handle, borrowed ref-type handle, simple value-type, compact packed-struct signatures, read-only `string`/`any` input arguments, `string`/`any` returns, non-mutable array/dictionary/dict-of-array arguments and returns including borrowed ref handles, mutable scalar/value-type arguments, mutable runtime entity/proto/fixed refs, mutable text arguments, mutable array/dictionary/dict-of-array arguments including call-scoped validated ref handles, and callback arguments, including nested callback argument signatures. Callback return values remain pending ABI work.

For a metadata-suffixed WASM export with a callback argument, encode the metadata suffix as `callback_<ret>_<args>_callback`; for example `Func__callback_int32_int32_callback__int32` receives a `callback(int32,int32)` argument and returns `int32`. A nested callback argument is encoded recursively, for example `Func__callback_void_callback_int32_int32_callback_callback__int32` receives a `callback(void, callback(int32,int32))` argument. The physical arguments are `ptr:i32, len:i32` containing UTF-8 bytes for the full registered function name or a temporary callback token. Native WAMR and the Web sidecar host copy that value into callee module memory before calling the export. An empty name represents a null callback. If the source callback is an object-bound delegate, `ScriptSystem` registers a `__fonline_callback_N` token whose lifetime is limited to the current WASM export call unless the module calls `fonline.callback_retain(ptr,len)` before the export returns. Retained tokens remain resolvable by `fonline.api` callback imports in later export calls until `fonline.callback_release(ptr,len)` drops the last retain.

For a metadata-suffixed WASM export with a read-only `string` or `any` input argument, the native WAMR backend allocates temporary module memory with `wasm_runtime_module_malloc`, copies the engine UTF-8 bytes into that memory, calls the export, and frees the temporary allocation after the call. The Web backend passes the engine-side buffer to `wasm-host.js`; the host allocates temporary memory inside the target sidecar module, copies the bytes, calls the export, and then frees the allocation. Web sidecar modules using text-input exports must export `memory` and an allocator pair named `fonline_malloc`/`fonline_free`, `malloc`/`free`, or `__wasm_malloc`/`__wasm_free`.

For a metadata-suffixed WASM export with a `string` or `any` return, the physical result is one `i64`: low 32 bits are a pointer in the callee module memory, high 32 bits are the UTF-8 byte length. Native WAMR validates that application-memory range and copies the bytes into an engine-owned `string` or string-backed `any_t` immediately; the module keeps ownership of the returned memory. On Web, `wasm-host.js` reads the sidecar module memory, copies the bytes into a temporary engine heap buffer, returns the packed engine `ptr,len` pair to C++, and the C++ bridge frees that temporary buffer after copying into the script result. Web sidecar modules using text-return exports must export `memory`; no sidecar allocator is needed unless the same export also receives text input.

For a metadata-suffixed WASM export with a fixed-size object-shaped value struct such as `irect`, non-mutable arguments use `ptr:i32, byte_len:i32` and returns use one packed physical `i64` `ptr,byte_len` result. The buffer contains the value's metadata layout bytes; supported fields are primitive numeric/bool values, enums, `hstring`, and supported nested value structs, and the total value size must fit the fixed object-value storage limit. Mutable object-shaped refs use the normal `_mut` suffix, for example `Func__irect_mut__void`, with physical `ptr:i32, byte_len:i32`; native WAMR and Web sidecar paths copy the initial value into module memory and copy the same fixed-size byte range back after the export returns. Web sidecar modules using direct object-shaped value arguments must export `memory` plus an allocator/free pair; modules that only return such values need `memory` but not an allocator.

For a metadata-suffixed WASM export with a mutable `string&` or `any&` argument, the suffix appends `_mut`, for example `Func__string_mut__void`. The physical arguments are `ptr:i32, byte_len:i32, capacity_byte_len:i32, required_byte_len_ptr:i32`. Native WAMR and the Web sidecar host copy the initial UTF-8 bytes into temporary module memory, call the export, read the required byte length, and copy the modified bytes back only when they fit in `capacity_byte_len`. The current export bridge allocates `capacity_byte_len` equal to the initial byte length; exports can edit in place or shrink the text, and a grow attempt reports the required byte length then fails the script call without partially overwriting the caller value. WASM treats `any&` here as the same string-backed UTF-8 representation as `string&`.

For a metadata-suffixed WASM export with runtime entity, proto, or fixed-type arguments/returns, the physical value is `i64`. Runtime entity arguments are packed from `Entity::GetId()` and returns are resolved through `BaseEngine::ResolveScriptEntityHandle()` before the engine caller receives the `Entity*`. Proto and fixed-type arguments/returns use the proto id hash and resolve through `GetProtoEntity()`. A zero handle maps to `null`; non-zero unresolved or type-mismatched handles fail the script call. Mutable handle refs use suffix `_mut`, for example `Func__Critter_mut__void`, and the mutable value physical shape `ptr:i32, byte_len:i32`; the buffer is exactly 8 bytes, copied back after the export returns, and zero again maps to `null`.

For a metadata-suffixed WASM export with a non-mutable array argument, the physical arguments are `ptr:i32, byte_len:i32`. The native WAMR backend serializes the caller's `DataAccessor` array into temporary module memory, calls the export, and frees that allocation after the call. The Web backend serializes the same bytes in engine memory and lets `wasm-host.js` allocate/copy/free the sidecar module buffer. Fixed-size arrays use raw element bytes, so `uint8[]` is the byte-buffer path; element families match the import-side array ABI: primitive numeric/bool values, enums, `hstring`, `ident`, proto/fixed handles, runtime entity handles, simple value wrappers, compact packed structs, and fixed-size object-shaped value structs up to the collection buffer storage limit such as `irect`. `string[]` and `any[]` use the counted UTF-8 blob `u32 count` followed by repeated `u32 byte_len` plus bytes. Array returns use one physical `i64` result with low 32 bits as a module pointer and high 32 bits as byte length; the engine copies and decodes the returned blob immediately into the caller's `DataAccessor`. Web sidecar modules using array inputs must export `memory` plus an allocator/free pair; modules that only return arrays need `memory` but not an allocator.

For a metadata-suffixed WASM export with a non-mutable `dict<K,V>` argument, the physical arguments are also `ptr:i32, byte_len:i32`. The blob is little-endian `u32 count`, followed by repeated key/value entries. `string` and string-backed `any` entries are `u32 byte_len` plus UTF-8 bytes; fixed-size entries use the same raw wire bytes as export arrays, including primitive numeric/bool values, enums, `hstring`, `ident`, runtime entity handles, proto/fixed handles, simple value wrappers, compact packed structs, and fixed-size object-shaped value structs. Dictionary returns use one physical `i64` `ptr,byte_len` result and are copied/decoded immediately into the caller's `DataAccessor`. Metadata suffixes use `<key>_<value>_dict`, for example `Func__string_string_dict__int32` or `Func__void__string_string_dict`. Web sidecar modules using dictionary inputs must export `memory` plus an allocator/free pair; modules that only return dictionaries need `memory` but not an allocator.

For a metadata-suffixed WASM export with a non-mutable `dict<K,V[]>` argument or return, the suffix is `<key>_<value>_array_dict`, for example `Func__string_int32_array_dict__int32`, `Func__string_bool_array_dict__int32`, `Func__string_ucolor_array_dict__int32`, `Func__string_irect_array_dict__int32`, `Func__string_Rule_array_dict__int32`, or `Func__void__string_Rule_array_dict`. The outer dictionary uses the same `u32 count` key/value stream as `dict<K,V>`, and each value is a nested array blob: `u32 count` followed by fixed-size elements, or repeated `u32 byte_len` plus UTF-8 bytes for `string`/`any`. Fixed-size nested elements include primitive numeric/bool values, enums, `hstring`, `ident`, runtime entity/proto/fixed handles, simple one-field value types such as `ucolor`, compact packed structs, and fixed-size object-shaped value structs such as `irect`. Mutable `dict<K,V[]>&` uses `<key>_<value>_array_dict_mut` and the same `ptr,byte_len,capacity_byte_len,required_byte_len_ptr` no-partial-copy contract as mutable dictionaries. Callback values remain pending.

For a metadata-suffixed WASM export with a mutable scalar/value-type or runtime entity/proto/fixed handle argument, the suffix appends `_mut`, for example `Func__int32_mut__void`, `Func__irect_mut__void`, or `Func__Critter_mut__void`. The physical arguments are `ptr:i32, byte_len:i32`. Native WAMR serializes the initial engine value bytes into temporary module memory, calls the export, copies the same byte range back into the caller value, then frees the temporary allocation. The Web backend uses `wasm-host.js` to allocate/copy/free sidecar module memory and copy the modified bytes back into the Emscripten engine heap before C++ stores them through the caller's `DataAccessor`. Supported mutable export values are primitive numeric/bool values, enums, `hstring`, `ident`, runtime entity/proto/fixed handles, simple value wrappers, compact packed structs, and fixed-size object-shaped value structs such as `irect`.

For a metadata-suffixed WASM export with a mutable array argument, the suffix appends `_array_mut`, for example `Func__int32_array_mut__void`. The physical arguments are `ptr:i32, byte_len:i32, capacity_byte_len:i32, required_byte_len_ptr:i32`. Native WAMR and the Web sidecar host copy the initial serialized array blob into temporary module memory, call the export, read the required byte length, and copy the modified blob back only when it fits in `capacity_byte_len`. The current export bridge allocates `capacity_byte_len` equal to the initial serialized size; exports can edit in place or shrink the array, and a grow attempt reports the required byte length then fails the script call without partially overwriting the caller value. Supported element families match non-mutable export arrays: fixed-size raw element bytes, including scalar-compatible and fixed-size object-shaped value structs, plus counted UTF-8 blobs for `string[]` and `any[]`.

Method and property arrays use raw byte buffers. A read-only metadata array argument expands to `ptr, byte_len`; array returns and property array getters append output `ptr, byte_len` and return the required byte length as `i32`, mirroring the string-return query/retry pattern. The bridge projects method buffers back into the normal `DataAccessor` path, so generated wrappers still receive `vector<T>` / `readonly_vector<T>` rather than a WASM-specific type. Property array setters validate the incoming blob and then write it through the regular property setter path. Scalar-compatible arrays require `byte_len` to be aligned to the element wire size. Supported element families are primitive numeric/bool values, enums, `hstring`, `ident`, proto/fixed handles, runtime entity handles, simple value wrappers, compact packed structs, and fixed-size object-shaped value structs up to the import buffer scalar storage limit when their fields are primitive, enum, `hstring`, or supported nested value structs. `uint8[]` is therefore the byte-buffer ABI, and `irect[]` is the larger value-struct buffer ABI. `string[]` and `any[]` use a little-endian blob: `u32 count`, then repeated `u32 byte_len` plus UTF-8 bytes. For text-array returns and property array getters, a too-small output buffer is left untouched and the returned byte length tells the caller how much memory to allocate.

Mutable array method arguments (`T[]&`, `string[]&`, and `any[]&`) expand to `ptr, byte_len, capacity_byte_len, required_byte_len_ptr`. `byte_len` describes the initial array contents, `capacity_byte_len` describes the writable output capacity, and `required_byte_len_ptr` receives the byte length of the modified array after the generated wrapper returns. If the modified array does not fit in `capacity_byte_len`, the bridge writes only the required byte length and leaves the output array buffer unchanged; callers can grow module memory and retry. `T[]&` uses the same scalar-compatible element bytes as read-only `T[]`, and string-like arrays use the same counted UTF-8 blob as read-only `string[]`/`any[]`.

Method and property dictionaries currently support `dict<K,V>` when both key and value are either `string`, `any`, one of the scalar-compatible element families used by `T[]`, or a fixed-size object-shaped value struct that fits the import collection buffer rules; method dictionary keys and values may also be borrowed ref-type handles where the method ABI explicitly supports them. Property dictionaries cannot use ref keys: `PropertyRegistrar` rejects `dict<RefType,V>` and `dict<RefType,V[]>` declarations because property storage has no fixed key size or persistence policy for borrowed ref handles, so they never enter the metadata-derived import inventory. A read-only dictionary argument expands to `ptr, byte_len`; a dictionary return or property dictionary getter appends output `ptr, byte_len` and returns the required byte length as `i32`. The WASM blob is little-endian `u32 count`, then repeated key/value entries. String-like entries are `u32 byte_len` plus UTF-8 bytes; fixed-size scalar-compatible, object-shaped value-struct, and ref-handle entries are raw element bytes. Property raw dictionary storage omits the outer `u32 count`; the bridge counts entries while validating getter output and strips the count after validating setter input, then routes the raw stream through the regular property setter path. If a dictionary return or property getter does not fit in the output buffer, the bridge leaves the output buffer unchanged and returns the required size. Mutable dictionary arguments (`dict<K,V>&`) expand to `ptr, byte_len, capacity_byte_len, required_byte_len_ptr`; they use the same blob format and the same no-partial-copy growth contract as mutable arrays. Mutable method dictionary keys and values may be borrowed ref handles; property dictionary ref keys remain rejected.

Dictionary-of-array method and property values (`dict<K,V[]>`) use the same outer dictionary ABI. Each value entry is a nested array blob: `u32 count`, then fixed-size scalar-compatible elements, fixed-size object-shaped value-struct elements, or repeated `u32 byte_len` plus UTF-8 bytes for `string`/`any`; method values may also use borrowed ref-type handles as fixed `u64` elements. Mutable `dict<K,V[]>&` uses the same `ptr, byte_len, capacity_byte_len, required_byte_len_ptr` shape and no-partial-copy growth contract as mutable dictionaries, including borrowed ref keys and nested values on method arguments and metadata-suffixed export collections. Property dict-of-array getters/setters use the same count-prefix conversion as plain property dictionaries. The bridge stores method input nested arrays through the generic `DataAccessor` path, so generated native wrappers still receive ordinary `map<K, vector<V>>` values for supported metadata element families, including enums, one-field value types, compact packed structs, fixed-size object-shaped structs such as `irect`, and borrowed ref handles. Callback values, structs that exceed collection buffer storage or contain unsupported fields, and other registerable ownership-sensitive metadata remain in the descriptor inventory with an explicit pending-ABI reason until the corresponding ABI is implemented; invalid property ref-key dictionaries are rejected earlier during property registration.

Mutable scalar/value-type method arguments (`T&`) also use pointer/length pairs. The buffer contains the initial raw value bytes in the engine value layout; the bridge copies them into local native storage, calls the generated wrapper with an ordinary `T&`, and copies the modified bytes back before returning. This covers primitive refs, enums, `hstring`, `ident`, simple value wrappers, compact packed structs such as `mpos&`, and fixed-size object-shaped structs such as `irect&`; mutable entity/proto refs, mutable WASM-exported collections/callback refs, and richer nested object ownership still need dedicated rules.

When adding a new host import, update `WasmImports.*` first, then add the matching JavaScript implementation in `BuildTools/web/wasm-host.js`. The import name, module name, parameter list, result list, and WAMR native signature must stay identical across those two places. Engine services that need runtime state should be introduced deliberately with explicit ownership and thread/reentrancy rules; do not expose entity pointers or mutable engine state as raw integers.

`WasmBaker` is the bake-time owner for WASM script resources when `FO_WASM_SCRIPTING` is enabled and the `Wasm` baker is requested. It still copies ready `.wasm` files unchanged, but it can also consume project-authored `.fowasm` descriptors. A descriptor causes the baker to create a side-specific frontend API manifest directly from the live `EngineMetadata` and write it to baked resources as `WasmApi/server.json`, `WasmApi/client.json`, or `WasmApi/mapper.json`. This frontend data is not emitted by `BuildTools/codegen.py`: codegen remains responsible for generated C++ metadata, and baking derives script-frontend data from that metadata.

`.fowasm` descriptors use a `[WasmScript]` section. A descriptor can either name a generic project-owned `Command`, or choose a known frontend such as `AssemblyScript`:

```ini
[WasmScript]
Frontend = AssemblyScript
Target = Server
Source = Scripts/Wasm/Combat.ts
Output = Scripts/Wasm/Combat.server.wasm
Inputs = Scripts/Wasm/shared.ts

[AssemblyScript]
Compiler = npm exec --yes --package assemblyscript -- asc
Args = --optimize
Bindings = fonline_api.ts
```

`Target` is `Server`, `Client`, or `Mapper`. `Source`, `Inputs`, and the descriptor itself participate in incremental bake checks, so authors can edit WASM script sources between resource bakes and get a rebuilt `.wasm` without rerunning codegen. The engine stages input files into a temporary `.wasm-build/<Pack>/...` tree, expands `{api}`, `{api_dir}`, `{side}`, `{pack}`, `{build_dir}`, `{source}`, `{output}`, `{output_dir}`, `{bindings}`, and `{bindings_dir}`, then reads the produced `.wasm` and writes it to `Output` as a baked resource. If the compiler also emits `<output>.map`, the baker writes `Output + ".map"` as a baked sidecar.

`Frontend = AssemblyScript` routes the staged source through `WasmAssemblyScriptBaker`. The frontend baker reads the same side-specific `WasmApi/*.json` manifest, writes generated AssemblyScript declarations to `Bindings` next to the staged source (`fonline_api.ts` by default), and exposes supported `fonline.api` metadata imports through an `Api` namespace plus the bootstrap `fonline` runtime imports. If `Command` is absent, the default compiler command is `{Compiler} {source} --outFile {output} {Args}`, with `npm exec --yes --package assemblyscript -- asc` and `--optimize` as defaults. If `Command` is present, the baker still generates the bindings file and lets the custom command consume `{bindings}` or `{bindings_dir}`. A descriptor without `Command` and without a known `Frontend` only emits the metadata-derived API manifest, which is useful for tooling.

Debug metadata is toolchain-owned. Embedded DWARF/name sections stay inside the copied `.wasm` bytes; if a Web-side toolchain emits an external source map named like `Module.client.wasm.map`, place it next to the `.wasm` file and `package.py -wasm-scripting` will copy it next to the packaged sidecar under `WasmScripts/`. Source maps are not part of `WasmScripts/manifest.json`, do not affect import/export validation, and are ignored by native WAMR; use browser DevTools for Web sidecar source mapping and the engine logs/module export names for native runtime triage.

## WASM backend work plan

The current milestone is a metadata-backed ABI that works in native/Android through WAMR and in Web through browser `WebAssembly`.

Completed:
- optional `FO_WASM_SCRIPTING` build wiring;
- WAMR native runtime loading for bundled WAMR;
- Web package sidecar layout, manifest generation, and `wasm-host.js`;
- `ScriptSystem` registration for raw scalar exports and scalar-compatible metadata-suffixed exports;
- shared `fonline.log_*` import smoke path;
- C++ import registry plus native WAMR and Web manifest/import signature validation;
- read-only runtime context imports for side and time snapshots;
- first read-only pointer/length import via `fonline.log_utf8`;
- side-aware metadata-derived WASM API import tables from the script API inventory recorded by `EngineMetadata` during method/property registration, with scalar-compatible entries marked separately from entries that still need a richer ABI;
- native WAMR raw import registration for scalar-compatible metadata methods and plain scalar property get/set imports under the `fonline.api` import module;
- Web host registration for scalar-compatible metadata methods and properties under the same `fonline.api` import module, using `wasm-host.js` scalar packing and C++ callbacks into the shared metadata bridge;
- entity receiver handle ABI for scalar-compatible metadata methods and plain scalar property get/set imports, using a leading `ident`/`i64` receiver resolved by the owning runtime engine;
- `hstring` method/property ABI as `i64` hash values resolved through engine hash storage for method calls;
- `ident` method/property ABI as `i64`, plus runtime entity argument and return ABI as nullable-aware `ident`/`i64` handles resolved by the owning runtime engine;
- proto method/property ABI as nullable-aware `hstring`/`i64` handles resolved through engine proto metadata;
- fixed-type method/property value ABI as `hstring`/`i64` handles plus fixed-type property getter receivers resolved through engine proto metadata;
- enum ABI through the enum's underlying numeric scalar, simple one-field value-type ABI through the wrapped field scalar, and small packed value-struct ABI through raw `i32`/`i64` layout bytes;
- method/property `string`/`any` ABI as UTF-8 `ptr,len`, including method return/property getter output buffers, WAMR `*~` native signatures, and Web trampoline copying between module memory and temporary engine memory;
- mutable method `string&`/`any&` ABI as UTF-8 `ptr,byte_len,capacity_byte_len,required_byte_len_ptr`, including no-partial-copy growth reporting and Web trampoline copy-back into module memory;
- scalar-compatible, fixed-size object-shaped value-struct, and `string[]`/`any[]` property array get/set ABI through the same `ptr,byte_len` and output `ptr,byte_len -> required byte length` contract as method arrays;
- scalar-compatible/fixed-size value-struct/`string`/`any` property `dict<K,V>` and `dict<K,V[]>` get/set ABI through method dictionary blobs, with conversion to and from property raw dictionary streams;
- mutable scalar/value-type `&` argument ABI as raw `ptr,len` in/out buffers, including WAMR `*~` native signatures and Web trampoline copy-back into module memory;
- direct fixed-size object-shaped value-struct method/property ABI, where non-mutable inputs use `ptr,byte_len`, returns/getters use no-partial output `ptr,byte_len -> required byte length`, and mutable method refs use `ptr,byte_len` copy-back;
- scalar-compatible and fixed-size object-shaped value-struct method `T[]` input/return ABI as raw byte buffers, including `uint8[]` byte buffers and Web trampoline copying between module memory and temporary engine memory;
- method `string[]`/`any[]` input/return ABI as a counted UTF-8 blob carried over the same raw byte-buffer trampoline;
- mutable method `T[]&`/`string[]&`/`any[]&` ABI as `ptr,byte_len,capacity_byte_len,required_byte_len_ptr`, including no-partial-copy growth reporting and Web trampoline copy-back into module memory;
- scalar-compatible/`string`/`any` method `dict<K,V>` input/return ABI as counted key/value blobs with no-partial-copy growth reporting for returns;
- mutable scalar-compatible/`string`/`any` method `dict<K,V>&` ABI as `ptr,byte_len,capacity_byte_len,required_byte_len_ptr`, including no-partial-copy growth reporting and Web trampoline copy-back into module memory;
- scalar-compatible/fixed-size value-struct/`string`/`any` method `dict<K,V[]>` input/return and mutable `dict<K,V[]>&` ABI as counted nested array blobs;
- callback argument ABI as UTF-8 registered script function names or temporary delegate tokens resolved through `ScriptSystem`, including explicit retain/release for tokens that must outlive the creating export call and non-scalar/nested callback signatures when the named target backend supports them;
- borrowed ref-type receiver handles for visible ref methods and dynamic ref layout property get/set imports;
- ref-type `__Factory` construction imports and `PassOwnership` method returns plus explicit `__AddRef`/`__Release` imports for balancing retained ref handles;
- borrowed ref-type handles inside method array/dictionary/dict-of-array key and value positions, including mutable collection copy-back;
- metadata-suffixed WASM export registration for scalar-compatible enum/value-type callback targets, runtime entity/proto/fixed-type handle arguments, returns, mutable refs, borrowed ref-type collections, call-scoped validated mutable direct/collection ref handles, fixed-size object-shaped value-struct direct value buffers and collection buffers, and dict-of-array nested scalar-compatible values, read-only `string`/`any` input arguments, non-mutable array/dictionary/dict-of-array arguments, mutable scalar/value-type arguments, mutable text arguments, mutable array arguments, mutable dictionary and dict-of-array arguments, callback arguments including nested callback signatures and retained temporary delegate tokens, `string`/`any` returns, direct fixed-size object-shaped value returns, and non-mutable array/dictionary/dict-of-array returns backed by copied module-memory bytes;
- focused unit coverage for `WasmBaker`, AssemblyScript frontend binding/default-command generation, the import registry, callback-token retain/release lifecycle, package-side `.wasm` helper rules, metadata API direct method/property value imports, mutable direct text imports, and property array/dictionary imports including fixed-size object-shaped value structs, native WAMR scalar and metadata-suffixed export calls including read-only text inputs, text returns, direct object-shaped value inputs/returns/refs, array/dictionary/dict-of-array inputs/returns with primitive numeric/bool/text/handle/value-type/object-shaped nested arrays, mutable scalar/text/array/dictionary/dict-of-array copy-back, callback, nested callback, and retained temporary callback delegate token forwarding, and entity/proto/fixed handle roundtrips plus mutable handle ref copy-back, runtime context imports, and `wasm-host.js` scalar/context/string/value/array/dict/callback paths.

The current manual `WasmImports.*` registry is a bootstrap host-services layer for `fonline.log_*`, runtime context snapshots, and low-level memory helpers. It is not the final engine API list. The final WASM API bridge must import exactly the same script-visible engine surface that AngelScript gets: every supported `///@` declaration flows into generated metadata once, the metadata layer records a side-aware script API inventory, and WASM consumes that inventory through backend-specific ABI adapters and side filters. The import descriptors are still materialized from metadata because dispatch needs live `MethodDesc` and `Property` pointers; if descriptor materialization becomes a measured cost, the cache belongs to metadata registration or resource baking, not to a separate frontend-output path in `codegen.py`.

Post-milestone extension boundaries:
- callback return values need a retained delegate-object ABI before an engine API can return callbacks/delegates to WASM;
- property ref-key dictionaries need a shared property raw-storage, serialization, and AngelScript accessor policy for fixed borrowed-handle keys before `dict<RefType,V>` or `dict<RefType,V[]>` properties can be enabled;
- mutable or stateful host-service imports need attachment, threading, ownership, and reentrancy rules beyond the current read-only runtime snapshot imports.

The metadata-backed API bridge is intentionally larger than adding individual imports. Its implementation needs these substeps:

1. Define the stable WASM ABI for metadata types: scalar values, strings/`any`/byte buffers, `hstring`, entity ids/handles, proto ids, arrays/dictionaries, nullable values, and callback/event references.
2. Keep the `WasmApiImportTable` inventory metadata-owned so server/client/mapper APIs expose only the functions and types valid for that side without adding a parallel WASM declaration list.
3. Keep frontend API data in the bake/runtime metadata path. If the ABI needs a compact cache later, generate it from `EngineMetadata` during registration or baking; do not move language-frontend output into `codegen.py`.
4. Route generated imports through the existing generated script method descriptors/wrappers, property registrators, and `ScriptSystem` call path where possible, preserving validation, nullability, ownership, and reentrancy rules.
5. Add package/runtime validation that a WASM module imports only names and exact signatures generated from metadata.
6. Add tests for generated descriptors, native WAMR calls, Web host calls, side filtering, and rejection of unsupported metadata shapes.

## Attributes, declarations, and metadata

`Source/Scripting/AngelScript/AngelScriptAttributes.cpp` parses engine-specific script attributes and declaration tags. Important contracts include:

- nullable `T?` suffix stripping and propagation into metadata;
- `///@ Event` declarations and matching `[[Event]]` handlers;
- `///@ RemoteCall` declarations and matching `[[ServerRemoteCall]]`, `[[ClientRemoteCall]]`, or `[[AdminRemoteCall]]` implementations;
- module/init-function priorities;
- callback attribute validation rules;
- `[[InvokeEntry]]` for functions dispatched only by name through the global `Invoke(...)` helper. It blocks ordinary direct calls while still allowing a function reference for `NameOf(...)` registration.

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

For entity instance methods, the AngelScript dispatch layer validates the receiver before entering the native method body. `Entity_MethodCall` calls `CheckScriptEntityAccessAndNonDestroyed`, which checks server sync coverage and destroyed state for the `self` entity. Do not add an entry-only `ValidateEntityAccess(self)` or repeat the receiver check before ordinary receiver reads. Later in the body, validate entities only at real access/assert boundaries such as event dispatch or post-reentry continuation. When a covered entity must keep its own lock across a detach or reparent, use the cover-retaining, idempotent `EnsureEntitySynced(...)`; it retains existing caller cover — never releasing or parking on it — and cannot acquire an omitted dependency.

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
- `Serializer.fos`
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
- `FO_WASM_SCRIPTING` wires the optional WASM runtime library and the `WasmBaker`. Native and Android builds link WAMR; Web builds package sidecar scripts for the browser host. Projects may provide ready `.wasm` files or `.fowasm` descriptors whose frontend compiler commands run during resource baking from metadata-derived API manifests.
- `BakeResources` and `ForceBakeResources` also depend on code generation and run the project baker app.

Script compilation and resource baking are adjacent but not identical. Script compilation produces bytecode/runtime inputs; baking packages resources and metadata for runtime consumption. See [BakingPipeline.md](BakingPipeline.md) for resource baking.

## Mono, WASM, and native scripting roots

`Source/Scripting/Mono/` contains C# support files such as `AssemblyInfo.cs`, `BasicTypes.cs`, `Entity.cs`, `Initializator.cs`, `MapSprite.cs`, and `Link.xml`. BuildTools can wire Mono compilation when `FO_MONO_SCRIPTING` is enabled.

`Source/Scripting/Wasm/` contains the optional WAMR-backed loader/call bridge for baked `.wasm` modules. Treat it as an experimental metadata-backed ABI backend with scalar-compatible and fixed-size direct/collection value-struct buffers, direct fixed-size object-shaped `fonline.api` method/property value imports, runtime entity/proto/fixed handle, borrowed ref-handle ownership, call-scoped validated mutable ref-handle copy-back, text-input/text-return, mutable direct text import, array export, dictionary export, dict-of-array export including scalar-compatible and object-shaped nested value types, property array/dictionary imports, mutable scalar and runtime entity/proto/fixed handle export, mutable direct object-shaped value export, mutable text export, mutable array export, mutable dictionary export, mutable dict-of-array export, and callback/nested-callback/retained temporary delegate export-argument support, not as an AngelScript-equivalent gameplay surface until the remaining ownership-sensitive metadata ABIs are implemented and covered by runtime tests.

`Source/Scripting/Native/` currently contains `.keepalive`, marking the source-root location for native scripting integration. Do not document Native, Mono, or WASM as equivalent to the AngelScript runtime unless the implementation and tests are expanded.

## Tests to inspect

Script behavior is covered by focused tests:

- `Source/Tests/Test_AngelScriptAttributes.cpp` — attribute parsing, nullable suffix handling, events, remote calls, and callback rules.
- `Source/Tests/Test_AngelScriptBaker.cpp` — AngelScript bytecode/resource baking path.
- `Source/Tests/Test_WasmBaker.cpp` — optional WASM baker copy path, `.fowasm` metadata-derived frontend API manifest generation, AssemblyScript frontend binding/default-command generation, C++ import registry contracts, callback-token retain/release lifecycle, metadata API bridge coverage including borrowed ref-type receiver imports, read-only and mutable ref arrays/dictionaries/ref-key dictionaries, dynamic ref layout properties, mutable direct text imports, direct method/property value imports, and property array/dictionary imports with fixed-size object-shaped value structs, Web host trampoline tests, and native WAMR scalar plus metadata-suffixed runtime-call coverage, including text-input, text-return, direct object-shaped value input/return/ref, array, dictionary, dict-of-array with primitive numeric/bool/text/handle/value-type/object-shaped/ref nested arrays, mutable scalar, mutable text, mutable array, mutable dictionary, mutable dict-of-array, callback, nested callback, and retained temporary callback delegate token arguments, entity/proto/fixed handle refs, simple borrowed ref-handle exports, borrowed ref collection exports, call-scoped validated mutable ref copy-back, and forged-handle rejection, when `FO_WASM_SCRIPTING` is enabled.
- `Source/Tests/Test_AngelScriptBytecode.cpp` — bytecode compilation/loading behavior.
- `Source/Tests/Test_CommonScriptMethods.cpp` — common exported methods.
- `Source/Tests/Test_ServerScriptMethods.cpp` — server exported methods.
- `Source/Tests/Test_ScriptBuiltins.cpp` — built-in script helpers/types.
- `Source/Tests/Test_ScriptEntityOps.cpp` — script/entity interactions.

Use these tests as executable documentation when changing script registration, generated wrappers, method signatures, nullability, event declarations, or remote-call dispatch.

## Change routing

- Backend-neutral call ABI: `Source/Common/ScriptSystem.*`.
- AngelScript compiler/runtime lifecycle: `Source/Scripting/AngelScript/AngelScriptScripting.*` and `AngelScriptBackend.*`.
- WASM runtime lifecycle and scalar-compatible metadata ABI: `Source/Scripting/Wasm/WasmScripting.*`, `Source/Scripting/Wasm/WasmBackend.*`, `BuildTools/cmake/stages/ThirdParty.cmake`, and [BakingPipeline.md](BakingPipeline.md).
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
4. For WASM backend changes, configure once with `FO_WASM_SCRIPTING` disabled to protect the default build, configure/build one native preset with `FO_WASM_SCRIPTING` enabled, and configure/build one Web preset with `FO_WASM_SCRIPTING` enabled.
5. For nullable changes, run the nullability analyzers described in [Nullability.md](Nullability.md).
6. For server/client/mapper method changes, validate the owning runtime path; do not rely only on compilation.
7. Update [ScriptMethodsMap.md](ScriptMethodsMap.md) when exported method files are added, removed, or materially regrouped.
