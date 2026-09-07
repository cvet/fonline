---
layout: default
title: Руководство по дереву исходного кода
locale: ru
document_id: source-tree
permalink: /Docs/ru/contributing/source-tree/
---

<!-- docs-translation: {"document_id":"source-tree","locale":"ru","source_path":"Docs/en/contributing/source-tree/index.md","source_sha256":"3b2568b1d00bb6d0617a27e5f48164e5090b181dc39a56c489a9850079775882"} -->

# Руководство по дереву исходного кода

Это руководство объясняет, откуда начинать навигацию по `Source/`. Оно дополняет краткий [Source README](../../../../Source/README.ru.md).

## Быстрая маршрутизация

- Изменение запуска executable или target entrypoints: `Source/Applications/` и [Applications](../../reference/applications.md).
- Изменение низкоуровневой platform/utilities: `Source/Essentials/`.
- Изменение общих entity/property/map/config/network primitives: `Source/Common/`.
- Изменение client-side runtime или views: `Source/Client/`.
- Изменение authoritative world/server behavior: `Source/Server/`.
- Изменение scripting integration или видимых скриптам native methods: `Source/Scripting/`.
- Изменение developer tools, baking, Mapper или editors: `Source/Tools/`.
- Изменение абстракции application/window/rendering: `Source/Frontend/`.
- Поиск примеров поведения или regression coverage: `Source/Tests/`.

Сначала переходите в каталог-владелец. Перед указанием конкретного файла,
helper или target проверяйте его точное написание в текущем inventory исходного
кода: соседние имена backend и сгенерированные проектом executable нельзя
использовать как шаблоны.

## `Source/Applications/`

Содержит точки входа приложений и библиотек. Примеры включают client, варианты server, Mapper, editor, baker, AngelScript compiler и обёртки testing app. Wiring build targets находится в `BuildTools/cmake/stages/Applications.cmake`.

См. [Applications](../../reference/applications.md).

## `Source/Essentials/`

Низкоуровневые переиспользуемые primitives. Текущие файлы охватывают logging, core helpers, compression, containers, serialization, filesystem, exception handling, memory system, sockets, platform helpers, stack traces, string utilities, strong types, time helpers и worker threads.

Этот слой не должен получать game-specific rules. Он обязан оставаться пригодным для каждой runtime side.

## `Source/Common/`

Общий runtime code для client/server/tools/scripts. Основные области:

- База движка и общая настройка: `EngineBase.*`, `Common.*`.
- Entities/properties/prototypes: `Entity.*`, `EntityProperties.*`, `EntityProtos.*`, `Properties.*`, `ProtoManager.*`.
- Карты и движение: `MapLoader.*`, `Geometry.*`, `Movement.*`, `PathFinding.*`, `LineTracer.*`.
- Networking primitives: `NetBuffer.*`, `NetworkUdp.*`.
- Config/data access: `ConfigFile.*`, `DataSource.*`, `FileSystem.*`, `CacheStorage.*`.
- Общие presentation metadata и resources: `AnimationInfo.*`, `ModelBounds.*`, `SpriteResource.*`.
- Script bridge: `ScriptSystem.*`.

Если изменение переиспользуемо и общее для client и server, оно, вероятно, начинается здесь.

## `Source/Client/`

Client-side runtime и presentation-facing state. Он включает client startup/composition, connection handling, resource management, views для critters/items/maps/locations/player state, sprite/model/effect/font managers, render targets и варианты network-client transport. Polygonal sprite submission принадлежит `DefaultSprites.*`; автоматическая проекция model frame/view изолирована в `ModelSpriteLayout.*`. Parsing font descriptors, slot binding, measurement, wrapping и glyph drawing проходят через `FontManager.*` и [Font Format](../../how-to/content/font-format.md).

Не помещайте authoritative решения game state в client без документированного server contract и validation.

## `Source/Server/`

Authoritative runtime. Он включает server startup/composition, players, critters, items, maps, locations, entity managers, data validation, database backends, варианты network-server transport, server connections и поддержку updater backend.

Вопросы persistence, validation и authoritative entity lifecycle обычно начинаются с server behavior.

## `Source/Scripting/`

Script integration и регистрация видимых скриптам native methods. Каталог разделён на integration folders (`AngelScript`, `Native`, `Mono`) и файлы регистрации methods, сгруппированные по runtime side и entity type: common/client/server global methods и critter/item/map/player methods.

При изменении nullable script/native signatures используйте [Nullability](../../../Nullability.md).

## `Source/Tools/`

Developer и build-time tools. Текущие tool files включают baker classes,
config/effect/image/map/model/proto/text bakers, Mapper, AnimationViewer,
ParticleViewer и размещённый в Mapper SPARK particle editor. В текущем дереве
нет generic Editor или реализации AssetExplorer.

Cross-baker reporting принадлежит `BakingReport.*`. Polygonal 2D geometry
изолирована в `SpriteMeshing.*`, а model animation bounds вычисляются
`ModelBoundsCalculator.*`; их container integration остаётся в соответствующих
image/model bakers.

Документация build/resource pipeline должна ссылаться на эти файлы и вызывающий их CMake stage, а не делать выводы из имён приложений.
Particle authoring имеет отдельный `ParticleBaker`: работу с `.spark`/`.spk`, `.efkproj`/`.efk`, backend options, SPARK registry/editor, Effekseer compilation, measured bounds, runtime framing и client integration направляйте в [Particle Format](../../how-to/content/particle-format.md).
Сфокусированная проверка critter animation и baked particles проходит через
[просмотр анимации и частиц](../../how-to/tools/animation-particle-viewers.md).
У font descriptors также нет отдельного baker: raw-copy settings направляйте через `RawCopyBaker`, ссылочные textures через `ImageBaker`, а parsing `.fofnt`/`.fnt` и text layout через [Font Format](../../how-to/content/font-format.md).

## `Source/Frontend/`

Абстракция application и rendering. Содержит варианты `Application*.cpp` и rendering backends, включая Direct3D, OpenGL, null rendering и общие rendering interfaces.

Этот слой относится к запуску native client, headless modes, testing, Web и Android platform notes.

## `Source/Tests/`

Тесты являются исполняемой базой знаний для многих подсистем движка. Имена файлов сгруппированы по подсистеме (`Test_Geometry.cpp`, `Test_NetBuffer.cpp`, `Test_DataBase.cpp`, `Test_AngelScript*.cpp` и т. д.). При добавлении новых категорий расширяйте [Source/Tests README](../../../../Source/Tests/README.ru.md) и сверяйте [Testing](../../../Testing.md) с текущим runner и generated targets.

## Антипаттерны навигации

- Не выводите target names из одного встраивающего проекта и не документируйте их как универсальные имена движка.
- Не помещайте game-specific behavior в документацию движка, если оно явно не помечено как пример.
- Не описывайте generated output как hand-authored source.
- Не превращайте source-tree README в большие руководства; используйте тематические страницы `Docs/` и ссылки на них.
