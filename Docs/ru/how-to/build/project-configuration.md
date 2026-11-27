---
layout: default
title: Конфигурация игрового проекта
locale: ru
document_id: project-configuration
permalink: /Docs/ru/how-to/build/project-configuration.html
---

# Конфигурация игрового проекта
<!-- docs-translation: {"document_id":"project-configuration","locale":"ru","source_path":"Docs/en/how-to/build/project-configuration.md","source_sha256":"41ef8bcec279c0bb8c6ded2e5d0d8f51ce9656989495b71713163fcbe22d96bd"} -->
Руководство показывает, как embedding project должен создавать `.fomain`,
resource packs и именованные sub-configs. Точная runtime model описана в
[Конфигурация и источники данных](../../reference/settings/configuration-and-data-sources.md), а
актуальные имена встроенных settings приведены в
[generated settings reference](../../../generated/api/settings.md).

## Проверенные исходные пути

- `Source/Common/Settings.h`
- `Source/Common/Settings.cpp`
- `Source/Common/Settings.inc`
- `Source/Frontend/ApplicationInit.cpp`
- `Source/Tests/Test_Settings.cpp`
- `BuildTools/cmake/stages/ScriptsAndBaking.cmake`
- `Examples/MinimalProject/CMakeLists.txt`
- `Examples/MinimalProject/FOnlineStarter.fomain`
- `Examples/MinimalMultiplayer/CMakeLists.txt`
- `Examples/MinimalMultiplayer/FOnlineMinimalMultiplayer.fomain`

## Начните с исполняемого baseline

Для первого headless-запуска используйте структуру
[MinimalProject](../../../../Examples/MinimalProject/README.ru.md), для
client/server игры:
[MinimalMultiplayer](../../../../Examples/MinimalMultiplayer/README.ru.md).
Сохраняйте игровой репозиторий корнем CMake и явно задавайте master config:

```cmake
include(Engine/BuildTools/Init.cmake)

SetOption(FO_MAIN_CONFIG "MyGame.fomain")
SetOption(FO_DEV_NAME "MYGAME")
SetOption(FO_NICE_NAME "My Game")
```

`FO_MAIN_CONFIG` является configure-time project option. Содержимое `.fomain`
задает runtime и baking settings. Не переносите product values в defaults
движка только ради отказа от сопровождения project file.

`Common.ResourcesPath` задаёт runtime-каталог ресурсов, default `Resources`.
Серверная клиентская раздача находится в его дочернем `Client`. CMake-параметров
каталога ресурсов нет. Packaged bootstrap читает только эту зашитую строку до
выбора одного полного профиля; CLI overrides сохраняются.

## Порядок приоритетов

Для unpackaged application settings применяются в таком порядке:

1. выбранный `.fomain`, найденный через `-ApplyConfig` или подъем по каталогам
   до `FO_MAIN_CONFIG`;
2. каждый явно выбранный `-ApplySubConfig` в порядке command line;
3. local config установленного клиента в writable cache, если он существует;
4. обычные command-line setting overrides;
5. platform/build auto-settings.

Packaged-запуск выбирает ресурсный профиль либо бинарный при отсутствии optional
ресурсов и применяет только его к нулевому объекту. Defaults перед ним не применяются.
Фиксированная область binary patch равна 40000 байт и проверяется при упаковке.
Профили содержат все ненулевые объявленные значения вместо metadata deltas;
пропущенные типизированные значения остаются нулём после загрузки metadata.
Затем следуют local config, CLI и auto settings. Packaged `-ApplySubConfig` сообщается через
`FO_VERIFY_AND_CONTINUE`; профиль выбирается при упаковке.

В авторском конфиге задавайте все обязательные настройки, включая намеренные нули.
Фильтрация принадлежит baker. `Baking.BootstrapGameSettings` сохраняет проверку имён;
все ненулевые game settings уже доступны в `ApplicationInitHook`.

В authored files и operational commands используйте fully qualified names,
например `Server.DbStorage`. Parser принимает короткие имена встроенных
settings, но qualified names делают ownership и review однозначными.

## Корневые settings

Записывайте по одному намеренному значению в строке:

```ini
Common.GameName = My Game
Common.GameVersion = 0.1.0

Network.ServerPort = 4000
Network.WebSocketPort = 4001

Server.DbStorage = Memory
ServerNetwork.DisableNetworking = False

Baking.BakeLanguages = engl russ
Baking.BakeOutput = Baking
Common.ResourcesPath = Resources
Common.PlatformBinariesPath = PlatformBinaries
Common.CachePath = Cache

ManagedScript.Assemblies = MyGame
ManagedScript.ProjectName = MyGame
ManagedScript.TargetFramework = net10.0
ManagedScript.MsBuild = dotnet msbuild
ManagedScript.Dirs = Engine/Source/Scripting/Managed/CoreScripts Scripts
ManagedScript.Analyzers = Engine/Source/Scripting/Managed/Analyzers/FOnline.Analyzers.csproj
```

Неизвестные имена становятся project custom settings, доступными через
`GetCustomSetting` / `FindCustomSetting`; config baker отмечает каждое как
`Unknown setting <name> = <value>`, не прерывая bake, и записывает его только в
baked server profile. Это намеренное поведение для
game-owned configuration, но опечатка в имени built-in setting поэтому может
выглядеть допустимой. Добавляйте focused project test для каждого content ID,
port/profile, prototype name, path или custom setting, влияющего на startup или
gameplay.

Значения с начальным `+` накапливаются вместо замены. Strings добавляются через
пробел, vectors получают новые elements, числовые значения складываются,
booleans используют logical OR, а enums: bitwise OR. Используйте это осознанно
и проверяйте итоговое значение, не предполагая list-only behavior.
`$ENV{NAME}` и `$FILE{path}` разрешаются при чтении authored config, в том числе
во время baking, поэтому конкретные значения могут попасть в generated internal
configs. `$TARGET_ENV{NAME}` и `$TARGET_FILE{path}` остаются directives во время
baking и разрешаются только тогда, когда их читает target application; текущий
packager не предоставляет общий resolver target directives. Не храните
credentials в tracked config, используйте target forms для runtime secrets и
следуйте [Security and Secrets](../release/security-and-secrets.md) для command-line,
logging, signing, CI, rotation и artifact boundaries.

## Определение resource packs

Resource pack выбирает inputs, bakers и runtime recipients:

```ini
[ResourcePack]
Name = Protos
InputDirs = Content Maps
IncludePatterns = **
ExcludePatterns = **/Draft/**
Bakers = Proto

[ResourcePack]
Name = Maps
InputDirs = Maps
IncludePatterns = **/*.fomap
Bakers = Map

[ResourcePack]
Name = ServerScripts
InputDirs = Scripts
IncludePatterns = **/*.fos
Bakers = AngelScript
ServerOnly = True

[ResourcePack]
Name = ManagedScripts
InputDirs = Engine/Source/Scripting/Managed/CoreScripts Scripts
IncludePatterns = *
Bakers = Managed
```

