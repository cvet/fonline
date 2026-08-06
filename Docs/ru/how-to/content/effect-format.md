---
layout: default
title: Формат эффектов и runtime шейдеров
locale: ru
document_id: effect-format-guide
permalink: /Docs/ru/how-to/content/effect-format.html
---

<!-- docs-translation: {"document_id":"effect-format-guide","locale":"ru","source_path":"Docs/en/how-to/content/effect-format.md","source_sha256":"6c1d1e311816be8c4049c69ba79b24e3c7beab13bccbedca344196cfcbcacb01"} -->

# Формат эффектов и runtime шейдеров

FOnline использует файлы `.fofx` для авторских GPU-эффектов. Один исходный файл
объединяет состояние рендеринга, один или несколько проходов вершинного и
фрагментного шейдера, принадлежащие Engine шейдерные ресурсы и данные для
запекания backend-специфичных артефактов шейдера.

Используйте это руководство для авторинга и понимания runtime-поведения. Точный
контракт текущей ревизии приведён в сгенерированном
[справочнике формата эффектов](../../reference/effect-format/index.md), его
отдельных страницах о [состоянии рендеринга](../../reference/effect-format/render-state.md),
[ресурсах](../../reference/effect-format/resources.md),
[запекании](../../reference/effect-format/baking.md) и
[runtime](../../reference/effect-format/runtime.md), а также в
[канонической JSON-модели](../../../generated/effect-format.json).

## Область и источник истины

Поведение определяют:

- `Source/Tools/EffectBaker.cpp`: разбор `.fofx`, компиляция шейдеров,
  reflection, backend-варианты и диагностика;
- `Source/Frontend/Rendering.h` и `Source/Frontend/Rendering.cpp`: форматы
  вершин, встроенные buffer-ы, состояние проходов/рендеринга и загрузка
  отражённых ресурсов;
- `Source/Client/EffectManager.cpp`: кеширование по пути, эффекты по умолчанию,
  `ScriptValueBuf` и обновления buffer-ов каждого кадра;
- `Source/Client/Client.cpp` и
  `Source/Scripting/ClientGlobalScriptMethods.cpp`: выбираемые скриптами эффекты
  и API script value;
- `Source/Frontend/Rendering-*.cpp`: загрузка шейдеров backend-ов, pipeline-ы,
  привязка дескрипторов и отрисовка;
- `Source/Tests/Test_EffectBaker.cpp`: исполняемые примеры запекания и ошибок.

Эта страница является переиспользуемой документацией Engine. Подключаемый
проект владеет каталогом эффектов, порядком переопределения resource pack,
профилем качества шейдеров, художественным направлением, конкретными
назначениями `EffectType` и всей семантикой, присвоенной `ScriptValueBuf`.

`BuildTools/EffectFormatInterface.json` является структурированным контрактом,
закреплённым за исходниками. `BuildTools/docs_effect_format.py` проверяет его
source anchors, выводит значения compile-time ограничений из CMake-интерфейса
проекта и формирует сгенерированный справочник. Изменение parser-а, baker-а,
ресурсов или runtime должно обновлять эту модель в той же правке.

## Минимальный эффект

Минимальный однопроходный эффект не текстурированного quad может выглядеть так:

```ini
[Effect]

[VertexShader]
layout(set = 0, binding = 0, std140) uniform ProjBuf
{
    mat4 ProjMatrix;
};

layout(location = 0) in vec3 InPosition;

void main(void)
{
    gl_Position = ProjMatrix * vec4(InPosition, 1.0);
}

[FragmentShader]
layout(location = 0) out vec4 FragColor;

void main(void)
{
    FragColor = vec4(1.0);
}
```

Файл обязан содержать `[Effect]` и пригодный исходный код вершинного и
фрагментного шейдера для каждого объявленного прохода. Baker сам добавляет
директиву версии, квалификатор точности и compile-time define-ы Engine.

## Структура файла

