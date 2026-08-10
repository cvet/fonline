# Generated API and Metadata

This document explains the engine code-generation and metadata-registration flow. Use it when changing generated source, metadata annotations, property definitions, or script-visible API contracts.

## Ownership model

The engine owns the reusable metadata/codegen machinery. An embedding project supplies project configuration, extra metadata sources, common headers, and script/content inputs through CMake options and project files.

Generated files are build artifacts. Document the source annotations, templates, generator inputs, and validation flow; do not treat generated output as hand-authored engine source.

## Source paths inspected

- `BuildTools/cmake/stages/Codegen.cmake`
- `BuildTools/cmake/stages/EngineSources.cmake`
- `BuildTools/cmake/helpers/Build.cmake`
- `BuildTools/cmake/helpers/State.cmake`
- `BuildTools/codegen.py`
- `Source/Common/MetadataRegistration.h`
- `Source/Common/MetadataRegistration.cpp`
- `Source/Common/MetadataRegistration.template.cpp`
- `Source/Common/GenericCode.template.cpp`
- `Source/Common/EngineBase.h`
- `Source/Common/EngineBase.cpp`
- `Source/Common/Properties.h`
- `Source/Common/Properties.cpp`
- `Source/Common/Entity.h`
- `Source/Common/Entity.cpp`
- `Source/Tools/MetadataBaker.h`
- `Source/Tools/MetadataBaker.cpp`
- `Source/Tools/WasmBaker.h`
- `Source/Tools/WasmBaker.cpp`
- `Source/Tools/WasmAssemblyScriptBaker.h`
- `Source/Tools/WasmAssemblyScriptBaker.cpp`
- `Source/Tests/Test_EngineMetadata.cpp`
- `Source/Tests/Test_MetadataBaker.cpp`
- `Source/Tests/Test_Properties.cpp`
- `PUBLIC_API.md`

## CMake codegen stage

`BuildTools/cmake/stages/Codegen.cmake` constructs the generator command and output list.

Important command arguments include:

- `-maincfg` — embedding project's main config (`FO_MAIN_CONFIG`).
- `-buildhash` — current build hash.
- `-genoutput` — generated output directory, currently `GeneratedSource` under the CMake binary dir.
- `-devname` / `-nicename` — project identity values.
- `-embedded` — embedded data capacity (`FO_EMBEDDED_DATA_CAPACITY`).
- `-internalcfg` — internal config capacity (`FO_INTERNAL_CONFIG_CAPACITY`).
- `-meta` — metadata source entries from `FO_SOURCE_META_FILES` and `FO_MONO_SOURCE`.
- `-commonheader` — extra common headers from `FO_ADDED_COMMON_HEADERS`.
- `-enginedefine` — repeatable `NAME=VALUE` engine value/shape configuration macro (`FO_GEOMETRY`, `FO_MAP_*`, `FO_EFFECT_*`, `FO_MODEL_*`, `FO_USE_NAMESPACE`, `FO_NO_*`, `FO_MAIN_CONFIG`, ...), resolved to a literal at configure time and emitted into `EngineConfig.gen.h` instead of being passed as a `-D` compiler define. Feature/backend toggles (`FO_ENABLE_3D`, `FO_*_SCRIPTING`, `FO_*_PARTICLES`) and per-config `FO_DEBUG` stay compiler-side — they gate whole files/headers before any engine header is included.

The stage creates normal and forced code-generation command targets and appends `CodeGeneration` to `FO_GEN_DEPENDENCIES`.

## Generated outputs

`Codegen.cmake` declares generated outputs under `GeneratedSource/`, including:

