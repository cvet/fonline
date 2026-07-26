# Effect Format And Shader Runtime

FOnline uses `.fofx` files for authored GPU effects. One source combines render
state, one or more vertex/fragment shader passes, Engine-owned shader resources,
and the data needed to bake backend-specific shader artifacts.

Use this guide for authoring and runtime behavior. Use the generated
[effect-format reference](generated/effect-format/index.md), its focused
[render-state](generated/effect-format/render-state.md),
[resource](generated/effect-format/resources.md),
[baking](generated/effect-format/baking.md), and
[runtime](generated/effect-format/runtime.md) pages, plus the
[canonical JSON model](generated/effect-format.json), for the exact
current-revision contract.

## Scope and authority

The owning sources are:

- `Source/Tools/EffectBaker.cpp` for `.fofx` parsing, shader compilation,
  reflection, backend flavors, and diagnostics;
- `Source/Frontend/Rendering.h` and `Source/Frontend/Rendering.cpp` for vertex
  layouts, built-in buffers, pass/render state, and reflected-resource loading;
- `Source/Client/EffectManager.cpp` for path caching, default effects,
  `ScriptValueBuf`, and per-frame buffer updates;
- `Source/Client/Client.cpp` and
  `Source/Scripting/ClientGlobalScriptMethods.cpp` for script-selected effects
  and script-value APIs;
- `Source/Frontend/Rendering-*.cpp` for backend shader loading, pipelines,
  descriptor binding, and drawing;
- `Source/Tests/Test_EffectBaker.cpp` for executable bake and failure examples.

This page is reusable Engine documentation. An embedding project owns its
effect catalog, resource-pack override order, shader quality profile, art
direction, concrete `EffectType` assignments, and every semantic meaning
assigned to `ScriptValueBuf`.

`BuildTools/EffectFormatInterface.json` is the source-backed structured
contract. `BuildTools/docs_effect_format.py` validates its source anchors,
derives compile-limit defaults from the CMake project interface, and renders
the generated reference. Parser, baker, resource, or runtime drift must update
that model in the same change.

## Minimal effect

A one-pass untextured quad effect can be as small as:

```ini
[Effect]

[VertexShader]
layout(set = 0, binding = 0, std140) uniform ProjBuf
{
    mat4 ProjMatrix;
};

layout(location = 0) in vec3 InPosition;

void main(void)
{
    gl_Position = ProjMatrix * vec4(InPosition, 1.0);
}

[FragmentShader]
layout(location = 0) out vec4 FragColor;

void main(void)
{
    FragColor = vec4(1.0);
}
```

The file must have `[Effect]` and usable vertex/fragment source for every
declared pass. The baker supplies the version directive, precision qualifier,
and Engine compile-time defines.

## File structure

`.fofx` uses the Engine `ConfigFile` parser in content-collection mode. Config
keys belong in `[Effect]`; shader section bodies are collected as raw text.

### `[Effect]`

This required section owns:

- `Version`;
- `Passes`;
- `ShadowPass` in 3D builds;
- `BlendFunc`;
- `BlendEquation`;
- `DepthWrite`;
- `DepthFunc`;
- the `_PassN` variants of blend and depth keys.

The runtime reparses this section from the baked copy of the original `.fofx`
file. Render state is therefore not encoded only in shader binaries or
reflection metadata.

### `[ShaderCommon]`

This optional raw-text section is prepended to both stages of every pass. Use
it for constants and helper functions shared by the effect's stages/passes.

There is no `.fofx` include directive. Keep reusable code inside
`[ShaderCommon]`, duplicate deliberately between separate effect resources, or
generate project effects outside the Engine format if the project owns such a
workflow.

### Shader stages and pass fallback

For pass `N`, the baker resolves stages in this order:

1. `[VertexShader PassN]`, then `[VertexShader]`;
2. `[FragmentShader PassN]`, then `[FragmentShader]`.

An empty pass-specific section is treated as absent and falls back to the
generic section. If neither source exists for a stage, baking fails.

Pass numbering is one-based. Keep the spelling distinction clear:

- shader section: `[FragmentShader Pass2]`;
- state key: `BlendFunc_Pass2`.

