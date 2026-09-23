---
title: Map File Syntax
document_id: generated-map-format-syntax
locale: en
generated: true
---

# Map File Syntax

> Generated reference. Do not edit directly. Update `BuildTools/MapFormatInterface.json` or the owning engine metadata, then run `python BuildTools/docs_map_format.py --write`.

[Index](index.md) | [Syntax](syntax.md) | [Properties](properties.md) | [Baking](baking.md) | [Validation](validation.md) | [Canonical JSON](../../../generated/map-format.json) | [Authoring guide](../../how-to/content/map-format.md)

A map container is a configured prototype file with one or more `[ProtoMap]` anchors. Each anchor owns nested `[$Name/Critter]` and `[$Name/Item]` sections; an explicit map id may replace `$Name`.

## Section forms

| Stable ID | Syntax | Receiver | Cardinality | Meaning |
| --- | --- | --- | --- | --- |
| <a id="entry-map-format-section-proto-map-87b500e90a"></a><code>map-format.section.proto-map</code> | <code>[ProtoMap]</code> | <code>Map</code> | one or more per container | Declares map-level properties and starts the context used by following [$Name/Critter] and [$Name/Item] sections. |
| <a id="entry-map-format-section-critter-43a7c80eec"></a><code>map-format.section.critter</code> | <code>[$Name/Critter] or [&lt;MapId&gt;/Critter]</code> | <code>Critter</code> | zero or more per declared map | Places a critter prototype and applies per-placement Critter property overrides. |
| <a id="entry-map-format-section-item-4bff4b001c"></a><code>map-format.section.item</code> | <code>[$Name/Item] or [&lt;MapId&gt;/Item]</code> | <code>Item</code> | zero or more per declared map | Places a map, inventory, or container item and applies per-placement Item property overrides. |

## Control directives

| Directive | Sections | Syntax | Required | Default | Meaning |
| --- | --- | --- | --- | --- | --- |
| <a id="entry-map-format-directive-map-name-75435bbdeb"></a><code>$Name</code> | ProtoMap | <code>$Name = &lt;MapId&gt;</code> | no | source file basename | Selects the Map prototype id and both baked resource basenames. Without $Name the anchor resolves to the source basename; multi-map containers should name every anchor explicitly. |
| <a id="entry-map-format-directive-map-parent-a0e92e0e8a"></a><code>$Parent</code> | ProtoMap | <code>$Parent = &lt;ParentMapId&gt; [&lt;ParentMapId&gt; ...]</code> | no | no parents | Uses ordinary Map prototype inheritance during prototype baking. Mapper save output is flattened and does not preserve this directive. |
| <a id="entry-map-format-directive-map-text-0e499cda62"></a><code>$Text &lt;language&gt;</code> | ProtoMap | <code>$Text &lt;language&gt; = &lt;localized text&gt;</code> | no | no map text | Contributes localized prototype text and is preserved as an extra ProtoMap field by mapper load/save. |
| <a id="entry-map-format-directive-entity-id-3236826b49"></a><code>$Id</code> | Critter, Item | <code>$Id = &lt;positive integer&gt;</code> | no | next available positive id | Provides a shared placement identity used by ownership references. Missing, non-positive, or duplicate ids are repaired during load; authored maps should not rely on repair. |
| <a id="entry-map-format-directive-entity-proto-c5b2eab9ec"></a><code>$Proto</code> | Critter, Item | <code>$Proto = &lt;PrototypeId&gt;</code> | yes | none | Resolves the base Critter or Item prototype before placement overrides are applied. |

## Minimal map

```ini
[ProtoMap]
$Name = SmallRoom
Size = 80 80
WorkHex = 40 40

[$Name/Critter]
$Id = 1
$Proto = Guard
Hex = 38 40
Dir = 3

[$Name/Item]
$Id = 2
$Proto = MetalDoor
Hex = 42 40
```

Within one selected map, placement order is not semantic: the loader processes all critters, then all items. Keep explicit unique ids for stable ownership references and reviewable diffs.
