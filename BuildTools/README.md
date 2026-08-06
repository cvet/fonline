---
layout: default
title: FOnline Engine Build Tools
permalink: /BuildTools/README.html
locale: en
document_id: buildtools-readme
---

# FOnline Engine build tools

## Build scripts

Builds normally start in an embedding project. Use [Build Workflow](../Docs/en/how-to/build/) for the supported workflow and this file for BuildTools-specific commands and environment inputs.

The exact main `buildtools.py` command surface is generated from its executable parser. Browse the [BuildTools CLI reference](../Docs/en/reference/buildtools/index.md) or the [canonical JSON model](../Docs/generated/cli.json) instead of maintaining a separate command inventory.

Engine-owned helper scripts use a separate manifest-backed surface. Browse the [helper CLI reference](../Docs/en/reference/helper-cli/index.md) or [canonical JSON model](../Docs/generated/helper-cli.json) for codegen, Mono compilation, coverage, gameplay-test orchestration, Windows 7 import validation, Android-device, local-web-server, and MSI commands. The executable parsers own syntax; `HelperCliInterface.json` owns purpose, audience, invocation owner, and the explicit boundary from the main CLI and packager.

`gameplay_test_runner.py` executes checked multi-process smoke manifests with readiness, required/forbidden markers, one scenario deadline, cleanup, and a compact JSON report. Its reusable testing contract and real headless server/client proof are documented in [Gameplay and Integration Testing](../Docs/en/how-to/testing/gameplay-and-integration.md).

`ai_control_client.py` is the standard-library reference client for the experimental project-neutral AiControl NDJSON/TCP envelope. It reads shared tokens from an environment variable, refuses remote endpoints without explicit opt-in, and is exercised by [AiControl Protocol](../Docs/en/how-to/ai-control-protocol.md) plus the runnable [protocol sample](../Examples/AiControlSample/README.md).

Package declarations and payload capabilities are versioned in `PackageInterface.json`, consumed by `package.py`, and rendered in the [package interface reference](../Docs/en/reference/packages/index.md). An embedding project's concrete `DefinePackage(...)` matrix remains project-owned.

Native-extension roles and hooks are versioned in `NativeExtensionInterface.json`, consumed by CMake/codegen, and rendered in the [native-extension reference](../Docs/en/reference/native-extension/index.md). Authoring and compatibility guidance lives in [Native Extensions](../Docs/en/how-to/native-extensions.md).

Prototype grammar and validation rules are versioned in `PrototypeFormatInterface.json`; `docs_prototype_format.py` validates live parser/baker anchors, derives the built-in metadata property catalog, and renders the [prototype-format reference](../Docs/en/reference/prototype-format/index.md). Authoring and migration guidance lives in [Prototype Format](../Docs/en/how-to/content/prototype-format.md).

Map grammar, placement ownership, mapper normalization, and side-specific bake rules are versioned in `MapFormatInterface.json`; `docs_map_format.py` validates live loader/baker/mapper anchors, derives the built-in Map/Critter/Item property catalog, and renders the [map-format reference](../Docs/en/reference/map-format/index.md). Authoring guidance lives in [Map Format](../Docs/en/how-to/content/map-format.md).

Model-description grammar, mesh inputs, composition rules, and compile-time limits are versioned in `ModelFormatInterface.json`; `docs_model_format.py` compares all accepted spellings with `ModelDescriptionParser::ParseToken` and renders the [model-format reference](../Docs/en/reference/model-format/index.md). Authoring guidance lives in [Model Format](../Docs/en/how-to/content/model-format.md).

Text-pack syntax, language normalization, prototype `$Text`, runtime lookup, and renderer color tags are versioned in `TextFormatInterface.json`; `docs_text_format.py` validates live source anchors, derives language defaults and generated prototype pack names, and renders the [text-format reference](../Docs/en/reference/text-format/index.md). Authoring guidance lives in [Text and Localization](../Docs/en/how-to/content/text-and-localization.md).

Effect sections, pass/render state, built-in shader resources, backend outputs, runtime caching, and script control are versioned in `EffectFormatInterface.json`; `docs_effect_format.py` validates live baker/renderer/runtime anchors, derives compile-limit defaults, and renders the [effect-format reference](../Docs/en/reference/effect-format/index.md). Authoring guidance lives in [Effect Format](../Docs/en/how-to/content/effect-format.md).

Image source formats, FOFRM composition, legacy filename selectors, baked sprite records, stock runtime factories, atlases, caches, and validation are versioned in `ImageFormatInterface.json`; `docs_image_format.py` validates live baker/client/test anchors, derives both extension registries, and renders the [image-format reference](../Docs/en/reference/image-format/index.md). Authoring guidance lives in [Image And Sprite Formats](../Docs/en/how-to/content/image-format.md).

Particle XML, registered SPARK objects, `SparkQuadRenderer`, editor behavior, raw-copy delivery, runtime caches/render paths, and integrations are versioned in `ParticleFormatInterface.json`; `docs_particle_format.py` derives the live registry, descriptors, editor coverage, extensions, and settings and renders the [particle-format reference](../Docs/en/reference/particle-format/index.md). Authoring guidance lives in [Particle Format And Runtime](../Docs/en/how-to/content/particle-format.md).

