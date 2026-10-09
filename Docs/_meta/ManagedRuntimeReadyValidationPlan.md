# Managed runtime READY validation

## State

Validating documentation reconciliation after owner publication. No commit or
publication is authorized for this follow-up.

## Baseline and scope

The current published baseline is Engine
`6db09ef1d49a44307dbdadf747087247d5f5e78b` (`2026.1.17-dev`), which contains
the runtime validation fix. The follow-up candidate is `2026.1.18-dev` and
changes documentation review metadata, a stale documentation-workflow test and completion notes only. The project
baseline is `4de7aaa1739456e4f67c39f63fee30a302e8500d`. The earlier validation
results below retain their exact original source scope.

The original baseline was Engine `cb33d42c7e4e2678e9b34ccf913d7b88186f979b`
(`2026.1.15-dev`). The candidate was prepared in an isolated worktree and
copied into the main Engine checkout after validation. Engine HEAD and the
project gitlink retained that published baseline during the run. Its embedding-project
baseline was `825b865aa1308c8bcf062b7f14975d21de60d0c3`, published independently
by the owner after validation of `263047b2e200fa8bbeb65a386d2dbd2e4ffb7915` started.
The original candidate version was `2026.1.16-dev`.

The defect is reproducible without a runtime build: an existing empty output
directory plus READY causes `setup_mono` to report success. A Windows
embedding-job log separately records missing `monosgen-2.0.lib` after a
successful setup claim. The reason that archive disappeared is not established.

## Contract

A READY marker certifies the inputs required by native linking and managed
baking: the embedding header, CoreLib/class-library entry, platform-specific
static archives, and browser glue where applicable. Check an existing tree,
a downloaded cache candidate, prebuilt input, and the freshly published result.
Recover incomplete local/cached trees through the existing restore/republication
route. Reject incomplete prebuilt input before copying or writing READY.
Keep valid trees reusable, target filenames correct, source-build markers
resumable, and valid cache identities unchanged.

No gameplay, script API, configuration-key, persistence, network-compatibility,
native ABI or resource-schema migration is introduced. Player knowledge-base
advice remains accurate because runtime preparation changes no game mechanic.
No Authoring or project operations files are touched.

## Original validation milestones

- [x] Add missing/empty-input regressions across desktop, Android, Apple and Web targets; confirm the baseline fails
- [x] Validate every setup entry and recovery result against the required target inputs
- [x] Keep existing complete-tree reuse and cache/source recovery tests green
- [x] Update the English owning guide, Russian mirror and dated migration notes
- [x] Regenerate affected documentation models, translation, site and AI artifacts in dependency order
- [x] Validate version/changelog against the exact baseline and required documentation gates
- [x] Integrate the verified candidate into the main Engine worktree after current project validation releases its inputs
- [x] Reconcile project integration documentation
- [x] Render the complete Jekyll artifact and run the full pinned browser audit
- [x] Complete the affected native/managed gameplay checks

## Current follow-up

- [x] Rebase unpublished work onto the fetched project and Engine bases; preserve published history
- [x] Retain named safety stashes and resolve restored documentation conflicts
- [x] Preserve the incoming ignored-output layout and dated release notes
- [x] Review translation parity and correct the testing/changelog review hashes
- [x] Prepare documentation outputs under the new layout and validate the authored corpus
- [x] Render the current owned build and pass the full 592-route browser audit
- [x] Reconcile the stale generated-workflow test with the aggregate preparation entrypoint
- [ ] Repeat full documentation discovery and generated/rendered checks after the test/release-note correction
- [ ] Qualify affected native/project integration on the new published source scope

The owner independently published the preserved runtime and cursor fixes after
conflict resolution. No ordinary agent commit or push occurred. The intervening
Engine update restores application defaults and moves documentation delivery
outputs out of versioned source. The project update adds checkpoint landing.
Earlier native/browser results do not qualify these additional changes.

## Validation on the original baseline

The runtime regressions passed: 536 tests, one off-host Windows-source-build skip.
The immutable baseline fails 384 defect checks while 21 positive controls pass.
All seven target archive fixtures match the real CMake link inputs; all seven
cache identities are unchanged. The version/changelog gate passed against the
original baseline; the new publication range requires a fresh check.
The existing Engine update CI job now runs this regression set.

An isolated Linux ASAN nested-sync unit passed on unchanged source;
that result does not resolve the separate Windows ASAN timeout or qualify this
runtime-preparation change.

The completed native controller froze the integrated Engine version
`2026.1.16-dev` and project C# inputs through its complete acceptance run.

The complete documentation unittest discovery passed 566 tests with one skip.
Generated models/references, localization, description translations, site/search,
AI evaluation and AI delivery checks passed. Documentation validation passed all
411 Markdown entries. The exact-baseline aggregate reports zero contract changes
across 17 domains and no required dispositions.

