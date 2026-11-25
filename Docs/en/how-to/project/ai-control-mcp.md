---
layout: default
title: Drive a Game with the AiControl MCP Adapter
locale: en
document_id: project-ai-control-mcp
permalink: /Docs/en/how-to/project/ai-control-mcp.html
---

# Drive a Game with the AiControl MCP Adapter

This guide shows how a game project lets an AI agent observe and play a development build through the Engine's MCP adapter in `BuildTools/ai_control_mcp`. The adapter speaks the Model Context Protocol over stdio to the agent's MCP client and the [AiControl protocol](../ai-control-protocol.md) over TCP to the project's bridge. It covers the commands, exit codes, the generic tools, the security rules, the launch profile that starts owned processes, local and CI wiring, and the registration extension point through which a project adds its own game tools.

The adapter is a development and QA tool. It is never part of a shipped game: do not package it, do not start it from a production client or server, and keep the bridge it talks to compiled out of release builds as the protocol's security rules require.

## Source paths inspected

- `BuildTools/ai_control_mcp/__main__.py`
- `BuildTools/ai_control_mcp/cli.py`
- `BuildTools/ai_control_mcp/stdio.py`
- `BuildTools/ai_control_mcp/server.py`
- `BuildTools/ai_control_mcp/registry.py`
- `BuildTools/ai_control_mcp/context.py`
- `BuildTools/ai_control_mcp/schemas.py`
- `BuildTools/ai_control_mcp/core_tools.py`
- `BuildTools/ai_control_mcp/session.py`
- `BuildTools/ai_control_mcp/launch.py`
- `BuildTools/ai_control_mcp/process_owner.py`
- `BuildTools/ai_control_mcp/runner.py`
- `BuildTools/ai_control_mcp/advisory.py`
- `BuildTools/ai_control_client.py`
- `BuildTools/AiControlProtocol.json`
- `BuildTools/HelperCliInterface.json`
- `BuildTools/foconfig.py`
- `BuildTools/tests/test_ai_control_mcp.py`
- `BuildTools/tests/test_ai_control_protocol.py`
- `BuildTools/cmake/helpers/Build.cmake`
- `BuildTools/cmake/stages/Applications.cmake`
- `Source/Common/Settings.cpp`
- `Source/Common/Settings.inc`
- `Source/Frontend/ApplicationInit.cpp`
- `Examples/AiControlSample/ai_control_sample.py`
- `Examples/AiControlSample/README.md`
- `Docs/en/how-to/ai-control-protocol.md`
- `Docs/en/how-to/build/project-configuration.md`
- `Docs/en/how-to/release/security-and-secrets.md`

## What it does

| Layer | Owner | Content |
|---|---|---|
| MCP stdio | Engine | One UTF-8 JSON-RPC 2.0 message per line; `initialize`, `ping`, `tools/*`, `resources/*`, `prompts/*`; batches, malformed frames and oversized lines are rejected without ending the session |
| AiControl transport | Engine | `BuildTools/ai_control_client.py`; one authenticated connection per request, strict envelope and id checks, 1 MiB line limit |
| Session | Engine | Endpoint registry, selected endpoint, one event cursor per endpoint, fresh command completion, fresh observations, readiness polling |
| Processes | Engine | Launch profile, binary resolution, `-ApplyConfig`/`-ApplySubConfig`/`--Group.Name value` command lines, independent process owners, log tails |
| Game tools | Project | Typed actions, observation projections, readiness gate, prompts, guides, recipes, agent memory and policy |

Run it when an agent or a scripted playtest must drive a real client: reproducing a bug report through the ordinary player path, a smoke run after a gameplay change, or an exploratory session. For deterministic tests that need no agent, prefer the [project test routes](project-tests.md).

## Run it

From the root of a game project that embeds the Engine at `Engine/`, check the configuration first: `--list-tools` loads the project module and the launch profile, prints every tool and exits.

```powershell
$env:FONLINE_AI_TOKEN = "<development token>"
py -3 Engine\BuildTools\ai_control_mcp --project-root . --project-module Tools/AiControl/mygame_mcp.py --launch-profile Tools/AiControl/launch-profile.json --tool-prefix mygame_ --list-tools
```

```bash
export FONLINE_AI_TOKEN='<development token>'
python3 Engine/BuildTools/ai_control_mcp --project-root . --project-module Tools/AiControl/mygame_mcp.py --launch-profile Tools/AiControl/launch-profile.json --tool-prefix mygame_ --list-tools
```

