# FOnline Production Documentation Program

- **Status:** In progress
- **Baseline date:** 2026-07-10
- **Engine baseline:** `67ee893ae721d149cd44ff314abd8036adfd3821`
- **Last Frontier baseline:** `805caa79976b7cf4f81e46e1cf9ca0f1ea96ba43`
- **TLA baseline:** `b603d8fdbc2b2f89f233b2a1938686ead9d8d480`
- **Predecessor:** [DocumentationExpansionPlan.md](DocumentationExpansionPlan.md), which remains the completed historical plan for the first source-coverage pass.

## Outcome

Turn the FOnline documentation into an independent, production-grade product that lets a game developer, engine contributor, tools author, release engineer, or AI agent answer these questions without opening a particular game repository:

- What is FOnline, which use cases and platforms does it support, and what is stable?
- How do I create, build, run, test, debug, package, and update a new game?
- How do I author scripts, entities, properties, prototypes, maps, resources, effects, and localization?
- What is the complete script, settings, build, command-line, metadata, and file-format API?
- Which architectural and game-development practices are recommended, and why?
- How do I migrate a game across engine changes without breaking saves, network compatibility, or content?
- Where can I inspect a minimal, tested, legally redistributable example of each important workflow?

The finished documentation must work from a standalone clone of `cvet/fonline`. Last Frontier and FOnline: The Life After (TLA) are research inputs and conformance projects, never required dependencies or normative proof.

## Source-of-truth order

When sources disagree, use this order:

1. Engine source, engine tests, BuildTools, and generated metadata.
2. A documented and tested public contract in the engine repository.
3. Engine documentation generated from that contract.
4. Engine documentation maintained by hand and verified against the owning source paths.
5. Public example repositories pinned to a known engine revision.
6. Last Frontier and TLA as evidence of real integration needs.

Project documentation can reveal missing engine documentation or reusable tooling. It cannot define engine behavior by itself.

## Baseline audit

### What is already strong

The first expansion program produced a substantial internal reference set:

- 38 Markdown pages under `Docs/`, about 76,700 words in this snapshot.
- Source-grounded architecture, source-tree, build-pipeline, baking, runtime, networking, persistence, rendering, scripting, testing, and platform pages.
- Detailed native contracts for nullability, smart pointers, exception safety, and Clang Thread Safety Analysis.
- A maintained AI entry point in `AGENTS.md` and a human-oriented repository landing page.
- A verification workflow, backlog, research template, and dated verification report.

This is a useful maintainer reference. It should be preserved, reorganized, and made easier to enter rather than rewritten from zero.

### Critical production gaps

The current set is not yet a self-sufficient game-developer product:

| Finding | Current evidence and status | Required response |
|---|---|---|
| Beginner path stops at a headless milestone | `TUTORIAL.md` now runs the engine-owned starter through configure, build, bake, AngelScript lifecycle, and clean shutdown on Windows; it does not yet create a playable client/map loop | Keep the tested lesson and add tagged public first-client, first-content, and first-test lessons |
| Public API reference is incomplete | `Docs/generated/api.json` schema v2 and seven checked Markdown pages render native-codegen symbols plus explicit/default `ApiContract` provenance; project remote calls have a baked supplement; CMake, the main BuildTools CLI, package declarations/payloads, helper CLIs, native extensions, prototype format, map format, model format, text format, effect format, image format, particle format, and font format have separate source-backed models/references; all fourteen generated domains participate in base-revision diff enforcement, while broad owner-reviewed classifications remain | Extend coverage through each owning contract and review classifications under release policy |
| Standalone engine boundary was broken | The original audit found 33 root-escaping links and 17 project-dependent docs; `docs_validate.py` now rejects escapes and the maintained corpus passes standalone validation | Keep the gate required and use tagged HTTPS links for future cross-project examples |
| Several pages mixed engine and project workflows | Debugging, updater, Web, mapper, Android, nullability, and testing pages were split into reusable engine procedures on 2026-07-10 | Reintroduce concrete recipes only through tagged public sample repositories |
| Manual inventories drifted | The original 947/18 script-method and 84-test counts disagreed with prose | `Docs/generated/source-inventory.json`, `Docs/generated/api.json`, and the Markdown reference are generated and checked in CI; do not restore manual counts |
| Settings outside codegen remain undocumented | Every native codegen setting has a rendered symbol, every declared project-facing CMake option has a generated type/default/required/category record, and the main BuildTools CLI records parser defaults and choices | Extend structured coverage to package and project-authored settings through their owning parsers |
| Build/API reference is incomplete | CMake options/stages/helpers, the main BuildTools CLI, package declarations/targets/platforms/packs/payloads, helper-script CLIs, native-extension roles/hooks/bindings, prototype grammar/properties, map authoring/baking, model composition, text/localization, effect authoring/baking/runtime, image/FOFRM baking/runtime, particle XML/tooling/runtime, and bitmap-font binding/layout now have checked generated references and a shared cross-domain revision comparator | Continue with remaining authored-format and project-settings contracts |
| Format authoring is incomplete in Engine | Prototype, map, `.fo3d` model, `.fotxt`/prototype-text, `.fofx` effect, image/FOFRM, `.fopts`/SPARK particle, and FOFNT/BMFont syntax, delivery, runtime interpretation, and validation now have engine-owned guides plus generated source-backed models; model animation and 2D sprite root motion have focused source-backed guides; GUI and dialogs still rely partly on project-specific material | Continue with GUI/dialog ownership classification plus audio, video, and tool manuals from their owning implementations |
| Verification did not catch structural drift | Earlier pages cited missing paths and stale numeric claims as verified | Source-path, manifest, local-link/anchor, placeholder, generated-inventory, and fourteen-domain base-revision checks now run in a fast docs job; broader semantic checks remain |
| Publication pipeline is incomplete | Fast standalone checks and a GitHub Pages-compatible Jekyll `_site` artifact job are now wired; the first CI render is still pending, and snippet, accessibility, and translation-parity gates do not yet exist | Confirm the first rendered artifact, then add the remaining focused gates without coupling docs to a native build |
| Current site is a repository landing page | Manifest-backed navigation, compact static search, responsive layout, source links, local theme preference, page table of contents, rolling `master` identity, ADR-0006 version/locale policy, and a generated current/planned route catalog are implemented over Markdown; the first rendered artifact and production deployment remain unobserved, and the physical locale migration is not implemented | Confirm the Jekyll artifact and production route, then move canonical pages behind durable legacy Markdown routes, add bilingual navigation, and complete accessibility checks without changing publication architecture |
| No visual teaching assets | `Docs/` contains Markdown only and no owned diagrams, screenshots, or tutorial media | Add versioned diagrams and tool/runtime screenshots with accessibility metadata |
| No public canonical example game | `Examples/MinimalProject` is an engine-owned runnable headless scaffold; ADR 0005, the checked four-repository registry, governance overlay, exact-pin validator, and Windows/Linux CI policy now define publication, but no tagged standalone template repository or playable slice exists yet | Obtain green starter CI evidence, then perform the owner-gated first template publication |
| No translation system | Every current public human stable ID now has a planned English/Russian target and normalized-hash policy, but the Russian tree, glossary, parity gate, and reviewed translations do not exist | Add the Russian mirror only after the first execution slice is green and the English information architecture is ready for translation |
| AI access is only partly structured | `AGENTS.md`, generated `llms.txt`, bounded context, public manifest, compact site search, source inventory, fourteen generated contract models, and aggregate JSON diff reports now provide routing, ownership, stable IDs, signatures, provenance, and change classification | Add clean Markdown endpoints and a versioned retrieval/task evaluation set, then test standalone task completion |

