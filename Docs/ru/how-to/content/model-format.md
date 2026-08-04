---
layout: default
title: Формат моделей и 3D-композиция
document_id: model-format-guide
locale: ru
permalink: /Docs/ru/how-to/content/model-format.html
---

<!-- docs-translation: {"document_id":"model-format-guide","locale":"ru","source_path":"Docs/en/how-to/content/model-format.md","source_sha256":"a013884c3965997fe1b36397fbafb41fdd330690416d8d5d6e6066fb3bb4b020"} -->

# Формат моделей и 3D-композиция

FOnline использует описания моделей `.fo3d`, чтобы собрать на клиенте запечённые 3D-меши, преобразованные исходные анимации, выбираемую слоями экипировку, дочерние модели, частицы, переопределения материалов и объёмы отсечения.

Это руководство описывает авторинг и runtime-модель. Точный контракт текущей ревизии приведён в сгенерированных справочниках [токенов](../../reference/model-format/tokens.md), [ресурсов и лимитов](../../reference/model-format/assets.md), [правил валидации](../../reference/model-format/validation.md) и в [канонической JSON-модели](../../../generated/model-format.json).

## Область и авторитетные источники

Контрактом владеют:

- `Source/Tools/ModelMeshBaker.cpp` и `Source/Common/ModelMeshData.*` для импорта и проверки мешей `.fbx` / `.obj`, а также mesh-only payload `LFMODMSH`;
- `Source/Tools/ModelSourceLoader.*`, `Source/Tools/ModelAnimationConverter.*` и `Source/Common/ModelAnimationData.*` для извлечения исходного скелета и клипов, анализа совместимости, преобразования Ozz и нативного rig payload;
- `Source/Tools/ModelInfoBaker.cpp` для разбора `.fo3d`, раскрытия include, проверки зависимостей и исходников, сериализации model info, создания runtime rig и метаданных анимации;
- `Source/Client/ModelManager.*`, `ModelHierarchy.*`, `ModelInformation.*`, `ModelInstance.*` и `ModelAnimation.*` для строгой runtime-загрузки, общих неизменяемых данных, композиции и позы экземпляра, контроллеров анимации и рисования;
- `Source/Frontend/Rendering.h` и сгенерированный CMake-интерфейс проекта для compile-time лимитов моделей;
- тесты baker, mesh data, source loader, animation data/converter/runtime, совместимости скелета, Ozz и client engine как исполняемые примеры грамматики, форматов, преобразования, загрузки и ошибок.

Это переиспользуемая документация движка. Встраивающий проект владеет конкретными именами моделей, значениями слоёв, enum анимаций, художественными правилами, экипировкой, игровыми таймингами и сценами визуальной проверки.

Полная машинная модель генерируется из `BuildTools/ModelFormatInterface.json`. Генератор напрямую сравнивает документированный набор токенов с `ModelDescriptionParser::ParseToken`; расхождение парсера и документации останавливает проверку.

## Обзор конвейера

Конвейер моделей состоит из двух упорядоченных baker и общих модулей загрузки и преобразования:

1. `ModelMeshBaker` с порядком `4` импортирует `.fbx` и `.obj` и записывает версионированный mesh-only ресурс `LFMODMSH` по тому же пути и с тем же расширением. Клипы и изменяемая поза в этот payload не входят.
2. `ModelInfoBaker` с порядком `6` разбирает конкретные `.fo3d`, проверяет ссылки и свежесть исходников, загружает выбранные скелеты и клипы через `ModelSourceAssetCache`, преобразует канонический runtime rig и записывает `LFMODINF` с обязательным payload `LFOZZRIG` по пути `.fo3d`. Он также создаёт `ModelAnimationInfo.foinfo` для общих длительностей и границ.

