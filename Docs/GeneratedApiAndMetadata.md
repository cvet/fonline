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
- `Source/Common/MetadataRegistration-Template.cpp`
- `Source/Common/GenericCode-Template.cpp`
- `Source/Common/Properties.h`
- `Source/Common/Properties.cpp`
- `Source/Common/Entity.h`
- `Source/Common/Entity.cpp`
- `Source/Tools/MetadataBaker.h`
- `Source/Tools/MetadataBaker.cpp`
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

The stage creates normal and forced code-generation command targets and appends `CodeGeneration` to `FO_GEN_DEPENDENCIES`.

## Generated outputs

`Codegen.cmake` declares generated outputs under `GeneratedSource/`, including:

- `CodeGenTouch`
- `Version-Include.h`
- `EmbeddedResources-Include.h`
- `InternalConfig-Include.h`
- `MetadataRegistration-Server.cpp`
- `MetadataRegistration-Client.cpp`
- `MetadataRegistration-Mapper.cpp`
- `MetadataRegistration-ServerStub.cpp`
- `MetadataRegistration-ClientStub.cpp`
- `MetadataRegistration-MapperStub.cpp`
- `GenericCode-Common.cpp`

These file names are useful for understanding build flow, but changes should usually be made in templates, annotations, metadata sources, or generator scripts rather than in generated output.

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

`Source/Common/MetadataRegistration-Template.cpp` is the template used to generate side-specific registration files. It contains code-generation markers such as `///@ CodeGen RegisterHelpers` and `///@ CodeGen Register`.

`Source/Common/GenericCode-Template.cpp` is the template for generated common code.

## Engine hook tags

Project/native extension code can mark selected C++ functions with `///@ EngineHook`. `BuildTools/codegen.py` validates hook names and emits no-op stubs for hooks that the embedding project does not implement. Current hook names recognized by the generator are:

- `ApplicationInitHook(AppInitFlags, GlobalSettings&)`
- `ApplicationShutdownHook()`
- `ServerInitHook(ServerEngine*)`
- `ClientInitHook(ClientEngine*)`
- `ClientStartupSettingsHook(GlobalSettings&, int32_t clientIndex, bool embedded)`
- `ConfigSectionParseHook(...)`
- `ConfigEntryParseHook(...)`
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
- Generator script behavior: `BuildTools/codegen.py`.
- Static metadata registration template: `Source/Common/MetadataRegistration-Template.cpp`.
- Generated common code template: `Source/Common/GenericCode-Template.cpp`.
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
