---
title: Контракт декодирования аудио
document_id: generated-audio-decoding
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-audio-decoding","locale":"ru","source_path":"Docs/en/reference/audio/decoding.md","source_sha256":"1495139ffa9bfddd7fc3f9a9ea3821e47628698cf12365be755ef7d2107cdb76"} -->

# Контракт декодирования аудио

> Сгенерированный справочник. Не редактируйте его источник напрямую. Обновите `BuildTools/AudioInterface.json`, затем выполните `python BuildTools/docs_audio.py --write`.

[Индекс](index.md) | [Форматы](formats.md) | [Доставка](delivery.md) | [Декодирование](decoding.md) | [Воспроизведение](playback.md) | [Проверка](validation.md) | [Канонический JSON](../../../generated/audio.json) | [Руководство](../../how-to/content/audio.md)

| Стабильный ID | Правило | Требование | Причина | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-audio-decoding-wav-chunk-order-ac41e721f1"></a><code>audio.decoding.wav-chunk-order</code> | Порядок чанков WAV | Размещайте RIFF, WAVE, fmt, необязательный fact и data точно в порядке, ожидаемом LoadWav. | Загрузчик является сфокусированным последовательным reader, а не универсальным обходчиком чанков RIFF. | [Source/Client/SoundManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/SoundManager.cpp) |
| <a id="entry-audio-decoding-wav-pcm-width-2099ecdd6f"></a><code>audio.decoding.wav-pcm-width</code> | Разрядность WAV PCM | Готовьте WAV как несжатый PCM с 8-bit unsigned или 16-bit signed samples; число каналов и частота могут отличаться и преобразуются frontend. | LoadWav сопоставляет форматам AppAudio только эти две разрядности до преобразования устройства. | [Source/Client/SoundManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/SoundManager.cpp) |
| <a id="entry-audio-decoding-acm-shape-b7a4574d99"></a><code>audio.decoding.acm-shape</code> | Форма воспроизведения ACM | Ожидайте декодирование ACM-эффектов как mono, а ACM-музыки как stereo signed 16-bit samples с частотой 22050 Hz. | Эти значения назначаются по роли воспроизведения после работы CACMUnpacker. | [Source/Client/SoundManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/SoundManager.cpp) |
| <a id="entry-audio-decoding-ogg-streaming-6ff234d02d"></a><code>audio.decoding.ogg-streaming</code> | Streaming Ogg | Ожидайте декодирование Ogg Vorbis порциями 64 KiB на native и 128 KiB на Web; короткие файлы сохраняются целиком и освобождают stream после первого декодирования. | SoundManager использует платформенный размер streaming-порции и очищает OggStream, когда первое чтение достигает EOF. | [Source/Client/SoundManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/SoundManager.cpp) |
| <a id="entry-audio-decoding-device-conversion-c6306e36cc"></a><code>audio.decoding.device-conversion</code> | Преобразование устройства | Позвольте AppAudio до воспроизведения преобразовать формат samples, число каналов и частоту в формат активного устройства вывода SDL. | SoundManager не требует совпадения авторских ресурсов с одним фиксированным аппаратным форматом. | [Source/Client/SoundManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/SoundManager.cpp), [Source/Frontend/Application.cpp](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.cpp) |
| <a id="entry-audio-decoding-callback-mixing-f9db5306d1"></a><code>audio.decoding.callback-mixing</code> | Микширование в audio callback | Считайте воспроизведение работой клиентского audio callback; изменения списка активных звуков должны удерживать lock аудиоустройства. | Callback потока SDL просит SoundManager заполнить вывод, пока операции play/stop игрового потока могут добавлять или удалять звуки. | [Source/Client/SoundManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/SoundManager.cpp), [Source/Frontend/Application.cpp](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.cpp) |
| <a id="entry-audio-decoding-unsupported-extension-a56ebf6594"></a><code>audio.decoding.unsupported-extension</code> | Ограничение неподдерживаемого расширения | Используйте только пути wav, acm или ogg. В текущем загрузчике нет резервной ветки отклонения, поэтому другой явный суффикс минует все декодеры и может поставить в очередь пустой звук, вернув успех. | Это задокументированное ограничение реализации, а не поддержка произвольных аудиоформатов; вызывающий код и проверка контента должны исключать неподдерживаемые суффиксы. | [Source/Client/SoundManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/SoundManager.cpp) |
