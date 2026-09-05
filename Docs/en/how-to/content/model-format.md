---
layout: default
title: Model Format and 3D Composition
document_id: model-format-guide
locale: en
permalink: /Docs/en/how-to/content/model-format.html
---

# Model Format and 3D Composition

FOnline uses `.fo3d` model descriptions to compose baked 3D meshes, converted source animations, layer-selected equipment, child models, particles, material overrides, and cut volumes into a client-side model instance.

Use this guide for the authoring and runtime model. Use the generated [token reference](../../reference/model-format/tokens.md), [asset and limit reference](../../reference/model-format/assets.md), [validation rules](../../reference/model-format/validation.md), and [canonical JSON model](../../../generated/model-format.json) for the exact current-revision contract.

## Scope and authority

The owning sources are:

- `Source/Tools/ModelMeshBaker.cpp` and `Source/Common/ModelMeshData.*` for `.fbx` / `.obj` mesh import, validation, and the mesh-only `LFMODMSH` payload;
- `Source/Tools/ModelSourceLoader.*`, `Source/Tools/ModelAnimationConverter.*`, and `Source/Common/ModelAnimationData.*` for source skeleton/clip extraction, compatibility analysis, Ozz conversion, and the native rig payload;
- `Source/Tools/ModelInfoBaker.cpp` for `.fo3d` parsing, include expansion, dependency and source validation, model-info serialization, runtime-rig creation, and animation metadata;
- `Source/Client/ModelManager.*`, `ModelHierarchy.*`, `ModelInformation.*`, `ModelInstance.*`, and `ModelAnimation.*` for strict runtime loading, shared immutable data, per-instance composition/pose state, animation controllers, and drawing;
- `Source/Frontend/Rendering.h` plus the generated CMake project interface for compile-time model limits;
- the model baker, mesh-data, source-loader, animation-data/converter/runtime, skeleton-compatibility, Ozz, and client-engine tests for executable grammar, wire, conversion, loading, and failure examples.

This page is reusable Engine documentation. An embedding project owns concrete model names, layer meanings, model-layer values, animation enums, art direction, equipment rules, gameplay timing, and visible validation scenes.

The exhaustive machine model is generated from `BuildTools/ModelFormatInterface.json`. Its generator compares the documented token set directly with `ModelDescriptionParser::ParseToken`; parser drift makes documentation validation fail.

## Pipeline overview

The model pipeline has two ordered bakers and shared source/conversion modules:

1. `ModelMeshBaker` runs at order `4`. It imports `.fbx` and `.obj` source files and writes a versioned, mesh-only `LFMODMSH` resource at the same path and extension. Clips and mutable pose data are not stored in this hierarchy payload.
2. `ModelInfoBaker` runs at order `6`. It parses concrete `.fo3d` files, validates references and source freshness, loads the selected source skeletons/clips through the per-bake `ModelSourceAssetCache`, converts a canonical runtime rig, and writes versioned `LFMODINF` with a required `LFOZZRIG` payload at the same `.fo3d` path. It also emits `ModelAnimationInfo.foinfo` for common duration and bounds lookup.

The client never parses authored text or source FBX/OBJ data. `ModelManager` loads shared mesh hierarchies, `ModelInformation` strictly loads one immutable model description and runtime rig, and each `ModelInstance` owns mutable controllers, pose matrices, children, particles, and render composition. Old headerless payloads and partial rig fallbacks are rejected.

Files whose basename starts with `TEMPLATE_` are include-only. They affect concrete descriptions and bake timestamps, but are not emitted as independent `.fo3d` resources or `ModelAnimationInfo.foinfo` sections.

## Source mesh contract

### Supported inputs

Current `ModelMeshBaker` scans only:

- `.fbx` for skeletal or static meshes, skinning, material diffuse texture names, source skeletons, and animation clips;
- `.obj` for static models, attachments, and cut volumes.

Legacy `.x` and `.3ds` model paths are not current inputs. A file retaining one of those extensions is not selected by `ModelMeshBaker`.

