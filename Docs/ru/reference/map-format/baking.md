---
title: Запекание и runtime-загрузка карт
document_id: generated-map-format-baking
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-map-format-baking","locale":"ru","source_path":"Docs/en/reference/map-format/baking.md","source_sha256":"0f8bc7e92eca412554edd9ffe6de07b234b9461ca89087b29bd1aa8dfece192c"} -->

# Запекание и runtime-загрузка карт

> Сгенерированный справочник. Не редактируйте напрямую. Обновите `BuildTools/MapFormatInterface.json` или владеющие метаданные движка, затем запустите `python BuildTools/docs_map_format.py --write`.

[Индекс](index.md) | [Синтаксис](syntax.md) | [Свойства](properties.md) | [Запекание](baking.md) | [Валидация](validation.md) | [Каноническая JSON-модель](../../../generated/map-format.json) | [Руководство по авторингу](../../how-to/content/map-format.md)

Контейнеры карт выбираются по `Baking.ProtoFileExtensions plus at least one [ProtoMap] anchor`; `.fomap` является соглашением проекта, а не требованием движка. Каждая объявленная карта выпускает связанную пару ресурсов `<MapId>.fomap-bin-server` и `<MapId>.fomap-bin-client`.

Серверные данные содержат размещённых криттеров и все предметы. Клиентские данные содержат видимые статические предметы; скрытые статические предметы добавляют необходимые хеши строк, но не запись клиентского предмета.

## Владение предметом

| Владение | Значение | Поддерживается картой | Ссылка/позиция | Значение | Источник enum |
| --- | --- | --- | --- | --- | --- |
| <a id="entry-map-format-ownership-map-hex-2dae405d94"></a><code>MapHex</code> | 0 | да | <code>Hex</code> | Размещает предмет на гексе карты. Статические предметы обязаны использовать этот режим. | [Source/Common/Common.h](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L1078) |
| <a id="entry-map-format-ownership-critter-inventory-08f10bd139"></a><code>CritterInventory</code> | 1 | да | <code>CritterId</code> | Создаёт нестатический предмет в инвентаре размещённого криттера, заданного CritterId. | [Source/Common/Common.h](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L1079) |
| <a id="entry-map-format-ownership-item-container-c580fe1c90"></a><code>ItemContainer</code> | 2 | да | <code>ContainerId</code> | Создаёт нестатический предмет внутри размещённого нестатического предмета, заданного ContainerId. | [Source/Common/Common.h](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L1080) |
| <a id="entry-map-format-ownership-nowhere-a9c5a87d13"></a><code>Nowhere</code> | 3 | нет | <code>none</code> | Присутствует в enum, но не поддерживается как режим авторского размещения на карте. | [Source/Common/Common.h](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L1081) |

## Разделение runtime-данных

- Статические предметы `MapHex` становятся неизменяемыми записями сетки и могут блокировать движение или стрельбу, предоставлять триггеры и занимать multihex-ячейки.
- Нестатические предметы `MapHex` и размещённые криттеры создаются для каждого экземпляра карты; их авторские ID переназначаются в runtime-ID.
- Записи `CritterInventory` и `ItemContainer` присоединяются после создания их непосредственных владельцев. При отсутствии сопоставления владельца дочерний предмет пропускается.
- Клиент восстанавливает из клиентского бинарного файла только видимый статический слой карты; динамические сущности поступают через обычную runtime-синхронизацию.
