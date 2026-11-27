---
layout: default
title: Триггер, отправляющий персонажа на другую карту
locale: ru
document_id: recipe-map-transition
permalink: /Docs/ru/how-to/mechanics/map-transition.html
recipe_kind: primitive
---

# Триггер, отправляющий персонажа на другую карту
<!-- docs-translation: {"document_id":"recipe-map-transition","locale":"ru","source_path":"Docs/en/how-to/mechanics/map-transition.md","source_sha256":"d28be8227cc2b85dd417e2a6dc23a7c548e9ab8b2c9b04f00342efacf76a13ca"} -->
Этот рецепт строит площадку выхода: статический предмет на полу одной карты, который отправляет каждое наступившее на него существо на гекс другой карты. Площадка несёт пункт назначения как данные, а ключ может её запирать. Перенос нельзя выполнять внутри callback триггера: движок вызывает триггер, когда покрыта только карта, по которой идёт существо; рецепт запускает time event с нулевой задержкой, который покрывает существо, обе карты и локацию назначения и только потом вызывает `TransferToMap`. Песочница хранит вариант с переносом внутри триггера как отрицательный контроль и показывает его сбой.

| Backend | Статус |
| --- | --- |
| AngelScript | `runtime-verified` |
| Managed C# | `shape` |

`runtime-verified` здесь означает, что сценарий `map-transition` внутри процесса, сценарий с настоящим клиентом `map-transition-client` и двухфазные сценарии перезапуска `map-transition-restart-write` и `map-transition-restart-verify` прошли на собранной песочнице после сборки ресурсов на хосте Windows. Не проверены: компиляция и запуск Managed C#, динамические (не статические) предметы-триггеры, переносы на глобальную карту и пункт назначения, выбираемый среди нескольких экземпляров одного прототипа карты.

## Проверенные исходные пути

- `Examples/MechanicsSandbox/Scripts/Recipes/MapTransition.fos`
- `Examples/MechanicsSandbox/Scripts/Tests/Test_MapTransition.fos`
- `Examples/MechanicsSandbox/Managed/Recipes/MapTransition.cs`
- `Examples/MechanicsSandbox/Content/MapTransition.fopro`
- `Examples/MechanicsSandbox/Maps/MapTransition_Yard.fomap`
- `Examples/MechanicsSandbox/FOnlineMechanicsSandbox.fomain`
- `Examples/MechanicsSandbox/mechanics-smoke.json`
- `Source/Server/Map.cpp`
- `Source/Server/MapManager.cpp`
- `Source/Server/StaticMap.cpp`
- `Source/Server/Server.cpp`
- `Source/Common/TimeEvents.cpp`
- `Docs/generated/api.json`

## Задача

Наступление на гекс должно начинать смену карты, а подходящий для «существо наступило сюда» hook движка — статический предмет-триггер. Сложнее всего сам перенос: он меняет исходную карту, карту назначения и локацию назначения, тогда как триггер выполняется внутри шага движения. Какие площадки существуют, куда ведут, что их открывает и что видит игрок во время смены — игровая политика; cover, порядок проверок и последствия для сохранения определяет контракт движка.

## Используемые примитивы движка

| Символ | Сторона | Роль в рецепте |
| --- | --- | --- |
| `script.property.Item.IsTrigger` | сервер | Включает площадку в поиск триггеров и обработку триггеров при движении |
| `script.property.Item.TriggerScript` | сервер | Называет функцию `[[ItemTrigger]]`, которую движок вызывает при каждом входе и выходе |
| `script.property.Item.NoBlock` | сервер | У площадки должно быть true, иначе движение не сможет закончиться на её гексе |
| `script.property.Item.ShootThru` | сервер | Тоже должно быть true; площадка, останавливающая выстрелы, блокирует и движение |
| `script.event.server.Game.OnStaticItemWalk` | сервер | Вызывается для того же пересечения после скрипта триггера; тест считает его вызовы |
| `script.method.server.Entity.StartTimeEvent#b494dfd4a4fa81ea` | сервер | `StartTimeEvent(timespan delay, callback(void,Entity,any[]) func, any[] data)` передаёт пункт назначения отложенной половине |
| `script.method.server.Game.GetMap#28a9744f2e2f208e` | сервер | `GetMap(hstring mapPid, int32 skipCount = 0)` находит карту назначения по id прототипа |
| `script.method.server.Critter.GetMap` | сервер | Называет исходную карту |
| `script.method.server.Game.Sync#a0c5737d3ba111b8` | сервер | `Sync(Entity[] entities)` покрывает существо, исходную карту и карту назначения |
| `script.method.server.Game.SyncWiden` | сервер | Добавляет локацию назначения к удерживаемому cover |
| `script.method.server.Map.GetLocation` | сервер | Называет локацию, которой принадлежит покрытая карта |
| `script.method.server.Critter.TransferToMap#c9880f573836f54f` | сервер | `TransferToMap(Map map, mpos hex)` переносит существо |
| `script.method.server.Critter.MapTransfersLocked` | сервер | True, пока выполняется перенос этого существа |
| `script.event.server.Game.OnCritterTransfer` | сервер | Вызывается один раз после завершённого переноса с предыдущей картой |
| `script.method.server.Entity.MakePersistent` | сервер | Даёт тестовому существу или локации собственную запись |
| `script.method.server.Entity.IsPersistent` | сервер | Тест читает, сохранило ли существо свою запись |
| `script.method.server.Game.LoadCritter#e4b9bdc5bdeb7919` | сервер | `LoadCritter(ident crId, bool forPlayer)` восстанавливает запись на глобальную карту |
| `script.method.server.Game.LoadCritter#f50433829d33781c` | сервер | `LoadCritter(ident crId, bool forPlayer, Map map)` восстанавливает её на карту |

