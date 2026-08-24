---
layout: default
title: Mapper Tools
locale: en
document_id: mapper-tools
permalink: /Docs/en/how-to/tools/mapper.html
---

# Mapper Tools

> Engine-owned documentation for reusable mapper lifecycle, authoring, particle-preview, and off-screen capture APIs. Game-specific maps, marker prototypes, preview generators, UI consumers, and launch profiles belong in the embedding project.

The mapper is both an interactive editor and a scriptable map-processing host. Mapper-side AngelScript can create or load maps, author entities, control the editor view, capture a rendered frame, inspect atlas geometry, preview particles, and quit without connecting to a game server.

Use [Mapper Interactive Manual](mapper-interactive.md) for menus, windows, manual
editing, history, save discipline, and visible UI evidence. This page
owns the scriptable and native integration surface.

## Automation decision

Protect authored maps first: observe the dirty marker and History, use `Ctrl+S`
only for an intentional save, inspect the text diff, then validate the runtime
scene. A reproducible headless batch starts in `Game.OnStart`, advances bounded
warmup loops from `Game.OnLoop`, calls `Game.SaveMapperScreenshot`, and exits
through `Game.RequestQuit`.

Choose the capture route deliberately. `SaveMapperScreenshot` synchronously
writes the map render as TGA only; warm up a newly shown map before the call,
and expect non-uniform dimensions after resize or crop. It does not include the
application-level ImGui windows. Capture the visible application window with a
platform screenshot tool when review evidence must include Mapper UI.
The embedding project owns the batch plan, map selection, output naming,
conversion, retries, and validation of generated assets.

The dirty marker and History are interactive evidence; there is no
`Game.GetDirtyMap()` query in the current Mapper script API. Fixed camera and
overlay inputs make captures comparable, but do not promise byte-identical
pixels across renderer, driver, font, or asset revisions.

## Ownership boundary

The engine owns:

- `MapperEngine` map loading, display, processing, saving, and teardown;
- mapper-side `Game.*` script methods;
- the `Render.HeadlessWindow` off-screen host mode;
- camera, overlay, visibility, and scroll controls used by automation;
- TGA screenshot and atlas-diagnostic readback from mapper render targets;
- the backend-neutral particle preview and the SPARK source editor.

An embedding project owns:

- the mapper startup sub-config and script entry point;
- which maps and prototype markers are included or hidden;
- output naming, retries, cropping, conversion, metadata, and downstream UI;
- project settings used to pass a batch plan into its script;
- validation of its authored map containers and generated assets.

The engine page therefore documents reusable primitives and an integration pattern, not one game's preview pipeline.

## Map lifecycle API

Mapper-only methods are exported by `Source/Scripting/MapperGlobalScriptMethods.cpp`:

| Method | Purpose |
|---|---|
| `Game.NewMap(name, width, height)` | Create a blank map with a synthesized `[ProtoMap]` header and centered work hex. |
| `Game.NewMapFromText(name, text)` | Create a map from caller-authored `[ProtoMap]` header text. |
| `Game.LoadMap(mapName)` | Load a declared map by name from the configured prototype data sources. |
| `Game.ShowMap(map)` | Make a loaded map current and visible. |
| `Game.UnloadMap(map)` | Remove a loaded map from the mapper. |
| `Game.GetLoadedMaps(index)` | Enumerate loaded maps through the mapper's indexed API. |
| `Game.GetMapFileNames(dir, recursive)` | Enumerate declared map names below a map-data directory. |
| `Game.ResizeMap(width, height)` | Resize the current map. |
| `Game.SaveMap(map, customName)` | Save through the normal mapper path resolution. |
| `Game.SaveMapToPath(map, subDir, name)` | Save under `<MapsRoot>/<subDir>/<name>.<prototype-extension>`; rejects path separators in `name` and `..` traversal. |

`NewMapFromText` requires a `[ProtoMap]` section. Use it when automation must control fields such as `Size`, `WorkHex`, `ScrollAxialArea`, `Outside`, or `FixedTime` before entities are placed.

Map containers have no engine-mandated extension. Mapper scans files whose extensions appear in `Baking.ProtoFileExtensions`, enumerates every `[ProtoMap]` anchor, and addresses maps by declared name. One container may hold several maps with `[$Name/Item]` and `[$Name/Critter]` sections. Saving a map from a multi-map container preserves sibling map blocks.