- `CodeGenTouch`
- `EngineConfig.gen.h` — one macro-only header consumed at the top of `Source/Essentials/BasicCore.h`. It contains both the engine configuration macros and the build/version string macros `FO_BUILD_HASH` / `FO_DEV_NAME` / `FO_NICE_NAME` / `FO_COMPATIBILITY_VERSION` / `FO_GIT_BRANCH`. Replaces the former `Version-Include.h`.
- `EmbeddedResources.gen.inc`
- `InternalConfig.gen.inc`
- `MetadataRegistration-Server.gen.cpp`
- `MetadataRegistration-Client.gen.cpp`
- `MetadataRegistration-Mapper.gen.cpp`
- `MetadataRegistration-ServerStub.gen.cpp`
- `MetadataRegistration-ClientStub.gen.cpp`
- `MetadataRegistration-MapperStub.gen.cpp`
- `GenericCode-Common.gen.cpp`

These file names are useful for understanding build flow, but changes should usually be made in templates, annotations, metadata sources, or generator scripts rather than in generated output.

## Script backend consumers

Generated metadata is the source of truth for script-visible engine API. AngelScript registration consumes it today, and the WASM API bridge consumes the same metadata rather than maintaining a separate import list. A new `///@` declaration should enter metadata once; script backends then expose or reject it from that shared representation according to side filters and the backend's supported ABI shapes. Codegen preserves nested `ScriptFunc<...>` callback signatures in metadata strings instead of flattening the callback argument list.

`BuildTools/codegen.py` stops at generated C++ metadata, wrappers, and metadata-registration code. It does not generate frontend compiler bindings for WASM languages. WASM frontend data is generated later by `Source/Tools/WasmBaker.*` during resource baking: the baker constructs the same side-specific `BakerServerEngine`, `BakerClientEngine`, or `BakerMapperEngine` metadata view used by script compilation, materializes a `WasmApiImportTable`, and writes `WasmApi/server.json`, `WasmApi/client.json`, or `WasmApi/mapper.json` for frontend compilers and baked tooling. Language helpers such as `WasmAssemblyScriptBaker.*` consume that bake-time manifest and generate frontend declarations in the staged script build tree, so TypeScript/AssemblyScript output stays downstream of metadata baking rather than becoming a codegen artifact.

For WASM, `EngineMetadata` records a side-aware script API inventory while metadata is registered: `RegisterEntityMethod(s)` and `RegisterRefTypeMethod(s)` record method entries, and `PropertyRegistrator::RegisterProperty()` notifies the metadata owner so generated and dynamic `ExportProperty` registrators record getter/setter entries. `WasmApiBridge.*` materializes a `WasmApiImportTable` for `fonline.api` from that inventory by resolving each entry back to the live `MethodDesc` or `Property`, enabling numeric/enum/`hstring`/`ident` scalar global methods, mutable scalar/value-type `&` arguments as raw `ptr,len` in/out buffers, mutable `string&`/`any&` method arguments as UTF-8 `ptr,byte_len,capacity_byte_len,required_byte_len_ptr` buffers, direct fixed-size object-shaped value-struct method arguments/returns/mutable refs and property values as raw `ptr,byte_len` buffers, scalar-compatible and fixed-size object-shaped value-struct `T[]` method inputs and returns as raw byte buffers, scalar-compatible and fixed-size value-struct `T[]&` mutable method arguments as `ptr,byte_len,capacity_byte_len,required_byte_len_ptr`, `string[]`/`any[]` method inputs/returns and mutable method arguments as counted UTF-8 blobs, scalar-compatible/fixed-size value-struct/`string`/`any` `dict<K,V>` method inputs/returns and `dict<K,V>&` mutable method arguments as counted key/value blobs, scalar-compatible/fixed-size value-struct/`string`/`any` `dict<K,V[]>` method inputs/returns and mutable arguments as counted nested array blobs, borrowed ref-type `T[]`, `dict<K,T>`, and `dict<K,T[]>` method values for inputs/returns and mutable argument copy-back, borrowed ref-type dictionary keys for method `dict<K,V>` and `dict<K,V[]>` inputs/returns and mutable argument copy-back, `string`/`string_view`/`any` method input and return values as UTF-8 `ptr,len` buffers, metadata callback method arguments including nested callback-argument signatures as UTF-8 script function names, simple one-field value types, small packed value structs, larger fixed-size value structs inside collection buffers, fixed-size value structs as metadata-suffixed export direct value buffers, runtime-entity receiver methods, runtime entity argument/return handles, proto/fixed-type argument/return handles, borrowed ref-type receiver methods, ref-type `__Factory` construction imports, ref-type `PassOwnership` returns plus explicit `__AddRef`/`__Release` lifecycle imports, simple ref-type argument/return handles, fixed-type property getter receivers, dynamic ref-type layout property get/set imports, scalar-compatible or `string`/`any` property get/set imports, scalar-compatible/fixed-size value-struct or `string[]`/`any[]` property array get/set imports, and scalar-compatible/fixed-size value-struct/`string`/`any` property `dict<K,V>` / `dict<K,V[]>` get/set imports. Native WAMR and the Web host consume that same table, so server/client/mapper import inventories stay tied to the metadata side registered for the owning engine instance without a backend-specific declaration list.

