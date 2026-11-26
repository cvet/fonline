---
layout: default
title: Прерываемое длительное действие
locale: ru
document_id: recipe-long-action
permalink: /Docs/ru/how-to/mechanics/long-action.html
recipe_kind: primitive
---

# Прерываемое длительное действие
<!-- docs-translation: {"document_id":"recipe-long-action","locale":"ru","source_path":"Docs/en/how-to/mechanics/long-action.md","source_sha256":"730c2993d2848c70bda0a104e20b6c86d4ab465cf6bf25c50f03712f594f8737"} -->
Этот рецепт строит добычу, которая занимает время: игрок просит добыть ресурс из месторождения, сервер запускает одну задачу для криттера, и когда время выходит, месторождение выдаёт один результат и теряет один заряд — если только что-то не прервало задачу раньше. Уход с места, перенос, нокаут, смерть и отключение игрока отменяют задачу через одну функцию `Cancel`. Токен отличает живое завершение от устаревшего, потому что остановка таймера не останавливает завершение, которое уже выполняется; песочница хранит вариант только с таймером как отрицательный контроль и показывает, что он выдаёт результат после остановки.

| Backend | Статус |
| --- | --- |
| AngelScript | `runtime-verified` |
| Managed C# | `shape` |

`runtime-verified` здесь означает, что сценарий `long-action` внутри процесса, сценарий с настоящим клиентом `long-action-client` и двухфазные сценарии перезапуска `long-action-restart-write` и `long-action-restart-verify` прошли на собранной песочнице после сборки ресурсов на хосте Windows. Не проверены: компиляция и запуск Managed C#, смена игроком управляемого криттера, прерывание, которое приходит, пока завершение приостановлено внутри собственного `Game.Sync`, и путь восстановления серверного снимка.

## Проверенные исходные пути

- `Examples/MechanicsSandbox/Scripts/Recipes/LongAction.fos`
- `Examples/MechanicsSandbox/Scripts/Tests/Test_LongAction.fos`
- `Examples/MechanicsSandbox/Managed/Recipes/LongAction.cs`
- `Examples/MechanicsSandbox/Content/LongAction.fopro`
- `Examples/MechanicsSandbox/mechanics-smoke.json`
- `Source/Common/TimeEvents.cpp`
- `Source/Server/Server.cpp`
- `Source/Server/MapManager.cpp`
- `Source/Scripting/ServerCritterScriptMethods.cpp`
- `Docs/generated/api.json`

## Задача

Задача, которая длится секунды, не должна завершаться после того, как мир сделал её бессмысленной. Игрок ушёл, криттер потерял сознание или погиб, соединение игрока оборвалось или месторождение истощилось, а таймер, сработавший всё равно, выдал бы результат за работу, которой никто не делал. Движок даёт time events и события, сигнализирующие о каждом прерывании; держать одну задачу на криттера, отменять её по каждому сигналу и отличать живое завершение от устаревшего — это рецепт. Какие действия длительные, сколько они длятся и что выдают — игровая политика.

## Используемые примитивы движка

| Символ | Сторона | Роль в рецепте |
| --- | --- | --- |
| `script.method.server.Entity.StartTimeEvent#b494dfd4a4fa81ea` | сервер | `StartTimeEvent(timespan delay, callback(void,Entity,any[]) func, any[] data)` планирует завершение с токеном в данных |
| `script.method.server.Entity.StopTimeEvent#ddfbd34ed1764f92` | сервер | `StopTimeEvent(callback(void,Entity,any[]) func)` убирает ожидающее завершение |
| `script.method.server.Entity.CountTimeEvent#3032339c32e34b9d` | сервер | `CountTimeEvent(callback(void,Entity,any[]) func, bool excludeExecuting = false)` по запросу считает только ожидающие завершения |
| `script.event.server.Game.OnCritterStartMoving` | сервер | Прерывание: началась ходьба |
| `script.event.server.Game.OnCritterTransfer` | сервер | Прерывание: криттера перенесли, в том числе в пределах одной карты |
| `script.event.server.Game.OnPlayerLogout` | сервер | Прерывание: соединение управляющего игрока завершилось |
| `script.property.Critter.Condition` | общая | Сигнал о нокауте и смерти; события смерти в движке нет |
| `script.enum.CritterCondition` | общая | Работать может только `Alive` |
| `script.method.server.Critter.SetCondition` | сервер | Тесты с её помощью отправляют криттеров в нокаут и убивают их |
| `script.method.server.Player.Disconnect` | сервер | Клиентская проверка заканчивается корректным отключением |
| `script.method.server.Player.GetControlledCritter` | сервер | Действующий криттер запроса и отключения |
| `script.method.server.Map.GetItem` | сервер | Находит месторождение по id на карте самого криттера |
| `script.method.server.Game.Sync#b42b6f6d6b684d2a` | сервер | `Sync(Entity entity1, Entity entity2)` покрывает игрока и карту при запросе |
| `script.method.server.Game.Sync#a0c5737d3ba111b8` | сервер | `Sync(Entity[] entities)` покрывает криттера и карту при завершении |

