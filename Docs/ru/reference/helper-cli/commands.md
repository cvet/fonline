---
title: Команды вспомогательных CLI
document_id: generated-helper-cli-commands
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-helper-cli-commands","locale":"ru","source_path":"Docs/en/reference/helper-cli/commands.md","source_sha256":"4ddf038aa2cef65eeb7407ddb9423ee4729247171941809056e11848eb66b047"} -->

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
| <a id="entry-helper-cli-codegen-argument-enginedefine-8961bb73cd"></a><code>helper-cli.codegen.argument.enginedefine</code> | <code>-enginedefine</code> | <code>option</code> | нет | <code>1</code> | - | - | Define конфигурации движка NAME=VALUE, создаваемый как макрос в EngineConfig.gen.h. |
| <a id="entry-helper-cli-codegen-argument-meta-467f731eb8"></a><code>helper-cli.codegen.argument.meta</code> | <code>-meta</code> | <code>option</code> | да | <code>1</code> | - | - | Путь к метаданным Script API, то есть тегам ///@. |
| <a id="entry-helper-cli-codegen-argument-commonheader-888570e588"></a><code>helper-cli.codegen.argument.commonheader</code> | <code>-commonheader</code> | <code>option</code> | нет | <code>1</code> | - | - | Путь к общему header-файлу. |
| <a id="entry-helper-cli-codegen-argument-genoutput-b6e5ab6468"></a><code>helper-cli.codegen.argument.genoutput</code> | <code>-genoutput</code> | <code>option</code> | да | <code>1</code> | - | - | Каталог выходного сгенерированного кода. |
| <a id="entry-helper-cli-codegen-argument-verbose-1ae6caa3b7"></a><code>helper-cli.codegen.argument.verbose</code> | <code>-verbose</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | Подробный режим. |

### Точный вывод `--help` верхнего уровня

```text
usage: codegen.py [-h] -maincfg MAINCFG -buildhash BUILDHASH -devname DEVNAME -nicename NICENAME -embedded EMBEDDED [-enginedefine ENGINEDEFINE] -meta META [-commonheader COMMONHEADER] -genoutput GENOUTPUT [-verbose]

FOnline code generator

options:
  -h, --help            show this help message and exit
  -maincfg MAINCFG      main config file
  -buildhash BUILDHASH  build hash
  -devname DEVNAME      dev game name
  -nicename NICENAME    nice game name
  -embedded EMBEDDED    embedded buffer capacity
  -enginedefine ENGINEDEFINE
                        engine configuration define NAME=VALUE emitted as a macro into EngineConfig.gen.h
  -meta META            path to script api metadata (///@ tags)
  -commonheader COMMONHEADER
                        path to common header file
  -genoutput GENOUTPUT  generated code output dir
  -verbose              verbose mode
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
usage: codecoverage.py clean [-h] --workspace-root WORKSPACE_ROOT --build-dir BUILD_DIR --binary BINARY --backend {gcc,llvm,msvc} --output-dir OUTPUT_DIR [--object OBJECTS] ...
```

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-codecoverage-command-clean-argument-workspace-root-b6d8a2447e"></a><code>helper-cli.codecoverage.command.clean.argument.workspace_root</code> | <code>--workspace-root</code> | <code>option</code> | да | <code>1</code> | - | - | Каталог исходников подключающего проекта. |
| <a id="entry-helper-cli-codecoverage-command-clean-argument-build-dir-094d74e38d"></a><code>helper-cli.codecoverage.command.clean.argument.build_dir</code> | <code>--build-dir</code> | <code>option</code> | да | <code>1</code> | - | - | Настроенный каталог сборки CMake. |
| <a id="entry-helper-cli-codecoverage-command-clean-argument-binary-d3b3cf975b"></a><code>helper-cli.codecoverage.command.clean.argument.binary</code> | <code>--binary</code> | <code>option</code> | да | <code>1</code> | - | - | Инструментированный исполняемый файл тестов. |
| <a id="entry-helper-cli-codecoverage-command-clean-argument-backend-1dd776233e"></a><code>helper-cli.codecoverage.command.clean.argument.backend</code> | <code>--backend</code> | <code>option</code> | да | <code>1</code> | <code>gcc</code>, <code>llvm</code>, <code>msvc</code> | - | Backend компилятора/toolchain покрытия. |
| <a id="entry-helper-cli-codecoverage-command-clean-argument-output-dir-89690c3f10"></a><code>helper-cli.codecoverage.command.clean.argument.output_dir</code> | <code>--output-dir</code> | <code>option</code> | да | <code>1</code> | - | - | Каталог выходных данных покрытия и отчётов. |
| <a id="entry-helper-cli-codecoverage-command-clean-argument-objects-394f819f2f"></a><code>helper-cli.codecoverage.command.clean.argument.objects</code> | <code>--object</code> | <code>option</code> | нет | <code>1</code> | - | - | Дополнительный инструментированный исполняемый файл или динамическая библиотека для отчёта LLVM; аргумент можно повторять. |
| <a id="entry-helper-cli-codecoverage-command-clean-argument-binary-args-51d31087ed"></a><code>helper-cli.codecoverage.command.clean.argument.binary_args</code> | <code>binary_args</code> | <code>positional</code> | да | <code>...</code> | - | - | Аргументы, передаваемые тестовому бинарному файлу. |

#### Точный вывод `--help`

```text
usage: codecoverage.py clean [-h] --workspace-root WORKSPACE_ROOT --build-dir BUILD_DIR --binary BINARY --backend {gcc,llvm,msvc} --output-dir OUTPUT_DIR [--object OBJECTS] ...

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
  --object OBJECTS      additional instrumented executable or shared library for LLVM reporting; repeatable
```

<a id="entry-helper-cli-codecoverage-command-run-6ff9c981af"></a>
### `run`

Запустить инструментированный тестовый бинарный файл и собрать данные покрытия.

Stable ID: `helper-cli.codecoverage.command.run`

```text
usage: codecoverage.py run [-h] --workspace-root WORKSPACE_ROOT --build-dir BUILD_DIR --binary BINARY --backend {gcc,llvm,msvc} --output-dir OUTPUT_DIR [--object OBJECTS] ...
```

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-codecoverage-command-run-argument-workspace-root-3ea13c9ab2"></a><code>helper-cli.codecoverage.command.run.argument.workspace_root</code> | <code>--workspace-root</code> | <code>option</code> | да | <code>1</code> | - | - | Каталог исходников подключающего проекта. |
| <a id="entry-helper-cli-codecoverage-command-run-argument-build-dir-23ad87643e"></a><code>helper-cli.codecoverage.command.run.argument.build_dir</code> | <code>--build-dir</code> | <code>option</code> | да | <code>1</code> | - | - | Настроенный каталог сборки CMake. |
| <a id="entry-helper-cli-codecoverage-command-run-argument-binary-3d92cbd0fb"></a><code>helper-cli.codecoverage.command.run.argument.binary</code> | <code>--binary</code> | <code>option</code> | да | <code>1</code> | - | - | Инструментированный исполняемый файл тестов. |
| <a id="entry-helper-cli-codecoverage-command-run-argument-backend-da1a5d1400"></a><code>helper-cli.codecoverage.command.run.argument.backend</code> | <code>--backend</code> | <code>option</code> | да | <code>1</code> | <code>gcc</code>, <code>llvm</code>, <code>msvc</code> | - | Backend компилятора/toolchain покрытия. |
| <a id="entry-helper-cli-codecoverage-command-run-argument-output-dir-3fc1672e9a"></a><code>helper-cli.codecoverage.command.run.argument.output_dir</code> | <code>--output-dir</code> | <code>option</code> | да | <code>1</code> | - | - | Каталог выходных данных покрытия и отчётов. |
| <a id="entry-helper-cli-codecoverage-command-run-argument-objects-9659c0efc0"></a><code>helper-cli.codecoverage.command.run.argument.objects</code> | <code>--object</code> | <code>option</code> | нет | <code>1</code> | - | - | Дополнительный инструментированный исполняемый файл или динамическая библиотека для отчёта LLVM; аргумент можно повторять. |
| <a id="entry-helper-cli-codecoverage-command-run-argument-binary-args-0e1e4b8437"></a><code>helper-cli.codecoverage.command.run.argument.binary_args</code> | <code>binary_args</code> | <code>positional</code> | да | <code>...</code> | - | - | Аргументы, передаваемые тестовому бинарному файлу. |

#### Точный вывод `--help`

```text
usage: codecoverage.py run [-h] --workspace-root WORKSPACE_ROOT --build-dir BUILD_DIR --binary BINARY --backend {gcc,llvm,msvc} --output-dir OUTPUT_DIR [--object OBJECTS] ...

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
  --object OBJECTS      additional instrumented executable or shared library for LLVM reporting; repeatable
```

<a id="entry-helper-cli-codecoverage-command-report-8de4627b8f"></a>
### `report`

Создать текстовый и HTML-отчёты из собранных данных покрытия.

Stable ID: `helper-cli.codecoverage.command.report`

```text
usage: codecoverage.py report [-h] --workspace-root WORKSPACE_ROOT --build-dir BUILD_DIR --binary BINARY --backend {gcc,llvm,msvc} --output-dir OUTPUT_DIR [--object OBJECTS] ...
```

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-codecoverage-command-report-argument-workspace-root-c368887528"></a><code>helper-cli.codecoverage.command.report.argument.workspace_root</code> | <code>--workspace-root</code> | <code>option</code> | да | <code>1</code> | - | - | Каталог исходников подключающего проекта. |
| <a id="entry-helper-cli-codecoverage-command-report-argument-build-dir-ef1800b8f4"></a><code>helper-cli.codecoverage.command.report.argument.build_dir</code> | <code>--build-dir</code> | <code>option</code> | да | <code>1</code> | - | - | Настроенный каталог сборки CMake. |
| <a id="entry-helper-cli-codecoverage-command-report-argument-binary-ccafd82b7a"></a><code>helper-cli.codecoverage.command.report.argument.binary</code> | <code>--binary</code> | <code>option</code> | да | <code>1</code> | - | - | Инструментированный исполняемый файл тестов. |
| <a id="entry-helper-cli-codecoverage-command-report-argument-backend-927a805169"></a><code>helper-cli.codecoverage.command.report.argument.backend</code> | <code>--backend</code> | <code>option</code> | да | <code>1</code> | <code>gcc</code>, <code>llvm</code>, <code>msvc</code> | - | Backend компилятора/toolchain покрытия. |
| <a id="entry-helper-cli-codecoverage-command-report-argument-output-dir-ef938b1452"></a><code>helper-cli.codecoverage.command.report.argument.output_dir</code> | <code>--output-dir</code> | <code>option</code> | да | <code>1</code> | - | - | Каталог выходных данных покрытия и отчётов. |
| <a id="entry-helper-cli-codecoverage-command-report-argument-objects-96a47bda0f"></a><code>helper-cli.codecoverage.command.report.argument.objects</code> | <code>--object</code> | <code>option</code> | нет | <code>1</code> | - | - | Дополнительный инструментированный исполняемый файл или динамическая библиотека для отчёта LLVM; аргумент можно повторять. |
| <a id="entry-helper-cli-codecoverage-command-report-argument-binary-args-6f9a492ac0"></a><code>helper-cli.codecoverage.command.report.argument.binary_args</code> | <code>binary_args</code> | <code>positional</code> | да | <code>...</code> | - | - | Аргументы, передаваемые тестовому бинарному файлу. |

#### Точный вывод `--help`

```text
usage: codecoverage.py report [-h] --workspace-root WORKSPACE_ROOT --build-dir BUILD_DIR --binary BINARY --backend {gcc,llvm,msvc} --output-dir OUTPUT_DIR [--object OBJECTS] ...

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
  --object OBJECTS      additional instrumented executable or shared library for LLVM reporting; repeatable
```

<a id="entry-helper-cli-codecoverage-command-full-43f183aedc"></a>
### `full`

Очистить данные, запустить инструментированный бинарный файл и создать отчёты.

Stable ID: `helper-cli.codecoverage.command.full`

```text
usage: codecoverage.py full [-h] --workspace-root WORKSPACE_ROOT --build-dir BUILD_DIR --binary BINARY --backend {gcc,llvm,msvc} --output-dir OUTPUT_DIR [--object OBJECTS] ...
```

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-codecoverage-command-full-argument-workspace-root-ad42e148ad"></a><code>helper-cli.codecoverage.command.full.argument.workspace_root</code> | <code>--workspace-root</code> | <code>option</code> | да | <code>1</code> | - | - | Каталог исходников подключающего проекта. |
| <a id="entry-helper-cli-codecoverage-command-full-argument-build-dir-0d3d1b2e06"></a><code>helper-cli.codecoverage.command.full.argument.build_dir</code> | <code>--build-dir</code> | <code>option</code> | да | <code>1</code> | - | - | Настроенный каталог сборки CMake. |
| <a id="entry-helper-cli-codecoverage-command-full-argument-binary-f3c0888145"></a><code>helper-cli.codecoverage.command.full.argument.binary</code> | <code>--binary</code> | <code>option</code> | да | <code>1</code> | - | - | Инструментированный исполняемый файл тестов. |
| <a id="entry-helper-cli-codecoverage-command-full-argument-backend-fa1db784c3"></a><code>helper-cli.codecoverage.command.full.argument.backend</code> | <code>--backend</code> | <code>option</code> | да | <code>1</code> | <code>gcc</code>, <code>llvm</code>, <code>msvc</code> | - | Backend компилятора/toolchain покрытия. |
| <a id="entry-helper-cli-codecoverage-command-full-argument-output-dir-7b53f8aad9"></a><code>helper-cli.codecoverage.command.full.argument.output_dir</code> | <code>--output-dir</code> | <code>option</code> | да | <code>1</code> | - | - | Каталог выходных данных покрытия и отчётов. |
| <a id="entry-helper-cli-codecoverage-command-full-argument-objects-8a1e4c2132"></a><code>helper-cli.codecoverage.command.full.argument.objects</code> | <code>--object</code> | <code>option</code> | нет | <code>1</code> | - | - | Дополнительный инструментированный исполняемый файл или динамическая библиотека для отчёта LLVM; аргумент можно повторять. |
| <a id="entry-helper-cli-codecoverage-command-full-argument-binary-args-586e8c3c4a"></a><code>helper-cli.codecoverage.command.full.argument.binary_args</code> | <code>binary_args</code> | <code>positional</code> | да | <code>...</code> | - | - | Аргументы, передаваемые тестовому бинарному файлу. |

#### Точный вывод `--help`

```text
usage: codecoverage.py full [-h] --workspace-root WORKSPACE_ROOT --build-dir BUILD_DIR --binary BINARY --backend {gcc,llvm,msvc} --output-dir OUTPUT_DIR [--object OBJECTS] ...

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
  --object OBJECTS      additional instrumented executable or shared library for LLVM reporting; repeatable
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

<a id="entry-helper-cli-format-content-55dd5b4af8"></a>
## Content formatter

Format .fomain, prototype and optional AngelScript files in the Engine layout and refuse any rewrite that would change what the Engine reads.

- Стабильный ID: `helper-cli.format-content`
- Программа: `format_content.py`
- Владелец: `tooling`
- Аудитория: `game-developer`, `embedding-project-build-system`, `ci-maintainer`
- Владелец вызова: BuildTools/buildtools.py format-content, embedding-project format tasks and CI drift checks
- Исходный парсер: [BuildTools/format_content.py](https://github.com/cvet/fonline/blob/master/BuildTools/format_content.py)

### Аргументы верхнего уровня

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-format-content-argument-check-8b8c3a568e"></a><code>helper-cli.format-content.argument.check</code> | <code>--check</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | report files that need formatting and exit with 1 without writing |
| <a id="entry-helper-cli-format-content-argument-project-root-6e4fec9931"></a><code>helper-cli.format-content.argument.project_root</code> | <code>--project-root</code> | <code>option</code> | нет | <code>1</code> | - | - | project root for relative paths and the .fomain lookup (default: FO_PROJECT_ROOT, then the current directory) |
| <a id="entry-helper-cli-format-content-argument-fomain-a606ea4773"></a><code>helper-cli.format-content.argument.fomain</code> | <code>--fomain</code> | <code>option</code> | нет | <code>1</code> | - | - | main config path (default: the only *.fomain file in the project root) |
| <a id="entry-helper-cli-format-content-argument-skip-fomain-73a5e4f85a"></a><code>helper-cli.format-content.argument.skip_fomain</code> | <code>--skip-fomain</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | do not format the main config |
| <a id="entry-helper-cli-format-content-argument-skip-protos-f2852331c7"></a><code>helper-cli.format-content.argument.skip_protos</code> | <code>--skip-protos</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | do not format prototype files |
| <a id="entry-helper-cli-format-content-argument-proto-path-b8908326cc"></a><code>helper-cli.format-content.argument.proto_path</code> | <code>--proto-path</code> | <code>option</code> | нет | <code>1</code> | - | - | format only these prototype files or directories instead of the Proto resource packs (repeatable) |
| <a id="entry-helper-cli-format-content-argument-metadata-5d8267408d"></a><code>helper-cli.format-content.argument.metadata</code> | <code>--metadata</code> | <code>option</code> | нет | <code>1</code> | - | - | read property and setting order from these files, in this order, instead of the Metadata resource packs (repeatable) |
| <a id="entry-helper-cli-format-content-argument-fos-509bfd7d26"></a><code>helper-cli.format-content.argument.fos</code> | <code>--fos</code> | <code>option</code> | нет | <code>1</code> | - | - | also format AngelScript files matching this glob relative to the project root with clang-format 20 (repeatable) |

### Точный вывод `--help` верхнего уровня

```text
usage: format_content.py [-h] [--check] [--project-root PROJECT_ROOT] [--fomain FOMAIN] [--skip-fomain] [--skip-protos] [--proto-path PATH] [--metadata PATH] [--fos PATTERN]

Format .fomain, prototype and optional AngelScript files without changing what the Engine reads from them.

options:
  -h, --help            show this help message and exit
  --check               report files that need formatting and exit with 1 without writing
  --project-root PROJECT_ROOT
                        project root for relative paths and the .fomain lookup (default: FO_PROJECT_ROOT, then the current directory)
  --fomain FOMAIN       main config path (default: the only *.fomain file in the project root)
  --skip-fomain         do not format the main config
  --skip-protos         do not format prototype files
  --proto-path PATH     format only these prototype files or directories instead of the Proto resource packs (repeatable)
  --metadata PATH       read property and setting order from these files, in this order, instead of the Metadata resource packs (repeatable)
  --fos PATTERN         also format AngelScript files matching this glob relative to the project root with clang-format 20 (repeatable)
