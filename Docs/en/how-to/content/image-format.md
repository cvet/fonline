---
layout: default
title: Image And Sprite Formats
locale: en
document_id: image-format-guide
permalink: /Docs/en/how-to/content/image-format.html
---

# Image And Sprite Formats

FOnline bakes RGBA/legacy images, animations, direction sheets and `.fofrm`
compositions into a versioned sprite container, optionally with cropped RGBA
and validated silhouette geometry. Clients upload frames to atlases as
`AtlasSprite` or `SpriteSheet`.

Authoring and operation are covered here; the generated
[image-format reference](../../reference/image-format/index.md) owns exact
revision contracts:
[source-format](../../reference/image-format/formats.md),
[FOFRM](../../reference/image-format/fofrm.md),
[filename-option](../../reference/image-format/options.md),
[baking](../../reference/image-format/baking.md),
[runtime](../../reference/image-format/runtime.md), and
[validation](../../reference/image-format/validation.md),
[canonical JSON](../../../generated/image-format.json).

## Scope and authority

The owning sources are:

- `Source/Tools/ImageBaker.cpp` and `ImageBaker.h` for source discovery,
  importer dispatch, output naming, per-pack `SpriteInfo`, mesh integration,
  and serialization;
- `Source/Tools/ImageImport.cpp` and `ImageImport.h` for common frame/context
  types and disabled-importer diagnostics;
- `Source/Tools/ImageBaker-Fofrm.cpp`, `ImageBaker-Png.cpp`, and
  `ImageBaker-Tga.cpp` for mandatory composition and RGBA import;
- adjacent `ImageBaker-Art.cpp`, `ImageBaker-Bam.cpp`, `ImageBaker-Frm.cpp`,
  `ImageBaker-Mos.cpp`, `ImageBaker-Rix.cpp`, `ImageBaker-Spr.cpp`,
  `ImageBaker-Til.cpp`, and `ImageBaker-Zar.cpp` for optional importers;
  `ImageBaker-ZarDecoder.cpp` shares their ZAR/TIL decoder while retaining
  the format-specific red/blue channel order;
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

`BuildTools/ImageFormatInterface.json` owns the structured contract.
`BuildTools/docs_image_format.py` checks source anchors, live baker/runtime
extensions and the absence of serialized FOFRM `Effect`, then renders references.
Importer, descriptor, container, factory, atlas and cache changes require model updates.

`ImageBaker::AddLoader` callbacks receive `const ImageImport::Context&`:
use `*context.Files` for the pack and `context.Resolve(path)` for checked nesting
with filename options. Move former `ImageBaker` types `FrameShot`, `FrameSequence`,
`FrameCollection`, `MAX_DIRS_MINUS_ONE` to `ImageImport` and replace `FileCollection`
loader parameters with this context. Frame/container meanings stay unchanged.

Projects own asset catalogs/licenses, pack precedence, source policy, visual
style, animation substitutions, hit expectations, movement tuning and visual baselines.

## Choose a source format

| Need | Preferred source | Notes |
|---|---|---|
| Ordinary static image or transparent sprite | PNG | Recommended project-authored lossless input. Palette, low-bit grayscale, `tRNS`, and 16-bit channels are normalized to RGBA8. |
| Existing TrueColor pipeline output | TGA | Use the supported type 2/type 10, 24/32-bpp, no-image-ID, bottom-left-origin subset. |
| Multi-frame or directional project sprite | FOFRM referencing PNG/TGA | Keeps composition, offsets, frame deltas, and timing explicit and reviewable. |
| Existing Fallout FRM/FR0 asset | FRM or FR0 | Preserve only when the project can redistribute the source and has a visual regression route. |
| Existing Arcanum ART or Tactics SPR asset | FOFRM referencing ART/SPR | Filename options select palettes, frames, mirrors, colors, and sequences. SPR should normally be wrapped because of the default runtime boundary. |
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

### Whole-source alias

