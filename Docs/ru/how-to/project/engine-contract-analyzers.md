---
layout: default
title: Анализаторы контрактов движка для проектов на Managed C#
locale: ru
document_id: project-engine-contract-analyzers
permalink: /Docs/ru/how-to/project/engine-contract-analyzers.html
---

# Анализаторы контрактов движка для проектов на Managed C#
<!-- docs-translation: {"document_id":"project-engine-contract-analyzers","locale":"ru","source_path":"Docs/en/how-to/project/engine-contract-analyzers.md","source_sha256":"PLACEHOLDER"} -->
`FOnline.Analyzers` проверяет исходники скриптов Managed C# на соответствие контрактам, которыми владеет движок: как вызываются точки входа с атрибутами, как пишутся вызовы помощников `Invariant` и какие статические поля может очистить проход при остановке. На этой странице перечислены эти правила и показано, как встраивающий игровой проект включает и настраивает их и как держит собственные правила стиля и именования в отдельном анализаторе рядом с ними. Правила покрытия сущностей (`FOSYNC*`) находятся в той же сборке и описаны в [Managed C# Scripting](../scripting/managed-csharp.md#серверная-синхронизация-сущностей).

## Проверенные пути исходников

- `Source/Scripting/Managed/Analyzers/EntryPointAnalyzer.cs`
- `Source/Scripting/Managed/Analyzers/InvariantAnalyzer.cs`
- `Source/Scripting/Managed/Analyzers/ScriptStaticAnalyzer.cs`
- `Source/Scripting/Managed/Analyzers/CoverVocabulary.cs`
- `Source/Scripting/Managed/Analyzers/FOnline.Analyzers.csproj`
- `Source/Scripting/Managed/Analyzers/AnalyzerReleases.Unshipped.md`
- `Source/Scripting/Managed/Analyzers/Acceptance/EngineContractProbe.cs`
- `Source/Scripting/Managed/Analyzers/Tests/EngineContractTests.cs`
- `Source/Scripting/Managed/Analyzers/Tests/FOnline.Analyzers.Tests.csproj`
- `Source/Scripting/Managed/CoreScripts/Attributes.cs`
- `Source/Scripting/Managed/CoreScripts/DatabaseDocument.cs`
- `Source/Scripting/Managed/CoreScripts/Invariant.cs`
- `Source/Scripting/Managed/CoreScripts/ScriptStaticCleanup.cs`
- `Source/Scripting/Managed/ManagedScriptBackend.cpp`
- `Source/Scripting/AngelScript/AngelScriptAttributes.cpp`
- `Source/Common/Settings.inc`
- `Source/Tools/ManagedScriptBaker.cpp`

## Правила

Каждое правило включено по умолчанию с уровнем `Warning`. Сгенерированный проект скриптов задаёт `TreatWarningsAsErrors=true`, поэтому даже ненастроенное правило уже проваливает `CompileManagedScripts` и сборку ресурсов Managed.

| ID | Что проверяет | По умолчанию | Владелец контракта в движке |
|---|---|---|---|
| `FOENTRY001` | Метод с атрибутом диспетчеризации не вызывается из обычного кода | Warning | Диспетчеры в `ManagedScriptBackend.cpp`; бэкенд AngelScript применяет такой же запрет в `AngelScriptAttributes.cpp` |
| `FOENTRY002` | Метод, который движок вызывает или находит через атрибут, объявлен `internal` | Warning | Поиск через отражение в CoreScripts и в бэкенде |
| `FOENTRY003` | Каждое имя в параметре `fonline_entry.*` разрешается в тип | Warning | Настройка двух правил выше |
| `FOVERIFY001` | Сообщение `Invariant.Verify` описывает инвариант, а не повторяет условие | Warning | `CoreScripts/Invariant.cs` |
| `FOVERIFY002` | Доказанное нарушение оформляется через `Invariant.Failed`, а не `Invariant.Verify(false, ...)` | Warning | `CoreScripts/Invariant.cs` |
| `FOVERIFY003` | Условие `Invariant.Verify` и значение `Invariant.VerifyNotNull` не выполняют работу | Warning | `CoreScripts/Invariant.cs` |
| `FOSTATIC001` | Ни одно статическое поле скрипта не остаётся вне досягаемости очистки при остановке | Warning | `CoreScripts/ScriptStaticCleanup.cs` |

