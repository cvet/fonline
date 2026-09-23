---
title: Синтаксис описания моделей
document_id: generated-model-format-syntax
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-model-format-syntax","locale":"ru","source_path":"Docs/en/reference/model-format/syntax.md","source_sha256":"1a71d34dfba7469732721c9281a90d0b3bd1610869e600b0c315cb7776f7e904"} -->

# Синтаксис описания моделей

> Сгенерированный справочник. Не редактируйте напрямую. Обновите `BuildTools/ModelFormatInterface.json`, затем запустите `python BuildTools/docs_model_format.py --write`.

[Индекс](index.md) | [Синтаксис](syntax.md) | [Токены](tokens.md) | [Композиция](composition.md) | [Ресурсы](assets.md) | [Анимация](animation.md) | [Валидация](validation.md) | [Каноническая JSON-модель](../../../generated/model-format.json) | [Руководство](../../how-to/content/model-format.md)

Parser хранит состояние и разделяет вход по whitespace. Компактная строка допустима, но порядок directives определяет текущий layer link и selector mesh.

## Минимальное конкретное описание

```text
Model Body.fbx

Anim CritterStateAnim.Unarmed CritterActionAnim.Idle ModelFile Idle

Layer 1
Value 1
Attach Hat.fbx Link Head
```

## Include-шаблон

```text
# TEMPLATE_Humanoid.fo3d
Model %mesh%
Scale* %scale%
```

```text
# Human.fo3d
Include TEMPLATE_Humanoid.fo3d mesh Human.fbx scale 0.9
```

## Правила синтаксиса

| Стабильный ID | Правило | Требование | Причина |
| --- | --- | --- | --- |
| <a id="entry-model-format-rule-lexical-syntax-3a2486e250"></a><code>model-format.rule.lexical-syntax</code> | Разделение по whitespace | Directives и arguments разделяются whitespace; # и ; начинают comments; quoting/escaping для paths с spaces отсутствуют. | Parser удаляет comments и извлекает tokens строки через istringstream. |
| <a id="entry-model-format-rule-multiple-directives-5e3fe606e6"></a><code>model-format.rule.multiple-directives</code> | Последовательный разбор | Строка может содержать несколько directives; каждая забирает точное число arguments, затем разбор продолжается. | Компактные entries допустимы, но order меняет current link или mesh selector последующих modifiers. |
| <a id="entry-model-format-rule-selector-order-f53cd0c2a9"></a><code>model-format.rule.selector-order</code> | Порядок selectors | После Layer или Value задайте Root, Attach либо AttachParticles до transforms, materials, disables или cuts. | Layer и Value выбирают dummy link и очищают Mesh; ранние modifiers отбрасываются. |
| <a id="entry-model-format-rule-template-files-5334d3672b"></a><code>model-format.rule.template-files</code> | Имена templates | Basename include-only files начинается с TEMPLATE_; concrete files не используют этот prefix. | Templates участвуют в timestamps/parsing, но не выдаются как отдельные resources или metadata sections. |
| <a id="entry-model-format-rule-include-replacements-2c1af91e2a"></a><code>model-format.rule.include-replacements</code> | Scope replacements | Arguments Include являются pairs name/value и заменяют каждое буквальное %name% до tokenization. | Простая текстовая замена не понимает token boundaries; placeholders не должны случайно совпадать. |
| <a id="entry-model-format-rule-relative-paths-cfa2a858c1"></a><code>model-format.rule.relative-paths</code> | Владение paths | Model, Include, Attach, Cut разрешаются относительно declaring .fo3d; animation files — относительно concrete description, кроме ModelFile; particles/effects глобальны. | Перемещение template или description может изменить contributed asset paths. |
| <a id="entry-model-format-rule-zero-identity-4b43d7830b"></a><code>model-format.rule.zero-identity</code> | Ноль — transform identity | Нулевые transform и Speed не дают вклада. Варианты +/* инициализируют нулевое поле operand до дальнейших операций. | Runtime SetAnimData пропускает нули, а parser accumulation поддерживает самостоятельные template Scale*/Speed*. |
| <a id="entry-model-format-rule-layer-zero-fe807a48c3"></a><code>model-format.rule.layer-zero</code> | Нулевой layer value неактивен | Runtime value 0 не выбирает link; authored Root и Attach требуют ненулевого Value. | Composition loop пропускает ноль, baker запрещает links с нулём. |
