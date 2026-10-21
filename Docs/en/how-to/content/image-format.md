---
layout: default
title: Image And Sprite Formats
locale: en
document_id: image-format-guide
permalink: /Docs/en/how-to/content/image-format.html
---

# Image And Sprite Formats

FOnline bakes RGBA/legacy frames and `.fofrm` sheets into versioned RGBA/mesh
containers for client `AtlasSprite`/`SpriteSheet` atlases.

Exact contracts: [image reference](../../reference/image-format/index.md),
[canonical JSON](../../../generated/image-format.json).

## Scope and authority

`BuildTools/ImageFormatInterface.json` owns source-anchored import, recipe,
dependency, meshing and runtime contracts. `BuildTools/docs_image_format.py`
checks and renders them; update both with behavior.

`ImageBaker::AddLoader` uses `const ImageImport::Context&`: read `*context.Files`,
resolve nested inputs through `context.Resolve(path)`, declare renamed outputs
with `ResolveOutput` and auxiliary reads with dependency discovery. Frame types
live in `ImageImport`; projects own art policy, movement and visible acceptance.

## Choose a source format

| Need | Preferred source | Notes |
|---|---|---|
| Static image/transparent sprite | PNG | Lossless authored input; palette, low-bit grayscale, `tRNS`, 16-bit channels normalize to RGBA8. |
| Existing TrueColor output | TGA | Type 2/10, 24/32-bpp, no image ID, bottom-left origin. |
| Multi-frame/directional sprite | FOFRM referencing PNG/TGA | Reviewable composition, offsets, frame deltas, size and timing. |
| Existing Fallout asset | FRM or FR0 | Requires redistribution rights and visual regression checks. |
| Existing Arcanum/Tactics asset | FOFRM referencing ART/SPR | Options select palettes/frames/mirrors/colors/sequences; wrap SPR for runtime. |
| Existing Infinity Engine asset | ZAR/TIL/MOS/BAM; FOFRM for selection | Import path; prefer PNG for new art. |

The built-in image baker does not support JPEG, BMP, GIF, DDS, WebP, AVIF or SVG;
convert to PNG/supported TGA. Renderer/library support does not extend the
`ImageBaker`/runtime `SpriteFactory` contract.

## Pipeline overview

1. Mounted sources form a dependency graph before output filters select roots.
2. Loaders build `FrameCollection`; meshing pads/crops RGBA and resolves anchors.
3. Root containers and `SpriteInfo/<PackName>.foinfo` are published.
4. `SpriteManager` dispatches baked bytes to atlas sprites or `SpriteSheet`.

The stock client runs no source decoders. Baked `Sprite.png` contains the Engine
container; its extension selects dispatch, not payload format.

### Dependencies, publication and incremental baking

FOFRM inputs remain readable by the resolver even when pack patterns exclude
standalone outputs. Logical source paths resolve through the mounted catalog and
overlay winners. The root-only graph plan suppresses consumed candidates; full
Master baking and on-demand lookup also retain standalone candidates for an
independent game resource or model texture reference. Local cache availability
and package export are separate decisions.

`Resources.usage.json` selects the shipped outputs. A flattened FOFRM has no
runtime dependency on its input PNG or intermediate FOFRM, so those copies are
excluded unless independently referenced. Separate root wrappers remain useful
for distinct transformations. `RawCopy` yields to image ownership. A loader that
changes `NewName` must declare that same resource path through `ResolveOutput`.

The graph uses the native `ImageRecipe` parser for quoted `Frames` and records
FRM palettes/FRX auxiliary directions, including absent optional files. Missing
inputs, cycles and more than 64 nested FOFRM nodes fail with a dependency chain.
Filename options belong to references, not separate physical files.

BLAKE2b fingerprints include transitive content, paths and mount winners,
optional-input absence, recipe/options, build and baking contract, and
`SpriteMesh` settings. Byte changes are detected even when size and mtime stay
unchanged. Current roots still enter the output manifest, so stale-output cleanup
works during incremental baking. Changes to dependencies require no descriptor
touch or forced bake.

