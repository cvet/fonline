---
layout: default
title: Gameplay and Integration Testing
locale: en
document_id: gameplay-testing
permalink: /Docs/en/how-to/testing/gameplay-and-integration.html
---

# Gameplay and Integration Testing

> Engine-owned documentation. This guide defines reusable test selection, deterministic fixture, process-runner, marker, deadline, cleanup, and evidence contracts for games that embed FOnline.

## Purpose

Use this guide when a change must be proved beyond one native function but does not yet require a packaged release or device laboratory. The Engine supplies a project-neutral process runner and two kinds of proof:

- [Gameplay Test Harness Fixture](../../../../Examples/GameplayTestHarness/README.md) proves the runner itself without compiled game binaries;
- [Minimal Multiplayer](../../../../Examples/MinimalMultiplayer/README.md) proves the same runner against baked content, metadata, a headless server, a headless client, networking, map load, remote calls, replicated state, and an interaction.

For the native Catch2 executable, generated targets, sanitizers, and coverage inventory, use [Testing](../../contributing/testing/). For a first end-to-end lesson, use [First Automated Test](../../tutorials/first-test.md).

## Test decision

Use boundary-based test selection and run narrow-first so the first failing
layer gives a focused failure location. For a process smoke, declare a
`ready_marker`, ordered `required_markers`, `forbidden_markers`, and one common
deadline; always retain command lines, logs, exit reasons, and cleanup results.
The Engine owns this runner contract. The embedding project owns its script test
registration API, fixture catalog, gameplay assertions, acceptance thresholds,
accounts, databases, scenes, and release lanes.

## Select the narrowest owning boundary

Choose the first layer that can observe the contract being changed. Add broader evidence only when the behavior crosses another boundary.

| Change boundary | First proof | Add when |
|---|---|---|
| Pure native algorithm or data structure | focused Catch2 case | integration changes construction, serialization, threads, or runtime roles |
| Parser, baker, or generated metadata | focused native test plus bake | a runtime consumes the generated result |
| One script module or callback | project-owned script test in the affected side | state crosses module, entity, persistence, or client/server boundaries |
| Authored prototype, map, text, or resource | bake plus semantic content assertion | a client must load, render, hear, or interact with it |
| Server lifecycle or world setup | one headless server process | networking or a client-observable result is part of the contract |
| Client/server protocol or gameplay interaction | ordered headless server/client smoke | presentation, package, device, or backend behavior is claimed |
| Package, updater, platform, or release behavior | package/platform acceptance lane | production infrastructure or recovery policy is claimed |

This is boundary-based test selection, not a fixed pyramid. A broad smoke cannot replace a focused failure location, and a unit test cannot prove a process, network, or baked-content boundary it never enters.

Run narrow-first: prove the smallest owner, then add each crossed layer in dependency order. Stop on the first failure and diagnose that layer before increasing the scope.

## Deterministic fixture contract

A gameplay fixture should make setup and completion explicit:

1. Start from an isolated working directory, storage namespace, account set, ports, and output path.
2. Select a named config or sub-config whose behavior is checked in with the fixture.
3. Create or load only the world, entities, and authored references required by the assertion.
4. Seed controllable randomness, or remove randomness from the test route.
5. Emit low-volume semantic markers at readiness and asserted outcomes.
6. Request normal shutdown after success so teardown is exercised.
7. Bound every wait and retain enough process output plus a structured result to diagnose failure.

Do not treat a fixed sleep as success evidence. A delay may pace a deterministic fixture, but the pass condition must be an exit code, semantic marker, decoded artifact, state query, or visible result owned by the changed boundary.

The test owns cleanup for every process and temporary resource it creates. Use unique ports or serialized CI jobs when a fixed runtime port is unavoidable. Never point a smoke test at production storage, credentials, accounts, or services.

## Process runner contract

`BuildTools/gameplay_test_runner.py` executes a checked JSON manifest without a shell. Commands are arrays, so arguments retain their boundaries. Runtime paths and other environment-specific values use `{name}` placeholders supplied through repeated `--value name=value` options.

Schema version 1 has this ownership:

- root: suite `name`, `default_timeout_seconds`, optional `forbidden_markers`, and ordered `scenarios`;
- scenario: stable `id`, optional timeout/forbidden-marker overrides, and ordered `processes`;
- process: stable `id`, command array, optional working directory/environment, readiness contract, required/forbidden markers, and expected exit code.

Unknown fields, duplicate ids or markers, invalid types, and unresolved placeholders are configuration errors. Environment values are added to the inherited process environment. Do not put secrets in manifests, placeholder values, commands, markers, or reports: command lines and process environments can be observable outside the runner.

A minimal ordered server/client scenario looks like this:

```json
{
  "schema_version": 1,
  "name": "project-smoke",
  "default_timeout_seconds": 60,
  "forbidden_markers": ["FATAL ERROR!", "ScriptException"],
  "scenarios": [
    {
      "id": "server-client",
      "processes": [
        {
          "id": "server",
          "command": ["{server}", "-ApplyConfig", "{config}"],
          "ready_marker": "project_server_ready",
          "ready_timeout_seconds": 20,
          "required_markers": ["project_server_ready", "project_server_passed"]
        },
        {
          "id": "client",
          "command": ["{client}", "-ApplyConfig", "{config}"],
          "required_markers": ["project_client_connected", "project_client_passed"]
        }
      ]
    }
  ]
}
```

Run it from the embedding-project root:

```bash
python Engine/BuildTools/gameplay_test_runner.py \
  --manifest Tests/gameplay-smoke.json \
  --value server=Build/bin/Game_ServerHeadless \
  --value client=Build/bin/Game_ClientHeadless \
  --value config=Game.fomain \
  --report Workspace/gameplay-smoke-report.json
```

Paths and target names are examples; each project owns its generated executable names and layout.

## Readiness and marker semantics

Processes launch in manifest order. When a process declares `ready_marker`, the runner waits for that marker before launching the next process. This is the server-readiness gate for a client or dependent worker. `ready_timeout_seconds` is capped by the scenario's common deadline.

After launch, the runner merges each process's standard error into standard output, decodes it as UTF-8 with replacement for invalid bytes, prefixes each displayed line with suite/scenario/process identity, and evaluates:

- `required_markers`: every marker must appear in that process's output;
- `forbidden_markers`: no process may emit its process, scenario, or suite forbidden markers;
- `expected_exit_code`: defaults to 0 and must match exactly.

Markers are test protocol tokens, not prose. Make them stable, unique, low-volume, and emitted only after the asserted state exists. Include data only when it is part of the assertion, for example `project_supply_collected=1`. Keep noisy diagnostics in normal logs and list broad catastrophic signatures as forbidden markers.

A marker can prove that instrumented code reached a state; it cannot by itself prove unobserved pixels, sound, durability, package contents, or external service behavior. Add the owning artifact decoder, state check, screenshot, audible review, restart, or platform lane for those claims.

## Deadlines and cleanup

Each scenario has one common monotonic deadline. Readiness and process completion consume the same budget, preventing a two-process test from silently receiving the full timeout twice. On failure, timeout, or incomplete startup, the runner visits launched processes in reverse order, requests termination, waits up to five seconds, then kills a process that did not stop.

Timeout is a test failure, not permission to increase the limit immediately. First determine whether readiness was never reached, a dependency was unavailable, shutdown deadlocked, or the fixture depended on timing. Increase a deadline only when measured valid work on supported CI hosts requires it.

## Result contract

The command returns:

- `0`: every scenario passed;
- `1`: a process, exit-code, marker, readiness, or timeout contract failed;
- `2`: CLI or manifest configuration is invalid.

With `--report`, the runner writes JSON schema version 1. It records suite/scenario status and duration, timeout state, reasons, process exit codes, and missing/forbidden markers. It deliberately excludes commands, environments, and full logs. CI should retain the report and its normal process log together: the report supports automation, while the prefixed log supplies diagnostic context.

## CMake and CI integration

Wire a gameplay smoke as a named custom target that depends on the exact binaries and baked artifacts it consumes. Pass target paths with generator expressions, keep the manifest in `SOURCES`, use `VERBATIM`, and use a terminal when interleaved process output is useful. The [Minimal Multiplayer CMake file](../../../../Examples/MinimalMultiplayer/CMakeLists.txt) is the current executable pattern.

CI should run the synthetic runner tests on every change to the runner or schema. Run at least one real headless server/client example on each platform claimed by the example. Product CI then adds its own script tests, content assertions, package lanes, persistence backends, visible checks, and release gates according to the boundaries it owns.

## Failure routing

| Symptom | Inspect first |
|---|---|
| Configuration exit 2 | manifest schema, unknown fields, placeholder spelling, and `KEY=VALUE` arguments |
| Process failed to start | resolved executable/working-directory path and host permissions |
| Readiness marker missing | earliest process log, selected config/sub-config, startup dependencies, and marker ownership |
| Required marker missing | the narrow assertion before it, then remote-call/entity/content state at the crossed boundary |
| Forbidden marker found | first occurrence and its native/script source; do not whitelist a real failure signature |
| Exit code mismatch after all markers | normal shutdown, teardown callbacks, worker/database drain, and runtime host result |
| Timeout | last semantic marker from every process, then deadlock, unavailable dependency, and cleanup behavior |
| Local pass but CI failure | ports, path case, locale/encoding, host capacity, graphics/audio assumptions, and undeclared state |

## Project-owned boundary

The Engine does not define a game's script test registration API, fixture catalog, gameplay assertions, account/database policy, test filters, authored ids, ports, timing budgets, package matrix, device lab, or acceptance thresholds. Keep those with the embedding project and link here for runner semantics.

Likewise, project-specific orchestration frameworks are evidence inputs, not Engine APIs. Promote only a reusable primitive after it has Engine-owned implementation, deterministic tests, an independent guide, and a real public example.

An AiControl or MCP adapter can supply semantic observations and actions inside a
project smoke, but it does not replace the process runner's deadline, cleanup,
marker, and report contract. Keep endpoint schemas and gameplay tools in the
project; use [AiControl Protocol](../ai-control-protocol.md) only for the common
wire envelope and command lifecycle. Never count the protocol-only Python sample
as proof that a native FOnline client drained a command or that the game server
enforced normal authority.

## Source paths inspected

- `BuildTools/gameplay_test_runner.py`
- `BuildTools/tests/test_gameplay_test_runner.py`
- `Examples/GameplayTestHarness/fixture_process.py`
- `Examples/GameplayTestHarness/synthetic-smoke.json`
- `Examples/MinimalMultiplayer/tutorial-smoke.json`
- `Examples/MinimalMultiplayer/run_tutorial_smoke.py`
- `Examples/MinimalMultiplayer/CMakeLists.txt`
- `Examples/MinimalMultiplayer/Scripts/Tutorial.fos`
- `Source/Applications/TestingApp.cpp`
- `BuildTools/cmake/stages/Applications.cmake`
- `.github/workflows/validate.yml`
