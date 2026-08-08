---
title: Generated AiControl Protocol Reference
document_id: generated-ai-control-protocol-index
locale: en
generated: true
---

# Generated AiControl Protocol Reference

> Generated reference. Do not edit directly. Update `BuildTools/AiControlProtocol.json`, then run `python BuildTools/docs_ai_control_protocol.py --write`.

[Index](index.md) | [Wire](wire.md) | [Methods](methods.md) | [Commands and events](commands-events.md) | [Security](security.md) | [Integration and validation](integration-validation.md) | [Canonical JSON](../../../generated/ai-control-protocol.json) | [Guide](../../how-to/ai-control-protocol.md)

This reference defines the Engine-owned, project-neutral envelope for opt-in AI observation and control bridges. It does not define a game schema, MCP namespace, listener inside the core runtime, or server-authority bypass.

## Contract status

| Field | Value |
| --- | --- |
| Stability | <code>experimental</code> |
| Support policy | The envelope is versioned but experimental; embedding projects must pin an Engine revision and own their observation and action schemas. |
| Protocol version | <code>1</code> |
| JSON-RPC marker | <code>2.0</code> |
| Default endpoint | <code>127.0.0.1:43011</code> |
| Maximum JSON payload | <code>1048576</code> |
| Stable entries | 49 |
| Source manifest | [BuildTools/AiControlProtocol.json](https://github.com/cvet/fonline/blob/master/BuildTools/AiControlProtocol.json) |
| Contract digest | <code>d23079d2dda2357f9293250a395817f576437531c970245e710c436880bf8c65</code> |

| Reference | Purpose |
| --- | --- |
| [Wire](wire.md) | Framing, envelopes, ordering, and error codes. |
| [Methods](methods.md) | The six transport methods and their results. |
| [Commands and events](commands-events.md) | Common command fields and asynchronous completion. |
| [Security](security.md) | Loopback, tokens, shipping builds, and authority. |
| [Integration and validation](integration-validation.md) | Project ownership and executable evidence. |

## Boundary

Included:

- UTF-8 newline-delimited JSON over a TCP byte stream
- JSON-RPC-shaped request, result, and error envelopes
- authorization, liveness, status, observation, event, and action methods
- bounded transport, command queue, and event history behavior
- accepted-command and asynchronous completion lifecycle
- loopback-first threat boundary and reference validation

Excluded:

- a listener compiled into the FOnline core runtime
- project observation fields, game action names, entity semantics, and readiness gates
- MCP tool names, orchestration recipes, game-playing policies, and model prompts
- server authority bypasses, administrator commands, TLS, discovery, and internet exposure
