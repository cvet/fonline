# Image And Sprite Formats

FOnline bakes authored and legacy images into a compact client sprite container.
The baker can import static RGBA sources, legacy indexed formats, animations,
direction sheets, and text `.fofrm` compositions. It can also replace the
ordinary full-frame quad with a validated indexed silhouette and crop the
serialized RGBA canvas to that geometry. At runtime the stock client reads the
versioned container, places concrete frames in a texture atlas, and creates either
an `AtlasSprite` or a `SpriteSheet`.

Use this guide for authoring decisions and operational behavior. Use the
generated [image-format reference](generated/image-format/index.md), its
[source-format](generated/image-format/formats.md),
[FOFRM](generated/image-format/fofrm.md),
[filename-option](generated/image-format/options.md),
[baking](generated/image-format/baking.md),
[runtime](generated/image-format/runtime.md), and
[validation](generated/image-format/validation.md) pages, plus the
[canonical JSON model](generated/image-format.json), for the exact
current-revision contract.

## Scope and authority

The owning sources are:

- `Source/Tools/ImageBaker.cpp` and `ImageBaker.h` for source discovery,
  format decoding, FOFRM composition, output naming, per-pack `SpriteInfo`,
  mesh integration, and serialization;
- `Source/Tools/SpriteMeshing.cpp` and `SpriteMeshing.h` for mask construction,
  candidate generation, scoring, triangulation, and coverage validation;
- `Source/Common/SpriteResource.cpp` and `SpriteResource.h` for the versioned
  shared decoder, frame/mesh records, and `SpriteInfo` index format;
- `Source/Client/DefaultSprites.cpp` and `DefaultSprites.h` for baked-container
  loading, `AtlasSprite`, `SpriteSheet`, frame offsets, and atlas upload;
- `Source/Client/SpriteManager.cpp` for extension dispatch and sprite caches;
- `Source/Client/TextureAtlas.cpp` for atlas allocation;
- `Source/Tests/Test_ImageBaker.cpp` and `Test_TextureAtlas.cpp` for executable
  import, failure, and allocation examples.

`BuildTools/ImageFormatInterface.json` is the source-backed structured
contract. `BuildTools/docs_image_format.py` validates every declared source
anchor, derives the live baker and runtime extension lists, verifies that the
legacy FOFRM `Effect` field is still absent from serialized output, and renders
the generated reference. Importer, descriptor, container, factory, atlas, or
cache drift must update that model in the same change.

This page is reusable Engine documentation. An embedding project owns its
concrete asset catalog, licenses, resource-pack precedence, source-file policy,
visual style, animation substitutions, hit-test expectations, movement tuning,
and visible acceptance baselines.

## Choose a source format

| Need | Preferred source | Notes |
|---|---|---|
| Ordinary static image or transparent sprite | PNG | Recommended project-authored lossless input. Palette, low-bit grayscale, `tRNS`, and 16-bit channels are normalized to RGBA8. |
| Existing TrueColor pipeline output | TGA | Use the supported type 2/type 10, 24/32-bpp, no-image-ID, bottom-left-origin subset. |
| Multi-frame or directional project sprite | FOFRM referencing PNG/TGA | Keeps composition, offsets, frame deltas, and timing explicit and reviewable. |
| Existing Fallout FRM/FR0 asset | FRM or FR0 | Preserve only when the project can redistribute the source and has a visual regression route. |
| Existing Tactics ART/SPR asset | FOFRM referencing ART/SPR | Filename options select palettes, frames, mirrors, colors, and sequences. SPR should normally be wrapped because of the default runtime boundary. |
| Existing Infinity Engine asset | ZAR/TIL/MOS/BAM, normally through FOFRM when selection is needed | Treat as an import path, not a recommended format for new art. |

The built-in image baker does not support JPEG, BMP, GIF, DDS, WebP, AVIF, or
SVG. Convert such inputs to PNG or the supported TGA subset before baking. Do
not infer support from a renderer or third-party library that happens to know a
format; `ImageBaker` and the selected runtime `SpriteFactory` are the contract.

## Pipeline overview

The normal path is:

1. Resource packs expose source files through `FileCollection`.
2. `ImageBaker` scans its registered extensions or receives one target path.
3. The selected loader returns a `FrameCollection` containing one common frame
   count/timing value and either one sequence or a full direction set.
4. `BakeCollection` optionally builds and scores a silhouette mesh for each
   unique frame, pads or crops the RGBA canvas, and resolves its logical root.
