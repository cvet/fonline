# Documentation Maintenance

> Engine-owned documentation. This page explains how to keep the FOnline engine documentation source-grounded, navigable, and separated from embedding-project content.

## Purpose

Use this page when adding, verifying, or reorganizing engine docs. It is the maintainer workflow companion to the machine-readable [documentation manifest](documentation-manifest.json), [DocumentationBacklog.md](DocumentationBacklog.md), [DocumentationResearchTemplate.md](DocumentationResearchTemplate.md), [DocumentationVerificationReport.md](DocumentationVerificationReport.md), [SitePublication.md](SitePublication.md), [Docs/README.md](README.md), and [../AGENTS.md](../AGENTS.md).

## Source paths inspected

- `../AGENTS.md`
- `README.md`
- `Docs/README.md`
- `Docs/DocumentationBacklog.md`
- `Docs/DocumentationExpansionPlan.md`
- `Docs/DocumentationResearchTemplate.md`
- `Docs/DocumentationVerificationReport.md`
- `Docs/documentation-manifest.json`
- `Docs/generated/api.json`
- `Docs/generated/api/*.md`
- `Docs/contract-change-dispositions.json`
- `Docs/generated/source-inventory.json`
- `Docs/generated/cli.json`
- `Docs/generated/cli/*.md`
- `Docs/generated/helper-cli.json`
- `Docs/generated/helper-cli/*.md`
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
- `Docs/generated/particle-format/*.md`
- `Docs/generated/font-format.json`
- `Docs/generated/font-format/*.md`
- `Docs/generated/package.json`
- `Docs/generated/package/*.md`
- `Examples/PublicRepositories.json`
- `Examples/PublicRepositoryTemplate/`
- `BuildTools/docs_examples.py`
- `BuildTools/tests/test_docs_examples.py`
- `Docs/generated/public-examples.json`
- `Docs/generated/public-examples/*.md`
- `BuildTools/docs_ai_delivery.py`
- `BuildTools/tests/test_docs_ai_delivery.py`
- `BuildTools/docs_site.py`
- `BuildTools/tests/test_docs_site.py`
- `BuildTools/tests/test_docs_site_layout.py`
- `_data/docs-site.json`
- `assets/docs-search.json`
- `Docs/generated/document-routes.json`
- `llms.txt`
- `llms-full.txt`
- `docs-manifest.json`
- `BuildTools/docs_api.py`
- `BuildTools/docs_api_diff.py`
- `BuildTools/docs_contract_diff.py`
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
- `BuildTools/docs_package.py`
- `BuildTools/docs_validate.py`
- `BuildTools/tests/test_docs_api.py`
- `BuildTools/tests/test_docs_api_diff.py`
- `BuildTools/tests/test_docs_contract_diff.py`
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

## Standard doc slice workflow

