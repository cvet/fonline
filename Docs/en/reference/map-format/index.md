---
title: Generated Map Format Reference
document_id: generated-map-format-index
locale: en
generated: true
---

# Generated Map Format Reference

> Generated reference. Do not edit directly. Update `BuildTools/MapFormatInterface.json` or the owning engine metadata, then run `python BuildTools/docs_map_format.py --write`.

[Index](index.md) | [Syntax](syntax.md) | [Properties](properties.md) | [Baking](baking.md) | [Validation](validation.md) | [Canonical JSON](../../../generated/map-format.json) | [Authoring guide](../../how-to/content/map-format.md)

This reference describes the reusable engine contract for authored `.fomap` files, their side-specific bake products, and initial runtime materialization.

## Contract status

| Field | Value |
| --- | --- |
| Stability | <code>experimental</code> |
| Support policy | The contract is generated for a pinned engine revision. Project map catalogs, custom metadata, gameplay semantics, and composition policy remain project-owned. |
| Source manifest | [BuildTools/MapFormatInterface.json](https://github.com/cvet/fonline/blob/master/BuildTools/MapFormatInterface.json) |
| Contract digest | <code>dfd4b084c30506fb490134d815f495da0843ba79733b7273e503436d909f4cf8</code> |

| Reference | Entries | Purpose |
| --- | --- | --- |
| [Syntax](syntax.md) | 8 | Sections and control directives. |
| [Properties](properties.md) | 108 | Engine-owned Map, Critter, and Item properties. |
| [Baking](baking.md) | 4 | Ownership and server/client materialization. |
| [Validation](validation.md) | 17 | Source-backed requirements and limitations. |

## Boundary

Included:

- one or more ProtoMap anchors with nested map-addressed Critter and Item sections
- map identity, placement ids, prototype references, ownership, and property overrides
- mapper normalization, side-specific baking, and runtime materialization
- engine-owned Map, Critter, and Item property catalogs

Excluded:

- project-authored properties, prototype ids, map catalogs, and directory taxonomy
- quest, encounter, balance, visual-kit, and level-design policy
- runtime save records and dynamic entities created after map materialization
- pathfinding, combat geometry, lighting design, and renderer-specific composition guidance

Embedding projects must document their added metadata, prototypes, map catalog, and level-design conventions separately.
