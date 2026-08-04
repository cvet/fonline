---
title: Команды BuildTools
document_id: generated-cli-commands
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-cli-commands","locale":"ru","source_path":"Docs/en/reference/buildtools/commands.md","source_sha256":"d91c3a987bbddfabba77adb77ca708c58b43e59bbfdc1b30c94b7f0b484b507b"} -->

# Команды BuildTools

> Сгенерированный справочник. Не редактируйте эту страницу напрямую. Обновите `BuildTools/buildtools.py`, затем выполните `python BuildTools/docs_cli.py --write`.

[Индекс справочника](index.md) | [Команды](commands.md) | [Каноническая JSON-модель](../../../generated/cli.json) | [Контракт генерации](../metadata/)

Запускайте команды из корня репозитория движка через `python BuildTools/buildtools.py <command>`. Точные блоки usage и справки ниже созданы исполняемым парсером.

<a id="entry-cli-buildtools-command-env-947ff38c97"></a>
## `env`

Определить окружение BuildTools.

Стабильный ID: `cli.buildtools.command.env`

```text
usage: buildtools.py env [-h] [--shell {bash,cmd,plain}] [--summary]
                         [--summary-only]
```

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-cli-buildtools-command-env-argument-shell-794f1a40be"></a><code>cli.buildtools.command.env.argument.shell</code> | <code>--shell</code> | <code>option</code> | нет | <code>1</code> | <code>bash</code>, <code>cmd</code>, <code>plain</code> | <code>plain</code> | Синтаксис вывода окружения. |
| <a id="entry-cli-buildtools-command-env-argument-summary-ae64b9a7c1"></a><code>cli.buildtools.command.env.argument.summary</code> | <code>--summary</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | Добавить сводку определённого окружения. |
| <a id="entry-cli-buildtools-command-env-argument-summary-only-e2c3c736a7"></a><code>cli.buildtools.command.env.argument.summary_only</code> | <code>--summary-only</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | Вывести только сводку определённого окружения. |

### Точный вывод `--help`

```text
usage: buildtools.py env [-h] [--shell {bash,cmd,plain}] [--summary]
                         [--summary-only]

options:
  -h, --help            show this help message and exit
  --shell {bash,cmd,plain}
                        environment output syntax
  --summary             append the resolved environment summary
  --summary-only        print only the resolved environment summary
```

<a id="entry-cli-buildtools-command-build-359b790fe1"></a>
## `build`

Настроить и собрать цель.

Стабильный ID: `cli.buildtools.command.build`

```text
usage: buildtools.py build [-h] platform target [config]
```

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-cli-buildtools-command-build-argument-platform-1bd10776e8"></a><code>cli.buildtools.command.build.argument.platform</code> | <code>platform</code> | <code>positional</code> | да | <code>1</code> | - | - | Ключ платформы движка, например linux, win64, web или android-arm64. |
| <a id="entry-cli-buildtools-command-build-argument-target-54402fb830"></a><code>cli.buildtools.command.build.argument.target</code> | <code>target</code> | <code>positional</code> | да | <code>1</code> | - | - | Профиль цели BuildTools, например client, server, baker или unit-tests. |
| <a id="entry-cli-buildtools-command-build-argument-config-f6fde5057e"></a><code>cli.buildtools.command.build.argument.config</code> | <code>config</code> | <code>positional</code> | нет | <code>?</code> | - | <code>Release</code> | Конфигурация сборки CMake. |

### Точный вывод `--help`

```text
usage: buildtools.py build [-h] platform target [config]

positional arguments:
  platform    engine platform key, such as linux, win64, web, or android-arm64
  target      BuildTools target profile, such as client, server, baker, or
              unit-tests
  config      CMake build configuration

options:
  -h, --help  show this help message and exit
```

<a id="entry-cli-buildtools-command-validate-2ae8a97c09"></a>
## `validate`

Настроить и проверить сценарии.

Стабильный ID: `cli.buildtools.command.validate`

```text
usage: buildtools.py validate [-h] names [names ...]
```

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-cli-buildtools-command-validate-argument-names-6cc5a8c6f4"></a><code>cli.buildtools.command.validate.argument.names</code> | <code>names</code> | <code>positional</code> | да | <code>+</code> | - | - | Одно или несколько имён сценариев проверки. |

### Точный вывод `--help`