5. It writes the private RGBA/mesh frame container under the source path or a
   loader-provided `NewName`, and maintains `SpriteInfo/<PackName>.foinfo`.
6. At client runtime, `SpriteManager` selects a factory from the lowercased
   extension and `DefaultSpriteFactory` reads the baked bytes.
7. Concrete frames enter the requested texture atlas; animation/direction
   metadata becomes a `SpriteSheet` when needed.

Source decoders do not run in the stock client. A file named `Sprite.png` in a
baked resource pack contains the Engine sprite container, not original PNG
bytes. The retained extension is dispatch identity, not a promise about the
baked payload format.

## FOFRM grammar

FOFRM uses the Engine `ConfigFile` parser. The root/default section describes a
single-direction sequence. Directional sheets use `[dir_N]` or `[Dir_N]`
sections.

### Minimal static image

```ini
count = 1
fps = 10
frm = Icon.png
```

The unnumbered `frm`/`Frm` alias is accepted only for reference zero. Numbered
keys are clearer and scale to animations:

```ini
fps = 8
count = 3
frm_0 = Idle_00.png
frm_1 = Idle_01.png
frm_2 = Idle_02.png
```

References resolve relative to the `.fofrm` directory. Keep related source
frames beside the descriptor or in a stable relative subtree; do not rely on a
developer machine's current directory.

### Sequence placement and frame deltas

The root or each direction can set signed placement offsets with either naming
style:

```ini
offs_x = -24
offs_y = -63
```

or:

```ini
OffsetX = -24
OffsetY = -63
```

Direction offsets inherit the values held while the previous direction was
parsed when omitted. This can be useful for legacy data, but it is easy to
misread. Production directional descriptors should write both offsets in every
direction section.

Per-reference deltas use `next_x_N` / `next_y_N` or `NextX_N` / `NextY_N`:

```ini
next_x_0 = 2
next_y_0 = -1
```

The descriptor delta is added to each imported child frame's own `NextX` and
`NextY`. These values are not ordinary image placement offsets. Multi-frame
runtime sheets retain them as per-frame displacement consumed by specialized
presentation code. The detailed walk/run projection and authoritative-movement
boundary are documented in [SpriteRootMotion.md](SpriteRootMotion.md).

### Direction sheets

A directional descriptor must contain either one sequence or every direction
from zero through `GameSettings::MAP_DIR_COUNT - 1`:

```ini
fps = 10
count = 2

[dir_0]
offs_x = -24
offs_y = -63
frm_0 = Walk_NE_00.png
frm_1 = Walk_NE_01.png

[dir_1]
offs_x = -24
offs_y = -63
frm_0 = Walk_E_00.png
frm_1 = Walk_E_01.png

# Continue every configured map direction.
```

Every direction must flatten to the same final frame count. A partial direction
set, a missing later section, or a different flattened count fails baking.

### Nested references and flattening

Each `frm_N` may reference any registered image-loader extension and may include
`$` filename options. If a child has several frames, FOFRM appends the child's
`Main` sequence to the parent. It does not compose the child's direction sheets
or child sequence-level `OffsX`/`OffsY`. A nested child frame that is already a
shared record is rejected because its index is not rebased during flattening.

The distinction between descriptor count and flattened frame count matters:

```ini
fps = 10
count = 2
frm_0 = FirstCycle.bam
frm_1 = SecondCycle.bam
```

FOFRM computes whole duration as `1000 * count / fps`, while the runtime divides
that duration by the flattened frame count. If each BAM reference contributes
several frames, playback is faster than an author may expect. For predictable
cadence, reference one static frame per descriptor slot or calculate timing from
the actual flattened count and verify it in a visible client.

`fps = 0` creates zero whole ticks and deliberately disables normal playback.
For a playing multi-frame sheet, keep integer `AnimTicks / frame_count` at least
one millisecond; the runtime update loop divides by that value.

### Legacy `Effect` key

The parser accepts `effect` and `Effect` into
`FrameCollection::EffectName`, but the baker does not serialize or apply it and
the stock runtime does not select a shader from it. Treat the key as ignored
compatibility input. Select project effects through the owning renderer,
prototype, GUI, or script surface documented by [EffectFormat.md](EffectFormat.md).

## Legacy filename options

Options appear after `$` and before the extension. `LoadAny` strips them from
the physical lookup path and passes them to the selected loader.

### ART

```text
Actor$1THF5-7.art
```

- `0` through `3` select a palette; the last selector wins and an unavailable
  palette falls back to palette zero.
- `T` derives alpha from the maximum RGB component; palette index zero remains
  fully transparent.
