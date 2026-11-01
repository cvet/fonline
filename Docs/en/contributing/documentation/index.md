---
layout: default
title: Documentation Maintenance
locale: en
document_id: documentation-maintenance
permalink: /Docs/en/contributing/documentation/
---

# Documentation Maintenance

> Engine-owned documentation. This page explains how to keep the FOnline engine documentation source-grounded, navigable, and separated from embedding-project content.

`docs-manifest.json` is the shared generated metadata index. `docs_localization.py` owns `#/translation_status`; `docs_site.py` owns `#/routing`. Each writer preserves other sections and each check compares its own section. Run localization, site/search/routes, AI evaluation when affected, then AI delivery; the last step refreshes public document and artifact hashes. Hashes that attest reviewed Russian translations remain in their source comments. Do not edit generated sections by hand. The index and navigation declare `VERSION` as their version source, without storing a copy of its value.

## Purpose

Run `python BuildTools/docs_prepare.py` before validation or a site build. It rebuilds ignored delivery indexes and reports in dependency order without rewriting versioned public contract models, Markdown, SVG, or PNG. Keep the source manifest, policies, translations and contract models in Git; `docs-manifest.json`, the `llms` endpoints, navigation/search indexes and validation catalogs belong to the completed build. `AGENTS.md` remains the checkout agent's entry point; the `llms` files serve external website readers.

