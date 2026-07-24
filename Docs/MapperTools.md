# Mapper Tools

> Engine-owned documentation. Paths under `../` are relative to the FOnline engine root. Paths under `../../` point to an embedding game project such as Last Frontier when this engine is used as a submodule.

This page documents mapper-specific engine behavior and known mapper automation workflows. For the broader tool map, see [Tools.md](Tools.md); for native mapper script methods, see [ScriptMethodsMap.md](ScriptMethodsMap.md).

## Source paths inspected

- `Source/Applications/MapperApp.cpp`
- `Source/Tools/Mapper.h`
- `Source/Tools/Mapper.cpp`
- `Source/Tools/ParticleEditor.h`
- `Source/Tools/ParticleEditor.cpp`
- `Source/Tools/SparkParticleEditor.h`
- `Source/Tools/SparkParticleEditor.cpp`
- `Source/Scripting/MapperGlobalScriptMethods.cpp`
- `Source/Scripting/CommonGlobalScriptMethods.cpp`
- `Source/Client/MapView.h`
- `Source/Client/MapView.cpp`
- `Source/Client/ParticleSprites.h`
- `Source/Client/VisualParticles.h`
- `Source/Common/Geometry.cpp`
- `../../Scripts/MapperRender.fos`
- `../../Tools/MapPreview/generate_map_preview.py`
- `../../Tools/MapPreview/map_preview_overrides.ini`
- `../../LastFrontier.fomain`

## Mapper lifecycle and source ownership

`Source/Applications/MapperApp.cpp` is the application entry point. It initializes the application/frontend layer, waits for persistent data readiness on Web builds, constructs `MapperEngine`, locks input when `HeadlessWindow` is active, calls `MapperMainLoop()` every frame, and calls `Shutdown()` before exit.

`Source/Tools/Mapper.h` / `Source/Tools/Mapper.cpp` own the reusable mapper runtime. `MapperEngine` is client-like: it uses the client/view/rendering side of the engine, registers mapper metadata, creates sprite factories, processes input, draws editor UI, and loads/saves maps without connecting to a game server.

Important source areas:

- `MapperEngine::MapperMainLoop()` — per-frame mapper execution.
- `MapperEngine::DrawMapperFrame()` — map/editor frame drawing.
- `MapperEngine::ProcessMapperInputEvent()` and cursor helpers — input-to-map interactions.
- `DrawMainPanelImGui()`, `DrawWorkspaceWindowImGui()`, `DrawContentWindowImGui()`, `DrawMapListWindowImGui()`, `DrawMapWindowImGui()`, `DrawInspectorImGui()`, `DrawHistoryWindowImGui()`, `DrawSettingsWindowImGui()`, and `DrawConsoleImGui()` — major ImGui UI panels.
- `LoadMapFromText()`, `LoadMap()`, `ShowMap()`, `SaveCurrentMap()`, and `SaveMap()` — map file lifecycle. Maps are addressed by their declared id, not by file, and map files carry no dedicated engine extension: any file with a `Baking.ProtoFileExtensions` extension that declares `[ProtoMap]` anchors is a map container (`MapLoader::EnumerateMaps` doubles as the detector; the `.fomap` extension is an embedding-project convention). A container may declare several maps (`[ProtoMap]` anchors + `[$Name/Item]`/`[$Name/Critter]` nested sections), `LoadMap` scans anchors to locate the owning file, the map browser and `Game.GetMapFileNames` enumerate declared maps, saving a map from a multi-map file preserves its sibling maps byte-exact, and brand-new map files inherit the extension of the container they land beside.
- `UnloadMap()` and `Shutdown()` — map teardown. `MapView`'s destructor enforces empty-state invariants (entity/item lists cleared, render targets released) that only `MapView::DestroySelf()` (invoked via `UnloadMap()`) satisfies. `MapperEngine` overrides `ClientEngine::Shutdown()` (called by `MapperApp` before the engine is destroyed) to unload every still-open map in `LoadedMaps` — `ClientEngine::Shutdown()` alone only cleans the single `_curMap` — and then chains to `ClientEngine::Shutdown()` for the rest of the client teardown (events, network, render target, location/player). Quitting with maps open therefore neither trips the `MapView` invariants nor skips base shutdown.
- `Source/Scripting/MapperGlobalScriptMethods.cpp` — mapper-side native script helpers exposed through `Mapper_Game_*` methods.

## Extension points and boundaries

Use mapper script methods for editor automation that acts on mapper-owned state: adding/deleting/moving/selecting entities, adding tiles, loading/unloading/saving/showing maps, resizing maps, and managing mapper tabs. The current method group is mapped in [ScriptMethodsMap.md](ScriptMethodsMap.md).

### Programmatic / AI authoring exports

