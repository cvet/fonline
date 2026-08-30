---
layout: default
title: Скриптовый runtime
locale: ru
document_id: scripting-runtime
permalink: /Docs/ru/explanation/scripting-runtime/
---

# Скриптовый runtime

<!-- docs-translation: {"document_id":"scripting-runtime","locale":"ru","source_path":"Docs/en/explanation/scripting-runtime/index.md","source_sha256":"6ce5488c827a4878c79d88fb54bab7a6bdba46b13262a4cd359616a9de6bb011"} -->

> Документация движка. Эта страница описывает переиспользуемое поведение скриптового runtime в `Source/Common/ScriptSystem.*` и `Source/Scripting/`; конкретные игровые скрипты, квесты, правила и политика контента принадлежат подключающему проекту.

## Назначение

Скриптовый слой является контрактом между C++ runtime движка и поведением, созданным авторами игры. Он предоставляет скриптам сущности движка, глобальные сервисы, события, remote calls, value types, коллекции, reflection helpers и helpers инструментов/frontend, при этом владение C++, метаданные, nullability, персистентность, сеть и валидация остаются в движке.

Читайте эту страницу вместе со следующими материалами:

- [Стиль AngelScript и рефакторинг](../../how-to/scripting/style-and-refactoring.md) — построение модуля, layout исходного кода, поведение formatter, дисциплина сгенерированных файлов, пакеты рефакторинга и validation gates.
- [GeneratedApiAndMetadata.md](../../reference/metadata/index.md) — сгенерированные метаданные, аннотации `///@` и результат codegen.
- [Жизненный цикл и конкурентность скриптов](../../how-to/scripting/lifecycle-and-concurrency.md) — инициализация модулей, владение callback, `[[Async]]`, `Yield`, серверные synchronization cover, владение изменяемым состоянием и правила завершения.
- [Удалённые вызовы](../../reference/scripting/remote-calls.md) — грамматика, направление, обработчики, авторитетность, генерация проектного каталога и проверка remote calls.
- [Nullability.md](../../../Nullability.md) — контракты `T?` в скриптах и `ptr<T>`·`nptr<T>` в нативном коде на границе script/native.
- [Модель сущностей](../entity-and-property-model/) — сущности, прототипы, свойства и holder, доступные скриптам.
- [Серверный runtime](../runtime/server.md) и [клиентский runtime](../runtime/client.md) — runtime events и владение script callback.
- [Инструменты Mapper](../../how-to/tools/mapper.md) для специфичных скриптовых помощников Mapper.
- [Карта методов скриптового API](../../reference/script-api/method-ownership.md) — карта файлов нативных script methods.
- [Текст и локализация](../../how-to/content/text-and-localization.md) — `TextPackKey`, `LanguageName`, `Game.GetText`, переключение языка и граница проектного lexem formatting.

## Проверенные пути исходного кода

- `Source/Common/ScriptSystem.h`
- `Source/Common/ScriptSystem.cpp`
- `Source/Scripting/AngelScript/AngelScriptScripting.h`
- `Source/Scripting/AngelScript/AngelScriptScripting.cpp`
- `Source/Scripting/AngelScript/AngelScriptBackend.h`
- `Source/Scripting/AngelScript/AngelScriptBackend.cpp`
- `Source/Scripting/AngelScript/AngelScriptAttributes.cpp`
- `Source/Scripting/AngelScript/AngelScriptCall.cpp`
- `Source/Scripting/AngelScript/AngelScriptEntity.cpp`
- `Source/Scripting/AngelScript/AngelScriptGlobals.cpp`
- `Source/Scripting/AngelScript/AngelScriptRemoteCalls.cpp`
- `Source/Scripting/AngelScript/AngelScriptReflection.cpp`
- `Source/Scripting/AngelScript/CoreScripts/*.fos`
- `ThirdParty/AngelScript/sdk/angelscript/source/as_compiler.cpp`
- `Source/Scripting/*ScriptMethods.cpp`
- `Source/Scripting/Mono/*.cs`
- `Source/Scripting/Native/.keepalive`
- `BuildTools/cmake/stages/ScriptsAndBaking.cmake`
- `Source/Tests/Test_AngelScriptAttributes.cpp`
- `Source/Tests/Test_AngelScriptBaker.cpp`
- `Source/Tests/Test_AngelScriptBytecode.cpp`
- `Source/Tests/Test_AngelScriptCall.cpp`
- `Source/Tests/Test_ClientDataValidation.cpp`
- `Source/Tests/Test_CommonScriptMethods.cpp`
- `Source/Tests/Test_EntityLifecycle.cpp`
- `Source/Tests/Test_EntitySync.cpp`
- `Source/Tests/Test_MetadataBaker.cpp`
- `Source/Tests/Test_NetBuffer.cpp`
- `Source/Tests/Test_ScriptBuiltins.cpp`
- `Source/Tests/Test_ScriptEntityOps.cpp`
- `Source/Tests/Test_ServerScriptMethods.cpp`

