---
title: Доставка аудиоресурсов
document_id: generated-audio-delivery
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-audio-delivery","locale":"ru","source_path":"Docs/en/reference/audio/delivery.md","source_sha256":"65ba79c4d1dfc88dcdf419da715bc05eee8461e6b98fe726590dcde3734e0eee"} -->

# Доставка аудиоресурсов

> Сгенерированный справочник. Не редактируйте его источник напрямую. Обновите `BuildTools/AudioInterface.json`, затем выполните `python BuildTools/docs_audio.py --write`.

[Индекс](index.md) | [Форматы](formats.md) | [Доставка](delivery.md) | [Декодирование](decoding.md) | [Воспроизведение](playback.md) | [Проверка](validation.md) | [Канонический JSON](../../../generated/audio.json) | [Руководство](../../how-to/content/audio.md)

| Стабильный ID | Правило | Требование | Причина | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-audio-delivery-raw-copy-f938a33272"></a><code>audio.delivery.raw-copy</code> | Доставка через AudioBaker | Включайте Audio baker в каждый ресурсный пакет, владеющий runtime-аудио: он принимает настроенные входы wav и ogg и записывает проверенный payload Ogg Vorbis. | Аудиоресурсы больше не используют RawCopy: WAV нормализуется и кодируется, а уже нативный Ogg проверяется и копируется без дополнительного прохода с потерями. | [Source/Tools/AudioBaker.h](https://github.com/cvet/fonline/blob/master/Source/Tools/AudioBaker.h), [Source/Tools/AudioBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/AudioBaker.cpp) |
| <a id="entry-audio-delivery-client-index-a1456e4c25"></a><code>audio.delivery.client-index</code> | Клиентский индекс звуков | Вызывайте AudioManager.IndexFiles после появления клиентских ресурсов; он записывает каждый путь, чей суффикс присутствует в Audio.SoundFileExtensions. | Менеджер предоставляет этот каталог путей, сформированный по исходникам, а Game.PlaySound получает точный путь выбранного ресурса и не разрешает понятия или варианты. | [Source/Client/AudioManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/AudioManager.cpp) |
| <a id="entry-audio-delivery-effect-identity-c2478ffbdd"></a><code>audio.delivery.effect-identity</code> | Точный авторский путь как идентификатор | Передавайте в Game.PlaySound и Game.PlayMusic точный путь запечённого ресурса, включая авторское расширение. | AudioBaker сохраняет авторский путь, заменяя байты WAV на Vorbis, а AudioManager читает этот путь напрямую, не меняя регистр и не заменяя расширение. | [Source/Tools/AudioBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/AudioBaker.cpp), [Source/Client/AudioManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/AudioManager.cpp) |
| <a id="entry-audio-delivery-extension-precedence-ec0db763ec"></a><code>audio.delivery.extension-precedence</code> | Разделение исходного расширения и payload | Считайте суффикс .wav в запечённых ресурсах идентификатором исходника, а не runtime-кодека: все runtime-байты имеют формат Ogg Vorbis, а исходники .ogg сохраняют расширение .ogg. | Сохранение авторских путей не требует переписывать ссылки проекта, а единый runtime-декодер и единый формат package payload устраняют платформенно-зависимое поведение форматов. | [Source/Tools/AudioBaker.h](https://github.com/cvet/fonline/blob/master/Source/Tools/AudioBaker.h) |
| <a id="entry-audio-delivery-music-path-22673a0d79"></a><code>audio.delivery.music-path</code> | Владение путём музыки | Передавайте музыку по точному пути ресурса; в отличие от эффектов, она не разрешается через нормализованный индекс имён звуков. | PlayMusic передаёт указанное имя файла непосредственно в AudioManager.Load. | [Source/Client/AudioManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/AudioManager.cpp) |
