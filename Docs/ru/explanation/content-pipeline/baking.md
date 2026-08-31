---
layout: default
title: Конвейер запекания ресурсов
document_id: baking-pipeline
locale: ru
permalink: /Docs/ru/explanation/content-pipeline/baking.html
---

<!-- docs-translation: {"document_id":"baking-pipeline","locale":"ru","source_path":"Docs/en/explanation/content-pipeline/baking.md","source_sha256":"904518250c363682fbe0ee1b8a98076e1fb1b9b7e301f940f43aec655c024015"} -->

# Конвейер запекания ресурсов

Этот документ описывает конвейер запекания ресурсов Engine: где он подключён, какие исходники определяют поведение baker-ов и как проверять изменения. Общая карта инструментов приведена в разделе [Tools](../../../Tools.md).

Документ относится к переиспользуемому поведению Engine. Правила каталогов контента конкретной игры и политика выпуска пакетов принадлежат документации встраивающего проекта.

## Состояние контракта

Конвейер запекания является привязанным к ревизии контрактом со статусом `experimental`. Engine владеет схемой resource pack, именами и порядком встроенных baker-ов, правилами incremental/full rebuild, приватными runtime payload, схемой отчёта и extension hook. Встраивающий проект владеет именами пакетов, раскладкой входных данных, дополнительными baker-ами, распределением по runtime-сторонам, политикой контента, генерируемыми исходниками проекта и приёмкой релиза.

Нормативными являются текущие исходники Engine и его сгенерированные модели контрактов. Конфигурация проекта показывает один из способов собрать конвейер, но не переопределяет переиспользуемый контракт baker-ов.

## Проверенные исходники

- `BuildTools/cmake/stages/ScriptsAndBaking.cmake`
- `BuildTools/cmake/helpers/Build.cmake`
- `BuildTools/cmake/helpers/WriteBuildHash.cmake`
- `Source/Common/Settings.h`
- `Source/Common/Settings.cpp`
- `Source/Applications/BakerApp.cpp`
- `Source/Applications/BakerLib.cpp`
- `Source/Tools/Baker.h`
- `Source/Tools/Baker.cpp`
- `Source/Tools/BakingReport.h`
- `Source/Tools/BakingReport.cpp`
- `Source/Tools/MetadataBaker.h`
- `Source/Tools/MetadataBaker.cpp`
- `Source/Tools/ConfigBaker.h`
- `Source/Tools/ConfigBaker.cpp`
- `Source/Tools/RawCopyBaker.h`
- `Source/Tools/RawCopyBaker.cpp`
- `Source/Tools/ImageBaker.h`
- `Source/Tools/ImageBaker.cpp`
- `Source/Tools/SpriteMeshing.h`
- `Source/Tools/SpriteMeshing.cpp`
- `Source/Common/SpriteResource.h`
- `Source/Common/SpriteResource.cpp`
- `Source/Tools/EffectBaker.h`
- `Source/Tools/EffectBaker.cpp`
- `Source/Tools/ProtoBaker.h`
- `Source/Tools/ProtoBaker.cpp`
- `Source/Tools/MapBaker.h`
- `Source/Tools/MapBaker.cpp`
- `Source/Tools/TextBaker.h`
- `Source/Tools/TextBaker.cpp`
- `Source/Tools/ProtoTextBaker.h`
- `Source/Tools/ProtoTextBaker.cpp`
- `Source/Tools/ModelMeshBaker.h`
- `Source/Tools/ModelMeshBaker.cpp`
- `Source/Common/ModelMeshData.h`
- `Source/Common/ModelMeshData.cpp`
- `Source/Tools/ModelInfoBaker.h`
- `Source/Tools/ModelInfoBaker.cpp`
- `Source/Tools/ParticleBaker.h`
- `Source/Tools/ParticleBaker.cpp`
- `Source/Common/AnimationInfo.h`
- `Source/Common/AnimationInfo.cpp`
- `Source/Common/ModelBounds.cpp`
- `Source/Common/ModelBounds.h`
- `Source/Tools/ModelBoundsCalculator.h`
- `Source/Tools/ModelBoundsCalculator.cpp`
- `Source/Tools/ModelSourceLoader.h`
- `Source/Tools/ModelSourceLoader.cpp`
- `Source/Tools/ModelAnimationConverter.h`
- `Source/Tools/ModelAnimationConverter.cpp`
- `Source/Common/ModelAnimationData.h`
- `Source/Common/ModelAnimationData.cpp`
- `Source/Tools/AngelScriptBaker.h`
- `Source/Tools/AngelScriptBaker.cpp`
- `Source/Tests/Test_BakerSetup.cpp`
- `Source/Tests/Test_MetadataBaker.cpp`
- `Source/Tests/Test_ConfigBaker.cpp`
- `Source/Tests/Test_RawCopyBaker.cpp`
- `Source/Tests/Test_ImageBaker.cpp`
- `Source/Tests/Test_EffectBaker.cpp`
- `Source/Tests/Test_ProtoBaker.cpp`
- `Source/Tests/Test_ProtoTextBaker.cpp`
- `Source/Tests/Test_MapBaker.cpp`
- `Source/Tests/Test_TextBaker.cpp`
- `Source/Tests/Test_ModelBaker.cpp`
- `Source/Tests/Test_ParticleBaker.cpp`
- `Source/Tests/Test_ModelMeshData.cpp`
- `Source/Tests/Test_ModelAnimationData.cpp`
- `Source/Tests/Test_ModelAnimationConverter.cpp`
- `Source/Tests/Test_ModelSkeletonCompatibility.cpp`
- `Source/Tests/Test_ModelSourceLoader.cpp`
- `Source/Tests/Test_OzzAnimation.cpp`
- `Source/Tests/Test_AngelScriptBaker.cpp`

## Что делает запекание

Запекание преобразует ресурсы и конфигурацию проекта в данные, готовые для runtime. Этап `ScriptsAndBaking.cmake` создаёт командные targets `BakeResources` и `ForceBakeResources`; они запускают baker-приложение проекта с его главным конфигурационным файлом.

