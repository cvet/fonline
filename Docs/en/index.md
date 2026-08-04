---
layout: default
title: FOnline Engine Documentation
locale: en
document_id: documentation-home
permalink: /Docs/en/
---

# FOnline Engine Documentation

This is the human documentation entry point for the reusable FOnline engine.
It is written for game developers, tool authors, release operators, and engine
contributors working from an Engine checkout without another game's
documentation.

## Start here

- [Getting Started](tutorials/getting-started.md) routes a new developer through
  the repository, supported workflows, and the engine/project ownership split.
- [First FOnline Headless Project](tutorials/first-project.md) configures, builds,
  and runs the smallest tested server milestone.
- [First Playable Client](tutorials/first-client.md) adds a connected desktop
  client, map loading, and a server-authoritative interaction.
- [First Content Change](tutorials/first-content.md) follows localized prototype
  data through baking and runtime lookup.
- [First Automated Test](tutorials/first-test.md) adds metadata, server-content,
  and client-visible checks.
- [Minimal Project](../../Examples/MinimalProject/README.md) and
  [Minimal Multiplayer](../../Examples/MinimalMultiplayer/README.md) are the
  engine-owned runnable sources behind the tutorials.

## Build and ship a game

- [Embedding Project](how-to/build/embedding-project.md) defines the repository
  boundary between a game and its pinned Engine revision.
- [Project Configuration](how-to/build/project-configuration.md) covers
  `.fomain`, resource packs, sub-configs, overrides, and validation.
- [Build Workflow](how-to/build/index.md) covers presets, prerequisites, target
  selection, and the normal configure/build loop.
- [Generated Content Workflow](how-to/build/generated-content.md) gives the
  dependency order for codegen, scripts, resources, metadata, docs, and site
  artifacts.
- [Support Matrix](reference/platforms/support-matrix.md) distinguishes verified,
  smoke-gated, source-capable, experimental, and unsupported combinations.
- [Packaging and Release](how-to/release/packaging.md) owns package declarations,
  payloads, provenance, signing boundaries, acceptance, publication, and
  rollback.
- [Security and Secrets](how-to/release/security-and-secrets.md),
  [Release Operations](how-to/release/operations.md), and
  [Backup and Recovery](how-to/release/backup-and-recovery.md) cover the reusable
  operational boundaries around a release.
- [Engine Upgrade Guide](how-to/migration/engine-upgrade.md) defines full-range
  revision reconciliation, compatibility review, regeneration, and rollback.

## Author content

- [Prototype Format](how-to/content/prototype-format.md)
- [Map Format](how-to/content/map-format.md)
- [Model Format](how-to/content/model-format.md) and
  [Model Animation](how-to/content/model-animation.md)
- [Image and Sprite Formats](how-to/content/image-format.md) and
  [Sprite Root Motion](how-to/content/sprite-root-motion.md)
- [Text and Localization](how-to/content/text-and-localization.md)
- [Effect Format](how-to/content/effect-format.md)
- [Particle Format and Runtime](how-to/content/particle-format.md)
- [Font Formats and Text Layout](how-to/content/font-format.md)
- [Audio](how-to/content/audio.md) and [Video](how-to/content/video.md)

Each guide links to its generated reference when the engine owns a declarative
grammar or machine-readable contract. A game owns its concrete catalogs,
balance, quests, dialog content, visual policy, and localization policy.

## Understand the runtime

- [Engine Architecture](explanation/architecture/index.md) and
  [Source Tree](contributing/source-tree/index.md) explain where behavior belongs.
- [Entity and Property Model](explanation/entity-and-property-model/index.md),
  [Maps, Movement, and Geometry](explanation/maps-and-movement.md),
  [Authority and Networking](explanation/authority-and-networking/index.md), and
  [Persistence](explanation/persistence/index.md) describe the reusable world
  model.
- [Client Runtime](explanation/runtime/client.md),
  [Server Runtime](explanation/runtime/server.md),
  [Frontend and Rendering](explanation/rendering/index.md), and
  [Client Runtime Split and Updater](explanation/runtime/client-updater.md) cover
  the process and presentation layers.
- [Scripting Runtime](explanation/scripting-runtime/index.md),
  [Script Lifecycle and Concurrency](how-to/scripting/lifecycle-and-concurrency.md),
  [AngelScript Style and Refactoring](how-to/scripting/style-and-refactoring.md),
  and [Remote Calls](reference/scripting/remote-calls.md) define the reusable
  scripting contract.