## Render state

### Pass count and shader version

`Passes` defaults to `1` and must be in
`1..FO_EFFECT_MAX_PASSES` (Engine default `6`). The value controls every stage
compile, metadata artifact, and backend pass object.

`Version` defaults to `310`. The baker emits:

```glsl
#version 310 es
precision highp float;
```

Do not write a second `#version` directive in authored shader text.

### Blend state

`BlendFunc` defaults to:

```ini
BlendFunc = SrcAlpha InvSrcAlpha
```

It must contain exactly two factors, source first:

`Zero`, `One`, `SrcColor`, `InvSrcColor`, `DstColor`, `InvDstColor`,
`SrcAlpha`, `InvSrcAlpha`, `DstAlpha`, `InvDstAlpha`, `ConstantColor`,
`InvConstantColor`, or `SrcAlphaSaturate`.

`BlendEquation` defaults to `FuncAdd`. Accepted values are `FuncAdd`,
`FuncSubtract`, `FuncReverseSubtract`, `Max`, and `Min`.

Use `BlendFunc_PassN` and `BlendEquation_PassN` for pass-local overrides.
Unknown values fail effect construction.

### Depth state

`DepthWrite` defaults to `True`. `DepthFunc` defaults to `Always`; accepted
comparisons are `Always`, `Never`, `Less`, `LessEqual`, `Equal`,
`GreaterEqual`, `Greater`, and `NotEqual`.

Use `DepthWrite_PassN` and `DepthFunc_PassN` for pass-local overrides.

Depth state is active for `EffectUsage::QuadSprite` and, in 3D builds,
`EffectUsage::Model` when the target has a depth attachment. UI, primitives,
lights, and final blits may draw to targets where depth state has no effect.
See [FrontendAndRendering.md](FrontendAndRendering.md) for map depth ordering
and backend-specific rendering behavior.

### Shadow pass

In a 3D build, `ShadowPass` defaults to `-1`. Set it to a one-based pass index
to mark that pass as the model shadow pass. The index is validated against the
compiled pass limit. Runtime model drawing can disable marked shadow passes
without disabling the other passes.

## Shader compiler environment

Each pass is parsed and linked through glslang as GLSL for Vulkan 1.0 and
SPIR-V 1.0. Before `[ShaderCommon]` and the stage body, the baker supplies:

```glsl
#version <Version> es
precision highp float;
#define MAX_SCRIPT_VALUES <FO_EFFECT_SCRIPT_VALUES>
```

When `FO_ENABLE_3D` is active, it also supplies:

```glsl
#define MAX_BONES <FO_MODEL_MAX_BONES>
#define MAX_TEXTURES <FO_MODEL_MAX_TEXTURES>
```

The linked program must build reflection successfully. Vertex outputs and
fragment inputs therefore need compatible locations/types even if one backend
would otherwise accept looser source.

## Vertex input contract

Effect usage is fixed when a path is first loaded. The shader's input
locations must match that usage.

### ImGui, QuadSprite, and Primitive

These usages share `Vertex2D`:

| Location | GLSL type | Native field | Meaning |
|---|---|---|---|
| `0` | `vec3` | `PosX`, `PosY`, `PosZ` | position/depth |
| `1` | `vec4` | `Color` | normalized vertex color |
| `2` | `vec2` | `TexU`, `TexV` | texture coordinate |
| `3` | `vec2` | `EggFlags` | egg or draw-path auxiliary data |

A shader may omit unused inputs. Keep declared locations/types compatible with
the table.

### Model

`EffectUsage::Model` uses `Vertex3D`:

| Location | GLSL type | Meaning |
|---|---|---|
| `0` | `vec3` | position |
| `1` | `vec3` | normal |
| `2` | `vec2` | primary texture coordinate |
| `3` | `vec2` | base/secondary texture coordinate |
| `4` | `vec3` | tangent |
| `5` | `vec3` | bitangent |
| `6` | `vec4` | blend weights |
| `7` | `vec4` | blend indices |
| `8` | `vec4` | normalized vertex color |

