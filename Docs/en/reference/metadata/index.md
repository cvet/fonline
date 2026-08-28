---
layout: default
title: Generated API and Metadata
locale: en
document_id: generated-api-metadata
permalink: /Docs/en/reference/metadata/
---

# Generated API and Metadata

This document explains the engine code-generation and metadata-registration flow. Use it when changing generated source, metadata annotations, property definitions, or script-visible API contracts.

For the task-oriented regeneration order across configure-time codegen, scripts, baked resources, metadata, documentation, site, and AI artifacts, use [Generated Content Workflow](../../how-to/build/generated-content.md).

## Ownership model

The engine owns the reusable metadata/codegen machinery. An embedding project supplies project configuration, extra metadata sources, common headers, and script/content inputs through CMake options and project files.

Generated files are build artifacts. Document the source annotations, templates, generator inputs, and validation flow; do not treat generated output as hand-authored engine source.

## Source paths inspected

- `BuildTools/cmake/stages/Codegen.cmake`
- `BuildTools/cmake/stages/EngineSources.cmake`
- `BuildTools/cmake/helpers/Build.cmake`
- `BuildTools/cmake/helpers/State.cmake`
- `BuildTools/buildtools.py`
- `BuildTools/codegen.py`
- `BuildTools/docs_api.py`
- `BuildTools/docs_api_diff.py`
- `BuildTools/docs_contract_diff.py`
- `BuildTools/docs_cli.py`
- `BuildTools/HelperCliInterface.json`
- `BuildTools/docs_helper_cli.py`
- `BuildTools/NativeExtensionInterface.json`
- `BuildTools/docs_native_extension.py`
- `BuildTools/PackageInterface.json`
- `BuildTools/package.py`
- `BuildTools/docs_package.py`
- `BuildTools/docs_examples.py`
- `BuildTools/tests/test_docs_examples.py`
- `BuildTools/docs_cmake.py`
- `BuildTools/docs_reference.py`
- `BuildTools/ModelFormatInterface.json`
- `BuildTools/docs_model_format.py`
- `BuildTools/TextFormatInterface.json`
- `BuildTools/docs_text_format.py`
- `BuildTools/EffectFormatInterface.json`
- `BuildTools/docs_effect_format.py`
- `BuildTools/ImageFormatInterface.json`
- `BuildTools/docs_image_format.py`
- `BuildTools/ParticleFormatInterface.json`
- `BuildTools/docs_particle_format.py`
- `BuildTools/FontFormatInterface.json`
- `BuildTools/docs_font_format.py`
- `BuildTools/AudioInterface.json`
- `BuildTools/docs_audio.py`
- `BuildTools/tests/test_docs_audio.py`
- `BuildTools/VideoInterface.json`
- `BuildTools/docs_video.py`
- `BuildTools/tests/test_docs_video.py`
- `BuildTools/GuiRuntimeInterface.json`
- `BuildTools/docs_gui_runtime.py`
- `BuildTools/tests/test_docs_gui_runtime.py`
- `BuildTools/AiControlProtocol.json`
- `BuildTools/docs_ai_control_protocol.py`
- `BuildTools/tests/test_docs_ai_control_protocol.py`
- `BuildTools/docs_metadata.py`
- `BuildTools/docs_public_api.py`
- `BuildTools/tests/test_docs_public_api.py`
- `BuildTools/docs_ai_delivery.py`
- `BuildTools/tests/test_docs_ai_delivery.py`
- `BuildTools/docs_ai_eval.py`
- `BuildTools/tests/test_docs_ai_eval.py`
- `BuildTools/docs_snippets.py`
- `BuildTools/tests/test_docs_snippets.py`
- `BuildTools/SupportMatrix.json`
- `BuildTools/docs_support_matrix.py`
- `BuildTools/tests/test_docs_support_matrix.py`
- `BuildTools/docs_localization.py`
- `BuildTools/tests/test_docs_localization.py`
- `BuildTools/ExternalProjectEvidence.json`
- `BuildTools/docs_external_evidence.py`
- `BuildTools/tests/test_docs_external_evidence.py`
- `BuildTools/DocumentationDiagrams.json`
- `BuildTools/docs_diagrams.py`
- `BuildTools/tests/test_docs_diagrams.py`
- `BuildTools/DocumentationScreenshots.json`
- `BuildTools/docs_screenshots.py`
- `BuildTools/tests/test_docs_screenshots.py`
- `BuildTools/docs_site.py`
- `BuildTools/docs_site_artifact.py`
- `BuildTools/tests/test_docs_site.py`
- `BuildTools/tests/test_docs_site_layout.py`
- `BuildTools/tests/test_docs_site_artifact.py`
- `BuildTools/tests/test_docs_browser.py`
- `BuildTools/tests/test_docs_api.py`
- `BuildTools/tests/test_docs_api_diff.py`
- `BuildTools/tests/test_docs_contract_diff.py`
- `BuildTools/tests/test_docs_cli.py`
- `BuildTools/tests/test_docs_helper_cli.py`
- `BuildTools/tests/test_docs_native_extension.py`
- `BuildTools/tests/validate_native_extension_interface.cmake`
- `BuildTools/tests/test_docs_package.py`
- `BuildTools/tests/validate_package_interface.cmake`
- `BuildTools/tests/test_docs_cmake.py`
- `BuildTools/tests/test_docs_reference.py`
- `BuildTools/tests/test_docs_model_format.py`
- `BuildTools/tests/test_docs_text_format.py`
- `BuildTools/tests/test_docs_effect_format.py`
- `BuildTools/tests/test_docs_image_format.py`
- `BuildTools/tests/test_docs_particle_format.py`
- `BuildTools/tests/test_docs_font_format.py`
- `BuildTools/tests/test_docs_metadata.py`
- `Docs/generated/api.json`
- `Docs/en/reference/script-api/*.md`
- `Docs/ru/reference/script-api/*.md`
- `Docs/generated/api/*.md` (legacy routes)
- `BuildTools/cmake/ProjectInterface.json`
- `BuildTools/tests/validate_project_interface.cmake`
- `Docs/generated/cmake.json`
- `Docs/en/reference/cmake/*.md`
- `Docs/ru/reference/cmake/*.md`
- `Docs/generated/cmake/*.md` (legacy routes)
- `Docs/generated/cli.json`
- `Docs/en/reference/buildtools/*.md`
- `Docs/ru/reference/buildtools/*.md`
- `Docs/generated/cli/*.md` (legacy routes)
- `Docs/generated/helper-cli.json`
- `Docs/en/reference/helper-cli/*.md`
- `Docs/ru/reference/helper-cli/*.md`
- `Docs/generated/helper-cli/*.md` (legacy routes)
- `Docs/generated/native-extension.json`
- `Docs/generated/native-extension/*.md`
- `Docs/generated/prototype-format.json`
- `Docs/generated/map-format.json`
- `Docs/generated/package.json`
- `Docs/generated/package/*.md`
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
- `Docs/generated/audio/*.md` (legacy routes)
- `Docs/generated/video.json`
- `Docs/en/reference/video/*.md`
- `Docs/generated/video/*.md` (legacy routes)
- `Docs/generated/gui-runtime.json`
- `Docs/en/reference/gui-runtime/*.md`
- `Docs/generated/gui-runtime/*.md` (legacy routes)
- `Docs/generated/ai-control-protocol.json`
- `Docs/en/reference/ai-control-protocol/*.md`
- `Examples/PublicRepositories.json`
- `Examples/PublicRepositoryTemplate/`
- `Docs/generated/public-examples.json`
- `Docs/generated/public-examples/*.md`
- `Docs/en/reference/public-contract/index.md`
- `Docs/ru/reference/public-contract/index.md`
- `PUBLIC_API.md` (legacy route)
- `Docs/contract-change-dispositions.json`
- `llms.txt`
- `llms-full.txt`
- `docs-manifest.json`
- `Docs/ai-evaluation.json`
- `Docs/generated/ai-evaluation-report.json`
- `BuildTools/SnippetPolicy.json`
- `Docs/generated/snippets.json`
- `Docs/generated/support-matrix.json`
- `Docs/en/reference/platforms/generated-matrix.md`
- `Docs/generated/translation-status.json`
- `Docs/generated/external-project-evidence.json`
- `Docs/generated/external-project-evidence/index.md`
- `Docs/generated/diagrams.json`
- `Docs/assets/diagrams/*.svg`
- `Docs/generated/screenshots.json`
- `Docs/assets/screenshots/*.png`
- `_data/docs-site.json`
- `assets/docs-search.json`
- `assets/docs-search.ru.json`
- `Docs/generated/document-routes.json`
- `Source/Common/MetadataRegistration.h`
- `Source/Common/MetadataRegistration.cpp`
- `Source/Common/MetadataRegistration.template.cpp`
- `Source/Common/GenericCode.template.cpp`
- `Source/Common/Properties.h`
- `Source/Common/Properties.cpp`
- `Source/Common/Entity.h`
- `Source/Common/Entity.cpp`
- `Source/Tools/MetadataBaker.h`
- `Source/Tools/MetadataBaker.cpp`
- `Source/Tests/Test_EngineMetadata.cpp`
- `Source/Tests/Test_MetadataBaker.cpp`
- `Source/Tests/Test_Properties.cpp`
- `PUBLIC_API.md`