Категории правил: `Dispatch` (`FOENTRY`), `Verification` (`FOVERIFY`) и `Lifetime` (`FOSTATIC`); правила `FOSYNC` используют `Synchronization`.

### Точки входа: FOENTRY001 и FOENTRY002

Атрибуты диспетчеризации: `[ModuleInit]`, `[PropertyGetter]`, `[PropertySetter]`, `[AnimCallback]`, `[ScriptFuncRegistrar]` и каждый атрибут, класс которого несёт `[EntryPointMarker]`: `[Event]`, `[TimeEvent]`, три маркера удалённых вызовов, `[ItemInit]`, `[ItemTrigger]`, `[ItemStatic]`, `[CritterInit]`, `[MapInit]` и `[LocationInit]`. Прямой вызов пропускает контекст, который готовит диспетчер, и лишает смысла объявление `[RequiresCover]` у точки входа, на которое опирается `FOSYNC003`. Подписка группой методов остаётся способом регистрации и не сообщается.

Атрибуты регистрации: `[CallableByName]`, `[CallableFromNative]`, `[CallableByEngine]`, `[PropertyMigrator]` и `[ProtoMigrator]`. Обычный вызов такого метода из C# допустим; проверяется только его уровень доступа.

`FOENTRY002` относится к обеим группам. Переопределения и реализации интерфейсов сохраняют доступ базового члена и не проверяются. Код, который `ManagedScriptBaker` генерирует в пространстве имён `FOnline`, намеренно держит закрытые члены для поиска и не проверяется; код, сгенерированный генератором проекта, проверяется.

```csharp
// Violation: FOENTRY002 (public entry point) and FOENTRY001 (direct call)
public static class Combat
{
    [Event]
    public static void OnDamaged(Critter cr) { ApplyDamage(cr); }

    public static void Heal(Critter cr) { OnDamaged(cr); }

    private static void ApplyDamage(Critter cr) { }
}
```

```csharp
// Fix: the entry point is internal, and ordinary code calls the shared helper
public static class Combat
{
    [Event]
    internal static void OnDamaged(Critter cr) { ApplyDamage(cr); }

    public static void Heal(Critter cr) { ApplyDamage(cr); }

    private static void ApplyDamage(Critter cr) { }
}
```

### Инварианты: от FOVERIFY001 до FOVERIFY003

Проверяются только вызовы, которые связываются с `FOnline.Invariant`. `FOVERIFY001` судит только сообщения-литералы: сообщение, равное условию (без учёта пробелов), или сообщение без пробелов, являющееся фрагментом условия, например `"cr.HasKit"`. Фраза, цитирующая границу, например `"Group size must be >= 1"`, принимается. `FOVERIFY003` сообщает об `await`, присваиваниях, инкрементах и декрементах, а также об аргументах `out` или `ref` внутри условия; аргументы контекста не проверяются.

```csharp
// Violations: FOVERIFY001, FOVERIFY002 and FOVERIFY003, in that order
public static class Building
{
    public static void Check(Critter cr, string text)
    {
        Invariant.Verify(cr.HasKit, "cr.HasKit");

        if (!cr.HasKit) {
            Invariant.Verify(false, "Builder must carry a construction kit");
        }

        Invariant.Verify(int.TryParse(text, out int count), "Count must be an integer");
    }
}
```

```csharp
// Fixes: a sentence plus context, Failed for a proved branch, the work before the check
public static class Building
{
    public static void Check(Critter cr, string text)
    {
        Invariant.Verify(cr.HasKit, "Builder must carry a construction kit", cr.Id);

        if (!cr.HasKit) {
            Invariant.Failed("Builder must carry a construction kit");
        }

        bool parsed = int.TryParse(text, out int count);
        Invariant.Verify(parsed, "Count must be an integer", text);
    }
}
```

