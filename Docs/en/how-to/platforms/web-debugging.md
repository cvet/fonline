---
layout: default
title: Web Build, Packaging, and Browser Debugging
locale: en
document_id: web-debugging
permalink: /Docs/en/how-to/platforms/web-debugging.html
---

# Web Build, Packaging, and Browser Debugging

This guide is the Engine-owned procedure for preparing the pinned Emscripten toolchain, building and packaging a WebAssembly client, serving it for local diagnosis, connecting it to a project server, and qualifying a browser deployment. It follows the current BuildTools, package shell, Web runtime, networking, renderer, updater, support model, and checked project evidence. An embedding project owns its content bake, server profile, authentication, public origin, browser matrix, deployment, monitoring, and release decision.

## Contract status

This is the production contract for the reusable Web client path at the current Engine revision. Engine source, checked models, and tests are normative. Last Frontier and FOnline TLA are pinned discovery and compatibility evidence only; their task names, ports, domains, tokens, CI jobs, and acceptance coverage do not extend Engine support.

The page is independently usable from an embedding-project checkout in which the Engine lives at `Engine/`. Replace `<ProjectDevName>` and `<Config>` with project-owned values. Project evidence is pinned in `BuildTools/ExternalProjectEvidence.json`; reusable claims are re-derived from Engine-owned source.

Web delivery has four separate evidence layers:

1. the Emscripten C++ client build;
2. the generated browser package and local HTTP load;
3. a real browser connection, render, input, audio, storage, and lifecycle run;
4. production hosting, security, compatibility, observability, rollout, and rollback.

Success at one layer does not qualify the next one.

## Scope and authority

The owning Engine sources are:

- `ThirdParty/emscripten`, `BuildTools/buildtools.py`, and host wrappers for the toolchain pin, workspace preparation, configure, and build runners;
- `BuildTools/cmake/stages/Init.cmake` for the Web platform tuple, optimization modes, WebAssembly memory, WebGL, filesystem, exception, and export flags;
- `BuildTools/PackageInterface.json`, `BuildTools/package.py`, and `BuildTools/cmake/stages/Packages.cmake` for package grammar, binary patching, resource preloading, and output artifacts;
- `BuildTools/web/default-index.html` and `BuildTools/web/simple-web-server.py` for the stock shell, diagnostics, query arguments, and development server;
- `Source/Common/WebRelated.*`, `Source/Frontend/Rendering-OpenGL.cpp`, and application initialization for canvas layout, clipboard, IDBFS hydration, main-loop, error, and WebGL behavior;
- `Source/Client/NetworkClient-Sockets.cpp`, `Source/Common/Settings.inc`, and `Source/Client/Updater.*` for WebSocket selection and update capability;
- `Examples/ContentShowcase`, including its package/runtime contracts and pinned Playwright harness, for the reusable browser fixture;
- `BuildTools/SupportMatrix.json` and `.github/workflows/validate.yml` for the current support label and Engine build gate.

An embedding project owns the `.fomain`, resource bake, selected config, server, WebSocket endpoint, login route, public shell customization, CDN/reverse proxy, browser support statement, performance budgets, analytics, secrets, and release evidence. Do not copy project values into reusable Engine policy.

## Support and qualification matrix

| Layer | Current Engine evidence | What it proves | What remains project-owned |
|---|---|---|---|
| Toolchain | Emscripten `6.0.3` pin and workspace preparer | Reproducible selected SDK input | Host image, cache, mirrors, and outage recovery |
| Build | required `web-client` CI lane on Ubuntu 24.04 | Browser client compiles and links to `Web-wasm` | Game bake, package, server, and browser behavior |
| Renderer | WebGL exactly 2; Vulkan and SDL_GPU excluded | Compiled graphics contract | Browser/GPU/driver matrix and visible correctness |
| Package | Web `Client` + `wasm`, resources required | Stock shell, patched wasm, and preloaded resources can be emitted | Public hosting and immutable release artifact |
| Runtime | Engine canvas, clipboard, IDBFS load, WebSocket, and main-loop code | Reusable mechanisms exist | User-gesture, storage, reconnect, lifecycle, and game-flow acceptance |
| Browser automation | required `web-showcase-runtime` route with pinned Chromium | One deterministic package, network, WebGL 2, lifecycle, and compositor-pixel fixture | Project browser/GPU/device/game-flow release gates |