For building maps from scratch (e.g. an AI authoring pipeline; see the embedding project's `Docs/MapAuthoring.md`), these `Mapper_Game_*` methods complete the authoring loop:

| Method | Purpose |
|--------|---------|
| `Game.NewMap(name, width, height)` â†’ `MapView*` | Create a blank map from a synthesized `[ProtoMap]` header (Size + centered WorkHex). The only create-blank-map entry point; wraps the internal `MapperEngine::LoadMapFromText`. |
| `Game.NewMapFromText(name, text)` â†’ `MapView*` | Create a map from caller-authored `[ProtoMap]` header text (full control of Size/WorkHex/ScrollAxialArea/Outside/FixedTime/â€¦). Pre-checked for a `[ProtoMap]` section. |
| `Game.SetEntityProperty(entity, propName, valueText)` â†’ `bool` | Set any per-instance property by name/text on a placed item/critter (Dir, MapEntry.*, SpawnData.*, Light*, Locker.*, Offset, InitScript, ContainerId, Ownership, â€¦). Routes through the inspector apply path (`ApplyEntityPropertyText`), so multihex `SameSibling` siblings and offset/anim refresh are handled. Returns false if the text cannot be parsed for the property type. |
| `Game.SaveMapToPath(map, subDir, name)` | Save `map` into `<MapsRoot>/<subDir>/<name>.fomap` (resolved from the Maps data-source disk root). Refuses path separators in `name` and any `..` traversal. Use this for a sandboxed authoring area (e.g. `Generated`) instead of `SaveMap`, whose fall-back places brand-new names next to an arbitrary existing map. |

Placement (`AddItem`/`AddCritter`) returns the live view; set direction and other per-instance fields with `SetEntityProperty` (or the view's typed property accessors) after the call. These are mapper-only editor methods (no server connection, no network message, no serialized-contract change), so they do not affect client/server compatibility.

Do not put authoritative gameplay policy in mapper helpers. The mapper can inspect and author map content, but server runtime rules, persistence authority, and gameplay validation remain server-side; see [ServerRuntime.md](ServerRuntime.md) and [EntityModel.md](EntityModel.md).

Do not document one game's mapper binary name or content pipeline as universal engine behavior. The sections below include Last Frontier examples because this repository is currently embedded there; reusable mechanics are the `MapperEngine` lifecycle, mapper script helper surface, and headless render capability.

## Animation Viewer

`Source/Tools/AnimationViewer.{h,cpp}` is an ImGui window for reviewing critters and their animations without launching the game: pick a prototype, see it rendered at its real in-game size, and click any animation the critter actually has to play it. Open it from the mapper's `View` menu (`Animation viewer`); it starts hidden.

Layout: prototype list with a text filter on the left, preview in the middle, animation list on the right. The preview draws into its own offscreen `RenderTexture` (the pattern `ParticleEditor` uses, including the `IsRenderTargetFlipped()` orientation handling), so it is independent of the map view. On selection the model is **prewarmed**: idle is played with `ModelAnimFlags::NoSmooth` so it settles at once (the first frame is the idle pose, not a cross-fade from the bind pose), and `Sprite::Prewarm()` warms the particle systems so any effects are already emitting. A non-looped clip falls back to idle when it ends (with a normal blend), so the window never sits on a frozen last frame.

Review controls: facing is an **angle** (degrees), the same currency the engine uses — a 2D sprite takes its hex direction from the angle (`mdir::hex()`) and a 3D model rotates to it (`mdir(angle)`, applied snapped so a review sees the pose at once). The `Angle` slider sets it directly, and **holding the left mouse button and dragging horizontally over the preview turns the critter left/right** (`DRAG_DEG_PER_PX` per pixel); the preview image is an `InvisibleButton` so it captures the drag and the zoom wheel. **Holding the right mouse button and dragging pans** the view (a camera offset applied to the anchor, moving the model, crosshair, and overlays together); the pan resets when a different critter is selected. The default 210° is hex direction 3 (`dir*60+30`), the front-facing review angle. Because the model rotates continuously while a sprite snaps to the nearest hex frame, the same drag reads as smooth turning for models and stepped turning for 2D critters. `Zoom` scales the review (so `1.00x` is exactly what the map shows) and also responds to the mouse wheel while the pointer is over the preview. In the default **sprite** path the model is rendered at native scale into the atlas and that baked frame is then magnified, so zoom shows honest sprite pixelation — exactly what the game's cached sprite looks like blown up. To inspect the model without that quantization, tick **Direct draw** (below), which renders the real geometry at the zoom.

The preview pins the critter **root** — the hex ground point the game stands critters on — to a fixed anchor (horizontal centre, two-thirds down). That point inside a sprite is bottom-centre adjusted by the sprite offset, `(size.width/2 - offset.x, size.height - offset.y)`, exactly the value `MapSprite::GetSpriteRootOffset()` subtracts from the hex screen position when drawing the map (do **not** confuse it with `Sprite::GetOffset()` itself, which is near the sprite top and marks the head, not the feet). The preview places the sprite so that point lands on the anchor (`pos = anchor - root_in_sprite*zoom`). Anchoring by the root instead of centering the per-clip bounds keeps the feet still from clip to clip. A faint crosshair is drawn through the anchor into the render target before the model (`SpriteManager::DrawPoints`, `LineList`), so it reads as a ground reference behind the creature and marks where the root is.

Debug-draw toggles (under the `Angle`/`Zoom` row) overlay a model's authored anchor geometry so a bad rig is visible at a glance:

- **Direct draw** (off by default) — renders a 3D model as real geometry straight into the scene depth buffer (`Sprite::DrawInScene`), instead of through the cached atlas sprite. The model is scaled to the zoom (`ModelInstance::SetScale`) and drawn 1:1, so it stays crisp at any magnification; the ortho depth range is widened for the draw so a scaled-up model is not near/far clipped. Off = the atlas-sprite path (native render magnified — pixelated at high zoom, exactly what the game uses by default, `Render.ModelDirectDraw = false`). 2D critters always use the atlas path.
- **Root** — the ground crosshair described above.
- **Name level** — a full-width horizontal line at the height where the critter name would sit: the top of the view rect shifted by the global `Settings->NameOffset` **plus the prototype's own `NameOffset` property** (`proto->GetNameOffset()`), matching `CritterHexView::GetNameTextPos` (`Settings->NameOffset + GetNameOffset()`).
- **Draw rect** — the whole frame the model rasterizes into (the maximal drawing area): for a 3D model `ModelInstance::GetDrawSize()`, placed so the model origin sits at its exact pixel inside the frame (`ModelInstance::GetFramePivot()`). The frame is the **tight** projected extent of the model's *current animation* (the active clip's baked bounds), with the origin anchored at its real projected position — there is no fixed "root at three-quarters" fraction and no power-of-two padding, so a low or centre-origin creature (a crab) no longer reserves a tall empty frame. It is per-animation and direction-independent (the layout excludes the facing rotation); if a rotated pose or an emitting particle projects past the frame, the render's frame-expansion pass grows it. The overlay box is positioned from the same pivot, so it always matches where the model actually rasterizes.
- **View rect** — the visual/logical rect from `Sprite::GetViewSize()`, sourced from the model's stable **idle-pose** bounds (`_viewBounds`), not the current animation. It stays put across the animation set for a given configuration (so an extreme frame such as a strike can extend past it), which is why name/UI text anchors to it. Positioned bottom-centred as `MapSprite::GetViewRect` does.

All overlays are drawn on top of the model (except the root crosshair, which stays behind it) as `LineList` primitives, and rects are scaled by the review zoom while marker crosses keep a fixed screen size.

Below the animation list is a **Model hierarchy** sub-window (3D models only): the model's bone tree from `ModelInformation::GetRootBone()`, each node with a checkbox. Ticking a bone marks its position in the preview with a coloured cross; the tree label and the cross share a colour hashed from the bone name so several enabled bones stay distinguishable. A bone's sprite position comes from `ModelInstance::GetBoneSpritePos()`, which projects the bone's world matrix through the model's frame projection and converts it into the cropped sprite's pixel space (matching `GetSpriteBounds`' bottom-up Y convention), so the marker lands on the bone regardless of clip or zoom. Selecting a different critter clears the enabled set.