FOFNT/BMFont descriptors, font-slot binding, bind-time scaling, text layout, rendering flags, inline colors, and validation are versioned in `FontFormatInterface.json`; `docs_font_format.py` validates parser, resource, enum, atlas, cache, and bundled-descriptor anchors and renders the [font-format reference](../Docs/en/reference/font-format/index.md). Authoring guidance lives in [Font Format And Text Layout](../Docs/en/how-to/content/font-format.md).

WAV/ACM/Ogg delivery, decoder limits, effect identities and numbered variants, exact-path music, repeat timing, frontend mixing, and silent/headless behavior are versioned in `AudioInterface.json`; `docs_audio.py` derives live resource, decoder, frontend, setting, and test evidence and renders the [audio reference](../Docs/en/reference/audio/index.md). Authoring guidance lives in [Audio Resources and Playback](../Docs/en/how-to/content/audio.md).

Ogg/Theora delivery, whole-resource decoding, fullscreen queue/input/music
behavior, embedded playback, rendering, and validation gaps are versioned in
`VideoInterface.json`; `docs_video.py` derives live decoder, client, script,
rendering, raw-copy, dependency, and test evidence and renders the
[video reference](../Docs/en/reference/video/index.md). Integration guidance lives in
[Video Resources and Playback](../Docs/en/how-to/content/video.md).

The reusable AngelScript GUI types, documented members/callbacks, screen API,
annotations, lifecycle, layout, drawing, input, and embedding hooks are
versioned in `GuiRuntimeInterface.json`; `docs_gui_runtime.py` derives the live
CoreScripts contract and renders the
[GUI runtime reference](../Docs/en/reference/gui-runtime/index.md). Integration guidance lives
in [GUI Runtime](../Docs/en/how-to/runtime/gui.md). Declarative GUI formats and generators
remain outside this Engine contract.

Public example ownership, ordering, exact Engine pins, compatibility lanes, governance files, source-staging exclusions, and release gates are versioned in `Examples/PublicRepositories.json`; `docs_examples.py` validates the shared overlay, materializes a clean review candidate only when the exact Engine checkout is clean and remote-reachable, emits the [public-example registry](../Docs/en/reference/public-examples/index.md), and verifies candidate external repositories before publication.

Validation scenarios can be run one at a time or batched in a single command:

```bash
Engine/BuildTools/validate.sh unit-tests
Engine/BuildTools/validate.sh android-arm64-client linux-client linux-server
```

BuildTools Python regression tests live under `Engine/BuildTools/tests/` and can be run directly:

```bash
pytest -q Engine/BuildTools/tests
```

## CMake layout

All internal CMake modules now live under `Engine/BuildTools/cmake`.
The public entry point kept at the `Engine/BuildTools` root is `Init.cmake`; staged CMake implementation lives under `Engine/BuildTools/cmake/stages/` and helpers under `Engine/BuildTools/cmake/helpers/`.
`cmake/ProjectInterface.json` is the versioned source of truth read by configure for project options, stage order, entrypoints, hooks, and the selected helper surface. Browse the generated [CMake project-interface reference](../Docs/en/reference/cmake/index.md) or its [canonical JSON model](../Docs/generated/cmake.json) instead of copying declarations from stage implementation files. For project-owned targets and SDKs, use [Project-Local Dependencies](../Docs/en/how-to/native-extensions/project-dependencies.md) and `AddProjectLibraries` instead of mutating internal role lists.
The executable validation/starter project lives under `Engine/Examples/MinimalProject`. BuildTools copies it into `Workspace/validation-project` and links its `Engine/` child back to the current checkout before configuring validation targets.

## Documentation generators

