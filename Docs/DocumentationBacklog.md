# Documentation Backlog

This backlog tracks the planned engine documentation set. It exists so documentation work stays source-grounded: every topic lists the source areas to inspect before writing.

## Status legend

- `planned` — topic is identified but not researched yet.
- `researching` — source/tests/build files are being inspected.
- `drafted` — a first doc exists, but it may need deeper validation.
- `verified` — links and formatting were checked after the latest edit.

## Navigation and maintenance

- `verified` — [README.md](../README.md), [Docs/README.md](README.md), [GettingStarted.md](GettingStarted.md), [EmbeddingProject.md](EmbeddingProject.md), [BuildWorkflow.md](BuildWorkflow.md), [DocumentationExpansionPlan.md](DocumentationExpansionPlan.md), this backlog, and [DocumentationResearchTemplate.md](DocumentationResearchTemplate.md).
- `verified` — [SourceTree.md](SourceTree.md).
- `verified` — [Architecture.md](Architecture.md), [Applications.md](Applications.md)
  - Verified against `Source/Applications/`, common engine base/entity/script-system owners, client/server/frontend entry points, and application CMake wiring on 2026-05-18.

## Architecture and source navigation

- `verified` — [Architecture.md](Architecture.md)
  - Verified against `Source/Applications/`, `Source/Common/EngineBase.*`, `Source/Common/Entity.*`, `Source/Common/ScriptSystem.*`, client/server/frontend owners, and application CMake wiring on 2026-05-18.
- `verified` — [SourceTree.md](SourceTree.md)
  - Verified against current top-level `Source/` areas, `BuildTools/cmake/stages/Applications.cmake`, and `Source/Tests/` on 2026-05-18.
- `verified` — [Applications.md](Applications.md)
  - Verified against all current `Source/Applications/*.cpp` entry points plus `BuildTools/cmake/stages/Applications.cmake` and `BuildTools/cmake/helpers/Build.cmake` on 2026-05-18.

## Build, generation, and resources

- `verified` — [BuildWorkflow.md](BuildWorkflow.md)
  - Verified against engine CMake/BuildTools entry points, staged CMake build/test/package wiring, validators, `Source/Applications/TestingApp.cpp`, and test docs on 2026-05-18.
- `verified` — [BuildToolsPipeline.md](BuildToolsPipeline.md)
  - Verified against `BuildTools/Init.cmake`, current staged CMake files, helpers, codegen/package scripts, and build-generation ownership boundaries on 2026-05-18.
- `verified` — [BakingPipeline.md](BakingPipeline.md)
  - Verified against `BuildTools/cmake/stages/ScriptsAndBaking.cmake`, build-hash helper, baker apps/library entry points, all current `Source/Tools/*Baker.*` implementations, and baker tests on 2026-05-18.
- `verified` — [GeneratedApiAndMetadata.md](GeneratedApiAndMetadata.md)
  - Verified against `BuildTools/cmake/stages/Codegen.cmake`, `BuildTools/codegen.py`, metadata registration templates/runtime, property/entity model owners, `MetadataBaker`, and metadata/property tests on 2026-05-18.

## Runtime model

- `verified` — [EntityModel.md](EntityModel.md)
  - Verified against `Source/Common/Entity.*`, `EntityProperties.*`, `EntityProtos.*`, `Properties.*`, `PropertiesSerializator.*`, `ProtoManager.*`, and entity/property tests on 2026-05-18.
- `verified` — [MapsMovementGeometry.md](MapsMovementGeometry.md)
  - Verified against `Source/Common/Geometry.*`, `LineTracer.*`, `Movement.*`, `PathFinding.*`, `MapLoader.*`, `Source/Tools/MapBaker.*`, and map/movement tests on 2026-05-18.
- `verified` — [Networking.md](Networking.md)
  - Verified against `Source/Common/NetBuffer.*`, `NetworkUdp.*`, `Source/Client/NetworkClient*`, `Source/Server/NetworkServer*`, network tests, and client/server integration tests on 2026-05-18.
- `verified` — [Persistence.md](Persistence.md)
  - Verified against `Source/Server/DataBase.*`, `Source/Server/DataBase-*.cpp`, and `Source/Tests/Test_DataBase.cpp` on 2026-05-18.

## Client, server, and platform runtime

- `verified` — [ClientRuntime.md](ClientRuntime.md)
  - Verified against `Source/Client/Client.*`, `ClientConnection.*`, `ResourceManager.*`, client view/sprite/render-target managers, and client tests on 2026-05-18.
- `verified` — [FrontendAndRendering.md](FrontendAndRendering.md)
  - Verified against `Source/Frontend/Application*.cpp`, `Rendering*.cpp`, `Source/Client/RenderTarget.*`, `SpriteManager.*`, `EffectManager.*`, `BuildTools/cmake/stages/Packages.cmake`, and `Source/Tests/Test_Rendering.cpp` on 2026-05-18.