`SetFillViewport(true)` makes the window occupy the whole viewport without a title bar or move/resize handles — used by the standalone application, where the window is the entire program.

Prototypes are dressed the way the game dresses them: `Render.ModelLayerProperties` declares `<PropertyName>=<LayerIndex>` pairs, and the viewer reads those properties off the prototype to build the model layer array passed to `PlayAnim`. Which property feeds which layer is game-specific (armor, clothing, hair slots), so it lives in settings rather than engine code; without the setting a critter renders in its bare default layers. A mapped property that is disabled on the host's engine side (e.g. a server-only property under a `ClientSide` engine) is skipped rather than read, so the tool stays generic — it never hard-codes a game property name and never throws on a scope mismatch. This dresses the model with the prototype's own layer values; game-specific dressing beyond that (equipped/starting items resolved through project scripts) is intentionally left to the game, not the engine tool.

Both critter families go through the same `Sprite` surface:

- **3D** (`.fo3d`): the animation list is the model description's authored clip table, read through `ModelInformation::GetAvailableAnimations()` (reachable from an instance via `ModelInstance::GetInformation()`). No probing, so the list cannot contain entries the model does not have.
- **2D** (sprite sheets): there is no authored clip table, so the viewer probes `ResourceManager::GetCritterAnimFrames(...)` across the declared enum ranges; a resolvable frame set is the evidence that the animation exists.

Animation rows are labelled with resolved enum names (`Unarmed / Idle`); a value the enum does not name falls back to its raw number rather than being hidden, so authored content using unnamed values still shows up.

The window owns no engine services — a host passes `BaseEngine`, `SpriteManager`, `ResourceManager`, and `GameTimer` and calls `Draw()` from its ImGui pass — so any ImGui-capable host can embed it.

### Library boundary

The viewer ships as its own static library, `AnimationViewerLib`, linking only `ClientLib` + `CommonLib`. It has **no mapper dependency**: everything it touches (prototypes, sprite manager, resource manager, model instances) is client-side. `MapperLib` links it to offer the `View` → `Animation viewer` entry, and the standalone application links it directly; neither host is privileged.

### Standalone application

`Source/Applications/AnimationViewerApp.cpp` builds `<DevName>_AnimationViewer`, which boots straight into the viewer with no map, no mapper UI, and no server connection. It owns a plain `ClientEngine` — the client constructor already builds every service the viewer needs (effects, sprite manager with the model/particle/default factories, fonts, indexed resources, prototypes) — and drives its own minimal frame (`FrameAdvance` → `UpdateEffects` → `FontMngr.FrameUpdate` → `BeginScene` → viewer draw → `EndScene`) rather than the client's networked main loop. The window opens on start, since it is the whole application.

Note that constructing a `ClientEngine` also initializes AngelScript and fires the embedding game's client start events, so project client scripts run in this process. They have no window of their own here (only the viewer is drawn), but the startup cost and any side effects of those scripts are present. Skipping them would require an engine-level opt-out, which is deliberately not introduced for a dev tool.

The executable is built alongside the mapper under `FO_BUILD_MAPPER` and lands in its own `AnimationViewer` binary directory.

## Particle Viewer

`Source/Tools/ParticleViewer.{h,cpp}` is a **view-only** ImGui window for previewing particle effects without launching the game, mirroring the Animation Viewer. Authoring stays in the mapper's `SparkParticleEditor` (`Windows → Particle preview`); this window never edits — it only plays.

Layout: particle-resource list with a text filter on the left, preview on the right. The list is every baked runtime resource the particle sprite factory advertises — `ParticleSpriteFactory::GetExtensions()` (`spk` for Spark, `efk` for Effekseer) enumerated through `Resources.FilterFiles(ext)` — so it reflects the loaded content exactly. Selecting one loads it as a `Sprite` through the ordinary `SpriteManager::LoadSprite(path, AtlasType::MapSprites)` path (which routes `.spk`/`.efk` to `ParticleSpriteFactory`, producing a `ParticleSprite`), then plays it.

A particle does not auto-play on load. The viewer starts it with `ParticleSprite::PlayWithSeed(seed)` (which `Respawn(seed)`s the system and begins emission) and, when **Prewarm** is on, warms it so the window opens mid-effect rather than on a cold start. Each frame it steps `Sprite::Update()`; with **Loop** on it restarts a finite burst once it finishes (`IsPlaying()` goes false) so a one-shot effect keeps replaying. The seed is the per-system random seed: a fixed `Seed` replays a burst identically, **New seed** reseeds and restarts, and **Replay** restarts with the current seed.

The preview draws into its own offscreen `RenderTexture` (the Animation Viewer pattern, including `IsRenderTargetFlipped()` handling) and pins the effect **root** — bottom-centre adjusted by the sprite offset, `(size.width/2 - offset.x, size.height - offset.y)`, the same value `MapSprite::GetSpriteRootOffset()` uses — to a fixed anchor, so it stays put while the effect evolves. `Zoom` (slider + mouse wheel over the preview) magnifies the atlas frame; **holding the right mouse button and dragging pans** (the pan moves effect, crosshair, and overlays together, and resets on a new selection). Atlas-mode effects render through `Update()`→`DrawToAtlas()` then `DrawSpriteSize`; a draw-in-scene effect (`Sprite::IsDirectDraw()`) is drawn with `Sprite::DrawInScene` under a widened ortho depth range instead.

Debug-draw toggles (all off by default):

- **Root** — a faint crosshair through the anchor, drawn into the target before the effect so it reads as a ground reference.
- **Draw rect** — the whole sprite frame the effect rasterizes into (`Sprite::GetSize()`), its maximal draw area.
- **View rect** — the visual rect from `Sprite::GetViewSize()` when the resource carries one; a particle carries none by default, so this is simply skipped for it, but the toggle stays for resources that do.

