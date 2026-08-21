---
title: BuildTools Commands
document_id: generated-cli-commands
locale: en
generated: true
---

# BuildTools Commands

> Generated reference. Do not edit this page directly. Update `BuildTools/buildtools.py`, then run `python BuildTools/docs_cli.py --write`.

[Reference index](index.md) | [Commands](commands.md) | [Canonical JSON model](../../../generated/cli.json) | [Generation contract](../metadata/)

Run commands from the engine repository root with `python BuildTools/buildtools.py <command>`. The exact usage and help blocks below are emitted by the executable parser.

<a id="entry-cli-buildtools-command-env-947ff38c97"></a>
## `env`

resolve BuildTools environment

Stable ID: `cli.buildtools.command.env`

```text
usage: buildtools.py env [-h] [--shell {bash,cmd,plain}] [--summary] [--summary-only]
```

| Stable ID | Argument | Kind | Required | Values | Choices | Default | Description |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-cli-buildtools-command-env-argument-shell-794f1a40be"></a><code>cli.buildtools.command.env.argument.shell</code> | <code>--shell</code> | <code>option</code> | no | <code>1</code> | <code>bash</code>, <code>cmd</code>, <code>plain</code> | <code>plain</code> | environment output syntax |
| <a id="entry-cli-buildtools-command-env-argument-summary-ae64b9a7c1"></a><code>cli.buildtools.command.env.argument.summary</code> | <code>--summary</code> | <code>option</code> | no | <code>0</code> | - | <code>false</code> | append the resolved environment summary |
| <a id="entry-cli-buildtools-command-env-argument-summary-only-e2c3c736a7"></a><code>cli.buildtools.command.env.argument.summary_only</code> | <code>--summary-only</code> | <code>option</code> | no | <code>0</code> | - | <code>false</code> | print only the resolved environment summary |

### Exact `--help` output

```text
usage: buildtools.py env [-h] [--shell {bash,cmd,plain}] [--summary] [--summary-only]

options:
  -h, --help            show this help message and exit
  --shell {bash,cmd,plain}
                        environment output syntax
  --summary             append the resolved environment summary
  --summary-only        print only the resolved environment summary
```

<a id="entry-cli-buildtools-command-build-359b790fe1"></a>
## `build`

configure and build a target

Stable ID: `cli.buildtools.command.build`

```text
usage: buildtools.py build [-h] platform target [config]
```

| Stable ID | Argument | Kind | Required | Values | Choices | Default | Description |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-cli-buildtools-command-build-argument-platform-1bd10776e8"></a><code>cli.buildtools.command.build.argument.platform</code> | <code>platform</code> | <code>positional</code> | yes | <code>1</code> | - | - | engine platform key, such as linux, win64, web, or android-arm64 |
| <a id="entry-cli-buildtools-command-build-argument-target-54402fb830"></a><code>cli.buildtools.command.build.argument.target</code> | <code>target</code> | <code>positional</code> | yes | <code>1</code> | - | - | BuildTools target profile, such as client, server, baker, or unit-tests |
| <a id="entry-cli-buildtools-command-build-argument-config-f6fde5057e"></a><code>cli.buildtools.command.build.argument.config</code> | <code>config</code> | <code>positional</code> | no | <code>?</code> | - | <code>Release</code> | CMake build configuration |

### Exact `--help` output

```text
usage: buildtools.py build [-h] platform target [config]

positional arguments:
  platform    engine platform key, such as linux, win64, web, or android-arm64
  target      BuildTools target profile, such as client, server, baker, or unit-tests
  config      CMake build configuration

options:
  -h, --help  show this help message and exit
```

<a id="entry-cli-buildtools-command-validate-2ae8a97c09"></a>
## `validate`

configure and validate scenarios

Stable ID: `cli.buildtools.command.validate`

```text
usage: buildtools.py validate [-h] names [names ...]
```

| Stable ID | Argument | Kind | Required | Values | Choices | Default | Description |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-cli-buildtools-command-validate-argument-names-6cc5a8c6f4"></a><code>cli.buildtools.command.validate.argument.names</code> | <code>names</code> | <code>positional</code> | yes | <code>+</code> | - | - | one or more named validation scenarios |

