---
layout: default
title: Frontend и рендеринг
locale: ru
document_id: frontend-rendering
permalink: /Docs/ru/explanation/rendering/
---

<!-- docs-translation: {"document_id":"frontend-rendering","locale":"ru","source_path":"Docs/en/explanation/rendering/index.md","source_sha256":"48ccb6ee99b8d8504872039017c4b7e6417a78f460dfc43a40e12c097a30c291"} -->

# Frontend и рендеринг

Экспериментальный декодер Ogg/Theora, порядок полноэкранной отрисовки,
встроенное представление через `RenderIface`, загрузка текстур, поведение
соотношения сторон и граница визуальной приёмки описаны в разделе
[Видео](../../how-to/content/video.md). Эта страница отвечает за общую
инфраструктуру frontend и renderer, но не за проектную политику синематиков.

> Документация движка. Страница описывает переиспользуемые абстракции приложения, ввода, звука, окна и рендеринга из `Source/Frontend/`, а также клиентский мост render targets из `Source/Client/`.

## Статус контракта

Это основанное на исходном коде объяснение текущей архитектуры frontend и
рендеринга. Настройки проекта, сгенерированные script API, форматы ресурсов и
сериализованные metadata эффектов сохраняют уровни стабильности, заданные их
владеющими справочниками. Native-классы `Application`, `Renderer`, контексты
backend, распределители atlas и клиентские draw managers считаются деталями
реализации, если [индекс публичных контрактов](../../reference/public-contract/index.md) не говорит обратного.

Встраиваемый проект отвечает за выбранные backends, authored effects,
визуальный уровень качества, целевые разрешения, бюджеты производительности и
приёмку платформ. Движок отвечает за семантику выбора backend, контракты render
resources, соглашения о матрицах и depth и описанные здесь переиспользуемые
маршруты проверки.

## Назначение

Frontend-слой образует границу между платформой и runtime движка. Он владеет
окнами, границами кадров, очередями ввода, преобразованием touch/gamepad,
доступом к аудиоустройству, выбором renderer и низкоуровневыми объектами render
backend. Клиентский runtime использует этот слой через устойчивые интерфейсы и
не обращается напрямую к SDL, OpenGL, Direct3D или Web API.

Читайте эту страницу вместе со следующими документами:

- [Клиентский runtime](../runtime/client.md) описывает, как эти сервисы используют `ClientEngine`, `SpriteManager`, `MapView` и runtime managers.
- [Форматы изображений и спрайтов](../../how-to/content/image-format.md) описывают загрузку baked sprites, размещение в atlas, filter borders, hit masks и identity cache изображений.
- [Форматы шрифтов и layout текста](../../how-to/content/font-format.md) описывают bitmap fonts, размещение glyphs в atlas, масштаб при bind, layout текста и rendering flags.
- [Build Workflow](../../how-to/build/) и [BuildTools Pipeline](../../reference/cmake-and-buildtools/pipeline.md) описывают configure/build composition.
- [Сборка, упаковка и отладка в браузере](../../how-to/platforms/web-debugging.md) и [сборка, упаковка и отладка на Android](../../how-to/platforms/android-debugging.md) описывают package/debug flows платформ.
- [Нативная отладка и отладка AngelScript](../../troubleshooting/debugging.md) описывает native debugging и stack traces.

## Проверенные пути исходного кода

- `Source/Frontend/Application.h`
- `Source/Frontend/Application.cpp`
- `Source/Frontend/ApplicationInit.cpp`
- `Source/Frontend/ApplicationHeadless.cpp`
- `Source/Frontend/ApplicationStub.cpp`
- `Source/Frontend/Rendering.h`
- `Source/Frontend/Rendering.cpp`
- `Source/Frontend/Rendering-OpenGL.cpp`
- `Source/Frontend/Rendering-Direct3D.cpp`
- `Source/Frontend/Rendering-Vulkan.cpp`
- `Source/Frontend/Rendering-SDLGpu.cpp`
- `Source/Frontend/Rendering-Null.cpp`
- `Source/Common/Settings.inc`
- `Source/Client/RenderTarget.h`
- `Source/Client/RenderTarget.cpp`
- `Source/Client/SpriteManager.h`
- `Source/Client/SpriteManager.cpp`
- `Source/Client/DefaultSprites.h`
- `Source/Client/DefaultSprites.cpp`
- `Source/Client/TextureAtlas.h`
- `Source/Client/TextureAtlas.cpp`
- `Source/Client/ModelSprites.h`
- `Source/Client/ModelSprites.cpp`
- `Source/Client/ModelSpriteLayout.h`
- `Source/Client/ModelSpriteLayout.cpp`
- `Source/Common/AnimationInfo.h`
- `Source/Common/AnimationInfo.cpp`
- `Source/Common/ModelBounds.h`
- `Source/Common/ModelBounds.cpp`
- `Source/Common/Geometry.h`
- `Source/Common/Geometry.cpp`
- `Source/Client/EffectManager.h`
- `BuildTools/cmake/stages/Packages.cmake`
- `Source/Tests/Test_Rendering.cpp`
- `Source/Tests/Test_Geometry.cpp`
- `Source/Tests/Test_ModelBaker.cpp`
- `Source/Tests/Test_ImageBaker.cpp`
- `Source/Tests/Test_TextureAtlas.cpp`
- `Source/Tools/ImageBaker.cpp`
- `Source/Tools/SpriteMeshing.cpp`
- `Source/Tools/BakingReport.cpp`
- `BuildTools/cmake/stages/Init.cmake`

## Карта слоёв

Frontend/rendering разделён на три слоя:

1. **Application layer** (`Application`, `AppWindow`, `AppInput`, `AppAudio`, `AppRender`) владеет платформенными сервисами и границами кадров.
2. **Renderer layer** (`Renderer` и его backends) владеет GPU/null resources: текстурами, draw buffers, эффектами, матрицами, scissor state, presentation и обработкой resize.
3. **Client drawing layer** (`SpriteManager`, `RenderTargetManager`, `EffectManager`, `MapView`) строит операции движка и игры поверх renderer abstraction.

Благодаря этому основная часть клиентского кода не зависит от renderer. Клиент
запрашивает sprites, effects, draw buffers, render targets и input events, а
выбранный backend решает, как они реализуются.

Декодирование исходных изображений принадлежит baker, а не frontend. Во время
выполнения `DefaultSpriteFactory` превращает baked RGBA container в sprites в
atlas, `TextureAtlasManager` выделяет место по `AtlasType`, а renderer получает
только загрузки регионов текстуры и draw data. Полный путь описан в
[форматах изображений и спрайтов](../../how-to/content/image-format.md), а
движение отдельных кадров через offsets - в [Sprite Root Motion](../../how-to/content/sprite-root-motion.md).

Дескрипторы bitmap fonts являются raw runtime resources; связанные PNG/TGA всё
равно проходят обычный image baking. `FontManager` разбирает descriptor,
загружает обычные и bordered glyph regions в font atlas и отправляет текст через
`SpriteManager`. Backend renderers не интерпретируют FOFNT/BMFont syntax или
wrapping flags. Этой границей и точным поведением измерения/отрисовки владеет
[справочник шрифтов](../../how-to/content/font-format.md).

## Соглашение о матрицах

В render math движка действует одно соглашение:

- `mat44` — тип матрицы GLM из `Source/Common/Common.h`.
- Хранение column-major. Прямой индекс имеет вид `matrix[column][row]`, translation находится в `matrix[3].xyz`, а `glm::value_ptr(matrix)` можно копировать в uniform buffers без transpose.
- Алгебра использует column vectors: `clip = Proj * View * Model * vec4(position, 1.0)`.
- Shader code следует тому же правилу через `ProjMatrix * vec4(...)`. Различия backend должны находиться в constructors матриц и shader cross-compilation, а не в случайных transpose на call sites.

Имена `RowMajor`/`ColumnMajor` допустимы только на явной границе преобразования
с внешним форматом. Внутренние renderer, model, particle и geometry paths
называют матрицы по роли: `ProjMatrix`, `ViewMatrix`, `ViewProjMatrix`,
`WorldMatrix`.

## Инициализация приложения

`InitApp()` и `LoadAppSettings()` в `Source/Frontend/ApplicationInit.cpp`
готовят global settings и application services до создания engine object
клиентом, сервером или инструментом.

Основные обязанности:

- разобрать command line и локальную конфигурацию;
- загрузить app settings из config/cache sources;
- один раз инициализировать frontend globals;
- при необходимости вызвать поддержку baking через `FO_BakeResources`;
- подготовить app-level services для клиентов, инструментов и headless/test modes.

Эта инициализация намеренно общая не только для графического клиента. Server,
Mapper, Editor, tests и package flows используют разные flags и window modes,
но там, где это применимо, проходят через общий frontend setup.

## Сервисы приложения