`[[ItemTrigger]]`, `[[TimeEvent]]`, `[[Async]]`, `StaticItem`, `Entity.IsDestroyed` и `ZERO_TIMESPAN` — встроенные средства backend, а строки `///@ Property` — метаданные проекта.

## Минимальный пример

В `Examples/MechanicsSandbox/`: `Scripts/Recipes/MapTransition.fos` — сам рецепт, `Scripts/Tests/Test_MapTransition.fos` содержит его тесты и наивную площадку, `Content/MapTransition.fopro` — площадку, наивную площадку, ключ и прототип курьера, `Maps/MapTransition_Yard.fomap` размещает шесть статических площадок, а `Maps/MapTransition_Hall.fomap` и `Maps/MapTransition_Cellar.fomap` — карты назначения. `Managed/Recipes/MapTransition.cs` — форма Managed C#. Команды приведены в разделе [Маршрут проверки](#маршрут-проверки).

## Пошагово

Объявите данные выхода как серверные свойства предмета-площадки. Они задаются при каждом размещении, поэтому один прототип обслуживает любой выход:

```angelscript
///@ Property Item Server hstring ExitMap
///@ Property Item Server mpos ExitHex
///@ Property Item Server bool ExitNeedsKey
```

Прототип называет функцию триггера и делает предмет триггером. `NoBlock` и `ShootThru` обязательны: движок помечает гекс заблокированным для движения, если у статического предмета нет `NoBlock`, и заблокированным и для выстрелов, и для движения, если нет `ShootThru` (`StaticMap::ApplyItemToField` в `Source/Server/StaticMap.cpp`), поэтому движение к такому гексу сразу заканчивается как `HexBusy`:

```ini
[ProtoItem]
$Name = MapTransition_Pad
$Text engl = "Exit pad"
$Text russ = "Площадка выхода"
NoBlock = True
ShootThru = True
IsTrigger = True
TriggerScript = MapTransition::OnExitPad
```

Размещение в файле карты задаёт пункт назначения, а `ExitNeedsKey` запирает площадку:

```ini
[MapTransition_Yard/Item]
$Id = 3
$Proto = MapTransition_Pad
Hex = 30 29
Static = True
ExitMap = MapTransition_Hall
ExitHex = 10 10
ExitNeedsKey = True
```

Триггер вызывается при каждом входе на гекс и каждом выходе с него, в шаге движения, после смены гекса существа (`Map::VerifyTrigger` в `Source/Server/Map.cpp`, вызывается из `ServerEngine::ProcessCritterMovingBySteps`). Игнорируйте выходы, применяйте правило, которому нужны только существо и площадка, а остальное передайте time event. Пункт назначения передаётся как данные таймера, поэтому в отложенную половину попадают одни значения:

```angelscript
// The pad names this function in its TriggerScript; the engine calls it once for each entry and each exit of a hex
[[ItemTrigger]]
void OnExitPad(Critter cr, StaticItem pad, bool entered, mdir dir)
{
    if (!entered) {
        return;
    }

    // The rule reads only the critter and the immutable pad data, so it needs no further cover
    if (pad.ExitNeedsKey && cr.GetItem(KeyPid) == null) {
        Game.Log("map_transition_refused reason=no_key critter=" + cr.Id.value);
        return;
    }

    // A trigger runs inside the walking step with only the walking map covered; the transfer needs more, so a time event does it
    mpos exitHex = pad.ExitHex;
    any[] data = {any(pad.ExitMap), any(exitHex.x), any(exitHex.y)};
    cr.StartTimeEvent(ZERO_TIMESPAN, CompleteExit, data);
}
```