### Exact `--help` output

```text
usage: buildtools.py validate [-h] names [names ...]

positional arguments:
  names       one or more named validation scenarios

options:
  -h, --help  show this help message and exit
```

<a id="entry-cli-buildtools-command-setup-mono-765deb9994"></a>
## `setup-mono`

prepare mono runtime

Stable ID: `cli.buildtools.command.setup-mono`

```text
usage: buildtools.py setup-mono [-h] os_name arch config
```

| Stable ID | Argument | Kind | Required | Values | Choices | Default | Description |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-cli-buildtools-command-setup-mono-argument-os-name-492ca88106"></a><code>cli.buildtools.command.setup-mono.argument.os_name</code> | <code>os_name</code> | <code>positional</code> | yes | <code>1</code> | - | - | runtime operating-system key |
| <a id="entry-cli-buildtools-command-setup-mono-argument-arch-5a0e3638e2"></a><code>cli.buildtools.command.setup-mono.argument.arch</code> | <code>arch</code> | <code>positional</code> | yes | <code>1</code> | - | - | runtime architecture key |
| <a id="entry-cli-buildtools-command-setup-mono-argument-config-71f64f592d"></a><code>cli.buildtools.command.setup-mono.argument.config</code> | <code>config</code> | <code>positional</code> | yes | <code>1</code> | - | - | runtime build configuration |

### Exact `--help` output

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

format engine source files

Stable ID: `cli.buildtools.command.format-source`

```text
usage: buildtools.py format-source [-h]
```

No command-specific arguments.

### Exact `--help` output

```text
usage: buildtools.py format-source [-h]

options:
  -h, --help  show this help message and exit
```

<a id="entry-cli-buildtools-command-toolset-d0a17e5293"></a>
## `toolset`

build an existing toolset target

Stable ID: `cli.buildtools.command.toolset`

```text
usage: buildtools.py toolset [-h] target
```

| Stable ID | Argument | Kind | Required | Values | Choices | Default | Description |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-cli-buildtools-command-toolset-argument-target-a4718bcecd"></a><code>cli.buildtools.command.toolset.argument.target</code> | <code>target</code> | <code>positional</code> | yes | <code>1</code> | - | - | target from the configured toolset build tree |

### Exact `--help` output

```text
usage: buildtools.py toolset [-h] target

positional arguments:
  target      target from the configured toolset build tree

options:
  -h, --help  show this help message and exit
```

<a id="entry-cli-buildtools-command-build-auxiliary-436100c86d"></a>
## `build-auxiliary`

build a separately packaged auxiliary tool

Stable ID: `cli.buildtools.command.build-auxiliary`

```text
usage: buildtools.py build-auxiliary [-h] {effekseer-editor} [{Debug,Release}]
```

| Stable ID | Argument | Kind | Required | Values | Choices | Default | Description |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-cli-buildtools-command-build-auxiliary-argument-target-5e2885ee6b"></a><code>cli.buildtools.command.build-auxiliary.argument.target</code> | <code>target</code> | <code>positional</code> | yes | <code>1</code> | <code>effekseer-editor</code> | - | auxiliary tool to build |
| <a id="entry-cli-buildtools-command-build-auxiliary-argument-config-67f5cbe064"></a><code>cli.buildtools.command.build-auxiliary.argument.config</code> | <code>config</code> | <code>positional</code> | no | <code>?</code> | <code>Debug</code>, <code>Release</code> | <code>Release</code> | build configuration (default: Release) |

### Exact `--help` output

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

prepare shared workspace parts

Stable ID: `cli.buildtools.command.prepare-workspace`

```text
usage: buildtools.py prepare-workspace [-h] [--check] {toolset,emscripten,android-sdk,android-ndk,dotnet,xwin,msan-libcxx} [{toolset,emscripten,android-sdk,android-ndk,dotnet,xwin,msan-libcxx} ...]
```

