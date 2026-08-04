# FOnline Production Documentation Program

- **Status:** In progress
- **Baseline date:** 2026-07-10
- **Engine baseline:** `67ee893ae721d149cd44ff314abd8036adfd3821`
- **Last Frontier baseline:** `805caa79976b7cf4f81e46e1cf9ca0f1ea96ba43`
- **TLA baseline:** `b603d8fdbc2b2f89f233b2a1938686ead9d8d480`
- **Latest reconciliation:** 2026-08-04, Engine through `fac978a67d1e601eb77389e8dc562d7e511705a0`, Last Frontier through `50b8cb4e9ec706887708640e4474b9f8281097d8`, and TLA through `b603d8fdbc2b2f89f233b2a1938686ead9d8d480`
- **Predecessor:** [DocumentationExpansionPlan.md](_meta/DocumentationExpansionPlan.md), which remains the completed historical plan for the first source-coverage pass.

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

## Latest reconciliation checkpoint

The 2026-07-30 update audited 20 incoming Engine commits and 191 incoming Last Frontier commits against preserved pre-update branches. It reconciled the SQLite build option, removed the retired generic Editor package/option, added the standalone animation and particle viewer targets, expanded effect resources and variants, updated SPARK/Effekseer capabilities and automatic bounds, corrected `.fo3d` particle attachments to baked `.spk`/`.efk` resources, and regenerated API, inventory, contract, site, route, example, and AI-delivery artifacts.

A follow-up fetch on 2026-07-31 found no newer `origin/master` or embedding
project `origin/main` revision. The source audit classified the current Last
Frontier `.fodlg` parser, baker, runtime, editor, and audit as project-owned;
Engine owns the extension and baking primitives, not that dialog format.
The same-day publication audit built the repository Markdown with the pinned
GitHub Pages stack and validated the resulting HTML: all 175 public routes, 30
static endpoints, and 20,250 local references passed the post-build gate.
A refreshed build after adding the AI evaluation contract passes all 176
current routes, 31 static endpoints, and 20,539 local references.
The following snippet-validation build passes all 177 current routes, 32
static endpoints, and 20,831 local references.
The profiling-guide build passes all 178 current routes, 39 static endpoints,
and 21,135 local references plus all 356 desktop/mobile browser checks.
The focused-viewer-guide build passes all 179 current routes, 39 static
endpoints, and 21,443 local references plus all 358 desktop/mobile browser
checks.

The current checked models include an eighteenth experimental AiControl protocol domain with 49 source-backed entries, a standard-library reference client, malformed-peer tests, and a runnable 12-check sample. The generated support model distinguishes ten build-, smoke-, and source-capability profiles. The pinned external-project inventory classifies 30 concerns from exact Last Frontier/TLA paths; generated evidence remains authoritative for current disposition and source counts. Locale groups 45 through 48 moved the generated native script API, cross-domain public contract index, Generated API and Metadata guide, native Essentials reference, package reference, support matrix projection, public-example registry, Configuration and Data Sources reference, and Tools index to canonical EN/RU routes while retaining their former paths as durable pointers. All 197 physical locale pairs are present and complete-parity enforcement is active. The stable-locator description catalog also runs in `complete` mode: all 4,917 reader-facing model values across twenty generated domains have current reviewed Russian overlays, including 2,474 native API descriptions and contract notes plus 221 map/prototype property projections. Exact-source translation memory may reuse a reviewed value only when its complete source and normalized hash match and all donors agree; missing, stale, unknown, ambiguous, or structurally unsafe generated-prose translations fail CI. The latest generated localization, description-translation, site, snippet, evaluation, and AI-delivery counts are recorded in [DocumentationVerificationReport.md](_meta/DocumentationVerificationReport.md); their checked artifacts, rather than this roadmap paragraph, remain authoritative. ADR 0003 and ADR 0004 keep fail-closed whole-document budgets at 2 MiB for AI context and 1.75 MiB per locale for search; truncation and silent document removal remain forbidden.

The update does not close the production program. Highest-value remaining work is:

1. finish and review the reserved private example repositories, then perform owner-gated tagging/publication;
2. confirm the landed GitHub Actions artifact and production Pages source, then complete manual assistive-technology/zoom review and migrated-route verification;
3. keep the complete 197-page Russian mirror and all 4,917 generated-description overlays green while completing human language and accessibility review;
4. run and review the task set with at least two independent model families, and add same-domain Markdown aliases only if the publication platform can do so without content duplication;
5. add further runtime/tutorial result media only where task review proves that text, diagrams, and the four reproducible Mapper/SPARK/Direct3D/WebGL captures are insufficient.

The 2026-08-03 external audit reverified all four example repositories as
private: the project template remains source-staged and the other three remain
reservation shells, with no observed passing required checks or public
releases. `https://fonline.ru/` and the legacy `/Docs/` index return HTTP 200,
while representative generated `/Docs/en/` and `/Docs/ru/` routes return 404.
An authenticated 2026-08-02 audit confirms legacy Pages status `built`, source
`master:/`, CNAME `fonline.ru`, and enforced HTTPS. Public DNS resolves to the
four Pages IPv4 addresses and `www` aliases the apex, but the GitHub ownership
challenge TXT record is not observed. The locale corpus, new CI artifact,
migrated-route evidence, and domain-ownership verification still require the
documentation branch and the external administrator workflow.

The 2026-08-03 Content Showcase continuation completed the reusable Web
evidence layer locally. A native host now force-bakes the fixture, the Web lane
builds and verifies an exact six-file raw/ZIP payload, and the isolated
`web-showcase-runtime` target starts the native server and packaged HTTP server
before pinned Chromium checks required responses, client/server lifecycle
markers, a real 1280 x 800 WebGL 2 context, runtime errors, and compositor
pixels. The checked WebGL capture and machine provenance record are local
fixture evidence. Linux native/OpenGL, public-host, remote pinned/current,
security, tag, and visibility gates remain outstanding.

