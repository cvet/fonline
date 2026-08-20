---
title: Generated Prototype Format Reference
document_id: generated-prototype-format-index
locale: en
generated: true
---

# Generated Prototype Format Reference

> Generated reference. Do not edit directly. Update `BuildTools/PrototypeFormatInterface.json` or the owning engine metadata, then run `python BuildTools/docs_prototype_format.py --write`.

[Index](index.md) | [Syntax](syntax.md) | [Properties](properties.md) | [Validation](validation.md) | [Canonical JSON](../../../generated/prototype-format.json) | [Authoring guide](../../how-to/content/prototype-format.md)

This reference describes the engine-owned prototype grammar and the built-in metadata available to any embedding project at this engine revision.

## Contract status

| Field | Value |
| --- | --- |
| Stability | <code>experimental</code> |
| Support policy | The grammar is documented for a pinned engine revision. Concrete project entity types, properties, file extensions, ids, and gameplay semantics remain project-owned. |
| Source manifest | [BuildTools/PrototypeFormatInterface.json](https://github.com/cvet/fonline/blob/master/BuildTools/PrototypeFormatInterface.json) |
| Contract digest | <code>38b76342c3494988f8fa27a2715cbe09b9e02937d9d71e9309f2e90eae23c4f5</code> |

| Reference | Entries | Purpose |
| --- | --- | --- |
| [Syntax](syntax.md) | 5 | Discovery, sections, identity, and inheritance. |
| [Properties](properties.md) | 113 | Built-in HasProtos types and engine-owned property keys. |
| [Validation](validation.md) | 13 | Source-backed bake and migration requirements. |

## Boundary

Included:

- prototype file discovery and side-specific bake outputs
- section forms, prototype identity, inheritance, and migration
- property applicability and strict text-value validation
- built-in InitScript callback signature validation
- built-in HasProtos entity types and their engine-owned property catalog

Excluded:

- project-authored entity and FixedType declarations or properties
- nested map placement sections addressed as $Name/Critter or $Name/Item
- gameplay balance, content ids, directory taxonomy, and localization policy
- runtime persistence records and project migration rollout policy

Embedding projects must generate or document their additional entity declarations, `FixedType` metadata, properties, extensions, and content IDs separately.