Клиент не разбирает авторский текст, FBX или OBJ. `ModelManager` загружает общие иерархии мешей, `ModelInformation` строго читает неизменяемое описание и rig, а каждый `ModelInstance` владеет изменяемыми контроллерами, матрицами позы, дочерними объектами, частицами и render-композицией. Старые payload без заголовка и частичные fallback для rig отклоняются.

Файлы с basename `TEMPLATE_` предназначены только для include. Они влияют на конкретные описания и timestamps запекания, но не создаются как самостоятельные `.fo3d`-ресурсы или секции `ModelAnimationInfo.foinfo`.

## Контракт исходного меша

### Поддерживаемые входы

Текущий `ModelMeshBaker` сканирует только:

- `.fbx` для скелетных и статических мешей, skinning, имён diffuse-текстур материалов, исходных скелетов и анимационных клипов;
- `.obj` для статических моделей, присоединяемых объектов и объёмов отсечения.

Устаревшие `.x` и `.3ds` не являются текущими входами. `ModelMeshBaker` не выберет файл с таким расширением.

### Поведение импорта

Mesh baker и source loader используют закреплённую версию `ufbx`, но имеют разных владельцев. Первый выдаёт иерархию, bind, vertices, indices, skin и materials; второй извлекает проверенные данные skeleton/TRS/clip для преобразования анимации. Встроенные файлы игнорируются, skinning вычисляется, веса очищаются, а отсутствующие normals обрабатываются детерминированно.

Требования к исходникам:

- faces должны быть заранее триангулированы;
- конкретная модель должна содержать хотя бы один drawable mesh;
- имена nodes становятся именами bones, а nodes с геометрией — именами drawable meshes;
- используйте один material на drawable node, если важно детерминированное владение текстурой;
- файловая текстура `DiffuseColor` первого material становится texture slot `0`;
- имена текстур сохраняются без исходной директории и затем разрешаются относительно запечённого меша;
- используется только первый skin deformer;
- число skin clusters должно помещаться в `FO_MODEL_MAX_BONES`;
- у вершины сохраняются только `FO_MODEL_BONES_PER_VERTEX` влияний, после чего веса нормализуются;
- меш без skin получает детерминированную привязку к одной кости;
- имена animation stacks являются clip names для `Anim`, но клипы загружаются из исходника и входят в rig model info, а не в `LFMODMSH`;
- прямые `.fbx`-attachments должны содержать только rest pose; если у присоединённого исходника есть клипы, используйте дочерний `.fo3d` с явными `Anim`;
- внешние animation sources должны содержать только hierarchy и animation. Drawable geometry запрещена, кроме точного файла с временным исключением `AllowAnimationGeometry`.

Source loader отклоняет не-конечные transforms, дубликаты clip names без учёта регистра, неверные durations и key times, превышение лимитов и глубины, а также некорректные отношения skeleton. Animation sources могут добавлять совместимые канонические joints без физического `ModelBone`; физические meshes и cuts остаются в базовой иерархии.

Текущие лимиты по умолчанию:

| Опция проекта | Runtime-константа | По умолчанию |
| --- | --- | ---: |
| `FO_MODEL_LAYERS_COUNT` | `MODEL_LAYERS_COUNT` | `30` |
| `FO_MODEL_MAX_TEXTURES` | `MODEL_MAX_TEXTURES` | `8` |
| `FO_MODEL_MAX_BONES` | `MODEL_MAX_BONES` | `54` |
| `FO_MODEL_BONES_PER_VERTEX` | `MODEL_BONES_PER_VERTEX` | `4` |

Это контракты бинарной формы и shader layout. При переопределении проект обязан использовать одинаковые значения в client binaries, запечённых моделях, `Critter.ModelLayers`, effects и packages.

## Лексический синтаксис

`.fo3d` представляет собой последовательность токенов, разделённых whitespace:

- `#` и `;` начинают комментарий;
- quoted strings и escape syntax отсутствуют;
- paths и names не могут содержать whitespace;
- в одной строке может быть несколько directives;
- directive забирает обязательные arguments, затем разбор продолжается со следующего token;
- неизвестный token или отсутствующий argument является ошибкой запекания;
- integer arguments принимают числа, явные booleans или enum names из metadata resolver;
- float arguments должны быть конечными числами.

