---
title: Text Pack Syntax
document_id: generated-text-format-syntax
locale: en
generated: true
---

# Text Pack Syntax

> Generated reference. Do not edit directly. Update `BuildTools/TextFormatInterface.json`, then run `python BuildTools/docs_text_format.py --write`.

[Index](index.md) | [Syntax](syntax.md) | [Languages](languages.md) | [Prototype text](proto-text.md) | [Runtime](runtime.md) | [Validation](validation.md) | [Canonical JSON](../../../generated/text-format.json) | [Guide](../../how-to/content/text-and-localization.md)

A raw text file supplies the collection through its filename. Each parsed logical entry supplies Key1, Key2, and Text:

```text
{Welcome}{}{Welcome to the wasteland.}
{QuestName}{Short}{A difficult choice}
{LongMessage}{}{First line
Second line}
```

Raw `.fotxt` does not author Key3. Use prototype `$Text` fields when a prototype-owned key needs both Key2 and Key3.

| Stable ID | Rule | Requirement | Why | Source |
| --- | --- | --- | --- | --- |
| <a id="entry-text-format-syntax-entry-b9ee5d2d47"></a><code>text-format.syntax.entry</code> | Raw entry shape | A parsed .fotxt entry contains Key1, Key2, and Text in three brace-delimited fields; the collection is the text-pack filename segment. | LoadFromString supplies the collection separately and constructs TextPackKey from the two authored key fields. | [Source/Common/TextPack.cpp](https://github.com/cvet/fonline/blob/master/Source/Common/TextPack.cpp) |
| <a id="entry-text-format-syntax-key-tuple-fa1c37769d"></a><code>text-format.syntax.key-tuple</code> | Structured key identity | TextPackKey identity is Collection plus Key1, Key2, and Key3, each represented as a hashed string wrapper. | Raw .fotxt entries leave Key3 empty, while prototype $Text fields may populate Key2 and Key3. | [Source/Common/TextPack.h](https://github.com/cvet/fonline/blob/master/Source/Common/TextPack.h) |
| <a id="entry-text-format-syntax-multiline-text-cd9358a779"></a><code>text-format.syntax.multiline-text</code> | Multiline value | Only the third field may continue across physical lines; joined lines retain newline characters until the first closing brace. | The first two ExtractBraceToken calls disable multiline input and the third enables it. | [Source/Common/TextPack.cpp](https://github.com/cvet/fonline/blob/master/Source/Common/TextPack.cpp) |
| <a id="entry-text-format-syntax-comment-boundary-f69ad9c0ed"></a><code>text-format.syntax.comment-boundary</code> | No comment grammar | The parser skips a physical line only when it cannot find the first opening brace; comment conventions are project policy and comment text must not contain parseable brace fields. | LoadFromString has no comment-token branch and begins parsing at the first opening brace anywhere on the line. | [Source/Common/TextPack.cpp](https://github.com/cvet/fonline/blob/master/Source/Common/TextPack.cpp) |
| <a id="entry-text-format-syntax-empty-fields-e47e09fc8e"></a><code>text-format.syntax.empty-fields</code> | Required and optional fields | The collection and Key1 must be non-empty; Key2 and Text may be empty. | The parser rejects only an empty collection or first key after all three fields were extracted. | [Source/Common/TextPack.cpp](https://github.com/cvet/fonline/blob/master/Source/Common/TextPack.cpp) |
| <a id="entry-text-format-syntax-variants-a3b452dc5d"></a><code>text-format.syntax.variants</code> | Duplicate-key variants | Multiple entries may share the same complete TextPackKey and remain adjacent ordered variants in the sorted backing vector. | Variant count and indexed selection use one binary-search range; duplicate cardinality is not language-normalized and read paths never repair shared state. | [Source/Common/TextPack.h](https://github.com/cvet/fonline/blob/master/Source/Common/TextPack.h), [Source/Tests/Test_TextPack.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_TextPack.cpp) |
| <a id="entry-text-format-syntax-no-closing-brace-escape-5b8f40ba36"></a><code>text-format.syntax.no-closing-brace-escape</code> | Closing-brace boundary | The first closing brace terminates each field; the raw format has no escape sequence for a literal closing brace inside Text. | ExtractBraceToken searches directly for the next closing brace and does not decode escapes. | [Source/Common/TextPack.cpp](https://github.com/cvet/fonline/blob/master/Source/Common/TextPack.cpp) |
