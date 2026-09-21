---
layout: default
title: Скрипты Managed C#
locale: ru
document_id: managed-csharp-scripting
permalink: /Docs/ru/how-to/scripting/managed-csharp.html
---

# Скрипты Managed C#
<!-- docs-translation: {"document_id":"managed-csharp-scripting","locale":"ru","source_path":"Docs/en/how-to/scripting/managed-csharp.md","source_sha256":"fe58d5395276a40f9b70cd5d3ee1ebb387d4278a612e0ba4ced145f7d66943fd"} -->
> Документация движка. Это руководство описывает переиспользуемый backend Managed C#, его контракт authoring, сгенерированный API, lifecycle, синхронизацию, сборку, доставку и проверку. Игровые модули и политика конкретного проекта принадлежат подключающему проекту.

## Статус контракта

Managed C# является реализованным скриптовым backend для сервера, клиента, Mapper, bakers и тестов. Он встраивает Mono, компилирует проектные скрипты настроенным .NET SDK и предоставляет те же backend-neutral метаданные движка, которые использует AngelScript. Native scripting остаётся нереализованной заготовкой source root и не является равноценным третьим backend.

Подключающий проект выбирает скриптовый backend при конфигурации. Для полностью managed-проекта включите `FO_MANAGED_SCRIPTING` и отключите `FO_ANGELSCRIPT_SCRIPTING`. Наличие каталога исходников само по себе не включает backend: project configuration, resource packs, generated metadata, script sources, packages и tests должны совпадать.

Используйте это руководство вместе со следующими страницами:

- [Скриптовый runtime](../../explanation/scripting-runtime/) — backend-neutral façade и сравнение с AngelScript;
- [Жизненный цикл и конкурентность скриптов](lifecycle-and-concurrency.md) — общие правила lifecycle для обоих реализованных backend;
- [Удалённые вызовы](../../reference/scripting/remote-calls.md) — wire contract;
- [Сгенерированный API и метаданные](../../reference/metadata/) — граф зависимостей от исходника до generated output;
- [Упаковка](../release/packaging.md) и [Client Updater](../../explanation/runtime/client-updater.md) — доставка.

## Владение и layout исходников

Движок владеет следующими поверхностями:

- `Source/Scripting/Managed/ManagedRuntime.*` — process-wide запуск Mono и подключение native threads;
- `ManagedScriptBackend.*` — экземпляр backend, assembly load context, marshalling, callbacks, events, remote calls, exceptions и shutdown;
- `ManagedPInvokeTable.*` и `BuildTools/generate_pinvoke_table.py` — регистрация native interop shims;
- `CoreScripts/*.cs` — переиспользуемое пространство имён `FOnline`, attributes, helpers синхронизации, async scheduler, invoke/remote-call bridge, verify и helpers value types;
- `ManagedHost/ManagedLoadContextHost.cs` — изоляция assemblies для каждого backend;
- `Analyzers/FOnline.Analyzers.csproj` — compile-time анализ synchronization cover сущностей;
- `Source/Tools/ManagedScriptBaker.*` — генерация project/API и assemblies.

Подключающий проект владеет игровыми скриптами, своим пространством имён, проектными attributes и registrars, высокоуровневыми библиотеками вроде GUI object model, выбором resource packs, target framework, analyzers, tests и release qualification. Движок больше не поставляет прежнюю высокоуровневую библиотеку AngelScript CoreScripts; перенос её копии в Engine под C# нарушил бы ту же границу владения.

## Настройка backend

CMake-переключатель — `FO_MANAGED_SCRIPTING`. Он подключает managed runtime, backend, baker, tests CoreScripts и wiring приложений. `FO_NATIVE_SCRIPTING`, `FO_ANGELSCRIPT_SCRIPTING` и `FO_MANAGED_SCRIPTING` являются независимыми build options, но production-проект должен осознанно выбрать один gameplay backend, если он не проверяет cross-backend invocation.

Immutable startup settings группы `ManagedScript` определяют generated project:

| Настройка | Контракт |
| --- | --- |
| `ManagedScript.Assemblies` | Логические entry assemblies для сборки. |
| `ManagedScript.ProjectName` | Базовое имя generated solution и project. |
| `ManagedScript.TargetFramework` | Target framework generated SDK-style project. |
| `ManagedScript.MsBuild` | Команда сборки generated project. |
| `ManagedScript.Dirs` | Source roots для top-level `.cs`; обычно Engine CoreScripts и проектные скрипты. |
| `ManagedScript.GeneratedDir` | Необязательный каталог generated project; пустое значение выбирает `GeneratedSource/Managed` в build tree. |
| `ManagedScript.ExtraSources` | Дополнительные inputs вида `assembly,target,path`. |
| `ManagedScript.ExtraReferences` | Дополнительные references вида `assembly,target,reference`. |
| `ManagedScript.Analyzers` | Проекты Roslyn analyzer, включённые в generated build. |
| `ManagedScript.AnalyzerPackages` | NuGet packages Roslyn analyzer в виде точных пар `name,version`. |
| `ManagedScript.AdditionalFiles` | Файлы конфигурации analyzer, передаваемые как MSBuild `AdditionalFiles`. |
| `ManagedScript.AnalysisLevel` / `AnalysisMode` | Необязательные overrides analysis level/mode SDK. |
| `ManagedScript.BakerDryRun` | Структурный режим baker для тестов; он не доказывает наличие исполняемых assemblies. |
| `ManagedScript.DeepTrackEntityWrappers` | Opt-in shutdown diagnostics с именами живых entity wrappers; обычный счётчик активен всегда. |

Добавьте resource pack с `Managed` в списке `Bakers`. Inputs этого pack должны включать Engine `CoreScripts`, project script roots и источники `///@` metadata, которые нужны этим скриптам. Выбор assembly, target, pack и metadata является единым контрактом: сборка отдельного project, отличающегося от входов baker, не является проверкой движка.

## Generated project и assemblies

`ManagedScriptBaker` создаёт target API files, один SDK-style project и solution в настроенном generated directory. Generated filenames содержат `.gen`, auto-generated banner и собственную nullable directive. Устаревшие generated files, которых нет в новом наборе, удаляются. Не коммитьте и не редактируйте эти outputs вручную, если подключающий проект явно не считает конкретный generated input авторским.

Generated project включает nullable analysis, warnings as errors, стиль движка, настроенные analyzers и sources/references выбранных assembly и runtime target. Генерация API покрывает:

- Engine settings, enums, value types, entities, prototypes, fixed types и dynamic ref types;
- scalar, array, list, dictionary и поддерживаемые dict-of-list формы properties;
- methods, overload identities, mutable arguments, callback delegates и events;
- sender facades remote calls и регистрацию inbound handlers;
- native ref-type wrappers с явным управлением ссылкой, когда borrow живёт дольше вызова.

Неподдерживаемая форма type/member останавливает baking через `ManagedScriptBakerException`; baker не должен создавать placeholder, который упадёт только при выполнении gameplay.

Compiled entry assemblies зависят от target, например `<Pack>.Server.dll`, `<Pack>.Client.dll` и `<Pack>.Mapper.dll`. Они записываются в `Assemblies/<Target>Assemblies/` внутри baked pack. Helpers и dependencies остаются рядом с entry assembly.

## Форма авторского кода

Используйте пространство имён подключающего проекта и импортируйте `FOnline` для generated Engine types. Engine CoreScripts используют file-scoped `namespace FOnline;`; игровые domain types не должны находиться там.

Считайте nullable reference types частью контракта script/native. Generated reference/entity API различает nullable и non-nullable значения. Ожидаемое отсутствие сужайте обычной веткой; нарушение инварианта проверяйте через `Game.Verify` или `Game.VerifyNotNull`. Managed reference на сущность движка не владеет её persistence или lifetime.

Не храните изменяемое process-wide состояние. У каждого backend изолированный load context, но gameplay state всё равно принадлежит `Game`, entity, property, ref type или другому явному per-engine owner. Static constants и immutable type metadata допустимы; mutable cache, registry, timer gate или collection не получают правильное владение только потому, что записаны в C# static field.

Используйте фиксированные сообщения исключений и передавайте динамические значения как context через Engine verification helpers. Не превращайте hardcoded текст исключения или лога в player-visible строку; локализация остаётся проектной.

## Инициализация и attributes

`Initializator.InitializeEarly` выполняется до обычной module initialization. Он отвергает каждый объявленный `async void` method, регистрирует Engine attributed functions и remote calls, затем запускает проектные методы `[ScriptFuncRegistrar]`. Registrar должен быть static, parameterless и возвращать `void`; он нужен для проектных attributes, доступных bake-time reflection.

`Initializator.Initialize` запускает static constructors, находит методы `[ModuleInit(priority)]`, сортирует их по возрастанию priority и вызывает. Module initializer должен быть static, parameterless и возвращать `void` или `Task`. Результат `Task` ожидается в private synchronous continuation context; null task является ошибкой.

