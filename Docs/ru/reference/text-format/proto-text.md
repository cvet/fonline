---
title: Текст прототипов
document_id: generated-text-format-proto-text
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-text-format-proto-text","locale":"ru","source_path":"Docs/en/reference/text-format/proto-text.md","source_sha256":"c8ffd94a205e8b4217c86589833f8897fe7553f7cf125a48dfe3e0ce055ac4f6"} -->

# Текст прототипов

> Сгенерированный справочник. Не редактируйте его напрямую. Обновите `BuildTools/TextFormatInterface.json`, затем выполните `python BuildTools/docs_text_format.py --write`.

[Индекс](index.md) | [Синтаксис](syntax.md) | [Языки](languages.md) | [Текст прототипов](proto-text.md) | [Runtime](runtime.md) | [Проверка](validation.md) | [Канонический JSON](../../../generated/text-format.json) | [Руководство](../../how-to/content/text-and-localization.md)

Локализованный текст прототипа задаётся внутри любой допустимой секции прототипа:

```ini
[ProtoItem]
$Name = LaserRifle
$Text engl Name = Laser rifle
$Text engl Desc Short = Compact description
$Text russ Name = Localized name
```

Полный ключ состоит из сгенерированного пакета, id прототипа, необязательного Key2 и необязательного Key3. Если язык опущен, выбирается первый настроенный элемент `BakeLanguages`.

## Генерируемые пакеты

<code>Items</code>, <code>Critters</code>, <code>Maps</code>, <code>Locations</code>, <code>Protos</code>

| Стабильный ID | Правило | Требование | Причина | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-text-format-proto-syntax-712e1b0662"></a><code>text-format.proto.syntax</code> | Ключ текста прототипа | Текст прототипа использует $Text [Language] [Key2] [Key3] = Value и принимает не более четырёх токенов ключа вместе с $Text. | Id прототипа становится Key1, а необязательные последующие токены становятся Key2 и Key3. | [Source/Tools/ProtoTextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ProtoTextBaker.cpp) |
| <a id="entry-text-format-proto-default-language-38c15ef84b"></a><code>text-format.proto.default-language</code> | Опущенный язык | Если токен языка опущен, поле $Text принадлежит первому элементу Baking.BakeLanguages. | С исходными текстовыми пакетами используется один упорядоченный контракт базового языка. | [Source/Tools/ProtoTextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ProtoTextBaker.cpp) |
| <a id="entry-text-format-proto-escape-decoding-c7c388d194"></a><code>text-format.proto.escape-decoding</code> | Декодирование escape-последовательностей | Значения $Text прототипов проходят через StringEscaping::DecodeString до добавления в текстовый пакет. | Последовательности наподобие \n становятся настоящими символами новой строки в запечённом значении. | [Source/Tools/ProtoTextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ProtoTextBaker.cpp), [Source/Tests/Test_ProtoTextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_ProtoTextBaker.cpp) |
| <a id="entry-text-format-proto-inheritance-bf3c06135b"></a><code>text-format.proto.inheritance</code> | Унаследованный текст | Родительские поля $Text рекурсивно объединяются до полей потомка. | Потомок наследует отсутствующие точные текстовые ключи, а его собственное присваивание точного ключа заменяет унаследованное значение. | [Source/Tools/ProtoTextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ProtoTextBaker.cpp) |
| <a id="entry-text-format-proto-pack-routing-e03d4dcd20"></a><code>text-format.proto.pack-routing</code> | Маршрутизация генерируемого пакета | Прототипы Item, Critter, Map и Location направляются в Items, Critters, Maps и Locations; другие неэкспортируемые entity или fixed type с HasProtos направляются в Protos. | Эти пять имён образуют переиспользуемый набор текстовых пакетов прототипов, генерируемых Engine. | [Source/Tools/ProtoTextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ProtoTextBaker.cpp) |
| <a id="entry-text-format-proto-complete-output-set-fafcdb598b"></a><code>text-format.proto.complete-output-set</code> | Полный набор выходов | ProtoTextBaker создаёт все пять генерируемых пакетов для каждого настроенного языка, включая пустые пакеты. | Форма бинарного выхода детерминирована для разных проектов и наборов языков. | [Source/Tools/ProtoTextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ProtoTextBaker.cpp) |
| <a id="entry-text-format-proto-intersections-9cf6753839"></a><code>text-format.proto.intersections</code> | Межтиповые пересечения | Два исходника прототипов не могут создать один полный ключ в одном сгенерированном пакете и языке. | Пересечения считаются ошибками и прерывают запекание текста прототипов. | [Source/Tools/ProtoTextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ProtoTextBaker.cpp) |
| <a id="entry-text-format-proto-unsupported-language-b3301197c5"></a><code>text-format.proto.unsupported-language</code> | Неподдерживаемый язык прототипа | Для языка $Text, отсутствующего в Baking.BakeLanguages, выводится предупреждение, и поле пропускается. | Локализованный текст прототипов следует настроенному набору выходных языков. | [Source/Tools/ProtoTextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ProtoTextBaker.cpp) |
