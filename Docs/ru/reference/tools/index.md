---
layout: default
title: Инструменты
locale: ru
document_id: tools
permalink: /Docs/ru/reference/tools/
---

<!-- docs-translation: {"document_id":"tools","locale":"ru","source_path":"Docs/en/reference/tools/index.md","source_sha256":"3e4aae068a33dd3e9e7c6166dbb7e8a321142a200b691972c05bfe766bc96f2f"} -->

# Инструменты

> Документация движка. Эта страница сопоставляет переиспользуемые инструменты из `Source/Tools/` с точками входа приложений. Конкретные игровые content pipelines, project-specific command lines и release automation принадлежат документации встраивающего проекта, если они явно не отмечены как примеры.

## Назначение

Используйте эту страницу, чтобы определить владельца workflow, найти точку входа приложения в tool layer или перейти к более глубокому документу с деталями реализации.

В FOnline центральным интерактивным средством редактирования является Mapper. Отдельно собранный upstream Effekseer Editor доступен как необязательный сопутствующий authoring tool. Вместе эти маршруты включают:

- command-line bakers для ресурсов, scripts и metadata;
- Mapper и встроенный в него particle editor;
- processors для отдельных типов assets: images, effects, models, maps, protos, text, configs и raw copies;
- application wrappers в `Source/Applications/`, которые инициализируют platform layer движка и запускают инструмент.

## Проверенные пути исходников

- `Source/Tools/Baker.h`
- `Source/Tools/Baker.cpp`
- `Source/Tools/*Baker.h`
- `Source/Tools/*Baker.cpp`
- `Source/Tools/Mapper.h`
- `Source/Tools/Mapper.cpp`
- `Source/Tools/AnimationViewer.h`
- `Source/Tools/AnimationViewer.cpp`
- `Source/Tools/ParticleViewer.h`
- `Source/Tools/ParticleViewer.cpp`
- `Source/Tools/SparkParticleEditor.h`
- `Source/Tools/SparkParticleEditor.cpp`
- `Source/Applications/BakerApp.cpp`
- `Source/Applications/ASCompilerApp.cpp`
- `Source/Applications/MapperApp.cpp`
- `Source/Applications/AnimationViewerApp.cpp`
- `Source/Applications/ParticleViewerApp.cpp`
- `Source/Applications/TestingApp.cpp`
- `BuildTools/EffekseerEditor/build.ps1`
- `BuildTools/buildtools.py`
- `ThirdParty/Effekseer/Dev/Editor/`
- `ThirdParty/Effekseer/Dev/Cpp/Viewer/`
- соответствующие tool/baker tests в `Source/Tests/`

## Карта слоя инструментов

В tool layer есть три основные формы:

1. **Пакетные command tools** - `BakerApp.cpp` и `ASCompilerApp.cpp` выполняют детерминированные преобразования файлов по settings проекта и resource packs.
2. **Интерактивный runtime tool** - `MapperApp.cpp` создает frontend window и выполняет per-frame цикл редактирования maps и content в Mapper.
3. **Переиспользуемые processors** - `Source/Tools/*Baker.*`, `Mapper.*` и реализации particle subeditors предоставляют работу, используемую приложениями.

Создание CMake targets и package wiring описаны в [Applications](../applications.md) и [конвейере BuildTools](../cmake-and-buildtools/pipeline.md). Внутреннее устройство resource bake описано в [конвейере baking](../../explanation/content-pipeline/baking.md).

## Точки входа приложений

### Приложение Baker

`Source/Applications/BakerApp.cpp` инициализирует application layer с `DisableLogTags`, создает `MasterBaker`, вызывает `BakeAll()` и завершает процесс с результатом baking.

Используйте его для полного resource baking. `BuildTools/cmake/stages/ScriptsAndBaking.cmake` связывает command targets `BakeResources` и `ForceBakeResources` с project baker app.

### Приложение компилятора AngelScript

`Source/Applications/ASCompilerApp.cpp` подготавливает metadata и компилирует AngelScript resource packs. Сначала оно выполняет metadata baking для packs с `MetadataBaker`, затем компилирует packs с `AngelScriptBaker`.

Используйте его для build path `CompileAngelScript`. Более широкая модель script runtime описана в разделе [Скриптинг](../../explanation/scripting-runtime/).

### Приложение Mapper

`Source/Applications/MapperApp.cpp` инициализирует frontend/application layer, ожидает готовности persistent data в Web builds, создает `MapperEngine`, блокирует input в headless mode, вызывает `MapperMainLoop()` в каждом frame и корректно завершает Mapper при выходе.