The supported Engine application is the browser client. Do not infer support for Web server, Mapper, Baker, or other applications from source branches that happen to compile under Emscripten. The `smoke_gated` label qualifies the checked fixture under pinned Chromium; it is not production-browser certification for an embedding game.

## Prepare the host and workspace

`ThirdParty/emscripten` pins `6.0.3`. The preparer removes and reclones `Workspace/emsdk`, installs and activates that exact version with `--build=Release --shallow`, and BuildTools runs configure/build inside its `emsdk_env` script. It does not use an arbitrary system `emcc`.

On a fresh Linux host, provision Node.js, Java, common build packages, and the SDK:

```bash
bash Engine/BuildTools/prepare-workspace.sh web-packages web
```

When Linux host packages already exist, prepare only the SDK:

```bash
bash Engine/BuildTools/prepare-workspace.sh web
```

On Windows, use the checked PowerShell wrapper or direct host-workspace command for the `web` feature. Windows preparation installs the workspace SDK but does not provision every host prerequisite. macOS has no checked Web workspace preparer in the current host map and therefore is not an advertised Web build host.

The selected toolchain is `Workspace/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake`. An explicit `FO_EMSDK` value takes precedence over that inferred workspace location and is normalized before package/build helpers use it; use this override for an already activated reviewed SDK, not to bypass the checked version policy. Windows uses Ninja Multi-Config; Linux uses Unix Makefiles. Keep `Workspace/emsdk` and build/output directories disposable, but keep the pin reviewed in source.

## Build configurations and Web limits

Build the browser client with useful debug information:

```bash
python3 Engine/BuildTools/buildtools.py build web client RelWithDebInfo
```

Use `Debug` when Emscripten assertions and level-2 stack-overflow checks are needed. `RelWithDebInfo` carries `-g3` and an optimized non-Debug link. `Release` and `Release_Ext` add `-O3 -flto`; use them only after the diagnostic route is healthy. A successful native build does not substitute for this target.

The current Web link contract includes:

- target tuple `Web-wasm`, browser OS, wasm architecture, and `.js` executable suffix;
- 16 MiB stack, 256 MiB initial memory, 4 GiB maximum, growth enabled, and LZ4 resource support;
- WebGL minimum and maximum version 2, OpenGL ES mode, no Vulkan, no SDL_GPU, and no linked SDL library (`-sUSE_SDL=0`);
- forced filesystem and IDBFS support, disabled dynamic execution, strict JavaScript checks, and strict undefined-symbol/unimplemented-syscall behavior for the shipping client;
- exported `_main`, `_malloc`, `_free` and the runtime methods required by the stock shell and resource loader;
- WebAssembly exception catching and abort-on-wasm-exception behavior.

The wasm unit-test target alone relaxes undefined-symbol and unimplemented-syscall checks because linked server/database code references unavailable POSIX operations that runnable tests do not call. Never generalize that exception to a shipping client.

Memory growth does not remove the 4 GiB ceiling, browser process limits, allocation spikes, or GPU-resource limits. Establish project budgets with representative content and long-running browser evidence.

## Bake, build, and package

These are separate stages. First bake the selected project's current resources/configuration using its documented bake target. Then build the matching Web client. Finally create one local browser package per config:

```bash
python3 Engine/BuildTools/buildtools.py package-web-debug <ProjectDevName> <Config> [<Config> ...]
```

The helper does not build or bake. It resolves the project git `HEAD` as the build hash, consumes binaries from the configured output plus project inputs, invokes `package.py` as `Client Web wasm Raw+WebServer`, and writes:

```text
Workspace/web-debug/<ProjectDevName>-Client-<Config>-Web/
```

If source, scripts, resources, `.fomain`, Engine compatibility, or the selected config changed, rebake/rebuild/repackage the affected layer. Refreshing a browser cannot repair stale generated inputs.

## Browser package contract

A stock raw Web package contains:

- `index.html` from the Engine shell;
- `<ProjectDevName>_Client.js` and `<ProjectDevName>_Client.wasm`;
- `Resources.data` and `Resources.js` generated by the pinned Emscripten `file_packager.py` with `--preload` and `--lz4`;
- optional `web-loading-image.<ext>` from `Web.LoadingImage`;
- `web-server.py` when the `WebServer` pack token is selected;
- an archive as well when a project package declaration also selects `Zip`.

Web packaging supports only the `Client` target, only the `wasm` architecture, and requires resources; `NoRes` is rejected. The packager copies JavaScript/WebAssembly, patches embedded resources, effective config, and packaged build name into the wasm, preloads the configured client resource directory, and removes that unpacked directory after `Resources.data` is produced.

`Web.LoadingImage` is resolved relative to the main `.fomain` and packaging fails when the configured file is missing. `Web.BackgroundColor` defaults to `rgb(0, 0, 0)`. Treat the output directory as generated and immutable: change the owning template/config and rebuild instead of hand-editing release files.

For release declarations, use the checked package grammar and [Packaging and Release](../release/packaging.md). A package declaration still needs a fresh bake, matching build hash, complete artifact, provenance, and browser acceptance.

## Shell arguments and secret boundary

The stock shell converts every non-empty URL `key=value` query pair into `--key value` in `Module.arguments`. This permits typed launch overrides such as:

```text
http://localhost:7000/index.html?ClientNetwork.WebSocketHost=127.0.0.1&Network.WebSocketPort=4026&Network.SecuredWebSockets=False
```

With no query, the client uses its packaged effective configuration. Query keys are not allowlisted by the shell; Engine setting validation remains authoritative.

The complete URL is visible to browser history, screenshots, copied links, reverse proxies, access logs, telemetry, referrers, support tools, and anyone with page access. Never put passwords, long-lived bearer tokens, signing material, database strings, or reusable administrator credentials in query parameters. If a project uses a browser login token, it must define a narrow audience, short lifetime, single-use/revocation behavior, transport protection, logging redaction, and incident response in its own threat model. See [Security and Secrets](../release/security-and-secrets.md).

The shell loads `Resources.js` before the main client JavaScript, changes the virtual working directory to `/`, exposes a `#canvas`, and keeps the last 200 console entries for the in-page log/error panel. `F8` toggles the panel. `window.foShowError`, JavaScript errors, unhandled promise rejections, and stderr containing error/exception text feed the visible diagnostic surface; stdout and stderr are also mirrored to the browser console.

## Serve a local package

Run the generated development helper from any working directory; it always serves the directory containing the script:

```bash
python3 Workspace/web-debug/<ProjectDevName>-Client-<Config>-Web/web-server.py --port 7000
```

Open `http://localhost:7000/index.html`. Do not use `file://`: WebAssembly, packaged-resource fetches, browser security, clipboard, storage, and networking require an HTTP origin.

The helper is intentionally minimal:

- it defaults to port `7000` and sends no-store/no-cache headers;
- it uses a threaded Python server and binds `('', port)`, which exposes the listener on all host interfaces allowed by the OS/firewall;
- `--fork` forks only where `os.fork` exists and is effectively a no-op on Windows;
- it has no TLS, authentication, access control, explicit wasm MIME override, COOP/COEP policy, compression, health check, or production hardening.

Use it only with non-sensitive development artifacts on a trusted network, close the listener after use, and apply host firewall policy. Prefer a project test server bound to `127.0.0.1` for automated local runs. Never publish `web-server.py` as the production origin.

