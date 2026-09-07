---
title: Runtime API текста
document_id: generated-text-format-runtime
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-text-format-runtime","locale":"ru","source_path":"Docs/en/reference/text-format/runtime.md","source_sha256":"00567811d45b7005748a5ba4e082ed92123d22a6e845b3282e0ec22e96daa83b"} -->

# Runtime API текста

> Сгенерированный справочник. Не редактируйте его напрямую. Обновите `BuildTools/TextFormatInterface.json`, затем выполните `python BuildTools/docs_text_format.py --write`.

[Индекс](index.md) | [Синтаксис](syntax.md) | [Языки](languages.md) | [Текст прототипов](proto-text.md) | [Runtime](runtime.md) | [Проверка](validation.md) | [Канонический JSON](../../../generated/text-format.json) | [Руководство](../../how-to/content/text-and-localization.md)

Точными экспортируемыми сигнатурами владеет сгенерированный справочник API. Эта страница описывает выбор, поведение при отсутствии данных и доступность по сторонам.

## Script-методы

| Стабильный ID | Сигнатура | Стороны | Поведение | Отсутствующие или некорректные данные | Источник |
| --- | --- | --- | --- | --- | --- |
| <a id="entry-text-format-runtime-get-language-3fe5c8a9cf"></a><code>text-format.runtime.get-language</code> | <code>LanguageName Game.GetLanguage()</code> | <code>server</code>, <code>client</code>, <code>mapper</code> | Возвращает текущую настройку Engine Language в строгой обёртке LanguageName. | Неприменимо. | [Source/Scripting/CommonGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/CommonGlobalScriptMethods.cpp) |
| <a id="entry-text-format-runtime-get-current-text-5e96e49ab2"></a><code>text-format.runtime.get-current-text</code> | <code>string Game.GetText(TextPackKey textKey)</code> | <code>client</code>, <code>mapper</code> | Возвращает первый вариант из текущего языка клиента. | Отсутствующий ключ возвращает пустую строку. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp) |
| <a id="entry-text-format-runtime-get-indexed-text-1c9e1cc0c1"></a><code>text-format.runtime.get-indexed-text</code> | <code>string Game.GetText(TextPackKey textKey, int32 textIndex)</code> | <code>client</code>, <code>mapper</code> | Возвращает вариант из текущего языка клиента, выбранный textIndex по индексу с нуля. | Отсутствующий ключ или выход за диапазон возвращает пустую строку; отрицательный индекс вызывает исключение. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp) |
| <a id="entry-text-format-runtime-get-language-text-eeccc868d4"></a><code>text-format.runtime.get-language-text</code> | <code>string Game.GetText(string langName, TextPackKey textKey)</code> | <code>client</code>, <code>mapper</code> | Пустой langName или текущий язык использует текущий пакет; другой langName загружает либо повторно использует кешированный пакет и возвращает первый вариант. | Для отсутствующего или неподдерживаемого непустого языка нет автоматического fallback, поэтому ключ даёт пустую строку. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp), [Source/Client/Client.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/Client.cpp) |
| <a id="entry-text-format-runtime-get-count-ce8c833d65"></a><code>text-format.runtime.get-count</code> | <code>int32 Game.GetTextCount(TextPackKey textKey)</code> | <code>server</code>, <code>client</code>, <code>mapper</code> | Возвращает число вариантов, сохранённых под полным ключом. | Возвращает ноль при отсутствии ключа. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp), [Source/Scripting/ServerGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ServerGlobalScriptMethods.cpp) |
| <a id="entry-text-format-runtime-is-present-3705f3473d"></a><code>text-format.runtime.is-present</code> | <code>bool Game.IsTextPresent(TextPackKey textKey)</code> | <code>server</code>, <code>client</code>, <code>mapper</code> | Сообщает, существует ли хотя бы один вариант под полным ключом. | Возвращает false при отсутствии ключа. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp), [Source/Scripting/ServerGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ServerGlobalScriptMethods.cpp) |
| <a id="entry-text-format-runtime-change-language-f4b573ddec"></a><code>text-format.runtime.change-language</code> | <code>void Game.ChangeLanguage(string langName)</code> | <code>client</code>, <code>mapper</code> | Загружает langName в текущий пакет клиента и записывает настройку Language. | Engine не проверяет идентификатор, не применяет другой fallback и не вызывает callback проекта для обновления GUI. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp), [Source/Client/Client.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/Client.cpp) |
| <a id="entry-text-format-runtime-server-boundary-2b4580120e"></a><code>text-format.runtime.server-boundary</code> | <code>server: IsTextPresent and GetTextCount only</code> | <code>server</code> | Сервер загружает один пакет для Settings.Language и предоставляет запросы наличия и количества. | В API Engine нет server script overload Game.GetText. | [Source/Server/Server.cpp](https://github.com/cvet/fonline/blob/master/Source/Server/Server.cpp), [Source/Scripting/ServerGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ServerGlobalScriptMethods.cpp) |

## Встроенные теги рендерера

| Стабильный ID | Правило | Требование | Причина | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-text-format-rendering-inline-color-bd3b5426c1"></a><code>text-format.rendering.inline-color</code> | Встроенные цветовые теги | Клиентский рендерер шрифтов распознаёт @color:HEX@ для добавления цвета из шести или восьми шестнадцатеричных цифр и @color@ для восстановления предыдущего цвета. | Цветовые теги являются синтаксисом рендерера, независимым от разбора текстового пакета и проектного форматирования lexem. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-text-format-rendering-no-colorize-a6ac56231f"></a><code>text-format.rendering.no-colorize</code> | NoColorize | FontFlag.NoColorize удаляет допустимые встроенные цветовые теги, отрисовывая все глифы базовым цветом. | Проход форматирования всегда удаляет распознанные теги, но записывает цвета отдельных глифов только при включённой колоризации. | [Source/Client/FontManager.h](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.h), [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |

Движок не интерпретирует игровые lexem наподобие тегов имени игрока, пола, аргумента, вложенного текста или случайного выбора. Подключаемый проект, добавляющий их, владеет их грамматикой, тестами, диагностикой и порядком относительно цветовых тегов рендерера.
