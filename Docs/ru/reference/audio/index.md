---
title: Сгенерированный справочник audio
document_id: generated-audio-index
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-audio-index","locale":"ru","source_path":"Docs/en/reference/audio/index.md","source_sha256":"2ffb13237752f6a36ba5accdcf490688b88a27ce102d96acb02d9c8bc21fa61d"} -->

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
| Digest контракта | <code>1042c9d004031d23a6ab77827c060b15c1725d59061b1a6671ba32b06709f03e</code> |
| Runtime-форматы | <code>.ogg</code> |
| Missing-suffix fallback | no |
| Сторона выполнения | <code>client</code> |
| Сфокусированные нативные audio-тесты | 3 |

| Справочник | Записи | Назначение |
| --- | --- | --- |
| [Форматы](formats.md) | 3 | Допустимые контейнеры/кодеки и роли источников. |
| [Доставка](delivery.md) | 5 | Audio baking, authored paths, indexing, and payload rules. |
| [Декодирование](decoding.md) | 8 | Ограничения форматов, streaming, преобразование и микширование. |
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