- `verified` — [ServerRuntime.md](ServerRuntime.md)
  - Verified against `Source/Server/Server.*`, `Source/Server/*Manager.*`, `Source/Server/Player.*`, `Source/Server/Critter.*`, `Source/Server/Map.*`, `Source/Server/Location.*`, `Source/Server/Item.*`, `Source/Server/ClientDataValidation.*`, `Source/Server/UpdaterBackend.*`, server tests, and integration tests on 2026-05-18.
- `verified` — [ClientUpdater.md](ClientUpdater.md)
  - Verified against `Source/Applications/ClientApp.cpp`, `ClientLib.cpp`, `Source/Client/ClientRuntimeApi.*`, `Source/Client/Updater.*`, `Source/Server/UpdaterBackend.*`, `Source/Server/Server.cpp`, updater settings, packaging code, runtime API tests, and updater pipeline tests on 2026-05-18.
- `verified` — [WebDebugging.md](WebDebugging.md), [AndroidDebugging.md](AndroidDebugging.md), [Debugging.md](Debugging.md)
  - Verified against platform BuildTools flows, web/Android package helpers, VS Code task/launch files, frontend/application debugging hooks, stack-trace/exception handling code, natvis files, and stack/exception tests on 2026-05-18.

## Scripting and tools

- `verified` — [Scripting.md](Scripting.md)
  - Verified against `Source/Common/ScriptSystem.*`, AngelScript backend/runtime/compiler files, core scripts, native script method files, build script-compilation wiring, and scripting tests on 2026-05-18.
- `verified` — [ScriptMethodsMap.md](ScriptMethodsMap.md)
  - Verified against all 18 current `Source/Scripting/*ScriptMethods.cpp` files and 874 current `///@ ExportMethod` declarations; count refreshed on 2026-05-21 after default-argument overload collapse.
- `verified` — [Nullability.md](Nullability.md)
  - Verified against AngelScript nullable suffix parsing, the `ptr<T>`·`nptr<T>` native pointer vocabulary, codegen runtime checks, nullable analyzer tools, parent VS Code/CI task wiring, and nullable/script tests on 2026-05-18.
- `verified` — [Tools.md](Tools.md)
  - Verified against current `Source/Tools/*.h`, `Source/Tools/*.cpp`, tool application entry points, baker tests, mapper/editor/asset/particle owners, and tool/build routing docs on 2026-05-18.
- `verified` — [MapperTools.md](MapperTools.md)
  - Verified against `Source/Applications/MapperApp.cpp`, `Source/Tools/Mapper.*`, mapper/common script methods, client map-view helpers, geometry transforms, and the current embedding-project mapper-render/map-preview scripts on 2026-05-18.

## Essentials, infrastructure, and tests

- `verified` — [Essentials.md](Essentials.md)
  - Created and verified against `Source/Essentials/*.h`, `Source/Essentials/*.cpp`, `BuildTools/cmake/stages/EngineSources.cmake`, and essentials tests on 2026-05-18.
- `verified` — [ConfigurationAndDataSources.md](ConfigurationAndDataSources.md)
  - Created and verified against `Source/Common/ConfigFile.*`, `Settings.*`, `DataSource.*`, `FileSystem.*`, `CacheStorage.*`, `Source/Essentials/DiskFileSystem.*`, resource/baker consumers, and focused tests on 2026-05-18.
- `verified` — [Testing.md](Testing.md)
  - Created and verified against `Source/Applications/TestingApp.cpp`, all 79 current `Source/Tests/Test_*.cpp` files, `FO_TESTS_SOURCE`, generated test/coverage target wiring, and `Source/Tests/README.md` on 2026-05-18.
- `verified` — [DocumentationMaintenance.md](DocumentationMaintenance.md)
  - Created and verified against `../AGENTS.md`, the docs hub, backlog, expansion plan, research template, verification report, and current source-grounded doc conventions on 2026-05-18.

## Native conventions and safety contracts

Docs added after the initial 2026-05-18 backlog slice, covering native C++ vocabulary and safety contracts referenced from `AGENTS.md`:

- `verified` — [SmartPointers.md](SmartPointers.md)
  - Verified against `Source/Essentials/SmartPointers.h` and the embedding-project audit tooling reference (`Tools/SmartPointerAudit/smart_pointer_audit.py`) on 2026-07-08.
