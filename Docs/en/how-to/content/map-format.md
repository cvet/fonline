---
layout: default
title: FOnline Map Format
document_id: map-format-guide
locale: en
permalink: /Docs/en/how-to/content/map-format.html
---

# FOnline Map Format

This guide defines the reusable engine contract for authored `.fomap` files. It covers source syntax, map and placement identity, property overrides, item ownership, mapper round-tripping, side-specific baking, and initial runtime materialization.

Use the generated [map format reference](../../reference/map-format/index.md) for an exhaustive, revision-pinned contract:

- [syntax and directives](../../reference/map-format/syntax.md);
- [Map, Critter, and Item properties](../../reference/map-format/properties.md);
- [ownership, baking, and runtime loading](../../reference/map-format/baking.md);
- [validation rules with stable IDs](../../reference/map-format/validation.md);
- [canonical JSON](../../../generated/map-format.json) for tools and AI agents.

The engine owns this grammar and its load/bake behavior. An embedding project owns its map catalog, custom metadata, prototype IDs, visual kits, encounters, quests, balance, and level-design policy.

## Placement decision

Give every authored Critter and Item placement an explicit positive `$Id`, and
keep those IDs unique across both Critter and Item sections in the whole map.
Every placement also needs `$Proto`; it selects the critter or item prototype
and is independent of placement identity.

Express ownership with explicit links. `CritterInventory` items carry the
owning placement in `CritterId`; `ItemContainer` items carry the parent item
placement in `ContainerId`. Never infer ownership from section order or nearby
coordinates.

## Minimal Map

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

A configured map container has one or more `[ProtoMap]` anchors. Placement sections address an anchor as `[$Name/Critter]` and `[$Name/Item]`, or use the anchor's explicit map ID in place of `$Name`. Data before the first anchor, an unknown address, and every other section form are rejected. In particular, the current loader rejects bare `[Critter]` and `[Item]` sections. Legacy projects may still contain those or the older `[Header]`, `[Tiles]`, and `[Objects]` forms, but they are not valid current-engine input.

The shared configuration parser provides `key = value`, repeated sections, `#` comments, backslash continuation, and `key += value` append syntax. Property types still determine which textual values and append operations are valid.

## Map Identity

Each `[ProtoMap]` starts a map definition. `$Name` selects its Map prototype ID, the placement-section address, and the basename of both baked resources:

```text
SmallRoom.fomap-bin-server
SmallRoom.fomap-bin-client
```

When a container has one unnamed anchor, the source filename without `.fomap` is used. A multi-map container must give every anchor a unique explicit `$Name`; its placements then use that name, for example `[VaultEntrance/Critter]`. Keep the source basename and `$Name` equal for ordinary one-map files. A mismatch is legal and useful for a deliberate canonical rename, but it makes targeted baking and source lookup less obvious.

`$Parent` uses ordinary Map prototype inheritance during prototype baking. It is not a mapper-safe source construct: mapper source loading does not resolve the parent chain, and mapper save emits its current full map property state without `$Parent`. Prefer explicit `[ProtoMap]` properties for maps that are edited in the mapper. If inheritance is required, validate both the baked runtime result and every mapper round-trip.

`$Text <language>` fields belong to prototype-text baking. The mapper explicitly retains these fields as extra `[ProtoMap]` data. Other unknown `$` directives are not preserved by mapper save.

## Placement Identity

Every addressed Critter and Item placement requires `$Proto`:

```ini
[$Name/Item]
$Id = 20
$Proto = Locker
Hex = 45 42
```

The prototype is resolved first. The remaining keys are applied as per-placement property overrides to a copy of that prototype's properties.

`$Id` is optional at the loader level. Missing, non-positive, and duplicate values are replaced with the next available positive ID. That repair exists to load imperfect content; it is a poor authoring contract because ownership references can silently point at a different entity after repair.

For production maps:

1. assign every placement an explicit positive `$Id`;
2. keep IDs unique across both Critter and Item sections, not only within one section type;
3. keep IDs stable across edits when another placement refers to them;
4. use an audit or bake gate to reject accidental duplicates before review.

Textual interleaving is not an execution-order guarantee. The loader processes every Critter section first and every Item section second. Mapper save also normalizes ordering: critters and their inventory items come before map items and their direct children.

