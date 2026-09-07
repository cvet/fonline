---
layout: default
title: Жизненный цикл и конкурентность скриптов
locale: ru
document_id: script-lifecycle-concurrency
permalink: /Docs/ru/how-to/scripting/lifecycle-and-concurrency.html
---

# Жизненный цикл и конкурентность скриптов

<!-- docs-translation: {"document_id":"script-lifecycle-concurrency","locale":"ru","source_path":"Docs/en/how-to/scripting/lifecycle-and-concurrency.md","source_sha256":"af884e30fafef1ce54242efa9756e2447b7f00ec42ea4301ec39188a75f3320a"} -->

> Документация движка. Это руководство описывает переиспользуемое поведение lifecycle и concurrency AngelScript. Модули проекта, gameplay policies и проектные synchronization helpers принадлежат подключающей игре.

## Назначение

Используйте это руководство, когда скрипту нужно инициализировать модуль, подписать callback, подождать через `Yield`, изменить серверные сущности, владеть runtime-состоянием или корректно завершить работу. Перед написанием кода явно ответьте на четыре вопроса:

1. Кто вызывает эту функцию и в какой фазе runtime?
2. Может ли эта цепочка вызовов приостановиться или продолжиться на другом server worker?
3. Какой объект владеет изменяемым состоянием и его lifetime?
4. Какой synchronization cover сущностей действителен именно в этой точке?

Примеры должны сохранять тот же контракт. Когда документированный lookup может
не вернуть сущность, храните результат как `T?` и сужайте его перед
использованием. Вызывайте `Yield` только из транзитивной `[[Async]]`-цепочки, а
после возобновления заново разрешайте и сужайте сущность и получайте cover. Если
точной сигнатуры lookup или callback нет в справочнике-владельце, объясняйте
lifecycle без выдуманного сокращённого кода.
Native entry cover зависит от dispatcher: правило inbound remote call ниже не
доказывает, что другой event, setter, callback или direct script entry начинает
с пустым или тем же cover. Перед утверждением initial cover проверяйте его
native dispatcher-владелец.

Для каждой server entry point, обращающейся к entity, используйте следующее
решение:

`ServerEngine::RunScriptContext()` создаёт вложенный `SyncContext`, который
может хранить entity cover; сам по себе он не покрывает ни одной entity. Initial
entity cover внутри этого context может установить native dispatcher-владелец.

1. Полагайтесь на initial synchronization cover только тогда, когда native
   dispatcher-владелец этой entry point доказывает точный покрытый набор.
2. Если требуемая entity находится вне доказанного набора или initial set не
   документирован, до чтения или изменения вызовите `Game.Sync(...)` с полным
   требуемым набором. Не заполняйте пробел в evidence утверждением, что
   посторонний event, setter, callback или direct entry начинает с пустым cover.
3. После `Yield` заново получите полный cover до обращения к entity: cover
   предыдущего выполнения не переживает suspension.

Не перефразируйте это как «events, setters или callbacks не создают
дополнительный sync scope». Это то же неподтверждённое обобщение между
dispatchers в другой форме. Указывайте только доказанный dispatcher cover и
явный cover `Game.Sync(...)`, используемый скриптом.

Читайте руководство вместе со следующими материалами:

- [Скриптовый runtime](../../explanation/scripting-runtime/) — полная скриптовая подсистема и путь нативной привязки.
- [Модель сущностей](../../explanation/entity-and-property-model/) — владение сущностями, свойствами, holder и destruction.
- [Серверный runtime](../../explanation/runtime/server.md) и [клиентский runtime](../../explanation/runtime/client.md) — side-specific loops и managers.
- [Удалённые вызовы](../../reference/scripting/remote-calls.md) — сетевые entry points и границы авторитетности.
- [Nullability.md](../../../Nullability.md) — handles, которые могут исчезнуть до возобновления continuation.
- [сгенерированный справочник API](../../../generated/api/index.md) — текущие сигнатуры методов, атрибуты, настройки и ссылки на исходный код.

## Проверенные пути исходного кода