`.fofx` использует parser Engine `ConfigFile` в режиме сбора содержимого. Ключи
конфигурации находятся в `[Effect]`; тела секций шейдеров собираются как
необработанный текст.

### `[Effect]`

Эта обязательная секция содержит:

- `Version`;
- `Passes`;
- `ShadowPass` в 3D-сборках;
- `BlendFunc`;
- `BlendEquation`;
- `DepthWrite`;
- `DepthFunc`;
- `DepthVariants`;
- `CullVariants`;
- варианты ключей смешивания и глубины с суффиксом `_PassN`.

Runtime повторно разбирает эту секцию из запечённой копии исходного файла
`.fofx`. Поэтому состояние рендеринга хранится не только в бинарных файлах
шейдера или метаданных reflection.

### `[ShaderCommon]`

Эта необязательная секция необработанного текста добавляется перед обеими
стадиями каждого прохода. Используйте её для констант и вспомогательных функций,
общих для стадий или проходов эффекта.

В `.fofx` нет директивы include. Храните переиспользуемый код в
`[ShaderCommon]`, осознанно дублируйте его между разными ресурсами эффектов либо
генерируйте проектные эффекты вне формата Engine, если такой процесс принадлежит
проекту.

### Стадии шейдера и fallback прохода

Для прохода `N` baker ищет стадии в следующем порядке:

1. `[VertexShader PassN]`, затем `[VertexShader]`;
2. `[FragmentShader PassN]`, затем `[FragmentShader]`.

Пустая секция конкретного прохода считается отсутствующей и использует общую
секцию. Если исходный текст стадии не найден ни в одной из них, запекание
завершается ошибкой.

Нумерация проходов начинается с единицы. Не смешивайте два варианта записи:

- секция шейдера: `[FragmentShader Pass2]`;
- ключ состояния: `BlendFunc_Pass2`.

## Состояние рендеринга

### Число проходов и версия шейдера

Значение `Passes` по умолчанию равно `1` и должно находиться в диапазоне
`1..FO_EFFECT_MAX_PASSES` (значение Engine по умолчанию `6`). Оно определяет
компиляцию каждой стадии, артефакты метаданных и backend-объекты проходов.

Значение `Version` по умолчанию равно `310`. Baker формирует:

```glsl
#version 310 es
precision highp float;
```

Не добавляйте вторую директиву `#version` в авторский текст шейдера.

### Состояние смешивания

Значение `BlendFunc` по умолчанию:

```ini
BlendFunc = SrcAlpha InvSrcAlpha
```

Оно обязано содержать ровно два фактора, первым идёт исходный:

`Zero`, `One`, `SrcColor`, `InvSrcColor`, `DstColor`, `InvDstColor`,
`SrcAlpha`, `InvSrcAlpha`, `DstAlpha`, `InvDstAlpha`, `ConstantColor`,
`InvConstantColor` или `SrcAlphaSaturate`.

Значение `BlendEquation` по умолчанию равно `FuncAdd`. Допустимы `FuncAdd`,
`FuncSubtract`, `FuncReverseSubtract`, `Max` и `Min`.

Для переопределений конкретного прохода используйте `BlendFunc_PassN` и
`BlendEquation_PassN`. Неизвестные значения приводят к ошибке создания эффекта.

### Состояние глубины

Значение `DepthWrite` по умолчанию равно `True`. Значение `DepthFunc` по
умолчанию равно `Always`; допустимы сравнения `Always`, `Never`, `Less`,
`LessEqual`, `Equal`, `GreaterEqual`, `Greater` и `NotEqual`.

Для переопределений конкретного прохода используйте `DepthWrite_PassN` и
`DepthFunc_PassN`.

Состояние глубины действует для `EffectUsage::QuadSprite` и, в 3D-сборках,
`EffectUsage::Model`, если у цели есть depth attachment. UI, примитивы, свет и
финальные blit-проходы могут рисоваться в цели, где состояние глубины не влияет
на результат.

