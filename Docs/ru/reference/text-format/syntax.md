---
title: Синтаксис текстового пакета
document_id: generated-text-format-syntax
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-text-format-syntax","locale":"ru","source_path":"Docs/en/reference/text-format/syntax.md","source_sha256":"fb0ead1682beb3f9f189608cc900d33070810ca10507349a808168e65436d57f"} -->

# Синтаксис текстового пакета

> Сгенерированный справочник. Не редактируйте его напрямую. Обновите `BuildTools/TextFormatInterface.json`, затем выполните `python BuildTools/docs_text_format.py --write`.

[Индекс](index.md) | [Синтаксис](syntax.md) | [Языки](languages.md) | [Текст прототипов](proto-text.md) | [Runtime](runtime.md) | [Проверка](validation.md) | [Канонический JSON](../../../generated/text-format.json) | [Руководство](../../how-to/content/text-and-localization.md)

Исходный текстовый файл задаёт коллекцию своим именем. Каждая разобранная логическая запись задаёт Key1, Key2 и Text:

```text
{Welcome}{}{Welcome to the wasteland.}
{QuestName}{Short}{A difficult choice}
{LongMessage}{}{First line
Second line}
```

Исходный `.fotxt` не задаёт Key3. Используйте поля `$Text` прототипа, когда принадлежащему прототипу ключу нужны одновременно Key2 и Key3.

| Стабильный ID | Правило | Требование | Причина | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-text-format-syntax-entry-b9ee5d2d47"></a><code>text-format.syntax.entry</code> | Форма исходной записи | Разобранная запись .fotxt содержит Key1, Key2 и Text в трёх полях, ограниченных фигурными скобками; коллекцию задаёт сегмент имени файла текстового пакета. | LoadFromString получает коллекцию отдельно и создаёт TextPackKey из двух заданных полей ключа. | [Source/Common/TextPack.cpp](https://github.com/cvet/fonline/blob/master/Source/Common/TextPack.cpp) |
| <a id="entry-text-format-syntax-key-tuple-fa1c37769d"></a><code>text-format.syntax.key-tuple</code> | Идентичность структурированного ключа | Идентичность TextPackKey состоит из Collection, Key1, Key2 и Key3, каждое из которых представлено обёрткой хешированной строки. | Исходные записи .fotxt оставляют Key3 пустым, а поля $Text прототипов могут заполнять Key2 и Key3. | [Source/Common/TextPack.h](https://github.com/cvet/fonline/blob/master/Source/Common/TextPack.h) |
| <a id="entry-text-format-syntax-multiline-text-cd9358a779"></a><code>text-format.syntax.multiline-text</code> | Многострочное значение | Только третье поле может продолжаться на физических строках; объединённые строки сохраняют переводы строк до первой закрывающей фигурной скобки. | Первые два вызова ExtractBraceToken запрещают многострочный ввод, а третий разрешает его. | [Source/Common/TextPack.cpp](https://github.com/cvet/fonline/blob/master/Source/Common/TextPack.cpp) |
| <a id="entry-text-format-syntax-comment-boundary-f69ad9c0ed"></a><code>text-format.syntax.comment-boundary</code> | Нет грамматики комментариев | Парсер пропускает физическую строку только тогда, когда не может найти первую открывающую фигурную скобку; соглашения о комментариях являются политикой проекта, а текст комментария не должен содержать разбираемые поля в фигурных скобках. | В LoadFromString нет ветки токенов комментария, и разбор начинается с первой открывающей фигурной скобки в любом месте строки. | [Source/Common/TextPack.cpp](https://github.com/cvet/fonline/blob/master/Source/Common/TextPack.cpp) |
| <a id="entry-text-format-syntax-empty-fields-e47e09fc8e"></a><code>text-format.syntax.empty-fields</code> | Обязательные и необязательные поля | Collection и Key1 не могут быть пустыми; Key2 и Text могут быть пустыми. | После извлечения всех трёх полей парсер отклоняет только пустую коллекцию или первый ключ. | [Source/Common/TextPack.cpp](https://github.com/cvet/fonline/blob/master/Source/Common/TextPack.cpp) |
| <a id="entry-text-format-syntax-variants-a3b452dc5d"></a><code>text-format.syntax.variants</code> | Варианты повторяющегося ключа | Несколько записей могут использовать один полный TextPackKey и оставаться соседними упорядоченными вариантами в отсортированном backing vector. | Количество вариантов и выбор по индексу используют один binary-search range; нормализация языков не выравнивает количество дубликатов, а read paths никогда не исправляют разделяемое состояние. | [Source/Common/TextPack.h](https://github.com/cvet/fonline/blob/master/Source/Common/TextPack.h), [Source/Tests/Test_TextPack.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_TextPack.cpp) |
| <a id="entry-text-format-syntax-no-closing-brace-escape-5b8f40ba36"></a><code>text-format.syntax.no-closing-brace-escape</code> | Граница закрывающей скобки | Первая закрывающая фигурная скобка завершает каждое поле; в исходном формате нет escape-последовательности для литеральной закрывающей скобки внутри Text. | ExtractBraceToken напрямую ищет следующую закрывающую скобку и не декодирует escape-последовательности. | [Source/Common/TextPack.cpp](https://github.com/cvet/fonline/blob/master/Source/Common/TextPack.cpp) |
