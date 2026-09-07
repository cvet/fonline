---
layout: default
title: Стиль AngelScript и рефакторинг
locale: ru
document_id: angelscript-style
permalink: /Docs/ru/how-to/scripting/style-and-refactoring.html
---

<!-- docs-translation: {"document_id":"angelscript-style","locale":"ru","source_path":"Docs/en/how-to/scripting/style-and-refactoring.md","source_sha256":"1da17388dd4451ddab1c7c423acbf60f804e93325d80c89154d96fc5f01312ec"} -->

# Стиль AngelScript и рефакторинг

> Документация движка. Это руководство определяет переиспользуемый контракт исходного кода, форматирования, модулей и рефакторинга, поддержанный текущими компилятором, форматтером, CoreScripts и тестами FOnline. Проект игры владеет своей предметной лексикой, каталогом модулей, генерируемыми форматами проекта, игровой архитектурой и политикой миграций.

## Статус контракта

Это руководство для `current-revision`, а не обещание, что каждый исторический проект FOnline уже следует этим правилам. Нормативные утверждения выведены из текущего кода и тестов движка. `Source/Scripting/AngelScript/CoreScripts` содержит сопровождаемые примеры, а Last Frontier и FOnline TLA служат только сравнительными свидетельствами.

За подробными владельцами см. [объяснение scripting runtime](../../explanation/scripting-runtime/), [руководство по жизненному циклу и параллелизму](lifecycle-and-concurrency.md), [контракт nullability](../../contributing/coding-contracts/nullability.md) и [workflow сгенерированного содержимого](../build/generated-content.md). Эта страница отвечает за путь от изменения авторского `.fos` до проверяемого результата с сохранённым поведением.

## Область и владение

Используйте руководство при добавлении, перемещении, форматировании или рефакторинге AngelScript движка или проекта. Оно разделяет четыре задачи:

1. layout исходного кода и форматирование, которые могут обеспечивать инструменты движка;
2. правила модулей, сторон, атрибутов, изменяемого состояния и nullability, обеспечиваемые компиляцией или validation;
3. сгенерированные или чувствительные к совместимости имена, где сначала надо менять владельца;
4. политику проекта, которую нельзя представлять универсальным правилом движка.

Движок может определять контракт компилятора и форматтера. Он не может выбрать язык комментариев игры, игровую терминологию, декомпозицию сервисов, постоянную схему, ключи содержимого, тестовые сцены или пороги приёмки.

## Краткое соглашение

Перед изменением переиспользуемого или проектного скриптового кода:

- назначайте каждому авторскому файлу одно основное namespace, совпадающее с основой имени файла, например `Time.fos` и `namespace Time`;
- отделяйте заданный первой строкой порядок исходников `Sort N` от runtime-порядка `[[ModuleInit(priority)]]`;
- сохраняйте в namespace одну ясную ответственность и явно вызывайте другие модули как `Namespace::Function()`;
- изолируйте объявления сторон через `#if SERVER`, `#if CLIENT` или `#if MAPPER`; например, в server module задано `SERVER=1`, тогда как `#ifdef SERVER` истинно на каждой стороне, потому что все side macros всегда определены как `0` или `1`;
- сохраняйте авторитетную мутацию на владеющей стороне, а изменяемое состояние на владеющем объекте движка или сущности;
- используйте точные атрибуты функций и объявлений, которые требует dispatcher или generator;
- входите в принадлежащие dispatcher функции с атрибутами через маршрут dispatcher;
- по умолчанию оставляйте module globals только const: `Script.MutableGlobalsAllowedNamespaces` является узким compatibility escape hatch для явно владеемых legacy namespaces, а не разрешением на mutable state без владельца;
- записывайте nullable handles как `T?`, сужайте перед использованием и применяйте non-null тип, когда отсутствие не входит в контракт;
- изменяйте авторские входы и регенерируйте производные `.fos`, не исправляя сгенерированный результат вручную;
- классифицируйте изменение как mechanical, structural, behavioral или contract work; используйте небольшие узкие batches и добавляйте migration/compatibility proof для contract work;
- запускайте `python BuildTools/buildtools.py format-source`, компилируйте все затронутые стороны без warnings и выполняйте самый узкий тест, наблюдающий поведение;
- оставляйте каталог модулей, язык комментариев, игровую лексику и архитектуру, генерируемые проектом форматы, миграции persistence, fixtures и gameplay acceptance policy во встраивающем проекте. Engine не владеет каталогом модулей игры только потому, что предоставляет compiler или formatter.

