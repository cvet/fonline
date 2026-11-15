---
layout: default
title: Патрулирование NPC с наблюдением
locale: ru
document_id: recipe-npc-patrol
permalink: /Docs/ru/how-to/mechanics/npc-patrol.html
recipe_kind: primitive
---

# Патрулирование NPC с наблюдением
<!-- docs-translation: {"document_id":"recipe-npc-patrol","locale":"ru","source_path":"Docs/en/how-to/mechanics/npc-patrol.md","source_sha256":"2e8f08fc53b7a18f4f6beb225653cd2e945b78dfe3d93beda30036f954c0cd37"} -->
Этот рецепт строит стража, который идёт по сохранённому маршруту от точки к точке, останавливается, когда в поле зрения появляется нарушитель, и потом продолжает путь. Ходьба асинхронна: `MoveToHex` возвращается сразу, а сам путь занимает реальное серверное время, поэтому рецепт — цепочка коротких time events, по одному на отрезок, а событие остановки движка толкает цепочку вперёд. Маршрут и его индекс — сохраняемые данные; таймеры и подписки на события — состояние среды выполнения, которое `InitScript` стража пересоздаёт после каждой загрузки.

| Backend | Статус |
| --- | --- |
| AngelScript | `runtime-verified` |
| Managed C# | `shape` |

`runtime-verified` здесь означает, что сценарий `npc-patrol` внутри процесса и двухфазные сценарии перезапуска `npc-patrol-restart-write` и `npc-patrol-restart-verify` прошли на собранной песочнице после сборки ресурсов на хосте Windows. Не проверены: компиляция и запуск Managed C#, исходы пути `TraceFailed`, `GagCritter` и `Deadlock`, путевая точка, занятая другим криттером, правило видимости с ограничением по расстоянию (в песочнице его нет, см. [Наблюдение](#наблюдение)) и путь восстановления серверного снимка.

## Проверенные исходные пути

- `Examples/MechanicsSandbox/Scripts/Recipes/NpcPatrol.fos`
- `Examples/MechanicsSandbox/Scripts/Tests/Test_NpcPatrol.fos`
- `Examples/MechanicsSandbox/Managed/Recipes/NpcPatrol.cs`
- `Examples/MechanicsSandbox/Content/NpcPatrol.fopro`
- `Examples/MechanicsSandbox/FOnlineMechanicsSandbox.fomain`
- `Examples/MechanicsSandbox/mechanics-smoke.json`
- `Source/Scripting/ServerCritterScriptMethods.cpp`
- `Source/Server/Server.cpp`
- `Source/Server/MapManager.cpp`
- `Source/Common/TimeEvents.cpp`
- `BuildTools/validation-project/TestServerExtension.cpp`
- `Docs/en/reference/native-extension/hooks.md`
- `Docs/generated/api.json`

## Задача

Патрулирующий NPC должен идти дальше, не заставляя скрипт ждать, переживать заблокированные путевые точки, реагировать на увиденное и возвращаться после перезапуска. Движок даёт ходьбу по пути, уведомление об остановке и события видимости; цикл, который их связывает, политика сбоев и пауза — это рецепт. Маршруты, скорости, поведение при тревоге и смысл замеченного нарушителя — игровая политика.

## Используемые примитивы движка

