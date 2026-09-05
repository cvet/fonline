---
title: AiControl Wire Contract
document_id: generated-ai-control-protocol-wire
locale: en
generated: true
---

# AiControl Wire Contract

> Generated reference. Do not edit directly. Update `BuildTools/AiControlProtocol.json`, then run `python BuildTools/docs_ai_control_protocol.py --write`.

[Index](index.md) | [Wire](wire.md) | [Methods](methods.md) | [Commands and events](commands-events.md) | [Security](security.md) | [Integration and validation](integration-validation.md) | [Canonical JSON](../../../generated/ai-control-protocol.json) | [Guide](../../how-to/ai-control-protocol.md)

| Stable ID | Rule | Requirement | Why | Source |
| --- | --- | --- | --- | --- |
| <a id="entry-ai-control-protocol-wire-tcp-stream-a1496d2000"></a><code>ai-control-protocol.wire.tcp-stream</code> | TCP byte stream | A bridge accepts an explicitly configured TCP endpoint; clients must not assume service discovery, TLS, HTTP, or WebSocket framing. | The common project implementations are local tooling channels, not an internet service protocol. | [Examples/AiControlSample/ai_control_sample.py](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/ai_control_sample.py) |
| <a id="entry-ai-control-protocol-wire-ndjson-62b9249f69"></a><code>ai-control-protocol.wire.ndjson</code> | One JSON object per line | Each request and response is one JSON object terminated by LF; peers process lines in connection order. | Line framing is streamable, inspectable, and shared by both audited project bridges. | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) |
| <a id="entry-ai-control-protocol-wire-utf8-bbcd1f1cb5"></a><code>ai-control-protocol.wire.utf8</code> | UTF-8 encoding | JSON lines are encoded and decoded as strict UTF-8; malformed input receives a parse error or closes the connection. | Project observations and action payloads may contain localized text. | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) |
| <a id="entry-ai-control-protocol-wire-line-limit-f2c1dfbe4d"></a><code>ai-control-protocol.wire.line-limit</code> | Bounded line size | A request or response JSON payload is at most 1 MiB before the LF terminator; oversized input is rejected and the connection may close. | A local automation channel still needs deterministic memory bounds. | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) |
| <a id="entry-ai-control-protocol-wire-request-envelope-e3a81d64d9"></a><code>ai-control-protocol.wire.request-envelope</code> | Request envelope | A request object carries jsonrpc=2.0, a caller-chosen id, a non-empty method string, and an object-valued params member. | A small JSON-RPC-shaped envelope gives correlation without claiming the full JSON-RPC specification. | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) |
| <a id="entry-ai-control-protocol-wire-response-envelope-bfea6842b8"></a><code>ai-control-protocol.wire.response-envelope</code> | Response envelope | A response echoes jsonrpc=2.0 and the request id, then contains exactly one of result or error; error contains an integer code and string message. | Strict correlation prevents one automation step from consuming another step's result. | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) |
| <a id="entry-ai-control-protocol-wire-sequential-connection-09dc2b0d93"></a><code>ai-control-protocol.wire.sequential-connection</code> | Sequential request processing | A client sends a request and consumes its matching response before reusing that connection; clients must not require multiplexing or concurrent in-flight requests. | The contract remains implementable by a single project-owned listener and avoids hidden ordering races. | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) |

## Error codes

| Stable ID | Code | Name | Use | Source |
| --- | --- | --- | --- | --- |
| <a id="entry-ai-control-protocol-error-parse-12e2c3421b"></a><code>ai-control-protocol.error.parse</code> | <code>-32700</code> | Parse error | Reject malformed UTF-8 JSON. | [Examples/AiControlSample/ai_control_sample.py](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/ai_control_sample.py) |
| <a id="entry-ai-control-protocol-error-invalid-request-c140fc72db"></a><code>ai-control-protocol.error.invalid-request</code> | <code>-32600</code> | Invalid request | Reject a non-object, invalid envelope, missing method, or oversized request. | [Examples/AiControlSample/ai_control_sample.py](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/ai_control_sample.py) |
| <a id="entry-ai-control-protocol-error-method-not-found-416e6e9008"></a><code>ai-control-protocol.error.method-not-found</code> | <code>-32601</code> | Method not found | Reject an unknown protocol method. | [Examples/AiControlSample/ai_control_sample.py](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/ai_control_sample.py) |
| <a id="entry-ai-control-protocol-error-invalid-params-24842d0a4b"></a><code>ai-control-protocol.error.invalid-params</code> | <code>-32602</code> | Invalid params | Reject missing command type or structurally invalid method parameters. | [Examples/AiControlSample/ai_control_sample.py](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/ai_control_sample.py) |
| <a id="entry-ai-control-protocol-error-unauthorized-afd4c4e043"></a><code>ai-control-protocol.error.unauthorized</code> | <code>-32001</code> | Unauthorized | Reject every method except auth until the connection is authorized when a token is configured. | [Examples/AiControlSample/ai_control_sample.py](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/ai_control_sample.py) |
| <a id="entry-ai-control-protocol-error-queue-full-1c63470f94"></a><code>ai-control-protocol.error.queue-full</code> | <code>-32002</code> | Command queue full | Reject act when the bounded project command queue has no capacity. | [Examples/AiControlSample/ai_control_sample.py](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/ai_control_sample.py) |
