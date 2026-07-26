---
title: Generated BuildTools CLI Reference
document_id: generated-cli-index
locale: en
generated: true
---

# Generated BuildTools CLI Reference

> Generated reference. Do not edit this page directly. Update `BuildTools/buildtools.py`, then run `python BuildTools/docs_cli.py --write`.

[Reference index](index.md) | [Commands](commands.md) | [Canonical JSON model](../cli.json) | [Generation contract](../../GeneratedApiAndMetadata.md)

This reference is generated from the same `argparse.ArgumentParser` used by the executable BuildTools entry point. Parser changes therefore make the committed model and pages stale.

## Contract status

| Field | Value |
| --- | --- |
| Stability | <code>internal</code> |
| Since | Not declared |
| Support policy | No versioned CLI support line is declared; pin an engine revision in automation. |
| Source parser | [BuildTools/buildtools.py](https://github.com/cvet/fonline/blob/master/BuildTools/buildtools.py) |
| Contract digest | <code>de65387247c562f7232b85e632f7f23ceb35f75e88d35b5989bcdc2fbb2ff6b1</code> |

## Coverage

| Reference | Entries | Purpose |
| --- | --- | --- |
| [Commands](commands.md) | 12 | Commands with 24 command-specific arguments. |
| Global arguments | 0 | Arguments accepted before a command. |

## Top-level help

```text
usage: buildtools.py [-h]
                     {env,build,validate,setup-mono,format-source,toolset,build-auxiliary,prepare-workspace,package-web-debug,package-android-debug,host-check,prepare-host-workspace} ...

Shared BuildTools helpers

positional arguments:
  {env,build,validate,setup-mono,format-source,toolset,build-auxiliary,prepare-workspace,package-web-debug,package-android-debug,host-check,prepare-host-workspace}
    env                 resolve BuildTools environment
    build               configure and build a target
    validate            configure and validate scenarios
    setup-mono          prepare mono runtime
    format-source       format engine source files
    toolset             build an existing toolset target
    build-auxiliary     build a separately packaged auxiliary tool
    prepare-workspace   prepare shared workspace parts
    package-web-debug   package the local web debug client
    package-android-debug
                        package the local android debug client
    host-check          check host prerequisites
    prepare-host-workspace
                        prepare host workspace and prerequisites

options:
  -h, --help            show this help message and exit
```

## Boundary

Included:

- top-level commands and arguments exposed by BuildTools/buildtools.py create_parser()
- argparse defaults, choices, cardinality, descriptions, usage, and --help output

Excluded from this slice:

- helper-script command lines outside BuildTools/buildtools.py
- package.py declaration and payload contracts
- validation-target semantics and internal Python helpers

Excluded surfaces remain implementation details until an owning parser-backed contract and compatibility policy are published.
