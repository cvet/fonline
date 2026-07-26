---
title: Generated Effect Format Reference
document_id: generated-effect-format-index
locale: en
generated: true
---

# Generated Effect Format Reference

> Generated reference. Do not edit directly. Update `BuildTools/EffectFormatInterface.json`, then run `python BuildTools/docs_effect_format.py --write`.

[Index](index.md) | [Syntax](syntax.md) | [Render state](render-state.md) | [Resources](resources.md) | [Baking](baking.md) | [Runtime](runtime.md) | [Validation](validation.md) | [Canonical JSON](../effect-format.json) | [Guide](../../EffectFormat.md)

This reference describes the reusable Engine-owned `.fofx` authoring, baking, renderer-resource, runtime-loading, and script-control contract. Concrete shader catalogs, visual policy, and ScriptValue slot meanings remain project-owned.

## Contract status

| Field | Value |
| --- | --- |
| Stability | <code>experimental</code> |
| Support policy | The contract is generated for a pinned Engine revision. Projects own effect catalogs, visual policy, quality profiles, concrete override paths, and ScriptValue slot meanings. |
| Source manifest | [BuildTools/EffectFormatInterface.json](https://github.com/cvet/fonline/blob/master/BuildTools/EffectFormatInterface.json) |
| Contract digest | <code>639a106c254f8593874e3a9f6da992058e665a6c20a63ad48b5a3ae82b50d22f</code> |
| Source extension | <code>.fofx</code> |
| Runtime side | <code>client</code> |

| Reference | Entries | Purpose |
| --- | --- | --- |
| [Syntax](syntax.md) | 4 | Required and optional sections plus pass fallback. |
| [Render state](render-state.md) | 8 | Pass count, blending, depth, shader version, and shadow state. |
| [Resources](resources.md) | 12 resources / 4 limits | Vertex layouts, samplers, built-in uniform buffers, and bindings. |
| [Baking](baking.md) | 8 | Compiler environment, reflection, output flavors, and SDL remapping. |
| [Runtime](runtime.md) | 7 rules / 4 methods | Effect selection, caching, ScriptValue persistence, and updates. |

## Boundary

Included:

- .fofx config sections, shader-stage fallback, pass declarations, render state, and shader compiler prelude
- vertex input layouts, recognized samplers, built-in uniform buffers, descriptor conventions, reflection, and backend limits
- per-pass baked artifacts for Vulkan, SDL_GPU, OpenGL, OpenGL ES, Direct3D, and Metal
- runtime loading, path cache identity, EffectUsage selection, script-value persistence, script methods, and validation

Excluded:

- project effect catalogs, shader art direction, quality tiers, and concrete resource-pack shadowing policy
- project-defined ScriptValue indices, ranges, ownership, defaults, and gameplay meaning
- general GLSL language reference, GPU performance tuning, and vendor-specific shader debugging
- particle-system authoring, image formats, sprite baking, model-description grammar, and GUI layout