Используйте его для редактирования maps, mapper-side automation и headless workflows на основе Mapper. Полный пользовательский UI workflow приведен в [интерактивном руководстве Mapper](../../how-to/tools/mapper-interactive.md), а lifecycle, automation и script helpers описаны в [инструментах Mapper](../../how-to/tools/mapper.md).

### Тестовое приложение

`Source/Applications/TestingApp.cpp` является точкой входа test runner. Владение suites описано в [инвентаре native test suites](../../../../Source/Tests/README.ru.md) и разделе [Тестирование](../../contributing/testing/).

## Инструменты baking

Семейство baking является наиболее зрелой группой batch tools. `Source/Tools/Baker.h` и `.cpp` определяют общую инфраструктуру:

- `BakingContext` несет settings, имя pack, write callback, существующие baked files и hints sync/async mode;
- `BaseBaker` определяет `GetName()`, `GetOrder()` и `BakeFiles()`;
- `BaseBaker::SetupBakers()` создает запрошенные built-in bakers и предоставляет `SetupBakersHook()` для project/external extension;
- `MasterBaker::BakeAll()` координирует полное baking resource packs;
- `BakerDataSource` адаптирует baked/input files к интерфейсу источника данных движка.

Встроенные реализации bakers:

- `Source/Tools/MetadataBaker.*` - разбирает metadata tags и создает metadata resources для runtime registration;
- `Source/Tools/ConfigBaker.*` - выпекает configuration resources;
- `Source/Tools/RawCopyBaker.*` - копирует выбранные resources без преобразования;
- `Source/Tools/ImageBaker.*` - импортирует image/sprite/frame formats, включая классические Fallout formats и PNG/TGA;
- `Source/Tools/EffectBaker.*` - выпекает shader/effect sources и shader stages;
- `Source/Tools/ParticleBaker.*` - преобразует native SPARK XML `.spark` в загружаемый из memory `.spk` и компилирует Effekseer XML `.efkproj` в проверенные raw `.efk` (`SKFE`) resources. Авторские runtime binaries `.spk`/`.efk` отклоняются;
- `Source/Tools/ProtoBaker.*` - выпекает prototype files с metadata/script-aware validation;
- `Source/Tools/MapBaker.*` - выпекает map files и проверяет отношения maps/protos;
- `Source/Tools/TextBaker.*` - выпекает text packs;
- `Source/Tools/ProtoTextBaker.*` - выпекает prototype text data;
- `Source/Tools/ModelMeshBaker.*` - при включенной 3D support выпекает текущую FBX/OBJ hierarchy, meshes, skinning, materials и animation data; см. [Формат моделей](../../how-to/content/model-format.md);
- `Source/Tools/ModelInfoBaker.*` - при включенной 3D support проверяет и выпекает composition `.fo3d` и общую таблицу длительностей model animation; см. [Формат моделей](../../how-to/content/model-format.md) и [Анимация моделей](../../how-to/content/model-animation.md);
- `Source/Tools/AngelScriptBaker.*` - при включенной AngelScript support компилирует и выпекает AngelScript bytecode resources.

Семантика tuple, speed, alias, metadata и typed lookup анимаций описана в [анимации моделей](../../how-to/content/model-animation.md). Per-frame offsets 2D sprites и зависящие от движения walk/run cycles описаны в [корневом движении спрайтов](../../how-to/content/sprite-root-motion.md). Подробный порядок baking, settings, запись outputs и validation принадлежат [конвейеру baking](../../explanation/content-pipeline/baking.md).

FOFNT и AngelCode BMFont descriptors не имеют собственного editor или dedicated baker в движке. Они создаются внешними средствами, копируются `RawCopyBaker` и разбираются клиентом; связанные images проходят обычный image pipeline. Синтаксис descriptor, владение `.bmfc`, binding/layout behavior и validation описаны в [форматах шрифтов и компоновке текста](../../how-to/content/font-format.md).

Для audio также нет dedicated editor или transcoding baker в движке. Создавайте WAV/ACM/Ogg внешними средствами, копируйте runtime bytes через `RawCopyBaker` и используйте раздел [Audio](../../how-to/content/audio.md) для требований к format, naming, playback и audible validation.

Video также полагается на внешние authoring tools. `RawCopyBaker` копирует `.ogv` без transcoding, а клиент декодирует Theora в Ogg. Точные paths, authoring limits, хранение whole resource in memory, fullscreen/embedded presentation, отдельный sound и visible validation описаны в разделе [Video](../../how-to/content/video.md).