## CMake codegen stage

`BuildTools/cmake/stages/Codegen.cmake` constructs the generator command and output list.

Important command arguments include:

- `-maincfg` — embedding project's main config (`FO_MAIN_CONFIG`).
- `-buildhash` — current build hash.
- `-genoutput` — generated output directory, currently `GeneratedSource` under the CMake binary dir.
- `-devname` / `-nicename` — project identity values.
- `-embedded` — embedded data capacity (`FO_EMBEDDED_DATA_CAPACITY`).
- `-meta` — metadata source entries from `FO_SOURCE_META_FILES` and `FO_MONO_SOURCE`.
- `-commonheader` — extra common headers from `FO_ADDED_COMMON_HEADERS`.
- `-enginedefine` — repeatable `NAME=VALUE` engine value/shape configuration macro (`FO_GEOMETRY`, `FO_MAP_*`, `FO_EFFECT_*`, `FO_MODEL_*`, `FO_USE_NAMESPACE`, `FO_NO_*`, `FO_MAIN_CONFIG`, ...), resolved to a literal at configure time and emitted into `EngineConfig.gen.h` instead of being passed as a `-D` compiler define. Feature/backend toggles (`FO_ENABLE_3D`, `FO_*_SCRIPTING`, `FO_*_PARTICLES`) and per-config `FO_DEBUG` stay compiler-side — they gate whole files/headers before any engine header is included.

The stage creates normal and forced code-generation command targets and appends `CodeGeneration` to `FO_GEN_DEPENDENCIES`.

`InternalConfig.gen.inc` reserves a fixed Engine-owned 10000-byte patch area. The
embedding project cannot resize it; `package.py` discovers that exact capacity
from the generated binary markers before writing the bootstrap config.

## Generated outputs

`Codegen.cmake` declares generated outputs under `GeneratedSource/`, including:

- `CodeGenTouch`
- `EngineConfig.gen.h` — one macro-only header consumed at the top of `Source/Essentials/BasicCore.h`. It contains both the engine configuration macros and the build/version string macros `FO_BUILD_HASH` / `FO_DEV_NAME` / `FO_NICE_NAME` / `FO_COMPATIBILITY_VERSION` / `FO_GIT_BRANCH`. Replaces the former `Version-Include.h`.
- `EmbeddedResources.gen.inc`
- `InternalConfig.gen.inc`
- `MetadataRegistration-Server.gen.cpp`
- `MetadataRegistration-Client.gen.cpp`
- `MetadataRegistration-Mapper.gen.cpp`
- `MetadataRegistration-ServerStub.gen.cpp`
- `MetadataRegistration-ClientStub.gen.cpp`
- `MetadataRegistration-MapperStub.gen.cpp`
- `GenericCode-Common.gen.cpp`

These file names are useful for understanding build flow, but changes should usually be made in templates, annotations, metadata sources, or generator scripts rather than in generated output.

Resource bakers also produce runtime metadata outside this code-generation output set. In particular, `ModelInfoBaker` writes `ModelAnimationInfo.foinfo`, which `BaseEngine` registers for `Game.GetModelAnimDuration`; its private layout and source-owned animation semantics are documented in [Model Animation](../../how-to/content/model-animation.md).

## Canonical documentation API model

[generated/api.json](../../../generated/api.json) is the deterministic machine-readable model for the engine-owned native codegen surface. `BuildTools/docs_api.py` does not parse C++ declarations independently: it starts a read-only `BuildTools/codegen.py` metadata session, then serializes the same typed tag objects used to generate bindings and metadata registration.

The model currently covers:

- exported enums and enum values;
- value types and layout fields;
- reference types, fields, and methods;
- entities, properties, native script methods, and exported events;
- settings from `///@ ExportSettings` blocks;
- native migration rules;
- source-authored stability/lifecycle declarations from `///@ ApiContract` tags;
- the codegen parser's allowed targets, hook names, and migration-rule kinds.

Every addressable entry has a stable `family_id`, a unique `id`, kind, runtime sides, receiver where applicable, normalized script-facing signature, flags, description, source path/line, stability, since/deprecation fields, related examples, and explicit/default contract provenance. Arguments carry defaults, nullability, and by-reference state. Properties and settings carry mutability; settings also record whether their names match the default `Common.SecretSettingTokens` command-line redaction policy. The field is deliberately named `command_line_redacted_by_default`: it does not classify credentials, promise that every credential-like name is masked, or account for a customized token list.

Settings declared in one `///@ ExportSettings` block share that annotation's source line in the current model. The setting name remains the stable identity; exact per-macro lines can be added later inside the owning codegen parser without changing IDs.

The generated `summary` reports symbol counts by kind and stability, explicit contract declarations and affected symbols, default-internal coverage, description coverage, missing provenance, and contributing source-file coverage. These numbers are generated quality signals; do not copy them into manually maintained public prose.

Single declarations use their family ID directly. Overload families append a deterministic signature hash to `id`, while retaining the unhashed `family_id` for grouping. A signature change to a non-overloaded symbol therefore keeps its identity; an overloaded signature change appears as removal/addition until explicit source-authored IDs are introduced.

Unclassified entries use the ADR-defined `internal` stability label. Reachability through generated bindings does not promote a symbol to `stable` or `experimental`.

### Source-owned symbol descriptions

Reader-facing descriptions stay with the export metadata that defines each symbol:

- an ordinary adjacent comment or an inline export-tag comment describes the exported type, entity, method, event, property, or setting;
- fields and methods inside `///@ ExportRefType` blocks use their adjacent or inline member comments;
- explicit enum values use `///@ EnumValueDoc <Enum> <Value> // <description>` after their `ExportEnum` declaration when the C++ initializer itself has no suitable documentation surface;
- value-layout fields use `///@ ValueFieldDoc <Type> <Field> // <description>` after their `ExportValueType` tag. This form also works for aliases and strong types whose script layout has no matching field declaration at the alias site.
- generated `GameProperty` through `LocationProperty` enum values inherit the exact description and source location of their owning `ExportProperty`; each generated `None` value explicitly means that no property identifier is selected.
- the 23 `ImGui_*` enum wrappers resolve each inline Dear ImGui alias against the pinned `ThirdParty/imgui/imgui.h`. Values with an upstream comment inherit that prose and exact source line; `ImGui_StyleVar` aliases resolve to the semantic comment on the corresponding `ImGuiStyle` field. Zero, composite, corrected, or otherwise independently undocumented values require an explicit `EnumValueDoc` fallback.
- the 103 physical `KeyCode` values resolve to their exact `SDL_SCANCODE_*` mapping lines in `Source/Frontend/Application.cpp`; the no-key sentinel and synthetic UTF-8 `Text` event remain explicit `EnumValueDoc` records in the declaration header.

`EnumValueDoc` and `ValueFieldDoc` reject unknown types, unknown members, duplicates, and empty descriptions. The Dear ImGui resolver also rejects missing or malformed wrapper aliases and any value that has neither usable vendored prose nor an explicit fallback; the key-code resolver rejects a declared value without either an exact SDL mapping or an explicit synthetic-value description. Documentation records the exact owning tag, property, mapping, vendored value, or `ImGuiStyle` field source line and is excluded from the runtime compatibility hash. Generated property-enum documentation is attached only after the compatibility contribution has been hashed, so documentation changes cannot alter client/server compatibility. A description explains current behavior; stability comes only from the separate exact, family, or scope contract.

### API contract annotations

`///@ ApiContract` is a documentation-only tag parsed by the same `BuildTools/codegen.py` metadata session as runtime exports. It is deliberately excluded from the client/server compatibility hash. A tag selects one exact symbol `id`, every current overload sharing a `family_id`, or the complete inventory-pinned native-codegen scope:

```cpp
///@ ApiContract script.method.common.Game.BreakIntoDebugger internal
///@ ApiContract scope:native-codegen experimental Since=2022.1.0.wip SymbolCount=2492 InventorySha256=8f70fbf7882f6b81ca6141efc8e3e0240eb97b0e8a9b36fe6be530345846f4ab
///@ ApiContract script.method.common.Game.LoadData experimental Since=0.4.0 Example=Docs/Examples/LoadData.md
///@ ApiContract script.method.common.Game.OldCall deprecated DeprecatedSince=0.5.0 Replacement=script.method.common.Game.NewCall Removal=1.0.0
```

Supported labels and fields follow ADR 0002:

- `internal` accepts no lifecycle fields; it records that the default status was explicitly reviewed.
- `stable` and `experimental` require `Since=<version>`.
- `scope:native-codegen` requires a positive `SymbolCount` and lowercase 64-hex `InventorySha256` over newline-joined, lexically sorted stable IDs. Both pins must match before the scope label is applied.
- `deprecated` requires `DeprecatedSince=<version>`, a live `Replacement=<id-or-family>`, and `Removal=<target>`; optional `Since` records original introduction when known.
- `Example=<root-relative-path-or-http-url>` may be repeated. Local paths must exist and stay inside the engine root.
- A normal `///` comment immediately above the tag becomes contract notes. It does not replace the export annotation's symbol description.

Generation rejects unknown selectors, stale scope pins, multiple scope declarations, overlapping exact/family declarations, missing replacements, self-replacements, invalid examples, duplicate fields, and incomplete lifecycle metadata. An exact declaration may deliberately override the one scope declaration. Contract provenance is stored separately from declaration provenance, so readers can distinguish `experimental (scope)`, exact classifications, and an unclassified default.

The current checked classification applies revision-pinned `experimental` status to all 2,472 native-codegen symbols since `2022.1.0.wip`, with the development-only `Game.BreakIntoDebugger` exact override remaining `internal`. The scope's count and inventory digest force owner review for every future symbol-set change. Broad `stable` and lifecycle-specific `deprecated` promises remain release-policy and owner review work.

Adding or editing `ApiContract` tags must not change the runtime compatibility hash. The focused API tests compare hashes with and without valid contract metadata.

The model is intentionally scoped as `engine-native-codegen`. Project-authored remote calls are covered by the separate baked-metadata supplement below because they belong to an embedding project, not the engine snapshot. CMake options/stages/helpers, the main BuildTools CLI, package declarations/payloads, and helper-script CLIs have separate source-backed models described below. Native-extension ABI details still require their owning structured source before the full public-reference exit gate can close.

Regenerate and verify from the engine root:

```bash
python BuildTools/tests/test_docs_api.py
python BuildTools/docs_api.py --write
python BuildTools/docs_api.py --check
```

The JSON is checked in for GitHub Pages, offline use, and AI retrieval. Never edit it manually; CI and `BuildTools/docs_validate.py` reject stale output.

## Generated Markdown reference

[English native script API reference](../../reference/script-api/index.md) is the canonical human entry point rendered directly by GitHub Pages; the maintained Russian mirror starts at [Russian native script API reference](../../../ru/reference/script-api/index.md), while `Docs/generated/api/*.md` remains a durable legacy route. `BuildTools/docs_reference.py` reads only the canonical JSON model; it does not parse C++, infer missing descriptions, or maintain a parallel symbol inventory. The generated pages split the current model into:

- native script methods;
- entity properties;
- engine events;
- entities, enums, value types, and reference types with their members;
- engine settings;
- native migration rules.

Each symbol appears under a deterministic HTML anchor with its unique ID, normalized signature, runtime sides, explicit/default API contract, declaration and contract provenance, lifecycle fields, examples, flags where applicable, and source-authored description. Source links target the repository's `master` branch while the canonical model retains the generated path/line provenance. Revision-pinned links remain publication work rather than an implied guarantee in this first renderer.

The settings page reports `command_line_redacted_by_default` as a command-line masking-policy result. It does not rename that field to `sensitive` or present it as a credential classification.

Regenerate and verify the Markdown layer after regenerating the JSON model:

```bash
python BuildTools/tests/test_docs_reference.py
python BuildTools/docs_reference.py --write
python BuildTools/docs_reference.py --check
```

The page set and byte-for-byte output are declared in [documentation-manifest.json](../../../documentation-manifest.json). The standalone validator and documentation CI reject missing, manually edited, or stale pages.

## Generated CMake project interface

[English CMake reference](../../reference/cmake/index.md) is the human entry point for the CMake surface intended for embedding projects; the maintained translation starts at the [Russian CMake reference](../../../ru/reference/cmake/index.md). `BuildTools/cmake/ProjectInterface.json` is the documentation model, while `BuildTools/Init.cmake` and the stage/helper files remain configure-time authority. The structural CMake test compares option names, stage order, entrypoints, hooks, helper declarations, and source paths so generated documentation cannot silently drift from the implementation.

`BuildTools/docs_cmake.py` validates the manifest independently and emits [generated/cmake.json](../../../generated/cmake.json), canonical English pages for options, stages/hooks, and selected helpers, and durable pointers at the former `Docs/generated/cmake/*.md` routes. Every record has a stable `cmake.option.*`, `cmake.stage.*`, or `cmake.helper.*` ID. The Markdown output includes defaults, required state, override precedence, signatures, allowed roles, responsibilities, and source links without reparsing CMake syntax. The Russian pages are reviewed translations with source-hash and code-fence parity gates; they are not overwritten by the English generator.

The scope is currently `experimental`: it records the exact revision-pinned interface but does not declare a versioned support line. Commands under `BuildTools/cmake/helpers/` are not public merely because they are reachable; only helpers listed in the manifest are in this documented surface. Package declaration grammar and BuildTools command-line interfaces are separate domains, and the aggregate contract gate compares each model under its own stability policy.

Regenerate and verify from the engine root:

```bash
python BuildTools/tests/test_docs_cmake.py
cmake -P BuildTools/tests/validate_project_interface.cmake
python BuildTools/docs_cmake.py --write
python BuildTools/docs_cmake.py --check
```

The structural CMake test verifies the runtime-loaded stage/entrypoint/hook/helper shape. Focused Python tests and `BuildTools/docs_validate.py` verify schema rules, deterministic IDs, source paths, escaped Markdown, and byte-for-byte freshness.

## Generated BuildTools CLI reference

[English BuildTools CLI reference](../../reference/buildtools/index.md) is the human entry point for the main BuildTools command line; the maintained translation starts at the [Russian BuildTools CLI reference](../../../ru/reference/buildtools/index.md). `BuildTools/docs_cli.py` imports `BuildTools/buildtools.py`, invokes the same `create_parser()` factory used by the executable, and emits [generated/cli.json](../../../generated/cli.json), canonical English command pages, and durable pointers at the former `Docs/generated/cli/*.md` routes. No second command list or Python-source parser is maintained.

Every command and argument receives a stable `cli.buildtools.*` ID. The model records positional/optional kind, action, cardinality, choices, defaults, type, parser description, usage, exact `--help` output, source, and a deterministic contract digest. Missing argument prose stays visibly marked as a parser documentation gap; descriptions belong in `create_parser()` so executable help and generated reference improve together.

The current scope is `internal`: the model gives revision-pinned truth but does not promise a versioned CLI support line. Helper scripts and the `package.py` invocation/payload domain have separate models below. All three models participate in the aggregate revision comparator described below.

Regenerate and verify from the engine root:

```bash
python BuildTools/tests/test_docs_cli.py
python BuildTools/docs_cli.py --write
python BuildTools/docs_cli.py --check
```

The focused test compares generated top-level help with the executable `buildtools.py --help`, validates deterministic IDs and Markdown escaping, and proves write/check stale detection. `BuildTools/docs_validate.py` and documentation CI also reject missing or manually edited CLI outputs.

## Generated helper CLI reference

[English helper CLI reference](../../reference/helper-cli/index.md) is the human entry point for executable engine helper scripts outside the main BuildTools and package command lines; the maintained translation starts at the [Russian helper CLI reference](../../../ru/reference/helper-cli/index.md). `BuildTools/HelperCliInterface.json` owns the helper ID, purpose, owner, audiences, and invocation owner. `BuildTools/docs_helper_cli.py` imports each declared `create_parser()` factory and emits [generated/helper-cli.json](../../../generated/helper-cli.json), canonical English exact help-backed pages, and durable pointers at the former `Docs/generated/helper-cli/*.md` routes.

The generator also scans `BuildTools/**/*.py` with Python's AST. A new top-level `create_parser()` must be included in the helper manifest or explicitly excluded because another canonical model owns it; otherwise generation fails. This keeps the main `buildtools.py` and `package.py` surfaces separate while preventing a new executable helper from bypassing documentation.

Every helper, argument, and subcommand has a stable `helper-cli.*` ID. The current model covers code generation, Mono script compilation, coverage collection/reporting, gameplay process tests, the AiControl protocol client, Windows 7 PE-import validation, Android device control, the packaged local web server, and MSI creation. It records exact 80-column help, parser type/default/choice/cardinality data, source, ownership, audience, and invocation context. The scope is `internal` and revision-pinned; publishing exact syntax does not promise cross-revision compatibility.

Regenerate and verify from the engine root:

```bash
python BuildTools/tests/test_docs_helper_cli.py
python BuildTools/docs_helper_cli.py --write
python BuildTools/docs_helper_cli.py --check
```

The focused test invokes every real helper and subcommand with `--help`, checks deterministic output and IDs, proves AST inventory enforcement, and exercises write/check stale detection. Parser help belongs in the executable factory; ownership prose belongs in the manifest.

## Generated native-extension interface

