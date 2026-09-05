---
layout: default
title: AiControl Protocol
document_id: ai-control-protocol-guide
locale: en
permalink: /Docs/en/how-to/ai-control-protocol.html
---

# AiControl Protocol

FOnline projects can expose a development client to automated QA agents, local
tools, or an MCP adapter without making one game's commands part of the Engine.
This page defines that reusable boundary. The exact machine contract is the
[generated AiControl protocol reference](../reference/ai-control-protocol/index.md),
and the runnable transport example is
[Examples/AiControlSample](../../../Examples/AiControlSample/README.md).

The contract is **experimental**. Pin an Engine revision, version the project
observation separately, and treat every listener as a security-sensitive
development feature.

## Integration decision

A complete integration keeps four boundaries visible:

1. Reuse only the Engine-owned UTF-8 newline-delimited TCP envelope, common
   methods and errors, size/queue/history bounds, authorization, accepted
   sequence ids, and terminal `command_completed` lifecycle.
2. Keep the native listener's application roles and enablement, observation
   fields and `schemaVersion`, action names and semantics, event extensions,
   MCP tool names and namespaces, launch recipes, agent policy, and redaction
   policy in the embedding project. Last Frontier and TLA are separate project
   examples; neither project's schema becomes Engine behavior.
3. Drain commands through a safe project client-loop handoff and ordinary
   authenticated gameplay paths. The game server remains authoritative; an AI
   adapter never creates a second authority channel.
4. Report evidence in layers. The protocol sample proves framing, bounds,
   authorization, and command lifecycle only; it is not a FOnline runtime
   proof. Separately prove the actual native client bridge with a real client,
   server rejection and authority behavior, and listener exclusion in shipping
   release artifacts.

## What the Engine owns

The Engine repository owns:

- the UTF-8 newline-delimited JSON over TCP envelope;
- the `auth`, `ping`, `status`, `observe`, `events`, and `act` methods;
- the common request/result/error shapes and error codes;
- bounded line, command-queue, and event-history requirements;
- accepted-command sequence ids and the terminal `command_completed` event;
- the loopback-first security policy;
- the standard-library reference client, protocol sample, generated contract,
  focused tests, and compatibility-diff policy.

This is a protocol and integration contract, not a listener in the FOnline core
runtime. The reference server deliberately stands in for a project client loop.

## What the project owns

Every embedding project owns:

- whether a native listener exists and which application roles compile it;
- the compile-time shipping exclusion and runtime enable setting;
- observation fields, `schemaVersion`, readiness gates, entity projections, and
  information-redaction policy;
- action names, parameter semantics, normal gameplay validation, cancellation,
  and failure messages;
- event types beyond `command_completed`;
- MCP tool names, launch recipes, endpoint registries, memory, agent policies,
  prompts, and reports;
- native/script integration tests against the actual project client.

Last Frontier and TLA both have client bridges, but their observations, action
catalogs, QA commands, and MCP namespaces are different. Those surfaces are
evidence for this common envelope, not Engine behavior and not templates to copy
unchanged.

## Architecture

A normal integration has four ownership zones:

1. A project native extension owns a bounded TCP listener, connection-local
   authorization, envelope parsing, and thread-safe queues.
2. The project client loop publishes immutable observation snapshots and drains
   accepted commands at a safe script/native lifecycle point.
3. Project action handlers call ordinary client behavior or authenticated game
   RPCs. The game server remains authoritative and retains server authority.
4. An optional adapter turns project observations and actions into semantic MCP
   tools. It does not alter the wire contract.

The listener thread must never retain or mutate script objects, entities, GUI
nodes, or engine state. Pass plain copied values through a bounded queue. Read
[NativeExtensions.md](../../NativeExtensions.md) for extension roles and lifecycle,
[Script Lifecycle and Concurrency](scripting/lifecycle-and-concurrency.md) for script
thread ownership, and [Nullability.md](../contributing/coding-contracts/nullability.md) for native/script handles.

## Wire protocol

The bridge is a sequential request/response protocol over a TCP byte stream.
Each message is one UTF-8 JSON object followed by LF. The JSON payload before LF
must not exceed 1 MiB. A client sends one request and consumes the matching
response before reusing the connection; multiplexing is not part of the
contract.

The envelope is JSON-RPC-shaped and uses `jsonrpc: "2.0"`, but the bridge does
not claim complete JSON-RPC 2.0 behavior such as notifications, batches,
discovery, or every standard validation rule.

Request:

```json
{"jsonrpc":"2.0","id":1,"method":"observe","params":{}}
```