### External project findings

Last Frontier is the richer current source of documentation practices: its top-level `Docs/*.md` set is about 309,000 words and covers content workflows, testing boundaries, generated-file ownership, migrations, nullability, synchronization, packaging, and game-system ownership. It is also intentionally product-specific, so copying pages wholesale would recreate the dependency this program must remove.

TLA is valuable as a second large embedding project and a compatibility check:

- Its [README](https://github.com/cvet/fonline-tla/blob/b603d8fdbc2b2f89f233b2a1938686ead9d8d480/README.md) and [CMakeLists.txt](https://github.com/cvet/fonline-tla/blob/b603d8fdbc2b2f89f233b2a1938686ead9d8d480/CMakeLists.txt) show a real project root, staged engine build, resources, scripts, native extensions, and packaging.
- Its [Animation.md](https://github.com/cvet/fonline-tla/blob/b603d8fdbc2b2f89f233b2a1938686ead9d8d480/Docs/Animation.md) supplied discovery material for the now source-verified 2D contract in [SpriteRootMotion.md](SpriteRootMotion.md). TLA's tracked `.fo3d` files are historical integration evidence only: several use removed tokens or unsupported mesh extensions, while current grammar and composition are pinned to Engine source in [ModelFormat.md](ModelFormat.md) and [generated/model-format/index.md](generated/model-format/index.md).
- Its [Game.engl.fotxt](https://github.com/cvet/fonline-tla/blob/b603d8fdbc2b2f89f233b2a1938686ead9d8d480/Texts/Game.engl.fotxt) demonstrates historical numeric keys, duplicate variants, renderer `@color` tags, project-owned `@arg` formatting, and an incomplete symbolic-key migration. It is integration evidence for compatibility review, not the normative current parser/runtime contract now owned by [TextAndLocalization.md](TextAndLocalization.md) and [generated/text-format/index.md](generated/text-format/index.md).
- Its [ScriptStyle.md](https://github.com/cvet/fonline-tla/blob/b603d8fdbc2b2f89f233b2a1938686ead9d8d480/Docs/ScriptStyle.md) contains useful examples of module ownership and generated-file discipline, mixed with TLA-only language and refactoring policy.
- Recent TLA history is dominated by broad refactoring and engine migrations. Treat observed patterns as candidates to validate, not standards to copy.
- The scripting API URL advertised by the TLA README returned HTTP 404 during this audit. A generated reference is not complete until publication and link monitoring are tested too.

### Material extraction map

| Existing material | Reusable knowledge to extract | Engine destination | Material that must remain project-owned |
|---|---|---|---|
| Last Frontier `Architecture.md`, `BuildAndLaunch.md`, `Onboarding.md` | Change-to-build routing, staged workflow, generated-output ownership, first-run expectations | Tutorials, build how-tos, project template | LF targets, presets, subconfigs, binaries, ports, services |
| Last Frontier `Prototypes.md` | INI syntax, identity/inheritance, engine components, serialization and migration behavior | File-format and migration reference generated/verified from engine parsers | LF field catalogs, balance semantics, ids, directory taxonomy |
| Last Frontier `Localization.md` | `.fotxt` grammar, `TextPackKey`, tags, language baking, fallback and validation | Text/localization reference and how-tos | LF pack names, source-language policy, authored strings |
| Last Frontier `Scripts.md`, `Events.md`, `Properties.md` | Module lifecycle, side boundaries, attributes, events/remotes, property flags and generated accessors | Scripting guide and generated API reference | LF module map, gameplay properties, custom events/remotes |
| Last Frontier `Multithreading.md` and sync audits | Callback context, lock-set replacement, entity cover, re-entrancy, post-yield validation | Script concurrency and lifecycle explanation/how-to | LF helper names, audit inventory, game-specific lock groups |
| Last Frontier `Testing.md` | Test-boundary selection, deterministic fixtures, narrow-first validation, log interpretation | Testing strategy and example-project harness | LF gameplay suites, MCP runners, CI names and fixtures |
| Last Frontier `NativeExtensions.md`, `ProjectThirdPartyMaintenance.md` | Role-scoped native sources, hooks/exports, dependency patching and packaging boundaries | Native-extension and project dependency guides | Steam/Sentry/dialog/web integrations and vendored project libraries |
| Last Frontier `GuiSystem.md`, `Dialogs.md` | Authoring ergonomics, generator ownership, declarative validation patterns | Companion-system decision and, if moved, engine/companion docs | Current project generators, runtime scripts, dialog schemas until reusable code moves |
| Last Frontier `ContentWorkflow.md`, `MapAuthoring.md` | Goal-oriented recipes and validation structure | Content how-tos and public tutorial projects | LF quests, AI art pipeline, map kits, content ids |
| Last Frontier `Profiling.md` | Tracy capture model, reproducible scenes, client/server measurement boundaries | Profiling how-to | LF scene ids, tasks, report locations |
| TLA README and CMake root | Independent integration shape and migration pressure | Starter-template conformance tests | TLA targets, packages, content and local policies |
| TLA `Animation.md` and `.fo3d` assets | Historical 2D root-motion behavior and a broad sample of legacy model composition | [SpriteRootMotion.md](SpriteRootMotion.md), [ModelFormat.md](ModelFormat.md), and [ModelAnimation.md](ModelAnimation.md), all re-derived from current source | TLA assets, removed tokens, unsupported mesh extensions, enum policy, and gameplay timing remain non-normative |
| TLA `ScriptStyle.md` and refactoring plan | A second sample of module ownership and generated-file discipline | Best-practice comparison input | Russian-comment policy, refactoring status, legacy exceptions |

### Engine work that documentation alone cannot solve

Some gaps require reusable engine artifacts before the corresponding documentation can be truthful:

- A public project template and project generator; the runnable internal baseline now lives at `Examples/MinimalProject`, while repository creation, client/content milestones, and release tags remain.
- A codegen-owned public API model and renderer; prose cannot reliably enumerate 947 and growing script methods.
- Structured documentation metadata for public symbols, including description, stability, version, deprecation, and example links.
- Generated settings, CMake option/helper, CLI, package, and file-format reference inputs.
- Engine-owned, project-neutral analyzers for contracts currently documented through Last Frontier tools, especially nullability and smart-pointer audits.
- A reusable integration/gameplay smoke harness that public examples can consume without importing LF tests.
- A clear home for reusable dialog/GUI generation, formatters, migration guards, and content audits: Engine, a companion package, or intentionally project-only.
- Tagged engine releases and a support policy; documentation versioning cannot compensate for an undefined release contract.
- Small redistributable tutorial resources and explicit asset licenses; a tutorial cannot depend on proprietary or legacy game data.
- Documentation build, generation, snippet, and publication commands exposed through BuildTools/CI.

## Audiences and required journeys

| Audience | Minimum successful journey |
|---|---|
| New game developer | Clone a supported starter, configure, bake, start a server and client, enter a map, change one script, and see the result |
| Gameplay/script developer | Find an API by task or symbol, understand side/authority/nullability/sync rules, add behavior, and write a focused test |
| Content author | Create or modify a prototype, map, text, effect, or resource; validate it; diagnose a bake/runtime failure |
| Native extension developer | Add a role-scoped source file, implement a hook/export, understand ABI and compatibility impact, and test it |
| Engine contributor | Locate ownership, change source safely, run the right tests, update generated/public contracts, and document the change |
| Tool developer | Extend a baker, mapper/editor surface, or BuildTools command and document inputs, outputs, and failure modes |
| Release/operator | Build reproducibly, package supported platforms, configure secrets safely, deploy, update, recover, and diagnose startup/runtime problems |
| AI coding agent | Select the authoritative page, retrieve exact contracts and examples, avoid project-only assumptions, and cite the owning source/version |

Every public page must name at least one audience and answer one concrete learning, task, information, or understanding need.

## Information architecture

Use the [Diataxis](https://diataxis.fr/) distinction between tutorials, how-to guides, reference, and explanation. Existing subsystem pages are mostly explanation or maintainer reference; the largest missing areas are tutorials, task-oriented how-tos, and generated reference.

Target localized source tree:

```text
Docs/
|-- README.md               # thin language router for the GitHub repository view
|-- en/                     # canonical English tree
|   |-- index.md
|   |-- tutorials/
|   |   |-- first-project.md
|   |   |-- first-multiplayer-loop.md
|   |   |-- first-content-change.md
|   |   `-- first-test.md
|   |-- how-to/
|   |   |-- build/
|   |   |-- scripting/
|   |   |-- content/
|   |   |-- tools/
|   |   |-- platforms/
|   |   |-- package-and-release/
|   |   `-- migrate/
|   |-- reference/
|   |   |-- script-api/           # generated
|   |   |-- settings/             # generated
|   |   |-- cmake-and-buildtools/ # generated where possible
|   |   |-- metadata/
|   |   |-- file-formats/
|   |   |-- command-line/
|   |   |-- compatibility/
|   |   `-- support-matrix.md
|   |-- explanation/
|   |   |-- architecture/
|   |   |-- entity-and-property-model/
|   |   |-- authority-and-networking/
|   |   |-- persistence/
|   |   |-- rendering/
|   |   |-- scripting-runtime/
|   |   `-- concurrency-and-lifecycle/
|   |-- contributing/
|   |   |-- source-tree/
|   |   |-- coding-contracts/
|   |   |-- testing/
|   |   |-- documentation/
|   |   `-- third-party/
|   |-- troubleshooting/
|   `-- glossary.md
|-- ru/                     # one-to-one mirror of en/ after the English IA freezes
|-- assets/                 # shared published media, styles, and search assets
`-- _meta/                  # plans, reports, inventories; excluded from publication
```

Rules:

- Preserve stable document IDs and redirects when current files move.
- Keep one canonical owner for each contract; other pages link to it.
- Keep tutorials linear and guaranteed to succeed on a pinned example revision.
- Keep how-to guides goal-oriented and free of long conceptual detours.
- Generate reference from the machinery it describes whenever possible.
- Keep explanation source-grounded but readable without opening implementation files.
- Move plans, dated audits, and verification logs under `_meta/`; do not mix them into the first-run navigation.
- Keep root, `Source/`, `Source/Tests/`, and `BuildTools/` READMEs short and route deep human content into the localized docs site.

## Public contract model

### Surfaces that need an explicit contract

| Surface | Contract owner |
|---|---|
| Project integration | `BuildTools/Init.cmake`, stage helpers, CMake options, stage hooks, project template |
| Runtime configuration | `Source/Common/Settings.inc`, config parser, resource packs, subconfigs, command-line overrides |
| Script API | codegen metadata, `Source/Scripting/*ScriptMethods.cpp`, exported entities/properties/events/remotes/enums/types |
| Metadata authoring | all supported `///@` annotations, signatures, side rules, migration semantics |
| Content formats | parser and baker implementations for config, prototypes, maps, text, effects, images, models, and particles |
| Native extension API | hook names/signatures, source roles, exported methods, lifecycle, ABI and compatibility rules |
| Tooling API | BuildTools CLI, baker/compiler/mapper/editor commands, inputs, outputs, exit codes, generated files |
| Runtime compatibility | save migrations, network/compatibility version, client runtime ABI, package/update compatibility |

### Stability labels

Every public surface receives one label:

- `stable`: compatibility is maintained within the declared release line; breaking changes require migration and release notes.
- `experimental`: usable and documented, but may change with an explicit changelog entry.
- `deprecated`: supported temporarily with replacement and removal target documented.
- `internal`: described for contributors but not promised to embedding projects.

Do not promise broad backward compatibility before the engine support/release policy exists. ADR-0006 governs documentation channels and deliberately defers release snapshots; it does not make engine integration stable. Until a product support policy exists, label the current integration surface experimental and pin examples to exact engine commits.

### Generated reference pipeline

Extend the existing codegen and settings sources instead of creating a second ad hoc C++ parser:

1. Emit a deterministic, versioned `api.json` model from the same parsed metadata used to generate bindings.
2. Include symbol ID, kind, runtime side, receiver, signature, defaults, nullability, mutability, source path, source line, stability, since/deprecated fields, and related examples.
3. Generate script API pages grouped by task and symbol, with direct links to source and runnable examples.
4. Generate settings pages from all 265 declarations in `Settings.inc`, including type, default, fixed/variable/read-only state, side, secrecy, and description.
5. Generate CMake option and stage-helper reference from a runtime-consumed structured manifest. This now emits the canonical `cmake.json` model and checked Markdown pages.
6. Generate CLI reference from `argparse` definitions and executable `--help` output. This now emits separate canonical models and checked Markdown for the main BuildTools CLI and all inventoried helper-script CLIs.
7. Generate metadata-annotation grammar and supported-value tables from codegen/metadata parser definitions.
8. Add schema/reference generators for formats whose parsers or registries expose enforceable structure. Prototype, map, model, text, effect, image, particle, and font models now derive and validate their owning source surfaces; remaining formats need owning generators or focused hand-written grammar tests.
9. Generate an aggregate contract diff report between engine revisions and require migration/release notes for breaking public changes in every modeled domain.
10. Check generated pages into the repository for GitHub/offline browsing, but make source metadata authoritative and fail CI when regeneration changes the tree.

The first generator release may publish signatures before every description is complete. Production release requires descriptions and at least one example for every stable public group, not necessarily every internal symbol.

## Content coverage program

### Priority 0: first successful game

- Supported-host prerequisites and support matrix.
- Clone/submodule/project-template workflow.
- Minimal annotated `CMakeLists.txt`, presets, and `.fomain`.
- Resource-pack and subconfig mental model.
- Configure, generate, bake, build, and launch commands with expected outputs.
- Minimal server/client connection, player creation, map entry, and shutdown.
- First script change and first engine/game log trace.
- First deterministic test and first CI run.
- Troubleshooting for the ten most common setup failures.

### Priority 0: script and data contracts

- Script module lifecycle and side selection: common, server, client, mapper.
- Attributes, events, remote calls, time events, generated properties, settings, and content constants.
- Server authority and client presentation boundaries.
- Nullability, handle identity, ownership, lifetime, and entity validity.
- Script concurrency: callback execution, `[[Async]]`, lock-set replacement, entity/map/location cover, post-yield revalidation, and re-entrant events.
- Persistence, replication, migrations, and compatibility impact.
- Complete generated script API and settings reference.
- `.fomain`, `.fos`, `.fopro`, `.foitem`, `.focr`, `.foloc`, `.fomap`, `.fotxt`, and `.fofx` references.

### Priority 1: content and tools

- Prototype inheritance, components, identity, text, migration, and validation.
- Map format, geometry, blocking, movement, entrances, mapper workflow, and headless automation.
- Localization/text-pack grammar, tags, fallback, language baking, and validation.
- Images, sprite sheets, root motion, texture atlases, fonts, audio, and video.
- Effects/shaders, render backends, depth conventions, script values, and cross-platform limits.
- 3D model, animation, material, particle, and model-baker workflows.
- Mapper, editor, asset explorer, and particle editor user manuals with screenshots.
- Native extensions, custom hooks, custom methods, and project-local third-party code.
- Packaging, updater, Web, Android, desktop, and server-service how-tos.

### Priority 1: engineering and operations

- Unit, script/gameplay, integration, package, and smoke-test boundaries.
- Native and script debugging, stack traces, sanitizers, coverage, and profiling.
- Reproducible builds, CI, artifact provenance, signing, secrets, and release verification.
- Database backend choice, backups, migrations, recovery logs, and operational limits.
- Security model for untrusted clients, remote calls, inbound properties, networking, and updater inputs.
- Upgrade guides and changelog entries for every public breaking change.

### Priority 2: optional and companion systems

Dialogs, generated GUI definitions, gameplay test harnesses, AI-control tooling, migration guards, formatters, and project audits currently live partly or wholly in embedding projects. For each system, choose one disposition before documenting it as an engine feature:

1. Move the reusable implementation and tests into the engine, then document it as engine-owned.
2. Publish it as a versioned companion package/repository with its own docs.
3. Keep it project-specific and mention only the extension point or general pattern.

Never describe a Last Frontier or TLA implementation as built into the engine when the required code is not in `cvet/fonline`.

## Best-practice promotion process

Last Frontier should seed candidate practices because it has the broadest current validation matrix. TLA should challenge those practices against a second, older, actively refactored game. A practice becomes an engine recommendation only when all applicable gates pass:

1. The behavior is supported by an engine contract or intentionally reusable helper.
2. The recommendation is not tied to one game's names, content, tasks, CI, or product policy.
3. The pattern is covered by an engine test, a public example test, or both.
4. TLA either uses the pattern successfully or a documented engine reason explains why it should migrate.
5. The recommendation includes failure modes and an anti-pattern, not only a preferred code shape.

Candidate practice families:

- authoritative server and presentation-only client responsibilities;
- engine/game/native-extension ownership;
- data-driven content and generated symbol use instead of hard-coded ids;
- namespace-per-module and explicit runtime-side boundaries;
- event subscription from module initialization and plain helpers behind attributed entry points;
- explicit nullability and narrowing;
- async entity-cover acquisition and post-yield/re-entrant revalidation;
- generated-file source ownership;
- migration rules for serialized names and compatibility changes;
- focused validation first, then broader build/test gates;
- content-backed setting validation in tests instead of startup-wide assertions;
- deterministic tests, zero warnings, and source-level fixes;
- pinned engine revisions and coherent submodule updates;
- source-grounded docs updated with behavior.

Examples that remain project policy include game balance, quest architecture, exact login flow, comment language, project-specific sync helper names, exact VS Code task names, and release infrastructure.

## Public example repository program

Create a small family of public repositories. One repository should teach one level of responsibility; do not turn a single sample into another full game.

### 1. `cvet/fonline-project-template`

Purpose: the canonical GitHub template and the source of every quickstart command.

Required contents:

- Engine submodule pinned to a documented revision or release.
- Minimal staged `CMakeLists.txt`, `CMakePresets.json`, `.fomain`, resource packs, scripts, and test wiring.
- Windows and Linux local presets; optional platform presets can be added after the baseline is green.
- One-command configure/bake/build/headless-smoke workflow.
- Empty or minimal `SourceExt/` with comments showing when it is needed.
- CI for format, generation, bake, build, unit tests, and server-start smoke.
- No copied Last Frontier/TLA game content or assets.
- A project generator in the engine, for example `BuildTools/new-project.py`, whose template output is byte-for-byte checked against this repository.

Exit gate: a clean Windows or Linux machine can follow the published quickstart and reach the documented successful server state in under 30 minutes.

### 2. `cvet/fonline-minimal-multiplayer`

Purpose: a tiny playable vertical slice used by the tutorials.

Required contents:

- One small map and location, one player prototype, one NPC, and one item.
- Server-authoritative interaction, one replicated/persisted property, one event, and one remote call.
- English and Russian text, one minimal client UI interaction, and clear generated-file ownership.
- A deterministic server-side scenario plus a client-visible smoke test.
- Tutorial tags or releases that match stable lesson checkpoints; do not maintain divergent long-lived tutorial branches.
- A downloadable desktop build and, after native stability, a live Web build.

Exit gate: every tutorial step is reproduced in CI from its pinned tag, and the final project is understandable without Last Frontier or TLA.

### 3. `cvet/fonline-content-showcase`

Purpose: presentation-quality inspection of rendering and authoring capabilities without a large gameplay codebase.

Required contents:

- A compact gallery map demonstrating sprites, animation/root motion, lighting, effects, particles, audio, and optional 3D content.
- Mapper/editor authoring examples and source assets next to their runtime result.
- Cross-backend screenshots and a browser build where supported.
- CC0, public-domain, or purpose-made assets with an explicit machine-readable provenance manifest.
- Performance budgets and backend/platform limitations documented honestly.

Exit gate: the repository produces a public Web or downloadable showcase, all asset rights are auditable, and screenshots are regenerated from a tagged build.

### 4. `cvet/fonline-native-extension-sample`

Purpose: an advanced, still-minimal example of project-native C++ integration.

Required contents:

- One lifecycle hook, one script export, one role-specific source, and one focused native test.
- CMake wiring, pointer/nullability conventions, ABI boundaries, and compatibility impact.
- No game-specific service integration.

This repository is Priority 1 and starts only after the template and script API reference are stable.

### Shared repository policy

- The policy is now source-controlled in `Examples/PublicRepositories.json`, explained by `Docs/PublicExampleRepositories.md` and ADR 0005, projected to generated JSON/Markdown, and enforced by `BuildTools/docs_examples.py`. `Examples/PublicRepositoryTemplate` owns the common governance/workflow/provenance files. All four remote repositories were created privately on 2026-07-20; the template candidate is source-staged and the other three names are reserved. Public visibility and release remain separate administrator-gated transitions.
- Pin exact engine revisions; never build examples against a floating default branch in release CI.
- Run a scheduled compatibility job against current engine `master` and open an update PR only after all gates pass.
- Reuse a versioned workflow from the engine repository for common checks.
- Publish the engine revision in every artifact and screenshot.
- Keep READMEs short, visual, and task-oriented; link deep contracts to the engine docs site.
- Include `LICENSE`, dependency licenses, asset provenance, security policy, and contribution rules.
- Verify every docs snippet against one exact example path/tag.

## Documentation platform and localization

### Publishing stack

Keep the existing GitHub Pages/Jekyll publication contract. GitHub Pages renders the repository's Markdown, `_config.yml` configures the site and Slate theme, and the root `CNAME` binds the production site to `fonline.ru`. This is an architectural constraint, not an open framework-selection item.

Human-authored content remains in versioned `.md` files in this repository. Do not introduce Docusaurus, a parallel `website/` content tree, or checked-in generated HTML. Site presentation may use GitHub Pages-supported Jekyll facilities such as YAML front matter, layouts, includes, data files, theme overrides, and static assets, but those facilities must remain a thin rendering layer over the Markdown corpus.

Production publishing continues through the existing GitHub Pages branch/folder configuration. Phase 0 must record that exact repository setting and the DNS ownership procedure, but must not replace the publication route. CI should build with a GitHub Pages-compatible Jekyll environment so pull requests exercise the same Markdown, Liquid, theme, and supported-plugin constraints as production.

Required site capabilities:

- responsive Jekyll navigation organized by task/audience from stable manifest IDs (implemented for the current English paths);
- static full-text search over prose and generated API symbols, with a compact index derived from Markdown (implemented under a 1 MiB budget);
- English/Russian language switch preserving the same relative document path and stable document ID;
- current/stable version indicator and unmaintained-version banners (rolling `master` is now the explicit `current` channel; tagged release snapshots and banners remain deferred until supported release lines exist);
- source/edit links pinned to the displayed engine revision (implemented for rolling `master`);
- redirect map for all moved legacy pages (the generated current-to-planned map and durable Markdown strategy are implemented; pointer pages land with each actual move);
- syntax highlighting for AngelScript and FOnline data formats;
- Mermaid or checked-in generated diagrams plus accessible alt text;
- local Jekyll preview plus a rendered `_site` artifact on pull requests;
- sitemap, canonical URLs, Open Graph metadata, and downloadable offline bundle;
- no analytics credential or external runtime dependency required to read docs.

Do not create historical documentation copies before the engine has a useful tag series and support policy; `VERSION` is still `2022.1.0.wip`. Until then, publish current documentation at stable URLs and retain commit-linked CI artifacts for historical inspection. If supported release snapshots are later required, generate or maintain them as ordinary Markdown paths or release branches through a separate ADR, without changing the GitHub Pages/Jekyll publication contract.

### Locale layout decision

Do not create sibling roots named `Docs.EN` and `Docs.RU`. Use lowercase locale directories inside the existing documentation root so the same files are readable in both the GitHub repository UI and on the Jekyll site:

```text
Docs/
  en/             # canonical English human docs
  ru/             # Russian mirror with identical relative paths and doc IDs
  assets/         # published diagrams, screenshots, styles, and search assets
  _meta/          # internal plans, inventories, and verification reports; not published
README.md         # English repository landing page
README.ru.md      # Russian repository landing page
AGENTS.md         # AI-maintainer instructions, not translated as human docs
_config.yml       # GitHub Pages/Jekyll configuration
CNAME             # fonline.ru
```

Every migrated public page receives YAML front matter with at least a stable document ID, locale, title, and permalink. English and Russian copies use the same path below `Docs/<locale>/`, allowing a Jekyll include to construct the language switch without a framework-specific translation registry. ADR-0006 and `Docs/generated/document-routes.json` reserve those paths and their canonical owners now. Existing public URLs must remain as durable Markdown pointer pages when files move.

Translation rules:

- English is canonical because engine identifiers, source comments, public symbols, and the international upstream repository are English.
- Every public human page, including contributor and operator guides, must have a Russian mirror before a production docs release.
- Internal plans/reports under `_meta/` are excluded from the public site; if published, they become translation-scoped.
- Identifiers, code, command output, paths, config keys, API signatures, and serialized names are never translated.
- Maintain a bilingual terminology glossary and translation memory keyed by stable doc/symbol IDs.
- Store a canonical-content hash for each translation. CI marks a Russian page stale when its English source changes.
- Translation parity is a release-blocking gate. Ordinary source PRs may carry a clearly visible `translation-pending` state only while the docs site is pre-production.
- Human review is required for public terminology, safety instructions, and migration guidance even when AI drafts the translation.
- Generated API translations use symbol-ID keyed description catalogs rather than hand-edited generated pages.

## AI-first delivery

Human clarity remains the primary quality signal. Add machine-oriented delivery without creating a second contradictory documentation set:

- Keep `AGENTS.md` concise and navigational; it points to the same owning human docs and generated contracts.
- Generate `/llms.txt` as a curated map of the most important Markdown pages and examples. Treat it as an emerging proposal, not a replacement for normal docs, sitemap, or APIs.
- Generate `llms-full.txt` or an equivalent bounded context bundle for offline assistants.
- Publish `docs-manifest.json` with stable IDs, titles, audience, Diataxis kind, locale, engine version, stability, owner, source paths, examples, and content hash.
- Publish `api.json`, `settings.json`, and compatibility-diff JSON for tools and agents.
- Offer clean Markdown for every public page, not only client-rendered HTML.
- Use stable headings, explicit prerequisites, deterministic commands, expected output, and exact failure messages.
- Keep code examples complete enough to compile or bake; avoid ellipses in normative snippets.
- Add an evaluation set of representative tasks and questions. Test retrieval, source selection, version selection, and final answer correctness with at least two model families.
- Record whether an answer came from stable reference, explanation, or an example project so agents can distinguish contract from illustration.

The `llms.txt` work follows the public [llms.txt proposal](https://llmstxt.org/), including its recommendation to provide concise Markdown routing and optional full-context material.

## Quality and CI gates

### Pull-request gates

| Gate | What it proves |
|---|---|
| `docs-format` | Markdown/frontmatter/style/terminology are structurally valid |
| `docs-links` | Internal links and anchors resolve; no local link escapes the engine root |
| `docs-paths` | Declared source/test/build paths exist at the documented revision |
| `docs-generated` | API/settings/CMake/main-helper CLI/native-extension/package/reference generation is deterministic and committed output is current |
| `docs-pages` | The bilingual GitHub Pages/Jekyll site builds from repository Markdown without warnings |
| `docs-snippets` | Shell, CMake, C++, AngelScript, config, and data snippets parse or execute in their declared harness |
| `docs-examples` | Pinned starter/tutorial contracts still configure, bake, build, and smoke-test |
| `docs-i18n` | Document IDs mirror correctly and translation freshness is known |
| `docs-contract-diff` | Public/model-contract removals and shape changes have stability, migration, and release-note disposition |
| `docs-accessibility` | Navigation, headings, links, contrast, alt text, and keyboard use meet the selected WCAG target |

External link checks should run on a schedule with retry/cache rather than making every source PR depend on third-party availability. Broken owned links remain release blockers.

### Page definition of done

A public page is complete only when:

- its audience, kind, scope, prerequisites, and stability are explicit;
- its claims are tied to owning source/tests or generated contracts;
- it contains no Last Frontier/TLA dependency;
- commands and snippets pass the declared harness;
- expected success and common failure signals are shown;
- related tutorial/how-to/reference/explanation pages are linked without duplicating ownership;
- generated inventories are not restated as manual counts;
- English review is complete and Russian freshness is known;
- redirects preserve any replaced public URL;
- the docs site and standalone GitHub rendering both work.

## Execution roadmap

### Phase 0 - Governance, inventory, and architecture decisions

- [x] Approve audiences, public surfaces, stability labels, and source-of-truth order.
- [x] Add a machine-readable document/source ownership manifest.
- [x] Classify every current page by audience, Diataxis kind, public/internal status, locale scope, and destination path.
- [ ] Record every engine reference currently owned only by Last Frontier or TLA.
- [x] Write ADRs that record the fixed GitHub Pages/Jekyll publishing contract, bilingual layout, navigation/search implementation, generated-reference format, documentation release/version policy, and example-repository ownership. ADR 0001 owns Pages/locale layout, ADR 0002 owns API stability, ADR 0003 owns manifest-backed AI delivery, ADR 0004 owns site navigation/search, ADR 0005 owns public example repositories, and ADR 0006 owns current/release documentation channels plus locale/route migration.
- [x] Preserve the current public URL map and define redirects before moving files. `Docs/generated/document-routes.json` records every current path, canonical future owner, planned locale pair, and required durable Markdown redirect.
- [ ] Establish owners and review requirements for runtime, scripting, content, tools, platforms, release, and translation.

Exit gate: every current page and identified gap has one owner, disposition, priority, and target location.

### Phase 1 - Standalone independence and truthful entry points

- [x] Add standalone link/path checks and reject all relative links that escape the engine root.
- [x] Remove Last Frontier task names, binary names, config files, tests, and scripts from normative engine procedures.
- [x] Split project-specific sections out of debugging, Web, Android, mapper, updater, nullability, and testing pages.
- [ ] Replace project examples with links to tagged public example repositories only after those examples exist.
- [x] Correct stale source paths and replace generated/manual counts with checked inventory data.
- [x] Replace the `TUTORIAL.md` placeholder with a tested lesson and make `PUBLIC_API.md` an explicit, CI-checked route to generated-reference work.
- [ ] Retire the legacy root entry paths through redirects after their localized replacement pages ship.
- [ ] Make `README.md`, `README.ru.md`, `Docs/README.md`, `Docs/en/index.md`, `Docs/ru/index.md`, and `AGENTS.md` route humans and agents to different but consistent entry points.
- [ ] Move the historical expansion plan, backlog, research template, and verification reports under `_meta/` after redirect coverage exists.

Exit gate: a standalone engine clone has zero root-escaping local links, zero missing declared source paths, zero placeholders presented as usable docs, and no project file required to follow an engine procedure.

### Phase 2 - Documentation platform and fast quality loop

- [ ] Record the current GitHub Pages source branch/folder and DNS ownership; keep root `CNAME` set to `fonline.ru` and validate it in CI.
- [x] Pin a GitHub Pages-compatible Jekyll validation environment where needed, and add a local preview command plus CI build without creating a separate docs application. Ruby `3.3.4`, `github-pages` `232`, the official Pages build action, and the `_site` review artifact are wired; the first CI render remains to be observed.
- [x] Extend `_config.yml` and add only the layouts, data, theme overrides, and local assets needed to render the Markdown corpus professionally. The custom layout consumes generated manifest-owned navigation and keeps technical prose in Markdown.
- [ ] Implement bilingual navigation, static search, stable IDs/permalinks, source links, legacy redirect tests, and rendered `_site` artifacts for pull requests. Current English navigation/search, rolling source links, version/locale policy, stable target IDs, route/redirect tests, and the artifact job are implemented; physical `Docs/en` / `Docs/ru` pages, migrated permalinks, language switching, and production artifact inspection remain.
- [ ] Add format, terminology, link, anchor, source-path, and accessibility checks.
- [ ] Add deterministic diagram generation and an owned asset directory.
- [ ] Publish current documentation at `fonline.ru` through the existing GitHub Pages route while retaining commit-addressable CI artifacts.
- [x] Keep the docs job independent from a full native build where possible; schedule expensive snippet/example/platform checks separately.

Exit gate: the GitHub Pages-compatible build succeeds from repository Markdown, every documentation PR receives a rendered `_site` artifact, all fast gates finish cleanly, and the production route still resolves through `CNAME` to `fonline.ru`.

### Phase 3 - Public API and generated reference

- [x] Define and document the public API/stability policy.
- [x] Emit the canonical API JSON model from existing codegen parsing. `BuildTools/docs_api.py` serializes the same parsed tag objects as `codegen.py` into checked `Docs/generated/api.json`; its declared scope is `engine-native-codegen`, with excluded domains listed explicitly.
- [x] Generate script method/property/event/remote/enum/type reference pages. Native codegen symbols render from `api.json`; project-authored remote calls render as a separate project-owned JSON/Markdown supplement from paired `MetadataBaker` outputs through `BuildTools/docs_metadata.py`.
- [x] Generate settings, CMake options, stage helpers/hooks, main/helper CLI, native-extension, prototype-format, map-format, model-format, text-format, effect-format, image-format, particle-format, font-format, and package reference pages. Native codegen settings, runtime-consumed CMake/native-extension models, executable-parser-backed main/helper CLI models, source-backed prototype/map/model/text/effect/image/particle/font models, and the runtime-consumed package model are generated and checked.
- [ ] Add source comments/metadata required to document stable symbols accurately. The docs-only `///@ ApiContract` grammar, lifecycle validation, exact/family selectors, provenance, and hash-invariance tests are implemented; 2,459 symbols remain default-internal and source descriptions/examples still need owner review before any broad public promise.
- [x] Add generated-contract diff and breaking-change classification. `BuildTools/docs_contract_diff.py` compares native API, CMake, main CLI, package, helper CLI, native-extension, prototype-format, map-format, model-format, text-format, effect-format, image-format, particle-format, and font-format models with the PR/push base revision; the native layer preserves overload/stability semantics; aggregate JSON/Markdown artifacts require exact shared-ledger dispositions for public breaks and model/parser contract changes.
- [x] Replace manual `ScriptMethodsMap.md` counts with generated indexes and task-oriented overview prose.
- [ ] Replace obsolete `PUBLIC_API.md` content with the generated and policy-backed public contract.

Exit gate: 100 percent of public symbols and settings appear in deterministic reference output, and a breaking public change cannot merge without an explicit disposition.

### Phase 4 - Canonical starter and first-run tutorials

- [ ] Build and publish `fonline-project-template` from an engine-owned scaffold. The source scaffold, registry, governance overlay, exact-pin/current-Engine validator, generated reference, private source-staged repository, and initial pinned/current CI evidence are complete; protected settings, public visibility, tag, and artifact remain owner-gated.
- [x] Test Windows and Linux prerequisites and quickstart commands on clean CI images. Pinned run `29739863448` is green on `windows-latest` and `ubuntu-latest`; current-Engine run `29740066760` is green on Ubuntu.
- [x] Write and locally verify the first configure/build/bake/headless-server tutorial against the engine-owned scaffold.
- [ ] Write first-server/client, first-content-change, and first-test tutorials.
- [x] Include expected logs, artifact paths, and recovery for the first headless milestone.
- [ ] Add a support matrix with verified, experimental, and unsupported combinations.
- [ ] Measure time-to-first-success with developers who did not work on the engine.

Exit gate: a new developer can reach a successful server/client or documented headless milestone in under 30 minutes without another game repository.

### Phase 5 - Scripting, content, tools, and best practices

- [x] Write the complete scripting guide and script concurrency/lifecycle guide.
- [ ] Write config, resource-pack, subconfig, metadata, migration, and generated-content guides.
- [ ] Write engine-owned format references from current parsers, registries, and bakers. Prototype, map, model, text, effect, image, particle, and font contracts are complete; GUI/dialog ownership plus audio and video remain.
- [x] Publish the prototype authoring guide and generated syntax/property/validation reference from `ProtoBaker`, configuration/property parsers, metadata, and focused tests.
- [x] Publish the map authoring/baking/runtime guide and generated section, placement, ownership, property, and validation reference.
- [x] Publish the text/localization guide and generated raw-pack, language, prototype-text, runtime, rendering, and validation reference.
- [x] Publish the `.fofx` effect guide and generated syntax, render-state, resource, baking, runtime, and validation reference.
- [x] Publish the FOFNT/BMFont authoring, binding, layout, rendering, diagnostics, and generated contract reference from `FontManager`, settings/enums, raw-copy delivery, bundled descriptors, and focused tests.
- [ ] Write remaining task guides for audio, video, and tool-specific workflows. Particle authoring/runtime is complete; the broader ParticleEditor manual still needs versioned screenshots.
- [x] Publish a source-backed guide for `.fo3d` animation tuples, `AnimSpeed`, one-step aliases, effective duration metadata, and typed common/client lookup.
- [x] Move and verify the TLA engine-owned 2D sprite root-motion material against current `ImageBaker`, `SpriteSheet`, `MovingContext`, and `CritterHexView` source; keep it separate from `.fo3d` skeletal animation.
- [x] Publish the complete current `.fo3d` model-description contract with exact parser-token coverage, FBX/OBJ inputs, layers, attachments, transforms, materials, cuts, runtime composition, generated reference, and visible-validation boundary.
- [ ] Extract reusable Last Frontier practices through the promotion gates above.
- [ ] Publish mapper/editor/asset/particle-tool manuals with versioned screenshots.
- [ ] Classify dialogs, GUI generators, gameplay-test harnesses, analyzers, formatters, and audits as engine, companion, or project-owned before documenting them.
- [x] Publish the native-extension guide and generated interface reference.
- [ ] Publish the broader project-local dependency guide.

Exit gate: every supported authored input and developer tool has a reference page, at least one how-to, a validation route, and an honest ownership label.

### Phase 6 - Debugging, testing, packaging, operations, and migration

- [ ] Rewrite platform procedures against the starter/tutorial projects.
- [ ] Document test-boundary selection, sanitizers, coverage, native/script debugging, and profiling.
- [ ] Document reproducible packaging for desktop, Web, Android, iOS, and server/service targets that are currently supported.
- [ ] Document updater, signing, secrets, database operations, recovery, and security boundaries.
- [ ] Add engine-upgrade, save migration, network compatibility, and client-runtime ABI guides.
- [ ] Validate every support claim on the declared host/target matrix.

Exit gate: a release engineer can build, diagnose, package, and upgrade a minimal game using only engine docs and public examples.

### Phase 7 - Public tutorial and showcase repositories

- [ ] Populate and publish the reserved private `fonline-minimal-multiplayer` repository, then bind every tutorial step to a tag.
- [ ] Populate and publish the reserved private `fonline-content-showcase` repository with licensed assets, screenshots, and a Web build.
- [ ] Populate and publish the reserved private `fonline-native-extension-sample` repository after extension contracts stabilize.
- [ ] Add scheduled current-engine compatibility runs and automated update PRs. The weekly/manual workflow exists and its first manual run is green; automated reviewed update PRs remain.
- [ ] Add cross-repository link, revision, asset-license, and snippet checks. The shared local validator now enforces registry identity, exact pins, required files, unresolved placeholders, and asset provenance; live cross-repository checks await the first published repository.

Exit gate: all required example repositories pass their pinned and current-engine compatibility gates, and every public asset has auditable provenance.

### Phase 8 - AI delivery and evaluation

- [x] Generate `llms.txt`, bounded full-context output, docs manifest, and machine-readable references. `BuildTools/docs_ai_delivery.py` projects the source manifest and canonical Markdown into root `llms.txt`, a 1.25 MiB-capped `llms-full.txt`, and public `docs-manifest.json`; canonical API/CMake/CLI/helper/native-extension/prototype/map/model/package/image JSON models remain separately linked rather than duplicated into the bundle.
- [ ] Add clean Markdown endpoints to the published site.
- [ ] Build a versioned task/question evaluation set for architecture, scripting, content, debugging, migration, and release.
- [ ] Evaluate at least two model families against standalone engine docs only.
- [ ] Fix retrieval ambiguity, duplicated ownership, missing prerequisites, and unsupported assumptions revealed by the evaluation.
- [x] Keep AI entry points generated from the same navigation/manifest as the human site. Focused tests, `--check`, standalone validation, and the documentation workflow reject path/filter/hash/budget/output drift.

Exit gate: agents select the correct owning source/version and complete at least 90 percent of the agreed representative tasks without Last Frontier-specific assumptions.

### Phase 9 - Russian mirror and production launch

- [ ] Freeze the English public information architecture and stable IDs for the translation pass. The generated route inventory and all present target paths are frozen by ADR-0006; complete the remaining English coverage and first execution slice before declaring the translation-pass freeze.
- [ ] Create the Russian locale tree with one-to-one document coverage.
- [ ] Create and review the bilingual glossary and API-description translation catalog.
- [ ] Translate tutorials first, then how-tos, reference descriptions, explanation, troubleshooting, and contributor/operator guides.
- [ ] Add translation hash/parity gates and language-preserving links.
- [ ] Add `README.ru.md` and language selection from both repository and site entry points.
- [ ] Run native-speaker review, code/identifier integrity checks, accessibility checks, and bilingual search evaluation.
- [ ] Remove the temporary `translation-pending` allowance from production branches.
- [ ] Publish the production site and monitor owned/external links, search failures, and common zero-result queries.

Exit gate: every public human document is available in English and Russian, both locales pass the same site/link/snippet gates, and no stale translation is shipped as current.

## Program metrics

Production launch requires all of these:

- 0 local links escaping the engine repository.
- 0 broken owned links or missing declared source paths.
- 0 manually maintained API/test/settings counts in public prose.
- 100 percent generated coverage for declared public symbols and engine settings.
- 100 percent of stable public groups have descriptions and tested examples.
- 100 percent of supported authored formats have reference, validation, and failure guidance.
- 100 percent of public English document IDs have current Russian mirrors.
- 100 percent of normative snippets are parsed, compiled, baked, or executed by CI.
- Starter time-to-first-success at or below 30 minutes on verified Windows and Linux hosts.
- At least 90 percent success on the agreed human usability tasks and AI evaluation tasks.
- Every public example artifact identifies its engine revision and asset provenance.
- Every modeled public breaking change produces an aggregate contract diff, migration guidance, and release note.

## Governance and maintenance

- Documentation is part of the definition of done for behavior and public-contract changes.
- CODEOWNERS route docs by subsystem; public API, migration, security, and release pages require an owning engineer review.
- Generated reference is updated in the same change as source annotations.
- Example repositories consume pinned engine revisions and scheduled compatibility PRs.
- A quarterly source-coverage audit checks ownership mappings and unreferenced public surfaces; daily work relies on change-scoped CI, not calendar-only reviews.
- Search analytics and issue labels feed a visible documentation backlog; failed user journeys outrank word-count goals.
- The verification report records semantic review at an engine commit, not merely a date and link check.
- Old pages remain as redirects for at least one supported release line.
- No documentation change is considered complete while staging contains accidental files or generated output differs from its source.

## Risks and controls

| Risk | Control |
|---|---|
| Engine refactoring invalidates prose quickly | Generate reference, tie pages to source owners, use aggregate contract diffs, and version only supported releases |
| Last Frontier content leaks into engine docs | Standalone root-boundary CI, ownership review, and tagged public examples |
| TLA legacy patterns become accidental recommendations | Require engine-contract and cross-project validation before promotion |
| Public API promises freeze unstable internals | Use explicit stability labels and delay stable promises until release policy exists |
| Translation doubles churn too early | Stabilize English structure first, then translate; track hashes and block stale production releases |
| Example repositories drift | Pin revisions, share workflows, run scheduled compatibility, and automate update PRs |
| Generated reference is complete but unreadable | Add task indexes, curated examples, descriptions, search aliases, and usability evaluation |
| Site tooling becomes a new maintenance burden | Keep Markdown canonical, use only GitHub Pages-supported Jekyll features, pin the validation environment, and avoid a parallel site framework or generated-content tree |
| Asset licensing blocks public showcases | Use CC0/public-domain/purpose-made assets and require provenance manifests before merge |
| Platform claims exceed tested reality | Publish a status matrix and require a reproducible gate for every `verified` claim |

## First execution slice

Start with the smallest work that changes the reliability model before writing more prose:

1. [x] Create the document/source ownership manifest and page classification inventory.
2. [x] Add standalone docs CI with root-escape, link, anchor, source-path, placeholder, and generated-inventory checks.
3. [x] Remove the 33 root-escaping links and split the five most project-dependent pages.
4. [x] Replace manual API/test/settings counts with generated inventory data.
5. [x] Write the public-contract ADR and the ADR recording the existing GitHub Pages/Jekyll publication contract and bilingual layout.
6. [x] Create the engine-owned starter scaffold and prove one clean Windows/Linux headless path. Pinned Windows/Linux and current-Engine Linux routes are green on clean GitHub runners.
7. [x] Use that path to replace the placeholder tutorial with the first tested lesson.

Do not begin full Russian translation or broad prose migration before this slice is green; otherwise the program will duplicate stale structure and project dependencies into both locales.

## Research basis

Repository evidence reviewed for this plan:

- Current engine `README.md`, `AGENTS.md`, `TUTORIAL.md`, `PUBLIC_API.md`, all `Docs/*.md`, source READMEs, `BuildTools/README.md`, docs configuration, and validation workflow.
- Current source/build surfaces under `Source/`, `BuildTools/`, `Resources/`, `.github/workflows/`, metadata annotations, `Settings.inc`, test suites, the former `BuildTools/validation-project`, and its current replacement at `Examples/MinimalProject`.
- Last Frontier documentation and workflows as the primary mature embedding-project sample.
- Public TLA repository, especially its README, build root, agent/style docs, animation notes, and recent refactoring history.

External documentation-design references:

- [Diataxis documentation framework](https://diataxis.fr/)
- [About GitHub Pages and Jekyll](https://docs.github.com/en/pages/setting-up-a-github-pages-site-with-jekyll/about-github-pages-and-jekyll)
- [Adding a theme to a GitHub Pages site using Jekyll](https://docs.github.com/en/pages/setting-up-a-github-pages-site-with-jekyll/adding-a-theme-to-your-github-pages-site-using-jekyll)
- [Managing a custom domain for a GitHub Pages site](https://docs.github.com/en/pages/configuring-a-custom-domain-for-your-github-pages-site/managing-a-custom-domain-for-your-github-pages-site)
- [llms.txt proposal](https://llmstxt.org/)

These external sources guide information architecture and delivery. Engine source and tests remain authoritative for FOnline behavior.