Публичная frontend-поверхность определена в `Source/Frontend/Application.h`.

### `Application`

`Application` владеет frontend-состоянием уровня процесса:

- главным и дочерними окнами;
- выбором активного окна;
- границами кадра `BeginFrame()` / `EndFrame()`;
- границами рендеринга окна `BeginWindowRender()` / `EndWindowRender()`;
- открытием ссылок и пользовательскими message/progress/choice dialogs;
- регистрацией callback главного цикла;
- запросом выхода и ожиданием завершения;
- распознаванием touch gestures и обновлением состояния gamepad.

### `AppRender` / `IAppRender`

`AppRender` — принадлежащий приложению facade над выбранным `Renderer`.
Клиентский и инструментальный код использует его вместо downcast к конкретному
GPU backend. Контракт включает:

- создание ресурсов через `CreateTexture()`, `CreateDrawBuffer()` и `CreateEffect()`;
- доступ к projection через `CreateOrthoMatrix()` и `GetProjMatrix()`;
- выбор target/depth range через `SetRenderTarget()` и `SetOrthoDepthRange()`;
- очистку target через `ClearRenderTarget()`;
- clipping через `EnableScissor()` и `DisableScissor()`;
- ориентацию render target через `IsRenderTargetFlipped()`.

`AppWindow::GetRender()` возвращает facade, принадлежащий конкретному real или
virtual window. Операции должны оставаться внутри владеющего window/engine
instance: render resources и settings встроенного клиента не являются globals
хоста.

### `AppWindow` / `IAppWindow`

Обязанности окна:

- size, screen size, position, display rect, focus и fullscreen state;
- minimize, blink, always-on-top, title, input grabbing и destruction;
- различение реальных OS windows и **virtual windows** (`IsVirtual()`), которые multi-client host композитит сам и рендерит в собственном virtual size;
- получение native `WindowInternalHandle` для render backends;
- использование `HeadlessWindowStub` в headless/stub contexts.

### `AppInput` / `IAppInput`

Обязанности ввода:

- polling очереди `InputEvent`, очистка и добавление events;
- управление положением mouse;
- включение screen keyboard;
- clipboard text;
- gamepad state;
- нормализация mouse, keyboard, wheel, touch tap/double-tap/scroll/zoom.

При преобразовании platform mouse button в script-facing `MouseButton`
сохраняется конкретный id (`Left`, `Right`, `Middle`, `Ext0`/`Ext1`, ...).
Неизвестные native buttons игнорируются и не превращаются в primary click.
Низкоуровневые события переводятся в script events в
`ClientEngine::ProcessInputEvent()`.

Главный источник `InputEvent::MouseMoveEvent` — SDL mouse-motion. На backends,
где SDL даёт global mouse coordinates (Windows, macOS, X11 и разрешённые OS/2
drivers), `Application::BeginFrame()` также опрашивает global state, пока
приложение в focus. Если SDL motion event в кадре не было, но global position
изменилась, frontend синтезирует mouse-move. Поэтому cursor и edge scroll не
замирают, когда OS pointer выходит за окно. Тот же перевод host-to-active-window
преобразует координаты embedded virtual clients через display rect и
aspect-fit mapping.

### `AppAudio` / `IAppAudio`

Обязанности audio:

- сообщать, включён ли звук;
- устанавливать audio stream callback;
- преобразовывать audio formats;
- смешивать audio;
- lock/unlock устройства вокруг critical sections.

Поддерживаемые WAV/ACM/Ogg profiles, raw-copy delivery, поиск effects/music,
streaming, repeat, volume, return values и audible validation описаны в
[Audio](../../how-to/content/audio.md) и сгенерированном
[audio reference](../../reference/audio/index.md). Здесь описана platform
abstraction, а не правила authoring звука конкретной игры.

## Headless и stub modes

Для tools, tests, CI и platform staging важны два неграфических режима:

- `Source/Frontend/ApplicationHeadless.cpp` поддерживает работу без видимого client window.
- `Source/Frontend/ApplicationStub.cpp` предоставляет stub implementations render/input/audio/window interfaces.

Stub layer не является полноценным renderer. Он позволяет tests и
non-graphical flows проверять engine logic без GPU/window/audio device. Тест,
которому нужен видимый рендеринг, должен заявлять это явно и не полагаться на
stub behavior.

## Абстракция рендеринга

`Source/Frontend/Rendering.h` определяет renderer-facing types:

- `RenderType` — семейство backend.
- `EffectUsage` — категория эффекта при load/compile.
- `RenderPrimitiveType` — topology draw buffer.
- `BlendFuncType` и `BlendEquationType` — blend state из effect config.
- `DepthVariantType` и `EFFECT_DEPTH_VARIANTS` — slot варианта depth state отдельного draw.
- `Vertex2D` и `Vertex3D` — layouts вершин sprite/model paths.
- `RenderTexture` — texture/render-target resource backend.
- `RenderDrawBuffer` — vertex/index storage, загружаемый в backend.
- `RenderEffect` — shader/effect со standard uniform и script-value buffers.
- `Renderer` — интерфейс, реализованный конкретными backends.

Для primitive batches из `SpriteManager::DrawPoints` поля `PosX/PosY` —
локальные pixel coordinates относительно `draw_area` с вычтенным scroll,
а `TexU/TexV` содержат `PrimitivePoint::TexUV + draw_area.xy`. Для устойчивых
к миру fragment effects один и тот же constant `TexUV` задаётся всем вершинам
batch: это абсолютная, привязанная к map origin pixel position anchor-hex
`_screenRawHex`. Shader восстанавливает абсолютный world pixel как
`gl_FragCoord.xy + InTexCoord`. Константный varying не даёт barycentric rounding
двигать noise; пространственное изменение предоставляет `gl_FragCoord`. Схема
устойчива к camera scroll/zoom, deformation fan triangles и parity offset-row
hex grid. `MapView::LightFanToPrimitves` вычисляет значение через
`GeometryHelper::GetHexOffset(mpos(0, 0), _screenRawHex)`, а
`Primitive_Light.fofx` использует его для world-stable jitter края света.
Нормализованная radial distance хранится в `PrimitivePoint::PointPosZ` и через
`InPosition.z` позволяет `smoothstep` плавно поднять brightness от нуля на rim.

`Source/Frontend/Rendering.cpp` владеет backend-independent helpers, включая
проверки allocation draw buffers и parsing effect configuration. Он читает
sections `Effect`/`EffectInfo`, pass count, blend settings и script-visible
buffers до передачи shader files конкретному backend.

### Геометрия atlas спрайтов и моделей

`EffectUsage::QuadSprite` — историческое имя effect slot, а не ограничение на
четыре вершины. Sprite draw buffer является indexed triangle list; исходный
`AtlasSprite` может отправлять baked silhouette vertices/indices вместо
неявного rectangle из 4 vertices/6 indices. Все backends потребляют один и тот
же buffer, отдельного backend-specific polygon path нет.

Local mesh coordinates отсчитываются от точного bounding box выбранной
геометрии. Baked frame хранит исходный logical bitmap size и origin crop внутри
него, а individual sprite offset сохраняет исходный logical root. Screen
position и atlas UV являются affine mappings одной cropped coordinate, поэтому
scale, rotation, map projection, depth standing sprite и egg flags продолжают
работать для каждой вершины без пустых texture rows/columns. Map lighting
сохраняет плоскость полного bitmap quad: `DrawSprites` передаёт left/right
colors, а mesh vertex получает
`lerp(left, right, clamp((localX + sourceOffsetX) / sourceWidth, 0, 1))`.
Это положение по горизонтали в исходном bitmap, а не в crop или opaque contour;
из-за RGBA8 округление промежуточных mesh colors допускает отличие не более
одной единицы channel.

Baked polygon mesh используется только обычной отрисовкой полного изображения.
У каждой allocation остаётся корректный rectangle `GetAtlasRect()`, который
может быть меньше исходного logical image и смещён внутри него. Region crops,
tiled patterns, padded custom effects/outlines и mapper previews проецируют
physical rectangle обратно в logical coordinates через `SourceOffset`.
Region UV нормализованы к исходному image; transparent cropped margins
вырезаются из destination. Поэтому polygon crop не сдвигает и не растягивает
GUI 9-slice, repeated pattern, preview или source-region composition. Эффект,
создающий pixels за пределами silhouette, обязан использовать padded/quad path.

Runtime model sprite тоже может использовать cropped quad, но его logical
layout вычисляется автоматически. `.fo3d` больше не принимает `DrawSize` и
`ViewSize`; default render settings также не задают fallback dimensions.
`ModelAnimationInfo.foinfo` bounds schema v2 содержит aggregate root-space model
AABB, отдельный idle-priority view AABB и AABB каждой animation. В сборках
`FO_ENABLE_3D` общий loader `EngineMetadata` один раз разбирает и проверяет весь
companion; renderer получает immutable model records из registry вместо второго
client config parser/cache bounds.

