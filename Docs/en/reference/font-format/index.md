---
title: Generated Font Format Reference
document_id: generated-font-format-index
locale: en
generated: true
---

# Generated Font Format Reference

> Generated reference. Do not edit directly. Update `BuildTools/FontFormatInterface.json`, then run `python BuildTools/docs_font_format.py --write`.

[Index](index.md) | [Formats](formats.md) | [Binding](binding.md) | [Authoring](authoring.md) | [Layout](layout.md) | [Rendering](rendering.md) | [Validation](validation.md) | [Canonical JSON](../../../generated/font-format.json) | [Guide](../../how-to/content/font-format.md)

This reference describes the Engine-owned TrueType text pipeline: face binding, the built-in default font, colored faces, the bitmap-font converter, text layout, rendering, and validation. Project font selection and typography policy remain outside this contract.

## Contract status

| Field | Value |
| --- | --- |
| Stability | <code>experimental</code> |
| Support policy | TrueType faces are the only runtime text format and client layout behavior is supported but still experimental; embedding projects own font choice, glyph coverage, typography, GUI slots, and visual acceptance. |
| Source manifest | [BuildTools/FontFormatInterface.json](https://github.com/cvet/fonline/blob/master/BuildTools/FontFormatInterface.json) |
| Contract digest | <code>b46e34458f6ee9e8fdc7a9d188406fc487a45a41def68babd26991162be34494</code> |
| Runtime faces | <code>.ttf</code> |
| Default font | <code>Fonts/FOnline-Regular.ttf</code>, <code>10</code> px, <code>Embedded</code> |
| Embedded data capacity default | <code>400000</code> |
| Colored face load flags | <code>FT_LOAD_COLOR &#124; FT_LOAD_NO_HINTING</code> |
| Client atlas | <code>IfaceSprites</code> |

| Reference | Entries | Purpose |
| --- | --- | --- |
| [Formats](formats.md) | 3 | Runtime face formats, converter sources, and shipped faces. |
| [Binding](binding.md) | 10 | Face binding, the default font, slots, atlas, and Embedded capacity. |
| [Authoring](authoring.md) | 7 | The bitmap-font converter, its output, and its manifest. |
| [Layout](layout.md) | 10 | TextFormat, wrapping, alignment, measurement, and glyph fallback. |
| [Rendering](rendering.md) | 9 | Glyph cells, colors, outlines, effects, color tags, and cache. |
| [Validation](validation.md) | 6 | Failure modes and executable validation gates. |

## Boundary

Included:

- TrueType faces rasterized at runtime through the face-list Game.BindFont overload
- the engine default font bound to the Default slot before any script runs
- colored faces drawn with their own COLR layer colors
- the bitmap-font converter that turns authored bitmap fonts into TrueType faces, and its manifest
- raw-copy delivery of faces and the Embedded pack capacity that holds the default font
- font slots, atlas placement, glyph sheets, and rebinding
- TextFormat flags, wrapping, alignment, measurement, inline colors, and caching
- source-backed diagnostics and validation routing

Excluded:

- embedding-project font names, slot catalogs, GUI assignments, and typography policy
- font licensing, redistribution rights, and language-specific glyph acceptance
- bitmap font descriptors as a runtime format: the client loads neither .fofnt, .fnt, nor .bmfc
- OpenType CFF, WOFF, variable-font instances, text shaping, and bidirectional layout, which the client does not provide
- project-local localization packs and authored display strings