```text
usage: buildtools.py validate [-h] names [names ...]

positional arguments:
  names       one or more named validation scenarios

options:
  -h, --help  show this help message and exit
```

<a id="entry-cli-buildtools-command-setup-mono-765deb9994"></a>
## `setup-mono`

Подготовить runtime Mono.

Стабильный ID: `cli.buildtools.command.setup-mono`

```text
usage: buildtools.py setup-mono [-h] os_name arch config
```

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-cli-buildtools-command-setup-mono-argument-os-name-492ca88106"></a><code>cli.buildtools.command.setup-mono.argument.os_name</code> | <code>os_name</code> | <code>positional</code> | да | <code>1</code> | - | - | Ключ операционной системы runtime. |
| <a id="entry-cli-buildtools-command-setup-mono-argument-arch-5a0e3638e2"></a><code>cli.buildtools.command.setup-mono.argument.arch</code> | <code>arch</code> | <code>positional</code> | да | <code>1</code> | - | - | Ключ архитектуры runtime. |
| <a id="entry-cli-buildtools-command-setup-mono-argument-config-71f64f592d"></a><code>cli.buildtools.command.setup-mono.argument.config</code> | <code>config</code> | <code>positional</code> | да | <code>1</code> | - | - | Конфигурация сборки runtime. |

### Точный вывод `--help`

```text
usage: buildtools.py setup-mono [-h] os_name arch config

positional arguments:
  os_name     runtime operating-system key
  arch        runtime architecture key
  config      runtime build configuration

options:
  -h, --help  show this help message and exit
```

<a id="entry-cli-buildtools-command-format-source-f2335e6ebf"></a>
## `format-source`

Форматировать исходники движка.

Стабильный ID: `cli.buildtools.command.format-source`

```text
usage: buildtools.py format-source [-h]
```

У команды нет собственных аргументов.

### Точный вывод `--help`

```text
usage: buildtools.py format-source [-h]

options:
  -h, --help  show this help message and exit
```

<a id="entry-cli-buildtools-command-toolset-d0a17e5293"></a>
## `toolset`

Собрать существующую цель toolset.

Стабильный ID: `cli.buildtools.command.toolset`

```text
usage: buildtools.py toolset [-h] target
```

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-cli-buildtools-command-toolset-argument-target-a4718bcecd"></a><code>cli.buildtools.command.toolset.argument.target</code> | <code>target</code> | <code>positional</code> | да | <code>1</code> | - | - | Цель из настроенного дерева сборки toolset. |

### Точный вывод `--help`

```text
usage: buildtools.py toolset [-h] target

positional arguments:
  target      target from the configured toolset build tree

options:
  -h, --help  show this help message and exit
```

<a id="entry-cli-buildtools-command-build-auxiliary-436100c86d"></a>
## `build-auxiliary`

Собрать отдельно упаковываемый вспомогательный инструмент.

Стабильный ID: `cli.buildtools.command.build-auxiliary`

```text
usage: buildtools.py build-auxiliary [-h] {effekseer-editor} [{Debug,Release}]
```

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-cli-buildtools-command-build-auxiliary-argument-target-5e2885ee6b"></a><code>cli.buildtools.command.build-auxiliary.argument.target</code> | <code>target</code> | <code>positional</code> | да | <code>1</code> | <code>effekseer-editor</code> | - | Собираемый вспомогательный инструмент. |
| <a id="entry-cli-buildtools-command-build-auxiliary-argument-config-67f5cbe064"></a><code>cli.buildtools.command.build-auxiliary.argument.config</code> | <code>config</code> | <code>positional</code> | нет | <code>?</code> | <code>Debug</code>, <code>Release</code> | <code>Release</code> | Конфигурация сборки (по умолчанию: Release). |

### Точный вывод `--help`

```text
usage: buildtools.py build-auxiliary [-h] {effekseer-editor} [{Debug,Release}]

positional arguments:
  {effekseer-editor}  auxiliary tool to build
  {Debug,Release}     build configuration (default: Release)

options:
  -h, --help          show this help message and exit
```

<a id="entry-cli-buildtools-command-prepare-workspace-82abeebbaf"></a>
## `prepare-workspace`

Подготовить общие части рабочего каталога.

Стабильный ID: `cli.buildtools.command.prepare-workspace`

