# Managed runtime READY validation

## State

Validated locally. No commit or publication is authorized.

## Baseline and scope

The baseline is Engine `cb33d42c7e4e2678e9b34ccf913d7b88186f979b`
(`2026.1.15-dev`). The candidate was prepared in an isolated worktree and
copied into the main Engine checkout after validation. Engine HEAD and the
project gitlink retain that published baseline. The current embedding-project
baseline is `825b865aa1308c8bcf062b7f14975d21de60d0c3`, published independently
by the owner after validation of `263047b2e200fa8bbeb65a386d2dbd2e4ffb7915` started.
The candidate version is `2026.1.16-dev`, subject to recalculation if the
published master advances.

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

## Plan

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

## Validation

The runtime regressions passed: 536 tests, one off-host Windows-source-build skip.
The immutable baseline fails 384 defect checks while 21 positive controls pass.
All seven target archive fixtures match the real CMake link inputs; all seven
cache identities are unchanged. The version/changelog gate passes against HEAD.
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