### Import behavior

The mesh baker and source loader use pinned `ufbx` through separate owners. The mesh baker emits hierarchy, bind, vertex, index, skin, and material data; the source loader extracts validated skeleton/TRS/clip data for animation conversion. Embedded files are ignored, skinning is evaluated, skin weights are cleaned, and missing normals receive deterministic handling in the mesh path.

Author meshes with these constraints:

- faces must already be triangles;
- a concrete model must contain at least one drawable mesh;
- node names become bone names, and nodes with attached geometry become drawable mesh names;
- use one material per drawable node when deterministic texture ownership matters;
- the first material's file-backed `DiffuseColor` texture becomes texture slot `0`;
- texture filenames are stored without their original directory and later resolved relative to the baked mesh;
- only the first skin deformer is consumed;
- skin-cluster count must fit `FO_MODEL_MAX_BONES`;
- only `FO_MODEL_BONES_PER_VERTEX` influences are retained per vertex, then normalized;
- a non-skinned mesh receives a deterministic single-bone fallback;
- animation stack names are the clip names used by `.fo3d` `Anim` entries, but those clips are loaded from source and converted into the model-info rig rather than serialized into `LFMODMSH`;
- direct `.fbx` attachments must be rest-only: use a child `.fo3d` with explicit `Anim` mappings when an attached source contains clips;
- external animation sources should contain hierarchy and animation only. Drawable geometry is rejected unless the exact selected file has a temporary `AllowAnimationGeometry` exception.

The source loader rejects non-finite transforms, duplicate case-insensitive clip names, invalid durations or key times, excessive counts/depth, and malformed skeleton relationships before conversion. Animation sources may contribute compatible canonical joints that have no physical `ModelBone`; physical meshes and cuts remain in the base hierarchy.

The current default limits are:

| Project option | Runtime constant | Default |
|---|---|---:|
| `FO_MODEL_LAYERS_COUNT` | `MODEL_LAYERS_COUNT` | `30` |
| `FO_MODEL_MAX_TEXTURES` | `MODEL_MAX_TEXTURES` | `8` |
| `FO_MODEL_MAX_BONES` | `MODEL_MAX_BONES` | `54` |
| `FO_MODEL_BONES_PER_VERTEX` | `MODEL_BONES_PER_VERTEX` | `4` |

These are binary and shader shape contracts. If a project overrides them, its client binaries, baked model resources, `Critter.ModelLayers` data, effects, and packages must use the same values.

## Lexical syntax

A `.fo3d` file is a sequence of whitespace-separated tokens:

- `#` and `;` start a comment;
- there is no quoted-string or escape syntax;
- paths and names therefore cannot contain whitespace;
- one line may contain multiple directives;
- each directive consumes its required arguments and parsing continues with the next token;
- unknown tokens and missing arguments are bake errors;
- integer arguments accept numbers, explicit booleans, or enum names known to the baking metadata resolver;
- float arguments must parse as finite numbers.

Compact entries are legal:

```text
Layer 1 Value 2 Attach Hat.fbx Link Head RotY 180 Texture 0 Hat.tga
```

For maintainability, keep structural selectors (`Layer`, `Value`, `Root`, `Attach`) before the modifiers that apply to them.

## Minimal descriptions

A static model can be as small as:

```text
Model Props/Crate.obj
```

A skeletal model with one animation and one layer-selected attachment:

```text
Model Characters/Human.fbx
RotationBone Spine

Anim CritterStateAnim.Unarmed CritterActionAnim.Idle ModelFile Idle
Anim CritterStateAnim.Unarmed CritterActionAnim.Walk ModelFile Walk

Layer 1
Value 1
Attach Items/Hat.fbx Link Head
```

Enum names depend on the embedding project's metadata. Numeric examples in engine tests prove parser behavior but are not a recommended project vocabulary.

## Includes and templates

`Include` parses another file inline:

```text
Include TEMPLATE_Humanoid.fo3d mesh Human.fbx scale 0.9
```

Arguments after the path are name/value pairs. Before tokenization, every literal `%name%` in the included text is replaced with its value:

```text
# TEMPLATE_Humanoid.fo3d
Model %mesh%
Scale* %scale%
```

Important include rules:

- the include path is relative to the file containing `Include`;
- `Model`, `Attach`, and `Cut` paths inside the included text are relative to the included file;
- an `Anim` file other than `ModelFile` is resolved later relative to the concrete `.fo3d` output;
- include arguments consume the rest of their line, so do not place another directive after them;
- replacements are plain text, not token-aware substitutions;
- included content shares parser state with its caller;
- recursive includes are rejected;
- the newest timestamp in the complete include graph controls incremental rebaking.

Prefer self-contained templates that establish their own `Root`, `Layer`, `Value`, and `Mesh` context. A template that silently depends on caller state is difficult for humans, AI agents, and validators to reason about.

## Parser state

The parser tracks:

- the selected `Layer`;
- the selected `Value`;
- the current link receiving modifiers;
- the current `Mesh` selector used by `Texture` and `Effect`.

At file start, the current link is the default root. Top-level transforms and material modifiers therefore apply to the base model even without an explicit `Root`.

`Layer` or `Value`:

- updates that selector;
- clears `Mesh`;
- redirects the current link to a dummy object.

After selecting a layer/value pair, write `Root`, `Attach`, or `AttachParticles` before any transform, material, disable, or cut directive. Modifiers written while the dummy link is current are parsed but discarded.

There is no directive that restores `Layer` to the initial `-1` state. Author all default-root declarations before the first `Layer`, or put them in an earlier include.

`Root` also clears `Mesh`. `Attach` and `AttachParticles` create a new link and clear `Mesh`. Put `Mesh` after the link selector it should affect.

`Link` is stored only for a non-default, non-dummy link. On the default root it is ignored. A `Link` on a layer `Root` is serialized but has no child to attach; omit it.

## Layers and values

`Layer` selects an index in the fixed model-layer array. The valid range is:

```text
0 <= layer < FO_MODEL_LAYERS_COUNT
```

`Value` selects an exact project-defined integer. Zero means inactive and cannot create a layer `Root`, `Attach`, or `AttachParticles` entry.

At runtime, `ModelInstance::PlayAnim`:

1. copies the supplied model-layer array, or reuses the previous array;
2. applies exact `AnimLayerValue` overrides for the requested state/action pair;
3. resets the model to default-root data;
4. finds entries whose `Layer` and `Value` match;
5. applies root modifiers and material changes;
6. creates or retains child models and particles;
7. removes no-longer-selected children and particles;
8. regenerates combined meshes when composition changed.

A layer value is rendering composition state, not merely cosmetic metadata. Changing it can alter transforms, animation speed, visible geometry, materials, draw effects, cuts, child models, particles, and batching.

The meaning of every layer index and value belongs in the embedding project's documentation and tests.

## Root modifiers

`Root` selects the base model's default link when no layer has been selected:

```text
Root
Scale 0.9
RotX 90
```

With a selected non-zero layer/value pair, it creates a conditional root modifier:

```text
Layer 3
Value 2
Root
DisableMesh Torso
Texture 0 Armor.tga
```

Conditional root links can:

- add transforms and speed multipliers;
- override textures and effects;
- disable other layers;
- disable meshes;
- add cut volumes.

They do not create a child model.

## Model attachments

`Attach` requires a selected layer and non-zero value:

```text
Layer 1
Value 4
Attach Weapons/Rifle.fo3d Link RightHand
```

The child path is relative to the declaring file.

### Single-bone attachment

With `Link <bone>`, the complete child is parented to one validated bone:

```text
Attach Hat.fbx Link Head
```

The link's rotation, translation, scale, speed, materials, disables, and cuts apply inside the child model instance.

### Shared-skeleton attachment

Without `Link`, runtime pairs same-named child and parent bones:

```text
Attach ArmorTorso.fbx
```

Use this only for clothing or body-part assets authored against the same skeleton. Runtime creation fails if no common bones exist.

### Child descriptor versus direct mesh

Use `Attach child.fo3d` when the child needs its own:

- base mesh selection;
- nested layers or attachments;
- default material/effect policy;
- cuts;
- animation declarations;
- rendering flags.

Use direct `.fbx` / `.obj` attachment for a simple baked hierarchy. A child `.fo3d` is baked and validated independently; the parent validates that the descriptor exists and that the parent `Link` bone is valid.

A direct attachment has no description-level scale correction. Its static maximum-axis extent must remain within `Baking.ModelAttachmentMinExtent` .. `Baking.ModelAttachmentMaxExtent`; otherwise baking fails with the measured extent and limit. Use a child `.fo3d` when an explicit scale is part of the composition. Mesh nodes with a negative transform determinant are rejected earlier by `ModelMeshBaker`: reset/freeze mirrored geometry to a positive transform before export instead of relying on the baker to flip normals and winding.

## Particle attachments

`AttachParticles` is layer-selected:

```text
Layer 8
Value 1
AttachParticles Particles/Jet.spk Link Backpack
MoveY 0.15
RotY 90
```

The particle path is a global baked-resource path, not relative to the `.fo3d` file. Reference the generated `.spk` or `.efk` resource rather than its `.spark` or `.efkproj` authoring source. Always provide a valid `Link` bone; runtime particle creation requires it.

The link's `MoveX`, `MoveY`, `MoveZ`, and `RotY` feed particle placement. The particle instance remains alive while the exact layer/value link stays active and is removed when composition changes.

The attached resource's XML, registered SPARK objects, renderer fields, effects/textures, runtime cache, and visible validation are owned by [Particle Format And Runtime](particle-format.md). Model-bone particles use the direct 3D composition path rather than `ParticleSprite`'s atlas/direct-scene selector.

## Transforms and speed

Per-link fields are:

- `RotX`, `RotY`, `RotZ` in degrees;
- `MoveX`, `MoveY`, `MoveZ` in model coordinates;
- `ScaleX`, `ScaleY`, `ScaleZ`;
- `Speed` as a playback multiplier.

`Scale` sets all three scale axes.

Every field has assignment, additive, and multiplicative forms:

```text
Scale 0.9
Scale+ 0.1
Scale* 1.5

RotY 90
RotY+ 15
RotY* 0.5
```

The `+` and `*` forms use special zero initialization:

- if the current field is zero, the operand becomes the field value;
- otherwise addition or multiplication is applied normally.

This lets a template use `Scale* 0.9` or `Speed* 1.2` without requiring an earlier assignment. It also means declaration order is observable.

At runtime, zero means identity/no contribution. Non-zero transforms are multiplied into the current model transform. A negative final `Speed` is rejected during baking; zero means no speed contribution.

## Meshes, textures, and effects

`Mesh` selects a drawable node by name:

```text
Mesh Torso
Texture 0 Armor.tga
Effect Effects/Armor.fofx
```

`Mesh All` clears the selector, so following material directives target every drawable mesh in the current link's model.

`Subset` is obsolete. The parser consumes its argument and logs a warning, but does not select anything. Never use it in new content.

### Textures

```text
Texture <slot> <name>
```

The slot must be in `[0, FO_MODEL_MAX_TEXTURES)`.

For a normal texture name:

- the target `Mesh` must exist and be drawable;
- the texture path is resolved relative to the current target mesh file;
- the texture must exist in baked resources.

An imported material's diffuse texture is the default for slot `0`. All other slots start empty unless assigned.

Inside an attached child, `Parent` copies the first matching parent's current texture at the same slot. `Parent_<mesh>` copies it from the named parent mesh:

```text
Texture 0 Parent_Torso
```

Do not use `Parent` on a root description. When a parent has several meshes, prefer the explicit suffix.

