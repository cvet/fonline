# Unit Tests

This directory contains deterministic engine tests built into the generated test application. For the full maintained test map, validation routing, and coverage target details, see [../../Docs/Testing.md](../../Docs/Testing.md).

## Framework and target

- Framework: Catch2 (`catch_amalgamated.hpp`)
- Test application entry point: `Source/Applications/TestingApp.cpp`
- Test source list owner: `BuildTools/cmake/stages/EngineSources.cmake` (`FO_TESTS_SOURCE`)
- Generated executable target shape: `<ProjectDevName>_UnitTests`
- Generated run target: `RunUnitTests`
- Generated coverage target shape: `<ProjectDevName>_CodeCoverage` plus `RunCodeCoverage`, `GenerateCodeCoverageReport`, and `AnalyzeCodeCoverage` when coverage is enabled

The executable target uses the embedding project's development-name prefix (`<ProjectDevName>_UnitTests`); `RunUnitTests` is the generated runner target. Treat the prefix as project-generated, not universal engine API.

## Current test suites

The complete source-backed filename list and count are generated in [source-inventory.json](../../Docs/generated/source-inventory.json). [Testing.md](../../Docs/Testing.md) provides the maintained ownership groups and validation routing.

After adding, removing, or renaming a `Test_*.cpp` file, regenerate the inventory from the engine root:

```bash
python BuildTools/docs_inventory.py --write
python BuildTools/docs_inventory.py --check
```

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

The model-pipeline coverage is intentionally split. `Test_ModelMeshData.cpp`
owns the mesh-only wire contract; `Test_ModelSourceLoader.cpp` and
`Test_ModelAnimationConverter.cpp` own source extraction and canonical
conversion; `Test_ModelAnimationData.cpp` owns the versioned rig archive;
`Test_ModelBaker.cpp` crosses source-backed baking and binding resolution; and
the animation, runtime-pose, procedural, skeleton-compatibility, and Ozz suites
cover the production sampling and matrix path. Keep these boundaries green
independently, then use `Test_ClientEngine.cpp` for the baker-to-client parser
boundary.

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

The documentation CI job rejects stale generated inventory; the groups above are representative and do not replace the generated complete list.

## Running tests

Prefer running the generated run target from a configured build directory:

```bash
cmake --build . --config RelWithDebInfo --target RunUnitTests
```

Use the executable target directly when you need Catch2 arguments. Generated test binaries are normally emitted under `Binaries/Tests-*`, for example:

- Windows: `Binaries/Tests-Windows-win64/<ProjectDevName>_UnitTests.exe`
- Linux: `Binaries/Tests-Linux-x64/<ProjectDevName>_UnitTests`

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
