---
title: Effect File Syntax
document_id: generated-effect-format-syntax
locale: en
generated: true
---

# Effect File Syntax

> Generated reference. Do not edit directly. Update `BuildTools/EffectFormatInterface.json`, then run `python BuildTools/docs_effect_format.py --write`.

[Index](index.md) | [Syntax](syntax.md) | [Render state](render-state.md) | [Resources](resources.md) | [Baking](baking.md) | [Runtime](runtime.md) | [Validation](validation.md) | [Canonical JSON](../effect-format.json) | [Guide](../../EffectFormat.md)

A minimal one-pass effect contains the required config section and one vertex/fragment shader pair:

```ini
[Effect]

[VertexShader]
layout(set = 0, binding = 0, std140) uniform ProjBuf { mat4 ProjMatrix; };
layout(location = 0) in vec3 InPosition;
void main(void) { gl_Position = ProjMatrix * vec4(InPosition, 1.0); }

[FragmentShader]
layout(location = 0) out vec4 FragColor;
void main(void) { FragColor = vec4(1.0); }
```

| Stable ID | Section | Requirement | Why | Source |
| --- | --- | --- | --- | --- |
| <a id="entry-effect-format-section-effect-2883b72871"></a><code>effect-format.section.effect</code> | [Effect] | Every .fofx source must contain an Effect section. It stores pass count, shader version, shadow-pass selection, and default or per-pass render state. | Both the baker and runtime loader reject a source without this section. | [Source/Tools/EffectBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/EffectBaker.cpp), [Source/Frontend/Rendering.cpp](https://github.com/cvet/fonline/blob/master/Source/Frontend/Rendering.cpp) |
| <a id="entry-effect-format-section-shader-common-3bb6a58153"></a><code>effect-format.section.shader-common</code> | [ShaderCommon] | ShaderCommon is optional raw shader text prepended to both stages of every pass after the Engine-generated prelude. | It is the only format-level reuse block; the .fofx parser has no include directive. | [Source/Tools/EffectBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/EffectBaker.cpp) |
| <a id="entry-effect-format-section-vertex-0fcaf320c3"></a><code>effect-format.section.vertex</code> | [VertexShader] and [VertexShader PassN] | Each declared pass needs non-empty vertex shader text. The baker first reads VertexShader PassN and falls back to the generic VertexShader section when the pass-specific section is empty. | A shared vertex stage can serve all passes while selected passes override it. | [Source/Tools/EffectBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/EffectBaker.cpp) |
| <a id="entry-effect-format-section-fragment-1425c78deb"></a><code>effect-format.section.fragment</code> | [FragmentShader] and [FragmentShader PassN] | Each declared pass needs non-empty fragment shader text. The baker first reads FragmentShader PassN and falls back to the generic FragmentShader section when the pass-specific section is empty. | Multi-pass effects often share geometry processing and vary only the fragment stage. | [Source/Tools/EffectBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/EffectBaker.cpp) |
