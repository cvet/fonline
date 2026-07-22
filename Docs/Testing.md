# Testing

> Engine-owned documentation. This page maps the current engine test executable, generated test targets, coverage targets, and every `Source/Tests/Test_*.cpp` suite currently present in the checkout.

## Purpose

Use this page when choosing validation for an engine change or when adding/removing tests. The source-tree README at [../Source/Tests/README.md](../Source/Tests/README.md) is a short entry point; this page is the maintained full test map.

## Source paths inspected

- `Source/Applications/TestingApp.cpp`
- `Source/Tests/README.md`
- all current `Source/Tests/Test_*.cpp` files
- `BuildTools/cmake/stages/EngineSources.cmake`
- `BuildTools/cmake/stages/Applications.cmake`
- `BuildTools/cmake/stages/Init.cmake`
- `BuildTools/codecoverage.py`
- `BuildTools/validate.sh`
- `BuildTools/validate.cmd`
- parent VS Code task references where available

## Test runner model

`Source/Applications/TestingApp.cpp` is the test application entry point. It requires `FO_TESTING_APP`, initializes the application layer with `InitApp(-1, nullptr)`, marks `IsTestingInProgress`, and delegates execution to `Catch::Session().run(argc, argv)`.

`BuildTools/cmake/stages/EngineSources.cmake` owns `FO_TESTS_SOURCE`, the explicit list of test source files compiled into test builds. `BuildTools/cmake/stages/Applications.cmake` builds test executables through `SetupTestBuild(name)`:

- `UnitTests` when `FO_UNIT_TESTS` is enabled;
- `CodeCoverage` when `FO_CODE_COVERAGE` is enabled.

For an embedding project with dev name `LF`, the standard generated names are `LF_UnitTests`, `RunUnitTests`, `LF_CodeCoverage`, `RunCodeCoverage`, `GenerateCodeCoverageReport`, and `AnalyzeCodeCoverage`. Treat the prefix as project-generated, not universal.

## Running tests

Preferred local baseline from a configured build:

```bash
cmake --build . --config RelWithDebInfo --target RunUnitTests
```

With `FO_EFFEKSEER_PARTICLES` enabled, the focused `[particle]` Catch2 cases
invoke the published helper through the production `ParticleBaker` path. They
cover text compilation, dependency invalidation, malformed XML, and rejection
of cooked files presented as authored inputs.

The executable target can also be invoked directly when you need Catch2 arguments. In Last Frontier-style layouts, test binaries are emitted under `Binaries/Tests-*`, for example `Binaries/Tests-Windows-win64/LF_UnitTests.exe` or `Binaries/Tests-Linux-x64/LF_UnitTests`.

For broad validation scenarios, the BuildTools validators can run selected scenarios:

```bash
Engine/BuildTools/validate.sh unit-tests
Engine/BuildTools/validate.sh android-arm64-client linux-client linux-server
```

Use the smallest focused tests first, then the broader run target when the change crosses subsystem boundaries.

### Unit tests under sanitizers

The unit tests also run under Clang sanitizers via dedicated validators, which select
the matching `San_*` build type and run `RunUnitTests` instrumented:

```bash
Engine/BuildTools/validate.sh unit-tests-san-address    # AddressSanitizer (+LeakSanitizer)
Engine/BuildTools/validate.sh unit-tests-san-memory     # MemorySanitizer (requires Workspace/msan-libcxx)
Engine/BuildTools/validate.sh unit-tests-san-undefined  # UndefinedBehaviorSanitizer
Engine/BuildTools/validate.sh unit-tests-san-thread     # ThreadSanitizer
```

The `validate.yml` workflow runs these as a `unit-tests-sanitizers` matrix job.
ASan/MSan/UBSan/TSan are blocking legs. The `unit-tests-san-memory` validator prepares
`Workspace/msan-libcxx` by building LLVM's `libc++`, `libc++abi`, and `libunwind`
with MSan instrumentation, then configures `San_Memory` with `FO_MSAN_LIBCXX_ROOT`.
The runtime build applies a narrow libunwind ignorelist so C++ exception and
sanitizer-report unwinding do not self-report on ABI register snapshots. Engine
native stack capture and the backward-cpp signal handler are disabled under
`FO_MEMORY_SANITIZER` so MSan owns fatal reports. `unit-tests-san-memory-with-origins`
is available locally as the slower diagnostic variant when a future MSan finding
needs origin tracking. `San_DataFlow` remains
intentionally unwired: DataFlowSanitizer is a taint-tracking framework, not a
defect detector.

