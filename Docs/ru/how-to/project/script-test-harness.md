---
layout: default
title: Шаблон обвязки скриптовых тестов для AngelScript и Managed C#
locale: ru
document_id: project-script-test-harness
permalink: /Docs/ru/how-to/project/script-test-harness.html
---

# Шаблон обвязки скриптовых тестов для AngelScript и Managed C#
<!-- docs-translation: {"document_id":"project-script-test-harness","locale":"ru","source_path":"Docs/en/how-to/project/script-test-harness.md","source_sha256":"90b163e542a92991509f8c80cae0f23e384a4b28c426773e9bae4ac42e00aaab"} -->
Это руководство даёт игровому проекту небольшую копируемую обвязку, которая
выполняет игровые тесты уровня скриптов внутри одного сервера без графического
интерфейса и сообщает о них через фиксированный контракт маркеров. Движок
поставляет ядро обвязки для обоих скриптовых бэкендов как пример, фрагмент
настроек, манифест раннера и тесты, закрепляющие контракт; игровой проект
копирует ядро, пишет собственные наборы и фикстуры и запускает их через
`BuildTools/gameplay_test_runner.py`. Сначала прочитайте
[Добавление и запуск тестов в игровом проекте](project-tests.md): там описаны
выбор границы теста и исключение тестов из поставляемых пакетов.

## Проверенные исходные пути

- `Examples/GameplayTestHarness/README.md`
- `Examples/GameplayTestHarness/ScriptTests/AngelScript/Testing.fos`
- `Examples/GameplayTestHarness/ScriptTests/AngelScript/Test_HarnessExample.fos`
- `Examples/GameplayTestHarness/ScriptTests/Managed/Testing.cs`
- `Examples/GameplayTestHarness/ScriptTests/Managed/Test_HarnessExample.cs`
- `Examples/GameplayTestHarness/ScriptTests/Managed/Verification/ScriptTestHost.cs`
- `Examples/GameplayTestHarness/ScriptTests/Managed/Verification/ScriptTestHost.csproj`
- `Examples/GameplayTestHarness/ScriptTests/Managed/Verification/Test_HarnessSelfCheck.cs`
- `Examples/GameplayTestHarness/ScriptTests/script-tests.fomain`
- `Examples/GameplayTestHarness/ScriptTests/script-tests.json`
- `Examples/MinimalMultiplayer/CMakeLists.txt`
- `Examples/MinimalMultiplayer/run_tutorial_smoke.py`
- `BuildTools/gameplay_test_runner.py`
- `BuildTools/tests/test_script_test_harness_example.py`
- `Source/Common/Settings.inc`
- `Source/Common/Common.h`
- `Source/Common/Common.cpp`
- `Source/Essentials/BasicCore.cpp`
- `Source/Frontend/Application.cpp`
- `Source/Scripting/CommonGlobalScriptMethods.cpp`
- `Source/Scripting/AngelScript/AngelScriptBackend.cpp`
- `Source/Scripting/AngelScript/AngelScriptGlobals.cpp`
- `Source/Scripting/Managed/CoreScripts/Attributes.cs`
- `Source/Scripting/Managed/CoreScripts/TimePlace.cs`

## Что принадлежит движку, а что проекту

| Часть | Владелец | Где |
|---|---|---|
| Запуск процессов, обязательные и запрещённые маркеры, код выхода, deadline, отчёт | Движок | `BuildTools/gameplay_test_runner.py`, [Gameplay- и integration-тестирование](../testing/gameplay-and-integration.md) |
| Строки маркеров, имена настроек и поведение обвязки из этого руководства | Пример движка | `Examples/GameplayTestHarness/ScriptTests` |
| Скопированный файл обвязки, его пространство имён и проектные расширения | Игровой проект | каталог тестовых скриптов проекта |
| Наборы, фикстуры, идентификаторы контента, очистка и проверки утечек, пороги приёмки | Игровой проект | файлы наборов проекта |

