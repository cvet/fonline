---
title: Model Description Syntax
document_id: generated-model-format-syntax
locale: en
generated: true
---

# Model Description Syntax

> Generated reference. Do not edit directly. Update `BuildTools/ModelFormatInterface.json`, then run `python BuildTools/docs_model_format.py --write`.

[Index](index.md) | [Syntax](syntax.md) | [Tokens](tokens.md) | [Composition](composition.md) | [Assets](assets.md) | [Animation](animation.md) | [Validation](validation.md) | [Canonical JSON](../../../generated/model-format.json) | [Guide](../../how-to/content/model-format.md)

The parser is stateful and whitespace-tokenized. A compact line is legal, but directive order determines the current layer link and mesh selector.

## Minimal concrete description

```text
Model Body.fbx

Anim CritterStateAnim.Unarmed CritterActionAnim.Idle ModelFile Idle

Layer 1
Value 1
Attach Hat.fbx Link Head
```

## Include template

```text
# TEMPLATE_Humanoid.fo3d
Model %mesh%
Scale* %scale%
```

```text
# Human.fo3d
Include TEMPLATE_Humanoid.fo3d mesh Human.fbx scale 0.9
```

## Syntax rules

| Stable ID | Rule | Requirement | Why |
| --- | --- | --- | --- |
| <a id="entry-model-format-rule-lexical-syntax-3a2486e250"></a><code>model-format.rule.lexical-syntax</code> | Whitespace tokenization | Directives and arguments are whitespace-separated; # and ; start comments; there is no quoting or escaping for paths containing spaces. | The parser strips comments and feeds each line through istringstream token extraction. |
| <a id="entry-model-format-rule-multiple-directives-5e3fe606e6"></a><code>model-format.rule.multiple-directives</code> | Sequential line parsing | A line may contain multiple directives; each directive consumes its exact argument count and parsing continues with the next token. | Compact layer entries are legal, but ordering changes which current link or mesh selector receives later modifiers. |
| <a id="entry-model-format-rule-selector-order-f53cd0c2a9"></a><code>model-format.rule.selector-order</code> | Selector ordering | After Layer or Value, author Root, Attach, or AttachParticles before link modifiers such as transforms, materials, disables, or cuts. | Layer and Value point at a dummy link and clear Mesh; modifiers written before a real link is created are discarded. |
| <a id="entry-model-format-rule-template-files-5334d3672b"></a><code>model-format.rule.template-files</code> | Template naming | Name include-only files with a basename beginning TEMPLATE_; concrete files must not use that prefix. | Template files participate in include timestamps and parsing but ModelInfoBaker does not emit them as independent resources or animation-metadata sections. |
| <a id="entry-model-format-rule-include-replacements-2c1af91e2a"></a><code>model-format.rule.include-replacements</code> | Include replacement scope | Include arguments are name/value pairs replacing every literal %name% occurrence in the included text before tokenization. | Replacement is plain text and does not understand token boundaries; choose placeholder names that cannot collide accidentally. |
| <a id="entry-model-format-rule-relative-paths-cfa2a858c1"></a><code>model-format.rule.relative-paths</code> | Path ownership | Model, Include, Attach, and Cut paths resolve relative to their declaring .fo3d file; animation files resolve relative to the concrete description unless ModelFile is used; particle and effect paths are global baked-resource paths. | Moving a template or concrete description can change the asset paths contributed by directives inside that file. |
| <a id="entry-model-format-rule-zero-identity-4b43d7830b"></a><code>model-format.rule.zero-identity</code> | Zero is transform identity | Treat zero transform and Speed fields as no contribution. The + and * variants initialize a zero field from their operand before applying later operations. | Runtime SetAnimData skips zero fields, while parser accumulation deliberately makes template-only Scale* and Speed* useful. |
| <a id="entry-model-format-rule-layer-zero-fe807a48c3"></a><code>model-format.rule.layer-zero</code> | Layer zero value is inactive | A runtime layer value of zero selects no link; authored Root and Attach entries require a non-zero Value. | The composition loop skips zero layer values and the baker rejects links created with zero. |
