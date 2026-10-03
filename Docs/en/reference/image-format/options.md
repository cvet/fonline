---
title: Legacy Image Filename Options
document_id: generated-image-format-options
locale: en
generated: true
---

# Legacy Image Filename Options

> Generated reference. Do not edit directly. Update `BuildTools/ImageFormatInterface.json`, then run `python BuildTools/docs_image_format.py --write`.

[Index](index.md) | [Formats](formats.md) | [FOFRM](fofrm.md) | [Options](options.md) | [Baking](baking.md) | [Runtime](runtime.md) | [Validation](validation.md) | [Canonical JSON](../../../generated/image-format.json) | [Guide](../../how-to/content/image-format.md)

| Stable ID | Syntax | Behavior | Why | Source |
| --- | --- | --- | --- | --- |
| <a id="entry-image-format-option-art-6327308876"></a><code>image-format.option.art</code> | <code>Name$[0-3][T][H][V][Fframe&#124;Ffrom-to].art</code> | Digits select the last requested available palette, T derives alpha from maximum RGB while index zero remains transparent, H/V mirror, and F selects an inclusive ascending or descending clamped frame/range; letters are case-insensitive and unknown characters are ignored. | Options are parsed from the source reference without changing the physical source filename. | [Source/Tools/ImageBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ImageBaker.cpp) |
| <a id="entry-image-format-option-spr-25a63adbd0"></a><code>image-format.option.spr</code> | <code>Name$[part,r,g,b]...Sequence.spr</code> | Zero or more bracket entries set clamped RGB offsets for part 0 other, 1 skin, 2 hair, or 3 armor; an out-of-range part applies the RGB values to all parts, and text after the last bracket selects a sequence case-insensitively (empty selects the first). | The importer composes palette layers and animation selection before writing runtime RGBA frames. | [Source/Tools/ImageBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ImageBaker.cpp) |
| <a id="entry-image-format-option-bam-816a86bae2"></a><code>image-format.option.bam</code> | <code>Name$cycle[-frame].bam</code> | The integer before '-' selects a cycle and an optional non-negative integer after '-' selects one frame; out-of-range cycle/frame values fall back to zero, while no frame selector imports the whole cycle. | The source can expose many cycles while one resource path needs deterministic output. | [Source/Tools/ImageBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ImageBaker.cpp) |