[Native extension reference](../../reference/native-extension/index.md) is the exact reference for project-native source roles consumed by current targets, supported engine hooks, generated fallbacks, and native binding rules. `BuildTools/NativeExtensionInterface.json` is documentation/validation data rather than a runtime input; `BuildTools/docs_native_extension.py` and the structural CMake test compare it with current CMake/codegen behavior and emit [generated/native-extension.json](../../../generated/native-extension.json) plus role, hook, and binding pages.

Every role, hook, and binding rule has a stable `native-extension.*` ID. The model records role libraries/consumers, hook signatures/call sites/defaults, compatibility-hashed presence, and registration/namespace/pointer/dependency rules. Its scope is `experimental` and revision-pinned: source-compatible use at a pinned engine revision is documented, but binary compatibility across independently built revisions is not promised. Project implementations, SDKs, settings, persistence, and packaging remain project-owned.

Regenerate and verify from the engine root:

```bash
python BuildTools/tests/test_docs_native_extension.py
cmake -P BuildTools/tests/validate_native_extension_interface.cmake
python BuildTools/docs_native_extension.py --write
python BuildTools/docs_native_extension.py --check
```

The engine-owned [minimal project](../../../../Examples/MinimalProject/README.md) then proves one `SERVER` export and one optional hook through configure, codegen, bake, link, and runtime. Authoring and lifecycle guidance lives in [Native Extensions](../../how-to/native-extensions.md).

## Generated prototype-format reference

[en/reference/prototype-format/index.md](../../reference/prototype-format/index.md) is the exact reference for prototype input selection, section forms, control directives, built-in `HasProtos` entity types/properties, and source-backed validation rules. `BuildTools/PrototypeFormatInterface.json` owns the reviewed grammar contract; `BuildTools/docs_prototype_format.py` validates its anchors against `ProtoBaker`, configuration/property parsing, and settings source, then derives the built-in property catalog from the live API metadata model.

Every section, directive, rule, entity type, and property has a stable `prototype-format.*` ID. Grammar/rule entries are `experimental`; the revision-derived entity/property inventory remains `internal`. The model records parser applicability, side-specific skip behavior, temporary/virtual exclusion, provenance, and the current engine defaults without importing any embedding project's extensions, fixed types, custom properties, IDs, or gameplay semantics.

Regenerate and verify from the engine root:

```bash
python BuildTools/tests/test_docs_prototype_format.py
python BuildTools/docs_prototype_format.py --write
python BuildTools/docs_prototype_format.py --check
```

Human authoring, inheritance, migration, and project-boundary guidance lives in [Prototype Format](../../how-to/content/prototype-format.md). An embedding project should publish a companion catalog from combined engine/project metadata and prove semantic field combinations with its real bake and subsystem tests.

## Generated map-format reference

[en/reference/map-format/index.md](../../reference/map-format/index.md) is the exact reference for `.fomap` sections, placement directives, item ownership, Map/Critter/Item properties, side-specific bake output, mapper round-trip behavior, and runtime materialization. `BuildTools/MapFormatInterface.json` owns reviewed grammar and behavioral rules; `BuildTools/docs_map_format.py` validates live loader/baker/mapper/runtime anchors and derives the property and `ItemOwnership` catalogs from the current API model.

Every section, directive, ownership mode, rule, and property has a stable `map-format.*` ID. Grammar, ownership, and behavior rules are `experimental`; the revision-derived property catalog is `internal`. Project map IDs, custom metadata, catalogs, composition policy, quests, encounters, and level-design rules remain outside the engine model.

Regenerate and verify from the engine root:

```bash
python BuildTools/tests/test_docs_map_format.py
python BuildTools/docs_map_format.py --write
python BuildTools/docs_map_format.py --check
```

Human authoring and project-boundary guidance lives in [Map Format](../../how-to/content/map-format.md). An embedding project should link to it for reusable grammar while keeping its concrete map catalog, graphical kits, generators, and gameplay validation in project docs.

## Generated model-format reference

[en/reference/model-format/index.md](../../reference/model-format/index.md) is the human projection of the current `.fo3d` parser, mesh-input, composition, animation-adjacency, and validation contract. `BuildTools/ModelFormatInterface.json` owns stable compile-limit, asset, token, and rule records; `BuildTools/docs_model_format.py` compares every accepted spelling directly with `ModelDescriptionParser::ParseToken`, validates source anchors and project-interface limits, and emits [generated/model-format.json](../../../generated/model-format.json) plus seven canonical Markdown pages and durable legacy routes.

Regenerate and verify from the engine root:

```bash
python BuildTools/tests/test_docs_model_format.py
python BuildTools/docs_model_format.py --write
python BuildTools/docs_model_format.py --check
```

The generated reference is exhaustive for the current parser surface. [Model Format](../../how-to/content/model-format.md) remains the human guide for ordering, state transitions, composition practices, current versus removed tokens, and the engine/project ownership boundary.

## Generated text-format reference

[en/reference/text-format/index.md](../../reference/text-format/index.md) is the human
projection of raw `.fotxt` syntax, ordered language normalization, prototype
`$Text`, runtime lookup, renderer color tags, and validation.
`BuildTools/TextFormatInterface.json` owns stable rule and method records;
`BuildTools/docs_text_format.py` validates their live source anchors, derives
the Engine defaults for `Baking.BakeLanguages` and `Client.Language`, verifies
the five prototype output packs against `ProtoTextBaker`, and emits
[generated/text-format.json](../../../generated/text-format.json) plus six Markdown
pages.

Regenerate and verify from the engine root:

```bash
python BuildTools/tests/test_docs_text_format.py
python BuildTools/docs_text_format.py --write
python BuildTools/docs_text_format.py --check
```

[Text and Localization](../../how-to/content/text-and-localization.md) remains the human guide for
authoring practices, bake-time fallback, runtime missing-data behavior, and the
boundary from project-owned pack catalogs and lexem formatters.

## Generated effect-format reference

[en/reference/effect-format/index.md](../../reference/effect-format/index.md) is the
human projection of `.fofx` sections, pass/render state, vertex layouts,
built-in samplers and uniform buffers, descriptor conventions, backend
artifacts, runtime cache identity, script values, and validation.
`BuildTools/EffectFormatInterface.json` owns stable limit, section, option,
resource, baking, runtime, script-method, and validation records;
`BuildTools/docs_effect_format.py` validates their live baker/renderer/runtime
anchors, derives the current CMake limit defaults, and emits
[generated/effect-format.json](../../../generated/effect-format.json) plus seven
Markdown pages.

Regenerate and verify from the engine root:

```bash
python BuildTools/tests/test_docs_effect_format.py
python BuildTools/docs_effect_format.py --write
python BuildTools/docs_effect_format.py --check
```

[Effect Format](../../how-to/content/effect-format.md) remains the human guide for authoring,
backend/resource behavior, path-only cache identity, `ScriptValueBuf`
lifetime, project override policy, and visible cross-backend validation.

## Generated image-format reference

[en/reference/image-format/index.md](../../reference/image-format/index.md) is the
human projection of the twelve ImageBaker source extensions, FOFRM fields and
flattening, legacy filename selectors, private baked records, stock client
factory coverage, sprite sheets, atlases, caches, and validation.
`BuildTools/ImageFormatInterface.json` owns stable format, field, option,
baking, runtime, and validation records. `BuildTools/docs_image_format.py`
validates their live baker/client/test anchors, derives the baker and default
runtime extension lists, verifies the ignored FOFRM Effect boundary, and emits
[generated/image-format.json](../../../generated/image-format.json) plus seven Markdown
pages.

Regenerate and verify from the engine root:

```bash
python BuildTools/tests/test_docs_image_format.py
python BuildTools/docs_image_format.py --write
python BuildTools/docs_image_format.py --check
```

[Image And Sprite Formats](../../how-to/content/image-format.md) remains the human guide for source selection,
FOFRM authoring, legacy import, baking/runtime boundaries, atlas/cache behavior,
project policy, diagnostics, and visible validation.

## Generated particle-format reference

[en/reference/particle-format/index.md](../../reference/particle-format/index.md) is the
human projection of the `.spark`/`.spk` and `.efkproj`/`.efk` pipelines, the
registered SPARK graph surface, the supported Effekseer profile, mandatory
baked bounds, client caches/render paths, integrations, and validation.
`BuildTools/ParticleFormatInterface.json` owns stable family, object, XML,
renderer, tooling, runtime, integration, and validation records.
`BuildTools/docs_particle_format.py` derives the live registry, local descriptor
attributes, editor parity, backend extensions and options, and Engine defaults, then emits
[generated/particle-format.json](../../../generated/particle-format.json) plus eight
Markdown pages.

Regenerate and verify from the engine root:

```bash
python BuildTools/tests/test_docs_particle_format.py
python BuildTools/docs_particle_format.py --write
python BuildTools/docs_particle_format.py --check
```