### Effects

```text
Effect Effects/SkinnedArmor.fofx
```

Effect paths are global baked-resource paths loaded for model usage. `Parent` and `Parent_<mesh>` copy the parent's current effect using the same attached-child rules as textures.

Meshes can share one combined draw batch only when effect, texture set, and bone capacity are compatible. Material overrides may therefore change batching and should be measured on representative composed models.

## Disabling layers and meshes

`DisableLayer` accepts hyphen-separated layer indices:

```text
DisableLayer 5-6-7
```

When the link is active, those layer slots are skipped inside the affected model instance.

`DisableMesh` accepts hyphen-separated drawable node names:

```text
DisableMesh Hair-HelmetBase
```

`DisableMesh All` stores a wildcard and disables every mesh in the affected model instance.

Use disables to express mutually exclusive composition, but keep project layer ownership explicit. Cyclic or order-dependent exclusion policy quickly becomes difficult to test.

## Cut volumes

`Cut` removes geometry from selected composed-mesh layers:

```text
Cut CutVolumes/Helmet.obj All HeadVolume - - -
```

The six arguments are:

1. cut-volume `.fbx` / `.obj` path, relative to the declaring file;
2. hyphen-separated target layers, or `All`;
3. hyphen-separated drawable shape names from the cut file, or `All`;
4. first unskin bone, or `-`;
5. second unskin bone, or `-`;
6. unskin shape, `~shape` for reversed behavior, or `-`.

`All` layers expands to every compile-time layer except the currently selected layer. At default-root scope, all layers are included.

`All` shapes selects every drawable shape except the separately named unskin shape.

The current runtime classifies a cut shape by its baked vertex count:

- exactly `36` vertices: axis-aligned box bounds;
- any other count: sphere radius derived from the X extent.

This is an Engine format rule, not a general mesh heuristic. Author dedicated simple cut assets and validate the result visually.

Both unskin bones must be provided together. An unskin shape requires both bones. All referenced bones and drawable shapes are checked during baking.

Applying any cut disables normal culling for the composed model. Treat cuts as a correctness feature with a rendering cost; avoid using detailed production meshes as cut volumes.

## Rendering controls

### Automatic model-sprite layout

`DrawSize` and `ViewSize` are removed legacy directives. `ModelInfoBaker` writes
aggregate, idle-priority view/name, and per-animation bounds to
`ModelAnimationInfo.foinfo` version 2. At runtime the client projects those bounds for
each direction, extends them with enabled child models and layers, and derives the
offscreen frame, visual anchor, lighting envelope, and interaction/view rectangle.

Every non-particle child link serializes a validated root-space AABB; the
default link and particle links carry no geometry payload. The baker includes
disabled meshes, nested descriptions, link transforms, and the parent's sampled
animations when calculating that envelope. Runtime framing unions the active
animation bounds with the selected link envelopes and projects only their
corners—there is no per-frame weighted-vertex sweep. Live particles can still
force bounded expansion and rerender when they exceed the baked geometry
envelope. Authors therefore tune source transforms, animation reach,
attachments, and `Render.ModelProjFactor`, not fixed pixel rectangles inside
`.fo3d`.

