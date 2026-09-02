---
layout: default
title: Тестирование
locale: ru
document_id: testing
permalink: /Docs/ru/contributing/testing/
---

# Тестирование

<!-- docs-translation: {"document_id":"testing","locale":"ru","source_path":"Docs/en/contributing/testing/index.md","source_sha256":"b8a5a8eac73a777f3b31f8ffc9006b7360e46fcd19388fbb0cf9c5e2e5af51e8"} -->

> Документация принадлежит движку. Страница описывает текущий test executable,
> сгенерированные test/coverage targets и полный набор suites из
> `Source/Tests/Test_*.cpp`.

## Назначение

Используйте эту страницу, чтобы подобрать native-проверку для изменения движка
или добавить Catch2-тест. Краткая точка входа находится в
[Source/Tests/README.ru.md](../../../../Source/Tests/README.ru.md), а здесь
поддерживается полная карта. Для детерминированных script/content/server/client
процессов проекта продолжайте с
[Gameplay и integration testing](../../how-to/testing/gameplay-and-integration.md).

## Проверенные исходные пути

- `Source/Applications/TestingApp.cpp`
- `Source/Tests/README.md` и все текущие `Source/Tests/Test_*.cpp`
- `BuildTools/cmake/stages/EngineSources.cmake`
- `BuildTools/cmake/stages/Applications.cmake`
- `BuildTools/cmake/stages/Init.cmake`
- `BuildTools/cmake/helpers/RunAndLog.cmake`
- `BuildTools/codecoverage.py`
- `BuildTools/validate.sh` и `BuildTools/validate.cmd`

## Модель test runner

`Source/Applications/TestingApp.cpp` является entry point тестового приложения.
Он требует `FO_TESTING_APP`, вызывает `InitAppForTesting()`, выставляет
`IsTestingInProgress` и передает выполнение в
`Catch::Session().run(argc, argv)`.

`EngineSources.cmake` владеет явным списком `FO_TESTS_SOURCE`.
`Applications.cmake` строит executable через `SetupTestBuild(name)`:

- `UnitTests`, когда включен `FO_UNIT_TESTS`;
- `CodeCoverage`, когда включен `FO_CODE_COVERAGE`.

Стандартные имена используют development-префикс проекта:
`<ProjectDevName>_UnitTests`, `RunUnitTests`,
`<ProjectDevName>_CodeCoverage`, `RunCodeCoverage`,
`GenerateCodeCoverageReport`, `AnalyzeCodeCoverage`. Префикс генерирует проект,
он не является универсальным именем движка.