Настройки `Testing.*` — метаданные проекта, а не группа настроек движка.
`Source/Common/Settings.inc` содержит только настройки, которые читает нативный
код, а эти настройки нативный код не читает. Объявление в движке к тому же
конфликтовало бы с проектом, который уже объявляет те же имена через
`///@ Setting`. Поэтому объявления находятся в файлах обвязки, а значения — в
`script-tests.fomain`.

Для этого протокола раннер расширять не понадобилось. Маркер — подстрока вывода
процесса, поэтому обвязка пишет фиксированный признак приёмки `status=passed`, а
не счётчик, который проверка подстроки оценить не может.

## Контракт маркеров

Обвязка пишет эти строки через `Game.Log` в таком порядке:

```text
[Testing] START registered=<n> selected=<n> filter='<Testing.Filter>'
[Testing] PASS <suite>.<name>
[Testing] FAIL <suite>.<name>: <reason>
[Testing] ERROR <configuration problem>
[Testing] COMPLETE status=<passed|failed> registered=<n> selected=<n> passed=<n> failed=<n> skipped=<n> not_run=<n>
```

- `START` пишется один раз, после регистрации всех наборов. `PASS` или `FAIL`
  пишется один раз на каждый выполненный тест. `ERROR` пишется вместо запуска
  тестов, если регистрация или конфигурация некорректны.
- `COMPLETE` — завершающая строка. `status=passed` требует отсутствия ошибки
  конфигурации, хотя бы одного выбранного теста и `failed=0`. Пустой выбор
  считается провалом, поэтому фильтр с опечаткой не может пройти.
- `skipped` считает тесты, которые фильтр оставил за бортом. `not_run` считает
  выбранные тесты, которые не выполнились, потому что
  `Testing.StopOnFirstFailure` остановил прогон или ему помешала ошибка
  конфигурации.
- Причина `FAIL` — текст проваленного ожидания, `timeout` или
  `exception: <text>`. Обвязка AngelScript берёт текст из `GetExceptionInfo()`,
  управляемая обвязка — имя типа исключения и его сообщение.
- При `Testing.ExitOnFinish = True` обвязка вызывает `Game.RequestQuit(passed)`.
  Сервер без графического интерфейса затем завершается с `EXIT_SUCCESS` или
  `EXIT_FAILURE` (`exit_app` в `Source/Essentials/BasicCore.cpp`), то есть с
  кодом 0 или 1 в Windows и Linux.

Манифест раннера принимает прогон только по завершающей строке и коду выхода:

```json
{
  "schema_version": 1,
  "name": "script-tests",
  "default_timeout_seconds": 300,
  "forbidden_markers": [
    "FATAL ERROR!",
    "ScriptException",
    "[Testing] FAIL",
    "[Testing] ERROR",
    "[Testing] COMPLETE status=failed"
  ],
  "scenarios": [
    {
      "id": "script-tests",
      "processes": [
        {
          "id": "server",
          "command": [
            "{server}",
            "-ApplyConfig",
            "{config}",
            "-ApplySubConfig",
            "ScriptTests"
          ],
          "required_markers": [
            "[Testing] START",
            "[Testing] COMPLETE status=passed"
          ]
        }
      ]
    }
  ]
}
```

Число строк `PASS`, работающий процесс или отсутствие строки `FAIL` приёмкой не
являются. Дополнительную диагностику держите под другим префиксом, чтобы она
никогда не совпала с маркером приёмки или провала.

## Настройки

| Настройка | Тип | Значение в главном файле | Смысл |
|---|---|---|---|
| `Testing.Enabled` | `bool` | `False` | Подписывает обвязку на `Game.OnStart`; в обычной игре выключена |
| `Testing.Filter` | `string` | пусто | Токены через запятую; тест запускается, если `suite.name` содержит любой токен без учёта регистра; пустое значение запускает всё |
| `Testing.StopOnFirstFailure` | `bool` | `False` | Останавливает прогон после первого `FAIL` и сообщает остальные как `not_run` |
| `Testing.ExitOnFinish` | `bool` | `True` | После `COMPLETE` запрашивает выход со статусом прогона |
| `Testing.TestTimeout` | `int32` | `10000` | Сторожевой таймер теста в миллисекундах; регистрация может его переопределить; ноль или меньше — ошибка конфигурации |