Successful-output snapshots and individual SpriteInfo entries live under
`.baker-cache/Image/<PackName>/`; they are private, non-shipping files. A missing
or corrupt snapshot triggers conversion. New snapshots follow successful output
writes; current entries rebuild the aggregate SpriteInfo index without decoding
unchanged pixels. On-demand reindexing uses the same graph/fingerprints and
checks the pack that declared each output, preventing stale overlay files from
resurfacing. These baker rules do not clear an already loaded client sprite cache.

## FOFRM grammar

FOFRM uses `ConfigFile`. Names are case-sensitive: `Source`, `Fps`, `Offset`,
`Frame`, `Frames`, `FrameOffsets`, `Size`, `FrameSizes`.
Root is direction 0. Hex: `[HexDir 1]`..`[HexDir 5]`; square:
`[SquareDir 1]`..`[SquareDir 7]`. Use exact one-space names and indexes,
not degrees; named zero/old Dir forms fail.
Names fail before Source/frames; legacy refusals use
`FO_TEMPORARY_COMPAT(FofrmLegacyDescriptor, "2026-12-31")`.

Write `Key = Value`; continue with space-backslash, last line without it.

### Whole-source alias

```ini
Source = Actor$r=3.art
```

Source inherits a relative binary or FOFRM collection, including every direction,
placement, timing and movement. Root transforms default every direction; named
sections override existing directions. Offset adds to the inherited sequence
anchor. Source excludes Frame/Frames; obsolete Alias is rejected with
`FO_TEMPORARY_COMPAT(FofrmWholeCollectionSource, "2026-12-31")`.

### Minimal static image

```ini
Frame = Icon.png
```

Use Frame (one whole path) or Frames (nonempty AnyData String array).
Quote Frames paths with spaces, retain repeats, one per continued line.
Paths are relative; count is derived, no Count key; Fps defaults to 10.

```ini
Frames = \
 "Idle 00.png" \
 Idle_01.png \
 Idle_02.png
Fps = 8
```

### Sequence and frame placement

Offset: signed int32 X Y, initially zero, carrying from root/previous direction.
FrameOffsets: one int32 X Y per local reference before flattening; omitted zero,
no inheritance. Empty/invalid/overflowing/wrong-count/reference-free lists fail.

```ini
Frames = \
 A.png \
 B.png
Offset = -24 -63
FrameOffsets = \
 2 -1 \
 0 3
```

FrameOffsets: AnyData String-array, strict integers, one pair per continued line.
All child frames get `Offset + FrameOffsets[i]` plus child frame placement, without
accumulation. Imported Motion is unchanged ([root motion](sprite-root-motion.md));
NextX_N/NextY_N fails before Source.

### Physical frame size

Size resizes to exact dimensions, without preserving aspect ratio:

```ini
Frame = Icon.png
Size = 64 48
```

Size/FrameSizes axes are strict AnyData integers in `1..65535`. FrameSizes =
one W H pair per reference. Root scalar values default directions; local arrays
replace them. A scalar and its array form in one section conflict. Source counts
as one reference and applies defaults to every retained direction.

| Operation | Keys and values |
| --- | --- |
| Input sequence | InputDir / FrameInputDirs: existing source direction; explicit root Source selection retains one sequence. |
| Crop then grid | Crop / FrameCrops: X Y W H, fully inside source. Grid: W H cells, exact divisibility, row-major. |
| Reflect then rotate | Flip / FrameFlips: None, X, Y, XY. Rotate / FrameRotations: 0, 90, 180, 270 clockwise. |
| Resize then canvas | Size / FrameSizes; Canvas / FrameCanvases: W H. CanvasOffset / FrameCanvasOffsets: nonnegative X Y from top-left, default 0 0; clipping fails. |
| Flatten then select | FrameIndices: nonempty AnyData indices; order/repeats allowed, out-of-range fails. Source defaults every direction. |
| Movement | Motion / FrameMotions: signed int16 X Y replaces imported motion; absence preserves it. |
| Filtering | Resample: Nearest, Bilinear (default), Area (down/identity only). PixelUsage: Color (default), Data, Normal. ColorSpace: Encoded (default), Linear; Color only. |

