# Frontend and Rendering

> Engine-owned documentation. This page describes the reusable application, input, audio, window, and rendering abstractions under `Source/Frontend/` plus the client render-target bridge in `Source/Client/`.

## Purpose

The frontend layer is the boundary between the platform and the engine runtime. It owns windows, frame boundaries, input queues, touch/gamepad translation, audio device access, renderer selection, and low-level render backend objects. The client runtime consumes this layer through stable interfaces instead of calling SDL, OpenGL, Direct3D, or Web APIs directly.

Read this page together with:

- [ClientRuntime.md](ClientRuntime.md) for how `ClientEngine`, `SpriteManager`, `MapView`, and runtime managers use these services.
- [BuildWorkflow.md](BuildWorkflow.md) and [BuildToolsPipeline.md](BuildToolsPipeline.md) for configure/build composition.
- [WebDebugging.md](WebDebugging.md) and [AndroidDebugging.md](AndroidDebugging.md) for platform package/debug flows.
- [Debugging.md](Debugging.md) for native debugging and stack traces.

## Source paths inspected

- `Source/Frontend/Application.h`
- `Source/Frontend/Application.cpp`
- `Source/Frontend/ApplicationInit.cpp`
- `Source/Frontend/ApplicationHeadless.cpp`
- `Source/Frontend/ApplicationStub.cpp`
- `Source/Frontend/Rendering.h`
- `Source/Frontend/Rendering.cpp`
- `Source/Frontend/Rendering-OpenGL.cpp`
- `Source/Frontend/Rendering-Direct3D.cpp`
- `Source/Frontend/Rendering-Null.cpp`
- `Source/Client/RenderTarget.h`
- `Source/Client/RenderTarget.cpp`
- `Source/Client/SpriteManager.h`
- `Source/Client/SpriteManager.cpp`
- `Source/Common/Geometry.h`
- `Source/Common/Geometry.cpp`
- `Source/Client/EffectManager.h`
- `BuildTools/cmake/stages/Packages.cmake`
- `Source/Tests/Test_Rendering.cpp`
- `Source/Tests/Test_Geometry.cpp`

## Layer map

The frontend/rendering split has three layers:

1. **Application layer** (`Application`, `AppWindow`, `AppInput`, `AppAudio`, `AppRender`) owns platform services and frame boundaries.
2. **Renderer layer** (`Renderer` and its backends) owns GPU/null rendering resources: textures, draw buffers, effects, matrices, scissor state, presentation, and resize handling.
3. **Client drawing layer** (`SpriteManager`, `RenderTargetManager`, `EffectManager`, `MapView`) builds engine/game drawing operations on top of the renderer abstraction.

This keeps most client code renderer-agnostic. The client asks for sprites, effects, draw buffers, render targets, and input events; the selected backend decides how those are implemented.

## Matrix convention

Engine render math uses one matrix convention:

- `mat44` is the GLM matrix type defined in `Source/Common/Common.h`.
- Storage is column-major. Direct indexing is `matrix[column][row]`; translation is `matrix[3].xyz`; `glm::value_ptr(matrix)` can be copied into uniform buffers without transposing.
- Algebra uses column vectors. Compose transforms as `clip = Proj * View * Model * vec4(position, 1.0)`.
- Shader code follows the same convention with `ProjMatrix * vec4(...)`. Backend-specific differences belong inside the matrix constructors and shader cross-compilation path, not in ad-hoc row/column transposes at call sites.

Use `RowMajor`/`ColumnMajor` naming only for explicit boundary conversion with external data formats. Internal renderer, model, particle, and geometry code should name matrices by role (`ProjMatrix`, `ViewMatrix`, `ViewProjMatrix`, `WorldMatrix`) rather than by storage order.

## Application initialization

`InitApp()` and `LoadAppSettings()` in `Source/Frontend/ApplicationInit.cpp` prepare global settings and application services before the client/server/tool app creates its engine object.

Notable responsibilities:

- parse command-line and local configuration;
- load app settings from config/cache sources;
- initialize frontend globals once;
- optionally locate and call baking support through `FO_BakeResources` when resource baking is needed by the current app flow;
- prepare the app-level services used by clients, tools, and headless/test modes.