- `Source/Common/ScriptSystem.h`
- `Source/Common/ScriptSystem.cpp`
- `Source/Common/EntityProperties.h`
- `Source/Common/Entity.h`
- `Source/Common/Entity.cpp`
- `Source/Client/Client.cpp`
- `Source/Server/EntityManager.cpp`
- `Source/Server/Server.cpp`
- `Source/Server/WorkerPool.cpp`
- `Source/Tools/Baker.cpp`
- `Source/Server/EntitySync.h`
- `Source/Server/EntitySync.cpp`
- `Source/Scripting/ServerGlobalScriptMethods.cpp`
- `Source/Scripting/ServerCritterScriptMethods.cpp`
- `Source/Scripting/ServerItemScriptMethods.cpp`
- `Source/Scripting/ServerLocationScriptMethods.cpp`
- `Source/Scripting/ServerMapScriptMethods.cpp`
- `Source/Scripting/AngelScript/AngelScriptAttributes.cpp`
- `Source/Scripting/AngelScript/AngelScriptBackend.cpp`
- `Source/Scripting/AngelScript/AngelScriptCall.cpp`
- `Source/Scripting/AngelScript/AngelScriptContext.cpp`
- `Source/Scripting/AngelScript/AngelScriptEntity.cpp`
- `Source/Scripting/AngelScript/AngelScriptGlobals.cpp`
- `Source/Scripting/AngelScript/AngelScriptRemoteCalls.cpp`
- `Source/Scripting/AngelScript/CoreScripts/Input.fos`
- `ThirdParty/AngelScript/sdk/angelscript/source/as_compiler.cpp`
- `ThirdParty/AngelScript/sdk/angelscript/source/as_scriptengine.cpp`
- `Source/Tests/Test_AngelScriptCall.cpp`
- `Source/Tests/Test_AngelScriptAttributes.cpp`
- `Source/Tests/Test_AngelScriptBaker.cpp`
- `Source/Tests/Test_EntityLifecycle.cpp`
- `Source/Tests/Test_EntitySync.cpp`
- `Source/Tests/Test_ServerMapOperations.cpp`
- `Source/Tests/Test_ScriptEntityOps.cpp`
- `Source/Tests/Test_ScriptBuiltins.cpp`

## Модель runtime

Выполнение скриптов состоит из ограниченных входов, а не происходит в одном сериализованном потоке всей игры:

| Фаза | Владелец | Важная граница |
|---|---|---|
| Компиляция и bake | AngelScript compiler и bakers | Атрибуты, использование callback, nullable handles и изменяемые globals проверяются до runtime. |
| Инициализация модуля | `ScriptSystem::InitModules()` | Init functions выполняются по возрастанию приоритета, пока присваивание globals временно разрешено. |
| Инициализация сущности | `EntityManager::CallInit()` | Сущность отмечается инициализированной, срабатывает её событие `On*Init`, затем выполняется необязательный сохранённый callback `InitScript`. |
| Dispatch callback | Client loop или server worker job | Events, time events, remote calls и native re-entry вызывают атрибутированные функции через принадлежащий им API. |
| Приостановка | AngelScript context manager | `Yield` сохраняет script continuation, но текущая нативная execution scope возвращается. |
| Возобновление | Client scheduled-callback pass или server worker pool | Continuation выполняется позже; на сервере она может работать на другом worker под новым synchronization context. |
| Уничтожение сущности | Владелец entity/manager | Callbacks событий и storage time event очищаются; серверные dispatch jobs отменяются владеющим manager. |
| Завершение runtime | Client/server и scripting backend | Global events/time events, сущности, script globals, contexts и backend освобождаются в порядке, заданном владельцами. |

Из этой модели следуют два практических правила:

- живой script handle не доказывает, что сущность всё ещё валидна или синхронизирована;
- код после любой точки приостановки является новым наблюдением изменяемого мира, а не продолжением атомарной транзакции.

## Инициализация модуля

Объявите глобальную функцию без аргументов, возвращающую `void`, с атрибутом `[[ModuleInit]]`. Атрибут принимает необязательный знаковый целочисленный приоритет:

```angelscript
[[ModuleInit(100)]]
void InitializeInventoryRules()
{
    Game.OnSomeEvent.Subscribe(OnSomeEvent);
}
```

Имя события выше приведено для примера; используйте сгенерированное событие из текущего справочника API.

Runtime индексирует валидные init functions, сохраняет их приоритеты и применяет стабильную сортировку по возрастанию. `[[ModuleInit]]` без аргумента имеет приоритет `0`. Меньшие значения выполняются раньше. Равные приоритеты сохраняют порядок регистрации, но межмодульные зависимости всё равно следует задавать явными приоритетами, а не полагаться на порядок обнаружения файлов или bytecode.

`ScriptSystem::InitModules()` выполняет последовательность:

1. размораживает script globals;
2. вызывает все зарегистрированные init functions;
3. прерывает запуск при ошибке любого вызова;
4. замораживает globals после завершения всех функций.

Следовательно, `Game.SetConstGlobalVar(...)` допустим только при инициализации. После границы freeze он выбрасывает исключение. Baker также отклоняет изменяемые globals уровня модуля, если их namespace явно не разрешён в `Script.MutableGlobalsAllowedNamespaces`.

Используйте module init для регистрации и построения неизменяемых lookup. Не запускайте gameplay work, предполагающий, что world, current player, map или network session уже существуют. Клиент вызывает `Game.OnStart` после инициализации модулей; сервер запускает свои initialization lifecycle events после `InitModules()` в последовательности старта сервера.

## События и владение callback

Event handler должен иметь `[[Event]]` и передаваться через `Subscribe` или `Unsubscribe`. Прямые вызовы event handler запрещены проверкой атрибутов. То же правило владения применяется к другим callback attributes:

- функции `[[TimeEvent]]` принадлежат API `StartTimeEvent`, `StopTimeEvent`, `CountTimeEvent`, `RepeatTimeEvent` и `SetTimeEventData`;
- handlers remote call принадлежат dispatcher, описанному в [удалённых вызовах](../../reference/scripting/remote-calls.md);
- callbacks анимации и свойств принадлежат соответствующим registration APIs.

Такое разделение делает контекст вызова видимым. Помещайте переиспользуемую логику в обычный helper, а атрибутированный callback пусть адаптирует к нему аргументы события.

```angelscript
[[Event]]
void OnSomeEvent(Critter critter)
{
    ApplyImmediateRule(critter);
}

void ApplyImmediateRule(Critter critter)
{
    // Ordinary logic that may also be called from another ordinary function.
}
```

Подписки хранятся на сущности, владеющей событием. `Entity::MarkAsDestroyed()` очищает все callbacks событий и записи time event этой сущности до установки destroyed state. Server managers дополнительно отменяют запланированные jobs time event до окончательного уничтожения сущности.

Явно отписывайтесь, когда поведение должно прекратиться до уничтожения владельца, при замене callback или когда долгоживущий global owner должен освободить проектный объект. Не ведите второй глобальный реестр проекта только ради отписки обычных entity callbacks при destruction: он дублирует владение lifetime движка и создаёт дополнительный источник stale handles.

Event dispatch может запускать пользовательские callbacks, изменяющие подписки, поэтому движок обходит snapshot callback. Не рассчитывайте, что подписка, добавленная во время dispatch, будет вызвана в том же dispatch.

### Граница preload персистентности

`Game.OnCritterPreLoad` является серверным migration hook для существующего сохранённого криттера. Он срабатывает один раз после восстановления свойств криттера, инвентаря и вложенных сущностей и после регистрации криттера, но до входа на карту или глобальную карту, обработки видимости, `OnCritterInit(critter, false)` и `OnCritterLoad`. Новые криттеры это событие не получают.

Callback выполняется при заблокированных map transfers. Ограничьте его нормализацией собственных сохранённых свойств и инвентаря криттера: остальной мир может быть восстановлен лишь частично, а разрешение, загрузка или перемещение других сохранённых сущностей на этой фазе не поддерживаются. Handler может явно уничтожить криттера, чтобы отбросить устаревший сохранённый граф. Исключение или остановка цепочки событий вместо этого помечает загрузку неуспешной и оставляет сохранённую запись для диагностики. Полный порядок загрузки и поведение player-bound описаны в [серверном runtime](../../explanation/runtime/server.md).

