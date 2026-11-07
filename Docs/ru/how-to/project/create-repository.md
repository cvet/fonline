---
layout: default
title: Создание репозитория игры на FOnline
locale: ru
document_id: project-create-repository
permalink: /Docs/ru/how-to/project/create-repository.html
---

# Создание репозитория игры на FOnline
<!-- docs-translation: {"document_id":"project-create-repository","locale":"ru","source_path":"Docs/en/how-to/project/create-repository.md","source_sha256":"e0c04fc4cc1c330cd29c8790ccfaecdd06ca353c7e2aa932bb315d19d05bef43"} -->
Это руководство ведёт игровую команду от пустого каталога до репозитория,
который конфигурируется, собирает ресурсы и проходит smoke-запуск на закреплённой
ревизии движка. Здесь объясняется, чем владеет каждый корневой файл, как
добавить Engine как submodule, какие пути игнорировать, каким форматам нужны
правила атрибутов и когда принимать решение о Git LFS. Руководство ссылается на
страницы сборки, которые владеют темой, а не повторяет списки опций и справочник
по settings.

## Проверенные исходные пути

- `.gitattributes`
- `BuildTools/Init.cmake`
- `BuildTools/buildtools.py`
- `BuildTools/cmake/ProjectInterface.json`
- `BuildTools/cmake/helpers/Build.cmake`
- `BuildTools/cmake/stages/Applications.cmake`
- `BuildTools/cmake/stages/Init.cmake`
- `Docs/en/how-to/build/embedding-project.md`
- `Docs/en/how-to/build/project-configuration.md`
- `Docs/en/how-to/build/public-example-repositories.md`
- `Docs/en/how-to/build/generated-content.md`
- `Docs/en/reference/cmake/stages.md`
- `Docs/en/tutorials/first-project.md`
- `Examples/MinimalProject`
- `Examples/MinimalMultiplayer/CMakeLists.txt`
- `Examples/MinimalMultiplayer/CMakePresets.json`
- `Examples/MinimalMultiplayer/FOnlineMinimalMultiplayer.fomain`
- `Examples/NativeExtensionSample`
- `Examples/PublicRepositoryTemplate`
- `Examples/PublicRepositories.json`

## Сначала примите решения

Некоторые решения дёшево принять в первый день и дорого менять, когда уже
появился контент.

