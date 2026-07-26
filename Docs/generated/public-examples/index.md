---
title: Generated Public Example Repository Registry
document_id: generated-public-examples-index
locale: en
generated: true
---

# Generated Public Example Repository Registry

> Generated reference. Do not edit this page directly. Update `Examples/PublicRepositories.json` or the governance overlay, then run `python BuildTools/docs_examples.py --write`.

[Human policy](../../PublicExampleRepositories.md) | [Canonical JSON](../public-examples.json) | [Source registry](../../../Examples/PublicRepositories.json)

This registry describes illustrative embedding-project repositories. Engine behavior remains normative only in Engine source, tests, and owning documentation.

## Program contract

| Field | Value |
| --- | --- |
| Organization | `cvet` |
| Engine repository | `cvet/fonline` |
| Release Engine ref | `exact-commit` |
| Development ref | `master` (weekly) |
| Update delivery | `reviewed-pull-request` |
| Contract digest | `f583216e49948329e9ebb5dd1cb9db2decbc199b753f8d61c78f01f67f646c4a` |

## Portfolio

| Order | Repository | Tier | Source status | Remote | Owner | Purpose |
| ---: | --- | --- | --- | --- | --- | --- |
| 1 | `cvet/fonline-project-template` | `foundation` | `source-ready` | `private` / `source-staged` | Build and release maintainers | Canonical GitHub template and source for the first successful headless-project quickstart. |
| 2 | `cvet/fonline-minimal-multiplayer` | `tutorial` | `planned` | `private` / `reserved` | Documentation maintainers | Tiny playable vertical slice used by first server, client, content, scripting, persistence, and test tutorials. |
| 3 | `cvet/fonline-content-showcase` | `showcase` | `planned` | `private` / `reserved` | Content and asset maintainers | Presentation-quality gallery for rendering and authoring capabilities without a large gameplay codebase. |
| 4 | `cvet/fonline-native-extension-sample` | `advanced` | `planned` | `private` / `reserved` | Engine runtime maintainers | Advanced minimal example of project-native C++ composition, lifecycle hooks, script exports, tests, and ABI review. |

## cvet/fonline-project-template

Stable ID: `project-template`  
Engine-owned source: `Examples/MinimalProject`  
Remote: `private` / `source-staged` (created `2026-07-20`)  
Dependencies: None  
Asset policy: `none`

Capabilities:

- `configure`
- `resource-bake`
- `headless-server`
- `native-extension`
- `remote-call-metadata`
- `deterministic-smoke`

Required checks:

- `governance-contract`
- `pinned-engine`
- `current-engine`
- `windows-smoke`
- `linux-smoke`

Exit gate: A clean Windows or Linux host reaches the documented successful server state in under 30 minutes from a tagged exact Engine revision.

## cvet/fonline-minimal-multiplayer

Stable ID: `minimal-multiplayer`  
Engine-owned source: Not assigned  
Remote: `private` / `reserved` (created `2026-07-20`)  
Dependencies: `project-template`  
Asset policy: `project-original-or-permissive`

Capabilities:

- `one-map-location`
- `player-and-npc`
- `item-interaction`
- `replicated-persisted-property`
- `event-and-remote-call`
- `english-and-russian-text`
- `client-visible-smoke`

Required checks:

- `governance-contract`
- `pinned-engine`
- `current-engine`
- `windows-smoke`
- `linux-smoke`
- `tutorial-tag-replay`

Exit gate: Every tutorial checkpoint reproduces from its pinned tag and the final project is understandable without Last Frontier or TLA.

## cvet/fonline-content-showcase

Stable ID: `content-showcase`  
Engine-owned source: Not assigned  
Remote: `private` / `reserved` (created `2026-07-20`)  
Dependencies: `project-template`, `minimal-multiplayer`  
Asset policy: `audited-public-or-project-original`

Capabilities:

- `sprites-and-animation`
- `lighting-and-effects`
- `particles-and-audio`
- `mapper-source-assets`
- `cross-backend-captures`
- `web-build`

Required checks:

- `governance-contract`
- `pinned-engine`
- `current-engine`
- `asset-provenance`
- `performance-budget`
- `capture-reproduction`

Exit gate: A tagged build produces the public showcase and reproducible captures with complete machine-readable rights and provenance.

## cvet/fonline-native-extension-sample

Stable ID: `native-extension-sample`  
Engine-owned source: Not assigned  
Remote: `private` / `reserved` (created `2026-07-20`)  
Dependencies: `project-template`  
Asset policy: `none`

Capabilities:

- `lifecycle-hook`
- `script-export`
- `role-specific-source`
- `focused-native-test`
- `abi-and-compatibility-review`

Required checks:

- `governance-contract`
- `pinned-engine`
- `current-engine`
- `native-unit-test`
- `native-extension-contract`

Exit gate: The tagged sample demonstrates one complete native extension path without game-specific services and passes the generated extension contract checks.