Every Frame-array has one entry per original reference, before grid/flattening.
Bitmap operations preserve literal Offset/FrameOffsets and movement. Default
Bilinear uses pixel-center Q16 premultiplied alpha; Linear uses fixed sRGB16.
Data filters channels independently; Normal renormalizes RGB, zero becomes +Z.
Data/Normal disable alpha mesh cropping, including shared pixel owners, to keep
channels outside visible alpha intact.
Nearest/identity preserve bytes. Full arithmetic and validation are in the
[FOFRM reference](../../reference/image-format/fofrm.md).

### Direction sheets

Use one sequence or a complete direction table. Counts and timelines may differ.
Frame/Frames tables reject gaps; Source can override existing directions locally.

```ini
Frames = \
 Walk_NE_00.png \
 Walk_NE_01.png
Offset = -24 -63
Fps = 10

[HexDir 1]
Frames = \
 Walk_E_00.png \
 Walk_E_01.png

# Continue every configured map direction.
```

### Nested references and flattening

Frame/Frames append child Main frames with registered extensions/`$` options,
retaining frame placement, discarding directions/sequence placement.
Shared references are checked and materialized before composing or transforming pixels.

```ini
Frames = \
 FirstCycle.bam \
 SecondCycle.bam
Fps = 10
```

Without explicit timing, Frame/Frames assigns 100 ms to each final frame; Source
preserves its timeline. Duration sets the total in milliseconds; FrameDurations
lists one positive duration per final frame. They conflict with Fps in the same
section. Root Duration/Fps are defaults; local timing wins, FrameDurations is local.
Each direction has 1..65535 frames and a positive uint32 duration sum. Duration
must be at least the frame count; integer boundary division preserves its sum.
Fps is root-only and positive. See the reference for exact validation.

### Legacy `Effect` key

Effect is refused before resolving inputs, with
`FO_TEMPORARY_COMPAT(FofrmRetiredEffect, "2026-12-31")`.
Choose effects through [renderer/prototype/GUI/script surfaces](effect-format.md).

## Legacy filename options

`LoadAny` strips `$` options before physical lookup. ART selects palettes, frames,
ranges, rotations and mirrors; SPR selects named sequences/directions; BAM selects
cycle/frame. See the [filename-option reference](../../reference/image-format/options.md)
and [source-format reference](../../reference/image-format/formats.md) for defaults,
clamping, canvas placement and raw timing normalization. Verify imported root motion
and cadence. The default runtime factory does not register `.spr`.

### ART

Use `$F` ranges, `$r=` rotations and palette/mirror selectors from the reference.

### SPR

Select the named sequence and direction before composing a FOFRM root.

### BAM

Select cycle/frame; invalid selectors fall back to zero.

## Source-format details

### PNG

PNG normalizes palette/grayscale, `tRNS`, 16-bit channels and missing alpha to
one RGBA8 frame; corrupt input fails through libpng. The
[source-format reference](../../reference/image-format/formats.md) owns exact
normalization. Project color-management policy remains explicit.

### TGA

Use raw/RLE TrueColor type 2/10, 24/32-bpp, no image ID and bottom-left origin.
Other types/depths fail; the importer does not honor the origin bit or skip IDs.
Prefer PNG unless the exporter has a tested TGA preset. See the
[source-format reference](../../reference/image-format/formats.md).

### Fallout FRM and FR0

FRM/FR0 palette, animated-color, split-direction and output-name rules are in the
[source-format reference](../../reference/image-format/formats.md). Verify final
directions, root motion and cadence after import; auxiliary files join the graph.

### RIX, ZAR, TIL, MOS, and BAM

These compatibility importers retain format-specific palettes, compression,
transparency and timing; see the [source-format reference](../../reference/image-format/formats.md).
Use PNG plus FOFRM for new project art.

## Baked container boundary

The private stream is versioned between `ImageBaker` and `ReadSpriteResource`.
Its framing, concrete/shared RGBA records, offsets, motion and mesh data are
specified in the [baking reference](../../reference/image-format/baking.md).
Project scripts and external tools must use source formats rather than parsing
or producing this stream. Rebuild matching binaries and packs after a layout change.

