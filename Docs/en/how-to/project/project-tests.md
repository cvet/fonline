---
layout: default
title: Add and Run Tests in a Game Project
locale: en
document_id: project-tests
permalink: /Docs/en/how-to/project/project-tests.html
---

# Add and Run Tests in a Game Project

This guide is for a game team that wants automated tests beyond "it starts". It shows how to choose the cheapest test boundary that can observe a change, how to add native unit tests through the `TESTS` source role, how to build a script-level test harness driven by a sub-config and project-defined settings, how to run real server and client processes, how to keep test sources out of shipping resource packs, and how to decide that a run passed.

The Engine supplies the native test executable, the process runner and the rules for markers and exit codes. It does not define a game's script test registry as an Engine API, nor its fixtures, accounts or acceptance thresholds; those are project code. For script tests it ships a copyable harness example that a project adopts as its own code; see [Script Test Harness Pattern for AngelScript and Managed C#](script-test-harness.md).

## Source paths inspected

- `BuildTools/gameplay_test_runner.py`
- `BuildTools/cmake/RunTestExecutable.cmake`
- `BuildTools/cmake/helpers/Build.cmake`
- `BuildTools/cmake/stages/Applications.cmake`
- `BuildTools/cmake/stages/EngineSources.cmake`
- `BuildTools/cmake/stages/ScriptsAndBaking.cmake`
- `Source/Applications/TestingApp.cpp`
- `Source/Common/Settings.inc`
- `Source/Common/Settings.cpp`
- `Source/Tests/Test_Settings.cpp`
- `Source/Tools/MetadataBaker.cpp`
- `Source/Tools/ConfigBaker.cpp`
- `Examples/GameplayTestHarness/README.md`
- `Examples/GameplayTestHarness/synthetic-smoke.json`
- `Examples/GameplayTestHarness/ScriptTests/script-tests.json`
- `Examples/MinimalMultiplayer/CMakeLists.txt`
- `Examples/MinimalMultiplayer/FOnlineMinimalMultiplayer.fomain`
- `Examples/MinimalMultiplayer/Scripts/Tutorial.fos`
- `Examples/MinimalMultiplayer/tutorial-smoke.json`
- `Examples/NativeExtensionSample/CMakeLists.txt`
- `Examples/NativeExtensionSample/Tests/NativeExtensionCoreTest.cpp`
- `Docs/en/how-to/testing/gameplay-and-integration.md`
- `Docs/en/how-to/project/script-test-harness.md`
- `Docs/en/contributing/testing/index.md`
- `Docs/en/tutorials/first-test.md`
- `Docs/en/how-to/native-extensions.md`
- `Docs/en/how-to/build/project-configuration.md`
- `Docs/en/how-to/build/generated-content.md`
- `Docs/en/explanation/content-pipeline/baking.md`
- `Docs/en/reference/metadata/index.md`
- `Docs/en/reference/script-api/settings.md`

## Choose the test boundary

Pick the first layer that can observe the contract, and add a broader one only when behavior crosses another boundary. [Gameplay and Integration Testing](../testing/gameplay-and-integration.md) owns the full selection table; the project-facing summary is:

| You changed | Start with | Needs |
|---|---|---|
| A pure native function or data structure in `SourceExt/` | Native unit test (Catch2) through the `TESTS` role | `FO_UNIT_TESTS=ON` |
| A script module, callback, content invariant or prototype | Script-level test in one headless server, selected by a sub-config | A project harness (below) |
| A client-visible result, remote call or replicated state | Real headless server and client processes through the process runner | Baked content, ordered markers |
| Persistence across restart or an old save | Script or process test on an isolated, file-backed database | See [data migrations](data-migrations.md) |
| Packaging, updater or platform behavior | A package lane | See [continuous integration](continuous-integration.md) |

A broad smoke test never replaces a focused failure location, and a unit test cannot prove a process, network or baked-content boundary it never enters.

## Add native unit tests