The window owns no engine services — a host passes `BaseEngine` and `SpriteManager` and calls `Draw()` from its ImGui pass. `SetFillViewport(true)` makes it occupy the whole viewport (used by the standalone application).

### Library boundary

The viewer ships as its own static library, `ParticleViewerLib`, linking only `ClientLib` + `CommonLib` — no mapper dependency; everything it touches (sprite manager, particle factory, resource file system) is client-side. The standalone application links it directly.

### Standalone application

`Source/Applications/ParticleViewerApp.cpp` builds `<DevName>_ParticleViewer`, which boots straight into the viewer with no map, no mapper UI, and no server connection. It owns a plain `ClientEngine` (whose constructor already registers the particle sprite factory and indexes resources) and drives its own minimal frame, exactly like the Animation Viewer app; the same `ClientEngine` startup note applies (project client scripts run in-process). It is built alongside the mapper under `FO_BUILD_MAPPER` and lands in its own `ParticleViewer` binary directory. VS Code tasks: `Build :: LF Particle Viewer` / `Launch Particle Viewer [windows]`.

## Existing project workflows

### Interactive particle preview

Open **Windows -> Particle preview** in the interactive mapper to inspect baked particle resources on the current map. The preview asks the registered particle sprite factory for its supported extensions, so Mapper itself does not know whether SPARK, Effekseer, or no particle runtime is selected. Mapper caches the effect catalog and resource metadata while it has focus. SPARK `.spark` and Effekseer `.efkproj` sources are baked to `.spk` and `.efk`; the preview never loads authoring files directly.

When native Mapper regains focus, the particle subeditor reindexes its resource data sources (including `BakerDataSource`) once and compares path, size, and source write time under every effect directory. The tracked Effekseer XML is compiled to its `.efk` output by the native baking source; the preview never loads the XML directly. Incremental compilation consults the owning effect's dependency snapshot under `BakeOutput/.baker-cache`, comparing the project and each compiler-reported dependency by physical path, size, and write time. A changed, deleted, or renamed dependency recompiles only effects that reference it. Added and removed effects update the sorted catalog; a modified active effect or dependency invalidates its sprite/texture cache and recreates the preview with the same placement, seed, scale, offset, and prewarm setting. If the reindex reports no source changes, catalog and preview state are untouched. **Refresh** forces the same invalidation/reload path as a manual fallback. The window does not expose or edit raw particle project files. Web builds consume host-prebaked `.efk`.

### SPARK particle editor

Open **Windows -> SPARK particle editor** to browse raw `.spark` sources and open one
SPARK graph/preview window per asset. The editor uses Mapper's raw resource
filesystem for source discovery and saving, and Mapper's baked resources for
effects and textures. Saving reindexes the baked resource source and invalidates
the saved asset's `.spk` sprite cache. Closing a modified window opens a Save /
Discard / Cancel confirmation. Effekseer authoring remains external: edit the
tracked `.efkproj`, then inspect the baked `.efk` through the backend-neutral
**Particle preview**. Authored `.spk`/`.efk` resource inputs are rejected.

Mapper is the engine's central interactive editing application. The former
generic Editor executable, `EditorLib`, and asset-explorer shell were removed;
new interactive content tooling belongs in Mapper.

The placement selector supports two map positions:

- **Mouse position** uses the most recent valid mapper `MousePos` captured while the pointer was over the map. Move the pointer onto the desired map hex before pressing **Play** in the floating window.
- **View center** resolves the center of the current map viewport when **Play** is pressed.

**Scale**, **Offset X/Y**, **Seed**, and **Prewarm** are preview-only controls. The bundled SPARK and Effekseer runtimes both provide independent per-effect seeded playback, so **Seed** always applies. **Play** loads the selected resource through `SpriteManager::LoadSprite(..., AtlasType::MapSprites)`, verifies that it is a `ParticleSprite`, starts it with a seeded respawn, applies the particle-system scale, optionally prewarms it, and attaches it to the selected hex as a temporary `DrawOrderType::Particles` `MapSprite`. **Restart** rebuilds the preview at its existing hex with the current controls. **Remove** invalidates it.

Effekseer uses the direct-scene path. Its one-second prewarm is deferred until
the first `DrawInScene` after the current map transform has been set; scheduled
updates pause while that prewarm is pending, and the update clock is then
resynchronized so the wait is not counted again.

Startup automation uses the same path and is useful for seeded smoke checks.
Supply a start map plus the optional preview settings; both bundled runtimes
support seeded playback, so the same seed reproduces the effect:

```powershell
.\Binaries\Mapper-Windows-win64\LF_Mapper.exe `
  --Mapper.StartMap Dev/TestRoom.fomap `
  --Mapper.ParticlePreviewEffect Particles/NextSoft01/HealPotion1.efk `
  --Mapper.ParticlePreviewSeed 123 `
  --Mapper.ParticlePreviewScale 1.0 `
  --Mapper.ParticlePreviewPrewarm False
