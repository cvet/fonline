# Gameplay Test Harness Fixture

This Engine-owned fixture exercises `BuildTools/gameplay_test_runner.py` without a compiled game. It proves ordered process launch, server readiness, concurrent output capture, required and forbidden markers, exit codes, timeout cleanup, and JSON reporting. It does not claim gameplay correctness; [../MinimalMultiplayer](../MinimalMultiplayer) supplies the real baked server/client proof.

Run the positive fixture from the Engine root:

```bash
python BuildTools/gameplay_test_runner.py \
  --manifest Examples/GameplayTestHarness/synthetic-smoke.json \
  --value python=python \
  --value fixture=Examples/GameplayTestHarness/fixture_process.py \
  --report Workspace/gameplay-test-harness-report.json
```

A pass requires both processes to exit with code 0, every required marker to appear, no forbidden marker to appear, and the scenario to finish before its deadline. The runner returns 0 for a passing suite, 1 for a process/marker/timeout failure, and 2 for invalid CLI or manifest input.

The fixture process is intentionally not a mock FOnline runtime. Its only job is to make runner failures deterministic and fast. Use the Minimal Multiplayer scenario for actual engine startup, content, networking, remote-call, map-load, and interaction evidence.

## Script test harness

`ScriptTests/` holds a copyable server-side harness core for each script backend: `AngelScript/Testing.fos` and `Managed/Testing.cs`, an example suite for each, the settings snippet `script-tests.fomain` and the runner manifest `script-tests.json`. A game project copies one core into its own test scripts, writes its suites and fixtures beside it, and accepts a run only on `[Testing] COMPLETE status=passed` plus exit code 0. [Script Test Harness Pattern](../../Docs/en/how-to/project/script-test-harness.md) owns the marker and settings contract.

The Engine has no managed game project, so `Managed/Verification/` compiles the managed core with Engine CoreScripts and a stand-in of the generated server API, then runs it on a virtual clock. The stand-in proves the harness's control flow and markers, not Engine dispatch. The AngelScript core needs a project's script compiler and is compiled in the project that copies it. Run the checks from the Engine root:

```bash
python -m pytest BuildTools/tests/test_script_test_harness_example.py -q -p no:cacheprovider
```
