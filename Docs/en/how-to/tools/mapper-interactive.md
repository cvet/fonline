---
layout: default
title: Mapper Interactive Manual
locale: en
document_id: mapper-interactive-manual
permalink: /Docs/en/how-to/tools/mapper-interactive.html
---

# Mapper Interactive Manual

> Engine-owned manual for the stock interactive Mapper. Project-specific map
> catalogs, prototypes, editor tabs, scripts, validation rules, and release
> acceptance belong in the embedding game.

Use this page when editing maps by hand. Use [Mapper Tools](mapper.md)
for mapper-side AngelScript, headless map processing, render capture, and
automation APIs. Use [Map Format](../content/map-format.md) for the serialized `.fomap`
contract and [Particle Authoring Tools](particle-authoring.md) for the
particle windows shown below.

## Build and launch

Enable the Mapper in the embedding project's configure preset and build both
resources and the application:

```bash
cmake --build Build/<preset> --config RelWithDebInfo --target BakeResources <DevName>_Mapper
```

Launch the generated Mapper with the project's main config. A startup map is
optional:

```bash
<output>/Binaries/Mapper-<platform>-<arch>/<DevName>_Mapper \
  -ApplyConfig <path-to-project.fomain> \
  -Mapper.StartMap <DeclaredMapName>
```

The executable consumes baked client resources for rendering and the raw input
directories declared by resource packs for map and source-asset editing. If a
prototype, image, effect, particle, or script changed, bake before judging the
editor. A successful executable launch does not prove that its resources are
current.

`Mapper.StartMap` names a declared `[ProtoMap]`, not necessarily a filename.
`Mapper.StartHexX` and `Mapper.StartHexY` can move the initial camera when both
are positive. Project launch tasks should own exact paths and subconfigs rather
than asking every author to reconstruct them.

## Screen orientation

<figure>
<img src="../../../assets/screenshots/mapper-particle-preview.png" alt="FOnline Mapper at 1280 by 800 showing the Workspace and Controls windows, the Particle Preview panel with Documentation.spk selected, deterministic seed and prewarm controls, and the live radiation particle centered on TutorialMap." loading="lazy">
<figcaption>Mapper capture from the minimal multiplayer example. Particle Preview selects the baked Documentation.spk resource, places it at the view center, and exposes deterministic scale, offset, seed, and prewarm controls beside the normal Workspace and Controls windows.</figcaption>
</figure>

The main viewport is the map. The menu bar and floating ImGui windows are tools
over that viewport; they are not serialized into the map. Window positions and
visibility are per-user tool settings. A restored layout may therefore differ
from the first launch. `Windows -> Settings -> Reset layout` returns tool
windows to their first-use positions without changing map data.

The normal work surfaces are:

| Surface | Purpose |
|---|---|
| **Map browser** | Filter every declared map, open it, and distinguish the current (`*`) and already loaded (`+`) entries. |
| **Controls** | Inspect the current map, mouse hex, time, FPS, tile layer, zoom, visibility, selection policy, roof preview, and critter direction. |
| **Workspace** | Filter and place item, tile, critter, and project-defined prototypes through tabs and subtabs. |
| **Content** | Inspect container contents, loaded maps, map creation/loading/saving, resize controls, and mapper messages. |
| **Inspector** | Edit the selected entity's typed properties, arrays, and structs; reset values to the prototype and optionally apply an edit to all selected peers. |
| **History** | Inspect and jump through the current map's undo/redo history. |
| **Console** | Run mapper commands and review command history. |

Map browser and Controls start visible in a fresh layout. Workspace is toggled
with `F7`; Content uses `Shift+F7`. The Inspector appears with `F9` when an
entity or container item is selected.

## Menu reference

### File

| Command | Behavior |
|---|---|
| **Save current** (`Ctrl+S`) | Serialize the current map through its resolved source container. |
| **Reset changes** | Reload the current map from its last saved source state. |
| **Exit** | Request normal Mapper shutdown. |

A dirty current map adds a visible `*** Save ***` button at the right edge of
the menu bar. Treat that marker as unsaved authored state. Save intentionally;
do not assume process exit commits it.

### Windows

The menu opens Workspace, Content, Console, Critter animations, Animation
viewer, Particle viewer, Script call, Map browser, Controls, History, particle
backend tools, and Settings. Backend entries are conditional:

- **Particle preview** appears when at least one particle runtime backend is
  enabled.
- **SPARK particle editor** appears only with `FO_SPARK_PARTICLES`.
- Effekseer authoring remains in the external pinned Effekseer editor; Mapper
  previews its baked `.efk` output.

The focused standalone viewers are documented in
[Animation and Particle Viewers](animation-particle-viewers.md).

### Edit

