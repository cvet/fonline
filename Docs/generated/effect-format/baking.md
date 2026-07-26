---
title: Effect Baking And Backends
document_id: generated-effect-format-baking
locale: en
generated: true
---

# Effect Baking And Backends

> Generated reference. Do not edit directly. Update `BuildTools/EffectFormatInterface.json`, then run `python BuildTools/docs_effect_format.py --write`.

[Index](index.md) | [Syntax](syntax.md) | [Render state](render-state.md) | [Resources](resources.md) | [Baking](baking.md) | [Runtime](runtime.md) | [Validation](validation.md) | [Canonical JSON](../effect-format.json) | [Guide](../../EffectFormat.md)

Every pass produces one metadata file and seven flavors per shader stage. The original `.fofx` source is copied to baked resources because the runtime still reads `[Effect]` state from it.

Stage flavors: <code>spv</code>, <code>spv_sdl</code>, <code>glsl</code>, <code>glsl_es</code>, <code>hlsl</code>, <code>msl_mac</code>, <code>msl_ios</code>

| Stable ID | Rule | Requirement | Why | Source |
| --- | --- | --- | --- | --- |
| <a id="entry-effect-format-baking-compiler-prelude-8ab13cba3b"></a><code>effect-format.baking.compiler-prelude</code> | Generated shader prelude | The baker prepends '#version &lt;Version&gt; es', 'precision highp float', MAX_SCRIPT_VALUES, and, in 3D builds, MAX_BONES and MAX_TEXTURES before ShaderCommon and stage text. | Authored shaders share one compile-time shape contract and must not duplicate these directives. | [Source/Tools/EffectBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/EffectBaker.cpp) |
| <a id="entry-effect-format-baking-compiler-target-4540599eb1"></a><code>effect-format.baking.compiler-target</code> | glslang target environment | Both stages compile as GLSL for a Vulkan 1.0 client and SPIR-V 1.0 target, then link and build reflection once per pass. | All backend flavors derive from the same validated linked program. | [Source/Tools/EffectBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/EffectBaker.cpp) |
| <a id="entry-effect-format-baking-link-interface-6b2005fdf3"></a><code>effect-format.baking.link-interface</code> | Stage interface validation | Vertex and fragment stages must parse, link, and expose a valid reflection model for every pass. | Location/type mismatches and syntax errors fail the resource bake before runtime. | [Source/Tools/EffectBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/EffectBaker.cpp) |
| <a id="entry-effect-format-baking-metadata-3a793719e1"></a><code>effect-format.baking.metadata</code> | Per-pass metadata | Each pass emits an info artifact with [EffectInfo] native bindings and [EffectInfoSdl] per-stage dense slots and resource counts. | Runtime backends consume reflection results without running glslang. | [Source/Tools/EffectBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/EffectBaker.cpp) |
| <a id="entry-effect-format-baking-native-flavors-0930a361af"></a><code>effect-format.baking.native-flavors</code> | Native and cross-compiled flavors | Each stage emits native Vulkan SPIR-V, desktop GLSL 330, GLSL ES 300, HLSL Shader Model 4.0, and Metal source for macOS and iOS. | One source effect supports the active renderer backends without project-side shader forks. | [Source/Tools/EffectBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/EffectBaker.cpp) |
| <a id="entry-effect-format-baking-sdl-remap-216674e85a"></a><code>effect-format.baking.sdl-remap</code> | SDL_GPU descriptor remap | The baker copies native SPIR-V, rewrites descriptor sets and bindings to SDL_GPU's per-stage convention, and emits the result as spv_sdl. Metal source is compiled from the remapped module. | Native Vulkan bindings remain untouched while SDL_GPU gets dense stage-local slots. | [Source/Tools/EffectBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/EffectBaker.cpp) |
| <a id="entry-effect-format-baking-source-copy-cb116df5fd"></a><code>effect-format.baking.source-copy</code> | Baked source copy | The original .fofx source is written to the baked resource set after all pass artifacts. | RenderEffect reparses [Effect] state at runtime; the source file is part of the runtime artifact set. | [Source/Tools/EffectBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/EffectBaker.cpp), [Source/Frontend/Rendering.cpp](https://github.com/cvet/fonline/blob/master/Source/Frontend/Rendering.cpp) |
| <a id="entry-effect-format-baking-incremental-be6a0c7939"></a><code>effect-format.baking.incremental</code> | Incremental output set | The bake checker tracks the source copy, pass metadata, and every stage/flavor artifact for every declared pass. | A missing or stale flavor must make the effect eligible for rebaking. | [Source/Tools/EffectBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/EffectBaker.cpp) |