- `verified` — [ExceptionSafety.md](ExceptionSafety.md)
  - Verified against `Source/Essentials/MemorySystem.h`, `ExceptionHandling.h`, `Containers.h`, `CommonHelpers.h`, `BasicCore.h`, `Source/Server/DataBase.cpp`, `Source/Server/WorkerPool.cpp`, and the pinning tests `Source/Tests/Test_EntityLifecycle.cpp` / `Test_ServerMapOperations.cpp` on 2026-07-08.
- `verified` — [ThreadSafetyAnalysis.md](ThreadSafetyAnalysis.md)
  - Verified against `Source/Essentials/Threading.h` `FO_TSA_*` annotations and the `-Werror=thread-safety` toolchain enforcement in `BuildTools/cmake/stages/Init.cmake` on 2026-07-08.

## Production documentation program

- `verified` — [ProductionDocumentationPlan.md](ProductionDocumentationPlan.md)
  - Baseline audit completed on 2026-07-10 against the standalone engine docs/source/build/test surfaces, the Last Frontier documentation corpus, and the public TLA integration project.
  - The plan defines the next program: standalone independence, public-contract generation, first-run tutorials, public example repositories, professional publication, AI-readable outputs, and a final English/Russian mirror.
  - `verified` describes the source-grounded plan document, not completion of its execution phases.
- `verified` — documentation ownership manifest and standalone validation
  - [documentation-manifest.json](documentation-manifest.json) classifies every maintained Markdown entry by stable ID, audience, Diataxis kind, visibility, translation scope, owner, lifecycle state, destination, and engine-owned source paths.
  - `BuildTools/docs_validate.py`, its focused unit tests, and the `Validate documentation` GitHub Actions job reject unclassified pages, missing sources, broken local links/anchors, and links that resolve outside the engine root.
  - `AndroidDebugging.md`, `WebDebugging.md`, `MapperTools.md`, `Debugging.md`, `ClientUpdater.md`, `Nullability.md`, and test-target guidance were separated from Last Frontier-only paths, tasks, configs, scripts, and binary names on 2026-07-10.
- `verified` — generated source inventory and documentation decisions
  - `BuildTools/docs_inventory.py` deterministically records exported script methods, native test files, and fixed/variable settings in `generated/source-inventory.json`; unit tests and CI reject drift without duplicating current counts in prose.
  - ADR 0001 records the GitHub Pages/Jekyll/Markdown/`fonline.ru` publication route and future `Docs/en` / `Docs/ru` mirrors. ADR 0002 records public API stability and breaking-change policy.
- `verified` - canonical native-codegen API model
  - [generated/api.json](generated/api.json) is emitted from the existing typed `codegen.py` parse result rather than a second C++ signature parser. It carries family/symbol IDs, overload identity, runtime sides, signatures, defaults, nullability, mutability, command-line redaction-policy state, descriptions, stability, and source provenance.
  - `BuildTools/tests/test_docs_api.py`, `BuildTools/docs_api.py --check`, manifest freshness validation, and the fast documentation CI job reject parser/model drift.
  - The native model defaults unclassified symbols to `internal` and explicitly excludes remote calls, CMake/CLI/package/helper-CLI surfaces, and native-extension ABI details. Each excluded surface now has a separate owning model or project-baked catalog rather than being folded into native symbols.
- `verified` - generated native-codegen Markdown reference
  - [generated/api/index.md](generated/api/index.md) routes to seven deterministic GitHub Pages-compatible pages for all 2,467 current model symbols: methods, properties, events, types and members, settings, and migration rules.
  - `BuildTools/docs_reference.py` reads only `generated/api.json`, emits stable symbol anchors and source links, and preserves missing descriptions as a visible metadata backlog rather than inventing prose.
  - Focused unit tests, manifest classification, byte-for-byte standalone validation, and the fast documentation CI job reject missing or manually edited generated pages.
- `verified` - source-owned API contract metadata
  - `BuildTools/codegen.py` now parses docs-only `///@ ApiContract` declarations with exact/family selectors, ADR stability labels, lifecycle fields, repeatable examples, notes, and source provenance without adding them to the runtime compatibility hash.
  - API schema v2 rejects unknown/overlapping selectors, missing deprecated replacements, self-replacements, invalid local examples, and incomplete lifecycle fields; focused tests cover overload families and hash invariance.
  - [generated/api/index.md](generated/api/index.md) reports 1 explicit contract declaration/symbol and 2,466 default-internal symbols. The explicit entry confirms `Game.BreakIntoDebugger` as internal; no stable API promise was guessed.