На уровне приложений и исходников владельцы разделены так:

- `BakerApp.cpp` является оболочкой executable: создаёт `MasterBaker` и вызывает `BakeAll()`;
- `BakerLib.cpp` экспортирует `FO_BakeResources()` для library-based сценариев. В Linux export map оставляет единственным публичным символом именно этот вход, а post-build проверка закрепляет ABI. Символы allocator-а и реализации Engine связываются локально, поэтому release baker, загруженный sanitizer-host-ом, не перехватывает allocation или глобальное runtime-состояние host-а;
- `Baker.h/.cpp` владеют общим контекстом, созданием baker-ов, data source, записью результатов и `MasterBaker`;
- `BakingReport.h/.cpp` владеют DTO отчёта, потокобезопасной агрегацией, JSON-сериализацией и построением пути отчёта.

### Имена output согласуются с именами, к которым обращались baker-ы

`MasterBaker::BakeAllInternal()` согласует дерево output с фактическими именами, которые адресовали baker-ы, двумя шагами вокруг sweep устаревших файлов:

1. `ReconcileStaleCasedOutputDirs()` обходит ожидаемые каталоги от мелких к глубоким и переименовывает различающиеся только регистром, записывая `Rename stale-cased dir <from> to <to>`. Каталоги идут первыми, чтобы последующие переименования файлов уже попадали в родителя с правильным именем.
2. После sweep та же проверка применяется к файлам с сообщением `Rename stale-cased file <from> to <to>`.

Это необходимо на файловых системах без учёта регистра: запись файла или создание каталога повторно использует старую directory entry, outdated sweep сравнивает пути без учёта регистра, а incremental bake может пропустить якобы актуальный artifact. Runtime lookup остаётся точным, поэтому старое написание превращается в неразрешимый ресурс.

Согласование выполняется один раз за bake по уже созданным output, не добавляет работу к каждой записи, не удаляет и не пересоздаёт содержимое и исправляет даже пропущенные как up-to-date artifacts. На case-sensitive файловой системе старое имя удаляет обычный sweep. Контракт закреплён тестами `BakerMasterRenamesStaleCasedOutputAfterCaseOnlyInputRename`, `BakerMasterRenamesStaleCasedOutputDirAfterCaseOnlyInputDirRename` и `DiskFileSystemNameCase`.

## Точки входа CMake

`BuildTools/cmake/stages/ScriptsAndBaking.cmake` определяет `AddBakingTarget` как проверяемый helper project interface и использует его для создания стандартных команд запекания после появления application targets.

- `BakeResources` создаётся вызовом `AddBakingTarget(BakeResources)` и запускает baker с `-ForceBaking False`.
- `ForceBakeResources` создаётся вызовом `AddBakingTarget(ForceBakeResources FORCE)` и запускает его с `-ForceBaking True`.
- Обе стандартные цели передают главный конфигурационный файл проекта через `-ApplyConfig <FO_MAIN_CONFIG>` и используют subconfig `NONE` по умолчанию.
- Каждая цель, созданная через `AddBakingTarget`, работает из `FO_OUTPUT_PATH`, зависит от `ForceCodeGeneration` и записывает `Baking/Resources.build-hash` через `BuildTools/cmake/helpers/WriteBuildHash.cmake`.
- `CompileAngelScript` также зависит от `ForceCodeGeneration`, поэтому метаданные и генерируемый код не могут отстать от компиляции скриптов или запуска baker-а.

После выполнения `SetupScriptsAndBaking()` встраивающий проект может добавить цель для собственного subconfig, не копируя команду запуска baker-а:

```cmake
AddBakingTarget(Game_PublicResources
    SUB_CONFIG PublicGame
    COMMENT "Bake public resources")
```

Полная сигнатура: `AddBakingTarget(<target> [SUB_CONFIG <name>] [FORCE] [COMMENT <text>])`. По умолчанию `SUB_CONFIG` равен `NONE`, `COMMENT` равен `Bake resources`, а `FORCE` переключает `-ForceBaking` с `False` на `True`. Неизвестные аргументы и ключи без значений останавливают конфигурацию. Имя дополнительной цели и соответствующий subconfig принадлежат встраивающему проекту.

Финальные имена application targets и их зависимости задаются проектом и preset-ом. Не следует выдавать имена одного проекта за универсальный интерфейс Engine.

## Runtime-классы

### `BakingContext`

`BakingContext` из `Baker.h` переносит общие данные одного запуска:

- `Settings` — текущие `BakingSettings`;
- `PackName` — имя обрабатываемого resource pack;
- `BakeChecker` — callback проверки актуальности существующего результата;
- `WriteData` — callback асинхронной записи;
- `BakedFiles` — data source уже запечённых файлов;
- `ForceSyncMode` — необязательное принудительное синхронное выполнение.

В проходе `MasterBaker` контекст также содержит общий collector отчёта и стабильное имя baker-а для attribution. `WriteData` возвращает `BakingWriteResult`, различающий реально изменившуюся запись и byte-identical файл, которому только обновили timestamp.

### `BaseBaker`

Каждая реализация baker-а предоставляет `GetName()` со стабильным именем конфигурации, `GetOrder()` с ключом детерминированного порядка и `BakeFiles()` с преобразованием файлов.

Жёсткие cross-pack зависимости разведены по стадиям: `ParticleBaker` выполняется перед `ModelInfoBaker`, затем идут `ProtoBaker` и `MapBaker`. Описание модели может ссылаться на запечённую частицу, прототип — на модель, карта — на прототип. Эти проверки не должны гоняться с публикацией собственных зависимостей во время чистого или принудительного rebuild.

`BaseBaker::SetupBakers()` создаёт запрошенные встроенные baker-ы и затем вызывает `SetupBakersHook()`, позволяя внешнему коду добавить проектные реализации. Каждый baker получает собственную копию общего контекста. При активном отчёте callbacks check/write оборачиваются именем baker-а, поэтому attribution остаётся верным и для его асинхронных задач. Защищённые helpers позволяют встроенным и проектным baker-ам добавлять counters и histograms; без collector-а они являются no-op.

