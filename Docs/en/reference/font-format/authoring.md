---
title: Font Authoring Contract
document_id: generated-font-format-authoring
locale: en
generated: true
---

# Font Authoring Contract

> Generated reference. Do not edit directly. Update `BuildTools/FontFormatInterface.json`, then run `python BuildTools/docs_font_format.py --write`.

[Index](index.md) | [Formats](formats.md) | [Binding](binding.md) | [Authoring](authoring.md) | [Layout](layout.md) | [Rendering](rendering.md) | [Validation](validation.md) | [Canonical JSON](../../../generated/font-format.json) | [Guide](../../how-to/content/font-format.md)

## Converter facts

| Field | Value |
| --- | --- |
| Source formats | <code>.fofnt</code>, <code>.fnt</code> |
| Font units per bitmap pixel | <code>100</code> |
| Native size range | 1..255 px |
| Neutral glyph grey | <code>128</code> |
| Manifest fonts | <code>9</code> |
| Python packages | <code>fontTools</code>, <code>numpy</code>, <code>PIL</code>, <code>potrace</code> |

## Authoring rules

| Stable ID | Rule | Requirement | Why | Source |
| --- | --- | --- | --- | --- |
| <a id="entry-font-format-authoring-converter-d7b0700993"></a><code>font-format.authoring.converter</code> | Bitmap-font converter | Convert an authored bitmap font with python BuildTools/bitmap_font_to_ttf.py SOURCE OUTPUT.ttf --family NAME; it needs fontTools, Pillow, numpy, and potracer, and the runtime and the documentation gates need none of them. | The converter is the only reader of .fofnt and BMFont v3 sources; it writes a TrueType face whose output is byte-identical between runs. | [BuildTools/bitmap_font_to_ttf.py](https://github.com/cvet/fonline/blob/master/BuildTools/bitmap_font_to_ttf.py) |
| <a id="entry-font-format-authoring-native-size-metrics-0e220541ef"></a><code>font-format.authoring.native-size-metrics</code> | Native size and metrics | Bind a converted face at its native pixel size, the source line height plus its line gap, to reproduce the bitmap layout; the face carries 100 font units per bitmap pixel, and a native size outside 1..255 is rejected. | The converter writes units per em as the native size times the pixel unit and refuses a size that an embedded bitmap strike cannot carry. | [BuildTools/bitmap_font_to_ttf.py](https://github.com/cvet/fonline/blob/master/BuildTools/bitmap_font_to_ttf.py) |
| <a id="entry-font-format-authoring-single-color-0af62a9423"></a><code>font-format.authoring.single-color</code> | Single-color fonts | Expect a single-color source to become an 8-bit EBDT/EBLC bitmap strike at the native size, which FreeType draws pixel for pixel, plus outlines traced from the glyph coverage for every other size; a no-op prep program keeps FreeType from auto-hinting the face. | The strike preserves the authored pixels at the native size in every raster mode, while the traced outlines let the same face scale to other pixel sizes. | [BuildTools/bitmap_font_to_ttf.py](https://github.com/cvet/fonline/blob/master/BuildTools/bitmap_font_to_ttf.py) |
| <a id="entry-font-format-authoring-multi-color-d867c9c701"></a><code>font-format.authoring.multi-color</code> | Multi-color fonts | Expect a source with more than the neutral grey to become COLR v0 layers over a CPAL palette, one layer per distinct pixel color made of whole-pixel rectangles, with traced silhouettes as the base glyphs. | Color layers keep the authored colors exact at the native size and scale the pixel grid at other sizes, and renderers without color support still draw the silhouettes. | [BuildTools/bitmap_font_to_ttf.py](https://github.com/cvet/fonline/blob/master/BuildTools/bitmap_font_to_ttf.py) |
| <a id="entry-font-format-authoring-skipped-codepoints-3eee9d8e9a"></a><code>font-format.authoring.skipped-codepoints</code> | Skipped control codes | Do not expect glyphs for C0 and C1 control codes in a converted face; give the source an explicit space letter, because the client refuses a face chain without one. | Control codes are layout commands for the engine, so the converter skips them, while the font manager requires the space letter to measure and draw text. | [BuildTools/bitmap_font_to_ttf.py](https://github.com/cvet/fonline/blob/master/BuildTools/bitmap_font_to_ttf.py), [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-authoring-manifest-c39b2b6008"></a><code>font-format.authoring.manifest</code> | Source manifest and staleness check | Register every converted face in Resources/FontSources/Fonts.json and run python BuildTools/bitmap_font_to_ttf.py --manifest Resources/FontSources/Fonts.json to regenerate them; add --check to fail when a committed face differs from a fresh conversion. | The manifest lists each source, output path, family, and style relative to itself, and --check compares output bytes, which is stable because timestamps are fixed. | [BuildTools/bitmap_font_to_ttf.py](https://github.com/cvet/fonline/blob/master/BuildTools/bitmap_font_to_ttf.py), [Resources/FontSources/Fonts.json](https://github.com/cvet/fonline/blob/master/Resources/FontSources/Fonts.json) |
| <a id="entry-font-format-authoring-sources-not-packed-7c85123d50"></a><code>font-format.authoring.sources-not-packed</code> | Sources stay outside resource packs | Keep bitmap sources in Resources/FontSources, which no resource pack includes; only the generated faces ship, from Resources/Embedded/Fonts for the default font and Resources/Core/Fonts for the others. | The manifest writes faces into the Embedded and Core font directories, and the runtime has no use for the source descriptors or their images. | [Resources/FontSources/Fonts.json](https://github.com/cvet/fonline/blob/master/Resources/FontSources/Fonts.json) |

## Converter commands

```powershell
python BuildTools\bitmap_font_to_ttf.py SOURCE.fofnt OUTPUT.ttf --family "Name"
python BuildTools\bitmap_font_to_ttf.py --manifest Resources\FontSources\Fonts.json
python BuildTools\bitmap_font_to_ttf.py --manifest Resources\FontSources\Fonts.json --check
```

Converted faces are generated data: edit the bitmap source or the converter, regenerate through the manifest, and never edit a face by hand. The `--check` mode fails when a committed face differs from a fresh conversion.
