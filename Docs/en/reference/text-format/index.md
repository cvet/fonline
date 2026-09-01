---
title: Generated Text And Localization Reference
document_id: generated-text-format-index
locale: en
generated: true
---

# Generated Text And Localization Reference

> Generated reference. Do not edit directly. Update `BuildTools/TextFormatInterface.json`, then run `python BuildTools/docs_text_format.py --write`.

[Index](index.md) | [Syntax](syntax.md) | [Languages](languages.md) | [Prototype text](proto-text.md) | [Runtime](runtime.md) | [Validation](validation.md) | [Canonical JSON](../../../generated/text-format.json) | [Guide](../../how-to/content/text-and-localization.md)

This reference describes the reusable Engine-owned text-pack, language, prototype-text, runtime lookup, and inline color contract. Concrete game pack catalogs and formatting lexems remain project-owned.

## Contract status

| Field | Value |
| --- | --- |
| Stability | <code>experimental</code> |
| Support policy | The contract is generated for a pinned Engine revision. Projects own language policy, pack catalogs, translation workflow, semantic key conventions, and any game-specific lexem or argument formatter. |
| Source manifest | [BuildTools/TextFormatInterface.json](https://github.com/cvet/fonline/blob/master/BuildTools/TextFormatInterface.json) |
| Contract digest | <code>80ed40f35a558a8a45189c0f267066579a507a44ed72c94206ef41c83640a196</code> |
| Source filename | <code>&lt;TextPack&gt;.&lt;Language&gt;.fotxt</code> |
| Baked filename | <code>&lt;ResourcePack&gt;.&lt;TextPack&gt;.&lt;Language&gt;.fotxt-bin</code> |
| Raw entry | <code>&#123;Key1&#125;&#123;Key2&#125;&#123;Text&#125;</code> |

| Reference | Entries | Purpose |
| --- | --- | --- |
| [Syntax](syntax.md) | 7 | Raw brace fields, key identity, multiline text, and variants. |
| [Languages](languages.md) | 10 | Filename selection, defaults, rebakes, and normalization. |
| [Prototype text](proto-text.md) | 8 | $Text grammar, inheritance, pack routing, and decoding. |
| [Runtime](runtime.md) | 8 methods / 2 rendering rules | Script lookup, language switching, server boundary, and color tags. |

## Boundary

Included:

- .fotxt file naming, brace-field syntax, multiline values, structured keys, and duplicate variants
- BakeLanguages ordering, incremental rebakes, bake-time language normalization, and binary output names
- $Text prototype fields, inheritance, output-pack routing, escape decoding, and validation
- client and server script lookup surfaces, language switching, and inline renderer color tags

Excluded:

- project language priorities, translation ownership, terminology, and release-readiness policy
- project-specific lexems, argument substitution, random-choice syntax, dialog formatting, and chat wrappers
- concrete pack names beyond the five Engine-generated prototype packs
- font selection, layout, shaping, and general GUI authoring