```

<a id="entry-helper-cli-fo-formats-9aa60fdd1b"></a>
## Engine content format reader

Print effective configuration, resource-pack inputs, inherited prototype values and text-pack entries as the Engine parsers read them.

- Стабильный ID: `helper-cli.fo-formats`
- Программа: `fo_formats.py`
- Владелец: `tooling`
- Аудитория: `game-developer`, `tool-developer`, `ci-maintainer`
- Владелец вызова: embedding-project content tools and direct developer diagnostics
- Исходный парсер: [BuildTools/fo_formats.py](https://github.com/cvet/fonline/blob/master/BuildTools/fo_formats.py)

### Аргументы верхнего уровня

На этом уровне аргументов нет.

### Точный вывод `--help` верхнего уровня

```text
usage: fo_formats.py [-h] {config,protos,text} ...

Print Engine content formats as the Engine reads them, as JSON.

positional arguments:
  {config,protos,text}
    config              Effective settings, resource packs and sub-configs of a main config.
    protos              Effective prototype values of every Proto baker pack.
    text                Entries of one .fotxt text pack file.

options:
  -h, --help            show this help message and exit
```

<a id="entry-helper-cli-fo-formats-command-config-a240801186"></a>
### `config`

Effective settings, resource packs and sub-configs of a main config.

Stable ID: `helper-cli.fo-formats.command.config`

```text
usage: fo_formats.py config [-h] [--sub-config SUB_CONFIG] [--files] main_config
```

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-fo-formats-command-config-argument-main-config-f5f3e4257e"></a><code>helper-cli.fo-formats.command.config.argument.main_config</code> | <code>main_config</code> | <code>positional</code> | да | <code>1</code> | - | - | Main configuration file (.fomain). |
| <a id="entry-helper-cli-fo-formats-command-config-argument-sub-config-8c714c49f3"></a><code>helper-cli.fo-formats.command.config.argument.sub_config</code> | <code>--sub-config</code> | <code>option</code> | нет | <code>1</code> | - | - | Apply this [SubConfig] section after the main settings. |
| <a id="entry-helper-cli-fo-formats-command-config-argument-files-2e43f74a8d"></a><code>helper-cli.fo-formats.command.config.argument.files</code> | <code>--files</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | Also list the input files of every resource pack. |

#### Точный вывод `--help`

```text
usage: fo_formats.py config [-h] [--sub-config SUB_CONFIG] [--files] main_config

positional arguments:
  main_config           Main configuration file (.fomain).

options:
  -h, --help            show this help message and exit
  --sub-config SUB_CONFIG
                        Apply this [SubConfig] section after the main settings.
  --files               Also list the input files of every resource pack.
```

<a id="entry-helper-cli-fo-formats-command-protos-b4ecbf6062"></a>
### `protos`

Effective prototype values of every Proto baker pack.

Stable ID: `helper-cli.fo-formats.command.protos`

```text
usage: fo_formats.py protos [-h] [--sub-config SUB_CONFIG] [--type TYPE_NAME] [--name NAME] main_config
```

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-fo-formats-command-protos-argument-main-config-a9280d2130"></a><code>helper-cli.fo-formats.command.protos.argument.main_config</code> | <code>main_config</code> | <code>positional</code> | да | <code>1</code> | - | - | Main configuration file (.fomain). |
| <a id="entry-helper-cli-fo-formats-command-protos-argument-sub-config-717ae55730"></a><code>helper-cli.fo-formats.command.protos.argument.sub_config</code> | <code>--sub-config</code> | <code>option</code> | нет | <code>1</code> | - | - | Apply this [SubConfig] section after the main settings. |
| <a id="entry-helper-cli-fo-formats-command-protos-argument-type-name-f7ba8ff641"></a><code>helper-cli.fo-formats.command.protos.argument.type_name</code> | <code>--type</code> | <code>option</code> | нет | <code>1</code> | - | - | Print only prototypes of this type. |
| <a id="entry-helper-cli-fo-formats-command-protos-argument-name-6b38e320b2"></a><code>helper-cli.fo-formats.command.protos.argument.name</code> | <code>--name</code> | <code>option</code> | нет | <code>1</code> | - | - | Print only the prototype with this migrated name. |

#### Точный вывод `--help`

```text
usage: fo_formats.py protos [-h] [--sub-config SUB_CONFIG] [--type TYPE_NAME] [--name NAME] main_config

positional arguments:
  main_config           Main configuration file (.fomain).

options:
  -h, --help            show this help message and exit
  --sub-config SUB_CONFIG
                        Apply this [SubConfig] section after the main settings.
  --type TYPE_NAME      Print only prototypes of this type.
  --name NAME           Print only the prototype with this migrated name.
```

<a id="entry-helper-cli-fo-formats-command-text-0daf766e0e"></a>
### `text`

Entries of one .fotxt text pack file.

Stable ID: `helper-cli.fo-formats.command.text`

```text
usage: fo_formats.py text [-h] path
```

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-fo-formats-command-text-argument-path-5eefdb030f"></a><code>helper-cli.fo-formats.command.text.argument.path</code> | <code>path</code> | <code>positional</code> | да | <code>1</code> | - | - | Text pack file named &lt;TextPack&gt;.&lt;Language&gt;.fotxt. |

#### Точный вывод `--help`

```text
usage: fo_formats.py text [-h] path

positional arguments:
  path        Text pack file named <TextPack>.<Language>.fotxt.

options:
  -h, --help  show this help message and exit
```

<a id="entry-helper-cli-migration-guard-3e5c9b3e72"></a>
## Identity migration guard

Require a MigrationRule for every removed or renamed prototype, property and enum value and refuse reuse of retired identifiers against a committed manifest.

- Стабильный ID: `helper-cli.migration-guard`
- Программа: `migration_guard.py`
- Владелец: `quality`
- Аудитория: `game-developer`, `embedding-project-build-system`, `ci-maintainer`
- Владелец вызова: embedding-project local validation tasks and CI content gates
- Исходный парсер: [BuildTools/migration_guard.py](https://github.com/cvet/fonline/blob/master/BuildTools/migration_guard.py)

### Аргументы верхнего уровня

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-migration-guard-argument-check-208ac0af24"></a><code>helper-cli.migration-guard.argument.check</code> | <code>--check</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | Compare the tree with the manifest; exit 1 on any violation. |
| <a id="entry-helper-cli-migration-guard-argument-update-manifest-21506ae2f8"></a><code>helper-cli.migration-guard.argument.update_manifest</code> | <code>--update-manifest</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | Rewrite the manifest from the tree; refuses while a rule is missing or an identifier is reused. |
| <a id="entry-helper-cli-migration-guard-argument-root-a5a58eaaa8"></a><code>helper-cli.migration-guard.argument.root</code> | <code>--root</code> | <code>option</code> | нет | <code>1</code> | - | <code>.</code> | Project root; relative paths resolve against it (default: current directory). |
| <a id="entry-helper-cli-migration-guard-argument-config-99037a53c4"></a><code>helper-cli.migration-guard.argument.config</code> | <code>--config</code> | <code>option</code> | нет | <code>1</code> | - | - | JSON guard configuration, relative to the root. |
| <a id="entry-helper-cli-migration-guard-argument-main-config-419b231c42"></a><code>helper-cli.migration-guard.argument.main_config</code> | <code>--main-config</code> | <code>option</code> | нет | <code>1</code> | - | - | Main configuration file (.fomain) whose packs define the scope. |
| <a id="entry-helper-cli-migration-guard-argument-sub-config-1eb8560b24"></a><code>helper-cli.migration-guard.argument.sub_config</code> | <code>--sub-config</code> | <code>option</code> | нет | <code>1</code> | - | - | Apply this [SubConfig] of the main config before reading resource packs. |
| <a id="entry-helper-cli-migration-guard-argument-manifest-e4892f0674"></a><code>helper-cli.migration-guard.argument.manifest</code> | <code>--manifest</code> | <code>option</code> | нет | <code>1</code> | - | - | Manifest file to check or rewrite. |
| <a id="entry-helper-cli-migration-guard-argument-content-dir-e52d6014db"></a><code>helper-cli.migration-guard.argument.content_dir</code> | <code>--content-dir</code> | <code>option</code> | нет | <code>1</code> | - | - | Prototype directory; replaces the Proto packs of the main config. |
| <a id="entry-helper-cli-migration-guard-argument-proto-extension-df65aff360"></a><code>helper-cli.migration-guard.argument.proto_extension</code> | <code>--proto-extension</code> | <code>option</code> | нет | <code>1</code> | - | - | Prototype file extension used with --content-dir (default: Baking.ProtoFileExtensions). |
| <a id="entry-helper-cli-migration-guard-argument-script-dir-28433b02c0"></a><code>helper-cli.migration-guard.argument.script_dir</code> | <code>--script-dir</code> | <code>option</code> | нет | <code>1</code> | - | - | Script source directory; replaces the Metadata packs of the main config. |
| <a id="entry-helper-cli-migration-guard-argument-native-source-86584a00f7"></a><code>helper-cli.migration-guard.argument.native_source</code> | <code>--native-source</code> | <code>option</code> | нет | <code>1</code> | - | - | Root-relative glob of native sources with property and enum declarations. |

### Точный вывод `--help` верхнего уровня

```text
usage: migration_guard.py [-h] (--check | --update-manifest) [--root ROOT] [--config CONFIG] [--main-config MAIN_CONFIG] [--sub-config SUB_CONFIG] [--manifest MANIFEST] [--content-dir CONTENT_DIR] [--proto-extension PROTO_EXTENSION]
                          [--script-dir SCRIPT_DIR] [--native-source NATIVE_SOURCE]

Require a MigrationRule for every removed or renamed prototype, property and enum value, and refuse reuse of a retired identifier, against a committed manifest.

options:
  -h, --help            show this help message and exit
  --check               Compare the tree with the manifest; exit 1 on any violation.
  --update-manifest     Rewrite the manifest from the tree; refuses while a rule is missing or an identifier is reused.
  --root ROOT           Project root; relative paths resolve against it (default: current directory).
  --config CONFIG       JSON guard configuration, relative to the root.
  --main-config MAIN_CONFIG
                        Main configuration file (.fomain) whose packs define the scope.
  --sub-config SUB_CONFIG
                        Apply this [SubConfig] of the main config before reading resource packs.
  --manifest MANIFEST   Manifest file to check or rewrite.
  --content-dir CONTENT_DIR
                        Prototype directory; replaces the Proto packs of the main config.
  --proto-extension PROTO_EXTENSION
                        Prototype file extension used with --content-dir (default: Baking.ProtoFileExtensions).
  --script-dir SCRIPT_DIR
                        Script source directory; replaces the Metadata packs of the main config.
  --native-source NATIVE_SOURCE
                        Root-relative glob of native sources with property and enum declarations.
```

<a id="entry-helper-cli-ai-control-mcp-f5daba9e92"></a>
## MCP-адаптер AiControl

Предоставляет bridge AiControl MCP-клиенту через stdio: общие инструменты протокола, endpoint, готовности и собственных процессов плюс инструменты, зарегистрированные проектом; инструмент разработки, никогда не поставляется в выпуске.

- Стабильный ID: `helper-cli.ai-control-mcp`
- Программа: `ai_control_mcp`
- Владелец: `tooling`
- Аудитория: `game-developer`, `tool-developer`, `ci-maintainer`
- Владелец вызова: конфигурации MCP-клиентов встраивающего проекта, проектные модули инструментов и раннеры ИИ-плейтестов
- Исходный парсер: [BuildTools/ai_control_mcp/cli.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_mcp/cli.py)

### Аргументы верхнего уровня

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-ai-control-mcp-argument-host-7b34651295"></a><code>helper-cli.ai-control-mcp.argument.host</code> | <code>--host</code> | <code>option</code> | нет | <code>1</code> | - | <code>127.0.0.1</code> | Хост bridge по умолчанию. |
| <a id="entry-helper-cli-ai-control-mcp-argument-port-abe34ef593"></a><code>helper-cli.ai-control-mcp.argument.port</code> | <code>--port</code> | <code>option</code> | нет | <code>1</code> | - | <code>43011</code> | Порт bridge по умолчанию и базовый порт для запусков. |
| <a id="entry-helper-cli-ai-control-mcp-argument-timeout-fb4db925fb"></a><code>helper-cli.ai-control-mcp.argument.timeout</code> | <code>--timeout</code> | <code>option</code> | нет | <code>1</code> | - | <code>5.0</code> | Тайм-аут сокета bridge в секундах. |
| <a id="entry-helper-cli-ai-control-mcp-argument-token-env-860365e381"></a><code>helper-cli.ai-control-mcp.argument.token_env</code> | <code>--token-env</code> | <code>option</code> | нет | <code>1</code> | - | <code>FONLINE_AI_TOKEN</code> | Переменная окружения с общим токеном bridge. |
| <a id="entry-helper-cli-ai-control-mcp-argument-allow-remote-198670fb65"></a><code>helper-cli.ai-control-mcp.argument.allow_remote</code> | <code>--allow-remote</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | Разрешить не-loopback bridges; им также нужен токен. |
| <a id="entry-helper-cli-ai-control-mcp-argument-project-root-7c11b8ad5e"></a><code>helper-cli.ai-control-mcp.argument.project_root</code> | <code>--project-root</code> | <code>option</code> | нет | <code>1</code> | - | <code>.</code> | Корень для путей запуска, бинарников и журналов. |
| <a id="entry-helper-cli-ai-control-mcp-argument-project-module-9c91958182"></a><code>helper-cli.ai-control-mcp.argument.project_module</code> | <code>--project-module</code> | <code>option</code> | нет | <code>1</code> | - | - | Имя модуля через точку или файл .py, чей register(registry) добавляет проектные инструменты; можно повторять. |
| <a id="entry-helper-cli-ai-control-mcp-argument-module-path-88a2b91f23"></a><code>helper-cli.ai-control-mcp.argument.module_path</code> | <code>--module-path</code> | <code>option</code> | нет | <code>1</code> | - | - | Каталог, добавляемый в путь импорта для проектных модулей, заданных через точку. |
| <a id="entry-helper-cli-ai-control-mcp-argument-launch-profile-651c076ad8"></a><code>helper-cli.ai-control-mcp.argument.launch_profile</code> | <code>--launch-profile</code> | <code>option</code> | нет | <code>1</code> | - | - | JSON-профиль запуска; заменяет профиль, заданный проектным модулем. |
| <a id="entry-helper-cli-ai-control-mcp-argument-tool-prefix-81f493735e"></a><code>helper-cli.ai-control-mcp.argument.tool_prefix</code> | <code>--tool-prefix</code> | <code>option</code> | нет | <code>1</code> | - | <code>fo_</code> | Префикс имён базовых инструментов. |
| <a id="entry-helper-cli-ai-control-mcp-argument-server-name-fc5871ec1c"></a><code>helper-cli.ai-control-mcp.argument.server_name</code> | <code>--server-name</code> | <code>option</code> | нет | <code>1</code> | - | <code>fonline-ai-control</code> | Имя serverInfo для MCP. |
| <a id="entry-helper-cli-ai-control-mcp-argument-keep-processes-on-exit-f35bb54895"></a><code>helper-cli.ai-control-mcp.argument.keep_processes_on_exit</code> | <code>--keep-processes-on-exit</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | Оставить запущенные процессы работать после выхода адаптера; их деревья затем нужно остановить вручную. |
| <a id="entry-helper-cli-ai-control-mcp-argument-max-message-bytes-54d556734f"></a><code>helper-cli.ai-control-mcp.argument.max_message_bytes</code> | <code>--max-message-bytes</code> | <code>option</code> | нет | <code>1</code> | - | <code>4194304</code> | Самая длинная допустимая строка сообщения MCP. |
| <a id="entry-helper-cli-ai-control-mcp-argument-list-tools-734a9df574"></a><code>helper-cli.ai-control-mcp.argument.list_tools</code> | <code>--list-tools</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | Напечатать имена и описания зарегистрированных инструментов в JSON и выйти. |

### Точный вывод `--help` верхнего уровня

```text
usage: ai_control_mcp [-h] [--host HOST] [--port PORT] [--timeout TIMEOUT] [--token-env TOKEN_ENV] [--allow-remote] [--project-root PROJECT_ROOT] [--project-module PROJECT_MODULE] [--module-path MODULE_PATH]
                      [--launch-profile LAUNCH_PROFILE] [--tool-prefix TOOL_PREFIX] [--server-name SERVER_NAME] [--keep-processes-on-exit] [--max-message-bytes MAX_MESSAGE_BYTES] [--list-tools]

Serve an AiControl bridge to an MCP client over stdio; a development tool, never shipped.

options:
  -h, --help            show this help message and exit
  --host HOST           Default bridge host.
  --port PORT           Default bridge port and base port for launches.
  --timeout TIMEOUT     Bridge socket timeout in seconds.
  --token-env TOKEN_ENV
                        Environment variable containing the shared bridge token.
  --allow-remote        Permit non-loopback bridges; they also require a token.
  --project-root PROJECT_ROOT
                        Root for launch paths, binaries and logs.
  --project-module PROJECT_MODULE
                        Dotted module name or .py file whose register(registry) adds project tools; repeatable.
  --module-path MODULE_PATH
                        Directory added to the import path for dotted project modules.
  --launch-profile LAUNCH_PROFILE
                        JSON launch profile; overrides a profile set by a project module.
  --tool-prefix TOOL_PREFIX
                        Prefix of the core tool names.
  --server-name SERVER_NAME
                        MCP serverInfo name.
  --keep-processes-on-exit
                        Leave launched processes running after the adapter exits; their trees must then be stopped manually.
  --max-message-bytes MAX_MESSAGE_BYTES
                        Largest accepted MCP message line.
  --list-tools          Print the registered tool names and descriptions as JSON, then exit.
```

<a id="entry-helper-cli-windows7-import-check-a0c7e4cb59"></a>
## Проверка импортов Windows 7

Проверяет совместимость импортов слинкованных PE-файлов с Windows 7 и по запросу требует флаг большого адресного пространства у исполняемых файлов.

- Стабильный ID: `helper-cli.windows7-import-check`
- Программа: `check_windows7_imports.py`
- Владелец: `quality`
- Аудитория: `engine-contributor`, `embedding-project-build-system`, `release-operator`
- Владелец вызова: CI и релизная проверка Windows 7 подключающего проекта
- Исходный парсер: [BuildTools/check_windows7_imports.py](https://github.com/cvet/fonline/blob/master/BuildTools/check_windows7_imports.py)

### Аргументы верхнего уровня

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-windows7-import-check-argument-require-large-address-aware-eb51248263"></a><code>helper-cli.windows7-import-check.argument.require_large_address_aware</code> | <code>--require-large-address-aware</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | Требовать флаг большого адресного пространства у проверяемых исполняемых файлов. |
| <a id="entry-helper-cli-windows7-import-check-argument-binaries-90e898370f"></a><code>helper-cli.windows7-import-check.argument.binaries</code> | <code>binaries</code> | <code>positional</code> | да | <code>+</code> | - | - | Проверяемый слинкованный исполняемый PE-файл или DLL. |

### Точный вывод `--help` верхнего уровня

```text
usage: check_windows7_imports.py [-h] [--require-large-address-aware] binaries [binaries ...]

