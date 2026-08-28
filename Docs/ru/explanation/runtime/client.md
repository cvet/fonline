---
layout: default
title: Клиентская среда выполнения
locale: ru
document_id: client-runtime
permalink: /Docs/ru/explanation/runtime/client.html
---

<!-- docs-translation: {"document_id":"client-runtime","locale":"ru","source_path":"Docs/en/explanation/runtime/client.md","source_sha256":"71c90f097393a1f47c932747332dc30629ddce4b50aa6fbf2a9717094d00fa79"} -->

# Клиентская среда выполнения

> Документация движка. Эта страница описывает переиспользуемое поведение клиентского runtime в `Source/Client/`; политика игрового интерфейса, игровые правила и конкретный контент принадлежат встраиваемому проекту.

## Назначение

Клиентский runtime превращает запечённые ресурсы, состояние сервера, локальный ввод и скрипты в вид игры для игрока. Он не владеет решениями игрового дизайна, а предоставляет переиспользуемые части движка, которыми игровой проект управляет через скрипты, конфигурацию, ресурсы и серверные сообщения.

Читайте эту страницу вместе со следующими документами:

- [Модель сущностей](../entity-and-property-model/) - модель сущностей, свойств и прототипов, которую оборачивают клиентские views.
- [Карты, движение и геометрия](../maps-and-movement.md) - позиции на карте, поиск пути, трассировка линий и контексты движения.
- [Форматы изображений и спрайтов](../../how-to/content/image-format.md) - импорт изображений, композиция FOFRM, загрузка запечённых sprites, atlases и cache behavior.
- [Форматы шрифтов и компоновка текста](../../how-to/content/font-format.md) - дескрипторы bitmap fonts, привязка slots, масштаб при привязке, layout текста, rendering flags и inline colors.
- [Sprite Root Motion](../../how-to/content/sprite-root-motion.md) - 2D offsets отдельных кадров и согласование циклов ходьбы и бега с движением.
- [Video.md](../../../Video.md) - экспериментальные ресурсы Ogg/Theora, fullscreen и embedded playback, память и визуальная проверка.
- [Сеть и авторитетность](../authority-and-networking/) - command buffers, transports и синхронизация свойств.
- [Frontend и рендеринг](../rendering/) - platform windows, input, audio и renderer backends.
- [сборка, упаковка и отладка в браузере](../../how-to/platforms/web-debugging.md), [сборка, упаковка и отладка на Android](../../how-to/platforms/android-debugging.md) и [нативная отладка и отладка AngelScript](../../troubleshooting/debugging.md) для platform-specific validation flows.

## Проверенные пути исходного кода

- `Source/Client/Client.h`
- `Source/Client/Client.cpp`
- `Source/Client/ClientConnection.h`
- `Source/Client/ClientConnection.cpp`
- `Source/Client/ResourceManager.h`
- `Source/Client/ResourceManager.cpp`
- `Source/Client/FontManager.h`
- `Source/Client/FontManager.cpp`
- `Source/Client/MapView.h`
- `Source/Client/MapView.cpp`
- `Source/Scripting/ClientMapScriptMethods.cpp`
- `Source/Scripting/AngelScript/CoreScripts/Gui.fos`
- `Source/Client/CritterView.h`
- `Source/Client/CritterHexView.h`
- `Source/Client/ItemView.h`
- `Source/Client/ItemHexView.h`
- `Source/Client/LocationView.h`
- `Source/Client/PlayerView.h`
- `Source/Client/DefaultSprites.h`
- `Source/Client/DefaultSprites.cpp`
- `Source/Client/SpriteManager.h`
- `Source/Client/SpriteManager.cpp`
- `Source/Client/TextureAtlas.h`
- `Source/Client/TextureAtlas.cpp`
- `Source/Client/ModelAnimation.h`
- `Source/Client/ModelAnimation.cpp`
- `Source/Client/ModelBakedData.h`
- `Source/Client/ModelBakedData.cpp`
- `Source/Client/ModelManager.h`
- `Source/Client/ModelManager.cpp`
- `Source/Client/ModelInstance.h`
- `Source/Client/ModelInstance.cpp`
- `Source/Client/ModelInformation.h`
- `Source/Client/ModelInformation.cpp`
- `Source/Client/ModelHierarchy.h`
- `Source/Client/ModelHierarchy.cpp`
- `Source/Client/ModelSprites.h`
- `Source/Client/ModelSprites.cpp`
- `Source/Client/ModelSpriteLayout.h`
- `Source/Client/ModelSpriteLayout.cpp`
- `Source/Common/AnimationInfo.h`
- `Source/Common/AnimationInfo.cpp`
- `Source/Common/ModelBounds.h`
- `Source/Common/ModelBounds.cpp`
- `Source/Client/ParticleSprites.h`
- `Source/Client/ParticleSprites.cpp`
- `Source/Client/RenderTarget.h`
- `Source/Client/RenderTarget.cpp`
- `Source/Tests/Test_ClientEngine.cpp`
- `Source/Tests/Test_ClientRuntimeApi.cpp`
- `Source/Tests/Test_ClientDataValidation.cpp`
- `Source/Tests/Test_ClientServerIntegration.cpp`
- `Source/Tests/Test_ModelBaker.cpp`
- `Source/Tests/Test_ModelAnimationRuntime.cpp`

## Владелец runtime: `ClientEngine`

`ClientEngine` из `Source/Client/Client.h` - центральный объект клиентской части движка. Он наследует `BaseEngine` и `AnimationResolver`, владеет высокоуровневым жизненным циклом клиента и предоставляет event hooks клиентским скриптам.