Компактная запись допустима:

```text
Layer 1 Value 2 Attach Hat.fbx Link Head RotY 180 Texture 0 Hat.tga
```

Для сопровождения размещайте structural selectors (`Layer`, `Value`, `Root`, `Attach`) перед относящимися к ним modifiers.

## Минимальные описания

Минимальная статическая модель:

```text
Model Props/Crate.obj
```

Скелетная модель с анимациями и attachment, выбранным слоем:

```text
Model Characters/Human.fbx
RotationBone Spine

Anim CritterStateAnim.Unarmed CritterActionAnim.Idle ModelFile Idle
Anim CritterStateAnim.Unarmed CritterActionAnim.Walk ModelFile Walk

Layer 1
Value 1
Attach Items/Hat.fbx Link Head
```

Enum names зависят от metadata встраивающего проекта. Числа в тестах движка доказывают поведение parser, но не являются рекомендуемым словарём проекта.

## Include и шаблоны

`Include` разбирает другой файл inline:

```text
Include TEMPLATE_Humanoid.fo3d mesh Human.fbx scale 0.9
```

После path идут пары name/value. До tokenization каждое буквальное `%name%` в подключаемом тексте заменяется значением:

```text
# TEMPLATE_Humanoid.fo3d
Model %mesh%
Scale* %scale%
```

Правила include:

- path задаётся относительно файла, содержащего `Include`;
- пути `Model`, `Attach` и `Cut` в подключённом тексте задаются относительно include-файла;
- внешний файл `Anim` позднее разрешается относительно итогового конкретного `.fo3d`;
- include arguments занимают остаток строки, поэтому следующую directive на той же строке размещать нельзя;
- replacements являются простой заменой текста, а не token-aware операцией;
- включённый текст разделяет parser state с вызывающим файлом;
- рекурсивные include отклоняются;
- самый новый timestamp во всём include graph управляет incremental rebake.

Предпочитайте самодостаточные шаблоны, явно устанавливающие `Root`, `Layer`, `Value` и `Mesh`. Скрытая зависимость от состояния вызывающего файла затрудняет работу людей, ИИ и валидаторов.

## Состояние парсера

Parser отслеживает выбранные `Layer`, `Value`, текущий link для modifiers и текущий selector `Mesh` для `Texture` и `Effect`.

В начале файла текущим link является default root, поэтому top-level transforms и materials применяются к базовой модели даже без `Root`.

`Layer` или `Value` изменяет selector, очищает `Mesh` и переводит текущий link на dummy object. После выбора пары layer/value запишите `Root`, `Attach` или `AttachParticles` до transform, material, disable или cut. Modifiers при активном dummy link разбираются, но отбрасываются.

Directive для возврата `Layer` к начальному `-1` нет. Все default-root declarations должны идти до первого `Layer` либо в более раннем include.

`Root` также очищает `Mesh`. `Attach` и `AttachParticles` создают link и очищают `Mesh`. Размещайте `Mesh` после selector link, к которому он относится.

`Link` сохраняется только для не-default и не-dummy link. На default root он игнорируется. `Link` у layer `Root` сериализуется, но дочернего объекта для присоединения нет; не задавайте его.

## Слои и значения

`Layer` выбирает индекс фиксированного массива:

```text
0 <= layer < FO_MODEL_LAYERS_COUNT
```

`Value` выбирает точное целое, определённое проектом. Ноль означает inactive и не может создать layer entry `Root`, `Attach` или `AttachParticles`.

В runtime `ModelInstance::PlayAnim` копирует или повторно использует массив слоёв, применяет точные overrides `AnimLayerValue`, сбрасывает модель к default root, находит совпавшие пары `Layer`/`Value`, применяет root modifiers и materials, создаёт или сохраняет children и particles, удаляет больше не выбранные элементы и перестраивает combined meshes при изменении композиции.

