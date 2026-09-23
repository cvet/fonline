---
title: AiControl Integration and Validation
document_id: generated-ai-control-protocol-integration-validation
locale: en
generated: true
---

# AiControl Integration and Validation

> Generated reference. Do not edit directly. Update `BuildTools/AiControlProtocol.json`, then run `python BuildTools/docs_ai_control_protocol.py --write`.

[Index](index.md) | [Wire](wire.md) | [Methods](methods.md) | [Commands and events](commands-events.md) | [Security](security.md) | [Integration and validation](integration-validation.md) | [Canonical JSON](../../../generated/ai-control-protocol.json) | [Guide](../../how-to/ai-control-protocol.md)

## Integration rules

| Stable ID | Rule | Requirement | Why | Source |
| --- | --- | --- | --- | --- |
| <a id="entry-ai-control-protocol-integration-native-extension-e36ecf4ff0"></a><code>ai-control-protocol.integration.native-extension</code> | Project-owned listener | Implement the listener as an opt-in embedding-project native extension unless and until a reviewed Engine runtime owner is introduced. | The current protocol is reusable, but listener policy and shipping risk remain project responsibilities. | [Examples/AiControlSample/README.md](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/README.md) |
| <a id="entry-ai-control-protocol-integration-loop-ownership-48c18d7fea"></a><code>ai-control-protocol.integration.loop-ownership</code> | Game-loop command drain | The network thread only validates and queues act requests; the owning project client loop drains commands and mutates client state. | Game objects and script runtime state are not socket-thread safe. | [Examples/AiControlSample/ai_control_sample.py](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/ai_control_sample.py) |
| <a id="entry-ai-control-protocol-integration-observation-schema-f76de10379"></a><code>ai-control-protocol.integration.observation-schema</code> | Version project observations | The observation object carries a project-owned schema version and enough readiness/action metadata for its adapter; the Engine protocol does not define game fields. | Last Frontier and TLA legitimately expose different game models. | [Examples/AiControlSample/ai_control_sample.py](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/ai_control_sample.py) |
| <a id="entry-ai-control-protocol-integration-command-completion-dc8b830972"></a><code>ai-control-protocol.integration.command-completion</code> | Completion event | Every accepted command eventually emits command_completed with commandSeq, success, and message, including unsupported or failed project actions. | Acceptance only proves queue insertion; agents need a correlated terminal result. | [Examples/AiControlSample/ai_control_sample.py](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/ai_control_sample.py) |
| <a id="entry-ai-control-protocol-integration-mcp-boundary-61085ca8b1"></a><code>ai-control-protocol.integration.mcp-boundary</code> | MCP adapter boundary | An MCP adapter may map project observations/actions into semantic tools, but its tool namespace, launch orchestration, memory, prompts, and gameplay policies are project-owned. | Transport compatibility must not falsely standardize one game's QA surface. | [Examples/AiControlSample/README.md](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/README.md) |
| <a id="entry-ai-control-protocol-integration-bounded-state-40117587d7"></a><code>ai-control-protocol.integration.bounded-state</code> | Bounded queues and history | Command queues and retained event history have positive configured limits exposed through status; full queues reject rather than overwrite commands. | A stalled client loop must not create unbounded memory growth or silent command loss. | [Examples/AiControlSample/ai_control_sample.py](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/ai_control_sample.py) |

## Validation rules

| Stable ID | Rule | Requirement | Why | Source |
| --- | --- | --- | --- | --- |
| <a id="entry-ai-control-protocol-validation-protocol-smoke-cb6a17829f"></a><code>ai-control-protocol.validation.protocol-smoke</code> | Protocol smoke | Run the reference client against the sample server and prove auth, liveness, status, observation, invalid input, action acceptance, completion, state update, and event cursor behavior. | A rendered schema alone cannot prove connection state and asynchronous lifecycle behavior. | [Examples/AiControlSample/run_protocol_smoke.py](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/run_protocol_smoke.py) |
| <a id="entry-ai-control-protocol-validation-malformed-peer-87c73605b1"></a><code>ai-control-protocol.validation.malformed-peer</code> | Malformed peer responses | Client tests reject malformed JSON, unsupported envelopes, mismatched ids, dual result/error responses, and oversized lines. | Automation must fail closed instead of consuming ambiguous data. | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) |
| <a id="entry-ai-control-protocol-validation-security-30298ca936"></a><code>ai-control-protocol.validation.security</code> | Security boundary tests | Tests prove non-loopback refusal, per-connection authorization, wrong-token rejection, and absence of token command-line arguments. | Security prose must remain executable as the helper evolves. | [Examples/AiControlSample/run_protocol_smoke.py](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/run_protocol_smoke.py) |
| <a id="entry-ai-control-protocol-validation-project-native-1a7e43bc67"></a><code>ai-control-protocol.validation.project-native</code> | Project native integration | A real project separately builds its native bridge, starts an actual client, verifies queue draining on the client loop, and checks clean shutdown/reconnect behavior. | The Python sample is protocol proof, not FOnline native runtime proof. | [Examples/AiControlSample/README.md](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/README.md) |
| <a id="entry-ai-control-protocol-validation-gameplay-authority-b2fddff3ee"></a><code>ai-control-protocol.validation.gameplay-authority</code> | Normal gameplay path | Project tests show representative actions pass through ordinary server validation and that rejected gameplay actions complete as failures. | A successful protocol smoke cannot prove game authorization or anti-cheat boundaries. | [Examples/AiControlSample/README.md](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/README.md) |
| <a id="entry-ai-control-protocol-validation-shipping-artifact-764a4733a6"></a><code>ai-control-protocol.validation.shipping-artifact</code> | Shipping artifact inspection | Release validation confirms production clients cannot open the AiControl listener and do not contain the project remote-command implementation. | A disabled default is weaker than absence from the shipped binary. | [Examples/AiControlSample/README.md](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/README.md) |

## Validation commands

```powershell
python Examples\AiControlSample\run_protocol_smoke.py
python BuildTools\tests\test_ai_control_protocol.py
python BuildTools\docs_ai_control_protocol.py --check
```
