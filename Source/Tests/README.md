# Unit Tests

This directory contains unit tests for deterministic engine/common functionality.

## Framework and target

- Framework: Catch2 (`catch_amalgamated.hpp`)
- Test executable target: `LF_UnitTests`
- Run target: `RunUnitTests`
- Test entrypoint: `Engine/Source/Applications/TestingApp.cpp`

## Current test suites

- `Test_AnyData.cpp` — serialization/parsing and container value behavior
- `Test_CommonHelpers.cpp` — helper utilities and container helpers
- `Test_Compressor.cpp` — compression/decompression roundtrips and invalid input handling
- `Test_ConfigFile.cpp` — config parser string-view storage, hook rewrites, collected content, and parse benchmark
- `Test_Containers.cpp` — container helpers, concepts, formatter/hash checks
- `Test_DataSerialization.cpp` — binary reader/writer and pointer/bounds behavior
- `Test_EngineMetadata.cpp` — migration rule registration, chain resolution, and cycle rejection
- `Test_RemoteCallValidation.cpp` — inbound remote-call payload validation for UTF-8 strings, enum values, finite floats, hashes, and nested collections
- `Test_Rendering.cpp` — null renderer texture/storage validation and effect/draw-buffer smoke checks
- `Test_ExtendedTypes.cpp` — value types (`ipos`, `isize`, `irect`, float variants)
- `Test_GenericUtils.cpp` — hashing, random baseline checks, lerp/float helpers
- `Test_Geometry.cpp` — distance/direction/angle and traversal helpers
- `Test_HashedString.cpp` — hash resolve behavior and failure paths
- `Test_SafeArithmetics.cpp` — casting, clamping, arithmetic safety helpers
- `Test_StringUtils.cpp` — string conversions/parsing and text helpers
- `Test_StrongType.cpp` — strong type operators, formatting, streaming, hashing
- `Test_TimeRelated.cpp` — timespan/timepoint conversions and formatting

## Running tests

Prefer running unit tests from VS Code CMake Tools:

- build or run the `RunUnitTests` target for the standard workflow;
- use `LF_UnitTests` directly when you need the executable target without the wrapper target.

If you need the binary location for debugger or launch configuration wiring, the executable is emitted under `Binaries/Tests-*` for the active platform, for example:

- Windows: `Binaries/Tests-Windows-win64/LF_UnitTests.exe`
- Linux: `Binaries/Tests-Linux-x64/LF_UnitTests`

From a configured build directory the equivalent shell command is:

```bash
cmake --build . --config RelWithDebInfo --target RunUnitTests
```

## Notes

- Keep tests deterministic and platform-stable.
- Avoid network, filesystem, and timing-sensitive behavior in unit suites unless mocked.
- New test sources must be added to `FO_TESTS_SOURCE` in `Engine/BuildTools/FinalizeGeneration.cmake`.
- Treat `LF_UnitTests` run through VS Code CMake Tools as the minimum validation baseline for engine-side changes.
