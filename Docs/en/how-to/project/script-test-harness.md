---
layout: default
title: Script Test Harness Pattern for AngelScript and Managed C#
locale: en
document_id: project-script-test-harness
permalink: /Docs/en/how-to/project/script-test-harness.html
---

# Script Test Harness Pattern for AngelScript and Managed C#

This guide gives a game project a small, copyable harness that runs script-level gameplay tests inside one headless server and reports them through a fixed marker contract. The Engine ships the harness core for both script backends as an example, a settings snippet, a runner manifest and tests that pin the contract; the game project copies the core, writes its own suites and fixtures, and runs them through `BuildTools/gameplay_test_runner.py`. Read [Add and Run Tests in a Game Project](project-tests.md) first for choosing the test boundary and keeping tests out of shipping packs.

## Source paths inspected

- `Examples/GameplayTestHarness/README.md`
- `Examples/GameplayTestHarness/ScriptTests/AngelScript/Testing.fos`
- `Examples/GameplayTestHarness/ScriptTests/AngelScript/Test_HarnessExample.fos`
- `Examples/GameplayTestHarness/ScriptTests/Managed/Testing.cs`
- `Examples/GameplayTestHarness/ScriptTests/Managed/Test_HarnessExample.cs`
- `Examples/GameplayTestHarness/ScriptTests/Managed/Verification/ScriptTestHost.cs`
- `Examples/GameplayTestHarness/ScriptTests/Managed/Verification/ScriptTestHost.csproj`
- `Examples/GameplayTestHarness/ScriptTests/Managed/Verification/Test_HarnessSelfCheck.cs`
- `Examples/GameplayTestHarness/ScriptTests/script-tests.fomain`
- `Examples/GameplayTestHarness/ScriptTests/script-tests.json`
- `Examples/MinimalMultiplayer/CMakeLists.txt`
- `Examples/MinimalMultiplayer/run_tutorial_smoke.py`
- `BuildTools/gameplay_test_runner.py`
- `BuildTools/tests/test_script_test_harness_example.py`
- `Source/Common/Settings.inc`
- `Source/Common/Common.h`
- `Source/Common/Common.cpp`
- `Source/Essentials/BasicCore.cpp`
- `Source/Frontend/Application.cpp`
- `Source/Scripting/CommonGlobalScriptMethods.cpp`
- `Source/Scripting/AngelScript/AngelScriptBackend.cpp`
- `Source/Scripting/AngelScript/AngelScriptGlobals.cpp`
- `Source/Scripting/Managed/CoreScripts/Attributes.cs`
- `Source/Scripting/Managed/CoreScripts/TimePlace.cs`

## What the Engine owns and what the project owns

| Part | Owner | Where |
|---|---|---|
| Process launch, required and forbidden markers, exit code, deadline, report | Engine | `BuildTools/gameplay_test_runner.py`, [Gameplay and Integration Testing](../testing/gameplay-and-integration.md) |
| Marker lines, settings names and harness behavior in this guide | Engine example | `Examples/GameplayTestHarness/ScriptTests` |
| The copied harness file, its namespace and any project extensions | Game project | the project's test script directory |
| Suites, fixtures, content ids, cleanup and leak checks, acceptance thresholds | Game project | the project's suite files |

The `Testing.*` settings are project metadata, not an Engine settings group. `Source/Common/Settings.inc` holds only settings that native code consumes, and no native code reads these. An Engine declaration would also collide with a project that already declares the same names with `///@ Setting`. The harness files therefore carry the declarations, and `script-tests.fomain` carries their values.

The runner needed no extension for this protocol. A marker is a substring of process output, so the harness writes a fixed acceptance token, `status=passed`, instead of a count that a substring check cannot evaluate.

## Marker contract

The harness writes these lines through `Game.Log`, in this order:

```text
[Testing] START registered=<n> selected=<n> filter='<Testing.Filter>'
[Testing] PASS <suite>.<name>
[Testing] FAIL <suite>.<name>: <reason>
[Testing] ERROR <configuration problem>
[Testing] COMPLETE status=<passed|failed> registered=<n> selected=<n> passed=<n> failed=<n> skipped=<n> not_run=<n>
```

