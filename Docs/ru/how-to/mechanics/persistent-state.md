---
layout: default
title: Собственное сохраняемое состояние персонажа
locale: ru
document_id: recipe-persistent-state
permalink: /Docs/ru/how-to/mechanics/persistent-state.html
recipe_kind: primitive
---

# Собственное сохраняемое состояние персонажа
<!-- docs-translation: {"document_id":"recipe-persistent-state","locale":"ru","source_path":"Docs/en/how-to/mechanics/persistent-state.md","source_sha256":"9ff7535ff584521c78b957b1500975f22fe60ab07aa8603a3bba6977768d0642"} -->
Этот рецепт добавляет значение, которым владеет сервер, которое сохраняется вместе с персонажем и возвращается после переподключения или перезапуска сервера: счётчик визитов, помеченный один раз при создании и изменяемый через одного писателя. Рецепт показывает, какие флаги свойств это обеспечивают, какой hook движка выполняется на каком пути восстановления, какие записи движок удаляет без запроса и какой двухфазный тест доказывает перезапуск.

| Backend | Статус |
| --- | --- |
| AngelScript | `runtime-verified` |
| Managed C# | `shape` |

`runtime-verified` здесь означает, что сценарий `persistent-state` внутри процесса на базе данных в памяти и двухфазные сценарии перезапуска `persistent-state-restart-write` и `persistent-state-restart-verify` на одном каталоге базы данных JSON прошли на собранной песочнице после сборки ресурсов на хосте Windows. Не проверены: компиляция и запуск Managed C#, путь восстановления серверного снимка, а также backend SQLite и MongoDB.

## Проверенные исходные пути

- `Examples/MechanicsSandbox/Scripts/Recipes/PersistentState.fos`
- `Examples/MechanicsSandbox/Scripts/Sandbox.fos`
- `Examples/MechanicsSandbox/Scripts/Tests/Test_PersistentState.fos`
- `Examples/MechanicsSandbox/Managed/Recipes/PersistentState.cs`
- `Examples/MechanicsSandbox/Content/PersistentState.fopro`
- `Examples/MechanicsSandbox/FOnlineMechanicsSandbox.fomain`
- `Examples/MechanicsSandbox/mechanics-smoke.json`
- `Source/Server/Server.cpp`
- `Source/Server/EntityManager.cpp`
- `Source/Server/Map.cpp`
- `Source/Server/DataBase.cpp`
- `Source/Common/Settings.inc`
- `Docs/generated/api.json`

## Задача

Персонажу нужно собственное состояние: счётчик, время первой встречи, флаг состоявшегося разговора. Игрок ожидает увидеть его после переподключения, а оператор — после перезапуска. Движок хранит сохраняемые свойства сохраняемых сущностей; скрипт выбирает флаги, инициализирует значение один раз, пишет его из одного места и доказывает перезапуск. Что означает значение, что считается визитом и что он открывает, — игровая политика, она остаётся в игровом проекте.

## Используемые примитивы движка

| Символ | Сторона | Роль в рецепте |
| --- | --- | --- |
| `script.event.server.Game.OnCritterInit` | сервер | Помечает новое существо один раз (`firstTime == true`); на каждом пути восстановления выполняется с `firstTime == false` |
| `script.event.server.Game.OnCritterLoad` | сервер | Вызывается только после `Game.LoadCritter`; тесты считают его, чтобы показать, что это не hook восстановления |
| `script.property.Game.SynchronizedTime` | общая | Игровые часы, которые сервер сохраняет; метка создания читает их |
| `script.method.server.Entity.MakePersistent` | сервер | Даёт сущности и её дочерним сущностям запись в базе данных |
| `script.method.server.Entity.IsPersistent` | сервер | Проверка в тесте, что запись существует |
| `script.method.server.Game.UnloadCritter` | сервер | Убирает живое существо; вместе с `Game.LoadCritter` воспроизводит восстановление в одном процессе |
| `script.method.server.Game.LoadCritter#f50433829d33781c` | сервер | `Critter Game.LoadCritter(ident crId, bool forPlayer, Map map)` восстанавливает сохранённое существо на его карту |
| `script.method.server.Game.GetLocations#8eb7a49e747de27a` | сервер | Находит сохранённую локацию после перезапуска |
| `setting.server.Server.DbStorage` | серверная настройка | `Memory` для тестов внутри процесса, `JSON <directory>` для перезапуска |