## Интерактивные инструменты разработчика

### Mapper

`Source/Tools/Mapper.h` и `.cpp` реализуют `MapperEngine`, mapper-specific client-like runtime для редактирования maps и map entities. Он наследуется через client/view side движка, регистрирует mapper metadata, настраивает sprite factories, обрабатывает input, рисует map/editor UI frames, загружает и сохраняет maps, а также предоставляет scripts вспомогательные средства mapper automation.

Основные области в `Mapper.cpp`:

- lifecycle frame через `MapperMainLoop()` и `DrawMapperFrame()`;
- обработка input и helpers cursor/hex;
- ImGui panels для workspace, content, map list, map window, inspector, history, settings, console, script calls и backend-neutral particle-subeditor dispatch;
- загрузка, показ и сохранение maps через `LoadMapFromText()`, `LoadMap()`, `ShowMap()`, `SaveCurrentMap()` и `SaveMap()`;
- render-only capture через `SaveMapperScreenshot()` и отложенный full-window capture через `RequestMapperWindowScreenshot()`, который завершается непосредственно перед presentation и поэтому включает ImGui;
- интеграция mapper script system через mapper metadata и `MapperGlobalScriptMethods.cpp`.

Меню, windows, editing, history, save discipline и диагностика ошибок описаны в [интерактивном руководстве Mapper](../../how-to/tools/mapper-interactive.md). Lifecycle, extension points и automation/headless-render workflows описаны в [инструментах Mapper](../../how-to/tools/mapper.md).

### Отдельные viewers анимаций и частиц

`Source/Applications/AnimationViewerApp.cpp` и `ParticleViewerApp.cpp` являются сфокусированными hosts client content, которые создаются вместе с Mapper при включенном `FO_BUILD_MAPPER`. Они создают client-side resource services, но не запускают networking, login, maps и обычный client main loop. Каждое приложение открывает одно tool window на весь viewport, продвигает render-side managers и сохраняет пользовательский layout и controls при shutdown.

`AnimationViewer` перечисляет загруженные critter prototypes и пары animations, предоставляемые каждым 2D или 3D critter. Он показывает direction, scale, overlays, hierarchy, model layers и one-shot/idle transitions в игровом размере.

`ParticleViewer` перечисляет baked extensions, объявленные `ParticleSpriteFactory`, показывает `.spk` и `.efk` через игровой runtime и предоставляет проверку deterministic seed, direction, scale, prewarm, root и draw frame. Это только viewer: `.spark` редактируется в Mapper, а `.efkproj` - в Effekseer Editor.

Точные build/launch commands, controls AnimationViewer и ParticleViewer, persisted settings, review workflows, диагностика ошибок и provenance screenshots приведены в [инструментах просмотра](../../how-to/tools/animation-particle-viewers.md). В текущем движке нет generic приложения Editor или AssetExplorer.

Package interface содержит явные binary roles `AnimationViewer` и `ParticleViewer` для native Windows/Linux developer artifacts. Оба являются package targets без ресурсов (`NoRes`); при запуске предоставьте обычные настроенные data sources или developer bundle встраивающего проекта. Generic role `Editor` отсутствует.

### Редактор частиц SPARK

`Source/Tools/ParticleEditor.h` и `.cpp` определяют virtual boundary particle subeditor для Mapper и backend-neutral окно **Windows -> Particle preview**. `Source/Tools/SparkParticleEditor.h` и `.cpp` определяют SPARK-specific subeditor и asset editor. Его browser перечисляет исходные `.spark` sources и открывает отдельное editor window для выбранного asset. Каждое окно показывает memory-backed resource, предоставляет native object parameters, включая FOnline `SparkQuadRenderer`, сохраняет XML через raw resource filesystem Mapper и при закрытии измененного asset запрашивает Save / Discard / Cancel. Для Effekseer используется отдельно собранный upstream Editor, описанный ниже. Native Mapper показывает `.efk`, созданный его baking data source, и остается финальной проверкой через собственный renderer FOnline.

`Source/Tools/EffekseerCompiler.h/.cpp` является native C++ fixed-profile compiler, который напрямую использует `ParticleBaker`. Он принимает Editor 1.80.5, XML `.efkproj` project version 3 и возвращает raw `SKFE` bytes вместе с путями связанных ресурсов. Runtime targets потребляют только сгенерированный binary и не включают compiler; Web использует заранее baked `.efk`.

