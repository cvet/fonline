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

## Running code coverage

The repository now has a dedicated `FO_CODE_COVERAGE` workflow for the engine test binary `LF_CodeCoverage`, and the generated report reflects the full `Engine/Source` tree.

- CMake target that executes the instrumented binary: `RunCodeCoverage`
- CMake target that builds the local report from collected data: `GenerateCodeCoverageReport`
- CMake target that performs the full run plus report generation: `AnalyzeCodeCoverage`

Coverage builds use isolated build directories, but they emit binaries into the regular workspace `Binaries/` output layout.

### Coverage presets

- Linux Clang: `clang-coverage` with build preset `clang-coverage-debug`
- Linux GCC: `gcc-coverage` with build preset `gcc-coverage-debug`
- Windows MSVC: `msvc2026-coverage` with build preset `msvc2026-coverage-debug`
- Windows clang-cl: `clang-cl-coverage` with build preset `clang-cl-coverage-debug`

Coverage binaries are emitted under the regular `Binaries/Tests-*` paths.
Coverage reports are emitted under `CodeCoverage/<Toolchain>/`.

### Local workflow

Prefer the dedicated VS Code tasks:

- `Code Coverage :: Configure`
- `Code Coverage :: Build LF CodeCoverage`
- `Code Coverage :: Run CodeCoverage`
- `Code Coverage :: Generate Code Coverage Report`
- `Analyze Engine Coverage`

Alternative tasks exist for Linux GCC and Windows clang-cl when you need to switch backend explicitly.

In FOnline Tools, task labels using the `Group :: Item` convention are collapsed into a submenu named `Group ...`. Keep top-level tasks like `Analyze ...` without the `::` separator when they must stay visible in the root list.

Platform-specific tasks and launch configurations use hidden label tags like `[linux]` and `[windows]`. FOnline Tools uses these tags for filtering and removes them from the displayed names in its picker.

From the command line the usual entry point is:

```bash
cmake --preset clang-coverage
cmake --build --preset clang-coverage-debug --target AnalyzeCodeCoverage
```

### External tools required by backend

- Linux GCC: `lcov`
- Linux Clang: `llvm-profdata` and `llvm-cov`
- Windows MSVC and clang-cl: `Microsoft.CodeCoverage.Console` and `dotnet-coverage`

The internal coverage tool prints a short summary to the console and writes the generated HTML report to:

- `CodeCoverage/<Toolchain>/<Platform-Config>/report/index.html`
- `CodeCoverage/<Toolchain>/<Platform-Config>/report/summary.txt`

### Current filtering rules

The local summary report is currently limited to engine sources under `Engine/Source/` and excludes:

- `Engine/Source/Applications/`
- `Engine/Source/Tests/`
- `ThirdParty/`
- generated sources under `GeneratedSource/`

## Notes

- Keep tests deterministic and platform-stable.
- Avoid network, filesystem, and timing-sensitive behavior in unit suites unless mocked.
- New test sources must be added to `FO_TESTS_SOURCE` in `Engine/BuildTools/FinalizeGeneration.cmake`.
- Treat `LF_UnitTests` run through VS Code CMake Tools as the minimum validation baseline for engine-side changes.
