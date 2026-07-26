# FOnline Engine Documentation

This directory contains maintained documentation for reusable engine behavior. It is the user-facing documentation hub for developers who embed or modify FOnline.

## Start here

- [../TUTORIAL.md](../TUTORIAL.md) - tested first headless project lesson.
- [../Examples/MinimalProject/README.md](../Examples/MinimalProject/README.md) - canonical engine-owned starter and CI smoke contract.
- [PublicExampleRepositories.md](PublicExampleRepositories.md) - public example portfolio, ownership, repository template, exact Engine pins, compatibility lanes, releases, support, and asset provenance.
- [NativeExtensions.md](NativeExtensions.md) - project-native C++ roles, hooks, exports, lifecycle, dependencies, compatibility, and validation.
- [PrototypeFormat.md](PrototypeFormat.md) - engine-owned prototype syntax, identity, inheritance, properties, references, migrations, and project boundary.
- [MapFormat.md](MapFormat.md) - `.fomap` sections, placement identity, ownership, mapper round-trip, side-specific baking, and runtime materialization.
- [ModelFormat.md](ModelFormat.md) - `.fo3d` syntax, FBX/OBJ inputs, layers, attachments, transforms, materials, cuts, runtime composition, and validation.
- [ImageFormat.md](ImageFormat.md) - PNG/TGA and legacy image import, FOFRM composition, baked sprites, runtime factories, atlases, caches, and validation.
- [EffectFormat.md](EffectFormat.md) - `.fofx` sections, passes, render state, shader resources, baking outputs, runtime caching, script values, and validation.
- [ParticleFormat.md](ParticleFormat.md) - optional SPARK/Effekseer selection, `.spark`/`.efkproj` authoring, `.spk`/`.efk` baking, Mapper tools, runtime routes, script/model integration, and validation.
- [FontFormat.md](FontFormat.md) - FOFNT and AngelCode BMFont descriptors, binding, scaling, measurement, wrapping, rendering flags, inline colors, and validation.
- [ModelAnimation.md](ModelAnimation.md) - `.fo3d` animation tuples, `AnimSpeed`, one-step aliases, baked duration metadata, typed script lookup, and project boundary.
- [SpriteRootMotion.md](SpriteRootMotion.md) - 2D sprite `NextX`/`NextY` offsets, walk/run phase, frame selection, visual alignment, and project boundary.
- [SitePublication.md](SitePublication.md) - GitHub Pages/Jekyll environment, generated navigation/search, local preview, CI `_site` artifact, custom-domain contract, and production verification.
- [GettingStarted.md](GettingStarted.md) — first route through the engine docs.
- [EmbeddingProject.md](EmbeddingProject.md) — how a game repository should embed and own the engine.
- [BuildWorkflow.md](BuildWorkflow.md) — build, presets, platform prerequisites, and validation strategy.
- [ThirdPartyMaintenance.md](ThirdPartyMaintenance.md) — engine vendored dependency update, pruning, version pin, and local patch workflow.
- [ProductionDocumentationPlan.md](ProductionDocumentationPlan.md) — active production-level roadmap for standalone developer docs, generated reference, public examples, AI delivery, and English/Russian Markdown publication through GitHub Pages/Jekyll at `fonline.ru`.
- [DocumentationExpansionPlan.md](DocumentationExpansionPlan.md) — completed historical roadmap for the initial source-coverage pass.
- [DocumentationBacklog.md](DocumentationBacklog.md) — planned and verified documentation slices.
- [DocumentationVerificationReport.md](DocumentationVerificationReport.md) — source-check reports for recently verified slices.
- [DocumentationResearchTemplate.md](DocumentationResearchTemplate.md) — checklist/template for source-grounded doc slices.
- [DocumentationMaintenance.md](DocumentationMaintenance.md) — maintainer workflow for source-grounded docs, backlog/report updates, and validation.
- [documentation-manifest.json](documentation-manifest.json) — machine-readable stable IDs, audiences, owners, lifecycle state, migration paths, and source ownership for every maintained Markdown entry.
- [../llms.txt](../llms.txt) - generated public current-page route and canonical machine-readable reference map for AI clients.
- [../llms-full.txt](../llms-full.txt) - generated, 1.25 MiB-bounded standalone context bundle of public current Markdown and generated-reference indexes.
- [../docs-manifest.json](../docs-manifest.json) - generated public document catalog with canonical/source URLs, provenance, lifecycle metadata, byte sizes, and normalized content hashes.
- [../_data/docs-site.json](../_data/docs-site.json) - generated Jekyll navigation groups and rolling source identity derived from stable document IDs.
- [../assets/docs-search.json](../assets/docs-search.json) - generated compact static search index for all public current human documentation.
- [generated/document-routes.json](generated/document-routes.json) - generated current URL map, planned English/Russian paths, canonical target owners, and required legacy redirects.
- [generated/api/index.md](generated/api/index.md) - generated, searchable native API reference for methods, properties, events, types, settings, and migrations.
- [generated/api.json](generated/api.json) - canonical source-parsed native codegen API model with symbol IDs, signatures, runtime sides, stability, and provenance.
- [generated/cmake/index.md](generated/cmake/index.md) - generated CMake project-interface reference for options, stages, hooks, and selected embedding helpers.
- [generated/cmake.json](generated/cmake.json) - canonical machine-readable CMake interface model consumed from the same manifest as configure-time declarations.
- [generated/cli/index.md](generated/cli/index.md) - generated BuildTools command-line reference from the executable `argparse` parser.
- [generated/cli.json](generated/cli.json) - canonical machine-readable BuildTools CLI model with stable command and argument IDs.
- [generated/package/index.md](generated/package/index.md) - generated `DefinePackage`, platform, pack-token, payload, artifact, and internal packager CLI reference.
- [generated/package.json](generated/package.json) - canonical runtime-consumed package interface model with stable package IDs.
- [generated/public-examples/index.md](generated/public-examples/index.md) - generated public example repository portfolio, owners, dependencies, checks, artifacts, and exit gates.
- [generated/public-examples.json](generated/public-examples.json) - canonical machine-readable public example program and governance contract.
- [generated/prototype-format/index.md](generated/prototype-format/index.md) - generated prototype sections, directives, built-in types/properties, and validation rules.
- [generated/prototype-format.json](generated/prototype-format.json) - canonical source-backed prototype-format model with stable contract IDs.
- [generated/map-format/index.md](generated/map-format/index.md) - generated map sections, directives, ownership modes, property catalog, baking split, and validation rules.
- [generated/map-format.json](generated/map-format.json) - canonical source-backed map-format model with stable contract IDs.
- [generated/model-format/index.md](generated/model-format/index.md) - generated `.fo3d` token, composition, asset, animation, and validation reference.
- [generated/model-format.json](generated/model-format.json) - canonical source-backed model-format contract with exact parser-token coverage.
- [generated/text-format/index.md](generated/text-format/index.md) - generated `.fotxt`, language normalization, prototype-text, runtime lookup, color-tag, and validation reference.
- [generated/text-format.json](generated/text-format.json) - canonical source-backed text-format contract.
- [generated/effect-format/index.md](generated/effect-format/index.md) - generated `.fofx` syntax, render-state, resource, baking, runtime, and validation reference.
- [generated/effect-format.json](generated/effect-format.json) - canonical source-backed effect-format contract.
- [generated/image-format/index.md](generated/image-format/index.md) - generated image-source, FOFRM, filename-option, baking, runtime, and validation reference.
- [generated/image-format.json](generated/image-format.json) - canonical source-backed image-format contract.
- [generated/particle-format/index.md](generated/particle-format/index.md) - generated particle backend, source/runtime form, baking, rendering, tooling, integration, and validation reference.
- [generated/particle-format.json](generated/particle-format.json) - canonical source-backed particle-format contract.
- [generated/font-format/index.md](generated/font-format/index.md) - generated font descriptor, binding, layout, rendering, and validation reference.
- [generated/font-format.json](generated/font-format.json) - canonical source-backed font-format contract.
- [ApiChangeManagement.md](ApiChangeManagement.md) - fourteen-domain base-revision contract diff, breaking-change classification, shared disposition ledger, and CI gate.
- [generated/source-inventory.json](generated/source-inventory.json) — deterministic source inventory for exported script methods, engine test files, and settings declarations.
- [Decisions/0001-github-pages-markdown-publication.md](Decisions/0001-github-pages-markdown-publication.md) — accepted GitHub Pages/Jekyll, Markdown, `fonline.ru`, and English/Russian locale-layout contract.
- [Decisions/0002-public-api-stability-contract.md](Decisions/0002-public-api-stability-contract.md) — accepted stability labels, source-of-truth order, and breaking-change requirements for the public API.
- [Decisions/0003-manifest-backed-ai-documentation-delivery.md](Decisions/0003-manifest-backed-ai-documentation-delivery.md) - accepted manifest-backed `llms.txt`, bounded context, public manifest, and deterministic-hash contract.
- [Decisions/0004-manifest-backed-site-navigation-search.md](Decisions/0004-manifest-backed-site-navigation-search.md) - accepted manifest-backed Jekyll navigation, compact static search, responsive layout, and rolling-version contract.
- [Decisions/0005-public-example-repository-ownership.md](Decisions/0005-public-example-repository-ownership.md) - accepted external example ownership, exact-pin, compatibility, governance, and publication-authority contract.
- [Decisions/0006-documentation-version-locale-routing.md](Decisions/0006-documentation-version-locale-routing.md) - accepted rolling/current version, deferred release snapshot, bilingual target, and durable legacy-route contract.
- [../README.md](../README.md) — polished repository landing page.
- [../AGENTS.md](../AGENTS.md) — AI-maintainer entry point and working conventions.

