---
title: Форматы аудиоресурсов
document_id: generated-audio-formats
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-audio-formats","locale":"ru","source_path":"Docs/en/reference/audio/formats.md","source_sha256":"baeaa5eced3560269a3bd251d87ccc6ad08425d10e13c64c84e40f6fe53947b1"} -->

# Форматы аудиоресурсов

> Сгенерированный справочник. Не редактируйте его источник напрямую. Обновите `BuildTools/AudioInterface.json`, затем выполните `python BuildTools/docs_audio.py --write`.

[Индекс](index.md) | [Форматы](formats.md) | [Доставка](delivery.md) | [Декодирование](decoding.md) | [Воспроизведение](playback.md) | [Проверка](validation.md) | [Канонический JSON](../../../generated/audio.json) | [Руководство](../../how-to/content/audio.md)

| Стабильный ID | Суффикс | Роль | Контракт | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-audio-format-wav-005081e1f5"></a><code>audio.format.wav</code> | <code>.wav</code> | Короткие эффекты или музыка, полностью декодируемые до воспроизведения | Используйте файл RIFF/WAVE с непосредственно следующим чанком fmt, необязательным чанком fact, непосредственно следующим чанком data, тегом формата PCM 1 и 8-bit unsigned либо 16-bit signed samples. | [Source/Client/SoundManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/SoundManager.cpp) |
| <a id="entry-audio-format-acm-f70b222843"></a><code>audio.format.acm</code> | <code>.acm</code> | Устаревшие эффекты и музыка, полностью декодируемые до воспроизведения | Передавайте поток Interplay ACM, принимаемый CACMUnpacker; штатный контракт воспроизводит эффекты как mono, а музыку как stereo signed 16-bit audio с частотой 22050 Hz. | [Source/Client/SoundManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/SoundManager.cpp) |
| <a id="entry-audio-format-ogg-091dbf2da3"></a><code>audio.format.ogg</code> | <code>.ogg</code> | Современные эффекты или streaming-музыка | Используйте поток Ogg с аудио Vorbis; SoundManager декодирует interleaved signed 16-bit данные и сохраняет декодер, только если после первой порции остаются данные. | [Source/Client/SoundManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/SoundManager.cpp) |
