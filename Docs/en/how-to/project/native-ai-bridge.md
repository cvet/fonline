---
layout: default
title: Embed the AiControl Listener in a Game Client
locale: en
document_id: project-native-ai-bridge
permalink: /Docs/en/how-to/project/native-ai-bridge.html
---

# Embed the AiControl Listener in a Game Client

The Engine ships an optional native listener for the [AiControl protocol](../ai-control-protocol.md): a loopback TCP endpoint inside a development client that lets a QA tool, a local script or an MCP adapter observe the game and queue commands. The Engine owns the transport, authorization, bounded queues and shutdown; the game project owns its settings, observation schema and command set. Use this page to compile the listener into development clients only, start it from project settings, register the project's commands, and migrate a project that kept its own copy of the listener. Like the protocol, the listener is experimental.

## Source paths inspected

- `Source/Client/AiControlListener.h`
- `Source/Client/AiControlListener.cpp`
- `Source/Client/Client.h`
- `Source/Client/Client.cpp`
- `Source/Scripting/ClientGlobalScriptMethods.cpp`
- `Source/Scripting/ServerGlobalScriptMethods.cpp`
- `Source/Server/ServerImage.h`
- `Source/Server/ServerImage.cpp`
- `Source/Essentials/NetSockets.h`
- `Source/Essentials/NetSockets.cpp`
- `Source/Tests/Test_AiControlListener.cpp`
- `Source/Tests/Test_ServerImage.cpp`
- `BuildTools/cmake/stages/Init.cmake`
- `BuildTools/cmake/stages/EngineSources.cmake`
- `BuildTools/cmake/ProjectInterface.json`
- `BuildTools/validation-project/CMakeLists.txt`
- `BuildTools/AiControlProtocol.json`
- `BuildTools/ai_control_client.py`
- `Examples/AiControlSample/`

## What the Engine provides and what stays in the project

| Concern | Owner | Where |
| --- | --- | --- |
| TCP listener, newline framing, `auth`/`ping`/`status`/`observe`/`events`/`act`, error codes | Engine | `AiControlListener` |
| Loopback default, token rule, bounded queue and event history, line limit | Engine | `AiControlListener::Start` and the socket thread |
| Listener lifetime and per-frame dispatch of native command handlers | Engine | `ClientEngine` |
| Compile-time inclusion | Project build | `FO_AI_CONTROL` CMake option |
| Enable flag, host, port, token, queue sizes | Project settings | values passed to `Game.AiControlStart` |
| Observation fields, `schemaVersion`, readiness, redaction | Project scripts | `Game.AiControlSetObservation` |
| Command names, parameters, validation, gameplay calls | Project native code or scripts | command handlers or `Game.AiControlPullCommand` |
| MCP tool names, launch recipes, agent policy | Project tooling | outside the client |

The listener never reads or changes game state on its own thread. It copies each accepted `act` request into a queue; the project command set runs on the client loop thread.

## Compile it into development clients only

`FO_AI_CONTROL` (default `OFF`, see the [CMake option reference](../../reference/cmake/options.md)) compiles the listener into Windows, Linux and macOS client and Mapper builds. On Web, Android and iOS the option has no effect. With the option off, `Game.AiControlAvailable` is `false`, `Game.AiControlStart` throws `AiControlException`, and the binary contains no listener code.

Keep the decision in one project-owned variable so that a release configuration cannot inherit the development default:

```cmake
# Development builds default to the listener; release and package jobs configure with -DPROJECT_AI_CONTROL=OFF
option(PROJECT_AI_CONTROL "Compile the AiControl listener into development clients" ON)
SetOption(FO_AI_CONTROL ${PROJECT_AI_CONTROL})
```

Put the option before `StartProjectGeneration()`, next to the other `SetOption` calls. Every release, store and package workflow must pass `-DPROJECT_AI_CONTROL=OFF`. A runtime setting alone is not enough: it leaves the listening-socket code in the shipped binary.

