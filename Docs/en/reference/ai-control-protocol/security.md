---
title: AiControl Security Boundary
document_id: generated-ai-control-protocol-security
locale: en
generated: true
---

# AiControl Security Boundary

> Generated reference. Do not edit directly. Update `BuildTools/AiControlProtocol.json`, then run `python BuildTools/docs_ai_control_protocol.py --write`.

[Index](index.md) | [Wire](wire.md) | [Methods](methods.md) | [Commands and events](commands-events.md) | [Security](security.md) | [Integration and validation](integration-validation.md) | [Canonical JSON](../../../generated/ai-control-protocol.json) | [Guide](../../how-to/ai-control-protocol.md)

## Security rules

| Stable ID | Rule | Requirement | Why | Source |
| --- | --- | --- | --- | --- |
| <a id="entry-ai-control-protocol-security-disabled-default-cc8a9a4e17"></a><code>ai-control-protocol.security.disabled-default</code> | Disabled by default | An embedding project must require explicit configuration to start a listener. | A control listener is a security-sensitive development feature. | [Examples/AiControlSample/README.md](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/README.md) |
| <a id="entry-ai-control-protocol-security-loopback-default-9ef3468888"></a><code>ai-control-protocol.security.loopback-default</code> | Loopback first | Listeners and clients default to 127.0.0.1 and refuse non-loopback operation without explicit operator opt-in. | The protocol has no transport encryption or peer identity beyond a shared token. | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) |
| <a id="entry-ai-control-protocol-security-remote-token-46ab4a63ad"></a><code>ai-control-protocol.security.remote-token</code> | Remote token required | A sample or project listener exposed beyond loopback requires a non-empty token in addition to explicit remote opt-in. | An empty-token remote listener is an unauthenticated process-control channel. | [Examples/AiControlSample/ai_control_sample.py](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/ai_control_sample.py) |
| <a id="entry-ai-control-protocol-security-plaintext-376f7db0e7"></a><code>ai-control-protocol.security.plaintext</code> | No TLS | Treat the shared token and all payloads as plaintext on the TCP path; use an authenticated encrypted tunnel if non-loopback transport is unavoidable. | Token authentication is not confidentiality or replay protection. | [Examples/AiControlSample/README.md](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/README.md) |
| <a id="entry-ai-control-protocol-security-secret-input-206c1da3bb"></a><code>ai-control-protocol.security.secret-input</code> | Environment-backed token | Reference tools read tokens from a named environment variable and do not accept or print raw token arguments. | Command lines, process listings, shell history, and reports are common secret leak paths. | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) |
| <a id="entry-ai-control-protocol-security-shipping-build-5df2405dda"></a><code>ai-control-protocol.security.shipping-build</code> | Compile out shipping surface | Projects should compile the listener and remote-command path out of production clients, not merely disable them in a runtime config. | Removing the socket and command path reduces attack and antivirus heuristic surface. | [Examples/AiControlSample/README.md](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/README.md) |
| <a id="entry-ai-control-protocol-security-server-authority-b28f36d0f6"></a><code>ai-control-protocol.security.server-authority</code> | Preserve server authority | Project actions use normal client input or authenticated gameplay RPC paths; the bridge does not grant server authority or administrator capability by default. | AI QA should exercise the same validation boundary as a player. | [Examples/AiControlSample/README.md](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/README.md) |