```

`Mapper.ParticlePreviewEffect` is empty by default. When it is set and the start
map loads, Mapper places the effect at the initial viewport center, opens the
preview window, and logs `Mapper particle preview started`. Scale is constrained
to `0.01..100`; the interactive offset remains a window-only control.

The temporary sprite is not a map entity: it does not enter map serialization, dirty tracking, or undo history. `MapperEngine` keeps the loaded `Sprite` alive while `MapView` owns the borrowed `MapSprite`; the map-sprite validity callback prevents stale-pointer access when the view rebuilds. A render-only map rebuild, including window resize, invalidates that borrowed `MapSprite`, so Mapper reattaches the same live preview sprite and preserves its simulation state. Changing the selected resource, switching maps, unloading the owner map, closing the preview window, resetting the ImGui layout, or shutting down the mapper removes the preview. A startup-created preview is preserved across the preview window's first ImGui appearance instead of being mistaken for a stale pre-index state.

## Headless Map Render

### Pipeline goal

For the [Checkpoints](../../Docs/Checkpoints.md) UI we need a flat picture of every static map so the schema generator can post-process it (place entry-point markers, downscale, run the AI generator). Doing this from `LF_Client` would require connecting to a server and walking through the entry; the mapper already loads `.fomap` files directly, so it is the cheapest place to run the export.

### Building blocks

Native helpers in [../Source/Scripting/MapperGlobalScriptMethods.cpp](../Source/Scripting/MapperGlobalScriptMethods.cpp), exported into the mapper-side AS `Game.*` surface:

| Method | Purpose |
|--------|---------|
| `Game.GetCurMapHexSize()` â†’ `msize` | Hex grid size of the currently shown map |
| `Game.GetCurMapPixelSize()` â†’ `isize32` | Pixel bounds of the full map (`width * MAP_HEX_WIDTH`, `height * MAP_HEX_LINE_HEIGHT + tail`) |
| `Game.SetMapperViewSize(size)` | Resize the mapper's render view (wraps `MapView::SetScreenSize`) |
| `Game.CenterMapperOnHex(hex)` | Snap the camera to a hex (wraps `MapView::InstantScrollTo`) |
| `Game.CenterMapperOnRawHex(rawHex)` | Snap the camera to a raw hex without validating it against the authored map rectangle. Use this for preview frames whose visual center follows `ScrollAxialArea`, because that area can extend outside normal map hex bounds. |
| `Game.SetMapperZoom(zoom)` | Set the camera zoom and zoom target (wraps `MapView::InstantZoom`) so warmup frames do not interpolate back to the previous mapper zoom |
| `Game.CalcMapperFitZoom(viewport)` â†’ `float` | Zoom factor needed for the playable area (`ScrollAxialArea`, axial basis; falls back to hex bounding box) to fit a given viewport in pixels â€” same basis as the engine's `MapView::RefreshMinZoom`. NB: this preview helper returns the RAW fit zoom (so a small area fills the frame); `RefreshMinZoom` branches on mode for a SUB-viewport `ScrollAxialArea`: in the live GAME it DROPS the scroll min-zoom (a blocker-constrained map zooms out normally, the client `Camera.SpritesZoomMin` floor governs) instead of forcing a zoom-in, while in the MAPPER it KEEPS the SAA-fit floor so an editor wheel zoom-out (with `Scroll check` enabled) BUMPS into the playable-area border. |
| `Game.SetMapperOverlayVisible(visible)` | Toggle mapper-only scroll-block markers. They are drawn by `Scripts/MapperOverlay.fos` through `Map.DrawMapSprite`, so off means clean previews; defaults to on for normal mapper use. |
| `Game.SetMapperHiddenSpritesVisible(visible)` | Toggle mapper rendering of `AlwaysHideSprite` items. The normal mapper can still show them for editing, while the preview driver disables them by default for client-like captures without invisible blockers / entry markers. |
| `Game.AddMapperIgnoredItemPids(itemPids)` | Add item prototypes to the active map's mapper ignore list and rebuild the map. The preview driver uses this for explicit special marker suppression (`Entrance`, `Trigger`, `ExitGrid`, blockers, lights). |
| `Game.SetMapperScrollCheckEnabled(enabled)` | Toggle mapper scroll clamping (`MapView::SetScrollCheck`). Switching it ON updates the camera IMMEDIATELY: `RefreshMinZoom` raises the zoom *target* to the scroll min-zoom (SAA-fit), `SetScrollCheck` applies it through `InstantZoom` (which rebuilds the rendered view — writing `SpritesZoom` directly only moves the readout and leaves the view stuck at the old zoom) and re-clamps the scroll offset inside `ScrollAxialArea`. The render driver disables it before centering so large/low-zoom captures are not clamped back to `ScrollAxialArea`. |
| `Game.SaveMapperScreenshot(filePath)` | Dump the active main render target to a TGA file (Y-flipped to match the renderer) via the engine-shared `WriteSimpleTga` helper; the helper swaps red/blue so the file is in TGA-native B,G,R,A order and reads back with correct colors |

Mapper exit is the common `Game.RequestQuit()` from [CommonGlobalScriptMethods.cpp](../Source/Scripting/CommonGlobalScriptMethods.cpp) â€” no mapper-specific wrapper.

### Driver script

[Scripts/MapperRender.fos](../../Scripts/MapperRender.fos) wires the helpers into a single-process autopilot that batches multiple maps:

1. Subscribes to `Game.OnStart` (mapper-side) and `Game.OnLoop`.
2. The mapper process is launched with the engine `Render.HeadlessWindow=True` setting so batch runs lock interactive input, suppress exception message boxes, and create the engine render-host window hidden for off-screen rendering; this setting is intentionally separate from the project `Mapper.Render*` settings. Screenshot readback still comes from the mapper sprite manager's main render target.
3. On start, reads `Settings.Mapper.RenderMaps` (space-separated map names or paths relative to `Maps/`) and `Settings.Mapper.RenderOutputDir`. Empty `RenderMaps` disables the autopilot â€” mapper opens its usual interactive UI.
4. For every map: disable critter and fast/special marker drawing via `Hex.ShowCrit=False` and `Hex.ShowFast=False`, `Game.LoadMap`, `Game.ShowMap`, hide mapper-only overlays via `Game.SetMapperOverlayVisible(false)`, add the known blocker/entrance/trigger marker prototypes to the map ignore list, disable scroll clamping/input drift for the capture, clear mouse/key scroll flags during every warmup tick, hide client-invisible blocker sprites via `Mapper.RenderHideHiddenSprites` by default, auto-fit zoom via `Game.CalcMapperFitZoom(viewport) / Mapper.RenderFitPadding` unless `Mapper.RenderZoomOverride` is set, then center on `Mapper.RenderCenterRawHexX/Y`, `Mapper.RenderCenterHexX/Y`, or the playable-area midpoint. Python-side per-map `Viewport` overrides can make that one frame larger than the final PNG, and `ViewportCrop` can cut a fixed inner rectangle from it.
5. Waits `Settings.Mapper.RenderWarmupFrames` `OnLoop` ticks so the new map actually paints into the main render target (the first frame after `ShowMap` still holds the previous frame).
6. Calls `Game.SaveMapperScreenshot("<RenderOutputDir>/<OutputName>.tga")`, then unloads the current map and advances to the next. `OutputName` comes from `Mapper.RenderPlanOutputNames` when provided, otherwise from the loaded map name.
7. After the last map: if `Settings.Mapper.RenderQuitWhenDone` is true, calls `Game.RequestQuit()`.

Single-process batching exists because the mapper's startup is the slowest step (asset baking, AS bytecode load, GPU device init). Rendering 297 maps in one invocation pays it once instead of 297 times.

### Settings

| Setting | Default | ÐžÐ¿Ð¸ÑÐ°Ð½Ð¸Ðµ |
|---------|---------|----------|
| `Mapper.RenderMaps` | `""` | Space-separated list of map names or paths relative to `Maps/` to load (without `.fomap`). Empty value disables the autopilot â€” mapper opens normally. |
| `Mapper.RenderOutputDir` | `""` | Directory for `<OutputName>.tga` outputs. Forward slashes recommended. Created on demand by `WriteSimpleTga`. |
| `Mapper.RenderWarmupFrames` | `4` | Number of `OnLoop` ticks to wait between `ShowMap` and `SaveMapperScreenshot`. Bumping this helps if the dumped TGA looks empty/half-painted. |
| `Mapper.RenderQuitWhenDone` | `True` | If true, the mapper quits after the last map. Set to `False` for interactive debugging. |
| `Mapper.RenderFitPadding` | `1.50` | Extra zoom-out factor above the calculated fit zoom. Keeps trees, cliffs, shadows, and other sprites that protrude outside `ScrollAxialArea` inside the captured viewport. |
| `Mapper.RenderPlanViewports` | empty | Optional semicolon-separated batch list of `W,H` viewports, aligned with `Mapper.RenderMaps`. Used by the preview tool so different maps can render with different overscan sizes in one mapper launch. |
| `Mapper.RenderPlanCenters` | empty | Optional semicolon-separated batch list of raw-hex `X,Y` centers, aligned with `Mapper.RenderMaps`. |
| `Mapper.RenderPlanZooms` | empty | Optional semicolon-separated batch list of zoom values, aligned with `Mapper.RenderMaps`. |
| `Mapper.RenderPlanOutputNames` | empty | Optional semicolon-separated batch list of screenshot base names, aligned with `Mapper.RenderMaps`. The preview tool uses this to load maps from subfolders while keeping `<MapPid>.tga/.png` output names. |
| `Mapper.RenderHideHiddenSprites` | `True` | If true, hides `AlwaysHideSprite` items for a client-like render. Set false only for schematic debugging that needs blocker/ground markers. |
| `Mapper.RenderDumpAtlases` | `False` | Dumps warmed texture atlases after each rendered map, including the polygon-sprite mesh overlay used for visual audits. |
| `Mapper.RenderCenterHexX/Y` | `0 0` | Optional capture camera center. `0 0` means auto-center on the playable area; explicit values are used for tuned one-frame previews. |
| `Mapper.RenderCenterRawHexX/Y` | `0 0` | Optional raw camera center. Takes precedence over `Mapper.RenderCenterHexX/Y` and can point outside the authored map rectangle for axial-border-aligned previews. |
| `Mapper.RenderZoomOverride` | `0.0` | Optional capture zoom. `0.0` means auto-fit; positive values force a tuned zoom. |

`MapperRender` SubConfig in [LastFrontier.fomain](../../LastFrontier.fomain) carries these defaults plus `Parent = Dev`, so a render run inherits the dev-friendly settings.

### Running it

```
LF_Mapper --Render.HeadlessWindow True \
    --ApplySubConfig MapperRender \
    --Mapper.RenderMaps="Encounter_1 Antenna BomberCrash" \
    --Mapper.RenderOutputDir=Workspace/MapPreview
