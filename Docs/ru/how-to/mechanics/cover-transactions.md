---
layout: default
title: Захват нескольких сущностей для одного действия
locale: ru
document_id: mechanics-cover-transactions
permalink: /Docs/ru/how-to/mechanics/cover-transactions.html
---

# Захват нескольких сущностей для одного действия
<!-- docs-translation: {"document_id":"mechanics-cover-transactions","locale":"ru","source_path":"Docs/en/how-to/mechanics/cover-transactions.md","source_sha256":"1bff5ef3d2199f01cf250a611375d21e7ac96d4c63dbd0e2ff8d774fbcde249b"} -->
Действие, которое читает одну сущность и меняет другую, корректно только тогда, когда его проверки и записи выполняются под одним cover всех участвующих сущностей. На этой странице разобрана одна такая транзакция из `Examples/MechanicsSandbox`: персонаж после короткой задержки передаёт предмет из инвентаря персонажу рядом с собой. Она начинается с действующей сущности, расширяет cover на партнёра, отдаёт всё на время задержки и фиксирует результат под одним новым запросом.

| Backend | Статус |
| --- | --- |
| AngelScript | `runtime-verified` |
| Managed C# | `shape` |

## Проверенные исходные пути

- `Examples/MechanicsSandbox/Scripts/Recipes/CoverTransaction.fos`
- `Examples/MechanicsSandbox/Scripts/Tests/Test_CoverTransaction.fos`
- `Examples/MechanicsSandbox/Managed/Recipes/CoverTransaction.cs`
- `Examples/MechanicsSandbox/mechanics-smoke.json`
- `Examples/MechanicsSandbox/FOnlineMechanicsSandbox.fomain`
- `Source/Server/EntitySync.cpp`
- `Source/Scripting/ServerGlobalScriptMethods.cpp`
- `Source/Common/ScriptSystem.h`
- `Source/Tests/Test_ServerEntityLifetime.cpp`
- `Source/Scripting/Managed/CoreScripts/Sync.cs`
- `Source/Scripting/Managed/Analyzers/SyncCoverAnalyzer.cs`
- `Docs/generated/api.json`

## Задача

Отдающий должен всё ещё держать предмет, оба персонажа должны стоять рядом на одной карте, а предмет должен оказаться ровно у одного из них. Каждый факт принадлежит своей сущности, сервер выполняет задания на нескольких рабочих потоках (`Server.WorkerThreads = 2` в песочнице), а передача длится 300 мс. Пока действие ждёт, другое задание может переместить предмет, сдвинуть персонажа или уничтожить его. Дальность и задержка — игровая политика; захват cover, ожидание и повторная проверка — то, чего требует контракт движка.

## Используемые примитивы движка

| Символ | Назначение |
| --- | --- |
| `script.method.server.Game.GetCritter` | Разрешает участника по идентификатору; null, если его нет или он уничтожается |
| `script.method.server.Game.Sync#3a28adb1be02af8c` | Захватывает cover одного отдающего |
| `script.method.server.Game.SyncWiden` | Добавляет получателя к удерживаемому cover |
| `script.method.server.Game.Sync#b42b6f6d6b684d2a` | Захватывает обоих участников одним запросом при фиксации |
| `script.property.Critter.MapId`, `script.method.server.Game.GetDistance#dacea0c00ba4c5f1` | Общее правило соседства |
| `script.method.server.Critter.GetItem#3e4a8f273e418d3b` | Находит предмет в инвентаре отдающего |
| `script.method.server.Game.MoveItem#c26f82c316789816` | Перемещает его получателю; возвращает null, если перемещение его удаляет |
| `script.method.server.Game.DestroyCritter` | Тест уничтожает получателя во время ожидания |

`Yield`, `[[Async]]`, `[[TimeEvent]]`, `Entity.IsDestroyed` и `ZERO_IDENT` — встроенные средства backend.

## Модель cover на практике