## Callbacks `InitScript` сущности

`Item`, `Critter`, `Map` и `Location` имеют серверное persistent property `InitScript`. Оно называет глобальный callback с соответствующей сущностью и `bool firstTime`; авторские значения проверяются на сигнатуру при server baking. Точные сигнатуры и правила авторинга прототипов находятся в [Формате прототипов](../content/prototype-format.md#init-scripts).

При обычной инициализации сущности `EntityManager::CallInit()`:

1. проверяет сущность и возвращается, если инициализация уже выполнялась;
2. удерживает сущность живой и устанавливает initialized flag;
3. запускает соответствующее событие `Game.OnItemInit`, `OnCritterInit`, `OnMapInit` или `OnLocationInit`;
4. вызывает именованный `InitScript`, только если событие не уничтожило сущность;
5. рекурсивно инициализирует оставшихся живыми owned children.

`firstTime` равен `true` для новых сущностей и `false` для восстановленного состояния мира. Новая location инициализирует дочерние maps до callback location, тогда как загрузка мира начинается с location и проходит вниз через maps, critters и items. Не кодируйте предположения о межсущностном порядке в init callback.

Неразрешённое имя или несовпадающая сигнатура выбрасывают `ScriptException`. Поскольку initialized flag и dispatch события предшествуют разрешению функции, это fail-loud гарантия lifecycle уровня `Basic`, а не транзакционный rollback. Исключение в script body проходит другой путь: `ScriptFunc::Call()` имеет `noexcept`, сообщает об исключении и возвращает `false`. Обычный `CallInit()` считает его уже зарегистрированным; runtime `SetupScript` / `SetupScriptEx` преобразуют результат в `ScriptException("Call init failed", ...)`.

`SetupScript(typedFunction)` и `SetupScriptEx(name)` сразу выполняют callback с `firstTime = true`, а имя функции сохраняют только после успеха. Typed overload отклоняет delegates, потому что persisted callback должен глобально разрешаться. Пустое property означает отсутствие entity-specific callback; проекты могут использовать соответствующее глобальное событие `Game.On*Init`, когда один subscriber владеет поведением для множества prototypes.

Callback входит с cover своей сущности. Доступ к другой сущности требует обычного полного cover или проверенного расширения. Callback не является неявно async, и ни один cover не переживает `Yield`.

## Распространение Async и `Yield`

`Yield(int durationMs)` зарегистрирован с атрибутом функции `Async`. `Async` является транзитивным marker: каждая script function, непосредственно вызывающая функцию `[[Async]]`, также должна иметь `[[Async]]`. Callback может одновременно иметь атрибут вызова и marker:

```angelscript
[[TimeEvent]] [[Async]]
void RefreshLater(Critter critter)
{
    Yield(10);
    // This is a later observation. Revalidate before accessing mutable state.
}
```

При `Yield` AngelScript приостанавливает активный context и планирует `ResumeSpecificContext()` на указанное время игрового кадра. Script stack и local handles остаются в этом context. Нативный вызов, выполнявший context, возвращается с результатом suspended.

Стороны runtime возобновляют работу по-разному:

- **Клиент:** delayed callbacks обрабатываются из snapshot callback, срок которых уже наступил в начале текущего прохода main loop. Поэтому `Yield(0)` возобновляется на следующем проходе, после того как loop может обработать сеть и input.
- **Сервер:** delayed callbacks передаются worker pool. Каждая worker job имеет synchronization context, а каждое script execution создаёт вложенный synchronization context. Continuation может возобновиться на другом worker thread.

Context manager не позволяет двум workers одновременно выполнять один suspended AngelScript context. Это защищает сам объект continuation, но не сериализует несвязанные callbacks и не делает состояние проекта thread-safe.

### Правило приостановки

Считайте каждый `Yield` и каждый способный приостановиться helper границей транзакции:

1. Завершите или отмените текущую мутацию до suspension.
2. Не переносите ожидание `Game.Lock` через suspension.
3. После resumption заново получите требуемый entity cover.
4. Заново разрешите или проверьте сущности, которые могли быть уничтожены, detached или reparented.
5. Перед записью повторно прочитайте свойства, коллекции, parent links и другие изменяемые решения.

Идентификаторы часто безопаснее как состояние continuation, чем предположительно валидный snapshot сущности. Разрешение идентификатора после resumption всё равно требует обработки null/destroyed и синхронизации до доступа.

## Серверная синхронизация сущностей

Server callbacks могут выполняться конкурентно. Движок проверяет доступ к авторитетной сущности против `SyncContext` текущего потока; непокрытое чтение или запись является ошибкой контракта, а не безобидной гонкой.

`Game.Sync(...)` является видимой скриптам границей получения cover:

- заменяет текущий entity cover переданными non-null сущностями и определёнными движком auto-widen partners;
- перегрузки принимают одну, две, три сущности или массив;
- перегрузка массива отклоняет null entries;
- следующий `Game.Sync(...)` не расширяет прежний cover, поэтому запрашивайте полный набор для следующей операции;
- `Game.GetHeldSyncEntities()` сообщает владельцев текущего entity cover для диагностики;
- `Game.IsEntityLocked(entity)` проверяет coverage без диагностического сообщения о непокрытом доступе;
- `Game.SyncRelease()` освобождает entity cover и все singleton lock entries текущей script execution scope.

`Game.Sync(...)` несёт `[[Async]]`, поэтому marker распространяется на непосредственных script callers даже тогда, когда получение завершается без suspension AngelScript context.

### Cover нативного входа

Каждое server-side AngelScript execution входит через `ServerEngine::RunScriptContext()` и получает собственный вложенный `SyncContext`; event, setter или remote-call dispatcher не создаёт ещё одну script scope вокруг handler. Native entry points могут установить начальный cover внутри активного context до dispatch.

Для входящего server remote call `Process_RemoteCall()` синхронизирует аргумент `Player` непосредственно перед вызовом script handler. Entity widening включает контролируемого игроком `Critter` и обратную связь critter-to-player, поэтому handler может читать эту связанную пару без дополнительного `Game.Sync(...)`. Независимо разрешённые сущности всё ещё требуют явного полного cover. Последующий `Game.Sync(...)` заменяет entry cover, поэтому снова включите player или controlled critter, если они нужны следующей операции.

### Повторное подключение существующего игрока

`Game.LoginPlayerToExistentRecord(unloginedPlayer, playerId)` сохраняет текущий cover вызывающего. При live reconnect вызывающий должен заранее получить полный граф:

- входящий unlogined `Player`;
- существующий live `Player`;
- его controlled `Critter`, если он есть;
- текущие `Map` и `Location`, если critter находится на карте;
- каждого участника текущей global-map group, если critter находится на глобальной карте.

Auto-widen пары Player/Critter не покрывает parent map криттера или parent location карты. Аналогично, cover лидера группы не покрывает остальных участников global-map group. Полный граф нужен, потому что reconnect вызывает `OnPlayerLogin` и отправляет initial information криттера без сужения caller cover; initial info локальной карты читает map/location, а initial info глобальной карты сериализует участников группы.

Типичный проектный поток сначала разрешает необязательного live player через `Game.GetPlayer(playerId)`, синхронизирует входящего и live players, обнаруживает covered controlled critter, затем получает и повторно проверяет map/location или global-group graph. Каждый последующий `Game.Sync(...)` заменяет предыдущий набор, поэтому все требуемые сущности должны присутствовать в финальном запросе. Helper и каждый прямой caller должны иметь `[[Async]]`.

Вход в offline stored record не имеет существующего runtime player. Он начинается с покрытым unlogined player и строит восстановленный character graph через обычный проектный bootstrap `OnPlayerLogin`.

### `Game.Lock` является отдельным механизмом

`Game.Lock()` получает глобальный singleton lock `Game` в отдельном recursive bucket. Сопоставляйте его с `Game.Unlock()` и держите critical section короткой. `Game.Unlock()` не меняет entity cover.

Не вызывайте `Game.Sync(...)`, пока удерживается `Game.Lock()`. Движок отклоняет такой порядок, предотвращая цикл singleton/entity lock. `Game.SyncRelease()` опустошает оба bucket, включая несбалансированные recursive singleton entries, но это поведение teardown является защитной границей, а не рекомендуемой заменой парным `Lock`/`Unlock`.

### Cover не переживает приостановку

`ServerEngine::RunScriptContext()` создаёт вложенный `ScopedSyncContext` вокруг каждого вызова `ctx->Execute()`. Destructor scope освобождает все entity и singleton locks при возврате `ctx->Execute()`, включая возврат из-за `Yield`. Resumption снова вызывает `ctx->Execute()` под новым вложенным context.

Поэтому следующее рассуждение неверно:

```text
Game.Sync(entity) -> read state -> Yield(...) -> write using the old cover and decision
```

Правильная форма:

```text
resolve entity -> Game.Sync(entity) -> read/mutate -> Yield(...)
resolve or revalidate entity -> Game.Sync(entity) -> re-read -> decide/mutate
```

У native extension есть дополнительный helper расширения `EnsureEntitySynced(...)` для операций движка, которые включают покрытого descendant или только что созданную сущность в текущий context. Он не заменяет видимый скриптам контракт `Game.Sync(...)` и не должен предоставляться как проектный workaround для неверно ограниченной операции.

## Владение изменяемым состоянием

Выбирайте самого узкого владельца, lifetime которого соответствует состоянию:

| Состояние | Предпочтительный владелец |
|---|---|
| На сущность, persistent или replicated | Объявленное property сущности с правильными persistence/sync flags. |
| На сущность, только runtime | Неперсистентное property сущности или component движка/проекта, которым владеет сущность. |
| На экземпляр движка | Manager или component, доступный через `ServerEngine`, `ClientEngine` или другого явного владельца. |
| Неизменяемые данные модуля | `const` global, инициализированный напрямую или через `Game.SetConstGlobalVar` во время module init. |
| Короткое состояние continuation | Local values в функции `[[Async]]` с повторной проверкой изменяемых данных мира после suspension. |

Избегайте изменяемых script-global dictionaries с ключом entity id для обычного состояния сущности. Они отделяют данные от lifetime, могут утекать при повторном использовании id или между тестами, требуют независимой очистки и становятся общей concurrency boundary. Если состояние принадлежит сущности, хранение на ней оставляет явными правила synchronization, destruction, persistence и replication.

Разрешённый namespace mutable globals является escape hatch для прошедшей review подсистемы, а не архитектурой по умолчанию. Его владелец должен определить synchronization, reset behavior, изоляцию экземпляров и shutdown cleanup.

## Уничтожение и завершение

Уничтожение сущности и shutdown runtime связаны, но различаются:

- `Entity::MarkAsDestroyed()` очищает callbacks сущности и записи time event.
- Server entity managers отменяют dispatcher jobs до окончательного уничтожения.
- Client и server shutdown очищают global events/time events и уничтожают owned entities в упорядоченной последовательности runtime.
- `AngelScriptBackend` уничтожает context manager, затем вызывает `asIScriptEngine::ShutDownAndRelease()`, пока modules, types, behaviours и backend links ещё доступны. Исправленный shutdown AngelScript вызывает exits модулей, освобождает globals, выполняет полные проходы garbage collection до опустошения или стабилизации live set, удаляет modules, повторяет collection и сообщает о неосвобождаемых survivors до сброса backend links.

Условие остановки garbage collection — **опустошение или стабилизация**, а не обязательное опустошение. Стабильный live set может содержать неосвобождаемых survivors; shutdown сообщает о них для диагностики и продолжает упорядоченный teardown.

Эти пункты не задают единый полный порядок, объединяющий очистку client/server entities с внутренними стадиями modules и garbage collection backend. Следуйте каждой принадлежащей владельцу последовательности shutdown независимо, пока runtime source не задаёт межвладельческую границу порядка.

Не используйте `Game.OnFinish` только для повторения этих действий владельцев. Используйте его для функционального project teardown: сброса project service, завершения external session или отмены работы, которая принадлежит внешнему относительно обычного entity lifetime владельцу.

Script expression temporaries, возвращаемые object handles, delegates, arrays и dictionaries принадлежат runtime согласно зарегистрированным behaviours AngelScript. Unsafe-reference expressions могут отложить очистку receiver и arguments до безопасной точки выражения; native/script call bridges удерживают скопированный результат и освобождают заменённые объекты и объекты exception path. Проектные скрипты не должны добавлять ручные reference-count или shutdown registries для компенсации этих деталей. Сохранившийся object graph указывает на отсутствующий owner release или GC-enumeration contract в типе-владельце.

Suspended continuation может пережить gameplay assumptions, при которых началась. После resumption destroyed handle следует считать невалидным, даже если local variable остаётся non-null на уровне языка скриптов; обычная защита entity access должна отклонить destroyed access. Когда destruction является нормальным исходом, предпочитайте явное разрешение и nullable narrowing.

## Рекомендуемый процесс

При добавлении или изменении callback:

1. Определите API-владельца и примените требуемый callback attribute.
2. Добавьте `[[Async]]` ко всей прямой цепочке вызовов, если она достигает `Yield`, `Game.Sync` или другой async-marked функции.
3. Перечислите все авторитетные сущности, которые читаются или изменяются между точками suspension.
4. Получите один полный cover для операции и не удерживайте `Game.Lock` во время его получения.
5. Разместите mutable state на владельце его lifecycle; документируйте любое исключение mutable-global.
6. После suspension или re-entry повторно проверяйте состояние и получайте cover.
7. Добавьте узкий script/compiler/runtime test и выполните script bake подключающего проекта.

## Маршруты проверки

Выбирайте самый узкий gate, доказывающий изменённый контракт:

| Изменение | Минимальная проверка |
|---|---|
| Callback attribute или правило прямого вызова | `Test_AngelScriptAttributes` и компиляция/bake скриптов проекта. |
| Политика mutable global | `Test_AngelScriptBaker` и компиляция/bake скриптов проекта. |
| `Yield` или планирование context | Тесты AngelScript context и затронутый client/server runtime test. |
| Server cover, singleton lock или access validation | `Test_EntitySync`, затронутые script-method/entity tests и runtime-путь проекта. |
| Cover входящего server remote call | Server runtime/remote-call tests и handler, читающий controlled critter вызывающего. |
| Миграция persisted critter preload | `Test_EntityLifecycle`, migration и bake tests подключающего проекта. |
| Authoring или runtime resolution `InitScript` | `Test_ServerMapOperations`, специализированные baker tests и bake подключающего проекта. |
| Lifetime callback/time event сущности | Entity/time-event tests и smoke path destruction либо shutdown. |
| Lifetime script object или shutdown GC | `Test_AngelScriptCall`, `Test_ScriptBuiltins` и engine shutdown smoke path. |
| Только поведение проектного скрипта | Bake подключающего проекта и самый узкий gameplay/scene test. |

Для всех изменений документации движка также выполняйте standalone gate из [регламента сопровождения документации](../../contributing/documentation/).

## Контрольный список review

- Callback входит через принадлежащий ему API, а не вызывается напрямую.
- Каждый async caller имеет `[[Async]]`.
- Ни entity cover, ни snapshot состояния не считаются сохранившимися после `Yield`.
- `Game.Lock` сбалансирован и освобождён до `Game.Sync`.
- Один вызов `Game.Sync` называет полный набор сущностей следующей операции.
- Preload migration изменяет только поддерживаемое на этой фазе состояние восстановленного криттера.
- Mutable state находится на явном владельце lifecycle.
- Destruction cleanup не дублируется в project-global registry без функциональной причины.
- Выбранный тест выполняет настоящую границу compile, scheduling, synchronization или teardown.