Reject imports Windows 7 SP1 cannot resolve from Windows 7-compatible PE binaries

positional arguments:
  binaries              linked PE executable or DLL to inspect

options:
  -h, --help            show this help message and exit
  --require-large-address-aware
                        Require the large-address flag on checked executables
```

<a id="entry-helper-cli-markdown-links-dd3ef679c3"></a>
## Markdown link check

Check local Markdown links, GitHub-style heading anchors, UTF-8, fenced-code balance, and optional single-H1 structure under one repository root.

- Стабильный ID: `helper-cli.markdown-links`
- Программа: `markdown_links.py`
- Владелец: `quality`
- Аудитория: `game-developer`, `engine-contributor`, `embedding-project-build-system`, `ci-maintainer`
- Владелец вызова: embedding-project documentation wrappers, local tasks, and CI documentation jobs
- Исходный парсер: [BuildTools/markdown_links.py](https://github.com/cvet/fonline/blob/master/BuildTools/markdown_links.py)

### Аргументы верхнего уровня

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-markdown-links-argument-root-186c609292"></a><code>helper-cli.markdown-links.argument.root</code> | <code>--root</code> | <code>option</code> | нет | <code>1</code> | - | <code>.</code> | repository root that bounds link targets (default: current directory) |
| <a id="entry-helper-cli-markdown-links-argument-include-b34d770aa2"></a><code>helper-cli.markdown-links.argument.include</code> | <code>--include</code> | <code>option</code> | нет | <code>1</code> | - | - | root-relative document glob; repeatable (default: **/*.md and **/*.markdown) |
| <a id="entry-helper-cli-markdown-links-argument-exclude-386dea20e1"></a><code>helper-cli.markdown-links.argument.exclude</code> | <code>--exclude</code> | <code>option</code> | нет | <code>1</code> | - | - | root-relative file or directory glob removed from the document set; repeatable |
| <a id="entry-helper-cli-markdown-links-argument-target-root-fcbed4592b"></a><code>helper-cli.markdown-links.argument.target_root</code> | <code>--target-root</code> | <code>option</code> | нет | <code>1</code> | - | - | extra directory, relative to --root, that links may point into without its documents being checked; repeatable |
| <a id="entry-helper-cli-markdown-links-argument-skip-target-04617bb757"></a><code>helper-cli.markdown-links.argument.skip_target</code> | <code>--skip-target</code> | <code>option</code> | нет | <code>1</code> | - | - | root-relative glob of link targets that are not verified, such as a submodule that is not checked out; repeatable |
| <a id="entry-helper-cli-markdown-links-argument-allow-line-fragments-ca419e631a"></a><code>helper-cli.markdown-links.argument.allow_line_fragments</code> | <code>--allow-line-fragments</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | accept #L10 and #L10-L20 line fragments on Markdown targets |
| <a id="entry-helper-cli-markdown-links-argument-require-reference-definitions-9d60c1c23a"></a><code>helper-cli.markdown-links.argument.require_reference_definitions</code> | <code>--require-reference-definitions</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | report [text][label] links whose label has no definition in the same document |
| <a id="entry-helper-cli-markdown-links-argument-require-single-h1-5cf2bb0ab9"></a><code>helper-cli.markdown-links.argument.require_single_h1</code> | <code>--require-single-h1</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | report documents that do not contain exactly one H1 heading |
| <a id="entry-helper-cli-markdown-links-argument-h1-exempt-a1a2bb6f20"></a><code>helper-cli.markdown-links.argument.h1_exempt</code> | <code>--h1-exempt</code> | <code>option</code> | нет | <code>1</code> | - | - | root-relative glob of documents exempt from --require-single-h1; repeatable |
| <a id="entry-helper-cli-markdown-links-argument-list-24f08b8e09"></a><code>helper-cli.markdown-links.argument.list</code> | <code>--list</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | print the selected documents instead of checking them |

### Точный вывод `--help` верхнего уровня

```text
usage: markdown_links.py [-h] [--root ROOT] [--include GLOB] [--exclude GLOB] [--target-root DIR] [--skip-target GLOB] [--allow-line-fragments] [--require-reference-definitions] [--require-single-h1] [--h1-exempt GLOB] [--list]

Check local Markdown links, heading anchors, UTF-8 and fenced-code balance under one root

options:
  -h, --help            show this help message and exit
  --root ROOT           repository root that bounds link targets (default: current directory)
  --include GLOB        root-relative document glob; repeatable (default: **/*.md and **/*.markdown)
  --exclude GLOB        root-relative file or directory glob removed from the document set; repeatable
  --target-root DIR     extra directory, relative to --root, that links may point into without its documents being checked; repeatable
  --skip-target GLOB    root-relative glob of link targets that are not verified, such as a submodule that is not checked out; repeatable
  --allow-line-fragments
                        accept #L10 and #L10-L20 line fragments on Markdown targets
  --require-reference-definitions
                        report [text][label] links whose label has no definition in the same document
  --require-single-h1   report documents that do not contain exactly one H1 heading
  --h1-exempt GLOB      root-relative glob of documents exempt from --require-single-h1; repeatable
  --list                print the selected documents instead of checking them
```

<a id="entry-helper-cli-native-source-audit-b6c759ee81"></a>
## Native source audits

Audit native C++ scopes for allocator, smart-pointer and local-variable contract violations with Engine-owned and project baselines.

- Стабильный ID: `helper-cli.native-source-audit`
- Программа: `native_source_audit.py`
- Владелец: `quality`
- Аудитория: `engine-contributor`, `embedding-project-build-system`, `ci-maintainer`
- Владелец вызова: Engine validation CI, embedding-project native audit wrappers and direct developer use
- Исходный парсер: [BuildTools/native_source_audit.py](https://github.com/cvet/fonline/blob/master/BuildTools/native_source_audit.py)

### Аргументы верхнего уровня

На этом уровне аргументов нет.

### Точный вывод `--help` верхнего уровня

```text
usage: native_source_audit.py [-h] AUDIT ...

Audit native C++ sources against the Engine coding contracts. Each audit scans --scope paths below --root (default: the Engine Source directory), applies the Engine-owned baseline plus project baselines, and fails on remaining findings or
on stale baseline rows. Exit codes: 0 passed or skipped, 1 findings or stale baseline rows, 2 invalid invocation, configuration, baseline or tool failure.

positional arguments:
  AUDIT
    allocator           Report standard containers, allocating std:: helpers and C allocation outside the memory system.
    smart-pointer       Check the ptr/nptr/unique/refcount smart-pointer vocabulary and raw-pointer boundaries.
    smart-pointer-ast   Find raw pointer declarations on lines added since a git revision with clang-query.
    local-variables     Find redundant top-level const locals (clang-query) and use-after-move (clang-tidy).
    explicit-local-types
                        Find auto locals whose deduced type is a simple explicit type (clang-query).

options:
  -h, --help            show this help message and exit
```

<a id="entry-helper-cli-native-source-audit-command-allocator-f2635ae834"></a>
### `allocator`

Report standard containers, allocating std:: helpers and C allocation outside the memory system.

Stable ID: `helper-cli.native-source-audit.command.allocator`

```text
usage: native_source_audit.py allocator [-h] [--root ROOT] [--scope SCOPE] [--exclude EXCLUDE] [--config CONFIG] [--baseline BASELINE] [--no-engine-baseline] [--report REPORT] [--emit-baseline EMIT_BASELINE] [--max-printed MAX_PRINTED]
                                        [--engine-root ENGINE_ROOT] [--fail-on {error,warning,none}]
```

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-native-source-audit-command-allocator-argument-root-b51b4df71f"></a><code>helper-cli.native-source-audit.command.allocator.argument.root</code> | <code>--root</code> | <code>option</code> | нет | <code>1</code> | - | - | Root that scopes, excludes, reports and project baselines are relative to (default: current directory). |
| <a id="entry-helper-cli-native-source-audit-command-allocator-argument-scope-e650c5db32"></a><code>helper-cli.native-source-audit.command.allocator.argument.scope</code> | <code>--scope</code> | <code>option</code> | нет | <code>1</code> | - | - | File or directory to audit, relative to --root; repeatable (default: config scopes, else the Engine Source directory). |
| <a id="entry-helper-cli-native-source-audit-command-allocator-argument-exclude-c757307d99"></a><code>helper-cli.native-source-audit.command.allocator.argument.exclude</code> | <code>--exclude</code> | <code>option</code> | нет | <code>1</code> | - | - | fnmatch pattern of root-relative paths to skip; a pattern ending in / skips a subtree; repeatable. |
| <a id="entry-helper-cli-native-source-audit-command-allocator-argument-config-1351d21964"></a><code>helper-cli.native-source-audit.command.allocator.argument.config</code> | <code>--config</code> | <code>option</code> | нет | <code>1</code> | - | - | JSON configuration with scopes, exclude patterns, baselines and audit options; relative to --root. |
| <a id="entry-helper-cli-native-source-audit-command-allocator-argument-baseline-e81d76ed86"></a><code>helper-cli.native-source-audit.command.allocator.argument.baseline</code> | <code>--baseline</code> | <code>option</code> | нет | <code>1</code> | - | - | Additional project baseline TSV with root-relative paths; repeatable. |
| <a id="entry-helper-cli-native-source-audit-command-allocator-argument-no-engine-baseline-49facc3a84"></a><code>helper-cli.native-source-audit.command.allocator.argument.no_engine_baseline</code> | <code>--no-engine-baseline</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | Do not apply BuildTools/audit_baselines/&lt;audit&gt;.tsv from the Engine. |
| <a id="entry-helper-cli-native-source-audit-command-allocator-argument-report-bc2532a22e"></a><code>helper-cli.native-source-audit.command.allocator.argument.report</code> | <code>--report</code> | <code>option</code> | нет | <code>1</code> | - | - | Write remaining findings and stale baseline rows as TSV. |
| <a id="entry-helper-cli-native-source-audit-command-allocator-argument-emit-baseline-693dd50cd4"></a><code>helper-cli.native-source-audit.command.allocator.argument.emit_baseline</code> | <code>--emit-baseline</code> | <code>option</code> | нет | <code>1</code> | - | - | Write candidate baseline rows (empty reason column) for the remaining findings. |
| <a id="entry-helper-cli-native-source-audit-command-allocator-argument-max-printed-6237b58a2c"></a><code>helper-cli.native-source-audit.command.allocator.argument.max_printed</code> | <code>--max-printed</code> | <code>option</code> | нет | <code>1</code> | - | <code>50</code> | Maximum findings printed to stdout; 0 prints all (default: 50). |
| <a id="entry-helper-cli-native-source-audit-command-allocator-argument-engine-root-dabaaa3d97"></a><code>helper-cli.native-source-audit.command.allocator.argument.engine_root</code> | <code>--engine-root</code> | <code>option</code> | нет | <code>1</code> | - | - | Engine checkout whose Source rules and BuildTools/audit_baselines apply (default: the Engine containing this tool). |
| <a id="entry-helper-cli-native-source-audit-command-allocator-argument-fail-on-143ec39510"></a><code>helper-cli.native-source-audit.command.allocator.argument.fail_on</code> | <code>--fail-on</code> | <code>option</code> | нет | <code>1</code> | <code>error</code>, <code>warning</code>, <code>none</code> | <code>error</code> | Minimum severity that fails the command (default: error). |

#### Точный вывод `--help`

```text
usage: native_source_audit.py allocator [-h] [--root ROOT] [--scope SCOPE] [--exclude EXCLUDE] [--config CONFIG] [--baseline BASELINE] [--no-engine-baseline] [--report REPORT] [--emit-baseline EMIT_BASELINE] [--max-printed MAX_PRINTED]
                                        [--engine-root ENGINE_ROOT] [--fail-on {error,warning,none}]

Report standard containers, allocating std:: helpers and bare C allocation calls that bypass the Engine memory system.

options:
  -h, --help            show this help message and exit
  --root ROOT           Root that scopes, excludes, reports and project baselines are relative to (default: current directory).
  --scope SCOPE         File or directory to audit, relative to --root; repeatable (default: config scopes, else the Engine Source directory).
  --exclude EXCLUDE     fnmatch pattern of root-relative paths to skip; a pattern ending in / skips a subtree; repeatable.
  --config CONFIG       JSON configuration with scopes, exclude patterns, baselines and audit options; relative to --root.
  --baseline BASELINE   Additional project baseline TSV with root-relative paths; repeatable.
  --no-engine-baseline  Do not apply BuildTools/audit_baselines/<audit>.tsv from the Engine.
  --report REPORT       Write remaining findings and stale baseline rows as TSV.
  --emit-baseline EMIT_BASELINE
                        Write candidate baseline rows (empty reason column) for the remaining findings.
  --max-printed MAX_PRINTED
                        Maximum findings printed to stdout; 0 prints all (default: 50).
  --engine-root ENGINE_ROOT
                        Engine checkout whose Source rules and BuildTools/audit_baselines apply (default: the Engine containing this tool).
  --fail-on {error,warning,none}
                        Minimum severity that fails the command (default: error).
```

<a id="entry-helper-cli-native-source-audit-command-smart-pointer-da5c5ce703"></a>
### `smart-pointer`

Check the ptr/nptr/unique/refcount smart-pointer vocabulary and raw-pointer boundaries.

Stable ID: `helper-cli.native-source-audit.command.smart-pointer`

```text
usage: native_source_audit.py smart-pointer [-h] [--root ROOT] [--scope SCOPE] [--exclude EXCLUDE] [--config CONFIG] [--baseline BASELINE] [--no-engine-baseline] [--report REPORT] [--emit-baseline EMIT_BASELINE]
                                            [--max-printed MAX_PRINTED] [--engine-root ENGINE_ROOT] [--fail-on {error,warning,none}] [--inventory] [--inventory-output INVENTORY_OUTPUT] [--inventory-details-output INVENTORY_DETAILS_OUTPUT]
                                            [--summary-output SUMMARY_OUTPUT] [--jobs JOBS]
```

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-native-source-audit-command-smart-pointer-argument-root-bc026d9d60"></a><code>helper-cli.native-source-audit.command.smart-pointer.argument.root</code> | <code>--root</code> | <code>option</code> | нет | <code>1</code> | - | - | Root that scopes, excludes, reports and project baselines are relative to (default: current directory). |
| <a id="entry-helper-cli-native-source-audit-command-smart-pointer-argument-scope-b023b38f83"></a><code>helper-cli.native-source-audit.command.smart-pointer.argument.scope</code> | <code>--scope</code> | <code>option</code> | нет | <code>1</code> | - | - | File or directory to audit, relative to --root; repeatable (default: config scopes, else the Engine Source directory). |
| <a id="entry-helper-cli-native-source-audit-command-smart-pointer-argument-exclude-053fc1fd37"></a><code>helper-cli.native-source-audit.command.smart-pointer.argument.exclude</code> | <code>--exclude</code> | <code>option</code> | нет | <code>1</code> | - | - | fnmatch pattern of root-relative paths to skip; a pattern ending in / skips a subtree; repeatable. |
| <a id="entry-helper-cli-native-source-audit-command-smart-pointer-argument-config-1331c94d38"></a><code>helper-cli.native-source-audit.command.smart-pointer.argument.config</code> | <code>--config</code> | <code>option</code> | нет | <code>1</code> | - | - | JSON configuration with scopes, exclude patterns, baselines and audit options; relative to --root. |
| <a id="entry-helper-cli-native-source-audit-command-smart-pointer-argument-baseline-7b6d3a9111"></a><code>helper-cli.native-source-audit.command.smart-pointer.argument.baseline</code> | <code>--baseline</code> | <code>option</code> | нет | <code>1</code> | - | - | Additional project baseline TSV with root-relative paths; repeatable. |
| <a id="entry-helper-cli-native-source-audit-command-smart-pointer-argument-no-engine-baseline-a3a6eeecd5"></a><code>helper-cli.native-source-audit.command.smart-pointer.argument.no_engine_baseline</code> | <code>--no-engine-baseline</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | Do not apply BuildTools/audit_baselines/&lt;audit&gt;.tsv from the Engine. |
| <a id="entry-helper-cli-native-source-audit-command-smart-pointer-argument-report-a00b854ac3"></a><code>helper-cli.native-source-audit.command.smart-pointer.argument.report</code> | <code>--report</code> | <code>option</code> | нет | <code>1</code> | - | - | Write remaining findings and stale baseline rows as TSV. |
| <a id="entry-helper-cli-native-source-audit-command-smart-pointer-argument-emit-baseline-14aaecc175"></a><code>helper-cli.native-source-audit.command.smart-pointer.argument.emit_baseline</code> | <code>--emit-baseline</code> | <code>option</code> | нет | <code>1</code> | - | - | Write candidate baseline rows (empty reason column) for the remaining findings. |
| <a id="entry-helper-cli-native-source-audit-command-smart-pointer-argument-max-printed-1d8f2623b6"></a><code>helper-cli.native-source-audit.command.smart-pointer.argument.max_printed</code> | <code>--max-printed</code> | <code>option</code> | нет | <code>1</code> | - | <code>50</code> | Maximum findings printed to stdout; 0 prints all (default: 50). |
| <a id="entry-helper-cli-native-source-audit-command-smart-pointer-argument-engine-root-450d894b59"></a><code>helper-cli.native-source-audit.command.smart-pointer.argument.engine_root</code> | <code>--engine-root</code> | <code>option</code> | нет | <code>1</code> | - | - | Engine checkout whose Source rules and BuildTools/audit_baselines apply (default: the Engine containing this tool). |
| <a id="entry-helper-cli-native-source-audit-command-smart-pointer-argument-fail-on-e469434b31"></a><code>helper-cli.native-source-audit.command.smart-pointer.argument.fail_on</code> | <code>--fail-on</code> | <code>option</code> | нет | <code>1</code> | <code>error</code>, <code>warning</code>, <code>none</code> | <code>error</code> | Minimum severity that fails the command (default: error). |
| <a id="entry-helper-cli-native-source-audit-command-smart-pointer-argument-inventory-f6423df9ea"></a><code>helper-cli.native-source-audit.command.smart-pointer.argument.inventory</code> | <code>--inventory</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | Print non-failing inventory counters for pointer vocabulary and raw-pointer markers. |
| <a id="entry-helper-cli-native-source-audit-command-smart-pointer-argument-inventory-output-f8f36da320"></a><code>helper-cli.native-source-audit.command.smart-pointer.argument.inventory_output</code> | <code>--inventory-output</code> | <code>option</code> | нет | <code>1</code> | - | - | Write per-file inventory counters as TSV. |
| <a id="entry-helper-cli-native-source-audit-command-smart-pointer-argument-inventory-details-output-d67935ddb7"></a><code>helper-cli.native-source-audit.command.smart-pointer.argument.inventory_details_output</code> | <code>--inventory-details-output</code> | <code>option</code> | нет | <code>1</code> | - | - | Write line-level inventory markers as TSV. |
| <a id="entry-helper-cli-native-source-audit-command-smart-pointer-argument-summary-output-ac8ab040b4"></a><code>helper-cli.native-source-audit.command.smart-pointer.argument.summary_output</code> | <code>--summary-output</code> | <code>option</code> | нет | <code>1</code> | - | - | Write a JSON summary of findings and inventory counters. |
| <a id="entry-helper-cli-native-source-audit-command-smart-pointer-argument-jobs-c22afc8be0"></a><code>helper-cli.native-source-audit.command.smart-pointer.argument.jobs</code> | <code>--jobs</code> | <code>option</code> | нет | <code>1</code> | - | <code>0</code> | Worker processes; 0 uses the CPU count; output order does not depend on it (default: 0). |

#### Точный вывод `--help`

```text
usage: native_source_audit.py smart-pointer [-h] [--root ROOT] [--scope SCOPE] [--exclude EXCLUDE] [--config CONFIG] [--baseline BASELINE] [--no-engine-baseline] [--report REPORT] [--emit-baseline EMIT_BASELINE]
                                            [--max-printed MAX_PRINTED] [--engine-root ENGINE_ROOT] [--fail-on {error,warning,none}] [--inventory] [--inventory-output INVENTORY_OUTPUT] [--inventory-details-output INVENTORY_DETAILS_OUTPUT]
                                            [--summary-output SUMMARY_OUTPUT] [--jobs JOBS]

