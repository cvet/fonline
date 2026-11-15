---
layout: default
title: Положить и взять между персонажем и контейнером
locale: ru
document_id: recipe-container-transfer
permalink: /Docs/ru/how-to/mechanics/container-transfer.html
recipe_kind: primitive
---

# Положить и взять между персонажем и контейнером
<!-- docs-translation: {"document_id":"recipe-container-transfer","locale":"ru","source_path":"Docs/en/how-to/mechanics/container-transfer.md","source_sha256":"PLACEHOLDER"} -->
Игрок кладёт предмет в сундук на карте или в сумку, которую несёт, и снова берёт его. Этот рецепт перемещает предметы между двумя держателями под одним cover, проверяет досягаемость сундука и принадлежность предмета, группирует содержимое по ключу секции, отправляет клиенту снимок содержимого и доказывает перезапуском, что оба держателя и секции возвращаются. Вместимость, секции и то, что считается контейнером, — политика проекта; движок хранит вложенный предмет с ключом группировки и ничего больше.

| Backend | Статус |
| --- | --- |
| AngelScript | `runtime-verified` |
| Managed C# | `shape` |

`runtime-verified` здесь означает, что сценарий `container-transfer` внутри процесса, сценарий `container-transfer-client` с настоящим клиентом без графического интерфейса, отправляющим честные и поддельные запросы, и двухфазные сценарии перезапуска `container-transfer-restart-write` и `container-transfer-restart-verify` прошли на собранной песочнице после сборки ресурсов на хосте Windows. Не проверены: компиляция и запуск Managed C#, `withInnerEntities = true` в `Critter.SendItems` и счётчики предметов в накапливаемом свойстве (накопление — политика проекта и не моделируется).

## Проверенные исходные пути

- `Examples/MechanicsSandbox/Scripts/Recipes/ContainerTransfer.fos`
- `Examples/MechanicsSandbox/Scripts/Sandbox.fos`
- `Examples/MechanicsSandbox/Scripts/Tests/Test_ContainerTransfer.fos`
- `Examples/MechanicsSandbox/Managed/Recipes/ContainerTransfer.cs`
- `Examples/MechanicsSandbox/Content/ContainerTransfer.fopro`
- `Examples/MechanicsSandbox/FOnlineMechanicsSandbox.fomain`
- `Examples/MechanicsSandbox/mechanics-smoke.json`
- `Source/Server/Item.cpp`
- `Source/Server/ItemManager.cpp`
- `Source/Scripting/ServerGlobalScriptMethods.cpp`
- `Source/Scripting/ServerItemScriptMethods.cpp`
- `Source/Server/EntitySync.cpp`
- `Source/Client/Client.cpp`
- `Docs/generated/api.json`

## Задача

В каждом переносе участвуют два держателя: инвентарь действующего персонажа и контейнер — либо сундук на карте, либо сумка в том же инвентаре. Игрок называет контейнер и предмет; оба идентификатора — лишь заявления. Сервер должен найти контейнер только там, куда персонаж дотягивается, найти предмет только там, где персонаж им владеет, применить правило вместимости и переместить предмет, покрыв все сущности, которых касается перемещение. Какие предметы являются контейнерами, сколько вмещают и что означают секции — игровая политика.

## Используемые примитивы движка