- `verified` - multi-domain generated contract diff and disposition gate
  - `BuildTools/docs_contract_diff.py` compares native API, CMake, the main BuildTools CLI, package, helper CLI, native-extension, prototype-format, map-format, model-format, text-format, effect-format, image-format, particle-format, and font-format models by stable ID in one deterministic JSON/Markdown report. `BuildTools/docs_api_diff.py` remains the native symbol/overload layer.
  - Baseline `stable`, `experimental`, and `deprecated` breaks plus model-source/scope/contract changes require exact schema-v2 `Docs/contract-change-dispositions.json` entries bound to the affected domain and both domain contract digests. Internal CLI/package/helper-CLI entry changes remain visible without becoming compatibility promises.
  - Focused native and aggregate tests plus standalone manifest tests cover native/CMake/native-extension/prototype-format/map-format/model-format/text-format/effect-format/image-format/particle-format/font-format removals, nested documentation, policy changes, internal-vs-experimental enforcement, stability withdrawal, stale dispositions, model drift, partial bootstrap, report artifacts, and missing enforcement. GitHub Actions compares the complete PR/push range and preserves the fourteen-domain report on failure.
- `verified` - project remote-call reference and generated catalog
  - [RemoteCalls.md](RemoteCalls.md) owns the declaration, direction, handler, namespace, serialization, authority, compatibility, and validation contract without importing game-specific calls into engine docs.
  - `BuildTools/docs_metadata.py` strictly decodes paired server/client `.fometa` outputs from the owning `MetadataBaker`, rejects malformed or mismatched records, and emits stable project-owned JSON/Markdown IDs without reparsing `.fos`.
  - Four focused tests cover binary framing, pairing, nullability, malformed input, determinism, and write/check behavior. The engine-owned starter proves both side handlers and the decoder against actual baked metadata in `win64-starter-smoke`.
- `verified` - generated CMake project-interface reference
  - `BuildTools/cmake/ProjectInterface.json` is read by configure and the documentation generator, so public option declarations, stage order/entrypoints/hooks, and selected helper names have one versioned source of truth.
  - [generated/cmake/index.md](generated/cmake/index.md) routes to checked option, stage/hook, and helper pages backed by the canonical [generated/cmake.json](generated/cmake.json) model and stable `cmake.*` IDs.
  - Five focused Python tests, a structural CMake script, manifest freshness validation, and the fast documentation job reject schema, runtime-command, source-path, escaping, and committed-output drift.
- `verified` - generated BuildTools CLI reference
  - `BuildTools/docs_cli.py` imports the executable `BuildTools/buildtools.py::create_parser()` and emits [generated/cli.json](generated/cli.json) plus checked index/command pages with stable `cli.buildtools.*` IDs, exact help output, defaults, choices, and cardinality.
  - Four focused tests compare generated help with the executable, cover parser boundaries/determinism/escaping/write-check behavior, and the standalone validator plus fast documentation job reject stale CLI artifacts.
  - The 12-command surface is explicitly `internal` until a versioned support policy is approved; package and helper CLI contracts use their own completed models, and all participate in multi-domain diff enforcement.
- `verified` - generated helper CLI reference
  - `BuildTools/HelperCliInterface.json` owns stable helper identity, purpose, audience, invocation owner, and explicit exclusions; executable `create_parser()` factories remain authoritative for syntax.
  - `BuildTools/docs_helper_cli.py` emits [generated/helper-cli.json](generated/helper-cli.json) plus checked index/command pages for 7 helpers, 11 commands, and 53 arguments with stable `helper-cli.*` IDs and exact help output.
  - Four focused tests execute every real helper/subcommand help path, enforce complete AST parser inventory, determinism, escaping, and stale detection. Manifest validation, CI freshness checks, and the fifth aggregate diff domain reject drift without promoting this `internal` surface to stable.
- `verified` - native-extension interface and executable conformance
  - `BuildTools/NativeExtensionInterface.json` is consumed by CMake/codegen and owns the five source roles, eight optional engine hooks with generated fallbacks, and six binding rules under stable `native-extension.*` IDs.
  - [NativeExtensions.md](NativeExtensions.md) documents source composition, roles, exports, hook lifecycle, instance-owned state, dependencies/platform guards, revision compatibility, and validation. [generated/native-extension/index.md](generated/native-extension/index.md) routes to deterministic role, hook, and binding pages plus the canonical JSON model.
  - Unknown CMake roles now fail at configure time; the structural test proves routing/header behavior and role parity with the project-interface manifest. Five focused generator tests, standalone validation, CI freshness checks, and the sixth aggregate domain reject manifest/runtime/reference drift under an explicit `experimental`, revision-pinned policy.
  - The engine-owned starter compiles a `SERVER` extension, suppresses one hook fallback, generates `Game.NativeStarterValue()`, calls it from baked AngelScript, and requires `starter_native_extension_value=42` at runtime.
