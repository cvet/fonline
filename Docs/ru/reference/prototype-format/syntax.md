---
title: Синтаксис файлов прототипов
document_id: generated-prototype-format-syntax
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-prototype-format-syntax","locale":"ru","source_path":"Docs/en/reference/prototype-format/syntax.md","source_sha256":"ae0c6cbc8f7777b6a27bb4af429611b747cde29119cd351d7699c7702a385ae6"} -->

# Синтаксис файлов прототипов

> Сгенерированный справочник. Не редактируйте напрямую. Обновите `BuildTools/PrototypeFormatInterface.json` или владеющие метаданные движка, затем запустите `python BuildTools/docs_prototype_format.py --write`.

[Индекс](index.md) | [Синтаксис](syntax.md) | [Свойства](properties.md) | [Валидация](validation.md) | [Каноническая JSON-модель](../../../generated/prototype-format.json) | [Руководство](../../how-to/content/prototype-format.md)

`Baking.ProtoFileExtensions` выбирает входные файлы. Значения движка по умолчанию: <code>fopro</code>. Встраивающий проект может добавлять расширения, не меняя разбор секций.

Каждый pack создаёт `<pack>.fopro-bin-<side>` для <code>server</code>, <code>client</code>, <code>mapper</code>. Каждая верхнеуровневая секция `[ProtoMap]` добавляет прототип Map; вложенные секции размещения на карте пропускаются.

## Формы секций

| Стабильный ID | Синтаксис | Разрешение | Значение |
| --- | --- | --- | --- |
| <a id="entry-prototype-format-section-proto-entity-2782eabb8e"></a><code>prototype-format.section.proto-entity</code> | <code>[Proto&lt;Type&gt;]</code> | Метаданные сущности типа &lt;Type&gt; с HasProtos | Объявляет прототип встроенного или проектного типа сущности, в метаданных которого включены прототипы. |
| <a id="entry-prototype-format-section-fixed-type-12fc343b51"></a><code>prototype-format.section.fixed-type</code> | <code>[&lt;FixedType&gt;]</code> | Тип метаданных, объявленный через ///@ FixedType | Объявляет подобное прототипу фиксированное значение, заданное метаданными встраивающего проекта. |
| <a id="entry-prototype-format-section-fomap-proto-map-88acc25d02"></a><code>prototype-format.section.fomap-proto-map</code> | <code>[ProtoMap]</code> | Map | Каждая верхнеуровневая секция ProtoMap объявляет прототип Map. Вложенные секции размещения $Name/Item и $Name/Critter пропускаются ProtoBaker и обрабатываются MapBaker. |

## Управляющие директивы

| Стабильный ID | Синтаксис | Значение по умолчанию | Значение |
| --- | --- | --- | --- |
| <a id="entry-prototype-format-directive-name-3d7ea9d8be"></a><code>prototype-format.directive.name</code> | <code>$Name = &lt;PrototypeId&gt;</code> | имя исходного файла без расширения | Задаёт идентичность прототипа внутри разрешённого типа. Миграция ID выполняется до проверки дубликатов. |
| <a id="entry-prototype-format-directive-parent-2b3d1ad932"></a><code>prototype-format.directive.parent</code> | <code>$Parent = &lt;ParentId&gt; [&lt;ParentId&gt; ...]</code> | без родителей | Перечисляет через пробел прототипы-родители того же типа. Родители объединяются слева направо, затем применяется дочерний прототип. |

## Минимальный пример

```ini
[ProtoItem]
$Name = BaseItem

[ProtoItem]
$Name = DerivedItem
$Parent = BaseItem
```

Тип выбирает секция, а не расширение или каталог. Значения разбирает общий parser конфигурации, включая комментарии `#`, продолжение строки конечным обратным слешем после пробела или табуляции и синтаксис добавления `key += value`.