### Статические поля скриптов: FOSTATIC001

Контекст загрузки скриптов не выгружаемый, поэтому статическое поле скрипта удерживает свой граф объектов всё время жизни процесса. При остановке `ScriptStaticCleanup` очищает каждое статическое ссылочное поле скриптовых и динамических сборок вне пространства имён `FOnline`. Три формы ему недоступны, и правило сообщает о них: статическое поле обобщённого типа (включая типы, вложенные в него), поле или событие-поле с `[ThreadStatic]` и статическое поле типа-значения, которое может содержать ссылку. Константы, неуправляемые типы-значения и вычисляемые свойства не имеют достижимого хранилища и не сообщаются. Типы в `FOnline` и его дочерних пространствах имён находятся вне и прохода очистки, и правила.

```csharp
// Violation: FOSTATIC001
public static class Cache<T>
{
    public static T? Last { get; set; }
}
```

```csharp
// Fix: the state lives behind an ordinary static reference field the sweep clears
public static class Cache
{
    private static readonly System.Collections.Generic.Dictionary<System.Type, object> Last =
        new System.Collections.Generic.Dictionary<System.Type, object>();
}
```

## Включение анализаторов

Укажите проект анализатора в основной конфигурации. Пути задаются относительно каталога конфигурации, элементы списка разделяются пробелами:

```ini
ManagedScript.Analyzers = Engine/Source/Scripting/Managed/Analyzers/FOnline.Analyzers.csproj
```

`ManagedScriptBaker` добавляет каждый элемент в сгенерированный проект как ссылку на анализатор. Изменение проекта анализатора, дополнительного файла или `.editorconfig` перекомпилирует скрипты, поэтому изменённое правило не может молчать за актуальным результатом. После включения анализаторов запустите `CompileManagedScripts` для каждой роли: каждая роль компилируется отдельно и под `#if SERVER`, `#if CLIENT` или `#if MAPPER` может дать разные результаты.

## Настройка уровней и параметров

Roslyn читает уровни из файлов `.editorconfig` над каждым исходником скрипта. Ключ действует на файлы, которым соответствует его секция:

```ini
[*.cs]
dotnet_diagnostic.FOENTRY001.severity = error
dotnet_diagnostic.FOENTRY002.severity = error
dotnet_analyzer_diagnostic.category-Verification.severity = error
dotnet_diagnostic.FOSTATIC001.severity = error

# Tests call entry points directly to exercise the dispatch path itself
[Scripts/Tests/**.cs]
dotnet_diagnostic.FOENTRY001.severity = none
```

Допустимые уровни: `error`, `warning`, `suggestion`, `silent` и `none`. Для каждого применяемого правила лучше явно задать `error`, чтобы проверка не зависела от `TreatWarningsAsErrors`. Понижайте правило только для пути с записанной причиной, как выше; у анализаторов нет исключения по имени вызывающего.

Два параметра расширяют словарь точек входа атрибутами проекта. Значения перечисляют через запятую имена метаданных (`Namespace.TypeName`, `Outer+Nested` для вложенного типа). Параметр действует на методы, объявленные в файлах, которые покрывает его секция, поэтому задавайте его в секции, покрывающей все исходники скриптов:

| Параметр | Действие |
|---|---|
| `fonline_entry.dispatched_attributes` | Методы с атрибутом считаются точками входа: `FOENTRY001` и `FOENTRY002` |
| `fonline_entry.registered_attributes` | Методы с атрибутом ищутся по имени: только `FOENTRY002` |

```ini
[*.cs]
fonline_entry.dispatched_attributes = MyGame.QuestHookAttribute
fonline_entry.registered_attributes = MyGame.DialogScriptAttribute, MyGame.ConsoleCommandAttribute
```

