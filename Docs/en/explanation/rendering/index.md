---
layout: default
title: Frontend and Rendering
locale: en
document_id: frontend-rendering
permalink: /Docs/en/explanation/rendering/
---

# Frontend and Rendering

## Bounded map view presentation

The `ViewMap` packet carries a focal hex and signed 32-bit radius. Zero shows the full map; a positive radius initially centers the camera there and anchors visibility to that hex. Manual and automatic scrolling remain available within authored bounds after zoom or screen-size changes. Resizing clamps the camera instead of returning it to the focal hex. Camera movement never moves or enlarges visibility.

Between map sprites/fog and the final map flush, `MapView` draws bounded-view fog with `Effects/Primitive_ViewAreaFog.fofx`. For direct and offscreen drawing, it fades smoothly inward from the inclusive hex boundary and stays opaque outside, independently of regular fog settings. A project may override the path but must retain full outside opacity. Vertex red encodes the hex-radius fraction (zero at center, one at/beyond the boundary); texture coordinates encode normalized ellipse position; egg coordinates encode absolute world pixels for camera-independent edge noise. The server separately enforces dynamic subject visibility; static baked resources stay packaged. Hex/item screen queries and map item lookups exclude out-of-radius cells and items, including baked static items.

See [Video](../../how-to/content/video.md) for the experimental Ogg/Theora decoder, fullscreen draw order, embedded `RenderIface` presentation, texture upload, aspect behavior and visible acceptance boundary. This page owns generic frontend/rendering infrastructure; projects own cinematic policy.

> Engine-owned documentation of reusable application, input, audio, window and rendering abstractions in `Source/Frontend/`, plus the client render-target bridge in `Source/Client/`.

## Contract status

This source-backed guide explains current reusable frontend/rendering architecture. Project settings, generated script APIs, resource formats and serialized effect metadata retain their owning references' stability status. Native classes such as `Application`, `Renderer`, backend contexts, atlas allocators and client draw managers remain implementation surfaces unless the [Public Contract Index](../../reference/public-contract/index.md) states otherwise.

Projects own backend selection, authored effects, visual quality, target resolutions, performance budgets and platform acceptance. Engine owns backend-selection semantics, render-resource contracts, matrix/depth conventions and the validation routes here.

## Purpose

The frontend connects platform and engine runtime: windows, frame boundaries, input queues, touch/gamepad translation, audio devices, renderer selection and low-level backend objects. Clients use its stable interfaces rather than SDL, OpenGL, Direct3D or Web APIs directly.

Read this page together with:

- [Client Runtime](../runtime/client.md) for how `ClientEngine`, `SpriteManager`, `MapView`, and runtime managers use these services.
- [Image And Sprite Formats](../../how-to/content/image-format.md) for baked sprite loading, atlas placement, filter borders, hit masks, and image cache identity.
- [Font Formats And Text Layout](../../how-to/content/font-format.md) for bitmap-font texture loading, atlas placement, bind-time raster scaling, text layout, and rendering flags.
- [Build Workflow](../../how-to/build/) and [BuildTools Pipeline](../../reference/cmake-and-buildtools/pipeline.md) for configure/build composition.
- [Web Build, Packaging, and Browser Debugging](../../how-to/platforms/web-debugging.md) and [Android Build, Packaging, and Device Debugging](../../how-to/platforms/android-debugging.md) for platform package/debug flows.
- [Native, AngelScript, and Managed Debugging](../../troubleshooting/debugging.md) for native debugging and stack traces.

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
- `Source/Frontend/Rendering-Vulkan.cpp`
- `Source/Frontend/VulkanColorFormat.h`
- `Source/Frontend/VulkanColorFormat.cpp`
- `Source/Frontend/Rendering-SDLGpu.cpp`
- `Source/Frontend/Rendering-Null.cpp`
- `Source/Common/Settings.inc`
- `Source/Client/RenderTarget.h`
- `Source/Client/RenderTarget.cpp`
- `Source/Client/ResourceManager.h`
- `Source/Client/ResourceManager.cpp`
- `Source/Client/CritterFramesCache.h`
- `Source/Client/CritterFramesCache.cpp`
- `Source/Client/SpriteManager.h`
- `Source/Client/SpriteManager.cpp`
- `Source/Client/DefaultSprites.h`
- `Source/Client/DefaultSprites.cpp`
- `Source/Client/TextureAtlas.h`
- `Source/Client/TextureAtlas.cpp`
- `Source/Client/ModelSprites.h`
- `Source/Client/ModelSprites.cpp`
- `Source/Client/ModelSpriteLayout.h`
- `Source/Client/ModelSpriteLayout.cpp`
- `Source/Common/AnimationInfo.h`
- `Source/Common/AnimationInfo.cpp`
- `Source/Common/ModelBounds.h`
- `Source/Common/ModelBounds.cpp`
- `Source/Common/Geometry.h`
- `Source/Common/Geometry.cpp`
- `Source/Client/EffectManager.h`
- `BuildTools/cmake/stages/Packages.cmake`
- `Source/Tests/Test_Rendering.cpp`
- `Source/Tests/Test_VulkanColorFormat.cpp`
- `Source/Tests/Test_Geometry.cpp`
- `Source/Tests/Test_ModelBaker.cpp`
- `Source/Tests/Test_ImageBaker.cpp`
- `Source/Tests/Test_TextureAtlas.cpp`
- `Source/Tools/ImageBaker.cpp`
- `Source/Tools/SpriteMeshing.cpp`
- `Source/Tools/BakingReport.cpp`
- `BuildTools/cmake/stages/Init.cmake`

## Layer map

The frontend/rendering split has three layers:

1. **Application layer** (`Application`, `AppWindow`, `AppInput`, `AppAudio`, `AppRender`) owns platform services and frame boundaries.
2. **Renderer layer** (`Renderer` and its backends) owns GPU/null rendering resources: textures, draw buffers, effects, matrices, scissor state, presentation, and resize handling.
3. **Client drawing layer** (`SpriteManager`, `RenderTargetManager`, `EffectManager`, `MapView`) builds engine/game drawing operations on top of the renderer abstraction.

Most client code is renderer-agnostic: it requests sprites, effects, draw buffers, render targets and input events; the selected backend implements them.

The baker decodes image sources. At runtime, `DefaultSpriteFactory` converts baked RGBA containers to atlas-backed sprites; `TextureAtlasManager` allocates by `AtlasType`; renderers receive only texture-region uploads and draw data. [Image And Sprite Formats](../../how-to/content/image-format.md) owns this source-to-atlas contract; [Sprite Root Motion](../../how-to/content/sprite-root-motion.md) owns movement-driven per-frame offsets.

Bitmap-font descriptors remain raw runtime resources; their PNG/TGA images undergo normal baking. `FontManager` parses descriptors, uploads normal/bordered glyph regions to the font atlas and submits text through `SpriteManager`. Backends interpret neither FOFNT/BMFont syntax nor wrapping flags. [Font Formats And Text Layout](../../how-to/content/font-format.md) owns this boundary and exact draw/measurement behavior.

## Matrix convention

Engine render math uses one convention:

- `mat44` is the GLM matrix type defined in `Source/Common/Common.h`.
- Storage is column-major. Direct indexing is `matrix[column][row]`; translation is `matrix[3].xyz`; `glm::value_ptr(matrix)` can be copied into uniform buffers without transposing.
- Algebra uses column vectors. Compose transforms as `clip = Proj * View * Model * vec4(position, 1.0)`.
- Shader code follows the same convention with `ProjMatrix * vec4(...)`. Backend differences belong in matrix constructors and shader cross-compilation, not ad-hoc row/column transposes at call sites.

Use `RowMajor`/`ColumnMajor` only for explicit external-data conversion. Internal renderer, model, particle and geometry matrices should use role names (`ProjMatrix`, `ViewMatrix`, `ViewProjMatrix`, `WorldMatrix`), not storage order.

## Application initialization

`InitApp()` and `LoadAppSettings()` in `Source/Frontend/ApplicationInit.cpp` prepare global settings and application services before client, server or tool engine creation.