Контракт принадлежит страницам [жизненный цикл и конкурентность скриптов](../scripting/lifecycle-and-concurrency.md#серверная-синхронизация-сущностей) и [серверная среда выполнения](../../explanation/runtime/server.md#синхронизация-и-блокировки-сущностей). Для одного действия он сводится к пяти правилам:

- **Один запрос называет всё.** `Game.Sync` заменяет cover перечисленными живыми сущностями и их партнёрами автоматического расширения (игрок и его управляемый криттер); к прежнему cover он ничего не добавляет. Набор захватывается за один проход в порядке адресов, и именно это исключает взаимную блокировку (`SyncContext::AcquireLocks` в `Source/Server/EntitySync.cpp`).
- **Удержание сущности покрывает её содержимое.** Cover отдающего покрывает предметы, которые он несёт; cover карты покрывает криттеров и предметы на ней. Операции, меняющей контейнер, нужен сам контейнер: удаление криттера меняет его карту, поэтому тест захватывает локацию перед `Game.DestroyCritter`.
- **Захват — точка, где работают другие задания.** Если запрошенное ещё не удерживается, контекст отпускает свои блокировки, прежде чем захватить новый набор (`SyncContext::SyncEntities`). При конкуренции поток ждёт, ничего не удерживая, включая внешние контексты, и получает всё обратно до возврата из вызова. Всё прочитанное до вызова нужно прочитать после него заново.
- **Приостановку не переживает ни один cover.** `Yield` завершает выполнение скрипта и его контекст синхронизации; продолжение начинается без cover ([cover не переживает приостановку](../scripting/lifecycle-and-concurrency.md#cover-не-переживает-приостановку)).
- **Уничтоженная сущность пропускается без сообщения.** Перегрузки `Game.Sync` для двух и трёх сущностей и для массива, а также `Game.SyncWiden`, пропускают сущность, уничтоженную после того, как её разрешили; в сгенерированном справочнике они помечены `AllowDestroyedEntityArgs`. У перегрузки для одной сущности `script.method.server.Game.Sync#3a28adb1be02af8c` этого флага нет, поэтому уничтоженный аргумент вместо этого вызывает ошибку на границе скрипта (`ConvertArg` в `Source/Common/ScriptSystem.h`). В любом случае проверяйте `IsDestroyed` под новым cover.

## Разбор транзакции

`HandOverItem` — обычная функция `[[Async]]`: time event, обработчик удалённого вызова или тест вызывают её напрямую с идентификаторами, и каждый вызывающий тоже несёт `[[Async]]`. Через задержку переходят только идентификаторы:

```angelscript
// Ids, not handles, cross the suspension: every step resolves and covers its entities again
[[Async]]
bool HandOverItem(ident giverId, ident receiverId, ident itemId)
{
    if (!PrepareHandOver(giverId, receiverId, itemId)) {
        Game.Log("cover_transaction_refused step=prepare");
        return false;
    }

    // The hand-over takes time; no cover and no decision survive this suspension
    Yield(HandOverDelayMs);

    return CommitHandOver(giverId, receiverId, itemId);
}
```

### Подготовка: cover действующего, затем расширение

```angelscript
[[Async]]
bool PrepareHandOver(ident giverId, ident receiverId, ident itemId)
{
    Critter? giver = Game.GetCritter(giverId);
    if (giver == null) {
        return false;
    }

    // Start from the acting critter, then widen the cover to the partner it names
    Game.Sync(giver);

    Critter? receiver = Game.GetCritter(receiverId);
    if (receiver == null) {
        return false;
    }

    Game.SyncWiden(array<Entity> = {receiver});

    // Widening can hand the held locks over before it returns, so the checks follow it
    return CanHandOver(giver, receiver) && giver.GetItem(itemId) != null;
}
```

Шаг подготовки отказывает рано, до того как игрок будет ждать впустую. Его ответ ничего не решает за фиксацию.

### Фиксация: разрешить заново и захватить одним запросом

```angelscript
[[Async]]
bool CommitHandOver(ident giverId, ident receiverId, ident itemId)
{
    // A later observation: resolve both parties again and cover them with one request
    Critter? giver = Game.GetCritter(giverId);
    Critter? receiver = Game.GetCritter(receiverId);
    if (giver == null || receiver == null) {
        Game.Log("cover_transaction_refused step=commit reason=party_gone");
        return false;
    }

    Game.Sync(giver, receiver);

    if (!CanHandOver(giver, receiver)) {
        Game.Log("cover_transaction_refused step=commit reason=parties_changed");
        return false;
    }

    Item? item = giver.GetItem(itemId);
    if (item == null) {
        Game.Log("cover_transaction_refused step=commit reason=item_gone");
        return false;
    }

    // All checks ran under the same cover as the move, so nothing can change in between
    Item? moved = Game.MoveItem(item, receiver);
    return moved != null;
}
```

### Общее правило обоих шагов

```angelscript
// The rules that both parties must satisfy, read from the currently covered state
bool CanHandOver(Critter giver, Critter receiver)
{
    // Game.Sync skips an entity destroyed after it was resolved instead of failing
    if (giver.IsDestroyed || receiver.IsDestroyed) {
        return false;
    }

    if (giver.MapId == ZERO_IDENT || giver.MapId != receiver.MapId) {
        return false;
    }

    return Game.GetDistance(giver, receiver) <= HandOverDistance;
}
```

## Расширение cover и повторная проверка после приостановки

Что делает `Game.SyncWiden`, по `Source/Server/EntitySync.cpp`:

1. `SyncContext::WidenEntities` собирает один запрос из уже удерживаемых контекстом сущностей, кроме уничтоженных, и новых сущностей и передаёт его в `SyncContext::SyncEntities`.
2. `TryRetainCoveredRequest` оставляет удерживаемые блокировки на месте, только если каждая запрошенная сущность уже удерживается или покрыта через собственную цепочку родителей, как предмет внутри удерживаемого криттера. Этот случай закрепляет `ServerSyncWidenOfCoveredEntityKeepsHeldLocks` в `Source/Tests/Test_ServerEntityLifetime.cpp`.
3. Иначе `SyncEntities` вызывает `ReleaseLocks()`, а затем `AcquireLocks(...)` для всего объединения. Между этими вызовами другое задание может захватить отдающего и изменить его.

При передаче получатель — другой криттер на карте, а не что-то внутри отдающего, поэтому расширение идёт по пути с освобождением. После возврата оба участника захвачены, но отдающий мог сместиться или лишиться предмета с момента возврата `Game.Sync(giver)`. Поэтому `PrepareHandOver` выполняет `CanHandOver` и ищет предмет после расширения, а не между двумя вызовами. Относитесь к расширению как к новому `Game.Sync` объединения: после него всё захвачено, но не обязательно осталось прежним.

После `Yield` у фиксации нет ни cover, ни надёжных дескрипторов. Она разрешает обоих участников по идентификаторам, захватывает их одним `Game.Sync(giver, receiver)`, повторяет каждую проверку, снова разрешает предмет и только потом перемещает его. Все решения и запись выполняются под этим одним cover, поэтому между последней проверкой и перемещением никакое другое задание участников изменить не может.

## Эквивалент на Managed C#

Статус `shape`: `Managed/Recipes/CoverTransaction.cs` повторяет те же шаги, но ни одна линия движка его не компилирует, поэтому описанные ниже вердикты анализатора на этом файле не наблюдались.

```csharp
    private static async Task<bool> PrepareHandOver(ident giverId, ident receiverId, ident itemId)
    {
        Critter? giver = Game.GetCritter(giverId);
        if (giver == null || !await Sync.Lock(giver)) {
            return false;
        }

        // Start from the acting critter, then widen the cover to the partner it names
        Critter? receiver = Game.GetCritter(receiverId);
        if (receiver == null || !await Sync.Widen(receiver)) {
            return false;
        }

        // Widening can hand the held locks over before it returns, so the checks follow it
        return CanHandOver(giver, receiver) && giver.GetItem(itemId) != null;
    }
```

- `Sync.Lock` и `Sync.Widen` из `CoreScripts/Sync.cs` оборачивают `Game.Sync` и `Game.SyncWiden`, которые `FOSYNC005` запрещает вне помощников. Они возвращают `false`, если сущность уничтожена или уничтожается другим потоком, до захвата или после него; `FOSYNC010` запрещает отбрасывать этот ответ, поэтому форма встраивает его в каждое условие.
- `CanHandOver` объявляет `[RequiresCover]` на обоих параметрах; `FOSYNC002` сообщает о вызывающем, который этот cover не захватывает и сам его не объявляет.
- `await ScriptTask.Delay(HandOverDelayMs)` заменяет `Yield`. `await` завершает cover, и `FOSYNC009` сообщает о захваченном значении, использованном после `await`, которое ничто заново не подтвердило; `CommitHandOver` вместо этого разрешает свежие значения.
- Анализатор моделирует `Sync.Widen` как расширение удерживаемого cover (`[CoverEffect(CoverEffectKind.Extend)]`), поэтому `FOSYNC009` не требует заново подтверждать отдающего после него. Это утверждение о cover, а не о состоянии: нативное расширение, описанное выше, всё равно отпускает и заново захватывает блокировки, поэтому проверки по-прежнему идут после расширения. `FOSYNC015` сообщает о расширении, которое называет только сущности, уже покрытые вызывающим.

Правила и их настройка описаны в разделах [серверная синхронизация сущностей](../scripting/managed-csharp.md#серверная-синхронизация-сущностей) и [анализаторы контрактов движка](../project/engine-contract-analyzers.md).

## Ловушки

- Вызывать `Game.Sync(a)`, а затем `Game.Sync(b)`, чтобы добавить `b`. Второй вызов заменяет первый; запрашивайте обе сущности сразу или расширяйте cover.
- Проверять до расширения и действовать после него. Расширение может сначала отпустить удерживаемый cover; проверяйте после него.
- Хранить дескрипторы `Item` или `Critter` через `Yield` и действовать по ним. Переносите идентификаторы и разрешайте их заново.
- Перемещать несколько предметов под разными cover или проверять после первого перемещения. Проверьте всё под одним cover, затем пишите.
- Считать вернувшийся `Game.Sync` доказательством того, что сущности живы. Он пропускает уничтоженные сущности; проверяйте `IsDestroyed`, как это делает `CanHandOver`.
- Вызывать `Game.Sync`, удерживая `Game.Lock()`. Движок выбрасывает исключение, потому что такой порядок замкнул бы цикл блокировок ([`Game.Lock` является отдельным механизмом](../scripting/lifecycle-and-concurrency.md#gamelock-является-отдельным-механизмом)).
- Захватывать только криттеров, когда операция меняет их карту. Уничтожение или перенос криттера меняет его карту; захватывайте карту или её локацию.

## Маршрут проверки

Сценарий `cover-transactions` в `mechanics-smoke.json` использует подконфигурацию `RecipeTests` (база данных в памяти, сеть отключена) и `-Testing.Filter cover_transaction.`. Каждый тест запускает `HandOverItem` из time event:

| Тест | Другое задание во время ожидания 300 мс | Ожидаемый результат |
| --- | --- | --- |
| `hand_over_moves_the_item` | ничего | посылка у получателя |
| `item_taken_during_the_wait_is_not_moved` | через 50 мс передаёт посылку прохожему | фиксация отказывает с `item_gone`; посылка остаётся у прохожего |
| `receiver_destroyed_during_the_wait_is_resolved_again` | через 50 мс уничтожает получателя | фиксация отказывает с `party_gone`; посылка остаётся у отдающего |

Мешающие задания — обычные time events в `Test_CoverTransaction.fos`. `DestroyReceiver` захватывает локацию получателя через `Game.Sync(location)` перед `Game.DestroyCritter`, потому что удаление получателя меняет его карту. Кроме маркера `[Testing] PASS cover_transaction.<test>` для каждого теста и `[Testing] COMPLETE status=passed`, runner требует:

```text
cover_transaction_interference=parcel_moved
cover_transaction_refused step=commit reason=item_gone
cover_transaction_interference=receiver_destroyed
cover_transaction_refused step=commit reason=party_gone
```

Запускайте песочницу из `Examples/MechanicsSandbox` командой `python validate.py` (Windows) или `python3 validate.py` (Linux). Для готовой сборки ресурсов `Examples/MechanicsSandbox/README.ru.md` показывает серверную команду для одного набора тестов; передайте `-Testing.Filter "cover_transaction."`.

Проверка в среде выполнения: три теста прошли на хосте Windows с двумя рабочими потоками, и оба маркера вмешательства и оба отказа появились в журнале сервера. Не проверено: окно освобождения внутри расширения, которое не открывает ни один детерминированный тест (утверждение выше взято из `EntitySync.cpp` и названного модульного теста); отказы шага подготовки; запуск на Linux; backend Managed C#.

## См. также

- [Создание игровых механик](index.md)
- [Проверяемый запрос действия](validated-action.md)
- [Жизненный цикл и конкурентность скриптов](../scripting/lifecycle-and-concurrency.md)
- [Серверная среда выполнения](../../explanation/runtime/server.md#синхронизация-и-блокировки-сущностей)
- [Скрипты Managed C#](../scripting/managed-csharp.md#серверная-синхронизация-сущностей)
- [Сгенерированный справочник методов](../../reference/script-api/methods.md)