- [GUI Runtime](how-to/runtime/gui.md) covers screen registration, lifecycle,
  layout, drawing, input, and embedding hooks.

## Use tools and validate changes

- [Mapper Interactive Manual](how-to/tools/mapper-interactive.md) covers daily
  map editing, history, save discipline, and visible review.
- [Mapper Tools](how-to/tools/mapper.md) covers mapper lifecycle, script
  automation, deterministic screenshots, and headless integration.
- [Particle Authoring Tools](how-to/tools/particle-authoring.md) and
  [Animation and Particle Viewers](how-to/tools/animation-particle-viewers.md)
  cover focused visual inspection.
- [Gameplay and Integration Testing](how-to/testing/gameplay-and-integration.md),
  [Testing](contributing/testing/index.md), [Debugging](troubleshooting/debugging.md),
  and [Profiling](how-to/quality/profiling.md) route changes to appropriate
  evidence.
- [AiControl Protocol](how-to/ai-control-protocol.md) and the
  [runnable protocol sample](../../Examples/AiControlSample/README.md) define the
  project-neutral automation transport and its security boundary.

## Reference

- [Applications](reference/applications.md) lists executable and library entry
  points.
- [CMake Project Interface](reference/cmake/index.md) contains exact options,
  stages, hooks, and project-facing helpers.
- [BuildTools CLI](reference/buildtools/index.md) and
  [Helper CLI](reference/helper-cli/index.md) contain parser-backed commands,
  arguments, defaults, choices, and exact help output.
- [Generated API and Metadata](reference/metadata/index.md) explains the
  source-annotation, code generation, generated-model, and publication pipeline.
- [Essentials](reference/native/essentials.md) defines the native foundation
  layer, its strict include order, allocation vocabulary, and subsystem map.
- [Public Contract Index](reference/public-contract/index.md) routes all generated contract
  domains and their stability labels.
- [Script API Method Ownership](reference/script-api/method-ownership.md) maps
  native script exports by runtime side and receiver family.
- [Native Extension Reference](reference/native-extension/index.md) covers roles,
  hooks, fallbacks, and generated bindings.
- [Platform Support Matrix](reference/platforms/support-matrix.md) records the
  evidence behind support claims.

Canonical machine-readable models live under [`Docs/generated/`](../generated/).
AI clients should start with [`llms.txt`](../../llms.txt), use
[`docs-manifest.json`](../../docs-manifest.json) for stable document metadata,
and load [`llms-full.txt`](../../llms-full.txt) only when a bounded standalone
corpus is appropriate.

## Maintain the engine and documentation

- [Documentation Maintenance](contributing/documentation/index.md) defines
  source ownership, revision reconciliation, regeneration order, and review
  evidence.
- [Translation Workflow](contributing/documentation/translation.md) defines the
  English source, Russian mirror, glossary, freshness hashes, and code parity.
- [Site Publication](contributing/documentation/site-publication.md) defines the
  GitHub Pages/Jekyll route, local preview, rendered artifact, and `fonline.ru`
  production checks.
- [Contract Change Management](contributing/contract-change-management.md)
  classifies generated-model changes and breaking-change dispositions.
- [Documentation Snippet Validation](contributing/documentation/snippets.md) and
  [AI Documentation Evaluation](contributing/documentation/ai-evaluation.md)
  define executable examples and retrieval/evidence quality gates.
- [Native Coding Contracts](contributing/coding-contracts/) and
  [Third-Party Maintenance](contributing/third-party/) own low-level contributor
  conventions.

The active production roadmap and verification history remain in
[`Docs/ProductionDocumentationPlan.md`](https://github.com/cvet/fonline/blob/master/Docs/ProductionDocumentationPlan.md),
[`Docs/_meta/DocumentationBacklog.md`](https://github.com/cvet/fonline/blob/master/Docs/_meta/DocumentationBacklog.md), and
[`Docs/_meta/DocumentationVerificationReport.md`](https://github.com/cvet/fonline/blob/master/Docs/_meta/DocumentationVerificationReport.md)
until their planned `_meta/` migration has durable redirects.

## Ownership boundary

Engine documentation owns reusable runtime behavior, tools, build and platform
contracts, formats, generated APIs, and native/script conventions. An embedding
project owns concrete game content, product rules, deployment policy, service
credentials, and project-specific commands.

Normative Engine procedures must be executable from an Engine checkout and must
not depend on Last Frontier, TLA, or another project's files. External projects
may provide version-pinned evidence, but reusable helpers and regressions cited
as the source of an Engine guarantee belong in this repository.
