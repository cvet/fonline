---
title: AiControl Methods
document_id: generated-ai-control-protocol-methods
locale: en
generated: true
---

# AiControl Methods

> Generated reference. Do not edit directly. Update `BuildTools/AiControlProtocol.json`, then run `python BuildTools/docs_ai_control_protocol.py --write`.

[Index](index.md) | [Wire](wire.md) | [Methods](methods.md) | [Commands and events](commands-events.md) | [Security](security.md) | [Integration and validation](integration-validation.md) | [Canonical JSON](../../../generated/ai-control-protocol.json) | [Guide](../../how-to/ai-control-protocol.md)

| Stable ID | Method | Params | Result | Contract | Source |
| --- | --- | --- | --- | --- | --- |
| <a id="entry-ai-control-protocol-method-auth-a69cc82e95"></a><code>ai-control-protocol.method.auth</code> | <code>auth</code> | <code>&#123;token: string&#125;</code> | <code>&#123;authorized: boolean&#125;</code> | Authenticate the current connection with the configured shared token; a failed attempt leaves it unauthorized and a later attempt may succeed. | [Examples/AiControlSample/ai_control_sample.py](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/ai_control_sample.py) |
| <a id="entry-ai-control-protocol-method-ping-87dbd1fcf9"></a><code>ai-control-protocol.method.ping</code> | <code>ping</code> | <code>&#123;&#125;</code> | <code>&#123;ok: true&#125;</code> | Report bridge liveness after authorization. | [Examples/AiControlSample/ai_control_sample.py](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/ai_control_sample.py) |
| <a id="entry-ai-control-protocol-method-status-0ad2a41458"></a><code>ai-control-protocol.method.status</code> | <code>status</code> | <code>&#123;&#125;</code> | <code>&#123;running, host, port, queuedCommands, maxQueuedCommands, events, maxEvents, observationSeq, lastError&#125;</code> | Return transport state, queue/event occupancy and limits, latest observation sequence, and the last bridge error. | [Examples/AiControlSample/ai_control_sample.py](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/ai_control_sample.py) |
| <a id="entry-ai-control-protocol-method-observe-56481e1dba"></a><code>ai-control-protocol.method.observe</code> | <code>observe</code> | <code>&#123;&#125;</code> | <code>&#123;observationSeq: integer, observation: object&#125;</code> | Return the latest complete project-owned observation snapshot and its monotonically increasing replacement sequence. | [Examples/AiControlSample/ai_control_sample.py](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/ai_control_sample.py) |
| <a id="entry-ai-control-protocol-method-events-226d53f902"></a><code>ai-control-protocol.method.events</code> | <code>events</code> | <code>&#123;afterSeq?: integer, limit?: integer&#125;</code> | <code>&#123;latestSeq: integer, events: [&#123;seq: integer, event: object&#125;]&#125;</code> | Return retained events with seq greater than afterSeq in ascending order; clamp limit to 1..500. | [Examples/AiControlSample/ai_control_sample.py](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/ai_control_sample.py) |
| <a id="entry-ai-control-protocol-method-act-938cfa4f33"></a><code>ai-control-protocol.method.act</code> | <code>act</code> | <code>project command object with non-empty type</code> | <code>&#123;accepted: true, commandSeq: integer&#125;</code> | Enqueue one project-defined command and return its sequence; acceptance is not completion or gameplay success. | [Examples/AiControlSample/ai_control_sample.py](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/ai_control_sample.py) |