1. Pick a coherent slice from [DocumentationBacklog.md](DocumentationBacklog.md).
2. Inspect the source paths named in the backlog and any related tests/build files.
3. Write or update the owning doc with `Source paths inspected`.
4. Prefer source relationships and ownership boundaries over long API trivia.
5. Add a validation checklist to deep subsystem docs.
6. Review the owning structured contract when a generated surface changes. For native API changes, update `///@ ApiContract` metadata as needed and use `BuildTools/docs_api_diff.py` for symbol-level diagnosis. For project-facing CMake changes, update `BuildTools/cmake/ProjectInterface.json` and run the structural CMake test. For main BuildTools CLI changes, keep `create_parser()` authoritative. For helper-script CLI changes, keep the executable `create_parser()` authoritative, update `BuildTools/HelperCliInterface.json` when ownership/audience/invocation changes, and regenerate the helper reference. For native-extension role/hook/binding changes, update `BuildTools/NativeExtensionInterface.json`, run its structural test, and validate the minimal starter or affected project. For prototype parser/property/metadata changes, update `BuildTools/PrototypeFormatInterface.json` and [PrototypeFormat.md](PrototypeFormat.md), regenerate its model/reference, run the focused test, and rebake an affected project. For map parser/baker/mapper/materialization changes, update `BuildTools/MapFormatInterface.json` and [MapFormat.md](MapFormat.md), regenerate its model/reference, run focused map tests, and rebake an affected project. For `.fo3d` parser state/tokens, FBX/OBJ import, model layers, attachments, particles, transforms, materials, cuts, rendering flags, or model limits, update `BuildTools/ModelFormatInterface.json` and [ModelFormat.md](ModelFormat.md), regenerate its model/reference, run focused model tests, and validate a visible client scene. For `.fotxt`, language normalization, prototype `$Text`, text script methods, or inline color tags, update `BuildTools/TextFormatInterface.json` and [TextAndLocalization.md](TextAndLocalization.md), regenerate its model/reference, run focused text tests, and rebake an affected project. For `.fofx`, effect state, shader resources, `EffectBaker`, backend bindings, `EffectManager`, script values, or `FO_EFFECT_*` limits, update `BuildTools/EffectFormatInterface.json` and [EffectFormat.md](EffectFormat.md), regenerate its model/reference, run focused effect tests, and validate every affected backend/profile in a visible client scene. For `ImageBaker`, FOFRM, built-in image loaders, baked sprite records, default factory coverage, atlas upload, or image caches, update `BuildTools/ImageFormatInterface.json` and [ImageFormat.md](ImageFormat.md), regenerate its model/reference, run focused image documentation/native tests, and rebake plus visibly inspect an affected project. For `FO_*_PARTICLES`, `.spark`/`.efkproj` parsing, `.spk`/`.efk` baking, backend composition, SPARK/Effekseer rendering, Mapper particle tools, particle caches, script methods, or model-particle links, update `BuildTools/ParticleFormatInterface.json` and [ParticleFormat.md](ParticleFormat.md), regenerate its model/reference, run focused particle documentation and baker/runtime tests, and rebake plus visibly inspect every affected backend and integration path. For `.fofnt`/`.fnt`, raw-copy selection, `FontManager`, slot/flag enums, bind-time scale, measurement, wrapping, or inline colors, update `BuildTools/FontFormatInterface.json` and [FontFormat.md](FontFormat.md), regenerate its model/reference, run focused font documentation and native tests, and rebake plus visibly inspect an affected project. For model animation tokens, clip durations, aliases, baked duration metadata, or script lookup changes, also update [ModelAnimation.md](ModelAnimation.md), run its focused source test, regenerate the native API/reference when needed, and rebake an affected project. For image-frame offsets, baked sprite offset transport, or client walk/run phase behavior, update [SpriteRootMotion.md](SpriteRootMotion.md), run its focused source test and image-baker tests, and validate the affected locomotion in a visible client scene. For package declarations or payload behavior, update `BuildTools/PackageInterface.json` and run the structural package test. For the public example portfolio, shared repository files, source scaffold, compatibility boundaries, or publication state, update `Examples/PublicRepositories.json` and [PublicExampleRepositories.md](PublicExampleRepositories.md), regenerate its model/reference, and validate affected external repositories in both Engine modes. Regenerate every affected runtime model, compare all fourteen runtime domains with `BuildTools/docs_contract_diff.py`, and complete [ApiChangeManagement.md](ApiChangeManagement.md) dispositions for baseline-public or model-contract breaks. Project-authored remote calls remain project-owned: bake both sides and regenerate/check their catalog with `BuildTools/docs_metadata.py`.
   For the current 3D subsystem, treat `ModelSourceLoader`, `ModelAnimationConverter`, `ModelAnimationData`, `ModelMeshData`, `ModelManager`, `ModelInformation`, `ModelInstance`, and `ModelAnimation` as co-owners with the two bakers. A parser, source, compatibility, mesh/rig wire, Ozz runtime, or ownership change updates both model guides and the structured model contract, runs the focused model/Ozz native suites, force-rebakes an affected project, proves the following incremental bake is clean, and validates the affected pose/composition visibly.