| Stable ID | Argument | Kind | Required | Values | Choices | Default | Description |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-cli-buildtools-command-prepare-workspace-argument-parts-6dfc61f96e"></a><code>cli.buildtools.command.prepare-workspace.argument.parts</code> | <code>parts</code> | <code>positional</code> | yes | <code>+</code> | <code>toolset</code>, <code>emscripten</code>, <code>android-sdk</code>, <code>android-ndk</code>, <code>dotnet</code>, <code>xwin</code>, <code>msan-libcxx</code> | - | workspace components to prepare |
| <a id="entry-cli-buildtools-command-prepare-workspace-argument-check-6062fdb605"></a><code>cli.buildtools.command.prepare-workspace.argument.check</code> | <code>--check</code> | <code>option</code> | no | <code>0</code> | - | <code>false</code> | check availability without installing or building |

### Exact `--help` output

```text
usage: buildtools.py prepare-workspace [-h] [--check] {toolset,emscripten,android-sdk,android-ndk,dotnet,xwin,msan-libcxx} [{toolset,emscripten,android-sdk,android-ndk,dotnet,xwin,msan-libcxx} ...]

positional arguments:
  {toolset,emscripten,android-sdk,android-ndk,dotnet,xwin,msan-libcxx}
                        workspace components to prepare

options:
  -h, --help            show this help message and exit
  --check               check availability without installing or building
```

<a id="entry-cli-buildtools-command-repair-checkout-case-675fd250f4"></a>
## `repair-checkout-case`

realign working-tree entry names with the git index

Stable ID: `cli.buildtools.command.repair-checkout-case`

```text
usage: buildtools.py repair-checkout-case [-h] [--check]
```

| Stable ID | Argument | Kind | Required | Values | Choices | Default | Description |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-cli-buildtools-command-repair-checkout-case-argument-check-659400bbbd"></a><code>cli.buildtools.command.repair-checkout-case.argument.check</code> | <code>--check</code> | <code>option</code> | no | <code>0</code> | - | <code>false</code> | check for case drift without renaming entries |

### Exact `--help` output

```text
usage: buildtools.py repair-checkout-case [-h] [--check]

options:
  -h, --help  show this help message and exit
  --check     check for case drift without renaming entries
```

<a id="entry-cli-buildtools-command-package-web-debug-0138fed878"></a>
## `package-web-debug`

package the local web debug client

Stable ID: `cli.buildtools.command.package-web-debug`

```text
usage: buildtools.py package-web-debug [-h] devname configs [configs ...]
```

| Stable ID | Argument | Kind | Required | Values | Choices | Default | Description |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-cli-buildtools-command-package-web-debug-argument-devname-4999bca224"></a><code>cli.buildtools.command.package-web-debug.argument.devname</code> | <code>devname</code> | <code>positional</code> | yes | <code>1</code> | - | - | short project name for binary/directory naming (e.g. LF) |
| <a id="entry-cli-buildtools-command-package-web-debug-argument-configs-a9e7b9ea6a"></a><code>cli.buildtools.command.package-web-debug.argument.configs</code> | <code>configs</code> | <code>positional</code> | yes | <code>+</code> | - | - | config names to package (e.g. RemoteSceneLaunch LocalTest) |

### Exact `--help` output

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

package the local android debug client

Stable ID: `cli.buildtools.command.package-android-debug`

```text
usage: buildtools.py package-android-debug [-h] devname {android-arm32,android-arm64,android-x86} configs [configs ...]
```

| Stable ID | Argument | Kind | Required | Values | Choices | Default | Description |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-cli-buildtools-command-package-android-debug-argument-devname-3230199d8e"></a><code>cli.buildtools.command.package-android-debug.argument.devname</code> | <code>devname</code> | <code>positional</code> | yes | <code>1</code> | - | - | short project name for binary/directory naming (e.g. LF) |
| <a id="entry-cli-buildtools-command-package-android-debug-argument-platform-baae3fe430"></a><code>cli.buildtools.command.package-android-debug.argument.platform</code> | <code>platform</code> | <code>positional</code> | yes | <code>1</code> | <code>android-arm32</code>, <code>android-arm64</code>, <code>android-x86</code> | - | Android target platform (e.g. android-arm64) |
| <a id="entry-cli-buildtools-command-package-android-debug-argument-configs-bd15b3fbb9"></a><code>cli.buildtools.command.package-android-debug.argument.configs</code> | <code>configs</code> | <code>positional</code> | yes | <code>+</code> | - | - | config names to package (e.g. LocalTest) |

