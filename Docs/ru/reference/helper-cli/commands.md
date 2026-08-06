---
title: Команды вспомогательных CLI
document_id: generated-helper-cli-commands
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-helper-cli-commands","locale":"ru","source_path":"Docs/en/reference/helper-cli/commands.md","source_sha256":"d3291a632418848347bd8d3ea396f511fb64d7df07b4229fb9e4fc762c07573a"} -->

# Команды вспомогательных CLI

> Сгенерированный справочник. Не редактируйте эту страницу напрямую. Обновите `BuildTools/HelperCliInterface.json` или владеющий исполняемый парсер, затем выполните `python BuildTools/docs_helper_cli.py --write`.

[Индекс справочника](index.md) | [Команды](commands.md) | [Каноническая JSON-модель](../../../generated/helper-cli.json) | [Контракт генерации](../metadata/)

Команды приведены с точными созданными парсером usage и справкой фиксированной ширины 80 столбцов. Запускайте helper из корня репозитория движка, если владелец вызова не задаёт другой рабочий каталог.

<a id="entry-helper-cli-codegen-60abdf415d"></a>
## Генерация кода

Создаёт конфигурацию движка, регистрацию метаданных, привязки скриптов и исходники встроенных ресурсов.

- Стабильный ID: `helper-cli.codegen`
- Программа: `codegen.py`
- Владелец: `build-release`
- Аудитория: `engine-contributor`, `embedding-project-build-system`
- Владелец вызова: BuildTools/cmake/stages/Codegen.cmake
- Исходный парсер: [BuildTools/codegen.py](https://github.com/cvet/fonline/blob/master/BuildTools/codegen.py)

### Аргументы верхнего уровня

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-codegen-argument-maincfg-a02226a81e"></a><code>helper-cli.codegen.argument.maincfg</code> | <code>-maincfg</code> | <code>option</code> | да | <code>1</code> | - | - | Основной файл конфигурации. |
| <a id="entry-helper-cli-codegen-argument-buildhash-bbbdec66bb"></a><code>helper-cli.codegen.argument.buildhash</code> | <code>-buildhash</code> | <code>option</code> | да | <code>1</code> | - | - | Хеш сборки. |
| <a id="entry-helper-cli-codegen-argument-devname-47485557c0"></a><code>helper-cli.codegen.argument.devname</code> | <code>-devname</code> | <code>option</code> | да | <code>1</code> | - | - | Краткое имя игры для разработки. |
| <a id="entry-helper-cli-codegen-argument-nicename-161cc8093d"></a><code>helper-cli.codegen.argument.nicename</code> | <code>-nicename</code> | <code>option</code> | да | <code>1</code> | - | - | Читаемое имя игры. |
| <a id="entry-helper-cli-codegen-argument-embedded-f936b1a395"></a><code>helper-cli.codegen.argument.embedded</code> | <code>-embedded</code> | <code>option</code> | да | <code>1</code> | - | - | Ёмкость буфера встроенных данных. |
| <a id="entry-helper-cli-codegen-argument-internalcfg-9f8ef3aef8"></a><code>helper-cli.codegen.argument.internalcfg</code> | <code>-internalcfg</code> | <code>option</code> | да | <code>1</code> | - | - | Ёмкость буфера внутренней конфигурации. |
| <a id="entry-helper-cli-codegen-argument-enginedefine-8961bb73cd"></a><code>helper-cli.codegen.argument.enginedefine</code> | <code>-enginedefine</code> | <code>option</code> | нет | <code>1</code> | - | - | Define конфигурации движка NAME=VALUE, создаваемый как макрос в EngineConfig.gen.h. |
| <a id="entry-helper-cli-codegen-argument-meta-467f731eb8"></a><code>helper-cli.codegen.argument.meta</code> | <code>-meta</code> | <code>option</code> | да | <code>1</code> | - | - | Путь к метаданным Script API, то есть тегам ///@. |
| <a id="entry-helper-cli-codegen-argument-commonheader-888570e588"></a><code>helper-cli.codegen.argument.commonheader</code> | <code>-commonheader</code> | <code>option</code> | нет | <code>1</code> | - | - | Путь к общему header-файлу. |
| <a id="entry-helper-cli-codegen-argument-genoutput-b6e5ab6468"></a><code>helper-cli.codegen.argument.genoutput</code> | <code>-genoutput</code> | <code>option</code> | да | <code>1</code> | - | - | Каталог выходного сгенерированного кода. |
| <a id="entry-helper-cli-codegen-argument-verbose-1ae6caa3b7"></a><code>helper-cli.codegen.argument.verbose</code> | <code>-verbose</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | Подробный режим. |

### Точный вывод `--help` верхнего уровня

```text
usage: codegen.py [-h] -maincfg MAINCFG -buildhash BUILDHASH -devname DEVNAME -nicename NICENAME -embedded EMBEDDED -internalcfg INTERNALCFG [-enginedefine ENGINEDEFINE] -meta META [-commonheader COMMONHEADER] -genoutput GENOUTPUT
                  [-verbose]

FOnline code generator

options:
  -h, --help            show this help message and exit
  -maincfg MAINCFG      main config file
  -buildhash BUILDHASH  build hash
  -devname DEVNAME      dev game name
  -nicename NICENAME    nice game name
  -embedded EMBEDDED    embedded buffer capacity
  -internalcfg INTERNALCFG
                        internal config buffer capacity
  -enginedefine ENGINEDEFINE
                        engine configuration define NAME=VALUE emitted as a macro into EngineConfig.gen.h
  -meta META            path to script api metadata (///@ tags)
  -commonheader COMMONHEADER
                        path to common header file
  -genoutput GENOUTPUT  generated code output dir
  -verbose              verbose mode
```

<a id="entry-helper-cli-compile-mono-scripts-ad6011a439"></a>
## Компиляция скриптов Mono

Компилирует настроенные сборки Mono для ролей приложений движка.

- Стабильный ID: `helper-cli.compile-mono-scripts`
- Программа: `compile-mono-scripts.py`
- Владелец: `scripting`
- Аудитория: `engine-contributor`, `embedding-project-build-system`
- Владелец вызова: BuildTools/cmake/stages/ScriptsAndBaking.cmake
- Исходный парсер: [BuildTools/compile-mono-scripts.py](https://github.com/cvet/fonline/blob/master/BuildTools/compile-mono-scripts.py)

### Аргументы верхнего уровня

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-compile-mono-scripts-argument-scripts-f97fbb98f7"></a><code>helper-cli.compile-mono-scripts.argument.scripts</code> | <code>-scripts</code> | <code>option</code> | да | <code>1</code> | - | - | Путь к каталогу скриптов. |
| <a id="entry-helper-cli-compile-mono-scripts-argument-assembly-c17a7110d1"></a><code>helper-cli.compile-mono-scripts.argument.assembly</code> | <code>-assembly</code> | <code>option</code> | нет | <code>1</code> | - | - | Имя сборки. |

### Точный вывод `--help` верхнего уровня

```text
usage: compile-mono-scripts.py [-h] -scripts SCRIPTS [-assembly ASSEMBLY]

FOnline scripts generation

options:
  -h, --help          show this help message and exit
  -scripts SCRIPTS    path to scripts directory
  -assembly ASSEMBLY  assembly name
```

<a id="entry-helper-cli-codecoverage-b014400e5e"></a>
## Покрытие кода

Очищает, собирает и формирует отчёты покрытия кода движка для сгенерированных тестовых целей.

- Стабильный ID: `helper-cli.codecoverage`
- Программа: `codecoverage.py`
- Владелец: `quality`
- Аудитория: `engine-contributor`, `ci-maintainer`
- Владелец вызова: BuildTools/cmake/stages/Applications.cmake
- Исходный парсер: [BuildTools/codecoverage.py](https://github.com/cvet/fonline/blob/master/BuildTools/codecoverage.py)

### Аргументы верхнего уровня

На этом уровне аргументов нет.

### Точный вывод `--help` верхнего уровня

```text
usage: codecoverage.py [-h] {clean,run,report,full} ...

Run and analyze engine code coverage

positional arguments:
  {clean,run,report,full}
    clean               Remove previously collected coverage data and reports
    run                 Run the instrumented test binary and collect coverage data
    report              Generate text and HTML reports from collected coverage data
    full                Clean, run the instrumented binary, and generate reports

options:
  -h, --help            show this help message and exit
```

<a id="entry-helper-cli-codecoverage-command-clean-af34f1698d"></a>
### `clean`

Удалить ранее собранные данные покрытия и отчёты.

Stable ID: `helper-cli.codecoverage.command.clean`

```text
usage: codecoverage.py clean [-h] --workspace-root WORKSPACE_ROOT --build-dir BUILD_DIR --binary BINARY --backend {gcc,llvm,msvc} --output-dir OUTPUT_DIR ...
```

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-codecoverage-command-clean-argument-workspace-root-b6d8a2447e"></a><code>helper-cli.codecoverage.command.clean.argument.workspace_root</code> | <code>--workspace-root</code> | <code>option</code> | да | <code>1</code> | - | - | Каталог исходников подключающего проекта. |
| <a id="entry-helper-cli-codecoverage-command-clean-argument-build-dir-094d74e38d"></a><code>helper-cli.codecoverage.command.clean.argument.build_dir</code> | <code>--build-dir</code> | <code>option</code> | да | <code>1</code> | - | - | Настроенный каталог сборки CMake. |
| <a id="entry-helper-cli-codecoverage-command-clean-argument-binary-d3b3cf975b"></a><code>helper-cli.codecoverage.command.clean.argument.binary</code> | <code>--binary</code> | <code>option</code> | да | <code>1</code> | - | - | Инструментированный исполняемый файл тестов. |
| <a id="entry-helper-cli-codecoverage-command-clean-argument-backend-1dd776233e"></a><code>helper-cli.codecoverage.command.clean.argument.backend</code> | <code>--backend</code> | <code>option</code> | да | <code>1</code> | <code>gcc</code>, <code>llvm</code>, <code>msvc</code> | - | Backend компилятора/toolchain покрытия. |
| <a id="entry-helper-cli-codecoverage-command-clean-argument-output-dir-89690c3f10"></a><code>helper-cli.codecoverage.command.clean.argument.output_dir</code> | <code>--output-dir</code> | <code>option</code> | да | <code>1</code> | - | - | Каталог выходных данных покрытия и отчётов. |
| <a id="entry-helper-cli-codecoverage-command-clean-argument-binary-args-51d31087ed"></a><code>helper-cli.codecoverage.command.clean.argument.binary_args</code> | <code>binary_args</code> | <code>positional</code> | да | <code>...</code> | - | - | Аргументы, передаваемые тестовому бинарному файлу. |

#### Точный вывод `--help`

```text
usage: codecoverage.py clean [-h] --workspace-root WORKSPACE_ROOT --build-dir BUILD_DIR --binary BINARY --backend {gcc,llvm,msvc} --output-dir OUTPUT_DIR ...

Remove previously collected coverage data and reports

positional arguments:
  binary_args           arguments passed to the test binary

options:
  -h, --help            show this help message and exit
  --workspace-root WORKSPACE_ROOT
                        embedding-project source directory
  --build-dir BUILD_DIR
                        configured CMake build directory
  --binary BINARY       instrumented test executable
  --backend {gcc,llvm,msvc}
                        coverage compiler/toolchain backend
  --output-dir OUTPUT_DIR
                        coverage data and report output directory
```

<a id="entry-helper-cli-codecoverage-command-run-6ff9c981af"></a>
### `run`

Запустить инструментированный тестовый бинарный файл и собрать данные покрытия.

Stable ID: `helper-cli.codecoverage.command.run`

```text
usage: codecoverage.py run [-h] --workspace-root WORKSPACE_ROOT --build-dir BUILD_DIR --binary BINARY --backend {gcc,llvm,msvc} --output-dir OUTPUT_DIR ...
```

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-codecoverage-command-run-argument-workspace-root-3ea13c9ab2"></a><code>helper-cli.codecoverage.command.run.argument.workspace_root</code> | <code>--workspace-root</code> | <code>option</code> | да | <code>1</code> | - | - | Каталог исходников подключающего проекта. |
| <a id="entry-helper-cli-codecoverage-command-run-argument-build-dir-23ad87643e"></a><code>helper-cli.codecoverage.command.run.argument.build_dir</code> | <code>--build-dir</code> | <code>option</code> | да | <code>1</code> | - | - | Настроенный каталог сборки CMake. |
| <a id="entry-helper-cli-codecoverage-command-run-argument-binary-3d92cbd0fb"></a><code>helper-cli.codecoverage.command.run.argument.binary</code> | <code>--binary</code> | <code>option</code> | да | <code>1</code> | - | - | Инструментированный исполняемый файл тестов. |
| <a id="entry-helper-cli-codecoverage-command-run-argument-backend-da1a5d1400"></a><code>helper-cli.codecoverage.command.run.argument.backend</code> | <code>--backend</code> | <code>option</code> | да | <code>1</code> | <code>gcc</code>, <code>llvm</code>, <code>msvc</code> | - | Backend компилятора/toolchain покрытия. |
| <a id="entry-helper-cli-codecoverage-command-run-argument-output-dir-3fc1672e9a"></a><code>helper-cli.codecoverage.command.run.argument.output_dir</code> | <code>--output-dir</code> | <code>option</code> | да | <code>1</code> | - | - | Каталог выходных данных покрытия и отчётов. |
| <a id="entry-helper-cli-codecoverage-command-run-argument-binary-args-0e1e4b8437"></a><code>helper-cli.codecoverage.command.run.argument.binary_args</code> | <code>binary_args</code> | <code>positional</code> | да | <code>...</code> | - | - | Аргументы, передаваемые тестовому бинарному файлу. |

#### Точный вывод `--help`

```text
usage: codecoverage.py run [-h] --workspace-root WORKSPACE_ROOT --build-dir BUILD_DIR --binary BINARY --backend {gcc,llvm,msvc} --output-dir OUTPUT_DIR ...

Run the instrumented test binary and collect coverage data

positional arguments:
  binary_args           arguments passed to the test binary

options:
  -h, --help            show this help message and exit
  --workspace-root WORKSPACE_ROOT
                        embedding-project source directory
  --build-dir BUILD_DIR
                        configured CMake build directory
  --binary BINARY       instrumented test executable
  --backend {gcc,llvm,msvc}
                        coverage compiler/toolchain backend
  --output-dir OUTPUT_DIR
                        coverage data and report output directory
```

<a id="entry-helper-cli-codecoverage-command-report-8de4627b8f"></a>
### `report`

Создать текстовый и HTML-отчёты из собранных данных покрытия.

Stable ID: `helper-cli.codecoverage.command.report`

```text
usage: codecoverage.py report [-h] --workspace-root WORKSPACE_ROOT --build-dir BUILD_DIR --binary BINARY --backend {gcc,llvm,msvc} --output-dir OUTPUT_DIR ...
```

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-codecoverage-command-report-argument-workspace-root-c368887528"></a><code>helper-cli.codecoverage.command.report.argument.workspace_root</code> | <code>--workspace-root</code> | <code>option</code> | да | <code>1</code> | - | - | Каталог исходников подключающего проекта. |
| <a id="entry-helper-cli-codecoverage-command-report-argument-build-dir-ef1800b8f4"></a><code>helper-cli.codecoverage.command.report.argument.build_dir</code> | <code>--build-dir</code> | <code>option</code> | да | <code>1</code> | - | - | Настроенный каталог сборки CMake. |
| <a id="entry-helper-cli-codecoverage-command-report-argument-binary-ccafd82b7a"></a><code>helper-cli.codecoverage.command.report.argument.binary</code> | <code>--binary</code> | <code>option</code> | да | <code>1</code> | - | - | Инструментированный исполняемый файл тестов. |
| <a id="entry-helper-cli-codecoverage-command-report-argument-backend-927a805169"></a><code>helper-cli.codecoverage.command.report.argument.backend</code> | <code>--backend</code> | <code>option</code> | да | <code>1</code> | <code>gcc</code>, <code>llvm</code>, <code>msvc</code> | - | Backend компилятора/toolchain покрытия. |
| <a id="entry-helper-cli-codecoverage-command-report-argument-output-dir-ef938b1452"></a><code>helper-cli.codecoverage.command.report.argument.output_dir</code> | <code>--output-dir</code> | <code>option</code> | да | <code>1</code> | - | - | Каталог выходных данных покрытия и отчётов. |
| <a id="entry-helper-cli-codecoverage-command-report-argument-binary-args-6f9a492ac0"></a><code>helper-cli.codecoverage.command.report.argument.binary_args</code> | <code>binary_args</code> | <code>positional</code> | да | <code>...</code> | - | - | Аргументы, передаваемые тестовому бинарному файлу. |

#### Точный вывод `--help`

```text
usage: codecoverage.py report [-h] --workspace-root WORKSPACE_ROOT --build-dir BUILD_DIR --binary BINARY --backend {gcc,llvm,msvc} --output-dir OUTPUT_DIR ...

Generate text and HTML reports from collected coverage data

positional arguments:
  binary_args           arguments passed to the test binary

options:
  -h, --help            show this help message and exit
  --workspace-root WORKSPACE_ROOT
                        embedding-project source directory
  --build-dir BUILD_DIR
                        configured CMake build directory
  --binary BINARY       instrumented test executable
  --backend {gcc,llvm,msvc}
                        coverage compiler/toolchain backend
  --output-dir OUTPUT_DIR
                        coverage data and report output directory
```

<a id="entry-helper-cli-codecoverage-command-full-43f183aedc"></a>
### `full`

Очистить данные, запустить инструментированный бинарный файл и создать отчёты.

Stable ID: `helper-cli.codecoverage.command.full`

```text
usage: codecoverage.py full [-h] --workspace-root WORKSPACE_ROOT --build-dir BUILD_DIR --binary BINARY --backend {gcc,llvm,msvc} --output-dir OUTPUT_DIR ...
```

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-codecoverage-command-full-argument-workspace-root-ad42e148ad"></a><code>helper-cli.codecoverage.command.full.argument.workspace_root</code> | <code>--workspace-root</code> | <code>option</code> | да | <code>1</code> | - | - | Каталог исходников подключающего проекта. |
| <a id="entry-helper-cli-codecoverage-command-full-argument-build-dir-0d3d1b2e06"></a><code>helper-cli.codecoverage.command.full.argument.build_dir</code> | <code>--build-dir</code> | <code>option</code> | да | <code>1</code> | - | - | Настроенный каталог сборки CMake. |
| <a id="entry-helper-cli-codecoverage-command-full-argument-binary-f3c0888145"></a><code>helper-cli.codecoverage.command.full.argument.binary</code> | <code>--binary</code> | <code>option</code> | да | <code>1</code> | - | - | Инструментированный исполняемый файл тестов. |
| <a id="entry-helper-cli-codecoverage-command-full-argument-backend-fa1db784c3"></a><code>helper-cli.codecoverage.command.full.argument.backend</code> | <code>--backend</code> | <code>option</code> | да | <code>1</code> | <code>gcc</code>, <code>llvm</code>, <code>msvc</code> | - | Backend компилятора/toolchain покрытия. |
| <a id="entry-helper-cli-codecoverage-command-full-argument-output-dir-7b53f8aad9"></a><code>helper-cli.codecoverage.command.full.argument.output_dir</code> | <code>--output-dir</code> | <code>option</code> | да | <code>1</code> | - | - | Каталог выходных данных покрытия и отчётов. |
| <a id="entry-helper-cli-codecoverage-command-full-argument-binary-args-586e8c3c4a"></a><code>helper-cli.codecoverage.command.full.argument.binary_args</code> | <code>binary_args</code> | <code>positional</code> | да | <code>...</code> | - | - | Аргументы, передаваемые тестовому бинарному файлу. |

#### Точный вывод `--help`

```text
usage: codecoverage.py full [-h] --workspace-root WORKSPACE_ROOT --build-dir BUILD_DIR --binary BINARY --backend {gcc,llvm,msvc} --output-dir OUTPUT_DIR ...

Clean, run the instrumented binary, and generate reports

positional arguments:
  binary_args           arguments passed to the test binary

options:
  -h, --help            show this help message and exit
  --workspace-root WORKSPACE_ROOT
                        embedding-project source directory
  --build-dir BUILD_DIR
                        configured CMake build directory
  --binary BINARY       instrumented test executable
  --backend {gcc,llvm,msvc}
                        coverage compiler/toolchain backend
  --output-dir OUTPUT_DIR
                        coverage data and report output directory
```

<a id="entry-helper-cli-gameplay-test-runner-b34ed8deb4"></a>
## Запуск игровых тестов

Запускает упорядоченные многопроцессные игровые smoke-сценарии с контрактами готовности, маркеров, deadline, очистки и JSON-отчёта.

- Стабильный ID: `helper-cli.gameplay-test-runner`
- Программа: `gameplay_test_runner.py`
- Владелец: `quality`
- Аудитория: `game-developer`, `engine-contributor`, `embedding-project-build-system`, `ci-maintainer`
- Владелец вызова: CMake-цели подключающего проекта и CI smoke-задачи игровых тестов
- Исходный парсер: [BuildTools/gameplay_test_runner.py](https://github.com/cvet/fonline/blob/master/BuildTools/gameplay_test_runner.py)

### Аргументы верхнего уровня

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-gameplay-test-runner-argument-manifest-214c4bd23c"></a><code>helper-cli.gameplay-test-runner.argument.manifest</code> | <code>--manifest</code> | <code>option</code> | да | <code>1</code> | - | - | Путь к манифесту сценария. |
| <a id="entry-helper-cli-gameplay-test-runner-argument-value-40c5a38d08"></a><code>helper-cli.gameplay-test-runner.argument.value</code> | <code>--value</code> | <code>option</code> | нет | <code>1</code> | - | - | Значение placeholder; повторяйте при необходимости. |
| <a id="entry-helper-cli-gameplay-test-runner-argument-report-55cdc99b26"></a><code>helper-cli.gameplay-test-runner.argument.report</code> | <code>--report</code> | <code>option</code> | нет | <code>1</code> | - | - | Необязательный путь результата JSON. |

### Точный вывод `--help` верхнего уровня

```text
usage: gameplay_test_runner.py [-h] --manifest MANIFEST [--value KEY=VALUE] [--report REPORT]

Run project-neutral multi-process gameplay smoke scenarios from a checked JSON manifest.

options:
  -h, --help           show this help message and exit
  --manifest MANIFEST  scenario manifest path
  --value KEY=VALUE    placeholder value; repeat as needed
  --report REPORT      optional JSON result path
```

<a id="entry-helper-cli-ai-control-client-35184e9731"></a>
## Клиент протокола AiControl

Вызывает совместимый с Engine проектный мост AiControl через версионированный NDJSON/TCP envelope, не раскрывая shared tokens в командной строке.

- Стабильный ID: `helper-cli.ai-control-client`
- Программа: `ai_control_client.py`
- Владелец: `tooling`
- Аудитория: `game-developer`, `engine-contributor`, `tool-developer`, `ci-maintainer`
- Владелец вызова: ИИ-адаптеры проекта, smoke-тесты протокола и непосредственная диагностика разработчиком
- Исходный парсер: [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py)

### Аргументы верхнего уровня

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-ai-control-client-argument-host-0554537b33"></a><code>helper-cli.ai-control-client.argument.host</code> | <code>--host</code> | <code>option</code> | нет | <code>1</code> | - | <code>127.0.0.1</code> | Host моста. |
| <a id="entry-helper-cli-ai-control-client-argument-port-fb6c2c628e"></a><code>helper-cli.ai-control-client.argument.port</code> | <code>--port</code> | <code>option</code> | нет | <code>1</code> | - | <code>43011</code> | Порт моста. |
| <a id="entry-helper-cli-ai-control-client-argument-timeout-3a4b9b5c1e"></a><code>helper-cli.ai-control-client.argument.timeout</code> | <code>--timeout</code> | <code>option</code> | нет | <code>1</code> | - | <code>5.0</code> | Timeout сокета в секундах. |
| <a id="entry-helper-cli-ai-control-client-argument-token-env-4a7ef4dfd5"></a><code>helper-cli.ai-control-client.argument.token_env</code> | <code>--token-env</code> | <code>option</code> | нет | <code>1</code> | - | <code>FONLINE_AI_TOKEN</code> | Переменная окружения с общим токеном. |
| <a id="entry-helper-cli-ai-control-client-argument-allow-remote-0d33b73a66"></a><code>helper-cli.ai-control-client.argument.allow_remote</code> | <code>--allow-remote</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | Разрешить endpoint вне loopback; транспорт остаётся незашифрованным. |

### Точный вывод `--help` верхнего уровня

```text
usage: ai_control_client.py [-h] [--host HOST] [--port PORT] [--timeout TIMEOUT] [--token-env TOKEN_ENV] [--allow-remote] {ping,status,observe,events,act} ...

Call an Engine-compatible AiControl bridge over NDJSON/TCP.

positional arguments:
  {ping,status,observe,events,act}
    ping                Check bridge liveness.
    status              Read bridge status and queue limits.
    observe             Read the latest project observation.
    events              Read events after a sequence cursor.
    act                 Enqueue one project-defined command.

options:
  -h, --help            show this help message and exit
  --host HOST           Bridge host.
  --port PORT           Bridge port.
  --timeout TIMEOUT     Socket timeout in seconds.
  --token-env TOKEN_ENV
                        Environment variable containing the shared token.
  --allow-remote        Permit a non-loopback endpoint; transport remains unencrypted.
```

<a id="entry-helper-cli-ai-control-client-command-ping-2a253f06de"></a>
### `ping`

Проверить доступность моста.

Stable ID: `helper-cli.ai-control-client.command.ping`

```text
usage: ai_control_client.py ping [-h]
```

На этом уровне аргументов нет.

#### Точный вывод `--help`

```text
usage: ai_control_client.py ping [-h]

options:
  -h, --help  show this help message and exit
```

<a id="entry-helper-cli-ai-control-client-command-status-71ec51cb72"></a>
### `status`

Прочитать состояние моста и ограничения очереди.

Stable ID: `helper-cli.ai-control-client.command.status`

```text
usage: ai_control_client.py status [-h]
```

На этом уровне аргументов нет.

#### Точный вывод `--help`

```text
usage: ai_control_client.py status [-h]

options:
  -h, --help  show this help message and exit
```

<a id="entry-helper-cli-ai-control-client-command-observe-38177f229e"></a>
### `observe`

Прочитать последнее наблюдение проекта.

Stable ID: `helper-cli.ai-control-client.command.observe`

```text
usage: ai_control_client.py observe [-h]
```

На этом уровне аргументов нет.

#### Точный вывод `--help`

```text
usage: ai_control_client.py observe [-h]

options:
  -h, --help  show this help message and exit
```

<a id="entry-helper-cli-ai-control-client-command-events-e9dd1890b0"></a>
### `events`

Прочитать события после sequence-cursor.

Stable ID: `helper-cli.ai-control-client.command.events`

```text
usage: ai_control_client.py events [-h] [--after-seq AFTER_SEQ] [--limit LIMIT]
```

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-ai-control-client-command-events-argument-after-seq-f5b3920536"></a><code>helper-cli.ai-control-client.command.events.argument.after_seq</code> | <code>--after-seq</code> | <code>option</code> | нет | <code>1</code> | - | <code>0</code> | Исключающий cursor событий. |
| <a id="entry-helper-cli-ai-control-client-command-events-argument-limit-a61d74a970"></a><code>helper-cli.ai-control-client.command.events.argument.limit</code> | <code>--limit</code> | <code>option</code> | нет | <code>1</code> | - | <code>100</code> | Максимальное количество возвращаемых событий. |

#### Точный вывод `--help`

```text
usage: ai_control_client.py events [-h] [--after-seq AFTER_SEQ] [--limit LIMIT]

options:
  -h, --help            show this help message and exit
  --after-seq AFTER_SEQ
                        Exclusive event cursor.
  --limit LIMIT         Maximum events to return.
```

<a id="entry-helper-cli-ai-control-client-command-act-a8414f2a7f"></a>
### `act`

Поставить в очередь одну определённую проектом команду.

Stable ID: `helper-cli.ai-control-client.command.act`

```text
usage: ai_control_client.py act [-h] --type TYPE [--target-id TARGET_ID] [--item-id ITEM_ID] [--aux-id AUX_ID] [--x X] [--y Y] [--screen-x SCREEN_X] [--screen-y SCREEN_Y] [--int-arg INT_ARG] [--string-arg STRING_ARG] [--append]
```

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-ai-control-client-command-act-argument-type-30243d4b4c"></a><code>helper-cli.ai-control-client.command.act.argument.type</code> | <code>--type</code> | <code>option</code> | да | <code>1</code> | - | - | Определённый проектом тип команды. |
| <a id="entry-helper-cli-ai-control-client-command-act-argument-target-id-cc6d8b017b"></a><code>helper-cli.ai-control-client.command.act.argument.target_id</code> | <code>--target-id</code> | <code>option</code> | нет | <code>1</code> | - | - | Необязательный идентификатор целевой сущности. |
| <a id="entry-helper-cli-ai-control-client-command-act-argument-item-id-82973c779f"></a><code>helper-cli.ai-control-client.command.act.argument.item_id</code> | <code>--item-id</code> | <code>option</code> | нет | <code>1</code> | - | - | Необязательный идентификатор сущности предмета. |
| <a id="entry-helper-cli-ai-control-client-command-act-argument-aux-id-12c9597c1d"></a><code>helper-cli.ai-control-client.command.act.argument.aux_id</code> | <code>--aux-id</code> | <code>option</code> | нет | <code>1</code> | - | - | Необязательный идентификатор вспомогательной сущности. |
| <a id="entry-helper-cli-ai-control-client-command-act-argument-x-4507b45828"></a><code>helper-cli.ai-control-client.command.act.argument.x</code> | <code>--x</code> | <code>option</code> | нет | <code>1</code> | - | - | Необязательная мировая координата X проекта. |
| <a id="entry-helper-cli-ai-control-client-command-act-argument-y-4125d48b44"></a><code>helper-cli.ai-control-client.command.act.argument.y</code> | <code>--y</code> | <code>option</code> | нет | <code>1</code> | - | - | Необязательная мировая координата Y проекта. |
| <a id="entry-helper-cli-ai-control-client-command-act-argument-screen-x-180c94f1ff"></a><code>helper-cli.ai-control-client.command.act.argument.screen_x</code> | <code>--screen-x</code> | <code>option</code> | нет | <code>1</code> | - | - | Необязательная экранная координата X. |
| <a id="entry-helper-cli-ai-control-client-command-act-argument-screen-y-e60dd59502"></a><code>helper-cli.ai-control-client.command.act.argument.screen_y</code> | <code>--screen-y</code> | <code>option</code> | нет | <code>1</code> | - | - | Необязательная экранная координата Y. |
| <a id="entry-helper-cli-ai-control-client-command-act-argument-int-arg-d52ef6d5f8"></a><code>helper-cli.ai-control-client.command.act.argument.int_arg</code> | <code>--int-arg</code> | <code>option</code> | нет | <code>1</code> | - | - | Необязательный целочисленный payload. |
| <a id="entry-helper-cli-ai-control-client-command-act-argument-string-arg-a167eef2cd"></a><code>helper-cli.ai-control-client.command.act.argument.string_arg</code> | <code>--string-arg</code> | <code>option</code> | нет | <code>1</code> | - | - | Необязательный строковый payload. |
| <a id="entry-helper-cli-ai-control-client-command-act-argument-append-460a176550"></a><code>helper-cli.ai-control-client.command.act.argument.append</code> | <code>--append</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | Запросить проектную семантику добавления в очередь. |

#### Точный вывод `--help`

```text
usage: ai_control_client.py act [-h] --type TYPE [--target-id TARGET_ID] [--item-id ITEM_ID] [--aux-id AUX_ID] [--x X] [--y Y] [--screen-x SCREEN_X] [--screen-y SCREEN_Y] [--int-arg INT_ARG] [--string-arg STRING_ARG] [--append]

options:
  -h, --help            show this help message and exit
  --type TYPE           Project-defined command type.
  --target-id TARGET_ID
                        Optional target entity identifier.
  --item-id ITEM_ID     Optional item entity identifier.
  --aux-id AUX_ID       Optional auxiliary entity identifier.
  --x X                 Optional project world X coordinate.
  --y Y                 Optional project world Y coordinate.
  --screen-x SCREEN_X   Optional screen X coordinate.
  --screen-y SCREEN_Y   Optional screen Y coordinate.
  --int-arg INT_ARG     Optional integer payload.
  --string-arg STRING_ARG
                        Optional string payload.
  --append              Request project queue append semantics.
```

<a id="entry-helper-cli-windows7-import-check-a0c7e4cb59"></a>
## Проверка импортов Windows 7

Проверяет слинкованные PE-файлы и отклоняет импорты, недоступные в Windows 7.

- Стабильный ID: `helper-cli.windows7-import-check`
- Программа: `check_windows7_imports.py`
- Владелец: `quality`
- Аудитория: `engine-contributor`, `embedding-project-build-system`, `release-operator`
- Владелец вызова: CI и релизная проверка Windows 7 подключающего проекта
- Исходный парсер: [BuildTools/check_windows7_imports.py](https://github.com/cvet/fonline/blob/master/BuildTools/check_windows7_imports.py)

### Аргументы верхнего уровня

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-windows7-import-check-argument-binaries-90e898370f"></a><code>helper-cli.windows7-import-check.argument.binaries</code> | <code>binaries</code> | <code>positional</code> | да | <code>+</code> | - | - | Проверяемый слинкованный исполняемый PE-файл или DLL. |

### Точный вывод `--help` верхнего уровня

```text
usage: check_windows7_imports.py [-h] binaries [binaries ...]

Reject CreateFile2 from Windows 7-compatible PE binaries

positional arguments:
  binaries    linked PE executable or DLL to inspect

options:
  -h, --help  show this help message and exit
```

<a id="entry-helper-cli-android-device-ab99179ae9"></a>
## Управление устройством Android

Находит, подключает, устанавливает, запускает, останавливает и проверяет Android Wi-Fi-устройства через adb.

- Стабильный ID: `helper-cli.android-device`
- Программа: `android_device.py`
- Владелец: `platform`
- Аудитория: `game-developer`, `engine-contributor`
- Владелец вызова: Android-задачи подключающего проекта и непосредственное использование разработчиком
- Исходный парсер: [BuildTools/android_device.py](https://github.com/cvet/fonline/blob/master/BuildTools/android_device.py)

### Аргументы верхнего уровня

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-android-device-argument-workspace-root-8726d0799e"></a><code>helper-cli.android-device.argument.workspace_root</code> | <code>--workspace-root</code> | <code>option</code> | нет | <code>1</code> | - | - | Путь к рабочему каталогу, содержащему android-sdk и android-debug. |

### Точный вывод `--help` верхнего уровня

```text
usage: android_device.py [-h] [--workspace-root WORKSPACE_ROOT] {discover,connect,install,launch,launch-game,stop,logcat} ...

Android Wi-Fi device helper for BuildTools tasks

positional arguments:
  {discover,connect,install,launch,launch-game,stop,logcat}
    discover            List Android Wi-Fi devices discovered through adb mdns
    connect             Connect to an Android Wi-Fi device and cache the endpoint
    install             Install an APK on the selected Android Wi-Fi device
    launch              Launch an Android activity on the selected device
    launch-game         Launch the Android game activity and pass RemoteSceneLaunch server host override
    stop                Force-stop an Android package on the selected device
    logcat              Stream logcat from the selected device

options:
  -h, --help            show this help message and exit
  --workspace-root WORKSPACE_ROOT
                        Workspace directory path containing android-sdk and android-debug
```

<a id="entry-helper-cli-android-device-command-discover-f8951fbd7c"></a>
### `discover`

Перечислить Android-устройства Wi-Fi, найденные через adb mdns.

Stable ID: `helper-cli.android-device.command.discover`

```text
usage: android_device.py discover [-h]
```

На этом уровне аргументов нет.

#### Точный вывод `--help`

```text
usage: android_device.py discover [-h]

options:
  -h, --help  show this help message and exit
```

<a id="entry-helper-cli-android-device-command-connect-6083ee40ae"></a>
### `connect`

Подключиться к Android-устройству по Wi-Fi и кешировать endpoint.

Stable ID: `helper-cli.android-device.command.connect`

```text
usage: android_device.py connect [-h] [--device DEVICE]
```

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-android-device-command-connect-argument-device-933a2c37f1"></a><code>helper-cli.android-device.command.connect.argument.device</code> | <code>--device</code> | <code>option</code> | нет | <code>1</code> | - | - | IP-адрес устройства с необязательным портом; при отсутствии используются автоматический поиск и интерактивный выбор. |

#### Точный вывод `--help`

```text
usage: android_device.py connect [-h] [--device DEVICE]

options:
  -h, --help       show this help message and exit
  --device DEVICE  Device IP[:port]; if omitted, auto-discovery and interactive selection are used
```

<a id="entry-helper-cli-android-device-command-install-20d466716c"></a>
### `install`

Установить APK на выбранное Android-устройство Wi-Fi.

Stable ID: `helper-cli.android-device.command.install`

```text
usage: android_device.py install [-h] --apk APK [--device DEVICE]
```

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-android-device-command-install-argument-apk-052b021543"></a><code>helper-cli.android-device.command.install.argument.apk</code> | <code>--apk</code> | <code>option</code> | да | <code>1</code> | - | - | Путь к APK. |
| <a id="entry-helper-cli-android-device-command-install-argument-device-117a72e901"></a><code>helper-cli.android-device.command.install.argument.device</code> | <code>--device</code> | <code>option</code> | нет | <code>1</code> | - | - | IP-адрес устройства с необязательным портом; при отсутствии используется кешированный endpoint или поиск. |

#### Точный вывод `--help`

```text
usage: android_device.py install [-h] --apk APK [--device DEVICE]

options:
  -h, --help       show this help message and exit
  --apk APK        APK path
  --device DEVICE  Device IP[:port]; if omitted, cached endpoint or discovery is used
```

<a id="entry-helper-cli-android-device-command-launch-7c998b622c"></a>
### `launch`

Запустить Android activity на выбранном устройстве.

Stable ID: `helper-cli.android-device.command.launch`

```text
usage: android_device.py launch [-h] --activity ACTIVITY [--device DEVICE]
```

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-android-device-command-launch-argument-activity-8e9055617d"></a><code>helper-cli.android-device.command.launch.argument.activity</code> | <code>--activity</code> | <code>option</code> | да | <code>1</code> | - | - | Полностью квалифицированный компонент activity, например com.example.game/.FOnlineActivity. |
| <a id="entry-helper-cli-android-device-command-launch-argument-device-fb19f4776c"></a><code>helper-cli.android-device.command.launch.argument.device</code> | <code>--device</code> | <code>option</code> | нет | <code>1</code> | - | - | IP-адрес устройства с необязательным портом; при отсутствии используется кешированный endpoint или поиск. |

#### Точный вывод `--help`

```text
usage: android_device.py launch [-h] --activity ACTIVITY [--device DEVICE]

options:
  -h, --help           show this help message and exit
  --activity ACTIVITY  Fully qualified activity component, e.g. com.example.game/.FOnlineActivity
  --device DEVICE      Device IP[:port]; if omitted, cached endpoint or discovery is used
```

<a id="entry-helper-cli-android-device-command-launch-game-408e50236e"></a>
### `launch-game`

Запустить Android activity игры и передать переопределение хоста сервера RemoteSceneLaunch.

Stable ID: `helper-cli.android-device.command.launch-game`

```text
usage: android_device.py launch-game [-h] --activity ACTIVITY [--device DEVICE] [--server-host SERVER_HOST]
```

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-android-device-command-launch-game-argument-activity-428147ad37"></a><code>helper-cli.android-device.command.launch-game.argument.activity</code> | <code>--activity</code> | <code>option</code> | да | <code>1</code> | - | - | Полностью квалифицированный компонент activity, например com.example.game/.FOnlineActivity. |
| <a id="entry-helper-cli-android-device-command-launch-game-argument-device-d749dc44b6"></a><code>helper-cli.android-device.command.launch-game.argument.device</code> | <code>--device</code> | <code>option</code> | нет | <code>1</code> | - | - | IP-адрес устройства с необязательным портом; при отсутствии используется кешированный endpoint или поиск. |
| <a id="entry-helper-cli-android-device-command-launch-game-argument-server-host-3f5230a55e"></a><code>helper-cli.android-device.command.launch-game.argument.server_host</code> | <code>--server-host</code> | <code>option</code> | нет | <code>1</code> | - | - | IP-адрес или имя host для ClientNetwork.ServerHost; при отсутствии определяется по маршруту к выбранному устройству. |

#### Точный вывод `--help`

```text
usage: android_device.py launch-game [-h] --activity ACTIVITY [--device DEVICE] [--server-host SERVER_HOST]

options:
  -h, --help            show this help message and exit
  --activity ACTIVITY   Fully qualified activity component, e.g. com.example.game/.FOnlineActivity
  --device DEVICE       Device IP[:port]; if omitted, cached endpoint or discovery is used
  --server-host SERVER_HOST
                        Host IP or name for ClientNetwork.ServerHost; if omitted, auto-detected from the route to the selected device
```

<a id="entry-helper-cli-android-device-command-stop-74b36b2258"></a>
### `stop`

Принудительно остановить Android-пакет на выбранном устройстве.

Stable ID: `helper-cli.android-device.command.stop`

```text
usage: android_device.py stop [-h] --package PACKAGE_NAME [--device DEVICE]
```

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-android-device-command-stop-argument-package-name-8a29334041"></a><code>helper-cli.android-device.command.stop.argument.package_name</code> | <code>--package</code> | <code>option</code> | да | <code>1</code> | - | - | Имя Android-пакета. |
| <a id="entry-helper-cli-android-device-command-stop-argument-device-8c3cd8d602"></a><code>helper-cli.android-device.command.stop.argument.device</code> | <code>--device</code> | <code>option</code> | нет | <code>1</code> | - | - | IP-адрес устройства с необязательным портом; при отсутствии используется кешированный endpoint или поиск. |

#### Точный вывод `--help`

```text
usage: android_device.py stop [-h] --package PACKAGE_NAME [--device DEVICE]

options:
  -h, --help            show this help message and exit
  --package PACKAGE_NAME
                        Android package name
  --device DEVICE       Device IP[:port]; if omitted, cached endpoint or discovery is used
```

<a id="entry-helper-cli-android-device-command-logcat-5f28df274e"></a>
### `logcat`

Передавать поток logcat с выбранного устройства.

Stable ID: `helper-cli.android-device.command.logcat`

```text
usage: android_device.py logcat [-h] [--device DEVICE]
```

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-android-device-command-logcat-argument-device-71db85d287"></a><code>helper-cli.android-device.command.logcat.argument.device</code> | <code>--device</code> | <code>option</code> | нет | <code>1</code> | - | - | IP-адрес устройства с необязательным портом; при отсутствии используется кешированный endpoint или поиск. |

#### Точный вывод `--help`

```text
usage: android_device.py logcat [-h] [--device DEVICE]

options:
  -h, --help       show this help message and exit
  --device DEVICE  Device IP[:port]; if omitted, cached endpoint or discovery is used
```

<a id="entry-helper-cli-simple-web-server-58fbf70798"></a>
## Локальный Web-сервер

Раздаёт упакованный Web-клиент с локального HTTP-сервера без кеширования.

- Стабильный ID: `helper-cli.simple-web-server`
- Программа: `simple-web-server.py`
- Владелец: `platform`
- Аудитория: `game-developer`, `release-operator`
- Владелец вызова: payload WebServer в BuildTools/package.py
- Исходный парсер: [BuildTools/web/simple-web-server.py](https://github.com/cvet/fonline/blob/master/BuildTools/web/simple-web-server.py)

### Аргументы верхнего уровня

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-simple-web-server-argument-port-20ae9fb491"></a><code>helper-cli.simple-web-server.argument.port</code> | <code>--port</code> | <code>option</code> | нет | <code>1</code> | - | <code>7000</code> | Порт Web-сервера. |
| <a id="entry-helper-cli-simple-web-server-argument-fork-b1141869ad"></a><code>helper-cli.simple-web-server.argument.fork</code> | <code>--fork</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | Создать дочерний процесс. |

### Точный вывод `--help` верхнего уровня

```text
usage: simple-web-server.py [-h] [--port PORT] [--fork]

Simple HTTP server

options:
  -h, --help   show this help message and exit
  --port PORT  web server port
  --fork       fork process
```

<a id="entry-helper-cli-createmsi-18899fd2a5"></a>
## Создание MSI

Собирает установщик MSI из созданного упаковщиком определения WiX.

- Стабильный ID: `helper-cli.createmsi`
- Программа: `createmsi.py`
- Владелец: `build-release`
- Аудитория: `release-operator`, `engine-contributor`
- Владелец вызова: пакет Wix в BuildTools/package.py
- Исходный парсер: [BuildTools/msicreator/createmsi.py](https://github.com/cvet/fonline/blob/master/BuildTools/msicreator/createmsi.py)

### Аргументы верхнего уровня

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-createmsi-argument-jsonfile-385f92bb78"></a><code>helper-cli.createmsi.argument.jsonfile</code> | <code>definition.json</code> | <code>positional</code> | да | <code>1</code> | - | - | Только имя файла определения пакета WiX в рабочем каталоге. |

### Точный вывод `--help` верхнего уровня

```text
usage: createmsi.py [-h] definition.json

Build an MSI package from a WiX definition

positional arguments:
  definition.json  bare WiX package definition filename in the working directory

options:
  -h, --help       show this help message and exit
```
