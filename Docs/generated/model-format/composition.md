---
title: Model Composition
document_id: generated-model-format-composition
locale: en
generated: true
---

# Model Composition

> Generated reference. Do not edit directly. Update `BuildTools/ModelFormatInterface.json`, then run `python BuildTools/docs_model_format.py --write`.

[Index](index.md) | [Syntax](syntax.md) | [Tokens](tokens.md) | [Composition](composition.md) | [Assets](assets.md) | [Animation](animation.md) | [Validation](validation.md) | [Canonical JSON](../model-format.json) | [Guide](../../ModelFormat.md)

Runtime composition starts from the default Root data, then activates links whose `Layer` and `Value` match the current model-layer array.

## Layer composition flow

1. Copy the project-provided layer array.
2. Apply exact `AnimLayerValue` overrides for the requested animation.
3. Apply default Root transforms, materials, effects, disables, and cuts.
4. Activate matching layer Root entries and child/particle attachments.
5. Remove children and particles whose links are no longer active.
6. Regenerate combined meshes when composition, materials, effects, or cuts changed.

## Composition directives

| Directive | Context | Authoring contract | Runtime effect | Source |
| --- | --- | --- | --- | --- |
| <code>Root</code> | <code>description or selected Layer/Value</code> | Selects the default root modifier when no Layer was selected, or creates a layer/value root modifier when Layer and non-zero Value are active. | The selected modifier can transform the model, change speed/materials/effects, disable meshes/layers, and apply cuts without creating a child. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp) |
| <code>Attach</code> | <code>selected Layer/Value</code> | Creates a layer-selected child-model link. The path is relative to the file containing the directive. | With Link, the child is attached to one parent bone. Without Link, same-named child and parent bones are paired for a shared-skeleton attachment. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp), [Source/Client/ModelInstance.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/ModelInstance.cpp) |
| <code>AttachParticles</code> | <code>selected Layer/Value</code> | Creates a layer-selected particle link. The resource path is stored verbatim rather than relative to the description. | The client creates the particle on the Link bone and removes it when the activating layer value is no longer selected. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp), [Source/Client/ModelInstance.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/ModelInstance.cpp) |
| <code>Link</code> | <code>current layer link</code> | Sets the parent bone for the current non-default link. It is ignored while the parser points at the default or dummy link. | A child model attaches as one object to this bone; particles require this bone. Empty child-model links do not consume it at runtime. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp) |
| <code>Cut</code> | <code>current link</code> | Adds one or more baked cut volumes to selected composed-mesh layers. Hyphen separates layer and shape lists; - omits unskin fields and ~ reverses the unskin shape. | Combined geometry inside or outside the authored cut shapes is removed; optional paired bones drive unskin handling. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp), [Source/Client/ModelInstance.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/ModelInstance.cpp) |
| <code>RotX</code>, <code>RotY</code>, <code>RotZ</code>, <code>MoveX</code>, <code>MoveY</code>, <code>MoveZ</code>, <code>ScaleX</code>, <code>ScaleY</code>, <code>ScaleZ</code>, <code>Speed</code> | <code>current link</code> | Sets one transform axis or playback-speed multiplier on the current link. Rotation values are authored in degrees. | Non-zero values multiply the model transform or speed chain. Zero means no contribution at runtime. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp), [Source/Client/ModelInstance.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/ModelInstance.cpp) |
| <code>Scale</code> | <code>current link</code> | Sets ScaleX, ScaleY, and ScaleZ to the same authored value. | A non-zero value contributes a uniform scale transform. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp) |
| <code>RotX+</code>, <code>RotY+</code>, <code>RotZ+</code>, <code>MoveX+</code>, <code>MoveY+</code>, <code>MoveZ+</code>, <code>ScaleX+</code>, <code>ScaleY+</code>, <code>ScaleZ+</code>, <code>Speed+</code> | <code>current link</code> | Adds to one transform or speed field. When the current field is zero, the operand becomes the initial value. | Includes can layer additive adjustments without requiring a preceding base assignment. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp) |
| <code>Scale+</code> | <code>current link</code> | Applies the additive rule to all three scale axes. | Provides a uniform additive scale adjustment for templates and selected links. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp) |
| <code>RotX*</code>, <code>RotY*</code>, <code>RotZ*</code>, <code>MoveX*</code>, <code>MoveY*</code>, <code>MoveZ*</code>, <code>ScaleX*</code>, <code>ScaleY*</code>, <code>ScaleZ*</code>, <code>Speed*</code> | <code>current link</code> | Multiplies one transform or speed field. When the current field is zero, the operand becomes the initial value. | Includes can apply proportional adjustments while preserving zero as the runtime identity sentinel. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp) |
| <code>Scale*</code> | <code>current link</code> | Applies the multiplicative rule to all three scale axes. | Provides a uniform proportional scale adjustment for templates and selected links. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp) |
| <code>DisableLayer</code> | <code>current link</code> | Adds layer indices to the current link's disabled-layer set. Every value is range checked. | When the link is active, matching layer slots are skipped for that model instance. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp), [Source/Client/ModelInstance.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/ModelInstance.cpp) |
| <code>DisableMesh</code> | <code>current link</code> | Adds drawable mesh names to the current link's disabled set. All stores the empty wildcard. | When the link is active, matching meshes in that model instance are omitted from combined geometry. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp), [Source/Client/ModelInstance.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/ModelInstance.cpp) |
| <code>Texture</code> | <code>current link and Mesh selector</code> | Overrides one texture slot on the selected mesh or all meshes. Non-Parent names resolve relative to the current model mesh; Parent copies the active parent texture from an attached-model context. | The override participates in mesh batching and texture-atlas coordinate adjustment. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp), [Source/Client/ModelInstance.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/ModelInstance.cpp) |
| <code>Effect</code> | <code>current link and Mesh selector</code> | Overrides the draw effect on the selected mesh or all meshes. Parent copies the active parent effect from an attached-model context. | Meshes with different effects cannot share one combined draw batch. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp), [Source/Client/ModelInstance.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/ModelInstance.cpp) |
| <code>DisableShadow</code> | <code>description</code> | Disables shadow rendering for every instance of the description. | The model-level flag combines with the per-instance shadow toggle. | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp), [Source/Client/ModelInstance.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/ModelInstance.cpp) |

## Attachment choice

- Use `Attach child.fo3d` when the child needs its own model description, layers, materials, cuts, or animation declarations.
- Use `Attach child.fbx` or `Attach child.obj` for a direct baked hierarchy.
- Add `Link Bone` to place the complete child under one parent bone.
- Omit `Link` only when parent and child intentionally share same-named bones and the child should follow the parent skeleton.
- Use `AttachParticles ... Link Bone`; the runtime requires a target bone.
