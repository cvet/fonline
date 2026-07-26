---
title: Generated API Reference
document_id: generated-api-index
locale: en
generated: true
---

# Generated API Reference

> Generated reference. Do not edit these pages directly. The canonical input is [`Docs/generated/api.json`](../api.json), produced from the engine's native codegen metadata parser.

This reference describes the declarations in the model's `engine-native-codegen` scope. It is searchable, source-linked input for developers and AI agents, but it is not yet the complete stable public API contract.

| Reference | Symbols | Coverage |
| --- | --- | --- |
| [Native script methods](methods.md) | 948 | Native methods exported to scripts. |
| [Entity properties](properties.md) | 133 | Generated entity property contracts. |
| [Engine events](events.md) | 121 | Server, client, common, and mapper events. |
| [Script types](types.md) | 966 | Entities, enums, value types, reference types, fields, and methods. |
| [Engine settings](settings.md) | 271 | Fixed and runtime-variable engine settings. |
| [Migration rules](migrations.md) | 28 | Native metadata migration declarations. |

## Model quality

| Signal | Count |
| --- | --- |
| Addressable symbols | 2467 |
| Symbols with descriptions | 597 |
| Symbols missing descriptions | 1870 |
| Symbols without source provenance | 147 |
| Metadata source files | 42 |
| Explicit contract declarations | 1 |
| Explicitly classified symbols | 1 |
| Default-internal symbols | 2466 |

## Stability labels

| Label | Symbols |
| --- | --- |
| <code>internal</code> | 2467 |

## Scope

Included:

- native script enums, value types, reference types, entities, properties, methods, and events
- engine settings parsed from ExportSettings
- native migration rules
- source-authored API contracts parsed from ApiContract

Excluded from the current model:

- project-authored script metadata, including remote calls
- CMake options and stage helpers
- BuildTools command-line interfaces
- package layouts and native extension ABI details

Project-facing CMake declarations are intentionally outside this model; use the separate [generated CMake project-interface reference](../cmake/index.md).

The main BuildTools command line is also outside this model; use the separate [parser-backed CLI reference](../cli/index.md).

Package declarations and payloads use their own runtime-consumed contract; use the separate [package interface reference](../package/index.md).

Source links follow the repository's `master` branch. The path and line stored in the canonical JSON are the generated provenance record; revision-pinned links remain part of the publication roadmap.