The listener uses the Engine socket wrapper (`tcp_server` and `tcp_socket` in `Source/Essentials/NetSockets.h`) and needs no extra third-party include path. The address family is IPv4.

## Start and stop it from project settings

The Engine adds no settings. The project declares its own (for example with `///@ Setting Client` in its scripts), decides whether the listener is enabled, and passes the values to the script API:

| Script API | Behavior |
| --- | --- |
| `Game.AiControlAvailable` | `true` when this build compiled the listener. |
| `Game.AiControlStart(host, port, token, maxQueuedCommands, maxEvents)` | Binds synchronously and starts the socket thread. Throws `AiControlException` when the listener is compiled out, already started, the port is outside 1..65535, a bound is not positive, the host cannot be resolved, a non-loopback host has an empty token, or the bind fails. An empty host means `127.0.0.1`. |
| `Game.AiControlStop()` | Stops accepting, closes the connection, joins the socket thread and completes every queued command with `success: false`. Does nothing when stopped. |
| `Game.AiControlIsRunning()` | `true` while the socket thread serves connections. |
| `Game.AiControlSetObservation(json)` | Replaces the observation. The text must be one JSON object of at most 1 MiB minus a 1 KiB envelope reserve; line breaks are flattened. |
| `Game.AiControlPushEvent(json)` | Appends one JSON object of at most 64 KiB to the bounded event history. |
| `Game.AiControlPullCommand(...)` | Takes the next command that no native handler claims; see below. |
| `Game.AiControlCompleteCommand(commandSeq, success, message)` | Appends the terminal `command_completed` event; the message is cut at 4 KiB. |
| `Game.AiControlGetStatus()` | Returns the `status` result object as JSON text. |

Start the listener from the client start event after the project's modules are ready. Compute per-instance ports in the project when several clients run on one machine (for example a base port plus a client index). Read the token from a project secret source or an environment variable; never put it in a committed config, a command line, a log or a report.

`ClientEngine` creates the listener object with the engine, calls native command handlers once per frame before `OnLoop`, and destroys the listener in `ClientEngine::Shutdown` right after `OnFinish`. Scripts that publish a final observation or complete commands must do it in `OnFinish` or earlier.

## Security model

- Off by default twice: the build option is `OFF`, and nothing starts the listener until the project calls `Game.AiControlStart`.
- Loopback first: an empty host binds `127.0.0.1`. Any address in `127.0.0.0/8` counts as loopback after name resolution.
- A non-loopback address (including `0.0.0.0`) requires a non-empty token; the start call throws otherwise. Prefer an authenticated encrypted tunnel to a loopback listener over a routable bind.
- Authorization is per connection. With a token, every method except `auth` returns `-32001` until `auth` succeeds; a reconnect starts unauthorized. Token comparison takes the same time for every candidate of a given length.
- One connection is served at a time. Requests are limited to 1 MiB per line; a larger line receives `-32600` and the connection closes. A peer that stops reading is dropped after 10 seconds of blocked sending, and `Stop()` interrupts the wait.
- The token and every payload are plaintext. The listener grants no server authority: project commands must use ordinary client input or authenticated gameplay calls.
- Error-level log lines and log lines that mention exceptions or assertions are forwarded as `runtime_exception` events while the listener runs. Keep secrets out of logs, because these events reach the connected tool.

## Register the project command set

A project can handle commands natively, from scripts, or both. Native handlers are registered from project CLIENT code, typically in the `ClientInitHook` engine hook (see [native extension hooks](../../reference/native-extension/hooks.md)):