Validate every direction and representative animation in a visible client. Use
`Game.DumpAtlases()` or the mapper's **Dump atlases** command when diagnosing clipping,
unexpected empty space, polygon edges, or crop placement. See
[Baking Pipeline](../../explanation/content-pipeline/baking.md#shared-animation-metadata) and
[Frontend and Rendering](../../explanation/rendering/#sprite-and-model-atlas-geometry) for the binary and
runtime contracts.

### Other flags

- `DisableShadow` disables model shadow drawing.
- `DisableAnimationInterpolation` selects nearest-key sampling when the baked runtime rig is loaded.
- `DisableBackwardAnim` selects forward walk/run instead of `WalkBack` / `RunBack` and aligns look direction with movement.
- `RotationBone <bone>` enables the movement overlay controller and directional torso/head rotation around a validated body bone.
- `FastTransitionBone <bone>` resets transition state for a newly attached child using that link bone.

## Animation boundary

The `.fo3d` animation directives are:

```text
Anim <state> <action> <ModelFile|animation-source> <clip|~clip|Base>
AnimSpeed <state> <action> <positive-factor>
AllowAnimationGeometry <external-animation-file>
AnimLayerValue <state> <action> <layer> <value>
StateAnimEqual <from> <to>
ActionAnimEqual <from> <to>
FastTransitionBone <bone>
RotationBone <bone>
DisableAnimationInterpolation
DisableBackwardAnim
```

Use [Model Animation](model-animation.md) for:

- first-entry-wins tuple behavior;
- `ModelFile`, `Base`, and reversed `~clip` lookup;
- source skeleton compatibility, conversion into the required runtime rig, and the temporary external-animation geometry exception;
- one-step aliases;
- effective duration;
- common versus loaded-client lookup;
- animation substitutions and validation.

`AnimLayerValue` applies to the exact requested pair before model composition. Alias resolution belongs to animation lookup; do not assume an alias also rewrites the key used for layer overrides.

`AllowAnimationGeometry` is a narrow migration aid, not a normal asset policy. It names one exact external file selected by `Anim`, resolves from the final concrete `.fo3d`, and is consumed only by baker validation. It is not serialized. Duplicate paths, duplicate resolved targets, non-selected files, and exceptions left behind after geometry removal are hard errors. Repair the source into a geometry-free animation export while preserving required helper/bone hierarchy, then remove the exception in the same asset migration.

3D skeletal animation is separate from the 2D `NextX` / `NextY` contract in [Sprite Root Motion](sprite-root-motion.md).

## Runtime loading and caching

`ModelManager::CreateModel(name)` accepts:

- a baked `.fo3d` description, which creates full model information and composition behavior;
- a baked mesh path, which creates a basic rest-pose hierarchy-backed model without `.fo3d` declarations or an animation controller.

Model descriptions and mesh hierarchies are cached by resource name. Immutable animation clips, remaps, bindings, and canonical skeleton data belong to `ModelInformation`; mutable timelines, sampled poses, matrices, linked children, and procedural transforms belong to each `ModelInstance`. Layer changes reuse active child links by stable baked link id and remove children and particles that no longer match.

Combined mesh generation merges compatible visible meshes until effect, texture, or bone-capacity differences require a new batch. Cuts are applied after parent and child meshes have been combined.

Do not mutate or parse the baked binary `.fo3d`, `.fbx`, or `.obj` payloads from project scripts. Their binary layout is a private baker/runtime contract.

## Failure behavior

`ModelInfoBaker` rejects or reports:

- missing `Model`;
- missing, unreadable, stale, or malformed baked meshes and their source files;
- a primary mesh with no drawable geometry;
- missing default diffuse textures;
- missing explicit textures, effects, particles, child descriptions, or cut files;
- invalid layer or texture indices;
- zero layer values for `Root` / `Attach`;
- missing bones or drawable mesh references;
- malformed include graphs or replacement pairs;
- invalid or non-finite numbers;
- negative link `Speed`;
- non-positive `AnimSpeed`;
- unknown animation enums, missing clips, incompatible source skeletons, or invalid runtime-rig conversion;
- direct attached FBX files with clips, external animation files with unexpected drawable geometry, and duplicate/non-selected/stale `AllowAnimationGeometry` exceptions;
- mirrored mesh nodes and direct FBX/OBJ attachments outside the configured Engine world-unit extent;
- invalid cut layer/shape/unskin combinations;
- unknown tokens.

Runtime loading repeats critical binary and range checks. Runtime exceptions indicate corrupted/stale baked data or a validation gap; do not catch them and substitute an unrelated model as a silent fallback.

## Legacy content warning

Do not infer current support from old FOnline project files. In particular:

- `.x` and `.3ds` are not selected by the current mesh baker;
- `AnimEqual` was replaced by the domain-specific `StateAnimEqual` and `ActionAnimEqual`;
- `CalculateTangentSpace`, `RenderFrame`, and `RenderFrames` are not current tokens;
- `Subset` is accepted only as an obsolete warning path and has no selection effect.

Port legacy assets by first translating them to the current source formats and grammar, then validating the result against the current Engine revision. Legacy project content is evidence of historical usage, not a normative format specification.

## Authoring practices

For maintainable model descriptions:

1. Keep default-root declarations before the first `Layer`.
2. Give every concrete description exactly one intentional final `Model`.
3. Prefix include-only files with `TEMPLATE_`.
4. Make templates establish their own selector context instead of inheriting caller state.
5. Use enum names for state/action and project layer constants where metadata exposes them.
6. Document every project layer index, allowed value, owner, and conflicting layer.
7. Use one drawable node per independently overridden material or effect.
8. Use direct mesh attachments for simple props and child `.fo3d` descriptions for reusable composed objects.
9. Always give particle attachments an explicit `Link`.
10. Use exact case in paths, bone names, mesh names, animation stacks, and effects.
11. Keep cut volumes simple and purpose-built.
12. Exercise the full layer combination matrix, not only each attachment in isolation.
13. Export external animation files without drawable geometry; preserve required helper/bone hierarchy and tracks when repairing older files.
14. Treat every `AllowAnimationGeometry` line as temporary migration debt with a named source-repair owner, then remove it as soon as the export is clean.
15. After source-loader, converter, mesh-wire, or animation-source changes, run a full force bake followed by an incremental bake to prove both conversion and dependency timestamps.

For AI-authored content, record the intended parser state before emitting each modifier:

```text
current layer = 3
current value = 2
current link = Attach Armor.fo3d
current mesh = Torso
next directive = Texture 0 Parent_Torso
```

If that state cannot be stated unambiguously, split the compact line or make the selectors explicit.

## Validation workflow

After changing model assets or descriptions:

1. Regenerate and validate the format reference:

   ```powershell
   python BuildTools\docs_model_format.py --write
   python BuildTools\docs_model_format.py --check
   python -m unittest BuildTools.tests.test_docs_model_format
   ```

2. Run focused Engine model tests:

   ```powershell
   .\Binaries\Tests-Windows-win64\LF_UnitTests.exe "ModelBaker*"
   ```

3. Rebake the embedding project:

   ```powershell
   cmake --build Build\Auto --config RelWithDebInfo --target BakeResources
   ```

4. Launch a visible client scene that covers:

   - automatic framing, view/name anchoring, and atlas crop bounds;
   - every authored layer/value;
   - single-bone and shared-skeleton attachments;
   - particles;
   - texture/effect inheritance;
   - mesh/layer disables;
   - cuts;
   - idle, movement, turn, backward, and action animations;
   - shadows and animation interpolation.

5. Record project-specific layer semantics, expected screenshots, and regression routes in the embedding project's documentation.

A clean bake proves grammar, asset closure, enum/range validity, and baked serialization. It does not prove pose quality, scale, clipping, material appearance, animation blending, cut geometry, interaction bounds, or performance.

## Change routing

When changing:

- `.fo3d` tokens, parser state, include behavior, path rules, or validation: update this guide, `BuildTools/ModelFormatInterface.json`, generated model-format outputs, and focused tests;
- `.fbx` / `.obj` import, skinning, material extraction, animation-stack conversion, or model limits: update the asset and limit contract and run native mesh-baker tests;
- model layers, attachments, particles, transforms, textures, effects, cuts, batching, or rendering flags: update composition/runtime sections and validate a visible client scene;
- animation tuples, aliases, speed, duration, or script lookups: update [Model Animation](model-animation.md);
- 2D frame offsets or walk/run sprite phase: update [Sprite Root Motion](sprite-root-motion.md);
- project model catalogs or layer meanings: update only the embedding-project documentation while linking back to this reusable contract.
