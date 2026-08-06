---
title: Generated Support Matrix
document_id: generated-support-matrix-index
locale: en
generated: true
generated_by: BuildTools/docs_support_matrix.py
---

# Generated Support Matrix

This page is generated from the checked support policy and the live BuildTools/CI target registry.
A build gate proves configuration and compilation; only a smoke gate proves the named process route.

## Evidence levels

| Level | Meaning |
|---|---|
| `build_gated` | Configured and compiled by the required validation workflow on every change. |
| `smoke_gated` | Build-gated and exercised by an automated process-level starter, native-extension, multiplayer, or content-showcase smoke test. |
| `source_capable` | The checked BuildTools registry exposes a target, but the required workflow does not exercise it. |
| `not_in_public_matrix` | No supported validation target is published for this application/platform combination. |

## Platform profiles

| Host / target | Compiler | Level | Applications | Required validation targets | Runtime evidence | Limitations |
|---|---|---|---|---|---|---|
| Windows / Windows x64 | MSVC 19.44 or newer | `smoke_gated` | desktop client and headless client; server, headless server, and Windows service; mapper, animation viewer, and particle viewer; AngelScript compiler; resource baker | `win64-client`, `win64-server`, `win64-mapper`, `win64-ascompiler`, `win64-baker`, `win64-starter-smoke`, `win64-native-extension-smoke`, `win64-tutorial-smoke`, `win64-showcase-smoke`, `win64-tutorial-package`, `win64-package-smoke` | The starter, native-extension, minimal-multiplayer, and content-showcase process routes run through BuildTools validation; packaging-matrix and tutorial-package routes build archives, inventory payloads, and start packaged headless client/server binaries with embedded config. | A green smoke route proves the stock tutorial and unsigned package-fixture paths, not every renderer, driver, database, installer, signing, store, deployment, or rollback combination. |
| Windows / Windows x86 | MSVC 19.44 or newer | `build_gated` | desktop client | `win32-client` | No process-level smoke route is required. | Server and tool applications are outside the published x86 validation surface. |
| Ubuntu 24.04 / Linux x64 | Clang 20 or newer | `smoke_gated` | desktop client and headless client; server, headless server, and daemon; mapper, animation viewer, and particle viewer; AngelScript compiler; resource baker | `linux-client`, `linux-server`, `linux-mapper`, `linux-ascompiler`, `linux-baker`, `linux-starter-smoke`, `linux-native-extension-smoke`, `linux-tutorial-smoke`, `linux-showcase-smoke`, `linux-showcase-capture`, `linux-tutorial-package`, `linux-package-smoke` | The required workflow owns starter, native-extension, minimal-multiplayer, and content-showcase source process routes, a software-Mesa OpenGL Content Showcase capture with pixel validation, plus packaged tutorial and packaging-matrix routes that build tarballs, inventory payloads, and start packaged headless client/server binaries with embedded config. | The current matrix qualifies Ubuntu 24.04 and unsigned fixture package paths only after retained required-job evidence; other distributions, installers, signing, stores, deployment, and rollback require project acceptance. |
| Ubuntu 24.04 / Linux x64 | GCC 13 or newer | `build_gated` | desktop client and headless client; server, headless server, and daemon; mapper, animation viewer, and particle viewer; AngelScript compiler; resource baker | `linux-gcc-client`, `linux-gcc-server`, `linux-gcc-mapper`, `linux-gcc-ascompiler`, `linux-gcc-baker` | No GCC-specific process smoke route is required. | Runtime qualification comes from the Clang lane; a project relying on GCC-specific behavior needs its own smoke lane. |
| macOS 26 Intel and Apple Silicon runners / macOS x64 and arm64 | AppleClang | `build_gated` | desktop client | `mac-client` | No process-level client smoke route is required. | The public matrix does not qualify macOS server, mapper, tools, packaging, signing, or notarization. |
| macOS 26 Intel and Apple Silicon runners / iOS client | AppleClang | `build_gated` | client library/application bundle inputs | `ios-client` | No simulator or device smoke route is required. | Provisioning, signing, App Store delivery, device input, audio, networking, and lifecycle acceptance are project-owned. |
| Ubuntu 24.04 cross-build host / Android armeabi-v7a and arm64-v8a | Android NDK Clang | `build_gated` | client shared library/package inputs | `android-arm32-client`, `android-arm64-client` | No emulator or physical-device smoke route is required. | APK assembly, signing, store delivery, device GPU/input/audio, pause/resume, and network acceptance are project-owned. |
| cross-build host / Android x86 | Android NDK Clang | `source_capable` | client shared library/package inputs | `android-x86-client` | No required CI or runtime lane exists. | Do not advertise Android x86 as a supported release target without a project-owned build and device/emulator gate. |
| Ubuntu 24.04 cross-build host / WebAssembly | Emscripten Clang | `smoke_gated` | browser client | `web-client`, `web-showcase-runtime` | The required workflow builds the stock browser client and runs the Content Showcase through native-host force-baking, raw/ZIP Web packaging, exact payload checks, a native server, packaged HTTP delivery, Chromium, a real WebGL 2 context, lifecycle markers, and compositor-pixel validation. | The stock web target is WebGL 2 client-only. The Content Showcase qualifies one deterministic localhost fixture under pinned Chromium; it does not qualify every browser/GPU pair or production hosting, headers, persistence, audio activation, deployment, CDN, signing, or rollback behavior. Those remain project-owned acceptance gates. |
| Windows / Windows x64 | ClangCL 20 or newer | `source_capable` | desktop client; server; AngelScript compiler; resource baker | `win64-clang-client`, `win64-clang-server`, `win64-clang-ascompiler`, `win64-clang-baker` | No required CI or runtime lane exists. | Use MSVC for the qualified Windows route unless the embedding project adds and owns ClangCL validation. |

## Renderer qualification

| Platforms | Compiled backends | Qualification boundary |
|---|---|---|
| Windows, Linux, macOS | platform OpenGL plus opt-out Vulkan and SDL_GPU where the source backend can initialize | Build coverage is not renderer/runtime-driver coverage; every shipped backend needs a visible project scene. |
| Android and iOS | OpenGL ES plus platform capabilities; Vulkan and SDL_GPU remain compile-time optional outside Web | Device acceptance is required. |
| Web | WebGL 2 | Vulkan and SDL_GPU are excluded by the platform stage; browser acceptance is required. |
| headless applications | null/headless frontend | Do not infer visible rendering or audio support from a headless smoke. |

## Summary

- Platform profiles: **10**
- Build-gated profiles: **8**
- Smoke-gated profiles: **3**
- Distinct required CI validation targets: **35**

See [Support Matrix](support-matrix.md) for release interpretation and project acceptance requirements.