| Решение | Почему оно первое | Где находится |
|---|---|---|
| Идентичность проекта: `FO_DEV_NAME`, `FO_NICE_NAME`, имя главного файла `.fomain` | `FO_DEV_NAME` служит префиксом каждой сгенерированной цели и бинарного файла (например, `<DEV>_ServerHeadless`). Presets, CI и объявления пакетов ссылаются на эти имена, поэтому меняйте их все вместе или не меняйте совсем. | `CMakeLists.txt`, имя файла `.fomain` |
| Скриптовый backend | У AngelScript и Managed C# разные предварительные требования к сборке, имена сборщиков ресурсов и анализаторы. Можно включить оба, но каждый включённый backend нужно собирать, компилировать и тестировать. | `FO_ANGELSCRIPT_SCRIPTING`, `FO_MANAGED_SCRIPTING`; см. [Скрипты на Managed C#](../scripting/managed-csharp.md) |
| Форма мира | `FO_GEOMETRY`, ширина и высота гекса и угол камеры обязательны. Они входят в отпечаток сборки ресурсов, поэтому их изменение делает недействительными все собранные карты и спрайты. | `CMakeLists.txt`; см. [Отпечаток конфигурации бейкинга](../build/project-configuration.md#отпечаток-конфигурации-бейкинга) |
| Роли приложений | `FO_BUILD_CLIENT`, `FO_BUILD_SERVER`, `FO_BUILD_MAPPER`, `FO_BUILD_BAKER` и `FO_BUILD_ASCOMPILER` выключены (OFF), пока что-то их не включит. | Presets |
| Расположение Engine и закрепление ревизии | Каждый клон и каждый CI-раннер должны уметь получить закреплённую ревизию. | `.gitmodules`, gitlink |
| Git LFS | Перенос существующих бинарных файлов в LFS переписывает историю. См. [Решение об использовании Git LFS](#решение-об-использовании-git-lfs). | `.gitattributes` |
| Целевые платформы | Возможность сборки, возможность упаковки и поддержка различаются; сверяйтесь с [матрицей поддержки](../../reference/platforms/support-matrix.md). | Presets, CI |

## Подготовка рабочей станции

Установите Git, CMake 3.22 или новее, настоящий интерпретатор Python 3 и
toolchain C++20. Подробный список для каждой ОС приведён в разделе
[Предварительные требования](../build/index.md#предварительные-требования);
полный маршрут настройки рабочей станции описан в
[workstation-setup.md](workstation-setup.md).

Проверьте интерпретатор, прежде чем на него полагаться. `python` может оказаться
Python 2, а `python3` может отсутствовать; запускатор Windows `py -3` выбирает
Python 3:

```powershell
py -3 --version
git config --global core.longpaths true
```

```bash
python3 --version
```

После добавления Engine в следующем разделе `host-check` сообщает об
отсутствующих предварительных требованиях, ничего не устанавливая:

```powershell
py -3 Engine\BuildTools\buildtools.py host-check windows
```

```bash
python3 Engine/BuildTools/buildtools.py host-check linux
```

Команда принимает macOS, но это руководство не описывает маршрут для macOS;
что поддерживается, смотрите в [матрице поддержки](../../reference/platforms/support-matrix.md).

## Создание репозитория и добавление Engine

1. Создайте репозиторий:

   ```bash
   mkdir MyGame
   cd MyGame
   git init -b main
   ```

2. Добавьте Engine в `Engine/`. Этот путь предполагают все примеры Engine, все
   пути пакетов `.fomain` по умолчанию (`Engine/Resources/Core`) и туториалы:

   ```bash
   git submodule add https://github.com/cvet/fonline.git Engine
   ```

3. Закрепите точную ревизию. `git submodule add` извлекает вершину ветки по
   умолчанию, а это не релизное решение. Выберите ревизию осознанно, извлеките её
   внутри submodule и добавьте в индекс новый gitlink:

   ```bash
   git -C Engine checkout <engine-sha>
   git -C Engine branch -r --contains <engine-sha>
   git add .gitmodules Engine
   git submodule status Engine
   ```

   Вывод `branch -r --contains` должен содержать опубликованную ветку, обычно
   `origin/master`. Коммит, существующий только в вашем локальном клоне Engine,
   заставит каждый другой клон и CI-раннер завершиться ошибкой при
   инициализации submodule. Прочитайте `Engine/VERSION` и
   [список изменений](../../reference/changelog.md) для выбранной ревизии и
   запишите оба значения рядом с SHA в сообщении первого коммита.

4. Инициализируйте Engine в других клонах по имени. Если в проекте нет других
   submodule, это равносильно обычной инициализации; если есть приватные или
   большие, команда не даст их загрузить:

   ```bash
   git submodule update --init --recursive Engine
   ```

   Не используйте `git clone --recurse-submodules` или
   `git submodule update --init --recursive` без указания пути в репозитории,
   который содержит и ненужные вам submodule. На проверенной ревизии сам Engine
   не объявляет вложенных submodule, поэтому `--recursive` сегодня ничего не
   стоит; оставьте его, потому что более поздняя ревизия может добавить
   вложенный submodule.

5. Никогда не добавляйте `Engine/` в `.gitignore` игры: Git отказывается
   добавлять в индекс игнорируемый путь без `--force`, поэтому при каждом
   обновлении закрепления потребуется обходной приём. Принадлежащие Engine
   примеры игнорируют `/Engine/` только потому, что внутри репозитория Engine
   этот путь является локальной ссылкой, а не submodule.

6. Переход на другую ревизию Engine -- это проверяемое изменение, а не правка
   указателя. Используйте [обновление встраивающего проекта](../migration/engine-upgrade.md)
   и избегайте `git submodule update --remote`, который переносит закрепление на
   вершину ветки.

### Абсолютный или относительный URL submodule

| Форма URL | Когда использовать | Ограничение |
|---|---|---|
| `https://github.com/cvet/fonline.git` | Команда использует публичный Engine напрямую. Именно это примеры Engine записывают в `.gitmodules`. | Каждому раннеру нужен исходящий доступ к этому хосту. |
| `../fonline.git` (относительный) | Команда держит собственное зеркало или форк Engine на том же Git-сервере и в том же пространстве имён, что и репозиторий игры. | Git разрешает путь относительно URL `origin` суперпроекта. Зеркало должно содержать закреплённый коммит, а каждому участнику и раннеру нужен доступ к обоим репозиториям с теми же учётными данными. |

Если зеркало Engine не находится рядом с репозиторием игры, используйте
абсолютный URL. В любом случае держите зеркало обычным зеркалом `cvet/fonline`,
чтобы закреплённый SHA оставался тем же.

## Корневые файлы и что чем владеет

| Файл или каталог | Владеет | Примечания |
|---|---|---|
| `CMakeLists.txt` | Идентичностью продукта, опциями backend и геометрии, регистрацией исходников, пакетами, целями проекта | Вызывает стадии Engine в фиксированном порядке. См. [Написание корневого CMakeLists.txt](#написание-корневого-cmakeliststxt). |
| `CMakePresets.json` | Точками входа configure и build для каждого toolchain | Принадлежит проекту; Engine не публикует список presets. См. [Presets](#presets). |
| `<Name>.fomain` | Настройками среды выполнения и сборки ресурсов, пакетами ресурсов, подконфигурациями | Выбирается через `FO_MAIN_CONFIG`. Правила составления: [Конфигурация игрового проекта](../build/project-configuration.md). |
| `VERSION` | Версией проекта, которую читает `Common.GameVersion` | Принадлежит проекту и отличается от `Engine/VERSION`. Создайте его сейчас с начальным значением и передайте владение генератору; см. [versioning-and-releases.md](versioning-and-releases.md). |
| `Scripts/` | Исходниками AngelScript `.fos` или Managed C# `.cs` | Имена каталогов выбираются через `InputDirs` ваших пакетов ресурсов. |
| `SourceExt/` | Необязательным проектным нативным C++ | Регистрируется по ролям через `AddEngineSources`; см. [Нативные расширения](../native-extensions.md). |
| `Content/`, `Maps/` | Прототипами, картами и текстами | Имена -- соглашение проекта; `MinimalMultiplayer` использует `Content`, `Maps`, `Particles`. |
| `.gitmodules`, `Engine` | Расположением Engine и закреплённым gitlink | Записываются командой `git submodule add`. |
| `.gitignore` | Деревьями сгенерированных файлов и вывода | См. [Игнорирование сгенерированного вывода](#игнорирование-сгенерированного-вывода). |
| `.gitattributes` | Окончаниями строк и бинарными форматами | См. [Объявление окончаний строк и бинарных форматов](#объявление-окончаний-строк-и-бинарных-форматов). |

Не называйте каталог авторского контента `Resources`, `ServerResources`,
`PlatformBinaries`, `Baking` или `Cache`. `Common.ResourcesPath` по умолчанию
равен `Resources`, а остальные -- имена вывода сборки ресурсов или среды
выполнения, которые примеры Engine игнорируют.

Типичное дерево после первого запуска:

```text
MyGame/
├── Engine/                 submodule, pinned
├── .gitmodules
├── .gitignore
├── .gitattributes
├── CMakeLists.txt
├── CMakePresets.json
├── MyGame.fomain
├── VERSION
├── Scripts/
├── SourceExt/              only when the project has native code
├── Content/                prototypes and text
├── Maps/
└── Docs/                   project policy only; link to Engine docs for mechanics
```

Правило владения между `Engine/` и репозиторием игры определяет страница
[Встраивание FOnline в игровой проект](../build/embedding-project.md).

## Написание корневого CMakeLists.txt

Корневой файл подключает `Engine/BuildTools/Init.cmake`, задаёт обязательные
опции, а затем ровно один раз вызывает точку входа каждой стадии по порядку.
Пропуск стадии, повторный вызов или вызов не по порядку прерывает configure.
Приведённый ниже аннотированный скелет получен из
`Examples/MinimalProject/CMakeLists.txt` и через presets включает AngelScript,
сборщик ресурсов и headless-сервер:

```cmake
cmake_minimum_required(VERSION 3.22)
project(MyGame)

# Engine must be a submodule or link at Engine/ next to this file.
include(Engine/BuildTools/Init.cmake)

# Identity: DEV_NAME prefixes generated targets, NICE_NAME is the display name.
SetOption(FO_MAIN_CONFIG "MyGame.fomain")
SetOption(FO_DEV_NAME "MYGAME")
SetOption(FO_NICE_NAME "My Game")

# World shape: required, and part of the baking fingerprint.
SetOption(FO_GEOMETRY HEXAGONAL)
SetOption(FO_MAP_HEX_WIDTH 32)
SetOption(FO_MAP_HEX_HEIGHT 16)
SetOption(FO_MAP_CAMERA_ANGLE 25.6589)

# Required common output root; a per-preset build tree keeps bakes separate.
SetOption(FO_OUTPUT_PATH ${CMAKE_BINARY_DIR})

# Backends and features.
SetOption(FO_ENABLE_3D OFF)
SetOption(FO_ANGELSCRIPT_SCRIPTING ON)
SetOption(FO_MANAGED_SCRIPTING OFF)
SetOption(FO_NATIVE_SCRIPTING OFF)
SetOption(FO_UNIT_TESTS ON)

# Stages, in the order the pipeline validates.
StartProjectGeneration()
RegisterProjectOptions()
AddThirdPartyLibraries()
# AddEngineSources(SERVER SourceExt/ServerExtension.cpp)  goes here when native code exists
RegisterEngineSources()
SetupCodeGeneration()
BuildCoreLibraries()
BuildApplications()
SetupScriptsAndBaking()
# DefinePackage and AddBakingTarget calls go here
BuildPackages()
# Project-owned custom targets such as a smoke target go here
FinalizeProjectGeneration()
```

Моменты, в которых легко ошибиться:

- `SetOption` присваивает значение, только если переменная ещё не определена.
  Фактический приоритет таков: подходящая переменная окружения `FO_`, затем
  существующее значение кэша или `-D`, затем ваш `SetOption`, затем объявленное
  значение по умолчанию. Поэтому presets могут переопределять эти значения через
  `cacheVariables`.
- `SetOption` и `SetOptionValues` проверяют каждое имя `FO_`. Снятая с
  поддержки опция останавливает configure прямо в вызове и называет замену;
  любое другое необъявленное имя `FO_` останавливает стадию Init сообщением
  `SetOption: <name> is not a declared option` (или `SetOptionValues: ...`).
  Имена, заданные через `-D`, presets или окружение, не проверяются. Копируйте
  имена из сгенерированного [справочника опций](../../reference/cmake/options.md);
  переключатель Managed -- `FO_MANAGED_SCRIPTING`.
- Необязательные нативные компоненты, такие как провайдеры баз данных,
  выбираются через `SetOptionValues` до `StartProjectGeneration`; см.
  [Опциональные native-компоненты](../build/project-configuration.md#опциональные-native-компоненты).
- Нативные исходники добавляются по ролям (`COMMON`, `SERVER`, `CLIENT`,
  `MAPPER`, `BAKER`, `TESTS`) через `AddEngineSources` до
  `RegisterEngineSources`. `Examples/NativeExtensionSample` показывает
  `SourceExt/`, покрытую unit-тестами базовую библиотеку и smoke-цель. Его
  интеграция списка линковки `FO_SERVER_LIBS` описана в примерах как состояние,
  привязанное к ревизии, а не как объявленный helper; проверяйте её заново при
  каждой смене закрепления Engine.
- Пакеты объявляются между `SetupScriptsAndBaking()` и `BuildPackages()`;
  проектная цель сборки ресурсов создаётся сразу после
  `SetupScriptsAndBaking()`. См. [Упаковка и релиз](../release/packaging.md) и
  [Добавление проектной цели запекания](../build/embedding-project.md#добавление-проектной-цели-запекания).
- Скелету не нужен hook для библиотеки сборщика ресурсов. Непакетированный
  клиент или сервер загружает `<FO_DEV_NAME>_BakerLib` из своего каталога, чтобы
  при запуске собрать изменённые исходники, а `BuildApplications()` копирует
  библиотеку рядом с каждым таким исполняемым файлом, если `FO_BUILD_BAKER`
  включён. Вызов из проекта вроде
  `CopyTargetRuntimeToTarget(${FO_DEV_NAME}_Server ${FO_DEV_NAME}_BakerLib)`
  прерывает configure. Собственные файлы среды выполнения проекта копируются из
  hook `Finalize Pre`; см.
  [Копирование файлов среды выполнения проекта рядом с приложениями](../build/embedding-project.md#копирование-файлов-среды-выполнения-проекта-рядом-с-приложениями).
- Хэш сборки -- это `HEAD` репозитория проекта. Вне Git-репозитория или до
  первого коммита CMake без сообщения об ошибке переходит на случайный хэш
  (в логе configure виден `Build hash: ...-random`). Для первого configure
  это нормально, но закоммитьте изменения, прежде чем оценивать инкрементальное
  поведение.

Список стадий, hooks и ответственности генерируется на странице
[Стадии и hooks проекта CMake](../../reference/cmake/stages.md). Смысл
сгенерированных выходных данных описан в
[Работе с генерируемым содержимым](../build/generated-content.md).

## Написание главной конфигурации

Начните `.fomain` с `Examples/MinimalProject/FOnlineStarter.fomain` и держите
одно осознанное значение на строку. Небольшая конфигурация для AngelScript
выглядит так:

```ini
Common.GameName = My Game
Common.GameVersion = $FILE{VERSION}
Common.ResourcesPath = Resources
Common.PlatformBinariesPath = PlatformBinaries
Common.CachePath = Cache

Network.ServerPort = 4000
Network.WebSocketPort = 4001

Server.DbStorage = Memory

Baking.BakeLanguages = engl
Baking.BakeOutput = Baking

[SubConfig]
Name = Smoke
ServerNetwork.DisableNetworking = True

[ResourcePack]
Name = Metadata
InputDirs = Scripts
Bakers = Metadata
ServerOnly = True

[ResourcePack]
Name = Scripts
InputDirs = Scripts
Bakers = AngelScript
ServerOnly = True

[ResourcePack]
Name = Embedded
InputDirs = Engine/Resources/Embedded
IncludePatterns = **
Bakers = Image Effect RawCopy

[ResourcePack]
Name = Core
InputDirs = Engine/Resources/Core Engine/Resources/Embedded
IncludePatterns = **
Bakers = Image Effect RawCopy
```

Создайте `VERSION` с начальным значением, например `0.1.0`, до первой сборки
ресурсов; `$FILE{VERSION}` читается относительно каталога этого файла. Поля
пакетов ресурсов, наследование подконфигураций, приоритеты и поведение `$FILE`
описаны в [Конфигурации игрового проекта](../build/project-configuration.md).

Входные каталоги пакета монтируются рекурсивно, а пустой `IncludePatterns`
выбирает все файлы, поэтому для рекурсии пакету не нужен никакой ключ.
`RecursiveInput` не является полем пакета ресурсов и игнорируется, если его
записать; не добавляйте его.

## Presets

Держите один канонический configure preset, который каждый участник и каждый
скрипт могут назвать, не зная хоста, и добавляйте явные presets для каждого
toolchain, который вы действительно поддерживаете. CI-задача или smoke-скрипт
должны вызывать тот явный preset, который они квалифицируют; разработчик,
которому нужно «просто собрать», вызывает preset по умолчанию.

```json
{
  "version": 3,
  "cmakeMinimumRequired": {
    "major": 3,
    "minor": 22,
    "patch": 0
  },
  "configurePresets": [
    {
      "name": "base",
      "hidden": true,
      "binaryDir": "${sourceDir}/Build/${presetName}",
      "cacheVariables": {
        "FO_BUILD_SERVER": "ON",
        "FO_BUILD_BAKER": "ON"
      }
    },
    {
      "name": "default",
      "displayName": "Default (host generator)",
      "inherits": "base"
    },
    {
      "name": "windows-vs",
      "displayName": "Windows x64 (installed Visual Studio)",
      "inherits": "base",
      "architecture": {
        "value": "x64",
        "strategy": "set"
      },
      "condition": {
        "type": "equals",
        "lhs": "${hostSystemName}",
        "rhs": "Windows"
      }
    },
    {
      "name": "linux-gcc",
      "displayName": "Linux GCC (Ninja Multi-Config)",
      "inherits": "base",
      "generator": "Ninja Multi-Config",
      "cacheVariables": {
        "CMAKE_C_COMPILER": "gcc",
        "CMAKE_CXX_COMPILER": "g++"
      },
      "condition": {
        "type": "equals",
        "lhs": "${hostSystemName}",
        "rhs": "Linux"
      }
    }
  ],
  "buildPresets": [
    {
      "name": "default-bake",
      "configurePreset": "default",
      "configuration": "Release",
      "targets": [
        "BakeResources"
      ]
    }
  ]
}
```

- Скрытый `base` содержит то, что общее для всех presets: переключатели ролей и
  `binaryDir` для каждого preset. Поскольку скелет задаёт `FO_OUTPUT_PATH`
  равным каталогу сборки, у каждого preset свои деревья `Baking/`, `Binaries/` и
  `Cache/`, так что два toolchain никогда не делят одну сборку ресурсов.
- `default` не закрепляет ни генератор, ни компилятор, поэтому CMake выбирает
  генератор хоста по умолчанию. По той же причине примеры Engine оставляют выбор
  генератора в Windows за CMake: закрепление одной версии Visual Studio
  отвергло бы более новые установки.
- Закрепляйте генератор и компилятор только в явных presets. Добавьте клиенты,
  Mapper, кросс-компиляцию и presets пакетов, которые нужны вашей команде; это
  руководство их не перечисляет, потому что они различаются от игры к игре.
- Версии 3 схемы presets достаточно для `condition` и `${hostSystemName}`, а
  CMake 3.21 её читает. Примеры Engine объявляют версию 3; используйте самую
  низкую версию, которую поддерживает ваш минимальный CMake, потому что более
  новая схема заставляет старый CMake отвергнуть весь файл.
- Добавьте в `base` нужные вам роли (`FO_BUILD_CLIENT`, `FO_BUILD_MAPPER`,
  `FO_BUILD_ASCOMPILER`). Пользовательские цели в стиле `RunStarterSmoke` в
  примерах защищены условием `FO_BUILD_SERVER AND FO_BUILD_BAKER`.

## Игнорирование сгенерированного вывода

Игнорируйте всё, что записывает конвейер, и всё, что инструмент оставляет рядом
с исходниками. Привязывайте записи к корню репозитория:

```text
/Build/
/Baking/
/Baking.lock
/AS_DEBUG.lock
/Binaries/
/Cache/
/Resources/
/ServerResources/
/PlatformBinaries/
/Workspace/
/*.log
__pycache__/
```

- Если `FO_OUTPUT_PATH` указывает на каталог сборки, большая часть вывода
  попадает в `Build/<preset>/`. Остальные записи покрывают запуски из корня
  репозитория, как это делают `.gitignore` примеров, а также файл
  `<output>.lock`, который удерживает активная сборка ресурсов. См.
  [Владение выводом сборки ресурсов](../../explanation/content-pipeline/baking.md);
  никогда не удаляйте lock-файл, чтобы обойти выполняющуюся сборку.
- `Workspace/` -- принятое по соглашению место для логов задач, дампов и
  скриншотов.
- Добавьте каталоги редакторов и IDE, которыми пользуется ваша команда; Engine
  их не предписывает.

## Объявление окончаний строк и бинарных форматов

Побайтово чувствительные шаги (проверки на основе хэшей, записи происхождения,
проверочные гейты `--check` для сгенерированных файлов, входные данные сборки
ресурсов) ведут себя по-разному, если клоны в Windows и Linux расходятся в
окончаниях строк. Объявите политику один раз. Этот шаблон следует собственному
`.gitattributes` Engine и политике, общей для репозиториев с примерами:

```text
* text=auto eol=lf

# Windows scripts keep CRLF
*.bat  text eol=crlf
*.cmd  text eol=crlf
*.ps1  text eol=crlf

# FOnline text formats
*.fomain text
*.fopro  text
*.fomap  text
*.fofx   text
*.fofnt  text
*.fofrm  text
*.fo3d   text
*.fotxt  text
*.fos    text
*.cs     text
*.spark  text
*.efkproj text

# FOnline and common binary formats
*.spk    binary
*.efk    binary
*.png    binary
*.tga    binary
*.jpg    binary
*.wav    binary
*.ogg    binary
*.ogv    binary
*.fbx    binary
*.zip    binary
```

- `eol=lf` для `*` -- политика, которую используют репозитории с примерами.
  Оставляйте CRLF только для перечисленных выше типов скриптов Windows и для
  файлов проектов Visual Studio, если вы их коммитите.
- Добавляйте каждое авторское текстовое или бинарное расширение, которое вводит
  ваша игра, включая форматы, принадлежащие проекту. Корневой `.gitattributes`
  Engine -- справочный список расширений FOnline, но он также содержит правила
  только для Engine.
- Не полагайтесь на `text=auto` в угадывании важных бинарных файлов: пометка
  формата как `binary` также отключает для него попытки diff и merge.
- Один раз выполните `git add --renormalize .` после добавления или изменения
  правил и просмотрите результат до коммита.

## Решение об использовании Git LFS

Engine не требует Git LFS, и его репозиторий LFS не использует. Игра решает это
заранее, потому что последующее преобразование уже закоммиченных бинарных файлов
переписывает историю.

Примите эти решения до коммита первого бинарного файла:

1. Версионируются ли крупные бинарные исходники (текстуры, аудио, модели,
   исходная графика, упакованные архивы) в репозитории игры вообще или лежат в
   хранилище ассетов, на которое репозиторий лишь ссылается.
2. Какие расширения и какой порог размера отправляются в LFS; это фиксируется в
   `.gitattributes` с фильтром LFS.
3. Где находится хранилище объектов LFS, кто может в него писать, какова его
   квота и срок хранения. Собственные и размещённые хранилища ведут себя
   по-разному; выбор -- политика проекта.
4. Как CI получает объекты. Checkout, пропускающий LFS, создаёт pointer-файлы,
   из-за которых сборка ресурсов затем падает неочевидным образом. Загружайте
   объекты LFS на том же шаге задачи, что и checkout, и завершайтесь с ошибкой
   сразу, если pointer-файл дошёл до сборки ресурсов. Порядок шагов задачи
   описан в [continuous-integration.md](continuous-integration.md).
5. Как вы проверяете, что каждый файл, подпадающий под правило LFS, действительно
   хранится через LFS.

Без LFS держите бинарные файлы небольшими и не коммитьте сгенерированный вывод.
В любом случае зафиксируйте это решение в документации проекта.

## Копирование примера или старт со скелета

| Отправная точка | Что это | Для чего использовать |
|---|---|---|
| `Examples/MinimalProject` | Исполняемый стартовый проект: headless-сервер, сборщик ресурсов, один скрипт, одно нативное серверное расширение, один smoke-раннер и presets для Windows/Linux | Самый быстрый первый зелёный результат. Скопируйте его, затем сократите. |
| `Examples/MinimalMultiplayer` | Небольшой проект клиент/сервер с контентом, пакетом и мультиплеерным smoke | Образец для ролей клиента, пакетов контента и объявления пакета |
| `Examples/NativeExtensionSample` | Расширенный нативный маршрут hook/export/test с `SourceExt/` и `Tests/` | Образец, когда в проекте есть нативный код |
| `Examples/PublicRepositoryTemplate` | Оверлей публикации для публичных репозиториев с примерами Engine: плейсхолдеры, файлы управления, workflows и метаданные репозитория | Не стартовый проект игры. Не копируйте его в игру; его файлы ожидают `example-repository.json` и принадлежащий Engine процесс staging |

Маршрут, который быстрее всего приводит новую команду к зелёному результату:

1. Скопируйте содержимое `Examples/MinimalProject` в корень нового репозитория,
   исключив любую локальную ссылку `Engine` и любой каталог `Build/`. Удалите
   строку `/Engine/` из скопированного `.gitignore`: пример игнорирует этот путь,
   потому что внутри репозитория Engine он является локальной ссылкой, а
   репозиторий игры отслеживает его как submodule.
2. Добавьте Engine как submodule, как описано выше.
3. Меняйте идентичность одной операцией: `FO_DEV_NAME`, `FO_NICE_NAME`, имя файла
   `.fomain`, значение `FO_MAIN_CONFIG` и имена в presets и smoke-цели. Не
   переименовывайте идентификаторы по одному.
4. Выполните smoke ниже и требуйте зелёного результата до любых правок
   геймплея.
5. Только после этого уберите демонстрационные части (нативное расширение и его
   библиотеку `INTERFACE`, демонстрационные удалённые вызовы) отдельным
   изменением, сохранив маркер запуска скрипта и контракт smoke-раннера.

Примеры написаны так, что находят Engine в `Engine/` рядом с `CMakeLists.txt`.
Чтобы запустить пример на месте внутри репозитория Engine для оценки, создайте
там ссылку и удалите её потом. Собственный `.gitignore` примера исключает эту
ссылку; никогда её не коммитьте. Junction Windows удаляется командой
`cmd /c rmdir Engine`, символическая ссылка Linux -- `rm Engine`; рекурсивное
удаление ссылки может пройти по ней в checkout Engine.

```powershell
cd Examples\MinimalProject
New-Item -ItemType Junction -Path Engine -Target ..\..
```

```bash
cd Examples/MinimalProject
ln -s ../.. Engine
```

Без ссылки configure останавливается с сообщением
`include could not find requested file: Engine/BuildTools/Init.cmake`. См.
[Первый headless-проект FOnline](../../tutorials/first-project.md).

## Проверка первого запуска

На хосте Linux один раз установите предварительные требования, которыми владеет
закреплённая ревизия Engine:

```bash
Engine/BuildTools/prepare-workspace.sh linux-packages
```

Для проекта, скопированного из `Examples/MinimalProject`, и его presets
(`windows`, `linux`, `windows-smoke`, `linux-smoke`) выполните из корня
репозитория:

```powershell
cmake --preset windows
cmake --build --preset windows-smoke
```

```bash
cmake --preset linux
cmake --build --preset linux-smoke
```

Smoke-цель собирает headless-сервер, собирает ресурсы и запускает
подконфигурацию `StarterSmoke`. Успех требует всех маркеров и нулевого кода
выхода:

```text
starter_native_extension_value=42
starter_server_started
starter_smoke_passed
```

Для проекта, который начинается со скелета из этого руководства, равнозначные
проверки таковы:

```powershell
cmake --preset default
cmake --build --preset default-bake
```

```bash
cmake --preset default
cmake --build --preset default-bake
```

Затем соберите сгенерированную цель `<FO_DEV_NAME>_ServerHeadless`, запустите её
с `-ApplyConfig <project-root>/MyGame.fomain -ApplySubConfig Smoke` и напишите
smoke-раннер, который, как `run_starter_smoke.py`, применяет таймаут, требует
маркеры жизненного цикла от вашего события запуска и завершается с ошибкой, если
маркер отсутствует. Процесс, который просто запустился и завершился, не является
пройденным smoke. Сборка ресурсов preset по умолчанию и успешный запуск сервера
-- два факта, которые нужно установить, прежде чем добавлять что-либо ещё.
Configure скелета, как было замечено, проходит проверку опций и доходит до
регистрации сторонних библиотек в Windows с установленным генератором Visual
Studio; при написании этого руководства он не доводился до конца и не собирался,
поэтому считайте проверкой ваш первый запуск.

Пересобирайте с `ForceBakeResources` после изменения состава пакетов, сборщиков
ресурсов или языков. Если сборка выглядит устаревшей, см.
[Восстановление](../build/generated-content.md#восстановление) в «Работе с
генерируемым содержимым».

## Устранение проблем первого запуска

| Симптом | Причина | Восстановление |
|---|---|---|
| `include could not find requested file: Engine/BuildTools/Init.cmake`, затем `Unknown CMake command "SetOption"` | `Engine/` отсутствует, не инициализирован или является ссылкой на неверный каталог | Выполните `git submodule update --init --recursive Engine` или исправьте цель ссылки |
| `git submodule update` не находит закреплённый коммит | SHA существует только в локальном клоне, или в зеркале его нет | Закрепите коммит, который показывает `git -C Engine branch -r --contains <sha>`, или обновите зеркало |
| Configure прерывается, называя пустую обязательную переменную | Отсутствует обязательная опция из таблицы в разделе [Написание корневого CMakeLists.txt](#написание-корневого-cmakeliststxt) | Добавьте строку `SetOption` |
| `SetOption: <name> is not a declared option` или `<name> was removed` | Имя, переданное в `SetOption` или `SetOptionValues`, записано с опечаткой или снято с поддержки | Сверьте его со [справочником опций](../../reference/cmake/options.md); для снятого имени выполните замену из сообщения |
| Опция, по-видимому, ни на что не влияет | Имя пришло из `-D`, preset или окружения, которые не проверяются, и записано с опечаткой или снято с поддержки | Сверьте его со [справочником опций](../../reference/cmake/options.md) |
| `CopyTargetRuntimeToTarget: the Applications stage already copies <FO_DEV_NAME>_BakerLib next to <target>; remove the project-side copy` | `CMakeLists.txt` всё ещё сам копирует библиотеку сборщика ресурсов, как делали проекты для более ранней ревизии Engine | Удалите этот вызов и все остальные вызовы `CopyTargetRuntimeToTarget(... _BakerLib)`; см. [Перенос проекта, который сам копирует сборщик ресурсов](../build/embedding-project.md#перенос-проекта-который-сам-копирует-сборщик-ресурсов) |
| Непакетированный клиент или сервер сообщает `Warning! <FO_DEV_NAME>_BakerLib not found. Resources may be out of date` или `Baker not found. Unable to bake resources` | Preset оставляет `FO_BUILD_BAKER` выключенным, или целевая платформа не собирает библиотеку сборщика ресурсов | Включите `FO_BUILD_BAKER` в preset и пересоберите цель приложения: она сначала собирает и копирует библиотеку |
| Сборка ресурсов падает на pointer-файлах или неожиданном тексте в бинарных файлах | Объекты LFS не были загружены | Загрузите объекты LFS до сборки ресурсов и проверьте фильтры в `.gitattributes` |
| Массовые изменения окончаний строк в diff | Отсутствующий или поздно добавленный `.gitattributes` | Добавьте правила, выполните `git add --renormalize .`, просмотрите результат |

## Контрольный список проверки

- [ ] `git submodule status Engine` показывает нужный SHA без префикса `+` или
  `-`, и этот SHA указан в выводе `git -C Engine branch -r --contains <sha>`
  как опубликованный.
- [ ] Свежий клон с последующим `git submodule update --init --recursive Engine`
  воспроизводит checkout, в том числе на образе CI-раннера.
- [ ] `Engine/` не игнорируется, а `.gitmodules` записывает выбранную вами форму
  URL.
- [ ] `CMakeLists.txt` задаёт каждую обязательную опцию, использует только
  объявленные имена опций и вызывает все десять стадий по одному разу по порядку.
- [ ] `CMakeLists.txt` не содержит вызова `CopyTargetRuntimeToTarget` для
  `<FO_DEV_NAME>_BakerLib`; копирование файлов среды выполнения проекта
  находится в hook `Finalize Pre`.
- [ ] `FO_DEV_NAME`, `FO_NICE_NAME`, имя `.fomain` и presets согласованы.
- [ ] `VERSION` существует, и `Common.GameVersion` берётся из него.
- [ ] `.gitignore` покрывает вывод сборки, сборки ресурсов, lock-файлы, логи и
  рабочее пространство и не затрагивает ни один авторский каталог.
- [ ] `.gitattributes` объявляет политику окончаний строк и каждый бинарный
  формат, а решение по LFS зафиксировано.
- [ ] Preset по умолчанию конфигурируется, а smoke-цель или цель сборки ресурсов
  зелёная на каждом заявленном хосте.
- [ ] Smoke требует маркеры жизненного цикла и нулевой код выхода, а не просто
  запущенный процесс.
- [ ] Второй клон проходит те же команды без локального состояния.