Successful response:

```json
{"jsonrpc":"2.0","id":1,"result":{"observationSeq":4,"observation":{"schemaVersion":1}}}
```

Error response:

```json
{"jsonrpc":"2.0","id":1,"error":{"code":-32001,"message":"Unauthorized"}}
```

The response must echo the request id and contain exactly one of `result` or
`error`. The generated [wire reference](../reference/ai-control-protocol/wire.md)
owns the error-code table and exact framing rules.

### Authorization

Authorization is connection-local. When a token is configured, `auth` is the
only method accepted before successful authentication. A wrong token returns
`{"authorized":false}` and leaves the same connection unauthorized; a later
attempt may succeed. Reconnecting always starts a new authorization state.

An empty token may be convenient for a loopback-only development listener. It
must never authorize a listener bound beyond loopback.

### Methods

The six protocol methods are intentionally small:

| Method | Purpose |
|--------|---------|
| `auth` | Authorize the current connection with a shared token. |
| `ping` | Prove transport liveness, not game readiness. |
| `status` | Inspect listener state, bounded queue/history occupancy, observation sequence, and last bridge error. |
| `observe` | Read the latest complete project-owned observation. |
| `events` | Poll retained transient events after an exclusive sequence cursor. |
| `act` | Queue one project-owned command and return its command sequence. |

Use the generated [method reference](../reference/ai-control-protocol/methods.md)
for exact parameter and result shapes.

### Observations and events

`observe` returns an envelope around one project object:

```json
{
  "observationSeq": 4,
  "observation": {
    "schemaVersion": 1,
    "ready": true,
    "availableActions": ["move"]
  }
}
```

`observationSeq` changes when the published snapshot is replaced. It is not an
event cursor. The project must publish a complete, internally consistent copy;
clients should never need to combine fields from two snapshots.

`events` uses `afterSeq` as an exclusive cursor and returns retained records in
ascending order. The response also returns `latestSeq`, even when no retained
event is newer. Event history is bounded: a slow adapter can miss old events and
must resynchronize from `observe` instead of assuming an infinite log.

## Command lifecycle

An `act` request has a required non-empty project command `type`. The protocol
also standardizes optional convenience slots named `targetId`, `itemId`,
`auxId`, `x`, `y`, `screenX`, `screenY`, `intArg`, `stringArg`, and `append`.
Their units and meaning remain project-owned; a project may use an action-specific
nested schema instead of forcing every operation through these slots.

Queue acceptance is only the first phase:

```json
{"jsonrpc":"2.0","id":7,"result":{"accepted":true,"commandSeq":12}}
```

After the owning client loop runs the command, it appends a correlated terminal
event:

```json
{
  "seq": 38,
  "event": {
    "type": "command_completed",
    "commandSeq": 12,
    "success": true,
    "message": "moved"
  }
}
```

Every accepted command must eventually complete, including unknown,
game-rejected, cancelled, and teardown-interrupted commands. Keep completion
messages stable enough for diagnostics, but use explicit project event fields
for behavior that an adapter must branch on.

If the command queue is full, `act` returns `-32002`; it must not overwrite or
silently drop a queued command. The adapter should wait for progress, refresh
status/events, and retry only when retrying is safe for that project action.

## Security boundary

An AiControl listener is a remote-control surface even when its intended caller
is a local test process. Apply all of these rules:

- Keep the feature disabled by default.
- Remote operation requires a non-empty token.
- Bind loopback by default. Require an explicit operator opt-in and a non-empty
  token before binding any non-loopback address.
- Treat the token and payload as plaintext. This protocol has no TLS, replay
  protection, user identity, or authorization scopes. Prefer loopback; otherwise
  provide an independently authenticated encrypted tunnel.
- Read tokens from a secret provider or environment variable. Do not put them in
  command lines, committed configs, logs, screenshots, fixtures, or reports.
- Compile the listener and remote-command path out of production clients. A
  runtime setting alone leaves the security and antivirus heuristic surface in
  the binary.
- Bound line size, pending commands, retained events, observation size, and
  observed entity counts.
- Expose player-equivalent actions by default. Keep administrator/setup tools in
  a separate explicit project policy and record their use in QA evidence.
- Redact secrets, hidden server state, other players' private state, and data a
  normal client should not know.

The shared token is a minimal local-development gate, not a general security
system. Do not publish this TCP endpoint directly on a LAN or the internet.

## Native project integration

Use a project native extension when a real FOnline client needs the bridge:

1. Add a project CMake option that gates the complete listener implementation
   and defaults according to the project's development policy. Force it off in
   every release/package workflow.
