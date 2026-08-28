---
title: Справочник формата моделей
document_id: generated-model-format-index
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-model-format-index","locale":"ru","source_path":"Docs/en/reference/model-format/index.md","source_sha256":"2d5341150d6879a75410b13184d59af77b119113f8d975ed2cdd3b48efe05653"} -->

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
| Дайджест контракта | <code>c6a3ec054d2d0df5013725e5383d4287d45387a0ed150e562f8c17983449a7b0</code> |
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