## Architecture and source navigation

- [Architecture.md](Architecture.md) — engine layer map and runtime/build composition overview.
- [SourceTree.md](SourceTree.md) — source-directory guide for maintainers.
- [Applications.md](Applications.md) — executable/library entry points and CMake target ownership notes.
- [Essentials.md](Essentials.md) — low-level platform, logging, memory, filesystem, serialization, sockets, utility layer, and `vector` / `small_vector` selection rules.
- [SmartPointers.md](SmartPointers.md) — native C++ pointer ownership/nullability vocabulary and migration rules.
- [ThreadSafetyAnalysis.md](ThreadSafetyAnalysis.md) — `FO_TSA_*` Clang Thread Safety Analysis annotations, locking primitives, and `-Werror=thread-safety` enforcement.
- [LocalVariables.md](LocalVariables.md) — explicit simple local types, redundant top-level local `const`, and use-after-move validation.

## Build and generation

- [generated/cmake/index.md](generated/cmake/index.md) - exact generated project options, stage order, hook points, and published helper signatures.
- [generated/cli/index.md](generated/cli/index.md) - exact generated `buildtools.py` commands, arguments, defaults, choices, and help output.
- [generated/helper-cli/index.md](generated/helper-cli/index.md) - exact generated codegen, coverage, Android, web-server, MSI, and other helper-script command lines.
- [generated/native-extension/index.md](generated/native-extension/index.md) - exact generated source roles, engine hooks, fallbacks, and native binding rules.
- [generated/prototype-format/index.md](generated/prototype-format/index.md) - exact generated prototype grammar, built-in property applicability, and validation rules.
- [generated/map-format/index.md](generated/map-format/index.md) - exact generated `.fomap` grammar, ownership, property applicability, bake split, and validation rules.
- [generated/model-format/index.md](generated/model-format/index.md) - exact generated `.fo3d` tokens, source assets, composition, animation adjacency, and validation rules.
- [generated/text-format/index.md](generated/text-format/index.md) - exact generated `.fotxt`, language normalization, prototype-text, runtime lookup, and validation rules.
- [generated/effect-format/index.md](generated/effect-format/index.md) - exact generated `.fofx` sections, render state, shader resources, backend outputs, runtime cache, and validation rules.
- [generated/image-format/index.md](generated/image-format/index.md) - exact generated source formats, FOFRM fields, legacy selectors, baked records, runtime factories, atlases, caches, and validation rules.
- [generated/particle-format/index.md](generated/particle-format/index.md) - exact optional backends, source/runtime forms, baker boundaries, rendering routes, tooling, integrations, and validation rules.
- [generated/font-format/index.md](generated/font-format/index.md) - exact FOFNT/BMFont descriptors, slot binding, scale constraints, layout, rendering flags, colors, and validation rules.
- [generated/package/index.md](generated/package/index.md) - exact package declaration grammar, support matrix, payload layouts, and artifact outputs.
- [generated/public-examples/index.md](generated/public-examples/index.md) - exact external example portfolio, ownership, compatibility lanes, required artifacts, and publication gates.
- [BuildWorkflow.md](BuildWorkflow.md) — build, presets, platform prerequisites, and validation strategy.
- [ThirdPartyMaintenance.md](ThirdPartyMaintenance.md) — engine vendored dependency update, pruning, version pin, and local patch workflow.
- [BuildToolsPipeline.md](BuildToolsPipeline.md) — staged CMake pipeline, hooks, helpers, and change routing.
- [BakingPipeline.md](BakingPipeline.md) — resource baking, baker classes, script compile adjacency, and validation.
- [GeneratedApiAndMetadata.md](GeneratedApiAndMetadata.md) — codegen, metadata registration, generated files, and property contracts.
- [PrototypeFormat.md](PrototypeFormat.md) - prototype authoring model, inheritance, references, migrations, and engine/project ownership.
- [MapFormat.md](MapFormat.md) - map source authoring, placement ownership, mapper normalization, and server/client bake behavior.
- [ModelFormat.md](ModelFormat.md) - `.fo3d` syntax, source meshes, includes, layers, attachments, transforms, materials, cuts, baking, and runtime composition.
- [TextAndLocalization.md](TextAndLocalization.md) - raw text packs, language baking/fallback, prototype `$Text`, script lookup, renderer color tags, and project-owned formatting.
- [EffectFormat.md](EffectFormat.md) - effect source authoring, vertex/resource contracts, baking, backend bindings, runtime selection, and ScriptValue ownership.
- [ImageFormat.md](ImageFormat.md) - image source selection, FOFRM composition, legacy import options, baked/runtime boundaries, atlas behavior, and project validation.
- [ParticleFormat.md](ParticleFormat.md) - SPARK/Effekseer authoring, generated resources, Engine baker/renderer/tool behavior, caches, render routes, and project validation.
- [FontFormat.md](FontFormat.md) - bitmap-font authoring, resource delivery, slot binding, measurement/drawing behavior, rendering flags, and project validation.
- [ModelAnimation.md](ModelAnimation.md) - model animation tuple authoring, effective durations, aliases, bake/runtime behavior, and typed lookups.
- [SpriteRootMotion.md](SpriteRootMotion.md) - per-frame sprite displacement, baked/runtime transport, movement-driven walk cycles, and visual validation.
- [ApiChangeManagement.md](ApiChangeManagement.md) - fourteen-domain canonical model comparison and public breaking-change workflow.
- [ConfigurationAndDataSources.md](ConfigurationAndDataSources.md) — config parsing, settings application, resource-pack data sources, file lookup, and caches.

