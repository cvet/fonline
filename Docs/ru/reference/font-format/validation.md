---
title: Контракт проверки шрифтов
document_id: generated-font-format-validation
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-font-format-validation","locale":"ru","source_path":"Docs/en/reference/font-format/validation.md","source_sha256":"0ce2479e51d7f46a0c4d83af638207f3a4cf2d605e8460140765250ca296fa94"} -->

# Контракт проверки шрифтов

> Сгенерированный справочник. Не редактируйте его напрямую. Обновите `BuildTools/FontFormatInterface.json`, затем выполните `python BuildTools/docs_font_format.py --write`.

[Индекс](index.md) | [Форматы](formats.md) | [FOFNT](fofnt.md) | [BMFont](bmfont.md) | [Привязка](binding.md) | [Компоновка](layout.md) | [Отрисовка](rendering.md) | [Проверка](validation.md) | [Канонический JSON](../../../generated/font-format.json) | [Руководство](../../how-to/content/font-format.md)

| Стабильный ID | Правило | Требование | Причина | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-font-format-validation-descriptor-and-image-presence-aa2fae5bd3"></a><code>font-format.validation.descriptor-and-image-presence</code> | Наличие дескриптора и изображения | Считайте ресурс непрошедшим gate, если дескриптор отсутствует, FOFNT не содержит Image или относительное изображение не загружается как спрайт атласа. Runtime сообщает об отсутствующем дескрипторе и файле изображения, но сейчас не имеет явной проверки пустого Image. | Проверка до запуска должна покрывать отсутствие Image, потому что загрузчик достигает image_name.back() раньше, чем может выдать диагностическое сообщение. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-validation-fofnt-header-3b9e8b7f37"></a><code>font-format.validation.fofnt-header</code> | Заголовок FOFNT и UTF-8 | Отклоняйте FOFNT, у которого первый ключ не Version, версия выше 2 или строка Letter не содержит одной корректной кодовой точки UTF-8. | Это жёсткие ошибки парсера, а не восстанавливаемые случаи отсутствующего глифа. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-validation-bmfont-header-aa7522bad1"></a><code>font-format.validation.bmfont-header</code> | Заголовок, отступы и страницы BMFont | Отклоняйте дескрипторы BMFont не binary v3, без отступов 1/1/1/1 или с числом страниц не равным одному. | Runtime имеет явные исключения для всех трёх несовместимостей. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-validation-signed-bmfont-metrics-bf09e269db"></a><code>font-format.validation.signed-bmfont-metrics</code> | Ограничение беззнаковых метрик BMFont | Учитывайте, что текущий загрузчик читает xoffset, yoffset и xadvance через GetLEUInt16, хотя поставляемые бинарные шрифты содержат отрицательные выносы. | Значения вроде -2 сейчас интерпретируются как 65534 и могут смещать отрисованный глиф далеко от ожидаемой позиции; исправление runtime должно быть отдельным изменением кода. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp), [Source/Common/FileSystem.h](https://github.com/cvet/fonline/blob/master/Source/Common/FileSystem.h) |
| <a id="entry-font-format-validation-scale-range-ab30f43b55"></a><code>font-format.validation.scale-range</code> | Диапазон масштаба | Отклоняйте NaN, бесконечность, ноль, отрицательные значения и значения больше единицы до изменения таблицы шрифтов или атласа. | Engine поддерживает детерминированное уменьшение при привязке, а не увеличение растра. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-validation-generated-contract-40d35172b7"></a><code>font-format.validation.generated-contract</code> | Расхождение сгенерированного контракта | Перегенерируйте и проверяйте модель font-format при изменении ключей парсера, бинарных констант, enum шрифтов, диспетчеризации привязки, raw-copy, масштаба, кэша или поставляемых дескрипторов. | Проверяемая модель заставляет CI отклонить незаметное расхождение исходников и документации. | [BuildTools/docs_font_format.py](https://github.com/cvet/fonline/blob/master/BuildTools/docs_font_format.py) |
| <a id="entry-font-format-validation-engine-tests-0ba5ebbb02"></a><code>font-format.validation.engine-tests</code> | Регрессионные gate Engine | После изменений FontManager или дескрипторов запускайте целевой тест документации и полный сгенерированный target модульных тестов Engine. | Структурные проверки фиксируют выведенные из исходников контракты, а нативные тесты покрывают пути ресурсов и создания клиента. | [Source/Tests/Test_Mapper.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_Mapper.cpp), [Source/Tests/Test_ClientServerIntegration.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_ClientServerIntegration.cpp) |
| <a id="entry-font-format-validation-embedding-project-3dcc84a5cf"></a><code>font-format.validation.embedding-project</code> | Запекание и видимая проверка проекта | Запекайте дескриптор и изображение, запускайте измерения каждого масштаба и визуально проверяйте обычные, обведённые, перенесённые, выровненные, локализованные случаи и отсутствующие глифы. | Успешный raw-copy не доказывает покрытие глифов, отступ атласа, типографику, backend отрисовки или посадку GUI. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp), [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |

## Команды проверки

```powershell
python BuildTools\docs_font_format.py --check
python -m unittest BuildTools.tests.test_docs_font_format
cmake --build <build-dir> --config RelWithDebInfo --target RunUnitTests
```

Подключаемый проект также должен совместно запечь дескриптор и изображение, запустить регрессию измерения текста и проверить в видимом клиенте репрезентативный обычный, обведённый, масштабированный, перенесённый и локализованный текст.
