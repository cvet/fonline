---
title: Generated BuildTools CLI Reference
document_id: generated-cli-index
locale: en
generated: true
---

# Generated BuildTools CLI Reference

> Generated reference. Do not edit this page directly. Update `BuildTools/buildtools.py`, then run `python BuildTools/docs_cli.py --write`.

[Reference index](index.md) | [Commands](commands.md) | [Canonical JSON model](../../../generated/cli.json) | [Generation contract](../metadata/)

This reference is generated from the same `argparse.ArgumentParser` used by the executable BuildTools entry point. Parser changes therefore make the committed model and pages stale.

## Contract status

| Field | Value |
| --- | --- |
| Stability | <code>internal</code> |
| Since | Not declared |
| Support policy | No versioned CLI support line is declared; pin an engine revision in automation. |
| Source parser | [BuildTools/buildtools.py](https://github.com/cvet/fonline/blob/master/BuildTools/buildtools.py) |
| Contract digest | <code>5079db447e1798d3ebc361db85498e4e22ef94e756573587aea998593d44a363</code> |

## Coverage

| Reference | Entries | Purpose |
| --- | --- | --- |
| [Commands](commands.md) | 15 | Commands with 39 command-specific arguments. |
| Global arguments | 0 | Arguments accepted before a command. |

## Top-level help

```text
usage: buildtools.py [-h]
                     {env,build,validate,setup-mono,format-source,format-content,toolset,build-auxiliary,prepare-workspace,repair-checkout-case,package-web-debug,package-android-debug,managed-project-path,host-check,prepare-host-workspace} ...

Shared BuildTools helpers

positional arguments:
  {env,build,validate,setup-mono,format-source,format-content,toolset,build-auxiliary,prepare-workspace,repair-checkout-case,package-web-debug,package-android-debug,managed-project-path,host-check,prepare-host-workspace}
    env                 resolve BuildTools environment
    build               configure and build a target
    validate            configure and validate scenarios
    setup-mono          prepare mono runtime
    format-source       format engine source files
    format-content      format project .fomain, prototype and optional AngelScript files
    toolset             build an existing toolset target
    build-auxiliary     build a separately packaged auxiliary tool
    prepare-workspace   prepare shared workspace parts
    repair-checkout-case
                        realign working-tree entry names with the git index
    package-web-debug   package the local web debug client
    package-android-debug
                        package the local android debug client
    managed-project-path
                        print the generated managed C# project and solution paths
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