## Runtime model

- [EntityModel.md](EntityModel.md) — entity/property/prototype runtime model, holders, events, and serialization relationships.
- [ExceptionSafety.md](ExceptionSafety.md) — engine-invariant stability under exceptions: terminate-on-OOM allocation model, the entity-lifecycle throw-as-signal contract, post-mutation `FO_STRONG_ASSERT` policy, and the disposition decision rules.
- [MapsMovementGeometry.md](MapsMovementGeometry.md) — map coordinates, geometry modes, path finding, line tracing, movement contexts, and map loading.
- [Networking.md](Networking.md) — network buffers, command packing, client/server transports, and ordered UDP.
- [Persistence.md](Persistence.md) — server database facade, backend implementations, commit queue, and recovery logs.

## Client, frontend, and platform runtime

- [ClientRuntime.md](ClientRuntime.md) — client lifecycle, server connection, view entities, resources, sprites, render-target bridge, and client tests.
- [ServerRuntime.md](ServerRuntime.md) — authoritative server lifecycle, managers, entity ownership, networking, persistence, movement, and updater backend.
- [FrontendAndRendering.md](FrontendAndRendering.md) — application abstraction, windows/input/audio, headless/stub modes, renderers, the screen-size/resolution + letterbox model (windowed, fullscreen, multi-client virtual windows), effects, and platform package boundaries.
- [ClientUpdater.md](ClientUpdater.md) — client host/runtime split, runtime ABI, updater protocol, and server-side updater backend.
- [Debugging.md](Debugging.md) — native debugger support, stack traces, Visual Studio helpers, and client host/runtime validation.
- [Scripting.md](Scripting.md) — script system lifecycle, AngelScript backend, native method exports, core scripts, and compile flow.
- [ScriptLifecycleAndConcurrency.md](ScriptLifecycleAndConcurrency.md) - module initialization, callback ownership, `[[Async]]`/`Yield`, server synchronization covers, mutable-state ownership, and teardown.
- [RemoteCalls.md](RemoteCalls.md) - client/server declaration grammar, generated caller surfaces, handler binding, authority rules, baked project catalog, and validation.
- [ScriptMethodsMap.md](ScriptMethodsMap.md) — native `///@ ExportMethod` file map by runtime side and receiver family.
- [Nullability.md](Nullability.md) — `T?` script / `ptr<T>`·`nptr<T>` native boundary contract and project-analyzer requirements.
- [SmartPointers.md](SmartPointers.md) — C++ `ptr` / `nptr` / owning-pointer nullability contracts.
- [Tools.md](Tools.md) — engine tool map: baker, AS compiler, mapper, editor, asset explorer, particle editor, and asset processors.
- [MapperTools.md](MapperTools.md) — mapper lifecycle, native mapper helpers, and reusable headless-capture integration.
- [WebDebugging.md](WebDebugging.md) — WebAssembly target preparation, package layout, and debug workflow.
- [AndroidDebugging.md](AndroidDebugging.md) — Android target preparation, package layout, ADB workflow, and host-server connection debugging.

## Source and tooling references

- [../Source/README.md](../Source/README.md) — source-tree overview.
- [Testing.md](Testing.md) — maintained full test-suite map, generated test targets, coverage routing, and validation ownership.
- [../Source/Tests/README.md](../Source/Tests/README.md) — short source-tree entry point for engine unit-test suites.
- [../BuildTools/README.md](../BuildTools/README.md) — build tooling notes.
- [../PUBLIC_API.md](../PUBLIC_API.md) — public native API notes.
- [../TUTORIAL.md](../TUTORIAL.md) — tutorial material.

## Documentation ownership

Use engine docs for reusable engine mechanics: runtime behavior, tool contracts, platform build/debug flows, generated API mechanics, updater protocol, mapper tooling, and script/native conventions.

Use the embedding project's docs for concrete game content, product rules, quests, balance, text, maps, release policy, and project-specific commands.

Engine docs must not depend on embedding-project files, scripts, tests, CI, or generated artifacts as the proof of engine behavior. If a reusable helper or regression test is cited from an engine doc, it belongs in the engine repository. Cross-project examples use stable HTTPS links to tagged public revisions; local Markdown links must remain inside the engine root.