SPARK editor компилируется только с `FO_SPARK_PARTICLES`. Neutral preview всегда входит в Mapper, запрашивает у зарегистрированной particle sprite factory доступные extensions и скрывает menu entry, если particle runtime не включен. Оба runtime options по умолчанию равны `OFF`, поэтому встраивающий проект должен явно включить нужную particle support.

### Developer bundle Effekseer Editor

`BuildTools/EffekseerEditor/build.ps1` собирает закрепленный source-only upstream English-only Editor как изолированный Windows win64 developer tool. Script публикует self-contained managed UI на .NET 10 вместе с native Viewer, material compilers Direct3D 11/OpenGL, material editor, English localization, icons, meshes и лицензией инструмента. Оба процесса Editor используют английский subset Source Han размером 17.4 KiB вместо upstream коллекции CJK fonts размером 17.4 MiB. Это не FOnline application entry point, и ни один из UI/viewer components не линкуется в runtime library.

Изолированная native build использует принадлежащие движку source trees zlib, libpng, Ogg и Vorbis, не сохраняя дубликаты под Effekseer. Локальная docking-imgui остается необходимой, потому что Viewer backends используют multi-viewport ABI, отсутствующий в стандартной ветке imgui движка.

Editor не имеет Engine CMake option или target и не является универсальным выбором `buildtools.py build`. В Windows запускайте `buildtools.py build-auxiliary effekseer-editor Release`; зарегистрированный recipe конфигурирует только изолированную upstream native build. В переиспользуемом packager нет binary role `EffekseerEditor`. Встраивающий проект использует generic package declaration `INCLUDE`, чтобы скопировать prebuilt payload в developer package и обновить существующий `SingleZip`.

Адаптация FOnline делает authoring source-first. Команды Editor **Save** и **Save As** записывают нормализованный XML `.efkproj`. Стандартный preview Editor остается доступным для authoring iteration. GIF recording отключен в bundle FOnline; preview в Mapper по-прежнему обязателен для проверки baked effect через renderer FOnline и supported-capability gate.

Отдельного generic приложения Editor или `EditorLib` нет; Mapper является центральным интерактивным средством редактирования движка.

Практический workflow описан в [инструментах создания частиц](../../how-to/tools/particle-authoring.md). Точный source/runtime contract, покрытие SPARK object handlers, нормализация save, preview dependencies и project validation boundary описаны в [формате и runtime частиц](../../how-to/content/particle-format.md) и сгенерированной [справке по particle tooling](../../../generated/particle-format/tooling.md). Project-specific asset catalogs и visual-quality procedures остаются во встраивающем проекте.

## Границы владения

Документация движка используется для:

- точек входа apps/tools и их переиспользуемого lifecycle;
- типов bakers и принадлежащих движку преобразований;
- extension points Mapper и particle editors;
- validation checklists и tests;
- ссылок на CMake/app wiring.

Документация встраивающего проекта используется для:

- конкретной политики размещения content;
- game-specific соглашений maps/protos/text;
- product-specific generated outputs;
- точных preset names или binary names вроде `LF_Mapper`, если они явно не помечены как пример;
- downstream pipelines, использующих tool output для конкретной игровой функции.

## Проверяемые тесты

Поведение bakers и tools покрывается сфокусированными тестами в `Source/Tests/`:

- `Test_BakerSetup.cpp`
- `Test_ConfigBaker.cpp`
- `Test_MetadataBaker.cpp`
- `Test_RawCopyBaker.cpp`
- `Test_ImageBaker.cpp`
- `Test_EffectBaker.cpp`
- `Test_ParticleBaker.cpp`
- `Test_ProtoBaker.cpp`
- `Test_ProtoTextBaker.cpp`
- `Test_MapBaker.cpp`
- `Test_TextBaker.cpp`
- `Test_ModelBaker.cpp`
- `Test_AngelScriptBaker.cpp`

Поведение Mapper UI слабее покрыто отдельными unit tests. Проверяйте такие пути в самом Mapper.

## Маршрутизация изменений

