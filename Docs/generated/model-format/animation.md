---
title: Model Animation Directives
document_id: generated-model-format-animation
locale: en
generated: true
---

# Model Animation Directives

> Generated reference. Do not edit directly. Update `BuildTools/ModelFormatInterface.json`, then run `python BuildTools/docs_model_format.py --write`.

[Index](index.md) | [Syntax](syntax.md) | [Tokens](tokens.md) | [Composition](composition.md) | [Assets](assets.md) | [Animation](animation.md) | [Validation](validation.md) | [Canonical JSON](../model-format.json) | [Guide](../../ModelFormat.md)

This page lists `.fo3d` directives that participate in animation selection or movement-pose composition. Effective durations, alias materialization, and script lookup are documented in [ModelAnimation.md](../../ModelAnimation.md).

| Directive | Syntax | Authoring contract | Runtime effect |
| --- | --- | --- | --- |
| <code>Anim</code> | <code>Anim &lt;state&gt; &lt;action&gt; &lt;ModelFile&#124;animation-mesh&gt; &lt;clip&#124;~clip&#124;Base&gt;</code> | Maps a state/action pair to a source animation clip. ModelFile selects the primary model source, ~ reverses playback, and Base selects the first source clip before conversion into the baked runtime rig. | The first declaration for a pair is registered; model-specific lookup and substitutions are described in ModelAnimation.md. |
| <code>AnimSpeed</code> | <code>AnimSpeed &lt;state&gt; &lt;action&gt; &lt;positive-float&gt;</code> | Sets authored playback speed for one mapped state/action pair. | The speed multiplies runtime playback and divides the common effective duration. |
| <code>AllowAnimationGeometry</code> | <code>AllowAnimationGeometry &lt;external-animation-file&gt;</code> | Temporarily permits drawable geometry in one exact external Anim source while that source is repaired into a geometry-free animation file. The path resolves from the final concrete description, like an external Anim path. | Validation-only: the exception is not serialized. Duplicate, unselected, duplicate-resolved, or stale exceptions fail the bake, so remove each line with the repaired source export. |
| <code>AnimLayerValue</code> | <code>AnimLayerValue &lt;state&gt; &lt;action&gt; &lt;layer&gt; &lt;value&gt;</code> | Overrides one layer value whenever the exact authored state/action pair is requested. | The override is applied before redundant-call detection and model composition. |
| <code>FastTransitionBone</code> | <code>FastTransitionBone &lt;bone&gt;</code> | Marks a validated base-model bone for immediate transition reset when a newly attached child uses that Link bone. | The next body-animation track resets transition state for the marked attachment bone. |
| <code>StateAnimEqual</code> | <code>StateAnimEqual &lt;from-state&gt; &lt;to-state&gt;</code> | Defines a one-step state-animation alias. | The alias is applied once before exact animation lookup and has priority over an exact source-key entry. |
| <code>ActionAnimEqual</code> | <code>ActionAnimEqual &lt;from-action&gt; &lt;to-action&gt;</code> | Defines a one-step action-animation alias. | The alias is applied once before exact animation lookup and has priority over an exact source-key entry. |
| <code>DisableAnimationInterpolation</code> | <code>DisableAnimationInterpolation</code> | Disables keyframe interpolation on the model animation controller. | The registered animation controller samples without interpolation. |
| <code>DisableBackwardAnim</code> | <code>DisableBackwardAnim</code> | Disables WalkBack and RunBack selection for movement-pose animation. | Movement always uses forward walk/run and SetMoveDir also aligns look direction. |
| <code>RotationBone</code> | <code>RotationBone &lt;bone&gt;</code> | Selects the validated torso/body rotation bone and enables the movement overlay controller. | Look and move directions may diverge; body and configured head bones receive directional rotation while movement/turn animations play on the overlay controller. |

## Separation from 2D root motion

These directives drive 3D skeletal clips and model composition. `NextX` / `NextY` sprite-frame offsets and movement-projected frame selection belong to [SpriteRootMotion.md](../../SpriteRootMotion.md).
