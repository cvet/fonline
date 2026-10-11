---
title: Доставка видеоресурсов
document_id: generated-video-delivery
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-video-delivery","locale":"ru","source_path":"Docs/en/reference/video/delivery.md","source_sha256":"7889179e47036d3e86707a3915b33487008f4019c7272fe97a8bb633cc989778"} -->

# Доставка видеоресурсов

> Сгенерированный справочник. Не редактируйте его источник напрямую. Обновите `BuildTools/VideoInterface.json`, затем выполните `python BuildTools/docs_video.py --write`.

[Индекс](index.md) | [Форматы](formats.md) | [Доставка](delivery.md) | [Декодирование](decoding.md) | [Полный экран](fullscreen.md) | [Встроенное](embedded.md) | [Проверка](validation.md) | [Канонический JSON](../../../generated/video.json) | [Руководство](../../how-to/content/video.md)

| Стабильный ID | Правило | Требование | Причина | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-video-delivery-raw-copy-065ac8d80c"></a><code>video.delivery.raw-copy</code> | Доставка raw copy | Сохраняйте ogv в Baking.RawCopyFileExtensions и включайте RawCopy в клиентский ресурсный пакет, владеющий видеофайлами. | Video baker отсутствует; воспроизведение потребляет доставленные байты. | [Source/Common/Settings.inc](https://github.com/cvet/fonline/blob/master/Source/Common/Settings.inc), [Source/Tools/RawCopyBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/RawCopyBaker.cpp) |
| <a id="entry-video-delivery-exact-path-1964d0511a"></a><code>video.delivery.exact-path</code> | Точный путь ресурса | Передавайте полный доставленный путь видео вместе с расширением; поиск видео не имеет суффикса по умолчанию или нормализованного индекса основы имени. | Полноэкранный и управляемый скриптом тракты вызывают Ресурсs.ReadFile с переданным путём. | [Source/Client/Client.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/Client.cpp), [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp) |
| <a id="entry-video-delivery-client-only-c11a2c2a05"></a><code>video.delivery.client-only</code> | Владение клиентской среды выполнения | Доставляйте видео в клиентские ресурсы и запускайте воспроизведение в среде выполнения клиента или mapper. | Декодирование, создание текстуры, рисование и экспортированные методы находятся в клиентском слое. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp) |
| <a id="entry-video-delivery-memory-budget-05df3e8e88"></a><code>video.delivery.memory-budget</code> | Бюджет памяти всего ресурса | Для каждого активного воспроизведения закладывайте байты сжатого файла, один CPU RGBA-кадр и одну GPU-текстуру; штатный тракт не выполняет streaming из хранилища ресурсов. | ReadFile.GetData переносит весь ресурс в VideoClip.RawVideoData до декодирования пакетов. | [Source/Client/VideoClip.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/VideoClip.cpp), [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp) |
