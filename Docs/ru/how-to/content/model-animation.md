---
layout: default
title: Метаданные и длительность анимаций моделей
locale: ru
document_id: model-animation
permalink: /Docs/ru/how-to/content/model-animation.html
---

# Метаданные и длительность анимаций моделей

<!-- docs-translation: {"document_id":"model-animation","locale":"ru","source_path":"Docs/en/how-to/content/model-animation.md","source_sha256":"87210fa5e304b04865797e03928507322f11ac2dd143fea378d35c4e81a41886"} -->

Это руководство описывает переиспользуемый контракт FOnline, который преобразует исходные анимации моделей и пары `(state, action)` в клиентский runtime rig и общие метаданные эффективного цикла. Оно следует текущим реализациям source loader, converter, `ModelInfoBaker`, клиентского lookup, регистрации общих метаданных, script exports и тестов движка. Подключающий проект может определять свои имена моделей, правила использования animation enum, игровое время и fallback policy, но не должен повторно реализовывать lookup движка или разбирать приватные baked payloads.

## Статус контракта

Это принадлежащий Engine production-контракт для animation tuples в `.fo3d`, authored playback speed, одношаговых aliases, преобразования в canonical rig, метаданных эффективной длительности и typed duration lookup. Нормативны исходный код и тесты Engine. Last Frontier и FOnline TLA используются только как discovery и compatibility evidence; их assets, enum policy, timing policy и исторические tokens не расширяют грамматику.

Контракт можно использовать независимо от checkout любого из проектов. Project evidence записывается в `BuildTools/ExternalProjectEvidence.json`, а эта страница заново выводит каждое продвигаемое утверждение из текущего кода и тестов Engine.

## Область действия и авторитетные источники

Контрактом владеют:

- `Source/Tools/ModelSourceLoader.*`: извлечение source skeleton/TRS/clips, валидация и single-flight cache на один bake;
- `Source/Tools/ModelAnimationConverter.*` и `Source/Common/ModelAnimationData.*`: анализ canonical compatibility, Ozz conversion и versioned wire contract runtime rig;
- `Source/Tools/ModelInfoBaker.cpp`: animation tokens `.fo3d`, проверка dependencies и geometry, расчёт effective duration, materialization aliases, выходы `LFMODINF`/`LFOZZRIG` и `ModelAnimationInfo.foinfo`;
- `Source/Client/ModelInformation.*`, `ModelAnimation.*` и `ModelInstance.*`: строгая загрузка rig, одношаговый alias lookup, владение timeline/sampling/pose и duration query загруженного instance;
- `Source/Common/AnimationInfo.cpp`: decoding durations/bounds, а также `Source/Common/EngineBase.cpp`: регистрация и lookup общих метаданных;
- `Source/Scripting/CommonGlobalScriptMethods.cpp` и `Source/Scripting/ClientCritterScriptMethods.cpp`: script access;
- тесты model baker, source loader, animation data/converter/runtime, skeleton compatibility, Ozz, client engine и common script methods: исполняемые примеры и failure behavior.

Полная грамматика `.fo3d`, FBX/OBJ inputs, layers, attachments, transforms, materials, cuts, rendering controls и runtime composition описаны в [формате моделей](model-format.md). Эта страница владеет более узким контрактом tuples, aliases, speed, duration и script lookup; gameplay policy остаётся ответственностью проекта.

## Создание animation tuples

`ModelInfoBaker` запекает model descriptions `.fo3d`, не являющиеся templates. Basename, начинающийся с `TEMPLATE_`, является include-only template: после раскрытия `Include` его declarations влияют на конкретные model descriptions, но отдельная baked model или metadata section для него не создаётся.

В расчёте длительности участвуют следующие declarations:

```text
Anim <state> <action> <model-file> <animation-name>
AnimSpeed <state> <action> <positive-playback-factor>
AllowAnimationGeometry <external-animation-file>
StateAnimEqual <input-state> <resolved-state>
ActionAnimEqual <input-action> <resolved-action>
```