- `docs_api.py` runs the existing `codegen.py` metadata parser in read-only mode, applies source-owned `///@ ApiContract` classifications, and writes/checks `Docs/generated/api.json`.
- `docs_api_diff.py` provides the native-symbol comparison layer, including overload families and source-owned stability.
- `docs_contract_diff.py` compares the native API, CMake, main CLI, package, helper CLI, native-extension, prototype-format, map-format, model-format, text-format, effect-format, image-format, particle-format, font-format, audio, video, GUI runtime, and AiControl protocol models in one revision-pair report and enforces exact shared-ledger dispositions for baseline-public or model-contract breaks.
- `docs_public_api.py` writes/checks the canonical EN/RU contract indexes plus the root `PUBLIC_API.md` legacy route from all eighteen machine models and their generated human references; native inventory counts and default stability are never maintained by hand.
- `docs_external_evidence.py` validates the pinned Last Frontier/TLA discovery inventory, owner review policy, dispositions, priorities, Engine/planned targets, and deterministic internal report. With both exact checkouts it also proves every recorded path exists in the pinned commit through `git cat-file`.
- `docs_cmake.py` validates `cmake/ProjectInterface.json` and writes/checks `Docs/generated/cmake.json`, the English Markdown reference under `Docs/en/reference/cmake/`, and durable route pointers under `Docs/generated/cmake/`.
- `docs_cli.py` loads the executable `buildtools.py` `argparse` parser and writes/checks `Docs/generated/cli.json`, exact help-backed English Markdown under `Docs/en/reference/buildtools/`, and durable route pointers under `Docs/generated/cli/`.
- `docs_helper_cli.py` validates `HelperCliInterface.json`, proves complete `create_parser()` inventory coverage, and writes/checks `Docs/generated/helper-cli.json`, exact help-backed English Markdown under `Docs/en/reference/helper-cli/`, and durable route pointers under `Docs/generated/helper-cli/`.
- `docs_native_extension.py` validates the runtime-consumed native role/hook manifest and writes/checks `Docs/generated/native-extension.json` plus role, hook, and binding pages.
- `docs_prototype_format.py` validates `PrototypeFormatInterface.json` against live parser/baker sources, derives built-in entity/property applicability from metadata, and writes/checks `Docs/generated/prototype-format.json` plus syntax, property, and validation pages.
- `docs_map_format.py` validates `MapFormatInterface.json` against live loader/baker/mapper/runtime sources, derives Map/Critter/Item property and ItemOwnership data from metadata, and writes/checks `Docs/generated/map-format.json` plus syntax, property, baking, and validation pages.
- `docs_model_format.py` validates `ModelFormatInterface.json` against the live model parser, mesh baker, client runtime, limits, and tests, then writes/checks `Docs/generated/model-format.json` plus syntax, token, composition, asset, animation, and validation pages.
- `docs_text_format.py` validates `TextFormatInterface.json` against text-pack, baker, runtime, script API, settings, renderer, and test sources, then writes/checks `Docs/generated/text-format.json` plus syntax, language, prototype-text, runtime, and validation pages.
- `docs_effect_format.py` validates `EffectFormatInterface.json` against the effect baker, render-effect/runtime/cache/script API, backend conventions, project limits, and tests, then writes/checks `Docs/generated/effect-format.json` plus syntax, render-state, resource, baking, runtime, and validation pages.
- `docs_image_format.py` validates `ImageFormatInterface.json` against ImageBaker, FOFRM/import sources, stock client factory/sheet/atlas/cache behavior, and tests, then writes/checks `Docs/generated/image-format.json`, canonical English pages under `Docs/en/reference/image-format/`, and compatibility routes under `Docs/generated/image-format/`.
- `docs_particle_format.py` validates `ParticleFormatInterface.json` against raw-copy settings, SPARK XML/registry/descriptors, the Engine renderer, ParticleEditor, client runtime, script/model integrations, and tests, then writes/checks `Docs/generated/particle-format.json` plus XML, object, renderer, tooling, runtime, integration, and validation pages.
- `docs_audio.py` validates `AudioInterface.json` against raw-copy settings, resource indexing, WAV/ACM/Ogg decoding, script playback, frontend conversion/mixing, headless behavior, and native-test inventory, then writes/checks `Docs/generated/audio.json` plus format, delivery, decoding, playback, and validation pages.
- `docs_video.py` validates `VideoInterface.json` against raw-copy settings, Ogg/Theora decoding, fullscreen queue/input/music/drawing, embedded script playback, renderer behavior, dependencies, and native-test inventory, then writes/checks `Docs/generated/video.json` plus format, delivery, decoding, fullscreen, embedded, and validation pages.
- `docs_gui_runtime.py` validates `GuiRuntimeInterface.json` against `Gui.fos`, `Input.fos`, native client dispatch, tutorial boundaries, and test inventory, then writes/checks `Docs/generated/gui-runtime.json` plus type, screen API, lifecycle, layout/rendering, input, and integration/validation pages.
- `docs_ai_control_protocol.py` validates `AiControlProtocol.json` against the reference client and runnable sample, then writes/checks `Docs/generated/ai-control-protocol.json` plus wire, method, command/event, security, and integration/validation pages.
- `docs_package.py` validates the runtime-consumed package manifest and executable `package.py` parser, then writes/checks `Docs/generated/package.json` plus package reference pages.
- `docs_examples.py` validates `Examples/PublicRepositories.json` and the governance overlay, writes/checks `Docs/generated/public-examples.json` plus its registry page, materializes a source-ready example into a new clean candidate directory, and verifies external repository metadata, exact gitlink pins, required files, and provenance file bytes.
- `docs_reference.py` renders the canonical API model into GitHub Pages-compatible Markdown under `Docs/generated/api/`.
- `docs_metadata.py` strictly decodes project-baked `Metadata.fometa-server/client`, verifies both sides agree, and writes/checks a project-owned remote-call JSON/Markdown catalog.
- `docs_inventory.py` writes/checks the independent export-method, native-test, and setting declaration inventory.
- `docs_localization.py` enforces complete bilingual coverage, the glossary, stable locale targets, normalized English hashes, exact translated fences, and language-preserving links, then writes/checks `Docs/generated/translation-status.json`.
- `docs_description_translations.py` inventories reader-facing prose in 20 generated contract models, applies the reviewed stable-ID Russian overlay, rejects duplicate/unknown/stale/type-changing/code-changing records, and writes/checks `Docs/generated/description-translation-status.json`. Missing entries remain explicit until the semantic catalog can move from `registered-translations-current` to `complete`.
- `docs_ai_delivery.py` projects `Docs/documentation-manifest.json` and canonical Markdown into root `llms.txt`, bounded `llms-full.txt`, and public `docs-manifest.json`; it normalizes content hashes and rejects stale, oversized, or non-deterministic output.
- `docs_site.py` resolves manifest-owned stable document IDs into checked localized Jekyll navigation data, bounded English and Russian static search indexes, and the public version/locale/legacy-route catalog; it rejects unknown/duplicate/omitted top-level pages, route collisions, ambiguous canonical targets, missing locale pairs, cross-locale search ownership, and oversized or stale output.
- `docs_ai_eval.py` validates the versioned standalone task set in `Docs/ai-evaluation.json` against the manifest and the same compact search model used by the browser, then writes/checks `Docs/generated/ai-evaluation-report.json` with ranks, evidence checks, success rate, and MRR.
- `docs_ai_model_eval.py` optionally runs isolated tasks through a local Ollama model, retaining exact input/prompt/model hashes, streamed raw attempts, evidence observations, and resumable task state under ignored `Workspace/ai-evaluation/`. It repeats bounded verbatim decision and query-relevant sections from retrieved candidate documents near the answer instruction while keeping the answer rubric hidden; it is a reviewed local evidence tool, not a CI dependency.
- `docs_ai_model_review.py` creates, finalizes, and validates compact semantic reviews for raw model-family runs; `--require-run` additionally proves the retained raw SHA-256 and embedded run metadata.
- `docs_snippets.py` inventories every fence in public/current/human documents, applies the language/harness contract from `SnippetPolicy.json`, writes/checks `Docs/generated/snippets.json`, and optionally requires the real Bash and PowerShell parsers without executing commands.
- `docs_diagrams.py` validates `DocumentationDiagrams.json`, owning-document alt/caption markup, source provenance, canvas bounds, node overlap, and local-only SVG safety; it writes/checks desktop/mobile SVG variants for three teaching diagrams plus `Docs/generated/diagrams.json` with exact hashes.
- `docs_screenshots.py` validates `DocumentationScreenshots.json`, exact PNG
  bytes/dimensions, owning-document alt/caption markup, capture environment,
  source provenance, and recapture triggers; it writes/checks
  `Docs/generated/screenshots.json` with exact image, manifest, and source
  hashes.