Undo and redo show the current operation label when one exists. Select all,
clear selection, delete, copy, cut, and paste operate on mapper entities and
participate in map history. Copy/paste uses the Mapper's in-process entity
buffer, not an interchange format or the operating-system clipboard.

### View

Visibility toggles cover Items, Scenery, Walls, Critters, Tiles, Roof, and
Fast. Changing one rebuilds the current map so the result is immediate.
`Axial grid selection` chooses the selection lattice. `Select entire entity`
controls whether selection expands from a visual component to its owning
entity.

Visibility is an authoring aid. It does not remove content or prove runtime
visibility, blocking, lighting, or ownership.

### Tools

| Command | Intended use |
|---|---|
| **Rebuild map** | Recreate current map presentation after relevant data or visibility changes. |
| **Mark blocked hexes** | Visualize blocked cells for authoring inspection. |
| **Reverse lights** | Run the mapper reverse-light command. |
| **Merge by command / Break by command** | Run project command handlers for item composition. |
| **Merge multihex items / Break multihex items** | Convert between compatible item sets and multihex meshes. |

Merge and break are structural changes. Review selection, ownership, offsets,
blocking, and undo history before saving.

### System

System toggles fullscreen (`F11`), minimizes (`F12`), dumps texture atlases,
and controls edge scrolling for the active window mode. Atlas dumps are
diagnostic output; they are not authored resources.

## Open and inspect a map

1. Bake the current project resources.
2. Launch the project-owned Mapper task or executable.
3. Open **Map browser**, filter by declared map name, and select the map.
4. Confirm the map name in **Controls**.
5. Inspect size, work hex, fixed/outside behavior, and authored sections
   against [Map Format](../content/map-format.md).
6. Toggle one content class at a time when a dense map is hard to read.
7. Open **Content** to confirm loaded-map state and source destination before
   editing.

The Mapper may keep multiple maps loaded while showing one current map.
Current, loaded, and saved are separate states. Closing a tab or changing the
current map does not silently make another map's dirty state authoritative.

## Place and edit entities

Workspace tabs are driven by loaded prototypes and project script
customization. The stock modes include Item, Tile, Critter, Fast, Ignore,
Inventory, Messages, Maps, and ten custom slots.

1. Open Workspace with `F7`.
2. Choose a tab and subtab.
3. Filter by prototype name when the collection is large.
4. Select a prototype preview to enter placement mode.
5. Left-click a valid map hex to place it.
6. Right-click or press `Escape` to leave placement mode.
7. Select the new entity and press `F9` to inspect instance properties.
8. Save only after validating direction, offsets, ownership, blocking, and
   project-required fields.

The Inspector parses values through the same property system used by mapper
automation. It supports scalar values, arrays, and registered structs. Invalid
text is not a partial edit. `PageUp` and `PageDown` move through property
rows; `Escape` first cancels the active property edit, then clears selection or
placement.

Use **Apply to all** only for a deliberately homogeneous selection. A property
with the same display name can still carry prototype-specific meaning in the
embedding project.

## Selection, movement, and clipboard

Left-click selects or places according to the current mode. Drag selection and
movement are committed as history entries when the interaction finishes.
Right-drag pans the map and preserves inertial motion; a right click without a
pan cancels placement or selection context. Arrow keys scroll.

Middle-click invokes the current context action: it can rotate selected
critters or the particle preview direction, and returns zoom to `1.0`.
Use Controls when direction or current zoom must be explicit.

Copy, cut, and paste preserve mapper entity data in an internal buffer.
After pasting:

- confirm the destination hex and offsets;
- inspect placement IDs and ownership;
- verify multihex and blocking behavior;
- check references that may not be valid outside the source map;
- review the resulting history entry before save.

For serialized placement identity and section ownership, use
[Map Format](../content/map-format.md).

## Keyboard reference

Hotkeys are suppressed while an ImGui text field is active.

| Key | Action |
|---|---|
| `F1` .. `F6` | Toggle Items, Scenery, Walls, Critters, Tiles, and Fast visibility. |
| `F7` / `Shift+F7` | Toggle Workspace / Content. |
| `F8` | Toggle edge scrolling for the current fullscreen/windowed mode. |
| `F9` | Open Inspector for selection, or clear selection when Inspector is visible. |
| `F10` | Toggle the mapper hex overlay. |
| `F11` / `F12` | Toggle fullscreen / minimize. |
| `Shift+F11` | Dump texture atlases. |
| `Shift+0` .. `Shift+4` | Choose tile layer 0 through 4. |
| `Tab` / `Shift+Tab` | Toggle axial-grid selection / whole-entity selection. |
| `Delete` | Delete selected entities. |
| `Escape` | Cancel property edit, clear selection, or leave placement mode. |
| Numpad `+` / `-` | Shift map time by plus/minus one hour when nothing is selected. |
| `Ctrl+Z` / `Ctrl+Y` | Undo / redo. |
| `Ctrl+A` | Select all. |
| `Ctrl+C` / `Ctrl+X` / `Ctrl+V` | Copy / cut / paste. |
| `Ctrl+S` | Save current map. |
| `Ctrl+D` | Toggle camera scroll checking for the current map. Keep it enabled during normal interactive work; disable it deliberately for overscan inspection. |
| `Ctrl+B` | Mark blocked hexes, equivalent to **Tools -> Mark blocked hexes**. |
| `~` | Toggle Console. |
| Arrow keys | Scroll the current map. |