Check the smart-pointer vocabulary, nullable checks and raw-pointer boundary shapes with text rules.

options:
  -h, --help            show this help message and exit
  --root ROOT           Root that scopes, excludes, reports and project baselines are relative to (default: current directory).
  --scope SCOPE         File or directory to audit, relative to --root; repeatable (default: config scopes, else the Engine Source directory).
  --exclude EXCLUDE     fnmatch pattern of root-relative paths to skip; a pattern ending in / skips a subtree; repeatable.
  --config CONFIG       JSON configuration with scopes, exclude patterns, baselines and audit options; relative to --root.
  --baseline BASELINE   Additional project baseline TSV with root-relative paths; repeatable.
  --no-engine-baseline  Do not apply BuildTools/audit_baselines/<audit>.tsv from the Engine.
  --report REPORT       Write remaining findings and stale baseline rows as TSV.
  --emit-baseline EMIT_BASELINE
                        Write candidate baseline rows (empty reason column) for the remaining findings.
  --max-printed MAX_PRINTED
                        Maximum findings printed to stdout; 0 prints all (default: 50).
  --engine-root ENGINE_ROOT
                        Engine checkout whose Source rules and BuildTools/audit_baselines apply (default: the Engine containing this tool).
  --fail-on {error,warning,none}
                        Minimum severity that fails the command (default: error).
  --inventory           Print non-failing inventory counters for pointer vocabulary and raw-pointer markers.
  --inventory-output INVENTORY_OUTPUT
                        Write per-file inventory counters as TSV.
  --inventory-details-output INVENTORY_DETAILS_OUTPUT
                        Write line-level inventory markers as TSV.
  --summary-output SUMMARY_OUTPUT
                        Write a JSON summary of findings and inventory counters.
  --jobs JOBS           Worker processes; 0 uses the CPU count; output order does not depend on it (default: 0).
```

<a id="entry-helper-cli-native-source-audit-command-smart-pointer-ast-1e450a0b76"></a>
### `smart-pointer-ast`

Find raw pointer declarations on lines added since a git revision with clang-query.

Stable ID: `helper-cli.native-source-audit.command.smart-pointer-ast`

```text
usage: native_source_audit.py smart-pointer-ast [-h] [--root ROOT] [--scope SCOPE] [--exclude EXCLUDE] [--config CONFIG] [--baseline BASELINE] [--no-engine-baseline] [--report REPORT] [--emit-baseline EMIT_BASELINE]
                                                [--max-printed MAX_PRINTED] [--engine-root ENGINE_ROOT] [--compilation-database COMPILATION_DATABASE] [--clang-query CLANG_QUERY] [--clang-tidy CLANG_TIDY] [--require-tooling]
                                                [--source-pattern SOURCE_PATTERN] [--unit-pattern UNIT_PATTERN] [--jobs JOBS] [--batch-size BATCH_SIZE] [--timeout TIMEOUT] [--diff-base DIFF_BASE] [--all]
```

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-native-source-audit-command-smart-pointer-ast-argument-root-709ef1d1da"></a><code>helper-cli.native-source-audit.command.smart-pointer-ast.argument.root</code> | <code>--root</code> | <code>option</code> | нет | <code>1</code> | - | - | Root that scopes, excludes, reports and project baselines are relative to (default: current directory). |
| <a id="entry-helper-cli-native-source-audit-command-smart-pointer-ast-argument-scope-3181f4d5e6"></a><code>helper-cli.native-source-audit.command.smart-pointer-ast.argument.scope</code> | <code>--scope</code> | <code>option</code> | нет | <code>1</code> | - | - | File or directory to audit, relative to --root; repeatable (default: config scopes, else the Engine Source directory). |
| <a id="entry-helper-cli-native-source-audit-command-smart-pointer-ast-argument-exclude-d54e3ec139"></a><code>helper-cli.native-source-audit.command.smart-pointer-ast.argument.exclude</code> | <code>--exclude</code> | <code>option</code> | нет | <code>1</code> | - | - | fnmatch pattern of root-relative paths to skip; a pattern ending in / skips a subtree; repeatable. |
| <a id="entry-helper-cli-native-source-audit-command-smart-pointer-ast-argument-config-e3d7bd573c"></a><code>helper-cli.native-source-audit.command.smart-pointer-ast.argument.config</code> | <code>--config</code> | <code>option</code> | нет | <code>1</code> | - | - | JSON configuration with scopes, exclude patterns, baselines and audit options; relative to --root. |
| <a id="entry-helper-cli-native-source-audit-command-smart-pointer-ast-argument-baseline-be3de9b966"></a><code>helper-cli.native-source-audit.command.smart-pointer-ast.argument.baseline</code> | <code>--baseline</code> | <code>option</code> | нет | <code>1</code> | - | - | Additional project baseline TSV with root-relative paths; repeatable. |
| <a id="entry-helper-cli-native-source-audit-command-smart-pointer-ast-argument-no-engine-baseline-06eac88335"></a><code>helper-cli.native-source-audit.command.smart-pointer-ast.argument.no_engine_baseline</code> | <code>--no-engine-baseline</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | Do not apply BuildTools/audit_baselines/&lt;audit&gt;.tsv from the Engine. |
| <a id="entry-helper-cli-native-source-audit-command-smart-pointer-ast-argument-report-ecdfab81d1"></a><code>helper-cli.native-source-audit.command.smart-pointer-ast.argument.report</code> | <code>--report</code> | <code>option</code> | нет | <code>1</code> | - | - | Write remaining findings and stale baseline rows as TSV. |
| <a id="entry-helper-cli-native-source-audit-command-smart-pointer-ast-argument-emit-baseline-2bd7f0b33c"></a><code>helper-cli.native-source-audit.command.smart-pointer-ast.argument.emit_baseline</code> | <code>--emit-baseline</code> | <code>option</code> | нет | <code>1</code> | - | - | Write candidate baseline rows (empty reason column) for the remaining findings. |
| <a id="entry-helper-cli-native-source-audit-command-smart-pointer-ast-argument-max-printed-c7519ee967"></a><code>helper-cli.native-source-audit.command.smart-pointer-ast.argument.max_printed</code> | <code>--max-printed</code> | <code>option</code> | нет | <code>1</code> | - | <code>50</code> | Maximum findings printed to stdout; 0 prints all (default: 50). |
| <a id="entry-helper-cli-native-source-audit-command-smart-pointer-ast-argument-engine-root-6ab07b2daf"></a><code>helper-cli.native-source-audit.command.smart-pointer-ast.argument.engine_root</code> | <code>--engine-root</code> | <code>option</code> | нет | <code>1</code> | - | - | Engine checkout whose Source rules and BuildTools/audit_baselines apply (default: the Engine containing this tool). |
| <a id="entry-helper-cli-native-source-audit-command-smart-pointer-ast-argument-compilation-database-275f0f7d31"></a><code>helper-cli.native-source-audit.command.smart-pointer-ast.argument.compilation_database</code> | <code>--compilation-database</code> | <code>option</code> | нет | <code>1</code> | - | - | Directory containing compile_commands.json (default: config compilation_database). |
| <a id="entry-helper-cli-native-source-audit-command-smart-pointer-ast-argument-clang-query-a5636d9a3a"></a><code>helper-cli.native-source-audit.command.smart-pointer-ast.argument.clang_query</code> | <code>--clang-query</code> | <code>option</code> | нет | <code>1</code> | - | - | clang-query executable (default: FO_CLANG_QUERY, then PATH). |
| <a id="entry-helper-cli-native-source-audit-command-smart-pointer-ast-argument-clang-tidy-0287724cf6"></a><code>helper-cli.native-source-audit.command.smart-pointer-ast.argument.clang_tidy</code> | <code>--clang-tidy</code> | <code>option</code> | нет | <code>1</code> | - | - | clang-tidy executable for use-after-move (default: FO_CLANG_TIDY, then PATH). |
| <a id="entry-helper-cli-native-source-audit-command-smart-pointer-ast-argument-require-tooling-fd897a294d"></a><code>helper-cli.native-source-audit.command.smart-pointer-ast.argument.require_tooling</code> | <code>--require-tooling</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | Fail with exit code 2 instead of skipping when clang tools or the compilation database are missing. |
| <a id="entry-helper-cli-native-source-audit-command-smart-pointer-ast-argument-source-pattern-321fd38859"></a><code>helper-cli.native-source-audit.command.smart-pointer-ast.argument.source_pattern</code> | <code>--source-pattern</code> | <code>option</code> | нет | <code>1</code> | - | - | clang-query isExpansionInFileMatching regex (default: derived from the scopes). |
| <a id="entry-helper-cli-native-source-audit-command-smart-pointer-ast-argument-unit-pattern-cee1272720"></a><code>helper-cli.native-source-audit.command.smart-pointer-ast.argument.unit_pattern</code> | <code>--unit-pattern</code> | <code>option</code> | нет | <code>1</code> | - | - | Only analyze compilation units whose display path matches this regex. |
| <a id="entry-helper-cli-native-source-audit-command-smart-pointer-ast-argument-jobs-57b1c95d62"></a><code>helper-cli.native-source-audit.command.smart-pointer-ast.argument.jobs</code> | <code>--jobs</code> | <code>option</code> | нет | <code>1</code> | - | <code>4</code> | Parallel clang processes (default: 4). |
| <a id="entry-helper-cli-native-source-audit-command-smart-pointer-ast-argument-batch-size-2c4770a500"></a><code>helper-cli.native-source-audit.command.smart-pointer-ast.argument.batch_size</code> | <code>--batch-size</code> | <code>option</code> | нет | <code>1</code> | - | <code>2</code> | Translation units per clang process (default: 2). |
| <a id="entry-helper-cli-native-source-audit-command-smart-pointer-ast-argument-timeout-c997a6ad5b"></a><code>helper-cli.native-source-audit.command.smart-pointer-ast.argument.timeout</code> | <code>--timeout</code> | <code>option</code> | нет | <code>1</code> | - | <code>900</code> | Seconds allowed per clang process (default: 900). |
| <a id="entry-helper-cli-native-source-audit-command-smart-pointer-ast-argument-diff-base-5535e3d5d0"></a><code>helper-cli.native-source-audit.command.smart-pointer-ast.argument.diff_base</code> | <code>--diff-base</code> | <code>option</code> | нет | <code>1</code> | - | - | Report only matches on lines added since this git revision. |
| <a id="entry-helper-cli-native-source-audit-command-smart-pointer-ast-argument-all-017217c67b"></a><code>helper-cli.native-source-audit.command.smart-pointer-ast.argument.all</code> | <code>--all</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | Report every match in the scopes instead of added lines only. |

#### Точный вывод `--help`

```text
usage: native_source_audit.py smart-pointer-ast [-h] [--root ROOT] [--scope SCOPE] [--exclude EXCLUDE] [--config CONFIG] [--baseline BASELINE] [--no-engine-baseline] [--report REPORT] [--emit-baseline EMIT_BASELINE]
                                                [--max-printed MAX_PRINTED] [--engine-root ENGINE_ROOT] [--compilation-database COMPILATION_DATABASE] [--clang-query CLANG_QUERY] [--clang-tidy CLANG_TIDY] [--require-tooling]
                                                [--source-pattern SOURCE_PATTERN] [--unit-pattern UNIT_PATTERN] [--jobs JOBS] [--batch-size BATCH_SIZE] [--timeout TIMEOUT] [--diff-base DIFF_BASE] [--all]

Run clang-query over a compilation database and report raw pointer declarations, fields, parameters and returns.

options:
  -h, --help            show this help message and exit
  --root ROOT           Root that scopes, excludes, reports and project baselines are relative to (default: current directory).
  --scope SCOPE         File or directory to audit, relative to --root; repeatable (default: config scopes, else the Engine Source directory).
  --exclude EXCLUDE     fnmatch pattern of root-relative paths to skip; a pattern ending in / skips a subtree; repeatable.
  --config CONFIG       JSON configuration with scopes, exclude patterns, baselines and audit options; relative to --root.
  --baseline BASELINE   Additional project baseline TSV with root-relative paths; repeatable.
  --no-engine-baseline  Do not apply BuildTools/audit_baselines/<audit>.tsv from the Engine.
  --report REPORT       Write remaining findings and stale baseline rows as TSV.
  --emit-baseline EMIT_BASELINE
                        Write candidate baseline rows (empty reason column) for the remaining findings.
  --max-printed MAX_PRINTED
                        Maximum findings printed to stdout; 0 prints all (default: 50).
  --engine-root ENGINE_ROOT
                        Engine checkout whose Source rules and BuildTools/audit_baselines apply (default: the Engine containing this tool).
  --compilation-database COMPILATION_DATABASE
                        Directory containing compile_commands.json (default: config compilation_database).
  --clang-query CLANG_QUERY
                        clang-query executable (default: FO_CLANG_QUERY, then PATH).
  --clang-tidy CLANG_TIDY
                        clang-tidy executable for use-after-move (default: FO_CLANG_TIDY, then PATH).
  --require-tooling     Fail with exit code 2 instead of skipping when clang tools or the compilation database are missing.
  --source-pattern SOURCE_PATTERN
                        clang-query isExpansionInFileMatching regex (default: derived from the scopes).
  --unit-pattern UNIT_PATTERN
                        Only analyze compilation units whose display path matches this regex.
  --jobs JOBS           Parallel clang processes (default: 4).
  --batch-size BATCH_SIZE
                        Translation units per clang process (default: 2).
  --timeout TIMEOUT     Seconds allowed per clang process (default: 900).
  --diff-base DIFF_BASE
                        Report only matches on lines added since this git revision.
  --all                 Report every match in the scopes instead of added lines only.
```

<a id="entry-helper-cli-native-source-audit-command-local-variables-55804e1b24"></a>
### `local-variables`

Find redundant top-level const locals (clang-query) and use-after-move (clang-tidy).

Stable ID: `helper-cli.native-source-audit.command.local-variables`

```text
usage: native_source_audit.py local-variables [-h] [--root ROOT] [--scope SCOPE] [--exclude EXCLUDE] [--config CONFIG] [--baseline BASELINE] [--no-engine-baseline] [--report REPORT] [--emit-baseline EMIT_BASELINE]
                                              [--max-printed MAX_PRINTED] [--engine-root ENGINE_ROOT] [--compilation-database COMPILATION_DATABASE] [--clang-query CLANG_QUERY] [--clang-tidy CLANG_TIDY] [--require-tooling]
                                              [--source-pattern SOURCE_PATTERN] [--unit-pattern UNIT_PATTERN] [--jobs JOBS] [--batch-size BATCH_SIZE] [--timeout TIMEOUT] [--checks {all,redundant-local-const,use-after-move}]
                                              [--mode {check,report,apply}]