- `docs_site_artifact.py` validates the completed Jekyll `_site` tree against the route catalog and source artifacts. It rejects missing current/available-locale routes, altered or absent static endpoints, wrong canonical URLs/languages, missing accessibility landmarks/names, duplicate IDs, and broken links to publishable local resources; CI retains its JSON report.
- `docs-browser/audit.mjs` serves that completed `_site` tree locally and uses the lock-file-pinned Playwright Chromium plus axe-core to audit every route at desktop and mobile widths. It rejects WCAG 2.2 A/AA violations, runtime/resource errors, page-level horizontal scrolling, broken responsive layout, and keyboard failures in skip navigation, search, theme, copy, and the mobile focus-trapped drawer; CI retains JSON and screenshots.
- `docs_validate.py` validates the documentation manifest, local links/anchors, source ownership, Pages contract, and freshness of every generated artifact.

Run their focused tests and checks from the engine root; generated JSON and Markdown are checked in and must not be edited manually.

Materialize a review candidate only from a clean, remotely fetchable exact
Engine commit:

```bash
python BuildTools/docs_examples.py --stage-repository minimal-multiplayer --engine-revision "$(git rev-parse HEAD)" --output Workspace/fonline-minimal-multiplayer
```

```bash
python -m unittest discover -s BuildTools/tests -p "test_docs*.py"
cmake -P BuildTools/tests/validate_project_interface.cmake
cmake -P BuildTools/tests/validate_native_extension_interface.cmake
cmake -P BuildTools/tests/validate_package_interface.cmake
python BuildTools/docs_cmake.py --check
python BuildTools/docs_cli.py --check
python BuildTools/docs_helper_cli.py --check
python BuildTools/docs_native_extension.py --check
python BuildTools/docs_prototype_format.py --check
python BuildTools/docs_map_format.py --check
python BuildTools/docs_model_format.py --check
python BuildTools/docs_text_format.py --check
python BuildTools/docs_effect_format.py --check
python BuildTools/docs_image_format.py --check
python BuildTools/docs_particle_format.py --check
python BuildTools/docs_font_format.py --check
python BuildTools/docs_audio.py --check
python BuildTools/docs_video.py --check
python BuildTools/docs_gui_runtime.py --check
python BuildTools/docs_ai_control_protocol.py --check
python BuildTools/docs_package.py --check
python BuildTools/docs_examples.py --check
python BuildTools/docs_public_api.py --check
python BuildTools/docs_external_evidence.py --check
python BuildTools/docs_inventory.py --check
python BuildTools/docs_localization.py --check --enforce-complete
python BuildTools/docs_description_translations.py --check
python BuildTools/docs_diagrams.py --check
python BuildTools/docs_screenshots.py --check
python BuildTools/docs_snippets.py --check --external
python BuildTools/docs_site.py --check
python BuildTools/docs_ai_eval.py --check
python BuildTools/docs_site_artifact.py --site-dir _site
npm ci --prefix BuildTools/docs-browser
npx --prefix BuildTools/docs-browser playwright install chromium
npm --prefix BuildTools/docs-browser run audit
python BuildTools/docs_ai_delivery.py --check
python BuildTools/docs_validate.py
```