Managed marker attributes соответствуют ролям Engine dispatcher, а не обычным direct calls:

- `[Event]` для event handlers;
- `[TimeEvent]` для callbacks time events;
- `[PropertyGetter]` и `[PropertySetter]`;
- `[ServerRemoteCall]`, `[ClientRemoteCall]` и `[AdminRemoteCall]`;
- `[ItemTrigger]`, `[ItemInit]`, `[ItemStatic]`, `[CritterInit]`, `[MapInit]` и `[LocationInit]`;
- `[AnimCallback]`, `[ClassExtension]` и зарегистрированные проектом markers;
- `[CallableByName]` для functions, намеренно доступных named invocation.

Generated event wrappers отвергают handler без `[Event]` до создания native subscription. Named calls также закрыты по умолчанию: `Game.Invoke` и administrative dispatch принимают только явно отмеченные functions и подходящий allowlist.

## Events, callbacks, timers и named calls

Generated event wrappers поддерживают handlers, возвращающие `void`, `Task`, `EventResult` или `Task<EventResult>`. Native dispatch ожидает tasks с результатом до решения о продолжении subscriber chain. Используйте `Task<EventResult>`, если handler после await может употребить или уничтожить argument, который не должны получить последующие subscribers.

Time-event API принимают synchronous delegates и generated async delegates, возвращающие `Task`. Identity delegate является частью регистрации: остановка time event отдельно созданным delegate не находит существующий callback. Repeat/cancel меняет будущее расписание и не отменяет уже запущенный task.

Entity-only post-set реакции могут возвращать `Task`; value-transforming setters с `ref` arguments и property getters остаются synchronous, потому что native caller нужен результат до возврата.

Inbound remote-call handlers могут возвращать `void`, `Task` или `Task<T>`. У remote call нет wire result, поэтому незавершённые tasks наблюдаются без блокировки network/client pump; значение `Task<T>` игнорируется. Named script function с non-generic `Task` использует ту же async boundary. `Task<T>` named function остаётся synchronous, когда native code нужен `T`.

## Async и планирование continuations

`Game.YieldAsync(milliseconds)` — managed-аналог приостановки скрипта. Он завершается через Engine time event и продолжается через принадлежащий backend `ScriptSynchronizationContext`. Не используйте `async void`; возвращайте `Task` или `Task<T>`, чтобы движок мог наблюдать completion и faults.

Каждый backend владеет отдельной очередью continuation. `BaseEngine::FrameAdvance` обрабатывает только свои backend после освобождения frame-property lock. Каждая продолженная continuation снова входит в свой Engine через `RunScriptContext`; на сервере это создаёт новый synchronization context. Новая continuation ждёт следующего frame, поэтому yielding loop не может занять весь текущий frame.

Synchronous native-result callbacks и module initialization используют private continuation queue и обрабатывают только continuations собственного await. `Game.YieldAsync` в таком контексте запрещён, потому что заблокированный caller не может продвинуть timer pump. Уже completed tasks остаются допустимыми.

`ConfigureAwait(false)`, `Task.Run`, `Task.Factory`, `Parallel` и ручная отправка работы в ThreadPool намеренно обходят Engine synchronization context. Там можно выполнять изолированные вычисления, но нельзя вызывать Engine API. Привязка entry assembly к backend всё ещё позволяет определить Engine из такого thread, поэтому ошибка не обязана проявляться на каждом native call; надёжно её отклоняют только чувствительные к синхронизации маршруты, например server entity access. Подключающий проект должен статически запрещать эти escape API и возвращаться в захваченный Engine context до обращения к состоянию движка.

## Серверная синхронизация сущностей

Контракт server cover не зависит от backend: script caller должен покрыть все существующие сущности, которые native call graph может читать или изменять. `await` завершает прежний cover; перед повторным использованием заново найдите или проверьте сохранённую сущность и снова получите cover.

Managed scripts объявляют и доказывают этот контракт через:

- `[RequiresCover]` на parameter или receiver method;
- `[ProvidesCover]` на parameter или return value, устанавливающем cover;
- `[PreservesCover]` на awaitable helper, восстанавливающем cover caller;
- `CoverReach.Parent`, `Ancestors` и `DestroyGraph` для transitive requirements;
- CoreScript helpers `Sync` как единственные обычные wrappers над raw `Game.Sync`, `SyncRelease`, `Lock` и `Unlock`.