Клиент проецирует enabled animation bounds через active base transform и
вычисляет extrema для любого continuous facing angle. Body и projected shadow
задают animation-wide `DrawRect` и dimensions active logical scratch frame.
View bound выбирает `Unarmed + Idle`, затем любую Idle, затем deterministic
animation/static fallback; проекция по всем направлениям даёт устойчивый
`ViewRect`. Logical frame — tight extent текущей animation, выровненный вверх к
sprite frame scale, но не к power of two. Ground root находится в точном
projected pixel `(-DrawRect.x, -DrawRect.y)`, доступном через
`ModelInstance::GetFramePivot()`, а не в фиксированной доле frame. Поэтому
low/center-origin creature не резервирует пустую высоту. `ViewRect` исключает
shadow и не зависит от меняющегося atlas crop, чтобы name, coarse picking,
transparent eggs и flying text не дёргались при turn/animation.

View rectangle начинается с baked idle-priority bound. Live weighted pose не
участвует: накопление per-frame vertices растянуло бы name rectangle по всему
root motion clip. Selected child models учитываются через baked link envelopes.
`SelectModelViewBounds` заменяет idle view полным текущим
animation-plus-link envelope только тогда, когда его projected top ниже, поэтому
prone/corpse configuration опускает name, а поднятое оружие не поднимает его.
Presentation policy проекта по-прежнему может задаваться authored `NameOffset`.

`SelectModelViewBounds` позволяет view целиком следовать **вниз** за active
animation: если после тех же base transform/projection baked box clip имеет
более низкий top, берётся весь box clip. Сравнение выполняется в projected
space, что важно для imports с `RotX = +/-90`. Corpse/prone body иначе оставил бы
name на standing height; при этом более широкий lying pose требует полного box.
Повышенный weapon/overhead swing игнорируется, поэтому name не поднимается во
время удара. Оба input baked per clip, и результат не дрейфует внутри animation.

Automatic logical frame владеет переиспользуемым scratch render target 2x.
Клиент объединяет baked active-animation bounds с AABB каждого выбранного
non-particle child link, затем проецирует восемь corners envelope по facing
sweep. Это удерживает layer/equipment geometry в facing-independent fixed frame
без per-frame weighted-vertex walk. Envelope расширяют только emitting particle
systems; dormant effect ничего не резервирует и попадает в bounded expansion
pass, если начинает emission.

Измерение содержит два rectangle. `ModelSpriteBounds::Rect` — полный baked
geometry envelope вместе с shadow, live particles и full frame, принудительно
выбранным effect; он задаёт frame и atlas crop.
`ModelSpriteBounds::PoseRect` захватывается до particles и возвращается через
`ModelSprite::GetPoseRect` / `Game.GetDrawCritter3dBounds` для fit-to-area. Уже
emitted world-space particles не масштабируются вместе с model, поэтому их
включение в fit создало бы feedback loop. Effects входят во frame, но не в fit.

Если exact envelope больше, frame расширяется и
перерисовывается. Последовательные frame placements объединяются как
root-relative intervals, поэтому соседние округлённые pivots или живая
world-space particle не заставляют стабильный frame бесконечно чередоваться.
Interval anchor знаковый: tight frame может целиком находиться по одну сторону
от model root, оставляя pivot за пределами frame; bounded retry loop всё равно
отклоняет действительно unbounded layout.

Layout helpers без live GPU используют
`AppRender::MIN_ATLAS_SIZE / FRAME_SCALE`
(`MODEL_SPRITE_MAX_LOGICAL_FRAME_DIMENSION`) как portable logical limit; atlas
limit bake host не является контрактом игрового устройства. Scratch texture
model sprite ограничивается `Render.ModelSpriteMaxTextureWidth` / `Height` и
atlas текущей машины. Envelope сверх limit рисуется с crop, а bounded retry loop
по-прежнему отклоняет layout, который не сходится внутри limit.
`ModelInstance::SetupFrame` также отклоняет logical
`draw_size * FRAME_SCALE` выше `AppRender::MAX_ATLAS_WIDTH` /
`MAX_ATLAS_HEIGHT` текущей машины, называя model и оба размера до анонимного
device texture-allocation failure. Meshes, отключённые собственным default link
модели в `.fo3d`, не входят ни в drawn, ни в pose bounds; runtime учитывает
default `DisableMesh` так же, как layer/attachment, удерживая geometry внутри
baked layout budget.

В atlas
выделяется и копируется только selected region, а crop origin отражается в
sprite offset, сохраняя root, hit test и map position.

Active layer/child tree расширяет idle-priority base view и aggregate lighting
bounds. Animation switch может обновить `DrawRect`/scratch frame, но `ViewRect`
использует накопленный view envelope model+layers и не заменяется временно
меньшим root-model idle view. Left/right map-light colors выбираются в
root-relative crop endpoints configuration envelope, поэтому изменение scratch
size не меняет light mix, а wide gear не clamp-ится к base-model endpoints.

Внутри одного active animation/combined-mesh envelope следующие pose changes
только расширяют slot. Identity envelope меняется после стабилизации enabled
body/movement tracks, изменения generated mesh composition или shadow coverage;
это разрешает одно shrink к новому stable envelope вместо накопления всех
animation за lifetime. Direction в identity не входит. Новая allocation
резервируется и копируется до публикации frame/crop; неудача оставляет старую
allocation live и планирует retry.

Model-attached particles после первого update используют live SPARK AABB, до
него — advertised canvas, и выбирают весь current frame. При изменении scratch
frame уже emitted atlas-space particles rebased до rerender. Non-default model
effects отключают tight crop. Это не bound для shader displacement: эффект,
сдвигающий vertices за обычную geometry, требует отдельного conservative
контракта. При `Render.ModelDirectDraw` atlas preview/hit-test сохраняются, а
видимая geometry рисуется напрямую в scene.

`Game.DumpAtlases()` и команда Mapper **Dump atlases** аннотируют read-back TGA,
не меняя runtime texture: magenta — triangle edges, cyan — mesh vertices,
yellow rectangle — implicit quad, red X — explicitly empty baked frame.
`AtlasSprite` владеет mesh metadata, а live allocation хранит nullable
non-owning observer и очищает его при release, поэтому reused slot не показывает
stale geometry.

`AtlasSprite` отдельно хранит authored logical size/offset и cropped allocation.
Mesh vertices размещаются через `SourceOffset` в logical canvas, UV локальны к
crop. Поэтому `GetSize()`, `GetOffset()`, scale и hit testing сохраняют исходный
контракт даже после удаления transparent borders: экономия texture memory не
видна GUI layout, anchors или input routing.

Runtime allocation остаётся per image, но `TextureAtlasLayout` использует
dynamic MaxRects вместо order-sensitive guillotine tree. Он хранит overlapping
maximal free rectangles, выбирает best short-side fit, затем long-side fit и
wasted area без rotation. Manager сравнивает fit во всех существующих atlases
данного type и только затем создаёт page; при equal score выбирается старый
atlas. Packed rectangle уже включает one-pixel texture border, поэтому padding,
sprite pixels и UV не меняются.

Font sheets, model material textures, particle maps и Spine attachment textures
— rectangular consumers: authored glyph/normalized UV адресуют весь source
bitmap, а consumer получает только atlas rect без `SourceOffset`. Они загружаются
через `SpriteManager::LoadSpriteAsQuad`, который использует baked mesh metadata
только для восстановления исходного logical canvas перед upload. Обычный
`AtlasSprite` раскрыл бы crop dimensions и сдвинул UV. Runtime-generated model и
particle sprites уже занимают rectangular allocations и восстановления не
требуют.

Каждый live sprite владеет engine `unique_del_*` handle на encapsulated,
stable-address `TextureAtlasLayout::Allocation`. Release очищает mesh observer
и за constant time возвращает rectangle прямо в free list. Список не coalesce-ится
сразу: после placement miss `DefragmentFreeRectangles` перестраивает точный
maximal set из live allocations до создания новой page. Prune содержащихся
rectangles запускается только после роста значительно выше результата прошлого
prune и индексирует keepers по coarse atlas cells, не выполняя full scan на hot
path. Surviving sprites, pixels и UV не двигаются; settings и serialization не
меняются.

`Render.DrawWireframe` включает backend-independent geometry overlay.
`SpriteManager` копирует реально отправленные triangle edges после position,
scale, rotation, map projection и standing depth, затем рисует opaque magenta
line list поверх normal pass. Видны и два triangles обычного quad, поэтому
toggle не зависит от `SpriteMesh.Enabled`. По умолчанию он выключен и не меняет
draw buffer, atlas или baked resource.

## Render backends

Компилируемый набор `RenderType` и способы выбора:

| `RenderType` | Выбор | Текущий контракт |
| --- | --- | --- |
| `Null` | `Render.NullRenderer` или headless/stub path | Реализованный CPU-only backend проверки без видимого GPU output. |
| `OpenGL` | `Render.ForceOpenGL` или последний automatic GPU choice | Реализованный native OpenGL/OpenGL ES/WebGL; render targets перевёрнуты. |
| `Direct3D` | `Render.ForceDirect3D` или первый automatic Windows choice | Реализованный Direct3D 11; render targets не перевёрнуты. |
| `Metal` | `Render.ForceMetal` | Прямой Metal — placeholder: enum/platform flag существуют, но force приводит к `NotImplementedException`, класса `Metal_Renderer` нет. |
| `Vulkan` | `Render.ForceVulkan` или automatic choice перед OpenGL, если более ранний backend не создан | Реализованный dynamically loaded Vulkan; render targets не перевёрнуты. |
| `SDLGpu` | `Render.ForceSDLGpu`, при необходимости `Render.SDLGpuDriver` | Реализованный явный SDL_GPU поверх Vulkan/Metal/D3D12; render targets не перевёрнуты. |

`Application` сначала учитывает force selectors, затем compiled automatic order.
Это не health-probing fallback chain: после создания backend ошибка init
возвращается наружу, и другой backend не пробуется. Force selectors должны быть
взаимоисключающими. `Render.ForceSDLGpu` не становится automatic default.
Прямой Metal недоступен; реализованный Apple Metal route проходит через
`Render.ForceSDLGpu = True` и `Render.SDLGpuDriver = metal`.

Vulkan и SDL_GPU включаются по умолчанию в non-headless/non-Web builds и
отключаются `FO_DISABLE_VULKAN` / `FO_DISABLE_SDL_GPU`. Сборка использует
vendored SDL headers/drivers, а не внешний Vulkan SDK. OpenGL, Direct3D и
platform flags задаёт platform branch в `BuildTools/cmake/stages/Init.cmake`.

### Null renderer

`Source/Frontend/Rendering-Null.cpp` реализует `Null_Renderer`, `Null_Texture`,
`Null_DrawBuffer` и `Null_Effect`. Backend нужен tests/headless flows без GPU,
но всё равно проверяет dimensions, buffer counts, render-target state и доступ
к texture regions, поэтому ловит многие нарушения API.

### OpenGL renderer

`Source/Frontend/Rendering-OpenGL.cpp` реализует OpenGL/WebGL path:

- создаёт SDL/OpenGL или WebGL context;
- загружает и проверяет GL entry points/extensions;
- ограничивает atlas через `AppRender::MAX_ATLAS_SIZE` и backend limits;
- создаёт textures, draw buffers и effects;
- загружает vertex/fragment shader через effect loader;
- сообщает `IsRenderTargetFlipped() == true`;
- укладывает требуемые shader uniform blocks в один bump-allocated UBO, zero-initializes отсутствующие blocks, делает один `glBufferSubData` и связывает ranges через `glBindBufferRange`;
- пропускает redundant `SetRenderTarget` для уже применённого target; cache invalidates при resize/destruction, а texture creation восстанавливает текущий framebuffer.

OpenGL — основной путь WebAssembly/WebGL; изменения проверяются по
[сборкой, упаковкой и отладкой в браузере](../../how-to/platforms/web-debugging.md).

### Direct3D renderer

`Source/Frontend/Rendering-Direct3D.cpp` реализует Direct3D 11:

- создаёт D3D device, swap chain и render-target resources;
- не фиксирует refresh rate windowed swap chain, оставляя выбор desktop compositor;
- создаёт textures/staging textures, draw buffers, constant buffers и effects;
- загружает vertex/pixel shaders через effect loader;
- при resize пересоздаёт backbuffer/depth resources;
- сообщает `IsRenderTargetFlipped() == false`.

Изменения Direct3D проверяются в Windows build/debug flow встраиваемого проекта.

### Placeholder прямого Metal

`FO_HAVE_METAL`, `RenderType::Metal`, `Render.ForceMetal` и SDL Metal window flag
присутствуют, но direct `Metal_Renderer` не реализован. Поэтому
`Render.ForceMetal = True` немедленно вызывает `NotImplementedException`; такую
конфигурацию нельзя рекламировать или считать release evidence.

Metal driver SDL_GPU — другой, реализованный backend. Используйте
`Render.ForceSDLGpu = True` и `Render.SDLGpuDriver = metal` и применяйте те же
visible/shader/interaction/performance gates, что к Vulkan и D3D12 drivers.

### Vulkan renderer

`Source/Frontend/Rendering-Vulkan.cpp` реализует Vulkan. Он включён по умолчанию
(кроме `FO_DISABLE_VULKAN`, headless-only и Web) и не требует внешнего Vulkan
SDK: headers приходят из vendored SDL3
(`ThirdParty/SDL/src/video/khronos`), loader разрешается динамически.
`Render.ForceVulkan` выбирает его явно; иначе он может стать automatic choice,
если более ранний backend не настроен.

`vulkan-1.lib` не линкуется. Файл компилируется с `VK_NO_PROTOTYPES`, а
`SDL_Vulkan_LoadLibrary` и `SDL_Vulkan_GetVkGetInstanceProcAddr` загружают
entry points через X-macro table. Поэтому binary не имеет load-time import
`vulkan-1.dll` и запускается без Vulkan runtime, пока backend не выбран; при
выборе отсутствие runtime даёт ошибку `SDL_Vulkan_LoadLibrary`.

Основные свойства:

- **Одна queue, два frames in flight.** `VULKAN_FRAMES_IN_FLIGHT = 2`; каждый slot содержит command buffer, fence, acquire semaphore, descriptor pool, mapped uniform bump buffer, staging ring и deferred-destroy queue. `BeginFrame()` ждёт fence своего slot, очищает deferred destroys, resets pools, acquires swapchain image, clears и начинает render pass. `EndFrame()` submits и presents. Render-complete semaphores принадлежат swapchain images, acquire semaphores — slots. Fence гарантирует завершение всех более ранних submissions этой queue.
- **Deferred destroy per slot.** `Destroy*Safe(...)` добавляют handles в current slot; queue очищается после ожидания его fence. Разные helper names нужны из-за integer handles на 32-bit. Всегда используется `VK_NULL_HANDLE`. Swapchain recreation делает `vkDeviceWaitIdle`, очищает все queues и перестраивает synchronization.
- **Uploads записываются в frame command buffer, readback flush-ит его.** `UpdateTextureRegion` временно завершает render pass и пишет barrier/copy/barrier в тот же buffer, сохраняя immediate-mode order: clear atlas до upload выполняется именно первым. Pixels идут через pooled staging ring. `GetTextureRegion` вызывает `FlushFrameCommandBufferMidFrame()`, submit/wait и затем immediate staging copy. Upload вне recording frame также использует immediate path.
- **Dynamic geometry использует per-draw-buffer/per-slot ring pools.** Каждый `DrawBuffer::Upload` берёт следующий persistently mapped HOST_VISIBLE buffer текущего slot. Ring resets при первом acquire нового frame после fence. Capacity grows только при необходимости; steady state — memcpy без create/allocate/free на каждый draw. Static buffers используют one-off staging в device-local memory.
- **Shaders baked с `highp`.** `mediump` превращается в SPIR-V `RelaxedPrecision`, который Vulkan drivers могут выполнить как FP16; большие time/world values переполняются и дают black output. Effect baker поэтому выдаёт `precision highp float`.
- **Backbuffer metrics обновляются при resize без обязательного `SetRenderTarget`.** `ApplySwapchainTargetMetrics()` вызывается при выборе backbuffer, `OnResizeWindow()` и deferred recreation. Это важно для host ImGui, который рисует прямо в swapchain.
- **Два fixed descriptor set layouts.** Set 0 — uniform buffers, set 1 — combined image samplers. На draw backend выделяет sets из per-frame pool и пишет uniforms в один host-visible bump buffer; binding берётся из reflected `EffectInfo`.
- **Контракт `.fofx`.** Uniform blocks объявляются как `layout(set = 0, binding = N, std140)`, samplers — `layout(set = 1, binding = N)`. Пропущенный `set` помещает всё в set 0, создаёт descriptor mismatch/never-updated validation errors и может привести к device loss. OpenGL/Direct3D имеют отдельные namespaces и такой дефект там не проявляют.
- **Полнота uniforms.** Descriptor set обязан записать каждый используемый block, поэтому required-but-unset standard buffers zero-initialized перед upload.
- **Surface format.** Swapchain использует `VK_FORMAT_B8G8R8A8_UNORM` / `SRGB_NONLINEAR`, проверенный через surface formats. Texture render targets используют тот же color format для совместимости render pass; CPU upload/readback меняет R/B.
- **Present mode следует `Render.VSync`.** VSync использует FIFO; при выключенном VSync предпочтительны IMMEDIATE, затем MAILBOX. Выбор логируется при creation/recreation.
- **Orientation.** `IsRenderTargetFlipped() == false`, ortho использует Y-up как другие backends; Vulkan Y-down компенсируется negative-height viewport Vulkan 1.1. Матрицы остаются одинаковыми, что важно для CPU-side model camera math и front-face winding.
- **Point primitives.** Из-за ограничений SPIRV-Cross `POINT_LIST` отображается в `TRIANGLE_LIST`; текущий content point rendering не использует.
- **Physical device.** Предпочитается discrete GPU с graphics+present queue family и swapchain extension.
- **Validation.** При `Render.RenderDebug` или debug build доступный `VK_LAYER_KHRONOS_validation` пишет сообщения `[VkLayer/...]` через `VK_EXT_debug_utils`.