| Символ | Сторона | Роль в рецепте |
| --- | --- | --- |
| `script.method.server.Player.GetControlledCritter` | сервер | Действующий персонаж берётся у вызвавшего игрока |
| `script.method.server.Game.Sync#b42b6f6d6b684d2a` | сервер | Покрывает игрока вместе с его персонажем и карту одним запросом |
| `script.method.server.Critter.GetItem#3e4a8f273e418d3b` | сервер | Разрешает идентификатор только внутри инвентаря действующего персонажа |
| `script.method.server.Map.GetItem` | сервер | Разрешает идентификатор сундука среди предметов карты действующего персонажа |
| `script.method.server.Game.GetDistance#3dade8869ec8c0ac` | сервер | Досягаемость между действующим персонажем и сундуком |
| `script.property.Item.Ownership` | общая | Отличает сундук на карте от носимой сумки; также показывает, где предмет после перемещения |
| `script.enum-value.ItemOwnership.MapHex` | общая | Размещение сундука, стоящего на карте |
| `script.method.server.Item.GetItems` | сервер | Содержимое контейнера, при необходимости одной секции |
| `script.method.server.Game.MoveItem#2ed0d82a6f587a26` | сервер | `Game.MoveItem(item, toCont, stackId)` кладёт предмет в контейнер |
| `script.method.server.Game.MoveItem#c26f82c316789816` | сервер | `Game.MoveItem(item, toCr)` возвращает его персонажу |
| `script.property.Item.ContainerId` | общая | Родительский контейнер, пока предмет внутри него |
| `script.property.Item.ContainerStack` | общая | Ключ группировки; рецепт хранит в нём секцию |
| `script.method.server.Item.AddItem#c6fbcb98959420b1` | сервер | Тест создаёт предмет прямо внутри сундука |
| `script.method.server.Critter.SendItems` | сервер | Отправляет клиенту действующего персонажа снимок содержимого |
| `script.event.client.Game.OnReceiveItems` | клиент | Принимает снимок вместе со значением контекста |
| `script.method.server.Game.DestroyItem` | сервер | Отрицательный контроль уничтожает контейнер вместе с содержимым |

Строки `///@ Property` и `///@ RemoteCall` — метаданные проекта, а `[[ServerRemoteCall]]`, `[[ClientRemoteCall]]`, `[[Async]]`, `[[Event]]`, `[[ModuleInit]]`, `Player.ClientCall` и `Player.ServerCall` — встроенные средства backend; в модель API они не входят. Схема запроса — [рецепт проверяемого запроса действия](validated-action.md), правила cover описаны в разделе [Захват нескольких сущностей для одного действия](cover-transactions.md).

## Минимальный пример

В `Examples/MechanicsSandbox/`: `Scripts/Recipes/ContainerTransfer.fos` содержит объявления, путь переноса, вызов снимка и клиентские обработчики; `Content/ContainerTransfer.fopro` — сундук, сумку и два вида предметов; `Scripts/Tests/Test_ContainerTransfer.fos` — сценарии и серию запросов клиента; `Managed/Recipes/ContainerTransfer.cs` — форму Managed C#. Команды приведены в разделе [Маршрут проверки](#маршрут-проверки).

## Пошагово

### 1. Объявите данные политики и вызовы

`ContainerCapacity` делает предмет контейнером, а `StorageSection` — ключ группировки, который несёт предмет. Оба свойства задаются авторами; у движка нет флага контейнера, а `Item::AddItemToContainer` проверяет лишь то, что перемещение не создаёт цикл. Удалённые вызовы называют контейнер и предмет, а `put` выбирает направление:

```angelscript
///@ Property Item Common int32 ContainerCapacity
///@ Property Item Common int32 StorageSection

///@ RemoteCall Server TransferItem(ident containerId, ident itemId, bool put) MaxBytes 17
///@ RemoteCall Server OpenContainer(ident containerId) MaxBytes 8
///@ RemoteCall Client ContainerResult(ident containerId, bool accepted) MaxBytes 9
```

Сундук и сумка различаются только данными:

```ini
# A chest that stands on the map and holds three items, and a bag that holds two and travels with its owner
[ProtoItem]
$Name = ContainerTransfer_Chest
$Text engl = "Storage chest"
$Text russ = "Сундук"
ContainerCapacity = 3
```

### 2. Покройте обоих держателей одним запросом

Обработчик определяет действующего персонажа и его карту, затем запрашивает игрока и карту одним `Game.Sync`. Покрытие игрока покрывает его персонажа и всё, что тот несёт, а покрытие карты — её предметы, сундук и предметы внутри него. После захвата действующий персонаж и его карта определяются заново, потому что тем временем могли выполниться другие задания:

```angelscript
[[ServerRemoteCall]] [[Async]]
void TransferItem(Player player, ident containerId, ident itemId, bool put)
{
    // The engine supplies the player; the actor and its map are derived, and one cover holds both holders
    Critter? actor = player.GetControlledCritter();
    Map? map = actor != null ? actor.GetMap() : null;
    if (actor == null || map == null) {
        Refuse(player, containerId, "no_actor");
        return;
    }

    Game.Sync(player, map);

    actor = player.GetControlledCritter();
    if (actor == null || actor.MapId != map.Id) {
        Refuse(player, containerId, "actor_moved");
        return;
    }

    string reason = TryTransfer(actor, map, containerId, itemId, put);
    if (!reason.isEmpty()) {
        Refuse(player, containerId, reason);
        return;
    }

    player.ClientCall.ContainerResult(containerId, true);
}
```

### 3. Разрешите контейнер там, куда персонаж дотягивается

Носимая сумка находится в инвентаре и досягаемости не требует; сундук на карте должен быть в пределах одного гекса. Всё остальное, включая предмет без вместимости и нулевой идентификатор, ни во что не разрешается:

```angelscript
// A chest stands on the map and must be near; a bag in the actor's own inventory is always in reach
Item? FindReachableContainer(Critter actor, Map map, ident containerId)
{
    Item? container = actor.GetItem(containerId);
    if (container == null) {
        container = map.GetItem(containerId);
    }

    if (container == null || container.ContainerCapacity <= 0) {
        return null;
    }

    if (container.Ownership == ItemOwnership::MapHex && Game.GetDistance(actor, container) > ReachDistance) {
        return null;
    }

    return container;
}
```

### 4. Положить и взять

Положить: предмет разрешается только в инвентаре действующего персонажа, применяются правила проекта, а секция передаётся как идентификатор стопки. Взять: предмет ищется через покрытый контейнер, а не глобальным поиском по идентификатору:

```angelscript
string TryPut(Critter actor, Item container, ident itemId)
{
    // Only the actor's own inventory resolves, so a foreign or invented id is not an item to move
    Item? item = actor.GetItem(itemId);
    if (item == null) {
        return "not_carried";
    }

    // Policy: containers do not nest, which also keeps a container out of its own subtree
    if (item.Id == container.Id || item.ContainerCapacity > 0) {
        return "no_nesting";
    }

    if (container.GetItems().length() >= container.ContainerCapacity) {
        return "full";
    }

    // The section is the grouping key; the engine merges nothing, so equal sections stay separate items
    Item? moved = Game.MoveItem(item, container, any(item.StorageSection));
    return moved != null ? "" : "move_failed";
}

string TryTake(Critter actor, Item container, ident itemId)
{
    // The item is found through the covered container; a global id lookup would hand back an uncovered entity
    Item[] contents = container.GetItems();

    for (int i = 0; i < contents.length(); i++) {
        if (contents[i].Id == itemId) {
            Item? moved = Game.MoveItem(contents[i], actor);
            return moved != null ? "" : "move_failed";
        }
    }

    return "not_inside";
}
```

### 5. Отправьте снимок, а не догадку

`OpenContainer` повторяет шаги 2 и 3, затем отправляет содержимое со значением контекста, называющим контейнер (`actor.SendItems(container.GetItems(), false, false, any(containerId.value))`, `ContainerTransfer.fos`, строки 70-71). Клиент получает отсоединённые представления предметов и то же значение контекста:

```angelscript
// Received items are detached views: read them now, they are not part of the client's inventory
[[Event]]
void OnReceiveItems(Item[] items, any contextParam)
{
    int64 containerId = contextParam;
    Game.Log("container_snapshot container=" + containerId + " count=" + items.length());
}
```

## Полномочия и синхронизация