Значение `DepthVariants` по умолчанию равно `False`. Включайте его, только если
runtime-вызывающий код должен выбирать проверку/запись глубины для каждого
вызова отрисовки, как это делают узлы emitter-ов Effekseer. Эффект с этой опцией
строит четыре состояния: test/write, test/no-write, no-test/write и
no-test/no-write. Варианты с проверкой сохраняют авторский `DepthFunc`, а
варианты без проверки используют `Always`. Без этой опции отрисовка может
использовать только состояние, полученное из авторских `DepthWrite` и
`DepthFunc`.

Порядок глубины карты и backend-специфичное поведение описаны в
[Frontend и рендеринг](../../explanation/rendering/).

### Culling для отдельного вызова отрисовки

Значение `CullVariants` по умолчанию равно `False`. По умолчанию допустим только
`CullModeType::None`. Включайте опцию, когда модели или частицы должны выбирать
`None`, `Back` или `Front` для каждого вызова отрисовки. Backend-ы, запекающие
culling в состояние устройства или pipeline, создадут дополнительные варианты;
запрос не объявленного эффектом варианта завершится ошибкой, а не скрытой
отрисовкой с неверным состоянием.

### Теневой проход

В 3D-сборке значение `ShadowPass` по умолчанию равно `-1`. Укажите начинающийся
с единицы индекс прохода, чтобы пометить его как теневой проход модели. Индекс
проверяется относительно compile-time лимита проходов. Runtime-отрисовка модели
может отключить отмеченные теневые проходы, не отключая остальные.

## Окружение компилятора шейдеров

Каждый проход разбирается и связывается glslang как GLSL для Vulkan 1.0 и
SPIR-V 1.0. Перед `[ShaderCommon]` и телом стадии baker добавляет:

```glsl
#version <Version> es
precision highp float;
#define MAX_SCRIPT_VALUES <FO_EFFECT_SCRIPT_VALUES>
```

При активном `FO_ENABLE_3D` он также добавляет:

```glsl
#define MAX_BONES <FO_MODEL_MAX_BONES>
#define MAX_TEXTURES <FO_MODEL_MAX_TEXTURES>
```

Связанная программа обязана успешно построить reflection. Поэтому выходы
вершинной стадии и входы фрагментной должны иметь совместимые locations/types,
даже если отдельный backend допускает менее строгий исходный код.

## Контракт входных вершин

Effect usage фиксируется при первой загрузке пути. Входные locations шейдера
должны соответствовать этому usage.

### ImGui, QuadSprite и Primitive

Эти usage совместно используют `Vertex2D`:

| Location | Тип GLSL | Нативное поле | Значение |
|---|---|---|---|
| `0` | `vec3` | `PosX`, `PosY`, `PosZ` | позиция/глубина |
| `1` | `vec4` | `Color` | нормализованный цвет вершины |
| `2` | `vec2` | `TexU`, `TexV` | текстурные координаты |
| `3` | `vec2` | `EggFlags` | egg или вспомогательные данные draw path |

Шейдер может не объявлять неиспользуемые входы. Объявленные locations/types
должны оставаться совместимыми с таблицей.

### Model

`EffectUsage::Model` использует `Vertex3D`:

| Location | Тип GLSL | Значение |
|---|---|---|
| `0` | `vec3` | позиция |
| `1` | `vec3` | нормаль |
| `2` | `vec2` | основные текстурные координаты |
| `3` | `vec2` | базовые/вторичные текстурные координаты |
| `4` | `vec3` | tangent |
| `5` | `vec3` | bitangent |
| `6` | `vec4` | веса смешивания |
| `7` | `vec4` | индексы смешивания |
| `8` | `vec4` | нормализованный цвет вершины |

Модельные эффекты существуют только в 3D-сборках. Сохраняйте
`FO_MODEL_BONES_PER_VERTEX = 4`; активные backend-layout-ы проверяют эту форму.

## Контракт дескрипторов и привязок

### Нативное соглашение