Проверяйте Vulkan на машине с рабочим runtime и, для validation output, Khronos
validation layer. Запускайте visible client с
`Render.ForceVulkan=True Render.RenderDebug=True`; в log должно быть ноль
`[VkLayer/...]` errors. Vulkan SDK не является build prerequisite движка.

### SDL_GPU renderer

`Source/Frontend/Rendering-SDLGpu.cpp` реализует opt-in backend SDL3 `SDL_GPU`,
который достигает Vulkan/Metal/D3D12 через vendored drivers. Он включён по
умолчанию (`FO_HAVE_SDL_GPU`), кроме headless-only/Web и
`FO_DISABLE_SDL_GPU`, но выбирается только `Render.ForceSDLGpu`.
`Render.SDLGpuDriver` может зафиксировать `vulkan`, `metal` или `direct3d12`, а
`Render.RenderDebug` включает debug mode.

Основные свойства:

- **Immediate-mode поверх explicit passes.** `Context` держит не более одного open render/copy pass; passes открываются lazy, clear хранится до следующего load-op, uploads идут через cycled transfer buffers, readback submits и ждёт fence.
- **Backbuffer proxy.** Рендеринг идёт в RGBA8 proxy, затем `Present()` blit-ит его в swapchain. Это сохраняет uniform color formats и безопасные mid-frame flushes.
- **Pipeline cache per effect.** Immutable pipelines keyed по pass, topology, наличию depth, `DisableBlending`, `DisableCulling`.
- **Отдельные SDL shader flavors.** SDL_GPU требует per-stage descriptor sets (vertex sampler/UBO sets 0/1, fragment sets 2/3), поэтому baker выдаёт `-spv_sdl`, remapped `-msl_*` и `[EffectInfoSdl]`. Native Vulkan `-spv` не меняется.
- **Push uniforms.** `SDL_PushGPU{Vertex,Fragment}UniformData` отправляет до четырёх slots per stage на каждый draw; baker проверяет limit. Public optionals effect сохраняют persistent semantics других backends.
- **`ProjBuf`/`MainTexBuf` caller-owned при наличии.** Backend auto-fills их только при `_needX && !X.has_value()`, затем resets лишь эти два. Это критично для 3D: `ModelInstance` передаёт model projection; без проверки skinned mesh оказался бы вне atlas. Остальные externally fed buffers также не перезаписываются и сохраняются между draws.
- **Общие black-map fixes.** `highp` SPIR-V и wrapped shader time исключают FP16 overflow и `sin(large time)` NaN.
- **Topology/orientation/depth.** `POINT_LIST` становится `TRIANGLE_LIST`, `IsRenderTargetFlipped() == false`, ortho использует depth `[0,1]`, depth texture — `D24_UNORM` или `D32_FLOAT`. Max atlas size 4096 из-за отсутствия query в SDL_GPU.

Запускайте client scene с `Render.ForceSDLGpu=True Render.RenderDebug=True`,
проверяя видимые map и GUI без validation errors рядом с default backend. Для
driver-specific claim явно задавайте `Render.SDLGpuDriver`.

## Render targets и клиентский мост

`Source/Client/RenderTarget.h/.cpp` связывают high-level drawing code с backend
textures. `RenderTargetManager`:

- создаёт render targets с optional depth и linear filtering;
- выделяет `RenderTexture` через `IAppRender::CreateTexture()`;
- сохраняет и восстанавливает предыдущий backend target при allocation/clear;
- поддерживает stack `PushRenderTarget()` / `PopRenderTarget()`;
- очищает current target;
- изменяет размеры targets;
- читает pixels с небольшим cache последнего pixel pick;
- удаляет targets, очищает stack и умеет dump textures для диагностики.

`MapView`, `SpriteManager`, `ModelSpriteFactory` и `ParticleSpriteFactory`
используют targets для map layers, light buffers, model/particle atlas render,
hit testing и offscreen composition.

При загрузке локальной карты `View.MapRenderTargetScale` фиксирует размеры
render targets карты, освещения и indoor mask как логический размер экрана,
умноженный на этот коэффициент. Движок ограничивает размер пределом текстуры
renderer; вид, превышающий получившийся target, разбивается на несколько chunks.

`Gui::CheckHit` кеширует boolean result для текущих `Game.FrameTime` и query
position, поскольку cursor drawing, zoom и movement могут проверять одну точку
несколько раз за frame, тогда как `FindHit` обходит все screen trees. Изменения
GUI activation, geometry, ordering, scroll, crop, изображения для transparent
hit и hittability инвалидируют cache. Refresh resolution и language проходит
через `_RefreshPositionRecursive`; setters с неизменившимся значением не
инвалидируют cache. Внутренний `_Move` намеренно не инвалидирует сам по себе,
поскольку `Draw` использует временную пару `_Move` каждый frame, поэтому direct
callers, меняющие persistent layout, инвалидируют явно.

Model-attached SPARK systems сохраняют уже spawned particles в simulation space,
пока emitter следует attachment point модели. Non-identity root transform
particle resource выбирает position+facing вместо полного bone matrix, чтобы
старые particles оставались world-stable, а новые появлялись в текущей точке.
Model movement offset вычитается в particle model space до camera
rotation/projection; setup-time positive и draw-time negative offsets
сокращаются для newly emitted particles.

## Размер экрана, разрешение и letterboxing

Рендеринг использует два разных размера:

- **Logical screen size** — `Settings.View.ScreenWidth/ScreenHeight`. В этой системе координат игра рисует `_rtMain`, projection и GUI/ImGui.
- **Backbuffer size** — реальный output surface: pixels OS window, monitor fullscreen или virtual render texture embedded client.

Игра всегда рисует `_rtMain` в logical size. Финальный blit при
`Renderer::SetRenderTarget(nullptr)` растягивает его в backbuffer **с сохранением
aspect ratio**, центрируя и добавляя bars только при разных aspect. При равных
размерах это 1:1. `_rtMain` изменяется по `GetScreenSize()` на
screen-size-changed event. Events семантически разделены:
`OnScreenSizeChanged` — logical size, `OnWindowSizeChanged` — physical/host size.

Script offscreen surfaces (`Game.ActivateOffscreenSurface` /
`Game.PresentOffscreenSurface`) также работают в logical coordinates, потому
что scripts рисуют их при active `_rtMain`. Pooled targets создаются по
`SpriteManager::GetScreenSize()` и resize-ятся до reuse; иначе GUI effects могут
обрезать content после увеличения resolution. Active scissor stack применяется
при flush и к offscreen surfaces, поэтому cropped GUI subtree сохраняет viewport
boundary внутри эффекта.

### Windowed

Window pixels и logical size поддерживаются равными. Событие
`SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` вне fullscreen записывает
`Settings.ScreenWidth/Height`, вызывает `OnWindowSizeChanged`, а
`OnScreenSizeChanged` — только при реальном изменении settings.
`Game.SetResolution(w, h)` сначала меняет logical size, затем OS window, если
клиент не fullscreen и не virtual; последующее OS event не вызывает повторный
GUI/map refresh того же logical resolution.

### Fullscreen (borderless desktop)

`SDL_SetWindowFullscreenMode(window, nullptr)` держит framebuffer равным monitor
size. Fullscreen resolution означает **logical render size**:
`Game.SetResolution` меняет logical size, а blit aspect-fits его в monitor.
Startup/toggle/pixel-size events fullscreen обновляют renderer/backbuffer, но не
перезаписывают `Settings.ScreenWidth/Height` и не вызывают
`OnScreenSizeChanged`, иначе выбранное logical resolution схлопнулось бы к
monitor size. `AppWindow::ToggleFullscreen()` помечает transition до SDL call,
потому что queued event может прийти при старых flags. Bars ожидаемы только при
разном aspect.

`SDL_SetWindowSize` не действует в fullscreen/maximized. Поэтому native client
сохраняет выбранный size как pending windowed size. При выходе из fullscreen
`SpriteManager::ToggleFullscreen()` применяет его и заново центрирует окно с
учётом accumulated resolution delta.

