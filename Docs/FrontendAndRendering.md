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
- `Source/Client/EffectManager.h`
- `BuildTools/cmake/stages/Packages.cmake`
- `Source/Tests/Test_Rendering.cpp`

## Layer map

The frontend/rendering split has three layers:

1. **Application layer** (`Application`, `AppWindow`, `AppInput`, `AppAudio`, `AppRender`) owns platform services and frame boundaries.
2. **Renderer layer** (`Renderer` and its backends) owns GPU/null rendering resources: textures, draw buffers, effects, matrices, scissor state, presentation, and resize handling.
3. **Client drawing layer** (`SpriteManager`, `RenderTargetManager`, `EffectManager`, `MapView`) builds engine/game drawing operations on top of the renderer abstraction.

This keeps most client code renderer-agnostic. The client asks for sprites, effects, draw buffers, render targets, and input events; the selected backend decides how those are implemented.

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

The client turns these lower-level events into script events in `ClientEngine::ProcessInputEvent()`.

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
- `Vertex2D` and `Vertex3D` — vertex layouts used by sprite and model paths.
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

## Effects and shader data

`RenderEffect` owns standard buffers used by render paths:

- projection/main texture data;
- transparent egg parameters;
- sprite border parameters;
- time/random/script values;
- camera/model/model-texture/model-animation data.

`EffectManager` in `Source/Client/EffectManager.h` loads minimal/default effects, resolves script-selected effects, writes script-value buffers, and performs per-frame updates. When adding an effect feature, document whether the change belongs in:

- effect config parsing in `Rendering.cpp`;
- backend shader loading/drawing in `Rendering-OpenGL.cpp` or `Rendering-Direct3D.cpp`;
- client effect selection/update code in `EffectManager`;
- map/client draw sequencing in `MapView` or `SpriteManager`.

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