On MSVC, the `San_Address`/`Debug_San_Address` configs additionally link executables with
`/STACK:8388608` (`AddExecutableApplication` in `BuildTools/cmake/helpers/Build.cmake`):
ASan's stack-frame inflation overflows the 1 MiB Windows executable default on recursion
depths that fit every production configuration, so sanitizer runs get the same 8 MiB
reserve that Linux runs already have from the default rlimit. Production configs keep the
1 MiB default.

Vendored third-party libraries are excluded from UBSan's `-fsanitize=function` and
`-fsanitize=alignment` checks (the rest of `-fsanitize=undefined` still applies to them).
`DisableLibWarnings` adds `-fno-sanitize=function,alignment` on the
`San_Undefined`/`San_Address_Undefined` configs because several vendored libraries trip
those two checks by design:

- `function`: AngelScript's script-call dispatch invokes registered C functions through
  `bool(*)(void*,void*)` and similar signatures, and C callback APIs do the same.
- `alignment`: AngelScript builds its bytecode in an `asDWORD[]` (4-byte) buffer and packs
  pointer-sized `asPWORD` operands at 4-byte-aligned slots
  (`*(asPWORD*)(bc+1) = ...` in `GenerateFactoryStubForTemplateObjectInstance`), which UBSan
  reports as a misaligned store even though it is correct on every architecture the engine
  targets.

Both are third-party idioms, not undefined behaviour in engine code, so they must not fail
the UBSan leg (which CI runs with `halt_on_error=1`). First-party engine code keeps both
checks fully active.

LeakSanitizer runs as part of the address-sanitizer leg (CI sets `ASAN_OPTIONS=detect_leaks=1`).
It runs with **no suppression list** — every leak it can report is fixed at the source rather than
masked. Notable cases:

- backward-cpp's libbfd stack-trace resolver (`Source/Essentials/StackTrace.cpp`) caches each
  binary's ELF symbol table and DWARF debug info inside libbfd, hung off the open `bfd` handle, and
  never fully frees it on `bfd_close`. The resolver is therefore a single process-lifetime instance
  (`GetNativeTraceResolver`, serialized by `StackTraceState::NativeResolverLocker`): it is created
  once, never destroyed, and stays reachable from a static root, so each binary is symbolized once
  and those libbfd caches remain reachable — LSan does not report them.
- The AngelScript backend deletes the preprocessor line-number translator during engine userdata
  cleanup, and each SPARK context frees its `IOManager` converters at context shutdown.
- Owning containers free their contents transitively: e.g. `EntityTypeDesc::PropRegistrator` is a
  `unique_ptr` so every `PropertyRegistrator` (and the `Property` objects it holds) is freed when
  `EngineMetadata`'s type maps are destroyed.

## Code coverage

When `FO_CODE_COVERAGE` is enabled, `BuildTools/cmake/stages/Init.cmake` selects the backend from the compiler:

- MSVC / clang-cl: MSVC-style coverage output;
- Clang: LLVM profile/coverage mapping;
- GCC: GCC/lcov-style coverage flags.

`BuildTools/cmake/stages/Applications.cmake` wires coverage command targets through `BuildTools/codecoverage.py`:

- `CleanCodeCoverageData`
- `RunCodeCoverage`
- `GenerateCodeCoverageReport`
- `AnalyzeCodeCoverage`

Coverage output is rooted under `CodeCoverage/<Toolchain>/<Platform-Config>/`.
`BuildTools/codecoverage.py` reports first-party production engine sources under
`Engine/Source/`; it excludes `Source/Tests/`, `ThirdParty/`,
`GeneratedSource/`, and `Applications/` from the denominator. See
[../Source/Tests/README.md](../Source/Tests/README.md) for current local task
notes.

## Current test inventory

