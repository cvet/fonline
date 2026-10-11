---
layout: default
title: Animation and Particle Viewers
locale: en
document_id: viewer-tools
permalink: /Docs/en/how-to/tools/animation-particle-viewers.html
---

# Animation and Particle Viewers

> Engine-owned documentation for the focused AnimationViewer and
> ParticleViewer tools. Project asset catalogs, visual acceptance criteria,
> build presets, package composition, and retained review captures belong to
> the embedding game project.

## Purpose

Use the focused viewers to inspect one critter or one baked particle effect
without starting a server, entering a map, or navigating the gameplay client.
Both tools use the same client-side prototype, resource, sprite, model,
effect, font, and renderer services as the game, but drive a minimal
viewer-only frame loop.

The viewers answer focused content questions:

| Question | Tool |
|---|---|
| Which animation pairs does this critter expose, and do its scale, facing, root, frame, hierarchy, and attachments look correct? | AnimationViewer |
| Does this baked particle load through the selected runtime, and how do seed, prewarm, direction, scale, frame, and direct-scene drawing affect it? | ParticleViewer |
| Is the asset placed at the correct map depth and position? | Mapper |
| Does the asset behave correctly with gameplay timing, attachments, visibility, networking, and load? | A representative client scene |

For a focused inspection request, make the decision in this order:

1. Select AnimationViewer for critter clips, `1.00x` scale, `Angle`, `Root`,
   one-shot versus `Loop`, hierarchy, layers, and attachments. Select
   ParticleViewer for a baked particle identity, backend, `Seed`, `Prewarm`,
   `Direction`, scale, frame, and direct-scene route.
2. Record exact Engine/project revisions, configuration, prototype or resource,
   renderer, controls, and logs. For particles, pin `Seed`, `Prewarm`, and
   `Direction`, use `Replay` after a change, and compare atlas/frame drawing
   with `Draw in scene`.
3. Treat a process-start smoke and focused-viewer pixels as focused evidence
   only. Continue through Mapper for placement/depth/composition and a
   representative client scene for timing, attachments, visibility,
   networking, renderer behavior, and load.
4. Retain each layer's captures and provenance separately; one viewer screenshot
   cannot promote an asset to production-ready.