### Exact `--help` output

```text
usage: buildtools.py package-android-debug [-h] devname {android-arm32,android-arm64,android-x86} configs [configs ...]

positional arguments:
  devname               short project name for binary/directory naming (e.g. LF)
  {android-arm32,android-arm64,android-x86}
                        Android target platform (e.g. android-arm64)
  configs               config names to package (e.g. LocalTest)

options:
  -h, --help            show this help message and exit
```

<a id="entry-cli-buildtools-command-host-check-20b3f27def"></a>
## `host-check`

check host prerequisites

Stable ID: `cli.buildtools.command.host-check`

```text
usage: buildtools.py host-check [-h] {linux,macos,windows}
```

| Stable ID | Argument | Kind | Required | Values | Choices | Default | Description |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-cli-buildtools-command-host-check-argument-host-dd9bf4c335"></a><code>cli.buildtools.command.host-check.argument.host</code> | <code>host</code> | <code>positional</code> | yes | <code>1</code> | <code>linux</code>, <code>macos</code>, <code>windows</code> | - | host platform to inspect |

### Exact `--help` output

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

prepare host workspace and prerequisites

Stable ID: `cli.buildtools.command.prepare-host-workspace`

```text
usage: buildtools.py prepare-host-workspace [-h] [--check]
                                            {linux,windows,macos}
                                            [{common-packages,linux-packages,showcase-display-packages,web-packages,android-packages,windows-cross-packages,msi-packages,all-packages,linux,web,android-arm32,android-arm64,android-x86,toolset,dotnet,windows-cross,msan-libcxx,all} ...]
```

| Stable ID | Argument | Kind | Required | Values | Choices | Default | Description |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-cli-buildtools-command-prepare-host-workspace-argument-host-8fb8276bf1"></a><code>cli.buildtools.command.prepare-host-workspace.argument.host</code> | <code>host</code> | <code>positional</code> | yes | <code>1</code> | <code>linux</code>, <code>windows</code>, <code>macos</code> | - | host platform to prepare |
| <a id="entry-cli-buildtools-command-prepare-host-workspace-argument-features-1d7026db67"></a><code>cli.buildtools.command.prepare-host-workspace.argument.features</code> | <code>features</code> | <code>positional</code> | no | <code>*</code> | <code>common-packages</code>, <code>linux-packages</code>, <code>showcase-display-packages</code>, <code>web-packages</code>, <code>android-packages</code>, <code>windows-cross-packages</code>, <code>msi-packages</code>, <code>all-packages</code>, <code>linux</code>, <code>web</code>, <code>android-arm32</code>, <code>android-arm64</code>, <code>android-x86</code>, <code>toolset</code>, <code>dotnet</code>, <code>windows-cross</code>, <code>msan-libcxx</code>, <code>all</code> | - | feature groups to prepare; omit to use the host defaults |
| <a id="entry-cli-buildtools-command-prepare-host-workspace-argument-check-f990629a92"></a><code>cli.buildtools.command.prepare-host-workspace.argument.check</code> | <code>--check</code> | <code>option</code> | no | <code>0</code> | - | <code>false</code> | check availability without installing or building |

### Exact `--help` output

```text
usage: buildtools.py prepare-host-workspace [-h] [--check]
                                            {linux,windows,macos}
                                            [{common-packages,linux-packages,showcase-display-packages,web-packages,android-packages,windows-cross-packages,msi-packages,all-packages,linux,web,android-arm32,android-arm64,android-x86,toolset,dotnet,windows-cross,msan-libcxx,all} ...]

positional arguments:
  {linux,windows,macos}
                        host platform to prepare
  {common-packages,linux-packages,showcase-display-packages,web-packages,android-packages,windows-cross-packages,msi-packages,all-packages,linux,web,android-arm32,android-arm64,android-x86,toolset,dotnet,windows-cross,msan-libcxx,all}
                        feature groups to prepare; omit to use the host defaults

options:
  -h, --help            show this help message and exit
  --check               check availability without installing or building
```
