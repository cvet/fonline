---
layout: default
title: Выбор backend для скриптов
locale: ru
document_id: choosing-a-script-backend
permalink: /Docs/ru/explanation/scripting-runtime/choosing-a-backend.html
---

# Выбор backend для скриптов
<!-- docs-translation: {"document_id":"choosing-a-script-backend","locale":"ru","source_path":"Docs/en/explanation/scripting-runtime/choosing-a-backend.md","source_sha256":"c3b1efa3b284ce9b2ca149b2e724b5daf5c8754071b04a04eb088db7fa0d5725"} -->
Встраивающий игровой проект выбирает backend игровых скриптов при настройке
сборки: AngelScript (модули `.fos`) или Managed C# (модули `.cs` на встроенном
Mono). Выбор меняет требования к toolchain, sanitizer-линии, отладку,
статический анализ, стиль тестов и контракт конкурентности сервера. Сделать его
дёшево до первого скрипта и дорого отменить потом. Эта страница перечисляет, что
нужно каждому backend, чем они различаются и как настроить каждый из них;
полные контракты описаны в профильных руководствах.

## Проверенные исходные пути

- `BuildTools/cmake/ProjectInterface.json`
- `BuildTools/cmake/stages/Init.cmake`
- `BuildTools/cmake/stages/ProjectOptions.cmake`
- `BuildTools/cmake/stages/Applications.cmake`
- `BuildTools/cmake/stages/ScriptsAndBaking.cmake`
- `BuildTools/cmake/stages/ThirdParty.cmake`
- `BuildTools/buildtools.py`
- `BuildTools/README.md`
- `BuildTools/SupportMatrix.json`
- `Source/Common/Settings.inc`
- `Source/Server/Server.cpp`
- `Source/Server/EntitySync.cpp`
- `Source/Tests/Test_ServerEngine.cpp`
- `Docs/en/explanation/scripting-runtime/index.md`
- `Docs/en/explanation/runtime/server.md`
- `Docs/en/how-to/scripting/managed-csharp.md`
- `Docs/en/how-to/scripting/style-and-refactoring.md`
- `Docs/en/how-to/scripting/lifecycle-and-concurrency.md`
- `Docs/en/how-to/build/generated-content.md`
- `Docs/en/how-to/build/project-configuration.md`
- `Docs/en/troubleshooting/debugging.md`
- `Docs/en/reference/platforms/support-matrix.md`
- `Docs/en/reference/script-api/settings.md`
- `Docs/en/contributing/coding-contracts/nullability.md`
- `Examples/MinimalProject/`

## Краткий ответ

Оба реализованных backend используют одни и те же метаданные, модель
сущностей, события, удалённые вызовы и native export surface, поэтому игровой
дизайн не зависит от выбора. Native scripting (`FO_NATIVE_SCRIPTING`) пока
только зарезервированная опция: в её корневом каталоге исходников лежит один
файл-заглушка, а описание в CMake сообщает, что Native backend не реализован.
Не выбирайте его.

| Если проекту нужно | Предпочтительно | Почему (источник утверждения) |
| --- | --- | --- |
| Поддерживаемая, запускаемая отправная точка | AngelScript | Все пять проектов в `Examples/` содержат модули `.fos` и задают `FO_ANGELSCRIPT_SCRIPTING ON`. Управляемого стартового примера в Engine пока нет: в `Examples/` не отслеживается ни одного файла `.cs`. |
| Самый маленький toolchain | AngelScript | Managed C# дополнительно требует .NET SDK на сборочном хосте и при первой сборке собирает встроенную среду выполнения Mono из исходников. |
| Линии MemorySanitizer или ThreadSanitizer при включённых скриптах | AngelScript | `Init.cmake` отклоняет `San_Memory*` и `San_Thread`, когда включён `FO_MANAGED_SCRIPTING`. |
| Пошаговое выполнение скриптов в редакторе | AngelScript | Engine поставляет опциональную точку подключения отладчика AngelScript. У C# аналога нет. |
| Доказательство на этапе компиляции корректности cover сущностей на сервере | Managed C# | Анализатор Roslyn (диагностики `FOSYNC`) входит в управляемую сборку; AngelScript полагается на проверки cover во время выполнения. |
| `Task`/`await`, инструменты Roslyn, NuGet-анализаторы, навигация в IDE | Managed C# | Baker генерирует `.gen.csproj`/`.gen.sln`, которые открываются в IDE, а сборка считает предупреждения ошибками. |
| Замена метода скрипта в работающем процессе | Managed C# | Weaving точек патча и live-компилятор существуют только в управляемом backend, и оба опциональны. |