Use `Alias` to keep the source's complete direction sheet, offsets, timing and
frame deltas. Ordinary `frm` composition retains only child Main frames.

```ini
Alias = Actor$r=3.art
```

`Alias` resolves a binary image source relative to the descriptor; another
FOFRM is rejected. The baked output retains the descriptor path. Other
composition keys are ignored. Filename options pass unchanged to the registered
source loader, exactly as for ordinary `frm` references. FOFRM has no
format-specific fields. Without source options selecting a direction, an alias
keeps all source directions.

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
boundary are documented in [Sprite Root Motion](sprite-root-motion.md).

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
prototype, GUI, or script surface documented by [Effect Format](effect-format.md).

## Legacy filename options

Options appear after `$` and before the extension. `LoadAny` strips them from
the physical lookup path and passes them to the selected loader.

### ART

```text
Actor$1THF5-7.art
```

- `0` through `3` select a palette; the last selector wins and an unavailable
  palette falls back to the first present palette slot.
- `T` derives alpha from the maximum RGB component; palette index zero remains
  fully transparent.
- `H` and `V` mirror horizontally and vertically.
- `F5` selects frame 5; `F5-7` and `F7-5` select inclusive ascending or
  descending ranges. Bounds clamp to the available frame table.
- Option letters are case-insensitive. Unknown characters are ignored.

ART is the Arcanum format. The 132-byte header stores bit depth (8), not
rotation count. Flag `0x1` means one stored rotation; otherwise there are eight.
All rotation-major frame tables precede the pixel streams. Raw data has exactly
width × height bytes; RLE is bounded by each frame's declared data size.
Truncated tables/streams, underfilled or overflowing RLE, negative dimensions
and values outside the baked canvas/offset range fail during baking.