- Решает сервер. Действующий персонаж — `Player.GetControlledCritter()`, карта — карта действующего персонажа, а контейнер и предмет разрешаются только внутри этих двух держателей. Поддельный идентификатор из инвентаря другого персонажа или из далёкого сундука ни во что не разрешается и получает отказ.
- `Game.MoveItem` проверяет cover для предмета, для держателя, который он покидает, и для места назначения (`Server_Game_MoveItem` в `Source/Scripting/ServerGlobalScriptMethods.cpp`). `Game.Sync(player, map)` обеспечивает все три в обоих направлениях: удержание сущности покрывает каждую сущность под ней, поэтому сумка в инвентаре, сундук и каждый вложенный предмет покрыты ([модель cover](cover-transactions.md#модель-cover-на-практике)).
- Обработчики несут `[[Async]]`, потому что его несёт `Game.Sync`; после захвата ни один шаг не приостанавливается, поэтому проверки и перемещение выполняются под одним cover.

## Сохранение и миграция

Предметы внутри сохраняемого контейнера получают запись в базе данных, когда попадают в него (`Item::AddItemToContainer` в `Source/Server/Item.cpp`), а инвентарь персонажа хранится вместе с персонажем. Пара перезапуска кладёт предметы в сундук на карте и оставляет один у персонажа, затем новый серверный процесс находит сундук через его локацию: сундук по-прежнему хранит оба предмета, каждый в своей секции, персонаж по-прежнему несёт свой предмет, а взятие затем работает на восстановленной паре. Персонаж, выгруженный и загруженный в одном процессе, тоже сохраняет сумку с содержимым.

`Item.ContainerStack` сохраняется вместе с предметом, поэтому номера секций — сохраняемые данные. Перенумерация секции — миграция данных, а переименованным `StorageSection` или `ContainerCapacity` нужно правило `Property Rename` ([Объявляйте правило для каждого переименования, удаления и переноса](../project/data-migrations.md#объявляйте-правило-для-каждого-переименования-удаления-и-переноса)).

## Ловушки и антипаттерны

- Считать идентификатор стопки накоплением. Описание `Item.ContainerStack` говорит, что ключ служит для выбора и слияния предметов, но никакой код движка ничего не сливает: два предмета с одинаковым прототипом и одинаковым ключом остаются двумя предметами, а `Item.GetItems` с ключом только фильтрует по нему. Песочница добавляет две руды и траву, находит три предмета, находит две в секции руды, а любое количество считает в данных проекта.
- Перемещать контейнер в самого себя. `Game.MoveItem(bag, bag)` выбрасывает исключение, но оставляет сумку с `Ownership == Nowhere` (значение `3`), без держателя и не уничтоженной: `ItemManager::MoveItem` отсоединяет источник раньше, чем `Item::AddItemToContainer` отклоняет цикл. Песочница печатает `container_transfer_self_move_state ownership=3 destroyed=false`. Правило рецепта о запрете вложенности не пускает контейнер ни в один контейнер, поэтому этот путь не достигается; проект, допускающий вложенность, должен проверять цикл до вызова.
- Разрешать контейнер или предмет глобальным поиском по идентификатору. Он может вернуть сущность вне cover и такую, на которую у персонажа нет прав. Разрешайте через `Critter.GetItem`, `Map.GetItem` и `Item.GetItems` покрытого контейнера.
- Уничтожать контейнер, в котором ещё лежат предметы. Содержимое принадлежит контейнеру и уходит вместе с ним; это показывает отрицательный контроль `destroying_the_container_destroys_its_contents`. Если предметы должны уцелеть, сначала заберите или переместите их.
- Разделять проверку вместимости и перемещение сменой cover. Обе операции должны идти под одним cover из шага 2; `Game.Sync` между ними позволяет другому заданию сначала заполнить сундук.
- Доверять снимку после изменений. Предметы на клиенте — копия одного момента; после любого изменения запросите снимок снова через `OpenContainer`.

## Маршрут проверки

Сценарий `container-transfer` запускает `-Testing.Filter container_transfer.` на `RecipeTests`. Его контроль накопления, `sections_group_items_and_nothing_is_merged`, кладёт в сундук две руды и траву и ожидает три предмета. Ловушка движка — тоже тест: перемещение выбрасывает исключение, а состояние сумки записывается в журнал:

```angelscript
    bool threw = false;

    try {
        Game.MoveItem(bag, bag);
    }
    catch {
        threw = true;
    }

    Testing::Expect(ctx, threw, "The engine refuses a container inside itself");
    Game.Log("container_transfer_self_move_state ownership=" + int(bag.Ownership) + " destroyed=" + (bag.IsDestroyed ? "true" : "false"));
```

Сценарий `container-transfer-client` запускает сервер с `RecipeNetworkTests` и `-Testing.Filter container_transfer_client.`, ждёт `container_transfer_waiting_for_client` и запускает клиент без графического интерфейса с `RecipeClient`, как в [маршруте проверки проверяемого запроса действия](validated-action.md#маршрут-проверки). Клиент открывает сундук, кладёт траву, пробует предмет другого персонажа, пробует далёкий сундук, берёт сохранённую руду и отправляет `ContainerDrillSent`; затем сервер проверяет, что траву хранит только ближний сундук, а далёкий пуст. Пара перезапуска использует `RecipeRestart`, как в [маршруте проверки сохраняемого состояния](persistent-state.md#маршрут-проверки). Запустите все сценарии песочницы из `Examples/MechanicsSandbox` командой `python validate.py` на Windows (PowerShell) или `python3 validate.py` на Linux (bash); запуск успешен, когда `BuildTools/gameplay_test_runner.py` сообщает `status=passed` для каждого сценария и завершается с кодом `0`. Среди требуемых маркеров:

```text
container_transfer_refused reason=not_carried
container_transfer_refused reason=unknown_container
container_snapshot container=
[Testing] PASS container_transfer_client.client_requests_are_validated_and_snapshots_arrive
[Testing] PASS container_transfer_restart_verify.both_holders_survive_restart
```

## Managed C#

Статус `shape`: тест движка разбирает `Managed/Recipes/ContainerTransfer.cs` и сравнивает его строки `///@` с модулем AngelScript, они совпадают; ни одна линия движка его не компилирует и не запускает. Обработчик — `async Task` и `internal`, а `Sync.Lock` заменяет `Game.Sync` ([анализаторы контрактов движка](../project/engine-contract-analyzers.md), [серверная синхронизация сущностей](../scripting/managed-csharp.md#серверная-синхронизация-сущностей)):

```csharp
    [ServerRemoteCall]
    internal static async Task TransferItem([RequiresCover] Player player, ident containerId, ident itemId, bool put)
    {
        // The engine supplies the player; the actor and its map are derived, and one cover holds both holders
        Critter? actor = player.GetControlledCritter();
        Map? map = actor?.GetMap();
        if (actor == null || map == null) {
            Refuse(player, containerId, "no_actor");
            return;
        }

        if (!await Sync.Lock(player, map)) {
            return;
        }
```

Проект на managed backend должен выполнить ту же серию запросов клиента и пару перезапуска, прежде чем полагаться на эту форму.

## Как это расширяет крупный проект

- **Накопление и счётчики.** Храните количество в изменяемом свойстве предмета и сливайте в коде проекта под тем же cover; движок этого не сделает.
- **Вложенные контейнеры.** Допускайте их только с явной проверкой цикла перед каждым перемещением и решите, засчитывается ли содержимое во вместимость родителя.
- **Общие и запертые контейнеры.** Владение, ключи и право открывать сундук — данные политики на предмете; правило досягаемости рецепта — то место, куда их добавить.
- **Что остаётся в игровом проекте.** Что такое контейнер, вместимость, секции, стоимость и время переноса и тексты каждого отказа.

## См. также

- [Создание игровых механик](index.md)
- [Захват нескольких сущностей для одного действия](cover-transactions.md)
- [Проверяемый запрос действия](validated-action.md)
- [Собственное сохраняемое состояние персонажа](persistent-state.md)
- [Удалённые вызовы](../../reference/scripting/remote-calls.md)
- [Жизненный цикл и конкурентность скриптов](../scripting/lifecycle-and-concurrency.md)
- [Скрипты Managed C#](../scripting/managed-csharp.md)
- [Миграция сохранённых данных и контента в игровом проекте](../project/data-migrations.md)