## Runtime loading, atlas, and caches

`SpriteManager` selects a registered `SpriteFactory` by lowercased extension.
The default factory passes all bytes to `ReadSpriteResource` to validate magic,
version, records, mesh geometry, footer and trailing data, then checks one/full directions.

A single frame/direction becomes `AtlasSprite`: placement uses the baked draw offset;
`Motion` is read but ignored. Otherwise `SpriteSheet` stores parallel
direction sheets and concrete/shared frame displacement in `_sprOffset`.

`SpriteSheet` supports direction selection, randomized `Prewarm`, normalized time,
looping and reverse playback. `Play` ignores one frame; changing direction
preserves normalized phase across different counts and durations. Frame durations
are positive milliseconds; cumulative boundaries select frames without division
by an integer frame period.

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

`SpriteInfo/<PackName>.foinfo` v2 indexes per-direction counts, per-frame durations,
bounds, offsets and shared pixels. Each logical slot owns timing, placement and
motion. Server/client `EngineMetadata` reads it without
RGBA decoding. The baker maintains this index from current output snapshots;
an absent index can be reconstructed during ordinary or on-demand baking.

Every copyable load, including the first, returns `MakeCopy()`; the cache retains
the factory prototype. `SpriteSheet` copies own playback and frame tables, start
at frame/direction zero with valid size/anchor and retain state/action tags.
`AtlasSprite::MakeCopy()` shares atlas pixels/geometry. First/last extraction
cannot shorten later loads; each trimmed direction has one sprite and offset.
`_nonFoundSprites` memoizes missing files/extensions, unknown factories and load
failures by path. `CleanupSpriteCache()` does not clear it: recreate the manager
or restart after adding a previously failed resource.

## Authoring practices

1. Prefer PNG, explicit FOFRM keys and single-frame references. Convert
   ambiguous/unsupported TGA; keep lowercase extensions and case-correct paths.
2. Keep one animation per descriptor; nesting obscures flattening/timing.
3. Order Frame/Frames, Size/FrameSizes, Offset, FrameOffsets, metadata; indent continuations one
   space. Runtime order/whitespace is free.
4. FrameOffsets adjusts draw placement; imported Motion drives presentation
   root motion. Inspect both visually; neither drives movement/collision.
5. Wrap SPR unless a project factory handles it; check generated options and
   native fixtures before editing selectors.
6. Redistribute assets/palettes with permission and project provenance.
7. Visually check dimensions, borders, filtering, hit masks, directions,
   cadence and stop/start behavior.
8. Rebuild the baker after container/mesh-policy code changes; dependency
   fingerprints include SpriteMesh settings. Inspect BakingReport.json and
   Game.DumpAtlases() for triangle cost, padding, crop and fallback changes.

## Diagnostics

Check the published resource path, pack and usage manifest first. A local
consumed input is exported only when independently referenced. Resolve FOFRM paths relative to their descriptor and strip `$` options for
physical lookup. See [validation rules](../../reference/image-format/validation.md)
for grammar, dependency, direction, pixel and timeline refusals.

For missing-cache retries, atlas alpha/hit area, TGA origin and directional placement,
use the corresponding runtime/source sections above. A one-frame collection does not
play; other timelines require positive durations, without a global integer frame period.

Every loader raises `Image import failed` with source path/options/size and the
original exception. Each FOFRM `Source`/nested `Frame` level retains that context.
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

Native frame data uses `isize32 Size`, `ipos32 Offset` for independent draw placement,
and `ipos32 Motion` for per-frame animation deltas accumulated during playback. FRM/FRx frame deltas and
ART DeltaX/DeltaY populate Motion. BAM signed centers and SPR bbox/center placement
populate Offset, including repeated/shared frames; they do not imply movement.
Bitmap transforms preserve placement and motion in their original pixel units.
SpriteResource v3 retains uint16 dimensions and int16 motion; the baker checks
these ranges before writing. Custom native loaders must migrate the old scalar
fields. FOFRM uses Motion/FrameMotions; Move/FrameMoves fail before resolving Source.