### Embedded clients в multi-client host (virtual windows)

`ServerApp` может размещать несколько embedded clients (`Single`/`Tile`/
`Cascade`, `Spawn Client`). Каждый имеет собственный engine instance,
`GlobalSettings` и virtual `AppWindow`:

- physical virtual size (`_virtualSize`, `GetSize()`) отделён от logical resolution (`_virtualScreenSize`, `GetScreenSize()`);
- game render идёт в `_rtMain` logical size, затем aspect-fit в `_virtualRenderTex` physical size;
- host aspect-fit/center-ит virtual texture в display rect и обратным тем же mapping переводит input, поэтому black bars не искажают local mouse coordinates.

Resolution change обновляет settings **владеющего engine**, не host. Virtual
`SetScreenSize` хранит logical value в `_virtualScreenSize`, а
`SpriteManager::SetScreenSize` отражает его в собственные
`Settings.ScreenWidth/Height` до handlers. `SetResolution` не вызывает
`SetWindowSize` для virtual window и не меняет `_virtualSize`; host layout и
render texture не resize-ятся. В standalone один engine, поэтому settings
engine и `App->Settings` совпадают.

GUI re-center выполняется через `OnScreenSizeChanged` →
`Gui::Callback_OnResolutionChanged()`, заново layout-ящий screens по
`Settings.View.ScreenWidth/Height`; `Anchor: None` центрируется по parent/screen.
Таким образом, `_rtMain` задаёт pixels, а settings — layout coordinates.

Local-map viewport при фактическом logical size change сразу recenter-ится на
chosen critter. `MapView` берёт logical client size, а не physical backbuffer;
fullscreen scaling остаётся финальному blit.

## Effects и shader data

Syntax `.fofx`, pass/render state, vertex inputs, resources, descriptors, baked
artifacts, cache identity, lifetime script values и authoring validation
принадлежат [Effect Format](../../how-to/content/effect-format.md) и
[сгенерированному reference](../../reference/effect-format/index.md). Эта
страница описывает участие effects в frontend/render pipeline.

`RenderEffect` владеет standard buffers:

- projection/main texture;
- transparent egg;
- sprite border;
- time/random/script values;
- camera/model/model-texture/model-animation.

`EffectManager` загружает minimal/default effects, выбирает script effects,
пишет script-value buffers и обновляет их per frame. Scripts задают один float
через `Game.SetEffectScriptValue(...)` или range через
`Game.SetEffectScriptValues(effectType, effectSubtype, valueStartIndex, values,
valuesOffset = 0, valuesCount = -1)`. Оба API проверяют effect, наличие
`ScriptValueBuf` и range `EFFECT_SCRIPT_VALUES`.

**Shader time session-relative и wrapped.** `TimeBuf` (`FrameTime.x` /
`GameTime.x`) rebased к первому rendered frame и wrapped по 8192 s в
`EffectManager::PerFrameEffectUpdate`. Это animation phase, не absolute clock.
Большой steady-clock/session time ухудшает fp32 `fract`/hash/`sin`, вызывает
stepping и затем теряет frame delta. Exact wrap сохраняет granularity менее 1 ms
для любой сессии ценой phase pop примерно раз в 2,3 часа; используйте его для
noise/ambient math. Hash lattice дополнительно wrap-ится локально; script-side
accumulators через `ScriptValueBuf` требуют того же подхода.

При изменении effect определите владельца:

- config parsing — `Rendering.cpp`;
- shader loading/draw — соответствующий OpenGL, Direct3D, Vulkan или SDL_GPU backend;
- selection/update — `EffectManager`;
- map/client sequencing — `MapView` или `SpriteManager`.

### Базовые эффекты minimal profile

Движок поставляет fixed base effects в `Resources/Core/Effects/` и bootstrap
effects в `Resources/Embedded/Effects/`. Header каждого `.fofx` фиксирует
назначение, slot и принцип работы.

Base shaders рассчитаны на минимальный Direct3D feature level 9_x: без
`gl_FragCoord`/position semantic, screen derivatives и dynamic array/vector
indexing. Cross-compiler всё равно выдаёт HLSL SM4, GLSL 330, GLSL ES 300 и
Metal, но source избегает возможностей, не работающих на weakest profile.
Строка `Profile: minimal` в header фиксирует это ограничение.

Default mapping: `Font`/`Iface`/`Generic`/`Critter`/`Rain` → `2D_Default`;
`Roof`/`Tile`/`Flat` → `2D_NoDepth`; `Primitive` → `Primitive_Default`;
`Light` → `Primitive_Light`; `Fog` → `Primitive_Fog`;
`FlushPrimitive`/`FlushMap`/`FlushLight`/`FlushFog`/`FlushRenderTarget` →
соответствующие `Flush_*`; `SkinnedModel` → `3D_Skinned`; `ImGui` → setting
`ImGuiDefaultEffect`. Остальные `2D_WithoutEgg`, `3D_NormalMapping`,
`Flush_Map_BlackWhite`, `Font_Default`, `Interface_Default`, `Particles_*`
выбираются per draw/mesh/particle system.

`FlushMap` соединяет intermediate map target с готовым viewport layer. Его RGB
уже alpha-composited, поэтому `Flush_Map` и `Flush_Map_BlackWhite` пишут output
alpha `1.0`: нельзя снова умножать RGB на промежуточный alpha или переносить
coverage. Project override этого slot сохраняет правило. Generic
`FlushRenderTarget` сохраняет RGBA для model/particle/GUI/offscreen surfaces.

Проект с richer hardware размещает собственные advanced-profile copies в pack,
baked после `Core`/`Embedded`, под тем же resource name. Проектная копия shadows
engine fallback и может использовать derivatives/per-fragment lighting, но
minimal base движка не меняется.

## Depth state эффекта и общий depth buffer карты

Каждый pass `.fofx` задаёт:

- `DepthWrite` (default `True`) → `_depthWrite[pass]`.
- `DepthFunc` (default `Always`) → `_depthFunc[pass]`: `Always`, `Never`, `Less`, `LessEqual`, `Equal`, `GreaterEqual`, `Greater`, `NotEqual`. Backends учитывают различие NDC-Z OpenGL `[-1,1]` и D3D/Vulkan/SDL_GPU `[0,1]`, поэтому depth effects проверяются на каждом заявленном backend.
- `DepthVariants` (default `False`) → `_depthVariants`. Один draw может установить `RenderEffect::DepthVariant`: `FromEffect`, `TestWrite`, `TestNoWrite`, `NoTestWrite`, `NoTestNoWrite`; `Test` использует effect `DepthFunc`, `NoTest` заменяет его на `Always`. Это позволяет particle node хранить test/write intent без отдельного effect file на каждую комбинацию.

Resolved state адресуется slot через `ResolveDepthVariantSlot` и
`EFFECT_DEPTH_VARIANTS = 4`. Direct3D заранее строит depth-stencil object per
slot, Vulkan — pipeline per `(pass, primitive, blend, slot)`, SDL_GPU включает
slot в cache key, OpenGL применяет state напрямую. Resolver отклоняет slot,
который не был built, чтобы backend не отключил depth молча и не переиспользовал
stale pipeline. Slot кодирует **resolved** state, поэтому эквивалентный requested
variant допустим даже для effect без alternatives. `RenderEffect::CanBatch`
сравнивает `DepthVariant`.

## Snapshot фона сцены

Refractive content не может читать target, в который пишет. Поэтому
`SpriteManager::AcquireSceneBackground()` копирует current target в отдельный
target-sized render target. Copy lazy и не чаще одного раза на direct-draw
replay: `DrawSprites` invalidates snapshot перед replay, а copy выполняется
только при запросе. Blit opaque — нужны colors позади, а не повторное blending.

Effect читает copy через `RenderEffect::BackgroundTex` рядом с `MainTex` и
`IndoorMaskTex`; shader объявляет `BackgroundTex`. Все четыре реализованных GPU
backends связывают его одинаково. Snapshot сохраняет orientation source target,
поэтому shader сам flip-ит screen lookup для flipped texture.

Particle runtime получает provider через
`ParticleRuntimeServices::SceneBackgroundProvider` непосредственно в draw.
`Unavailable` означает отсутствие scene (например atlas offscreen) и fail-close.
`Deferred` используется для auxiliary preview системы, которую позже рисуют
directly: distortion node пропускается без retirement, а scene draw запрашивает
provider снова.

Model-attached runtime получает provider только внутри
`ModelInstance::DrawInScene`. Direct model всё равно обновляет auxiliary atlas
для preview/hit test; при `Render.ModelDirectDraw` этот refresh возвращает
`Deferred`, и attachment доживает до subsequent scene replay. Обычная
atlas-rendered model возвращает `Unavailable`.

## Face culling отдельного draw