Описывайте ресурсы по нативному соглашению Vulkan:

- descriptor set `0`: uniform buffer-ы;
- descriptor set `1`: combined image sampler-ы.

Binding-и являются явными целыми числами. Для нативного пути они не обязаны
быть плотными, но должны быть уникальны внутри одной стадии шейдера и класса
ресурсов.

Тесты EffectBaker содержат исходники без `set = ...`; glslang всё равно может
скомпилировать такие fixtures. В production-эффектах указывайте set-ы явно.
Нативный backend Vulkan использует исходный SPIR-V и не исправляет ошибочный
авторский descriptor set.

### Переназначение SDL_GPU

Baker копирует нативный SPIR-V и переписывает decorations дескрипторов:

| Стадия/ресурс | Descriptor set |
|---|---:|
| sampler-ы вершинной стадии | `0` |
| uniform buffer-ы вершинной стадии | `1` |
| sampler-ы фрагментной стадии | `2` |
| uniform buffer-ы фрагментной стадии | `3` |

Внутри каждого класса/стадии авторские binding-и сортируются и переназначаются
в плотные слоты `0..N-1`. Каждая стадия ограничена `16` sampler-ами и `4`
uniform buffer-ами. Переназначенный модуль сохраняется как `spv_sdl`; исходный
код SDL Metal также компилируется из этого модуля.

Отсутствующие binding-и, повторяющиеся binding-и одной стадии, storage image и
мёртвые объявления дескрипторов останавливают запекание: полное и
детерминированное переназначение для них невозможно.

## Текстуры, предоставляемые Engine

Runtime распознаёт следующие имена sampler-ов:

| Sampler | Доступность | Producer |
|---|---|---|
| `MainTex` | все сборки | основная текстура текущего спрайта/render target/модели |
| `IndoorMaskTex` | клиентские пути карты | текущая indoor mask |
| `BackgroundTex` | direct-scene пути с передачей вызывающим кодом | снимок сцены перед рефракционной отрисовкой |
| `ModelTex0..ModelTexN-1` | 3D-сборки | слоты текстур модели |

Неизвестные имена sampler-ов попадают в reflection, но `RenderEffect` не имеет
для них producer-а Engine. Поэтому исходник может скомпилироваться, а runtime
оставит sampler непривязанным. Считайте таблицу authoring allowlist, пока
изменение renderer/runtime не добавит producer в той же правке.

## Встроенные uniform buffer-ы

Имена uniform block-ов и их layout в байтах фиксированы. EffectBaker сравнивает
отражённые размеры с нативными структурами и отклоняет неизвестные блоки.

### Общие buffer-ы

| Блок | Форма GLSL | Producer/значение |
|---|---|---|
| `ProjBuf` | `mat4 ProjMatrix` | текущая 2D- или 3D-проекция |
| `MainTexBuf` | `vec4 MainTexSize` | ширина, высота, обратная ширина/высота |
| `EggBuf` | `vec4 EggData[3]` | две egg mask и параметр перехода |
| `SpriteBorderBuf` | `vec4 SpriteBorder` | UV-прямоугольник спрайта в атласе |
| `ParticleSamplingBuf` | `vec4 ParticleSampling` | параметры выборки частиц, ограничения атласа, искажения и ориентации фона для отдельной отрисовки |
| `TimeBuf` | `vec4 FrameTime; vec4 GameTime` | секунды в `.x`, отсчитываемые от сессии и оборачиваемые на `8192` |
| `RandomValueBuf` | `vec4 RandomValue` | четыре значения кадра в диапазоне `[0,1]` |
| `ScriptValueBuf` | `vec4 ScriptValue[MAX_SCRIPT_VALUES / 4]` | управляемые проектом float-слоты |
| `CameraBuf` | `vec4 MapAnchorScreenPos; vec4 ChunkScreenAnchor` | аффинные UV-базисы мира/экрана |

Для `CameraBuf` вычисляйте любой базис так:

```glsl
vec2 uv = Basis.xy + TexCoord * Basis.zw;
```

`MapAnchorScreenPos` привязан к миру и не зависит от zoom.
`ChunkScreenAnchor` привязан к экрану. Не сводите ни один из них к вычитанию:
масштабные компоненты учитывают дополненные/разбитые на chunks render target-ы.

`ParticleSamplingBuf` заполняется runtime-ами частиц, а не общим путём спрайтов.
Значение его компонентов принадлежит выбранной паре эффекта/runtime частиц;
стандартные эффекты Effekseer используют их для point sampling, безопасного
ограничения атласа, интенсивности искажения и вертикальной ориентации
`BackgroundTex`.

### Buffer-ы моделей

| Блок | Форма GLSL | Значение |
|---|---|---|
| `ModelBuf` | `vec4 LightColor; vec4 GroundPosition; mat4 WorldMatrices[MAX_BONES]` | освещение, ground anchor, матрицы skinning |
| `ModelTexBuf` | `vec4 TexAtlasOffset[MAX_TEXTURES]; vec4 TexSize[MAX_TEXTURES]` | преобразования атласа и размеры текстур |
| `ModelAnimBuf` | `vec4 AnimNormalizedTime; vec4 AnimAbsoluteTime` | нормализованное и зацикленное абсолютное время анимации |

Storage buffer-ы и storage image не поддерживаются.

## Владение и время жизни ScriptValueBuf

Значение `FO_EFFECT_SCRIPT_VALUES` по умолчанию равно `16`; подключаемый проект
может его переопределить. Значение должно быть положительным и делиться на
четыре. Это compile-time форма Engine, которая обязана совпадать со
сгенерированным/запечённым define-ом шейдера.

Runtime ведёт себя так:

1. Первая загрузка эффекта, объявляющего `ScriptValueBuf`, создаёт обнулённый
   buffer.
2. Скриптовые записи изменяют кешированный объект `RenderEffect`.
3. Значения сохраняются до перезаписи или явной очистки.
4. `Game.SetEffect(...)` меняет выбранный объект, но не очищает значения ни
   старого, ни нового объекта.
5. Возврат к ранее загруженному пути возвращает прежнее содержимое его buffer-а.
6. `Game.ClearEffectScriptValues(...)` обнуляет весь buffer выбранного объекта.

Ключом кеша служит только путь ресурса. Если один путь выбран в нескольких
совместимых слотах, они совместно используют один buffer. Проект должен
назначить каждому диапазону слотов одного владельца и документировать коллизии.
Повторная отправка значений после смены варианта остаётся хорошей проектной
практикой, когда варианты используют разные пути или объявления buffer-а, но
`SetEffect` не гарантирует сброс. SetEffect does not reset cached values.

## Runtime-загрузка и идентичность кеша

`EffectManager::LoadEffect(usage, path)` возвращает существующий кешированный
объект, если путь загружался раньше. Ключ кеша не включает `EffectUsage`.
Следовательно, первая загрузка фиксирует usage объекта и допущения backend-а о
pipeline и формате входных данных.

Не используйте один путь для несовместимых категорий:

- `ImGui`;
- `QuadSprite`;
- `Primitive`;
- `Model`.

Разные ресурсы могут содержать одинаковый текст шейдера, если им нужны разные
usage. Ясность путей важнее устранения небольшого дублирования исходника.

Runtime загружает:

- запечённый исходник `.fofx` для числа проходов и состояния рендеринга;
- `.fofx-N-info` для отражённых нативных и SDL-слотов ресурсов;
- вариант шейдера, требуемый активным renderer-ом.

Отсутствующий исходник, метаданные или вариант шейдера является ошибкой
загрузки.

## Скриптовый API

Поверхность скриптов клиента/mapper:

```angelscript
Game.SetEffect(effectType, effectSubtype, effectPath);
Game.SetEffectScriptValue(effectType, effectSubtype, valueIndex, value);
Game.SetEffectScriptValues(
    effectType,
    effectSubtype,
    valueStartIndex,
    values,
    valuesOffset = 0,
    valuesCount = -1);
Game.ClearEffectScriptValues(effectType, effectSubtype);
```

При пустом пути `SetEffect` восстанавливает значение слота по умолчанию.
Непустой путь загружается с usage эффекта этого слота по умолчанию.

Вызовы script value разрешают выбранную сейчас цель и завершаются ошибкой,
когда:

- type/subtype не поддерживается или некорректен;
- целевая entity или offscreen-слот не существует;
- эффект не загружен;
- эффект не объявляет `ScriptValueBuf`;
- входной или целевой диапазон выходит за границы.

Используйте ranged-метод для блоков параметров, обновляемых вместе. Он проверяет
`valuesOffset`, выводит оставшееся число элементов при `valuesCount = -1` и
выполняет одну нативную запись.

Записи ScriptValue для отдельных шрифтов не поддерживаются;
`EffectType::Font` принимает только subtype `-1` для общего эффекта шрифтов.
`GenericSprite` и `CritterSprite` могут выбирать общий слот с subtype `0` либо
живую entity по id. Offscreen-subtype должен быть предварительно
зарегистрирован и загружен.

## Выходы запекания

Для каждого прохода и стадии EffectBaker создаёт:

| Вариант | Потребитель |
|---|---|
| `spv` | нативный Vulkan; источник кросс-компиляции GLSL/ES/HLSL |
| `spv_sdl` | путь SDL_GPU Vulkan |
| `glsl` | desktop OpenGL (`330`) |
| `glsl_es` | OpenGL ES/WebGL (`300 es`) |
| `hlsl` | Direct3D (Shader Model `4.0`) |
| `msl_mac` | SDL_GPU Metal на macOS |
| `msl_ios` | SDL_GPU Metal на iOS |

Схема имён:

```text
<path>.fofx-<pass>-<vert|frag>-<flavor>
<path>.fofx-<pass>-info
```

`[EffectInfo]` хранит общие для программы отражённые binding-и и доказывает
размеры встроенных uniform buffer-ов. `[EffectInfoSdl]` хранит плотные слоты
отдельных стадий и числа sampler/UBO. Исходник копируется в путь запечённого
ресурса.

## Resource pack и переопределения

Engine предоставляет эффекты минимального профиля в `Resources/Core/Effects/`
и bootstrap-эффекты в `Resources/Embedded/Effects/`. Подключаемый проект может
предоставить более поздний ресурс с тем же путём и перекрыть значение Engine по
умолчанию.

Переиспользуемые значения Engine по умолчанию должны оставаться
консервативными. Более богатые проектные копии могут требовать более высокий
аппаратный профиль, но проект обязан проверить каждый поставляемый backend и
сохранить намеренный fallback. Текущая карта слотов по умолчанию и политика
минимального профиля описаны в
[Frontend и рендеринг](../../explanation/rendering/).

## Практики авторинга

- Начинайте с ближайшего эффекта Engine с теми же `EffectUsage`, входами вершин
  и встроенными buffer-ами.
- Объявляйте descriptor set-ы и binding-и явно, даже если test fixture
  показывает, что glslang способен вывести set.
- Сохраняйте малое число проходов. Каждый проход умножает компиляцию стадий,
  метаданные, backend-объекты и работу отрисовки.
- Удаляйте неиспользуемые sampler-ы и uniform block-и. Мёртвые объявления
  дескрипторов отклоняются при переназначении SDL.
- Используйте только распознаваемые имена buffer-ов и точно копируйте их layout.
- Централизуйте слоты `ScriptValueBuf` в проектном коде и документации: один
  владелец на диапазон и стабильная семантика между вариантами шейдера.
- Для непрерывных снимков предпочитайте `SetEffectScriptValues`; когда важна
  семантика сброса, явно вызывайте `ClearEffectScriptValues`.