### Effekseer project compiler

`Source/Tools/EffekseerCompiler.h/.cpp` is a native C++ module compiled into
`BakerLib`. For each fixed Editor-1.80.5 / project-version-3 `.efkproj`, it
validates the XML profile and returns raw `SKFE` bytes plus the referenced
textures, models, sounds, and curves. `ParticleBaker` calls the module directly
and validates every result with the vendored Effekseer Core before publishing.

`ParticleBaker` resolves dependency paths inside the project's physical
directory resource source and stores a per-effect path/size/write-time snapshot
below `<BakeOutput>/.baker-cache/Effekseer/`. The source project and dependency
snapshot independently invalidate the derived `.efk`; after compiler code
changes, use `ForceBakeResources`. A changed effect recompiles on demand without
invalidating unrelated effects in Mapper's focus-triggered resource reindex.

The compiler is not linked into or packaged with runtime clients. Native Baker
and Mapper hosts use it when derived resources are stale; Web clients consume
host-prebaked `.efk` resources.

### Effekseer Editor developer bundle

The pinned upstream Effekseer Editor is built as a standalone Windows win64
developer tool. It is independent of `FO_EFFEKSEER_PARTICLES` and is not
represented by an engine CMake option, application target, or universal
`buildtools.py build` target. Runtime builds therefore never acquire the
Editor toolchain or its Viewer/UI libraries.

Build and stage it through the shared auxiliary-tool entry point:

```powershell
$env:FO_OUTPUT = (Get-Location).Path
python Engine\BuildTools\buildtools.py build-auxiliary effekseer-editor Release
```

The script builds the managed .NET 10 UI and native Viewer/material tools in
isolated output directories, then stages a self-contained payload. Languages,
fonts, icons, meshes, `LICENSE_TOOL`, the material editor, and Direct3D
11/OpenGL material compilers are part of that payload. GIF recording is
disabled in this FOnline bundle, avoiding the otherwise unused libgd
dependency; ordinary editing and the interactive Editor preview remain
available.

The FOnline adaptation is source-first. Editor **Save** and **Save As** accept
only `.efkproj` and atomically write normalized UTF-8 XML without a BOM. The
Editor's stock preview is for authoring iteration; the embedding project's
Mapper preview remains the final validation path through FOnline's renderer
and capability gate.

The reusable package schema has no Effekseer-specific binary role. An embedding
project ships a separately staged tool through the generic package declaration
`INCLUDE <source-path-glob> <target-path-in-pack>`. Source globs are relative to
`FO_OUTPUT_PATH`; the packager replaces the owned target tree and updates an
existing `SingleZip` without duplicate or stale entries.

## Build environment variables

Build scripts (sh/bat) can be called both from current directory (e.g. `./linux.sh`) or repository root (e.g. `BuildTools/linux.sh`).  
Following environment variables may be set before starting build scripts:

BuildTools consumes only project-specific overrides with the `FO_` prefix. Any declared project-interface option can be overridden from an environment variable with the same name; the exact list and precedence are in the [generated options reference](../Docs/en/reference/cmake/options.md). Standard tool variables such as `ANDROID_HOME`, `ANDROID_SDK_ROOT`, and `ANDROID_NDK_ROOT` are reserved for spawned external tools and are not read as BuildTools inputs.

#### FO_ENGINE_ROOT

Path to root directory of FOnline repository.  
If not specified, the path is taken one level above the running script file (outside `BuildTools`, at the repository root).

*Default: `$(dirname ./script.sh)/../`*  
*Example: `export FO_ENGINE_ROOT=/mnt/d/fonline`*

#### FO_WORKSPACE

Path to directory where all intermediate build files will be stored.  
Default behaviour is build in current directory plus `Workspace`.  

*Default: `$PWD/Workspace`*  
*Example: `export FO_WORKSPACE=/mnt/d/fonline-workspace`*

#### FO_ANDROID_HOME / FO_ANDROID_SDK_ROOT / FO_ANDROID_NDK_ROOT

Optional explicit Android SDK and NDK overrides for BuildTools. If not set, BuildTools uses prepared workspace locations and then the system `/usr/lib/android-sdk` fallback on Linux when available.

#### FO_CLANG_FORMAT

