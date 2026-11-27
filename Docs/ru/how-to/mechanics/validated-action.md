---
layout: default
title: Проверяемый запрос действия
locale: ru
document_id: recipe-validated-action
permalink: /Docs/ru/how-to/mechanics/validated-action.html
recipe_kind: primitive
---

# Проверяемый запрос действия
<!-- docs-translation: {"document_id":"recipe-validated-action","locale":"ru","source_path":"Docs/en/how-to/mechanics/validated-action.md","source_sha256":"1acc83cae4516f17ab3f66eb4f8b6291dc720c65696f5a64a33fd3d3f254e886"} -->
Игрок щёлкает по устройству, и клиент просит сервер его активировать. Рецепт рассматривает этот удалённый вызов как намерение, а не как команду: сервер определяет действующего персонажа по соединению, разрешает названную цель только там, где персонаж может до неё дотянуться, выполняет все проверки в фиксированном порядке и лишь затем списывает стоимость и применяет эффект. Поддельные и устаревшие запросы отклоняются с записанной в журнал причиной и ничего не меняют.

| Backend | Статус |
| --- | --- |
| AngelScript | `runtime-verified` |
| Managed C# | `shape` |

## Проверенные исходные пути

- `Examples/MechanicsSandbox/Scripts/Recipes/ValidatedAction.fos`
- `Examples/MechanicsSandbox/Scripts/Tests/Test_ValidatedAction.fos`
- `Examples/MechanicsSandbox/Scripts/Sandbox.fos`
- `Examples/MechanicsSandbox/Content/ValidatedAction.fopro`
- `Examples/MechanicsSandbox/Managed/Recipes/ValidatedAction.cs`
- `Examples/MechanicsSandbox/mechanics-smoke.json`
- `Examples/MechanicsSandbox/FOnlineMechanicsSandbox.fomain`
- `Source/Server/Server.cpp`
- `Source/Server/ClientDataValidation.cpp`
- `Source/Server/EntitySync.cpp`
- `Source/Scripting/AngelScript/AngelScriptAttributes.cpp`
- `Source/Scripting/AngelScript/AngelScriptRemoteCalls.cpp`
- `Source/Scripting/Managed/Analyzers/SyncCoverAnalyzer.cs`
- `Docs/generated/api.json`

## Задача

Устройство на карте принадлежит одному персонажу, который может активировать его не дальше чем с двух гексов за одно очко энергии действия. Клиент знает, по какому устройству щёлкнул игрок, но не решает, состоится ли активация. Изменённый клиент может прислать любой идентификатор, а честный — идентификатор, устаревший, пока запрос был в пути. Какие устройства существуют, их дальность и цена — игровая политика. Рецепт учит той части, которую движок оставляет каждому проекту: откуда берётся каждый факт, в каком порядке идут проверки и в какой момент списывается стоимость.

## Используемые примитивы движка

| Символ | Сторона | Зачем |
| --- | --- | --- |
| `script.method.server.Player.GetControlledCritter` | сервер | Действующий персонаж берётся из вызывающего игрока, а не из payload |
| `script.method.server.Critter.GetMap` | сервер | Карта персонажа ограничивает, где может быть цель |
| `script.method.server.Game.Sync#b42b6f6d6b684d2a` | сервер | Захватывает cover игрока (вместе с его существом) и карты одним запросом |
| `script.property.Critter.MapId` | сервер | Подтверждает, что после захвата персонаж всё ещё на этой карте |
| `script.method.server.Map.GetItem` | сервер | Разрешает заявленный идентификатор только среди предметов этой карты |
| `script.method.server.Game.GetDistance#3dade8869ec8c0ac` | сервер | Дальность между персонажем и устройством |
| `script.method.common.Game.Log` | обе | Причины отказа на сервере, ответ на клиенте |
| `script.method.server.Map.AddItem#93da72a5ec69c371` | сервер | Тест размещает устройства |
| `script.method.server.Game.LoginPlayerToNewRecord`, `script.method.server.Player.SwitchCritter` | сервер | Вход в песочницу даёт клиенту без графического интерфейса управляемого существа |
| `setting.server.ServerNetwork.MaxRemoteCallPayloadSize` | сервер | Общий для сервера предел payload поверх `MaxBytes` каждого вызова |

