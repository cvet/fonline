---
title: Контракт проверки аудио
document_id: generated-audio-validation
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-audio-validation","locale":"ru","source_path":"Docs/en/reference/audio/validation.md","source_sha256":"80aa569f279fc01bd275f10629d062714e54391335b04b520b841698fe5664d5"} -->

# Контракт проверки аудио

> Сгенерированный справочник. Не редактируйте его источник напрямую. Обновите `BuildTools/AudioInterface.json`, затем выполните `python BuildTools/docs_audio.py --write`.

[Индекс](index.md) | [Форматы](formats.md) | [Доставка](delivery.md) | [Декодирование](decoding.md) | [Воспроизведение](playback.md) | [Проверка](validation.md) | [Канонический JSON](../../../generated/audio.json) | [Руководство](../../how-to/content/audio.md)

| Стабильный ID | Правило | Требование | Причина | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-audio-validation-generated-reference-657f0f8e81"></a><code>audio.validation.generated-reference</code> | Справочник по исходникам | Перегенерируйте и проверяйте audio-модель при изменении SoundManager, ResourceManager, AppAudio, настроек, raw-copy доставки или script entry points. | Сгенерированная модель выявляет drift расширений, декодеров, размера порции, defaults, настроек и headless-поведения до незаметного устаревания текста. | [BuildTools/docs_audio.py](https://github.com/cvet/fonline/blob/master/BuildTools/docs_audio.py) |
| <a id="entry-audio-validation-raw-copy-3e0a946475"></a><code>audio.validation.raw-copy</code> | Гейт baking | Запеките пакет с репрезентативным аудио и убедитесь, что ожидаемые относительные пути и исходные байты попали в клиентские ресурсы. | Проверки документации доказывают покрытие настроенных расширений, но не wiring ресурсных пакетов встраиваемого проекта. | [Source/Tests/Test_RawCopyBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_RawCopyBaker.cpp), [Source/Tools/RawCopyBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/RawCopyBaker.cpp) |
| <a id="entry-audio-validation-decoder-diagnostics-65927ba99d"></a><code>audio.validation.decoder-diagnostics</code> | Диагностика декодера | Считайте false-результаты воспроизведения и строки лога RIFF, PCM, ACM, Ogg, преобразования или неподдерживаемого формата ошибками авторинга. | Загрузчики сообщают о границах ошибок конкретных форматов вместо подстановки тишины как допустимого ресурса. | [Source/Client/SoundManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/SoundManager.cpp) |
| <a id="entry-audio-validation-headless-boundary-d3aa6fb0d9"></a><code>audio.validation.headless-boundary</code> | Граница headless | Не заявляйте слышимую проверку по headless- или stub-приложению; AppAudio там отключён, а воспроизведение может сообщить о no-op success. | Headless-аудио намеренно не предоставляет активного устройства или callback. | [Source/Frontend/ApplicationHeadless.cpp](https://github.com/cvet/fonline/blob/master/Source/Frontend/ApplicationHeadless.cpp) |
| <a id="entry-audio-validation-native-test-gap-4426613ecf"></a><code>audio.validation.native-test-gap</code> | Пробел сфокусированных нативных тестов | Сохраняйте видимым отсутствие fixture декодирования/воспроизведения SoundManager, пока тесты не покроют корректные и ошибочные WAV, ACM, Ogg, варианты, повтор и замену. | RawCopyBaker и широкие клиентские тесты не исполняют описанный здесь контракт кодеков и callback. | [Source/Tests/README.md](https://github.com/cvet/fonline/blob/master/Source/Tests/README.md), [BuildTools/docs_audio.py](https://github.com/cvet/fonline/blob/master/BuildTools/docs_audio.py) |
| <a id="entry-audio-validation-visible-client-4feb1d4bfa"></a><code>audio.validation.visible-client</code> | Видимая слышимая проверка | На каждой заявленной платформе используйте видимый клиент с включённым аудио: воспроизведите ресурс каждого формата, нумерованное семейство, замену композиции, отложенный и немедленный повтор и крайние значения громкости. | Только активное платформенное аудиоустройство доказывает преобразование, планирование callback, микширование и слышимый вывод. | [Source/Frontend/Application.cpp](https://github.com/cvet/fonline/blob/master/Source/Frontend/Application.cpp), [Source/Client/SoundManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/SoundManager.cpp) |
| <a id="entry-audio-validation-project-boundary-66ad789ce4"></a><code>audio.validation.project-boundary</code> | Владение встраиваемого проекта | Храните соглашения каталога, spatial/recipient-политику, мастеринг, лицензии, атрибуцию, бюджеты и игровые триггеры в проектной документации и тестах. | Движок предоставляет декодирование и глобальный клиентский mixer, а не полный игровой аудиодизайн или систему управления ресурсами. | [Source/Client/SoundManager.h](https://github.com/cvet/fonline/blob/master/Source/Client/SoundManager.h) |

## Команды проверки

```powershell
python BuildTools\docs_audio.py --check
python -m unittest BuildTools.tests.test_docs_audio
cmake --build <build-dir> --config RelWithDebInfo --target RunUnitTests
```

Сейчас нет сфокусированного нативного fixture декодера/воспроизведения. Встраиваемый проект также должен запечь репрезентативные WAV, ACM и Ogg, вызвать эффекты и музыку в видимом клиенте с включённым аудио, проверить логи и поведение громкости/повтора на каждой заявленной платформе.