Optional explicit path to the `clang-format` binary used by `buildtools.py format-source`. The binary must still satisfy the version-20 gate. When unset, BuildTools searches the system `PATH` for `clang-format-20` then `clang-format`. An embedding project can use this to point the Engine formatter at its own pinned or bundled binary.

## Shared workspace preparation

Shared workspace parts are prepared through `buildtools.py` and wrapped by the platform-specific scripts.

- Linux: `Engine/BuildTools/prepare-workspace.sh`
- macOS: `Engine/BuildTools/prepare-mac-workspace.sh`
- Windows: `Engine/BuildTools/prepare-win-workspace.ps1`

At the moment the shared flow covers:

- `toolset`
- `emscripten`
- `android-ndk`
- `dotnet`
- `xwin`
- `msan-libcxx`

Linux system package installation is explicit and separate from workspace preparation:

- `common-packages`
- `linux-packages`
- `web-packages`
- `android-packages`
- `windows-cross-packages`
- `msi-packages`
- `all-packages`

Workspace features such as `linux`, `web`, `android-arm64`, and `windows-cross` do not install apt packages. On a fresh host, pass the matching `*-packages` feature first. `all-packages` installs every group above (including `msi-packages`, the `wixl` MSI-installer toolset). Because apt lives only on the host-provisioning path, no `prepare-workspace` part installs system packages, and parallel CI jobs never contend for the apt lock.

Host prerequisite checks are also available through the main tool:

- `buildtools.py host-check linux`
- `buildtools.py host-check macos`
- `buildtools.py host-check windows`

Host wrapper scripts now delegate to the unified workspace preparation command:

- `buildtools.py prepare-host-workspace linux ...`
- `buildtools.py prepare-host-workspace windows ...`
- `buildtools.py prepare-host-workspace macos ...`

Emscripten version is pinned by `Engine/ThirdParty/emscripten` and installed into `Workspace/emsdk`.

Examples:

```bash
python3 Engine/BuildTools/buildtools.py prepare-workspace toolset
python3 Engine/BuildTools/buildtools.py prepare-workspace emscripten
python3 Engine/BuildTools/buildtools.py prepare-workspace android-ndk dotnet
python3 Engine/BuildTools/buildtools.py prepare-workspace msan-libcxx
python3 Engine/BuildTools/buildtools.py prepare-workspace toolset emscripten android-ndk dotnet --check
python3 Engine/BuildTools/buildtools.py prepare-host-workspace linux web-packages web dotnet
```

`msan-libcxx` is Linux-only and intentionally excluded from the default `all`
workspace feature because it downloads matching LLVM sources and builds
`libc++`, `libc++abi`, and `libunwind` with MemorySanitizer instrumentation. The
runtime build also passes `BuildTools/sanitizers/msan-runtime-ignorelist.txt` so
libunwind does not self-report on ABI register snapshots during C++ exception or
sanitizer-report unwinding. The `unit-tests-san-memory` validator prepares it
automatically before configuring `San_Memory`; use the explicit workspace command
only when pre-warming a CI host or debugging the runtime build.

Linux hosts can prepare the Windows cross-compilation SDK/CRT through the same wrapper:

```bash
bash Engine/BuildTools/prepare-workspace.sh windows-cross-packages windows-cross
bash Engine/BuildTools/prepare-workspace.sh windows-cross
python3 Engine/BuildTools/buildtools.py prepare-workspace xwin
python3 Engine/BuildTools/buildtools.py build win64 client Release
python3 Engine/BuildTools/buildtools.py build win32 client Release
```

The `windows-cross-packages` feature installs/checks Linux prerequisites. The `windows-cross` wrapper feature and direct `prepare-workspace xwin` command are workspace-only: they use the xwin version pinned in `Engine/ThirdParty/xwin`, prepare both `x86` and `x86_64` SDK/CRT trees into `Workspace/xwin`, and intentionally skip system package installation for pre-provisioned CI hosts. `buildtools.py` splats the primary architecture first, then merges secondary architecture library directories from isolated splats to avoid the `xwin 0.6.6-rc.2` shared-symlink race in one multi-arch invocation. Each splat passes `--http-retry 5` so transient Microsoft CDN body-read failures are retried before failing the workspace preparation.

For `win32`, `buildtools.py` passes `CMAKE_SYSTEM_PROCESSOR=x86`; the toolchain keeps the xwin `x86` library paths and forces `clang-cl --target=i686-pc-windows-msvc` so CMake compiler probes do not emit x64 objects for an x86 link.

## Windows web debug workflow

The local Windows web debug flow uses these shared commands:

- `buildtools.py build web client RelWithDebInfo`
- `buildtools.py package-web-debug`

For an optimized browser build use:

- `buildtools.py build web client Release`

The packaged browser build is emitted into `Workspace/web-debug/<ProjectDevName>-Client-<Config>-Web` and can be served by the generated `web-server.py` helper.

## Android debug workflow

The local Android debug flow uses these shared commands:

Supported Android platform identifiers are `android-arm32`, `android-arm64`, and `android-x86`.

Device deployment is built around ADB over Wi-Fi. The target device must have wireless debugging enabled and be paired with this host if Android requests pairing.

