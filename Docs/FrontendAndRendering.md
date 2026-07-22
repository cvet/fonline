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
- `Source/Frontend/Rendering-Vulkan.cpp`
- `Source/Frontend/Rendering-SDLGpu.cpp`
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
- `Vertex2D` and `Vertex3D` — vertex layouts used by sprite and model paths. For primitive batches uploaded through `SpriteManager::DrawPoints`, `PosX/PosY` are the draw-area-local pixel coordinates (with the `draw_area` scroll offset subtracted), and `TexU/TexV` carry `PrimitivePoint::TexUV + draw_area.xy` — that is, `DrawPoints` adds the draw-area top-left to whatever the caller authored. The intended idiom for world-stable per-fragment effects (dither, noise, gradient mapping) is to author the **same constant** `TexUV` on every vertex of a primitive batch — the absolute map-origin-anchored pixel position of the screen-anchor hex `_screenRawHex`. The fragment shader then reconstructs each fragment's true absolute world pixel position as `gl_FragCoord.xy + InTexCoord`. Because every vertex carries the same constant, varying interpolation is degenerate (no barycentric rounding can crawl the noise), and the rasterizer's per-pixel `gl_FragCoord` provides the spatial variation. This is robust against camera scroll, camera zoom, fan-triangle deformation from smooth sprite movement, and the `from_hex.x` parity sensitivity of `GeometryHelper::GetHexOffset` on offset-row hexagonal grids. `MapView::LightFanToPrimitves` authors it via `GeometryHelper::GetHexOffset(mpos(0, 0), _screenRawHex)`. `Primitive_Light.fofx` consumes it (`worldPixel = gl_FragCoord.xy + InTexCoord`) to jitter the light's edge taper with world-stable noise; other primitive shaders ignore `InTexCoord` and are unaffected. The light fan also carries the **normalized radial distance** in `PrimitivePoint::PointPosZ` (`LightFanToPrimitves`' `rim_dell`: 0 at the center, ~1 at the rim) → `InPosition.z`; `Primitive_Light.fofx` reads it as `Rim` and `smoothstep`s the outer band (`EdgeTaperStart`..1.0) to zero so brightness rises gently *from zero* at the rim instead of ending in a hard constant-slope edge (which is very visible when the light moves).
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
- reports render-target textures as vertically flipped (`IsRenderTargetFlipped() == true`);
- **uniform blocks go through one shared bump-allocated UBO** (ES 3.0 / GL 3.1 compatible): per
  draw, every shader-required block — default-initialized to zero when the engine did not fill
  it, since a skipped block would otherwise keep a binding into a region whose storage dies at
  the per-frame orphan in `Present()` — is packed into a contiguous region, uploaded with a
  single `glBufferSubData`, and bound per pass with `glBindBufferRange` (replaces up to ~8 tiny
  per-draw `glBufferData` re-specifications of per-effect UBOs);
- **`SetRenderTarget` elides redundant re-selects** of the already-applied target (bind, viewport,
  aspect-fit and ortho recompute are skipped); the cache is invalidated on resize and on
  destruction of the cached texture, and mid-frame texture creation restores the *current*
  target's framebuffer rather than unconditionally the base one.

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

### Vulkan renderer

`Source/Frontend/Rendering-Vulkan.cpp` implements the Vulkan path. It is built by default (opt out with `FO_DISABLE_VULKAN`; also skipped for headless-only and web builds) and needs **no external Vulkan SDK** — there is no `find_package(Vulkan)`. The build compiles against the Vulkan headers already vendored with SDL3 (`ThirdParty/SDL/src/video/khronos`, wired as a SYSTEM include when `FO_HAVE_VULKAN`), and the loader is resolved dynamically at runtime. It is selected at runtime by `Render.ForceVulkan` (or as an automatic fallback when no other backend is configured).