Основные обязанности:

- инициализировать и завершать клиентский runtime;
- выполнять покадровый `MainLoop()`;
- обрабатывать события ввода приложения;
- подключаться к серверу и отключаться через `ClientConnection`;
- создавать, регистрировать, отменять регистрацию и находить клиентские сущности по id;
- получать и применять сетевые сообщения персонажей, предметов, карт, custom entities, синхронизации времени, движения, действий и свойств;
- владеть клиентскими managers sprites, effects, fonts, sounds, video playback, resources, cache storage и render targets;
- поднимать события движка `OnStart`, `OnLoop`, `OnConnected`, `OnDisconnected`, стадии render-map, события ввода, входа и выхода сущностей и загрузки или выгрузки карты.

`ClientEngine` намеренно широк: это composition root, где данные Common-слоя (`Entity`, свойства, прототипы, networking buffers) встречаются с сервисами Frontend-слоя (`Application`, render, input, audio) и игровыми скриптами. Контракты игровых media отдельно описаны в [Audio.md](../../../Audio.md) и [Video.md](../../../Video.md).

## Жизненный цикл клиента

Обычный жизненный цикл клиента состоит из следующих фаз:

1. **Инициализация приложения** выполняется во frontend-слое. `Application` владеет главным окном, renderer, input и audio. См. [Frontend и рендеринг](../rendering/).
2. **Выбор файловой системы ресурсов** начинается в `GetClientResources(GlobalSettings&)` из `Source/Client/Client.cpp`, который строит клиентское представление `FileSystem` для runtime managers. Установленные клиенты могут добавлять writable resource overlay с более высоким приоритетом над read-only base resources; см. [Конфигурация и источники данных](../../reference/settings/configuration-and-data-sources.md) и [Client Updater](client-updater.md).
3. **Создание движка** связывает settings, resources, главное окно приложения, сгенерированные метаданные, script modules и client managers.
4. **`OnStart` и инициализация скриптов** предоставляют скриптам первую клиентскую точку входа. `Source/Tests/Test_ClientEngine.cpp` проверяет, что module init и loop calls доступны в самодостаточном client runtime.
5. **Главный цикл** обрабатывает frontend input, network packets, scripted loop callbacks, visual effects, video playback, map processing и обновления, связанные с rendering.
6. **Сетевое подключение** начинается с `Connect()`, передающего transport setup и handshake в `ClientConnection`.
7. **Состояние карты и сущностей** приходит в сетевых сообщениях, представляется клиентскими view entities и обновляется через property sync и movement/action packets.
8. **Завершение работы** отключает сеть, уничтожает inner entities, очищает caches и render targets и освобождает frontend resources.

При изменении startup или shutdown согласованно обновляйте script events, lifetime managers, регистрацию сущностей и network callbacks: эти пути тесно связаны.

## Подключение к серверу и отправка сообщений

`ClientConnection` (`Source/Client/ClientConnection.h`, `Source/Client/ClientConnection.cpp`) владеет состоянием клиентского transport и скрывает, используется ли interthread, TCP sockets или UDP-capable sockets.

Основные обязанности:

- `Connect()` выбирает connection mode из `ClientNetworkSettings` и создаёт `NetworkClientConnection`;
- `CreateNetworkConnection(bool use_udp)` выбирает socket или UDP socket transport;
- `Process()` продвигает connect, receive, dispatch и send;
- `Disconnect()` завершает активный transport и вызывает настроенный disconnect handler;
- `TryFallbackToTcp()` предоставляет fallback при ошибке настройки UDP;
- `Net_SendHandshake()`, `Net_OnHandshakeAnswer()` и `Net_OnPing()` обрабатывают protocol messages уровня соединения;
- `AddMessageHandler(NetMessage, MessageCallback)` связывает protocol messages с handlers `ClientEngine::Net_On...`.

Семантическими handlers владеет `ClientEngine`. Среди них `Net_OnInitData`, `Net_OnAddCritter`, `Net_OnRemoveCritter`, `Net_OnProperty`, `Net_OnLoadMap`, `Net_OnSomeItems`, `Net_OnRemoteCall`, `Net_OnAddCustomEntity` и `Net_OnRemoveCustomEntity`.

Формат протокола описан в [сети и авторитетности](../authority-and-networking/). Проверка client/server handshake находится в `Source/Tests/Test_ClientServerIntegration.cpp`, прежде всего в `ClientAndServerHandshakeOverInterthreadTransport`.

Клиентские script continuations, запланированные через `ScheduleDelayedCallback()`, обрабатываются один раз за проход main loop из snapshot callback, уже готовых в начале этого прохода. Callback, который планирует другой callback с нулевой задержкой, включая `Yield(0)`, продолжится на следующем проходе, а не re-enter немедленно. Это не позволяет script wait loops вытеснить следующий network/input tick.

## Модель сущностей и представлений

Клиентские игровые объекты не являются необработанными серверными сущностями. Это view entities, объединяющие данные сущностей Common-слоя с клиентским состоянием rendering, input и presentation.

Основные view types:

- `PlayerView` - клиентское представление текущей сущности игрока.
- `LocationView` - клиентская сущность локации.
- `MapView` - сущность карты вместе с map rendering, локальными spatial indexes, fog, lighting, scrolling, zoom и hit testing.
- `CritterView` - сущность персонажа, видимая вне контекста текущей карты.
- `CritterHexView` - персонаж на гексе карты с поведением движения, направления и map rendering.
- `ItemView` - сущность предмета в инвентаре или общем клиентском контексте.
- `ItemHexView` - предмет на гексе карты с map flags, размещением sprite, blockers и влиянием на lighting.
- custom client entities - создаются `CreateCustomEntityView()` при получении синхронизируемого custom entity entry.

`ClientEngine::RegisterEntity()` и `ClientEngine::UnregisterEntity()` поддерживают lookup id-to-entity для network handlers и скриптов. `Source/Tests/Test_ClientEngine.cpp` проверяет регистрацию клиентских сущностей и их удаление из lookup.

Script GUI widgets `ItemView` кешируют handles предметов, привязанные к cells. `Resort()` удаляет destroyed handles, возвращённые stale supplier, сохраняет cell только когда источник вернул тот же живой экземпляр handle, и перепривязывает replacement clone с тем же entity id, чтобы callback отрисовки предмета видел актуальное количество и остальные projected properties.

## Архитектура runtime 3D-моделей

Прежние umbrella modules `3dAnimation` и `3dStuff` удалены. Клиентский 3D path теперь использует пары модулей `Model*.h` / `Model*.cpp` с одной границей владения на модуль. Отдельного клиентского utility module `ModelMesh` нет: типы представления mesh принадлежат `ModelHierarchy`. Отдельного `ModelPoseRuntime` также нет: renderer-independent операции pose и links входят в `ModelAnimation`.

| Модуль | Ответственность |
| --- | --- |
| `ModelManager` | Runtime entry point, caches описаний и hierarchies моделей, загрузка baked mesh и создание model instances. |
| `ModelHierarchy` | Общая загруженная topology `ModelBone`, mesh/bind data, model textures/effects и типы представления mesh для hierarchy и instances. Не владеет изменяемым результатом animation pose. |
| `ModelInformation` | Одно разрешённое baked-описание `.fo3d`: ссылка на hierarchy, canonical/runtime joint identities, animation lookup tables, cuts/links, draw metadata и один immutable `ModelAnimationRuntimeRig`. |
| `ModelInstance` | Всё изменяемое per-instance состояние: controller timelines, runtime pose, snapshots world matrices, mesh state и batching, cuts, attachments, particles, procedural transforms, projection и draw submission. |
| `ModelAnimation` | Timeline, transitions, callbacks, reverse/freeze/play-once и binding semantics animation controller движка вместе с engine-owned контрактом clip/rig/pose, sampling и blending на Ozz за PImpl, canonical-joint mapping, linked-pose resolution и проверенным построением rest-pose matrices для direct raw models. |
| `ModelBakedData` | Небольшие defensive reader helpers, общие для client model loaders, прежде всего preflight count-versus-unread-data до allocation. |
| `ModelSprites` | Адаптер `ModelManager` / `ModelInstance` к atlas-backed и direct-scene sprite rendering. |

Направление владения выбрано намеренно:

- `ModelManager` кеширует общие объекты `ModelHierarchy` и `ModelInformation`;
- каждый созданный `ModelInstance` ссылается на один `ModelInformation`, но владеет своими изменяемыми controllers, pose buffers, matrices, mesh overrides и children;
- общие nodes `ModelBone` хранят только topology, rest/bind и drawable data; результаты pose никогда не записываются обратно в общую hierarchy;
- canonical animation joints могут существовать без физического `ModelBone`, поэтому canonical indexing и cross-model pose links принадлежат `ModelAnimation`, а не `ModelHierarchy`.

Это разделение устраняет прежнюю зависимость include-everything. Вызывающий код подключает только manager, information, hierarchy, instance, animation или baked-data contract, который он использует. Небольшие пассивные типы остаются со своим owning module, а не образуют пустые translation units.

`ModelAnimationRuntimeClip`, `ModelAnimationRuntimeRig` и `ModelAnimationRuntimePose` скрывают backend state за PImpl. Public header предоставляет только engine-owned runtime values и spans; Ozz headers, archive objects, sampling contexts и matrix buffers остаются в `ModelAnimation.cpp`. `ModelAnimationData` задаёт native versioned wire contract, общий с baker, а `ModelAnimationConverter` владеет offline conversion. Engine API не называет и не моделирует взаимозаменяемые animation backends: Ozz является implementation detail native model-animation runtime и baked format движка.

## Анимация модели персонажа

Полный контракт загрузки и композиции `.fo3d`, включая source meshes, layers, root modifiers, child models, particles, transforms, materials, cuts и rendering controls, описан в [формате моделей](../../how-to/content/model-format.md).

3D-модели персонажей используют отдельные controllers body/action и movement animations. `ModelInstance::PlayAnim()` применяет animation-specific speed (`AnimSpeed`) к controller тела и действия, а `RefreshMoveAnimation()` назначает gait и масштабирование movement speed track контроллера движения. Когда активны оба, movement controller продвигается только с base/link/global speed модели и не должен наследовать `AnimSpeed` текущего действия тела. Иначе быстрые действия вроде use/pick-up чрезмерно ускоряют цикл ног движущегося персонажа.

Имя анимации с префиксом `~` проигрывает source clip в обратном направлении: playback time `t` выбирает clip в `duration - fmod(t, duration)`, поэтому точная граница loop начинает с конца clip. При отключённой интерполяции то же правило nearest key применяется к этому обратному source time.

