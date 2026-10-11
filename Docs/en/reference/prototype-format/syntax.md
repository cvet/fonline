---
title: Prototype File Syntax
document_id: generated-prototype-format-syntax
locale: en
generated: true
---

# Prototype File Syntax

> Generated reference. Do not edit directly. Update `BuildTools/PrototypeFormatInterface.json` or the owning engine metadata, then run `python BuildTools/docs_prototype_format.py --write`.

[Index](index.md) | [Syntax](syntax.md) | [Properties](properties.md) | [Validation](validation.md) | [Canonical JSON](../../../generated/prototype-format.json) | [Authoring guide](../../how-to/content/prototype-format.md)

`Baking.ProtoFileExtensions` selects input files. Engine defaults: <code>fopro</code>. An embedding project may add extensions without changing how sections are parsed.

Each pack emits `<pack>.fopro-bin-<side>` for <code>server</code>, <code>client</code>, <code>mapper</code>. Every top-level `[ProtoMap]` anchor contributes a Map prototype; nested map-placement sections are skipped.

## Section forms

| Stable ID | Syntax | Resolution | Meaning |
| --- | --- | --- | --- |
| <a id="entry-prototype-format-section-proto-entity-2782eabb8e"></a><code>prototype-format.section.proto-entity</code> | <code>[Proto&lt;Type&gt;]</code> | Entity metadata type &lt;Type&gt; with HasProtos | Declares a prototype for a built-in or project-authored entity type whose metadata enables prototypes. |
| <a id="entry-prototype-format-section-fixed-type-12fc343b51"></a><code>prototype-format.section.fixed-type</code> | <code>[&lt;FixedType&gt;]</code> | A metadata type declared with ///@ FixedType | Declares a prototype-like fixed value defined by embedding-project metadata. |
| <a id="entry-prototype-format-section-fomap-proto-map-88acc25d02"></a><code>prototype-format.section.fomap-proto-map</code> | <code>[ProtoMap]</code> | Map | Each top-level ProtoMap anchor declares a Map prototype. Nested $Name/Item and $Name/Critter placement sections are skipped by ProtoBaker and consumed by MapBaker. |

## Control directives

| Stable ID | Syntax | Default | Meaning |
| --- | --- | --- | --- |
| <a id="entry-prototype-format-directive-name-3d7ea9d8be"></a><code>prototype-format.directive.name</code> | <code>$Name = &lt;PrototypeId&gt;</code> | source file name without its extension | Sets the prototype identity within its resolved type. The identity is migration-resolved before duplicate detection. |
| <a id="entry-prototype-format-directive-parent-2b3d1ad932"></a><code>prototype-format.directive.parent</code> | <code>$Parent = &lt;ParentId&gt; [&lt;ParentId&gt; ...]</code> | no parents | Lists same-type parent prototypes separated by spaces. Parents are merged left to right and the child is applied last. |

## Minimal example

```ini
[ProtoItem]
$Name = BaseItem

[ProtoItem]
$Name = DerivedItem
$Parent = BaseItem
```

The section selects the type. The extension and directory do not. Values use the shared configuration parser, including `#` comments, a trailing backslash preceded by space or tab for continuation, and `key += value` append syntax.