Culling — свойство **draw**, не effect usage. Caller задаёт
`RenderEffect::CullMode`: `None`, `Back`, `Front`; default `None`.
`RenderEffect::CanBatch` сравнивает mode. 3D models задают `Back` либо `None`,
particle runtime переводит culling emitter node (`Front`, `Back`, `Double`).

Backends с baked rasterizer state создают варианты только при
`CullVariants = True`: Direct3D — rasterizer object per mode, Vulkan — pipeline
per `(pass, primitive, blend, depth slot, cull mode)`, SDL_GPU — cache key,
OpenGL — immediate `glCullFace`. `ResolveCullMode()` throws при unbuilt mode,
исключая silent drop/fallback. Front face везде counter-clockwise.

`MapView::_rtMap` создаётся `with_depth`, поэтому world использует общий depth
buffer. `EffectUsage::QuadSprite` и `Model` участвуют в нём; на targets без
depth attachment (UI, light, final screen flush) state является no-op.

- **Screen-space quads** (GUI, fonts, render-target blits, non-map effects) начинают с `Vertex2D::PosZ = 0`. Для map sprite `SpriteManager` перезаписывает Z перед flush в `_rtMap`.
- **Standing map sprites** (`Item`, `Critter`) пишут depth, но не test-ят его (`Always` + write в `2D_Default`/`2D_WithoutEgg`). Depth нужен direct particles/models; sprite-vs-sprite решает painter order. Их vertical planes имеют общий gradient `ProjectMapYToVerticalDepth` и не пересекаются, поэтому order по anchor depth точно совпадает с per-pixel `LessEqual`, не создавая z-fighting coincident rows.
- `MapSpriteList::MakeDrawOrderPos` сортирует standing sprites по `GeometryHelper::GetHexScreenRow(GetHexPos().y)`, то есть по классу одинаковой ground depth (`+2X/-1Y`), а не по raw hex row. Контракт закреплён `Test_Geometry.cpp`.
- Depth/sort anchor — **logical root**, не bitmap bottom-center. Item proto `Offset` одновременно позиционирует visual bitmap и хранится как `_rootOffset`; depth proxy вычитает его в `GetMapRootOffset()` и `scene_pos_y`, чтобы tree anchors на trunk. Critter root offset равен нулю.
- Только standing `Item`/`Critter` работают с depth. Floor tiles, roofs и flat overlays painter-only/depth-inert. `MapSprite` учитывает `Elevation`; `HexOffset` и runtime/tweak offsets проецируются по ground plane и меняют screen/depth непрерывно. Viewport-only `field.Offset` в world depth не входит; intrinsic `Sprite::Offset` определяет logical ground root.
- Floor/flat layers сохраняют atlas XY/UV и используют no-depth effects. Tiles/roofs выбирают `Effects.Tile`/`Effects.Roof`, flat items — `Effects.Flat` по `GetDrawFlatten()`. Для script `MapSpriteHolder` default effect назначает `MapView` по draw-order segment: tile/pre-light, flat/after-light, roof или generic.
- Item draw order определяется `GetDrawFlatten()`, не `IsScenery`/`IsWall`: upright → `Item`, static flat → `FlatItemPreLight`, dynamic flat → `FlatItemAfterLight`. Старые Scenery/Item пары слоёв объединены; одинаковый hex tie-break-ится add order (`_globalPos`). Dead critters сохраняют `Effects.Critter` и depth write, но standing sprites их не test-ят и покрывают по draw order.
- Flat/background layers рисуются до standing sprites с `DepthWrite=False`, `DepthFunc=Always`; им не нужны ground projection или layer bias. Standing sprites получают per-vertex `PosZ` через `ProjectMapYToVerticalDepth`, без draw-order bias.
- Единственный оставшийся layer-bias применяется direct-draw particles/models в конце sprite pass: один шаг из half-pixel budget `MAP_LAYER_DEPTH_BIAS / (DrawOrderType::Last + 1)`, ниже threshold subpixel snapping.
- `Core` и `Embedded` `2D_Default.fofx` обязаны проецировать `InPosition.xyz`; flatten Z уничтожит scene depth. `2D_Default` и `2D_WithoutEgg` discard final alpha `<= 1/255`, чтобы transparent texels не писали invisible depth.
- **Roofs** — floor tiles с positive `Elevation = Geometry.MapRoofElevation`; XY anchor задаёт `BaseTile.Offset`. Roof range `Roof..Last` рисуется отдельным trailing pass после всех lower layers и их direct replay. `Effects.Roof` не test/write depth, поэтому крыша всегда рисуется сверху и не clips subsequent content.
- **Particles** используют `LessEqual`, обычно `DepthWrite=False`; color variants включают `DepthVariants`. Atlas variants discard alpha `<= 1/255`, принимают `ParticleSamplingBuf` для point sampling через texel-center snap и вручную clamp/repeat внутри atlas sub-rectangle. SPARK non-atlas variants передают готовые atlas coordinates и neutral sampling buffer.
- **Models** (`3D_*`) используют `LessEqual` + `DepthWrite=True`, поэтому direct mesh пишет реальную surface depth, а не плоскость atlas quad.
- `OnRenderMap_AfterSpritesAndFog` вызывается после sprite/fog pass до flush map target. Script debug markers вычисляют position из `Map.GetHexMapPos`, `entity.GetSpriteOffset`, `entity.Elevation` и draw-area origin, не обращаясь к transient sprite instances.
- **Contours принадлежат scripts проекта.** Native contour pass отсутствует. Проект может cache-ить entities с ненулевым `Contour` и на `OnRenderMap_AfterSprites` вызывать `Map.DrawEntitySprite(entity, effectSubtype, colour, padding)`. Mapper selection использует тот же property-based путь.

## Спрайты direct-to-scene

`Sprite` может переопределить `IsDirectDraw()` и рисовать geometry прямо в
current scene target с общим depth вместо atlas quad batch. Чтобы не разрывать
batch вокруг каждого такого sprite, `SpriteManager::DrawSprites` собирает их и
после общего `Flush()` выполняет `Sprite::DrawInScene(scene_pos, depth)`.
Opaque sprites пишут depth (`Always`, write), direct transparents test-ят его
(`LessEqual`, no write). Anchor использует projected
`hex + HexOffset + SpriteOffset/TweakOffset + Elevation`, исключает viewport
`field.Offset` и получает только один anchor-bias step.

`ParticleSprite` поддерживает два типа из `.spark` attribute `draw in scene`:

- **Atlas** (default false): `Update()` двигает simulation и по cadence обновляет offscreen atlas через `DrawParticleToAtlas`; sprite рисуется batched quad, `IsDirectDraw() == false`.
- **Scene** (true): `IsDirectDraw() == true`; simulation продолжается вне visibility, а `DrawInScene` без advancement обновляет transform и рисует прямо в `_rtMap` через map view-proj. Particles сохраняют lifetime offscreen и depth-sort against scene.

`ParticleSprite::Play()` respawn-ит backend-neutral `ParticleSystem` до start.
Facade делегирует через `ParticleRuntimeSystem`, поэтому renderer-facing code не
содержит SPARK/Effekseer branches. One-shot SPARK можно повторно запускать после
`Game.PlaySprite(...)` и cache reuse.

Live bounds и rebasing emitted particles также являются capabilities facade.
Оба backend сообщают bounds baked extent: SPARK через `.spk` `bounds`, Effekseer
через `.efk` trailer. Rebasing нужен только SPARK world-space; Effekseer
композитит instance transforms с root matrix и использует no-op.

Seeded respawn детерминирован per system instance. Effekseer передаёт seed
manager handle. Каждый `SparkParticleRuntimeBackend` владеет `SPKContext` с IO
registry, default zone и ambient generator; loaded graph связывается с context.
Каждая system хранит собственный generator state и временно bind-ит его при
clone/prewarm/update, поэтому interleaved effects и разные engine instances не
влияют на sequence.

`SparkExtension.h` раскрывает только facade, forward declarations и plain
renderer data helpers. SPARK headers, `SparkQuadRenderer` и adapter скрыты в
`.cpp`; Mapper/Baker читают properties через helpers.

`ParticleSystem::SetScale()` обновляет neutral setup, применяет zero-delta
transform refresh и заставляет atlas redraw без respawn/reset elapsed time.
Контракт одинаков для atlas/scene и обоих runtimes.

Core Effekseer runtime также проходит через эти sprite/direct-scene paths.
Effekseer renderer interfaces используются как callbacks evaluated data:
FOnline копирует значения, строит `RenderDrawBuffer`, выбирает `RenderEffect` и
отправляет через обычный renderer. Никакой stock Effekseer graphics backend не
нужен.