Каждый baked `.fo3d` имеет обязательный versioned header `LFMODINF` schema 1 и обязательный payload `LFOZZRIG`, созданный для pinned `ozz-animation` 0.16.0. `ModelInformation` строго загружает и хранит один immutable `ModelAnimationRuntimeRig`. Его PImpl содержит canonical Ozz skeleton, unique clips, base/clip remaps, presence masks, nearest timelines и разрешённую таблицу state/action bindings. Loader отклоняет старые unversioned files и любой partial или inconsistent rig; после ошибки Ozz он не переходит к другому payload. Финальный mesh-only wire transition использует compatibility marker `0.0.30` и требует полного rebake ресурсов.

Ozz является production path animated pose. Controllers тела и движения продвигают только timeline/event state. Каждая зарегистрированная анимация хранит прямые Ozz clip index/duration/reverse metadata и immutable набор bound joints из presence mask clip. Joint привязывается только при совпадении canonical и точного runtime имени, поэтому resource-renamed root модели намеренно не попадает в source-root animation. Per-track masks allowed joints и transition suppression дополнительно фильтруют bindings до передачи track state в per-instance Ozz pose.

Ozz выполняет sampling clip, blending тела, замену movement-only joints, procedural pre-rotation тела и головы и local-to-model evaluation. Каждый `ModelInstance` сохраняет snapshot полученных world matrices, а skin palettes, linked children, particles и bone queries читают snapshot owning instance. Attachments link-all переопределяют только совпавший joint после evaluation и намеренно не пересчитывают descendants, сохраняя установленный порядок attachments.

Canonical joint names, точные runtime lookup names и bindings name-to-index не зависят от физической hierarchy `ModelBone`. Base joints сохраняют read-only physical bone для meshes и cuts; joints из анимации не имеют `ModelBone` и никогда не материализуются в общей cached hierarchy. Base root намеренно хранит runtime alias resource path, а contributed joints используют canonical names, сохраняя точный legacy lookup и semantics подавления root animation. Runtime particles, attachment resolution, link-all matching и queries bone position работают с canonical indices; поэтому link-all может привязать contributed joints без physical bones. Authored one-bone link validation в текущей baking schema по-прежнему ограничена base hierarchy.

Static `.fo3d` instances вычисляют пустой набор Ozz tracks, поэтому canonical rest и procedural transforms тела и головы используют тот же runtime, что и animated instances. За пределами Ozz остаются только direct raw-model instances, строящие parent-ordered world matrices через validation helpers из `ModelAnimation`.

Клиентская script pair `Game.DrawCritter3d(...)` и `Game.GetDrawCritter3dBounds(...)` поддерживает переиспользуемый GUI layout вокруг model sprite. После отрисовки instance запрос bounds возвращает два rectangles относительно draw anchor либо `false`, если instance ещё не создал valid model sprite. `drawRect` охватывает полный цикл выбранной animation и непрерывный диапазон facing вместе с projected shadow. `viewRect` - стабильный логический rectangle модели и layers для names, coarse picking и подобного presentation. Код GUI preview вписывает и центрирует draw rectangle; world-space overlays используют stable view rectangle как logical anchor, не дублируя 3D projection rules и не завися от текущего atlas crop.

Прежние custom pose evaluator и общая изменяемая table matrix output удалены. Baked model meshes теперь начинаются обязательным header `LFMODMSH` schema 1 и содержат только recursive hierarchy/bind/drawable mesh payload. Клиент потребляет его целиком и отклоняет данные без header, с несовпадающей schema, truncated или trailing data; serialized TRS tail и legacy mesh fallback отсутствуют. Runtime identity clip, duration, joint presence и sampling поступают только из baked Ozz rig.

Вложенный hash LF archive обнаруживает случайное повреждение, но не обеспечивает authentication. Ozz deserialization считает baked resource pack доверенным; deployments, позволяющие атакующему изменять packs, должны аутентифицировать pack до запуска loader.

## Загрузка sprites и atlases

Стандартный клиент не декодирует исходные PNG/TGA или legacy image bytes. `SpriteManager` приводит расширение baked resource к нижнему регистру, выбирает зарегистрированный `SpriteFactory` и кеширует копируемые sprites по path и `AtlasType`. `DefaultSpriteFactory` читает private container `ImageBaker` и возвращает `AtlasSprite` для одного static frame либо `SpriteSheet` для animation/directions. Конкретные RGBA frames загружаются в требуемый atlas с дублированными one-pixel filter borders и hit mask из alpha. Missing paths и неизвестные или failed factories memoized отдельно на весь lifetime manager.

Полный контракт source format, FOFRM, baked record, default extension, atlas и cache описан в [форматах изображений и спрайтов](../../how-to/content/image-format.md). Код проекта должен пропускать поддерживаемые sources через pinned baker, а не разбирать private baked stream.

Particle resources идут через отдельный backend-neutral factory path. `ParticleManager` выбирает включённый SPARK или Effekseer backend по baked extension `.spk` или `.efk`, кеширует immutable backend data и failed loads по точному path и создаёт независимое simulation state для каждого instance. `ParticleSpriteFactory` получает framing sprite из обязательных baked bounds и выбирает atlas-backed либо direct map-scene rendering; attachments к model bone используют 3D composition path. Полный контракт authoring, baking, renderer, cache, tooling, script и validation описан в [формате и исполнении частиц](../../how-to/content/particle-format.md).

## Root motion 2D-sprite