WASM treats engine `any_t` as its native string-backed representation and uses the same UTF-8 ABI as `string`; this is not a structured variant ABI. Entity receiver and entity value imports use `ident`/`i64` handles that the runtime resolves through `BaseEngine::ResolveScriptEntityHandle()`; nullable entity arguments and returns use `0` as null. `hstring`, proto, and fixed-type values are projected as checked `i64` hash values, with proto/fixed handles resolved through `GetProtoEntity()`. Ref-type values use opaque `i64` pointers supplied by the engine; ordinary receiver imports and non-mutable collection values/keys are borrowed, while `__Factory` and other `PassOwnership` ref returns transfer one reference that the WASM module must balance through the matching `__Release` import. `__Factory` is imported without a receiver as `<RefType>___Factory__void__<RefType>`, and `__AddRef` is available when a module needs to retain a borrowed ref. Enums use their underlying scalar kind, one-field value types use their wrapped field kind, compact structs up to 8 bytes use raw `i32`/`i64` layout bytes, `uint8[]` is the metadata-backed byte-buffer path, and larger fixed-size value structs up to the fixed object-value storage limit use raw bytes in direct import buffers, import collection buffers, and metadata-suffixed export direct/collection buffers when their fields are primitive, enum, `hstring`, or supported nested value structs. Callback imports carry the callback function name or a `__fonline_callback_N` temporary delegate token; the target can be any already registered script function whose metadata signature matches, including signatures with callback arguments. Temporary tokens are valid only during the WASM export call that created them unless the module calls `fonline.callback_retain(ptr,len)` before the call returns and later balances it with `fonline.callback_release(ptr,len)`.

WASM-exported callback targets may use raw scalar signatures or metadata-suffixed signatures such as `Func__TestMode__TestMode`, `Func__ProtoItem__ProtoItem`, `Func__RefCounter__RefCounter`, `Func__string__int32`, `Func__void__string`, `Func__irect__int32`, `Func__void__irect`, `Func__irect_mut__void`, `Func__int32_array__int32`, `Func__void__uint8_array`, `Func__string_string_dict__int32`, `Func__void__string_string_dict`, `Func__string_int32_array_dict__int32`, `Func__string_bool_array_dict__int32`, `Func__string_ucolor_array_dict__int32`, `Func__string_irect_array_dict__int32`, `Func__void__string_irect_array_dict`, `Func__string_irect_array_dict_mut__void`, `Func__string_Rule_array_dict__int32`, `Func__void__string_Rule_array_dict`, `Func__RefCounter_array__int32`, `Func__void__RefCounter_array`, `Func__string_RefCounter_dict__int32`, `Func__void__string_RefCounter_array_dict`, `Func__int32_mut__void`, `Func__Critter_mut__void`, `Func__string_mut__void`, `Func__int32_array_mut__void`, `Func__string_string_dict_mut__void`, `Func__string_Rule_array_dict_mut__void`, `Func__callback_int32_int32_callback__int32`, and `Func__callback_void_callback_int32_int32_callback_callback__int32`, which the backend registers as `Module::Func` after validation. Export-side runtime entity/proto/fixed handles are `i64`, with returns resolved before the engine caller receives an `Entity*`; simple ref-type export arguments and returns are borrowed opaque `i64` pointers and are not retained/released by the export bridge. Export-side read-only `string`/`any`, direct fixed-size object-shaped values, non-mutable array, non-mutable dictionary, non-mutable dict-of-array including borrowed ref handles and fixed-size object-shaped value structs, mutable scalar/value-type and runtime entity/proto/fixed handle refs, and callback inputs are copied into temporary module memory as `ptr,len` pairs. Mutable `string&`/`any&`, mutable array, mutable dictionary, and mutable dict-of-array inputs are copied as `ptr,byte_len,capacity_byte_len,required_byte_len_ptr`; mutable scalar/value-type/handle, mutable direct object-shaped value, mutable text, mutable array, mutable dictionary, and mutable dict-of-array inputs are copied back after the export returns when the result fits the provided capacity. Callback inputs carry the registered function name or call-scoped temporary delegate token as UTF-8 bytes, and matching text, direct fixed-size object-shaped value, array, dictionary, and dict-of-array returns use a packed physical `i64` whose low 32 bits are a module pointer and high 32 bits are the byte length copied immediately by the engine.

