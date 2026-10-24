---
title: Сгенерированный справочник форматов частиц
document_id: generated-particle-format-index
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-particle-format-index","locale":"ru","source_path":"Docs/en/reference/particle-format/index.md","source_sha256":"e5ed2c59d01f262be70014f6082e8594596e1968f998516400a6aef832335e65"} -->

# Сгенерированный справочник форматов частиц

> Сгенерированный справочник. Не редактируйте эту страницу напрямую. Обновите `BuildTools/ParticleFormatInterface.json`, затем выполните `python BuildTools/docs_particle_format.py --write`.

[Индекс справочника](index.md) | [Правила исходников](xml.md) | [Форматы и backend-ы](objects.md) | [Отрисовка](renderer.md) | [Инструменты](tooling.md) | [Runtime](runtime.md) | [Интеграция](integration.md) | [Проверка](validation.md) | [Каноническая JSON-модель](../../../generated/particle-format.json) | [Руководство](../../how-to/content/particle-format.md) | [Инструменты авторинга](../../how-to/tools/particle-authoring.md)

Этот справочник описывает контракт необязательных backend-ов SPARK и Effekseer: авторинг, запекание, runtime, Mapper, интеграцию и проверку.

## Состояние контракта

| Поле | Значение |
| --- | --- |
| Стабильность | <code>experimental</code> |
| Политика поддержки | SPARK и Effekseer являются независимыми необязательными backend-ами. Подключаемые проекты должны закреплять ревизию Engine и явно включать, проверять и поддерживать поставляемые ими форматы. |
| Исходный манифест | <code>BuildTools/ParticleFormatInterface.json</code> |
| Дайджест контракта | <code>9c352b7dc317ed0092966188834a5895046e8111a848712cd0d58e68d4b0a8c2</code> |
| Авторские расширения | <code>spark</code>, <code>efkproj</code> |
| Расширения runtime | <code>spk</code>, <code>efk</code> |
| Сторона runtime | <code>client</code> |

| Справочник | Записи | Назначение |
| --- | --- | --- |
| [Source rules](xml.md) | 12 | Границы авторского XML и зависимостей. |
| [Formats and backends](objects.md) | 4 | Необязательные backend-ы и формы source-to-runtime. |
| [Rendering](renderer.md) | 19 | Маршруты и поля отрисовки backend-ов. |
| [Tooling](tooling.md) | 5 | Процессы авторинга в Mapper и отдельных инструментах. |
| [Runtime](runtime.md) | 19 | Компоновка, маршрутизация, seed, масштаб и prewarm. |
| [Integration](integration.md) | 7 | Границы спрайтов, моделей, скриптов и проекта. |
| [Validation](validation.md) | 7 | Gate документации, native-кода, запекания и видимой проверки. |

## Граница ответственности

Включено:

- авторинг SPARK .spark и доставка запечённых .spk
- авторинг Effekseer .efkproj и доставка запечённых .efk
- независимый от backend runtime частиц и интеграция спрайтов
- предпросмотр Mapper и инструменты авторинга SPARK
- интеграция ресурсов, моделей и клиентских скриптов
- основанные на исходниках проверки и production-gate

Исключено:

- каталоги частиц, имена файлов, визуальная политика и бюджеты подключаемого проекта
- выбранные проектом эффекты, текстуры, модели и сцены приёмки
- неподдерживаемые семейства renderer-ов и расширенные возможности Effekseer
- поведение upstream-редакторов, отсутствующее в поставляемых инструментах
- сгенерированные файлы .spk и .efk как авторские исходники
