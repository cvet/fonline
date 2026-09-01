# Documentation Verification Report

This report records source-grounded documentation verification passes for the engine docs in this checkout. It is not a replacement for the backlog; it records what was checked and which limitations remain. Dated entries preserve intermediate implementation evidence; when an older entry conflicts with a later reconciliation or the checked-out source, the later evidence and current source are authoritative.

## 2026-09-01 - ZIP recovery reconciliation through `e76c33f502a`

Scope:

- Reconciled the documentation branch after its previous Engine merge
  `764c2e1c8cdce67892757b14e6f1af12de503b84` with `origin/master` through
  `e76c33f502ae89f7b676df0812f674ec05346fe7`.
- Audited the complete one-commit incoming range: deterministic post-write
  validation for disk and embedded resource ZIPs, one shared installed-resource
  overlay for gameplay and updater metadata, splash lookup through that overlay,
  and archive/entry-aware ZIP read diagnostics.
- Preserved the legacy monolithic pages as short compatibility routes and
  reconciled the reusable behavior into canonical English/Russian owners.

Documentation and contract reconciliation:

- Documented that package creation reopens every resource ZIP, verifies the
  exact ordered entry set, and streams every entry through the CRC-checking
  reader before accepting either a disk payload or an embedded pack.
- Documented `GetClientResources(const ClientSettings&)` as the single overlay
  constructor used by the client runtime and updater, including writable-pack
  precedence for metadata and updater-splash lookup.
- Documented contextual ZIP failures: archive and entry identity plus separate
  read and close failures. This improves recovery evidence without turning a
  packaging check into proof of delivery, installation, or successful rollback.
- Updated the Russian BuildTools translation, synchronized all four changed
  page pairs, and regenerated the source inventory, snippet inventory,
  translation status, site/search/route data, deterministic AI evaluation, and
  AI delivery files. All 4,969 generated descriptions and all 197 translation
  pairs are current.

Validation:

- Focused ZIP/package, updater, configuration, runtime, localization, generated
  reference, inventory, site, AI, and documentation validation passed 135 tests
  plus 28 subtests.
- Direct generated checks report 6 package targets, 6 platforms, 19 packs;
  957 export methods, 106 native-test files, and 284 settings; 386 public routes;
  and 397 valid Markdown entries.
- The external Bash parser could not run because this Windows host exposes WSL
  without an installed Linux distribution. The direct snippet validator passed
  310 normative snippets, 159 evidence blocks, and retained 183 external-parser
  checks for Linux CI.
- Deterministic AI evaluation passed 27 tasks and 65 retrieval checks at 100
  percent success and 0.915 MRR.

Disposition:

- The incoming reusable ZIP validation, overlay precedence, and diagnostic
  behavior are represented in canonical Engine docs and generated discovery
  surfaces. No manual Engine source change was needed on the documentation
  branch; the source and native tests come from `origin/master`.
- A consuming project must still rebuild against the reconciled Engine and
  exercise a genuinely damaged installed pack, updater recovery, and packaged
  client/server payloads. Automated unit and documentation evidence does not
  replace those project/release acceptance gates.

## 2026-09-01 - master reconciliation through `df06df50a99`

Scope:

- Reconciled documentation branch head
  `c9fc71b4fa6712d9280830b019b19f847fe14b57` with Engine
  `origin/master` through `df06df50a99e13d7e2284b5f250ace67c3c58ff4`.
- Audited the complete three-commit incoming range: client-package filtering
  for unrequested headless runtimes, engine-owned deque/random/sleep
  primitives with compatibility version `0.0.45`, and platform OS-call
  isolation in the `WinApi` and `Posix` Essentials modules.
- Preserved the legacy monolithic documentation as short compatibility routes
  and reconciled the behavior into the canonical English/Russian owners.

Documentation and contract reconciliation:

- Documented `basic_deque`, `random_generator`, coarse and precise sleep,
  platform-specific OS wrappers, the complete thirty-module Essentials source
  order, and the associated unit-test ownership.
- Documented the Windows/Linux client-runtime variant filter: ordinary client
  packages exclude sibling headless libraries, explicit `Headless` packages
  retain them, and server `PlatformBinaries` remain unchanged.
- Corrected the reusable text-format contract after `TextPack` replaced its
  multimap with sorted vector storage. Load, merge, fallback, serialization,
  and script-visible behavior remain stable; native callers must allow the
  pack to normalize its storage before read-only binary-search access.
- Regenerated the text, audio, and map models and bilingual references,
  source inventory, translation status, snippet inventory, site/search/route
  data, deterministic AI evaluation, AI delivery files, screenshot inventory,
  public API, and example configurations. All 4,969 generated descriptions are
  current.
- The strict comparison against `c9fc71b4f` found one required text-format
  disposition. It is recorded as a breaking native rebuild boundary coordinated
  with compatibility `0.0.45`; no required disposition is missing.

Validation:

- Focused documentation and package coverage passed 55 tests plus 3 subtests;
  the corrected audio suite passed all 11 tests and the regenerated screenshot
  inventory passed all 9 tests.
- After regenerating the screenshot inventory, documentation discovery passed
  612 tests and 133 subtests with one expected skip. The external-parser fixture
  was deselected because this Windows host exposes WSL without an installed
  Linux distribution; the direct snippet validator independently passed 310
  normative snippets, 159 evidence blocks, and 183 external-parser checks.
- Deterministic AI evaluation passed 27 tasks and 65 retrieval checks at 100
  percent success and 0.915 MRR. The strict contract-diff gate reports zero
  missing dispositions.
- Last Frontier rebuilt `LF_UnitTests` against the reconciled Engine after its
  dialog baker was adapted to the mutable normalization boundary. All 581 test
  cases and 706,941 assertions passed.

Disposition:

- The incoming reusable behavior is represented in canonical Engine docs,
  tests, and generated contract surfaces. The only Last Frontier source change
  required by the Engine update is the narrow dialog-text integration adapter;
  it belongs to the project repository and is not included in this Engine
  branch.
- Real ZIP/MSI payload inspection and visible client/updater acceptance remain
  the project/release gates recorded by the consuming repository. Automated
  documentation, package-unit, and native-unit evidence does not replace those
  gates.

## 2026-08-31 - follow-up master reconciliation through `bd344844382`

Scope:

- Reconciled documentation branch head `4b7b350ad` with Engine
  `origin/master` through `bd3448443823cc0a9d9de1481e309c9455c1a9da`.
- Audited the complete eighteen-commit incoming range: restored property
  `Min`/`Max` clamping, engine-owned string and function-object types,
  `Test_DumpArtifacts`, Mapper interface hiding and scroll clamping, detached
  static items without ids, single-threaded server logic, map multihex lines,
  per-animation model bounds, and entity collection conversion fixes.
- Reconciled reusable contracts into the canonical English/Russian owners and
  regenerated their structured, site, route, search, and AI-delivery surfaces.

Documentation and contract reconciliation:

- Documented property-range enforcement and the conversion boundary for
  scalar and collection values, including the corrected entity fixtures.
- Documented `fo::string` inline-capacity ownership, project native-extension
  interop, engine-owned move-only/copyable callables, and their selection and
  lifetime rules.
- Documented dump-artifact directory ownership, Mapper `F7` interface hiding,
  the corrected scroll clamp, static-item empty-id handling,
  `Server.SingleThreadedLogic`, runtime multihex-line footprints, and the
  versioned per-animation model-bounds payload.
- Regenerated 2,501 native API entries and 4,969 translated generated
  descriptions. The strict comparison against `4b7b350ad` found five API
  changes; all three required dispositions are present and none is missing.
- The project-baked catalog exposed a stale documentation decoder constant:
  runtime metadata layout version 3 was rejected as version 2 even though the
  decoder already understood the mandatory remote-call `Limits` trailer. The
  decoder and its mismatch test now track version 3, and the paired Last
  Frontier server/client catalog generates and checks successfully with 400
  calls.

Validation:

- `docs_validate.py` passed all 397 Markdown entries. Complete documentation
  discovery passed all 548 tests after the first pass exposed and the
  reconciliation corrected four stale inventory/count expectations.
- External snippet validation passed 310 normative snippets, 159 evidence
  blocks, and 183 external-parser checks. Deterministic AI evaluation passed
  27 tasks and 65 retrieval checks at 100 percent success and 0.915 MRR.
- The focused project-catalog suite passed all 6 tests. Last Frontier rebuilt
  `LF_UnitTests`; a first randomized run reported one failing test, while the
  recorded rerun passed all 578 test cases and 446,544 assertions.
- Last Frontier completed a forced full bake in 3 minutes 51 seconds with zero
  dialog-composition errors, then completed an incremental bake with zero
  files rebuilt in every pack.

Disposition:

- The reusable incoming range and the layout-3 project-catalog handoff are
  represented in canonical documentation, tests, and generated artifacts.
- Mapper interface/scroll behavior, animation fitting, runtime multihex
  placement, and other visible behavior still require their normal human or
  project gameplay acceptance; automated documentation and bake evidence does
  not replace those gates.

## 2026-08-30 - follow-up master reconciliation through `7c5d32b973`

Scope:

- Reconciled documentation branch head
  `de60506dc800be9d8b5e8fb723ec896dde629044` with Engine
  `origin/master` through `7c5d32b973d392cbc6903128627f8093acfb2e88`.
- Audited the complete four-commit incoming range: bounded cache reads and
  checked writes, explicit browser-client quit, bounded inbound networking and
  remote-call payloads, and the follow-up source-compatibility adjustment to
  `SetDataChecked`.
- Preserved the legacy monolithic documentation as short compatibility routes
  and reconciled incoming behavior into the canonical English/Russian pages.

Documentation and contract reconciliation:

- Documented bounded cache-read statuses, checked cache writes, and the disk
  backend's bounded-read behavior.
- Documented explicit Web shutdown and next-frame main-loop cancellation, while
  keeping browser tab close as best-effort host cleanup rather than a portable
  application callback.
- Documented frame-scoped reads, exact frame consumption, the 4 MiB default
  `ServerNetwork.MaxBufferedInputSize`, the 1 MiB default
  `ServerNetwork.MaxRemoteCallPayloadSize`, and worker-owned disconnect after a
  latched overflow.
- Documented per-call `MaxBytes` and `MaxCollectionSize` source options, the
  mandatory baked `Limits` trailer, limit composition, pre-allocation checks,
  and nested collection enforcement. Updated the project metadata decoder and
  generated-catalog schema to expose those limits.
- Regenerated 2,500 native API entries and the dependent bilingual references,
  inventory, screenshot, snippet, site/search, route, AI-evaluation, and
  AI-delivery artifacts. All 4,965 generated descriptions are current.
- The strict comparison against the pre-merge documentation contract records
  four API changes across eighteen domains. Both required dispositions are
  present: compatibility `0.0.43` and the two new networking settings.

Validation:

- `docs_validate.py` passed all 397 Markdown entries. The complete documentation
  discovery suite passed all 548 tests after correcting three stale
  count/legacy-route expectations found by its first pass.
- External snippet validation passed 310 normative snippets, 159 evidence
  blocks, and 183 external-parser checks. Deterministic AI evaluation passed 27
  tasks and 65 retrieval checks at 100 percent success and 0.915 MRR.
- The strict contract-diff gate reports zero missing dispositions.
- Last Frontier integration built `LF_UnitTests`; the complete native suite
  passed 571 test cases and 445,645 assertions.

Disposition:

- The incoming reusable behavior is represented in canonical Engine docs and
  generated contract surfaces. Non-documentation edits on the docs branch are
  limited to source-owned API inventory pins, metadata decoding, and their test
  expectations; no project-specific runtime feature was added.
- The final Last Frontier bake and generated project remote-call catalog belong
  to the subsequent project `origin/main` merge, because that incoming project
  range supplies the two required server settings. Browser-visible quit
  behavior remains a manual/CI acceptance boundary.

## 2026-08-30 - master reconciliation through `d697d6e1da`

Scope:

- Reconciled documentation branch head
  `0cee27133fcb5744c77b7fd53c7e7520e99de8cb` with Engine
  `origin/master` through `d697d6e1dab20b6c1700ffa370f84ece49d80d8c`.
- Audited the complete fifteen-commit incoming range after
  `8235f89d62d7ce1de66ba82491b274c5a47b27a1`, including early game-setting
  bootstrap, model-info baking performance and bounds policy, metadata layout
  version 2, script Entity promotion, link lifetime protection, detached item
  ownership, and the 32-bit rpmalloc span correction.
- Resolved eight conflicts in legacy monolithic documentation by retaining the
  short compatibility routes and reconciling the incoming behavior into the
  canonical English/Russian owning pages.

Documentation and contract reconciliation:

- Documented `Baking.BootstrapGameSettings`, its declared-name and fixed-budget
  validation, full per-subconfig serialization, early-startup use, and the fact
  that listed secret values become part of every baked binary configuration.
- Documented 60 Hz model-bound sampling, reusable animation sampling state,
  precise per-vertex versus conservative per-bone-envelope modes, and the
  shipped-build quality boundary. Reconciled the removal of `PoseRect` with the
  public `DrawRect`/`ViewRect` fitting contract.
- Documented metadata layout version 2, scalar/array/dictionary script Entity
  promotion and mismatch handling, detached items entering client views with
  `ItemOwnership::Nowhere`, and the shared detached-item receive path.
- Documented per-link `fo::atomic_mutex` protection and widened owning handles
  during server synchronization, plus the 16 MiB 32-bit and 256 MiB 64-bit
  rpmalloc span policies.
- Updated the project metadata decoder for layout version 2. Regenerated 2,498
  native API entries and all dependent bilingual reference, inventory,
  screenshot, snippet, site/search, route, AI-evaluation, and AI-delivery
  artifacts. All 4,963 generated descriptions and all 197 English/Russian page
  pairs are current; the route model contains 386 routes.
- The strict contract comparison records five API changes across eighteen
  domains. All three required breaking-change dispositions are owner-reviewed,
  with zero missing: the strict model inventory, compatibility `0.0.42`, and
  the three-argument `Game.GetDrawCritter3dBounds` signature.

Validation:

- `docs_validate.py` passed all 397 Markdown entries. The focused documentation
  suite passed 76 tests.
- External snippet validation passed 310 normative snippets, 159 evidence
  blocks, and 183 external-parser checks. Deterministic AI evaluation passed 27
  tasks and 65 retrieval checks at 100 percent success and 0.915 MRR.
- Last Frontier integration configured successfully and built `LF_UnitTests`.
  The complete native suite passed 570 test cases and 445,609 assertions.

Disposition:

- The incoming reusable behavior is represented in the canonical Engine docs
  and generated contract surfaces. Post-merge edits outside documentation are
  limited to source-owned API contract pins, the metadata-layout decoder, and
  their regression expectations; no project-specific runtime feature was added
  to Engine.
- A full Last Frontier bake and project-specific documentation reconciliation
  remain tied to the subsequent project `origin/main` merge. Visible-client
  inspection of model framing was not performed here and remains a human/CI
  acceptance boundary.

## 2026-08-28 - master reconciliation through `8235f89d62`

Scope:

- Reconciled documentation branch head
  `e0d3fb7543915302d2c298afe358a48961a10e47` with Engine `origin/master`
  through `8235f89d62d7ce1de66ba82491b274c5a47b27a1`.
- Audited the complete thirteen-commit incoming range, including configured
  language fallbacks, deterministic prototype inheritance, metadata-backed
  setting updates, blocked-hex and pathfinding corrections, model bounds and
  link geometry, map-item hit testing, and the compatibility migration through
  `0.0.40`.

Documentation and contract reconciliation:

- Documented `Baking.BakeLanguages` entries as either `language` or
  `child:parent`, with unique identifiers, parent-before-child ordering, and
  explicit fallback lookup. Documented deterministic left-to-right prototype
  parent merging, cycle rejection, and
  `Baking.AllowRepeatedProtoParents`.
- Reconciled configuration documentation with the fixed 10,000-entry internal
  store, removal of `FO_INTERNAL_CONFIG_CAPACITY`, root-config baselines versus
  subconfig deltas, and correct false/empty setting propagation. Removed the
  obsolete `-internalcfg` helper-CLI surface from generated references.
- Updated the reusable movement, rendering, model-format, metadata, server, and
  migration references for blocked-hex correction, grid-size clamping,
  root-space model bounds, geometry links, active-sprite item hit testing, and
  the current compatibility rules.
- Regenerated 2,496 API entries, 43 CMake entries, 14 prototype-format entries,
  and all dependent bilingual reference/site/route/AI artifacts. All 4,961
  generated descriptions and 197 English/Russian page pairs are current; the
  route model contains 386 routes.
- The strict contract comparison records thirteen changes across eighteen
  domains. All six required dispositions are owner-reviewed, with zero missing.

Validation:

- `docs_validate.py` passed all 397 Markdown entries. Focused documentation
  unittests passed 65 tests; the focused pytest set passed 19 tests and 48
  subtests.
- External snippet validation passed 310 normative snippets, 159 evidence
  blocks, and 183 external-parser checks. Deterministic AI evaluation passed 27
  tasks and 65 retrieval checks at 100 percent success and 0.915 MRR.
- Last Frontier integration configured successfully, built `LF_UnitTests`, and
  passed the complete native suite: 566 test cases and 445,437 assertions.
  `BakeResources` then completed all project packages in 8 minutes 57 seconds;
  dialog composition reported zero errors.

Disposition:

- The complete incoming behavior is reflected in reusable Engine documentation
  and the generated contract surfaces. No project-specific implementation was
  added to the Engine documentation branch.
- Live visual inspection of model framing and sprite hit testing was not
  performed in this reconciliation. Native tests and full project baking are
  green; platform CI and a visible-client pass remain the visual acceptance
  boundary.

## 2026-08-25 - master reconciliation through `5440adeeec`

Scope:

- Reconciled documentation branch head
  `04cf9f9c8b616e6e2cd5fd915a7efdb017c39be1` with Engine `origin/master`
  through `5440adeeec07eb16d5cb0e2ed6eb1f32e71c482b`.
- Audited the complete three-commit incoming range: the third-party/toolchain
  refresh, richer AngelScript entity error context, and client map-light plus
  GUI hit-test caching optimizations.
- Reviewed all incoming non-vendored BuildTools, source, test, and legacy-doc
  changes. Vendored dependency revisions remain owned by their upstream
  snapshots and the canonical third-party inventory.

Documentation and contract reconciliation:

- Updated the bilingual Android workflow for command-line tools `15859902`,
  `android sdk install`, build-tools 36.0.0, Gradle 9.5.0, Android Gradle
  Plugin 9.3.0, and Java 17; updated the Web workflow for Emscripten 6.0.8.
- Documented sanitizer stack-reporting limits, expanded entity exception
  context, map-light translation during scrolling, exact hex-offset geometry,
  and the per-frame GUI `CheckHit` cache with its explicit
  `InvalidateHitCache` escape hatch.
- Recorded the exact third-party snapshot revision/date for the AngelScript WIP
  fork and reconciled the BuildTools English/Russian entry point.
- Regenerated the native API and 21 API reference pages. The aggregate contract
  comparison against the pre-merge documentation head records nine changes
  across eighteen domains: five ImGui enum-value renumberings, one description
  update, the GUI model digest/count change, and two additive GUI cache entries.
  All six required dispositions are owner-reviewed with zero missing.
- Completed all 4,953 generated-description translations and refreshed the
  bilingual localization, snippets, site/search, route, deterministic AI, and
  AI-delivery artifacts in dependency order.

Validation:

- The first focused pass exposed five stale upstream expectations; after
  reconciling Android retry arguments, Android pins, GUI counts, and translation
  counts, the focused documentation set passed 113 tests and 37 subtests.
- The first complete discovery passed 606 tests with one skip and 129 subtests,
  and exposed three additional stale expectations: the description total, the
  renamed portable-script CMake expression, and the Emscripten pin. Their
  corrected focused set passes 10/10.
- The final complete discovery on the regenerated tree passes 609 tests with
  one skip and 129 subtests in 892 seconds.
- External snippet parsing passes 310/310 normative snippets, 159 evidence
  blocks, and 183 Bash/PowerShell checks. Deterministic AI evaluation passes 27
  tasks and 65 retrieval checks at 100 percent success and 0.915 MRR.

Disposition:

- The incoming source behavior is documented without introducing a new Engine
  runtime feature on the documentation branch. The only post-merge BuildTools
  source edit corrects an obsolete `sdkmanager` comment; the remaining
  reconciliation changes are documentation models, generated artifacts, and
  regression expectations.
- Native compilation and platform packaging are left to the current-head CI
  matrix because this checkout reconciliation does not modify the incoming C++
  implementation or vendored payloads.

## 2026-08-24 - docs-only contract and clean-checkout validation reconciliation

Scope:

- Audited documentation branch head `a2dc6cc427106d4d12e47f2db193c7bd91792962`
  after its additive merge of `origin/master` through
  `4a898d59e642fd437fe4b7b277db7d27d31e8e94`.
- Reproduced the shared CI failure from clean-checkout logs: the branch deleted
  `BuildTools/validation-project`, while `buildtools.py` still copies that
  fixture for unit, sanitizer, coverage, and platform validation. The local
  checkout had not exposed the deletion before publication.
- Re-audited project CMake, package, and native-extension documentation after
  the earlier removal of runtime changes from this documentation branch.

Results:

- Restored the three-file validation project exactly from current master, so
  clean checkouts retain the source fixture required by all validation lanes.
- Kept `ProjectInterface.json`, `PackageInterface.json`, and
  `NativeExtensionInterface.json` as documentation/validation models. Current
  CMake and packager/codegen implementations remain runtime authority; no
  manifest loader or runtime role rejection was reintroduced.
- Reworked the structural project/native-extension CMake tests to read the
  models directly and compare them with current declarations, routing, source
  paths, helper availability, role consumers, and odd-argument diagnostics.
- Corrected generated and human EN/RU claims: `AddBakingTarget` is defined in
  `ScriptsAndBaking.cmake`; arbitrary role tokens create an unconsumed
  `FO_<ROLE>_SOURCE` list; package/project/native manifests are not runtime
  inputs. Historical entries below now identify superseded intermediate
  implementations explicitly.
- Normalized Bash snippet-parser stdin to LF bytes on Windows and added a
  subshell regression, eliminating the local CRLF-only false diagnostic.

Validation:

- All three structural CMake scripts passed.
- Focused CMake/native-extension/package documentation tests passed 22/22.
- Complete `BuildTools/tests` discovery passed 570/570 in 477 seconds before
  the final snippet-transport-only correction; its focused six-test suite then
  passed on the corrected implementation.
- External parsing passed all 310 normative snippets, 159 evidence blocks, and
  183 Bash/PowerShell checks. Standalone documentation validation passed all
  397 maintained Markdown entries.
- Replacement GitHub Actions evidence remains attached to the pull request and
  must pass on the published reconciliation commit before merge.

## 2026-08-20 - documentation branch and master reconciliation through `b4bf39a42`

Scope:

- Engine documentation head `917e6f752e30000bcf4f149d0c49269de74a33cd`
  reconciled with published `origin/master`
  `b4bf39a420ed1d8d785c8ca8bb51f69ba97eb66d`. The first incoming range
  through `0a38e0e5dcbbdf78b70bacda3438d0407fcf17e3` is preserved by merge commit
  `66a711a18b36ab6ce52af476a7ea8bffc2d16653`; the metadata-version follow-up
  is prepared as a second merge.
- Complete incoming range
  `439fb9231522bc51acfe100772cc954c9d3f0c2c..b4bf39a420ed1d8d785c8ca8bb51f69ba97eb66d`
  (65 commits), including 33 preflight conflict paths and the follow-up
  metadata-version guard.
- Reusable API, build, configuration, content-format, GUI, Mapper, scripting,
  testing, release, localization, site, and AI-delivery documentation plus
  source-owned generators and focused regressions.

Results:

- Resolved the merge source-first with no unmerged paths. The safety ref
  `backup/docs-pre-update-20260820` preserves the pre-merge documentation head.
- Regenerated 21 API reference pages from 2,494 explicitly classified symbols;
  all 4,947 generated descriptions have current Russian translations.
- Regenerated the bilingual site and route artifacts: 112 navigation entries,
  197 English and 197 Russian searchable documents, 386 public routes, and 188
  planned redirects.
- Regenerated snippets and AI delivery: 310 normative snippets, 159 evidence
  blocks, 183 external-parser checks, 27 retrieval tasks, 65 checks at 100
  percent within threshold and 0.900 MRR, and 386 delivered documents.
- Replaced the Mapper particle documentation captures with reproducible D3D11
  1280x800 screenshots. Their SHA-256 values are
  `75cb718080bcd3ab657d1885cdb571b39df6680a000e0947e2be8229f0c045e0`
  and `61292f3d05fd652235a6afad0e14b2c862d6bb879319a2379d2073140d89a006`.
- Compared the reconciled models with the safety branch: the API report records
  363 changes and 18 required dispositions; the aggregate report records 372
  changes across 18 domains and 23 required dispositions. Both strict checks
  pass with zero missing dispositions. Bootstrap comparison with the published
  master, which predates these generated models, also passes with zero changes.
- Added backward-compatible comparison for the legacy prototype-format model
  that predates the explicit property-serializer source field, with regression
  coverage.
- Fixed the on-demand baker's maximum-`uint64` rebuild sentinel and covered it
  with three focused Baker test cases (66 assertions).
- Made the Minimal Multiplayer fixture's distance-based visibility contract
  explicit through a public `CheckCritterVisibilityHook`, and made generic
  client/mapper API coverage tolerate the documented disabled-3D exception in
  the fixture's `FO_ENABLE_3D=OFF` profile. The corrected event descriptions
  now state that Engine fires the three appearance and disappearance tiers.
- Regenerated the Packaging Matrix full config after the incoming settings
  additions exposed its stale checked-in output.
- Reconciled the metadata file header, persistent-property restore,
  client/server handshake, updater response, and `Network.ForceMetadataVersion`
  setting with the reusable metadata and updater documentation. The canonical
  English and Russian references own the new contract while the flat legacy
  routes retain their stable headings.
- Fixed the tutorial metadata decoder exposed by the first package run. The
  shared Python decoder now validates the metadata magic, file-format version,
  non-empty metadata version, and section framing; the tutorial runner delegates
  to it instead of retaining an obsolete headerless parser. Focused tests cover
  invalid magic, invalid file version, empty metadata version, and truncation.

Validation:

- Complete documentation discovery passed 547 tests after the metadata-version
  reconciliation and parser correction.
- Standalone documentation validation passed all 397 maintained Markdown
  entries; generated API, descriptions, references, CLI, snippets, site,
  localization, AI evaluation, AI delivery, and screenshots pass current-state
  checks.
- `git diff --check` and `git diff --cached --check` pass; CRLF conversion
  notices are informational and no whitespace errors were reported.
- `FOMM_UnitTests` built in `RelWithDebInfo`; the focused Baker regression passed
  three cases and 66 assertions. The five initially exposed profile-portability
  cases then passed 816 assertions, and the complete native suite passed all
  424,519 assertions in 423 test cases. `ForceBakeResources` and `FOMM_Mapper`
  also built successfully, and the Mapper captures were inspected from the real
  client render path.
- `win64-tutorial-package` passed both unpackaged scenarios, the packaged
  server/client scenario, archive checks, metadata-version agreement, updater
  readiness, and manifest generation after the decoder correction.
  `win64-package-smoke` passed baking, raw/ZIP/headless/service payload checks,
  packaged client/server execution, and its packaging manifest after the stale
  generated config was refreshed.

Limitations and handoff:

- The local Ruby environment has no `bundle` executable, so the production
  Jekyll render and browser artifact gate were not rerun. Structural site tests
  and generated-site checks pass; CI remains authoritative for the pinned Ruby
  render.
- The reconciled changes remain local until their merge commit is created.
  Publication and the dependent Last Frontier merge require explicit owner
  authorization; no push is implied by this validation record.

## 2026-08-08 - model-frame placement and early time-event dispatch

Scope:

- `Docs/en/explanation/rendering/index.md` and its Russian mirror
- `Docs/en/explanation/runtime/client.md` and its Russian mirror
- `Docs/en/explanation/entity-and-property-model/index.md` and its Russian mirror

Source areas checked:

- `Source/Client/ModelSpriteLayout.*` and `Source/Client/ModelSprites.cpp` for current/required frame placement merging, signed pivots, retry bounds, and atlas publication.
- `Source/Common/TimeEvents.cpp` for the engine-frame-clock deadline check in `TimeEventManager::FireAndAdvance()`.
- `Source/Tests/Test_ModelSpriteLayout.cpp` and `Source/Tests/Test_CommonScriptMethods.cpp` for the incoming regression cases.

Results:

- Documented the root-relative interval union that prevents adjacent pixel-rounded model pivots from alternating indefinitely and accepts a tight frame whose root pivot lies outside the frame.
- Documented that an external dispatcher waking before the engine clock reaches `FireTime` receives the remaining delay without firing the callback.
- Audited the complete incoming Engine and Last Frontier ranges recorded in `Docs/ProductionDocumentationPlan.md`; the Last Frontier MCP, dialog-content, and synchronization-test changes remain project-owned.

Validation:

- Regenerated and checked localization, site, AI-evaluation, and AI-delivery artifacts after the bilingual edits: 197/197 translation pairs, 386 public routes, and 65/65 AI retrieval checks at 0.908 MRR.
- Seven focused documentation modules passed 47 tests.
- `LF_UnitTests` built in `RelWithDebInfo`; `ModelSpriteFramePlacementMerge*` passed 22 assertions in two test cases and `GameLevelTimeEvents` passed 68 assertions in one test case.

CI follow-up:

- The first two current-head validation runs rejected a stale
  `Docs/generated/snippets.json` corpus hash. Regenerating snippets before the
  dependent site, evaluation, and AI-delivery outputs changed only that hash
  and its projection in `docs-manifest.json`; the checked corpus remains 307
  normative snippets, 157 evidence blocks, and 182 external-parser checks.
- One duplicated `win64-package-smoke` job completed while the other stopped
  during an asynchronous compressed server write after the packaged client had
  already passed and unloaded. A clean local rerun of
  `BuildTools\validate.cmd win64-package-smoke` at the same source revision
  passed configure, build, packaging, updater synchronization, packaged
  client/server execution, DLL unload, and graceful server shutdown. No runtime
  contract or documentation correction is inferred from the single
  non-reproduced job failure; the refreshed CI head remains authoritative.

## 2026-05-18 — source-tree and runtime model slice

Scope:

- `Docs/SourceTree.md`
- `Docs/EntityModel.md`
- `Docs/MapsMovementGeometry.md`
- `Docs/Persistence.md`
- `Docs/DocumentationBacklog.md`

Source areas checked:

- `Source/Applications/`, `Source/Client/`, `Source/Common/`, `Source/Server/`, `Source/Scripting/`, `Source/Tools/`, `Source/Frontend/`, `Source/Essentials/`, and `Source/Tests/` for the source-tree routing page.
- `Source/Common/Entity.*`, `EntityProperties.*`, `EntityProtos.*`, `Properties.*`, `PropertiesSerializer.*`, and `ProtoManager.*` for entity/property/prototype claims.
- `Source/Common/Geometry.*`, `LineTracer.*`, `Movement.*`, `PathFinding.*`, `MapLoader.*`, and `Source/Tools/MapBaker.*` for map/movement/geometry claims.
- `Source/Server/DataBase.*`, `Source/Server/DataBase-*.cpp`, and `Source/Tests/Test_DataBase.cpp` for persistence claims.

Results:

- Backticked source/build/doc path checks for this slice: no remaining missing paths after replacing the stale Docs/Testing.md future route in `SourceTree.md` and linking runtime map behavior to the now-present `ServerRuntime.md` / `ClientRuntime.md` pages.
- Symbol spot checks found the documented owners in current source: `EntityTypeDesc`, `FO_ENTITY_PROPERTY`, `ProtoEntity`, `PropertyRegistrator`, `GeometryHelper`, `FindPathInput`, `MovingContext`, `MapLoader`, `DataBaseImpl`, `RecoveryLogHandle`, and `CommitNextChange`.
- Current test inventory observed in this checkout: 79 `Source/Tests/Test_*.cpp` files.
- Promoted in `Docs/DocumentationBacklog.md`: `SourceTree.md`, `EntityModel.md`, `MapsMovementGeometry.md`, and `Persistence.md` from `drafted` to `verified`.

Follow-up:

- Completed by the later 2026-05-18 client/frontend runtime slice below; continue next with `Docs/Networking.md` and `Docs/ServerRuntime.md`.
- Docs/Testing.md, Docs/Essentials.md, Docs/ConfigurationAndDataSources.md, and Docs/DocumentationMaintenance.md are still planned in this checkout unless created in a later pass.

## 2026-05-18 — client/frontend runtime slice

Scope:

- `Docs/ClientRuntime.md`
- `Docs/FrontendAndRendering.md`
- `Docs/DocumentationBacklog.md`

Source areas checked:

- `Source/Client/Client.*`, `Source/Client/ClientConnection.*`, `Source/Client/ResourceManager.*`, `Source/Client/MapView.*`, client entity/view classes, sprite factories, `Source/Client/RenderTarget.*`, `Source/Client/SpriteManager.*`, and `Source/Client/EffectManager.*` for client runtime composition, network dispatch, view-entity ownership, resources, sprites, effects, input mapping, and render-target claims.
- `Source/Frontend/Application*.cpp`, `Source/Frontend/Application.h`, `Source/Frontend/Rendering*.cpp`, `Source/Frontend/Rendering.h`, and `BuildTools/cmake/stages/Packages.cmake` for application services, headless/stub flows, renderer backends, package-boundary claims, and platform/frontend ownership.
- `Source/Tests/Test_ClientEngine.cpp`, `Source/Tests/Test_ClientServerIntegration.cpp`, `Source/Tests/Test_ClientDataValidation.cpp`, `Source/Tests/Test_ClientRuntimeApi.cpp`, and `Source/Tests/Test_Rendering.cpp` for the current engine-local validation surfaces named by the promoted pages.

Results:

- Backticked source/build/doc path checks for this slice: no missing paths after adding `Source/Tests/Test_Rendering.cpp` to the frontend/rendering page's inspected sources.
- Symbol spot checks found the documented owners and APIs in current source, including `ClientEngine`, `ClientConnection`, `GetClientResources`, `CreateNetworkConnection`, `TryFallbackToTcp`, client `Net_On...` handlers, `MapView`, client view classes, sprite/effect/render-target managers, `Application`, `AppWindow`, `AppInput`, `AppAudio`, `Renderer`, `RenderEffect`, `Null_Renderer`, `OpenGL_Renderer`, `Direct3D_Renderer`, and `IsRenderTargetFlipped`.
- Current focused test inventory observed for this slice: four `Source/Tests/Test_Client*.cpp` files plus `Source/Tests/Test_Rendering.cpp`.
- Promoted in `Docs/DocumentationBacklog.md`: `ClientRuntime.md` and `FrontendAndRendering.md` from `drafted` to `verified`.

Follow-up:

- Completed by the later 2026-05-18 networking/server runtime slice below; continue next with `Docs/ClientUpdater.md` and updater boundary checks.

## 2026-05-18 — networking/server runtime slice

Scope:

- `Docs/Networking.md`
- `Docs/ServerRuntime.md`
- `Docs/DocumentationBacklog.md`

Source areas checked:

- `Source/Common/NetBuffer.*` and `Source/Common/NetworkUdp.*` for message framing, hash/debug-hash serialization, and ordered UDP behavior.
- `Source/Client/NetworkClient*` and `Source/Server/NetworkServer*` for transport-neutral client/server abstractions plus interthread, socket, UDP, ASIO, and WebSocket implementations.
- `Source/Server/Server.*`, `Source/Server/EntityManager.*`, `MapManager.*`, `CritterManager.*`, `ItemManager.*`, `Player.*`, `Critter.*`, `Map.*`, `Location.*`, `Item.*`, `ClientDataValidation.*`, and `UpdaterBackend.*` for authoritative server runtime ownership, entity/session flow, validation, managers, movement, persistence handoff, and updater hosting.
- `Source/Tests/Test_NetworkUdp.cpp`, `Source/Tests/Test_NetworkClient.cpp`, `Source/Tests/Test_NetworkServer.cpp`, `Source/Tests/Test_ServerEngine.cpp`, `Source/Tests/Test_ServerItems.cpp`, `Source/Tests/Test_ServerMapOperations.cpp`, `Source/Tests/Test_ServerAdvancedOps.cpp`, `Source/Tests/Test_ServerScriptMethods.cpp`, `Source/Tests/Test_ClientServerIntegration.cpp`, `Source/Tests/Test_ClientDataValidation.cpp`, and `Source/Tests/Test_DataBase.cpp` for the current validation surfaces named by the promoted pages.

Results:

- Added a `Source paths inspected` section to `Docs/Networking.md` and replaced its generic integration-test wording with the concrete current `Source/Tests/Test_ClientServerIntegration.cpp` reference.
- Backticked source/build/doc path checks for this slice: no missing paths.
- Symbol spot checks found the documented owners and APIs in current source, including `NetBuffer`, `NetOutBuffer`, `NetInBuffer`, `NetworkClientConnection`, `NetworkServerConnection`, `NetworkServer`, `UdpOrderedChannel`, UDP packet helpers, `ServerEngine`, server init/job methods, server events, `EntityManager`, `MapManager`, `CritterManager`, `ItemManager`, `Player`, inbound `Process_*` handlers, client-data validation functions, movement helpers, and `UpdaterBackend`.
- Current focused test inventory observed for this slice: three `Source/Tests/Test_Network*.cpp` files, five `Source/Tests/Test_Server*.cpp` files, plus `Source/Tests/Test_ClientServerIntegration.cpp`, `Source/Tests/Test_ClientDataValidation.cpp`, and `Source/Tests/Test_DataBase.cpp`.
- Promoted in `Docs/DocumentationBacklog.md`: `Networking.md` and `ServerRuntime.md` from `drafted` to `verified`.

Follow-up:

- Completed by the later 2026-05-18 client updater slice below; continue next with platform debugging docs: `Docs/WebDebugging.md`, `Docs/AndroidDebugging.md`, and `Docs/Debugging.md`.

## 2026-05-18 — client updater/runtime split slice

Scope:

- `Docs/ClientUpdater.md`
- updater-boundary references in `Docs/ServerRuntime.md` and `Docs/ClientRuntime.md`
- `Docs/DocumentationBacklog.md`

Source areas checked:

- `Source/Applications/ClientApp.cpp` and `Source/Applications/ClientLib.cpp` for host/runtime loading, fallback, staged binary promotion, reload results, and platform gating.
- `Source/Client/ClientRuntimeApi.*`, `Source/Client/Updater.*`, `Source/Frontend/ApplicationInit.cpp`, `Source/Essentials/DiskFileSystem.*`, `Source/Essentials/Platform.*`, `Source/Common/Common.h`, and `Source/Common/Settings.inc` for runtime ABI, updater protocol versioning, update targets, installed-client writable paths, disk hashing/cache behavior, and updater settings.
- `Source/Server/UpdaterBackend.*` and `Source/Server/Server.cpp` for server-side descriptor generation, file serving, target-specific binaries, and `UpdateFileMaxPortionSize` use.
- `BuildTools/cmake/stages/Applications.cmake`, `BuildTools/package.py`, and `BuildTools/msicreator/createmsi.py` for client host/library build gates, runtime binary packaging/staging, and Windows MSI installer metadata.
- `Source/Tests/Test_ClientRuntimeApi.cpp` for runtime ABI coverage. Embedding-project updater pipeline tests are project-owned supplemental checks and are not engine documentation dependencies.

Results:

- Added a `Source paths inspected` section to `Docs/ClientUpdater.md`.
- Replaced obsolete package-entry wording with the current `build_runtime_update_target_name` owner in `BuildTools/package.py`.
- Backticked source/build/doc path checks for this slice: no missing paths.
- Symbol spot checks found the documented owners and APIs in current source, including `UpdaterBackend`, `LoadFromClientResources`, `ProcessUpdateFile`, `GetUpdateDescriptor`, `FO_CLIENT_RUNTIME_HOST_ABI_VERSION`, `ClientRuntimeMetadata`, `ClientRuntimeExports`, `ClientRuntimeResult`, `FO_QueryClientRuntimeExports`, `ApplyStagedBinaryUpdate`, `GetClientRuntimeLivePath`, `MakeClientRuntimeStagingPath`, `RunClientFromLibrary`, `RunEmbeddedOrLoadedClient`, `ResolveRequestedClientRuntime`, `ResolveUserWritablePath`, `fs_make_writable_path`, `Platform::GetUserDataBase`, `CanSelfUpdateNativeModules`, `FO_UPDATER_VERSION`, `UpdateFileTarget`, `ClientBinaries`, `ClientResources`, `GetCurrentBinaryUpdateTargetName`, `UpdateFileMaxPortionSize`, `UpdateFilesInMemory`, `PlatformBinaries`, and `build_runtime_update_target_name`.
- Promoted in `Docs/DocumentationBacklog.md`: `ClientUpdater.md` from `drafted` to `verified`.

Follow-up:

- Completed by the later 2026-05-18 platform debugging slice below; continue next with `Docs/Architecture.md` and `Docs/Applications.md`.

## 2026-05-18 — platform debugging slice

Scope:

- `Docs/WebDebugging.md`
- `Docs/AndroidDebugging.md`
- `Docs/Debugging.md`
- `Docs/DocumentationBacklog.md`

Source areas checked:

- `BuildTools/buildtools.py`, `BuildTools/prepare-workspace.sh`, `BuildTools/prepare-win-workspace.ps1`, `ThirdParty/emscripten`, parent `.vscode/tasks.json`, parent `.vscode/launch.json`, parent `CMakePresets.json`, `LastFrontier.fomain`, `Scripts/Scenes.fos`, and `Scripts/GameState.fos` for web build/package/launch and remote-scene debugging flow.
- `BuildTools/android_device.py`, `BuildTools/package.py`, `BuildTools/android-project/`, `FOnlineActivity.java`, Android SDK/NDK pins, parent VS Code tasks, package definitions, CI workflow, and project config for Android package/install/launch/resource-copy behavior.
- `BuildTools/natvis/`, `BuildTools/cmake/stages/Finalize.cmake`, `BuildTools/cmake/helpers/Build.cmake`, `Source/Essentials/StackTrace.*`, `BaseLogging.*`, `ExceptionHandling.*`, `Source/Scripting/AngelScript/AngelScriptContext.cpp`, `Source/Frontend/ApplicationInit.cpp`, `Source/Tests/Test_StackTrace.cpp`, and `Source/Tests/Test_ExceptionHandling.cpp` for native visualizers, stack traces, exception callbacks, logging/crash paths, and debugger routing.

Results:

- Added `Source paths inspected` sections to the web, Android, and native debugging docs.
- Corrected stale stack/exception documentation from the previous context-object / deferred-log callback model to the current `CatchedStackTraceData`, `FormatStackTrace(const CatchedStackTraceData&)`, `WriteLogMessage`, and `SafeWriteStackTrace` model.
- Corrected the AngelScript provider name from the old stack-frame wording to current `CollectScriptStackLayers` / script-layer behavior.
- Corrected web preset references to the parent `../../CMakePresets.json` path.
- Backticked source/build/doc path checks for this slice: no missing paths.
- Symbol spot checks found the documented owners and APIs in current source/config, including `package-web-debug`, `prepare-host-workspace`, `Workspace/web-debug`, `package-android-debug`, `android-arm64`, `launch-game`, `FOnlineActivity`, `ClientNetwork.ServerHost`, `FO_STACK_TRACE_ENTRY`, `StackTraceData`, `CatchedStackTraceData`, `SetScriptStackTraceProvider`, `CollectScriptStackLayers`, `ResolveStackTrace`, `FormatStackTrace`, `SafeWriteStackTrace`, `GetStackTraceEntry`, `BaseEngineException`, `WriteLogMessage`, and `SetAsyncLogWriting`.
- Promoted in `Docs/DocumentationBacklog.md`: `WebDebugging.md`, `AndroidDebugging.md`, and `Debugging.md` from `drafted` to `verified`.

Follow-up:

- Completed by the later 2026-05-18 architecture/applications slice below; continue next with the build/generation slice.

## 2026-05-18 — architecture/applications slice

Scope:

- `Docs/Architecture.md`
- `Docs/Applications.md`
- `Docs/DocumentationBacklog.md`

Source areas checked:

- `Source/Applications/` for all current app entry-point files.
- `Source/Common/EngineBase.*`, `Source/Common/Entity.*`, `Source/Common/ScriptSystem.*`, `Source/Client/Client.h`, `Source/Server/Server.h`, `Source/Frontend/Application.h`, and `Source/Frontend/ApplicationInit.cpp` for the architecture layer map.
- `BuildTools/cmake/stages/Applications.cmake` and `BuildTools/cmake/helpers/Build.cmake` for application target wiring and helper ownership.

Results:

- Added `Source paths inspected` sections to the architecture and applications docs.
- Replaced stale future-doc wording with links to now-present `BuildToolsPipeline.md`, `Scripting.md`, `ServerRuntime.md`, `BakingPipeline.md`, and `GeneratedApiAndMetadata.md`; kept testing routed to `Source/Tests/README.md` until a dedicated Docs/Testing.md exists.
- Backticked source/build/doc path checks for this slice: no missing paths.
- Symbol/path spot checks found all documented application entry points and app-wiring helpers in current source.
- Promoted in `Docs/DocumentationBacklog.md`: `Architecture.md` and `Applications.md` from `drafted` to `verified`.

Follow-up:

- Completed by the later 2026-05-18 build/generation slice below; continue next with the script-boundary slice.

## 2026-05-18 — build/generation slice

Scope:

- `Docs/BuildToolsPipeline.md`
- `Docs/BakingPipeline.md`
- `Docs/GeneratedApiAndMetadata.md`
- `Docs/DocumentationBacklog.md`

Source areas checked:

- `BuildTools/Init.cmake`, all current `BuildTools/cmake/stages/*.cmake`, `BuildTools/cmake/helpers/*.cmake`, `BuildTools/codegen.py`, and `BuildTools/package.py` for staged build/generation/package ownership.
- `Source/Applications/BakerApp.cpp`, `Source/Applications/BakerLib.cpp`, `Source/Tools/Baker.*`, all current `Source/Tools/*Baker.*` implementations, `BuildTools/cmake/stages/ScriptsAndBaking.cmake`, and baker tests for baking behavior.
- `BuildTools/cmake/stages/Codegen.cmake`, `BuildTools/cmake/stages/EngineSources.cmake`, metadata helpers/state, `BuildTools/codegen.py`, `Source/Common/MetadataRegistration.*`, `Source/Common/MetadataRegistration.template.cpp`, `Source/Common/GenericCode.template.cpp`, `Source/Common/Properties.*`, `Source/Common/Entity.*`, `Source/Tools/MetadataBaker.*`, metadata/property tests, and `PUBLIC_API.md` for generated API and metadata behavior.

Results:

- Added `Source paths inspected` sections to the BuildTools, baking, and generated API/metadata docs.
- Replaced remaining future-script-doc wording in build routing with a real link to the present `Scripting.md` page.
- Confirmed current built-in baker owners include `ModelMeshBaker` / `ModelInfoBaker` under `Source/Tools/` and current model-baker coverage in `Source/Tests/Test_ModelBaker.cpp`.
- Backticked source/build/doc path checks for this slice: no missing paths.
- Symbol spot checks found the documented owners and APIs in current source, including `AddStageHook`, `AddExecutableApplication`, `AddSharedApplication`, `BakeResources`, `ForceBakeResources`, `CompileAngelScript`, `CompileMonoScripts`, `BaseBaker`, `SetupBakers`, `MasterBaker`, `MetadataBaker`, `CodeGeneration`, `ForceCodeGeneration`, `FO_SOURCE_META_FILES`, `FO_MONO_SOURCE`, `FO_ADDED_COMMON_HEADERS`, `FO_EMBEDDED_DATA_CAPACITY`, `FO_INTERNAL_CONFIG_CAPACITY`, `RegisterDynamicMetadata`, `MetadataRegistration` templates, `GenericCode-Template`, and `PropertyRegistrator`.
- Promoted in `Docs/DocumentationBacklog.md`: `BuildToolsPipeline.md`, `BakingPipeline.md`, and `GeneratedApiAndMetadata.md` from `drafted` to `verified`.

Follow-up:

- Completed by the later 2026-05-18 scripting/nullability slice below; continue next with the tools/mapper slice.

## 2026-05-18 — scripting/nullability slice

Scope:

- `Docs/Scripting.md`
- `Docs/ScriptMethodsMap.md`
- `Docs/Nullability.md`
- `Docs/DocumentationBacklog.md`

Source areas checked:

- `Source/Common/ScriptSystem.*`, `Source/Scripting/AngelScript/AngelScriptScripting.*`, `AngelScriptBackend.*`, `AngelScriptAttributes.cpp`, `AngelScriptCall.cpp`, `AngelScriptEntity.cpp`, `AngelScriptGlobals.cpp`, `AngelScriptRemoteCalls.cpp`, `AngelScriptReflection.cpp`, engine core scripts, Mono/Native roots, and `BuildTools/cmake/stages/ScriptsAndBaking.cmake` for scripting runtime/build flow.
- All 18 current `Source/Scripting/*ScriptMethods.cpp` files for native exported method ownership and current export counts.
- `Source/Scripting/AngelScript/AngelScriptAttributes.cpp`, `Source/Tools/MetadataBaker.cpp`, `BuildTools/codegen.py`, `Source/Common/ScriptSystem.h`, `Source/Essentials/BasicCore.h`, nullable analyzer tools under `../Tools/NullableEstimate/`, parent VS Code/CI task wiring, and nullable/script tests for nullability contracts.

Results:

- Confirmed the current native script method map: 874 `///@ ExportMethod` declarations across 18 script method files after the 2026-05-21 default-argument overload collapse.
- Corrected stale nullability workflow wording: current nullable appliers preserve author-chosen markers and remove redundant guards; they do not own automatic contract inference.
- Replaced stale parent docs routes in `Nullability.md` with current engine docs and the current `Source/Tests/README.md` testing source of truth.
- Backticked source/build/doc path checks for this slice: no missing paths.
- Symbol spot checks found the documented owners and APIs in current source, including `ScriptSystem`, `ScriptSystemBackend`, `RegisterBackend`, `MapScriptTypes`, `InitModules`, `FindFunc`, `CheckFunc`, `CallFunc`, `CallAdminFunc`, `NativeDataProvider`, `CheckArgNotNull`, `CheckReturnNotNull`, `InitAngelScriptScripting`, `CompileAngelScript`, `AngelScriptBackend`, `RegisterMetadata`, `CompileTextScripts`, `LoadBinaryScripts`, `StripNullableTypeSuffix`, and `is_validated_pointer_meta_type`.
- Promoted in `Docs/DocumentationBacklog.md`: `Scripting.md`, `ScriptMethodsMap.md`, and `Nullability.md` from `drafted` to `verified`.

Follow-up:

- Completed by the later 2026-05-18 tools/mapper slice below; continue next with the planned docs set.

## 2026-05-18 — tools/mapper slice

Scope:

- `Docs/Tools.md`
- `Docs/MapperTools.md`
- `Docs/DocumentationBacklog.md`

Source areas checked:

- All current `Source/Tools/*.h` and `Source/Tools/*.cpp` files, tool application entry points under `Source/Applications/`, and focused baker tests for reusable tool ownership.
- `Source/Applications/MapperApp.cpp`, `Source/Tools/Mapper.*`, `Source/Scripting/MapperGlobalScriptMethods.cpp`, `Source/Scripting/CommonGlobalScriptMethods.cpp`, `Source/Client/MapView.*`, and `Source/Common/Geometry.cpp` for mapper lifecycle, mapper automation helpers, screenshot/readback flow, and map/camera transform claims.
- Embedding-project examples explicitly marked as examples: `../../Scripts/MapperRender.fos`, `../../Tools/MapPreview/generate_map_preview.py`, `../../Tools/MapPreview/map_preview_overrides.ini`, and `../../LastFrontier.fomain`.

Results:

- Added `Source paths inspected` to `Docs/MapperTools.md` and kept project-specific map-preview/checkpoint details explicitly routed through `../../...` embedding-project paths.
- Replaced stale parent-doc links in `MapperTools.md` with engine-local `Architecture.md` / `Scripting.md` links where the owner now exists in engine docs; project-only build/checkpoint routes remain plain embedding-project paths.
- Backticked source/build/doc path checks for this slice: no missing paths.
- Symbol spot checks found the documented owners and APIs in current source, including `MasterBaker`, `BaseBaker`, `SetupBakers`, `MapperEngine`, `MapperMainLoop`, `DrawMapperFrame`, `ProcessMapperInputEvent`, `LoadMapFromText`, `LoadMap`, `ShowMap`, `SaveCurrentMap`, `SaveMap`, `Mapper_Game_*` exports, `Common_Game_RequestQuit`, `MapView::SetScreenSize`, `MapView::InstantScrollTo`, `MapView::InstantZoom`, `WriteSimpleTga`, and `GetHexOffset`.
- Promoted in `Docs/DocumentationBacklog.md`: `Tools.md` and `MapperTools.md` from `drafted` to `verified`.

Follow-up:

- Completed by the later 2026-05-18 planned-docs completion slice below; the initial documentation backlog is now complete.
## 2026-05-18 — planned-docs completion slice

Scope:

- `Docs/Essentials.md`
- `Docs/ConfigurationAndDataSources.md`
- `Docs/Testing.md`
- `Docs/DocumentationMaintenance.md`
- `Docs/README.md`
- `Source/Tests/README.md`
- `AGENTS.md`
- `Docs/DocumentationBacklog.md`

Source areas checked:

- `Source/Essentials/*.h`, `Source/Essentials/*.cpp`, `BuildTools/cmake/stages/EngineSources.cmake`, and essentials tests for the low-level foundation page.
- `Source/Common/ConfigFile.*`, `Settings.*`, `Settings.inc`, `DataSource.*`, `FileSystem.*`, `CacheStorage.*`, `Source/Essentials/DiskFileSystem.*`, `Source/Client/ResourceManager.*`, baker/config consumers, BuildTools generation/baking/package stages, and focused config/data-source/cache/filesystem tests for configuration/data-source routing.
- `Source/Applications/TestingApp.cpp`, all 79 current `Source/Tests/Test_*.cpp` files, `FO_TESTS_SOURCE` in `BuildTools/cmake/stages/EngineSources.cmake`, generated test/coverage target wiring in `BuildTools/cmake/stages/Applications.cmake`, coverage setup in `BuildTools/cmake/stages/Init.cmake`, `BuildTools/codecoverage.py`, and validator wrappers for the test-suite page.
- `../AGENTS.md`, `README.md`, `Docs/README.md`, `Docs/DocumentationBacklog.md`, `Docs/DocumentationExpansionPlan.md`, `Docs/DocumentationResearchTemplate.md`, and this report for documentation-maintenance workflow.

Results:

- Created the final four planned docs and linked them from `Docs/README.md` and `AGENTS.md` where appropriate.
- Updated `Source/Tests/README.md` from a partial stale inventory to a complete short source-tree entry point linked to `Docs/Testing.md`.
- Confirmed the current test inventory is 79 `Source/Tests/Test_*.cpp` suites and listed every suite in `Docs/Testing.md` and `Source/Tests/README.md`.
- Promoted in `Docs/DocumentationBacklog.md`: `Essentials.md`, `ConfigurationAndDataSources.md`, `Testing.md`, and `DocumentationMaintenance.md` from `planned` to `verified`.
- Marked the initial documentation backlog plan complete. Future doc work should be driven by new source changes, stale findings, or explicit requests.

Follow-up:

- No backlog-planned docs remain. Re-run the documented checks whenever source or docs change.
## 2026-05-18 — build workflow completion slice

Scope:

- `Docs/BuildWorkflow.md`
- `README.md`
- `Docs/DocumentationExpansionPlan.md`
- `Docs/DocumentationBacklog.md`

Source areas checked:

- `../CMakeLists.txt`, `../BuildTools/README.md`, `../BuildTools/Init.cmake`, `../BuildTools/validate.sh`, `../BuildTools/validate.cmd`, `../BuildTools/buildtools.py`, staged CMake files under `../BuildTools/cmake/stages/`, helpers under `../BuildTools/cmake/helpers/`, `../Source/Applications/TestingApp.cpp`, and `../Source/Tests/README.md`.

Results:

- Added source-inspection provenance and validation checklist to `BuildWorkflow.md`.
- Routed build validation to the newly created `Testing.md`, `Essentials.md`, and `ConfigurationAndDataSources.md` where appropriate.
- Added the final planned docs to the root `README.md` documentation index and refreshed `DocumentationExpansionPlan.md` current-baseline list.
- Promoted `BuildWorkflow.md` from `drafted` to `verified`; no non-legend `planned`, `researching`, or `drafted` backlog entries remain.

Follow-up:

- The initial documentation backlog remains complete; future work should be driven by source changes, stale findings, or new requested topics.

## 2026-07-08 — native conventions and safety contracts

Scope:

- `Docs/SmartPointers.md`
- `Docs/ExceptionSafety.md`
- `Docs/ThreadSafetyAnalysis.md`
- `Docs/DocumentationBacklog.md`
- `Docs/DocumentationExpansionPlan.md`

Source areas checked:

- `Source/Essentials/SmartPointers.h`, `Source/Essentials/BasicCore.h`, and `Source/Essentials/ExceptionHandling.cpp` for the native pointer vocabulary, non-null enforcement, and the `FO_BASIC_STRONG_ASSERT` bridge.
- `Source/Essentials/MemorySystem.h`, `Source/Essentials/Containers.h`, `Source/Essentials/CommonHelpers.h`, `Source/Server/DataBase.cpp`, `Source/Server/WorkerPool.cpp`, `Source/Tests/Test_EntityLifecycle.cpp`, and `Source/Tests/Test_ServerMapOperations.cpp` for exception-safety and lifecycle-invariant claims.
- `Source/Essentials/Threading.h` and `BuildTools/cmake/stages/Init.cmake` for `FO_TSA_*` annotations, `fo::` lock wrappers, and `-Wthread-safety` / `-Werror=thread-safety` Clang enforcement.
- Embedding-project example path `../../Tools/SmartPointerAudit/smart_pointer_audit.py` for the non-normative smart-pointer audit reference.

Results:

- Recorded the post-initial-backlog native-conventions docs in `Docs/DocumentationBacklog.md` with current source-validation status.
- Marked `Docs/DocumentationExpansionPlan.md` as a completed historical roadmap so future doc work starts from source changes, stale findings, or explicit requests instead of a duplicate inventory.
- Confirmed the native-conventions docs are routed from `AGENTS.md`, root `README.md`, and `Docs/README.md`.
- Markdown link check over `README.md`, `AGENTS.md`, `Docs/**/*.md`, `Source/README.md`, `Source/Tests/README.md`, and `BuildTools/README.md`: passed.
- Backticked source/build/doc path spot checks for this slice: no missing checked paths.
- `git diff --check`: passed.

Follow-up:

- No queued documentation slice remains in the expansion plan. Future native-convention updates should be driven by source changes and recorded here plus in `Docs/DocumentationBacklog.md`.

## 2026-07-10 - production documentation program audit and plan

Scope:

- `Docs/ProductionDocumentationPlan.md`
- `Docs/README.md`
- `Docs/DocumentationBacklog.md`
- `Docs/DocumentationExpansionPlan.md`
- `Docs/DocumentationVerificationReport.md`

Repository areas checked:

- Engine `README.md`, `AGENTS.md`, `TUTORIAL.md`, `PUBLIC_API.md`, all current `Docs/*.md`, source/build README files, `_config.yml`, `.github/workflows/validate.yml`, and the completed documentation expansion artifacts.
- Current source/build coverage under `Source/`, `BuildTools/`, `Resources/`, metadata annotations, `Source/Common/Settings.inc`, script method files, `Source/Tests/Test_*.cpp`, and `BuildTools/validation-project`.
- The Last Frontier documentation corpus at project commit `805caa79976b7cf4f81e46e1cf9ca0f1ea96ba43` as the primary mature embedding-project sample.
- Public `cvet/fonline-tla` files and recent history at commit `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` as the second integration sample.
- Diataxis, official GitHub Pages/Jekyll and custom-domain guidance, and the public `llms.txt` proposal for information-architecture, publication, localization, and AI-delivery choices.

Audit results:

- Confirmed the existing engine set is a strong internal reference but lacks a working beginner tutorial, current public API contract, generated full reference, canonical starter project, docs CI/site, translation system, and AI-readable manifest/evaluation.
- Counted 33 current Markdown links that escape a standalone engine root and 17 engine docs containing Last Frontier names or parent-project path markers; the new production plan itself has no project-path dependency.
- Confirmed current manual-inventory drift: 947 `///@ ExportMethod` declarations versus 932/874 documented counts, and 84 `Test_*.cpp` files versus 81/79 documented counts.
- Confirmed `Source/Common/Settings.inc` contains 265 fixed/variable settings with no complete generated settings reference.
- Confirmed `TUTORIAL.md`, `PUBLIC_API.md`, and the BuildTools README still contain placeholders or stale public-entry text.
- Confirmed TLA contains engine-owned animation documentation worth migrating, while its advertised scripting API URL returned HTTP 404 during this audit.

Plan results:

- Added a ten-phase execution roadmap covering standalone independence, docs platform and CI, public API generation, starter/tutorial work, content/tooling coverage, operations/migrations, public showcase repositories, AI delivery, and final Russian localization.
- Defined four public example repositories: a canonical template, a minimal multiplayer tutorial, a content/rendering showcase, and a native-extension sample.
- Kept the existing Markdown-to-GitHub Pages/Jekyll publication contract and `fonline.ru` custom domain, with mirrored `Docs/en/` and `Docs/ru/` trees instead of `Docs.EN` / `Docs.RU` sibling roots or a separate site framework.
- Added measurable launch gates, page definition of done, ownership/stability rules, cross-project best-practice promotion criteria, risks, and the first execution slice.
- Routed the active plan from `Docs/README.md`, `DocumentationBacklog.md`, and the completed historical expansion plan.

Mechanical checks:

- Local Markdown link check over 46 engine documentation entry files: passed in the current embedding checkout.
- Production-plan standalone local-link and repository-boundary check: passed.
- Production-plan heading/fence and whitespace checks: passed.
- Existing GitHub Pages contract check: root `CNAME` is `fonline.ru` and `_config.yml` selects `jekyll-theme-slate`.
- `git diff --check` for tracked edits plus an explicit untracked-plan whitespace check: passed; line-ending conversion warnings remain informational.
- Staged area: empty.

Follow-up:

- Start with the plan's `First execution slice`: ownership manifest, standalone docs CI, removal of root-escaping dependencies, generated inventories, public-contract/GitHub Pages ADRs, and the first runnable starter path.

## 2026-07-10 - standalone docs infrastructure and first starter lesson

Scope:

- `Docs/documentation-manifest.json`, `BuildTools/docs_validate.py`, `BuildTools/docs_inventory.py`, their focused tests, and `.github/workflows/validate.yml`.
- Standalone rewrites for the project-dependent platform/debugging/updater/mapper/nullability/testing pages and removal of all root-escaping local links.
- `Docs/Decisions/0001-github-pages-markdown-publication.md` and `Docs/Decisions/0002-public-api-stability-contract.md`.
- `Examples/MinimalProject/`, `BuildTools/buildtools.py`, `TUTORIAL.md`, and the first-run routes in `GettingStarted.md`, `EmbeddingProject.md`, and `BuildWorkflow.md`.

Source areas checked:

- All maintained Markdown entry points, root `CNAME`, `_config.yml`, current engine-owned source references, and the documentation publication/ownership plan.
- All 18 `Source/Scripting/*ScriptMethods.cpp` files, all 84 `Source/Tests/Test_*.cpp` files, and `Source/Common/Settings.inc` for deterministic inventory generation.
- BuildTools validation-project preparation, CMake stages, baker/resource-pack behavior, AngelScript side preprocessing, application settings loading, headless server startup, and database/network initialization.

Results:

- Added a machine-readable ownership/classification manifest covering 49 maintained Markdown entries, including the canonical example README.
- Added a fast standalone validator for manifest coverage, source paths, local links/anchors, repository-boundary escapes, placeholder honesty, Pages domain/config, and generated-inventory freshness.
- Added deterministic source inventory output for 947 exported script methods, 84 native test files, and 265 fixed/variable settings; removed stale hand-maintained method/test counts from prose.
- Replaced the tutorial placeholder with a tested headless lesson and replaced the former empty internal validation scaffold with `Examples/MinimalProject`.
- Added Windows and Linux starter targets to CI. Windows x64 passed locally; Linux remains pending because WSL is unavailable on this host and must be confirmed by GitHub Actions.
- Hardened the smoke with a 60-second timeout, process-exit validation, and required AngelScript lifecycle markers. BuildTools now recreates the disposable project copy so removed files cannot survive between runs.

Integration findings incorporated into the example and tutorial:

- The `Config` baker requires a complete explicit engine settings set, so the first unpackaged milestone intentionally omits that packaging layer.
- Importing all `CoreScripts` also imports project contracts such as world-time and generated GUI symbols; the minimal server pack contains only its owned script.
- AngelScript baking still validates each runtime side, so the module retains one common declaration outside its server-only lifecycle block.
- Unpackaged runtime startup consumes explicit `.fomain` values; the starter therefore declares memory storage, ID allocation, network ports, and a smoke-only networking override.

Mechanical checks:

- `python BuildTools/tests/test_docs_inventory.py`: 2 tests passed.
- `python BuildTools/tests/test_docs_validate.py`: 8 tests passed.
- `python BuildTools/docs_inventory.py --check`: current at 947 methods, 84 tests, and 265 settings.
- `python BuildTools/docs_validate.py`: passed for all 49 maintained Markdown entries.
- `python BuildTools/buildtools.py validate win64-starter-smoke`: passed after a clean project copy, resource bake, both lifecycle markers, and clean server shutdown.
- Python byte-compilation for the new validation/generation/smoke scripts: passed.
- Engine-style `clang-format` dry-run for the new C++ and AngelScript files: passed with the locally available formatter; the repository-required version 20 is not installed on this host.
- `git diff --check`: passed. Engine and parent-project staging areas are empty.

Follow-up:

- Confirm `linux-starter-smoke` in GitHub Actions, then mark the first execution slice complete.
- Continue with the generated public API model and GitHub Pages-compatible Jekyll preview rather than broad translation or prose migration.

## 2026-07-10 - GitHub Pages-compatible preview and publication contract

Scope:

- `_config.yml`, `.ruby-version`, `Gemfile`, `.gitignore`, and `.github/workflows/validate.yml`.
- `Docs/SitePublication.md`, `Docs/documentation-manifest.json`, and the human/AI entry-point routes.
- `BuildTools/docs_validate.py` and `BuildTools/tests/test_docs_validate.py`.
- Production-plan and backlog status for the documentation platform slice.

Evidence checked:

- Current GitHub Pages dependency set: Ruby `3.3.4`, `github-pages` `232`, Jekyll `3.10.0`, and Slate `0.2.0`.
- Official `actions/jekyll-build-pages@v1` source, inputs, and GitHub Pages-compatible Docker build behavior.
- Live `https://fonline.ru` landing page, root `CNAME`, and public DNS resolution to `185.199.108.153` through `185.199.111.153`.
- Public repository data and an unauthenticated Pages API request. The exact configured production source branch/folder could not be proved without repository administrator access and remains explicitly pending.

Results:

- Pinned the local compatibility environment in `.ruby-version` and `Gemfile`; `Gemfile.lock` is intentionally ignored as recommended for this Pages workflow.
- Extended `_config.yml` with the production URL/repository, strict front matter, supported relative-link rendering, and local/build-tree exclusions while retaining the Slate theme and Markdown source model.
- Added a dependent `Build documentation site` CI job that renders with the official Pages action and uploads `documentation-site-<commit-sha>` from `_site/` for 14 days. It validates only and does not replace the production Pages route.
- Added the engine-owned publication/operator guide and routed it from `README.md`, `Docs/README.md`, `AGENTS.md`, and the documentation maintenance guide.
- Expanded the machine-readable publication contract and validator so domain, config, Ruby version, Pages gem pin, source-verification state, Jekyll action, destination, and artifact upload cannot drift silently.
- Maintained an honest `pending-admin-verification` state for Pages source branch/folder instead of inferring an administrator setting from public output.

Mechanical checks:

- `python BuildTools/tests/test_docs_validate.py`: 11 tests passed, including wrong gem pin, missing Jekyll action, and incomplete verified-source failures.
- `python BuildTools/tests/test_docs_inventory.py`: 2 tests passed.
- `python BuildTools/docs_inventory.py --check`: current at 947 methods, 84 tests, and 265 settings.
- `python BuildTools/docs_validate.py`: passed for all 50 maintained Markdown entries.
- Python byte-compilation of the edited validator and tests: passed.
- Isolated Ruby `3.3.4` started successfully from `Workspace/`; local bundle installation reached native extensions and then correctly required RubyInstaller Devkit/MSYS2, which is unavailable on this host. No system Ruby was installed.

Follow-up:

- Observe the first green `Build documentation site` job and inspect its `_site` artifact.
- Have a repository administrator record the actual Pages source mode, branch, and folder in the manifest and this guide.
- Keep the existing production route unchanged; continue with navigation/search only after the current render is confirmed.

## 2026-07-10 - canonical native-codegen API model

Scope:

- `BuildTools/codegen.py`, `BuildTools/docs_api.py`, and `BuildTools/tests/test_docs_api.py`.
- `Docs/generated/api.json`, `Docs/documentation-manifest.json`, `BuildTools/docs_validate.py`, and documentation CI.
- `PUBLIC_API.md`, `Docs/GeneratedApiAndMetadata.md`, `Docs/ScriptMethodsMap.md`, and documentation maintenance routes.
- Production-plan and backlog status for Phase 3.

Source areas checked:

- Typed codegen tag records and parsers for exported enums, value/reference types, entities, properties, methods, events, settings, hooks, and migration rules.
- Metadata registration target expansion, common/client/mapper side rules, method receiver expansion, nullability/default normalization, and value-type layout parsing.
- All non-test C++/header/include inputs under `Source/`, with the independent source inventory retained as a cross-check.
- ADR 0002 stability defaults and the production plan's required canonical-model fields and explicit domain boundaries.

Results:

- Added parallel source provenance for parsed codegen tags and a resettable metadata parse session. Provenance is not added to tag dataclasses or compatibility-hash inputs, so documentation paths/lines do not alter the runtime compatibility contract.
- Added a deterministic `engine-native-codegen` JSON model with flat addressable symbols, normalized signatures, runtime sides, receivers, arguments/defaults/nullability, property/setting mutability, exact default command-line redaction state, descriptions, source locations, and explicit stability/version/example fields.
- Single symbols use their family ID; overloads retain the family ID and receive a deterministic signature-hash suffix. Generation rejects duplicate final IDs.
- The generated snapshot contains 2,459 addressable symbols: 947 native methods, 133 properties, 120 events, 265 settings, plus entities, enums/values, value/reference types and members, and migration rules.
- Every unclassified symbol is `internal`. The model lists project script metadata/remote calls, CMake/CLI/package contracts, and native-extension ABI details as excluded rather than implying false completeness.
- Added generated description/stability/provenance coverage metrics. The snapshot exposes a large source-comment backlog instead of filling missing descriptions with guessed prose.
- Added API-model freshness to the manifest, standalone validator, and fast CI job; generated JSON remains checked in for GitHub Pages, offline use, and AI retrieval.
- Kept `PUBLIC_API.md` as a visible placeholder route until generated human pages, explicit stability, remaining domains, and API-diff enforcement are complete.

Mechanical checks:

- Reused the codegen metadata parser twice in one Python process: both passes produced the expected method/source mapping without state leakage.
- `python BuildTools/tests/test_docs_api.py`: 3 tests passed, covering deterministic overload IDs/source lines, independent inventory parity, and stable JSON metadata.
- `python BuildTools/tests/test_docs_validate.py`: 12 tests passed, including stale API-model rejection.
- `python BuildTools/docs_api.py --check`: current for methods, properties, events, and settings.
- `python BuildTools/docs_validate.py`: passed for all 50 maintained Markdown entries after API-model integration.
- `python BuildTools/buildtools.py validate win64-starter-smoke`: production codegen, C++ build, resource bake, compatibility-version emission, both required script lifecycle markers, and clean server shutdown passed.

Follow-up:

- Generate human script/settings reference pages from `api.json` without restating signatures manually.
- Add source-authored stability/since/deprecation metadata before promoting any reachable symbol from `internal`.
- Integrate remote-call and other script-authored metadata through their owning parser, then add API diff classification.

## 2026-07-10 - generated native-codegen Markdown reference

Scope:

- `BuildTools/docs_reference.py` and `BuildTools/tests/test_docs_reference.py`.
- Seven generated pages under `Docs/generated/api/`, their manifest classifications, standalone freshness validation, and documentation CI commands.
- `PUBLIC_API.md`, `Docs/README.md`, `Docs/GeneratedApiAndMetadata.md`, `Docs/ScriptMethodsMap.md`, ADR 0002, and Phase 3 status routes.

Source areas checked:

- Every symbol kind and common field emitted by `BuildTools/docs_api.py`.
- Jekyll-compatible Markdown/front-matter requirements, stable symbol anchors, local navigation, and GitHub source-line links.
- The explicit distinction between command-line redaction-policy state and semantic credential sensitivity.

Results:

- Added a deterministic renderer that consumes only `Docs/generated/api.json`; it does not parse C++ or create a competing symbol model.
- Generated separate index, method, property, event, type/member, setting, and migration pages covering all 2,459 current native-codegen symbols.
- Added stable per-symbol anchors, normalized signatures, runtime sides, current stability labels, flags, source provenance links, and source-authored descriptions. Missing descriptions remain visible as the generated metadata-quality backlog.
- Classified all seven pages as public human reference with future English mirror destinations. The maintained Markdown inventory now contains 57 entries.
- Extended the manifest validator and fast documentation job so missing, stale, or manually edited generated pages fail byte-for-byte freshness checks.
- Kept `PUBLIC_API.md` as a visible placeholder because project-authored remote calls, CMake/CLI/package surfaces, explicit non-internal stability labels, and API-diff enforcement remain incomplete.

Mechanical checks:

- `python BuildTools/tests/test_docs_reference.py`: 5 tests passed, including every-symbol anchor coverage, Markdown/Liquid-safe escaping, unknown-kind rejection, deterministic output, and write/check behavior.
- `python BuildTools/tests/test_docs_validate.py`: 13 tests passed, including stale generated-page rejection.
- `python BuildTools/docs_api.py --check`, `python BuildTools/docs_reference.py --check`, and `python BuildTools/docs_inventory.py --check`: current.
- `python BuildTools/docs_validate.py`: passed for all 57 maintained Markdown entries.
- Python byte-compilation and `git diff --check`: passed.

Follow-up:

- Add source-authored stability/since/deprecation metadata and a reviewable classification backlog.
- Integrate project-authored remote calls through their owning structured parser.
- Add API snapshot diffing and explicit breaking-change disposition before replacing the public placeholder route.

## 2026-07-10 - source-owned API contract metadata

Scope:

- `BuildTools/codegen.py`, `BuildTools/docs_api.py`, `BuildTools/docs_reference.py`, and their focused tests.
- The first real `///@ ApiContract` source declaration in `Source/Scripting/CommonGlobalScriptMethods.cpp`.
- API schema v2, regenerated JSON/Markdown, ADR 0002, authoring guidance, and Phase 3 status.

Source areas checked:

- Existing codegen tag/meta/source stores, parser reset behavior, compatibility hashing, runtime generation loops, and export overload identity.
- ADR requirements for stable, experimental, internal, and deprecated surfaces, including since/replacement/removal policy.
- Generated page coverage for every symbol kind and local/HTTP example-link routing.

Results:

- Added the docs-only `ApiContract` tag to the normal typed codegen parser. It accepts exact symbol IDs or family IDs, stability and lifecycle fields, repeatable examples, and preceding contract notes while remaining outside the runtime compatibility hash.
- API schema v2 resolves selectors only after all canonical symbol IDs exist. Unknown and overlapping selectors, invalid examples, missing/self deprecated replacements, and incomplete lifecycle fields are hard generation errors.
- Every model symbol now records explicit/default contract provenance separately from declaration provenance. Summary fields expose declaration count, explicitly affected symbols, default-internal backlog, and explicit labels.
- Generated Markdown now renders API contract state, lifecycle, examples, notes, and contract source for methods, properties, events, settings, migrations, entities, types, and all type members.
- Explicitly classified `Game.BreakIntoDebugger` as development-only `internal`. The remaining 2,458 native-codegen symbols stay `internal (default)`; no stable/experimental promise was inferred from reachability or project usage.

Mechanical checks:

- Engine-only compatibility hash before and after adding `ApiContract`: `6586593177bf1e5f` in both runs.
- `python BuildTools/tests/test_docs_api.py`: 5 tests passed, including overload-family classification, deprecated replacement/lifecycle data, unknown-selector rejection, and hash invariance.
- `python BuildTools/tests/test_docs_reference.py`: 5 tests passed with every-symbol anchor and explicit contract rendering coverage.
- `python BuildTools/tests/test_docs_validate.py`: 14 tests passed, including explicit API schema-version pinning.
- API JSON, all seven reference pages, and source inventory freshness checks: current.
- `python BuildTools/docs_validate.py`: passed for all 57 maintained Markdown entries.
- `python BuildTools/buildtools.py validate win64-starter-smoke`: normal project codegen/build/bake and script lifecycle smoke passed without compatibility drift.
- Python byte-compilation and `git diff --check`: passed.

Follow-up:

- Integrate project-authored remote calls through their owning structured parser without making engine docs depend on a game repository.
- Add API snapshot diffing and breaking-change disposition against explicit contract labels.
- Review non-internal classifications only with release/support policy and domain-owner approval; continue exposing default-internal and missing-description counts meanwhile.

## 2026-07-10 - project remote-call reference and baked catalog

Scope:

- `BuildTools/docs_metadata.py` and `BuildTools/tests/test_docs_metadata.py`.
- `Docs/RemoteCalls.md`, scripting/nullability/metadata routes, public entry points, manifest, CI, backlog, and Phase 3 status.
- `Examples/MinimalProject` declarations, both side-specific handlers, CMake smoke inputs, and runtime verifier.

Source areas checked:

- `MetadataBaker` remote-call grammar, target expansion, side direction, source-file hint, binary container framing, and focused parser failures.
- Dynamic metadata registration, inbound/outbound uniqueness, AngelScript caller registration, handler declaration resolution, attribute validation, serialization, and payload-end checks.
- The existing native `api.json` ownership boundary and the requirement that concrete game calls remain project-owned.

Results:

- Added a strict little-endian `.fometa` decoder and project catalog generator that consumes the owning parser's server/client bake outputs instead of reparsing `.fos`.
- The generator rejects truncated/trailing/invalid-UTF-8 data, malformed records, duplicate inputs/calls, signature/source mismatches, and unpaired production records. It emits deterministic JSON and GitHub Pages Markdown with `script.remote-call.<target>.<name>` IDs, caller/handler surfaces, nullable argument data, input hashes, and paired evidence.
- Added the engine-owned remote-call reference for declaration grammar, direction, namespace/file binding, supported runtime payload families, server authority, bounded payloads, compatibility, catalog generation, troubleshooting, and validation.
- Kept `api.json` scoped to engine-native codegen while documenting the baked project supplement as the owning representation for game-authored calls.
- Extended the minimal project with one call in each direction. The first full bake correctly rejected the missing client inbound implementation even with `FO_BUILD_CLIENT=0`; adding the matching `[[ClientRemoteCall]]` proved that baking validates both side contracts.
- Updated Phase 3 method/property/event/remote/enum/type reference coverage to complete. API diffing, remaining CMake/CLI/package domains, and broad owner-reviewed stability classification remain open.

Mechanical checks:

- `python BuildTools/tests/test_docs_metadata.py`: 4 tests passed, covering binary framing, direction pairing, nullability, malformed input, determinism, and CLI write/check behavior.
- `python BuildTools/tests/test_docs_api.py`: 5 tests passed.
- `python BuildTools/tests/test_docs_reference.py`: 5 tests passed.
- `python BuildTools/tests/test_docs_inventory.py`: 2 tests passed.
- `python BuildTools/tests/test_docs_validate.py`: 14 tests passed.
- Native API JSON, all seven generated reference pages, and source inventory freshness checks: current.
- `python BuildTools/docs_validate.py`: passed for all 58 maintained Markdown entries.
- Actual starter `.fometa-server/client` inputs produced and then passed `--check` for a two-call JSON/Markdown project catalog.
- `python BuildTools/buildtools.py validate win64-starter-smoke`: production codegen/build/bake, both side handler bindings, runtime lifecycle, paired catalog IDs, and clean shutdown passed with compatibility version `23c3c0e2b71a1ed3`.
- Python byte-compilation for the generator, tests, and starter runner: passed.
- `git diff --check`: passed; Engine and parent-project staging areas are empty.

Follow-up:

- Add API snapshot diffing and require an explicit breaking-change disposition against source-owned contract labels.
- Generate CMake option/stage helper and BuildTools CLI/package references from their owning structured definitions.
- Confirm `linux-starter-smoke` and the Jekyll site artifact in GitHub Actions; administrator confirmation of the production Pages source remains external.

## 2026-07-11 - API diff and breaking-change disposition gate

Scope:

- `BuildTools/docs_api_diff.py`, its focused tests, and the cumulative disposition ledger.
- Documentation manifest validation and the GitHub Actions base-revision report/enforcement path.
- `Docs/ApiChangeManagement.md`, ADR 0002, generated API ownership docs, public/AI routes, backlog, and Phase 3 status.

Source areas checked:

- Canonical schema v2 symbol IDs, overload identity, stability/lifecycle fields, normalized signatures, parser contract, scope, descriptions, and provenance.
- Accepted stable/experimental/deprecated/internal policy and its migration/release/compatibility requirements.
- GitHub Actions pull-request base SHA and push `before` revision semantics, full-history checkout, always-uploaded diagnostics, and first-model bootstrap behavior.

Results:

- Added deterministic file/git revision comparison with separate full-model and provenance-insensitive contract digests. Source path/line churn cannot create a false breaking change or invalidate an otherwise identical contract disposition.
- Added additive, documentation, policy, and breaking classifications. Non-overloaded signatures modify one stable ID; signature-hashed overload changes are represented as removal/addition under the same family.
- Public enforcement uses baseline stability. Public removals/shape changes and `stable -> experimental/internal` or `experimental/deprecated -> internal` withdrawals require an exact disposition; internal refactors remain visible without becoming compatibility promises.
- Canonical source-parser, model-scope, and parser-contract changes always require disposition.
- Added a cumulative ledger whose entries bind change ID plus baseline/current contract hashes and require owner classification, rationale, migration, release-note, and compatibility handling. Old unmatched entries are inert rather than reusable approvals.
- GitHub Actions now compares the complete PR/push range, writes `Workspace/api-diff.json` and `.md`, blocks missing dispositions, and uploads the report even when enforcement fails. Standalone validation rejects removal of the ledger, full-history checkout, base-ref argument, or `--enforce`.
- Added the public maintainer guide and changed the Phase 3 API-diff item to complete. The overall public route remains a placeholder because non-codegen CMake/CLI/package domains and broad owner-reviewed non-internal classifications are still incomplete.

Mechanical checks:

- `python BuildTools/tests/test_docs_api_diff.py`: 7 tests passed, including public removal, signature change, stability-withdrawal bypass, overload replacement, parser-contract drift, stale digest, invalid ledger, and CLI artifact enforcement.
- `python BuildTools/tests/test_docs_validate.py`: 16 tests passed, including malformed ledger and missing workflow-enforcement failures.
- Existing API/reference/remote-metadata/inventory tests: 5 + 5 + 4 + 2 passed.
- Native API JSON, all seven generated reference pages, and source inventory freshness checks: current.
- `python BuildTools/docs_validate.py`: passed for all 59 maintained Markdown entries.
- Identical current/baseline models produced `pass` with zero changes; the real current `HEAD` produced the expected `bootstrap` report because this uncommitted documentation program has not yet placed `api.json` in git history.
- GitHub Actions YAML parsed successfully.
- `python BuildTools/buildtools.py validate win64-starter-smoke`: production codegen/build/bake, runtime lifecycle, paired remote-call catalog verification, and clean shutdown passed with compatibility version `23c3c0e2b71a1ed3`.
- Python byte-compilation and `git diff --check`: passed; Engine and parent-project staging areas are empty.

Follow-up:

- Observe the first API-diff artifact after this model lands; that first base without `api.json` is the only expected bootstrap run.
- Generate CMake option/stage helper/hook reference from owning structured definitions, then cover BuildTools CLI/package surfaces.
- Obtain owner/release-policy review before promoting native symbols beyond `internal`; diff enforcement does not infer public promises.
- Confirm `linux-starter-smoke` and the Jekyll site artifact in GitHub Actions; administrator confirmation of the production Pages source remains external.

## 2026-07-11 - generated CMake project-interface reference

Scope:

- `BuildTools/cmake/ProjectInterface.json` as the then-runtime-consumed project option, stage/entrypoint/hook, and selected-helper contract. This intermediate implementation was superseded by the 2026-08-24 docs-only reconciliation.
- `BuildTools/docs_cmake.py`, its focused tests, the structural CMake test, canonical JSON model, and four generated Markdown pages.
- Documentation manifest/freshness validation, GitHub Actions commands, BuildTools/public/AI routes, backlog, and Phase 3 status.

Source areas checked:

- Existing option declarations and precedence in `BuildTools/cmake/stages/Init.cmake` and `BuildTools/cmake/helpers/Options.cmake`.
- Stage execution, ordering, hook registration, and public entrypoints in `BuildTools/Init.cmake` plus all ten current stage files.
- Project-facing helper definitions in `BuildTools/Init.cmake` and `BuildTools/cmake/helpers/Build.cmake`, including repeated role/path pairs for `AddEngineSources`.
- Engine-owned minimal-project CMake composition and the production `win64-starter-smoke` configure/build/bake/runtime path.

Results:

- Added a strict schema-1 project-interface manifest containing the current 43 options, ten ordered stages, and five selected embedding helpers. Each documentation record has a stable `cmake.option.*`, `cmake.stage.*`, or `cmake.helper.*` ID.
- CMake now reads that manifest during configure: stage entrypoints are generated from its ordered records, hook names are validated from it, published helpers must resolve to real commands, and the `Init` stage declares options from the same data rendered by documentation.
- Added deterministic `Docs/generated/cmake.json` plus index, option, stage/hook, and helper pages with defaults, required state, precedence, signatures, roles, responsibilities, scope/support status, and source links.
- Declared the surface `experimental` with no versioned support line. BuildTools CLI/package grammar and native-extension ABI remain separate unfinished domains; native API diffing does not yet compare `cmake.json`.
- Manifest validation and the fast documentation workflow now reject CMake schema, source-path, structural-command, and byte-for-byte generated-output drift. All four human pages are classified for the future English/Russian mirror.
- The first full smoke exposed an unquoted semicolon in a boolean option help string. `DeclareBoolOption` now quotes the complete description, and the structural test declares every manifest option so this class of failure is caught before a native build.
- Removed the configure-time `cmake-vars.txt` side effect by consuming `cmake --help-variable-list` through `OUTPUT_VARIABLE`; script-mode checks no longer dirty the checkout.

Mechanical checks:

- Focused documentation tests: 46 passed (`5` API, `7` API diff, `5` CMake, `5` native reference, `4` remote metadata, `2` inventory, `18` standalone validator).
- `cmake -P BuildTools/tests/validate_project_interface.cmake`: passed while declaring all 43 options and verifying ten entrypoints/two hook positions per stage/five helpers; no `cmake-vars.txt` was created.
- Native API JSON, CMake JSON/pages, seven native reference pages, and source inventory freshness checks: current.
- `python BuildTools/docs_validate.py`: passed for all 63 maintained Markdown entries.
- `python BuildTools/buildtools.py validate win64-starter-smoke`: configure, native build, codegen, bake, both required lifecycle markers, baked remote-call catalog verification, and clean shutdown passed with compatibility version `23c3c0e2b71a1ed3`.
- Python byte-compilation and `git diff --check`: passed; Engine and parent-project staging areas are empty.

Follow-up:

- Generate a BuildTools CLI model/reference from owning `argparse` definitions and tested help output.
- Define and generate package declaration/payload contracts from the owning CMake/package parser instead of prose duplication.
- Extend revision comparison and disposition policy to the separate CMake and future CLI/package models before treating the complete project interface as stable.
- Confirm `linux-starter-smoke`, the Jekyll site artifact, and the first landed API-diff artifact in GitHub Actions; production Pages source confirmation remains external.

## 2026-07-12 - revision reconciliation and generated BuildTools CLI reference

Scope:

- Engine fast-forward `67ee893ae721d149cd44ff314abd8036adfd3821..411fbf09739a670125ddaaded1df2c4981f033e5`, integrated by embedding-project fast-forward `805caa799..f40bc2104` and its matching Engine gitlink.
- Revision-update ownership in `AGENTS.md` and `Docs/DocumentationMaintenance.md`, with a matching embedding-project reconciliation procedure maintained outside this engine repository.
- `BuildTools/buildtools.py::create_parser()`, new `BuildTools/docs_cli.py`, focused tests, canonical CLI JSON, two generated Markdown pages, manifest freshness validation, and documentation CI.
- Backlog, production-plan, public/API/build routes, and generated native/CMake cross-links.

Source areas checked:

- Every incoming Engine commit and the complete name/status diff, including source, tests, and owning docs rather than commit subjects alone.
- Location property broadcasts (`d49333973`), MSBuild test logging (`50245ab41`), removed helpers (`3a10b3549`), smart-pointer refactoring (`2eaf2f628`), finite numeric/property/layout and `BindFont` scale contracts (`6453772d6`), compile repairs (`26fdf21be`), guarded `nptr` dereference behavior (`d9f56e848`), property overlay alignment (`1c1314ae7`), Vulkan handle ownership (`c27171505`), and sanitizer/Direct3D/runtime exception fixes (`411fbf097`).
- `Source/Common/Common.h`, native metadata/codegen output, `ClientRuntime.md`, `ServerRuntime.md`, `EntityModel.md`, `Persistence.md`, `Essentials.md`, `Nullability.md`, `FrontendAndRendering.md`, `ExceptionSafety.md`, `Testing.md`, and `BuildToolsPipeline.md`.
- The actual BuildTools `argparse.ArgumentParser`, all command-specific executable `--help` paths, existing CI/starter consumers, and the standalone manifest/publication boundary.

Results:

- Added a mandatory revision reconciliation workflow: record old/new SHAs, retain safety copies, audit the full range, route each changed surface to an owning doc, regenerate affected contracts, compare old/current models, validate the embedding project, and drop the safety stash only after conflict/staging/freshness checks pass.
- Removed fixed compatibility-version examples from both engine and embedding-project maintainer instructions. The instructions now locate the current `MigrationRule Version` marker from source, preventing copied documentation from silently becoming stale.
- Reconciled the incoming runtime changes with their owning pages. Incoming documentation was retained where source-backed and supplemented for Location property broadcasts, MSBuild `RunAndLog.cmake`, and guard-aware direct `nptr<T>` dereference. Compile-only/helper-removal commits required no public behavior claim.
- Regenerated the native API model/reference against a preserved pre-update model. The exact delta contains two breaking-but-internal modifications: `migration.Version.0.0` now resolves to migration version 28, and `script.method.client.Game.BindFont` adds `float32 defaultScale = 1.0f`. Current policy requires no disposition for either change.
- Added a deterministic schema-1 `buildtools-cli` model generated by importing the executable `create_parser()` factory, not by parsing Python source or maintaining a second command list. It contains stable command/argument IDs, action/cardinality/choice/default/type data, exact usage and help output, source/scope metadata, and a contract digest.
- Added generated CLI index and command pages for all 11 commands and 22 arguments. Filling missing descriptions in `create_parser()` improved executable `--help` and generated reference together. The surface remains honestly classified `internal` until a versioned support policy is approved.
- Added byte-for-byte model/page freshness checks to the standalone validator and GitHub Actions. The focused test executes top-level help and all 11 `<command> --help` routes at a fixed width and proves generation remains identical under a different ambient terminal width.
- Updated generated native/CMake indexes, human navigation, manifest ownership, BuildTools docs, backlog, production plan, and public placeholder boundaries. Package contracts, helper-script CLIs, and multi-domain compatibility comparison remain open rather than being implied complete.

Mechanical checks:

- Focused documentation tests: 51 passed (`5` API, `7` API diff, `4` CLI, `5` CMake, `5` native reference, `4` remote metadata, `2` inventory, `19` standalone validator).
- Native API, CLI, CMake, native Markdown, and source-inventory `--check` commands: current. The inventory reports 947 export methods, 84 native test files, and 265 settings; the standalone manifest covers 65 Markdown entries.
- Preserved-baseline API diff: `pass`, two changes, zero required dispositions, zero missing dispositions.
- `cmake -P BuildTools/tests/validate_project_interface.cmake`: passed for 43 options, ten stages, and five selected helpers.
- `python BuildTools/buildtools.py validate win64-starter-smoke`: clean configure/build/bake/runtime shutdown on Engine `411fbf097`; both lifecycle markers and baked remote-call metadata passed with compatibility version `3c1157d446f74afe`.
- Embedding-project `BakeResources`: clean full rebake on root `f40bc2104`, including script compilation and 612 maps, with project compatibility version `a74c943a85d389ee`.

Follow-up:

- Define and generate package declaration and payload contracts from their owning CMake/package parser.
- Extend revision comparison and disposition policy across native, CMake, CLI, and package models before promoting the complete project interface beyond revision-pinned status.
- Confirm the first landed API-diff, documentation-site, and `linux-starter-smoke` artifacts in GitHub Actions; production Pages source confirmation remains an administrator task.

## 2026-07-12 - revision reconciliation and generated package contract

Scope:

- Root fast-forward `f40bc2104..4a0c0efc6` and Engine fast-forward `411fbf097..bd6f7316c`, including restoration of both documentation worktrees and resolution of the incoming `Docs/ExceptionSafety.md` overlap.
- The then-runtime-consumed `BuildTools/PackageInterface.json`, `BuildTools/package.py`, `DefinePackage`, deterministic package JSON/Markdown generation, structural tests, standalone freshness validation, and documentation CI. Runtime manifest consumption was superseded by the 2026-08-24 docs-only reconciliation.
- Package/public/AI/maintenance routes, project build and architecture links, backlog, production plan, and current package claims in the embedding project.

Source areas checked:

- Incoming malformed pre-handshake logging and exception-safety commits, their native tests, and owning networking/exception documentation.
- `DefinePackage` parsing and command construction in CMake, executable `package.py` argument parsing, platform implementations, payload staging, resource modes, archive/install outputs, and the current Last Frontier package declarations.
- Current and historical Android package entries in the embedding project, including removal commit `39196acf9` and the remaining CI SDK/NDK setup.

Results:

- Reconciled incoming exception and networking behavior without making reusable engine guidance depend on Last Frontier's project-local analyzer. Removed the unsupported `archive/noexcept-sweep` tag claim and documented project baselines as non-normative.
- Compared the regenerated native API model with the preserved pre-update baseline: zero contract changes, zero required dispositions, and zero missing dispositions.
- Added one runtime-consumed package contract for five targets, six platforms, 19 pack tokens, six payload families, eight artifact-producing packs, and the 13-argument internal packager CLI. Stable `package.*` IDs feed a deterministic JSON model and five GitHub Pages-compatible reference pages.
- Fixed both documented `argparse` factories to declare stable program names. A combined test discovery run can no longer leak its own `sys.argv[0]` into generated BuildTools CLI or package help.
- `package.py` now rejects malformed pack/architecture lists, unsupported platforms, invalid target/platform/pack combinations, placeholder packs, missing target-required modifiers, and modifier-only requests before staging. `DefinePackage` now requires at least one `BINARY` clause.
- A real `MakePackage-Dev` run exposed repeated same-name runtime members in `SingleZip`. The archiver now stores byte-identical members once and rejects conflicting contents; the generated contract and regression test describe and enforce that behavior.
- Corrected stale Last Frontier claims that `Daily`, `Staging`, and `Prod` currently emit Android APKs. The current declarations produce no Android package; SDK/NDK preparation alone is not an APK input or artifact.
- Public navigation and manifest ownership now expose the generated package reference while keeping the embedding project's concrete package matrix outside the reusable engine contract.

Mechanical checks:

- Focused documentation tests: 57 passed (`5` API, `7` API diff, `4` CLI, `5` CMake, `5` package, `5` native reference, `4` remote metadata, `2` inventory, `20` standalone validator).
- Package implementation tests: 6 passed and one platform-specific WiX test skipped; both package and project-interface CMake structural checks passed.
- Native API, CLI, CMake, package, native Markdown, and source-inventory generated checks are current. Standalone validation covers 70 maintained Markdown entries.
- `python BuildTools/buildtools.py validate win64-starter-smoke`: configure, build, bake, runtime lifecycle, remote-call metadata, and clean shutdown passed on Engine `bd6f7316c` with compatibility version `3c1157d446f74afe`.
- Last Frontier `BakeResources` passed on root `4a0c0efc6` with compatibility version `a74c943a85d389ee`.
- Last Frontier `MakePackage-Dev` passed after building its declared Server, Client, ClientLib, Mapper, and BakerLib inputs. The resulting 232,451,650-byte package-wide ZIP contains ten unique members and no duplicate names or warnings.

Follow-up:

- Add a multi-domain revision comparison and disposition gate for native API, CMake, BuildTools CLI, and package models.
- Cover helper-script CLIs and project-authored configuration keys as separate owner-backed surfaces rather than extending the internal packager CLI beyond its source boundary.
- Confirm the first landed documentation-site/API-diff artifacts and `linux-starter-smoke` in GitHub Actions; production Pages source confirmation remains an administrator task.

## 2026-07-13 - resource-pack reconciliation and multi-domain contract diff

Scope:

- Root fast-forward `4a0c0efc6..255a94836` and Engine fast-forward `bd6f7316c..3c1b0d0a7`, including named safety preservation and conflict-free restoration of both documentation worktrees.
- Incoming resource-pack glob filtering, offscreen scissor behavior, project resource-pack migration, and their owning engine/project documentation.
- Native API, CMake, main BuildTools CLI, and package model comparison through one revision-pair report and shared disposition ledger.

Source areas checked:

- `FileSystem::FilterFiles`, `ResourcePackInfo`, `GlobalSettings::AddResourcePacks`, every baker/tool resource consumer, `SpriteManager` scissor handling, and the new FileSystem/Settings/Baker tests.
- Incoming `ConfigurationAndDataSources.md`, `FrontendAndRendering.md`, Engine maintainer rules, Last Frontier `LastFrontier.fomain`, and the stale project architecture table.
- All four canonical generated models, their source parsers/manifests, the specialized native API comparator, documentation manifest/validator, GitHub Actions workflow, and public change-management routes.

Results:

- Retained the complete incoming engine guidance for recursive resource mounting, case-sensitive include/exclude globs, separator normalization, precedence, examples, and offscreen scissor preservation. Corrected Last Frontier's stale `RecursiveInput` architecture claim and routed detailed semantics to the reusable engine page.
- Regenerated native API, CMake, CLI, package, native Markdown, and source inventory outputs after the revision update. All four canonical models are byte-identical to the preserved pre-update snapshots; both specialized API and aggregate reports contain zero changes and zero required dispositions.
- Added `BuildTools/docs_contract_diff.py`. The aggregate report delegates native symbols/overloads to `docs_api_diff.py` and flattens CMake/CLI/package records by their source-owned stable IDs. Nested documentation changes, policy changes, additions, removals, shape changes, source/scope changes, and domain stability are classified without raw JSON text matching.
- CMake remains `experimental`, so breaking entry changes block without disposition. Main CLI and package domains remain `internal`, so entry churn is visible but does not create an accidental compatibility promise. Model source/scope/contract changes always require review.
- Replaced the API-only ledger with schema-v2 `Docs/contract-change-dispositions.json`. Entries bind explicit domain, domain-prefixed change ID, and that domain's baseline/current contract digests; stale or cross-domain entries cannot satisfy the gate.
- Added partial Git bootstrap: a model absent from the selected base is visibly bootstrapped while already-landed domains are still compared. Unknown Git revisions remain errors, and local directory baselines must contain all four models.
- GitHub Actions now writes and always uploads `Workspace/contract-diff.json` and `.md`; standalone validation pins the four model paths, aggregate generator/test, shared ledger, full-history checkout, base ref, and enforcement switch.
- Updated change-management, ADR, public API route, BuildTools/AI/maintenance guidance, backlog, production plan, manifest ownership, and embedding-project update instructions. Helper-script CLIs, native-extension ABI, project-authored settings, and behavior behind unchanged declarations remain explicit future domains.

Mechanical checks:

- Combined documentation discovery: 65 tests passed (`57` previous focused/validator tests plus `8` aggregate contract-diff tests).
- All API/CMake/CLI/package/reference/inventory generators are current; standalone validation passes for 70 maintained Markdown entries.
- Both CMake structural tests passed. Package implementation tests passed 6 with one platform-specific WiX skip. Python byte-compilation passed for all documentation generators and the changed BuildTools entrypoints.
- Preserved-baseline aggregate diff: `pass`, four domains, zero changes, zero required dispositions, zero missing dispositions. Specialized native API diff reports the same zero delta.
- `RunUnitTests`: all 334 test cases and 355,687 assertions passed, including the incoming resource-pack/FileSystem/Settings/Baker coverage.
- `win64-starter-smoke`: configure, build, bake, paired remote-call metadata, lifecycle markers, and clean shutdown passed on Engine `3c1b0d0a7` with compatibility version `3c1157d446f74afe`.
- Last Frontier `BakeResources`: clean full rebuild on root `255a94836`, project version `0.3.512`, compatibility version `a74c943a85d389ee`, including script compilation and 612 maps under the new glob-filter contract.
- Final root/Engine `git diff --check`, conflict-marker, JSON/YAML parsing, staging, gitlink, and upstream checks passed. Both branches report `0 0` against upstream; only the two named safety stashes were removed, leaving older stashes untouched.

Follow-up:

- Model helper-script CLIs by owner and user impact instead of folding unrelated parsers into the main BuildTools CLI model.
- Define native-extension ABI coverage only after the support boundary and release policy are owner-approved.
- Confirm the first landed aggregate contract-diff/site artifacts and `linux-starter-smoke` in GitHub Actions; production Pages source confirmation remains an administrator task.

## 2026-07-13 - generated helper CLI reference

Scope:

- Every engine-owned Python helper with a top-level executable `create_parser()` outside the separately modeled main BuildTools and package command lines.
- Runtime parser ownership, deterministic JSON/Markdown generation, complete parser discovery, standalone freshness validation, documentation CI, and aggregate contract comparison.
- Codegen, Mono compilation, coverage, Android-device, local-web-server, and MSI invocation owners plus the embedding-project update-reconciliation route.

Source areas checked:

- `BuildTools/codegen.py`, `compile-mono-scripts.py`, `codecoverage.py`, `android_device.py`, `web/simple-web-server.py`, and `msicreator/createmsi.py` parser/runtime paths.
- CMake codegen, scripts/baking, coverage targets, package WebServer/Wix consumers, current platform/build prose, and focused BuildTools/package tests.
- `BuildTools/docs_cli.py`, the new helper manifest/generator/tests, aggregate comparator/disposition validation, documentation manifest/validator, GitHub Actions, ADR, public/AI routes, backlog, and production plan.

Results:

- Added runtime-owned `create_parser()` factories with stable program names for all six helpers. Coverage now exposes documented `clean`, `run`, `report`, and `full` subcommands; MSI execution and generated help use the same parser while preserving the existing `run(list[str])` fixture boundary and bare-filename validation.
- Added `BuildTools/HelperCliInterface.json` for stable helper identity, owner, audiences, invocation owner, and explicit main-CLI/package exclusions. `BuildTools/docs_helper_cli.py` AST-scans the BuildTools tree and fails when a new parser is neither modeled nor assigned to another canonical domain.
- Generated [generated/helper-cli.json](../generated/helper-cli.json) plus checked index/command pages for 6 helpers, 11 subcommands, 17 global arguments, and 35 subcommand arguments. Every helper/command/argument has a stable `helper-cli.*` ID and exact fixed-width executable help.
- Added helper CLI as the fifth aggregate contract domain. It remains `internal` and revision-pinned: shape/ownership changes are visible, but ordinary entry churn does not create an accidental compatibility promise. Model source/scope/contract changes retain mandatory disposition handling.
- Added model/page freshness and workflow checks to standalone validation and CI. Human, AI, public-API, maintenance, change-policy, backlog, and production-plan routes now point to the helper reference and identify native-extension ABI as the next local contract gap.
- Fixed the CMake Mono invocation to pass the parser-required `FO_OUTPUT_PATH` scripts/project directory. Updated stale codegen pointer fixtures to the current `ptr`/`nptr` ABI and isolated the WiX package test from its global subprocess monkeypatch.

Mechanical checks:

- Combined documentation discovery: 70 tests passed, including 4 helper CLI tests and 21 standalone validator tests. Every real helper/subcommand `--help` path, AST inventory, deterministic rendering, escaping, and stale detection passed.
- API/CMake/main-CLI/helper-CLI/package/reference/inventory generators are current; standalone documentation validation passes for 72 maintained Markdown entries.
- Both CMake structural tests passed. Focused codegen/package pytest passed 11 tests with one platform-specific skip. Python byte-compilation and JSON/YAML parsing passed.
- Aggregate Git-baseline report: visible first-landing bootstrap for the new domain, 5 domains, zero changes, zero required dispositions, and zero missing dispositions.
- `win64-starter-smoke`: configure, build, codegen, bake, paired remote-call metadata, lifecycle markers, and clean shutdown passed on Engine `3c1b0d0a7` with compatibility version `3c1157d446f74afe`.
- Last Frontier `BakeResources`: codegen and incremental bake passed on root `255a94836`, project version `0.3.512`, compatibility version `a74c943a85d389ee`, with no warnings or errors.
- Final root/Engine fetch, `git diff --check`, conflict, staging, branch, and gitlink checks passed. Both repositories report `0 0` against upstream; the root gitlink and Engine HEAD are `3c1b0d0a7`; the four older user stashes remain untouched.
- Local Jekyll rendering was not available because this Windows environment has no Ruby/Bundler. The repository pins remain machine-validated and the GitHub Actions `jekyll-build-pages` artifact job is still the publication render gate.

Follow-up:

- Native-extension ABI ownership/model coverage is completed in the next section without promoting project-local extension behavior to an engine guarantee.
- Confirm the first landed six-domain contract-diff, documentation-site, and `linux-starter-smoke` artifacts in GitHub Actions; production Pages source confirmation remains an administrator task.

## 2026-07-13 - native-extension interface and conformance

Scope:

- Project-native C++ source roles, engine hooks, generated fallbacks, script exports, binding/lifetime/dependency rules, revision compatibility, and executable conformance.
- Runtime CMake/codegen ownership, deterministic JSON/Markdown reference, sixth-domain change policy, human/AI navigation, update reconciliation, and the engine-owned minimal project.

Source areas checked:

- `BuildTools/Init.cmake`, `BuildTools/cmake/ProjectInterface.json`, role routing/core libraries/stages, `BuildTools/codegen.py`, and all ten hook call sites under `Source/`.
- The new runtime manifest/generator/tests, aggregate comparator, documentation validator/manifest/workflow, public API/ADR/maintenance routes, backlog, production plan, and minimal project.

Results:

- Added the then-runtime-consumed `BuildTools/NativeExtensionInterface.json` with five roles, ten hooks, six binding rules, stable `native-extension.*` IDs, explicit `experimental` revision-pinned policy, and no cross-revision binary compatibility promise. Runtime manifest consumption was superseded by the 2026-08-24 docs-only reconciliation.
- At this intermediate revision CMake loaded the manifest, verified exact role parity with `ProjectInterface.json`, and rejected unknown `AddEngineSources` roles; `codegen.py` derived hook recognition, compatibility-hash participation, declarations, and fallback definitions from the same manifest. The later docs-only reconciliation restored source-owned CMake/codegen behavior and changed these manifests to checked documentation models.
- Added [NativeExtensions.md](../NativeExtensions.md), [generated/native-extension.json](../generated/native-extension.json), and four checked reference pages. The guide covers composition order, role selection, namespaces/exports, hook fallbacks/lifecycle, instance-owned state, dependencies/platform guards, secrets, update reconciliation, and validation.
- Added native extensions as the sixth aggregate domain. Experimental role/hook/binding removals or shape changes require exact disposition; project implementations, SDKs, settings, persistence, packaging, and release policy remain outside the engine guarantee.
- Expanded the minimal project with `Server_Game_NativeStarterValue`, a generated `Game.NativeStarterValue()` call, and required runtime marker `starter_native_extension_value=42`; the existing visibility hook still proves fallback suppression.

Mechanical checks:

- Combined documentation discovery: 76 tests passed, including 5 native-extension tests and 22 standalone-validator tests. All six generated models are current; standalone validation passes for 77 maintained Markdown entries.
- Three structural CMake tests passed, including valid role routing/header behavior, role-manifest parity, and expected rejection of unknown `EDITOR`. Focused codegen/package pytest passed 11 tests with one platform-specific skip.
- Aggregate Git-baseline report: visible first-landing bootstrap for the sixth domain, zero changes, zero required dispositions, and zero missing dispositions.
- `win64-starter-smoke`: configure, native compile/link, codegen, bake, `starter_native_extension_value=42`, lifecycle markers, paired remote-call metadata, and clean shutdown passed with compatibility version `9112a846dd71cc41`.
- `RunUnitTests`: all 334 test cases and 355,687 assertions passed with project compatibility version `a74c943a85d389ee`.
- Last Frontier `BakeResources`: codegen and incremental bake passed for project version `0.3.512` with no warnings or errors.
- Final root/Engine fetch found no incoming commits. Root `255a948368bbe57745571828965997cf395ff3c0` and Engine `3c1b0d0a78042fcecdb4f29904c1efd46bed1102` each report `0 0` against upstream; the root gitlink matches Engine HEAD, staging and unmerged counts are zero, and the four older user stashes remain untouched.
- Local Jekyll rendering remains unavailable because this Windows host has no Ruby/Bundler; GitHub Actions `jekyll-build-pages` remains the publication render gate.

Follow-up:

- Publish the separate `fonline-native-extension-sample` only after the in-tree contract is reviewed and tagged; keep the engine-owned minimal project as the CI conformance source.
- Confirm the first landed six-domain contract-diff, documentation-site, and `linux-starter-smoke` artifacts in GitHub Actions; production Pages source confirmation remains an administrator task.

## 2026-07-13 - script lifecycle and concurrency guide

Scope:

- Reusable AngelScript module initialization, callback ownership, async propagation, suspension/resumption, server entity synchronization, mutable-state ownership, destruction, and shutdown.
- Root update from `255a948368bbe57745571828965997cf395ff3c0` to `035b3068d8670d5a275d64f7ea500f1d489dafcc`; Engine remained at `3c1b0d0a78042fcecdb4f29904c1efd46bed1102`.
- Human/AI routing, translation classification, focused source-backed validation, documentation CI, and embedding-project ownership boundaries.

Source areas checked:

- `ScriptSystem` init ordering/global freeze, AngelScript attribute validation and backend function indexing, context suspension/resumption, client scheduled-callback snapshots, and server worker/script sync-context scopes.
- `EntitySync` replacement covers and singleton buckets, script-visible `Game.Sync` / `Game.SyncRelease` / `Game.Lock`, entity event/time-event cleanup, and focused engine tests.
- Incoming Last Frontier feedback/localization/test changes, including the move from shared id-keyed footstep cadence state to entity-owned non-persistent state; project code was research evidence only, not a normative engine dependency.
- The current public TLA project layout and scripting guidance as a non-normative research input; untagged project helpers were not promoted into engine guarantees.

Results:

- Added [ScriptLifecycleAndConcurrency.md](../ScriptLifecycleAndConcurrency.md). It defines the runtime as bounded script entries and makes the suspension rule explicit: server covers, singleton locks, and mutable-state decisions do not survive `Yield`; continuations must re-resolve/revalidate, reacquire, and re-read.
- Documented stable ascending `[[ModuleInit]]` priorities, the global freeze boundary, callback-only attribute ownership, transitive `[[Async]]`, client next-pass `Yield(0)`, server worker-pool resumption, complete-set `Game.Sync` replacement, the `Game.Lock` ordering constraint, and entity-owned state guidance.
- Kept Last Frontier's module topology in its project `Docs/Scripts.md` and routed reusable semantics to the engine guide. Updated engine human/AI indexes, scripting/test routes, root maintainer routing, and the translation-required manifest target.
- Added `test_docs_script_lifecycle.py` with four source-backed checks. `docs_validate.py` now requires the focused test in the GitHub documentation workflow, and validator regression coverage proves omission fails.

Mechanical checks:

- Focused lifecycle documentation tests: 4 passed.
- Standalone validator tests: 23 passed.
- Complete documentation discovery: 81 tests passed; standalone validation covers 78 maintained Markdown entries; documentation-manifest JSON, disposition JSON, GitHub Actions YAML, Jekyll config YAML, links, anchors, source paths, and generated freshness checks passed.
- Last Frontier `BakeResources`: clean full rebuild on root `035b3068d`, project version `0.3.513`, compatibility version `a74c943a85d389ee`; AngelScript compilation and all 612 maps completed without warnings or errors.
- Final fetch reports root `035b3068d` and Engine `3c1b0d0a7` each `0 0` against upstream; the root gitlink matches Engine HEAD, staging and unmerged counts are zero, and `git diff --check` passes. The task safety stash was removed after validation; two older root stashes and two older Engine stashes remain untouched.
- Local Jekyll rendering remains unavailable because Ruby/Bundler is not installed on this Windows host; GitHub Actions `jekyll-build-pages` remains the publication render gate.

Follow-up:

- Build the next local production slice as an engine-owned authored-format model/reference from its parser or baker.
- Confirm the first landed documentation-site, six-domain contract-diff, and `linux-starter-smoke` artifacts; production Pages source confirmation remains administrator work.

## 2026-07-13 - prototype format reference and update reconciliation

Scope:

- Safe root update from `035b3068d8670d5a275d64f7ea500f1d489dafcc` to `34d36e7017b05ecd2f546f0d67819940277156af` and Engine update from `3c1b0d0a78042fcecdb4f29904c1efd46bed1102` to `06b0ef451be87fb94080af8307f633921c285ba2`, preserving the existing dirty documentation program and generated artifacts.
- The first engine-owned authored-content contract: prototype discovery, section grammar, identity, inheritance, property applicability and values, references, migrations, diagnostics, and baker side outputs.
- Deterministic machine reference, GitHub Pages-compatible human guidance, seventh-domain evolution policy, embedding-project routing, and update-maintenance obligations.

Source areas checked:

- `Source/Tools/Baker/ProtoBaker.cpp`, `Source/Common/ConfigFile.cpp`, `Source/Common/Properties.cpp`, property serialization and migration paths, `Settings.inc`, API-model metadata, and focused engine tests.
- Incoming script context lifetime, inbound remote-call synchronization, `Game.OnCritterPreLoad`, updater documentation, and the five-test `ScriptLifecycleAndConcurrency.md` source contract.
- Last Frontier prototype catalogs, project-required `$Name` policy, content routes, maintenance workflow, and full resource bake. Project conventions were retained as companion guidance rather than promoted to engine guarantees.

Results:

- Added `BuildTools/PrototypeFormatInterface.json`, `docs_prototype_format.py`, and five focused tests. The generated model and four Markdown pages expose 4 prototype types, 113 live properties, 92 parser-authorable properties, 21 excluded temporary/core properties, and 11 source-backed format and validation rules.
- Added [PrototypeFormat.md](../PrototypeFormat.md) as the reusable authoring guide. It distinguishes engine basename fallback from stricter project identity policy, explains deterministic parent-before-component inheritance, rejects cycles, documents strict scalar/container values and references, and routes project semantic catalogs back to the embedding game.
- Integrated `prototype-format` as the seventh aggregate contract domain with stable IDs, source provenance, manifest ownership, freshness validation, CI execution, and experimental breaking-change disposition rules.
- Updated human, AI, public API, baking, entity, generated-metadata, maintenance, backlog, production-plan, and Last Frontier routes. Prototype parser/property-metadata changes now explicitly trigger same-change regeneration, review, and project bake.
- Reconciled the incoming API model at 2,460 symbols: 947 methods, 133 properties, 121 events, and 265 settings. `script.event.server.Game.OnCritterPreLoad` is recorded as the sole additive baseline change; 2,459 symbols remain default-internal and one contract is explicit.

Mechanical checks:

- Focused lifecycle tests: 5 passed. Focused prototype-format tests: 5 passed. Complete `test_docs*.py` discovery: 88 passed. Standalone validator tests: 24 passed; `docs_validate.py` validates 83 maintained Markdown entries.
- All API/reference/inventory/CMake/main CLI/helper CLI/native-extension/prototype-format/package generators are current. Three structural CMake interface tests passed; JSON parsing and `git diff --check` passed.
- Preserved-baseline aggregate diff: visible bootstrap for `prototype-format`, 7 domains, one additive change (`script.event.server.Game.OnCritterPreLoad`), zero required dispositions, and zero missing dispositions.
- `win64-starter-smoke`: Release configure/build, codegen, bake, native-extension value `42`, script lifecycle markers, paired remote-call metadata, server startup, and clean shutdown passed on Engine `06b0ef451` with compatibility version `6aaf98cf04f2acbd`.
- Last Frontier `BakeResources`: clean full rebuild on root `34d36e701`, project version `0.3.521`, compatibility version `83c5b2872ccdcf5c`; scripts, prototypes, texts, and all 612 maps completed without warnings or errors.
- Final fetch reports root and Engine `0 0` against upstream. The root gitlink and Engine HEAD both resolve to `06b0ef451`; staging and unmerged sets are empty, and only this task's two named safety stashes are removed after validation while four older user stashes remain untouched.
- Local Jekyll rendering remains unavailable because this Windows host has no Ruby/Bundler. GitHub Actions `jekyll-build-pages` remains the publication render gate for the Markdown site.

Follow-up:

- Model the complete `.fomap` contract beyond its prototype-like `[Header]`, including tile/object records, coordinates, ownership, diagnostics, and baker/runtime interpretation.
- Confirm the first landed seven-domain contract-diff, documentation-site, and `linux-starter-smoke` artifacts in GitHub Actions; production Pages source confirmation remains administrator work.

## 2026-07-14 - map format reference and strict current grammar

Scope:

- Safe root update from `34d36e7017b05ecd2f546f0d67819940277156af` to `a3dc2b77d2c7ddd085b39b4b4952f49aedffff70` and Engine update from `06b0ef451be87fb94080af8307f633921c285ba2` to `c523569b6232ac4f672612aea99ef06e97aa97b9`, preserving dirty documentation work and unrelated stashes.
- Reusable `.fomap` source syntax, map and placement identity, ownership references, property overrides, mapper round-trip, server/client baking, static/dynamic behavior, bounds, and runtime materialization.
- Deterministic JSON/Markdown reference, eighth-domain change policy, human/AI routing, update triggers, and Last Frontier integration boundaries.

Source areas checked:

- `Source/Common/ConfigFile.cpp`, `MapLoader.cpp`, metadata/property declarations, `Source/Tools/ProtoBaker.cpp`, `ProtoTextBaker.cpp`, `MapBaker.cpp`, mapper load/save, client static-map loading, server static-map loading/materialization, and focused map tests.
- Incoming root/Engine source, test, generated-model, and documentation changes across the complete recorded SHA ranges; Last Frontier maps and map-authoring tools were integration evidence, not normative engine dependencies.
- Existing prototype-format, contract-diff, documentation manifest/validator/workflow, public routes, maintenance policy, backlog, production plan, and publication-compatible Markdown structure.

Results:

- Corrected a real source/documentation mismatch. Current mapper, authored maps, and loaders use `[ProtoMap]`, but `ProtoBaker` retained a legacy `[Header]` branch and `MapBaker::ResolveMapName` read `$Name` from that obsolete section. The parser now requires exactly one first `[ProtoMap]`, accepts only `[Critter]` and `[Item]` afterward, rejects legacy/unknown sections, and names both baked outputs from `ProtoMap/$Name` or the source basename.
- Added focused unit coverage for first-section/cardinality/closed-section rules, current ProtoMap prototype handling, and explicit canonical baked output naming.
- Added `BuildTools/MapFormatInterface.json`, `BuildTools/docs_map_format.py`, five focused generator tests, [MapFormat.md](../MapFormat.md), [generated/map-format.json](../generated/map-format.json), and five checked reference pages. The model exposes 3 sections, 5 directives, 4 ownership modes, 16 rules, and 108 current Map/Critter/Item properties, including 87 authorable entries.
- Integrated `map-format` as the eighth aggregate contract domain. Fixed the shared disposition validator so `prototype-format` and `map-format` change IDs can be recorded, and added regression coverage that resolves experimental breaks across both domains.
- Added exact dispositions for replacing the erroneous experimental `[Header]` entry with `[ProtoMap]`; current projects already on `[ProtoMap]` require no content migration, while legacy content must convert and rebake both sides.
- Routed reusable map mechanics through engine docs and retained Last Frontier piece catalogs, composition grammar, AI authoring tools, custom metadata, and semantic validation in project docs.

Mechanical checks:

- Focused map-format tests: 5 passed. Complete `test_docs*.py` discovery: 94 passed. Standalone validator tests: 25 passed; `docs_validate.py` validates 89 maintained Markdown entries.
- All API/reference/inventory/CMake/main CLI/helper CLI/native-extension/prototype-format/map-format/package generators are current. Three structural CMake interface tests passed.
- Preserved-baseline aggregate diff: 4 changes across 8 domains, visible `map-format` bootstrap, 2 required dispositions satisfied, and 0 missing dispositions.
- Exception-safety audit: 5,259 functions checked, 0 errors, and 0 warnings after re-deriving unchanged `Basic`, `Strong`, and `Basic` levels for `MapLoader::Load`, `MapBaker::ResolveMapName`, and `ProtoBaker::BakeProtoFiles`.
- `RunUnitTests`: all 340 test cases and 355,901 assertions passed.
- Last Frontier `BakeResources`: clean full rebuild for project version `0.3.528`, compatibility version `b2418f8f43331b44`; scripts, prototypes, texts, and all 612 maps baked without warnings or errors.
- Final fetch reports root and Engine `0 0` against upstream. Root HEAD is `a3dc2b77d2c7ddd085b39b4b4952f49aedffff70`; root gitlink, upstream gitlink, Engine HEAD, and Engine upstream are `c523569b6232ac4f672612aea99ef06e97aa97b9`. Staging and unmerged sets are empty. Only the two named map-format safety stashes were removed after validation; four older user stashes remain untouched.
- Starter smoke was not rerun because the minimal-project/build surface did not change. Local Jekyll rendering remains unavailable because Ruby/Bundler is not installed; GitHub Actions `jekyll-build-pages` remains the publication render gate.

Follow-up:

- Select the next authored-format model from a confirmed parser/baker coverage gap; do not recreate Last Frontier-only conventions in engine docs.
- Confirm the first landed eight-domain contract-diff, documentation-site, and `linux-starter-smoke` artifacts; production Pages source confirmation remains administrator work.

## 2026-07-15 - manifest-backed AI documentation delivery

Scope:

- Safe Last Frontier update from `a3dc2b77d2c7ddd085b39b4b4952f49aedffff70` to `fd49c9583f5dcf9438b2d06a2adb5ed87e20ae79`; Engine remained at `c523569b6232ac4f672612aea99ef06e97aa97b9`.
- Deterministic `llms.txt`, bounded `llms-full.txt`, and public `docs-manifest.json` delivery from the canonical documentation manifest and Markdown corpus.
- GitHub Pages/Jekyll publication, stable URLs, normalized content hashes, visibility filtering, AI routing, maintenance triggers, CI freshness, and embedding-project reconciliation.

Source areas checked:

- `Docs/documentation-manifest.json`, public/current documentation entries, generated reference indexes, existing publication decisions, Jekyll configuration, documentation validators, and GitHub Actions.
- The new AI-delivery generator, focused tests, source-backed manifest and artifact validation, human/AI entry routes, maintenance guidance, backlog, production plan, and ADR 0003.
- The incoming Last Frontier `GlobalMap.CombatLocationProto` setting, its project documentation, runtime consumers, and project-owned resource-setting gameplay test. No reusable Engine contract changed in the incoming range.

Results:

- Added `BuildTools/docs_ai_delivery.py` and ADR 0003. The source manifest now owns the canonical locale, source ref, curated starting IDs, generated-page policy, and hard 1 MiB context budget.
- Generated root [llms.txt](../../llms.txt) with all public current documentation routes, [llms-full.txt](../../llms-full.txt) with public current authored pages plus generated indexes only, and [docs-manifest.json](../../docs-manifest.json) with stable IDs, audiences, Diataxis type, ownership/state/stability, canonical/site/source URLs, normalized SHA-256 hashes, sizes, and artifact metadata.
- The full-context artifact is assembled from whole documents only and fails rather than truncating content when the budget is exceeded. Placeholder, internal, and generated detail pages are excluded according to the recorded policy.
- The three files are ordinary repository-root static artifacts, so the existing GitHub Pages/Jekyll deployment serves them at stable `fonline.ru` URLs without a separate renderer or documentation source.
- Last Frontier maintenance guidance now requires same-change reconciliation and generator checks when an Engine update changes inventoried Markdown, manifest metadata, public paths, generated models, or publication policy.

Mechanical checks:

- Focused AI-delivery tests: 5 passed. Standalone validator tests: 26 passed. Complete `test_docs*.py` discovery: 100 passed; `docs_validate.py` validates 90 maintained Markdown entries.
- All API, CMake, main CLI, helper CLI, native-extension, prototype-format, map-format, package, reference, inventory, and AI-delivery generated outputs are current. The public catalog contains 85 documents and `llms-full.txt` is 851,160 bytes against the 1,048,576-byte hard limit.
- Three structural CMake interface tests passed. Preserved-baseline aggregate diff remains at 4 changes across 8 domains, with 2 required dispositions satisfied and 0 missing.
- Last Frontier `BakeResources` completed a clean full rebuild for project version `0.3.529`, compatibility version `b2418f8f43331b44`; scripts and all 612 maps baked without warnings or errors.
- The first focused gameplay invocation exposed only a stale `LF_ServerHeadless` binary with old native bindings. Rebuilding that exact target completed without warnings; `resources.script_tunable_settings_are_valid` then passed 1/1 with zero failures, timeouts, skips, or global exception delta.
- Local Ruby/Bundler is unavailable on this Windows host. GitHub Actions `jekyll-build-pages` remains the authoritative render and publication gate; production Pages source confirmation remains administrator work.
- Final fetch found no new root or Engine commits. Root and Engine report upstream parity, the root gitlink matches Engine HEAD, staging and unmerged sets are empty, and `git diff --check` passes. Only the two named task safety stashes were removed after validation; four older user stashes remain untouched.

Follow-up:

- Confirm the first landed AI-delivery and `jekyll-build-pages` artifacts, then verify `https://fonline.ru/llms.txt`, `https://fonline.ru/llms-full.txt`, and `https://fonline.ru/docs-manifest.json` from the production deployment.
- Continue the production program with professional site navigation/search and clean Markdown endpoints, followed by reviewed public example repositories; freeze the English information architecture before introducing maintained `Docs/en` and `Docs/ru` trees.

## 2026-07-15 - manifest-backed site navigation and search

Scope:

- Safe continuation from Last Frontier `fd49c9583f5dcf9438b2d06a2adb5ed87e20ae79` and Engine `c523569b6232ac4f672612aea99ef06e97aa97b9`, with named root and Engine `include-untracked` safety stashes around the dirty documentation program.
- Manifest-owned information architecture, deterministic navigation/search data, a custom GitHub Pages/Jekyll reader shell, responsive interaction, and static search without changing Markdown ownership or the existing publication architecture.
- A final fetch discovered Last Frontier `fc098abd8faf2ecbb669a607959c8c65f725fd61`. The complete incoming range was audited and reconciled: it changes quest content, quest/encounter tests, AiControl playtest tooling, and project-owned quest/bag documentation, but not the Engine gitlink, reusable Engine contracts, the Engine documentation manifest, or the site-delivery policy. Engine remained at `c523569b6232ac4f672612aea99ef06e97aa97b9`.

Results:

- Added `BuildTools/docs_site.py` and ADR 0004. `Docs/documentation-manifest.json` now owns the site title/description, layout and asset contract, search policy, eight navigation groups, and exact top-level document placement.
- Generated `_data/docs-site.json` with 59 navigation items and `assets/docs-search.json` with 84 public current human documents in 638,290 bytes. The compact weighted index preserves technical identifiers and remains below its 1 MiB hard limit.
- Added `_layouts/default.html`, local CSS/JavaScript, and an engine-owned bitmap mark. The shell provides a persistent desktop sidebar, responsive drawer, current-page state, page-local table of contents, static search, source route, copy controls, rolling `master` indicator, and persisted light/dark theme without remote application dependencies.
- Extended AI delivery so `llms.txt` and public `docs-manifest.json` expose the generated site navigation and search artifacts with normalized hashes. The site generator runs before AI delivery, and standalone validation plus GitHub Actions enforce both outputs in that dependency order.
- Updated publication, maintenance, generated-metadata, navigation, production-plan, backlog, AI-maintainer, and Last Frontier integration guidance. Same-change maintenance now requires assigning eligible pages to manifest navigation and regenerating site data before AI-delivery artifacts.

Mechanical checks:

- Focused site generator tests: 5 passed. Focused layout contract tests: 5 passed. Complete `test_docs*.py` discovery: 111 passed. Standalone validator tests are included in that discovery; `docs_validate.py` validates 91 maintained Markdown entries.
- All API, CMake, main CLI, helper CLI, native-extension, prototype-format, map-format, package, reference, inventory, site, and AI-delivery generated outputs are current. The AI catalog contains 86 public documents and 864,863 full-context bytes.
- Three structural CMake interface tests passed. Preserved-baseline aggregate diff remains at 4 changes across 8 domains, with 2 required dispositions satisfied and 0 missing.
- Browser checks exercised a 1440 x 1000 desktop layout, table of contents, theme preference, and technical `Game.Sync` search, then a 390 x 844 mobile viewport. Mobile document width and scroll width both measured 390 px, the long heading wrapped within 354 px, and the animated drawer settled from x=0 through x=292 without overlap or browser exceptions.
- Ruby, Bundler, Docker, and Podman remain unavailable locally, so the browser pass used a static behavioral preview of the authored layout, CSS, and JavaScript. GitHub Actions `jekyll-build-pages` remains the authoritative Liquid render and publication gate.
- This slice changed documentation/site tooling only. It did not change Engine runtime/build contracts or Last Frontier gameplay behavior, so `RunUnitTests` and `BakeResources` were not rerun for this slice.
- Final fetches report root and Engine `0 0` against upstream. Root HEAD is `fc098abd8faf2ecbb669a607959c8c65f725fd61`; root gitlink, upstream gitlink, Engine HEAD, and Engine upstream are `c523569b6232ac4f672612aea99ef06e97aa97b9`. Staging and unmerged sets are empty, `git diff --check` passes, the four task-created safety stashes were removed after validation, and the four older user stashes remain untouched.

Follow-up:

- Confirm the first landed site-data and `jekyll-build-pages` artifacts, verify production navigation/search and the public AI endpoints on `fonline.ru`, and confirm the repository Pages source setting.
- Continue with ownership, support policy, shared template, and CI contract for reviewed public example repositories before creating the repositories themselves.

## 2026-07-15 - public example repository governance and template

Scope:

- Safe Last Frontier update from `fc098abd8faf2ecbb669a607959c8c65f725fd61` to `aef3174067dc1812a62b15d5fb8f04e3db18d1ce` and Engine update from `c523569b6232ac4f672612aea99ef06e97aa97b9` to `1bcf6e101a25533f701cc4a65fdfe93fe0de5bee`, preserving the dirty documentation program and unrelated stashes.
- Final reconciliation advanced Last Frontier again to `ac841fd79` through four project commits and Engine to `2f4fc0adf` through one test-only commit, with named include-untracked safety stashes around both dirty worktrees.
- A professional, owner-gated public-example program for `fonline-project-template`, `fonline-minimal-multiplayer`, `fonline-content-showcase`, and `fonline-native-extension-sample` before any external repository is created.
- Exact Engine pinning, scheduled current-Engine compatibility, repository governance, release evidence, updater/ABI boundaries, support/security ownership, and asset provenance.

Source areas checked:

- `Examples/MinimalProject`, the public project-composition CMake surface, existing starter smoke, documentation/site/AI manifests, maintenance policy, GitHub validation workflow, and generated-contract machinery.
- The complete incoming root and Engine ranges. Project maps, gameplay, analytics, tests, MapAuthor/AiControl tooling, CI, and project docs remained project-owned. The reusable Engine range changed the updater protocol from generation 1 to 2, client host/runtime ABI from 2 to 3, and compatibility migration from 29 to 30; its updater guide and focused native/integration tests were preserved.
- The final root range updates quest/gameplay synchronization, synchronization-audit contracts and tests, project-owned quest/faction/global-map documentation, and the Engine gitlink. The final Engine range only supplies explicit types for integration-test port and response-encryption-key values; it does not change reusable runtime behavior or require a new Engine documentation contract.
- The refreshed Last Frontier bake exposed an incoming test compile error: `Sync::Snapshot()` returns base `Entity` handles, so `Test_Factions.fos` could not read `restoredCover[0].Id`. The exact-cover regression now compares handles directly, using the engine's identity fallback after entity `opEquals` removal.
- The proposed repository contract, ownership and dependency graph, required checks/artifacts, publication overlay, placeholder removal, submodule state, and asset provenance rules.

Results:

- Added [PublicExampleRepositories.md](../PublicExampleRepositories.md), ADR 0005, `Examples/PublicRepositories.json`, and a reusable `Examples/PublicRepositoryTemplate/` governance overlay. The registry defines four sequenced repositories, one source-ready template, stable owners, exact release pins, a weekly current-Engine lane, reviewed tags, required evidence, and exit gates.
- Added `BuildTools/docs_examples.py`, five focused tests, a deterministic machine model, generated Markdown reference, and repository verification in `pinned` and `current` modes. Standalone validation and GitHub Actions now enforce registry semantics, overlay completeness, generated freshness, workflow markers, exact gitlink/checkout agreement for releases, placeholder removal, and asset source/license/hash/path evidence.
- Made `Examples/MinimalProject` directly configurable through Windows and Linux presets. The Windows preset intentionally lets CMake select the newest installed Visual Studio instead of rejecting compatible newer installations with a hard-coded VS 2022 generator.
- Routed the human guide, generated reference, machine model, and ownership decision through the documentation manifest, site navigation/search, AI delivery, maintainer indexes, maintenance guidance, production plan, backlog, and Last Frontier Engine-update workflow.
- Reconciled updater protocol generation 2 and client host/runtime ABI 3 into the release policy. Repositories must publish a full client package across this frozen-host boundary; the current-Engine lane must not overwrite the exact release pin or claim release compatibility.

Mechanical checks:

- Focused public-example tests: 5 passed. Complete `test_docs*.py` discovery: 117 passed. `docs_validate.py` validates 95 maintained Markdown entries.
- All 13 API/CMake/CLI/helper/native-extension/prototype/map/package/reference/inventory/public-example/site/AI generated-document checks are current. The public-example model contains four repositories, one source-ready repository, and zero published repositories.
- The site model contains 62 navigation items and 87 searchable documents in 652,423 bytes. AI delivery contains 89 public documents and 894,249 full-context bytes.
- Three structural CMake interface tests passed. The preserved-baseline aggregate diff remains at four changes across eight domains, with two required dispositions satisfied and zero missing.
- `win64-starter-smoke` on Engine `1bcf6e101` reached native-extension value `42`, `starter_server_started`, `starter_smoke_passed`, and clean shutdown with updater protocol generation 2. The standalone `windows` preset then configured cleanly with CMake-selected `Visual Studio 18 2026`, MSVC 19.51, and the same Engine revision. The final Engine advance to `2f4fc0adf` is test-only and does not invalidate those runtime/configuration results.
- Final `RunUnitTests` on Engine `2f4fc0adf` passed 341 test cases and 355,926 assertions. Last Frontier `BakeResources` then completed for project version `0.3.532`, compiling scripts and baking all 612 maps; `factions.guard_report_offense_restores_exact_caller_cover` passed 1/1 with zero failures, timeouts, skips, or global exception delta on the rebuilt updater-generation-2 `LF_ServerHeadless`.
- Final fetches report Last Frontier `ac841fd791cb371d23e93df83e598ebbb5bdc27e` and Engine `2f4fc0adfdabf71316f087bf36ceb6baf49c81da` at `0 0` against upstream. The root gitlink matches Engine HEAD, staged and unmerged sets are empty, and `git diff --check` passes. Four task-created safety stashes were removed by verified hash; the two older root and two older Engine stashes remain untouched.

Follow-up:

- Owner authorization is still required before creating or publishing `cvet/fonline-project-template`; materialize it from `Examples/MinimalProject` plus the checked overlay, replace every placeholder, pin the exact Engine gitlink, enable repository security/branch protection, pass both compatibility lanes, and publish the first reviewed tag.
- Keep the other three repositories blocked on their recorded dependencies and exit gates. Confirm the first landed public-example, documentation-site, AI-delivery, and `jekyll-build-pages` artifacts in GitHub Actions before treating external publication as complete.

## 2026-07-16 - model animation metadata and duration reference

Scope:

- Safe Last Frontier update from `ac841fd791cb371d23e93df83e598ebbb5bdc27e` to `aa0d3b5e0a31ec92fa2efb9196282d66ad348025` and Engine update from `2f4fc0adfdabf71316f087bf36ceb6baf49c81da` to `fe7fb1e73af4d66a5ddd37828b5dff1544d54884`, with complete incoming-range audits and named `include-untracked` safety stashes around both dirty worktrees.
- Reusable `.fo3d` animation tuple metadata, speed scaling, state/action aliases, effective duration, private baked metadata, common script lookup, client loaded-model distinction, diagnostics, and embedding-project boundaries.
- Human, AI, site, generated-reference, maintenance, backlog, production-plan, and Last Frontier routing for the new guide.

Source areas checked:

- `Source/Tools/ModelInfoBaker.cpp`, model-info parsing and registration, common and client script bindings, loaded-model animation lookup/substitution, metadata access, focused native model-baker tests, and current generated script API.
- The complete incoming root and Engine ranges. The final root range changes combat speech, AI/fleeing behavior, synchronization helpers/audits, GUI, focused tests, and matching project documentation. The final Engine range only advances `MigrationRule Version` to `0 0 31`; generated compatibility and API artifacts were refreshed.
- Existing baking, generated-metadata, tool, method-map, documentation-maintenance, site/AI-delivery, contract-diff, and project-integration guidance.

Results:

- Added [ModelAnimation.md](../ModelAnimation.md), defining `Anim`, `AnimSpeed`, `StateAnimEqual`, and `ActionAnimEqual` from current source, including one-step alias behavior, source-tuple priority, unresolved-entry omission, and effective duration as `round((clip duration / speed) * 1000)` milliseconds.
- Documented `ModelAnimationInfo.foinfo` as private baker/runtime metadata rather than an authored contract. Common `Game.GetModelAnimDuration` returns zero for missing tuples; client `Critter.GetModelAnimDuration` queries the loaded model and may follow cross-model substitutions.
- Added five source-backed documentation tests and made them mandatory in standalone validation and GitHub Actions. Routed the guide through the manifest, navigation/search, AI artifacts, indexes, maintenance triggers, and Last Frontier content/build references.
- Regenerated the API and reference inventory at 2,461 entries, including 948 methods. The site contains 63 navigation items and 88 searchable documents in 658,355 bytes; AI delivery contains 90 public documents and 907,903 full-context bytes.

Mechanical checks:

- Focused model-animation documentation tests: 5 passed. Standalone validator tests: 29 passed. Complete `test_docs*.py` discovery: 123 passed; `docs_validate.py` validates 96 maintained Markdown entries.
- All 13 API/CMake/CLI/helper/native-extension/prototype/map/package/reference/inventory/public-example/site/AI generated-document checks are current. Three structural CMake interface tests passed.
- Preserved-baseline aggregate diff: 5 changes across 8 domains, 2 required dispositions satisfied, and 0 missing dispositions. The API delta includes the migration-version advance and additive internal `Game.GetModelAnimDuration` method.
- Focused native model-animation tests passed 3 test cases and 170 assertions. The authoritative `RunUnitTests` target passed 342 test cases and 356,125 assertions. An earlier randomized invocation exited without a Catch2 failure summary; focused and direct full reruns were green before the authoritative target rerun.
- Last Frontier `BakeResources` completed a clean full rebuild for project version `0.3.534`; scripts, 64 model-info files, prototypes, texts, and all 612 maps baked without warnings or errors. The previously recorded `footsteps` duration/cadence regression passed 8/8. After final synchronization, `combat_speech` passed 9/9 and the complete filtered run passed 10/10 with zero failures, timeouts, skips, or global exception delta.
- Incoming synchronization-audit tests passed 71/71. The final root and Engine parity, gitlink, staging, unmerged, diff, and stash checks are recorded in the active Last Frontier plan after the closing fetch.

Follow-up:

- Complete the broader source-backed model contract: full `.fo3d` composition, model layers, mesh/material/texture behavior, root motion, rendering/runtime substitution, and asset-pipeline validation.
- Confirm the landed documentation, contract-diff, site, AI-delivery, and `jekyll-build-pages` artifacts in GitHub Actions. Production Pages source confirmation and public example-repository creation remain owner-gated.

## 2026-07-16 - 2D sprite root motion and walk-cycle reference

Scope:

- Continued from Last Frontier `aa0d3b5e0a31ec92fa2efb9196282d66ad348025` and Engine `fe7fb1e73af4d66a5ddd37828b5dff1544d54884`. Opening and closing fetches both reported upstream parity, so no incoming range or task safety stash was required.
- Reusable 2D sprite root motion from authored `NextX` / `NextY` frame offsets through baking, sprite-sheet loading, movement-phase selection, rendered offset, direction changes, lifecycle resets, and zero-vector fallback.
- Human, AI, site, maintenance, backlog, production-plan, and Last Frontier integration routing. Authoritative movement, networking, 3D skeletal animation, and the complete `.fo3d` grammar remain outside this slice.

Source areas checked:

- `ImageBaker::FrameShot`, FOFRM parsing, baked collection serialization, `DefaultSpriteFactory`, `SpriteSheet`, `MovingContext`, and the 2D walk/run path in `CritterHexView`.
- TLA snapshot `b603d8fdbc2b2f89f233b2a1938686ead9d8d480`, pinned to Engine `801da0753b3a4bc2d1f52d0a0297bc658006ec05`, was used for discovery only. Every promoted behavior was re-derived from the current Engine source.
- Existing movement, baking, client-runtime, tool, documentation-maintenance, site, and AI-delivery guidance plus the current native ImageBaker test surface.

Results:

- Added [SpriteRootMotion.md](../SpriteRootMotion.md), separating 2D sprite root motion from `.fo3d` model animation and documenting source ownership, authoring, transport, activation, displacement, anchoring, phase projection, frame selection, rendered offset, transitions, fallback behavior, and embedding-project responsibilities.
- Added five source-backed documentation tests and made them mandatory in standalone validation and GitHub Actions. Routed the guide through the manifest, site navigation/search, AI artifacts, maintainer indexes, maintenance triggers, production planning, backlog, and Last Frontier content guidance.
- The guide states the ownership boundary explicitly: root motion selects and offsets rendered walk/run frames; it does not alter the logical path, speed, hex transitions, authoritative position, or network state.

Mechanical checks:

- Focused sprite-root-motion documentation tests: 5 passed. Standalone validator tests: 30 passed. Complete `test_docs*.py` discovery: 129 passed; `docs_validate.py` validates 97 maintained Markdown entries.
- All 13 API/CMake/CLI/helper/native-extension/prototype/map/package/reference/inventory/public-example/site/AI generated-document checks were current before this report entry; site and AI artifacts are regenerated again after recording it.
- Focused native ImageBaker validation passed 1 test case and 745 assertions, including expected malformed-input branches. Runtime/native behavior and project content did not change, so full `RunUnitTests` and Last Frontier `BakeResources` were not rerun for this documentation-only slice.
- The native suite has no focused `CritterHexView` root-motion fixture. A visible client scene remains the semantic validation gate for walk-cycle phase, direction changes, and perceived foot sliding.

Follow-up:

- Complete the independent `.fo3d` model-description and composition reference without conflating skeletal model animation with 2D sprite root motion.
- Consider adding a focused native `CritterHexView` phase/transition fixture and a reusable visible locomotion validation scene before changing the algorithm.
- Confirm the landed documentation, site, AI-delivery, and `jekyll-build-pages` artifacts in GitHub Actions; production Pages source confirmation remains owner-gated.

## 2026-07-16 - model-description format and reconnect reconciliation

Scope:

- Safe Last Frontier updates from `aa0d3b5e0a31ec92fa2efb9196282d66ad348025` through `513f9531c`, `95f0fb857`, `6811bb561`, and `8693be022`; Engine updates from `fe7fb1e73af4d66a5ddd37828b5dff1544d54884` through `5ce19ec24` and `dc630f17e`. Named root and Engine `include-untracked` safety stashes protected the dirty documentation program across every update.
- Independent reusable `.fo3d` documentation covering source meshes, parser grammar/state, layers, attachments, particles, transforms, textures/effects, cuts, animation integration, baking, runtime composition, diagnostics, and embedding-project validation.
- Reconciliation of the Engine caller-owned existing-player reconnect synchronization contract with Last Frontier authentication, plus every project behavior added by the incoming ranges.

Source areas checked:

- `ModelDescriptionParser`, `ModelInfoBaker`, `ModelMeshBaker`, `3dStuff`, render-time model limits, model-baker tests, generated project settings, and current TLA `.fo3d` evidence. TLA remained discovery evidence only; obsolete tokens and asset formats were not promoted.
- Engine login/chosen-critter lifecycle and synchronization changes, project authentication dispatch, local-map and global-map group ownership, generated API/migration data, exception-safety classifications, and the complete incoming root/Engine ranges.
- Incoming project faction AI, embedded-client PDA reload, Battalion identity/uniform/cursor behavior, fox pack behavior, analytics hard-disconnect cleanup, Antenna guard engagement radii, and AiControl client network statistics. Project-owned behavior stayed in Last Frontier docs and tests.

Results:

- Added [ModelFormat.md](../ModelFormat.md), `BuildTools/ModelFormatInterface.json`, `BuildTools/docs_model_format.py`, a canonical generated JSON model, seven generated reference pages, and seven source-backed documentation tests.
- Added `model-format` as the ninth aggregate contract-diff domain and routed it through CI, the documentation manifest, navigation/search, AI delivery, generated-reference maintenance, public API notes, production planning, backlog, and Last Frontier authoring routes.
- Last Frontier now prepares the full existing-login synchronization cover before calling Engine: incoming/live players, the controlled critter, local map/location, or every stable global-map group member. The auth chain is explicitly async, the reusable ownership rule is documented in [ScriptLifecycleAndConcurrency.md](../ScriptLifecycleAndConcurrency.md), and focused local/global regressions guard the integration.
- The first full native run exposed compressed test transport being parsed as raw messages. The lifecycle fixture now uses persistent stream decompression before inspecting message frames; final upstream `dc630f17e` adopted the same implementation, and the remaining local change is the explicit direct `Compressor.h` dependency.
- The project exception-safety audit writer now omits trailing empty TSV fields while preserving round trips, supports canonical `update --normalize`, and keeps the two changed server methods classified as `Basic`. The final audit checks 5,262 functions with zero errors or warnings.
- The closing project update keeps Antenna's dense headhunter population on tactical local aggro radii and exposes client ping/FPS through the AiControl observation and MCP schema. Its static-location and MCP tests are project-owned and do not introduce a reusable Engine documentation domain.

Mechanical checks:

- Focused model-format documentation tests: 7 passed. Complete `test_docs*.py` discovery: 137 passed; `docs_validate.py` validates 105 maintained Markdown entries.
- All API, CMake, main CLI, helper CLI, native-extension, prototype-format, map-format, model-format, package, reference, inventory, public-example, site, and AI-delivery generated outputs are current. The site model contains 66 navigation items and 97 searchable documents in 699,781 bytes; AI delivery contains 99 public documents and 958,741 full-context bytes. Three structural CMake interface tests pass.
- Preserved-baseline aggregate contract diff: 2 changes across 9 domains, with 0 missing dispositions. The changes are compatibility migration `0.0.32` and the documented internal `Game.LoginPlayerToExistentRecord` synchronization contract.
- Focused native ModelBaker validation passed 1 test case and 16 assertions. Final authoritative `RunUnitTests` on Engine `dc630f17e` passed 342 test cases and 356,156 assertions.
- Last Frontier `BakeResources` completed clean full rebuilds for project version `0.3.535`, compiling scripts and baking 64 model-info files and all 612 maps. The rebuilt `LF_ServerHeadless` passed 9/9 focused reconnect, faction AI, fox-pack, analytics logout, PDA reload, Battalion cursor/identity, and Antenna guard-radius tests with zero failures, timeouts, skips, or global exception delta.
- The complete AiControl MCP suite passed 1,522 tests. Its first post-update run exposed a stale hard-coded authored-exit coordinate; the path-selection regression now derives the current first target from the parsed map contract while still proving that an unreachable group is skipped for the reachable second group.
- Final fetches report Last Frontier `8693be022b9a7f6d3df4227d5cd2cee4d6afea5a` and Engine `dc630f17e1281358bbf2b603ca4bfd257cc27c94` at `0 0` against upstream. The root gitlink matches Engine HEAD, staged and unmerged sets are empty, and `git diff --check` passes. Six task-created safety stashes were removed by verified hash; the two older root and two older Engine stashes remain untouched.

Follow-up:

- Publish the first owner-approved repository from [PublicExampleRepositories.md](../PublicExampleRepositories.md), then use its review feedback to refine the reusable model example without weakening exact Engine pinning or provenance gates.
- Confirm the landed nine-domain contract-diff, documentation-site, AI-delivery, `RunUnitTests`, and `jekyll-build-pages` artifacts in GitHub Actions. Production Pages source confirmation remains administrator work.

## 2026-07-16 - documentation version, locale, and stable route contract

Scope:

- Continued from Last Frontier `8693be022b9a7f6d3df4227d5cd2cee4d6afea5a` and Engine `dc630f17e1281358bbf2b603ca4bfd257cc27c94`. Opening fetches reported upstream parity in both repositories, so no incoming range or task safety stash was required.
- Converted the accepted rolling-version, bilingual layout, stable URL, and redirect decisions into one machine-readable and CI-enforced Engine contract without moving the English corpus or beginning Russian translation.
- Kept GitHub Pages/Jekyll and repository Markdown as the only publication architecture.

Source areas checked:

- `Docs/documentation-manifest.json`, ADRs 0001 through 0005, `Docs/SitePublication.md`, the production plan, backlog, maintenance workflow, and human/AI entry points.
- `BuildTools/docs_ai_delivery.py`, `BuildTools/docs_site.py`, `BuildTools/docs_validate.py`, their focused tests, the default Jekyll layout, generated site/search/AI outputs, and the GitHub Pages workflow.
- Last Frontier Engine-update maintenance guidance and the active project plan, so future root/Engine updates regenerate site, route, and AI data in dependency order.

Results:

- Added [ADR-0006](../Decisions/0006-documentation-version-locale-routing.md). The unversioned site is now explicitly the rolling `current` channel on `master`; tagged snapshots remain deferred until supported release lines and a support matrix exist.
- Added source-owned `versioning` and `localization` sections to the documentation manifest. English remains canonical, Russian remains planned, `Docs/en` targets mirror to `Docs/ru`, and five README-style entry points have explicit locale pairs.
- Extended `BuildTools/docs_site.py` to schema 2 and generated [document-routes.json](../generated/document-routes.json). The model records current URLs, canonical future owners, planned English/Russian paths, availability, and every legacy route that must survive a move.
- Multiple old pages may converge only when exactly one non-`replace` document owns the future target. The current public API routes correctly converge on the generated API index.
- Added a validated `redirect` document state with `> Legacy route.` marker, non-human/non-search classification, stable target ID, shared canonical target, a direct Markdown link to that canonical file, and generated redirect ownership. This lets old Markdown URLs remain readable in GitHub and Jekyll after a move without generated HTML or another redirect plugin.
- Site navigation data and public `docs-manifest.json` now expose the same rolling version and locale policy. The layout labels `Current master` from generated data instead of maintaining an independent version string.
- Updated publication, generated-metadata, maintenance, backlog, production-plan, BuildTools, human/AI index, and Last Frontier Engine-update guidance. Physical `Docs/en` / `Docs/ru` migration, language switching, translation hashes/parity, and reviewed translations remain pending.

Mechanical checks:

- Focused AI-delivery tests passed 7/7, site generator tests 8/8, layout tests 5/5, and standalone-validator tests 35/35.
- Complete `test_docs*.py` discovery passed 146 tests. `docs_validate.py` validates 106 maintained Markdown entries.
- The generated site contains 67 navigation items and 98 searchable documents in 704,751 bytes. The route catalog contains 100 public routes, 94 planned legacy redirects, 97 canonical translation targets, and zero completed translation pairs, so no missing Russian page is presented as current.
- AI delivery contains 100 public document records; `llms-full.txt` is 972,340 bytes against the 1,048,576-byte hard limit. Its public manifest reports `current/master`, deferred release snapshots, canonical `en`, and the route-catalog artifact hash.
- Python compilation, generated `--check` gates, standalone link/source/manifest validation, and `git diff --check` pass. This slice changes documentation tooling and prose only, so native unit tests and Last Frontier resource baking were not rerun.
- Ruby and Bundler are not installed on this host. GitHub Actions `jekyll-build-pages` remains the authoritative Liquid render and publication gate.
- Closing fetches report Last Frontier `8693be022b9a7f6d3df4227d5cd2cee4d6afea5a` and Engine `dc630f17e1281358bbf2b603ca4bfd257cc27c94` at `0 0` against upstream. The root gitlink matches Engine HEAD, no task stash was created, and the four pre-existing user stashes remain untouched.

Follow-up:

- Keep full Russian translation blocked until the first execution slice has a green Linux starter run and the remaining English coverage is ready for the translation-pass freeze.
- Migrate public pages in reviewed groups: create the canonical `Docs/en` page, retain the old Markdown route as a validated pointer, add the reviewed `Docs/ru` counterpart, then enable language-preserving navigation and translation-hash parity.
- Confirm the landed documentation-site, route-catalog, AI-delivery, and `jekyll-build-pages` artifacts on `fonline.ru`; Pages source branch/folder confirmation remains administrator work.

## 2026-07-16 - text and localization format reference

Scope:

- Safe Last Frontier update from `8693be022b9a7f6d3df4227d5cd2cee4d6afea5a` to `2dd4968c03b765a29cc09c3ecf722102c45488c2` and Engine update from `dc630f17e1281358bbf2b603ca4bfd257cc27c94` to `dc124039423df71931cf3d7fd18a9664b20a469c`, with named root/Engine safety stashes retained through reconciliation and validation.
- Independent reusable documentation for raw `.fotxt`, structured keys and variants, ordered language baking/fallback, prototype `$Text`, runtime script lookup, language switching, renderer color tags, diagnostics, authoring practices, and the embedding-project formatting boundary.
- Last Frontier integration cleanup for its Russian-first policy, concrete packs, semantic key conventions, translation guards, `TextFormatting.fos`, and GUI refresh behavior. TLA revision `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` remained historical integration evidence only.

Source areas checked:

- `Source/Common/TextPack.*`, `Source/Tools/TextBaker.*`, `Source/Tools/ProtoTextBaker.*`, `Source/Common/Settings.inc`, client/server language loading, common/client/server script methods, AngelScript text value types, font inline-color parsing, and focused native text tests.
- Last Frontier `Docs/Localization.md`, `Scripts/TextFormatting.fos`, configured packs/languages, translation helpers/guards, and current project lookup conventions.
- TLA `README.md`, `Texts/`, and `Texts/Game.engl.fotxt` for historical numeric keys, duplicate variants, inline color, and project lexem evidence; no project-only formatter was promoted into Engine.

Results:

- Added [TextAndLocalization.md](../TextAndLocalization.md), `BuildTools/TextFormatInterface.json`, `BuildTools/docs_text_format.py`, [generated/text-format.json](../generated/text-format.json), six generated reference pages, and seven source-backed documentation tests.
- The generated model contains 38 stable entries: 7 syntax rules, 9 language rules, 8 prototype-text rules, 7 runtime methods, 2 rendering rules, and 5 validation rules. Engine defaults and the five prototype output packs are derived from live source.
- Added `text-format` as the tenth aggregate contract-diff domain and routed it through CI, standalone validation, the documentation manifest, navigation/search, AI delivery, generated-reference maintenance, public API notes, production planning, backlog, and Last Frontier integration routes.
- Corrected two project documentation errors: four-character language identifiers are a Last Frontier convention rather than an Engine parser requirement, and script `Game.GetText(key, skipCount = 0)` selects the first indexed variant rather than choosing a duplicate randomly.
- That source correction exposed a Last Frontier integration gap: `CombatSpeech::ReceiveCombatSpeech` currently uses the no-index overload, so its authored multi-line pools always resolve variant `0`. Project localization/combat docs now state the live behavior rather than promising random rotation.
- Separated Engine renderer-owned `@color` push/reset tags from Last Frontier-owned `@pname@`, `@arg@`, `@text@`, `@rnd@`, gender, and variant formatting.
- The preserved-baseline aggregate run exposed false map-format breaks caused only by derived `enum_source.line` movement. The comparator now excludes enum source provenance from contract digests/diffs, with a regression test proving line movement produces zero map-format changes.

Mechanical checks:

- Focused text-format tests: 7 passed; aggregate contract-diff tests: 9 passed; standalone-validator tests: 36 passed.
- Complete `test_docs*.py` discovery: 155 passed. `docs_validate.py` validates 113 maintained Markdown entries.
- All API, reference, map-format, text-format, site, AI-delivery, and remaining generated checks are current. The site contains 69 navigation items and 105 searchable documents in 727,071 bytes; the route catalog contains 107 public routes and 101 planned redirects.
- AI delivery contains 107 public document records; `llms-full.txt` is 992,872 bytes against the 1,048,576-byte limit.
- Preserved-baseline aggregate contract diff: 2 internal/API changes across 10 domains, with 0 required and 0 missing dispositions. The new text-format domain has zero baseline-to-current drift after bootstrap-equivalent seeding.
- This slice changes documentation tooling and prose only. Native text behavior, Last Frontier content, and runtime code did not change, so native unit tests and `BakeResources` were not rerun.
- Closing fetches report Last Frontier `2dd4968c03b765a29cc09c3ecf722102c45488c2` and Engine `dc124039423df71931cf3d7fd18a9664b20a469c` at `0 0` against upstream. The root gitlink matches Engine HEAD, staged and unmerged sets are empty, both `git diff --check` runs pass, the two task safety stashes were removed by verified hash, and the two older root plus two older Engine stashes remain untouched.

Follow-up:

- Add a public minimal localization example after the first template repository is owner-approved, including one raw pack, one prototype `$Text` entry, explicit variant selection, and a visible language-switch check.
- Restore Last Frontier combat-speech rotation as a gameplay change with explicit index selection and a deliberate server/client ownership decision; extend the focused suite beyond pool richness to prove runtime selection.
- Continue the remaining authored-format program with images, particles, GUI, and dialogs from their owning parsers/bakers rather than project prose.
- Confirm the landed eleven-domain diff, documentation-site, AI-delivery, and `jekyll-build-pages` artifacts in GitHub Actions; production Pages source confirmation remains administrator work.

## 2026-07-16 - effect format reference and update reconciliation

Scope:

- Safe Last Frontier update from `2dd4968c03b765a29cc09c3ecf722102c45488c2` to `5c500fd3bdc42e88720380b0d814999e470a6408` and Engine update from `dc124039423df71931cf3d7fd18a9664b20a469c` to `204d223d6ef6514494c4af053c323aa2c73e0a48`, with named root/Engine safety stashes retained through reconciliation and validation.
- Independent reusable documentation for `.fofx` sections, pass-specific shader/state fallback, render state, vertex/resource contracts, backend artifacts, runtime loading and path-only caching, script methods, `ScriptValueBuf` lifetime, diagnostics, and embedding-project responsibilities.
- Reconciliation of the incoming hard-error `InitScript` resolution contract, Last Frontier documentation ownership, and the project sync-flow CI move.

Source areas checked:

- `EffectBaker`, `RenderEffect`, all active renderer backends, `EffectManager`, client effect script methods, CMake effect limits, built-in effect sources, and focused native effect-baker tests.
- Last Frontier `Resources/Visual/Effects/`, effect selection/tuning scripts, project render settings, content validation routes, and existing shader policy.
- The complete incoming root and Engine ranges, including `ScriptHelpers::CallInitScript`, entity initialization, prototype callback metadata, exception-safety classifications, and sync-flow workflow ownership.

Results:

- Added [EffectFormat.md](../EffectFormat.md), `BuildTools/EffectFormatInterface.json`, `BuildTools/docs_effect_format.py`, a canonical generated JSON model, seven generated reference pages, and seven source-backed documentation tests.
- Added `effect-format` as the eleventh aggregate contract-diff domain and routed it through CI, the standalone validator, manifest, navigation/search, AI delivery, public API notes, maintenance, production planning, backlog, and Last Frontier authoring routes.
- The generated effect model contains 55 contract entries, 12 built-in resource records, and 8 validation rules. It records that `SetEffect` selects a path-cached `RenderEffect`; it does not clear cached script values or transfer them between paths. Last Frontier comments and docs now explain why tuning tools still re-push panel state after a variant change.
- The incoming built-in `InitScript` callback contract is now documented in the prototype and script-lifecycle references. Its intentional prototype-format scope expansion has an exact shared-ledger disposition with migration guidance for missing or mismatched `void(Entity, bool firstTime)` callbacks.
- The standalone validator fixture now isolates text/effect source anchors without one generated domain overwriting another, excludes generated effect detail pages from authored inventory checks, and the aggregate comparator no longer reports the obsolete "all four models" diagnostic.

Mechanical checks:

- Focused effect-format documentation tests passed 7/7. Complete `test_docs*.py` discovery passed 164 tests; `docs_validate.py` validates 121 maintained Markdown entries.
- Effect, site, AI-delivery, and aggregate contract generated checks pass. The site model contains 71 navigation items, 113 searchable documents, 115 public routes, and 109 planned redirects; its search artifact is 762,325 bytes.
- AI delivery contains 115 public documents; `llms-full.txt` is 1,029,603 bytes against the 1,048,576-byte hard limit.
- Preserved-baseline aggregate contract diff passes with 4 changes across 11 domains, 1 required disposition, and 0 missing dispositions. The reported changes are the current compatibility migration, the `LoginPlayerToExistentRecord` documentation update, and the documented `InitScript` scope/rule additions.
- The rebuilt `LF_UnitTests` binary passes all 4 focused `EffectBaker*` test cases with 106 assertions. Last Frontier `BakeResources` completed a clean full rebuild for version `0.3.539`, including scripts, 64 model-info files, and all 612 maps.
- The project sync-flow audit passes 111 tests. The reconciled exception-safety audit checks 5,262 functions with zero errors or warnings.
- Closing fetches report Last Frontier `5c500fd3bdc42e88720380b0d814999e470a6408` and Engine `204d223d6ef6514494c4af053c323aa2c73e0a48` at `0 0` against upstream, with the root gitlink matching Engine HEAD.

Follow-up:

- Continue the authored-format program with image/sprite source formats beyond root motion, particles, GUI, and dialogs from their owning parsers, bakers, and reusable modules.
- Add public example effects only after the first repository is owner-approved; keep every sample pinned to an exact Engine revision and validate the intended renderer/backend profile visibly.
- Confirm the landed eleven-domain contract-diff, documentation-site, AI-delivery, focused native, and `jekyll-build-pages` artifacts in GitHub Actions; production Pages source confirmation remains administrator work.

## 2026-07-18 - image and sprite format reference and update reconciliation

Scope:

- Safe Last Frontier update from `5c500fd3bdc42e88720380b0d814999e470a6408` to `819e45ec2b0df61c4bc172560cf7894a74357cda` and Engine update from `204d223d6ef6514494c4af053c323aa2c73e0a48` to `65dfb851c7b111e9e613be98c6bc1d49d3d61145`, with twelve named root/Engine `include-untracked` safety stashes retained through opening, closing, final, late third-party, closure, and upstream-fix reconciliation, then removed by verified hash after all gates passed. The root gitlink remains at `f667a85d9`; the deliberate two-commit working delta is the audited upstream rpmalloc/MSVC plus glslang warning fix.
- Independent reusable documentation for accepted image sources, FOFRM composition, per-file options, frame metadata, baked sprite collections, runtime factories, atlases, caches, diagnostics, validation, and embedding-project responsibilities.
- Reconciliation of all 24 incoming Last Frontier commits and eleven incoming Engine commits. Project-owned gameplay, synchronization, authentication, analytics, AI, quest, map-authoring, test-runner, and project dependency behavior remain in Last Frontier docs; reusable runtime and dependency behavior remain in their Engine owners.

Source areas checked:

- `ImageBaker`, all registered raster and legacy decoders, FOFRM parsing and flattening, baked collection serialization, `DefaultSpriteFactory`, `SpriteSheet`, `TextureAtlas`, `ResourceManager`, image settings, and focused native tests.
- Current Last Frontier PNG, TGA, and FOFRM usage for integration evidence. Project asset catalogs, art direction, pack policy, and visual composition remain project-owned; legacy binary formats are documented as import paths rather than recommended authoring formats.
- The complete incoming root and Engine ranges, including server entity-lock ownership, multi-target movement, `ItemView` handle comparison, malformed compressed-stream handling, personal-room fallback, crafting cadence, quest migration, tunnel-exit coverage, the network-client header guard, accepted-connection ownership, pre-login progress deadlines, engine dependency/toolchain refreshes, project-local curl/sentry refreshes, and their reusable/project documentation owners.

Results:

- Added [ImageFormat.md](../ImageFormat.md), `BuildTools/ImageFormatInterface.json`, `BuildTools/docs_image_format.py`, [generated/image-format.json](../generated/image-format.json), seven generated reference pages, and seven source-backed documentation tests.
- The generated model contains 49 entries. It derives 12 baker extensions (`fofrm`, `frm`, `fr0`, `rix`, `art`, `spr`, `zar`, `til`, `mos`, `bam`, `png`, `tga`) and 11 stock runtime extensions from current source. Direct `.spr` is intentionally identified as baker-only in the stock runtime route; projects should wrap it through FOFRM unless they extend the factory.
- Documented FOFRM aliases, relative `$` references, direction completeness, Main-sequence flattening, inherited offsets, descriptor-count timing, shared-record rejection, and the current parsed-but-unserialized `EffectName` behavior. The guide also distinguishes the stable baked RGBA8 collection from authored syntax and runtime atlas/cache policy.
- Added `image-format` as the twelfth aggregate contract-diff domain and routed it through CI, standalone validation, the source manifest, navigation/search, AI delivery, generated-reference maintenance, public API notes, production planning, backlog, and Last Frontier authoring routes.
- The complete image owner raised `llms-full.txt` above the old 1 MiB budget. ADR 0003 and the source manifest now set a reviewed 1.25 MiB (1,310,720-byte) fail-closed limit; generation still rejects overflow and never truncates a document. The separate search-index limit remains 1 MiB.
- Rebuilding current unit tests initially exposed an incoming test-only include cycle: `Test_NetworkClient.cpp` included both `ClientConnection.h` and its then-unguarded transitive `NetworkClient.h`. The final Engine range restores the owning header guard; the local redundant-include cleanup remains behavior-neutral, and the target builds on the final Engine revision.
- Root reconciliation corrected project documentation tails left by the incoming code: `DefaultPersonalRoomLocation` covers any null/non-room respawn, `Mobs::SpawnMob` has a monotonic caller-cover postcondition, caravan materialization remains independently cover-neutral, every visible `ToGlobalArea_*` tunnel exit requires hidden `ExitGrid.MultihexMesh` coverage, and project auth policy routes reusable accepted-connection and pre-login timeout mechanics to [Networking.md](../Networking.md).
- The late dependency refresh is owned by [ThirdPartyMaintenance.md](../ThirdPartyMaintenance.md), `ThirdParty/README.md`, and the incoming Last Frontier refresh plan. The project reconciliation also updates stale operational version extraction to libcurl 8.21.0 and sentry-native 0.15.3 and makes their source headers the documented version authority; no image-format contract changed.
- Native validation exposed two integration gaps in the refreshed Engine pins on MSVC. The owning upstream commits add a reusable `TargetCompileOptions` wrapper, build rpmalloc 2.x as C11 with `/experimental:c11atomics`, and rename glslang's shadowing local in the vendored header with an explicit `(FOnline Patch)` marker. This checkout also makes the declared C standard required. Clean `BakerLib`, `LF_UnitTests`, `LF_ServerHeadless`, and full project bake rebuilds pass without the incoming `C4458` warning.
- The closure root commit separates production sync-flow collection from the full gameplay-test graph, removes obsolete baseline entries, strengthens Irvin and tunnel survival/lock tests, and teaches the Windows updater test to skip Direct3D only when Session 0 cannot create a swap chain. Project testing and multithreading docs now describe the live two-gate CI topology and conditional renderer coverage.
- Sync-flow analysis found four stale auth-wrapper `SyncScope` promises, four missing Irvin quest lifecycle reproofs in production code, one missing tunnel-location cover, and the corresponding Irvin lifecycle boundaries in the registered gameplay test. The source now proves those contracts explicitly; six stale production entries and the complete touched Irvin callback owner were removed from their baselines instead of accepting new debt.

Mechanical checks:

- Focused image-format documentation tests passed 7/7, aggregate contract-diff tests 9/9, AI-delivery tests 8/8, and standalone-validator tests 37/37. Complete `test_docs*.py` discovery passed 172 tests; `docs_validate.py` validates 129 maintained Markdown entries.
- Image, API, reference, inventory, site, and AI artifacts were regenerated in dependency order and pass their freshness checks. The API model contains 950 methods, 133 properties, 121 events, and 266 settings. The site model contains 73 navigation items, 121 searchable documents, 123 public routes, and 117 planned redirects; its search artifact is 798,919 bytes against the 1 MiB limit.
- AI delivery contains 123 public documents; `llms-full.txt` is 1,069,024 bytes against the reviewed 1.25 MiB (1,310,720-byte) limit. Aggregate comparison against the opening Engine revision reports bootstrap status across all 12 domains with zero required or missing dispositions.
- The rebuilt final-revision `LF_UnitTests` target passes all 346 test cases and 356,278 assertions. This includes `ImageBaker` with 745 assertions, `TextureAtlasSpaceNode` with 40, seven `NetworkServer*` tests with 47, and `ServerDisconnectsPreLoginConnectionAfterLoginTimeout` with 39; earlier incoming compressor and malformed-client-input regressions pass another 12 and 6 assertions.
- Production sync-flow checks analyze 6,925 functions after excluding gameplay-test scripts during collection and accept all 1,685 current diagnostics against a reduced 1,388-entry baseline with zero new or stale entries. The isolated testing-callback gate still collects the complete 10,367-function graph and accepts all 4,881 current diagnostics against its reduced 3,395-entry baseline, also with zero new or stale entries; all 113 analyzer tests pass.
- The complete MCP adapter suite passes 1,633/1,633 tests. The reconciled exception-safety audit checks 5,274 functions with zero errors or warnings after a structured three-way baseline merge retained current upstream inventory plus reviewed local classifications.
- Last Frontier `ForceBakeResources` completed full rebuilds during dependency reconciliation for version `0.3.554` and again on final Engine `65dfb851c` for version `0.3.555`. Both include 11,864 CommonArt files, 1,429 InterfaceArt files, 818 CrittersArt files, scripts, prototypes, texts, 64 model-info files, and all 612 maps. The focused quest, encounter, dialog, and tunable-setting run passed 6/6, both reconnect/auth covers passed 2/2, and the final Engine-tip tunnel/Irvin rerun passed 2/2 with zero failures, timeouts, skips, or global exception delta.
- The project curl TLS source gate passes with peer verification enabled and strict host verification (`VERIFYPEER=1`, `VERIFYHOST=2`). The reconciled dependency-refresh plan now makes host MSVC an explicit complementary validation lane for future allocator and vendored-header updates.
- Closing fetches report Last Frontier `819e45ec2b0df61c4bc172560cf7894a74357cda` and Engine `65dfb851c7b111e9e613be98c6bc1d49d3d61145` at `0 0` against upstream. The root gitlink deliberately remains `f667a85d99394071e6edcb2501371e1c2f6b07c5`; staged and unmerged sets are empty, both `git diff --check` runs pass, all twelve task safety stashes were removed by verified hash, and the two older root plus two older Engine stashes remain untouched.

Follow-up:

- Continue the remaining authored-format program with particles, GUI, and dialogs from their owning parsers, bakers, runtime modules, and tests.
- Add a minimal public image/FOFRM example only after the first example repository is owner-approved; keep modern authored assets inspectable and legacy binary fixtures narrowly licensed and provenance-audited.
- Confirm the landed twelve-domain diff, documentation-site, AI-delivery, focused native, and `jekyll-build-pages` artifacts in GitHub Actions; production Pages source confirmation remains administrator work.

## 2026-07-19 - particle format reference and update reconciliation

Scope:

- Safe Last Frontier update from `819e45ec2b0df61c4bc172560cf7894a74357cda` to `d583aba4f59840133fa1d78f6aa563a0bbd8930c` and Engine update from `65dfb851c7b111e9e613be98c6bc1d49d3d61145` to `42b038cf5ac60f3533e8a760505da95078a900c5`, with named root and Engine `include-untracked` safety stashes retained through reconciliation and validation.
- Independent reusable documentation for `.fopts` SPARK XML, registered graph objects, the stock editor, raw-copy baking, client caching and cloning, renderer fields, atlas/direct-scene/model integration, diagnostics, performance, and embedding-project responsibilities.
- Complete reconciliation of the incoming Engine EffectBaker warning boundary and the incoming Last Frontier Battalion/Lendale quest, test, documentation, and playthrough changes. Neither incoming range changes the reusable particle contract.

Source areas checked:

- Vendored SPARK object registration, descriptors, XML loader/saver, graph validation, reference handling, and the exact Engine-owned `SparkQuadRenderer` registration.
- `VisualParticles`, `SparkExtension`, `ParticleSprites`, `ParticleEditor`, `RawCopyBaker`, model-info baking/runtime, script integration, settings defaults, focused native tests, and current Last Frontier authored particle systems and textures.
- The complete incoming root and Engine ranges. Project quest behavior remains in Last Frontier docs/tests; the warning-suppression scope remains in Engine build and third-party maintenance ownership.

Results:

- Added [ParticleFormat.md](../ParticleFormat.md), `BuildTools/ParticleFormatInterface.json`, `BuildTools/docs_particle_format.py`, [generated/particle-format.json](../generated/particle-format.json), eight generated reference pages, and eight source-backed documentation tests.
- The generated model contains 96 entries, 37 object registrations, and 12 renderer fields. It derives the 36 SPARK core registrations plus the Engine renderer, proves editor parity, and rejects object, renderer, setting, source-anchor, and generated-output drift.
- Added `particle-format` as the thirteenth aggregate contract-diff domain and routed it through CI, standalone validation, manifest, navigation/search, AI delivery, generated-reference maintenance, public API notes, production planning, backlog, and Last Frontier authoring routes.
- Documented that `.fopts` is copied unchanged rather than compiled, the XML graph must contain exactly one `System`, failed base-system loads are path-cached, instances are deep-copied, one `draw in scene` renderer promotes the whole sprite, model-bone particles use a separate runtime route, and `blend mode` is currently declared but unused.
- The audit corrected adjacent model documentation drift: `.fope` was replaced with the live `.fopts` suffix in the model contract, guide, and focused native fixture. The reusable `ModelProjFactor` default is now documented as `40`; Last Frontier retains its project override of `32`.
- Last Frontier routes reusable mechanics to the Engine guide while retaining its seven particle systems, six textures, selected effects, shot/grenade/model/scene integrations, render overrides, performance policy, and visible acceptance gates.

Mechanical checks:

- Focused particle documentation tests pass 8/8. Complete `test_docs*.py` discovery passes 180 tests; `docs_validate.py` validates 138 maintained Markdown entries.
- Particle, model, inventory, site, and AI generated checks are current. The site model contains 75 navigation items, 130 searchable documents, 132 public routes, and 126 planned redirects; its search artifact is 832,870 bytes against the 1 MiB limit.
- AI delivery contains 132 public documents; `llms-full.txt` is 1,099,989 bytes against the reviewed 1.25 MiB (1,310,720-byte) limit. Aggregate comparison against the opening Engine revision reports bootstrap status across all 13 domains with zero required or missing dispositions.
- The rebuilt `LF_UnitTests` target passes focused `RawCopyBaker` (15 assertions), `ModelBakers` (16 assertions), and four `EffectBaker*` cases (106 assertions), followed by a successful complete native suite.
- Last Frontier `BakeResources` completes a clean full rebuild for version `0.3.555`, including 11,864 CommonArt files, 1,429 InterfaceArt files, 818 CrittersArt files, scripts, 64 model-info files, and all 612 maps. The reconciled quest run passes 275/275 tests across 14 suites with zero failures, timeouts, skips, or global exception delta; the quest suite itself passes 177/177.
- The complete AiControl MCP suite passes 1,645/1,645 tests. A visible Direct3D `DevTest` smoke run with `SceneParticlesDemo.Enabled = True` renders all four cursor-wave groups and confirms scene-depth occlusion; the local capture is `Workspace/ParticleDocsLive/direct3d.png`, and no temporary capture code remains in authored source.
- Ruby and Bundler are unavailable on this host, so local Liquid rendering could not run. GitHub Actions `jekyll-build-pages` remains the authoritative site-render and publication gate.
- Closing fetches report Last Frontier `d583aba4f59840133fa1d78f6aa563a0bbd8930c` and Engine `42b038cf5ac60f3533e8a760505da95078a900c5` at `0 0` against upstream. The root gitlink matches Engine HEAD, staged and unmerged sets are empty, both `git diff --check` runs pass, and the two task safety stashes were removed by verified hash while the two older root and two older Engine stashes remain untouched.

Follow-up:

- Continue the remaining reusable format/tooling program with GUI, dialogs, fonts, audio/video, and focused editor manuals from their owning source and tests.
- Publish the first owner-approved repository from [PublicExampleRepositories.md](../PublicExampleRepositories.md), then use its review feedback to refine the minimal particle example while preserving exact Engine pins and asset provenance.
- Confirm the landed thirteen-domain diff, documentation-site, AI-delivery, native/project checks, and `jekyll-build-pages` artifacts in GitHub Actions. Production Pages source confirmation remains administrator work.

## 2026-07-19 - font format and text layout reference and final update reconciliation

Scope:

- Safe Last Frontier update from `d583aba4f59840133fa1d78f6aa563a0bbd8930c` to `12c09d41ef9c9b163f54c5c43fa7ae5c3038f053` and Engine update from `42b038cf5ac60f3533e8a760505da95078a900c5` to `6307c716cda32ab857c7754be39130dd17d6ef46`, with eight named root/Engine `include-untracked` safety stashes retained through opening, closure, final, and post-final reconciliation.
- Independent reusable documentation for `.fofnt`, binary BMFont v3, source-image dependencies, raw-copy delivery, runtime slots, sprite/atlas/effect binding, scaling, borders, grayscale processing, inline colors, text layout flags, hit testing, diagnostics, and embedding-project responsibilities.
- Complete reconciliation of the incoming package DSL and Windows 7 build lanes, the atomic mesh-only/Ozz model subsystem, and the latest Last Frontier weapon-event, Gambell intro, transferable-loot, platform-build, raider-hunt, campaign-hunt recovery, map-feedback, death-movement, gameplay-test, and synchronization changes. Project behavior remains in Last Frontier owners; reusable packaging, model, and font behavior remain in Engine owners.

Source areas checked:

- `FontManager`, font script methods and enums, sprite and atlas dependencies, effect binding, resource loading, settings, built-in tests, and Last Frontier font assets, GUI bindings, scale settings, localization integration, and visible PDA use.
- `DefinePackage`, package generation, archive and WiX tests, CMake build stages, Windows architecture normalization, `check_windows7_imports.py`, and the complete final Engine range.
- `ModelSourceLoader`, `ModelAnimationConverter`, `ModelMeshData`, `ModelAnimationData`, `ModelManager`, `ModelHierarchy`, `ModelInformation`, `ModelInstance`, `ModelAnimation`, their focused native tests, and the complete post-final 3D/Ozz range.
- The complete final Last Frontier range, including weapon snapshots, intro scene progression, medical-history loot, Windows 7 CI/package integration, world-raider contracts, and the production plus testing-callback synchronization gates.

Results:

- Added [FontFormat.md](../FontFormat.md), `BuildTools/FontFormatInterface.json`, `BuildTools/docs_font_format.py`, [generated/font-format.json](../generated/font-format.json), eight generated reference pages, and nine source-backed documentation tests.
- The generated font model contains 57 contract entries, 13 `.fofnt` fields, and 9 BMFont rules. It distinguishes `.bmfc` authoring sidecars from runtime inputs, records signed BMFont metrics, derives the live `FontType` slots and layout flags, and keeps project typography and GUI composition outside the reusable contract.
- Added `font-format` as the fourteenth aggregate contract-diff domain and routed it through CI, standalone validation, manifest, navigation/search, AI delivery, generated-reference maintenance, public API notes, production planning, backlog, and Last Frontier integration routes.
- The source audit fixed two font-loader defects: empty `.fofnt` names are rejected before suffix inspection, and signed BMFont offsets/advances are preserved instead of being decoded as unsigned values. A visible Direct3D PDA check confirms regular, bordered, and scaled `Big` text; the local capture is `Workspace/FontDocsLive/pda.png`.
- Reconciled the live package contract to per-`BINARY POSTFIX`, removed the obsolete package-wide postfix option from the machine model, documented `win32-win7` and `win64-win7`, and added the PE import checker to the generated helper-CLI reference. Focused CMake tests prove that each binary postfix is stored independently.
- The exact preserved-baseline aggregate diff reports 15 changes across 14 domains: 9 package changes, 2 helper-CLI changes, and 4 model-format changes, with all 3 required migration dispositions satisfied and 0 missing dispositions.
- Last Frontier integration docs now route reusable font mechanics to the Engine owner while retaining project font names, scales, GUI placement, Russian-first localization, and visible acceptance policy. Final callback audits also drove explicit synchronization reproofs in the touched combat, faction-contract, quest, and intro tests rather than accepting new baseline debt.
- The post-final model reconciliation replaces deleted `3dStuff` / `3dAnimation` anchors with the live source owners and documents the coordinated `LFMODMSH` mesh, `LFMODINF` description, and required `LFOZZRIG` rig boundary. Direct FBX runtime fallback is not promised; `AllowAnimationGeometry` is documented as a temporary validation-only source-repair exception, and model/particle manifests, maintenance routes, generated references, site data, and AI delivery now follow the current modules.
- Repeated synchronization checks exposed late lifecycle defects rather than documentation drift. `Combat::ApplyDamage` now stops after a damage callback destroys its target, `ToDeadWithImpulse` also rejects destroying corpses, lethal-damage tests strictly re-prove their corpse handles, and embedded-client tests reacquire the current controlled critter after destructive reset helpers. Production baseline keys decreased by eight with one count shrinking from three to one; the callback baseline decreased by four. No new diagnostic was accepted.

Mechanical checks:

- Focused font documentation tests pass 9/9. Complete `test_docs*.py` discovery passes 189 tests; `docs_validate.py` validates 147 maintained Markdown entries.
- All fourteen structured domains, API/reference/inventory, site, and AI artifacts are current. The API model contains 941 methods, 133 properties, 121 events, 266 settings, and one explicit contract; the site model contains 77 navigation items, 139 searchable documents, 141 public routes, and 135 planned redirects. AI delivery contains 141 public documents.
- Package and Windows 7 tests pass 24 tests with one platform-dependent skip; all three CMake interface-contract validation projects pass. Python generator checks and the exact aggregate contract-diff gate pass.
- Last Frontier `CompileAngelScript` and `ForceBakeResources` complete cleanly for version `0.3.560`, including 65 model descriptions and all 612 maps; a following ordinary bake reports zero stale outputs in every category. `LF_Client`, `LF_ServerHeadless`, and `LF_UnitTests` rebuild cleanly on the Ozz/meshoptimizer toolchain. Focused model/Ozz coverage passes 63 test cases and 5,334 assertions; the complete native suite passes all 403 test cases and 361,305 assertions.
- Production synchronization analysis covers 6,932 functions and accepts all 1,650 diagnostics with zero new or stale entries. The isolated callback gate covers 10,393 functions and accepts all 4,856 diagnostics with zero new or stale entries; all 122 analyzer tests pass. The exception-safety audit checks 5,453 functions with zero errors or warnings, and the smart-pointer audit checks 463 files with no diagnostics.
- The complete AiControl MCP suite passes 1,672 tests plus 57 subtests and its static smoke gate passes. Seven focused gameplay cases pass with zero failures, timeouts, skips, or global exception delta: both death-movement scenarios, map-feedback look-distance and F5 embedded-client probes, Lendale Butchers, stale intro completion, and direct-damage event delivery. The earlier broad reconciliation evidence remains recorded above, while these focused runs are the authoritative post-final-refresh checks.
- Ruby and Bundler are unavailable on this host, so local Liquid rendering could not run. GitHub Actions `jekyll-build-pages` remains the authoritative site-render and publication gate.

Follow-up:

- Continue the remaining reusable authoring program with GUI, dialogs, audio/video, and focused editor manuals from their owning parsers, runtime modules, tools, and tests.
- Publish the first owner-approved repository from [PublicExampleRepositories.md](../PublicExampleRepositories.md), then use it to validate the minimal font/localization/GUI path against an exact Engine revision and provenance-reviewed assets.
- Confirm the landed fourteen-domain diff, documentation-site, AI-delivery, package/Win7, native/project, and `jekyll-build-pages` artifacts in GitHub Actions; production Pages source confirmation remains administrator work.

## 2026-07-19 - small-vector guidance and dependency-order reconciliation

Scope:

- Safe Last Frontier update from `12c09d41ef9c9b163f54c5c43fa7ae5c3038f053` through `e1fac76d5fb1c5486327f5a244fcfec343c99f0d` and `ee7bd89a7d19449c64b4dce6b9290356e95f12d0` to `c7f2c93247f283fa2a06acea4a55855c2ba3170a`, and Engine update from `6307c716cda32ab857c7754be39130dd17d6ef46` through `c01dbd04a1830d7ab5968fbc011128a48abe6a86` to `12e63fc4c4730719b7f4123ecdbdd783cda675fb`.
- Reconcile the Engine `small_vector` adoption, overlay repack-capacity correction, and baker output dependency ordering without promoting Last Frontier behavior into reusable contracts.
- Reconcile the project PlayerStart, personal-room terminal, open-hunt, Headhunters defection/recruiter routing, dynamic encounter, and Rvach movement-animation changes with their owning project documentation and tests.

Results:

- [Essentials.md](../en/reference/native/essentials.md) now owns `vector` versus `small_vector` selection, measured inline capacity, address invalidation on inline moves, exact-type boundaries, complete-element requirements, nested-NSDMI constraints, formatter behavior, exception caveats, and adoption validation. [Debugging.md](../Debugging.md) documents the automatically wired `small_vector.natvis`; [ExceptionSafety.md](../ExceptionSafety.md) distinguishes terminate-on-OOM allocation from element construction and move failures.
- `Test_Containers.cpp` now proves allocator identity, inline-to-heap growth, `inlined()` state, inline relocation, heap-buffer transfer, mixed-mode swap, and formatting. The focused `Containers` and `CommonHelpers` cases pass 25 and 46 assertions respectively.
- The incoming [EntityModel.md](../EntityModel.md) correction records that overlay capacity must be re-evaluated after a repack changes the aligned tail. The incoming [BakingPipeline.md](../BakingPipeline.md) correction records dependency-order output discovery, later-pack replacement, and reverse-priority file resolution. Both claims have focused native regressions in their owning source areas.
- Last Frontier `Docs/AiControl.md` now records that open hunts may match semantic quest roles without a fixed map prototype, search authored spawn waypoints on dynamic encounter maps, patrol relative to the target territory, explore before exiting, and preserve remembered maximum health while recovering a wounded target. It also owns the Headhunters recruiter routes, four-quest raider-hunt family, headquarters exclusion, and proactive-combat suppression during peaceful approaches; these remain project-runner behavior rather than reusable Engine contracts. The Rvach asset fix remains project-owned and is documented by its dedicated implementation plan.
- Full callback analysis exposed retained critter handles after destructive reset helpers in the terminal, PlayerStart, GUI initial-build, and new Rvach tests. Each callback now reacquires and re-locks the live entity instead of expanding the baseline. The project exception-safety baseline also re-derives the changed `BakerDataSource` constructor at its unchanged `Strong` level.

Mechanical checks:

- Complete `test_docs*.py` discovery passes 189 tests and standalone validation covers 147 maintained Markdown entries. All fourteen structured domains plus API, reference, inventory, site, route, and AI-delivery artifacts pass their freshness checks. The preserved-baseline aggregate diff remains 15 changes across 14 domains, with all 3 required dispositions present and 0 missing.
- Last Frontier `CompileAngelScript` passes on version `0.3.564`. `BakeResources` completes a full rebuild with 65 model descriptions and all 612 maps, then an ordinary bake reports zero outputs in every category. Both Rvach model reports contain `animation_data_issues=0`.
- The complete rebuilt native suite passes 406 test cases and 361,348 assertions. This includes the new overlay-growth case and both baker dependency/priority cases in addition to the focused container coverage.
- Production synchronization remains clean at 6,936 functions and 1,650 accepted diagnostics; the latest isolated callback gate covers 10,414 functions and accepts 4,846 diagnostics against 3,363 baseline entries, with zero new or stale diagnostics. All 122 analyzer self-tests pass. Exception-safety checks 5,453 functions with zero errors or warnings, and SmartPointerAudit checks 463 files with no diagnostics.
- The complete AiControl suite passes 1,678 tests and its static smoke gate passes. The latest two regressions cover Headhunters recruiter-to-hunt routing and peaceful-approach attack suppression. The Rvach embedded-client regression passes `idle=true walk=true run=true`; the immediately preceding reconciliation also passes two client terminal/initial-build scenarios and three PlayerStart exit scenarios, all with zero failures, timeouts, skips, or global exception delta.
- Final fetch parity is `0/0` for Last Frontier `c7f2c93247f283fa2a06acea4a55855c2ba3170a` and Engine `12e63fc4c4730719b7f4123ecdbdd783cda675fb`; the root gitlink matches Engine HEAD, staged and unmerged sets are empty, and `git diff --check` passes in both repositories. All thirteen safety stashes created by this documentation pass were removed while the four unrelated pre-existing stashes remain untouched.
- Ruby and Bundler remain unavailable on this host. GitHub Actions `jekyll-build-pages` is the authoritative Liquid render and publication gate for the Markdown site at `fonline.ru`.

Follow-up:

- Keep the broader `small_vector` gameplay matrix and before/after Tracy allocation captures open until they can run on a stable profiling host; the reusable semantic, native, audit, bake, and focused gameplay gates are complete.
- Continue the source-backed authoring program with GUI, dialogs, audio/video, and focused editor manuals, then publish the first owner-approved exact-pin example repository.
- Confirm the landed documentation, contract diff, site, AI delivery, native/project, and `jekyll-build-pages` artifacts in GitHub Actions. Production Pages source confirmation remains administrator work.

## 2026-07-20 - polygonal-sprite reconciliation and private example staging

Scope:

- Safe Last Frontier update from `c7f2c93247f283fa2a06acea4a55855c2ba3170a` to `c97c0a993aa1170afd26272cf421067c18ed29fd` across nine commits, and Engine update from `12e63fc4c4730719b7f4123ecdbdd783cda675fb` to `9d74c751f5684f80aef3b35a0eb16a8fabf9fa42` for `Polygonal sprites (#187)`.
- Complete reconciliation of the incoming sprite/model resource, baking, rendering, atlas, dependency, and setting contracts with the standalone Engine documentation corpus and minimal project.
- Administrator-authorized creation and initial private staging of the four planned `cvet/fonline-*` example repositories without claiming public release.

Results:

- [ImageFormat.md](../ImageFormat.md), [ModelFormat.md](../ModelFormat.md), [ModelAnimation.md](../ModelAnimation.md), [BakingPipeline.md](../BakingPipeline.md), [FrontendAndRendering.md](../FrontendAndRendering.md), [SourceTree.md](../SourceTree.md), and [ThirdPartyMaintenance.md](../ThirdPartyMaintenance.md) now cover SpriteResource v2, per-frame offsets and polygon meshes, SpriteInfo indexing, automatic model bounds/layout, renamed animation metadata, atlas diagnostics, and the Clipper2/earcut ownership boundary. Their structured models and source anchors were reconciled with the incoming source.
- The first exact-pin template smoke found that a standalone config did not materialize a valid `SpriteMesh.MaxTriangles` without explicit values. `Examples/MinimalProject/FOnlineStarter.fomain` now records all four disabled defaults, the format/baking guides document unconditional group validation, and focused tests prevent the scaffold from dropping them.
- The same smoke found that the candidate runner imported the locally developed but not-yet-pinned `BuildTools/docs_metadata.py`. The runner now strictly decodes the small paired metadata contract itself, checks both required calls on server/client evidence, and safely relays logs through constrained Windows console encodings. The richer catalog generator remains the owning documentation tool without becoming a standalone runtime dependency.
- `Examples/PublicRepositories.json` now separates source lifecycle from remote visibility/state and rejects any private repository described as published. Generated output omits private repository URLs. `cvet/fonline-project-template` is private and source-staged at `9946ca42c332a294f8fedd2732e7850a01c1ec27`; the other three private repositories are reserved at `97d232431488125b370be352fdcf28f66e6cbf4f`, `011dab0d07eef6387609821206b8ee534ec51c3f`, and `97823816ab333a62aced43edd4daafa19c5fee22`.
- The first clean Ubuntu workflow selected stock Clang 18, below the incoming Engine minimum of Clang 20. The second reached CMake with supported stock GCC but exposed the missing clean-host X11 package set. The Linux preset and focused tests now lock GCC 13+, while both governance workflows prepare versioned prerequisites through the checked-out Engine's own `prepare-workspace.sh linux-packages linux` command instead of duplicating package lists.

Mechanical checks so far:

- Model-format and image-format write/check passes complete with seven focused tests each; model-animation passes five focused tests. Public-example generation/check and its five focused tests pass with four private remotes, one source-ready candidate, and zero published repositories.
- The exact-pin external template validator passes for Engine `9d74c751f5684f80aef3b35a0eb16a8fabf9fa42`. Full Windows `python validate.py` passes on template commit `a2cdc06`, including configure, baker/server build, resource bake, headless lifecycle and native markers, and paired baked remote-call metadata; current `9946ca42` preserves that route and adds the Linux preset, prerequisite workflow, and corresponding prose.
- All 189 documentation tests pass, standalone validation covers 147 Markdown entries, and generated API/inventory report 941 export methods, 267 settings, and 93 native test files. The complete rebuilt native binary passes 422 test cases and 424,528 assertions.
- GitHub API verification confirms all four repositories are private, use `main`, and point at the expected local commits. `Pinned Engine` run `29739863448` on `9946ca42` passed on `windows-latest` and `ubuntu-latest`; manual `Current Engine Compatibility` run `29740066760` passed on Ubuntu against Engine `master`. Private staging still does not satisfy the public branch/security/tag/artifact gates.
- Final fetch parity is `0/0` for Last Frontier `c97c0a993aa1170afd26272cf421067c18ed29fd` and Engine `9d74c751f5684f80aef3b35a0eb16a8fabf9fa42`; the root gitlink matches Engine HEAD and unmerged sets are empty. Task safety stashes `25150f45` and `d064b811` were removed only after these checks, while the four older user stashes remain untouched.

Follow-up:

- Keep all repositories private until their recorded source, CI, branch/security, tag, artifact, and review gates pass. Populate the three reserved repositories in dependency order; do not turn reservation READMEs into public examples.

## 2026-07-30 - rendering, particle, package, and update reconciliation

Scope:

- Rebased Last Frontier from local `2608f229df474784464fee912338cfedb9301d6d` onto upstream `9a625c4e2666c76d4ebf60274453890fde90fead`, preserving seven local documentation commits at `571bca4549fe49a9bb2e369d3476eb4f06b74517` before reconciliation edits.
- Rebased Engine from local `3c75e62766c93f4376dfa9a7123f395aaf18025a` onto upstream `fee50fb636b5bd1e30509aded929df1fc0e95db5`, preserving fourteen local documentation commits at `fac978a67d1e601eb77389e8dc562d7e511705a0` before reconciliation edits.
- Audited the complete 20-commit Engine range and the 191-commit project range with safety branches `backup/docs-pre-update-20260730` retained in both repositories.

Source areas checked:

- Project-interface options, package targets, application/core-library wiring, native-extension consumers, viewer entry points, and the minimal starter.
- Effect state/resources, render buffers, particle backend composition, SPARK/Effekseer baker/runtime/tooling paths, measured bounds, sprite framing, and `.fo3d` model attachment validation.
- SQLite replacement, server/client lifecycle changes, same-stem mapper loading, animation/particle viewers, map-entrance preview generation, and project authentication reconnect integration.
- Generated API/inventory, all fourteen contract domains, Markdown/site routes, public-example registry, and AI delivery.

Results:

- Corrected the generated CMake model from retired `FO_DISABLE_UNQLITE` to `FO_DISABLE_SQLITE` and removed the no-op `FO_BUILD_EDITOR` option plus its two Last Frontier preset entries.
- Replaced the removed package `Editor` target with `AnimationViewer` and `ParticleViewer`, corrected their platform/pack matrices, and updated both Python and structural CMake gates.
- Documented effect depth/cull variants, `BackgroundTex`, and `ParticleSamplingBuf`; documented the expanded Effekseer geometry/material/depth/distortion profile and mandatory automatic particle bounds.
- Removed the last current `.fopts` raw-copy claims. The minimal starter no longer raw-copies `fopts`; SPARK authors `.spark` and references `.spk`, Effekseer authors `.efkproj` and references `.efk`; model and particle generated contracts now use the baked extensions.
- Corrected native-extension consumer inventories after removal of the generic Editor, documented focused viewer ownership, and synchronized the structural role/hook gate.
- Added project-owned map-entrance preview generation guidance and corrected the checkpoint cross-link. Updated stale authentication tests and current guidance from `PrepareExistingLoginCover` to `LockExistingRecordLoginCover`.
- Raised the reviewed `llms-full.txt` hard budget from 1.25 MiB to 1.5 MiB after the complete public corpus reached 1,318,732 bytes; no document was truncated or silently removed.

Mechanical checks:

- Complete Engine documentation discovery passes 190 tests; standalone validation passes for 148 maintained Markdown entries.
- Generated inventory/API are current at 952 methods, 133 properties, 121 events, 271 settings, one explicitly classified symbol, and 98 native test files.
- CMake, package, effect, particle, model, native-extension, public-example, site, and AI generators pass freshness checks. The site model contains 78 navigation items, 140 searchable documents, 142 public routes, and 136 planned redirects; AI delivery contains 142 public documents.
- The preserved-baseline aggregate comparator passes with 50 changes across 14 domains, 15 required dispositions, and 0 missing.
- All three structural CMake contracts pass. Focused native `ModelBakers` passes 19 assertions; `[particle]` passes 4,900 assertions across 28 cases.
- Last Frontier configures cleanly with MSVC 19.51, both particle backends, SQLite, and the two focused viewers. `BakeResources` passes for version `0.3.605`, including 65 model descriptions and all 612 maps. The `authentication` suite reports 29 total, 26 passed, 3 platform-dependent skips, 0 failures/timeouts, and zero reported global exception delta.

Residual:

- The parallel authentication worker logs an `EntitySyncException` during `ServerEngine::Shutdown` when the shutdown context reaches an unlogged player that was not captured by the whole-world registry lock. The runner still exits zero. This is a runtime/test-harness defect, not a documentation failure, but the authentication run must not be described as exception-clean until the shutdown path captures that entity correctly and a regression proves it.
- Local Liquid rendering was not run. GitHub Actions `jekyll-build-pages` remains the authoritative production Jekyll/Pages gate.

Follow-up:

- Fix and regress the parallel-worker shutdown synchronization defect separately from this documentation reconciliation.
- Continue the production program with GUI/dialog/audio/video/tool manuals, playable tutorials, the remaining private examples, production Pages/accessibility verification, visual teaching assets, Russian mirror/parity tooling, and AI task evaluations.
- Keep both `backup/docs-pre-update-20260730` branches until the reconciliation is reviewed and landed.

## 2026-07-30 - audio contract and project-boundary pass

Scope:

- Audited current audio resource indexing, raw-copy delivery, WAV/ACM/Ogg
  decoder paths, script entry points, SDL conversion/mixing, repeat behavior,
  silent/headless paths, native tests, and Last Frontier integration.
- Added an independent Engine-owned authoring/runtime guide and a separate
  Last Frontier guide for its concrete music, ambient, recipient, asset,
  provenance, and audible-acceptance policy.
- Added audio as the fifteenth generated contract-diff domain without treating
  project scripts or assets as reusable Engine proof.

Results:

- [Audio.md](../Audio.md) now owns the reusable accepted-format boundaries,
  raw-copy pack contract, effect-name normalization and numbered variants,
  exact-path music replacement, repeat timing, frontend conversion/mixing,
  disabled/headless semantics, diagnostics, project boundary, and maintenance
  triggers.
- `BuildTools/AudioInterface.json` and `BuildTools/docs_audio.py` produce 32
  stable entries across three formats plus six generated pages. Focused tests
  pin live source derivation, unsupported-extension rejection, manifest
  freshness, routing, and aggregate comparison.
- `SoundManager::Load` now rejects and logs an explicit unsupported suffix
  before an undecoded empty sound can be enqueued. Its re-derived
  exception-safety level remains `Strong`.
- The Last Frontier integration guide records its client-only sound pack,
  Ogg-music/WAV-effect use, state transitions, ambient emitters, recipient
  filtering, concrete integration points, user volume controls, and visible
  validation. It also records the current lack of a colocated sound-asset
  provenance ledger instead of inferring redistribution rights.

Mechanical checks:

- All 199 `test_docs*.py` tests pass. This includes 9 focused audio tests and
  all 37 standalone-validator fixture scenarios with the fifteenth model.
- Standalone validation passes for 155 maintained Markdown entries. Site
  delivery contains 80 navigation items, 147 searchable documents, 149 public
  routes, and 143 planned redirects. AI delivery contains 149 public documents
  and 1,341,822 full-context bytes.
- The preserved-baseline comparator reports 50 changes across 15 domains, all
  15 required dispositions present, and 0 missing.
- `LF_UnitTests` builds cleanly; `RunUnitTests` passes 464 test cases and
  430,113 assertions. Exception-safety checks 6,096 functions with 0 errors;
  three pre-existing manual-review notes remain warnings.
- `git diff --check` passes in both repositories.

Residual:

- No focused native `SoundManager` codec/playback fixture exists. Raw-copy
  coverage, source-backed checks, and the full headless native suite do not
  prove device conversion, heard output, transition loudness, or platform
  codec behavior; a visible audible client pass remains required for future
  audio behavior or asset changes.
- The production documentation program remains active. Continue with
  GUI/dialog ownership, video and focused tool manuals, playable tutorials,
  private example completion/publication gates, production Pages/accessibility,
  teaching visuals, Russian mirror/parity, and AI task evaluation.

## 2026-07-30 - experimental video contract and project adoption boundary

Scope:

- Audited `VideoClip`, fullscreen client ownership, script exports, texture
  drawing, raw-copy settings, Theora/Ogg build wiring, native-test inventory,
  Last Frontier resources/config/scripts, and TLA integration evidence.
- Added an Engine-owned experimental playback guide and a separate Last
  Frontier adoption guide without claiming that the project currently ships
  video.
- Added video as the sixteenth aggregate contract domain and routed it through
  standalone validation, GitHub Actions, site/search/routes, and AI delivery.

Results:

- [Video.md](../Video.md) now owns exact-path `.ogv` delivery, Ogg/Theora headers
  and frames, 1024-byte in-memory parser portions, the ten-stream state limit,
  CPU YCbCr-to-opaque-RGBA conversion, texture ownership, fullscreen
  replacement/queue/input/music/draw semantics, embedded `RenderIface`
  playback, diagnostics, loop risk, and project boundaries.
- `BuildTools/VideoInterface.json` and `BuildTools/docs_video.py` derive 34
  stable experimental `video.*` entries from live source and produce seven
  generated reference pages. Source anchors and derived values fail closed.
- The Last Frontier guide records the actual no-video state, then defines a
  staged dedicated-pack, catalog, server-authority/recipient, controller,
  skip/state, subtitle/localization, audio-restoration, budget, provenance, and
  visible-acceptance plan.
- TLA revision `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` is retained only
  as historical integration evidence: its separate pack and recipient wrappers
  are useful shapes, while its disabled intro and framebuffer/empty-texture
  warning are a regression lead rather than production proof.

Mechanical checks:

- All 208 `test_docs*.py` tests pass, including nine focused video tests and
  all 37 standalone-validator fixture scenarios with the sixteenth model.
- Standalone validation passes for 163 maintained Markdown entries. Site
  delivery contains 82 navigation items, 155 searchable documents, 157 public
  routes, and 151 planned redirects. AI delivery contains 157 public documents
  and 1,363,751 full-context bytes.
- The preserved pre-update baseline requires explicit missing-model bootstrap
  for the newly introduced audio/video models and reports 50 changes across 16
  domains, 15 required dispositions, and 0 missing.
- A corrected repository-root-aware project link check passes 549 local targets
  across 15 changed Markdown files. `git diff --check` passes in both
  repositories with only expected line-ending notices.

Residual:

- There is no focused native video decoder, queue, rendering, or loop fixture.
  Source-backed checks do not prove first frame, sustained motion, texture
  presentation, cleanup, audio drift, or platform behavior.
- `looped = true` reaches `Stop()` and `Resume()` at end-of-stream, but no test
  proves Ogg/Theora rewind, packet reset, frame-counter reset, or a clean second
  cycle. Keep looping experimental.
- Last Frontier has no `.ogv`, video pack, cinematic controller, subtitle
  track, or visible diagnostic scene. Keeping `ogv` in raw-copy extensions is
  not delivery or support evidence.

Follow-up:

- Add a tiny redistributable Theora fixture and focused native coverage where
  the decoder can be isolated, then build the Last Frontier diagnostic scene
  and controller before integrating any story cinematic.
- Continue the production program with GUI/dialog ownership, focused tool
  manuals, playable tutorials, private example completion/publication gates,
  production Pages/accessibility, teaching visuals, Russian mirror/parity, and
  AI task evaluation.

## 2026-07-30 - follow-up Last Frontier revision reconciliation

Scope:

- Fetched both repositories after the audio/video pass. Engine upstream
  remained at `fee50fb636b5bd1e30509aded929df1fc0e95db5`; Last Frontier
  advanced from `9a625c4e2666c76d4ebf60274453890fde90fead` to
  `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`.
- Preserved both dirty trees with named safety branches and stashes, rebased
  the seven local Last Frontier documentation commits, restored the fourteen
  local Engine documentation commits, and verified that the root gitlink and
  Engine checkout both resolve to
  `fac978a67d1e601eb77389e8dc562d7e511705a0`.
- Audited all nineteen incoming project commits. Seventeen affect only the
  operations journal; the two source-bearing commits own a gameplay
  documentation truth pass and the `AK47Knife` price/budget correction.

Results:

- The incoming truth pass and current source agree on 63 craft definitions,
  ten reachable recipe manuals, completed-iteration craft XP semantics, the
  current intro quest source, and `Properties.md` as a maintained reference
  rather than the exhaustive generated schema.
- `AK47Knife` now costs 115 Coins, uses the same 170% trader-budget tier as
  `AK47`, and is no longer a cost/power baseline exception. The focused sibling
  regression passed 1/1 with `AK47=262`, `AK47Knife=287`, and no global
  exception delta.
- Incremental `BakeResources` passed without warnings for Last Frontier
  `0.3.605`; `ProtoAudit` produced an empty report (`TOTAL_ISSUES=0`).
- All 208 Engine documentation tests and standalone validation for 163
  maintained Markdown entries pass. Site and AI delivery remain deterministic
  at 82 navigation items, 155 searchable documents, 157 public routes, 151
  redirects, and 157 public AI documents.
- Project link and anchor validation passes 1,099 local targets across 67
  locally changed or incoming-audit documents. No merge conflicts or
  root/Engine gitlink mismatch remain.

Residual:

- The safety stashes and branches remain intentionally retained until review
  and landing. This revision update closes only the synchronization slice; the
  production documentation program continues with GUI/dialog ownership and the
  other planned deliverables.

## 2026-07-30 - GUI runtime contract and project-generator boundary

Scope:

- Audited `CoreScripts/Gui.fos`, `Input.fos`, native client input/render
  dispatch, the Engine tutorial boundary, Last Frontier `.fogui` sources,
  `.foguischeme`, Python generator, generated module, and `ClientMain.fos`.
- Classified the reusable AngelScript GUI runtime as Engine-owned and the
  current declarative format, editor, generator, screen catalog, styles,
  assets, and concrete integration as Last Frontier-owned.
- Kept Engine at `fac978a67d1e601eb77389e8dc562d7e511705a0` and the project
  gitlink at the same revision; no newer upstream Engine commit appeared during
  this slice.

Results:

- [GuiRuntime.md](../GuiRuntime.md) now documents the reusable object model,
  screen registration/stack/lifecycle, layout, drawing, focus, mouse/keyboard
  input, drag/drop, grids/item views, project hooks, touch boundary,
  diagnostics, and validation without claiming an Engine `.fogui` parser.
- `BuildTools/GuiRuntimeInterface.json` and
  `BuildTools/docs_gui_runtime.py` produce 82 stable experimental entries, 12
  runtime types, 159 documented members, 39 callbacks, 31 screen API
  overloads, 8 metadata annotations, one canonical JSON model, and seven
  generated reference pages.
- GUI runtime is the seventeenth aggregate contract domain. The preserved
  pre-update baseline reports 50 changes, 15 required dispositions, and 0
  missing; audio, video, and GUI runtime remain explicit bootstrap domains.
- The source API inventory now records the live
  `MessageBoxType[] MessageTypes` type and the integration-critical
  `SetHasOnDraw(bool)` method.
- Last Frontier's `GuiSystem.md` routes reusable behavior to Engine and keeps
  project authoring/generation ownership. Its generator now emits the live
  `void OnMove(ipos deltaPos) override` signature, with a focused regression.

Mechanical checks:

- All 217 Engine `test_docs*.py` tests pass, including nine focused GUI tests
  and all 37 standalone-validator fixture scenarios with the seventeenth
  model. The full project `test_generate_gui_screens.py` suite passes 34/34.
- Regenerating all 98 Last Frontier scheme entries leaves
  `Scripts/GuiScreens.gen.fos` byte-identical at
  `CC42BAF4706095947CE02FF14D4FA9E275E53E73D9274821BDE1F3FF665AD581`.
- Standalone validation passes for 171 maintained Markdown entries. Site
  delivery contains 84 navigation items, 163 searchable documents, 165 public
  routes, and 159 planned redirects. AI delivery contains 165 public documents
  and 1,389,102 full-context bytes.

Residual:

- Engine has no self-contained native fixture that constructs the CoreScripts
  GUI runtime, renders pixels, drives input, changes resolution/language, and
  proves teardown. The complete surface remains experimental.
- Stock `Input.fos` bridges mouse, keyboard, and input-loss events; native touch
  events require project adaptation and visible proof.
- The current Last Frontier generator/editor are project-owned. Reclassify
  them only if reusable code is deliberately promoted to Engine or a companion
  repository.

Follow-up:

- Continue with dialog ownership and focused editor/tool manuals, then use a
  future standalone client example to close the GUI pixel/interaction fixture
  gap.
- Retain the existing safety branches and stashes until review and landing.

## 2026-07-31 - dialog ownership and project-tool manuals

Scope:

- Fetched the Last Frontier and Engine remotes, confirmed both current
  checkouts remain zero commits behind their tracked upstream branches, and
  audited the complete live dialog stack.
- Inspected the project `COMMON` parser/data model, `BAKER` validation/text
  output, script runtime, authored packs, visual editor, source audit, CI
  wiring, and canonical project documentation.
- Compared that source with the standalone Engine tree before deciding whether
  `.fodlg` could be documented as a FOnline format.

Results:

- Engine contains no `.fodlg` parser, dialog baker/runtime, or visual editor.
  The current Last Frontier implementation is formally classified as
  project-owned until reusable code, fixtures, tests, and compatibility policy
  are deliberately promoted to Engine or a versioned companion.
- [Embedding FOnline](../en/how-to/build/embedding-project.md) and
  [NativeExtensions.md](../NativeExtensions.md) now document how project-owned
  game-system formats compose with Engine without becoming Engine contracts.
  The production plan and backlog no longer leave dialog ownership undecided.
- Last Frontier's canonical dialog guide now names the parser/baker/runtime/
  editor/audit ownership boundary. Separate `DialogEditor` and `DialogAudit`
  operator manuals cover build, migration, round-trip risk, validation, CI,
  and rule-extension policy.
- The project audit previously interpreted only one trailing `*` answer
  collision marker while the native parser and editor accept repeated markers.
  The audit now strips all trailing markers for link validation, guarded by a
  seven-test regression suite and both project validation workflows.
- No eighteenth aggregate Engine contract domain was added: there is no
  Engine-owned dialog implementation from which to derive one. The honest
  reusable deliverable is the extension/ownership pattern, while the concrete
  schema remains with its source owner.

Mechanical checks:

- All seven dialog-audit unit tests pass.
- Active dialog audit: 143 packs, 0 errors, 12 non-failing orphan warnings.
  Archival audit: 136 packs, 0 errors, 269 non-failing orphan warnings.
- All 217 Engine `test_docs*.py` tests pass. Standalone validation passes for
  171 maintained Markdown entries.
- Site delivery remains deterministic at 84 navigation items, 163 searchable
  documents, 165 public routes, 159 planned redirects, and 1,036,237 search
  bytes. AI delivery contains 165 public documents and 1,391,707 full-context
  bytes.
- Last Frontier local-link/anchor validation passes 598 targets across 21
  changed or untracked Markdown files. Root and Engine `git diff --check`
  pass; Engine reports only the existing line-ending notices.

Residual:

- A reusable dialog companion is not yet designed or published. Extraction
  must move implementation and tests with the docs and define exact Engine
  compatibility; copying the project guide alone would create a false public
  contract.
- The remaining production program still includes broader tool manuals with
  versioned screenshots, playable tutorials, publication/accessibility,
  physical locale mirrors, translation parity, and AI evaluation.
- Existing safety branches and stashes remain retained until review and
  landing.

## 2026-07-31 - generated cross-domain public contract index

Scope and source reconciliation:

- Fetched Last Frontier `origin/main` and Engine `origin/master`. The project
  remains at `d54645eda827c4d4ade75e4f542cf6b7c8f9682f`, seven commits ahead and
  zero behind `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`. Engine remains at
  `fac978a67d1e601eb77389e8dc562d7e511705a0`, fourteen commits ahead and zero
  behind `fee50fb636b5bd1e30509aded929df1fc0e95db5`. Both incoming ranges are
  empty.
- Re-audited the root `PUBLIC_API.md`, ADR-0002, all seventeen generated
  contract models and human indexes, aggregate contract-diff ordering,
  manifest ownership, site/AI routes, and the documentation workflow.

Implemented and corrected:

- Added `BuildTools/docs_public_api.py` and six focused tests. The generator
  validates each live model through the aggregate comparator, requires its
  generated human index, preserves the owning stability label, and renders
  the root public contract entry deterministically.
- Replaced the historical placeholder with a 6,047-byte current index over
  all seventeen domains. The current scope summary is thirteen experimental
  and four internal domains. Native status is generated from source-backed
  data: 2,472 symbols, 601 described, 1 explicitly classified, and 2,471
  inheriting the default `internal` classification.
- Declared the output, all source models, and all source references in the
  documentation manifest; moved the stable route to `current`; added it to
  navigation and AI routing; and made aggregate validation require exact
  content plus focused-test and `--check` workflow markers.
- Updated ADR-0002, the maintenance playbook, build-tool guide, documentation
  index, production plan, and backlog. The policy now says explicitly that a
  generated inventory does not promote reachable symbols to a supported API.

Mechanical evidence:

- `test_docs_public_api.py` passes all 6 tests. Complete `test_docs*.py`
  discovery passes all 294 tests; standalone validation passes 187 maintained
  Markdown entries.
- Snippet validation passes 256/256 normative blocks, 141 evidence blocks,
  and 147 real Bash/PowerShell parser checks. The root generator's `--check`
  mode and the aggregate documentation validator both pass.
- Localization reports 0/180 current translations. Site data contains 101
  navigation items, 180 searchable documents, 181 routes, and 174 planned
  redirects in a 1,020,945-byte search index.
- AI evaluation passes 15 tasks and 30 retrieval checks with 83.3 percent
  top-1, 100 percent top-3, and 0.911 MRR. AI delivery contains 181 documents;
  `llms-full.txt` is 1,569,993 bytes against the 1,572,864-byte hard limit.

Residual:

- This Windows environment has neither Ruby/Bundler nor Docker, so a fresh
  Jekyll artifact and complete Chromium route audit could not be rebuilt for
  this route-state change. The source/site/layout/artifact/browser focused
  tests pass, but the required GitHub Pages build job must supply the rendered
  evidence after landing.
- The full AI context has only 2,871 bytes of remaining capacity. The next
  material documentation addition must deliberately remove or exclude
  redundant context before regeneration rather than raising the production
  cap casually.
- Broad source-owner stability classification, 180 reviewed Russian
  counterparts, independent model-family evaluation, owner-gated public
  example publication, production Pages-source confirmation, and manual
  assistive-technology/zoom review remain open.
- Existing safety branches and stashes remain retained until review and
  landing.

## 2026-07-31 - interactive Mapper and particle-authoring manuals

Scope and source reconciliation:

- Fetched Last Frontier `origin/main` and Engine `origin/master`. The project
  remains at `d54645eda827c4d4ade75e4f542cf6b7c8f9682f`, seven commits ahead and
  zero behind `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`. Engine remains at
  `fac978a67d1e601eb77389e8dc562d7e511705a0`, fourteen commits ahead and zero
  behind `fee50fb636b5bd1e30509aded929df1fc0e95db5`. Both incoming ranges are
  empty.
- Audited Mapper menus, windows, selection/history/save behavior, Particle
  Preview, the SPARK source browser/editor, focused viewers, both particle
  backends, baked sprite loading, ImGui frame composition, and existing
  project-owned Last Frontier map/particle workflows.

Implemented and corrected:

- Added [Mapper Manual](../MapperManual.md) and
  [Particle Authoring Tools](../ParticleAuthoringTools.md), then routed them
  through human and AI indexes, maintenance triggers, project integration
  pages, deterministic retrieval evaluation, and the production plan.
- Added a source-owned screenshot contract, generator, focused tests, and two
  accessible `1280x800` Direct3D captures. The independent
  `Examples/MinimalMultiplayer` fixture now supplies a looping SPARK source,
  an MIT-provenance texture, a deterministic Mapper profile, and deferred
  full-window capture.
- Added `RequestMapperWindowScreenshot` while preserving the synchronous
  map-only `SaveMapperScreenshot` contract. The frontend can render the current
  ImGui frame into a texture before present, restores render/scissor state on
  failure, and rejects empty output paths. The SPARK editor now reads baked
  sprite resources through the canonical versioned parser instead of a stale
  private record layout.
- Added strict XML snippet parsing and a reviewed full-context exclusion list
  for redundant routing pages so complete AI documents remain under the
  `1.5 MiB` delivery budget without truncation.

Runtime and mechanical evidence:

- Windows Release builds of `FOMM_Mapper` and `FOMM_ParticleViewer` pass.
  `ForceBakeResources` bakes the SPARK fixture, and the deterministic Mapper
  profile exits successfully after writing the full-window TGA. Its converted
  PNG is byte-identical to the checked capture with SHA-256
  `f5834f6fbba987d5b9eaa8108437ea5c0ff1a29a2e0aeb73b05871bdec82f404`;
  the SPARK editor capture remains
  `168ed7aecf787293778be0e844590925227d76ec52583c07065aeb7c7bf5f9be`.
  The official `Examples/MinimalMultiplayer/validate.py` Windows lane also
  passes configure/build, bake, content validation, remote calls, replicated
  persistent state, map load, and item-interaction smoke.
- Smart-pointer audit passes 495 files with zero findings. Exception-safety
  audit passes 6,104 functions with zero errors and three retained
  manual-review warnings. The aggregate contract comparator reports 51
  changes across 17 domains, all 15 required dispositions satisfied, and the
  new Mapper method as an additive internal API.
- Complete `test_docs*.py` discovery passes all 285 tests; standalone
  validation passes 187 Markdown entries. All generated artifacts are current:
  253/253 normative snippets, 141 evidence blocks, 144 real shell-parser
  checks, 0/179 translations, 181 public AI documents in 1,560,796 bytes, and
  15 AI tasks with 30/30 retrieval checks, 83.3 percent top-1, 100 percent
  top-3, and 0.911 MRR.
- The pinned Ruby `3.3.4`/`github-pages` `232` production build passes 181
  routes, 40 static endpoints, and 22,056 local references. Playwright
  `1.62.0`, Chromium `151.0.7922.34`, and axe-core `4.12.1` pass all 181
  desktop and 181 mobile pages, three interaction profiles, and five
  screenshots. All 19 desktop and 6,916 mobile raw contrast-incomplete nodes
  pass the computed fallback with no failed or unresolved nodes; minimum
  ratios are 5.009:1 and 5.779:1.

Residual:

- The full native FOMM suite passes 369 of 370 test cases. The remaining
  `MapReentrantEvents` case has four reproducible failures in
  `Test_ServerMapOperations.cpp`; it does not exercise the Mapper capture or
  particle-authoring paths and remains a separate native regression to fix
  before claiming a completely green Engine test run.
- Full compile-database local-variable analysis remains unavailable in this
  Windows configuration; both analyzer self-test suites pass, and the touched
  native code was compiled by the Mapper and ParticleViewer targets.
- Production Pages-source confirmation, landed CI artifact review, manual
  assistive-technology/zoom review, 179 reviewed Russian counterparts,
  independent model-family evaluation, and owner-gated example repository
  publication remain external or later-phase gates.
- Existing safety branches and stashes remain retained until review and
  landing.

## 2026-07-31 - deterministic public-example candidate staging

Scope and source reconciliation:

- Fetched Last Frontier `origin/main` and Engine `origin/master` again. The
  project remains seven commits ahead and zero behind
  `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`; Engine remains fourteen
  commits ahead and zero behind
  `fee50fb636b5bd1e30509aded929df1fc0e95db5`. Both incoming ranges are empty.
- Audited the canonical public-example registry and governance overlay, the
  private `cvet/fonline-minimal-multiplayer` reservation, the already staged
  project-template repository, exact Engine gitlink behavior, source asset
  provenance, and the current minimal multiplayer source.

Implemented:

- Added non-destructive `docs_examples.py --stage-repository` materialization.
  It accepts only a source-ready registry entry, requires an exact revision
  equal to a clean checkout contained by a fetched remote-tracking branch,
  refuses an existing or nested output, excludes declared local/generated
  paths, rejects source links/junctions, preserves the deep source README as
  `TUTORIAL.md`, renders the common overlay, and aligns metadata plus
  Engine-owned provenance URLs to one pin.
- Added explicit display names, primary checks, and source exclusions to the
  registry. The shared overlay now owns `.gitattributes` and `.gitmodules`;
  the concise README routes readers to `TUTORIAL.md`.
- Strengthened provenance validation to hash the actual asset bytes instead of
  checking only digest syntax and file existence. Added regression coverage
  for clean staging, exclusions, placeholder elimination, pin rewriting,
  output reuse, non-ready sources, dirty/mismatched/unpublished Engine state,
  a substituted `.gitmodules` URL, and tampered asset bytes.
- Updated reusable and Last Frontier maintenance policy so every future Engine
  update rematerializes affected source-ready candidates only after the Engine
  commit is landed and fetchable, then runs pinned/current checks before a
  private push or any visibility transition.

Evidence:

- Focused public-example tests pass 8/8. Complete documentation discovery
  passes 288 tests; standalone validation passes 187 maintained Markdown
  entries. Snippet validation passes 256/256 normative blocks, 141 evidence
  blocks, and 147 external-parser checks.
- Localization reports 0/179 current translations. Site data contains 100
  navigation items, 179 searchable documents, 181 routes, 174 planned
  redirects, and a 1,017,783-byte search index. AI evaluation passes 15 tasks
  and 30 retrieval checks with 100 percent success and 0.911 MRR; AI delivery
  contains 181 documents in 1,562,497 bytes.
- A real isolated candidate produced 13 authored source files plus the common
  overlay, committed `Engine/` as gitlink
  `fee50fb636b5bd1e30509aded929df1fc0e95db5`, and passed the current structural
  repository validator, including the Radiation asset byte digest.
- Its actual `python validate.py` run failed during `BakeResources` exactly at
  `MapperCapture.fos`: published `fee50fb63` has no
  `Game.RequestMapperWindowScreenshot`. That checkout also predates
  `Engine/BuildTools/docs_examples.py`. This is the expected publication gate,
  not evidence for a releasable candidate.
- Engine and project `git diff --check` pass. No main working-tree file was
  staged, committed, pushed, or reverted by this checkpoint.

Residual:

- Do not push `fonline-minimal-multiplayer` from the published master pin. Land
  the pending Engine documentation, full-window Mapper screenshot API, source
  fixture, and validator first; fetch the resulting remote revision; ensure a
  clean checkout; rematerialize; run the structural validator plus the complete
  pinned and current Windows/Linux lanes; only then push to the existing private
  repository and change its registry state to `source-staged`.
- The project-template private candidate must receive the revised shared
  `.gitattributes`, `.gitmodules`, README/TUTORIAL, and byte-provenance contract
  through the same reviewed rollout. Public visibility, protected-branch
  settings, tags, artifacts, and releases remain separate owner gates.

## 2026-07-31 - complete rendered browser and automated WCAG gate

Scope and source reconciliation:

- Fetched Last Frontier `origin/main` and Engine `origin/master` again after
  the implementation. The project remains at
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f`, seven commits ahead and zero
  behind `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`. Engine remains at
  `fac978a67d1e601eb77389e8dc562d7e511705a0`, fourteen commits ahead and zero
  behind `fee50fb636b5bd1e30509aded929df1fc0e95db5`. Both incoming ranges are
  empty, so no source/API/content reconciliation was required.
- Audited the repository-owned layout, CSS, JavaScript, generated route
  catalog, production-compatible Jekyll artifact, static accessibility
  boundary, CI ordering, manifest ownership, and the remaining
  `docs-accessibility` production-plan gate.

Implemented and corrected:

- Added the private lock-file-pinned `BuildTools/docs-browser` harness with
  Node `24.16.0`, Playwright `1.62.0`, Chromium `151.0.7922.34`, and axe-core
  `4.12.1`. It serves only the completed `_site` tree on an ephemeral
  loopback port and emits a schema-versioned JSON report plus desktop,
  mobile-document, and open-mobile-navigation screenshots.
- Every generated current/available-locale route is checked at 1440 x 1000
  and 390 x 844 for the declared WCAG 2.2 A/AA axe tags, console/page/request
  failures, resource status, fixed-layout containment, and reachable
  page-level horizontal scroll. Separate keyboard scenarios prove the skip
  link, main focus, `Ctrl+K` and prose search, dialog Escape, theme
  persistence, copy feedback, mobile drawer semantics, focus containment,
  and focus restoration.
- Added mobile navigation accessibility state and focus ownership: the
  closed drawer is `inert` and `aria-hidden`, opening moves focus inside,
  `Tab` remains within the drawer/toggle cycle, and Escape closes the drawer
  and restores toggle focus. Search Escape is explicit instead of relying on
  browser-default dialog cancellation.
- Corrected defects found by the first complete run: table-of-contents and
  generated source-link targets now meet the 24 px WCAG 2.2 target; wide
  generated tables scroll inside the reading column on every viewport;
  genuinely scrollable tables receive keyboard focus; mobile code blocks no
  longer create page-level horizontal movement; and root page scroll is
  horizontally contained.
- Wired the focused structural test, exact dependency/lock validation,
  manifest browser policy, standalone validator, Jekyll job, setup/install
  steps, always-retained report/screenshots, maintenance routing,
  publication guide, generated-artifact guide, backlog, and production plan.
  Browser dependencies are excluded from the published Jekyll tree.
- Axe `incomplete` results are not suppressed or counted as conformance.
  The report records rule-level route/node totals with bounded example paths
  and targets. For clipped `color-contrast` nodes it also computes effective
  alpha-composited colors and WCAG luminance ratios; a failed or unresolved
  fallback fails the route.

Mechanical checks:

- The production-compatible static artifact audit passes all 177 routes, 32
  static endpoints, and 20,831 local references with zero errors.
- The complete Chromium matrix passes all 177 desktop and all 177 mobile
  routes: 354 rendered page/profile checks, zero axe violations, zero
  runtime/resource/layout error groups, both interaction profiles, and three
  retained screenshots.
- Axe reported 19 raw desktop and 6,904 raw mobile `color-contrast`
  incomplete nodes because their text was clipped inside scroll containers.
  The computed fallback passed all 6,923 nodes with zero failed/unresolved
  results; minimum observed ratios were 5.009:1 desktop and 5.779:1 mobile.
- Focused browser, layout, site, snippet, AI-evaluation, AI-delivery, syntax,
  and manifest tests pass. The complete documentation suite and final
  generated-delivery freshness checks are rerun after this evidence entry.

Residual:

- Automated axe and computed contrast coverage are not a screen reader and do
  not establish cognitive accessibility or production equivalence. Perform
  keyboard-only, 200 percent zoom, and representative screen-reader checks on
  the landed release artifact and `fonline.ru`.
- The local artifact does not prove the configured GitHub Pages
  branch/folder or the first landed browser-validation artifact. Repository
  administration must confirm both without changing the existing Markdown
  publication architecture.
- Physical English/Russian trees and 175 reviewed translations, visual
  teaching media, independent model-family evaluation, and owner-gated
  public-example releases remain open.
- Existing safety branches and stashes remain retained until review and
  landing.

## 2026-07-31 - first playable tutorial and source-ready multiplayer example

Scope and revision reconciliation:

- Fetched Last Frontier `origin/main` and Engine `origin/master`. The project
  remains at `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` with upstream
  `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`; Engine remains at
  `fac978a67d1e601eb77389e8dc562d7e511705a0` with upstream
  `fee50fb636b5bd1e30509aded929df1fc0e95db5`. Both ranges have no incoming
  commits, so no source contract reconciliation was required.
- Inspected the starter/example program, application settings startup,
  AngelScript CMake, content baking, remote-call metadata, client/server
  lifecycle, and CI routing.

Implemented and corrected:

- Added `Examples/MinimalMultiplayer`: desktop/headless clients, headless
  server, baker, one location/map, player, guide, item interaction, remote
  calls, synchronized persistent property, EN/RU text, content test, and
  deterministic multiplayer smoke.
- Added three tutorials and registered them plus the example README in
  navigation, search, planned EN/RU routes, and AI delivery.
- Promoted `cvet/fonline-minimal-multiplayer` to `source-ready` while retaining
  truthful private/reserved remote state. The generated public-example model
  now records two source-ready and two planned repositories without exposing a
  private URL.
- Added Windows/Linux `tutorial-smoke` CI routes. Disposable copies exclude
  local Engine/generated/cache/log paths before linking the reviewed checkout.
- Fixed ordinary application startup to apply declared Engine defaults before
  project config and overrides. The focused Settings regression protects a
  project override together with non-zero networking/frontend defaults.
- Corrected vendored AngelScript CMake assembler detection for current
  CMake/Visual Studio, retained `(FOnline Patch)` provenance, and added a
  structural regression. The real x64 MASM source assembles and links.
- Scoped GUI format detection and example README inventory to explicit
  Engine-owned trees, preventing project junctions from recursing into or
  redefining the public corpus.
- Search tokenization now drops pure numeric components that the top-level
  filter already treated as low-value noise, restoring useful index headroom.

Mechanical checks:

- `cmake --build --preset windows-check` passes from
  `Examples/MinimalMultiplayer`: build, bake, metadata symmetry, server content
  test, login, map load, localized text lookup, item collection, replicated
  count, and clean shutdown are green.
- `python BuildTools/buildtools.py validate win64-tutorial-smoke` passes the
  same route from a fresh disposable project copy using the shared CI entry
  point.
- Focused `FOMM_UnitTests.exe "Settings" --reporter compact` passes all 69
  assertions. `BuildTools.tests.test_angelscript_cmake` and Python bytecode
  compilation for the changed runners/tools pass.
- All 217 `test_docs*.py` tests pass. Standalone documentation validation
  passes for 175 maintained Markdown entries.
- Public-example, site, and AI artifacts regenerate deterministically: 88
  navigation items, 167 searchable documents, 169 public routes, 162 planned
  redirects, a 993,742-byte search index, and 169 AI-delivered documents in a
  1,410,280-byte full-context bundle.

Residual:

- `linux-tutorial-smoke` is wired but still needs clean GitHub runner evidence.
  The desktop target is built, while a visible pixel/input acceptance capture
  remains a publication gate rather than a headless-smoke claim.
- The multiplayer repository remains a private reserved shell. Governance
  overlay materialization, exact Engine gitlink, pinned/current CI, lesson
  tags, downloadable artifact, branch/security settings, and any visibility
  transition remain owner-gated.
- Pages source confirmation, accessibility, screenshots, physical EN/RU
  mirrors, translation parity, stable Markdown endpoints, and AI evaluations
  remain.

## 2026-07-31 - support matrix, project workflows, and localization freshness

Scope and source reconciliation:

- Rechecked Last Frontier `origin/main` and Engine `origin/master`. The project
  remains at `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` with upstream
  `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`; Engine remains at
  `fac978a67d1e601eb77389e8dc562d7e511705a0` with upstream
  `fee50fb636b5bd1e30509aded929df1fc0e95db5`. Both incoming ranges are empty.
- Audited BuildTools validation targets, CMake host/target/application gates,
  the complete documentation workflow, settings/config/resource loaders,
  generated source and baking stages, metadata outputs, updater/network/save
  boundaries, and the existing locale/route policy.

Implemented:

- Added a source-owned ten-profile support matrix plus checked JSON/Markdown
  projection. It separates source capability, required builds, process smoke,
  and project/device qualification instead of inferring support from adjacent
  CI lanes.
- Added standalone guides for project configuration, generated-content
  dependency order and recovery, and complete Engine-update adoption across
  contract diff, configuration/content, saves, gameplay compatibility,
  updater generation, frozen client ABI, rollout, validation, and docs.
- Added a 34-record English/Russian terminology glossary and a localization
  generator that records every required pair and normalized source hash.
  Existing translations now fail on stale/mismatched metadata, fenced-code
  drift, or links that abandon an available locale counterpart.
- Wired support and locale generation, focused tests, source declarations,
  freshness checks, site routes, AI delivery, and standalone validation into
  the manifest and documentation workflow.
- Search now omits numeric components and terms present in more than 60% of
  indexed documents. Technical identifiers remain weighted and searchable,
  while the browser artifact retains useful growth headroom below its 1 MiB
  fail-closed limit.

Mechanical checks:

- All 230 `test_docs*.py` tests pass. Bytecode compilation passes for the new
  generators and changed tests.
- Standalone documentation validation passes for 181 maintained Markdown
  entries.
- Support generation is current. Localization generation reports `0/173`
  reviewed Russian counterparts as current and all 173 missing, which is the
  expected honest pre-production state.
- Site delivery is deterministic at 94 navigation items, 173 searchable
  documents, 175 public routes, 168 planned redirects, and a 973,658-byte
  search index. AI delivery contains 175 public documents in a 1,463,287-byte
  full-context bundle.

Residual:

- Phase 9 remains open: the 173 Russian pages, reviewed API-description
  translation catalog, language switch, bilingual search/accessibility
  review, and production `--enforce-complete` gate are not implemented.
- Clean Linux evidence for the playable tutorial, public example repository
  promotion, the first observed Pages artifact and configured production
  source, durable migrated Markdown endpoints, versioned screenshots, and AI
  task evaluation remain production gates.
- Existing safety branches and stashes remain retained until review and
  landing.

## 2026-07-31 - rendered GitHub Pages artifact validation

Scope and source reconciliation:

- Fetched Last Frontier `origin/main` and Engine `origin/master` again. The
  project remains at `d54645eda827c4d4ade75e4f542cf6b7c8f9682f`, seven commits
  ahead and zero behind `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`.
  Engine remains at `fac978a67d1e601eb77389e8dc562d7e511705a0`, fourteen
  commits ahead and zero behind
  `fee50fb636b5bd1e30509aded929df1fc0e95db5`. No incoming source range
  required documentation reconciliation.
- Audited the pinned GitHub Pages stack, Jekyll configuration and plugins,
  manifest routes, generated navigation/search data, layout, public static
  endpoints, nested README handling, and the actual production-mode HTML
  artifact.

Implemented and corrected:

- Added `BuildTools/docs_site_artifact.py` and focused tests. The validator
  compares `_site` with the documentation manifest and route catalog, checks
  byte-identical public JSON/text endpoints, and emits
  `Workspace/docs-site-artifact-report.json`.
- Added rendered checks for HTML5 doctype, page language/title/viewport,
  canonical URL, one `main#main-content` and H1, skip navigation, image alt
  text, button names, duplicate IDs, local links/fragments/resources, locale
  routes, and search targets. GitHub Actions now runs the gate after Jekyll and
  uploads the report even when validation fails.
- Pinned the six manifest-owned nested README routes with explicit permalinks
  and narrowed `_config.yml` exclusions so GitHub Pages 3.10 renders them.
  Standard `index.md` pages now use Jekyll's canonical directory URL in
  navigation, search, planned locale routes, and the route catalog.
- Corrected a Liquid <code>&#123;&#123; content &#125;&#125;</code> collision
  in ADR 0004, several
  line-wrapped links that `jekyll-relative-links` left as `.md`, and the
  unstable generated anchor for `cast<T?>`.
- Updated publication, maintenance, generated-artifact, plan, backlog, root
  routing, and AI-maintainer guidance so rendered validation and same-change
  maintenance are part of the documented delivery contract.

Mechanical checks:

- A clean production-mode Jekyll build with Ruby `3.3.4`, `github-pages`
  `232`, and the locked Gemfile completes successfully. The post-build audit
  passes all 175 public routes, 30 static endpoints, and 20,250 local
  references with zero errors.
- All 238 `test_docs*.py` tests pass, including focused rendered-artifact,
  route, layout, workflow, permalink, canonical, accessibility, and negative
  fixture coverage.
- Site, localization, and AI outputs regenerate in dependency order. The site
  remains at 94 navigation items, 173 searchable documents, 175 public routes,
  168 planned redirects, and a 974,254-byte search index. AI delivery remains
  at 175 public documents in a 1,467,727-byte full-context bundle.

Residual:

- The local artifact proves compatibility with the pinned production stack; it
  does not prove the repository's configured Pages source or the live
  `fonline.ru` deployment. Confirm the first landed GitHub Actions artifact,
  branch/folder setting, DNS ownership, and production endpoints separately.
- Full browser keyboard/contrast/WCAG testing, executable snippet checks,
  migrated legacy-route behavior, physical EN/RU mirrors and language switch,
  173 reviewed Russian counterparts, versioned teaching media, and AI task
  evaluations remain open.
- Existing safety branches and stashes remain retained until review and
  landing.

## 2026-07-31 - AI retrieval evaluation and clean Markdown routing

Scope and source reconciliation:

- Fetched Last Frontier `origin/main` and Engine `origin/master` again. The
  project remains at `d54645eda827c4d4ade75e4f542cf6b7c8f9682f`, seven commits
  ahead and zero behind `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`.
  Engine remains at `fac978a67d1e601eb77389e8dc562d7e511705a0`, fourteen
  commits ahead and zero behind
  `fee50fb636b5bd1e30509aded929df1fc0e95db5`. Both incoming ranges are empty.
- Audited the production plan's AI-delivery gaps, manifest ownership,
  compact-search ranking in Python and the browser, generated AI routes,
  GitHub Pages/Jekyll 3.10 route constraints, and the checked publication
  artifact.

Implemented and corrected:

- Added [AiEvaluation.md](../AiEvaluation.md), the versioned
  `Docs/ai-evaluation.json` source, `BuildTools/docs_ai_eval.py`, focused
  tests, and the deterministic
  [generated/ai-evaluation-report.json](../generated/ai-evaluation-report.json).
  The source owns 12 tasks across architecture, scripting, content,
  debugging, migration, and release, with 24 retrieval checks plus current
  answer-evidence and forbidden-assumption checks.
- Added a shared `docs_site.search_documents` implementation and kept browser
  ranking equivalent. Each effective query token now votes at most once,
  absent terms do not raise the match threshold, and prefix postings are
  bounded before aggregation. The first real corpus run exposed and corrected
  the previous absent-token/repeated-prefix ambiguity.
- Raised AI-delivery schema to 2. Every public document now exposes a
  source-ref-pinned clean `markdown_url` while retaining its canonical
  `fonline.ru` HTML URL; `llms.txt` routes agents to Markdown and links HTML
  secondarily. Same-domain raw aliases remain open because the pinned Pages
  stack cannot produce them without duplicate authored content or a custom
  plugin.
- Wired the evaluation source/report through the manifest, navigation,
  localization inventory, AI delivery, standalone validation, CI, rendered
  endpoint validation, maintainer routing, publication guide, ADR, backlog,
  and production plan. The required generation order is localization, site
  search/routes, AI evaluation, then AI delivery.

Mechanical checks:

- All 242 `test_docs*.py` tests pass. Python bytecode compilation passes for
  the changed generators and tests.
- Standalone validation passes for 182 maintained Markdown entries.
- The deterministic evaluation passes all 12 tasks and 24 retrieval checks:
  79.2 percent at rank 1, 100 percent within rank 3, and 0.889 mean reciprocal
  rank, with zero stale owners, evidence checks, or forbidden assumptions.
- Current generated delivery contains 95 navigation items, 174 searchable
  documents, 176 public routes, 169 planned redirects, a 980,625-byte search
  index, 174 required Russian counterparts, and 176 AI-delivered documents in
  a 1,480,504-byte full-context bundle.
- A clean production-mode build with Ruby `3.3.4`, `github-pages` `232`, and
  the locked Gemfile passes the rendered audit for 176 routes, 31 static
  endpoints, and 20,539 local references. The machine report is retained at
  `Workspace/docs-site-artifact-report.json`.

Residual:

- Deterministic retrieval and current evidence do not prove final model
  answers. Run the complete isolated task set with at least two materially
  different model families and record exact versions, inputs, outputs,
  reviewer scoring, latency, and token use.
- Canonical human pages remain at `fonline.ru`; source-ref-pinned Markdown is
  served by GitHub raw content. A same-domain Markdown alias needs a
  non-duplicating capability in the actual publication platform.
- Executable snippet gates, full browser keyboard/contrast/WCAG review,
  production Pages-source confirmation, physical English/Russian trees and
  174 reviewed translations, visual teaching media, and public-example
  publication gates remain open.
- Existing safety branches and stashes remain retained until review and
  landing.

## 2026-07-31 - complete fenced-snippet parser coverage

Scope and source reconciliation:

- Fetched Last Frontier `origin/main` and Engine `origin/master` after the
  implementation. The project remains at
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f`, seven commits ahead and zero
  behind `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`. Engine remains at
  `fac978a67d1e601eb77389e8dc562d7e511705a0`, fourteen commits ahead and zero
  behind `fee50fb636b5bd1e30509aded929df1fc0e95db5`. Both incoming ranges are
  empty.
- Audited every fenced block in the manifest-owned public/current/human
  corpus, the existing localization fence parity check, production
  `docs-snippets` requirements, generated-artifact ownership, CI dependency
  order, and the rendered GitHub Pages endpoint set.

Implemented and corrected:

- Added [SnippetValidation.md](../SnippetValidation.md),
  `BuildTools/SnippetPolicy.json`, `BuildTools/docs_snippets.py`, focused
  tests, and the deterministic
  [generated/snippets.json](../generated/snippets.json). The inventory records
  document/heading/line ownership, language, contract, harness, normalized
  hash, template status, and result for every public fence.
- Added strict parsers for C-family delimiter/comment/string structure, CMake
  calls, FOnline INI/data sections and continuations including embedded
  shader bodies, JSON, Python, and text evidence. Untyped, unknown, empty,
  unclosed, malformed, or stale snippets fail.
- Added real Bash `-n` and PowerShell AST parsing with inert placeholder
  normalization. Commands are never executed. A dedicated
  `documentation-snippets` CI job owns the external parsers and is required
  before the rendered-site job.
- Classified five previously untyped call-flow/output diagrams as `text`.
  No ignore list or silent fallback exists; every fence is in the generated
  report.
- Wired snippet policy/report ownership through the manifest, standalone
  validator, AI delivery, rendered static endpoints, localization order,
  maintainer routing, publication guide, generated-artifact reference,
  production plan, and backlog.

Mechanical checks:

- All 248 `test_docs*.py` tests pass, including malformed-fence, every-harness,
  no-command-execution, stale-output, workflow, and standalone integration
  cases. Python bytecode compilation passes for the new tool and tests.
- Standalone validation passes for 183 maintained Markdown entries.
- The current report covers 175 public/current/human documents and 372 fenced
  blocks: all 231 normative blocks pass, 141 `text` evidence blocks pass, 79
  templates are explicit, and 125 Bash/PowerShell blocks pass real external
  parsers. Normative coverage is exactly 100 percent with zero errors.
- Current delivery contains 96 navigation items, 175 searchable documents,
  177 public routes, 170 planned redirects, a 985,243-byte search index, 175
  required Russian counterparts, and 177 AI-delivered documents in a
  1,491,574-byte full-context bundle.
- A clean production-mode Jekyll build with Ruby `3.3.4`, `github-pages`
  `232`, and the locked Gemfile passes all 177 routes, 32 static endpoints,
  and 20,831 local references. The byte-identical snippet report is present in
  the published artifact.

Residual:

- Lexical parser success does not replace semantic compile, bake, launch, or
  smoke evidence claimed by a particular guide. Existing owner tests remain
  required; complete source/tag binding for external examples remains part of
  the public-repository gates.
- Manual production keyboard/zoom/screen-reader review, production
  Pages-source confirmation, physical English/Russian trees and 175 reviewed
  translations, versioned tool/runtime screenshots, independent model-family
  evaluation, and public-example publication gates remain open.
- Existing safety branches and stashes remain retained until review and
  landing.

## 2026-07-31 - source-owned responsive documentation diagrams

Scope and source reconciliation:

- Fetched Last Frontier `origin/main` and Engine `origin/master` before this
  slice. The project remains at
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f`, seven commits ahead and zero
  behind `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`. Engine remains at
  `fac978a67d1e601eb77389e8dc562d7e511705a0`, fourteen commits ahead and zero
  behind `fee50fb636b5bd1e30509aded929df1fc0e95db5`. Both incoming ranges are
  empty.
- Audited the production plan, public Markdown, site layout/assets, generated
  delivery models, browser harness, and Engine/game ownership boundary. The
  corpus had no owned teaching image beyond the site mark and no visual model
  for architecture, generation order, or human/AI delivery.

Implemented and corrected:

- Added `BuildTools/DocumentationDiagrams.json` as the reviewed semantic
  source and `BuildTools/docs_diagrams.py` as a standard-library-only,
  deterministic generator. It validates stable IDs, owning documents,
  descriptive alt text, explanatory captions, source provenance, roles,
  canvas bounds, non-overlapping nodes, local-only SVG safety, and exact
  checked output.
- Generated three teaching diagrams for [Architecture](../Architecture.md),
  [Generated Content Workflow](../en/how-to/build/generated-content.md), and
  [Documentation Site Publication](../SitePublication.md). Each has a horizontal
  desktop variant and an automatically derived vertical mobile variant, so
  mobile text is not a scaled-down desktop canvas.
- Added [generated/diagrams.json](../generated/diagrams.json) with the source
  manifest hash, owning document, dimensions, alt/caption text, complete
  source paths, variant paths, and exact SVG hashes. The six SVG variants live
  under `Docs/assets/diagrams/` and contain native `title`/`desc` metadata,
  no script, no `foreignObject`, and no external resource.
- Added focused schema/freshness/XML/safety/layout/hash/palette-contrast tests,
  aggregate stale-output coverage, manifest ownership, CI checks, maintenance
  triggers, generation order, site artifact endpoints, and AI machine-model
  discovery. Generated SVG and catalog files are never hand-edited.
- Extended the rendered browser gate to decode every content image before
  checking it, retain desktop/mobile architecture-diagram screenshots, and
  keep the existing navigation screenshots. Visual review corrected crossing
  desktop arrows, unreadably reduced mobile text, fixed-header screenshot
  overlap, lazy-image timing, and an interaction focus timing race.

Mechanical checks:

- `BuildTools/tests/test_docs_diagrams.py` passes all 7 tests. Focused browser
  tests pass all 4 tests, positive and stale-diagram aggregate fixtures pass,
  Python/Node syntax checks pass, and standalone validation covers 183
  maintained Markdown entries.
- Complete `BuildTools/tests/test_docs*.py` discovery passes all 260 tests.
- A clean production-mode build with Ruby `3.3.4` and `github-pages` `232`
  produced `Workspace/site-artifact-20260731-17`. Static validation passes all
  177 routes, 39 static endpoints, and 20,835 local references with zero
  errors.
- The final pinned Chromium matrix passes all 177 desktop and 177 mobile
  routes: 354 page/profile checks, zero axe violations, zero
  runtime/resource/layout errors, all 3 interaction profiles, and 5 retained
  screenshots. All 19 desktop and 6,906 mobile raw `color-contrast`
  incomplete nodes pass the computed fallback; failed/unresolved counts are
  zero and minimum ratios remain 5.009:1 and 5.779:1.
- The generated catalog contains 3 owned diagrams, 6 responsive variants, 19
  checked source paths, and exact hashes. Search is 988,546 bytes and the AI
  full-context bundle is 1,501,560 bytes, both within their fail-closed
  budgets.

Residual:

- Versioned Mapper/editor/runtime screenshots and tutorial result media still
  need exact build/tag provenance, asset licensing, alt text, capture
  commands, and recapture triggers. The generated architecture diagrams do
  not substitute for those product-facing visuals.
- Manual keyboard-only, 200 percent zoom, and representative screen-reader
  review still belong to the landed release artifact and `fonline.ru`.
- Production Pages source confirmation, physical English/Russian trees and
  175 reviewed translations, independent model-family evaluation, and
  owner-gated public-example releases remain open.
- Existing safety branches and stashes remain retained until review and
  landing.

## 2026-07-31 - reusable Tracy profiling guide

Scope and source reconciliation:

- Fetched Last Frontier `origin/main` and Engine `origin/master`. The project
  remains at `d54645eda827c4d4ade75e4f542cf6b7c8f9682f`, seven commits ahead and
  zero behind `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`. Engine remains at
  `fac978a67d1e601eb77389e8dc562d7e511705a0`, fourteen commits ahead and zero
  behind `fee50fb636b5bd1e30509aded929df1fc0e95db5`. Both incoming ranges are
  empty.
- Audited all four Engine profiling configurations, Tracy compile wiring and
  vendored version, native and AngelScript zones, frame marks, plots, logs,
  thread naming, allocation events, the engine-owned multiplayer sample, and
  the project-owned Last Frontier profiling workflow.

Implemented and corrected:

- Added [Profiling](../Profiling.md) as the Engine-owned guide for selecting one
  measured process, matching the exact Tracy `0.13.1` tools, building
  on-demand or total profiles, running isolated client/server captures,
  designing reproducible workloads, interpreting timelines/CSV exports, and
  adding focused instrumentation.
- Recorded the current limitations explicitly: Engine captures do not provide
  first-party GPU zones or Tracy lock wrappers, and allocator events do not
  prove complete accounting for plain third-party C allocations.
- Routed the guide through the manifest, site navigation/search, AI delivery,
  maintainer entry points, testing/debugging cross-links, maintenance
  triggers, CI, backlog, and production plan. Added a profiling question to
  the deterministic AI set and seven focused source/routing tests.
- Corrected the engine-owned MinimalMultiplayer launch recipe after live
  validation proved that its runtime working directory must be
  `Build/windows`; the config path is now relative to that directory. Last
  Frontier documentation now links to the Engine contract while retaining
  ownership of scenes, runners, thresholds, and reports.

Runtime and mechanical evidence:

- Built `BakeResources` plus regular and `Profiling_OnDemand` client/server
  target pairs for MinimalMultiplayer on Windows. Built `tracy-capture` and
  `tracy-csvexport` from the exact upstream `v0.13.1` tag.
- Rejected an intentionally retained near-empty client attempt with only
  three frames and zero zones. A profiled-server workload capture completed
  10.11 seconds with 42,418 zones and a 470,781-byte trace; CSV export exposed
  native source rows and the AngelScript `EnterWorld(Player@)` zone from
  `Scripts/Tutorial.fos`. Runtime stdout/stderr contained no error, fatal,
  assertion, or exception lines.
- The focused profiling suite passes 7 tests. Complete `test_docs*.py`
  discovery passes all 267 tests; standalone validation passes 184 maintained
  Markdown entries. Snippet validation passes 240/240 normative blocks plus
  141 evidence and 134 external-parser checks.
- Localization reports 0/176 current translations. Site data contains 97
  navigation items, 176 searchable documents, 178 routes, 171 planned
  redirects, and a 993,786-byte search index. AI evaluation passes 13 tasks
  and 26 retrieval checks with 80.8 percent top-1, 100 percent top-3, and
  0.897 MRR; AI delivery contains 178 documents in 1,521,319 bytes.
- A fresh production-mode artifact built with Ruby `3.3.4` and
  `github-pages` `232` passes static validation for all 178 routes, 39 static
  endpoints, and 21,135 local references. Chromium passes 178 desktop and 178
  mobile routes, all 3 interaction profiles, and 5 screenshots with zero
  violations or runtime/resource/layout errors. All 19 desktop and 6,911
  mobile raw contrast-incomplete nodes pass the computed fallback; failed and
  unresolved counts are zero.

Residual:

- The landed CI artifacts, configured production Pages source, and the live
  `fonline.ru` deployment still require observation outside this worktree.
- Remaining production work is unchanged in kind: focused tool manuals,
  versioned tool/runtime screenshots, 176 reviewed Russian counterparts,
  independent model-family evaluation, and the owner-gated public example
  release gates.
- Existing safety branches and stashes remain retained until review and
  landing.

## 2026-07-31 - focused animation and particle viewer guide

Scope and source reconciliation:

- Fetched Last Frontier `origin/main` and Engine `origin/master`. The project
  remains at `d54645eda827c4d4ade75e4f542cf6b7c8f9682f`, seven commits ahead and
  zero behind `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`. Engine remains at
  `fac978a67d1e601eb77389e8dc562d7e511705a0`, fourteen commits ahead and zero
  behind `fee50fb636b5bd1e30509aded929df1fc0e95db5`. Both incoming ranges are
  empty.
- Audited the focused viewer implementations and application hosts, Mapper
  embedding, CMake source/library/application/output wiring, package
  capabilities, resource mounting, particle backends, model discovery, and
  per-user settings. The current Engine has no generic Editor or AssetExplorer
  application.

Implemented and corrected:

- Added [Viewer Tools](../ViewerTools.md) as the Engine-owned guide for choosing,
  building, launching, and reviewing assets in AnimationViewer and
  ParticleViewer. It documents exact controls, persisted state, package/data
  boundaries, failure diagnosis, review provenance, and the required later
  Mapper/client checks.
- Corrected stale public tool inventories and the Mapper menu route, added the
  guide to navigation/search/AI delivery, and added a representative AI task
  for animation and particle inspection. Last Frontier now routes reusable
  behavior to the Engine guide while retaining project assets, commands, and
  acceptance policy.
- Added an Engine and Last Frontier maintenance trigger so viewer source,
  application, target, packaging, resource, control, or settings changes
  require same-change documentation and focused validation.

Runtime and mechanical evidence:

- Configured MinimalMultiplayer with `FO_BUILD_MAPPER=ON` and built
  `BakeResources`, `FOMM_AnimationViewer`, and `FOMM_ParticleViewer` in Windows
  `RelWithDebInfo`. Both applications remained alive through an eight-second
  DirectX startup smoke using the sample configuration, with no error, fatal,
  assertion, or exception lines.
- The focused viewer suite passes all 8 tests. Complete `test_docs*.py`
  discovery passes all 275 tests; standalone validation passes 185 maintained
  Markdown entries. Snippet validation passes 244/244 normative blocks plus
  141 evidence and 138 external-parser checks.
- Localization reports 0/177 current translations. Site data contains 98
  navigation items, 177 searchable documents, 179 routes, 172 planned
  redirects, and a 1,000,949-byte search index. AI evaluation passes 14 tasks
  and 28 retrieval checks with 82.1 percent top-1, 100 percent top-3, and
  0.905 MRR; AI delivery contains 179 documents in 1,540,417 bytes.
- A fresh production-mode artifact built with Ruby `3.3.4` and
  `github-pages` `232` passes static validation for all 179 routes, 39 static
  endpoints, and 21,443 local references. Chromium passes 179 desktop and 179
  mobile routes, all 3 interaction profiles, and 5 screenshots with zero
  violations or runtime/resource/layout errors. All 19 desktop and 6,913
  mobile raw contrast-incomplete nodes pass the computed fallback; failed and
  unresolved counts are zero and minimum ratios remain 5.009:1 and 5.779:1.

Residual:

- This viewer-only checkpoint originally had no representative particle
  asset. The later interactive Mapper/particle-authoring checkpoint added a
  provenance-pinned SPARK fixture, full-window capture, and visible acceptance
  without changing the focused viewer contract recorded here.
- Broader Mapper and SPARK/Effekseer manuals plus two versioned captures are
  now complete. The 179 reviewed Russian counterparts, independent
  model-family evaluation, and owner-gated public-example releases remain
  open.
- Existing safety branches and stashes remain retained until review and
  landing.

## 2026-07-31 - pinned external-project evidence and owner review policy

Scope and source reconciliation:

- Fetched Last Frontier `origin/main` and Engine `origin/master`. The project
  remains at `d54645eda827c4d4ade75e4f542cf6b7c8f9682f`, seven commits ahead and
  zero behind `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`. Engine remains at
  `fac978a67d1e601eb77389e8dc562d7e511705a0`, fourteen commits ahead and zero
  behind `fee50fb636b5bd1e30509aded929df1fc0e95db5`. Both incoming ranges are
  empty.
- Re-cloned public TLA at exact `b603d8fdbc2b2f89f233b2a1938686ead9d8d480`
  and re-audited its README, maintainer guide, nullability notes, five focused
  documents, build root, native extensions, scripts, content, resources, and
  workflows against the current Last Frontier and Engine documentation map.

Implemented and corrected:

- Added `BuildTools/ExternalProjectEvidence.json` with 29 classified concerns
  and 108 exact source paths. The result records 19 promoted, 2 boundary-owned,
  5 promotion-candidate, and 3 project-owned decisions; every record has a
  priority, current or planned target, primary owner, required reviews,
  promotion gate, and explicit decision.
- Added `BuildTools/docs_external_evidence.py`, its generated internal
  JSON/Markdown audit, seven focused tests, manifest artifact declaration,
  aggregate freshness validation, and required CI checks. External source
  verification resolves every path through `git cat-file` at the recorded
  SHA, so a dirty Last Frontier worktree cannot silently become snapshot
  evidence.
- Added `localization` as a separate documentation owner and made the manifest
  define exact scope, required evidence, and co-review triggers for all eleven
  owners. The translation workflow now belongs to that owner while text-format
  mechanics remain under `content-data`.
- Closed the Phase 0 evidence/ownership items and the Phase 5 tool-classification
  item. The project maintenance playbook now requires updating and verifying
  this Engine registry when Last Frontier introduces a new reusable concern or
  changes a promotion decision.
- Reviewed the bounded AI context rather than raising its cap. After growth
  left only 692 bytes, the policy excluded redundant `ScriptMethodsMap.md`
  from the full bundle while retaining it in `llms.txt`, search, and the public
  manifest; `PUBLIC_API.md`, `GeneratedApiAndMetadata.md`, and the generated
  API index remain the maintained contract routes.

Mechanical evidence:

- The focused external-evidence suite passes all 7 tests. Complete
  `test_docs*.py` discovery passes all 301 tests; standalone validation passes
  188 maintained Markdown entries.
- Exact external source verification passes all 108 paths in Last Frontier
  `d54645e` and TLA `b603d8f`. Snippet validation passes 256/256 normative
  blocks, 141 evidence blocks, and 147 real Bash/PowerShell parser checks.
- Localization remains 0/180 current translations. Site data remains 101
  navigation items, 180 searchable documents, 181 routes, and 174 planned
  redirects in a 1,021,557-byte search index.
- AI evaluation remains green for 15 tasks and 30 retrieval checks with 83.3
  percent top-1, 100 percent top-3, and 0.911 MRR. AI delivery contains 181
  public documents and a 1,559,072-byte full-context bundle, leaving 13,792
  bytes under the fixed 1,572,864-byte cap.

Residual:

- External source verification is intentionally local/manual because the
  standalone Engine CI checkout cannot access the private Last Frontier
  repository. Normal CI still validates the pinned revisions, schema, owner
  policy, Engine targets, deterministic output, and workflow wiring.
- The five recorded promotion candidates remain real work: a reusable gameplay
  harness, project-neutral AI-control protocol/sample, broader packaging and
  release procedure, security/secrets/recovery guides, and enforceable common
  AngelScript style guidance.
- Public-example publication, platform/release validation, broad native API
  classification, 180 reviewed Russian counterparts, production Pages-source
  confirmation, manual accessibility review, and independent model-family
  evaluation remain open.
- This Windows environment still lacks Ruby/Bundler and Docker, so the next
  landed GitHub Pages job remains the owner of fresh rendered-site evidence.

## 2026-07-31 - packaging and release procedure

Scope and source reconciliation:

- Refetched Last Frontier `origin/main` and Engine `origin/master`; both
  incoming ranges remain empty. The project checkout is still
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` over upstream
  `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`, and Engine is still
  `fac978a67d1e601eb77389e8dc562d7e511705a0` over upstream
  `fee50fb636b5bd1e30509aded929df1fc0e95db5`.
- Audited `PackageInterface.json`, the executable `package.py` parser and
  payload paths, `DefinePackage`/Packages CMake stages, package determinism
  tests, support registry and required workflow, updater packaging, Web and
  Android guides, and both Engine-owned starter/tutorial projects.
- Verified all 108 pinned Last Frontier/TLA evidence paths again after moving
  the packaging procedure from planned to current Engine ownership.

Implemented and corrected:

- Added [PackagingAndRelease.md](../PackagingAndRelease.md) as the reusable human
  procedure for package declarations, build/bake/package ordering,
  Windows/Linux/Web/Android payloads, server/service/daemon variants,
  signing, secret handling, provenance, acceptance, publication, rollback,
  and failure routing.
- Separated build capability, packager capability, project qualification, and
  release publication. The guide records the current packager's explicit
  macOS/iOS rejection even though narrower Apple client inputs are build-gated;
  it does not invent an application bundle, signing, device, or store route.
- Added a project-owned package matrix schema, artifact manifest requirements,
  a twelve-layer acceptance matrix, and a twelve-step release checklist.
  Full-package byte identity remains a tested project/toolchain claim rather
  than being inferred from deterministic resource ZIP entries.
- Registered the guide under the `build-release` owner, site navigation,
  `llms.txt` start routes, AI evaluation, maintenance rules, roadmap, and the
  Last Frontier routing pages. Last Frontier keeps its concrete package rows,
  CI hosts, credentials, infrastructure, database, and release acceptance.
- Kept the 1.5 MiB AI cap. The reviewed full-context policy now omits the
  redundant `BuildTools/README.md` while its exact generated CLI/helper/package
  references and task guides remain in `llms.txt`, search, and the public
  manifest.

Mechanical evidence:

- The package documentation suite passes 6 tests, the resource/archive
  determinism suite passes 3 tests, and
  `cmake -P BuildTools/tests/validate_package_interface.cmake` passes.
- Complete `test_docs*.py` discovery passes all 302 tests. Standalone
  validation passes 189 maintained Markdown entries, and all affected
  generated package, support, external-evidence, localization, snippet, site,
  AI-evaluation, and AI-delivery checks are current.
- Snippet validation covers 400 fences: 258/258 normative blocks pass, 142
  evidence blocks remain structural, and 148 Bash/PowerShell blocks require
  real no-execution parsers. Localization remains 0/181 current.
- Site delivery contains 102 navigation items, 181 searchable documents, 182
  public routes, and 175 planned redirects in a 1,030,315-byte search index.
- AI evaluation passes 15 tasks and 31 retrieval checks with 83.9 percent
  top-1, 100 percent top-3, and 0.909 MRR. AI delivery contains 182 public
  documents and a 1,539,988-byte full-context bundle, leaving 32,876 bytes
  under the fixed cap.

Residual:

- `Examples/PackagingMatrix` plus immutable public minimal-project package
  artifacts remain the promotion gate. Therefore the platform-procedure and
  every-support-claim Phase 6 items remain open.
- Reusable service operations, security, database recovery, and incident
  guides remain separate Phase 6 work. Product providers, credentials,
  environments, data, and incident policy stay project-owned.
- Engine still has no macOS/iOS packager. Apple release support must not be
  claimed until a project-owned or promoted route assembles, signs, installs,
  and repeatedly qualifies the real artifact.
- Fresh rendered GitHub Pages evidence still belongs to the required Linux CI
  job because this Windows host has neither Ruby/Bundler nor Docker.

## 2026-07-31 - executable native packaging matrix

Scope and ownership:

- Added `Examples/PackagingMatrix` as an Engine-owned validation fixture rather
  than expanding the readable starter/tutorial configs with every runtime
  setting required by `ConfigBaker`.
- `generate_config.py` derives the checked-in full config from
  `Source/Common/Settings.inc`, excludes only runtime-managed settings named by
  `Settings.cpp`, preserves quoted comma-containing defaults, and fails its
  freshness check on settings drift.
- `PackagingAndRelease.md`, `SupportMatrix.md`, `BuildWorkflow.md`, public-example
  policy, maintenance routing, Phase 6 status, the backlog, and external-project
  evidence now describe the executable fixture without promoting it to a game
  release, installer, signing, deployment, or rollback claim.

Executable contract:

- `win64-package-smoke` and `linux-package-smoke` are required BuildTools
  validation profiles over the standalone fixture. They build ordinary and
  headless client/server binaries, Windows service or Linux daemon, and baker;
  force-bake server/client `PackageSmoke` configs; then create raw payloads and
  ZIP or TAR.GZ archives.
- `verify_package.py` checks required roles and Linux executable modes,
  normalizes only ZIP/TAR root representation, compares every archive member
  with the staged payload, hashes both artifacts and every payload file, and
  records the exact Engine revision.
- The packaged headless server is started with its embedded config, exposes the
  updater, and reaches `packaging_matrix_server_ready`. The packaged headless
  client loads its packaged runtime library, completes the real updater
  handshake, observes `Common.Packaged` plus the fixture setting, emits
  `packaging_matrix_client_passed`, and exits zero. The server then emits
  `packaging_matrix_server_passed` and shuts down cleanly with exit zero.
- The required workflow uploads archives plus `packaging-manifest.json` as
  `packaging-evidence-<validation-target>-<commit-sha>` and fails when evidence
  is missing. This makes artifact identity explicit without claiming that
  unsigned fixture binaries are production releases.

Mechanical evidence:

- `python BuildTools/buildtools.py validate win64-package-smoke` passed locally
  against Engine `fac978a67d1e601eb77389e8dc562d7e511705a0`. Config baking
  emitted all four root/subconfig server/client files; both Windows archives,
  archive/payload parity, service-role presence, updater handshake, runtime
  markers, and the generated SHA-256 manifest passed.
- Focused packaging-matrix/support/package/external-evidence/snippet/site/AI
  tests pass. Complete `test_docs*.py` discovery passes all 302 tests, and
  standalone validation passes 189 maintained Markdown entries.
- Snippet validation covers 401 fences: 259/259 normative blocks pass, 142
  evidence blocks remain structural, and 149 Bash/PowerShell blocks require
  real no-execution parsers. Localization remains 0/181 current translations.
- Before this entry, site delivery contained 181 searchable documents in
  1,031,657 bytes, and AI delivery used 1,543,551 full-context bytes. Both were
  below their fixed limits; final regenerated figures follow from the checked
  artifacts rather than these pre-entry measurements.

Residual:

- The Linux profile and immutable upload are required and structurally tested,
  but their first landed Ubuntu run has not yet been observed. Windows local
  evidence must not be generalized to Linux before that job is green.
- `Examples/MinimalMultiplayer` still needs its own publishable package lane,
  exact release pin, immutable artifact, and owner-gated repository release.
  The Engine fixture proves reusable package mechanics only.
- Signing identities, MSI/APK/store routes, visible rendering, production
  services, databases, backup/restore, incident response, and rollout/rollback
  remain project or future reusable-operations work.

## 2026-07-31 - security and secret-flow boundary

Scope:

- traced `GlobalSettings` defaults, authored/internal config application,
  `$ENV`/`$FILE` and target-time substitution, command-line masking, baking
  save/output, and settings UI exposure;
- traced Windows hook order and Android root/sub-config, Gradle environment,
  generated-project, and fallback-signing paths;
- separated reusable Engine mechanics from project providers, credentials,
  environments, account policy, incident severity, and operational retention;
- added package behavior and documentation tests plus maintenance, upgrade,
  release, AI-evaluation, roadmap, and embedding-project routes.

Findings and changes:

- `Common.SecretSettingTokens` masks only the value printed by
  `ApplyCommandLine()`; it is not a credential type, storage mechanism, or
  general redactor. Raw arguments, settings UI, baked values, project logs,
  crash output, and process memory remain separate exposure paths.
- ordinary `$ENV`/`$FILE` directives resolve during baking and can materialize
  into internal config. The new guide therefore requires `$TARGET_ENV` or
  `$TARGET_FILE` for sensitive runtime/package inputs.
- `package.py` previously read Android package metadata from the baked client
  config even though Android signing settings are server-scoped, and Windows
  hook expressions remained literal. It now derives the authored root plus
  selected sub-config and resolves ordinary/target directives on the package
  host. Passwords still cross to Gradle only through the two dedicated
  environment variables and are absent from the generated Gradle source.
- `test_package_security.py` proves parent/child precedence, fail-closed missing
  inputs, environment/file resolution, unbaked Android signing overlays, and
  Windows hook execution. `test_docs_security_and_secrets.py` pins the guide,
  implementation handoff, and manifest classification.
- All 43 CI-style `test_docs_*.py` modules pass as isolated processes; the
  largest validator module passes all 41 tests. Standalone validation covers
  190 Markdown entries, all 108 pinned external paths verify, and package
  structural/generated checks are current.
- Snippet coverage is 403 fences: 261/261 normative blocks pass, 142 evidence
  blocks remain structural, and 150 external-parser checks pass. Site delivery
  contains 182 searchable documents, 183 public routes, and 1,039,972 search
  bytes under the 1 MiB cap. AI delivery contains 183 public documents and
  1,560,837 bytes under the 1.5 MiB cap; 16 deterministic tasks and 34 retrieval
  checks pass at 100% success with 0.902 MRR. Translation remains 0/182 current.
- A fresh `win64-package-smoke` after the package-host resolution change rebuilt
  client/server/headless/service payloads, passed raw/archive inventory parity,
  ran the packaged server and client through updater generation 2 and host ABI
  3, observed both fixture pass markers, and wrote the SHA-256 evidence manifest.

Residual:

- Engine tests prove substitution and handoff mechanics, not the security of a
  project secret manager, runner, provider account, application logging,
  artifact store, database, or distribution channel.
- Production signing still requires project-qualified synthetic-secret scans,
  signature verification, protected environment approvals, runner isolation,
  and revocation rehearsal before real credentials are introduced.
- At this checkpoint, reusable service release operations and database
  backup/recovery remained separate Phase 6 documentation gaps.

## 2026-07-31 - reusable server release operations

Scope:

- traced windowed, headless, Windows service, and non-Windows daemon entrypoints;
- traced asynchronous startup, `IsStarted()`/`IsStartingError()`, health-file
  transitions, Unix signal handling, shutdown stages, worker drain, and final
  database commit behavior;
- separated reusable lifecycle evidence from project service managers,
  topology, traffic, databases, observability, SLOs, and incident authority;
- integrated the guide with package, updater, security, persistence, upgrade,
  site, localization, AI delivery/evaluation, and embedding-project routes.

Findings and changes:

- [ReleaseOperations.md](../ReleaseOperations.md) now owns process selection,
  immutable release units, readiness/health gates, preflight, staged rollout,
  graceful stop, rollback, failure routing, and release-environment validation.
- Readiness requires process liveness, `Start server complete!`, and a
  project-owned functional probe. `Starting...` in the optional health file is
  not ready, daemon-parent exit is not ready, and `ShutdownGraceMs` bounds only
  the first worker-pool drain.
- Windows service startup failure now requests unsuccessful quit and joins the
  server thread before reporting `STOPPED`. Daemon startup now fails closed
  when `fork()` fails.
- `test_docs_release_operations.py` pins lifecycle markers, source behavior,
  guide structure, manifest classification, and CI wiring. A fresh isolated
  `Build/DocsOps` configured successfully; `LF_UnitTests` built cleanly with
  both application entrypoints and the full native suite exited successfully.
- All 44 CI-style `test_docs_*.py` modules pass as isolated processes; the
  aggregate validator module passes all 41 positive and fail-closed fixtures.
- Generated evidence, localization, snippets, site/search/routes, AI
  evaluation, and AI delivery were refreshed. Site delivery contains 183
  searchable documents, 184 public routes, and 1,045,216 bytes under the 1 MiB
  cap. AI delivery contains 184 public documents and 1,571,968 bytes under the
  1.5 MiB cap. All 17 tasks and 37 retrieval checks pass at 100 percent with
  0.896 MRR; translation remains 0/183 current.

Residual:

- The source and Windows test build compile both testing entrypoints, but a
  real Windows SCM install/control run and a real Linux daemon/signal run still
  belong to their required host lanes before those project routes are called
  production-qualified.
- Engine has no built-in traffic drain or HTTP health endpoint. Projects must
  own and test those integrations without presenting them as Engine behavior.
- `BackupAndRecovery.md` remains planned; provider
  consistency, retention, RPO/RTO, restore authority, and production data stay
  project-owned.
- Search and full-context budgets have only 3,360 and 896 bytes free. The next
  substantial public guide needs an explicit reviewed delivery/content-budget
  decision rather than truncation or a silent limit increase.

## 2026-07-31 - backup, restore, and disaster-recovery boundary

Scope:

- traced the public database facade, commit queue, failed-write spill,
  reconnect/startup replay, panic, startup, and graceful shutdown paths;
- audited JSON, SQLite, Mongo, and Memory backend storage/consistency behavior
  plus the database recovery tests;
- reconciled the reusable Engine contract with Last Frontier's existing live
  Mongo dump, timer/off-host copy, and scratch restore runbooks;
- reviewed site-search and full-context capacity before adding another public
  owner, retaining whole-document fail-closed delivery.

Findings and changes:

- [BackupAndRecovery.md](../BackupAndRecovery.md) now owns backend durable sets,
  pending/committed oplog limits, backup manifests, quiesced and proven-online
  consistency, incident capture, isolated restore, semantic read/write/restart
  checks, migration compatibility, RPO/RTO evidence, and DR drills. Provider,
  schedule, retention, credentials, concrete migration, production data, and
  restore authority remain project-owned.
- The recovery oplog is now documented as failed-write replay only, never a
  backup, history, replication stream, or point-in-time source. The guide pins
  prefix matching, idempotent replay limits, append/fsync, conflict handling,
  panic thresholds, and the inability to reconstruct successful writes after
  an older snapshot.
- Source audit found that `DataBase.OpLogPath` without `.oplog` made pending
  and committed handles resolve to the same path and fail through an opaque
  file lock. Startup now rejects that configuration explicitly; a native test
  and `test_docs_backup_recovery.py` pin the behavior and generated setting
  reference.
- Last Frontier's `prod-mongodump` runbook now labels its live standalone
  Mongo archive as a logical storage copy without a transaction-wide snapshot.
  The scratch restore runbook labels its result storage-only. Both route to
  the complete Engine procedure and forbid calling dump/timer/off-host/storage
  success alone migration-safe or DR-qualified.
- External evidence now records 20 promoted, 2 boundary-owned, 4 promotion
  candidates, and 3 project-owned concerns across the same 108 pinned paths.
  The operations/recovery concern has no remaining planned Engine target.
- ADR 0003 raises the reviewed full-context cap from 1.5 MiB to 2 MiB; ADR 0004
  raises search from 1 MiB to 1.25 MiB. Both generators still fail on overflow,
  include complete documents, and prohibit silent removal/truncation.

Validation:

- `test_docs_backup_recovery.py`: 3 tests passed. The new native oplog-path
  case passed independently with one assertion.
- A fresh `Build/DocsOps` rebuild of `LF_UnitTests` completed without warnings;
  the complete native suite exited successfully after the source/settings
  change.
- All 45 `test_docs_*.py` modules pass as isolated CI-style processes. All 31
  generated/freshness/standalone checks pass; `docs_validate.py` validates 192
  Markdown entries. The aggregate contract comparator reports zero current
  changes across its 17 available baseline domains.
- Generated delivery now contains 105 navigation items, 184 searchable
  documents, 185 public routes, 178 planned redirects, and a 1,053,818-byte
  search index. AI delivery contains 185 public documents in 1,589,906 bytes.
  All 18 tasks and 40 retrieval checks pass at 100 percent with 0.904 MRR;
  localization remains 0/184 current.

Residual:

- No production database, provider account, timer, service, backup, or restore
  was touched in this documentation pass. Last Frontier still needs an
  owner-approved quiesced or otherwise provider-proven consistent capture,
  immutable recovery-unit sidecar, isolated game startup plus semantic
  read/write/restart automation, and measured RPO/RTO drill before claiming
  production DR qualification.
- SQLite uses WAL with `synchronous=NORMAL`, Mongo inherits URI/provider
  guarantees, and Engine exposes no snapshot, online-backup, checkpoint,
  traffic-drain, restore-orchestration, or point-in-time controller. Projects
  must qualify those boundaries on their real topology.
- The expanded budgets have 256,902 search bytes and 507,246 full-context
  bytes free. Further increases still require explicit ADR review.
## 2026-07-31 - project-local dependency ownership and role linking

Scope:

- `BuildTools/cmake/ProjectInterface.json`, `BuildTools/Init.cmake`,
  `BuildTools/cmake/helpers/Build.cmake`, `State.cmake`, `ThirdParty.cmake`,
  `CoreLibs.cmake`, and `Packages.cmake`.
- `Examples/MinimalProject/CMakeLists.txt`,
  `StarterServerExtension.cpp`, its README, and the executable Windows starter
  validation path.
- Last Frontier `CMakeLists.txt`, project native-extension/dependency docs, and
  the pinned external-evidence record. The project remains integration
  evidence; it is not normative Engine source.

Results:

- The audit found no selected project-facing command for attaching an existing
  project CMake target to an Engine role. Embedding projects had to mutate
  internal `FO_*_LIBS` state, and `MapperLib` had no mapper-only dependency
  list. `AddProjectLibraries(ROLES ... LIBRARIES ...)` is now a public
  experimental, revision-pinned helper for all five native roles. It rejects
  missing, unknown, unexpected, and post-CoreLibs registration and deduplicates
  each role entry; `FO_MAPPER_LIBS` is consumed only by `MapperLib`.
- [ProjectDependencies.md](../ProjectDependencies.md) now owns dependency
  classification, delivery models, dependency records, stage order, selected
  helper usage, controlled `find_package()`, role selection, headers/warnings,
  platform states, package payloads, ABI/allocation/lifetime, licensing and
  supply-chain review, updates, rollback, validation, and failure routing.
  Engine-vendored maintenance and native bridge behavior remain separate
  owners.
- The minimal project links an `INTERFACE` usage requirement only to `SERVER`;
  `StarterServerExtension.cpp` fails compilation if propagation is absent.
  Last Frontier routes curl, Spine, SHA, Steamworks, Sentry, and platform
  libraries through the selected helper instead of appending to Engine state.
- The external-evidence record remains `promoted` and now names the complete
  Engine guide. Site navigation, planned locale routes, AI delivery, and a new
  three-query evaluation task all use the same stable document ID.

Validation:

- `cmake -P BuildTools/tests/validate_project_interface.cmake` passed its
  then-current positive routing checks and child-process unknown-role rejection;
  this rejection behavior was superseded by the 2026-08-24 docs-only reconciliation.
- `test_docs_project_dependencies.py` passed 3/3; `test_docs_cmake.py` passed
  5/5; the complete isolated `test_docs_*.py` set passed 46/46 after updating
  the pinned corpus total for the new normative CMake fence.
- Last Frontier `Build/DocsOps` reconfigured successfully and
  `LF_UnitTests` rebuilt warning-clean with project targets routed through the
  helper.
- `python BuildTools/buildtools.py validate win64-starter-smoke` completed a
  clean configure/build/codegen/bake/runtime route. The log reached
  `starter_native_extension_value=42`, `starter_smoke_passed`, and clean server
  shutdown.
- The aggregate contract command completed in bootstrap mode across all 17
  domains with no missing dispositions. AI evaluation passed 19 tasks and 43
  retrieval checks at 100 percent success and 0.899 MRR.

Residual boundary:

- The local executable proof is Windows x64 with the fixture's server and Baker
  roles plus the current Last Frontier Windows build graph. It does not promote
  Linux/macOS, proprietary-SDK-enabled, shared-runtime packaging, or service
  initialization lanes without their own CI/host/package evidence.
- Project inventories, SDK licenses, credentials, providers, platform support,
  runtime payload hashes, security response, and release approval remain owned
  by each embedding project.

## 2026-07-31 - AngelScript style and refactoring promotion

### Scope and evidence

- Compared Last Frontier `AGENTS.md` / `Docs/Scripts.md` and TLA
  `Docs/ScriptStyle.md` / `Docs/Refactoring.md` against current Engine
  CoreScripts, `.clang-format`, `BuildTools/buildtools.py`, AngelScript
  attributes/compiler behavior, and focused native tests.
- Promoted only practices with current Engine ownership: namespace-to-file
  routing, side visibility, the clang-format 20 wrapper and
  nullable/template/array/named-argument repairs, generated-source ownership,
  nullability/invariant routing, change classification, narrow batches, and
  compile/runtime proof.
- Kept comment language, file-header mandates, game vocabulary, module
  ordering, project generators, migration backlog, quality-ratchet thresholds,
  fixtures, and gameplay acceptance outside Engine.

### Changes

- Added [AngelScriptStyle.md](../AngelScriptStyle.md), routed it from the scripting
  index, build/generated-content/maintenance guides, `AGENTS.md`, the human
  site, AI start set, and Last Frontier project documentation.
- Added `BuildTools/tests/test_docs_angelscript_style.py` to validate the prose,
  formatter repair behavior, every current CoreScripts
  namespace/preprocessor/encoding/EOF contract, manifest/site/AI/evidence
  registration, and workflow gate.
- Corrected the stale `MapperCore.fos` entry in [Scripting.md](../Scripting.md) and
  removed the project-name-specific formatter example from
  `BuildTools/README.md`.
- Promoted `angelscript-style-and-refactoring` in the external evidence ledger.
  The distribution is now 21 promoted, 2 boundary-owned, 3 promotion
  candidates, and 3 project-owned records across the unchanged 29 concerns and
  108 exact external paths.

### Validation

- `test_docs_angelscript_style.py`: 4/4 passed.
- `test_docs_external_evidence.py`: 7/7 passed; generated evidence is current.
- `test_docs_snippets.py`: 5/5 passed; external-parser validation passed
  265/265 normative snippets, with 142 evidence snippets and 151 external
  parser checks.
- `test_docs_site.py`: 10/10 passed; the current site has 107 navigation items,
  186 searchable documents, 187 public routes, 180 planned redirects, and
  1,069,216 search bytes.
- `test_docs_ai_eval.py`: 4/4 passed; the current 20-task/45-query set passes at
  100 percent and 0.904 MRR.
- `test_docs_ai_delivery.py`: 10/10 passed; 187 public documents occupy
  1,626,402 bytes of the 2 MiB context budget.
- `test_docs_localization.py`: 6/6 passed; 0/186 required Russian counterparts
  are current.
- `test_docs_inventory.py`: 2/2 passed; `docs_validate.py` passed 194 maintained
  Markdown entries.

## 2026-07-31 - Gameplay and integration test harness promotion

### Scope and evidence

- Compared Last Frontier's project test layers and pipeline runners plus TLA's
  historical test planning with the current Engine native test targets,
  Minimal Multiplayer, and CI boundaries.
- Promoted only reusable contracts: select the narrowest observable boundary,
  build deterministic fixtures, launch dependent processes after semantic
  readiness, assert required/forbidden markers and exit codes under one
  deadline, clean up every child, and retain structured plus log evidence.
- Kept script test registration, gameplay fixtures/assertions, project filters,
  authored ids, accounts, databases, ports, package/device lanes, and
  acceptance thresholds outside Engine.

### Changes

- Added [GameplayTesting.md](../GameplayTesting.md) and routed it independently
  from the native Catch2/coverage inventory in [Testing.md](../Testing.md).
- Added `BuildTools/gameplay_test_runner.py`, registered its executable parser
  in `HelperCliInterface.json`, regenerated the helper CLI model/reference, and
  added `Examples/GameplayTestHarness` as a deterministic synthetic fixture.
- Replaced Minimal Multiplayer's private process-orchestration implementation
  with `tutorial-smoke.json` plus the shared runner while retaining its strict
  baked metadata decoder. The public-example registry now requires the
  manifest and declares the capability.
- Promoted `gameplay-test-harness` in the external evidence ledger. The 29
  records now comprise 22 promoted, 2 boundary-owned, 2 promotion candidates,
  and 3 project-owned decisions across the unchanged 108 source paths.

### Validation

- `test_gameplay_test_runner.py`: 4/4 passed, covering positive ordered launch,
  JSON report shape, missing/forbidden markers, common-deadline timeout and
  cleanup, malformed manifests, and unresolved placeholders.
- The current Windows Minimal Multiplayer binaries passed both manifest
  scenarios: metadata agreement and content assertions, followed by real
  headless server/client networking, map load, remote calls, replicated state,
  item interaction, and clean shutdown.
- `test_docs_gameplay_testing.py`: 4/4 passed; helper CLI generation and its
  focused 4-test suite pass with 8 helpers and 21 global arguments.
- Snippet generation passes 267/267 normative blocks, 143 evidence blocks, and
  152 external-parser checks. Site generation reports 108 navigation items,
  187 searchable documents, 188 public routes, 181 planned redirects, and a
  1,075,935-byte search artifact.
- AI evaluation passes 21 tasks and 47 retrieval checks at 100 percent success
  and 0.894 MRR. AI delivery contains 188 public documents in 1,640,891 bytes.
  Localization correctly reports 0/187 current Russian counterparts. The
  standalone validator passes all 195 maintained Markdown entries.

## 2026-07-31 - Minimal Multiplayer native package acceptance

### Scope and boundary

- Closed the remaining source-side packaging gap between the reusable
  `Examples/PackagingMatrix` fixture and the publishable Minimal Multiplayer
  game example.
- Kept `packaging-platform-and-release` as a promotion candidate: local source
  and Windows evidence are complete, but Linux, landed immutable CI evidence,
  an exact remotely reachable example pin, release tag, and public visibility
  have not been observed.
- Retained product-owned signing, installers/stores, visible renderer/audio,
  durable storage, deployment, upgrade, backup, and rollback outside the
  tutorial fixture.

### Changes

- Added a `Tutorial` package for Windows x64 ZIP/raw and Linux x64 tar.gz/raw
  client/server payloads, including headless roles and the `TutorialSmoke`
  embedded configuration.
- Added a settings-derived complete `.fomain` generator and a pre-bake
  freshness target. The first package attempt exposed the real Config-baker
  requirement that every saved server/client setting be initialized; the
  generated config now tracks `Settings.inc` and excludes only settings
  explicitly managed by `Settings.cpp`.
- Added `package-smoke.json` and `verify_tutorial_package.py` for archive/raw
  parity, executable-role checks, SHA-256 artifact and payload inventories,
  exact Engine revision, shared-runner gameplay execution, and JSON reports.
- Registered Windows/Linux package presets, BuildTools validation profiles,
  required workflow lanes, commit-addressed artifact uploads, public-example
  capabilities/checks/artifacts, support-matrix evidence, maintenance routes,
  and source ownership.

### Executable evidence

- `cmake --build --preset windows-package` passed against Engine
  `fac978a67d1e601eb77389e8dc562d7e511705a0` after reproducing and correcting
  the incomplete-config and package-directory-name failures.
- The verifier compared both ZIP archives with their raw payloads, hashed 28
  client files and 37 server files, and retained
  `tutorial-packaging-manifest.json` plus
  `tutorial-package-runtime-report.json`.
- The packaged headless client completed the real updater handshake, connected
  to the packaged server, loaded `TutorialMap`, observed localized content,
  invoked the item interaction, received replicated state, emitted both smoke
  markers, and shut down with the server under one bounded scenario deadline.

### Residual

- `linux-tutorial-package` is source-wired and required but has no observed
  landed Ubuntu artifact yet. Windows evidence must not be generalized to it.
- The reserved private `cvet/fonline-minimal-multiplayer` shell cannot be
  staged from the current source until this Engine revision is contained by a
  fetched remote branch. Publication, branch/security policy, immutable tags,
  and public artifacts remain owner-gated.

### Documentation validation

- Complete `test_docs*.py` discovery passes 323/323 tests. Package-focused
  tests pass 4/4, including generated-config freshness and the exact
  `TutorialSmoke` package-name contract.
- Snippet validation covers 412 fences: 269/269 normative blocks pass, 143
  evidence blocks remain structural, and 154 shell-parser checks pass.
- Site delivery remains 108 navigation items, 187 searchable documents, 188
  public routes, and 181 planned redirects in 1,077,613 bytes. AI evaluation
  remains 21 tasks and 47 retrieval checks at 100 percent / 0.894 MRR; AI
  delivery contains 188 documents in 1,645,084 bytes.
- Localization correctly remains 0/187 current Russian counterparts; it is a
  production-launch gap rather than a hidden partial mirror. Standalone
  validation accepts all 195 maintained Markdown entries.

## 2026-07-31 - AiControl protocol promotion

### Scope and evidence

- Audited the Last Frontier and pinned TLA client native bridges, game scripts,
  MCP adapters, and AiControl guides at Last Frontier
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` and TLA
  `b603d8fdbc2b2f89f233b2a1938686ead9d8d480`.
- Promoted only their convergent project-neutral layer: UTF-8 NDJSON over TCP,
  JSON-RPC-style request/response correlation, per-connection authorization,
  six common methods, bounded command/event state, command completion events,
  loopback-first exposure, and native-thread/MCP ownership boundaries.
- Kept observation payloads, action names and semantics, administrator tools,
  readiness policy, launch/process orchestration, server authority, and
  `lf_*`/`tla_*` MCP namespaces project-owned.

### Changes

- Added [AiControlProtocol.md](../AiControlProtocol.md), a 49-entry canonical
  model, six generated reference pages, and the eighteenth aggregate contract
  domain. The generated public index preserves its `experimental` stability.
- Added the standard-library `BuildTools/ai_control_client.py` reference client
  and registered its public parser in the helper-CLI model. Remote endpoints
  require explicit opt-in, credentials come from the environment, responses
  are bounded and correlated, and malformed or ambiguous replies fail closed.
- Added `Examples/AiControlSample`, including a bounded standalone server and a
  12-check end-to-end smoke. It demonstrates protocol behavior without
  claiming native FOnline runtime, gameplay authority, or production network
  security.
- Promoted `ai-control-bridge` in the pinned external-evidence inventory. The
  29 records and 114 exact source references now comprise 23 promoted, 2
  boundary-owned, 1 promotion candidate, and 3 project-owned decisions;
  packaging remains the only promotion candidate.
- Routed the guide, generated reference, sample, maintenance triggers, site,
  localization plan, AI delivery, and Last Frontier integration docs through
  their owning layers. Updates to either side now require reciprocal protocol
  reconciliation and exact external-source verification.

### Validation

- The standalone protocol smoke passes all 12 checks. Eight client/server
  tests cover malformed peers, response correlation, queue/event bounds,
  authorization, remote opt-in, and token CLI policy; five documentation tests
  cover deterministic generation, source anchors, human boundaries, manifest,
  CI, and contract registration.
- Exact external-project verification passes all 114 pinned source references.
  Aggregate contract diff reports 81 changes across 18 domains, with all 15
  required dispositions satisfied and the new domain correctly bootstrapped.
- Complete `test_docs*.py` discovery passes 328/328 tests. The separate
  protocol suite also passes with `ResourceWarning` promoted to an error.
- The regenerated corpus contains 435 fenced snippets: 281/281 normative
  blocks pass, 154 evidence blocks remain structural, and 160 external-parser
  checks pass. AI evaluation covers 22 tasks and 50 retrieval checks at 100
  percent success and 0.900 MRR.
- Site delivery contains 111 navigation items, 195 searchable documents, 196
  public routes, and 188 planned redirects. AI delivery contains 196 public
  documents; localization honestly remains 0/195 current Russian counterparts.

### Residual boundary

- The Python sample is executable transport evidence, not proof of a native
  listener integrated with the FOnline client loop. A separate native public
  example would require exact Engine pinning, server-authority preservation,
  Windows/Linux lanes, shipping exclusion, and owner review.
- The protocol has no TLS and is not an Internet-facing control plane.
  Non-loopback use requires an explicit project threat model, transport
  protection, secret delivery, authorization, rate/size limits, audit policy,
  and a production packaging decision.
- Game schemas, high-volume telemetry, administrator capabilities, credentials,
  process supervision, and acceptance thresholds remain embedding-project
  responsibilities and are intentionally absent from the Engine model.

## 2026-07-31 - First physical English/Russian migration group

### Scope and route ownership

- Migrated the getting-started and first-client tutorials to canonical
  `Docs/en/tutorials` paths and added complete reviewed counterparts at the
  mirrored `Docs/ru/tutorials` paths. Both pairs carry explicit locale,
  stable document ID, permalink, and current canonical-source hash metadata.
- Retained `Docs/GettingStarted.md` and `Docs/FirstClientTutorial.md` as visible
  `> Legacy route.` Markdown pointers. Their former heading anchors remain
  present and forward to the matching canonical English anchors.
- Kept one manifest record and stable ID per human document. The standalone
  inventory now recognizes a hash-current Russian file as the locale variant
  of that canonical record instead of requiring a duplicate manifest ID.
- Localization status reports 2 current of 195 required Russian counterparts,
  193 missing, 1.03 percent coverage, and incomplete production parity. Locale
  status is now `canonical-source-partial-migration` / `partial`; enforcement
  remains `existing-translations-current` until every required page is ready.

### Site and browser delivery

- Site schema 3 emits one localized navigation model, independent bounded
  `assets/docs-search.json` and `assets/docs-search.ru.json` indexes, and the
  shared route catalog. Current output has 111 navigation items, 195 English
  search documents, 2 Russian search documents, 198 route records, 188 legacy
  redirects, 194 translation targets, and 2 complete locale pairs.
- The default layout resolves language pairs by stable ID, renders localized
  controls and group headings, sets `html lang`, and shows EN/RU only for a
  current pair. Untranslated Russian navigation destinations visibly fall back
  to English; Russian search results never cross locales.
- The static artifact gate now requires and validates every locale index and
  every available locale route. A production-mode local Jekyll render passed
  200 rendered routes, 43 static endpoints, and 26,595 local references.
- The full Chromium/axe run passed 400 desktop/mobile page checks, four
  interaction profiles, and six retained screenshots. The added profile proved
  Russian search for `игровой клиент`, the active-locale state, `html lang=ru`,
  and navigation to the exact paired English route. The first run correctly
  found an undefined active-locale contrast color and ASCII-only browser
  tokenization; both root causes were fixed before the green rerun.
- The render used the exact `github-pages` 232 and Bundler 2.5.11 dependency
  pins with locally available Ruby 3.3.12. CI remains authoritative for the
  repository's exact Ruby 3.3.4 pin and the first landed artifact.

### Generated and standalone checks

- Snippet validation remains 281/281 normative blocks, 154 evidence blocks,
  and 160 external-parser checks. AI evaluation remains 22 tasks and 50
  retrieval checks at 100 percent success and 0.900 MRR.
- Localization, site, layout, rendered-artifact, browser, AI-delivery,
  AI-evaluation, and standalone-validator focused suites pass. Standalone
  validation accepts 205 Markdown entries, including the two Russian variants.
  Complete `test_docs*.py` discovery passes 332/332 tests.
- Documentation maintenance now requires update reconciliation to regenerate
  localization status, both search indexes, routes/navigation, evaluation, and
  AI delivery in dependency order, then prove the locale pair in the rendered
  artifact and browser audit.

### Residual production work

- Translate and review the remaining 193 required pages, then add the Russian
  repository/subsystem entry points and symbol-ID API-description catalog.
- Enable `--enforce-complete` only after one-to-one parity, native-language
  review, identifier/code integrity, bilingual accessibility, and search
  evaluation are green.
- Confirm the exact landed GitHub Actions artifact and configured Pages source,
  then perform production-domain keyboard, 200 percent zoom, and representative
  screen-reader review. No local result is a substitute for those owner-gated
  publication checks.

## 2026-07-31 - Second physical English/Russian tutorial group

### Scope and source reconciliation

- Migrated First Content Change and First Automated Test to canonical
  `Docs/en/tutorials/first-content.md` and `first-test.md` routes, added complete
  reviewed Russian mirrors, and updated the four-page tutorial chain to stay in
  the selected locale.
- Retained `Docs/FirstContentTutorial.md` and `Docs/FirstTestTutorial.md` as
  visible `> Legacy route.` pointers with every former heading anchor. The
  canonical stable IDs now belong only to the English pages; separate
  non-human redirect records preserve the old URLs.
- Re-audited the executable Minimal Multiplayer evidence. The content lesson
  now correctly identifies `tutorial-smoke.json` and `package-smoke.json` as
  owners of the visible-name marker, and routes default-language changes
  through `generate_config.py` instead of the generated `.fomain`.
- All seven canonical tutorial fences remain byte-identical in Russian. Their
  normalized source hashes are current. Localization reports 4/195 current,
  191 missing, 2.05 percent coverage, and intentionally incomplete parity.

### Generated, rendered, and browser evidence

- Regenerated snippets, localization status, site/navigation/search/routes,
  AI evaluation, and AI delivery in dependency order. Current site output has
  111 navigation items, 195 English search documents, 4 Russian search
  documents, 200 route records, 188 legacy redirects, 194 translation targets,
  and 4 complete locale pairs. AI delivery contains 200 public documents.
- Snippet validation passes 281/281 normative blocks, 154 evidence blocks, and
  160 external-parser checks. AI evaluation passes 22 tasks and 50 retrieval
  checks at 100 percent success and 0.900 MRR. External project evidence
  remains current.
- Focused validation passes 85 tests; complete `test_docs*.py` discovery passes
  332/332 tests. Standalone validation accepts 207 Markdown entries, including
  all four Russian variants.
- The production-mode local Jekyll artifact passes 204 rendered routes, 43
  static endpoints, and 27,099 local references. The exact `github-pages` 232
  and Bundler 2.5.11 pins run on local Ruby 3.3.12; CI remains authoritative for
  the repository's Ruby 3.3.4 pin.
- The first 408-check Chromium run had one non-reproduced mobile navigation
  timeout on `/`; the other 407 route/profile checks, all interaction profiles,
  and all accessibility checks passed. A complete repeat passed all 408 page
  checks, four interaction profiles, and six screenshots with zero errors.
  All 20 desktop and 7,100 mobile axe incomplete contrast nodes passed the
  computed-color fallback with no failed or unresolved nodes.

### Residual production work

- Translate and review the remaining 191 required pages, add Russian
  repository/subsystem entry points and the symbol-ID API-description catalog,
  then enable complete-parity enforcement only after the full bilingual gate
  passes.
- Confirm the landed GitHub Actions artifact and configured Pages source, then
  complete production-domain keyboard, 200 percent zoom, and representative
  screen-reader review.

## 2026-08-02 - Generated-description localization: foundation and first nineteen domains

### Contract and renderer coverage

- Added `Docs/description-translations.ru.json` as the reviewed Russian overlay
  catalog and `BuildTools/docs_description_translations.py` as its sole
  inventory, validation, application, and status owner. Locators are derived
  from stable keyed-list identity rather than array position. Every registered
  entry pins the normalized English SHA-256; stale, unknown, duplicate, empty,
  type-changing, list-length-changing, or inline-code-changing entries fail.
- The generated status inventories 2,824 reader-facing strings across all
  twenty current machine-model domains. It reports 2,222 current translations,
  602 missing entries, and `complete: false`. Enforcement remains
  `registered-translations-current`: every committed overlay must be current,
  but semantic completion cannot be claimed or enabled until all debt is
  reviewed.
- Nineteen domains are complete: AiControl protocol 134/134, audio 103/103, video 106/106, effect format 157/157, font format 187/187, GUI runtime 199/199, helper CLI 125/125, image format 154/154, model format 135/135, package 62/62, particle format 339/339, public examples 13/13, support
  matrix 76/76, native extensions 43/43, prototype format 75/75, the main CLI
  40/40, map format 99/99, CMake 64/64, and text format 111/111.
- Auditing the CMake and text-format renderers found reader-facing fields that
  the initial generic inventory did not cover. CMake now includes scope and
  precedence lists. Text format includes behavior and missing-behavior fields,
  scope lists, and stable keyed rule names in addition to descriptions,
  requirements, rationales, and notes. Technical language-neutral values use
  explicit `preserve_source` records instead of being silently omitted.
- `docs_cmake.py` and `docs_text_format.py` now generate canonical English and
  Russian references plus all durable legacy outputs through the same
  overlay-aware render path. Russian fixed scaffolding is authored separately;
  fenced examples remain byte-identical. Metadata records exact model and
  catalog source hashes. `docs_validate.py` calls the same text-format render
  function, so standalone validation cannot bypass localization.
- The Audio renderer audit expanded its initial 66-entry inventory to 103 by
  adding stable rule names, format roles, and included/excluded scope lists.
  `docs_audio.py` now generates all six Russian references from the complete
  overlay, preserves the exact validation fence, records English source hashes,
  and owns those paths in the generated-artifact manifest. Standalone
  validation calls the same overlay-aware renderer.
- The Video renderer audit expanded its initial 70-entry inventory to 106 by
  adding all rule names and both scope lists. `docs_video.py` now owns the seven
  Russian references, exact validation fence, translation hashes, and generated
  manifest paths through the same overlay-aware validation route.
- The Model Format renderer audit expanded its initial 76-entry inventory to
  135 by adding scope lists, asset names and requirements, token runtime effects,
  and stable rule names. `docs_model_format.py` now owns seven Russian references,
  preserves all syntax and validation fences exactly, records canonical English
  hashes, and is called through the same overlay-aware standalone validation path.
- The Image Format renderer audit expanded its initial 104-entry inventory to
  154 by adding scope lists, format availability, and rendered field/rule names.
  `docs_image_format.py` now owns seven Russian references, preserves the exact
  validation fence, records canonical English hashes, and shares its
  overlay-aware render path with standalone validation.
- The Helper CLI renderer audit expanded its initial 105-entry inventory to
  125 by adding scope lists, helper names, and invocation ownership. Its two
  Russian references localize generated prose and table scaffolding outside
  fences while preserving every exact 80-column `argparse --help` block; the
  temporary fixture now declares the translation-catalog dependency.
- The AiControl Protocol renderer audit expanded its initial 100-entry
  inventory to 134 by adding scope lists and reader-facing wire, error,
  security, integration, and validation names. Six Russian references preserve
  method names, command fields, wire values, and validation commands while
  translating the project-neutral lifecycle, security, and evidence contract.
- The Effect Format renderer audit expanded its initial 112-entry inventory to
  157 by adding scope lists, rendered section/resource/rule names, and script
  method behavior. `docs_effect_format.py` now owns seven Russian references,
  preserves the syntax and validation command fences, records canonical English
  hashes, and shares its overlay-aware render path with standalone validation.
- The Font Format renderer audit expanded its initial 128-entry inventory to
  187 by adding scope lists, format roles, and rendered FOFNT/rule names. The
  complete overlay also covers source-derived FontFlag descriptions.
  `docs_font_format.py` now owns eight Russian references, preserves validation
  commands, records English hashes, and shares its renderer with standalone
  validation.
- The GUI Runtime renderer audit expanded its initial 166-entry inventory to
  199 by adding scope lists and all rendered lifecycle, layout, input,
  integration, and validation names. Its 199-entry overlay also covers derived
  type, screen API, and annotation rationale templates. `docs_gui_runtime.py`
  now owns seven Russian references and uses the same hash-pinned renderer in
  standalone validation.
- The Particle Format renderer audit expanded its initial 228-entry inventory
  to 339 by adding scope lists, backend/family and rule names, renderer defaults,
  and graph-object family labels. `docs_particle_format.py` now owns eight
  Russian references, preserves validation commands and technical identifiers,
  and removes unsupported inline-code decoration found in the former static RU
  XML table.

### Generated and standalone validation

- Regenerated description status, localization, snippets, site, AI evaluation,
  and AI delivery in dependency order. Physical localization remains 195/195
  and complete. Site generation remains 110 navigation items, 195 English and
  195 Russian search documents, 384 public documents, and 188 redirects.
- Snippet validation passes 293/293 normative blocks, 156 evidence blocks, and
  170 external-parser checks. AI evaluation passes 27 tasks, 63 retrieval
  checks, and 84 answer checks at 100 percent retrieval success and 0.886 MRR.
- Complete `BuildTools/tests` discovery passes 517/517 tests in 197.842
  seconds. Standalone validation accepts 391 Markdown entries. A fixture fix
  now derives all generated Russian reference exclusions from each renderer's
  declared output paths, preventing future generators from creating undeclared
  fixture pages.
- `git diff --check` passes apart from repository line-ending notices. The
  production Jekyll build succeeds, and rendered-artifact validation covers 579
  routes, 44 static endpoints, and 80,239 local references.
- The pinned Chromium/axe audit passes 1,158 desktop/mobile page checks, 11
  interaction profiles, and 13 screenshots with zero route or interaction
  finding groups. The documented command is
  `npm --prefix BuildTools/docs-browser run audit`.

### Revision reconciliation and residual work

- The opening and closing fetches found no incoming range. Last Frontier
  remains `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` and 7/0 ahead/behind
  `origin/main` `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`; Engine remains
  `fac978a67d1e601eb77389e8dc562d7e511705a0` and 14/0 ahead/behind
  `origin/master` `fee50fb636b5bd1e30509aded929df1fc0e95db5`; TLA remains
  `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` and matches
  `origin/master`. Dirty worktrees were preserved.
- Native API renderer audit and semantic translation completed in the
  subsequent 2026-08-02 pass. The current inventory is 2,939/2,939 after the
  API model stopped treating six IDE suppression comments as documentation and
  complete 121-event API gained source-owned descriptions; semantic
  enforcement now uses `complete`.
- Broad public-contract classification, complete platform evidence, landed
  CI/Pages confirmation, manual assistive-technology and zoom review,
  independent model-family evaluation, and owner-authorized example repository
  publication remain separate production gates.

## 2026-08-02 - Forty-eighth physical locale group: configuration, data sources, and tools

### Revision and source audit

- Re-fetched Last Frontier, Engine, and TLA before starting the group. No
  incoming range existed: Last Frontier remained `d54645eda827` and 7/0
  ahead/behind `origin/main` `48cf5f5dd99e`; Engine remained
  `fac978a67d1e` and 14/0 ahead/behind `origin/master` `fee50fb636b5`; TLA
  remained `b603d8fdbc2b` and matched `origin/master`. Dirty worktrees were
  preserved.
- The closing fetch after rendered-browser validation found the same revisions
  and no incoming range in any repository; no late source, config, test, or
  documentation change required another reconciliation pass.
- Re-audited `ConfigFile.*`, `Settings.*`, `DataSource.*`, `FileSystem.*`,
  `CacheStorage.*`, `SettingsStorage.*`, `ApplicationInit.cpp`, the baker and
  application entry points, all current `*Baker` registrations, Mapper capture
  APIs, package roles, and focused native tests.
- Confirmed the effective runtime setting order as Engine defaults, project or
  packaged config, selected sub-configs, writable local-config cache,
  command-line overrides applied exactly once, then derived auto settings.
  Confirmed cached-mount reindexing, installed-client writable overlays, and
  best-effort per-application GUI preference persistence.
- Confirmed thirteen built-in bakers: Metadata, Config, RawCopy, Image, Effect,
  Particle, Proto, Map, Text, ProtoText, ModelMesh, ModelInfo, and AngelScript.
  The Tools owner also now records render-only `SaveMapperScreenshot()` and
  deferred full-window `RequestMapperWindowScreenshot()` capture.

### Canonical routes and complete parity

- Moved Configuration and Data Sources to
  `Docs/en/reference/settings/configuration-and-data-sources.md` and Tools to
  `Docs/en/reference/tools/index.md`. Added complete reviewed Russian mirrors
  at the matching `Docs/ru/` routes.
- Replaced the former flat files with durable Markdown pointers that preserve
  every previous H2/H3 route. Updated README, AGENTS, build, runtime, testing,
  native Essentials, viewer, external-evidence, project Architecture, and
  contract-disposition links to the canonical owners.
- The rendered-artifact gate caught and corrected the Tools index permalink:
  canonical locale URLs use `/Docs/en/reference/tools/` and
  `/Docs/ru/reference/tools/`, not explicit `index.html` URLs. The focused test
  now pins both directory permalinks.
- Localization reports 195/195 current counterparts, zero missing, 100 percent
  physical coverage, and `complete: true`. Manifest enforcement is now
  `complete`; CI invokes `docs_localization.py --check --enforce-complete`, so
  a new required English page cannot land without a current Russian pair.
- Physical route parity does not claim complete semantic translation of
  source-authored dynamic prose in generated API/package/support/example
  tables. Those descriptions still require reviewed stable-ID/model overlays;
  fixed generated scaffolding and all authored human pages are translated.

### Focused and corpus validation

- Added five focused tests in
  `BuildTools/tests/test_docs_configuration_tools.py`. They pin canonical and
  legacy manifest ownership, complete locale enforcement, current config and
  filesystem APIs, default-before-project startup order, the exact thirteen
  baker registrations, both Mapper capture modes, preserved headings,
  directory permalinks, and CI wiring.
- Complete `test_docs*.py` discovery passes 485/485 tests in 394.827 seconds.
  Complete `test_*.py` discovery passes 511/511 tests in 190.004 seconds.
- All 31 generator freshness checks pass. The extended external snippet,
  three CMake interface, and standalone validation set passes 5/5.
- Standalone validation covers 391 Markdown entries. Snippet validation covers
  292/292 normative blocks, 156 evidence blocks, and 169 real external-parser
  checks.
- Source inventories report 953 export methods, 98 native test files, and 271
  settings. AI evaluation remains 27 tasks, 63 retrieval checks, and 84 answer
  checks at 100 percent retrieval success and 0.886 MRR.

### Publication and AI delivery

- Site generation contains 110 navigation items, 195 English and 195 Russian
  search documents, 384 public routes, and 188 planned redirects. Search
  indexes occupy 1,156,685 English bytes and 1,625,414 Russian bytes, both
  within the fail-closed 1.75 MiB per-locale budget.
- AI delivery contains 384 public documents and 1,809,518 full-context bytes,
  within the fail-closed 2 MiB budget.
- A production Jekyll build passes. Rendered-artifact validation covers 579
  routes, 43 static endpoints, and 80,236 local references.
- Pinned Chromium/axe validation passes all 1,158 desktop/mobile rendered-page
  checks, 11 interaction profiles, and 13 screenshots.

### Residual production work

- No physical locale route remains. Keep the 195/195 complete gate green while
  implementing reviewed translation overlays for source-authored generated
  descriptions and the stable-symbol-ID API description catalog.
- Public API source descriptions/examples, broad non-internal stability review,
  complete support-matrix qualification, final starter/platform execution
  evidence, manual assistive-technology and zoom review, independent
  model-family evaluation, landed CI/Pages evidence, and owner-authorized
  example repository staging/publication remain open production gates.

## 2026-08-02 - Forty-seventh physical locale group: package, support, and public examples

### Revision and contract audit

- Re-fetched Last Frontier, Engine, and TLA before closing the group. No incoming
  range exists: Last Frontier remains `d54645eda827` and 7/0 ahead/behind
  `origin/main` `48cf5f5dd99e`; Engine remains `fac978a67d1e` and 14/0
  ahead/behind `origin/master` `fee50fb636b5`; TLA remains
  `b603d8fdbc2b` and exactly matches `origin/master`. Dirty worktrees were
  preserved.
- Re-audited `BuildTools/PackageInterface.json`, `BuildTools/package.py`, the
  package CMake consumer, and the generated declaration/matrix/payload/CLI
  model. The checked contract currently contains six targets, six platforms,
  nineteen pack tokens, eight output-producing packs, and thirteen CLI
  arguments; it remains revision-pinned and does not claim a project release
  matrix.
- Re-audited the ten support profiles and their live workflow targets, plus the
  four-repository example portfolio. Two examples are source-ready, all four
  remotes remain private, one is source-staged, and none is represented as
  published. Build-only, process-smoke, source-capable, private, reserved, and
  published states remain distinct evidence levels.

### Locale and route migration

- `docs_package.py` now emits five canonical English pages under
  `Docs/en/reference/packages`, five hash-pinned Russian counterparts, and five
  durable `Docs/generated/package` pointers. The pointers retain every former
  H2/H3 and all explicit stable entry anchors.
- `docs_support_matrix.py` and `docs_examples.py` now emit canonical EN/RU
  references plus their former generated indexes as durable pointers. Stable
  document IDs belong only to canonical English owners; legacy paths are
  non-human redirects with separate IDs.
- Active repository, BuildTools, guide, metadata, CMake, public-contract,
  evidence, site, and AI links now route directly to locale owners. The
  generated support detail remains discoverable through its owning Support
  Matrix page without duplicating top-level navigation.
- Localization reports 193/195 current physical mirrors, 2 missing, and 98.97
  percent coverage. Source-authored free-text fields in the generated package,
  support, and example models remain in their source language where no
  reviewed translation overlay exists; semantic catalog parity remains a
  production follow-up rather than an implied property of route parity.

### Maintenance and AI contract

- Package, support-matrix, public-example, public-contract, and standalone
  validator tests now pin canonical/Russian/legacy output sets, translation
  hashes, heading parity, manifest ownership, public-contract routing, and
  isolated fixture inventories. Future parser, support-policy, or registry
  changes must regenerate all locale and pointer outputs in the same change.
- Added `release-package-support-and-example-evidence`. The evaluation corpus
  now contains 27 tasks, 63 retrieval checks at 100 percent success and 0.886
  MRR, and 84 answer checks. It rejects valid syntax, build-only lanes, private
  reserved repositories, and unobserved checks as sufficient production
  release evidence.

### Validation evidence

- Snippets pass 292/292 normative blocks, 156 evidence blocks, and 169
  external-parser checks. Complete documentation discovery passes 506/506
  tests. All 31 documentation generators, the external snippet parser, and the
  three CMake structural validators pass; standalone validation accepts 389
  maintained Markdown entries.
- Source site data contains 110 navigation items, 195 English and 193 Russian
  searchable documents, 382 public routes, and 188 planned redirects. Search
  indexes occupy 1,156,514 English bytes and 1,603,881 Russian bytes, both
  within the reviewed 1.75 MiB per-locale budget. AI delivery contains 382
  public documents and 1,809,219 full-context bytes, within its 2 MiB budget.
- Production Jekyll rendering passes 575 routes, 43 static endpoints, and
  79,647 local references. Chromium passes all 1,150 desktop/mobile page
  checks, eleven interaction profiles, and thirteen screenshots with zero
  runtime, responsive-layout, or axe violations.

### Residual production work

- Two physical locale owners remain: Configuration and Data Sources and Tools.
  They are the next direct migration group.
- Semantic translation overlays for source-authored generated descriptions,
  the symbol-ID API-description catalog, broad owner-reviewed stability,
  complete-parity enforcement, manual production assistive-technology/zoom
  review, independent model-family evaluation, landed Pages/CI evidence, and
  public example release gates remain open.

## 2026-08-02 - Forty-sixth physical locale group: generated metadata and native Essentials

### Revision and source audit

- Re-fetched Last Frontier, Engine, and TLA before the group. No incoming range
  was waiting: Last Frontier remains `d54645eda827` and 7/0 ahead/behind
  `origin/main` `48cf5f5dd99e`; Engine remains `fac978a67d1e` and 14/0
  ahead/behind `origin/master` `fee50fb636b5`; TLA remains
  `b603d8fdbc2b` and exactly matches `origin/master`. Existing dirty worktrees
  were preserved.
- Re-audited `Essentials.h`, every authored `.h`/`.cpp`/`.inc`, staged CMake
  source ownership, core-library construction, Natvis, direct tests, and
  higher-layer consumers. The former thematic prose order did not match the
  live strict 23-header dependency order and omitted `Threading.*` plus the two
  `.inc` support files; the canonical guide now records the exact order and
  complete inventory.
- Re-audited metadata/codegen ownership against all eighteen contract models,
  native metadata registration, property/runtime consumers, project remote-call
  supplements, documentation evidence/media, localization, site, browser, and
  AI-delivery generators. The guide now covers the current Audio, Video, GUI
  Runtime, AiControl, public-contract, support/localization, evidence, diagram,
  and screenshot layers rather than the older partial domain set.

### Locale and route migration

- Moved the complete authored references to
  `Docs/en/reference/metadata/index.md` and
  `Docs/en/reference/native/essentials.md`; added full reviewed Russian mirrors
  under the matching `Docs/ru` paths. Translation metadata pins normalized
  source hashes, all 41 metadata H2/H3 headings and all 16 Essentials H2-H4
  headings are paired, and all 28 metadata fenced bodies are byte-identical.
- `Docs/GeneratedApiAndMetadata.md` and `Docs/Essentials.md` are durable
  non-human pointers. They retain every former H2/H3 route and link both locale
  owners; the metadata pointer also carries exact source-file links required by
  standalone validation.
- Updated the manifest, repository/site entrypoints, current human links,
  external-project evidence target, and CMake/main/helper/native script
  reference generators. Generated pages now link their locale-local metadata
  owner instead of returning to the flat route.
- Localization reports 186/195 current translations, 9 missing, 95.38 percent
  coverage, and intentionally incomplete production parity.

### Maintenance and AI contract

- Added seven focused `test_docs_metadata_essentials.py` checks for canonical
  and legacy ownership, translation hash/heading/fence parity, exact umbrella
  order, complete CMake/source inventory, all eighteen generated model domains,
  canonical generator links, and active-route hygiene. The test is mandatory in
  `validate.yml`.
- Added `ai-agent` audience routing for native Essentials and the
  `architecture-essentials-and-metadata-ownership` task. The corpus now has 26
  tasks, 60 retrieval checks at 100 percent success and 0.900 MRR, and 80
  answer checks. It rejects layer bypasses, hand-edited generated output,
  project evidence promoted to Engine authority, and reachability treated as a
  compatibility promise.
- Locale-preserving validation exposed and removed four explicit English
  cross-links from the Russian metadata guide and routed its minimal-project
  evidence to `README.ru.md`. Hash reconciliation covered every EN page changed
  by the canonical-link migration without weakening fenced-code parity.

### Validation evidence

- Snippets pass 292/292 normative blocks, 156 evidence blocks, and 169
  external-parser checks. Complete documentation discovery passes 504/504
  tests; all 31 documentation generators, the external snippet parser, and the
  three CMake structural validators pass. Standalone validation accepts 382
  maintained Markdown entries.
- Source site data contains 111 navigation items, 195 English and 186 Russian
  searchable documents, 375 public routes, and 188 planned redirects. The EN/RU
  search indexes remain within their reviewed 1.75 MiB per-locale budgets. AI
  delivery contains 375 public documents and remains within the reviewed 2 MiB
  full-context budget.
- Production Jekyll rendering passes 561 routes and 43 static endpoints.
  Chromium passes all 1,122 desktop/mobile page checks, eleven interaction
  profiles, and thirteen screenshots with zero runtime, responsive-layout, or
  axe violations.

### Residual production work

- Nine locale owners remain: five generated package-reference pages, the
  generated support-matrix and public-example indexes, Configuration and Data
  Sources, and Tools. The next efficient group is the package reference plus
  the two small generated indexes, followed by the two authored guides.
- The symbol-ID API-description translation catalog, owner-reviewed stable
  descriptions/examples, broad non-internal classification, complete-parity
  enforcement, manual production assistive-technology/zoom review, independent
  model-family evaluation, landed Pages/CI evidence, and example-repository
  publication gates remain open.

## 2026-08-02 - Forty-fifth physical locale group: native script API and public contract index

### Revision and contract audit

- Re-fetched Last Frontier, Engine, and TLA before and after the group. No
  incoming commit was waiting: the audited snapshots remain Last Frontier
  `d54645eda827`, Engine `fac978a67d1e`, and TLA `b603d8fdbc2b`. The local
  project and Engine histories are respectively seven and fourteen commits
  ahead of their fetched remotes; all pre-existing dirty worktree changes were
  preserved.
- Re-audited the native-codegen inventory and the eighteen-domain public
  contract aggregator against current parsers, machine models, generated
  references, change-management policy, Last Frontier integration use, and TLA
  compatibility evidence. The native model contains 2,472 symbols: 953 methods,
  133 properties, 121 events, and 271 settings among the modeled kinds. It has
  601 source-backed descriptions, 1,871 missing descriptions, one explicit
  internal contract, and 2,471 symbols inheriting the default internal label.
- Reachability and generated inventory presence therefore remain separate from
  a compatibility promise. No symbol was promoted by this locale migration;
  owner review, stable-group descriptions/examples, and aggregate contract-diff
  dispositions retain their existing release gates.

### Locale and route migration

- `BuildTools/docs_reference.py` now emits 21 checked Markdown files: seven
  canonical English native API pages, seven reviewed Russian mirrors, and seven
  durable pointers at the former `Docs/generated/api/*` routes. The pointers
  preserve every former H2/H3 plus every explicit group and symbol anchor.
- `BuildTools/docs_public_api.py` now emits the canonical
  `Docs/en/reference/public-contract/index.md`, its reviewed Russian mirror, and
  the root `PUBLIC_API.md` durable route. Manifest ownership keeps the existing
  stable human document ID on the canonical page and assigns separate redirect
  IDs to all old routes. Current human, AI, runtime, rendering, maintenance, and
  repository entrypoints route directly to the locale owners.
- Russian generated pages translate the fixed page scaffold, headings, tables,
  navigation, status policy, provenance guidance, and contract-index prose.
  Source-authored symbol descriptions remain source-backed English text until
  the required stable-symbol-ID translation catalog exists; the pages state
  that limitation rather than claiming semantic description parity.
- Localization reports 184/195 current translations, 11 missing, 94.36 percent
  coverage, and intentionally incomplete parity. The new current owners are the
  seven native API pages and the public contract index.

### Maintenance and AI contract

- The generated-artifact manifest and standalone validator now require all 24
  outputs from both generators, including both locales and every legacy route.
  Focused tests reject stale output, missing locale metadata, changed headings
  or anchors, incorrect relative links, and incomplete multi-output checks.
- EN/RU maintenance guidance now requires contract models/references first,
  both locale indexes and legacy routes next, translation/hash reconciliation,
  then search, evaluation, and AI-delivery regeneration. Existing RU source
  hashes were refreshed only for the EN pages changed by this route migration.
- Added the `migration-public-contract-selection` AI task. It requires the
  reachability/stability distinction, the inventory/default-internal boundary,
  source/model/reference truth order, and an exact Engine commit plus complete
  generated-domain comparison. The corpus now has 25 tasks, 57 retrieval checks
  at 100 percent success and 0.895 MRR, and 76 answer checks.

### Validation evidence

- All 31 checkable documentation generators are current. Snippets pass 287/287
  normative blocks, 156 evidence blocks, and 164 external-parser checks.
  Standalone validation accepts 380 Markdown entries.
- Complete `test_docs*.py` discovery passes 471/471 tests. The three structural
  CMake interface validators and 25/25 AiControl/gameplay/package support tests
  pass.
- Source site data contains 111 navigation items, 195 English and 184 Russian
  searchable documents, 373 public routes, and 188 planned redirects. The EN/RU
  search indexes are 1,155,332 and 1,542,054 bytes. AI delivery contains 373
  public documents and a 1,803,726-byte full-context bundle within its reviewed
  2 MiB cap.
- Production Jekyll rendering with the pinned GitHub Pages stack succeeds.
  Rendered artifact validation passes 557 routes, 43 static endpoints, and
  77,683 local references.
- Chromium passes all 1,114 desktop/mobile page checks, eleven interaction
  profiles, and thirteen screenshots with zero runtime, layout, or axe
  violations. All 194 desktop and 15,661 mobile axe incomplete contrast nodes
  pass the computed-color fallback with no failed or unresolved result.

### Residual production work

- Eleven required locale owners remain: Generated API and Metadata, Native
  Essentials, five package-reference pages, the generated support-matrix and
  public-example indexes, Configuration and Data Sources, and Tools. The next
  direct group owns Generated API and Metadata plus Native Essentials.
- The symbol-ID API-description translation catalog, owner-reviewed stable
  descriptions/examples, broad non-internal classification, complete-parity
  enforcement, manual production assistive-technology/zoom review, independent
  model-family evaluation, and external publication gates remain open.

## 2026-08-02 - Forty-fourth physical locale group: documentation home and generated build interfaces

### Revision and source audit

- Re-fetched Last Frontier, Engine, and TLA before the group. The opening
  snapshots were Last Frontier `d54645eda827`, Engine `fac978a67d1e`, and TLA
  `b603d8fdbc2b`; no upstream commit was waiting in any audited range. Existing
  dirty worktrees were preserved.
- Re-audited the executable `BuildTools/buildtools.py::create_parser()`, the
  then-runtime-consumed `BuildTools/cmake/ProjectInterface.json`, every declared
  stage/helper source, `BuildTools/HelperCliInterface.json`, and every helper
  `create_parser()` against current Last Frontier and TLA integration usage.
- The exact current surfaces contain 44 CMake options, 10 ordered stages, 6
  selected project helpers, 12 main BuildTools commands with 24 command
  arguments, and 9 helper programs with 16 subcommands and 74 arguments. Last
  Frontier exercises all three domains; TLA remains useful simpler CMake
  compatibility evidence but is not authority for the main/helper CLI.

### Locale and route migration

- Added `Docs/en/index.md` and its reviewed `Docs/ru/index.md` mirror as the
  canonical human homes at `/Docs/en/` and `/Docs/ru/`. `Docs/README.md`
  remains a non-human durable pointer and preserves every former H2 route.
- Moved the generated CMake, main BuildTools CLI, and helper CLI Markdown into
  canonical `Docs/en/reference/` owners and added eight reviewed Russian
  mirrors. The machine models remain under `Docs/generated/*.json`.
- Updated all three generators to emit canonical English pages plus durable
  legacy pages under their former `Docs/generated/*` Markdown routes. Every
  former H2/H3 and explicit stable-ID anchor is retained and links onward to
  both locales. Focused tests reject missing locale pointers, headings, anchors,
  stale output, and the CMake-to-package relative route regression found by
  rendered validation.
- Current root, AI, BuildTools, build, upgrade, dependency, pipeline, and public
  contract entrypoints now route directly to localized owners. The public API
  index generator owns these canonical links, so regeneration cannot restore
  the old human routes.

### Maintenance and AI contract

- The EN/RU maintenance guide now requires parser, helper-manifest, or CMake
  interface changes to regenerate the model, canonical English pages, and
  legacy pointers; update the reviewed Russian mirror and source hash in the
  same change; and run focused, localization, site, and rendered validation.
- Added the `migration-build-interface-selection` AI task. It requires the
  Engine/project CMake ownership split, exact `FO_`/cache/`SetOption`/default
  precedence, parser-backed main/helper CLI boundaries, package-domain
  separation, and revision-pinned automation. The corpus now has 24 tasks, 55
  retrieval checks at 100 percent success and 0.903 MRR, and 72 answer checks.
- Localization reports 176/195 current translations, 19 missing, 90.26 percent
  coverage, and intentionally incomplete parity. The nine new current pages
  are the documentation home and eight generated build-interface pages.

### Validation evidence

- Generator checks are current for all 31 checkable documentation artifacts.
  Snippets pass 287/287 normative blocks, 156 evidence blocks, and 164 external
  parser checks. Standalone validation accepts 372 Markdown entries.
- The focused migration/navigation set passes 52 tests. Complete
  `test_docs*.py` discovery passes 470/470 tests. The three structural CMake
  interface validators and 25 AiControl/gameplay/package support tests pass.
- Production Jekyll rendering with the repository pins succeeds. Rendered
  artifact validation passes 541 routes, 43 static endpoints, and 72,948 local
  references; source site data contains 111 navigation items, 195 English and
  176 Russian search documents, 365 public routes, and 188 planned redirects.
- Chromium passes all 1,082 route/profile checks, eleven interaction profiles,
  and thirteen screenshots with zero errors or axe violations. All 54 desktop
  and 10,103 mobile axe incomplete contrast nodes pass the computed-color
  fallback with no failed or unresolved nodes.

### Residual production work

- Translate and review the remaining 19 required pages. The immediate locale
  candidates are generated API/metadata, Native Essentials, package/support/
  public-example generated references, the native API page set,
  Configuration and Data Sources, and Tools.
- Same-domain clean Markdown aliases, broad non-internal stability review,
  manual production assistive-technology/zoom review, independent model-family
  evaluation, and the external publication gates remain open.

## 2026-08-02 - Forty-third physical locale group: Mapper Interactive Manual and Mapper Tools

### Mapper implementation and workflow audit

- Moved the stock interactive manual to
  `Docs/en/how-to/tools/mapper-interactive.md`, added its reviewed Russian
  counterpart, and retained `Docs/MapperManual.md` as a durable pointer.
  Moved the reusable automation guide to
  `Docs/en/how-to/tools/mapper.md`, added its Russian counterpart, and retained
  `Docs/MapperTools.md` as a durable pointer. The two pointers preserve all 36
  former H2/H3 routes, while current Engine and Last Frontier entrypoints link
  directly to the locale-aware owners.
- Re-audited `MapperEngine`, its application host, menus, windows, controls,
  selection/clipboard/history, Inspector property path, layout storage/reset,
  particle tools, focused-viewer embedding, map loading/saving/teardown,
  mapper-side bindings, camera/overlay/visibility controls, TGA and atlas
  readback, application-level ImGui composition, settings, and the
  MinimalMultiplayer capture profile against the current dirty worktree.
- Added the two source-backed shortcuts missing from the former manual:
  `Ctrl+D` toggles current-map scroll checking and `Ctrl+B` invokes blocked-hex
  marking. Normal interactive work retains scroll checking; deliberate
  overscan capture may disable it.
- Corrected the former map-item animation statement. Mapper mode calls
  `ItemHexView::RefreshAnim()`, stops the sprite, and applies `SetTime(0.0f)`;
  the guide no longer calls this a normalized-time API. Critter viewer
  playback and representative client behavior remain separate evidence.
- Pinned map-container declaration lookup, sibling preservation, constrained
  `SaveMapToPath` traversal handling, loaded-map teardown, entity/property
  exports, view controls, synchronous map-only capture, deferred full-window
  capture through `Application::OnBeforePresent`, one-pending-request behavior,
  one-loop completion, TGA-only output, and non-uniform pixel acceptance.
- Re-reviewed Last Frontier's project-owned `Scripts/MapperRender.fos` and
  `Tools/MapPreview/generate_map_preview.py` as evidence for single-process
  batching, warmup, per-map view plans, output processing, and project
  acceptance. TLA retains a project Mapper target but no reusable focused
  viewer or capture workflow, so it remains compatibility and negative
  evidence rather than copied guidance.

### Governance, localization, and generated delivery

- The manifest now owns both guides at canonical EN paths with
  `current/retain` status and registers both former flat paths as redirects to
  stable IDs. Content navigation and the AI start set retain those IDs. The
  Mapper screenshot manifest now names the canonical interactive manual as
  owner, and current Engine/project routing plus update instructions use the
  canonical paths.
- `test_docs_mapper_tools.py` adds ten source-backed tests for UI/hotkeys,
  settings, map/entity/view exports, traversal safety, screenshot lifecycle,
  map-item freeze behavior, locale/fence parity, all legacy headings, fixture
  provenance, manifest/navigation/AI/evidence ownership, current entrypoints,
  and mandatory CI wiring. `docs_validate.py` fails closed when the suite is
  removed from the workflow.
- External evidence remains 30 records and now contains 180 exact source
  references from Last Frontier `d54645e` and TLA `b603d8f`: 24 promoted, 2
  boundary-owned, 1 promotion candidate, and 3 project-owned decisions.
- Localization reports 167/195 current translations (85.64 percent), with 28
  missing. The Mapper manual pins normalized English hash
  `bcd9ff6c9b6aee35af2fb44a0e1b213368f54247f6cc9522ed95677091ae4251`;
  Mapper Tools pins
  `1ba97b08fd9d40d6cbbff4dfd735dcf70616da6ce5675719b1007762d9c8ccd4`.
  All English pages touched only for canonical links have matching reviewed
  Russian text and refreshed hashes.
- Site generation contains 111 navigation items, 356 public routes, 188
  planned redirects, and 195 English plus 167 Russian searchable documents.
  Search indexes occupy 1,159,647 English and 1,378,290 Russian bytes. AI
  delivery contains 356 public documents and 1,823,450 full-context bytes.
- Added `automate-mapper-map-capture` to the deterministic AI set. It requires
  save safety, warmed `Game.OnStart`/`Game.OnLoop` batching, explicit
  map-only versus full-window capture, pixel evidence, and the Engine/project
  ownership boundary. The corpus now has 23 tasks, 52 retrieval checks at 100
  percent success and 0.907 MRR, and 68 answer checks. Snippets remain 287/287
  normative, 156 evidence, and 164 external-parser checks.

### Standalone, rendered, and browser validation

- Mapper documentation passes 10/10 focused tests. Final split discovery
  passes all 470 documentation tests: 455 in the main run and 15 in the
  browser/snippet/rendered-artifact modules. Gameplay runner, minimal package,
  AiControl protocol, package security, and packaging-matrix supporting suites
  pass 25/25; all three structural CMake interface validators pass.
- All 31 documentation generators/checkers report current output, the
  external snippet/parser pass is green, standalone validation accepts 363
  Markdown entries, and the eighteen-domain contract diff reports zero
  changes or required dispositions. `git diff --check` reports no whitespace
  errors in Engine or Last Frontier.
- The production-mode local Jekyll build uses Ruby 3.3.4, Bundler 2.5.11,
  `github-pages` 232, and Jekyll 3.10.0. The artifact gate passes 523 rendered
  routes, 43 static endpoints, and 70,408 local references.
- Node 24.16.0, Playwright 1.62.0, Chromium 151.0.7922.34, and axe-core 4.12.1
  pass all 1,046 desktop/mobile page checks, eleven interaction profiles, and
  thirteen screenshots with zero errors or axe violations. The contrast
  fallback passes all 54 desktop and 9,848 mobile incomplete nodes with zero
  failed or unresolved results.

### Residual and revision state

- Remaining locale work is 28 required pages plus the symbol-ID API-description
  catalog and complete-parity enforcement. Landed CI/Pages evidence,
  independent model-family evaluation, manual production keyboard, 200
  percent zoom and representative screen-reader review, final
  platform/tutorial execution evidence, and the owner-gated public-example
  publication work remain open.
- Opening and pre-report closing fetches found no incoming range. Last Frontier
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` remains 7 ahead / 0 behind
  `origin/main` `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`; Engine
  `fac978a67d1e601eb77389e8dc562d7e511705a0` remains 14 ahead / 0 behind
  `origin/master` `fee50fb636b5bd1e30509aded929df1fc0e95db5`; and TLA
  `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` exactly matches
  `origin/master`. Existing unstaged work remains in Last Frontier and Engine
  (64 and 393 porcelain entries before this report); staged and unmerged sets
  are not modified by this group. No commit or push is part of this group.

## 2026-08-02 - Forty-second physical locale group: Animation and Particle Viewers

### Viewer implementation and production-workflow audit

- Replaced the former flat mixed-purpose page with the independent canonical
  `Docs/en/how-to/tools/animation-particle-viewers.md`, added a complete
  Russian counterpart, and retained `Docs/ViewerTools.md` as a durable pointer
  preserving all 14 canonical H2/H3 routes. Engine and Last Frontier routing,
  maintenance, build, model, particle, Mapper, application, and repository
  entrypoints now link directly to the locale-aware owner.
- Re-audited both focused implementations and hosts, Mapper embedding, CMake
  source/library/application/output wiring, package capabilities, resource
  mounting, per-user settings, and the MinimalMultiplayer target shape. Both
  standalone hosts own a client-side service set and minimal frame loop, omit
  gameplay startup/network/main-loop behavior, use `BakerDataSource` during
  development, and mount configured client plus Mapper resources when
  packaged.
- Pinned the complete AnimationViewer contract: prototype filtering, exact 3D
  animation-pair discovery, 2D directional probing, project-defined model
  layer mapping, atlas versus direct geometry drawing, 0.25x through 8.00x
  zoom, facing, pan, root/name/render/view overlays, one-shot return to idle,
  hierarchy bones and attachments, and the exact persisted/non-persisted
  boundary.
- Pinned the complete ParticleViewer contract: active-factory `.spk`/`.efk`
  discovery, visible load failures, fixed seed/replay/new-seed behavior,
  loop/prewarm, direction, camera-relative world-particle rebasing, atlas
  versus direct-scene drawing, wireframe/root/frame overlays, and the exact
  persisted/non-persisted boundary.
- Added an explicit production evidence ladder: validate source and bake,
  isolate the asset in a focused viewer, prove placement/depth in Mapper,
  prove timing/ownership/visibility/networking/load in a representative client
  scene, then retain exact revisions, configuration, controls, logs, and
  captures. A startup smoke, empty MinimalMultiplayer window, or manual viewer
  screenshot is no longer easy to misstate as visual, gameplay, or automated
  regression evidence.
- The guide keeps source authoring in the owning model/particle tools and keeps
  project presets, assets, acceptance thresholds, package policy, and captures
  outside Engine. It records Last Frontier's stable viewer tasks,
  developer-only package, model and particle review workflows, and Engine-pin
  maintenance trigger as external evidence. TLA has no equivalent focused
  viewer workflow and remains negative compatibility evidence, not a template.

### Governance, localization, and generated delivery

- Migrated the manifest owner to the canonical EN path with `retain/current`
  status and registered the old flat path as a redirect to stable ID
  `viewer-tools`. The content navigation ID remains stable, so external AI and
  site clients do not need a task/schema migration.
- Expanded `mapper-and-focused-viewers` from four to eight external paths.
  Exact source verification passes against Last Frontier `d54645e` and TLA
  `b603d8f`. The inventory remains 30 records and now contains 178 exact source
  references: 24 promoted, 2 boundary-owned, 1 promotion candidate, and 3
  project-owned decisions.
- Eight focused tests pin source/CMake/package behavior, both localized guide
  structures, byte-identical fenced commands, every legacy heading, active
  entrypoints, manifest/navigation/AI routing, project evidence, and CI
  inclusion. `docs_validate.py` now makes the viewer focused suite mandatory;
  its isolated fail-closed regression passes in 2.47 seconds.
- The Russian guide pins normalized English hash
  `1d2d5ed5d848d23525b1166fa064ab5d18a64197fa61000d727cd8d0a9489e8d`.
  Localization reports 165/195 current translations (84.62 percent), with 30
  missing. Related repository, particle-authoring, applications, source-tree,
  documentation-maintenance, and AI-evaluation mirrors carry current routes
  and hashes.
- Site generation contains 111 navigation items, 354 public routes, 188
  planned redirects, and 195 English plus 165 Russian searchable documents.
  Search indexes occupy 1,159,384 English and 1,351,076 Russian bytes. AI
  delivery contains 354 public documents and 1,821,840 full-context bytes.
- The viewer AI task now requires separate focused-viewer, Mapper,
  representative-client, and retained-provenance evidence. The corpus remains
  22 tasks and 50 retrieval checks at 100 percent success and 0.903 MRR, with
  64 answer checks. Snippets remain 287/287 normative, 156 evidence, and 164
  external-parser checks.

### Standalone, rendered, and browser validation

- Viewer Tools passes 8/8 focused tests. Complete documentation discovery
  passes 459/459 tests. Gameplay runner, minimal package, AiControl protocol,
  package security, and AngelScript CMake supporting suites pass 23/23 in one
  run; all three structural CMake interface validators pass.
- All 31 documentation generators/checkers report current output, including
  exact external-source verification. Standalone validation accepts 361
  Markdown entries, and `git diff --check` reports no whitespace error in
  Engine or Last Frontier. No viewer/CMake/runtime source changed in this
  group, so the previously recorded clean viewer builds and startup smokes
  remain historical evidence rather than being presented as a new run.
- Standalone validation caught one link to the planned but not yet migrated
  Tools index; it was corrected to the existing durable owner. The first
  rendered artifact pass then caught three line-wrapped Russian Markdown links
  that Jekyll had left as `.md` URLs; all were reformatted and the dependent
  site/AI artifacts regenerated before acceptance.
- The production-mode local Jekyll build with Ruby 3.3.4, Bundler 2.5.11, and
  `github-pages` 232 succeeds. The corrected artifact gate passes 519 rendered
  routes, 43 static endpoints, and 69,841 local references.
- Node 24.16.0, Playwright 1.62.0, Chromium 151.0.7922.34, and axe-core 4.12.1
  pass all 1,038 desktop/mobile page checks, eleven interaction profiles, and
  thirteen screenshots with zero errors or axe violations. The reviewed
  contrast fallback passes all 53 desktop and 9,837 mobile incomplete nodes
  with zero failed or unresolved results.

### Residual and revision state

- Remaining locale work is 30 required pages plus the symbol-ID
  API-description catalog and complete-parity enforcement. Landed CI/Pages
  evidence, independent model-family evaluation, manual production keyboard,
  200 percent zoom and representative screen-reader review, final
  platform/tutorial execution evidence, and the owner-gated public-example
  publication work from Group 35 remain open.
- Opening and pre-report closing fetches found no incoming range. Last Frontier
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` remains 7 ahead / 0 behind
  `origin/main` `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`; Engine
  `fac978a67d1e601eb77389e8dc562d7e511705a0` remains 14 ahead / 0 behind
  `origin/master` `fee50fb636b5bd1e30509aded929df1fc0e95db5`; and TLA
  `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` exactly matches
  `origin/master`. Existing unstaged work remains in Last Frontier and Engine
  (64 and 392 porcelain entries before this report); staged and unmerged sets
  are not modified by this group. No commit or push is part of this group.

## 2026-08-02 - Forty-first physical locale group: AngelScript Style and Refactoring

### Compiler, formatter, state, and attribute audit

- Replaced the former flat mixed-purpose guide with the independent canonical
  `Docs/en/how-to/scripting/style-and-refactoring.md`, added a complete Russian
  counterpart, and retained `Docs/AngelScriptStyle.md` as a durable pointer
  preserving all 40 canonical H2/H3 routes. Engine, BuildTools, documentation,
  and Last Frontier entrypoints now link directly to the locale-aware owner.
- Re-derived script assembly from `AngelScriptBackend.cpp`: configured files
  become one generated root include list per side; only the first line is
  inspected for `Sort N`; missing values use zero; stable ordering is numeric
  then filename stem. The guide separates this preprocessing/declaration order
  from runtime `[[ModuleInit(priority)]]` order, which remains owned by the
  lifecycle guide.
- Pinned independent SERVER, CLIENT, and MAPPER module compilation and the
  exact always-defined `0`/`1` side macros. The guide now explicitly requires
  `#if SIDE`; `#ifdef SIDE` is recorded as a cross-side isolation bug rather
  than an acceptable spelling.
- Re-audited the Engine formatter wrapper and CoreScripts `.clang-format`:
  clang-format major 20, four spaces, no tabs, 160 columns, function/control
  brace policy, outer-namespace layout, preserved include/comment order,
  `FO_CLANG_FORMAT` discovery, masked nullable/named-argument repairs, UTF-8
  BOM removal, retained LF/CRLF convention, and one EOF terminator. The guide
  also corrects the ownership boundary: Engine `format-source` covers Engine
  `Source`, while an embedding project must format its separate authored tree
  through its own documented wrapper.
- Verified all eleven current CoreScripts for namespace-to-file ownership,
  balanced guards, UTF-8/BOM and EOF shape. The reusable naming/comment policy
  remains intentionally small and does not promote a game's language, header,
  terminology, helper order, or file-size thresholds into Engine policy.
- Re-derived the mutable-global gate from settings, backend, and baker tests.
  Mutable module globals are rejected unless their namespace matches a prefix
  in `Script.MutableGlobalsAllowedNamespaces`; const globals remain legal.
  The stale test comment naming removed path-based exemptions was corrected to
  the current namespace setting without changing runtime behavior.
- Re-derived attributed-call validation and documented all twelve current
  built-in blockers: events, time events, animation callbacks, property
  accessors, three remote-call directions, item trigger/static functions,
  module initializers, and invoke entries. Project extras and direct-call
  namespace exceptions are narrow compatibility surfaces. Any non-blocking
  attribute remains a transitive marker, with `[[Async]]` as the common
  lifecycle example.
- Expanded refactoring guidance into mechanical, structural, behavioral, and
  contract classes, each with its own minimum evidence. The safe batch route
  now starts with authored/generated and side/compatibility ownership, captures
  a baseline, changes one class, regenerates and formats, compiles every
  affected side warning-free, and proves the observable behavior.

### Project evidence, governance, and generated delivery

- Expanded the existing `angelscript-style-and-refactoring` evidence record
  from four to eight exact paths. Last Frontier contributes its current
  project formatter and `Test_`-only mutable-global/direct-call exceptions as
  positive evidence. TLA contributes an independent formatter plus a broad
  production mutable-global allowlist and ongoing refactoring log as negative
  migration evidence, never an Engine default.
- Exact source verification passes against Last Frontier `d54645e` and TLA
  `b603d8f`. The inventory remains 30 records but now contains 174 exact source
  references: 24 promoted, 2 boundary-owned, 1 promotion candidate, and 3
  project-owned decisions.
- Seven focused tests pin canonical EN/RU structure and byte-identical fences,
  all durable headings, backend sort and side macros, namespace-based mutable
  globals, direct-call blockers and marker logic, formatter repair and file
  shape, every CoreScripts namespace/guard, manifest/navigation/evaluation,
  exact project evidence, maintenance routes, and CI inclusion. A new
  `docs_validate.py` regression makes the focused suite mandatory in the
  documentation workflow.
- The Russian guide pins normalized English hash
  `fdcfcd87c4c6b018aab8c9e8db2424f2500a71a1224a9622472462c33fdf10ab`.
  Localization reports 164/195 current translations (84.10 percent), with 31
  missing. Related BuildTools, scripting-runtime, generated-content,
  build-workflow, documentation-maintenance, and AI-evaluation mirrors were
  updated with current canonical links and hashes.
- Site generation contains 111 navigation items, 353 public routes, 188
  planned redirects, and 195 English plus 164 Russian searchable documents.
  Search indexes occupy 1,158,958 English and 1,339,500 Russian bytes. AI
  delivery contains 353 public documents and 1,819,614 full-context bytes.
- The AngelScript AI task now separately pins file/side module construction and
  the mutable-global compatibility boundary. The corpus remains 22 tasks and
  50 retrieval checks at 100 percent success and 0.903 MRR, with 63 answer
  checks. Snippets pass 287/287 normative, 156 evidence, and 164
  external-parser checks.

### Standalone and rendered validation

- AngelScript Style and Refactoring passes 7/7 focused tests. External
  evidence, localization, site, AI-evaluation, AI-delivery, and documentation
  foundation suites pass. Complete documentation discovery passes 458/458
  tests. Gameplay runner, minimal package, AiControl protocol, package
  security, and AngelScript CMake suites pass 4/4, 4/4, 8/8, 6/6, and 1/1;
  all three structural CMake interface validators pass.
- Every affected generator reports current output, exact external project paths
  verify, `git diff --check` reports no whitespace error, and standalone
  validation accepts 360 Markdown entries. No `.fos`, compiler, formatter, or
  runtime behavior changed, so this group did not require a native rebuild or
  execution of `format-source` over the pre-existing dirty Engine source. The
  focused formatter test exercises BOM, CRLF, nullable, array, cast/template,
  named-argument, and literal/comment preservation on an isolated file.
- The production-mode local Jekyll build with Ruby 3.3.4, Bundler 2.5.11, and
  `github-pages` 232 succeeds. The artifact gate passes 517 rendered routes, 43
  static endpoints, and 69,568 local references.
- Node 24.16.0, Playwright 1.62.0, Chromium 151.0.7922.34, and axe-core 4.12.1
  pass all 1,034 desktop/mobile page checks, eleven interaction profiles, and
  thirteen screenshots with zero errors or axe violations. The reviewed
  contrast fallback passes all 53 desktop and 9,837 mobile incomplete nodes
  with zero failed or unresolved results.

### Residual and revision state

- Remaining locale work is 31 required pages plus the symbol-ID API-description
  catalog and complete-parity enforcement. Landed CI/Pages evidence,
  independent model-family evaluation, manual production keyboard, 200 percent
  zoom and representative screen-reader review, final platform/tutorial
  execution evidence, and the owner-gated public-example publication work from
  Group 35 remain open.
- Opening and closing fetches found no incoming range. Last Frontier
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` remains 7 ahead / 0 behind
  `origin/main` `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`; Engine
  `fac978a67d1e601eb77389e8dc562d7e511705a0` remains 14 ahead / 0 behind
  `origin/master` `fee50fb636b5bd1e30509aded929df1fc0e95db5`; and TLA
  `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` exactly matches
  `origin/master`. Existing unstaged work remains in Last Frontier and Engine
  (64 and 392 porcelain entries); staged and unmerged sets are empty in all
  three repositories. No commit or push is part of this group.

## 2026-08-02 - Fortieth physical locale group: Native and AngelScript Debugging

### Native, mixed-stack, crash, and live-attach audit

- Replaced the old mixed-purpose page with the independent canonical
  `Docs/en/troubleshooting/debugging.md`, added a complete Russian counterpart,
  and retained `Docs/Debugging.md` as a durable pointer preserving all 44
  canonical H2/H3 routes. Active Engine and Last Frontier entrypoints now link
  directly to the locale-aware owner while the old URL remains compatible.
- Re-derived symbol and configuration behavior from current CMake source:
  `expr_DebugInfo` excludes only `MinSizeRel`; MSVC uses `/Zi` and
  `/DEBUG:FULL`, Linux/macOS use `-g` and `-rdynamic`, and `/JMC` is attached to
  MSVC Debug and RelWithDebInfo. The guide distinguishes symbol availability
  from `FO_DEBUG`/assertion/CRT semantics, records MSVC-only
  `Release_Debugging`, and routes sanitizer/platform-specific limits without
  presenting Debug success as release-like acceptance.
- Re-audited cached native-debugger detection and break behavior on Windows,
  Linux, and macOS. The guide now explains why late attach does not refresh
  Engine-triggered breaks and why a process launched under a native debugger
  does not exercise the ordinary backward-cpp crash-to-log route.
- Re-derived mixed stack behavior from `StackTrace`, `ExceptionHandling`,
  `AngelScriptContext`, logging, and focused tests. Script layers can interleave
  with native bridges through birth anchors; `FO_STACK_TRACE_ENTRY` is a Tracy
  zone rather than a manual shadow stack; exception origin and catch traces,
  bounded native resolution cache, safe address fallback, synchronous fatal
  logging, alternate signal stacks, and every destructive `FO_SELFTEST_CRASH`
  mode are explicit. Windows minidumps, Linux core setup, macOS crash archives,
  symbol stores, upload, retention, and privacy remain project/operator-owned.
- Verified generated MSVC attachment of Engine Natvis/NatJMC plus vendored GLM,
  ImGui, small-vector, and ufbx visualizers. Visualizers are documented as an
  inspection aid, not an ownership, lifetime, or optimizer guarantee.
- Re-derived the live AngelScript endpoint from settings, backend, context, and
  transport source: disabled by default, loopback bind, line-cue retention and
  bytecode-optimization cost, TCP range `43000..44999`, UDP discovery `43001`,
  protocol v1, one active session, role/process selection, basename-keyed
  breakpoints, script-only attach stacks, and read-only locals. Pause,
  continue, step, breakpoints, stack, locals, disconnect, and runtime stop
  events are separated from unsupported globals, mutation, memory, expression,
  advanced-breakpoint, and reverse controls.
- The endpoint has no authentication, authorization, encryption, or integrity
  protection, so the documented contract keeps it on loopback and forbids
  public, untrusted-LAN, production-service, and shared-CI exposure. The bundled
  adapter remains source-capable rather than release-qualified: it has no lock
  file, checked VSIX, publication evidence, required build lane, or live
  endpoint CI; its current TypeScript test exercises the sample/mock runtime.

### Project evidence, governance, and generated delivery

- Added `native-debugging-workflows` to the exact external-evidence inventory.
  Last Frontier supplies Windows/Linux native profiles, explicit AngelScript
  enablement in compounds, a loopback base bind, static workflow checks, and a
  Linux crash-diagnostic subprocess lane. TLA supplies independent profiles but
  combines disabled base enablement with a wildcard bind and no explicit
  compound override; this is recorded as negative evidence, never a template.
- Source verification passes against exact Last Frontier `d54645e` and TLA
  `b603d8f` trees. The inventory now contains 30 records and 170 exact source
  references: 24 promoted, 2 boundary-owned, 1 promotion candidate, and 3
  project-owned decisions.
- Seven focused tests pin the guide, build flags, debugger detection, stack and
  crash source, Natvis, endpoint commands and limitations, adapter delivery
  truth, exact project evidence, EN/RU fence parity, legacy headings, manifest,
  maintenance triggers, and CI inclusion. A separate validator regression makes
  the focused suite mandatory in `.github/workflows/validate.yml`. Last
  Frontier's six workflow tests and its static checker remain green after the
  canonical-link update.
- The Russian page preserves its text fence byte-for-byte and pins normalized
  English hash
  `c8c084f6db63f9503e5302bf455969e6fb8b31a75223fb207963b8bc0c092f17`.
  Localization reports 163/195 current translations (83.59 percent), with 32
  missing.
- The complete Russian debugging guide exceeded the former 1.25 MiB search
  ceiling at 163/195 coverage. ADR 0004, the site publication guide, manifest,
  tests, roadmap, and Russian mirrors now explicitly use a fail-closed 1.75 MiB
  per-locale budget intended for the complete corpus. No document or token
  class was removed to pass. Current search uses 1,156,120 English and
  1,324,711 Russian bytes.
- Site generation contains 111 navigation items, 352 public routes, 188 planned
  redirects, and 195 English plus 163 Russian searchable documents. AI delivery
  contains 352 public documents and 1,808,291 full-context bytes. The
  architecture-boundary query was made task-specific after the new debugging
  page exposed an overly broad retrieval phrase; the 22-task/50-check gate is
  again 100 percent with 0.913 MRR. Snippets pass 287/287 normative, 156
  evidence, and 164 external-parser checks.

### Standalone and rendered validation

- Native/AngelScript Debugging passes 7/7 focused tests. External evidence,
  localization, site, and governance suites pass 7/7, 7/7, 11/11, and 4/4.
  Complete documentation discovery passes 454/454 tests. Gameplay runner,
  minimal package, AiControl protocol, package security, and AngelScript CMake
  suites pass 4/4, 4/4, 8/8, 6/6, and 1/1. All three structural CMake interface
  validators pass, exact external project paths verify, all generated artifacts
  report current, and standalone validation accepts 359 Markdown entries.
- No Engine C++ or adapter behavior changed, so this documentation group did
  not require a native rebuild, destructive crash execution, or a new live
  debugger session. Existing native unit tests and Last Frontier crash/runtime
  evidence are cited only to define current coverage and remaining gaps.
- The production-mode local Jekyll build with Ruby 3.3.4, Bundler 2.5.11, and
  `github-pages` 232 succeeds. The artifact gate passes 515 rendered routes, 43
  static endpoints, and 69,268 local references.
- Node 24.16.0, Playwright 1.62.0, Chromium 151.0.7922.34, and axe-core 4.12.1
  pass all 1,030 desktop/mobile page checks, eleven interaction profiles, and
  thirteen screenshots with zero errors or axe violations. The reviewed
  contrast fallback passes all 53 desktop and 9,837 mobile incomplete nodes
  with zero failed or unresolved results.

### Residual and revision state

- Remaining locale work is 32 required pages plus the symbol-ID API-description
  catalog and complete-parity enforcement. Final starter/tutorial execution
  evidence for the platform family, a reproducibly distributed debugger
  adapter and live endpoint CI, landed CI/Pages evidence, independent
  model-family evaluation, and manual production keyboard, 200 percent zoom,
  and representative screen-reader review remain open. Public-example
  publication retains the owner-gated work recorded by Group 35.
- Opening and closing fetches found no incoming range. Last Frontier
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` remains 7 ahead / 0 behind
  `origin/main` `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`; Engine
  `fac978a67d1e601eb77389e8dc562d7e511705a0` remains 14 ahead / 0 behind
  `origin/master` `fee50fb636b5bd1e30509aded929df1fc0e95db5`; and TLA
  `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` exactly matches
  `origin/master`. Existing unstaged work remains in Last Frontier and Engine
  (64 and 391 porcelain entries); staged and unmerged sets are empty in all
  three repositories. No commit or push is part of this group.

## 2026-08-02 - Thirty-ninth physical locale group: Web Build, Packaging, and Browser Debugging

### Source, package, runtime, hosting, and project-evidence audit

- Migrated the Engine-owned Web procedure to
  `Docs/en/how-to/platforms/web-debugging.md`, added a complete independent
  Russian counterpart, and retained `Docs/WebDebugging.md` as a durable pointer
  preserving all 24 canonical H2/H3 routes.
- Re-audited the exact Emscripten 6.0.3 workspace pin and host preparation,
  BuildTools configure/build runners, the `Web-wasm` CMake contract, WebGL2-only
  renderer, WebAssembly stack/memory/filesystem/exception/export flags,
  `package-web-debug`, package grammar and `file_packager.py`, the stock HTML
  shell, and the generated development server. The guide now separates bake,
  build, package, local HTTP load, browser runtime, and production release
  evidence rather than treating a successful link as browser support.
- Documented the exact package and security boundary: Web packaging is
  client/wasm/resources-only, patches config/resources/build identity into the
  wasm, emits `Resources.data`/`Resources.js`, and exposes query pairs as Engine
  arguments. Query strings are a public logging/history/referrer surface. The
  stock `web-server.py` binds all host interfaces, has no TLS/authentication or
  explicit wasm/isolation headers, and is development-only. Its HTML comment
  now names the real WebSocket host/port settings rather than the native TCP
  pair.
- Re-derived runtime behavior from `WebRelated`, OpenGL, networking, settings,
  logging, and updater source: adaptive `#canvas` layout, permission-sensitive
  clipboard handling, IDBFS initial hydration without a generic automatic
  write-back guarantee, Emscripten main-loop ownership, browser-console error
  evidence, `WebSocketHost`/`WebSocketPort` plus `ws`/`wss`, no Web UDP/proxy
  path, `Web-wasm` updater identity, and unsupported native-module self-update.
- Expanded production guidance to MIME, HTTPS/WSS, proxy upgrades, atomic
  artifact deployment, cache/compression, optional isolation headers, security
  headers, secrets/privacy, observability, browser acceptance, performance,
  rollout, and rollback. Exact Last Frontier evidence now includes its
  loopback Playwright runner and three packaged browser tests; TLA remains
  independent preset/configuration discovery with no equivalent checked
  browser-package lane.

### Canonical routes, localization, and generated delivery

- Seven focused tests pin guide boundaries, toolchain/build flags, package
  output, stock shell/server behavior, runtime/network/storage/updater source,
  exact external evidence, EN/RU fence parity, durable headings, manifest
  ownership, CI inclusion, and maintenance triggers. A separate validator test
  makes the focused Web suite mandatory in the documentation workflow.
- The Russian page preserves all eight fenced blocks byte-for-byte and pins
  normalized English hash
  `8e73eaf40678c05868adbf33f63657bd1a1e36b071a4f15fa0d0501a1a7872fa`.
  Localization reports 162/195 current translations (83.08 percent), with 33
  missing.
- Site generation contains 111 navigation items, 351 public routes, 188
  planned redirects, and 195 English plus 162 Russian searchable documents.
  Search indexes occupy 1,151,224 English bytes and 1,301,202 Russian bytes.
- AI delivery contains 351 public documents and 1,792,388 full-context bytes;
  deterministic evaluation remains 22 tasks and 50 retrieval checks at 100
  percent success and 0.897 MRR. Snippets pass 287/287 normative, 156 evidence,
  and 164 external-parser checks.

### Standalone and rendered validation

- Web Debugging passes 7/7 focused tests. Package security, package docs,
  support matrix, localization, external evidence, and snippet suites pass
  6/6, 6/6, 6/6, 7/7, 7/7, and 5/5. Complete documentation discovery passes
  446/446 tests; gameplay runner, minimal package, AiControl protocol, package
  security, and AngelScript CMake suites pass 4/4, 4/4, 8/8, 6/6, and 1/1.
  All three structural CMake interface validators pass, every affected
  generator reports current output, and standalone validation accepts 358
  Markdown entries. No C++ or package behavior changed, so this group required
  no native rebuild; the only runtime-template edit corrected a comment and is
  covered by the focused shell/source assertions.
- The production-mode local Jekyll artifact passes 513 rendered routes, 43
  static endpoints, and 68,965 local references with `github-pages` 232,
  Bundler 2.5.11, and local Ruby 3.3.4.
- Chromium passes all 1,026 desktop/mobile page checks, eleven interaction
  profiles, and thirteen screenshots with zero errors or axe violations. The
  computed-color fallback passes all 53 desktop and 9,790 mobile incomplete
  contrast nodes with no failed or unresolved result.

### Residual and revision state

- Remaining locale work is 33 required pages. Native Debugging is the remaining
  procedure in this platform family. Final starter/tutorial execution evidence,
  the symbol-ID API-description catalog, complete-parity enforcement, landed
  CI/Pages evidence, independent model-family evaluation, and manual production
  keyboard, 200 percent zoom, and representative screen-reader review remain.
  Public-example publication retains the owner-gated work recorded by Group 35.
- The opening and closing fetches found no incoming range: Last Frontier
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` remains 7 ahead / 0 behind
  `origin/main` `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`; Engine
  `fac978a67d1e601eb77389e8dc562d7e511705a0` remains 14 ahead / 0 behind
  `origin/master` `fee50fb636b5bd1e30509aded929df1fc0e95db5`; and TLA
  `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` exactly matches
  `origin/master`. The project and Engine worktrees retain existing unstaged
  work (62 and 390 porcelain entries respectively); staged and unmerged sets
  are empty in all three repositories. No commit or push is part of this group.

## 2026-08-02 - Thirty-eighth physical locale group: Android Build, Packaging, and Device Debugging

### Source, packaging, runtime, and project-evidence audit

- Migrated the Engine-owned Android procedure to
  `Docs/en/how-to/platforms/android-debugging.md`, added a complete independent
  Russian counterpart, and retained `Docs/AndroidDebugging.md` as a durable
  pointer preserving all 23 prior H2/H3 routes.
- Re-audited Android platform registration, workspace preparation, SDK/NDK and
  Java pins, native build presets, `package-android-debug`, package staging,
  Gradle assembly, manifest/template behavior, `FOnlineActivity`, resource-copy
  revision handling, `android_device.py`, and native-updater limits. The guide
  distinguishes source-capable Android x86 from the ARM32/ARM64 CI lanes and
  native build success from Gradle, APK, device, release, and store acceptance.
- Pinned the current toolchain contract: command-line tools `14742923`, NDK
  `android-ndk-r29`, native API 23, build-tools 34.0.0, platform 35, Java 17,
  Gradle 8.12, and Android Gradle Plugin 8.7.3. Packaging renames the client
  library to `libmain.so`, stages baked resources under Android assets, requires
  an all-or-none release signing tuple, passes passwords only through
  environment variables, and uses an isolated Gradle home.
- Documented the exact runtime boundary: the activity loads only `main`, accepts
  only the typed `server_host` Intent extra, copies assets after an APK revision
  change or missing metadata, and retains the cache. The Wi-Fi-oriented ADB
  helper discovers explicit, cached, connected, mDNS, or interactive endpoints,
  does not implement pairing, installs with `adb install -r`, and exposes fixed
  launch/logcat behavior.
- Expanded external evidence to exact Last Frontier package settings, ARM64
  local tasks, all-ABI cross-platform CI, and current non-APK release workflows,
  plus TLA's three presets and two CI build lanes. TLA remains discovery
  evidence; no APK/device qualification claim was promoted from it.

### Canonical routes, localization, and generated delivery

- Eight focused tests pin support qualification, toolchain values, package and
  signing behavior, activity/resource lifecycle, ADB selection, exact external
  evidence, EN/RU parity, durable legacy headings, manifest ownership, CI, and
  maintenance triggers.
- The Russian page preserves all twelve fenced blocks byte-for-byte and pins
  normalized English hash
  `f5ce593e0fb4aa0dac8a4e7020361fa4299cd53007dc18f04545f6f47c375ce5`.
  Localization reports 161/195 current translations (82.56 percent), with 34
  missing.
- Site generation contains 111 navigation items, 350 public routes, 188
  planned redirects, and 195 English plus 161 Russian searchable documents.
  Search indexes occupy 1,143,969 English bytes and 1,284,217 Russian bytes.
- AI delivery contains 350 public documents and 1,771,910 full-context bytes;
  deterministic evaluation remains 22 tasks and 50 retrieval checks at 100
  percent success and 0.897 MRR. Snippets pass 285/285 normative, 155 evidence,
  and 162 external-parser checks.

### Standalone, native, and rendered validation

- Android Debugging passes 8/8 focused tests. Package security, package docs,
  support matrix, localization, external evidence, and snippet suites pass
  6/6, 6/6, 6/6, 7/7, 7/7, and 5/5. Complete documentation discovery passes
  438/438 tests; gameplay runner, minimal package, AiControl protocol, package
  security, and AngelScript CMake suites pass 4/4, 4/4, 8/8, 6/6, and 1/1.
  All three structural CMake interface validators pass, and standalone
  validation accepts 357 Markdown entries.
- `LF_UnitTests` builds cleanly in `RelWithDebInfo` and the complete binary
  exits successfully after exercising its native, baker, client, server, and
  script fixtures.
- The production-mode local Jekyll artifact passes 511 rendered routes, 43
  static endpoints, and 68,675 local references with `github-pages` 232,
  Bundler 2.5.11, and local Ruby 3.3.12.
- Chromium passes all 1,022 desktop/mobile page checks, eleven interaction
  profiles, and thirteen screenshots with zero errors or axe violations. The
  computed-color fallback passes all 51 desktop and 9,724 mobile incomplete
  contrast nodes with no failed or unresolved result.

### Residual and revision state

- Remaining locale work is 34 required pages. The next platform procedure is
  Web Debugging; native Debugging also remains in the platform family. The
  symbol-ID API-description catalog, complete-parity enforcement, landed
  CI/Pages evidence, independent model-family evaluation, and manual production
  keyboard, 200 percent zoom, and representative screen-reader review remain.
  Public-example publication retains the owner-gated work recorded by Group 35.
- The opening and closing fetches found no incoming range: Last Frontier
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` remains 7 ahead / 0 behind
  `origin/main` `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`; Engine
  `fac978a67d1e601eb77389e8dc562d7e511705a0` remains 14 ahead / 0 behind
  `origin/master` `fee50fb636b5bd1e30509aded929df1fc0e95db5`; and TLA
  `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` exactly matches
  `origin/master`. Repository-local work remains unstaged and no commit or push
  is part of this documentation group.

## 2026-08-02 - Thirty-seventh physical locale group: Sprite Root Motion and Walk Cycles

### Source, runtime-contract, and project-evidence audit

- Migrated the Engine-owned 2D root-motion procedure to
  `Docs/en/how-to/content/sprite-root-motion.md`, added a complete independent
  Russian counterpart, and retained `Docs/SpriteRootMotion.md` as a durable
  pointer preserving all 19 H2 routes.
- Re-audited image import/baking, SpriteResource v2 transport, direction sheets,
  `MovingContext`, geometry re-partition, `CritterHexView`, `ResourceManager`,
  and focused image/geometry tests. The guide now distinguishes intentionally
  unclamped live multi-hex `HexOffset` values from map-aware normalization at
  movement stop and pins the invariant that a successful re-split preserves
  rendered world position.
- Corrected the Fallout animation merge path so the base cycle total is
  subtracted from the first appended frame independently on `x` and `y`; the
  previous second subtraction accidentally targeted `x`. The full project
  `LF_UnitTests` binary rebuilds and passes after recompiling
  `ResourceManager.cpp`.
- Expanded the model/animation/root-motion evidence record with exact Last
  Frontier character-generator and documentation-maintenance sources. The
  inventory remains 29 records and now verifies 147 exact source references.
  Last Frontier's 3D `*_RM` rule remains only an ownership boundary; TLA's
  `raw_ptr` and `LoadAnimation` names are historical migration evidence. No
  current authored 2D root-motion asset was observed in either pinned project,
  so no concrete offset, speed, or gait-quality claim was promoted.

### Canonical routes, localization, and generated delivery

- The canonical guide adds contract status, stop-time normalization, a visual
  acceptance matrix, project extraction rules, and maintenance triggers. Eight
  focused tests pin source markers, per-axis merge behavior, movement and
  geometry lifecycle, external evidence, EN/RU fenced-code identity, manifest
  ownership, and every durable legacy heading.
- The Russian page preserves all eleven fenced blocks byte-for-byte and pins
  normalized English hash
  `a1b169c4348ff21d943000d405c7d4106cc28f6e1b79e18fb5ca9282b9e726da`.
  Localization reports 160/195 current translations (82.05 percent), with 35
  missing.
- Site generation contains 111 navigation items, 349 public routes, 188
  planned redirects, and 195 English plus 160 Russian searchable documents.
  Search indexes occupy 1,137,177 English bytes and 1,266,800 Russian bytes,
  below their independent 1.25 MiB limits.
- AI delivery contains 349 public documents and 1,750,622 full-context bytes;
  deterministic evaluation remains 22 tasks and 50 retrieval checks at 100
  percent success and 0.897 MRR. Snippets pass 280/280 normative, 154 evidence,
  and 159 external-parser checks.

### Standalone, native, and rendered validation

- Sprite Root Motion passes 8/8 focused tests, Image Format passes 9/9, and
  Model Format passes 10/10. Complete documentation discovery passes 429/429
  tests; gameplay runner, minimal package, AiControl protocol, package-security,
  and AngelScript CMake suites pass 4/4, 4/4, 8/8, 6/6, and 1/1. All three
  structural CMake interface validators pass, and standalone validation accepts
  356 Markdown entries.
- `LF_UnitTests` builds cleanly in `RelWithDebInfo` and the complete binary
  exits successfully after exercising its native, baker, client, server, and
  script fixtures.
- The production-mode local Jekyll artifact passes 509 rendered routes, 43
  static endpoints, and 68,386 local references with `github-pages` 232,
  Bundler 2.5.11, and local Ruby 3.3.12.
- Chromium passes all 1,018 desktop/mobile page checks, eleven interaction
  profiles, and thirteen screenshots with zero errors or axe violations. The
  computed-color fallback passes all 51 desktop and 9,691 mobile incomplete
  contrast nodes with no failed or unresolved result.

### Residual and revision state

- Remaining locale work is 35 required pages, followed by the symbol-ID API
  description catalog, complete-parity enforcement, landed CI/Pages evidence,
  independent model-family evaluation, and manual production keyboard, 200
  percent zoom, and representative screen-reader review. Public-example
  publication retains the owner-gated work recorded by Group 35.
- The initial and final fetches found no incoming range: Last Frontier
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` remains 7 ahead / 0 behind
  `origin/main` `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`; Engine
  `fac978a67d1e601eb77389e8dc562d7e511705a0` remains 14 ahead / 0 behind
  `origin/master` `fee50fb636b5bd1e30509aded929df1fc0e95db5`; and TLA
  `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` exactly matches
  `origin/master`. Repository-local work remains unstaged and no commit or push
  is part of this documentation group.

## 2026-08-02 - Thirty-sixth physical locale group: Model Animation Metadata and Duration

### Source, runtime-contract, and project-evidence audit

- Migrated the Engine-owned model-animation procedure to
  `Docs/en/how-to/content/model-animation.md`, added a complete independent
  Russian counterpart, and retained `Docs/ModelAnimation.md` as a durable
  pointer preserving all 14 H2/H3 routes.
- Re-audited `ModelSourceLoader`, `ModelAnimationConverter`,
  `ModelAnimationData`, `ModelInfoBaker`, `AnimationInfo`, the client model
  information/animation/instance owners, both script methods, and the focused
  baker/source/converter/runtime/Ozz/common-method tests. The guide now pins
  finite reciprocal validation, the signed 32-bit millisecond output range,
  rejection of positive sub-millisecond cycles that round to zero, and
  first-entry/one-step-alias behavior directly to source.
- Split acceptance into authored tuple, baked metadata, visible client pose,
  and project gameplay-timing layers. A successful bake no longer implies a
  visual or authoritative gameplay claim; explicit zero-duration fallback and
  semantic timing tests remain project-owned.
- Expanded the existing model/animation/root-motion evidence record from broad
  directories to ten exact Last Frontier/TLA files. Last Frontier's named enum
  tuples, include template, tuple `AnimSpeed`, current aliases, and exact
  geometry exceptions are useful current evidence. TLA's bare `AnimEqual` and
  model-level `Speed` are recorded as rejected migration evidence, not current
  alias or tuple-speed grammar. The complete inventory remains 29 records and
  now verifies 145 exact source references.

### Canonical routes, localization, and generated delivery

- The canonical guide adds contract status, project extraction rules,
  maintenance triggers, a timing acceptance matrix, and complete failure
  boundaries. Seven focused tests pin source markers, millisecond limits,
  runtime lookups, external evidence, EN/RU fenced-code identity, manifest
  ownership, and every durable legacy heading.
- The Russian page preserves all five fenced blocks byte-for-byte and pins
  normalized English hash
  `87210fa5e304b04865797e03928507322f11ac2dd143fea378d35c4e81a41886`.
  Localization reports 159/195 current translations (81.54 percent), with 36
  missing.
- Site generation contains 111 navigation items, 348 public routes, 188
  planned redirects, and 195 English plus 159 Russian searchable documents.
  Search indexes occupy 1,134,864 English bytes and 1,251,252 Russian bytes,
  below their independent 1.25 MiB limits.
- AI delivery contains 348 public documents and 1,743,569 full-context bytes;
  deterministic evaluation remains 22 tasks and 50 retrieval checks at 100
  percent success and 0.897 MRR. Snippets pass 280/280 normative, 154 evidence,
  and 159 external-parser checks.

### Standalone and rendered validation

- Model Animation passes 7/7 focused tests and Model Format passes 10/10.
  Complete documentation discovery passes 426/426 tests; gameplay runner,
  minimal package, AiControl protocol, package-security, and AngelScript CMake
  suites pass 4/4, 4/4, 8/8, 6/6, and 1/1. All three structural CMake interface
  validators pass, `git diff --check` reports no whitespace errors, and
  standalone validation accepts 355 Markdown entries.
- The production-mode local Jekyll artifact passes 507 rendered routes, 43
  static endpoints, and 68,114 local references with `github-pages` 232,
  Bundler 2.5.11, and local Ruby 3.3.12.
- Chromium passes all 1,014 desktop/mobile page checks, eleven interaction
  profiles, and thirteen screenshots with zero errors or axe violations. All
  51 desktop and 9,682 mobile incomplete contrast nodes pass the
  computed-color fallback with no failed or unresolved results.

### Residual and revision state

- Remaining locale work is 36 required pages, followed by the symbol-ID API
  description catalog, complete-parity enforcement, landed CI/Pages evidence,
  independent model-family evaluation, and manual production keyboard, 200
  percent zoom, and representative screen-reader review. Public-example
  publication retains the owner-gated work recorded by Group 35.
- The initial and final fetches found no incoming range: Last Frontier
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` remains 7 ahead / 0 behind
  `origin/main` `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`; Engine
  `fac978a67d1e601eb77389e8dc562d7e511705a0` remains 14 ahead / 0 behind
  `origin/master` `fee50fb636b5bd1e30509aded929df1fc0e95db5`; and TLA
  `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` exactly matches
  `origin/master`.

## 2026-08-01 - Thirty-fifth physical locale group: Public Example Repositories

### Source, project-evidence, and remote audit

- Migrated the Engine-owned Public Example Repositories procedure to
  `Docs/en/how-to/build/public-example-repositories.md`, added a complete
  independent Russian counterpart, and retained
  `Docs/PublicExampleRepositories.md` as a durable pointer preserving all 17
  H2/H3 routes.
- Re-audited `Examples/PublicRepositories.json`, the governance overlay,
  Minimal Project, Minimal Multiplayer, GameplayTestHarness, AiControl sample,
  `docs_examples.py`, focused tests, ADR-0005, package/updater boundaries, the
  external Last Frontier/TLA evidence record, and the current validation
  workflow. Production projects remain evidence inputs; practices are promoted
  only through Engine-owned fixtures, validators, exact pins, and compatibility
  lanes.
- An authenticated GitHub audit confirmed all four `cvet` repositories are
  private on `main`. Observed heads are project-template
  `9946ca42c332a294f8fedd2732e7850a01c1ec27`, minimal-multiplayer
  `97d232431488125b370be352fdcf28f66e6cbf4f`, content-showcase
  `011dab0d07eef6387609821206b8ee534ec51c3f`, and native-extension-sample
  `97823816ab333a62aced43edd4daafa19c5fee22`. Only the template contains
  candidate source; the other three contain reservation READMEs.
- The staged template pins reachable Engine commit
  `9d74c751f5684f80aef3b35a0eb16a8fabf9fa42`, but that revision predates
  `BuildTools/docs_examples.py`; its workflows contain a fallback skip and no
  retained workflow run or commit status was observed for the current head.
  This is explicitly classified as private staging, not publication evidence.

### Registry, guide, and localization implementation

- Extended every machine-registry remote record with `verified_on`, exact
  default branch and head commit, observed Engine revision where staged, and
  `required_checks_state`. The generator rejects malformed observations,
  branch drift, a staged/published record without an exact Engine revision,
  and any published record whose visibility/state/checks are not
  public/published/passing.
- Added a seven-part publication decision, explicit administrator-owned
  security/branch/release observations, clean-clone and no-fallback CI gates,
  exact artifact/tag identity, byte-level provenance, support boundaries, and
  extraction rules for Last Frontier/TLA practices. Phase 7 now separately
  tracks restaging/publishing the project template before the three later
  repositories.
- The complete Russian guide preserves all four fenced command blocks
  byte-for-byte and pins normalized English hash
  `e73f28650a4a1aebbe655df162a70ca5da710fcc4fb22242e12538289200aa18`.
  Localization reports 158/195 current translations (81.03 percent), with 37
  missing.

### Generated and validation evidence

- Site generation contains 111 navigation items, 347 public routes, 188
  planned redirects, and 195 English plus 158 Russian searchable documents.
  Search indexes occupy 1,133,368 English bytes and 1,240,369 Russian bytes,
  below their independent 1.25 MiB limits.
- AI delivery contains 347 public documents and 1,738,771 full-context bytes;
  deterministic evaluation remains 22 tasks and 50 retrieval checks at 100
  percent success and 0.897 MRR. Snippets pass 280/280 normative, 154
  evidence, and 159 external-parser checks.
- Nine focused public-example tests pass. Complete documentation discovery
  passes 424/424 tests; the AngelScript/CMake and documentation-CMake focused
  suites pass 1/1 and 5/5, all three structural CMake interface validators
  pass, and standalone validation accepts 354 Markdown entries.

### Rendered public-example proof

- The production-mode local Jekyll artifact passes 505 rendered routes, 43
  static endpoints, and 67,851 local references with `github-pages` 232,
  Bundler 2.5.11, and local Ruby 3.3.12.
- Chromium passes all 1,010 desktop/mobile page checks, eleven interaction
  profiles, and thirteen screenshots with zero errors or axe violations. All
  51 desktop and 9,690 mobile incomplete contrast nodes pass the computed-color
  fallback with no failed or unresolved results; the new Russian route is among
  the directly observed desktop paths.

### Residual and revision state

- Remaining locale work is 37 required pages, followed by the symbol-ID API
  description catalog, complete-parity enforcement, landed CI/Pages evidence,
  and manual production keyboard, 200 percent zoom, and representative
  screen-reader review.
- Example publication still requires restaging the template at an exact Engine
  revision containing the validator, fresh pinned/current Windows/Linux
  evidence without fallback skips, branch/security configuration, immutable
  tag/artifact evidence, remote staging of Minimal Multiplayer, source for the
  showcase and native-extension sample, and owner-authorized visibility changes.
- The initial and final fetches found no incoming range: Last Frontier
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` remains 7 ahead / 0 behind
  `origin/main` `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`; Engine
  `fac978a67d1e601eb77389e8dc562d7e511705a0` remains 14 ahead / 0 behind
  `origin/master` `fee50fb636b5bd1e30509aded929df1fc0e95db5`; and TLA
  `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` exactly matches
  `origin/master`.

## 2026-08-01 - Thirty-fourth physical locale group: Client Updater

### Contract and project evidence

- Migrated the source-backed Client Runtime Split and Updater explanation to
  `Docs/en/explanation/runtime/client-updater.md`, added a complete independent
  Russian counterpart, and retained `Docs/ClientUpdater.md` as a durable
  pointer preserving all 21 H2/H3 routes.
- Re-audited `ClientApp.cpp`, `ClientLib.cpp`, `ClientRuntimeApi.*`,
  `Updater.*`, `UpdaterBackend.*`, player/connection dispatch, settings,
  application/package CMake, `package.py`, platform/filesystem helpers, native
  tests, and current Last Frontier updater pipeline tests. The audit pins
  client runtime ABI 3, updater generation 2, promote-and-exit, installed
  selector validation, stateless offset transfer, package variants, PDB
  delivery, and platform capability boundaries.
- Corrected two material source drifts: the live backend interface takes
  `ptr<Player>` and returns `const_span<uint8_t>`, while the old page still
  showed `ServerConnection*` and `const vector<uint8_t>&`; disk hash-cache
  entries are keyed as `<basename>-<16-hex path digest>.hash`, not the
  collision-prone `<basename>.hash` previously documented.
- Expanded the external-project evidence to 138 exact paths across 29
  concerns, including 24 updater/signing paths. Last Frontier supplies
  executable portable/installed, corruption, staging, postfix, PDB, missing
  payload, and restart lanes; TLA supplies compatibility/configuration
  evidence but no equivalent updater pipeline, so settings alone are not
  treated as operational proof.

### Implementation and localization

- Added explicit contract status, source ownership, exact ABI/wire/backend
  signatures, cache-key mechanics, environment-specific transfer/memory
  guidance, portable/installed and variant acceptance lanes, and the security
  boundary for signing the final patched executable bytes actually served by
  the packaged server.
- Updated live Engine and Last Frontier routing to canonical locale owners.
  The complete Russian mirror preserves all five fenced blocks byte-for-byte
  and pins normalized English hash
  `8d166219f924cc1e22a5a830e60c71e782357ddc155a2cd42f1bb42bb41ff61e`.
- Added seven focused tests for ABI/protocol generations, host/selector
  behavior, stateless backend transfer, resume/cache/package mechanics,
  project evidence, locale completeness, and canonical/legacy route
  ownership. Localization reports 157/195 current translations (80.51
  percent), with 38 missing.

### Generated and validation evidence

- Site generation contains 111 navigation items, 346 public routes, 188
  planned redirects, and 195 English plus 157 Russian searchable documents.
  Search indexes occupy 1,131,223 English bytes and 1,223,728 Russian bytes,
  below the independent 1.25 MiB limits.
- AI delivery contains 346 public documents and 1,731,737 full-context bytes;
  deterministic evaluation remains 22 tasks and 50 retrieval checks at 100
  percent success and 0.897 MRR. Snippets pass 280/280 normative, 154
  evidence, and 159 external-parser checks.
- The seven focused Client Updater tests and the 66-test affected set pass.
  Complete documentation discovery passes 449/449 tests, all three reusable
  CMake interface validators pass, and standalone validation accepts 353
  Markdown entries.

### Rendered Client Updater proof

- The production-mode local Jekyll artifact passes 503 rendered routes, 43
  static endpoints, and 67,575 local references with `github-pages` 232,
  Bundler 2.5.11, and local Ruby 3.3.12. The rendered gate caught and drove
  two publication fixes: a directory permalink inconsistent with sibling
  runtime pages, and a split inline-code span whose `<version>` token became
  an unclosed HTML element and hid the remainder of the page.
- Chromium passes all 1,006 desktop/mobile page checks, eleven interaction
  profiles, and thirteen screenshots with zero errors or axe violations. All
  incomplete contrast nodes pass the computed-color fallback with no failed
  or unresolved results.

### Residual and revision state

- Remaining locale work is 38 required pages, followed by the symbol-ID API
  description catalog, complete-parity enforcement, landed CI/Pages evidence,
  and manual production keyboard, 200 percent zoom, and representative
  screen-reader review.
- The initial and final fetches found no incoming range: Last Frontier
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` remains 7 ahead / 0 behind
  `origin/main` `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`; Engine
  `fac978a67d1e601eb77389e8dc562d7e511705a0` remains 14 ahead / 0 behind
  `origin/master` `fee50fb636b5bd1e30509aded929df1fc0e95db5`; and TLA
  `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` exactly matches
  `origin/master`.

## 2026-08-01 - Thirty-third physical locale group: Frontend and Rendering

### Contract and project evidence

- Migrated the source-backed Frontend and Rendering explanation to canonical
  `Docs/en/explanation/rendering/index.md`, added a complete independent
  Russian counterpart, and retained `Docs/FrontendAndRendering.md` as a durable
  pointer preserving all 37 H2/H3 routes.
- Re-audited `Source/Frontend`, `Source/Client`, `Source/Tools`,
  `Source/Common/Settings.inc`, `BuildTools/cmake/stages/Init.cmake`, and focused
  rendering tests. The audit adds the previously undocumented
  `AppRender`/`IAppRender` service and pins the exact Null, OpenGL, Direct3D,
  Vulkan, and SDL_GPU backend inventory plus compile-time selection order.
- Corrected three material contract drifts: direct Metal is a declared
  placeholder that throws when forced, not an implemented renderer; Vulkan
  validation needs a runtime and optional validation layer but no build-time
  SDK; and SDL_GPU belongs in the focused backend acceptance matrix.
- Pinned Last Frontier's current render configuration and effect overrides plus
  TLA's historical render configuration as external evidence. The resulting
  practices keep force selectors disabled in normal profiles, preserve effect
  slot contracts, treat `ModelProjFactor` as project-wide tuning, and reject
  copying stale project settings into a newer Engine. The external inventory
  now covers 119 exact paths across 29 concerns.

### Implementation and localization

- Added contract status, exact build flags and runtime selectors, automatic
  selection failure semantics, direct-Metal status, SDL_GPU/Vulkan validation,
  and embedding-project practices. Engine and Last Frontier routing now points
  to the canonical locale owners, including affected source comments.
- The Russian mirror independently preserves application services, backend
  internals, atlas/model geometry, render targets and resolution, effects,
  scene composition, SPARK/Effekseer/model/ribbon/track behavior, project
  boundaries, and focused validation. Its source metadata pins normalized hash
  `520e4d0c0f2928da470e8358e38aabcf5bd4bea7240ff189f4176a28fa7acdb6`.
- Added seven focused Frontend and Rendering tests for application services,
  backend inventory, build/runtime switches, validation, external evidence,
  locale completeness, and canonical/legacy routes. Localization reports
  156/195 current translations (80.00 percent), with 39 missing.

### Generated and validation evidence

- Site generation contains 111 navigation items, 345 public routes, 188
  planned redirects, and 195 English plus 156 Russian searchable documents.
  Search indexes occupy 1,129,929 English bytes and 1,200,859 Russian bytes,
  below the independent 1.25 MiB limits.
- AI delivery contains 345 public documents and 1,726,286 full-context bytes;
  deterministic evaluation remains 22 tasks and 50 retrieval checks at 100
  percent success and 0.897 MRR. Snippets pass 280/280 normative, 154 evidence,
  and 159 external-parser checks.
- The seven focused Frontend and Rendering tests pass; the affected frontend,
  effect, image, model, localization, profiling, and testing set passes 52/52.
  Complete documentation discovery passes 416/416 tests, all three reusable
  CMake interface validators pass, and standalone validation accepts 352
  Markdown entries.

### Rendered Frontend and Rendering proof

- The production-mode local Jekyll artifact passes 501 rendered routes, 43
  static endpoints, and 67,291 local references with `github-pages` 232,
  Bundler 2.5.11, and local Ruby 3.3.12. The rendered gate caught and drove the
  correction of one English fragment used against the translated Russian
  Particle Format page.
- Chromium passes all 1,002 desktop/mobile page checks, eleven interaction
  profiles, and thirteen screenshots with zero errors or axe violations. All
  50 desktop and 9,667 mobile incomplete contrast nodes pass the computed-color
  fallback with zero failed or unresolved results.

### Residual and revision state

- Remaining locale work is 39 required pages, followed by the symbol-ID API
  description catalog, complete-parity enforcement, landed CI/Pages evidence,
  and manual production keyboard, 200 percent zoom, and representative
  screen-reader review.
- The initial and final fetches found no incoming range: Last Frontier
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` remains 7 ahead / 0 behind
  `origin/main` `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`; Engine
  `fac978a67d1e601eb77389e8dc562d7e511705a0` remains 14 ahead / 0 behind
  `origin/master` `fee50fb636b5bd1e30509aded929df1fc0e95db5`; and TLA
  `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` exactly matches
  `origin/master`.

## 2026-08-01 - Thirty-second physical locale group: Baking Pipeline

### Contract and project evidence

- Migrated the source-backed Baking Pipeline explanation to canonical
  `Docs/en/explanation/content-pipeline/baking.md`, added a complete independent
  Russian counterpart, and retained `Docs/BakingPipeline.md` as a durable
  pointer preserving all 20 former H2/H3 routes.
- Re-audited `Settings.h/.cpp`, `ScriptsAndBaking.cmake`, the build-hash helper,
  baker application/library entry points, all 13 built-in baker headers and
  registrations, report ownership, image/model/particle payloads, and focused
  native tests. The audit removed duplicated `BakerDataSource` prose and
  replaced an incomplete baker list with exact names, orders, and feature
  guards.
- Corrected a real cross-document drift: `ModelInfoBaker` returns order `6`,
  while the Model Format guide/model/reference still claimed `5`. English and
  Russian guides, `ModelFormatInterface.json`, generated JSON/Markdown, contract
  digest, and a new exact-order test now agree with the live header.
- Pinned `LastFrontier.fomain` and `TLA.fomain` as additional external evidence.
  Both projects independently separate metadata, scripts, text, prototypes,
  maps, art, and raw-copy resources into ordered packs. The Engine guide derives
  reusable composition practices while leaving names, paths, custom bakers,
  side policy, and release acceptance project-owned. The inventory now covers
  116 exact paths across 29 concerns.

### Implementation and localization

- Added contract status, source-owned resource-pack fields, project composition
  practices, exact `ForceCodeGeneration` dependencies, a complete 13-row baker
  table, and locale-aware links throughout Engine and Last Frontier entrypoints.
  All affected Russian source hashes and the generated Model Format reference
  digest were reconciled.
- Added five focused Baking Pipeline tests for live names/orders, CMake
  invocation dependencies, resource-pack fields, pinned project evidence, and
  canonical/legacy route ownership. The adjacent Model Format suite now pins
  the live order explicitly and contains ten tests.
- The Russian mirror preserves the report schema, resource-pack rules,
  sprite-mesh diagnostics and geometry policy, SPARK/Effekseer behavior,
  model/Ozz wire contracts, test routing, and validation checklist. Localization
  reports 155/195 current translations (79.49 percent), with 40 missing.

### Generated and validation evidence

- Site generation contains 111 navigation items, 344 public routes, 188
  planned redirects, and 195 English plus 155 Russian searchable documents.
  Search indexes occupy 1,128,749 English bytes and 1,166,687 Russian bytes,
  below the independent 1.25 MiB limits.
- AI delivery contains 344 public documents and 1,718,975 full-context bytes;
  deterministic evaluation remains 22 tasks and 50 retrieval checks at 100
  percent success and 0.900 MRR. Snippets pass 280/280 normative, 154 evidence,
  and 159 external-parser checks.
- Focused Baking Pipeline, Model Format, and localization suites pass 5/5,
  10/10, and 7/7. Complete documentation discovery passes 409/409 tests, all
  three reusable CMake interface validators pass, and standalone validation
  accepts 351 Markdown entries.
- Production Jekyll passes 499 rendered routes, 43 static endpoints, and
  66,984 local references with `github-pages` 232, Bundler 2.5.11, and local
  Ruby 3.3.12. Chromium passes all 998 desktop/mobile page checks, eleven
  interaction profiles, and thirteen screenshots with zero errors.
- Axe reports no violations. All 50 desktop and 9,667 mobile incomplete
  contrast nodes pass the computed-color fallback with zero failed or
  unresolved results.

### Residual and revision state

- Remaining locale work is 40 required pages, followed by the symbol-ID API
  description catalog, complete-parity enforcement, landed CI/Pages evidence,
  and manual production keyboard, 200 percent zoom, and representative
  screen-reader review.
- The initial and pre-render fetches found no incoming range: Last Frontier
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` remains 7 ahead / 0 behind
  `origin/main` `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`; Engine
  `fac978a67d1e601eb77389e8dc562d7e511705a0` remains 14 ahead / 0 behind
  `origin/master` `fee50fb636b5bd1e30509aded929df1fc0e95db5`; and TLA
  `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` exactly matches
  `origin/master`.

## 2026-08-01 - Thirty-first physical locale group: Prototype Format

### Contract and project evidence

- Migrated the authored Prototype Format guide and all four generated
  references to canonical `Docs/en/` routes and added complete `Docs/ru/`
  mirrors. Five durable legacy pointers preserve all 14 former guide H2 routes,
  every generated H2/H3 route, and all 135 generated stable entry anchors.
- Re-audited `ConfigFile`, `ProtoBaker`, property loading/serialization,
  metadata, migrations, callback validation, and native tests. The generated
  model derives four built-in `HasProtos` entity types and 113 properties, of
  which 92 are authorable, plus three section forms, two control directives,
  and 13 source-backed rules. It now records the live `/` and `$` ID rejection
  and the exact whitespace-before-backslash continuation rule.
- Last Frontier configures six extensions, 43 project `FixedType`
  declarations, custom `Modifier`/`Faction` prototype entities, and project
  semantic audits; its project guide now correctly states that sections and
  metadata select type while extensions provide discovery and organization.
  TLA configures five extensions and a large legacy corpus without `.foinfo`;
  its bare map placements and project layout remain migration/advisory evidence,
  not current reusable grammar.

### Implementation and localization

- Updated prototype-format generation to emit four canonical English pages and
  four legacy pointers in one deterministic pass. Updated public-contract and
  external-evidence routing, manifest ownership, Engine and Last Frontier
  links, BuildTools entrypoints, project ownership prose, locale fixtures, and
  all affected translation hashes. Eight focused tests cover the source model,
  guide boundaries, generated pages, legacy headings/anchors, and
  canonical/legacy manifest ownership.
- Added complete Russian guide/reference translations while preserving all
  fenced examples, 135 stable entry anchors, 113 property rows, source links,
  types, sides, and flags. Localization reports 154/195 current translations
  (78.97 percent), with 41 missing.

### Generated, rendered, and browser evidence

- Site generation contains 111 navigation items, 343 public routes, 188
  planned redirects, 195 English and 154 Russian searchable documents, and 154
  complete locale pairs. Search indexes occupy 1,128,319 English bytes and
  1,145,089 Russian bytes under their independent 1.25 MiB limits.
- AI delivery contains 343 public documents and 1,715,724 full-context bytes;
  evaluation passes 22 tasks and 50 retrieval checks at 100 percent success and
  0.900 MRR. Snippets pass 280/280 normative, 154 evidence, and 159
  external-parser checks.
- Complete Engine documentation discovery passes 403/403 tests, all three
  reusable CMake interface validators pass, and standalone validation accepts
  350 Markdown entries. Production Jekyll passes 497 rendered routes, 43
  static endpoints, and 66,679 local references.
- Chromium covers 994 rendered page checks, eleven interaction profiles, and
  thirteen screenshots with zero errors. Axe reports no violations; all 50
  desktop and 9,663 mobile incomplete contrast nodes pass the computed-color
  fallback with zero failed or unresolved nodes.

### Residual and revision state

- Remaining locale work is 41 required pages, followed by the symbol-ID API
  description catalog, complete-parity enforcement, landed CI/Pages evidence,
  and manual production keyboard, 200 percent zoom, and representative
  screen-reader review.
- The final fetch found no incoming range: Last Frontier
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` remains 7 ahead / 0 behind
  `origin/main` `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`; Engine
  `fac978a67d1e601eb77389e8dc562d7e511705a0` remains 14 ahead / 0 behind
  `origin/master` `fee50fb636b5bd1e30509aded929df1fc0e95db5`; and TLA
  `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` exactly matches
  `origin/master`.

## 2026-08-01 - Thirtieth physical locale group: Model Format

### Contract and project evidence

- Migrated the authored Model Format guide plus all seven generated references
  to canonical `Docs/en/` routes and added complete `Docs/ru/` mirrors. The
  eight former flat/generated routes remain durable pointers preserving all 32
  prior guide H2/H3 routes and all 63 generated stable entry anchors.
- Re-audited the guide and structured model against live `ModelMeshBaker`,
  `ModelSourceLoader`, `ModelAnimationConverter`, `ModelInfoBaker`, client model
  composition, compile-time limits, and native tests. The generated contract
  remains 32 token groups, 59 accepted parser spellings, six asset kinds, and
  13 rules. Particle attachments now correctly name baked `.spk` / `.efk`
  resources instead of the retired `.fopts` assumption, and generated
  animation links resolve correctly from the locale reference directory.
- Last Frontier contains 70 `.fo3d` files using 21 distinct current directives
  with no unknown token or retired mesh extension. It retains concrete model
  catalogs, layer semantics, enums, art policy, and visible acceptance. TLA's
  59 files contain five removed directives (`AnimEqual`,
  `CalculateTangentSpace`, `DrawSize`, `RenderFrame`, and `RenderFrames`) plus
  `.x` / `.3ds` paths, so they remain historical migration evidence only.

### Implementation and localization

- Updated model-format generation to emit seven canonical English pages and
  seven legacy pointers in one deterministic pass. Updated public-contract
  routing, manifest ownership, external-evidence targets, Engine and Last
  Frontier links, BuildTools entrypoints, maintenance guidance, plan counts,
  and all affected translation hashes. Nine focused tests cover the live
  parser/model, current particle boundary, canonical guide, legacy headings and
  anchors, and canonical/legacy manifest ownership.
- Added complete Russian guide/reference translations while preserving every
  fenced command/example, stable ID, numeric limit, source link, and parser
  spelling. Localization reports 149/195 current translations (76.41 percent),
  with 46 missing.

### Generated, rendered, and browser evidence

- Site generation contains 111 navigation items, 338 public routes, 188
  planned redirects, 195 English and 149 Russian searchable documents, and 149
  complete locale pairs. Search indexes occupy 1,127,719 English bytes and
  1,122,351 Russian bytes under their independent 1.25 MiB limits.
- AI delivery contains 338 public documents and 1,714,561 full-context bytes;
  evaluation passes 22 tasks and 50 retrieval checks at 100 percent success and
  0.900 MRR. Snippets pass 280/280 normative, 154 evidence, and 159
  external-parser checks.
- Complete Engine documentation discovery passes 400/400 tests, all three
  reusable CMake interface validators pass, and standalone validation accepts
  345 Markdown entries. Production Jekyll passes 487 rendered routes, 43
  static endpoints, and 65,239 local references.
- Chromium covers 974 rendered page checks, eleven interaction profiles, and
  thirteen screenshots with zero errors. Axe reports no violations; all 50
  desktop and 9,487 mobile incomplete contrast nodes pass the computed-color
  fallback with zero failed or unresolved nodes.

### Residual and revision state

- Remaining locale work is 46 required pages, followed by the symbol-ID API
  description catalog, complete-parity enforcement, landed CI/Pages evidence,
  and manual production keyboard, 200 percent zoom, and representative
  screen-reader review.
- The final fetch found no incoming range: Last Frontier
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` remains 7 ahead / 0 behind
  `origin/main` `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`; Engine
  `fac978a67d1e601eb77389e8dc562d7e511705a0` remains 14 ahead / 0 behind
  `origin/master` `fee50fb636b5bd1e30509aded929df1fc0e95db5`; and TLA
  `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` exactly matches
  `origin/master`.

## 2026-08-01 - Twenty-ninth physical locale group: Map Format

### Contract and project evidence

- Migrated the authored map guide plus all five generated references to
  canonical `Docs/en/` routes and added complete `Docs/ru/` mirrors. The six
  former flat/generated routes remain durable pointers that preserve all prior
  guide H2 routes, generated H2/H3 routes, and 136 stable entry anchors.
- Corrected the stale human guide against live `MapLoader`, `MapBaker`,
  `MapView`, Mapper splice, runtime materialization, and unit-test evidence.
  Current containers accept one or more `[ProtoMap]` anchors, require nested
  `[$Name/Critter]` / `[$Name/Item]` or explicit-map-ID sections, emit `$Name`
  on Mapper save, flatten `$Parent`, and preserve every non-selected sibling
  map block byte-exact. Bare `[Critter]` / `[Item]` sections are rejected.
- Last Frontier already follows the current addressed-section contract and
  owns composition, catalog, graphical-kit, generator, and gameplay gates.
  TLA's legacy bare sections and large historical placement IDs are useful
  migration evidence only and are not normative for current Engine authoring.
  External source snapshots remain verified at the pinned revisions.

### Implementation and localization

- Updated map generation to emit five canonical English pages and five legacy
  pointers in one deterministic pass. Updated public-contract routing,
  manifest ownership, external-evidence targets, Engine and Last Frontier
  links, BuildTools entrypoints, maintenance guidance, plan counts, and every
  affected translation hash. Eight focused Map Format tests cover the live
  model, corrected guide, legacy headings/anchors, and canonical/legacy
  ownership.
- Fixed the AI full-context `indexes-only` policy to recognize locale-migrated
  generated pages from `generated: true`, not only the old `Docs/generated/`
  path. Canonical generated detail tables remain discoverable through
  `llms.txt`, JSON models, and the public manifest without being duplicated in
  `llms-full.txt`.
- Localization reports 141/195 current translations (72.31 percent), with 54
  missing. The generated Map Format model remains source-backed at 3 sections,
  5 directives, 4 ownership modes, 16 rules, and 108 properties.

### Generated, rendered, and browser evidence

- Site generation contains 111 navigation items, 330 public routes, 188
  planned redirects, 195 English and 141 Russian searchable documents, and 141
  complete locale pairs. Search indexes occupy 1,127,720 English bytes and
  1,079,204 Russian bytes under their independent 1.25 MiB limits.
- AI delivery contains 330 public documents and 1,713,938 full-context bytes;
  evaluation passes 22 tasks and 50 retrieval checks at 100 percent success and
  0.900 MRR. Snippets pass 280/280 normative, 154 evidence, and 159
  external-parser checks.
- Complete Engine documentation discovery passes 398/398 tests, all three
  reusable CMake interface validators pass, and standalone validation accepts
  337 Markdown entries. Production Jekyll passes 471 rendered routes, 43
  static endpoints, and 63,066 local references.
- Chromium covers 942 rendered page checks, eleven interaction profiles, and
  thirteen screenshots with zero errors. Axe reports no violations; all 49
  desktop and 9,220 mobile incomplete contrast nodes pass the computed-color
  fallback with zero failed or unresolved nodes.

### Residual and revision state

- Remaining locale work is 54 required pages, followed by the symbol-ID API
  description catalog, complete-parity enforcement, landed CI/Pages evidence,
  and manual production keyboard, 200 percent zoom, and representative
  screen-reader review.
- The final fetch found no incoming range: Last Frontier
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` remains 7 ahead / 0 behind
  `origin/main` `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`; Engine
  `fac978a67d1e601eb77389e8dc562d7e511705a0` remains 14 ahead / 0 behind
  `origin/master` `fee50fb636b5bd1e30509aded929df1fc0e95db5`; and TLA
  `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` exactly matches
  `origin/master`.

## 2026-08-01 - Twenty-eighth physical locale group: Font Format

### Scope and source audit

- Migrated the authored font guide and all eight generated font-format
  references to canonical English routes under `Docs/en/how-to/content/` and
  `Docs/en/reference/font-format/`. Added complete reviewed Russian mirrors at
  matching `Docs/ru/` routes. The former guide and eight generated routes
  remain durable Markdown pointers preserving every prior H2/H3 route and all
  57 generated stable entry anchors.
- Re-audited FOFNT v2 parsing, binary BMFont v3 block order and signed metrics,
  raw-copy delivery, referenced-image loading, `FontType`/`FontFlag`, bind-time
  scale `(0, 1]`, atlas and border preparation, wrapping/alignment,
  measurement/drawing, inline colors, effects, cache lifetime, updater fallback,
  bundled descriptors, and focused native evidence. No reusable Engine behavior
  correction was required.
- Last Frontier confirms the reusable boundary with three startup slots,
  `Gui.BigFontScale`, Russian glyph coverage, project-owned typography, an
  embedded measurement regression, and visible regular/bordered acceptance.
  TLA manually binds nine semantic slots but has no comparable typed policy or
  focused regression, so it remains advisory evidence.

### Implementation and localization

- Updated the generator to emit eight canonical English references and eight
  generated legacy pointers in one deterministic pass. Updated public contract
  routing, manifest ownership, external-evidence targets, BuildTools and source
  maps, active Engine and Last Frontier links, maintenance guidance, locale
  inventory regressions, and every affected translation hash.
- Added regressions for canonical generation, legacy heading and stable-anchor
  retention, canonical/legacy manifest ownership, and all nine new locale IDs.
  Localization reports 135/195 current translations (69.23 percent), with 60
  missing.

### Generated, rendered, and browser evidence

- Site generation contains 111 navigation items, 324 public routes, 188
  planned redirects, 195 English and 135 Russian searchable documents, and 135
  complete locale pairs. Search indexes occupy 1,127,355 English bytes and
  1,048,869 Russian bytes under their independent 1.25 MiB limits.
- AI delivery contains 324 public documents and 2,054,174 full-context bytes;
  evaluation passes 22 tasks and 50 retrieval checks at 100 percent success and
  0.900 MRR. Snippets pass 280/280 normative, 154 evidence, and 159
  external-parser checks.
- Complete Engine documentation discovery passes 395/395 tests, all three
  reusable CMake interface validators pass, and standalone validation accepts
  331 Markdown entries. Production Jekyll passes 459 rendered routes, 43 static
  endpoints, and 61,372 local references.
- The final Chromium gate covers 918 rendered page checks, eleven interaction
  profiles, and thirteen screenshots with no errors or axe violations. All 40
  desktop and 7,127 mobile axe incomplete contrast nodes pass the computed-color
  fallback with no failed or unresolved nodes.

### Residual and revision state

- Remaining locale work is 60 required pages, followed by the symbol-ID API
  description catalog, complete-parity enforcement, landed CI/Pages evidence,
  and manual production keyboard, 200 percent zoom, and representative
  screen-reader review.
- The final fetch found no incoming range: Last Frontier
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` remains 7 ahead / 0 behind
  `origin/main` `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`; Engine
  `fac978a67d1e601eb77389e8dc562d7e511705a0` remains 14 ahead / 0 behind
  `origin/master` `fee50fb636b5bd1e30509aded929df1fc0e95db5`; and TLA
  `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` exactly matches
  `origin/master`.

## 2026-08-01 - Twenty-seventh physical locale group: Particle Format

### Scope and source audit

- Migrated both authored particle guides and all eight generated particle-format
  references to canonical English routes under `Docs/en/how-to/` and
  `Docs/en/reference/particle-format/`. Added complete reviewed Russian mirrors
  at matching `Docs/ru/` routes. The two former authored routes and eight
  generated routes remain durable Markdown pointers preserving every prior
  H2/H3 route and generated stable entry anchor.
- Re-audited the particle feature switches, source and baked extensions,
  `ParticleBaker`, SPARK registry/editor, fixed Effekseer compiler/runtime
  profile, measured bounds, sprite framing, render routes, client script API,
  model attachments, Mapper tools, and focused native tests. The generated
  contract contains 113 entries across two backends, four formats, 37 SPARK
  graph objects, twelve XML rules, nineteen renderer records, five tooling,
  fourteen runtime, six integration, and seven validation records.
- Last Frontier confirms the reusable boundary and supplies project-owned
  catalog, asset provenance, settings, budgets, integrations, acceptance
  scenes, and visible gates. TLA contains only sparse legacy particle settings
  and remains advisory evidence; no reusable Engine behavior correction was
  required.

### Implementation and localization

- Updated the generator to emit eight canonical English references and eight
  generated legacy pointers in one deterministic pass. Updated the public
  contract index, manifest ownership, screenshot ownership/relative embedding,
  external-evidence targets, active Engine and Last Frontier links, maintenance
  guidance, BuildTools routing, and every affected source-hash pair.
- Added regressions for canonical generation, legacy heading and stable-anchor
  retention, canonical/legacy manifest ownership, nested screenshot owners, and
  the complete locale inventory. Localization reports 126/195 current
  translations (64.62 percent), with 69 missing.

### Generated, rendered, and browser evidence

- Site generation contains 111 navigation items, 315 public routes, 188
  planned redirects, 195 English and 126 Russian searchable documents, and 126
  complete locale pairs. Search indexes occupy 1,127,323 English bytes and
  1,004,844 Russian bytes under their independent 1.25 MiB limits.
- AI delivery contains 315 public documents and 2,017,284 full-context bytes;
  evaluation passes 22 tasks and 50 retrieval checks at 100 percent success
  and 0.900 MRR. Snippets pass 280/280 normative, 154 evidence, and 159
  external-parser checks.
- Complete Engine documentation discovery passes 393/393 tests, all three
  reusable CMake interface validators pass, and standalone validation accepts
  322 Markdown entries. Production Jekyll passes 441 rendered routes, 43 static
  endpoints, and 58,964 local references. The final Chromium gate covers 882
  desktop/mobile page checks, eleven interaction profiles, and thirteen
  screenshots with no errors or reported accessibility violations.

### Residual and revision state

- Remaining locale work is 69 required pages, followed by the symbol-ID API
  description catalog, complete-parity enforcement, landed CI/Pages evidence,
  and manual production keyboard, 200 percent zoom, and representative
  screen-reader review.
- The audited revision state is Last Frontier
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` at 7 ahead / 0 behind
  `origin/main` `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`; Engine
  `fac978a67d1e601eb77389e8dc562d7e511705a0` at 14 ahead / 0 behind
  `origin/master` `fee50fb636b5bd1e30509aded929df1fc0e95db5`; and TLA
  `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` matching `origin/master`.

## 2026-08-01 - Twenty-sixth physical locale group: Image Format

### Scope and source audit

- Migrated the authored Image And Sprite Formats guide and all seven generated
  image-format references to canonical English routes under
  `Docs/en/how-to/content/` and `Docs/en/reference/image-format/`. Added
  complete reviewed Russian mirrors at matching `Docs/ru/` routes. The former
  authored route and seven generated routes remain durable Markdown pointers
  preserving every prior H2/H3 route and generated stable entry anchor.
- Re-audited `ImageBaker`, FOFRM/import parsing, filename selectors, baked
  sprite records, default sprite factories, sheets, atlases, caches, and
  focused native tests. The current generated contract contains 51 entries:
  twelve formats, nine FOFRM fields, three filename options, ten baking rules,
  eight runtime rules, and nine validation rules. The baker accepts twelve
  source extensions; the default runtime factories expose eleven directly.
- Last Frontier confirms the reusable boundary and supplies project practice
  for FOFRM composition, alpha/offset handling, atlas ownership, and visible
  acceptance. TLA contains sparse legacy image usage and remains advisory
  evidence; no reusable Engine behavior correction was required.

### Implementation and localization

- Updated the image-format generator to emit seven canonical English
  references and seven generated legacy pointers in one deterministic pass.
  Updated the public contract index, documentation manifest, external-evidence
  targets, active Engine and Last Frontier links, update guidance, BuildTools
  routing, and source-hash pairs.
- Added regressions for canonical generation, legacy heading and stable-anchor
  retention, canonical/legacy manifest ownership, and the complete locale
  inventory. Localization reports 116/195 current translations (59.49
  percent), with 79 missing.

### Generated, rendered, and browser evidence

- Site generation contains 111 navigation items, 305 public routes, 188
  planned redirects, 195 English and 116 Russian searchable documents, and
  116 complete locale pairs. Search indexes occupy 1,127,947 English bytes and
  946,188 Russian bytes under their independent 1.25 MiB limits.
- AI delivery contains 305 public documents and 1,951,347 full-context bytes;
  evaluation passes 22 tasks and 50 retrieval checks at 100 percent success
  and 0.900 MRR. Snippets pass 280/280 normative, 154 evidence, and 159
  external-parser checks.
- Complete Engine documentation discovery passes 391/391 tests, all three
  reusable CMake interface validators pass, and standalone validation accepts
  312 Markdown entries. Production Jekyll passes 421 rendered routes, 43
  static endpoints, and 56,215 local references. Chromium passes 842
  desktop/mobile page checks, eleven interaction profiles, and thirteen
  screenshots with no errors or reported accessibility violations. All 8,509
  raw contrast-incomplete nodes pass the reviewed luminance fallback, with no
  failed or unresolved results.

### Residual and revision state

- Remaining locale work is 79 required pages, followed by the symbol-ID API
  description catalog, complete-parity enforcement, landed CI/Pages evidence,
  and manual production keyboard, 200 percent zoom, and representative
  screen-reader review.
- The final fetch found no incoming range: Last Frontier
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` remains 7 ahead / 0 behind
  `origin/main` `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`; Engine
  `fac978a67d1e601eb77389e8dc562d7e511705a0` remains 14 ahead / 0 behind
  `origin/master` `fee50fb636b5bd1e30509aded929df1fc0e95db5`; and TLA
  `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` exactly matches
  `origin/master`.

## 2026-08-01 - Twenty-fifth physical locale group: Effect Format

### Scope and source audit

- Migrated the authored Effect Format guide and all seven generated effect
  references to canonical English routes under `Docs/en/how-to/content/` and
  `Docs/en/reference/effect-format/`. Added complete reviewed Russian mirrors
  at matching `Docs/ru/` routes. The former authored route and seven generated
  routes remain durable Markdown pointers preserving every prior H2/H3 route
  and generated stable entry anchor.
- Re-audited the `.fofx` parser, render-state normalization, built-in resource
  binding, baker, runtime effect/variant caches, script-value plumbing,
  validation rules, generator model, and focused tests. The current generated
  contract contains 57 syntax/render-state entries, 12 resources, and eight
  validation rules.
- Last Frontier confirms the reusable boundary and supplies project practice
  for effect ownership, path-based cache identity, and `ScriptValue` slot
  policy. TLA contains only sparse effect usage and remains advisory evidence;
  no reusable Engine behavior correction was required.

### Implementation and localization

- Updated the effect-format generator to emit seven canonical English
  references and seven generated legacy pointers in one deterministic pass.
  Updated the public contract index, documentation manifest, external-evidence
  targets, active Engine and Last Frontier links, update guidance, and
  source-hash pairs.
- Added regressions for canonical generation, legacy heading and stable-anchor
  retention, canonical/legacy manifest ownership, and the complete locale
  inventory. Localization reports 108/195 current translations (55.38
  percent), with 87 missing.

### Generated, rendered, and browser evidence

- Site generation contains 111 navigation items, 297 public routes, 188
  planned redirects, 195 English and 108 Russian searchable documents, and
  108 complete locale pairs. Search indexes occupy 1,127,878 English bytes and
  900,822 Russian bytes under their independent 1.25 MiB limits.
- AI delivery contains 297 public documents and 1,916,356 full-context bytes;
  evaluation passes 22 tasks and 50 retrieval checks at 100 percent success
  and 0.900 MRR. Snippets pass 280/280 normative, 154 evidence, and 159
  external-parser checks.
- Complete Engine documentation discovery passes 389/389 tests, all three
  reusable CMake interface validators pass, and standalone validation accepts
  304 Markdown entries. Production Jekyll passes 405 rendered routes, 43
  static endpoints, and 54,068 local references. Chromium passes 810
  desktop/mobile page checks, eleven interaction profiles, and thirteen
  screenshots with no errors or reported accessibility violations. All 8,303
  raw contrast-incomplete nodes pass the reviewed luminance fallback, with no
  failed or unresolved results.

### Residual and revision state

- Remaining locale work is 87 required pages, followed by the symbol-ID API
  description catalog, complete-parity enforcement, landed CI/Pages evidence,
  and manual production keyboard, 200 percent zoom, and representative
  screen-reader review.
- The final fetch found no incoming range: Last Frontier
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` remains 7 ahead / 0 behind
  `origin/main` `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`; Engine
  `fac978a67d1e601eb77389e8dc562d7e511705a0` remains 14 ahead / 0 behind
  `origin/master` `fee50fb636b5bd1e30509aded929df1fc0e95db5`; and TLA
  `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` exactly matches
  `origin/master`.

## 2026-08-01 - Twenty-fourth physical locale group: Native Extensions and Dependencies

### Scope and source audit

- Migrated the Native Extensions and Project-Local Dependencies guides, the
  ThirdParty Maintenance guide, and all four generated native-extension
  references to canonical English routes under `Docs/en/how-to/`,
  `Docs/en/contributing/`, and `Docs/en/reference/native-extension/`. Added
  complete reviewed Russian mirrors at matching `Docs/ru/` routes. The three
  former authored routes and four generated routes remain durable Markdown
  pointers preserving every prior H2/H3 route and generated stable entry
  anchor.
- Re-audited `BuildTools/NativeExtensionInterface.json`,
  `AddEngineSources`, `AddProjectLibraries`, role/library routing, codegen
  metadata participation, all hook call sites and defaults, the minimal
  project, focused CMake validators, and Engine ThirdParty policy. The current
  contract contains five roles, eight hooks, six binding rules, and seven
  compatibility-hashed hook-presence records.
- Last Frontier and TLA confirm the reusable boundary: Engine owns role,
  hook, binding, allocator, and vendoring contracts, while concrete SDKs,
  service integrations, credentials, package payloads, and game-system
  extensions remain project-owned. No reusable Engine behavior correction was
  required.

### Implementation and localization

- Updated the native-extension generator to emit four canonical English
  references and four generated legacy pointers in one deterministic pass.
  Updated the public contract index, documentation manifest, external-evidence
  targets, active Engine and Last Frontier links, project update guidance, and
  source-hash pairs.
- Added regressions for canonical generation, legacy heading and stable-anchor
  retention, canonical/legacy manifest ownership, project dependency routing,
  and the complete locale inventory.
- Search JSON now writes compact UTF-8 instead of expanding Russian text into
  `\uXXXX` escapes. The schema and ranking are unchanged; a focused regression
  pins readable UTF-8 output. Localization reports 100/195 current
  translations (51.28 percent), with 95 missing.

### Generated, rendered, and browser evidence

- Site generation contains 111 navigation items, 289 public routes, 188
  planned redirects, 195 English and 100 Russian searchable documents, and
  100 complete locale pairs. Search indexes occupy 1,127,806 English bytes and
  856,330 Russian bytes under their independent 1.25 MiB limits.
- AI delivery contains 289 public documents and 1,876,298 full-context bytes;
  evaluation passes 22 tasks and 50 retrieval checks at 100 percent success
  and 0.900 MRR. Snippets pass 280/280 normative, 154 evidence, and 159
  external-parser checks.
- Complete Engine documentation discovery passes 387/387 tests, all three
  reusable CMake interface validators pass, and standalone validation accepts
  296 Markdown entries. Production Jekyll passes 389 rendered routes, 43
  static endpoints, and 51,909 local references. Chromium passes 778
  desktop/mobile page checks, eleven interaction profiles, and thirteen
  screenshots with no errors or reported accessibility violations.

### Residual and revision state

- Remaining locale work is 95 required pages, followed by the symbol-ID API
  description catalog, complete-parity enforcement, landed CI/Pages evidence,
  and manual production keyboard, 200 percent zoom, and representative
  screen-reader review.
- The final fetch found no incoming range: Last Frontier
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` remains 7 ahead / 0 behind
  `origin/main` `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`; Engine
  `fac978a67d1e601eb77389e8dc562d7e511705a0` remains 14 ahead / 0 behind
  `origin/master` `fee50fb636b5bd1e30509aded929df1fc0e95db5`; and TLA
  `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` exactly matches
  `origin/master`.

## 2026-08-01 - Twenty-third physical locale group: AiControl Protocol

### Scope and source audit

- Migrated the authored AiControl Protocol guide plus all six generated
  protocol references to canonical English routes under `Docs/en/how-to/` and
  `Docs/en/reference/ai-control-protocol/`. Added complete reviewed Russian
  mirrors at the matching `Docs/ru/` routes. The former guide and six generated
  routes remain durable Markdown pointers that preserve every prior H2/H3
  route and generated stable entry anchor.
- Re-audited the structured protocol model, standard-library reference client,
  standalone sample server, smoke runner, malformed-peer tests, and both
  maintained project bridges. The current model contains 49 experimental
  entries: seven wire rules, six methods, six errors, eleven common command
  fields, seven security rules, six integration rules, and six validation
  rules. The protocol smoke passes all 12 lifecycle checks and the reference
  client passes eight focused malformed-peer, security, and state tests.
- Last Frontier and TLA independently confirm the transport envelope,
  connection-local authorization, bounded command/event lifecycle, and
  correlated completion event. Their observation schemas, game actions,
  administrator tools, readiness rules, launch orchestration, and `lf_*` /
  `tla_*` MCP namespaces remain project-owned. No reusable Engine behavior
  correction was required.

### Implementation and localization

- Updated the AiControl generator to emit canonical English references and all
  six legacy pointers in one deterministic pass. Updated the public contract
  index, documentation manifest, external-evidence targets, active Engine and
  Last Frontier routing links, generated-inventory isolation, maintenance
  guidance, and source-hash pairs.
- Added regressions for canonical generation, legacy heading and stable-anchor
  retention, canonical/legacy manifest ownership, and locale inventory.
  Localization reports 93/195 current translations (47.69 percent), with 102
  missing.

### Generated, rendered, and browser evidence

- Site generation contains 111 navigation items, 282 public routes, 188
  planned redirects, 195 English and 93 Russian searchable documents, and 93
  complete locale pairs. AI delivery contains 282 public documents and
  1,861,562 full-context bytes; evaluation passes 22 tasks and 50 retrieval
  checks at 100 percent success and 0.900 MRR. Snippets pass 280/280
  normative, 154 evidence, and 159 external-parser checks.
- Complete Engine documentation discovery passes 385/385 tests, all three
  reusable CMake interface validators pass, and standalone validation accepts
  289 Markdown entries. Production Jekyll passes 375 rendered routes, 43
  static endpoints, and 50,042 local references. Chromium passes 750
  desktop/mobile page checks, eleven interaction profiles, and thirteen
  screenshots with no errors or reported accessibility violations.

### Residual and revision state

- Remaining locale work is 102 required pages, followed by the symbol-ID API
  description catalog, complete-parity enforcement, landed CI/Pages evidence,
  and manual production keyboard, 200 percent zoom, and representative
  screen-reader review.
- The final fetch found no incoming range: Last Frontier
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` remains 7 ahead / 0 behind
  `origin/main` `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`; Engine
  `fac978a67d1e601eb77389e8dc562d7e511705a0` remains 14 ahead / 0 behind
  `origin/master` `fee50fb636b5bd1e30509aded929df1fc0e95db5`; and TLA
  `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` exactly matches
  `origin/master`.

## 2026-08-01 - Twenty-second physical locale group: Text and Localization

### Scope and source audit

- Migrated the authored Text and Localization guide plus all six generated
  text-format references to canonical English routes under
  `Docs/en/how-to/content/` and `Docs/en/reference/text-format/`. Added complete
  reviewed Russian mirrors at the matching `Docs/ru/` routes. The former guide
  path and six generated paths remain durable Markdown pointers that preserve
  every prior H2/H3 route and generated stable entry anchor.
- Re-audited `TextPack`, `TextBaker`, `ProtoTextBaker`, language settings,
  client/server script methods, renderer color tags, and focused native tests.
  The current model contains 38 entries: seven syntax, nine language, eight
  prototype-text, seven runtime, two rendering, and five validation rules.
  Last Frontier's explicit `@arg:name@` practice and TLA's older project lexem
  forms remain project evidence rather than Engine parser/runtime promises; no
  reusable Engine behavior correction was required.

### Implementation and localization

- Updated the text-format generator to emit canonical English references and
  all six legacy pointers in one deterministic pass. Updated the public
  contract index, documentation manifest, Engine/project routing links,
  maintenance guidance, generated-inventory isolation, and external evidence.
- Added regressions for canonical generation, legacy heading and stable-anchor
  retention, canonical/legacy manifest ownership, and locale inventory.
  Localization reports 86/195 current translations (44.10 percent), with 109
  missing.

### Generated, rendered, and browser evidence

- Site generation contains 111 navigation items, 275 public routes, 188
  planned redirects, 195 English and 86 Russian searchable documents, and 86
  complete locale pairs. AI delivery contains 275 public documents and
  1,833,574 full-context bytes; evaluation passes 22 tasks and 50 retrieval
  checks at 100 percent success and 0.900 MRR. Snippets pass 280/280
  normative, 154 evidence, and 159 external-parser checks.
- Complete Engine documentation discovery passes 383/383 tests, all three
  reusable CMake interface validators pass, and standalone validation accepts
  282 Markdown entries. Production Jekyll passes 361 rendered routes, 43
  static endpoints, and 48,173 local references. Chromium passes 722
  desktop/mobile page checks, eleven interaction profiles, and thirteen
  screenshots with no errors or reported accessibility violations.

### Residual and revision state

- Remaining locale work is 109 required pages, followed by the symbol-ID API
  description catalog, complete-parity enforcement, landed CI/Pages evidence,
  and manual production keyboard, 200 percent zoom, and representative
  screen-reader review.
- The final fetch found no incoming range: Last Frontier
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` remains 7 ahead / 0 behind
  `origin/main` `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`; Engine
  `fac978a67d1e601eb77389e8dc562d7e511705a0` remains 14 ahead / 0 behind
  `origin/master` `fee50fb636b5bd1e30509aded929df1fc0e95db5`; and TLA
  `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` exactly matches
  `origin/master`.

## 2026-08-01 - Twenty-first physical locale group: GUI Runtime

### Scope and source audit

- Migrated the authored GUI Runtime guide and all seven generated GUI
  references to canonical English routes under `Docs/en/how-to/runtime/` and
  `Docs/en/reference/gui-runtime/`. Added complete reviewed Russian mirrors at
  the matching `Docs/ru/` routes. The former guide path and seven former
  generated paths remain durable Markdown pointers that preserve every prior
  H2/H3 route and generated stable entry anchor.
- Re-audited CoreScripts GUI and input behavior, native client event dispatch,
  the structured interface model and its focused tests, and Last Frontier's
  project-owned GUI generator and integration boundary. The current model
  contains 82 entries: 12 types, 159 members, 39 callbacks, 31 screen API
  overloads, and eight metadata annotations. Declarative `.fogui` ownership
  remains project-local; no reusable Engine behavior correction was required.

### Implementation and localization

- Updated the GUI generator to emit canonical English references and all
  legacy pointers in one deterministic pass. Updated the public contract
  index, documentation manifest, project/Engine routing links, maintenance
  guidance, generated-inventory isolation, and Last Frontier evidence routes.
- Added regressions for canonical generation, legacy heading and stable-anchor
  retention, manifest ownership, Public API source copying, and localization
  inventory. Localization reports 79/195 current translations (40.51 percent),
  with 116 missing.

### Generated, rendered, and browser evidence

- Site generation contains 111 navigation items, 268 public routes, 188
  planned redirects, 195 English and 79 Russian searchable documents, and 79
  complete locale pairs. AI delivery contains 268 public documents and
  1,806,554 full-context bytes; evaluation passes 22 tasks and 50 retrieval
  checks at 100 percent success and 0.900 MRR. Snippets pass 280/280
  normative, 154 evidence, and 159 external-parser checks.
- Complete Engine documentation discovery passes 381/381 tests, all three
  reusable CMake interface validators pass, and standalone validation accepts
  275 Markdown entries. Production Jekyll passes 347 rendered routes, 43
  static endpoints, and 46,323 local references. Chromium passes 694
  desktop/mobile page checks, eleven interaction profiles, and thirteen
  screenshots with no errors or reported accessibility violations.

### Residual and revision state

- Remaining locale work is 116 required pages, followed by the symbol-ID API
  description catalog, complete-parity enforcement, landed CI/Pages evidence,
  and manual production keyboard, 200 percent zoom, and representative
  screen-reader review.
- The final fetch found no incoming range: Last Frontier
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` remains 7 ahead / 0 behind
  `origin/main` `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`; Engine
  `fac978a67d1e601eb77389e8dc562d7e511705a0` remains 14 ahead / 0 behind
  `origin/master` `fee50fb636b5bd1e30509aded929df1fc0e95db5`; and TLA
  `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` exactly matches
  `origin/master`.

## 2026-08-01 - Twentieth physical locale group: Audio and Video delivery

### Scope and source audit

- Migrated the reusable Audio and Video guides plus all thirteen generated
  media-reference pages to canonical English routes under
  `Docs/en/how-to/content/` and `Docs/en/reference/`. Added complete reviewed
  Russian mirrors at the matching `Docs/ru/` routes. The two former authored
  paths and thirteen former generated paths remain durable Markdown pointers
  that preserve every previous H2/H3 route and every generated stable entry
  anchor.
- Re-audited the audio/video source manifests, baker and runtime ownership,
  format and decoder boundaries, fullscreen and embedded playback behavior,
  settings, native tests, and Last Frontier integration links against the
  current Engine and project revisions. The generated JSON models remain the
  machine source under `Docs/generated/`; only their human projections moved
  into the locale tree.

### Implementation and localization

- Updated both media generators to emit canonical English references and
  generated legacy pointers in one deterministic pass. Updated the public
  contract index, documentation manifest, navigation detail classification,
  active Engine/project owner links, maintenance instructions, and source
  evidence routes to use the canonical pages.
- Added regressions for legacy heading and stable-anchor retention, canonical
  and legacy manifest ownership, Public API reference copying, generated
  locale inventory isolation, and the new publication baseline. Localization
  reports 71/195 current translations (36.41 percent), with 124 missing.

### Generated, rendered, and browser evidence

- Audio generation is current at 32 entries, three formats, and ten playback
  rules; Video generation is current at 34 entries, nine fullscreen rules,
  and seven embedded rules. External Last Frontier and TLA source snapshots
  verify against their exact checked-out revisions.
- Site generation contains 111 navigation items, 260 public routes, 188
  planned redirects, 195 English and 71 Russian searchable documents, and 71
  complete locale pairs. AI delivery contains 260 public documents and
  1,750,190 full-context bytes; evaluation passes 22 tasks and 50 retrieval
  checks at 100 percent success and 0.900 MRR. Snippets pass 280/280
  normative, 154 evidence, and 159 external-parser checks.
- Complete Engine documentation discovery passes 380/380 tests, all three
  reusable CMake interface validators pass, and standalone validation accepts
  267 Markdown entries. Production Jekyll passes 331 rendered routes, 43
  static endpoints, and 44,118 local references. Chromium passes 662
  desktop/mobile page checks, eleven interaction profiles, and thirteen
  screenshots with no errors or reported accessibility violations.

### Residual and revision state

- Remaining locale work is 124 required pages, followed by the symbol-ID API
  description catalog, complete-parity enforcement, landed CI/Pages evidence,
  and manual production keyboard, 200 percent zoom, and representative
  screen-reader review.
- The final fetch found no incoming range: Last Frontier
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` remains 7 ahead / 0 behind
  `origin/main` `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`; Engine
  `fac978a67d1e601eb77389e8dc562d7e511705a0` remains 14 ahead / 0 behind
  `origin/master` `fee50fb636b5bd1e30509aded929df1fc0e95db5`; and TLA
  `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` exactly matches
  `origin/master`.

## 2026-08-01 - Nineteenth physical locale group: contract governance decisions

### Scope and source audit

- Migrated Generated Contract Change Management and ADR-0001 through ADR-0006
  to canonical EN/RU contributor routes under
  `Docs/{en,ru}/contributing/`. Their seven former flat paths remain durable
  Markdown pointers with every previous H2/H3 anchor.
- Re-audited the aggregate contract comparator, all eighteen live model names,
  disposition ledger, PR/push baseline selection, Pages/Jekyll/CNAME boundary,
  manifest-backed AI delivery, per-locale navigation/search, public-example
  authority, and version/locale routing against current source models, tests,
  generated artifacts, and workflow jobs.
- Re-fetched Last Frontier, Engine, and TLA before the audit. No incoming
  project or engine range changed the reviewed contracts.

### Corrections and delivery

- Added the omitted AiControl protocol row, generator, source model, tests,
  stable-ID behavior, and disposition domain to the eighteen-domain change
  guide. The previous prose claimed eighteen models while enumerating only
  seventeen.
- Repaired duplicate decision numbering in ADR-0003, made its current Russian
  delivery behavior explicit, changed ADR-0004's historical Slate context to
  past tense, and replaced stale first-two-page and five-README claims with
  generated localization/route ownership. ADR-0001 now names the live
  Jekyll/artifact/browser gates, and ADR-0005 uses one stable title form.
- Expanded manifest source evidence, switched active Engine links and generated
  `PUBLIC_API.md` ownership to canonical routes, and added four CI-required
  tests covering all live comparator models, current delivery configuration,
  decision numbering, and every canonical/legacy route pair.
- Extended translation fence validation from column-zero fences to all
  Markdown-valid zero-to-three-space fences and added a regression for code
  nested under a numbered step.

### Validation evidence

- The new governance foundation suite passes 4/4; the localization suite
  passes 7/7; complete `test_docs*.py` discovery passes 378/378, and standalone
  validation accepts 252 Markdown entries.
- Snippets pass 280/280 normative blocks, 154 evidence blocks, and 159 external
  parser checks. EN/RU structure matches 77 H2/H3 headings and five fenced
  blocks across the seven new pairs.
- Localization reports 56/195 current translations (28.72 percent), with 139
  missing. Site generation contains 111 navigation items, 245 public routes,
  188 planned redirects, 195 English and 56 Russian search documents, and 56
  complete locale pairs. Search payloads are 1,127,316/1,021,204 bytes.
- AI delivery contains 245 public documents and 1,706,135 full-context bytes.
  Deterministic evaluation remains green for all 22 tasks and 50 retrieval
  checks at the 100-percent threshold with 0.900 MRR.
- Production Jekyll passes 301 rendered routes, 43 static endpoints, and 40,166
  local references. Chromium passes 602 desktop/mobile page checks, eleven
  interaction profiles, and thirteen screenshots with no errors or axe
  violations.
- The final fetch found no incoming range: Last Frontier
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` remains 7 ahead / 0 behind
  `origin/main` `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`; Engine
  `fac978a67d1e601eb77389e8dc562d7e511705a0` remains 14 ahead / 0 behind
  `origin/master` `fee50fb636b5bd1e30509aded929df1fc0e95db5`; TLA
  `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` exactly matches
  `origin/master`.

### Residual production work

- Translate and review the remaining 139 required pages plus the symbol-ID API
  description catalog, then enable complete-parity enforcement.
- Confirm the configured GitHub Pages source and landed site/package artifacts,
  complete production keyboard/zoom/screen-reader review, publish the private
  example repositories only through their owner gates, and run at least two
  independent model-family evaluations.

## 2026-08-01 - Eighteenth physical locale group: documentation operations

### Scope and source audit

- Migrated Documentation Maintenance and Documentation Site Publication to
  canonical EN/RU contributor routes under
  `Docs/{en,ru}/contributing/documentation/`. Their former flat paths remain
  durable Markdown pointers with every previous H2/H3 anchor.
- Re-audited manifest ownership, revision-update reconciliation, generator
  dependency order, documentation CI, GitHub Pages/Jekyll/CNAME publication,
  locale routing, static artifact validation, pinned browser dependencies,
  accessibility interactions, and production verification against current
  source, generated models, tests, and workflow jobs.
- Reverified Last Frontier's Engine-update documentation policy as external
  integration evidence and TLA's submodule-update rules as a second project
  boundary. Neither project is normative for Engine behavior or publication.

### Corrections and delivery

- Replaced the stale hand-maintained local test list with complete
  `test_docs_*.py` discovery plus the non-documentation fixture tests,
  structural CMake checks, external snippet parse, and aggregate validator.
  The explicit workflow expansion remains the authoritative CI inventory.
- Corrected the site guide from two locale pairs to 49/195 current mirrors,
  from six to seven explicit README pairs, and from six/four browser artifacts
  to the current eleven interactions and thirteen screenshots. A live
  2026-08-01 check confirmed that `fonline.ru` still serves the basic Slate
  repository presentation, so Pages source settings remain
  `pending-admin-verification` rather than inferred.
- Corrected the Last Frontier maintenance guide from seventeen to eighteen
  generated contract domains, restored `AGENTS.md` as its AI-maintainer owner,
  and pinned the full Engine documentation generator order for every incoming
  project/Engine revision.
- Added four source-backed tests for revision reconciliation and external
  evidence, exact Pages/domain/runtime pins, generator/CI order, current
  browser/locale counts, and canonical/legacy route ownership. Updated the
  profiling and viewer focused tests to follow the canonical maintenance path.

### Validation evidence

- The new foundation suite passes 4/4; all affected focused suites pass;
  complete `test_docs*.py` discovery passes 373/373, and standalone validation
  accepts 245 Markdown entries.
- Snippets pass 280/280 normative blocks, 154 evidence blocks, and 159 external
  parser checks. EN/RU structure matches 24 H2/H3 headings and nine fenced
  blocks across the two new pairs.
- Localization reports 49/195 current translations (25.13 percent), with 146
  missing. Site generation contains 111 navigation items, 238 public routes,
  188 planned redirects, 195 English and 49 Russian search documents, and 49
  complete locale pairs.
- AI delivery contains 238 public documents and 1,701,954 full-context bytes.
  Deterministic evaluation remains green for all 22 tasks and 50 retrieval
  checks at the 100-percent threshold with 0.900 MRR.
- Production Jekyll passes 287 rendered routes, 43 static endpoints, and 38,375
  local references. Chromium passes 574 desktop/mobile page checks, eleven
  interaction profiles, and thirteen screenshots with no errors or axe
  violations.
- The final fetch found no incoming range: Last Frontier
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` remains 7 ahead / 0 behind
  `origin/main` `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`; Engine
  `fac978a67d1e601eb77389e8dc562d7e511705a0` remains 14 ahead / 0 behind
  `origin/master` `fee50fb636b5bd1e30509aded929df1fc0e95db5`; TLA
  `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` exactly matches
  `origin/master`.

### Residual production work

- Translate and review the remaining 146 required pages plus the symbol-ID API
  description catalog, then enable complete-parity enforcement.
- Confirm the configured GitHub Pages source and landed site/package artifacts,
  complete production keyboard/zoom/screen-reader review, publish the private
  example repositories only through their owner gates, and run at least two
  independent model-family evaluations.

## 2026-08-01 - Seventeenth physical locale group: documentation quality contracts

### Scope and source audit

- Migrated Documentation Translation Workflow, Documentation Snippet
  Validation, and AI Documentation Evaluation to canonical EN/RU routes under
  `Docs/{en,ru}/contributing/documentation/`. Their three former flat paths
  remain durable Markdown pointers with every previous H2/H3 anchor.
- Re-audited localization metadata/hash/fence/link validation, all eleven
  snippet languages and parser harnesses, external shell-parser CI, the
  22-task/50-query/61-answer-check AI schema, Python/browser ranking parity,
  manifest ownership, and workflow order against current source and tests.
- Used Last Frontier localization and glossary practice only as external
  governance evidence. TLA has no production documentation localization,
  snippet, or AI-evaluation workflow suitable for normative promotion.

### Corrections and delivery

- Raised `Docs/ai-evaluation.json` from a stale 90-percent retrieval lower bound
  to the 100-percent threshold already promised by the guide. All 50 queries
  pass within their declared rank; model-family final-task success remains a
  separate 90-percent production target.
- Added the current 22/50/61 task/check inventory to the AI guide and preserved
  the explicit boundary between deterministic retrieval evidence and actual
  model answer quality.
- Added four source-backed tests for translation normalization/fence/link
  parity, exact snippet policy and CI ownership, AI threshold/count/ranking
  parity, canonical/legacy routes, and external-evidence ownership.

### Validation evidence

- The group-focused suite passes 33/33 tests; complete `test_docs*.py` discovery
  passes 369/369; standalone validation accepts 243 Markdown entries.
- Snippets pass 280/280 normative blocks, 154 evidence blocks, and 159 external
  parser checks. EN/RU structure matches 24 H2/H3 headings and five fenced
  blocks across the three pairs.
- Localization reports 47/195 current translations (24.10 percent), with 148
  missing. Site generation contains 111 navigation items, 236 public routes,
  188 planned redirects, 195 English and 47 Russian search documents, and 47
  complete locale pairs.
- AI delivery contains 236 public documents and 1,702,696 full-context bytes.
  Deterministic evaluation passes all 22 tasks and 50 retrieval checks at the
  new 100-percent threshold with 0.900 MRR.
- Production Jekyll passes 283 routes, 43 static endpoints, and 37,821 local
  references. Chromium passes 566 desktop/mobile checks, eleven interaction
  profiles, and thirteen screenshots with no errors or axe violations.
- The final fetch found no incoming range: Last Frontier
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` remains 7 ahead / 0 behind
  `origin/main` `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`; Engine
  `fac978a67d1e601eb77389e8dc562d7e511705a0` remains 14 ahead / 0 behind
  `origin/master` `fee50fb636b5bd1e30509aded929df1fc0e95db5`; TLA
  `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` exactly matches
  `origin/master`.

### Residual production work

- Translate and review the remaining 148 required pages plus the symbol-ID API
  description catalog, then enable complete-parity enforcement.
- Run and review at least two materially different model families, confirm
  landed CI/Pages artifacts and production source settings, and complete the
  production accessibility review.

## 2026-08-01 - Sixteenth physical locale group: release safety operations

### Scope and source audit

- Migrated Release Operations, Backup and Recovery, and Security and Secrets to
  canonical EN/RU routes under `Docs/{en,ru}/how-to/release/`. Their three
  former flat paths remain durable Markdown pointers with every previous H2/H3
  anchor.
- Re-audited Windows service registration/startup/shutdown, non-Windows daemon
  startup, service log and health-file locations, JSON/SQLite/Mongo/Memory
  durable-state boundaries, recovery-oplog handling, package-time secret
  resolution, signing handoff, redaction limits, and project-owned operational
  policy against current Engine source and tests.
- Checked Last Frontier's Mongo backup/restore runbooks and `backup_mongo.py`
  implementation as integration evidence. TLA contains no reusable release,
  recovery, or secret-management practice suitable for promotion.

### Corrections and delivery

- Fixed Windows service registration to store the current command line once and
  append the actually consumed `--server-service-start` flag. The old code
  duplicated the executable and registered the unused `--server-service` flag.
- Corrected the guide's log-location claim: health evidence remains in the
  working directory, while logs move under a resolved non-empty
  `Client.UserWritablePath`.
- Strengthened recovery procedure with prior-point size/count plausibility,
  explicit destination allowlisting, dry-run/remap proof, and exact-name cleanup
  guards. The security guide retains the deliberately narrow command-line
  masking guarantee and rejects treating repository-secret masking as a content
  scanner.
- Added four cross-domain release-safety tests plus focused route/source tests,
  updated external-evidence ownership and active Last Frontier links, and kept
  all three legacy route IDs redirect-only.

### Validation evidence

- The group-focused suite passes 32/32 tests; complete `test_docs*.py` discovery
  passes 365/365; standalone validation accepts 240 Markdown entries.
- `LF_ServerService` and `LF_UnitTests` build cleanly. An initial unseeded native
  run exited 1 without a retained Catch summary; a complete repeat passed 465
  test cases and 430,115 assertions, and the exact original seed `3646595785`
  passed 465 cases and 430,114 assertions. This is recorded as a non-reproduced
  native-test flake, not a clean first-pass claim.
- Exception Safety checks 6,104 functions with zero errors and three standing
  manual-review warnings after re-deriving and accepting the six changed body
  hashes. Both local-variable analyzer self-test suites pass eight tests.
- Snippets pass 280/280 normative blocks, 154 evidence blocks, and 159 external
  parser checks. EN/RU parity matches 33 H2/H3 headings and ten fenced blocks.
- Localization reports 44/195 current translations (22.56 percent), with 151
  missing. Site generation contains 111 navigation items, 233 public routes,
  188 planned redirects, 195 English and 44 Russian search documents, and 44
  complete locale pairs. AI delivery contains 233 public documents and
  1,701,152 full-context bytes; retrieval remains 22 tasks/50 checks at 100
  percent success and 0.900 MRR.
- Production Jekyll passes 277 routes, 43 static endpoints, and 37,073 local
  references. Chromium passes 554 desktop/mobile checks, eleven interaction
  profiles, and thirteen screenshots with no errors or axe violations.
- The final fetch found no incoming range: Last Frontier
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` remains 7 ahead / 0 behind
  `origin/main` `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`; Engine
  `fac978a67d1e601eb77389e8dc562d7e511705a0` remains 14 ahead / 0 behind
  `origin/master` `fee50fb636b5bd1e30509aded929df1fc0e95db5`; TLA
  `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` exactly matches
  `origin/master`.

### Residual production work

- Translate and review the remaining 151 required pages plus the symbol-ID API
  description catalog, then enable complete-parity enforcement.
- Confirm landed CI/Pages artifacts and production source settings, and complete
  production-domain keyboard, 200 percent zoom, and screen-reader review.

## 2026-08-01 - Fifteenth physical locale group: build and release foundations

### Scope and source audit

- Migrated BuildTools Pipeline, Upgrade an Embedding Project, Support Matrix,
  and Packaging and Release to canonical EN/RU routes under
  `Docs/{en,ru}/reference/` and `Docs/{en,ru}/how-to/`. Their four former flat
  paths remain durable Markdown pointers with all previous H2/H3 anchors.
- Re-audited the strict ten-stage project interface, current application set,
  required validation matrices, generated-contract comparison CLI, all ten
  support profiles, six package targets, six package platforms, nineteen pack
  tokens, native/Web/Android payloads, Windows installer path, signing hooks,
  and project-owned qualification/release boundaries against live manifests,
  generators, source, workflow, examples, and tests.
- Corrected a source-of-truth defect before translation: the required workflow
  still named `win64-editor`, `linux-editor`, and `linux-gcc-editor` after the
  generic Editor application had been removed. Those three stale matrix
  entries are gone; Mapper and the focused animation/particle viewers remain
  the documented authoring tools.
- Corrected the pipeline's stage ordering and application inventory, the
  upgrade guide's generated-document dependency order, source inventories for
  support/release claims, and every active Engine and Last Frontier link to
  the new locale-aware owners. Generated `PUBLIC_API.md`, support-matrix,
  external-evidence, locale, site, route, search, and AI outputs now consume
  the canonical paths rather than their redirect files.
- Added five source-backed build/release tests covering stage order and Editor
  absence, required CI targets, contract-diff arguments and documentation
  dependency order, support/package capability boundaries, and canonical plus
  legacy route ownership. The new suite runs in fast documentation CI.
- Russian metadata pins normalized hashes
  `4e09fcb815ee562e45daaf8ae924b3f39e696ef4e42cbc6ef93a22428d8b32e3`,
  `4488a08b34a9219d424ffe453b822bc140df72ec5f69bf925d1fd06ec721bf4b`,
  `589f26e4529eee7a08a9bb2bc35e2499606f58745b2401c0fee62e5b5dd42bff`,
  and `88c7455b2ce1d77087a99cde75793489d79bce764e9dade2bf7d24dce7a96a38`.
  Locale validation proves matching levels for all 61 headings and exact
  equality for all nine fenced bodies.

### Generated and standalone validation

- Localization reports 41/195 current, 154 missing, 21.03 percent coverage,
  and intentionally incomplete production parity. Site data contains 111
  navigation items, 41 locale pairs, 195 English search documents, 41 Russian
  search documents, 230 public routes, and 188 planned redirects.
- AI delivery contains 230 public documents and 1,699,531 full-context bytes;
  AI evaluation remains 22 tasks and 50 retrieval checks at 100 percent
  success and 0.900 MRR.
- Snippet validation passes 280/280 normative blocks, 154 evidence blocks, and
  159 external-parser checks. Complete `test_docs*.py` discovery passes
  361/361, including the five new build/release checks. Standalone validation
  accepts 237 manifest-owned Markdown entries.

### Rendered build/release proof

- The production-mode local Jekyll artifact passes 271 rendered routes, 43
  static endpoints, and 36,277 local references with `github-pages` 232,
  Bundler 2.5.11, and local Ruby 3.3.12. CI remains authoritative for the
  repository's exact Ruby 3.3.4 pin.
- Chromium passes all 542 desktop/mobile page checks, eleven interaction
  profiles, and thirteen screenshots with zero errors or axe violations.

### Final update reconciliation

- The final fetch confirms Last Frontier
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` is 7 ahead / 0 behind
  `origin/main` `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`; Engine
  `fac978a67d1e601eb77389e8dc562d7e511705a0` is 14 ahead / 0 behind
  `origin/master` `fee50fb636b5bd1e30509aded929df1fc0e95db5`; and TLA
  `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` exactly matches
  `origin/master`. No incoming range requires another reconciliation pass.

### Residual production work

- Translate and review the remaining 154 subsystem/reference pages and create
  the symbol-ID API-description catalog before enabling complete parity.
- Confirm the landed GitHub Actions artifact and configured Pages source, then
  complete production-domain keyboard, 200 percent zoom, and representative
  screen-reader review.

## 2026-08-01 - Fourteenth physical locale group: build foundations

### Scope and source audit

- Migrated Build Workflow, Embedding FOnline in a Game Project, Generated
  Content Workflow, and Configure a Game Project to canonical EN/RU routes
  under `Docs/{en,ru}/how-to/build/`. Their four former flat paths remain
  durable Markdown pointers with all previous H2/H3 anchors preserved.
- Re-audited the strict ten-stage CMake project interface, current application
  targets, script-compilation and baking dependencies, `.fomain` precedence,
  resource-pack schema, multi-parent sub-config merge, host/target directives,
  minimal embedding project, and project-owned dialog-format boundary against
  current Engine source and tests. Last Frontier and TLA were inspected only
  as integration evidence; TLA's older application wiring is non-normative.
- Corrected five source drifts before translation: Engine has no root
  `CMakeLists.txt`; Applications includes the viewer and no generic editor;
  the live debug setting is `Render.RenderDebug`; `+` has type-specific string,
  vector, numeric, Boolean, and enum semantics; and documentation regeneration
  requires snippets and localization before site, AI evaluation, and delivery.
- Added five source-backed build-foundation tests for stage ordering and strict
  dispatch, codegen dependencies and documentation order, resource packs and
  sub-config inheritance, embedding ownership, and canonical/legacy routes.
  Both this test and the earlier testing-foundation test now run in the fast
  documentation workflow.
- Russian metadata pins normalized hashes
  `c2b78b50520d25da5222856dc1b36b39dc69f0ac631d80972ee1a1d03b24b858`,
  `d0d931bcc3e4c1e5036ac10da027063bbfb23f377f765bb3452b5e6f68433bae`,
  `5e394bba5d29008c25e58d77a9d15df6edbde65b7ade8837dfc5eb1b57da9ad9`,
  and `62b2873d82382ca47dfa2df626e5c472b10a3328c4e39a3bdfb7ffcbf19ec221`.
  Locale validation proves all 42 heading levels and 20 fenced bodies match.

### Generated and standalone validation

- Localization reports 37/195 current, 158 missing, 18.97 percent coverage,
  and intentionally incomplete production parity. Site data contains 111
  navigation items, 37 locale pairs, 195 English search documents, 37 Russian
  search documents, 226 public routes, and 188 planned redirects.
- AI delivery contains 226 public documents and 1,696,004 full-context bytes;
  AI evaluation remains 22 tasks and 50 retrieval checks at 100 percent
  success and 0.900 MRR.
- Snippet validation passes 280/280 normative blocks, 154 evidence blocks, and
  159 external-parser checks. Complete `test_docs*.py` discovery passes
  356/356. Standalone validation accepts 233 manifest-owned Markdown entries.

### Rendered build-foundation proof

- The production-mode local Jekyll artifact passes 263 rendered routes and 43
  static endpoints with `github-pages` 232, Bundler 2.5.11, and local Ruby
  3.3.12. CI remains authoritative for the exact Ruby 3.3.4 pin.
- Chromium passes all 526 desktop/mobile page checks, eleven interaction
  profiles, and thirteen screenshots with zero errors or axe violations.

### Final update reconciliation

- The final fetch confirms Last Frontier
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` is 7 ahead / 0 behind
  `origin/main` `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`; Engine
  `fac978a67d1e601eb77389e8dc562d7e511705a0` is 14 ahead / 0 behind
  `origin/master` `fee50fb636b5bd1e30509aded929df1fc0e95db5`; and TLA
  `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` exactly matches
  `origin/master`. No incoming range requires another reconciliation pass.

### Residual production work

- Translate and review the remaining 158 subsystem/reference pages and create
  the symbol-ID API-description catalog before enabling complete parity.
- Confirm the landed GitHub Actions artifact and configured Pages source, then
  complete production-domain keyboard, 200 percent zoom, and representative
  screen-reader review.

## 2026-08-01 - Thirteenth physical locale group: quality workflows

### Scope and source audit

- Migrated Testing, Gameplay and Integration Testing, and Profiling to
  canonical EN/RU paths under `Docs/{en,ru}/contributing/testing/` and
  `Docs/{en,ru}/how-to/`. Their three former flat paths remain durable Markdown
  pointers with the previous H2/H3 anchors preserved.
- Re-audited `TestingApp.cpp`, the complete 98-file `FO_TESTS_SOURCE` list,
  generated source inventory, test and coverage targets, all four blocking
  sanitizer lanes, Windows 7 import validation, gameplay process-runner schema
  and cleanup semantics, the minimal multiplayer smoke, all four Tracy build
  profiles, instrumentation surfaces, and the exact vendored Tracy 0.13.1
  boundary.
- Corrected one fresh source drift before translation: the test application now
  initializes through `InitAppForTesting()`, not the retired general
  `InitApp(-1, nullptr)` path. A new source-backed test requires the checkout,
  `FO_TESTS_SOURCE`, and generated inventory to contain the same test files and
  pins runner, coverage, sanitizer, canonical-route, and legacy-route claims.
- Verified the documented `tracy-capture` and `tracy-csvexport` flags against
  the official Tracy v0.13.1 tagged sources. The guide keeps CPU, GPU, lock,
  allocator, single-process capture, workload, and embedding-project ownership
  boundaries explicit.
- Russian metadata pins normalized hashes
  `194a66da34f3947b8a187a265b6b49f7915bcf4a9e7c0b5e7257e3d9f46d6c7a`,
  `ac95144021d218a7de114306b5e934415ae1f84cda3fdebc1f755c9176c6aca5`,
  and `5a5cb1033bf9e296f30b9e39a04e42ac65bc50a0a635e18dfa381e2a5733d4ad`.
  Locale validation proves matching heading-level sequences and all 15 fenced
  bodies byte-equivalent after line-ending normalization.

### Generated and standalone validation

- Localization reports 33/195 current, 162 missing, 16.92 percent coverage,
  and intentionally incomplete production parity. Site data contains 111
  navigation items, 33 locale pairs, 195 English search documents, 33 Russian
  search documents, 222 public routes, and 188 planned redirects.
- AI delivery contains 222 public documents; AI evaluation remains 22 tasks
  and 50 retrieval checks at 100 percent success and 0.900 MRR.
- Snippet validation passes 280/280 normative blocks, 154 evidence blocks, and
  159 external-parser checks. Complete `test_docs*.py` discovery passes
  351/351, including the three new testing-foundation checks. Standalone
  validation accepts 229 manifest-owned Markdown entries.

### Rendered quality-workflow proof

- The production-mode local Jekyll artifact passes 255 rendered routes, 43
  static endpoints, and 34,090 local references with `github-pages` 232,
  Bundler 2.5.11, and local Ruby 3.3.12. The artifact gate caught and drove the
  correction of one English fragment used against the translated Russian
  Server Runtime page. CI remains authoritative for the exact Ruby 3.3.4 pin.
- Chromium passes all 510 desktop/mobile page checks, eleven interaction
  profiles, and thirteen screenshots with zero errors or axe violations.

### Final update reconciliation

- The final fetch confirms Last Frontier
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` is 7 ahead / 0 behind
  `origin/main` `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`; Engine
  `fac978a67d1e601eb77389e8dc562d7e511705a0` is 14 ahead / 0 behind
  `origin/master` `fee50fb636b5bd1e30509aded929df1fc0e95db5`; and TLA
  `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` exactly matches
  `origin/master`. No incoming range requires another reconciliation pass.

### Residual production work

- Translate and review the remaining 162 subsystem/reference pages and create
  the symbol-ID API-description catalog before enabling complete parity.
- Confirm the landed GitHub Actions artifact and configured Pages source, then
  complete production-domain keyboard, 200 percent zoom, and representative
  screen-reader review.

## 2026-08-01 - Twelfth physical locale group: native coding contracts

### Scope and source audit

- Migrated Exception Safety, Local Variables, Nullability, Smart Pointers, and
  Thread Safety Analysis to canonical EN/RU paths under
  `Docs/{en,ru}/contributing/coding-contracts/`. Their five former flat paths
  are durable Markdown pointers with the previous H2/H3 anchors preserved.
- Re-audited all eight current destruction-convergence loops, lifecycle tests,
  allocator and exception primitives, the AngelScript nullable compiler/runtime
  checks, native binding guards, the complete smart-pointer wrapper vocabulary,
  Clang 20 local-variable policy, all twenty public `FO_TSA_*` annotations,
  wrapper locks, and both Clang warning-as-error forms in CMake.
- Corrected three source-of-truth defects before translation: restored an
  accidentally truncated proto/fixed-type getter paragraph in Nullability,
  removed a stale Exception Safety claim that map-grid reads require
  `NOT_DESTROYING`, and replaced the old client-input wording with the actual
  untrusted transport plus handler-semantic validation boundary.
- Removed Last Frontier analyzer, baseline, and allowlist claims from normative
  Engine authority. Engine owns its wrapper/compiler/runtime contracts and
  in-repository tests; whole-tree smart-pointer, allocator, exception-safety,
  and local-variable analyzers remain project-owned until promoted with
  reusable Engine implementations and tests.
- Added six source-backed checks in
  `test_docs_native_coding_contracts.py`: teardown-loop count and lifecycle
  evidence, project-owned local analyzer boundary, nullable compiler/runtime
  and native checks, smart-pointer vocabulary and removed permissive modes,
  exact TSA macro/toolchain inventory, and canonical/legacy manifest ownership.
- Russian metadata pins normalized hashes
  `553dec19e8362a02388d69c0a884ebdd238f85cdc7905ee93d1b5e68ad11334c`,
  `6c798039ced9306e31388ca5f287d2e64d9cf0ddbc5fc539acdb9382a10aa7cb`,
  `84091d6708b8c6ff1ddd1fda85796452f935e3d582257062202df65ef1ff49ee`,
  `d184cd78affcc8a010c7da3fb541e46a403181ac7633ca67237706e83c347f64`,
  and `83f13dbf27f02c0bea98329121428aba244ed9eab9665ad20d86de98c37d42b5`.
  Locale validation proves matching heading-level sequences and all 30 fenced
  bodies byte-equivalent after line-ending normalization.

### Generated and standalone validation

- Localization reports 30/195 current, 165 missing, 15.38 percent coverage,
  and intentionally incomplete production parity. Site data contains 111
  navigation items, 30 locale pairs, 195 English search documents, and 30
  Russian search documents.
- Route generation contains 219 public records, 188 planned redirects, 195
  translation targets, and 30 complete pairs. AI delivery contains 219 public
  documents; AI evaluation remains 22 tasks and 50 retrieval checks at 100
  percent success and 0.900 MRR.
- Snippet validation passes 280/280 normative blocks, 154 evidence blocks, and
  159 external-parser checks. Complete `test_docs*.py` discovery passes
  348/348, including the six new coding-contract checks. Standalone validation
  accepts 226 manifest-owned Markdown entries.

### Rendered coding-contract proof

- The production-mode local Jekyll artifact passes 249 rendered routes, 43
  static endpoints, and 33,265 local references with `github-pages` 232,
  Bundler 2.5.11, and local Ruby 3.3.12. The artifact gate caught and drove the
  removal of one duplicate Russian smart-cast compatibility anchor before the
  successful rerun. CI remains authoritative for the exact Ruby 3.3.4 pin.
- Chromium passes all 498 desktop/mobile page checks, eleven interaction
  profiles, and thirteen screenshots with zero errors or axe violations.

### Final update reconciliation

- The final fetch confirms Last Frontier
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` is 7 ahead / 0 behind
  `origin/main` `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`; Engine
  `fac978a67d1e601eb77389e8dc562d7e511705a0` is 14 ahead / 0 behind
  `origin/master` `fee50fb636b5bd1e30509aded929df1fc0e95db5`; and TLA
  `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` exactly matches
  `origin/master`. No incoming range requires another reconciliation pass.

### Residual production work

- Translate and review the remaining 165 subsystem/reference pages and create
  the symbol-ID API-description catalog before enabling complete parity.
- Confirm the landed GitHub Actions artifact and configured Pages source, then
  complete production-domain keyboard, 200 percent zoom, and representative
  screen-reader review.

## 2026-08-01 - Eleventh physical locale group: scripting foundations

### Scope and source audit

- Migrated Scripting, Script Lifecycle and Concurrency, Remote Calls, and the
  Script Methods Map to canonical EN/RU paths. Their four former flat paths are
  durable Markdown pointers that preserve the previous section anchors.
- Resolved a route-plan collision before publication: the generated API index
  retains `/Docs/en/reference/script-api/index.md`, while the human method-owner
  map now owns `/Docs/en/reference/script-api/method-ownership.md`. Stable IDs,
  manifest records, locale switching, search, AI delivery, and all project
  links now agree on those distinct identities.
- Re-audited `ScriptSystem`, the AngelScript backend/compiler/runtime, core
  scripts, all 18 native script-method owners, the server remote-call receive
  path, inbound payload validation, and current scripting tests. Remote Calls
  now records payload-size validation and
  `ValidateInboundRemoteCallData()` before player synchronization and dispatch,
  including collection-count, minimum-wire-size, and full-consumption checks.
  The method map also records both current Mapper screenshot APIs.
- Added four source-backed `test_docs_scripting_foundations.py` checks for core
  script inventory, exact native method-owner coverage, remote-call validation
  order, and canonical/legacy manifest ownership. Existing lifecycle checks now
  read the canonical path.
- The Russian pages pin normalized English hashes
  `a78cb94cfe2455a83731f1a58f6286a3799461ca3bc6cf40b5cc74f907677536`,
  `8f529c514d4ae09a0b6b63f9798afe6433e4cf081b1f2b1f1c5c03b2128d2c78`,
  `35c282439b3dbfc9900061899a9cdbcc6d9efd9341ff0516b9bfb769a9afcb0b`,
  and `71efd74bee7e4dd1ad06e4b474f4ff26bad4b936bec0fae1bc0fdcb4121da8d5`.
  Locale validation also proves matching heading structure and identical fenced
  code after the required line-ending normalization.

### Generated and standalone validation

- Localization reports 25/195 current, 170 missing, 12.82 percent coverage,
  and intentionally incomplete production parity. Site data contains 111
  navigation items, 195 English search documents (1,125,511 bytes), and 25
  Russian search documents (576,000 bytes).
- Route generation contains 214 public records, 188 planned redirects, 195
  translation targets, and 25 complete pairs. AI delivery contains 214 public
  documents; `llms.txt` is 60,262 bytes and `llms-full.txt` is 1,688,384 bytes.
- External-project evidence is current after remapping the four promoted
  scripting routes. Snippet validation passes 281/281 normative blocks, 154
  evidence blocks, and 160 external-parser checks. AI evaluation passes 22
  tasks and 50 retrieval checks at 100 percent success and 0.900 MRR.
- Group-focused localization/site/source checks pass 30/30. Complete
  `test_docs*.py` discovery passes 342/342, and standalone validation accepts
  221 manifest-owned Markdown entries.

### Rendered scripting proof

- The production-mode local Jekyll artifact passes 239 rendered routes, 43
  static endpoints, and 31,959 local references with `github-pages` 232,
  Bundler 2.5.11, and local Ruby 3.3.12. CI remains authoritative for the exact
  repository Ruby 3.3.4 pin.
- Chromium passes all 478 desktop/mobile page checks, eleven interaction
  profiles, and thirteen screenshots with zero errors or axe violations. All
  40 desktop and 7,127 mobile incomplete contrast nodes pass the computed-color
  fallback; there are no failed or unresolved results.

### Final update reconciliation

- The final fetch confirms Last Frontier
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` is 7 ahead / 0 behind
  `origin/main` `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`; Engine
  `fac978a67d1e601eb77389e8dc562d7e511705a0` is 14 ahead / 0 behind
  `origin/master` `fee50fb636b5bd1e30509aded929df1fc0e95db5`; and TLA
  `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` exactly matches
  `origin/master`. No incoming range requires another reconciliation pass.

### Residual production work

- Translate and review the remaining 170 subsystem/reference pages and create
  the symbol-ID API-description catalog before enabling complete parity.
- Confirm the landed GitHub Actions artifact and configured Pages source, then
  complete production-domain keyboard, 200 percent zoom, and representative
  screen-reader review.

## 2026-08-01 - Tenth physical locale group: maps, networking, and persistence

### Scope and source audit

- Migrated Maps, Movement, and Geometry, Networking, and Persistence to their
  canonical EN/RU explanation paths. The three former flat paths remain visible
  durable Markdown pointers with every former H2 anchor, while stable IDs
  `maps-movement-geometry`, `networking`, and `persistence` now belong to the
  canonical sources.
- Re-audited geometry, line tracing, path finding, movement interpolation, map
  loading/baking, all client/server transport implementations, inbound buffer
  and content hardening, unresolved-hash recovery, connection lifetime,
  database facade/backends, commit/reconnect/panic flow, operation logs, and all
  named native tests. Current Last Frontier auth, persistence-maintenance, and
  movement-debug references now route to the canonical Engine owners.
- Corrected four source/documentation drifts before translation: the current
  `MapLoader::Load` signature includes `file_name` and pointer callback types;
  `MapLoader::EnumerateMaps` and exact `Test_Geometry`/`Test_PathFinding` owners
  are documented; Networking describes all five inbound-hardening layers and
  includes `MaxReorderAhead`; Persistence captures the current dirty-tree rule
  that enabled `DataBase.OpLogPath` must end in `.oplog` and derives the
  `-committed.oplog` progress path.
- Added four source-backed `test_docs_runtime_foundations.py` checks for those
  surfaces and canonical/legacy manifest ownership. A Unicode arrow exposed a
  Markdown/Jekyll slug mismatch; an explicit stable
  `inbound-hardening-untrusted-client-server` anchor now passes both source and
  rendered-fragment validators in English and Russian.
- The Russian pages pin normalized English hashes
  `9d3702a565220c1cafd6b38b5d51d2e9d02d7f9070c7115f1d228df5c0914e74`,
  `fbf8da4f2db6374e06cb357fe60694eda8a29227eaad517c83e380e6d90ee2a8`,
  and `146ff388eee4e4e06ec00a1d42b8e7f70ba4525a4dddbbf78c26551e706d4914`.
  The map-loader C++ fence is byte-identical across locales.

### Generated and standalone validation

- Localization reports 21/195 current, 174 missing, 10.77 percent coverage,
  and intentionally incomplete production parity. Site data contains 111
  navigation items, 195 English search documents (1,125,159 bytes), and 21
  Russian search documents (484,418 bytes).
- Route generation contains 210 public records, 188 planned redirects, 194
  translation targets, and 21 complete pairs. AI delivery contains 210 public
  documents; `llms-full.txt` is 1,684,986 bytes and remains within budget.
- External-project evidence is current after remapping persistence/networking
  promotion targets. Snippet validation passes 281/281 normative blocks, 154
  evidence blocks, and 160 external-parser checks. AI evaluation passes 22
  tasks and 50 retrieval checks at 100 percent success and 0.900 MRR.
- Focused runtime/locale/site/evidence/AI/snippet checks pass 100/100 tests.
  Complete `test_docs*.py` discovery passes 338/338, and standalone validation
  accepts 217 manifest-owned Markdown entries.

### Rendered foundational-runtime proof

- The production-mode local Jekyll artifact passes 231 rendered routes, 43
  static endpoints, and 30,843 local references with `github-pages` 232,
  Bundler 2.5.11, and local Ruby 3.3.12. CI remains authoritative for the exact
  repository Ruby 3.3.4 pin.
- Chromium passes all 462 desktop/mobile page checks, eleven interaction
  profiles, and thirteen screenshots with zero errors. All 22 desktop and
  7,115 mobile axe incomplete contrast nodes pass the computed-color fallback;
  there are no failed or unresolved results.

### Final update reconciliation

- The final fetch confirms Last Frontier
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` is 7 ahead / 0 behind
  `origin/main` `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`; Engine
  `fac978a67d1e601eb77389e8dc562d7e511705a0` is 14 ahead / 0 behind
  `origin/master` `fee50fb636b5bd1e30509aded929df1fc0e95db5`; and TLA
  `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` exactly matches
  `origin/master`. No incoming range requires another reconciliation pass.

### Residual production work

- Translate and review the remaining 174 subsystem/reference pages and create
  the symbol-ID API-description catalog before enabling complete parity.
- Confirm the landed GitHub Actions artifact and configured Pages source, then
  complete production-domain keyboard, 200 percent zoom, and representative
  screen-reader review.

## 2026-08-01 - Ninth physical locale group: entity and runtime core

### Scope and source audit

- Migrated Entity Model, Client Runtime, and Server Runtime to canonical EN/RU
  paths. `Docs/EntityModel.md`, `Docs/ClientRuntime.md`, and
  `Docs/ServerRuntime.md` remain visible durable pointers with their former H2
  anchors, while the canonical records retain stable IDs `entity-model`,
  `client-runtime`, and `server-runtime`.
- Re-audited the Common entity/property/prototype implementation, client and
  server composition roots, resource/view/model managers, synchronization and
  recipient-broadcast contracts, updater hosting, persistence boundaries, and
  all 16 native test files named by the three guides. A stale server-runtime
  reference to future scripting documentation was corrected before translation.
- The Russian pages pin normalized English hashes
  `6278c5c99ed0e38dcb6060f477848672f6acf99f8f1e7f2b6328e0ae66834f5c`,
  `5b0348d50055f0b57e57abcd3b3a4d765f2321b1b30ef04cc75e01099419c5b1`,
  and `abcf8b1246debf66fdf70ad13731027d95c17bd8edaa1fc61ce149af3d06dc73`.
  The full server translation preserves the independent-root cover protocol,
  sync-free recipient fan-out, TSA guards, temporary entity-access validation,
  re-entrant lifecycle rules, persistence migration, message-drain SyncContext,
  movement reconciliation, updater ownership, and validation checklist.
- Current indexes and cross-references now use canonical locale-aware runtime
  routes. The repository and Applications Russian pages were reviewed and
  rehashed after their links moved to the available Russian counterparts.

### Generated and standalone validation

- Localization reports 18/195 current, 177 missing, 9.23 percent coverage, and
  intentionally incomplete production parity. Site data contains 111
  navigation items, 195 English search documents (1,124,870 bytes), and 18
  Russian search documents (408,253 bytes).
- Route generation contains 207 public records, 188 planned redirects, 194
  translation targets, and 18 complete pairs. AI delivery contains 207 public
  documents; `llms-full.txt` is 1,683,130 bytes and remains within budget.
- Snippet validation passes 281/281 normative blocks, 154 evidence blocks, and
  160 external-parser checks. AI evaluation passes 22 tasks and 50 retrieval
  checks at 100 percent success and 0.900 MRR.
- Focused locale/site/layout/validation/AI/snippet checks pass 86/86 tests.
  Complete `test_docs*.py` discovery passes 334/334, and standalone validation
  accepts 214 manifest-owned Markdown entries.

### Rendered runtime-route proof

- The production-mode local Jekyll artifact passes 225 rendered routes, 43
  static endpoints, and 30,039 local references with `github-pages` 232,
  Bundler 2.5.11, and local Ruby 3.3.12. CI remains authoritative for the exact
  repository Ruby 3.3.4 pin.
- Chromium passes all 450 desktop/mobile page checks, eleven interaction
  profiles, and thirteen screenshots with zero errors. All 22 desktop and
  7,115 mobile axe incomplete contrast nodes pass the computed-color fallback;
  there are no failed or unresolved results.

### Final update reconciliation

- The final fetch confirms Last Frontier
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` is 7 ahead / 0 behind
  `origin/main` `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`; Engine
  `fac978a67d1e601eb77389e8dc562d7e511705a0` is 14 ahead / 0 behind
  `origin/master` `fee50fb636b5bd1e30509aded929df1fc0e95db5`; and TLA
  `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` exactly matches
  `origin/master`. No incoming source or documentation range requires another
  reconciliation pass for this group.

### Residual production work

- Translate and review the remaining 177 subsystem/reference pages and create
  the symbol-ID API-description catalog before enabling complete parity.
- Confirm the landed GitHub Actions artifact and configured Pages source, then
  complete production-domain keyboard, 200 percent zoom, and representative
  screen-reader review.

## 2026-08-01 - Eighth physical locale group: architecture and source navigation

### Scope and source audit

- Migrated Engine Architecture, Source Tree Guide, and Applications and Entry
  Points to canonical EN/RU paths while retaining `Docs/Architecture.md`,
  `Docs/SourceTree.md`, and `Docs/Applications.md` as visible durable pointers
  with their former heading anchors. Stable IDs and locale-aware permalinks now
  own the canonical pages rather than the flat routes.
- Audited all 13 current `Source/Applications/*.cpp` files, all 10 current
  `BuildTools/cmake/stages/*.cmake` files, application-target wiring, and the
  client startup settings hook. The reusable descriptions match the current
  source. A stale statement that treated `Docs/Testing.md` as future work was
  corrected before translation.
- The Russian metadata pins normalized English hashes
  `d32b75f58a220743e1084fc5e93c03ae2a12f270520160f9f20935d1c428230c`,
  `1677c032ccfb5129471dba57a295dc4cc2401ad1d58969e90f84da282824caaf`,
  and `ec0cd5c63ae491cb1b03f975c0aee49e8a379a40aea3e4b1bc7068302137187b`.
- Nested canonical ownership exposed two stale path assumptions. The diagram
  generator now resolves SVG references relative to each owning document, and
  the browser audit locates the architecture page by stable document ID instead
  of the retired physical route. Both behaviors have regression coverage.

### Generated and standalone validation

- Localization reports 15/195 current, 180 missing, 7.69 percent coverage, and
  intentionally incomplete production parity. Site data contains 111
  navigation items, 15 locale pairs, 195 English search documents (1,125,120
  bytes), and 15 Russian search documents (219,672 bytes).
- Route generation contains 204 public records, 188 planned redirects, 194
  translation targets, and 15 complete pairs. AI delivery contains 204 public
  documents; `llms-full.txt` is 1,681,240 bytes and remains within budget.
- Snippet validation passes 281/281 normative blocks, 154 evidence blocks, and
  160 external-parser checks. AI evaluation passes 22 tasks and 50 retrieval
  checks at 100 percent success and 0.900 MRR.
- Focused locale/site/diagram/browser/aggregate validation passes 96/96 tests.
  Complete `test_docs*.py` discovery passes 334/334, and standalone validation
  accepts 211 manifest-owned Markdown entries.

### Rendered canonical-route proof

- The production-mode local Jekyll artifact passes 219 rendered routes, 43
  static endpoints, and 29,185 local references with `github-pages` 232,
  Bundler 2.5.11, and local Ruby 3.3.12. CI remains authoritative for the exact
  repository Ruby 3.3.4 pin.
- Chromium passes all 438 desktop/mobile page checks, eleven interaction
  profiles, and thirteen screenshots with zero errors. Architecture diagram
  rendering is proved on `/Docs/en/explanation/architecture/`; Russian search,
  every explicit README pair, and locale switching remain green.
- All 22 desktop and 7,115 mobile axe incomplete contrast nodes pass the
  computed-color fallback with no failed or unresolved results.

### Final update reconciliation

- The final fetch confirms Last Frontier
  `d54645eda827c4d4ade75e4f542cf6b7c8f9682f` is 7 ahead / 0 behind
  `origin/main` `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5`; Engine
  `fac978a67d1e601eb77389e8dc562d7e511705a0` is 14 ahead / 0 behind
  `origin/master` `fee50fb636b5bd1e30509aded929df1fc0e95db5`; and TLA
  `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` exactly matches
  `origin/master`. No incoming source or documentation range requires another
  reconciliation pass for this group.

### Residual production work

- Translate and review the remaining 180 subsystem/reference pages and create
  the symbol-ID API-description catalog before enabling complete parity.
- Confirm the landed GitHub Actions artifact and configured Pages source, then
  complete production-domain keyboard, 200 percent zoom, and representative
  screen-reader review.

## 2026-07-31 - Third physical locale group and translated entrypoint

### Scope and source ownership

- Migrated the tested headless-project lesson from `TUTORIAL.md` to canonical
  `Docs/en/tutorials/first-project.md`, added its complete reviewed Russian
  mirror, and retained the old route as a visible pointer with all former
  heading anchors. The existing `legacy-tutorial-entry` stable ID remains the
  canonical document identity; the pointer has a separate non-human route ID.
- Added the manifest-planned `Examples/MinimalProject/README.ru.md` entrypoint
  beside the retained English README. Both declare locale, stable document ID,
  and explicit permalink; the Russian page preserves all four fenced bodies
  and links to the Russian first-project lesson.
- Reconciled root, maintainer, onboarding, build, embedding, backlog, and
  generated source links. The GUI runtime source model now cites the canonical
  first-project path instead of the legacy pointer; all seven GUI reference
  pages and the cross-domain public API index were regenerated.
- The two new translations preserve seven fenced bodies byte-for-byte and pin
  current normalized source hashes. Localization now reports 6/195 current,
  189 missing, 3.08 percent coverage, and intentionally incomplete parity.

### Generated and executable validation

- Current output contains 111 navigation items, 195 English and 6 Russian
  search documents, 201 route records, 188 legacy redirects, 194 translation
  targets, and 6 complete locale pairs. AI delivery contains 201 public
  documents and remains below its full-context budget.
- Snippet validation passes 281/281 normative blocks, 154 evidence blocks, and
  160 external-parser checks. AI evaluation passes 22 tasks and 50 retrieval
  checks at 100 percent success and 0.900 MRR. External project evidence
  remains current.
- Complete `test_docs*.py` discovery passes 332/332 tests. The expanded
  validator fixture explicitly excludes only its synthetic GUI supporting
  source while production inventory remains fail-closed. Standalone validation
  accepts 208 manifest-owned Markdown entries.

### Rendered entrypoint proof

- The production-mode local Jekyll artifact passes 207 rendered routes, 43
  static endpoints, and 27,481 local references with `github-pages` 232,
  Bundler 2.5.11, and local Ruby 3.3.12. CI remains authoritative for the exact
  repository Ruby 3.3.4 pin.
- Chromium passes all 414 desktop/mobile page checks, five interaction
  profiles, and seven screenshots with zero errors. The added interaction
  opens `/Examples/MinimalProject/README.ru.html`, confirms active Russian
  state and `html lang=ru`, clicks EN, and reaches the exact paired
  `/Examples/MinimalProject/README.html` route.
- All 20 desktop and 7,103 mobile axe incomplete contrast nodes pass the
  computed-color fallback with no failed or unresolved results. The original
  Russian documentation search and exact Docs-locale switch remain green.

### Residual production work

- Translate and review the remaining 189 required pages, including the other
  repository/subsystem entrypoints and the symbol-ID API-description catalog,
  then enable complete-parity enforcement only after the full bilingual gate
  passes.
- Confirm the landed GitHub Actions artifact and configured Pages source, then
  complete production-domain keyboard, 200 percent zoom, and representative
  screen-reader review.

## 2026-07-31 - Fourth physical locale group and repository entrypoints

### Scope and locale ownership

- Added complete reviewed `README.ru.md` counterparts for the repository home
  and `Examples/MinimalMultiplayer`. Both English pages now declare explicit
  locale, stable document ID, and permalink metadata; the Russian pages use the
  same IDs, explicit `.ru.html` routes, and current normalized source hashes.
- The root translation preserves both fenced bodies byte-for-byte. The
  Minimal Multiplayer translation preserves all ten fenced bodies, keeps the
  runnable source/package/Mapper contract intact, and routes the playable-client
  lesson to the canonical locale-specific tutorial.
- Russian tutorial links now select the root and example counterpart whenever
  that counterpart is current. `Source/README.md` and `BuildTools/README.md`
  remain explicit English fallbacks because their planned Russian targets are
  still missing.
- Localization reports 8/195 current, 187 missing, 4.10 percent coverage, and
  intentionally incomplete production parity. Site data contains 111
  navigation items, 8 locale pairs, 195 English search documents (1,124,889
  bytes), and 8 Russian search documents (111,008 bytes).

### Generated and standalone validation

- Route generation contains 201 public records, 188 legacy redirects, 194
  translation targets, and 8 complete pairs. AI delivery contains 201 public
  documents; `llms-full.txt` is 1,678,867 bytes and remains within budget.
- Snippet validation passes 281/281 normative blocks, 154 evidence blocks, and
  160 external-parser checks. AI evaluation passes 22 tasks and 50 retrieval
  checks at 100 percent success and 0.900 MRR.
- Focused locale/site/layout/browser validation passes 27/27 tests. Complete
  `test_docs*.py` discovery passes 332/332, and standalone validation accepts
  208 manifest-owned Markdown entries.

### Rendered entrypoint proof

- The production-mode local Jekyll artifact passes 209 rendered routes, 43
  static endpoints, and 27,825 local references with `github-pages` 232,
  Bundler 2.5.11, and local Ruby 3.3.12. CI remains authoritative for the exact
  repository Ruby 3.3.4 pin.
- Chromium passes all 418 desktop/mobile page checks, seven interaction
  profiles, and nine screenshots with zero errors. Dedicated interactions open
  `/README.ru.html`, `/Examples/MinimalProject/README.ru.html`, and
  `/Examples/MinimalMultiplayer/README.ru.html`, verify the active Russian
  state and `html lang=ru`, then reach each exact paired English route.
- All 21 desktop and 7,109 mobile axe incomplete contrast nodes pass the
  computed-color fallback with no failed or unresolved results. Russian search,
  responsive navigation, keyboard operation, theme persistence, code copy, and
  architecture-diagram rendering remain green.

### Residual production work

- Translate and review the remaining 187 required pages, including subsystem
  README entrypoints and the symbol-ID API-description catalog, then enable
  complete-parity enforcement only after the full bilingual gate passes.
- Confirm the landed GitHub Actions artifact and configured Pages source, then
  complete production-domain keyboard, 200 percent zoom, and representative
  screen-reader review.

## 2026-07-31 - Fifth physical locale group and source entrypoints

### Scope and source-backed correction

- Added complete reviewed Russian counterparts for `Source/README.md` and
  `Source/Tests/README.md`. Both EN/RU pairs now carry explicit layout, locale,
  stable document ID, and permalink metadata plus current normalized-source
  hashes on the Russian pages.
- Corrected the English source-tree entrypoint before translation: it now routes
  all seven current top-level source roles and uses maintained architecture
  references. The unit-test entrypoint now lists all 98 current `Test_*.cpp`
  files; its previous 71-file list omitted 27 existing core, networking, and
  frontend tests but contained no nonexistent names.
- Added a source-backed regression that requires both EN and RU unit-test
  READMEs to contain exactly the generated test inventory. The two translated
  command fences remain byte-identical and pass their real shell parsers.
- Localization reports 10/195 current, 185 missing, 5.13 percent coverage, and
  intentionally incomplete production parity. Site data contains 111
  navigation items, 10 locale pairs, 195 English search documents (1,125,352
  bytes), and 10 Russian search documents (134,028 bytes).

### Generated and standalone validation

- Route generation contains 201 public records, 188 legacy redirects, 194
  translation targets, and 10 complete pairs. AI delivery contains 201 public
  documents; `llms-full.txt` is 1,680,319 bytes and remains within budget.
- Snippet validation passes 281/281 normative blocks, 154 evidence blocks, and
  160 external-parser checks. AI evaluation passes 22 tasks and 50 retrieval
  checks at 100 percent success and 0.900 MRR.
- Focused inventory/locale/site/layout/browser validation passes 30/30 tests.
  Complete `test_docs*.py` discovery passes 333/333, and standalone validation
  accepts 208 manifest-owned Markdown entries.

### Rendered source-entrypoint proof

- The production-mode local Jekyll artifact passes 211 rendered routes, 43
  static endpoints, and 28,083 local references with `github-pages` 232,
  Bundler 2.5.11, and local Ruby 3.3.12. CI remains authoritative for the exact
  repository Ruby 3.3.4 pin.
- Chromium passes all 422 desktop/mobile page checks, nine interaction
  profiles, and eleven screenshots with zero errors. Dedicated interactions
  open `/Source/README.ru.html` and `/Source/Tests/README.ru.html`, verify the
  active Russian state and `html lang=ru`, then reach each exact English pair.
- All 21 desktop and 7,110 mobile axe incomplete contrast nodes pass the
  computed-color fallback with no failed or unresolved results. The previous
  three entrypoint checks, Russian search, responsive navigation, keyboard,
  theme, copy, and diagram scenarios remain green.

### Residual production work

- Translate and review the remaining 185 required pages, including BuildTools,
  AiControl sample, and subsystem entrypoints plus the symbol-ID API-description
  catalog, then enable complete parity only after the full bilingual gate
  passes.
- Confirm the landed GitHub Actions artifact and configured Pages source, then
  complete production-domain keyboard, 200 percent zoom, and representative
  screen-reader review.

## 2026-07-31 - Sixth physical locale group and AiControl sample

### Scope and executable source audit

- Added a complete reviewed `Examples/AiControlSample/README.ru.md` counterpart
  and made the English page's default Jekyll layout explicit. Both pages use the
  existing stable document ID and exact locale-specific permalinks; the Russian
  metadata pins the current normalized English hash.
- Reconciled the README with `ai_control_sample.py`, the reference client,
  generated protocol model, and focused tests before translation. The live
  protocol smoke passes all 12 authorization, liveness, state, command,
  completion, and event-cursor checks; 13 protocol/documentation tests also
  prove token and loopback boundaries plus bounded state.
- Both PowerShell fences are byte-identical to English and pass the real parser.
  The Russian guide retains the explicit no-TLS, server-authority, shipping
  compile-out, and project-owned observation/action boundaries.
- Localization reports 11/195 current, 184 missing, 5.64 percent coverage, and
  intentionally incomplete production parity. Site data contains 111
  navigation items, 11 locale pairs, 195 English search documents (1,125,352
  bytes), and 11 Russian search documents (141,128 bytes).

### Generated and standalone validation

- Route generation contains 201 public records, 188 legacy redirects, 194
  translation targets, and 11 complete pairs. AI delivery contains 201 public
  documents; `llms-full.txt` is 1,680,334 bytes and remains within budget.
- Snippet validation passes 281/281 normative blocks, 154 evidence blocks, and
  160 external-parser checks. AI evaluation passes 22 tasks and 50 retrieval
  checks at 100 percent success and 0.900 MRR.
- Focused protocol/locale/site/layout/browser validation passes 40/40 tests.
  Complete `test_docs*.py` discovery passes 333/333, and standalone validation
  accepts 208 manifest-owned Markdown entries.

### Rendered sample-entrypoint proof

- The production-mode local Jekyll artifact passes 212 rendered routes, 43
  static endpoints, and 28,209 local references with `github-pages` 232,
  Bundler 2.5.11, and local Ruby 3.3.12. CI remains authoritative for the exact
  repository Ruby 3.3.4 pin.
- Chromium passes all 424 desktop/mobile page checks, ten interaction profiles,
  and twelve screenshots with zero errors. The new interaction opens
  `/Examples/AiControlSample/README.ru.html`, verifies active Russian state and
  `html lang=ru`, then reaches the exact English pair.
- All 21 desktop and 7,112 mobile axe incomplete contrast nodes pass the
  computed-color fallback with no failed or unresolved results. Russian search
  and the previous five entrypoint checks remain green.

### Residual production work

- Translate and review the remaining 184 required pages. `BuildTools/README.md`
  is the last manifest-planned public entrypoint without its Russian pair;
  subsystem guides and the symbol-ID API-description catalog remain afterward.
- Confirm the landed GitHub Actions artifact and configured Pages source, then
  complete production-domain keyboard, 200 percent zoom, and representative
  screen-reader review.

## 2026-07-31 - Seventh physical locale group and BuildTools entrypoint

### Scope and operational correction

- Added a complete reviewed `BuildTools/README.ru.md` counterpart and made the
  English page's layout, title, locale, stable document ID, and permalink
  explicit. The Russian metadata pins normalized English hash
  `de06e4915d1eccdfd058dac4d211e15441816201a4a3de4586c328aca7ba7356`.
- Corrected the English operational source before translating it. The test
  instructions now discover all 49 current `test_docs*.py` files instead of
  naming 29, and the command set includes AiControl, inventory, localization,
  and aggregate validation checks. The `FO_WORKSPACE` example no longer exports
  `FO_ENGINE_ROOT`.
- Removed the Last Frontier-specific antivirus-plan path and volatile provider
  price/reputation promises from reusable signing guidance. The examples are
  provider-neutral integration shapes and defer current CA/key-storage policy
  to the selected provider and embedding-project release owner.
- Live generated checks confirm 12 main CLI commands/24 arguments, 9 helper
  CLIs/16 subcommands/74 arguments, 44 CMake options/10 stages/6 helpers, and a
  package model with 6 targets, 6 platforms, and 19 packs. All nine translated
  fences are byte-identical and parser-clean.
- Localization reports 12/195 current, 183 missing, 6.15 percent coverage, and
  intentionally incomplete production parity. Site data contains 111
  navigation items, 12 locale pairs, 195 English search documents (1,125,298
  bytes), and 12 Russian search documents (193,432 bytes).

### Generated and standalone validation

- Route generation contains 201 public records, 188 legacy redirects, 194
  translation targets, and 12 complete pairs. AI delivery contains 201 public
  documents; `llms-full.txt` is 1,680,336 bytes and remains within budget.
- Snippet validation passes 281/281 normative blocks, 154 evidence blocks, and
  160 external-parser checks. AI evaluation passes 22 tasks and 50 retrieval
  checks at 100 percent success and 0.900 MRR.
- Focused CLI/CMake/package/locale/site/snippet validation passes 52/52 tests.
  Complete `test_docs*.py` discovery passes 334/334, and standalone validation
  accepts 208 manifest-owned Markdown entries.

### Rendered BuildTools proof

- The production-mode local Jekyll artifact passes 213 rendered routes, 43
  static endpoints, and 28,375 local references with `github-pages` 232,
  Bundler 2.5.11, and local Ruby 3.3.12. CI remains authoritative for the exact
  repository Ruby 3.3.4 pin.
- Chromium passes all 426 desktop/mobile page checks, eleven interaction
  profiles, and thirteen screenshots with zero errors. The new interaction
  opens `/BuildTools/README.ru.html`, verifies active Russian state and
  `html lang=ru`, then reaches the exact English pair.
- All 22 desktop and 7,115 mobile axe incomplete contrast nodes pass the
  computed-color fallback with no failed or unresolved results. Every explicit
  README entrypoint pair, Russian search, and the existing UI scenarios remain
  green.

### Residual production work

- All manifest-planned README entrypoints now have reviewed current Russian
  counterparts. Translate and review the remaining 183 subsystem/reference
  pages and create the symbol-ID API-description catalog before enabling
  complete parity.
- Confirm the landed GitHub Actions artifact and configured Pages source, then
  complete production-domain keyboard, 200 percent zoom, and representative
  screen-reader review.

## 2026-08-02 - Complete generated-description localization and Native API

### Source-model correction and reviewed catalog

- Completed the stable-locator Russian description catalog for all twenty
  generated contract domains. The current inventory contains 4,917/4,917 current
  entries with zero missing or stale records; physical document parity remains
  independently complete at 195/195.
- Added 2,474 reviewed Native API entries: 2,472 descriptions, one exact
  contract note, and one model-level scope note. The translations preserve signatures, identifiers,
  literals, paths, units, inline-code spans, and synchronization terminology.
- Corrected `BuildTools/docs_api.py` so `ReSharper disable` and `NOLINT`
  suppression comments cannot become reader-facing descriptions. This removed
  six false descriptions without hiding ordinary source prose; a focused
  regression protects the distinction.
- The canonical API inventory contains 2,472 symbols, and all 2,472 now have
  source-grounded descriptions. Stability is classified independently from
  prose through the exact and inventory-pinned scope contracts described below.

### Rendering and fail-closed enforcement

- `BuildTools/docs_reference.py` now applies the semantic overlay to all seven
  canonical Russian Native API pages while retaining source hashes from the
  canonical English model. English JSON and Markdown remain unchanged in
  meaning, and legacy routes continue to point at the canonical pages.
- `Docs/description-translations.ru.json` and CI now use `complete`
  enforcement. Missing, stale, structurally altered, or unclassified entries
  fail before rendering; language-neutral values remain explicit catalog
  decisions rather than implicit fallbacks.
- Both localization guides and the production plan record the complete twenty-
  domain, 4,917-entry contract and the exact `--enforce-complete` maintenance
  commands.

### Full validation evidence

- Regenerated all 32 documentation/model/site/AI artifact families in declared
  dependency order. The complete `BuildTools/tests/test_*.py` discovery passes
  535 tests in 152.436 seconds after the reference-type, value-field, enum, and
  inventory-pinned stability
  completeness expansions;
  all three CMake interface validators pass.
- Standalone validation accepts 391 Markdown entries. Snippet validation passes
  293 normative blocks, 156 evidence blocks, and 170 external-parser checks.
  AI evaluation passes 27 tasks and 63 retrieval checks at 100 percent success,
  100 percent top-three retrieval, and 0.886 mean reciprocal rank.
- A production Jekyll 3.10.0 build using local Ruby 3.3.12 and Bundler 2.5.11
  succeeds in 29.963 seconds of Jekyll generation time and 34.347 seconds wall
  time. CI remains authoritative for the repository's exact Ruby 3.3.4 pin.
  Artifact validation covers 579 rendered routes, 44 static endpoints, and
  80,241 local references with zero errors.
  Site generation contains 110 navigation items, 195 English plus 195 Russian
  searchable documents, 384 public routes, 188 durable redirects, and 195/195
  completed Russian mirror targets. The search indexes occupy 1,159,732 English
  and 1,621,124 Russian bytes. AI delivery contains 384 public documents and
  1,818,789 full-context bytes.
- Chromium 151 passes 1,158 desktop/mobile page checks, eleven interaction
  profiles, thirteen screenshots, and the automated WCAG 2.2 A/AA axe subset
  with zero failed or unresolved findings. The current report is
  `Workspace/docs-browser-api-stability-report.json` with screenshots under
  `Workspace/docs-browser-api-stability-screenshots/`.
- The source-authoring continuation documents all 121 event symbols at their
  `FO_ENTITY_EVENT` declarations: 31 server globals, 60 client/Mapper-shared
  hooks, 26 server entity events, and four Mapper-only hooks. It records actual
  firing stages, mutable arguments, `StopChain` behavior, and the exported
  project-defined slots that Engine does not fire directly. All 121 events now
  have reviewed Russian overlay entries.
- The same source-authoring pass documents all 133 property symbols: three
  shared declarations expanded across seven entity types plus every Game,
  Player, Item, Critter, Map, and Location property. The descriptions pin
  ownership, persistence, multihex, visibility, movement, rendering, lighting,
  day-color, and callback semantics, and all properties have reviewed Russian
  overlay entries. A focused regression now rejects any undescribed event or
  property.
- Regenerated the map- and prototype-format models that project Native API
  properties into authored-data references, then propagated the reviewed
  property translations to 108 map-format and 113 prototype-format locators.
  This keeps all three reference surfaces semantically identical instead of
  leaving the property prose untranslated outside the API tables.
- The next method slice documents all 16 exports in
  `Source/Scripting/ClientItemScriptMethods.cpp`, including clone ownership,
  recursive map-position resolution, immediate container contents, map-only
  animation and movement, alpha fading, and finish behavior. All 16 have
  reviewed Russian overlays, and a focused regression rejects future missing
  descriptions in that source file. The affected API, inventory, translation,
  reference, localization, snippet, site, and AI-delivery suites pass 66 tests;
  standalone validation still accepts all 391 maintained Markdown entries.
- Both exports in `Source/Scripting/ClientImGuiScriptMethods.cpp` now document
  positive-size and UV validation, atlas-only partial UV rendering, missing-
  sprite behavior, image-button identity, state framing, background/tint, and
  press results. Their reviewed Russian overlays and the per-source completeness
  regression close the second client method owner.
- All 33 exports in `Source/Scripting/ClientEntityScriptMethods.cpp` now
  distinguish one-shot and repeating client time events, direct and contextual
  payloads, callback-wide versus ID-specific selection, and the rescheduling
  side effect of changing a repeat interval. Reviewed Russian overlays and the
  same per-source completeness regression close the third client method owner.
- All 38 exports in `Source/Scripting/ClientCritterScriptMethods.cpp` now
  separate replicated state and inventory queries from local map presentation,
  movement, 2D/3D animation, particles, bone positions, alpha, and predictive
  inventory-slot changes. The descriptions pin off-map exceptions, 2D/3D
  fallbacks, lookup priority, non-authoritative mutation, and callback lifetime;
  reviewed Russian overlays and completeness coverage close the fourth owner.
- All 59 exports in `Source/Scripting/ClientMapScriptMethods.cpp` now document
  render-event restrictions, item and critter lookup order, path and multihex
  behavior, screen/map coordinate conversion, scrolling, zoom, fog, transparent
  masks, visibility fields, and client-local item creation. Reviewed Russian
  overlays and per-source completeness coverage close the fifth client method
  owner and leave no undescribed `Map` receiver methods.
- All 56 exports in `Source/Scripting/MapperGlobalScriptMethods.cpp` now
  document Mapper entity editing, map lifecycle and sandboxed saves, tab
  palettes, view centering, overlays, fit zoom, and both map-only and full-window
  screenshot paths. Reviewed Russian overlays and per-source completeness
  coverage close the Mapper method owner.
- All 77 exports in `Source/Scripting/CommonGlobalScriptMethods.cpp` now
  document debugger and process integration, resources, UTF-8 conversion,
  geometry and line tracing, prototype lookup and filters, language and time
  conversion, plus every common one-shot and repeating time-event overload.
  Reviewed Russian overlays and per-source completeness coverage close the
  common `Game` owner.
- All 118 exports in `Source/Scripting/ClientGlobalScriptMethods.cpp` now
  document client state, entity lookup, geometry, media, text, effects, input
  simulation, sprite lifetime, manual rendering, 2D/3D critter previews,
  offscreen surfaces, screenshots, cache storage, user configuration, window
  control, and connection requests. Reviewed Russian overlays and per-source
  completeness coverage close the client `Game` owner.
- All 236 exports in `Source/Scripting/CommonImGuiScriptMethods.cpp` now
  document active-frame access, balanced window and scope lifetimes, widgets,
  tables, popups, menus, tabs, logging, color conversion, temporary text
  buffers, clipboard access, and ini persistence. Reviewed Russian overlays and
  per-source completeness coverage close the final method owner; no native
  method remains without source prose.
- The eight previously undescribed geometric value types exported from
  `Source/Essentials/ExtendedTypes.h` now define their signed width, numeric
  precision, component names, and script mutability at the source tags.
  Reviewed Russian overlays and a focused completeness assertion protect the
  `ipos8`, `ipos16`, `ipos`, `isize`, `irect`, `fpos`, `fsize`, and `frect`
  contracts. Field descriptions remain a separate structured-metadata task.
- The remaining twelve value types now document strong identity semantics,
  map coordinates and directions, text-pack names and keys, duration versus
  monotonic versus synchronized time, and per-frame gamepad snapshots. All 22
  native value types now have reviewed Russian overlays, and the API regression
  rejects any future undescribed value type globally.
- All 28 migration rules now explain their lookup effect: the existing
  compatibility-version rule plus 27 legacy Item, Critter, Map, and Location
  property-name mappings. Reviewed Russian overlays are derived from the exact
  structured scope, old name, and replacement, and a global regression rejects
  any future undescribed migration rule.
- `BuildTools/codegen.py` now preserves adjacent and inline source comments on
  exported reference-type members while rejecting detached comments and export
  tags. All six reference types, their 41 fields, and their 28 methods now
  document ownership, disposal, mutable rendering state, movement geometry and
  timing, and deferred time-event changes. All 75 contracts have reviewed
  Russian overlays, a focused parser fixture protects comment association, and
  global completeness assertions reject future gaps in all three reference
  symbol kinds.
- All seven exported entities now define their reusable script identity at the
  source tag: the global `Game` and `ImGui` receivers, player session versus
  controlled critter, location versus instantiated map, character state, and
  item ownership plus static/abstract variants. Reviewed Russian overlays and
  a global completeness assertion reject future undescribed entity contracts.
- A documentation-only `ValueFieldDoc` tag now assigns descriptions and exact
  provenance to value-layout fields, including aliases and strong types that
  have no local C++ member declaration. The parser rejects unknown types or
  fields, duplicates, and empty comments without changing the runtime
  compatibility hash. All 62 fields across text formatting, identity, map and
  screen geometry, text-pack keys, color, time, and gamepad state have reviewed
  English source descriptions and Russian overlays; a global assertion rejects
  future missing value-field prose.
- All 54 exported enum types now explain their role, including render and input
  categories, movement and entity-event state, critter and item semantics, the
  Dear ImGui binding groups, and seven generated entity-property identifier
  sets. Generated `*Property` enum prose is derived during codegen without
  inventing source provenance; all type descriptions have reviewed Russian
  overlays, and a global assertion rejects future undescribed enum types.
- All 140 generated entity-property enum values now carry meaning: 133 reuse
  the exact description and source location of their owning `ExportProperty`,
  while each `None` value defines the no-selection sentinel. Documentation is
  attached after compatibility hashing. Exact-source translation memory reuses
  the 133 reviewed property translations only when source text and normalized
  SHA-256 both match and all donors agree; ambiguous reuse remains missing and
  fails complete enforcement. Focused regressions protect provenance, complete
  generated-value coverage, unambiguous reuse, and runtime-hash independence.
- A documentation-only `EnumValueDoc` tag now gives source-owned enum values
  exact prose and provenance while remaining outside the runtime compatibility
  hash. The parser rejects unknown enums or values, duplicates, and empty
  comments; focused tests also prove that removing documentation tags does not
  change the hash.
- Source tags and reviewed Russian overlays now cover 159 ordinary enum values:
  event outcomes and priorities, movement states, mouse buttons, render
  primitives, transparent-egg slots, critter/item/visibility semantics,
  multihex generation, map draw ordering, egg appearance, effects, and font
  flags.
- All 302 values across the 23 `ImGui_*` enum families now have source-backed
  descriptions and reviewed Russian overlays. The resolver maps 234 values to
  exact lines in the pinned Dear ImGui header, including semantic `ImGuiStyle`
  field comments for all 19 style variables; 68 zero, composite, corrected, or
  otherwise independently undocumented values use explicit Engine-owned
  `EnumValueDoc` fallbacks. Missing or malformed aliases and undocumented new
  values fail codegen, while all imported documentation remains outside the
  runtime compatibility hash.
- All 40 `EngineInfoMessage` values now document their conventional login,
  connection, loading, moderation, or runtime status role without claiming
  automatic Engine dispatch. The transport carries the enum and `extraText` as
  opaque payload; embedding projects own text, localization, and send policy.
  All values have explicit source tags and reviewed Russian overlays.
- All 105 `KeyCode` values now have descriptions and reviewed Russian overlays.
  The generator resolves 103 physical values to exact `SDL_SCANCODE_*` mapping
  lines in `MakeInputKeyMap()` and requires explicit tags for the no-key sentinel
  and synthetic UTF-8 `Text` event. Missing mappings fail codegen, and the API
  model now has zero undescribed symbols.
- Mapper event comments changed a declared screenshot source input, so the
  screenshot provenance catalog was regenerated before downstream AI delivery.
  The aggregate contract diff passes in explicit eighteen-domain bootstrap
  mode because the fetched `origin/master` does not yet contain generated
  contract models; it reports zero changes or required dispositions rather
  than claiming an unavailable cross-revision comparison.

### Inventory-pinned Native API stability review

- `Source/Common/Common.h` now owns a documentation-only
  `scope:native-codegen` contract that classifies the complete current inventory
  as revision-bound `experimental` since `2022.1.0.wip`. The declaration pins
  all 2,472 stable IDs by both count and SHA-256; additions, removals, or ID
  changes fail generation until an owner reviews and updates both values.
- An exact source declaration keeps the development-only
  `Game.BreakIntoDebugger` helper `internal`. The resulting model contains 2,471
  `experimental` symbols, one `internal` symbol, zero unclassified defaults,
  and no broad `stable` promise.
- The parser rejects missing, malformed, or stale scope pins and multiple scope
  declarations. Focused tests cover stale count and digest failures, exact scope
  overrides, family and deprecation contracts, and compatibility-hash
  invariance. EN/RU references render the scope once at model level and identify
  scope provenance per symbol without duplicating its prose into every row.

### External publication audit

- An authenticated GitHub audit on 2026-08-02 reverified all four registered
  `cvet` example repositories on private `main` branches. The observed heads
  remain `9946ca42c332a294f8fedd2732e7850a01c1ec27` for the source-staged project
  template, `97d232431488125b370be352fdcf28f66e6cbf4f` for minimal multiplayer,
  `011dab0d07eef6387609821206b8ee534ec51c3f` for the content showcase, and
  `97823816ab333a62aced43edd4daafa19c5fee22` for the native-extension sample.
  The latter three still contain explicit reservation commits; no passing
  required checks or public releases were observed. The registry records this
  as private staging, not publication evidence.
- Authenticated GitHub Pages API evidence reports status `built`, build type
  `legacy`, source `master:/`, CNAME `fonline.ru`, and enforced HTTPS. The
  latest Pages deployment for remote commit
  `fee50fb636b5bd1e30509aded929df1fc0e95db5` succeeded; its legacy `validate`
  workflow failed in code-coverage, macOS client, and retired Editor lanes and
  does not contain the documentation artifact/browser jobs pending on the
  local branch.
- Live HTTP checks return 200 for `https://fonline.ru/` and the legacy
  `https://fonline.ru/Docs/` index. The apex resolves to all four GitHub Pages
  IPv4 addresses and `www.fonline.ru` aliases it, while the GitHub ownership
  challenge TXT record is not observed. Representative current locale routes
  under `/Docs/en/` and `/Docs/ru/` return 404 because the documentation branch
  and its generated route corpus have not landed. The source setting and DNS
  routing are therefore confirmed, but the landed artifact, ownership
  verification, migrated routes, and production accessibility are not.

### Residual production work

- Promote individual groups to `stable` only after supported release lines and
  owner policy exist; the completed `experimental` review must not be described
  as a cross-revision compatibility guarantee.
- Land the documentation branch on the confirmed legacy `master:/` Pages
  source, obtain green documentation artifact/browser jobs, inspect the
  deployed routes, and establish or confirm the GitHub domain-ownership TXT
  record without changing the existing `fonline.ru` route.
- Complete human native-speaker, 200 percent zoom, and representative screen-
  reader review; run the standalone task set against at least two independent
  model families.
- Restage and validate the private example repositories at an exact published
  Engine revision before owner-authorized visibility, immutable tags, and
  artifacts.

## 2026-08-03 - native-extension sample source-ready slice

Scope:

- `Examples/NativeExtensionSample/`
- `Examples/PublicRepositories.json`
- `BuildTools/buildtools.py`
- `.github/workflows/validate.yml`
- the public-example and support-matrix generators, tests, and EN/RU guides

Results:

- Added an engine-owned standalone project that demonstrates the complete
  server-native path without Last Frontier, TLA, external SDKs, credentials, or
  distributable assets. A project library is routed through
  `AddProjectLibraries(ROLES SERVER ...)`, while the registered extension owns
  one `ServerEngine.UserData` aggregate, implements `ServerInitHook`, and
  exports `Game.NativeExtensionValue()` to server AngelScript.
- Added a focused C++ executable test plus a timeout-bounded server smoke. The
  source-staging validator preserves the implementation and test, applies the
  shared governance overlay, keeps empty asset provenance explicit, and rejects
  publication placeholders or missing required files.
- Added `win64-native-extension-smoke` and
  `linux-native-extension-smoke` to the BuildTools validation registry, required
  workflow, support model, and generated EN/RU support reference.
- The first clean Windows run correctly rejected an AngelScript module that
  became empty on the client preprocessing side. Adding the common
  `GetSampleName()` function made the module valid on both baker passes. The
  repeated `win64-native-extension-smoke` run on Engine
  `fac978a67d1e601eb77389e8dc562d7e511705a0` passed codegen, compile/link,
  resource baking, `native_extension_core_test_passed`, hook initialization,
  the script-visible value `42`, requested shutdown, and complete server
  teardown.
- The generated portfolio now reports four private repositories, three
  source-ready sources, one planned source, one remotely staged candidate, and
  zero published repositories. `fonline-native-extension-sample` remains a
  reserved private remote; the local Windows result is not remote pinned/current
  evidence and does not authorize publication.
- The complete `test_docs_*.py` discovery passes 510 tests. Standalone
  validation accepts all 391 Markdown entries; snippets pass 293 normative,
  156 evidence, and 170 external-parser checks; physical localization remains
  195/195 and generated-description localization remains 4,917/4,917. AI
  evaluation passes 27 tasks and 63 retrieval checks at 100 percent with 0.886
  MRR.
- A production render with the repository pin Ruby `3.3.4`, Bundler `2.5.11`,
  `github-pages` `232`, and Jekyll `3.10.0` completes successfully. Rendered
  artifact validation accepts 579 routes, 44 static endpoints, and 80,241 local
  references. The first complete Chromium run had one non-reproduced 15-second
  navigation timeout on `/README.ru.html`; an immediate complete rerun passes
  all 1,158 desktop/mobile page checks, 11 interaction profiles, and 13
  screenshots with zero errors or unresolved accessibility findings.

Remaining gates:

- Run and retain the Linux lane, materialize the candidate at a clean published
  Engine revision, stage it privately, and obtain both pinned/current remote
  results.
- Configure branch/security policy and produce the immutable tag/artifact before
  any owner-authorized visibility change.

## 2026-08-03 - content-showcase source-ready slice

Scope:

- `Examples/ContentShowcase/`
- `Examples/PublicRepositories.json`
- `BuildTools/buildtools.py`, `_config.yml`, and `.github/workflows/validate.yml`
- the public-example, support-matrix, localization, site, and AI-delivery models
- mirrored EN/RU example and public-repository guidance

Results:

- Added a standalone Engine-owned content project with no Last Frontier or TLA
  dependency. It owns 13 deterministic project-original CC0 source assets and
  demonstrates TGA, FOFRM animation with `NextX`/`NextY`, FOFX, SPARK, WAV,
  prototypes, a map, client/server scripts, baking, native runtime smoke, and a
  deterministic client capture contract.
- Added byte-level provenance, source-size and runtime budgets, clean-staging
  rules, timeout-bounded client/server smoke, region-based pixel checks, and 12
  warmed Direct3D 11 capture samples. The accepted 1280 x 800 capture has
  1,024,000 visible pixels, 54,226 bright pixels, 136 sampled colors, and
  non-empty header, runtime, gallery, and footer regions.
- Added isolated `win64-showcase-smoke`, `linux-showcase-smoke`,
  `web-showcase-build`, `web-showcase-package`, and `web-showcase-runtime`
  validation targets. The required workflow uses the complete runtime route so
  it does not rebuild the same Web client in three jobs. The Windows lane
  passes source validation, client/server/mapper/baker compilation, baking,
  content tests, client/server smoke, and clean shutdown. The Emscripten 6.0.3
  compile lane passes authored-source validation plus client
  JavaScript/WebAssembly checks.
- The complete Web route force-bakes through a native Windows/Linux host,
  builds the Web client, creates raw and ZIP packages, and verifies the exact
  six-file inventory, WebAssembly magic, nontrivial `Resources.data`, LZ4
  loader, generated shell/server, archive parity, and CRC. The isolated local
  package contains 583,552 bytes of compressed resource data. An implementation
  defect found during this work made package helpers ignore an explicit
  `FO_EMSDK` in favor of the workspace default; BuildTools now gives the
  explicit reviewed SDK precedence and a behavioral regression test pins it.
- The isolated runtime then starts the native server and packaged HTTP server,
  opens pinned Chromium, requires successful shell/JavaScript/WebAssembly/data
  responses plus client/server readiness markers, obtains a real 1280 x 800
  WebGL 2 drawing buffer, rejects console/page/network/Engine failures, and
  validates the compositor screenshot with the native region contract. The
  retained WebGL capture records Chromium `151.0.7922.34`, `WebKit WebGL`,
  1,024,000 visible pixels, 54,290 bright pixels, 131 sampled colors, package
  and image SHA-256 values, and zero runtime errors. A Chromium
  `net::ERR_ABORTED` is tolerated only when the same required resource already
  returned HTTP 200 and the complete readiness contract passes; all other
  failed requests remain fatal.
- The portfolio now reports four private repositories, four source-ready local
  sources, and zero published repositories. `fonline-content-showcase` remains
  a reserved private remote; this local slice does not authorize staging,
  visibility, tags, releases, or public support claims.
- Complete `test_docs_*.py` discovery passes 513 tests. Standalone validation
  accepts all 393 Markdown entries; snippets pass 302 normative, 157 evidence,
  and 179 external-parser checks. Physical localization is 197/197 and
  generated-description localization is 4,917/4,917. AI evaluation passes 27
  tasks and 63 retrieval checks at 100 percent with 0.897 MRR.
- Production Jekyll testing exposed a local recursive junction at
  `Examples/ContentShowcase/Engine`. `_config.yml` now excludes build and Engine
  paths for every standalone example, with a focused regression test. A clean
  GitHub Pages-compatible Jekyll 3.10.0 build then completes successfully.
  Rendered artifact validation accepts 583 routes, 44 static endpoints, and
  81,919 local references. Pinned Chromium validation passes all 1,166
  desktop/mobile page checks, 13 interaction profiles, and 15 screenshots with
  zero errors or unresolved automated accessibility findings. The complete
  Russian Content Showcase entrypoint screenshot was also inspected directly:
  both Direct3D 11 and WebGL 2 images, navigation, table of contents, commands,
  tables, and footer render without overlap or clipping.

Remaining gates:

- Run and retain `linux-showcase-smoke`, capture and validate Linux OpenGL
  pixels, and retain the exact Engine revision as backend evidence.
- Materialize the candidate at a clean published Engine revision, stage it in
  the reserved private remote, and obtain pinned/current, security, immutable
  tag, artifact, and owner publication approval evidence.
- Complete human accessibility and native-speaker review as part of the wider
  production documentation plan.

## 2026-08-03 - internal documentation isolation and `_meta` migration

Scope:

- `Docs/_meta/` plus the four historical repository pointer files
- `_config.yml` and `Docs/documentation-manifest.json`
- `BuildTools/docs_ai_delivery.py`, `BuildTools/docs_site_artifact.py`, and
  `BuildTools/docs_validate.py`
- public EN/RU maintainer, publication, metadata, updater, example, home, and
  ADR routes that refer to source-only evidence

Results:

- Moved the historical expansion plan, backlog, research template, and
  verification report to canonical source-only paths under `Docs/_meta/`.
  Their former paths remain short repository pointers but are excluded from
  GitHub Pages together with the active production plan.
- Classified the generated external-project evidence model as internal.
  AI delivery now omits internal generated models, Jekyll excludes their raw
  and rendered paths, and public pages use GitHub source URLs instead of
  promising same-domain routes.
- Added a post-build fail-closed check for every raw Markdown/JSON, index, and
  rendered HTML variant of an internal path. Running it against the previous
  artifact correctly rejected 13 published internal variants. A clean Jekyll
  rebuild contains zero internal variants and passes 583 routes, 43 public
  static endpoints, and 81,889 local references.
- Complete `test_docs_*.py` discovery passes 515 tests. Standalone validation
  accepts all 397 Markdown entries; snippets pass 302 normative, 157 evidence,
  and 179 external-parser checks. Physical localization remains 197/197 and
  generated-description localization remains 4,917/4,917.
- AI evaluation passes 27 tasks and 63 retrieval checks at 100 percent with
  0.905 MRR. Pinned Chromium validation passes all 1,166 desktop/mobile page
  checks, 13 interaction profiles, and 15 screenshots with zero errors.

Remaining gates:

- Retain the first landed GitHub Pages artifact and production-source proof,
  then complete manual assistive-technology and native-speaker review.
- Run and retain the Linux OpenGL Content Showcase lane.
- Stage and approve the private public-example candidates only after their
  exact Engine revision is available from the published Engine remote.

Post-validation repository reconciliation:

- A closing fetch found seven incoming Last Frontier commits in
  `48cf5f5dd99e5dbb9fca4cbb8d428f84f1b67da5..937f704512f54abc2cc86b4d8f957aab48f2db1b`.
  The project merged them at
  `50b8cb4e9ec706887708640e4474b9f8281097d8`; Engine remained at
  `fac978a67d1e601eb77389e8dc562d7e511705a0` with no incoming Engine range.
- The seven commits changed one of 76 pinned Last Frontier evidence paths:
  `Docs/MapAuthoring.md`. Its continuous encounter-exit-band rule remains
  project-owned level-design policy and does not change the reusable Engine map
  contract. Incoming `Economy.md` and the encounter plan already own the trader
  stock and generated-map behavior. Project `GuiSystem.md` was corrected to
  describe preview fitting through `ViewRect` rather than shadow-inclusive
  `DrawRect`; temporary quest diagnostics require no contract documentation.
- The external-evidence snapshot now pins Last Frontier `50b8cb4e`; exact
  source hashes and the internal generated inventory were regenerated before
  the final documentation checks.

## 2026-08-04 - 200-percent documentation reflow evidence

Scope:

- `BuildTools/docs-browser/audit.mjs` and its focused policy tests
- `Docs/documentation-manifest.json` and the standalone manifest validator
- the mirrored EN/RU site-publication and translation procedures
- the production plan, backlog, and active Last Frontier documentation plan

Results:

- Added a manifest-owned `zoom-200` profile that audits every route at a
  640 x 512 CSS viewport with device scale factor 2, corresponding to a
  1280 x 1024 physical capture at 200 percent browser zoom. It keeps a desktop
  user agent while requiring the compact navigation/reflow contract.
- Added a Russian interaction scenario for exact locale, DPR, CSS/physical
  viewport, heading visibility, drawer focus transfer/restoration, complete
  drawer closure, page-level overflow, and the retained
  `zoom-200-russian-documentation.png` screenshot. Diagnostic `--route-limit`
  now limits only the route matrix while selecting interaction fixtures from
  the complete catalog.
- Direct review of the first screenshot found the navigation drawer still
  visible during its closing CSS transition. The harness now waits until the
  sidebar is fully outside the viewport. The corrected Russian screenshot and
  the zoom-profile architecture diagram were inspected at full width and are
  readable without overlap, clipping, blank media, or an open drawer.
- The GitHub Pages-compatible Jekyll build passes 583 routes, 43 public static
  endpoints, and 81,889 local references. Node `24.16.0`, Playwright `1.62.0`,
  Chromium `151.0.7922.34`, and axe-core `4.12.1` pass all 1,749 route/profile
  checks, 14 interaction profiles, and 17 screenshots with zero browser
  errors, axe violations, failed contrast fallbacks, or unresolved findings.
- Complete `test_docs_*.py` discovery passes 515 tests. Standalone validation
  accepts all 397 Markdown entries; snippets remain 302/302 normative, 157
  evidence, and 179 external-parser checks. AI evaluation remains 27 tasks and
  63 retrieval checks at 100 percent with 0.905 MRR.
- The complete discovery also exposed CRLF drift in three text assets owned by
  Content Showcase. Their LF-normalized bytes matched the existing provenance;
  the source files and exact hashes were reconciled without changing shader,
  particle, or animation behavior, and isolated candidate materialization now
  passes on the platform-independent byte contract.

Remaining gates:

- Repeat 200-percent zoom on the landed artifact and production domain, and
  complete native-speaker plus representative screen-reader review. The local
  deterministic profile is strong reflow evidence, not a claim about every
  browser, operating-system magnifier, font override, or production render.
- Retain the first landed documentation artifact and production-source proof.
- Complete Linux OpenGL Content Showcase evidence and private-example
  pinned/current, security, immutable-release, and owner-publication gates.

## 2026-08-04 - isolated two-family AI documentation baseline

Scope:

- `Docs/ai-evaluation.json` and the deterministic retrieval report
- `BuildTools/docs_ai_model_eval.py` plus its focused tests
- `BuildTools/docs_ai_model_review.py` plus its focused tests
- the canonical EN/RU AI-evaluation guide, production plan, and backlog
- compact independent reviews under `Docs/_meta/ai-evaluation/`

Pinned evidence:

- Engine HEAD was `fac978a67d1e601eb77389e8dc562d7e511705a0`; the embedding
  Last Frontier checkout was `50b8cb4e9ec706887708640e4474b9f8281097d8` but was not
  supplied to either model. The evaluation source SHA-256 was
  `61f393127f602eb58b11005ad129952845f3918d031250b60fdb27b1a68d429e`.
- Both runs used the same input hashes, six candidates, 24,000-byte per-document
  and 60,000-byte aggregate limits, 32,768 context tokens, 6,000 predicted
  tokens, temperature 0, seed 0, and a 300-second streaming deadline. The
  harness SHA-256 recorded by each raw report was
  `b5c59c52f0da899d7d9614d00651722f2a22dec93d5708b55e9609d6ea2c01cc`.
- The Qwen 3.5 run used Ollama 0.32.5 model
  `huihui_ai/qwen3.5-abliterated:9b`, digest
  `92a443adb124f5e805bbdee23fdb38fcd22a7bf00a1016b53f764e741369c600`.
  Its raw report SHA-256 is
  `f1a521136e4347b12f0cc5703a189a5a3ac7a624dc07b42396aebe73282c9314`.
- The GPT-OSS run used Ollama 0.32.5 model `gpt-oss:20b`, digest
  `17052f91a42e97930aa6e28a6c6c06a983e6a58dbb00434885a0cf5313e376f7`.
  Its raw report SHA-256 is
  `6f155237da922318b832609f3e73a1ea3feac0773bfb1c39c485cfcb9d5fcc63`.

Results:

- The deterministic gate passes 27 tasks and all 65 retrieval checks at 100
  percent, with 100-percent top-three retrieval and 0.908 MRR.
- Both model families completed 27/27 tasks, selected the primary owner for
  27/27 tasks, received observable source evidence for all 84 hidden answer
  criteria, and triggered zero automatic project-term assumptions. The input
  contract therefore isolates answer quality from missing-rubric evidence.
- Independent semantic review by `OpenAI Codex gpt-5.6-sol independent
  semantic reviewer with maintainer spot-check` accepted 6/27 Qwen tasks
  (22.2 percent) and 5/27 GPT-OSS tasks (18.5 percent). Every source-selection
  decision passed. The compact reviews retain answer-check, forbidden-
  assumption, grounding, task-success, reviewer, exact-run-hash, and model
  metadata without committing the large raw responses.
- Qwen most often failed by omitting one or more dimensions of a coupled task.
  GPT-OSS had the same completeness problem and also invented plausible build
  targets, command forms, settings, or support claims. One malformed GPT-OSS
  response was retained verbatim, rejected by the schema, and corrected through
  the bounded retry path rather than silently accepted. A two-task Qwen
  `--self-review` smoke did not materially close the omissions and is not used
  as independent scoring evidence.

Disposition and limitations:

- The requirement to execute and review two materially different model
  families is complete. The production exit gate is not: both results are far
  below the unchanged 90-percent per-family task-success target, and grounding
  failures prevent release acceptance independently of the aggregate score.
- Retrieval-weight tuning is not the current remedy because both families
  selected every owner. The next pass must add concise, source-backed decision
  and acceptance checklists to affected owning pages, make exact command and
  support boundaries easier to cite, preserve coupled workflows where they are
  genuinely coupled, and rerun two families after those corpus changes.
- Raw reports remain ignored under `Workspace/ai-evaluation/`; compact reviews
  are versioned internal evidence. Plain review `--check` works without raw
  files, while `--require-run` verifies raw availability, SHA-256, model,
  provider, parameters, input hashes, source ref, and completion time. The
  baseline remains reproducible by its recorded hashes; later guide/manifest
  edits deliberately create a new corpus revision and do not retroactively
  rewrite these results.
- Remaining non-AI gates are unchanged: landed Pages/CI and production-domain
  evidence, Linux OpenGL Content Showcase evidence, private-example release
  controls and publication approval, and native-speaker plus representative
  assistive-technology review.

Validation:

- Complete `test_docs_*.py` discovery passes 530 tests. Standalone validation
  accepts all 397 manifest-owned Markdown entries. Translation parity is
  197/197; AI delivery contains 386 public documents; the deterministic
  evaluation remains 27/65 at 100 percent and 0.908 MRR.
- Snippet validation passes 304/304 normative blocks, 157 evidence blocks, and
  181 external-parser checks. Both compact reviews pass `--check --require-run`
  against their retained raw SHA-256 values.
- GitHub Pages-compatible Jekyll 3.10.0 on the pinned Ruby 3.3.4 and Bundler
  2.5.11 builds successfully. Rendered-artifact validation passes 583 routes,
  43 static endpoints, and 81,889 local references. Node 24.16.0, Playwright
  1.62.0, Chromium 151.0.7922.34, and axe-core 4.12.1 pass all 1,749
  desktop/mobile/200-percent route checks, 14 interaction profiles, and 17
  screenshots with no browser audit failure. The long evaluator commands and
  exact model digests therefore remain contained at mobile and 200-percent
  reflow rather than creating page-level overflow.

## 2026-08-04 - first AI answer remediation smoke

Scope and changes:

- Added bilingual, source-backed decision/checklist summaries to twenty-one
  owning pages and generated entry points implicated by the reviewed baselines.
  The summaries cover architecture and project ownership, AiControl, native
  dependencies, AngelScript, map/particle/Mapper workflows, tests, debugging,
  profiling, contracts, support, packaging, secrets, rollout, recovery,
  BuildTools, public-contract selection, Essentials, and package/example
  evidence.
- Reworded composite benchmark questions where the review rubric previously
  expected ABI/allocator, evidence-layer, or documentation-routing detail that
  the user-facing question did not name. Task count and hidden answer-check
  count remain 27 and 84.
- The local Ollama harness now repeats only verbatim decision and
  query-relevant sections from retrieved candidates in a bounded quick-evidence
  block. The rubric remains absent from the prompt; exact prompt and input hashes
  remain retained. Eighteen focused runner/reviewer tests cover the path.

Diagnostic evidence:

- All five-task smoke variants completed with 100-percent owner selection and
  100-percent hidden-criterion observability. Same-corpus `--self-review` and
  owner-only context did not materially improve composite coverage and remain
  diagnostic options, not the retained answer profile.
- Bounded quick evidence materially improved maintainer-reviewed completeness,
  most clearly for the complete AngelScript checklist and GPT-OSS AiControl and
  viewer evidence layers. It did not eliminate every ownership, ABI, policy, or
  grounding failure, so no smoke result is recorded as production acceptance.
- `qwen3:14b-q4_K_M` required 846.1 seconds for five tasks on this host and
  introduced grounding regressions despite the larger model. It is rejected as
  the routine qualification profile. Historical Qwen 3.5 9B and GPT-OSS 20B
  compact baselines remain immutable until a full rerun is independently
  reviewed.

Disposition:

- Keep the bilingual owner summaries and quick-evidence presentation because
  they improve human scanning and both observed model families without keyword
  stuffing or hidden-rubric leakage.
- Keep the 90-percent per-family target unchanged. The next evidence step is a
  full two-family run on the finalized corpus, followed by compact independent
  review and a focused second remediation batch for any remaining failures.
- Focused generator, localization, legacy-route, AI runner/reviewer, and owning
  documentation tests pass. Full discovery, rendered site, and browser evidence
  must be refreshed after the full rerun and final report changes.

## 2026-08-04 - final two-family AI qualification

Scope and evaluated revision:

- Finalized the 27-task standalone Engine evaluation at Engine
  `fac978a67d1e601eb77389e8dc562d7e511705a0`; the embedding Last Frontier
  checkout was `50b8cb4e9ec706887708640e4474b9f8281097d8` and was not supplied to
  either model.
- The source contains 65 retrieval checks and 92 answer checks. Direct recount
  of `Docs/ai-evaluation.json` and both raw runs agrees on 92; the complete test
  suite pins that total to prevent report drift.
- Both final runs used six candidates, 24,000-byte per-document and 60,000-byte
  aggregate limits, 32,768 context tokens, 6,000 predicted tokens, temperature
  0, seed 0, and a 300-second streaming deadline. Their shared harness SHA-256
  is `5fdd971f46898f789817c1f87531af862ec4ccd3b38e30f6a9ce987915f3aa09`.
- Shared input SHA-256 values are
  `dd3e5f83d17b2b5aafcf787f374446929f8d412db68ed6a193aaed0479d2e310`
  for `Docs/ai-evaluation.json`,
  `b264cb916ab4690829502de18a9ca7328e252fd3adc587dec3362e1403bd4499`
  for `Docs/generated/ai-evaluation-report.json`,
  `f14577a60f6d7e074bdbe24b890b927de8ed9797ddcc5f6e621353cc19865d30`
  for `docs-manifest.json`, and
  `471a86233febdf79dd931d16ef077d854839266852682f500a2a0ee533c4407f`
  for `llms.txt`. Reporting-only guide, plan, route, search, and delivery changes
  were regenerated after qualification; they do not rewrite the evaluated
  corpus or either raw run's pinned input record.

Retained model evidence:

- Ollama 0.32.5 Qwen 3.5 model
  `huihui_ai/qwen3.5-abliterated:9b`, digest
  `92a443adb124f5e805bbdee23fdb38fcd22a7bf00a1016b53f764e741369c600`,
  produced
  `Workspace/ai-evaluation/2026-08-04-qwen3.5-9b-v12-final.json` with
  SHA-256
  `30acb42ee573e85176cbf3f58cbd34d922b21b4742b984713fa04f58e07ff466`.
  Its compact review is
  `Docs/_meta/ai-evaluation/2026-08-04-qwen3.5-9b-v12-final.review.json`
  with SHA-256
  `c579f6d400f575b191c2a8f3c2ffad97848527046010465ba16a04f09fcc814d`.
- Ollama 0.32.5 GPT-OSS model `gpt-oss:20b`, digest
  `17052f91a42e97930aa6e28a6c6c06a983e6a58dbb00434885a0cf5313e376f7`,
  produced
  `Workspace/ai-evaluation/2026-08-04-gpt-oss-20b-v13-final.json` with
  SHA-256
  `49b2706895a13c477e04669ea89df0dd82a3407ba3ab4af49c3d8de985611291`.
  Its compact review is
  `Docs/_meta/ai-evaluation/2026-08-04-gpt-oss-20b-v13-final.review.json`
  with SHA-256
  `e58b5cad481fa0d9854d8f561fc30d7dce949945b3bb8c405fd9a8137983d8ec`.
- The harness records one invalid-response retry with thinking disabled and one
  semantic-completion repair limited to missing required evidence tokens, an
  invalid selected document ID, a missing primary owner, or zero valid
  citations. It retains every response attempt and redacts hidden reasoning.

Independent review results:

- Both families completed 27/27 tasks with zero transport errors, selected the
  primary owner for 27/27 tasks, and received observable source evidence for all
  92 answer checks. The deterministic gate remains 65/65 at 100 percent with
  0.908 MRR.
- `OpenAI Codex gpt-5.6-sol independent semantic reviewer with maintainer
  spot-check` accepted Qwen 27/27 (100 percent) and GPT-OSS 25/27 (92.6
  percent). Both exceed the unchanged 90-percent per-family production target.
- GPT-OSS failed `inspect-animation-particle-assets` because it prescribed
  `Game.SaveMapperScreenshot` and `RequestMapperWindowScreenshot` where the
  supplied Mapper evidence boundary allowed only placement, depth, and
  composition. It failed `migration-complete-engine-range` because it repeated
  adoption-stage names without an actionable persisted-state and validation
  procedure. The compact review preserves the exact failed answer check,
  forbidden assumption, grounding decision, and reviewer note.
- Neither family produced an unsupported safety, migration, compatibility, or
  release claim. The migration failure is an omission rather than false
  operational advice; the viewer failure is outside those critical domains.
  The AI answer-quality exit gate is therefore complete without lowering its
  threshold or editing correct owner documentation to accommodate one model.

Validation:

- Focused AI runner/reviewer tests pass 22/22. Both compact reviews pass
  `--check --require-run` against their retained raw reports. Complete
  `test_docs_*.py` discovery passes 537/537 after pinning the exact 92-check
  total and making release-safety prose assertions insensitive to Markdown line
  wrapping without weakening their source checks.
- Freshness and standalone checks pass: 304/304 normative snippets, 157
  evidence blocks, 181 external-parser checks, 197/197 current locale pairs,
  397 maintained Markdown entries, 386 public AI-delivery documents, and the
  27-task/65-query deterministic evaluation at 100 percent and 0.908 MRR.
- A fresh production build uses Ruby 3.3.4, Bundler 2.5.11,
  `github-pages` 232, and Jekyll 3.10.0. Rendered-artifact validation passes 583
  routes, 43 static endpoints, and 81,911 local references.
- Node 24.16.0, Playwright 1.62.0, Chromium 151.0.7922.34, and axe-core 4.12.1
  pass 1,749 desktop/mobile/200-percent rendered page checks, 14 interaction
  profiles, and 17 screenshots. The long model evidence paths and SHA-256 values
  do not introduce layout overflow in either locale.

Disposition:

- Phase 8 answer remediation and the two-family quality exit gate are complete.
  Preserve the current owner summaries, exact evidence boundaries, objective
  repair limits, hidden rubric, raw hashes, compact reviews, and 90-percent
  target as the maintained regression contract.
- Remaining production gates are external or human: clean pinned/current
  qualification, release controls, immutable artifacts, and owner-authorized
  visibility for the four private examples; landed Pages/domain CI evidence;
  and native-speaker, production 200-percent zoom, and representative
  assistive-technology review.

## 2026-08-06 - upstream baking-target reconciliation

Scope and source revisions:

- Reconciled the documentation production branch at
  `e72873767b6adc1137f6c0d16789498b89652247` with Engine `origin/master`
  through `f83305017369020e8e6888e76acf824bb24bf202`. The incoming Engine range
  contains the project baking-target helper and the macOS CPU-load span
  lifetime correction.
- Audited the embedding Last Frontier repository through
  `8a39ad7f8c4d9cf5ee114a44caaa988f2dab5b74`. Its new
  `BakePublicResources` target, `PublicGame` subconfig, and CI route are
  integration evidence; their project-specific policy remains in that
  repository.
- TLA remains pinned at
  `b603d8fdbc2b2f89f233b2a1938686ead9d8d480`; this reconciliation introduced
  no new TLA-derived normative claim.

Contract and documentation changes:

- Promoted `AddBakingTarget` into the early-loaded BuildTools helper layer and
  registered it as the seventh public CMake helper. This intermediate placement
  was superseded by the later source reconciliation, which keeps the public
  helper in `ScriptsAndBaking.cmake` and documents its post-stage availability.
- Documented the exact target signature, `NONE` subconfig default, force mode,
  build-hash update, and required call order after `SetupScriptsAndBaking()` in
  canonical English and Russian baking, embedding, and pipeline owners.
- Added the stable-locator Russian description overlay and source ownership to
  the machine-readable project-interface and documentation manifests. The
  generated description catalog remains complete at 4,918/4,918 values.
- Retained upstream's `const_span` construction over the owned macOS CPU-load
  buffer. No reusable documentation contract changes for that internal
  implementation correction.

Validation:

- Complete `BuildTools/tests` discovery passes 572/572. The focused CMake,
  baking, build-foundation, localization, translation, and documentation
  quality set passes 39/39.
- Generated snippet validation passes 307/307 normative snippets, 157 evidence
  blocks, and 182 external-parser checks. All 197 locale pairs and all 4,918
  generated-description overlays are current.
- Deterministic AI evaluation passes all 65 retrieval checks across 27 tasks at
  100 percent success and 0.908 MRR. Generated CMake, site, inventory, and AI
  delivery artifacts were refreshed in dependency order.

CI reconciliation:

- The first current-base workflow exposed a coverage-only compile failure in
  the portable AngelScript wrapper path. The test destructor adapter no longer
  adds a `noexcept` function type that the object-first generic wrapper does not
  accept; the replacement workflow passes the code-coverage job.
- The replacement workflow then exposed concurrent `BakeResources` and
  `ForceBakeResources` writers in the Minimal Multiplayer package lane. Both
  tutorial smoke and package checks now share `ForceBakeResources`, so CMake
  schedules one baker before the two read-only consumers. Focused public-example
  and package tests pass 16/16 and pin the single-target dependency.

Disposition:

- The reusable helper contract is ready for CI and maintainer review. Merge and
  publication evidence remain attached to the Engine pull request rather than
  being inferred from this local reconciliation.
- Linux/OpenGL Content Showcase evidence is complete. Remaining example gates
  are clean pinned/current qualification, release controls, immutable
  artifacts, and owner-authorized visibility for the four private repositories.

## 2026-09-01 - shared resource index and native-call ABI reconciliation

Scope and source revisions:

- Reconciled the documentation branch at
  `6acbb385eaa88cfbfe5a6d435b808281bef6bc2a` with Engine `origin/master`
  through `e7a34e81cccc902eb68f531da0d3693f755c0af6`. The incoming range fixes
  one-byte AngelScript value arguments on native ABIs, removes quadratic baker
  input-directory deduplication, and introduces a shared point-lookup index over
  mounted resource packs.
- Audited the corresponding Last Frontier integration range through project
  `e5f5aa550c36f32f7092ee6bebb70fe0bb289c04`: it splits `CommonArt` into
  bounded resource packs, separates sound and music, adds a resource-pack audit,
  and pins this Engine revision. Those project-specific pack names and CI rules
  remain owned by the project documentation.
- The three conflicts were legacy documentation routes. Their redirect/stub
  role was preserved while the reusable contracts were reconciled in the
  canonical English/Russian owners; no legacy monolith was restored.

Contract and documentation changes:

- Configuration and data-source documentation now defines the immutable pack
  snapshot, live-directory fallback, mount-priority winner, explicit
  reindex/clean lifecycle, lock-free point reads, and the deliberate absence of
  a lower-priority fallback when the indexed owner cannot open its entry.
- Baking documentation records one mount per distinct `InputDirs` path, stable
  first-seen ordering, `DataSourceRef` borrowing, and linear deduplication.
- Scripting-runtime and third-party documentation records exact one-byte VM
  argument normalization for boolean values on x64 GCC/MSVC and ARM64, including
  the native-call regression test.
- Generated inventory, snippets, translation status, site/search data, AI
  evaluation, manifest, and standalone AI-delivery artifacts were regenerated
  in dependency order. All four affected Russian owner pages carry current
  source hashes.

Validation:

- `LF_UnitTests` builds cleanly and passes 583/583 test cases with 707,016
  assertions, including the new shared-index, pack-snapshot, baker-deduplication,
  and AngelScript boolean ABI coverage.
- Documentation checks pass 310/310 normative snippets, 159 evidence blocks,
  183 external-parser checks, 197/197 current locale pairs, 397 maintained
  Markdown entries, 386 public AI-delivery documents, and all 65 retrieval checks
  across 27 tasks at 100 percent success and 0.915 MRR.
- Contract-diff reconciliation against
  `6acbb385eaa88cfbfe5a6d435b808281bef6bc2a` reports zero changes across all
  18 tracked public-contract domains and therefore requires no disposition
  records.
- Full `test_docs_*.py` discovery reached 548 tests. The one repository defect
  it found, the missing shared-index anchor on the legacy configuration route,
  was corrected and its focused five-test owner suite passes. The remaining
  external-parser case cannot start locally because Windows has WSL enabled but
  no installed Linux distribution; the direct 183-parser validation is green,
  and the actual WSL/Linux invocation remains a required CI check.

Disposition:

- The reusable Engine contract and generated corpus are reconciled for review.
  Publish the Engine documentation branch before the embedding project branch
  so the root gitlink never points at an unavailable commit.
- Merge-readiness still depends on current remote CI, including the real Linux
  external-parser path and package/runtime acceptance; this local report does
  not substitute for those gates.
