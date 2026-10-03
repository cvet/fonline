---
title: Форматы аудиоресурсов
document_id: generated-audio-formats
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-audio-formats","locale":"ru","source_path":"Docs/en/reference/audio/formats.md","source_sha256":"8df33ba8ff5ae57b56fc8a9793b6b5425b2d137b4479ba0b5a0b05ce14e300df"} -->

# Форматы аудиоресурсов

> Сгенерированный справочник. Не редактируйте его источник напрямую. Обновите `BuildTools/AudioInterface.json`, затем выполните `python BuildTools/docs_audio.py --write`.

[Индекс](index.md) | [Форматы](formats.md) | [Доставка](delivery.md) | [Декодирование](decoding.md) | [Воспроизведение](playback.md) | [Проверка](validation.md) | [Канонический JSON](../../../generated/audio.json) | [Руководство](../../how-to/content/audio.md)

| Стабильный ID | Суффикс | Роль | Контракт | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-audio-format-wav-005081e1f5"></a><code>audio.format.wav</code> | <code>.wav</code> | Авторский вход, преобразуемый в runtime-payload Ogg Vorbis | Используйте RIFF/WAVE PCM с разрядностью 8, 16, 24 или 32 бита либо IEEE float с разрядностью 32 бита; чанки могут идти в любом порядке, а неизвестные чанки пропускаются с учётом выравнивания RIFF. | [Source/Tools/AudioBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/AudioBaker.cpp) |
| <a id="entry-audio-format-ogg-091dbf2da3"></a><code>audio.format.ogg</code> | <code>.ogg</code> | Нативный runtime-payload и авторский вход без перекодирования | Используйте битовый поток Ogg с аудио Vorbis; AudioBaker проверяет и копирует авторский .ogg без повторного кодирования с потерями, а AudioManager передаёт каждый запечённый аудиоресурс в libvorbisfile как поток. | [Source/Client/AudioManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/AudioManager.cpp), [Source/Tools/AudioBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/AudioBaker.cpp) |