[Particle Format And Runtime](../../how-to/content/particle-format.md) remains the human guide for authoring,
editor round-trip, effects/textures, caches, atlas/direct-scene selection,
script/model integration, diagnostics, and project-owned visual validation.

## Generated font-format reference

[Font Format reference](../../reference/font-format/index.md) is the human
projection of FOFNT and AngelCode BMFont descriptors, resource delivery, font
slots, bind-time scale, text measurement and wrapping, rendering flags, inline
colors, atlas/cache behavior, and validation. `BuildTools/FontFormatInterface.json`
owns reviewed format, field, binding, layout, rendering, and validation records.
`BuildTools/docs_font_format.py` derives live extension registries, parser keys,
binary constants, signed metric reads, enums, scale guards, atlas/cache behavior,
and bundled descriptor evidence, then emits
[generated/font-format.json](../../../generated/font-format.json) plus eight Markdown
pages.

Regenerate and verify from the engine root:

```bash
python BuildTools/tests/test_docs_font_format.py
python BuildTools/docs_font_format.py --write
python BuildTools/docs_font_format.py --check
```

[Font Formats And Text Layout](../../how-to/content/font-format.md) remains the human guide for authoring, binding,
layout, diagnostics, project-owned slot/glyph policy, and visible validation.

## Generated audio reference

[Audio reference](../../reference/audio/index.md) is the exact projection of the
stock client's WAV, ACM, and Ogg Vorbis delivery, decoding, streaming, mixing,
repeat, and playback surface. `BuildTools/AudioInterface.json` owns stable
format, delivery, decoding, playback, and validation records.
`BuildTools/docs_audio.py` validates those records against RawCopyBaker,
ResourceManager, SoundManager, script methods, settings, and live source
anchors, then emits [generated/audio.json](../../../generated/audio.json), six canonical
English pages, and durable pages at the former `Docs/generated/audio/*.md`
routes.

The model is `experimental` and revision-pinned. It deliberately excludes a
game's sound catalog, spatialization rules, music state machine, mastering,
licensing, and audible acceptance evidence.

Regenerate and verify from the engine root:

```bash
python BuildTools/tests/test_docs_audio.py
python BuildTools/docs_audio.py --write
python BuildTools/docs_audio.py --check
```

[Audio Resources And Playback](../../how-to/content/audio.md) remains the human
guide for delivery, authoring choices, diagnostics, project boundaries, and
audible validation.

## Generated video reference

[Video reference](../../reference/video/index.md) is the exact projection of the
current Ogg/Theora primitive: raw-copy delivery, whole-resource buffering,
packet/header/frame decoding, CPU YCbCr-to-RGBA conversion, fullscreen queues,
input interruption, separate music pairing, embedded `VideoPlayback`, and
visible validation boundaries. `BuildTools/VideoInterface.json` owns the stable
records; `BuildTools/docs_video.py` validates their source anchors and emits
[generated/video.json](../../../generated/video.json), seven canonical English pages,
and durable legacy routes.

The model is `experimental` and revision-pinned. It is not a project cinematic
system and does not claim container-audio decoding, subtitles, localization,
story policy, streaming, DRM, accessibility acceptance, or asset provenance.

Regenerate and verify from the engine root:

```bash
python BuildTools/tests/test_docs_video.py
python BuildTools/docs_video.py --write
python BuildTools/docs_video.py --check
```

[Video Resources And Playback](../../how-to/content/video.md) remains the human
guide for delivery, runtime use, diagnostics, project policy, and visible
acceptance.

## Generated GUI-runtime reference

[GUI runtime reference](../../reference/gui-runtime/index.md) is the exact
projection of the reusable AngelScript GUI object model and screen API in
`Source/Scripting/AngelScript/CoreScripts/Gui.fos` and its input integration.
`BuildTools/GuiRuntimeInterface.json` owns stable type, callback, lifecycle,
layout, rendering, input, integration, and validation records.
`BuildTools/docs_gui_runtime.py` validates declarations and behavioral anchors
against live scripts and native integration points, then emits
[generated/gui-runtime.json](../../../generated/gui-runtime.json), seven canonical
English pages, and durable legacy routes.

The model is `experimental` and revision-pinned. It explicitly declares zero
Engine-owned declarative GUI formats: `.fogui`, generators, screen catalogs,
styles, fonts, images, gameplay presentation, and accessibility acceptance
remain embedding-project responsibilities.

Regenerate and verify from the engine root:

```bash
python BuildTools/tests/test_docs_gui_runtime.py
python BuildTools/docs_gui_runtime.py --write
python BuildTools/docs_gui_runtime.py --check
```

[GUI Runtime](../../how-to/runtime/gui.md) remains the human guide for lifecycle,
project hooks, authoring boundaries, diagnostics, and end-to-end validation.

## Generated package interface

[the generated package reference](../packages/index.md) is the human entry point for package declarations and payloads. `BuildTools/PackageInterface.json` models the current `DefinePackage`/`package.py` capabilities for documentation and validation; it is not read by the packager. `BuildTools/docs_package.py` also calls the executable `package.py::create_parser()` and emits [generated/package.json](../../../generated/package.json) plus declaration, matrix, payload/artifact, and CLI pages.

The model gives `DefinePackage`, its two clauses and per-binary `POSTFIX` modifier, six targets, six platforms, nineteen pack tokens, six platform payloads, and thirteen packager arguments stable `package.*` IDs. The target set includes standalone `AnimationViewer` and `ParticleViewer` packages. It distinguishes implemented platforms/packs from explicit unsupported and placeholder entries rather than presenting comments as support. Runtime validation rejects unknown or duplicate packs/architectures, unsupported platforms, incompatible target/platform/pack combinations, missing required `NoRes`, and modifier-only lists before output staging. The Windows matrix includes the `win32-win7` and `win64-win7` selectors and documents their canonical-architecture plus explicit-postfix behavior.

The package scope is currently `internal` and revision-pinned. It does not import an embedding project's package matrix or release policy, and it excludes project configuration-key schema, secret provisioning, exact resource content, deployment topology, and external signing-tool operation. Those project responsibilities remain documented by the embedding project.

Regenerate and verify from the engine root:

```bash
python BuildTools/tests/test_docs_package.py
cmake -P BuildTools/tests/validate_package_interface.cmake
python BuildTools/docs_package.py --write
python BuildTools/docs_package.py --check
```

The focused Python test covers manifest/parser agreement, runtime pack validation, executable help, stable IDs, deterministic escaped Markdown, and stale detection. The structural CMake test executes the real `DefinePackage` macro and verifies `CONFIG`, repeated `BINARY`, per-entry `POSTFIX` storage, and isolation from sibling entries. The standalone validator and documentation CI require both checks and byte-for-byte generated outputs.

## Generated AiControl protocol reference

[AiControl protocol reference](../../reference/ai-control-protocol/index.md)
is the human entry point for the experimental project-neutral AI-control
envelope. `BuildTools/AiControlProtocol.json` owns 49 stable wire, method, error,
common-command, security, integration, and validation entries.
`BuildTools/docs_ai_control_protocol.py` verifies those declarations against the
standard-library reference client and runnable sample, then emits
[generated/ai-control-protocol.json](../../../generated/ai-control-protocol.json) plus six
checked reference pages.

The model deliberately excludes project observations, game action names,
administrator commands, readiness semantics, MCP namespaces, and launch policy.
`Examples/AiControlSample` proves authorization, request/response framing,
bounded command/event state, accepted-command correlation, asynchronous
completion, observation replacement, and event cursors without claiming to be a
native FOnline client. Project native/script/server-authority and shipping-build
evidence remains mandatory under [AiControl Protocol](../../how-to/ai-control-protocol.md).

Regenerate and verify from the engine root:

```bash
python BuildTools/tests/test_ai_control_protocol.py
python BuildTools/tests/test_docs_ai_control_protocol.py
python Examples/AiControlSample/run_protocol_smoke.py
python BuildTools/docs_ai_control_protocol.py --write
python BuildTools/docs_ai_control_protocol.py --check
```

## Generated public example repository registry

[the generated public-example registry](../public-examples/index.md) is the human projection of the external example portfolio. `Examples/PublicRepositories.json` owns stable repository IDs, ordering, dependencies, accountable roles, lifecycle state, required checks/artifacts, exact Engine pin policy, compatibility lanes, asset policy, and exit gates. `BuildTools/docs_examples.py` validates that source and `Examples/PublicRepositoryTemplate`, then emits [generated/public-examples.json](../../../generated/public-examples.json).

The same tool validates a candidate external repository. In pinned mode it requires `example-repository.json`, the committed `Engine/` gitlink, and the checked-out Engine revision to agree exactly. In current mode it preserves the release gitlink while reporting the temporarily tested Engine revision. Both modes reject missing governance files, unresolved publication placeholders, and incomplete asset provenance.

Regenerate and verify from the engine root:

```bash
python BuildTools/tests/test_docs_examples.py
python BuildTools/docs_examples.py --write
python BuildTools/docs_examples.py --check
```