Without `--list-tools` the same command serves MCP on stdin/stdout until the client closes stdin; diagnostics go to stderr. An MCP client starts it as a stdio server. The exact configuration file belongs to the client; a typical entry has this shape:

```json
{
  "mcpServers": {
    "mygame": {
      "command": "python3",
      "args": [
        "Engine/BuildTools/ai_control_mcp",
        "--project-root", ".",
        "--project-module", "Tools/AiControl/mygame_mcp.py",
        "--launch-profile", "Tools/AiControl/launch-profile.json",
        "--tool-prefix", "mygame_"
      ]
    }
  }
}
```

Set the token variable in the environment the client inherits, never in this file. To try the adapter without a game, start the protocol sample from [Examples/AiControlSample](../../../../Examples/AiControlSample/README.md) in another terminal with `py -3 Engine\Examples\AiControlSample\ai_control_sample.py` (it listens on the default port `43011`) and call `mygame_ping`, `mygame_observe` and `mygame_act` against it.

### Exit codes

| Code | Meaning |
|---|---|
| `0` | Standard input ended or a termination signal arrived; owned processes were stopped unless `--keep-processes-on-exit` was given. `--list-tools` completed |
| `2` | Invalid invocation or configuration: unknown option, missing project root, project module not found or without `register(registry)`, duplicate tool name, invalid launch profile, non-loopback host without `--allow-remote` or without a token |

A failing tool call never ends the process. Invalid tool arguments return JSON-RPC error `-32602`; a bridge failure (refused connection, timeout, malformed peer, `queue_full`, unauthorized) returns a tool result with `isError: true` and an `error` object; an unexpected handler exception returns `-32603`.

## Options

| Option | Purpose |
|---|---|
| `--host`, `--port` | Default endpoint, also the base port for launched bridges; default `127.0.0.1:43011` |
| `--timeout` | Socket timeout per bridge request in seconds; default `5.0` |
| `--token-env NAME` | Environment variable holding the shared token; default `FONLINE_AI_TOKEN`. There is no option that takes a raw token |
| `--allow-remote` | Permit non-loopback endpoints; they additionally need a non-empty token |
| `--project-root DIR` | Root for launch profile paths, binaries and logs; default the current directory |
| `--project-module SPEC` | Dotted module name or `.py` file whose `register(registry)` adds project tools; repeatable |
| `--module-path DIR` | Directory added to the import path for dotted project modules; repeatable |
| `--launch-profile FILE` | JSON launch profile; overrides a profile set by a project module |
| `--tool-prefix TEXT` | Prefix of the core tool names; default `fo_` |
| `--server-name TEXT` | `serverInfo.name` reported to the client; default `fonline-ai-control` |
| `--keep-processes-on-exit` | Leave launched processes running after the adapter exits |
| `--max-message-bytes N` | Largest accepted MCP line; default 4 MiB, minimum 1024 |
| `--list-tools` | Print tool names and descriptions as JSON and exit |

The generated [helper CLI reference](../../reference/helper-cli/commands.md) records the exact help text of the current revision.

## Core tools

The core registers only tools that follow from the [protocol methods](../../reference/ai-control-protocol/methods.md) plus endpoint, readiness and process control. The protocol has no screenshot method, so screenshots are project tools. Names below omit the prefix.

| Tool | Purpose |
|---|---|
| `status`, `ping`, `observe` | The protocol methods; `observe` with `fresh: true` waits up to `freshTimeoutMs` for a strictly newer `observationSeq` and reports `observationFresh` |
| `events` | Events after an explicit `afterSeq`; does not move the cursor |
| `next_events` | Events after the endpoint's cursor and advance it; `reset` restarts from `0`, `seekLatest` jumps to `latestSeq` |
| `act` | Queue `command` (an object with a non-empty `type`); with `waitForCompletion` or `syncAfterCompletion` waits for its completion |
| `wait_command` | Wait for `command_completed` of a `commandSeq` recorded after `afterSeq` |
| `connect` | Select a registered endpoint, a managed process endpoint, or a direct `host`/`port`, then verify it with `ping` |
| `endpoints` | Registered endpoints, the selection and cursors; `probe: true` pings each one |
| `wait_ready`, `wait_all_ready` | Poll `observe` until the readiness gate passes, the owning process exits, or the timeout elapses |
| `launch_options` | Roles, binaries, bridge wiring and the `[SubConfig]` sections of the configured `.fomain` |
| `launch`, `processes`, `stop_process` | Start a profile role under an independent owner, list owned processes, stop one tree or all trees |
| `logs` | Tail of an owned process log (default `<binary stem>.log` in its working directory) or a named log in the project root |

