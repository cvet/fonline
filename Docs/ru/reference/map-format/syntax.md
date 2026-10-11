---
title: Синтаксис файла карты
document_id: generated-map-format-syntax
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-map-format-syntax","locale":"ru","source_path":"Docs/en/reference/map-format/syntax.md","source_sha256":"a35812c193f7fd439d396f1b60c119ec337f86cbdd2b52b3b3df334db4a5b3cb"} -->

# Синтаксис файла карты

> Сгенерированный справочник. Не редактируйте напрямую. Обновите `BuildTools/MapFormatInterface.json` или владеющие метаданные движка, затем запустите `python BuildTools/docs_map_format.py --write`.

[Индекс](index.md) | [Синтаксис](syntax.md) | [Свойства](properties.md) | [Запекание](baking.md) | [Валидация](validation.md) | [Каноническая JSON-модель](../../../generated/map-format.json) | [Руководство по авторингу](../../how-to/content/map-format.md)

Контейнер карт является настроенным файлом прототипов с одним или несколькими якорями `[ProtoMap]`. Каждый якорь владеет вложенными секциями `[$Name/Critter]` и `[$Name/Item]`; вместо `$Name` можно использовать явный ID карты.

## Формы секций

| Стабильный ID | Синтаксис | Получатель | Кардинальность | Значение |
| --- | --- | --- | --- | --- |
| <a id="entry-map-format-section-proto-map-87b500e90a"></a><code>map-format.section.proto-map</code> | <code>[ProtoMap]</code> | <code>Map</code> | одна или несколько на контейнер | Объявляет свойства уровня карты и начинает контекст для следующих секций [$Name/Critter] и [$Name/Item]. |
| <a id="entry-map-format-section-critter-43a7c80eec"></a><code>map-format.section.critter</code> | <code>[$Name/Critter] or [&lt;MapId&gt;/Critter]</code> | <code>Critter</code> | ноль или больше на объявленную карту | Размещает прототип криттера и применяет переопределения свойств Critter для размещения. |
| <a id="entry-map-format-section-item-4bff4b001c"></a><code>map-format.section.item</code> | <code>[$Name/Item] or [&lt;MapId&gt;/Item]</code> | <code>Item</code> | ноль или больше на объявленную карту | Размещает предмет карты, инвентаря или контейнера и применяет переопределения свойств Item для размещения. |

## Управляющие директивы

| Директива | Секции | Синтаксис | Обязательна | По умолчанию | Значение |
| --- | --- | --- | --- | --- | --- |
| <a id="entry-map-format-directive-map-name-75435bbdeb"></a><code>$Name</code> | ProtoMap | <code>$Name = &lt;MapId&gt;</code> | нет | базовое имя исходного файла | Выбирает ID прототипа Map и базовые имена обоих запечённых ресурсов. Без $Name якорь разрешается в базовое имя исходника; в контейнере с несколькими картами каждый якорь следует именовать явно. |
| <a id="entry-map-format-directive-map-parent-a0e92e0e8a"></a><code>$Parent</code> | ProtoMap | <code>$Parent = &lt;ParentMapId&gt; [&lt;ParentMapId&gt; ...]</code> | нет | без родителей | Использует обычное наследование прототипов Map при запекании прототипов. Сохранение в Mapper выпускает развёрнутый результат и не сохраняет эту директиву. |
| <a id="entry-map-format-directive-map-text-0e499cda62"></a><code>$Text &lt;language&gt;</code> | ProtoMap | <code>$Text &lt;language&gt; = &lt;localized text&gt;</code> | нет | без текста карты | Добавляет локализованный текст прототипа и сохраняется циклом загрузки и сохранения Mapper как дополнительное поле ProtoMap. |
| <a id="entry-map-format-directive-entity-id-3236826b49"></a><code>$Id</code> | Critter, Item | <code>$Id = &lt;positive integer&gt;</code> | нет | следующий свободный положительный ID | Задаёт общую идентичность размещения для ссылок владения. Отсутствующие, неположительные и повторяющиеся ID исправляются при загрузке; авторские карты не должны полагаться на исправление. |
| <a id="entry-map-format-directive-entity-proto-c5b2eab9ec"></a><code>$Proto</code> | Critter, Item | <code>$Proto = &lt;PrototypeId&gt;</code> | да | отсутствует | Разрешает базовый прототип Critter или Item до применения переопределений размещения. |

## Минимальная карта

```ini
[ProtoMap]
$Name = SmallRoom
Size = 80 80
WorkHex = 40 40

[$Name/Critter]
$Id = 1
$Proto = Guard
Hex = 38 40
Dir = 3

[$Name/Item]
$Id = 2
$Proto = MetalDoor
Hex = 42 40
```

В пределах одной выбранной карты порядок размещений не имеет семантики: загрузчик обрабатывает всех криттеров, затем все предметы. Используйте явные уникальные ID для стабильных ссылок владения и понятных diff.
