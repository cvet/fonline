---
title: Helper Commands
document_id: generated-helper-cli-commands
locale: en
generated: true
---

# Helper Commands

> Generated reference. Do not edit this page directly. Update `BuildTools/HelperCliInterface.json` or the owning executable parser, then run `python BuildTools/docs_helper_cli.py --write`.

[Reference index](index.md) | [Commands](commands.md) | [Canonical JSON model](../../../generated/helper-cli.json) | [Generation contract](../metadata/)

Commands are shown with exact parser-generated usage and help at a fixed 80-column width. Invoke a helper from the engine repository root unless its invocation owner sets another working directory.

<a id="entry-helper-cli-codegen-60abdf415d"></a>
## Code generation

Generate engine configuration, metadata registration, script bindings, and embedded-resource source files.

- Stable ID: `helper-cli.codegen`
- Program: `codegen.py`
- Owner: `build-release`
- Audience: `engine-contributor`, `embedding-project-build-system`
- Invocation owner: BuildTools/cmake/stages/Codegen.cmake
- Parser source: [BuildTools/codegen.py](https://github.com/cvet/fonline/blob/master/BuildTools/codegen.py)

### Top-level arguments

| Stable ID | Argument | Kind | Required | Values | Choices | Default | Description |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-codegen-argument-maincfg-a02226a81e"></a><code>helper-cli.codegen.argument.maincfg</code> | <code>-maincfg</code> | <code>option</code> | yes | <code>1</code> | - | - | main config file |
| <a id="entry-helper-cli-codegen-argument-buildhash-bbbdec66bb"></a><code>helper-cli.codegen.argument.buildhash</code> | <code>-buildhash</code> | <code>option</code> | yes | <code>1</code> | - | - | build hash |
| <a id="entry-helper-cli-codegen-argument-devname-47485557c0"></a><code>helper-cli.codegen.argument.devname</code> | <code>-devname</code> | <code>option</code> | yes | <code>1</code> | - | - | dev game name |
| <a id="entry-helper-cli-codegen-argument-nicename-161cc8093d"></a><code>helper-cli.codegen.argument.nicename</code> | <code>-nicename</code> | <code>option</code> | yes | <code>1</code> | - | - | nice game name |
| <a id="entry-helper-cli-codegen-argument-embedded-f936b1a395"></a><code>helper-cli.codegen.argument.embedded</code> | <code>-embedded</code> | <code>option</code> | yes | <code>1</code> | - | - | embedded buffer capacity |
| <a id="entry-helper-cli-codegen-argument-internalcfg-9f8ef3aef8"></a><code>helper-cli.codegen.argument.internalcfg</code> | <code>-internalcfg</code> | <code>option</code> | yes | <code>1</code> | - | - | internal config buffer capacity |
| <a id="entry-helper-cli-codegen-argument-enginedefine-8961bb73cd"></a><code>helper-cli.codegen.argument.enginedefine</code> | <code>-enginedefine</code> | <code>option</code> | no | <code>1</code> | - | - | engine configuration define NAME=VALUE emitted as a macro into EngineConfig.gen.h |
| <a id="entry-helper-cli-codegen-argument-meta-467f731eb8"></a><code>helper-cli.codegen.argument.meta</code> | <code>-meta</code> | <code>option</code> | yes | <code>1</code> | - | - | path to script api metadata (///@ tags) |
| <a id="entry-helper-cli-codegen-argument-commonheader-888570e588"></a><code>helper-cli.codegen.argument.commonheader</code> | <code>-commonheader</code> | <code>option</code> | no | <code>1</code> | - | - | path to common header file |
| <a id="entry-helper-cli-codegen-argument-genoutput-b6e5ab6468"></a><code>helper-cli.codegen.argument.genoutput</code> | <code>-genoutput</code> | <code>option</code> | yes | <code>1</code> | - | - | generated code output dir |
| <a id="entry-helper-cli-codegen-argument-verbose-1ae6caa3b7"></a><code>helper-cli.codegen.argument.verbose</code> | <code>-verbose</code> | <code>option</code> | no | <code>0</code> | - | <code>false</code> | verbose mode |

### Exact top-level `--help` output

```text
usage: codegen.py [-h] -maincfg MAINCFG -buildhash BUILDHASH -devname DEVNAME
                  -nicename NICENAME -embedded EMBEDDED
                  -internalcfg INTERNALCFG [-enginedefine ENGINEDEFINE]
                  -meta META [-commonheader COMMONHEADER] -genoutput GENOUTPUT
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
                        engine configuration define NAME=VALUE emitted as a
                        macro into EngineConfig.gen.h
  -meta META            path to script api metadata (///@ tags)
  -commonheader COMMONHEADER
                        path to common header file
  -genoutput GENOUTPUT  generated code output dir
  -verbose              verbose mode
```

<a id="entry-helper-cli-compile-mono-scripts-ad6011a439"></a>
## Mono script compilation

Compile configured Mono assemblies for engine application roles.

- Stable ID: `helper-cli.compile-mono-scripts`
- Program: `compile-mono-scripts.py`
- Owner: `scripting`
- Audience: `engine-contributor`, `embedding-project-build-system`
- Invocation owner: BuildTools/cmake/stages/ScriptsAndBaking.cmake
- Parser source: [BuildTools/compile-mono-scripts.py](https://github.com/cvet/fonline/blob/master/BuildTools/compile-mono-scripts.py)

### Top-level arguments

| Stable ID | Argument | Kind | Required | Values | Choices | Default | Description |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-compile-mono-scripts-argument-scripts-f97fbb98f7"></a><code>helper-cli.compile-mono-scripts.argument.scripts</code> | <code>-scripts</code> | <code>option</code> | yes | <code>1</code> | - | - | path to scripts directory |
| <a id="entry-helper-cli-compile-mono-scripts-argument-assembly-c17a7110d1"></a><code>helper-cli.compile-mono-scripts.argument.assembly</code> | <code>-assembly</code> | <code>option</code> | no | <code>1</code> | - | - | assembly name |

### Exact top-level `--help` output

```text
usage: compile-mono-scripts.py [-h] -scripts SCRIPTS [-assembly ASSEMBLY]

FOnline scripts generation

options:
  -h, --help          show this help message and exit
  -scripts SCRIPTS    path to scripts directory
  -assembly ASSEMBLY  assembly name
```

<a id="entry-helper-cli-codecoverage-b014400e5e"></a>
## Code coverage

Clean, collect, and report engine code coverage for generated test targets.

- Stable ID: `helper-cli.codecoverage`
- Program: `codecoverage.py`
- Owner: `quality`
- Audience: `engine-contributor`, `ci-maintainer`
- Invocation owner: BuildTools/cmake/stages/Applications.cmake
- Parser source: [BuildTools/codecoverage.py](https://github.com/cvet/fonline/blob/master/BuildTools/codecoverage.py)

### Top-level arguments

No arguments at this level.

### Exact top-level `--help` output

```text
usage: codecoverage.py [-h] {clean,run,report,full} ...

Run and analyze engine code coverage

positional arguments:
  {clean,run,report,full}
    clean               Remove previously collected coverage data and reports
    run                 Run the instrumented test binary and collect coverage
                        data
    report              Generate text and HTML reports from collected coverage
                        data
    full                Clean, run the instrumented binary, and generate
                        reports

options:
  -h, --help            show this help message and exit
```

<a id="entry-helper-cli-codecoverage-command-clean-af34f1698d"></a>
### `clean`

Remove previously collected coverage data and reports

Stable ID: `helper-cli.codecoverage.command.clean`

```text
usage: codecoverage.py clean [-h] --workspace-root WORKSPACE_ROOT
                             --build-dir BUILD_DIR --binary BINARY
                             --backend {gcc,llvm,msvc} --output-dir OUTPUT_DIR
                             ...
```

| Stable ID | Argument | Kind | Required | Values | Choices | Default | Description |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-codecoverage-command-clean-argument-workspace-root-b6d8a2447e"></a><code>helper-cli.codecoverage.command.clean.argument.workspace_root</code> | <code>--workspace-root</code> | <code>option</code> | yes | <code>1</code> | - | - | embedding-project source directory |
| <a id="entry-helper-cli-codecoverage-command-clean-argument-build-dir-094d74e38d"></a><code>helper-cli.codecoverage.command.clean.argument.build_dir</code> | <code>--build-dir</code> | <code>option</code> | yes | <code>1</code> | - | - | configured CMake build directory |
| <a id="entry-helper-cli-codecoverage-command-clean-argument-binary-d3b3cf975b"></a><code>helper-cli.codecoverage.command.clean.argument.binary</code> | <code>--binary</code> | <code>option</code> | yes | <code>1</code> | - | - | instrumented test executable |
| <a id="entry-helper-cli-codecoverage-command-clean-argument-backend-1dd776233e"></a><code>helper-cli.codecoverage.command.clean.argument.backend</code> | <code>--backend</code> | <code>option</code> | yes | <code>1</code> | <code>gcc</code>, <code>llvm</code>, <code>msvc</code> | - | coverage compiler/toolchain backend |
| <a id="entry-helper-cli-codecoverage-command-clean-argument-output-dir-89690c3f10"></a><code>helper-cli.codecoverage.command.clean.argument.output_dir</code> | <code>--output-dir</code> | <code>option</code> | yes | <code>1</code> | - | - | coverage data and report output directory |
| <a id="entry-helper-cli-codecoverage-command-clean-argument-binary-args-51d31087ed"></a><code>helper-cli.codecoverage.command.clean.argument.binary_args</code> | <code>binary_args</code> | <code>positional</code> | no | <code>...</code> | - | - | arguments passed to the test binary |

#### Exact `--help` output

```text
usage: codecoverage.py clean [-h] --workspace-root WORKSPACE_ROOT
                             --build-dir BUILD_DIR --binary BINARY
                             --backend {gcc,llvm,msvc} --output-dir OUTPUT_DIR
                             ...

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

Run the instrumented test binary and collect coverage data

Stable ID: `helper-cli.codecoverage.command.run`

```text
usage: codecoverage.py run [-h] --workspace-root WORKSPACE_ROOT
                           --build-dir BUILD_DIR --binary BINARY
                           --backend {gcc,llvm,msvc} --output-dir OUTPUT_DIR
                           ...
```

| Stable ID | Argument | Kind | Required | Values | Choices | Default | Description |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-codecoverage-command-run-argument-workspace-root-3ea13c9ab2"></a><code>helper-cli.codecoverage.command.run.argument.workspace_root</code> | <code>--workspace-root</code> | <code>option</code> | yes | <code>1</code> | - | - | embedding-project source directory |
| <a id="entry-helper-cli-codecoverage-command-run-argument-build-dir-23ad87643e"></a><code>helper-cli.codecoverage.command.run.argument.build_dir</code> | <code>--build-dir</code> | <code>option</code> | yes | <code>1</code> | - | - | configured CMake build directory |
| <a id="entry-helper-cli-codecoverage-command-run-argument-binary-3d92cbd0fb"></a><code>helper-cli.codecoverage.command.run.argument.binary</code> | <code>--binary</code> | <code>option</code> | yes | <code>1</code> | - | - | instrumented test executable |
| <a id="entry-helper-cli-codecoverage-command-run-argument-backend-da1a5d1400"></a><code>helper-cli.codecoverage.command.run.argument.backend</code> | <code>--backend</code> | <code>option</code> | yes | <code>1</code> | <code>gcc</code>, <code>llvm</code>, <code>msvc</code> | - | coverage compiler/toolchain backend |
| <a id="entry-helper-cli-codecoverage-command-run-argument-output-dir-3fc1672e9a"></a><code>helper-cli.codecoverage.command.run.argument.output_dir</code> | <code>--output-dir</code> | <code>option</code> | yes | <code>1</code> | - | - | coverage data and report output directory |
| <a id="entry-helper-cli-codecoverage-command-run-argument-binary-args-0e1e4b8437"></a><code>helper-cli.codecoverage.command.run.argument.binary_args</code> | <code>binary_args</code> | <code>positional</code> | no | <code>...</code> | - | - | arguments passed to the test binary |

#### Exact `--help` output

```text
usage: codecoverage.py run [-h] --workspace-root WORKSPACE_ROOT
                           --build-dir BUILD_DIR --binary BINARY
                           --backend {gcc,llvm,msvc} --output-dir OUTPUT_DIR
                           ...

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

Generate text and HTML reports from collected coverage data

Stable ID: `helper-cli.codecoverage.command.report`

```text
usage: codecoverage.py report [-h] --workspace-root WORKSPACE_ROOT
                              --build-dir BUILD_DIR --binary BINARY
                              --backend {gcc,llvm,msvc}
                              --output-dir OUTPUT_DIR
                              ...
```

| Stable ID | Argument | Kind | Required | Values | Choices | Default | Description |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-codecoverage-command-report-argument-workspace-root-c368887528"></a><code>helper-cli.codecoverage.command.report.argument.workspace_root</code> | <code>--workspace-root</code> | <code>option</code> | yes | <code>1</code> | - | - | embedding-project source directory |
| <a id="entry-helper-cli-codecoverage-command-report-argument-build-dir-ef1800b8f4"></a><code>helper-cli.codecoverage.command.report.argument.build_dir</code> | <code>--build-dir</code> | <code>option</code> | yes | <code>1</code> | - | - | configured CMake build directory |
| <a id="entry-helper-cli-codecoverage-command-report-argument-binary-ccafd82b7a"></a><code>helper-cli.codecoverage.command.report.argument.binary</code> | <code>--binary</code> | <code>option</code> | yes | <code>1</code> | - | - | instrumented test executable |
| <a id="entry-helper-cli-codecoverage-command-report-argument-backend-927a805169"></a><code>helper-cli.codecoverage.command.report.argument.backend</code> | <code>--backend</code> | <code>option</code> | yes | <code>1</code> | <code>gcc</code>, <code>llvm</code>, <code>msvc</code> | - | coverage compiler/toolchain backend |
| <a id="entry-helper-cli-codecoverage-command-report-argument-output-dir-ef938b1452"></a><code>helper-cli.codecoverage.command.report.argument.output_dir</code> | <code>--output-dir</code> | <code>option</code> | yes | <code>1</code> | - | - | coverage data and report output directory |
| <a id="entry-helper-cli-codecoverage-command-report-argument-binary-args-6f9a492ac0"></a><code>helper-cli.codecoverage.command.report.argument.binary_args</code> | <code>binary_args</code> | <code>positional</code> | no | <code>...</code> | - | - | arguments passed to the test binary |

#### Exact `--help` output

```text
usage: codecoverage.py report [-h] --workspace-root WORKSPACE_ROOT
                              --build-dir BUILD_DIR --binary BINARY
                              --backend {gcc,llvm,msvc}
                              --output-dir OUTPUT_DIR
                              ...

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

Clean, run the instrumented binary, and generate reports

Stable ID: `helper-cli.codecoverage.command.full`

```text
usage: codecoverage.py full [-h] --workspace-root WORKSPACE_ROOT
                            --build-dir BUILD_DIR --binary BINARY
                            --backend {gcc,llvm,msvc} --output-dir OUTPUT_DIR
                            ...
```

| Stable ID | Argument | Kind | Required | Values | Choices | Default | Description |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-codecoverage-command-full-argument-workspace-root-ad42e148ad"></a><code>helper-cli.codecoverage.command.full.argument.workspace_root</code> | <code>--workspace-root</code> | <code>option</code> | yes | <code>1</code> | - | - | embedding-project source directory |
| <a id="entry-helper-cli-codecoverage-command-full-argument-build-dir-0d3d1b2e06"></a><code>helper-cli.codecoverage.command.full.argument.build_dir</code> | <code>--build-dir</code> | <code>option</code> | yes | <code>1</code> | - | - | configured CMake build directory |
| <a id="entry-helper-cli-codecoverage-command-full-argument-binary-f3c0888145"></a><code>helper-cli.codecoverage.command.full.argument.binary</code> | <code>--binary</code> | <code>option</code> | yes | <code>1</code> | - | - | instrumented test executable |
| <a id="entry-helper-cli-codecoverage-command-full-argument-backend-fa1db784c3"></a><code>helper-cli.codecoverage.command.full.argument.backend</code> | <code>--backend</code> | <code>option</code> | yes | <code>1</code> | <code>gcc</code>, <code>llvm</code>, <code>msvc</code> | - | coverage compiler/toolchain backend |
| <a id="entry-helper-cli-codecoverage-command-full-argument-output-dir-7b53f8aad9"></a><code>helper-cli.codecoverage.command.full.argument.output_dir</code> | <code>--output-dir</code> | <code>option</code> | yes | <code>1</code> | - | - | coverage data and report output directory |
| <a id="entry-helper-cli-codecoverage-command-full-argument-binary-args-586e8c3c4a"></a><code>helper-cli.codecoverage.command.full.argument.binary_args</code> | <code>binary_args</code> | <code>positional</code> | no | <code>...</code> | - | - | arguments passed to the test binary |

#### Exact `--help` output

```text
usage: codecoverage.py full [-h] --workspace-root WORKSPACE_ROOT
                            --build-dir BUILD_DIR --binary BINARY
                            --backend {gcc,llvm,msvc} --output-dir OUTPUT_DIR
                            ...

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
## Gameplay test runner

Run ordered multi-process gameplay smoke scenarios with readiness, marker, deadline, cleanup, and JSON-report contracts.

- Stable ID: `helper-cli.gameplay-test-runner`
- Program: `gameplay_test_runner.py`
- Owner: `quality`
- Audience: `game-developer`, `engine-contributor`, `embedding-project-build-system`, `ci-maintainer`
- Invocation owner: embedding-project CMake targets and CI gameplay smoke jobs
- Parser source: [BuildTools/gameplay_test_runner.py](https://github.com/cvet/fonline/blob/master/BuildTools/gameplay_test_runner.py)

### Top-level arguments

| Stable ID | Argument | Kind | Required | Values | Choices | Default | Description |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-gameplay-test-runner-argument-manifest-214c4bd23c"></a><code>helper-cli.gameplay-test-runner.argument.manifest</code> | <code>--manifest</code> | <code>option</code> | yes | <code>1</code> | - | - | scenario manifest path |
| <a id="entry-helper-cli-gameplay-test-runner-argument-value-40c5a38d08"></a><code>helper-cli.gameplay-test-runner.argument.value</code> | <code>--value</code> | <code>option</code> | no | <code>1</code> | - | - | placeholder value; repeat as needed |
| <a id="entry-helper-cli-gameplay-test-runner-argument-report-55cdc99b26"></a><code>helper-cli.gameplay-test-runner.argument.report</code> | <code>--report</code> | <code>option</code> | no | <code>1</code> | - | - | optional JSON result path |

### Exact top-level `--help` output

```text
usage: gameplay_test_runner.py [-h] --manifest MANIFEST [--value KEY=VALUE]
                               [--report REPORT]

Run project-neutral multi-process gameplay smoke scenarios from a checked JSON
manifest.

options:
  -h, --help           show this help message and exit
  --manifest MANIFEST  scenario manifest path
  --value KEY=VALUE    placeholder value; repeat as needed
  --report REPORT      optional JSON result path
```

<a id="entry-helper-cli-ai-control-client-35184e9731"></a>
## AiControl protocol client

Call an Engine-compatible project AiControl bridge through the versioned NDJSON/TCP envelope without exposing shared tokens on the command line.

- Stable ID: `helper-cli.ai-control-client`
- Program: `ai_control_client.py`
- Owner: `tooling`
- Audience: `game-developer`, `engine-contributor`, `tool-developer`, `ci-maintainer`
- Invocation owner: embedding-project AI adapters, protocol smoke tests, and direct developer diagnostics
- Parser source: [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py)

### Top-level arguments

| Stable ID | Argument | Kind | Required | Values | Choices | Default | Description |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-ai-control-client-argument-host-0554537b33"></a><code>helper-cli.ai-control-client.argument.host</code> | <code>--host</code> | <code>option</code> | no | <code>1</code> | - | <code>127.0.0.1</code> | Bridge host. |
| <a id="entry-helper-cli-ai-control-client-argument-port-fb6c2c628e"></a><code>helper-cli.ai-control-client.argument.port</code> | <code>--port</code> | <code>option</code> | no | <code>1</code> | - | <code>43011</code> | Bridge port. |
| <a id="entry-helper-cli-ai-control-client-argument-timeout-3a4b9b5c1e"></a><code>helper-cli.ai-control-client.argument.timeout</code> | <code>--timeout</code> | <code>option</code> | no | <code>1</code> | - | <code>5.0</code> | Socket timeout in seconds. |
| <a id="entry-helper-cli-ai-control-client-argument-token-env-4a7ef4dfd5"></a><code>helper-cli.ai-control-client.argument.token_env</code> | <code>--token-env</code> | <code>option</code> | no | <code>1</code> | - | <code>FONLINE_AI_TOKEN</code> | Environment variable containing the shared token. |
| <a id="entry-helper-cli-ai-control-client-argument-allow-remote-0d33b73a66"></a><code>helper-cli.ai-control-client.argument.allow_remote</code> | <code>--allow-remote</code> | <code>option</code> | no | <code>0</code> | - | <code>false</code> | Permit a non-loopback endpoint; transport remains unencrypted. |

### Exact top-level `--help` output

```text
usage: ai_control_client.py [-h] [--host HOST] [--port PORT]
                            [--timeout TIMEOUT] [--token-env TOKEN_ENV]
                            [--allow-remote]
                            {ping,status,observe,events,act} ...

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
  --allow-remote        Permit a non-loopback endpoint; transport remains
                        unencrypted.
```

<a id="entry-helper-cli-ai-control-client-command-ping-2a253f06de"></a>
### `ping`

Check bridge liveness.

Stable ID: `helper-cli.ai-control-client.command.ping`

```text
usage: ai_control_client.py ping [-h]
```

No arguments at this level.

#### Exact `--help` output

```text
usage: ai_control_client.py ping [-h]

options:
  -h, --help  show this help message and exit
```

<a id="entry-helper-cli-ai-control-client-command-status-71ec51cb72"></a>
### `status`

Read bridge status and queue limits.

Stable ID: `helper-cli.ai-control-client.command.status`

```text
usage: ai_control_client.py status [-h]
```

No arguments at this level.

#### Exact `--help` output

```text
usage: ai_control_client.py status [-h]

options:
  -h, --help  show this help message and exit
```

<a id="entry-helper-cli-ai-control-client-command-observe-38177f229e"></a>
### `observe`

Read the latest project observation.

Stable ID: `helper-cli.ai-control-client.command.observe`

```text
usage: ai_control_client.py observe [-h]
```

No arguments at this level.

#### Exact `--help` output

```text
usage: ai_control_client.py observe [-h]

options:
  -h, --help  show this help message and exit
```

<a id="entry-helper-cli-ai-control-client-command-events-e9dd1890b0"></a>
### `events`

Read events after a sequence cursor.

Stable ID: `helper-cli.ai-control-client.command.events`

```text
usage: ai_control_client.py events [-h] [--after-seq AFTER_SEQ]
                                   [--limit LIMIT]
```

| Stable ID | Argument | Kind | Required | Values | Choices | Default | Description |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-ai-control-client-command-events-argument-after-seq-f5b3920536"></a><code>helper-cli.ai-control-client.command.events.argument.after_seq</code> | <code>--after-seq</code> | <code>option</code> | no | <code>1</code> | - | <code>0</code> | Exclusive event cursor. |
| <a id="entry-helper-cli-ai-control-client-command-events-argument-limit-a61d74a970"></a><code>helper-cli.ai-control-client.command.events.argument.limit</code> | <code>--limit</code> | <code>option</code> | no | <code>1</code> | - | <code>100</code> | Maximum events to return. |

#### Exact `--help` output

```text
usage: ai_control_client.py events [-h] [--after-seq AFTER_SEQ]
                                   [--limit LIMIT]

options:
  -h, --help            show this help message and exit
  --after-seq AFTER_SEQ
                        Exclusive event cursor.
  --limit LIMIT         Maximum events to return.
```

<a id="entry-helper-cli-ai-control-client-command-act-a8414f2a7f"></a>
### `act`

Enqueue one project-defined command.

Stable ID: `helper-cli.ai-control-client.command.act`

```text
usage: ai_control_client.py act [-h] --type TYPE [--target-id TARGET_ID]
                                [--item-id ITEM_ID] [--aux-id AUX_ID] [--x X]
                                [--y Y] [--screen-x SCREEN_X]
                                [--screen-y SCREEN_Y] [--int-arg INT_ARG]
                                [--string-arg STRING_ARG] [--append]
```

| Stable ID | Argument | Kind | Required | Values | Choices | Default | Description |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-ai-control-client-command-act-argument-type-30243d4b4c"></a><code>helper-cli.ai-control-client.command.act.argument.type</code> | <code>--type</code> | <code>option</code> | yes | <code>1</code> | - | - | Project-defined command type. |
| <a id="entry-helper-cli-ai-control-client-command-act-argument-target-id-cc6d8b017b"></a><code>helper-cli.ai-control-client.command.act.argument.target_id</code> | <code>--target-id</code> | <code>option</code> | no | <code>1</code> | - | - | Optional target entity identifier. |
| <a id="entry-helper-cli-ai-control-client-command-act-argument-item-id-82973c779f"></a><code>helper-cli.ai-control-client.command.act.argument.item_id</code> | <code>--item-id</code> | <code>option</code> | no | <code>1</code> | - | - | Optional item entity identifier. |
| <a id="entry-helper-cli-ai-control-client-command-act-argument-aux-id-12c9597c1d"></a><code>helper-cli.ai-control-client.command.act.argument.aux_id</code> | <code>--aux-id</code> | <code>option</code> | no | <code>1</code> | - | - | Optional auxiliary entity identifier. |
| <a id="entry-helper-cli-ai-control-client-command-act-argument-x-4507b45828"></a><code>helper-cli.ai-control-client.command.act.argument.x</code> | <code>--x</code> | <code>option</code> | no | <code>1</code> | - | - | Optional project world X coordinate. |
| <a id="entry-helper-cli-ai-control-client-command-act-argument-y-4125d48b44"></a><code>helper-cli.ai-control-client.command.act.argument.y</code> | <code>--y</code> | <code>option</code> | no | <code>1</code> | - | - | Optional project world Y coordinate. |
| <a id="entry-helper-cli-ai-control-client-command-act-argument-screen-x-180c94f1ff"></a><code>helper-cli.ai-control-client.command.act.argument.screen_x</code> | <code>--screen-x</code> | <code>option</code> | no | <code>1</code> | - | - | Optional screen X coordinate. |
| <a id="entry-helper-cli-ai-control-client-command-act-argument-screen-y-e60dd59502"></a><code>helper-cli.ai-control-client.command.act.argument.screen_y</code> | <code>--screen-y</code> | <code>option</code> | no | <code>1</code> | - | - | Optional screen Y coordinate. |
| <a id="entry-helper-cli-ai-control-client-command-act-argument-int-arg-d52ef6d5f8"></a><code>helper-cli.ai-control-client.command.act.argument.int_arg</code> | <code>--int-arg</code> | <code>option</code> | no | <code>1</code> | - | - | Optional integer payload. |
| <a id="entry-helper-cli-ai-control-client-command-act-argument-string-arg-a167eef2cd"></a><code>helper-cli.ai-control-client.command.act.argument.string_arg</code> | <code>--string-arg</code> | <code>option</code> | no | <code>1</code> | - | - | Optional string payload. |
| <a id="entry-helper-cli-ai-control-client-command-act-argument-append-460a176550"></a><code>helper-cli.ai-control-client.command.act.argument.append</code> | <code>--append</code> | <code>option</code> | no | <code>0</code> | - | <code>false</code> | Request project queue append semantics. |

#### Exact `--help` output

```text
usage: ai_control_client.py act [-h] --type TYPE [--target-id TARGET_ID]
                                [--item-id ITEM_ID] [--aux-id AUX_ID] [--x X]
                                [--y Y] [--screen-x SCREEN_X]
                                [--screen-y SCREEN_Y] [--int-arg INT_ARG]
                                [--string-arg STRING_ARG] [--append]

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
## Windows 7 import validation

Inspect linked PE files and reject imports that are unavailable on Windows 7.

- Stable ID: `helper-cli.windows7-import-check`
- Program: `check_windows7_imports.py`
- Owner: `quality`
- Audience: `engine-contributor`, `embedding-project-build-system`, `release-operator`
- Invocation owner: embedding-project Windows 7 CI and release validation
- Parser source: [BuildTools/check_windows7_imports.py](https://github.com/cvet/fonline/blob/master/BuildTools/check_windows7_imports.py)

### Top-level arguments

| Stable ID | Argument | Kind | Required | Values | Choices | Default | Description |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-windows7-import-check-argument-binaries-90e898370f"></a><code>helper-cli.windows7-import-check.argument.binaries</code> | <code>binaries</code> | <code>positional</code> | yes | <code>+</code> | - | - | linked PE executable or DLL to inspect |

### Exact top-level `--help` output

```text
usage: check_windows7_imports.py [-h] binaries [binaries ...]

Reject CreateFile2 from Windows 7-compatible PE binaries

positional arguments:
  binaries    linked PE executable or DLL to inspect

options:
  -h, --help  show this help message and exit
```

<a id="entry-helper-cli-android-device-ab99179ae9"></a>
## Android device control

Discover, connect, install, launch, stop, and inspect Android Wi-Fi devices through adb.

- Stable ID: `helper-cli.android-device`
- Program: `android_device.py`
- Owner: `platform`
- Audience: `game-developer`, `engine-contributor`
- Invocation owner: embedding-project Android tasks and direct developer use
- Parser source: [BuildTools/android_device.py](https://github.com/cvet/fonline/blob/master/BuildTools/android_device.py)

### Top-level arguments

| Stable ID | Argument | Kind | Required | Values | Choices | Default | Description |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-android-device-argument-workspace-root-8726d0799e"></a><code>helper-cli.android-device.argument.workspace_root</code> | <code>--workspace-root</code> | <code>option</code> | no | <code>1</code> | - | - | Workspace directory path containing android-sdk and android-debug |

### Exact top-level `--help` output

```text
usage: android_device.py [-h] [--workspace-root WORKSPACE_ROOT]
                         {discover,connect,install,launch,launch-game,stop,logcat} ...

Android Wi-Fi device helper for BuildTools tasks

positional arguments:
  {discover,connect,install,launch,launch-game,stop,logcat}
    discover            List Android Wi-Fi devices discovered through adb mdns
    connect             Connect to an Android Wi-Fi device and cache the
                        endpoint
    install             Install an APK on the selected Android Wi-Fi device
    launch              Launch an Android activity on the selected device
    launch-game         Launch the Android game activity and pass
                        RemoteSceneLaunch server host override
    stop                Force-stop an Android package on the selected device
    logcat              Stream logcat from the selected device

options:
  -h, --help            show this help message and exit
  --workspace-root WORKSPACE_ROOT
                        Workspace directory path containing android-sdk and
                        android-debug
```

<a id="entry-helper-cli-android-device-command-discover-f8951fbd7c"></a>
### `discover`

List Android Wi-Fi devices discovered through adb mdns

Stable ID: `helper-cli.android-device.command.discover`

```text
usage: android_device.py discover [-h]
```

No arguments at this level.

#### Exact `--help` output

```text
usage: android_device.py discover [-h]

options:
  -h, --help  show this help message and exit
```

<a id="entry-helper-cli-android-device-command-connect-6083ee40ae"></a>
### `connect`

Connect to an Android Wi-Fi device and cache the endpoint

Stable ID: `helper-cli.android-device.command.connect`

```text
usage: android_device.py connect [-h] [--device DEVICE]
```

| Stable ID | Argument | Kind | Required | Values | Choices | Default | Description |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-android-device-command-connect-argument-device-933a2c37f1"></a><code>helper-cli.android-device.command.connect.argument.device</code> | <code>--device</code> | <code>option</code> | no | <code>1</code> | - | - | Device IP[:port]; if omitted, auto-discovery and interactive selection are used |

#### Exact `--help` output

```text
usage: android_device.py connect [-h] [--device DEVICE]

options:
  -h, --help       show this help message and exit
  --device DEVICE  Device IP[:port]; if omitted, auto-discovery and
                   interactive selection are used
```

<a id="entry-helper-cli-android-device-command-install-20d466716c"></a>
### `install`

Install an APK on the selected Android Wi-Fi device

Stable ID: `helper-cli.android-device.command.install`

```text
usage: android_device.py install [-h] --apk APK [--device DEVICE]
```

| Stable ID | Argument | Kind | Required | Values | Choices | Default | Description |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-android-device-command-install-argument-apk-052b021543"></a><code>helper-cli.android-device.command.install.argument.apk</code> | <code>--apk</code> | <code>option</code> | yes | <code>1</code> | - | - | APK path |
| <a id="entry-helper-cli-android-device-command-install-argument-device-117a72e901"></a><code>helper-cli.android-device.command.install.argument.device</code> | <code>--device</code> | <code>option</code> | no | <code>1</code> | - | - | Device IP[:port]; if omitted, cached endpoint or discovery is used |

#### Exact `--help` output

```text
usage: android_device.py install [-h] --apk APK [--device DEVICE]

options:
  -h, --help       show this help message and exit
  --apk APK        APK path
  --device DEVICE  Device IP[:port]; if omitted, cached endpoint or discovery
                   is used
```

<a id="entry-helper-cli-android-device-command-launch-7c998b622c"></a>
### `launch`

Launch an Android activity on the selected device

Stable ID: `helper-cli.android-device.command.launch`

```text
usage: android_device.py launch [-h] --activity ACTIVITY [--device DEVICE]
```

| Stable ID | Argument | Kind | Required | Values | Choices | Default | Description |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-android-device-command-launch-argument-activity-8e9055617d"></a><code>helper-cli.android-device.command.launch.argument.activity</code> | <code>--activity</code> | <code>option</code> | yes | <code>1</code> | - | - | Fully qualified activity component, e.g. com.example.game/.FOnlineActivity |
| <a id="entry-helper-cli-android-device-command-launch-argument-device-fb19f4776c"></a><code>helper-cli.android-device.command.launch.argument.device</code> | <code>--device</code> | <code>option</code> | no | <code>1</code> | - | - | Device IP[:port]; if omitted, cached endpoint or discovery is used |

#### Exact `--help` output

```text
usage: android_device.py launch [-h] --activity ACTIVITY [--device DEVICE]

options:
  -h, --help           show this help message and exit
  --activity ACTIVITY  Fully qualified activity component, e.g.
                       com.example.game/.FOnlineActivity
  --device DEVICE      Device IP[:port]; if omitted, cached endpoint or
                       discovery is used
```

<a id="entry-helper-cli-android-device-command-launch-game-408e50236e"></a>
### `launch-game`

Launch the Android game activity and pass RemoteSceneLaunch server host override

Stable ID: `helper-cli.android-device.command.launch-game`

```text
usage: android_device.py launch-game [-h] --activity ACTIVITY
                                     [--device DEVICE]
                                     [--server-host SERVER_HOST]
```

| Stable ID | Argument | Kind | Required | Values | Choices | Default | Description |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-android-device-command-launch-game-argument-activity-428147ad37"></a><code>helper-cli.android-device.command.launch-game.argument.activity</code> | <code>--activity</code> | <code>option</code> | yes | <code>1</code> | - | - | Fully qualified activity component, e.g. com.example.game/.FOnlineActivity |
| <a id="entry-helper-cli-android-device-command-launch-game-argument-device-d749dc44b6"></a><code>helper-cli.android-device.command.launch-game.argument.device</code> | <code>--device</code> | <code>option</code> | no | <code>1</code> | - | - | Device IP[:port]; if omitted, cached endpoint or discovery is used |
| <a id="entry-helper-cli-android-device-command-launch-game-argument-server-host-3f5230a55e"></a><code>helper-cli.android-device.command.launch-game.argument.server_host</code> | <code>--server-host</code> | <code>option</code> | no | <code>1</code> | - | - | Host IP or name for ClientNetwork.ServerHost; if omitted, auto-detected from the route to the selected device |

#### Exact `--help` output

```text
usage: android_device.py launch-game [-h] --activity ACTIVITY
                                     [--device DEVICE]
                                     [--server-host SERVER_HOST]

options:
  -h, --help            show this help message and exit
  --activity ACTIVITY   Fully qualified activity component, e.g.
                        com.example.game/.FOnlineActivity
  --device DEVICE       Device IP[:port]; if omitted, cached endpoint or
                        discovery is used
  --server-host SERVER_HOST
                        Host IP or name for ClientNetwork.ServerHost; if
                        omitted, auto-detected from the route to the selected
                        device
```

<a id="entry-helper-cli-android-device-command-stop-74b36b2258"></a>
### `stop`

Force-stop an Android package on the selected device

Stable ID: `helper-cli.android-device.command.stop`

```text
usage: android_device.py stop [-h] --package PACKAGE_NAME [--device DEVICE]
```

| Stable ID | Argument | Kind | Required | Values | Choices | Default | Description |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-android-device-command-stop-argument-package-name-8a29334041"></a><code>helper-cli.android-device.command.stop.argument.package_name</code> | <code>--package</code> | <code>option</code> | yes | <code>1</code> | - | - | Android package name |
| <a id="entry-helper-cli-android-device-command-stop-argument-device-8c3cd8d602"></a><code>helper-cli.android-device.command.stop.argument.device</code> | <code>--device</code> | <code>option</code> | no | <code>1</code> | - | - | Device IP[:port]; if omitted, cached endpoint or discovery is used |

#### Exact `--help` output

```text
usage: android_device.py stop [-h] --package PACKAGE_NAME [--device DEVICE]

options:
  -h, --help            show this help message and exit
  --package PACKAGE_NAME
                        Android package name
  --device DEVICE       Device IP[:port]; if omitted, cached endpoint or
                        discovery is used
```

<a id="entry-helper-cli-android-device-command-logcat-5f28df274e"></a>
### `logcat`

Stream logcat from the selected device

Stable ID: `helper-cli.android-device.command.logcat`

```text
usage: android_device.py logcat [-h] [--device DEVICE]
```

| Stable ID | Argument | Kind | Required | Values | Choices | Default | Description |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-android-device-command-logcat-argument-device-71db85d287"></a><code>helper-cli.android-device.command.logcat.argument.device</code> | <code>--device</code> | <code>option</code> | no | <code>1</code> | - | - | Device IP[:port]; if omitted, cached endpoint or discovery is used |

#### Exact `--help` output

```text
usage: android_device.py logcat [-h] [--device DEVICE]

options:
  -h, --help       show this help message and exit
  --device DEVICE  Device IP[:port]; if omitted, cached endpoint or discovery
                   is used
```

<a id="entry-helper-cli-simple-web-server-58fbf70798"></a>
## Local web server

Serve a packaged web client from a no-cache local HTTP server.

- Stable ID: `helper-cli.simple-web-server`
- Program: `simple-web-server.py`
- Owner: `platform`
- Audience: `game-developer`, `release-operator`
- Invocation owner: BuildTools/package.py WebServer payload
- Parser source: [BuildTools/web/simple-web-server.py](https://github.com/cvet/fonline/blob/master/BuildTools/web/simple-web-server.py)

### Top-level arguments

| Stable ID | Argument | Kind | Required | Values | Choices | Default | Description |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-simple-web-server-argument-port-20ae9fb491"></a><code>helper-cli.simple-web-server.argument.port</code> | <code>--port</code> | <code>option</code> | no | <code>1</code> | - | <code>7000</code> | web server port |
| <a id="entry-helper-cli-simple-web-server-argument-fork-b1141869ad"></a><code>helper-cli.simple-web-server.argument.fork</code> | <code>--fork</code> | <code>option</code> | no | <code>0</code> | - | <code>false</code> | fork process |

### Exact top-level `--help` output

```text
usage: simple-web-server.py [-h] [--port PORT] [--fork]

Simple HTTP server

options:
  -h, --help   show this help message and exit
  --port PORT  web server port
  --fork       fork process
```

<a id="entry-helper-cli-createmsi-18899fd2a5"></a>
## MSI creation

Build an MSI installer from a package-generated WiX definition.

- Stable ID: `helper-cli.createmsi`
- Program: `createmsi.py`
- Owner: `build-release`
- Audience: `release-operator`, `engine-contributor`
- Invocation owner: BuildTools/package.py Wix pack
- Parser source: [BuildTools/msicreator/createmsi.py](https://github.com/cvet/fonline/blob/master/BuildTools/msicreator/createmsi.py)

### Top-level arguments

| Stable ID | Argument | Kind | Required | Values | Choices | Default | Description |
| --- | --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-createmsi-argument-jsonfile-385f92bb78"></a><code>helper-cli.createmsi.argument.jsonfile</code> | <code>definition.json</code> | <code>positional</code> | yes | <code>1</code> | - | - | bare WiX package definition filename in the working directory |

### Exact top-level `--help` output

```text
usage: createmsi.py [-h] definition.json

Build an MSI package from a WiX definition

positional arguments:
  definition.json  bare WiX package definition filename in the working
                   directory

options:
  -h, --help       show this help message and exit
```