Маркер диспетчеризации проекта, чей диспетчер синхронизирует субъект, вместо этого должен нести `[EntryPointMarker]` на классе атрибута. Одно это объявление делает его точкой входа для `FOENTRY001`, `FOENTRY002` и `FOSYNC003` без каких-либо параметров. Имя, которое не разрешается, сообщается как `FOENTRY003` без места в исходнике, поэтому опечатка в параметре не может тихо исключить атрибут из правил.

## Правила проекта в анализаторе проекта

Стиль, именование, уровни доступа и прочая политика игры принадлежат проекту, а не `FOnline.Analyzers`. Держите их в анализаторе проекта и подключайте его рядом с анализатором движка:

```ini
ManagedScript.Analyzers = Engine/Source/Scripting/Managed/Analyzers/FOnline.Analyzers.csproj Tools/Analyzers/MyGame.Analyzers.csproj
ManagedScript.AnalyzerPackages = Microsoft.CodeAnalysis.BannedApiAnalyzers,4.14.0
ManagedScript.AdditionalFiles = Scripts/BannedSymbols.txt
```

`ManagedScript.AnalyzerPackages` принимает пакеты NuGet как точные пары `name,version`; плавающая версия отклоняется. `ManagedScript.AdditionalFiles` передаёт файлы конфигурации, например список запрещённых символов.

Проект анализатора игры повторяет форму `FOnline.Analyzers.csproj`:

- целевая платформа `netstandard2.0`, в которой компилятор загружает анализаторы;
- ссылка на `Microsoft.CodeAnalysis.CSharp` версии не новее Roslyn из SDK, который собирает скрипты, с `PrivateAssets="all"`;
- `EnableDefaultItems` и `EnableDefaultCompileItems` равны `false`, а исходники перечислены явно, потому что сборка скриптов вызывает проект со своей `Configuration` (`Server`, `Client`, `Mapper`), и шаблон захватил бы второе дерево `obj/<Config>`;
- включён `EnforceExtendedAnalyzerRules`, и каждый ID учтён в `AnalyzerReleases.Shipped.md` и `AnalyzerReleases.Unshipped.md` (правило `RS2008`);
- для ID и категории используется префикс проекта; префикс `FO` не используется повторно.

Правило проекта, которому нужно знать точки входа движка, должно читать то же объявление `[EntryPointMarker]` или параметры `fonline_entry.*`, а не держать свой список имён атрибутов движка.

## Доказательство срабатывания правил

Правило, которое включено, но не доходит до компиляции, выглядит в точности как правило, которое ничего не нашло. Движок держит по одному намеренному дефекту на правило в `Analyzers/Acceptance/EngineContractProbe.cs`. Каждый маркер `// ACCEPT: <ID>` ожидает этот ID внутри следующего за ним объявления; `// ACCEPT-NA: <ID> - <reason>` записывает правило, которое не может сработать на исходнике. Самотесты компилируют пробу с настоящими `CoreScripts/Attributes.cs` и `CoreScripts/Invariant.cs`, требуют срабатывания каждого маркера, отклоняют неожиданные диагностики и требуют маркер или причину для каждого правила этих анализаторов. Второй проход компилирует все CoreScripts с символами `SERVER`, `CLIENT` и `MAPPER` и требует, чтобы в них не было этих диагностик, потому что каждый проект компилирует CoreScripts с включёнными анализаторами.

Проект применяет тот же приём к уровням, которые он настроил: держит файл пробы вне `ManagedScript.Dirs`, компилирует его в отдельном приёмочном прогоне с настоящим сгенерированным проектом и `.editorconfig` проекта и падает, если размеченный ID не появился. Добавляйте случай в пробу каждый раз, когда правило поднимается до `error`.

## Запуск самотестов

Запускайте из корня движка на Windows или Linux с .NET 10 SDK в `PATH`:

```powershell
dotnet run --project Source/Scripting/Managed/Analyzers/Tests/FOnline.Analyzers.Tests.csproj --configuration Release
```

```bash
dotnet run --project Source/Scripting/Managed/Analyzers/Tests/FOnline.Analyzers.Tests.csproj --configuration Release
```