Слейте `Examples/GameplayTestHarness/ScriptTests/script-tests.fomain` с главным
`.fomain`. Главному файлу нужно значение каждой объявленной настройки, а
подконфигурация включает обвязку:

```ini
Testing.Enabled = False
Testing.Filter =
Testing.StopOnFirstFailure = False
Testing.ExitOnFinish = True
Testing.TestTimeout = 10000
AngelScript.MutableGlobalsAllowedNamespaces = Testing

[SubConfig]
Name = ScriptTests
Testing.Enabled = True
ServerNetwork.DisableNetworking = True
Server.DbStorage = Memory
```

`AngelScript.MutableGlobalsAllowedNamespaces` относится только к бэкенду
AngelScript. Обвязка хранит состояние прогона в одной глобальной переменной
модуля, `Testing::Run`, а `AngelScriptBackend.cpp` при загрузке серверного
модуля отклоняет изменяемые глобальные переменные вне перечисленных префиксов
пространств имён. Оставьте значение ровно `Testing` и не включайте тестовые
исходники в поставляемую сборку ресурсов, чтобы исключение никогда не попало в
поставляемый модуль. Управляемая обвязка хранит то же состояние в одном
статическом поле: это состояние тестового процесса, которое никогда не
сохраняется, а изолированный контекст загрузки каждого бэкенда держит его
отдельно для каждого движка. Игровое состояние по-прежнему принадлежит `Game`,
сущности или ref type; см. [Managed C#](../scripting/managed-csharp.md).

Переопределяйте настройку в командной строке сервера через
`-Testing.Filter <value>`. Всегда передавайте значение: `Source/Common/Common.cpp`
даёт опции, за которой сразу следует другая опция, неявное значение `1`. Раннер
отклоняет пустой `--value`, поэтому держите манифест CI без фильтра, а для
отфильтрованных прогонов используйте отдельный манифест или прямой запуск.

## Копирование обвязки в проект

1. Скопируйте один файл ядра в каталог тестовых скриптов проекта, например
   `Tests/Scripts`: `ScriptTests/AngelScript/Testing.fos` для AngelScript или
   `ScriptTests/Managed/Testing.cs` для Managed C#. В файле C# замените
   заглушку `namespace MyGame;` пространством имён скриптов проекта.
2. Слейте фрагмент настроек с главным `.fomain`, как показано выше.
3. Укажите тестовый каталог входным каталогом пакетов метаданных и скриптов и
   исключите его из поставляемой сборки ресурсов через `Baking.IgnoreInputDirs`,
   как описано в разделе
   [об исключении тестовых исходников из поставляемых пакетов](project-tests.md#исключение-тестовых-исходников-из-поставляемых-пакетов).
   Проекты на Managed C# также добавляют каталог в `ManagedScript.Dirs`, который
   просматривает файлы `.cs` верхнего уровня каждого указанного корня.
4. Напишите наборы рядом с ядром (следующий раздел).
5. Скомпилируйте каждую сторону через `CompileAngelScript` или
   `CompileManagedScripts`, соберите ресурсы и запустите подконфигурацию
   `ScriptTests`.

Не добавляйте в скопированное ядро игровые типы. Тогда фикстуры,
идентификаторы контента и политика проекта живут в других файлах, а более
поздняя версия примера движка ложится на ядро чистым diff.

## Регистрация наборов и фикстуры в файлах проекта

Набор подписывается на `Game.OnTestingInit` в инициализаторе модуля и
регистрирует тесты из обработчика события. Обвязка вызывает это событие один раз
из своего обработчика `Game.OnStart`, поэтому порядок инициализации модулей не
важен. Правила регистрации:

- `RegisterTest(suite, name, func, timeoutMs = 0)` допустим только во время
  вызова события; более поздний вызов бросает исключение.
- Полное имя — `suite.name`. Имя набора с `.`, пустое имя, отрицательный
  тайм-аут или повтор полного имени сообщаются как `ERROR` и проваливают весь
  прогон, вместо того чтобы молча потерять остаток набора.
- Тест проходит, если его обратный вызов завершился без проваленного ожидания.
  `Expect`, `ExpectEq` и `ExpectStrEq` записывают первый провал и возвращают
  `false`, чтобы тест мог выйти раньше. `Pass` и `Fail` задают вердикт явно;
  побеждает первый вердикт.
- Исключение, вышедшее из обратного вызова, становится `FAIL` с текстом
  исключения.

Набор AngelScript из `Test_HarnessExample.fos`:

```angelscript
namespace Test_HarnessExample
{

#if SERVER

[[ModuleInit]]
void InitTestHarnessExample()
{
    Game.OnTestingInit.Subscribe(OnTestingInit);
}

[[Event]]
void OnTestingInit()
{
    Testing::RegisterTest("harness_example", "split_drops_empty_entries", SplitDropsEmptyEntries);
    Testing::RegisterTest("harness_example", "time_event_completes_a_deferred_test", TimeEventCompletesADeferredTest, 2000);
}

void SplitDropsEmptyEntries(Testing::TestContext ctx)
{
    string[] parts = "alpha,,beta".split(",", true);
    if (!Testing::ExpectEq(ctx, parts.length(), 2, "Split entry count")) {
        return;
    }

    Testing::ExpectStrEq(ctx, parts[1], "beta", "Second entry");
}

void TimeEventCompletesADeferredTest(Testing::TestContext ctx)
{
    Testing::Defer(ctx);
    Game.StartTimeEvent(Testing::Milliseconds(50), FinishDeferredTest, ctx.Id);
}

[[TimeEvent]]
void FinishDeferredTest(any value)
{
    // A null context means the watchdog already failed this test
    Testing::TestContext? ctx = Testing::Resume(int(value));
    if (ctx == null) {
        return;
    }

    Testing::Pass(ctx);
}

#endif

}
```

Тест AngelScript, растянутый на несколько тиков движка, вызывает
`Testing::Defer(ctx)`, передаёт `ctx.Id` через полезную нагрузку временного
события и позже получает контекст через `Testing::Resume(id)`, который
возвращает `null` после завершения теста. До сторожевого таймера могут дойти
только отложенные тесты. Исключение в более позднем обратном вызове находится
вне `try` обвязки, поэтому оно проявляется как `timeout` вместе с записью
движка об исключении.

Тот же набор на Managed C# из `Test_HarnessExample.cs`:

```csharp
namespace MyGame;

using System;
using System.Threading.Tasks;
using FOnline;

public static class Test_HarnessExample
{
#if SERVER
    [ModuleInit]
    internal static void InitTestHarnessExample()
    {
        Game.OnTestingInit.Subscribe(OnTestingInit);
    }

    [Event]
    internal static void OnTestingInit()
    {
        Testing.RegisterTest("harness_example", "split_drops_empty_entries", SplitDropsEmptyEntries);
        Testing.RegisterTest("harness_example",
                             "awaited_time_step_completes_the_test",
                             AwaitedTimeStepCompletesTheTest,
                             2000);
    }

    private static void SplitDropsEmptyEntries(Testing.TestContext ctx)
    {
        string[] parts = "alpha,,beta".Split(',', StringSplitOptions.RemoveEmptyEntries);
        if (!Testing.ExpectEq(ctx, parts.Length, 2, "Split entry count")) {
            return;
        }

        Testing.ExpectStrEq(ctx, parts[1], "beta", "Second entry");
    }

    private static async Task AwaitedTimeStepCompletesTheTest(Testing.TestContext ctx)
    {
        await Testing.WaitAsync(ctx, 50);
        Testing.Pass(ctx);
    }
#endif
}
```

Управляемый тест может возвращать `Task`. Обвязка ожидает его из обработчика
`[TimeEvent]`, а сторожевой таймер проваливает задачу, не завершившуюся за
тайм-аут. `Testing.WaitAsync(ctx, milliseconds)` приостанавливает тест через
временное событие движка, а сторожевой таймер отменяет приостановленное
ожидание.

Оба ядра выполняют тест одинаково. Вот шаг AngelScript, который запускает
сторожевой таймер, выполняет обратный вызов и завершает тест без отсрочки:

```angelscript
int timeoutMs = test.TimeoutMs > 0 ? test.TimeoutMs : Settings.Testing.TestTimeout;
run.WatchdogId = Game.StartTimeEvent(Milliseconds(timeoutMs), WatchTest, ctx.Id);

TestFunc? func = test.Func;

try {
    if (func != null) {
        func(ctx);
    }
}
catch {
    Fail(ctx, "exception: " + GetExceptionInfo());
}

// A callback that returns without a verdict and without Defer has passed
if (!ctx.Finished && !ctx.Deferred) {
    ctx.Finished = true;
    ctx.Ok = true;
}

if (ctx.Finished && !ctx.Completed) {
    CompleteTest(ctx);
}
```

И управляемый эквивалент:

```csharp
int timeoutMs = test.TimeoutMs > 0 ? test.TimeoutMs : Settings.Testing.TestTimeout;
run.WatchdogId = Game.StartTimeEvent(Milliseconds(timeoutMs), WatchTest, ctx.Id);

try {
    await test.Run(ctx);
}
catch (Exception exception) {
    Fail(ctx, "exception: " + exception.GetType().Name + ": " + exception.Message);
}

// A task that completes without a verdict has passed; a late completion after the watchdog changes nothing
if (!ctx.Finished) {
    ctx.Finished = true;
    ctx.Ok = true;
}

if (!ctx.Completed) {
    CompleteTest(ctx);
}
```

Держите игровые фикстуры в файлах проекта рядом с наборами, например в
`Fixtures.fos` или `TestFixtures.cs`: они создают нужные тесту сущности,
записывают их идентификаторы в контекст, принадлежащий проекту, и уничтожают их
с проверкой утечек до того, как тест сообщит результат. Вызывайте
`Testing::Fail` или `Testing.Fail` из фикстуры, когда очистка находит утечку.
Ядро обвязки ничего не знает о картах, криттерах, предметах или контенте.

## Запуск тестов

Во время разработки запускайте сервер без графического интерфейса напрямую.
Windows (PowerShell):

```powershell
& "<server-headless-binary>" -ApplyConfig "<project>.fomain" -ApplySubConfig ScriptTests -Testing.Filter "<filter>"
```

Linux (bash):

```bash
<server-headless-binary> -ApplyConfig <project>.fomain -ApplySubConfig ScriptTests -Testing.Filter "<filter>"
```

Для приёмки запускайте полный набор через раннер. Windows (PowerShell):

```powershell
py -3 Engine/BuildTools/gameplay_test_runner.py `
  --manifest Tests/script-tests.json `
  --value "server=<server-headless-binary>" `
  --value "config=<project>.fomain" `
  --report Workspace/script-tests-report.json
```

Linux (bash):

```bash
python3 Engine/BuildTools/gameplay_test_runner.py \
  --manifest Tests/script-tests.json \
  --value server=<server-headless-binary> \
  --value config=<project>.fomain \
  --report Workspace/script-tests-report.json
```

| Код выхода | Сервер с `Testing.ExitOnFinish = True` | `gameplay_test_runner.py` |
|---|---|---|
| `0` | `COMPLETE status=passed` | все сценарии прошли |
| `1` | `COMPLETE status=failed` | нарушен контракт маркеров, кода выхода, готовности или тайм-аута |
| `2` | обвязка не выдаёт | некорректный CLI или манифест |

При `Testing.ExitOnFinish = False` сервер продолжает работать после `COMPLETE`;
используйте это только для интерактивного осмотра, потому что раннер тогда
остановит процесс по своему deadline.

## Проверка примера в движке

`BuildTools/tests/test_script_test_harness_example.py` проверяет, что оба ядра
объявляют одни и те же настройки и событие, пишут одни и те же поля маркеров в
одном порядке, и что фрагмент настроек и манифест им соответствуют. Если SDK
`dotnet` есть в `PATH`, тест также компилирует управляемое ядро, пример набора и
наборы отрицательного контроля вместе с CoreScripts движка и заменителем
сгенерированного серверного API (`ScriptTestHost.cs`), а затем выполняет
сценарии успеха, провала, тайм-аута, исключения, фильтра, пустого выбора,
ошибки конфигурации и раннера. Без `dotnet` эти случаи пропускаются с этой
причиной.

```powershell
py -3 -m pytest BuildTools/tests/test_script_test_harness_example.py -q -p no:cacheprovider
```

```bash
python3 -m pytest BuildTools/tests/test_script_test_harness_example.py -q -p no:cacheprovider
```

Заменитель доказывает управляющую логику и маркеры обвязки, а не диспетчеризацию
движка. Тесты движка не компилируют ядро AngelScript, потому что для компиляции
скриптов нужна сборка `<ProjectDevName>_ASCompiler` проекта. После копирования
любого ядра скомпилируйте его целями проекта и один раз запустите
подконфигурацию `ScriptTests` с намеренно проваленным тестом, чтобы увидеть
`FAIL` и `COMPLETE status=failed`.

## Подключение к локальной задаче и CI

Добавьте пользовательскую цель рядом со сборкой ресурсов по образцу
`RunTutorialChecks` в `Examples/MinimalMultiplayer/CMakeLists.txt`:

```cmake
find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_custom_target(RunScriptTests
    COMMAND ${Python3_EXECUTABLE}
        "${CMAKE_CURRENT_SOURCE_DIR}/Engine/BuildTools/gameplay_test_runner.py"
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/Tests/script-tests.json"
        --value "server=$<TARGET_FILE:${FO_DEV_NAME}_ServerHeadless>"
        --value "config=${CMAKE_CURRENT_SOURCE_DIR}/${FO_MAIN_CONFIG}"
        --report "${FO_OUTPUT_PATH}/script-tests-report.json"
    DEPENDS ${FO_DEV_NAME}_ServerHeadless ForceBakeResources
    SOURCES Tests/script-tests.json
    WORKING_DIRECTORY ${FO_OUTPUT_PATH}
    VERBATIM
    USES_TERMINAL
    COMMENT "Run script tests")
```

Зависимость от цели сборки ресурсов оставляет одного писателя на вывод сборки;
не запускайте тесты, пока идёт другая сборка того же вывода. В CI запускайте
цель после нативных unit-тестов и до smoke-тестов процессов, сохраняйте
JSON-отчёт и лог с префиксами как артефакты и принимайте решение по задаче
только по коду выхода раннера. Размещением задач и хранением артефактов владеет
страница
[Настройка непрерывной интеграции для игрового проекта](continuous-integration.md).

## Расширение в проекте

- **Цепочка подконфигураций.** Поместите переключатели детерминированного мира
  в одну тестовую подконфигурацию, например `GameplayTests`, и выводите из неё
  узкие подконфигурации через `Parent = GameplayTests` для флагов функций,
  нужных лишь некоторым наборам. Тогда манифест выбирает подконфигурацию по
  имени. Приоритетом подконфигураций и цепочками `Parent` владеет страница
  [Конфигурация проекта](../build/project-configuration.md).
- **Тонкая обёртка.** Скрипт проекта может определить пути к бинарникам, выбрать
  манифест и вызвать `gameplay_test_runner.main(argv)`, как это делает
  `Examples/MinimalMultiplayer/run_tutorial_smoke.py`. Для прогона с фильтром он
  пишет временную копию манифеста, команда которой добавляет `-Testing.Filter` и
  его значение, а манифест в репозитории остаётся без фильтра для CI.
- **Больше настроек.** Параллельные экземпляры, шардирование, повторы или
  множители тайм-аутов — политика проекта. Объявите их дополнительными строками
  `///@ Setting` в файле проекта рядом с ядром, дайте каждой значение в главном
  файле и не меняйте пять настроек контракта.
- **Больше фикстур.** Добавляйте помощники фикстур и проверки утечек в файлы
  проекта, которые вызывают `Fail`. Держите рядом с каждой фикстурой короткий
  комментарий о том, что она создаёт и почему её очистка полна.
- **Больше маркеров.** Сохраняйте пять видов строк. Дополнительные сведения,
  например время или число ожиданий, пишите в строки с другим префиксом и
  никогда не добавляйте в `COMPLETE`, чтобы манифесты продолжали совпадать.

### Переход с прежней обвязки проекта

1. Перечислите настройки, вызовы регистрации, строки маркеров и поведение
   выхода прежней обвязки.
2. Скопируйте ядро движка и перенесите проектные помощники из прежнего ядра в
   файлы фикстур.
3. Переименуйте настройки, отличающиеся от контракта, например проектную
   настройку тайм-аута в `Testing.TestTimeout`, в объявлениях, главном
   `.fomain`, каждой подконфигурации и каждом средстве запуска. Настройки — это
   конфигурация, а не сохраняемые данные, поэтому миграция сохранений не нужна.
4. Переведите наборы на `RegisterTest(suite, name, func, timeoutMs)`; сохраните
   имена наборов и тестов, чтобы существующие фильтры продолжали работать.
5. Замените прежние маркеры приёмки в каждом манифесте, задаче и шаге CI на
   `[Testing] COMPLETE status=passed` и запрещённые маркеры выше.
6. Удалите прежнее ядро, когда каждая сторона компилируется, а намеренно
   проваленный тест даёт `FAIL` и `COMPLETE status=failed` с кодом выхода 1.

## Разбор сбоев

| Симптом | Что проверить сначала |
|---|---|
| Нет ни одной строки `[Testing]` | `Testing.Enabled` в применённой подконфигурации и наличие файла обвязки во входных скриптах сборки |
| Запуск сервера падает на изменяемой глобальной переменной | `AngelScript.MutableGlobalsAllowedNamespaces = Testing` в главном `.fomain` |
| Сторона падает с «Nothing was built in the module» | Все скрипты этой стороны находятся внутри `#if SERVER`; обычно у проекта есть другие модули для этой стороны |
| `ERROR duplicate test` или `ERROR invalid registration` | Названная регистрация в файлах наборов |
| `ERROR no registered test matches the filter` | Значение `Testing.Filter` или отсутствующая подписка набора на `Game.OnTestingInit` |
| `FAIL <test>: timeout` | Не потерял ли отложенный или асинхронный тест своё продолжение, затем лог движка на исключение в более позднем обратном вызове |
| Тайм-аут раннера без `COMPLETE` | `Testing.ExitOnFinish` и последняя строка `PASS` или `FAIL` перед остановкой |

## Контрольный список проверки

- Скопированное ядро отличается от примера движка только пространством имён и
  комментариями.
- У каждой настройки `Testing.*` есть значение в главном файле, и обвязку
  включает только подконфигурация.
- Манифест требует `[Testing] COMPLETE status=passed` и запрещает
  `[Testing] FAIL`, `[Testing] ERROR` и `[Testing] COMPLETE status=failed`.
- Прежде чем доверять набору, намеренно проваленный тест даёт `FAIL`,
  `COMPLETE status=failed` и код выхода 1.
- Тестовые исходники, включая ядро, отсутствуют в поставляемых пакетах.
- Игровые фикстуры, идентификаторы контента и очистка находятся в файлах
  проекта, а не в ядре.