## Карта слоёв

Скриптовая подсистема состоит из четырёх слоёв:

1. **Общий runtime facade** — `Source/Common/ScriptSystem.h` и `.cpp` определяют независимые от backend `ScriptSystem`, `ScriptFuncDesc`, `ScriptFunc`, `FuncCallData`, `DataAccessor`, адаптеры нативных вызовов, init functions, loop callbacks и карты типов.
2. **Реализация backend** — `Source/Scripting/AngelScript/` предоставляет текущий production-backend. Mono и native scripting имеют placeholder/source roots, но реализованный путь компилятора и runtime скриптов в этом дереве принадлежит AngelScript.
3. **Видимые скриптам нативные методы** — файлы `Source/Scripting/*ScriptMethods.cpp` содержат функции `///@ ExportMethod`, сгруппированные по стороне runtime и типу receiver. Codegen читает эти аннотации и создаёт descriptors и wrappers методов.
4. **Библиотека core scripts и игровые скрипты** — `Source/Scripting/AngelScript/CoreScripts/*.fos` предоставляет переиспользуемые скриптовые helpers движка. Подключающие проекты добавляют свои `.fos` и метаданные через конфигурацию проекта и запекание ресурсов и скриптов.

Движок владеет переиспользуемым bridge. Подключающий проект владеет игровыми скриптами и выбирает включённые возможности через конфигурацию проекта, build presets и входы `.fomain`.

## `ScriptSystem`: независимая от backend диспетчеризация

`ScriptSystem` — C++ facade runtime, который используют клиент, сервер, Mapper, тесты и инструменты со скриптами. Основные обязанности:

- регистрация одного или нескольких `ScriptSystemBackend` через `RegisterBackend()`;
- сопоставление C++-типов с descriptors метаданных через `MapScriptTypes()` и `MapEngineType()` / `MapEngineDictType()`;
- инициализация модулей через `InitModules()`;
- поиск и вызов глобальных функций через `FindFunc()`, `CheckFunc()`, `CallFunc()` и `CallAdminFunc()`;
- хранение полученных от backend записей `ScriptFuncDesc` через `AddGlobalScriptFunc()`;
- запуск зарегистрированных init functions и loop callbacks через `AddInitFunc()`, `AddLoopCallback()` и `ProcessScriptEvents()`.

`ScriptFunc<TRet, Args...>` нормализует нативные аргументы в `FuncCallData` и перехватывает script exceptions, чтобы вызывающий мог продолжить работу после ошибки callback. Состояние очистки результата хранится только для возвращаемых типов, отличных от `void`; у void callback нет return storage, которое требовалось бы освобождать при перемещении или уничтожении отложенного callback во время teardown сущности. `NativeDataProvider` и `NativeDataCaller` адаптируют к общему представлению вызова C++-массивы, словари, сущности, callbacks, value types и mutable references.

На этой границе также вставляются сгенерированные проверки nullability. `NativeDataProvider::CheckArgNotNull()` и `CheckReturnNotNull()` вызываются созданными codegen lambda `MethodDesc::Call`, а не только AngelScript-адаптером. Полный контракт описан в [Nullability.md](../../../Nullability.md).

## Путь AngelScript runtime

`InitAngelScriptScripting()` в `Source/Scripting/AngelScript/AngelScriptScripting.cpp` подготавливает AngelScript runtime, создаёт `AngelScriptBackend`, регистрирует его в `ScriptSystemBackend::ANGELSCRIPT_BACKEND_INDEX` и загружает бинарные скрипты из ресурсов.