Model effects exist only in 3D builds. Keep
`FO_MODEL_BONES_PER_VERTEX = 4`; active backend layouts assert that shape.

## Descriptor and binding contract

### Native convention

Author resources for the native Vulkan convention:

- descriptor set `0`: uniform buffers;
- descriptor set `1`: combined image samplers.

Bindings are explicit integers. Bindings need not be dense for the native
path, but they must be unique within one shader stage and resource class.

The EffectBaker tests contain sources that omit `set = ...`; glslang can still
compile those fixtures. Production effects should state sets explicitly. The
native Vulkan backend consumes the original SPIR-V and does not repair an
incorrect authored descriptor set.

### SDL_GPU remap

The baker makes a copy of native SPIR-V and rewrites descriptor decorations to:

| Stage/resource | Descriptor set |
|---|---:|
| vertex samplers | `0` |
| vertex uniform buffers | `1` |
| fragment samplers | `2` |
| fragment uniform buffers | `3` |

Within each class/stage, authored bindings are sorted and rewritten to dense
slots `0..N-1`. Each stage is limited to `16` samplers and `4` uniform buffers.
The remapped module is emitted as `spv_sdl`; the SDL Metal source is also
compiled from this remapped module.

Missing bindings, duplicate same-stage bindings, storage images, and dead
descriptor declarations fail the bake because the remap cannot be made
complete and deterministic.

## Engine-provided textures

The runtime recognizes these sampler names:

| Sampler | Availability | Producer |
|---|---|---|
| `MainTex` | all builds | current sprite/render-target/model primary texture |
| `IndoorMaskTex` | client map paths | current indoor mask |
| `ModelTex0..ModelTexN-1` | 3D builds | model texture slots |

Unknown sampler names are reflected, but `RenderEffect` has no Engine producer
for them. A source can therefore compile while the runtime leaves the sampler
unbound. Treat the table as the authoring allowlist unless a renderer/runtime
change adds a producer in the same change.

## Built-in uniform buffers

Uniform block names and byte layouts are fixed. EffectBaker compares reflected
sizes to the native structures and rejects unknown blocks.

### General buffers

| Block | GLSL shape | Producer/meaning |
|---|---|---|
| `ProjBuf` | `mat4 ProjMatrix` | current 2D or 3D projection |
| `MainTexBuf` | `vec4 MainTexSize` | width, height, reciprocal width/height |
| `EggBuf` | `vec4 EggData[3]` | two egg masks plus transition parameter |
| `SpriteBorderBuf` | `vec4 SpriteBorder` | sprite atlas UV rectangle |
| `TimeBuf` | `vec4 FrameTime; vec4 GameTime` | `.x` seconds, session-relative, wrapped at `8192` |
| `RandomValueBuf` | `vec4 RandomValue` | four per-frame random values in `[0,1]` |
| `ScriptValueBuf` | `vec4 ScriptValue[MAX_SCRIPT_VALUES / 4]` | project-controlled float slots |
| `CameraBuf` | `vec4 MapAnchorScreenPos; vec4 ChunkScreenAnchor` | world/screen affine UV bases |

For `CameraBuf`, evaluate either basis as:

```glsl
vec2 uv = Basis.xy + TexCoord * Basis.zw;
```

`MapAnchorScreenPos` is world anchored and zoom invariant.
`ChunkScreenAnchor` is screen anchored. Do not reduce either to a subtraction;
the scale terms account for padded/chunked render targets.

### Model buffers

| Block | GLSL shape | Meaning |
|---|---|---|
| `ModelBuf` | `vec4 LightColor; vec4 GroundPosition; mat4 WorldMatrices[MAX_BONES]` | lighting, ground anchor, skin matrices |
| `ModelTexBuf` | `vec4 TexAtlasOffset[MAX_TEXTURES]; vec4 TexSize[MAX_TEXTURES]` | atlas transforms and texture dimensions |
| `ModelAnimBuf` | `vec4 AnimNormalizedTime; vec4 AnimAbsoluteTime` | normalized and looped absolute animation time |

Storage buffers and storage images are unsupported.

## ScriptValueBuf ownership and lifetime

