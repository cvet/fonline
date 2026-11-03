---
layout: default
title: Смещения кадров спрайтов и циклы ходьбы
locale: ru
document_id: sprite-root-motion
permalink: /Docs/ru/how-to/content/sprite-root-motion.html
---
<!-- docs-translation: {"document_id":"sprite-root-motion","locale":"ru","source_path":"Docs/en/how-to/content/sprite-root-motion.md","source_sha256":"8ad3d93337f7794dbe36cb2735a9e317972abd811e931e09de4e7e72e5420f2a"} -->
# Корневое движение спрайтов и циклы ходьбы

FOnline переносит запечённые 2D-дельты кадров и согласует позы walk/run с
авторитетным движением по гексам. Проект владеет art, animation mapping,
настройкой движения и визуальной приёмкой.

## Статус контракта

Исходники/тесты Engine определяют независимый 2D-контракт переноса offsets,
выбора кадров по движению, передачи фазы и нормализации при остановке.
Last Frontier/TLA — discovery и compatibility evidence, закреплённые в
`BuildTools/ExternalProjectEvidence.json`; их art, policy и исторические имена
не расширяют контракт. Выводите переиспользуемые утверждения из Engine.

[Форматы изображений и спрайтов](image-format.md) описывают импорт, FOFRM,
контейнеры, загрузку, atlases и caches. Анимация/длительности `.fo3d` принадлежат
[анимации моделей](model-animation.md).

## Область и источники истины

Интервалы маршрута, elapsed/lease и look-ahead в MovingContext используют timespan.
Сетевые мс и расстояния прежние; offset при смене скорости округляется до целой мс.

Контрактом владеют следующие исходники:

- `Source/Tools/ImageImport.*` - `FrameShot::Motion` и общий контекст загрузчика;
- `Source/Tools/ImageBaker.*` - координация, преобразования и сериализация запечённых спрайтов; соседние пары `ImageBaker-*.h/.cpp` владеют импортом исходных форматов;
- `Source/Common/SpriteResource.*` - общий декодер кадров SpriteResource v3 и перенос `NextOffset`;
- `Source/Client/DefaultSprites.*` - `SpriteSheet::_sprOffset` / `_sprMotion`, загрузка запечённых данных, общие кадры и листы по направлениям;
- `Source/Common/Movement.*` - авторитетный прогресс времени/пути и построение целочисленного пиксельного `HexOffset`;
- `Source/Common/Geometry.*` - re-split без потери позиции между гексом и offset с учётом проходимости;
- `Source/Client/AnimationMovement.*` — чистая математика накопления дельт, проекции цикла, переноса фазы, выбора кадра и визуальной компенсации;
- `Source/Client/CritterHexView.*` — anchor цикла, displacement движения, применение animation offset и нормализация при остановке;
- `Source/Client/CritterFramesCache.cpp` - кеш листов по направлениям и тегов анимации, разрешённых проектом; проект выбирает, извлекает и склеивает кадры через общий API спрайтов;
- `Source/Tests/Test_Mapper.cpp`, `Source/Tests/Test_ImageBaker.cpp` и `Source/Tests/Test_Geometry.cpp` — математика цикла, реальный потребитель CritterHexView, перенос формата и нормализация геометрии.

Документация и контент проектов полезны как integration evidence, но не являются источником истины для переиспользуемого поведения Engine.

## Три независимые позиции

Для движущегося 2D-криттера участвуют три связанные величины:

1. **Логическое движение**: `MovingContext` владеет прогрессом пути, текущим гексом, направлением, завершением и скоростью движения.
2. **Интерполяция внутри гекса**: `MovingContext::BuildProgress` преобразует плавную позицию сегмента в целочисленные пиксельные значения `HexOffset`. `CritterHexView::ProcessMoving` применяет логический гекс и offset.
3. **Авторское смещение спрайта**: каждый кадр анимации содержит целочисленную delta `(Motion.x, Motion.y)`. `CritterHexView` выбирает кадр и вычисляет `_offsAnim`, чтобы отображаемый спрайт следовал созданному циклу ходьбы, пока логическое движение остаётся авторитетным.

Смещения кадров — данные представления. Они не перемещают сущность, не выбирают путь, не меняют скорость, не разрешают переход между гексами и не реплицируют движение по сети.

## Создание покадровых offsets

`ImageImport::FrameShot` хранит:

```cpp
isize32 Size;
ipos32 Motion;
ipos32 Offset;
```

`Size` задаёт размер изображения, `Offset` — независимую привязку при отрисовке,
`Motion` — пиксельная дельта кадра, накапливаемая при проигрывании. Каждая пара добавляет пиксельное смещение в цикле из `N` кадров одного направления:

```text
delta[i] = Motion[i]
accum[i] = sum(delta[0..i])
T        = sum(delta[0..N-1])
```

`T` — смещение цикла. Длина и угол могут отличаться от шага сетки; runtime
проецирует движение на него без предположений о шаге гекса или длине цикла.

Пары кадров FRM/FRx и DeltaX/DeltaY ART задают движение. Центры BAM и расчёт
BBox/центра SPR задают `Offset`, без импортированного движения. PNG, TGA, MOS,
RIX, ZAR и TIL движения не импортируют. FOFRM сохраняет дельты, если
Motion/FrameMotions явно не заменяет движение: int16 X Y, одна пара на ссылку
до разворачивания. Пиксельные преобразования не вращают и не масштабируют движение.
Необязательные пары FrameOffsets меняют размещение картинки (`Offset + FrameOffsets[i]`)
без накопления и root motion; NextX_N/NextY_N запрещены. Число пар, границы раздела
и вложенность описывает [грамматика FOFRM](image-format.md#грамматика-fofrm).

ART сохраняет DeltaX/DeltaY как Motion со знаками отражения. Точки привязки
выравнивают холст и размещение последовательности. Source сохраняет оба вида;
обычная композиция отбрасывает размещение дочерней последовательности,
сохраняя покадровое размещение/дельты. См. [ART](image-format.md#art).

Offsets зависят от направления. Проект выбирает кадры и объединяет последовательности
через [общий API спрайтов](image-format.md#script-owned-2d-critter-animations). Движок
не задаёт правила Fallout-имён, обрезки или склейки.

## Запечённое и runtime-представление

`ImageBaker::BakeCollection` записывает проверенные компоненты Motion как signed16, размещение
int32 и положительную uint32 длительность каждого логического кадра в SpriteResource v3.
Shared-слот ссылается на прежние пиксели/геометрию, сохраняя собственные движение,
размещение и время. `ReadSpriteResource` декодирует движение в `SpriteResourceFrameData::NextOffset`.

`DefaultSpriteFactory::LoadSprite` переводит размещение в координаты логического
холста и хранит его независимо от движения `NextOffset` каждой записи. Shared-запись
использует логическое размещение спрайта-источника плюс разность собственного
и исходного запечённых `Offset`; цепочки сохраняют эту привязку. У направлений
собственные числа кадров и таймлайны.

`SpriteSheet::GetSpriteOffset(frame)` читает обычное смещение спрайта до рута из
`_sprOffset`. `GetSpriteMotion()[frame]` читает покадровую дельту визуального переноса
из `_sprMotion`. `SetFrame(index, sprite, offset, motion)` записывает каждый вход
в соответствующее поле. Копии листов содержат независимые значения.
`CritterHexView::RefreshAnimOffset` передаёт накопление дельт motion и согласование позы
в `AnimationMovement`, сохраняя выбранный лист и anchor относительно пути. Отдельного объекта motion и системы перемещения
персонажа по анимации нет.

`Game.GetSpriteFrameMotion` читает дельту кадра; `Game.CreateSpriteSequence` хранит
переданные `motions` отдельно от абсолютных draw offsets. Выбор кадров, накопление
для последней позы и склейка последовательностей принадлежат проекту.

Созданные листы разделяют исходные кадры `AtlasSprite`. Независимые абсолютные опоры
отрисовки хранятся в `SpriteSheet::_sprOffset` / `_sprMotion`. `SetAnimSpr` выбирает только
изображение и draw anchor. `RefreshAnimOffset` вместе выбирает позу по движению и её
визуальную компенсацию до применения изображения; frozen/timed кадры задаёт playback.
`RefreshOffs` однократно складывает
`HexOffset + extra displacement + _offsAnim`. MapSprite применяет draw anchor
только к размещению изображения, не добавляя его в motion или логический мировой
корень. Ни одно из этих значений не меняет прохождение пути.

<a id="когда-root-motion-управляет-криттером"></a>

## Когда движение выбирает кадр

Ветка движения в `CritterHexView::Process` активна, только когда выполняются все условия:

- криттер использует 2D-спрайт, а не загруженную 3D-модель;
- у криттера есть активный `MovingContext`;
- проект вернул `CritterAnimationFlags::Movement` без `Freeze` из `OnCritterAnimationInit`.

В этой ветке кадр выбирает позиция движения. Прошедшее время анимации не продвигает цикл ходьбы/бега независимо.

Без флага Movement `RefreshAnimOffset` накапливает offsets до выбранного кадра включительно в `_offsAnim`. Это позволяет авторскому action визуально смещаться, пока логический криттер остаётся привязанным к месту. Sheet walk/run без активного движения не считается самостоятельной командой движения сущности.

## Непрерывный displacement движения

`EvaluateMovementDisplacement` измеряет текущую целочисленную пиксельную позицию криттера относительно начала активного `MovingContext`:

```text
pos = GetHexOffset(start_hex, current_hex)
    + current_hex_offset
    - start_hex_offset
```

Логический гекс и `HexOffset` при пересечении границы гекса меняются в противоположных направлениях, поэтому их сумма остаётся пространственно непрерывной. Благодаря этому animation phase переживает логические snap между гексами.

`MovingContext::BuildProgress` вычисляет плавную интерполяцию сегмента и округляет каждый компонент `HexOffset` до целого пикселя. Результат намеренно не ограничивается одним гексом. Округлённый прогресс пути может отставать от плавной позиции, а client reconciliation при быстром stop/start может включить межгексовую delta, поэтому корректный движущийся offset способен охватывать больше одного гекса. Отображение остаётся правильным, потому что позиция спрайта равна сумме `current_hex + offset`; clamp нарушил бы этот invariant.

## Остановка и нормализация offset

`SetMoving` и `StopMoving` очищают anchor sheet и phase displacement. Затем `StopMoving` вызывает `CritterHexView::NormalizeHexOffset`, чтобы перенести целые гексы из потенциально большого `HexOffset` в логический гекс, сохранив отображаемую мировую позицию.

`GeometryHelper::NormalizeHexOffset` заново представляет ту же пиксельную позицию как ближайший логический гекс плюс внутригексовый остаток. `CritterHexView` передаёт predicate, отклоняющий поля карты с флагом `MoveBlocked`. Клиент соблюдает следующие правила:

- если нормализация остаётся на текущем гексе, остаток можно обновить без проверки проходимости;
- если ближайший целевой гекс находится внутри карты и проходим, логический гекс и остаток обновляются, после чего offsets спрайта обновляются;
- если цель находится вне карты или заблокирована, нормализация отклоняется, а существующая пара гекс/offset остаётся без изменений;
- оба успешных представления рисуются в одной мировой позиции, поэтому обычная остановка не создаёт визуального скачка.

Не ограничивайте live movement offset, имитируя этот lifecycle step. Multi-hex offsets корректны во время интерполяции; нормализация является stop-time re-partition с проверкой карты.

## Anchor и фаза цикла

`CritterHexView` хранит anchor для конкретного sheet:

```cpp
nptr<const SpriteSheet> _walkAnchorAnim;
ipos32 _walkAnchorDisp;
```

Для активного sheet:

```text
rel             = pos - anchor
rel_dot_total   = dot(rel, T)
total_dot_total = dot(T, T)
cycle_proj      = rel_dot_total mod total_dot_total
```

Отрицательный результат modulo переносится в `[0, total_dot_total)`. Поэтому `cycle_proj / total_dot_total` задаёт текущую phase в `[0, 1)` без floating-point projection.

Первый sheet нового движения получает anchor в текущем `pos`, то есть фазу ноль. Следующее движение получает новый anchor, потому что оба lifecycle-метода очищают предыдущий sheet и displacement.

При смене листа `RefreshAnimOffset` устанавливает привязку до выбора кадра и расчёта компенсации, включая `RefreshView` перед следующим `Process`. Чистая математика находится в `AnimationMovement`, состояние привязки — в `CritterHexView`; изображение и его draw anchor в расчёты не входят.

## Выбор кадра

`AnimationMovement::EvaluateWalkFrame` проецирует каждое накопленное авторское смещение на `T`:

```text
accum_dot_total[i] = dot(accum[i], T)
```

Выбирается первый кадр с наименьшим абсолютным расстоянием:

```text
abs(accum_dot_total[i] - cycle_proj)
```

Таким образом выбирается авторская pose, ближайшая к текущей позиции криттера вдоль оси цикла. Алгоритм использует целочисленную арифметику `int32_t` / `int64_t` для displacement и dot products.

Если `T` равен нулю, `dot(T, T)` также равен нулю и выбирается кадр `0`. Ветка offset walk/run также оставляет `_offsAnim` нулевым. Цикл с нулевой суммой является допустимыми fallback-данными, но не может пространственно управлять циклом ходьбы.

## Отображаемый offset

Для ненулевого `T` функция `AnimationMovement::EvaluateWalkFrame` вычисляет целый номер цикла floor division, включая отрицательный проецируемый displacement:

```text
cycle_number = floor(rel_dot_total / total_dot_total)
cycle_start  = anchor + cycle_number * T
offs_anim    = (cycle_start - pos) + accum[i]
```

Размещение map sprite включает логический гекс и `HexOffset`. `offs_anim`
компенсирует линейное движение внутри кадра и ставит спрайт в накопленный
авторский offset. Смена кадра продвигает визуал на следующую delta, пока
логическое движение продолжается.

Это выравнивание, а не locomotion, управляемая анимацией. Изменение `Motion` меняет визуальный шаг и выбор фазы, но не заставляет server-side entity двигаться дальше или быстрее.

## Смена направления и sheet

Поворот внутри одного `MovingContext` обычно выбирает другой direction-specific `SpriteSheet` с иным `T` и, возможно, числом кадров. Перезапуск с нулевой фазы визуально сбрасывал бы походку на каждом повороте.

При смене выбранного листа `RefreshAnimOffset` вызывает `AnimationMovement::TransferWalkPhase` до выбора кадра и расчёта смещения, включая `RefreshView` до следующего `Process`. Владельцем нового anchor остаётся `CritterHexView`: он сдвигает привязку так, чтобы projection на новый cycle vector начиналась с той же доли:

```text
old_phase      = old_cycle_proj / dot(T_old, T_old)
new_anchor     = pos - round(old_phase * T_new)
```

Локальная функция модуля `ScaleWalkPhase` точно масштабирует компоненты с округлением к ближайшему целому со знаком (половины от нуля), без переполнения промежуточного числителя. Если прежний total vector равен нулю, сохранять нечего и новый sheet получает anchor в текущей позиции.

Логика действует для любой смены выбранного sheet walk/run, не только для изменения направления. Content substitution или resource selection, вернувшие другой sheet, проходят тот же re-anchoring path.

## Ошибки и fallback-поведение

| Условие | Текущий результат |
|---|---|
| Отсутствует FrameOffsets FOFRM | Корректировка размещения нулевая; импортированные root-motion дельты не меняются. |
| При сериализации Motion выходит за диапазон знакового 16-битного числа | Numeric conversion завершается ошибкой вместо тихого переполнения. |
| В запечённом спрайте нет кадров или направлений | Клиентская загрузка не проходит validation. |
| Запечённый кадр является sprite reference | Копируются спрайт и offset кадра по ссылке. |
| Сумма цикла walk/run `T` равна нулю | Выбирается кадр `0`, frame-offset offset движения остаётся нулевым. |
| Нет активного движения при Movement без Freeze | Ветка согласования не применяет поправку анимации. |
| Нет флага Movement | Offsets визуально накапливаются до выбранного кадра. |
| Криттер использует 3D-модель | Ветка смещений кадров 2D-спрайта пропускается. |
| Целевой гекс при остановке находится вне карты или заблокирован | Нормализация отклоняется; текущий логический гекс и offset сохраняются. |

## Матрица визуальной приёмки

| Маршрут | Собираемое evidence | Признак ошибки |
|---|---|---|
| Прямая ходьба и бег | Стабильный cadence на нескольких полных циклах каждой поддерживаемой скорости | Foot sliding, обратный порядок poses или периодический скачок |
| Все созданные направления | Правильный вектор displacement и сопоставимая длина цикла | Отражённая ось, диагональный drift или различающийся темп |
| Поворот во время движения | Phase продолжается при замене sheet | Походка перезапускается или спрайт скачет на каждом повороте |
| Быстрый stop/start | Логический гекс догоняет позицию без отображаемого скачка | Растущий offset, отделившийся idle sprite или цикл blocked-path resync |
| Граница гекса и reconciliation | Мировая позиция спрайта непрерывна при re-partition гекс/offset | Залипание на краю клетки или скачок на один гекс |
| Extraction, merge или substitution | Выбранный sheet сохраняет ожидаемые cumulative offsets по обеим осям | Первый добавленный кадр скачет или одна ось переносится неверно |
| Fallback с нулевой суммой | Стабильный кадр `0` без frame-offset offset | Деление на ноль, нестабильный выбор кадра или drift |
| Поддерживаемые zoom/renderers | Foot placement остаётся приемлемым в сцене проекта | Зависящий от zoom jitter или неприемлемый cadence |

Автоматические тесты baker и geometry подтверждают данные и нормализацию. Они не заменяют видимую клиентскую приёмку качества походки.

## Практики создания контента

1. `Motion` — покадровые дельты в пикселях, а не абсолютные координаты.
2. Проверяйте каждое направление и согласованность cycle vectors при поворотах.
3. Сохраняйте `T` ненулевым для locomotion sheets, пространственно управляющих походкой.
4. Согласуйте deltas с poses: большая коррекция одного кадра скачет даже при правильной сумме.
5. Проверяйте изменения offsets при mirroring, extraction, merges и first/last substitutions.
6. Настройте скорость авторитетным движением проекта, затем согласуйте визуальный цикл.
7. Проверяйте anchor transfer и normalization при быстрых поворотах и stop/start.
8. Проверяйте поддерживаемые zoom, geometry и renderers; приёмка cadence/foot sliding принадлежит проекту.

## Project evidence и правила извлечения

Закреплённый Last Frontier направляет 2D-работу через `Docs/ContentWorkflow.md`,
согласование Engine — через `Docs/DocumentationMaintenance.md`. В основном
`.fo3d` pipeline пропускает translation `*_RM` в `Docs/CharacterGenerator.md`:
Engine владеет мировым движением. Это не подтверждает значения 2D `Motion`.

Проверенные LF/TLA trees не содержат авторского FOFRM/FRM production cycle с
подтверждённым ненулевым displacement. TLA `Docs/Animation.md` исторически
объясняет projection; его `raw_ptr<const SpriteSheet>`/
`DefaultSpriteFactory::LoadAnimation` устарели. Текущие имена —
`nptr<const SpriteSheet>`/`DefaultSpriteFactory::LoadSprite`.

Извлекайте только концепции, записывайте точные paths/commits в evidence model,
выводите результат из Engine. Скорости, offsets и визуальная приёмка принадлежат проекту.

## Граница проекта

Engine владеет импортом исходных форматов, переносом запечённых offsets, sheet по направлениям, авторитетной интерполяцией движения, anchor/phase math, выбором кадра, финальным клиентским выравниванием и stop-time нормализацией гекса/offset.

Встраиваемый проект владеет sprite assets, композицией FOFRM, animation substitutions, state/action mapping, скоростями движения, выбором геометрии, визуальными критериями качества и регрессиями сцен. Руководство проекта может описывать конкретный art pipeline и tuning values, но должно ссылаться сюда, а не повторять runtime-алгоритм.

## Триггеры сопровождения

Повторно проверяйте эту страницу и focused tests, когда изменение затрагивает любую из следующих поверхностей:

- `FrameShot::Motion`, версию SpriteResource, `NextOffset`, shared-frame behavior или `SpriteSheet::_sprOffset` / `_sprMotion`;
- импорт FOFRM/legacy images, mirroring, cropping, frame extraction, animation merge, cloning или substitution;
- `MovingContext::BuildProgress`, интерполяцию гекс/offset, prediction reconciliation или `GeometryHelper::NormalizeHexOffset`;
- накопление дельт, проекцию цикла, перенос фазы, выбор кадра и визуальную компенсацию в `AnimationMovement`;
- lifecycle движения `CritterHexView`, активацию правилами проекта, владение anchor, `_offsAnim`, проходимость карты;
- обновление закреплённого Last Frontier или TLA, меняющее соответствующие assets, project policy или historical evidence.

При обновлении внешнего проекта или Engine сначала выполните fetch, запишите старый/новый диапазон commits, проверьте весь входящий диапазон, обновите `BuildTools/ExternalProjectEvidence.json`, перегенерируйте site/AI/localization artifacts в порядке зависимостей и сохраняйте независимость страницы от project-only helpers.

## Маршруты проверки

Для изменений документации:

```bash
python BuildTools/tests/test_docs_sprite_root_motion.py
python BuildTools/docs_external_evidence.py --check
python BuildTools/docs_localization.py --check
python BuildTools/docs_site.py --check
python BuildTools/docs_ai_delivery.py --check
python BuildTools/docs_validate.py
```

Для импорта offsets и переноса запечённых данных запустите тесты `ImageBaker` в Engine test binary встраиваемого проекта. `Test_ImageBaker.cpp` покрывает знаковые FRM offsets, ключи и разделы FOFRM в PascalCase, отклонение старых имён, вложенную композицию, mirroring, выбор кадров и baked round trips. `Test_Geometry.cpp` покрывает успешный re-split, отказ для заблокированной цели, поведение внутри текущего гекса, unrestricted normalization и отказ за пределами карты.

`CritterAnimationPreservesProjectSelectionWhileMoving` проверяет реальный потребитель криттера с произвольными идентификаторами, явным флагом движения и замороженными кадрами. После изменения `MovingContext`, `CritterHexView`, `SpriteSheet`, `CritterFramesCache` или выбора анимации проекта или созданных locomotion offsets выполните [матрицу визуальной приёмки](#матрица-визуальной-приёмки) в видимой клиентской сцене. Baking подтверждает data path, geometry tests - re-split, а видимая сцена - согласование походки.