Движущиеся 2D-персонажи сохраняют логический progress path/hex в `MovingContext`, а `CritterHexView` проецирует текущее integer-pixel displacement на накопленный цикл `NextX` / `NextY` выбранного walk/run sheet. Полученные frame и `_offsAnim` выравнивают визуальную походку без изменения authoritative movement. Смена direction-specific sheet сохраняет wrapped phase цикла; начало и остановка движения сбрасывают anchor. Полный контракт данных и математики описан в [Sprite Root Motion](../../how-to/content/sprite-root-motion.md).

## `MapView`: представление карты и локальное пространственное состояние

`MapView` - крупнейший client view class, поскольку связывает несколько подсистем:

- загрузку map file/static data через `LoadFromFile()` и `LoadStaticData()`;
- обработку карты через `Process()`;
- map rendering через `DrawMap()` и staged render events в `ClientEngine`;
- field indexes предметов и персонажей;
- item hit testing по active map и indoor-mask sprites: drawable хранит owner
  `ItemHexView`, hidden primary sprite пропускается, а разрешённый
  extra/multihex sprite остаётся selectable. Перемещение или уничтожение item
  сразу меняет active-sprite set, поэтому stale padded viewport cell не может
  победить;
- преобразование координат, zoom, scrolling, screen-to-map и map-to-screen;
- helpers поиска и обрезки пути поверх `PathFinding::FindPath()`;
- line tracing для bullets и light fans. `MapView::ApplyLightFan` трассирует каждый light source на полный `Distance` в гексах, не менее 1, и передаёт per-light falloff metadata primitive shaders через `PrimitivePoint::EggData` в vertex attribute slot 3 (`InTexEggCoord`). `LightFanToPrimitves` записывает traced radius в гексах и normalized center alpha, чтобы shader мог восстановить hex-distance-from-edge fragment и затухание на фиксированной полосе гексов независимо от общей длины света. Light fan персонажа следует sprite offset персонажа (`HexView::GetSpriteOffsetPtr()`), поэтому свет всегда остаётся под sprite и эти значения не должны расходиться. Offset ограничен в источнике: `current_hex` из `MovingContext::EvaluateRawProgress` может отставать от smooth lerp, а client prediction reconciliation в `ReceiveCritterMoving` может включить inter-hex delta в offset при быстрых нажатиях шага, но `CritterHexView::StopMoving` переносит накопленный offset обратно в hex, привязываясь к фактически достигнутому sprite гексу и сохраняя только sub-hex remainder с инвариантом `hex + offset`, чтобы sprite не прыгал. Нормализация отказывается выбирать другой target hex, если это поле movement-blocked, и сохраняет исходный offset, не позволяя client reconciliation сообщить hex, к которому сервер не может найти путь. Offset не может неограниченно расти и утащить fan от персонажа. Items используют тот же `GetSpriteOffsetPtr()`;
- fog-of-war layers;
- локальные lighting sources и render targets;
- helpers mapper mode для tools движка.

`MapView` остаётся клиентским view общей модели карты. Переиспользуемые правила координат и поиска пути принадлежат [картам, движению и геометрии](../maps-and-movement.md); presentation details render targets, light textures, transparent eggs, map scrolling и hit testing принадлежат этой странице и [Frontend и рендеринг](../rendering/).

Интенсивность map light source задаётся percentage magnitude (`0..100`; отрицательные значения сохраняют magnitude, но включают constant/personal capacity semantics). `MapView` ограничивает текущий animated percentage, преобразует его во внутреннюю raw falloff scale (`0..10000`), а затем масштабирует RGB light map к диапазону света движка (`0..200`) и primitive alpha к `0..255` через percentage day-light capacity source. `SetDayColors()` обязан инвалидировать применённые light fans при изменении day color или percentage light capacity, поскольку оба значения входят в cached per-hex lighting.

`GetHexOffset(from, to)` равен `GetHexPos(to) - GetHexPos(from)`, поэтому при прокрутке view origin `RebuildMapOffset()` перемещает каждую light vertex на одинаковую pixel delta. `MapView` сдвигает cached `_lightPoints` на эту delta вместо перестроения каждого fan. Скрытие последнего видимого hex источника света по-прежнему заставляет перестроить primitives, чтобы убрать оставшиеся triangles, а вошедшие в view источники заново применяют fans и выполняют обычное перестроение. `Test_Geometry.cpp` закрепляет инвариант uniform translation.

Переиспользуемый API представления карты включает `SetExtraScrollOffset()` для transient camera offsets, которыми владеют скрипты. Движок применяет offset к map view, но game-specific screen effects вроде quake/shake timing и fade overlays принадлежат скриптам встраиваемого проекта.

## Ресурсы, sprites, effects и render targets

Клиентский resource path начинается с `FileSystem` из `GetClientResources()` и организован runtime managers:

- `ResourceManager` индексирует resource files, разрешает default sprites предметов, загружает и кеширует frames анимаций персонажей, обрабатывает Fallout-style mapping animation frames и предоставляет normalized mappings sound names. Extension precedence, identities effects и numbered variants описаны в [Audio.md](../../../Audio.md).
- `SpriteManager` владеет sprite factories, atlases, primitive drawing, draw ordering, scissor stack, размерами window/screen и render-target drawing.
- `DefaultSpriteFactory` загружает atlas sprites и sprite sheets из стандартных image/animation resources, включая optional per-frame silhouette mesh от `ImageBaker`. Resource decoder также заполняет payload `Sprite` общей записи `AnimationInfo`: frame count, duration, directions и разрешённые per-frame bounds. Этот payload доступен и в сборках без 3D. При startup `EngineMetadata` читает соответствующие компактные indexes version 1 `SpriteInfo/<PackName>.foinfo`, поэтому общие queries sprite metadata не декодируют pixel payloads. Source-backed sprite использует свой mesh для обычной full-image draw; явный empty mesh пропускает draw, а missing/quad mesh сохраняет four-vertex path runtime-generated atlas sprites.
- При `FO_ENABLE_3D` `ModelSpriteFactory` превращает model resources в atlas-backed sprites. Он получает из `EngineMetadata` уже разобранные aggregate version 2, idle-priority view и per-animation bounds из `ModelAnimationInfo.foinfo`; client model layer не открывает и не разбирает companion повторно, authored `.fo3d` `DrawSize` и `ViewSize` отсутствуют. Envelopes включённых body/movement animations проецируются через активный model transform для каждого facing direction, чтобы получить grid-aligned logical scratch frame, достаточный для тела и projected shadow. Отдельный view envelope (`Unarmed + Idle`, любой Idle, затем deterministic fallback) задаёт начальный body `ViewRect`, а aggregate `ModelBounds` задаёт horizontal-lighting frame. Layout math без live GPU использует `AppRender::MIN_ATLAS_SIZE / FRAME_SCALE` как portable logical limit (`MODEL_SPRITE_MAX_LOGICAL_FRAME_DIMENSION`), а не принимает atlas limit bake host за возможности игрового устройства. В runtime scratch texture ограничивается `Render.ModelSpriteMaxTextureWidth` / `Height` и atlas текущей машины; logical frame равен этому texture size, делённому на `FRAME_SCALE`. Pose или lighting envelope сверх limit рисуется с crop, в том числе когда authored scale и GUI preview `SetScale` вместе увеличивают допустимую модель. Invalid или non-finite bounds по-прежнему вызывают assert с шестью координатами и именем файла модели. Bounds runtime layers и child models расширяют и rectangle view/name, и aggregate horizontal-lighting frame; envelope сбрасывается при изменении mesh composition, а в остальных случаях только растёт. Names, coarse picking, transparent eggs, flying text и attachments остаются в автоматически выведенных bounds без authored sizes. Смена body/movement animation может обновить scratch frame, но обязана сохранить накопленный configuration view envelope, а не вернуться к idle-only view root model; иначе turn animation временно сдвинет name и anchor flying text у экипированного персонажа.

  Модель рендерится в переиспользуемый scratch target 2x для automatic frame. Per-animation prediction объединяется с baked AABB выбранных geometry links; для sizing runtime проецирует corners envelopes вместо skinning combined-mesh vertices. Если live particle bounds требуют больший scratch frame, factory перед expansion/rerender объединяет current и required placements как знаковые root-relative intervals. Это поглощает чередование соседних округлённых pivots и допускает tight frame, целиком лежащий по одну сторону от model root; bounded retry loop всё равно завершается ошибкой, а не принимает действительно unbounded или clipped frame. Offset cropped sprite сохраняет фиксированный model root, hit-test coordinates и стабильный horizontal lighting gradient. Настройка scratch frame не резервирует atlas space: allocation выполняется только после определения final crop. Изменённое placement готовится локально, рендерится и публикуется только после успешного atlas copy; failure запрашивает немедленный redraw и сохраняет прежнюю allocation. Atlas slot только расширяется, пока identity active animation/mesh/shadow envelope неизменна, а затем может однократно уменьшиться после завершения transition или изменения mesh composition. Emitting particle на model attachment добавляет обязательные bake-time bounds, преобразованные через текущий attachment, без per-frame live-AABB walk. При изменении frame уже emitted atlas-space particles rebased, поэтому expansion их не сдвигает; particles принудительно включают full-frame crop. Full-frame cropping не замораживает pivot: expansion по-прежнему выводит позицию model origin из полного geometry/particle envelope, а изменение только pivot запускает ещё одну sizing pose, чтобы дополнительное пространство появилось со стороны эффекта, а не накапливалось справа и снизу. Non-default model effect также отключает tight crop; effects, смещающие vertices за обычную skinned geometry, требуют отдельного conservative rendering contract, потому что bounds schema version 2 не кодирует shader displacement.
- `ParticleSpriteFactory` выполняет аналогичную работу для particle resources.
- `EffectManager` загружает default/minimal effects, разрешает выбранные скриптом effects и обновляет per-frame effect buffers.
- `FontManager` загружает fonts и форматирует или рисует текст, включая inline color tags.
- `RenderTargetManager` создаёт, изменяет размер, pushes, pops, читает, очищает, сохраняет и удаляет offscreen render targets.

Для 3D views персонажа idle refresh запускает animations alive-state с начала. Idle dead condition замораживается на последнем frame. Idle других non-alive conditions замораживается на первом frame, поэтому встраиваемый проект должен авторить его как требуемую resting pose состояния.

Эти managers работают с renderer, но не зависят от его реализации. Они обращаются через abstractions `IAppRender` / `Renderer`, поэтому одна клиентская логика работает с OpenGL, Direct3D или null renderer в зависимости от platform/build configuration.

`ParticleManager` и `ParticleSystem` - backend-neutral dispatch facades для sprites и model attachments. `ParticleRuntime.cpp` служит composition point: создаёт включённые реализации `ParticleRuntimeBackend`, а manager выбирает одну по extension ресурса. Каждая живая particle владеет ровно одним `ParticleRuntimeSystem`; общие timing, scale и render scheduling остаются в `ParticleSystem`, а simulation и backend-specific rendering являются virtual runtime operations.

