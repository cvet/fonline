---
layout: default
title: Карта методов скриптового API
locale: ru
document_id: script-methods-map
permalink: /Docs/ru/reference/script-api/method-ownership.html
---

# Карта методов скриптового API

<!-- docs-translation: {"document_id":"script-methods-map","locale":"ru","source_path":"Docs/en/reference/script-api/method-ownership.md","source_sha256":"b6c90ffeebeb1098a6f9e52e719a81735379319a7ca54be7921714d20b5d19ec"} -->

> Документация движка. Эта страница сопоставляет нативные файлы `///@ ExportMethod` в `Source/Scripting/` с их обязанностями в скриптовом интерфейсе. Она дополняет страницу [Скриптовый runtime](../../explanation/scripting-runtime/), но не является полным сгенерированным справочником API.

## Назначение

Используйте эту страницу при добавлении, переносе или review нативных методов, видимых скриптам. До изменения входов codegen должно быть понятно, кому принадлежит метод:

- выберите правильную сторону runtime (`Common`, `Server`, `Client` или `Mapper`);
- выберите правильное семейство receiver или сущности (`Game`, `Entity`, `Critter`, `Map`, `Item`, `Location`, `Player`, `ImGui`);
- сохраните границы серверной авторитетности и клиентского представления;
- обновите тесты и документацию при переносе групп экспортируемых методов.

## Инвентарь исходного кода

Авторитетный список файлов и число объявлений `///@ ExportMethod` в каждом из `Source/Scripting/*ScriptMethods.cpp` генерируются в [source-inventory.json](../../../generated/source-inventory.json). Полные разобранные записи методов находятся в [канонической модели API](../../../generated/api.json), а [сгенерированный справочник методов](../../../generated/api/methods.md) показывает ID перегрузок, сигнатуры, значения по умолчанию, nullable-признаки, стороны runtime, фактические receiver, стабильность и исходные позиции. Эта страница отвечает за понятное человеку объяснение семейств методов и границ стороны/receiver; она не дублирует сгенерированные итоги и сигнатуры.

После добавления, удаления или перемещения export обновите инвентарь:

```bash
python BuildTools/docs_api.py --write
python BuildTools/docs_api.py --check
python BuildTools/docs_reference.py --write
python BuildTools/docs_reference.py --check
python BuildTools/docs_inventory.py --write
python BuildTools/docs_inventory.py --check
```

## Соглашения об именовании и владении

Экспортируемые функции являются C++-функциями с `FO_SCRIPT_API`; скриптовое имя выводится из префикса стороны и типа. Распространённые формы:

- `Common_Game_*` — нейтральная к стороне глобальная утилита, доступная через game/global object.
- `Common_ImGui_*` — нейтральные к стороне обёртки ImGui.
- `Server_Game_*`, `Server_Map_*`, `Server_Critter_*` и другие — авторитетные серверные методы.
- `Client_Game_*`, `Client_Map_*`, `Client_Critter_*` и другие — методы клиента, представления и frontend.
- `Mapper_Game_*` — автоматизация Mapper и редактора.

Префикс является частью контракта владения. Не переносите метод в более удобный файл, если это меняет владельца состояния, которое он изменяет.

## Общие методы

### `Source/Scripting/CommonGlobalScriptMethods.cpp`

- Префикс: `Common_Game_*`
- Владение: межсторонние глобальные helpers, которым не требуется исключительно серверное авторитетное состояние или исключительно клиентское состояние rendering.
- Типичные обязанности:
  - журналирование и helpers остановки в отладчике;
  - quit/invoke helpers;
  - чтение ресурсов и конфигурации, а также типизированный facade длительности поверх полных метаданных анимации модели от baker (`Game.GetModelAnimDuration`);
  - случайные значения, время, UTF-8, буфер обмена и открытие ссылок;
  - геометрические helpers для расстояния, направления, угла линии, интервалов и трассировки;
  - общая сериализация и форматирование.