7. Add or update the page entry in `Docs/documentation-manifest.json`, keep its stable ID and locale target authoritative, and assign each public current human top-level page to exactly one `site_delivery.navigation` group. Regenerate/check both delivery layers in dependency order: `_data/docs-site.json`, `assets/docs-search.json`, and `Docs/generated/document-routes.json` with `BuildTools/docs_site.py`, followed by `llms.txt`, `llms-full.txt`, and `docs-manifest.json` with `BuildTools/docs_ai_delivery.py`. The public manifest hashes all site data; none of these files may be edited manually.
8. Update [Docs/README.md](README.md) when a new user-facing page is added.
9. Promote backlog status only after semantic source review, not just link checks.
10. Add a dated section to [DocumentationVerificationReport.md](DocumentationVerificationReport.md) with scope, sources, fixes, and checks.
11. Run `python BuildTools/docs_validate.py` and keep staging empty unless the owner explicitly asks to stage/commit.

### Moving a public document

Do not treat a file rename as sufficient route migration:

1. Keep the stable document ID on the new canonical page.
2. Move the canonical content to the manifest-owned English target and add the matching Russian target only when its reviewed translation exists.
3. Retain the old Markdown path as a short durable pointer to the canonical page so GitHub and Jekyll both preserve the legacy URL.
4. Mark the old record as a replacement/route alias and make the new record the one non-`replace` owner of the target.
5. Regenerate `Docs/generated/document-routes.json`; the old route must appear in `legacy_redirects` and resolve to the expected canonical document ID.
6. Update navigation/search only for the canonical page. A legacy pointer is not a second searchable owner.
7. Run the focused site, AI-delivery, standalone validation, and Jekyll artifact checks before removing any temporary migration state.

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
2. Find the owning page through [README.md](README.md) and the `sources` fields in [documentation-manifest.json](documentation-manifest.json).
3. Keep useful incoming documentation, but correct stale paths, project dependencies, unsupported claims, or missing validation evidence immediately.
4. Update the owning page and cross-links in the same worktree. An embedding-project workaround or test is not normative engine proof; reusable proof belongs under this repository.
5. Record the exact SHA range, contract changes, generated delta, affected docs, and checks in [DocumentationVerificationReport.md](DocumentationVerificationReport.md).

Generated-surface triggers:

| Incoming change | Required reconciliation |
|---|---|
| `BuildTools/codegen.py`, native metadata annotations, `Source/Scripting/`, or `Source/Common/Settings.inc` | Regenerate `Docs/generated/api.json` and native Markdown pages; use `docs_api_diff.py` for symbol details and include the model in the aggregate contract diff. |
| `BuildTools/cmake/ProjectInterface.json` or the project-facing CMake option/stage/helper implementation | Regenerate/check the CMake model/pages and run `validate_project_interface.cmake`. Add newly public declarations to the manifest rather than documenting implementation-only helpers. |
| `BuildTools/buildtools.py::create_parser()` or command-line help/default/choice behavior | Regenerate/check `Docs/generated/cli.json` and its Markdown pages with `docs_cli.py`; run the focused CLI documentation test. |
| A helper script's `create_parser()` or `BuildTools/HelperCliInterface.json` | Regenerate/check `Docs/generated/helper-cli.json` and its Markdown pages with `docs_helper_cli.py`; run the focused helper CLI test and review owner/audience/invocation changes. |
| `ProtoBaker`, `ConfigFile` prototype syntax, property text loading/serialization, `HasProtos` metadata, or `Baking.ProtoFileExtensions` | Update `BuildTools/PrototypeFormatInterface.json` and [PrototypeFormat.md](PrototypeFormat.md); regenerate/check the prototype-format model/pages, run `test_docs_prototype_format.py` and the aggregate diff, then rebake an affected embedding project and update its companion metadata/semantic docs. |
| `MapLoader`, `MapBaker`, mapper load/save, `ItemOwnership`, static-map loading, or map content materialization | Update `BuildTools/MapFormatInterface.json` and [MapFormat.md](MapFormat.md); regenerate/check the map-format model/pages, run `test_docs_map_format.py`, affected engine map tests, and the aggregate diff, then rebake an affected embedding project and update project map/content guidance. |
| `ModelMeshBaker`, `ModelSourceLoader`, `ModelAnimationConverter`, `ModelInfoBaker`, `.fo3d` syntax/state, FBX/OBJ import, model layers/links, particles, transforms, textures/effects, cuts, rendering flags, `ModelManager`/`ModelInformation`/`ModelInstance`/`ModelAnimation`, or `FO_MODEL_*` shape limits | Update `BuildTools/ModelFormatInterface.json`, [ModelFormat.md](ModelFormat.md), and [ModelAnimation.md](ModelAnimation.md) as applicable; regenerate/check the model-format model/pages, run both focused documentation suites plus the owning native model tests and aggregate diff, then force-rebake an affected project, verify the incremental bake settles, and validate every changed composition/animation in a visible client scene. |
| `TextPack`, `TextBaker`, `ProtoTextBaker`, `.fotxt`, `Baking.BakeLanguages`, `Client.Language`, text script methods, or inline `@color` parsing | Update `BuildTools/TextFormatInterface.json` and [TextAndLocalization.md](TextAndLocalization.md); regenerate/check the text-format model/pages, run `test_docs_text_format.py`, focused text-baker tests, and the aggregate diff, then rebake an affected project and validate language switching plus project formatting in a visible client. |
| `EffectBaker`, `.fofx` sections/state, `RenderEffect` buffers, renderer descriptor/pipeline handling, `EffectManager`, effect script methods, or `FO_EFFECT_*` limits | Update `BuildTools/EffectFormatInterface.json` and [EffectFormat.md](EffectFormat.md); regenerate/check the effect-format model/pages, run `test_docs_effect_format.py`, focused effect-baker tests, and the aggregate diff, then rebake an affected project and validate every changed slot/backend/profile in a visible client. |
| `ImageBaker`, FOFRM fields/flattening, built-in image loaders, baked sprite records, `DefaultSpriteFactory`, `SpriteManager` extension/cache behavior, or `TextureAtlas` upload | Update `BuildTools/ImageFormatInterface.json` and [ImageFormat.md](ImageFormat.md); regenerate/check the image-format model/pages, run `test_docs_image_format.py`, focused image/atlas tests, and the aggregate diff, then rebake an affected project and inspect dimensions, alpha, directions, cadence, hit masks, and relevant client profiles visibly. |
| `FO_*_PARTICLES`, `.spark`/`.efkproj` parsing, `.spk`/`.efk` baking, backend composition/rendering, Mapper particle tools, `ParticleManager`, `ParticleSpriteFactory`, script methods, or model-particle links | Update `BuildTools/ParticleFormatInterface.json` and [ParticleFormat.md](ParticleFormat.md); regenerate/check the particle-format model/pages, run `test_docs_particle_format.py`, focused baker/runtime/model tests, and the aggregate diff, then rebake an affected project and inspect every enabled backend plus sprite, map, script, and model-bone routes visibly. |
| `.fofnt`/`.fnt` parsing, `Baking.RawCopyFileExtensions`, `FontManager`, `FontType`, `FontFlag`, `TextFormat`, `Game.BindFont`, measurement, wrapping, or inline colors | Update `BuildTools/FontFormatInterface.json` and [FontFormat.md](FontFormat.md); regenerate/check the font-format model/pages, run `test_docs_font_format.py`, native unit tests, and the aggregate diff, then rebake an affected project and validate measurement plus visible text rendering. |
| `BuildTools/PackageInterface.json`, `DefinePackage`, `Packages.cmake`, or `package.py` target/platform/pack/payload behavior | Regenerate/check `Docs/generated/package.json` and its Markdown pages with `docs_package.py`; run the focused Python and structural CMake package tests plus an affected package path. |
| `Examples/PublicRepositories.json`, `Examples/PublicRepositoryTemplate`, `Examples/MinimalProject`, public example ownership/lifecycle/remote visibility/pins, common fixed settings required by the minimal config, or a compatibility boundary consumed by examples | Update [PublicExampleRepositories.md](PublicExampleRepositories.md), regenerate/check the public-example model with `docs_examples.py`, run its focused tests, and validate each affected source-staged or published repository in pinned and current modes. Keep private remote state separate from source readiness, and do not publish or update external links until its owner-gated exit gate is green. |
| Export methods, native test files, or settings declarations | Regenerate/check `Docs/generated/source-inventory.json`. |
| Any inventoried Markdown, its visibility/state/owner/target/sources, publication URL, versioning policy, locale policy, or canonical generated model path | Update `Docs/documentation-manifest.json` as needed, regenerate/check site delivery first, then regenerate/check root `llms.txt`, `llms-full.txt`, and `docs-manifest.json` with `docs_ai_delivery.py`. A context-budget failure requires an explicit manifest/policy review, never truncation. |
| Any public Markdown title/path/state/target, README locale pair, `site_delivery` navigation/routing/search policy, Jekyll layout, or site asset | Regenerate/check `_data/docs-site.json`, `assets/docs-search.json`, and `Docs/generated/document-routes.json` with `docs_site.py`; run both focused site tests and inspect desktop/mobile/search behavior in the Jekyll artifact. Route collisions, ambiguous replacement owners, or a search-budget failure require an explicit policy fix, never silent document removal. |
| Metadata baker or remote-call format/runtime | Update [RemoteCalls.md](RemoteCalls.md); an embedding project must rebake both sides and regenerate its project-owned remote-call catalog. |
| Module init, callback attributes, `Yield`, server script scheduling/synchronization, mutable-global policy, or entity callback teardown | Update [ScriptLifecycleAndConcurrency.md](ScriptLifecycleAndConcurrency.md), run `test_docs_script_lifecycle.py`, and run the narrow source/runtime tests named by the guide. |
| `ModelInfoBaker`, `.fo3d` animation tokens, model mesh clip durations, state/action aliases, `ModelAnimationInfo.foinfo`, metadata registration, or duration script methods | Update [ModelAnimation.md](ModelAnimation.md), run `test_docs_model_animation.py`, regenerate/check the native API/reference and aggregate diff when exports changed, run focused model-baker/common-script tests, and rebake an affected embedding project. |
| `ImageBaker::FrameShot::NextX` / `NextY`, baked sprite offsets, `SpriteSheet`, movement interpolation consumed by rendering, or `CritterHexView` walk/run phase and anchor behavior | Update [SpriteRootMotion.md](SpriteRootMotion.md), run `test_docs_sprite_root_motion.py` plus focused image-baker tests, rebake affected sprite assets, and validate straight movement, turns, and stop/start in a visible client scene. |
| Build, package, platform, runtime, persistence, networking, or pointer/nullability behavior | Update the owning source-grounded page and run the narrow behavior/test path named there. |

