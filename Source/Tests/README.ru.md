---
layout: default
title: Модульные тесты
permalink: /Source/Tests/README.ru.html
locale: ru
document_id: unit-tests-readme
---

<!-- docs-translation: {"document_id":"unit-tests-readme","locale":"ru","source_path":"Source/Tests/README.md","source_sha256":"8e8493dc53dd1a51b42413a31f6679ebd725cb82c2f515a089e5e995af41a2e6"} -->

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

Полный список имён файлов и их количество, полученные из исходного кода, генерируются в [source-inventory.json](../../Docs/generated/source-inventory.json). В разделе [Тестирование](../../Docs/ru/contributing/testing/) приведены актуальные группы владения и маршрутизация проверок.

После добавления, удаления или переименования файла `Test_*.cpp` перегенерируйте инвентарь из корня движка:

```bash
python BuildTools/docs_inventory.py --write
python BuildTools/docs_inventory.py --check
```

### Конфигурация, источники данных, файлы и кеши

- `Source/Tests/Test_CacheStorage.cpp`
- `Source/Tests/Test_ConfigFile.cpp`
- `Source/Tests/Test_DataSource.cpp`
- `Source/Tests/Test_FileSystem.cpp`
- `Source/Tests/Test_Settings.cpp`

### Базовая платформа, контейнеры и утилиты

- `Source/Tests/Test_BaseLogging.cpp`
- `Source/Tests/Test_BasicCore.cpp`
- `Source/Tests/Test_CommonHelpers.cpp`
- `Source/Tests/Test_Compressor.cpp`
- `Source/Tests/Test_Containers.cpp`
- `Source/Tests/Test_DataSerialization.cpp`
- `Source/Tests/Test_DiskFileSystem.cpp`
- `Source/Tests/Test_ExceptionHandling.cpp`
- `Source/Tests/Test_ExtendedTypes.cpp`
- `Source/Tests/Test_FunctionObjects.cpp`
- `Source/Tests/Test_GenericUtils.cpp`
- `Source/Tests/Test_GlobalData.cpp`
- `Source/Tests/Test_HashedString.cpp`
- `Source/Tests/Test_Logging.cpp`
- `Source/Tests/Test_MemorySystem.cpp`
- `Source/Tests/Test_Platform.cpp`
- `Source/Tests/Test_SafeArithmetics.cpp`
- `Source/Tests/Test_SettingsStorage.cpp`
- `Source/Tests/Test_SmartPointers.cpp`
- `Source/Tests/Test_StackTrace.cpp`
- `Source/Tests/Test_StringObject.cpp`
- `Source/Tests/Test_StringUtils.cpp`
- `Source/Tests/Test_StrongType.cpp`
- `Source/Tests/Test_TimeRelated.cpp`
- `Source/Tests/Test_WorkerPool.cpp`
- `Source/Tests/Test_WorkThread.cpp`

### Общая модель среды выполнения

- `Source/Tests/Test_AnyData.cpp`
- `Source/Tests/Test_ApplicationHeadless.cpp`
- `Source/Tests/Test_Common.cpp`
- `Source/Tests/Test_EngineMetadata.cpp`
- `Source/Tests/Test_EntityLifecycle.cpp`
- `Source/Tests/Test_EntityProtos.cpp`
- `Source/Tests/Test_Geometry.cpp`
- `Source/Tests/Test_LineTracer.cpp`
- `Source/Tests/Test_MapLoader.cpp`
- `Source/Tests/Test_Movement.cpp`
- `Source/Tests/Test_PathFinding.cpp`
- `Source/Tests/Test_Properties.cpp`
- `Source/Tests/Test_ProtoManager.cpp`
- `Source/Tests/Test_TextPack.cpp`
- `Source/Tests/Test_Timer.cpp`
- `Source/Tests/Test_TwoDimensionalGrid.cpp`

### Сеть и интеграция сервера с клиентом

- `Source/Tests/Test_ClientDataValidation.cpp`
- `Source/Tests/Test_ClientEngine.cpp`
- `Source/Tests/Test_ClientRuntimeApi.cpp`
- `Source/Tests/Test_ClientServerIntegration.cpp`
- `Source/Tests/Test_DataBase.cpp`
- `Source/Tests/Test_EntitySync.cpp`
- `Source/Tests/Test_FogOfWar.cpp`
- `Source/Tests/Test_LocationAndEntityMgmt.cpp`
- `Source/Tests/Test_ModelAnimation.cpp`
- `Source/Tests/Test_NetBuffer.cpp`
- `Source/Tests/Test_NetSockets.cpp`
- `Source/Tests/Test_NetworkClient.cpp`
- `Source/Tests/Test_NetworkServer.cpp`
- `Source/Tests/Test_NetworkUdp.cpp`
- `Source/Tests/Test_ServerAdvancedOps.cpp`
- `Source/Tests/Test_ServerEngine.cpp`
- `Source/Tests/Test_ServerEventContracts.cpp`
- `Source/Tests/Test_ServerItems.cpp`
- `Source/Tests/Test_ServerMapOperations.cpp`