Current count: **86** `Test_*.cpp` suites.

### Essentials and low-level utilities

- `Source/Tests/Test_BaseLogging.cpp`
- `Source/Tests/Test_BasicCore.cpp`
- `Source/Tests/Test_CommonHelpers.cpp`
- `Source/Tests/Test_Compressor.cpp`
- `Source/Tests/Test_Containers.cpp`
- `Source/Tests/Test_DataSerialization.cpp`
- `Source/Tests/Test_DiskFileSystem.cpp`
- `Source/Tests/Test_ExceptionHandling.cpp`
- `Source/Tests/Test_ExtendedTypes.cpp`
- `Source/Tests/Test_GenericUtils.cpp`
- `Source/Tests/Test_GlobalData.cpp`
- `Source/Tests/Test_HashedString.cpp`
- `Source/Tests/Test_Logging.cpp`
- `Source/Tests/Test_MemorySystem.cpp`
- `Source/Tests/Test_NetSockets.cpp`
- `Source/Tests/Test_Platform.cpp`
- `Source/Tests/Test_SafeArithmetics.cpp`
- `Source/Tests/Test_SmartPointers.cpp`
- `Source/Tests/Test_StackTrace.cpp`
- `Source/Tests/Test_StringUtils.cpp`
- `Source/Tests/Test_StrongType.cpp`
- `Source/Tests/Test_TimeRelated.cpp`
- `Source/Tests/Test_WorkThread.cpp`
- `Source/Tests/Test_WorkerPool.cpp`

### Configuration, data sources, files, and caches

- `Source/Tests/Test_CacheStorage.cpp`
- `Source/Tests/Test_ConfigFile.cpp`
- `Source/Tests/Test_DataSource.cpp`
- `Source/Tests/Test_FileSystem.cpp`
- `Source/Tests/Test_Settings.cpp`

### Common runtime model

- `Source/Tests/Test_AnyData.cpp`
- `Source/Tests/Test_Common.cpp`
- `Source/Tests/Test_EngineMetadata.cpp`
- `Source/Tests/Test_EntityLifecycle.cpp`
- `Source/Tests/Test_EntityProtos.cpp`
- `Source/Tests/Test_Geometry.cpp`
- `Source/Tests/Test_LineTracer.cpp`
- `Source/Tests/Test_MapLoader.cpp`
- `Source/Tests/Test_Movement.cpp`
- `Source/Tests/Test_PathFinding.cpp`
- `Source/Tests/Test_Properties.cpp`
- `Source/Tests/Test_ProtoManager.cpp`
- `Source/Tests/Test_TextPack.cpp`
- `Source/Tests/Test_Timer.cpp`
- `Source/Tests/Test_TwoDimensionalGrid.cpp`

### Networking and server/client integration

- `Source/Tests/Test_ClientDataValidation.cpp`
- `Source/Tests/Test_ClientEngine.cpp`
- `Source/Tests/Test_ClientRuntimeApi.cpp`
- `Source/Tests/Test_ClientServerIntegration.cpp`
- `Source/Tests/Test_DataBase.cpp`
- `Source/Tests/Test_EntitySync.cpp`
- `Source/Tests/Test_FogOfWar.cpp`
- `Source/Tests/Test_LocationAndEntityMgmt.cpp`
- `Source/Tests/Test_NetBuffer.cpp`
- `Source/Tests/Test_NetworkClient.cpp`
- `Source/Tests/Test_NetworkServer.cpp`
- `Source/Tests/Test_NetworkUdp.cpp`
- `Source/Tests/Test_ServerAdvancedOps.cpp`
- `Source/Tests/Test_ServerEngine.cpp`
- `Source/Tests/Test_ServerEventContracts.cpp`
- `Source/Tests/Test_ServerItems.cpp`
- `Source/Tests/Test_ServerMapOperations.cpp`

### Scripting and script-visible APIs

- `Source/Tests/Test_AngelScriptAlignment.cpp`
- `Source/Tests/Test_AngelScriptAttributes.cpp`
- `Source/Tests/Test_AngelScriptBytecode.cpp`
- `Source/Tests/Test_AngelScriptCall.cpp`
- `Source/Tests/Test_CommonScriptMethods.cpp`
- `Source/Tests/Test_ScriptBuiltins.cpp`
- `Source/Tests/Test_ScriptEntityOps.cpp`
- `Source/Tests/Test_ServerScriptMethods.cpp`