Layer value — это состояние render-композиции, а не только косметические metadata. Оно может менять transforms, animation speed, geometry, materials, effects, cuts, child models, particles и batching. Семантика каждого layer/value принадлежит документации и тестам проекта.

## Модификаторы корня

Без выбранного layer `Root` выбирает default link базовой модели:

```text
Root
Scale 0.9
RotX 90
```

При выбранной ненулевой паре layer/value он создаёт условный root modifier:

```text
Layer 3
Value 2
Root
DisableMesh Torso
Texture 0 Armor.tga
```

Условные root links могут добавлять transforms и speed multipliers, переопределять textures/effects, отключать layers и meshes, добавлять cuts. Дочернюю модель они не создают.

## Присоединение моделей

`Attach` требует выбранного layer и ненулевого value:

```text
Layer 1
Value 4
Attach Weapons/Rifle.fo3d Link RightHand
```

Путь дочернего объекта задаётся относительно declaring file.

### Присоединение к одной кости

При `Link <bone>` весь child становится дочерним объектом одной проверенной кости:

```text
Attach Hat.fbx Link Head
```

Rotation, translation, scale, speed, materials, disables и cuts этого link применяются внутри child instance.

### Общий скелет

Без `Link` runtime сопоставляет одноимённые кости child и parent:

```text
Attach ArmorTorso.fbx
```

Используйте это только для одежды и частей тела, созданных под один skeleton. Если общих bones нет, runtime creation завершается ошибкой.

### Дочернее описание или прямой меш

Используйте `Attach child.fo3d`, когда child нужны собственные base mesh, layers/attachments, default material/effect policy, cuts, animations или rendering flags. Для простой запечённой иерархии достаточно прямого `.fbx` / `.obj`. Дочерний `.fo3d` запекается самостоятельно; parent проверяет существование descriptor и bone из `Link`.

## Присоединение частиц

`AttachParticles` выбирается слоем:

```text
Layer 8
Value 1
AttachParticles Particles/Jet.spk Link Backpack
MoveY 0.15
RotY 90
```

Particle path является глобальным путём запечённого ресурса, а не путём относительно `.fo3d`. Ссылайтесь на созданный `.spk` или `.efk`, а не на авторский `.spark` или `.efkproj`. Всегда задавайте корректную bone в `Link`: runtime-создание частиц требует её.

`MoveX/Y/Z` и `RotY` определяют placement частицы. Instance живёт, пока активна точная пара layer/value, и удаляется при изменении композиции. XML, SPARK objects, renderer fields, effects/textures, runtime cache и визуальная проверка принадлежат [формату частиц](particle-format.md). Частицы на bones используют прямой путь 3D-композиции, а не selector atlas/direct-scene у `ParticleSprite`.

## Преобразования и скорость

Поля link: `RotX/Y/Z` в градусах, `MoveX/Y/Z` в координатах модели, `ScaleX/Y/Z` и playback multiplier `Speed`. `Scale` задаёт все три оси.

У каждого поля есть assignment, additive и multiplicative формы:

```text
Scale 0.9
Scale+ 0.1
Scale* 1.5

RotY 90
RotY+ 15
RotY* 0.5
```

Для форм `+` и `*` действует особая инициализация: если поле равно нулю, operand становится его значением; иначе выполняется обычное сложение или умножение. Поэтому template может использовать `Scale* 0.9` или `Speed* 1.2` без предшествующего assignment, а порядок declarations наблюдаем.

В runtime ноль означает identity/no contribution. Ненулевые transforms перемножаются с текущим model transform. Отрицательный итоговый `Speed` запрещён при запекании; ноль означает отсутствие вклада в скорость.

## Меши, текстуры и эффекты

`Mesh` выбирает drawable node:

```text
Mesh Torso
Texture 0 Armor.tga
Effect Effects/Armor.fofx
```