Полное резюме границы Engine/project обязано сохранить оба правила, которые
легко теряются при сокращении: вход в принадлежащие dispatcher функции с
атрибутами выполняется только через маршрут dispatcher, а генерируемые проектом
форматы, миграции persistence и gameplay acceptance остаются политикой проекта.

Правило namespace-to-file является соглашением движка и подсказкой для поиска, а не грамматикой AngelScript. Сгенерированное или совместимое исключение следует закрепить в его generator или validator, не ослабляя значение по умолчанию.

## Как скрипты становятся модулем

### Обнаружение и порядок файлов

Backend получает настроенные скриптовые файлы, читает первую строку каждого файла и ищет `Sort N`. При отсутствии директивы используется значение сортировки `0`. Затем выполняется стабильная сортировка по возрастанию числового значения, а при равных значениях по основе имени файла. Сгенерированный корневой исходник включает каждый полученный файл.

Этот порядок может влиять на preprocessing и видимость объявлений. Сохраняйте `Sort N` в первой строке, когда он нужен, рассматривайте существующее значение как влияющее на поведение и не используйте его вместо явного порядка жизненного цикла. Порядком module initializer во время исполнения владеет `[[ModuleInit(priority)]]`; этот контракт описан в [руководстве по жизненному циклу](lifecycle-and-concurrency.md).

Обычно проекты задают скриптовые входы через интеграцию с движком. Авторским модулям не нужен вручную сопровождаемый граф include. Поэтому ручное изменение include или sort остаётся структурным, пока не скомпилированы все затронутые стороны и не прошёл соответствующий startup path.

### Раздельная компиляция сторон

FOnline выполняет preprocessing и компилирует отдельный модуль для каждой запрошенной стороны. Все три макроса стороны существуют в каждой компиляции и имеют значение `0` или `1`:

- server: `SERVER=1`, `CLIENT=0`, `MAPPER=0`;
- client: `SERVER=0`, `CLIENT=1`, `MAPPER=0`;
- mapper: `SERVER=0`, `CLIENT=0`, `MAPPER=1`.

Используйте проверки значения, например `#if SERVER`. Условие `#ifdef SERVER` истинно на каждой стороне и не изолирует server code. Сбалансированный preprocessor guard доказывает только лексическую структуру; компиляция каждой затронутой стороны доказывает корректность её объявлений, атрибутов и вызовов.

### Владение namespace и файлом

Сопровождаемые CoreScripts используют одно top-level namespace, совпадающее с основой каждого `.fos`:

```angelscript
// Time.fos
namespace Time
{

timespan Seconds(int value)
{
    return timespan(value, SecondsPlace);
}

}
```

Так участники, diagnostics, search и retrieval systems получают один маршрут от `Time::Seconds` к `Time.fos`. Файл может содержать target guards или private implementation helpers, но его основное публичное владение должно оставаться очевидным.

### Границы зависимостей и совместимости

Cross-namespace вызовы должны называть владельца. Переносите helper только когда его поведение и callers принадлежат принимающему модулю, а не ради размера или предпочтительного порядка.

Считайте следующие элементы контрактами, пока владеющие код и тесты не докажут иное:

- first-line sort directives и side guards;
- function attributes и dispatcher signatures;
- declaration metadata `///@`;
- имена remote-call subsystem и method;
- reflection strings, invoke names, property names, enum values, serialized identifiers и content keys;
- имена сгенерированных файлов и generator inputs.

Локальное переименование является механическим только когда ни одна из этих поверхностей не может его наблюдать.

## Контракт форматтера