| Символ | Сторона | Роль в рецепте |
| --- | --- | --- |
| `script.method.server.Critter.MoveToHex#59785332969dbd15` | сервер | `MoveToHex(mpos hex, int32 cut, int32 speed, callback(bool,Critter,Item) gagCallabck)` запускает один отрезок и возвращает его контекст |
| `script.ref-type.common.MovingContext` | общая | Контекст этого отрезка |
| `script.ref-method.common.MovingContext.IsCompleted` | общая | True сразу для отклонённого или уже завершённого запроса |
| `script.ref-method.common.MovingContext.GetCompleteReason` | общая | Причина такого немедленного завершения |
| `script.method.server.Critter.GetMovingState` | сервер | Результат только что остановившейся ходьбы |
| `script.enum.MovingState` | общая | Итоговый результат или состояние в процессе |
| `script.enum-value.MovingState.Success` | общая | Отрезок дошёл до своего гекса |
| `script.enum-value.MovingState.Stopped` | общая | Ходьбу остановил скрипт или запрос |
| `script.enum-value.MovingState.HexBusy` | общая | Гекс путевой точки занят или недоступен |
| `script.method.server.Critter.StopMoving` | сервер | Прерывает ходьбу, когда страж видит нарушителя |
| `script.event.server.Game.OnCritterStopMoving` | сервер | Вызывается после остановки активной ходьбы, по любой причине |
| `script.event.server.Critter.OnCritterAppeared` | сервер | Событие криттера: другой криттер попал в его поле зрения |
| `script.event.server.Critter.OnCritterDisappeared` | сервер | Другой криттер покинул поле зрения |
| `script.property.Critter.LookDistance` | общая | Дальность обзора, которую может применять правило видимости проекта |
| `script.property.Critter.InitScript` | сервер | Называет функцию, которая выполняется для нового и для каждого восстановленного стража |
| `script.method.server.Entity.StartTimeEvent#e59548a665ddb838` | сервер | `StartTimeEvent(timespan delay, callback(void,Entity) func)` планирует следующий отрезок |
| `script.method.server.Entity.CountTimeEvent#7ed3553b71048a0d` | сервер | `CountTimeEvent(callback(void,Entity) func, bool excludeExecuting = false)` держит один ожидающий отрезок |
| `script.method.server.Entity.StopTimeEvent#d792082ed73a8fc8` | сервер | Убирает ожидающий отрезок, когда страж останавливается |
| `script.method.server.Game.Sync#b42b6f6d6b684d2a` | сервер | `Sync(Entity entity1, Entity entity2)` покрывает стража и его карту |
| `script.method.server.Critter.GetMap` | сервер | Карта, по которой идёт отрезок |

`[[TimeEvent]]`, `[[Async]]`, `[[Event]]`, `[[ModuleInit]]`, `Critter.OnCritterAppeared.Subscribe` и `Entity.IsDestroyed` — встроенные средства backend, а четыре строки `///@ Property` — метаданные проекта.

## Минимальный пример

В `Examples/MechanicsSandbox/`: `Scripts/Recipes/NpcPatrol.fos` — сам рецепт, `Scripts/Tests/Test_NpcPatrol.fos` содержит его тесты, `Content/NpcPatrol.fopro` — стража, нарушителя и блокирующий ящик, а `Managed/Recipes/NpcPatrol.cs` — форму Managed C#. Команды приведены в разделе [Маршрут проверки](#маршрут-проверки).

## Пошагово

Маршрут и индекс сохраняемые. Счётчик сбоев и флаг паузы — `Mutable`, но не `Persistent`: после перезапуска оба начинают с нуля.

```angelscript
///@ Property Critter Server mpos[] PatrolRoute Mutable Persistent
///@ Property Critter Server int32 PatrolIndex Mutable Persistent
///@ Property Critter Server int32 PatrolFailures Mutable
///@ Property Critter Server bool PatrolPaused Mutable
```

