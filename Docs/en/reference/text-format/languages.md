---
title: Languages And Normalization
document_id: generated-text-format-languages
locale: en
generated: true
---

# Languages And Normalization

> Generated reference. Do not edit directly. Update `BuildTools/TextFormatInterface.json`, then run `python BuildTools/docs_text_format.py --write`.

[Index](index.md) | [Syntax](syntax.md) | [Languages](languages.md) | [Prototype text](proto-text.md) | [Runtime](runtime.md) | [Validation](validation.md) | [Canonical JSON](../../../generated/text-format.json) | [Guide](../../how-to/content/text-and-localization.md)

Language fallback is materialized by the bakers. Runtime lookup reads the selected binary language pack and does not consult the base pack.

## Engine defaults

| Setting | Source default | Meaning |
| --- | --- | --- |
| `Baking.BakeLanguages` | <code>engl</code> | Ordered output languages; the first is the normalization base. |
| `Client.Language` | <code>engl</code> | Initial current-language pack loaded by the client. |

| Stable ID | Rule | Requirement | Why | Source |
| --- | --- | --- | --- | --- |
| <a id="entry-text-format-language-filename-978417b676"></a><code>text-format.language.filename</code> | Source filename | A .fotxt basename contains exactly two dot-separated segments: text-pack name and language identifier. | TextBaker rejects every other basename shape. | [Source/Tools/TextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/TextBaker.cpp) |
| <a id="entry-text-format-language-identifier-ce551dd6b3"></a><code>text-format.language.identifier</code> | Language identifier | The Engine treats the filename suffix as an opaque configured string and imposes no fixed character count. | Acceptance is equality against Baking.BakeLanguages; no length or locale-shape validator exists in TextBaker. | [Source/Tools/TextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/TextBaker.cpp) |
| <a id="entry-text-format-language-base-c0360baaa0"></a><code>text-format.language.base</code> | Base language | Baking.BakeLanguages must be non-empty and its first entry is the base language used for normalization. | Both text bakers reject an empty list and TextPack::FixPacks normalizes every later language against the first pack set. | [Source/Tools/TextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/TextBaker.cpp), [Source/Common/TextPack.cpp](https://github.com/cvet/fonline/blob/master/Source/Common/TextPack.cpp) |
| <a id="entry-text-format-language-default-source-required-002dd0909e"></a><code>text-format.language.default-source-required</code> | Base source required | For every changed raw text pack, the selected file set must contain the base-language source. | TextBaker cannot normalize a changed pack without the base language and throws before parsing output. | [Source/Tools/TextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/TextBaker.cpp) |
| <a id="entry-text-format-language-incremental-pack-set-df1a474be2"></a><code>text-format.language.incremental-pack-set</code> | Incremental pack completion | When one language file of a text pack changes, TextBaker includes every configured language file for that same pack in the rebake. | A partial language update must still pass complete-pack normalization. | [Source/Tools/TextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/TextBaker.cpp), [Source/Tests/Test_TextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_TextBaker.cpp) |
| <a id="entry-text-format-language-unsupported-237d34519f"></a><code>text-format.language.unsupported</code> | Unsupported language | A raw text file whose suffix is absent from Baking.BakeLanguages is warned and skipped. | Unsupported raw languages do not create binary packs. | [Source/Tools/TextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/TextBaker.cpp) |
| <a id="entry-text-format-language-bake-time-fallback-7fa08c4b83"></a><code>text-format.language.bake-time-fallback</code> | Bake-time fallback | Missing language packs and missing keys are copied from the base language during baking; packs and keys absent from the base are removed from non-base languages. | Runtime lookup loads already-normalized binary packs and does not perform fallback. | [Source/Common/TextPack.cpp](https://github.com/cvet/fonline/blob/master/Source/Common/TextPack.cpp) |
| <a id="entry-text-format-language-variant-cardinality-5ec37b5527"></a><code>text-format.language.variant-cardinality</code> | Variant cardinality | Normalization aligns key presence, not the number or ordering of duplicate variants under an existing key. | FixStr tests only whether a key count is zero and preserves all existing variants when the key exists. | [Source/Common/TextPack.cpp](https://github.com/cvet/fonline/blob/master/Source/Common/TextPack.cpp) |
| <a id="entry-text-format-language-binary-name-b4b2342893"></a><code>text-format.language.binary-name</code> | Baked filename | Raw and prototype text bakers emit &lt;ResourcePack&gt;.&lt;TextPack&gt;.&lt;Language&gt;.fotxt-bin. | TextPack::LoadFromResources later requires exactly those three basename segments. | [Source/Tools/TextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/TextBaker.cpp), [Source/Common/TextPack.cpp](https://github.com/cvet/fonline/blob/master/Source/Common/TextPack.cpp) |