After every generated-surface trigger, run `docs_contract_diff.py` against the preserved old models or the intended Git base. A zero native API delta does not dispose CMake, main CLI, package, helper CLI, native-extension, prototype-format, map-format, model-format, text-format, effect-format, image-format, particle-format, or font-format changes.

The update is not complete while a generator reports stale output, incoming behavior has no owning documentation disposition, conflict markers remain, or the safety stash is the only copy of unresolved work. Drop the safety stash only after final checks and an empty staged area are confirmed.

## Backlog status meanings

- `planned` — topic is identified but not researched yet.
- `researching` — source inspection is in progress.
- `drafted` — a first doc exists, but semantic validation is incomplete.
- `verified` — the page was checked against current source and post-edit mechanical checks passed.

Do not leave chat-only progress as the source of truth. If a slice is complete or blocked, record it in the backlog/report.

## Link and path validation

At minimum, validate:

- manifest coverage, ownership metadata, and declared source paths;
- Markdown links and anchors across every inventoried doc;
- resolved local links remain inside the engine root;
- Backticked source/build/doc paths that should exist in the engine checkout.
- Stale alternate-layout terms known from older snapshots.
- Test inventory coverage when editing [Testing.md](Testing.md).
- `git diff --check`.
- staged area and working-tree status.

Run the fast standalone gate from the engine root:

```bash
python BuildTools/tests/test_docs_api.py
python BuildTools/tests/test_docs_api_diff.py
python BuildTools/tests/test_docs_contract_diff.py
python BuildTools/tests/test_docs_cli.py
python BuildTools/tests/test_docs_helper_cli.py
python BuildTools/tests/test_docs_native_extension.py
python BuildTools/tests/test_docs_prototype_format.py
python BuildTools/tests/test_docs_map_format.py
python BuildTools/tests/test_docs_model_format.py
python BuildTools/tests/test_docs_text_format.py
python BuildTools/tests/test_docs_effect_format.py
python BuildTools/tests/test_docs_image_format.py
python BuildTools/tests/test_docs_particle_format.py
python BuildTools/tests/test_docs_font_format.py
python BuildTools/tests/test_docs_package.py
python BuildTools/tests/test_docs_cmake.py
python BuildTools/tests/test_docs_reference.py
python BuildTools/tests/test_docs_script_lifecycle.py
python BuildTools/tests/test_docs_validate.py
python BuildTools/tests/test_docs_inventory.py
python BuildTools/tests/test_docs_examples.py
python BuildTools/tests/test_docs_site.py
python BuildTools/tests/test_docs_site_layout.py
python BuildTools/tests/test_docs_ai_delivery.py
cmake -P BuildTools/tests/validate_project_interface.cmake
cmake -P BuildTools/tests/validate_package_interface.cmake
cmake -P BuildTools/tests/validate_native_extension_interface.cmake
python BuildTools/docs_api.py --check
python BuildTools/docs_cli.py --check
python BuildTools/docs_helper_cli.py --check
python BuildTools/docs_native_extension.py --check
python BuildTools/docs_prototype_format.py --check
python BuildTools/docs_map_format.py --check
python BuildTools/docs_model_format.py --check
python BuildTools/docs_text_format.py --check
python BuildTools/docs_effect_format.py --check
python BuildTools/docs_image_format.py --check
python BuildTools/docs_particle_format.py --check
python BuildTools/docs_font_format.py --check
python BuildTools/docs_package.py --check
python BuildTools/docs_cmake.py --check
python BuildTools/docs_reference.py --check
python BuildTools/docs_inventory.py --check
python BuildTools/docs_examples.py --check
python BuildTools/docs_site.py --check
python BuildTools/docs_ai_delivery.py --check
python BuildTools/docs_validate.py
```

The same commands run in the `Validate documentation` GitHub Actions job and do not require an embedding project or native build.

Changes that affect rendered output must also follow [SitePublication.md](SitePublication.md). Run `bundle exec jekyll build --trace` when the pinned Ruby/Bundler environment is available; every pull request also receives a GitHub Pages-compatible `_site` artifact from the `Build documentation site` job.

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

[../AGENTS.md](../AGENTS.md) is the AI-maintainer entry point. It routes AI agents to human docs and records repository conventions such as not committing/pushing without explicit instruction. Keep it concise and navigational; put detailed human-readable procedures in `Docs/`. Root `llms.txt`, `llms-full.txt`, and `docs-manifest.json` are generated retrieval routes for external agents; `_data/docs-site.json`, `assets/docs-search.json`, and `Docs/generated/document-routes.json` are the matching human navigation/search/routing projection. All six must stay derived from the same manifest/corpus.

If a future AI agent continues this roadmap, it should re-anchor from git status, the backlog, and the verification report before editing. Context from chat is secondary to the repository state.

## Validation checklist

1. New docs are classified in `Docs/documentation-manifest.json` and linked from [Docs/README.md](README.md) and, if relevant, [../AGENTS.md](../AGENTS.md).
2. Backlog status matches actual source validation state.
3. Verification report records every promoted slice.
4. API changes have a base-revision report and every required public disposition passes [ApiChangeManagement.md](ApiChangeManagement.md).
5. AI delivery and human site delivery are regenerated and their focused tests/checks pass.
6. `python BuildTools/tests/test_docs_validate.py` and `python BuildTools/docs_validate.py` pass.
7. Link/path/test/stale-term checks pass after report updates.
8. `git diff --cached --name-only` is empty unless staging was explicitly requested.
9. Final report names changed files and confirms no commit/push happened unless requested.
