---
layout: default
title: Временный эффект, переживающий перезапуск
locale: ru
document_id: recipe-timed-effect
permalink: /Docs/ru/how-to/mechanics/timed-effect.html
recipe_kind: primitive
---

# Временный эффект, переживающий перезапуск
<!-- docs-translation: {"document_id":"recipe-timed-effect","locale":"ru","source_path":"Docs/en/how-to/mechanics/timed-effect.md","source_sha256":"9475520d6ce5e947cf1d45d44895b0f73ed74b8c4f8804c3bd885b1f70c183d6"} -->
Этот рецепт строит усиление, которое заканчивается в свой срок, даже если между началом и концом сервер перезапускается или криттер выгружается и загружается снова. Очевидный вариант — time event, запущенный в момент начала эффекта, — молча теряет таймер при каждой перезагрузке; песочница хранит этот вариант как отрицательный контроль и доказывает потерю. Рабочий вариант хранит срок окончания в сохраняемом свойстве, заново запускает таймер из hook восстановления и допускает только один таймер.

| Backend | Статус |
| --- | --- |
| AngelScript | `runtime-verified` |
| Managed C# | `shape` |

`runtime-verified` здесь означает, что сценарий `timed-effect` внутри процесса и двухфазные сценарии перезапуска `timed-effect-restart-write` и `timed-effect-restart-verify` на одном каталоге базы данных JSON прошли на собранной песочнице после сборки ресурсов на хосте Windows, а наивный вариант ожидаемо не сработал. Песочница не проверяет компиляцию и запуск Managed C#. Порядок восстановления серверного снимка закреплён юнит-тестом Engine, а не песочницей.

## Проверенные исходные пути

- `Examples/MechanicsSandbox/Scripts/Recipes/TimedEffect.fos`
- `Examples/MechanicsSandbox/Scripts/Tests/Test_TimedEffect.fos`
- `Examples/MechanicsSandbox/Managed/Recipes/TimedEffect.cs`
- `Examples/MechanicsSandbox/Content/TimedEffect.fopro`
- `Examples/MechanicsSandbox/mechanics-smoke.json`
- `Source/Common/TimeEvents.cpp`
- `Source/Server/Server.cpp`
- `Source/Server/Server.h`
- `Source/Server/EntityManager.cpp`
- `Docs/generated/api.json`

## Задача

