---
layout: default
title: Documentation Maintenance
locale: en
document_id: documentation-maintenance
permalink: /Docs/en/contributing/documentation/
---

# Documentation Maintenance

> Engine-owned documentation. This page explains how to keep the FOnline engine documentation source-grounded, navigable, and separated from embedding-project content.

## Purpose

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
- `Docs/generated/gui-runtime.json`
- `Docs/generated/gui-runtime/*.md`
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
- `Docs/generated/translation-status.json`
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
- `_data/docs-site.json`
- `assets/docs-search.json`
- `assets/docs-search.ru.json`
- `Docs/generated/document-routes.json`
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
- `BuildTools/docs_gui_runtime.py`
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
- `BuildTools/tests/test_docs_gui_runtime.py`
- `BuildTools/tests/validate_native_extension_interface.cmake`
- `BuildTools/tests/test_docs_package.py`
- `BuildTools/tests/validate_package_interface.cmake`
- `BuildTools/tests/test_docs_validate.py`
- `.github/workflows/validate.yml`
- `_config.yml`, `Gemfile`, `.ruby-version`, and `CNAME`
- representative source-grounded subsystem docs under `Docs/`

## Documentation ownership rules

Engine docs should explain reusable engine behavior:

- source layout and architecture;
- application and build-tool entry points;
- runtime/entity/network/persistence/client/server/frontend behavior;
- scripting, generated metadata, nullability, and native method exports;
- bakers, mapper, editor, and reusable tool mechanics;
- platform build/debug flows;
- tests and validation routing.

Embedding-project docs should own:

- concrete game content, balance, quests, text, maps, factions, and release policy;
- game-specific scripts and native extensions;
- exact binary names/presets unless explicitly shown as examples;
- product-specific generated outputs and downstream pipelines.

Engine docs must not depend on embedding-project scripts, tests, tools, CI jobs, or generated artifacts as normative validation for engine behavior. If a reusable validation helper is important enough to cite from an engine doc, keep that helper in the engine repository. If a project-specific helper is useful, cite it from that project's docs instead.

Engine documentation now lives in `Engine/Docs/`. Do not maintain parallel engine-owned explanations in an embedding project's docs; route project docs back to the engine page and keep only project-specific wrappers, commands, and policy there.

Engine Markdown links must resolve inside the engine checkout. Do not use parent-project paths even for non-normative examples: use a stable HTTPS link to a tagged public example repository, or describe the project-owned responsibility in plain prose until such an example exists.

Every maintained Markdown entry is classified in `Docs/documentation-manifest.json`. The manifest owns its stable ID, audience, Diataxis kind, visibility, translation scope, domain owner, lifecycle state, migration destination, and source paths. The same manifest owns the rolling/current version channel, deferred release-snapshot policy, canonical and planned locales, explicit README locale pairs, and route migration strategy. A documentation change that adds, moves, retires, retargets, or translates a page must update the manifest in the same change.

### Owners and review requirements

The manifest also owns one review contract for every domain owner. A documentation change needs the primary owner named by the page or structured contract, the evidence listed for that owner, and every co-review triggered by the affected boundary. `localization` owns documentation locale parity and native-language review; `content-data` still owns Engine text-format behavior and authored-data mechanics. Build/release, runtime, scripting, content, frontend, networking, tooling, platform, quality, localization, and documentation review are separate responsibilities even when one maintainer currently fills several roles.