```

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-native-source-audit-command-local-variables-argument-root-e9f15d4745"></a><code>helper-cli.native-source-audit.command.local-variables.argument.root</code> | <code>--root</code> | <code>option</code> | нет | <code>1</code> | - | - | Root that scopes, excludes, reports and project baselines are relative to (default: current directory). |
| <a id="entry-helper-cli-native-source-audit-command-local-variables-argument-scope-2360e0a319"></a><code>helper-cli.native-source-audit.command.local-variables.argument.scope</code> | <code>--scope</code> | <code>option</code> | нет | <code>1</code> | - | - | File or directory to audit, relative to --root; repeatable (default: config scopes, else the Engine Source directory). |
| <a id="entry-helper-cli-native-source-audit-command-local-variables-argument-exclude-88e7b5e761"></a><code>helper-cli.native-source-audit.command.local-variables.argument.exclude</code> | <code>--exclude</code> | <code>option</code> | нет | <code>1</code> | - | - | fnmatch pattern of root-relative paths to skip; a pattern ending in / skips a subtree; repeatable. |
| <a id="entry-helper-cli-native-source-audit-command-local-variables-argument-config-635bf74540"></a><code>helper-cli.native-source-audit.command.local-variables.argument.config</code> | <code>--config</code> | <code>option</code> | нет | <code>1</code> | - | - | JSON configuration with scopes, exclude patterns, baselines and audit options; relative to --root. |
| <a id="entry-helper-cli-native-source-audit-command-local-variables-argument-baseline-db1963dc9f"></a><code>helper-cli.native-source-audit.command.local-variables.argument.baseline</code> | <code>--baseline</code> | <code>option</code> | нет | <code>1</code> | - | - | Additional project baseline TSV with root-relative paths; repeatable. |
| <a id="entry-helper-cli-native-source-audit-command-local-variables-argument-no-engine-baseline-36631b1c23"></a><code>helper-cli.native-source-audit.command.local-variables.argument.no_engine_baseline</code> | <code>--no-engine-baseline</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | Do not apply BuildTools/audit_baselines/&lt;audit&gt;.tsv from the Engine. |
| <a id="entry-helper-cli-native-source-audit-command-local-variables-argument-report-11c8d43103"></a><code>helper-cli.native-source-audit.command.local-variables.argument.report</code> | <code>--report</code> | <code>option</code> | нет | <code>1</code> | - | - | Write remaining findings and stale baseline rows as TSV. |
| <a id="entry-helper-cli-native-source-audit-command-local-variables-argument-emit-baseline-9a7c7af585"></a><code>helper-cli.native-source-audit.command.local-variables.argument.emit_baseline</code> | <code>--emit-baseline</code> | <code>option</code> | нет | <code>1</code> | - | - | Write candidate baseline rows (empty reason column) for the remaining findings. |
| <a id="entry-helper-cli-native-source-audit-command-local-variables-argument-max-printed-9f68994ec7"></a><code>helper-cli.native-source-audit.command.local-variables.argument.max_printed</code> | <code>--max-printed</code> | <code>option</code> | нет | <code>1</code> | - | <code>50</code> | Maximum findings printed to stdout; 0 prints all (default: 50). |
| <a id="entry-helper-cli-native-source-audit-command-local-variables-argument-engine-root-6e36b23522"></a><code>helper-cli.native-source-audit.command.local-variables.argument.engine_root</code> | <code>--engine-root</code> | <code>option</code> | нет | <code>1</code> | - | - | Engine checkout whose Source rules and BuildTools/audit_baselines apply (default: the Engine containing this tool). |
| <a id="entry-helper-cli-native-source-audit-command-local-variables-argument-compilation-database-d207dadd7a"></a><code>helper-cli.native-source-audit.command.local-variables.argument.compilation_database</code> | <code>--compilation-database</code> | <code>option</code> | нет | <code>1</code> | - | - | Directory containing compile_commands.json (default: config compilation_database). |
| <a id="entry-helper-cli-native-source-audit-command-local-variables-argument-clang-query-cdea2cb1c5"></a><code>helper-cli.native-source-audit.command.local-variables.argument.clang_query</code> | <code>--clang-query</code> | <code>option</code> | нет | <code>1</code> | - | - | clang-query executable (default: FO_CLANG_QUERY, then PATH). |
| <a id="entry-helper-cli-native-source-audit-command-local-variables-argument-clang-tidy-2932561bf7"></a><code>helper-cli.native-source-audit.command.local-variables.argument.clang_tidy</code> | <code>--clang-tidy</code> | <code>option</code> | нет | <code>1</code> | - | - | clang-tidy executable for use-after-move (default: FO_CLANG_TIDY, then PATH). |
| <a id="entry-helper-cli-native-source-audit-command-local-variables-argument-require-tooling-5775a36475"></a><code>helper-cli.native-source-audit.command.local-variables.argument.require_tooling</code> | <code>--require-tooling</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | Fail with exit code 2 instead of skipping when clang tools or the compilation database are missing. |
| <a id="entry-helper-cli-native-source-audit-command-local-variables-argument-source-pattern-8ebec8e148"></a><code>helper-cli.native-source-audit.command.local-variables.argument.source_pattern</code> | <code>--source-pattern</code> | <code>option</code> | нет | <code>1</code> | - | - | clang-query isExpansionInFileMatching regex (default: derived from the scopes). |
| <a id="entry-helper-cli-native-source-audit-command-local-variables-argument-unit-pattern-8e072b4a30"></a><code>helper-cli.native-source-audit.command.local-variables.argument.unit_pattern</code> | <code>--unit-pattern</code> | <code>option</code> | нет | <code>1</code> | - | - | Only analyze compilation units whose display path matches this regex. |
| <a id="entry-helper-cli-native-source-audit-command-local-variables-argument-jobs-7d25d664a0"></a><code>helper-cli.native-source-audit.command.local-variables.argument.jobs</code> | <code>--jobs</code> | <code>option</code> | нет | <code>1</code> | - | <code>4</code> | Parallel clang processes (default: 4). |
| <a id="entry-helper-cli-native-source-audit-command-local-variables-argument-batch-size-b175a924ef"></a><code>helper-cli.native-source-audit.command.local-variables.argument.batch_size</code> | <code>--batch-size</code> | <code>option</code> | нет | <code>1</code> | - | <code>2</code> | Translation units per clang process (default: 2). |
| <a id="entry-helper-cli-native-source-audit-command-local-variables-argument-timeout-aff4d887c1"></a><code>helper-cli.native-source-audit.command.local-variables.argument.timeout</code> | <code>--timeout</code> | <code>option</code> | нет | <code>1</code> | - | <code>900</code> | Seconds allowed per clang process (default: 900). |
| <a id="entry-helper-cli-native-source-audit-command-local-variables-argument-checks-d4437d53fc"></a><code>helper-cli.native-source-audit.command.local-variables.argument.checks</code> | <code>--checks</code> | <code>option</code> | нет | <code>1</code> | <code>all</code>, <code>redundant-local-const</code>, <code>use-after-move</code> | <code>all</code> | Checks to run (default: all). |
| <a id="entry-helper-cli-native-source-audit-command-local-variables-argument-mode-4af34bc325"></a><code>helper-cli.native-source-audit.command.local-variables.argument.mode</code> | <code>--mode</code> | <code>option</code> | нет | <code>1</code> | <code>check</code>, <code>report</code>, <code>apply</code> | <code>check</code> | check fails on active findings; report never fails; apply removes redundant const (requires --checks redundant-local-const). |

#### Точный вывод `--help`

```text
usage: native_source_audit.py local-variables [-h] [--root ROOT] [--scope SCOPE] [--exclude EXCLUDE] [--config CONFIG] [--baseline BASELINE] [--no-engine-baseline] [--report REPORT] [--emit-baseline EMIT_BASELINE]
                                              [--max-printed MAX_PRINTED] [--engine-root ENGINE_ROOT] [--compilation-database COMPILATION_DATABASE] [--clang-query CLANG_QUERY] [--clang-tidy CLANG_TIDY] [--require-tooling]
                                              [--source-pattern SOURCE_PATTERN] [--unit-pattern UNIT_PATTERN] [--jobs JOBS] [--batch-size BATCH_SIZE] [--timeout TIMEOUT] [--checks {all,redundant-local-const,use-after-move}]
                                              [--mode {check,report,apply}]

Run clang-query and clang-tidy over a compilation database to find redundant top-level local const and use-after-move.

options:
  -h, --help            show this help message and exit
  --root ROOT           Root that scopes, excludes, reports and project baselines are relative to (default: current directory).
  --scope SCOPE         File or directory to audit, relative to --root; repeatable (default: config scopes, else the Engine Source directory).
  --exclude EXCLUDE     fnmatch pattern of root-relative paths to skip; a pattern ending in / skips a subtree; repeatable.
  --config CONFIG       JSON configuration with scopes, exclude patterns, baselines and audit options; relative to --root.
  --baseline BASELINE   Additional project baseline TSV with root-relative paths; repeatable.
  --no-engine-baseline  Do not apply BuildTools/audit_baselines/<audit>.tsv from the Engine.
  --report REPORT       Write remaining findings and stale baseline rows as TSV.
  --emit-baseline EMIT_BASELINE
                        Write candidate baseline rows (empty reason column) for the remaining findings.
  --max-printed MAX_PRINTED
                        Maximum findings printed to stdout; 0 prints all (default: 50).
  --engine-root ENGINE_ROOT
                        Engine checkout whose Source rules and BuildTools/audit_baselines apply (default: the Engine containing this tool).
  --compilation-database COMPILATION_DATABASE
                        Directory containing compile_commands.json (default: config compilation_database).
  --clang-query CLANG_QUERY
                        clang-query executable (default: FO_CLANG_QUERY, then PATH).
  --clang-tidy CLANG_TIDY
                        clang-tidy executable for use-after-move (default: FO_CLANG_TIDY, then PATH).
  --require-tooling     Fail with exit code 2 instead of skipping when clang tools or the compilation database are missing.
  --source-pattern SOURCE_PATTERN
                        clang-query isExpansionInFileMatching regex (default: derived from the scopes).
  --unit-pattern UNIT_PATTERN
                        Only analyze compilation units whose display path matches this regex.
  --jobs JOBS           Parallel clang processes (default: 4).
  --batch-size BATCH_SIZE
                        Translation units per clang process (default: 2).
  --timeout TIMEOUT     Seconds allowed per clang process (default: 900).
  --checks {all,redundant-local-const,use-after-move}
                        Checks to run (default: all).
  --mode {check,report,apply}
                        check fails on active findings; report never fails; apply removes redundant const (requires --checks redundant-local-const).
```

<a id="entry-helper-cli-native-source-audit-command-explicit-local-types-ce31af3e54"></a>
### `explicit-local-types`

Find auto locals whose deduced type is a simple explicit type (clang-query).

Stable ID: `helper-cli.native-source-audit.command.explicit-local-types`

```text
usage: native_source_audit.py explicit-local-types [-h] [--root ROOT] [--scope SCOPE] [--exclude EXCLUDE] [--config CONFIG] [--baseline BASELINE] [--no-engine-baseline] [--report REPORT] [--emit-baseline EMIT_BASELINE]
                                                   [--max-printed MAX_PRINTED] [--engine-root ENGINE_ROOT] [--compilation-database COMPILATION_DATABASE] [--clang-query CLANG_QUERY] [--clang-tidy CLANG_TIDY] [--require-tooling]
                                                   [--source-pattern SOURCE_PATTERN] [--unit-pattern UNIT_PATTERN] [--jobs JOBS] [--batch-size BATCH_SIZE] [--timeout TIMEOUT] [--mode {check,report,apply}] [--also-redundant-local-const]
                                                   [--json JSON] [--apply-report APPLY_REPORT]
```

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-native-source-audit-command-explicit-local-types-argument-root-369f3dee8f"></a><code>helper-cli.native-source-audit.command.explicit-local-types.argument.root</code> | <code>--root</code> | <code>option</code> | нет | <code>1</code> | - | - | Root that scopes, excludes, reports and project baselines are relative to (default: current directory). |
| <a id="entry-helper-cli-native-source-audit-command-explicit-local-types-argument-scope-2a0af532d0"></a><code>helper-cli.native-source-audit.command.explicit-local-types.argument.scope</code> | <code>--scope</code> | <code>option</code> | нет | <code>1</code> | - | - | File or directory to audit, relative to --root; repeatable (default: config scopes, else the Engine Source directory). |
| <a id="entry-helper-cli-native-source-audit-command-explicit-local-types-argument-exclude-c7ae1ce9b5"></a><code>helper-cli.native-source-audit.command.explicit-local-types.argument.exclude</code> | <code>--exclude</code> | <code>option</code> | нет | <code>1</code> | - | - | fnmatch pattern of root-relative paths to skip; a pattern ending in / skips a subtree; repeatable. |
| <a id="entry-helper-cli-native-source-audit-command-explicit-local-types-argument-config-652948dd1f"></a><code>helper-cli.native-source-audit.command.explicit-local-types.argument.config</code> | <code>--config</code> | <code>option</code> | нет | <code>1</code> | - | - | JSON configuration with scopes, exclude patterns, baselines and audit options; relative to --root. |
| <a id="entry-helper-cli-native-source-audit-command-explicit-local-types-argument-baseline-0e2606b2b1"></a><code>helper-cli.native-source-audit.command.explicit-local-types.argument.baseline</code> | <code>--baseline</code> | <code>option</code> | нет | <code>1</code> | - | - | Additional project baseline TSV with root-relative paths; repeatable. |
| <a id="entry-helper-cli-native-source-audit-command-explicit-local-types-argument-no-engine-baseline-9345df01ac"></a><code>helper-cli.native-source-audit.command.explicit-local-types.argument.no_engine_baseline</code> | <code>--no-engine-baseline</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | Do not apply BuildTools/audit_baselines/&lt;audit&gt;.tsv from the Engine. |
| <a id="entry-helper-cli-native-source-audit-command-explicit-local-types-argument-report-f05b60f2e2"></a><code>helper-cli.native-source-audit.command.explicit-local-types.argument.report</code> | <code>--report</code> | <code>option</code> | нет | <code>1</code> | - | - | Write remaining findings and stale baseline rows as TSV. |
| <a id="entry-helper-cli-native-source-audit-command-explicit-local-types-argument-emit-baseline-e5158c899a"></a><code>helper-cli.native-source-audit.command.explicit-local-types.argument.emit_baseline</code> | <code>--emit-baseline</code> | <code>option</code> | нет | <code>1</code> | - | - | Write candidate baseline rows (empty reason column) for the remaining findings. |
| <a id="entry-helper-cli-native-source-audit-command-explicit-local-types-argument-max-printed-6be8beaba4"></a><code>helper-cli.native-source-audit.command.explicit-local-types.argument.max_printed</code> | <code>--max-printed</code> | <code>option</code> | нет | <code>1</code> | - | <code>50</code> | Maximum findings printed to stdout; 0 prints all (default: 50). |
| <a id="entry-helper-cli-native-source-audit-command-explicit-local-types-argument-engine-root-fe32165f03"></a><code>helper-cli.native-source-audit.command.explicit-local-types.argument.engine_root</code> | <code>--engine-root</code> | <code>option</code> | нет | <code>1</code> | - | - | Engine checkout whose Source rules and BuildTools/audit_baselines apply (default: the Engine containing this tool). |
| <a id="entry-helper-cli-native-source-audit-command-explicit-local-types-argument-compilation-database-5cd6c9a80a"></a><code>helper-cli.native-source-audit.command.explicit-local-types.argument.compilation_database</code> | <code>--compilation-database</code> | <code>option</code> | нет | <code>1</code> | - | - | Directory containing compile_commands.json (default: config compilation_database). |
| <a id="entry-helper-cli-native-source-audit-command-explicit-local-types-argument-clang-query-b26471837d"></a><code>helper-cli.native-source-audit.command.explicit-local-types.argument.clang_query</code> | <code>--clang-query</code> | <code>option</code> | нет | <code>1</code> | - | - | clang-query executable (default: FO_CLANG_QUERY, then PATH). |
| <a id="entry-helper-cli-native-source-audit-command-explicit-local-types-argument-clang-tidy-842844fa57"></a><code>helper-cli.native-source-audit.command.explicit-local-types.argument.clang_tidy</code> | <code>--clang-tidy</code> | <code>option</code> | нет | <code>1</code> | - | - | clang-tidy executable for use-after-move (default: FO_CLANG_TIDY, then PATH). |
| <a id="entry-helper-cli-native-source-audit-command-explicit-local-types-argument-require-tooling-ed6e9af4ce"></a><code>helper-cli.native-source-audit.command.explicit-local-types.argument.require_tooling</code> | <code>--require-tooling</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | Fail with exit code 2 instead of skipping when clang tools or the compilation database are missing. |
| <a id="entry-helper-cli-native-source-audit-command-explicit-local-types-argument-source-pattern-227e6ed556"></a><code>helper-cli.native-source-audit.command.explicit-local-types.argument.source_pattern</code> | <code>--source-pattern</code> | <code>option</code> | нет | <code>1</code> | - | - | clang-query isExpansionInFileMatching regex (default: derived from the scopes). |
| <a id="entry-helper-cli-native-source-audit-command-explicit-local-types-argument-unit-pattern-6c260c2a0d"></a><code>helper-cli.native-source-audit.command.explicit-local-types.argument.unit_pattern</code> | <code>--unit-pattern</code> | <code>option</code> | нет | <code>1</code> | - | - | Only analyze compilation units whose display path matches this regex. |
| <a id="entry-helper-cli-native-source-audit-command-explicit-local-types-argument-jobs-f4f8a61b02"></a><code>helper-cli.native-source-audit.command.explicit-local-types.argument.jobs</code> | <code>--jobs</code> | <code>option</code> | нет | <code>1</code> | - | <code>4</code> | Parallel clang processes (default: 4). |
| <a id="entry-helper-cli-native-source-audit-command-explicit-local-types-argument-batch-size-d6c7bdfe90"></a><code>helper-cli.native-source-audit.command.explicit-local-types.argument.batch_size</code> | <code>--batch-size</code> | <code>option</code> | нет | <code>1</code> | - | <code>2</code> | Translation units per clang process (default: 2). |
| <a id="entry-helper-cli-native-source-audit-command-explicit-local-types-argument-timeout-85f2404c4b"></a><code>helper-cli.native-source-audit.command.explicit-local-types.argument.timeout</code> | <code>--timeout</code> | <code>option</code> | нет | <code>1</code> | - | <code>900</code> | Seconds allowed per clang process (default: 900). |
| <a id="entry-helper-cli-native-source-audit-command-explicit-local-types-argument-mode-620cef2533"></a><code>helper-cli.native-source-audit.command.explicit-local-types.argument.mode</code> | <code>--mode</code> | <code>option</code> | нет | <code>1</code> | <code>check</code>, <code>report</code>, <code>apply</code> | <code>check</code> | check fails on candidates; report never fails; apply rewrites the eligible auto tokens. |
| <a id="entry-helper-cli-native-source-audit-command-explicit-local-types-argument-also-redundant-local-const-13904bda78"></a><code>helper-cli.native-source-audit.command.explicit-local-types.argument.also_redundant_local_const</code> | <code>--also-redundant-local-const</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | Run the redundant-local-const matcher in the same AST pass. |
| <a id="entry-helper-cli-native-source-audit-command-explicit-local-types-argument-json-4b6cefcd40"></a><code>helper-cli.native-source-audit.command.explicit-local-types.argument.json</code> | <code>--json</code> | <code>option</code> | нет | <code>1</code> | - | - | Write the full candidate report as JSON. |
| <a id="entry-helper-cli-native-source-audit-command-explicit-local-types-argument-apply-report-d7e1a78868"></a><code>helper-cli.native-source-audit.command.explicit-local-types.argument.apply_report</code> | <code>--apply-report</code> | <code>option</code> | нет | <code>1</code> | - | - | Apply a previously reviewed JSON report without running clang-query (requires --mode apply). |

#### Точный вывод `--help`

```text
usage: native_source_audit.py explicit-local-types [-h] [--root ROOT] [--scope SCOPE] [--exclude EXCLUDE] [--config CONFIG] [--baseline BASELINE] [--no-engine-baseline] [--report REPORT] [--emit-baseline EMIT_BASELINE]
                                                   [--max-printed MAX_PRINTED] [--engine-root ENGINE_ROOT] [--compilation-database COMPILATION_DATABASE] [--clang-query CLANG_QUERY] [--clang-tidy CLANG_TIDY] [--require-tooling]
                                                   [--source-pattern SOURCE_PATTERN] [--unit-pattern UNIT_PATTERN] [--jobs JOBS] [--batch-size BATCH_SIZE] [--timeout TIMEOUT] [--mode {check,report,apply}] [--also-redundant-local-const]
                                                   [--json JSON] [--apply-report APPLY_REPORT]