`<state>` и `<action>` должны разрешаться в текущие значения enum `CritterStateAnim` и `CritterActionAnim`. В authored files предпочитайте именованные enum entries: они показывают намерение и проверяются тем же name resolver, что и numeric values.

Declaration `Anim` выбирает один clip для tuple `(state, action)`:

```text
Anim CritterStateAnim::Unarmed CritterActionAnim::Walk ModelFile Walk
Anim CritterStateAnim::Unarmed CritterActionAnim::Run ANIM_Human.fbx Run
```

- `ModelFile` выбирает source из declaration `Model` текущего model description.
- Другой filename разрешается относительно файла `.fo3d`.
- `Base` выбирает первый animation clip в проверенном source asset.
- Ведущий `~` удаляется до проверки animation name и lookup длительности.
- Referenced baked mesh, текущий source file и animation clip должны существовать. Duplicate tuple declarations не являются механизмом override: для conversion и duration metadata выбирается первый tuple, поэтому каждый canonical tuple должен быть уникален.

`AnimSpeed` относится к тому же tuple и должен быть положительным с конечным reciprocal. Baker записывает effective authored cycle, а не исходную длину clip:

```text
effective_duration_ms = round((clip_duration_seconds / AnimSpeed) * 1000)
```

Например, clip длительностью 1 секунда с `AnimSpeed ... 2` даёт `500 ms`. Результат до округления должен быть конечным, больше нуля и не превышать signed 32-bit millisecond maximum. Положительный результат меньше миллисекунды, который округляется до нуля, отклоняется. Runtime movement-speed scaling применяется отдельно во время воспроизведения animation клиентом и не запекается в это значение.

### Преобразование source и исключения для geometry

`ModelInfoBaker` загружает выбранные source assets через один `ModelSourceAssetCache` на bake, проверяет freshness source и baked mesh, анализирует skeleton compatibility и преобразует выбранные clips в canonical runtime rig. Animation-only joints могут добавлять совместимые canonical joints, не становясь physical mesh bones. Несовместимые roots, parents, transforms, names, limits или clip data приводят к ошибке до записи output.

Внешний source из `Anim` должен содержать только transform hierarchy и animation. Drawable geometry запрещена, поскольку client rig не использует этот duplicate mesh. `AllowAnimationGeometry <file>` является точным временным исключением для исправления существующих exports:

- path разрешается из итогового конкретного `.fo3d`, как и внешний path `Anim`;
- он должен точно называть внешний source, выбранный первым effective tuple `Anim`;
- duplicate lines, duplicate resolved paths, невыбранные files и исключения, оставленные после удаления geometry, ломают bake;
- исключение не сериализуется и не влияет на runtime.

При удалении geometry из animation export сохраняйте helper/bone names, parents и tracks. Удаляйте исключение вместе с исправленным source, а не превращайте его в постоянный allowlist.

## Одношаговые aliases

`StateAnimEqual A B` означает, что input state `A` один раз заменяется на `B` до tuple lookup. `ActionAnimEqual` делает то же для action. State и action mappings независимы и оба применяются до lookup.

Правила намеренно совпадают с `ModelInformation::GetAnimationIndexEx`:

- каждая map проверяется один раз; aliases не обходятся рекурсивно;
- alias имеет приоритет над exact tuple, state или action которого является alias source;
- циклы поэтому описывают одношаговые swaps, а не recursive chains;
- alias input попадает в общие метаданные только тогда, когда его одношаговый результат достигает реального tuple с положительной длительностью;
- aliases без такого результата пропускаются.

Native regression использует следующий компактный пример:

```text
Anim 1 3 ModelFile Base
Anim 0 3 ModelFile Base
Anim 1 5 ModelFile Base
AnimSpeed 1 3 2
AnimSpeed 0 3 4
AnimSpeed 1 5 5
StateAnimEqual 0 1
ActionAnimEqual 3 5
ActionAnimEqual 5 3
ActionAnimEqual 4 6
```

