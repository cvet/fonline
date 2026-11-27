---
layout: default
title: Экипировка в слот по одному общему правилу
locale: ru
document_id: recipe-equip-slot
permalink: /Docs/ru/how-to/mechanics/equip-slot.html
recipe_kind: primitive
---

# Экипировка в слот по одному общему правилу
<!-- docs-translation: {"document_id":"recipe-equip-slot","locale":"ru","source_path":"Docs/en/how-to/mechanics/equip-slot.md","source_sha256":"cd38d8fee76fd1c21f9ce0a8e472b01b7fb96d07fed04044743c699e7e0f22d0"} -->
Игрок перетаскивает броню на слот брони. Интерфейс должен сразу знать, разрешено ли это, а сервер должен решить по-настоящему. Этот рецепт пишет правило один раз, компилирует его для обеих сторон, применяет в единственном серверном обработчике запроса и исправляет любое перемещение, обошедшее этот обработчик. Он же показывает факт движка, определяющий весь замысел: события, которое могло бы наложить вето на перемещение предмета до того, как оно произойдёт, нет.

| Backend | Статус |
| --- | --- |
| AngelScript | `runtime-verified` |
| Managed C# | `shape` |

`runtime-verified` здесь означает, что сценарий `equip-slot` внутри процесса и сценарий `equip-slot-client`, где настоящий клиент без графического интерфейса, подключённый к серверу, отправляет честные и поддельные запросы, прошли на собранной песочнице после сборки ресурсов на хосте Windows. Не проверены: компиляция и запуск Managed C#, клиентский интерфейс, который предсказывает перемещение локально, слоты за третьим и сохранение слота при перезапуске.

## Проверенные исходные пути

- `Examples/MechanicsSandbox/Scripts/Recipes/EquipSlot.fos`
- `Examples/MechanicsSandbox/Scripts/Sandbox.fos`
- `Examples/MechanicsSandbox/Scripts/Tests/Test_EquipSlot.fos`
- `Examples/MechanicsSandbox/Managed/Recipes/EquipSlot.cs`
- `Examples/MechanicsSandbox/Content/EquipSlot.fopro`
- `Examples/MechanicsSandbox/FOnlineMechanicsSandbox.fomain`
- `Examples/MechanicsSandbox/mechanics-smoke.json`
- `Source/Scripting/ServerCritterScriptMethods.cpp`
- `Source/Server/CritterManager.cpp`
- `Source/Server/Item.cpp`
- `Source/Common/Settings.inc`
- `Source/Common/Common.h`
- `Docs/generated/api.json`

## Задача

Броня подходит к слоту брони, меч — к основному слоту, камень не подходит ни к одному слоту экипировки, и никто не вправе перемещать чужой предмет. Клиент хочет подсветить неверное место для броска без обращения к серверу, а сервер не должен верить, что клиент это сделал. Какие бывают слоты, что куда подходит и как экипировка влияет на характеристики — игровая политика. Движок фиксирует меньшее: три встроенных значения слота, обмен предметов в слоте для одного предмета и событие после фиксации.

## Используемые примитивы движка

| Символ | Сторона | Роль в рецепте |
| --- | --- | --- |
| `script.enum.CritterItemSlot` | общая | Тип слота; рецепт расширяет его одним значением, объявленным в скрипте |
| `script.enum-value.CritterItemSlot.Inventory` | общая | Неэкипированный инвентарь, значение `0`; всегда разрешённое назначение |
| `script.enum-value.CritterItemSlot.Main` | общая | Встроенный слот экипировки, значение `1` |
| `script.property.Item.CritterSlot` | общая | Слот, который занимает предмет инвентаря; аудит читает его после перемещения |
| `script.property.Item.CritterId` | общая | Владеющий персонаж; правило отказывает для предмета, который не принадлежит действующему персонажу |
| `script.method.server.Critter.ChangeItemSlot` | сервер | Переносит предмет инвентаря в разрешённый слот и меняет местами предмет, занимавший слот для одного предмета |
| `script.event.server.Game.OnCritterItemMoved` | сервер | Вызывается после того, как предмет вошёл в инвентарь, вышел из него или сменил слот; hook аудита |
| `script.method.server.Player.GetControlledCritter` | сервер | Действующий персонаж берётся у вызвавшего игрока |
| `script.method.server.Critter.GetItem#3e4a8f273e418d3b` | сервер | Разрешает заявленный идентификатор только в инвентаре действующего персонажа |
| `script.method.client.Game.Chosen` | клиент | Собственный персонаж клиента для общего правила |
| `setting.common.Critter.CritterSlotEnabled` | общая настройка | Включает слоты по индексу; слоту, объявленному в скрипте, нужна своя запись |