Отдельный `BuildTools/check_windows7_imports.py <binary> [...]` проверяет один
или несколько PE-файлов, fail-closed обрабатывает поврежденный ввод и запрещает
импорт `CreateFile2`. Проектная CI-ветка Windows 7 должна запускать его для всех
связанных executable и runtime DLL после линковки и до упаковки; см.
[Windows 7 compatibility lane](../../how-to/build/#контур-совместимости-с-windows-7).

## Запуск тестов

Предпочтительная локальная проверка из настроенной build directory:

```bash
cmake --build . --config RelWithDebInfo --target RunUnitTests
```

При включенном `FO_EFFEKSEER_PARTICLES` focused `[particle]` cases вызывают
публичный helper через production-путь `ParticleBaker`: проверяются text
compilation, dependency invalidation, malformed XML и запрет cooked-файлов как
authored inputs.

Executable можно вызвать напрямую с аргументами Catch2. Он находится под
`Binaries/Tests-*`, например
`Binaries/Tests-Windows-win64/<ProjectDevName>_UnitTests.exe` или
`Binaries/Tests-Linux-x64/<ProjectDevName>_UnitTests`.

Tests dump-ов atlas и render target используют `TexDumpArtifacts` из
`Source/Tests/Test_DumpArtifacts.h`. До production dump test сохраняет snapshot
существующих каталогов `TexDump_*`, а затем удаляет только новые каталоги своего
run. Поэтому parallel или прерванный test session не удаляет ранее собранные
diagnostic evidence, а cleanup ограничен artifacts с доказанным ownership.

Для Visual Studio/MSBuild `RunUnitTests` пишет process output в
`<build-dir>/<ProjectDevName>_UnitTests.log` и использует exit code процесса.
Так ожидаемые строки `error` из negative cases не превращаются в ошибки MSBuild.
При failure helper также выводит captured output перед остановкой, поэтому CI log
называет failing test/assertion даже после удаления runner workspace и file log.

BuildTools может запускать выбранные широкие сценарии:

```bash
Engine/BuildTools/validate.sh unit-tests
Engine/BuildTools/validate.sh android-arm64-client linux-client linux-server
```

Начинайте с минимального focused test и добавляйте общий target, когда изменение
пересекает границы подсистем.

### Unit-тесты под sanitizers

Выделенные validators выбирают соответствующий `San_*` build type и запускают
инструментированный `RunUnitTests`:

```bash
Engine/BuildTools/validate.sh unit-tests-san-address    # AddressSanitizer (+LeakSanitizer)
Engine/BuildTools/validate.sh unit-tests-san-memory     # MemorySanitizer (requires Workspace/msan-libcxx)
Engine/BuildTools/validate.sh unit-tests-san-undefined  # UndefinedBehaviorSanitizer
Engine/BuildTools/validate.sh unit-tests-san-thread     # ThreadSanitizer
```

Workflow `validate.yml` выполняет их матрицей `unit-tests-sanitizers`; все четыре
ветки блокирующие. MemorySanitizer подготавливает `Workspace/msan-libcxx` из
инструментированных `libc++`, `libc++abi`, `libunwind` и передает
`FO_MSAN_LIBCXX_ROOT`. Узкий libunwind ignorelist не дает unwinding-у исключений
самому срабатывать на ABI snapshots. Native stack capture и backward-cpp signal
handler отключаются под MSan и TSan, чтобы reports принадлежали runtimes
sanitizer; кроме того, symbolization backward-cpp/libbfd под TSan приводит к
неприемлемому росту shadow memory. Более медленный
`unit-tests-san-memory-with-origins` предназначен для
локальной диагностики. `San_DataFlow` не подключен: DataFlowSanitizer является
taint framework, а не общим defect detector.

Приложение, загружающее `BakerLib` под sanitizer, должно использовать библиотеку
той же `San_*`-конфигурации. Скрытие ELF exports не устраняет переходы через
общий C++ runtime и allocator; совпадающие конфигурации сохраняют единый runtime
contract.

На MSVC `San_Address` и `Debug_San_Address` получают executable stack 8 MiB.
ASan раздувает stack frames и может переполнить Windows-default 1 MiB; production
configurations сохраняют стандартное значение.

Для vendored third-party кода UBSan исключает только `function` и `alignment`:
AngelScript вызывает зарегистрированные C-функции через обобщенные signatures и
укладывает pointer operands в 4-byte-aligned bytecode slots. Остальные undefined
checks активны, а first-party Engine сохраняет и эти две проверки.

LeakSanitizer входит в address leg с `detect_leaks=1` и без suppression list.
Process-lifetime resolver backward-cpp остается достижим из static root, а
AngelScript preprocessor translator, SPARK converters и owning metadata
containers освобождаются при shutdown. Новые утечки исправляются в источнике,
а не скрываются.

## Code coverage

При `FO_CODE_COVERAGE` backend выбирается компилятором:

- MSVC и clang-cl используют MSVC-style output;
- Clang использует LLVM profile/coverage mapping;
- GCC использует GCC/lcov flags.

`Applications.cmake` подключает через `BuildTools/codecoverage.py` targets
`CleanCodeCoverageData`, `RunCodeCoverage`, `GenerateCodeCoverageReport` и
`AnalyzeCodeCoverage`. Результат находится под
`CodeCoverage/<Toolchain>/<Platform-Config>/`.

В denominator входят first-party production sources из `Engine/Source/`;
`Source/Tests/`, `ThirdParty/`, `GeneratedSource/` и `Applications/` исключены.
Локальные примечания находятся в
[Source/Tests/README.ru.md](../../../../Source/Tests/README.ru.md).

Coverage зависит от platform и environment. Sources, не скомпилированные в текущем build, не имеют mapping и отдельно показываются как untouched. Sources, которые компилируются, но не могут выполниться в headless test process, входят в `ENVIRONMENT_EXCLUDED_SOURCES` с письменной причиной; сейчас это device-backed audio/video, Mongo/updater infrastructure и намеренно завершающий процесс diagnostic self-test. Loopback sockets и debugger endpoint остаются в headline. Report раздельно показывает scoped, all-source и excluded buckets. Exclusion является routing decision: его обязан покрыть owning platform, windowed run или integration lane с реальным endpoint.

### Шаблоны focused harness

- **ImGui panels:** создайте backend-less context, задайте `ImGuiBackendFlags_RendererHasTextures` и используйте `ImGui::LogToBuffer(depth)` для auto-open tree nodes и доказательства nested text. Collapsing headers требуют ручной записи IDs в `StateStorage`. Context уничтожается на scope exit. Для widget branch `ImGuiTestHarness::ActivateItem` требует два frame; controls в child windows адресуются через `ActivateChildItem`, а между presses очищается stale active ID.
- **Server diagnostics:** sync point сам не покрывает entities. Snapshot ещё не вошедших players берётся под publication lock; lock освобождается до entity locks, после чего один replacement cover охватывает snapshot и registered world. Fixture должен содержать настоящего not-logged-in player.
- **Inbound remote calls:** entry покрывает calling player и controlled critter. Любая вторая entity требует явного `Game.Sync`; ожидаемый cover violation нельзя проверять под script `try/catch`, потому что session завершится до следующего probe.
- **Crash reporting:** non-terminating crash stream проверяется через private log file с последующим возвратом на `NUL` или `/dev/null`. Terminating reporters запускаются вне процесса через `DiagnosticSelfTest`; `main_basic_strong_assert`, `main_fatal_exit` и `main_failure_exit` различают ранний fatal report и raw status-only exit.
- **Fonts без assets:** синтезируйте `.fofnt` text или BMFont blocks `BMF\3` в памяти, предоставьте sprite и bind scale из `(0..1]`. `SplitLines` выдаёт pages размером с rect, поэтому для нескольких outputs нужен короткий rectangle.
- **Logged-in client/server:** login remote calls объявляются в обоих metadata blobs в противоположных направлениях с правильным subsystem/namespace. До login insertion добавьте хотя бы одно project-owned persistent `Player` property, затем создайте и переключите critter и перенесите его в location/map. Оба `.fomap-bin-*` blob начинаются с `BAKED_MAP_FILE_MAGIC` и `BAKED_MAP_FILE_VERSION`; после header client layout заканчивается после counts hash table и static items.
- **World reload:** используйте file-backed JSON, отметьте ожидаемые entities persistent, остановите один server и запустите второй на том же каталоге. Critter восстанавливается через owning map или global-map membership; off-map runtime critter не reload-ится.
- **Headless 3D:** запеките недегенеративный triangle, создайте description настоящим `ModelInfoBaker`, предоставьте source и baked mesh, `Metadata.fometa-client` и `ModelAnimationInfo.foinfo`, затем создайте instance через null renderer. Fixture metadata создавайте через `BakerTests::MakeMetadataBlob` или `MakeEmptyMetadataBlob`: registration отклоняет blob без обязательной metadata version.
- **Static maps и disk writes Mapper:** сначала запишите baked-map format header, затем настоящий payload `Properties::StoreAllData()` для server map records; zero length недопустим. Server payload продолжается hashes, critters и items, а более короткий client payload содержит hashes и static items. Mapper save tests требуют настоящий Maps root через `InputDirs` с reference `.fomap`; предпочитайте `SaveMapToDir`, потому что plain `SaveMap` иначе может записать в working directory процесса. Удаление статического предмета на карте наблюдаемо от начала до конца только тогда, когда *один и тот же* id статического предмета есть в обоих payload — серверу он нужен в `StaticItemsById`, чтобы удалить, а клиенту нужен построенный из него view, чтобы убрать, — поэтому `Test_ClientServerIntegration` держит такой предмет в обоих map blobs.

## Текущий набор тестов

Полный отсортированный список и authoritative count генерируются из
`Source/Tests/Test_*.cpp` в
[source-inventory.json](../../../generated/source-inventory.json). Не копируйте
полный список или total в prose.

```bash
python BuildTools/docs_inventory.py --write
python BuildTools/docs_inventory.py --check
```

Группы ниже помогают выбрать starting area; generated JSON остается исчерпывающим.

### Конфигурация и источники данных

Начните с `Test_CacheStorage.cpp`, `Test_ConfigFile.cpp`, `Test_DataSource.cpp`,
`Test_FileSystem.cpp`, `Test_Settings.cpp` и `Test_SettingsStorage.cpp`.

### Общая runtime-модель

Сюда относятся headless application (`Test_ApplicationHeadless`), metadata, entities/prototypes, properties, geometry, map loading,
movement/pathfinding, text packs, timers и two-dimensional grids. Основные suites:
`Test_EngineMetadata`, `Test_EntityLifecycle`, `Test_EntityProtos`,
`Test_MapLoader`, `Test_Movement`, `Test_PathFinding`, `Test_Properties`,
`Test_ProtoManager` и соседние common tests.

### Networking и server/client integration

Начните с `Test_ClientDataValidation`, `Test_NetBuffer`, `Test_NetworkClient`,
`Test_NetworkServer`, `Test_NetworkUdp`, `Test_ClientServerIntegration`,
`Test_EntitySync`, server engine/map/item suites, database, fog of war и
location/entity management.

### Scripting и script-visible API

AngelScript compiler/runtime, bytecode, calls, attributes, builtins, entities и
native script methods покрывают `Test_AngelScript*`, `Test_ScriptBuiltins`,
`Test_ScriptEntityOps`, `Test_CommonScriptMethods` и
`Test_ServerScriptMethods`.

### Bakers и инструменты

Сюда входят Baker setup, config/effect/image/map/metadata/model/particle/proto/
text processors, Mapper, texture atlas, model source loader, Ozz и complete
model-animation family.

Model-animation suites разделяют production contract. `Test_ModelMeshData`
проверяет обязательный `LFMODMSH` schema-1 wire format, structural validation,
truncation и exact writer compatibility. `Test_ClientEngine` пересекает реальный
`BakerLib`/`ClientLib` payload boundary. `Test_ModelSourceLoader` проверяет
OBJ/ASCII-FBX, cache single-flight и error fan-out. `Test_ModelAnimationData`,
converter, procedural pose, runtime, baker и timeline suites покрывают archive,
joint remap, manifests, sampling/blending, canonical resolution, links и event
state.

После source-loader, mesh-wire или converter изменений запускайте
`ForceBakeResources` на реальном содержимом проекта, затем обычный
`BakeResources`, который должен остаться incremental-clean на неизмененном tree.

### Rendering/frontend smoke tests

`Test_ImGui` закрепляет backend-less harness активации widgets и состояния windows для coverage diagnostic panels.

`Test_EffekseerParticleRuntime` проводит cooked legacy/modern effects через
реальные Sprite/Ring callbacks и проверяет topology, geometry, UV, Z-sort,
index chunking и scale reapplication. `Test_ParticleBaker` проверяет authored
source discovery/output mapping и запрет runtime `.spk`/`.efk` как входов.
Общие renderer-контракты принадлежат `Test_Rendering.cpp`.

### Сводка владения

| Область | Типичные starting points |
|---|---|
| Essentials | Logging, containers, serialization, filesystem, exceptions, memory, platform, smart pointers, stack traces, strings, time, workers. |
| Configuration/data | Cache, config, data source, filesystem, settings. |
| Common runtime | Metadata, entities/prototypes, geometry, maps, movement, pathfinding. |
| Networking/integration | Buffers, connections, UDP ordering, server/client runtime, updater, database. |
| Scripting | AngelScript extensions, exports, methods, entities и value semantics. |
| Bakers/tools | Baking, metadata/resource packs, Mapper/editors и asset processors. |
| Frontend/rendering | Application init, visible/headless behavior и renderer-facing contracts. |

## Маршрутизация проверки по типу изменения

- Essentials: [Essentials](../../reference/native/essentials.md) и соответствующие tests.
- Config/files/cache/resources:
  [Конфигурация и источники данных](../../reference/settings/configuration-and-data-sources.md),
  parser/filesystem/cache tests и потребители bake/runtime.
- BuildTools/CMake/codegen: [конвейер BuildTools](../../reference/cmake-and-buildtools/pipeline.md),
  [Generated API](../../reference/metadata/index.md) и хотя бы один generated target.
- Bakers/resources: [Baking Pipeline](../../explanation/content-pipeline/baking.md) и owning baker tests.
- Runtime entities/maps/persistence/networking: русские
  [Entity Model](../../explanation/entity-and-property-model/),
  [Maps and Movement](../../explanation/maps-and-movement.md),
  [Persistence](../../explanation/persistence/),
  [Networking](../../explanation/authority-and-networking/) и focused tests.
- Client/frontend/server:
  [Client Runtime](../../explanation/runtime/client.md),
  [Frontend и рендеринг](../../explanation/rendering/),
  [Server Runtime](../../explanation/runtime/server.md) и integration/smoke tests.
- Scripting: [runtime](../../explanation/scripting-runtime/),
  [lifecycle/concurrency](../../how-to/scripting/lifecycle-and-concurrency.md),
  [method ownership](../../reference/script-api/method-ownership.md),
  [nullability](../coding-contracts/nullability.md) и соответствующие suites.

## Добавление и удаление тестов

1. Добавьте детерминированный `Source/Tests/Test_*.cpp`.
2. Включите его в `FO_TESTS_SOURCE` в `EngineSources.cmake`.
3. Выполните `python BuildTools/docs_inventory.py --write`.
4. Меняйте эту страницу, только если изменились ownership group или validation route.
5. Запустите focused binary и, по возможности, `RunUnitTests`.
6. При изменении coverage проверьте соответствующий target.

## Контрольный список

1. `python BuildTools/docs_inventory.py --check` подтверждает точное соответствие
   generated inventory каталогу `Source/Tests`.
2. `python BuildTools/docs_validate.py` проверяет artifacts и ссылки.
3. Target names описаны как производные `FO_DEV_NAME`.
4. Изменения `TestingApp.cpp`, `FO_TESTS_SOURCE`, ownership groups или coverage
   wiring обновляют этот документ в той же change.

## См. также

- [Profiling](../../how-to/quality/profiling.md) — Tracy build modes и captures.
- [Нативная отладка и отладка AngelScript](../../troubleshooting/debugging.md) для native и AngelScript diagnosis.