Строки `///@ Property` — метаданные проекта, а `[[ModuleInit]]`, `[[Event]]`, `Game.OnCritterInit.Subscribe`, `Entity.Id` и `ZERO_SYNCTIME` — встроенные средства backend; ни то, ни другое не входит в модель API. События сохранения нет: сохраняемое свойство сохраняемой сущности ставится в очередь записи в базу данных, когда меняется его значение (`ServerEngine::OnSaveEntityValue` в `Source/Server/Server.cpp`). `Game.OnCritterPreLoad` существует, но это [hook миграции для сохранённых существ](../scripting/lifecycle-and-concurrency.md#граница-preload-персистентности), а не место для игровой инициализации.

## Минимальный пример

В `Examples/MechanicsSandbox/`: `Scripts/Recipes/PersistentState.fos` содержит объявления, метку инициализации и единственного писателя; `Scripts/Sandbox.fos` — общие локацию, карту и вход клиента без графического интерфейса; `Scripts/Tests/Test_PersistentState.fos` — тесты внутри процесса и обе фазы перезапуска; `Content/PersistentState.fopro` — прототип `PersistentState_Npc`; `FOnlineMechanicsSandbox.fomain` — подконфигурации `RecipeTests` и `RecipeRestart`; `Managed/Recipes/PersistentState.cs` — форму Managed C#. Команды приведены в разделе [Маршрут проверки](#маршрут-проверки).

## Пошагово

Объявите состояние. `VisitCount` виден управляющему клиенту; `FirstVisitTime` никогда не покидает сервер. Оба свойства `Mutable Persistent`: сборщик ресурсов требует этого для значения, сохраняемого для каждой сущности ([флаги свойств](../../explanation/entity-and-property-model/choosing-a-representation.md#свойство-прототипа-и-компонент-свойств)):

```angelscript
///@ Property Critter Common int32 VisitCount Mutable Persistent OwnerSync
///@ Property Critter Server synctime FirstVisitTime Mutable Persistent
```

Инициализируйте в `Game.OnCritterInit` и только при `firstTime`, равном true. Восстановленное существо уже несёт то, что вернула база данных, поэтому безусловное присваивание здесь затирало бы сохранённые данные при каждой загрузке:

```angelscript
[[ModuleInit]]
void InitPersistentState()
{
    Game.OnCritterInit.Subscribe(OnCritterInit);
}

// firstTime is true only for a new critter; a restored critter keeps what the database returned
[[Event]]
void OnCritterInit(Critter cr, bool firstTime)
{
    if (firstTime) {
        cr.FirstVisitTime = Game.SynchronizedTime;
    }
}
```

Пишите значение из одной функции. Чтение свойства возвращает копию; изменение становится состоянием и попадает в очередь записи в базу данных только после присваивания обратно:

```angelscript
// The only writer of VisitCount: read the stored value, change the copy, assign it back
int32 RecordVisit(Critter cr)
{
    int32 count = cr.VisitCount + 1;
    cr.VisitCount = count;
    return count;
}
```

Со стороны контента это обычный прототип; состоянию не нужно заданное значение, потому что новые существа начинают со значения по умолчанию из объявления:

```ini
# A character whose visit counter is saved with it
[ProtoCritter]
$Name = PersistentState_Npc
$Text engl = "Regular visitor"
$Text russ = "Постоянный посетитель"
```

Созданное существо временное. Оно получает запись, когда сохраняемым делают его самого или локацию, в которой он находится. Затем тест внутри процесса выгружает его и загружает обратно. `Game.UnloadCritter` сохраняет запись существа, сохраняемого только через локацию, делая эту сохраняемость явной, см. [Ловушки](#ловушки-и-антипаттерны):

```angelscript
    location.MakePersistent(true);

    ident crId = cr.Id;
    synctime stamp = cr.FirstVisitTime;
    Game.UnloadCritter(cr);

    Critter restored = Game.LoadCritter(crId, false, map);
    Testing::ExpectEq(ctx, restored.VisitCount, 2, "The count comes back from the database");
    Testing::Expect(ctx, restored.FirstVisitTime == stamp, "A restore keeps the creation stamp");
    Testing::ExpectEq(ctx, restored.TestRestoreInitCount, 1, "Game.LoadCritter runs OnCritterInit(cr, false)");
    Testing::ExpectEq(ctx, restored.TestLoadEventCount, 1, "Game.LoadCritter fires OnCritterLoad");
```

Запись игрока не требует особой подготовки. `Game.LoginPlayerToNewRecord` сохраняет имя игрока вместе с его сохраняемыми свойствами, поэтому новый игрок, у которого все свойства остались по умолчанию, всё равно получает запись:

```angelscript
    unloginedPlayer.SetName("SandboxPlayer");
    Player player = Game.LoginPlayerToNewRecord(unloginedPlayer);
```

## Полномочия и синхронизация

- Сервер — единственный писатель. `RecordVisit` — единственное место, которое меняет `VisitCount`; клиент, которому нужно записать визит, просит об этом удалённым вызовом, который проверяет сервер ([Удалённые вызовы: авторитет и обработка ошибок](../../reference/scripting/remote-calls.md#авторитет-и-обработка-ошибок) и [рецепт проверяемого действия](validated-action.md)).
- `OwnerSync` отправляет значение клиенту игрока, который управляет существом, а `PublicSync` дополнительно рассылал бы его наблюдателям (`ServerEngine::OnSendCritterValue` в `Source/Server/Server.cpp`). Без `ModifiableByClient` клиент не может его изменить ([runtime свойств](../../explanation/entity-and-property-model/index.md#runtime-свойств)).
- Свойство `Server` никогда не отправляется и не принимает флаг синхронизации. Держите на стороне сервера факты, которые клиент видеть не должен, как здесь `FirstVisitTime`.
- `OnCritterInit` получает управление с покрытым cover существом ([callbacks `InitScript` сущности](../scripting/lifecycle-and-concurrency.md#callbacks-initscript-сущности)). `RecordVisit` затрагивает только `cr`, поэтому вызывающий код должен держать cover этого существа; форма Managed C# выражает это через `[RequiresCover]`. Тест перезапуска вызывает `Game.Sync` для восстановленной локации, прежде чем читать существо.

## Сохранение и миграция

Что возвращается после перезапуска: сохраняемые свойства сущностей, у которых была запись, и `Game.SynchronizedTime`, которое сервер сохраняет и продолжает. Что не возвращается: временные сущности, временные свойства и time events; для срока окончания нужен [рецепт временного эффекта](timed-effect.md).

Какой hook выполняется на каком пути:

| Путь | `Game.OnCritterPreLoad` | `Game.OnCritterInit` | `Game.OnCritterLoad` |
| --- | --- | --- | --- |
| Новое существо (`Map.AddCritter`, `Game.CreateCritter`) | нет | один раз, `firstTime == true` | нет |
| Существо карты, восстановленное при старте сервера | один раз (исходники) | один раз, `firstTime == false` (проверено) | никогда (проверено) |
| `Game.LoadCritter` | один раз (исходники) | один раз, `firstTime == false` (проверено) | один раз (проверено) |

Строки с пометкой «проверено» подсчитаны тестами песочницы; «исходники» означает `EntityManager::LoadCritter` в `Source/Server/EntityManager.cpp`, который вызывают оба пути восстановления. Единственное место вызова `OnCritterLoad` — `ServerEngine::LoadCritter` в `Source/Server/Server.cpp`. Поэтому код восстановления относится к `OnCritterInit` с `firstTime == false` или к `InitScript` сущности.

Переименование сохраняемого свойства — изменение совместимости. Объявите правило в том же изменении и оставьте игровой код только на новом имени ([Объявляйте правило для каждого переименования, удаления и переноса](../project/data-migrations.md#объявляйте-правило-для-каждого-переименования-удаления-и-переноса)):

```text
///@ MigrationRule Property Critter Rename VisitCount VisitTotal
```

Удалённое свойство сохраняет постоянную метку `Remove`, а смена типа — это новое имя, а не переименование. Доказывайте миграцию так же, как этот рецепт: сохранение, записанное старыми объявлениями, перезапуск и проверки загруженных значений ([Протестируйте миграцию](../project/data-migrations.md#протестируйте-миграцию)).

## Ловушки и антипаттерны

- База данных в памяти ничего не доказывает о перезапуске. При `Server.DbStorage = Memory` записи живут, пока живёт процесс; сохранение доказывает только второй процесс на том же `JSON <directory>` или той же базе данных, как [Протестируйте миграцию](../project/data-migrations.md#протестируйте-миграцию) требует для любого сохраняемого изменения.
- Сохраняемое свойство временной сущности не записывается: `ServerEngine::OnSaveEntityValue` (`Source/Server/Server.cpp`) пропускает сущности без записи. Текущие значения попадают в базу данных, только когда сущность получает запись (`EntityManager::MakePersistentRecursive` сохраняет документ целиком), и теряются, если этого не происходит. Существо, созданное `Map.AddCritter`, остаётся временным, пока сохраняемым не сделают его самого или его локацию; тест `new_critter_is_temporary_until_made_persistent` проверяет оба состояния.
- Сохраняемость, унаследованная от карты, заканчивается, когда существо покидает карту. `Map::AddCritter` повышает существо на сохраняемой карте, а `Map::RemoveCritter` понижает существо, которое не сохраняемо явно, и это удаляет его запись (`Source/Server/Map.cpp`, `EntityManager::MakeNonPersistentRecursive` в `Source/Server/EntityManager.cpp`). Поэтому переход на временную карту удаляет запись, а переход на другую сохраняемую карту удаляет её и пишет заново. Исключение — `Game.UnloadCritter`: перед снятием существа с карты он делает такую сохраняемость явной (`ServerEngine::UnloadCritter` в `Source/Server/Server.cpp`), поэтому запись сохраняется, и загруженное существо владеет ею сам. Вызывайте `MakePersistent(true)` для существа, которое должно сохранять запись при переходах.
- `Game.OnCritterLoad` — не hook восстановления. Он никогда не выполняется для существ, восстановленных вместе с картами при старте ([time events после перезагрузки из базы данных](../scripting/lifecycle-and-concurrency.md#time-events-после-перезагрузки-из-базы-данных) формулирует то же правило), поэтому состояние, которое восстанавливается только там, отсутствует после каждого перезапуска.
- `Baking.PrebakeOnStartup` по умолчанию `true` (`Source/Common/Settings.inc`), поэтому каждый процесс многопроцессного теста запускает сборщик ресурсов перед стартом. Песочница выполняет сборку ресурсов один раз и задаёт `Baking.PrebakeOnStartup = False`, чтобы каждая фаза стартовала на этой одной сборке.
- Глобальный словарь скрипта с ключом по id сущности вместо свойства отделяет данные от времени жизни и сохранения сущности ([антипаттерны](../../explanation/entity-and-property-model/choosing-a-representation.md#антипаттерны)).

## Маршрут проверки

Подконфигурации выбирают базу данных. Тесты внутри процесса используют память; пара перезапуска использует каталог JSON, который runner заменяет своим через `-Server.DbStorage`:

```ini
# In-process recipe tests on an in-memory database
[SubConfig]
Name = RecipeTests
Testing.Enabled = True
ServerNetwork.DisableNetworking = True
Server.DbStorage = Memory

# Two-phase restart tests; the runner passes its own JSON directory with -Server.DbStorage
[SubConfig]
Name = RecipeRestart
Testing.Enabled = True
ServerNetwork.DisableNetworking = True
Server.DbStorage = JSON RecipeRestartDb
```

Фаза 1 требует пустую базу данных, создаёт существо, записывает три визита, делает локацию сохраняемой и проверяет `cr.IsPersistent()`. Фаза 2 — новый процесс сервера на том же каталоге. Она читает то, что вернуло восстановление мира, и считает hooks:

```angelscript
    Location[] locations = Game.GetLocations(Sandbox::LocationPid);
    if (!Testing::ExpectEq(ctx, locations.length(), 1, "The saved location was loaded")) {
        return;
    }

    Game.Sync(locations[0]);
    Critter[] critters = locations[0].GetMapByIndex(0).GetCritters(NpcPid, CritterFindType::Any);
    if (!Testing::ExpectEq(ctx, critters.length(), 1, "The saved critter was loaded with its map")) {
        return;
    }

    Critter cr = critters[0];
    Testing::ExpectEq(ctx, cr.VisitCount, 3, "The count survives the restart");
    Testing::Expect(ctx, cr.FirstVisitTime == cr.TestExpectedFirstVisitTime, "The restore keeps the creation stamp");
    Testing::ExpectEq(ctx, cr.TestRestoreInitCount, 1, "World loading runs OnCritterInit(cr, false) once");
    Testing::ExpectEq(ctx, cr.TestLoadEventCount, 0, "World loading does not fire OnCritterLoad for a map critter");
```

Все сценарии песочницы запускаются из `Examples/MechanicsSandbox` командой `python validate.py` в Windows (PowerShell) или `python3 validate.py` в Linux (bash); требования перечислены в её `README.ru.md`. Запуск проходит, когда `BuildTools/gameplay_test_runner.py` сообщает `status=passed` для каждого сценария и завершается с кодом `0`. Сценарии этого рецепта требуют, среди прочего, таких маркеров:

```text
[Testing] PASS persistent_state.unload_and_load_keep_the_state
[Testing] PASS persistent_state_restart_write.save_state_before_restart
[Testing] PASS persistent_state_restart_verify.state_survives_restart
[Testing] COMPLETE status=passed
```

Чтобы повторить перезапуск вручную на готовой сборке ресурсов, выполните фазу 1, а затем фазу 2 с одним и тем же пустым каталогом из каталога сборки (PowerShell; в Linux используйте `./Binaries/Server-Linux-x64/FOMECH_ServerHeadless`):

```powershell
& ".\Binaries\Server-Windows-win64\FOMECH_ServerHeadless.exe" -ApplyConfig "<sandbox>\FOnlineMechanicsSandbox.fomain" -ApplySubConfig RecipeRestart -Server.DbStorage "JSON <empty-directory>" -Testing.Filter "persistent_state_restart_write."
& ".\Binaries\Server-Windows-win64\FOMECH_ServerHeadless.exe" -ApplyConfig "<sandbox>\FOnlineMechanicsSandbox.fomain" -ApplySubConfig RecipeRestart -Server.DbStorage "JSON <empty-directory>" -Testing.Filter "persistent_state_restart_verify."
```

## Managed C#

Статус `shape`: тест движка разбирает `Managed/Recipes/PersistentState.cs` и сравнивает его строки `///@` с модулем AngelScript, они идентичны; ни одна линия движка его не компилирует и не запускает. Hook и писатель сохраняют ту же структуру. Методы, которые вызывает движок, объявлены `internal` ([форма авторского кода Managed C#](../scripting/managed-csharp.md#форма-авторского-кода)), а `[RequiresCover]` указывает cover, который должен держать вызывающий код ([серверная синхронизация сущностей](../scripting/managed-csharp.md#серверная-синхронизация-сущностей)):

```csharp
    [ModuleInit]
    internal static void InitPersistentState()
    {
        Game.OnCritterInit.Subscribe(OnCritterInit);
    }

    // firstTime is true only for a new critter; a restored critter keeps what the database returned
    [Event]
    internal static void OnCritterInit([RequiresCover] Critter cr, bool firstTime)
    {
        if (firstTime) {
            cr.FirstVisitTime = Game.SynchronizedTime;
        }
    }

    // The only writer of VisitCount: read the stored value, change the copy, assign it back
    public static int RecordVisit([RequiresCover] Critter cr)
    {
        int count = cr.VisitCount + 1;
        cr.VisitCount = count;
        return count;
    }
```

Правила сохранения, hooks и ловушки выше не зависят от backend. Проект на managed backend должен выполнить тот же двухфазный тест перезапуска своей обвязкой, прежде чем полагаться на эту форму.

## Как это расширяет крупный проект

- **Каталоги данных.** Пороги и награды, которые зависят от значения, — заданные авторами данные: один настраиваемый скаляр — это `///@ Setting`, таблица именованных строк — [каталог FixedType](../../explanation/entity-and-property-model/choosing-a-representation.md#строка-каталога-fixedtype). Значение для каждого персонажа остаётся свойством.
- **Компоненты.** Если состояние есть только у некоторых видов существ, сгруппируйте его поля в компонент свойств с маркером наличия вместо плоских свойств у каждого существа ([компоненты свойств](../content/prototype-format.md#компоненты-свойств)). Несколько значений, которые всегда меняются вместе, становятся RefType в одном свойстве и записываются тем же шагом «прочитать, изменить, присвоить обратно», что и в `RecordVisit`.
- **События проекта и правило производителя.** Если на изменение реагируют другие модули, объявите событие проекта и вызывайте его из единственного писателя: у каждого объявленного события ровно один производитель, и ни один подписчик не ждёт события, которое никто не вызывает. Форма объявления, не входящая в песочницу:

```text
///@ Event Server Game OnVisitRecorded(Critter cr, int32 count)
```

- **Что остаётся в игровом проекте.** Что такое визит, все числа, выводимые из счётчика, тексты и контент, а также правила миграции для собственных имён проекта. Движок предоставляет флаги, hooks, базу данных и порядок восстановления.

## См. также

- [Создание игровых механик](index.md)
- [Временный эффект, переживающий перезапуск](timed-effect.md)
- [Жизненный цикл и конкурентность скриптов](../scripting/lifecycle-and-concurrency.md)
- [Выбор представления игровых данных](../../explanation/entity-and-property-model/choosing-a-representation.md)
- [Модель сущностей](../../explanation/entity-and-property-model/index.md)
- [Сохранение данных](../../explanation/persistence/index.md#связь-с-состоянием-сущности)
- [Миграция сохранённых данных и контента в игровом проекте](../project/data-migrations.md)
- [Удалённые вызовы](../../reference/scripting/remote-calls.md)
- [Шаблон обвязки скриптовых тестов](../project/script-test-harness.md)
