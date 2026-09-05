---
layout: default
title: Конфигурация игрового проекта
locale: ru
document_id: project-configuration
permalink: /Docs/ru/how-to/build/project-configuration.html
---

# Конфигурация игрового проекта

<!-- docs-translation: {"document_id":"project-configuration","locale":"ru","source_path":"Docs/en/how-to/build/project-configuration.md","source_sha256":"8ab91530230aa86d3ba715748ca6db90fec2840628e6cd62f00c13825629b88c"} -->

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

## Порядок приоритетов

Для unpackaged application settings применяются в таком порядке:

1. defaults из `Source/Common/Settings.inc`;
2. выбранный `.fomain`, найденный через `-ApplyConfig` или подъем по каталогам
   до `FO_MAIN_CONFIG`;
3. каждый явно выбранный `-ApplySubConfig` в порядке command line;
4. local config установленного клиента в writable cache, если он существует;
5. обычные command-line setting overrides;
6. platform/build auto-settings.

В packaged applications внешний `.fomain` заменяется generated internal
config с фиксированной движком patch area размером 10000 bytes. Root-значения
game settings также передаются в metadata, а internal config содержит только
deltas выбранного sub-config, включая явные false или empty overrides. Command
line всё равно применяется после local config. Более поздние слои
переопределяют ранние scalar settings.

Metadata применяется только после создания `BaseEngine`. Если project code
читает объявленный game setting в `ApplicationInitHook` или более раннем startup
path, добавьте его fully qualified name в `Baking.BootstrapGameSettings`.
Config baker запишет такой setting полностью в каждый internal config и
отклонит неизвестное имя или опечатку. Не включайте обычные runtime settings:
они принадлежат metadata, а расход фиксированной patch area на них может
сорвать packaging.

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
Baking.ServerResources = ServerResources
Baking.ClientResources = Resources
Baking.PlatformBinaries = PlatformBinaries
Baking.CacheResources = Cache
```

Неизвестные имена становятся project custom settings и доступны через
`GetCustomSetting` / `FindCustomSetting`. Это намеренное поведение для
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
```

Допустимые fields:

| Field | Значение |
|---|---|
| `Name` | Обязательная identity pack и generated resource entry |
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
старых project files, но не является текущим field `ResourcePackInfo`;
рекурсию задавайте через `IncludePatterns = **`.

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
`CompileAngelScript`, `BakeResources` и `ForceBakeResources`.

Держите sub-configs узкими:

- environment modes выбирают infrastructure и diagnostics;
- tests выбирают deterministic fixtures и headless behavior;
- scenes выбирают startup content;
- release modes выбирают product-safe settings;
- secrets не находятся в sub-configs.

## Проверка изменения конфигурации

1. Повторите configure embedding project, если изменились CMake options или
   main config path.
2. Выполните `CompileAngelScript`, если изменились script inputs или metadata.
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