2. Store only plain copied command, event, status, and observation values in the
   native bridge. Give all shared containers explicit locks and positive caps.
3. Start after the owning client/script module is ready. Refuse unsafe host,
   port, token, or capacity settings before creating the listener thread.
4. Parse and enqueue on the listener thread. Pull commands and publish snapshots
   from the client loop through narrow exported methods.
5. On teardown, stop accepting, close the active socket, wake the thread, join
   it, fail any accepted unfinished commands, unregister callbacks, and release
   project state before the client engine disappears.
6. Keep logs low-volume and secret-free. Report protocol failures through
   `status.lastError` and project diagnostics without copying arbitrary payloads.

One active client connection is sufficient for the reference contract. A
project may support more, but it then owns authorization isolation, event cursors,
write serialization, observation fan-out, and load limits. Clients must not
depend on that extension.

The Engine does not currently provide a native listener helper. Promoting one
would require a core runtime owner, cross-platform socket tests, shutdown and
thread-safety proof, a stable configuration surface, and a separate security
review. Do not describe a project `SourceExt` implementation as built-in Engine
behavior.

## MCP adapter integration

The reference client in `BuildTools/ai_control_client.py` is a small transport
library and diagnostic CLI. An MCP adapter should build on the same rules but
remain project-specific:

1. Connect to an explicit endpoint and authenticate once per connection.
2. Call `status` and `observe`; validate the project's observation
   `schemaVersion` before exposing tools.
3. Convert only current `availableActions` and visible entities into semantic
   tools. Avoid making the model synthesize raw command objects when a typed tool
   can validate them first.
4. Keep an independent event cursor per endpoint. Correlate every accepted
   command with `command_completed` and define timeouts/cancellation.
5. Separate process launch, endpoint selection, screenshots, logs, memory, and
   game-playing policy from the wire client.
6. Include bridge version, project schema version, Engine/project revision, and
   endpoint identity in reports, but never include the token.

Do not publish Last Frontier's `lf_*` tools or TLA's `tla_*` tools as generic
FOnline methods. A small project may expose only `observe`, `move`, and
`interact`; a larger game may need dialogs, inventories, combat, parties, and
project QA setup. Both can use the same envelope.

## Validation

Run the project-neutral proof from the Engine root:

```powershell
python Examples\AiControlSample\run_protocol_smoke.py
python BuildTools\tests\test_ai_control_protocol.py
python BuildTools\tests\test_docs_ai_control_protocol.py
python BuildTools\docs_ai_control_protocol.py --check
```

The smoke starts an ephemeral loopback listener and proves wrong-token rejection,
per-connection authorization, liveness, status fields, initial observation,
invalid params, unknown methods, action acceptance, asynchronous completion,
updated observation, and exclusive event cursors. Focused malformed-peer tests
also reject mismatched ids, ambiguous responses, invalid JSON, and oversized
lines.

This sample is **not a FOnline runtime proof**. A project integration must also:

- build every native role that includes or excludes the bridge;
- run a real client and prove observation publication plus client-loop command
  draining;
- exercise representative normal gameplay acceptance and server rejection;
- test reconnect, queue saturation, event rollover, shutdown during an accepted
  command, and process cleanup;
- inspect release artifacts to prove the listener cannot start and the project
  remote-command implementation is absent.

For longer multi-process scenarios, compose project checks with the
[gameplay test harness](testing/gameplay-and-integration.md) rather than adding process control to
the protocol.

## Maintenance

`BuildTools/AiControlProtocol.json` is the canonical structured contract. Update
it in the same change as any framing, method, error, common field, security,
integration, or validation behavior. Then run:

```powershell
python BuildTools\docs_ai_control_protocol.py --write
python BuildTools\docs_helper_cli.py --write
python BuildTools\docs_contract_diff.py --baseline-git-ref origin/master --allow-missing-baseline --write --enforce
python BuildTools\docs_public_api.py --write
```

The AiControl protocol is the eighteenth generated compatibility domain. A
baseline-public removal, restriction, or stability withdrawal requires an entry
in `Docs/contract-change-dispositions.json` under
[Generated Contract Change Management](../contributing/contract-change-management.md). Project observation/action
schema changes belong in project documentation and project tests unless the
shared envelope itself changes.

When Last Frontier, TLA, or another maintained project changes its bridge,
re-audit the complete incoming project range. Promote only behavior supported by
at least one independent Engine artifact and review; keep game schemas, MCP tool
names, and QA policy in their owning projects.