Application initialization is intentionally shared by more than the graphical client. Server, mapper, editor, testing, and package flows may use different flags or window modes, but they should still go through the shared frontend setup where applicable.

## Application services

`Source/Frontend/Application.h` defines the public frontend surface.

### `Application`

`Application` owns process-level frontend state:

- main and child windows;
- active window selection;
- frame boundaries through `BeginFrame()` and `EndFrame()`;
- render-window boundaries through `BeginWindowRender()` and `EndWindowRender()`;
- link opening and user-facing message/progress/choice dialogs;
- main-loop callback registration;
- quit requests and wait-for-quit synchronization;
- touch gesture resolution and gamepad state refresh.

### `AppWindow` / `IAppWindow`

Window responsibilities include:

- size, screen size, position, display rect, focus, fullscreen state;
- minimize, blink, always-on-top, title, input grabbing, and destruction;
- distinguishing real OS windows from **virtual** windows (`IsVirtual()`) — host-composited embedded windows used by the multi-client host, whose render size is their own per-window virtual size rather than the shared logical screen size;
- resolving a native `WindowInternalHandle` for render backends;
- resolving a `HeadlessWindowStub` in headless/stub contexts.

### `AppInput` / `IAppInput`

Input responsibilities include:

- polling queued `InputEvent` values;
- clearing and pushing events;
- mouse position control;
- screen keyboard enablement;
- clipboard text;
- gamepad state;
- normalization of mouse, keyboard, wheel, touch tap, touch double-tap, touch scroll, and touch zoom events.

Mouse button input preserves the concrete platform button id when mapping to script-facing `MouseButton`
values (`Left`, `Right`, `Middle`, `Ext0`/`Ext1`, ...); unknown native buttons are ignored rather than
falling back to a primary click.

The client turns these lower-level events into script events in `ClientEngine::ProcessInputEvent()`.

SDL mouse-motion events are the primary source for `InputEvent::MouseMoveEvent`. On backends where SDL exposes global mouse coordinates (Windows, macOS, X11, and the whitelisted OS/2 drivers), `Application::BeginFrame()` also polls global mouse state while the app remains focused. If no SDL motion event arrived in that frame and the global position changed, the frontend synthesizes a mouse-move event from the global position. This keeps the game cursor and edge-scroll state updating when the OS pointer has moved outside the client window instead of freezing at the last in-window event. The same host-to-active-window translation path is used for this synthetic event, so embedded virtual clients still receive local logical coordinates through their display rect and aspect-fit mapping.

### `AppAudio` / `IAppAudio`

Audio responsibilities include:

- reporting whether audio is enabled;
- setting an audio stream callback;
- converting audio formats;
- mixing audio;
- locking and unlocking the audio device around critical sections.

## Headless and stub modes

Two non-normal modes are important for tools, tests, CI, and platform staging:

- `Source/Frontend/ApplicationHeadless.cpp` supports headless operation where a real visible client window is not required.
- `Source/Frontend/ApplicationStub.cpp` provides stub implementations of render, input, audio, and window interfaces.

The stub layer is not a full renderer. It exists so tests and non-graphical flows can exercise engine logic without assuming that a real GPU/window/audio device is available. When a test depends on visible rendering, it should say so explicitly instead of relying on stub behavior.

## Rendering abstraction

`Source/Frontend/Rendering.h` defines the renderer-facing types:

- `RenderType` — backend family selection.
- `EffectUsage` — effect category used when loading/compiling effects.
- `RenderPrimitiveType` — primitive topology used by draw buffers.
- `BlendFuncType` and `BlendEquationType` — blend-state configuration read from effect config.
- `Vertex2D` and `Vertex3D` — vertex layouts used by sprite and model paths. For primitive batches uploaded through `SpriteManager::DrawPoints`, `PosX/PosY` are the draw-area-local pixel coordinates (with the `draw_area` scroll offset subtracted), and `TexU/TexV` carry `PrimitivePoint::TexUV + draw_area.xy` — that is, `DrawPoints` adds the draw-area top-left to whatever the caller authored. The intended idiom for world-stable per-fragment effects (dither, noise, gradient mapping) is to author the **same constant** `TexUV` on every vertex of a primitive batch — the absolute map-origin-anchored pixel position of the screen-anchor hex `_screenRawHex`. The fragment shader then reconstructs each fragment's true absolute world pixel position as `gl_FragCoord.xy + InTexCoord`. Because every vertex carries the same constant, varying interpolation is degenerate (no barycentric rounding can crawl the noise), and the rasterizer's per-pixel `gl_FragCoord` provides the spatial variation. This is robust against camera scroll, camera zoom, fan-triangle deformation from smooth sprite movement, and the `from_hex.x` parity sensitivity of `GeometryHelper::GetHexOffset` on offset-row hexagonal grids. `MapView::LightFanToPrimitves` authors it via `GeometryHelper::GetHexOffset(mpos(0, 0), _screenRawHex)`. Existing primitive shaders ignore `InTexCoord` and are unaffected.
- `RenderTexture` — backend texture/render-target resource.
- `RenderDrawBuffer` — vertex/index storage uploaded to the backend.
- `RenderEffect` — shader/effect object plus standard uniform/script-value buffers.
- `Renderer` — backend interface implemented by concrete renderers.

`Source/Frontend/Rendering.cpp` owns backend-independent helper behavior, including draw-buffer allocation checks and effect configuration parsing. It reads effect sections such as `Effect` and `EffectInfo`, pass counts, blend settings, and script-visible buffers before backend-specific code consumes shader files.

## Render backends

### Null renderer

`Source/Frontend/Rendering-Null.cpp` implements `Null_Renderer`, `Null_Texture`, `Null_DrawBuffer`, and `Null_Effect`.

Use it for tests, headless flows, and validation that should not require a GPU. It still validates dimensions, buffer counts, render-target state, and texture region access, so it is useful for catching many API misuse errors.

### OpenGL renderer

`Source/Frontend/Rendering-OpenGL.cpp` implements the OpenGL/WebGL path.

Important behaviors:

- creates an SDL/OpenGL or WebGL context depending on platform;
- loads and validates required GL entry points/extensions;
- sizes the atlas against `AppRender::MAX_ATLAS_SIZE` and backend limits;
- creates textures, draw buffers, and effects;
- compiles/loads vertex and fragment shader content through the effect loader;
- reports render-target textures as vertically flipped (`IsRenderTargetFlipped() == true`).

OpenGL is the path to inspect for WebAssembly/WebGL behavior. Pair renderer changes with [WebDebugging.md](WebDebugging.md) validation.

### Direct3D renderer

`Source/Frontend/Rendering-Direct3D.cpp` implements the Direct3D 11 path.

Important behaviors:

- creates D3D device/swap-chain/render-target resources;
- creates textures, staging textures, draw buffers, constant buffers, and effects;
- loads vertex/pixel shader content through the effect loader;
- handles resize by recreating backbuffer/depth resources;
- reports render-target textures as not flipped (`IsRenderTargetFlipped() == false`).

Direct3D changes are Windows-specific and should be validated through a Windows embedding-project build/debug flow.

## Render targets and client bridge

`Source/Client/RenderTarget.h` and `Source/Client/RenderTarget.cpp` are the client-side bridge from high-level drawing code to backend textures.

`RenderTargetManager` responsibilities:

- create render targets with optional depth and linear filtering;
- allocate backend `RenderTexture` objects through `IAppRender::CreateTexture()`;
- preserve/restore the previous backend render target while allocating and clearing a new target;
- maintain a render-target stack through `PushRenderTarget()` and `PopRenderTarget()`;
- clear the current render target;
- resize render targets;
- read pixels from render targets with a small last-pixel-pick cache;
- delete render targets and clear the stack;
- dump render-target textures for debugging.

`MapView`, `SpriteManager`, `ModelSpriteFactory`, and `ParticleSpriteFactory` all rely on render targets for map layers, light buffers, model/particle atlas rendering, hit testing, and offscreen composition.

Model-attached SPARK particle systems keep already spawned particles in their simulation space while the emitter follows the model attachment point. A non-identity root transform in the particle resource selects the position-plus-facing path instead of inheriting the full bone matrix; this keeps lingering particles world-stable during model movement while new particles spawn at the current attachment point. The model movement offset is subtracted in particle model space before camera rotation and projection so the setup-time positive offset and draw-time negative offset cancel for newly emitted particles.

## Screen size, resolution, and letterboxing

Two distinct sizes drive client rendering:

- **Logical screen size** — `Settings.View.ScreenWidth/ScreenHeight`. This is the coordinate space the game renders in: the main render target `_rtMain` (`SpriteManager`), the projection matrix, and the GUI/ImGui display size all use it.
- **Backbuffer (framebuffer) size** — the actual output surface: the OS window's pixel size in windowed mode, the monitor size in fullscreen, or an embedded client's virtual render texture in the multi-client host.

The game always renders into `_rtMain` at the logical size; the final blit (`Renderer::SetRenderTarget(nullptr)` in the backends) then **stretches/upscales `_rtMain` with aspect ratio preserved** into the backbuffer (centered, with bars only when the aspects differ). This is deliberate: fullscreen must scale the chosen logical resolution up to the monitor without non-proportional distortion. When the two sizes are equal the blit is 1:1 with no bars. Accordingly `_rtMain` is sized to `GetScreenSize()` and is resized on the screen-size-changed event. Dispatchers are semantic: `OnScreenSizeChanged` fires only when the logical screen size changes, while `OnWindowSizeChanged` fires when the physical/host window changes.

Script offscreen surfaces (`Game.ActivateOffscreenSurface` / `Game.PresentOffscreenSurface`) also operate in the logical screen coordinate space, because scripts draw them while `_rtMain` is active. Pooled offscreen render targets must therefore be created at `SpriteManager::GetScreenSize()` and resized when the logical resolution changes before they are reused; otherwise effects such as monitor-noise GUI composition can clip content that moves outside the old resolution.

### Windowed

Window pixel size and logical screen size are kept equal. Resizing the OS window raises `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED`; while the main window is not fullscreen, that event writes `Settings.ScreenWidth/Height` from the new pixel size, fires `OnWindowSizeChanged`, and fires `OnScreenSizeChanged` only when those settings actually changed. `Game.SetResolution(w, h)` first updates the logical size through `SetScreenSize`, then resizes the OS window only when the client is neither fullscreen nor virtual; the following OS-window resize is treated as a window-size event only if it reports the same logical size, avoiding a second GUI/map screen-size refresh for the same resolution change.

### Fullscreen (borderless desktop)

The window uses `SDL_SetWindowFullscreenMode(window, nullptr)`, so the framebuffer is always the monitor size and cannot be resized to a sub-monitor resolution. A "resolution" in fullscreen is the **logical** render size: `Game.SetResolution` changes the logical size (`SetScreenSize`), and the backbuffer blit stretches/upscales that logical render to the monitor **with aspect ratio preserved**. Fullscreen startup, fullscreen toggles, and fullscreen `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` events update the renderer/backbuffer only; they must not overwrite `Settings.ScreenWidth/Height` or fire `OnScreenSizeChanged`, otherwise the selected logical resolution collapses to the monitor size and there is nothing left to stretch. `AppWindow::ToggleFullscreen()` marks the transition before calling SDL because SDL can queue the pixel-size event while the OS/window flags still appear to be in the previous mode. This is not a non-proportional stretch; bars are expected only when the selected logical aspect differs from the monitor aspect.

SDL documents that `SDL_SetWindowSize` has no effect while a window is fullscreen or maximized, so the engine must not rely on that call changing the live fullscreen framebuffer. For native non-virtual clients, `Game.SetResolution` still records the requested size as the pending windowed size while fullscreen. When the client leaves fullscreen, `SpriteManager::ToggleFullscreen()` applies that pending size to the restored window and then re-centers the window using the accumulated resolution delta. This preserves both rules: fullscreen presents as aspect-preserving stretch to the monitor, and returning to windowed mode uses the last selected resolution as the OS window size.

### Embedded clients in the multi-client host (virtual windows)

`ServerApp` can host several embedded clients (the `Single`/`Tile`/`Cascade` layouts, `Spawn Client`). Each embedded client is its **own engine instance** with its **own `GlobalSettings`** and a **virtual** `AppWindow` (`IsVirtual()`). A virtual window:

- keeps physical virtual-window size (`_virtualSize`, `GetSize()`) separate from logical client resolution (`_virtualScreenSize`, `GetScreenSize()`);
- renders game content into `_rtMain` at the logical size, then aspect-fits that render target into `_virtualRenderTex` at the physical virtual-window size;
- is composited by the host: `ServerApp` draws the client's virtual render texture aspect-fitted and centered into a per-client display rect (`SetDisplayRect`), and maps input back through that rect, `_virtualSize`, and the same aspect-fit content rect used for rendering so black bars do not skew client-local mouse coordinates.

Because each embedded engine owns its settings, a resolution change must update the **owning engine's** settings, not the host's. Virtual `AppWindow::SetScreenSize`/`GetScreenSize` store the logical size in `_virtualScreenSize`, while `SpriteManager::SetScreenSize` mirrors the new size into the embedded engine's own `Settings.ScreenWidth/Height` before the screen-size-changed handlers run. `SetResolution` skips `SetWindowSize` for virtual windows, and `SetScreenSize` does not mutate `_virtualSize`, so changing a client resolution no longer resizes the virtual render texture or the host layout. A standalone client has a single engine where the engine's settings and `App->Settings` are the same instance, so the real window handles it directly.

GUI screens re-center on a resolution change through the client's `OnScreenSizeChanged` handler → `Gui::Callback_OnResolutionChanged()`, which re-runs each screen's layout against the current `Settings.View.ScreenWidth/Height` (a screen with `Anchor: None` is centered against the parent/screen size). This is why both `_rtMain`/`GetScreenSize()` **and** the engine's own settings must reflect the new logical size: the render target controls what is drawn, the settings control where the GUI lays it out.

Local-map viewports recenter instantly on the chosen critter when their screen size actually changes. This keeps the player anchored after resolution changes in standalone clients, fullscreen logical-resolution changes, and embedded virtual clients. `MapView` must derive that size from the logical client screen size, not from the physical OS window/backbuffer size; fullscreen scaling is handled by the final render-target blit.

## Effects and shader data

`RenderEffect` owns standard buffers used by render paths:

- projection/main texture data;
- transparent egg parameters;
- sprite border parameters;
- time/random/script values;
- camera/model/model-texture/model-animation data.

`EffectManager` in `Source/Client/EffectManager.h` loads minimal/default effects, resolves script-selected effects, writes script-value buffers, and performs per-frame updates. Scripts can write one `ScriptValueBuf` float with `Game.SetEffectScriptValue(...)`, or write a contiguous range with `Game.SetEffectScriptValues(effectType, effectSubtype, valueStartIndex, values, valuesOffset = 0, valuesCount = -1)` to avoid repeated native calls when updating shader parameter blocks. Both APIs validate the selected effect, require the shader to declare `ScriptValueBuf`, and enforce the configured `EFFECT_SCRIPT_VALUES` range. When adding an effect feature, document whether the change belongs in:

- effect config parsing in `Rendering.cpp`;
- backend shader loading/drawing in `Rendering-OpenGL.cpp` or `Rendering-Direct3D.cpp`;
- client effect selection/update code in `EffectManager`;
- map/client draw sequencing in `MapView` or `SpriteManager`.

## Per-effect depth state and the shared map depth buffer

Effects carry per-pass depth state parsed from the `.fofx` `[Effect]` block:

- `DepthWrite` (default `True`) → `RenderEffect::_depthWrite[pass]` (depth write mask).
- `DepthFunc` (default `Always`) → `RenderEffect::_depthFunc[pass]` (`DepthFuncType`: `Always`/`Never`/`Less`/`LessEqual`/`Equal`/`GreaterEqual`/`Greater`/`NotEqual`). Both `Rendering-OpenGL.cpp` and `Rendering-Direct3D.cpp` translate it (`ConvertDepthFunc`) and the backends diverge in NDC-Z (`[-1,1]` GL/GL-ES vs `[0,1]` D3D), so depth-dependent effects must be validated on both.

The map render target (`MapView::_rtMap`) is created `with_depth`, giving the world one shared depth buffer. `EffectUsage::QuadSprite` and `EffectUsage::Model` effects participate in it (depth state is a hardware no-op on targets without a depth attachment — UI, light, flush-to-screen):

