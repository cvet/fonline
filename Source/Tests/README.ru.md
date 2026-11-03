---
layout: default
title: Модульные тесты
permalink: /Source/Tests/README.ru.html
locale: ru
document_id: unit-tests-readme
---
<!-- docs-translation: {"document_id":"unit-tests-readme","locale":"ru","source_path":"Source/Tests/README.md","source_sha256":"04362e9ccb117d89993a2b0f29a13a30646dcf281ed4e0c8f3ccdcc413ae68c8"} -->
# Модульные тесты

Детерминированные тесты движка; карта, проверки и покрытие ведутся в разделе [Тестирование](../../Docs/ru/contributing/testing/).

## Фреймворк и цель

- Фреймворк: Catch2 (`catch_amalgamated.hpp`)
- Точка входа тестового приложения: `Source/Applications/TestingApp.cpp`
- Владелец списка исходных файлов тестов: `BuildTools/cmake/stages/EngineSources.cmake` (`FO_TESTS_SOURCE`)
- Формат имени генерируемого исполняемого файла: `<ProjectDevName>_UnitTests`
- Генерируемая цель запуска: `RunUnitTests`
- Переключатель покрытия: `FO_CODE_COVERAGE`; цели ниже.

`<ProjectDevName>` — префикс встраивающего проекта, а не универсальный API.

## Текущие наборы тестов

Генерируемые имена и количество файлов: [source-inventory.json](https://fonline.ru/Docs/generated/source-inventory.json); владение и проверки: [Тестирование](../../Docs/ru/contributing/testing/).

`Test_RpcRuntimeTracker.cpp` — детерминированный набор без сети для учёта RPC.
Он фиксирует включительные границы warning/disconnect, ограничение предупреждений,
учёт вызовов и предел pending, ограниченное объединение истории прихода,
выбор ожидающего фрейма и сохранение полного 64-битного хеша. Явные фикстуры
`nanotime` включают нулевые отметки.

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

Из настроенного каталога сборки:

```bash
cmake --build . --config RelWithDebInfo --target RunUnitTests
```

Передавайте аргументы Catch2 напрямую исполняемому файлу в `Binaries/Tests-*`:

- Windows: `Binaries/Tests-Windows-win64/<ProjectDevName>_UnitTests.exe`
- Linux: `Binaries/Tests-Linux-x64/<ProjectDevName>_UnitTests`

## Запуск покрытия кода

Сборки с покрытием используют путь `FO_CODE_COVERAGE`, описанный в разделе [Тестирование](../../Docs/ru/contributing/testing/). Генерируются следующие цели:

- `RunCodeCoverage`
- `GenerateCodeCoverageReport`
- `AnalyzeCodeCoverage`

Исполняемая цель: `<ProjectDevName>_CodeCoverage`.

Отчёты о покрытии создаются в `CodeCoverage/<Toolchain>/<Platform-Config>/`,
а `Source/Tests/` исключается из знаменателя покрытия исходного кода.

## Общие вспомогательные средства тестов

Заголовочные вспомогательные средства находятся рядом с наборами тестов и не входят в `FO_TESTS_SOURCE`:

`Test_ClientEngineHelpers.h`, `Test_ClientServerIntegrationHelpers.h`, `Test_EntityLifecycleHelpers.h`, `Test_LocationAndEntityMgmtHelpers.h`, `Test_MapperHelpers.h`, `Test_ServerEngineHelpers.h`, `Test_ServerItemsHelpers.h` и `Test_ServerMapOperationsHelpers.h` разделяют построение native-ресурсов с наборами AngelScript. Компиляция для конкретного бэкенда остаётся под guards внутри этих фикстур.

- `Source/Tests/Test_BakerHelpers.h` содержит fixtures запечённых ресурсов (спрайтов, прототипов и metadata) и
  `TestRig` для настоящих bakers в памяти. AS-фикстуры ресурсов вызывают `AddBakedScriptBytecode` перед
  выпечкой server proto: реальные выходы трёх ролей копируются в `BakedFiles` до очистки `Outputs`
  следующим `MakeContext`. Дисковые фикстуры включают AngelScript вместе с Metadata перед Proto.
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