Объявления `///@ Property` и `///@ RemoteCall`, `[[ServerRemoteCall]]`, `[[ClientRemoteCall]]`, `[[Async]]`, `Player.ServerCall`, `Player.ClientCall`, `Entity.Id` и `Entity.ProtoId` — метаданные проекта или встроенные средства backend, они не входят в сгенерированную модель API; их грамматика описана в [удалённых вызовах](../../reference/scripting/remote-calls.md#грамматика-объявления). В движке нет события «использовать» или конвейера действий: этим конвейером служат удалённый вызов и его обработчик.

## Минимальный пример

| Файл в `Examples/MechanicsSandbox` | Роль |
| --- | --- |
| `Scripts/Recipes/ValidatedAction.fos` | Объявления, серверный обработчик и клиентский обработчик ответа |
| `Content/ValidatedAction.fopro` | Прототип предмета `ValidatedAction_Device` |
| `Scripts/Tests/Test_ValidatedAction.fos` | Создаёт цели, управляет настоящим клиентом без графического интерфейса, проверяет изменения |
| `Scripts/Sandbox.fos` | Общее создание карты и вход клиента без графического интерфейса |
| `Managed/Recipes/ValidatedAction.cs` | Те же объявления и та же последовательность на Managed C# |

## Пошагово

### 1. Объявите запрос и ответ

```angelscript
///@ Property Critter Common int32 ActionEnergy Mutable Persistent OwnerSync
///@ Property Item Server ident DeviceOwnerId Mutable Persistent
///@ Property Item Common int32 DeviceActivations Mutable Persistent PublicSync

///@ RemoteCall Server ActivateDevice(ident deviceId) MaxBytes 8
///@ RemoteCall Client DeviceActivationResult(ident deviceId, bool accepted) MaxBytes 9
```

Запрос несёт один `ident`, и `MaxBytes 8` — ровно его размер; ответ состоит из `ident` и `bool`. Владелец хранится в свойстве `Server`, поэтому ни один клиент его не получает. Энергия объявлена `OwnerSync`, чтобы клиент владельца мог сделать кнопку неактивной, но сервер перед действием читает её заново.

### 2. Определите действующего персонажа по вызывающему игроку

```angelscript
    // The engine supplies the calling player; nothing in the payload names the actor
    Critter? actor = player.GetControlledCritter();
    if (actor == null) {
        Refuse(player, deviceId, "no_actor");
        return;
    }

    Map? map = actor.GetMap();
    if (map == null) {
        Refuse(player, deviceId, "actor_not_on_map");
        return;
    }
```

Движок передаёт обработчику первым аргументом `Player` того соединения, которое прислало вызов; в объявлении вообще нет аргумента с действующим персонажем. Игрок без управляемого существа или с существом на глобальной карте получает отказ раньше, чем просматривается хоть один идентификатор из payload.

### 3. Захватите карту и определите всё заново

```angelscript
    // The entry cover holds the player and its critter; map items need the map, and Sync replaces the set
    Game.Sync(player, map);

    // Other work may run while the cover changes hands, so the actor and its map are derived again
    actor = player.GetControlledCritter();
    if (actor == null || actor.MapId != map.Id) {
        Refuse(player, deviceId, "actor_moved");
        return;
    }
```

Диспетчер входит в обработчик с захваченными игроком и его управляемым существом ([cover нативного входа](../scripting/lifecycle-and-concurrency.md#cover-нативного-входа)). Для чтения предметов карты нужна карта, и `Game.Sync` заменяет cover полным набором. Если карта занята, захват отпускает всё, что держит поток, включая cover входа, ждёт и возвращает всё перед выходом (`SyncContext::SyncEntities` и `SyncContext::AcquireLocks` в `Source/Server/EntitySync.cpp`). За время ожидания другое задание может переместить персонажа, поэтому обработчик заново определяет персонажа и проверяет, что тот всё ещё на захваченной карте.

### 4. Разрешите цель и проверьте её

```angelscript
    // Only items on the actor's own map resolve; an id from anywhere else is not a target
    Item? device = map.GetItem(deviceId);
    if (device == null || device.ProtoId != DevicePid) {
        Refuse(player, deviceId, "unknown_target");
        return;
    }

    if (device.DeviceOwnerId != actor.Id) {
        Refuse(player, deviceId, "not_owner");
        return;
    }

    if (Game.GetDistance(actor, device) > ActivationDistance) {
        Refuse(player, deviceId, "too_far");
        return;
    }

    if (actor.ActionEnergy < ActivationCost) {
        Refuse(player, deviceId, "no_energy");
        return;
    }
```

Порядок выбран намеренно:

1. Идентичность и вид. `Map.GetItem` разрешает только предметы карты персонажа, поэтому идентификатор устройства на другой карте даёт `unknown_target`, даже если персонаж им владеет; глобальный поиск вроде `Game.GetItem` нашёл бы его и оставил проверку карты на память разработчика. Проверка прототипа не даёт применить действие к другому предмету.
2. Владение, прочитанное из свойства, доступного только серверу.
3. Дальность. Она меняется при движении, поэтому в неё упираются и честные клиенты.
4. Достаточность ресурса — последней, потому что именно это значение сейчас будет списано.

### 5. Спишите стоимость, затем примените эффект

```angelscript
    // Every check that can refuse has passed: charge the cost, then apply the effect
    actor.ActionEnergy = actor.ActionEnergy - ActivationCost;
    device.DeviceActivations = device.DeviceActivations + 1;

    Game.Log("validated_action_accepted device=" + deviceId.value);
    player.ClientCall.DeviceActivationResult(deviceId, true);
```

После списания ничто не может отказать, и последнюю проверку от записи не отделяет ни одна приостановка, поэтому отклонённый запрос не оставляет следов и ничего не нужно возвращать. Списание и эффект выполняются под тем же cover, что и проверки.

### 6. Отказывайте с причиной в журнале

```angelscript
// Logs the caller and the claimed id for diagnosis; the reply tells the client only that it failed
void Refuse(Player player, ident deviceId, string reason)
{
    Game.Log("validated_action_refused reason=" + reason + " player=" + player.Id.value + " device=" + deviceId.value);
    player.ClientCall.DeviceActivationResult(deviceId, false);
}
```

Строка журнала называет вызывающего и заявленный идентификатор, но не весь payload. Ответ сообщает только `false`: различие `not_owner` и `unknown_target` подсказало бы зондирующему клиенту, существует ли где-то устройство с таким идентификатором.

### 7. Покажите ответ на клиенте

```angelscript
[[ClientRemoteCall]]
void DeviceActivationResult(ident deviceId, bool accepted)
{
    Game.Log("validated_action_result device=" + deviceId.value + " accepted=" + (accepted ? "true" : "false"));
}
```

Клиентский обработчик только отображает результат. Настоящий интерфейс обновил бы виджет; авторитетное состояние он не меняет никогда.

## Полномочия и синхронизация

| Факт | Откуда берётся |
| --- | --- |
| Кто действует | Предоставленный движком `Player`, затем `GetControlledCritter()` |
| Где | `GetMap()` персонажа; клиент никогда не называет карту |
| Какое устройство | `deviceId` от клиента, разрешённый только на этой карте |
| Владелец, дальность, энергия | Серверные свойства и `Game.GetDistance`, прочитанные под новым cover |
| Результат | Ответ сервера; клиент только отображает его |

Транспортные проверки выполняются до обработчика. `ServerEngine::Process_RemoteCall` в `Source/Server/Server.cpp` находит вызов по имени и выбрасывает исключение, если payload больше `MaxBytes` вызова или `ServerNetwork.MaxRemoteCallPayloadSize` (`setting.server.ServerNetwork.MaxRemoteCallPayloadSize`, по умолчанию 1 МиБ), ещё до копирования тела. Затем `ValidateInboundRemoteCallData` в `Source/Server/ClientDataValidation.cpp` проверяет объявленную форму и число элементов коллекций, и только после этого захватывается игрок и вызывается обработчик; декодер AngelScript повторяет проверку размера для каждого входящего вызова (`AngelScriptRemoteCalls.cpp`). Вызов, не прошедший эти проверки, до обработчика не доходит. Корректно сформированный запрос с поддельным идентификатором — вопрос предметной области, и обработчик отвечает на него отказом ([авторитет и обработка ошибок](../../reference/scripting/remote-calls.md#авторитет-и-обработка-ошибок)).

Задание игрока обрабатывает сообщения одного соединения по порядку (`ServerEngine::ProcessPlayer`), поэтому его обработчики начинаются в порядке поступления. Обработчик, приостановившийся через `Yield`, возвращает управление в этот цикл, и следующий запрос того же клиента может выполниться раньше, чем он продолжится. Если действию нужна задержка, переносите через неё идентификаторы и после неё повторите шаги 2–5, как на странице [захват нескольких сущностей для одного действия](cover-transactions.md#расширение-cover-и-повторная-проверка-после-приостановки); никогда не списывайте стоимость до приостановки, чтобы проверить после неё.

## Сохранение и миграция

`ActionEnergy`, `DeviceOwnerId` и `DeviceActivations` объявлены `Mutable Persistent`, поэтому сохраняются вместе со своими сущностями. Восстановится ли само устройство, зависит от того, сохраняемы ли оно и карта, в списке которой оно числится ([владение сущностями и сохранение](../../explanation/runtime/server.md#владение-сущностями-и-сохранение)); тест работает на базе данных `Memory` и перезапуск не выполняет. Переименованному свойству нужен `///@ MigrationRule Property <Entity> Rename <Old> <New>` ([миграции данных](../project/data-migrations.md)). Изменение имени, аргументов или пределов удалённого вызова меняет сетевой контракт: клиент и сервер должны быть из одной сборки ресурсов ([совместимость и выпуск](../../reference/scripting/remote-calls.md#совместимость-и-выпуск)).

## Ловушки и антипаттерны

- Принимать идентификатор персонажа, владельца или карты из payload. Вызывающий `Player` — единственная личность, за которую ручается движок ([авторитет и обработка ошибок](../../reference/scripting/remote-calls.md#авторитет-и-обработка-ошибок)).
- Разрешать цель глобальным поиском. `Game.GetItem` найдёт устройство где угодно в мире; `Map.GetItem` на карте самого персонажа делает «досягаемость» частью поиска.
- Списывать стоимость до проверки, которая ещё может отказать. Тогда игрок платит за отказ, либо в коде появляется путь возврата, которым может воспользоваться поддельный запрос; списывайте после последней отказывающей проверки, как на шаге 5.
- Переносить решение через захват или `Yield`. `Game.Sync` может отпустить и заново захватить cover потока, а приостановку не переживает ни один cover ([правило приостановки](../scripting/lifecycle-and-concurrency.md#правило-приостановки)); определяйте и проверяйте заново под новым cover.
- Не указывать `MaxBytes` и `MaxCollectionSize`. Пропущенный предел записывается как `0`, неограниченное структурное значение; остаётся только общий для сервера потолок 1 МиБ ([аргументы и сериализация](../../reference/scripting/remote-calls.md#аргументы-и-сериализация)).
- Собирать отображаемый текст на сервере. На сервере нет `Game.GetText`; отправляйте код или ключ текста, и пусть клиент разрешит его на своём языке ([script API времени выполнения](../content/text-and-localization.md#script-api-времени-выполнения)).
- Проверять обработчик прямым вызовом. AngelScript отклоняет прямой вызов функции `[[ServerRemoteCall]]` при компиляции или загрузке модуля (`DIRECT_CALL_BLOCKING_ATTRIBUTES` в `AngelScriptAttributes.cpp`; исключение составляют только пространства имён из `AngelScript.AttributedFunctionDirectCallAllowedNamespaces`, а песочница их не перечисляет), а Managed C# сообщает о нём как о `FOENTRY001`. Прямой вызов к тому же обошёл бы пределы payload, предоставленный движком `Player` и cover входа — то, чему посвящён рецепт.

## Маршрут проверки

Сценарий `validated-action` в `mechanics-smoke.json` запускает сервер с подконфигурацией `RecipeNetworkTests` и `-Testing.Filter validated_action.`, ждёт маркера готовности `validated_action_waiting_for_client` и затем запускает клиент без графического интерфейса с `RecipeClient`:

```ini
# Server side of tests that need a connected headless client
[SubConfig]
Name = RecipeNetworkTests
Testing.Enabled = True
Testing.TestTimeout = 60000
Server.DbStorage = Memory
```

`RecipeClient` включает `Sandbox.ClientAutomation` с пустым рендерером и без звука. Клиент подключается, попадает на карту песочницы через `Sandbox::EnterSandbox`, и серверный тест отправляет ему одну серию запросов: устройство персонажа на другой карте, устройство без владельца, устройство персонажа в восьми гексах, правильное устройство и правильное устройство повторно, когда энергии уже нет.

```angelscript
    // Energy for exactly one activation, so the repeated valid request must be refused for its cost
    actor.ActionEnergy = ValidatedAction::ActivationCost;
    actor.TestValidTargetId = own.Id;

    ident[] drill = {elsewhere.Id, foreign.Id, far.Id, own.Id, own.Id};
    player.ClientCall.RunDeviceDrill(ctx.Id, drill);
    Game.Log("validated_action_drill_sent");
```

Клиент отправляет пять запросов через сгенерированный вызывающий интерфейс, а затем `DeviceDrillSent`; когда приходит этот последний вызов, тест проверяет, что энергия равна `0` и только у правильного устройства одна активация. Сам вызов с серией объявлен с `MaxBytes 64 MaxCollectionSize 8`. Кроме `[Testing] PASS validated_action.client_requests_are_validated_on_the_server` и `[Testing] COMPLETE status=passed`, runner требует следующие серверные маркеры:

```text
validated_action_refused reason=unknown_target
validated_action_refused reason=not_owner
validated_action_refused reason=too_far
validated_action_accepted device=
validated_action_refused reason=no_energy
```

Всю песочницу запускайте из `Examples/MechanicsSandbox` командой `python validate.py` (Windows) или `python3 validate.py` (Linux); фильтра по сценариям у runner нет. При готовой сборке и собранных ресурсах на Windows из каталога сборки:

```powershell
py -3 "<sandbox>\run_mechanics_smoke.py" .\Binaries\Server-Windows-win64\FOMECH_ServerHeadless.exe .\Binaries\Client-Windows-win64\FOMECH_ClientHeadless.exe "<sandbox>\FOnlineMechanicsSandbox.fomain"
```

Проверка в среде выполнения: сценарий прошёл на хосте Windows с настоящим клиентом без графического интерфейса, подключённым к серверу; сервер записал в журнал четыре отказа с причинами в порядке серии и одно принятие, а клиент — пять ответов, из них один `accepted=true`. Не проверено: запуск на Linux, backend Managed C#, отказы `no_actor`, `actor_not_on_map` и `actor_moved`, слишком большие или искажённые payload (сгенерированный вызывающий интерфейс не может их создать), одновременные запросы нескольких клиентов и сохранение через перезапуск.

## Managed C#

Статус: `shape`. `Managed/Recipes/ValidatedAction.cs` объявляет те же строки `///@` и повторяет те же шаги, но ни одна линия движка его не компилирует, поэтому ни компилятор, ни анализаторы его не проверяли.

```csharp
        // The entry cover holds the player and its critter; map items need the map, and Lock replaces the set
        if (!await Sync.Lock(player, map)) {
            return;
        }

        // Other work may run while the cover changes hands, so the actor and its map are derived again
        actor = player.GetControlledCritter();
        if (actor == null || actor.MapId != map.Id) {
            Refuse(player, deviceId, "actor_moved");
            return;
        }
```

- Обработчик объявлен как `[ServerRemoteCall] internal static async Task ActivateDevice([RequiresCover] Player player, ident deviceId)`: он `internal` (`FOENTRY002`) и объявляет `[RequiresCover]` на синхронизированном движком `Player` (`FOSYNC003`); см. [анализаторы контрактов движка](../project/engine-contract-analyzers.md) и [серверную синхронизацию сущностей](../scripting/managed-csharp.md#серверная-синхронизация-сущностей).
- `Sync.Lock` заменяет `Game.Sync`, который `FOSYNC005` запрещает вне помощников `Sync`. Он отвечает, доступны ли ещё сущности, а `FOSYNC010` запрещает отбрасывать этот ответ. Эта форма возвращается без ответа клиенту, когда ответ `false`; отправьте там отказ, если клиент его ждёт.
- `await` завершает cover; `FOSYNC009` сообщает о захваченном значении, использованном после `await`, которое ничто заново не подтвердило.

## Как это расширяет крупный проект

- **Данные вместо констант.** Виды устройств становятся компонентами свойств прототипа или каталогом FixedType, а дальность и стоимость — значениями `///@ Setting`; см. [выбор представления игровых данных](../../explanation/entity-and-property-model/choosing-a-representation.md).
- **Честные гонки и подделка.** Устаревшая цель, сместившийся персонаж или исчерпанный ресурс могут прийти и от честного клиента: откажите, запишите в журнал на низком уровне и сообщите игроку. Корректно сформированное значение, которое честный клиент создать не может, например отрицательное количество или вариант, которого интерфейс не предлагает, проект может считать нарушением протокола, громко записывать в журнал или разрывать соединение.
- **Локализованная обратная связь об отказе.** Отвечайте причиной в виде целочисленного enum или `TextPackKey` вместо `false`, и пусть клиент разрешает её через `Game.GetText`, который есть только на клиенте и в Mapper. Делайте причины грубыми там, где точная выдала бы скрытое состояние.
- **Общие правила.** Чистая функция правила, скомпилированная для обеих сторон, может сделать кнопку неактивной на клиенте; сервер всё равно выполняет её на своих данных.
- **Стоимость и пределы.** Время перезарядки и ограничение частоты запросов — политика проекта. `ServerNetwork.MaxPendingRemoteCalls` и пределы возраста в среде выполнения ограничивают незавершённые обработчики, а не частоту запросов ([мониторинг времени выполнения](../../reference/scripting/remote-calls.md#мониторинг-времени-выполнения-и-отключение)).
- **События проекта.** Вызывайте событие проекта вроде «устройство активировано» после завершения изменения состояния, чтобы подписчики видели согласованный мир.
- **Проверка каталога.** Генерируйте каталог удалённых вызовов проекта из сборки ресурсов и просматривайте его пределы ([создание проектного каталога](../../reference/scripting/remote-calls.md#создание-проектного-каталога)).

## См. также

- [Создание игровых механик](index.md)
- [Захват нескольких сущностей для одного действия](cover-transactions.md)
- [Удалённые вызовы](../../reference/scripting/remote-calls.md)
- [Жизненный цикл и конкурентность скриптов](../scripting/lifecycle-and-concurrency.md#серверная-синхронизация-сущностей)
- [Скрипты Managed C#](../scripting/managed-csharp.md)
- [Текст и локализация](../content/text-and-localization.md)
- [Сгенерированный справочник методов](../../reference/script-api/methods.md)