`[[TimeEvent]]`, `[[Async]]`, `[[ServerRemoteCall]]`, `[[ClientRemoteCall]]`, `[[PropertySetter]]`, `Game.AddPropertySetter` и `Yield` — встроенные средства backend, а строки `///@ Property` и `///@ RemoteCall` — метаданные проекта.

## Минимальный пример

В `Examples/MechanicsSandbox/`: `Scripts/Recipes/LongAction.fos` — сам рецепт, `Scripts/Tests/Test_LongAction.fos` содержит его тесты и наивное завершение, `Content/LongAction.fopro` — прототипы месторождений (`NodeCharges = 2`, `NodeWorkMs = 800` и медленное месторождение на 60000 мс для теста перезапуска), руду и работника, а `Managed/Recipes/LongAction.cs` — форму Managed C#. Команды приведены в разделе [Маршрут проверки](#маршрут-проверки).

## Пошагово

Реестр выполняющейся задачи живёт на криттере и существует только во время работы: флага `Persistent` нет, поэтому после перезапуска каждый криттер простаивает. Заряды месторождения сохраняемые и публичные; время работы — авторские данные в прототипе:

```angelscript
///@ Property Critter Server bool ActionActive Mutable
///@ Property Critter Server int32 ActionToken Mutable
///@ Property Critter Server ident ActionTarget Mutable
///@ Property Item Common int32 NodeCharges Mutable Persistent PublicSync
///@ Property Item Common int32 NodeWorkMs

///@ RemoteCall Server BeginGather(ident nodeId) MaxBytes 8
///@ RemoteCall Client GatherResult(ident nodeId, bool accepted) MaxBytes 9
```

`BeginGather` — обработчик запроса, построенный как в рецепте [Проверяемый запрос действия](validated-action.md): он определяет действующего криттера по игроку, покрывает игрока и карту через `Game.Sync`, определяет действующего заново, вызывает `Start` и отвечает через `GatherResult`. `Start` допускает одну задачу на криттера. Второй запрос, пока идёт первая, отклоняется как `busy`, а отказ ничего не запускает. В данных таймера передаётся только токен; карта и месторождение заново находятся при завершении:

```angelscript
// Starts the one action a critter may have; returns the refusal reason or an empty string. The caller covers the critter and its map
string Start(Critter cr, Map map, ident nodeId)
{
    if (cr.Condition != CritterCondition::Alive) {
        return "not_alive";
    }

    // One job per critter; a cancelled completion may still be running with a stale token, so only a waiting one counts
    if (cr.ActionActive || cr.CountTimeEvent(FinishAction, true) != 0) {
        return "busy";
    }

    // The node is resolved on the critter's own map; its work time is data on the item
    Item? node = map.GetItem(nodeId);
    if (node == null || node.NodeWorkMs <= 0 || node.NodeCharges <= 0) {
        return "no_node";
    }

    if (Game.GetDistance(cr, node) > WorkDistance) {
        return "too_far";
    }

    cr.ActionToken = cr.ActionToken + 1;
    cr.ActionActive = true;
    cr.ActionTarget = nodeId;

    // Only an id and a token travel in the timer: no handle to the critter's map or node survives the wait
    any[] data = {any(cr.ActionToken)};
    cr.StartTimeEvent(Milliseconds(node.NodeWorkMs), FinishAction, data);
    return "";
}
```

`Cancel` — единственный выход для каждого прерывания, и его безопасно вызывать, когда ничего не выполняется. Он сбрасывает флаг и увеличивает токен, поэтому уже запущенное завершение тоже находит несовпадающий токен, а затем убирает ожидающий таймер. `IsCurrent(cr, token)` — это `cr.ActionActive && cr.ActionToken == token`:

```angelscript
// The one way out for every interrupt; it is safe to call when nothing is running
void Cancel(Critter cr, string reason)
{
    if (!cr.ActionActive) {
        return;
    }

    cr.ActionActive = false;

    // A completion that is already running cannot be stopped, so it also finds a token that no longer matches
    cr.ActionToken = cr.ActionToken + 1;
    cr.StopTimeEvent(FinishAction);
    Game.Log("long_action_cancelled critter=" + cr.Id.value + " reason=" + reason);
}
```

Каждое прерывание — один факт движка, подключённый к `Cancel` в `InitLongAction`:

```angelscript
[[ModuleInit]]
void InitLongAction()
{
    // The interrupts: each one is an engine fact that makes the work pointless
    Game.OnCritterStartMoving.Subscribe(OnCritterStartMoving);
    Game.OnCritterTransfer.Subscribe(OnCritterTransfer);
    Game.OnPlayerLogout.Subscribe(OnPlayerLogout);

    // The engine has no death event; the condition property is the signal
    Game.AddPropertySetter(CritterProperty::Condition, OnConditionChanged);
}
```

| Прерывание | Сигнал движка | Причина в журнале |
| --- | --- | --- |
| Криттер начинает идти | `Game.OnCritterStartMoving`, который вызывается и при смене скорости идущего криттера | `moved` |
| Криттера переносят | `Game.OnCritterTransfer`, в том числе для `TransferToMap` в пределах одной карты | `transferred` |
| Нокаут или смерть | Callback после записи свойства `Critter.Condition` | `incapacitated` |
| Игрок отключается | `Game.OnPlayerLogout`, к которому приводит `Player.Disconnect()` | `disconnected` |

Завершение выполняется спустя долгое время после решения о старте, поэтому оно проверяет всё заново. Токен проверяется до cover, что ничего не стоит для устаревшего вызова, и ещё раз после него, потому что захват cover даёт выполняться другим заданиям. Карта и месторождение находятся по id уже под новым cover; работа меняет предмет карты, поэтому карта входит в cover:

```angelscript
// The completion step: it runs long after the decision to start, so it resolves and checks everything again
[[Async]]
void Complete(Critter cr, int32 token)
{
    if (!IsCurrent(cr, token)) {
        return;
    }

    Map? map = cr.GetMap();
    if (map == null) {
        Cancel(cr, "no_map");
        return;
    }

    // The work changes a map item, so the map joins the cover; getting it may let other work run first
    ident nodeId = cr.ActionTarget;
    Game.Sync(array<Entity> = {cr, map});

    if (cr.IsDestroyed || cr.MapId != map.Id || !IsCurrent(cr, token)) {
        return;
    }

    Item? node = map.GetItem(nodeId);
    if (node == null || node.NodeCharges <= 0) {
        Cancel(cr, "node_gone");
        return;
    }

    cr.ActionActive = false;
    node.NodeCharges = node.NodeCharges - 1;
    cr.AddItem(OrePid);
    Game.Log("long_action_done critter=" + cr.Id.value + " node=" + nodeId.value);
}
```

`FinishAction` — точка входа `[[TimeEvent]]`, которая читает токен из массива данных и вызывает `Complete`; вызов вынесен отдельно, чтобы тест мог напрямую передать устаревший токен.

### Наивный вариант не работает

`Scripts/Tests/Test_LongAction.fos` хранит вариант, который отменяет только через `StopTimeEvent`. Его завершение приостанавливается, как любое завершение, ожидающее cover:

```angelscript
[[TimeEvent]] [[Async]]
void NaiveFinish(Critter cr, any[] data)
{
    // The naive completion suspends like any completion that waits for its cover
    Yield(300);
    Game.Sync(cr);
    cr.AddItem(OrePid);
    Game.Log("long_action_naive_granted critter=" + cr.Id.value);
}
```

Отмена, пришедшая во время приостановки, убирает только запись таймера:

```angelscript
    Game.Log("long_action_naive_cancel pending=" + worker.CountTimeEvent(NaiveFinish) + " pending_not_running=" + worker.CountTimeEvent(NaiveFinish, true));
    worker.StopTimeEvent(NaiveFinish);
```