Для base clip длительностью 1 секунда input `(0, 3)` один раз разрешается в `(1, 5)` и поэтому возвращает `200 ms`. Exact authored value `(0, 3)`, равное `250 ms`, недостижимо, потому что обе input components являются alias sources. Input action `4` разрешается в `6`, для которого tuple отсутствует, поэтому metadata entry не создаётся.

## Результат bake и распространение

`ModelInfoBaker::BakeFiles` создаёт два связанных output. Каждый конкретный `.fo3d` превращается в versioned `LFMODINF` с обязательным runtime payload `LFOZZRIG`, а выбранный набор моделей также создаёт `ModelAnimationInfo.foinfo` для общего lookup длительности и bounds. Metadata pass:

1. собирает каждый выбранный `.fo3d`, не являющийся template;
2. раскрывает includes, проверяет freshness dependencies source/baked mesh и читает clip durations из проверенных source assets;
3. вычисляет положительные effective durations;
4. materializes input tuples по одношаговым правилам aliases;
5. записывает model sections в детерминированном порядке source paths.

Generated metadata file содержит по одной section на каждый конкретный model path. Параллельные массивы `StateAnimations`, `ActionAnimations` и `DurationsMs` хранят effective durations. Bounds schema version 2 также хранит aggregate model bounds, idle-priority view bounds и параллельные per-animation bounds arrays. И это представление, и byte streams model-info/rig являются приватными baker/runtime contracts. Game scripts должны использовать typed engine methods, а не читать их.

Поведение не ветвится по resource `PackName`: любой resource pack, выбирающий `ModelInfo`, может создать таблицу. Размещайте результирующий pack на каждой runtime side, вызывающей общий duration API. Распространённая конфигурация отправляет metadata моделей серверу и клиенту, но оставляет mesh, texture и rendering assets только в client packs.

Если в выбранном source set нет конкретных `.fo3d`, duration metadata file не записывается. Concrete model без animation tuples с положительной длительностью не получает duration section, хотя его model-info output всё равно содержит обязательный проверенный rig payload текущей schema.

## Runtime и script lookup

`ModelInformation` строго загружает один immutable canonical skeleton, clip set, binding table, remaps, presence/nearest data и rest-pose contract из обязательного rig payload. Старые unversioned model-info data, missing/partial rigs, identity mismatches, malformed counts и trailing data отклоняются; fallback на legacy pose evaluator отсутствует. `ModelAnimation` скрывает Ozz objects за принадлежащим Engine runtime interface, а каждый `ModelInstance` владеет mutable controller timelines, sampled pose buffers, world matrices, linked children и procedural overrides. Shared `ModelHierarchy` остаётся physical mesh topology и никогда не получает mutable pose output.

`BaseEngine` регистрирует `ModelAnimationInfo.foinfo` при startup после prototypes и до завершения регистрации metadata. Регистрация проверяет versioned duration/bounds payload, уникальность model sections и tuples. Path каждой section регистрируется в hash storage движка и становится model key, используемым scripts.

Общий API доступен в server, client и mapper runtimes:

```angelscript
timespan cycle = Game.GetModelAnimDuration(
    modelName,
    CritterStateAnim::Unarmed,
    CritterActionAnim::Walk);
```

Метод возвращает baked effective duration для exact input tuple после materialization aliases. Нулевой `timespan` возвращается, если metadata file, model или tuple отсутствуют. Игра, использующая это значение для authoritative timing, должна определить и протестировать явный zero-duration fallback.

Доступный только в client/mapper метод `Critter.GetModelAnimDuration(state, action)` относится к другой границе. Он обращается к текущему загруженному 3D model instance, использует converted runtime clip/controller metadata и может продолжить lookup через настроенный на клиенте cross-model animation substitute path. Метод возвращает ноль для non-model critter или unresolved animation и выбрасывает исключение, если 3D support не собран. Используйте общий метод `Game`, когда серверу и клиенту нужна одинаковая baked duration локальной модели; используйте метод `Critter`, когда коду нужен именно результат rendered instance на клиенте.

## Матрица приёмки времени