1. Enable the test targets in the root `CMakeLists.txt` before project generation. `Examples/MinimalProject/CMakeLists.txt` and `Examples/MinimalMultiplayer/CMakeLists.txt` use `SetOption(FO_UNIT_TESTS ON)`.
2. Write Catch2 test cases in a `.cpp` file with the same includes as an Engine test: `Common.h` first, then `catch_amalgamated.hpp`.

   ```cpp
   #include "Common.h"

   #include "catch_amalgamated.hpp"

   FO_BEGIN_NAMESPACE

   TEST_CASE("ProjectCooldownRoundsUp", "[project][cooldown]")
   {
       CHECK(1 + 1 == 2);
   }

   FO_END_NAMESPACE
   ```

3. Register the file with the `TESTS` role, between third-party setup and `RegisterEngineSources()`:

   ```cmake
   AddEngineSources(
       SERVER SourceExt/ServerExtension.cpp
       TESTS SourceExt/Test_ProjectExtension.cpp)
   ```

   The `TESTS` role compiles the file directly into the unit-test executable and coverage executable. It adds nothing to runtime libraries and has no script export target. Roles come in pairs; an odd argument count is a configure-time error.

4. Reconfigure, build and run. The generated names carry the project's development-name prefix:

   ```bash
   cmake --build <build-dir> --config RelWithDebInfo --target RunUnitTests
   ```

   The executable is `<ProjectDevName>_UnitTests` under `Binaries/Tests-*`. Run it directly with a Catch2 tag or name to execute a subset, for example `<ProjectDevName>_UnitTests "[project]"`.

Two properties of the build matter for planning. By the CMake source lists, `FO_TESTS_SOURCE` holds the Engine's own test files as well as your `TESTS` files, so `RunUnitTests` in a game project runs the Engine suite too; give project cases a distinctive tag such as `[project]` and use the direct executable for quick runs. Also, `RunUnitTests` passes only when the executable exits with code 0 and its output contains a Catch2 summary (`All tests passed (...)` or the `test cases:` totals line); an executable that exits 0 without a summary fails (`BuildTools/cmake/RunTestExecutable.cmake`).

A pure library can also be tested without Catch2: `Examples/NativeExtensionSample` builds its core as a static library with `-Werror`, links it into a small executable that returns non-zero on failure and prints `native_extension_core_test_passed`, and wires both into a custom target. Use that shape when the code under test has no Engine dependencies.

## Build a script-level test harness

The Engine does not define a test registry, a discovery attribute or a runner inside the game. A project builds the smallest harness that fits: settings that switch test mode on, an entry event that runs assertions, a marker per verified milestone, and a clean quit. `Examples/MinimalMultiplayer/Scripts/Tutorial.fos` is the reference shape.

