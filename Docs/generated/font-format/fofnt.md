---
title: FOFNT Field Reference
document_id: generated-font-format-fofnt
locale: en
generated: true
---

# FOFNT Field Reference

> Generated reference. Do not edit directly. Update `BuildTools/FontFormatInterface.json`, then run `python BuildTools/docs_font_format.py --write`.

[Index](index.md) | [Formats](formats.md) | [FOFNT](fofnt.md) | [BMFont](bmfont.md) | [Binding](binding.md) | [Layout](layout.md) | [Rendering](rendering.md) | [Validation](validation.md) | [Canonical JSON](../font-format.json) | [Guide](../../FontFormat.md)

| Stable ID | Key | Syntax | Behavior | Source |
| --- | --- | --- | --- | --- |
| <a id="entry-font-format-fofnt-version-bac978e025"></a><code>font-format.fofnt.version</code> | Version | <code>Version &lt;integer&gt;</code> | Make Version the first parsed key; author version 2 for current resources. Values greater than 2 are rejected. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-fofnt-image-a02f11cb08"></a><code>font-format.fofnt.image</code> | Image | <code>Image &lt;relative-resource&gt;[*]</code> | Name a non-empty image resource relative to the descriptor; append * to request grayscale normalization and runtime tinting. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-fofnt-line-height-3ec623f1ea"></a><code>font-format.fofnt.line-height</code> | LineHeight | <code>LineHeight &lt;integer-pixels&gt;</code> | Set the glyph-line height in pixels, or leave it zero/omitted to derive the maximum authored glyph height. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-fofnt-y-advance-71ad32b001"></a><code>font-format.fofnt.y-advance</code> | YAdvance | <code>YAdvance &lt;integer-pixels&gt;</code> | Set the extra vertical gap added between consecutive text lines. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-fofnt-letter-7e4bc3a9d5"></a><code>font-format.fofnt.letter</code> | Letter | <code>Letter '&lt;UTF-8-codepoint&gt;'</code> | Start each glyph record with Letter and one valid UTF-8 codepoint after the first apostrophe. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-fofnt-position-x-01b533225c"></a><code>font-format.fofnt.position-x</code> | PositionX | <code>PositionX &lt;integer-pixels&gt;</code> | Set the glyph rectangle's left coordinate in the image. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-fofnt-position-y-56c8405277"></a><code>font-format.fofnt.position-y</code> | PositionY | <code>PositionY &lt;integer-pixels&gt;</code> | Set the glyph rectangle's top coordinate in the image. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-fofnt-width-916e35ae75"></a><code>font-format.fofnt.width</code> | Width | <code>Width &lt;integer-pixels&gt;</code> | Set the visible glyph rectangle width, excluding the one-pixel sampling border. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-fofnt-height-01ca83487f"></a><code>font-format.fofnt.height</code> | Height | <code>Height &lt;integer-pixels&gt;</code> | Set the visible glyph rectangle height, excluding the one-pixel sampling border. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-fofnt-offset-x-fcaee963d3"></a><code>font-format.fofnt.offset-x</code> | OffsetX | <code>OffsetX &lt;signed-integer-pixels&gt;</code> | Set the signed horizontal bearing in Engine coordinates; drawing places the quad at cursor X minus OffsetX. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-fofnt-offset-y-85bff2a932"></a><code>font-format.fofnt.offset-y</code> | OffsetY | <code>OffsetY &lt;signed-integer-pixels&gt;</code> | Set the signed vertical bearing in Engine coordinates; drawing places the quad at cursor Y minus OffsetY. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-fofnt-x-advance-51728242c9"></a><code>font-format.fofnt.x-advance</code> | XAdvance | <code>XAdvance &lt;signed-integer-pixels&gt;</code> | Set the horizontal cursor advance after the glyph; include an explicit space glyph when spaces must consume width. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-fofnt-end-and-comments-3a52ef092d"></a><code>font-format.fofnt.end-and-comments</code> | End and comments | <code>End &#124; #comment &#124; ;comment</code> | Terminate the useful descriptor with End; # and ; begin comments only when encountered inside the whitespace-delimited key token. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