- `START` is written once, after every suite has registered. `PASS` or `FAIL` is written once per executed test. `ERROR` is written instead of running tests when registration or configuration is invalid.
- `COMPLETE` is the terminal line. `status=passed` requires no configuration error, at least one selected test and `failed=0`. An empty selection is a failure, so a mistyped filter cannot pass.
- `skipped` counts tests that the filter left out. `not_run` counts selected tests that did not execute because `Testing.StopOnFirstFailure` stopped the run or a configuration error prevented it.
- A `FAIL` reason is the failed expectation text, `timeout`, or `exception: <text>`. The AngelScript harness uses `GetExceptionInfo()` for the text; the managed harness uses the exception type name and message.
- With `Testing.ExitOnFinish = True`, the harness calls `Game.RequestQuit(passed)`. A headless server then exits with `EXIT_SUCCESS` or `EXIT_FAILURE` (`exit_app` in `Source/Essentials/BasicCore.cpp`), that is 0 or 1 on Windows and Linux.

The runner manifest accepts a run only on the terminal line and the exit code:

```json
{
  "schema_version": 1,
  "name": "script-tests",
  "default_timeout_seconds": 300,
  "forbidden_markers": [
    "FATAL ERROR!",
    "ScriptException",
    "[Testing] FAIL",
    "[Testing] ERROR",
    "[Testing] COMPLETE status=failed"
  ],
  "scenarios": [
    {
      "id": "script-tests",
      "processes": [
        {
          "id": "server",
          "command": [
            "{server}",
            "-ApplyConfig",
            "{config}",
            "-ApplySubConfig",
            "ScriptTests"
          ],
          "required_markers": [
            "[Testing] START",
            "[Testing] COMPLETE status=passed"
          ]
        }
      ]
    }
  ]
}
```

A `PASS` count, a running process or a missing `FAIL` line is not acceptance. Keep extra diagnostics under another prefix so they can never match an acceptance or failure marker.

## Settings

| Setting | Type | Master value | Meaning |
|---|---|---|---|
| `Testing.Enabled` | `bool` | `False` | Subscribes the harness to `Game.OnStart`; off in normal play |
| `Testing.Filter` | `string` | empty | Comma-separated tokens; a test runs when `suite.name` contains any token, ignoring case; empty runs all |
| `Testing.StopOnFirstFailure` | `bool` | `False` | Stops after the first `FAIL` and reports the rest as `not_run` |
| `Testing.ExitOnFinish` | `bool` | `True` | Requests quit with the run status after `COMPLETE` |
| `Testing.TestTimeout` | `int32` | `10000` | Per-test watchdog in milliseconds; a registration may override it; zero or less is a configuration error |

Merge `Examples/GameplayTestHarness/ScriptTests/script-tests.fomain` into the master `.fomain`. The master needs a value for every declared setting, and the sub-config switches the harness on:

```ini
Testing.Enabled = False
Testing.Filter =
Testing.StopOnFirstFailure = False
Testing.ExitOnFinish = True
Testing.TestTimeout = 10000
AngelScript.MutableGlobalsAllowedNamespaces = Testing

[SubConfig]
Name = ScriptTests
Testing.Enabled = True
ServerNetwork.DisableNetworking = True
Server.DbStorage = Memory
```

`AngelScript.MutableGlobalsAllowedNamespaces` applies to the AngelScript backend only. The harness keeps its run state in one module global, `Testing::Run`, and `AngelScriptBackend.cpp` rejects mutable globals outside the listed namespace prefixes when it loads the server module. Keep the entry exactly `Testing` and keep test sources out of shipping bakes, so the exception never reaches a shipped module. The managed harness keeps the same state in one static field: it is test-process state that is never persisted, and each backend's isolated load context keeps it per engine. Gameplay state still belongs on `Game`, an entity or a ref type; see [Managed C#](../scripting/managed-csharp.md).

Override a setting on the server command line with `-Testing.Filter <value>`. Always pass a value: `Source/Common/Common.cpp` gives an option that is directly followed by another option the implicit value `1`. The runner rejects an empty `--value`, so keep the unfiltered CI manifest free of a filter argument and use a separate manifest or a direct launch for filtered runs.

## Copy the harness into a project