Roslyn analyzer сообщает invalid annotations (`FOSYNC001`), неудовлетворённый transitive cover (`FOSYNC002`), отсутствующие declarations entry point (`FOSYNC003`), probing вместо acquisition (`FOSYNC004`), raw synchronization calls вне helper (`FOSYNC005`) и использование cover без нового доказательства после `await` (`FOSYNC009`). `FOSYNC006` и `FOSYNC007` удалены: используйте `using GameLock scope = GameLock.Acquire();`; его `ref struct` scope освобождает lock на любом пути и не может пережить `await`. Подключите analyzer через `ManagedScript.Analyzers` или `ManagedScript.AnalyzerPackages` и считайте warnings ошибками сборки.

Provider inference сначала доказывает, что candidate call выполняется на каждом
returning path, и лишь затем обходит его callees. Call внутри conditional branch
не может доказать cover; такой порядок также не позволяет dense conditional call
cycles разрастаться экспоненциально. Analyzer self-tests ограничивают этот graph
по времени и по-прежнему требуют `FOSYNC009` для uncovered use после `await`.

`Sync.Acquire` расширяет связанный cover на месте через `Game.SyncWiden`, не освобождая и не захватывая заново уже покрытые сущности. Нативный cover остаётся непрерывным при проходе по отношениям `[SyncWiden]`, без race window между двумя наборами.

Attributes являются доказательством, а не операцией блокировки. Entry point отмечает entity, которую Engine уже синхронизировал. Обычный helper получает нужный cover или распространяет `[RequiresCover]` на caller.

## Значения, коллекции, properties и lifetime

Bridge преобразует поддерживаемые primitives, enums, strings, `hstring`, value types, entities, ref types, lists, dictionaries, delegates, mutable arguments и return values через Engine metadata. Зарегистрированный value type является plain packed data: каждое поле — primitive, enum, `hstring` или single-field value type; offset каждого поля выровнен по его размеру; полный размер не содержит tail padding; native twin trivially copyable и имеет тот же размер. Metadata registration отклоняет остальные формы. Generated C# structs используют sequential layout, а backend проверяет Mono value size до копирования байтов.

Generated entity properties имеют native backing. Dynamic ref types — managed DTO, значения которых материализуются из native property storage или присваиваются туда. Getter возвращает detached structured state; сохраняйте изменение через read-modify-reassign, если generated member сам не является live wrapper.

Native ref types являются явными borrowed wrappers. Если проект хранит один после вызова/frame, следуйте generated контракту `__AddRef()`/`__Release()`. Factory-backed wrapper начинает с reference, которую нужно освободить после передачи владения или detach.

`hstring` — восьмибайтовое blittable value с указателем на native intern entry. Frames и value types копируют этот указатель без преобразования; только property/RPC storage хранит 64-bit hash и преобразует его на границе storage. Значение интернируется через Engine metadata, привязанные к entry assembly, и разрешает текст именно из этой записи, без process-wide hash fallback между экземплярами Engine. Static managed fields всё равно инициализируются отдельно в каждом load context.

Массивы primitives, enums, `hstring` и зарегистрированных value types проходят как raw bytes через `GetPropertyList<T>` / `SetPropertyList<T>`. Длинный read повторяется прямо в итоговый storage списка при сохранённом cover. Strings, dictionaries, dynamic ref types, nullable proto/fixed-type values и другие structured forms остаются на converting bridge, но generated access выбирает property по registrar index и не передаёт имена owner/property повторно.

### Indexed native interop ABI

`ManagedScriptBaker` и native backend используют общий `ManagedInteropAbi`: manifest плотных ids methods, events, settings и inner entities плюс content hash. Generated bind stubs `*Abi.gen.cs` вызывают `Native.BindAbi` из `Initializator.InitializeEarly`; несовпадение hash или count останавливает load до выполнения scripts. Generated ABI files входят в incremental bake stamp, поэтому generator-only change не может опубликовать новые wrappers со старой assembly.

Indexed path покрывает primitives, enums, `hstring`, зарегистрированные value types и by-value handles entity/proto/fixed/ref types. Methods используют `CallMethodIndexed`, подходящие events — `FireEventIndexed`, numeric/bool settings — `GetSettingValue<T>`, а inner entities собираются одним snapshot `FillInnerEntities` вместо `Count` и серии indexed lookups. Complex signatures используют соответствующий boxed path с тем же плотным id. Nullability входит в manifest: non-nullable handle slot отклоняет zero, nullable handle может нести zero, а dynamic ref types, by-ref handles и results abstract/base entity остаются boxed там, где нужен runtime type.