## Undo, reset, and save discipline

History is per current map and bounded; it is not a source-control replacement.
Use it for local authoring operations, then review the serialized diff.

Before saving:

1. Confirm the intended map and source container.
2. Check the dirty marker and most recent History entries.
3. Inspect changed entities and their placement IDs.
4. Rebuild the map if visibility or composition changed.
5. Save with `Ctrl+S`.
6. Review the text diff.
7. Run project format/prototype/map validation and rebake.
8. Validate the map in a runtime scene, not only in Mapper.

**Reset changes** discards the current map's unsaved edits and restores the
source version. Source control remains the recovery path after a saved mistake.

## Settings and layout recovery

Settings lists the current resolution, fullscreen state, popular resolutions,
and **Reset layout**. Window layout is stored separately from baked resources:
the registry under `HKCU\Software\FOnline\Mapper` on Windows and the
per-application user-data store on other platforms.

Reset layout when windows are off-screen after a monitor or DPI change. Do not
delete resource caches or rebake merely to repair an ImGui layout.

## Particle and viewer windows

Use Particle Preview for map-context placement, deterministic seed/prewarm,
scale, offsets, restart, and removal. Use Particle Viewer for isolated
playback and viewport diagnostics. Use the SPARK editor only on authored
`.spark` sources. The complete choice and validation workflow is in
[Particle Authoring Tools](particle-authoring.md).

## Screenshot and automation contract

The mapper script API exposes one screenshot method:

| Method | Frame contents | Completion |
|---|---|---|
| `Game.SaveMapperScreenshot(path)` | Current map render target and mapper script interface drawing; excludes the later application-level ImGui composition. | Synchronous PNG write. |

There is no Engine script method for full-window UI capture. The minimal
multiplayer example provides a reproducible visible profile; after its windows
settle, capture the application window with a platform screenshot tool:

```powershell
cmake --build Build\windows --config Release --target ForceBakeResources FOMM_Mapper
Build\windows\Binaries\Mapper-Windows-win64\FOMM_Mapper.exe `
  -ApplyConfig FOnlineMinimalMultiplayer.fomain `
  -ApplySubConfig MapperDocumentationCapture
```

The profile opens `TutorialMap`, starts `Documentation.spk` with a fixed seed,
and fixes the viewport at `1280x800`. The checked-in PNG and complete source
hashes are recorded in
[generated/screenshots.json](../../../generated/screenshots.json).

Use `Render.HeadlessWindow = True` for off-screen map rendering, but not
`Render.NullRenderer`: a null renderer cannot produce a visual frame.

## Failure diagnosis

| Symptom | Check |
|---|---|
| Map is absent from Map browser | Prototype input roots, `Baking.ProtoFileExtensions`, declared `[ProtoMap]` name, and bake/config selection. |
| Prototype is absent from Workspace | Resource pack inclusion, prototype bake, collection name, and project tab scripts. |
| Map renders black | Whether the map is intentionally empty, current zoom/hex, missing tiles/images/effects, and renderer log. |
| Edit appears but cannot save | Source path resolution, read-only files, multi-map container ownership, and mapper messages. |
| Inspector rejects a value | Generated property type, enum spelling, array/struct syntax, nullability, and prototype constraints. |
| Particle source is listed but preview is not | Enabled backend, baked `.spk`/`.efk`, referenced effect/texture, bounds, seed/prewarm, and log exceptions. |
| Windows are missing or off-screen | Open Windows menu, then use Settings -> Reset layout. |
| UI screenshot contains only the map | `SaveMapperScreenshot` is map-only; capture the visible application window with a platform screenshot tool. |

Treat `ScriptException`, `VerificationException`, assertion/fatal lines,
missing resource logs, failed saves, and invalid map declarations as failures.
Do not publish a screenshot or map because the process merely returned zero.

## Project-owned completion gate

An embedding project should add:

- a documented launch task and representative map;
- project tab/filter conventions;
- property and placement rules;
- content and map validators;
- a clean bake;
- runtime scene acceptance on supported renderers;
- screenshot provenance when project docs show concrete assets;
- an update rule that reviews this manual and [Mapper Tools](mapper.md)
  whenever its Engine pin changes.

The interactive Mapper is an authoring surface. The serialized source, project
tests, and runtime behavior remain the authority.
