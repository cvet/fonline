---
title: Generated Font Format Reference
document_id: generated-font-format-index
locale: en
generated: true
---

# Generated Font Format Reference

> Generated reference. Do not edit directly. Update `BuildTools/FontFormatInterface.json`, then run `python BuildTools/docs_font_format.py --write`.

[Index](index.md) | [Formats](formats.md) | [FOFNT](fofnt.md) | [BMFont](bmfont.md) | [Binding](binding.md) | [Layout](layout.md) | [Rendering](rendering.md) | [Validation](validation.md) | [Canonical JSON](../font-format.json) | [Guide](../../FontFormat.md)

This reference describes Engine-owned font descriptors, client binding, text layout, rendering, scaling, and validation. Project font selection and typography policy remain outside this contract.

## Contract status

| Field | Value |
| --- | --- |
| Stability | <code>experimental</code> |
| Support policy | The two runtime descriptor formats and client layout behavior are supported but still experimental; embedding projects own font choice, glyph coverage, typography, GUI slots, and visual acceptance. |
| Source manifest | [BuildTools/FontFormatInterface.json](https://github.com/cvet/fonline/blob/master/BuildTools/FontFormatInterface.json) |
| Contract digest | <code>bb663fe4cf25ccd150c4136bbd4854a07ec38984661ffda543e5048d677374a5</code> |
| Runtime descriptors | <code>.fofnt</code>, <code>.fnt</code> |
| FOFNT maximum version | <code>2</code> |
| BMFont binary version | <code>3</code> |
| Client atlas | <code>IfaceSprites</code> |

| Reference | Entries | Purpose |
| --- | --- | --- |
| [Formats](formats.md) | 3 | Descriptor roles and supported resource suffixes. |
| [FOFNT](fofnt.md) | 13 | Text descriptor keys and glyph metrics. |
| [BMFont](bmfont.md) | 9 | Accepted binary-v3 blocks and metric transformations. |
| [Binding](binding.md) | 8 | Raw-copy, slot, atlas, scale, and startup behavior. |
| [Layout](layout.md) | 9 | TextFormat, wrapping, alignment, measurement, and glyph fallback. |
| [Rendering](rendering.md) | 7 | Texture preparation, borders, effects, color tags, and cache. |
| [Validation](validation.md) | 8 | Failure modes and executable validation gates. |

## Boundary

Included:

- .fofnt text descriptors and their glyph metrics
- binary BMFont v3 .fnt descriptors accepted by the client
- raw-copy delivery and separately baked font images
- Game.BindFont dispatch, font slots, atlas placement, and startup fallback
- bind-time downscaling, grayscale conversion, border generation, and effects
- TextFormat flags, wrapping, alignment, measurement, inline colors, and caching
- source-backed diagnostics and validation routing

Excluded:

- embedding-project font names, slot catalogs, GUI assignments, and typography policy
- font licensing, redistribution rights, and language-specific glyph acceptance
- BMFont text or XML descriptors, which the runtime does not parse
- .bmfc authoring-tool configuration semantics
- vector font or runtime TTF/OTF rasterization, which the client does not provide
- project-local localization packs and authored display strings