`Mesh All` очищает selector, и следующие material directives применяются ко всем drawable meshes текущей модели link. `Subset` устарел: parser забирает argument и пишет warning, но ничего не выбирает.

### Текстуры

```text
Texture <slot> <name>
```

Slot должен лежать в `[0, FO_MODEL_MAX_TEXTURES)`. Для обычного имени выбранный `Mesh` должен существовать и быть drawable, path разрешается относительно target mesh, а ресурс должен быть запечён. Imported diffuse texture является default для slot `0`; остальные slots изначально пусты.

В attached child `Parent` копирует первую совпавшую текущую texture родителя в том же slot, а `Parent_<mesh>` — texture конкретного parent mesh:

```text
Texture 0 Parent_Torso
```

Не используйте `Parent` в root description; при нескольких parent meshes задавайте suffix явно.

### Эффекты

```text
Effect Effects/SkinnedArmor.fofx
```

Effect paths являются глобальными baked-resource paths. `Parent` и `Parent_<mesh>` копируют текущий effect родителя по тем же правилам. Meshes объединяются в draw batch только при совместимых effect, texture set и bone capacity, поэтому material overrides могут менять batching и требуют измерения на типичных композициях.

## Отключение слоёв и мешей

`DisableLayer` принимает layer indices через дефис:

```text
DisableLayer 5-6-7
```

При активном link эти slots пропускаются внутри model instance.

`DisableMesh` принимает drawable node names через дефис, а `DisableMesh All` сохраняет wildcard и отключает все meshes:

```text
DisableMesh Hair-HelmetBase
```

Используйте отключения для взаимоисключающей композиции, но явно документируйте project ownership слоёв. Циклическая и зависящая от порядка политика плохо тестируется.

## Объёмы отсечения

`Cut` удаляет geometry из выбранных слоёв combined mesh:

```text
Cut CutVolumes/Helmet.obj All HeadVolume - - -
```

Шесть arguments:

1. path `.fbx` / `.obj` cut volume относительно declaring file;
2. target layers через дефис либо `All`;
3. drawable shape names из cut file через дефис либо `All`;
4. первая unskin bone либо `-`;
5. вторая unskin bone либо `-`;
6. unskin shape, `~shape` для reversed behavior либо `-`.

`All` для layers раскрывается во все compile-time layers, кроме текущего выбранного. В default-root scope включаются все layers. `All` для shapes выбирает все drawable shapes, кроме отдельно указанного unskin shape.

Runtime классифицирует cut shape по числу запечённых vertices: ровно `36` означает axis-aligned box bounds, любое другое число — sphere radius по X extent. Это правило формата движка, а не общий mesh heuristic. Создавайте простые специализированные cut assets и проверяйте результат визуально.

Обе unskin bones задаются вместе; unskin shape требует обеих. Все bones и drawable shapes проверяются при запекании. Любой cut отключает обычный culling composed model, поэтому это correctness feature с render cost.

## Управление рендерингом

### Автоматический layout model sprite

`DrawSize` и `ViewSize` удалены. `ModelInfoBaker` пишет aggregate bounds, idle-priority view/name bounds и per-animation bounds в `ModelAnimationInfo.foinfo` версии 2. Клиент проецирует их для каждого направления, расширяет с учётом children и layers и вычисляет offscreen frame, visual anchor, lighting envelope и interaction/view rectangle.

Начальный frame покрывает bounds всех включённых animations. Точное weighted skinning текущей позы даёт более тесный atlas crop; ограниченный expansion/rerender обрабатывает выход позы за allocation. Авторы настраивают source transforms, animation reach, attachments и `Render.ModelProjFactor`, а не фиксированные пиксельные прямоугольники в `.fo3d`.

