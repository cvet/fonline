---
layout: default
title: Модульные тесты
permalink: /Source/Tests/README.ru.html
locale: ru
document_id: unit-tests-readme
---
<!-- docs-translation: {"document_id":"unit-tests-readme","locale":"ru","source_path":"Source/Tests/README.md","source_sha256":"10848253df3a7f279a26361271710bc5f4379403393e3a92830e1a6ebed56a6c"} -->
# Модульные тесты

Этот каталог содержит детерминированные тесты движка, встроенные в генерируемое тестовое приложение. Полная актуальная карта тестов, маршрутизация проверок и сведения о целях покрытия приведены в разделе [Тестирование](../../Docs/ru/contributing/testing/).

## Фреймворк и цель

- Фреймворк: Catch2 (`catch_amalgamated.hpp`)
- Точка входа тестового приложения: `Source/Applications/TestingApp.cpp`
- Владелец списка исходных файлов тестов: `BuildTools/cmake/stages/EngineSources.cmake` (`FO_TESTS_SOURCE`)
- Формат имени генерируемого исполняемого файла: `<ProjectDevName>_UnitTests`
- Генерируемая цель запуска: `RunUnitTests`
- Формат имени генерируемой цели покрытия: `<ProjectDevName>_CodeCoverage`, а также `RunCodeCoverage`, `GenerateCodeCoverageReport` и `AnalyzeCodeCoverage`, когда покрытие включено

Имя исполняемой цели использует префикс имени проекта для разработки (`<ProjectDevName>_UnitTests`); `RunUnitTests` является генерируемой целью запуска. Считайте префикс частью генерации проекта, а не универсальным API движка.

## Текущие наборы тестов

Полный список имён файлов и их количество, полученные из исходного кода, генерируются в [source-inventory.json](https://fonline.ru/Docs/generated/source-inventory.json). В разделе [Тестирование](../../Docs/ru/contributing/testing/) приведены актуальные группы владения и маршрутизация проверок.

После добавления, удаления или переименования файла `Test_*.cpp` перегенерируйте инвентарь из корня движка:

```bash
python BuildTools/docs_inventory.py --write
python BuildTools/docs_inventory.py --check
```

`FO_TESTS_SOURCE` безусловный. Translation units конкретного бэкенда содержат полные guards, общие native-наборы выполняются в обоих бэкендах. Header-фикстуры защищают AS-компиляцию; полный [контракт владения](../../Docs/ru/contributing/testing/) ведёт карта тестирования.

`Test_ManagedScriptRuntime.cpp` компилирует `Fixtures/ManagedRuntimeProbe.cs` настоящим baker/SDK и проверяет server/client/mapper bridge и изоляцию двух серверов. Требуются настроенные SDK/runtime; покрытие и пределы описывает [карта тестирования](../../Docs/ru/contributing/testing/).

Регрессии отсоединения при уничтожении выполняются при любом scripting backend: `CritterAttachmentTeardown`
проверяет связи сервера, native-отчёты и доставку оставшемуся игроку; `ClientCritterFinalizationDropsAttachmentIds`
проверяет очистку клиентского кеша с сохранением живого peer. Они входят в существующие наборы жизненного цикла сущностей сервера и клиента.

- `Source/Tests/Test_AudioBakerAcm.cpp` — опциональный декодер Fallout ACM и некорректные входные данные.
- `Source/Tests/Test_SpriteSheet.cpp` — независимые кадры, явные смещения, направления, кэш обработчика и границы.

## Запуск тестов

Предпочтительно запускайте генерируемую цель из настроенного каталога сборки:

```bash
cmake --build . --config RelWithDebInfo --target RunUnitTests
```

Запускайте исполняемую цель напрямую, когда требуются аргументы Catch2. Генерируемые тестовые файлы обычно размещаются в `Binaries/Tests-*`, например:

- Windows: `Binaries/Tests-Windows-win64/<ProjectDevName>_UnitTests.exe`
- Linux: `Binaries/Tests-Linux-x64/<ProjectDevName>_UnitTests`

## Запуск покрытия кода

Сборки с покрытием используют путь `FO_CODE_COVERAGE`, описанный в разделе [Тестирование](../../Docs/ru/contributing/testing/). Генерируются следующие цели:

- `RunCodeCoverage`
- `GenerateCodeCoverageReport`
- `AnalyzeCodeCoverage`

Отчёты о покрытии создаются в `CodeCoverage/<Toolchain>/<Platform-Config>/`,
а `Source/Tests/` исключается из знаменателя покрытия исходного кода.

## Общие вспомогательные средства тестов

Заголовочные вспомогательные средства находятся рядом с наборами тестов и не входят в `FO_TESTS_SOURCE`:

`Test_ClientEngineHelpers.h`, `Test_ClientServerIntegrationHelpers.h`, `Test_EntityLifecycleHelpers.h`, `Test_LocationAndEntityMgmtHelpers.h`, `Test_MapperHelpers.h`, `Test_ServerEngineHelpers.h`, `Test_ServerItemsHelpers.h` и `Test_ServerMapOperationsHelpers.h` разделяют построение native-ресурсов с наборами AngelScript. Компиляция для конкретного бэкенда остаётся под guards внутри этих фикстур.

- `Source/Tests/Test_BakerHelpers.h` содержит fixtures запечённых ресурсов (спрайтов, прототипов и metadata) и
  `TestRig`, который запускает настоящие bakers поверх источников в памяти.
- `Source/Tests/Test_ParticleFixtures.h` содержит fixtures ресурсов частиц.
- `Source/Tests/Test_ImGuiHarness.h` нажимает виджеты ImGui по label, чтобы ветвь за кнопкой,
  checkbox, selectable или свёрнутой секцией выполнялась в headless-кадре. Контракт закреплён
  тестом `ImGuiTestHarnessPressesWidgetsByLabel` в `Test_ImGui.cpp`; правила использования приведены
  в разделе [Тестирование](../../Docs/ru/contributing/testing/).
- `Source/Tests/Test_DumpArtifacts.h` запоминает каталоги `TexDump_*`, существующие в рабочем каталоге,
  чтобы набор, запускающий выгрузку atlas, удалял только артефакты, созданные собственным запуском.

## Примечания

- Сохраняйте тесты детерминированными и стабильными на всех платформах.
- Избегайте зависимости модульных тестов от сети, файловой системы и времени, если такая зависимость не замокана или не изолирована.
- Новые исходные файлы тестов необходимо добавлять в `FO_TESTS_SOURCE` в `BuildTools/cmake/stages/EngineSources.cmake`.
- При добавлении, удалении или перегруппировке наборов обновляйте раздел [Тестирование](../../Docs/ru/contributing/testing/) и этот README.
- Считайте `RunUnitTests` минимальной широкой проверкой изменений движка после прохождения сфокусированных тестов.