Model attachments не владеют `ParticleSprite`, поэтому их simulation явно продвигается `ModelInstance::ProcessAnimation` после вычисления pose полной parent/child hierarchy. Этот порядок обязателен для effects на child joints: предыдущая pose child заставляет automatic frame sizing следовать за stale effect box и не сходиться. Particle получает logical frame delta модели; sizing с нулевой delta обновляет placement без второго продвижения emission. Prewarming откладывает reset model clock до следующего реального advance animation, не позволяя времени ожидания вне экрана превратиться в одно большое первое update, разрушающее распределение warmed particle age.

Resource invalidation следует той же neutral boundary: `SpriteManager -> ParticleSpriteFactory -> ParticleManager` уведомляет каждый backend через `ParticleRuntimeBackend::InvalidateResource()`, поскольку изменённый файл может быть dependency, а не root asset backend. SPARK удаляет matching parsed graph для изменённого `.spk` и очищает graph cache при изменении texture или render-effect dependency, поэтому следующая particle creation повторно загружает graph и dependency. Failed loads не кешируются. Backends без parsed-asset cache реализуют invalidation как no-op.

`VisualParticles.h` / `.cpp` не содержат names particle backends и feature guards. Concrete types и capabilities находятся в extension files. SPARK editor выполняет checked typed access только после перехода neutral boundary `GetRuntimeSystem()`, поэтому новый runtime не добавляет branch, enum value или vendor type в common facade.

Particle backends являются независимыми build features: `FO_SPARK_PARTICLES` и `FO_EFFEKSEER_PARTICLES` по умолчанию `OFF`. Встраиваемый проект может включить один или оба; particle sprite factory объявляет только extensions включённых backends, а отключённый backend не компилирует и не линкует upstream runtime.

Sources SPARK `.spark` запекаются в binaries `.spk`. `SparkParticleRuntimeBackend` принимает только `.spk` и загружает его через binary path `loadFromBuffer` SPARK; XML не попадает в runtime. Binary path должен быть behaviorally equivalent stream loader: truncated или oversized payloads, неизвестные object types, несовпадающие descriptor signatures, нулевые или out-of-range object references и ссылки на несовместимый object class инвалидируют graph. Регистрация custom FOnline SPARK objects общая для baker, editor и client через thread-safe path `EnsureSparkParticleObjectsRegistered()`. `SparkQuadRenderer::Setup()` разрешает effect и texture до возврата particle; missing render dependencies дают обычную load failure вместо позднего исключения при первой draw. Новый renderer в SPARK editor привязывается к owning runtime до инициализации preview graph; пока effect или texture не назначены, он ничего не рисует, позволяя завершить настройку без dereference incomplete backend state.

Baked raw `.efk` resources выбирают `EffekseerParticleRuntimeBackend` за тем же facade. Клиент никогда не загружает XML `.efkproj` и не запускает build-time compiler, включая Web: host pipeline обязан запечь `.efk` до packaging. Каждый `ClientEngine` или Mapper владеет Effekseer core manager; каждый `Create` разбирает новый effect, а общий particle sprite factory отдельно кеширует успешно загруженные atlas textures. Effekseer продвигает hierarchy, emission и lifetime state; custom renderer callbacks копируют evaluated render snapshot в FOnline-owned packets. Effekseer graphics backend не участвует. Начальный capability gate поддерживает только CPU Sprite/Ring и отклоняет unsupported renderer или material families до возврата particle system. Dynamic callback checks fail closed при non-finite evaluated data, invalid UV ranges или несовпадении atlas filter и удаляют уже созданный handle. Simulation update отделён от draw, поэтому lifetime effect не останавливается, когда direct-scene sprite вне rendered viewport; stopped handles продвигаются через deferred removal queues Effekseer до освобождения wrapper.

Исключение - явный direct-scene prewarm request: он остаётся pending до первого `DrawInScene`, предоставляющего текущий transform, а scheduled updates в это время приостановлены. Effekseer prewarm продвигает ровно одну секунду, затем синхронизирует update clock до возобновления обычной simulation, чтобы offscreen wait не учитывался дважды.

Sprite mesh geometry не зависит от pixel-exact hit mask. `FillAtlas` по-прежнему выводит hit testing напрямую из source alpha и `Render.SpriteHitValue`; contour simplification или dilation меняют только triangles, отправляемые на draw. Cropped regions, repeated patterns, fonts, render-target blits и padded custom-effect/contour draws намеренно создают quads, потому что их sampling rectangle не является silhouette исходного sprite.

### Шрифты и inline color tags

Полный authoring и runtime contract принадлежит [форматам шрифтов и компоновке текста](../../how-to/content/font-format.md); точные fields, enum values, limits и validation records приведены в [сгенерированном справочнике](../../reference/font-format/index.md). Здесь `FontManager` только помещён в общий жизненный цикл клиента.

`FontManager::FormatText()` удаляет tags `@color:0xBBGGRR@` / `@color:0xAABBGGRR@` и при draw formatting записывает разобранный `ucolor` в per-glyph color buffer formatted text. Reset tag `@color@` восстанавливает предыдущий inline color либо base draw color, если inline color не активен. `FontFlag::NoColorize` также удаляет tags, но продолжает рисовать caller-provided base color.