### Поддерживаемая команда и версия

`Source/Scripting/AngelScript/CoreScripts/.clang-format` является определением layout для движка. Текущий контракт использует clang-format 20, четыре пробела, запрещает tabs, задаёт лимит 160 столбцов, переносит function braces на следующую строку, оставляет control braces на той же строке, добавляет braces телам control flow, не отступает тело внешнего namespace, не сортирует includes и не переформатирует comments.

Из репозитория движка выполните:

```bash
python BuildTools/buildtools.py format-source
git diff --exit-code
```

`format-source` форматирует поддерживаемые файлы внутри Engine `Source`; он не обнаруживает отдельное script tree использующего движок проекта. Проект должен применять документированный formatter к собственным `.fos` и отдельно запускать команду движка, когда изменён исходный код submodule.

BuildTools использует `FO_CLANG_FORMAT`, если он задан, иначе ищет `clang-format-20`, затем `clang-format`, и отклоняет binary, major version которого не равен 20. CI движка повторно запускает wrapper и требует пустой diff.

### Что исправляет wrapper

Raw clang-format разбирает `.fos` как C++ и может разделять специфические токены AngelScript. Wrapper маскирует strings, character literals, line comments и block comments, а после форматирования исправляет как минимум следующие формы:

- nullable declarations, например `Critter? target`;
- nullable parameters, return types, casts и template arguments;
- nullable array elements, например `Item?[]`;
- named arguments, например `Create(count: 2)`.

Используйте wrapper, а не raw clang-format. Formatter проекта может охватывать дополнительные авторские форматы, но его путь `.fos` должен сохранять эту семантику или делегировать эквивалентной логике движка.

### Кодировка, окончания строк и EOF

Wrapper читает и записывает UTF-8, удаляет UTF-8 BOM, сохраняет существующий выбор LF или CRLF и оставляет ровно один line terminator в EOF. Одна только нормализация окончаний строк не считается семантическим различием форматирования.

Не задавайте через это руководство общепроектное правило line endings. Этой политикой владеют repository attributes и formatter проекта. Гарантия движка состоит в сохранении входного соглашения форматируемого файла.

### Чего форматирование не доказывает

Форматирование не доказывает ownership namespace, authority стороны, согласованное поведение ролей, применение атрибутов, маршрутизацию callbacks, политику mutable globals, владение генерацией, поток nullability, совместимость serialization или runtime behavior. Просматривайте formatter output до завершения механической фазы.

## Layout исходного кода

### Порядок внутри namespace

Держите связанные объявления вместе и следуйте порядку окружающего модуля. Публичное владение важнее косметического единообразия: не переставляйте initialization, registration, callbacks или declaration metadata без проверки того, наблюдает ли consumer исходный порядок.

Не задавайте в документации движка универсальный порядок gameplay helpers. CoreScripts являются переиспользуемой основой, а игра может иначе группировать domain declarations и закреплять структуру проектными checks.

### Имена и комментарии

Переиспользуемая основа намеренно мала:

- имена namespaces и типов используют `PascalCase`;
- имена public functions следуют окружающему script API движка;
- local names выражают намерение и следуют окружающему модулю;
- compatibility names меняются только через владеющий API или migration process;
- comments объясняют намерение, invariants, ownership или неочевидное ограничение вместо пересказа следующей строки.

Движок не предписывает естественный язык комментариев игры, шаблон file header, единую лексику переменных NPC или items либо универсальный максимальный размер модуля.

### Изменяемое состояние и globals

После module build backend отклоняет каждый изменяемый module-level global, namespace которого не совпадает с настроенным префиксом в `Script.MutableGlobalsAllowedNamespaces`. Const globals принимаются. Поэтому пустой список по умолчанию требует const-only module globals.

Предпочитайте состояние, которым владеет engine instance, entity или явный lifecycle object. Если legacy project временно требует mutable globals, разрешайте только самый узкий namespace prefix, фиксируйте владельца и условие удаления и тестируйте startup на каждой стороне. Prefix matching означает, что широкая запись может разрешить больше namespaces, чем рассчитывал автор.

