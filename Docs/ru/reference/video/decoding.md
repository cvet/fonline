---
title: Контракт декодирования видео
document_id: generated-video-decoding
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-video-decoding","locale":"ru","source_path":"Docs/en/reference/video/decoding.md","source_sha256":"46a3c92c80c7378e9a22c87cc95d7f16273b7bd3ad4599f40aeca64762235142"} -->

# Контракт декодирования видео

> Сгенерированный справочник. Не редактируйте его источник напрямую. Обновите `BuildTools/VideoInterface.json`, затем выполните `python BuildTools/docs_video.py --write`.

[Индекс](index.md) | [Форматы](formats.md) | [Доставка](delivery.md) | [Декодирование](decoding.md) | [Полный экран](fullscreen.md) | [Встроенное](embedded.md) | [Проверка](validation.md) | [Канонический JSON](../../../generated/video.json) | [Руководство](../../how-to/content/video.md)

| Стабильный ID | Правило | Требование | Причина | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-video-decoding-ogg-pages-7e5c1f85e9"></a><code>video.decoding.ogg-pages</code> | Приём страниц и пакетов Ogg | Считайте вход страницами Ogg, читаемыми в sync layer порциями по 1024 байта, и поддерживайте не более десяти одновременно обнаруженных логических потоков. | DecodePacket владеет фиксированной таблицей состояния десяти потоков и копирует ограниченные порции из ресурса в памяти. | [Source/Client/VideoClip.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/VideoClip.cpp) |
| <a id="entry-video-decoding-headers-a17928b083"></a><code>video.decoding.headers</code> | Выбор заголовка Theora | Передавайте поток, заголовки которого принимает th_decode_headerin и setup которого позволяет выделить контекст декодера. | Создание завершается ошибкой при неудаче поиска пакета, setup-данных или выделения декодера. | [Source/Client/VideoClip.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/VideoClip.cpp) |
| <a id="entry-video-decoding-frame-clock-3f8beb68d4"></a><code>video.decoding.frame-clock</code> | Выбор кадра по часам | Готовьте корректные метаданные числителя и знаменателя fps; целевой кадр выводится из прошедшего монотонного времени и стоимости декодера. | RenderFrame умножает прошедшие секунды на отношение fps Theora и декодирует положительную разницу кадров. | [Source/Client/VideoClip.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/VideoClip.cpp) |
| <a id="entry-video-decoding-pixel-formats-e2858cfb95"></a><code>video.decoding.pixel-formats</code> | Поддерживаемая chroma subsampling | Используйте TH_PF_420, TH_PF_422 или TH_PF_444; любой другой пиксельный формат Theora останавливает воспроизведение. | CPU-преобразование выбирает делители chroma только для этих трёх enum-значений. | [Source/Client/VideoClip.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/VideoClip.cpp) |
| <a id="entry-video-decoding-rgba-output-4e80c7f310"></a><code>video.decoding.rgba-output</code> | CPU-вывод RGBA | Ожидайте преобразование каждого декодированного кадра из YCbCr в непрозрачный RGBA на CPU до загрузки текстуры. | RenderFrame записывает ограниченные компоненты RGB и alpha 0xFF в RenderedTextureData. | [Source/Client/VideoClip.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/VideoClip.cpp) |
| <a id="entry-video-decoding-error-stop-fe12a95aeb"></a><code>video.decoding.error-stop</code> | Ошибка декодирования останавливает воспроизведение | Считайте повреждённые данные кадра, ошибку цветового вывода, неподдерживаемый пиксельный формат и конец потока условиями остановки и проверяйте клиентские логи. | Ошибки кадра записываются в лог и вызывают Stop; конец потока также останавливает ролик. | [Source/Client/VideoClip.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/VideoClip.cpp) |
| <a id="entry-video-decoding-no-container-audio-baa734faa9"></a><code>video.decoding.no-container-audio</code> | Аудио контейнера не декодируется | Не полагайтесь на аудиопоток внутри Ogg-видео; при необходимости готовьте звук как отдельный клиентский музыкальный ресурс. | VideoClip подключает только декодирование пакетов Theora, а полноэкранная связка запускает музыку SoundManager из пути после вертикальной черты. | [Source/Client/VideoClip.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/VideoClip.cpp), [Source/Client/Client.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/Client.cpp) |