## Connect to a project server

The Web client always uses `ClientNetwork.WebSocketHost` and `Network.WebSocketPort`. `Network.SecuredWebSockets` selects `ws://` or `wss://`. Native `ClientNetwork.ServerHost` / `Network.ServerPort` are not the Web transport endpoint, UDP is disabled by the Web build, and the native proxy path is not available.

Start a compatible project server with its WebSocket listener, then verify the endpoint from browser DevTools. A page served over HTTPS normally must connect through `wss://`; an insecure `ws://` request is mixed content and is expected to be blocked. The project owns certificate names, TLS termination, reverse-proxy upgrade headers, origin policy, firewall exposure, rate limits, authentication, compatibility errors, and reconnect behavior.

Separate these failures:

1. HTTP cannot load package files;
2. JavaScript/WebAssembly initialization aborts;
3. WebSocket DNS/TCP/TLS/upgrade fails;
4. Engine handshake or compatibility fails;
5. login, scene, or gameplay scripts fail after connection.

A shared loading screen does not make them one problem. Keep browser console/network evidence and the matching server/proxy logs.

## Browser runtime behavior

### Canvas and rendering

The runtime targets `#canvas` and creates a WebGL2 context. It listens for window, `visualViewport`, and fullscreen changes. Adaptive sizing clamps page dimensions, configured min/max values, height percentage, aspect factor, and position factors; fullscreen centers the canvas. `Module.foScreenWidth` and `Module.foScreenHeight` can override calculated dimensions when a custom shell sets nonzero values.

The stock shell reports WebGL context loss and requires a reload. The Engine does not promise transparent context restoration. Qualify resizing, fullscreen, high-DPI behavior, orientation, browser zoom, context loss, background/resume, and representative rendering in the project's supported browser matrix.

### Main loop, audio, and input

The Web main loop is installed through `emscripten_set_main_loop_arg(..., 0, 1)`. Browser scheduling, throttling in background tabs, visibility changes, and user-gesture rules remain browser behavior. The exported runtime includes Emscripten's audio-context resume helper, but a project must still prove first-use audio activation, mute/unmute, interruption, resume, and device changes through real interaction.

Canvas copy events use the Engine clipboard. The runtime requests clipboard-read permission on the first pointer interaction when APIs exist and intercepts non-repeated `Ctrl+V`; it falls back to the Engine clipboard when navigator access fails. Clipboard APIs depend on secure contexts, permissions, focus, and user gestures, and several failures are intentionally swallowed. Test paste/copy visibly instead of treating absence of an exception as success.

### Persistent data

The runtime creates `/PersistentData`, mounts IDBFS, calls `FS.syncfs(true)`, and delays normal startup until initial browser-to-virtual-filesystem hydration completes. The callback currently marks readiness even when `err` is non-null. The audited generic path does not prove automatic write-back after every later modification.

Therefore do not promise durable saves merely because IDBFS mounted. The project must define what data belongs there, when writes are flushed, quota/eviction behavior, private/incognito behavior, schema/version migration, corruption recovery, user data deletion, and multi-tab conflict policy. Test cold reload and browser restart using the exact production origin; storage is origin-scoped.

### Logs and fatal errors

File logging and asynchronous file-log writing are disabled on Web. The primary reusable evidence is the browser console plus the stock in-page panel. Capture console entries, JavaScript errors, unhandled rejections, page crashes, network traces, server logs, and exact artifact revisions. A copied error panel is useful but does not replace the earlier console/network timeline.

## Browser diagnostics

Use DevTools in this order:

1. **Console:** Engine stdout/stderr, JavaScript exceptions, WebAssembly aborts, failed assertions, and WebGL messages.
2. **Network:** `index.html`, main `.js`, `.wasm`, `Resources.js`, `Resources.data`, status, MIME, content encoding, cache headers, redirects, and WebSocket upgrade/frames.
3. **Sources:** generated JavaScript, WebAssembly debugging support, and source artifacts actually emitted by the selected Emscripten configuration. `-g3` does not by itself promise a separate source-map file in every package.
4. **Application/storage:** origin, IndexedDB/IDBFS state, quota, cache/service-worker state added by the project, and data after reload.
5. **Performance/memory:** long tasks, frame pacing, heap growth, GPU pressure, resource download/decompression, and background throttling after correctness is established.

Preserve the page URL without secrets, browser/version, OS/GPU, package hash, Engine/project revisions, server config, proxy headers, and reproduction steps. Compare a local raw package with the public origin to isolate hosting from runtime.

## Native updater and redeployment

The updater identifies this platform as `Web` / `Web-wasm`, uses `/` as the virtual runtime root, and returns false from `CanSelfUpdateNativeModules`. A browser client cannot repair an incompatible native/WebAssembly generation by downloading a replacement module in place.

Publish matching `index.html`, main JavaScript, wasm, and resource pair as one versioned artifact. Avoid a deployment window where a cached shell loads a new wasm with old resources or vice versa. Use versioned directories or another atomic switch, explicit cache policy, health/smoke checks, and rollback to a complete prior artifact. Reload/redeploy a compatible package for native changes; distinguish that from any project resource-update path.

## Production hosting and security

The production origin must provide at least:

| Concern | Required project decision/evidence |
|---|---|
| MIME | `.wasm` served as `application/wasm`; JavaScript/data/image types correct; no HTML fallback for missing artifacts |
| TLS | HTTPS origin, valid certificate chain/name, `wss://` game route, secure redirects, and no mixed content |
| WebSocket proxy | Upgrade/connection forwarding, timeouts, frame/body limits, origin policy, client IP/trust boundary, and useful failure logs |
| Artifact atomicity | shell/JS/wasm/resources from one revision, versioned URL or atomic switch, integrity/provenance, and complete rollback |
| Cache | short/no-cache policy for mutable entry points; reviewed immutable policy only for content-addressed/versioned assets |
| Compression | browser/proxy-tested Brotli/gzip policy without double-compressing or corrupting wasm/data; range behavior where used |
| Isolation headers | deliberate COOP/COEP/CORP/CORS policy when SharedArrayBuffer/threads or project assets require it; third-party compatibility tested |
| Security headers | CSP and embedding/frame policy compatible with Emscripten and approved integrations; referrer and permissions policy reviewed |
| Secrets/privacy | no reusable secrets in package/query/logs; storage, telemetry, consent, retention, and deletion documented |
| Operations | health checks, synthetic browser smoke, error/performance telemetry, alerting, staged rollout, and rollback exercise |

The stock helper does not satisfy this table. Hosting headers can also break a package that works locally, so qualify the actual public route and CDN/proxy configuration.

## Browser and release acceptance matrix

| Route | Minimum project evidence | Failure signal |
|---|---|---|
| Artifact load | fresh origin loads HTML, JS, wasm, and resources from one revision with correct MIME/status | 404, HTML-as-wasm, stale mixed revision, decompression failure |
| Startup | wasm initializes, IDBFS hydration completes, first rendered frame appears | abort, permanent loading state, memory ceiling, storage exception |
| Rendering | representative GUI, fonts, images, models/sprites, effects, resizing, fullscreen, and context-loss policy | blank/corrupt frame, clipping, shader failure, unrecoverable resize |
| Input/clipboard | mouse, keyboard, wheel, touch where claimed, focus, paste/copy, and modal interaction | duplicate/lost input, blocked clipboard, unusable focus |
| Audio | user-gesture activation, playback, mute, interruption, background/resume, and claimed browsers | suspended context, silence, duplicate playback, lost device |
| Networking | `wss://` connect, authentication, compatibility rejection, reconnect, latency/loss, proxy timeout, and server restart | mixed-content block, failed upgrade, silent timeout, reconnect loop |
| Persistence | cold reload/restart, quota/eviction simulation, migration, corruption recovery, private mode, and deletion | lost/stale save, startup hang, cross-version corruption |
| Lifecycle | hidden/background tab, throttling, resume, browser navigation, refresh, multi-tab policy | runaway loop, stale socket, duplicated session, unrecoverable state |
| Performance | download/startup budget, frame time, memory growth, long representative session, low-end supported device | budget regression, unbounded heap/GPU growth, tab kill |
| Security | headers, origin/CORS, token lifetime/redaction, dependency/shell review, abuse/rate-limit behavior | leaked query/token, permissive origin, blocked required asset |
| Rollout | canary/synthetic smoke against real origin, observability, complete prior artifact, practiced rollback | local pass but public failure, mixed release, no recovery path |