The Last Frontier authentication suite is functionally green after updating stale reconnect-helper references, but its parallel worker logs an `EntitySyncException` while disconnecting an unlogged player during shutdown despite returning success. That runtime harness defect is a separate source/test follow-up and must not be reported as clean validation evidence until fixed.

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
| Playable beginner path is source-ready but unpublished | `Examples/MinimalMultiplayer` and three manifest-owned tutorials cover first client/map interaction, localized content change, layered tests, and native package acceptance; source and package routes are locally Windows-green, while the external repository/tag and clean Linux package evidence do not exist yet | Land an exact reachable Engine pin, stage the reviewed source privately, run pinned/current Windows/Linux smoke and package lanes, then publish immutable lesson tags through the owner-gated process |
| Public API reference is incomplete | `Docs/generated/api.json` schema v2 and seven checked Markdown pages render native-codegen symbols plus explicit/default `ApiContract` provenance; project remote calls have a baked supplement; CMake, the main BuildTools CLI, package declarations/payloads, helper CLIs, native extensions, prototype format, map format, model format, text format, effect format, image format, particle format, font format, audio, video, GUI runtime, and AiControl protocol have separate source-backed models/references; all eighteen generated domains participate in base-revision diff enforcement, while broad owner-reviewed classifications remain | Extend coverage through each owning contract and review classifications under release policy |
| Standalone engine boundary was broken | The original audit found 33 root-escaping links and 17 project-dependent docs; `docs_validate.py` now rejects escapes and the maintained corpus passes standalone validation | Keep the gate required and use tagged HTTPS links for future cross-project examples |
| Several pages mixed engine and project workflows | Debugging, updater, Web, mapper, Android, nullability, and testing pages were split into reusable engine procedures on 2026-07-10 | Reintroduce concrete recipes only through tagged public sample repositories |
| Manual inventories drifted | The original 947/18 script-method and 84-test counts disagreed with prose | `Docs/generated/source-inventory.json`, `Docs/generated/api.json`, and the Markdown reference are generated and checked in CI; do not restore manual counts |
| Settings outside codegen remain undocumented | Every native codegen setting has a rendered symbol, every declared project-facing CMake option has a generated type/default/required/category record, the main BuildTools CLI records parser defaults and choices, package declarations have a generated contract, and project-authored `.fomain` composition has a source-backed task guide | Add project-owned generated catalogs where an embedding project introduces settings beyond the Engine schema |
| Build/API reference is incomplete | CMake options/stages/helpers, the main BuildTools CLI, package declarations/targets/platforms/packs/payloads, helper-script CLIs, native-extension roles/hooks/bindings, prototype grammar/properties, map authoring/baking, model composition, text/localization, effect authoring/baking/runtime, image/FOFRM baking/runtime, particle XML/tooling/runtime, bitmap-font binding/layout, audio delivery/playback, experimental video delivery/presentation, and the reusable GUI runtime now have checked generated references and a shared cross-domain revision comparator | Continue with remaining authored-format and project-settings contracts |
| Format authoring is incomplete in Engine | Prototype, map, `.fo3d` model, `.fotxt`/prototype-text, `.fofx` effect, image/FOFRM, `.spark`/`.spk` and `.efkproj`/`.efk` particle, FOFNT/BMFont, WAV/ACM/Ogg audio, experimental Ogg/Theora video, and the reusable AngelScript GUI runtime now have engine-owned guides plus generated source-backed models; model animation and 2D sprite root motion have focused source-backed guides; Engine intentionally has no `.fogui` parser/generator or dialog-tree format | Keep the audited dialog stack project-owned until reusable code and tests move; continue focused tool manuals from each owning implementation |
| Verification did not catch structural drift | Earlier pages cited missing paths and stale numeric claims as verified | Source-path, manifest, local-link/anchor, placeholder, generated-inventory, and eighteen-domain base-revision checks now run in a fast docs job; broader semantic checks remain |
| Publication pipeline is incomplete | Fast standalone checks, complete fenced-snippet inventory/parser coverage, complete EN/RU hash/code/link parity, a GitHub Pages-compatible Jekyll `_site` artifact job, and a lock-file-pinned Chromium/axe browser gate are wired. `docs_snippets.py` requires every normative public fence to pass its declared harness and real shell parsers; `docs_site_artifact.py` validates rendered routes, endpoints, links/fragments/resources, baseline HTML accessibility, and absence of internal plans or generated evidence; `docs-browser/audit.mjs` validates every route in desktop, mobile, and 200-percent reflow profiles plus keyboard/search/theme/copy/navigation scenarios. The local artifact passes 583 routes and 43 public static endpoints; the 1,749-page/profile browser matrix plus 14 interactions and 17 screenshots is green. The first landed CI artifacts and manual production assistive-technology review remain pending | Confirm the first landed artifacts and production source, then complete the human release checks without coupling docs to a native build |
| Current site is a repository landing page | Manifest-backed localized navigation, separate bounded EN/RU search, responsive layout, stable-ID language switching, source links, local theme preference, page table of contents, rolling `master` identity, ADR-0006 route policy, and a generated current/planned route catalog are implemented over Markdown. All 197 physical pairs route under `Docs/en` and `Docs/ru` behind durable legacy pointers; the manifest-owned 200-percent Chromium profile and reviewed Russian 1280 x 1024 screenshot now provide local reflow evidence; final counts belong to the verification report | Confirm the GitHub Pages source and production route and complete production-domain zoom/screen-reader review without changing publication architecture |
| Visual teaching coverage is incomplete | Three source-owned deterministic SVG diagrams explain the Engine/game boundary, generated-content dependency order, and human/AI publication flow. Two 1280x800 Mapper/SPARK screenshots are reproduced by the independent minimal multiplayer fixture, while Content Showcase adds separately checked 1280x800 Direct3D 11 and Chromium WebGL 2 runtime captures with exact source/image/package hashes and recapture triggers. Linux OpenGL and broader task-driven runtime media remain gaps | Add media only where it materially improves a task guide, and capture it from exact reviewed builds with the same provenance/recapture contract |
| No public canonical example game | `Examples/MinimalProject`, `Examples/MinimalMultiplayer`, `Examples/ContentShowcase`, and `Examples/NativeExtensionSample` are source-ready, executable Engine-owned projects. The template is source-staged privately; the other three repositories remain reserved private shells, and none has a public tag/artifact | Complete branch/security gates, pinned/current CI, immutable tags/artifacts, and the explicit owner-authorized visibility transitions |
| Russian mirror still needs human production review | All 197 required physical counterparts are current; the 34-term glossary, normalized hashes, fenced-code parity, language-preserving links, and `complete` enforcement fail closed in CI. The generated-description catalog separately covers all 4,917 reader-facing values across twenty machine-model domains in `complete` mode. Local 200-percent Chromium reflow passes every route and the dedicated Russian screenshot has been visually reviewed | Keep both automated gates green and complete native-speaker, production-domain 200-percent zoom, and representative screen-reader review before production publication |
| AI access is only partly structured | `AGENTS.md`, generated `llms.txt`, bounded context, public manifest, source-ref-pinned clean Markdown URLs, compact site search, a versioned deterministic evaluation set, source inventory, eighteen generated contract models, and aggregate JSON diff reports now provide routing, ownership, stable IDs, signatures, provenance, change classification, and retrieval evidence | Add same-domain Markdown aliases if the publication platform gains a non-duplicating route, then run and review at least two independent model families |