`CompileAngelScript()` является compiler-side entry point для инструментов и тестов. Он создаёт автономный `ScriptSystem`, регистрирует метаданные, компилирует текстовые script files и возвращает bytecode.

`AngelScriptBackend` владеет конкретным экземпляром движка и жизненным циклом модулей:

- `RegisterMetadata()` привязывает метаданные движка и регистрирует типы C++ и script-visible.
- `BindRequiredStuff()` регистрирует массивы, словари, строки, математические и value types, globals, entity wrappers, remote callers, reflection helpers и helpers backend.
- `CompileTextScripts()` препроцессит исходный код, добавляет script sections в модуль, разрешает includes, собирает модуль и сериализует bytecode.
- `LoadBinaryScripts()` загружает скомпилированный bytecode из ресурсов во время выполнения.
- `SetMessageCallback()` / `SendMessage()` направляют диагностику compiler/runtime вызывающему. Диагностические позиции AngelScript сохраняют исходную строку скрипта, но форматируют только имя файла без полного пути, поэтому журналы стабильны между локальной и CI workspace.
- cleanup и post-cleanup callbacks освобождают ресурсы backend в контролируемом порядке.

Таким образом, AngelScript используется в двух режимах: tooling mode во время компиляции и runtime mode. Одинаковый код метаданных и регистрации типов должен оставаться совместимым с обоими.

AngelScript назначает IDs зарегистрированных object types лениво. Разные script contexts могут одновременно запросить один новый type, поэтому vendored runtime читает и инициализирует `asCTypeInfo::typeId` под reader/writer lock Engine и повторно читает значение после получения exclusive access. `AngelScriptTypeIdsAreLazilyAssignedAcrossThreads` запускает 16 native workers для 128 новых типов через публичный `asITypeInfo::GetTypeId()` и требует один одинаковый валидный ID для каждого типа.

Нативные методы, зарегистрированные через сгенерированные descriptors `MethodDesc`, вызываются через `ScriptGenericCall()`. Унифицированный slot `FuncCallData` для изменяемого простого аргумента является **адресом переменной вызывающего**: адресом значения для primitives, enums и value types (`int32&`, `mpos&`, `string&`) и адресом ячейки handle для object handles (`Critter@&`). Все producers со стороны AngelScript соблюдают этот контракт: `ScriptGenericCall()` классифицирует аргумент по регистрационным descriptors `MethodDesc` / `EntityEventDesc`, из которых создана декларация `&` или `@&`; он и семейство `Invoke` разрешают mutable arguments через `asIScriptGeneric::GetArgAddress()`, то есть pointer из стека, тогда как обычные input arguments используют `GetAddressOfArg()`. Consumers работают симметрично: `NativeDataCaller::ConvertArg` / `ReturnArg` читают и записывают через slot, а AngelScript-to-AngelScript ветвь `ScriptFuncCall()` передаёт slot напрямую в `asIScriptContext::SetArgAddress()`. Это относится к событиям с by-ref arguments и к `Invoke`, нацеленному на script function. Regression coverage: `Test_CommonScriptMethods.cpp` (`TimePackingOperations`, `GameInvokeOperations/ByNameWithRefArgs`) и `Test_ScriptEntityOps.cpp` (`AdvancedServerOperations/CustomEntityEventRefArgs`).

Когда включён `asEP_ALLOW_UNSAFE_REFERENCES`, AngelScript может откладывать освобождение receiver метода и аргументов до безопасной точки выражения. Компиляция short-circuit boolean обрабатывает отложенные параметры левого операнда после материализации его primitive-результата `bool` и до объединения branch bytecode. Иначе правый операнд может повторно использовать временный object slot и перезаписать сохранённый receiver без освобождения. `ScriptBuiltinsDeferredReceiverTemporaryIsReleased` покрывает сочетание property accessor и method call, которое проявило проблему при завершении GUI.

### Завершение AngelScript backend

`~AngelScriptBackend()` завершает runtime в фиксированном порядке: останавливает debugger endpoint, выполняет зарегистрированные cleanup callbacks, сбрасывает context manager, затем вызывает `asIScriptEngine::ShutDownAndRelease()`, пока script modules, object types, behaviours и backend links ещё существуют. Путь shutdown AngelScript вызывает `CallExit()` каждого модуля, деинициализирует globals, повторяет полные проходы GC до опустошения live set или прекращения прогресса, удаляет модули и сообщает об объектах, которые всё ещё нельзя уничтожить. Фиксированного предела проходов нет: script destructors могут создать ещё один конечный собираемый граф, которому потребуется следующий проход. После освобождения engine backend сбрасывает `_meta`, `_scriptSys`, `_engine` и `_entityMngr`, затем выполняет post-cleanup callbacks.