### Bakers and tools

- `Source/Tests/Test_AngelScriptBaker.cpp`
- `Source/Tests/Test_BakerSetup.cpp`
- `Source/Tests/Test_ConfigBaker.cpp`
- `Source/Tests/Test_EffectBaker.cpp`
- `Source/Tests/Test_ImageBaker.cpp`
- `Source/Tests/Test_MapBaker.cpp`
- `Source/Tests/Test_Mapper.cpp`
- `Source/Tests/Test_MetadataBaker.cpp`
- `Source/Tests/Test_ModelBaker.cpp`
- `Source/Tests/Test_ParticleBaker.cpp`
- `Source/Tests/Test_ProtoBaker.cpp`
- `Source/Tests/Test_ProtoTextBaker.cpp`
- `Source/Tests/Test_RawCopyBaker.cpp`
- `Source/Tests/Test_TextBaker.cpp`
- `Source/Tests/Test_TextureAtlas.cpp`

### Rendering/frontend smoke tests

- `Source/Tests/Test_EffekseerParticleRuntime.cpp` — runs cooked legacy and modern Effekseer
  effects through the native runtime's real Sprite/Ring callbacks and validates deterministic
  multi-instance topology, FOnline geometry, atlas UVs, all three Z-sort modes, Ring index-budget
  chunking, and facade-level scale reapplication without respawn or timing reset.
- `Source/Tests/Test_ParticleBaker.cpp` — covers `.efkproj` source discovery,
  `.spark`/`.efkproj` output-key mapping, generated binary validation, and
  rejection of authored `.spk`/`.efk` runtime inputs. The build/integration
  bake path exercises the native fixed-profile exporter on real XML projects.
- `Source/Tests/Test_Rendering.cpp`

## Validation routing by change type

- Essentials utilities: start with [Essentials.md](Essentials.md) and the essentials tests listed above.
- Config, file lookup, caches, resource packs: [ConfigurationAndDataSources.md](ConfigurationAndDataSources.md), parser/filesystem/cache tests, and affected bake/runtime consumers.
- BuildTools/CMake/generation: [BuildToolsPipeline.md](BuildToolsPipeline.md), [GeneratedApiAndMetadata.md](GeneratedApiAndMetadata.md), codegen/property/metadata tests, and at least one generated target.
- Bakers/resources: [BakingPipeline.md](BakingPipeline.md) and the matching baker tests.
- Runtime entity/map/persistence/networking: [EntityModel.md](EntityModel.md), [MapsMovementGeometry.md](MapsMovementGeometry.md), [Persistence.md](Persistence.md), [Networking.md](Networking.md), and the focused runtime tests.
- Client/frontend/server: [ClientRuntime.md](ClientRuntime.md), [FrontendAndRendering.md](FrontendAndRendering.md), [ServerRuntime.md](ServerRuntime.md), and the matching integration/smoke tests.
- Scripting: [Scripting.md](Scripting.md), [ScriptMethodsMap.md](ScriptMethodsMap.md), [Nullability.md](Nullability.md), and the script/baker/method tests.

## Adding or removing tests

1. Add the new `Source/Tests/Test_*.cpp` file with deterministic Catch2 tests.
2. Add it to `FO_TESTS_SOURCE` in `BuildTools/cmake/stages/EngineSources.cmake`.
3. Update this page and [../Source/Tests/README.md](../Source/Tests/README.md) so the inventory stays complete.
4. Run the focused test binary and, when practical, `RunUnitTests`.
5. If coverage behavior changed, verify the relevant coverage target.

## Validation checklist

1. Every current `Source/Tests/Test_*.cpp` file should appear in this page.
2. No deleted/nonexistent test file should be listed.
3. Target names should be described as generated from `FO_DEV_NAME`, not hard-coded as universal engine names.
4. If `TestingApp.cpp`, `FO_TESTS_SOURCE`, or coverage target wiring changes, update this page in the same change.
