# Web Debugging

> Engine-owned documentation for the reusable Emscripten build, browser package, local web server, and browser-side diagnostics. Project configs, server launch profiles, scenes, ports, public URLs, and editor tasks belong in the embedding project.

Use this guide to prepare the pinned Emscripten workspace, build a browser client, package one or more project configs, serve the generated files without stale caching, and identify whether a failure belongs to the engine web layer or the game project.

## Ownership boundary

The engine owns:

- Emscripten workspace preparation and toolchain selection;
- the `web` client build platform;
- `package-web-debug` and the generated browser package layout;
- the default HTML shell and generated `web-server.py` helper;
- packaging of client resources through Emscripten's file packager;
- engine-side browser runtime and networking behavior.

An embedding project owns:

- its development name and selected config/sub-configs;
- resource baking and server startup;
- host, port, authentication, scene, and gameplay policy;
- browser launch profiles and public reverse-proxy routes;
- web-specific UI/gameplay smoke tests.

Replace `<ProjectDevName>` and `<Config>` below with project-owned values. Do not treat example config names or ports as engine defaults.

## Prepare the web workspace

From an embedding-project root where the engine checkout is `Engine/`, prepare the pinned Emscripten SDK. Include `web-packages` only when provisioning a fresh Linux host that still needs system packages:

```bash
# Fresh host.
bash Engine/BuildTools/prepare-workspace.sh web-packages web

# Host dependencies already exist.
bash Engine/BuildTools/prepare-workspace.sh web
```

The Emscripten version is pinned by `ThirdParty/emscripten` and prepared under `Workspace/emsdk`. BuildTools resolves the toolchain from that workspace rather than relying on an arbitrary system installation.

## Build and package

Build the browser client with debug information:

```bash
python3 Engine/BuildTools/buildtools.py build web client RelWithDebInfo
```

Use `Release` for an optimized package after the debug path is healthy. Generate one browser package per selected project config:

```bash
python3 Engine/BuildTools/buildtools.py package-web-debug <ProjectDevName> <Config> [<Config> ...]
```

Each package is written below:

```text
Workspace/web-debug/<ProjectDevName>-Client-<Config>-Web/
```

The directory contains the browser entry page, JavaScript/WebAssembly artifacts, packaged resources, and a generated `web-server.py`. Treat this directory as build output. Change the engine web template or project config instead of hand-editing generated files.

## Serve locally

Run the generated helper from any working directory; it serves the directory containing the script and defaults to port `7000`:

```bash
python3 Workspace/web-debug/<ProjectDevName>-Client-<Config>-Web/web-server.py --port 7000
```

Open `http://localhost:7000/index.html`. The helper sends `Cache-Control: no-store, no-cache` plus matching `Pragma` and `Expires` headers so rebuilt JavaScript, WebAssembly, and resources are not hidden by browser cache.

Do not open the generated HTML directly through `file://`. Browser security, WebAssembly loading, network requests, and packaged-resource fetches require an HTTP origin.

## Connect to a project server

The package bakes the selected project's client configuration. The project must provide a compatible server and a client-visible `ClientNetwork.ServerHost` / port route for that config.

For local development, decide explicitly whether the browser should connect to:

- a server on the same host;
- another host on the local network;
- a reverse-proxied development endpoint;
- a public staging environment.

Those routes have different origin, TLS, firewall, and proxy requirements. Record them in project documentation and keep secrets out of generated packages. Repackage after config or compatibility changes; restarting only the browser cannot update baked client resources.

Scene selection, loading screens, login, and post-connect UI are game-script concerns. When the network handshake succeeds but gameplay remains blocked, switch to the project's script/UI diagnostics instead of changing the web build pipeline.

## Browser diagnostics

Start with the browser developer tools:

1. **Console:** JavaScript exceptions, WebAssembly aborts, failed assertions, and engine log output.
2. **Network:** missing files, wrong MIME types, stale proxy responses, failed websocket/HTTP requests, and unexpected redirects.
3. **Sources:** generated JavaScript and available source maps for breakpoint inspection.
4. **Application/storage:** cached service data or local storage owned by the project.
5. **Performance/memory:** browser-only stalls or growth after correctness is established.

Keep the terminal running `web-server.py` visible. A browser 404 and a server connection failure are different boundaries even when both end on the same loading screen.

## Troubleshooting by layer