- Тесты: `Source/Tests/Test_CommonScriptMethods.cpp`, `Source/Tests/Test_ScriptBuiltins.cpp`.
- Контракты tuple анимации модели, aliases, bake, общего lookup и клиентского экземпляра: [Model Animation](../../how-to/content/model-animation.md).

### `Source/Scripting/CommonImGuiScriptMethods.cpp`

- Префиксы: `Common_Game_ImGui`, `Common_ImGui_*`
- Владение: видимые скриптам обёртки ImGui, общие для инструментов и frontend, которые предоставляют ImGui.
- Типичные обязанности:
  - начало и завершение окна, стек стилей;
  - layout, текст, widgets, popup, меню, таблицы и дочерние окна;
  - редакторы значений и controls;
  - обёртки draw-list и UI helpers, где они реализованы.
- Семантику UI-обёрток держите здесь; политика конкретного runtime относится к клиентским, mapper- или проектным скриптам.

## Серверные методы

### `Source/Scripting/ServerGlobalScriptMethods.cpp`

- Префикс: `Server_Game_*`
- Владение: авторитетные глобальные операции игры и сервера.
- Типичные обязанности:
  - создание, загрузка, выгрузка и уничтожение криттеров и сущностей;
  - получение, перемещение и уничтожение предметов и коллекций предметов;
  - создание локаций и карт и запрос мировых сущностей;
  - серверные глобальные операции, требующие владения `ServerEngine`.
- Связанные страницы: [Серверный runtime](../../explanation/runtime/server.md), [Модель сущностей](../../explanation/entity-and-property-model/), [Персистентность](../../explanation/persistence/).
- Тесты: `Source/Tests/Test_ServerScriptMethods.cpp` и серверные runtime-тесты.

### `Source/Scripting/ServerEntityScriptMethods.cpp`

- Префикс: `Server_Entity_*`
- Владение: серверные операции базовой сущности.
- Типичные обязанности:
  - переключение персистентности через `IsPersistent` и `MakePersistent`;
  - запуск, подсчёт, остановка, повтор и изменение данных time event сущности.
- Эти операции являются серверными, потому что персистентность и авторитетное планирование сущностей принадлежат серверному runtime.

### `Source/Scripting/ServerCritterScriptMethods.cpp`

- Префикс: `Server_Critter_*`
- Владение: авторитетные операции криттера.
- Типичные обязанности:
  - настройка скрипта и init callbacks;
  - состояние движения, UID движения, перенос между картой и глобальной картой;
  - отношения игрока и управления;
  - helpers состояний alive, knockout и dead;
  - видимость, направление, операции инвентаря и обновление представления.
- Связанные страницы: [Серверный runtime](../../explanation/runtime/server.md), [Карты и движение](../../explanation/maps-and-movement.md).

### `Source/Scripting/ServerMapScriptMethods.cpp`