[External Project Evidence And Promotion Inventory](https://github.com/cvet/fonline/blob/master/Docs/generated/external-project-evidence/index.md) is the checked internal discovery ledger for Last Frontier and TLA. Its records must name an exact snapshot source, disposition, priority, Engine or project target, primary owner, required reviews, and promotion gate. External projects never become normative merely because a record exists: a `promoted` claim is re-derived from Engine source/tests, `boundary-owned` keeps the concrete implementation outside Engine, `promotion-candidate` names missing reusable artifacts, and `project-owned` forbids an invented Engine contract. Update and source-verify this ledger when either project's evidence changes a promotion decision or reveals a new reusable concern; the ledger itself is excluded from the public site and AI delivery.

## Standard doc slice workflow

1. Pick a coherent slice from the [documentation backlog](https://github.com/cvet/fonline/blob/master/Docs/_meta/DocumentationBacklog.md).
2. Inspect the source paths named in the backlog and any related tests/build files.
3. Write or update the owning doc with `Source paths inspected`.
4. Prefer source relationships and ownership boundaries over long API trivia.
5. Add a validation checklist to deep subsystem docs.
6. Review the owning structured contract when a generated surface changes. For native API changes, update `///@ ApiContract` metadata as needed and use `BuildTools/docs_api_diff.py` for symbol-level diagnosis. For project-facing CMake changes, update `BuildTools/cmake/ProjectInterface.json` and run the structural CMake test. For main BuildTools CLI changes, keep `create_parser()` authoritative. For helper-script CLI changes, keep the executable `create_parser()` authoritative, update `BuildTools/HelperCliInterface.json` when ownership/audience/invocation changes, and regenerate the helper reference. For native-extension role/hook/binding changes, update `BuildTools/NativeExtensionInterface.json`, run its structural test, and validate the minimal starter or affected project. For prototype parser/property/metadata changes, update `BuildTools/PrototypeFormatInterface.json` and [PrototypeFormat.md](../../../PrototypeFormat.md), regenerate its model/reference, run the focused test, and rebake an affected project. For map parser/baker/mapper/materialization changes, update `BuildTools/MapFormatInterface.json` and [Map Format](../../how-to/content/map-format.md), regenerate its model/reference, run focused map tests, and rebake an affected project. For `.fo3d` parser state/tokens, FBX/OBJ import, model layers, attachments, particles, transforms, materials, cuts, rendering flags, or model limits, update `BuildTools/ModelFormatInterface.json` and [Model Format](../../how-to/content/model-format.md), regenerate its model/reference, run focused model tests, and validate a visible client scene. For `.fotxt`, language normalization, prototype `$Text`, text script methods, or inline color tags, update `BuildTools/TextFormatInterface.json` and [Text and Localization](../../how-to/content/text-and-localization.md), regenerate its model/reference, run focused text tests, and rebake an affected project. For `.fofx`, effect state, shader resources, `EffectBaker`, backend bindings, `EffectManager`, script values, or `FO_EFFECT_*` limits, update `BuildTools/EffectFormatInterface.json` and [Effect Format](../../how-to/content/effect-format.md), regenerate its model/reference, run focused effect tests, and validate every affected backend/profile in a visible client scene. For `ImageBaker`, FOFRM, built-in image loaders, baked sprite records, default factory coverage, atlas upload, or image caches, update `BuildTools/ImageFormatInterface.json` and [Image And Sprite Formats](../../how-to/content/image-format.md), regenerate its model/reference, run focused image documentation/native tests, and rebake plus visibly inspect an affected project. For `FO_*_PARTICLES`, `.spark`/`.efkproj` parsing, `.spk`/`.efk` baking, backend composition, SPARK/Effekseer rendering, Mapper particle tools, particle caches, script methods, or model-particle links, update `BuildTools/ParticleFormatInterface.json` and [Particle Format And Runtime](../../how-to/content/particle-format.md), regenerate its model/reference, run focused particle documentation and baker/runtime tests, and rebake plus visibly inspect every affected backend and integration path. For `.fofnt`/`.fnt`, raw-copy selection, `FontManager`, slot/flag enums, bind-time scale, measurement, wrapping, or inline colors, update `BuildTools/FontFormatInterface.json` and [Font Formats And Text Layout](../../how-to/content/font-format.md), regenerate its model/reference, run focused font documentation and native tests, and rebake plus visibly inspect an affected project. For `SoundManager`, WAV/ACM/Ogg decoding, sound-name indexing, `Game.PlaySound`/`Game.PlayMusic`, audio settings, `AppAudio`, or raw-copy audio delivery, update `BuildTools/AudioInterface.json` and [Audio.md](../../../Audio.md), regenerate its model/reference, run focused audio documentation and native tests, and rebake plus audibly inspect an affected project on every claimed platform. For `VideoClip`, Ogg/Theora decoding, fullscreen queue/input/music/drawing, `VideoPlayback`, script drawing, or raw-copy OGV delivery, update `BuildTools/VideoInterface.json` and [Video.md](../../../Video.md), regenerate its model/reference, run focused video documentation and native tests, and rebake plus visibly inspect an affected project on every claimed platform. For CoreScripts GUI types/API/lifecycle/layout/input or native GUI event dispatch, update `BuildTools/GuiRuntimeInterface.json` and [GUI Runtime](../../how-to/runtime/gui.md), regenerate its model/reference, run focused GUI documentation tests, and validate an affected embedding project visibly; declarative formats and generators remain project-owned unless deliberately promoted. For AiControl framing, methods, errors, common command fields, authorization, bounds, lifecycle, threat policy, reference client, or sample behavior, update `BuildTools/AiControlProtocol.json` and [AiControl Protocol](../../how-to/ai-control-protocol.md), regenerate the AiControl protocol plus helper-CLI references, run both focused AiControl suites, and validate every affected project-native/client/MCP path; project observations, game actions, administrator commands, and MCP namespaces stay project-owned. For model animation tokens, clip durations, aliases, baked duration metadata, or script lookup changes, also update [Model Animation](../../how-to/content/model-animation.md), run its focused source test, regenerate the native API/reference when needed, and rebake an affected project. For image-frame offsets, baked sprite offset transport, or client walk/run phase behavior, update [Sprite Root Motion](../../how-to/content/sprite-root-motion.md), run its focused source test and image-baker tests, and validate the affected locomotion in a visible client scene. For package declarations or payload behavior, update `BuildTools/PackageInterface.json` and run the structural package test. For the public example portfolio, shared repository files, source scaffold, compatibility boundaries, or publication state, update `Examples/PublicRepositories.json` and [PublicExampleRepositories.md](../../../PublicExampleRepositories.md), regenerate its model/reference, and validate affected external repositories in both Engine modes. Regenerate every affected runtime model, compare all eighteen runtime domains with `BuildTools/docs_contract_diff.py`, regenerate the root contract index with `BuildTools/docs_public_api.py`, and complete [Contract Change Management](../contract-change-management.md) dispositions for baseline-public or model-contract breaks. Project-authored remote calls remain project-owned: bake both sides and regenerate/check their catalog with `BuildTools/docs_metadata.py`.
   For the current 3D subsystem, treat `ModelSourceLoader`, `ModelAnimationConverter`, `ModelAnimationData`, `ModelMeshData`, `ModelManager`, `ModelInformation`, `ModelInstance`, and `ModelAnimation` as co-owners with the two bakers. A parser, source, compatibility, mesh/rig wire, Ozz runtime, or ownership change updates both model guides and the structured model contract, runs the focused model/Ozz native suites, force-rebakes an affected project, proves the following incremental bake is clean, and validates the affected pose/composition visibly.
7. Add or update the page entry in `Docs/documentation-manifest.json`, keep its stable ID and locale target authoritative, and assign each public current human top-level page to exactly one `site_delivery.navigation` group. Regenerate source-owned diagrams first with `BuildTools/docs_diagrams.py` whenever their manifest, owning prose, or source provenance changes. Recapture any source-owned screenshot whose recorded trigger fired, then regenerate `Docs/generated/screenshots.json` with `BuildTools/docs_screenshots.py`; changing only the catalog to preserve an obsolete image is not reconciliation. Regenerate `Docs/generated/snippets.json` next with `BuildTools/docs_snippets.py` whenever any scoped fence or snippet policy changes. Regenerate the canonical EN/RU public contract indexes and root legacy route after their eighteen source models and references. Regenerate translation status after these source assets; every existing Russian page must carry the new normalized English hash or the change fails. Then regenerate `_data/docs-site.json`, both `assets/docs-search.json` and `assets/docs-search.ru.json`, and `Docs/generated/document-routes.json` with `BuildTools/docs_site.py`. Regenerate `Docs/generated/ai-evaluation-report.json` with `BuildTools/docs_ai_eval.py` whenever an evaluation owner/evidence heading or English search behavior changes. Finally regenerate `llms.txt`, `llms-full.txt`, and `docs-manifest.json` with `BuildTools/docs_ai_delivery.py`. The public manifest hashes all diagram/screenshot/snippet/site/evaluation data; none of these files may be edited manually.
8. Update the [documentation index](../../index.md) when a new user-facing page is added.
9. Promote backlog status only after semantic source review, not just link checks.
10. Add a dated section to the [verification report](https://github.com/cvet/fonline/blob/master/Docs/_meta/DocumentationVerificationReport.md) with scope, sources, fixes, and checks.
11. Run `python BuildTools/docs_validate.py` and keep staging empty unless the owner explicitly asks to stage/commit.

### Moving a public document

Do not treat a file rename as sufficient route migration:

1. Keep the stable document ID on the new canonical page.
2. Move the canonical content to the manifest-owned English target and add the matching Russian target only when its reviewed translation exists.
3. Retain the old Markdown path as a short durable pointer to the canonical page so GitHub and Jekyll both preserve the legacy URL.
4. Mark the old record as a replacement/route alias and make the new record the one non-`replace` owner of the target.
5. Regenerate `Docs/generated/document-routes.json`; the old route must appear in `legacy_redirects` and resolve to the expected canonical document ID.
6. Update navigation/search only for the canonical page. A legacy pointer is not a second searchable owner.
7. Require `locale: en` / `locale: ru` front matter on the new pair, verify the stable-ID language switch, and prove that each locale index returns only its own canonical routes.
8. Run the focused localization, site, AI-delivery, standalone validation, Jekyll artifact, and browser locale-interaction checks before removing any temporary migration state.

The current route remains canonical until this complete change lands. A planned path in the route catalog does not authorize deleting the old file.

## Revision update reconciliation

Pulling or changing the engine revision is a documentation event, not only a Git operation. Every incoming commit is a candidate contract change even when it already contains documentation. The maintainer performing the update owns the reconciliation in the same worktree.

Before updating:

1. Record the current engine SHA and target branch/ref.
2. Preserve a dirty worktree with a named stash or separate worktree, including untracked generated documentation, and retain that safety copy until validation passes.
3. Preserve the current generated JSON model under ignored `Workspace/` when it is not available from a committed baseline.

After the fast-forward/rebase:

```bash
git log --oneline <old-engine-sha>..<new-engine-sha>
git diff --name-status <old-engine-sha>..<new-engine-sha>
git diff --stat <old-engine-sha>..<new-engine-sha>
```

Then reconcile each changed surface:

1. Read the incoming source and tests, not only commit subjects or incoming prose.
2. Find the owning page through the [documentation index](../../index.md) and the `sources` fields in the [documentation manifest](../../../documentation-manifest.json).
3. Keep useful incoming documentation, but correct stale paths, project dependencies, unsupported claims, or missing validation evidence immediately.
4. Update the owning page and cross-links in the same worktree. An embedding-project workaround or test is not normative engine proof; reusable proof belongs under this repository.
5. Record the exact SHA range, contract changes, generated delta, affected docs, and checks in the [verification report](https://github.com/cvet/fonline/blob/master/Docs/_meta/DocumentationVerificationReport.md).

Generated-surface triggers:

| Incoming change | Required reconciliation |
|---|---|
| `BuildTools/codegen.py`, native metadata annotations, `Source/Scripting/`, or `Source/Common/Settings.inc` | Regenerate `Docs/generated/api.json` and native Markdown pages; use `docs_api_diff.py` for symbol details and include the model in the aggregate contract diff. |
| `BuildTools/cmake/ProjectInterface.json` or the project-facing CMake option/stage/helper implementation | Regenerate/check `Docs/generated/cmake.json`, canonical English pages under `Docs/en/reference/cmake/`, and legacy route pointers under `Docs/generated/cmake/`; update the reviewed Russian mirror and its source hash in the same change. Run `validate_project_interface.cmake`, the focused CMake documentation test, localization checks, and site validation. Add newly public declarations to the manifest rather than documenting implementation-only helpers. For project library roles/linking, update [ProjectDependencies.md](../../../ProjectDependencies.md), its focused test, the minimal fixture, and an affected embedding-project configure/build. |
| `BuildTools/buildtools.py::create_parser()` or command-line help/default/choice behavior | Regenerate/check `Docs/generated/cli.json`, canonical English pages under `Docs/en/reference/buildtools/`, and legacy route pointers under `Docs/generated/cli/` with `docs_cli.py`; update the reviewed Russian mirror and its source hash in the same change. Run the focused CLI documentation test, localization checks, and site validation. |
| A helper script's `create_parser()` or `BuildTools/HelperCliInterface.json` | Regenerate/check `Docs/generated/helper-cli.json`, canonical English pages under `Docs/en/reference/helper-cli/`, and legacy route pointers under `Docs/generated/helper-cli/` with `docs_helper_cli.py`; update the reviewed Russian mirror and its source hash in the same change. Run the focused helper CLI test, localization checks, and site validation, and review owner/audience/invocation changes. |
| `ProtoBaker`, `ConfigFile` prototype syntax, property text loading/serialization, `HasProtos` metadata, or `Baking.ProtoFileExtensions` | Update `BuildTools/PrototypeFormatInterface.json` and [PrototypeFormat.md](../../../PrototypeFormat.md); regenerate/check the prototype-format model/pages, run `test_docs_prototype_format.py` and the aggregate diff, then rebake an affected embedding project and update its companion metadata/semantic docs. |
| `MapLoader`, `MapBaker`, mapper load/save, `ItemOwnership`, static-map loading, or map content materialization | Update `BuildTools/MapFormatInterface.json` and [Map Format](../../how-to/content/map-format.md); regenerate/check the map-format model/pages, run `test_docs_map_format.py`, affected engine map tests, and the aggregate diff, then rebake an affected embedding project and update project map/content guidance. |
| `ModelMeshBaker`, `ModelSourceLoader`, `ModelAnimationConverter`, `ModelInfoBaker`, `.fo3d` syntax/state, FBX/OBJ import, model layers/links, particles, transforms, textures/effects, cuts, rendering flags, `ModelManager`/`ModelInformation`/`ModelInstance`/`ModelAnimation`, or `FO_MODEL_*` shape limits | Update `BuildTools/ModelFormatInterface.json`, [Model Format](../../how-to/content/model-format.md), and [Model Animation](../../how-to/content/model-animation.md) as applicable; regenerate/check the model-format model/pages, run both focused documentation suites plus the owning native model tests and aggregate diff, then force-rebake an affected project, verify the incremental bake settles, and validate every changed composition/animation in a visible client scene. |
| `TextPack`, `TextBaker`, `ProtoTextBaker`, `.fotxt`, `Baking.BakeLanguages`, `Client.Language`, text script methods, or inline `@color` parsing | Update `BuildTools/TextFormatInterface.json` and [Text and Localization](../../how-to/content/text-and-localization.md); regenerate/check the text-format model/pages, run `test_docs_text_format.py`, focused text-baker tests, and the aggregate diff, then rebake an affected project and validate language switching plus project formatting in a visible client. |
| `EffectBaker`, `.fofx` sections/state, `RenderEffect` buffers, renderer descriptor/pipeline handling, `EffectManager`, effect script methods, or `FO_EFFECT_*` limits | Update `BuildTools/EffectFormatInterface.json` and [Effect Format](../../how-to/content/effect-format.md); regenerate/check the effect-format model/pages, run `test_docs_effect_format.py`, focused effect-baker tests, and the aggregate diff, then rebake an affected project and validate every changed slot/backend/profile in a visible client. |
| `ImageBaker`, FOFRM fields/flattening, built-in image loaders, baked sprite records, `DefaultSpriteFactory`, `SpriteManager` extension/cache behavior, or `TextureAtlas` upload | Update `BuildTools/ImageFormatInterface.json` and [Image And Sprite Formats](../../how-to/content/image-format.md); regenerate/check the image-format model/pages, run `test_docs_image_format.py`, focused image/atlas tests, and the aggregate diff, then rebake an affected project and inspect dimensions, alpha, directions, cadence, hit masks, and relevant client profiles visibly. |
| `FO_*_PARTICLES`, `.spark`/`.efkproj` parsing, `.spk`/`.efk` baking, backend composition/rendering, Mapper particle tools, `ParticleManager`, `ParticleSpriteFactory`, script methods, or model-particle links | Update `BuildTools/ParticleFormatInterface.json` and [Particle Format And Runtime](../../how-to/content/particle-format.md); regenerate/check the particle-format model/pages, run `test_docs_particle_format.py`, focused baker/runtime/model tests, and the aggregate diff, then rebake an affected project and inspect every enabled backend plus sprite, map, script, and model-bone routes visibly. |
| `MapperEngine`, stock Mapper menus/windows/controls/hotkeys/history/layout, mapper-side script exports, headless view/capture, TGA/atlas readback, or full-window composition | Update [Mapper Interactive Manual](../../how-to/tools/mapper-interactive.md) and [Mapper Tools](../../how-to/tools/mapper.md), run `test_docs_mapper_tools.py`, and exercise the affected interactive or headless path against an Engine-owned fixture. A changed UI, capture path, fixture, or recorded trigger also requires recapturing the exact screenshot, regenerating its provenance, rebuilding Jekyll, and inspecting desktop/mobile output; a changed map format or particle path additionally follows its owning row. |
| `AnimationViewer`, `ParticleViewer`, their application hosts/libraries/targets/output paths/package roles, Mapper embedding, focused controls, data-source mounting, or persisted settings | Update [Animation and Particle Viewers](../../how-to/tools/animation-particle-viewers.md), run `test_docs_viewer_tools.py`, build and launch both viewers when shared host/CMake/settings behavior changed (otherwise the affected viewer), and execute the affected visible review workflow against current content. Particle changes also follow the particle-format row; model/animation changes also follow the model-format and model-animation routes. |
| `.fofnt`/`.fnt` parsing, `Baking.RawCopyFileExtensions`, `FontManager`, `FontType`, `FontFlag`, `TextFormat`, `Game.BindFont`, measurement, wrapping, or inline colors | Update `BuildTools/FontFormatInterface.json` and [Font Formats And Text Layout](../../how-to/content/font-format.md); regenerate/check the font-format model/pages, run `test_docs_font_format.py`, native unit tests, and the aggregate diff, then rebake an affected project and validate measurement plus visible text rendering. |
| `SoundManager`, `ResourceManager` sound indexing, WAV/ACM/Ogg decoding, `Game.PlaySound`/`Game.PlayMusic`, `Audio.*`, `AppAudio`, or audio raw-copy delivery | Update `BuildTools/AudioInterface.json` and [Audio.md](../../../Audio.md); regenerate/check the audio model/pages, run `test_docs_audio.py`, native unit tests, and the aggregate diff, then rebake representative formats and validate effects, music, repeat, volume, and diagnostics audibly in a visible client on every claimed platform. |
| `VideoClip`, Ogg/Theora decoding, `Game.PlayVideo`, fullscreen queue/input/music/drawing, `VideoPlayback`, `Game.DrawVideoPlayback`, or OGV raw-copy delivery | Update `BuildTools/VideoInterface.json` and [Video.md](../../../Video.md); regenerate/check the video model/pages, run `test_docs_video.py`, native unit tests, and the aggregate diff, then rebake representative assets and validate first frame, motion, completion, skip, queue, aspect, audio, cleanup, and any claimed loop visibly on every claimed platform. |
| `CoreScripts/Gui.fos`, `Input.fos`, native GUI input/render dispatch, GUI annotations, types, screen API, lifecycle, layout, drawing, focus, drag/drop, grids, item views, or required project hooks | Update `BuildTools/GuiRuntimeInterface.json` and [GUI Runtime](../../how-to/runtime/gui.md); regenerate/check the GUI runtime model/pages, run `test_docs_gui_runtime.py` and the aggregate diff, then regenerate/rebake an affected embedding project and validate changed screens, resolutions, languages, input modes, and renderers visibly. Keep project declarative formats and generators in project documentation. |
| `BuildTools/PackageInterface.json`, `DefinePackage`, `Packages.cmake`, `package.py`, `Examples/PackagingMatrix`, or package target/platform/pack/payload/config behavior | Update [Packaging and Release](../../how-to/release/packaging.md) and, when qualification changed, the [Support Matrix](../../reference/platforms/support-matrix.md); regenerate/check `Docs/generated/package.json` and its Markdown pages with `docs_package.py`; regenerate/check the fixture config after `Settings.inc` changes; run the focused Python and structural CMake package tests plus every affected `*-package-smoke` or product package path. Re-audit signing order, secret boundaries, artifact inventory, updater payloads, acceptance, and rollback claims. |
| Native build configurations or symbol flags, `IsRunInDebugger`, `BreakIntoDebugger`, stack capture/resolution, exception/crash handlers, `FO_SELFTEST_CRASH`, Natvis/NatJMC, `Script.Debugger*`, the AngelScript debugger endpoint/protocol, or the VS Code adapter | Update [Native and AngelScript Debugging](../../troubleshooting/debugging.md) and its Russian mirror; refresh exact project evidence when cited files or revisions changed; run `test_docs_debugging.py`, stack/exception and affected runtime tests, the adapter typecheck/build or live-endpoint gate when applicable, and project static/live launch checks. Keep debugger bind loopback, distinguish symbols from debug semantics, preserve crash-artifact privacy ownership, and never infer a live capability from mock adapter controls or static profiles. |
| Emscripten pin/preparation, Web CMake flags, Web package/shell/server, canvas/clipboard/IDBFS/main-loop behavior, WebSocket selection, Web native-updater capability, support label, hosting/security contract, or project Web evidence | Update [Web Build, Packaging, and Browser Debugging](../../how-to/platforms/web-debugging.md), plus Packaging, Security, Support Matrix, Networking, or Client Updater when their boundary changed; run `test_docs_web_debugging.py`, package/security/support tests, the affected Web build, a fresh bake/package, HTTP artifact/header inspection, and applicable browser/release acceptance rows. Build-only evidence must not be promoted to browser or production-deployment qualification. |
| Android platform/ABI mappings, SDK/NDK/API pins, package settings, Gradle/manifest/SDL templates, `FOnlineActivity`, resource staging, ADB endpoint/install/launch/log behavior, Android native-updater capability, support labels, or project Android evidence | Update [Android Build, Packaging, and Device Debugging](../../how-to/platforms/android-debugging.md), plus Packaging, Security, Support Matrix, or Client Updater when their boundary changed; run `test_docs_android_debugging.py`, package/security/support tests, the affected Android native build, a fresh bake and Gradle assembly, APK inspection, install/update, cold/warm launch, and applicable device acceptance rows. Build-only evidence must not be promoted to APK, device, release, or store qualification. |
| `GlobalSettings` substitution/precedence/save/draw/log behavior, `Common.SecretSettingTokens`, `ConfigBaker`, package signing fields or host directive resolution, the Android Gradle signing template, or workflow credential boundaries | Update [Security and Secrets](../../how-to/release/security-and-secrets.md), [Project Configuration](../../how-to/build/project-configuration.md), and affected release/platform docs; run `test_package_security.py`, `test_docs_security_and_secrets.py`, settings unit tests when native behavior changed, and the affected package lane. Inspect baked configs, generated package trees, logs, archives, caches, and manifests with synthetic values only; re-audit project secret provisioning, rotation, and incident routing without copying credentials into evidence. |
| `DataBase.*`, `Server.DbStorage`, `DataBase.*` settings, JSON/SQLite/Mongo/Memory storage, commit drain, recovery oplog, panic/reconnect, persisted startup/shutdown, migration compatibility, or project backup/restore procedure | Update [Persistence](../../explanation/persistence/), [Backup and Recovery](../../how-to/release/backup-and-recovery.md), and affected release/upgrade/security docs; run `test_docs_backup_recovery.py`, database unit tests, and the affected backend/project restore lane. Re-prove the complete durable set, consistency method, oplog behavior, isolated semantic restore, RPO/RTO evidence, and old/new binary compatibility without checking provider credentials or production data into the Engine. |
| `Examples/PublicRepositories.json`, `Examples/PublicRepositoryTemplate`, `Examples/MinimalProject`, public example ownership/lifecycle/remote visibility/pins, source-staging exclusions/materialization, common fixed settings required by the minimal config, or a compatibility boundary consumed by examples | Update [Public Example Repositories](../../how-to/build/public-example-repositories.md), regenerate/check the public-example model with `docs_examples.py`, run its focused tests, rematerialize the affected candidate from a clean remotely fetchable exact Engine commit, and validate each affected source-staged or published repository in pinned and current modes. Keep private remote state separate from source readiness, and do not publish or update external links until its owner-gated exit gate is green. |
| `BuildTools/buildtools.py` validation profiles, required workflow platform matrices, platform detection, application construction, runtime smoke evidence, or uploaded validation artifacts | Update `BuildTools/SupportMatrix.json` and the [Support Matrix](../../reference/platforms/support-matrix.md), regenerate/check its model/page with `docs_support_matrix.py`, run the affected validation target, and keep build, smoke, package-fixture, source-capable, and project-qualified claims separate. |
| `BuildTools/gameplay_test_runner.py`, its manifest schema, `Examples/GameplayTestHarness`, or an Engine example's process smoke | Update [Gameplay and Integration Testing](../../how-to/testing/gameplay-and-integration.md), `HelperCliInterface.json` when CLI syntax/ownership changes, and the affected example manifest; run `test_gameplay_test_runner.py`, `test_docs_gameplay_testing.py`, helper-CLI generation/tests, and at least one real baked headless server/client smoke. Keep project script registries, fixtures, content assertions, ports, backends, platform labs, and thresholds in the embedding project. |
| `Profiling_*` configurations, `FO_TRACY`/`TRACY_*` wiring, `TracyVersion.hpp`, native/script zones, frame marks, plots, log messages, thread names, or allocation tracking | Update [Profiling](../../how-to/quality/profiling.md), run `test_docs_profiling.py`, build the affected on-demand/total profile, and record one isolated client or server capture as appropriate. A Tracy update also requires matching capture/export tools and one capture on each runtime side before changing protocol/version claims. |
| Canonical English human prose, locale targets, glossary terms, or an existing Russian translation | Update the paired Russian page, retain identical fenced code, refresh only after review, regenerate `Docs/generated/translation-status.json` with `docs_localization.py`, then regenerate both locale search indexes and the route/navigation model with `docs_site.py`. Run focused parity, site, artifact, and browser locale-interaction tests. Complete manifest enforcement rejects every missing or stale page pair. |
| Reader-facing prose in any generated contract model, `Docs/description-translations.ru.json`, or a Russian generated reference | Keep canonical JSON and stable IDs unchanged, update the stable-locator translation and its exact source hash, regenerate the owning model/pages, then regenerate `Docs/generated/description-translation-status.json` with `docs_description_translations.py`. Run its focused test plus the owning generator test. Missing records remain explicit until semantic enforcement becomes `complete`; unknown, stale, position-based, type-changing, or code-changing records are defects. |
| `BuildTools/DocumentationDiagrams.json`, a diagram-owning document, diagram source provenance, caption/alt text, or shared diagram CSS | Update the source manifest and owning prose together, regenerate/check `Docs/generated/diagrams.json` plus `Docs/assets/diagrams/*.svg` with `docs_diagrams.py`, run `test_docs_diagrams.py`, rebuild Jekyll, and inspect the retained desktop/mobile diagram screenshots. Do not edit generated SVG by hand or use a diagram as the only representation of a required procedure. |
| `BuildTools/DocumentationScreenshots.json`, Mapper/SPARK/viewer UI, a capture fixture, screenshot-owning prose, image bytes, capture environment, or listed source provenance | Rebuild the exact recorded fixture, reproduce the interaction at the recorded viewport/backend, recapture without cosmetic edits, update the source manifest and owning alt/caption together, regenerate/check `Docs/generated/screenshots.json`, run `test_docs_screenshots.py`, rebuild Jekyll, and inspect the decoded desktop/mobile page. Never update only a hash to bless an unexplained visual difference. |
| A fenced block in public/current/human Markdown or `BuildTools/SnippetPolicy.json` | Declare a supported language, update [Documentation Snippet Validation](snippets.md) when policy changes, regenerate/check `Docs/generated/snippets.json`, run `test_docs_snippets.py` plus `docs_snippets.py --check --external`, and run the semantic compile/bake/smoke owner for every claimed outcome. Untyped/ignored fences are not allowed. |
| Export methods, native test files, or settings declarations | Regenerate/check `Docs/generated/source-inventory.json`. |
| Any inventoried Markdown, its visibility/state/owner/target/sources, publication URL, versioning policy, locale policy, or canonical generated model path | Update `Docs/documentation-manifest.json` as needed, regenerate/check snippets first when fenced content/scope changed, site delivery next, AI evaluation after search when affected, then root `llms.txt`, `llms-full.txt`, and `docs-manifest.json` with `docs_ai_delivery.py`. A context-budget failure requires an explicit manifest/policy review, never truncation. |
| Any public Markdown title/path/state/target, README locale pair, `site_delivery` navigation/routing/search/browser policy, Jekyll include/exclude rule, layout, or site asset | Regenerate/check `_data/docs-site.json`, both locale search indexes, and `Docs/generated/document-routes.json` with `docs_site.py`; run the focused source/layout/artifact/browser tests, `docs_site_artifact.py`, and the pinned `docs-browser` audit against the same completed `_site` tree. Route collisions, missing rendered pages/endpoints, wrong `html lang`, broken language pairs, cross-locale search results, browser runtime/resource failures, WCAG violations, keyboard/focus regressions, page overflow, ambiguous replacement owners, or a per-locale search-budget failure require an explicit policy fix, never silent document removal or a lowered gate. |
| `Docs/ai-evaluation.json`, a task-owning stable ID, an answer-evidence heading/term, compact-search tokenization/ranking, or the current source ref | Update [AI Documentation Evaluation](ai-evaluation.md), regenerate/check `Docs/generated/ai-evaluation-report.json` with `docs_ai_eval.py` after `docs_site.py`, run its focused test, and record model-family findings separately. Never add irrelevant keywords or lower a threshold merely to make a rank green. |
| Metadata baker or remote-call format/runtime | Update [Remote Calls](../../reference/scripting/remote-calls.md); an embedding project must rebake both sides and regenerate its project-owned remote-call catalog. |
| Module init, callback attributes, `Yield`, server script scheduling/synchronization, mutable-global policy, or entity callback teardown | Update [Script Lifecycle and Concurrency](../../how-to/scripting/lifecycle-and-concurrency.md), run `test_docs_script_lifecycle.py`, and run the narrow source/runtime tests named by the guide. |
| `.fos` module ordering, side macros, namespace/file ownership, CoreScripts formatting, mutable globals, attributed calls, generated-script discipline, or refactoring guidance | Update [AngelScript Style and Refactoring](../../how-to/scripting/style-and-refactoring.md) and its Russian mirror, run `test_docs_angelscript_style.py`, use the owning formatter, compile every affected side warning-free, and execute the narrowest behavior/contract test. Keep game comment language, domain vocabulary, generated formats, migrations, and gameplay acceptance in the embedding project. |
| `ModelInfoBaker`, `.fo3d` animation tokens, model mesh clip durations, state/action aliases, `ModelAnimationInfo.foinfo`, metadata registration, or duration script methods | Update [Model Animation](../../how-to/content/model-animation.md), run `test_docs_model_animation.py`, regenerate/check the native API/reference and aggregate diff when exports changed, run focused model-baker/common-script tests, and rebake an affected embedding project. |
| `ImageBaker::FrameShot::NextX` / `NextY`, baked sprite offsets, `SpriteSheet`, movement interpolation consumed by rendering, or `CritterHexView` walk/run phase and anchor behavior | Update [Sprite Root Motion](../../how-to/content/sprite-root-motion.md), run `test_docs_sprite_root_motion.py` plus focused image-baker tests, rebake affected sprite assets, and validate straight movement, turns, and stop/start in a visible client scene. |
| Build, package, platform, runtime, persistence, networking, or pointer/nullability behavior | Update the owning source-grounded page and run the narrow behavior/test path named there. |

After every generated-surface trigger, run `docs_contract_diff.py` against the preserved old models or the intended Git base. A zero native API delta does not dispose CMake, main CLI, package, helper CLI, native-extension, prototype-format, map-format, model-format, text-format, effect-format, image-format, particle-format, font-format, audio, video, or GUI runtime changes.

The update is not complete while a generator reports stale output, incoming behavior has no owning documentation disposition, conflict markers remain, or the safety stash is the only copy of unresolved work. Drop the safety stash only after final checks and an empty staged area are confirmed.

## Backlog status meanings

- `planned` вЂ” topic is identified but not researched yet.
- `researching` вЂ” source inspection is in progress.
- `drafted` вЂ” a first doc exists, but semantic validation is incomplete.
- `verified` вЂ” the page was checked against current source and post-edit mechanical checks passed.

Do not leave chat-only progress as the source of truth. If a slice is complete or blocked, record it in the backlog/report.

## Link and path validation

At minimum, validate:

- manifest coverage, ownership metadata, and declared source paths;
- Markdown links and anchors across every inventoried doc;
- resolved local links remain inside the engine root;
- Backticked source/build/doc paths that should exist in the engine checkout.
- Stale alternate-layout terms known from older snapshots.
- Test inventory coverage when editing [Testing](../testing/).
- `git diff --check`.
- staged area and working-tree status.

Run the complete standalone gate from the engine root. Test discovery prevents a newly added `test_docs_*.py` file from being silently omitted from the local procedure:

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

The `Validate documentation` and `Parse documentation snippets` jobs in `.github/workflows/validate.yml` are the authoritative CI expansion: they run each focused test and generator check explicitly, then classify contract changes against the base revision. Keep those jobs and this aggregate local route behaviorally equivalent. They do not require an embedding project or native build.

Changes that affect rendered output must also follow the [site publication guide](site-publication.md). Run `bundle exec jekyll build --trace`, `python BuildTools/docs_site_artifact.py --site-dir _site`, and the pinned browser audit when the Ruby/Bundler/Node environment is available; every pull request also receives a GitHub Pages-compatible `_site` artifact plus static and browser validation reports from the `Build documentation site` job.

Planned future docs should be written as plain prose unless the checker intentionally exempts them; do not backtick/link missing docs as if they already exist.

## Source-grounded writing conventions

- Start with purpose and reader routing.
- Include `Source paths inspected` for subsystem pages.
- Route to sibling docs instead of duplicating deep details.
- Use exact file paths for source ownership.
- Use tagged public example URLs for cross-project evidence; never make a local embedding-project checkout part of an engine procedure.
- Avoid promising unsupported workflows; say what the current source/test/build wiring supports.
- Update related docs when a change moves ownership between pages.

## AI maintainer notes

[AGENTS.md](../../../../AGENTS.md) is the AI-maintainer entry point. It routes AI agents to human docs and records repository conventions such as not committing/pushing without explicit instruction. Keep it concise and navigational; put detailed human-readable procedures in `Docs/`. Root `llms.txt`, `llms-full.txt`, and `docs-manifest.json` are generated retrieval routes for external agents; `_data/docs-site.json`, both locale search indexes, and `Docs/generated/document-routes.json` are the matching human navigation/search/routing projection. All seven must stay derived from the same manifest/corpus.

If a future AI agent continues this roadmap, it should re-anchor from git status, the backlog, and the verification report before editing. Context from chat is secondary to the repository state.

## Validation checklist

1. New docs are classified in `Docs/documentation-manifest.json` and linked from the [documentation index](../../index.md) and, if relevant, [AGENTS.md](../../../../AGENTS.md).
2. Backlog status matches actual source validation state.
3. Verification report records every promoted slice.
4. API changes have a base-revision report and every required public disposition passes [Contract Change Management](../contract-change-management.md).
5. AI delivery and human site delivery are regenerated and their focused tests/checks pass.
6. `python BuildTools/tests/test_docs_validate.py` and `python BuildTools/docs_validate.py` pass.
7. Link/path/test/stale-term checks pass after report updates.
8. `git diff --cached --name-only` is empty unless staging was explicitly requested.
9. Final report names changed files and confirms no commit/push happened unless requested.
