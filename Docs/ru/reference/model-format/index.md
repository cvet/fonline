---
title: Справочник формата моделей
document_id: generated-model-format-index
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-model-format-index","locale":"ru","source_path":"Docs/en/reference/model-format/index.md","source_sha256":"0cbdbc6128a0bc5a1bba6b111fc8bcae8ca066d0fd6a13a82462d423c9d32483"} -->

# Справочник формата моделей

> Сгенерированный справочник. Не редактируйте напрямую. Обновите `BuildTools/ModelFormatInterface.json`, затем запустите `python BuildTools/docs_model_format.py --write`.

[Индекс](index.md) | [Синтаксис](syntax.md) | [Токены](tokens.md) | [Композиция](composition.md) | [Ресурсы](assets.md) | [Анимация](animation.md) | [Валидация](validation.md) | [Каноническая JSON-модель](../../../generated/model-format.json) | [Руководство](../../how-to/content/model-format.md)

Справочник описывает переиспользуемый язык `.fo3d`, принадлежащий движку, и собираемые им ресурсы моделей. Конкретные игровые модели и семантика слоёв принадлежат проекту.

## Состояние контракта

| Поле | Значение |
| --- | --- |
| Стабильность | <code>experimental</code> |
| Политика поддержки | Контракт генерируется для закреплённой ревизии движка. Каталоги моделей, семантика слоёв, enum анимаций, визуальные правила и конкретные ресурсы принадлежат проектам. |
| Исходный манифест | [BuildTools/ModelFormatInterface.json](https://github.com/cvet/fonline/blob/master/BuildTools/ModelFormatInterface.json) |
| Дайджест контракта | <code>f6b988ffdcc8e32bca13a9318482407410733edf7ddeb44fc30a4af53e3faa84</code> |
| Расширение исходника | <code>.fo3d</code> |
| Входные меши | <code>.fbx</code>, <code>.obj</code> |
| Runtime-сторона | <code>client</code> |

| Справочник | Записей | Назначение |
| --- | --- | --- |
| [Токены](tokens.md) | 32 группы / 59 написаний | Все принимаемые текущим parser токены. |
| [Ресурсы](assets.md) | 6 | Входы мешей, описаний, текстур, эффектов и частиц. |
| [Валидация](validation.md) | 15 | Правила авторинга, запекания, runtime и legacy. |

## Граница

Включено:

- лексический синтаксис .fo3d, include-шаблоны, состояние parser и разрешение путей;
- слои моделей, root modifiers, attachments мешей/моделей/частиц, transforms, materials, effects и cuts;
- входы FBX и OBJ, требования к baked hierarchy, compile-time лимиты и runtime-композиция;
- точки интеграции анимации со специализированным справочником model animation.

Не включено:

- каталоги моделей проекта, семантика номеров слоёв, enum assignments, правила экипировки и игровые тайминги;
- руководства по DCC-инструментам Blender, Maya, 3ds Max и другим;
- детали реализации renderer backend и справочник shader language;
- offsets кадров 2D-спрайтов и sprite root motion.
