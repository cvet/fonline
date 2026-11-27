# Project Lifecycle Documentation Plan

> Internal plan. It extends Phase 5 ("Extract reusable Last Frontier practices") and Priority 0/1 of the [production documentation program](../ProductionDocumentationPlan.md) with one coherent scope: how an embedding game project is created, set up on a workstation and in CI, and maintained over Engine updates and releases.

## Status

- State: validating
- Owner: documentation program
- Last updated: 2026-11-07
- Engine baseline: `8b32281d50548593b81912d24d7625a145c8f277` (`2026.1.112-dev`)
- Last Frontier baseline (Managed C#): `aee423f7d4`, Engine gitlink `8b32281d50`
- TLA baseline (AngelScript): `2d59e858b051e3fc1c152efcb19c0dcb2a3d0691`, Engine gitlink `6ea95ac28d733b396eec6e9b9d1bb167ea267b8c` (ancestor of the Engine baseline)

## Goal

A game team that has only a clone of `cvet/fonline` can:

1. prepare a workstation and create a new project repository with a correct submodule, ignore/attribute files, presets, `.fomain`, `VERSION` and first green configure/bake/smoke;
2. work in VS Code (or another IDE) with the Engine-supplied FOnline Tools extension and AngelScript debugger, including a project task graph that follows the extension's label grammar;
3. choose a script backend and set up its formatting, analysis, tests and CI;
4. maintain the project: routine quality gates, Engine updates, data migrations, versioning, releases and diagnosis of setup failures;
5. optionally run the project with AI coding agents using a project `AGENTS.md`, task plans and an isolated task workspace.

Success is measured by the page definition of done in the production plan, by runnable snippets against `Examples/MinimalProject` (and a future managed starter), and by an AI-evaluation question set for the new journeys.

## Constraints

- Engine documentation stands alone. Last Frontier and TLA are research and conformance inputs only; no normative statement may depend on their paths, task names, hosts, tools or policies. Cite them only through the external-project evidence model or pinned HTTPS links.
- A practice becomes an Engine recommendation only through the [best-practice promotion process](../ProductionDocumentationPlan.md#best-practice-promotion-process). The matrix below records the current gate status of each candidate.
- English is canonical. Complete EN/RU parity is enforced, so every new or changed page lands with its Russian counterpart, manifest entry and translation hashes in the same change.
- Reference content (CMake options, CLI, settings) stays generated from its owner. Task guides link to generated pages and never copy option lists.
- Engine code or tooling changes required by a page (see [Engine prerequisites](#engine-prerequisites)) follow Engine versioning, changelog and migration-note rules.
- No commits, pushes, publication or changes to embedding-project repositories under this plan without separate owner authority.

## Research summary

Three source audits were run on 2026-11-06 against the baselines above: Engine documentation coverage, Last Frontier setup/maintenance practices, and TLA setup/maintenance practices. Their conclusions are summarized here; re-derive any claim from source before writing the corresponding page.

### Coverage matrix

| Lifecycle topic | Current Engine owner | Depth | Main gap |
|---|---|---|---|
| Host prerequisites and toolchain | `how-to/build/index.md#prerequisites`, `BuildTools/README.md`, generated `reference/buildtools/commands.md` | partial | No per-OS workstation guide; `host-check`/`prepare-*-workspace` exist only in the BuildTools README and generated CLI; no version-pin table (VS toolset, CMake, .NET, clang-format 20); no Python 3 detection rule; no macOS route; nothing on Windows long paths or symlinks |
| New repository from zero | `how-to/build/embedding-project.md`, `project-configuration.md`, `generated-content.md`, `tutorials/first-project.md` | partial | `git submodule add`, pin choice, relative submodule URL, `.gitignore`, `.gitattributes`, LFS decision, presets and `VERSION` are not taught for a game; only the example-publisher section of `public-example-repositories.md` touches them |
| Script backend choice | `how-to/scripting/managed-csharp.md`, `explanation/scripting-runtime/index.md` | partial | No decision guide and no side-by-side setup checklist; no managed starter in `Examples/` |
| IDE / VS Code | `troubleshooting/debugging.md` (AngelScript adapter fields only) | none | `BuildTools/fonline-tools` is undocumented; no install route for either extension; no workspace task/launch/settings guidance; `Engine/.vscode/extensions.json` unmentioned |
| Formatting | `how-to/scripting/style-and-refactoring.md#formatter-contract` | partial | No project wiring (`FO_CLANG_FORMAT`, `.fos` nullable-syntax hazard, C# formatting, CI drift gate) |
| Engine update from the project side | `how-to/migration/engine-upgrade.md`, `how-to/release/versioning.md`, changelog | complete process, weak mechanics | No literal submodule commands, gitlink/index/HEAD comparison recipe, consumer-impact search, batching guidance or plan/record template; contract-diff paths are ambiguous from a project root |
| Project data migrations and compatibility | `style-and-refactoring.md`, `managed-csharp.md`, prototype format, generated `script-api/migrations.md` | scattered | No single project how-to (organization, old-save tests, retention and removal); no rule for who bumps the gameplay compatibility version |
| Project CI | `public-example-repositories.md#ci-lanes`, `security-and-secrets.md`, `packaging.md` | example-centric | No "CI for your game" guide: scoped checkout, LFS ordering, portable steps, caches/mirrors, job order, scope classification, artifacts, release preflight |
| Project tests | `contributing/testing/index.md`, `how-to/testing/gameplay-and-integration.md`, `tutorials/first-test.md` | partial | No project-oriented page on native `TESTS` sources, a script harness driven by a sub-config, filters, terminal markers and excluding tests from shipping packs |
| Static analysis for projects | `contributing/coding-contracts/*`, `managed-csharp.md` | partial, contributor-scoped | No project-facing summary of what is automatic, what a project adds, and baseline-with-reason discipline |
| Project docs and agent process | `embedding-project.md#documentation-composition` | mention | No guidance on project `AGENTS.md`, docs layout, task plans, `Workspace/<task-id>/` |
| Project-owned generators | `how-to/build/generated-content.md` | partial | Does not say which generators a project typically owns and how to gate them (`--check`) |
| Operations | `how-to/release/operations.md`, backup/recovery, updater | good for server | No log naming/rotation/retention page, no Docker coverage for `BuildTools/docker`, no release-channel guidance |
| Setup troubleshooting | scattered "Common failures" sections | mention | No `troubleshooting/setup.md` |

The external-project evidence model (`Docs/generated/external-project-evidence/`) has no concern for IDE/editor tooling, repository hygiene, project CI structure, project quality gates, project versioning or project documentation/agent process.

### Defects found during the audit

These are fixed first because new pages would otherwise build on wrong material:

1. All five example `CMakeLists.txt` call `SetOption(FO_MONO_SCRIPTING OFF)`; the declared option is `FO_MANAGED_SCRIPTING` (`BuildTools/cmake/ProjectInterface.json`). `SetOption` in `BuildTools/Init.cmake` silently accepts unknown names, so misspelled or retired options pass unnoticed. Fix the examples and decide on an Engine-side unknown-option diagnostic.
2. `tutorials/first-project.md` says BuildTools recreates `Workspace/validation-project` and that generated files stay under `Workspace/`, while `Examples/MinimalProject/validate.py` runs `cmake --preset <host>` and `--build --preset <host>-smoke` in place and the same page lists `Build/<host>/` paths. Re-verify by a clean run (the example includes `Engine/BuildTools/Init.cmake` relative to itself) and correct the tutorial or the script.
3. `Docs/en/index.md` says the build workflow page covers presets, but that page declines to describe presets.
4. `BuildTools/README.md` "Build environment variables" still refers to retired `./linux.sh`-style scripts.
5. The production plan's Phase 0 counts (30 concerns / 24 promoted) were stale against the generated evidence (32 / 25). The 195 and 197 locale-pair statements in `_meta/DocumentationBacklog.md` are dated entries of an append-only log, each superseding the previous one, not a defect.
6. `BuildTools/fonline-tools` commits both `out/` (tsc) and `dist/` (esbuild) outputs while `package.json` uses only `dist/`; there is no documented build, check or install route, and no test.

Embedding-project discrepancies observed during the audit (TLA debugger bind host `0.0.0.0` versus the loopback guidance in `troubleshooting/debugging.md`, stale TLA update-plan pin text, stale Last Frontier architecture notes) are reported to those projects; they are not Engine work.

## Practice promotion matrix

LF = Last Frontier, TLA = The Life After. "Both" satisfies promotion gate 4. Dispositions: **teach** (generic Engine recommendation), **optional** (documented as a conditional pattern with its trigger), **mechanism** (document the Engine contract only), **project** (mention only the extension point), **defer** (not mature enough).

| Practice | LF | TLA | Engine contract | Disposition |
|---|---|---|---|---|
| Engine as submodule, staged root `CMakeLists.txt` (`Init.cmake` → stages → `FinalizeProjectGeneration`) | yes | yes | yes | mechanism (already documented; add a complete annotated skeleton) |
| Relative submodule URL; scoped `git submodule update --init --recursive Engine` when other submodules exist | scoped init | relative URL | no | teach |
| `.gitignore` for outputs (`Build/`, `Baking/`, `Binaries/`, `Workspace/`, logs, session markers) and `.gitattributes` with eol/binary rules for every FOnline format | yes | yes | no (format list is Engine knowledge) | teach; consider an Engine-supplied attribute template |
| Single canonical `auto` configure preset plus explicit toolchain presets | yes | yes | presets are project-owned | teach the pattern, keep preset lists out of Engine docs |
| Verify a real Python 3 interpreter (`py -3` on Windows, `python3` elsewhere); UTF-8 for non-ASCII content | yes | yes | Engine wrappers already do this | teach |
| Windows long-path readiness | guard file + CI env | not observed | `buildtools.py` uses `core.longpaths` for its own clones | optional (Windows) |
| Git LFS for binary content with a content audit | yes (self-hosted store) | no LFS | none | optional; host choice is project policy |
| `.NET` SDK pin through `global.json` | yes | n/a | Managed backend requires .NET | teach in the managed setup checklist |
| VS Code task label grammar (`Group :: Item`, `[hidden]`, platform tags, `[sort:N]`, `Ctrl+Shift+1..9`) | yes | yes | yes (`BuildTools/fonline-tools`) | mechanism + teach |
| Task chain where every build/bake task depends on configure, `Prepare` = bake + build, `Launch` depends on `Prepare` | yes | yes | no | teach |
| Aggregate verification task in sequential order (generate → format → analyze → validate → test) | `Generate, Format, Validate All` | `Verify :: All` | no | teach the pattern; contents are project-owned |
| Extensions delivered as tracked symlinks `.vscode/extensions/<name>` → `Engine/BuildTools/<name>` | yes | yes | no install route exists | teach after the Engine install route is decided (requires `core.symlinks` and Windows symlink permission) |
| Workspace settings: `*.fo*` file associations, watcher/search excludes for output trees, `cmake.useCMakePresets` | yes | yes | no | teach |
| Launch profiles: native launch with `preLaunchTask`, AngelScript attach, compound launch + attach | native + web | native + `fos` attach | adapter fields documented | mechanism + example profile |
| Bundled pinned clang-format 20 shared through `FO_CLANG_FORMAT`; formatter wrapper for `.fos` nullable syntax; CI drift check | yes | yes | `format-source` honours `FO_CLANG_FORMAT` | teach |
| Warnings are failures; baselines carry written reasons, stale rows fail, gates are never lowered | yes | yes (ratchet) | Engine policy for itself | teach |
| Analyzer acceptance probes proving each gate fires | yes | no | no | optional |
| Generated, never hand-edited project `VERSION` feeding `Common.GameVersion` | git-history generator | patch bump generator | `Common.GameVersion` is project-owned | teach "generated and monotonic"; scheme is project policy |
| Manual release dispatch with version/tag preflight; dry run on other refs | yes | yes | no | teach |
| Portable CI steps as Python commands instead of shell glue; interpreter resolution step | `Tools/CiSteps` | `build.sh`/`toolset.sh` wrappers | Engine wrappers exist | teach the principle, not either tool |
| Docs-only change scope classification, workflow-syntax gate, content-validation aggregator | yes | no | no | optional (advanced CI) |
| Engine update plan: old/new SHAs of both repositories, full incoming range, changelog migrations oldest first, consumer search over project sources, reconfigure, force bake, full verification, explicit gitlink/index/HEAD comparison | yes | yes | process documented | teach the mechanics and a record template in `engine-upgrade.md` |
| Embedding project qualifies the incoming Engine range and upstreams reusable fixes | yes | yes | `embedding-project.md#validation-principle` | teach |
| Central migration module with per-domain parts; current-only gameplay code | `Migration*.cs` | `Migration.fos` | `///@ MigrationRule` | teach organization; rules stay generated reference |
| Identity manifest guard (no reuse of removed IDs/names) | `Tools/MigrationGuard` | no | no | optional concept; P2 tool disposition |
| Data-representation choice: setting vs FixedType `.foinfo` vs prototype property/component vs RefType vs ValueType | yes | partial | yes (entity/property model) | teach as an explanation decision table |
| Gameplay test harness driven by a sub-config and project settings; acceptance by terminal markers and exit code | yes | yes | runner contract documented | teach the project wiring |
| Exclude test scripts from shipping bakes (`Baking.IgnoreInputDirs`) | yes | verify | yes | teach after TLA verification |
| Test-quality static gate and runner meta self-test | yes | no | no | optional |
| Mutation and flake testing | experimental | no | no | defer |
| Process ownership: never kill by image name, never delete bake locks, never overlap a bake with its consumers | yes | yes | lock documented in `baking.md` | teach |
| Per-application log files `<exe>.log`, `.prev.log` rotation, client session marker | yes | yes | yes (`ApplicationInit.cpp`, `ClientSessionMarker.cpp`) | mechanism (new reference section) |
| `Server.SingleThreadedLogic` as a project choice | no | yes | yes | mechanism; explain the trade-off in the backend/concurrency decision guide |
| Single `AGENTS.md` front door that explicitly routes to `Engine/AGENTS.md`; other harness files are pointers | yes | yes | Engine has its own | teach (AI-assisted teams) |
| Dated task plans and ignored `Workspace/<task-id>/` intermediates | yes | yes | Engine contributors use the same | teach (AI-assisted teams) |
| Project documentation composition: link to Engine for mechanics, keep only policy locally | yes | yes | documented | mechanism (extend) |
| Comment language, localization editions, knowledge-base coupling, authority rules | yes | yes | n/a | project |

## Target information architecture

Add one task-oriented section, `Docs/en/how-to/project/` (mirrored in `Docs/ru/how-to/project/`), as the lifecycle hub for game teams. Existing owners keep their contracts; the new pages route to them instead of duplicating them.

| Page | Diataxis | Content | Main existing owners linked |
|---|---|---|---|
| `how-to/project/index.md` — Project Lifecycle | how-to hub | Map of setup → daily work → update → release; a routine maintenance checklist | all below |
| `how-to/project/workstation-setup.md` | how-to | Per-OS prerequisites with a version table generated or checked from BuildTools; `host-check` and `prepare-*-workspace`; Python 3 rule; .NET for managed; Windows long paths and symlinks | `how-to/build/index.md`, BuildTools CLI reference |
| `how-to/project/create-repository.md` | how-to | From empty directory to first green smoke: submodule add with relative URL and pin, root files (`CMakeLists.txt`, `CMakePresets.json`, `.fomain`, `VERSION`), `.gitignore`, `.gitattributes`, optional LFS, first configure/bake/smoke | `embedding-project.md`, `project-configuration.md`, `first-project.md` |
| `how-to/project/vscode-workspace.md` | how-to | Recommended extensions; installing FOnline Tools and the AngelScript debugger; label grammar and hotkeys; configure-first task chain; aggregate verification task; launch and attach profiles; workspace settings; notes for Visual Studio and other IDEs | `troubleshooting/debugging.md` |
| `how-to/project/quality-gates.md` | how-to | Formatting (`FO_CLANG_FORMAT`, `.fos` and C# specifics, drift gate), warnings-as-errors, analyzers available to projects, baselines with reasons, generator `--check` gates, aggregate route order | `style-and-refactoring.md`, `managed-csharp.md`, coding contracts, `generated-content.md` |
| `how-to/project/continuous-integration.md` | how-to | Pinned Engine checkout, scoped submodules, LFS ordering, portable steps, caches/mirrors, job order (validate → compile → bake → unit → gameplay → package), scope classification, artifacts, secrets masking, manual release preflight | `public-example-repositories.md#ci-lanes`, `security-and-secrets.md`, `packaging.md` |
| `how-to/project/versioning-and-releases.md` | how-to | Generated monotonic project `VERSION`, `Common.GameVersion`, when to bump gameplay compatibility, tags and release dispatch | `versioning.md`, `client-updater.md`, `operations.md` |
| `how-to/project/data-migrations.md` | how-to | Organizing `///@ MigrationRule` declarations, current-only code, old-save tests on an isolated database copy, retention and removal, identity guards | generated `script-api/migrations.md`, persistence explanation |
| `how-to/project/project-tests.md` | how-to | Native `TESTS` sources and `RunUnitTests`, a script test harness driven by a sub-config, filters, terminal markers, test exclusion from shipping packs, CI gating | `gameplay-and-integration.md`, `contributing/testing/index.md` |
| `how-to/project/ai-assisted-maintenance.md` | how-to | Project `AGENTS.md` shape and routing to `Engine/AGENTS.md`, pointer files for other harnesses, task plans, `Workspace/<task-id>/`, evidence versus authority | `contributing/documentation/index.md` |
| `explanation/scripting-runtime/choosing-a-backend.md` | explanation | AngelScript vs Managed C#: capabilities, platforms, tooling, debugging, analysis, test styles, concurrency choices (`Server.SingleThreadedLogic`) | `scripting-runtime/index.md`, `managed-csharp.md` |
| `explanation/entity-and-property-model/choosing-a-representation.md` | explanation | Decision table: setting, FixedType catalog, prototype property/component, RefType, ValueType, with anti-patterns | entity/property model, metadata reference |
| `troubleshooting/setup.md` | troubleshooting | Missing toolchain, wrong VS toolset, CMake version, Python not found, uninitialized submodule, case drift, path length, symlinks, antivirus/file locks, stale workspace cache, mixed bakes | first-project, project-configuration, generated-content |

Changes to existing pages:

- `how-to/migration/engine-upgrade.md`: literal submodule commands, gitlink/index/HEAD comparison, consumer-impact search, batching for long ranges, an update record template, project-root paths for contract diff.
- `how-to/release/operations.md` (or a new `reference/runtime/log-files.md`): log naming, `.prev.log`, session marker, levels, rotation and retention.
- `how-to/release/` Docker coverage for `BuildTools/docker`, if it is a supported route; otherwise label it internal.
- `how-to/build/embedding-project.md`: link the lifecycle hub; annotated complete root `CMakeLists.txt` skeleton.
- `contributing/tooling/vscode-extensions.md` (new, contributor scope): FOnline Tools source, build (`npm run build`, `npm run typecheck`), why output is committed, how to add a feature, how to test; AngelScript debugger delivery status.
- `Docs/en/index.md` and navigation: add the lifecycle hub; correct the presets claim.

## Engine prerequisites

Status after the 2026-11-07 round:

1. **FOnline Tools delivery.** Done. The extension is a working Engine-owned tool installed through tracked workspace symlinks; typecheck, build and `npm test` pass in a scratch copy, the stale `out/` is removed and the bundle is rebuilt. No CI job builds the extensions yet: the only Node job in `validate.yml` belongs to the documentation browser gate and uses a lock file, which the extensions deliberately do not have.
2. **AngelScript debugger delivery.** Done on the same terms. The attach `processId` filter and the test script were fixed.
3. **Unknown CMake option diagnostic.** Done: `SetOption` and `SetOptionValues` reject undeclared and retired `FO_` names (see the changelog for the exact messages and migration).
4. **Managed C# starter.** Open. An executable managed counterpart of `Examples/MinimalProject` would give the backend checklist and the project-test page a runnable C# route.
5. **Attribute template.** Open. An Engine-supplied `.gitattributes` fragment (or generated list) for FOnline text and binary formats.
6. **Reusable tool dispositions.** Recorded as the [extraction candidate list](#engine-extraction-candidates); each move is a separate change that updates `ExternalProjectEvidence.json`.

## Plan

### Phase A — Corrections and inventory

- [x] Fix audit defects: example options and presets, First Headless Project tutorial and example README, BuildTools README script names, AngelScript setting spellings, updater `CompatibilityVersion` wording, production-plan counts. The `fonline-tools` outputs defect and every defect reported by the page authors were fixed on 2026-11-07 with the owner's authorization (extensions, host scripts, CMake option diagnostics, documentation inconsistencies, three failing tests, the layout check, `ApplicationInit.cpp`); see the verification report. The backlog's 195/197 statements are an append-only log, not a defect.
- [x] Add external-project evidence records `ide-and-editor-workflow`, `repository-hygiene-and-attributes`, `project-ci-structure`, `project-quality-gates`, `project-versioning-and-release-identity`, `project-docs-and-agent-process`. The existing pinned snapshot revisions were kept; re-pinning to the baselines above would require re-verifying every source reference and is deferred.
- [x] Owner decisions of 2026-11-07: the FOnline Tools and AngelScript debugger extensions are working tools, not experimental, installed through tracked workspace symlinks; the list of what can move into the Engine is recorded in [Engine extraction candidates](#engine-extraction-candidates). Until a candidate moves, the pages describe it only as a project-owned pattern.

### Phase B — Priority 0 pages

- [x] `how-to/project/index.md`, `workstation-setup.md`, `create-repository.md`, `vscode-workspace.md`, `troubleshooting/setup.md` with RU mirrors.
- [x] `engine-upgrade.md` mechanics extension with RU update.
- [x] Command snippets registered with the snippet harness (430/430 normative fences pass). Only the missing-`Engine` configure failure was reproduced on Windows; no page claims a Linux or macOS run.

### Phase C — Priority 1 pages

- [x] `quality-gates.md`, `continuous-integration.md`, `versioning-and-releases.md`, `data-migrations.md`, `project-tests.md` with RU mirrors.
- [x] `choosing-a-backend.md`, limited to documented capabilities because Engine ships no managed starter.
- [x] Log-file reference. Docker: `BuildTools/docker/win/Dockerfile` is unsupported and unexercised, so no page was written.

### Phase D — Priority 2 pages

- [x] `ai-assisted-maintenance.md`, `choosing-a-representation.md`, `contributing/tooling/vscode-extensions.md` with RU mirrors.
- [ ] Pages for any tool that Phase A moved into Engine (none moved).

### Phase E — Integration and evidence

- [x] Manifest entries and a `project` navigation group, translation hashes, generated-description overlays (5013/5013 current), site/AI delivery artifacts.
- [x] Three AI-evaluation tasks for the VS Code, setup and update journeys.
- [ ] Conformance walk-through of Last Frontier and TLA against each page (project follow-ups, not Engine requirements).
- [x] Dated section in `_meta/DocumentationVerificationReport.md`.

## Engine extraction candidates

Source: two read-only audits of Last Frontier (70 tool directories, native extensions, scripts, CI, editor setup) and TLA, cross-checked against what the Engine already ships. Every move changes a disposition recorded in `BuildTools/ExternalProjectEvidence.json` (project generators, formatters, audits, CI and versioning are `boundary-owned`; the dialog and GUI stacks are project-owned) and therefore needs Engine code, an Engine test, a documented command and a second project that can use it. TLA is AngelScript and Last Frontier is Managed C#, so a tool that serves only one backend has no second project.

### Move now (small, generic, duplicated)

| # | Candidate | Evidence | Engine home and first step |
|---|---|---|---|
| 1 | Finalize hook that copies the BakerLib runtime next to every application that loads the baker | Hand-written in both projects and in two Engine examples; the application lists have drifted; `CopyTargetRuntimeToTarget` exists but no stage applies it | An Engine stage in `BuildTools/cmake/stages/Applications.cmake`; remove the four copies |
| 2 | Script API `Game.Sha1`, `Game.Sha2`, `Game.HmacSha2` | Same signatures and vendored sources in both projects (the copies drifted); LibreSSL is already in `ThirdParty` | Common script API with the existing `Test_SHA.cpp`; choose explicit names under the API-stability rules |
| 3 | Markdown link and anchor audit | Three independent implementations (Engine `docs_validate.py`, a Last Frontier audit, a TLA check) | One module `BuildTools/markdown_links.py` reused by `docs_validate.py` and exposed as a command |
| 4 | Artifact checksum sidecars (`.sha256`) for final packages | Generic and tested in Last Frontier; TLA publishes releases without checksums | `package.py` option or `BuildTools/artifact_checksums.py` |
| 5 | Engine update report | The upgrade guide carries the plan skeleton; the report script is copied from a listing | `BuildTools/engine_upgrade_report.py` |
| 6 | Formatters for `.fomain` and prototype files, plus delivery of the pinned clang-format | Diverged forks in both projects (about 120 differing lines each; 8 tests in Last Frontier); the Engine owns both formats; `format-source` handles only C++ and C#; two identical 3.9 MB formatter binaries are committed | `format-content` command beside `format-source`; merge the forks, move the tests; ship the pinned formatter |

### Move after generalizing

| # | Candidate | Evidence | Condition or first step |
|---|---|---|---|
| 7 | Native contract audits: allocator, smart pointer, exception safety, entity cover (sync scope), local variables, explicit local types | They exist to enforce `Engine/AGENTS.md` and scan `Engine/Source`; 7701 of 7702 exception-baseline rows describe Engine code; only Last Frontier runs them, so an Engine commit can break its gate later | Parameterize scope and excludes, move the Engine baselines into Engine, run in Engine CI; start with the allocator and smart-pointer audits (no clang needed) |
| 8 | Python readers for Engine formats (prototype `$Parent` resolution, text packs, `.fomain`) | Eight Last Frontier tools parse prototypes and sixteen parse text packs, each differently; the Engine has only a config reader | A library tested against ProtoBaker output; it makes 9 and 10 cheap |
| 9 | Migration guard core (a `MigrationRule` for every removed or renamed identity, no id reuse) | Last Frontier guard with 440 test lines; TLA has 123 rules and no guard | Backend-neutral guard reading content roots from `.fomain`; project id manifests stay local |
| 10 | Project version generator | Same algorithm in both projects; encodings differ | `BuildTools/project_version.py` with base ref and digit limits as parameters; the encoding stays project policy |
| 11 | Native half of the nullability check (`ptr`/`nptr` in `FO_SCRIPT_API` signatures) | TLA runs it as a CI gate; the two copies differ by 815 lines | Move the native half; the AngelScript `T?` half follows with the AngelScript tooling |
| 12 | Engine-contract Roslyn rules (internal entry points, hashed literals, unclearable statics, entry-point calls, `Invariant.Verify` misuse) | They describe Engine scripting contracts; the Engine already has `FOnline.Analyzers` and the loading mechanism | Add as `FO` rules with an `.editorconfig` migration note; style rules stay in the project |
| 13 | AiControl MCP adapter core (framing, process ownership, launch, runner) | About 635 shared definitions in the two adapters (460 byte-identical); neither uses the Engine reference client | `BuildTools/ai_control_mcp/` on top of `ai_control_client.py`; game tools stay in projects |
| 14 | Native AiControl listener (`ClientAiBridge.cpp`) | About 80 percent identical in both projects; Last Frontier has a security fix (no empty token on a non-loopback host) that TLA lacks | Needs the promotion gate in `ai-control-protocol.md` (runtime owner, socket tests, shutdown proof, security review) and the owner's decision |
| 15 | Script test protocol (`Testing.*` settings, terminal summary) | Same setting names in both projects, different language | A managed helper and an AngelScript example around `gameplay_test_runner.py` |
| 16 | Smaller items | Native comment rules (`CommentAudit`), `MoveSyncAnalysis` for the Engine-written `Network.MoveSyncTrace`, text-pack language-script lint, managed project locator, server image read API, release preflight with archive splitting, shared AngelScript hygiene checks | Each is small; do after 7 to 9 |

### Template or documentation only

The `.vscode` task and launch skeleton (already in the VS Code guide; ship it as a validated `Examples/MinimalProject/.vscode`), `.gitattributes` with the FOnline formats (the Engine template has four lines), CMake presets, the long-path guard and LFS content audit patterns, the `global.json` SDK pin, the `.fomain` sub-config and resource-pack layout, and the shared agent-instruction rules.

### Stay project-owned

The dialog stack and editor, the declarative GUI generator and editor, `Scripts/` libraries (the Engine removed its AngelScript CoreScripts on purpose; `Sprite.cs` should instead get its unsafe `AnimFree` use fixed in the Engine), content, quest, narrative, analytics, live-ops and profiling tools, the AngelScript-to-C# porter, and binaries without source (`LFBags`, `SlicePicture`, `ToHash`), which should be retired or documented.

### Project follow-ups found on the way (not Engine work)

TLA: the Engine layout check reports 7 violations in `SourceExt` and TLA does not run it; `ClientAiBridge.cpp` lacks the loopback and token guard; `FO_BUILD_EDITOR ON` is rejected by the current Engine (`Options.cmake`); `add_dependencies(Client ClientLib)` repeats what the Engine does; `migrate_map_dir.py` and `Scripts/FormatSource.bat` are dead. Last Frontier: `Docs/Architecture.md` describes the Engine submodule URL and the content-validation wiring incorrectly.

## Documentation

- Owning docs to update: the pages listed above, `ProductionDocumentationPlan.md` (link this plan from Phase 5 and refresh counts), `_meta/DocumentationBacklog.md`, `Docs/documentation-manifest.json`, `Docs/en/index.md`, `reference/changelog.md` for Engine file changes.
- Cross-links to verify: `embedding-project.md`, `project-configuration.md`, `first-project.md`, `engine-upgrade.md`, `troubleshooting/debugging.md`, `gameplay-and-integration.md`, `managed-csharp.md`, `style-and-refactoring.md`.

## Validation

- Passed: `docs_prepare.py`, `docs_validate.py` (430 Markdown entries), `docs_snippets.py --external` (431 normative fences), `docs_localization.py --enforce-complete` (211/211), `docs_description_translations.py --check`, `docs_ai_eval.py` (31 tasks, 73 retrieval checks, 100 percent), `docs_contract_diff.py --baseline-git-ref HEAD --enforce`, `docs_engine_version.py --check` (`2026.1.113-dev`), `docs_external_evidence.py --verify-sources`.
- Focused tests for every fix were added or updated; the full `BuildTools/tests` result is recorded in the verification report.
- Not run: a full configure, bake or build of any example after the CMake edits, a native compile of the `ApplicationInit.cpp` change, anything on Linux or macOS, a real VS Code window, the rendered Jekyll site and browser gates.

## Open decisions

1. Which extraction candidates to start with (see the list above; every move changes an `ExternalProjectEvidence.json` disposition and needs Engine code, tests and documentation together).
2. A managed C# starter example and an Engine-supplied attribute template remain unplanned; the managed backend guide is limited to documented capabilities until a starter exists.
3. Owner review of the security gate before the native AiControl listener can move.

## Notes

- 2026-11-06: plan created from the three source audits; pages, mirrors, manifest entries, evidence records, evaluation tasks and verification report written the same day. Engine `VERSION` advanced to `2026.1.113-dev`; nothing is staged or committed.
- 2026-11-07: owner decisions applied (extensions are working tools, not experimental; extraction candidate list recorded; all reported defects fixed without further approval). The unknown-option diagnostic, host-feature strictness, Python 3.12 floor and extension changes are breaking for some projects and are described with search commands in the changelog migration notes.