Если ни одна строка не решает вопрос, начните с AngelScript: для него есть
запускаемые примеры, компиляторные линии матрицы поддержки и отладчик.

## Что требуется каждому backend

| Пункт | AngelScript | Managed C# |
| --- | --- | --- |
| Опция CMake | `FO_ANGELSCRIPT_SCRIPTING` | `FO_MANAGED_SCRIPTING` |
| Исходные файлы | модули проекта `.fos` | файлы `.cs` верхнего уровня проекта плюс `CoreScripts` движка |
| Дополнительное приложение сборки | `<DevName>_ASCompiler`, собирается при включённой `FO_BUILD_ASCOMPILER`; конфигурация завершается ошибкой, если эта опция включена без AngelScript | `<DevName>_ManagedScriptBaker`, собирается вместе с библиотекой baker (`FO_BUILD_BAKER`, `FO_BUILD_MAPPER`, `FO_BUILD_ASCOMPILER` или `FO_UNIT_TESTS`) |
| Дополнения к сборочному хосту | нет, кроме обычного C++ toolchain | .NET SDK, способный запустить `ManagedScript.MsBuild` (по умолчанию `dotnet msbuild`) для `ManagedScript.TargetFramework` (по умолчанию `net10.0`), и сборка среды выполнения Mono, описанная ниже |
| Baker пакета ресурсов | `Bakers = AngelScript` | `Bakers = Managed` |
| Настройки проекта | группа `AngelScript.*` | группа `ManagedScript.*` |
| Артефакт компиляции | запечённый байткод AngelScript | сборки по ролям (`<Pack>.Server.dll`, `<Pack>.Client.dll`, `<Pack>.Mapper.dll`) плюс runtime payload |
| Быстрая проверка компиляции | `CompileAngelScript` | `CompileManagedScripts` |

Опции представляют собой независимые булевы значения. Производственный проект
включает ровно одну; единственная документированная причина включить больше
одной — тесты межбackend-вызовов. Выбирайте их в `CMakeLists.txt` проекта так,
как это делает `Examples/MinimalProject/CMakeLists.txt`:

```cmake
SetOption(FO_ANGELSCRIPT_SCRIPTING ON)
SetOption(FO_NATIVE_SCRIPTING OFF)
SetOption(FO_MANAGED_SCRIPTING OFF)
```

Полностью управляемый проект меняет местами первую и третью строки. `SetOption`
лишь присваивает ещё не определённую переменную и никогда не сверяет имя с
`ProjectInterface.json`, поэтому опечатка в имени опции проходит молча;
проверяйте написание по [опциям CMake](../../reference/cmake/options.md).

## Платформа, sanitizers, отладка, анализ и тесты

| Аспект | AngelScript | Managed C# |
| --- | --- | --- |
| Линии, проверяемые Engine | Обязательные настольные линии [матрицы поддержки](../../reference/platforms/support-matrix.md) собирают компилятор AngelScript (`win64-ascompiler`, `linux-ascompiler`). | Единственные выделенные проверки BuildTools называются `managed-mac-client`, `managed-ios-simulator-client` и `managed-ios-device-client`; они запускаются через ручное задание workflow `managed-apple`, линкуют клиент и ничего не исполняют. |
| Охват платформ | У встроенной среды выполнения есть пропатченные пути native-вызовов для x64 GCC, x64 MSVC и ARM64; любую другую цель проверяйте самостоятельно. | Подключены сборочные пути для Windows, Linux, Android, WebAssembly, macOS и iOS. Это возможность исходников, а не обещание релиза: квалифицируйте каждую поставляемую цель собственным пакетом, payload, запуском и маршрутом обновления. |
| Ограничения хоста | нет, кроме обычного toolchain | Среду выполнения для цели Windows нельзя собрать на хосте не под Windows: конфигурация прерывается, если `FO_MANAGED_RUNTIME_PREBUILT` (переменная окружения) не указывает на дерево, собранное в Windows. |
| Sanitizers | Clang добавляет `San_Address`, `San_Memory`, `San_MemoryWithOrigins`, `San_Undefined`, `San_Thread`, `San_DataFlow` и `San_Address_Undefined`. | `San_Memory*` и `San_Thread` не предлагаются и прерывают configure при принудительном выборе. Остаются линии AddressSanitizer и undefined-behavior, для них нужны собственные проверки управляемой среды выполнения. |
| Живая отладка | Опциональная точка подключения: `AngelScript.DebuggerEnabled = True`, по умолчанию привязка к loopback, построчные точки останова, пошаговое выполнение, локальные переменные только для чтения. Только в конфигурациях разработки. | Адаптера редактора и launch-профиля Engine нет. Используйте сгенерированное решение для навигации, диагностик Roslyn, runtime-логов и точечных тестов; подключайте нативный или управляемый отладчик только после сверки сборок, payload и символов. |
| Статический анализ | Правила компилятора для атрибутов, прямых вызовов функций, принадлежащих диспетчеру, глобальных const-only переменных и nullability `T?`. Проверки соответствия объявлений сверх этого остаются на стороне проекта. | Nullable-ссылки, `TreatWarningsAsErrors`, checked-арифметика, `ManagedScript.Analyzers`/`AnalyzerPackages` и анализатор cover `FOSYNC`. |
| Форматирование | Обёртка Engine `buildtools.py format-source` требует clang-format 20 и исправляет синтаксис `.fos`, но форматирует только `Source` движка; форматирование `.fos` проект подключает сам. | Форматтер и проверка стиля принадлежат проекту; сгенерированный проект применяет стиль кода во время сборки. |
| Тесты Engine для изучения | `Test_AngelScript*.cpp` | `Test_ManagedScriptBaker.cpp`, `Test_ManagedScriptRuntime.cpp`, `BuildTools/tests/test_managed_*.py`, проекты тестов анализатора и CoreScripts |
| Тесты скриптов проекта | harness принадлежит проекту, запускается через [gameplay test runner](../../how-to/testing/gameplay-and-integration.md) | тот же контракт runner; harness написан на C# и принадлежит проекту |