- Screen-space quads (GUI, fonts, render-target blits, and non-map sprite effects) initialize `Vertex2D::PosZ` to `0.0f`; map sprites may start from the same atlas data, but `SpriteManager` overwrites their Z before flushing them into `_rtMap`.
- Opaque map sprites use `DepthFunc = Always` (the default): they write depth without rejecting fragments, so draw order and the 2.5D look are unchanged while the buffer is populated for later scene-depth tests. `MapSprite` receives critter/item `Elevation`; positive elevation shifts the sprite upward in screen Y and increases the same world-Z depth. `MapSprite::HexOffset` plus runtime sprite/tweak offsets are projected along the ground plane before depth is computed, so sub-hex movement changes both screen position and 3D depth continuously; viewport-only `field.Offset` is not part of world depth. The intrinsic `Sprite::Offset` is different: it defines which pixel inside the atlas quad is the logical root on the ground, so vertical depth and direct-to-scene anchors use that root instead of assuming the bitmap's lower center. Floor tile layers (`DrawOrderType::Tile..Tile4`) keep their atlas-provided screen-space XY/UV, but `SpriteManager` rewrites each vertex `PosZ` through `GeometryHelper::ProjectMapYToGroundDepth`, so their depth buffer shape is a horizontal ground-plane quad instead of a vertical billboard at the tile anchor. Standing sprite layers (`Scenery`, `Item`, `Critter`) also keep atlas XY/UV, but write per-vertex `PosZ` through `GeometryHelper::ProjectMapYToVerticalDepth`, so they behave like vertical planes standing on their ground anchor. These proxy-geometry layers write pure world depth without per-draw-order bias; otherwise the proxy plane is artificially pulled toward the camera and stops matching the logical root point. Flat overlays, roof tiles, and other layers still use a computed raw-depth bias until they get their own geometry proxy: `SpriteManager` divides a half-pixel depth budget by `DrawOrderType::Last + 1`, keeping the full layer spread below the subpixel snapping threshold. Both `Core` and `Embedded` `2D_Default.fofx` must project the full `InPosition.xyz`; if an override flattens to `InPosition.xy, 0.0`, particles have no useful scene depth to test against. `2D_Default.fofx` and `2D_WithoutEgg.fofx` discard fragments whose final alpha is at or below `1/255` (after egg alpha), so fully transparent sprite texels do not populate the depth buffer and clip in-scene particles behind the empty parts of the atlas quad.
- Particle effects (`Particles_*.fofx`) use `DepthFunc = LessEqual` + `DepthWrite = False`: tested against scene depth so they are occluded by closer geometry, without occluding each other.
- Model effects (`3D_*.fofx`) use `DepthFunc = LessEqual` + `DepthWrite = True`: direct-to-scene models write real mesh depth into `_rtMap`, so particles and later direct geometry can test against the model surface instead of the old model atlas quad.
- `OnRenderMap_AfterSpritesAndFog` fires after the sprite/fog map pass and before the map target is flushed. Scripts that need entity debug markers should iterate the relevant visible `Item`/`Critter` objects, combine `Map.GetHexMapPos(entity.Hex)`, `entity.GetSpriteOffset()`, and `entity.Elevation`, then subtract the event draw-area origin; this keeps selection/filtering in scripts without depending on transient sprite instances.

## Direct-to-scene sprites

