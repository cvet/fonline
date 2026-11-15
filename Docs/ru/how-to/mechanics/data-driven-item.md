---
layout: default
title: Поведение предмета, выбираемое данными
locale: ru
document_id: recipe-data-driven-item
permalink: /Docs/ru/how-to/mechanics/data-driven-item.html
recipe_kind: primitive
---

# Поведение предмета, выбираемое данными
<!-- docs-translation: {"document_id":"recipe-data-driven-item","locale":"ru","source_path":"Docs/en/how-to/mechanics/data-driven-item.md","source_sha256":"PLACEHOLDER"} -->
Предмет должен быть лекарством потому, что так сказано в его прототипе, а не потому, что скрипт перечисляет идентификаторы прототипов. Этот рецепт делает вид предмета компонентом свойств прототипа, один раз задаёт состояние каждого предмета через `InitScript` прототипа, читает настроечный предел из настройки в момент использования и добавляет проверку контента и правило переименования, которые удерживают данные в порядке. Игровая политика (чего стоит лекарство, какие бывают виды) остаётся в контенте и настройках.

| Backend | Статус |
| --- | --- |
| AngelScript | `runtime-verified` |
| Managed C# | `shape` |

`runtime-verified` здесь означает, что сценарий `data-driven-item` внутри процесса на базе данных в памяти и двухфазные сценарии перезапуска `data-driven-item-restart-write` и `data-driven-item-restart-verify` на одном каталоге базы данных JSON прошли на собранной песочнице после сборки ресурсов на хосте Windows. Не проверены: компиляция и запуск Managed C#, а также миграция сохранённого документа базы данных, в котором ещё стоит старый ключ свойства (проверен только авторский ключ прототипа).

## Проверенные исходные пути

- `Examples/MechanicsSandbox/Scripts/Recipes/DataDrivenItem.fos`
- `Examples/MechanicsSandbox/Scripts/Sandbox.fos`
- `Examples/MechanicsSandbox/Scripts/Tests/Test_DataDrivenItem.fos`
- `Examples/MechanicsSandbox/Managed/Recipes/DataDrivenItem.cs`
- `Examples/MechanicsSandbox/Content/DataDrivenItem.fopro`
- `Examples/MechanicsSandbox/FOnlineMechanicsSandbox.fomain`
- `Examples/MechanicsSandbox/mechanics-smoke.json`
- `Source/Scripting/AngelScript/AngelScriptEntity.cpp`
- `Source/Scripting/CommonGlobalScriptMethods.cpp`
- `Source/Common/Properties.cpp`
- `Source/Server/EntityManager.cpp`
- `Docs/generated/api.json`

## Задача

Игра поставляет десятки расходуемых предметов. Если скрипт решает, что делает предмет, сравнивая идентификаторы прототипов, то каждому новому предмету нужно изменение кода, контент нельзя проверить без запуска игры, а дизайнер не может подстроить значение. Нужна обратная форма: контент объявляет вид и его числа, один путь в скрипте их читает, а единственный код на каждый вид — само поведение. Тоник восстанавливает немного бодрости, имеет несколько зарядов и расходуется вместе с последним; камень должен пройти тот же путь нетронутым.

## Используемые примитивы движка

| Символ | Сторона | Роль в рецепте |
| --- | --- | --- |
| `script.property.Item.InitScript` | сервер | Называет функцию, которая задаёт состояние предмета; выполняется для созданных и восстановленных предметов |
| `script.property.Item.CritterId` | общая | Доказывает, что предмет лежит в инвентаре действующего персонажа |
| `script.method.common.Game.GetProtoItem` | общая | Читает прототип для проверки контента; для неизвестного идентификатора выбрасывает исключение |
| `script.method.common.Game.CheckProtoItem` | общая | Проверяет, существует ли идентификатор прототипа, прежде чем читать его |
| `script.method.server.Game.DestroyItem` | сервер | Расходует предмет вместе с последним зарядом |
| `script.method.server.Critter.AddItem#e8b1d5e540061d17` | сервер | Создаёт тоник в инвентаре персонажа; его вызывает тест, и он запускает init script |
| `script.method.server.Game.UnloadCritter` | сервер | Убирает живого персонажа, чтобы тест воспроизвёл восстановление в одном процессе |
| `script.method.server.Game.LoadCritter#f50433829d33781c` | сервер | Восстанавливает выгруженного персонажа вместе с его инвентарём |
| `script.method.server.Entity.IsPersistent` | сервер | Проверка в тесте, что у предмета в инвентаре есть собственная запись в базе данных |