- `buildtools.py build android-arm64 client RelWithDebInfo`
- `buildtools.py package-android-debug <ProjectDevName> android-arm64 <Config>`
- `android_device.py --workspace-root Workspace connect`

The Android SDK and NDK workspace parts must be prepared first. Use `android-packages` only on a fresh Linux host that still needs system packages:

- `bash Engine/BuildTools/prepare-workspace.sh android-arm64`
- `bash Engine/BuildTools/prepare-workspace.sh android-packages android-arm64`

The packaged Android build is emitted into `Workspace/android-debug/<ProjectDevName>-Client-<Config>-Android` as a ready-to-build Gradle project. Build and deploy:

```bash
python3 Engine/BuildTools/android_device.py --workspace-root Workspace connect
cd Workspace/android-debug/<ProjectDevName>-Client-<Config>-Android
./gradlew assembleDebug
python3 Engine/BuildTools/android_device.py --workspace-root Workspace install --apk Workspace/android-debug/<ProjectDevName>-Client-<Config>-Android/app/build/outputs/apk/debug/app-debug.apk
python3 Engine/BuildTools/android_device.py --workspace-root Workspace launch --activity com.example.game/.FOnlineActivity

# Pass the host address for a project config that expects a remote server.
python3 Engine/BuildTools/android_device.py --workspace-root Workspace launch-game --activity com.example.game/.FOnlineActivity
```

`launch-game` auto-detects the host LAN IP that reaches the selected Wi-Fi Android device and passes it as a runtime `ClientNetwork.ServerHost` override, which makes the packaged `RemoteSceneLaunch` client connect back to the host server without editing baked config files.

At runtime, `FOnlineActivity` stages `assets/Resources` into the app files directory on first launch after install or update and then starts the engine with absolute `Baking.ClientResources` and `Baking.CacheResources` overrides that point to that runtime location.

Android SDK command-line tools version is pinned by `Engine/ThirdParty/android-sdk` and installed into `Workspace/android-sdk`.

Android NDK version is pinned by `Engine/ThirdParty/android-ndk` and installed into `Workspace/android-ndk`.

The Gradle project template lives in `Engine/BuildTools/android-project/` and uses `$PLACEHOLDER$` tokens patched by `package.py` during packaging. Android configuration values come from the authored root plus the selected `SubConfig`; build-host `$ENV`/`$FILE` and `$TARGET_ENV`/`$TARGET_FILE` directives are resolved only when packaging, so sub-config overrides affect APK metadata without requiring signing values in the baked client config. Android SDKs that require application manifest metadata can use `Android.ManifestMetaData.<android:name> = <android:value>` settings; the packager emits them as `<meta-data>` entries inside `<application>`. SDK Gradle setup can use `Android.GradleMavenRepository.<name> = <url>` and `Android.GradleDependency.<name> = <Gradle dependency statement>` to add package-config-specific Maven repositories and `dependencies { ... }` entries. Package-specific Java sources can use `Android.JavaSource.<name> = <path/to/File.java>`; the packager copies each non-empty source into the generated app package namespace and patches `$PACKAGE$` / `$CONFIG$`.

Android release APK packaging signs the artifact. Configure signing through `Android.Keystore`, `Android.KeystorePassword`, `Android.KeyAlias`, and `Android.KeyPassword` in the project main config. Use `$TARGET_ENV{...}` (or a protected `$TARGET_FILE{...}` for a value, not a keystore path) for sensitive inputs so `ConfigBaker` does not materialize them. `package.py` passes `Android.KeystorePassword` and `Android.KeyPassword` to Gradle through `FO_ANDROID_RELEASE_STORE_PASSWORD` and `FO_ANDROID_RELEASE_KEY_PASSWORD` environment variables instead of writing them into the generated Gradle project. If you build the generated Gradle project manually, set those variables before `./gradlew assembleRelease`; if the signing settings are empty, packaging falls back to the Gradle debug signing key so generated package APKs remain installable on development devices. See [Security and Secrets](../Docs/en/how-to/release/security-and-secrets.md) for the complete boundary.

APK packaging runs Gradle with `GRADLE_USER_HOME` under the current workspace output tree instead of the shared `~/.gradle`, so parallel CI package jobs do not contend for global Gradle caches.

`android_device.py` first tries `adb mdns services`, shows any discovered Android Wi-Fi endpoints as a numbered list, caches the selected endpoint in `Workspace/android-debug/device-endpoint.txt`, and falls back to manual `IP[:port]` entry when discovery returns nothing.

## Packaging: post-build binary patching

Use [Packaging and Release](../Docs/en/how-to/release/packaging.md) for the complete declaration, build/bake/package, platform, signing, provenance, acceptance, and recovery workflow. This section documents BuildTools implementation details.