### External project findings

Last Frontier is the richer current source of documentation practices: its top-level `Docs/*.md` set is about 309,000 words and covers content workflows, testing boundaries, generated-file ownership, migrations, nullability, synchronization, packaging, and game-system ownership. It is also intentionally product-specific, so copying pages wholesale would recreate the dependency this program must remove.

TLA is valuable as a second large embedding project and a compatibility check:

- Its [README](https://github.com/cvet/fonline-tla/blob/b603d8fdbc2b2f89f233b2a1938686ead9d8d480/README.md) and [CMakeLists.txt](https://github.com/cvet/fonline-tla/blob/b603d8fdbc2b2f89f233b2a1938686ead9d8d480/CMakeLists.txt) show a real project root, staged engine build, resources, scripts, native extensions, and packaging.
- Its [Animation.md](https://github.com/cvet/fonline-tla/blob/b603d8fdbc2b2f89f233b2a1938686ead9d8d480/Docs/Animation.md) supplied discovery material for the now source-verified 2D contract in [Sprite Root Motion](en/how-to/content/sprite-root-motion.md). TLA's tracked `.fo3d` files are historical integration evidence only: several use removed tokens or unsupported mesh extensions, while current grammar and composition are pinned to Engine source in [Model Format](en/how-to/content/model-format.md) and the [generated model-format reference](en/reference/model-format/index.md).
- Its [Game.engl.fotxt](https://github.com/cvet/fonline-tla/blob/b603d8fdbc2b2f89f233b2a1938686ead9d8d480/Texts/Game.engl.fotxt) demonstrates historical numeric keys, duplicate variants, renderer `@color` tags, project-owned `@arg` formatting, and an incomplete symbolic-key migration. It is integration evidence for compatibility review, not the normative current parser/runtime contract now owned by [Text and Localization](en/how-to/content/text-and-localization.md) and the [generated text-format reference](en/reference/text-format/index.md).
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
| Last Frontier/TLA AiControl bridges, scripts, adapters, and docs | Shared NDJSON framing, authentication, bounded command/event lifecycle, completion correlation, and native/MCP integration boundaries | [AiControl Protocol](en/how-to/ai-control-protocol.md), generated protocol reference, standard-library client, and runnable transport sample | Observation schemas, actions, administrator tools, launch orchestration, readiness semantics, and `lf_*`/`tla_*` MCP namespaces |
| Last Frontier `NativeExtensions.md`, `ProjectThirdPartyMaintenance.md` | Role-scoped native sources, hooks/exports, dependency patching and packaging boundaries | [NativeExtensions.md](NativeExtensions.md) and [ProjectDependencies.md](ProjectDependencies.md) | Steam/Sentry/dialog/web integrations and vendored project libraries |
| Last Frontier `GuiSystem.md`, `Dialogs.md` | Runtime GUI behavior, authoring ergonomics, generator ownership, declarative validation patterns | [GUI Runtime](en/how-to/runtime/gui.md) for reusable runtime behavior; [Embedding FOnline](en/how-to/build/embedding-project.md) and [NativeExtensions.md](NativeExtensions.md) for the reusable project-format ownership pattern | Current `.fogui`/`.foguischeme` generator, editor, screens, styles, assets, integration scripts, and the complete `.fodlg` schema/parser/baker/runtime/editor/audit until reusable code moves |
| Last Frontier `ContentWorkflow.md`, `MapAuthoring.md` | Goal-oriented recipes and validation structure | Content how-tos and public tutorial projects | LF quests, AI art pipeline, map kits, content ids |
| Last Frontier `Profiling.md` | Tracy capture model, reproducible scenes, client/server measurement boundaries | Profiling how-to | LF scene ids, tasks, report locations |
| TLA README and CMake root | Independent integration shape and migration pressure | Starter-template conformance tests | TLA targets, packages, content and local policies |
| TLA `Animation.md` and `.fo3d` assets | Historical 2D root-motion behavior and a broad sample of legacy model composition | [Sprite Root Motion](en/how-to/content/sprite-root-motion.md), [Model Format](en/how-to/content/model-format.md), and [Model Animation](en/how-to/content/model-animation.md), all re-derived from current source | TLA assets, removed tokens, unsupported mesh extensions, enum policy, and gameplay timing remain non-normative |
| TLA `ScriptStyle.md` and refactoring plan | A second sample of module ownership and generated-file discipline | Best-practice comparison input | Russian-comment policy, refactoring status, legacy exceptions |

### Engine work that documentation alone cannot solve

Some gaps require reusable engine artifacts before the corresponding documentation can be truthful:

- The public project template source, generator, and private candidate now exist; protected settings, immutable release tags/artifacts, visibility transition, and measured newcomer evidence still require external repository and owner action.
- The codegen-owned API model, renderer, and eighteen-domain public index now exist; all 2,472 symbols have source-owned descriptions, and an inventory-pinned scope classifies 2,471 as revision-bound `experimental` while keeping the development debugger hook `internal`.
- Structured `ApiContract` metadata supports stability, version, deprecation, replacements, examples, provenance, and an owner-reviewed `scope:native-codegen` selector pinned by symbol count plus stable-ID digest. Broad `stable` promotion remains gated on supported release lines and explicit owner policy rather than reachability.
- Generated settings, CMake, CLI, package, native-extension, and authored/runtime format models now exist and are aggregate-diff gated; future Engine surfaces must enter the same model/disposition process.
- Nullability, smart-pointer, exception-safety, thread-safety, and local-variable analyzers are Engine-owned; project wrappers and project-specific baselines remain external evidence.
- The reusable integration/gameplay runner and guide are now Engine-owned and prove synthetic failure/timeout behavior plus real Minimal Multiplayer networking. Last Frontier pipeline runners, script suite registries, and TLA system/quest matrices remain project-owned.
- A reusable extraction and publication path for dialog generation, formatters,
  migration guards, and content audits if more games adopt them. The current
  Last Frontier dialog stack and GUI generator are intentionally project-owned;
  only the reusable Engine extension/baking primitives and GUI runtime are
  Engine contracts.
- Tagged engine releases and a support policy; documentation versioning cannot compensate for an undefined release contract.
- The complete source-ready Content Showcase now owns thirteen byte-verified CC0 project-original assets, deterministic generation, native baking/runtime smoke, a Direct3D 11 capture contract, explicit budgets, an exact Web package contract, and a locally green isolated Chromium/WebGL 2 runtime capture. Linux native/OpenGL evidence, exact-tag artifacts, and publication remain outstanding.
- Documentation build, generation, snippet, site, AI, contract, and external-evidence commands are exposed through BuildTools/CI; landed Pages and cross-repository artifacts remain external evidence gates.

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
8. Add schema/reference generators for formats whose parsers or registries expose enforceable structure. Prototype, map, model, text, effect, image, particle, font, and audio models now derive and validate their owning source surfaces; remaining formats need owning generators or focused hand-written grammar tests.
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
- Mapper, SPARK/Effekseer authoring, AnimationViewer, and ParticleViewer user manuals with versioned screenshots.
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

The 2026-07-31 dialog audit chose disposition 3 for the current Last Frontier
stack: it remains project-specific. [Embedding FOnline](en/how-to/build/embedding-project.md)
and [NativeExtensions.md](NativeExtensions.md) document the reusable ownership
and composition pattern; FOnline has no `.fodlg` contract. Revisit the decision
only when reusable implementation, fixtures, compatibility policy, and tests
are ready to move into Engine or a versioned companion.

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

Current Engine-owned source: `Examples/ContentShowcase` is source-ready and independent of Last Frontier/TLA. It includes deterministic TGA/WAV generation, FOFRM animation with root-motion metadata, `.fofx` plus SPARK particles, a compact prototype/map/script scene, thirteen byte-verified project-original CC0 assets, explicit source/runtime budgets, a two-scenario native smoke, and a twelve-sample region-validated Direct3D 11 capture. `win64-showcase-smoke` passes from the isolated BuildTools copy. The Emscripten 6.0.3 route also force-bakes with a native host, verifies the exact six-file raw/ZIP package including `Resources.data`, runs it in Chromium against the native server, and records a checked 1280 x 800 WebGL 2 capture. Linux native/OpenGL, public-host, remote pinned/current, and release evidence remain outstanding.

### 4. `cvet/fonline-native-extension-sample`

Purpose: an advanced, still-minimal example of project-native C++ integration.

Required contents:

- One lifecycle hook, one script export, one role-specific source, and one focused native test.
- CMake wiring, pointer/nullability conventions, ABI boundaries, and compatibility impact.
- No game-specific service integration.

This repository is Priority 1 and starts only after the template and script API reference are stable.

### Shared repository policy

- The policy is now source-controlled in `Examples/PublicRepositories.json`, explained by `Docs/PublicExampleRepositories.md` and ADR 0005, projected to generated JSON/Markdown, and enforced by `BuildTools/docs_examples.py`. `Examples/PublicRepositoryTemplate` owns the common governance/workflow/provenance files. Candidate materialization is deterministic, preserves the deep source guide as `TUTORIAL.md`, excludes declared local outputs, pins metadata/provenance to one exact Engine SHA, and refuses destructive output reuse. All four Engine-owned examples are source-ready. Their remote repositories were created privately on 2026-07-20; only the template candidate is source-staged and the other three remotes remain reserved. Public visibility and release remain separate administrator-gated transitions.
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
- static full-text search over prose and generated API symbols, with a compact index derived from Markdown (implemented under a reviewed 1.75 MiB per-locale budget);
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
| `docs-snippets` | Implemented: every public/current/human fence is inventoried; Shell, CMake, C++, AngelScript, config, data, and evidence blocks pass their declared static harness, with Bash/PowerShell checked by real no-execution parsers in a separate CI job |
| `docs-examples` | Pinned starter/tutorial contracts still configure, bake, build, and smoke-test |
| `docs-i18n` | Document IDs mirror correctly and translation freshness is known |
| `docs-contract-diff` | Public/model-contract removals and shape changes have stability, migration, and release-note disposition |
| `docs-accessibility` | Implemented automated gate: pinned Chromium and axe-core cover every route at desktop, mobile, and manifest-owned 200-percent reflow widths for WCAG 2.2 A/AA, runtime/resources, responsive containment, page overflow, keyboard skip/search/theme/copy, and compact/mobile focus behavior; clipped color-contrast incomplete nodes pass a computed effective-color/luminance fallback or fail the route, while raw axe provenance remains in the report |

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
- [x] Record every engine reference currently owned only by Last Frontier or TLA. `BuildTools/ExternalProjectEvidence.json` now classifies 30 reusable/project concerns across 180 source references in exact Last Frontier `50b8cb4` and TLA `b603d8f` snapshots; the generator records 24 promoted, 2 boundary-owned, 1 promotion-candidate, and 3 project-owned decisions and can prove every source with `git cat-file`.
- [x] Write ADRs that record the fixed GitHub Pages/Jekyll publishing contract, bilingual layout, navigation/search implementation, generated-reference format, documentation release/version policy, and example-repository ownership. ADR 0001 owns Pages/locale layout, ADR 0002 owns API stability, ADR 0003 owns manifest-backed AI delivery, ADR 0004 owns site navigation/search, ADR 0005 owns public example repositories, and ADR 0006 owns current/release documentation channels plus locale/route migration.
- [x] Preserve the current public URL map and define redirects before moving files. `Docs/generated/document-routes.json` records every current path, canonical future owner, planned locale pair, and required durable Markdown redirect.
- [x] Establish owners and review requirements for runtime, scripting, content, tools, platforms, release, and translation. `Docs/documentation-manifest.json` now defines eleven owners plus exact scope, required evidence, and co-review triggers; validation requires one policy for every owner, and `localization` separately owns documentation parity/review rather than format mechanics.

Exit gate: every current page and identified gap has one owner, disposition, priority, and target location.

### Phase 1 - Standalone independence and truthful entry points

- [x] Add standalone link/path checks and reject all relative links that escape the engine root.
- [x] Remove Last Frontier task names, binary names, config files, tests, and scripts from normative engine procedures.
- [x] Split project-specific sections out of debugging, Web, Android, mapper, updater, nullability, and testing pages.
- [ ] Replace project examples with links to tagged public example repositories only after those examples exist.
- [x] Correct stale source paths and replace generated/manual counts with checked inventory data.
- [x] Replace the `TUTORIAL.md` placeholder with a tested lesson and replace `PUBLIC_API.md` with a generated, CI-checked cross-domain contract index.
- [ ] Retire the legacy root entry paths through redirects after their localized replacement pages ship.
- [x] Make `README.md`, `README.ru.md`, `Docs/README.md`, `Docs/en/index.md`, `Docs/ru/index.md`, and `AGENTS.md` route humans and agents to different but consistent entry points.
- [x] Move the historical expansion plan, backlog, research template, and verification reports under `_meta/` after redirect coverage exists. Canonical internal records now live under `Docs/_meta/`; old repository paths are source-only pointers and the Pages artifact gate rejects published internal documents or generated models.

Exit gate: a standalone engine clone has zero root-escaping local links, zero missing declared source paths, zero placeholders presented as usable docs, and no project file required to follow an engine procedure.

### Phase 2 - Documentation platform and fast quality loop

- [ ] Record the current GitHub Pages source branch/folder and DNS ownership; keep root `CNAME` set to `fonline.ru` and validate it in CI. The source is now authenticated and recorded as legacy `master:/` with status `built` and HTTPS enforced. Public DNS routing is confirmed, but `_github-pages-challenge-cvet.fonline.ru` has no observed TXT record, so ownership verification remains open.
- [x] Pin a GitHub Pages-compatible Jekyll validation environment where needed, and add a local preview command plus CI build without creating a separate docs application. Ruby `3.3.4`, `github-pages` `232`, the official Pages build action, and the `_site` review artifact are wired. A clean production-mode local render passes; the first landed CI artifact remains to be observed.
- [x] Extend `_config.yml` and add only the layouts, data, theme overrides, and local assets needed to render the Markdown corpus professionally. The custom layout consumes generated manifest-owned navigation and keeps technical prose in Markdown.
- [ ] Implement bilingual navigation, static search, stable IDs/permalinks, source links, legacy redirect tests, and rendered `_site` artifacts for pull requests. Locale-aware navigation, independent bounded EN/RU indexes, stable-ID language switching, rolling source links, route/redirect tests, all forty-eight physical migration groups, and complete semantic overlays are implemented. Only the first landed CI artifact remains to be inspected before this item closes.
- [x] Add format, terminology, link, anchor, source-path, and accessibility checks. Source Markdown has terminology, local-link, anchor, source-path, front-matter, structure, and 100-percent normative fenced-snippet parser gates. Rendered HTML adds canonical route, endpoint, doctype/language/title/viewport, landmark/heading, skip-link, image-alt, button-name, duplicate-ID, fragment, and resource checks. The pinned browser gate now covers all 583 rendered English/legacy/Russian routes in desktop, mobile, and 200-percent reflow Chromium profiles (1,749 page checks) with WCAG 2.2 A/AA axe rules, layout/overflow/runtime checks, 14 interaction profiles, and 17 screenshots. The Russian 200-percent screenshot is visually reviewed; production-domain zoom and representative screen-reader review remain release activities rather than automated claims.
- [x] Add deterministic diagram generation and an owned asset directory. `BuildTools/DocumentationDiagrams.json` owns three source-grounded teaching diagrams; `BuildTools/docs_diagrams.py` emits accessible desktop/mobile SVG variants plus a hashed provenance catalog, and focused/aggregate/site/browser gates reject drift, missing assets, undecoded images, or missing alt/caption ownership.
- [ ] Publish current documentation at `fonline.ru` through the existing GitHub Pages route while retaining commit-addressable CI artifacts.
- [x] Keep the docs job independent from a full native build where possible; schedule expensive snippet/example/platform checks separately.

Exit gate: the GitHub Pages-compatible build succeeds from repository Markdown, every documentation PR receives a rendered `_site` artifact, all fast gates finish cleanly, and the production route still resolves through `CNAME` to `fonline.ru`.

### Phase 3 - Public API and generated reference

- [x] Define and document the public API/stability policy.
- [x] Emit the canonical API JSON model from existing codegen parsing. `BuildTools/docs_api.py` serializes the same parsed tag objects as `codegen.py` into checked `Docs/generated/api.json`; its declared scope is `engine-native-codegen`, with excluded domains listed explicitly.
- [x] Generate script method/property/event/remote/enum/type reference pages. Native codegen symbols render from `api.json`; project-authored remote calls render as a separate project-owned JSON/Markdown supplement from paired `MetadataBaker` outputs through `BuildTools/docs_metadata.py`.
- [x] Generate settings, CMake options, stage helpers/hooks, main/helper CLI, native-extension, prototype-format, map-format, model-format, text-format, effect-format, image-format, particle-format, font-format, audio, video, GUI runtime, and package reference pages. Native codegen settings, runtime-consumed CMake/native-extension models, executable-parser-backed main/helper CLI models, source-backed prototype/map/model/text/effect/image/particle/font/audio/video/GUI models, and the runtime-consumed package model are generated and checked.
- [x] Add source comments/metadata required to document classified symbols accurately. All 2,472 native symbols have source-owned English descriptions and complete reviewed Russian overlays. The docs-only `///@ ApiContract` grammar validates lifecycle data, exact/family selectors, provenance, and hash invariance; the reserved scope selector additionally pins all stable IDs by count and SHA-256. Owner review classifies 2,471 symbols as revision-bound `experimental` since `2022.1.0.wip` and preserves `Game.BreakIntoDebugger` as exact `internal`; no symbol remains unclassified, and no broad `stable` promise exists before supported release lines do.
- [x] Add generated-contract diff and breaking-change classification. `BuildTools/docs_contract_diff.py` compares all eighteen canonical models with the PR/push base revision; the native layer preserves overload/stability semantics; aggregate JSON/Markdown artifacts require exact shared-ledger dispositions for public breaks and model/parser contract changes.
- [x] Replace manual `ScriptMethodsMap.md` counts with generated indexes and task-oriented overview prose.
- [x] Replace obsolete `PUBLIC_API.md` content with the generated and policy-backed public contract. `BuildTools/docs_public_api.py` now renders canonical EN/RU indexes plus the root durable route from all eighteen checked models, preserves their actual stability labels, reports live native classification/description counts, and is enforced by focused tests, the manifest validator, and CI.

Exit gate: 100 percent of public symbols and settings appear in deterministic reference output, and a breaking public change cannot merge without an explicit disposition.

### Phase 4 - Canonical starter and first-run tutorials

- [ ] Build and publish `fonline-project-template` from an engine-owned scaffold. The source scaffold, registry, deterministic candidate materializer, governance overlay, exact-pin/current-Engine validator, generated reference, private source-staged repository, and initial pinned/current CI evidence are complete; the private candidate must be reconciled with the latest overlay, while protected settings, public visibility, tag, and artifact remain owner-gated.
- [x] Test Windows and Linux prerequisites and quickstart commands on clean CI images. Pinned run `29739863448` is green on `windows-latest` and `ubuntu-latest`; current-Engine run `29740066760` is green on Ubuntu.
- [x] Write and locally verify the first configure/build/bake/headless-server tutorial against the engine-owned scaffold.
- [x] Write first-server/client, first-content-change, and first-test tutorials. Their canonical pages are `Docs/en/tutorials/first-client.md`, `Docs/en/tutorials/first-content.md`, and `Docs/en/tutorials/first-test.md`; reviewed Russian mirrors live under `Docs/ru/tutorials/`, while the three former root-level tutorial pages remain durable legacy pointers. All three are backed by `Examples/MinimalMultiplayer`, its generated complete config, `RunTutorialChecks`, package acceptance target, and local Windows validation.
- [x] Include expected logs, artifact paths, and recovery for the headless and playable milestones.
- [x] Add a support matrix with verified, experimental, and unsupported combinations. The [Support Matrix](en/reference/platforms/support-matrix.md), `BuildTools/SupportMatrix.json`, and the generated projection separate build-gated, process-smoke-gated, source-capable, unsupported, and project/device-qualified claims for ten current profiles.
- [ ] Measure time-to-first-success with developers who did not work on the engine.

Exit gate: a new developer can reach a successful server/client or documented headless milestone in under 30 minutes without another game repository.

### Phase 5 - Scripting, content, tools, and best practices

- [x] Write the complete scripting guide and script concurrency/lifecycle guide.
- [x] Write config, resource-pack, subconfig, metadata, migration, and generated-content guides. [Configure a Game Project](en/how-to/build/project-configuration.md), [Generated Content Workflow](en/how-to/build/generated-content.md), and the [Engine Upgrade Guide](en/how-to/migration/engine-upgrade.md) own the task sequence while generated references retain exhaustive field/symbol inventories.
- [x] Write engine-owned format/runtime references from current parsers, registries, bakers, and reusable modules. Prototype, map, model, text, effect, image, particle, font, audio, experimental video, and GUI runtime contracts are complete. Engine has no declarative GUI parser or dialog-tree implementation; both absent surfaces have explicit ownership boundaries rather than invented Engine format guides.
- [x] Publish the prototype authoring guide and generated syntax/property/validation reference from `ProtoBaker`, configuration/property parsers, metadata, and focused tests.
- [x] Publish the map authoring/baking/runtime guide and generated section, placement, ownership, property, and validation reference.
- [x] Publish the text/localization guide and generated raw-pack, language, prototype-text, runtime, rendering, and validation reference.
- [x] Publish the `.fofx` effect guide and generated syntax, render-state, resource, baking, runtime, and validation reference.
- [x] Publish the FOFNT/BMFont authoring, binding, layout, rendering, diagnostics, and generated contract reference from `FontManager`, settings/enums, raw-copy delivery, bundled descriptors, and focused tests.
- [x] Write remaining task guides for tool-specific workflows. Audio, video,
  particle authoring/runtime, focused AnimationViewer/ParticleViewer,
  interactive Mapper, and SPARK/Effekseer authoring now have owning manuals.
  The Mapper and SPARK pages use two versioned screenshots reproduced from the
  independent minimal multiplayer fixture.
- [x] Publish a source-backed guide for `.fo3d` animation tuples, `AnimSpeed`, one-step aliases, effective duration metadata, and typed common/client lookup.
- [x] Move and verify the TLA engine-owned 2D sprite root-motion material against current `ImageBaker`, `SpriteSheet`, `MovingContext`, and `CritterHexView` source; keep it separate from `.fo3d` skeletal animation.
- [x] Publish the complete current `.fo3d` model-description contract with exact parser-token coverage, FBX/OBJ inputs, layers, attachments, transforms, materials, cuts, runtime composition, generated reference, and visible-validation boundary.
- [ ] Extract reusable Last Frontier practices through the promotion gates above.
- [x] Publish broader Mapper and SPARK/Effekseer authoring manuals with
  versioned screenshots. [Mapper Interactive Manual](en/how-to/tools/mapper-interactive.md) owns the
  interactive Mapper, [Particle Authoring Tools](en/how-to/tools/particle-authoring.md)
  owns the particle authoring workflow, and
  [Animation and Particle Viewers](en/how-to/tools/animation-particle-viewers.md) owns focused AnimationViewer/ParticleViewer
  inspection. The two checked captures are bound to exact minimal-example
  sources, hashes, environment, alt/caption text, and recapture triggers.
- [x] Classify gameplay-test harnesses, analyzers, formatters, and remaining
  audits as engine, companion, or project-owned before documenting them.
  The external-evidence registry classifies the generic gameplay harness and
  the shared AiControl transport/security/lifecycle layer as promoted, Engine
  analyzers as promoted, project generators and audits as boundary-owned, and
  the dialog stack/declarative GUI authoring as project-owned until reusable
  implementations and tests move into Engine. Packaging is the one remaining
  promotion candidate because its landed Linux and publication evidence is
  still incomplete.
- [x] Publish the native-extension guide and generated interface reference.
- [x] Publish the broader project-local dependency guide.
  [ProjectDependencies.md](ProjectDependencies.md) now owns dependency
  classification, delivery models and records, the public role-scoped
  `AddProjectLibraries` helper, controlled package discovery, platform and ABI
  boundaries, runtime payloads, licensing/security, updates, rollback, and the
  validation matrix. The CMake interface has a mapper-only dependency lane;
  the minimal project compiles a server-role usage requirement, and Last
  Frontier consumes the public helper instead of mutating internal role lists.

Exit gate: every supported authored input and developer tool has a reference page, at least one how-to, a validation route, and an honest ownership label.

### Phase 6 - Debugging, testing, packaging, operations, and migration

- [ ] Rewrite remaining platform procedures against the starter/tutorial projects. [Web Build, Packaging, and Browser Debugging](en/how-to/platforms/web-debugging.md), [Android Build, Packaging, and Device Debugging](en/how-to/platforms/android-debugging.md), and [Native and AngelScript Debugging](en/troubleshooting/debugging.md) are independently source-backed, fully mirrored in EN/RU, tied to exact Last Frontier/TLA evidence, and explicit about platform/runtime qualification gaps; all three still need final starter/tutorial execution evidence where applicable.
- [x] Document test-boundary selection, sanitizers, coverage, native/script debugging, and profiling. [Testing](en/contributing/testing/index.md) owns unit/integration boundaries, sanitizer lanes, and coverage; [Native and AngelScript Debugging](en/troubleshooting/debugging.md) owns symbol semantics, debugger detection, mixed stacks, crash logging, Natvis, live attach capabilities/security, and project launch integration; [Profiling](en/how-to/quality/profiling.md) owns the four Tracy configurations, exact tool-version boundary, isolated client/server capture, reproducible workloads, interpretation, and project automation contract with focused source-pinning tests.
- [x] Document reproducible packaging for desktop, Web, Android, iOS, and server/service targets that are currently supported. [Packaging and Release](en/how-to/release/packaging.md) owns package declarations, build/bake/package ordering, Windows/Linux/Web/Android payloads, server/service/daemon boundaries, artifact manifests, signing/secrets, acceptance, publication, and rollback. `Examples/PackagingMatrix` proves reusable native mechanics, while `Examples/MinimalMultiplayer` now supplies a publishable project's own Windows/Linux raw/archive/config/gameplay acceptance source. Both Windows routes are locally green; landed Linux and immutable external artifacts remain pending. The guide records macOS/iOS as build-gated client inputs with no current Engine packager instead of presenting unsupported Apple release artifacts.
- [x] Document updater, signing, secrets, database operations, recovery, and security boundaries. [Client Runtime Split and Updater](en/explanation/runtime/client-updater.md), the [Engine Upgrade Guide](en/how-to/migration/engine-upgrade.md), and [Packaging and Release](en/how-to/release/packaging.md) own updater/release mechanics. [Security and Secrets](en/how-to/release/security-and-secrets.md) owns config substitution timing, redaction limits, package-host signing handoff, CI trust boundaries, secret-free artifacts, rotation/revocation, and incident routing. [Release Operations](en/how-to/release/operations.md) owns process selection, readiness/health evidence, target preflight, staged rollout, graceful shutdown, rollback, and failure routing. [Backup and Recovery](en/how-to/release/backup-and-recovery.md) now owns JSON/SQLite/Mongo/Memory durable sets, recovery-oplog limits, consistency evidence, quiesced/proven-online backup, isolated semantic restore, DR drills, and provider/project ownership; focused source tests pin the contract.
- [x] Add engine-upgrade, save migration, network compatibility, and client-runtime ABI guides. The [Engine Upgrade Guide](en/how-to/migration/engine-upgrade.md) ties the complete incoming range, generated contract diff, configuration/content reconciliation, backup/restore, gameplay compatibility, updater generation, frozen host/runtime ABI, rollout, validation, and same-change documentation into one reusable procedure.
- [ ] Validate every support claim on the declared host/target matrix.

Exit gate: a release engineer can build, diagnose, package, and upgrade a minimal game using only engine docs and public examples.

### Phase 7 - Public tutorial and showcase repositories

- [ ] Restage and publish the private `fonline-project-template` repository. The current remote head contains candidate source at a reachable exact Engine pin, but that pin predates `docs_examples.py`. The 2026-08-03 audit reconfirmed successful pinned Windows/Linux run `29739863448` and current-Engine Linux run `29740066760`, but the repository-contract step used the old-file fallback, neither run retained an artifact, and no required commit status was observed. Restage at a reviewed pin containing the validator, pass clean Windows/Linux pinned/current lanes without fallback skips, configure branch/security gates, create the immutable tag/artifact, and obtain owner-authorized publication.
- [ ] Populate and publish the reserved private `fonline-minimal-multiplayer` repository, then bind every tutorial step to a tag. Source, governance metadata, generated complete config, smoke manifests, and native package acceptance are ready; remote staging at a reviewed Engine pin, clean Windows/Linux pinned/current evidence, immutable artifacts, and owner-authorized publication remain.
- [ ] Stage and publish the reserved private `fonline-content-showcase` repository. The Engine-owned source, thirteen CC0 project-original assets with byte-level provenance, deterministic generation, budgets, native smoke, Direct3D 11 capture, exact Web package, and isolated Chromium/WebGL 2 runtime/capture are complete locally. Run and retain Linux native/OpenGL evidence, materialize at a clean published Engine pin, obtain pinned/current remote checks, enforce branch/security policy, and create immutable artifacts before owner-authorized visibility.
- [ ] Populate and publish the reserved private `fonline-native-extension-sample` repository after extension contracts stabilize. The Engine-owned source is now complete under `Examples/NativeExtensionSample`: it composes a server-role library, owns per-server state through `ServerEngine.UserData`, implements `ServerInitHook`, exports one AngelScript method, includes a focused native test and deterministic runtime smoke, materializes through the shared governance overlay, and passes `win64-native-extension-smoke` on the current Engine. Remote staging, the Linux pinned/current lanes, protected settings, immutable tag/artifact, and visibility remain publication gates.
- [ ] Add scheduled current-engine compatibility runs and automated update PRs. The weekly/manual workflow template exists, but no retained run/status is observed on the current staging head; obtain fresh pinned/current evidence before adding reviewed update-PR automation.
- [ ] Add cross-repository link, revision, asset-license, and snippet checks. The shared local validator now enforces registry identity, exact pins, required files, unresolved placeholders, byte-level asset provenance, and publication only with passing recorded checks. Live cross-repository checks await staged source and then the first published repository.

Exit gate: all required example repositories pass their pinned and current-engine compatibility gates, and every public asset has auditable provenance.

### Phase 8 - AI delivery and evaluation

- [x] Generate `llms.txt`, bounded full-context output, docs manifest, and machine-readable references. `BuildTools/docs_ai_delivery.py` projects the source manifest and canonical Markdown into root `llms.txt`, a 2 MiB-capped `llms-full.txt`, and public `docs-manifest.json`; canonical API/CMake/CLI/helper/native-extension/prototype/map/model/package/image/effect/particle JSON models remain separately linked rather than duplicated into the bundle.
- [ ] Add clean Markdown endpoints to the published site. Every public record now exposes a `source_ref`-pinned GitHub raw Markdown URL and `llms.txt` uses it while retaining canonical `fonline.ru` HTML. Same-domain aliases remain open because the pinned GitHub Pages/Jekyll stack cannot emit them without duplicated content or a custom plugin.
- [x] Build a versioned task/question evaluation set for architecture, scripting, content, debugging, migration, and release. `Docs/ai-evaluation.json` owns 27 tasks, 65 retrieval checks, and 92 answer checks, including AngelScript style/refactoring, gameplay-test boundary/process semantics, project-local dependency integration, public-contract/stability selection, Essentials/metadata ownership, revision-pinned CMake/main/helper CLI selection, interactive and automated Mapper workflows, SPARK authoring, and release backup/recovery; `BuildTools/docs_ai_eval.py` validates ownership/evidence and emits a deterministic report from the browser-equivalent search contract.
- [x] Evaluate at least two model families against standalone engine docs only. The final 2026-08-04 Ollama 0.32.5 qualifications use exact model digests, identical source/input and harness hashes, isolated per-task prompts, retained raw and repair attempts, and independent semantic review. Qwen 3.5 `huihui_ai/qwen3.5-abliterated:9b` selected 27/27 owners with 92/92 rubric criteria observable and passed 27/27 tasks (100 percent); GPT-OSS `gpt-oss:20b` selected 27/27 owners with 92/92 criteria observable and passed 25/27 tasks (92.6 percent). The two GPT-OSS failures are a forbidden Mapper API prescription and a non-actionable adoption summary; neither is an unsupported safety, migration, compatibility, or release claim. Compact reviews under `Docs/_meta/ai-evaluation/` bind the ignored raw runs by SHA-256.
- [x] Fix retrieval ambiguity, duplicated ownership, missing prerequisites, and unsupported assumptions revealed by the evaluation, then rerun two materially different families. The deterministic 27-task/65-query gate is green at 100 percent and 0.908 MRR. Twenty-one affected owning pages expose concise source-backed decision/checklist summaries in both locales; composite questions name the boundaries their rubrics score; required evidence tokens make objective omissions repairable without exposing the hidden rubric; and the isolated harness retains one invalid-response retry plus one objective semantic-completion repair. The final independent reviews exceed the unchanged 90-percent target for both families with zero unsupported critical-domain claims, so the AI remediation and phase quality exit gate are complete. Historical pre-remediation reviews remain immutable.
- [x] Keep AI entry points generated from the same navigation/manifest as the human site. Focused tests, `--check`, standalone validation, and the documentation workflow reject path/filter/hash/budget/output drift.

Exit gate: agents select the correct owning source/version and complete at least 90 percent of the agreed representative tasks without Last Frontier-specific assumptions.

### Phase 9 - Russian mirror and production launch

- [x] Freeze the English public information architecture and stable IDs for the translation pass. ADR-0006 and the generated route inventory freeze the current target paths; all required EN/RU pairs and the first executable example slices are present.
- [x] Create the Russian locale tree with one-to-one document coverage. All 197 required counterparts live under canonical paired routes or explicit README pairs with durable flat-path pointers.
- [x] Create and review the bilingual glossary and API-description translation catalog. The source-owned glossary contains 34 reviewed term records. The stable-locator catalog covers all 4,917 reader-facing values across twenty generated domains, including 2,474 native API descriptions and contract notes plus 221 map/prototype property projections, validates exact source hashes, unambiguous exact-source reuse, and inline-code shape, and runs in `complete` mode.
- [x] Translate tutorials first, then how-tos, reference descriptions, explanation, troubleshooting, and contributor/operator guides. Every physical human route is reviewed and current in EN/RU, including Configuration and Data Sources and Tools. All twenty generated-model domains apply complete semantic overlays while preserving canonical English JSON, signatures, identifiers, paths, and fenced examples.
- [x] Add translation hash/parity gates and language-preserving links. Whole-document hash/code/link parity, front-matter locale, stable-ID switching, separate locale search, durable legacy pointers, and explicit `--enforce-complete` CI enforcement now cover all 197 pairs.
- [x] Add `README.ru.md` and language selection from both repository and site entry points.
- [ ] Run native-speaker review, code/identifier integrity checks, accessibility checks, and bilingual search evaluation. Automated identifier/code parity, bilingual search, responsive Chromium, keyboard interaction, WCAG 2.2 A/AA subset checks, complete 200-percent route reflow, and direct review of the dedicated Russian 1280 x 1024 screenshot pass; human native-speaker, production-domain 200-percent zoom, and representative screen-reader review remain.
- [x] Remove the temporary `translation-pending` allowance from production branches. Physical parity and all twenty semantic domains run in explicit fail-closed `complete` mode in CI.
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