Managed frames являются compact packed buffers, но native code не разыменовывает unaligned slot. `BuildManagedAbiNativeFrame` копирует inputs/result slots в aligned stack storage, native dispatch работает с ним, а `CopyBackManagedAbiNativeFrame` возвращает только mutable arguments и result. Event adapter возвращает `EventResult` через trailing `ref int`, копирует by-ref arguments после вызова и не boxing-ит result.

Native-to-managed callbacks, сигнатура которых состоит только из fixed values и handles entity/ref type, используют generated методы `CallbackAdapters.Adapt_<key>`. Один `ManagedCallbackPlan` разрешает adapter при регистрации; wrapper factories и native wrapper classes также регистрируются/кэшируются при ABI bind, поэтому dispatch не повторяет reflection и поиск constructor. Неподдерживаемые callback shapes остаются на boxed `MonoArray`/`DynamicInvoke`. Entity event subscriptions принадлежат native entity, а не одному wrapper: equal handler регистрируется идемпотентно, любой wrapper этой entity может его отписать, destruction удаляет subscriptions.

Каждый generated тип entity wrapper принадлежит одному load context backend. Поэтому внутри этого типа для equality и hashing достаточно native pointer; wrappers разных экземпляров Engine имеют разные runtime types. Живой wrapper проверяет `Native.IsBackendAlive` до выдачи pointer. После того как shutdown отвязывает entry assembly, поздний access бросает `ObjectDisposedException`, а поздний finalizer намеренно сохраняет native reference вместо обращения к уже освобождённому состоянию Engine.

Backend-owned caches строятся до hot-path use: managed helper methods, classes по metadata name, accessors dynamic ref type, wrapper constructors, callback adapters, list factories и per-event adapters. Typed custom setting хранит parsed cell за `GlobalSettings::GetCustomSettingsGeneration()`; каждый writer custom settings увеличивает generation, а warmed read выполняет только сравнение generation и копирование value. `ScriptSynchronizationContext` так же создаёт continuation queue только при первом post.

## Runtime loading, изоляция и shutdown

Mono инициализируется один раз на процесс. Первый managed entry на native worker Engine присоединяет thread к root domain и кэширует attachment на весь lifetime thread. Последующие entries только переводят attachment в GC-unsafe на время managed execution и снова паркуют его GC-safe во время native work или ожидания locks; reentrant entries наследуют attachment. Thread, инициализирующий Mono, является исключением: `mono_jit_init_version` присоединяет его неявно, а initialization scope освобождает принятый attachment. Затем backend создаёт собственный non-collectible `AssemblyLoadContext`; это граница per-engine isolation, потому что embedded runtime не предоставляет пригодный unload classic AppDomain.

До выполнения кода или type initializer из entry assembly backend вызывает `Native.BindBackend` со своим pointer. Каждый internal call, которому нужно состояние Engine, явно передаёт этот bound pointer, поэтому static constructors, marshalling constructors, callbacks и continuations определяют правильный Engine без thread-local caller state. Binding определяет только владельца; он не создаёт script synchronization context или server entity cover.

При запуске baked assemblies восстанавливаются в content-hashed подкаталоги writable `Cache/ManagedAssemblies/`. Уже совпадающие по байтам файлы переиспользуются, поэтому параллельные in-process Engine instances не перезаписывают загруженную Mono assembly. Отсутствие managed assemblies допустимо для tests/tools без baked scripts; настроенный gameplay project должен считать его ошибкой package или resource selection.

Shutdown сначала вызывает `BeginManagedTeardown`, который до любой другой очистки выполняет `Native.BeginBackendTeardown` и делает `Native.IsBackendTearingDown` истинным, пока backend ещё bound. Так wrapper, завершённый во время обычного runtime, отличается от wrapper, ставшего недостижимым из-за самого teardown. Wrapper с thread-affine native resource, который нельзя освободить из finalizer thread, в последнем случае может не сообщать о leak, потому что владеющая Engine subsystem уже уничтожается. `Native.IsBackendAlive` не позволяет провести это различие: unbind намеренно остаётся более поздним шагом, чтобы собранные во время shutdown entity wrappers ещё могли вернуть свои native references.

Затем shutdown закрывает scheduler continuations и удаляет queued work до освобождения backend state. Он очищает project static references и persistent callback roots, выполняет ограниченные collect/finalizer passes, пока Engine и assembly images ещё существуют, сообщает оставшиеся entity wrappers (и называет их при deep tracking), затем вызывает `Native.UnbindBackend` для каждой entry assembly до освобождения load scope и native global data. В native runtime ожидание finalizers выполняется на запрошенной Engine pool task с отдельным бюджетом пять секунд, чтобы заблокированный finalizer не остановил teardown thread навсегда. Timeout или оставшиеся wrappers являются diagnostics, и teardown продолжается; wrapper, завершившийся после unbind, не должен освобождать reference через мёртвое native state.

