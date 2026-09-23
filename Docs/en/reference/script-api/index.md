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
| [Native script methods](methods.md) | 990 | Native methods exported to scripts. |
| [Entity properties](properties.md) | 132 | Generated entity property contracts. |
| [Engine events](events.md) | 122 | Server, client, common, and mapper events. |
| [Script types](types.md) | 994 | Entities, enums, value types, reference types, fields, and methods. |
| [Engine settings](settings.md) | 268 | Read-only engine settings grouped by domain. |
| [Migration rules](migrations.md) | 28 | Native metadata migration declarations. |

## Model quality

| Signal | Count |
| --- | --- |
| Addressable symbols | 2534 |
| Symbols with descriptions | 2534 |
| Symbols missing descriptions | 0 |
| Symbols without source provenance | 14 |
| Metadata source files | 45 |
| Explicit contract declarations | 2 |
| Explicitly classified symbols | 2534 |
| Unclassified default symbols | 0 |

## Stability labels

| Label | Symbols |
| --- | --- |
| <code>experimental</code> | 2533 |
| <code>internal</code> | 1 |

## Scope contract

The complete current inventory is <code>experimental</code> since <code>2022.1.0.wip</code>. The declaration pins 2534 stable IDs with SHA-256 <code>2c8f3b855d6998553cb36e4b165ad22a9ee41984e0b3b465b996d1b80bf4861a</code>; any symbol addition, removal, or stable-ID change fails generation until an owner reviews and updates both pins.

The native-codegen surface is offered for evaluation only, and stays revision-pinned until supported release lines exist.<br>SymbolCount and InventorySha256 force owner review of every addition, removal or stable-ID change

Contract source: [Source/Common/Common.h:47](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L47)

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
