# Documentation Maintenance

> Engine-owned documentation. This page explains how to keep the FOnline engine documentation source-grounded, navigable, and separated from embedding-project content.

## Purpose

Use this page when adding, verifying, or reorganizing engine docs. It is the maintainer workflow companion to [DocumentationBacklog.md](DocumentationBacklog.md), [DocumentationExpansionPlan.md](DocumentationExpansionPlan.md), [DocumentationResearchTemplate.md](DocumentationResearchTemplate.md), [DocumentationVerificationReport.md](DocumentationVerificationReport.md), [Docs/README.md](README.md), and [../AGENTS.md](../AGENTS.md).

## Source paths inspected

- `../AGENTS.md`
- `README.md`
- `Docs/README.md`
- `Docs/DocumentationBacklog.md`
- `Docs/DocumentationExpansionPlan.md`
- `Docs/DocumentationResearchTemplate.md`
- `Docs/DocumentationVerificationReport.md`
- representative source-grounded subsystem docs under `Docs/`

## Documentation ownership rules

Engine docs should explain reusable engine behavior:

- source layout and architecture;
- application and build-tool entry points;
- runtime/entity/network/persistence/client/server/frontend behavior;
- scripting, generated metadata, nullability, and native method exports;
- bakers, mapper, editor, and reusable tool mechanics;
- platform build/debug flows;
- tests and validation routing.

Embedding-project docs should own:

- concrete game content, balance, quests, text, maps, factions, and release policy;
- game-specific scripts and native extensions;
- exact binary names/presets unless explicitly shown as examples;
- product-specific generated outputs and downstream pipelines.

When an engine doc intentionally references the embedding project, use an explicit `../../...` path and label it as an example.

## Standard doc slice workflow

1. Pick a coherent slice from [DocumentationBacklog.md](DocumentationBacklog.md).
2. Inspect the source paths named in the backlog and any related tests/build files.
3. Write or update the owning doc with `Source paths inspected`.
4. Prefer source relationships and ownership boundaries over long API trivia.
5. Add a validation checklist to deep subsystem docs.
6. Update [Docs/README.md](README.md) when a new user-facing page is added.
7. Promote backlog status only after semantic source review, not just link checks.
8. Add a dated section to [DocumentationVerificationReport.md](DocumentationVerificationReport.md) with scope, sources, fixes, and checks.
9. Run validation and keep staging empty unless the owner explicitly asks to stage/commit.

## Backlog status meanings

- `planned` — topic is identified but not researched yet.
- `researching` — source inspection is in progress.
- `drafted` — a first doc exists, but semantic validation is incomplete.
- `verified` — the page was checked against current source and post-edit mechanical checks passed.

Do not leave chat-only progress as the source of truth. If a slice is complete or blocked, record it in the backlog/report.

## Link and path validation

At minimum, validate:

- Markdown links in edited docs.
- Backticked source/build/doc paths that should exist in the engine checkout.
- Stale alternate-layout terms known from older snapshots.
- Test inventory coverage when editing [Testing.md](Testing.md).
- `git diff --check`.
- staged area and working-tree status.

Planned future docs should be written as plain prose unless the checker intentionally exempts them; do not backtick/link missing docs as if they already exist.

## Source-grounded writing conventions

- Start with purpose and reader routing.
- Include `Source paths inspected` for subsystem pages.
- Route to sibling docs instead of duplicating deep details.
- Use exact file paths for source ownership.
- Keep Last Frontier or other game examples explicitly marked as examples.
- Avoid promising unsupported workflows; say what the current source/test/build wiring supports.
- Update related docs when a change moves ownership between pages.

## AI maintainer notes

[../AGENTS.md](../AGENTS.md) is the AI-maintainer entry point. It routes AI agents to human docs and records repository conventions such as not committing/pushing without explicit instruction. Keep it concise and navigational; put detailed human-readable procedures in `Docs/`.

If a future AI agent continues this roadmap, it should re-anchor from git status, the backlog, and the verification report before editing. Context from chat is secondary to the repository state.

## Validation checklist

1. New docs are linked from [Docs/README.md](README.md) and, if relevant, [../AGENTS.md](../AGENTS.md).
2. Backlog status matches actual source validation state.
3. Verification report records every promoted slice.
4. Link/path/test/stale-term checks pass after report updates.
5. `git diff --cached --name-only` is empty unless staging was explicitly requested.
6. Final report names changed files and confirms no commit/push happened unless requested.