- `verified` - prototype-format guide and generated reference
  - [PrototypeFormat.md](PrototypeFormat.md) owns reusable file selection, section resolution, identity, inheritance, property applicability, strict values, references, migrations, project boundaries, and authoring practices without depending on Last Frontier.
  - `BuildTools/PrototypeFormatInterface.json` and `BuildTools/docs_prototype_format.py` validate live parser/baker anchors, derive 4 built-in `HasProtos` types and 113 properties from current metadata, and emit the canonical model plus syntax/property/validation pages with stable `prototype-format.*` IDs.
  - Five focused tests, manifest freshness validation, documentation CI, and the seventh aggregate contract domain reject grammar/source/property drift. Grammar/rules are `experimental`; the revision-derived property inventory stays `internal`.
  - Last Frontier now routes engine grammar to this guide while retaining project extensions, custom metadata, migration CI, field semantics, and gameplay validation in its own `Docs/Prototypes.md`.
- `verified` - map-format guide and generated reference
  - [MapFormat.md](MapFormat.md) owns strict `[ProtoMap]`/`[Critter]`/`[Item]` syntax, map and placement identity, ownership references, property overrides, mapper normalization, side-specific baking, bounds, static/dynamic behavior, and runtime materialization without importing Last Frontier catalogs or composition policy.
  - `BuildTools/MapFormatInterface.json` and `BuildTools/docs_map_format.py` validate live loader/baker/mapper/runtime anchors, derive 108 current Map/Critter/Item properties and all 4 `ItemOwnership` values from metadata, and emit the canonical model plus five reference pages with stable `map-format.*` IDs.
  - Focused tests, manifest freshness validation, documentation CI, and the eighth aggregate domain reject source/model/reference drift. Grammar/ownership/rules are `experimental`; the revision-derived property inventory stays `internal`.
  - Last Frontier routes reusable format mechanics to the engine guide while retaining its piece catalog, visual grammar, AI map-authoring tools, project metadata, and semantic validation in `Docs/MapAuthoring.md`.
- `verified` - model-format and 3D composition guide with generated reference
  - [ModelFormat.md](ModelFormat.md) owns current `.fo3d` lexical syntax, include templates, parser state, FBX/OBJ inputs, compile-time limits, layers, root modifiers, child models, particles, transforms, textures/effects, disables, cuts, rendering flags, runtime composition, failures, practices, and project boundaries.
  - `BuildTools/ModelFormatInterface.json` and `BuildTools/docs_model_format.py` compare 60 accepted parser spellings grouped into 33 stable token entries directly with `ModelDescriptionParser::ParseToken`, validate live baker/runtime/test anchors and `FO_MODEL_*` options, and emit the canonical model plus seven reference pages.
  - Seven focused tests, manifest freshness validation, documentation CI, and the ninth aggregate contract domain reject parser/manifest/source/reference drift. The guide explicitly rejects removed legacy tokens and `.x` / `.3ds` inputs instead of copying historical project examples.
  - Last Frontier retains concrete model assets, layer meanings, animation enums, art policy, gameplay timing, and visible project scenes while routing reusable format mechanics to the Engine guide.
- `verified` - text and localization guide with generated reference
  - [TextAndLocalization.md](TextAndLocalization.md) owns raw `.fotxt` syntax, structured keys, multiline values, comments, variants, ordered language normalization, prototype `$Text`, script lookup, language switching, renderer color tags, diagnostics, authoring practices, and the project-formatting boundary.
  - `BuildTools/TextFormatInterface.json` and `BuildTools/docs_text_format.py` validate live parser/baker/runtime/settings/renderer anchors, derive Engine language defaults and all five prototype output packs, and emit the canonical model plus six reference pages with 38 stable `text-format.*` entries.
  - Seven focused tests, manifest freshness validation, documentation CI, and the tenth aggregate contract domain reject source/model/reference drift. The reusable contract is `experimental`; project pack catalogs, language priorities, translation workflow, and lexem formatters remain project-owned.
  - Last Frontier routes parser, baking, and runtime mechanics to the Engine guide while retaining Russian-first policy, concrete packs, translation guards, semantic key conventions, `TextFormatting.fos`, and GUI refresh behavior in `Docs/Localization.md`.
- `verified` - effect-format guide with generated reference
  - [EffectFormat.md](EffectFormat.md) owns `.fofx` sections, shader-stage fallback, passes and render state, vertex layouts, built-in resources, backend artifacts, runtime cache identity, script values, diagnostics, authoring practices, and the embedding-project boundary.
  - `BuildTools/EffectFormatInterface.json` and `BuildTools/docs_effect_format.py` validate live baker, renderer, runtime, script-method, settings, and focused-test anchors; derive current effect limits; and emit the canonical model plus seven reference pages with stable `effect-format.*` entries.
  - Seven focused tests, manifest freshness validation, documentation CI, and the eleventh aggregate contract domain reject source/model/reference drift. The reusable contract is `experimental`; project shader catalogs, visual policy, effect-slot selection, and `ScriptValueBuf` meanings remain project-owned.
  - Last Frontier routes reusable mechanics to the Engine guide while retaining `Resources/Visual/Effects/`, minimal/advanced profile policy, slot-specific tuning, and visible project validation in `Docs/Scripts.md` and `Docs/ContentWorkflow.md`.