Globals, delegates, script object handles, массивы, словари и GUI object graphs должны очищаться shutdown модулей, destructors, `ReleaseAllHandles` и AngelScript GC. Проектным скриптам не следует добавлять очистку `Game.OnFinish` / `EngineCallback_Finish` только для подавления shutdown diagnostics; если граф переживает shutdown, исправьте release или GC-enumeration владельца в нативном коде.

Удаление или выгрузка сущности очищает её callbacks событий и time events из `Entity::MarkAsDestroyed()`, поэтому проектным скриптам не нужен центральный реестр unsubscribe / `StopTimeEvent` для обычного lifetime сущности. Mutators сущности и entry points событий и time events assert или verify при вызове после `MarkAsDestroyed()`, чтобы попытка заново наполнить уничтоженную сущность показывала stack trace места ошибки. Во время `ServerEngine::Shutdown` / `ClientEngine::Shutdown` движок также выполняет `UnsubscribeAllEvents()` и `ClearAllTimeEvents()` для глобальной сущности engine и всех живых сущностей до `DestroyAllEntities()`. Проектные скрипты не должны вручную вести unsubscribe, global-clear или `StopTimeEvent` cleanup в `Game.OnFinish` только ради тишины GC; там нужен только функциональный teardown.

Destroyed entities также отклоняются на script-to-native boundary. `NativeDataCaller::ConvertArg` сначала проверяет доступ, затем отвергает destroyed entity для любого обычного `///@ ExportMethod`. На сервере uncovered destroyed handle поэтому сообщает actionable fault отсутствующего cover, а всё ещё covered handle, который caller уничтожил и продолжил использовать, сообщает fault destroyed argument. На клиенте cover validation отсутствует и возможен только второй вариант.

Единственное исключение — `///@ ExportMethod ... AllowDestroyedEntityArgs`, которое codegen превращает в compile-time call-policy flag. Оно существует для явных synchronization primitives вроде `Game.Sync`: concurrent destroy всегда может произойти между script liveness check и вызовом синхронизации, а wrapper обязан вернуть `false`, а не завершиться на conversion аргумента. Не применяйте flag к обычным exports. Контракт закреплён `ServerEngineDestroyedEntityArgumentReportsMissingCoverFirst` и `SyncAcceptsDestroyedEntity`.

## Атрибуты, объявления и метаданные

`Source/Scripting/AngelScript/AngelScriptAttributes.cpp` разбирает специфичные для движка script attributes и declaration tags. Важные контракты:

- удаление nullable-суффикса `T?` и перенос признака в метаданные;
- объявления `///@ Event` и соответствующие обработчики `[[Event]]`;
- объявления `///@ RemoteCall`, необязательные структурные пределы `MaxBytes N` / `MaxCollectionSize N` и соответствующие реализации `[[ServerRemoteCall]]` или `[[ClientRemoteCall]]`;
- отдельный entry point административных команд `[[AdminRemoteCall]]`;
- приоритеты модулей и init functions;
- правила проверки callback attributes;
- `[[InvokeEntry]]` для функций, вызываемых только по имени через глобальный helper `Invoke(...)`. Атрибут запрещает обычные прямые вызовы, сохраняя возможность передать ссылку на функцию для регистрации `NameOf(...)`.

Эти атрибуты являются контрактами исходного кода. После preprocessing AngelScript видит нормализованные объявления, а метаданные и analyzers движка сохраняют высокоуровневый смысл FOnline.

Полная модель authoring и runtime для `[[ModuleInit]]`, callback-only attributes, транзитивного `[[Async]]`, `Yield`, серверных `Game.Sync` / `Game.Lock`, владения состоянием и teardown callback находится в разделе [Жизненный цикл и конкурентность скриптов](../../how-to/scripting/lifecycle-and-concurrency.md). Эта страница должна оставаться описанием состава подсистемы; при проектировании lifecycle-sensitive scripts используйте специализированное руководство.

