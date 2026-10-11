---
title: Сгенерированный справочник audio
document_id: generated-audio-index
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-audio-index","locale":"ru","source_path":"Docs/en/reference/audio/index.md","source_sha256":"afb33398c17989bea54e62d73a4a485a43f36b9cc14df5259d834770d31532f1"} -->

# Сгенерированный справочник audio

> Сгенерированный справочник. Не редактируйте его источник напрямую. Обновите `BuildTools/AudioInterface.json`, затем выполните `python BuildTools/docs_audio.py --write`.

[Индекс](index.md) | [Форматы](formats.md) | [Доставка](delivery.md) | [Декодирование](decoding.md) | [Воспроизведение](playback.md) | [Проверка](validation.md) | [Канонический JSON](../../../generated/audio.json) | [Руководство](../../how-to/content/audio.md)

Этот справочник описывает принадлежащие Engine доставку аудиоресурсов, декодирование, воспроизведение, микширование и точки входа скриптов. Каталоги звуков проекта, пространственная политика, автоматы состояний музыки, мастеринг и лицензирование находятся вне этого контракта.

## Статус контракта

| Поле | Значение |
| --- | --- |
| Стабильность | <code>experimental</code> |
| Политика поддержки | Авторские входы WAV и нативного Ogg, запечённый payload Ogg, handles звука, обновления позиционирования и поведение клиентского воспроизведения привязаны к ревизии сфокусированными тестами baker и mixer. |
| Исходный манифест | [BuildTools/AudioInterface.json](https://github.com/cvet/fonline/blob/master/BuildTools/AudioInterface.json) |
| Digest контракта | <code>a5b3cb18613c5f67a6be38634f1e162fc652de55e57bf8b14482015ca92c9205</code> |
| Runtime-форматы | <code>.ogg</code> |
| Missing-suffix fallback | no |
| Сторона выполнения | <code>client</code> |
| Сфокусированные нативные audio-тесты | 2 |

| Справочник | Записи | Назначение |
| --- | --- | --- |
| [Форматы](formats.md) | 2 | Допустимые контейнеры/кодеки и роли источников. |
| [Доставка](delivery.md) | 5 | Audio baking, authored paths, indexing, and payload rules. |
| [Декодирование](decoding.md) | 7 | Ограничения форматов, streaming, преобразование и микширование. |
| [Воспроизведение](playback.md) | 10 | Script methods, sound handles, placement, music, repeat, and volume. |
| [Проверка](validation.md) | 7 | Поведение при ошибках и границы проверки. |

## Граница

Включено:

- авторские входы WAV и нативные ресурсы Ogg Vorbis, принимаемые AudioBaker
- доставка через AudioBaker и индексирование путей ресурсов в AudioManager
- декодирование, преобразование, streaming, микширование, позиционирование, повтор и остановка в AudioManager
- клиентские script entry points Game.PlaySound, Game.UpdateSound и Game.PlayMusic
- настройки аудио, headless-поведение, диагностика и границы проектной проверки

Исключено:

- проектные каталоги звуков, сопоставления понятий с путями, выбор вариантов, автоматы состояний музыки, выбор ambient и политика пространственного звука
- запись, голосовой чат, устройства захвата, DSP-графы, шины, ducking и авторские кривые attenuation
- целевые параметры мастеринга, политика loudness, лицензирование, атрибуция и происхождение исходников
- звуковые узлы Effekseer, которые не поддерживаются средой выполнения Effekseer в FOnline
- видеоконтейнеры и связанный с видео звук
