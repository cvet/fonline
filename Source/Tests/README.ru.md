---
layout: default
title: Модульные тесты
permalink: /Source/Tests/README.ru.html
locale: ru
document_id: unit-tests-readme
---
<!-- docs-translation: {"document_id":"unit-tests-readme","locale":"ru","source_path":"Source/Tests/README.md","source_sha256":"cf5f6423c14e87de297d42b75249c486ae1560b29d5e94388178c58899cb9236"} -->
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

`Test_ClientEntityLifetime.cpp` выполняется с любым scripting backend. Он проверяет, что результат поиска в registry остаётся живым при освобождении последней другой ссылки из другого потока и что старая сущность не может удалить регистрацию новой сущности с тем же id.

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

## Запуск тестов

Предпочтительно запускайте генерируемую цель из настроенного каталога сборки:

```bash
cmake --build . --config RelWithDebInfo --target RunUnitTests
```

Запускайте исполняемую цель напрямую, когда требуются аргументы Catch2. Генерируемые тестовые файлы обычно размещаются в `Binaries/Tests-*`, например:

- Windows: `Binaries/Tests-Windows-win64/<ProjectDevName>_UnitTests.exe`
- Linux: `Binaries/Tests-Linux-x64/<ProjectDevName>_UnitTests`

## Отдельное измерение стоимости пакета поз

Скрытый (`[.]`) тест `ClientPoseBatchCost` в `Test_ClientPoseBatchCost.cpp` требует 3D. Запускайте только его из каталога с `ClientPoseBatchCost.json`. Поля: `schema: 1`, `rigFile` (полный архив `WriteModelAnimationRigData`), `nearestSampling` (bool), `clipIndex` (0..65534), `poseCount` (1..1024), `workers` (0..64), `samples` (2..2000), `iterations` (1..64), `warmup` (1..512). Каждая дорожка ограничена четырьмя миллионами измеряемых вычислений.

Тест чередует порядок прямого и пакетного расчёта при одинаковых временах clip, побайтово сравнивает все матрицы на владельце и сохраняет `samples.csv` и `result.json` с сигнатурами rig и счётчиками пакетов. Ноль workers означает прямую петлю; с workers тест обходит `ShouldRunParallel`, поэтому даже один элемент измеряет dispatch одного chunk. Прогрев, загрузка, сравнение результатов и файловый I/O не измеряются; dispatch/join scheduler измеряются. Нужны тихий хост, сырые пары и повторы. Замеры ядра не подтверждают ускорение кадра, визуальную или платформенную приёмку.

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
