---
title: Языки и нормализация
document_id: generated-text-format-languages
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-text-format-languages","locale":"ru","source_path":"Docs/en/reference/text-format/languages.md","source_sha256":"fca68c75d4873868ed9d540b2ba9b5cb3fca877d43b82fbd934ba74f0bd103d8"} -->

# Языки и нормализация

> Сгенерированный справочник. Не редактируйте его напрямую. Обновите `BuildTools/TextFormatInterface.json`, затем выполните `python BuildTools/docs_text_format.py --write`.

[Индекс](index.md) | [Синтаксис](syntax.md) | [Языки](languages.md) | [Текст прототипов](proto-text.md) | [Runtime](runtime.md) | [Проверка](validation.md) | [Канонический JSON](../../../generated/text-format.json) | [Руководство](../../how-to/content/text-and-localization.md)

Языковой fallback материализуется baker-ами. Runtime lookup читает выбранный бинарный языковой пакет и не обращается к базовому пакету.

## Значения движка по умолчанию

| Настройка | Исходное значение по умолчанию | Смысл |
| --- | --- | --- |
| `Baking.BakeLanguages` | <code>engl</code> | Упорядоченные выходные языки; первый служит базой нормализации. |
| `Client.Language` | <code>engl</code> | Начальный пакет текущего языка, загружаемый клиентом. |

| Стабильный ID | Правило | Требование | Причина | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-text-format-language-filename-978417b676"></a><code>text-format.language.filename</code> | Имя исходного файла | Базовое имя .fotxt содержит ровно два сегмента, разделённых точкой: имя текстового пакета и идентификатор языка. | TextBaker отклоняет любую другую форму базового имени. | [Source/Tools/TextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/TextBaker.cpp) |
| <a id="entry-text-format-language-identifier-ce551dd6b3"></a><code>text-format.language.identifier</code> | Идентификатор языка | Engine рассматривает суффикс имени файла как непрозрачную настроенную строку и не накладывает фиксированного ограничения на число символов. | Допустимость определяется равенством имени языка, разобранному из Baking.BakeLanguages; валидатора длины или формы локали нет. | [Source/Tools/TextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/TextBaker.cpp) |
| <a id="entry-text-format-language-base-c0360baaa0"></a><code>text-format.language.base</code> | Базовый язык | Baking.BakeLanguages не может быть пустым, а его первый bare language является базой нормализации по умолчанию. | TextPack::ParseBakeLanguages проверяет упорядоченные declarations до их использования любым text baker. | [Source/Tools/TextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/TextBaker.cpp), [Source/Common/TextPack.cpp](https://github.com/cvet/fonline/blob/master/Source/Common/TextPack.cpp) |
| <a id="entry-text-format-language-fallback-declaration-48971fa338"></a><code>text-format.language.fallback-declaration</code> | Объявление fallback | Каждая запись BakeLanguages имеет форму language или child:parent; имена уникальны, а parent обязан находиться раньше child. | Явный parent создаёт упорядоченную bake-time fallback chain, а bare language после первого использует fallback на первый язык. | [Source/Common/TextPack.cpp](https://github.com/cvet/fonline/blob/master/Source/Common/TextPack.cpp), [Source/Tests/Test_TextPack.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_TextPack.cpp) |
| <a id="entry-text-format-language-default-source-required-002dd0909e"></a><code>text-format.language.default-source-required</code> | Обязательный базовый исходник | Для каждого изменённого исходного текстового пакета выбранный набор файлов должен содержать исходник базового языка. | TextBaker не может нормализовать изменённый пакет без базового языка и вызывает исключение до разбора выхода. | [Source/Tools/TextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/TextBaker.cpp) |
| <a id="entry-text-format-language-incremental-pack-set-df1a474be2"></a><code>text-format.language.incremental-pack-set</code> | Дополнение инкрементального набора | При изменении одного языкового файла текстового пакета TextBaker включает в повторное запекание все настроенные языковые файлы того же пакета. | Частичное обновление языка всё равно должно пройти нормализацию полного пакета. | [Source/Tools/TextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/TextBaker.cpp), [Source/Tests/Test_TextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_TextBaker.cpp) |
| <a id="entry-text-format-language-unsupported-237d34519f"></a><code>text-format.language.unsupported</code> | Неподдерживаемый язык | Для исходного текстового файла с суффиксом, отсутствующим в Baking.BakeLanguages, выводится предупреждение, а файл пропускается. | Неподдерживаемые исходные языки не создают бинарных пакетов. | [Source/Tools/TextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/TextBaker.cpp) |
| <a id="entry-text-format-language-bake-time-fallback-7fa08c4b83"></a><code>text-format.language.bake-time-fallback</code> | Fallback при запекании | Отсутствующие пакеты и ключи копируются из настроенного fallback каждого языка во время запекания; пакеты и ключи, отсутствующие в этом fallback, удаляются. | Runtime lookup загружает уже нормализованные бинарные пакеты и не выполняет fallback. | [Source/Common/TextPack.cpp](https://github.com/cvet/fonline/blob/master/Source/Common/TextPack.cpp) |
| <a id="entry-text-format-language-variant-cardinality-5ec37b5527"></a><code>text-format.language.variant-cardinality</code> | Количество вариантов | Нормализация выравнивает наличие ключей, но не количество или порядок повторяющихся вариантов существующего ключа. | FixStr проверяет только нулевое количество ключа и сохраняет все существующие варианты, когда ключ существует. | [Source/Common/TextPack.cpp](https://github.com/cvet/fonline/blob/master/Source/Common/TextPack.cpp) |
| <a id="entry-text-format-language-binary-name-b4b2342893"></a><code>text-format.language.binary-name</code> | Имя запечённого файла | Baker-ы исходного текста и текста прототипов создают &lt;ResourcePack&gt;.&lt;TextPack&gt;.&lt;Language&gt;.fotxt-bin. | Позднее TextPack::LoadFromResources требует ровно три таких сегмента базового имени. | [Source/Tools/TextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/TextBaker.cpp), [Source/Common/TextPack.cpp](https://github.com/cvet/fonline/blob/master/Source/Common/TextPack.cpp) |
