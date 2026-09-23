---
permalink: /Examples/ContentShowcase/README.html
locale: en
document_id: content-showcase-readme
---

# FOnline Content Showcase

A standalone, presentation-focused FOnline project that demonstrates the
content pipeline without depending on Last Frontier, TLA, or another game.

![Direct3D 11 capture of the FOnline content gallery](captures/windows-direct3d11.png)

![OpenGL capture of the FOnline content gallery on Linux CI](captures/linux-opengl.png)

![WebGL 2 capture of the packaged FOnline content gallery](captures/web-webgl2.png)

The checked project covers:

- generated TGA source images and a six-frame FOFRM animation;
- authored `NextX`/`NextY` root-motion metadata;
- a static light source and a custom additive `.fofx` effect;
- a bounded SPARK particle system and short WAV effect;
- a source `.fomap` with explicit placement identifiers;
- English and Russian prototype text;
- headless source, content, and client/server lifecycle checks;
- a visible Direct3D capture with machine-checked composition regions;
- native Windows/Linux presets, an Xvfb/Mesa OpenGL capture lane, plus WebAssembly compilation, packaging, and browser runtime lanes;
- machine-readable asset rights, performance budgets, and capture provenance.

This is a rendering and authoring example, not a gameplay starter. Begin with
[`MinimalProject`](../MinimalProject/README.md) and
[`MinimalMultiplayer`](../MinimalMultiplayer/README.md) when learning the
project lifecycle or client/server interaction.

## Prerequisites

- CMake 3.22 or newer;
- Python 3;
- Node.js 20 or newer and the Emscripten SDK for Web runtime validation;
- Visual Studio with C++ support on Windows, or GCC and Ninja Multi-Config on
  Linux;
- Xvfb, Xauth, and Mesa software rendering packages for a Linux reference capture;
- an Engine checkout available at `Engine/`.

For a standalone checkout, initialize the exact Engine submodule recorded by
the repository. From the Engine source tree, the local development fixture may
instead use a junction or symbolic link from this directory back to the Engine
root.

## Validate the native project

Windows:

```powershell
python validate.py
```

Linux:

```bash
python3 validate.py
```

The validator checks deterministic source assets and their SHA-256 records,
regenerates and checks the complete `.fomain`, configures the host preset,
bakes all resources, builds the client and server, runs an isolated content
test, and completes a real headless client/server session. The runtime session
loads the TGA, FOFRM, SPARK, effect, and WAV resources before it may pass.

## Produce the reference capture

On Windows after configuration:

```powershell
cmake --build --preset windows-capture
```

On Linux after configuration, run the visible client inside an X11 display. The
required workflow installs the same pinned package group and forces software
Mesa for reproducibility:

```bash
LIBGL_ALWAYS_SOFTWARE=1 SDL_VIDEODRIVER=x11 \
  xvfb-run --auto-servernum --server-args="-screen 0 1280x800x24" \
  cmake --build --preset linux-capture
```

The target starts a real Direct3D 11 or OpenGL client at 1280 x 800 and records
twelve warm capture samples. `capture_showcase.py` rejects torn or incomplete samples
unless the header, runtime map, source gallery, and provenance footer are all
present, then stores the strongest complete frame in the backend-specific path
under `captures/`. The capture contract records the Engine
revision, source digest, selected sample, image hash, viewport, backend, date,
pixel evidence, and the GitHub run identity when it executes in CI.

The checked Windows Direct3D 11 and Chromium WebGL 2 images are local evidence.
The checked Linux OpenGL image is retained CI evidence from
`cvet/fonline-content-showcase` run `30937990249`; its archive digest, image
hash, process report, exact Engine pin, and pixel regions were verified before
the capture was admitted here. The Web record proves the packaged client loaded every required response,
connected to the native server, reached `resources_loaded` and `world_ready`,
created a 1280 x 800 WebGL 2 drawing buffer, and passed the same pixel-region
checks as the native captures. The required `linux-showcase-capture` job owns
the reproducible software-Mesa route and immutable workflow artifact; rerun it
whenever a recapture trigger changes the checked evidence.

## Validate the Web build

Activate the Emscripten SDK and expose its root as `FO_EMSDK`, then run:

```bash
python3 validate.py --web
```

The Web lane validates the authored-source contract, configures with the Engine
Emscripten toolchain, builds the browser client, and checks for nonempty
JavaScript and WebAssembly delivery artifacts. It is the fast compile-only
gate; it does not claim package or pixel evidence.

Build and inspect the complete delivery package with native-host baking:

```bash
python3 validate.py --web-package
```

This route force-bakes with the native Windows/Linux baker, builds the Web
client, creates the raw and ZIP payloads, and verifies the exact six-file
inventory, WebAssembly magic, `Resources.data`, LZ4 loader, generated Web page,
archive parity, and CRC. The explicit `FO_EMSDK` environment variable takes
precedence over any SDK inferred from the Engine workspace.

Install the example-owned pinned browser dependency once, then run the complete
non-mutating browser check:

```bash
cd WebTests
npm ci
npx playwright install chromium
cd ..
python3 validate.py --web-runtime
```

The runtime route includes the package route, starts the native headless server
and packaged HTTP server, observes all required HTTP responses and client/server
markers, requires a WebGL 2 context, rejects console, page, network, Engine, and
script failures, and checks the compositor screenshot with the native pixel
contract. Runtime output stays under `Workspace/`; maintainers use
`capture_showcase_web.py --update-contract --output captures/web-webgl2.png`
only when intentionally replacing the checked reference.

## Source map

| Path | Ownership |
|---|---|
| `ShowcaseAssets/Showcase/` | Redistributable image, animation, effect, particle, and audio sources |
| `Content/ShowcaseContent.fopro` | Critter, item, light, particle, and location prototypes plus EN/RU text |
| `Maps/ShowcaseMap.fomap` | Compact gallery layout with stable placement identifiers |
| `Scripts/Showcase.fos` | Login, lifecycle checks, resource loading, visible composition, and capture sampling |
| `assets/provenance.json` | Exact source path, license, origin, and SHA-256 for every distributed asset |
| `quality/performance-budget.json` | Asset-size, map-density, particle, audio, viewport, and FPS limits |
| `captures/capture-contract.json` | Per-backend evidence state and recapture triggers |
| `showcase-web-package.json` / `showcase-web-runtime.json` | Exact packaged payload, browser, network, marker, viewport, and failure contracts |
| `WebTests/` | Example-owned, lock-file-pinned Playwright runtime |
| `generate_assets.py` | Pure-standard-library deterministic TGA and WAV generation |
| `generate_config.py` | Full Engine-settings-derived project configuration |

Runtime resource names begin at `Showcase/`; `ShowcaseAssets` is only the
project-side data-source directory. Keeping authored input separate from
`Resources-Baked`, `ServerResources`, `Cache`, and `PlatformBinaries` prevents
local outputs from becoming accidental source.

## Authoring practices demonstrated

1. Keep source rights explicit. Every distributable asset is listed once with
   its exact digest and is covered by [`ASSET_LICENSE.md`](ASSET_LICENSE.md).
2. Keep expensive content bounded. Particle capacity, static item count, audio
   duration, source bytes, viewport, and target FPS are reviewed data rather
   than informal promises.
3. Test each boundary separately. Source checks prove bytes and references;
   baking proves converters; content tests prove prototypes and map structure;
   headless smoke proves loading and lifecycle; visible captures prove pixels.
4. Preserve authoring metadata. FOFRM frame timing and every `NextX`/`NextY`
   value are checked instead of treating the descriptor as an opaque file.
5. Keep backend claims evidence-based. Direct3D 11 and WebGL 2 have separate
   checked captures; neither implies OpenGL visual acceptance.

## Engine updates

An Engine revision change is a documentation-bearing change. In the same pull
request:

1. record the old and new exact Engine revisions;
2. review the complete incoming Engine range for image, effect, particle,
   audio, map, renderer, screenshot, configuration, CMake, and Web changes;
3. regenerate `FOnlineContentShowcase.fomain` and the deterministic assets;
4. run native source/content/runtime checks and every affected platform lane;
5. recapture each affected backend and update its evidence record;
6. update this English document and `README.ru.md` when commands, behavior,
   budgets, limitations, or evidence change.

Do not refresh only hashes after a visual difference. Explain the change,
review the decoded image, and retain the new capture only when the composition
is intentional.

## Honest limitations

- The gallery has no production authentication, persistence, combat, economy,
  deployment, installer, signing, or rollback policy.
- The headless lane confirms that audio is accepted, not that it is audible or
  mixed correctly on a real device.
- The sample demonstrates 2D content; a 3D model was intentionally omitted to
  keep source and rights review compact.
- FOFRM root-motion metadata affects presentation frames here and does not move
  an authoritative game entity.
- Public repository visibility, immutable tags, hosted Web output, and
  Linux OpenGL release evidence remain owner-gated publication work.
