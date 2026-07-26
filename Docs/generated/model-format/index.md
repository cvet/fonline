---
title: Generated Model Format Reference
document_id: generated-model-format-index
locale: en
generated: true
---

# Generated Model Format Reference

> Generated reference. Do not edit directly. Update `BuildTools/ModelFormatInterface.json`, then run `python BuildTools/docs_model_format.py --write`.

[Index](index.md) | [Syntax](syntax.md) | [Tokens](tokens.md) | [Composition](composition.md) | [Assets](assets.md) | [Animation](animation.md) | [Validation](validation.md) | [Canonical JSON](../model-format.json) | [Guide](../../ModelFormat.md)

This reference describes the reusable Engine-owned `.fo3d` language and the model assets it composes. Concrete game models and layer meanings remain project-owned.

## Contract status

| Field | Value |
| --- | --- |
| Stability | <code>experimental</code> |
| Support policy | The contract is generated for a pinned Engine revision. Projects own model catalogs, layer meanings, animation enums, visual policy, and concrete assets. |
| Source manifest | [BuildTools/ModelFormatInterface.json](https://github.com/cvet/fonline/blob/master/BuildTools/ModelFormatInterface.json) |
| Contract digest | <code>6aa5884b57138ce170ba7f63222631e1ea8178765ec33ce10e51e95b83bfdaa5</code> |
| Source extension | <code>.fo3d</code> |
| Mesh inputs | <code>.fbx</code>, <code>.obj</code> |
| Runtime side | <code>client</code> |

| Reference | Entries | Purpose |
| --- | --- | --- |
| [Tokens](tokens.md) | 32 groups / 59 spellings | Every accepted current parser token. |
| [Assets](assets.md) | 6 | Mesh, description, texture, effect, and particle inputs. |
| [Validation](validation.md) | 13 | Authoring, baking, runtime, and legacy rules. |

## Boundary

Included:

- .fo3d lexical syntax, include templates, parser state, and path resolution
- model layers, root modifiers, mesh/model/particle attachments, transforms, materials, effects, and cuts
- FBX and OBJ mesh input, baked hierarchy requirements, compile-time model limits, and runtime composition
- animation integration points that connect to the dedicated model-animation reference

Excluded:

- project model catalogs, layer-number semantics, enum assignments, equipment policy, and gameplay timing
- DCC authoring tutorials for Blender, Maya, 3ds Max, or other external tools
- renderer backend implementation details and shader-language reference
- 2D sprite frame offsets and sprite root motion
