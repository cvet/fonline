---
title: Text Format Validation
document_id: generated-text-format-validation
locale: en
generated: true
---

# Text Format Validation

> Generated reference. Do not edit directly. Update `BuildTools/TextFormatInterface.json`, then run `python BuildTools/docs_text_format.py --write`.

[Index](index.md) | [Syntax](syntax.md) | [Languages](languages.md) | [Prototype text](proto-text.md) | [Runtime](runtime.md) | [Validation](validation.md) | [Canonical JSON](../../../generated/text-format.json) | [Guide](../../how-to/content/text-and-localization.md)

| Stable ID | Rule | Requirement | Why | Source |
| --- | --- | --- | --- | --- |
| <a id="entry-text-format-validation-malformed-raw-d5a8fc093b"></a><code>text-format.validation.malformed-raw</code> | Malformed raw text | A raw source with a missing required field or unterminated value fails the text bake even if other entries parsed successfully. | LoadFromString reports aggregate failure and TextBaker rejects the complete source file. | [Source/Common/TextPack.cpp](https://github.com/cvet/fonline/blob/master/Source/Common/TextPack.cpp), [Source/Tools/TextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/TextBaker.cpp), [Source/Tests/Test_TextPack.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_TextPack.cpp) |
| <a id="entry-text-format-validation-empty-languages-e0d19568f1"></a><code>text-format.validation.empty-languages</code> | Empty BakeLanguages | TextBaker, ProtoTextBaker, and TextPack normalization reject an empty Baking.BakeLanguages list. | No deterministic base language can be selected. | [Source/Tools/TextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/TextBaker.cpp), [Source/Tools/ProtoTextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ProtoTextBaker.cpp), [Source/Common/TextPack.cpp](https://github.com/cvet/fonline/blob/master/Source/Common/TextPack.cpp) |
| <a id="entry-text-format-validation-raw-unsupported-warning-869bf64e2b"></a><code>text-format.validation.raw-unsupported-warning</code> | Unsupported raw language warning | Unsupported raw language files produce a warning and no output for that language. | Projects can keep unrelated sources in a broader input tree without silently publishing them. | [Source/Tools/TextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/TextBaker.cpp) |
| <a id="entry-text-format-validation-proto-token-count-4cb7b434c3"></a><code>text-format.validation.proto-token-count</code> | Prototype key token count | A prototype $Text key with more than Language, Key2, and Key3 after $Text fails baking. | TextPackKey has no additional key field. | [Source/Tools/ProtoTextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ProtoTextBaker.cpp), [Source/Tests/Test_ProtoTextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_ProtoTextBaker.cpp) |
| <a id="entry-text-format-validation-proto-intersection-be92726636"></a><code>text-format.validation.proto-intersection</code> | Prototype output intersection | Duplicate complete keys produced by different prototype type sources in one output pack fail baking. | The generated pack must not depend on iteration order across type catalogs. | [Source/Tools/ProtoTextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ProtoTextBaker.cpp), [Source/Tests/Test_ProtoTextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_ProtoTextBaker.cpp) |

## Validation commands

```powershell
python BuildTools\docs_text_format.py --check
python -m unittest BuildTools.tests.test_docs_text_format
cmake --build <build-dir> --config RelWithDebInfo --target RunUnitTests
```

In an embedding project, finish with its resource bake, localization guards, and a visible client check for language switching and formatted text that the project itself owns.
