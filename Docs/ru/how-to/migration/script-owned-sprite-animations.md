---
layout: default
title: Перенос анимаций спрайтов в скрипты
locale: ru
document_id: script-owned-sprite-animations
permalink: /Docs/ru/how-to/migration/script-owned-sprite-animations.html
---

# Перенос анимаций спрайтов в скрипты
<!-- docs-translation: {"document_id":"script-owned-sprite-animations","locale":"ru","source_path":"Docs/en/how-to/migration/script-owned-sprite-animations.md","source_sha256":"32b915a498127e0bea5533e0e34fde1ee56e4fd5b0413980fc6d8405d7c0cd8c"} -->
### Скриптовые 2D-анимации криттеров

Удалите OnCritterAnimationFrames(modelName,stateAnim,actionAnim,pass,flags,ox,oy,animName) и
OnCritterAnimationFallout(modelName,stateAnim,actionAnim,fStateAnim,fActionAnim,fStateAnimEx,fActionAnimEx,flags).
Пути, суффиксы, pass, опоры, выбор кадров и склейку определяет проект; CritterFramesCache кеширует
последовательности/теги. Импорт FRM и OnCritterAnimationSubstitute прежние. Подпишитесь на
Game.OnCritterAnimationSprite(hstring,CritterStateAnim,CritterActionAnim,uint32&). Ноль допускает
замену; ненулевой SpriteSheet копируется, handle поглощается даже при StopChain/исключении.
Статические изображения оборачивают в последовательность; переданный handle не переиспользуют.

GetSpriteFrameCount/GetSpriteDirectionCount/GetSpriteDuration/GetSpriteFrameOffset/GetSpriteFrameMotion читают 2D-handle.
CreateSpriteSequence(sources,frames,offsets,motions,framesPerDirection,duration) принимает
равные непустые массивы по направлению, затем кадру. Sources — handle, frames — индексы с нуля;
источник даёт соответствующее либо своё единственное направление. Offsets — абсолютные
пиксельные опоры, motions — явные дельты с игровой компенсацией стыка. Длина делится на
положительное framesPerDirection; направлений одно либо полное число геометрии. Неверные
массивы/handle/индексы/типы и отрицательная/выходящая за int32 миллисекунд длительность дают
исключение. Timespan хранится целыми миллисекундами, ноль допустим. Результат остановлен,
независим, пиксели/геометрия атласа общие. Освободите источники; FreeSprite освобождает созданные
последовательности, именованные кеширует по имени/атласу. При выгрузке карты клиент удаляет
неактивные именованные handles перед очисткой кешей спрайтов/атласов; активные handles остаются
действительными. Отсутствующий ресурс даёт ноль.
FreeSprite удаляет handle до Stop. При ошибке остановки экземпляр освобождается,
а исключение получает прямой вызывающий. Cleanup резолвера диагностирует эту ошибку,
сохраняя результат резолвера либо его исходное исключение.
Вместо старого обработчика имён/флагов задайте выбор явно:

```angelscript
namespace Animations
{
[[ModuleInit]]
void Init()
{
    Game.OnCritterAnimationSprite.Subscribe(Resolve);
}

[[Event]]
void Resolve(hstring model, CritterStateAnim state, CritterActionAnim action, uint& sprId)
{
    uint source = Game.LoadMapSprite("Sprites/Actor.fofrm");

    if (source == 0) {
        return;
    }

    sprId = Game.CreateSpriteSequence(uint[] = {source}, int[] = {0},
        ipos[] = {Game.GetSpriteFrameOffset(source, 0, mdir(0))},
        ipos[] = {Game.GetSpriteFrameMotion(source, 0, mdir(0))},
        1, Game.GetSpriteDuration(source));

    Game.FreeSprite(source);
}
}
```