- `verified` - image-format and FOFRM guide with generated reference
  - [ImageFormat.md](ImageFormat.md) owns all 12 built-in image source extensions, source selection, FOFRM fields/aliases/directions/flattening/timing, ART/SPR/BAM selectors, pixel/alpha constraints, the private baked container, stock runtime factory coverage, sprite sheets, atlases, caches, diagnostics, authoring practices, and the embedding-project boundary.
  - `BuildTools/ImageFormatInterface.json` and `BuildTools/docs_image_format.py` validate live baker/client/test anchors, derive the 12 baker and 11 default-runtime extensions, prove the direct-SPR and ignored-Effect boundaries, and emit the canonical model plus seven reference pages with 49 stable `image-format.*` entries.
  - Seven focused tests, manifest freshness validation, documentation CI, and the twelfth aggregate contract domain reject source/model/reference drift. The reusable contract is `experimental`; private byte-record entries are `internal`, the parsed-but-unused Effect field is `deprecated`, and project assets/licensing/pack policy/visual acceptance remain project-owned.
  - Last Frontier routes reusable mechanics to the Engine guide while retaining its PNG/TGA/FOFRM catalog, source/provenance policy, resource-pack precedence, substitutions, movement tuning, hit-test expectations, and visible client baselines in `Docs/ContentWorkflow.md`.
- `verified` - particle-format, SPARK tooling, and runtime guide with generated reference
  - [ParticleFormat.md](ParticleFormat.md) owns `.fopts` raw-copy delivery, SPARK XML graph rules, the exact registered object/editor surface, `SparkQuadRenderer`, texture/effect resolution, editor save normalization, base-graph caching and cloning, atlas/direct-scene rendering, script/model integration, diagnostics, authoring practices, and embedding-project validation.
  - `BuildTools/ParticleFormatInterface.json` and `BuildTools/docs_particle_format.py` derive the live `fopts` extension, 36 SPARK core registrations plus the Engine renderer, all 37 editor handlers, descriptor attributes, 12 renderer fields, Engine defaults, and source behavior into a canonical model plus eight reference pages with 96 stable `particle-format.*` entries.
  - Eight focused tests, manifest freshness validation, documentation CI, and the thirteenth aggregate contract domain reject registry/editor/source/model/reference drift. The reusable surface is `experimental`; derived implementation coverage stays `internal`, and project catalogs, effects, textures, settings overrides, budgets, art direction, and visual acceptance remain project-owned.
  - Last Frontier routes reusable mechanics to the Engine guide while retaining its seven particle systems, six textures, concrete effects, scene/shot/grenade integrations, render overrides, and visible project gates in `Docs/ContentWorkflow.md`.
- `verified` - bitmap-font format, binding, layout, and rendering guide with generated reference
  - [FontFormat.md](FontFormat.md) owns FOFNT and AngelCode BMFont descriptors, raw-copy and referenced-image delivery, slot binding, bind-time downscale, metrics, wrapping/alignment, draw flags, inline colors, atlas/cache behavior, diagnostics, authoring practices, and the embedding-project boundary.
  - `BuildTools/FontFormatInterface.json` and `BuildTools/docs_font_format.py` derive the live extensions, FOFNT keys/version, BMF v3 block and signed-metric contract, font/flag/layout enums, scale guard, atlas/cache behavior, and bundled descriptor evidence into a canonical model plus eight reference pages with 57 stable `font-format.*` entries.
  - Nine focused tests, manifest freshness validation, documentation CI, and the fourteenth aggregate contract domain reject descriptor/source/model/reference drift. The reusable surface is `experimental`; cache internals stay `internal`, and project slot assignment, glyph coverage, typography, localization acceptance, and UI baselines remain project-owned.
  - Source review fixed BMFont negative `xoffset`, `yoffset`, and `xadvance` decoding to use signed 16-bit reads and rejects an empty FOFNT image name before path access. Last Frontier routes reusable mechanics to the Engine guide while retaining concrete fonts, Russian glyph coverage, GUI slot policy, and embedded-client measurement/visual gates.