| Слой | Что проверять | Обязательное доказательство |
|---|---|---|
| Authored tuple | Named state/action, выбранные source и clip, speed и intentional aliases | Focused source test и model bake |
| Baked metadata | Exact effective milliseconds, deterministic aliases, bounds и pack distribution | Model-baker tests и проверка через typed common API |
| Client pose | Converted rig, sampled pose, attachments, substitutions и visual cycle | Видимая representative client или viewer scene |
| Gameplay timing | Attack windows, movement cadence, effects и zero-duration fallback | Project-owned gameplay или integration test на каждой authoritative side |

Успешный bake доказывает grammar, source compatibility, conversion и metadata shape. Он не доказывает корректность visible pose или справедливость и синхронизацию gameplay timing policy. Эти acceptance claims должны оставаться раздельными.

## Поведение при ошибках

| Условие | Результат |
|---|---|
| Неизвестное значение enum state/action | Model bake завершается ошибкой. |
| `AnimSpeed <= 0` или его reciprocal не является конечным | Model bake завершается ошибкой. |
| Effective milliseconds не являются конечными, неположительны, превышают maximum `int32_t` или округляются до нуля | Model bake завершается ошибкой. |
| Referenced baked mesh, source file или animation name отсутствует либо устарел | Model bake завершается ошибкой. |
| Source skeleton/clip data malformed или несовместимы с canonical rig | Model bake завершается ошибкой с контекстом source/conversion. |
| Direct attached FBX содержит clips | Model bake завершается ошибкой; используйте child `.fo3d` с explicit mappings. |
| Внешний animation source содержит drawable geometry | Model bake завершается ошибкой, если exact selected source не имеет временной строки `AllowAnimationGeometry`. |
| Geometry exception является duplicate, невыбранным, duplicate-resolved или устаревшим | Model bake завершается ошибкой; сузьте или удалите исключение. |
| Source clip duration неположительна или не является конечной | Source/model validation завершается ошибкой до output. |
| Alias expansion создаёт duplicate output tuple | Model bake завершается ошибкой. |
| Нет concrete model или положительного tuple | File или model section пропускается. |
| `ModelAnimationInfo.foinfo` отсутствует при startup | Engine пишет informational message; общие lookups возвращают ноль. |
| Metadata arrays malformed, non-positive или duplicated | Runtime startup registration завершается ошибкой. |
| Model-info или rig payload отсутствует, старый, truncated, inconsistent либо имеет trailing data | Client loading завершается ошибкой; legacy runtime fallback отсутствует. |
| Model или tuple отсутствует во время query | Typed common lookup возвращает ноль. |

## Практики создания контента

1. Храните один canonical `Anim` tuple на каждую semantic state/action pair и используйте aliases только для intentional reuse.
2. Предпочитайте именованные значения `CritterStateAnim` и `CritterActionAnim`; не кодируйте project meaning в необъяснённых integers.
3. Выносите повторяющиеся families в include-only templates и сохраняйте concrete models достаточно небольшими для review.
4. Считайте `AnimSpeed` authored playback speed. Не путайте model property `Speed` или runtime movement scaling с tuple duration.
5. Делайте pack с `ModelAnimationInfo.foinfo` доступным каждой side, вызывающей `Game.GetModelAnimDuration`.
6. Используйте typed API. Никогда не привязывайте game scripts к приватному layout duration или bounds arrays.
7. Сделайте zero handling явной project policy. Missing metadata не должна молча превращаться в произвольную timing constant.
8. Проверяйте и model bake, и gameplay path, использующий duration, особенно для attacks, movement cadence и effects.
9. Не храните geometry во внешних animation sources. Используйте `AllowAnimationGeometry` только как временный точный migration debt и удаляйте его вместе с исправленным export.
10. При удалении duplicate geometry сохраняйте helper/bone hierarchy и animation tracks; animation-only canonical joints поддерживаются, silent branch pruning не поддерживается.
11. После изменения source loader, converter, wire, skeleton или source animation выполняйте force bake, затем incremental bake, подтверждающий стабильность dependency timestamps.

## Project evidence и правила извлечения