The registry is documentation/governance metadata, not a ninth runtime compatibility domain. Example ownership and publication authority are defined in [Public Example Repositories](../../how-to/build/public-example-repositories.md) and [ADR-0005](../../contributing/decisions/0005-public-example-repository-ownership.md).

## Documentation snippet inventory

`BuildTools/SnippetPolicy.json` declares the complete supported fence-language
and parser-harness map. `BuildTools/docs_snippets.py` scans every public,
current, human manifest document, including generated reference pages, and
writes [generated/snippets.json](../../../generated/snippets.json). The report records
stable document ownership, heading/line location, normalized content hash,
template status, contract, harness, and result for every fenced block.

Normative command/source/config/data blocks require 100 percent parser
coverage. Plain `text` blocks are retained as evidence and checked for
transport-safe structure. The `--external` gate invokes Bash in parse-only
mode and the PowerShell language parser; neither harness executes documented
commands. C-family lexical checks do not replace the owning compile, bake, or
example smoke when prose claims a semantic outcome.

Regenerate and verify before localization/site/AI delivery whenever fenced
content or policy changes:

```bash
python BuildTools/tests/test_docs_snippets.py
python BuildTools/docs_snippets.py --write --external
python BuildTools/docs_snippets.py --check --external
```

See [Documentation Snippet Validation](../../contributing/documentation/snippets.md) for authoring,
template, failure, and semantic-owner rules.

## Documentation evidence and media artifacts