```

Or, if you prefer not to touch the CLI on every call, override `Mapper.RenderMaps` / `Mapper.RenderOutputDir` in a personal SubConfig and just pass `--ApplySubConfig MyMapperRender`.

Set `Mapper.RenderDumpAtlases=True` for a reproducible atlas-mesh audit without interactive mapper hotkeys.

### Current limitations (known scope)

**Backend caveat.** `Game.SaveMapperScreenshot` redraws one mapper frame and reads the sprite manager's main render target (`_rtMain`). If a backend is built without a readable main render target, the save path needs an engine-side framebuffer readback before headless screenshots can work there.

**Frame timing.** `OnLoop` fires before `MapperEngine::DrawMapperFrame()` ([../Source/Tools/Mapper.cpp:730-746](../Source/Tools/Mapper.cpp#L730)), so the first `OnLoop` after `OnStart` reads the previous frame's pixels. `Mapper.RenderWarmupFrames=4` skips a handful of ticks so the new map actually paints into the readable surface; bump it if the dumped TGA looks blank or stale.

**Static item animations.** In mapper mode the editor is a static surface, so `ItemHexView::RefreshAnim` pins every item sprite to its first frame (`Stop()` + `SetTime(0)`) instead of playing the default loop — gated on `MapView::IsMapperMode()`, which is enabled before items load. Doors and containers therefore render closed rather than mid-open, and a capture is identical regardless of `RenderWarmupFrames`. Frame-sequence (FOFRM) and Spine sprites freeze cleanly; model (`.fo3d`) and particle (`.spk`/`.efk`) sprites keep animating (their `Stop`/`SetTime` are no-ops), but normal walls/doors/containers are frame-sequence sprites.

**View bounds.** The preview pipeline renders one mapper frame per map. If a large map is clipped, use a larger viewport, a lower `Mapper.RenderZoomOverride`, an explicit `Mapper.RenderCenterRawHexX/Y`, or a per-map `ViewportCrop` over a larger one-frame viewport; do not stitch several captures together for checkpoint previews.

### Downstream pipeline (planned)

The TGA produced here is only step 1. Step 2 (place entry-point markers, gather metadata) and step 3 (run the AI generator) will live as Python tooling under `../../Tools/` once the rendering side stabilizes. Both will read the TGA plus a JSON sidecar describing entry positions; emitting that JSON is the next addition to `MapperRender.fos`.

## Map Entrance Preview

### Pipeline goal

The checkpoint UI ([Checkpoints.md](../../Docs/Checkpoints.md)) needs a static preview of every map and the pixel position of each `MapEntry` button so the player can pick *where* on the map they want to land. Generating those previews is what this tool does.

### Driver script

[Tools/MapPreview/generate_map_preview.py](../../Tools/MapPreview/generate_map_preview.py):

1. Parses `Maps/**/<MapName>.fomap` and collects every static item with `MapEntry.Name = ...`. Hex positions are read from the matching `Hex = X Y` line.
2. Invokes LF_Mapper with `Render.HeadlessWindow=True` plus the [MapperRender SubConfig](../../LastFrontier.fomain) (Steps from [Headless Map Render](#headless-map-render)). Output TGA goes to a temp dir.
3. Converts the TGA to PNG via Pillow, cropping to a fixed `ViewportCrop`, the authored `ScrollAxialArea` inner border, or the authored-content fallback. `--crop-padding` applies only to the fallback path; axial previews are not expanded to random rendered alpha bounds. A small `--axial-crop-inset` trims mapper border marker pixels after axial crop. The final PNG is scaled to `--output-height` (300px by default) with aspect ratio preserved, and every pixel coordinate written to `MapEntrancePreviews.foinfo` uses the resized image space.
4. For each entry and optional `ScrollAxialArea`, computes final PNG pixel positions by replicating the engine camera transform and subtracting the actual crop offset: `image_pixel = viewport_center + GetHexOffset(center_raw_hex, entry_hex) Ã— zoom âˆ’ crop_offset`. The offset formula mirrors [../Source/Common/Geometry.cpp](../Source/Common/Geometry.cpp).
5. Writes `<MapName>.png` plus/updates `MapEntrancePreviews.foinfo` in `Resources/MapPreview/MapEntrances/`, then updates the matching `Maps/**/<MapName>.fomap` with `MapEntrancePreview = <MapName>` when writing to the default output folder. PNGs are picked by the `MapPreview` ResourcePack rooted at `Resources/MapPreview`, and the fixed-type data is picked by the `Protos` ResourcePack through the same `Resources/MapPreview` root.

### Fixed-type format

`Resources/MapPreview/MapEntrances/MapEntrancePreviews.foinfo` is generated as `MapEntrancePreview` fixed-type data. `$Name` is the map pid, and the generator stores the matching reference in the matching source map as `MapEntrancePreview = <MapName>`, so UI code reads it from `Game.GetProtoMap(mapPid).MapEntrancePreview`. `ImageWidth` / `ImageHeight` are the actual final PNG dimensions after crop and resize (variable aspect ratio per map); `RenderZoomPpm` is the effective zoom scaled by 1,000,000.
If a map has several `MapEntry` items with the same name, the tool writes one entry name and averages the hex/button coordinates; `EntryHexCounts` records how many authored markers fed each entry.

```
[MapEntrancePreview]
$Name = BomberCrash
ImageWidth = 500
ImageHeight = 418
MapHexWidth = 200
MapHexHeight = 200
RenderZoomPpm = 107388
ScrollAxialArea = -10 20 120 80
AxialCropBounds = 12 8 220 140
AxialCropPolygon = 12 8 232 8 232 148 12 148
EntryNames = DefaultEntrance E NE
EntryHexes = 191 65 96 161 47 88
EntryButtonPixels = 128 128 156 100 200 80
EntryHexCounts = 1 1 1
```

When a map has `ScrollAxialArea`, the fixed-type entry also carries `ScrollAxialArea`, `AxialCropBounds` (`X Y Width Height` in final PNG pixels), and `AxialCropPolygon` (four final-PNG pixel points in `LT RT RB LB` order). Consumers that need the inner axial-border crop should use those fields instead of alpha bounds.

### Reading from AngelScript

Typical UI flow:

```angelscript
ProtoMap protoMap = Game.GetProtoMap(mapPid);
MapEntrancePreview preview = protoMap != null ? protoMap.MapEntrancePreview : null;
for (uint i = 0; preview != null && i < preview.EntryNames.length; i++) {
    int bx = preview.EntryButtonPixels[i * 2];
    int by = preview.EntryButtonPixels[i * 2 + 1];
    // place EntryNames[i] button at (bx, by) on MapEntrances/<mapPid>.png
}
```

### Running it

```
# one map
python Tools/MapPreview/generate_map_preview.py --map BomberCrash

