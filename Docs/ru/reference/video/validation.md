---
title: Контракт проверки видео
document_id: generated-video-validation
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-video-validation","locale":"ru","source_path":"Docs/en/reference/video/validation.md","source_sha256":"e55ce197708d18a39de4dc25a7d0de774caf8f16a5a3957027408a3e6dc4bdb1"} -->

# Контракт проверки видео

> Сгенерированный справочник. Не редактируйте его источник напрямую. Обновите `BuildTools/VideoInterface.json`, затем выполните `python BuildTools/docs_video.py --write`.

[Индекс](index.md) | [Форматы](formats.md) | [Доставка](delivery.md) | [Декодирование](decoding.md) | [Полный экран](fullscreen.md) | [Встроенное](embedded.md) | [Проверка](validation.md) | [Канонический JSON](../../../generated/video.json) | [Руководство](../../how-to/content/video.md)

| Стабильный ID | Правило | Требование | Причина | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-video-validation-raw-copy-d7b856e954"></a><code>video.validation.raw-copy</code> | Проверка доставленных байтов | Запеките и проверьте точный путь и байты клиентского ресурса до runtime-воспроизведения. | RawCopy является единственным преобразованием доставки, а runtime-пути точны. | [Source/Tools/RawCopyBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/RawCopyBaker.cpp) |
| <a id="entry-video-validation-visible-client-a82eb2ebe4"></a><code>video.validation.visible-client</code> | Требование видимого клиента | На каждой заявленной платформе проверьте в видимом клиенте первый кадр, движение, конец, пропуск, очередь, изменение размера и очистку текстуры. | Ни headless-, ни source-only-проверка не доказывает загрузку и показ текстуры. | [Source/Client/Client.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/Client.cpp) |
| <a id="entry-video-validation-no-native-fixture-4621f6e513"></a><code>video.validation.no-native-fixture</code> | Отсутствующий нативный video fixture | Считайте отсутствие Test_*Video* или Test_*Theora* пробелом покрытия и сохраняйте обязательными видимые регрессионные доказательства. | Текущий набор нативных тестов не содержит сфокусированного исходника декодера/воспроизведения видео. | [Source/Tests/README.md](https://github.com/cvet/fonline/blob/master/Source/Tests/README.md) |
| <a id="entry-video-validation-loop-risk-6c8fa0a021"></a><code>video.validation.loop-risk</code> | Зацикливание требует явного доказательства | Не обещайте зацикленные ролики, пока многоцикловая визуальная регрессия не докажет перемотку декодера и непрерывность кадров точного ресурса. | SetLooped доступен при создании, но текущие исходники не имеют сфокусированного loop fixture. | [Source/Client/VideoClip.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/VideoClip.cpp) |
| <a id="entry-video-validation-project-boundary-3c13a8c593"></a><code>video.validation.project-boundary</code> | Приёмка встраиваемого проекта | Встраиваемый проект владеет триггерами роликов, получателями, политикой пропуска/очереди, субтитрами, локализацией, вписыванием по пропорциям, аудиостратегией, последствиями сохранения, ресурсами, provenance, бюджетами и приёмочными тестами. | Engine поставляет примитивы декодера и показа, а не игровую систему роликов. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp) |

## Команды проверки

```powershell
python BuildTools\docs_video.py --check
python -m unittest BuildTools.tests.test_docs_video
cmake --build <build-dir> --config RelWithDebInfo --target RunUnitTests
```

Сфокусированный нативный fixture декодера/воспроизведения видео отсутствует. Видимый клиент должен доказать первый кадр, устойчивое движение, завершение, пропуск, переходы очереди, изменение размера, поведение связанной музыки, очистку и многоцикловое зацикливание для каждого точного ресурса и заявленной платформы.