Run clang-query over a compilation database and report auto locals that must spell their simple type explicitly.

options:
  -h, --help            show this help message and exit
  --root ROOT           Root that scopes, excludes, reports and project baselines are relative to (default: current directory).
  --scope SCOPE         File or directory to audit, relative to --root; repeatable (default: config scopes, else the Engine Source directory).
  --exclude EXCLUDE     fnmatch pattern of root-relative paths to skip; a pattern ending in / skips a subtree; repeatable.
  --config CONFIG       JSON configuration with scopes, exclude patterns, baselines and audit options; relative to --root.
  --baseline BASELINE   Additional project baseline TSV with root-relative paths; repeatable.
  --no-engine-baseline  Do not apply BuildTools/audit_baselines/<audit>.tsv from the Engine.
  --report REPORT       Write remaining findings and stale baseline rows as TSV.
  --emit-baseline EMIT_BASELINE
                        Write candidate baseline rows (empty reason column) for the remaining findings.
  --max-printed MAX_PRINTED
                        Maximum findings printed to stdout; 0 prints all (default: 50).
  --engine-root ENGINE_ROOT
                        Engine checkout whose Source rules and BuildTools/audit_baselines apply (default: the Engine containing this tool).
  --compilation-database COMPILATION_DATABASE
                        Directory containing compile_commands.json (default: config compilation_database).
  --clang-query CLANG_QUERY
                        clang-query executable (default: FO_CLANG_QUERY, then PATH).
  --clang-tidy CLANG_TIDY
                        clang-tidy executable for use-after-move (default: FO_CLANG_TIDY, then PATH).
  --require-tooling     Fail with exit code 2 instead of skipping when clang tools or the compilation database are missing.
  --source-pattern SOURCE_PATTERN
                        clang-query isExpansionInFileMatching regex (default: derived from the scopes).
  --unit-pattern UNIT_PATTERN
                        Only analyze compilation units whose display path matches this regex.
  --jobs JOBS           Parallel clang processes (default: 4).
  --batch-size BATCH_SIZE
                        Translation units per clang process (default: 2).
  --timeout TIMEOUT     Seconds allowed per clang process (default: 900).
  --mode {check,report,apply}
                        check fails on candidates; report never fails; apply rewrites the eligible auto tokens.
  --also-redundant-local-const
                        Run the redundant-local-const matcher in the same AST pass.
  --json JSON           Write the full candidate report as JSON.
  --apply-report APPLY_REPORT
                        Apply a previously reviewed JSON report without running clang-query (requires --mode apply).
```

<a id="entry-helper-cli-native-safety-audit-2f4598f1d2"></a>
## Native safety audits

Reject native code that changes entity cover behind server script exports and gate per-function exception-safety classifications of Engine and project sources against committed baselines.

- Стабильный ID: `helper-cli.native-safety-audit`
- Программа: `native_safety_audit.py`
- Владелец: `quality`
- Аудитория: `engine-contributor`, `game-developer`, `embedding-project-build-system`, `ci-maintainer`
- Владелец вызова: Engine validate workflow, embedding-project native validation tasks and CI
- Исходный парсер: [BuildTools/native_safety_audit.py](https://github.com/cvet/fonline/blob/master/BuildTools/native_safety_audit.py)

### Аргументы верхнего уровня

На этом уровне аргументов нет.

### Точный вывод `--help` верхнего уровня

```text
usage: native_safety_audit.py [-h] command ...

Audit Engine and project native sources against the entity-cover and exception-safety contracts.

positional arguments:
  command
    sync-scope        reject native code that obtains, replaces or releases entity cover for script exports
    exception-safety  check per-function exception-safety classifications against the baselines

options:
  -h, --help          show this help message and exit
```

<a id="entry-helper-cli-native-safety-audit-command-sync-scope-fd8f2cb3e2"></a>
### `sync-scope`

reject native code that obtains, replaces or releases entity cover for script exports

Stable ID: `helper-cli.native-safety-audit.command.sync-scope`

```text
usage: native_safety_audit.py sync-scope [-h] [--root ROOT] [--engine-root ENGINE_ROOT] [--scope DIR] [--config FILE] [--baseline FILE] [--json]
```

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-native-safety-audit-command-sync-scope-argument-root-ee86ae1201"></a><code>helper-cli.native-safety-audit.command.sync-scope.argument.root</code> | <code>--root</code> | <code>option</code> | нет | <code>1</code> | - | - | repository root for --scope, --config and --baseline paths (default: current directory) |
| <a id="entry-helper-cli-native-safety-audit-command-sync-scope-argument-engine-root-44d0761344"></a><code>helper-cli.native-safety-audit.command.sync-scope.argument.engine_root</code> | <code>--engine-root</code> | <code>option</code> | нет | <code>1</code> | - | - | Engine checkout to audit (default: the checkout that contains this script) |
| <a id="entry-helper-cli-native-safety-audit-command-sync-scope-argument-scope-510657e758"></a><code>helper-cli.native-safety-audit.command.sync-scope.argument.scope</code> | <code>--scope</code> | <code>option</code> | нет | <code>1</code> | - | - | root-relative project native directory audited with the Engine rules (repeatable) |
| <a id="entry-helper-cli-native-safety-audit-command-sync-scope-argument-config-aec59ba760"></a><code>helper-cli.native-safety-audit.command.sync-scope.argument.config</code> | <code>--config</code> | <code>option</code> | нет | <code>1</code> | - | - | project JSON configuration with vendored exclusions and reviewed project rules |
| <a id="entry-helper-cli-native-safety-audit-command-sync-scope-argument-baseline-1d3414d1ee"></a><code>helper-cli.native-safety-audit.command.sync-scope.argument.baseline</code> | <code>--baseline</code> | <code>option</code> | нет | <code>1</code> | - | - | project baseline TSV for findings in the --scope directories |
| <a id="entry-helper-cli-native-safety-audit-command-sync-scope-argument-json-5bed230967"></a><code>helper-cli.native-safety-audit.command.sync-scope.argument.json</code> | <code>--json</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | print the summary and diagnostics as JSON |

#### Точный вывод `--help`

```text
usage: native_safety_audit.py sync-scope [-h] [--root ROOT] [--engine-root ENGINE_ROOT] [--scope DIR] [--config FILE] [--baseline FILE] [--json]

options:
  -h, --help            show this help message and exit
  --root ROOT           repository root for --scope, --config and --baseline paths (default: current directory)
  --engine-root ENGINE_ROOT
                        Engine checkout to audit (default: the checkout that contains this script)
  --scope DIR           root-relative project native directory audited with the Engine rules (repeatable)
  --config FILE         project JSON configuration with vendored exclusions and reviewed project rules
  --baseline FILE       project baseline TSV for findings in the --scope directories
  --json                print the summary and diagnostics as JSON
```

<a id="entry-helper-cli-native-safety-audit-command-exception-safety-b885bae5b9"></a>
### `exception-safety`

check per-function exception-safety classifications against the baselines

Stable ID: `helper-cli.native-safety-audit.command.exception-safety`

```text
usage: native_safety_audit.py exception-safety [-h] [--root ROOT] [--engine-root ENGINE_ROOT] [--scope DIR] [--config FILE] [--baseline FILE] [--fail-on {error,warning}] [--level {NoThrow,Strong,Basic,None,Unclassified}] [--area AREA]
                                               [--file FILE_FILTER] [--function FUNCTION_FILTER] [--add-new] [--prune-removed] [--migrate-signatures] [--accept FILE::FUNCTION] [--accept-all-changed]
                                               [{audit,report,update}]
```

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-native-safety-audit-command-exception-safety-argument-root-5d7c6b896b"></a><code>helper-cli.native-safety-audit.command.exception-safety.argument.root</code> | <code>--root</code> | <code>option</code> | нет | <code>1</code> | - | - | repository root for --scope, --config and --baseline paths (default: current directory) |
| <a id="entry-helper-cli-native-safety-audit-command-exception-safety-argument-engine-root-083634bab3"></a><code>helper-cli.native-safety-audit.command.exception-safety.argument.engine_root</code> | <code>--engine-root</code> | <code>option</code> | нет | <code>1</code> | - | - | Engine checkout to audit (default: the checkout that contains this script) |
| <a id="entry-helper-cli-native-safety-audit-command-exception-safety-argument-scope-70c5a52bbc"></a><code>helper-cli.native-safety-audit.command.exception-safety.argument.scope</code> | <code>--scope</code> | <code>option</code> | нет | <code>1</code> | - | - | root-relative project native directory audited with the Engine rules (repeatable) |
| <a id="entry-helper-cli-native-safety-audit-command-exception-safety-argument-config-654e18e52f"></a><code>helper-cli.native-safety-audit.command.exception-safety.argument.config</code> | <code>--config</code> | <code>option</code> | нет | <code>1</code> | - | - | project JSON configuration with vendored exclusions and reviewed project rules |
| <a id="entry-helper-cli-native-safety-audit-command-exception-safety-argument-baseline-15731f3d0c"></a><code>helper-cli.native-safety-audit.command.exception-safety.argument.baseline</code> | <code>--baseline</code> | <code>option</code> | нет | <code>1</code> | - | - | project baseline TSV for findings in the --scope directories |
| <a id="entry-helper-cli-native-safety-audit-command-exception-safety-argument-action-fe490ae347"></a><code>helper-cli.native-safety-audit.command.exception-safety.argument.action</code> | <code>action</code> | <code>positional</code> | нет | <code>?</code> | <code>audit</code>, <code>report</code>, <code>update</code> | <code>audit</code> | audit (default) fails on drift; report summarizes the baselines; update maintains them |
| <a id="entry-helper-cli-native-safety-audit-command-exception-safety-argument-fail-on-7e82c37cb6"></a><code>helper-cli.native-safety-audit.command.exception-safety.argument.fail_on</code> | <code>--fail-on</code> | <code>option</code> | нет | <code>1</code> | <code>error</code>, <code>warning</code> | - | audit: also fail on warnings (pending None adjudication, review notes) |
| <a id="entry-helper-cli-native-safety-audit-command-exception-safety-argument-level-0401e86486"></a><code>helper-cli.native-safety-audit.command.exception-safety.argument.level</code> | <code>--level</code> | <code>option</code> | нет | <code>1</code> | <code>NoThrow</code>, <code>Strong</code>, <code>Basic</code>, <code>None</code>, <code>Unclassified</code> | - | report: list rows with this level |
| <a id="entry-helper-cli-native-safety-audit-command-exception-safety-argument-area-e417e6bb25"></a><code>helper-cli.native-safety-audit.command.exception-safety.argument.area</code> | <code>--area</code> | <code>option</code> | нет | <code>1</code> | - | - | report: list rows of this source area (second path component) |
| <a id="entry-helper-cli-native-safety-audit-command-exception-safety-argument-file-filter-b4b911f1b9"></a><code>helper-cli.native-safety-audit.command.exception-safety.argument.file_filter</code> | <code>--file</code> | <code>option</code> | нет | <code>1</code> | - | - | report: list rows whose file contains this text |
| <a id="entry-helper-cli-native-safety-audit-command-exception-safety-argument-function-filter-6fa689a4b9"></a><code>helper-cli.native-safety-audit.command.exception-safety.argument.function_filter</code> | <code>--function</code> | <code>option</code> | нет | <code>1</code> | - | - | report: list rows whose function contains this text |
| <a id="entry-helper-cli-native-safety-audit-command-exception-safety-argument-add-new-4c6e13395d"></a><code>helper-cli.native-safety-audit.command.exception-safety.argument.add_new</code> | <code>--add-new</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | update: add unclassified rows for new functions |
| <a id="entry-helper-cli-native-safety-audit-command-exception-safety-argument-prune-removed-9eddff4bd8"></a><code>helper-cli.native-safety-audit.command.exception-safety.argument.prune_removed</code> | <code>--prune-removed</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | update: remove rows whose function vanished |
| <a id="entry-helper-cli-native-safety-audit-command-exception-safety-argument-migrate-signatures-91bfff0832"></a><code>helper-cli.native-safety-audit.command.exception-safety.argument.migrate_signatures</code> | <code>--migrate-signatures</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | update: carry classifications across signature-only changes with identical body hashes |
| <a id="entry-helper-cli-native-safety-audit-command-exception-safety-argument-accept-e6ce13f96f"></a><code>helper-cli.native-safety-audit.command.exception-safety.argument.accept</code> | <code>--accept</code> | <code>option</code> | нет | <code>1</code> | - | - | update: refresh the body hash of a re-derived function (repeatable) |
| <a id="entry-helper-cli-native-safety-audit-command-exception-safety-argument-accept-all-changed-7220be3c80"></a><code>helper-cli.native-safety-audit.command.exception-safety.argument.accept_all_changed</code> | <code>--accept-all-changed</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | update: refresh every changed body hash |

#### Точный вывод `--help`

```text
usage: native_safety_audit.py exception-safety [-h] [--root ROOT] [--engine-root ENGINE_ROOT] [--scope DIR] [--config FILE] [--baseline FILE] [--fail-on {error,warning}] [--level {NoThrow,Strong,Basic,None,Unclassified}] [--area AREA]
                                               [--file FILE_FILTER] [--function FUNCTION_FILTER] [--add-new] [--prune-removed] [--migrate-signatures] [--accept FILE::FUNCTION] [--accept-all-changed]
                                               [{audit,report,update}]

positional arguments:
  {audit,report,update}
                        audit (default) fails on drift; report summarizes the baselines; update maintains them

options:
  -h, --help            show this help message and exit
  --root ROOT           repository root for --scope, --config and --baseline paths (default: current directory)
  --engine-root ENGINE_ROOT
                        Engine checkout to audit (default: the checkout that contains this script)
  --scope DIR           root-relative project native directory audited with the Engine rules (repeatable)
  --config FILE         project JSON configuration with vendored exclusions and reviewed project rules
  --baseline FILE       project baseline TSV for findings in the --scope directories
  --fail-on {error,warning}
                        audit: also fail on warnings (pending None adjudication, review notes)
  --level {NoThrow,Strong,Basic,None,Unclassified}
                        report: list rows with this level
  --area AREA           report: list rows of this source area (second path component)
  --file FILE_FILTER    report: list rows whose file contains this text
  --function FUNCTION_FILTER
                        report: list rows whose function contains this text
  --add-new             update: add unclassified rows for new functions
  --prune-removed       update: remove rows whose function vanished
  --migrate-signatures  update: carry classifications across signature-only changes with identical body hashes
  --accept FILE::FUNCTION
                        update: refresh the body hash of a re-derived function (repeatable)
  --accept-all-changed  update: refresh every changed body hash
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

<a id="entry-helper-cli-artifact-checksums-efe1b1aaa6"></a>
## Контрольные суммы артефактов

Записывает или проверяет sidecar-файлы SHA-256 и необязательный манифест для готовых релизных пакетов.