Callback collectors fail closed на malformed topology. Они проверяют hard limit
instances и exact count из `BeginRendering`/`BeginRenderingGroup`; strip требует
chain order. Ring packets копируют outer/center/inner shape/colors, воспроизводят
8-vertex/12-index segment topology, angular fades и Z-sort, разделяя geometry
при 64 000 vertices для 16-bit indices. Для Z-sort сортируется lightweight index
permutation, затем materialize snapshots: это сохраняет deterministic stable
order и избегает invalid `std::stable_sort` over-aligned SIMD elements.

**Distortion nodes** refract scene: RG channels texture задают displacement в
plane particle, alpha маскирует форму, background берётся из snapshot. Plane per
vertex передаётся через model vertex layout tangent/bitangent; intensity и
orientation background — в reserved `ParticleSamplingBuf`. Поддержана только
sprite family; остальные material families отклоняются при load.

**Model nodes** рисуют mesh per instance. `.efkmodel` сначала полностью
валидируется перед vendored parser: максимум 64 MiB, 4096 frames, 64000 vertices
и 21333 faces per frame, с проверкой indices/counts. Instance transform
применяется к vertices, сохраняя общий draw-buffer/atlas/batching path. Node
culling переходит в `CullMode`, animated mesh выбирает frame по instance time.

**Ribbon и Track** используют общий strip builder. Он превращает chain
cross-sections left/center/right в два quad strips с общей center line,
растягивает texture по chain, применяет colors/atlas flags и chunks по vertex
budget. Ribbon трансформирует authored edge offsets или ориентирует их вокруг
emitter up axis для viewpoint mode. Track строит width поперёк travel direction,
усредняет interior joints и интерполирует width/color head-tail-middle.
Неподдержанные spline smoothing, tiled UV, trail smoothing, view offset,
left-handed strips, non-default Track materials и Z-sort отклоняются до build.
`Positions[2]/[3]` ribbon не проверяются вне spline path, потому что приходят
uninitialized.

`Test_EffekseerParticleRuntime.cpp` содержит self-contained cooked fixtures
реального callback-to-FOnline path: legacy determinism/fixed-step/quad/UV;
project-authored 1.80.5 sprite Z-sort; modern SKFE/1810 Ring topology, radii,
UV, all Z-sort modes и 64k chunking. Strip fixtures компилируются в test, чтобы
получить одновременно несколько instances, и закрепляют topology, shared
center, continuity, UV, width и viewpoint orientation.

Adapter принимает одну Default-material color texture либо renderer-owned white
pixel. Поддержаны `Clamp` и `Repeat` внутри atlas rectangle; `Mirror` rejected.
`Nearest` на linearly filtered atlas реализован snap к texel center. Dormant
non-zero distortion intensity при disabled material игнорируется; active
distortion всё равно проходит capability gate.

Effekseer sprites всегда scene type. Direct-scene prewarm ждёт первого
`DrawInScene`, чтобы получить current map transform. До него `Update()` не
advance-ит system; затем Effekseer проходит ровно одну секунду и reset-ит
wall-clock origin, не считая offscreen wait второй раз. `RefreshRenderTransform`
делает только zero-delta refresh, не forced first tick.

Flag проходит `SparkQuadRenderer::GetDrawInScene()` →
`ParticleSystem::GetDrawInScene()` → `ParticleSpriteFactory::LoadSprite`.
Model-bone particles (`ModelInstance::RunParticle`) идут другим путём и attribute
не учитывают.

`ModelSprite` становится direct scene при `Render.ModelDirectDraw`. Default
`false` оставляет cached atlas path. При `true` `DrawInScene` строит общий map
view-proj, включает logical root (`scene_pos` + raw scene depth) и вызывает
`ModelInstance::DrawInScene`. Skinning/animation переиспользуются, atlas-only
camera tilt пропускается, а `DrawToAtlas` остаётся для preview/hit test и берёт
весь automatic logical frame. Attached SPARK/Effekseer используют active direct
proj; distortion получает snapshot on demand. Старый shadow pass выключен,
поскольку его math atlas-space.

Cached model-sprite frames используют вычисленный logical limit: minimum из
`Render.ModelSpriteMaxTextureWidth` / `Height` и
`AppRender::MAX_ATLAS_WIDTH` / `HEIGHT` текущей машины, делённый на
`FRAME_SCALE`, потому что model рендерится в physical scratch texture с
масштабом 2x. Dynamic model-bone particle bounds выше этого budget считаются
недоступными: существующий model frame остаётся валидным, а runaway outlying
geometry обрезается. Это не позволяет malformed или long-lived particle motion
запросить unbounded CPU/GPU allocation в headless и rendered paths.

**World scale.** `Render.ModelProjFactor` задаёт screen pixels per 3D world unit
одновременно для models и in-scene particles. Engine default `40.0`; проект
может согласованно переопределить. Authoring metric остаётся
**1 world unit = 1 hex = 1 m**, но projection factor не обязан равняться
`MAP_HEX_WIDTH`. Radius N units занимает N hexes и совпадает с масштабом direct
3D models. Полный neutral route описан в
[Particle Format](../../how-to/content/particle-format.md#runtime-контракт).

## Практики встраиваемого проекта

Текущие проекты показывают полезные способы composition, но конкретные
settings, effect names, scripts и acceptance evidence остаются project-owned:

1. Оставляйте все selectors `Render.Force*` выключенными в обычном shipping profile. Для диагностики и cross-backend acceptance создавайте узкие sub-configs/launch recipes ровно с одним force selector.
2. Не копируйте render block старого проекта. Начинайте с settings для точно pinned Engine revision и отдельно проверяйте каждый override. Backend selectors и model/layout settings менялись; старый `.fomain` — migration evidence, не template.
3. Размещайте richer shaders проекта в resource pack после Engine `Core`/`Embedded`. Shadow того же name допустим только как осознанная замена minimal base effect с сохранением alpha/depth/descriptor/uniform contract. Это advanced-profile overrides, а не изменение fallback движка.
4. Считайте `Render.ModelProjFactor` единым authoring decision models и in-scene particles. Его изменение — visible content-scale migration; вместе проверяйте creatures, attachments, effects, hit areas и map occlusion.
5. Logical resolution policy, options settings, project effect slots, offscreen composition, contours и визуальная/accessibility приёмка принадлежат проектным docs/tests. Движок даёт mechanics, но не сертифицирует UI/art direction игры.
6. Квалифицируйте каждый renderer/driver, заявленный release. Clean scene на default backend не доказывает Vulkan, SDL_GPU, WebGL или конкретный SDL driver. Фиксируйте selector, driver, platform, scene, logs и screenshot/interaction evidence.

## Platform packages и связь с BuildTools

`BuildTools/cmake/stages/Packages.cmake` участвует в generation package targets.
Package workflow решает, какие app/runtime artifacts поставлять; доступность
backend определяется configured source, compile definitions, third-party deps и
toolchains.

Сохраняйте границы:

- frontend source определяет возможности движка;
- CMake/BuildTools определяют built apps/backends/platform packages;
- project presets выбирают конфигурации;
- platform docs объясняют debug resulting package.

Generated target names одного проекта не являются универсальными Engine names.

## Тесты frontend/rendering

Минимальная engine-local проверка без GPU —
`Source/Tests/Test_Rendering.cpp`. `NullRenderer` проверяет texture
read/write/clear, upload draw buffer и effect draw, а также rejection unbuilt
depth variant. Atlas packing/dump geometry принадлежит
`Test_TextureAtlas.cpp`, matrix/depth projection — `Test_Geometry.cpp`, а
model/image/particle suites — соответствующим runtime paths.

Native tests доказывают backend-neutral invariants, но не реализацию GPU.
Добавляйте visible target-specific route для каждого затронутого backend:
Null/headless, OpenGL/WebGL, Direct3D, Vulkan, SDL_GPU. У direct Metal нет
реализованного route и acceptance evidence.

## Чек-лист проверки

При изменении frontend/rendering убедитесь, что:

- `Application` init работает для graphical, headless и test/tool flows;
- input changes сохраняют `InputEvent` invariants и mapping client script events;
- touch/gamepad changes platform-neutral либо явно guarded;
- изменение проверено на affected backend: Null/headless, OpenGL/WebGL, Direct3D, Vulkan или SDL_GPU; direct Metal placeholder не считается coverage;
- render-target stack push/pop и restoration previous target сохранены;
- orientation учитывает `IsRenderTargetFlipped()`: OpenGL flipped, Direct3D/Vulkan/SDL_GPU not flipped;
- effect changes описывают parsing config, shader files и script-value buffers; Vulkan resources соблюдают set-0-UBO/set-1-sampler;
- Web changes связаны со [сборкой, упаковкой и отладкой в браузере](../../how-to/platforms/web-debugging.md), Android — со [сборкой, упаковкой и отладкой на Android](../../how-to/platforms/android-debugging.md), native attach/debug — с [нативной отладкой и отладкой AngelScript](../../troubleshooting/debugging.md).
