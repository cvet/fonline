---
title: Generated API Reference
document_id: generated-api-index
locale: en
generated: true
---

# Generated API Reference

> Generated reference. Do not edit these pages directly. The canonical input is [`Docs/generated/api.json`](../../../generated/api.json), produced from the engine's native codegen metadata parser.

This reference describes the declarations in the model's `engine-native-codegen` scope. The current inventory is available to embedding projects as an `experimental`, revision-pinned API; no broad stable compatibility promise is implied.

| Reference | Symbols | Coverage |
| --- | --- | --- |
| [Native script methods](methods.md) | 957 | Native methods exported to scripts. |
| [Entity properties](properties.md) | 133 | Generated entity property contracts. |
| [Engine events](events.md) | 121 | Server, client, common, and mapper events. |
| [Script types](types.md) | 978 | Entities, enums, value types, reference types, fields, and methods. |
| [Engine settings](settings.md) | 283 | Fixed and runtime-variable engine settings. |
| [Migration rules](migrations.md) | 28 | Native metadata migration declarations. |

## Model quality

| Signal | Count |
| --- | --- |
| Addressable symbols | 2500 |
| Symbols with descriptions | 2500 |
| Symbols missing descriptions | 0 |
| Symbols without source provenance | 14 |
| Metadata source files | 45 |
| Explicit contract declarations | 2 |
| Explicitly classified symbols | 2500 |
| Unclassified default symbols | 0 |

## Stability labels

| Label | Symbols |
| --- | --- |
| <code>experimental</code> | 2499 |
| <code>internal</code> | 1 |

## Scope contract

The complete current inventory is <code>experimental</code> since <code>2022.1.0.wip</code>. The declaration pins 2500 stable IDs with SHA-256 <code>18d532157d973c01c0d351bd1bc51393cc29b63a97c2295090d2bd17754bdbd0</code>; any symbol addition, removal, or stable-ID change fails generation until an owner reviews and updates both pins.

The complete native-codegen surface is available to embedding projects for evaluation, but it remains revision-<br>pinned until supported release lines exist. SymbolCount and InventorySha256 force owner review for every addition,<br>removal, or stable-ID change instead of silently extending this experimental promise.

Contract source: [Source/Common/Common.h:48](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L48)

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

The main BuildTools command line is also outside this model; use the separate [parser-backed CLI reference](../buildtools/index.md).

Package declarations and payloads use their own runtime-consumed contract; use the separate [package interface reference](../packages/index.md).

Source links follow the repository's `master` branch. The path and line stored in the canonical JSON are the generated provenance record; revision-pinned links remain part of the publication roadmap.