## Property Overrides

Section properties use the receiver selected by the section:

| Section | Receiver | Typical engine properties |
| --- | --- | --- |
| `[ProtoMap]` | `Map` | `Size`, `WorkHex`, `DayTime`, `DayColor` |
| `[$Name/Critter]` or `[MapId/Critter]` | `Critter` | `Hex`, `Dir`, `Condition`, `InitScript` |
| `[$Name/Item]` or `[MapId/Item]` | `Item` | `Hex`, `Ownership`, `Static`, `Hidden`, `Count`, `PicMap` |

The generated [property catalog](../../reference/map-format/properties.md) is authoritative for built-in metadata at the pinned revision. It records type, runtime sides, flags, source, and whether text authoring is accepted. Project metadata can add more properties; document those in the project repository instead of extending this engine guide with one game's declarations.

Unknown, virtual, temporary, malformed, or invalid-for-the-current-side properties fail during property loading or validation. Side-specific properties can be skipped from the opposite output. Resource-valued properties are validated by `MapBaker`, so a syntactically valid map can still fail because an image, model, script, sound, or prototype dependency is absent.

## Item Ownership

`Ownership` determines where an authored item is materialized:

| Ownership | Reference or position | Supported map use |
| --- | --- | --- |
| `MapHex` | `Hex` | Static fixture or generated non-static map item |
| `CritterInventory` | `CritterId` | Direct inventory child of a placed critter |
| `ItemContainer` | `ContainerId` | Direct child of a placed non-static map item |
| `Nowhere` | none | Not supported for authored map placement |

Example inventory item:

```ini
[$Name/Critter]
$Id = 100
$Proto = Guard
Hex = 30 30

[$Name/Item]
$Id = 101
$Proto = Rifle
Ownership = CritterInventory
CritterId = 100
```

Example container and direct child:

```ini
[$Name/Item]
$Id = 200
$Proto = LootCrate
Hex = 35 30
Static = false

[$Name/Item]
$Id = 201
$Proto = Ammo
Ownership = ItemContainer
ContainerId = 200
```

The server creates critters and non-static map items first, records their authored-to-runtime ID mapping, then attaches child items. A child whose owner ID has no runtime mapping is skipped. Static items are not ordinary generated item owners, and child-of-child chains are not materialized by the current one-pass ID mapping. Keep authored containment one level deep and cover it with a runtime test when gameplay depends on it.

## Static And Dynamic Items

A static item must use `MapHex` ownership. During server map loading it becomes an immutable static-grid entry. Its `Hex`, multihex geometry, `NoBlock`, `ShootThru`, trigger flags, and static scripts contribute to map collision and interaction state.

A non-static `MapHex` item is a billet: the server creates a fresh runtime item for each map instance and remaps its authored ID. Placed critters follow the same per-instance generation model. Inventory and container children are created after those owners.

Choose deliberately:

- use static items for fixed map geometry, scenery, blockers, and triggers that do not need ordinary item lifecycle;
- use non-static items for loot, containers, movable or mutable objects, and ownership roots;
- do not mark an inventory or container child as static;
- do not rely on a static item's authored ID as an `ItemContainer` runtime owner.

## Server And Client Outputs

`MapBaker` produces a coupled pair:

- the server binary contains map string hashes, placed critters, and every item;
- the client binary contains string hashes and visible static item records;
- dynamic entities are synchronized through the normal runtime entity path, not embedded in the client static layer.

Hidden static items are omitted as client item records, but their client property strings are still collected into the hash dictionary. This lets server-only static logic retain identifiers needed by the client-side hash resolver without exposing a visible map entity.

Always regenerate and package both outputs after changing a map or a referenced prototype. Treat a one-sided stale result as invalid even when only one runtime role appears affected.

## Coordinates And Bounds

`ProtoMap.Size` defines valid map coordinates. Every placed critter and every `MapHex` item must have a `Hex` inside that size. Server loading rejects out-of-range positions.

Some mapper editing operations clamp moved entities and multihex coordinates back into bounds. That editor behavior is a convenience, not permission to commit invalid source. A project validator should parse and reject out-of-bounds authored coordinates before runtime packaging.

`MultihexMesh` is a sequence of x/y coordinate pairs. Mapper save normalizes the property to backslash-continued pairs:

```ini
MultihexMesh = \
 40 40 \
 41 40 \
 42 40
```

An odd number of values fails serialization. Mapper load may also coalesce eligible items into multihex meshes according to prototype `MultihexGeneration`; saving after that operation can substantially rewrite placement layout. Review those diffs as semantic map changes.

## Mapper Round-Trip

Mapper save is deterministic normalization for the selected map, not byte-preserving serialization of that map. It:

- writes `[ProtoMap]` first and records the selected map as `$Name`;
- serializes the mapper's current full Map property state;
- preserves `$Text*` extra fields;
- omits the `$Parent` control directive;
- emits placements as `[$Name/Critter]` and `[$Name/Item]`;
- emits explicit `$Id` and `$Proto` for placements;
- groups all critters before all map items;
- places direct inventory/container children immediately after their owner group;
- normalizes property order and `MultihexMesh` formatting;
- may merge items according to `MultihexGeneration` before save.

When the source container holds multiple maps, Mapper replaces only the selected map's section run and preserves every non-selected sibling map block byte-for-byte. That guarantee does not make the selected block byte-preserving: it is still normalized as described above.

Before using the mapper on hand-authored or generated maps, commit or otherwise preserve a reviewable source snapshot. After saving, inspect the complete selected-block diff, confirm the emitted `$Name`, verify that sibling map blocks are unchanged, and rebake both outputs.

For generators, prefer producing the canonical normalized form directly. Stable ordering and explicit IDs make later mapper diffs smaller and reduce ambiguity for humans and AI agents.

## Validation Workflow

For an embedding project, the production gate should include:

1. parse the configured container and require one or more `[ProtoMap]` anchors, with a unique explicit `$Name` on every anchor in a multi-map file;
2. accept only `[$Name/Critter]` / `[$Name/Item]` or equivalent explicit-map-ID placement addresses that resolve to a declared anchor;
3. require explicit positive IDs unique across Critter and Item placements within each map;
4. resolve every `$Proto`, ownership value, `CritterId`, and `ContainerId`;
5. validate receiver properties, side availability, resources, and map bounds;
6. bake both server and client map resources for every declared map;
7. load representative maps in the server and client or mapper;
8. round-trip multi-map containers and verify untouched sibling blocks byte-for-byte;
9. run project-specific checks for quests, spawns, collision, visual kits, scripts, and gameplay routes.

The engine unit tests cover parser strictness, ID repair, property application, canonical output naming, side payloads, hidden static items, mapper normalization, and server materialization. A project still needs content-backed validation because engine tests cannot know its metadata or map design rules.

## Best Practices

- Keep filename, `$Name`, and project catalog ID aligned for one-map files; use explicit unique names in multi-map containers.
- Put the first `[ProtoMap]` before content and use only addressed placement sections.
- Use explicit unique positive IDs even though the loader can repair them.
- Keep ownership shallow and references obvious in nearby sections.
- Use static items only for fixed `MapHex` fixtures.
- Keep map-level inheritance out of mapper-edited files unless its round-trip limitations are intentionally handled.
- Generate canonical formatting and review every mapper normalization diff.
- Validate source, both baked sides, and representative runtime loads in CI.
- Document project-specific map conventions next to project content, linking here for reusable engine mechanics.

## Source Authorities

The generated model tracks exact files and stable rule IDs. The primary implementation authorities are:

- `Source/Common/ConfigFile.cpp` for shared text syntax;
- `Source/Common/MapLoader.cpp` for section validation, placement IDs, and prototype resolution;
- `Source/Tools/ProtoBaker.cpp` and `Source/Tools/ProtoTextBaker.cpp` for `[ProtoMap]` prototype and text handling;
- `Source/Tools/MapBaker.cpp` for output identity, property application, validation, and side payloads;
- `Source/Tools/Mapper.cpp` and `Source/Client/MapView.cpp` for mapper load/save normalization;
- `Source/Server/MapManager.cpp` for static loading and per-instance materialization;
- `Source/Client/MapView.cpp` for client static-map loading.

When these files change, regenerate the [canonical model](../../../generated/map-format.json), compare its stable IDs with the previous revision, update this guide where behavior changed, and rerun the engine and embedding-project validation gates in the same change.
