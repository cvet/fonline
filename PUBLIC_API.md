# FOnline Engine Public API

> Placeholder route. FOnline does not yet publish a complete stable API contract from this path.

Use the current source-backed references while the public-contract and generated-reference work is in progress:

- [Generated API Reference](Docs/generated/api/index.md) for searchable, source-linked native methods, properties, events, types, settings, and migration rules.
- [Generated CMake Project Interface](Docs/generated/cmake/index.md) for source-linked project options, strict stages and hooks, and selected embedding helpers.
- [Generated BuildTools CLI Reference](Docs/generated/cli/index.md) for the main executable parser's commands, arguments, defaults, choices, and help.
- [Generated Helper CLI Reference](Docs/generated/helper-cli/index.md) for source-backed codegen, coverage, Android-device, web-server, MSI, and other helper-script command lines.
- [Generated Native Extension Interface](Docs/generated/native-extension/index.md) for role routing, supported engine hooks, fallback behavior, and native binding rules.
- [Generated Prototype Format Reference](Docs/generated/prototype-format/index.md) for sections, directives, built-in entity/property applicability, and validation rules.
- [Generated Map Format Reference](Docs/generated/map-format/index.md) for `.fomap` sections, directives, ownership, built-in placement properties, baking, and validation rules.
- [Generated Model Format Reference](Docs/generated/model-format/index.md) for `.fo3d` tokens, source assets, composition, animation adjacency, and validation rules.
- [Generated Text Format Reference](Docs/generated/text-format/index.md) for `.fotxt` syntax, language normalization, prototype `$Text`, runtime lookup, color tags, and validation.
- [Generated Effect Format Reference](Docs/generated/effect-format/index.md) for `.fofx` sections, passes, render state, resources, backend outputs, runtime caching, script values, and validation.
- [Generated Image Format Reference](Docs/generated/image-format/index.md) for source formats, FOFRM fields, legacy selectors, baking, default client loading, atlases, caches, and validation.
- [Generated Particle Format Reference](Docs/generated/particle-format/index.md) for SPARK and Effekseer source/runtime forms, baking, rendering, tooling, integrations, and validation.
- [Generated Font Format Reference](Docs/generated/font-format/index.md) for FOFNT/BMFont descriptors, binding, scaling, layout, rendering flags, inline colors, and validation.
- [Generated Package Interface](Docs/generated/package/index.md) for `DefinePackage`, targets, platforms, pack tokens, payloads, and artifacts.
- [Native Extensions](Docs/NativeExtensions.md) for project-native C++ authoring, lifecycle, dependencies, compatibility, and testing.
- [Prototype Format](Docs/PrototypeFormat.md) for prototype authoring, inheritance, references, migrations, and the engine/project boundary.
- [Map Format](Docs/MapFormat.md) for map source authoring, placement identity, ownership, mapper round-trip, and server/client bake behavior.
- [Model Format](Docs/ModelFormat.md) for `.fo3d` syntax, includes, layers, attachments, transforms, materials, cuts, baking, and runtime composition.
- [Effect Format](Docs/EffectFormat.md) for `.fofx` authoring, built-in buffers, descriptor conventions, baking, runtime selection, and project-owned slot policy.
- [Image And Sprite Formats](Docs/ImageFormat.md) for image source selection, FOFRM composition, legacy import, baked/runtime boundaries, and project-owned asset policy.
- [Particle Authoring And Runtime](Docs/ParticleFormat.md) for optional backend selection, `.spark`/`.efkproj` authoring, baked `.spk`/`.efk` resources, Engine tooling/runtime behavior, integrations, and project-owned visual policy.
- [Font Format And Text Layout](Docs/FontFormat.md) for bitmap-font authoring, Engine binding/layout/rendering behavior, and project-owned font-slot and glyph-coverage policy.
- [Generated API and Metadata](Docs/GeneratedApiAndMetadata.md) for code generation, metadata, and registration ownership.
- [Script Methods Map](Docs/ScriptMethodsMap.md) for native script-method families and their owning runtime sides.
- [Scripting](Docs/Scripting.md) for the AngelScript runtime and native binding boundary.
- [Remote Calls](Docs/RemoteCalls.md) for project-authored client/server contracts and the baked project catalog generator.
- [Configuration and Data Sources](Docs/ConfigurationAndDataSources.md) for settings/configuration behavior.
- [Canonical API model](Docs/generated/api.json) for deterministic native codegen symbols, signatures, runtime sides, nullability, defaults, source locations, and current stability labels.
- [Canonical CMake model](Docs/generated/cmake.json) for deterministic option, stage, hook, and helper records with stable IDs.
- [Canonical prototype-format model](Docs/generated/prototype-format.json) for deterministic grammar, built-in property, and validation-rule records.
- [Canonical map-format model](Docs/generated/map-format.json) for deterministic map grammar, ownership, property, bake, and validation-rule records.
- [Canonical model-format model](Docs/generated/model-format.json) for deterministic compile-limit, asset, token, and composition-rule records.
- [Canonical text-format model](Docs/generated/text-format.json) for deterministic text syntax, language, prototype-text, runtime, rendering, and validation records.
- [Canonical effect-format model](Docs/generated/effect-format.json) for deterministic syntax, render-state, resource, baking, runtime, script-method, and validation records.
- [Canonical image-format model](Docs/generated/image-format.json) for deterministic source-format, FOFRM field, filename-option, baking, runtime, and validation records.
- [Canonical particle-format model](Docs/generated/particle-format.json) for deterministic object, XML, renderer, tooling, runtime, integration, and validation records.
- [Canonical font-format model](Docs/generated/font-format.json) for deterministic descriptor, binding, layout, rendering, and validation records.
- [Generated Contract Change Management](Docs/ApiChangeManagement.md) for aggregate base-revision diff reports and mandatory public breaking-change dispositions.
- [Source Inventory](Docs/generated/source-inventory.json) for deterministic current export, test, and settings inventories.
- [Production Documentation Plan](Docs/ProductionDocumentationPlan.md#phase-3---public-api-and-generated-reference) for the remaining contract-domain and stability-review work required before this route can become authoritative.

Existing symbols and build helpers are not implicitly stable merely because they are reachable from an embedding project. Generated contract cells now distinguish explicit source classifications from the default `internal` policy, but a future public contract still requires owner-reviewed stability and migration/release disposition for breaking changes.

The current `api.json` scope is `engine-native-codegen`, and its generated Markdown pages are authoritative renderings of those declarations. Project-authored remote calls have a separate parser-owned supplement generated from paired baked metadata; they are not engine symbols and therefore do not belong in this repository's snapshot. The project-facing CMake, native-extension, prototype-format, map-format, model-format, text-format, effect-format, image-format, particle-format, and font-format surfaces are separate experimental models consumed or source-validated against runtime build/codegen/parser paths and their documentation generators. The main BuildTools CLI, package declarations/payloads, helper CLIs, and derived property inventories have source-backed internal contracts. One aggregate base-revision gate compares all fourteen generated domains while preserving their different stability policies. Explicit non-internal stability classification across the broad native API remains incomplete, so this route is still a visible placeholder.
