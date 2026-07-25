# Unit Tests

This directory contains deterministic engine tests built into the generated test application. For the full maintained test map, validation routing, and coverage target details, see [../../Docs/Testing.md](../../Docs/Testing.md).

## Framework and target

- Framework: Catch2 (`catch_amalgamated.hpp`)
- Test application entry point: `Source/Applications/TestingApp.cpp`
- Test source list owner: `BuildTools/cmake/stages/EngineSources.cmake` (`FO_TESTS_SOURCE`)
- Generated executable target shape: `<ProjectDevName>_UnitTests`
- Generated run target: `RunUnitTests`
- Generated coverage target shape: `<ProjectDevName>_CodeCoverage` plus `RunCodeCoverage`, `GenerateCodeCoverageReport`, and `AnalyzeCodeCoverage` when coverage is enabled

In Last Frontier-style builds the dev-name prefix is `LF`, so the common target names are `LF_UnitTests` and `RunUnitTests`. Treat that prefix as embedding-project generated, not universal engine API.

## Current test suites

Current count: **95** `Test_*.cpp` suites.

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
- `Source/Tests/Test_ModelAnimation.cpp`
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
- `Source/Tests/Test_ModelMeshData.cpp`
- `Source/Tests/Test_ModelAnimationData.cpp`
- `Source/Tests/Test_ModelAnimationConverter.cpp`
- `Source/Tests/Test_ModelAnimationPoseProcedural.cpp`
- `Source/Tests/Test_ModelAnimationRuntime.cpp`
- `Source/Tests/Test_ModelSkeletonCompatibility.cpp`
- `Source/Tests/Test_ModelSourceLoader.cpp`
- `Source/Tests/Test_OzzAnimation.cpp`
- `Source/Tests/Test_ProtoBaker.cpp`
- `Source/Tests/Test_ProtoTextBaker.cpp`
- `Source/Tests/Test_RawCopyBaker.cpp`
- `Source/Tests/Test_TextBaker.cpp`
- `Source/Tests/Test_TextureAtlas.cpp`

The model-animation coverage is intentionally split between wire-format corruption
tests (`Test_ModelAnimationData.cpp`), offline conversion and production runtime
ownership/type-tag tests (`Test_ModelAnimationConverter.cpp`), real
baker-to-runtime/binding resolution (`Test_ModelBaker.cpp`), and the model
animation controller/timeline contract (`Test_ModelAnimation.cpp`).
Renderer-independent rest pose matrices and canonical joint links live in
`Test_ModelAnimationRuntime.cpp`;
bounded procedural rotations and exact link-matrix overrides live in
`Test_ModelAnimationPoseProcedural.cpp`. Keep
these boundaries green independently while the immutable production payload is
staged ahead of the atomic sampler/matrix cutover.

### Rendering/frontend smoke tests

- `Source/Tests/Test_EffekseerParticleRuntime.cpp` — runs cooked legacy and modern Effekseer
  effects through the native runtime's real Sprite/Ring callbacks and validates deterministic
  multi-instance topology, FOnline geometry, atlas UVs, all three Z-sort modes, Ring index-budget
  chunking, and facade-level scale reapplication without respawn or timing reset.
- `Source/Tests/Test_ParticleBaker.cpp` — covers `.efkproj` source discovery,
  `.spark`/`.efkproj` output-key mapping, generated binary validation, rejection
  of authored `.spk`/`.efk` runtime inputs, and SPARK seeded-stream isolation
  across interleaved effects and independent engine contexts. The build/integration bake path
  exercises the native fixed-profile exporter on real XML projects.
- `Source/Tests/Test_Rendering.cpp`

## Running tests

Prefer running the generated run target from a configured build directory:

```bash
cmake --build . --config RelWithDebInfo --target RunUnitTests
```

Use the executable target directly when you need Catch2 arguments. In common Last Frontier output layouts, binaries are emitted under `Binaries/Tests-*`, for example:

- Windows: `Binaries/Tests-Windows-win64/LF_UnitTests.exe`
- Linux: `Binaries/Tests-Linux-x64/LF_UnitTests`

## Running code coverage

Coverage builds use the `FO_CODE_COVERAGE` path documented in [../../Docs/Testing.md](../../Docs/Testing.md). The generated targets are:

- `RunCodeCoverage`
- `GenerateCodeCoverageReport`
- `AnalyzeCodeCoverage`

Coverage reports are emitted under `CodeCoverage/<Toolchain>/<Platform-Config>/`
and exclude `Source/Tests/` from the reported source denominator.

## Notes

- Keep tests deterministic and platform-stable.
- Avoid network, filesystem, and timing-sensitive behavior in unit suites unless mocked or isolated.
- New test sources must be added to `FO_TESTS_SOURCE` in `BuildTools/cmake/stages/EngineSources.cmake`.
- Update [../../Docs/Testing.md](../../Docs/Testing.md) and this README when adding, removing, or regrouping test suites.
- Treat `RunUnitTests` as the minimum broad validation baseline for engine-side changes after focused tests pass.
