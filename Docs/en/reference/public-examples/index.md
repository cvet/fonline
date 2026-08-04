---
title: Generated Public Example Repository Registry
document_id: generated-public-examples-index
locale: en
generated: true
---

# Generated Public Example Repository Registry

> Generated reference. Do not edit this page directly. Update `Examples/PublicRepositories.json` or the governance overlay, then run `python BuildTools/docs_examples.py --write`.

[Human policy](../../how-to/build/public-example-repositories.md) | [Canonical JSON](../../../generated/public-examples.json) | [Source registry](../../../../Examples/PublicRepositories.json)

This registry describes illustrative embedding-project repositories. Engine behavior remains normative only in Engine source, tests, and owning documentation.

## Program contract

| Field | Value |
| --- | --- |
| Organization | `cvet` |
| Engine repository | `cvet/fonline` |
| Release Engine ref | `exact-commit` |
| Development ref | `master` (weekly) |
| Update delivery | `reviewed-pull-request` |
| Contract digest | `105ea0167404015cdcb46c9c3dee0c2328c877b9ecb58adbc230e2fbe89c3c54` |

## Publication evidence

Read each repository's source status, remote visibility/state, observed required-check state, exact Engine pin, update-delivery policy, and Contract digest together. Only `published` source with a `public` / `published` remote and `passing` observed checks is publication evidence. A private, reserved, source-staged, planned, or not-observed row remains pre-publication evidence even when its source is ready.

## Current registry state

- Source/remote: `4` source-ready, `4` private, and `0` published repositories.
- Observed required-check states: `not-observed`.
- Observed Engine pins: `project-template`=`9d74c751f5684f80aef3b35a0eb16a8fabf9fa42`, `minimal-multiplayer`=not observed, `content-showcase`=not observed, `native-extension-sample`=not observed.
- Program values required in the same report: release Engine ref `exact-commit`, update delivery `reviewed-pull-request`, Contract digest `105ea0167404015cdcb46c9c3dee0c2328c877b9ecb58adbc230e2fbe89c3c54`.
- `project-template`: source `source-ready`; remote `private` / `source-staged`; Engine pin `9d74c751f5684f80aef3b35a0eb16a8fabf9fa42`; required checks `not-observed`.
- `minimal-multiplayer`: source `source-ready`; remote `private` / `reserved`; Engine pin not observed; required checks `not-observed`.
- `content-showcase`: source `source-ready`; remote `private` / `reserved`; Engine pin not observed; required checks `not-observed`.
- `native-extension-sample`: source `source-ready`; remote `private` / `reserved`; Engine pin not observed; required checks `not-observed`.

## Portfolio

| Order | Repository | Tier | Source status | Remote | Owner | Purpose |
| ---: | --- | --- | --- | --- | --- | --- |
| 1 | `cvet/fonline-project-template` | `foundation` | `source-ready` | `private` / `source-staged` | Build and release maintainers | Canonical GitHub template and source for the first successful headless-project quickstart. |
| 2 | `cvet/fonline-minimal-multiplayer` | `tutorial` | `source-ready` | `private` / `reserved` | Documentation maintainers | Tiny playable vertical slice used by first server, client, content, scripting, persistence, and test tutorials. |
| 3 | `cvet/fonline-content-showcase` | `showcase` | `source-ready` | `private` / `reserved` | Content and asset maintainers | Presentation-quality gallery for rendering and authoring capabilities without a large gameplay codebase. |
| 4 | `cvet/fonline-native-extension-sample` | `advanced` | `source-ready` | `private` / `reserved` | Engine runtime maintainers | Advanced minimal example of project-native C++ composition, lifecycle hooks, script exports, tests, and ABI review. |

## cvet/fonline-project-template

- Stable ID: `project-template`
- Engine-owned source: `Examples/MinimalProject`
- Remote: `private` / `source-staged` (created `2026-07-20`)
- Remote observation: `2026-08-03`, branch `main`, head `9946ca42c332a294f8fedd2732e7850a01c1ec27`, required checks `not-observed`
- Observed Engine pin: `9d74c751f5684f80aef3b35a0eb16a8fabf9fa42`
- Dependencies: None
- Asset policy: `none`

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

- Stable ID: `minimal-multiplayer`
- Engine-owned source: `Examples/MinimalMultiplayer`
- Remote: `private` / `reserved` (created `2026-07-20`)
- Remote observation: `2026-08-03`, branch `main`, head `97d232431488125b370be352fdcf28f66e6cbf4f`, required checks `not-observed`
- Dependencies: `project-template`
- Asset policy: `project-original-or-permissive`

Capabilities:

- `one-map-location`
- `player-and-npc`
- `item-interaction`
- `replicated-persisted-property`
- `event-and-remote-call`
- `english-and-russian-text`
- `client-visible-smoke`
- `manifest-driven-gameplay-smoke`
- `native-package-acceptance`
- `mapper-ui-capture`
- `spark-particle-authoring`

Required checks:

- `governance-contract`
- `pinned-engine`
- `current-engine`
- `windows-smoke`
- `linux-smoke`
- `windows-package`
- `linux-package`
- `tutorial-tag-replay`

Exit gate: Every tutorial checkpoint reproduces from its pinned tag and the final project is understandable without Last Frontier or TLA.

## cvet/fonline-content-showcase

- Stable ID: `content-showcase`
- Engine-owned source: `Examples/ContentShowcase`
- Remote: `private` / `reserved` (created `2026-07-20`)
- Remote observation: `2026-08-03`, branch `main`, head `011dab0d07eef6387609821206b8ee534ec51c3f`, required checks `not-observed`
- Dependencies: `project-template`, `minimal-multiplayer`
- Asset policy: `audited-public-or-project-original`

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
- `web-build`
- `web-package`
- `web-runtime`
- `capture-reproduction`

Exit gate: A tagged build produces the public showcase and reproducible captures with complete machine-readable rights and provenance.

## cvet/fonline-native-extension-sample

- Stable ID: `native-extension-sample`
- Engine-owned source: `Examples/NativeExtensionSample`
- Remote: `private` / `reserved` (created `2026-07-20`)
- Remote observation: `2026-08-03`, branch `main`, head `97823816ab333a62aced43edd4daafa19c5fee22`, required checks `not-observed`
- Dependencies: `project-template`
- Asset policy: `none`

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