- `H` and `V` mirror horizontally and vertically.
- `F5` selects frame 5; `F5-7` and `F7-5` select inclusive ascending or
  descending ranges. Bounds clamp to the available frame table.
- Option letters are case-insensitive. Unknown characters are ignored.

ART's static flag forces one rotation. Eight-rotation input is remapped to the
current map geometry and becomes a complete Engine direction sheet.

### SPR

```text
Actor$[1,12,0,0][2,0,-8,4]Walk.spr
```

Parts are `0` other, `1` skin, `2` hair, and `3` armor. RGB offsets are added
and clamped to `0..255`. An out-of-range part value applies its RGB values to
all parts. Text after the final bracket selects a sequence case-insensitively;
an empty name selects the first sequence.

SPR imports layered pixels, removes invalid sequence frame indices, and emits
shared records for repeated indices. Its cadence is fixed at 10 fps. The stock
`DefaultSpriteFactory` does not register `.spr`, even though `ImageBaker` can
bake it. Normally reference SPR from a `.fofrm`, so the composed baked output
has the registered `.fofrm` extension. Direct `.spr` runtime paths require an
explicit custom sprite factory.

### BAM

```text
Spell$1.bam
Spell$1-3.bam
```

The first integer selects a cycle. An optional integer after `-` selects one
frame. Out-of-range cycle or frame values fall back to zero. Omitting the frame
selector imports the entire cycle. Negative selected-frame values are treated
as the whole-cycle form.

## Source-format details

### PNG

PNG is the default recommendation for project-authored images. The loader:

- strips 16-bit channels to 8-bit;
- expands low-bit grayscale and palette pixels;
- expands `tRNS` transparency;
- fills missing alpha with 255;
- emits one RGBA8 frame.

Corrupt input fails through the libpng error callback. Keep source color-space
and premultiplication policy explicit in the embedding project; ImageBaker does
not provide a project color-management workflow.

### TGA

The supported production subset is intentionally narrow:

- TrueColor type 2 (raw) or type 10 (RLE);
- 24-bit BGR or 32-bit BGRA pixels;
- no image ID;
- bottom-left origin, because the implementation always flips rows.

Indexed, grayscale, and other bit depths fail. The loader does not honor the
descriptor origin bit or skip an image ID, so exporting top-origin or ID-bearing
files can produce incorrect or rejected output. Prefer PNG unless an existing
toolchain has a tested TGA preset.

### Fallout FRM and FR0

FRM reads big-endian frame rate/count, sequence offsets, frame deltas, and one
or complete direction tables. A same-basename `.pal` overrides the built-in
Fallout palette. With the default palette, animated palette indices can expand
one source into a generated color cycle; verify final frame count and cadence.

FR0 is the entry point for split `fr0`, `fr1`, and later direction siblings.
Once multiple directions begin, a missing later file is an error. Critter paths
normalize to lowercase `.fofrm`; other split sets normalize to `.frm`.

### RIX, ZAR, TIL, MOS, and BAM

- RIX emits one opaque embedded-palette frame.
- ZAR emits one palette-backed raw/RLE frame with alpha.
- TIL imports nested ZAR frames as a 10 fps sequence.
- MOS/MOSC imports one tiled image, decompressing MOSC first; palette green
  `0x00FF00` is transparent.
- BAM/BAMC imports a selected cycle/frame, decompresses BAMC, supports RLE,
  derives frame deltas, and treats palette blue 255 as transparent.

These are compatibility importers. Do not choose them for new project art when
PNG plus FOFRM expresses the same authored intent more clearly.

## Baked container boundary

The baked byte stream is a private agreement between `ImageBaker` and
`DefaultSpriteFactory`, not a public project serialization format. Conceptually
it contains:

1. `SPRITE_RESOURCE_MAGIC` (`43`) and `SPRITE_RESOURCE_VERSION` (`2`);
2. little-endian `uint16` frame count and whole animation ticks;
3. `uint8` direction count (`1` or `GameSettings::MAP_DIR_COUNT`);
4. for every frame in every direction, a shared flag;
5. for a concrete frame, signed `int16` draw offset, `uint16` cropped
   width/height, signed `int16` `NextX`/`NextY`, and exactly
   `width * height * 4` RGBA bytes;
6. a `SpriteMeshKind`; a mesh record additionally stores vertex/index counts,
   logical source size and cropped origin, fixed-width local vertices, and
   triangle indices;
7. for a shared frame, a `uint16` earlier-frame index;
8. trailing `SPRITE_RESOURCE_MAGIC`.

