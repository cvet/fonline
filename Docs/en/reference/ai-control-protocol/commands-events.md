---
title: AiControl Commands and Events
document_id: generated-ai-control-protocol-commands-events
locale: en
generated: true
---

# AiControl Commands and Events

> Generated reference. Do not edit directly. Update `BuildTools/AiControlProtocol.json`, then run `python BuildTools/docs_ai_control_protocol.py --write`.

[Index](index.md) | [Wire](wire.md) | [Methods](methods.md) | [Commands and events](commands-events.md) | [Security](security.md) | [Integration and validation](integration-validation.md) | [Canonical JSON](../../../generated/ai-control-protocol.json) | [Guide](../../how-to/ai-control-protocol.md)

| Stable ID | Field | Type | Contract | Source |
| --- | --- | --- | --- | --- |
| <a id="entry-ai-control-protocol-command-type-9b682a6215"></a><code>ai-control-protocol.command.type</code> | <code>type</code> | <code>string</code> | Required non-empty project command discriminator. | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) |
| <a id="entry-ai-control-protocol-command-target-id-bebe0b0729"></a><code>ai-control-protocol.command.target-id</code> | <code>targetId</code> | <code>project identifier</code> | Optional primary target identifier. | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) |
| <a id="entry-ai-control-protocol-command-item-id-c0a823f488"></a><code>ai-control-protocol.command.item-id</code> | <code>itemId</code> | <code>project identifier</code> | Optional item identifier. | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) |
| <a id="entry-ai-control-protocol-command-aux-id-1681634d1b"></a><code>ai-control-protocol.command.aux-id</code> | <code>auxId</code> | <code>project identifier</code> | Optional auxiliary identifier. | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) |
| <a id="entry-ai-control-protocol-command-x-236370e0e6"></a><code>ai-control-protocol.command.x</code> | <code>x</code> | <code>integer</code> | Optional project world or grid X coordinate. | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) |
| <a id="entry-ai-control-protocol-command-y-e2be376e13"></a><code>ai-control-protocol.command.y</code> | <code>y</code> | <code>integer</code> | Optional project world or grid Y coordinate. | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) |
| <a id="entry-ai-control-protocol-command-screen-x-21053d00ef"></a><code>ai-control-protocol.command.screen-x</code> | <code>screenX</code> | <code>integer</code> | Optional screen-space X coordinate. | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) |
| <a id="entry-ai-control-protocol-command-screen-y-7347a1f652"></a><code>ai-control-protocol.command.screen-y</code> | <code>screenY</code> | <code>integer</code> | Optional screen-space Y coordinate. | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) |
| <a id="entry-ai-control-protocol-command-int-arg-e0c52d8389"></a><code>ai-control-protocol.command.int-arg</code> | <code>intArg</code> | <code>integer</code> | Optional project-defined integer payload. | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) |
| <a id="entry-ai-control-protocol-command-string-arg-84674f9aae"></a><code>ai-control-protocol.command.string-arg</code> | <code>stringArg</code> | <code>string</code> | Optional project-defined string payload. | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) |
| <a id="entry-ai-control-protocol-command-append-0c0a34c5f9"></a><code>ai-control-protocol.command.append</code> | <code>append</code> | <code>boolean</code> | Optional request for project-defined append rather than replace semantics. | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) |

## Completion envelope

An accepted command returns a `commandSeq`. The project later appends an event with `type=command_completed`, the same `commandSeq`, a boolean `success`, and a project-readable `message`. Acceptance never implies success.
