---
title: Доставка аудиоресурсов
document_id: generated-audio-delivery
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-audio-delivery","locale":"ru","source_path":"Docs/en/reference/audio/delivery.md","source_sha256":"4a069ea8449a510672de2c28ab306507afc74bcab40fd492ccfbaa28d7fb18c4"} -->

# Доставка аудиоресурсов

> Сгенерированный справочник. Не редактируйте его источник напрямую. Обновите `BuildTools/AudioInterface.json`, затем выполните `python BuildTools/docs_audio.py --write`.

[Индекс](index.md) | [Форматы](formats.md) | [Доставка](delivery.md) | [Декодирование](decoding.md) | [Воспроизведение](playback.md) | [Проверка](validation.md) | [Канонический JSON](../../../generated/audio.json) | [Руководство](../../how-to/content/audio.md)

| Стабильный ID | Правило | Требование | Причина | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-audio-delivery-raw-copy-f938a33272"></a><code>audio.delivery.raw-copy</code> | Доставка raw copy | Сохраняйте acm, ogg и wav в Baking.RawCopyFileExtensions и включайте RawCopy в каждый ресурсный пакет с runtime-аудио. | Отдельного audio baker нет; runtime-декодерам нужны исходные байты и относительный путь. | [Source/Common/Settings.inc](https://github.com/cvet/fonline/blob/master/Source/Common/Settings.inc), [Source/Tools/RawCopyBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/RawCopyBaker.cpp) |
| <a id="entry-audio-delivery-client-index-a1456e4c25"></a><code>audio.delivery.client-index</code> | Клиентский индекс звуков | Вызывайте ResourceManager.IndexFiles после появления клиентских ресурсов; он индексирует каждый путь wav, acm и ogg для поиска по имени эффекта. | Game.PlaySound использует заранее построенную карту имён, а не сканирует файловую систему при каждом вызове. | [Source/Client/ResourceManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/ResourceManager.cpp) |
| <a id="entry-audio-delivery-effect-identity-c2478ffbdd"></a><code>audio.delivery.effect-identity</code> | Идентификатор эффекта | Считайте идентификатором эффекта путь ресурса в нижнем регистре без последнего расширения. | PlaySound удаляет суффикс аргумента и переводит имя в нижний регистр перед запросом к так же нормализованному индексу ResourceManager. | [Source/Client/ResourceManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/ResourceManager.cpp), [Source/Client/SoundManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/SoundManager.cpp) |
| <a id="entry-audio-delivery-extension-precedence-ec0db763ec"></a><code>audio.delivery.extension-precedence</code> | Приоритет одинаковой основы | Не поставляйте несколько ресурсов wav/acm/ogg с одной нормализованной основой эффекта; при конфликте побеждает первое расширение в порядке wav, acm, ogg. | ResourceManager использует map::emplace при обходе фиксированного массива расширений, поэтому более поздний формат не заменяет существующую основу. | [Source/Client/ResourceManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/ResourceManager.cpp) |
| <a id="entry-audio-delivery-music-path-22673a0d79"></a><code>audio.delivery.music-path</code> | Владение путём музыки | Передавайте музыку по точному пути ресурса; в отличие от эффектов, она не разрешается через нормализованный индекс имён звуков. | PlayMusic передаёт указанное имя файла непосредственно в SoundManager.Load. | [Source/Client/SoundManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/SoundManager.cpp) |