- `verified` - model animation metadata and duration guide
  - [ModelAnimation.md](ModelAnimation.md) owns `.fo3d` animation tuples, positive authored speed, effective cycle calculation, one-step state/action aliases, `ModelAnimationInfo.foinfo`, common and client-instance lookup boundaries, failures, authoring practices, and project ownership without depending on Last Frontier or TLA.
  - Five focused source tests pin the guide to `ModelInfoBaker`, client alias selection, common metadata registration, both script exports, and native model/common-script regressions. The standalone validator requires the focused test in documentation CI.
  - [ModelFormat.md](ModelFormat.md) now owns the surrounding `.fo3d` grammar and runtime composition; Last Frontier retains concrete model assets, resource packs, enum selection, gameplay timing, fallback behavior, and project tests.
- `verified` - 2D sprite root motion and walk-cycle guide
  - [SpriteRootMotion.md](SpriteRootMotion.md) owns `NextX` / `NextY` import and baked transport, direction-specific `SpriteSheet` offsets, authoritative-movement separation, anchor-relative phase, frame selection, rendered alignment, sheet changes, lifecycle, fallbacks, and project validation.
  - Five focused source tests pin the guide to current `ImageBaker`, `DefaultSpriteFactory`, `MovingContext`, `CritterHexView`, image-baker regressions, manifest routing, and CI. The guide explicitly records the missing focused native `CritterHexView` fixture and requires a visible client scene for semantic gait validation.
  - The reusable material discovered in TLA `Docs/Animation.md` was re-derived against current Engine source. TLA remains integration evidence, not a normative dependency; its 2D sprite contract is kept separate from `.fo3d` skeletal animation.
- `verified` - script lifecycle and concurrency guide
  - [ScriptLifecycleAndConcurrency.md](ScriptLifecycleAndConcurrency.md) now owns module-init ordering and global freeze, callback-only attributes and entity lifetime, transitive `[[Async]]`, client/server `Yield` resumption, server entity covers, `Game.Lock`, mutable-state ownership, and shutdown boundaries independently of an embedding project.
  - [Scripting.md](Scripting.md), human/AI indexes, test routing, and the translation manifest route readers to the focused guide instead of mixing project module topology with reusable runtime semantics.
  - `BuildTools/tests/test_docs_script_lifecycle.py` pins the guide's central claims to current `ScriptSystem`, AngelScript attributes/context, client scheduling, server synchronization, and entity teardown markers; the standalone validator requires this test in documentation CI.
- `verified` - generated package declaration and payload reference
  - `BuildTools/PackageInterface.json` is consumed by `package.py` and `BuildTools/docs_package.py`, producing [generated/package.json](generated/package.json) plus five checked pages for `DefinePackage`, targets/platforms/packs, payloads/artifacts, and the internal packager CLI.
  - The model exposes stable `package.*` IDs for 5 targets, 6 platforms, 19 packs, 6 payloads, and 13 CLI arguments while marking macOS/iOS unsupported and AppImage placeholder instead of implying support.
  - Five focused Python tests, a structural CMake test of the real declaration macro, runtime combination validation, manifest freshness checks, and documentation CI reject schema/parser/CMake/output drift.
- `verified` — GitHub Pages-compatible preview contract
  - [SitePublication.md](SitePublication.md) records the Markdown/Jekyll/custom-domain contract, exact local runtime pins, preview commands, DNS verification, CI artifact review, and the boundary between repository data and private domain credentials.
  - The `Build documentation site` job uses the official Pages Jekyll build action and uploads commit-addressable `_site` output for 14 days without changing the existing production deployment route.
  - Manifest validation now rejects drift between `CNAME`, `_config.yml`, `.ruby-version`, `Gemfile`, the Pages source-verification state, and the build/upload workflow. The first CI render and administrator confirmation of the production source branch/folder remain pending.
- `verified` — canonical minimal project, Windows and Linux paths
  - [../Examples/MinimalProject/README.md](../Examples/MinimalProject/README.md) now owns a complete headless scaffold with CMake composition, explicit minimal configuration, AngelScript lifecycle, a server native hook/export, deterministic smoke sub-config, and a timeout/marker runner.
  - `python BuildTools/buildtools.py validate win64-starter-smoke` passed on 2026-07-10 after recreating the disposable project copy, baking scripts, reaching both lifecycle markers, and shutting down cleanly.
  - Private template run `29739863448` passed the exact-pin primary smoke on clean `windows-latest` and `ubuntu-latest` runners on 2026-07-20; manual current-Engine run `29740066760` passed the same Linux route after checking out Engine `master`. Linux host dependencies come from the checked-out Engine's versioned preparation command.
- `verified` — first tested tutorial lesson
  - [../TUTORIAL.md](../TUTORIAL.md) is no longer a placeholder. It documents the tested headless milestone, expected output and artifacts, a rebake exercise, deliberate starter limits, and recovery from the integration failures found while building it.

## Next recommended slice

