---
title: Text Layout Contract
document_id: generated-font-format-layout
locale: en
generated: true
---

# Text Layout Contract

> Generated reference. Do not edit directly. Update `BuildTools/FontFormatInterface.json`, then run `python BuildTools/docs_font_format.py --write`.

[Index](index.md) | [Formats](formats.md) | [FOFNT](fofnt.md) | [BMFont](bmfont.md) | [Binding](binding.md) | [Layout](layout.md) | [Rendering](rendering.md) | [Validation](validation.md) | [Canonical JSON](../../../generated/font-format.json) | [Guide](../../how-to/content/font-format.md)

## FontFlag values

| Name | Value | Source behavior |
| --- | --- | --- |
| <code>None</code> | <code>0x0000</code> | No additional behavior |
| <code>NoWrap</code> | <code>0x0001</code> | On rect-width overflow truncate the rest of the text instead of wrapping it to the next line |
| <code>TruncateLine</code> | <code>0x0002</code> | On rect-width overflow skip remaining glyphs until the next '\n' instead of wrapping |
| <code>CenterX</code> | <code>0x0004</code> | Horizontally center each line within the rect |
| <code>CenterY</code> | <code>0x0008</code> | Vertically center the text block within the rect |
| <code>AlignRight</code> | <code>0x0010</code> | Right-align each line within the rect |
| <code>AlignBottom</code> | <code>0x0020</code> | Vertically align the text block to the rect's bottom edge; also flips TextFormat::SkipLines from "skip from top" to "skip from bottom" |
| <code>KeepTail</code> | <code>0x0040</code> | When the text block is taller than the rect, render its tail (skip the leading overflowing lines) |
| <code>NoColorize</code> | <code>0x0080</code> | Strip inline color tags (@color:0x...@ / @color@), but render text with the base color as-is |
| <code>Justify</code> | <code>0x0100</code> | Justify each line: distribute extra spaces between words to fill the rect width |
| <code>Bordered</code> | <code>0x0200</code> | Render glyphs from the bordered/outlined font texture variant instead of the regular one |

## Layout rules

| Stable ID | Rule | Requirement | Why | Source |
| --- | --- | --- | --- | --- |
| <a id="entry-font-format-layout-text-format-80e94b41d3"></a><code>font-format.layout.text-format</code> | TextFormat value | Pass a Font slot, FontFlag bitmask, and nonnegative SkipLines count as TextFormat. | The exported value type is a fixed 12-byte layout consumed by measurement and drawing. | [Source/Client/FontManager.h](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.h) |
| <a id="entry-font-format-layout-wrap-overflow-56e4aad419"></a><code>font-format.layout.wrap-overflow</code> | Width overflow and wrapping | With finite width, default layout wraps at the latest space or tab and inserts a line break before an overlong token when no break point exists. | Layout mutates its cached text copy to establish line boundaries without changing the caller's source string. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-layout-no-wrap-and-truncate-a98195c11b"></a><code>font-format.layout.no-wrap-and-truncate</code> | NoWrap and TruncateLine | NoWrap ends draw text at the first width overflow; TruncateLine removes overflowing glyphs through the next authored newline. | These flags intentionally choose different loss behavior and should not be treated as synonyms. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-layout-horizontal-alignment-db87d0e726"></a><code>font-format.layout.horizontal-alignment</code> | Horizontal alignment | CenterX and AlignRight position each line independently inside the supplied rectangle; do not combine contradictory alignment flags. | The initial and every post-newline X coordinate are recalculated from that line's measured width. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-layout-vertical-alignment-e6cce90eb6"></a><code>font-format.layout.vertical-alignment</code> | Vertical alignment | CenterY centers the visible block and AlignBottom places it against the rectangle bottom using LineHeight and YAdvance. | Vertical placement depends on the number of lines that fit, not the total source-string byte length. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-layout-skip-lines-and-tail-1a695aec23"></a><code>font-format.layout.skip-lines-and-tail</code> | SkipLines and KeepTail | SkipLines removes leading lines by default and trailing lines with AlignBottom; KeepTail discards leading overflow so the newest visible lines remain. | These mechanisms serve pagination and log-tail behavior but use different counters and overflow conditions. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-layout-justification-280433804e"></a><code>font-format.layout.justification</code> | Justification | Justify distributes remaining finite rectangle width over breakable spaces on wrapped non-skipped lines. | Tabs are fixed at four SpaceWidth units and are not justification opportunities. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-layout-utf8-and-missing-glyphs-b65e93ffe1"></a><code>font-format.layout.utf8-and-missing-glyphs</code> | UTF-8 and missing glyphs | Author every required Unicode codepoint; invalid UTF-8 and absent glyphs consume no glyph width and render no fallback symbol. | The layout maps invalid sequences to codepoint zero and unknown codepoints to zero advance, while drawing skips missing map entries. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-layout-measurement-2c9e4eb598"></a><code>font-format.layout.measurement</code> | Measurement matches layout | Use Game.GetTextInfo and related line helpers with the same rectangle and TextFormat used for drawing, but do not infer draw-only NoWrap truncation from measurement. | Measurement and drawing share GetOrFormat, skips, line metrics, and bind-time scale; NoWrap truncation is intentionally gated to Draw mode. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp), [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp) |
