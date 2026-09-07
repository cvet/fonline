---
title: Package Payloads and Artifacts
document_id: generated-package-payloads
locale: en
generated: true
---

# Package Payloads and Artifacts

> Generated reference. Do not edit this page directly. Update `BuildTools/PackageInterface.json` or `BuildTools/package.py`, then run `python BuildTools/docs_package.py --write`.

[Index](index.md) | [Declaration](declaration.md) | [Matrix](matrix.md) | [Payloads](payloads.md) | [CLI](cli.md) | [Canonical JSON](../../../generated/package.json)

The packager stages one target payload, applies binary/resource transformations, emits artifact packs in a fixed finalization order, and removes the staged directory unless `Raw` retains it.

## Platform payloads

| Stable ID | Platform | Status | Payload |
| --- | --- | --- | --- |
| <a id="entry-package-payload-windows-6b9618ed08"></a><code>package.payload.Windows</code> | <code>Windows</code> | <code>implemented</code> | Patched PE executable or DLL, companion runtime files, PDBs when present, and resource ZIP directories unless NoRes is selected. win32-win7 and win64-win7 read the canonical win32/win64 binary entry selected by the same per-binary POSTFIX. |
| <a id="entry-package-payload-linux-92e6941cc5"></a><code>package.payload.Linux</code> | <code>Linux</code> | <code>implemented</code> | Patched executable or shared runtime, companion files, and resource ZIP directories unless NoRes is selected. |
| <a id="entry-package-payload-android-094c0cd541"></a><code>package.payload.Android</code> | <code>Android</code> | <code>implemented</code> | Generated Gradle project containing libmain.so per ABI and baked resources under app assets; Apk optionally emits a sibling APK. |
| <a id="entry-package-payload-web-04b1334012"></a><code>package.payload.Web</code> | <code>Web</code> | <code>implemented</code> | JavaScript, patched Wasm, HTML shell, Resources.data/Resources.js preload files, and optional local server helper. |
| <a id="entry-package-payload-macos-acb0bdca5d"></a><code>package.payload.macOS</code> | <code>macOS</code> | <code>unsupported</code> | No payload is emitted in the current repository state. |
| <a id="entry-package-payload-ios-c0357994eb"></a><code>package.payload.iOS</code> | <code>iOS</code> | <code>unsupported</code> | No payload is emitted in the current repository state. |

## Output-producing packs

| Stable ID | Pack | Status | Output | Effect |
| --- | --- | --- | --- | --- |
| <a id="entry-package-pack-raw-d7354ed4ba"></a><code>package.pack.Raw</code> | <code>Raw</code> | <code>implemented</code> | <code>&lt;target-output&gt;</code> | Retain the staged target directory after finalization. |
| <a id="entry-package-pack-zip-3dcbf3c445"></a><code>package.pack.Zip</code> | <code>Zip</code> | <code>implemented</code> | <code>&lt;target-output&gt;.zip</code> | Create a deterministic ZIP beside the staged target directory. |
| <a id="entry-package-pack-singlezip-0c96b3b8c6"></a><code>package.pack.SingleZip</code> | <code>SingleZip</code> | <code>implemented</code> | <code>&lt;output&gt;/&lt;output-name&gt;.zip</code> | Append the staged target directory to one package-wide ZIP; identical same-name members are stored once, while conflicting contents fail packaging. |
| <a id="entry-package-pack-tar-e6de1a9557"></a><code>package.pack.Tar</code> | <code>Tar</code> | <code>implemented</code> | <code>&lt;target-output&gt;.tar</code> | Create an uncompressed tar archive. |
| <a id="entry-package-pack-targz-41851c8dfc"></a><code>package.pack.TarGz</code> | <code>TarGz</code> | <code>implemented</code> | <code>&lt;target-output&gt;.tar.gz</code> | Create a gzip-compressed tar archive. |
| <a id="entry-package-pack-root-e56dc75506"></a><code>package.pack.Root</code> | <code>Root</code> | <code>implemented</code> | <code>&lt;output&gt;</code> | Merge the staged target directory into the package output root. |
| <a id="entry-package-pack-wix-a1cec8e2e5"></a><code>package.pack.Wix</code> | <code>Wix</code> | <code>implemented</code> | <code>&lt;output&gt;/&lt;nice-name&gt;[_&lt;postfix&gt;].msi</code> | Build a required MSI from the staged Windows client payload; POSTFIX is appended to the installer base name. |
| <a id="entry-package-pack-apk-961529b696"></a><code>package.pack.Apk</code> | <code>Apk</code> | <code>implemented</code> | <code>&lt;output&gt;/&lt;target-output-name&gt;.apk</code> | Run Gradle and copy the selected signed/debug APK beside the staged project. |

A package invocation must select at least one implemented output-producing pack. Modifier-only lists and unknown, duplicate, placeholder, unsupported-platform, or target-incompatible tokens fail before output staging.