### `MasterBaker`

`MasterBaker::BakeAll()` координирует настроенный проход и используется `BakerApp.cpp` и `BakerLib.cpp`. Один экземпляр владеет одним collector-ом отчёта и завершает его как при успехе, так и при исключении.

### `BakingReport`

`BakingReport.h` задаёт общие DTO и контракт результата записи. Реализация в `.cpp` отвечает за агрегацию, анализ sprite mesh, создание JSON и пути результатов. `Baker.cpp` управляет только lifecycle и пересылает события.

### `BakerDataSource`

`BakerDataSource` адаптирует входы и выходы к интерфейсу `DataSource`, хранит resource packs, output resources, cache checks и построение output paths. `Reindex()` пересоздаёт mounts, baker-ы, collections и output index и сообщает, изменились ли пути или source write times. Это позволяет долгоживущим tools обнаруживать и лениво запекать новые или изменённые ресурсы без постоянного полного сканирования диска. Dry-run обнаружения output и последующее per-file запекание не подключают collector `MasterBaker` и поэтому намеренно отсутствуют в его отчёте.

## Отчёт полного прохода

Каждая попытка `MasterBaker::BakeAll()` с непустым `BakeOutput` пересоздаёт `Baking.report.json`. Успешный полный rebuild дополнительно обновляет `Baking.full.report.json`:

```text
BakeOutput = Baking
runtime resource directory = Baking/
report = Baking/Baking.report.json
last complete-corpus report = Baking/Baking.full.report.json
```

Предыдущий обычный отчёт удаляется до запуска. После завершения `MasterBaker` ставит `success` или `failed`, сериализует всё накопленное и пишет файл даже при ошибке подготовки ресурсов или baker-а. `failureMessage` содержит текст исключения, а зарегистрированные, но не достигнутые baker entries остаются `not_run`. Ошибка записи отчёта делает неуспешным сам `BakeAll()`, поэтому успешный проход всегда имеет соответствующий отчёт.

Incremental и failed проходы не перезаписывают `Baking.full.report.json`; последний полный corpus snapshot остаётся доступен во время обычной разработки.

Cleanup устаревших runtime resources пропускает любой `*.report.json` непосредственно в корне `BakeOutput` (`REPORT_FILE_SUFFIX` в `Baker.h`). Суффикс охватывает два стандартных отчёта и проектные диагностические artifacts рядом с ними. Runtime resources всегда лежат ниже каталога pack, поэтому root-only правило не может сохранить действительно устаревший запечённый ресурс. Отчёт записывается прямо в корень `BakeOutput` после cleanup, не монтируется в baked `FileSystem`, не регистрируется как output и не доставляется runtime.

Поля верхнего уровня:

- `schemaVersion`, сейчас `1`;
- `status` и `failureMessage`;
- `buildHash`, `bakeOutput`, `durationMs`;
- `mode.forceRequested`, `mode.fullRebuild`, `mode.rebuildReason`, `mode.singleThread`;
- причины rebuild: `incremental`, `requested`, `build_hash_changed`, `missing_build_hash`.

`measurementScope` не позволяет принять incremental-выборку за статистику всего corpus. Input counts описывают полный настроенный набор, тогда как output activity и baker details относятся к текущему проходу. В частности, `Image.details.spriteMesh` измеряет только перестроенные кадры. `completeCorpusDetails` истинен лишь при `mode.fullRebuild`; для распределений по всей графике нужен force bake.

`totals` суммирует packs, типы baker-ов, invocations, входные файлы/байты, output checks, scheduled/up-to-date artifacts, submitted/changed/unchanged files и удалённые устаревшие output. Те же данные представлены в `bakers`, агрегированном по имени, и `packs`, где статистика и nested baker entries разделены по resource pack.

Каждый baker entry содержит order, состояние `success`/`failed`/`not_run`, invocation count, elapsed time, failure messages и полный доступный `availableInputFiles`/`availableInputBytes`. Output-поля различают:

- `checked` — уникальные artifact paths, переданные incremental checker;
- `scheduled` — пути, выбранные для rebuild;
- `upToDate` — пути, удовлетворённые текущим output;
- `cacheHitPercent` — доля up-to-date check calls;
- `submitted` — уникальные пути, созданные baker-ом;
- `changed` — записанный изменившийся контент;
- `unchanged` — byte-identical результат с обновлённым timestamp.

Raw-поля `checkCalls`, `scheduledCheckCalls`, `upToDateCheckCalls`, `submitCalls`, `submittedBytesAcrossCalls` сохраняются, потому что один путь может проверяться или подаваться несколько раз. Группы files/bytes включают распределение по extensions и 25 крупнейших путей. `details.counters` и `details.histograms` хранят domain-specific измерения, не меняя общей схемы.

### Статистика Image и sprite mesh

`ImageBaker` добавляет counters collections, directions, frame slots, unique frames, shared references и histogram форматов. Геометрия находится в `details.spriteMesh` как в агрегированном `Image`, так и в per-pack entry.

`settings` фиксирует effective `Enabled`, `AlphaThreshold`, `MaxTriangles`, `AreaSavingsWeight`, внутренние base dilation и maximum padding. Per-frame diagnostics отдельно хранят фактические dilation и simplification tolerance выбранного candidate.

`frames` разделяет уникальные сериализованные кадры и shared animation references. Проценты `mesh`, `quad`, `empty` используют только unique non-shared frames. Histogram triangles сообщает доли относительно meshes и unique frames, vertex histogram — относительно mesh frames. Отдельно считаются source-alpha и dilated connected components, selection tolerance и actual dilation.

`selectionOrigins` различает `greedy_whole`, `greedy_components`, `clustered_components`, `enclosing_triangle`, `enclosing_quad`, `detailed_constrained`, `detailed_simplified`, `detailed_expanded`. `quadReasons` различает `disabled`, `zero_dimensions`, `dilation_fills_frame`, `contour_extraction_failed`, `no_valid_candidate`, `score_preferred_quad`.