`BuildTools/ExternalProjectEvidence.json` records the exact Last Frontier and
FOnline TLA revisions inspected for reusable practices. The records classify
each concern as promoted, a promotion candidate, project-owned, or a boundary
owned elsewhere. `BuildTools/docs_external_evidence.py` validates source paths,
Engine targets, ownership, review roles, and promotion gates, then emits
[generated/external-project-evidence.json](https://github.com/cvet/fonline/blob/master/Docs/generated/external-project-evidence.json)
and its internal audit page. External projects remain discovery and
compatibility evidence; they never override Engine source, tests, manifests,
or Engine-owned examples.

`BuildTools/DocumentationDiagrams.json` and
`BuildTools/DocumentationScreenshots.json` are reviewed inventories for visual
documentation evidence. `BuildTools/docs_diagrams.py` emits deterministic
desktop/mobile SVG diagrams from declared nodes and relationships.
`BuildTools/docs_screenshots.py` validates checked-in PNG paths, dimensions,
source revision, capture command, owner, alt text, and freshness policy. The
resulting JSON models let the site and CI verify provenance without making
binary media a runtime contract.

Regenerate and verify these artifacts from the engine root:

```bash
python BuildTools/tests/test_docs_external_evidence.py
python BuildTools/tests/test_docs_diagrams.py
python BuildTools/tests/test_docs_screenshots.py
python BuildTools/docs_external_evidence.py --write
python BuildTools/docs_diagrams.py --write
python BuildTools/docs_screenshots.py --write
```

Refresh the pinned external snapshots before changing reusable-practice
claims, and update the evidence record in the same change. A screenshot is
supporting evidence only; the owning source-backed reference and its executable
validation remain authoritative.

## Generated support and localization status

`BuildTools/SupportMatrix.json` is the reviewed source for current host, target, architecture, compiler, application, and evidence profiles. `BuildTools/docs_support_matrix.py` validates referenced BuildTools targets and workflow lanes, then emits [generated/support-matrix.json](../../../generated/support-matrix.json) and the [generated support matrix](../platforms/generated-matrix.md). The model distinguishes source capability from a required build, process smoke, and embedding-project or device qualification; it is not a claim that every combinatorial target has been run.

`BuildTools/docs_localization.py` projects the canonical human-document inventory, [translation-glossary.json](../../../translation-glossary.json), normalized source hashes, and existing locale counterparts into [generated/translation-status.json](../../../generated/translation-status.json). Existing translations must carry the expected document ID, locale, source path/hash, byte-identical fenced code, and language-preserving internal links. Missing counterparts are reportable during the pre-production migration and fail when `--enforce-complete` is enabled.

Regenerate and verify from the engine root:

```bash
python BuildTools/docs_support_matrix.py --write
python BuildTools/docs_support_matrix.py --check
python BuildTools/docs_localization.py --write
python BuildTools/docs_localization.py --check
```

These models describe documentation evidence and translation freshness. They do not add runtime compatibility domains or turn a CI build into a support promise. Human interpretation and update procedures live in the [Support Matrix](../../reference/platforms/support-matrix.md) and [Translation Workflow](../../contributing/documentation/translation.md).

## AI documentation delivery

The machine-oriented entry layer is generated from the same [documentation-manifest.json](../../../documentation-manifest.json) that owns human pages. `BuildTools/docs_ai_delivery.py` does not parse engine source or invent a second API model. It projects reviewed document metadata and canonical Markdown into:

- root `llms.txt`, which routes public current documents through source-ref-pinned clean Markdown URLs, links their canonical HTML routes, and lists all canonical generated JSON models;
- root `llms-full.txt`, which contains authored public current documents and generated reference indexes under a strict byte budget;
- root `docs-manifest.json`, which exposes the rolling/current version channel, deferred release-snapshot state, locale policy, public stable IDs, owner/state/disposition, source, clean Markdown, raw, and canonical HTML URLs, source provenance, normalized content hashes, and generated-artifact hashes.

[ai-evaluation.json](../../../ai-evaluation.json) is the reviewed, versioned task source for architecture, scripting, content, debugging, migration, and release questions. `BuildTools/docs_ai_eval.py` validates task ownership and answer-evidence sentinels, runs every retrieval query through the same `docs_site.search_documents` ranking contract used by browser search, and writes [generated/ai-evaluation-report.json](../../../generated/ai-evaluation-report.json). The deterministic report proves route selection and current evidence only; model-family answer runs remain separately reviewed evidence under [AI Documentation Evaluation](../../contributing/documentation/ai-evaluation.md).

The full-context output deliberately excludes generated detail pages. Their canonical JSON models carry complete method, type, property, setting, CMake, CLI, native-extension, prototype, map, model, text, effect, image, particle, font, audio, video, GUI-runtime, AiControl-protocol, package, and public-example inventories more accurately and compactly. The generated indexes remain in the bundle so an agent can select the correct model and source.

Regenerate and verify from the engine root after any manifest or inventoried Markdown change:

```bash
python BuildTools/tests/test_docs_ai_delivery.py
python BuildTools/tests/test_docs_ai_eval.py
python BuildTools/docs_site.py --write
python BuildTools/docs_ai_eval.py --write
python BuildTools/docs_ai_eval.py --check
python BuildTools/docs_ai_delivery.py --write
python BuildTools/docs_ai_delivery.py --check
```

The public files are discovery/transport artifacts, not contract owners. API stability still comes from source-owned metadata and ADR-0002; documentation delivery and byte-budget policy come from [ADR-0003](../../contributing/decisions/0003-manifest-backed-ai-documentation-delivery.md).

## Documentation site data

Human site navigation, search, version/locale identity, and route migration use the same manifest records without becoming generated API domains. `BuildTools/docs_site.py` resolves stable document IDs into `_data/docs-site.json` for Jekyll/Liquid, tokenizes public current human Markdown into independent bounded English and Russian browser-search indexes, and writes `Docs/generated/document-routes.json` for current URLs, canonical future owners, available locale pairs, and required legacy redirects.

The navigation model requires exact coverage of top-level reader pages while keeping generated detail pages behind their generated indexes. Search includes those detail pages, weights titles and headings above body tokens, preserves technical identifiers, and stores only compact postings plus result metadata. It does not copy full Markdown bodies into the browser artifact or create a hosted search contract.

After Jekyll renders the repository, `BuildTools/docs_site_artifact.py` validates the completed `_site` tree against the route and artifact models. This post-build layer proves that promised routes, available locale pages, static JSON/text/assets, canonical metadata, accessibility landmarks/names, search targets, and publishable local links survived Jekyll processing. Its JSON report is CI evidence, not another checked-in generated reference.

`BuildTools/docs-browser/audit.mjs` then serves that exact tree from an ephemeral loopback port. The lock-file-pinned Playwright Chromium visits every route in the generated catalog at 1440 x 1000 and 390 x 844, injects the pinned axe-core engine for the declared WCAG 2.2 A/AA tags, and records runtime/resource, responsive-layout, page-overflow, and accessibility findings. Separate interaction profiles prove skip navigation, modal search, theme persistence, code-copy status, mobile drawer semantics, focus containment, and Escape restoration. Its JSON report and desktop/mobile screenshots are CI evidence; neither is a generated engine compatibility model.

Regenerate and verify after public page title/path/state/target changes, version/localization policy changes, navigation/routing policy changes, or search policy changes:

```bash
python BuildTools/tests/test_docs_site.py
python BuildTools/tests/test_docs_site_layout.py
python BuildTools/tests/test_docs_site_artifact.py
python BuildTools/tests/test_docs_browser.py
python BuildTools/docs_site.py --write
python BuildTools/docs_site.py --check
python BuildTools/docs_site_artifact.py --site-dir _site
npm ci --prefix BuildTools/docs-browser
npx --prefix BuildTools/docs-browser playwright install chromium
npm --prefix BuildTools/docs-browser run audit
```

These artifacts describe the current documentation revision and presentation routes. They do not participate in the eighteen-domain engine compatibility diff and do not promote a page, symbol, or helper to a stable public API. Navigation/search ownership is recorded in [ADR-0004](../../contributing/decisions/0004-manifest-backed-site-navigation-search.md); rolling/release versioning, locale targets, and durable route migration are recorded in [ADR-0006](../../contributing/decisions/0006-documentation-version-locale-routing.md).

## Multi-domain diff and change disposition

`BuildTools/docs_contract_diff.py` compares all eighteen canonical generated models with the same paths at a base revision. It delegates native symbols to `BuildTools/docs_api_diff.py`, preserving overload-family and source-owned stability behavior, and compares CMake/CLI/package/helper-CLI/native-extension/prototype-format/map-format/model-format/text-format/effect-format/image-format/particle-format/font-format/audio/video/GUI-runtime/AiControl-protocol entries by their stable IDs.

Breaking changes use baseline stability. A removed or modified `stable`, `experimental`, or `deprecated` entry cannot pass `--enforce` without an exact domain-bound entry in `Docs/contract-change-dispositions.json`; changing the current label to `internal` does not bypass the gate. Model-source, model-scope, and model-level contract changes always require disposition. Internal declaration changes remain visible without becoming accidental compatibility promises.

The aggregate diff writes `Workspace/contract-diff.json` and `.md`; CI uploads them rather than checking revision-pair reports into the current-reference site. The complete local/CI workflow, classifications, per-domain digest binding, schema-v2 ledger, limitations, and review checklist are in [Generated Contract Change Management](../../contributing/contract-change-management.md).

## Project remote-call supplement

Remote calls are declared in project `.fos` files and parsed by `Source/Tools/MetadataBaker.cpp`, so they must not be added to `api.json` by a parallel source parser. After a project bake, `BuildTools/docs_metadata.py` strictly decodes the authoritative `Metadata.fometa-server` and `Metadata.fometa-client` outputs, verifies that both sides agree, and emits a project-owned JSON/Markdown catalog.

From an embedding project root:

```bash
python Engine/BuildTools/docs_metadata.py \
  --root . \
  --metadata Baking/Metadata/Metadata.fometa-server \
  --metadata Baking/Metadata/Metadata.fometa-client \
  --write
```

Use the same arguments with `--check` after baking in project CI. The default outputs are `Docs/generated/project-remote-calls.json` and `Docs/generated/project-remote-calls.md`. They carry stable `script.remote-call.<target>.<name>` IDs, normalized signatures, caller/handler surfaces, input hashes, and paired direction evidence. They deliberately expose only a source file hint because the baked format does not retain a repository-relative path or line.

The complete declaration, runtime, authority, compatibility, and troubleshooting contract is [Remote Calls](../../reference/scripting/remote-calls.md). The engine-owned minimal project validates the decoder against real baker output without making this repository depend on an external game.

## Metadata registration entry points

Hand-authored declarations live in `Source/Common/MetadataRegistration.h`:

- `RegisterServerMetadata()`
- `RegisterClientMetadata()`
- `RegisterMapperMetadata()`
- `RegisterServerStubMetadata()`
- `RegisterClientStubMetadata()`
- `RegisterMapperStubMetadata()`
- `RegisterDynamicMetadata()`
- `ReadMetadataBin()`
- `ReadMetadataVersion()`

`Source/Common/MetadataRegistration.template.cpp` is the template used to generate side-specific registration files. It contains code-generation markers such as `///@ CodeGen RegisterHelpers` and `///@ CodeGen Register`.

`Source/Common/GenericCode.template.cpp` is the template for generated common code.

## Engine hook tags

Project/native extension code can mark selected C++ functions with `///@ EngineHook`. `BuildTools/codegen.py` validates hook names and emits no-op stubs for hooks that the embedding project does not implement. Current hook names recognized by the generator are:

- `ApplicationInitHook(AppInitFlags, GlobalSettings&)`
- `ApplicationShutdownHook()`
- `ServerInitHook(ServerEngine*)`
- `ClientInitHook(ClientEngine*)`
- `ClientStartupSettingsHook(GlobalSettings&, int32_t clientIndex, bool embedded)`
- `SetupBakersHook(...)`
- `CheckCritterVisibilityHook(...)`
- `CheckItemVisibilityHook(...)`

`ClientStartupSettingsHook` is called by app entry points immediately before constructing a client engine. Use it for project-owned startup setting adjustments; do not use it as a gameplay authority bypass.

`ApplicationShutdownHook` is a native lifecycle hook for project-owned process integrations that must be stopped before a client runtime DLL is unloaded. It is intentionally not part of the compatibility hash because it does not change script metadata, saved data, or the network contract.

## Dynamic metadata

`Source/Common/MetadataRegistration.cpp` implements `RegisterDynamicMetadata()`. It reads binary metadata sections and dispatches them into typed registration steps such as:

- enums
- entities
- entity holders
- fixed/value/reference types
- properties
- events
- remote calls
- settings
- migration rules

This is the runtime side of metadata that can be loaded from generated/baked data rather than compiled static registration alone.

### Metadata version

**A server and every connected client must run metadata produced by one bake.** Entity payloads address properties by the registration order in that metadata, so different bakes can make the same index refer to different properties. Such divergence is rejected as a build or deployment defect, never treated as a supported compatibility mode.

`FO_COMPATIBILITY_VERSION` cannot enforce this invariant by itself. Codegen sees engine and embedding-project C++ metadata sources, while project `///@ Property` declarations are registered from baked script metadata at runtime. The binary compatibility version therefore describes executables; the property layout belongs to resources.

`MetadataBaker` derives a deterministic metadata version from every parsed codegen tag before target filtering. Client, server, and mapper outputs from one bake share the version even though their emitted sections differ. Any tag-level contract change—property ordering, a fixed-type layout, enum values, events, settings, or remote calls—changes that version.

Every `Metadata.fometa-*` file begins with a fixed header before the section table:

| Field | Type | Purpose |
|-------|------|---------|
| magic | `uint32` | `METADATA_FILE_MAGIC`; rejects foreign or truncated input immediately |
| file version | `uint16` | `METADATA_FILE_VERSION`; a mismatch requires a rebake |
| metadata version | `uint16` length + bytes | deterministic version of the parsed tag stream |

`MakeMetadataHeader()` and `ReadMetadataHeader()` own the format in `MetadataRegistration.cpp`. `RegisterDynamicMetadata()` reads the header before any section and passes the value to `EngineMetadata::RegisterMetadataVersion()`. `ReadMetadataVersion()` reads only the header for updater and server-startup checks; runtime code retrieves the registered value through `EngineMetadata::GetMetadataVersion()`. The value is computed, not configured. `Network.ForceMetadataVersion` exists only to simulate mismatches in tests.

Four layers enforce the invariant:

1. One bake produces both `Baking.ServerResources` and `Baking.ClientResources`, and deployments update them together.
2. `UpdaterBackend::LoadFromClientResources` reads the version from the client packs it will distribute and fails startup with `UpdaterException` when it differs from the server's loaded version.
3. After synchronization, `Updater::FinishResourcesUpdate` re-reads local packs and returns `UpdaterResult::MetadataMismatch` before constructing `ClientEngine` if they still differ from the server.
4. The client handshake sends its version; the server returns its own version and a mismatch verdict as the final race check. See [Client Updater](../../explanation/runtime/client-updater.md#handshake).

Deserialization has an independent guard: `Properties::VerifyRestoredPropertyData()` checks that each serialized property is enabled for the target, non-virtual, and has the expected plain-data size. It throws `VerificationException` instead of reaching the strong assertion in `SetRawData`, keeping foreign layouts diagnosable.

When a mismatch appears, do not suppress the check. Server startup logs `Metadata version:` and rejection paths name both versions; updater logs also include the resource directory it read. Identify which server or client resource directory came from another bake and redeploy a matched set.

Focused coverage lives in `Test_MetadataBaker.cpp` (one version across targets and version changes), `Test_Properties.cpp` (`PropertiesRestoreRejectsForeignMetadata`), and `Test_ClientServerIntegration.cpp` (`ServerReportsMetadataMismatchInHandshake`).

Migration rules are generic `(kind, extra-info, target → replacement)` remaps with transitive resolution, authored as `///@ MigrationRule <Kind> ...`. Beyond `Proto`/`Property` (applied at proto lookup and property-name resolution), the `Enum` kind is consulted by `PropertiesSerializer` when a persisted enum value **name** no longer resolves on load: the rule remaps the old name to a current value — for scalar enum properties and enum dict keys — instead of throwing `EnumResolveException`. This keeps removed/renamed enum values from bricking old saves.

## Properties and generated contracts

`Source/Common/Properties.h` and `Source/Common/Properties.cpp` define the property runtime model used by entities and metadata. Key concepts include:

- `PropertyRawData`
- `Property`
- `PropertyRegistrar`
- `Properties`
- property getter/setter/post-set callbacks
- base type, struct layout, and serialization-related descriptors

Fixed value-type layouts are shared by native C++, AngelScript registration, and metadata field traversal. `hstring` therefore has an explicit ABI invariant: `sizeof(hstring) == sizeof(hstring::hash_t) == 8` on every supported target. On 32-bit targets the pointer-backed handle carries trailing padding to preserve that width and keep composite offsets (for example `TextPackKey`) platform-independent. The padding is not wire data: RPC/property serializers still convert the handle through `as_hash()` and resolve the received hash through the target engine's hash resolver.

When property metadata changes, inspect both the property runtime and the generator inputs/templates. Script-visible nullability or API changes should also update [Scripting](../../explanation/scripting-runtime/), [Script Methods Map](../../reference/script-api/method-ownership.md), and [Nullability.md](../../contributing/coding-contracts/nullability.md) as applicable.

## Public API relationship

[Public Contract Index](../../reference/public-contract/index.md) is the canonical
human map of all eighteen modeled contract domains. `BuildTools/docs_public_api.py`
reads the checked generated models and renders matching English and Russian
indexes; root [PUBLIC_API.md](../public-contract/index.md) is now only a durable legacy
route. The index reports each domain's current stability and points to its human
reference and machine model without promoting reachable implementation details
to supported API.

The native model remains separately authoritative for the current codegen
surface: use the [native script API reference](../../reference/script-api/index.md)
for human browsing and [generated/api.json](../../../generated/api.json) for exact
machine consumption. Most discovered symbols still inherit the default
`internal` label until their owners add explicit source-backed classifications;
the existence of a complete inventory is not a compatibility promise.

Regenerate and verify the contract index after any participating model changes:

```bash
python BuildTools/tests/test_docs_public_api.py
python BuildTools/docs_public_api.py --write
python BuildTools/docs_public_api.py --check
```

## Metadata and baker relationship

Metadata generation and metadata baking are related but not identical:

- Codegen produces generated C++/include files used by compiled targets.
- `MetadataBaker` participates in resource baking and produces side-specific metadata for runtime loading and project remote-call documentation.
- `RegisterDynamicMetadata()` consumes metadata binary data from resources.

For resource baking details, see [Baking Pipeline](../../explanation/content-pipeline/baking.md).

## Tests to inspect

Relevant tests include:

- `Source/Tests/Test_EngineMetadata.cpp`
- `Source/Tests/Test_MetadataBaker.cpp`
- `BuildTools/tests/test_docs_api.py`
- `BuildTools/tests/test_docs_api_diff.py`
- `BuildTools/tests/test_docs_contract_diff.py`
- `BuildTools/tests/test_docs_native_extension.py`
- `BuildTools/tests/test_docs_audio.py`
- `BuildTools/tests/test_docs_video.py`
- `BuildTools/tests/test_docs_gui_runtime.py`
- `BuildTools/tests/test_docs_ai_control_protocol.py`
- `BuildTools/tests/test_docs_public_api.py`
- `BuildTools/tests/test_docs_metadata.py`
- `Source/Tests/Test_Properties.cpp`
- Baker/codegen-adjacent tests such as `Test_BakerSetup.cpp` and the specific baker tests when metadata affects baked resources.

If a generated script API change is involved, inspect AngelScript-related tests as well.

## Change routing

- CMake generator arguments/output list: `BuildTools/cmake/stages/Codegen.cmake`.
- Generator script behavior: `BuildTools/codegen.py`.
- Static metadata registration template: `Source/Common/MetadataRegistration.template.cpp`.
- Generated common code template: `Source/Common/GenericCode.template.cpp`.
- Runtime dynamic metadata reader/registrar: `Source/Common/MetadataRegistration.cpp`.
- Property model: `Source/Common/Properties.*` and entity/prototype metadata code.
- Metadata resource baking: `Source/Tools/MetadataBaker.*` and [Baking Pipeline](../../explanation/content-pipeline/baking.md).
- Project remote-call catalogs: `BuildTools/docs_metadata.py` and [Remote Calls](../../reference/scripting/remote-calls.md).
- Project-native roles/hooks/bindings: `BuildTools/NativeExtensionInterface.json`, `BuildTools/docs_native_extension.py`, and [Native Extensions](../../how-to/native-extensions.md).
- Prototype format and built-in authoring metadata: `BuildTools/PrototypeFormatInterface.json`, `BuildTools/docs_prototype_format.py`, and [Prototype Format](../../how-to/content/prototype-format.md).
- Map format, placement ownership, mapper normalization, and map bake/materialization: `BuildTools/MapFormatInterface.json`, `BuildTools/docs_map_format.py`, and [Map Format](../../how-to/content/map-format.md).
- Font descriptors, slot binding, text layout, and rendering: `BuildTools/FontFormatInterface.json`, `BuildTools/docs_font_format.py`, and [Font Formats And Text Layout](../../how-to/content/font-format.md).
- Audio delivery, decoding, playback, and mixing: `BuildTools/AudioInterface.json`, `BuildTools/docs_audio.py`, and [Audio Resources And Playback](../../how-to/content/audio.md).
- Ogg/Theora delivery and playback: `BuildTools/VideoInterface.json`, `BuildTools/docs_video.py`, and [Video Resources And Playback](../../how-to/content/video.md).
- Reusable AngelScript GUI types, lifecycle, layout, and input: `BuildTools/GuiRuntimeInterface.json`, `BuildTools/docs_gui_runtime.py`, and [GUI Runtime](../../how-to/runtime/gui.md).
- Project-neutral AI-control envelope and reference client: `BuildTools/AiControlProtocol.json`, `BuildTools/docs_ai_control_protocol.py`, and [AiControl Protocol](../../how-to/ai-control-protocol.md).
- Aggregate human contract routing: `BuildTools/docs_public_api.py` and [Public Contract Index](../../reference/public-contract/index.md).
- External-project discovery evidence and visual documentation assets: `BuildTools/docs_external_evidence.py`, `BuildTools/docs_diagrams.py`, and `BuildTools/docs_screenshots.py`.
- Generated-contract revision comparison and dispositions: `BuildTools/docs_contract_diff.py`, its native layer `BuildTools/docs_api_diff.py`, and [Generated Contract Change Management](../../contributing/contract-change-management.md).
- Script runtime and script-visible signatures: [Scripting](../../explanation/scripting-runtime/), [Script Methods Map](../../reference/script-api/method-ownership.md), and [Nullability.md](../../contributing/coding-contracts/nullability.md).

## Validation checklist

1. Configure from an embedding project root so project metadata sources are available.
2. Run normal code generation and verify generated files are updated as expected.
3. Run forced code generation when generator caching/dependency behavior changes.
4. Build the smallest target that compiles the generated files.
5. Run metadata/property tests relevant to the change.
6. If metadata is baked, run the relevant baker test and bake target.
7. For project remote calls, run `BuildTools/tests/test_docs_metadata.py`, then regenerate/check the paired catalog from the current bake.
8. For native-extension changes, run its focused Python/CMake checks and the minimal starter or affected project runtime path.
9. For prototype-format changes, run its focused generator test/check and rebake an affected embedding project; add project semantic validation when custom metadata or field combinations change.
10. For map-format changes, run its focused generator test/check, affected map unit tests, and an embedding-project bake; add project content validation when custom metadata or map semantics change.
11. Compare all generated contracts with `BuildTools/docs_contract_diff.py --write --enforce`; use the specialized API comparator for deeper native-symbol diagnosis and complete exact dispositions for every required break.
12. Update docs that expose changed public contracts, especially [Scripting](../../explanation/scripting-runtime/), [Remote Calls](../../reference/scripting/remote-calls.md), [Native Extensions](../../how-to/native-extensions.md), [Prototype Format](../../how-to/content/prototype-format.md), [Map Format](../../how-to/content/map-format.md), [Script Methods Map](../../reference/script-api/method-ownership.md), [Nullability.md](../../contributing/coding-contracts/nullability.md), and the [Public Contract Index](../../reference/public-contract/index.md) when applicable.
13. Run the API/reference tests plus `python BuildTools/docs_api.py --check` and `python BuildTools/docs_reference.py --check`; regenerate both layers when metadata changed.
14. Run each affected domain generator's focused test and `--check`; for a cross-domain change, run `docs_contract_diff.py --enforce` and regenerate the public contract index.
15. After public Markdown, paths, diagrams, or screenshots change, regenerate snippets, localization status, site data, AI evaluation/delivery, then validate the built Jekyll artifact and browser audit in dependency order.