```text
usage: buildtools.py prepare-workspace [-h] [--check]
                                       {toolset,emscripten,android-sdk,android-ndk,dotnet,xwin,msan-libcxx} [{toolset,emscripten,android-sdk,android-ndk,dotnet,xwin,msan-libcxx} ...]
```

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-cli-buildtools-command-prepare-workspace-argument-parts-6dfc61f96e"></a><code>cli.buildtools.command.prepare-workspace.argument.parts</code> | <code>parts</code> | <code>positional</code> | да | <code>+</code> | <code>toolset</code>, <code>emscripten</code>, <code>android-sdk</code>, <code>android-ndk</code>, <code>dotnet</code>, <code>xwin</code>, <code>msan-libcxx</code> | - | Подготавливаемые компоненты рабочего каталога. |
| <a id="entry-cli-buildtools-command-prepare-workspace-argument-check-6062fdb605"></a><code>cli.buildtools.command.prepare-workspace.argument.check</code> | <code>--check</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | Проверить доступность без установки или сборки. |

### Точный вывод `--help`

```text
usage: buildtools.py prepare-workspace [-h] [--check]
                                       {toolset,emscripten,android-sdk,android-ndk,dotnet,xwin,msan-libcxx} [{toolset,emscripten,android-sdk,android-ndk,dotnet,xwin,msan-libcxx} ...]

positional arguments:
  {toolset,emscripten,android-sdk,android-ndk,dotnet,xwin,msan-libcxx}
                        workspace components to prepare

options:
  -h, --help            show this help message and exit
  --check               check availability without installing or building
```

<a id="entry-cli-buildtools-command-package-web-debug-0138fed878"></a>
## `package-web-debug`

Упаковать локальный Web-клиент для отладки.

Стабильный ID: `cli.buildtools.command.package-web-debug`

```text
usage: buildtools.py package-web-debug [-h] devname configs [configs ...]
```

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-cli-buildtools-command-package-web-debug-argument-devname-4999bca224"></a><code>cli.buildtools.command.package-web-debug.argument.devname</code> | <code>devname</code> | <code>positional</code> | да | <code>1</code> | - | - | Краткое имя проекта для имён бинарных файлов и каталогов (например, LF). |
| <a id="entry-cli-buildtools-command-package-web-debug-argument-configs-a9e7b9ea6a"></a><code>cli.buildtools.command.package-web-debug.argument.configs</code> | <code>configs</code> | <code>positional</code> | да | <code>+</code> | - | - | Имена упаковываемых конфигураций (например, RemoteSceneLaunch LocalTest). |

### Точный вывод `--help`

```text
usage: buildtools.py package-web-debug [-h] devname configs [configs ...]

positional arguments:
  devname     short project name for binary/directory naming (e.g. LF)
  configs     config names to package (e.g. RemoteSceneLaunch LocalTest)

options:
  -h, --help  show this help message and exit
```

<a id="entry-cli-buildtools-command-package-android-debug-e5508ac5d2"></a>
## `package-android-debug`

Упаковать локальный Android-клиент для отладки.

Стабильный ID: `cli.buildtools.command.package-android-debug`

```text
usage: buildtools.py package-android-debug [-h]
                                           devname
                                           {android-arm32,android-arm64,android-x86}
                                           configs [configs ...]
```

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-cli-buildtools-command-package-android-debug-argument-devname-3230199d8e"></a><code>cli.buildtools.command.package-android-debug.argument.devname</code> | <code>devname</code> | <code>positional</code> | да | <code>1</code> | - | - | Краткое имя проекта для имён бинарных файлов и каталогов (например, LF). |
| <a id="entry-cli-buildtools-command-package-android-debug-argument-platform-baae3fe430"></a><code>cli.buildtools.command.package-android-debug.argument.platform</code> | <code>platform</code> | <code>positional</code> | да | <code>1</code> | <code>android-arm32</code>, <code>android-arm64</code>, <code>android-x86</code> | - | Целевая платформа Android (например, android-arm64). |
| <a id="entry-cli-buildtools-command-package-android-debug-argument-configs-bd15b3fbb9"></a><code>cli.buildtools.command.package-android-debug.argument.configs</code> | <code>configs</code> | <code>positional</code> | да | <code>+</code> | - | - | Имена упаковываемых конфигураций (например, LocalTest). |

### Точный вывод `--help`

