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
- `ReadMetadataVersion()`

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

### Metadata version

**Invariant: a server and every client connected to it run on metadata produced by one bake.** This is not a
preference — the property index space that entity data travels by *is* the registration order of that metadata, so
two sides holding different bakes silently address different properties. A divergence is a defect in how the build
or the deploy was done; it is detected and refused, never tolerated or worked around.

Why the compatibility version does not cover it: `BuildTools/codegen.py` only receives the engine and embedding-
project C++ meta sources, so `FO_COMPATIBILITY_VERSION` changes with the binaries. Project `///@ Property`
declarations live in scripts and are registered at runtime from the baked metadata
(`RegisterDynamicMetadataProperties`), which means the property layout is a property of the *resources*, not of the
executable.

`MetadataBaker` therefore derives a **metadata version** from **every** codegen tag it parsed, in a deterministic
order. The input is the raw tag stream as read from the sources — *before* any target filtering — so client, server
and mapper of one bake always derive the same value even though their section bodies differ (`Entity`, `Event`,
`Setting`, `RemoteCall` are filtered per target on the way out). Hashing the finished file instead would not work
for exactly that reason; hashing the raw tags has no such limit, so no kind of divergence stays invisible: a
property insertion that shifts every reg index below it, a changed struct layout, a renamed enum entry, a new remote
call — all of them change the version, and all of them mean the two sides came from different bakes.

Every `Metadata.fometa-*` therefore opens with a fixed header, ahead of the section table:

| Field | Type | Purpose |
|-------|------|---------|
| magic | `uint32` | `METADATA_FILE_MAGIC` — a foreign or truncated file is rejected at the first bytes |
| file version | `uint16` | `METADATA_FILE_VERSION` — bumped when this file layout changes; a mismatch means "rebake" |
| metadata version | `uint16` length + bytes | the value above |

`MakeMetadataHeader()` writes it and `ReadMetadataHeader()` reads it, both in `MetadataRegistration.cpp`, so the
format lives in one place. `RegisterDynamicMetadata()` reads the header before any section and hands the version to
`EngineMetadata::RegisterMetadataVersion()`; `ReadMetadataVersion()` reads *only* the header, which is
what the updater and the server startup check use — neither walks the sections to answer "which bake is this". The
value is read back through `EngineMetadata::GetMetadataVersion()` — it is computed, not configured, so it is
deliberately **not** a setting (`Network.ForceMetadataVersion` exists only to simulate a divergence in tests).

Four layers keep the invariant, in the order they apply:

1. **One bake produces both sides.** `Baking.ServerResources` and `Baking.ClientResources` must be deployed
   together; refreshing one of them is the classic way to break this.
2. **The server refuses to distribute foreign resources.** `UpdaterBackend::LoadFromClientResources` reads the
   layout version out of the client packs it is about to hand out and fails startup (`UpdaterException`) when it
   differs from the one the server itself loaded.
3. **The updater syncs before a client exists.** `Updater::FinishResourcesUpdate` re-reads the version from the
   local packs after the sync and reports `UpdaterResult::MetadataMismatch` unless it equals the server's, so a
   `ClientEngine` is never constructed against data the server cannot talk to.
4. **The handshake is the last line.** The client sends its version, the server compares and answers with a verdict
   plus its own version; see [ClientUpdater.md](ClientUpdater.md).

Deserialization is guarded independently of all four: `Properties::VerifyRestoredPropertyData()` checks every
property write coming from a serialized payload (target enabled, non-virtual, plain size matches) and throws
`VerificationException` instead of reaching the strong assert inside `SetRawData` — a mismatch has to be diagnosable,
not a process termination inside a memcpy.

**When a divergence is reported, find the cause — do not silence the check.** The useful facts are in the logs: the
server prints `Metadata version:` at startup and names both versions when it rejects a client; the updater prints
the local version, the server version, and the resource directory it read. From there the question is always the
same: which of the two resource directories came from a different bake, and why.

Tests: `Test_MetadataBaker.cpp` (one version shared by every target, changed by a property insertion),
`Test_Properties.cpp` (`PropertiesRestoreRejectsForeignMetadata`),
`Test_ClientServerIntegration.cpp` (`ServerReportsMetadataMismatchInHandshake`).

## Properties and generated contracts

`Source/Common/Properties.h` and `Source/Common/Properties.cpp` define the property runtime model used by entities and metadata. Key concepts include:

- `PropertyRawData`
- `Property`
- `PropertyRegistrar`
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
- Generator script behavior: `BuildTools/codegen.py`.
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
