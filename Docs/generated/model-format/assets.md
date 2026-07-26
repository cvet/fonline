---
title: Model Assets And Limits
document_id: generated-model-format-assets
locale: en
generated: true
---

# Model Assets And Limits

> Generated reference. Do not edit directly. Update `BuildTools/ModelFormatInterface.json`, then run `python BuildTools/docs_model_format.py --write`.

[Index](index.md) | [Syntax](syntax.md) | [Tokens](tokens.md) | [Composition](composition.md) | [Assets](assets.md) | [Animation](animation.md) | [Validation](validation.md) | [Canonical JSON](../model-format.json) | [Guide](../../ModelFormat.md)

`ModelMeshBaker` bakes mesh sources before `ModelInfoBaker` validates and serializes concrete `.fo3d` descriptions.

## Asset inputs

| Stable ID | Asset | Extensions | Purpose | Requirements | Source |
| --- | --- | --- | --- | --- | --- |
| <a id="entry-model-format-asset-fbx-c5e5ad7161"></a><code>model-format.asset.fbx</code> | FBX mesh | <code>.fbx</code> | Imports the drawable hierarchy, material texture names, and skin data for the mesh payload, while ModelSourceLoader extracts the source skeleton and animation clips that ModelInfoBaker converts into the required runtime rig. | faces must be triangulatable and the imported face count must agree with the generated triangle count<br>skin clusters must fit FO_MODEL_MAX_BONES<br>mesh skin references must resolve against the physical mesh hierarchy<br>source skeletons and clips must pass finite-value, hierarchy, count, key, and compatibility validation before conversion | [Source/Tools/ModelMeshBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelMeshBaker.cpp), [Source/Tools/ModelSourceLoader.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelSourceLoader.cpp) |
| <a id="entry-model-format-asset-obj-6c06dcc043"></a><code>model-format.asset.obj</code> | OBJ mesh | <code>.obj</code> | Imports a static hierarchy and drawable mesh through the same ufbx path; missing vertex attributes receive deterministic defaults. | the file must contain at least one drawable mesh for use as a concrete model<br>OBJ is suitable for static attachments and cut volumes, not authored skeletal animation stacks | [Source/Tools/ModelMeshBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelMeshBaker.cpp), [Source/Tests/Test_ModelBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_ModelBaker.cpp) |
| <a id="entry-model-format-asset-description-c03e9e142e"></a><code>model-format.asset.description</code> | Model description | <code>.fo3d</code> | Composes a primary baked mesh with layer-selected root modifiers, child models, particles, materials, effects, cuts, and animation mappings. | every concrete description must resolve a Model directive<br>files whose basename starts with TEMPLATE_ are include-only and are not emitted as models | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp) |
| <a id="entry-model-format-asset-texture-e163265b77"></a><code>model-format.asset.texture</code> | Model texture | <code>.png</code>, <code>.tga</code>, <code>.dds</code> | Default diffuse textures and explicit non-Parent Texture values resolve relative to the owning baked mesh file. | every imported default diffuse texture must exist in baked resources<br>an explicit texture target mesh must be drawable | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp), [Source/Client/ModelHierarchy.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/ModelHierarchy.cpp) |
| <a id="entry-model-format-asset-effect-2c95b05b0c"></a><code>model-format.asset.effect</code> | Model effect | <code>.fofx</code> | An explicit non-Parent Effect value is a baked-resource path loaded for EffectUsage::Model. | the effect resource must exist<br>an explicit effect target mesh must be drawable | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp), [Source/Client/ModelHierarchy.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/ModelHierarchy.cpp) |
| <a id="entry-model-format-asset-particle-40ad9e9d94"></a><code>model-format.asset.particle</code> | Particle attachment | <code>.fopts</code> | A layer-selected AttachParticles entry instantiates a particle resource on a model bone while that layer value remains active. | the particle resource must exist in baked resources<br>author a non-empty Link bone; runtime particle creation requires it | [Source/Tools/ModelInfoBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ModelInfoBaker.cpp), [Source/Client/ModelInstance.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/ModelInstance.cpp) |

## Compile-time limits

| Stable ID | Project option | Runtime constant | Default | Meaning |
| --- | --- | --- | --- | --- |
| <a id="entry-model-format-limit-layers-8359438ceb"></a><code>model-format.limit.layers</code> | <code>FO_MODEL_LAYERS_COUNT</code> | <code>MODEL_LAYERS_COUNT</code> | <code>30</code> | Number of layer slots in every model-layer array and the exclusive upper bound for Layer, DisableLayer, AnimLayerValue, and Cut layer indices. |
| <a id="entry-model-format-limit-textures-c365b16c3e"></a><code>model-format.limit.textures</code> | <code>FO_MODEL_MAX_TEXTURES</code> | <code>MODEL_MAX_TEXTURES</code> | <code>8</code> | Number of texture slots available to each mesh and the exclusive upper bound for Texture indices. |
| <a id="entry-model-format-limit-bones-b52a056500"></a><code>model-format.limit.bones</code> | <code>FO_MODEL_MAX_BONES</code> | <code>MODEL_MAX_BONES</code> | <code>54</code> | Maximum number of skin-bone matrices that one combined draw batch can carry. |
| <a id="entry-model-format-limit-bones-per-vertex-e51c03b244"></a><code>model-format.limit.bones-per-vertex</code> | <code>FO_MODEL_BONES_PER_VERTEX</code> | <code>MODEL_BONES_PER_VERTEX</code> | <code>4</code> | Maximum number of imported skin influences retained per vertex before weights are normalized. |

The defaults above come from the generated project-interface contract. A project may override them, but client binaries, baked resources, model layer properties, shaders, and packaged content must agree.