Для `score_preferred_quad` объект `bestRejectedCandidates` описывает лучший валидный candidate, проигравший quad: origin, triangles, score, tolerance и dilation; связанные vertices/area остаются в top-list row. `geometry` и `area` показывают цену дополнительных triangles и экономию прозрачных pixels, включая `breakEvenAreaSavingsWeight`.

`geometry` сравнивает фактические vertices/triangles с all-quad baseline, считая empty как нулевую геометрию. `area` хранит точные doubled areas, pixel/percentage views, original quad area, submitted geometry и visible pixels. `cropping` считает уменьшенный canvas и сэкономленные RGBA bytes, `padding` — расширение, added pixels/bytes, maximum и histogram. Эти величины накапливаются раздельно.

Отчёт содержит score ranges, fixed quad score, классификацию ресурса (`mesh_only`, `quad_only`, `empty_only`, `mixed`) и пять детерминированных top-25 списков:

- `largestMissedSavings` для retained quads с наибольшей невидимой частью исходного frame;
- `largestRejectedCandidateSavings` для проигравших candidates;
- `mostComplexMeshes` по triangles и vertices;
- `largestCroppingSavings` по удалённым serialized pixels;
- `largestPaddingOverhead` по добавленному canvas.

Строки списков содержат source/output path, direction, frame index, form, origin/reason, geometry, components, padding, tolerance/dilation, source/baked/visible areas, transparent potential, overhead, savings и score. Tie-breaking включает оба пути, direction и frame index, поэтому отчёт детерминирован при параллельном bake.

Во время output discovery resource packs обходятся в порядке конфигурации. Более поздний baker может читать объявленные output предыдущей зависимости. При совпадении logical output path поздняя регистрация заменяет раннюю; lookup также идёт от последнего pack к первому, реализуя overlay rule.

## Проектирование resource pack в игре

`Settings.h/.cpp` определяют поля `[ResourcePack]`: `Name`, `InputDirs`, `InputFiles`, `IncludePatterns`, `ExcludePatterns`, `ServerOnly`, `ClientOnly`, `MapperOnly`, `Bakers`. Имя обязательно; одновременно может быть истинным не более одного side-only флага; относительные inputs разрешаются от каталога declaring config; порядок секций сохраняется.

Зрелые проекты и независимый TLA сходятся на следующих практиках. Это рекомендации, а не дополнительный синтаксис Engine:

- один pack имеет одну ясную ответственность и стабильное имя: metadata, scripts, texts, prototypes, maps, visual resources или sounds;
- `Bakers` содержит только преобразования, способные обработать этот pack;
- producer packs идут раньше consumers, а жёсткие зависимости получают разные orders; baker-ы одного order могут выполняться параллельно;
- `IncludePatterns`/`ExcludePatterns` отделяют templates, editor-only и private sources от runtime output;
- side-only флаг применяется лишь к действительно server/client/mapper ресурсу; pack без него доставляется server и client;
- обычный authored source задаётся через `InputDirs`, archives/data packs — через `InputFiles`; baked output не редактируется как исходник;
- после смены ревизии Engine, контракта baker-а, глобальных metadata или dependency graph выполняется force bake, если timestamp-инкрементальность не может доказать эквивалентность.

Точный синтаксис и precedence описаны в [Configure a Game Project](../../how-to/build/project-configuration.md), а порядок generated sources и восстановление — в [Generated Content Workflow](../../how-to/build/generated-content.md).

## Встроенные baker-ы

`BaseBaker::SetupBakers()` регистрирует следующие реализации:

| Имя в конфигурации | Класс | Порядок | Доступность |
| --- | --- | ---: | --- |
| `Metadata` | `MetadataBaker` | 1 | всегда |
| `Config` | `ConfigBaker` | 2 | всегда |
| `RawCopy` | `RawCopyBaker` | 4 | всегда |
| `Image` | `ImageBaker` | 4 | всегда |
| `Effect` | `EffectBaker` | 4 | всегда |
| `Text` | `TextBaker` | 4 | всегда |
| `ModelMesh` | `ModelMeshBaker` | 4 | `FO_ENABLE_3D` |
| `AngelScript` | `AngelScriptBaker` | 4 | `FO_ANGELSCRIPT_SCRIPTING` |
| `Particle` | `ParticleBaker` | 5 | всегда; backend зависит от build options |
| `ProtoText` | `ProtoTextBaker` | 6 | всегда |
| `ModelInfo` | `ModelInfoBaker` | 6 | `FO_ENABLE_3D` |
| `Proto` | `ProtoBaker` | 7 | всегда |
| `Map` | `MapBaker` | 8 | всегда |

Цепочка particle/model/prototype/map намеренно использует orders `5`, `6`, `7`, `8`. Baker-ы одного order могут идти параллельно и не должны читать output друг друга. Поведение конкретного asset type выводится из класса baker-а и его тестов, а не из одного расширения файла.

### Общие metadata анимации

`AnimationInfo` объединяет `SpriteInfo` для 2D frame count/duration/directions/bounds и, при `FO_ENABLE_3D`, `ModelAnimationInfo` для model/animation AABB и typed durations. `ReadSpriteResource` заполняет sprite payload из baked header/table. Для запросов без загрузки pixels `ImageBaker` пишет компактный index версии 1 `SpriteInfo/<PackName>.foinfo` для каждого pack.

Index является aggregate output. Обычный scan merge-ит изменённые entries с полным предыдущим index того же pack; явный запрос index пересобирает все entries. Предыдущий pack-local output монтируется отдельно от cross-pack registry, чтобы aggregate был доступен до первого invocation. Появление или исчезновение index требует full rebuild. `ReadAnimationInfo` загружает 2D indexes во всех builds, добавляет `ModelAnimationInfo.foinfo` в 3D и объединяет их по resource name в `EngineMetadata`.

`ModelInfoBaker` пишет model descriptions и companion `ModelAnimationInfo.foinfo` версии 2. Контракт включает:

- aggregate `ModelBoundsMin*`/`ModelBoundsMax*` по всем animation envelopes либо static geometry при отсутствии mappings;
- deterministic `ViewBoundsMin*`/`ViewBoundsMax*`: сначала `Unarmed + Idle`, затем любой Idle, первая валидная animation или static fallback;
- `BoundsStateAnimations`/`BoundsActionAnimations` и parallel min/max arrays для отдельных animation AABB.

Bounds строятся по каждому animation key, midpoint между соседними keys и сетке
60 Hz. Общий timeline не пропускает экстремум быстрой дуги и остаётся
детерминированным независимо от camera, projection factor, model-sprite
resolution и renderer backend.

`Baking.PreciseModelBounds` выбирает измерение geometry в каждом sample:

- `true` использует `ModelBoundsMeasurement::PerVertex` и преобразует каждую
  skinned vertex. `PublicGame` включает этот exact mode для shipped resources;
- `false` (default) использует `PerBoneEnvelope` и преобразует один заранее
  подготовленный envelope box на bone slot. Blended vertices являются convex
  combinations transformed bone positions, поэтому union может только
  увеличить envelope, но не обрезать exact geometry. Это conservative fast path
  для локальных рабочих bakes.

`ModelBoundsSampler` один раз подготавливает model hierarchy, mesh selection и
per-bone envelopes, после чего обрабатывает все clips из этого immutable plan.
Bounds attachments переиспользуют один sampled bone track на clip и bone вместо
повторного обхода parent hierarchy для каждого attachment. Sections model
descriptions строятся независимо и объединяются в sorted source order.

Каждый binary description link также содержит явный geometry discriminator:
non-particle child links несут обязательный aggregate root-space AABB и затем
`(state, action, AABB)` bounds каждого mapped parent clip; default link и
particle links не имеют geometry payload. Несколько state/action pairs,
использующих один sampled clip, ссылаются на его общий box. Invalid flags,
missing child bounds, неизвестные animation pairs и degenerate AABB отклоняются.
Missing/invalid aggregate или animation bounds являются bake error. Loader
строго проверяет version, required bounds и parallel arrays. Duration и bounds groups независимы: aliases могут
создать duration-only keys, а raw `.fo3d` — bounds-only keys. Static section
может не иметь обеих групп, но companion без model sections malformed.

Runtime объединяет active-animation bounds с bounds выбранных links. Для direct
child клиент выбирает link boxes, соответствующие active clips parent rig, и
использует aggregate envelope, когда matching clip не активен. Nested links
сохраняют собственный aggregate envelope, поскольку их clip indexes принадлежат
другому rig. Клиент проецирует только эти corners вместо обхода и skinning
combined-mesh vertices. Live particle bounds всё ещё могут расширить scratch
frame и вызвать rerender.

`ModelBounds` владеет finite/ordered AABB, point/bounds accumulation, transformed eight corners и guard `max(0.01, maxAbs * 0.001)`. `ModelBoundsCalculator` только читает baked data и sampling geometry. При отключении всех base meshes baker повторяет расчёт на unfiltered model; реально пустая/invalid geometry остаётся ошибкой.

`DrawSize` и `ViewSize` больше не входят в `.fo3d`; `ModelInfoBaker` отклоняет их. Incremental timestamp companion охватывает все inputs pack, включая animation FBX. Отчёт фиксирует model sections, bounds, durations, selected idle/fallback, cache/calculator counts и histogram maximum-axis extent. Individual descriptions проверяются до aggregate companion.

Companion хранит effective `(state, action)` durations после `AnimSpeed` и one-step aliases. Любой pack может выбрать `ModelInfo`; ветвления по `PackName` нет. `EngineMetadata` регистрирует records и hashes при startup. Скрипт `Game.GetModelAnimDuration` возвращает `timespan` либо zero при отсутствии resource/model/tuple. Внутренний config format не должен разбираться кодом проекта.

## Архитектура запекания 3D-моделей

Model pipeline разделён на извлечение source, compatibility analysis, conversion, native wire contracts и два финальных baker-а:

| Модуль | Слой | Ответственность |
| --- | --- | --- |
| `ModelSourceLoader` | Tools | Backend-neutral skeleton/clip/TRS, import через `ufbx`, полная validation и per-bake single-flight `ModelSourceAssetCache`. |
| `ModelAnimationConverter` | Tools | Детерминированная canonical hierarchy, compatibility diagnostics, contributed joints/root aliases/parents и conversion через pinned Ozz offline. |
| `ModelAnimationData` | Common | Versioned little-endian LF envelopes и rig manifest: identity/signatures, skeleton, remaps, clips, presence/nearest data и state/action bindings. |
| `ModelMeshData` | Common | Passive mesh DTO, versioned `LFMODMSH` codec и structural validation без runtime animation. |
| `ModelMeshBaker` | Tools | Проверка и запись mesh-only hierarchy, bind, vertices, indices, influences и drawables. |
| `ModelInfoBaker` | Tools | Разрешение `.fo3d`, dependencies, source/compatibility/conversion и запись `LFMODINF`, обязательного rig payload и `ModelAnimationInfo.foinfo`. |

Основной поток: `FBX/.obj + .fo3d` -> `ModelSourceLoader` -> `ModelAnimationConverter` -> `ModelAnimationData` -> `ModelInfoBaker` -> `ModelInformation` / `ModelAnimation`. Mesh-поток идёт как `FBX/.obj` -> `ModelMeshBaker` -> `ModelMeshData` -> `ModelManager` / `ModelHierarchy`. Они встречаются в `ModelInformation`; clips и mutable poses не сериализуются в shared mesh hierarchy.

Форматы отдельных областей вынесены в самостоятельные владельцы:

- [Prototype Format](../../how-to/content/prototype-format.md) и его [reference](../../reference/prototype-format/index.md) — sections, identity, inheritance, properties, references, migrations;
- [Map Format](../../how-to/content/map-format.md) и его [reference](../../reference/map-format/index.md) — `.fomap`, placement ownership и coupled server/client blobs;
- [Model Format](../../how-to/content/model-format.md) — `.fo3d`, includes, layers, attachments, materials, cuts и composition;
- [Model Animation](../../how-to/content/model-animation.md) — tuples, `AnimSpeed`, aliases, durations и runtime lookup;
- [Text and Localization](../../how-to/content/text-and-localization.md) и [text reference](../../reference/text-format/index.md) — `.fotxt`, normalization и `$Text`;
- [Image and Sprite Formats](../../how-to/content/image-format.md) и [image reference](../../reference/image-format/index.md) — import, FOFRM, baked sprites, atlases и caches;
- [Sprite Root Motion](../../how-to/content/sprite-root-motion.md) - `NextX`/`NextY` и movement phase;
- [Particle Format](../../how-to/content/particle-format.md) и [particle reference](../../reference/particle-format/index.md) — SPARK/Effekseer;
- [Font Formats](../../how-to/content/font-format.md) и [font reference](../../reference/font-format/index.md) — descriptors и layout;
- [Audio](../../how-to/content/audio.md), [audio reference](../../reference/audio/index.md), [Video](../../how-to/content/video.md), [video reference](../../reference/video/index.md) — raw-copy delivery и runtime decoding;
- [Effect Format](../../how-to/content/effect-format.md) и [effect reference](../../reference/effect-format/index.md) — `.fofx`, SPIR-V и backend resources.

Audio `.wav`/`.acm`/`.ogg`, video `.ogv` и bitmap-font descriptors не имеют отдельных baker-ов: `RawCopyBaker` сохраняет bytes/path, а изображения fonts обрабатывает `ImageBaker`. `EffectBaker` компилирует pass через glslang в native `-spv`, cross-compiled forms и opt-in SDL_GPU `-spv_sdl`/MSL. `[EffectInfo]` обслуживает GL/D3D/Vulkan, `[EffectInfoSdl]` хранит per-stage slots/counts; превышение 4 UBO или 16 samplers на stage, storage resources, duplicate/missing bindings и unused declarations являются hard errors.

`SpriteMesh.*` управляет polygonal sprite generation: `Enabled`, `AlphaThreshold`, `MaxTriangles`, `AreaSavingsWeight`. Все четыре значения объявляются проектом даже при disabled; threshold находится в `0..254`, triangle budget положителен, weight finite и non-negative. `SpriteMeshing` владеет mask/contours/candidates/triangulation/validation/scoring, а `ImageBaker` — decode, animation/shared frames, padding/cropping, serialization и report.

Внутренняя геометрическая политика использует one-pixel guard и до 20 pixels temporary padding. После выбора vertices определяют минимальный cropped canvas, offsets корректируются, а `AtlasSprite` восстанавливает logical size/anchor и UV. Quad/empty не crop/pad. Profitability всегда считается относительно original unpadded frame; один maximal-canvas search не может искусственно создать экономию.

Enclosing candidates строятся для каждого reachable triangle count до `MaxTriangles`: greedy reduction, bounded deterministic beam, exhaustive minimum-area triangle/parallelogram. Для нескольких contours рассматриваются individual и clustered partitions с bounded dynamic program. Candidate обязан покрывать весь hull/mask и не пересекать соседний local primitive. Score равен `savedFrameAreaRatio * AreaSavingsWeight - triangleCount`, quad score равен `-2`; при равенстве выигрывает меньшая area.

Detailed path трассирует pixel-cell boundaries, сохраняет islands/holes и перебирает deterministic simplification tolerances. Clipper2 нормализует/offset-ит paths, union с exact source запрещает потерю visible pixels, intersection с raster-dilated region ограничивает bridges. Supporting lines и constrained cleanup не режут stair-step corners; Earcut только triangulates outer rings/holes. Constrained, simplified и expanded families конкурируют независимо и проходят весь bounded ladder.

Coverage проверяется по полной unit-square area каждого visible pixel, а не несколькими samples. Constrained overdraw ограничен guard, cleanup и integer rounding; expanded/enclosing candidates оплачивают actual area через score. Failure reasons разделяют contour, offset, vertex limit, triangulation, area, coverage и tolerance. Empty mask даёт empty geometry, unsafe/unprofitable/oversized — quad. Delaunay refinement не применяется из-за отсутствия выгоды по count/area и bit-identical гарантии.

Baked sprite использует engine-owned magic/version, per-frame draw offset, cropped RGBA и mesh kind. `SpriteResource` является единственным строгим codec для client, particle editor и server-side image loader. Decoder проверяет exact slice consumption, footer, counts, indices, bounds и shared references. Mesh хранит fixed-width coordinates/indices, original logical size и cropped origin, который может быть negative. Runtime legacy fallback отсутствует. Plain rectangular consumers восстанавливают original canvas; font sheets загружаются через `SpriteManager::LoadSpriteAsQuad`. После изменения format или `SpriteMesh.*` без нового build hash нужен `ForceBakeResources`.

`MapBaker` пишет отдельные server/client blobs. Client blob содержит видимые static items, а hash dictionary также собирает client properties скрытых static items, чтобы `Common` hstring разрешались без раскрытия entities.

Оба blob начинаются с `BAKED_MAP_FILE_MAGIC` и
`BAKED_MAP_FILE_VERSION` из `Source/Common/MapLoader.h`.
`MapLoader::ReadBakedFileHeader` проверяет header до чтения payload в
`MapManager::LoadStaticMaps` или `MapView::LoadStaticData`. Hash table использует
парный контракт `DataWriter::WriteString` / `DataReader::ReadString`, а каждый
последующий count или byte size проверяется через
`DataReader::VerifyPayloadCount` до allocation или loop. Поэтому отсутствующие,
устаревшие, обрезанные или повреждённые данные вызывают
`DataReadingException`, а не интерпретируются как counts элементов. При
изменении layout увеличьте `BAKED_MAP_FILE_VERSION` и в том же изменении
запустите `ForceBakeResources`: timestamps исходников не доказывают, что
существующий output использует текущий layout.