Endpoint-targeting tools accept `endpointId`, or `processId` with optional `endpointIndex`; without them they use the selected endpoint.

### Command lifecycle and freshness

- Acceptance is not completion. `act` returns `accepted` and `commandSeq`; when waiting, it adds `completion` with `completed`, `timedOut`, the `event`, the consumed `events` and the cursor.
- Before sending a waited command, the adapter reads the bridge's `latestSeq`. Only a `command_completed` event with a higher sequence counts, so an older completion can never satisfy a new wait.
- `syncAfterCompletion` returns `sync.observation` from a snapshot published after the completion was seen, with `observationFresh: false` when none arrived within `freshTimeoutMs`.
- When `latestSeq` falls below the cursor, the bridge restarted: the cursor resets to `0` and the result carries `historyReset: true`. A waited command then reports `completed: false` instead of waiting for an event that cannot arrive.
- Event history is bounded on the bridge. A slow reader can miss events; resynchronize from `observe`.

## Security rules

The adapter keeps the [protocol security boundary](../ai-control-protocol.md#security-boundary):

- Loopback by default. A non-loopback endpoint, whether from `--host`, `connect` or a launch `bridgeHost`, needs `--allow-remote` and a non-empty token. The reference client enforces the same rule.
- The token comes only from the variable named by `--token-env`. Tool arguments cannot carry a token, and no tool result, endpoint list or launch command shows it.
- A launched process receives the token through its environment: the command line carries `--<tokenSetting> $TARGET_ENV{<token variable>}`, which the Engine resolves when the application reads its settings (see [Project Configuration](../build/project-configuration.md)). Launch `settings` cannot override the token setting.
- `binaryPath`, `workingDirectory` and log names must stay inside the project root or the owned process's working directory.
- The adapter stops only process trees it started. It never stops a process by image name, never touches a process that a developer or another adapter started, and with `--keep-processes-on-exit` it leaves its trees to the operator.

## Launch profile

`launch` needs a profile, either from `--launch-profile` or set by the project module. A complete example for a project whose `FO_DEV_NAME` is `MyGame` and whose client bridge reads `AiControl.*` settings:

```json
{
  "schemaVersion": 1,
  "devName": "MyGame",
  "config": "MyGame.fomain",
  "defaultRole": "server_headless",
  "defaultSubConfigs": [],
  "binarySearchDirs": ["Binaries", "Build"],
  "binaryEnvTemplate": "MYGAME_AI_{ROLE}_EXE",
  "nonInteractive": true,
  "bridge": {
    "enabledSetting": "AiControl.Enabled",
    "hostSetting": "AiControl.Host",
    "portSetting": "AiControl.Port",
    "tokenSetting": "AiControl.Token",
    "portStrideSetting": "AiControl.PortStride"
  },
  "roles": {
    "server_headless": {"target": "ServerHeadless", "endpoints": "embedded-clients", "defaultClientCount": 1},
    "client": {"target": "Client", "endpoints": "client"},
    "client_headless": {"target": "ClientHeadless", "endpoints": "client"}
  }
}
```

| Key | Meaning |
|---|---|
| `schemaVersion` | Must be `1` |
| `devName` | The project's `FO_DEV_NAME`; a role `target` becomes the binary `<devName>_<target>` (`Server`, `ServerHeadless`, `Client`, `ClientHeadless`, `Mapper`) |
| `config` | Project-relative `.fomain` passed as `-ApplyConfig`; omit it to let the application find its config |
| `roles` | Role name to `binary` (a file name) or `target`, `endpoints` (`none`, `client`, `embedded-clients`), `defaultClientCount`, fixed `settings`. Omitted: the five standard roles derived from `devName` |
| `defaultRole`, `defaultSubConfigs` | Role used when `launch` names none; sub-configs applied before the call's `subConfigs` |
| `binarySearchDirs` | Project-relative directories searched recursively; the newest matching file wins, so a stale copy never runs instead of a fresh build. Default `Binaries`, `Build`, `Workspace` |
| `binaryEnvTemplate` | Optional variable naming an explicit binary, `{ROLE}` replaced by the upper-case role |
| `bridge` | Setting names of the project's bridge. Without it, launches register no endpoints |
| `nonInteractive` | Set `Common.NonInteractive` when the bridge is enabled, so an error dialog cannot block an unattended run; default `true` |

Unknown keys fail the profile. For `embedded-clients` roles the launcher sets `Server.AutoStartClientOnServer` to `clientCount` and registers one endpoint per client at `bridgePort + index * bridgePortStride`; more than one client needs `portStrideSetting`. Launch arguments add `subConfigs`, scalar `settings` overrides, `clientCount`, `bridge: false`, `bridgeHost`, `bridgePort`, `bridgePortStride`, `binaryPath`, `workingDirectory`, `waitForBridge`, `timeoutMs` and `selectFirstEndpoint`. The result includes the command line, the endpoints, the readiness probe and the binary's `.build-hash` stamp written beside it by the build.

Process ownership is implemented with an independent guardian per launch: on Windows a kill-on-close job object, on Linux a child subreaper that signals only the descendants it adopted. End of input, `SIGTERM`/`SIGINT` and even a hard kill of the adapter stop every owned tree, including descendants that detached into their own session. Other platforms reject owned launches; use `--keep-processes-on-exit` there and stop the processes yourself.

## Wire it into a task and CI

- **Local task.** Point the editor's MCP client configuration at the command above. Keep a second task that runs the same command with `--list-tools`, so a broken module or profile is visible before an agent session starts.
- **Scripted playtests.** A runner starts the adapter through `ai_control_mcp.runner`, which reuses one MCP client implementation and atomic report writers:

```python
import sys
from pathlib import Path

sys.path.insert(0, str(Path("Engine/BuildTools").resolve()))

from ai_control_mcp import runner

client = runner.McpProcess(runner.build_adapter_command([
    "--project-root", ".",
    "--project-module", "Tools/AiControl/mygame_mcp.py",
    "--launch-profile", "Tools/AiControl/launch-profile.json",
    "--tool-prefix", "mygame_",
]))

try:
    runner.initialize_client(client, "mygame-smoke")
    launched = runner.call_tool(client, "mygame_launch", {"role": "server_headless", "subConfigs": ["AiPlaytest"]})
    endpoint = launched["endpoints"][0]
    runner.wait_ready(client, endpoint, tool_prefix="mygame_", timeout_ms=240000)
    moved = runner.call_tool(client, "mygame_move", {**runner.endpoint_args(endpoint), "x": 10, "y": 12})
    runner.verify_command_payload(moved, "move")
    runner.write_json_report(Path("Workspace/mygame-smoke/report.json"), {"status": "passed", "move": moved})
finally:
    client.close()
```

- **CI.** Run `--list-tools` with the project module on every change to the module or profile; it needs no build. Run scripted playtests only on jobs that already built and baked the client and server from one bake, and treat a missing completion, a readiness timeout or a non-zero runner exit as a failure. Do not start the adapter on packaging or release jobs.

## Extend it in a project

### The registration extension point

A project module is ordinary Python that defines `register(registry)`. The adapter imports it (a `.py` path, or a dotted name found through `--module-path`), puts the Engine `BuildTools` directory on the import path, and calls `register` once before serving. A complete minimal module for a game whose observation lists `availableActions` and accepts a `move` command:

```python
"""MyGame tools for the Engine AiControl MCP adapter."""

from ai_control_mcp.registry import InvalidArguments, object_schema
from ai_control_mcp.schemas import act_option_properties, endpoint_target_properties


def register(registry):
    registry.set_readiness(fields=["ready"])
    registry.set_instructions("Call mygame_wait_ready first, then mygame_actions, then mygame_move.")
    registry.add_tool(
        "mygame_actions",
        list_actions,
        description="List the actions the player character can take right now.",
        input_schema=object_schema(endpoint_target_properties()),
        annotations={"readOnlyHint": True},
    )
    registry.add_tool(
        "mygame_move",
        move,
        description="Walk the player character to a grid cell and wait until the client finishes the move.",
        input_schema=object_schema(
            {
                **endpoint_target_properties(),
                **act_option_properties(),
                "x": {"type": "integer", "minimum": 0},
                "y": {"type": "integer", "minimum": 0},
            },
            ["x", "y"],
        ),
    )


def list_actions(context, arguments):
    observation = context.observe(arguments)["observation"]
    return {"availableActions": observation.get("availableActions", [])}


def move(context, arguments):
    observation = context.observe(arguments)["observation"]

    if "move" not in observation.get("availableActions", []):
        raise InvalidArguments("move is not available in the current observation")

    options = {"waitForCompletion": True, **arguments}
    return context.act(options, {"type": "move", "x": arguments["x"], "y": arguments["y"]})
```

The registry API:

| Call | Effect |
|---|---|
| `add_tool(name, handler, description=, input_schema=, annotations=, replace=False)` or the `@registry.tool(...)` decorator | Registers a tool. Names match `[A-Za-z0-9_.-]{1,128}`; a duplicate fails at startup unless `replace=True` names an existing tool on purpose |
| `add_resource(uri, reader, name=, description=, mime_type=)` | Read-only MCP resource such as an operator guide; `reader(context)` returns text |
| `add_prompt(name, render, description=, arguments=)` | MCP prompt; `render(arguments)` returns the prompt text |
| `set_launch_profile(profile)` | The JSON profile as a dictionary; `--launch-profile` overrides it |
| `set_readiness(check=None, fields=())` | Readiness gate: `check(observation, arguments)` returns the names of unmet gates, or `fields` lists observation fields that must be truthy. Callers may replace the fields with `requireFields` |
| `set_instructions(text)` | `instructions` returned by `initialize` |
| `prefix`, `core_name(base)` | The core tool prefix and a core tool's full name |

A handler receives a `ToolContext` and the validated arguments. `context.endpoint(arguments)`, `context.observe(arguments)`, `context.request(arguments, method, params)` and `context.act(arguments, command)` reach the selected or named endpoint through the session; `context.orchestrator` launches and stops owned processes, so a project recipe tool composes `context.orchestrator.launch({...})` from its own scene or account arguments. Return a JSON object, or a `ToolResult` for other content. Raise `InvalidArguments` for caller mistakes and `ToolFailure` for a tool that ran and failed; bridge errors become tool errors automatically.

Input schemas use a validated JSON Schema subset: `type`, `properties`, `required`, `additionalProperties`, `items`, `enum`, `minimum`, `maximum`, `minLength`, `maxLength`, `minItems`, `maxItems`, `description`, `default`, `title`. A schema with another keyword fails at registration, so the adapter never advertises a constraint it does not enforce. `object_schema` rejects unknown arguments by default.

### What stays in the project

Keep typed game actions and their pre-checks, observation projections and summaries, readiness fields, screenshot tools built on project commands, launch recipes for scenes, accounts or multiplayer setups, agent memory, profiles and autonomous policies, prompts and guides, session journals and every playtest scenario. `ai_control_mcp.advisory` offers small helpers (`task_option`, `ranked_options`, `clamp_score`) for tools that suggest next actions; the ranking itself is project policy.

Choose one prefix for the core tools and the project tools so an agent sees one namespace, and never reuse a core tool name for a different meaning; override a core tool only with `replace=True` and the same contract.

### Migrate a project-owned copy

A project that carried its own adapter moves onto the Engine core in one change:

1. Inventory the old adapter: which tools are protocol-generic (status, ping, observe, events, cursor events, raw act, command wait, endpoint selection, readiness, launch, processes, stop, logs) and which are game tools.
2. Delete the private bridge client and stdio loop. Pass `--tool-prefix` with the old prefix so the generic tools keep their names, and move each game tool into the project module through `add_tool`.
3. Translate the old launch code into a launch profile: binary names become `devName` and role targets, the config file becomes `config`, the bridge settings become `bridge`. Recipes that only preset arguments become small project tools that call `context.orchestrator.launch`.
4. Replace `--token` arguments in runners and MCP client configurations by the token variable named by `--token-env`. Move runner helpers onto `ai_control_mcp.runner`.
5. Adapt to the stricter contract: `act` takes the command as a nested `command` object, endpoint lists no longer show tokens, results report `historyReset`, readiness fields are project-declared, and a malformed bridge response is a tool error instead of being skipped.
6. Run `--list-tools`, the project's scripted playtests and a manual agent session, then remove the old adapter, its process-control code and its tests of generic behavior.

## Validation checklist

1. `--list-tools` with the project module and profile exits `0` and lists the core tools with the chosen prefix plus every project tool.
2. Against the protocol sample, `ping`, `observe`, a waited `act` and `next_events` succeed, and a wrong token in the variable yields an `AuthorizationError` tool result.
3. `launch` of each profile role starts the newest build, its command line contains `$TARGET_ENV{...}` instead of the token, and `wait_ready` passes the project gate.
4. Closing the client's stdin stops every launched tree; `processes` is empty afterwards and no game process remains.
5. The adapter, the project module and the bridge are absent from release packages.