Строки `///@ Enum`, `///@ Property` и `///@ RemoteCall` — метаданные проекта, а `[[ServerRemoteCall]]`, `[[ClientRemoteCall]]`, `[[Event]]`, `[[ModuleInit]]`, `Player.ServerCall`, `Player.ClientCall` и клиентский getter `CurPlayer` — встроенные средства backend; в модель API они не входят. Грамматика удалённых вызовов описана в разделе [Удалённые вызовы](../../reference/scripting/remote-calls.md#грамматика-объявления), схема запроса — в [рецепте проверяемого запроса действия](validated-action.md).

## Минимальный пример

В `Examples/MechanicsSandbox/`: `Scripts/Recipes/EquipSlot.fos` содержит объявления, общее правило, серверный путь запроса, аудит и клиентский вызов; `Content/EquipSlot.fopro` — прототипы предметов; `FOnlineMechanicsSandbox.fomain` — настройку слотов; `Scripts/Tests/Test_EquipSlot.fos` — сценарии и серию запросов клиента; `Managed/Recipes/EquipSlot.cs` — форму Managed C#. Команды приведены в разделе [Маршрут проверки](#маршрут-проверки).

## Пошагово

### 1. Объявите слот, данные предмета и вызовы

В enum движка есть `Inventory = 0`, `Main = 1` и `Outside = 255`. Скрипт может добавлять к нему значения; `Armor = 2` — третий слот. `EquipSlot` — задаваемое авторами свойство `Common`, поэтому обе стороны читают слот, к которому подходит предмет. Удалённые вызовы несут только идентификатор предмета и желаемый слот, а `MaxBytes 9` — это один `ident` плюс один байт enum:

```angelscript
///@ Enum CritterItemSlot Armor = 2

///@ Property Item Common CritterItemSlot EquipSlot

///@ RemoteCall Server EquipItem(ident itemId, CritterItemSlot slot) MaxBytes 9
///@ RemoteCall Client EquipResult(ident itemId, bool accepted) MaxBytes 9
```

### 2. Включите слот в конфигурации

`ChangeItemSlot` отказывает для слота, который не включён в `Critter.CritterSlotEnabled`. Значение по умолчанию содержит две записи, поэтому третьему слоту нужна строка ниже:

```ini
Critter.CritterSlotEnabled = true true true
```

Две соседние настройки индексируются так же, и записи для слота `2` в них нет: `Critter.CritterSlotMultiItem` (по умолчанию `true false`) тогда считает новый слот слотом для одного предмета, так что второй предмет вытесняет первый, а `Critter.CritterSlotSendData` (по умолчанию `false true`) не пускает его содержимое к наблюдателям: они получают только слоты, отмеченные там как `true` (`Item::CanSendItem` в `Source/Server/Item.cpp`).

### 3. Задайте, к какому слоту подходит предмет

```ini
# Armor goes to the Armor slot, a sword to Main, a rock to no equipment slot at all
[ProtoItem]
$Name = EquipSlot_Armor
$Text engl = "Leather armor"
$Text russ = "Кожаная броня"
EquipSlot = Armor
```

В прототипе меча задано `EquipSlot = Main`; у камня не задано ничего, и его значение — по умолчанию `0`, обычный инвентарь.

### 4. Напишите правило один раз

В правиле нет данных, зависящих от стороны: `Item.CritterId` и `EquipSlot` есть на обеих сторонах, поэтому одна функция служит и интерфейсу, и серверу:

```angelscript
// The one rule, compiled for both sides: the UI predicts exactly what the server will decide
bool CanEquip(Critter cr, Item item, CritterItemSlot slot)
{
    if (item.CritterId != cr.Id) {
        return false;
    }

    // Putting an item back in the plain inventory is always allowed
    if (slot == CritterItemSlot::Inventory) {
        return true;
    }

    return item.EquipSlot == slot;
}
```

### 5. Примените его в обработчике запроса

Сервер берёт действующего персонажа у игрока, разрешает предмет только в инвентаре этого персонажа, выполняет правило и лишь затем меняет слот. Отказ записывает причину в журнал и отвечает `false`, ничего больше не сообщая клиенту:

```angelscript
// The only request path: the engine supplies the player, the critter and its inventory are derived from it
[[ServerRemoteCall]]
void EquipItem(Player player, ident itemId, CritterItemSlot slot)
{
    Critter? cr = player.GetControlledCritter();
    if (cr == null) {
        Refuse(player, itemId, "no_actor");
        return;
    }

    // Only the actor's own inventory resolves; any other id is not an item the actor may move
    Item? item = cr.GetItem(itemId);
    if (item == null) {
        Refuse(player, itemId, "not_owned");
        return;
    }

    if (!CanEquip(cr, item, slot)) {
        Refuse(player, itemId, "slot_rule");
        return;
    }

    cr.ChangeItemSlot(itemId, slot);
    Game.Log("equip_slot_accepted item=" + itemId.value + " slot=" + int(slot));
    player.ClientCall.EquipResult(itemId, true);
}

void Refuse(Player player, ident itemId, string reason)
{
    Game.Log("equip_slot_refused reason=" + reason + " player=" + player.Id.value + " item=" + itemId.value);
    player.ClientCall.EquipResult(itemId, false);
}
```

### 6. Проверьте после фиксации

Движок не может спросить скрипт до перемещения, поэтому аудит подписывается на событие, которое приходит после него. Он исправляет только перемещения, обошедшие обработчик, например проектный скрипт, вызвавший `ChangeItemSlot` напрямую:

```angelscript
// The engine fires this after the move is committed, so it cannot veto: it repairs a move that bypassed the request path
[[Event]]
void OnCritterItemMoved(Critter cr, Item item, CritterItemSlot fromSlot)
{
    // An item that left the inventory has no slot to audit
    if (item.CritterId != cr.Id || CanEquip(cr, item, item.CritterSlot)) {
        return;
    }

    Game.Log("equip_slot_repaired item=" + item.Id.value + " slot=" + int(item.CritterSlot));
    cr.ChangeItemSlot(item.Id, CritterItemSlot::Inventory);
}
```

### 7. Предскажите на клиенте

Клиент до отправки выполняет то же правило для своего выбранного персонажа, затем отправляет запрос и ждёт ответ; сам предмет он не перемещает:

```angelscript
// The client asks only for what the shared rule allows; the server repeats the check
void RequestEquip(Item item, CritterItemSlot slot)
{
    if (!CanEquip(Chosen, item, slot)) {
        Game.Log("equip_slot_client_refused item=" + item.Id.value);
        return;
    }

    CurPlayer.ServerCall.EquipItem(item.Id, slot);
}

[[ClientRemoteCall]]
void EquipResult(ident itemId, bool accepted)
{
    Game.Log("equip_slot_result item=" + itemId.value + " accepted=" + (accepted ? "true" : "false"));
}
```

## Полномочия и синхронизация

- Решает сервер. Действующий персонаж — `Player.GetControlledCritter()`, а предмет разрешается через `Critter.GetItem(itemId)`, который находит только собственный инвентарь действующего персонажа: предмет другого персонажа по идентификатору недостижим, поэтому `not_owned` не требует лишней проверки.
- Обработчик получает управление с покрытыми игроком и его управляемым персонажем, а это покрывает инвентарь персонажа; `ChangeItemSlot` затрагивает только этого персонажа и его предметы. `Game.Sync` не нужен; правилу, читающему другую сущность, он понадобится ([захват нескольких сущностей для одного действия](cover-transactions.md)).
- Проверка на клиенте — удобство. Она работает на данных клиента, а изменённый клиент может её пропустить, поэтому сервер повторяет её на своих данных.
- Наблюдатели видят экипированный предмет, только если в `Critter.CritterSlotSendData` по его индексу стоит `true`; владелец получает каждый включённый слот.

## Сохранение и миграция

`Item.CritterSlot` — сохраняемое базовое свойство, поэтому экипированный предмет хранит свой слот в своей записи; песочница не проверяет это перезапуском, потому что утверждение рецепта — правило, а не хранение. `EquipSlot` задаётся авторами и не сохраняется.

Объявленное в скрипте значение enum — сохраняемая идентичность: его номер и имя определяют смысл сохранённого слота. Не переиспользуйте номер `2` для другого смысла, а переименовывайте значение правилом `Enum` ([Объявляйте правило для каждого переименования, удаления и переноса](../project/data-migrations.md#объявляйте-правило-для-каждого-переименования-удаления-и-переноса)). Если позже выключить слот в `Critter.CritterSlotEnabled`, лежащие в нём предметы не переместятся: `Item::CanSendItem` перестанет их отправлять, поэтому сначала перенесите их в `Inventory`.

## Ловушки и антипаттерны

- Ждать события-вето. В `api.json` нет события, которое выполнялось бы до перемещения предмета; `Game.OnCritterItemMoved` вызывается после фиксации слота. Песочница регистрирует пробу с наивысшим приоритетом, и она уже видит нарушающий правило слот, когда скрипт перемещает броню в `Main`. Правило принадлежит обработчику запроса, а аудит — страховка.
- Доверять правилу клиента. Клиент песочницы пропускает правило и отправляет камень в Armor, броню в Main и броню постороннего персонажа; сервер отказывает с `slot_rule`, `slot_rule` и `not_owned` и принимает два честных запроса.
- Добавить значение enum, не включив слот. Тогда `ChangeItemSlot` выбрасывает `Slot is not allowed` и ничего не меняет (тест использует слот `3`, который не включает ни одна настройка); движок не выводит слоты из enum скрипта.
- Аудит, который не завершается. Перемещение предмета изнутри `Game.OnCritterItemMoved` вызывает событие снова, а обмен в слоте для одного предмета вызывает его для обоих предметов. Приведённый аудит заканчивается, потому что целевой слот исправления, `Inventory`, всегда проходит правило.
- Правило, читающее данные только сервера. Клиент вычислил бы другой ответ или не смог бы их прочитать; ограничивайте общую функцию свойствами, которые получают обе стороны.
- Класть отображаемый текст в ответ. Отвечайте кодом или `TextPackKey`, как сказано в [рецепте проверяемого запроса действия](validated-action.md#как-это-расширяет-крупный-проект).

## Маршрут проверки

Сценарий `equip-slot` запускает `-Testing.Filter equip_slot.` на `RecipeTests`. Его отрицательный контроль перемещает броню в `Main` прямым вызовом `ChangeItemSlot` и показывает, что движок принимает это, а аудит исправляет:

```angelscript
// Negative control: the engine runs no rule of its own, so a script that skips the request path moves anything anywhere
void EngineAcceptsAScriptMoveAndTheAuditRepairsIt(Testing::TestContext ctx)
{
    Map map = Sandbox::CreateMap();
    Critter cr = map.AddCritter(Sandbox::NpcPid, NpcHex, mdir(0));
    Item armor = cr.AddItem(ArmorPid);

    cr.TestProbeSlot = -1;
    cr.ChangeItemSlot(armor.Id, CritterItemSlot::Main);

    Testing::ExpectEq(ctx, cr.TestProbeSlot, int(CritterItemSlot::Main), "The engine committed the rule-breaking move before any audit ran");
    Testing::Expect(ctx, armor.CritterSlot == CritterItemSlot::Inventory, "The post-commit audit put the item back");

    Game.DestroyLocation(map.GetLocation());
}
```

Сценарий `equip-slot-client` запускает сервер с `RecipeNetworkTests` и `-Testing.Filter equip_slot_client.`, ждёт `equip_slot_waiting_for_client`, затем запускает клиент без графического интерфейса с `RecipeClient`, как в [маршруте проверки проверяемого запроса действия](validated-action.md#маршрут-проверки). Клиент сначала применяет общее правило локально к камню, затем отправляет три поддельных запроса, пропускающих правило, два честных и завершающий `EquipDrillSent`; обработчики одного соединения выполняются в порядке поступления, поэтому этот последний вызов обрабатывается после пяти запросов. Запустите все сценарии песочницы из `Examples/MechanicsSandbox` командой `python validate.py` на Windows (PowerShell) или `python3 validate.py` на Linux (bash); запуск успешен, когда `BuildTools/gameplay_test_runner.py` сообщает `status=passed` для каждого сценария и завершается с кодом `0`. Среди требуемых маркеров:

```text
equip_slot_repaired item=
equip_slot_client_refused item=
equip_slot_refused reason=slot_rule
equip_slot_refused reason=not_owned
equip_slot_accepted item=
[Testing] PASS equip_slot_client.client_requests_are_enforced_on_the_server
```

## Managed C#

Статус `shape`: тест движка разбирает `Managed/Recipes/EquipSlot.cs` и сравнивает его строки `///@` с модулем AngelScript, они совпадают; ни одна линия движка его не компилирует и не запускает. Аудит сохраняет структуру с `[RequiresCover]` на параметрах сущностей, а обработчик объявлен `internal` ([форма авторского кода Managed C#](../scripting/managed-csharp.md#форма-авторского-кода)):

```csharp
    // The engine fires this after the move is committed, so it cannot veto: it repairs a move that bypassed the request path
    [Event]
    internal static void OnCritterItemMoved([RequiresCover] Critter cr, [RequiresCover] Item item, CritterItemSlot fromSlot)
    {
        // An item that left the inventory has no slot to audit
        if (item.CritterId != cr.Id || CanEquip(cr, item, item.CritterSlot)) {
            return;
        }

        Game.Log("equip_slot_repaired item=" + item.Id + " slot=" + (int)item.CritterSlot);
        cr.ChangeItemSlot(item.Id, CritterItemSlot.Inventory);
    }
```

Клиент читает `Game.Chosen` и `Game.CurPlayer` как свойства. Проект на managed backend должен выполнить ту же серию запросов клиента, прежде чем полагаться на эту форму.

## Как это расширяет крупный проект

- **Больше слотов.** Добавьте по одному значению enum и по одной записи в три настройки на каждый слот и мигрируйте сохранённые слоты, когда номер меняет смысл. Ни один слот за индексом `2` не запускался.
- **Правило как данные.** Замените единственное значение `EquipSlot` компонентом или строкой каталога со списком допустимых слотов и требований, оставив `CanEquip` единственным читателем ([компоненты свойств](../content/prototype-format.md#компоненты-свойств)).
- **Предсказание.** Клиент, перемещающий предмет до прихода ответа, обязан откатить его при отказе; рецепт оставляет интерфейс в прежнем состоянии до ответа.
- **Последствия.** Характеристики от экипировки — это [производные значения](derived-stats.md), пересчитываемые по надетым предметам, а не число, добавленное в момент экипировки.
- **Что остаётся в игровом проекте.** Названия слотов, что куда подходит, стоимость и время экипировки и данные предметов.

## См. также

- [Создание игровых механик](index.md)
- [Проверяемый запрос действия](validated-action.md)
- [Производные характеристики с геттерами и сеттерами](derived-stats.md)
- [Удалённые вызовы](../../reference/scripting/remote-calls.md)
- [Жизненный цикл и конкурентность скриптов](../scripting/lifecycle-and-concurrency.md)
- [Скрипты Managed C#](../scripting/managed-csharp.md)
- [Миграция сохранённых данных и контента в игровом проекте](../project/data-migrations.md)