When a launch command is needed, copy the complete platform block from
[Build and launch](#build-and-launch), including its working-directory change
and the exact `--config` spelling. Do not synthesize a shorter cross-platform
path from a project target name.

The focused viewers are inspection tools, not authoring editors. Mapper edits
maps and SPARK sources. The external Effekseer Editor edits `.efkproj`
sources. The current Engine has no generic Editor application and no
AssetExplorer application or `Source/Tools/AssetExplorer.*` implementation.

## Production review contract

A production asset passes through distinct evidence layers. Do not collapse
them into one viewer screenshot:

1. validate authored source and bake output with the owning format tools;
2. use the focused viewer to isolate identity, animation or seed, facing,
   scale, bounds, hierarchy, and render-route defects;
3. use Mapper to prove map placement, depth, and composition;
4. use a representative client scene to prove gameplay timing, attachment
   ownership, visibility, networking, renderer behavior, and load;
5. retain the exact revisions, configuration, controls, logs, and captures
   needed to reproduce the accepted case.

A process-start smoke proves only that startup reached the frame loop. An empty
MinimalMultiplayer window proves the target and asset-free path, not content
quality. A focused viewer capture proves only the isolated state visible in
that capture. State these evidence limits in reviews and automation reports.

Projects should expose stable build and launch tasks for both viewers and put
the binaries only in a developer package that already supplies compatible
resources. Pin the project to an exact Engine revision, rebuild or rebake after
that pin changes, and repeat every affected layer. Last Frontier follows this
shape with project-named viewer targets, VS Code launch tasks, a developer-only
package, model review in `CharacterGenerator.md`, and particle acceptance in
`Particles.md`. Those project names, assets, and policies are examples rather
than Engine contracts. FOnline TLA currently supplies no equivalent focused
viewer workflow and is not normative evidence for these tools.

## Source paths inspected

- `Source/Tools/AnimationViewer.h`
- `Source/Tools/AnimationViewer.cpp`
- `Source/Tools/ParticleViewer.h`
- `Source/Tools/ParticleViewer.cpp`
- `Source/Tools/Mapper.h`
- `Source/Tools/Mapper.cpp`
- `Source/Applications/AnimationViewerApp.cpp`
- `Source/Applications/ParticleViewerApp.cpp`
- `Source/Common/SettingsStorage.h`
- `Source/Common/SettingsStorage.cpp`
- `Source/Client/ResourceManager.cpp`
- `Source/Client/ParticleSprites.h`
- `Source/Client/ParticleSprites.cpp`
- `BuildTools/cmake/stages/Init.cmake`
- `BuildTools/cmake/stages/EngineSources.cmake`
- `BuildTools/cmake/stages/CoreLibs.cmake`
- `BuildTools/cmake/stages/Applications.cmake`
- `BuildTools/cmake/ProjectInterface.json`
- `BuildTools/PackageInterface.json`
- `Examples/MinimalMultiplayer/CMakeLists.txt`
- `Examples/MinimalMultiplayer/CMakePresets.json`
- `Examples/MinimalMultiplayer/FOnlineMinimalMultiplayer.fomain`

## Application and ownership boundary

`FO_BUILD_MAPPER=ON` creates three project-named applications:

- `<DevName>_Mapper`;
- `<DevName>_AnimationViewer`;
- `<DevName>_ParticleViewer`.

The viewer targets link `AppFrontend`, their focused viewer library,
`ClientLib`, and `BakerLib`. They do not run Mapper, a server connection,
login, maps, or `ClientEngine::MainLoop()`. Their application hosts construct
a `ClientEngine`, advance time and render managers, draw one full-viewport
ImGui tool, and save per-user viewer settings during shutdown.

The applications intentionally do not call the gameplay
`ClientStartupSettingsHook`: there is no account, network session, embedded
client index, or gameplay startup policy to customize.

For an unpackaged development build, each viewer mounts a `BakerDataSource`
from the applied project configuration. It can therefore consume the same
authored and on-demand-baked sources as the project. For a packaged process,
it mounts the configured client and Mapper resource entries.

The reusable package interface exposes `AnimationViewer` and
`ParticleViewer` only as native Windows/Linux targets. Both require `NoRes`:
the package contains the tool binary, not a second copy of game resources.
An embedding project must place the tool beside compatible configured data
sources or include it in a developer bundle that already owns those
resources.

## Build and launch

The Engine-owned MinimalMultiplayer project provides concrete target and
output names. Enable the Mapper target family while configuring.

On Windows:

```powershell
Set-Location Examples\MinimalMultiplayer
cmake --preset windows -DFO_BUILD_MAPPER=ON
cmake --build Build\windows --config RelWithDebInfo --target BakeResources FOMM_AnimationViewer FOMM_ParticleViewer
```

Launch from the generated working directory so relative data-source and
baking paths resolve against `FO_OUTPUT_PATH`:

```powershell
Set-Location Build\windows
.\Binaries\AnimationViewer-Windows-win64\FOMM_AnimationViewer.exe -ApplyConfig ..\..\FOnlineMinimalMultiplayer.fomain
```

Or launch the particle viewer:

```powershell
Set-Location Build\windows
.\Binaries\ParticleViewer-Windows-win64\FOMM_ParticleViewer.exe -ApplyConfig ..\..\FOnlineMinimalMultiplayer.fomain
```

On Linux:

```bash
cd Examples/MinimalMultiplayer
cmake --preset linux -DFO_BUILD_MAPPER=ON
cmake --build Build/linux --config RelWithDebInfo --target BakeResources FOMM_AnimationViewer FOMM_ParticleViewer
cd Build/linux
./Binaries/AnimationViewer-Linux-x64/FOMM_AnimationViewer -ApplyConfig ../../FOnlineMinimalMultiplayer.fomain
```

Use `Binaries/ParticleViewer-Linux-x64/FOMM_ParticleViewer` for the other
viewer. In an embedding project, replace `FOMM` with `FO_DEV_NAME` and use
that project's preset, output directory, main config, enabled content
features, and platform architecture.

MinimalMultiplayer deliberately contains no project-owned image, model, or
particle assets, and both particle backends are disabled there. It proves the
target, application startup, config, renderer, and asset-free empty-state
paths. A meaningful visual review requires a content-bearing project:

- AnimationViewer needs at least one `ProtoCritter` with a loadable
  `ModelName`; 3D review also requires `FO_ENABLE_3D`.
- ParticleViewer needs `FO_SPARK_PARTICLES` and/or
  `FO_EFFEKSEER_PARTICLES`, an authored source included in a particle-baked
  resource pack, and the resulting `.spk` or `.efk`.

## Animation review workflow

AnimationViewer lays out a critter filter/list, a fixed 512 by 512 preview,
an animation list, and the 3D model hierarchy.

1. Bake current prototypes, images, models, and scripts before review.
2. Filter by prototype ID and select the exact `ProtoCritter`.
3. Confirm the displayed model path is the intended `ModelName` and that no
   load error appears.
4. Set `Zoom` to `1.00x` for the in-game scale check. Other zoom values are
   inspection aids.
5. Rotate through the required facings with `Angle` or held left-button
   horizontal drag. A 2D critter snaps to its directional frames; a 3D model
   turns continuously.
6. Select `Idle`, then each required state/action pair. Disable `Loop` to
   prove a one-shot finishes and returns to idle.
7. Enable `Root`, `Name level`, `Draw rect`, and `View rect` as needed to
   inspect anchors and authored bounds.
8. For a 3D model, compare atlas-backed sprite rendering with `Direct draw`,
   then inspect the hierarchy. Tick relevant bones and attachments to project
   their markers into the preview.
9. Compare the accepted asset in a representative client scene at the same
   facing, scale, layer state, and animation.

The left button rotates, the right button pans, and the wheel zooms while the
pointer is over the preview. Zoom is clamped from `0.25x` through `8.00x`.
Selecting another critter resets pan, active animation, and hierarchy markers.

### Animation discovery

For 3D models, the viewer lists the exact `(CritterStateAnim,
CritterActionAnim)` pairs reported by `ModelInformation`. For 2D critters,
there is no authored clip table, so the viewer probes the resource manager for
resolvable state/action frame sets at the current direction.

Model visual layers are project-defined. The viewer reads
`Render.ModelLayerProperties` entries in `<PropertyName>=<LayerIndex>` form
and supplies the selected prototype's property values to 3D animation
playback. Missing or disabled properties are ignored; malformed mappings are
written to the application log.

`Direct draw` affects 3D models only:

- off renders the normal cached atlas sprite and magnifies its pixels;
- on draws geometry directly into the preview and applies zoom as model
  scale.

The direct path is useful for geometry inspection, but the atlas path remains
the closer representation of ordinary cached gameplay drawing.

## Particle review workflow

ParticleViewer lists the baked extensions advertised by the active
`ParticleSpriteFactory`. With SPARK enabled this normally includes `.spk`;
with Effekseer enabled it normally includes `.efk`. Authored `.spark` and
`.efkproj` sources are not runtime viewer entries.

1. Enable the intended particle backend, bake the source and its
   dependencies, then launch the viewer against that exact output.
2. Filter by baked resource path and select the effect. A failed load remains
   visible in the window and is written to the application log.
3. Set a fixed `Seed`, disable `Loop`, and press `Replay` repeatedly. The same
   build, backend, asset, seed, direction, and update sequence should produce
   the same review case.
4. Compare `Prewarm` off and on. Off exposes startup emission; on advances the
   system before the first displayed frame.
5. Rotate with `Direction` or held left-button drag. This reproduces the
   facing that a critter attachment can pass to the effect.
6. Pan with held right-button drag and zoom with the wheel. Panning rebases
   world-space particles so the review behaves like a moving emitter rather
   than sliding an unchanged bitmap.
7. Enable `Root`, `Draw rect`, and `Show wireframe` to inspect the ground
   anchor, atlas frame, and particle quads.
8. Compare the authored `Draw in scene` route with the alternate route. This
   checkbox changes only the live preview object; it does not edit or save the
   source.
9. Validate the accepted effect in Mapper for placement/depth, then in a
   representative client scene for gameplay timing, attachment lifetime,
   visibility, renderer behavior, and performance.

`New seed` increments the current seed and immediately restarts the effect.
`Replay` restarts with the current seed. `Loop` restarts a finite effect after
it stops. Zoom is clamped from `0.25x` through `8.00x`.

### Atlas and direct-scene interpretation

An atlas effect rasterizes into its authored sprite frame at scale `1.0`; the
viewer magnifies that frame. A direct-scene effect draws real particle
geometry into the preview and applies viewer zoom to the particle system.
Direct drawing can reveal quads or trails outside the atlas frame, while the
atlas route exposes clipping and actual cached-pixel behavior.

Do not interpret the viewer's isolated transparent preview as proof of
distortion, blending, depth, fog, lighting, or overlap against a real map.
Those need the Mapper and client scene checks.

## Persisted settings

The viewers persist their layout and review state on clean shutdown:

| AnimationViewer | ParticleViewer |
|---|---|
| ImGui layout | ImGui layout |
| zoom and direction | zoom and direction |
| direct-draw and loop toggles | seed, loop, and prewarm |
| root/name/render/view overlays | root/draw-rect/wireframe overlays |
| last valid prototype | last valid resource path |

Filter text, pan, active clip, enabled hierarchy markers, and a transient
particle `Draw in scene` override are not persisted.

`SettingsStorage` scopes records under `AnimationViewer` or
`ParticleViewer`. Windows uses
`HKCU\Software\FOnline\<application-name>`. Other platforms use the Engine
per-user data store. Persistence is best-effort: backend failures are logged
and must not terminate a tool.

If content changed since the last run, a removed prototype/resource is not
reselected. To diagnose unexpected state, first reproduce with the persisted
selection and toggles recorded; clear only the owning viewer's store when a
clean-state comparison is required.

## Failure diagnosis

| Symptom | Check |
|---|---|
| Viewer target does not exist | Reconfigure with `FO_BUILD_MAPPER=ON`; the viewers are part of the Mapper target family. |
| Application cannot find the baker or resources | Launch from the configured `FO_OUTPUT_PATH` and apply the correct project config. |
| Critter list is empty | Confirm prototype packs are mounted and baked for the client/Mapper side. |
| Prototype declares no model | Set a valid `ProtoCritter.ModelName`; fallback gameplay stubs do not create an authored animation contract. |
| Model cannot load | Check the resource path, image/model baking output, enabled 3D support, and application log. |
| No animation entries | For 3D, inspect the `.fo3d` animation table; for 2D, verify directional state/action frame resources resolve. |
| Expected model clothes/layers are absent | Check `Render.ModelLayerProperties`, layer indices, and selected prototype values. |
| Particle list is empty | Enable at least one particle backend, include a particle baker, and confirm `.spk`/`.efk` exists in the mounted output. |
| Particle load fails | Inspect the visible error and log, then check backend capability, dependencies, atlas bounds, and baked format/version. |
| Seed comparison differs | Pin build/content/backend, fixed timestep/workload, direction, prewarm, scale, and update count before comparing. |
| Viewer looks correct but game does not | Reproduce in Mapper/client; focused viewers omit map depth, gameplay ownership, network state, attachment lifecycle, and representative load. |

## Review evidence and screenshots

A retained review record should identify:

- exact Engine and project revisions plus dirty state;
- target, configuration, compiler, platform, architecture, renderer, driver,
  window size, DPI scale, and frame cap;
- selected prototype or baked particle path and content revision;
- all non-default viewer controls, animation pair or seed, and backend;
- source asset, bake log, application log, screenshot, and the later
  Mapper/client acceptance result.

Capture the whole viewer window after resources are loaded and a stable review
state is visible. Do not crop away the selected ID/path, controls, overlays, or
diagnostic error. Public screenshots need licensed content, descriptive alt
text, exact build/tag provenance, and a recapture trigger when the UI,
renderer, selected fixture, or acceptance policy changes.

The viewers currently expose no script, command-line selection, screenshot,
or headless inspection API. Do not describe a manual screenshot as automated
regression evidence. Reusable automation requires a deliberately added stable
selection/control/capture interface plus tests and documentation.

## Maintenance

Update this guide in the same change when:

- the viewer target names, `FO_BUILD_MAPPER` relationship, output paths,
  application links, or package roles change;
- viewer data-source mounting, startup hooks, frame lifecycle, or shutdown
  persistence changes;
- AnimationViewer discovery, layer mapping, direct drawing, controls,
  hierarchy, attachments, overlays, or input changes;
- ParticleViewer extension discovery, seed/prewarm/loop behavior, direction,
  panning, direct-scene drawing, wireframe, overlays, or input changes;
- `SettingsStorage` backend, scope, keys, or failure behavior changes;
- Mapper embeds, renames, or removes either viewer;
- a stable automation or screenshot-capture interface is added.

Run the focused documentation test, build both viewer targets, launch each
against a current project, and perform the smallest affected visible workflow.
Particle backend changes also require [Particle Format](../content/particle-format.md)
tests and a representative client scene. Model/animation changes also require
[Model Format](../content/model-format.md), [Model Animation](../content/model-animation.md), and the
owning model tests.

## See also

- [Tools](../../../Tools.md) for the complete tool-layer map.
- [Mapper Tools](mapper.md) for map editing, particle placement, source
  editing, and mapper automation.
- [Model Format](../content/model-format.md) for model composition and baking.
- [Model Animation](../content/model-animation.md) for animation tuples, aliases, speed,
  and effective duration.
- [Particle Format](../content/particle-format.md) for SPARK/Effekseer authoring, baking,
  runtime, and Mapper preview.
- [Configuration And Data Sources](../../reference/settings/configuration-and-data-sources.md) for
  project config and per-user tool settings.
- [Applications](../../reference/applications.md) for application construction and target
  ownership.
- [Generated Package Reference](../../reference/packages/index.md) for native
  viewer package roles and `NoRes`.
