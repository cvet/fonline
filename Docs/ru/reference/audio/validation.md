---
title: Контракт проверки аудио
document_id: generated-audio-validation
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-audio-validation","locale":"ru","source_path":"Docs/en/reference/audio/validation.md","source_sha256":"9193c841da86ea9ddb384e497bbc0eb7ac2eea9ca9421e35435ffe5929db041c"} -->

# Контракт проверки аудио

> Сгенерированный справочник. Не редактируйте его источник напрямую. Обновите `BuildTools/AudioInterface.json`, затем выполните `python BuildTools/docs_audio.py --write`.

[Индекс](index.md) | [Форматы](formats.md) | [Доставка](delivery.md) | [Декодирование](decoding.md) | [Воспроизведение](playback.md) | [Проверка](validation.md) | [Канонический JSON](../../../generated/audio.json) | [Руководство](../../how-to/content/audio.md)

| Стабильный ID | Правило | Требование | Причина | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-audio-validation-generated-reference-657f0f8e81"></a><code>audio.validation.generated-reference</code> | Справочник по исходникам | Перегенерируйте и проверяйте audio-модель при изменении AudioBaker, AudioManager, AppAudio, настроек, доставки пакета или script entry points. | Сгенерированная модель выявляет drift расширений исходников, runtime-кодека, размера порции, настройки, handle, набора тестов и headless-поведения до незаметного устаревания текста. | [BuildTools/docs_audio.py](https://github.com/cvet/fonline/blob/master/BuildTools/docs_audio.py) |
| <a id="entry-audio-validation-raw-copy-3e0a946475"></a><code>audio.validation.raw-copy</code> | Гейт запекания аудио | Запеките репрезентативные входы PCM/float WAV и нативного Ogg, убедитесь в сохранении авторских путей, проверьте счётчики аудио и откройте каждый результат как Vorbis. | Сфокусированные тесты доказывают преобразование и passthrough, а запекание во встраиваемом проекте доказывает, что его пакет действительно выбирает Audio и поставляет ресурсы, используемые скриптами. | [Source/Tests/Test_AudioBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_AudioBaker.cpp), [Source/Tools/AudioBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/AudioBaker.cpp) |
| <a id="entry-audio-validation-decoder-diagnostics-65927ba99d"></a><code>audio.validation.decoder-diagnostics</code> | Диагностика декодера | Считайте исключения AudioBaker и диагностику открытия Ogg, декодирования или преобразования устройства ошибками авторинга либо упаковки; не принимайте нулевой handle воспроизведения за проверку контента. | WAV и Ogg без перекодирования отклоняются во время запекания, а существующий ресурс, который нельзя декодировать в runtime, записывает ошибку и входит в debugger вместо незаметного успеха с тишиной. | [Source/Tools/AudioBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/AudioBaker.cpp), [Source/Client/AudioManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/AudioManager.cpp) |
| <a id="entry-audio-validation-headless-boundary-d3aa6fb0d9"></a><code>audio.validation.headless-boundary</code> | Граница headless | Не заявляйте слышимую проверку по headless- или stub-приложению; AppAudio там отключён, а воспроизведение может сообщить о no-op success. | Headless-аудио намеренно не предоставляет активного устройства или callback. | [Source/Frontend/ApplicationHeadless.cpp](https://github.com/cvet/fonline/blob/master/Source/Frontend/ApplicationHeadless.cpp) |
| <a id="entry-audio-validation-native-test-gap-4426613ecf"></a><code>audio.validation.native-test-gap</code> | Сфокусированное нативное покрытие | Запускайте Test_AudioBaker и Test_AudioManager для запекания WAV/Ogg, passthrough, ошибочных входов, панорамирования, позиционированного воспроизведения, текущих обновлений, истечения handles и вывода mixer. | Эти suites напрямую исполняют контракт кодека и callback; видимый клиент всё ещё нужен, чтобы доказать работу реального платформенного устройства и слышимый вывод. | [Source/Tests/README.md](https://github.com/cvet/fonline/blob/master/Source/Tests/README.md), [BuildTools/docs_audio.py](https://github.com/cvet/fonline/blob/master/BuildTools/docs_audio.py) |
| <a id="entry-audio-validation-visible-client-4feb1d4bfa"></a><code>audio.validation.visible-client</code> | Видимая слышимая проверка | На каждой заявленной платформе используйте видимый клиент с включённым аудио: воспроизведите запечённый ресурс с WAV-путём и ресурс с нативным Ogg-путём, переместите позиционированный звук, замените музыку, проверьте отложенный и немедленный повтор и крайние значения громкости. | Только активное платформенное аудиоустройство доказывает преобразование, планирование callback, микширование и слышимый вывод. | [Source/Frontend/Application.cpp](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.cpp), [Source/Client/AudioManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/AudioManager.cpp) |
| <a id="entry-audio-validation-project-boundary-66ad789ce4"></a><code>audio.validation.project-boundary</code> | Владение встраиваемого проекта | Храните соглашения каталога, spatial/recipient-политику, мастеринг, лицензии, атрибуцию, бюджеты и игровые триггеры в проектной документации и тестах. | Движок предоставляет запекание, runtime-декодирование, клиентский mixer и параметры позиционирования, а не полный игровой аудиодизайн или систему управления ресурсами. | [Source/Client/AudioManager.h](https://github.com/cvet/fonline/blob/master/Source/Client/AudioManager.h) |

## Команды проверки

```powershell
python BuildTools\docs_audio.py --check
python -m unittest BuildTools.tests.test_docs_audio
cmake --build <build-dir> --config RelWithDebInfo --target RunUnitTests
```

Сейчас нет сфокусированного нативного fixture декодера/воспроизведения. Встраиваемый проект также должен запечь репрезентативные WAV, ACM и Ogg, вызвать эффекты и музыку в видимом клиенте с включённым аудио, проверить логи и поведение громкости/повтора на каждой заявленной платформе.