## Сущности и свойства в скриптах

`Source/Scripting/AngelScript/AngelScriptEntity.cpp` регистрирует script object types для сущностей движка, singleton-like components, property accessors, типов entity events и dispatch методов. Он соединяет сгенерированные метаданные с регистрационными вызовами AngelScript, чтобы скрипты работали с critters, items, maps, locations, players, prototypes, abstracts, statics, holders и property-backed components через видимые скриптам имена.

Lifetime сущностей всё равно принадлежит runtime движка:

- серверные скрипты работают с авторитетными сущностями, которыми владеют `ServerEngine` и managers;
- клиентские скрипты работают с view/client entities, которыми владеет `ClientEngine`;
- mapper-скрипты работают с editor state, которым владеет Mapper;
- script handles нельзя считать владением персистентностью.

Владение сущностями, свойствами и прототипами описано в [модели сущностей](../entity-and-property-model/), а границы базы данных — в [персистентности](../persistence/).

## Удалённые вызовы и callbacks событий

`Source/Scripting/AngelScript/AngelScriptRemoteCalls.cpp` регистрирует object types отправителей, включая `RemoteCaller` и `CritterRemoteCaller`. Объявления remote call опираются на метаданные, а runtime-обработка разделена по сторонам:

- серверная обработка команд проверяет пришедшие от клиента удалённые вызовы до запуска server script handlers;
- клиентский runtime получает пришедшие от сервера вызовы и dispatch клиентских script handlers;
- административные вызовы используют путь `CallAdminFunc()` и требуют атрибут `AdminRemoteCall`.

Для недоверенного вызова клиент-сервер задавайте `MaxBytes` как максимальный корректный сериализованный payload, а `MaxCollectionSize` — как максимальную корректную объявленную коллекцию. Сервер разрешает descriptor до allocation тела; native validation и AngelScript decoding применяют предел коллекции до `Reserve()` или создания контейнера, включая вложенные массивы словаря. Общая настройка `ServerNetwork.MaxRemoteCallPayloadSize` остаётся отдельным пределом для враждебного input. Полный контракт декларации, baked metadata и совместимости описан в разделе [Удалённые вызовы](../../reference/scripting/remote-calls.md).

Events и remote calls являются разными понятиями. Events описывают lifecycle движка/runtime и gameplay notifications; remote calls описывают доступные по сети script entry points. Оба механизма опираются на сигнатуры метаданных, nullable-контракты и сгенерированные descriptors.

Полный контракт authoring, caller surface, namespace, безопасности, запечённого каталога и совместимости находится в разделе [Удалённые вызовы](../../reference/scripting/remote-calls.md).

## Экспорт нативных script methods

Нативные script API группируются по имени файла:

- `Common*ScriptMethods.cpp` — API, общие для нескольких сторон, включая global helpers и обёртки ImGui.
- `Server*ScriptMethods.cpp` — авторитетные серверные API для создания игры, персистентности, движения, изменения сущностей и операций player/critter/map/item/location.
- `Client*ScriptMethods.cpp` — client/view API для UI, ресурсов, связанных с rendering операций карты, видимых critters/items, audio/video, input и локального состояния.
- `Mapper*ScriptMethods.cpp` — API Mapper и редактора для создания, перемещения, выбора, сохранения и организации сущностей карты.

Каждая экспортируемая функция помечена `///@ ExportMethod` и обычно начинается с префикса стороны и типа, например `Server_Map_`, `Client_Game_`, `Common_ImGui_` или `Mapper_Game_`. Codegen преобразует объявления в видимые скриптам descriptors и backend call wrappers. Завершающие C++-параметры по умолчанию сохраняются в метаданных и восстанавливаются в регистрационных объявлениях AngelScript; значения C++ value types вроде `fpos32 {}` нормализуются в script expressions вроде `fpos()`. Предпочитайте один экспортируемый метод с default arguments нескольким перегрузкам, которые только добавляют необязательные аргументы. Карта файлов и обязанностей приведена в [карте методов скриптового API](../../reference/script-api/method-ownership.md).

