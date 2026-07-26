---
title: Package Declaration Grammar
document_id: generated-package-declaration
locale: en
generated: true
---

# Package Declaration Grammar

> Generated reference. Do not edit this page directly. Update `BuildTools/PackageInterface.json` or `BuildTools/package.py`, then run `python BuildTools/docs_package.py --write`.

[Index](index.md) | [Declaration](declaration.md) | [Matrix](matrix.md) | [Payloads](payloads.md) | [CLI](cli.md) | [Canonical JSON](../package.json)

Declare a named package target and one or more binary payloads.

Stable ID: `package.declaration.DefinePackage`

Source: [BuildTools/cmake/helpers/Build.cmake](https://github.com/cvet/fonline/blob/master/BuildTools/cmake/helpers/Build.cmake); consumer: [BuildTools/cmake/stages/Packages.cmake](https://github.com/cvet/fonline/blob/master/BuildTools/cmake/stages/Packages.cmake).

```cmake
DefinePackage(<name>
    CONFIG <config>
    BINARY <target> <platform> <arch[+arch...]> <pack[+pack...]> [POSTFIX <value>]
    [BINARY ...]
)
```

| Stable ID | Clause | Arguments | Required | Repeatable | Purpose |
| --- | --- | --- | --- | --- | --- |
| <a id="entry-package-declaration-definepackage-clause-config-4d3ab33f4f"></a><code>package.declaration.DefinePackage.clause.CONFIG</code> | <code>CONFIG</code> | <code>&lt;config&gt;</code> | yes | no | Set the package-wide default config. |
| <a id="entry-package-declaration-definepackage-clause-binary-94febbf53b"></a><code>package.declaration.DefinePackage.clause.BINARY</code> | <code>BINARY</code> | <code>&lt;target&gt; &lt;platform&gt; &lt;arch&gt; &lt;pack&gt;</code> | yes | yes | Add one target/platform/architecture payload, optionally followed by POSTFIX and a value that apply together only to this binary entry. |

## Per-binary modifiers

| Stable ID | Modifier | Value | Default | Purpose |
| --- | --- | --- | --- | --- |
| <a id="entry-package-binary-option-postfix-5979921a13"></a><code>package.binary-option.POSTFIX</code> | <code>POSTFIX</code> | <code>string</code> | <code>empty</code> | Select the matching postfixed binary input and isolate this entry's target directory, packaged build name, runtime-update payloads, and MSI name without affecting sibling BINARY entries. |

`DefinePackage` requires `CONFIG`. Each `BINARY` becomes one `package.py` invocation. `POSTFIX` is optional and belongs only to the immediately preceding `BINARY`; it has no package-wide fallback.
