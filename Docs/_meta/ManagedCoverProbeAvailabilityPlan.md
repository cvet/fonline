# Destroyed-entity coverage probe availability

## Status

- State: complete; the fix is retained in the feature-branch integration with master.
- Updated: 2026-11-12 UTC.
- Integration baseline: master `54863a068c6c0dc73d0660a0147cfff601f74396` (`2026.1.119-dev`); target version `2026.1.120-dev`.
- Manual compatibility marker: `0.0.88` on the baseline, `0.0.89` for the integration. The marker advance belongs to the integrated
  attachment and movement network contract; the probe flag alone changes no wire, save or resource layout.
- Original local fix: Engine `7999575afdaf778913725443b1c4beddab74a3a6` (`2026.1.29-dev`). The evidence below keeps the
  revisions it was recorded on and is not relabeled as evidence for the integration.

## Problem and contract

An entity can finish destruction between Managed `Sync.IsUnavailable` reading
its lifecycle state and calling `Game.IsEntityLocked`. The coverage probe is
declared `FO_COVER_PROBE`, but its scalar argument lacks the existing
`AllowDestroyedEntityArgs` export flag. `NativeDataCaller::ConvertArg` therefore
throws before the non-diagnostic probe runs. This interrupts an otherwise
recoverable acquisition attempt.

The probe reports the caller's actual cover, including a destroyed entity still
held by its destroyer. After that cover is released it reports false. Ordinary
native calls retain their destroyed-argument validation and diagnostic order.
No catch-and-continue workaround belongs in an embedding project's location scan.

## Work and evidence

- [x] Add the existing export flag only to `Game.IsEntityLocked`.
- [x] Exercise held and uncovered destroyed handles through generated native bindings.
- [x] Run existing scalar destroyed-argument boundary tests and the full native suite.
- [x] Rebuild affected native roles and bake Managed Server, Client and Mapper.
- [x] Reconcile EN/RU synchronization, Managed and change/migration documentation,
      generated API/contracts, site and AI delivery in dependency order.
- [x] Validate a fresh embedding runtime and cold continuation of its retained data.

The local reproduction originates in a concurrent global-interest scan. That
project-specific receipt is integration evidence; reusable proof remains in
Engine tests. Save layouts, API names, parameter types and return semantics do
not change. Rebuild bindings and communicating roles together; no data conversion
or script call-site replacement is required.

The Managed-configured native suite passed 567 test cases and 666,849 assertions
with zero errors, failures or skips. The generated-binding regression passed all
13 assertions. The added AngelScript coverage assertion remains source coverage
for that separate backend and was not executed by this Managed build.

Documentation validation passed 412 Markdown entries. The rebuilt local site
passed 592 routes, 41 static endpoints and 85,622 local references. AI delivery
passed 28 tasks and 67 retrieval checks (100% success, MRR 0.930). Initial stale
reference pages and the missing internal-plan manifest entry were corrected;
an initial artifact invocation also used an incorrectly doubled relative path.
These local checks do not imply publication or production acceptance.

The embedding runtime passed 467 gameplay tests, including six client path
checks. Four registered opt-in process-restart stages were skipped by the
general faction run and remain separate evidence. An initial qualification
controller incorrectly required zero skips; that failure was retained before
the registry-aware qualification completed. Formatting touched only whitespace
in the native regression, whose rebuilt binary passed the focused 13 assertions;
the full suite evidence remains explicitly tied to its earlier binary.

An ordinary quest-return entry and dialog acceptance passed on the rebuilt
runtime. A separate cold continuation retained both actors' observed permanent
projections, experience, inventory, skills, points, factions and quests, plus
loaded ammunition and premium balances. Both owned native processes exited and
their logs contained no synchronization-access exception. This closes the local
integration check for the fix; wider game acceptance and deployment remain
owned by the embedding project.

## 2026-10-13 rebased source verification

Published master through 2026.1.35-dev is preserved. The retained fix is integrated into 2026.1.37-dev with marker0.0.73 and ABI9. Fresh native regression results:570 cases,667792 assertions, all passed. Historical embedding gameplay receipts above remain tied to their recorded revisions; they are not relabeled as evidence of the new pin.