Allowlist является compatibility escape hatch, а не доказательством правильной области global cache или service. Module initialization и границей freeze globals по-прежнему владеет [руководство по жизненному циклу](lifecycle-and-concurrency.md).

## Nullability и invariants

Используйте `T?` только когда отсутствие входит в контракт. Bind или guard nullable value перед dereference. Используйте обычную ветвь для ожидаемого отсутствия и `verify(...)` для нарушенного invariant; сохраняйте message фиксированным описанием и передавайте динамические values как context arguments.

```angelscript
Critter? target = Game.GetCritter(targetId);
if (target == null) {
    return;
}

ApplyEffect(target);
```

Не размножайте defensive checks вокруг non-null values или только что переданных entity arguments. Повторно проверяйте сохранённые entity handles после реальной lifetime boundary, например `Yield`, callback, способного уничтожить или отсоединить entity, либо хранения за пределами текущего call. Деталями narrowing владеет [контракт nullability](../../contributing/coding-contracts/nullability.md), а suspension и synchronization владеет [руководство по жизненному циклу](lifecycle-and-concurrency.md).

## Атрибуты и владение callback

Attributes являются контрактами compiler и dispatcher, а не декоративными labels. Pipeline выполняет preprocessing, извлекает и связывает attributes, проверяет их применение и special forms, проверяет callback и remote-call contracts и только затем выпускает bytecode.

### Блокировка прямых вызовов

Встроенный direct-call-blocking набор включает `Event`, `TimeEvent`, `AnimCallback`, `PropertyGetter`, `PropertySetter`, `ServerRemoteCall`, `ClientRemoteCall`, `AdminRemoteCall`, `ItemTrigger`, `ItemStatic`, `ModuleInit` и `InvokeEntry`. Script function с одним из этих attributes должна вызываться через владеющий dispatcher или API, а не как обычный helper.

Проекты могут добавлять dispatcher-owned attributes через `Script.ExtraDirectCallBlockingAttributes`. `Script.AttributedFunctionDirectCallAllowedNamespaces` может по префиксу освободить caller namespaces ради совместимости. Сохраняйте оба списка узкими и временными; когда поведению действительно нужны direct и dispatched entry, предпочтительнее выделить обычный helper.

Validator также проверяет ownership callback API, включая event subscription, time-event APIs, animation callbacks и регистрацию property getter/setter. Attribute с правильным написанием, но неправильным маршрутом всё равно нарушает контракт.

### Распространение markers

Любой function attribute, не классифицированный как direct-call-blocking, validator вызовов считает marker. Если функция вызывает marked function, caller должен нести тот же marker. Типичный пример `[[Async]]`: propagation делает transitive suspension boundary видимой, не скрывая её внутри helper.

Не удаляйте и не добавляйте marker как formatting cleanup. Компилируйте всех callers и сверяйтесь с владеющей документацией lifecycle или attribute до изменения call graph.

### Сгенерированные объявления

Комментарии объявлений, например `///@ Event`, `///@ RemoteCall`, `///@ Property` и `///@ Enum`, питают generated metadata и script declarations. Измените авторское объявление, выполните regeneration в порядке зависимостей, просмотрите generated diffs, затем компилируйте затронутые стороны.

Function attributes и объявления `///@` решают разные части pipeline. Не заменяйте одно другим из-за похожих имён.

## Владение сгенерированными скриптами

Каждому сгенерированному `.fos` нужен upstream owner: Engine metadata/codegen, project GUI generator, content generator или другой объявленный tool. Исправляйте владельца и регенерируйте. Ручная правка derived output не завершает изменение, потому что следующий generation pass её сотрёт.

Перед изменением незнакомого файла проверьте repository instructions, generated headers, build tasks и [workflow сгенерированного содержимого](../build/generated-content.md). Когда generated failure виден только в derived code, сохраняйте достаточно source-located evidence для исправления input или generator.

## Классификация рефакторинга

Классифицируйте изменение до редактирования. Необходимое доказательство для пакета определяет самая рискованная затронутая поверхность.

