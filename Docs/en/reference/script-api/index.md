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
| [Native script methods](methods.md) | 997 | Native methods exported to scripts. |
| [Entity properties](properties.md) | 132 | Generated entity property contracts. |
| [Engine events](events.md) | 122 | Server, client, common, and mapper events. |
| [Script types](types.md) | 997 | Entities, enums, value types, reference types, fields, and methods. |
| [Engine settings](settings.md) | 284 | Read-only engine settings grouped by domain. |
| [Migration rules](migrations.md) | 28 | Native metadata migration declarations. |

## Model quality

| Signal | Count |
| --- | --- |
| Addressable symbols | 2560 |
| Symbols with descriptions | 2560 |
| Symbols missing descriptions | 0 |
| Symbols without source provenance | 14 |
| Metadata source files | 43 |
| Explicit contract declarations | 3 |
| Explicitly classified symbols | 2560 |
| Unclassified default symbols | 0 |

## Stability labels

| Label | Symbols |
| --- | --- |
| <code>experimental</code> | 2559 |
| <code>internal</code> | 1 |

## Scope contract

The complete current inventory is <code>experimental</code> since <code>2022.1.0.wip</code>. The declaration pins 2560 stable IDs with SHA-256 <code>a30f22a3eab74219ad9a8a135b2e78064e31294b1813408a3e0cda4c0bc9ee81</code>; any symbol addition, removal, or stable-ID change fails generation until an owner reviews and updates both pins.

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