`FO_EFFECT_SCRIPT_VALUES` defaults to `16`; an embedding project may override
it. The value must be positive and divisible by four. It is a compiled Engine
shape and must match the generated/baked shader define.

The runtime behavior is:

1. The first load of an effect that declares `ScriptValueBuf` creates a zeroed
   buffer.
2. Script writes mutate the cached `RenderEffect` object.
3. Values remain until overwritten or explicitly cleared.
4. `Game.SetEffect(...)` changes the selected object but does not clear either
   object's values.
5. Returning to a previously loaded path returns its previous buffer contents.
6. `Game.ClearEffectScriptValues(...)` zeroes the selected object's whole
   buffer.

The cache key is the resource path only. If the same path is selected in
multiple compatible slots, they share one buffer. Projects should assign each
slot range one owner and document collisions. Re-pushing values after a
variant swap is still a good project practice when variants use different
paths or buffer declarations, but it is not a reset guarantee from
`SetEffect`. SetEffect does not reset cached values.

## Runtime loading and cache identity

`EffectManager::LoadEffect(usage, path)` returns an existing cached object when
the path was loaded before. The cache key does not include `EffectUsage`.
Therefore the first load fixes the object's usage and backend pipeline/input
layout assumptions.

Do not reuse one path across incompatible categories:

- `ImGui`;
- `QuadSprite`;
- `Primitive`;
- `Model`.

Separate resources may contain identical shader text when they need different
usages. Path clarity is more important than avoiding a tiny source duplicate.

The runtime loads:

- the baked `.fofx` source for pass count and render state;
- `.fofx-N-info` for reflected native and SDL resource slots;
- the backend flavor required by the active renderer.

Missing source, metadata, or shader flavor is a load error.

## Script API

The client/mapper script surface is:

```angelscript
Game.SetEffect(effectType, effectSubtype, effectPath);
Game.SetEffectScriptValue(effectType, effectSubtype, valueIndex, value);
Game.SetEffectScriptValues(
    effectType,
    effectSubtype,
    valueStartIndex,
    values,
    valuesOffset = 0,
    valuesCount = -1);
Game.ClearEffectScriptValues(effectType, effectSubtype);
```

`SetEffect` uses an empty path to restore the slot's default. A non-empty path
loads using the default slot's usage.

Script-value calls resolve the currently selected target and fail when:

- the type/subtype is unsupported or invalid;
- the target entity/offscreen slot does not exist;
- the effect is not loaded;
- the effect does not declare `ScriptValueBuf`;
- an input or destination range is out of bounds.

Use the ranged method for parameter blocks updated together. It validates
`valuesOffset`, derives the remaining count when `valuesCount = -1`, and makes
one native write.

Per-font ScriptValue writes are not supported; `EffectType::Font` accepts only
subtype `-1` for the shared font effect. `GenericSprite` and `CritterSprite`
can target the shared slot with subtype `0` or a live entity by id. Offscreen
subtypes must have been registered and loaded.

## Baking outputs

For each pass and stage, EffectBaker emits:

| Flavor | Consumer |
|---|---|
| `spv` | native Vulkan; source for GLSL/ES/HLSL cross-compilation |
| `spv_sdl` | SDL_GPU Vulkan path |
| `glsl` | OpenGL desktop (`330`) |
| `glsl_es` | OpenGL ES/WebGL (`300 es`) |
| `hlsl` | Direct3D (Shader Model `4.0`) |
| `msl_mac` | SDL_GPU Metal on macOS |
| `msl_ios` | SDL_GPU Metal on iOS |

Naming is:

```text
<path>.fofx-<pass>-<vert|frag>-<flavor>
<path>.fofx-<pass>-info
```

`[EffectInfo]` stores program-wide reflected bindings and proves built-in
uniform-buffer sizes. `[EffectInfoSdl]` stores stage-local dense slots and
sampler/UBO counts. The original source is copied to the baked resource path.

## Resource packs and overrides

The Engine provides minimal-profile effects in `Resources/Core/Effects/` and
bootstrap effects in `Resources/Embedded/Effects/`. An embedding project may
provide a later resource with the same path to shadow an Engine default.