For more than a handful of checks, copy the harness core from `Examples/GameplayTestHarness/ScriptTests` instead. It adds suite registration, the filter, per-test timeouts, stop-on-first-failure and a terminal `[Testing] COMPLETE status=passed` line for the process runner, with the same `Testing.*` settings. [Script Test Harness Pattern for AngelScript and Managed C#](script-test-harness.md) documents its marker contract, its settings and how a project extends it.

### Declare the settings

A project setting is declared with a metadata tag and addressed by its full dotted name. The tag is `///@ Setting <Common|Server|Client> <type> <Group.Name>`; the type must be a primitive, enum or string, and the name must contain a group. The master `.fomain` must carry a value for every declared setting or `MetadataBaker` fails the bake ("setting has no configured value"). A `Testing` group is a project choice, not an Engine group:

```angelscript
#if SERVER
///@ Setting Server bool Testing.Enabled
///@ Setting Server string Testing.Filter
#endif
```

```ini
Testing.Enabled = False
Testing.Filter =

[SubConfig]
Name = ScriptTests
Testing.Enabled = True
ServerNetwork.DisableNetworking = True
Server.DbStorage = Memory
```

Managed C# projects declare the same tags in `.cs` files and read them as `Settings.Testing.Enabled`; see [Managed C#](../scripting/managed-csharp.md). Keep deterministic-fixture choices in the sub-config: no networking, in-memory storage, fixed seeds, headless rendering and audio off (the `TutorialSmoke` sub-config in the example shows the client side).

### Run assertions and emit markers

```angelscript
#if SERVER
[[ModuleInit]]
void InitTests()
{
    Game.OnStart.Subscribe(OnTestStart);
}

[[Event]]
void OnTestStart()
{
    if (!Settings.Testing.Enabled) {
        return;
    }

    verify(Game.CheckProtoItem("ExampleItem".hstr()), "Example prototype is missing");

    Game.Log("project_test_content_passed");
    Game.RequestQuit(true);
}
#endif
```

`verify` is an invariant check the example scripts use; see [Nullability](../../contributing/coding-contracts/nullability.md) for who owns its definition. Assertions fail by throwing; the marker is logged only after the asserted state exists, and `Game.RequestQuit(true)` exercises normal teardown.

### Select tests with a filter

Filtering is project policy. A simple scheme is the `Testing.Filter` string above: the entry event reads it and runs only the matching group, and an empty value runs everything. Pass it as an ordinary setting override on the command line, using the same `-Group.Name value` form that the Engine's bake target uses (`-Baking.ForceBaking False`). Keep the filter a runner convenience; CI runs the unfiltered set. The copyable harness implements this scheme with comma-separated, case-insensitive tokens matched against `suite.name`, and it fails a run that selects no test.

### Launch it

```bash
<server-headless-binary> -ApplyConfig <project>.fomain -ApplySubConfig ScriptTests
```

```powershell
& "<server-headless-binary>" -ApplyConfig "<project>.fomain" -ApplySubConfig ScriptTests
```

Sub-config selection, precedence and `Parent` chaining are described in [project configuration](../build/project-configuration.md). The standard `BakeResources` and `ForceBakeResources` targets consume only the master config (`-ApplySubConfig NONE`), so settings that must exist at bake time belong in the master file.

## Run real processes and decide pass or fail

`BuildTools/gameplay_test_runner.py` runs a JSON manifest of ordered processes without a shell. Per process it checks `required_markers`, `forbidden_markers` and `expected_exit_code`, enforces one deadline per scenario and terminates every launched process on failure. The manifest schema, readiness semantics and cleanup order are in [Gameplay and Integration Testing](../testing/gameplay-and-integration.md).

```bash
python3 Engine/BuildTools/gameplay_test_runner.py \
  --manifest Tests/gameplay-smoke.json \
  --value server=<server-headless-binary> \
  --value config=<project>.fomain \
  --report Workspace/gameplay-smoke-report.json
```

```powershell
py -3 Engine/BuildTools/gameplay_test_runner.py `
  --manifest Tests/gameplay-smoke.json `
  --value "server=<server-headless-binary>" `
  --value "config=<project>.fomain" `
  --report Workspace/gameplay-smoke-report.json
```

Exit code `0` means every scenario passed, `1` means a process, marker, readiness or timeout contract failed, and `2` means invalid CLI or manifest input. `Examples/MinimalMultiplayer/tutorial-smoke.json` shows a content-test scenario followed by a server and client scenario.

Accept a run by positive evidence only:

- the runner (or the process) exit code is the expected value;
- every required marker appeared, in the output of the process that owns it;
- no forbidden marker appeared;
- for `RunUnitTests`, the Catch2 summary is present.

"No errors in the log" is not acceptance: a process that crashes before its first assertion also produces no error line. Treat a partial count, a running background job or a started command as not passed. Do not raise a deadline to hide a missing marker; find which process stopped producing markers first.

## Keep test sources out of shipping packs

Put test scripts in a directory of their own, list it as an input directory of each pack that reads it, and leave it out of the shipping bake with `Baking.IgnoreInputDirs`. The setting is `string[] Baking.IgnoreInputDirs`: "Resource pack input directories left out of every pack, relative to the config declaring the pack; each must be an input of some pack". A name that is not an input of any pack makes applying the settings fail with a `SettingsException`, so a misspelled entry cannot silently ship tests.

```ini
[SubConfig]
Name = Shipping
Baking.IgnoreInputDirs = Tests/Scripts

[ResourcePack]
Name = Metadata
InputDirs = Scripts Tests/Scripts
IncludePatterns = *
Bakers = Metadata
```

Repeat the test directory in every pack that reads it, including the metadata pack, so the test declarations leave the metadata too; `Source/Tests/Test_Settings.cpp` pins this behavior (`IgnoreInputDirsFromASubConfigLeaveTheDirectoryOutOfEveryPack`). Create a bake target that applies the sub-config:

```cmake
SetupScriptsAndBaking()

AddBakingTarget(Game_ShippingResources
    SUB_CONFIG Shipping
    FORCE
    COMMENT "Bake shipping resources without tests")
```

Notes:

- `AddBakingTarget` is documented in [Baking](../../explanation/content-pipeline/baking.md#cmake-entry-points). The target uses the same output directory as `BakeResources`. Changing pack membership is a case for a forced bake, so use `FORCE` and never run a development bake and a shipping bake at the same time.
- A pack-level `ExcludePatterns` entry is the static alternative when tests must never be baked into any variant.
- Because the metadata version covers every declared setting, shipping and development bakes have different metadata. That is expected; the invariant is that one server and its clients come from the same bake.
- The config baker logs `Unknown setting` for a configured value whose declaration was excluded (`Source/Tools/ConfigBaker.cpp`). Decide whether your pipeline treats that line as a failure; this behavior was read from source, not observed in a bake.
- Verify the result: assert in a post-bake check that no test resource appears in the shipping packs, and run the packaged smoke test from the shipping bake.

## Make tests deterministic and isolated

- Start each test from a fresh working directory, ports, accounts and output path. Never point a test at production storage, credentials or services.
- Use `Server.DbStorage = Memory` for logic tests. A test that claims persistence needs a file-backed JSON or SQLite directory (`JSON <directory>`, `DbSQLite <directory>`), a stop, and a restart on the same directory. See [Backup and Recovery](../release/backup-and-recovery.md) for the storage forms.
- Seed or remove randomness, and bound every wait. A fixed sleep may pace a fixture but is never the pass condition.
- Create only the entities the assertion needs and clean up what the test created.
- Verify that a new assertion can fail: temporarily break its input, observe the failure, then restore it. The [first test tutorial](../../tutorials/first-test.md) uses this fail-then-pass replay.
- Keep one writer per bake output. `MasterBaker` holds an exclusive output lock, and the example makes both bake-dependent checks depend on `ForceBakeResources` so no two bakers race over `Baking/`. Do not run gameplay tests against an output while a bake of that output is running, and do not delete a lock file to get around a refusal.

## Gate CI on the tests

Run the layers in dependency order and stop at the first failing layer: native unit tests, then script content tests, then the headless process smoke, then package checks. Retain the runner report together with the process log. A CI job passes only on the terminal evidence above, not on the absence of a failure message. Placement, caching and artifact retention are in [Set Up Continuous Integration for a Game Project](continuous-integration.md); where tests sit in the local aggregate route is in [Keep a Game Project Healthy](quality-gates.md).

## Validation checklist

- Native cases carry a project tag and run through the direct executable and `RunUnitTests`.
- Test settings are declared with `///@ Setting`, have values in the master `.fomain`, and are switched on only by a sub-config.
- Each scenario names its required markers, forbidden markers, deadline and expected exit code.
- A script-test scenario requires the harness's terminal `[Testing] COMPLETE status=passed` line, not a count of passing lines.
- A shipping bake target applies `Baking.IgnoreInputDirs` and a check proves tests are absent from its packs.
- Persistence claims are tested on a restart of a file-backed database, not on `Memory`.
- No test runs while a bake of the same output is in progress.
