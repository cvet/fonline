---
title: Контракт встроенного видео
document_id: generated-video-embedded
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-video-embedded","locale":"ru","source_path":"Docs/en/reference/video/embedded.md","source_sha256":"c44fd2af13ebeee92d15c15b01fccc14b6a7b00e87c4152dc4ca13b4f2405271"} -->

# Контракт встроенного видео

> Сгенерированный справочник. Не редактируйте его источник напрямую. Обновите `BuildTools/VideoInterface.json`, затем выполните `python BuildTools/docs_video.py --write`.

[Индекс](index.md) | [Форматы](formats.md) | [Доставка](delivery.md) | [Декодирование](decoding.md) | [Полный экран](fullscreen.md) | [Встроенное](embedded.md) | [Проверка](validation.md) | [Канонический JSON](../../../generated/video.json) | [Руководство](../../how-to/content/video.md)

| Стабильный ID | Правило | Требование | Причина | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-video-embedded-create-dd46921a64"></a><code>video.embedded.create</code> | Создание управляемого скриптом воспроизведения | Используйте Game.CreateVideoPlayback(exactPath, looped) для создания независимого воспроизведения с подсчётом ссылок и текстуры. | Экспортированный метод PassOwnership загружает ресурс, создаёт VideoClip и текстуру и сохраняет оба в VideoPlayback. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp) |
| <a id="entry-video-embedded-missing-file-6c5e775f2a"></a><code>video.embedded.missing-file</code> | Ошибка создания выбрасывает исключение | Перехватывайте или предотвращайте отсутствие управляемого скриптом видеоресурса; создание выбрасывает Video file not found. | В отличие от полноэкранного PlayVideo, CreateVideoPlayback превращает неудачный ReadFile в ScriptException. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp) |
| <a id="entry-video-embedded-render-event-77026ee7da"></a><code>video.embedded.render-event</code> | Рисование только в RenderIface | Вызывайте Game.DrawVideoPlayback только при обработке Game.OnRenderIface. | Метод выбрасывает исключение вне клиентской script draw scope. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp) |
| <a id="entry-video-embedded-positive-size-1a3e8315de"></a><code>video.embedded.positive-size</code> | Положительный размер цели продвигает воспроизведение | Передавайте положительные ширину и высоту в каждом кадре; нулевой или отрицательный размер пропускает декодирование, загрузку текстуры и рисование. | RenderFrame вызывается только внутри ветки положительного размера. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp) |
| <a id="entry-video-embedded-rectangle-c7450bb713"></a><code>video.embedded.rectangle</code> | Прямоугольник и пропорции принадлежат вызывающему коду | Выбирайте и поддерживайте целевой прямоугольник и политику пропорций в проектном UI-коде; Engine рисует точно указанную позицию и размер. | DrawVideoPlayback создаёт irect32 непосредственно из аргументов скрипта. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp) |
| <a id="entry-video-embedded-stopped-31b532e22e"></a><code>video.embedded.stopped</code> | Жизненный цикл поля Stopped | Опрашивайте VideoPlayback.Stopped, продолжая рисовать экземпляр; флаг становится true, когда DrawVideoPlayback обнаруживает остановку ролика. | Метод рисования очищает ресурсы и устанавливает экспортированное поле после проверки остановки. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp), [Source/Client/Client.h](https://github.com/cvet/fonline/blob/master/Source/Client/Client.h) |
| <a id="entry-video-embedded-null-noop-12f484cd61"></a><code>video.embedded.null-noop</code> | Null и завершённые экземпляры ничего не делают | Передача null или экземпляра с очищенными ресурсами воспроизведения ничего не рисует и не выбрасывает исключение. | DrawVideoPlayback рано возвращается для обоих состояний после проверки render scope. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp) |
