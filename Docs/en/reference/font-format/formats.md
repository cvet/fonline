---
title: Font Resource Formats
document_id: generated-font-format-formats
locale: en
generated: true
---

# Font Resource Formats

> Generated reference. Do not edit directly. Update `BuildTools/FontFormatInterface.json`, then run `python BuildTools/docs_font_format.py --write`.

[Index](index.md) | [Formats](formats.md) | [Binding](binding.md) | [Authoring](authoring.md) | [Layout](layout.md) | [Rendering](rendering.md) | [Validation](validation.md) | [Canonical JSON](../../../generated/font-format.json) | [Guide](../../how-to/content/font-format.md)

| Stable ID | Suffix | Role | Contract | Source |
| --- | --- | --- | --- | --- |
| <a id="entry-font-format-format-truetype-3db4652bb8"></a><code>font-format.format.truetype</code> | <code>.ttf</code> | The only runtime text format, rasterized by FreeType | Ship every text face as a TrueType font and bind it through the face-list Game.BindFont overload; the client draws text from nothing else. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp), [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-format-truetype-colored-bda5c5725e"></a><code>font-format.format.truetype-colored</code> | <code>.ttf</code> | TrueType face with COLR version 0 color layers over a CPAL palette | Author a multi-color face as COLR v0 layers with a CPAL palette; the client draws it with its own colors and multiplies them by the text color. | [ThirdParty/freetype/include/freetype/config/ftoption.h](https://github.com/cvet/fonline/blob/master/ThirdParty/freetype/include/freetype/config/ftoption.h), [Source/Client/FontRasterizer.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontRasterizer.cpp) |
| <a id="entry-font-format-format-bitmap-source-352a427410"></a><code>font-format.format.bitmap-source</code> | <code>.fofnt, .fnt</code> | Converter input only; the runtime does not load them | Keep an authored bitmap font as a .fofnt text descriptor with its image, or a binary BMFont v3 .fnt, outside every resource pack, and convert it to a TrueType face with the converter before shipping. | [BuildTools/bitmap_font_to_ttf.py](https://github.com/cvet/fonline/blob/master/BuildTools/bitmap_font_to_ttf.py), [Source/Client/FontRasterizer.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontRasterizer.cpp) |

## Shipped faces

| Pack | Faces |
| --- | --- |
| <code>Embedded</code> | <code>FOnline-Regular.ttf</code> |
| <code>Core</code> | <code>FOnline-Big.ttf</code>, <code>FOnline-BigNumbers.ttf</code>, <code>FOnline-Fat.ttf</code>, <code>FOnline-Numbers.ttf</code>, <code>FOnline-Old.ttf</code>, <code>FOnline-SandNumbers.ttf</code>, <code>FOnline-Special.ttf</code>, <code>FOnline-Thin.ttf</code> |

## Colored shipped faces

| Tables | Faces |
| --- | --- |
| <code>COLR</code>, <code>CPAL</code> | <code>FOnline-BigNumbers.ttf</code>, <code>FOnline-Numbers.ttf</code>, <code>FOnline-SandNumbers.ttf</code> |