Строки `///@ Property`, `///@ Setting` и `///@ MigrationRule` — метаданные проекта, а сгенерированные средства доступа `Item.HasRemedy`, `Item.Remedy`, `ProtoItem.HasRemedy`, `Settings.DataDrivenItem.MaxVigor` и `Entity.IsDestroyed` — встроенные средства backend; ничто из этого не входит в модель API. Механизм компонентов описан в разделе [Формат прототипов: компоненты свойств](../content/prototype-format.md#компоненты-свойств), контракт init script — в разделе [Формат прототипов: init scripts](../content/prototype-format.md#init-scripts).

## Минимальный пример

В `Examples/MechanicsSandbox/`: `Scripts/Recipes/DataDrivenItem.fos` содержит объявления, init script, путь использования и проверку контента; `Content/DataDrivenItem.fopro` — четыре прототипа предметов и одного персонажа; `FOnlineMechanicsSandbox.fomain` — значение настройки; `Scripts/Tests/Test_DataDrivenItem.fos` — сценарии; `Managed/Recipes/DataDrivenItem.cs` — форму Managed C#. Команды приведены в разделе [Маршрут проверки](#маршрут-проверки).

## Пошагово

### 1. Объявите компонент, состояние экземпляра и настройку

Первая строка — маркер: `Common bool` с `Component`. Ниже идут квалифицированные строки — его поля. Маркер и поля задаются авторами, поэтому без `Mutable`; `Mutable Persistent` только у `Remedy.ChargesLeft`, состояния отдельного предмета. `Vigor` персонажа — обычное сохраняемое состояние, а предел — `Setting`, то есть конфигурация, а не состояние ([выбор представления](../../explanation/entity-and-property-model/choosing-a-representation.md#setting)):

```angelscript
///@ Property Item Common bool Remedy Component
///@ Property Item Common int32 Remedy.Restore
///@ Property Item Common int32 Remedy.Charges
///@ Property Item Common int32 Remedy.ChargesLeft Mutable Persistent PublicSync
///@ Property Critter Common int32 Vigor Mutable Persistent OwnerSync
///@ Setting Server int32 DataDrivenItem.MaxVigor
///@ MigrationRule Property Item Rename RemedyRestore Remedy.Restore
```

Настройке нужно значение в конфигурации проекта, иначе сборщик метаданных отклонит объявление:

```ini
DataDrivenItem.MaxVigor = 100
```

### 2. Опишите вид в контенте

Тоник включает маркер, задаёт значения и называет init script. Одни квалифицированные поля маркер не включают ([компоненты свойств](../content/prototype-format.md#компоненты-свойств)):

```ini
# The item kind is the Remedy component: values and the init script come from the prototype
[ProtoItem]
$Name = DataDrivenItem_Tonic
$Text engl = "Tonic"
$Text russ = "Тоник"
Remedy = True
Remedy.Restore = 30
Remedy.Charges = 3
InitScript = DataDrivenItem::InitRemedy
```

У камня компонента нет вовсе, а пустой тоник — ошибка контента, которую ловит шаг 5:

```ini
# No component: the same use path must leave it alone
[ProtoItem]
$Name = DataDrivenItem_Stone
$Text engl = "Stone"
$Text russ = "Камень"

# A marker without values: the content check must catch it, because the defaults are zero
[ProtoItem]
$Name = DataDrivenItem_Blank
$Text engl = "Blank tonic"
$Text russ = "Пустой тоник"
Remedy = True
InitScript = DataDrivenItem::InitRemedy
```

### 3. Задайте состояние экземпляра один раз

`InitScript` — сохраняемое свойство каждого предмета, поэтому названная в нём функция выполняется для созданного предмета с `firstTime == true`, а для восстановленного с `false` ([init scripts](../content/prototype-format.md#init-scripts), [callbacks `InitScript` сущности](../scripting/lifecycle-and-concurrency.md#callbacks-initscript-сущности)). Сборщик разрешает имя и проверяет сигнатуру, поэтому опечатка ломает сборку. Заполнение только при создании и сохраняет израсходованные заряды при восстановлении:

```angelscript
// The prototype names this function in its InitScript: it runs for each created item and for each restored one
void InitRemedy(Item item, bool firstTime)
{
    // Creation seeds the instance state once; a restored item already carries what the last run left in it
    if (firstTime && item.HasRemedy) {
        item.Remedy.ChargesLeft = item.Remedy.Charges;
    }
}
```

### 4. Используйте предмет через маркер

Вид — это маркер, владелец определяется по `Item.CritterId`, а предел читается из настройки в момент использования. Отказавшее использование возвращает ноль и ничего не меняет; заряд тратится только после применения эффекта:

```angelscript
// Uses an inventory item and returns the vigor it restored; zero means nothing happened
int32 UseItem(Critter cr, Item item)
{
    // The kind comes from the component marker, never from the prototype id or from a nonempty field
    if (item.CritterId != cr.Id || !item.HasRemedy) {
        return 0;
    }

    // A marker without authored values is a content mistake; the default zero must not read as a valid effect
    if (item.Remedy.Restore <= 0 || item.Remedy.ChargesLeft <= 0) {
        return 0;
    }

    int32 room = Settings.DataDrivenItem.MaxVigor - cr.Vigor;
    int32 restored = item.Remedy.Restore < room ? item.Remedy.Restore : room;
    if (restored <= 0) {
        return 0;
    }

    cr.Vigor = cr.Vigor + restored;
    item.Remedy.ChargesLeft = item.Remedy.ChargesLeft - 1;

    // A spent item leaves the world with its last charge
    if (item.Remedy.ChargesLeft == 0) {
        Game.DestroyItem(item);
    }

    return restored;
}
```

### 5. Проверяйте контент до игры

Маркер без значений везде читается как ноль, поэтому путь использования и проверка контента требуют положительных значений. Проверка опирается на `Game.CheckProtoItem`, потому что `Game.GetProtoItem` выбрасывает исключение для неизвестного идентификатора:

```angelscript
// Content check for one prototype: a kind marker needs values that make the kind usable
bool IsValidRemedyProto(hstring protoId)
{
    if (!Game.CheckProtoItem(protoId)) {
        return false;
    }

    ProtoItem proto = Game.GetProtoItem(protoId);
    return proto.HasRemedy && proto.Remedy.Restore > 0 && proto.Remedy.Charges > 0;
}
```

## Полномочия и синхронизация

- Путь использования работает только на сервере (`#if SERVER`). Он ничему не доверяет от клиента: вызывающий код разрешает персонажа и предмет на сервере, а путь заново проверяет, что предмет лежит в инвентаре этого персонажа. Удалённый вызов перед ним строится по [рецепту проверяемого запроса действия](validated-action.md).
- Вызывающий код держит cover персонажа, а он покрывает предметы, которые персонаж несёт ([захват нескольких сущностей для одного действия](cover-transactions.md)). Init script получает управление с покрытым предметом.
- `Remedy.Restore` и `Remedy.Charges` — неизменяемые значения `Common` из запечённого прототипа: обе стороны читают одно число, и во время работы его ничто не изменит. `Remedy.ChargesLeft` — единственное изменяемое поле предмета и единственное, для которого нужно решение о синхронизации; флаг синхронизации относится к каждому свойству отдельно, а компонент его не даёт.
- Настройка имеет сторону `Server`: читает её только сервер, а рецепт считает её конфигурацией только для чтения, а не состоянием.

## Сохранение и миграция

`Remedy.ChargesLeft` и `Vigor` объявлены `Mutable Persistent`, поэтому у них есть запись в базе данных, когда она есть у предмета и персонажа. Поля прототипа и настройка не сохраняются: они берутся из следующей сборки ресурсов и следующей конфигурации, поэтому изменение `Remedy.Restore` или `DataDrivenItem.MaxVigor` меняет все существующие предметы при следующем использовании. Изменение `Remedy.Charges` существующие предметы не пополняет, потому что заряды были заданы один раз. Если перебалансировка должна дойти и до них, напишите миграцию или осознанно задайте значения заново.

Поле компонента заменило плоское свойство `RemedyRestore`. Правило сохраняет возможность загружать авторский текст и сохранения, где ещё стоит старый ключ, а игровой код читает только новое имя ([Объявляйте правило для каждого переименования, удаления и переноса](../project/data-migrations.md#объявляйте-правило-для-каждого-переименования-удаления-и-переноса)):

```text
///@ MigrationRule Property Item Rename RemedyRestore Remedy.Restore
```

Авторские ключи `.fopro` разрешаются по той же таблице правил, что и сохранённые ключи (`PropertyRegistrar::FindPersistedProperty` в `Source/Common/Properties.cpp`). Песочница собирает прототип, в котором ещё стоит старый ключ, без предупреждения, и `Remedy.Restore` на нём читается как `15`:

```ini
# Authored with the flat property name that the component field replaced
[ProtoItem]
$Name = DataDrivenItem_Legacy
$Text engl = "Old tonic"
$Text russ = "Старый тоник"
Remedy = True
RemedyRestore = 15
Remedy.Charges = 1
InitScript = DataDrivenItem::InitRemedy
```

`Remedy.Restore` неизменяемо, поэтому сохранённый предмет его не хранит; правило важно для авторского текста, а для любого поля `Mutable Persistent`, переносимого в компонент, тому же правилу нужен тест перезапуска, который загружает старое сохранение ([Протестируйте миграцию](../project/data-migrations.md#протестируйте-миграцию)).

## Ловушки и антипаттерны

- Определять вид по идентификатору прототипа или по непустому полю. Проверяйте булев маркер (`HasRemedy`): дочерний прототип может выключить унаследованный компонент через `Remedy = False`, сохранив значения полей, поэтому непустое поле ничего не доказывает.
- Читать поле компонента, не проверив маркер. В AngelScript средство доступа выбрасывает исключение `Component is not present on entity (check the Has-accessor first)` (`Entity_GetComponent` в `Source/Scripting/AngelScript/AngelScriptEntity.cpp`); тест песочницы `component_read_without_the_marker_throws` читает `stone.Remedy.Restore` и ожидает исключение. Для Managed C# документирован `InvalidOperationException` при том же чтении; это не запускалось.
- Доверять маркеру без значений. У пустого тоника есть `Remedy = True` и больше ничего, поэтому каждое поле читается как `0`, и предмет выглядит корректным лекарством, которое ничего не делает. Путь использования отказывает при неположительных значениях, а `IsValidRemedyProto` — проверка, которую тест контента запускает для каждого прототипа с маркером.
- Заполнять без защиты `firstTime`. Безусловное `ChargesLeft = Charges` в init script пополняет восстановленный предмет при каждой загрузке. Тест песочницы показывает, что защищённый скрипт сохраняет `2` заряда из `3` при выгрузке и загрузке и при настоящем перезапуске; вариант без защиты не запускается.
- Один раз читать настройку при инициализации и кэшировать её в глобальной переменной скрипта. Настройка — конфигурация, поэтому читайте `Settings.DataDrivenItem.MaxVigor` там, где она используется, как это делает тест песочницы: он опускает бодрость до значения на пять ниже максимума, и восстановление ограничивается числом `5`.
- Вызывать `Game.GetProtoItem` для идентификатора, которого может не быть. Он выбрасывает исключение; ставьте перед ним `Game.CheckProtoItem`, когда отсутствие — законный ответ.

## Маршрут проверки

Сценарии `data-driven-item`, `data-driven-item-restart-write` и `data-driven-item-restart-verify` в `mechanics-smoke.json` используют подконфигурации `RecipeTests` и `RecipeRestart`, описанные в [рецепте сохраняемого состояния](persistent-state.md#маршрут-проверки), а `-Testing.Filter` у них равен `data_driven_item.`, `data_driven_item_restart_write.` и `data_driven_item_restart_verify.`. Тест восстановления внутри процесса создаёт персонажа с тоником, тратит один заряд, выгружает персонажа и загружает его обратно:

```angelscript
    Map map = Sandbox::CreateMap();
    Location location = map.GetLocation();
    Critter cr = MakePatient(map);
    Item tonic = cr.AddItem(TonicPid);
    DataDrivenItem::UseItem(cr, tonic);
    location.MakePersistent(true);

    ident crId = cr.Id;
    Game.UnloadCritter(cr);

    Critter restored = Game.LoadCritter(crId, false, map);
    Item? restoredTonic = restored.GetItem(TonicPid);
    if (restoredTonic == null) {
        Testing::Fail(ctx, "The inventory item did not come back with its critter");
        return;
    }

    Testing::Expect(ctx, restoredTonic.HasRemedy, "The restored item still carries the component");
    Testing::ExpectEq(ctx, restoredTonic.Remedy.ChargesLeft, 2, "The init script did not reseed the restored item");
    Testing::ExpectEq(ctx, restored.Vigor, 30, "The critter vigor came back too");
```

Фаза проверки — новый серверный процесс на том же каталоге JSON; он находит сохранённого персонажа через его локацию и затем ожидает `2` оставшихся заряда, авторское значение `Remedy.Restore`, равное `30`, и по-прежнему пригодный к использованию предмет.

Запустите все сценарии песочницы из `Examples/MechanicsSandbox` командой `python validate.py` на Windows (PowerShell) или `python3 validate.py` на Linux (bash); запуск успешен, когда `BuildTools/gameplay_test_runner.py` сообщает `status=passed` для каждого сценария и завершается с кодом `0`. Среди требуемых маркеров:

```text
[Testing] PASS data_driven_item.kind_comes_from_the_component
[Testing] PASS data_driven_item.legacy_flat_property_resolves_into_the_component
[Testing] PASS data_driven_item_restart_verify.charges_survive_restart
[Testing] COMPLETE status=passed
```

## Managed C#

Статус `shape`: тест движка разбирает `Managed/Recipes/DataDrivenItem.cs` и сравнивает его строки `///@` с модулем AngelScript, они совпадают; ни одна линия движка его не компилирует и не запускает. Callback инициализации и путь использования сохраняют ту же структуру; параметры сущностей несут `[RequiresCover]` ([серверная синхронизация сущностей](../scripting/managed-csharp.md#серверная-синхронизация-сущностей)):

```csharp
    // The prototype names this method in its InitScript: it runs for each created item and for each restored one
    public static void InitRemedy([RequiresCover] Item item, bool firstTime)
    {
        // Creation seeds the instance state once; a restored item already carries what the last run left in it
        if (firstTime && item.HasRemedy) {
            item.Remedy.ChargesLeft = item.Remedy.Charges;
        }
    }
```

Сгенерированные средства доступа — `item.HasRemedy` и `item.Remedy.Restore`; чтение `Remedy` при `HasRemedy`, равном false, выбрасывает `InvalidOperationException` ([компоненты свойств](../content/prototype-format.md#компоненты-свойств)). Проект на managed backend должен выполнить тот же тест перезапуска, прежде чем полагаться на эту форму.

## Как это расширяет крупный проект

- **Больше видов.** Каждый вид — ещё один компонент со своим маркером (`Weapon`, `Armor`, `Food`), а не ещё одна ветка переключателя в скрипте. У предмета может быть несколько маркеров; каждый потребитель проверяет только нужный ему.
- **Структурированные таблицы.** Если виду нужна таблица именованных строк, например определения эффектов, общие для многих предметов, вынесите её в [каталог FixedType](../../explanation/entity-and-property-model/choosing-a-representation.md#строка-каталога-fixedtype) и ссылайтесь на строку из компонента.
- **Аудит контента.** Запускайте проверку контента для каждого прототипа в тесте проекта, а не при инициализации модуля, и проваливайте сборку на маркере без значений.
- **Эффекты.** Восстановление здесь — одно число. Декларативные эффекты, временные усиления и наложение — политика проекта; временному эффекту, переживающему перезапуск, нужен [рецепт временного эффекта](timed-effect.md).
- **Что остаётся в игровом проекте.** Виды, их числа, тексты предметов и экономика вокруг них.

## См. также

- [Создание игровых механик](index.md)
- [Собственное сохраняемое состояние персонажа](persistent-state.md)
- [Проверяемый запрос действия](validated-action.md)
- [Формат прототипов](../content/prototype-format.md)
- [Выбор представления игровых данных](../../explanation/entity-and-property-model/choosing-a-representation.md)
- [Жизненный цикл и конкурентность скриптов](../scripting/lifecycle-and-concurrency.md)
- [Миграция сохранённых данных и контента в игровом проекте](../project/data-migrations.md)