# checkpoint-relevant static maps
python Tools/MapPreview/generate_map_preview.py --checkpoint-maps

# every .fomap under Maps/
python Tools/MapPreview/generate_map_preview.py --all
```

Common knobs:

| Flag | Default | Purpose |
|------|---------|---------|
| `--map` | â€” | Map name without `.fomap` (repeatable). Mutually exclusive with `--all` / `--checkpoint-maps`. |
| `--checkpoint-maps` | â€” | Render the checkpoint UI set: maps from public static `Maps/**/*.foloc` (`IsPublic=True`) with multiple maps and/or maps that contain `MapEntry` items. |
| `--all` | â€” | Render every `.fomap` under `Maps/`. Mutually exclusive with `--map` / `--checkpoint-maps`. |
| `--mapper` | autodetect | Path to `LF_Mapper` binary. Tries `Binaries/Mapper-*` and `LF-Dev/Workspace/Mapper-*`. |
| `--viewport` | `2000x1000` | Exact mapper viewport WIDTHxHEIGHT, forwarded as `View.ScreenWidth`/`View.ScreenHeight`. Per-map `Viewport` overrides can render a larger one-frame overscan before the post-processor cuts the final image. |
| `--warmup-frames` | `4` | Forwarded to `Mapper.RenderWarmupFrames`. Bump if the preview comes out blank. |
| `--fit-padding` | `1.50` | Extra zoom-out factor forwarded to `Mapper.RenderFitPadding`. Increase when the rendered alpha touches an output edge and the preview still looks clipped. |
| `--crop-padding` | `40` | Transparent source pixels kept around the authored-content fallback crop. Maps with `ScrollAxialArea` crop to that authored inner axial border instead. |
| `--axial-crop-inset` | `90` | Source pixels trimmed from each side after `ScrollAxialArea` crop so mapper border marker sprites sitting exactly on the axial edge do not leak into the final preview. |
| `--min-render-zoom` | `0.05` | Lower bound for auto-fit zoom; explicit `--zoom` / per-map `Zoom` can go lower. Matches the engine map-view minimum. |
| `--max-render-zoom` | `1.00` | Upper bound for auto-fit zoom; prevents tiny marker-only maps from asking the mapper for extreme zoom. Explicit `--zoom` / per-map `Zoom` can go higher. |
| `--center-hex` | â€” | Override the capture center as raw `X,Y` for a tuned single-frame preview. Raw centers may sit outside the authored map bounds. |
| `--zoom` | â€” | Override the capture zoom for a tuned single-frame preview. |
| `--output-height` | `300` | Final PNG height after crop. Width is derived from aspect ratio, and `ButtonPixel` / `AxialCrop*` metadata is scaled to this final size. Use `0` to keep the crop height before `--max-output-size`. |
| `--max-output-size` | `2000` | Additional final width/height cap after `--output-height`. Use `0` to disable this cap. |
| `--overrides` | `../../Tools/MapPreview/map_preview_overrides.ini` | Per-map tuning file. Each `[MapName]` section can set `Viewport = W H`, `ViewportCrop = X Y W H`, `CenterRawHex = X Y` (preferred), legacy `CenterHex = X Y`, and `Zoom = value`. |
| `--ignore-overrides` | off | Ignore the per-map tuning file and use only auto-fit / CLI values. |
| `--hide-hidden-sprites` | on | Forwarded to `Mapper.RenderHideHiddenSprites`; explicit form of the default client-like preview behavior. |
| `--show-hidden-sprites` | off | Forwarded to `Mapper.RenderHideHiddenSprites=False`; use only for debugging schematic blocker/ground markers. |
| `--output-dir` | `<root>/Resources/MapPreview/MapEntrances` | Override sink directory. |
| `--keep-tga` | off | Copy the raw TGA next to the PNG for debugging. |
| `--skip-existing` | off | Skip maps whose `<name>.png` already exists. Use after a native crash mid-batch â€” the next run picks up where the previous one left off. |

The python script invokes `LF_Mapper` once for the planned batch, enables hidden-window batch mode with `Render.HeadlessWindow=True`, and passes per-map viewport, center, zoom, and output-name lists through `Mapper.RenderPlanViewports`, `Mapper.RenderPlanCenters`, `Mapper.RenderPlanZooms`, and `Mapper.RenderPlanOutputNames`; if edge-touch retry is needed, each retry round is also a batch rather than one mapper process per map. Map loading uses the path relative to `Maps/`, so maps in subfolders still load while output files and fixed-type records keep the stable map proto name. `--checkpoint-maps` builds its target list from authored location/map data: it scans `Maps/**/*.foloc` `MapProtos`, keeps only public static locations (`IsPublic=True`), skips encounter/camp templates (`ReadyForEncounter`, `ReadyForCamp`, authored `IsEncounter` / `IsCamp`, or explicit `EncounterGenerationMode`), keeps every map from remaining locations with more than one unique map proto, and also keeps any remaining public location map whose `.fomap` has at least one `MapEntry`. It also passes `Hex.FullscreenMouseScroll=False` and `Hex.WindowedMouseScroll=False`; `MapperRender.fos` clears the live scroll flags during warmup, disables `Hex.ShowCrit` / `Hex.ShowFast`, toggles off the mapper track layer, and explicitly ignores the known special marker prototypes after `ShowMap`, so the camera cannot drift and critters or mapper markers do not appear in checkpoint previews. The Python fit/crop pass ignores the same explicit marker prototypes when choosing fallback visual bounds, while still writing their `MapEntry` button coordinates to `MapEntrancePreviews.foinfo`; maps with `ScrollAxialArea` crop to that authored inner axial border and do not expand to rendered alpha bounds. When the mapper shows a loaded map, it applies a display-only time override: `FixedDayTime` for maps with `FixedTime=True`, or noon for outdoor maps without fixed time. This does not rewrite the map's authored `FixedDayTime`. A fixed `ViewportCrop` still comes from a single mapper frame; it is used for overscan cases where the mapper's minimum zoom fits the map only after rendering a slightly taller/wider viewport. After crop, the PNG is downscaled to the requested output height, and the script writes entry-point pixels plus axial crop metadata in that final coordinate space. When `--output-dir` is the default `Resources/MapPreview/MapEntrances`, it also writes the `ProtoMap.MapEntrancePreview` link into each rendered source map. Pillow (`pip install Pillow`) is the only non-stdlib dependency.

### Known limitations

- Engine geometry constants (`HEX_WIDTH = 32`, `HEX_HEIGHT = 16`, hexagonal layout) are mirrored as Python constants in the script. If `../../CMakeLists.txt` ever changes them, update both sides.
- If the planned crop touches the viewport edge, the script prints a warning; rerun with a larger `--fit-padding` or viewport.
- Button pixel coordinates are based on `MapEntry` hex centers. The preview image is cropped to `ScrollAxialArea` when authored, a per-map `ViewportCrop` when configured, or the fallback content/alpha bounds otherwise, so entries can legitimately land outside visible alpha if the entry item itself is hidden as client-invisible.
- AS `try { } catch { }` in [Scripts/MapperRender.fos](../../Scripts/MapperRender.fos) skips maps whose load/show/save throws a script exception, but a hard native crash (e.g. missing critter animation, broken proto) still kills the mapper. Run again with `--skip-existing` to resume after fixing the offending content.

### Timing

End-to-end wall-clock on the dev machine (DirectX, 297 maps):

- Mapper startup: paid per `LF_Mapper` invocation.
- Per-map work (LoadMap + overlay-toggle RebuildMap + auto-fit zoom + 4 warmup frames + DrawMapperFrame + read `_rtMain` + WriteSimpleTga): ~1â€“2s.
- TGA â†’ PNG crop + `MapEntrancePreviews.foinfo` update (Python, post-mapper): ~50ms per map.

For full-map batches, prefer `--skip-existing` while tuning so interrupted or corrected runs resume from already generated previews.

## See Also

- [Architecture.md](Architecture.md) — repository layout and engine layer ownership
- `../../Docs/BuildAndLaunch.md` — embedding-project build route for `LF_Mapper`
- `../../Docs/Checkpoints.md` — embedding-project consumer of rendered map schemas and entrance previews
- [Scripting.md](Scripting.md) — engine scripting runtime and mapper-side script method ownership
