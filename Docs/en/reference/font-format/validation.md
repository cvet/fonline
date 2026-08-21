---
title: Font Validation Contract
document_id: generated-font-format-validation
locale: en
generated: true
---

# Font Validation Contract

> Generated reference. Do not edit directly. Update `BuildTools/FontFormatInterface.json`, then run `python BuildTools/docs_font_format.py --write`.

[Index](index.md) | [Formats](formats.md) | [FOFNT](fofnt.md) | [BMFont](bmfont.md) | [Binding](binding.md) | [Layout](layout.md) | [Rendering](rendering.md) | [Validation](validation.md) | [Canonical JSON](../../../generated/font-format.json) | [Guide](../../how-to/content/font-format.md)

| Stable ID | Rule | Requirement | Why | Source |
| --- | --- | --- | --- | --- |
| <a id="entry-font-format-validation-descriptor-and-image-presence-aa2fae5bd3"></a><code>font-format.validation.descriptor-and-image-presence</code> | Descriptor and image presence | Fail the asset gate when the descriptor is missing, FOFNT omits Image, or the relative image cannot load as an atlas sprite. | Each condition throws a distinct FontManagerException during binding. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-validation-fofnt-header-3b9e8b7f37"></a><code>font-format.validation.fofnt-header</code> | FOFNT header and UTF-8 | Reject a FOFNT whose first key is not Version, whose version exceeds 2, or whose Letter line does not contain one valid UTF-8 codepoint. | These are hard parser failures rather than recoverable missing-glyph cases. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-validation-bmfont-header-aa7522bad1"></a><code>font-format.validation.bmfont-header</code> | BMFont header, padding, and pages | Reject BMFont descriptors that are not binary v3, do not use 1/1/1/1 padding, or declare any page count other than one. | The runtime has explicit exceptions for all three incompatibilities. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-validation-signed-bmfont-metrics-bf09e269db"></a><code>font-format.validation.signed-bmfont-metrics</code> | Signed BMFont metric regression | Keep xoffset, yoffset, and xadvance on GetLEInt16 and test against bundled binary fonts that contain negative bearings. | Unsigned reads turn values such as -2 into 65534 and move rendered glyphs far outside their intended position. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp), [Source/Common/FileSystem.h](https://github.com/cvet/fonline/blob/master/Source/Common/FileSystem.h) |
| <a id="entry-font-format-validation-scale-range-ab30f43b55"></a><code>font-format.validation.scale-range</code> | Scale range | Reject NaN, infinity, zero, negative values, and values greater than one before mutating the font table or atlas. | The Engine supports deterministic bind-time downscaling, not bitmap upscaling. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-validation-generated-contract-40d35172b7"></a><code>font-format.validation.generated-contract</code> | Generated contract drift | Regenerate and check the font-format model whenever parser keys, binary constants, font enums, binding dispatch, raw-copy defaults, scale, cache, or bundled descriptors change. | The checked model makes silent source/documentation drift fail CI. | [BuildTools/docs_font_format.py](https://github.com/cvet/fonline/blob/master/BuildTools/docs_font_format.py) |
| <a id="entry-font-format-validation-engine-tests-0ba5ebbb02"></a><code>font-format.validation.engine-tests</code> | Engine regression gates | Run the focused documentation test and the full generated Engine unit-test target after FontManager or font descriptor changes. | Structural checks pin source-derived contracts while native tests cover resource and client construction paths. | [Source/Tests/Test_Mapper.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_Mapper.cpp), [Source/Tests/Test_ClientServerIntegration.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_ClientServerIntegration.cpp) |
| <a id="entry-font-format-validation-embedding-project-3dcc84a5cf"></a><code>font-format.validation.embedding-project</code> | Embedding-project bake and visible check | Bake descriptor and image resources, run measurement tests for every bound scale, and visibly inspect regular, bordered, wrapped, aligned, localized, and missing-glyph cases. | A raw-copy success cannot prove glyph coverage, atlas padding, typography, backend rendering, or GUI fit. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp), [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |

## Validation commands

```powershell
python BuildTools\docs_font_format.py --check
python -m unittest BuildTools.tests.test_docs_font_format
cmake --build <build-dir> --config RelWithDebInfo --target RunUnitTests
```

An embedding project must also bake the descriptor and image together, run its text-measurement regression, and inspect representative regular, bordered, scaled, wrapped, and localized text in a visible client.
