---
layout: default
title: Engine Versioning and Release Notes
locale: en
document_id: engine-versioning
permalink: /Docs/en/how-to/release/versioning.html
---

# Engine Versioning and Release Notes

The root [VERSION](https://github.com/cvet/fonline/blob/master/VERSION) owns the Engine identifier. This year-based CalVer policy, [ADR-0002](../../contributing/decisions/0002-public-api-stability-contract.md), and the [contract-change workflow](../../contributing/contract-change-management.md) are mandatory for every master update. Read the [changelog](../../reference/changelog.md) before upgrading a game.

## Notation

Fields are `YEAR.MAJOR.MINOR.PATCH`: `YEAR` is the four-digit UTC year; `MAJOR` is the positive release-line ordinal for that year, starting at `1`, not a month or SemVer promise. `MINOR` counts master updates within the line; `PATCH` is release-only. No leading zeroes; minor/patch start at `0`.

| Context | Identifier | Meaning |
|---|---|---|
| master | `2026.1.21-dev` | Master change 21 in the first release line of 2026; patch is omitted |
| first cut | `2026.1.21.0-rc` | Stabilization cut from that exact master version |
| stabilization patch | `2026.1.21.1-rc` | First patch on the frozen cut |
| release | `2026.1.21.1` | Promotion of that tested candidate; only the suffix is removed |
| release patch | `2026.1.21.2` | Next patch on the same cut |
| next master line | `2026.2.0-dev`, `2026.2.1-dev`, `2026.2.2-dev` | Independent development after the first cut |

`VERSION` contains exactly one identifier, optionally one newline. Master always has three numeric fields and `-dev`; a release branch always has four fields and either `-rc` or no suffix. There are no numbered rc, alpha, beta, build-time, or Git suffixes. Git tags use `v<version>` and are immutable.

The calendar month is recorded in the UTC change/release date, not in `MAJOR`. Set the current UTC year when preparing a master update. At the first update in a new year, start `YEAR.1.0-dev`; otherwise increment minor by exactly one. After an owner-approved cut, advance major by one and reset minor to zero. Never roll a release branch's year, major, or cut minor forward: even a patch delivered in a later calendar year retains its cut identity.

## Every master update

Each first-parent master commit owns one minor increment and dated bilingual notes, including merges, docs, tests, CI, dependencies and reverts. Reconcile feature fixups with the latest master before integration; a direct multi-commit push must satisfy every step. Rebase unpublished commits onto the fetched tip; merge only diverged published histories. Recalculate the next version and preserve published ancestry before publication.

1. Record the exact baseline and target, audit the complete source/test/doc range, and identify every affected surface and existing-value semantic invariant.
2. Preserve the meaning of all existing valid inputs. Additions/extensions preserve old cases; replacements use a distinct name, type, key, or explicit format boundary and remove the obsolete API. Supply compile/bake/validation errors for affected old usage. Follow ADR-0002; a compatibility label does not waive this rule.
3. Increment `VERSION` from the current master tip. The first adoption of this policy replaces the historical `2022.1.0.wip` with `2026.1.1-dev`; it is not a reconstructed release history.
4. Add `## 2026.1.21-dev - 2026-10-04` in both changelog locales for the exact new identifier. Date each master note by the UTC commit date. Every entry has a non-empty `### Migration` / `### Миграция` section; explicitly say when no project migration is required and why. `Unreleased` is a drafting area, not a substitute for the dated published-step entry. Retain preceding entries so skipped versions can be migrated in order.
5. For every removal/replacement, write the exhaustive migration record below and bind any required generated-contract disposition to its exact base/current digests. Review semantic changes even when the static diff is empty.
6. Update each owning guide, examples, both locales, and `AGENTS.md` routing in the same change. Regenerate affected source-owned models/references, diagrams/screenshots, snippets, translation state, site/search/routes, AI evaluation, and AI delivery in dependency order. [Documentation maintenance](../../contributing/documentation/) owns the complete route.
7. Run affected native/script/configuration/content/package checks plus negative old-usage and positive unaffected/new-usage tests. Run version/changelog and full relevant documentation/site gates. Record exact results, compatibility/ABI/resource decisions, and residual limits; a version bump alone never changes runtime compatibility.
8. Before an authorized commit/publication run the update validator against the actual baseline. CI validates each incoming master step, not only the final version. Publish only with the owner's authorization and retain the published tip as an ancestor.

```bash
python BuildTools/docs_engine_version.py --check --branch master --baseline-git-ref HEAD
python BuildTools/tests/test_engine_version.py
```

For committed publication-range evidence:

```bash
python BuildTools/docs_engine_version.py --check --branch master   --baseline-git-ref <previous-master-sha> --target-git-ref <new-master-sha> --history
```

Source-only `--check` needs no Git. Comparisons require the exact available baseline and target; do not bypass missing/shallow baselines. PRs compare final head to base; pushes validate every incoming first-parent step. Pre-policy revisions still require a complete-range source audit.

## Exhaustive migration record

Record each removed/replaced contract separately, covering every requirement below with exact symbols, files, searches, ordering, values and outputs; agents must not guess replacements or product choices.

1. Exact introducing version and affected old/new revision range; all affected backends, roles, platforms, overloads, settings, fields, content formats and persisted properties.
2. Old name/signature/key/value and its original meaning; new declaration and meaning; removal rationale. State what remains invariant, including units, numeric IDs, sentinels, defaults, ordering, ownership and side effects.
3. Detection commands/patterns and the precise compiler/analyzer/baker/validator diagnostic, including dynamically declared config/content references. Explain how to prove that the project does not use the affected surface.
4. Complete before/after examples and deterministic transformations, including imports/callbacks, each scripting backend, native extension, configuration and authored content that the change touches. Resolve overloads and edge cases explicitly; list a removed feature's disposition when no replacement exists.
5. Ordered regeneration, configure, compile, bake, cache invalidation and packaging steps with all affected sides/roles. Link maintained runnable owners rather than a private game's tools.
6. Persisted-data conversions, exact `MigrationRule` operations where applicable, backup/isolated restore, default/missing/legacy values, idempotency, and rollback boundary. State explicitly when no data migration is needed.
7. Network compatibility marker, ABI/updater/resource-schema consequences, mixed-version restrictions, deployment order and rollback. State explicitly which identities do not change.
8. Acceptance checks: affected old usage must fail before gameplay, unaffected usage must retain results, migrated/new usage must pass, and required save/restore/backend paths must be exercised. Record exact completion evidence and any owner decision that still blocks adoption.

Keep English/Russian instructions semantically identical. For multiple skipped updates execute their records oldest to newest, honoring intermediate data and bake boundaries. Do not replace concrete migration instructions with a Git diff, ledger link, or “update callers.”

## Current API and database migrations

Engine supports only the current API at the selected revision. There are no old API aliases, permissive old-key parsing, fallback semantics, or general legacy runtime modes. Existing values retain their original meanings; a retired enum/property ID must not be reused to reinterpret stored values. A documented defect fix restores that meaning and includes evidence of the invariant; intentionally changing a valid old case is a replacement.

Existing `MigrationRule` handling is the narrow persisted-entity/property conversion exception. It does not preserve the old callable/configuration/content API and does not authorize general database business-data migration. Document the exact old/new stored property or reference and prove the conversion on isolated data; project-specific business transformations remain project-owned. The central `MigrationRule Version` marker is a separate compatibility-hash input, not the public version counter. Necessary old-build refusals carry the existing temporary-compatibility marker; that marker never licenses an old API shim.

An upgrade without project migration either preserves all used contracts or fails explicitly during compilation/baking/validation for affected usage. A same-shape semantic reinterpretation, silent coercion, default substitution, or failure only after normal gameplay begins is unacceptable. If the compiler cannot distinguish old use, introduce a distinct identifier/type/format plus a source-owned validator.

## Cut, stabilize and patch

1. The owner selects and records an exact tested master SHA/version, supported platform matrix and stabilization/patch authority. Cut `release/YEAR.MAJOR` at that SHA and use `YEAR.MAJOR.MINOR.0-rc`; record the frozen cut and its cumulative migration notes in both locales. Branch/tag creation and publication require the owner's command.
2. Independently start the next master major at minor zero. Release fixes keep cut year/major/minor and increment patch once per first-parent change, retaining `-rc` throughout stabilization. Document and validate every patch as completely as master changes.
3. Promote the tested candidate by dropping `-rc` without changing its patch. That promotion may change only `VERSION` and documentation/delivery; a code/build/test change needs another stabilization patch and requalification.
4. Later release fixes increment patch with no suffix. Apply/backport only the approved fixes, with tests, migration notes and documentation for the actual branch behavior; do not merge later master API additions wholesale into the frozen cut. If a fix needs new incompatible semantics, give it a distinct contract and explicit migration rather than repurposing existing values.
5. Run `docs_engine_version.py --check --branch release/YEAR.MAJOR` against the actual baseline and complete qualification matrix. Publish immutable `v<version>` tags and GitHub notes only after explicit authorization. Keep release provenance and supported/backport policy explicit; these future procedures do not claim that release branches or historical site snapshots already exist.

## Identities and consumers

| Identity | Meaning |
|---|---|
| `FO_ENGINE_VERSION` | Engine `VERSION`, validated by the shared parser |
| `FO_ENGINE_REVISION` | Engine SHA, tracked `-dirty` state, or `unknown` for a source archive |
| `FO_BUILD_HASH` | Embedding-project revision used by packages/updater |
| `Common.GameVersion` | Project-owned game version |
| `FO_COMPATIBILITY_VERSION` | Runtime contract digest including the separate manual migration marker |
| ABI/resource-schema versions | Their source-owned serialization/protocol contracts |

`BuildTools/engine_version.py` serves CMake, native codegen and docs. `VERSION`/Engine Git-ref changes invalidate generated metadata. Startup identifies Engine separately from game metadata; nested archives cannot inherit parent Git identity. Site navigation/routes and AI delivery share the version; its site link opens the selected locale's changelog. Docs roll on `current`/`master`; [ADR-0006](../../contributing/decisions/0006-documentation-version-locale-routing.md) owns snapshots. Historical `Since` stays unchanged. Project branding, Android/installer versions and package hashes remain project-owned.

## Source ownership

- `VERSION`, `BuildTools/engine_version.py`, `BuildTools/docs_engine_version.py`, `BuildTools/tests/test_engine_version.py`
- `BuildTools/cmake/helpers/EngineVersion.cmake`, `BuildTools/cmake/stages/Codegen.cmake`, `BuildTools/codegen.py`, `Source/Frontend/ApplicationInit.cpp`
- `.github/workflows/validate.yml`, `AGENTS.md`, `Docs/documentation-manifest.json`, `BuildTools/docs_site.py`, `BuildTools/docs_ai_delivery.py`