```cpp
#include "Common.h"

#include "Client.h"

FO_BEGIN_NAMESPACE

///@ EngineHook
FO_SCRIPT_API void ClientInitHook(ptr<ClientEngine> client);

void ClientInitHook(ptr<ClientEngine> client)
{
#if FO_HAVE_AI_CONTROL
    client->GetAiControl()->RegisterCommandHandler("wait", [](const AiControlCommand& command) {
        return AiControlCommandResult {.Success = true, .Message = strex("waited {}", command.IntArg).str()};
    });
#else
    ignore_unused(client);
#endif
}

FO_END_NAMESPACE
```

Command routing rules:

1. Commands keep their `act` order. Native handlers run on the client loop thread, either from the per-frame dispatch or inside `Game.AiControlPullCommand`.
2. A command whose `type` has a registered handler is completed with the handler's `AiControlCommandResult`.
3. Registering the empty type installs a fallback handler for every type without its own handler. Without a fallback, the first unclaimed command stops native dispatch until a script takes it with `Game.AiControlPullCommand` and completes it with `Game.AiControlCompleteCommand`.
4. A handler that throws completes its command with `success: false` and the exception text, then the exception propagates to the client loop.
5. Registering the same type twice throws. Handlers stay registered until client shutdown, so capture only state that lives as long as the client engine.

A project that handles commands only natively must register a fallback that rejects unknown types; otherwise an unknown command waits for a script consumer that never comes. A project that handles commands only in scripts registers nothing and drains `Game.AiControlPullCommand` from its loop event.

`AiControlCommand` decodes the protocol's convenience slots (`targetId`, `itemId`, `auxId`, `x`, `y`, `screenX`, `screenY`, `intArg`, `stringArg`, `append`) and keeps the complete `params` object in `ParamsJson` for project fields. Slot types are strict: identifiers are JSON integers or decimal strings, coordinates and `intArg` are 32-bit integers, `stringArg` is a string and `append` is a boolean. A wrong type is rejected with `-32602` before the command is queued. Every queued command must end with exactly one `command_completed` event.

## Verify the listener

The Engine unit tests start real listeners on loopback ports from 46100 upward and cover configuration refusal, token and per-connection authorization, envelope and framing errors, CRLF and pipelined requests, observation flattening, slot decoding, queue saturation, history rollover, handler ordering and failure, oversized and abandoned peers, bounded `Stop()`, port reuse after a stop and destruction of a running listener. The `BuildTools/validation-project` used by `validate unit-tests` enables `FO_AI_CONTROL`.

Run them from a configured build directory of a project that enables the option (replace `<ProjectDevName>`):

```powershell
cmake --build . --config Release --target <ProjectDevName>_UnitTests
.\Binaries\Tests-Windows-win64\<ProjectDevName>_UnitTests.exe "AiControlListener*"
```

```bash
cmake --build . --config Release --target <ProjectDevName>_UnitTests
./Binaries/Tests-Linux-x64/<ProjectDevName>_UnitTests "AiControlListener*"
```

The test binary exits with `0` and prints `All tests passed` when every case passes; any failing assertion makes the exit code non-zero. These tests prove the transport inside one process. Prove the project integration separately with a running development client and the reference client:

```powershell
$env:PROJECT_AI_TOKEN = "<token>"
python BuildTools\ai_control_client.py --port <port> --token-env PROJECT_AI_TOKEN status
python BuildTools\ai_control_client.py --port <port> --token-env PROJECT_AI_TOKEN act --type wait --int-arg 1
python BuildTools\ai_control_client.py --port <port> --token-env PROJECT_AI_TOKEN events
```

```bash
export PROJECT_AI_TOKEN='<token>'
python3 BuildTools/ai_control_client.py --port <port> --token-env PROJECT_AI_TOKEN status
python3 BuildTools/ai_control_client.py --port <port> --token-env PROJECT_AI_TOKEN act --type wait --int-arg 1
python3 BuildTools/ai_control_client.py --port <port> --token-env PROJECT_AI_TOKEN events
```

Wire the unit tests into CI through the project's ordinary unit-test job; the listener needs no extra job. Add a release-artifact check that a client built with the release configuration reports `Game.AiControlAvailable == false` or does not contain the listener symbols.