A `Sprite` may override `IsDirectDraw()` to render its own geometry **straight into the current scene render target** (with the shared depth buffer) instead of being batched as an atlas quad. Because such a sprite uses its own shader (not the sprite batch's), drawing it at its interleaved draw-order position would split the sprite batch around every one. Instead `SpriteManager::DrawSprites` **collects** direct-draw sprites during the batch loop and replays them in a single `Sprite::DrawInScene(scene_pos, depth)` pass (a `const` method, like `FillData`) *after* the whole sprite batch is flushed — so the batch stays intact. Opaque sprites write depth (`DepthFunc = Always`, `DepthWrite = True`) and direct-draw transparents only test it (`LessEqual`, `DepthWrite = False`), so scene occlusion comes from the shared depth buffer. Direct-draw anchors use the projected `hex + HexOffset + SpriteOffset/TweakOffset + Elevation` map position, deliberately excluding viewport-only `field.Offset`, and keep only a single computed anchor-bias step instead of inheriting their late draw order; otherwise `DrawOrderType::Particles` would become depth-closer than critters/scenery before the particle geometry itself is even considered.

`ParticleSprite` supports **two render types**, chosen per particle system by the `SparkQuadRenderer` `draw in scene` `.fopts` attribute (`ATTRIBUTE_TYPE_BOOL`, default false — alongside `draw size`):

- **Atlas type** (default, `draw in scene` absent/false): `Update()` renders the Spark system to an offscreen atlas (`ParticleSpriteFactory::DrawParticleToAtlas`); the sprite is then drawn as a flat batched quad. `IsDirectDraw()==false`.
- **Scene type** (`draw in scene = true`): `IsDirectDraw()==true`, `Update()` is a no-op, and `DrawInScene` renders the system directly into `_rtMap` through the map view-proj so particles depth-sort against scene geometry instead of being baked to a flat sprite.

`ParticleSprite::Play()` respawns its `ParticleSystem` before starting updates, so one-shot SPARK systems can be replayed after `Game.PlaySprite(...)` or after `AnimFree`/`AnimLoad` cache reuse.

The flag flows `SparkQuadRenderer::GetDrawInScene()` → `ParticleSystem::GetDrawInScene()` → `ParticleSpriteFactory::LoadSprite`. Model-bone particles (`ModelInstance::RunParticle`) are a separate path and ignore this attribute.

`ModelSprite` can also use the direct-to-scene path for visible map rendering when `Render.ModelDirectDraw` is enabled. With the default `false` value, map models stay on the cached atlas-sprite path: `ModelSprite::Update()` refreshes the model atlas and the sprite batch draws the atlas quad. With `Render.ModelDirectDraw = true`, `ModelSprite::DrawInScene` builds the same shared map view-proj basis as scene particles, bakes the map sprite's logical root (`scene_pos` + raw scene depth) into the proj, and calls `ModelInstance::DrawInScene`. The model animation/skinning path is reused, but the old atlas-only camera tilt is skipped so the shared map VP owns the tilt once. `DrawToAtlas` is retained for preview and hit-test data. Model-bone SPARK particles use the active direct-scene proj with `tilt_in_proj`, so attached transparent particles render in the same world-space map frame and test against shared depth. Direct scene draws still disable the old model shadow pass because its shader math is atlas-space and needs a separate world-space rewrite.

**World scale.** `Render.ModelProjFactor` is the screen px per 3D world unit (= `32` = `MAP_HEX_WIDTH`), i.e. **1 world unit = 1 hex = 1 m** — the single metric shared by 3D models and in-scene particles. So a scene-type system that emits within a radius of N units spans N hexes on the ground, matching direct-to-scene 3D models authored to the same scale.

## Platform packages and BuildTools relationship

`BuildTools/cmake/stages/Packages.cmake` participates in package target generation. Platform package workflows decide which app/runtime artifacts are packaged, but renderer/backend availability still comes from configured source, compile definitions, third-party dependencies, and platform toolchains.

Keep these boundaries clear:

- frontend source defines what the engine can do;
- CMake/BuildTools decide which apps/backends/platform packages are built;
- embedding-project presets choose concrete configurations;
- platform docs explain how to debug the resulting package.

Do not document one embedding project's generated target names as universal engine target names.

## Frontend/rendering validation tests

Use `Source/Tests/Test_Rendering.cpp` as the smallest current engine-local test surface for renderer API behavior that should not require a real GPU. The test exercises the null renderer path, draw-buffer limits, texture creation, render-target creation, and invalid-argument checks. Pair it with platform-specific manual/debug validation when changing OpenGL/WebGL or Direct3D backend code.

## Validation checklist

When changing frontend or rendering behavior, verify:

- `Application` init still works for graphical, headless, and test/tool flows.
- Input changes preserve `InputEvent` invariants and client script event mapping.
- Touch/gamepad changes are platform-neutral unless clearly guarded.
- Renderer changes are tested on the affected backend: null/headless, OpenGL/WebGL, or Direct3D.
- Render-target changes preserve stack push/pop behavior and previous-target restoration.
- Texture orientation changes account for `IsRenderTargetFlipped()` differences between OpenGL and Direct3D.
- Effect changes document config parsing, shader files, and script-value buffer implications.
- Web changes cross-link to [WebDebugging.md](WebDebugging.md); Android changes cross-link to [AndroidDebugging.md](AndroidDebugging.md); native attach/debug changes cross-link to [Debugging.md](Debugging.md).