Managed: обновите SDK; подпишите `[Event] void Resolve(hstring model,CritterStateAnim state,CritterActionAnim action,ref uint sprId)` из `[ModuleInit]` через `Game.OnCritterAnimationSprite.Subscribe(Resolve)`. Те же вызовы принимают uint[], int[], ipos[], ipos[] и timespan; освободите входы, передайте результат.
Native: замените оба resolver на `ResolveCritterAnimationSprite(hstring,CritterStateAnim,CritterActionAnim) -> shared_ptr<SpriteSheet>`, возвращающий независимую последовательность или null, если анимация не найдена; сигнатура замены прежняя. `SpriteSheet::SetFrame(index, sprite, offset, motion)` сохраняет исходный общий спрайт и независимые обычное смещение и покадровые дельты motion; `GetSprOffset(index)` читает опору листа направления. Удалите нативные вызовы `AtlasSprite::CopyWithOffset` и его конструктора-обёртки; передавайте исходный общий спрайт и смещение отдельно. Сигнатуры скриптов, исходные смещения, пиксели и геометрия атласа не меняются.
Поиск: `rg -n 'OnCritterAnimationFrames|OnCritterAnimationFallout|ResolveCritterAnimationFrames|ResolveCritterAnimationFallout' <source-roots>`; старые члены перестают компилироваться. Обновите metadata/привязки/SDK, соберите client/Mapper/baker/compiler/server, перепеките scripts/metadata, перезапустите и упакуйте совпадающие выходы. Проверьте направления, позы, обе оси стыка, оружие/смерть/перезарядку, исходный кеш и видимую ходьбу. Test_SpriteSheet/Test_ClientEngine проверяют native; игровые ресурсы/видимую приёмку — проект.
### Выбор воспроизведения и превью принадлежит проекту

Native `CritterStateAnim` и `CritterActionAnim` содержат только `None=0`. Остальные значения объявляет проект в metadata с прежними числовыми идентификаторами.

`OnCritterAnimationInit` принимает `(Critter cr, CritterStateAnim& stateAnim, CritterActionAnim& actionAnim, AbstractItem? contextItem, CritterAnimationContext context, int32 movingSpeed, CritterAnimationFlags& flags, float32& normalizedTime, float32& speed)`. Контекст сообщает покой, движение вперёд/назад, поворот влево/вправо, действие из очереди или превью. Проект выбирает пару, флаг `Freeze`, нормализованное положение заморозки, скорость воспроизведения и флаг `Movement` для 2D root motion. Движок не выводит эти решения из состояния жизни, оружия или номера действия. None/0 означает отсутствие решения; fallback и подстановки определяет проект.

Прежние `Render.WalkAnimBaseSpeed`, `Render.RunAnimStartSpeed` и `Render.RunAnimBaseSpeed` перенесите в настройки проекта, если сохраняете эту политику. Явно задайте `Animation.PreviewStateAnim` и `Animation.PreviewActionAnim` для Mapper, AnimationViewer и базовых границ модели; defaults движка равны нулю. Baker выбирает указанную пару, а при её отсутствии — доступные границы, без эвристики Unarmed/Idle. После смены пары перепеките модели. Native callback ModelInstance принимает контекст, скорость движения и параметры воспроизведения; `GetMovingContext()` заменяет `GetMovingAnim()`.

### Обход и порядок отрисовки принадлежат проекту

`Critter.PathfindingObstacle` задаёт штраф обхода персонажа независимо от состояния жизни. Это настраиваемый штраф, не жёсткий запрет прохода. `Critter.DrawFlatten` выбирает слой `DrawOrderType.FlatCritter` с прежним числом 13. Defaults обоих свойств не задают игровые правила. Проект устанавливает обработчики инициализации/загрузки и изменения состояния. Для сохранения `DeadDrawNoFlatten` объявите persistent PublicSync bool в проекте и перенесите правило миграции `IsNoFlatten -> DeadDrawNoFlatten`.

Применяйте эту политику и в Mapper: инициализируйте загруженные карты через `OnEditMapLoad`,
новые экземпляры — через игровой обработчик инициализации анимации, а производные параметры
пересчитывайте в setter-ах свойств. Текстовые правки Mapper теперь вызывают обычные setter-ы
и post-setter-ы; `DrawFlatten` обновляет слой без ручной перестройки вида.

### Исторические сообщения и ресурсы

При использовании старых сообщений объявите `EngineInfoMessage.NetWrongTagSkill=1029` и `NetPermanentDeath=1049` в metadata проекта; транспорт сообщений остаётся непрозрачным. Настройки `View.GlobalDayColorTime` и `View.GlobalDayColor` удалены из нативного контракта. Удалите эти ключи конфига, храните кривую в коде/контенте проекта, вычисляйте цвета в скриптах и передавайте их через `Map.SetDayColors`. `Map.GlobalDayColor` остаётся отдельным общим свойством карты с текущим цветом отрисовки. Шрифт Fallout и радиационные значки больше не поставляются из ресурсов движка; нужные файлы перенесите в проектные input packs с сохранением путей загрузки. `Android.Icon` необязателен и по умолчанию пуст: без него сохраняется значок платформы, явный PNG проверяется и копируется во все launcher densities.

