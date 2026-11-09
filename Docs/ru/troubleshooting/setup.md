---
layout: default
title: Устранение неполадок настройки проекта
locale: ru
document_id: setup-troubleshooting
permalink: /Docs/ru/troubleshooting/setup.html
---

# Устранение неполадок настройки проекта
<!-- docs-translation: {"document_id":"setup-troubleshooting","locale":"ru","source_path":"Docs/en/troubleshooting/setup.md","source_sha256":"28fe00a972980af248cb4186ec1d5d9be819f872255e094454a2af61ab685507"} -->
Эта страница сопоставляет сбои, с которыми новый игровой проект сталкивается
между чистой машиной и первой зелёной сборкой, с их причинами и способами
исправления. Каждая запись приводит сообщение дословно в том виде, в каком его
выдаёт BuildTools, конвейер CMake или среда выполнения, поэтому ищите на этой
странице текст, который видите у себя. Сбои среды выполнения и отладчика после
успешной сборки описаны в разделе
[Нативная отладка, AngelScript и Managed C#](debugging.md).

Сообщения, которые исходят из исходного кода Engine, процитированы из него.
Сообщения CMake были получены с CMake 4.1.1 на Windows x64 и в других версиях
могут отличаться формулировкой или переносом строк; CMake переносит длинные
сообщения по 80 столбцам и воспринимает точку с запятой внутри сообщения как
границу списка. Сообщения Git, компилятора или операционной системы приведены в
их обычной формулировке и не воспроизводились; в соответствующей записи это
сказано.

## Проверенные исходные пути

- `BuildTools/Init.cmake`
- `BuildTools/buildtools.py`
- `BuildTools/package.py`
- `BuildTools/build.cmd`
- `BuildTools/prepare-workspace.sh`
- `BuildTools/cmake/ProjectInterface.json`
- `BuildTools/cmake/helpers/Build.cmake`
- `BuildTools/cmake/helpers/Options.cmake`
- `BuildTools/cmake/stages/Init.cmake`
- `BuildTools/cmake/stages/ProjectOptions.cmake`
- `BuildTools/cmake/stages/Codegen.cmake`
- `BuildTools/cmake/stages/ThirdParty.cmake`
- `BuildTools/cmake/stages/ScriptsAndBaking.cmake`
- `Source/Tools/Baker.cpp`
- `Source/Server/Server.cpp`
- `Source/Server/UpdaterBackend.cpp`
- `Source/Client/Updater.cpp`
- `Source/Common/MetadataRegistration.cpp`
- `Docs/contract-change-dispositions.json`
- `Docs/en/tutorials/first-project.md`
- `Docs/en/how-to/build/project-configuration.md`
- `Docs/en/how-to/build/generated-content.md`
- `Docs/en/explanation/content-pipeline/baking.md`
- `Docs/en/reference/metadata/index.md`
- `Examples/MinimalProject/CMakeLists.txt`
- `Examples/MinimalProject/CMakePresets.json`

## Первичная диагностика

1. Копируйте первую ошибку, а не последнюю. После каждого сбоя CMake печатает `Configuring incomplete, errors occurred!`, а последующие ошибки часто вытекают из первой.
2. Определите уровень по префиксу сообщения. Строки `[BuildTools]` и «голые» строки приходят из `buildtools.py` или скриптов-обёрток. Блоки `CMake Error` приходят из CMake или конвейера Engine. Текст запущенного сервера, клиента или baker относится к сбою запекания или среды выполнения.
3. Выведите пути, которые определил BuildTools. Неверный рабочий каталог объясняет многие результаты «не найдено» и «не готово»:

```powershell
py -3 Engine\BuildTools\buildtools.py env --summary-only
```

```bash
python3 Engine/BuildTools/buildtools.py env --summary-only
```

4. Чините того, кто отвечает за сбой. Не удаляйте workspace, дерево сборки или файл блокировки первым шагом; записи ниже указывают, когда чистый каталог является правильным исправлением.

## Инструментарий и инструменты хоста

Сначала установите и проверьте инструменты по руководству
[Подготовка рабочей станции для проекта FOnline](../how-to/project/workstation-setup.md).

### Python отсутствует или это Python 2

```text
Python not found
```

Скрипты-обёртки Engine печатают это, если в `PATH` нет ни `py`, ни `python` (Windows) либо ни `python3`, ни `python` (POSIX). Установите Python 3.12 или новее. В Windows установите вместе с ним лаунчер `py` и откройте новую оболочку.

```text
  File "Engine/BuildTools/buildtools.py", line <n>
    platform: str
            ^
SyntaxError: invalid syntax
```

`python` указывает на Python 2, который не может разобрать аннотации типов в `buildtools.py`. Выполните ту же команду через `py -3` (Windows) или `python3` (в остальных системах). Если `python3` тоже отсутствует или устарел, исправьте порядок в `PATH`, а не правьте скрипты. Более старый Python 3 падает уже внутри BuildTools: BuildTools импортирует `NotRequired` из `typing` (3.11 и новее) и передаёт `onexc=` в `shutil.rmtree` (3.12 и новее). Этот сбой выведен из исходного кода и не воспроизводился на старом интерпретаторе.

```text
Could NOT find Python3 (missing: Python3_EXECUTABLE Interpreter)
```

CMake выполняет `find_package(Python3 REQUIRED COMPONENTS Interpreter)` на стадии Codegen и нуждается в интерпретаторе Python 3, даже если ваши собственные скрипты используют другой. Сделайте Python 3 доступным через `PATH` (или регистрацию лаунчера Windows) и выполните configure снова. Шаг среды выполнения Managed C# дополнительно требует Python 3.11 или новее.

### CMake слишком старый или не читает пресеты

```text
CMake Error at CMakeLists.txt:1 (cmake_minimum_required):
  CMake 3.22 or higher is required.  You are running version <installed>
```

Точка входа Engine требует CMake 3.22. Установите более новый CMake и удалите каталог сборки, созданный старым.

```text
CMake Error: Could not read presets from <project-root>:
CMakePresets.json:<line>: Unrecognized "version" <n>: must be >=1 and <=<supported>
```

Файл пресетов использует схему новее, чем понимает установленный CMake. Примеры Engine объявляют `"version": 3`, который читает каждый поддерживаемый CMake (3.22 или новее). Эта запись была воспроизведена с номером версии больше, чем в любом реальном примере; поддерживаемый максимум в сообщении зависит от установленного CMake. Установите более новый CMake или понизьте версию схемы собственного файла, предварительно убрав используемые им возможности.

```text
CMake Error: No such preset in <project-root>: "<name>"
```

Имя пресета написано с ошибкой, или `cmake` запущен из каталога, в котором нет нужного `CMakePresets.json`. Выполните `cmake --list-presets` из корня проекта.

### Компилятор или Visual Studio отсутствует или слишком старый

```text
Please install Visual Studio with C++ Build Tools or Build Tools for Visual Studio
```

`buildtools.py host-check windows` (и `prepare-host-workspace windows`) не нашёл установку с компонентом `Microsoft.VisualStudio.Component.VC.Tools.x86.x64`. Установите Visual Studio 2022 или Build Tools с рабочей нагрузкой «Разработка классических приложений на C++» ("Desktop development with C++").

```text
Generator

  Visual Studio 17 2022

could not find any instance of Visual Studio.
```

CMake запросил генератор Visual Studio, который не установлен. Установите эту версию или выберите генератор, существующий на машине. Первые строки ошибки CMake называют генератор; пресеты примеров Engine оставляют генератор в Windows незаданным, чтобы CMake выбрал самый новый из установленных.

```text
MSVC <MSVC_VERSION> is below the required minimum 1944 (toolset 14.44 / Visual Studio 2022 17.14).
Clang <version> is below the required minimum 20.0
GCC <version> is below the required minimum 13.0
Unsupported compiler '<id>'. Supported: Clang >= 20, GCC >= 13, MSVC >= 2022, AppleClang (any).
```

Стадия Init отвергает компилятор ниже минимума сразу после того, как configure его обнаружил. Обновите Visual Studio через её установщик либо установите GCC 13 или Clang 20 и выберите его переменными окружения `CC` и `CXX` или записями кэша `CMAKE_C_COMPILER` и `CMAKE_CXX_COMPILER` в вашем пресете. CMake запоминает компилятор в каталоге сборки, поэтому после его смены выполняйте configure в новый каталог. В Debian или Ubuntu `prepare-host-workspace linux common-packages` устанавливает Clang 20. Платформы сборки `-win7` дополнительно закрепляют toolset 14.44; см. [Процесс сборки](../how-to/build/index.md#контур-совместимости-с-windows-7).

### Пакеты Linux не устанавливаются

```text
Please use a Debian/Ubuntu host with apt-get available
Please install sudo or run the preparation script as root
```

Возможности `*-packages` устанавливаются через `apt-get` и требуют root или `sudo`. В другом дистрибутиве установите те же инструменты его менеджером пакетов и подготавливайте только части workspace (`toolset`, `web` и так далее), которые пакеты не устанавливают.

```text
Workspace part common-packages is not ready (<version>)
```

Это сообщение появляется с `--check` (или словом-обёрткой `check`), если пакеты ещё не были установлены. Выполните ту же команду без `--check`.

### Ninja или llvm-nm не найдены

```text
ninja not found; install Ninja or Visual Studio CMake tools
CMake Error: CMake was unable to find a build program corresponding to "<generator>".  CMAKE_MAKE_PROGRAM is not set.  You probably need to select a different build tool.
```

Пресет либо сборка Web или Android выбрали генератор Ninja или Makefile, программы которого нет в `PATH`. В Windows добавьте компонент Visual Studio «Инструменты CMake для Windows на C++» ("C++ CMake tools for Windows"), в котором BuildTools ищет `ninja.exe`, или установите Ninja. Пресет Linux в `Examples/MinimalProject` использует `Ninja Multi-Config`.

```text
Symbol reader for the Managed interop shim table not found. Install LLVM (llvm-nm) or the Visual Studio "C++ Clang tools for Windows" component, or set FO_MANAGED_NM
Managed runtime for Windows cannot be built on a non-Windows host (dotnet/runtime has no such cross-target); build it on Windows and point FO_MANAGED_RUNTIME_PREBUILT at the published output/mono/<triplet> tree
```

Оба сообщения относятся к конфигурациям Managed C#. Первому нужен `llvm-nm` или `nm`, который CMake может найти; если он установлен в другом месте, задайте `FO_MANAGED_NM` как полный путь к нему. Второе называет поддерживаемый маршрут: соберите дерево среды выполнения в Windows и передайте его через переменную окружения.

## Репозиторий и checkout Engine

### Не найден include Engine

```text
CMake Error at CMakeLists.txt:3 (include):
  include could not find requested file:

    Engine/BuildTools/Init.cmake


CMake Error at CMakeLists.txt:4 (SetOption):
  Unknown CMake command "SetOption".
```

Вторая ошибка является следствием первой: `SetOption` определяется в `Init.cmake`, поэтому после неудавшегося include ничего выполниться не может. Путь include задан относительно каталога с `CMakeLists.txt`. Проверьте причины по порядку:

1. Submodule так и не был инициализирован. `git submodule status Engine` показывает строку, начинающуюся с `-`, а каталог `Engine/` пуст.
2. Новый клон или checkout, сделанный через `git worktree add`, копирует проект без заполнения его submodule. Инициализируйте его, как показано ниже.
3. Проект является принадлежащим Engine примером, запускаемым на месте, а ему нужна запись `Engine` рядом с `CMakeLists.txt`. Создайте junction или ссылку, описанную в [руководстве по первому проекту](../tutorials/first-project.md).
4. `cmake` запущен из каталога, отличного от корня проекта, или указывает на такой каталог.
5. Каталог существует под другим написанием, например `engine`, в файловой системе с учётом регистра.

```bash
git submodule status Engine
git submodule update --init Engine
ls Engine/BuildTools/Init.cmake
```

```powershell
git submodule status Engine
git submodule update --init Engine
Test-Path Engine\BuildTools\Init.cmake
```

### Область и состояние submodule

Указывайте путь при инициализации или обновлении: `git submodule update --init Engine`. Голые `git submodule update --init --recursive` или `git clone --recurse-submodules` инициализируют также все остальные submodule проекта, включая большие репозитории контента, которые проекту может быть нужно не загружать в checkout разработчика или CI. В этой ревизии Engine `Engine` не объявляет собственных submodule, поэтому указания пути достаточно, а `--recursive Engine` эквивалентно ему.

После `git pull` или переключения ветки в проекте checkout `Engine/` остаётся на прежнем коммите, пока вы не выполните `git submodule update Engine`. Знак `+` в начале строки `git submodule status Engine` означает, что checkout отличается от коммита, записанного в проекте. Не считайте отсоединённый HEAD внутри `Engine/` проблемой; это нормальное состояние. Руководство [Обновление Engine во встраивающем проекте](../how-to/migration/engine-upgrade.md) объясняет три идентичности Engine и способ их сравнения.

### Расхождение регистра в checkout на Windows

```text
[BuildTools] Checkout case drift: <path> -> <tracked-name>
Checkout case drifted from the git index in <n> entries
```

Переименование, меняющее только регистр, записывается в индексе Git, но в файловой системе без учёта регистра существующий каталог сохраняет прежнее написание. Поиск ресурсов и include учитывает регистр, поэтому симптом проявляется позже и далеко от причины: файл, который явно лежит на диске, при запекании объявляется отсутствующим, либо задача Linux CI падает на пути, который работает на машине с Windows. Свежий клон не воспроизводит проблему.

Запустите исправление из корня проекта. `--check` сообщает о расхождении и завершается с ненулевым кодом, ничего не переименовывая:

```powershell
py -3 Engine\BuildTools\buildtools.py repair-checkout-case --check
py -3 Engine\BuildTools\buildtools.py repair-checkout-case
```

```bash
python3 Engine/BuildTools/buildtools.py repair-checkout-case --check
python3 Engine/BuildTools/buildtools.py repair-checkout-case
```

Успешный запуск пишет в журнал `Checkout entry names match the git index`. Команда рекурсивно обходит submodule.

```text
[BuildTools] Tracked path is checked in under more than one spelling: <a> and <b>
Case-conflicting tracked paths have to be fixed in the repository
```

Индекс содержит один и тот же путь в двух написаниях. В Linux это два пути, а в Windows одна запись, поэтому команда отказывается угадывать. Решите, какое написание верное, удалите другое через `git rm --cached`, сделайте коммит и повторите исправление.

### Слишком длинные пути

```text
C1083: Cannot open compiler generated file
error: unable to create file <path>: Filename too long
```

Первая строка является сбоем компилятора на пределе длины пути Windows; примечание в `BuildTools/cmake/stages/ThirdParty.cmake` фиксирует его на 297 символах при сборке управляемой среды выполнения. Вторая строка — обычная формулировка Git при выключенном `core.longpaths`; она не воспроизводилась. Исправляйте в таком порядке:

1. Перенесите checkout в короткий корень, например `C:\fo\game`. Всё, что ниже, включая `Build/`, увеличивает длину.
2. Выполните `git config --global core.longpaths true` до клонирования.
3. Включите настройку длинных путей операционной системы (`LongPathsEnabled` или политику «Enable Win32 long paths»). Engine её не устанавливает.
4. Для Managed C# в Windows задайте `FO_DOTNET_DIR` как короткий каталог до configure; он заменяет каталог `dotnet` по умолчанию внутри дерева сборки.

Подробности и причины, по которым Engine покрывает это лишь частично, описаны в разделе [Подготовка рабочей станции для проекта FOnline](../how-to/project/workstation-setup.md#длинные-пути-и-ссылки-в-windows).

### Не удаётся создать символические ссылки или junction

`buildtools.py validate` создаёт ссылку `Engine`, принадлежащим Engine примерам она нужна при запуске на месте, а некоторые проекты отслеживают ссылки расширений редактора. В Windows создание символической ссылки через `mklink /d` требует повышенных прав оболочки или Режима разработчика (Developer Mode); обычное сообщение cmd — `You do not have sufficient privilege to perform this operation.` (на хосте Windows, использованном для этой страницы, оно не воспроизводилось: там уже было право создавать ссылки). Используйте одно из исправлений:

- Включите Режим разработчика или запустите оболочку с повышенными правами для одной этой команды.
- Создайте junction вместо символической ссылки для каталога. Права для этого не нужны: `New-Item -ItemType Junction -Path Engine -Target <engine-checkout>`.
- Для ссылок, которые отслеживает проект, задайте `git config core.symlinks true` до checkout. При выключенной настройке Git записывает символическую ссылку как небольшой текстовый файл с путём цели, а инструмент, ожидающий каталог, падает с ошибкой «не найдено». Исправьте настройку и выполните checkout репозитория заново.

## Состояние workspace и запекания

### Часть workspace не готова

```text
Workspace part <name> is not ready (<version>)
Workspace EMSDK is not prepared: <path>
Toolset build directory not found: <path>
FO_XWIN_ROOT is not set. Run prepare-workspace.sh windows-cross to splat the Windows SDK.
```

Подготовленные части записывают `<part>-version.txt` в `FO_WORKSPACE`. Первое сообщение означает, что маркер отсутствует или содержит другую версию: часть здесь никогда не готовилась, обновление Engine изменило её закрепление либо команда запущена из другого каталога и, значит, с другим `Workspace`. Остальные три сообщают, что сборке нужна часть, не подготовленная в workspace, который определил BuildTools.

1. Выведите определённые пути через `env --summary-only` и убедитесь в значении `FO_WORKSPACE`.
2. Выполните ту же команду `prepare-host-workspace` или `prepare-workspace` без `--check`.
3. Не создавайте и не правьте файлы `*-version.txt` вручную; они подтверждают, что содержимое каталога соответствует версии.
4. Для Windows SDK в Linux используйте именованную возможность: `prepare-host-workspace linux windows-cross`.

### Устаревший каталог сборки отвергает toolchain

```text
CMake Error: Error: generator : Ninja Multi-Config
Does not match the generator used previously: Visual Studio 17 2022
Either remove the CMakeCache.txt file and CMakeFiles directory or choose a different binary directory.
```

Каталог сборки привязан к своему первому генератору и компилятору. После смены пресета, установки нового компилятора, переноса проекта или переключения генераторов выполняйте configure в новый каталог. Если вы сохраняете каталог, удалите только его `CMakeCache.txt` и `CMakeFiles`, как сказано в сообщении. Не удаляйте из-за этой ошибки результат запекания или workspace.

### Занята блокировка запекания или отладки

```text
Resource baking output lock is unavailable
AngelScript debug output lock is unavailable
```

Другой процесс владеет соседним файлом `<output-directory>.lock` (или `AS_DEBUG.lock` в сборке AngelScript с отладочной инструментацией). Неудавшийся baker оставляет прежние отчёты, хеш и ресурсы без изменений. Найдите владельца по пути исполняемого файла и командной строке, дождитесь его завершения и повторите. Не удаляйте файл блокировки: существующий файл не означает, что запекание идёт, а удаление удерживаемой блокировки POSIX позволяет второму процессу заблокировать новый файл с тем же именем. Не запускайте запекание, пока игровой процесс ещё читает тот же результат. Правила владения определены в разделе [Конвейер запекания ресурсов](../explanation/content-pipeline/baking.md#владение-выходным-каталогом-и-конкурирующие-процессы).

### Ресурсы сервера и клиента получены из разных запеканий

```text
Connected client <host> runs metadata version <a> while the server runs <b> - its synced resources do not match this server
Distributed client resources were baked apart from the server resources
Client updater: synced resources run metadata version <a> while the server runs <b>, resources <directory>
Baked metadata does not start with the metadata file marker
Baked metadata file version does not match the engine, resources must be rebaked
Baked metadata carries no version, resources must be rebaked
Build hash file <path> has wrong hash
```

Сервер и каждый обслуживаемый им клиент должны работать на ресурсах и бинарных файлах одного запекания. Данные сущностей адресуют свойства по их порядку в запечённых метаданных, поэтому смешанные наборы отвергаются намеренно, а не рассматриваются как режим совместимости. Последнее сообщение приходит от упаковщика и означает, что собранный результат относится к другому коммиту или configure, чем упаковываемый.

1. Пересоберите и перезапеките из единого чистого состояния: повторите configure, пересоберите цели, выполните `ForceBakeResources`.
2. Разверните пакеты сервера в `Common.ResourcesPath` и клиентский дистрибутив в его дочернем каталоге `Client` вместе.
3. После переключения коммитов повторите configure и пересоберите проект до нового запекания. `FO_BUILD_HASH` вкомпилирован в бинарные файлы и записывается в маркеры запекания, поэтому более старый бинарный файл не может завершить более новое запекание.
4. Сохраняйте `Network.ForceMetadataVersion` для имитации устаревшего клиента в тестах; это не исправление.

Формат метаданных и четыре слоя, которые его обеспечивают, описаны в разделе [Сгенерированный API и метаданные](../reference/metadata/index.md).

### Сбой загрузок

```text
Download <label> failed (<reason>), attempt <n>/5, retry in <seconds>s
received <written> of <declared> bytes
```

Подготовка workspace загружает архивы с хостов поставщиков и делает до пяти попыток. `received <written> of <declared> bytes` означает, что соединение оборвалось раньше времени; BuildTools сравнивает число байтов с `Content-Length` источника, чтобы усечённый архив не распаковался и не привёл к далёкому по месту сбою. Повторите на стабильной сети либо поставьте перед загрузками кэширующее зеркало через `FO_DOWNLOAD_MIRROR`, кэш подготовленного workspace через `FO_WORKSPACE_CACHE` и `FO_CI_CA` для машины, чьё хранилище корневых сертификатов нельзя починить. Переменные и их правила описаны в разделе [Процесс сборки](../how-to/build/index.md#загрузка-через-собственное-зеркало). Зеркало не охватывает `git clone` Emscripten SDK и исходного кода среды выполнения .NET, которые обращаются к GitHub напрямую.

## Параметры проекта

### Опечатка или снятая опция останавливает configure

`SetOption(<name> <value>)` задаёт переменную, если она ещё не определена; `SetOptionValues` сохраняет значение в кэше CMake. Обе команды проверяют каждое имя с префиксом `FO_`, потому что этот префикс принадлежит Engine. Снятое имя останавливает configure прямо в вызове, и сообщение называет замену. Типичный пример — снятый флаг бэкенда Mono:

```cmake
SetOption(FO_MONO_SCRIPTING ON)
```

```text
SetOption: FO_MONO_SCRIPTING was removed. Use FO_MANAGED_SCRIPTING for the Managed C# backend
```

`FO_MONO_SCRIPTING` намеренно не является псевдонимом: замените строку на `FO_MANAGED_SCRIPTING` и осознанно выберите `ON` или `OFF`. Так же, каждый со своей подсказкой, отвергаются `FO_BUILD_EDITOR`, `FO_INTERNAL_CONFIG_CAPACITY` и `FO_DISABLE_UNQLITE`.

Любое другое имя `FO_`, которое конвейер не объявляет, останавливает configure на стадии Init сразу после объявления опций, до проверок компилятора и платформы. Имя, заданное позже, например в хуке стадии, отвергается в своём вызове. Сообщение начинается с команды, получившей имя:

```text
SetOptionValues: FO_MANAGED_SCRIPTNG is not a declared option. Check the name against the CMake option reference; project variables need a prefix other than FO_
```

Сверьте написание с разделом [Параметры проекта CMake](../reference/cmake/options.md). Имена без префикса `FO_` не проверяются, поэтому переменные проекта вроде `MYGAME_FEATURE` продолжают работать.

Проверка охватывает только имена, переданные в `SetOption` и `SetOptionValues`. Имя, переданное через `-D`, в `cacheVariables` пресета или как переменная окружения, не проверяется, а необъявленное не имеет эффекта. Видимые последствия зависят от опции: цель бэкенда вроде `CompileManagedScripts` не существует, поскольку создаётся только при включённом `FO_MANAGED_SCRIPTING`, либо возможность остаётся в значении по умолчанию `OFF`. Обнаружить это можно по журналу configure. Каждая объявленная опция печатается один раз как `-- <name> = <value>`:

```text
-- FO_ANGELSCRIPT_SCRIPTING = OFF
-- FO_MANAGED_SCRIPTING = OFF
```

Опция, которую вы задали и которая в этом списке не появляется, не объявлена. Для имени, переданного через `-D` или в `cacheVariables` пресета, CMake может также завершить configure сообщением `Manually-specified variables were not used by the project`. Сравните свои файлы с объявленными именами в разделе [Параметры проекта CMake](../reference/cmake/options.md) либо запустите этот скрипт проверки из корня проекта. Сохраните его вне репозитория, например как `check_options.py`:

```python
import json
import re
import sys
from pathlib import Path

model = json.loads(Path("Engine/BuildTools/cmake/ProjectInterface.json").read_text(encoding="utf-8"))
declared = {option["name"] for option in model["options"]}
pattern = re.compile(r"\bFO_[A-Z][A-Z0-9_]*\b")

for name in sys.argv[1:]:
    text = Path(name).read_text(encoding="utf-8")
    for token in sorted(set(pattern.findall(text)) - declared):
        print(f"{name}: {token} is not a declared option")
```

```powershell
py -3 check_options.py CMakeLists.txt CMakePresets.json
```

```bash
python3 check_options.py CMakeLists.txt CMakePresets.json
```

Вывод — список для проверки, а не вердикт. В него попадают и переменные, не являющиеся опциями, например `FO_SERVER_LIBS` или `FO_ENGINE_ROOT`; любое другое имя — опечатка или снятая опция.

Два снятых входа отвергаются и вне этих команд. Если определена переменная `FO_DISABLE_MONGO` или `FO_DISABLE_SQLITE` (в кэше, окружении или вызовом `SetOption`/`SetOptionValues`), configure останавливается с текстом ниже; после этого выберите `FO_DATABASE_MONGO` или `FO_DATABASE_SQLITE` через `SetOptionValues` и очистите устаревшую запись кэша:

```text
FO_DISABLE_MONGO was removed. Select positive FO_DATABASE_MONGO/FO_DATABASE_SQLITE options with SetOptionValues; clear the retired cache entry
```

### Отсутствует обязательная опция

```text
FO_MAIN_CONFIG not specified
```

Стадия Init требует `FO_MAIN_CONFIG`, `FO_DEV_NAME`, `FO_NICE_NAME`, `FO_GEOMETRY`, `FO_MAP_HEX_WIDTH`, `FO_MAP_HEX_HEIGHT`, `FO_MAP_CAMERA_ANGLE` и `FO_OUTPUT_PATH` и сообщает о первой пустой текстом `<name> not specified`. Обычная причина — порядок: вызовы `SetOption` после `StartProjectGeneration()` выполняются слишком поздно, потому что стадия проверяет значения в ходе своей работы. Переместите все опции проекта перед первым вызовом стадии или передайте их записью `-D`, пресетом либо переменной окружения `FO_`.

Родственные проверки в тех же стадиях:

```text
FO_MAP_MAX_SIZE must be an integer from 200 to <limit> (<reason>); got '<value>'
FO_UNITY_BUILD_BATCH_SIZE must be a positive integer; got '<value>'
AngelScript compiler build can not be without AngelScript scripting enabled
Code coverage build can not be mixed with other builds
Invalid requested configuration type
```

Первые два отвергают значение вне диапазона, третье — сборку `FO_BUILD_ASCOMPILER` без `FO_ANGELSCRIPT_SCRIPTING`, четвёртое — сборку покрытия кода в сочетании с другой опцией сборки, а последнее — `CMAKE_BUILD_TYPE`, которого нет в списке проекта. Набор опций и правила конфигураций определены в руководстве [Конфигурация игрового проекта](../how-to/build/project-configuration.md).

### Стадии вызваны не по порядку или недопустимое переопределение

```text
Pipeline: stage 'ProjectOptions' invoked before 'Init'.  The canonical order is:
Invalid boolean environment override FO_MANAGED_SCRIPTING='maybe'. Use 1/0, ON/OFF, TRUE/FALSE, or YES/NO
```

Первое сообщение заканчивается десятью именами стадий по порядку; корневой `CMakeLists.txt` должен вызывать каждую ровно один раз в каноническом порядке, завершая `FinalizeProjectGeneration()`. Скопируйте последовательность из `Examples/MinimalProject/CMakeLists.txt`. Второе означает, что переменная окружения `FO_` содержит значение, не являющееся булевым. Переопределения из окружения приоритетнее пресетов и `SetOption`, поэтому уберите переменную из оболочки или профиля, который её экспортирует.

## Сбои, описанные в других документах

| Симптом | Страница |
| --- | --- |
| `ConfigBakerException: Main config baking error`, `Nothing was built in the module`, отсутствие `WorldTime` или `GuiScreens`, `[starter-smoke] server timed out` | [Первый headless-проект FOnline](../tutorials/first-project.md#устранение-неполадок) |
| `Config file not found`, `Sub config not found`, `Resource pack name not specified`, опечатка, ставшая пользовательской настройкой | [Конфигурация игрового проекта](../how-to/build/project-configuration.md#частые-ошибки) |
| Сгенерированный исходный код не компилируется, инкрементальные ресурсы остаются устаревшими, стороны метаданных расходятся | [Работа с генерируемым содержимым](../how-to/build/generated-content.md#восстановление) |
| Падения, трассировки стека, отладчик скриптов и сбои загрузки Managed C# | [Нативная отладка, AngelScript и Managed C#](debugging.md) |
| Подготовка toolchain для Web и Android | Руководства по сборке для [Web](../how-to/platforms/web-debugging.md) и [Android](../how-to/platforms/android-debugging.md) |
| `clang-format version 20 not found (set FO_CLANG_FORMAT or install clang-format-20 on PATH)` | [Контракт форматтера](../how-to/scripting/style-and-refactoring.md#контракт-форматтера) |

## Контрольный список проверки

Прежде чем сообщать о проблеме настройки, подтвердите каждый пункт и приложите вывод:

- Первая строка ошибки, скопированная из журнала, вместе со стеком вызовов.
- Вывод `env --summary-only` с удалёнными секретными значениями.
- `git submodule status Engine` и `git -C Engine rev-parse HEAD`.
- Версии Git, Python (`py -3` или `python3`), CMake и компилятора.
- Успешно ли тот же шаг проходит в принадлежащем Engine [первом проекте](../tutorials/first-project.md) на этой же машине.