### Механические изменения

Примеры: formatter output, исправление comment и local rename без reflected или serialized use.

Минимальное доказательство: запустить владеющий formatter, просмотреть diff и скомпилировать затронутые scripts без warnings.

### Структурные изменения

Примеры: extraction helper, перенос function, namespace move, разделение файла, перегруппировка side guards или изменение sort.

Минимальное доказательство: скомпилировать каждую затронутую сторону и запустить focused tests для module initialization, callbacks и перенесённых call paths. Убедиться, что generated и reflected ownership не изменились случайно.

### Поведенческие изменения

Примеры: condition, ordering, state mutation, callback result, authority decision или lifetime behavior.

Минимальное доказательство: focused runtime или gameplay tests и соответствующий integration path. Описывать изменение поведения отдельно от cleanup.

### Контрактные изменения

Примеры: attribute, имя или тип metadata, remote call, property, enum, persisted identifier, content key или generated schema.

Минимальное доказательство: regeneration, compatibility classification, migration или disposition при необходимости, compile и bake gates и runtime tests на обоих концах контракта.

## Безопасный workflow пакетов

### Определить владение

Прочитайте ближайшие repository instructions и owning docs. Определите, является ли каждый файл авторским или сгенерированным, какая сторона владеет поведением и пересекают ли имена process, persistence, reflection или content boundaries.

### Зафиксировать baseline

Запустите самый узкий существующий compile или test до широкого изменения. Запишите известные failures, не выдавая их молча за результат рефакторинга. Для migration инвентаризуйте временные allowlists, generated files и compatibility names до изменений.

### Менять один класс

По возможности отделяйте mechanical cleanup от structural, behavioral и contract changes. Малые пакеты позволяют связать failed compile, изменившийся callback order, отсутствующий side symbol или compatibility drift с одним решением.

Не удаляйте массово commented code и не переименовывайте strings, reflection tokens, metadata identifiers, serialized fields или content keys без определения владельца. Version control сохраняет старый текст, но не доказывает, что отключённый код устарел или имя не является контрактом.

### Регенерировать и форматировать

Запустите каждый owner generator, input которого изменился, затем правильный formatter для каждого authored tree. Не форматируйте generated file вместо исправления generator, если только contract генератора явно не включает такой formatting pass.

### Компилировать каждую сторону

Компилируйте SERVER, CLIENT и MAPPER везде, куда может попасть изменённый файл или metadata. Считайте warnings ошибками. Common-only изменение не доказано компиляцией одной удобной роли.

### Доказать поведение

Запустите самый узкий Engine unit test, project script test, scene или integration route, наблюдающий изменённое поведение. Если cleanup обнаруживает вероятный bug, либо докажите и исправьте его как отдельно описанное behavioral change, либо запишите точный follow-up.

## Матрица validation

| Изменение | Необходимое свидетельство |
| --- | --- |
| Исходный код Engine CoreScript | Engine `format-source`, чистый diff, компиляция затронутых scripts, focused Engine test |
| Авторский `.fos` проекта | Project formatter, `CompileAngelScript` или эквивалент для всех ролей, focused project test |
| Side guard или sort directive | Компиляция каждой затронутой стороны, startup или initialization test, когда важен порядок |
| Политика mutable globals | Startup compile/bake с точными namespace settings, lifecycle test, документированное условие удаления исключения |
| Function attribute или callback route | Attribute validation, владеющий dispatcher path, regression прямого вызова или propagation, когда применимо |
| Metadata `///@` или generated declaration | Regeneration, generated diff, compile/bake, compatibility review |
| Persisted, reflected, remote или content identifier | Contract disposition или migration и runtime tests producer/consumer |
| Широкий рефакторинг | Повторять применимые gates для каждого малого пакета; завершить aggregate validation проекта |

CI движка запускает `buildtools.py format-source` и `git diff --exit-code`. `BuildTools/tests/test_docs_angelscript_style.py` закрепляет маршрут документации, compiler ordering и side macros, настройки mutable globals и attributes, правила namespace/guard/encoding CoreScripts, formatter repairs, external evidence, localization и включение в workflow.