База aafc7fe74b (2026.1.57-dev), цель 2026.1.58-dev: runtime 0.0.76 запрещает смешанные выходы. Контейнер sprite v2 и Managed ABI 9 прежние. Сохранённые номера анимаций/сообщений и имена оставленных свойств сохраняют смысл; новые параметры обхода/отрисовки проект инициализирует при загрузке персонажей. Откат возвращает полный прежний набор binaries/scripts/metadata/pin вместе с игровой политикой. Старых игровых алиасов в движке нет.

### Владение ресурсами

`ResourceManager` владеет регистрацией фабрик, выбором по расширению, шаблонами
копируемых спрайтов, кешем неудачных путей, инвалидацией и повторными попытками.
`SpriteManager` владеет отрисовкой, атласами, батчами и обновлением живых спрайтов.
`FileSystem` владеет подключёнными источниками и чтением файлов. Фабрики по-прежнему
удерживают текстуры моделей и частиц для заимствующих их renderer callbacks; общего
кеша текстур это изменение не вводит и их время жизни не меняет.

Порядок создания: `SpriteManager`, `ResourceManager`, затем `CritterFramesCache`
и `FontManager`; уничтожение идёт в обратном порядке. Фабрики получают оба manager.
ResourceManager подписывается на начало сцены в прежней точке обновления фабрик,
перед обновлением живых спрайтов, и на очистку при нехватке памяти. Обе подписки
снимаются перед уничтожением кешей и фабрик, а также явно в `ClientEngine::Shutdown`.

`CritterFramesCache` кеширует разрешённые проектом направленные sheet и теги
анимаций. Изменяемые модели превью принадлежат `MapperEngine`. Имена файлов,
идентификаторы анимаций и правила склейки остаются у проекта.


Миграция C++: подключите ResourceManager.h для SpriteFactory и загрузки ресурсов; уберите FileSystem из конструктора SpriteManager, создайте `ResourceManager(ptr<FileSystem>, ptr<SpriteManager>)` и передайте его после `ptr<SpriteManager>` в Default/Model/ParticleSpriteFactory и FontManager. Вместо ResourceManager::IndexFiles() вызовите LoadDefaultSprites(settings). GetCritterAnimFrames/CleanupCritterFrames перенесены в `CritterFramesCache(ptr<AnimationResolver>)`; методы превью принадлежат Mapper. Updater и отдельные viewer получают явного владельца ресурсов. Этот перенос владения не меняет скриптовые сигнатуры, версию baked sprite или сохранённые данные; пересоберите нативные потребители вместе.

### Смещения спрайтов и покадровый motion

Термины различаются: offset — обычное смещение спрайта до рута; motion — авторская
покадровая дельта визуального переноса. SpriteSheet хранит их в `_sprOffset` и
`_sprMotion`. `SetFrame(index, sprite, offset, motion)` записывает каждый вход в
соответствующее поле. `GetSprOffset(frame)` читает offset, `GetSprMotion()[frame]` —
motion. Прежний нативный span дельт `GetSprOffset()` замените на `GetSprMotion()`.
Заимствованная необязательная подмена offset в рендерере задаётся через
`SetSpriteOffsetOverride`; владелец и fallback к Sprite::GetOffset сохраняются.

Публичные скриптовые имена сохраняют эти смыслы: `GetSpriteFrameOffset` читает
offset, `GetSpriteFrameMotion` — дельту motion, `CreateSpriteSequence` принимает
`offsets, motions` именно в таком порядке. Статическое изображение сохраняет свой offset и имеет нулевой motion.
Копии содержат независимые значения и разделяют исходные спрайты атласа.

`CritterHexView::RefreshAnimOffset` вычисляет по дельтам motion накопленный
визуальный перенос, выбор кадра по расстоянию, фазу цикла и компенсацию `_offsAnim`.
`SetAnimSpr` выбирает изображение и обычный offset; `RefreshOffs` один раз складывает
компенсацию, логическую интерполяцию и дополнительное смещение. Авторитет движения
остаётся у MovingContext. Смыслы NextX/NextY и SpriteResource v2 сохраняются.
Пересоберите нативные потребители вместе с согласованными игровыми скриптами
и метаданными для 2026.1.58-dev/runtime 0.0.76.