- Префикс: `Server_Map_*`
- Владение: авторитетные операции карты.
- Типичные обязанности:
  - настройка скрипта и получение локации;
  - создание и поиск предметов по id, гексу, радиусу или коллекции;
  - поиск статических предметов;
  - одностороннее удаление статического предмета на экземпляре карты (`RemoveStaticItem`; удалённые id читаются
    обратно через свойство `RemovedStaticItemIds`) — см.
    [Карты, движение и геометрия](../../explanation/maps-and-movement.md#удаление-статических-предметов);
  - поиск криттеров по id, гексу, радиусу, пути и условиям видимости;
  - запросы геометрии карты, пути и движения.
- Изменяющие мир операции должны оставаться здесь, а не в общих или клиентских helpers.

### `Source/Scripting/ServerItemScriptMethods.cpp`

- Префикс: `Server_Item_*`
- Владение: авторитетные операции предмета.
- Типичные обязанности:
  - настройка скрипта предмета;
  - добавление и запрос вложенных предметов;
  - разрешение позиции предмета на карте или владеющего криттера;
  - обновление видимости предмета.

### `Source/Scripting/ServerLocationScriptMethods.cpp`

- Префикс: `Server_Location_*`
- Владение: авторитетные операции локации.
- Типичные обязанности:
  - настройка скрипта локации;
  - добавление и запрос карт по pid, индексу или id;
  - возврат коллекций карт;
  - регенерация локаций.

### `Source/Scripting/ServerPlayerScriptMethods.cpp`

- Префикс: `Server_Player_*`
- Владение: операции подключённого игрока и сессии.
- Типичные обязанности:
  - получение host и port;
  - отключение и именование;
  - переключение управляемого криттера;
  - запрос управляемого криттера;
  - просмотр, reset и unload карты.
- Поток игрока и соединения описан в [серверном runtime](../../explanation/runtime/server.md).

## Клиентские методы

### `Source/Scripting/ClientGlobalScriptMethods.cpp`

- Префикс: `Client_Game_*`
- Владение: глобальные helpers клиентского runtime и frontend.
- Типичные обязанности:
  - текущие карта, локация и игрок, состояние мыши, gamepad и окна;
  - fullscreen, minimization и состояние соединения;
  - helpers расстояния и запрос видимых сущностей;
  - atlas, resource и debug helpers;
  - helpers разрешения, minimap и rendering, включая общий для анимации `DrawRect` и стабильные логические границы `ViewRect` экземпляра `DrawCritter3d`;
  - выбор effect и одиночная либо диапазонная запись буфера script value;
  - звук, музыка, видео, sprite и связанные с UI helpers, где они экспортированы.
- Связанные страницы: [Клиентский runtime](../../explanation/runtime/client.md), [Frontend и рендеринг](../../explanation/rendering/).

### `Source/Scripting/ClientEntityScriptMethods.cpp`

- Префикс: `Client_Entity_*`
- Владение: клиентские helpers time event базовой сущности.
- Типичные обязанности:
  - запуск, подсчёт, остановка и повтор time event;
  - установка данных time event.
- Повторяет часть серверного utility-интерфейса сущности, но работает с принадлежащими клиенту сущностями представления.

### `Source/Scripting/ClientCritterScriptMethods.cpp`

- Префикс: `Client_Critter_*`
- Владение: клиентские операции видимого криттера и его представления.
- Типичные обязанности:
  - отображаемое имя, online/alive/movement/model/visibility state;
  - наличие, запуск, остановка и обновление анимации и длительность пары `(state, action)` для загруженной модели (`Critter.GetModelAnimDuration`);
  - запросы инвентаря видимых клиентских криттеров;
  - позиция текста, particles, animation callbacks, позиции костей;
  - локальные helpers движения.
- Не добавляйте сюда авторитетную политику инвентаря или переноса: она принадлежит серверным методам.

### `Source/Scripting/ClientMapScriptMethods.cpp`

- Префикс: `Client_Map_*`
- Владение: клиентские операции карты, представления и rendering.
- Типичные обязанности:
  - рисование map sprites и entity sprites;
  - перестроение тумана и цветов дня;
  - состояние экрана и прокрутки;
  - поиск видимых предметов и криттеров по id, гексу, радиусу, пути и коллекциям;
  - запросы path и line tracing;
  - преобразование координат карты и экрана.
- Связанные страницы: [Клиентский runtime](../../explanation/runtime/client.md), [Карты и движение](../../explanation/maps-and-movement.md), [Frontend и рендеринг](../../explanation/rendering/).

### `Source/Scripting/ClientItemScriptMethods.cpp`

- Префикс: `Client_Item_*`
- Владение: клиентские операции видимого предмета и представления.
- Типичные обязанности:
  - helpers видимости и clone;
  - позиция на карте и состояние движения;
  - воспроизведение, время и направление анимации;
  - запросы вложенных предметов;
  - helpers alpha и finish.

### `Source/Scripting/ClientImGuiScriptMethods.cpp`

- Префикс: `Client_ImGui_*`
- Владение: клиентские helpers ImGui для image и image-button.
- Типичные обязанности:
  - image widgets на основе texture или sprite ресурсов клиента и frontend.
- Общие обёртки ImGui находятся в `CommonImGuiScriptMethods.cpp`.

### `Source/Scripting/ClientLocationScriptMethods.cpp`

- Владение: зарезервированная пустая группа клиентских методов локации.
- Сохраняйте файл как маркер маршрутизации, пока не появятся настоящие клиентские методы локации.

### `Source/Scripting/ClientPlayerScriptMethods.cpp`

- Владение: зарезервированная пустая группа клиентских методов игрока.
- Авторитетность игрока и сессии остаётся на сервере; добавляйте сюда только действительно клиентское поведение представления игрока.

## Методы Mapper

### `Source/Scripting/MapperGlobalScriptMethods.cpp`

- Префикс: `Mapper_Game_*`
- Владение: автоматизация Mapper и редактора.
- Типичные обязанности:
  - создание пустой карты (`NewMap` / `NewMapFromText`, оборачивающие внутренний `LoadMapFromText`);
  - добавление, удаление, перемещение и выбор сущностей;
  - установка любого instance property сущности по имени и тексту (`SetEntityProperty` через путь применения inspector);
  - добавление tiles;
  - загрузка, выгрузка, сохранение и показ карт, включая sandboxed сохранение в подкаталог (`SaveMapToPath`);
  - синхронное сохранение только render target карты через `SaveMapperScreenshot`; окна ImGui уровня приложения не входят в этот script capture;
  - запрос файлов загруженных карт;
  - изменение размера карт;
  - управление вкладками Mapper и pid-фильтрами вкладок.
- Связанная документация: [инструменты Mapper](../../how-to/tools/mapper.md).

## Добавление или перенос экспортируемых методов

Перед изменением файла `*ScriptMethods.cpp` пройдите этот список:

1. Определите сторону, владеющую состоянием: общая утилита, серверная авторитетность, клиентское представление/frontend или редактор Mapper.
2. Определите семейство receiver: global/game, entity, critter, map, item, location, player, ImGui или другой зарегистрированный тип.
3. Добавьте `///@ ExportMethod` и `FO_SCRIPT_API` в принадлежащий владельцу файл. Для необязательных завершающих аргументов используйте C++-параметры по умолчанию вместо дублирующих перегрузок, тела которых только подставляют fallback. Codegen нормализует значения типов движка вроде `isize32 {}` или `ucolor {}` в AngelScript-выражения `isize()` или `ucolor()`.
4. Обозначайте scalar pointer параметр или результат как `nptr<T>` только если он действительно принимает или возвращает null; иначе используйте non-null `ptr<T>`. См. [Nullability.md](../../../Nullability.md).
5. Перегенерируйте код, чтобы descriptors методов и wrappers отражали новую сигнатуру.
6. Добавьте или обновите самый узкий подходящий тест script method.
7. Проверяйте или добавляйте `///@ ApiContract` только когда статус поддержки метода имеет подтверждённое владельцем решение; достижимость не является обещанием стабильности.
8. Перегенерируйте `Docs/generated/api.json`, страницы сгенерированного справочника и `Docs/generated/source-inventory.json`; изменяйте эту страницу только при содержательном изменении файла или группы обязанностей.

## Контрольный список проверки

1. После изменения API или контракта выполните `python BuildTools/tests/test_docs_api.py`, `python BuildTools/docs_api.py --write`, `python BuildTools/docs_reference.py --write`, а затем обе команды с `--check`.
2. После изменения export-файлов выполните `python BuildTools/docs_inventory.py --write`, затем `--check`.
3. Выполните генерацию кода и скомпилируйте созданные wrappers.
4. Запустите подходящие тесты методов:
   - общие методы: `Source/Tests/Test_CommonScriptMethods.cpp`;
   - серверные методы: `Source/Tests/Test_ServerScriptMethods.cpp` и соответствующие server runtime tests;
   - поведение entity и handles: `Source/Tests/Test_ScriptEntityOps.cpp`;
   - builtins и types: `Source/Tests/Test_ScriptBuiltins.cpp`.
5. Запустите nullable analyzers, если изменилась pointer-сигнатура.
6. Проверьте runtime-поведение на стороне-владельце; одна компиляция не доказывает правильность размещения метода.
