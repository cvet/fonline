---
title: Prototype Text
document_id: generated-text-format-proto-text
locale: en
generated: true
---

# Prototype Text

> Generated reference. Do not edit directly. Update `BuildTools/TextFormatInterface.json`, then run `python BuildTools/docs_text_format.py --write`.

[Index](index.md) | [Syntax](syntax.md) | [Languages](languages.md) | [Prototype text](proto-text.md) | [Runtime](runtime.md) | [Validation](validation.md) | [Canonical JSON](../../../generated/text-format.json) | [Guide](../../how-to/content/text-and-localization.md)

Prototype-localized text is authored inside any valid prototype section:

```ini
[ProtoItem]
$Name = LaserRifle
$Text engl Name = Laser rifle
$Text engl Desc Short = Compact description
$Text russ Name = Localized name
```

The complete key is the generated pack, prototype id, optional Key2, and optional Key3. Omitting the language selects the first configured BakeLanguages entry.

## Generated packs

<code>Items</code>, <code>Critters</code>, <code>Maps</code>, <code>Locations</code>, <code>Protos</code>

| Stable ID | Rule | Requirement | Why | Source |
| --- | --- | --- | --- | --- |
| <a id="entry-text-format-proto-syntax-712e1b0662"></a><code>text-format.proto.syntax</code> | Prototype text key | Prototype text uses $Text [Language] [Key2] [Key3] = Value and accepts at most four key tokens including $Text. | The prototype id becomes Key1 and the optional trailing tokens become Key2 and Key3. | [Source/Tools/ProtoTextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ProtoTextBaker.cpp) |
| <a id="entry-text-format-proto-default-language-38c15ef84b"></a><code>text-format.proto.default-language</code> | Omitted language | When the language token is omitted, a $Text field belongs to the first Baking.BakeLanguages entry. | The same ordered base-language contract is shared with raw text packs. | [Source/Tools/ProtoTextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ProtoTextBaker.cpp) |
| <a id="entry-text-format-proto-escape-decoding-c7c388d194"></a><code>text-format.proto.escape-decoding</code> | Escaped value decoding | Prototype $Text values pass through StringEscaping::DecodeString before entering the text pack. | Sequences such as \n become actual newline characters in the baked value. | [Source/Tools/ProtoTextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ProtoTextBaker.cpp), [Source/Tests/Test_ProtoTextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_ProtoTextBaker.cpp) |
| <a id="entry-text-format-proto-inheritance-bf3c06135b"></a><code>text-format.proto.inheritance</code> | Inherited text | Parent $Text fields are merged recursively before the child fields. | A child inherits missing exact text keys while its own exact key assignment replaces the inherited value. | [Source/Tools/ProtoTextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ProtoTextBaker.cpp) |
| <a id="entry-text-format-proto-pack-routing-e03d4dcd20"></a><code>text-format.proto.pack-routing</code> | Generated pack routing | Item, Critter, Map, and Location prototypes route to Items, Critters, Maps, and Locations; other non-exported HasProtos entity or fixed types route to Protos. | These five names are the reusable Engine-generated prototype text-pack set. | [Source/Tools/ProtoTextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ProtoTextBaker.cpp) |
| <a id="entry-text-format-proto-complete-output-set-fafcdb598b"></a><code>text-format.proto.complete-output-set</code> | Complete output set | ProtoTextBaker creates all five generated packs for every configured language, including empty packs. | The binary output shape is deterministic across projects and language sets. | [Source/Tools/ProtoTextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ProtoTextBaker.cpp) |
| <a id="entry-text-format-proto-intersections-9cf6753839"></a><code>text-format.proto.intersections</code> | Cross-type intersections | Two prototype sources may not emit the same complete key into one generated pack and language. | Intersections are counted as errors and abort prototype text baking. | [Source/Tools/ProtoTextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ProtoTextBaker.cpp) |
| <a id="entry-text-format-proto-unsupported-language-b3301197c5"></a><code>text-format.proto.unsupported-language</code> | Unsupported prototype language | A $Text language absent from Baking.BakeLanguages is warned and omitted. | Prototype-localized text follows the configured output-language set. | [Source/Tools/ProtoTextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ProtoTextBaker.cpp) |