Выбирайте backend явно. Pack AngelScript запекает модули `.fos` через `AngelScriptBaker`; pack Managed компилирует настроенные top-level исходники `.cs` и generated API в target-specific assemblies. Managed pack должен включать Engine CoreScripts и проектные исходники из `ManagedScript.Dirs`; согласуйте с той же сборкой настройки assemblies, analyzers, extra sources/references и generated directory. Полный контракт backend описан в [Скриптах Managed C#](../scripting/managed-csharp.md).

Допустимые fields:

| Field | Значение |
|---|---|
| `Name` | Обязательная identity pack и generated resource entry |
| `ConfigDir` | Вычисляемый каталог владеющего config для разрешения относительных inputs; в секции не задаётся |
| `InputDirs` | Разделенные пробелами каталоги относительно owning config |
| `InputFiles` | Разделенные пробелами explicit files, также относительно config |
| `IncludePatterns` | Необязательный input glob allowlist |
| `ExcludePatterns` | Необязательный input glob denylist |
| `Bakers` | Разделенные пробелами имена bakers |
| `ServerOnly` | Создать только server resource entry |
| `ClientOnly` | Создать только client resource entry |
| `MapperOnly` | Создать только mapper resource entry |

Не более одного side-only flag может быть true. Без flags pack поставляется
server и client. Mapper-only packs отделены. `RecursiveInput` встречается в
старых project files, но не является текущим field `ResourcePackInfo` и
игнорируется: каждый input directory монтируется рекурсивно, а пустой
`IncludePatterns` выбирает все files, поэтому удалите этот key.

Разделяйте packs, если различаются ownership, release cadence, side visibility
или update policy. Не используйте pack order как скрытую систему gameplay
overrides: duplicate resource identities требуют явной project policy и test.

## Добавление именованных sub-configs

Sub-configs являются reviewed overlays для launch mode:

```ini
[SubConfig]
Name = LocalDev
Server.DbStorage = Memory
ServerNetwork.DisableNetworking = False
Render.RenderDebug = True

[SubConfig]
Name = TutorialSmoke
Parent = LocalDev
Tutorial.Automation = True
Render.HeadlessWindow = True
Render.NullRenderer = True
Audio.DisableAudio = True
```

Имена `Parent` должны ссылаться на более ранние sub-config sections. Несколько
parents применяются слева направо: поздние parents переопределяют ранние по key,
затем побеждает дочерняя section. Launch может передать несколько
`-ApplySubConfig`, они применяются в command-line order.

Используйте `-ApplySubConfig NONE` для generation/baking commands, которые
должны читать только master config. BuildTools делает это для
`CompileAngelScript`, `CompileManagedScripts`, `BakeResources` и `ForceBakeResources`.

Держите sub-configs узкими:

- environment modes выбирают infrastructure и diagnostics;
- tests выбирают deterministic fixtures и headless behavior;
- scenes выбирают startup content;
- release modes выбирают product-safe settings;
- secrets не находятся в sub-configs.

## Проверка изменения конфигурации

1. Повторите configure embedding project, если изменились CMake options или
   main config path.
2. Выполните `CompileAngelScript` и/или `CompileManagedScripts` для каждого включённого backend, чьи script inputs, generated API, analyzers или metadata изменились.
3. Выполните `BakeResources`; используйте `ForceBakeResources` после изменения
   pack membership, baker selection, include/exclude patterns, language sets или
   migration rules.
4. Запустите самый узкий sub-config, использующий измененный setting.
5. Проверьте startup logs на `Apply config`, `Apply sub config`, unknown/missing
   files, skipped languages, missing bakers и side resource entries.
6. Выполните project test, разрешающий custom settings и content-backed references.
7. Один раз выполните build/package, если изменилась internal config или
   composition runtime resource entries.

Два примера движка служат исполняемыми configuration fixtures:

```bash
(cd Examples/MinimalProject && python3 validate.py)
(cd Examples/MinimalMultiplayer && python3 validate.py)
```

В Windows используйте варианты с `win64-`.

## Частые ошибки

| Симптом | Причина | Восстановление |
|---|---|---|
| `Config file not found` | Неверный working directory, `FO_MAIN_CONFIG` или `-ApplyConfig` path | Передайте явный config path или запускайте ниже project root |
| `Sub config not found` | Опечатка в имени или section не загружена | Проверьте порядок/имя section и примененный master config |
| `Parent sub config not found` | Parent расположен позже или отсутствует | Переместите parent перед child или исправьте имя |
| `Resource pack name not specified` | Отсутствует `Name` | Добавьте уникальное pack name |
| Сторона получает неожиданный pack | Side-only flag отсутствует или неверен | Разделите packs и проверьте generated resource entries |
| Incremental bake сохраняет старый output | Изменился pack membership или baker | Выполните `ForceBakeResources` и удаляйте только документированные disposable outputs |
| Built-in setting выглядит проигнорированным | Победил более поздний sub-config/local config/CLI/auto layer | Проверьте полную precedence chain |
| Опечатка незаметно стала custom setting | Unknown names намеренно принадлежат проекту | Добавьте settings/content validation test |

## Дисциплина обновлений

При обновлении Engine или embedding project проверяйте изменения
`Settings.inc`, `Settings.cpp`, `ApplicationInit.cpp`, BuildTools project
options, baking stages и диапазона project `.fomain`. В том же change обновляйте
руководство, project config, tests и generated references, если изменились
precedence, fields, defaults, pack routing или launch profiles.

<a id="optional-native-components"></a>
## Опциональные native-компоненты

Перечислите используемые компоненты через `SetOptionValues` до `StartProjectGeneration`. Каждая опция — BOOL, по умолчанию OFF, с числовым дефайном 0/1. Общего списка выбора нет. Исходники и Engine-заголовки регистрируются и подключаются всегда; опциональное содержимое закрыто guards внутри файлов.

```cmake
SetOptionValues(
    FO_DATABASE_SQLITE ON
    FO_DATA_SOURCE_DAT ON
    FO_IMAGE_FRM ON)
```

| Опция | Роль и зависимость |
| --- | --- |
| `FO_DATA_SOURCE_DAT` | Common; DAT 2.0/2.1 и zlib |
| `FO_DATA_SOURCE_ZIP` | Common; внешние ZIP/BOS и bundled minizip |
| `FO_DATABASE_JSON` | Server; обычный JSON, существующая JSON-зависимость |
| `FO_DATABASE_SQLITE` | Server; SQLite и CBOR, без BSON |
| `FO_DATABASE_MONGO` | Server; mongoc и libbson |
| `FO_IMAGE_FRM` | Baker; FRM и FRx, Fallout-палитра под guard |
| `FO_IMAGE_RIX`, `FO_IMAGE_ART`, `FO_IMAGE_SPR` | Baker; отдельные legacy importers |
| `FO_IMAGE_ZAR`, `FO_IMAGE_TIL` | Baker; независимые importers и общий decoder под guard |
| `FO_IMAGE_MOS`, `FO_IMAGE_BAM` | Baker; отдельные legacy importers |
| `FO_AUDIO_ACM` | Baker; декодер Interplay ACM, встроенный кодировщик Ogg Vorbis |

Memory обязательна в Server; для Baker обязательны PNG/TGA/FOFRM и libpng, а для аудио — WAV/Ogg и Vorbis/Ogg. Embedded/APK сохраняют minizip при внешнем ZIP OFF. Stock client читает baked sprites и Vorbis-аудио без импортёров исходников и драйверов БД.

Старые `FO_DISABLE_MONGO`/`FO_DISABLE_SQLITE` завершаются ошибкой; удалите их из кеша/окружения и выберите положительные опции.

`SetOptionValues` и `SetOption` отвергают имя `FO_`, которое не является объявленной опцией, поэтому опечатка в имени компонента останавливает configure на стадии Init, а не оставляет компонент выключенным:

```text
SetOptionValues: FO_DATABASE_SQLLITE is not a declared option. Check the name against the CMake option reference; project variables need a prefix other than FO_
```

Снятые имена отвергаются прямо в вызове с указанием замены; их перечисляет раздел [Устранение неполадок настройки проекта](../../troubleshooting/setup.md#опечатка-или-снятая-опция-останавливает-configure).

<a id="baking-configuration-fingerprint"></a>
## Отпечаток конфигурации бейкинга

Объявление опции регистрирует её тип и влияние на бейкинг. `BAKING` в `DeclareBoolOption`/`DeclareValueOption` или в начале группы `Declare*Options` включает опции в `FO_BAKING_CONFIG_HASH`. Опции без отметки исключаются; [справочник опций](../../reference/cmake/options.md) показывает каждое решение.

Хеш учитывает идентичность проекта и выбор конфига, геометрию, лимиты моделей/эффектов, 3D, частицы, scripting backend и импортеры архивов/изображений/аудио. Провайдеры БД, диагностика, allocator, пути вывода и выбор native-целей не инвалидируют ресурсы.

CMake считает SHA-256 непосредственно перед генерацией кода, после предыдущих стадий и Codegen Pre hook. Имена сортируются, BOOL нормализуются в 0/1, границы строк сохраняются. Новые отмеченные проектные опции включаются автоматически; завершите их настройку до потребления значений в `SetupCodeGeneration`.

Baker пишет `Resources.baking-config-hash` и поле отчёта `bakingConfigHash`. Отсутствие/изменение отпечатка очищает устаревшие outputs и требует full bake независимо от `FO_BUILD_HASH`; прежние значения допускают incremental bake. Оба маркера удаляются до работы и записываются лишь после успешного запекания ресурсов. До упаковки повторите configure/build/bake.