Keep reusable Engine defaults conservative. Richer project copies may target a
higher hardware profile, but the project must validate every shipped backend
and preserve a deliberate fallback. See
[FrontendAndRendering.md](FrontendAndRendering.md) for the current default
slot map and minimal-profile policy.

## Authoring practices

- Start from the closest Engine effect with the same `EffectUsage`, vertex
  inputs, and built-in buffers.
- Declare descriptor sets and bindings explicitly, even when a test fixture
  demonstrates that glslang can infer a set.
- Keep pass count small. Every pass multiplies stage compilation, metadata,
  backend objects, and draw work.
- Remove unused samplers and uniform blocks. Dead descriptor declarations are
  rejected for SDL remapping.
- Use only recognized buffer names and copy their layouts exactly.
- Keep `ScriptValueBuf` slots centralized in project code/docs, with one owner
  per range and stable meanings across shader variants.
- Prefer `SetEffectScriptValues` for contiguous snapshots and explicit
  `ClearEffectScriptValues` when reset semantics matter.
- Do not use the same path for incompatible effect usages.
- Keep animation time periodic. `TimeBuf` wraps at `8192` seconds, and
  script-maintained float clocks should wrap before precision degrades.
- Test depth and blending on a target that actually has the relevant
  attachments and draw order.
- Validate the lowest hardware/profile the project claims to support; a
  successful cross-compile is not a visual or driver guarantee.

## Failure guide

| Symptom | Likely boundary |
|---|---|
| missing Effect/vertex/fragment error | section spelling, `Passes`, or fallback source |
| shader compiler diagnostic | GLSL syntax, version/profile, or stage interface |
| invalid uniform buffer size | block field order/type/array length differs from `RenderEffect` |
| invalid uniform buffer | unknown block name |
| explicit/duplicate binding error | missing or colliding stage-local binding |
| unused resource error | declared sampler/UBO is optimized out or never read |
| SDL stage-limit error | more than 16 samplers or 4 UBOs in one stage |
| effect loads but texture is empty | sampler name has no Engine producer or wrong set/binding |
| script-value write throws | wrong target, unloaded effect, no `ScriptValueBuf`, or bad range |
| script value appears in another slot | both slots resolve to the same cached path |
| switching back restores old tuning | expected path-cache persistence; clear or overwrite explicitly |
| works on one renderer only | backend flavor/profile/descriptor/depth difference |

## Validation workflow

After changing an Engine effect-format source, buffer, backend, or built-in
effect:

```powershell
python BuildTools\docs_effect_format.py --write
python BuildTools\tests\test_docs_effect_format.py
python BuildTools\docs_effect_format.py --check
python BuildTools\docs_contract_diff.py --help
```

Run the focused native baker tests through the configured embedding-project
unit-test target. `Source/Tests/Test_EffectBaker.cpp` is the owning test file.
Renderer state changes also need the relevant backend and visible scene.

For an embedding project:

1. regenerate/configure when a compile limit changed;
2. bake resources;
3. run project validators for effect paths, `EffectType` assignments, and
   ScriptValue ownership;
4. launch representative visible scenes for every affected slot;
5. test every shipped renderer/backend and minimum hardware profile;
6. compare default/fallback and overridden project effects.

## Change checklist

When the Engine contract changes, update together:

- `BuildTools/EffectFormatInterface.json`;
- `Docs/EffectFormat.md`;
- generated `Docs/generated/effect-format.json` and reference pages;
- `BuildTools/tests/test_docs_effect_format.py`;
- `Source/Tests/Test_EffectBaker.cpp` or the owning renderer/runtime test;
- [BakingPipeline.md](BakingPipeline.md) when output/compiler behavior changes;
- [FrontendAndRendering.md](FrontendAndRendering.md) when runtime/backend
  behavior changes;
- [GeneratedApiAndMetadata.md](GeneratedApiAndMetadata.md) and
  [ApiChangeManagement.md](ApiChangeManagement.md) when the structured contract
  or aggregate diff surface changes;
- embedding-project docs/tests for paths, slot semantics, fallbacks, and visual
  validation.