`ParticleBaker` обрабатывает `.spark` -> `.spk` при `FO_SPARK_PARTICLES` и `.efkproj` -> `.efk` при `FO_EFFEKSEER_PARTICLES`; обе options по умолчанию `OFF`. SPARK XML загружается с `SparkQuadRenderer`, texture paths разрешаются относительно source и не могут выходить из него. Unknown types, malformed XML, authored `.spk` и binary-save failure являются hard errors. Client и baker используют один `SparkExtension.cpp` через `ClientLib`; binary loader ограничивает payload, object/attribute counts, reads, signatures и typed references.

Effekseer source должен быть on-disk XML проекта Editor 1.80.5, version 3. Native `EffekseerCompiler` выдаёт `SKFE` bytes и dependencies, которые проверяются pinned C++ Core. Compiler входит в `BakerLib`, но не в production client; Mapper использует тот же on-demand path, Web получает заранее baked `.efk`.

Dependency snapshot хранится в `<BakeOutput>/.baker-cache/Effekseer/<pack>/<output>.deps` и включает path/size/write time проекта и каждого dependency. Он привязан к выбранному physical source, корректно работает с overlays и dirties только ссылающиеся effects. Missing/stale snapshot повторно анализирует project; stale physical output удаляется только для соответствующего effect. `.efkmodel` является runtime dependency и доставляется обычным raw-copy policy. Authored `.efk` запрещён, изменение compiler требует force bake.

`LFMODMSH` schema `1` с flags `0` содержит mesh-only hierarchy/bind/drawables, но не clips/TRS. Один codec используется baker/client, legacy headerless files, unknown schema/flags, truncation и trailing data отклоняются. Counts preflight-ятся до allocation; indices, palettes, weights, hierarchy depth (1024 joints total, 128 parent chain) и finite values строго проверяются.

`ModelMeshBaker` отклоняет mesh node с отрицательным determinant `geometry_to_world`. Такой node экспортирован с отрицательным scale: отражение меняет ориентацию поверхности, поэтому normals и winding треугольников расходятся с lighting и back-face culling. Baker не скрывает дефект source переворотом normals/winding; mirrored object нужно заморозить обратно к положительному scale в authoring tool.

`ModelInfoBaker` также проверяет масштаб модели по диапазону
`Baking.ModelAttachmentMinExtent` .. `Baking.ModelAttachmentMaxExtent`. Для
прямой ссылки `Attach` на bare `.fbx` проверяется maximum-axis extent статических
bounds. Для каждой секции `.fo3d` проверяется aggregate `ModelBounds`: union
animation envelopes либо static geometry, если mappings отсутствуют. Full bake
проверяет envelope при записи `ModelAnimationInfo.foinfo`, а targeted `.fo3d`
bake выполняет ту же проверку перед записью description. Token `.fo3d` `Scale`
не меняет baked envelope, поэтому root model в сантиметрах не освобождается от
проверки. Ошибка называет файл, измеренный extent и limit.

Baker не использует `AppRender::MAX_ATLAS_WIDTH` / `HEIGHT` build host как
контракт устройства. Portable layout math использует
`AppRender::MIN_ATLAS_SIZE / FRAME_SCALE`. Runtime preview zoom всё ещё может
увеличить допустимый envelope сверх `Render.ModelSpriteMaxTextureWidth` /
`Height`; `RefreshFrameLayout` ограничивает scratch texture и рисует с crop
вместо завершения процесса.

Schema 1 остаётся native-endian для little-endian targets; big-endian потребует новой схемы.

Перед записью `ModelMeshBaker` проверяет matrices, attributes, colors, weights/offsets и limits; top-four positive influences нормализуются и повторно проверяются. Ошибка всегда называет source/node/field/element/component, invalid values не clamp-ятся. После `ufbx_generate_indices` pinned meshoptimizer v1.2 (`9d9890c73011d75920af614485296d1e03e95448`) выполняет vertex-cache и vertex-fetch reorder во временных buffers. Input/output ranges проверяются до commit; allocator использует `SafeAllocator`. Это lossless reorder без смены schema. Overdraw, quantization, codecs, LOD, meshlets и packed layout требуют отдельной измеренной schema 2.

Animation extraction принадлежит `ModelSourceLoader`. Он извлекает skeleton/clips через `ufbx`, требует case-insensitive unique clip names, positive finite duration, valid hierarchy/output pairing, finite strictly ordered S/R/T keys, limits и normalized quaternions. `ModelSourceAssetCache` single-flight разделяет один parse или одно exception между parallel descriptions без удержания mutex во время FBX parsing.

`ModelAnimationConverter` создаёт deterministic parent-first canonical joints. Animation-only joints допустимы; duplicate names/hierarchies и incompatible parents — bake errors. Technical scene root может иметь пустое имя, остальные joints — нет. Разные root names нормализуются к base root. Rest-pose divergence является report-only: canonical rest берётся из base, animation-only rest identity, clip rest не заменяет fallback.

Key times могут лежать вне `[0,duration]`. Converter вычисляет endpoints `0`/`duration`, сохраняет interior keys и original duration, не растягивает и не clamp-ит take. Smooth quaternions subdivide-ятся с budget `0.05` degree для runtime parity `0.1`. Disabled interpolation требует shared cropped timeline и exact endpoint keys. Smooth track с первым key внутри `(0,duration]` отклоняется, если discontinuity нельзя выразить continuous interpolation.

Direct FBX attachment обязан быть rest-only; наличие clips требует `Attach ...fo3d`. External animation FBX обычно не содержит drawable geometry. Точное временное исключение `AllowAnimationGeometry <file>` допускается только для реально выбранного source с geometry; duplicates, stale/non-selected exceptions запрещены. Инвентарь ремонта конкретных assets принадлежит проекту.

Incremental invalidation `.fo3d` учитывает write time description/include graph и всех resolved `Model`, external `Anim`, direct `Attach`, `Cut`. Missing dependency — hard error. Animation-only FBX обязан rebuild-ить rig и `ModelAnimationInfo`, даже если mesh-only `LFMODMSH` не меняется.