Do not parse or generate this stream in project scripts or external content
tools. Feed supported source files through the pinned Engine baker. A container
layout change may be coordinated inside one Engine revision without preserving
cross-revision byte compatibility.

## Runtime loading, atlas, and caches

`SpriteManager` lowercases the path extension and selects a registered
`SpriteFactory`. The default factory delegates the complete byte span to
`ReadSpriteResource`, which validates magic, version, records, mesh geometry,
footer, and trailing data. The factory then validates the one/full-direction
invariant.

A one-frame, one-direction resource becomes `AtlasSprite`. Its sequence
`OffsX`/`OffsY` becomes the sprite placement offset; serialized `NextX`/`NextY`
is read but ignored. A multi-frame or directional resource becomes
`SpriteSheet`. Each direction owns a parallel sheet and each concrete or shared
frame retains its separate displacement in `_sprOffset`.

`SpriteSheet` can select a direction, randomize the initial frame with
`Prewarm`, set normalized time, and play looped or reversed. `Play` does nothing
for one frame or zero whole ticks. Direction changes do not themselves rewrite
the common animation clock.

Concrete RGBA frames are allocated in the requested `AtlasType`. The factory
requires positive dimensions, uploads the image, duplicates one-pixel edges for
linear filtering, and creates a hit mask from alpha through
`Settings.SpriteHitValue`. Atlas type is also part of the copyable-sprite cache
key, so the same path can have separate interface/map/one-image instances.

With `SpriteMesh.Enabled`, `ImageBaker` treats `alpha > AlphaThreshold` as the
visible mask, searches candidates up to `MaxTriangles`, and scores saved source
frame area against submitted triangles using `AreaSavingsWeight`. Every selected
mesh must cover all visible pixel cells. Invalid or unprofitable candidates keep
the regular quad; an empty mask records explicit empty geometry. Mesh frames may
use a cropped or slightly padded texture canvas, but their serialized offset,
logical source size, and source origin preserve placement, scaling, map-light
interpolation, and hit-test coordinates.

The baker validates the complete `SpriteMesh.*` group even when mesh generation
is disabled. Standalone project configs must therefore declare a positive
`MaxTriangles`, an `AlphaThreshold` in `0..254`, and a finite non-negative
`AreaSavingsWeight`; [the minimal project config](../Examples/MinimalProject/FOnlineStarter.fomain)
shows the explicit disabled defaults.