Single-threaded browser runtime не имеет пригодных managed thread pool и finalizer thread. Поэтому browser shutdown выполняет один inline collect/wait pass; `GC.WaitForPendingFinalizers()` возвращает сразу, finalizers позднее выполняются как main-thread jobs, а промежуточное число wrappers сообщается, но не проверяется. Поздний post не может выполниться на disposed Engine. Managed exceptions учитываются и логируются общим script exception path; deferred task fault наблюдается один раз. Managed frames и вложенные managed causes встраиваются в общий native stack trace, а native exceptions при проходе через managed code сохраняют identity через GC handles.

## Сборка и baking

Generated CMake target `CompileManagedScripts` запускает standalone `<ProjectDevName>_ManagedScriptBaker`. Он зависит от `ForceCodeGeneration`, загружает project configuration, готовит metadata, генерирует managed API/project, включая `*Abi.gen.cs`, и компилирует target assemblies без полного resource bake. Generated API files входят в assembly stamp.

`BakeResources` и `ForceBakeResources` запускают baker `Managed` внутри выбранного resource pack. Используйте compile target для быстрой проверки source/API, а bake target — для реального контракта resources, assemblies, runtime payload и metadata. После force bake выполните обычный incremental bake и потребуйте clean settle.

Runtime toolchain готовит `SetupManagedRuntime`; `PrepareManagedRuntimePayload` создаёт deployable subset и `runtime.manifest`. Setup выполняется в изолированном environment, чтобы локальные `DOTNET_*`, NuGet или SDK settings не меняли опубликованный runtime незаметно. Runtime source build отключает live NuGet advisory audit: reproducible dependency set задаёт pinned source revision, а не более позднее обновление feed. Перед каждым runtime build BuildTools удаляет target-dependent repo-local tasks semaphore dotnet, чтобы переход от desktop build к Android не переиспользовал неполный набор tasks. Настроенный workspace cache хранит только проверенное published runtime tree под target/toolchain-specific ключом; локальные runtime source checkouts не публикуются, неполный cache hit пересобирается, а stale SDK bootstrap без соответствующего shared runtime удаляется перед повтором setup.

## Packaging и updating

Prepared runtime содержит только managed class libraries, на которые реально ссылаются target assemblies, включая `System.Private.CoreLib.dll`; native runtime libraries, JIT binaries, headers, import libraries и symbols исключены из resource payload. Mono и generated native interop table остаются linked в application. Target-specific class libraries берутся из published runtime этой цели, а не из host SDK.

Managed baker помещает prepared runtime под `ManagedRuntime/` в тот же resource pack, что игровые assemblies. Client packaging пересобирает этот pack из runtime payload точного application target. Server packaging размещает одну target-specific copy для каждого распространяемого client target под `PlatformBinaries/<target>/`; updater подменяет ею common pack для этого target. Несколько native variants могут разделять один updater target, хотя их независимо собранные эквивалентные CoreLib payloads различаются побайтно, поэтому packaging детерминированно выбирает наименее квалифицированную подходящую binary entry — обычно default Release build — вместо требования byte-identical payloads.

Runtime startup атомарно восстанавливает выбранный payload в `<CacheDir>/ManagedRuntime/<content-hash>/`, добавляет каталог class libraries в search path Mono и использует cached payload как source of truth. Unpackaged native development binaries также получают prepared payload рядом с executable; packaged native, Web и Android applications используют resource-pack copy.

Embedded payload по умолчанию использует invariant globalization, потому что `System.Globalization.Native` не поставляется. Проект со своей проверенной globalization native library может изменить environment до startup runtime.

## Платформы и sanitizers

Managed scripting подключён к build paths Windows, Linux, Android, WebAssembly, macOS и iOS, но Engine source-capable path не является project release claim. Проверяйте каждый shipping target с точным project resource pack, assemblies, runtime payload, startup, callbacks, async work, shutdown, packaging и update route.

Web использует Mono interpreter и Engine JavaScript glue планирования/entropy; interpreter thread остаётся attached до teardown. Поскольку interpreter не компилирует native entry points, managed callbacks используют `mono_runtime_invoke`; probe modes thunk и `UnmanagedCallersOnly` пропускаются, когда `RuntimeFeature.IsDynamicCodeCompiled` равен false. При наличии загружаются script PDB resources, чтобы managed stack traces сохраняли source information. Android и Apple targets используют target-specific runtime archives и class libraries. Нельзя переиспользовать prepared payload одного target для другого target или architecture.