Last Frontier сейчас централизует named animation tuples, все declarations tuple `AnimSpeed`, пять state aliases и точные временные geometry exceptions в `Resources/CrittersArt/Critters/TEMPLATE_HumanAnimations.fo3d`. Concrete models, например `CR_HumanMaleNormal.fo3d`, включают этот template; vehicles `VH_Jagger.fo3d` и `VH_Snowmobile.fo3d` используют текущие declarations `ActionAnimEqual`. Переиспользуемые уроки: named enums, обозримые include templates, deliberate one-step aliases, точная настройка speed и временный geometry debt. Project-specific clip catalog, values, combat semantics и asset layout остаются вне этого контракта.

FOnline TLA даёт полезное compatibility evidence через `_VBMob.fo3d`, `_VBWeapon.fo3d`, `_VBHuman.fo3d` и concrete includes, например `VbDog.fo3d`. Его named tuples `Anim` и template composition остаются полезными наблюдениями. Однако `_VBHuman.fo3d` также содержит исторические bare declarations `AnimEqual`, которые текущий parser Engine не принимает; текущие aliases называются `StateAnimEqual` и `ActionAnimEqual`. Его model-level declarations `Speed` не являются tuple `AnimSpeed`. Не копируйте ни одну из этих исторических форм в новый контент.

При изменении revision подключающего проекта или TLA обновляйте evidence snapshot и заново проверяйте все затронутые files. Продвигайте только behavior, подтверждённое текущим исходным кодом и тестами Engine. Полезную project policy записывайте в документации соответствующего проекта, а отклонённые legacy patterns сохраняйте как migration evidence, не нормализуя их молча в контракт Engine.

## Граница проекта

Engine владеет token parsing, проверкой source/dependencies и compatibility, conversion в canonical rig, Ozz-backed runtime sampling, расчётом effective duration, одношаговыми local aliases, baked metadata, startup registration и typed lookups.

Подключающий проект владеет concrete model paths и assets, выбором state/action, настройкой cross-model substitutes, компоновкой resource packs, combat или movement timing policy, fallback behavior и semantic tests. Project documentation должна ссылаться сюда для переиспользуемого механизма и описывать рядом только решения проекта.

## Триггеры сопровождения

Обновляйте это руководство, его focused test, documentation manifest и external evidence в одном change при изменении любой из следующих surfaces:

- parsing animation tokens `.fo3d`, enum resolution, duplicate handling, source selection или validation `AnimSpeed`;
- source loading, skeleton compatibility, Ozz conversion, wire contracts rig или model-info;
- one-step alias lookup, materialization duration, bounds output, pack distribution или startup registration;
- `Game.GetModelAnimDuration`, `Critter.GetModelAnimDuration`, substitutions или zero-result behavior;
- pinned revision Last Frontier или TLA, меняющая cited evidence files.

До редактирования prose заново запускайте source-derived checks. Входящий project commit может быть evidence практики или потребности миграции, но сам по себе никогда не является достаточным доказательством behavior Engine.

## Маршруты проверки

Для изменений документации:

```bash
python BuildTools/tests/test_docs_model_animation.py
python BuildTools/docs_external_evidence.py --check --verify-sources --last-frontier-root ../ --tla-root ../Workspace/fonline-tla
python BuildTools/docs_api.py --check
python BuildTools/docs_reference.py --check
python BuildTools/docs_site.py --check
python BuildTools/docs_ai_delivery.py --check
python BuildTools/docs_validate.py
```

Для behavior движка соберите engine unit-test target подключающего проекта. Запустите focused cases model baker, mesh data, source loader, animation data/converter/runtime, procedural pose, skeleton compatibility, Ozz, client engine и `ModelAnimationInfoLookup`, затем полный unit-test target перед интеграцией revision Engine.

После изменения animation declarations `.fo3d`, enum metadata, model sources, wire contracts mesh/rig, source loading/conversion, `ModelInfoBaker`, metadata registration или любого script method выполните force rebake затронутого подключающего проекта, а затем incremental bake. Выполните project path, использующий animation или duration: успешный resource bake доказывает syntax/conversion/assets, а visible gameplay или integration testing доказывает pose, attachments, timing и presentation.