- Синтаксис model description, source meshes, layers, attachments, materials, cuts и runtime composition: `Source/Tools/ModelMeshBaker.*`, `Source/Tools/ModelInfoBaker.*`, [Формат моделей](../../how-to/content/model-format.md).
- Tuples model animation и metadata effective duration: `Source/Tools/ModelInfoBaker.*`, [Анимация моделей](../../how-to/content/model-animation.md).
- Image frame offsets и выравнивание 2D walk/run: `Source/Tools/ImageBaker.*`, `Source/Client/CritterHexView.*`, [Корневое движение спрайтов](../../how-to/content/sprite-root-motion.md).
- Полная оркестрация resource bake: `Source/Tools/Baker.*`, `Source/Applications/BakerApp.cpp`, [Конвейер baking](../../explanation/content-pipeline/baking.md).
- Компиляция script bytecode: `Source/Tools/AngelScriptBaker.*`, `Source/Applications/ASCompilerApp.cpp`, [Скриптинг](../../explanation/scripting-runtime/).
- Metadata tags и сгенерированные metadata resources: `Source/Tools/MetadataBaker.*`, [Сгенерированный API и метаданные](../metadata/index.md).
- Интерактивное редактирование maps: `Source/Tools/Mapper.*`, `Source/Applications/MapperApp.cpp`, [Интерактивное руководство Mapper](../../how-to/tools/mapper-interactive.md).
- Headless mapper automation и script helpers: `Source/Tools/Mapper.*`, `Source/Scripting/MapperGlobalScriptMethods.cpp`, [Инструменты Mapper](../../how-to/tools/mapper.md).
- Boundary particle editor и preview: `Source/Tools/ParticleEditor.*`; Mapper вызывает только neutral lifecycle и drawing hooks.
- Реализация SPARK particle editor: `Source/Tools/SparkParticleEditor.*`, собранная за neutral particle-editor boundary.
- Сборка/staging Effekseer authoring tool и optional generic package include: `BuildTools/buildtools.py`, `BuildTools/EffekseerEditor/build.ps1`, `ThirdParty/Effekseer/Dev/Editor/` и [конвейер BuildTools](../cmake-and-buildtools/pipeline.md).
- Оболочки standalone animation/particle viewers: `Source/Tools/AnimationViewer.*`, `Source/Tools/ParticleViewer.*`, `Source/Applications/AnimationViewerApp.cpp`, `Source/Applications/ParticleViewerApp.cpp` и [инструменты просмотра](../../how-to/tools/animation-particle-viewers.md).
- Particle source formats, baking и runtime factories: `Source/Tools/ParticleEditor.*`, [Формат и runtime частиц](../../how-to/content/particle-format.md).
- Particle Preview, SPARK editor, закрепленный Effekseer editor и visible review: [Инструменты создания частиц](../../how-to/tools/particle-authoring.md).
- Создание font descriptor и runtime binding: внешние bitmap-font tools, `Source/Tools/RawCopyBaker.*`, `Source/Client/FontManager.*`, [Форматы шрифтов и компоновка текста](../../how-to/content/font-format.md).
- Создание и playback audio: внешние audio tools, `Source/Tools/RawCopyBaker.*`, `Source/Client/SoundManager.*`, `Source/Frontend/Application.*`, [Audio](../../how-to/content/audio.md).
- Создание и playback video: внешние video tools, `Source/Tools/RawCopyBaker.*`, `Source/Client/VideoClip.*`, `Source/Client/Client.*`, [Video](../../how-to/content/video.md).
- Wiring application targets: [Applications](../applications.md) и [конвейер BuildTools](../cmake-and-buildtools/pipeline.md).

## Контрольный список проверки

1. Для изменений baker запустите самый узкий затронутый baker test, затем project bake target, если поведение пересекает границу resource pack.
2. Для изменений script compilation запустите AngelScript baker/compiler tests и project path `CompileAngelScript`.
3. Для изменений Mapper запустите mapper path, владеющий изменением; в headless flows проверяйте generated output, а не только exit code процесса.
4. Для изменений focused viewers запустите `BuildTools/tests/test_docs_viewer_tools.py`, соберите и запустите оба viewer targets и визуально выполните измененный workflow. Для изменений particle UI также проверьте интерактивный Mapper на затронутой platform/backend.
5. Для изменений Effekseer Editor выполните `buildtools.py build-auxiliary effekseer-editor Release` на Windows win64, запустите managed UI, сохраните текстовый `.efkproj`, затем выполните baking и preview проекта в Mapper. При изменении staged layout проверьте generic package `INCLUDE`.
6. Если изменилось wiring tool target, проверьте [Applications](../applications.md), [конвейер BuildTools](../cmake-and-buildtools/pipeline.md) и сгенерированные CMake targets из встраивающего проекта.
7. Оставляйте game-specific tool pipelines в документации встраивающего проекта и ссылайтесь на документацию движка для переиспользуемой механики.