Unsupported metadata shapes that can be registered, such as mutable ref handles, callback return values, and value structs that exceed the fixed buffer rules, remain visible as pending ABI entries instead of moving into a backend-specific declaration list. Property `dict<RefType,V>` and `dict<RefType,V[]>` declarations are rejected earlier by `PropertyRegistrator`: the shared property stream and AngelScript property helpers do not define a fixed size, persistence model, or borrowed-handle policy for ref-type keys, so no property import entry is created for those invalid declarations.

## Metadata registration entry points

Hand-authored declarations live in `Source/Common/MetadataRegistration.h`:

- `RegisterServerMetadata()`
- `RegisterClientMetadata()`
- `RegisterMapperMetadata()`
- `RegisterServerStubMetadata()`
- `RegisterClientStubMetadata()`
- `RegisterMapperStubMetadata()`
- `RegisterDynamicMetadata()`
- `ReadMetadataBin()`

`Source/Common/MetadataRegistration.template.cpp` is the template used to generate side-specific registration files. It contains code-generation markers such as `///@ CodeGen RegisterHelpers` and `///@ CodeGen Register`.

`Source/Common/GenericCode.template.cpp` is the template for generated common code.

## Engine hook tags

Project/native extension code can mark selected C++ functions with `///@ EngineHook`. `BuildTools/codegen.py` validates hook names and emits no-op stubs for hooks that the embedding project does not implement. Current hook names recognized by the generator are:

- `ApplicationInitHook(AppInitFlags, GlobalSettings&)`
- `ApplicationShutdownHook()`
- `ServerInitHook(ServerEngine*)`
- `ClientInitHook(ClientEngine*)`
- `ClientStartupSettingsHook(GlobalSettings&, int32_t clientIndex, bool embedded)`
- `SetupBakersHook(...)`
- `CheckCritterVisibilityHook(...)`
- `CheckItemVisibilityHook(...)`

`ClientStartupSettingsHook` is called by app entry points immediately before constructing a client engine. Use it for project-owned startup setting adjustments; do not use it as a gameplay authority bypass.

`ApplicationShutdownHook` is a native lifecycle hook for project-owned process integrations that must be stopped before a client runtime DLL is unloaded. It is intentionally not part of the compatibility hash because it does not change script metadata, saved data, or the network contract.

## Dynamic metadata

`Source/Common/MetadataRegistration.cpp` implements `RegisterDynamicMetadata()`. It reads binary metadata sections and dispatches them into typed registration steps such as:

- enums
- entities
- entity holders
- fixed/value/reference types
- properties
- events
- remote calls
- settings
- migration rules

This is the runtime side of metadata that can be loaded from generated/baked data rather than compiled static registration alone.

