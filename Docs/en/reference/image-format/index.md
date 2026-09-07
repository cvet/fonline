---
title: Generated Image Format Reference
document_id: generated-image-format-index
locale: en
generated: true
---

# Generated Image Format Reference

> Generated reference. Do not edit directly. Update `BuildTools/ImageFormatInterface.json`, then run `python BuildTools/docs_image_format.py --write`.

[Index](index.md) | [Formats](formats.md) | [FOFRM](fofrm.md) | [Options](options.md) | [Baking](baking.md) | [Runtime](runtime.md) | [Validation](validation.md) | [Canonical JSON](../../../generated/image-format.json) | [Guide](../../how-to/content/image-format.md)

This reference describes the Engine-owned image import, FOFRM composition, baked sprite, client loading, atlas, cache, and validation contract. Project asset catalogs and visual acceptance remain project-owned.

## Contract status

| Field | Value |
| --- | --- |
| Stability | <code>experimental</code> |
| Support policy | The contract is generated for a pinned Engine revision. Projects own asset catalogs, source licensing, visual style, compression policy, resource-pack precedence, animation substitutions, movement tuning, and visible acceptance. |
| Source manifest | [BuildTools/ImageFormatInterface.json](https://github.com/cvet/fonline/blob/master/BuildTools/ImageFormatInterface.json) |
| Contract digest | <code>e566e0b777acd21b4440657bb7dec1cbcfd15a53e99c9f128f4d0c303e17753a</code> |
| Baker | <code>Image</code>, order 4 |
| Baked pixels | <code>RGBA8</code> |
| Runtime side | <code>client</code> |

| Reference | Entries | Purpose |
| --- | --- | --- |
| [Formats](formats.md) | 12 | Accepted source formats and their current import behavior. |
| [FOFRM](fofrm.md) | 9 | Descriptor fields, aliases, directions, flattening, and timing. |
| [Options](options.md) | 3 | ART, SPR, and BAM filename selectors. |
| [Baking](baking.md) | 10 | Discovery, output naming, container records, and failures. |
| [Runtime](runtime.md) | 8 | Factory coverage, sprite sheets, atlas upload, and caches. |
| [Validation](validation.md) | 9 | Source constraints and executable checks. |

## Boundary

Included:

- the twelve built-in ImageBaker source extensions and their current import behavior
- FOFRM fields, aliases, direction sections, nested references, filename options, flattening, offsets, and timing
- the versioned private RGBA/mesh frame container, per-pack SpriteInfo index, and output-renaming behavior
- default client sprite-factory coverage, SpriteSheet playback, polygon or quad atlas drawing, hit masks, caches, and diagnostics
- focused source-anchor, generator, native-test, project-bake, and visible-validation boundaries

Excluded:

- project image catalogs, resource-pack precedence, asset licenses, art direction, quality targets, and acceptance baselines
- authoritative movement and the detailed walk/run projection algorithm documented by the Sprite Root Motion guide
- particle authoring, model textures, shader effects, GUI layout, fonts, video, and audio formats
- a public compatibility promise for the private baked byte stream or unsupported third-party image formats