Прочитайте [руководство по отладке](../../troubleshooting/debugging.md), прежде
чем полагаться на сценарий подключения отладчика, и [контракт
nullability](../../contributing/coding-contracts/nullability.md) для правил
`T?`, общих для обоих backend.

## Сборка и bake бок о бок

Оба маршрута зависят от `ForceCodeGeneration`, поэтому метаданные, видимые
скриптам, актуальны до компиляции любого скрипта. Команды одинаковы в PowerShell
и bash.

| Шаг | AngelScript | Managed C# |
| --- | --- | --- |
| Подготовка хоста | только обычный C++ toolchain | установите .NET SDK; при желании заранее подготовьте закреплённые исходники среды выполнения командой `py -3 Engine/BuildTools/buildtools.py prepare-workspace dotnet` (Windows) или `python3 Engine/BuildTools/buildtools.py prepare-workspace dotnet` (Linux) |
| Configure | `cmake --preset <your-preset>` с включённым AngelScript | то же, с включённым `FO_MANAGED_SCRIPTING`; цель `SetupManagedRuntime`, от которой зависят сгенерированные результаты, при первой сборке собирает Mono из ревизии, закреплённой в `ThirdParty/dotnet-runtime` |
| Компиляция скриптов | `cmake --build <build-dir> --config RelWithDebInfo --target CompileAngelScript` | `cmake --build <build-dir> --config RelWithDebInfo --target CompileManagedScripts` |
| Что запускается | `<DevName>_ASCompiler -ApplyConfig <project .fomain> -ApplySubConfig NONE` | `<DevName>_ManagedScriptBaker` с теми же аргументами: генерирует C# API, `.gen.csproj` и `.gen.sln`, затем компилирует сборки |
| Bake | `BakeResources` / `ForceBakeResources`; baker `AngelScript` записывает байткод в пакет | те же цели; baker `Managed` упаковывает сборки и `ManagedRuntime/` |
| Признак сбоя | диагностики компилятора с файлом и строкой скрипта | `ManagedScriptBakerException` для неподдерживаемых конструкций, иначе диагностики MSBuild/Roslyn |

Только bake доказывает доставку. Компиляция скриптов доказывает синтаксис и
согласованность метаданных; она не доказывает, что работают пакет ресурсов,
runtime payload или путь запуска.

`.fomain` фиксирует выбор backend как пакет ресурсов, поэтому держите его
согласованным с опцией CMake:

```ini
[ResourcePack]
Name = Scripts
InputDirs = Scripts
Bakers = AngelScript
ServerOnly = True
```

Для Managed C# выбирайте пакет и настройки сгенерированного проекта вместе;
приведённые ниже настройки — документированные значения по умолчанию плюс путь
анализатора из руководства по конфигурации проекта:

```ini
ManagedScript.Assemblies = MyGame
ManagedScript.ProjectName = MyGame
ManagedScript.TargetFramework = net10.0
ManagedScript.MsBuild = dotnet msbuild
ManagedScript.Dirs = Engine/Source/Scripting/Managed/CoreScripts Scripts
ManagedScript.Analyzers = Engine/Source/Scripting/Managed/Analyzers/FOnline.Analyzers.csproj

[ResourcePack]
Name = ManagedScripts
InputDirs = Engine/Source/Scripting/Managed/CoreScripts Scripts
IncludePatterns = *
Bakers = Managed
```

Все поля пакета описаны в разделе [Определение resource
packs](../../how-to/build/project-configuration.md#определение-resource-packs),
маршруты компиляции — в [процессе работы со сгенерированными
данными](../../how-to/build/generated-content.md#компиляция-scripts), а каждая
управляемая настройка — в [таблице настроек
`ManagedScript`](../../how-to/scripting/managed-csharp.md#настройка-backend).

В Windows пути `dotnet/runtime` находятся близко к `MAX_PATH`. Стадия CMake
документирует `FO_DOTNET_DIR` как переопределение: задайте его, в CMake или в
окружении, коротким путём до первой управляемой сборки.

## Выбор модели конкурентности

Серверные скрипты обычно выполняются в пуле рабочих потоков, поэтому каждая
точка входа скрипта должна доказать, что удерживает затрагиваемые сущности.
AngelScript использует операции cover времени выполнения (`Game.Sync`,
`Game.Lock`); Managed C# использует helpers `Sync`, которые проверяет анализатор
`FOSYNC`. Контракт описан в разделе [Жизненный цикл и конкурентность
скриптов](../../how-to/scripting/lifecycle-and-concurrency.md#серверная-синхронизация-сущностей).

`Server.SingleThreadedLogic` — способ отказаться от этого. Настройка доступна
только для чтения после запуска, относится к серверу и по умолчанию равна
`false` (см. [справочник настроек](../../reference/script-api/settings.md)).
При её включении:

- пул рабочих потоков сервера создаётся с одним потоком, и настройка имеет
  приоритет над `Server.WorkerThreads`;
- проверка доступа к сущностям принимает любую сущность, а вызовы получения
  cover ничего не захватывают, поэтому `Game.Sync` и `Game.SyncRelease` ничего
  не делают;
- `Game.Lock` и `Game.Unlock` по-прежнему берут singleton-корзину движка,
  которая не оспаривается;
- проверки времени жизни сущностей сохраняются: дескриптор, захваченный в одном
  задании, может ссылаться на сущность, уничтоженную до выполнения более
  позднего задания;
- режим читается для каждого экземпляра движка, поэтому по-разному настроенные
  серверы могут сосуществовать в одном процессе.

| Выбор | Когда | Цена |
| --- | --- | --- |
| По умолчанию (`false`) | Игре нужно, чтобы логика использовала несколько ядер, либо скрипты сохранят вызовы синхронизации для последующего переключения. | Каждая точка входа скрипта должна строить полный и корректный cover; ошибки — нарушения контракта, а не безобидные гонки. |
| `Server.SingleThreadedLogic = True` | Игра достаточно мала для одного рабочего потока логики, либо проект предпочитает последовательную семантику параллелизму. | Вся игровая логика делит один рабочий поток, поэтому параллельное масштабирование логики теряется. Настройка фиксируется на время запуска сервера. |

Отключение настройки позже восстанавливает полный контракт cover и не меняет
скрипты, сохранившие обычные вызовы синхронизации, но скрипты, написанные только
для последовательного режима, могут их не содержать. Для Managed C# исходники
анализатора Roslyn не содержат ссылок на эту настройку, поэтому при настроенном
анализаторе аннотации cover продолжают управлять сборкой в любом режиме. Этот
opt-out не зависит от backend и закреплён тестом
`ServerEngineSingleThreadedLogicRunsWithoutEntityCover` в
`Source/Tests/Test_ServerEngine.cpp`; полное описание принадлежит странице
[среды выполнения сервера](../runtime/server.md#синхронизация-и-блокировки-сущностей).

## Миграция между backend

Перенос проекта с AngelScript на Managed C# — это порт объявлений, жизненного
цикла, callbacks, именованных вызовов, синхронизации, тестов, пакетов ресурсов и
пакетов поставки, а не переименование файлов. Удаление файлов `.fos` без
удаления пакета baker AngelScript и без добавления пакета `Managed` не оставляет
ни одного активного игрового скрипта. Не меняйте публичные имена метаданных,
объявления удалённых вызовов, раскладки свойств и сохраняемые идентификаторы,
если миграция не меняет их намеренно, и не отправляйте случайно два обработчика
одного события. Соответствие атрибутов (`[[ModuleInit]]` → `[ModuleInit]`,
`[[Async]]`/`Yield` → `Task`/`ScriptTask.Delay`, cover времени выполнения →
`[RequiresCover]` и родственные) описано в [разделе миграции Managed
C#](../../how-to/scripting/managed-csharp.md#миграция-с-angelscript). Обратного
направления в руководствах Engine нет; считайте его тем же списком проверки,
применённым наоборот.

## Контрольный список настройки: AngelScript

1. Задайте `FO_ANGELSCRIPT_SCRIPTING ON` и `FO_MANAGED_SCRIPTING OFF` в
   `CMakeLists.txt` проекта; включите `FO_BUILD_ASCOMPILER` в preset или
   аргументах `-D`.
2. Добавьте `[ResourcePack]` с `Bakers = AngelScript` для каталога скриптов, как
   это делает `Examples/MinimalProject/FOnlineStarter.fomain`.
3. Определите настройки проекта `AngelScript.*`, например пространства имён, в
   которых разрешены изменяемые глобальные переменные; держите их минимальными.
4. Подключите форматтер `.fos`, сохраняющий nullable-синтаксис, и закрепите
   clang-format 20 ([контракт
   форматтера](../../how-to/scripting/style-and-refactoring.md#контракт-форматтера)).
5. Скомпилируйте каждую роль, до которой дотягиваются исходники, через
   `CompileAngelScript`, затем запустите `BakeResources`.
6. Решите, как задать `Server.SingleThreadedLogic`, и зафиксируйте это рядом с
   остальными стартовыми настройками проекта.
7. Добавьте тесты скриптов проекта и линию [process
   runner](../../how-to/testing/gameplay-and-integration.md); используйте
   `AngelScript.DebuggerEnabled` только в конфигурациях разработки.

## Контрольный список настройки: Managed C#

1. Установите .NET SDK, собирающий `ManagedScript.TargetFramework`, и закрепите
   его в проекте, чтобы все машины и CI-раннеры использовали один и тот же.
2. Задайте `FO_MANAGED_SCRIPTING ON` и `FO_ANGELSCRIPT_SCRIPTING OFF`; включите
   baker или другую опцию, собирающую библиотеку baker.
3. При желании заранее подготовьте исходники среды выполнения командой
   `prepare-workspace dotnet` и задайте короткий `FO_DOTNET_DIR` в Windows.
4. Добавьте настройки `ManagedScript.*` и пакет ресурсов `Managed`, включающий
   `CoreScripts` движка и каталог проекта.
5. Настройте `ManagedScript.Analyzers` и считайте предупреждения анализатора
   сбоями сборки.
6. Запустите `CompileManagedScripts`, затем `BakeResources`; убедитесь, что пакет
   содержит `Assemblies/Assemblies-<target>/` и `ManagedRuntime/`.
7. Решите, как задать `Server.SingleThreadedLogic`, и статически запретите
   `Task.Run`, `ConfigureAwait(false)` и обходы через ThreadPool.
8. Ограничьте sanitizer-линии конфигурациями AddressSanitizer или
   undefined-behavior.
9. Квалифицируйте каждую поставляемую платформу с её собственным runtime
   payload; никогда не используйте payload одной цели для другой.
10. Задокументируйте в проекте опции backend, закреплённую версию SDK, корни
    исходников, анализаторы и точные команды, как требует [граница документации
    проекта](../../how-to/scripting/managed-csharp.md#граница-документации-проекта).

## Контрольный список проверки

- [ ] Ровно одна из `FO_ANGELSCRIPT_SCRIPTING` и `FO_MANAGED_SCRIPTING`
  включена; `FO_NATIVE_SCRIPTING` выключена.
- [ ] Baker пакета ресурсов (`AngelScript` или `Managed`) соответствует опции
  CMake.
- [ ] Цель компиляции выбранного backend проходит для каждой роли без
  предупреждений.
- [ ] `BakeResources` завершается, и сервер запускается из запечённого вывода.
- [ ] `Server.SingleThreadedLogic` задана осознанно и задокументирована.
- [ ] Sanitizer-линии в CI существуют только для конфигураций, которые разрешает
  backend.
- [ ] Утверждения о платформах подкреплены приёмкой проекта, а не возможностью
  исходников Engine.
