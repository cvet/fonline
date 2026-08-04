---
title: Generated Helper CLI Reference
document_id: generated-helper-cli-index
locale: en
generated: true
---

# Generated Helper CLI Reference

> Generated reference. Do not edit this page directly. Update `BuildTools/HelperCliInterface.json` or the owning executable parser, then run `python BuildTools/docs_helper_cli.py --write`.

[Reference index](index.md) | [Commands](commands.md) | [Canonical JSON model](../../../generated/helper-cli.json) | [Generation contract](../metadata/)

This reference is generated from the `argparse.ArgumentParser` objects used by executable engine helper scripts. The manifest owns purpose and audience; source parsers own executable syntax.

## Contract status

| Field | Value |
| --- | --- |
| Stability | <code>internal</code> |
| Since | Not declared |
| Support policy | Helper command lines are revision-pinned implementation interfaces; automation must pin an engine revision. |
| Source manifest | [BuildTools/HelperCliInterface.json](https://github.com/cvet/fonline/blob/master/BuildTools/HelperCliInterface.json) |
| Contract digest | <code>fc411f8694ac5fe27dc4ff6cb8cd6491048554bb7117ce7dbff3299c45a34420</code> |

## Inventory

| Stable ID | Helper | Owner | Invocation owner | Parser source | Commands / global args |
| --- | --- | --- | --- | --- | --- |
| <a id="entry-helper-cli-codegen-60abdf415d"></a><code>helper-cli.codegen</code> | [Code generation](commands.md#entry-helper-cli-codegen-60abdf415d) | <code>build-release</code> | BuildTools/cmake/stages/Codegen.cmake | [BuildTools/codegen.py](https://github.com/cvet/fonline/blob/master/BuildTools/codegen.py) | 0 / 11 |
| <a id="entry-helper-cli-compile-mono-scripts-ad6011a439"></a><code>helper-cli.compile-mono-scripts</code> | [Mono script compilation](commands.md#entry-helper-cli-compile-mono-scripts-ad6011a439) | <code>scripting</code> | BuildTools/cmake/stages/ScriptsAndBaking.cmake | [BuildTools/compile-mono-scripts.py](https://github.com/cvet/fonline/blob/master/BuildTools/compile-mono-scripts.py) | 0 / 2 |
| <a id="entry-helper-cli-codecoverage-b014400e5e"></a><code>helper-cli.codecoverage</code> | [Code coverage](commands.md#entry-helper-cli-codecoverage-b014400e5e) | <code>quality</code> | BuildTools/cmake/stages/Applications.cmake | [BuildTools/codecoverage.py](https://github.com/cvet/fonline/blob/master/BuildTools/codecoverage.py) | 4 / 0 |
| <a id="entry-helper-cli-gameplay-test-runner-b34ed8deb4"></a><code>helper-cli.gameplay-test-runner</code> | [Gameplay test runner](commands.md#entry-helper-cli-gameplay-test-runner-b34ed8deb4) | <code>quality</code> | embedding-project CMake targets and CI gameplay smoke jobs | [BuildTools/gameplay_test_runner.py](https://github.com/cvet/fonline/blob/master/BuildTools/gameplay_test_runner.py) | 0 / 3 |
| <a id="entry-helper-cli-ai-control-client-35184e9731"></a><code>helper-cli.ai-control-client</code> | [AiControl protocol client](commands.md#entry-helper-cli-ai-control-client-35184e9731) | <code>tooling</code> | embedding-project AI adapters, protocol smoke tests, and direct developer diagnostics | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) | 5 / 5 |
| <a id="entry-helper-cli-windows7-import-check-a0c7e4cb59"></a><code>helper-cli.windows7-import-check</code> | [Windows 7 import validation](commands.md#entry-helper-cli-windows7-import-check-a0c7e4cb59) | <code>quality</code> | embedding-project Windows 7 CI and release validation | [BuildTools/check_windows7_imports.py](https://github.com/cvet/fonline/blob/master/BuildTools/check_windows7_imports.py) | 0 / 1 |
| <a id="entry-helper-cli-android-device-ab99179ae9"></a><code>helper-cli.android-device</code> | [Android device control](commands.md#entry-helper-cli-android-device-ab99179ae9) | <code>platform</code> | embedding-project Android tasks and direct developer use | [BuildTools/android_device.py](https://github.com/cvet/fonline/blob/master/BuildTools/android_device.py) | 7 / 1 |
| <a id="entry-helper-cli-simple-web-server-58fbf70798"></a><code>helper-cli.simple-web-server</code> | [Local web server](commands.md#entry-helper-cli-simple-web-server-58fbf70798) | <code>platform</code> | BuildTools/package.py WebServer payload | [BuildTools/web/simple-web-server.py](https://github.com/cvet/fonline/blob/master/BuildTools/web/simple-web-server.py) | 0 / 2 |
| <a id="entry-helper-cli-createmsi-18899fd2a5"></a><code>helper-cli.createmsi</code> | [MSI creation](commands.md#entry-helper-cli-createmsi-18899fd2a5) | <code>build-release</code> | BuildTools/package.py Wix pack | [BuildTools/msicreator/createmsi.py](https://github.com/cvet/fonline/blob/master/BuildTools/msicreator/createmsi.py) | 0 / 1 |

## Coverage

The model contains 9 helpers, 16 subcommands, 26 global arguments, and 48 subcommand arguments.

Included:

- engine-owned Python helper scripts that expose a top-level create_parser() factory
- exact argparse usage, help output, arguments, subcommands, ownership, audience, and invocation context

Excluded:

- the separately modeled BuildTools/buildtools.py command line
- the separately modeled BuildTools/package.py command line and package declaration contract
- documentation generators, tests, library-only modules, shell scripts, and embedding-project tools