`package.py` injects a few values into the already-linked binaries instead of recompiling per package. Each
target is a fixed-capacity placeholder reserved at compile time with `FO_KEEP_DATA_SYMBOL` (so the linker keeps
the array), and `patch_data(file, marker, data, max_size)` overwrites it in place: it locates the marker, writes
`data` followed by `#` padding up to `max_size`, and asserts the file size is unchanged. Nothing is relocated,
compressed, encrypted, or generated - the file layout is identical before and after, only reserved data bytes
change. Patched regions, all transparent identity/config **text** (never code):

- `PACKAGED_BUILD_NAME` - marker `###NotPackaged###`, a 128-byte array. The package/build identity string;
  each runtime variant patches its own so `IsPackaged()` and the build name reflect the package.
- `INTERNAL_CONFIG` - markers `###InternalConfig###...` / `###InternalConfigEnd###`, capacity
  `FO_INTERNAL_CONFIG_CAPACITY` (40000). The baked internal config blob.
- Embedded resources - capacity `FO_EMBEDDED_DATA_CAPACITY` (200000).

`package.py` also rewrites the PE PDB path (`patch_pe_pdb_path`) and the Android Gradle `$PLACEHOLDER$` tokens.

> **Antivirus note:** "a large placeholder region overwritten after the build" superficially resembles a
> packer/dropper stub, but here it is only transparent config/identity text written into reserved data arrays -
> no code is produced, decrypted, or executed. This is documented so a release engineer or AV reviewer can
> confirm it is benign; keep the capacities no larger than needed and whitelist the packaging step. Record any
> product-specific antivirus exception in the embedding project's release documentation.

## Packaging: Windows code signing

Packaged Windows binaries can be code-signed at release time so antivirus/SmartScreen trust the client (the
self-update flow downloads and executes a runtime DLL, so an unsigned client is the main heuristic trigger).
Signing is **off by default** (current behavior: unsigned) and tool-agnostic:

- Set `Packaging.CodeSigningHook` in the project main config to an **executable script** on the packaging host.
- During `finalize_output`, before any Zip/Tar/Wix/Raw step, `package.py` calls `<hook> <absolute-pe-path>`
  once for **every `*.exe`/`*.dll`** staged under the package tree - launcher exes (incl. the OpenGL variant),
  the runtime DLLs, and the client-runtime **update payloads** (the downloaded-and-executed DLL). Signing last
  means the signature covers the final patched bytes; covering the whole tree means archives, the MSI, and the
  updater payloads are all signed.
- The hook must exit `0`; a non-zero exit **fails the package** so a release that asked to be signed never
  ships unsigned. The hook must be directly executable on the host (shebang + `chmod +x` on Linux; a
  `.cmd`/`.bat`/`.exe` on Windows).
- The hook owns the tool, certificate, timestamp URL, and **secrets** - keep passwords/tokens in environment
  variables, never in the repo or main config. Always **timestamp** (RFC-3161) so signatures outlive the cert.

For publicly trusted signatures, follow the current CA and provider key-storage requirements; do not assume a
plain on-disk `.pfx` is acceptable. The hook remains provider-neutral. Representative integration shapes are:

```bash
# 1) Azure Trusted Signing on a Windows host through signtool and the provider dlib.
#    Azure credentials come from environment variables (AZURE_* / managed identity).
#    sign_windows.cmd  (args: %1 = file)
#    signtool sign /v /fd SHA256 /tr http://timestamp.acs.microsoft.com /td SHA256 ^
#      /dlib "%TRUSTED_SIGNING_DLIB%" /dmdf "%TRUSTED_SIGNING_METADATA_JSON%" "%~1"

# 2) SSL.com eSigner CodeSignTool on a Linux packaging host (Java). Credentials/TOTP come from env.
#    sign_windows.sh  (args: $1 = file)
#    CodeSignTool sign -username="$ESIGNER_USER" -password="$ESIGNER_PASS" \
#      -totp_secret="$ESIGNER_TOTP" -input_file_path="$1" -override

# 3) osslsigncode on Linux with a PKCS#11 cloud-HSM certificate or hardware token.
#    sign_windows.sh  (args: $1 = file)
#    tmp="$(mktemp)"; osslsigncode sign -pkcs11module "$PKCS11_MODULE" -key "$PKCS11_KEY_ID" \
#      -certs "$CERT_PEM" -h sha256 -n "Example Game" -i https://example.com \
#      -ts http://timestamp.digicert.com -in "$1" -out "$tmp" && mv -f "$tmp" "$1"
```

For Android, signing stays in Gradle (see the Android workflow above); this hook is Windows-only.

## Source formatting

`buildtools.py format-source` formats the Engine `Source/` tree, including `.fos`, with clang-format. The binary is resolved by `discover_clang_format()`: the `FO_CLANG_FORMAT` override first (when set), then `clang-format-20`/`clang-format` on `PATH`; the resolved binary must report major version 20. BuildTools then repairs AngelScript nullable and named-argument forms that clang-format parses as C++. The complete contract and embedding-project boundary are in [AngelScript Style and Refactoring](../Docs/en/how-to/scripting/style-and-refactoring.md).

## Pipeline documentation

For the maintained staged CMake pipeline guide, see [BuildTools Pipeline](../Docs/en/reference/cmake-and-buildtools/pipeline.md).