`Game.BindFont(font, path, defaultScale = 1.0)` может уменьшить привязанный font slot. Scale применяется один раз при bind: glyph bitmaps повторно rasterized на месте внутри atlas region font с area-average filter, а все metrics, включая advances, offsets, line height и space width, округляются до integers целевого размера. Runtime text pipeline (`Game.GetTextInfo(...)`, `Game.GetTextLines(...)`, `Game.DrawText(...)`) всегда работает в обычных integer pixel coordinates: scaled font ведёт себя как font, authored в меньшем размере, без fractional glyph positions. Scale должен быть в `(0..1]`; увеличение bitmap font отклоняется, для крупного текста создайте более крупный font asset.

## Ввод и hooks для скриптов

`ClientEngine::ProcessInputEvent()` принимает frontend values `InputEvent` и поднимает высокоуровневые script events:

- `OnMouseDown`, `OnMouseUp`, `OnMouseMove`;
- `OnTouchDown`, `OnTouchMove`, `OnTouchUp` для raw per-finger touch streams и `OnTouchTap`, `OnTouchDoubleTap`, `OnTouchScroll`, `OnTouchZoom` для aggregated gestures;
- `OnKeyDown`, `OnKeyUp`, `OnInputLost`;
- `OnScreenScroll` и `OnScreenSizeChanged`;
- события стадий map render `OnRenderMap_BeforeTiles`, `OnRenderMap_AfterSprites`, `OnRenderMap_AfterFlushMap`.

Input semantics задаются в `Source/Frontend/Application.h`; game-specific UI behavior должно оставаться в скриптах и GUI resources встраиваемого проекта.

**Typed text фильтруется здесь, а не во frontend-слое.** `ProcessInputEvent()` удаляет C0 control characters и `DEL` из `KeyDown.Text` до `OnKeyDown` и отбрасывает event `KeyCode::Text`, payload которого состоял только из control characters. Windows передаёт Alt+numpad как обычное OS text-input event, поэтому codes ниже `0x20` приходят как bare control characters; bitmap font рисует их CP437 pseudographics, и в chat или text fields появляются мусорные glyphs. Фильтрация по character, а не modifier Alt, сохраняет AltGr, обычный ввод и printable Alt codes (`Alt+0169`). Byte-wise filtering безопасен для UTF-8, поскольку каждый byte multibyte sequence не меньше `0x80`.

`ProcessInputEvent()` является правильным местом, поскольку через него проходят все input sources: SDL events из `ProcessInputEvents()`, scripted calls `Game.Simulate*` и automation bridge встраиваемого проекта. Filter во frontend/SDL-слое покрыл бы только OS path и был бы невидим simulated-input tests.

Клиентские скрипты могут синтезировать локальный ввод через тот же runtime path для automation и embedded-client probes. `Game.SimulateMouseMove(pos)`, `Game.SimulateMouseDown(pos, button)` и `Game.SimulateMouseUp(pos, button)` сохраняют held-button state на протяжении raw mouse gesture, включая позиции вне render window; `Game.SimulateMouseClick(pos, button)` отправляет полный mouse click или wheel event. `Game.SimulateTouchDown(fingerId, pos)`, `Game.SimulateTouchMove(fingerId, pos, offsetPos)` и `Game.SimulateTouchUp(fingerId, pos)` отправляют raw touch streams, `Game.SimulateTouchTap(pos)` - completed tap event, `Game.SimulateKeyPress(key, text)` - одну pair key down/up, а `Game.SimulateKeyboardPress(key1, key2, key1Text, key2Text)` остаётся доступным для two-key sequences.

Для локального prediction движения персонажа `ClientEngine::CritterMoveTo()` синхронизирует активный `MovingContext` с текущим client frame до начала нового движения или отправки stop request. Затем он нормализует локальную pair hex/offset до следующего request, поэтому быстрый start/stop input не сообщает серверу stale на один frame или чрезмерный offset.

## Тесты клиентской проверки

При изменении client runtime используйте минимальный подходящий scope тестов:

- `Source/Tests/Test_ClientEngine.cpp` - самодостаточный startup client engine, регистрация сущностей, инициализация script module и loop callbacks.
- `Source/Tests/Test_ClientServerIntegration.cpp` - client/server handshake через interthread transport и наблюдаемые client connection events.
- `Source/Tests/Test_ClientDataValidation.cpp` - проверка inbound remote-call payload для UTF-8, enums, floats, hashed strings, bools, truncation и ref-type payloads.
- `Source/Tests/Test_ClientRuntimeApi.cpp` - проверка ABI, exports и results client runtime для host/runtime split из [Client Updater](client-updater.md).

Точные имена test targets генерируются CMake/BuildTools-конфигурацией встраиваемого проекта; не фиксируйте имена targets одного проекта в документации движка.

## Checklist изменений

При изменении client runtime code проверьте:

- startup/shutdown `ClientEngine` не оставляет stale registered entities или render targets.
- Новые network messages описаны в [сети и авторитетности](../authority-and-networking/) и привязаны через `ClientConnection::AddMessageHandler()`.
- Новое synced state имеет ясную границу владения между Common entities/properties и client-only view state.
- Изменения map presentation не дублируют правила координат и поиска пути из [карт, движения и геометрии](../maps-and-movement.md).
- Изменения ресурсов указывают, какой owner затронут: `ResourceManager`, `SpriteManager`, конкретный sprite factory, `EffectManager` или `RenderTargetManager`.
- Platform-specific последствия rendering/input отражены в [Frontend и рендеринг](../rendering/), [сборке, упаковке и отладке в браузере](../../how-to/platforms/web-debugging.md) или [сборке, упаковке и отладке на Android](../../how-to/platforms/android-debugging.md).
