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