Windows installs console Ctrl+C/Ctrl+Break handling unconditionally after frontend initialization. It only latches quit; applications service `IsQuitRequested()` at safe exits. Tools/tests ignoring quit may finish current work, with no immediate-cancellation guarantee. POSIX registration is unchanged. Custom native loops needing cooperative early termination should poll `IsQuitRequested()`. See [Release Operations](../../how-to/release/operations.md#stop-safely).

Notable responsibilities:

- parse command-line and local configuration;
- load app settings from config/cache sources;
- initialize frontend globals once;
- optionally locate and call baking support through `FO_BakeResources` when resource baking is needed by the current app flow;
- prepare the app-level services used by clients, tools, and headless/test modes.

Server, mapper, editor, testing and package flows share frontend initialization where applicable, despite different flags or window modes; it is not exclusive to graphical clients.

`Common.NonInteractive = True` makes system dialogs unattended: errors/tracebacks are logged and initialized apps receive `RequestQuit(false)`, the desktop `Exit` button's failed-shutdown request. Before app creation, the process immediately exits with failure. It also covers direct `ShowErrorMessage` calls and debugger runs; game GUI behavior stays normal. Graphical frontends default to `False`.

`IsNonInteractiveMode()` in `Application.h` works before/after app creation. Headless always returns `true`, ignoring `Common.NonInteractive`; its `ShowErrorMessage` logs and directly requests failed shutdown. Graphical code owns unattended reporting and system dialogs. `Application.cpp` and `ApplicationHeadless.cpp` own messages; `ApplicationInit.cpp` owns early policy resolution and exception callbacks. `Render.HeadlessWindow` hides a graphical window without selecting the headless frontend.

Unattended graphical launchers should explicitly pass `--Common.NonInteractive True`. Initialization reads it before config, preventing missing/invalid config dialogs. During startup hooks, reporting reads live settings, protecting failures inside hooks when enabled. Every hook exit retains the final mode and clears the temporary settings borrow before move or unwind. Later fatal errors still exit with failure; nonfatal errors request ordinary shutdown under that resolved policy.

`AppInitFlags::StandaloneClient` identifies the standalone player-client application,
including its runtime DLL. Client engines embedded in a server window leave this
flag unset. It controls the standalone window's native-cursor policy, screen saver,
clear color, initial fullscreen and always-on-top behavior. Resource configuration
loads before frontend creation, preferring a server config and otherwise a client
config. The flag describes the application that owns these services; client
engines are created later and their presence alone does not set it.

After initialization, the client records the OS/version, CPU count, memory, SDL video driver, display mode and window metrics once in its log; a browser build also records its user agent. Each renderer names its selected device and driver, while degraded capabilities are warnings. These diagnostics describe the environment, not a guarantee that a particular GPU or platform passed release acceptance.

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

### `AppRender` / `IAppRender`

`AppRender` is the application-owned facade over the selected `Renderer`; client/tool consumers use it without downcasting to a GPU backend. It provides:

- resource creation through `CreateTexture()`, `CreateDrawBuffer()`, and
  `CreateEffect()`;
- projection access through `CreateOrthoMatrix()` and `GetProjMatrix()`;
- target and depth-range selection through `SetRenderTarget()` and
  `SetOrthoDepthRange()`;
- target clearing through `ClearRenderTarget()`;
- clipping through `EnableScissor()` and `DisableScissor()`;
- render-target orientation through `IsRenderTargetFlipped()`.

`AppWindow::GetRender()` exposes that real or virtual window's facade. Operations must use the owning window/engine instance; embedded-client render resources and settings are not host globals.

### `AppWindow` / `IAppWindow`

Window responsibilities include:

- size, screen size, position, display rect, focus, fullscreen state;
- minimize, blink, always-on-top, title, input grabbing, and destruction;
- distinguishing real OS windows from **virtual** windows (`IsVirtual()`) — host-composited embedded windows used by the multi-client host, whose render size is their own per-window virtual size rather than the shared logical screen size;
- resolving a native `WindowInternalHandle` for render backends;
- resolving a `HeadlessWindowStub` in headless/stub contexts.

### Display information for resolution selectors

`IAppWindow::GetDisplaySize()` returns desktop mode dimensions for the display containing the native window centre. `GetDisplayModes()` returns its unique positive SDL fullscreen width/height pairs, ignoring refresh-rate/pixel-format duplicates. Both query the current association each call, refreshing selectors after monitor moves. SDL display-mode units match existing monitor settings.

Managed clients use `Game.GetDisplaySize()` and `Game.GetDisplayModes()`. Virtual windows query the host main window's display. Null/headless/stub modes and native query failures return empty size/list. Callers own min/max filtering, standard-mode fallback and preservation of logical resolution. Queries follow the native window frontend's thread contract and alter neither window, logical resolution, fullscreen state nor refresh rate.

### `AppInput` / `IAppInput`

Input responsibilities include:

- polling queued `InputEvent` values;
- clearing and pushing events;
- mouse position control;
- screen keyboard enablement;
- clipboard text;
- gamepad state;
- normalization of mouse, keyboard, wheel, touch tap, touch double-tap, touch scroll, and touch zoom events.

Mouse input maps concrete platform button ids to script `MouseButton` values (`Left`, `Right`, `Middle`, `Ext0`/`Ext1`, ...). Unknown native buttons are ignored, never converted to primary clicks.

The client turns these lower-level events into script events in `ClientEngine::ProcessInputEvent()`.

SDL mouse-motion events primarily supply `InputEvent::MouseMoveEvent`. Where global coordinates exist (Windows, macOS, X11, whitelisted OS/2 drivers), `Application::BeginFrame()` also polls them while focused. If no motion event arrived that frame and position changed, it synthesizes a mouse-move event, keeping cursor/edge scrolling current outside the window. Host-to-active-window translation still maps embedded virtual clients through their display rect/aspect fit to local logical coordinates.

### `AppAudio` / `IAppAudio`

Audio responsibilities include:

- reporting whether audio is enabled;
- setting an audio stream callback;
- converting audio formats;
- mixing audio;
- locking and unlocking the audio device around critical sections.

[Audio](../../how-to/content/audio.md) and its generated [audio reference](../../reference/audio/index.md) own supported WAV/Ogg authoring profiles, AudioBaker delivery, effect/music lookup, playback handles, spatial updates, repeat, volume and audible validation. This page owns platform abstraction rather than game-facing audio authoring.

## Headless and stub modes

Two non-normal modes are important for tools, tests, CI, and platform staging:

- `Source/Frontend/ApplicationHeadless.cpp` supports headless operation where a real visible client window is not required.
- `Source/Frontend/ApplicationStub.cpp` provides stub implementations of render, input, audio, and window interfaces.

Stubs let tests and non-graphical flows exercise engine logic without a GPU, window or audio device; they are not full renderers. Tests requiring visible rendering should state that requirement explicitly.

### Frame pacing

Desktop client, Mapper and viewer loops use `FrameBalancer` (`Source/Common/Common.h`). `Render.VSync = true` disables pacing because presentation waits for display. Otherwise positive `Render.FixedFPS` caps via `precise_sleep` regardless of `Render.Sleep`; overruns shorten later waits with at most one second of debt. `Render.Sleep` applies only with `FixedFPS = 0`: zero yields, positive uses `coarse_sleep`, `-1` leaves it unpaced. `Source/Tests/Test_Common.cpp` covers these branches.

## Rendering abstraction

`Source/Frontend/Rendering.h` defines the renderer-facing types:

- `RenderType` — backend family selection.
- `EffectUsage` — effect category used when loading/compiling effects.
- `RenderPrimitiveType` — primitive topology used by draw buffers.
- `BlendFuncType` and `BlendEquationType` — blend-state configuration read from effect config.
- `DepthVariantType` and `EFFECT_DEPTH_VARIANTS` — the per-draw depth-state variant slot (see below).
- `Vertex2D` and `Vertex3D` — vertex layouts for sprite and model paths. For primitive batches uploaded through `SpriteManager::DrawPoints`, `PosX/PosY` are draw-area-local pixel coordinates (with the `draw_area` scroll offset subtracted), and `TexU/TexV` carry `PrimitivePoint::TexUV + draw_area.xy`; `DrawPoints` adds the draw-area top-left to whatever the caller authored. For world-stable per-fragment effects (dither, noise, gradient mapping), author the **same constant** `TexUV` on every vertex of a primitive batch — the absolute map-origin-anchored pixel position of the screen-anchor hex `_screenRawHex`. The fragment shader then reconstructs each fragment's true absolute world pixel as `gl_FragCoord.xy + InTexCoord`. With a constant on every vertex, varying interpolation is degenerate (no barycentric rounding can crawl the noise), and per-pixel `gl_FragCoord` supplies spatial variation. This is robust against camera scroll, zoom, fan-triangle deformation from smooth sprite movement, and the `from_hex.x` parity sensitivity of `GeometryHelper::GetHexOffset` on offset-row hexagonal grids. `MapView::LightFanToPrimitves` authors it via `GeometryHelper::GetHexOffset(mpos(0, 0), _screenRawHex)`. `Primitive_Light.fofx` consumes it (`worldPixel = gl_FragCoord.xy + InTexCoord`) to jitter the light's edge taper with world-stable noise; other primitive shaders ignore `InTexCoord`. The light fan also carries **normalized radial distance** in `PrimitivePoint::PointPosZ` (`LightFanToPrimitves`' `rim_dell`: 0 at the center, ~1 at the rim) → `InPosition.z`; `Primitive_Light.fofx` reads it as `Rim` and `smoothstep`s the outer band (`EdgeTaperStart`..1.0) to zero, so brightness rises gently *from zero* at the rim instead of ending in a hard constant-slope edge (very visible when the light moves).
- `RenderTexture` — backend texture/render-target resource, with blocking and requested-region readback paths.
- `RenderTextureReadback` — a pending CPU pixel snapshot; `ImmediateTextureReadback` holds pixels already available on the CPU.
- `RenderDrawBuffer` — vertex/index storage uploaded to the backend.
- `RenderEffect` — shader/effect object plus standard uniform/script-value buffers.
- `Renderer` — backend interface implemented by concrete renderers.

`Source/Frontend/Rendering.cpp` owns backend-independent draw-buffer allocation checks and effect parsing. It reads `Effect`/`EffectInfo` sections, pass counts, blending and script-visible buffers before backends consume shader files.

### Reading a texture back

`RenderTexture::GetTextureRegion(pos, size)` **blocks** for initialization, screenshots/dumps and other immediate-pixel callers, not repeated frame-path picking. Both readback paths require positive in-bounds rectangles.

`RequestTextureRegion(pos, size)` records a copy after earlier draws/clears at that request point. `RenderTextureReadback::TakePixels()` polls without waiting: `std::nullopt` until ready, then exactly one pixel transfer. Repeated taking or readback failure throws; discard failed requests before retrying, never treating unconfirmed pixels as ready. Layout/row order match `GetTextureRegion`. Requests and backend resources must stay within their owning renderer/context lifetime.

Requesting itself is not universally non-blocking; these fallbacks remain contractual:

| Backend | Request and completion |
| --- | --- |
| Null | CPU copy into `ImmediateTextureReadback`; ready immediately. |
| Direct3D 11 | `CopySubresourceRegion` to owned staging storage; `Map` with `D3D11_MAP_FLAG_DO_NOT_WAIT` polls completion. |
| OpenGL | Pixel-pack buffer plus `glFenceSync`; a zero-timeout `glClientWaitSync` polls completion. Requires sync, pixel-buffer and map-range support; Web and missing capabilities fall back to a blocking read at request time. |
| Vulkan | Host-visible copy in the recording frame command buffer, between render passes; completed-frame tracking or the frame fence establishes readiness without a request-time submit/wait. Outside frame recording, the request falls back to the blocking read. |
| SDL_GPU | Download transfer buffer in the current command buffer; a shared `SDL_QueryGPUFence` tracks completion. Only command buffers carrying readbacks acquire a submission fence; before submission the request is not ready. |

`ModelSprite::IsHitTest` reads a completed CPU `vector<bool>` alpha mask and requests refresh only inside sprite bounds; it never starts or collects GPU readback. Sprite updates prepare the first mask without picking. Later copies need a stale picture (`DrawToAtlas`) and refresh demand, at most once per 100 ms per sprite. Hover refreshes animated silhouettes; animation alone requests no copies. Every render update polls the single pending readback, keeping the last completed mask until replacement arrives. **Before the first mask is ready, hit testing returns false.** The silhouette can lag by refresh interval plus readback latency, without a completion deadline. Games must bound cursor-entity cache lifetime for stationary cursors to see new masks. This is client presentation/picking, not a server-authoritative hit or combat validation. Ordinary `AtlasSprite` masks use source pixels at load. `RenderTargetManager` no longer owns a last-pixel-pick cache or invalidation API.

`ModelSpriteHitMask` owns pending request, refresh demand/deadline and completed alpha mask. Request/collection exceptions release the request, mark stale, restore demand and propagate; later render updates retry under the copy interval. Redraw/demand during a pending request survive completion. `ModelSpriteHitMaskRetriesFailedReadback` covers first/replacement masks; `ModelSpriteHitMaskKeepsRedrawsWhileReadbackIsPending` pins redraw ordering. Deterministic `ModelSpriteHitMaskThrottlesRequestedCopiesButPollsPendingPixels` checks the 99/100 ms boundary, immediate polling and no demand-free copies. `ModelSpritePreparesItsHitMaskDuringRenderUpdate` checks first preparation/query-only picking with the Null renderer; visible backends separately require first-click checks with/without hover.

### Sprite and model atlas geometry

`EffectUsage::QuadSprite` is a historical effect slot, not four-vertex topology. All backends consume indexed triangle-list sprite buffers; source-backed `AtlasSprite` frames can emit baked silhouettes instead of implicit 4-vertex/6-index rectangles. No backend-specific polygon path exists.

Mesh coordinates use the exact selected-geometry bounding box. Baked frames retain original logical bitmap size and cropped-frame origin; sprite offset preserves the logical root. Screen position and atlas UV map the same cropped coordinate affinely, applying scaling, rotation, map projection, standing depth and egg flags to every vertex without unused texture rows/columns. Lighting retains the full-bitmap quad plane: `DrawSprites` supplies left/right colours; a cropped local X receives `lerp(left, right, clamp((localX + sourceOffsetX) / sourceWidth, 0, 1))`. This is normalized original-bitmap X (0 left, 0.5 centre, 1 right), not crop/contour position. RGBA8 `Vertex2D::Color` rounding can put intermediate vertices at most one channel unit from ideal quad interpolation.

Only ordinary full-image draws use baked polygon meshes. Every allocation retains a rectangular `GetAtlasRect()`, possibly smaller than and offset within the logical image. Region crops, tiled patterns, padded custom-effect/outline draws and mapper previews map it through `SourceOffset`. Region UVs stay normalized to the original image; transparent cropped margins are clipped from the destination. Polygon crops cannot shift/stretch GUI 9-slices, patterns, previews or source-region composition. Effects creating pixels outside the silhouette require a padded/quad path.

Runtime model sprites can use cropped quads with automatic layout. `.fo3d` rejects `DrawSize`/`ViewSize`; default render settings supply no fallback dimensions. `ModelAnimationInfo.foinfo` bounds schema version 2 provides aggregate root-space AABB, project-selected preview view AABB and per-animation AABBs. Under `FO_ENABLE_3D`, the common `EngineMetadata` loader parses/validates this companion once; rendering uses immutable registry records, never a second client parser or bounds cache.

The client projects enabled animation bounds through the active base transform over every continuous facing angle. Body plus projected shadow defines animation-wide `DrawRect` and logical scratch-frame dimensions. The separate view bound prefers `Animation.PreviewStateAnim` / `Animation.PreviewActionAnim`, then deterministic animation/static fallback; projection over all directions yields stable `ViewRect`. Logical frames tightly enclose the current animation, aligned to sprite scale rather than power of two. Ground root uses its exact projected pixel (`(-DrawRect.x, -DrawRect.y)`, exposed by `ModelInstance::GetFramePivot()`), not `(DrawWidth / 2, 3 * DrawHeight / 4)`. Low/centre-origin creatures reserve no tall empty area above a fixed anchor. Shadow-free view bounds ignore changing atlas crops, keeping names, coarse picking, transparent eggs and flying text steady across turns/animation changes.

View bounds start from the baked project-selected preview. Live weighted poses never contribute: per-frame vertex accumulation would let root motion enlarge the name rectangle across a clip. Selected children contribute baked link envelopes. `SelectModelViewBounds` substitutes the whole current animation-plus-link envelope only when its projected top is lower than the preview, lowering prone/corpse names without lifting them for raised weapons. Projects may still author `NameOffset`.

`SelectModelViewBounds` follows animation **downwards**: if the active clip's baked box tops out below idle under the same base transform/projection, it selects that whole box. Projected comparison handles imports with `RotX = +/-90`, where source Z points screen-up. Corpses/prone bodies would otherwise keep standing-height names; their greater width also requires more than replacing the top. Higher raised-weapon/swing bounds are intentionally ignored, so names never rise with swings. Both inputs are baked per clip, making the result constant within an animation.

Logical frames reuse a 2x scratch target. The factory caches matching sizes by LRU under a soft 8 x 1024 x 1024 colour-pixel budget (32 MiB RGBA plus backend depth); oversized frames occupy it alone. Cleanup releases all scratch targets, clearing the blit texture borrow before its owner; live atlas allocations/shared model materials survive. This bounds retained intermediate colour pixels, not total renderer memory or target count. Framing unions baked active-animation bounds with per-animation AABBs of selected direct non-particle child links with active parent clips. Missing matching clips or nested child rigs use aggregate link envelopes. Eight selected corners span the facing sweep; an affine inverse keeps the exact homogeneous row for division-free orthographic projection. Frames cover layer/equipment geometry without reserving rigid attachments' all-animation travel or traversing weighted vertices every frame. Only emitting particles extend them; dormant effects reserve nothing and expand boundedly on emission.

Internal `ModelSpriteBounds::Rect` contains baked geometry, projected shadow, live particles and effect-forced full frames. This dynamic renderer envelope sizes the frame and atlas crop.

`Game.GetDrawCritter3dBounds` exposes two stable layout rectangles. `DrawRect` conservatively bounds baked active clips, selected links and projected shadow for renderer-layout diagnostics. Semantic interface `ViewRect` normally uses project-selected preview bounds, substituting the complete lower/prone active clip when needed. Portraits/world overlays use it rather than the larger draw envelope. The separate pose rectangle was removed: it duplicated `DrawRect`'s active-model-occupancy contract without distinct behavior.

World-space emitted particles do not scale with models. Fitting internal `Rect` would repeatedly shrink the model while particles kept their size. Effects belong in frame bounds, not interface fitting.

If the exact envelope needs more logical space, the client expands and rerenders before copying. Root-relative interval unions prevent rounded adjacent pivots or live world-space particles from making stable frames oscillate. Signed anchors allow tight frames wholly on one side of the root, with pivots outside them; bounded retries still reject genuinely unbounded layouts.

GPU-free helpers use `AppRender::MIN_ATLAS_SIZE / FRAME_SCALE` (`MODEL_SPRITE_MAX_LOGICAL_FRAME_DIMENSION`) as portable logical ceiling; bake-host limits do not describe the game device. `Render.ModelSpriteMaxTextureWidth` / `Height` and the current atlas cap scratch textures. Larger envelopes draw cropped; bounded retries reject non-convergence within the cap. `ModelInstance::SetupFrame` rejects logical `draw_size * FRAME_SCALE` beyond current `AppRender::MAX_ATLAS_WIDTH` / `MAX_ATLAS_HEIGHT`, reporting model and both sizes before anonymous allocation failure. Default `.fo3d`-disabled meshes contribute to neither drawn nor pose bounds. Runtime honors that default like layer/attachment `DisableMesh`, keeping geometry within the baked budget.

Only selected regions enter the atlas. Crop origin adjusts sprite offset, preserving frame root, hit-test coordinates and map position. Active layer/child trees extend the project-selected preview view and aggregate lighting bounds. Animation changes may refresh `DrawRect`/scratch frame; `ViewRect` retains accumulated model/layer bounds instead of briefly using smaller root preview bounds, keeping names stable through turns. Left/right light colours sample root-relative crop endpoints on that envelope: scratch resizing preserves the mix, and wide gear avoids clamping to base-model endpoint colours.

Within one active animation/combined-mesh envelope, pose changes only enlarge slots. Identity changes when enabled body/movement tracks settle after transition, generated mesh composition changes or shadow coverage changes, permitting one shrink to the new stable envelope rather than lifetime accumulation. Direction is excluded, so turns retain the high-water slot. Reserve/copy new placements before publishing frame/crop/allocation; failed copies retain the old allocation and schedule retry. Active attached particles use SPARK's live render AABB after first update, the advertised canvas before it, and the entire current frame. Scratch resizing rebases emitted atlas-space particles before rerendering; non-default model effects also disable tight crops. This protects ordinary skinned output, not shader displacement: effects moving vertices beyond geometry need a separate conservative contract. `Render.ModelDirectDraw` keeps atlas preview/hit-test data while drawing visible geometry in scene.

`Game.DumpAtlases()` and mapper **Dump atlases** annotate read-back PNGs, never runtime textures: magenta triangle edges, cyan mesh vertices, yellow implicit quads and red X for explicitly empty baked frames. `AtlasSprite` owns mesh metadata; the live allocation borrows a nullable observer and clears it on release, preventing stale geometry after slot reuse.

`AtlasSprite` separates logical authored size/offset from atlas crops. Vertices use `SourceOffset` in logical-canvas coordinates; UVs stay crop-local. `GetSize()`, `GetOffset()`, scaling and hit tests retain source-image semantics despite removed transparent borders. Cropping saves texture memory without changing GUI layout, anchoring or input routing.

Allocation remains per image. Dynamic MaxRects `TextureAtlasLayout` retains overlapping maximal free rectangles, choosing short-side fit, long-side fit, then wasted area without rotation. The manager compares all existing atlases of the requested type before adding a page; ties retain the older page. Packed rectangles include the one-pixel border, preserving filtering padding, sprite pixels and UV calculation.

Mesh-texture pages start at 1024 x 2048 and grow for padded images within backend
limits; a 1024-wide model texture needs 1026 pixels including its border.

Font sheets, model materials, particle maps and Spine attachments consume rectangles rather than drawable polygons. Authored glyph/normalized UVs address the whole bitmap; consumers receive only atlas rectangles, no `SourceOffset`. They use `ResourceManager::LoadSpriteAsQuad`, reconstructing the logical canvas from baked mesh metadata before upload. Ordinary `AtlasSprite` loading would shift authored UVs through mesh padding/cropping. Runtime-generated model/particle sprites already have rectangular allocations and need no reconstruction.

Live sprites own engine `unique_del_*` handles to stable-address encapsulated `TextureAtlasLayout::Allocation`. Release clears the mesh observer and returns its rectangle to the free list in constant time. Free rectangles do not eagerly coalesce: on a placement miss, `DefragmentFreeRectangles` rebuilds the exact maximal set from live allocations before adding pages. Containment pruning runs only after growth well beyond the last pruned size, indexing keepers by coarse atlas cells to avoid hot-path scans. Surviving sprites, pixels and UVs never move; settings/resource serialization are unchanged.

`TextureAtlasManager::CleanupAtlases()` deletes empty pages and their manager-owned targets, including `OneImage`. `ResourceManager` calls it after cache eviction; new-page creation also cleans up. Pages with live allocations survive without pixel/UV movement. `ExpiredOneImageAtlasReleasesRenderTarget` and `AtlasCleanupReleasesOnlyEmptyPages` pin this boundary.

`Render.DrawWireframe` enables a backend-independent geometry overlay. `SpriteManager` copies submitted triangle edges after positioning, scaling, rotation, map projection and standing depth, drawing opaque magenta primitive lines over the normal sprite pass. It shows both ordinary-quad triangles independently of `SpriteMesh.Enabled`. Disabled by default, it changes neither sprite buffers, atlas textures nor baked resources.

## Render backends

The compiled `RenderType` inventory and selection surface are:

| `RenderType` | Selection | Current contract |
| --- | --- | --- |
| `Null` | `Render.NullRenderer` or a headless/stub path | Implemented CPU-only validation backend; no visible GPU output. |
| `OpenGL` | `Render.ForceOpenGL` or the last automatic GPU choice when compiled | Implemented native OpenGL/OpenGL ES/WebGL backend; render targets are flipped. |
| `Direct3D` | `Render.ForceDirect3D` or the first automatic Windows choice | Implemented Direct3D 11 backend; render targets are not flipped. |
| `Metal` | `Render.ForceMetal` | Direct Metal is a placeholder: the enum/platform flag exists, but forcing it throws `AppInitException`; there is no `Metal_Renderer`. |
| `Vulkan` | `Render.ForceVulkan` or the automatic choice before OpenGL when no earlier backend was created | Implemented dynamically loaded Vulkan backend; render targets are not flipped. |
| `SDLGpu` | `Render.ForceSDLGpu`, optionally with `Render.SDLGpuDriver` | Implemented explicit opt-in SDL_GPU backend over Vulkan/Metal/D3D12 drivers; render targets are not flipped. |

`Application` honors force selectors before compiled automatic selection. After backend creation, initialization failure is reported and it does not retry another backend; this is not health-probing fallback. Keep force selectors mutually exclusive. `Render.ForceSDLGpu` is never automatic. Direct Metal remains unavailable; supported Apple builds use SDL_GPU via `Render.ForceSDLGpu = True` and `Render.SDLGpuDriver = metal`.

Non-headless, non-Web builds enable Vulkan/SDL_GPU by default; `FO_DISABLE_VULKAN` and `FO_DISABLE_SDL_GPU` remove them. Vendored SDL headers/drivers require no external Vulkan SDK. The platform branch in `BuildTools/cmake/stages/Init.cmake` selects OpenGL, Direct3D and platform flags.

### Null renderer

`Source/Frontend/Rendering-Null.cpp` implements `Null_Renderer`, `Null_Texture`, `Null_DrawBuffer`, and `Null_Effect`.

Use it for tests, headless flows and GPU-free validation. Dimension, buffer-count, render-target-state and texture-region checks still catch API misuse.

Scissors allow negative origins and extension beyond targets, like GPU backends, for partially offscreen GUI. Width/height cannot be negative; texture read/write regions remain in-bounds. `NullRenderer` scissor sections cover both contracts.

### OpenGL renderer

`Source/Frontend/Rendering-OpenGL.cpp` implements the OpenGL/WebGL path.

Important behaviors:

- creates an SDL/OpenGL or WebGL context depending on platform;
- loads and validates required GL entry points/extensions;
- sizes the atlas against `AppRender::MAX_ATLAS_SIZE` and backend limits;
- creates textures, draw buffers, and effects;
- compiles/loads vertex and fragment shader content through the effect loader;
- reports render-target textures as vertically flipped (`IsRenderTargetFlipped() == true`);
- **Uniform blocks go through one shared bump-allocated UBO** (ES 3.0 / GL 3.1 compatible): per draw, every shader-required block — zero-initialized when the engine did not fill it, since a skipped block would keep a binding into storage orphaned by `Present()` — is packed contiguously, uploaded with one `glBufferSubData`, and bound per pass with `glBindBufferRange` (replacing up to ~8 tiny per-draw `glBufferData` re-specifications of per-effect UBOs);
- **`SetRenderTarget` elides redundant re-selects** of the applied target (bind, viewport, aspect-fit and ortho recompute are skipped); the cache is invalidated on resize and cached-texture destruction, and mid-frame texture creation restores the *current* target's framebuffer, not unconditionally the base one.

Inspect OpenGL for WebAssembly/WebGL behavior; pair changes with [Web Build, Packaging, and Browser Debugging](../../how-to/platforms/web-debugging.md) validation.

### Direct3D renderer

`Source/Frontend/Rendering-Direct3D.cpp` implements the Direct3D 11 path.

Baked `-dxbc` loads without client-side HLSL compilation or `d3dcompiler_47.dll`. Hardware normally requires feature level 10.0. A 2D-only build may accept 9.3 with `Baking.Direct3DLevel9Shaders`-baked effects; 3D builds never create 9.3 devices. That path caps atlases at 4096 pixels and uses unindexed point-list draws. Levels 9.1/9.2 are unsupported.

Device requests try 11.1 first. If an older Direct3D 11.0 runtime rejects the list with `E_INVALIDARG` (notably Windows 7 without its platform update), the engine retries the same hardware/WARP request without 11.1. Swap chains use the chosen adapter's DXGI factory; DXGI 1.1 rejects a separate factory. Other failures retain normal handling. Logs include feature level and available DXGI adapter, vendor/device IDs, user-mode driver version and dedicated video memory.

Important behaviors:

- creates D3D device/swap-chain/render-target resources;
- leaves the refresh rate unspecified for the windowed swap chain so DXGI follows the desktop compositor instead of requiring one hard-coded display mode;
- creates textures, staging textures, draw buffers, constant buffers, and effects;
- loads vertex/pixel shader content through the effect loader;
- handles resize by recreating backbuffer/depth resources;
- reports render-target textures as not flipped (`IsRenderTargetFlipped() == false`).

**Missing hardware is refused by default.** Without hardware, DXGI's WARP software rasterizer renders every frame on the CPU, starting an unplayable client. `Render.AllowSoftwareRenderer` (default off) explicitly allows it for diagnostics/headless machines; logs name it `Warp`. Otherwise device creation fails explicitly instead of presenting an apparently playable client.

Validate Windows-specific Direct3D changes through a Windows embedding-project build/debug flow.

### Direct Metal placeholder

`FO_HAVE_METAL`, `RenderType::Metal`, `Render.ForceMetal` and the SDL Metal window flag exist in the platform/application surface, but no direct `Metal_Renderer` is implemented. Selecting its compiled `Render.ForceMetal = True` branch immediately throws `AppInitException`. Direct Metal is unsupported and must not be advertised or used as release evidence.

SDL_GPU's separate implemented Metal backend uses `Render.ForceSDLGpu = True` and `Render.SDLGpuDriver = metal`. Apply the same visible, shader, interaction and performance gates as for its Vulkan/D3D12 drivers.

### Vulkan renderer

`Source/Frontend/Rendering-Vulkan.cpp` builds by default except with `FO_DISABLE_VULKAN`, headless-only or Web. SDL3's vendored headers (`ThirdParty/SDL/src/video/khronos`, SYSTEM with `FO_HAVE_VULKAN`) need no SDK or `find_package(Vulkan)`. `Render.ForceVulkan` selects it; automatic selection uses it only if no earlier backend is configured.

`Rendering-Vulkan.cpp` uses `VK_NO_PROTOTYPES`, neither linking `vulkan-1.lib` nor importing `vulkan-1.dll`. `SDL_Vulkan_LoadLibrary`/`SDL_Vulkan_GetVkGetInstanceProcAddr` bootstrap `vkGetInstanceProcAddr`; an X-macro table loads globals with null instance and remaining functions from the created instance, like `SDL_GL_GetProcAddress` for OpenGL. Missing runtime fails at `SDL_Vulkan_LoadLibrary` only when Vulkan is selected; other backends can start.

Design and important behaviors:

- **Single queue, two frames in flight.** `VULKAN_FRAMES_IN_FLIGHT` (= 2) frame slots each own a command buffer, fence, acquire semaphore, descriptor pool, persistently mapped uniform bump buffer, staging ring and deferred-destroy queue. `BeginFrame()` advances the slot, waits its fence (normally instant, replacing `vkQueueWaitIdle` so CPU frame N overlaps GPU frame N-1), flushes deferred destroys, acquires an image, resets fence and pool, refreshes aliases (`CommandBuffer`, `FrameDescriptorPool`, `FrameUniformBuffer`, …), clears the image and starts the pass. `EndFrame()` ends the pass, submits with the slot fence and image render-complete semaphore, and presents. Render-complete semaphores are per image, never re-signaled while presentation may wait; acquire semaphores are per slot. A fence signal covers all earlier queue submissions, anchoring per-slot reuse.
- **Deferred destroys are per frame slot.** `Destroy*Safe(...)` enqueues into the *current* slot, flushed after its fence wait, when both referencing frames are complete. Distinct names are needed because non-dispatchable handle typedefs collapse to one integer on 32-bit targets. All handles use `VK_NULL_HANDLE`, not `nullptr`, supporting pointer- and integer-backed ABIs. Swapchain recreation settles the device (`vkDeviceWaitIdle`), flushes queues and rebuilds sync objects.
- **Texture uploads and requested readbacks record into the frame command buffer; blocking readbacks flush it.** `UpdateTextureRegion` suspends the render pass and records barrier → copy → barrier, preserving prior clears and uploads through the staging ring. `RequestTextureRegion` records its copy between passes without a mid-frame submit/wait; `TakePixels` checks the completed-frame index or submission fence. `GetTextureRegion` calls `FlushFrameCommandBufferMidFrame()` (submit prefix, wait idle, resume recording) before its immediate staging copy. Operations outside frame recording use the immediate path. Preserve these ordering and readiness boundaries for immediate-queue operations.
- **Dynamic geometry uses per-draw-buffer, per-frame-slot ring pools.** Each `DrawBuffer::Upload` takes the next buffer from the draw buffer's growable ring of persistently mapped HOST_VISIBLE buffers for the current slot (one ring buffer per upload in a frame, preserving geometry snapshots of earlier pending draws). A ring resets on its first acquire in a new frame, after waiting for its slot fence, so every buffer is GPU-free. Buffers reallocate only on capacity growth, making steady-state uploads pure memcpy with no `vkCreateBuffer`/`vkAllocateMemory`/`vkFreeMemory` traffic (per-upload churn previously dominated backend CPU frame cost at ~25 ms/frame in crowds). Static buffers keep a one-off staging copy to device-local memory.
- **Shaders are baked with `highp` floats.** The baker emits ES shaders with `precision highp float`. `mediump` becomes SPIR-V `RelaxedPrecision`, ignored by desktop GL/D3D but honored as FP16 by NVIDIA Vulkan; large uniforms (frame seconds, world-anchored UVs) overflow half-float range (max 65504), collapsing consumers (time-driven weather/atmosphere post-processing) to black on Vulkan only.
- **Back-buffer target metrics follow resizes without `SetRenderTarget`.** `ApplySwapchainTargetMetrics()` recomputes letterboxed viewport, logical target size and projection for back-buffer rendering from `SetRenderTarget(nullptr)`, `OnResizeWindow()`, and after deferred swapchain recreation when the back buffer is active. The server host UI renders ImGui straight into the swapchain and never calls `SetRenderTarget`; without this refresh a post-init window or logical-size change leaves a stale projection and the UI draws shrunken into a corner (Direct3D gets the same refresh by ending `OnResizeWindow` with `SetRenderTarget(nullptr)`).
- **Two fixed descriptor set layouts.** **Set 0 holds uniform buffers** and **set 1 holds combined image samplers**, each with a fixed binding count. Per draw, the backend allocates one descriptor set per layout from a per-frame pool (reset every `BeginFrame()`) and writes uniforms into one host-visible **bump-allocated** buffer. Binding indices come from the effect's reflected `EffectInfo`.
- **Authored `.fofx` descriptor-set contract.** Every effect shader **must** declare uniform blocks with `layout(set = 0, binding = N, std140)` and samplers with `layout(set = 1, binding = N)`. Omitting `set` defaults every resource to set 0, colliding samplers with uniform buffers and producing `VkDescriptorType` mismatch (`VUID-…-layout-07990`) and "descriptor never updated" (`VUID-vkCmdDrawIndexed-None-08114`) validation errors, leading to device loss. Engine core/embedded effects follow this; embedding-project effects must too. OpenGL/Direct3D ignore `set` because UBOs and samplers use separate namespaces.
- **Uniform completeness.** Unlike OpenGL/D3D, where an unbound UBO retains its last value, a per-draw descriptor set must write every uniform block used. The backend zero-initializes every required-but-unset standard buffer (egg, sprite-border, time, random, script, camera, …) before upload.
- **Surface format.** Prefer `VK_FORMAT_B8G8R8A8_UNORM`, then `VK_FORMAT_R8G8B8A8_UNORM`, with `SRGB_NONLINEAR`, advertised by `vkGetPhysicalDeviceSurfaceFormatsKHR`. A lone `VK_FORMAT_UNDEFINED` allows either only in that color space; `_SRGB` is not UNORM. The context format survives recreation; incompatibility is rejected before old swapchain deletion. Textures/views and the shared render pass use it. CPU upload and both readbacks retain RGBA, alpha and row pitch; only BGRA swaps R/B.
- **Present mode honors `Render.VSync`.** `VSync = true` (or a surface with no better mode) uses `VK_PRESENT_MODE_FIFO_KHR`; with VSync off the swapchain prefers `IMMEDIATE` (uncapped, possible tearing), then `MAILBOX` (uncapped, no tearing), matching other backends. The chosen mode is logged at swapchain (re)creation (`Vulkan swapchain present mode: …`). A hardcoded FIFO would silently vsync-lock the backend and make cross-renderer frame-rate comparisons meaningless.
- **Orientation.** Unflipped targets (`IsRenderTargetFlipped() == false`, like Direct3D) share Y-up ortho with other backends. `GetProjMatrix()` drives CPU model camera math (`ModelSprites`/`ModelInstance`); Y-negation clips 3D. The negative-height viewport (core Vulkan 1.1; instance `VK_API_VERSION_1_1`) compensates Y-down with the former front-face winding. Android uses `VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR`; its compositor rotates unrotated frames. Rotation-related `VK_SUBOPTIMAL_KHR` is accepted; `VK_ERROR_SURFACE_LOST_KHR` rebuilds surface, swapchain and sync on an idle device once the native window returns, while the renderer pumps events and the application handles termination.
- **Point primitives.** Effect shaders cross-compile to HLSL/MSL via spirv-cross, which cannot express `gl_PointSize`, so a `POINT_LIST` topology (which Vulkan requires `PointSize` for) maps to `TRIANGLE_LIST`. Current content does not use point primitives; revisit if real point rendering is needed.
- **Physical device selection.** The backend prefers a discrete GPU exposing a graphics+present queue family for the surface and the swapchain extension, rather than blindly taking the first enumerated device.
- **Validation.** When `Render.RenderDebug` is set (or in a debug build) and `VK_LAYER_KHRONOS_validation` is available, the backend enables it and routes messages to the log as `[VkLayer/…]` through a `VK_EXT_debug_utils` messenger. A correct change should run with zero validation errors.

Validate on a working Vulkan runtime, adding Khronos validation layer for validation output. Run a visible client with `Render.ForceVulkan=True Render.RenderDebug=True`; require zero `[VkLayer/...` errors. Engine builds need no Vulkan SDK.

### SDL_GPU renderer

`Source/Frontend/Rendering-SDLGpu.cpp` is an opt-in SDL3 `SDL_GPU` backend for Vulkan/Metal/D3D12; vendored SDL3 ships all three drivers. Enabled by default (`FO_HAVE_SDL_GPU`) except headless-only/Web, it needs no external SDK and can be removed with `FO_DISABLE_SDL_GPU`. `Render.ForceSDLGpu` selects it; automatic selection remains unchanged, never defaulting to it. Optional `Render.SDLGpuDriver` pins `vulkan`/`metal`/`direct3d12`; `Render.RenderDebug` enables debug mode, including Vulkan validation on Vulkan.

Design and important behaviors:

- **Immediate-mode contract on explicit passes.** `Context` keeps at most one render/copy pass open, opens passes lazily, delays clears until a load-op, and uploads through cycled transfer buffers. Blocking `GetTextureRegion` submits and waits a fence. `RequestTextureRegion` materializes pending clears and records a download in the current command buffer without that wait; `Present()` acquires a shared submission fence only for command buffers with pending readbacks. `TakePixels()` polls the fence, and a blocking flush marks readbacks on its completed buffer ready.
- **Backbuffer proxy.** The window backbuffer is never rendered directly: `SetRenderTarget(nullptr)` targets an RGBA8 proxy texture (letterbox math shared with other backends), and `Present()` blits the proxy to the acquired swapchain texture, keeping mid-frame flushes safe and pipeline color formats uniform.
- **Per-effect pipeline cache.** Immutable graphics pipelines are cached per effect, keyed by pass, topology, depth-target presence, `DisableBlending`, and `DisableCulling`.
- **Consumes SDL-convention baked flavors, not native `-spv`.** SDL_GPU mandates per-stage descriptors (vertex samplers = set 0 / UBOs = set 1, fragment samplers = set 2 / UBOs = set 3), unlike native Vulkan's 2-set convention (UBO = set 0, sampler = set 1). The baker emits `-spv_sdl` — native SPIR-V with decorations rewritten for SDL — plus remapped `-msl_*` and `[EffectInfoSdl]` (per-stage sampler/UBO counts and dense slot indices). Native `-spv` (used by `Rendering-Vulkan`) is untouched. The backend picks `-spv_sdl` for Vulkan or `-msl_*` for Metal via `SDL_GetGPUShaderFormats` and reads `[EffectInfoSdl]`.
- **Push-style uniforms.** Uniform data is pushed with `SDL_PushGPU{Vertex,Fragment}UniformData` (at most 4 slots per stage) and re-pushed every draw; public uniform optionals keep their last value to emulate persistent buffers on other backends. The baker enforces the 4-UBO-per-stage limit at bake time.
- **`ProjBuf`/`MainTexBuf` are caller-owned when set, renderer-derived otherwise.** `DrawBuffer` auto-fills `ProjBuf` from current 2D ortho and `MainTexBuf` from bound texture size **only when the caller has not supplied them** (`_needX && !X.has_value()`), then `reset()`s those two after the draw. This mirrors native Vulkan and is load-bearing for 3D: `ModelSprites`/`ModelInstance` set `ProjBuf` to the per-frame model projection before drawing a critter to its atlas; overwriting it with 2D ortho projects the skinned mesh off-screen, so nothing rasterizes into the model atlas and 3D critters render as name-plates only (the "characters not drawn in SDL" bug). Other externally fed buffers (`EggBuf`, `ModelBuf`, …) are auto-derived only behind `!has_value()` and keep their last value.
- **Shares engine-wide black-map fixes.** Reusing the same baked SPIR-V pipeline (`precision highp float`) and epoch-based shader-time wrap in `EffectManager::PerFrameEffectUpdate`, the SDL_GPU backend inherits both Vulkan fixes (half-float overflow and `sin(large accumulated time)` NaN) and does not reproduce the black-map failure.
- **Point primitives, orientation, depth.** `POINT_LIST` maps to `TRIANGLE_LIST` (shaders lack `gl_PointSize`), mirroring native Vulkan. `IsRenderTargetFlipped()` is `false`, and ortho uses the `[0,1]` depth convention. Depth targets use `D24_UNORM` when supported, otherwise `D32_FLOAT`. Max atlas size is fixed at 4096 (SDL_GPU exposes no texture-size query).

Submit/wait failure marks every command-buffer readback failed. Later `TakePixels()` polls throw instead of staying pending or mapping unconfirmed transfer bytes. Only successful waits make a blocking flush's readbacks ready. Once rendering resumes, consumers may discard failed requests and record new ones.

Validate SDL_GPU with `Render.ForceSDLGpu=True Render.RenderDebug=True` (Vulkan-driver validation), checking a visible map and GUI without validation errors alongside the default backend. Pin `Render.SDLGpuDriver` for driver-specific acceptance.

## Render targets and client bridge

`Source/Client/RenderTarget.h` and `Source/Client/RenderTarget.cpp` bridge high-level drawing code to backend textures.

`RenderTargetManager` responsibilities:

- create render targets with optional depth and linear filtering;
- allocate backend `RenderTexture` objects through `IAppRender::CreateTexture()`;
- preserve/restore the previous backend render target while allocating and clearing a new target;
- maintain a render-target stack through `PushRenderTarget()` and `PopRenderTarget()`;
- clear the current render target;
- resize render targets;
- delete render targets and clear the stack;
- dump render-target textures for debugging.

`MapView`, `SpriteManager`, `ModelSpriteFactory` and `ParticleSpriteFactory` use targets for map layers, light buffers, model/particle atlases, hit tests and offscreen composition.

The manager owns all created targets; `MapView` borrows map/light/indoor-mask targets and releases all three in `OnDestroySelf()`. First flush queued sprite draws while textures live. Before mask deletion, `EffectManager::ClearIndoorMaskTexture()` clears matching `IndoorMaskTex` borrows in all cached effects, including inactive map-flush effects. Other maps' masks and surrounding target stacks survive. `GetRenderTargetCount()` counts manager-owned targets for lifecycle diagnostics, not backend memory or driver residency.

`Source/Tests/Test_ClientEntityLifetime.cpp` covers repeated default/disabled-mask/direct-draw release (`MapViewRenderTargetsAreReleasedOnDestroy`) and queued draws, cached effects, another live map and outer target (`MapViewDestroyClearsOnlyItsCachedIndoorMaskReferences`). Hardware memory and visible transitions require separate acceptance.

`MapView::DestroyRenderTargets()` serves unload and construction-failure cleanup; retained script handles never delay retirement. `ClientMapUnloadReleasesRenderTargetsWithRetainedHandles` and `ClientMapConstructionFailureReleasesRenderTargets` cover both. See [client lifetime](../runtime/client.md#map-unload-and-native-storage-lifetime) for immediate native buffer/pool retirement.

At local-map load, `View.MapRenderTargetScale` sets map/light/indoor-mask dimensions to logical screen size times scale, clamped to the renderer's texture limit. Larger views use multiple chunks.

Light compositing covers only the current chunk's drawn area, plus target padding and a one-eighth-chunk shake/refraction margin. `MapView::GetMapCompositeRect` supplies both ordinary `FlushLight` and custom fog-slot paths. When zoomed in, this avoids flushing the whole oversized target while preserving effect-readable edge pixels.

GUI object trees, screen traversal and any cached screen-tree query results belong to the embedding project; [GUI Integration Boundary](../../how-to/runtime/gui.md) defines the split. Engine supplies frame time, input and sprite hit-test primitives. Screen-tree cache keys and invalidation for activation, geometry, ordering, scroll, crop, transparency, hittability, language/resolution and persistent movement are project policy, including the distinction from transient draw-time offsets.

Model-attached SPARK particles keep simulation-space positions while emitters follow attachments. Non-identity resource root transforms select position-plus-facing rather than full-bone inheritance: lingering particles stay world-stable during model movement, new ones spawn at the current attachment. Model movement is subtracted in particle model space before camera rotation/projection; setup-positive and draw-negative offsets cancel for newly emitted particles.

## Screen size, resolution, and letterboxing

Two distinct sizes drive client rendering:

- **Logical screen size** — current `IAppWindow::GetScreenSize()` (script `Game.ScreenSize`). Startup `View.ScreenWidth`/`View.ScreenHeight` seed this state; `Render.Fullscreen` seeds live fullscreen state. These settings remain immutable. The main target `_rtMain` (`SpriteManager`), projection and GUI/ImGui use the live logical viewport.
- **Backbuffer (framebuffer) size** — the actual output surface: the OS window's pixel size in windowed mode, the monitor size in fullscreen, or an embedded client's virtual render texture in the multi-client host.

The game always renders to `_rtMain` at logical size. The final blit (`Renderer::SetRenderTarget(nullptr)`) centers and scales `_rtMain` to the backbuffer with aspect ratio preserved; bars occur only for aspect mismatch. Fullscreen must scale the chosen logical resolution proportionally. Equal sizes yield a bar-free 1:1 blit. `_rtMain` follows `GetScreenSize()` on screen-size-changed events. Main-window dispatchers distinguish logical changes (`OnScreenSizeChanged`) from physical/window changes (`OnWindowSizeChanged`).

Script offscreen surfaces (`Game.ActivateOffscreenSurface` / `Game.PresentOffscreenSurface`) use logical coordinates because scripts draw while `_rtMain` is active. Pools allocate at `SpriteManager::GetScreenSize()` and resize before reuse after logical changes; otherwise monitor-noise GUI effects can clip content beyond the old resolution. The active `SpriteManager` scissor stack applies when flushing to offscreen surfaces and `_rtMain`, preserving a cropped subtree's viewport inside the effect.

### Windowed

The main-window pixel-resize handler updates `ScreenState.Size` from reported pixels only for events not classified as fullscreen/backbuffer resizes. It emits `OnWindowSizeChanged`, and `OnScreenSizeChanged` only if logical size changed. `Game.SetResolution(w, h)` first calls `SetScreenSize` for logical dimensions, then requests native resizing only for non-fullscreen, non-virtual clients. A follow-up event reporting that logical size causes only a window-size notification, avoiding duplicate GUI/map refresh. Startup settings stay unchanged.

### Fullscreen (borderless desktop)

`SDL_SetWindowFullscreenMode(window, nullptr)` keeps the framebuffer at monitor size, never a smaller selectable resolution. Fullscreen resolution is logical: `Game.SetResolution` changes it through `SetScreenSize`, then aspect-preserving blitting scales to the monitor. Fullscreen startup/toggles/`SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` update renderer/backbuffer only; never overwrite `ScreenState.Size` or fire `OnScreenSizeChanged`, which would collapse chosen resolution to monitor size. `AppWindow::ToggleFullscreen()` marks transitions before SDL calls because queued events may arrive under old OS/window flags. Scaling stays proportional, with bars only for aspect mismatch.

SDL documents that `SDL_SetWindowSize` has no effect while fullscreen/maximized; it cannot resize the live fullscreen framebuffer. Native non-virtual `Game.SetResolution` still records pending windowed size while fullscreen. On fullscreen exit, `SpriteManager::ToggleFullscreen()` applies it and re-centers using accumulated resolution delta. Fullscreen preserves aspect; windowed mode restores the last selected resolution.

### Embedded clients in the multi-client host (virtual windows)

`ServerApp` hosts embedded clients in `Single`/`Tile`/`Cascade` layouts through `Spawn Client`. Each owns an engine instance, `GlobalSettings` and virtual `AppWindow` (`IsVirtual()`), which:

- keeps physical virtual-window size (`_virtualSize`, `GetSize()`) separate from logical client resolution (`_virtualScreenSize`, `GetScreenSize()`);
- renders game content into `_rtMain` at the logical size, then aspect-fits that render target into `_virtualRenderTex` at the physical virtual-window size;
- is composited by the host: `ServerApp` draws the client's virtual render texture aspect-fitted and centered into a per-client display rect (`SetDisplayRect`), and maps input back through that rect, `_virtualSize`, and the same aspect-fit content rect used for rendering so black bars do not skew client-local mouse coordinates.

Each embedded client keeps live logical size in its virtual `AppWindow`: `SetScreenSize`/`GetScreenSize` use `_virtualScreenSize`. `SpriteManager::SetScreenSize` forwards to that window before handlers run; it does not write startup settings. Virtual `SetResolution` skips `SetWindowSize`, and logical resizing leaves `_virtualSize`, the render texture and host layout unchanged. The standalone client shares its startup settings instance with `GetApp()->Settings`; its main window reads live size from `Application::ScreenState`.

An embedding project's `OnScreenSizeChanged` handler owns GUI relayout and re-centering from the owning client's live logical viewport, exposed by `Game.ScreenSize`; docking and anchoring remain project policy. `_rtMain`/`GetScreenSize()` and the GUI viewport must follow that logical size. Immutable `View.ScreenWidth`/`View.ScreenHeight` startup defaults are not live layout coordinates.

Actual logical screen-size changes instantly recenter local-map viewports on the chosen critter in standalone, fullscreen and virtual clients. `MapView` must use logical client size, not OS/backbuffer pixels; final target blitting owns fullscreen scaling.

## Effects and shader data

[Effect Format](../../how-to/content/effect-format.md) and the generated [effect-format reference](../../reference/effect-format/index.md) own `.fofx` sections, pass/render state, vertex inputs, resources, descriptors, baked artifacts, path-cache identity, script-value lifetime and authoring validation. This page owns frontend/render-pipeline participation.

`RenderEffect` owns standard buffers used by render paths:

- projection/main texture data;
- transparent egg parameters;
- sprite border parameters;
- time/random/script values;
- camera/model/model-texture/model-animation data.

`EffectManager` (`Source/Client/EffectManager.h`) loads minimal/default effects, resolves script selections, writes script-value buffers and updates each frame. Scripts write one `ScriptValueBuf` float through `Game.SetEffectScriptValue(...)`, or a contiguous range through `Game.SetEffectScriptValues(effectType, effectSubtype, valueStartIndex, values, valuesOffset = 0, valuesCount = -1)` to avoid repeated parameter-block native calls. Both validate the selected effect, require a `ScriptValueBuf` declaration and enforce `EFFECT_SCRIPT_VALUES` bounds.

**Shader time is session-relative and wrapped.** `TimeBuf` (`FrameTime.x` / `GameTime.x`, seconds) rebases to the first rendered frame and wraps at 8192 s in `EffectManager::PerFrameEffectUpdate`. It is periodic animation phase, not an absolute clock. Raw steady time counts seconds since boot; embedded clients can reach 10^5–10^6 s, where fp32 `fract()`/hash/`sin` steps and granularity exceeds frame delta (`sin(t * 76)` breaks within a day). Exact wrapping keeps granularity below 1 ms for any session, costing one phase pop per ~2.3h; consumers must confine this to noisy/ambient math. Still wrap hash-lattice time locally (`mod(p, period)` fog noise), and treat script `ScriptValueBuf` accumulators likewise: they quantize/freeze once ulp exceeds tick increment.

When adding an effect feature, document whether the change belongs in:

- effect config parsing in `Rendering.cpp`;
- backend shader loading/drawing in the affected OpenGL, Direct3D, Vulkan, or
  SDL_GPU implementation;
- client effect selection/update code in `EffectManager`;
- map/client draw sequencing in `MapView` or `SpriteManager`.

### Minimal-profile base effects

Engine base effects live in `Resources/Core/Effects/`, loaded as draw-slot defaults by `LOAD_DEFAULT_EFFECT` in `Source/Client/EffectManager.cpp`. Bootstrap `Resources/Embedded/Effects/` effects compile into binaries for rendering before external packs mount. Every `.fofx` starts with a `#` comment header describing purpose, slot and mechanism.

Base shaders support the optional Direct3D feature level 9.3 profile: no `gl_FragCoord`/position-semantic reads, derivatives (`dFdx`/`dFdy`/`fwidth`), texture-size queries or dynamic array/vector indexing. Bakers emit GLSL 330, GLSL ES 300, Metal and HLSL Shader Model 4.0, compiling HLSL to `-dxbc`; opt-in 9.3 adds `Aon9` for non-model effects. Engine bases stay minimal, per `Profile: minimal` headers. Normal Direct3D still requires feature level 10.0 or newer.

Default slot → effect mapping (`Source/Client/EffectManager.cpp`): `Font`/`Iface`/`Generic`/`Critter`/`Rain` → `2D_Default`; `Roof`/`Tile`/`Flat` → `2D_NoDepth`; `Primitive` → `Primitive_Default`; `Light` → `Primitive_Light`; `Fog` → `Primitive_Fog`; `FlushPrimitive`/`FlushMap`/`FlushLight`/`FlushFog`/`FlushRenderTarget` → the matching `Flush_*`; `SkinnedModel` → `3D_Skinned`; `ImGui` → the `ImGuiDefaultEffect` setting (`ImGui_Default`). `2D_WithoutEgg`, `3D_NormalMapping`, `Flush_Map_BlackWhite`, `Font_Default`, `Interface_Default` and `Particles_*` are selected per draw/mesh or by the particle system rather than fixed slot defaults.

`FlushMap` turns the intermediate map target into a completed viewport layer. Its RGB is already alpha-composited: never multiply it by target alpha or carry that coverage into the completed frame. `Flush_Map` and optional `Flush_Map_BlackWhite` write opaque alpha (`1.0`); project `FlushMap` overrides must preserve this. Generic `FlushRenderTarget` still preserves RGBA for authored model, particle, GUI and other offscreen alpha.

Projects targeting richer hardware keep advanced-profile copies under the same resource names in packs baked after `Core`/`Embedded`. Project copies shadow runtime bases, retaining Engine minimal fallback. Rich copies may use `gl_FragCoord`, derivatives, per-fragment lighting and similar features; Engine bases may not.

## Per-effect depth state and the shared map depth buffer

Effects carry per-pass depth state parsed from the `.fofx` `[Effect]` block:

- `DepthWrite` (default `True`) → `RenderEffect::_depthWrite[pass]` (depth write mask).
- `DepthFunc` (default `Always`) → `RenderEffect::_depthFunc[pass]` (`DepthFuncType`: `Always`/`Never`/`Less`/`LessEqual`/`Equal`/`GreaterEqual`/`Greater`/`NotEqual`). `Rendering-OpenGL.cpp` and `Rendering-Direct3D.cpp` translate it (`ConvertDepthFunc`), and backends diverge in NDC-Z (`[-1,1]` GL/GL-ES vs `[0,1]` D3D), so depth-dependent effects must be validated on both.
- `DepthVariants` (default `False`) → `RenderEffect::_depthVariants`. Opting in lets a **single draw** choose a depth state other than the declared one through `RenderEffect::DepthVariant` (`DepthVariantType`: `FromEffect`/`TestWrite`/`TestNoWrite`/`NoTestWrite`/`NoTestNoWrite`, where `Test` reuses the effect's own `DepthFunc` and `NoTest` replaces it with `Always`). Transparent geometry can carry per-item depth intent orthogonal to the shader — a particle system stores depth-test and depth-write flags per emitter node — and separate effect files would multiply with every other shader feature.

  Resolved state uses a **slot** (`ResolveDepthVariantSlot`, `EFFECT_DEPTH_VARIANTS` = 4) for prebuilt objects: `Rendering-Direct3D.cpp` creates `ID3D11DepthStencilState` per used slot, `Rendering-Vulkan.cpp` a pipeline per `(pass, primitive, blend, slot)`, `Rendering-SDLGpu.cpp` includes slot in its lazy cache key, and `Rendering-OpenGL.cpp` applies resolved state. Unbuilt resolved slots throw, preventing silent depth disable in Direct3D, stale Vulkan pipelines or OpenGL divergence. Resolved-state identity lets effects without declared variants accept equivalent requested states matching their own built slot. `RenderEffect::CanBatch` compares `DepthVariant`, keeping differently resolved draws separate.

## The scene background snapshot

Refraction cannot read its current draw target. `SpriteManager::AcquireSceneBackground()` copies it into a target-sized render target and returns the texture. Copying is lazy, at most once per direct-draw replay: `DrawSprites` invalidates before replay; only requesting draws copy, so non-refracting frames pay nothing. The opaque blit supplies background colours without another blend.

Effects sample the copy through `RenderEffect::BackgroundTex`, beside `MainTex`/`IndoorMaskTex`, with identical binding across all four backends and shader name `BackgroundTex`. Snapshot orientation matches its source target; shaders flip screen-space lookup for flipped targets instead of reorienting the copy.

`ParticleRuntimeServices::SceneBackgroundProvider` is pulled when a draw needs it, never pushed into every `Setup`. `Unavailable` means no refractable scene, such as an ordinary offscreen atlas; runtimes fail closed rather than sample empty targets. `Deferred` applies only to auxiliary previews preceding a direct-scene draw: skip the distortion node without retiring the system, then ask again at scene draw.

Attached particles receive the provider only during `ModelInstance::DrawInScene`. Direct models still refresh an auxiliary preview/hit-test atlas frame; with `Render.ModelDirectDraw`, it returns `Deferred`, preserving distortion until direct replay samples the current scene. Ordinary atlas-rendered models return `Unavailable` and fail closed because no later scene draw exists.

## Per-draw face culling

Culling belongs to the draw: callers set `RenderEffect::CullMode` (`CullModeType`: `None`/`Back`/`Front`, zero-initialized `None`). `RenderEffect::CanBatch` separates differing modes. 3D models use `Back`, or `None` with culling disabled; per-emitter formats such as Effekseer (`Front`, `Back`, `Double`) map their modes per draw.

Prebuilt rasterizers need each drawable mode, enabled by `CullVariants = True` in `[Effect]`: `Rendering-Direct3D.cpp` creates `ID3D11RasterizerState` per mode, `Rendering-Vulkan.cpp` a pipeline per `(pass, primitive, blend, depth slot, cull mode)`, `Rendering-SDLGpu.cpp` includes mode in its lazy cache key, and `Rendering-OpenGL.cpp` calls `glCullFace`. Central `RenderEffect::ResolveCullMode()` throws for unbuilt modes instead of dropping Vulkan draws or choosing different Direct3D state. Every backend uses counter-clockwise front faces.

The map target (`MapView::_rtMap`) uses `with_depth`, sharing one world depth buffer. `EffectUsage::QuadSprite` and `EffectUsage::Model` participate; on depthless UI/light/flush-to-screen targets, depth state is a hardware no-op:

- Screen-space quads (GUI, fonts, render-target blits, and non-map sprite effects) initialize `Vertex2D::PosZ` to `0.0f`; map sprites may start from the same atlas data, but `SpriteManager` overwrites their Z before flushing them into `_rtMap`.
- Standing map sprites (`Item`/`Critter`) **write** shared depth but do **not** test it (`DepthFunc = Always` in `2D_Default.fofx` / `2D_WithoutEgg.fofx`): writes let direct-draw particles and 3D models occlude against sprites; sprite-vs-sprite occlusion is pure painter order. Because all standing planes share one gradient (`ProjectMapYToVerticalDepth`), parallel planes never intersect, so anchor-depth ordering reproduces per-pixel `LessEqual` exactly, without interpolation noise that made *coincident* same-row planes stripe far rows. `MapSpriteList::MakeDrawOrderPos` therefore sorts by `GeometryHelper::GetHexScreenRow` (the row of `GetHexPos().y`, equal-ground-depth class — hexes related by +2X/−1Y), not hex row: hex row disagrees with depth (e.g. `hy−1, hx+4` is nearer yet sorts earlier), breaking painter order for overlapping large scenery. Pinned by `Source/Tests/Test_Geometry.cpp` (`GetHexScreenRow`). A *flat* depth-writing layer no longer clips standing sprites per pixel: a flattened corpse in an earlier layer is covered by any overlapping standing sprite. Per-vertex depth is the vertical-billboard proxy (`get_map_sprite_proj` → `GetHexWorldPos`/`ProjectWorldToMap`, anchored on sprite root; `MapView::InitView` reproduces `GetHexPos.y` exactly). **The depth/sort anchor is the object's LOGICAL root, not bitmap bottom-center.** For an item, proto `Offset` is the bottom-center→root vector (a tree trunk): it still positions the bitmap
 through `MapSprite::_pSprOffset` (visuals, lighting and `MeasureMapBorders` unchanged), but is also kept as a static root offset (`HexView::_rootOffset` → `MapSprite::_pRootOffset`) subtracted by depth — in `GetMapRootOffset()` for `sprite_proj.z` and from `scene_pos_y` — so a tall sprite anchors on its trunk, not `Offset` pixels below; otherwise it anchored too far south and occluded a front critter. Critters lack proto `Offset` (root from sprite anchor), so `_rootOffset` is zero. Standing item/critter layers alone use the depth buffer; flat/background layers are painter-only and depth-inert. `MapSprite` receives critter/item `Elevation`; positive elevation shifts screen Y up and increases world-Z depth. `MapSprite::HexOffset` plus runtime sprite/tweak offsets project on the ground plane before depth, so sub-hex movement changes screen position and 3D depth continuously; viewport-only `field.Offset` is not world depth. Intrinsic `Sprite::Offset` defines the logical ground root inside the atlas quad, so vertical depth and direct-scene anchors use it, not the bitmap's lower center. Floor tile layers (`DrawOrderType::Tile..Tile4`) and flat overlays (`FlatItemPreLight`/`HexGrid` pre-light, `FlatCritter`/`FlatItemAfterLight` post-light, below `NormalBegin`) are upright background sprites with atlas XY/UV and **never touch shared depth**. Entity sprites choose the no-depth effect at the entity level:
tiles and roofs resolve `Effects.Tile` / `Effects.Roof`; flat items resolve `Effects.Flat` (`ItemHexView::Init`, by `GetDrawFlatten()`) — all three → `2D_NoDepth.fofx`. Script-created `MapSpriteHolder` sprites lack an entity effect handle, so `MapView` supplies the segment default to `SpriteManager::DrawSprites` before batching: `Tile..PreLight` → `Effects.Tile`, `AfterLight..FlatEnd` → `Effects.Flat`, `Roof..RoofParticles` → `Effects.Roof`, normal layers → `Effects.Generic`. Item order uses `GetDrawFlatten()`, not `IsScenery`/`IsWall`: upright → `Item` (default), flat → `FlatItemPreLight` if `GetStatic()` else `FlatItemAfterLight`. Former `Scenery`/`Item` and `FlatScenery`/`FlatItem` layers merged, so upright items draw in **add order** (`MapSprite::_globalPos` tiebreaker once `_drawOrderPos` ties). Critters with project `DrawFlatten` keep `Effects.Critter` and write depth, but standing sprites do not depth-test, so a corpse is covered by draw order. All these (`DepthWrite = False` + `DepthFunc = Always`) draw before standing sprites, painter-sorted, so they cannot z-fight, seam, or clip a standing sprite/3D model; they need no vertex depth, ground projection, or layer bias. Standing layers (`Item`, `Critter`) keep atlas XY/UV but write per-vertex `PosZ` via `GeometryHelper::ProjectMapYToVerticalDepth`, like vertical planes on their ground anchor; no draw-order bias, which would pull the plane toward camera and move the particle-occlusion line off the logical root. The only remaining depth-bias user is the direct-draw path
 (particles/3D models replayed at each pass end): `SpriteManager` divides a half-pixel budget (`MAP_LAYER_DEPTH_BIAS`) by `DrawOrderType::Last + 1`, giving each direct-draw sprite one step above its world depth, below subpixel snapping. Both `Core` and `Embedded` `2D_Default.fofx` must project `InPosition.xyz`; flattening to `InPosition.xy, 0.0` leaves no useful scene depth. `2D_Default.fofx` and `2D_WithoutEgg.fofx` discard final alpha at or below `1/255` after egg alpha, so transparent texels do not populate depth and clip in-scene particles behind empty atlas areas.
- Within one standing screen row, `MapSpriteList::MakeDrawOrderPos` sorts by
  `[group 8][primary row 24][layer 8][sub-layer 8][hex X 16]`. Layer precedes
  sub-layer and X: critters paint in front of every upright item in their
  screen row; within the item layer, lower-sub-layer walls paint first.
  Nearer rows still win, and insertion order breaks an otherwise equal key.
  `Item.DrawOrderSubLayer` is mutable and publicly synchronized; a client
  property update refreshes the item's map sprites so their sort keys change
  without reloading the map.
- A flattened dead critter stays in the lower draw-order group than upright
  items and critters even when their screen row differs. Same-row door flaps
  and frames still follow sub-layer before X within the item layer. `Test_MapSprite.cpp` pins both
  cases; visual acceptance must still check the actual scene.
- Roof tiles (`IsRoofTile`) are ordinary floor tiles with fixed positive `Elevation` (`Geometry.MapRoofElevation`): projection raises their screen position onto building wall tops (the engine still auto-hides the roof group whose `RoofNum` the camera is inside). A roof is a tile lifted in screen Y; the flat tile/roof sub-hex XY anchor lives on the `BaseTile` prototype's `Offset`, not the removed per-side `Geometry.MapTileOffs*`/`MapRoofOffs*` settings, and roof-particle and mapper tile-preview paths read the same `Elevation` instead of the old 2D roof offset. The **roof draw-order range (`Roof..Last`) renders as a separate trailing pass** (`MapView::DrawSpritesWithFog` splits at `below_roof = Roof-1` and draws `[Roof..Last]` last via `DrawFoggedSpriteRange`): everything below, including each sprite pass's direct-draw 3D models and in-scene particles, draws first. Like floor tiles, roofs do not touch depth (`Effects.Roof` → `2D_NoDepth.fofx`, `DepthWrite = False` + `DepthFunc = Always`): drawn last and never depth-tested, the roof layer always paints on top of the building regardless of scene depth, and being depth-write-free it never clips later draws.
- Particle effects (`Particles_*.fofx`) declare `DepthFunc = LessEqual` + `DepthWrite = False`: they test scene depth so closer geometry occludes them, but do not occlude each other. Colour variants (`Particles_Color*.fofx`) also declare `DepthVariants = True`, because a runtime storing depth-test/write per emitter node selects the matching variant per draw. All colour and distortion `*Atlas` fragments discard final alpha at or below `1/255`, so a node opting into depth writes cannot turn transparent or filter-fringe texels into invisible occluders. They read `ParticleSamplingBuf`, whose `.x` snaps the sample to texel centre: filtering is per-atlas here (`Render.AtlasLinearFiltration` applies to every atlas) but per-node in particle formats, and a bilinear fetch at a texel centre returns exactly that texel, giving true point sampling from a linearly filtered atlas without a second atlas. `*Atlas` variants also read `SpriteBorderBuf` and address the whole coordinate inside that sub-rectangle, tiling or clamping as `ParticleSamplingBuf.y` says: hardware wrap or clamp cannot serve a shared atlas because both reach neighbouring entries. A runtime supplying raw per-node coordinates therefore uses `*Atlas` variants exclusively; non-atlas variants are for SPARK, which supplies final atlas coordinates and binds the neutral sampling buffer before every draw.
- Model effects (`3D_*.fofx`) use `DepthFunc = LessEqual` + `DepthWrite = True`: direct-to-scene models write real mesh depth into `_rtMap`, so particles and later direct geometry can test against the model surface instead of the old model atlas quad.
- `OnRenderMap_AfterSpritesAndFog` fires after the sprite/fog pass and before the map target flush. Scripts needing entity debug markers should iterate visible `Item`/`Critter` objects, combine `Map.GetHexMapPos(entity.Hex)`, `entity.GetSpriteOffset()`, and `entity.Elevation`, then subtract the event draw-area origin; selection and filtering stay in scripts without depending on transient sprite instances.
- **Entity contours/outlines are script-driven, not native.** There is no engine contour pass; an embedding-project script (Last Frontier: `ContourPipeline.fos`, compiled for `CLIENT` and `MAPPER`) caches entities whose `Contour` colour property is non-zero and, on `OnRenderMap_AfterSprites`, draws each via `Map.DrawEntitySprite(entity, contourEffectSubtype, colour, padding)` (a dilated silhouette, then the sprite on top so only the rim shows). The mapper's selection outline uses the same path by setting the selected entity's `Contour` property (registered for the mapper because `Client` registration includes that target), not a native call.

## Direct-to-scene sprites

`Sprite` may override `IsDirectDraw()` for shared-depth geometry in the current scene target rather than atlas-quad batching. Interleaving its own shader would split every surrounding batch. Instead `SpriteManager::DrawSprites` collects these sprites, then replays one `Sprite::DrawInScene(scene_pos, depth)` pass (`const`, like `FillData`) after flushing. Opaque sprites write depth (`DepthFunc = Always`, `DepthWrite = True`); direct transparents only test (`LessEqual`, `DepthWrite = False`). Anchors use projected `hex + HexOffset + SpriteOffset/TweakOffset + Elevation`, exclude viewport-only `field.Offset`, and retain one anchor-bias step rather than late draw order; otherwise `DrawOrderType::Particles` would appear depth-closer than critters/scenery before geometry is considered.

`ParticleSprite` supports **two render types**, chosen per particle system by the `SparkQuadRenderer` `draw in scene` `.spark` attribute (`ATTRIBUTE_TYPE_BOOL`, default false, alongside `draw size`):

- **Atlas type** (default, `draw in scene` absent/false): `Update()` advances simulation independently, then refreshes the offscreen atlas (`ParticleSpriteFactory::DrawParticleToAtlas`) at the configured animation cadence; the sprite is drawn as a flat batched quad and `IsDirectDraw()==false`.
- **Scene type** (`draw in scene = true`): `IsDirectDraw()==true`; `Update()` advances simulation even offscreen, and `DrawInScene` refreshes the scene transform without advancing frame time before rendering directly into `_rtMap` through the map view-proj. Particles keep their lifetime offscreen and depth-sort against scene geometry instead of being baked to a flat sprite.

`ParticleSprite::Play()` respawns backend-neutral `ParticleSystem` before updating. It delegates through `ParticleRuntimeSystem`; render-facing code has no SPARK/Effekseer dispatch or unnamed fallback. One-shot SPARK can replay after `Game.PlaySprite(...)` or `AnimFree`/`AnimLoad` cache reuse.

Live render bounds and rebasing of emitted particles are backend capabilities behind the same facade. Both backends report live bounds from their baked extent (SPARK through the `.spk` `bounds` attribute, Effekseer through the `.efk` bounds trailer). Rebasing is SPARK-only because it emits in world space; Effekseer composes instance transforms with the effect root matrix, so rebasing is a no-op.

Seeded respawn is deterministic per instance in both bundled runtimes. Effekseer seeds its manager handle. Each `SparkParticleRuntimeBackend` owns an `SPKContext` with IO registry, default zone and ambient generator; loaded graphs bind it before attribute import. Each `SparkParticleRuntimeSystem` keeps its generator state, temporarily binding it for cloning, prewarm and updates. Interleaved effects or separate engines cannot perturb seeded sequences.

`SparkExtension.h` exposes only facade, forward declarations and plain renderer-data helpers. SPARK headers, `SparkQuadRenderer` and its buffer adapter stay private to `SparkExtension.cpp`; Mapper/baker inspect data helpers rather than concrete renderer types.

`ParticleSystem::SetScale()` updates cached neutral setup, reapplies it with zero-delta transform refresh and forces atlas redraw without respawn or elapsed-time reset. All enabled runtimes, atlas and direct-scene sprites share this contract.

Core-only Effekseer uses the same sprite/direct-scene paths. Its renderer interfaces supply evaluated-data callbacks, not graphics backends: FOnline copies values, builds its `RenderDrawBuffer`, selects `RenderEffect` and submits normally. Mapper/game previews share this route without Effekseer Direct3D/OpenGL/Vulkan/SDL GPU code.

Collectors fail closed on malformed topology, enforcing the fixed supported-instance hard limit and exact `BeginRendering` count (`BeginRenderingGroup` for strip geometry per group, not node). Later `Rendering` cannot exceed it; indexed stitching requires strip chain order. Rings copy evaluated outer/center/inner shape/colours, preserve upstream eight-vertex/twelve-index topology, angular fades and Z-sort, splitting at 64,000 vertices for 16-bit indices. Both collectors stably Z-sort index permutations then materialize reordered instances, never sorting snapshots in place: `alignas(16)` SIMD members make `std::stable_sort` instantiate rejected extended-alignment `std::aligned_storage`. Stable permutation preserves deterministic order.

**Distortion nodes** displace background sampling with red/green texture channels in the particle's own plane, retaining alpha to mask refraction. Rotated/free particles require tangent/bitangent per vertex; screen axes would rotate refraction incorrectly. These draws use model layout rather than widening 2D vertices. Reserved `ParticleSamplingBuf` channels carry intensity/background orientation. Only sprites refract; other families reject this material at load, preventing accepted effects that later vanish.

**Model nodes** draw meshes per instance. `.efkmodel` loads through `Effekseer::ModelLoader`, whose vendored parser trusts byte counts/face indices. Engine first validates the whole payload and budgets (64 MiB, 4096 frames, 64000 vertices, 21333 faces per frame); only then does core parsing proceed. Instance transforms fold into world-space vertices like other families, retaining common draw-buffer/atlas/batching paths. `CullMode` carries node culling; instance time modulo frame count selects animated mesh frames.

**Ribbon/Track strips** differ only in cross-section production and share a builder: per-instance width triples (left/centre/right, with colours) become two quad strips sharing the centre line, with stretched chain UVs, resolved colour texture, atlas flags and Ring-budget chunks. Ribbon transforms both authored edge offsets through instance matrices, or spreads them around the emitter up axis for camera-facing viewpoint dependence. Track centres sections per instance across travel direction, averaged at interior joints to avoid kinks, interpolating head/tail width/colour toward the middle. Spline smoothing, tiled strip UVs, trail smoothing, view offset, left-handed strips, non-Default track materials and Z-sorting (which reorders stitched instances) are rejected before vertices. Ribbon `Positions[2]`/`[3]` are spline-only and arrive uninitialized; never validate them.

`Source/Tests/Test_EffekseerParticleRuntime.cpp` provides cooked callback-to-buffer fixtures without stock graphics backends. Legacy tests cover fixed-seed determinism, repeated fixed steps, multi-instance topology, quad/index order and atlas UVs. A project-authored Effekseer 1.80.5 fixture checks cooked `None`/`NormalOrder`/`ReverseOrder` Sprite Z-sort reaches callbacks with expected depth. SKFE/1810 TestData checks deterministic Ring topology/radii/UV/index order, all three Z-sort modes and 64,000-vertex chunks preventing 16-bit overflow. Project-compiled strips supply several live group instances, pinning two quads per segment, shared centre, continuity, stretched UVs, width and viewpoint orientation.

Adapters accept one Default-material colour texture, optionally omitted for renderer-owned white pixels matching Effekseer vertex colour. `Clamp`/`Repeat` resolve against published atlas sub-rectangles for any UV; only `Mirror` is rejected. Linear/Nearest requests need not match atlases: `Nearest` on linear atlases snaps each draw to texel centres. Non-zero intensity with Default-material distortion disabled is dormant/ignored; active distortion still fails the capability gate.

Effekseer sprites always use scene rendering. Prewarm waits for first `DrawInScene` after `Setup` supplies current map transform; `ParticleSprite::Update()` never advances while pending. Effekseer then advances exactly one second and resets wall-clock origin, avoiding another offscreen-time advance. `RefreshRenderTransform()` only refreshes at zero delta, never taking scheduled simulation's forced first-tick path.

`SparkQuadRenderer::GetDrawInScene()` → `ParticleSystem::GetDrawInScene()` → `ParticleSpriteFactory::LoadSprite` carries the flag. Model-bone particles (`ModelInstance::RunParticle`) follow a separate path and ignore it.

`ModelSprite` supports direct scenes with `Render.ModelDirectDraw`. Default `false` caches atlases: `ModelSprite::Update()` refreshes them and batches draw quads. With `Render.ModelDirectDraw = true`, `ModelSprite::DrawInScene` builds shared map view-proj, bakes logical root (`scene_pos` + raw depth) into proj and calls `ModelInstance::DrawInScene`. Animation/skinning is reused; shared VP applies tilt once, skipping old atlas-only tilt. `DrawToAtlas` keeps full automatic frames for preview/hit tests, preventing cached rectangles from culling live direct poses. Attached SPARK/Effekseer use active direct proj with `tilt_in_proj`, the same world-space frame/shared depth; distortion pulls replay scene snapshots on demand. Direct draws still disable old atlas-space shadows pending a world-space rewrite.

Cached frames use the minimum of `Render.ModelSpriteMaxTextureWidth` / `Height` and current `AppRender::MAX_ATLAS_WIDTH` / `HEIGHT`, divided by `FRAME_SCALE` for 2x physical scratch rendering. Above-budget dynamic model-bone particle bounds are unavailable: retain the established frame and clip runaway outliers. This bounds CPU/GPU allocation from malformed/long-lived particle motion in headless and rendered paths.

**World scale.** `Render.ModelProjFactor` is pixels per 3D world unit for 3D models and in-scene particles. Default `40.0` may be consistently overridden for both. **1 world unit = 1 hex = 1 m** remains the authoring metric; the pixel factor need not equal `MAP_HEX_WIDTH`. Radius N units therefore spans N ground hexes, matching direct 3D models at that scale. [Particle Format And Runtime](../../how-to/content/particle-format.md#runtime-contract) owns the full neutral route.

## Embedding-project practices

Current embedding projects illustrate composition; their settings, effect names, scripts and acceptance remain project-owned:

1. Keep every `Render.Force*` selector false in shipping. Use narrow backend sub-configs or launch recipes for diagnosis and cross-backend acceptance, with exactly one force selector per recipe.
2. Do not copy an older project's render block. Start from settings generated for the pinned Engine revision, then review each override. Backend selectors and model/layout settings have changed; an old `.fomain` is migration evidence, not a template.
3. Put richer project-owned shaders in a resource pack ordered after Engine `Core`/`Embedded`. Shadow the same resource name only when intentionally replacing a minimal base effect, preserving alpha, depth, descriptor, and uniform contracts. These are advanced-profile overrides, not Engine-fallback changes.
4. Treat `Render.ModelProjFactor` as one project-wide authoring decision shared by models and in-scene particles. Changing it is a visible content-scale migration; validate representative creatures, attachments, effects, hit areas, and map occlusion together.
5. Keep logical resolution policy, settings choices, effect slots, offscreen composition, contours, and accessibility/visual acceptance in project docs and tests. The Engine supplies mechanics but cannot certify a game's UI or art direction.
6. Qualify every renderer/driver a release claims. A clean default-backend scene does not prove Vulkan, SDL_GPU, WebGL, or a specific SDL_GPU driver; record selector, driver, platform, scene, logs, and screenshot or interaction evidence.

## Platform packages and BuildTools relationship

`BuildTools/cmake/stages/Packages.cmake` generates package targets. Platform workflows choose packaged app/runtime artifacts; backend availability still depends on configured source, compile definitions, dependencies and platform toolchains.

Keep these boundaries clear:

- frontend source defines what the engine can do;
- CMake/BuildTools decide which apps/backends/platform packages are built;
- embedding-project presets choose concrete configurations;
- platform docs explain how to debug the resulting package.

Do not document one embedding project's generated target names as universal engine target names.

## Frontend/rendering validation tests

`Source/Tests/Test_Rendering.cpp` is the minimal local GPU-free surface: `NullRenderer` covers texture read/write/clear, draw-buffer upload/effect draw and unbuilt-depth rejection. `Test_TextureAtlas.cpp` owns packing/dump geometry; `Test_Geometry.cpp` owns matrix/depth projection. Model/image/particle suites own their runtime paths.

`NullRenderer` readback sections check row layout, request-time snapshot, bounds and exactly-once transfer. `Test_ClientEngine.cpp`'s `ModelSpriteHitTestReadsItsMaskFromTheAtlas` covers CPU-mask reuse/redraw refresh, requires `FO_ENABLE_3D` and executes with either scripting backend. Neither proves fence timing or visible GPU picking.

Native tests prove backend-neutral invariants; GPU implementations need separate visible, target-specific acceptance for every affected backend: Null/headless, OpenGL/WebGL, Direct3D, Vulkan, SDL_GPU. Unimplemented direct Metal cannot supply acceptance evidence.

## Resource ownership

`ResourceManager` owns factory registration, extension dispatch, copyable templates, failed-path memoization, invalidation and retries. `SpriteManager` owns rendering, atlases, batching and live updates; `FileSystem` owns mounts/raw reads. Factories retain textures borrowed by model/particle render callbacks; lifetimes are unchanged and no shared texture cache is added.

Construct `SpriteManager`, `ResourceManager`, then `CritterFramesCache` and `FontManager`; destroy in reverse. Factories receive both sprite/resource managers. ResourceManager subscribes to begin-scene at the former factory-update position before live sprite updates, and to low-memory cleanup. Both callbacks detach before caches/factories die; `ClientEngine::Shutdown` also detaches them.

`CritterFramesCache` caches project-resolved directional sheets/animation tags. `MapperEngine` owns mutable critter previews. Neither selects project filenames, animation IDs or frame-splice rules.

## Validation checklist

When changing frontend or rendering behavior, verify:

- `Application` init still works for graphical, headless, and test/tool flows.
- Input changes preserve `InputEvent` invariants and client script event mapping.
- Touch/gamepad changes are platform-neutral unless clearly guarded.
- Renderer changes are tested on the affected backend: Null/headless, OpenGL/WebGL, Direct3D, Vulkan, or SDL_GPU. Direct Metal is a placeholder, not coverage.
- Render-target changes preserve stack push/pop and previous-target restoration.
- Texture orientation changes account for `IsRenderTargetFlipped()` differences between OpenGL (flipped) and Direct3D/Vulkan/SDL_GPU (not flipped).
- Effect changes document config parsing, shader files, and script-value buffers. On Vulkan, confirm the set-0-UBO / set-1-sampler descriptor-set contract.
- Web changes cross-link to [Web Build, Packaging, and Browser Debugging](../../how-to/platforms/web-debugging.md); Android changes to [Android Build, Packaging, and Device Debugging](../../how-to/platforms/android-debugging.md); native attach/debug changes to [Native, AngelScript, and Managed Debugging](../../troubleshooting/debugging.md).

The application thread retains rendering under [client multithreading](../runtime/client-multithreading.md); CPU pose batches own no GPU objects or command recording.

### CPU geometry batches

Atlas sprites, region quads and glyphs share pure `WritePreparedSpriteGeometry`. The application thread captures immutable inputs, reserves offsets and preserves draw order. Worker geometry completes before upload, wireframe or external-sprite buffer access; target/scissor transitions keep existing flushes. Serial clients call the kernel directly. Rotation, flattening, standing depth and both egg flags are captured values, never worker reads of map entities. [client multithreading](../runtime/client-multithreading.md#sprite-and-glyph-geometry) owns lifetime, exceptions, thresholds and measurement limits.

## Parallel map preparation

Light/fog workers produce only passive CPU results. `MapView` publishes source marks/ordered fans after its light barrier; `FogShape` binds draw/base offsets after sector completion, retaining transition state on the owner. Target selection, primitive assembly, upload and compositing keep application-thread order. [Client multithreading](../runtime/client-multithreading.md#light-and-fog-batches) owns direct bypass, immutable obstacles, failure/retry and provisional thresholds. Native equality is separate from visible backend/GPU validation.

Roof groups are rebuilt lazily after roof coverage changes, including dynamic insertion, removal and movement. Queries and map rebuilding call `EnsureRoofNums`; unchanged frames do not scan the entire map. The last hex passed to `SetHiddenRoof` is retained and resolved again after renumbering, so splitting or merging a group does not hide an unrelated roof. Roof lattices start at each actual tile anchor, including odd coordinates.