Прототип называет `InitScript`, который выполняется для нового стража (`firstTime == true`) и для каждого восстановленного (`firstTime == false`; [callbacks `InitScript` сущности](../scripting/lifecycle-and-concurrency.md#callbacks-initscript-сущности)). `LookDistance` разобран в разделе [Наблюдение](#наблюдение):

```ini
LookDistance = 12
InitScript = NpcPatrol::InitPatroller
```

Подписки бывают двух видов. Событие остановки глобальное, поэтому один обработчик обслуживает всех патрульных, и `InitNpcPatrol` подписывает его один раз при инициализации модуля (`Game.OnCritterStopMoving.Subscribe(OnCritterStopMoving)`). События появления принадлежат каждому криттеру, а подписки сущности не входят в её сохраняемое состояние, поэтому `InitScript` подписывается заново при каждой загрузке и, если маршрут сохранён, планирует следующий отрезок:

```angelscript
// The prototype names this function in its InitScript: it runs for a new patroller and again for every restored one
void InitPatroller(Critter cr, bool firstTime)
{
    // Subscriptions and walking are runtime state; the saved route is not, so a restored patroller starts over from it
    cr.OnCritterAppeared.Subscribe(OnSeen);
    cr.OnCritterDisappeared.Subscribe(OnLost);

    if (cr.PatrolRoute.length() != 0) {
        ScheduleLeg(cr, LegPauseMs);
    }
}
```

`StartPatrol` записывает маршрут, сбрасывает индекс, счётчик сбоев и флаг паузы и планирует первый отрезок из time event, а не из вызывающего кода. `ScheduleLeg` держит не больше одного ожидающего отрезка. Однократное событие остаётся в списке сущности, помеченное как выполняющееся, пока его callback не завершится и `TimeEventManager::PostFireTimeEvent` не удалит его (`Source/Common/TimeEvents.cpp`), поэтому отрезок, планирующий своего преемника изнутри себя, должен передавать `excludeExecuting = true`; со значением по умолчанию он считает сам себя, и преемник никогда не планируется:

```angelscript
// At most one waiting leg per critter; a running leg schedules its successor, so it does not count itself
void ScheduleLeg(Critter cr, int32 delayMs)
{
    if (cr.CountTimeEvent(PatrolStep, true) == 0) {
        cr.StartTimeEvent(Milliseconds(delayMs), PatrolStep);
    }
}
```

Один отрезок — один `MoveToHex`. Time event криттера покрывает только его криттера, а `MoveToHex` проверяет доступ и к карте криттера (`StartCritterMoveToHex` в `Source/Scripting/ServerCritterScriptMethods.cpp`), поэтому отрезок — `[[Async]]` и покрывает обоих. Отклонённый или уже завершённый запрос завершается сразу и никогда не вызывает событие остановки, поэтому отрезок читает его результат сам; ходьба, которая идёт, заканчивается событием остановки:

```angelscript
// A critter time event covers only its critter; walking also changes the map, so the map joins the cover
[[TimeEvent]] [[Async]]
void PatrolStep(Critter cr)
{
    Map? map = cr.GetMap();
    if (map == null || cr.PatrolPaused || cr.PatrolRoute.length() == 0) {
        return;
    }

    Game.Sync(cr, map);

    // Other work may run while the cover changes hands
    if (cr.IsDestroyed || cr.MapId != map.Id || cr.PatrolPaused) {
        return;
    }

    mpos[] route = cr.PatrolRoute;
    MovingContext move = cr.MoveToHex(route[cr.PatrolIndex % route.length()], 0, WalkSpeed, NoGagPassage);

    // A leg that is walking ends in OnCritterStopMoving; a refused one never raised that event, so its result is read here
    if (!move.IsCompleted()) {
        cr.PatrolFailures = 0;
        return;
    }

    MovingState state = move.GetCompleteReason();
    if (state == MovingState::Success) {
        Advance(cr);
        ScheduleLeg(cr, LegPauseMs);
        return;
    }

    RetryOrSkip(cr, state);
}
```

Обработчик остановки читает состояние только что закончившейся ходьбы. Маршрут продвигает только `Success`. `Stopped` — решение того, кто остановил ходьбу, поэтому обработчик его не трогает; любое другое состояние — неудавшийся отрезок. Ходьба, закончившаяся сама, вызывает это событие с `Success`, а немедленно завершённая не вызывает его никогда, поэтому индекс не может продвинуться дважды за один отрезок:

```angelscript
// The engine reports every stop, including the ones this script causes; only a finished walk moves the patrol on
[[Event]]
void OnCritterStopMoving(Critter cr)
{
    if (cr.PatrolRoute.length() == 0 || cr.PatrolPaused) {
        return;
    }

    MovingState state = cr.GetMovingState();
    if (state == MovingState::Success) {
        Advance(cr);
        ScheduleLeg(cr, LegPauseMs);
        return;
    }

    // A script stop is a decision of its caller; any other reason is a failed leg
    if (state != MovingState::Stopped) {
        RetryOrSkip(cr, state);
    }
}
```

Неудавшийся отрезок повторяется каждые `RetryMs`, а путевая точка пропускается после `MaxFailures` неудач подряд, поэтому один заблокированный гекс не может задержать патруль навсегда (`RetryOrSkip` в том же файле). Тест ставит блокирующий ящик на вторую путевую точку: патруль доходит до первой, пропускает вторую после трёх отказов и достигает третьей, а событие остановки вызывают только два завершённых пути.

### Наблюдение

`OnCritterAppeared` наблюдателя выполняется, когда другой криттер попадает в его поле зрения. Первый аргумент события сущности — её владелец. Пауза записывается первой, поэтому событие остановки, которое вызывает `StopMoving` с состоянием `Stopped`, находит флаг и не трогает патруль; затем таймер его возобновляет:

```angelscript
// Runs on the watcher: a critter entered its view; the event owner is the first argument
[[Event]]
void OnSeen(Critter cr, Critter seen)
{
    if (seen.ProtoId != IntruderPid) {
        return;
    }

    // The pause comes first, so the stop event that follows sees it and leaves the patrol alone
    cr.PatrolPaused = true;
    cr.StopMoving();
    cr.StopTimeEvent(PatrolStep);
    Game.Log("npc_patrol_saw critter=" + cr.Id.value + " seen=" + seen.Id.value);

    if (cr.CountTimeEvent(ResumePatrol) == 0) {
        cr.StartTimeEvent(Milliseconds(SightPauseMs), ResumePatrol);
    }
}
```

`OnLost` только пишет в журнал, а `ResumePatrol` снимает флаг и планирует отрезок. Что значит «в поле зрения», не решает собственный код движка. `MapManager::IsCritterSeeCritter` вызывает нативный hook `CheckCritterVisibilityHook`, который по умолчанию возвращает полную видимость ([справочник hook](../../reference/native-extension/hooks.md#checkcrittervisibilityhook)), поэтому без hook проекта каждый криттер на карте видит каждого другого, а `LookDistance` для этих событий ничего не меняет. Проект, которому нужна дальность, применяет `LookDistance` в своём hook, как это делает hook проверочного проекта через `GetDistance(...) <= LookDistance`; изменение свойства тогда заново запускает расчёт видимости (`ServerEngine::OnSetCritterLookDistance`). В песочнице hook нет, поэтому её `LookDistance = 12` ни на что не влияет, а тест ставит нарушителя в трёх гексах: он проверяет обработку событий, а не дальность.

## Полномочия и синхронизация

- Патруль — серверная логика NPC. Клиенты видят ходьбу через обычную рассылку движения и ничем не владеют.
- Time event криттера покрывает этого криттера. `PatrolStep` добавляет карту одним `Game.Sync` и после него проверяет всё заново, потому что захват cover даёт выполняться другим заданиям ([cover не переживает приостановку](../scripting/lifecycle-and-concurrency.md#cover-не-переживает-приостановку)).
- Обработчики остановки и появления пишут только свойства криттера-владельца и запускают или останавливают его time events, поэтому им не нужен cover сверх того, с которым их вызвал движок. Все поиски пути остаются в `PatrolStep`, и уведомление получается дешёвым.
- Каждый отрезок планируется через `ScheduleLeg` — из обработчика остановки, из `PatrolStep` и из `ResumePatrol`, — и он держит число ожидающих отрезков равным одному.

## Сохранение и миграция

| Переживает перезапуск | Не переживает |
| --- | --- |
| `PatrolRoute` и `PatrolIndex`, сохраняемые свойства стража | Time events `PatrolStep` и `ResumePatrol`: ни одна загрузка из базы данных их не восстанавливает |
| Сам страж, если его локация сохраняемая | Подписки стража на события и контекст движения идущего отрезка |
| | `PatrolFailures` и `PatrolPaused`, которые не сохраняемые |

После перезапуска `InitScript` выполняется с `firstTime == false`, подписывается заново и планирует отрезок от сохранённого индекса. Сценарии перезапуска проходят часть маршрута, останавливают сервер, запускают его на той же базе данных JSON и требуют: в маршруте три точки, индекс не откатился, существует ходьба или ожидающий отрезок, а нарушитель, поставленный позже, останавливает восстановленного стража, что доказывает подписку. Отрезок, который шёл в момент остановки, продолжается оттуда, где стоит страж, к сохранённому индексу, потому что индекс продвигается только по прибытии.

Путь восстановления серверного снимка возвращает захваченные time events после событий инициализации ([то же ограничение, что и в рецепте временного эффекта](timed-effect.md#сохранение-и-миграция)), поэтому захваченный `PatrolStep` может оказаться рядом с тем, который запланировала инициализация. В песочнице этот путь не запускался. Переименование `PatrolRoute` следует разделу [Объявляйте правило для каждого переименования, удаления и переноса](../project/data-migrations.md#объявляйте-правило-для-каждого-переименования-удаления-и-переноса).

## Ловушки и антипаттерны

- Подсчёт собственного таймера callback. `CountTimeEvent` считает выполняющийся однократный callback, если `excludeExecuting` не true, поэтому отрезок, планирующий преемника с защитой по умолчанию, ничего не планирует, и патруль молча заканчивается.
- Трактовка каждого `OnCritterStopMoving` как прибытия. Оно вызывается и при остановке скриптом `StopMoving` (`Stopped`), и при неудавшейся ходьбе, а отклонённый `MoveToHex` вызывает его, только если отказ прервал идущую ходьбу, и с состоянием `Stopped`; отказ у стоящего криттера не вызывает ничего. Читайте `GetMovingState` и сами читайте немедленный результат `MoveToHex`.
- Вызов `MoveToHex` из time event без cover карты. Собственного cover криттера недостаточно; вызов завершается ошибкой `Entity access without sync` для карты.
- Предположение, что у восстановленного стража остались подписки или таймеры. Подписки криттера и time events — состояние среды выполнения; возвращает их только `InitScript` (или `Game.OnCritterInit` с `firstTime == false`).
- Предположение, что `LookDistance` ограничивает наблюдение. Hook видимости по умолчанию возвращает полную видимость; дальность существует только там, где её применяет hook проекта. Запросы карты вроде `Map.GetCrittersWhoSeeHex` используют `LookDistance` напрямую.
- Предположение о том, что происходит, когда на путевой точке стоит другой криттер. Песочница наблюдала, что путь к такому гексу заканчивался с `Success`, а рецепт ничего об этом не утверждает; тесты блокируют путевые точки предметом.

## Маршрут проверки

Запускайте все сценарии песочницы из `Examples/MechanicsSandbox` командой `python validate.py` в Windows (PowerShell) или `python3 validate.py` в Linux (bash); предварительные условия перечислены в её `README.md`. Сценарии этого рецепта среди прочего требуют:

```text
[Testing] PASS npc_patrol.blocked_waypoint_is_retried_and_skipped_without_a_stop_event
[Testing] PASS npc_patrol.script_stop_is_not_continued_by_the_stop_event
[Testing] PASS npc_patrol_restart_verify.restored_guard_walks_again_and_watches_again
npc_patrol_leg_failed critter=
npc_patrol_saw critter=
```

Ходьба занимает реальное серверное время, поэтому каждый тест запускает патруль, а повторяющийся опрос оценивает его состояние. Пара перезапуска использует sub-config `RecipeRestart`; раннер подменяет путь к его базе данных каталогом, которым управляет сам:

```ini
# Two-phase restart tests; the runner passes its own JSON directory with -Server.DbStorage
[SubConfig]
Name = RecipeRestart
Testing.Enabled = True
ServerNetwork.DisableNetworking = True
Server.DbStorage = JSON RecipeRestartDb
```

Чтобы запустить набор внутри процесса на готовой сборке ресурсов, из каталога сборки (PowerShell; в Linux используйте `./Binaries/Server-Linux-x64/FOMECH_ServerHeadless`):

```powershell
& ".\Binaries\Server-Windows-win64\FOMECH_ServerHeadless.exe" -ApplyConfig "<sandbox>\FOnlineMechanicsSandbox.fomain" -ApplySubConfig RecipeTests -Testing.Filter "npc_patrol."
```

Для пары перезапуска используйте `-ApplySubConfig RecipeRestart -Server.DbStorage "JSON <empty-directory>"` с фильтрами `npc_patrol_restart_write.` и затем `npc_patrol_restart_verify.` на одном каталоге.

## Managed C#

Статус `shape`: тест движка разбирает `Managed/Recipes/NpcPatrol.cs` и сравнивает его строки `///@` с модулем AngelScript; ни одна линия движка не компилирует и не запускает его. Поток тот же. Отрезок — time event `async Task`, `internal`, потому что его вызывает движок, а cover — ожидаемый вспомогательный вызов, сообщающий об исчезнувшей сущности, поэтому вызов становится условием:

```csharp
        if (!await Sync.Lock(cr, map)) {
            return;
        }
```

## Как это расширяет крупный проект

- **Авторские маршруты.** Храните на страже id маршрута и индекс, а точки держите в авторском каталоге, например в строке [каталога FixedType](../../explanation/entity-and-property-model/choosing-a-representation.md#строка-каталога-fixedtype): маршрут редактируется как данные. Свойство `mpos[]` подходит для маршрутов, которые игра строит во время работы.
- **Политика тревоги.** Правила для нарушителей, состояния тревоги, погоня и то, что запускает замеченный нарушитель, заменяют `OnSeen`; порядок «сначала пауза» и единый планировщик остаются.
- **Настоящее правило видимости.** Реализуйте `CheckCritterVisibilityHook` в нативном расширении проекта ([нативные расширения](../native-extensions.md)) и решите, ограничивает ли `LookDistance` обзор, как это делает проверочный проект.
- **Событие проекта с производителем.** Вызывайте одно событие проекта там, где решается, что нарушитель замечен: так другие модули реагируют без опроса.
- **Что остаётся в игровом проекте.** Маршруты, скорости, паузы, пределы повторов, поведение при тревоге и правила групп. Движок даёт ходьбу, уведомление об остановке и события видимости; цикл и hook восстановления — код проекта.

## См. также

- [Создание игровых механик](index.md)
- [Временный эффект, переживающий перезапуск](timed-effect.md)
- [Захват нескольких сущностей для одного действия](cover-transactions.md)
- [Жизненный цикл и конкурентность скриптов: callbacks `InitScript` сущности](../scripting/lifecycle-and-concurrency.md#callbacks-initscript-сущности)
- [Карты, движение и геометрия: контексты движения](../../explanation/maps-and-movement.md#контексты-движения)
- [Серверная среда выполнения: отклонённый запрос движения завершает прерванный маршрут](../../explanation/runtime/server.md#отклонённый-запрос-движения-завершает-прерванный-маршрут)
- [Хуки движка](../../reference/native-extension/hooks.md#checkcrittervisibilityhook)
- [Скрипты Managed C#](../scripting/managed-csharp.md)
- [Шаблон обвязки скриптовых тестов](../project/script-test-harness.md)