Усиление, перезарядка или временный штраф длятся фиксированное время. Игрок ожидает, что эффект закончится вовремя, а не останется навсегда из-за перезапуска сервера, пока он действовал. Один time event такого обещания не держит: time events — состояние среды выполнения, и ни восстановление мира при старте, ни `Game.LoadCritter` их не восстанавливают ([time events после перезагрузки из базы данных](../scripting/lifecycle-and-concurrency.md#time-events-после-перезагрузки-из-базы-данных)). Длительность, сила усиления и правила их сложения — игровая политика, она остаётся в игровом проекте.

## Используемые примитивы движка

| Символ | Сторона | Роль в рецепте |
| --- | --- | --- |
| `script.event.server.Game.OnCritterInit` | сервер | Hook повторного запуска: `firstTime == false` на обоих путях восстановления |
| `script.method.server.Entity.StartTimeEvent#1e937a7976df63ac` | сервер | `StartTimeEvent(timespan delay, callback(void,Entity,TimeEventContext) func)` запускает таймер рецепта |
| `script.method.server.Entity.StartTimeEvent#e59548a665ddb838` | сервер | `StartTimeEvent(timespan delay, callback(void,Entity) func)` запускает таймер наивного контроля |
| `script.method.server.Entity.CountTimeEvent#417e7527b8152349` | сервер | Проверка; по умолчанию `excludeExecuting = false` учитывает и выполняющийся callback |
| `script.ref-type.common.TimeEventContext` | общая | Срабатывающее событие, которое передаётся в callback |
| `script.ref-method.common.TimeEventContext.Repeat` | общая | Снова ждёт срока, который сдвинулся позже |
| `script.ref-method.common.TimeEventContext.Stop` | общая | Удаляет событие после завершения callback |
| `script.property.Game.SynchronizedTime` | общая | Часы срока окончания; сервер сохраняет и продолжает их |
| `script.method.server.Entity.MakePersistent` | сервер | Даёт тестовому криттеру собственную запись |
| `script.method.server.Game.UnloadCritter` | сервер | Убирает криттера и очищает его time events |
| `script.method.server.Game.LoadCritter#f50433829d33781c` | сервер | Восстанавливает его и выполняет `OnCritterInit(cr, false)` |

`[[TimeEvent]]`, `[[Event]]`, `[[PropertySetter]]`, `Game.AddPropertySetter`, `ZERO_SYNCTIME` и `ZERO_TIMESPAN` — встроенные средства backend, а строка `///@ Property` — метаданные проекта. Намеренно не используется `Game.OnCritterLoad`: он никогда не выполняется для криттеров, восстановленных вместе с картами при старте.

## Минимальный пример

В `Examples/MechanicsSandbox/`: `Scripts/Recipes/TimedEffect.fos` — сам рецепт, `Scripts/Tests/Test_TimedEffect.fos` содержит его тесты и наивный вариант, `Content/TimedEffect.fopro` — прототип `TimedEffect_Npc`, а `Managed/Recipes/TimedEffect.cs` — форму Managed C#. Команды приведены в разделе [Маршрут проверки](#маршрут-проверки).

## Пошагово

Храните абсолютный срок окончания, а не оставшуюся длительность. `synctime` сравнивается с `Game.SynchronizedTime`; `PublicSync` позволяет клиентам, которые видят криттера, показывать оставшееся время без собственного таймера:

```angelscript
///@ Property Critter Common synctime BoostEndTime Mutable Persistent PublicSync

#if SERVER

[[ModuleInit]]
void InitTimedEffect()
{
    Game.OnCritterInit.Subscribe(OnCritterInit);
}
```

Прототипу для эффекта ничего не нужно; срок окончания начинается со значения по умолчанию из объявления, `ZERO_SYNCTIME`, которое означает отсутствие усиления:

```ini
# A character that carries a timed boost
[ProtoCritter]
$Name = TimedEffect_Npc
$Text engl = "Boosted runner"
$Text russ = "Ускоренный бегун"
```

Начало и продление усиления — одна операция: записать срок окончания, затем убедиться, что таймер существует. Срок окончания — источник истины; таймер только доставляет истечение:

```angelscript
// Starts the boost or moves its deadline; an armed timer reads the new deadline when it fires
void ApplyBoost(Critter cr, timespan duration)
{
    cr.BoostEndTime = Game.SynchronizedTime + duration;
    ArmBoostTimer(cr);
}

bool IsBoosted(Critter cr)
{
    return cr.BoostEndTime != ZERO_SYNCTIME;
}
```

Запускайте таймер заново при восстановлении в `Game.OnCritterInit` с `firstTime == false`. Песочница считает события: криттер карты, восстановленный при старте сервера, получает `OnCritterInit(cr, false)` один раз и `OnCritterLoad` ни разу, а `Game.LoadCritter` вызывает каждое событие по одному разу. Поэтому `OnCritterLoad` пропустил бы каждый перезапуск:

```angelscript
// Time events are not restored from the database: a restored critter re-arms from its saved deadline
[[Event]]
void OnCritterInit(Critter cr, bool firstTime)
{
    if (!firstTime && IsBoosted(cr)) {
        ArmBoostTimer(cr);
    }
}
```

Защищайте каждый запуск проверкой `CountTimeEvent`, чтобы обновление, повторная инициализация или любой более поздний путь запуска не мог запланировать второй таймер. Для уже прошедшего срока запускается нулевая задержка, которую движок поднимает до своего минимума в 1 миллисекунду (`TimeEventManager::StartTimeEventWithId` в `Source/Common/TimeEvents.cpp`), поэтому усиление истекает при следующей диспетчеризации:

```angelscript
// At most one timer per critter, whichever path asks first
void ArmBoostTimer(Critter cr)
{
    if (cr.CountTimeEvent(OnBoostTimer) != 0) {
        return;
    }

    cr.StartTimeEvent(RemainingTime(cr), OnBoostTimer);
}

timespan RemainingTime(Critter cr)
{
    timespan remaining = cr.BoostEndTime - Game.SynchronizedTime;
    return remaining > ZERO_TIMESPAN ? remaining : ZERO_TIMESPAN;
}
```

Callback заново читает срок окончания. `Repeat` оставляет то же событие ждать продлённого срока; он же превращает однократное событие в повторяющееся, поэтому каждый завершающий путь вызывает `Stop` ([завершение TimeEventContext](../scripting/lifecycle-and-concurrency.md#завершение-time-event)):

```angelscript
[[TimeEvent]]
void OnBoostTimer(Critter cr, TimeEventContext timer)
{
    // A second timer, for example from a start path that skipped the guard, finds the boost over and stops
    if (!IsBoosted(cr)) {
        timer.Stop();
        return;
    }

    // A deadline moved later keeps the same timer waiting for the rest of the time
    timespan remaining = RemainingTime(cr);
    if (remaining > ZERO_TIMESPAN) {
        timer.Repeat(remaining);
        return;
    }

    // Repeat made the event periodic, so the last firing stops it explicitly
    timer.Stop();
    cr.BoostEndTime = ZERO_SYNCTIME;
    Game.Log("timed_effect_boost_expired critter=" + cr.Id.value);
}
```

### Наивный вариант не работает

`Scripts/Tests/Test_TimedEffect.fos` хранит форму, которая выглядит правильной и неверна. Её таймер запускается только в callback после записи свойства, а загрузка из базы данных ничего не присваивает, поэтому callback больше не выполняется:

```angelscript
// Naive variant: the timer starts only when the deadline is assigned, and a database load assigns nothing
[[PropertySetter]]
void OnNaiveBoostEndTimeChanged(Critter cr)
{
    if (cr.NaiveBoostEndTime != ZERO_SYNCTIME) {
        cr.StartTimeEvent(cr.NaiveBoostEndTime - Game.SynchronizedTime, OnNaiveBoostExpired);
    }
}

[[TimeEvent]]
void OnNaiveBoostExpired(Critter cr)
{
    cr.NaiveBoostEndTime = ZERO_SYNCTIME;
}
```

Один и тот же криттер проходит выгрузку и загрузку с обоими вариантами; оба срока возвращаются, но возвращается только таймер рецепта:

```angelscript
    Critter restored = Game.LoadCritter(crId, false, map);
    Testing::Expect(ctx, TimedEffect::IsBoosted(restored), "The deadline comes back from the database");
    Testing::ExpectEq(ctx, restored.CountTimeEvent(TimedEffect::OnBoostTimer), 1, "OnCritterInit re-armed the recipe timer");
    Testing::Expect(ctx, restored.NaiveBoostEndTime != ZERO_SYNCTIME, "The naive deadline comes back too");
    Testing::ExpectEq(ctx, restored.CountTimeEvent(OnNaiveBoostExpired), 0, "Nothing re-armed the naive timer");
```

После настоящего перезапуска наивное усиление остаётся активным после своего срока, а усиление рецепта истекает. У наивного обработчика есть второй дефект, видимый в коде: при каждом новом сроке он без проверки запускает ещё один таймер, и более ранний таймер затем досрочно снимает продлённое усиление.

## Полномочия и синхронизация

- Срок окончания и таймер принадлежат серверу. Клиенты читают `BoostEndTime` через `PublicSync` и сравнивают его со своей копией `Game.SynchronizedTime`; игровые таймеры они не запускают.
- Time event принадлежит своей сущности. Серверное задание time event берёт cover этой сущности перед вызовом callback (`ServerEngine::TimeEventJob` в `Source/Server/Server.cpp`), поэтому `OnBoostTimer` может затрагивать `cr`; для другой сущности нужен полный cover `Game.Sync`.
- `OnCritterInit`, как и любой callback инициализации, получает управление с покрытым cover криттером ([callbacks `InitScript` сущности](../scripting/lifecycle-and-concurrency.md#callbacks-initscript-сущности)).
- `StartTimeEvent` отклоняет уничтоженные и уничтожаемые сущности ([события и временные события](../../explanation/entity-and-property-model/index.md#события-и-временные-события)), поэтому запуск из пути уничтожения или выгрузки — ошибка, а не отложенный старт.

## Сохранение и миграция

| Переживает перезапуск | Не переживает |
| --- | --- |
| `BoostEndTime`, сохраняемое свойство сохраняемого криттера | Time event: остановка сервера, `Game.UnloadCritter` и уничтожение очищают его, и ни одна загрузка из базы данных его не восстанавливает |
| `Game.SynchronizedTime`: сервер сохраняет его и продолжает при старте, поэтому простой не расходует усиление | Срок окончания, который хранится только в переменной скрипта или во временном свойстве |

Сценарии перезапуска проверяют именно это: после того как фаза 2 стартует на базе данных JSON фазы 1, оставшееся время больше нуля и не больше полной длительности, существует один таймер, и затем усиление истекает. Штатная остановка записывает точное время (`ServerEngine::FlushExactSyncTime` в `Source/Server/Server.cpp`). Во время работы сервер сохраняет часы с опережением `SyncTimePersistLead`, 10 секунд (`Source/Server/Server.h`), поэтому после аварийного завершения часы продолжают ход с опережением до 10 секунд, и срок наступает настолько же раньше; этот случай не тестируется.

Восстановление серверного снимка — единственный путь, который возвращает time events. `ServerEngine::RestoreSnapshotTimeEvents` (`Source/Server/Server.cpp`) перезапускает сохранённые таймеры сущности после загрузки мира и до событий инициализации этой сущности, поэтому проверка `CountTimeEvent` при инициализации видит сохранённый таймер, и повторный запуск ничего не делает; юнит-тест Engine закрепляет ровно один таймер для этого случая. `OnBoostTimer` по-прежнему останавливает второй таймер, не нашедший усиления, поэтому путь запуска, пропустивший проверку, не оставит устаревший таймер. Снимок также записывает callback по имени и сигнатуре и не восстанавливается, если они больше не разрешаются (`TimeEventManager::RestartTimeEvent` в `Source/Common/TimeEvents.cpp`), поэтому переименование `OnBoostTimer` делает сохранённые снимки непригодными. Песочница не выполняет восстановление снимка.

Переименование сохраняемого срока окончания следует разделу [Объявляйте правило для каждого переименования, удаления и переноса](../project/data-migrations.md#объявляйте-правило-для-каждого-переименования-удаления-и-переноса):

```text
///@ MigrationRule Property Critter Rename BoostEndTime BoostDeadline
```

Смена типа, например с сохранённого `timespan` на срок `synctime`, — это замена, а не переименование: новое имя свойства, метка `Remove` для старого и перенос значения, проверенный на изолированных данных ([Никогда не переиспользуйте выведенную из употребления идентичность](../project/data-migrations.md#никогда-не-переиспользуйте-выведенную-из-употребления-идентичность)).

## Ловушки и антипаттерны

- Запуск таймера только там, где начинается эффект, в обработчике свойства или в самом действии, теряет его при каждой перезагрузке. Отрицательный контроль показывает это после `Game.LoadCritter` и после настоящего перезапуска ([time events после перезагрузки из базы данных](../scripting/lifecycle-and-concurrency.md#time-events-после-перезагрузки-из-базы-данных)).
- Повторный запуск в `Game.OnCritterLoad` пропускает каждое восстановление при старте. Единственное место его вызова — `ServerEngine::LoadCritter` (`Source/Server/Server.cpp`), и песочница насчитывает ноль вызовов для восстановленного криттера карты. Используйте `Game.OnCritterInit` с `firstTime == false`.
- Запуск без проверки `CountTimeEvent` планирует дубликаты, как только второй путь запускает тот же эффект. Восстановление снимка проверку не ломает: оно перезапускает сохранённые таймеры сущностей до событий инициализации (последовательность старта в `Source/Server/Server.cpp` вызывает `RestoreSnapshotTimeEvents` между `EntityMngr.LoadEntities()` и `InitLoadedEntities()`), поэтому callback должен допускать второй таймер, только если другой путь запуска пропускает проверку.
- `Repeat` без `Stop` при последнем срабатывании оставляет периодическое событие. Если `Repeat` однажды изменил интервал, успешный вызов планирует следующее срабатывание (завершение в `TimeEventManager` в `Source/Common/TimeEvents.cpp`, ветка `IsRepeatChanged` и `RepeatDuration`), поэтому callback, который просто возвращается, продолжает срабатывать.
- Хранение оставшейся длительности вместо абсолютного срока требует перезаписи на каждом шаге и расходится после перезапусков. Абсолютный срок `synctime` остаётся верным, потому что сервер продолжает синхронизированное время.
- `ApplyBoost` может сдвинуть срок только позже. Более короткий новый срок сохраняет существующий таймер, который срабатывает в старое время и только тогда завершает усиление; проект, который сокращает эффекты, останавливает таймер через `StopTimeEvent` и запускает его заново.

## Маршрут проверки

Все сценарии песочницы запускаются из `Examples/MechanicsSandbox` командой `python validate.py` в Windows (PowerShell) или `python3 validate.py` в Linux (bash); требования перечислены в её `README.ru.md`. `run_mechanics_smoke.py` сначала удаляет каталог базы данных перезапуска, поэтому фаза 1 всегда начинается с пустой базы. Запуск проходит, когда `BuildTools/gameplay_test_runner.py` сообщает `status=passed` для каждого сценария и завершается с кодом `0`. Сценарии этого рецепта требуют, среди прочего:

```text
[Testing] PASS timed_effect.unload_and_load_rearm_only_the_guarded_timer
[Testing] PASS timed_effect_restart_write.start_boosts_before_restart
[Testing] PASS timed_effect_restart_verify.rearmed_timer_expires_after_restart
[Testing] PASS timed_effect_restart_verify.naive_variant_loses_timer_after_restart
[Testing] COMPLETE status=passed
timed_effect_boost_expired critter=
```

Чтобы выполнить набор тестов внутри процесса на готовой сборке ресурсов, запустите из каталога сборки (PowerShell; в Linux используйте `./Binaries/Server-Linux-x64/FOMECH_ServerHeadless`):

```powershell
& ".\Binaries\Server-Windows-win64\FOMECH_ServerHeadless.exe" -ApplyConfig "<sandbox>\FOnlineMechanicsSandbox.fomain" -ApplySubConfig RecipeTests -Testing.Filter "timed_effect."
```

Для пары перезапуска используйте `-ApplySubConfig RecipeRestart -Server.DbStorage "JSON <empty-directory>"` с фильтрами `timed_effect_restart_write.`, а затем `timed_effect_restart_verify.` на том же каталоге.

## Managed C#

Статус `shape`: тест движка разбирает `Managed/Recipes/TimedEffect.cs` и сравнивает его строку `///@` с модулем AngelScript; ни одна линия движка его не компилирует и не запускает. Поток тот же; `default(synctime)` и `default(timespan)` заменяют нулевые константы AngelScript. Проверка зависит от идентичности callback:

```csharp
    // At most one timer per critter, whichever path asks first; the handler is passed by method group,
    // so counting and starting name the same callback
    private static void ArmBoostTimer([RequiresCover] Critter cr)
    {
        if (cr.CountTimeEvent(OnBoostTimer) != 0) {
            return;
        }

        cr.StartTimeEvent(RemainingTime(cr), OnBoostTimer);
    }
```

Делегат входит в регистрацию time event, и отдельно созданный делегат, например лямбда или замыкание, не идентифицирует существующий callback ([таймеры в Managed C#](../scripting/managed-csharp.md#events-callbacks-timers-и-named-calls)). Time event, привязанный к делегату, к тому же блокирует захват серверного снимка ([согласованные с backend снимки](../../explanation/persistence/index.md#согласованные-с-backend-снимки)). Оставляйте callback статическим методом `[TimeEvent]`, объявленным `internal`, потому что его вызывает движок, и передавайте его как группу методов.

## Как это расширяет крупный проект

- **Каталоги данных.** Длительности, сила и правила сложения многих эффектов — заданные авторами строки, например [каталог FixedType](../../explanation/entity-and-property-model/choosing-a-representation.md#строка-каталога-fixedtype) с ключом по id эффекта. Криттер хранит только то, что меняется: срок окончания каждого активного эффекта, в отдельном свойстве на эффект или в списке RefType, который записывается шагом «прочитать, изменить, присвоить обратно», и один защищённый проверкой таймер, который просыпается к ближайшему сроку.
- **Компоненты.** Если эффект может быть только у некоторых видов криттеров, сгруппируйте его поля в [компонент свойств](../content/prototype-format.md#компоненты-свойств), а не добавляйте их каждому криттеру.
- **События проекта и правило производителя.** Вызывайте одно событие проекта там, где происходит истечение, — в callback таймера: так у каждого объявленного события есть производитель, а другие модули реагируют без опроса. Форма объявления, не входящая в песочницу:

```text
///@ Event Server Game OnBoostExpired(Critter cr)
```

- **Что остаётся в игровом проекте.** Виды эффектов, длительности, сила, правила сочетания эффектов, их тексты и значки. Движок предоставляет сохраняемые свойства, синхронизированное время и time events; hook восстановления, проверка и сверка со сроком — код проекта.

## См. также

- [Создание игровых механик](index.md)
- [Собственное сохраняемое состояние персонажа](persistent-state.md)
- [Жизненный цикл и конкурентность скриптов: time events после перезагрузки из базы данных](../scripting/lifecycle-and-concurrency.md#time-events-после-перезагрузки-из-базы-данных)
- [Серверная среда выполнения: инициализация и серверные задания](../../explanation/runtime/server.md#инициализация-и-серверные-задания)
- [Сохранение данных](../../explanation/persistence/index.md)
- [Миграция сохранённых данных и контента в игровом проекте](../project/data-migrations.md)
- [Скрипты Managed C#](../scripting/managed-csharp.md)
- [Шаблон обвязки скриптовых тестов](../project/script-test-harness.md)