`LoadMap` accepts either a declared map name or a directory-qualified
path/stem. A candidate file is accepted only when `MapLoader::EnumerateMaps`
finds the requested declaration (or the path's stem), so a same-stem sibling
such as `Area.foloc` cannot shadow `Area.fomap` merely because its extension
appears earlier in `Baking.ProtoFileExtensions`.

Prefer `SaveMapToPath` for generated or sandboxed authoring because its destination is explicit and constrained below the map data source. It inherits the extension of an existing map container. `SaveMap` is intended for normal mapper save behavior and can derive the directory from existing map state.

`MapperEngine::Shutdown()` unloads every map still present in `LoadedMaps` before chaining to `ClientEngine::Shutdown()`. This matters because `MapView` teardown requires the map's entities, items, and render targets to be released through `DestroySelf()`; closing the mapper with several tabs open must follow the same lifecycle as explicit `Game.UnloadMap` calls.

## Entity authoring API

| Method group | Purpose |
|---|---|
| `Game.AddItem`, `Game.AddCritter`, `Game.AddTile` | Place an entity or tile and return the live mapper view. |
| `Game.GetItemOnHex`, `Game.GetItemsOnHex` | Inspect items at a map hex. |
| `Game.GetCritterOnHex`, `Game.GetCrittersOnHex` | Inspect critters at a map hex with a `CritterFindType`. |
| `Game.MoveEntity` | Move a mapper entity to another hex. |
| `Game.DeleteEntity`, `Game.DeleteEntities` | Remove authored entities. |
| `Game.SelectEntity`, `Game.SelectEntities` | Change editor selection. |
| `Game.GetSelectedEntity`, `Game.GetSelectedEntities` | Read editor selection. |
| `Game.FindEntityById` | Resolve a loaded mapper entity by id. |
| `Game.SetEntityProperty` | Apply a property by name and text through the same parser/refresh path as the inspector. |

Placement returns a live view so scripts can immediately apply direction and per-instance fields. `SetEntityProperty` is the generic route when a tool does not have a generated typed accessor; it returns `false` when the property name/value cannot be applied.

Do not put authoritative gameplay policy in mapper automation. The mapper authors serialized inputs, while runtime authority and persistence remain server-owned; see [Server Runtime](../../explanation/runtime/server.md), [Entity Model](../../explanation/entity-and-property-model/), and [Persistence](../../explanation/persistence/).

## Particle tools

### Interactive particle preview

Open **Windows -> Particle preview** to inspect baked particle resources on the current map. The preview asks the registered particle sprite factory for supported extensions, so Mapper itself does not depend on SPARK or Effekseer. SPARK `.spark` and Effekseer `.efkproj` sources are baked to `.spk` and `.efk`; preview code loads only baked resources.

The catalog is refreshed when Mapper regains focus. Changed source files or dependencies invalidate the affected baked sprite and texture cache while preserving preview placement, seed, scale, offset, and prewarm controls. **Refresh** forces the same reindex and reload path. See [Particle Format And Runtime](../content/particle-format.md) for the source, baking, dependency, and runtime contracts.

**Mouse position** places an effect at the latest valid map cursor position. **View center** resolves the current viewport center when playback starts. **Play** creates a temporary `DrawOrderType::Particles` map sprite; **Restart** rebuilds it with the current controls, and **Remove** detaches it. The temporary sprite is not a serialized map entity and does not enter dirty tracking or undo history.

The optional `Mapper.ParticlePreviewEffect`, `Mapper.ParticlePreviewSeed`, `Mapper.ParticlePreviewScale`, and `Mapper.ParticlePreviewPrewarm` settings exercise the same path during startup. They are useful for reproducible smoke tests without making a project-specific launch profile part of the engine contract.

### SPARK source editor

Open **Windows -> SPARK particle editor** to browse raw `.spark` sources and open one graph/preview window per asset. Saving writes through the raw-resource filesystem, reindexes baked resources, and invalidates the saved asset's `.spk` sprite cache. Closing a modified window offers Save, Discard, and Cancel.

Set `Mapper.SparkEditorSource` to a raw asset path such as
`Documentation.spark` when a launch profile must open one editor
deterministically. Startup validates the path against indexed raw `.spark`
sources and fails clearly instead of silently showing an empty editor.

Effekseer authoring remains external. Build its editor through `BuildTools/buildtools.py build-auxiliary effekseer-editor <Config>`, edit the tracked `.efkproj`, then inspect the baked `.efk` through **Particle preview**.

## Focused viewers

### Animation viewer

Open **Windows -> Animation viewer** in Mapper, or run the standalone
`<DevName>_AnimationViewer` target created with `FO_BUILD_MAPPER`. The viewer
lists loaded critter prototypes and their available 2D/3D animation pairs,
plays one-shot clips before returning to idle, and exposes direction, scale,
root/render/view overlays, model layers, and the model hierarchy. It renders
through the same client sprite/model services as gameplay but has no map or
network session.

Use [Animation and Particle Viewers](animation-particle-viewers.md) for exact build/launch commands, controls,
model-layer and hierarchy behavior, persisted state, failure diagnosis,
evidence capture, and the required later client-scene check.

### Particle viewer

Open **Windows -> Particle viewer** in Mapper, or run the standalone
`<DevName>_ParticleViewer` target. This is a view-only, offscreen preview of
baked `.spk` and `.efk` resources reported by `ParticleSpriteFactory`. It uses
the normal `ParticleSystem` path and exposes seed, loop, prewarm, direction,
zoom/pan, root, draw-frame, and wireframe controls. Direct-scene particles are
shown through their authored route, including a background snapshot when the
effect requests scene distortion.

Use the interactive particle preview above when placement and map depth matter.
Use the focused viewer for fast geometry/frame inspection. Neither replaces a
representative client scene for attachment lifetime, gameplay timing, or
performance acceptance.

Use [Animation and Particle Viewers](animation-particle-viewers.md) for the complete focused particle review
workflow and the exact boundary between baked-resource inspection, Mapper
placement, and gameplay validation.

## View and capture API

| Method | Purpose |
|---|---|
| `Game.GetCurMapHexSize()` | Return the current map's hex dimensions. |
| `Game.GetCurMapPixelSize()` | Return full map pixel bounds. |
| `Game.SetMapperViewSize(size)` | Resize the mapper render view. |
| `Game.CenterMapperOnPlayableArea()` | Center using the current map's playable-area rules. |
| `Game.CenterMapperOnHex(hex)` | Center on a validated map hex. |
| `Game.CenterMapperOnRawHex(rawHex)` | Center on raw coordinates, including a point outside the authored rectangle. |
| `Game.SetMapperZoom(zoom)` | Apply camera zoom immediately. |
| `Game.CalcMapperFitZoom(viewport)` | Calculate the zoom needed to fit the playable area in a viewport. |
| `Game.SetMapperOverlayVisible(visible)` | Toggle mapper-only track and scroll-border overlays together. |
| `Game.SetMapperHexOverlayVisible(visible)` | Toggle the hex grid overlay. |
| `Game.SetMapperHiddenSpritesVisible(visible)` | Include or suppress sprites marked as hidden during normal client rendering. |
| `Game.AddMapperIgnoredItemPids(pids)` | Add item prototype ids to the current map's mapper ignore list and rebuild it. |
| `Game.SetMapperScrollCheckEnabled(enabled)` | Enable or disable camera clamping to authored scroll bounds. |
| `Game.SaveMapperScreenshot(path)` | Redraw and synchronously save the map render target as TGA; application-level ImGui windows are not included. |
| `Game.DumpAtlases()` | Save diagnostic TGA copies of live texture atlases with allocation and sprite-mesh overlays. |

`CalcMapperFitZoom` uses `ScrollAxialArea` when present and falls back to map bounds. A batch tool can apply an additional project-owned padding factor when tall sprites, shadows, or effects extend beyond the playable area.

`CenterMapperOnRawHex` and disabled scroll checking are useful when a project deliberately captures an overscan frame. Interactive mapper tools should normally retain scroll checking so the camera respects authored bounds. In the stock UI, `Ctrl+D` toggles the current map's scroll checking and `Ctrl+B` marks blocked hexes.

The mapper's interactive **Dump atlases** command and `Game.DumpAtlases()` share the same non-destructive diagnostic path. The read-back copy marks polygon edges and vertices, implicit quads, and explicitly empty frames without modifying the runtime atlas texture; see [Frontend and Rendering](../../explanation/rendering/) for the exact colors and lifetime rules.

## Current limitations: known scope

Mapper mode deliberately freezes every animated map item at time `0.0`.
`ItemHexView::RefreshAnim()` stops the loaded sprite and selects its
first frame instead of starting normal playback. Interactive Mapper views and
headless screenshots therefore remain deterministic for doors, containers,
and other multi-frame map items.

This is an authoring and capture rule, not a runtime presentation guarantee.
Validate animation timing, transitions, and final visual state in a client
scene. Critter animation inspection is a separate interactive Mapper tool and
is not frozen by this map-item rule.

## Headless capture integration

`Render.HeadlessWindow=True` creates the render host window hidden while preserving the graphics path required for off-screen drawing. It does not define a batch protocol; the embedding project's mapper script owns that orchestration.

A reusable batch driver follows this sequence:

1. Subscribe mapper-side functions to `Game.OnStart` and `Game.OnLoop`.
2. Read a project-owned batch description from config or a data file.
3. Load and show one map.
4. Configure view size, overlays, ignored prototype ids, hidden-sprite visibility, scroll checking, center, and zoom.
5. Wait enough loop iterations for map processing and drawing to settle.
6. Optionally dump atlas diagnostics, then call `Game.SaveMapperScreenshot` with an output TGA path.
7. Unload the map and continue with the next batch item.
8. Call the common `Game.RequestQuit()` after the batch completes.

`Game.OnLoop` fires before `MapperEngine::DrawMapperFrame()`. `SaveMapperScreenshot` performs an explicit draw before reading the main render target, but a newly shown map can still require warmup loops for resource/view state to settle. The number of warmup loops is a project policy and should be tested against the project's heaviest map assets.

Single-process batching is preferred when many maps share one resource set because mapper startup, script loading, and graphics initialization are paid once.

## Screenshot contract

`SaveMapperScreenshot` is the synchronous map-only path:

1. rejects an empty output path;
2. requires a current map;
3. calls `DrawMapperFrame()` to refresh the intermediate main render target;
4. reads RGBA pixels from that target;
5. flips rows and swaps red/blue channels for TGA ordering;
6. writes through the engine-shared `ImageWriter::WriteSimpleTga` helper.

It captures mapper script-interface drawing that is already in the map target,
but not the later application-level ImGui menu and tool windows.

This Engine script API has no full-window capture method. Use the synchronous
method for deterministic batch map frames. For manuals, bug reports, or UI
regression evidence, launch a visible reproducible profile and capture the
application window with a platform screenshot tool. [Mapper Interactive
Manual](mapper-interactive.md#screenshot-and-automation-contract) contains the
minimal-example recipe.

The script method produces TGA only. The Engine-owned documentation screenshot
pipeline may record an external visible-window capture and pins the exact
source, dimensions, image hash, environment, and recapture triggers in
`BuildTools/DocumentationScreenshots.json`. Project screenshot conversion,
cropping, size limits, alpha-bound analysis, and asset registration remain
embedding-project responsibilities unless a reusable helper is deliberately
promoted.

If a rendering backend does not expose a readable main render target, the method throws instead of producing an empty image. Backend support claims should be validated with a non-uniform test map and pixel inspection, not only by checking that a file exists.

## Interactive tab and overlay helpers

The same export file also exposes mapper tab management (`TabGet*Pids`, `TabSet*Pids`, `TabDelete`, `TabSelect`, `TabSetName`), editor messages, selection, and overlay inspection. These are useful for custom mapper UI scripts but are independent of headless capture.

Use [Script Methods Map](../../reference/script-api/method-ownership.md) as the current routing index for the complete mapper-side binding surface. Generated API reference will eventually replace manually maintained method inventories.

## Source paths inspected

- `Source/Tools/Mapper.cpp`
- `Source/Tools/ParticleEditor.h`
- `Source/Tools/ParticleEditor.cpp`
- `Source/Tools/SparkParticleEditor.h`
- `Source/Tools/SparkParticleEditor.cpp`
- `Source/Scripting/MapperGlobalScriptMethods.cpp`
- `Source/Scripting/ClientGlobalScriptMethods.cpp`
- `Source/Scripting/CommonGlobalScriptMethods.cpp`
- `Source/Common/Settings.inc`
- `Source/Common/Geometry.cpp`
- `Source/Client/MapView.cpp`
- `Source/Client/TextureAtlas.cpp`
- `Source/Client/ParticleSprites.h`
- `Source/Client/VisualParticles.h`

## Validation checklist

1. Build an embedding project's mapper target after changing mapper exports.
2. Compile a minimal mapper script that loads, shows, captures, unloads, and quits.
3. Run one visible interactive mapper smoke test to confirm editor behavior is unchanged.
4. Run one `Render.HeadlessWindow=True` capture and verify dimensions plus non-uniform pixels.
5. Capture one visible application window with a platform screenshot tool and
   verify that the intended menu/tool windows are present.
6. Preview one baked effect for each enabled particle backend; for SPARK editor changes, save and reload one `.spark` source.
7. When atlas diagnostics change, dump one warmed atlas and inspect mesh, quad, and empty-frame markers.
8. Confirm `SaveMapToPath` rejects traversal and writes only below the map data-source root.
9. Keep project-specific batch settings, prototype ids, output formats, and downstream tooling out of this page.

## See also

- [Tools.md](../../../Tools.md) for mapper/editor application ownership.
- [Mapper Interactive Manual](mapper-interactive.md) for the stock human workflow.
- [Particle Authoring Tools](particle-authoring.md) for Particle Preview
  and source-editor operation.
- [Animation and Particle Viewers](animation-particle-viewers.md) for AnimationViewer and ParticleViewer.
- [Map Format](../content/map-format.md) for map-container syntax, placement ownership, mapper round-trip behavior, and map baking.
- [Particle Format And Runtime](../content/particle-format.md) for particle source, baking, and runtime behavior.
- [Maps and Movement](../../explanation/maps-and-movement.md) for map coordinates and playable-area geometry.
- [Frontend and Rendering](../../explanation/rendering/) for render-target, atlas, sprite-mesh, and frontend ownership.
- [Scripting](../../explanation/scripting-runtime/) for mapper-side script registration and events.