Тест требует маркер `long_action_naive_granted critter=` и одну руду у отменённого работника. В момент остановки журнал показывает `pending=1 pending_not_running=0`: выполняющееся событие всё ещё считается, а при `excludeExecuting`, равном true, исключается. В исходниках `TimeEventManager::StopTimeEvent` (`Source/Common/TimeEvents.cpp`) стирает запись события, а `ServerEngine::OnTimeEventCancel` отменяет запланированное рабочее задание; запуск показывает, что уже начавшийся скрипт не прерывается. Отменить его может только состояние, которое завершение читает заново, — токен.

## Полномочия и синхронизация

- Клиент только просит. `BeginGather` принимает id месторождения и ничего больше; действующий криттер, карта, правило досягаемости, заряды и время работы берутся на сервере. Клиентская проверка отправляет далёкое месторождение, предмет, не являющийся месторождением, нужное месторождение и его же ещё раз, а сервер отвечает `too_far`, `no_node`, принятием и `busy`.
- Завершение — time event криттера, который покрывает этого криттера. Карта входит одним `Game.Sync` криттера и карты; месторождение — предмет карты и покрывается через неё ([модель cover](cover-transactions.md#модель-cover-на-практике)).
- Обработчики прерываний пишут только свойства криттера и останавливают его таймеры, поэтому cover, с которым их вызвали, достаточно. `OnPlayerLogout` достаёт криттера через игрока.
- Через ожидание проходят только id и токен. Handle карты или месторождения, удерживаемый через задержку, может указывать на уничтоженную сущность, а cover не переживает приостановку ([cover не переживает приостановку](../scripting/lifecycle-and-concurrency.md#cover-не-переживает-приостановку)).

## Сохранение и миграция

| Переживает перезапуск | Не переживает |
| --- | --- |
| `NodeCharges`, сохраняемое свойство месторождения | Реестр (`ActionActive`, `ActionToken`, `ActionTarget`) и таймер завершения |

Сценарии перезапуска запускают 60-секундную задачу в сохраняемой локации и останавливают сервер. После перезапуска работник простаивает, таймера завершения нет, ничего не выдано, у месторождения остались оба заряда, а работник может снова начать задачу. Рецепт намеренно бросает работу при перезапуске и списывает заряд при завершении, поэтому брошенная работа ничего не стоит. Проект, который возобновляет задачу, сохраняет срок окончания и заново запускает таймер из hook восстановления, как в рецепте [Временный эффект, переживающий перезапуск](timed-effect.md), и тогда же решает, что выдавать за задачу, которая закончилась, пока сервер не работал.

Переименование `NodeCharges` следует разделу [Объявляйте правило для каждого переименования, удаления и переноса](../project/data-migrations.md#объявляйте-правило-для-каждого-переименования-удаления-и-переноса).

## Ловушки и антипаттерны

- Отмена только через `StopTimeEvent`. Он убирает ожидающий таймер, но не завершение, которое уже началось, как показывает отрицательный контроль. Увеличивайте токен в `Cancel` и проверяйте его после последнего захвата cover.
- Подсчёт задачи по её собственному выполняющемуся таймеру. `CountTimeEvent` считает выполняющийся callback, если `excludeExecuting` не true, поэтому проверка `busy` в `Start` передаёт `true` и считает только ожидающее завершение; завершение, которое уже приняло решение, но ещё выполняется, не блокирует следующий старт.
- Ожидание события смерти. В движке его нет: сигнал — свойство состояния, а `SetCondition` в состояние, в котором криттер уже находится, ничего не меняет и не вызывает обработчик. Подключайте нокаут и смерть к этому обработчику.
- Удержание handles через ожидание. Храните id в данных таймера и находите сущности под новым cover; уничтоженная карта или месторождение тогда становятся причиной отказа, а не сбоем.
- Предположение, что список прерываний полон. Каждая дополнительная причина, например получение урона или выбрасывание инструмента, требует собственного вызова `Cancel`; события движка покрывают только четыре перечисленных. Смена игроком управляемого криттера среди них не значится и не запускалась.
- Запуск задачи без проверки состояния «жив», досягаемости и состояния месторождения на сервере. Это правила проверки, а клиентская копия этих правил — лишь удобство.

## Маршрут проверки

Запускайте все сценарии песочницы из `Examples/MechanicsSandbox` командой `python validate.py` в Windows (PowerShell) или `python3 validate.py` в Linux (bash); предварительные условия перечислены в её `README.md`. Сценарии этого рецепта среди прочего требуют:

```text
[Testing] PASS long_action.stale_completion_is_ignored
[Testing] PASS long_action.stop_time_event_does_not_stop_a_running_completion
[Testing] PASS long_action_client.client_requests_are_validated_and_disconnect_cancels
[Testing] PASS long_action_restart_verify.restart_abandons_the_work
long_action_refused reason=too_far
long_action_refused reason=no_node
long_action_refused reason=busy
long_action_naive_granted critter=
reason=disconnected
```

Сценарий с клиентом запускает сервер с sub-config `RecipeNetworkTests`, ждёт `long_action_waiting_for_client` и запускает клиент без графического интерфейса с `RecipeClient`. Клиент отправляет четыре запроса подряд, а затем `GatherDrillSent`; обработчики одного соединения выполняются в порядке прихода, поэтому сервер проверяет, что задачу начал только допустимый запрос, и затем отключает игрока посреди работы. Чтобы запустить набор внутри процесса на готовой сборке ресурсов, из каталога сборки (PowerShell; в Linux используйте `./Binaries/Server-Linux-x64/FOMECH_ServerHeadless`):

```powershell
& ".\Binaries\Server-Windows-win64\FOMECH_ServerHeadless.exe" -ApplyConfig "<sandbox>\FOnlineMechanicsSandbox.fomain" -ApplySubConfig RecipeTests -Testing.Filter "long_action."
```

Для пары перезапуска используйте `-ApplySubConfig RecipeRestart -Server.DbStorage "JSON <empty-directory>"` с фильтрами `long_action_restart_write.` и затем `long_action_restart_verify.` на одном каталоге.

## Managed C#

Статус `shape`: тест движка разбирает `Managed/Recipes/LongAction.cs` и сравнивает его строки `///@` с модулем AngelScript; ни одна линия движка не компилирует и не запускает его. Поток тот же. Завершение — time event `async Task`, `internal`, потому что его вызывает движок, а cover — ожидаемый вспомогательный вызов, сообщающий об исчезнувшей сущности, поэтому вызов становится условием. Обработчик свойства состояния форма регистрирует через Managed-написание `Game.AddPropertyDeferredSetter`, которое ни одна линия не компилировала:

```csharp
        if (!await Sync.Lock(cr, map)) {
            return;
        }
```

## Как это расширяет крупный проект

- **Много действий.** Один реестр на криттера обслуживает любое длительное действие: добавьте в реестр вид действия, дайте каждому виду собственную функцию завершения и оставьте единый `Cancel`. Действия, которые могут идти параллельно, требуют реестра на каждый вид.
- **Действия на данных.** Длительности, выдача и требования — авторские строки, например в [каталоге FixedType](../../explanation/entity-and-property-model/choosing-a-representation.md#строка-каталога-fixedtype) с ключом по id действия; предмет хранит только то, что меняется, — свои заряды.
- **Событие проекта с производителем.** Вызывайте одно событие проекта там, где задача завершается или отменяется: так модули заданий и статистики реагируют без опроса.
- **Возобновляемая работа.** Храните срок окончания в сохраняемом свойстве и заново запускайте таймер из hook восстановления ([временный эффект](timed-effect.md)); токен сохраняйте, потому что отменённое завершение может ещё выполняться.
- **Что остаётся в игровом проекте.** Какие действия длительные, их длительности, выдача, что прерывает их помимо четырёх сигналов движка, и показ хода выполнения на клиенте. Движок даёт time events и события прерываний; реестр и токен — код проекта.

## См. также

- [Создание игровых механик](index.md)
- [Проверяемый запрос действия](validated-action.md)
- [Временный эффект, переживающий перезапуск](timed-effect.md)
- [Захват нескольких сущностей для одного действия](cover-transactions.md)
- [Жизненный цикл и конкурентность скриптов: завершение time event](../scripting/lifecycle-and-concurrency.md#завершение-time-event)
- [Жизненный цикл и конкурентность скриптов: time events после перезагрузки из базы данных](../scripting/lifecycle-and-concurrency.md#time-events-после-перезагрузки-из-базы-данных)
- [Скрипты Managed C#](../scripting/managed-csharp.md)
- [Шаблон обвязки скриптовых тестов](../project/script-test-harness.md)