The Engine build lane intentionally does not supply this browser/release evidence. A project may call Web production-supported only when its applicable rows are repeatable and required by its release gate.

## Troubleshooting by layer

| Symptom | Inspect first |
|---|---|
| Workspace prepare fails | host Node/Java/common packages, network, disk, exact `6.0.3` pin, `Workspace/emsdk`, and activation logs |
| Configure uses the wrong compiler | BuildTools `web` platform, workspace toolchain path, `emsdk_env`, and stale build directory |
| Link fails only for Web | strict undefined/unimplemented syscall output, unsupported native dependency, Web platform guards, and exception flags |
| Package is absent | successful bake/build, exact dev name/config, project git revision, `FO_OUTPUT`, and `package-web-debug` log |
| Package rejects resources | current effective config, `Baking.ClientResources`, fresh metadata, no `NoRes`, and loading-image path |
| `index.html` loads but wasm/data does not | served package directory, status/MIME, HTML fallback, proxy rewrite, compression, and cache |
| Browser shows old code | exact origin/path, service worker or CDN added by project, entry-point cache, versioned artifact, and package hash |
| WebGL2 context cannot start | browser/WebGL2 availability, GPU/driver/blocklist, software-rendering policy, console, and context attributes |
| Client cannot connect | `WebSocketHost`, `WebSocketPort`, secure flag, DNS/TLS, mixed content, proxy upgrade, firewall, and server listener |
| Socket connects but login stalls | protocol/compatibility, authentication, server/client logs, then project script/UI flow |
| Clipboard is empty | secure context, permission, user gesture, focus, browser policy, navigator fallback, and visible Engine state |
| Data disappears after reload | production origin, IDBFS hydration, explicit later flush path, quota/eviction/private mode, and schema migration |
| Audio stays silent | user gesture, suspended audio context, tab visibility, browser autoplay policy, mixer/device, and project assets |
| Release fails but local helper works | public MIME/TLS/cache/compression/COOP/COEP/CSP/CORS/proxy headers and mixed artifact revisions |
| Client requests an incompatible native update | Web native self-update is unsupported; deploy and reload a compatible complete browser artifact |

Keep evidence per layer: workspace/configure/build log, package log and file inventory, HTTP headers, browser console/network trace, WebSocket frames, server/proxy logs, storage state, screenshot/video where visual, and exact revisions. Do not collapse every failure into "Web does not work."

## Project evidence and extraction rules

`Examples/ContentShowcase` is the Engine-owned reusable baseline. Its `web-showcase-runtime` route force-bakes on the native host, builds and verifies the raw/ZIP Web payload, starts the native server and generated HTTP server, requires successful `index.html`, JavaScript, WebAssembly, and resource responses, observes client/server readiness markers, creates a 1280 x 800 WebGL 2 drawing buffer in pinned Chromium, rejects console/page/network/Engine failures, and validates a compositor screenshot by content regions. The checked WebGL capture and machine record are local fixture evidence. They do not qualify production headers, public origin, authentication, persistence, audio activation, long-session behavior, or a game's supported browser/GPU matrix.