Отложенная половина — `[[Async]]` time event существа, поэтому при старте существо покрыто. Она находит карты, расширяет cover двумя запросами и после каждого проверяет всё заново:

```angelscript
// The deferred half: the critter is covered at entry, the maps and the destination location are not
[[TimeEvent]] [[Async]]
void CompleteExit(Critter cr, any[] data)
{
    hstring mapPid = hstring(data[0]);
    mpos exitHex = mpos(int(data[1]), int(data[2]));

    Map? destination = Game.GetMap(mapPid);
    Map? source = cr.GetMap();
    if (destination == null || source == null) {
        Game.Log("map_transition_refused reason=no_map critter=" + cr.Id.value);
        return;
    }

    // A transfer changes both maps, so both maps and the critter are covered by one request
    Game.Sync(array<Entity> = {cr, source, destination});

    if (cr.IsDestroyed || destination.IsDestroyed) {
        Game.Log("map_transition_refused reason=gone critter=" + cr.Id.value);
        return;
    }

    // Entering a map also enters its location, which only a covered map can name; widening may hand the cover over
    Game.SyncWiden(array<Entity> = {destination.GetLocation()});

    // The world may have changed while the cover changed hands
    if (cr.IsDestroyed || destination.IsDestroyed || cr.MapId != source.Id) {
        Game.Log("map_transition_refused reason=moved critter=" + cr.Id.value);
        return;
    }

    // Another transfer of the same critter is running; it owns the critter until it finishes
    if (cr.MapTransfersLocked) {
        Game.Log("map_transition_refused reason=locked critter=" + cr.Id.value);
        return;
    }

    cr.TransferToMap(destination, exitHex);
    Game.Log("map_transition_done critter=" + cr.Id.value);
}
```

Зачем нужен каждый шаг:

1. `Game.Sync` существа, исходной карты и карты назначения: `MapManager::Transfer` проверяет доступ к существу и к обеим картам (`Source/Server/MapManager.cpp`), а перенос меняет обе карты.
2. `Game.SyncWiden` локации назначения: когда карты различаются, `Transfer` проверяет и локацию назначения, а `Map.GetLocation` можно прочитать только у покрытой карты, поэтому локация не может войти в первый запрос.
3. Проверки после каждого захвата: оба захвата — точки, где могут выполняться другие задания, а расширение, добавляющее сущность, которую контекст ещё не покрывает, освобождает и заново захватывает весь удерживаемый набор ([расширение cover и повторная проверка после приостановки](cover-transactions.md#расширение-cover-и-повторная-проверка-после-приостановки)). К этому моменту существо могло исчезнуть, карта назначения — быть уничтожена, а существо — оказаться на другой карте.
4. Проверка `MapTransfersLocked`: `Transfer` выбрасывает `Critter transfers locked`, если уже выполняется другой перенос этого существа, поэтому рецепт отказывает.

### Наивный вариант не работает

`MapTransition_NaivePad` в `Content/MapTransition.fopro` называет скрипт триггера из `Scripts/Tests/Test_MapTransition.fos`, который вызывает `TransferToMap` прямо внутри шага движения. Шаг движения покрывает идущее существо и его карту, поэтому карта назначения не покрыта:

```angelscript
    try {
        cr.TransferToMap(hall, ExitHex);
    }
    catch {
        error = GetExceptionInfo();
    }
```

Тест требует серверный маркер `locked=false uncovered=true`: перенос выбросил `Entity access without sync`, существо в этот момент не было заблокировано, значит, сбой вызван отсутствующим cover, а не блокировкой. Существо остаётся на своей площадке на исходной карте, и `OnCritterTransfer` не вызывается.

## Полномочия и синхронизация

- Клиент никогда не запрашивает переход. Он идёт по обычному протоколу движения, сервер ведёт существо на площадку, а свойства назначения площадки — свойства `Server`, которые клиент не читает. После переноса клиент перезагружает карту и сообщает об этом; сценарий с настоящим клиентом ждёт этого сообщения.
- Триггер работает под cover шага движения: задание движения покрывает существо и его родительскую карту (`ServerEngine::CritterMovingJob` в `Source/Server/Server.cpp`). Рецепт ничего из этого в триггере не меняет: он читает существо и площадку и запускает time event.
- Time event покрывает своё существо и больше ничего, поэтому остальное рецепт захватывает сам. Порядок определяется тем, что можно прочитать: сначала карта назначения, затем локация, которую может назвать только эта карта.
- `OnCritterTransfer` вызывается один раз за завершённый перенос; для игрока в песочнице он вызывается дважды: при размещении при входе и при переходе по площадке, и второй вызов сообщает двор как `prevMap`.

## Сохранение и миграция

Запись существа следует за картой назначения. `Map::AddCritter` делает существо сохраняемым, если карта сохраняемая, а `Map::RemoveCritter` снова снимает сохраняемость, если существо не было сделано сохраняемым явно (`Source/Server/Map.cpp`). Сценарии перезапуска помещают трёх курьеров на сохраняемый двор и отправляют их через три площадки:

| Курьер | После переноса | После перезапуска |
| --- | --- | --- |
| Вошёл в сохраняемый зал | `IsPersistent()` равно true | Восстановлен вместе с залом |
| Вошёл во временный подвал | `IsPersistent()` равно false | Записи нет: `Game.LoadCritter(id, false)` выбрасывает `Critter data base loading error` |
| Явно сделан сохраняемым, затем вошёл в подвал | `IsPersistent()` равно true | Не размещён при восстановлении мира, потому что его карта не сохранялась; загрузка на другую карту отклоняется как `Cannot restore critter on stored map`, а `Game.LoadCritter(id, false)` загружает его на глобальную карту |

Проверки, на которых держатся первые две строки:

```angelscript
    Testing::Expect(ctx, a.IsPersistent(), "A courier that entered a persistent map keeps its record");
    Testing::Expect(ctx, !b.IsPersistent(), "A courier that entered a temporary map lost its record");
    Testing::Expect(ctx, c.IsPersistent(), "An explicitly persistent courier keeps its record in a temporary map");
```

Проект, отправляющий существ во временные карты, выбирает политику для каждого случая: отпустить запись или закрепить существо через `MakePersistent(true)` и самому отвечать за путь восстановления на случай, когда временной карты уже нет ([восстановление существа на сохранённой карте](../../explanation/runtime/server.md#восстановление-существа-на-сохранённой-карте)).

Данные площадки — авторское содержимое карты, поэтому изменение применяется пересборкой карты, и ни одна сохранённая запись их не хранит. Переименование объявленного свойства, например `ExitMap`, следует разделу [Объявляйте правило для каждого переименования, удаления и переноса](../project/data-migrations.md#объявляйте-правило-для-каждого-переименования-удаления-и-переноса); переименование ключа, заданного в карте, песочница не проверяла.

## Ловушки и антипаттерны

- Вызов `TransferToMap` внутри триггера выбрасывает исключение для непокрытой карты назначения, а захват более широкого cover внутри нативного шага движения рецепт не пробует. Отложите перенос в time event, как показывает отрицательный контроль.
- Площадка, блокирующая движение или выстрелы, не срабатывает: поиск пути отклоняет её гекс ещё до прихода существа. Задавайте `NoBlock` и `ShootThru` у каждого предмета-триггера.
- Если покрыть карту назначения и забыть её локацию, `MapManager::Transfer` завершится ошибкой, а сгенерированное описание `TransferToMap` об этом не говорит. Локацию называет покрытая карта, поэтому cover растёт двумя запросами, и после каждого все проверки повторяются.
- `Game.GetMap(mapPid)` возвращает первую живую карту с этим прототипом. Проект с несколькими экземплярами одного прототипа карты определяет назначение по экземпляру, например по id локации, сохранённому на площадке, или передаёт `skipCount`; в песочнице один экземпляр на прототип.
- Триггер срабатывает при каждом входе, а рецепт запускает по одному time event на вход без защиты. Два входа до выполнения первого события дали бы две попытки переноса, вторую — оттуда, где существо окажется к тому времени. Это следует из кода и не запускалось; проект, которому это важно, защищает запуск через `CountTimeEvent`.
- Существо, запись которого существует только потому, что его карта сохраняемая, теряет эту запись при входе во временную карту, и об этом ничто не сообщает. Выберите политику до появления первого временного пункта назначения.

## Маршрут проверки

Запускайте все сценарии песочницы из `Examples/MechanicsSandbox` командой `python validate.py` в Windows (PowerShell) или `python3 validate.py` в Linux (bash); предварительные условия перечислены в её `README.md`. Четыре сценария этого рецепта среди прочего требуют:

```text
[Testing] PASS map_transition.walking_onto_a_pad_transfers_the_critter
[Testing] PASS map_transition.locked_pads_need_the_key
[Testing] PASS map_transition.transfer_inside_the_walking_step_is_the_naive_variant
[Testing] PASS map_transition.persistence_follows_the_destination_map
[Testing] PASS map_transition_client.player_walks_onto_the_pad_and_the_client_reloads
[Testing] PASS map_transition_restart_verify.persistence_decided_by_the_destination_survives_restart
map_transition_refused reason=no_key
locked=false uncovered=true
error=GenericException: Critter data base loading error
error=GenericException: Cannot restore critter on stored map
```

Сценарий с клиентом запускает сервер с sub-config `RecipeNetworkTests` (`Testing.Enabled`, `Testing.TestTimeout = 60000`, `Server.DbStorage = Memory`), ждёт `map_transition_waiting_for_client` и запускает клиент без графического интерфейса с sub-config `RecipeClient`. Тест ждёт существа игрока на дворе, ведёт его к площадке зала и требует двух переносов, второй — со двора, и сообщения клиента `map_transition_client_map_loaded`.

Чтобы запустить набор внутри процесса на готовой сборке ресурсов, из каталога сборки (PowerShell; в Linux используйте `./Binaries/Server-Linux-x64/FOMECH_ServerHeadless`):

```powershell
& ".\Binaries\Server-Windows-win64\FOMECH_ServerHeadless.exe" -ApplyConfig "<sandbox>\FOnlineMechanicsSandbox.fomain" -ApplySubConfig RecipeTests -Testing.Filter "map_transition."
```

Для пары перезапуска используйте `-ApplySubConfig RecipeRestart -Server.DbStorage "JSON <empty-directory>"` с фильтрами `map_transition_restart_write.` и затем `map_transition_restart_verify.` на одном каталоге.

## Managed C#

Статус `shape`: тест движка разбирает `Managed/Recipes/MapTransition.cs` и сравнивает его строки `///@` с модулем AngelScript; ни одна линия движка не компилирует и не запускает его. Триггер — метод `[ItemTrigger]`, отложенная половина — метод `[TimeEvent]`, оба `internal`, потому что их вызывает движок. Оба cover становятся ожидаемыми вспомогательными вызовами, которые возвращают `false`, когда сущности уже нет, поэтому каждый является отдельным условием; расширение выглядит так:

```csharp
        // Entering a map also enters its location, which only a covered map can name; widening may hand the cover over
        if (!await Sync.Widen(destination.GetLocation())) {
            return;
        }
```

`Sync.Lock` и `Sync.Widen` оборачивают `Game.Sync` и `Game.SyncWiden`; правила анализатора для них описаны в разделе [Захват нескольких сущностей для одного действия](cover-transactions.md#эквивалент-на-managed-c).

## Как это расширяет крупный проект

- **Выходы на данных.** Замените три свойства площадки ключом в авторском каталоге, например строкой [каталога FixedType](../../explanation/entity-and-property-model/choosing-a-representation.md#строка-каталога-fixedtype) с пунктом назначения, гексом, ключом и ценой: выходы редактируются как данные, а площадка хранит один id.
- **Экземплярные пункты назначения.** Создавайте локацию назначения, когда прибывает первое существо, и переносите в её карту; тесты создают локации так же. Какой экземпляр получает группа и когда он уничтожается — политика проекта, которую рецепт не моделирует.
- **Последователи.** `MapManager::Transfer` заново присоединяет и переносит присоединённых существ после ведущего; правила групп и последователей — политика проекта, здесь они не проверялись.
- **Событие проекта с производителем.** Вызывайте одно событие проекта там, где происходит перенос: так другие модули реагируют без опроса. Форма объявления, не входящая в песочницу:

```text
///@ Event Server Game OnMapTransition(Critter cr, Map? from, Map to)
```

- **Что остаётся в игровом проекте.** Какие площадки существуют, их ключи и цены, что видит игрок во время смены и правило для временных пунктов назначения. Движок даёт вызов триггера, перенос и поведение сохранения; последовательность cover — код проекта.

## См. также

- [Создание игровых механик](index.md)
- [Захват нескольких сущностей для одного действия](cover-transactions.md)
- [Временный эффект, переживающий перезапуск](timed-effect.md)
- [Жизненный цикл и конкурентность скриптов: серверная синхронизация сущностей](../scripting/lifecycle-and-concurrency.md#серверная-синхронизация-сущностей)
- [Карты, движение и геометрия: модель блокировки](../../explanation/maps-and-movement.md#модель-блокировки)
- [Серверная среда выполнения: восстановление существа на сохранённой карте](../../explanation/runtime/server.md#восстановление-существа-на-сохранённой-карте)
- [Сохранение данных](../../explanation/persistence/index.md)
- [Скрипты Managed C#](../scripting/managed-csharp.md)
- [Шаблон обвязки скриптовых тестов](../project/script-test-harness.md)