```text
usage: buildtools.py package-android-debug [-h]
                                           devname
                                           {android-arm32,android-arm64,android-x86}
                                           configs [configs ...]

positional arguments:
  devname               short project name for binary/directory naming (e.g.
                        LF)
  {android-arm32,android-arm64,android-x86}
                        Android target platform (e.g. android-arm64)
  configs               config names to package (e.g. LocalTest)

options:
  -h, --help            show this help message and exit
```

<a id="entry-cli-buildtools-command-host-check-20b3f27def"></a>
## `host-check`

Проверить зависимости host-системы.

Стабильный ID: `cli.buildtools.command.host-check`

```text
usage: buildtools.py host-check [-h] {linux,macos,windows}
```

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-cli-buildtools-command-host-check-argument-host-dd9bf4c335"></a><code>cli.buildtools.command.host-check.argument.host</code> | <code>host</code> | <code>positional</code> | да | <code>1</code> | <code>linux</code>, <code>macos</code>, <code>windows</code> | - | Проверяемая host-платформа. |

### Точный вывод `--help`

```text
usage: buildtools.py host-check [-h] {linux,macos,windows}

positional arguments:
  {linux,macos,windows}
                        host platform to inspect

options:
  -h, --help            show this help message and exit
```

<a id="entry-cli-buildtools-command-prepare-host-workspace-f7fae13345"></a>
## `prepare-host-workspace`

Подготовить рабочий каталог и зависимости host-системы.

Стабильный ID: `cli.buildtools.command.prepare-host-workspace`

```text
usage: buildtools.py prepare-host-workspace [-h] [--check]
                                            {linux,windows,macos}
                                            [{common-packages,linux-packages,web-packages,android-packages,windows-cross-packages,msi-packages,all-packages,linux,web,android-arm32,android-arm64,android-x86,toolset,dotnet,windows-cross,msan-libcxx,all} ...]
```

| Стабильный ID | Аргумент | Вид | Обязателен | Значения | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-cli-buildtools-command-prepare-host-workspace-argument-host-8fb8276bf1"></a><code>cli.buildtools.command.prepare-host-workspace.argument.host</code> | <code>host</code> | <code>positional</code> | да | <code>1</code> | <code>linux</code>, <code>windows</code>, <code>macos</code> | - | Подготавливаемая host-платформа. |
| <a id="entry-cli-buildtools-command-prepare-host-workspace-argument-features-1d7026db67"></a><code>cli.buildtools.command.prepare-host-workspace.argument.features</code> | <code>features</code> | <code>positional</code> | нет | <code>*</code> | <code>common-packages</code>, <code>linux-packages</code>, <code>web-packages</code>, <code>android-packages</code>, <code>windows-cross-packages</code>, <code>msi-packages</code>, <code>all-packages</code>, <code>linux</code>, <code>web</code>, <code>android-arm32</code>, <code>android-arm64</code>, <code>android-x86</code>, <code>toolset</code>, <code>dotnet</code>, <code>windows-cross</code>, <code>msan-libcxx</code>, <code>all</code> | - | Подготавливаемые группы возможностей; не указывайте их, чтобы использовать значения host-системы по умолчанию. |
| <a id="entry-cli-buildtools-command-prepare-host-workspace-argument-check-f990629a92"></a><code>cli.buildtools.command.prepare-host-workspace.argument.check</code> | <code>--check</code> | <code>option</code> | нет | <code>0</code> | - | <code>false</code> | Проверить доступность без установки или сборки. |

### Точный вывод `--help`

```text
usage: buildtools.py prepare-host-workspace [-h] [--check]
                                            {linux,windows,macos}
                                            [{common-packages,linux-packages,web-packages,android-packages,windows-cross-packages,msi-packages,all-packages,linux,web,android-arm32,android-arm64,android-x86,toolset,dotnet,windows-cross,msan-libcxx,all} ...]

positional arguments:
  {linux,windows,macos}
                        host platform to prepare
  {common-packages,linux-packages,web-packages,android-packages,windows-cross-packages,msi-packages,all-packages,linux,web,android-arm32,android-arm64,android-x86,toolset,dotnet,windows-cross,msan-libcxx,all}
                        feature groups to prepare; omit to use the host
                        defaults

options:
  -h, --help            show this help message and exit
  --check               check availability without installing or building
```
