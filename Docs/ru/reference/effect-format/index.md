---
title: Сгенерированный справочник формата эффектов
document_id: generated-effect-format-index
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-effect-format-index","locale":"ru","source_path":"Docs/en/reference/effect-format/index.md","source_sha256":"83d495107a956d39ba98efe3f779f13d042d451f3b4349a768c924991e19f49a"} -->

# Сгенерированный справочник формата эффектов

> Сгенерированный справочник. Не редактируйте его напрямую. Обновите `BuildTools/EffectFormatInterface.json`, затем выполните `python BuildTools/docs_effect_format.py --write`.

[Индекс](index.md) | [Синтаксис](syntax.md) | [Состояние рендеринга](render-state.md) | [Ресурсы](resources.md) | [Запекание](baking.md) | [Runtime](runtime.md) | [Проверка](validation.md) | [Канонический JSON](../../../generated/effect-format.json) | [Руководство](../../how-to/content/effect-format.md)

Этот справочник описывает переиспользуемый контракт Engine для авторинга `.fofx`, запекания, ресурсов рендерера, runtime-загрузки и управления из скриптов. Конкретные каталоги шейдеров, визуальная политика и значения слотов ScriptValue принадлежат проекту.

## Состояние контракта

| Поле | Значение |
| --- | --- |
| Стабильность | <code>experimental</code> |
| Политика поддержки | Контракт генерируется для закреплённой ревизии Engine. Проекты владеют каталогами эффектов, визуальной политикой, профилями качества, конкретными путями переопределений и значениями слотов ScriptValue. |
| Исходный манифест | [BuildTools/EffectFormatInterface.json](https://github.com/cvet/fonline/blob/master/BuildTools/EffectFormatInterface.json) |
| Дайджест контракта | <code>bd50466bab9291d7b04d119f6fb9b613bc6507b1c771cef1605c7d7e7184aea0</code> |
| Расширение исходного файла | <code>.fofx</code> |
| Сторона runtime | <code>client</code> |

| Справочник | Записи | Назначение |
| --- | --- | --- |
| [Syntax](syntax.md) | 4 | Обязательные и необязательные секции, а также fallback проходов. |
| [Render state](render-state.md) | 10 | Число проходов, смешивание, глубина, версия шейдера и состояние теней. |
| [Resources](resources.md) | 12 resources / 4 limits | Форматы вершин, sampler-ы, встроенные uniform buffer-ы и привязки. |
| [Baking](baking.md) | 8 | Окружение компилятора, reflection, форматы выходов и переназначение SDL. |
| [Runtime](runtime.md) | 7 rules / 4 methods | Выбор эффектов, кеширование, сохранение ScriptValue и обновления. |

## Граница ответственности

Включено:

- секции конфигурации .fofx, fallback стадий шейдера, объявления проходов, состояние рендеринга, необязательные варианты глубины/culling для отдельных вызовов отрисовки и пролог компилятора шейдеров
- форматы входных вершин, распознаваемые sampler-ы, встроенные uniform buffer-ы, соглашения о дескрипторах, reflection и ограничения backend-ов
- запечённые артефакты каждого прохода для Vulkan, SDL_GPU, OpenGL, OpenGL ES, Direct3D и Metal
- runtime-загрузка, идентичность кеша по пути, выбор EffectUsage, сохранение script value, скриптовые методы и проверка

Исключено:

- проектные каталоги эффектов, художественные правила шейдеров, уровни качества и конкретная политика перекрытия ресурсов
- определяемые проектом индексы, диапазоны, владельцы, значения по умолчанию и игровая семантика ScriptValue
- общий справочник языка GLSL, оптимизация производительности GPU и отладка шейдеров конкретных производителей
- авторинг систем частиц, форматы изображений, запекание спрайтов, грамматика описаний моделей и компоновка GUI