- Стабильный ID: `helper-cli.artifact-checksums`
- Программа: `artifact_checksums.py`
- Владелец: `build-release`
- Аудитория: `release-operator`, `ci-maintainer`, `embedding-project-build-system`
- Владелец вызова: релизные задачи подключающего проекта после остановки последнего записывающего пакеты процесса
- Исходный парсер: [BuildTools/artifact_checksums.py](https://github.com/cvet/fonline/blob/master/BuildTools/artifact_checksums.py)

### Аргументы верхнего уровня

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-artifact-checksums-argument-directory-dbf51c94e9"></a><code>helper-cli.artifact-checksums.argument.directory</code> | <code>directory</code> | <code>positional</code> | да | <code>1</code> | - | - | Каталог готового пакета до публикации. |
| <a id="entry-helper-cli-artifact-checksums-argument-artifact-3dd87d0f1b"></a><code>helper-cli.artifact-checksums.argument.artifact</code> | <code>--artifact</code> | <code>option</code> | нет | <code>1</code> | - | - | Выбрать этот файл, заданный относительно каталога, вместо просмотра корня каталога; можно повторять. |
| <a id="entry-helper-cli-artifact-checksums-argument-suffix-3303b5534f"></a><code>helper-cli.artifact-checksums.argument.suffix</code> | <code>--suffix</code> | <code>option</code> | нет | <code>1</code> | - | - | Суффикс имени файла для выбора без учёта регистра; можно повторять; заменяет набор по умолчанию .zip, .tar, .tar.gz, .msi и .apk. |
| <a id="entry-helper-cli-artifact-checksums-argument-manifest-4e1036138a"></a><code>helper-cli.artifact-checksums.argument.manifest</code> | <code>--manifest</code> | <code>option</code> | нет | <code>1</code> | - | - | Также записать (или проверить) один манифест всех выбранных файлов в корне каталога, например SHA256SUMS. |
| <a id="entry-helper-cli-artifact-checksums-argument-no-sidecars-bd7146afb3"></a><code>helper-cli.artifact-checksums.argument.no_sidecars</code> | <code>--no-sidecars</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | Записывать или проверять только манифест; требует --manifest. |
| <a id="entry-helper-cli-artifact-checksums-argument-verify-7eee2f243b"></a><code>helper-cli.artifact-checksums.argument.verify</code> | <code>--verify</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | Сравнить существующие sidecar-файлы и манифест с текущими байтами файлов вместо их записи. |

### Точный вывод `--help` верхнего уровня

```text
usage: artifact_checksums.py [-h] [--artifact RELATIVE_PATH] [--suffix SUFFIX] [--manifest FILENAME] [--no-sidecars] [--verify] directory

Write or verify SHA-256 sidecars and an optional manifest for finished release packages.

positional arguments:
  directory             finished package directory, before publication

options:
  -h, --help            show this help message and exit
  --artifact RELATIVE_PATH
                        select this file, relative to the directory, instead of scanning the directory root; repeatable
  --suffix SUFFIX       case-insensitive file-name suffix to select; repeatable; replaces the default .zip, .tar, .tar.gz, .msi and .apk
  --manifest FILENAME   also write (or verify) one manifest of every selected file at the directory root, for example SHA256SUMS
  --no-sidecars         write or verify only the manifest; requires --manifest
  --verify              compare existing sidecars and manifest with the current file bytes instead of writing them
```

<a id="entry-helper-cli-project-version-52ce8849a0"></a>
## Генератор версии проекта

Создаёт или проверяет записываемый машиной монотонный файл VERSION проекта по встроенной или проектной схеме версий.

- Стабильный ID: `helper-cli.project-version`
- Программа: `project_version.py`
- Владелец: `build-release`
- Аудитория: `game-developer`, `ci-maintainer`, `embedding-project-build-system`
- Владелец вызова: задачи версии, hooks и проверки CI подключающего проекта
- Исходный парсер: [BuildTools/project_version.py](https://github.com/cvet/fonline/blob/master/BuildTools/project_version.py)

### Аргументы верхнего уровня

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-project-version-argument-root-e3201c76c3"></a><code>helper-cli.project-version.argument.root</code> | <code>--root</code> | <code>option</code> | нет | <code>1</code> | - | <code>.</code> | Каталог внутри рабочего дерева проекта (по умолчанию текущий каталог). |
| <a id="entry-helper-cli-project-version-argument-config-2c1e6b62ba"></a><code>helper-cli.project-version.argument.config</code> | <code>--config</code> | <code>option</code> | нет | <code>1</code> | - | - | JSON-файл с любыми из ключей file, base_ref, scheme, limits, since_commit; параметры командной строки имеют приоритет. |
| <a id="entry-helper-cli-project-version-argument-file-ae457daa3a"></a><code>helper-cli.project-version.argument.file</code> | <code>--file</code> | <code>option</code> | нет | <code>1</code> | - | - | Файл версии относительно --root (по умолчанию VERSION). |
| <a id="entry-helper-cli-project-version-argument-base-ref-6e2264ba65"></a><code>helper-cli.project-version.argument.base_ref</code> | <code>--base-ref</code> | <code>option</code> | нет | <code>1</code> | - | - | Git ref, версию которого должно превышать следующее значение, например origin/main; обязателен для создания. |
| <a id="entry-helper-cli-project-version-argument-scheme-a69912c938"></a><code>helper-cli.project-version.argument.scheme</code> | <code>--scheme</code> | <code>option</code> | нет | <code>1</code> | - | - | Встроенная схема patch-per-commit (по умолчанию) или counter либо &lt;file.py or module&gt;:&lt;factory&gt; проектной схемы. |
| <a id="entry-helper-cli-project-version-argument-limit-945d934d1d"></a><code>helper-cli.project-version.argument.limit</code> | <code>--limit</code> | <code>option</code> | нет | <code>1</code> | - | - | Исключающая верхняя граница одного компонента схемы, например patch=10000; заполненный компонент переносится в предыдущий; можно повторять. |
| <a id="entry-helper-cli-project-version-argument-since-commit-637b3f1b85"></a><code>helper-cli.project-version.argument.since_commit</code> | <code>--since-commit</code> | <code>option</code> | нет | <code>1</code> | - | - | Игнорировать значения, зафиксированные до этого коммита, например до смены кодирования; он должен быть предком каждого сравниваемого ref. |
| <a id="entry-helper-cli-project-version-argument-check-2421a0cc6a"></a><code>helper-cli.project-version.argument.check</code> | <code>--check</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | Проверить файл рабочего дерева, не записывая его. |

### Точный вывод `--help` верхнего уровня

```text
usage: project_version.py [-h] [--root ROOT] [--config CONFIG] [--file FILE] [--base-ref BASE_REF] [--scheme SCHEME] [--limit COMPONENT=N] [--since-commit SINCE_COMMIT] [--check]

Generate or check a machine-written monotonic project VERSION file against Git history.

options:
  -h, --help            show this help message and exit
  --root ROOT           directory inside the project work tree (default: current directory)
  --config CONFIG       JSON file with any of file, base_ref, scheme, limits, since_commit; options given on the command line win
  --file FILE           version file relative to --root (default: VERSION)
  --base-ref BASE_REF   Git ref whose version the next value must exceed, for example origin/main; required to generate
  --scheme SCHEME       built-in patch-per-commit (default) or counter, or <file.py or module>:<factory> of a project scheme
  --limit COMPONENT=N   exclusive upper bound of one scheme component, for example patch=10000; a full component carries into the previous one; repeatable
  --since-commit SINCE_COMMIT
                        ignore values committed before this commit, for example where the encoding changed; it must be an ancestor of every compared ref
  --check               validate the working-tree file without writing it
```

<a id="entry-helper-cli-release-preflight-f5192a7f8b"></a>
## Предварительная проверка релиза

Проверяет версию, тег и ассеты релиза проекта, выбирает публикацию или dry-run, делит слишком большие ассеты и записывает манифест контрольных сумм.

- Стабильный ID: `helper-cli.release-preflight`
- Программа: `release_preflight.py`
- Владелец: `build-release`
- Аудитория: `release-operator`, `ci-maintainer`
- Владелец вызова: ручные релизные задачи подключающего проекта перед созданием тега и публикацией
- Исходный парсер: [BuildTools/release_preflight.py](https://github.com/cvet/fonline/blob/master/BuildTools/release_preflight.py)

### Аргументы верхнего уровня

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-release-preflight-argument-root-3bdae0a691"></a><code>helper-cli.release-preflight.argument.root</code> | <code>--root</code> | <code>option</code> | нет | <code>1</code> | - | <code>.</code> | Каталог внутри рабочего дерева проекта (по умолчанию текущий каталог). |
| <a id="entry-helper-cli-release-preflight-argument-config-7795ea0c55"></a><code>helper-cli.release-preflight.argument.config</code> | <code>--config</code> | <code>option</code> | нет | <code>1</code> | - | - | JSON-конфигурация project_version.py; её file, scheme и limits действуют и здесь. |
| <a id="entry-helper-cli-release-preflight-argument-file-09ca21cfb3"></a><code>helper-cli.release-preflight.argument.file</code> | <code>--file</code> | <code>option</code> | нет | <code>1</code> | - | - | Файл версии относительно --root (по умолчанию VERSION). |
| <a id="entry-helper-cli-release-preflight-argument-scheme-00d87d25f5"></a><code>helper-cli.release-preflight.argument.scheme</code> | <code>--scheme</code> | <code>option</code> | нет | <code>1</code> | - | - | Схема версий, как для project_version.py (по умолчанию patch-per-commit). |
| <a id="entry-helper-cli-release-preflight-argument-limit-947859499f"></a><code>helper-cli.release-preflight.argument.limit</code> | <code>--limit</code> | <code>option</code> | нет | <code>1</code> | - | - | Граница компонента схемы, как для project_version.py; можно повторять. |
| <a id="entry-helper-cli-release-preflight-argument-tag-prefix-c323e1731a"></a><code>helper-cli.release-preflight.argument.tag_prefix</code> | <code>--tag-prefix</code> | <code>option</code> | нет | <code>1</code> | - | <code>v</code> | Префикс релизного тега перед версией (по умолчанию v). |
| <a id="entry-helper-cli-release-preflight-argument-remote-f7eaa2dcb3"></a><code>helper-cli.release-preflight.argument.remote</code> | <code>--remote</code> | <code>option</code> | нет | <code>1</code> | - | - | Также искать тег в этом удалённом репозитории Git, например origin. |
| <a id="entry-helper-cli-release-preflight-argument-ref-19af983c12"></a><code>helper-cli.release-preflight.argument.ref</code> | <code>--ref</code> | <code>option</code> | нет | <code>1</code> | - | - | Выпускаемый ref, например refs/heads/main (по умолчанию ветка, извлечённая в HEAD). |
| <a id="entry-helper-cli-release-preflight-argument-release-ref-d1273200a8"></a><code>helper-cli.release-preflight.argument.release_ref</code> | <code>--release-ref</code> | <code>option</code> | нет | <code>1</code> | - | - | Публикует только этот ref; любой другой ref даёт dry-run, а без параметра каждый запуск является dry-run. |
| <a id="entry-helper-cli-release-preflight-argument-dry-run-2b5fa5e478"></a><code>helper-cli.release-preflight.argument.dry_run</code> | <code>--dry-run</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | Принудительно включить режим dry-run даже на релизном ref. |
| <a id="entry-helper-cli-release-preflight-argument-engine-9472b02da4"></a><code>helper-cli.release-preflight.argument.engine</code> | <code>--engine</code> | <code>option</code> | нет | <code>1</code> | - | - | Также требовать, чтобы зафиксированный gitlink этого submodule Engine был достижим из remote-tracking-ветки submodule. |
| <a id="entry-helper-cli-release-preflight-argument-assets-dir-14259b81bb"></a><code>helper-cli.release-preflight.argument.assets_dir</code> | <code>--assets-dir</code> | <code>option</code> | нет | <code>1</code> | - | - | Каталог, файлы корня которого являются релизными ассетами. |
| <a id="entry-helper-cli-release-preflight-argument-asset-pattern-dc2cf1b2ab"></a><code>helper-cli.release-preflight.argument.asset_pattern</code> | <code>--asset-pattern</code> | <code>option</code> | нет | <code>1</code> | - | - | Шаблон имени файла ассетов в --assets-dir; можно повторять (по умолчанию каждый файл). |
| <a id="entry-helper-cli-release-preflight-argument-max-asset-bytes-88de42eb91"></a><code>helper-cli.release-preflight.argument.max_asset_bytes</code> | <code>--max-asset-bytes</code> | <code>option</code> | нет | <code>1</code> | - | - | Делить каждый ассет большего размера на части &lt;name&gt;.partNN не больше SIZE байт; суффиксы K, M и G означают степени 1024. |
| <a id="entry-helper-cli-release-preflight-argument-manifest-66e8bb1588"></a><code>helper-cli.release-preflight.argument.manifest</code> | <code>--manifest</code> | <code>option</code> | нет | <code>1</code> | - | - | Записать манифест SHA-256 итоговых ассетов в --assets-dir. |
| <a id="entry-helper-cli-release-preflight-argument-outputs-file-0239d782b6"></a><code>helper-cli.release-preflight.argument.outputs_file</code> | <code>--outputs-file</code> | <code>option</code> | нет | <code>1</code> | - | - | Дописать version, tag, previous_tag, publish и assets строками key=value, например в $GITHUB_OUTPUT. |
| <a id="entry-helper-cli-release-preflight-argument-summary-file-74fca7a46a"></a><code>helper-cli.release-preflight.argument.summary_file</code> | <code>--summary-file</code> | <code>option</code> | нет | <code>1</code> | - | - | Дописать Markdown-сводку, например в $GITHUB_STEP_SUMMARY. |

### Точный вывод `--help` верхнего уровня

```text
usage: release_preflight.py [-h] [--root ROOT] [--config CONFIG] [--file FILE] [--scheme SCHEME] [--limit COMPONENT=N] [--tag-prefix TAG_PREFIX] [--remote REMOTE] [--ref REF] [--release-ref RELEASE_REF] [--dry-run] [--engine PATH]
                            [--assets-dir ASSETS_DIR] [--asset-pattern GLOB] [--max-asset-bytes SIZE] [--manifest FILENAME] [--outputs-file OUTPUTS_FILE] [--summary-file SUMMARY_FILE]

Validate the version, tag and assets of a project release and decide between publish and dry-run.

options:
  -h, --help            show this help message and exit
  --root ROOT           directory inside the project work tree (default: current directory)
  --config CONFIG       project_version.py JSON config; its file, scheme and limits apply here
  --file FILE           version file relative to --root (default: VERSION)
  --scheme SCHEME       version scheme, as for project_version.py (default: patch-per-commit)
  --limit COMPONENT=N   scheme component limit, as for project_version.py; repeatable
  --tag-prefix TAG_PREFIX
                        release tag prefix before the version (default: v)
  --remote REMOTE       also look for the tag on this Git remote, for example origin
  --ref REF             ref being released, for example refs/heads/main (default: the branch checked out at HEAD)
  --release-ref RELEASE_REF
                        only this ref publishes; every other ref is a dry run, and without it every run is a dry run
  --dry-run             force dry-run mode even on the release ref
  --engine PATH         also require the committed gitlink of this Engine submodule to be reachable from a remote-tracking branch of the submodule
  --assets-dir ASSETS_DIR
                        directory whose root-level files are the release assets
  --asset-pattern GLOB  file-name pattern of the assets in --assets-dir; repeatable (default: every file)
  --max-asset-bytes SIZE
                        split each larger asset into <name>.partNN pieces of at most SIZE bytes; K, M and G suffixes are powers of 1024
  --manifest FILENAME   write a SHA-256 manifest of the final assets into --assets-dir
  --outputs-file OUTPUTS_FILE
                        append version, tag, previous_tag, publish and assets as key=value lines, for example to $GITHUB_OUTPUT
  --summary-file SUMMARY_FILE
                        append a Markdown summary, for example to $GITHUB_STEP_SUMMARY
```

<a id="entry-helper-cli-engine-upgrade-report-a1902fe984"></a>
## Отчёт об обновлении Engine

Записывает Markdown-запись обновления Engine с идентичностями Engine, входящим диапазоном, миграциями из истории изменений и поиском имён в проекте.

- Стабильный ID: `helper-cli.engine-upgrade-report`
- Программа: `engine_upgrade_report.py`
- Владелец: `documentation`
- Аудитория: `game-developer`, `engine-contributor`
- Владелец вызова: работа подключающего проекта по обновлению Engine, запуск из корня проекта
- Исходный парсер: [BuildTools/engine_upgrade_report.py](https://github.com/cvet/fonline/blob/master/BuildTools/engine_upgrade_report.py)

### Аргументы верхнего уровня

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-engine-upgrade-report-argument-project-9e74f0315a"></a><code>helper-cli.engine-upgrade-report.argument.project</code> | <code>--project</code> | <code>option</code> | нет | <code>1</code> | - | <code>.</code> | Рабочее дерево подключающего проекта (по умолчанию текущий каталог). |
| <a id="entry-helper-cli-engine-upgrade-report-argument-engine-b448f3e919"></a><code>helper-cli.engine-upgrade-report.argument.engine</code> | <code>--engine</code> | <code>option</code> | нет | <code>1</code> | - | <code>Engine</code> | Checkout Engine относительно --project (по умолчанию Engine). |
| <a id="entry-helper-cli-engine-upgrade-report-argument-old-ab375d7aa4"></a><code>helper-cli.engine-upgrade-report.argument.old</code> | <code>--old</code> | <code>option</code> | нет | <code>1</code> | - | - | Базовая ревизия Engine (по умолчанию родительский gitlink в HEAD). |
| <a id="entry-helper-cli-engine-upgrade-report-argument-new-8b113dcac3"></a><code>helper-cli.engine-upgrade-report.argument.new</code> | <code>--new</code> | <code>option</code> | нет | <code>1</code> | - | - | Целевая ревизия Engine, например коммит или тег v&lt;version&gt; (по умолчанию HEAD checkout Engine). |
| <a id="entry-helper-cli-engine-upgrade-report-argument-changelog-5ffef85197"></a><code>helper-cli.engine-upgrade-report.argument.changelog</code> | <code>--changelog</code> | <code>option</code> | нет | <code>1</code> | - | <code>Docs/en/reference/changelog.md</code> | Путь к истории изменений внутри репозитория Engine, читается на целевой ревизии (по умолчанию Docs/en/reference/changelog.md). |
| <a id="entry-helper-cli-engine-upgrade-report-argument-names-fcde25bd3f"></a><code>helper-cli.engine-upgrade-report.argument.names</code> | <code>--names</code> | <code>option</code> | нет | <code>1</code> | - | - | Текстовый файл с одним буквальным именем на строку для поиска в исходниках проекта; пустые строки пропускаются. |
| <a id="entry-helper-cli-engine-upgrade-report-argument-search-path-5e221d686e"></a><code>helper-cli.engine-upgrade-report.argument.search_path</code> | <code>--search-path</code> | <code>option</code> | нет | <code>1</code> | - | - | Pathspec проекта для поиска имён; можно повторять (по умолчанию весь проект без checkout Engine). |
| <a id="entry-helper-cli-engine-upgrade-report-argument-max-locations-011a38ebcd"></a><code>helper-cli.engine-upgrade-report.argument.max_locations</code> | <code>--max-locations</code> | <code>option</code> | нет | <code>1</code> | - | <code>20</code> | Число мест, перечисляемых для каждого имени в записи; число совпадений всегда полное (по умолчанию 20). |
| <a id="entry-helper-cli-engine-upgrade-report-argument-output-da11040904"></a><code>helper-cli.engine-upgrade-report.argument.output</code> | <code>--output</code> | <code>option</code> | нет | <code>1</code> | - | - | Записать запись в этот файл вместо стандартного вывода. |
| <a id="entry-helper-cli-engine-upgrade-report-argument-force-1707183875"></a><code>helper-cli.engine-upgrade-report.argument.force</code> | <code>--force</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | Заменить существующий файл --output. |

### Точный вывод `--help` верхнего уровня

```text
usage: engine_upgrade_report.py [-h] [--project PROJECT] [--engine ENGINE] [--old OLD] [--new NEW] [--changelog CHANGELOG] [--names NAMES] [--search-path PATHSPEC] [--max-locations MAX_LOCATIONS] [--output OUTPUT] [--force]

Write a Markdown Engine update record for an embedding project from read-only Git queries.

options:
  -h, --help            show this help message and exit
  --project PROJECT     embedding project work tree (default: current directory)
  --engine ENGINE       Engine checkout relative to --project (default: Engine)
  --old OLD             baseline Engine revision (default: the parent gitlink at HEAD)
  --new NEW             target Engine revision, for example a commit or a v<version> tag (default: the Engine checkout HEAD)
  --changelog CHANGELOG
                        changelog path inside the Engine repository, read at the target revision (default: Docs/en/reference/changelog.md)
  --names NAMES         text file with one literal name per line to search in the project sources; blank lines are ignored
  --search-path PATHSPEC
                        project pathspec to search for the names; repeatable (default: the whole project except the Engine checkout)
  --max-locations MAX_LOCATIONS
                        locations listed per name in the record; the match count is always complete (default: 20)
  --output OUTPUT       write the record to this file instead of standard output
  --force               replace an existing --output file
```

<a id="entry-helper-cli-script-binding-check-56e942e304"></a>
## Script binding check

Reject raw pointers in script-facing native signatures and compare AngelScript handler nullability with event and remote-call declarations.

- Стабильный ID: `helper-cli.script-binding-check`
- Программа: `check_script_bindings.py`
- Владелец: `quality`
- Аудитория: `game-developer`, `engine-contributor`, `ci-maintainer`
- Владелец вызова: Engine CI and embedding-project quality gates
- Исходный парсер: [BuildTools/check_script_bindings.py](https://github.com/cvet/fonline/blob/master/BuildTools/check_script_bindings.py)

### Аргументы верхнего уровня

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-script-binding-check-argument-root-0186131671"></a><code>helper-cli.script-binding-check.argument.root</code> | <code>--root</code> | <code>option</code> | нет | <code>1</code> | - | - | repository root that scopes and reported paths are relative to (default: the Engine root that contains this script) |
| <a id="entry-helper-cli-script-binding-check-argument-scope-ce0f036e90"></a><code>helper-cli.script-binding-check.argument.scope</code> | <code>--scope</code> | <code>option</code> | нет | <code>1</code> | - | - | root-relative native source directory; repeatable (default: Source) |
| <a id="entry-helper-cli-script-binding-check-argument-scripts-be2566252b"></a><code>helper-cli.script-binding-check.argument.scripts</code> | <code>--scripts</code> | <code>option</code> | нет | <code>1</code> | - | - | root-relative AngelScript directory whose handlers are compared; repeatable |
| <a id="entry-helper-cli-script-binding-check-argument-exclude-81d76422a0"></a><code>helper-cli.script-binding-check.argument.exclude</code> | <code>--exclude</code> | <code>option</code> | нет | <code>1</code> | - | - | fnmatch pattern for root-relative POSIX paths to skip; repeatable |
| <a id="entry-helper-cli-script-binding-check-argument-baseline-0b842d512b"></a><code>helper-cli.script-binding-check.argument.baseline</code> | <code>--baseline</code> | <code>option</code> | нет | <code>1</code> | - | - | accepted findings (rule, path, count, reason TSV); repeatable |
| <a id="entry-helper-cli-script-binding-check-argument-write-baseline-d4d708b0d6"></a><code>helper-cli.script-binding-check.argument.write_baseline</code> | <code>--write-baseline</code> | <code>option</code> | нет | <code>1</code> | - | - | write current finding counts with empty reasons instead of gating |
| <a id="entry-helper-cli-script-binding-check-argument-list-rules-8205fa771d"></a><code>helper-cli.script-binding-check.argument.list_rules</code> | <code>--list-rules</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | print rule identifiers and exit |

### Точный вывод `--help` верхнего уровня

```text
usage: check_script_bindings.py [-h] [--root ROOT] [--scope DIR] [--scripts DIR] [--exclude GLOB] [--baseline TSV] [--write-baseline TSV] [--list-rules]

Check script-facing native signatures for ptr/nptr spelling and compare AngelScript handlers with event and remote-call declarations

options:
  -h, --help            show this help message and exit
  --root ROOT           repository root that scopes and reported paths are relative to (default: the Engine root that contains this script)
  --scope DIR           root-relative native source directory; repeatable (default: Source)
  --scripts DIR         root-relative AngelScript directory whose handlers are compared; repeatable
  --exclude GLOB        fnmatch pattern for root-relative POSIX paths to skip; repeatable
  --baseline TSV        accepted findings (rule, path, count, reason TSV); repeatable
  --write-baseline TSV  write current finding counts with empty reasons instead of gating
  --list-rules          print rule identifiers and exit
```

<a id="entry-helper-cli-angelscript-hygiene-check-b5053583a9"></a>
## AngelScript hygiene check

Check authored AngelScript modules for namespace, include, side-macro, sort-directive and file-ending conventions with a count-ratchet baseline.

- Стабильный ID: `helper-cli.angelscript-hygiene-check`
- Программа: `check_angelscript_hygiene.py`
- Владелец: `quality`
- Аудитория: `game-developer`, `engine-contributor`, `ci-maintainer`
- Владелец вызова: Engine CI and embedding-project quality gates
- Исходный парсер: [BuildTools/check_angelscript_hygiene.py](https://github.com/cvet/fonline/blob/master/BuildTools/check_angelscript_hygiene.py)

### Аргументы верхнего уровня

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-angelscript-hygiene-check-argument-root-62e4bfdc20"></a><code>helper-cli.angelscript-hygiene-check.argument.root</code> | <code>--root</code> | <code>option</code> | нет | <code>1</code> | - | - | repository root that script directories and reported paths are relative to (default: the Engine root that contains this script) |
| <a id="entry-helper-cli-angelscript-hygiene-check-argument-scripts-3af5b503fb"></a><code>helper-cli.angelscript-hygiene-check.argument.scripts</code> | <code>--scripts</code> | <code>option</code> | нет | <code>1</code> | - | - | root-relative directory searched recursively for .fos files; repeatable (default: Examples) |
| <a id="entry-helper-cli-angelscript-hygiene-check-argument-config-0f43c6ee5e"></a><code>helper-cli.angelscript-hygiene-check.argument.config</code> | <code>--config</code> | <code>option</code> | нет | <code>1</code> | - | - | rule switches, rule options and excludes (schema_version 1) |
| <a id="entry-helper-cli-angelscript-hygiene-check-argument-exclude-6adf1dbcfb"></a><code>helper-cli.angelscript-hygiene-check.argument.exclude</code> | <code>--exclude</code> | <code>option</code> | нет | <code>1</code> | - | - | fnmatch pattern for root-relative POSIX paths to skip; repeatable |
| <a id="entry-helper-cli-angelscript-hygiene-check-argument-baseline-1036f14d59"></a><code>helper-cli.angelscript-hygiene-check.argument.baseline</code> | <code>--baseline</code> | <code>option</code> | нет | <code>1</code> | - | - | accepted findings (rule, path, count, reason TSV); repeatable |
| <a id="entry-helper-cli-angelscript-hygiene-check-argument-write-baseline-504af0139a"></a><code>helper-cli.angelscript-hygiene-check.argument.write_baseline</code> | <code>--write-baseline</code> | <code>option</code> | нет | <code>1</code> | - | - | write current finding counts with empty reasons instead of gating |
| <a id="entry-helper-cli-angelscript-hygiene-check-argument-list-rules-e0fb8a073d"></a><code>helper-cli.angelscript-hygiene-check.argument.list_rules</code> | <code>--list-rules</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | print rule identifiers, defaults and exit |

### Точный вывод `--help` верхнего уровня

```text
usage: check_angelscript_hygiene.py [-h] [--root ROOT] [--scripts DIR] [--config JSON] [--exclude GLOB] [--baseline TSV] [--write-baseline TSV] [--list-rules]

Check authored AngelScript modules for namespace, include, side-macro, sort-directive and file-ending conventions with an optional count-ratchet baseline

options:
  -h, --help            show this help message and exit
  --root ROOT           repository root that script directories and reported paths are relative to (default: the Engine root that contains this script)
  --scripts DIR         root-relative directory searched recursively for .fos files; repeatable (default: Examples)
  --config JSON         rule switches, rule options and excludes (schema_version 1)
  --exclude GLOB        fnmatch pattern for root-relative POSIX paths to skip; repeatable
  --baseline TSV        accepted findings (rule, path, count, reason TSV); repeatable
  --write-baseline TSV  write current finding counts with empty reasons instead of gating
  --list-rules          print rule identifiers, defaults and exit
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
| <a id="entry-helper-cli-createmsi-argument-wix-dir-ab41be5878"></a><code>helper-cli.createmsi.argument.wix_dir</code> | <code>--wix-dir</code> | <code>option</code> | нет | <code>1</code> | - | <code>-</code> | Каталог инструментов WiX; без этого параметра они ищутся в PATH. |
| <a id="entry-helper-cli-createmsi-argument-jsonfile-385f92bb78"></a><code>helper-cli.createmsi.argument.jsonfile</code> | <code>definition.json</code> | <code>positional</code> | да | <code>1</code> | - | - | Только имя файла определения пакета WiX в рабочем каталоге. |

### Точный вывод `--help` верхнего уровня

```text
usage: createmsi.py [-h] [--wix-dir directory] definition.json

Build an MSI package from a WiX definition

positional arguments:
  definition.json      bare WiX package definition filename in the working directory

options:
  -h, --help           show this help message and exit
  --wix-dir directory  directory containing WiX tools; omit to find them on PATH
```

<a id="entry-helper-cli-move-sync-analysis-d49c9c73db"></a>
## Анализ трейса синхронизации движения

Свести строки журнала MOVESYNC из Network.MoveSyncTrace в отчёты о расхождении, отставании и коррекциях по сценариям.

- Стабильный ID: `helper-cli.move-sync-analysis`
- Программа: `move_sync_analysis.py`
- Владелец: `networking`
- Аудитория: `game-developer`, `engine-contributor`, `ci-maintainer`
- Владелец вызова: плейтесты движения встраивающего игрового проекта и прямая диагностика разработчиком
- Исходный парсер: [BuildTools/move_sync_analysis.py](https://github.com/cvet/fonline/blob/master/BuildTools/move_sync_analysis.py)

### Аргументы верхнего уровня

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-move-sync-analysis-argument-logs-3467b9a7b1"></a><code>helper-cli.move-sync-analysis.argument.logs</code> | <code>logs</code> | <code>positional</code> | да | <code>+</code> | - | - | Файлы журналов со строками MOVESYNC (сервер и клиенты). |
| <a id="entry-helper-cli-move-sync-analysis-argument-geometry-1efb4132b0"></a><code>helper-cli.move-sync-analysis.argument.geometry</code> | <code>--geometry</code> | <code>option</code> | да | <code>1</code> | <code>hexagonal</code>, <code>square</code> | - | Геометрия игровой сетки проекта (FO_GEOMETRY). |
| <a id="entry-helper-cli-move-sync-analysis-argument-clock-c5f44c6507"></a><code>helper-cli.move-sync-analysis.argument.clock</code> | <code>--clock</code> | <code>option</code> | нет | <code>1</code> | <code>t</code>, <code>st</code> | <code>t</code> | t: общие монотонные часы (одна машина); st: синхронизированное время (несколько машин). |
| <a id="entry-helper-cli-move-sync-analysis-argument-actor-b6d2392d27"></a><code>helper-cli.move-sync-analysis.argument.actor</code> | <code>--actor</code> | <code>option</code> | нет | <code>1</code> | - | - | Разобрать весь трейс как один сценарий этого id криттера вместо сценариев, ограниченных метками. |
| <a id="entry-helper-cli-move-sync-analysis-argument-settle-ms-a0a6aa90f1"></a><code>helper-cli.move-sync-analysis.argument.settle_ms</code> | <code>--settle-ms</code> | <code>option</code> | нет | <code>1</code> | - | <code>0</code> | Продлить окно каждого сценария на столько миллисекунд после его конечной метки. |
| <a id="entry-helper-cli-move-sync-analysis-argument-extra-side-ec3cf909c8"></a><code>helper-cli.move-sync-analysis.argument.extra_side</code> | <code>--extra-side</code> | <code>option</code> | нет | <code>1</code> | - | - | Также читать в шкалы времени строки MOVESYNC этой стороны, которую пишет проект (можно повторять). |
| <a id="entry-helper-cli-move-sync-analysis-argument-timeline-32c7996c9b"></a><code>helper-cli.move-sync-analysis.argument.timeline</code> | <code>--timeline</code> | <code>option</code> | нет | <code>1</code> | - | - | Вывести сводную шкалу времени сценария с этой меткой (можно повторять, или 'all'). |
| <a id="entry-helper-cli-move-sync-analysis-argument-json-3dc3c07aae"></a><code>helper-cli.move-sync-analysis.argument.json</code> | <code>--json</code> | <code>option</code> | нет | <code>1</code> | - | - | Записать полный отчёт в JSON. |
| <a id="entry-helper-cli-move-sync-analysis-argument-markdown-ececb5d9f8"></a><code>helper-cli.move-sync-analysis.argument.markdown</code> | <code>--markdown</code> | <code>option</code> | нет | <code>1</code> | - | - | Записать сводную таблицу Markdown. |
| <a id="entry-helper-cli-move-sync-analysis-argument-require-converged-beecd1dc57"></a><code>helper-cli.move-sync-analysis.argument.require_converged</code> | <code>--require-converged</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | Завершиться с кодом 1, если хотя бы один сценарий заканчивается расхождением точек зрения. |

### Точный вывод `--help` верхнего уровня

```text
usage: move_sync_analysis.py [-h] --geometry {hexagonal,square} [--clock {t,st}] [--actor ACTOR] [--settle-ms SETTLE_MS] [--extra-side EXTRA_SIDE] [--timeline TIMELINE] [--json JSON] [--markdown MARKDOWN] [--require-converged]
                             logs [logs ...]

Analyse MOVESYNC movement synchronization traces written with Network.MoveSyncTrace.

positional arguments:
  logs                  log files holding MOVESYNC lines (server and clients)

options:
  -h, --help            show this help message and exit
  --geometry {hexagonal,square}
                        world grid geometry of the project (FO_GEOMETRY)
  --clock {t,st}        t: shared monotonic clock (one machine); st: synchronized time (several machines)
  --actor ACTOR         analyse the whole trace as one scenario of this critter id instead of mark-delimited scenarios
  --settle-ms SETTLE_MS
                        extend each scenario window by this many milliseconds after its end mark
  --extra-side EXTRA_SIDE
                        also read MOVESYNC lines of this project-written side into timelines (repeatable)
  --timeline TIMELINE   print the merged timeline of a scenario label (repeatable, or 'all')
  --json JSON           write the full report as JSON
  --markdown MARKDOWN   write the Markdown summary table
  --require-converged   exit 1 when any scenario ends with the views disagreeing
```

<a id="entry-helper-cli-check-text-packs-fedc98c863"></a>
## Проверка текстовых пакетов

Проверить исходные пакеты .fotxt на ошибки разбора, повторяющиеся варианты, текст в чужой письменности и согласованность ключей между языками до бейкинга.

- Стабильный ID: `helper-cli.check-text-packs`
- Программа: `check_text_packs.py`
- Владелец: `localization`
- Аудитория: `game-developer`, `embedding-project-build-system`, `ci-maintainer`
- Владелец вызова: задачи локализации встраивающего игрового проекта и контентные гейты CI
- Исходный парсер: [BuildTools/check_text_packs.py](https://github.com/cvet/fonline/blob/master/BuildTools/check_text_packs.py)

### Аргументы верхнего уровня

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-check-text-packs-argument-roots-627d26f42e"></a><code>helper-cli.check-text-packs.argument.roots</code> | <code>roots</code> | <code>positional</code> | да | <code>+</code> | - | - | Каталоги текстов для рекурсивного обхода; каждый корень проверяется как отдельный вход пакета ресурсов. |
| <a id="entry-helper-cli-check-text-packs-argument-languages-7d3cd02ab0"></a><code>helper-cli.check-text-packs.argument.languages</code> | <code>--languages</code> | <code>option</code> | нет | <code>1</code> | - | - | Упорядоченные объявления Baking.BakeLanguages, например 'engl russ'. |
| <a id="entry-helper-cli-check-text-packs-argument-config-f8719d6287"></a><code>helper-cli.check-text-packs.argument.config</code> | <code>--config</code> | <code>option</code> | нет | <code>1</code> | - | - | Основная конфигурация .fomain, из которой читается Baking.BakeLanguages. |
| <a id="entry-helper-cli-check-text-packs-argument-sub-config-8bcca85937"></a><code>helper-cli.check-text-packs.argument.sub_config</code> | <code>--sub-config</code> | <code>option</code> | нет | <code>1</code> | - | - | Подконфигурация, применяемая поверх основной секции конфигурации (вместе с --config). |
| <a id="entry-helper-cli-check-text-packs-argument-script-b9d05d8ff5"></a><code>helper-cli.check-text-packs.argument.script</code> | <code>--script</code> | <code>option</code> | нет | <code>1</code> | - | - | Письменности, допустимые для языка, например engl=latin (можно повторять). |
| <a id="entry-helper-cli-check-text-packs-argument-ignore-pattern-97a578746d"></a><code>helper-cli.check-text-packs.argument.ignore_pattern</code> | <code>--ignore-pattern</code> | <code>option</code> | нет | <code>1</code> | - | - | Текст, удаляемый перед проверкой письменности, например подстановки проекта (можно повторять). |
| <a id="entry-helper-cli-check-text-packs-argument-sparse-809ef46431"></a><code>helper-cli.check-text-packs.argument.sparse</code> | <code>--sparse</code> | <code>option</code> | нет | <code>1</code> | - | - | Язык, который намеренно наследует отсутствующие пакеты и ключи от резервного языка (можно повторять). |
| <a id="entry-helper-cli-check-text-packs-argument-strict-variant-counts-33eaebdbfd"></a><code>helper-cli.check-text-packs.argument.strict_variant_counts</code> | <code>--strict-variant-counts</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | Также сообщать о ключах, у которых число вариантов отличается от резервного языка. |
| <a id="entry-helper-cli-check-text-packs-argument-baseline-b7bb7e71e3"></a><code>helper-cli.check-text-packs.argument.baseline</code> | <code>--baseline</code> | <code>option</code> | нет | <code>1</code> | - | - | JSON-файл исключений с причиной для каждой известной находки (можно повторять). |
| <a id="entry-helper-cli-check-text-packs-argument-base-dir-318973e81c"></a><code>helper-cli.check-text-packs.argument.base_dir</code> | <code>--base-dir</code> | <code>option</code> | нет | <code>1</code> | - | - | Каталог, относительно которого записываются пути находок и исключений (по умолчанию: текущий каталог). |
| <a id="entry-helper-cli-check-text-packs-argument-json-48e08e1073"></a><code>helper-cli.check-text-packs.argument.json</code> | <code>--json</code> | <code>option</code> | нет | <code>1</code> | - | - | Дополнительно записать оставшиеся находки в JSON. |

### Точный вывод `--help` верхнего уровня

```text
usage: check_text_packs.py [-h] (--languages LANGUAGES | --config CONFIG) [--sub-config SUB_CONFIG] [--script LANGUAGE=SCRIPTS] [--ignore-pattern REGEX] [--sparse LANGUAGE] [--strict-variant-counts] [--baseline BASELINE]
                           [--base-dir BASE_DIR] [--json JSON]
                           roots [roots ...]

Lint raw .fotxt text packs against the Engine grammar and language normalization.

positional arguments:
  roots                 text directories to scan recursively; each root is checked as one resource-pack input

options:
  -h, --help            show this help message and exit
  --languages LANGUAGES
                        ordered Baking.BakeLanguages declarations, such as 'engl russ'
  --config CONFIG       main .fomain config whose Baking.BakeLanguages is read
  --sub-config SUB_CONFIG
                        sub-config applied over the main config section (with --config)
  --script LANGUAGE=SCRIPTS
                        letter scripts a language allows, such as engl=latin (repeatable)
  --ignore-pattern REGEX
                        text removed before the script check, such as project placeholders (repeatable)
  --sparse LANGUAGE     language that intentionally inherits missing packs and keys from its fallback (repeatable)
  --strict-variant-counts
                        also report keys whose variant count differs from the fallback language
  --baseline BASELINE   JSON exemption file with a reason per known finding (repeatable)
  --base-dir BASE_DIR   directory that finding and exemption paths are relative to (default: current directory)
  --json JSON           also write the remaining findings as JSON
```