## Read baked images on the server

Projects that kept a listener copy often kept a slot-based server image reader next to it, for example to sample a world-map mask. The Engine provides that reader as a script-owned reference type:

| Script API | Behavior |
| --- | --- |
| `Game.LoadImage(imageName)` | Reads a baked image resource with exactly one direction and one frame and returns a new `ServerImage`. Throws `ScriptException` for an empty name, a missing file, several directions or frames, a frame that references another frame, or an empty frame. |
| `ServerImage.GetSize()` | Width and height in pixels. |
| `ServerImage.GetColor(pos)` | The `ucolor` of one pixel; throws `ScriptException` for a position outside the image. |

The image lives as long as the script keeps a reference; there are no numbered slots to free. A project with `Game.LoadImage(slot, name)` and `Game.GetImageColor(slot, pos)` exports deletes them (codegen reports the duplicate `Game.LoadImage` until it does), stores the returned `ServerImage` in a script variable, and replaces `GetImageColor(slot, pos)` with `image.GetColor(pos)`. Unit tests now receive the real image or an exception instead of an empty size.

## Extend it in a project

- Keep project settings (`AiControl.Enabled`, host, port, port stride, token, queue sizes, observation interval) in the project and translate them into one `Game.AiControlStart` call.
- Put game commands in project handlers or scripts. Name them after player actions, validate them with the same rules as player input, and return stable completion messages.
- Version the observation object with a project `schemaVersion` and publish complete snapshots only.
- Add project events (for example `bridge_started`, dialog or combat notifications) with `Game.AiControlPushEvent`. The Engine reserves only `command_completed` and `runtime_exception`.
- Keep MCP adapters, tool names and launch recipes in project tooling; build them on `BuildTools/ai_control_client.py` or the same wire rules.

### Migrate from a project copy of the listener

A project that compiled its own listener into a CLIENT extension (exported `Client_Game_AiControl*` methods) migrates as follows:

1. Delete the project listener source and its CMake registration, the project asio include path that existed only for it, and the project compile definition that gated it. Set `FO_AI_CONTROL` from the project's development option instead. Codegen fails with duplicate exports until the old methods are gone.
2. Replace `Game.AiControlStart(enabled, host, port, token, maxQueuedCommands, maxEvents)` with an `if` on the project enable setting followed by `Game.AiControlStart(host, port, token, maxQueuedCommands, maxEvents)`. The call throws instead of returning `false`; zero or negative bounds are no longer raised to 1, and a second start without `Game.AiControlStop()` throws.
3. Add the `paramsJson` output to every `Game.AiControlPullCommand` call. Project-only fields that the old command structure carried as an extra slot are read from `paramsJson`, or move into `stringArg` in the project's adapter.
4. Publish observations and events as JSON objects. An empty string is no longer turned into `{}`, and invalid JSON throws at the publishing call instead of producing a parse-error field in the response.
5. Send slot values with their JSON types. Strings such as `"5"` for `x` or `intArg` are rejected with `-32602`; identifiers may stay decimal strings.
6. Keep project helpers that are not part of the protocol (entity id lookup, simulated touch input, embedded-client index) in the project extension under their own names.
7. Rebuild every client role, run the Engine `AiControlListener*` tests and the project's live-client checks, and confirm the release configuration excludes the listener.

## Validation checklist

- Development clients configure with `FO_AI_CONTROL=ON`; release and package jobs pass the project switch that turns it off.
- `Game.AiControlStart` is called once, after project initialization, with a loopback host or with a non-empty token from a secret source.
- Every command type the project accepts has a native handler or a script consumer, and every pulled command is completed.
- Observations are complete JSON objects with a project `schemaVersion` and contain no hidden server state or secrets.
- `AiControlListener*` unit tests pass, the reference client reaches a running development client, and a release build reports the listener as unavailable.