Из встраивающего игрового проекта добавьте к пути префикс `Engine/`. Первый запуск восстанавливает `Microsoft.CodeAnalysis.CSharp` из NuGet. При успехе выводится:

```text
OK: analyzer self-tests passed
```

| Код выхода | Значение |
|---|---|
| `0` | Все случаи прошли |
| `1` | Один или несколько случаев провалились; каждый выводится как `FAIL: <case>: <expected and actual>` |
| другой ненулевой | Набор остановился: фрагмент не компилируется или не найден `Source/Scripting/Managed` |

Команда проверена на Windows в PowerShell и Git Bash; форма для Linux та же и на хосте Linux не наблюдалась. В CI запускайте её в задании, которое ставит .NET 10 SDK, например через `actions/setup-dotnet`, и считайте ненулевой код выхода провалом.

## Миграция с копий под префиксом проекта

Проект, который держал собственные копии этих правил под ID с префиксом проекта, удаляет эти копии и переводит конфигурацию на ID движка:

| Прежнее правило проекта | Правило движка | Изменение поведения |
|---|---|---|
| Прямой вызов точки входа, привязанной к диспетчеру | `FOENTRY001` | Нет исключения по имени вызывающего; для тестов понижайте правило по пути. Маркеры проекта подключаются через `[EntryPointMarker]` или `fonline_entry.dispatched_attributes` |
| Вызываемый движком метод не `internal` | `FOENTRY002` | Проверяет код, сгенерированный проектом; пропускает только код сборщика в `FOnline`. Атрибуты проекта с регистрацией по имени подключаются через `fonline_entry.registered_attributes` |
| Сообщение `Invariant.Verify` пересказывает условие | `FOVERIFY001` | Нет |
| `Invariant.Verify(false, ...)` | `FOVERIFY002` | Нет |
| Действующий код в условии `Verify` | `FOVERIFY003` | Нет |
| Статическое поле вне досягаемости очистки при остановке | `FOSTATIC001` | Область задаётся пространством имён `FOnline`, а не путём к исходнику |

1. Удалите исходники анализатора проекта, тестовые случаи и строки учёта выпусков для заменённых правил. Правила проекта, выражающие политику игры, оставьте.
2. В каждом `.editorconfig` замените каждый ключ `dotnet_diagnostic.<old-id>.severity` на ID движка из таблицы, сохранив уровень.
3. Замените исключение тестов по имени вызывающего секцией по пути, которая задаёт `dotnet_diagnostic.FOENTRY001.severity = none`.
4. Пометьте атрибуты диспетчеризации проекта `[EntryPointMarker]` или перечислите их в `fonline_entry.dispatched_attributes`; атрибуты регистрации по имени перечислите в `fonline_entry.registered_attributes`.
5. Замените старые ID в приёмочных пробах, подавлениях (`#pragma warning disable`, `SuppressMessage`) и скриптах проверок. Найдите их командой `git grep -n "<old-id>"`.
6. Запустите `CompileManagedScripts` для каждой роли и приёмочный прогон. Пока загружены обе копии, каждый дефект сообщается под обоими ID; после шага 1 остаётся только ID движка.

Правила движка начинают действовать, как только обновление движка попадает в проект, поэтому выполняйте эти шаги в том же изменении, что и обновление.

## Контрольный список проверки

- `ManagedScript.Analyzers` перечисляет `FOnline.Analyzers.csproj`, и каждая роль компилируется без предупреждений.
- У каждого применяемого правила движка есть явный уровень в `.editorconfig`, а у каждого пониженного правила есть секция пути с причиной.
- Атрибуты диспетчеризации проекта несут `[EntryPointMarker]` или указаны в параметре `fonline_entry.*`, и `FOENTRY003` не сообщается.
- Правила стиля и именования проекта находятся в анализаторе проекта с ID под префиксом проекта.
- Проба доказывает, что каждое правило, поднятое проектом до `error`, по-прежнему срабатывает.
- Самотесты движка проходят с кодом выхода `0` после любого изменения в `Source/Scripting/Managed/Analyzers` или CoreScripts.