The pinned Last Frontier snapshot demonstrates project-owned Web settings and secure deployment profiles, local VS Code build/package/server/Chrome tasks, reusable package declarations, and a required nightly/manual Linux-Web pipeline. Its project runner binds a random loopback port, forces `application/wasm`, adds COOP/COEP and no-cache headers, launches Playwright Chromium with software WebGL, captures console/page errors/crashes, and tests packaged WebSocket login, token login, and a deterministic rendering/combat workload. This is a strong project qualification pattern, not an Engine support promise.

The pinned FOnline TLA snapshot independently carries Emscripten CMake presets and Web settings, but this audit found no equivalent checked browser-package/Playwright qualification lane. It is useful configuration/build-discovery evidence, not a browser release standard.

Promote a project observation only when the reusable mechanism and focused tests live in Engine. Keep these project-owned:

- domain, ports, certificates, proxy/CDN configuration, browser allowlist, auth/token policy, and public shell integrations;
- CI job names, editor launch tasks, scenes, accounts, gameplay markers, telemetry, budgets, artifacts, rollout, and rollback;
- storage schema/flush policy, reconnect semantics, application lifecycle, accessibility, privacy, and product security review.

Absence of a project browser lane is evidence of a gap, not evidence that browser behavior is acceptable.

## Maintenance triggers

Re-audit this guide in the same change when any of these move:

- Emscripten pin, host feature map/packages, workspace layout, environment wrapper, generator, configure/build command, platform tuple, or support/CI label;
- Web compile/link flags, memory/stack limits, exception policy, exports/runtime methods, filesystem, WebGL/backend selection, or wasm test exception;
- Web package platform/target/arch/pack grammar, binary patching, resource preloading/LZ4, shell substitutions, loading image/background, output naming, or archive behavior;
- default shell query parsing, load order, log/error panel, canvas, stdout/stderr, context-loss behavior, or development-server bind/cache/MIME/header behavior;
- canvas layout/settings, clipboard permissions/events, IDBFS mount/sync readiness, main loop, audio activation, logging, or fatal-error reporting;
- WebSocket host/port/secure selection, UDP/proxy availability, server WebSocket/TLS contract, updater platform/root, or native self-update capability;
- Content Showcase package/runtime/marker/pixel contracts, pinned Playwright version, production hosting/security/acceptance requirements, or Last Frontier/TLA evidence revision/path.

Update project integration docs in the same revision where observable project behavior changes. Run the focused Web documentation test, package/security/support tests, generated documentation gates, the Engine Web build lane, and every affected project browser/package/deployment route.

## Validation routes

From the Engine root, run the source-backed checks:

```bash
python3 BuildTools/tests/test_docs_web_debugging.py
python3 BuildTools/tests/test_docs_package.py
python3 BuildTools/tests/test_package_security.py
python3 BuildTools/tests/test_docs_support_matrix.py
python3 BuildTools/docs_validate.py
```

Run `BuildTools/validate.sh web-showcase-runtime` on Linux or `BuildTools\validate.cmd web-showcase-runtime` on Windows for the isolated reusable package/browser fixture. For a behavior change, also prepare the pinned SDK, build the Web client in the affected configurations, freshly bake/package a public minimal project, inspect every output and HTTP header, and execute applicable rows of the browser/release acceptance matrix against the real project server and production-like origin. A host-only documentation test, successful Emscripten link, or localhost fixture cannot replace project browser evidence.

## See also

- [Support Matrix](../../reference/platforms/support-matrix.md)
- [Build Workflow](../build/)
- [BuildTools Pipeline](../../reference/cmake-and-buildtools/pipeline.md)
- [Packaging and Release](../release/packaging.md)
- [Security and Secrets](../release/security-and-secrets.md)
- [Client Runtime Split and Updater](../../explanation/runtime/client-updater.md)
- [Networking and Authority](../../explanation/authority-and-networking/)
- [Gameplay and Integration Testing](../testing/gameplay-and-integration.md)
- [Android Build, Packaging, and Device Debugging](android-debugging.md)
