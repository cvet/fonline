# Persist retained critter changes from unload callbacks

## Status

- State: complete.
- Baseline: `a131717f8373b78f251c331699d15a4ab285eeca`, `2026.1.25-dev`.
- Embedding baseline: `b28eb8e9132366ef1561d165be2758a39a4db071`.
- Target: `2026.1.26-dev`; no history rewrite or publication.

## Contract

`ServerEngine::UnloadCritter` marks the critter destroying before dispatching
`OnCritterUnload`. The current persistence callback rejects every write to a
destroying entity, including synchronous unload-callback updates that should
survive reload. Permit persistent writes to this exact critter only during
that dispatcher-owned callback window. Restore the permission before native
teardown: map removal must retain the saved location/hex for a later load.
Destroyed entities and ordinary destruction keep rejecting persistence.
The transient permission belongs to the critter instance under its cover;
no saved property, script API, thread-local state or direct database workaround.

## Plan

- [x] Add native lifecycle regressions and observe failure before the fix.
- [x] Implement the bounded unload-event persistence permission.
- [x] Verify real reload, default-value writes, retained map position,
  ordinary destroying/finished entities and absence of recreated records.
- [x] Update the paired lifecycle guide and dated version/migration notes.
- [x] Regenerate/check contract and documentation artifacts.
- [x] Build and run the complete engine unit target without diagnostics.
- [x] Record exact evidence for the English Engine commit.

The accepted red fixture runs both expected native test cases. The unload
case restores LookDistance 12 instead of 42 and retains HideSprite true
instead of false; the finish case passes. An intermediate fixture incorrectly
expected the ordinary load path to restore local-map membership. Source
inspection shows it deliberately clears MapId and enters the global map;
the corrected regression checks saved map/hex fields before load and keeps
that existing policy. Compiler-only and zero-test attempts are retained as
diagnostics, not semantic acceptance.

## Validation boundary

Engine-owned `Test_ServerEntityLifetime.cpp` is the reusable semantic proof.
Embedding-project quest and Managed C# acceptance remain project-owned.
Existing database/property serialization, saved identifiers, public API,
network format and resource schemas keep their meanings.

The first diagnostic fixture was placed in the AngelScript-only lifecycle
target. The embedding build disables that backend, so the filter ran zero
tests (exit 2); this is not a red/green result. The temporary additions were
removed, and the regressions now use native event subscriptions in the
backend-independent server fixture with explicit expected test names.

## Acceptance

- The corrected Windows native source cohort configures and compiles cleanly.
  Both expected native regression cases run, with 29 assertions and no failure,
  error or skip. The full embedding-generated engine unit target passes
  554 cases and 667334 assertions. Its reusable semantic oracles are owned
  by Engine; the embedding game's persistence acceptance remains separate.
- The initial corrected-source build rejected a const captured borrow in the
  unwind guard. A mutable lambda preserves the borrow's writable contract;
  the failed cohort is retained and does not contribute acceptance.
- The seventeen-domain contract diff reports no change or required/missing
  disposition. Generated API/map/index and source documentation checks pass
  414 Markdown entries; lifecycle/localization/version fixtures pass 25 tests.
  Site/search/routes and AI artifacts are prepared in dependency order.
- Jekyll rendering remains unavailable because this Windows environment has
  no bundle/Ruby or WSL. No new rendered-site or browser proof is claimed.
- Native source/headers and the version are frozen throughout the successful
  producer. This completion record and the verification report are subsequent
  documentation edits. Safety stashes remain retained; no history rewrite,
  push, release or production qualification is performed.