Конфигурации MemorySanitizer и ThreadSanitizer запрещены с `FO_MANAGED_SCRIPTING`: embedded Mono и generated/JIT code не могут удовлетворить этим инструментам и иначе дают ложные failures. AddressSanitizer и поддерживаемые undefined/data-flow combinations всё равно требуют реальных managed build/runtime checks проекта.

## Диагностика и debugging

Managed backend передаёт фиксированный native context, managed exception text и stack information в общий script error path. Факт создания assemblies не доказывает startup или callback dispatch. Для qualification client/device/browser, где нельзя запустить native test suite, задайте `ManagedScript.InteropProbeOnStart = True`: startup логирует строку `INTEROP-TRANSPORT` для каждого условия и финальный summary.

`InteropProbe` сравнивает runtime invoke, classic thunk и `UnmanagedCallersOnly` transports там, где runtime их предоставляет, затем измеряет production dispatch и его части synchronization, attachment и overrun reporting. Каждая серия проверяет delivery/arguments и сообщает GC handles, metadata lookups, managed objects, wrapper construction и — под Tracy — native allocations per call. Counters thread-local и выключены вне measured stretch. Latency служит evidence для сравнения на quiet host, а не shared-CI threshold; allocation и delivery counts остаются hard assertions.

Когда включён `FO_TRACY`, backend устанавливает Mono profiler сразу после инициализации runtime и до выполнения любой entry assembly. Инструментация вызовов методов ограничена зарегистрированными images игровых assemblies и методами с metadata token; runtime plumbing и generated wrappers не попадают в call tree. JIT zones не фильтруются по image, потому что первое выполнение handler оплачивает компиляцию каждого достигнутого метода. Hook запрашивает `ENTER | LEAVE | EXCEPTION_LEAVE`, намеренно без `TAIL_CALL`: Mono может устранить self tail call без парного enter event, поэтому обработка такого уведомления как обычного leave закрыла бы зону caller. Exceptional или inlined leave закрывает per-thread stack зон до метода, названного Mono.