| Symptom | Check first |
|---|---|
| Emscripten workspace is missing | `Workspace/emsdk`, `ThirdParty/emscripten`, and the `web` prepare feature. |
| CMake/toolchain configure fails | The BuildTools `web` platform path and prepared Emscripten environment before project source. |
| Browser package is absent | Successful web client build, exact `<ProjectDevName>`, and selected config names passed to `package-web-debug`. |
| HTML loads but `.wasm` or resources return 404 | Generated package completeness and the directory served by its own `web-server.py`. |
| Browser keeps old code | Response cache headers, unexpected reverse-proxy caching, and the exact package directory/port currently open. |
| Client cannot connect | Baked project host/port, server listen address, browser network request, firewall, TLS/proxy route, and compatibility version. |
| Local HTTP works but public HTTPS fails | Reverse-proxy websocket/TLS configuration and mixed-content restrictions; these are deployment concerns. |
| Loading screen never clears after connect | Project scene/UI remotes and client scripts after proving the engine connection is alive. |
| Debug build works but Release fails | Optimization-sensitive undefined behavior, missing packaged files, and differences in project config. |

## Debug and release separation

`RelWithDebInfo` is the preferred first browser-debug configuration because it retains useful symbols while exercising an optimized-enough runtime. Use a fully optimized `Release` package for performance and production checks only after the same route works in the debug package.

The local generated server is intentionally small and cache-disabled. A production web deployment should use project-owned hosting that provides correct MIME types, compression, TLS, cache policy, websocket/proxy support, and immutable artifact/version routing.

## Source paths inspected

- `BuildTools/buildtools.py`
- `BuildTools/prepare-workspace.sh`
- `BuildTools/package.py`
- `BuildTools/web/default-index.html`
- `BuildTools/web/simple-web-server.py`
- `BuildTools/cmake/stages/Applications.cmake`
- `ThirdParty/emscripten`

## Validation checklist

1. `python3 BuildTools/buildtools.py package-web-debug --help` confirms the project-name/config interface.
2. A web client builds through `buildtools.py build web client RelWithDebInfo` with the pinned workspace.
3. `package-web-debug` produces one complete package per requested config.
4. The generated `web-server.py` serves `index.html`, JavaScript, WebAssembly, and resources without 404s or stale-cache headers.
5. Browser console and network tabs are clean through the first successful connection to a project-owned server.
6. Project-specific URLs, ports, tasks, scenes, and gameplay tests remain in project documentation.

## Troubleshooting

- **The generated web-debug package is missing** -> run `package-web-debug`; a fresh checkout has no browser package until packaging completes.
- **Assets or `.wasm` fail to load** -> inspect the generated package directory and `web-server.py`. Web debug packages use `Raw+WebServer`, so serving a different directory can hide packaged files.
- **The HTTP or game port is already in use** -> stop the stale listener before relaunching. Port values and cleanup tasks belong to the embedding project.
- **The client cannot connect** -> verify the embedding project's `ClientNetwork.ServerHost` and `ServerPort` settings and confirm that its server process started.
- **The web build fails before CMake configuration** -> verify the pinned Emscripten version under `ThirdParty/emscripten` and that the BuildTools web workspace preparation completed.
- **`RelWithDebInfo` links with DWARF** -> Web flags intentionally retain `-g3`; `Init.cmake` suppresses only Emscripten's informational `limited-postlink-optimizations` warning for debug-info configurations. Do not suppress unrelated compiler or linker warnings.
- **An official package fails in `patch_data`** -> the three patch markers (`EMBEDDED_RESOURCES` in generated `EmbeddedResources.gen.inc`, `INTERNAL_CONFIG` in generated `InternalConfig.gen.inc`, and `PACKAGED_MARK` in `Source/Common/Common.cpp`) must survive emcc `-O3 -flto`. Each definition uses `FO_KEEP_DATA_SYMBOL` from `Source/Essentials/BasicCore.h`; use the same macro for new package-time markers so LTO cannot discard their storage.

## See also

- [BuildWorkflow.md](BuildWorkflow.md) for reusable build entry points.
- [BuildToolsPipeline.md](BuildToolsPipeline.md) for workspace and package orchestration.
- [ClientRuntime.md](ClientRuntime.md) for client initialization ownership.
- [Networking.md](Networking.md) for engine transport and connection behavior.
- [AndroidDebugging.md](AndroidDebugging.md) for the sibling external-client platform workflow.