Native animation использует `ozz-animation` 0.16.0, commit `6cbdc790123aa4731d82e255df187b3a8a808256`. LF envelope хранит little-endian magic/schema/kind/flags, rig/source/cache signatures, pinned revision, UTF-8 identity, length и FNV-1a hash. Flags schema 1 равны zero. Signatures являются deterministic invalidation inputs, не security identity. Перед codec создаётся private Ozz allocator поверх `SafeAllocator`; public API не раскрывает Ozz allocator.

Reader получает expected identity и до Ozz codec отклоняет mismatch, unknown kind, flags, truncation, trailing bytes и hash mismatch. FNV-1a обнаруживает accidental corruption, но не аутентифицирует pack. Attacker-writable packs должны проверяться вне model loader.

`ModelInfoBaker` строит canonical runtime skeleton, unique animations, remaps, presence masks и nearest timelines. Payload проходит write/read-back validation. Signed TRS допускает mirror через X scale и требует `T*R*S` round-trip с relative tolerance `1e-4`; shear, zero scale, invalid quaternion/ranges/durations/timepoints, более 1024 joints или 65535 timepoints и animated aliased roots запрещены. Missing base tracks заполняются canonical rest, animation-only — identity, presence byte остаётся zero.

Каждый `.fo3d` output начинается с `LFMODINF`, schema `1`, flags `0`, затем positional description и обязательный length-prefixed `LFOZZRIG` schema 1. Rig содержит signatures, skeleton, base remap, unique animation/remap pairs и sorted `(StateAnim, ActionAnim) -> (clip index, reversed)`. Manifest повторяет source identity/signature и сверяет их с nested `LFOZZARC`. Counts/order/duplicates/bindings/remaps/consumption/tags/topology/rest/tracks/durations проверяются до публикации immutable `ModelInformation`. Unversioned descriptions не поддерживаются; compatibility marker mesh transition — `0.0.30`, нужен полный rebake.

Generated Ozz rig является единственным production clip/pose payload. `ModelAnimationController` хранит LF timeline/events и Ozz clip metadata; sampling, blending, movement replacement, procedural rotations и local-to-model выполняются per-instance. Cross-model sharing или persisted cache требуют отдельного измеренного обоснования.

## Связь с компиляцией скриптов

Тот же `ScriptsAndBaking.cmake` создаёт соседние команды:

- `CompileAngelScript` запускает AS compiler проекта при `FO_ANGELSCRIPT_SCRIPTING`;
- `CompileMonoScripts` запускает `BuildTools/compile-mono-scripts.py` с обязательным scripts/project directory `FO_OUTPUT_PATH` при `FO_MONO_SCRIPTING`.

Это отдельные targets от resource baking, но они находятся в одном preparation stage, потому что generated/baked runtime inputs образуют общий build workflow.

## Какие тесты проверять

Focused coverage находится в `Source/Tests/`:

- `Test_BakerSetup.cpp`
- `Test_ConfigBaker.cpp`
- `Test_MetadataBaker.cpp`
- `Test_RawCopyBaker.cpp`
- `Test_ImageBaker.cpp`
- `Test_EffectBaker.cpp`
- `Test_ProtoBaker.cpp`
- `Test_ProtoTextBaker.cpp`
- `Test_MapBaker.cpp`
- `Test_TextBaker.cpp`
- `Test_ModelBaker.cpp`
- `Test_ParticleBaker.cpp`
- `Test_ModelMeshData.cpp`
- `Test_ModelAnimationData.cpp`
- `Test_ModelAnimationConverter.cpp`
- `Test_ModelSkeletonCompatibility.cpp`
- `Test_ModelSourceLoader.cpp`
- `Test_OzzAnimation.cpp`
- `Test_AngelScriptBaker.cpp`

Выбирайте самый узкий тест, покрывающий изменённый baker. Сгенерированные проектом имена CMake targets следует узнавать из preset/build files проекта, а не фиксировать в Engine doc.

## Маршрутизация изменений

- Новый baker: изменить `Source/Tools/*Baker.*`, зарегистрировать в `BaseBaker::SetupBakers()` или `SetupBakersHook()`, добавить focused tests.
- Аргументы команд: обновить `ScriptsAndBaking.cmake`.
- Resource build hash: обновить `WriteBuildHash.cmake` и build tests.
- Metadata: обновить `MetadataBaker.*` и [Generated API and Metadata](../../reference/metadata/index.md).
- Prototype: обновить `PrototypeFormatInterface.json`, [Prototype Format](../../how-to/content/prototype-format.md), model/reference, focused test и aggregate contract diff.
- Map: обновить `MapFormatInterface.json`, [Map Format](../../how-to/content/map-format.md), reference, focused/native tests и project rebake.
- Image/sprite: обновить `ImageFormatInterface.json`, [Image and Sprite Formats](../../how-to/content/image-format.md), tests, rebake и visible inspection.
- Fonts: обновить `FontFormatInterface.json`, [Font Formats](../../how-to/content/font-format.md), tests, rebake и typography inspection.
- Audio: обновить `AudioInterface.json`, [Audio](../../how-to/content/audio.md), tests, rebake и audible inspection на заявленных платформах.
- Video: обновить `VideoInterface.json`, [Video](../../how-to/content/video.md), tests, rebake и visible inspection.
- Effects: обновить `EffectFormatInterface.json`, [Effect Format](../../how-to/content/effect-format.md), tests, contract diff и visible backend scene.
- Script-specific bake: обновить `AngelScriptBaker.*` и [Scripting](../scripting-runtime/).

## Контрольный список проверки

1. Выполнить configure из корня встраивающего проекта.
2. Собрать baker application/library, если изменён соответствующий код.
3. Запустить focused native tests изменённого baker-а.
4. Выполнить `BakeResources`, когда важны incremental/cache/build-hash semantics.
5. После model source или Ozz conversion выполнить `ForceBakeResources`: это positive integration gate на реальных FBX проекта, а не только synthetic fixtures.
6. Рассматривать generated output только как результат, не как authored source.
7. При изменении responsibilities обновить этот документ и [BuildTools Pipeline](../../reference/cmake-and-buildtools/pipeline.md).
