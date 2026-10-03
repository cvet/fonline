---
title: Effect Format Validation
document_id: generated-effect-format-validation
locale: en
generated: true
---

# Effect Format Validation

> Generated reference. Do not edit directly. Update `BuildTools/EffectFormatInterface.json`, then run `python BuildTools/docs_effect_format.py --write`.

[Index](index.md) | [Syntax](syntax.md) | [Render state](render-state.md) | [Resources](resources.md) | [Baking](baking.md) | [Runtime](runtime.md) | [Validation](validation.md) | [Canonical JSON](../../../generated/effect-format.json) | [Guide](../../how-to/content/effect-format.md)

| Stable ID | Rule | Requirement | Why | Source |
| --- | --- | --- | --- | --- |
| <a id="entry-effect-format-validation-required-sections-7eb31d9eec"></a><code>effect-format.validation.required-sections</code> | Required source content | Reject a missing Effect section or a pass without usable vertex or fragment text. | A partial source cannot produce a complete linked program or runtime state. | [Source/Tests/Test_EffectBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_EffectBaker.cpp) |
| <a id="entry-effect-format-validation-compiler-diagnostics-cc1a443e82"></a><code>effect-format.validation.compiler-diagnostics</code> | Compiler diagnostics | Shader parse/link/reflection failures stop effect baking and normalize compiler diagnostics to a single log line. | Build logs remain machine-readable while preserving the shader compiler message. | [Source/Tests/Test_EffectBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_EffectBaker.cpp), [Source/Tools/EffectBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/EffectBaker.cpp) |
| <a id="entry-effect-format-validation-buffer-layout-0086cb6c8e"></a><code>effect-format.validation.buffer-layout</code> | Uniform buffer layout | Reject known uniform buffers whose reflected byte size differs from the corresponding RenderEffect structure and reject every unknown uniform block. | All backends upload fixed native structures without per-shader layout adaptation. | [Source/Tests/Test_EffectBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_EffectBaker.cpp), [Source/Tools/EffectBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/EffectBaker.cpp) |
| <a id="entry-effect-format-validation-bindings-b0fad0d441"></a><code>effect-format.validation.bindings</code> | Descriptor bindings | Reject missing explicit bindings, duplicate same-stage bindings, dead descriptor declarations, and SDL_GPU stage-limit overflow. | Dense SDL slot remapping must be deterministic and complete. | [Source/Tools/EffectBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/EffectBaker.cpp) |
| <a id="entry-effect-format-validation-output-flavors-5e48dca94f"></a><code>effect-format.validation.output-flavors</code> | Baked flavor completeness | Verify source, metadata, native SPIR-V, SDL SPIR-V, GLSL, GLSL ES, HLSL, and both MSL outputs for every stage/pass. | A resource can pass one backend and still be incomplete for another package. | [Source/Tests/Test_EffectBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_EffectBaker.cpp) |
| <a id="entry-effect-format-validation-metadata-bindings-6620bcd7a8"></a><code>effect-format.validation.metadata-bindings</code> | Reflection metadata | Verify native [EffectInfo] binding values and SDL [EffectInfoSdl] counts/slots for representative effects. | Backends depend on metadata identity even when shader compilation itself succeeds. | [Source/Tests/Test_EffectBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_EffectBaker.cpp) |
| <a id="entry-effect-format-validation-cross-backend-3867b45fe8"></a><code>effect-format.validation.cross-backend</code> | Cross-backend visible validation | Validate project effects on every renderer/backend profile the embedding project ships, including depth/blend-sensitive scenes and the lowest supported shader profile. | Cross-compilation cannot prove driver behavior, feature-level support, visual output, or performance. | [Source/Frontend/Rendering-OpenGL.cpp](https://github.com/cvet/fonline/blob/master/Source/Frontend/Rendering-OpenGL.cpp), [Source/Frontend/Rendering-Direct3D.cpp](https://github.com/cvet/fonline/blob/master/Source/Frontend/Rendering-Direct3D.cpp), [Source/Frontend/Rendering-Vulkan.cpp](https://github.com/cvet/fonline/blob/master/Source/Frontend/Rendering-Vulkan.cpp), [Source/Frontend/Rendering-SDLGpu.cpp](https://github.com/cvet/fonline/blob/master/Source/Frontend/Rendering-SDLGpu.cpp) |
| <a id="entry-effect-format-validation-project-ownership-bdef2e2a3d"></a><code>effect-format.validation.project-ownership</code> | Project contract validation | An embedding project must validate every effect path, EffectType/subtype assignment, ScriptValue slot registry, buffer writer, resource-pack override, and visible fallback it owns. | The Engine can validate format mechanics but cannot infer project shader semantics or art-direction intent. | [Source/Client/EffectManager.h](https://github.com/cvet/fonline/blob/master/Source/Client/EffectManager.h), [Source/Frontend/Rendering.h](https://github.com/cvet/fonline/blob/master/Source/Frontend/Rendering.h) |

## Validation commands

```powershell
python BuildTools\docs_effect_format.py --check
python -m unittest BuildTools.tests.test_docs_effect_format
cmake --build <build-dir> --config RelWithDebInfo --target RunUnitTests
```

Finish in an embedding project with its resource bake and visible checks on every renderer/backend profile that the project supports.
