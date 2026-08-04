---
title: Сгенерированный справочник форматов изображений
document_id: generated-image-format-index
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-image-format-index","locale":"ru","source_path":"Docs/en/reference/image-format/index.md","source_sha256":"aa49b58ccde17cf965c12af520085521024145af6a4f426b43365e5dab2fcd15"} -->

# Сгенерированный справочник форматов изображений

> Сгенерированный справочник. Не редактируйте его напрямую. Обновите `BuildTools/ImageFormatInterface.json`, затем выполните `python BuildTools/docs_image_format.py --write`.

[Индекс](index.md) | [Форматы](formats.md) | [FOFRM](fofrm.md) | [Параметры](options.md) | [Запекание](baking.md) | [Runtime](runtime.md) | [Проверка](validation.md) | [Канонический JSON](../../../generated/image-format.json) | [Руководство](../../how-to/content/image-format.md)

Этот справочник описывает принадлежащий Engine контракт импорта изображений, композиции FOFRM, запечённых спрайтов, клиентской загрузки, атласов, кэшей и проверки. Каталоги ресурсов проекта и визуальная приёмка принадлежат проекту.

## Состояние контракта

| Поле | Значение |
| --- | --- |
| Стабильность | <code>experimental</code> |
| Политика поддержки | Контракт создаётся для закреплённой ревизии Engine. Проекты владеют каталогами ресурсов, лицензированием источников, визуальным стилем, политикой сжатия, приоритетом resource pack, подстановками анимаций, настройкой движения и видимой приёмкой. |
| Исходный manifest | [BuildTools/ImageFormatInterface.json](https://github.com/cvet/fonline/blob/master/BuildTools/ImageFormatInterface.json) |
| Digest контракта | <code>e566e0b777acd21b4440657bb7dec1cbcfd15a53e99c9f128f4d0c303e17753a</code> |
| Baker | <code>Image</code>, порядок 4 |
| Запечённые pixels | <code>RGBA8</code> |
| Сторона runtime | <code>client</code> |

| Справочник | Записей | Назначение |
| --- | --- | --- |
| [Форматы](formats.md) | 12 | Принимаемые исходные форматы и текущее поведение импорта. |
| [FOFRM](fofrm.md) | 9 | Поля descriptor, aliases, направления, flattening и timing. |
| [Параметры](options.md) | 3 | Selectors имени файла ART, SPR и BAM. |
| [Запекание](baking.md) | 10 | Обнаружение, имена выходов, записи контейнера и отказы. |
| [Runtime](runtime.md) | 8 | Покрытие factory, sprite sheets, загрузка в atlas и caches. |
| [Проверка](validation.md) | 9 | Ограничения источников и исполняемые проверки. |

## Граница

Включено:

- двенадцать встроенных расширений источников ImageBaker и их текущее поведение импорта
- поля и aliases FOFRM, разделы направлений, вложенные ссылки, параметры имени, flattening, offsets и timing
- версионированный частный контейнер RGBA/mesh-кадров, индекс SpriteInfo каждого pack и поведение переименования выхода
- покрытие штатной клиентской sprite factory, playback SpriteSheet, polygon/quad draw в atlas, hit masks, caches и diagnostics
- границы focused source anchors, generator, native tests, project bake и видимой проверки

Исключено:

- каталоги изображений проекта, приоритет resource pack, лицензии ресурсов, art direction, цели качества и эталоны приёмки
- authoritative movement и подробный алгоритм проекции walk/run из SpriteRootMotion.md
- авторинг частиц, model textures, shader effects, GUI layout, fonts, video и audio formats
- публичное обещание совместимости частного потока запечённых байтов или неподдерживаемых сторонних форматов изображений