Для instance methods сущности dispatch AngelScript проверяет receiver до входа в тело нативного метода. `Entity_MethodCall` вызывает `CheckScriptEntityAccessAndNonDestroyed`, который проверяет серверный synchronization cover и destroyed state сущности `self`. Не добавляйте entry-only `ValidateEntityAccess(self)` и не повторяйте проверку receiver перед обычным чтением. Далее в теле проверяйте сущности только на реальных границах доступа или assert, например при event dispatch либо продолжении после re-entry. Если покрытая сущность должна сохранить собственный lock при detach или reparent, используйте сохраняющий cover и идемпотентный `EnsureEntitySynced(...)`: он сохраняет уже имеющийся caller cover, никогда не освобождает и не паркуется на нём и не может получить пропущенную dependency.

При добавлении метода размещайте его на стороне, владеющей изменяемым состоянием. Например, авторитетное создание предмета относится к серверным методам, а sprite/UI helpers — к клиентским или общим frontend methods.

Lookup текста следует той же модели владения. Клиентские и mapper-скрипты могут получать строки и менять язык; серверные скрипты предоставляют только проверку наличия текста и число вариантов. Полный поведенческий контракт и семантика отсутствующих данных находятся в [руководстве по тексту и локализации](../../how-to/content/text-and-localization.md).

Client render helpers `Game.DrawSprite`, `Game.DrawSpritePattern` и `Game.DrawSpriteRegion` допустимы только в render-facing callbacks (`RenderIface` и GUI draw callbacks). `Game.DrawSpriteRegion(sprId, uv0, uv1, pos, size, color)` рисует нормализованный подпрямоугольник `[0, 1]` исходного логического изображения sprite в destination rectangle; polygon-cropped atlas frames преобразуются через исходный offset, а прозрачные обрезанные поля остаются прозрачными в результате. `Game.DrawSpritePattern` использует тот же контракт логического изображения для каждой полной или частичной tile. Region drawing предназначен для переиспользуемой GUI composition, например script-side 9-slice panels, и возвращает `false`, когда sprite не поддерживает atlas-region drawing.

## Core scripts

Библиотека AngelScript core движка находится в `Source/Scripting/AngelScript/CoreScripts/` и включает переиспользуемые модули:

- `Core.fos`
- `Math.fos`
- `Time.fos`
- `Color.fos` (`namespace Color`, `Color::Text`, `Color::Neutral`)
- `Input.fos`
- `Gui.fos`
- `Sprite.fos`
- `LineTracer.fos`
- `Serializer.fos`
- `FixedDropMenu.fos`
- `Tween.fos`

Считайте эти файлы библиотечным кодом движка. Игровые script modules должны находиться в подключающем проекте, а не расширять core scripts политикой конкретного проекта.

## Поток сборки и запекания

`BuildTools/cmake/stages/ScriptsAndBaking.cmake` подключает компиляцию скриптов к сборке проекта:

- `FO_ANGELSCRIPT_SCRIPTING` включает command target `CompileAngelScript`.
- Target запускает AS compiler app проекта (`${FO_DEV_NAME}_ASCompiler`) с аргументами основной конфигурации.
- `CompileAngelScript` зависит от `ForceCodeGeneration`, поэтому видимые скриптам сгенерированные метаданные актуальны до компиляции.
- `FO_MONO_SCRIPTING` подключает `CompileMonoScripts` через `BuildTools/compile-mono-scripts.py`, передаёт `FO_OUTPUT_PATH` как обязательный каталог scripts/project и задаёт `FO_MONO_ASSEMBLIES`; `FO_MONO_SOURCE` остаётся списком dependency и metadata CMake.
- `BakeResources` и `ForceBakeResources` также зависят от code generation и запускают baker app проекта.

Компиляция скриптов и запекание ресурсов находятся рядом, но не тождественны. Компиляция создаёт bytecode и runtime inputs; baking упаковывает ресурсы и метаданные для runtime. Запекание ресурсов описано в [Baking Pipeline](../content-pipeline/baking.md).

## Корни Mono и native scripting

`Source/Scripting/Mono/` содержит файлы поддержки C#, включая `AssemblyInfo.cs`, `BasicTypes.cs`, `Entity.cs`, `Initializator.cs`, `MapSprite.cs` и `Link.xml`. BuildTools может подключить компиляцию Mono при включённом `FO_MONO_SCRIPTING`.