## Диагностика отказов

| Симптом | Первая проверяемая граница |
| --- | --- |
| Server declaration появляется на client или mapper | Заменить `#ifdef SIDE` на `#if SIDE`; проверить вложенность guards |
| Symbol появляется или исчезает после переноса файла | First-line `Sort N`, порядок filename при равном sort, namespace qualification |
| Mutable global отклонён после module build | Владеющий state object и точный prefix `MutableGlobalsAllowedNamespaces`; не расширять вслепую |
| Direct call attributed function отклонён | Войти через владеющий dispatcher или выделить обычный helper |
| Caller не имеет `[[Async]]` или другого marker | Распространить marker по реальному call chain и проверить lifecycle boundary |
| Formatter разделяет `?` или `:` named argument | Использовать Engine-aware wrapper и подтвердить clang-format major 20 |
| Изменение generated `.fos` исчезает | Изменить generator input или generator, затем регенерировать |
| Одна роль компилируется, другая нет | Компилировать failing side с её реальными макросами `0`/`1` и generated declarations |
| Рефакторинг меняет startup behavior | Разделить file sort, module-init priority, callback registration и runtime event order |

## Граница политики проекта

Использующий движок проект должен документировать то, что движок выбрать не может:

- каталог модулей и доменов, gameplay architecture и authority decisions;
- язык комментариев, terminology, headers и локальные naming additions;
- имена generated scripts, generator commands и project-only authored formats;
- serialized identifiers, migration approvals и compatibility windows;
- test harness, fixtures, launch profiles и gameplay acceptance;
- formatter coverage вне Engine `Source`;
- временные namespace exceptions и план их удаления;
- пороги file-size, commented-code и другие quality ratchets.

Узкие test-only исключения globals и project formatter Last Frontier являются полезным текущим свидетельством. Широкий production mutable-global allowlist и ongoing refactoring log TLA являются свидетельством миграции, а не рекомендацией движка. Ни один проект не является нормативной зависимостью этого руководства.

## Триггеры сопровождения

Повторно проверяйте эту страницу в том же изменении при изменении любого из владельцев:

- script discovery, first-line sorting, side macros, module construction или bytecode pipeline в `AngelScriptBackend.cpp`;
- direct-call blockers, marker propagation, callback validation или special attributes в `AngelScriptAttributes.*`;
- `Script.MutableGlobalsAllowedNamespaces`, `Script.AttributedFunctionDirectCallAllowedNamespaces` или `Script.ExtraDirectCallBlockingAttributes`;
- `.fos` patterns, discovery/version checks clang-format, repair logic, encoding, line-ending или EOF behavior в `BuildTools/buildtools.py`;
- контракта CoreScripts `.clang-format` или сопровождаемого layout CoreScripts;
- владения generated declarations или связанных с этим руководством контрактов lifecycle/nullability;
- external project evidence, используемых для отделения переиспользуемых правил от политики проекта.

Запустите focused documentation test, localization check, snippet check, site generation check и standalone documentation validator. Если изменилось поведение Engine `.fos`, также запустите formatter, скомпилируйте каждую затронутую сторону и выполните владеющие native/script tests.

## Проверенные исходные пути

- `BuildTools/buildtools.py`
- `.github/workflows/validate.yml`
- `Source/Common/Settings.inc`
- `Source/Common/ScriptSystem.cpp`
- `Source/Scripting/AngelScript/CoreScripts/.clang-format`
- `Source/Scripting/AngelScript/CoreScripts/*.fos`
- `Source/Scripting/AngelScript/AngelScriptBackend.cpp`
- `Source/Scripting/AngelScript/AngelScriptAttributes.cpp`
- `Source/Scripting/AngelScript/AngelScriptAttributes.h`
- `Source/Tests/Test_AngelScriptAttributes.cpp`
- `Source/Tests/Test_AngelScriptBaker.cpp`
- `BuildTools/tests/test_docs_angelscript_style.py`
