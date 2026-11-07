---
layout: default
title: Добавление и запуск тестов в игровом проекте
locale: ru
document_id: project-tests
permalink: /Docs/ru/how-to/project/project-tests.html
---

# Добавление и запуск тестов в игровом проекте
<!-- docs-translation: {"document_id":"project-tests","locale":"ru","source_path":"Docs/en/how-to/project/project-tests.md","source_sha256":"d15d824b978236467ca56d2f6ddf6423c6996ceb2e9ef99843e7675cdc6f68d4"} -->
Это руководство для игровой команды, которой нужны автоматические тесты сверх
проверки «запускается». Оно показывает, как выбрать самую дешёвую границу теста,
способную наблюдать изменение, как добавить нативные unit-тесты через роль
исходников `TESTS`, как построить тестовую обвязку уровня скриптов, управляемую
подконфигурацией и настройками, определёнными в проекте, как запускать реальные
процессы сервера и клиента, как не допустить тестовые исходники в поставляемые
пакеты ресурсов и как решить, что прогон прошёл.

Движок предоставляет нативный тестовый исполняемый файл, раннер процессов и
правила для маркеров и кодов выхода. Он не определяет реестр скриптовых тестов
игры как API движка, а также фикстуры, учётные записи или пороги приёмки; всё
это код проекта. Для скриптовых тестов он поставляет копируемый пример обвязки,
который проект принимает как собственный код; см.
[Шаблон обвязки скриптовых тестов для AngelScript и Managed C#](script-test-harness.md).

## Проверенные исходные пути

- `BuildTools/gameplay_test_runner.py`
- `BuildTools/cmake/RunTestExecutable.cmake`
- `BuildTools/cmake/helpers/Build.cmake`
- `BuildTools/cmake/stages/Applications.cmake`
- `BuildTools/cmake/stages/EngineSources.cmake`
- `BuildTools/cmake/stages/ScriptsAndBaking.cmake`
- `Source/Applications/TestingApp.cpp`
- `Source/Common/Settings.inc`
- `Source/Common/Settings.cpp`
- `Source/Tests/Test_Settings.cpp`
- `Source/Tools/MetadataBaker.cpp`
- `Source/Tools/ConfigBaker.cpp`
- `Examples/GameplayTestHarness/README.md`
- `Examples/GameplayTestHarness/synthetic-smoke.json`
- `Examples/GameplayTestHarness/ScriptTests/script-tests.json`
- `Examples/MinimalMultiplayer/CMakeLists.txt`
- `Examples/MinimalMultiplayer/FOnlineMinimalMultiplayer.fomain`
- `Examples/MinimalMultiplayer/Scripts/Tutorial.fos`
- `Examples/MinimalMultiplayer/tutorial-smoke.json`
- `Examples/NativeExtensionSample/CMakeLists.txt`
- `Examples/NativeExtensionSample/Tests/NativeExtensionCoreTest.cpp`
- `Docs/en/how-to/testing/gameplay-and-integration.md`
- `Docs/en/how-to/project/script-test-harness.md`
- `Docs/en/contributing/testing/index.md`
- `Docs/en/tutorials/first-test.md`
- `Docs/en/how-to/native-extensions.md`
- `Docs/en/how-to/build/project-configuration.md`
- `Docs/en/how-to/build/generated-content.md`
- `Docs/en/explanation/content-pipeline/baking.md`
- `Docs/en/reference/metadata/index.md`
- `Docs/en/reference/script-api/settings.md`

## Выбор границы теста

Выберите первый слой, способный наблюдать контракт, и добавляйте более широкий,
только когда поведение пересекает ещё одну границу. Полной таблицей выбора
владеет страница
[Gameplay- и integration-тестирование](../testing/gameplay-and-integration.md);
краткая сводка для проекта такова:

| Что изменилось | С чего начать | Что нужно |
|---|---|---|
| Чистая нативная функция или структура данных в `SourceExt/` | Нативный unit-тест (Catch2) через роль `TESTS` | `FO_UNIT_TESTS=ON` |
| Скриптовый модуль, callback, инвариант контента или прототип | Тест уровня скриптов в одном headless-сервере, выбираемый подконфигурацией | Обвязка проекта (см. ниже) |
| Видимый клиенту результат, удалённый вызов или реплицируемое состояние | Реальные headless-процессы сервера и клиента через раннер процессов | Собранный контент, упорядоченные маркеры |
| Сохранность данных после перезапуска или старое сохранение | Тест скриптов или процессов на изолированной файловой базе данных | См. [миграции данных](data-migrations.md) |
| Упаковка, updater или поведение платформы | Маршрут проверки пакета | См. [непрерывную интеграцию](continuous-integration.md) |

Широкий smoke-тест никогда не заменяет точной локализации сбоя, а unit-тест не
может доказать границу процесса, сети или собранного контента, в которую он не
входит.

## Добавление нативных unit-тестов

1. Включите тестовые цели в корневом `CMakeLists.txt` до генерации проекта.
   `Examples/MinimalProject/CMakeLists.txt` и
   `Examples/MinimalMultiplayer/CMakeLists.txt` используют
   `SetOption(FO_UNIT_TESTS ON)`.
2. Напишите тестовые случаи Catch2 в файле `.cpp` с теми же include, что и в
   тесте Engine: сначала `Common.h`, затем `catch_amalgamated.hpp`.

   ```cpp
   #include "Common.h"

   #include "catch_amalgamated.hpp"

   FO_BEGIN_NAMESPACE

   TEST_CASE("ProjectCooldownRoundsUp", "[project][cooldown]")
   {
       CHECK(1 + 1 == 2);
   }

   FO_END_NAMESPACE
   ```

3. Зарегистрируйте файл с ролью `TESTS` между настройкой сторонних библиотек и
   `RegisterEngineSources()`:

   ```cmake
   AddEngineSources(
       SERVER SourceExt/ServerExtension.cpp
       TESTS SourceExt/Test_ProjectExtension.cpp)
   ```

   Роль `TESTS` компилирует файл непосредственно в исполняемый файл unit-тестов
   и исполняемый файл покрытия. Она ничего не добавляет в библиотеки среды
   выполнения и не имеет цели экспорта в скрипты. Роли передаются парами;
   нечётное число аргументов вызывает ошибку на этапе configure.

4. Повторите configure, соберите и запустите. Сгенерированные имена получают
   префикс development-имени проекта:

   ```bash
   cmake --build <build-dir> --config RelWithDebInfo --target RunUnitTests
   ```

   Исполняемый файл называется `<ProjectDevName>_UnitTests` и находится в
   `Binaries/Tests-*`. Чтобы выполнить подмножество, запустите его напрямую с
   тегом или именем Catch2, например `<ProjectDevName>_UnitTests "[project]"`.

Для планирования важны два свойства сборки. Судя по спискам исходников CMake,
`FO_TESTS_SOURCE` содержит собственные тестовые файлы Engine наряду с вашими
файлами `TESTS`, поэтому `RunUnitTests` в игровом проекте запускает и набор
тестов Engine; дайте проектным тестовым случаям отличительный тег, например
`[project]`, и для быстрых прогонов запускайте исполняемый файл напрямую. Кроме
того, `RunUnitTests` проходит, только если исполняемый файл завершается с кодом 0
и его вывод содержит итоговую сводку Catch2 (`All tests passed (...)` или
итоговую строку `test cases:`); исполняемый файл, завершившийся с кодом 0 без
сводки, считается сбоем (`BuildTools/cmake/RunTestExecutable.cmake`).

Чистую библиотеку можно тестировать и без Catch2:
`Examples/NativeExtensionSample` собирает своё ядро как статическую библиотеку с
`-Werror`, линкует её в небольшой исполняемый файл, который возвращает ненулевой
код при сбое и печатает `native_extension_core_test_passed`, и подключает оба к
пользовательской цели. Используйте эту схему, если у тестируемого кода нет
зависимостей от Engine.

## Построение тестовой обвязки уровня скриптов

Движок не определяет внутри игры ни реестра тестов, ни атрибута для их
обнаружения, ни раннера. Проект строит самую маленькую подходящую обвязку:
настройки, включающие тестовый режим, событие входа, выполняющее проверки, маркер
на каждую проверенную веху и чистый выход. Эталонная форма:
`Examples/MinimalMultiplayer/Scripts/Tutorial.fos`.

Если проверок больше нескольких, скопируйте вместо этого ядро обвязки из
`Examples/GameplayTestHarness/ScriptTests`. Оно добавляет регистрацию наборов,
фильтр, тайм-аут на каждый тест, остановку на первом провале и завершающую
строку `[Testing] COMPLETE status=passed` для раннера процессов, используя те же
настройки `Testing.*`. Страница
[Шаблон обвязки скриптовых тестов для AngelScript и Managed C#](script-test-harness.md)
описывает контракт маркеров, настройки и расширение обвязки в проекте.

### Объявление настроек

Настройка проекта объявляется тегом метаданных, а обращаются к ней по полному
имени через точку. Тег имеет вид
`///@ Setting <Common|Server|Client> <type> <Group.Name>`; тип должен быть
примитивом, enum или строкой, а имя должно содержать группу. Главный `.fomain`
должен содержать значение для каждой объявленной настройки, иначе
`MetadataBaker` проваливает сборку ресурсов («setting has no configured value»).
Группа `Testing` — выбор проекта, а не группа Engine:

```angelscript
#if SERVER
///@ Setting Server bool Testing.Enabled
///@ Setting Server string Testing.Filter
#endif
```

```ini
Testing.Enabled = False
Testing.Filter =

[SubConfig]
Name = ScriptTests
Testing.Enabled = True
ServerNetwork.DisableNetworking = True
Server.DbStorage = Memory
```

Проекты Managed C# объявляют те же теги в файлах `.cs` и читают их как
`Settings.Testing.Enabled`; см. [Managed C#](../scripting/managed-csharp.md).
Решения о детерминированных фикстурах держите в подконфигурации: без сети,
хранилище в памяти, фиксированные seed, headless-рендеринг и выключенный звук
(подконфигурация `TutorialSmoke` в примере показывает клиентскую сторону).

### Выполнение проверок и вывод маркеров

```angelscript
#if SERVER
[[ModuleInit]]
void InitTests()
{
    Game.OnStart.Subscribe(OnTestStart);
}

[[Event]]
void OnTestStart()
{
    if (!Settings.Testing.Enabled) {
        return;
    }

    verify(Game.CheckProtoItem("ExampleItem".hstr()), "Example prototype is missing");

    Game.Log("project_test_content_passed");
    Game.RequestQuit(true);
}
#endif
```

`verify` — проверка инварианта, которую используют скрипты примеров; кто владеет
её определением, см. на странице
[Nullable-типы](../../contributing/coding-contracts/nullability.md). Проверки
сообщают о сбое исключением; маркер пишется в лог только после того, как
проверяемое состояние существует, а `Game.RequestQuit(true)` задействует штатное
завершение работы.

### Выбор тестов фильтром

Фильтрация — политика проекта. Простая схема — строка `Testing.Filter` выше:
событие входа читает её и запускает только подходящую группу, а пустое значение
запускает всё. Передавайте её как обычное переопределение настройки в командной
строке в той же форме `-Group.Name value`, которую использует цель сборки
ресурсов Engine (`-Baking.ForceBaking False`). Фильтр — лишь удобство при
запуске; CI выполняет набор без фильтра. Копируемая обвязка реализует эту схему
токенами через запятую без учёта регистра, которые сопоставляются с
`suite.name`, и проваливает прогон, в котором не выбран ни один тест.

### Запуск

```bash
<server-headless-binary> -ApplyConfig <project>.fomain -ApplySubConfig ScriptTests
```

```powershell
& "<server-headless-binary>" -ApplyConfig "<project>.fomain" -ApplySubConfig ScriptTests
```

Выбор подконфигурации, приоритеты и цепочки `Parent` описаны в
[конфигурации игрового проекта](../build/project-configuration.md). Стандартные
цели `BakeResources` и `ForceBakeResources` используют только главную
конфигурацию (`-ApplySubConfig NONE`), поэтому настройки, которые должны
существовать во время сборки ресурсов, должны находиться в главном файле.

## Запуск реальных процессов и решение о прохождении

`BuildTools/gameplay_test_runner.py` выполняет JSON-манифест упорядоченных
процессов без оболочки. Для каждого процесса он проверяет `required_markers`,
`forbidden_markers` и `expected_exit_code`, соблюдает один крайний срок
(deadline) на сценарий и при сбое завершает каждый запущенный процесс. Схема
манифеста, семантика готовности и порядок очистки описаны на странице
[Gameplay- и integration-тестирование](../testing/gameplay-and-integration.md).

```bash
python3 Engine/BuildTools/gameplay_test_runner.py \
  --manifest Tests/gameplay-smoke.json \
  --value server=<server-headless-binary> \
  --value config=<project>.fomain \
  --report Workspace/gameplay-smoke-report.json
```

```powershell
py -3 Engine/BuildTools/gameplay_test_runner.py `
  --manifest Tests/gameplay-smoke.json `
  --value "server=<server-headless-binary>" `
  --value "config=<project>.fomain" `
  --report Workspace/gameplay-smoke-report.json
```

Код выхода `0` означает, что прошли все сценарии, `1` — что нарушен контракт
процесса, маркера, готовности или таймаута, а `2` — что входные данные CLI или
манифеста недопустимы. `Examples/MinimalMultiplayer/tutorial-smoke.json`
показывает сценарий теста контента, за которым следует сценарий сервера и
клиента.

Принимайте прогон только по положительным доказательствам:

- код выхода раннера (или процесса) равен ожидаемому значению;
- каждый обязательный маркер появился в выводе процесса, которому он
  принадлежит;
- ни один запрещённый маркер не появился;
- для `RunUnitTests` присутствует итоговая сводка Catch2.

«В логе нет ошибок» — это не приёмка: процесс, который падает до первой
проверки, тоже не выдаёт ни одной строки с ошибкой. Считайте частичный подсчёт,
выполняющееся фоновое задание или просто запущенную команду непройденными. Не
увеличивайте deadline, чтобы скрыть отсутствующий маркер; найдите, какой процесс
первым перестал выдавать маркеры.

## Исключение тестовых исходников из поставляемых пакетов

Поместите тестовые скрипты в отдельный каталог, укажите его как входной каталог
каждого пакета, который его читает, и исключите его из поставляемой сборки
ресурсов через `Baking.IgnoreInputDirs`. Настройка объявлена как
`string[] Baking.IgnoreInputDirs`: «Каталоги входов ресурсных пакетов,
исключаемые из всех пакетов относительно конфигурации, объявившей пакет; каждый
должен быть входом какого-либо пакета». Имя, не являющееся входом ни одного
пакета, приводит к сбою применения настроек с `SettingsException`, поэтому
опечатка в записи не может незаметно отправить тесты в поставку.

```ini
[SubConfig]
Name = Shipping
Baking.IgnoreInputDirs = Tests/Scripts

[ResourcePack]
Name = Metadata
InputDirs = Scripts Tests/Scripts
IncludePatterns = *
Bakers = Metadata
```

Повторите тестовый каталог в каждом пакете, который его читает, включая пакет
метаданных, чтобы тестовые объявления ушли и из метаданных;
`Source/Tests/Test_Settings.cpp` закрепляет это поведение
(`IgnoreInputDirsFromASubConfigLeaveTheDirectoryOutOfEveryPack`). Создайте цель
сборки ресурсов, применяющую подконфигурацию:

```cmake
SetupScriptsAndBaking()

AddBakingTarget(Game_ShippingResources
    SUB_CONFIG Shipping
    FORCE
    COMMENT "Bake shipping resources without tests")
```

Примечания:

- `AddBakingTarget` описан в разделе
  [Конвейер запекания ресурсов](../../explanation/content-pipeline/baking.md#точки-входа-cmake).
  Цель использует тот же выходной каталог, что и `BakeResources`. Изменение
  состава пакетов требует принудительной сборки ресурсов, поэтому используйте
  `FORCE` и никогда не запускайте сборку ресурсов для разработки и поставляемую
  сборку ресурсов одновременно.
- Запись `ExcludePatterns` на уровне пакета — статическая альтернатива, если
  тесты никогда не должны попадать в сборку ресурсов ни одного варианта.
- Поскольку версия метаданных учитывает каждую объявленную настройку, у
  поставляемой сборки ресурсов и сборки для разработки разные метаданные. Это
  ожидаемо; инвариант состоит в том, что сервер и его клиенты получены из одной
  сборки ресурсов.
- Сборщик конфигурации пишет в лог `Unknown setting` для заданного значения,
  объявление которого было исключено (`Source/Tools/ConfigBaker.cpp`). Решите,
  считает ли ваш конвейер эту строку сбоем; это поведение прочитано в исходном
  коде, а не наблюдалось при сборке ресурсов.
- Проверьте результат: в проверке после сборки ресурсов утверждайте, что ни один
  тестовый ресурс не попал в поставляемые пакеты, и запустите smoke-тест
  упакованной сборки, полученной из поставляемой сборки ресурсов.

## Детерминированность и изоляция тестов

- Начинайте каждый тест со свежих рабочего каталога, портов, учётных записей и
  пути вывода. Никогда не направляйте тест на production-хранилище, учётные
  данные или сервисы.
- Для тестов логики используйте `Server.DbStorage = Memory`. Тесту, который
  заявляет сохранность данных, нужны файловый каталог JSON или SQLite
  (`JSON <directory>`, `DbSQLite <directory>`), остановка и перезапуск на том же
  каталоге. Формы хранилища описаны на странице
  [Резервное копирование и восстановление](../release/backup-and-recovery.md).
- Фиксируйте seed случайности или убирайте её и ограничивайте каждое ожидание.
  Фиксированная пауза может задавать темп фикстуры, но никогда не является
  условием прохождения.
- Создавайте только те сущности, которые нужны проверке, и убирайте то, что
  создал тест.
- Убедитесь, что новая проверка может провалиться: временно сломайте её вход,
  пронаблюдайте сбой, затем восстановите вход.
  [Руководство по первому тесту](../../tutorials/first-test.md) использует такой
  повтор «сначала сбой, затем успех».
- Держите одного писателя на каждый вывод сборки ресурсов. `MasterBaker`
  удерживает эксклюзивную блокировку вывода, а пример делает обе проверки,
  зависящие от сборки ресурсов, зависимыми от `ForceBakeResources`, чтобы никакие
  два сборщика ресурсов не состязались за `Baking/`. Не запускайте
  gameplay-тесты на выводе, пока идёт сборка ресурсов этого вывода, и не удаляйте
  lock-файл, чтобы обойти отказ.

## Тесты как условие прохождения CI

Запускайте слои в порядке зависимостей и останавливайтесь на первом проваленном
слое: нативные unit-тесты, затем скриптовые тесты контента, затем smoke
headless-процессов, затем проверки пакетов. Сохраняйте отчёт раннера вместе с
логом процесса. CI-задача проходит только по итоговым доказательствам,
описанным выше, а не по отсутствию сообщения о сбое. Размещение, кэширование и
хранение артефактов описаны в руководстве
[Настройка непрерывной интеграции для игрового проекта](continuous-integration.md);
место тестов в локальном агрегирующем маршруте описано на странице
[Поддержание здоровья игрового проекта](quality-gates.md).

## Контрольный список проверки

- Нативные тестовые случаи имеют проектный тег и запускаются как напрямую через
  исполняемый файл, так и через `RunUnitTests`.
- Тестовые настройки объявлены через `///@ Setting`, имеют значения в главном
  `.fomain` и включаются только подконфигурацией.
- Каждый сценарий указывает свои обязательные маркеры, запрещённые маркеры,
  deadline и ожидаемый код выхода.
- Сценарий скриптовых тестов требует завершающую строку обвязки
  `[Testing] COMPLETE status=passed`, а не подсчёт строк об успехе.
- Цель поставляемой сборки ресурсов применяет `Baking.IgnoreInputDirs`, а
  проверка доказывает, что в её пакетах нет тестов.
- Заявления о сохранности данных проверены перезапуском файловой базы данных, а
  не на `Memory`.
- Ни один тест не запускается, пока идёт сборка ресурсов того же вывода.