Each direction aligns its selected frames on one transparent canvas so their
hot spots coincide. Sequence offsets locate that hot spot; `NextX`/`NextY`
retain the frame deltas. Square directions use ART rotations `1,2,3,4,5,6,7,0`;
hex directions use `1,2,3,5,6,7`. The `art/critter/`, `art/monster/` and
`art/unique_npc/` categories mirror rotations 1–3 from 7–5. Walls and portals
shift hot spots by (-40,+20) for rotations 0,1,6,7. Horizontal roof mirrors use
the left-edge anchor. These rules follow the [Arcanum renderer](https://github.com/alexbatalov/arcanum-ce/blob/main/first_party/tig/src/art.c).

Whole duration is `(1000 / fps) * selected_frames`, saturated at 65535 ms;
fps zero gives zero ticks for explicit frame indexing. Keep playing animations
short enough for a positive per-frame duration.

The ART loader accepts `r=0..7` (or uppercase `R=`) to select one clockwise
rotation from north. Use `Alias = Actor$r=3.art` to retain that rotation's
anchors and timing, or `frm = Actor$r=3.art` in ordinary composition. A source
with one stored rotation reuses it with wall placement for the requested
rotation. Put a palette digit before `r=` and alpha/mirror/frame options after
it, for example `Actor$1r=3HF0-1.art`. Missing, negative, multi-digit or
out-of-range rotations and invalid trailing characters fail baking. The equals
sign distinguishes this option from legacy `$r3` or `$R3`, which still select
palette 3.

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

`SpriteManager` selects a registered `SpriteFactory` by lowercased extension.
The default factory passes all bytes to `ReadSpriteResource` to validate magic,
version, records, mesh geometry, footer and trailing data, then checks one/full directions.

A single frame/direction becomes `AtlasSprite`: placement uses `OffsX`/`OffsY`;
`NextX`/`NextY` is read but ignored. Otherwise `SpriteSheet` stores parallel
direction sheets and concrete/shared frame displacement in `_sprOffset`.

`SpriteSheet` supports direction selection, randomized `Prewarm`, normalized time,
looping and reverse playback. `Play` ignores one frame or zero ticks; changing
direction preserves the common animation clock.

Concrete positive-size RGBA frames use the requested `AtlasType`, duplicated
one-pixel edges for linear filtering and an alpha hit mask from
`Settings.SpriteHitValue`. The cache key includes atlas type, allowing separate
interface/map/one-image instances of one path.

With `SpriteMesh.Enabled`, the visible mask is `alpha > AlphaThreshold`.
Candidates use up to `MaxTriangles`; `AreaSavingsWeight` scores saved frame area
against submitted triangles. Meshes must cover every visible pixel cell.
Invalid/unprofitable candidates retain quads; empty masks record empty geometry.
Cropping/padding preserves placement, scaling, map-light interpolation and hit
coordinates through serialized offsets, logical source size and source origin.

The baker validates the complete `SpriteMesh.*` group even when mesh generation
is disabled. Standalone project configs must therefore declare a positive
`MaxTriangles`, an `AlphaThreshold` in `0..254`, and a finite non-negative
`AreaSavingsWeight`; [the minimal project config](../../../../Examples/MinimalProject/FOnlineStarter.fomain)
shows the explicit disabled defaults.

Full-image draws submit baked indexed triangles; region crops, tiles, padded
effects/outlines, fonts, blits, runtime model sprites and particles use rectangles.
[Frontend and Rendering](../../explanation/rendering/#sprite-and-model-atlas-geometry)
owns draw/atlas dumps; [Baking Pipeline](../../explanation/content-pipeline/baking.md#image-and-sprite-mesh-statistics)
owns candidate policy and report fields.

`SpriteInfo/<PackName>.foinfo` v1 indexes duration, directions, frame bounds,
offsets and shared references. Server/client `EngineMetadata` reads it without
RGBA decoding. Adding or losing this per-pack index requires a full rebake.

Every copyable load, including the first, returns `MakeCopy()`; the cache retains
the factory prototype. `SpriteSheet` copies own playback and frame tables, start
at frame/direction zero with valid size/anchor and retain state/action tags.
`AtlasSprite::MakeCopy()` shares atlas pixels/geometry. First/last extraction
cannot shorten later loads; each trimmed direction has one sprite and offset.
`_nonFoundSprites` memoizes missing files/extensions, unknown factories and load
failures by path. `CleanupSpriteCache()` does not clear it: recreate the manager
or restart after adding a previously failed resource.

## Authoring practices

1. Prefer PNG and explicit FOFRM keys/one-frame references; convert unsupported
   or ambiguous TGA exports. Use lowercase extensions and stable case-correct paths.
2. Keep one animation per descriptor. Deep nesting obscures count, flattening and timing.
3. Write `fps`, `count`, every `frm_N` and both offsets explicitly, including
   every direction; keep complete direction sets with equal frame counts.
4. `NextX`/`NextY` is presentation data: inspect root motion visually;
   authoritative movement/collision must not depend on it.
5. Wrap SPR in FOFRM unless a project-owned factory handles it. Check generated
   options and native fixtures before editing legacy selectors.
6. Redistribute legacy assets/palettes only with permission; record project provenance.
7. Visually check dimensions, transparent borders, filtering, hit masks,
   directions, cadence and stop/start behavior.
8. Changes to `SpriteMesh.*`, container code or mesh policy require
   `ForceBakeResources`; timestamps cannot attest settings. Inspect
   `BakingReport.json` and `Game.DumpAtlases()` for triangle cost, padding,
   cropping and fallback changes.

## Diagnostics

| Symptom | Likely boundary | First checks |
|---|---|---|
| Targeted bake silently writes nothing | Unsupported/missing target or `BakeChecker` skip | Check extension, pack, case, timestamp/cache and full-scan discovery. |
| `Image file not found` | FOFRM reference | Resolve relative to descriptor; strip `$...` for the physical path. |
| `FOFRM file invalid data` | Missing slot, unequal/partial directions, empty child | Check `count`, `frm_N`, directions and flattened counts. |
| `FOFRM file invalid data (shared index)` | Nested shared records | Use concrete sources or remove nesting. |
| Direct `.spr` unknown extension | Runtime factory | Use baked `.fofrm` or a project factory. |
| Added sprite still missing | `_nonFoundSprites` | Confirm output; recreate manager/restart. |
| Animation stopped | One frame, zero ticks, invalid cadence | Check flattened count, `AnimTicks`, integer ticks/frame. |
| Wrong directional framing | Inherited/missing offsets, remap | Set explicit offsets; inspect every direction. |
| TGA flipped/shifted | Unsupported origin/image ID | Re-export supported TGA or PNG. |
| Alpha fringe/wrong hit area | Alpha, atlas border, `SpriteHitValue` | Inspect source alpha and final sprite in the intended profile. |

Every loader raises `Image import failed` with source path/options/size and the
original exception. Each FOFRM `Alias`/nested `frm` level retains that context.
`FileReader` adds byte position, requested size and buffer extent. Loaders do not log.

Scans log each `Image baking error`, then throw `Errors during images baking`
after all selected work. Fix each underlying file rather than the aggregate count.

## Validation workflow

For documentation/model changes:

```powershell
python BuildTools\docs_image_format.py --write
python BuildTools\docs_image_format.py --check
python -m unittest BuildTools.tests.test_docs_image_format
python BuildTools\docs_contract_diff.py --help
python BuildTools\docs_validate.py
```

Importer/container/atlas/playback changes require focused `RunUnitTests`:
`Test_ImageBaker.cpp` covers targeted/scanned baking, BakeChecker, PNG/TGA,
legacy loaders/options, FOFRM flattening/directions, shared records, renaming
and malformed inputs; `Test_TextureAtlas.cpp` covers allocation split/search/free.

For affected projects:

1. Regenerate model/references and review the aggregate `image-format` diff.
2. Rebake against the exact Engine pin; check resource/prototype path resolution.
3. Visually exercise changed images, animation, directions, mirrors, alpha,
   hit areas and every supported renderer/profile.
4. For locomotion, follow [Sprite Root Motion](sprite-root-motion.md): straight
   movement, turns, direction changes and stop/start transitions.

Native tests prove decoding/container invariants; framing, pack precedence,
cadence, filtering, clickability and perceived foot sliding require visual checks.

## Change checklist

For image changes:

- Update `BuildTools/ImageFormatInterface.json` and this guide together;
  regenerate `Docs/generated/image-format.json` and all image pages.
- Run `BuildTools/tests/test_docs_image_format.py`, complete docs validation,
  `docs_contract_diff.py` against the intended base and focused native tests.
- update [Baking Pipeline](../../explanation/content-pipeline/baking.md),
  [Client Runtime](../../explanation/runtime/client.md), or
  [Sprite Root Motion](sprite-root-motion.md) only when their owned behavior
  changes;
- update embedding-project docs only for concrete assets, policy, integration,
  and visible validation changes;
- keep public examples pinned to an exact Engine revision and do not promise
  support for a format or option that their validation route does not exercise.

## Importer composition and native extensions

PNG/TGA/FOFRM are mandatory. Other importers require individual `FO_IMAGE_*` [options](../build/project-configuration.md#optional-native-components). Known disabled inputs, including nested FOFRM references, report their path and required option. Adjacent `ImageBaker-*.h/.cpp` files guard palettes/helpers together; `ImageBaker-ZarDecoder` is shared by ZAR/TIL when either is enabled.

Native callbacks use `ImageImport::FrameShot`, `FrameSequence`, `FrameCollection`, `LoadFunc` and `(string_view name, string_view options, FileReader reader, const ImageImport::Context& context)`. Use `context.Files` for sibling assets, `context.Resolve(path)` for nesting, and the third `AddLoader` argument for output-name mapping. Scheduling, meshes and serialization stay in the coordinator. Stock clients still read baked containers without source decoders; runtime extensions/container version are unchanged.