### Скрипты и доступные из скриптов API

- `Source/Tests/Test_AngelScriptAlignment.cpp`
- `Source/Tests/Test_AngelScriptAttributes.cpp`
- `Source/Tests/Test_AngelScriptBytecode.cpp`
- `Source/Tests/Test_AngelScriptCall.cpp`
- `Source/Tests/Test_CommonScriptMethods.cpp`
- `Source/Tests/Test_ScriptBuiltins.cpp`
- `Source/Tests/Test_ScriptEntityOps.cpp`
- `Source/Tests/Test_ServerScriptMethods.cpp`

### Baker и инструменты

- `Source/Tests/Test_AngelScriptBaker.cpp`
- `Source/Tests/Test_BakerSetup.cpp`
- `Source/Tests/Test_ConfigBaker.cpp`
- `Source/Tests/Test_EffectBaker.cpp`
- `Source/Tests/Test_ImageBaker.cpp`
- `Source/Tests/Test_ImageWriter.cpp`
- `Source/Tests/Test_MapBaker.cpp`
- `Source/Tests/Test_Mapper.cpp`
- `Source/Tests/Test_MetadataBaker.cpp`
- `Source/Tests/Test_ModelBaker.cpp`
- `Source/Tests/Test_ModelBounds.cpp`
- `Source/Tests/Test_ParticleBaker.cpp`
- `Source/Tests/Test_ModelMeshData.cpp`
- `Source/Tests/Test_ModelAnimationData.cpp`
- `Source/Tests/Test_ModelAnimationConverter.cpp`
- `Source/Tests/Test_ModelAnimationPoseProcedural.cpp`
- `Source/Tests/Test_ModelAnimationRuntime.cpp`
- `Source/Tests/Test_ModelSkeletonCompatibility.cpp`
- `Source/Tests/Test_ModelSourceLoader.cpp`
- `Source/Tests/Test_OzzAnimation.cpp`
- `Source/Tests/Test_ProtoBaker.cpp`
- `Source/Tests/Test_ProtoTextBaker.cpp`
- `Source/Tests/Test_RawCopyBaker.cpp`
- `Source/Tests/Test_TextBaker.cpp`
- `Source/Tests/Test_TextureAtlas.cpp`

Покрытие конвейера моделей намеренно разделено. `Test_ModelMeshData.cpp`
владеет wire-контрактом только для mesh; `Test_ModelSourceLoader.cpp` и
`Test_ModelAnimationConverter.cpp` владеют извлечением исходных данных и
каноническим преобразованием; `Test_ModelAnimationData.cpp` владеет
версионированным архивом rig; `Test_ModelBaker.cpp` пересекает основанный на
исходных данных baking и разрешение привязок; наборы animation, runtime-pose,
procedural, skeleton-compatibility и Ozz покрывают производственный путь
семплирования и матриц. Поддерживайте эти границы независимо зелёными, затем
используйте `Test_ClientEngine.cpp` для границы между baker и парсером клиента.

### Тесты рендеринга и frontend

- `Source/Tests/Test_EffekseerParticleRuntime.cpp` - прогоняет подготовленные legacy- и modern-эффекты Effekseer через реальные callbacks Sprite/Ring нативной среды выполнения и проверяет детерминированную топологию нескольких экземпляров, геометрию FOnline, UV атласа, все три режима Z-sort, разбиение Ring по бюджету индексов и повторное применение масштаба на уровне facade без respawn или сброса времени.
- `Source/Tests/Test_ImGui.cpp`
- `Source/Tests/Test_ModelSpriteLayout.cpp`
- `Source/Tests/Test_ParticleBaker.cpp` - покрывает обнаружение исходных `.efkproj`, сопоставление выходных ключей `.spark`/`.efkproj`, проверку генерируемых бинарных данных, отклонение авторских `.spk`/`.efk` на входе среды выполнения и изоляцию seeded-stream SPARK между чередующимися эффектами и независимыми контекстами движка. Путь сборки и интеграционного baking применяет нативный экспортер с фиксированным профилем к реальным XML-проектам.
- `Source/Tests/Test_Rendering.cpp`

CI-задача документации отклоняет устаревший сгенерированный инвентарь. Заголовки выше отражают текущие исходные файлы для навигации, но не заменяют полный сгенерированный список.

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