Migration rules are generic `(kind, extra-info, target → replacement)` remaps with transitive resolution, authored as `///@ MigrationRule <Kind> ...`. Beyond `Proto`/`Property` (applied at proto lookup and property-name resolution), the `Enum` kind is consulted by `PropertiesSerializator` when a persisted enum value **name** no longer resolves on load: the rule remaps the old name to a current value — for scalar enum properties and enum dict keys — instead of throwing `EnumResolveException`. This keeps removed/renamed enum values from bricking old saves.

## Properties and generated contracts

`Source/Common/Properties.h` and `Source/Common/Properties.cpp` define the property runtime model used by entities and metadata. Key concepts include:

- `PropertyRawData`
- `Property`
- `PropertyRegistrator`
- `Properties`
- property getter/setter/post-set callbacks
- base type, struct layout, and serialization-related descriptors

Fixed value-type layouts are shared by native C++, AngelScript registration, and metadata field traversal. `hstring` therefore has an explicit ABI invariant: `sizeof(hstring) == sizeof(hstring::hash_t) == 8` on every supported target. On 32-bit targets the pointer-backed handle carries trailing padding to preserve that width and keep composite offsets (for example `TextPackKey`) platform-independent. The padding is not wire data: RPC/property serializers still convert the handle through `as_hash()` and resolve the received hash through the target engine's hash resolver.

When property metadata changes, inspect both the property runtime and the generator inputs/templates. Script-visible nullability or API changes should also update [Scripting.md](Scripting.md), [ScriptMethodsMap.md](ScriptMethodsMap.md), and [Nullability.md](Nullability.md) as applicable.

## Public API relationship

[../PUBLIC_API.md](../PUBLIC_API.md) documents public build/API knobs such as build toggles and project helper functions like resource/package additions. Keep public API docs high-level and stable; put generator internals here.

## Metadata and baker relationship

Metadata generation and metadata baking are related but not identical:

- Codegen produces generated C++/include files used by compiled targets.
- `MetadataBaker` participates in resource baking and can produce metadata data for runtime loading.
- `RegisterDynamicMetadata()` consumes metadata binary data from resources.

For resource baking details, see [BakingPipeline.md](BakingPipeline.md).

## Tests to inspect

Relevant tests include:

- `Source/Tests/Test_EngineMetadata.cpp`
- `Source/Tests/Test_MetadataBaker.cpp`
- `Source/Tests/Test_Properties.cpp`
- Baker/codegen-adjacent tests such as `Test_BakerSetup.cpp` and the specific baker tests when metadata affects baked resources.

If a generated script API change is involved, inspect AngelScript-related tests as well.

## Change routing

- CMake generator arguments/output list: `BuildTools/cmake/stages/Codegen.cmake`.
- Generator script behavior: `BuildTools/codegen.py`. Keep this limited to generated C++ metadata/wrappers; WASM frontend API manifests and language bindings belong to bake-time `WasmBaker`/frontend helpers.
- Static metadata registration template: `Source/Common/MetadataRegistration.template.cpp`.
- Generated common code template: `Source/Common/GenericCode.template.cpp`.
- Runtime dynamic metadata reader/registrar: `Source/Common/MetadataRegistration.cpp`.
- Property model: `Source/Common/Properties.*` and entity/prototype metadata code.
- Metadata resource baking: `Source/Tools/MetadataBaker.*` and [BakingPipeline.md](BakingPipeline.md).
- Script runtime and script-visible signatures: [Scripting.md](Scripting.md), [ScriptMethodsMap.md](ScriptMethodsMap.md), and [Nullability.md](Nullability.md).

## Validation checklist

1. Configure from an embedding project root so project metadata sources are available.
2. Run normal code generation and verify generated files are updated as expected.
3. Run forced code generation when generator caching/dependency behavior changes.
4. Build the smallest target that compiles the generated files.
5. Run metadata/property tests relevant to the change.
6. If metadata is baked, run the relevant baker test and bake target.
7. Update docs that expose changed public contracts, especially [Scripting.md](Scripting.md), [ScriptMethodsMap.md](ScriptMethodsMap.md), [Nullability.md](Nullability.md), and [../PUBLIC_API.md](../PUBLIC_API.md) when applicable.
