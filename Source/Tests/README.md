---
layout: default
title: Unit Tests
permalink: /Source/Tests/README.html
locale: en
document_id: unit-tests-readme
---

# Unit Tests

This directory contains deterministic engine tests built into the generated test application. For the full maintained test map, validation routing, and coverage target details, see [Testing](../../Docs/en/contributing/testing/).

## Framework and target

- Framework: Catch2 (`catch_amalgamated.hpp`)
- Test application entry point: `Source/Applications/TestingApp.cpp`
- Test source list owner: `BuildTools/cmake/stages/EngineSources.cmake` (`FO_TESTS_SOURCE`)
- Generated executable target shape: `<ProjectDevName>_UnitTests`
- Generated run target: `RunUnitTests`
- Generated coverage target shape: `<ProjectDevName>_CodeCoverage` plus `RunCodeCoverage`, `GenerateCodeCoverageReport`, and `AnalyzeCodeCoverage` when coverage is enabled

The executable target uses the embedding project's development-name prefix (`<ProjectDevName>_UnitTests`); `RunUnitTests` is the generated runner target. Treat the prefix as project-generated, not universal engine API.

## Current test suites

The complete source-backed filename list and count are generated in [source-inventory.json](https://fonline.ru/Docs/generated/source-inventory.json). [Testing](../../Docs/en/contributing/testing/) provides the maintained ownership groups and validation routing.

`Test_ClientEntityLifetime.cpp` runs with either scripting backend. It verifies that registry lookups
retain their result across another thread's final release and that an older entity cannot unregister
a successor with the same id.

`FO_TESTS_SOURCE` is unconditional. Backend-specific translation units own complete guards; general native suites run with either backend. Shared header fixtures keep AS compilation guarded. See the [backend ownership contract](../../Docs/en/contributing/testing/#test-runner-model).

`Test_ManagedScriptRuntime.cpp` compiles `Fixtures/ManagedRuntimeProbe.cs` with the real baker/SDK and runs server/client/mapper bridges and two-server isolation. It requires the configured SDK/runtime; the [test map](../../Docs/en/contributing/testing/#test-runner-model) owns coverage and limits.

After adding, removing, or renaming a `Test_*.cpp` file, regenerate the inventory from the engine root:

```bash
python BuildTools/docs_inventory.py --write
python BuildTools/docs_inventory.py --check
```

The attachment teardown regressions run with either scripting backend: `CritterAttachmentTeardown`
checks server links, native reports and delivery to the surviving player; `ClientCritterFinalizationDropsAttachmentIds`
checks client cache release while keeping the peer alive. They live in the existing server/client entity-lifetime suites.

- `Source/Tests/Test_AudioBakerAcm.cpp` - optional Fallout ACM decoder and invalid-input coverage.
- `Source/Tests/Test_SpriteSheet.cpp` - independent frame tables, explicit frame offsets, directional resolver caching and bounds.

## Running tests

Prefer running the generated run target from a configured build directory:

```bash
cmake --build . --config RelWithDebInfo --target RunUnitTests
```

Use the executable target directly when you need Catch2 arguments. Generated test binaries are normally emitted under `Binaries/Tests-*`, for example:

- Windows: `Binaries/Tests-Windows-win64/<ProjectDevName>_UnitTests.exe`
- Linux: `Binaries/Tests-Linux-x64/<ProjectDevName>_UnitTests`

## Running code coverage

Coverage builds use the `FO_CODE_COVERAGE` path documented in [Testing](../../Docs/en/contributing/testing/). The generated targets are:

- `RunCodeCoverage`
- `GenerateCodeCoverageReport`
- `AnalyzeCodeCoverage`

Coverage reports are emitted under `CodeCoverage/<Toolchain>/<Platform-Config>/`
and exclude `Source/Tests/` from the reported source denominator.

## Shared test helpers

Header-only helpers live next to the suites and are not listed in `FO_TESTS_SOURCE`:

`Test_ClientEngineHelpers.h`, `Test_ClientServerIntegrationHelpers.h`, `Test_EntityLifecycleHelpers.h`, `Test_LocationAndEntityMgmtHelpers.h`, `Test_MapperHelpers.h`, `Test_ServerEngineHelpers.h`, `Test_ServerItemsHelpers.h`, and `Test_ServerMapOperationsHelpers.h` share native resource construction with their AngelScript suites. Backend-specific compilation stays guarded inside these fixtures.

- `Source/Tests/Test_BakerHelpers.h` - baked-resource fixtures (sprites, protos, metadata) and a
  `TestRig` that runs the real bakers over in-memory sources.
- `Source/Tests/Test_ParticleFixtures.h` - particle asset fixtures.
- `Source/Tests/Test_ImGuiHarness.h` - presses ImGui widgets by label so the branch behind a button,
  checkbox, selectable or folded section runs in a headless frame. Pinned by
  `ImGuiTestHarnessPressesWidgetsByLabel` in `Test_ImGui.cpp`; usage rules are in
  [../../Docs/Testing.md](../../Docs/Testing.md).
- `Source/Tests/Test_DumpArtifacts.h` - records the `TexDump_*` directories present in the working
  directory so a suite that triggers an atlas dump removes only the ones its own run wrote.

## Notes

- Keep tests deterministic and platform-stable.
- Avoid network, filesystem, and timing-sensitive behavior in unit suites unless mocked or isolated.
- New test sources must be added to `FO_TESTS_SOURCE` in `BuildTools/cmake/stages/EngineSources.cmake`.
- Update [Testing](../../Docs/en/contributing/testing/) and this README when adding, removing, or regrouping test suites.
- Treat `RunUnitTests` as the minimum broad validation baseline for engine-side changes after focused tests pass.
