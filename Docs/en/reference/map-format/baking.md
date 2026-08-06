---
title: Map Baking And Runtime Loading
document_id: generated-map-format-baking
locale: en
generated: true
---

# Map Baking And Runtime Loading

> Generated reference. Do not edit directly. Update `BuildTools/MapFormatInterface.json` or the owning engine metadata, then run `python BuildTools/docs_map_format.py --write`.

[Index](index.md) | [Syntax](syntax.md) | [Properties](properties.md) | [Baking](baking.md) | [Validation](validation.md) | [Canonical JSON](../../../generated/map-format.json) | [Authoring guide](../../how-to/content/map-format.md)

Map containers are selected by `Baking.ProtoFileExtensions plus at least one [ProtoMap] anchor`; `.fomap` is a project convention, not an engine requirement. Each declared map emits `<MapId>.fomap-bin-server` and `<MapId>.fomap-bin-client` as a coupled resource pair.

The server payload contains placed critters and all items. The client payload contains visible static items; hidden static items contribute required string hashes but no client item record.

## Item ownership

| Ownership | Value | Map-supported | Reference/position | Meaning | Enum source |
| --- | --- | --- | --- | --- | --- |
| <a id="entry-map-format-ownership-map-hex-2dae405d94"></a><code>MapHex</code> | 0 | yes | <code>Hex</code> | Places the item on a map hex. Static items must use this mode. | [Source/Common/Common.h](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L1096) |
| <a id="entry-map-format-ownership-critter-inventory-08f10bd139"></a><code>CritterInventory</code> | 1 | yes | <code>CritterId</code> | Creates a non-static item in the inventory of the placed critter identified by CritterId. | [Source/Common/Common.h](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L1097) |
| <a id="entry-map-format-ownership-item-container-c580fe1c90"></a><code>ItemContainer</code> | 2 | yes | <code>ContainerId</code> | Creates a non-static item inside the placed non-static item identified by ContainerId. | [Source/Common/Common.h](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L1098) |
| <a id="entry-map-format-ownership-nowhere-a9c5a87d13"></a><code>Nowhere</code> | 3 | no | <code>none</code> | Exists in the enum but is not a supported authored map-placement mode. | [Source/Common/Common.h](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L1099) |

## Runtime split

- Static `MapHex` items become immutable grid entries and may block movement or shooting, expose triggers, and occupy multihex cells.
- Non-static `MapHex` items and placed critters are generated for each map instance; their authored ids are remapped to runtime ids.
- `CritterInventory` and `ItemContainer` records are attached after their direct owners are generated. A missing owner mapping skips the child.
- The client reconstructs only the visible static map layer from the client binary; dynamic entities arrive through normal runtime synchronization.