Проверяйте все направления и типичные animations в видимом клиенте. Для диагностики clipping, пустого пространства, polygon edges и crop placement используйте `Game.DumpAtlases()` или **Dump atlases** в Mapper. Бинарный и runtime-контракты описаны в [Baking Pipeline](../../explanation/content-pipeline/baking.md#общие-metadata-анимации) и [Frontend и рендеринг](../../explanation/rendering/#геометрия-atlas-спрайтов-и-моделей).

### Прочие флаги

- `DisableShadow` отключает тень модели.
- `DisableAnimationInterpolation` выбирает nearest-key sampling.
- `DisableBackwardAnim` выбирает forward walk/run вместо `WalkBack` / `RunBack` и совмещает look direction с движением.
- `RotationBone <bone>` включает movement overlay controller и directional rotation torso/head.
- `FastTransitionBone <bone>` сбрасывает transition state нового child на указанной link bone.

## Граница анимации

Animation directives `.fo3d`:

```text
Anim <state> <action> <ModelFile|animation-source> <clip|~clip|Base>
AnimSpeed <state> <action> <positive-factor>
AllowAnimationGeometry <external-animation-file>
AnimLayerValue <state> <action> <layer> <value>
StateAnimEqual <from> <to>
ActionAnimEqual <from> <to>
FastTransitionBone <bone>
RotationBone <bone>
DisableAnimationInterpolation
DisableBackwardAnim
```

[Model Animation](model-animation.md) описывает first-entry-wins tuples, `ModelFile`/`Base`/`~clip`, совместимость skeleton и rig conversion, one-step aliases, effective duration, common и loaded-client lookup, substitutions и validation.

`AnimLayerValue` применяется к точной запрошенной паре до model composition. Alias resolution относится к animation lookup; не считайте, что alias также переписывает key для layer override.

`AllowAnimationGeometry` — узкий migration aid. Он называет один точный внешний файл из `Anim`, разрешается от конечного `.fo3d`, используется только baker validation и не сериализуется. Duplicate paths/targets, невыбранные файлы и оставшиеся после удаления geometry исключения являются ошибками. Исправьте export, сохранив необходимую helper/bone hierarchy, и удалите исключение в той же миграции.

3D skeletal animation отделена от 2D-контракта `NextX` / `NextY` в [Sprite Root Motion](sprite-root-motion.md).

## Runtime-загрузка и кеширование

`ModelManager::CreateModel(name)` принимает запечённый `.fo3d` с полной composition semantics либо baked mesh path с простой rest-pose model без declarations и animation controller.

Descriptions и mesh hierarchies кешируются по resource name. Неизменяемые clips, remaps, bindings и canonical skeleton принадлежат `ModelInformation`; mutable timelines, poses, matrices, children и procedural transforms — каждому `ModelInstance`. При смене слоёв активные children повторно используются по stable baked link id, а переставшие совпадать children и particles удаляются.

Combined mesh generation объединяет совместимые видимые meshes, пока effect, textures или bone capacity не требуют нового batch. Cuts применяются после объединения parent и child meshes.

Не изменяйте и не разбирайте binary payload запечённых `.fo3d`, `.fbx` или `.obj` из project scripts. Их layout является приватным контрактом baker/runtime.

## Поведение при ошибках

`ModelInfoBaker` отклоняет или сообщает: отсутствующий `Model`; недоступные, устаревшие или malformed baked meshes и sources; primary mesh без drawable geometry; отсутствующие textures/effects/particles/children/cuts; неверные layer/texture indices; нулевые layer values для `Root`/`Attach`; отсутствующие bones или meshes; malformed includes; invalid/non-finite numbers; отрицательный `Speed`; неположительный `AnimSpeed`; неизвестные enums, clips или несовместимые skeleton; attached FBX с clips; внешнюю animation geometry без точного временного исключения; неверные cut combinations; неизвестные tokens.

Runtime повторяет критические binary и range checks. Runtime exception означает повреждённые или устаревшие baked data либо пробел в validation; не ловите её для тихой подстановки несвязанной модели.

## Предупреждение об устаревшем контенте

Не выводите текущую поддержку из старых FOnline-проектов:

- `.x` и `.3ds` не выбираются текущим mesh baker;
- `AnimEqual` заменён на `StateAnimEqual` и `ActionAnimEqual`;
- `CalculateTangentSpace`, `RenderFrame` и `RenderFrames` не являются текущими tokens;
- `Subset` оставлен только как warning path и ничего не выбирает.

Сначала переведите legacy assets на текущие source formats и grammar, затем проверяйте против текущей ревизии Engine. Старый проектный контент — свидетельство исторического использования, а не нормативная спецификация.

## Практики авторинга

1. Размещайте default-root declarations до первого `Layer`.
2. Оставляйте у concrete description ровно один намеренный итоговый `Model`.
3. Начинайте include-only files с `TEMPLATE_`.
4. Пусть template явно задаёт selector context.
5. Используйте enum names и project layer constants из metadata.
6. Документируйте каждый layer index, allowed value, owner и конфликтующий layer.
7. Делайте отдельный drawable node для независимо заменяемого material/effect.
8. Используйте direct mesh для простого prop, а child `.fo3d` для переиспользуемой композиции.
9. Всегда задавайте `Link` у particle attachment.
10. Соблюдайте регистр paths, bones, meshes, animation stacks и effects.
11. Делайте cut volumes простыми и специализированными.
12. Проверяйте матрицу сочетаний layers, а не только каждый attachment отдельно.
13. Экспортируйте external animations без drawable geometry, сохраняя нужную hierarchy.
14. Считайте каждый `AllowAnimationGeometry` временным долгом с владельцем ремонта.
15. После изменений loader, converter, mesh wire или animation sources выполняйте force bake и затем incremental bake.

Для ИИ-авторинга фиксируйте parser state перед каждым modifier:

```text
current layer = 3
current value = 2
current link = Attach Armor.fo3d
current mesh = Torso
next directive = Texture 0 Parent_Torso
```

Если состояние нельзя назвать однозначно, разбейте компактную строку и явно задайте selectors.

## Процесс проверки

1. Перегенерируйте и проверьте справочник:

   ```powershell
   python BuildTools\docs_model_format.py --write
   python BuildTools\docs_model_format.py --check
   python -m unittest BuildTools.tests.test_docs_model_format
   ```

2. Запустите focused Engine tests:

   ```powershell
   .\Binaries\Tests-Windows-win64\LF_UnitTests.exe "ModelBaker*"
   ```

3. Перезапеките встраивающий проект:

   ```powershell
   cmake --build Build\Auto --config RelWithDebInfo --target BakeResources
   ```

4. В видимом клиенте проверьте framing/anchors/crop bounds, все layer/value, оба вида attachment, particles, inheritance textures/effects, disables, cuts, idle/movement/turn/backward/action animations, shadows и interpolation.
5. Зафиксируйте project-specific layer semantics, ожидаемые screenshots и regression routes в документации проекта.

Чистый bake доказывает grammar, asset closure, enums/ranges и serialization. Он не доказывает качество позы, scale, clipping, материалы, blending, cut geometry, interaction bounds или performance.

## Маршрутизация изменений

- При изменении `.fo3d` tokens, parser state, include, path rules или validation обновите руководство, `BuildTools/ModelFormatInterface.json`, generated outputs и focused tests.
- При изменении импорта `.fbx` / `.obj`, skinning, materials, animation conversion или model limits обновите asset/limit contract и native mesh-baker tests.
- При изменении layers, attachments, particles, transforms, materials, cuts, batching или flags обновите composition/runtime sections и проверьте видимый client scene.
- Animation tuples, aliases, speed, duration и script lookup принадлежат также [Model Animation](model-animation.md).
- 2D frame offsets и sprite phase принадлежат [Sprite Root Motion](sprite-root-motion.md).
- Каталоги моделей и семантика слоёв принадлежат только документации встраивающего проекта, которая ссылается на этот контракт.