`Source/Scripting/Native/` сейчас содержит `.keepalive`, обозначающий расположение source root для интеграции native scripting. Не документируйте Native или Mono как эквивалент AngelScript runtime, пока не расширены реализация и тесты.

## Тесты для изучения

Поведение скриптов покрывают специализированные тесты:

- `Source/Tests/Test_AngelScriptAttributes.cpp` — разбор атрибутов, nullable-суффиксы, events, remote calls и правила callback.
- `Source/Tests/Test_AngelScriptBaker.cpp` — путь запекания bytecode и ресурсов AngelScript.
- `Source/Tests/Test_AngelScriptBytecode.cpp` — компиляция и загрузка bytecode.
- `Source/Tests/Test_AngelScriptCall.cpp` — ABI вызовов native/script и lifetime возвращаемых объектов.
- `Source/Tests/Test_ClientDataValidation.cpp` и `Test_NetBuffer.cpp` — проверка входящего payload remote calls и framing.
- `Source/Tests/Test_CommonScriptMethods.cpp` — общие экспортируемые методы.
- `Source/Tests/Test_EntityLifecycle.cpp` и `Test_EntitySync.cpp` — границы жизненного цикла и серверной синхронизации.
- `Source/Tests/Test_MetadataBaker.cpp` — грамматика запечённых метаданных events и remote calls.
- `Source/Tests/Test_ServerScriptMethods.cpp` — экспортируемые серверные методы.
- `Source/Tests/Test_ScriptBuiltins.cpp` — встроенные script helpers и types.
- `Source/Tests/Test_ScriptEntityOps.cpp` — взаимодействия script/entity.

Используйте эти тесты как исполняемую документацию при изменении регистрации скриптов, сгенерированных wrappers, сигнатур методов, nullability, объявлений событий или dispatch remote calls.

## Маршрутизация изменений

- Независимый от backend call ABI: `Source/Common/ScriptSystem.*`.
- Lifecycle compiler/runtime AngelScript: `Source/Scripting/AngelScript/AngelScriptScripting.*` и `AngelScriptBackend.*`.
- Построение script module, соглашения исходного кода, formatting, владение сгенерированными файлами и gates рефакторинга: [Стиль AngelScript и рефакторинг](../../how-to/scripting/style-and-refactoring.md).
- Синтаксис атрибутов и nullable preprocessing: `Source/Scripting/AngelScript/AngelScriptAttributes.*` и [Nullability.md](../../../Nullability.md).
- Регистрация сущностей и свойств: `Source/Scripting/AngelScript/AngelScriptEntity.*` и [модель сущностей](../entity-and-property-model/).
- Регистрация и dispatch remote caller: `Source/Scripting/AngelScript/AngelScriptRemoteCalls.*`, [удалённые вызовы](../../reference/scripting/remote-calls.md) и [сеть](../authority-and-networking/).
- Reflection helpers: `Source/Scripting/AngelScript/AngelScriptReflection.*`.
- Нативные экспортируемые методы: `Source/Scripting/*ScriptMethods.cpp` и [карта методов скриптового API](../../reference/script-api/method-ownership.md).
- Подключение build targets: `BuildTools/cmake/stages/ScriptsAndBaking.cmake` и [конвейер BuildTools](../../reference/cmake-and-buildtools/pipeline.md).
- Сгенерированные метаданные и codegen: [GeneratedApiAndMetadata.md](../../reference/metadata/index.md).

## Контрольный список проверки

1. Если изменились сигнатуры или аннотации, перегенерируйте код и проверьте diff сгенерированных метаданных и wrappers.
2. Скомпилируйте AngelScript через target `CompileAngelScript` подключающего проекта или эквивалентный AS compiler app.
3. Запустите самые узкие затронутые script tests, начиная по необходимости с `Test_AngelScriptAttributes`, `Test_CommonScriptMethods`, `Test_ServerScriptMethods`, `Test_ScriptBuiltins` и `Test_ScriptEntityOps`.
4. Для nullable-изменений выполните analyzers, описанные в [Nullability.md](../../../Nullability.md).
5. Для изменений server/client/mapper methods проверьте runtime-путь стороны-владельца; одной компиляции недостаточно.
6. Обновляйте [карту методов скриптового API](../../reference/script-api/method-ownership.md) при добавлении, удалении или содержательной перегруппировке файлов экспортируемых методов.