Use this page when adding, verifying, or reorganizing engine docs. It is the maintainer workflow companion to the machine-readable [documentation manifest](../../../documentation-manifest.json), [documentation backlog](https://github.com/cvet/fonline/blob/master/Docs/_meta/DocumentationBacklog.md), [research template](https://github.com/cvet/fonline/blob/master/Docs/_meta/DocumentationResearchTemplate.md), [verification report](https://github.com/cvet/fonline/blob/master/Docs/_meta/DocumentationVerificationReport.md), [site publication guide](site-publication.md), [documentation index](../../index.md), and [AI-maintainer entry point](../../../../AGENTS.md).

## Source paths inspected

- `../AGENTS.md`
- `README.md`
- `Docs/en/index.md`
- `Docs/ru/index.md`
- `Docs/README.md` (legacy route)
- `Docs/_meta/DocumentationBacklog.md`
- `Docs/_meta/DocumentationExpansionPlan.md`
- `Docs/_meta/DocumentationResearchTemplate.md`
- `Docs/_meta/DocumentationVerificationReport.md`
- `Docs/documentation-manifest.json`
- `Docs/generated/api.json`
- `Docs/generated/api/*.md`
- `Docs/contract-change-dispositions.json`
- `Docs/generated/source-inventory.json`
- `Docs/generated/cli.json`
- `Docs/en/reference/buildtools/*.md`
- `Docs/ru/reference/buildtools/*.md`
- `Docs/generated/cli/*.md` (legacy routes)
- `Docs/generated/helper-cli.json`
- `Docs/en/reference/helper-cli/*.md`
- `Docs/ru/reference/helper-cli/*.md`
- `Docs/generated/helper-cli/*.md` (legacy routes)
- `Docs/generated/cmake.json`
- `Docs/en/reference/cmake/*.md`
- `Docs/ru/reference/cmake/*.md`
- `Docs/generated/cmake/*.md` (legacy routes)
- `Docs/generated/native-extension.json`
- `Docs/generated/native-extension/*.md`
- `Docs/generated/prototype-format.json`
- `Docs/generated/prototype-format/*.md`
- `Docs/generated/map-format.json`
- `Docs/generated/map-format/*.md`
- `Docs/generated/model-format.json`
- `Docs/generated/model-format/*.md`
- `Docs/generated/text-format.json`
- `Docs/generated/text-format/*.md`
- `Docs/generated/effect-format.json`
- `Docs/generated/effect-format/*.md`
- `Docs/generated/image-format.json`
- `Docs/generated/image-format/*.md`
- `Docs/generated/particle-format.json`
- `Docs/en/reference/particle-format/*.md`
- `Docs/generated/font-format.json`
- `Docs/en/reference/font-format/*.md`
- `Docs/generated/audio.json`
- `Docs/en/reference/audio/*.md`
- `Docs/generated/video.json`
- `Docs/en/reference/video/*.md`

- `BuildTools/AiControlProtocol.json`
- `BuildTools/ai_control_client.py`
- `BuildTools/docs_ai_control_protocol.py`
- `BuildTools/tests/test_ai_control_protocol.py`
- `BuildTools/tests/test_docs_ai_control_protocol.py`
- `Examples/AiControlSample/`
- `Docs/generated/ai-control-protocol.json`
- `Docs/generated/ai-control-protocol/*.md`
- `Docs/generated/package.json`
- `Docs/generated/package/*.md`
- `Examples/PublicRepositories.json`
- `Examples/PublicRepositoryTemplate/`
- `BuildTools/docs_examples.py`
- `BuildTools/tests/test_docs_examples.py`
- `Docs/generated/public-examples.json`
- `Docs/generated/public-examples/*.md`
- `BuildTools/SupportMatrix.json`
- `BuildTools/docs_support_matrix.py`
- `BuildTools/tests/test_docs_support_matrix.py`
- `Docs/generated/support-matrix.json`
- `Docs/generated/support-matrix/*.md`
- `BuildTools/DocumentationDiagrams.json`
- `BuildTools/docs_diagrams.py`
- `BuildTools/tests/test_docs_diagrams.py`
- `Docs/generated/diagrams.json`
- `Docs/assets/diagrams/*.svg`
- `BuildTools/DocumentationScreenshots.json`
- `BuildTools/docs_screenshots.py`
- `BuildTools/tests/test_docs_screenshots.py`
- `Docs/generated/screenshots.json`
- `Docs/assets/screenshots/*.png`
- `BuildTools/SnippetPolicy.json`
- `BuildTools/docs_snippets.py`
- `BuildTools/tests/test_docs_snippets.py`
- `Docs/generated/snippets.json`
- `Docs/translation-glossary.json`
- `BuildTools/docs_localization.py`
- `BuildTools/tests/test_docs_localization.py`
- `docs-manifest.json#/translation_status`
- `Docs/description-translations.ru.json`
- `BuildTools/docs_description_translations.py`
- `BuildTools/tests/test_docs_description_translations.py`
- `Docs/generated/description-translation-status.json`
- `Docs/ai-evaluation.json`
- `BuildTools/docs_ai_eval.py`
- `BuildTools/tests/test_docs_ai_eval.py`
- `Docs/generated/ai-evaluation-report.json`
- `BuildTools/docs_ai_delivery.py`
- `BuildTools/tests/test_docs_ai_delivery.py`
- `BuildTools/docs_site.py`
- `BuildTools/docs_site_artifact.py`
- `BuildTools/tests/test_docs_site.py`
- `BuildTools/tests/test_docs_site_layout.py`
- `BuildTools/tests/test_docs_site_artifact.py`
- `BuildTools/docs-browser/package.json`
- `BuildTools/docs-browser/package-lock.json`
- `BuildTools/docs-browser/audit.mjs`
- `BuildTools/tests/test_docs_browser.py`
- `BuildTools/web/default-index.html`
- `BuildTools/web/simple-web-server.py`
- `Docs/Site/Data/docs-site.json`
- `Docs/Site/Assets/docs-search.json`
- `Docs/Site/Assets/docs-search.ru.json`
- `docs-manifest.json#/routing`
- `llms.txt`
- `llms-full.txt`
- `docs-manifest.json`
- `BuildTools/docs_api.py`
- `BuildTools/docs_api_diff.py`
- `BuildTools/docs_contract_diff.py`
- `BuildTools/docs_public_api.py`
- `Docs/en/reference/public-contract/index.md`, its RU mirror, and the `PUBLIC_API.md` legacy route
- `BuildTools/ExternalProjectEvidence.json`
- `BuildTools/docs_external_evidence.py`
- `BuildTools/tests/test_docs_external_evidence.py`
- `BuildTools/gameplay_test_runner.py`
- `BuildTools/tests/test_gameplay_test_runner.py`
- `BuildTools/tests/test_docs_gameplay_testing.py`
- `Docs/generated/external-project-evidence.json`
- `Docs/generated/external-project-evidence/index.md`
- `BuildTools/docs_reference.py`
- `BuildTools/docs_metadata.py`
- `BuildTools/docs_inventory.py`
- `BuildTools/docs_cli.py`
- `BuildTools/docs_helper_cli.py`
- `BuildTools/docs_native_extension.py`
- `BuildTools/docs_prototype_format.py`
- `BuildTools/docs_map_format.py`
- `BuildTools/docs_model_format.py`
- `BuildTools/docs_text_format.py`
- `BuildTools/docs_effect_format.py`
- `BuildTools/docs_image_format.py`
- `BuildTools/docs_particle_format.py`
- `BuildTools/docs_font_format.py`
- `BuildTools/docs_audio.py`
- `BuildTools/docs_video.py`
- `BuildTools/docs_package.py`
- `BuildTools/docs_validate.py`
- `BuildTools/tests/test_docs_api.py`
- `BuildTools/tests/test_docs_api_diff.py`
- `BuildTools/tests/test_docs_contract_diff.py`
- `BuildTools/tests/test_docs_public_api.py`
- `BuildTools/tests/test_docs_reference.py`
- `BuildTools/tests/test_docs_metadata.py`
- `BuildTools/tests/test_docs_inventory.py`
- `BuildTools/tests/test_docs_cli.py`
- `BuildTools/tests/test_docs_helper_cli.py`
- `BuildTools/tests/test_docs_native_extension.py`
- `BuildTools/tests/test_docs_prototype_format.py`
- `BuildTools/tests/test_docs_map_format.py`
- `BuildTools/tests/test_docs_model_format.py`
- `BuildTools/tests/test_docs_text_format.py`
- `BuildTools/tests/test_docs_effect_format.py`
- `BuildTools/tests/test_docs_image_format.py`
- `BuildTools/tests/test_docs_particle_format.py`
- `BuildTools/tests/test_docs_font_format.py`
- `BuildTools/tests/test_docs_audio.py`
- `BuildTools/tests/test_docs_video.py`
- `BuildTools/tests/validate_native_extension_interface.cmake`
- `BuildTools/tests/test_docs_package.py`
- `BuildTools/tests/validate_package_interface.cmake`
- `BuildTools/tests/test_docs_validate.py`
- `.github/workflows/validate.yml`
- `Docs/Site/_config.yml`, `Docs/documentation-manifest.json`, `BuildTools/docs_prepare.py`, `BuildTools/docs_site_build.py`
- representative source-grounded subsystem docs under `Docs/`

## Documentation ownership rules

Engine docs should explain reusable engine behavior:

- source layout and architecture;
- application/build-tool entry points;
- runtime/entity/network/persistence/client/server/frontend behavior;
- scripting, generated metadata, nullability, native method exports;
- bakers, mapper, editor, reusable tool mechanics;
- platform build/debug flows;
- tests, validation routing.

Embedding-project docs should own:

- concrete game content, balance, quests, text, maps, factions, release policy;
- game-specific scripts, native extensions;
- exact binary names/presets unless explicitly shown as examples;
- product-specific generated outputs, downstream pipelines.

Engine docs must not treat embedding-project scripts, tests, tools, CI jobs, or generated artifacts as normative engine validation. If a reusable validation helper is important enough to cite from an engine doc, keep it in the engine repository; cite a project-specific helper from that project's docs instead.

Engine documentation now lives in `Engine/Docs/`. Do not maintain parallel engine-owned explanations in an embedding project's docs; route project docs back to the engine page and keep only project-specific wrappers, commands, and policy there.

Engine Markdown links must resolve inside the engine checkout. Never use parent-project paths, even for non-normative examples: use a stable HTTPS link to a tagged public example repository, or describe the project-owned responsibility in plain prose until one exists.

`Docs/documentation-manifest.json` classifies every maintained Markdown entry and owns its stable ID, audience, Diataxis kind, visibility, translation scope, domain owner, lifecycle state, migration destination, and source paths. It also owns the rolling/current version channel, deferred release-snapshot policy, canonical/planned locales, explicit README locale pairs, and route migration strategy. Update it in the same change when a page is added, moved, retired, retargeted, or translated.

### Owners and review requirements

Each domain owner has a manifest-owned review contract. Changes need the primary owner named by the page or structured contract, that owner's listed evidence, and every boundary-triggered co-review. `localization` owns documentation locale parity and native-language review; `content-data` owns Engine text-format behavior and authored-data mechanics. Build/release, runtime, scripting, content, frontend, networking, tooling, platform, quality, localization, and documentation review remain separate responsibilities even when one maintainer fills several roles.

[External Project Evidence And Promotion Inventory](https://github.com/cvet/fonline/blob/master/Docs/generated/external-project-evidence/index.md) is the checked internal Last Frontier/TLA discovery ledger. Records require an exact snapshot source, disposition, priority, Engine or project target, primary owner, required reviews, and promotion gate. They never make external projects normative: `promoted` claims are re-derived from Engine source/tests; `boundary-owned` keeps concrete implementation outside Engine; `promotion-candidate` names missing reusable artifacts; `project-owned` forbids invented Engine contracts. Update and source-verify the ledger when either project's evidence changes a promotion decision or reveals a new reusable concern. It is excluded from the public site and AI delivery.

Every master update, even documentation-only work, also follows the [mandatory Engine update contract](../../how-to/release/versioning.md#every-master-update): version progression, dated bilingual change/migration notes, preserved semantics, and complete generated delivery. Run `BuildTools/docs_engine_version.py` against the actual baseline before authorized integration.

## Standard doc slice workflow

1. Pick a coherent slice from the documentation backlog.
2. Inspect the backlog's source paths and related tests/build files.
3. Write or update the owning doc with `Source paths inspected`.
4. Prefer source relationships and ownership boundaries over long API trivia.
5. Add a validation checklist to deep subsystem docs.
6. Review the owning structured contract when a generated surface changes. Native API changes: update `///@ ApiContract` as needed; diagnose symbols with `BuildTools/docs_api_diff.py`. Project-facing CMake: update `BuildTools/cmake/ProjectInterface.json`; run the structural CMake test. Main BuildTools CLI: keep `create_parser()` authoritative. Helper-script CLI: keep executable `create_parser()` authoritative; update `BuildTools/HelperCliInterface.json` on ownership/audience/invocation changes; regenerate the helper reference. Native-extension role/hook/binding: update `BuildTools/NativeExtensionInterface.json`; run its structural test; validate the minimal starter or affected project. Format triggers (prototype through video) use this pattern: update the named interface and guide, regenerate model/reference, run the listed tests, then perform the stated rebake/check. Prototype parser/property/metadata: `BuildTools/PrototypeFormatInterface.json` and [PrototypeFormat.md](../../../PrototypeFormat.md), focused test, rebake an affected project. Map parser/baker/mapper/materialization: `BuildTools/MapFormatInterface.json` and [Map Format](../../how-to/content/map-format.md), focused map tests, rebake an affected project. `.fo3d` parser state/tokens, FBX/OBJ import, model layers, attachments, particles, transforms, materials, cuts, rendering flags, or model limits: `BuildTools/ModelFormatInterface.json` and [Model Format](../../how-to/content/model-format.md), focused model tests, visible client scene. `.fotxt`, language normalization, prototype `$Text`, text script methods, or inline color tags: `BuildTools/TextFormatInterface.json` and [Text and Localization](../../how-to/content/text-and-localization.md), focused text tests, rebake an affected project. `.fofx`, effect state, shader resources, `EffectBaker`, backend bindings, `EffectManager`, script values, or `FO_EFFECT_*` limits: `BuildTools/EffectFormatInterface.json` and [Effect Format](../../how-to/content/effect-format.md), focused effect tests, every affected backend/profile in a visible client scene. `ImageBaker`, FOFRM, built-in image loaders, baked sprite records, default factory coverage, atlas upload, or image caches: `BuildTools/ImageFormatInterface.json` and [Image And Sprite Formats](../../how-to/content/image-format.md), focused image documentation/native tests, rebake and visibly inspect an affected project. `FO_*_PARTICLES`, `.spark`/`.efkproj` parsing, `.spk`/`.efk` baking, backend composition, SPARK/Effekseer rendering, Mapper particle tools, particle caches, script methods, or model-particle links: `BuildTools/ParticleFormatInterface.json` and [Particle Format And Runtime](../../how-to/content/particle-format.md), focused particle documentation and baker/runtime tests, rebake and visibly inspect every affected backend and integration path. `.fofnt`/`.fnt`, raw-copy selection, `FontManager`, slot/flag enums, bind-time scale, measurement, wrapping, or inline colors: `BuildTools/FontFormatInterface.json` and [Font Formats And Text Layout](../../how-to/content/font-format.md), focused font documentation and native tests, rebake and visibly inspect an affected project. `AudioManager`, WAV/Ogg decoding, sound-name indexing, `Game.PlaySound`/`Game.PlayMusic`, audio settings, `AppAudio`, or AudioBaker delivery: `BuildTools/AudioInterface.json` and [Audio.md](../../../Audio.md), focused audio documentation and native tests, rebake and audibly inspect an affected project on every claimed platform. `VideoClip`, Ogg/Theora decoding, fullscreen queue/input/music/drawing, `VideoPlayback`, script drawing, or raw-copy OGV delivery: `BuildTools/VideoInterface.json` and [Video.md](../../../Video.md), focused video documentation and native tests, rebake and visibly inspect an affected project on every claimed platform. Native GUI render/input primitives or script exports: update [Frontend and Rendering](../../explanation/rendering/) and [GUI Integration Boundary](../../how-to/runtime/gui.md); regenerate the script API; run focused native tests; visibly validate every affected scripting backend and screen in an embedding project; keep high-level libraries, formats, generators, and references project-owned. AiControl framing, methods, errors, common command fields, authorization, bounds, lifecycle, threat policy, reference client, or sample behavior: update `BuildTools/AiControlProtocol.json` and [AiControl Protocol](../../how-to/ai-control-protocol.md); regenerate the protocol plus helper-CLI references; run both focused suites; validate every affected project-native/client/MCP path; project observations, game actions, administrator commands, and MCP namespaces stay project-owned. Model animation tokens, clip durations, aliases, baked duration metadata, or script lookup: also update [Model Animation](../../how-to/content/model-animation.md); run its focused source test; regenerate the native API/reference when needed; rebake an affected project. Image-frame offsets, baked sprite offset transport, or client walk/run phase: update [Sprite Root Motion](../../how-to/content/sprite-root-motion.md); run its focused source test and image-baker tests; validate the affected locomotion in a visible client scene. Package declarations or payload behavior: update `BuildTools/PackageInterface.json`; run the structural package test. Public example portfolio, shared repository files, source scaffold, compatibility boundaries, or publication state: update `Examples/PublicRepositories.json` and [PublicExampleRepositories.md](../../../PublicExampleRepositories.md); regenerate its model/reference; validate affected external repositories in both Engine modes. Regenerate every affected runtime model, compare all seventeen generated contract domains with `BuildTools/docs_contract_diff.py`, regenerate the root contract index with `BuildTools/docs_public_api.py`, and complete Contract Change Management dispositions for baseline-public or model-contract breaks. Project-authored remote calls stay project-owned: bake both sides; regenerate/check their catalog with `BuildTools/docs_metadata.py`.
   For the current 3D subsystem, treat `ModelSourceLoader`, `ModelAnimationConverter`, `ModelAnimationData`, `ModelMeshData`, `ModelManager`, `ModelInformation`, `ModelInstance`, and `ModelAnimation` as co-owners with the two bakers. Any parser, source, compatibility, mesh/rig wire, Ozz runtime, or ownership change updates both model guides and the structured model contract, runs focused model/Ozz native suites, force-rebakes an affected project, proves the next incremental bake is clean, and visibly validates the affected pose/composition.
7. Add or update the `Docs/documentation-manifest.json` page entry, keep its stable ID and locale target authoritative, and assign each public current human top-level page to exactly one `site_delivery.navigation` group. Regenerate source-owned diagrams with `BuildTools/docs_diagrams.py` when manifest, owning prose, or source provenance changes. Recapture any source-owned screenshot whose recorded trigger fired, then regenerate `Docs/generated/screenshots.json` with `BuildTools/docs_screenshots.py`; catalog-only preservation of an obsolete image is not reconciliation. For scoped fence or snippet policy changes, regenerate `Docs/generated/snippets.json` with `BuildTools/docs_snippets.py`. After seventeen source models and references, regenerate the canonical EN/RU public contract indexes and root legacy route. Regenerate translation status after these source assets; every existing Russian page must carry the new normalized English hash or the change fails. Then regenerate `Docs/Site/Data/docs-site.json`, both `Docs/Site/Assets/docs-search.json` and `Docs/Site/Assets/docs-search.ru.json`, and `docs-manifest.json#/routing` with `BuildTools/docs_site.py`. Regenerate `Docs/generated/ai-evaluation-report.json` with `BuildTools/docs_ai_eval.py` when evaluation owner/evidence headings or English search behavior change. Finally regenerate `llms.txt`, `llms-full.txt`, and `docs-manifest.json` with `BuildTools/docs_ai_delivery.py`. The public manifest hashes all diagram/screenshot/snippet/site/evaluation data; never edit them.
8. Update the documentation index for a new user-facing page.
9. Promote backlog status only after semantic source review, not link checks alone.
10. Add a dated section to the [verification report](https://github.com/cvet/fonline/blob/master/Docs/_meta/DocumentationVerificationReport.md) with scope, sources, fixes, and checks.
11. Run `python BuildTools/docs_validate.py`; keep staging empty unless the owner explicitly asks to stage/commit.

### Moving a public document

A file rename alone is not route migration:

1. Keep the stable document ID on the new canonical page.
2. Move canonical content to the manifest-owned English target; add the matching Russian target only when its reviewed translation exists.
3. Retain the old Markdown path as a short durable pointer to the canonical page so GitHub and Jekyll preserve the legacy URL.
4. Mark the old record as a replacement/route alias; make the new record the one non-`replace` owner of the target.
5. Regenerate `docs-manifest.json#/routing`; the old route must appear in `legacy_redirects` and resolve to the expected canonical document ID.
6. Update navigation/search only for the canonical page; a legacy pointer is not a second searchable owner.
7. Require `locale: en` / `locale: ru` front matter on the new pair, verify the stable-ID language switch, and prove each locale index returns only its own canonical routes.
8. Before removing temporary migration state, run focused localization, site, AI-delivery, standalone validation, Jekyll artifact, and browser locale-interaction checks.

The current route stays canonical until this complete change lands; a planned route-catalog path does not authorize deleting the old file.

## Revision update reconciliation

Changing the Engine revision also changes documentation. Every incoming commit is a candidate contract change even when documented already. The updating maintainer owns reconciliation in the same worktree.

Before updating:

1. Record the current engine SHA and target branch/ref.
2. Preserve the dirty worktree, including untracked generated docs, in a named stash or separate worktree until validation passes.
3. Save the current generated JSON model under ignored `Workspace/` if no committed baseline provides it.

Rebase unpublished commits onto the fetched base, including on branches with an upstream.
Keep already-pushed tips as ancestors; merge only diverged published histories.
Apply separately to Engine and embedding projects.

After integration:

```bash
git log --oneline <old-engine-sha>..<new-engine-sha>
git diff --name-status <old-engine-sha>..<new-engine-sha>
git diff --stat <old-engine-sha>..<new-engine-sha>
```

Reconcile each changed surface:

1. Read incoming source/tests, not only commit subjects or incoming prose.
2. Find the owning page via the documentation index and the documentation manifest's `sources` fields.
3. Retain useful incoming docs; immediately correct stale paths, project dependencies, unsupported claims, or missing validation evidence.
4. Update owning pages/cross-links in the same worktree. Embedding-project workarounds/tests are not normative Engine proof; reusable proof belongs in this repository.
5. Record the exact SHA range, contract changes, generated delta, affected docs, and checks in the verification report.

Generated-surface triggers:

| Incoming change | Reconciliation |
|---|---|
| `BuildTools/codegen.py`, native metadata annotations, `Source/Scripting/`, or `Source/Common/Settings.inc` | Regenerate `Docs/generated/api.json` and native Markdown pages; use `docs_api_diff.py` for symbol details; include the model in the aggregate contract diff. |
| `BuildTools/cmake/ProjectInterface.json` or the project-facing CMake option/stage/helper implementation | Regenerate/check `Docs/generated/cmake.json`, canonical English pages under `Docs/en/reference/cmake/`, and legacy pointers under `Docs/generated/cmake/`; update the reviewed Russian mirror and its source hash in the same change. Run `validate_project_interface.cmake`, the focused CMake documentation test, localization checks, and site validation. Add newly public declarations to the manifest, not implementation-only helpers. For project library roles/linking, update [ProjectDependencies.md](../../../ProjectDependencies.md), its focused test, minimal fixture, and affected embedding-project configure/build. |
| `BuildTools/buildtools.py::create_parser()` or command-line help/default/choice behavior | Regenerate/check `Docs/generated/cli.json`, canonical English pages under `Docs/en/reference/buildtools/`, and legacy pointers under `Docs/generated/cli/` with `docs_cli.py`; update the reviewed Russian mirror and its source hash in the same change. Run the focused CLI documentation test, localization checks, and site validation. |
| A helper script's `create_parser()` or `BuildTools/HelperCliInterface.json` | Regenerate/check `Docs/generated/helper-cli.json`, canonical English pages under `Docs/en/reference/helper-cli/`, and legacy pointers under `Docs/generated/helper-cli/` with `docs_helper_cli.py`; update the reviewed Russian mirror and its source hash in the same change. Run the focused helper CLI test, localization checks, and site validation; review owner/audience/invocation changes. |
| `ProtoBaker`, `ConfigFile` prototype syntax, property text loading/serialization, `HasProtos` metadata, or `Baking.ProtoFileExtensions` | Update `BuildTools/PrototypeFormatInterface.json` and PrototypeFormat.md; regenerate/check prototype-format model/pages; run `test_docs_prototype_format.py` and aggregate diff; rebake an affected embedding project; update its companion metadata/semantic docs. |
| `MapLoader`, `MapBaker`, mapper load/save, `ItemOwnership`, static-map loading, or map content materialization | Update `BuildTools/MapFormatInterface.json` and Map Format; regenerate/check map-format model/pages; run `test_docs_map_format.py`, affected engine map tests, and aggregate diff; rebake an affected embedding project; update project map/content guidance. |
| `ModelMeshBaker`, `ModelSourceLoader`, `ModelAnimationConverter`, `ModelInfoBaker`, `.fo3d` syntax/state, FBX/OBJ import, model layers/links, particles, transforms, textures/effects, cuts, rendering flags, `ModelManager`/`ModelInformation`/`ModelInstance`/`ModelAnimation`, or `FO_MODEL_*` shape limits | Update `BuildTools/ModelFormatInterface.json`, Model Format, and Model Animation as applicable; regenerate/check model-format model/pages; run both focused documentation suites plus owning native model tests and aggregate diff; force-rebake an affected project; verify incremental bake settles; validate every changed composition/animation in a visible client scene. |
| `TextPack`, `TextBaker`, `ProtoTextBaker`, `.fotxt`, `Baking.BakeLanguages`, `Client.Language`, text script methods, or inline `@color` parsing | Update `BuildTools/TextFormatInterface.json` and Text and Localization; regenerate/check text-format model/pages; run `test_docs_text_format.py`, focused text-baker tests, and aggregate diff; rebake an affected project; validate language switching and project formatting in a visible client. |
| `EffectBaker`, `.fofx` sections/state, `RenderEffect` buffers, renderer descriptor/pipeline handling, `EffectManager`, effect script methods, or `FO_EFFECT_*` limits | Update `BuildTools/EffectFormatInterface.json` and Effect Format; regenerate/check effect-format model/pages; run `test_docs_effect_format.py`, focused effect-baker tests, and aggregate diff; rebake an affected project; validate every changed slot/backend/profile in a visible client. |
| `ImageBaker`, FOFRM fields/flattening, built-in image loaders, baked sprite records, `DefaultSpriteFactory`, `ResourceManager` extension/cache behavior, or `TextureAtlas` upload | Update `BuildTools/ImageFormatInterface.json` and Image And Sprite Formats; regenerate/check image-format model/pages; run `test_docs_image_format.py`, focused image/atlas tests, and aggregate diff; rebake an affected project; visibly inspect dimensions, alpha, directions, cadence, hit masks, and relevant client profiles. |
| `FO_*_PARTICLES`, `.spark`/`.efkproj` parsing, `.spk`/`.efk` baking, backend composition/rendering, Mapper particle tools, `ParticleManager`, `ParticleSpriteFactory`, script methods, or model-particle links | Update `BuildTools/ParticleFormatInterface.json` and Particle Format And Runtime; regenerate/check particle-format model/pages; run `test_docs_particle_format.py`, focused baker/runtime/model tests, and aggregate diff; rebake an affected project; visibly inspect every enabled backend plus sprite, map, script, and model-bone routes. |
| `MapperEngine`, stock Mapper menus/windows/controls/hotkeys/history/layout, mapper-side script exports, headless map capture, TGA/atlas readback, or visible-window evidence | Update [Mapper Interactive Manual](../../how-to/tools/mapper-interactive.md) and [Mapper Tools](../../how-to/tools/mapper.md); run `test_docs_mapper_tools.py`; exercise affected interactive or headless path on an Engine-owned fixture. A changed UI, capture path, fixture, or recorded trigger also requires recapturing the exact screenshot, regenerating provenance, rebuilding Jekyll, and inspecting desktop/mobile output; changed map format or particle paths also follow their rows. |
| `AnimationViewer`, `ParticleViewer`, their application hosts/libraries/targets/output paths/package roles, Mapper embedding, focused controls, data-source mounting, or persisted settings | Update [Animation and Particle Viewers](../../how-to/tools/animation-particle-viewers.md); run `test_docs_viewer_tools.py`; build and launch both viewers when shared host/CMake/settings behavior changed (else affected viewer); execute the affected visible review workflow against current content. Particle changes also follow the particle-format row; model/animation changes also follow the model-format and model-animation routes. |
| `.fofnt`/`.fnt` parsing, `Baking.RawCopyFileExtensions`, `FontManager`, `FontType`, `FontFlag`, `TextFormat`, `Game.BindFont`, measurement, wrapping, or inline colors | Update `BuildTools/FontFormatInterface.json` and Font Formats And Text Layout; regenerate/check font-format model/pages; run `test_docs_font_format.py`, native unit tests, and aggregate diff; rebake an affected project; validate measurement and visible text rendering. |
| `AudioManager`, WAV/Ogg decoding, `Game.PlaySound`/`Game.PlayMusic`, `Audio.*`, `AppAudio`, or audio baking | Update `BuildTools/AudioInterface.json` and Audio.md; regenerate/check audio model/pages; run `test_docs_audio.py`, native unit tests, and aggregate diff; rebake representative formats; validate effects, music, repeat, volume, and diagnostics audibly in a visible client on every claimed platform. |
| `VideoClip`, Ogg/Theora decoding, `Game.PlayVideo`, fullscreen queue/input/music/drawing, `VideoPlayback`, `Game.DrawVideoPlayback`, or OGV raw-copy delivery | Update `BuildTools/VideoInterface.json` and Video.md; regenerate/check video model/pages; run `test_docs_video.py`, native unit tests, and aggregate diff; rebake representative assets; validate first frame, motion, completion, skip, queue, aspect, audio, cleanup, and any claimed loop visibly on every claimed platform. |
| Native GUI input/render primitives or their script exports | Update Frontend and Rendering plus the generated script API; run focused native tests; compile/bake affected scripting backends in an embedding project; visibly validate screens, resolutions, languages, input modes, and renderers. Keep high-level GUI libraries, declarative formats, generators, and references in project documentation. |
| `BuildTools/PackageInterface.json`, `DefinePackage`, `Packages.cmake`, `package.py`, `Examples/PackagingMatrix`, or package target/platform/pack/payload/config behavior | Update [Packaging and Release](../../how-to/release/packaging.md) and, when qualification changed, the Support Matrix; regenerate/check `Docs/generated/package.json` and its Markdown pages with `docs_package.py`; regenerate/check fixture config after `Settings.inc` changes; run focused Python and structural CMake package tests plus every affected `*-package-smoke` or product package path. Re-audit signing order, secret boundaries, artifact inventory, updater payloads, acceptance, and rollback claims. |
| Native build configurations or symbol flags, `is_run_in_debugger`, `break_into_debugger`, stack capture/resolution, exception/crash handlers, `FO_SELFTEST_CRASH`, Natvis/NatJMC, `AngelScript.Debugger*`, the AngelScript debugger endpoint/protocol, Managed exception/stack reporting, generated C# projects, managed-debugger qualification, or the VS Code adapter | Update [Native, AngelScript, and Managed Debugging](../../troubleshooting/debugging.md) and its Russian mirror; refresh exact project evidence when cited files or revisions change; run `test_docs_debugging.py`, stack/exception and affected runtime tests, the adapter typecheck/build or live-endpoint gate when applicable, and project static/live launch checks. Keep debugger bind loopback; distinguish symbols from debug semantics; preserve crash-artifact privacy ownership; never infer live capability from mock adapter controls or static profiles. |
| Emscripten pin/preparation, Web CMake flags, Web package/shell/server, canvas/clipboard/IDBFS/main-loop behavior, WebSocket selection, Web native-updater capability, support label, hosting/security contract, or project Web evidence | Update [Web Build, Packaging, and Browser Debugging](../../how-to/platforms/web-debugging.md), plus Packaging, Security, Support Matrix, Networking, or Client Updater on boundary change; run `test_docs_web_debugging.py`, package/security/support tests, affected Web build, a fresh bake/package, HTTP artifact/header inspection, and applicable browser/release acceptance rows. Never promote build-only evidence to browser or production-deployment qualification. |
| Android platform/ABI mappings, SDK/NDK/API pins, package settings, Gradle/manifest/SDL templates, `FOnlineActivity`, resource staging, ADB endpoint/install/launch/log behavior, Android native-updater capability, support labels, or project Android evidence | Update [Android Build, Packaging, and Device Debugging](../../how-to/platforms/android-debugging.md), plus Packaging, Security, Support Matrix, or Client Updater on boundary change; run `test_docs_android_debugging.py`, package/security/support tests, affected Android native build, a fresh bake and Gradle assembly, APK inspection, install/update, cold/warm launch, and applicable device acceptance rows. Never promote build-only evidence to APK, device, release, or store qualification. |
| `GlobalSettings` substitution/precedence/save/draw/log behavior, `Common.SecretSettingTokens`, `ConfigBaker`, package signing fields or host directive resolution, the Android Gradle signing template, or workflow credential boundaries | Update [Security and Secrets](../../how-to/release/security-and-secrets.md), [Project Configuration](../../how-to/build/project-configuration.md), and affected release/platform docs; run `test_package_security.py`, `test_docs_security_and_secrets.py`, settings unit tests when native behavior changed, and the affected package lane. Inspect baked configs, generated package trees, logs, archives, caches, and manifests with synthetic values only; re-audit project secret provisioning, rotation, and incident routing without copying credentials into evidence. |
| `DataBase.*`, `Server.DbStorage`, `DataBase.*` settings, JSON/SQLite/Mongo/Memory storage, commit drain, recovery oplog, panic/reconnect, persisted startup/shutdown, migration compatibility, or project backup/restore procedure | Update [Persistence](../../explanation/persistence/), [Backup and Recovery](../../how-to/release/backup-and-recovery.md), and affected release/upgrade/security docs; run `test_docs_backup_recovery.py`, database unit tests, and affected backend/project restore lane. Re-prove complete durable set, consistency method, oplog behavior, isolated semantic restore, RPO/RTO evidence, and old/new binary compatibility without checking provider credentials or production data into the Engine. |
| `Examples/PublicRepositories.json`, `Examples/PublicRepositoryTemplate`, `Examples/MinimalProject`, public example ownership/lifecycle/remote visibility/pins, source-staging exclusions/materialization, common fixed settings required by the minimal config, or a compatibility boundary consumed by examples | Update [Public Example Repositories](../../how-to/build/public-example-repositories.md); regenerate/check public-example model with `docs_examples.py`; run its focused tests; rematerialize the affected candidate from a clean remotely fetchable exact Engine commit; validate each affected source-staged or published repository in pinned and current modes. Keep private remote state separate from source readiness; do not publish or update external links until its owner-gated exit gate is green. |
| `BuildTools/buildtools.py` validation profiles, required workflow platform matrices, platform detection, application construction, runtime smoke evidence, or uploaded validation artifacts | Update `BuildTools/SupportMatrix.json` and the [Support Matrix](../../reference/platforms/support-matrix.md); regenerate/check its model/page with `docs_support_matrix.py`; run the affected validation target; keep build, smoke, package-fixture, source-capable, and project-qualified claims separate. |
| `BuildTools/gameplay_test_runner.py`, its manifest schema, `Examples/GameplayTestHarness`, or an Engine example's process smoke | Update [Gameplay and Integration Testing](../../how-to/testing/gameplay-and-integration.md), `HelperCliInterface.json` when CLI syntax/ownership changes, and affected example manifest; run `test_gameplay_test_runner.py`, `test_docs_gameplay_testing.py`, helper-CLI generation/tests, and at least one real baked headless server/client smoke. Keep project script registries, fixtures, content assertions, ports, backends, platform labs, and thresholds in the embedding project. |
| `Profiling_*` configurations, `FO_TRACE_ENABLED`/`FO_TRACE_CATEGORIES`/`TRACY_*` wiring, `TracyVersion.hpp`, native/script zones, frame marks, plots, log messages, thread names, or allocation tracking | Update [Profiling](../../how-to/quality/profiling.md); run `test_docs_profiling.py`; build the affected on-demand/total profile; record one isolated client or server capture as appropriate. A Tracy update also requires matching capture/export tools and one capture on each runtime side before changing protocol/version claims. |
| Canonical English human prose, locale targets, glossary terms, or an existing Russian translation | Update the paired Russian page; retain identical fenced code; refresh only after review; regenerate `docs-manifest.json#/translation_status` with `docs_localization.py`; then regenerate both locale search indexes and the route/navigation model with `docs_site.py`. Run focused parity, site, artifact, and browser locale-interaction tests. Complete manifest enforcement rejects every missing or stale page pair. |
| Reader-facing prose in any generated contract model, `Docs/description-translations.ru.json`, or a Russian generated reference | Keep canonical JSON and stable IDs unchanged; update the stable-locator translation and its exact source hash; regenerate the owning model/pages; then regenerate `Docs/generated/description-translation-status.json` with `docs_description_translations.py`. Run its focused test plus the owning generator test. Missing records remain explicit until semantic enforcement becomes `complete`; unknown, stale, position-based, type-changing, or code-changing records are defects. |
| `BuildTools/DocumentationDiagrams.json`, a diagram-owning document, diagram source provenance, caption/alt text, or shared diagram CSS | Update the source manifest and owning prose together; regenerate/check `Docs/generated/diagrams.json` plus `Docs/assets/diagrams/*.svg` with `docs_diagrams.py`; run `test_docs_diagrams.py`; rebuild Jekyll; inspect retained desktop/mobile diagram screenshots. Never edit generated SVG by hand or use a diagram as the only representation of a required procedure. |
| `BuildTools/DocumentationScreenshots.json`, Mapper/SPARK/viewer UI, a capture fixture, screenshot-owning prose, image bytes, capture environment, or listed source provenance | Rebuild the exact recorded fixture; reproduce the interaction at the recorded viewport/backend; recapture without cosmetic edits; update the source manifest and owning alt/caption together; regenerate/check `Docs/generated/screenshots.json`; run `test_docs_screenshots.py`; rebuild Jekyll; inspect the decoded desktop/mobile page. Never update only a hash to bless an unexplained visual difference. |
| A fenced block in public/current/human Markdown or `BuildTools/SnippetPolicy.json` | Declare a supported language; update [Documentation Snippet Validation](snippets.md) when policy changes; regenerate/check `Docs/generated/snippets.json`; run `test_docs_snippets.py` plus `docs_snippets.py --check --external`; run the semantic compile/bake/smoke owner for every claimed outcome. Untyped/ignored fences are not allowed. |
| Export methods, native test files, or settings declarations | Regenerate/check `Docs/generated/source-inventory.json`. |
| Any inventoried Markdown, its visibility/state/owner/target/sources, publication URL, versioning policy, locale policy, or canonical generated model path | Update `Docs/documentation-manifest.json` as needed; regenerate/check snippets first when fenced content/scope changed; site delivery next; AI evaluation after search when affected; then root `llms.txt`, `llms-full.txt`, and `docs-manifest.json` with `docs_ai_delivery.py`. A context-budget failure requires explicit manifest/policy review, never truncation. |
| Any public Markdown title/path/state/target, README locale pair, `site_delivery` navigation/routing/search/browser policy, Jekyll include/exclude rule, layout, or site asset | Regenerate/check `Docs/Site/Data/docs-site.json`, both locale search indexes, and `docs-manifest.json#/routing` with `docs_site.py`; run focused source/layout/artifact/browser tests, `docs_site_artifact.py`, and the pinned `docs-browser` audit against the same completed `Workspace/DocumentationSite` tree. Route collisions, missing rendered pages/endpoints, wrong `html lang`, broken language pairs, cross-locale search results, browser runtime/resource failures, WCAG violations, keyboard/focus regressions, page overflow, ambiguous replacement owners, or a per-locale search-budget failure require explicit policy fixes; never silently remove a document or lower a gate. |
| `Docs/ai-evaluation.json`, a task-owning stable ID, an answer-evidence heading/term, compact-search tokenization/ranking, or the current source ref | Update [AI Documentation Evaluation](ai-evaluation.md); regenerate/check `Docs/generated/ai-evaluation-report.json` with `docs_ai_eval.py` after `docs_site.py`; run its focused test; record model-family findings separately. Never add irrelevant keywords or lower a threshold merely to make a rank green. |
| Metadata baker or remote-call format/runtime | Update [Remote Calls](../../reference/scripting/remote-calls.md); an embedding project must rebake both sides and regenerate its project-owned remote-call catalog. |
| Module init, callback attributes, `Yield`, server script scheduling/synchronization, mutable-global policy, or entity callback teardown | Update [Script Lifecycle and Concurrency](../../how-to/scripting/lifecycle-and-concurrency.md); run `test_docs_script_lifecycle.py`; run the narrow source/runtime tests named by the guide. |
| `.fos` module ordering, side macros, namespace/file ownership, CoreScripts formatting, mutable globals, attributed calls, generated-script discipline, or refactoring guidance | Update [AngelScript Style and Refactoring](../../how-to/scripting/style-and-refactoring.md) and its Russian mirror; run `test_docs_angelscript_style.py`; use the owning formatter; compile every affected side warning-free; execute the narrowest behavior/contract test. Keep game comment language, domain vocabulary, generated formats, migrations, and gameplay acceptance in the embedding project. |
| `ModelInfoBaker`, `.fo3d` animation tokens, model mesh clip durations, state/action aliases, `ModelAnimationInfo.foinfo`, metadata registration, or duration script methods | Update Model Animation; run `test_docs_model_animation.py`; regenerate/check native API/reference and aggregate diff when exports changed; run focused model-baker/common-script tests; rebake an affected embedding project. |
| `ImageImport::FrameShot::Motion`, baked sprite offsets, `SpriteSheet`, movement interpolation consumed by rendering, or `CritterHexView` walk/run phase and anchor behavior | Update [Sprite Root Motion](../../how-to/content/sprite-root-motion.md); run `test_docs_sprite_root_motion.py` plus focused image-baker tests; rebake affected sprite assets; validate straight movement, turns, and stop/start in a visible client scene. |
| Build, package, platform, runtime, persistence, networking, or pointer/nullability behavior | Update the owning source-grounded page and run the narrow behavior/test path named there. |

After every generated-surface trigger, run `docs_contract_diff.py` against preserved old models or the intended Git base. A zero native API delta does not dispose CMake, main CLI, package, helper CLI, native-extension, prototype-format, map-format, model-format, text-format, effect-format, image-format, particle-format, font-format, audio, or video changes.

The update is incomplete while a generator reports stale output, incoming behavior lacks an owning documentation disposition, conflict markers remain, or the safety stash is the only copy of unresolved work. Drop it only after final checks and an empty staged area are confirmed.

## Backlog status meanings

- `planned` — topic identified, not researched.
- `researching` — source inspection underway.
- `drafted` — first doc exists; semantic validation incomplete.
- `verified` — page checked against current source; post-edit mechanical checks passed.

Chat-only progress is not the source of truth; record a complete or blocked slice in the backlog/report.

## Link and path validation

At minimum, validate:

- manifest coverage, ownership metadata, declared source paths;
- Markdown links and anchors across every inventoried doc;
- resolved local links stay inside the engine root;
- Backticked source/build/doc paths that should exist in the engine checkout.
- Stale alternate-layout terms known from older snapshots.
- Test inventory coverage when editing [Testing](../testing/).
- `git diff --check`.
- staged area and working-tree status.

Run the complete standalone gate from the engine root; discovery prevents a newly added `test_docs_*.py` file from being silently omitted locally:

```bash
python -m unittest discover -s BuildTools/tests -p "test_docs_*.py"
python BuildTools/tests/test_gameplay_test_runner.py
python BuildTools/tests/test_minimal_multiplayer_package.py
python BuildTools/tests/test_ai_control_protocol.py
python BuildTools/tests/test_package_security.py
python BuildTools/tests/test_angelscript_cmake.py
cmake -P BuildTools/tests/validate_project_interface.cmake
cmake -P BuildTools/tests/validate_package_interface.cmake
cmake -P BuildTools/tests/validate_native_extension_interface.cmake
python BuildTools/docs_snippets.py --check --external
python BuildTools/docs_validate.py
```

The `Validate documentation` and `Parse documentation snippets` jobs in `.github/workflows/validate.yml` are the authoritative CI expansion: they run each focused test and generator check explicitly, then classify contract changes against the base revision. Keep those jobs and this aggregate local route behaviorally equivalent; they need no embedding project or native build.

Rendered-output changes must also follow the site publication guide. With Ruby/Bundler/Node available, run `python BuildTools/docs_site_build.py`, `python BuildTools/docs_site_artifact.py --site-dir Workspace/DocumentationSite`, and the pinned browser audit; every pull request also receives a GitHub Pages-compatible `Workspace/DocumentationSite` artifact plus static and browser validation reports from the `Build documentation site` job.

Planned future docs should be plain prose unless the checker explicitly exempts them; do not backtick/link missing docs as if they exist.

## Source-grounded writing conventions

- Start with purpose and reader routing.
- Include `Source paths inspected` for subsystem pages.
- Route to sibling docs rather than duplicate deep details.
- Use exact source-ownership paths.
- Use tagged public example URLs for cross-project evidence; never make a local embedding checkout part of an engine procedure.
- Avoid promising unsupported workflows; state what current source/test/build wiring supports.
- Update related docs when ownership moves between pages.

## AI maintainer notes

[AGENTS.md](../../../../AGENTS.md), the AI-maintainer entry point, routes to human docs and records conventions, including no commit/push without explicit instruction. Keep it concise and navigational; detailed human procedures belong in `Docs/`. Root `llms.txt`, `llms-full.txt`, and `docs-manifest.json` are generated external-agent retrieval routes; `Docs/Site/Data/docs-site.json`, both locale search indexes, and `docs-manifest.json#/routing` form the matching human navigation/search/routing projection. All six must derive from the same manifest/corpus.

A future AI agent continuing this roadmap should re-anchor from git status, backlog, and verification report before editing; chat context is secondary to repository state.

Keep instructions durable and actionable: rules/routes rather than incident logs or
copies of every subsystem procedure. On `AGENTS.md` refactoring, map old sections to
surviving owners, preserve incoming heading anchors, and check the client's
instruction-discovery budget. Read owning task contracts explicitly; never assume
a nested guide was injected. Contextualized owner corrections may motivate rules;
old transcripts and worker reports never grant authority. Source establishes
implemented behavior; owner requirements define intended behavior. Keep raw private
audit evidence in the task workspace, outside published docs.

## Validation checklist

1. Classify new docs in `Docs/documentation-manifest.json`; link them from documentation index and, when relevant, AGENTS.md.
2. Backlog status matches actual source validation state.
3. Verification report records every promoted slice.
4. API changes have a base-revision report and every required public disposition passes [Contract Change Management](../contract-change-management.md).
5. AI delivery and human site delivery are regenerated and their focused tests/checks pass.
6. `python BuildTools/tests/test_docs_validate.py` and `python BuildTools/docs_validate.py` pass.
7. Link/path/test/stale-term checks pass after report updates.
8. `git diff --cached --name-only` is empty unless staging was explicitly requested.
9. The final report names changed files and confirms no commit/push unless requested.