- Не используйте один путь для несовместимых effect usage.
- Поддерживайте периодическое время анимации. `TimeBuf` оборачивается на `8192`
  секундах; поддерживаемые скриптом float-часы следует оборачивать до потери
  точности.
- Проверяйте глубину и смешивание на цели, где действительно присутствуют
  нужные attachments и порядок отрисовки.
- Проверяйте минимальный аппаратный профиль, поддержку которого заявляет
  проект; успешная кросс-компиляция не гарантирует визуальный результат или
  поведение драйвера.

## Диагностика ошибок

| Симптом | Вероятная граница |
|---|---|
| ошибка отсутствующего Effect/vertex/fragment | написание секций, `Passes` или fallback исходника |
| диагностика компилятора шейдера | синтаксис GLSL, версия/профиль или интерфейс стадий |
| invalid uniform buffer size | порядок/тип полей или длина массива блока отличается от `RenderEffect` |
| invalid uniform buffer | неизвестное имя блока |
| ошибка explicit/duplicate binding | отсутствующий или конфликтующий локальный binding стадии |
| unused resource error | объявленный sampler/UBO оптимизирован или ни разу не читается |
| ошибка лимита стадии SDL | более 16 sampler-ов или 4 UBO в одной стадии |
| эффект загружается, но текстура пуста | имя sampler-а не имеет producer-а Engine либо неверны set/binding |
| запись script value выбрасывает исключение | неверная цель, незагруженный эффект, отсутствие `ScriptValueBuf` или неверный диапазон |
| script value появляется в другом слоте | оба слота разрешаются в один кешированный путь |
| возврат к эффекту восстанавливает старые настройки | ожидаемое сохранение кеша пути; явно очистите или перезапишите значения |
| работает только на одном renderer-е | различие backend-варианта, профиля, дескрипторов или глубины |

## Процесс проверки

После изменения исходника формата эффектов Engine, buffer-а, backend-а или
встроенного эффекта выполните:

```powershell
python BuildTools\docs_effect_format.py --write
python BuildTools\tests\test_docs_effect_format.py
python BuildTools\docs_effect_format.py --check
python BuildTools\docs_contract_diff.py --help
```

Запустите focused нативные тесты baker-а через настроенную unit-test цель
подключаемого проекта. Владеющий файл тестов:
`Source/Tests/Test_EffectBaker.cpp`. Изменения состояния renderer-а также
требуют проверки соответствующего backend-а и видимой сцены.

Для подключаемого проекта:

1. повторно сгенерируйте/настройте сборку, если изменился compile-time лимит;
2. запеките ресурсы;
3. запустите проектные validator-ы путей эффектов, назначений `EffectType` и
   владения ScriptValue;
4. запустите репрезентативные видимые сцены для каждого затронутого слота;
5. проверьте каждый поставляемый renderer/backend и минимальный аппаратный
   профиль;
6. сравните значения по умолчанию/fallback и переопределённые проектом эффекты.

## Checklist изменения

При изменении контракта Engine обновляйте вместе:

- `BuildTools/EffectFormatInterface.json`;
- `Docs/en/how-to/content/effect-format.md`;
- сгенерированные `Docs/generated/effect-format.json` и страницы справочника;
- `BuildTools/tests/test_docs_effect_format.py`;
- `Source/Tests/Test_EffectBaker.cpp` или владеющий тест renderer/runtime;
- [Baking Pipeline](../../explanation/content-pipeline/baking.md), когда меняется поведение
  выходов/компилятора;
- [Frontend и рендеринг](../../explanation/rendering/), когда меняется
  поведение runtime/backend;
- [GeneratedApiAndMetadata.md](../../reference/metadata/index.md) и
  [управление изменениями сгенерированного контракта](../../contributing/contract-change-management.md),
  когда меняется структурированный контракт или поверхность агрегированного
  diff;
- документацию/тесты подключаемого проекта для путей, семантики слотов,
  fallback и визуальной проверки.