The loader is **not** linked at build time (`vulkan-1.lib` is never referenced). Instead `Rendering-Vulkan.cpp` compiles with `VK_NO_PROTOTYPES` and resolves every entry point dynamically through SDL — `SDL_Vulkan_LoadLibrary` + `SDL_Vulkan_GetVkGetInstanceProcAddr` bootstrap `vkGetInstanceProcAddr`, then a small X-macro table (mirroring the OpenGL backend's `SDL_GL_GetProcAddress` table) loads global functions with a null instance and the rest from the created instance. Consequently a client built with Vulkan support carries **no load-time `vulkan-1.dll` import** and still launches on a machine without the Vulkan runtime; the loader is pulled in only when the Vulkan backend is actually selected (a missing runtime then throws from `SDL_Vulkan_LoadLibrary`, not at process start).

Design and important behaviors:

- **Single queue, two frames in flight.** The context owns `VULKAN_FRAMES_IN_FLIGHT` (= 2) frame slots, each bundling a command buffer, an in-flight fence, an acquire semaphore, a descriptor pool, a persistently-mapped uniform bump buffer, a texture-staging ring and a deferred-destroy queue. `BeginFrame()` advances the slot, waits its fence (normally instant — this replaces the old full `vkQueueWaitIdle`, so the CPU records frame N while the GPU renders frame N-1), flushes the slot's deferred destroys, resets its descriptor pool, points the context's current-slot aliases (`CommandBuffer`, `FrameDescriptorPool`, `FrameUniformBuffer`, …) at it, acquires a swapchain image, clears it, and begins the render pass; `EndFrame()` ends the pass, submits (signaling the slot fence and the acquired image's render-complete semaphore), and presents. Render-complete semaphores are **per swapchain image**, so a semaphore is never re-signaled while the presentation engine may still wait on it; acquire semaphores are per slot. A fence signal implies completion of all earlier submissions on the queue, which is the single correctness anchor for every per-slot resource reuse.
- **Deferred destroys are per frame slot.** `DestroyResourceSafe(...)` enqueues into the *current* slot's queue; the queue is flushed right after that slot's fence is waited, by which point both in-flight frames that could reference the resource are provably complete. Swapchain recreation paths settle the device (`vkDeviceWaitIdle`), flush all queues wholesale and rebuild every sync object.
- **Texture uploads record into the frame command buffer; readbacks flush it.** Draws and clears record into the frame command buffer (executed at present time). `UpdateTextureRegion` during a recording frame suspends the render pass and records barrier → `vkCmdCopyBufferToImage` → barrier into that same buffer, so program order preserves the engine's immediate-mode ordering (an atlas clear recorded earlier this frame executes before the upload — without this, "clear atlas, then upload sprites" would execute as *upload first, clear last* and silently erase glyphs/sprites) with no mid-frame submit or full-GPU wait; the pixels go through the frame slot's pooled staging ring. `GetTextureRegion` (readback) must observe everything recorded so far, so it first calls `FlushFrameCommandBufferMidFrame()` — submit the partially recorded frame buffer (waiting the swapchain-acquire semaphore if it is the frame's first submit), wait idle, resume recording — and then runs an immediate staging copy. Uploads outside a recording frame (texture init) use the immediate staging path too. Keep this invariant when adding any new immediate-queue operation.
- **Dynamic geometry goes through per-draw-buffer, per-frame-slot ring pools.** Every dynamic `DrawBuffer::Upload` takes the next buffer of the draw buffer's growable ring of persistently-mapped HOST_VISIBLE buffers for the current frame slot (one ring buffer per upload within a frame, so earlier draws pending in the frame command buffer keep their geometry snapshots). A ring resets on its first acquire in a new frame; its slot's in-flight fence was waited by then, so every buffer in it is GPU-free. Ring buffers only reallocate on capacity growth, so steady-state uploads are pure memcpy with zero `vkCreateBuffer`/`vkAllocateMemory`/`vkFreeMemory` traffic (per-upload buffer churn plus the matching deferred-destroy sweep previously dominated the backend's CPU frame cost ~25 ms/frame in crowd scenes). Static buffers keep the one-off staging copy to device-local memory.
- **Shaders are baked with `highp` floats.** The effect baker emits ES shaders with `precision highp float`. `mediump` would become SPIR-V `RelaxedPrecision`, which desktop GL/D3D silently ignore but NVIDIA Vulkan drivers honor as FP16 — large uniform values (frame time in seconds, world-anchored UVs) then overflow half-float range (max 65504) and shaders that consume them (e.g. time-driven weather/atmosphere post-processing) collapse to black on Vulkan only.
- **Back-buffer target metrics follow resizes without `SetRenderTarget`.** The letterboxed viewport, logical target size and projection for back-buffer rendering are recomputed by `ApplySwapchainTargetMetrics()` — from `SetRenderTarget(nullptr)`, from `OnResizeWindow()` and after a deferred swapchain recreation when the back buffer is the active target. The server host UI renders ImGui straight into the swapchain and never calls `SetRenderTarget`, so without the resize-path refresh a post-init window/logical-size change leaves a stale projection and the UI renders shrunken into a corner (Direct3D gets the same refresh by ending its `OnResizeWindow` with `SetRenderTarget(nullptr)`).
- **Two fixed descriptor set layouts.** **Set 0 holds uniform buffers** and **set 1 holds combined image samplers** (each layout declares a fixed number of bindings). Per draw, the backend allocates one descriptor set per layout from a per-frame descriptor pool (reset every `BeginFrame()`), and writes uniform data into a single host-visible **bump-allocated** uniform buffer. The binding index used within each set is the effect's reflected binding from the baked `EffectInfo`.
- **Authored `.fofx` descriptor-set contract.** Because of the two-set layout, every effect's shader **must** declare uniform blocks with `layout(set = 0, binding = N, std140)` and samplers with `layout(set = 1, binding = N)`. An effect that omits the `set` qualifier defaults every resource to set 0, which collides samplers with uniform buffers and produces `VkDescriptorType` mismatch (`VUID-…-layout-07990`) and "descriptor never updated" (`VUID-vkCmdDrawIndexed-None-08114`) validation errors, leading to device loss. Engine core/embedded effects follow this convention; embedding-project effects must follow it too. (OpenGL/Direct3D ignore the `set` qualifier because they bind UBOs and samplers in separate namespaces, so this only surfaces on Vulkan.)
- **Uniform completeness.** Unlike OpenGL/D3D, where an unbound UBO retains its last value, a per-draw descriptor set must write every uniform block the shader uses. The backend default-initializes any required-but-unset standard buffer (egg, sprite-border, time, random, script, camera, …) to zero before upload so no shader reads an unwritten descriptor.
- **Surface format.** The swapchain uses `VK_FORMAT_B8G8R8A8_UNORM` / `SRGB_NONLINEAR`, verified against `vkGetPhysicalDeviceSurfaceFormatsKHR`. The single render pass is shared by the swapchain framebuffers and all texture render targets, so texture render targets use the same color format for render-pass compatibility. CPU pixel upload/readback swizzles R↔B to match this format.
- **Present mode honors `Render.VSync`.** `VSync = true` (or a surface with no better mode) uses `VK_PRESENT_MODE_FIFO_KHR`; with VSync off the swapchain prefers `IMMEDIATE` (uncapped, possible tearing), then `MAILBOX` (uncapped, no tearing), matching how the other backends honor the setting in their present paths. The chosen mode is logged at swapchain (re)creation (`Vulkan swapchain present mode: …`). A hardcoded FIFO would silently vsync-lock the backend and make cross-renderer frame-rate comparisons meaningless.
- **Orientation.** Render-target textures are reported as not flipped (`IsRenderTargetFlipped() == false`, like Direct3D). The projection matrices are **identical to the other backends** (Y-up ortho); Vulkan's Y-down clip space is compensated by a **negative-height viewport** (core since Vulkan 1.1 — the instance requests `VK_API_VERSION_1_1`). Keeping the matrices identical matters beyond the GPU: `GetProjMatrix()` feeds engine-side 3D model camera math (`ModelSprites`/`3dStuff`), and a backend-specific Y-negated matrix breaks that CPU-side placement (3D models render clipped). The negative viewport flips screen-space winding exactly like the old Y-negated matrix did, so pipeline front-face settings are unaffected.
- **Point primitives.** Effect shaders are cross-compiled to HLSL/MSL via spirv-cross, which cannot express `gl_PointSize`, so a `POINT_LIST` topology (which Vulkan requires `PointSize` for) is mapped to `TRIANGLE_LIST`. Point primitives are not used by current content; revisit if real point rendering is needed.
- **Physical device selection.** The backend prefers a discrete GPU that exposes a graphics+present queue family for the surface and the swapchain extension, instead of blindly taking the first enumerated device.
- **Validation.** When `Render.RenderDebug` is set (or in a debug build) and the `VK_LAYER_KHRONOS_validation` layer is available, the backend enables it and routes layer messages to the log as `[VkLayer/…]` through a `VK_EXT_debug_utils` messenger. Use this for backend validation; a correct change should run with zero validation errors.

Vulkan changes should be validated on a platform with the Vulkan SDK by running a visible client with `Render.ForceVulkan=True Render.RenderDebug=True` and confirming the log has no `[VkLayer]` errors.

### SDL_GPU renderer

`Source/Frontend/Rendering-SDLGpu.cpp` implements a second, opt-in backend on top of SDL3's `SDL_GPU` API, which reaches Vulkan / Metal / D3D12 through one implementation (the vendored SDL3 already ships all three drivers). It is built by default (`FO_HAVE_SDL_GPU`), skipped only for headless-only and web builds, and can be force-disabled with `FO_DISABLE_SDL_GPU`; unlike Vulkan it needs no external SDK because the SDL3 GPU drivers are vendored. It is selected at runtime by `Render.ForceSDLGpu` (auto-selection is unchanged — it never becomes the automatic default). The optional `Render.SDLGpuDriver` pins a specific SDL_GPU driver (`vulkan` / `metal` / `direct3d12`); `Render.RenderDebug` maps to the SDL_GPU debug mode (Vulkan validation layers on the Vulkan driver).

Design and important behaviors:

- **Adapts the immediate-mode contract to SDL_GPU's explicit passes.** SDL_GPU records render/copy passes into per-frame command buffers, so the backend keeps a small pass state machine (`Context`): at most one render or copy pass is open at a time, passes begin lazily before the operation that needs them, `ClearRenderTarget` defers into the next render pass load-op, uploads run in copy passes through cycled transfer buffers, and texture readbacks submit the recorded work and wait on a fence.
- **Backbuffer proxy.** The window backbuffer is never rendered directly: `SetRenderTarget(nullptr)` targets an RGBA8 proxy texture (letterbox viewport math shared with the other backends) and `Present()` blits the proxy to the acquired swapchain texture, which keeps mid-frame flushes safe and pipeline color formats uniform.
- **Per-effect pipeline cache.** Graphics pipelines are immutable state objects cached per effect, keyed by pass, topology, depth-target presence, `DisableBlending`, and `DisableCulling`.
- **Consumes the SDL-convention baked flavors, not the native `-spv`.** SDL_GPU mandates a per-stage descriptor convention (vertex samplers = set 0 / UBOs = set 1, fragment samplers = set 2 / UBOs = set 3) that differs from the native Vulkan renderer's 2-set convention (UBO = set 0, sampler = set 1). So the effect baker emits an extra `-spv_sdl` flavor — the native SPIR-V with its descriptor decorations rewritten to the SDL convention — plus SDL-remapped `-msl_*` and an `[EffectInfoSdl]` metadata section (per-stage sampler/UBO counts + dense slot indices). The native `-spv` (consumed by `Rendering-Vulkan`) is untouched. The backend picks `-spv_sdl` for the Vulkan driver or `-msl_*` for the Metal driver via `SDL_GetGPUShaderFormats`, and reads the per-stage slots from `[EffectInfoSdl]`.
- **Push-style uniforms.** Uniform data is pushed with `SDL_PushGPU{Vertex,Fragment}UniformData` (at most 4 slots per stage) and re-pushed on every draw; the effect's public uniform optionals keep their last value to emulate the persistent-buffer semantics of the other backends. The 4-UBO-per-stage limit is enforced by the baker at bake time.
- **`ProjBuf`/`MainTexBuf` are caller-owned when set, renderer-derived otherwise.** `DrawBuffer` auto-fills `ProjBuf` from the renderer's current 2D ortho and `MainTexBuf` from the bound texture size **only when the caller has not already supplied them** (`_needX && !X.has_value()`), then `reset()`s just those two after the draw so the next 2D draw re-derives them. This mirrors the native Vulkan backend and is load-bearing for 3D: `ModelSprites`/`3dStuff` set `ProjBuf` externally to the per-frame model projection before drawing a critter model to its atlas — unconditionally overwriting it with the 2D ortho projects the skinned mesh off-screen, so nothing rasterizes into the model atlas and 3D critters render as name-plates only (the "characters not drawn in SDL" bug). The other externally fed buffers (`EggBuf`, `ModelBuf`, …) are likewise only auto-derived behind `!has_value()` and keep their last value across draws.
- **Shares the engine-wide black-map fixes.** Because it reuses the same baked SPIR-V pipeline (baked with `precision highp float`) and the same epoch-based shader-time wrap in `EffectManager::PerFrameEffectUpdate`, the SDL_GPU backend inherits both Vulkan-only fixes (half-float overflow and `sin(large accumulated time)` NaN) and does not reproduce the black-map failure.
- **Point primitives, orientation, depth.** `POINT_LIST` is remapped to `TRIANGLE_LIST` (shaders lack `gl_PointSize`), mirroring the native Vulkan renderer. `IsRenderTargetFlipped()` is `false` and the ortho matrix uses the `[0,1]` depth convention. Depth targets use `D24_UNORM` when supported, otherwise `D32_FLOAT`. Max atlas size is fixed at 4096 (SDL_GPU exposes no texture-size query).

Validate SDL_GPU changes with a client scene launch under `Render.ForceSDLGpu=True Render.RenderDebug=True` (Vulkan validation on the Vulkan driver), confirming a clean map/GUI render with no validation errors, side by side against the default backend.

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

`EffectManager` in `Source/Client/EffectManager.h` loads minimal/default effects, resolves script-selected effects, writes script-value buffers, and performs per-frame updates. Scripts can write one `ScriptValueBuf` float with `Game.SetEffectScriptValue(...)`, or write a contiguous range with `Game.SetEffectScriptValues(effectType, effectSubtype, valueStartIndex, values, valuesOffset = 0, valuesCount = -1)` to avoid repeated native calls when updating shader parameter blocks. Both APIs validate the selected effect, require the shader to declare `ScriptValueBuf`, and enforce the configured `EFFECT_SCRIPT_VALUES` range.

**Shader time is session-relative and wrapped.** `TimeBuf` (`FrameTime.x` / `GameTime.x`, seconds) is rebased to the first rendered frame and wrapped at 8192 s by `EffectManager::PerFrameEffectUpdate` — it is a periodic animation-phase input, not an absolute clock. The raw steady-clock time is seconds since OS boot (days-scale on long-running machines), and even session-relative time reaches 10^5–10^6 s on clients embedded into long-running servers; at such magnitudes fp32 `fract()`/hash/`sin` math degrades into visible stepping (high-frequency phases like `sin(t * 76)` break within a day) and the clock granularity eventually exceeds the frame delta. The fp32-exact wrap keeps granularity under 1 ms for any session length at the cost of a once-per-~2.3h phase pop, which consumers must keep on noisy/ambient math. Effects that feed the time into hash lattices should still wrap locally (`mod(p, period)` noise lattices in the fog effects); script-side accumulated effect clocks (e.g. weather anim clocks passed through `ScriptValueBuf`) need the same treatment — an fp32 accumulator that only grows will first quantize and then freeze once its ulp exceeds the per-tick increment.

When adding an effect feature, document whether the change belongs in:

- effect config parsing in `Rendering.cpp`;
- backend shader loading/drawing in `Rendering-OpenGL.cpp` or `Rendering-Direct3D.cpp`;
- client effect selection/update code in `EffectManager`;
- map/client draw sequencing in `MapView` or `SpriteManager`.

### Minimal-profile base effects

The engine ships a fixed set of base effects under `Resources/Core/Effects/` (loaded as the default for each draw slot by the `LOAD_DEFAULT_EFFECT` table in `Source/Client/EffectManager.cpp`) plus a few bootstrap effects under `Resources/Embedded/Effects/` (compiled into the binary so the renderer can draw before external resource packs are mounted). Each `.fofx` opens with a top-of-file `#` comment header stating what the effect does, which slot uses it, and how it works.

These base shaders are deliberately written for the **lowest Direct3D feature level** (down to feature level 9_x): no `gl_FragCoord` / position-semantic reads, no screen-space derivatives (`dFdx`/`dFdy`/`fwidth`), and no dynamic array/vector indexing — only constructs that compile and run on the weakest supported hardware. The cross-compiler still emits HLSL Shader Model 4.0, GLSL 330, GLSL ES 300 and Metal for every effect (`Source/Tools/EffectBaker.cpp`), but the *source* must avoid features that fail on the lowest runtime profile. Keep an engine base effect minimal; that is what its header's `Profile: minimal` line records.

Default slot → effect mapping (`Source/Client/EffectManager.cpp`): `Font`/`Iface`/`Generic`/`Critter`/`Rain` → `2D_Default`; `Roof`/`Tile`/`Flat` → `2D_NoDepth`; `Primitive` → `Primitive_Default`; `Light` → `Primitive_Light`; `Fog` → `Primitive_Fog`; `FlushPrimitive`/`FlushMap`/`FlushLight`/`FlushFog`/`FlushRenderTarget` → the matching `Flush_*`; `SkinnedModel` → `3D_Skinned`; `ImGui` → the `ImGuiDefaultEffect` setting (`ImGui_Default`). `2D_WithoutEgg`, `3D_NormalMapping`, `Flush_Map_BlackWhite`, `Font_Default`, `Interface_Default` and the `Particles_*` set are available effects selected per-draw / per-mesh / by the particle system rather than fixed slot defaults.

An **embedding project that targets richer hardware** keeps its own advanced-profile copies in a resource pack that bakes *after* `Core`/`Embedded` under the same resource name, so the project copy shadows the engine base at runtime while the engine keeps the minimal fallback. The richer copy is free to use `gl_FragCoord`, derivatives, per-fragment lighting, and similar; the engine base is not.

## Per-effect depth state and the shared map depth buffer

Effects carry per-pass depth state parsed from the `.fofx` `[Effect]` block:

- `DepthWrite` (default `True`) → `RenderEffect::_depthWrite[pass]` (depth write mask).
- `DepthFunc` (default `Always`) → `RenderEffect::_depthFunc[pass]` (`DepthFuncType`: `Always`/`Never`/`Less`/`LessEqual`/`Equal`/`GreaterEqual`/`Greater`/`NotEqual`). Both `Rendering-OpenGL.cpp` and `Rendering-Direct3D.cpp` translate it (`ConvertDepthFunc`) and the backends diverge in NDC-Z (`[-1,1]` GL/GL-ES vs `[0,1]` D3D), so depth-dependent effects must be validated on both.

The map render target (`MapView::_rtMap`) is created `with_depth`, giving the world one shared depth buffer. `EffectUsage::QuadSprite` and `EffectUsage::Model` effects participate in it (depth state is a hardware no-op on targets without a depth attachment — UI, light, flush-to-screen):

- Screen-space quads (GUI, fonts, render-target blits, and non-map sprite effects) initialize `Vertex2D::PosZ` to `0.0f`; map sprites may start from the same atlas data, but `SpriteManager` overwrites their Z before flushing them into `_rtMap`.
- Standing map sprites (`Item`/`Critter`) use `DepthFunc = LessEqual` with depth writes (`2D_Default.fofx` / `2D_WithoutEgg.fofx`): they both **test against and write** the shared depth buffer, so they occlude each other and the direct-draw particles / 3D models by real depth. Their per-vertex depth is the vertical-billboard proxy (`get_map_sprite_proj` → `GetHexWorldPos`/`ProjectWorldToMap`, anchored on the sprite root; the `MapView::InitView` view layout reproduces `GetHexPos.y` exactly, so the rendered screen position and the depth basis agree). **The depth/sort anchor is the object's LOGICAL root, not the bitmap bottom-center.** For an item the proto `Offset` is the bottom-center→root vector (a tree's trunk): it still positions the bitmap through `MapSprite::_pSprOffset` (so the visual, lighting and `MeasureMapBorders` are unchanged), but it is *also* kept as a separate static root offset (`HexView::_rootOffset` → `MapSprite::_pRootOffset`) that the depth proxy subtracts — in `GetMapRootOffset()` for `sprite_proj.z` and from `scene_pos_y` for the per-vertex reference — so a tall sprite anchors on its trunk instead of `Offset` pixels below it. Without this, the tree's depth anchored at the bitmap bottom (too far south/near) and it wrongly occluded a critter standing in front of it. Critters carry no proto `Offset` (their root comes from the sprite anchor), so their `_rootOffset` is zero. They are the **only** map sprite layer that participates in the depth buffer; every flat/background layer (floor tiles, roofs, flat ground overlays) is painter-only and depth-inert — neither writes nor tests (see below). `MapSprite` receives critter/item `Elevation`; positive elevation shifts the sprite upward in screen Y and increases the same world-Z depth. `MapSprite::HexOffset` plus runtime sprite/tweak offsets are projected along the ground plane before depth is computed, so sub-hex movement changes both screen position and 3D depth continuously; viewport-only `field.Offset` is not part of world depth. The intrinsic `Sprite::Offset` is different: it defines which pixel inside the atlas quad is the logical root on the ground, so vertical depth and direct-to-scene anchors use that root instead of assuming the bitmap's lower center. Floor tile layers (`DrawOrderType::Tile..Tile4`) and flat ground overlays (`FlatItemPreLight`/`HexGrid` pre-light, and `DeadCritter`/`FlatItemAfterLight` post-light — the layers below `NormalBegin`) are upright background sprites: they keep their atlas-provided screen-space XY/UV and **never touch the shared depth buffer**. Entity sprites choose the no-depth effect at the entity level: tiles and roofs resolve `Effects.Tile` / `Effects.Roof`; flat items resolve `Effects.Flat` (`ItemHexView::Init`, by `GetDrawFlatten()`) — all three → `2D_NoDepth.fofx`. Script-created `MapSpriteHolder` sprites have no entity-level effect handle, so `MapView` supplies the pass default effect to `SpriteManager::DrawSprites` from the draw-order segment before batching: `Tile..PreLight` → `Effects.Tile`, `AfterLight..FlatEnd` → `Effects.Flat`, `Roof..RoofParticles` → `Effects.Roof`, and normal layers → `Effects.Generic`. Item draw order is decided by `GetDrawFlatten()`, never by `IsScenery`/`IsWall`: upright → `Item` (the default), flat → `FlatItemPreLight` if `GetStatic()` (drawn pre-light) else `FlatItemAfterLight` (post-light). The former `Scenery`/`Item` (and `FlatScenery`/`FlatItem`) layers were merged, so upright items on one hex no longer force scenery behind items by class — they draw in **add order** (the `MapSprite::_globalPos` tiebreaker once `_drawOrderPos` ties on the merged layer + hex). (Dead critters drawn flattened keep `Effects.Critter` and still write depth; revisit if a corpse ever clips a standing critter.) All of these (`DepthWrite = False` + `DepthFunc = Always`) are drawn before the standing sprites and fully painter-sorted, so they cannot z-fight (coplanar layers), seam (abutting sprites), or clip a standing sprite's feet / a 3D model — they need no per-vertex depth, no ground-plane projection, and no per-layer bias. Standing sprite layers (`Item`, `Critter`) keep atlas XY/UV but write per-vertex `PosZ` through `GeometryHelper::ProjectMapYToVerticalDepth`, so they behave like vertical planes standing on their ground anchor; they carry no draw-order depth bias, because one would pull the vertical plane toward the camera and move the particle-occlusion line off the logical root point. The only remaining depth-bias user is the direct-draw path (particles / 3D models replayed at the end of each sprite pass): `SpriteManager` divides a half-pixel depth budget (`MAP_LAYER_DEPTH_BIAS`) by `DrawOrderType::Last + 1` and gives each direct-draw sprite a single such step above its world depth, keeping it below the subpixel snapping threshold (see *Direct-to-scene sprites*). Both `Core` and `Embedded` `2D_Default.fofx` must project the full `InPosition.xyz`; if an override flattens to `InPosition.xy, 0.0`, particles have no useful scene depth to test against. `2D_Default.fofx` and `2D_WithoutEgg.fofx` discard fragments whose final alpha is at or below `1/255` (after egg alpha), so fully transparent sprite texels do not populate the depth buffer and clip in-scene particles behind the empty parts of the atlas quad.
- Roof tiles (`IsRoofTile`) are ordinary floor tiles given a fixed positive `Elevation` (`Geometry.MapRoofElevation`): the projection raises their screen position onto the building's wall tops (the engine still auto-hides the roof group whose `RoofNum` the camera is inside). A roof is just a tile lifted in screen Y — the flat tile/roof sub-hex XY anchor now lives on the `BaseTile` prototype's `Offset`, not on the former per-side `Geometry.MapTileOffs*`/`MapRoofOffs*` settings (removed); the roof-particle and mapper tile-preview paths read the same `Elevation` instead of the old 2D roof offset. The **roof draw-order range (`Roof..Last`) is rendered as a separate trailing pass** (`MapView::DrawSpritesWithFog` splits at `below_roof = Roof-1` and draws `[Roof..Last]` last, via `DrawFoggedSpriteRange`): everything below it — including the direct-draw 3D models / in-scene particles each sprite pass replays at its end — is drawn first. Like floor tiles, roofs do not touch the depth buffer (`Effects.Roof` → `2D_NoDepth.fofx`, `DepthWrite = False` + `DepthFunc = Always`): being drawn last and never depth-tested, the roof layer always paints on top of the building regardless of the scene depth buffer, and being depth-write-free it never clips anything drawn after it.
- Particle effects (`Particles_*.fofx`) use `DepthFunc = LessEqual` + `DepthWrite = False`: tested against scene depth so they are occluded by closer geometry, without occluding each other.
- Model effects (`3D_*.fofx`) use `DepthFunc = LessEqual` + `DepthWrite = True`: direct-to-scene models write real mesh depth into `_rtMap`, so particles and later direct geometry can test against the model surface instead of the old model atlas quad.
- `OnRenderMap_AfterSpritesAndFog` fires after the sprite/fog map pass and before the map target is flushed. Scripts that need entity debug markers should iterate the relevant visible `Item`/`Critter` objects, combine `Map.GetHexMapPos(entity.Hex)`, `entity.GetSpriteOffset()`, and `entity.Elevation`, then subtract the event draw-area origin; this keeps selection/filtering in scripts without depending on transient sprite instances.
- **Entity contours/outlines are script-driven, not native.** There is no engine contour pass; an embedding-project script (Last Frontier: `ContourPipeline.fos`, compiled for `CLIENT` and `MAPPER`) keeps a cache of entities whose `Contour` colour property is non-zero and, on `OnRenderMap_AfterSprites`, draws each via `Map.DrawEntitySprite(entity, contourEffectSubtype, colour, padding)` (a dilated silhouette in the contour effect, then the sprite on top so only the rim shows). The mapper's selection outline uses the same path — it sets the selected entity's `Contour` property (the property is registered for the mapper because `Client` entity registration includes the mapper target) rather than any native call.

## Direct-to-scene sprites

A `Sprite` may override `IsDirectDraw()` to render its own geometry **straight into the current scene render target** (with the shared depth buffer) instead of being batched as an atlas quad. Because such a sprite uses its own shader (not the sprite batch's), drawing it at its interleaved draw-order position would split the sprite batch around every one. Instead `SpriteManager::DrawSprites` **collects** direct-draw sprites during the batch loop and replays them in a single `Sprite::DrawInScene(scene_pos, depth)` pass (a `const` method, like `FillData`) *after* the whole sprite batch is flushed — so the batch stays intact. Opaque sprites write depth (`DepthFunc = Always`, `DepthWrite = True`) and direct-draw transparents only test it (`LessEqual`, `DepthWrite = False`), so scene occlusion comes from the shared depth buffer. Direct-draw anchors use the projected `hex + HexOffset + SpriteOffset/TweakOffset + Elevation` map position, deliberately excluding viewport-only `field.Offset`, and keep only a single computed anchor-bias step instead of inheriting their late draw order; otherwise `DrawOrderType::Particles` would become depth-closer than critters/scenery before the particle geometry itself is even considered.

`ParticleSprite` supports **two render types**, chosen per particle system by the `SparkQuadRenderer` `draw in scene` `.spark` attribute (`ATTRIBUTE_TYPE_BOOL`, default false — alongside `draw size`):

- **Atlas type** (default, `draw in scene` absent/false): `Update()` advances simulation independently, then refreshes the offscreen atlas (`ParticleSpriteFactory::DrawParticleToAtlas`) at the configured animation cadence; the sprite is drawn as a flat batched quad. `IsDirectDraw()==false`.
- **Scene type** (`draw in scene = true`): `IsDirectDraw()==true`; `Update()` advances simulation even when the sprite is not visible, and `DrawInScene` refreshes the current scene transform without advancing frame time before rendering directly into `_rtMap` through the map view-proj. Particles therefore keep their lifetime offscreen and depth-sort against scene geometry instead of being baked to a flat sprite.

`ParticleSprite::Play()` respawns its backend-neutral `ParticleSystem` before starting updates. The facade delegates through `ParticleRuntimeSystem`; renderer-facing code contains no SPARK/Effekseer dispatch or unnamed default branch. One-shot SPARK systems can therefore be replayed after `Game.PlaySprite(...)` or after `AnimFree`/`AnimLoad` cache reuse.

Seeded respawn is deterministic per particle-system instance in both bundled
runtimes. Effekseer applies the seed to its manager handle. Each
`SparkParticleRuntimeBackend` owns an explicit `SPKContext` containing its IO
registry, default zone, and ambient generator state. Every loaded SPARK graph is
bound to that context before attribute import. Each `SparkParticleRuntimeSystem`
retains its own generator state and temporarily binds it to the owning context
while cloning, prewarming, or updating, so interleaved effects and separate
engine instances cannot perturb a seeded effect's sequence.

`SparkExtension.h` exposes only the backend facade, forward declarations, and
plain renderer data helpers. The SPARK headers, `SparkQuadRenderer`, and its
render-buffer adapter remain private to `SparkExtension.cpp`; Mapper and baker
inspect renderer properties through the data helpers instead of depending on
the concrete renderer type.

`ParticleSystem::SetScale()` updates the cached neutral runtime setup,
reapplies it with a zero-delta transform refresh, and forces an atlas redraw
without respawning or resetting elapsed time. The same contract therefore
applies to atlas and direct-scene sprites and to every enabled particle runtime.

The same sprite and direct-scene paths also host the core-only Effekseer
runtime. Effekseer renderer interfaces are used as evaluated-data callbacks,
not graphics backends: FOnline copies callback values, builds its own
`RenderDrawBuffer`, selects its own `RenderEffect`, and submits through the
normal renderer abstraction. This keeps Mapper and game preview on one path and
requires no Direct3D/OpenGL/Vulkan/SDL GPU code from Effekseer.

The Sprite and Ring callback collectors fail closed on malformed callback
topology. They enforce both the fixed supported-instance hard limit and the
exact instance count declared by `BeginRendering`; subsequent `Rendering`
calls cannot append more instances than that declaration. Ring packets copy
the evaluated outer/center/inner shape and color values, reproduce the upstream
eight-vertex/twelve-index segment topology and angular fades, and preserve
Z-sort order while splitting large geometry at 64,000 vertices for 16-bit index
builds.

`Source/Tests/Test_EffekseerParticleRuntime.cpp` carries self-contained cooked
fixtures that exercise the real Effekseer callback-to-FOnline-draw-buffer path
without a stock Effekseer graphics backend. The legacy fixture verifies
fixed-seed determinism, repeated fixed-step generation, multi-instance
callback-to-draw topology, generated quad geometry and index order, and
atlas-remapped UV coordinates. A project-authored Effekseer 1.80.5 fixture
additionally verifies that cooked `None`, `NormalOrder`, and `ReverseOrder`
Sprite Z-sort modes reach the callback and produce the expected quad depth
order. A modern SKFE/1810 upstream TestData fixture verifies deterministic Ring
topology, radii, UVs and index order, all three Ring Z-sort modes, and chunking
across the 64,000-vertex safety budget that prevents 16-bit index overflow.

The initial callback adapter accepts one Default-material color texture. Ring
nodes may omit it and then draw against a renderer-owned white pixel so their
vertex colors still match Effekseer. For an authored texture, its requested
Linear/Nearest mode must match the loaded FOnline atlas texture, and the sampler
must request `Clamp`; `Repeat` and `Mirror` are rejected regardless of the UV
values. Every textured callback UV rectangle must also stay inside `[0,1]`
instead of silently sampling neighboring atlas content. Per-effect
sub-rectangle wrapping is a separate renderer capability. Modern editor exports
may retain a non-zero distortion-intensity value while the Default material has
distortion disabled; that dormant value is ignored, while an active distortion
material still fails the capability gate.

Effekseer sprites always use the scene type. Direct-scene prewarm is queued
until the first `DrawInScene` after `Setup` has supplied the current map
transform. `ParticleSprite::Update()` does not advance the system while that
request is pending; Effekseer then advances exactly one second and resets the
wall-clock update origin, avoiding a second advance for time spent offscreen
before the first draw. `RefreshRenderTransform()` then performs only an
Effekseer zero-delta transform refresh before drawing; it never enters the
forced first-tick path used by ordinary scheduled simulation.

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
- Renderer changes are tested on the affected backend: null/headless, OpenGL/WebGL, Direct3D, or Vulkan.
- Render-target changes preserve stack push/pop behavior and previous-target restoration.
- Texture orientation changes account for `IsRenderTargetFlipped()` differences between OpenGL (flipped) and Direct3D/Vulkan (not flipped).
- Effect changes document config parsing, shader files, and script-value buffer implications. On Vulkan, confirm shader resources follow the set-0-UBO / set-1-sampler descriptor-set contract.
- Web changes cross-link to [WebDebugging.md](WebDebugging.md); Android changes cross-link to [AndroidDebugging.md](AndroidDebugging.md); native attach/debug changes cross-link to [Debugging.md](Debugging.md).
