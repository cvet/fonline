---
title: Форматы видеоресурсов
document_id: generated-video-formats
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-video-formats","locale":"ru","source_path":"Docs/en/reference/video/formats.md","source_sha256":"d227025cb75fb2c4245acd9a4883e56236ed0f9b1c8e4c96fd6a560512fd7415"} -->

# Форматы видеоресурсов

> Сгенерированный справочник. Не редактируйте его источник напрямую. Обновите `BuildTools/VideoInterface.json`, затем выполните `python BuildTools/docs_video.py --write`.

[Индекс](index.md) | [Форматы](formats.md) | [Доставка](delivery.md) | [Декодирование](decoding.md) | [Полный экран](fullscreen.md) | [Встроенное](embedded.md) | [Проверка](validation.md) | [Канонический JSON](../../../generated/video.json) | [Руководство](../../how-to/content/video.md)

| Стабильный ID | Правило | Требование | Причина | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-video-format-ogv-3e54cfa6a4"></a><code>video.format.ogv</code> | Видеоресурс Ogg | Доставляйте ресурс Ogg с видеопотоком Theora через видимый клиенту пакет RawCopy и передавайте video API его точный путь. | OGV является штатным соглашением raw copy; декодер читает страницы Ogg и подаёт заголовки и пакеты Theora вместо dispatch универсальной медиасреды. | [Source/Common/Settings.inc](https://github.com/cvet/fonline/blob/master/Source/Common/Settings.inc), [Source/Client/VideoClip.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/VideoClip.cpp) |
| <a id="entry-video-format-theora-bccabd3fa3"></a><code>video.format.theora</code> | Элементарное видео Theora в Ogg | Используйте декодируемый поток Theora с допустимыми размерами, метаданными частоты кадров и одним из поддерживаемых пиксельных форматов 4:2:0, 4:2:2 или 4:4:4. | Встроенный тракт напрямую подключает libtheora и не имеет альтернативного dispatch кодеков. | [BuildTools/cmake/stages/ThirdParty.cmake](https://github.com/cvet/fonline/blob/master/BuildTools/cmake/stages/ThirdParty.cmake), [Source/Client/VideoClip.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/VideoClip.cpp) |