Имена методов и source locations один раз разрешаются в process-wide table, общую для всех managed backend, а затем читаются под shared lock. В именах зон нет parameter lists и запятых, потому что Tracy CSV hotspot exporter не заключает это поле в кавычки. Callbacks Mono profiler являются `noexcept` C-ABI boundaries и не должны разворачивать стек через JIT-generated code. Как читать эти зоны под entry `Script execution overrun`, описано в [профилировании](../quality/profiling.md#зоны-managed-скриптов).

Используйте generated solution/project для IDE navigation и Roslyn diagnostics. Native startup и P/Invoke отлаживайте на границе host process; managed behavior — через runtime logs и focused callbacks, если подключающий проект не поддерживает проверенный managed debugger attach. UDP debugger AngelScript не отлаживает C#, и его settings нельзя выдавать за managed debugger.

Первичные маршруты диагностики:

| Симптом | Что проверить сначала |
| --- | --- |
| Нет generated type/member | Metadata input, target selection и diagnostic `ManagedScriptBaker`. |
| Build видит старый API | Выбор generated directory и dependency `CompileManagedScripts`. |
| Assembly собрана, но runtime ничего не загрузил | Baked pack и `Assemblies/<Target>Assemblies/`. |
| Работает native, но не Web/Android | Target-specific runtime payload и platform build, а не output host SDK. |
| Continuation не продолжается | Захваченный `ScriptSynchronizationContext`, frame pump и уход в ThreadPool. |
| Native API падает после `await` | Liveness entity и заново полученный synchronization cover. |
| Callback не регистрируется | Обязательный marker attribute и точная generated delegate signature. |
| Package не находит framework type | `ManagedRuntime/runtime.manifest` и target-specific замена pack. |
| ABI bind падает до module initialization | Stale generated `*Abi.gen.cs`, несовпадение native manifest/hash или пропущенная managed rebuild. |
| Unsubscribe через wrapper оставляет callback активным | Ownership subscription на native entity и delegate equality; wrapper-local event state запрещён. |

## Матрица проверки

| Изменение | Обязательные доказательства |
| --- | --- |
| Managed CoreScripts или backend | C# format/style checks, CoreScripts tests, generated project build и focused native unit tests. |
| Generated API shape или native export | Codegen, managed baker, generated diff, API contract diff и tests обоих backend для общего контракта. |
| Attribute, event, callback, timer или named call | Managed reflection/registration test и owning native/runtime dispatch. |
| Async scheduler | `test_managed_async_callbacks.py`, tests isolation/frame pump и awaited gameplay path подключающего проекта. |
| Entity-cover contract | Tests Roslyn analyzer, warning-free managed build и owning synchronized server behavior. |
| Runtime/cache/thread attachment | Native tests baker/backend и повторный multi-instance startup/shutdown. |
| Indexed ABI, callback adapters или wrapper caches | `Test_ManagedScriptBaker`, aligned-frame/native backend tests, `InteropProbe.VerifyTransports`, allocation counters и runtime точного target. |
| Package или updater | Tests runtime payload/packaging, проверка точного target package, startup из artifact и update replacement. |
| Platform claim | Configure/build, target payload, process/device/browser smoke и project acceptance этой платформы. |

Как минимум запустите focused Python suites `BuildTools/tests/test_managed_*.py`, test project analyzer, CoreScripts tests, `Test_ManagedScriptBaker.cpp`, generated unit-test target подключающего проекта, `CompileManagedScripts` и затронутый bake/package/runtime path. Dry-run marker baker, успешный `dotnet build` и native process-start smoke доказывают разные слои и должны отчитываться отдельно.

## Миграция с AngelScript

Source port завершён только тогда, когда перенесены declarations, generated API use, lifecycle, callbacks, named calls, synchronization, tests, resource inputs, packages и runtime evidence. Удаление `.fos` без удаления AngelScript baker или добавления Managed pack оставляет проект без активных gameplay scripts.

Сохраняйте public metadata names, declarations remote-call wire, property layouts, persisted identifiers и behavior, если migration намеренно их не меняет. Backend-neutral metadata не должна меняться только из-за нового синтаксиса. Запускайте оба backend во временном сравнительном lane только по явному плану проекта; не допускайте случайно двух handlers одного event или remote call.

Замените AngelScript `[[ModuleInit]]`, `[[Event]]`, `[[Async]]`/`Yield` и динамические комментарии синхронизации на managed `[ModuleInit]`, `[Event]`, `Task`/`YieldAsync` и contracts cover attributes/analyzer. Портируйте Engine helpers по семантике, а не транслитерацией синтаксиса или сохранением no-op compatibility shims.

## Граница документации проекта

Каждый managed embedding project должен документировать:

- выбранные backend options, target framework, pin SDK/runtime, source roots, assemblies, analyzers и generated directory;
- project namespace/layout, generated inputs, formatter/style gates и открытие generated project;
- module catalog, authority boundaries, project attributes, higher-level libraries и owners mutable state;
- точные команды compile, bake, test, package, launch, browser/device и update;
- migration status, намеренно unsupported shapes, platform qualification и operational rollback.

Engine guide задаёт reusable behavior; документация проекта должна объяснять, как оно настроено и доказано в этом repository.

## Триггеры сопровождения

Сверяйте эту страницу и английский оригинал в том же изменении, когда меняются:

- `FO_MANAGED_SCRIPTING`, managed CMake targets, toolchain setup или структура generated project;
- настройки `ManagedScript.*` либо discovery/output `ManagedScriptBaker`;
- CoreScript attributes, marshalling shapes, generated wrappers, events, remote calls или named invocation;
- continuation scheduling, thread attachment, load-context isolation, exception accounting или shutdown;
- synchronization-cover attributes или diagnostics analyzer;
- состав runtime payload, cache restoration, packaging, updater substitution или platform support.

Также перегенерируйте затронутые CMake, helper-CLI, package, API, public-contract, translation, site, search и agent-delivery artifacts в документированном порядке зависимостей.

## Проверенные пути исходников

- `Source/Common/ScriptSystem.*`
- `Source/Common/Settings.inc`
- `Source/Scripting/Managed/`
- `Source/Tools/ManagedScriptBaker.*`
- `Source/Applications/ManagedScriptBakerApp.cpp`
- `BuildTools/cmake/stages/Init.cmake`
- `BuildTools/cmake/stages/Applications.cmake`
- `BuildTools/cmake/stages/ScriptsAndBaking.cmake`
- `BuildTools/cmake/stages/ThirdParty.cmake`
- `BuildTools/cmake/stages/Packages.cmake`
- `BuildTools/managed_runtime_payload.py`
- `BuildTools/package.py`
- `BuildTools/tests/test_managed_*.py`
- `Source/Tests/Test_ManagedScriptBaker.cpp`
