---
layout: default
title: Подготовка рабочей станции для проекта FOnline
locale: ru
document_id: project-workstation-setup
permalink: /Docs/ru/how-to/project/workstation-setup.html
---

# Подготовка рабочей станции для проекта FOnline
<!-- docs-translation: {"document_id":"project-workstation-setup","locale":"ru","source_path":"Docs/en/how-to/project/workstation-setup.md","source_sha256":"646ade263ec4671dfe52f6d231a4c72e66902aea1d12e7aadd4837e79aabb907"} -->
Это руководство готовит машину с Windows или Linux (и macOS в той мере, в какой
её поддерживают инструменты Engine) до того, как игровая команда создаст или
склонирует репозиторий, встраивающий Engine. Здесь перечислены инструменты для
установки, показано, как проверить каждый из них, объяснены принадлежащие Engine
команды подготовки хоста и workspace, а в конце дан контрольный список, последний
шаг которого — первая зелёная сборка.

Общие предварительные требования и список платформ находятся в разделе
[Процесс сборки](../build/index.md#предварительные-требования) и в
[матрице поддержки](../../reference/platforms/support-matrix.md); эта страница их
не повторяет. Команды, помеченные для Windows, выполнялись на Windows x64.
Команды для Linux и macOS выведены из исходного кода BuildTools и на этих хостах
не запускались.

## Проверенные исходные пути

- `BuildTools/README.md`
- `BuildTools/buildtools.py`
- `BuildTools/Init.cmake`
- `BuildTools/cmake/stages/Init.cmake`
- `BuildTools/cmake/stages/Codegen.cmake`
- `BuildTools/cmake/stages/ThirdParty.cmake`
- `BuildTools/cmake/ProjectInterface.json`
- `BuildTools/build.cmd`
- `BuildTools/find-python.sh`
- `BuildTools/find-python.cmd`
- `BuildTools/prepare-workspace.sh`
- `BuildTools/prepare-mac-workspace.sh`
- `BuildTools/prepare-win-workspace.ps1`
- `Docs/en/reference/buildtools/commands.md`
- `Docs/en/reference/platforms/support-matrix.md`
- `Docs/en/how-to/build/index.md`
- `Docs/en/how-to/platforms/web-debugging.md`
- `Examples/MinimalProject/CMakePresets.json`
- `Examples/MinimalProject/CMakeLists.txt`
- `Source/Essentials/DiskFileSystem.cpp`
- `ThirdParty/dotnet-runtime`
- `.github/workflows/validate.yml`

## Решите, что устанавливать

Базовый набор нужен любой игре. Остальное зависит от скриптового backend и
целей, которые собирает проект.

| Что нужно | Что установить |
| --- | --- |
| Любой проект | Git, Python 3.12 или новее, CMake, нативный toolchain C++ |
| Скрипты Managed C# | Базовый набор, а также .NET SDK и, в Windows, поставщик `llvm-nm` |
| Web-клиент | Базовый набор, а также закреплённый Emscripten SDK, Node.js и JDK |
| Android-клиент | Базовый набор, а также закреплённые Android SDK и NDK и JDK 17 |
| Бинарные файлы Windows на хосте Linux | Базовый набор, а также закреплённое дерево MSVC SDK (`xwin`) |

AngelScript не требует ничего сверх базового набора: компилятор собирается из
исходного кода Engine как часть проекта.

## Обязательные инструменты

| Инструмент | Требование | Проверка |
| --- | --- | --- |
| Git | Любой актуальный выпуск. В Windows: Git for Windows. Добавляйте Git LFS, только если проект хранит в нём бинарные файлы. | `git --version` |
| Python 3 | 3.12 или новее. BuildTools передаёт `onexc=` в `shutil.rmtree`, а этот параметр существует начиная с 3.12, и каждая команда `buildtools.py` отказывается работать с более старым интерпретатором; CI Engine работает на 3.13. CMake также нужен интерпретатор Python 3, который может найти `find_package(Python3 ... Interpreter)`. | Windows: `py -3 --version`; в остальных системах: `python3 --version` |
| CMake | 3.22 или новее (`cmake_minimum_required(VERSION 3.22)` в `BuildTools/Init.cmake`). Presets примеров объявляют схему `"version": 3`, которую читает CMake 3.21. | `cmake --version` |
| Toolchain Windows | Visual Studio 2022 17.14 или новее с инструментами сборки C++ (MSVC toolset 14.44, `MSVC_VERSION` 1944). | `py -3 Engine\BuildTools\buildtools.py host-check windows` |
| Toolchain Linux | GCC 13 или новее либо Clang 20 или новее. | `g++ --version` или `clang++ --version` |
| Toolchain macOS | Xcode с инструментами командной строки (проверка configure принимает любой AppleClang). | `xcode-select -p` |
| Ninja | Только если preset выбирает генератор Ninja. Preset `linux` в `Examples/MinimalProject` использует `Ninja Multi-Config`. Инструменты CMake в Visual Studio поставляют копию, которую BuildTools находит в Windows. | `ninja --version` |
| .NET SDK | Только для Managed C#. SDK должен поддерживать `ManagedScript.TargetFramework` проекта; примеры используют `net10.0`, а CI Engine устанавливает `10.0.x`. | `dotnet --list-sdks` |
| `llvm-nm` | Managed C# в Windows. Установите компонент Visual Studio «C++ Clang tools for Windows» или LLVM либо задайте `FO_MANAGED_NM`. | `llvm-nm --version` |
| Node.js и JDK | Только для Web-сборок. Группа Linux `web-packages` устанавливает `nodejs` и `default-jre`; маршрут Windows устанавливает только SDK. | `node --version`, `java -version` |
| JDK 17 | Только для Android-сборок (`openjdk-17-jdk` в группе Linux `android-packages`). | `java -version` |
| clang-format 20 | Только если проект переиспользует обёртку форматтера Engine; см. [Контракт форматтера](../scripting/style-and-refactoring.md#контракт-форматтера). Если его нет в `PATH`, укажите на него через `FO_CLANG_FORMAT`. | `clang-format --version` |

Минимальные версии компилятора и CMake проверяются во время configure, поэтому
слишком старая машина падает с конкретным сообщением, а не с поздней ошибкой
компиляции; эти сообщения перечислены в разделе
[Устранение неполадок настройки проекта](../../troubleshooting/setup.md#инструментарий-и-инструменты-хоста).

### Выбор интерпретатора Python

Используйте `py -3` в Windows и `python3` во всех остальных системах. Имя
`python` ненадёжно: на машине, где установка Python 2 всё ещё стоит в `PATH`
раньше Python 3, `python` запускает Python 2, и `buildtools.py` останавливается
с `SyntaxError` прежде, чем успевает выполнить какую-либо проверку. Python 3
старше 3.12 доходит до проверки и останавливается с сообщением
`BuildTools requires Python 3.12 or newer, but <path> is Python <version>`.

Обёртки Engine выбирают интерпретатор, запуская его и сравнивая его версию с
минимумом 3.12. Скрипты Windows (`BuildTools/*.cmd` через
`BuildTools/find-python.cmd` и `BuildTools/prepare-win-workspace.ps1`) пробуют
`py -3`, затем `python`. Скрипты POSIX (`BuildTools/*.sh` через
`BuildTools/find-python.sh`) пробуют `python3`, затем самый новый `python3.N` из
`PATH`, затем `python`. Кандидат ниже минимума, включая Python 2, пропускается;
если не подходит ни один, обёртка завершается с кодом 1 и сообщением
`Python 3.12 or newer not found`. Ключ `-3` лаунчера `py` выбирает самый новый
зарегистрированный Python 3, поэтому в Windows установите 3.12 или новее через
официальный установщик.

```powershell
py -3 --version
python --version
```

```bash
python3 --version
command -v python3 python
```

В любой команде `python3` или `py -3` из документации Engine можно использовать
интерпретатор, прошедший эту проверку. Используйте UTF-8 для каждого файла
проекта, содержащего текст за пределами ASCII.

## Запуск проверок хоста Engine

BuildTools определяет свои пути от текущего каталога, поэтому запускайте эти
команды из корня игрового репозитория (или, пока вы осваиваетесь, из временного
каталога). `env --summary-only` выводит пути, которые использовала бы команда, и
ничего не меняет:

```powershell
py -3 Engine\BuildTools\buildtools.py env --summary-only
```

```bash
python3 Engine/BuildTools/buildtools.py env --summary-only
```

Проверьте `FO_PROJECT_ROOT` (по умолчанию текущий каталог), `FO_ENGINE_ROOT`
(родитель `BuildTools`) и `FO_WORKSPACE` (`Workspace` внутри текущего каталога).
Запуск из другого каталога молча выбирает другой workspace.

### host-check

`host-check` проверяет минимальный набор инструментов для одного хоста и при
успехе печатает `Host is ready!`. При сбое команда печатает по одной строке на
каждую проблему и завершается с кодом 1.

| Хост | Что проверяет | Сообщения |
| --- | --- | --- |
| `windows` | `vswhere` находит Visual Studio с компонентом `Microsoft.VisualStudio.Component.VC.Tools.x86.x64`; `cmake` есть в `PATH` | `Please install Visual Studio with C++ Build Tools or Build Tools for Visual Studio`, `Please install CMake from https://cmake.org` |
| `linux` | `cmake` есть в `PATH` | `Please install CMake` |
| `macos` | `xcode-select -p` выполняется успешно; `cmake` есть в `PATH` или находится по пути `/Applications/CMake.app/Contents/bin/cmake` | `Please install Xcode from AppStore`, `Please install CMake from https://cmake.org` |

Прежде всех этих строк `buildtools.py` на любом хосте сравнивает запустивший его
интерпретатор с минимумом 3.12 и, если он старше, завершается с кодом 1 и
сообщением из раздела [Выбор интерпретатора Python](#выбор-интерпретатора-python).
Помимо этого минимума проверка намеренно поверхностна: она не читает версии
инструментов, поэтому старый CMake или компилятор проходит, пока его не отвергнет
configure. Считайте её первым фильтром, а затем подтвердите версии командами из
таблицы выше.

```powershell
py -3 Engine\BuildTools\buildtools.py host-check windows
```

```bash
python3 Engine/BuildTools/buildtools.py host-check linux
```

В macOS равнозначная команда — `host-check macos`. Матрица поддержки отмечает
macOS как проверяемый сборкой в CI только для настольного клиента.

### prepare-host-workspace

`prepare-host-workspace <host> [feature ...] [--check]` выполняет проверку
хоста, создаёт `FO_WORKSPACE` и `FO_OUTPUT` и подготавливает запрошенные части
workspace. Если возможность (feature) не указана, используется значение по
умолчанию для хоста: `all` в Linux, `toolset` в Windows, ничего в macOS.
`--check` сообщает о готовности, ничего не устанавливая и не собирая, и
завершается с ненулевым кодом и сообщением
`Workspace part <name> is not ready (<version>)`, если часть отсутствует или была
подготовлена для другой версии.

Подготовленные части записывают в workspace файл-маркер `<part>-version.txt`.
Часть пропускается, пока её маркер совпадает с версией, которую ожидает
BuildTools, поэтому повтор команды обходится дёшево, а обновление Engine,
меняющее закрепление, заново подготавливает только затронутую часть.

| Хост | Возможность | Что подготавливает |
| --- | --- | --- |
| Linux | `common-packages`, `linux-packages`, `web-packages`, `android-packages`, `windows-cross-packages`, `msi-packages` | Пакеты Debian или Ubuntu через `apt-get`; `common-packages` устанавливается первой, если выбрана любая группа пакетов |
| Linux | `all-packages` | `linux-packages`, `web-packages`, `android-packages`, `windows-cross-packages` и `msi-packages` |
| Linux | `toolset`, `web`, `android-arm32`, `android-arm64`, `android-x86`, `dotnet`, `windows-cross`, `msan-libcxx` | Части workspace: дерево инструментов хоста, Emscripten, Android SDK и NDK, исходный код среды выполнения .NET, MSVC SDK через `xwin`, libc++ для MemorySanitizer |
| Linux | `all` | Части workspace `toolset`, `emscripten`, `android-sdk`, `android-ndk`, `dotnet` и `xwin`; группы пакетов и `msan-libcxx` не входят |
| Windows | `toolset`, `web`, `wix`, `all` | Дерево инструментов хоста, Emscripten, toolset WiX v3; `all` означает все три |
| macOS | нет | `prepare-host-workspace macos` не принимает возможностей и только выполняет проверку хоста |

Таблица задаёт полный словарь. Имя вне строк выбранного хоста, например `dotnet`
в Windows, `web` в macOS или выведенные из употребления `linux` и `packages`,
останавливает команду с кодом 2 до какой-либо подготовки, а сообщение
перечисляет имена, которые принимает этот хост, например
`prepare-host-workspace windows does not support dotnet: windows accepts toolset, web, wix, all (default: toolset)`.

Установка пакетов намеренно отделена от подготовки workspace. Возможность вроде
`web` никогда не устанавливает пакеты, поэтому на свежем хосте Linux сначала
передайте соответствующую возможность `*-packages`. Подготовщику Linux нужен
`apt-get` и root или `sudo`; когда группа этого требует, он добавляет apt-источник
LLVM для мажорной версии 20. Хосты, отличные от Debian или Ubuntu, устанавливают
те же инструменты своими средствами и пропускают возможности пакетов.

Часть `toolset` выполняет configure проекта в текущем каталоге, поэтому ей нужен
корневой `CMakeLists.txt`, подключающий `Engine/BuildTools/Init.cmake`.
Запускайте её, когда репозиторий уже существует. При каждой подготовке этой
части она удаляет и заново создаёт `Workspace/build-win64-toolset` (Windows) или
`Workspace/build-linux-toolset` (Linux), а `buildtools.py toolset <target>`
собирает цель в этом дереве.

```powershell
py -3 Engine\BuildTools\buildtools.py prepare-host-workspace windows toolset --check
py -3 Engine\BuildTools\buildtools.py prepare-host-workspace windows toolset
```

```bash
python3 Engine/BuildTools/buildtools.py prepare-host-workspace linux common-packages linux-packages
python3 Engine/BuildTools/buildtools.py prepare-host-workspace linux toolset --check
```

### prepare-workspace

`prepare-workspace <part> [part ...] [--check]` подготавливает части workspace
напрямую, без проверки хоста и групп пакетов. Доступные части: `toolset`,
`emscripten`, `android-sdk`, `android-ndk`, `dotnet`, `xwin`, `wix` и
`msan-libcxx`. Используйте эту команду, если сопоставление хоста выше не
предлагает нужную часть для вашей ОС. Например, часть `dotnet` клонирует
закреплённый исходный код среды выполнения .NET в `Workspace/dotnet/runtime`, и в
Windows она доступна только через эту команду:

```powershell
py -3 Engine\BuildTools\buildtools.py prepare-workspace dotnet --check
```

```bash
python3 Engine/BuildTools/buildtools.py prepare-workspace emscripten android-ndk dotnet
```

Помимо этого исходного кода среды выполнения, Managed C# нужен .NET SDK для
скриптов; эта часть SDK не устанавливает. `msan-libcxx` существует только в Linux
и касается только контрибьюторов. `xwin` подготавливает MSVC SDK на хосте Linux,
а `wix` подготавливает toolset WiX v3 только в Windows. Платформенными
процедурами, которым нужно больше базового набора, владеют страницы
[Web](../platforms/web-debugging.md), [Android](../platforms/android-debugging.md)
и [Managed C#](../scripting/managed-csharp.md).

### Скрипты-обёртки

Три скрипта вызывают `prepare-host-workspace` за вас. Они добавляют описанный
выше поиск Python и слово `check`, которое превращается в `--check`; все
остальные аргументы передаются без изменений, поэтому обёртки принимают ровно
возможности из таблицы для своего хоста и так же завершаются ошибкой на любом
другом имени.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Engine\BuildTools\prepare-win-workspace.ps1 toolset check
```

```bash
bash Engine/BuildTools/prepare-workspace.sh common-packages linux-packages toolset
bash Engine/BuildTools/prepare-mac-workspace.sh check
```

Особенности отдельных обёрток:

- `prepare-workspace.sh` отказывается работать без аргументов; передайте хотя бы
  одну возможность или `check`. `prepare-win-workspace.ps1` без аргументов
  подготавливает значение Windows по умолчанию `toolset`.
- В режиме `check` при отсутствии части `prepare-win-workspace.ps1` печатает
  `Workspace is not ready!` и завершается с кодом 10. Любой другой сбой, включая
  неизвестную возможность, завершает его с кодом 1.
- У `prepare-mac-workspace.sh` нет частей workspace для подготовки. Если проверка
  хоста не проходит вне режима `check`, скрипт ждёт нажатия Enter и повторяет её
  вместо выхода; неизвестная возможность сразу завершает его с кодом 2.

Эта страница вызывает `buildtools.py` напрямую; он принимает тот же словарь, что
и обёртки.

## Переменные workspace и кэша

BuildTools читает только переменные с префиксом `FO_`. Первые пять строк
определяют, куда попадают файлы; задайте их до первой команды и не меняйте,
потому что там хранятся маркеры workspace.

| Переменная | По умолчанию | Действие |
| --- | --- | --- |
| `FO_PROJECT_ROOT` | текущий каталог | Корень репозитория: используется для configure части `toolset`, запросов к Git и исправления регистра в checkout |
| `FO_ENGINE_ROOT` | родитель `BuildTools` | Checkout Engine |
| `FO_WORKSPACE` | `Workspace` внутри текущего каталога | Подготовленные SDK, маркеры и деревья сборки инструментов |
| `FO_OUTPUT` | `output` внутри workspace | Дерево вывода для сборок, которыми управляет BuildTools |
| `FO_BINARY_OUTPUT_POSTFIX` | пусто | Суффикс в именах каталогов вывода бинарных файлов |
| `FO_DOWNLOAD_MIRROR` | не задана | Базовый URL pull-through-зеркала для загрузки архивов |
| `FO_WORKSPACE_CACHE` | не задана | Базовый URL для подготовленных деревьев Emscripten и MSVC SDK, которые собираются один раз и загружаются целиком |
| `FO_CI_TOKEN` | не задана | Bearer-токен, который отправляется только на хост двух URL выше |
| `FO_CI_CA` | не задана | Путь к PEM-файлу с дополнительными якорями доверия, которые добавляются к системному хранилищу |
| `FO_CLANG_FORMAT` | поиск в `PATH` | Явный бинарный файл `clang-format` для `format-source` |
| `FO_DOTNET_DIR` | `dotnet` внутри дерева сборки | Рабочий каталог сборки управляемой среды выполнения; в Windows укажите в нём короткий путь |
| `FO_MANAGED_RUNTIME_PREBUILT` | не задана | Опубликованное дерево `output/mono/<triplet>`; обязательно для сборки под Windows на хосте, отличном от Windows |
| `FO_ANDROID_HOME`, `FO_ANDROID_SDK_ROOT`, `FO_ANDROID_NDK_ROOT` | подготовленные части workspace | Явные расположения Android SDK и NDK |

Семантика зеркала, проверка длины содержимого и правило промаха кэша описаны в
разделе [Процесс сборки](../build/index.md#загрузка-через-собственное-зеркало);
использование в CI и обращение с секретами описаны в руководстве
[Настройка непрерывной интеграции для игрового проекта](continuous-integration.md).
Зеркало покрывает загрузку архивов. Вызовы `git clone`, которые делает подготовка
workspace, например для Emscripten SDK и исходного кода среды выполнения .NET,
обращаются к GitHub напрямую.

Любую опцию проекта, объявленную в `BuildTools/cmake/ProjectInterface.json`,
также можно переопределить одноимённой переменной окружения, и такое
переопределение имеет приоритет над значениями `-D`, presets и значениями по
умолчанию из `SetOption`. Поэтому устаревшая переменная `FO_*`, оставшаяся в
профиле оболочки, меняет сборку, не появляясь в файлах проекта. Если результат
configure вас удивил, выведите переменные `FO_` своей оболочки:

```powershell
Get-ChildItem Env: | Where-Object { $_.Name -like 'FO_*' }
```

```bash
env | grep '^FO_'
```

## Длинные пути и ссылки в Windows

### Длинные пути

Engine и Git обрабатывают длинные пути по-разному, и проект должен покрыть то,
что не покрывает Engine.

- Нативный код Engine в Windows переходит на пути расширенной длины для длинных
  имён (`Source/Essentials/DiskFileSystem.cpp`), поэтому доступ к файлам из среды
  выполнения и сборщика ресурсов сам по себе не упирается в `MAX_PATH`.
- `buildtools.py` передаёт `core.longpaths=true` только в собственный клон
  исходного кода среды выполнения .NET. Он не меняет ни вашу конфигурацию Git, ни
  ваш репозиторий, ни реестр Windows.
- Компиляторы, MSBuild и сборка среды выполнения .NET по-прежнему видят пути в
  дереве сборки. Сборка управляемой среды выполнения документирует сбой на 297
  символах с `C1083: Cannot open compiler generated file`, и именно поэтому
  существует переопределение `FO_DOTNET_DIR`.
- На этой ревизии Engine самый длинный отслеживаемый путь внутри `Engine/`
  занимает 155 символов. К нему добавляются корень вашего checkout, `Engine/` и
  имя каталога сборки.

Настройте это до клонирования:

```powershell
git config --global core.longpaths true
```

Кроме того, держите корень checkout коротким, например `C:\fo\game`, и включите
политику Windows «Enable Win32 long paths» (`LongPathsEnabled` в
`HKLM\SYSTEM\CurrentControlSet\Control\FileSystem`), если ваши инструменты её
поддерживают. Значение реестра — настройка операционной системы, которую Engine
не задаёт и не проверяет. Для проекта Managed C# также задайте `FO_DOTNET_DIR`
как короткий каталог, например `C:\fo\dotnet`.

### Символические ссылки и junction

Самому Engine нужно немного ссылок, а сколько ещё добавить, решает проект.

- Репозиторий Engine не отслеживает символических ссылок, поэтому для его
  клонирования право на создание ссылок не нужно.
- `buildtools.py validate` создаёт ссылку `Engine` внутри своего временного
  проекта: `mklink /d` в Windows, символическую ссылку в POSIX. Windows разрешает
  `mklink /d` администраторам или при включённом Режиме разработчика
  (Developer Mode).
- Принадлежащим Engine примерам при запуске на месте нужна запись `Engine` рядом
  с их `CMakeLists.txt`. [Руководство по первому проекту](../../tutorials/first-project.md)
  создаёт её как junction (`New-Item -ItemType Junction`), для которого особые
  права не нужны.
- Проекту, который отслеживает символические ссылки в собственном репозитории,
  например папку расширения редактора, указывающую внутрь `Engine/BuildTools/`,
  нужны `core.symlinks`, равный `true`, и описанное выше право на ссылки. Без них
  Git извлекает символическую ссылку как обычный текстовый файл с путём цели, и
  инструмент, ожидающий каталог, падает. Проверьте настройку командой
  `git config --get core.symlinks` и после её исправления создайте checkout
  заново.

## Проверка рабочей станции

Пройдите этот список на каждой новой машине. Пунктам 1–4 проект не нужен.

1. `git --version` сообщает выпуск, который вы собираетесь использовать, а в
   Windows `git config --get core.longpaths` печатает `true`.
2. `py -3 --version` (Windows) или `python3 --version` (остальные системы)
   сообщает 3.12 или новее.
3. `cmake --version` сообщает 3.22 или новее, желательно 3.28 или новее.
4. `buildtools.py host-check <host>` печатает `Host is ready!`, а ваш компилятор
   удовлетворяет минимуму из таблицы инструментов.
5. Только для Managed C#: `dotnet --list-sdks` показывает SDK, соответствующий
   целевому фреймворку проекта, а в Windows находится `llvm-nm`.
6. В Windows корень репозитория короткий, а `env --summary-only` печатает
   ожидаемые пути workspace и вывода.
7. Из корня проекта `prepare-host-workspace <host> toolset --check` либо
   проходит успешно, либо сообщает о части, которую вы затем подготавливаете без
   `--check`.
8. Выполните принадлежащую Engine первую сборку из
   [руководства по первому проекту](../../tutorials/first-project.md). Она
   выполняет configure, собирает сборщик ресурсов и headless-сервер, выполняет
   сборку ресурсов, запускает сервер и требует маркеры жизненного цикла; зелёный
   прогон доказывает, что toolchain, Python, CMake и маршрут сборки ресурсов
   работают вместе.

После шага 8 переходите к руководству
[Создание репозитория игры на FOnline](create-repository.md), чтобы собрать
собственный проект, и держите открытой страницу
[Устранение неполадок настройки проекта](../../troubleshooting/setup.md) на
случай любого сбоя по пути. При сбоях среды выполнения или отладчика после
успешной сборки используйте страницу
[Нативная отладка, AngelScript и Managed C#](../../troubleshooting/debugging.md).

## Контрольный список проверки

- Для каждого инструмента из таблицы «Обязательные инструменты» на новой машине
  выведена версия.
- `host-check` завершается с кодом 0 и печатает `Host is ready!`.
- `env --summary-only` показывает нужные корень проекта, корень Engine и
  workspace.
- Руководство по первому проекту завершилось с кодом выхода 0 и всеми
  обязательными маркерами.
- Только Windows: настройка Git для длинных путей включена, а корень checkout
  короткий.
- Все отслеживаемые проектом символические ссылки существуют как ссылки, а не
  как текстовые файлы.