Ordinary full-image sprite draws submit the baked indexed triangle list. Region
crops, tiled patterns, padded custom-effect or outline draws, fonts, blits,
runtime model sprites, and particles continue to use their rectangular paths.
Use [FrontendAndRendering.md](FrontendAndRendering.md#sprite-and-model-atlas-geometry)
for exact draw and atlas-dump behavior. Use
[BakingPipeline.md](BakingPipeline.md#image-and-sprite-mesh-statistics) for
candidate policy and baking-report fields.

`SpriteInfo/<PackName>.foinfo` version 1 is a compact per-pack index of duration,
directions, frame bounds, offsets, and shared references. Common
`EngineMetadata` loads it on server and client without decoding RGBA payloads.
Adding or losing that aggregate index is a full-rebake condition.

Copyable cache entries store a prototype and return `MakeCopy()` so callers do
not share animation state. Missing files, missing extensions, unknown factories,
and load failures are separately memoized by path in `_nonFoundSprites`.
`CleanupSpriteCache()` does not clear that set. After adding a resource that the
running client already failed to load, recreate the sprite manager or restart
the client before testing the same path again.

## Authoring practices

For new game content:

1. Prefer PNG for source pixels and FOFRM for animation/direction composition.
2. Use lowercase extensions and stable case-correct paths even though extension
   dispatch itself is lowercased.
3. Keep one semantic animation per descriptor. Avoid deeply nested animated
   references because count, flattening, and timing become harder to review.
4. Write `fps`, `count`, every `frm_N`, and both offsets explicitly. In a
   directional file, write offsets in every direction.
5. Keep all direction sections complete and frame counts symmetrical.
6. Treat `NextX`/`NextY` as presentation data. Validate root motion visually
   and never use it as authoritative movement or collision state.
7. Wrap SPR imports in FOFRM unless the embedding project intentionally owns a
   custom factory.
8. Convert unsupported or ambiguous TGA exports to PNG.
9. Keep original legacy assets and custom palettes only when licensing permits
   redistribution; document provenance in the embedding project.
10. Review dimensions, transparent borders, atlas filtering, click/hit masks,
    direction mapping, cadence, and stop/start behavior in a visible client.
11. After changing `SpriteMesh.*`, container code, or mesh policy, run
    `ForceBakeResources`; source timestamps cannot prove that existing outputs
    used the same settings. Inspect `BakingReport.json` and `Game.DumpAtlases()`
    before accepting triangle cost, padding, crop, or fallback changes.

An AI author should prefer explicit FOFRM keys and one-frame PNG references.
Before changing an existing legacy option string, inspect the generated option
reference and the focused native fixtures rather than guessing from filenames.

## Diagnostics

| Symptom | Likely boundary | First checks |
|---|---|---|
| Targeted bake writes nothing and reports no error | Unsupported/missing target or `BakeChecker` skip | Confirm extension, source pack, path case, timestamp/cache policy, and whether a full scan sees the file. |
| `Image file not found` | FOFRM relative reference | Resolve from the descriptor directory and remove `$...` before checking the physical path. |
| `FOFRM file invalid data` | Missing slot, unequal directions, empty child, or partial direction set | Compare `count`, every `frm_N`, all direction sections, and flattened child counts. |
| `FOFRM file invalid data (shared index)` | Nested child already uses shared records | Flatten from concrete sources or remove the nested animation layer. |
| Direct `.spr` path reports unknown extension | Stock runtime factory boundary | Load the baked `.fofrm` wrapper or register a project-owned custom factory. |
| Sprite remains missing after the file is added | `_nonFoundSprites` memoization | Restart/recreate the client sprite manager and confirm baked output exists. |
| Animation does not play | One frame, zero ticks, or invalid effective cadence | Inspect flattened frame count and `AnimTicks`; calculate integer ticks per frame. |
| Direction changes show wrong framing | Inherited/missing offsets or source-direction remap | Make offsets explicit in every section and inspect all directions visibly. |
| TGA is flipped or shifted | Unsupported origin/image-ID assumptions | Re-export with the supported preset or convert to PNG. |
| Alpha fringe or incorrect hit area | Source alpha, atlas border, or `SpriteHitValue` | Inspect raw alpha and the final atlas-backed sprite in the intended client profile. |

During a scan, independent image failures are logged as `Image baking error` and
the baker throws one aggregate `Errors during images baking` exception after all
selected work completes. Fix every underlying file; the aggregate count is not
the root cause.

## Validation workflow

For documentation/model changes:

```powershell
python BuildTools\docs_image_format.py --write
python BuildTools\docs_image_format.py --check
python -m unittest BuildTools.tests.test_docs_image_format
python BuildTools\docs_contract_diff.py --help
python BuildTools\docs_validate.py
```

For importer, container, atlas, or playback changes, run the focused native
image-baker and texture-atlas coverage through the configured `RunUnitTests`
target. `Test_ImageBaker.cpp` covers targeted/scan baking, BakeChecker behavior,
PNG/TGA, every legacy importer, options, FOFRM flattening/directions, shared
records, output renaming, and malformed inputs. `Test_TextureAtlas.cpp` covers
allocator split/search/free behavior.

Then validate an affected embedding project:

1. regenerate the image-format model/reference and review the aggregate
   `image-format` contract diff;
2. rebake affected resources with the project's pinned Engine revision;
3. run the narrow project resource/prototype checks that resolve those paths;
4. launch a visible client scene that exercises every changed image, animation,
   direction, mirror, alpha edge, hit area, and supported renderer/profile;
5. for locomotion offsets, also follow
   [SpriteRootMotion.md](SpriteRootMotion.md) and inspect straight movement,
   turns, direction changes, and stop/start transitions.

Native tests prove decoding and container invariants. They do not prove project
art framing, resource-pack precedence, visual cadence, filtering, clickability,
or perceived foot sliding.

## Change checklist

When changing an image surface:

- update `BuildTools/ImageFormatInterface.json` and this guide in the same
  Engine change;
- regenerate `Docs/generated/image-format.json` and all generated image pages;
- run `BuildTools/tests/test_docs_image_format.py`, the complete documentation
  suite, and `docs_contract_diff.py` against the intended base;
- run focused native tests for the touched loader/runtime/atlas boundary;
- update [BakingPipeline.md](BakingPipeline.md),
  [ClientRuntime.md](ClientRuntime.md), or
  [SpriteRootMotion.md](SpriteRootMotion.md) only when their owned behavior
  changes;
- update embedding-project docs only for concrete assets, policy, integration,
  and visible validation changes;
- keep public examples pinned to an exact Engine revision and do not promise
  support for a format or option that their validation route does not exercise.
