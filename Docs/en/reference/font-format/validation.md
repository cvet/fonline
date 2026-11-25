---
title: Font Validation Contract
document_id: generated-font-format-validation
locale: en
generated: true
---

# Font Validation Contract

> Generated reference. Do not edit directly. Update `BuildTools/FontFormatInterface.json`, then run `python BuildTools/docs_font_format.py --write`.

[Index](index.md) | [Formats](formats.md) | [Binding](binding.md) | [Authoring](authoring.md) | [Layout](layout.md) | [Rendering](rendering.md) | [Validation](validation.md) | [Canonical JSON](../../../generated/font-format.json) | [Guide](../../how-to/content/font-format.md)

| Stable ID | Rule | Requirement | Why | Source |
| --- | --- | --- | --- | --- |
| <a id="entry-font-format-validation-face-loading-f937748005"></a><code>font-format.validation.face-loading</code> | Face loading failures | Expect a bind to throw, and keep the previous font, when a face file is missing, FreeType cannot open it as a face (a .fofnt, .fnt, or unsupported container), it has no Unicode character map, or it cannot be set to the pixel size. | The font manager reports a missing resource and the rasterizer reports each FreeType refusal with its own fixed message before any state changes. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp), [Source/Client/FontRasterizer.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontRasterizer.cpp) |
| <a id="entry-font-format-validation-default-font-presence-2b4d5ccbe0"></a><code>font-format.validation.default-font-presence</code> | Default font in every runtime rig | Provide Fonts/FOnline-Regular.ttf in any resource set that starts a client, the Mapper, or the updater, including test rigs, and keep it out of unused-resource pruning. | Startup binds the default font by path and throws when it is missing, and the resource usage collector adds the default font as a referenced resource. | [Source/Tools/ResourceUsage.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ResourceUsage.cpp), [Source/Tests/Test_BakerHelpers.h](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_BakerHelpers.h), [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-validation-converter-current-d7a6288548"></a><code>font-format.validation.converter-current</code> | Converted faces are current | Run python BuildTools/bitmap_font_to_ttf.py --manifest Resources/FontSources/Fonts.json --check and python BuildTools/tests/test_bitmap_font_to_ttf.py after changing a bitmap source or the converter; every committed face must equal a fresh conversion. | A face is generated data, so a hand edit or a stale conversion would otherwise ship unnoticed. | [BuildTools/bitmap_font_to_ttf.py](https://github.com/cvet/fonline/blob/master/BuildTools/bitmap_font_to_ttf.py), [BuildTools/tests/test_bitmap_font_to_ttf.py](https://github.com/cvet/fonline/blob/master/BuildTools/tests/test_bitmap_font_to_ttf.py) |
| <a id="entry-font-format-validation-generated-contract-40d35172b7"></a><code>font-format.validation.generated-contract</code> | Generated contract drift | Regenerate and check the font-format model whenever the face binding, the default font, raw-copy defaults, colored-face handling, converter constants, font enums, cache, or shipped faces change. | The checked model makes silent source/documentation drift fail CI. | [BuildTools/docs_font_format.py](https://github.com/cvet/fonline/blob/master/BuildTools/docs_font_format.py) |
| <a id="entry-font-format-validation-engine-tests-0ba5ebbb02"></a><code>font-format.validation.engine-tests</code> | Engine regression gates | Run the focused documentation test and the full generated Engine unit-test target after FontManager, FontRasterizer, or default-font changes. | Structural checks pin source-derived contracts while native tests cover face rasterization, colored faces, glyph sheets, and client construction paths. | [Source/Tests/Test_FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_FontManager.cpp), [Source/Tests/Test_FontRasterizer.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_FontRasterizer.cpp) |
| <a id="entry-font-format-validation-embedding-project-3dcc84a5cf"></a><code>font-format.validation.embedding-project</code> | Embedding-project bake and visible check | Bake the faces, run measurement tests for every bound pixel size, and visibly inspect regular, bordered, colored, wrapped, aligned, localized, and missing-glyph cases. | A raw-copy success cannot prove glyph coverage, hinting, typography, backend rendering, or GUI fit. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp), [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |

## Validation commands

```powershell
python BuildTools\docs_font_format.py --check
python -m unittest BuildTools.tests.test_docs_font_format
python BuildTools\bitmap_font_to_ttf.py --manifest Resources\FontSources\Fonts.json --check
python -m unittest BuildTools.tests.test_bitmap_font_to_ttf
cmake --build <build-dir> --config RelWithDebInfo --target RunUnitTests
```

An embedding project must also bake the faces, run its text-measurement regression, and inspect representative regular, bordered, colored, wrapped, and localized text in a visible client.
