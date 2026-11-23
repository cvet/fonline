---
layout: default
title: AiControl Protocol Sample
document_id: ai-control-sample-readme
locale: en
permalink: /Examples/AiControlSample/README.html
---

# AiControl Protocol Sample

This runnable sample demonstrates the project-neutral AiControl transport and
lifecycle contract. It is intentionally not a game and does not embed FOnline:
the server stands in for a project client loop so protocol behavior can be
tested without borrowing Last Frontier or TLA gameplay schemas.

The sample proves:

- UTF-8 newline-delimited JSON request and response framing;
- per-connection shared-token authorization;
- `ping`, `status`, `observe`, `events`, and `act` envelopes;
- bounded command and event queues;
- accepted-command sequence ids and later `command_completed` events;
- project-owned observations and action names.

Run the complete smoke from the Engine root:

```powershell
python Examples\AiControlSample\run_protocol_smoke.py
```

To inspect the bridge manually, put the token in an environment variable and
start the sample. The ready line reports the selected port when `--port 0` is
used:

```powershell
$env:FONLINE_AI_TOKEN = 'local-development-token'
python Examples\AiControlSample\ai_control_sample.py --port 43011
python BuildTools\ai_control_client.py --port 43011 status
python BuildTools\ai_control_client.py --port 43011 act --type move --x 7 --y 9
python BuildTools\ai_control_client.py --port 43011 events
python BuildTools\ai_control_client.py --port 43011 observe
```

The CLI reads the token from `FONLINE_AI_TOKEN` by default and never accepts it
as a command-line argument. Both programs refuse non-loopback endpoints unless
the operator explicitly enables them; the sample additionally requires a
non-empty token. The transport has no TLS, so loopback remains the recommended
boundary.

Read [AiControlProtocol.md](../../Docs/AiControlProtocol.md) before adapting the
sample. A real embedding project must replace the sample observation and action
handler, preserve normal server authority, compile the listener out of shipping
clients, and test the resulting native/script integration separately.