1. Copy one core file into the project's test script directory, for example `Tests/Scripts`: `ScriptTests/AngelScript/Testing.fos` for AngelScript or `ScriptTests/Managed/Testing.cs` for Managed C#. In the C# file, replace the placeholder `namespace MyGame;` with the project's script namespace.
2. Merge the settings snippet into the master `.fomain` as shown above.
3. List the test directory as an input of the metadata and script packs, and leave it out of the shipping bake with `Baking.IgnoreInputDirs`, as described in [keeping test sources out of shipping packs](project-tests.md#keep-test-sources-out-of-shipping-packs). Managed C# projects also add the directory to `ManagedScript.Dirs`, which scans top-level `.cs` files of each listed root.
4. Write suites next to the core (next section).
5. Compile every side with `CompileAngelScript` or `CompileManagedScripts`, bake, and run the `ScriptTests` sub-config.

Keep the copied core free of game types. Fixtures, content ids and project policy then live in other files, and a later update of the Engine example applies to the core as a clean diff.

## Register suites and keep fixtures in project files

A suite subscribes to `Game.OnTestingInit` in its module initializer and registers tests from the event handler. The harness fires that event once from its `Game.OnStart` handler, so module initialization order does not matter. Registration rules:

- `RegisterTest(suite, name, func, timeoutMs = 0)` is valid only while the event is being fired; a later call throws.
- The full name is `suite.name`. A suite name containing `.`, an empty name, a negative timeout or a duplicate full name is reported as `ERROR` and fails the whole run instead of silently dropping the rest of the suite.
- A test passes when its callback finishes without a failed expectation. `Expect`, `ExpectEq` and `ExpectStrEq` record the first failure and return `false`, so a test can return early. `Pass` and `Fail` set the verdict explicitly; the first verdict wins.
- An exception escaping the callback becomes a `FAIL` with the exception text.

An AngelScript suite, from `Test_HarnessExample.fos`:

```angelscript
namespace Test_HarnessExample
{

#if SERVER

[[ModuleInit]]
void InitTestHarnessExample()
{
    Game.OnTestingInit.Subscribe(OnTestingInit);
}

[[Event]]
void OnTestingInit()
{
    Testing::RegisterTest("harness_example", "split_drops_empty_entries", SplitDropsEmptyEntries);
    Testing::RegisterTest("harness_example", "time_event_completes_a_deferred_test", TimeEventCompletesADeferredTest, 2000);
}

void SplitDropsEmptyEntries(Testing::TestContext ctx)
{
    string[] parts = "alpha,,beta".split(",", true);
    if (!Testing::ExpectEq(ctx, parts.length(), 2, "Split entry count")) {
        return;
    }

    Testing::ExpectStrEq(ctx, parts[1], "beta", "Second entry");
}

void TimeEventCompletesADeferredTest(Testing::TestContext ctx)
{
    Testing::Defer(ctx);
    Game.StartTimeEvent(Testing::Milliseconds(50), FinishDeferredTest, ctx.Id);
}

[[TimeEvent]]
void FinishDeferredTest(any value)
{
    // A null context means the watchdog already failed this test
    Testing::TestContext? ctx = Testing::Resume(int(value));
    if (ctx == null) {
        return;
    }

    Testing::Pass(ctx);
}

#endif

}
```

An AngelScript test that spans several engine ticks calls `Testing::Defer(ctx)`, passes `ctx.Id` through a time-event payload, and resolves the context later with `Testing::Resume(id)`, which returns `null` once the test has completed. Only deferred tests can reach the watchdog. An exception in a later callback is outside the harness's `try`, so it surfaces as a `timeout` together with the engine's exception log.

The same suite in Managed C#, from `Test_HarnessExample.cs`:

```csharp
namespace MyGame;

using System;
using System.Threading.Tasks;
using FOnline;

public static class Test_HarnessExample
{
#if SERVER
    [ModuleInit]
    internal static void InitTestHarnessExample()
    {
        Game.OnTestingInit.Subscribe(OnTestingInit);
    }

    [Event]
    internal static void OnTestingInit()
    {
        Testing.RegisterTest("harness_example", "split_drops_empty_entries", SplitDropsEmptyEntries);
        Testing.RegisterTest("harness_example",
                             "awaited_time_step_completes_the_test",
                             AwaitedTimeStepCompletesTheTest,
                             2000);
    }

    private static void SplitDropsEmptyEntries(Testing.TestContext ctx)
    {
        string[] parts = "alpha,,beta".Split(',', StringSplitOptions.RemoveEmptyEntries);
        if (!Testing.ExpectEq(ctx, parts.Length, 2, "Split entry count")) {
            return;
        }

        Testing.ExpectStrEq(ctx, parts[1], "beta", "Second entry");
    }

    private static async Task AwaitedTimeStepCompletesTheTest(Testing.TestContext ctx)
    {
        await Testing.WaitAsync(ctx, 50);
        Testing.Pass(ctx);
    }
#endif
}
```

A managed test may return `Task`. The harness awaits it from a `[TimeEvent]` handler, and the watchdog fails a task that has not completed within the timeout. `Testing.WaitAsync(ctx, milliseconds)` suspends the test through an Engine time event, and the watchdog cancels a suspended wait.

Both cores run each test the same way. This is the AngelScript step that starts the watchdog, runs the callback and completes a test that did not defer:

```angelscript
int timeoutMs = test.TimeoutMs > 0 ? test.TimeoutMs : Settings.Testing.TestTimeout;
run.WatchdogId = Game.StartTimeEvent(Milliseconds(timeoutMs), WatchTest, ctx.Id);

TestFunc? func = test.Func;

try {
    if (func != null) {
        func(ctx);
    }
}
catch {
    Fail(ctx, "exception: " + GetExceptionInfo());
}

// A callback that returns without a verdict and without Defer has passed
if (!ctx.Finished && !ctx.Deferred) {
    ctx.Finished = true;
    ctx.Ok = true;
}

if (ctx.Finished && !ctx.Completed) {
    CompleteTest(ctx);
}
```

And the managed equivalent:

```csharp
int timeoutMs = test.TimeoutMs > 0 ? test.TimeoutMs : Settings.Testing.TestTimeout;
run.WatchdogId = Game.StartTimeEvent(Milliseconds(timeoutMs), WatchTest, ctx.Id);

try {
    await test.Run(ctx);
}
catch (Exception exception) {
    Fail(ctx, "exception: " + exception.GetType().Name + ": " + exception.Message);
}

// A task that completes without a verdict has passed; a late completion after the watchdog changes nothing
if (!ctx.Finished) {
    ctx.Finished = true;
    ctx.Ok = true;
}

if (!ctx.Completed) {
    CompleteTest(ctx);
}
```

Keep game fixtures in project files beside the suites, for example a `Fixtures.fos` or `TestFixtures.cs` that creates the entities a test needs, records their ids on a project-owned context and destroys them with a leak check before the test reports. Call `Testing::Fail` or `Testing.Fail` from the fixture when cleanup finds a leak. The harness core stays unaware of maps, critters, items or content.

## Run the tests

Launch the headless server directly while developing. Windows (PowerShell):

```powershell
& "<server-headless-binary>" -ApplyConfig "<project>.fomain" -ApplySubConfig ScriptTests -Testing.Filter "<filter>"
```

Linux (bash):

```bash
<server-headless-binary> -ApplyConfig <project>.fomain -ApplySubConfig ScriptTests -Testing.Filter "<filter>"
```

Run the unfiltered set through the runner for acceptance. Windows (PowerShell):

```powershell
py -3 Engine/BuildTools/gameplay_test_runner.py `
  --manifest Tests/script-tests.json `
  --value "server=<server-headless-binary>" `
  --value "config=<project>.fomain" `
  --report Workspace/script-tests-report.json
```

Linux (bash):

```bash
python3 Engine/BuildTools/gameplay_test_runner.py \
  --manifest Tests/script-tests.json \
  --value server=<server-headless-binary> \
  --value config=<project>.fomain \
  --report Workspace/script-tests-report.json
```

| Exit code | Server with `Testing.ExitOnFinish = True` | `gameplay_test_runner.py` |
|---|---|---|
| `0` | `COMPLETE status=passed` | every scenario passed |
| `1` | `COMPLETE status=failed` | a marker, exit code, readiness or timeout contract failed |
| `2` | not produced by the harness | invalid CLI or manifest |

With `Testing.ExitOnFinish = False` the server keeps running after `COMPLETE`; use that only for interactive inspection, because the runner would then stop the process at its deadline.

## Verify the example in the Engine

`BuildTools/tests/test_script_test_harness_example.py` checks that both cores declare the same settings and event, write the same marker fields in the same order, and that the snippet and manifest match them. When the `dotnet` SDK is on `PATH`, it also compiles the managed core, the example suite and negative-control suites against Engine CoreScripts and a stand-in of the generated server API (`ScriptTestHost.cs`), then runs passing, failing, timeout, exception, filter, empty-selection, configuration-error and runner scenarios. Without `dotnet` those cases skip with that reason.

```powershell
py -3 -m pytest BuildTools/tests/test_script_test_harness_example.py -q -p no:cacheprovider
```

```bash
python3 -m pytest BuildTools/tests/test_script_test_harness_example.py -q -p no:cacheprovider
```

The stand-in host proves the harness's control flow and markers, not Engine dispatch. The AngelScript core is not compiled by Engine tests, because a script compile needs a project's `<ProjectDevName>_ASCompiler` build. After copying either core, compile it with the project's own targets and run the `ScriptTests` sub-config once with a deliberately failing test to see `FAIL` and `COMPLETE status=failed`.

## Wire it into a local task and CI

Add a custom target next to the bake, modeled on `RunTutorialChecks` in `Examples/MinimalMultiplayer/CMakeLists.txt`:

```cmake
find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_custom_target(RunScriptTests
    COMMAND ${Python3_EXECUTABLE}
        "${CMAKE_CURRENT_SOURCE_DIR}/Engine/BuildTools/gameplay_test_runner.py"
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/Tests/script-tests.json"
        --value "server=$<TARGET_FILE:${FO_DEV_NAME}_ServerHeadless>"
        --value "config=${CMAKE_CURRENT_SOURCE_DIR}/${FO_MAIN_CONFIG}"
        --report "${FO_OUTPUT_PATH}/script-tests-report.json"
    DEPENDS ${FO_DEV_NAME}_ServerHeadless ForceBakeResources
    SOURCES Tests/script-tests.json
    WORKING_DIRECTORY ${FO_OUTPUT_PATH}
    VERBATIM
    USES_TERMINAL
    COMMENT "Run script tests")
```

Depending on the bake target keeps one writer per bake output; do not start the tests while another bake of the same output runs. In CI, run the target after native unit tests and before process smokes, keep the JSON report and the prefixed log as artifacts, and gate the job on the runner exit code alone. [Set Up Continuous Integration for a Game Project](continuous-integration.md) owns job placement and artifact retention.

## Extend it in a project

- **Sub-config chain.** Put deterministic-world switches in one test sub-config, for example `GameplayTests`, and derive focused sub-configs from it with `Parent = GameplayTests` for feature flags that only some suites need. The manifest then selects the sub-config by name. [Project configuration](../build/project-configuration.md) owns sub-config precedence and `Parent` chaining.
- **Thin wrapper.** A project script may resolve its binary paths, choose a manifest and call `gameplay_test_runner.main(argv)`, as `Examples/MinimalMultiplayer/run_tutorial_smoke.py` does. For a filtered run it writes a temporary copy of the manifest whose command adds `-Testing.Filter` and its value, and it keeps the checked-in manifest unfiltered for CI.
- **More settings.** Parallel instances, sharding, repetition or timeout multipliers are project policy. Declare them as additional `///@ Setting` lines in a project file beside the core, give each a master value, and keep the five contract settings unchanged.
- **More fixtures.** Add fixture helpers and leak checks in project files that call `Fail`. Keep a short comment next to each fixture that documents what it creates and why its cleanup is complete.
- **More markers.** Keep the five line kinds intact. Additional evidence, such as timings or expectation counts, goes on lines with another prefix and never inside `COMPLETE`, so manifests keep matching.

### Migrate an earlier project harness

1. Inventory the old harness's settings, registration calls, marker lines and quit behavior.
2. Copy the Engine core and move project-specific helpers out of the old core into fixture files.
3. Rename settings that differ from the contract, for example a project timeout setting to `Testing.TestTimeout`, in the declarations, the master `.fomain`, every sub-config and every launcher. Settings are configuration, not persisted data, so no save migration is involved.
4. Adapt suites to `RegisterTest(suite, name, func, timeoutMs)`; keep suite and test names stable so existing filters keep working.
5. Replace the old acceptance markers in every manifest, task and CI step with `[Testing] COMPLETE status=passed` and the forbidden markers above.
6. Delete the old core after every side compiles and a deliberately failing test produces `FAIL` plus `COMPLETE status=failed` with exit code 1.

## Failure routing

| Symptom | Check first |
|---|---|
| No `[Testing]` line at all | `Testing.Enabled` in the applied sub-config, and whether the harness file is in the baked script inputs |
| Server start fails on a mutable global | `AngelScript.MutableGlobalsAllowedNamespaces = Testing` in the master `.fomain` |
| A side fails with "Nothing was built in the module" | Every script of that side is inside `#if SERVER`; a project normally has other modules for that side |
| `ERROR duplicate test` or `ERROR invalid registration` | The named registration in the suite files |
| `ERROR no registered test matches the filter` | The `Testing.Filter` value, or a missing suite subscription to `Game.OnTestingInit` |
| `FAIL <test>: timeout` | Whether a deferred or asynchronous test lost its continuation, then the engine log for an exception in a later callback |
| Runner timeout without `COMPLETE` | `Testing.ExitOnFinish`, and the last `PASS` or `FAIL` line before the stop |

## Validation checklist

- The copied core differs from the Engine example only in its namespace and comments.
- Every `Testing.*` setting has a master value, and only a sub-config enables the harness.
- The manifest requires `[Testing] COMPLETE status=passed` and forbids `[Testing] FAIL`, `[Testing] ERROR` and `[Testing] COMPLETE status=failed`.
- A deliberately failing test yields `FAIL`, `COMPLETE status=failed` and exit code 1 before the suite is trusted.
- Test sources, including the core, are absent from shipping packs.
- Game fixtures, content ids and cleanup live in project files, not in the core.