The first external-snippet invocation failed because PowerShell was unavailable.
After preparing a private portable parser verified against its official release
checksum, the real external gate passed: 309 normative snippets, 159 evidence
snippets and 183 external-parser checks. The initial failure remains recorded.
The initially missing rendering tools were subsequently prepared in a private
prefix: Ruby 3.3.4, Bundler 2.5.11 and Node 24.16.0. Downloaded tool archives
were verified against official SHA-256 values; no system installation was made.
The github-pages 232 Jekyll render and rendered-artifact audit passed. The artifact
contains no published internal routes and passed 85,527 local-reference checks.
The full browser audit passed all 592 rendered routes in each of desktop,
mobile and 200-percent zoom profiles: 1,776 page checks, 15 interaction checks,
23 screenshots, no errors or unresolved automated findings. The pinned browser
stack was Playwright 1.62.0, Chromium 151.0.7922.34 and axe-core 4.12.1.
The rendered validation controller completed with exit code 0 and unchanged
source hashes. Internal completion notes added after that run do not change
the published pages; their generated AI metadata is checked separately.

The validator also accepts the complete actual Linux runtime used by the current
project native build, without modifying that runtime. This is input-completeness
evidence, not a Windows or cross-target native-runtime qualification.

The verified 20-file candidate was copied into the clean main Engine worktree
at the same published baseline. Previous files were backed up before copying,
all copied hashes matched the isolated candidate, staging remains empty, and
Engine HEAD plus the project gitlink retain the published baseline. The owner
independently published the project's prepared changes as `825b865a`; the obsolete
project controller was interrupted after its completed bake and before gameplay.
The project integration documentation is reconciled. Its corrected cursor fixture
places the pointer before waiting for camera motion to finish, retaining the
original stationary-camera/cache measurement assertions. Full project Content
validation passed 155 of 155 checks with unchanged inputs. The current native
configure, build, two model hit-mask tests (60 assertions), and resource bake
passed. Affected managed gameplay acceptance passed 439 unique cases and
1,555 assertions across two suite partitions, with unchanged staged inputs,
no failures, timeouts, skipped cases or global-exception deltas. The controller
completed with exit code 0, unchanged source hashes and the original project HEAD.

The obsolete native controller completed its build, model tests and bake before
being interrupted with exit code 130 during gameplay input preparation. No game
scenario ran in that interrupted controller. Its complete bake is retained only
as an incremental-build input, and no gameplay outcome is carried forward.

The main Engine index and project index remain empty. Previous files and absent
new-file markers are backed up; safety stashes remain retained. No ordinary
commit or push was performed. The rendered validation proof is
`825-engine-rendered-documentation/validation-proof.json` in the private CI-watch
evidence directory; the current native proof is
`825-camera-bootstrap-native-integration.json` there.

## Current workflow-test reconciliation

On the current published baseline, full documentation discovery ran 569 tests
with one unavailable external-shell-parser skip and one stale-command error. The generated
content guide already uses `docs_prepare.py --external`; the regression still
required the removed manual snippet/localization/site/AI command chain. The
corrected test checks the documented diagram/reference/preparation/validation
order. All six focused build-foundation checks passed on the isolated candidate.
The unchanged earlier current-version render passed all 592 routes across three
profiles (1776 page checks), 15 interactions and 23 screenshots, with zero errors.
That frozen artifact predates these release-note/test updates; repeat current
generation, discovery and rendered/browser gates before publication readiness.

The corrected full documentation discovery passed all 569 tests without skips
with the private portable PowerShell parser available; project link auditing
passed 4494 documents. Current native and Web compilation remain live.

The subsequent current-version render completed with exit code 0 and unchanged
source hashes. The artifact passed 85,531 local-reference checks, contains no
published internal routes, and passed all 592 rendered routes in three browser
profiles: 1,776 page checks, 15 interactions, 23 screenshots and zero errors.
The version gate accepted `2026.1.18-dev`; documentation validation accepted
all 411 Markdown entries. The proof is
`6db-engine18-workflow-fixed-rendered-documentation/validation-proof.json`
in the private CI-watch evidence directory. These internal completion notes
do not change the published pages; regenerate and check AI delivery metadata.

The current native build and bake completed on unchanged program inputs.
Two model hit-mask cases passed 60 assertions; four application/settings/cache
cases passed 307 assertions. The subsequent current all-role bake completed,
and the affected gameplay repeat accepted all 608 unique cases and 4,827
assertions across two suite partitions, without failures, timeouts, skips or
global-exception deltas. Program sources and baked inputs remained unchanged.
Both existing packaged Web startup/exit/replay tests passed on the newly baked
role assemblies. Three routes delayed actual native startup by 6,000 ms with
the original 5,000 ms timer and observed the first normal quit more than
7.28 seconds after the delay. Immutable package hashes remained unchanged.
These are project integration checks, not Windows ASAN or GPU first-render
qualification. No ordinary commit or push was performed. The current native
and Web proofs are `4de-configured-landing-native-validation.json` and
`4de-current-landing-packaged-web/validation-proof.json` in the private evidence
directory. These internal completion notes do not change the published pages;
regenerate and validate their AI delivery metadata before readiness.