- `verified` - manifest-backed AI documentation delivery
  - `BuildTools/docs_ai_delivery.py` generates root `llms.txt`, 1.25 MiB-bounded `llms-full.txt`, and public `docs-manifest.json` from the same source manifest and Markdown used by the human site. The reviewed increase from 1 MiB preserves complete documents after adding the image-format contract; fail-closed validation and the no-truncation rule remain unchanged.
  - Focused tests and standalone validation enforce public/current filtering, generated-index-only context policy, canonical/source/raw URLs, normalized SHA-256 hashes, deterministic output, workflow wiring, and the byte budget. ADR 0003 records the ownership and non-normative delivery contract.
- `verified` - manifest-backed documentation site navigation and search
  - `BuildTools/docs_site.py` generates `_data/docs-site.json` for Jekyll navigation and `assets/docs-search.json` for compact browser-local search from the same stable document IDs and Markdown corpus.
  - Navigation covers every public current human top-level page exactly once while generated detail pages remain searchable behind their indexes. The search artifact remains under a hard 1 MiB budget.
  - The custom default layout provides responsive desktop/mobile navigation, rolling `master` identity, page-local table of contents, source links, code-copy controls, and persisted light/dark preference without a remote application or asset dependency.
  - Focused generator and layout/static tests, standalone freshness validation, and CI wiring reject missing/duplicate navigation IDs, stale/oversized index output, missing local assets, and rendering-contract drift. ADR 0004 owns the boundary.

- `verified` - documentation version, locale, and stable route policy
  - [Decisions/0006-documentation-version-locale-routing.md](Decisions/0006-documentation-version-locale-routing.md) defines rolling `master` as the unversioned `current` channel, defers tagged snapshots until supported release lines exist, and records the bilingual route and durable Markdown redirect policy.
  - `BuildTools/docs_site.py` now emits [generated/document-routes.json](generated/document-routes.json) with every current public URL, canonical future owner, planned English/Russian path, availability state, and required legacy redirect.
  - Focused site, AI-delivery, layout, and standalone-validator tests reject version/source-ref drift, malformed locale pairs, ambiguous canonical targets, route collisions, invalid legacy pointer records, and stale generated route data.
  - The current English files have not moved and no Russian page is presented as complete. Physical locale migration, language switching, translation hashes/parity, and reviewed translations remain Phase 9 work.

The initial documentation backlog plus the native-extension, script lifecycle/concurrency, prototype-format, map-format, model-format, text-format, effect-format, image-format, particle-format, font-format, AI-delivery, site-navigation/search, version/locale/route, and public-example governance contracts are complete locally. The starter's pinned Windows/Linux and current-Engine clean-runner lanes are green. Finish the remaining production platform work by confirming the documentation-site and fourteen-domain contract-diff artifacts and recording the actual Pages source branch/folder. The reviewed `fonline-project-template` candidate is source-staged in a private repository; branch/security settings, first tag/artifact, and the explicit visibility transition still gate publication. GUI/dialog ownership, audio/video references, physical locale migration, clean published Markdown endpoints, broad non-internal stability review, visual teaching media, and English/Russian parity remain planned work.

## Verified public example repository governance (2026-07-15)

- [PublicExampleRepositories.md](PublicExampleRepositories.md) and ADR 0005 now own the four-repository portfolio, authority split, exact Engine pins, pinned/current compatibility lanes, release and tutorial tags, support/security boundaries, and asset provenance.
- `Examples/PublicRepositories.json` records four ordered repositories, one source-ready template, three planned follow-ons, owners, dependencies, checks, artifacts, asset policy, exit gates, and orthogonal remote visibility/staging state. The generated JSON/Markdown projection is consumed by site search and AI delivery and emits no URL for a private repository.
- `Examples/PublicRepositoryTemplate` supplies the checked CODEOWNERS, pull-request, security, support, contribution, license, third-party, pinned/current workflow, metadata, README, and provenance templates used at publication.
- `BuildTools/docs_examples.py` rejects floating release refs, incomplete portfolio ownership, template drift, unresolved publication placeholders, non-exact revisions, gitlink/checkout mismatch, missing governance files, and incomplete asset provenance. Focused tests and standalone validation enforce deterministic output and CI wiring.
- The incoming updater protocol generation 2 and client host/runtime ABI 3 boundary is reflected in example release policy: incompatible frozen native hosts require a new full client package and one manual reinstall, never an in-process reload workaround.
- All four repositories were created privately on 2026-07-20. The template candidate is pushed as `source-staged`, with green pinned Windows/Linux and current-Engine workflows; the other three repositories are reserved with honest staging READMEs. Branch/security settings, release tags/artifacts, and public visibility remain administrator-authorized work, and no private repository is exposed as an already published source.
