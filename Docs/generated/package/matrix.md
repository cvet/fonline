---
title: Package Targets, Platforms, and Packs
document_id: generated-package-matrix
locale: en
generated: true
---

# Package Targets, Platforms, and Packs

> Generated reference. Do not edit this page directly. Update `BuildTools/PackageInterface.json` or `BuildTools/package.py`, then run `python BuildTools/docs_package.py --write`.

[Index](index.md) | [Declaration](declaration.md) | [Matrix](matrix.md) | [Payloads](payloads.md) | [CLI](cli.md) | [Canonical JSON](../package.json)

## Targets

| Stable ID | Target | Resources | Required packs | Purpose |
| --- | --- | --- | --- | --- |
| <a id="entry-package-target-server-aa345c8bbf"></a><code>package.target.Server</code> | <code>Server</code> | <code>server-and-client</code> | - | Server binary plus server resources and client update resource packs. |
| <a id="entry-package-target-client-6dc545ee39"></a><code>package.target.Client</code> | <code>Client</code> | <code>client</code> | - | Client host/runtime binary and client resources. |
| <a id="entry-package-target-editor-fc4891f14c"></a><code>package.target.Editor</code> | <code>Editor</code> | <code>none</code> | <code>NoRes</code> | Editor binary without packaged game resources. |
| <a id="entry-package-target-mapper-2b4270f627"></a><code>package.target.Mapper</code> | <code>Mapper</code> | <code>none</code> | <code>NoRes</code> | Mapper binary without packaged game resources. |
| <a id="entry-package-target-baker-a25eccc9ba"></a><code>package.target.Baker</code> | <code>Baker</code> | <code>none</code> | <code>NoRes</code> | Baker executable or library without packaged game resources. |

## Platforms

| Stable ID | Platform | Status | Architectures | Targets | Payload |
| --- | --- | --- | --- | --- | --- |
| <a id="entry-package-platform-windows-8e4a2bee4e"></a><code>package.platform.Windows</code> | <code>Windows</code> | <code>implemented</code> | <code>win32</code>, <code>win64</code>, <code>win32-win7</code>, <code>win64-win7</code>, <code>arm64</code> | <code>Server</code>, <code>Client</code>, <code>Editor</code>, <code>Mapper</code>, <code>Baker</code> | Native PE payloads with optional runtime DLLs, symbols, archives, and MSI. The -win7 keys resolve to canonical win32/win64 binary architectures and require a matching explicit POSTFIX when the build output is postfixed. arm64 is accepted as an existing package input, but the standard BuildTools platform registry does not provide a Windows arm64 build lane. |
| <a id="entry-package-platform-linux-35772b1287"></a><code>package.platform.Linux</code> | <code>Linux</code> | <code>implemented</code> | <code>x64</code>, <code>arm64</code>, <code>x86</code>, <code>arm</code> | <code>Server</code>, <code>Client</code>, <code>Editor</code>, <code>Mapper</code>, <code>Baker</code> | Native ELF payloads with optional runtime libraries and archives. |
| <a id="entry-package-platform-android-0cfd01357a"></a><code>package.platform.Android</code> | <code>Android</code> | <code>implemented</code> | <code>arm32</code>, <code>arm64</code>, <code>x86</code> | <code>Client</code> | Gradle client project with native ABI libraries, assets, and optional APK. |
| <a id="entry-package-platform-web-c4af6ceacd"></a><code>package.platform.Web</code> | <code>Web</code> | <code>implemented</code> | <code>wasm</code> | <code>Client</code> | Browser client JavaScript/Wasm payload with preloaded resources. |
| <a id="entry-package-platform-macos-a9032ded5f"></a><code>package.platform.macOS</code> | <code>macOS</code> | <code>unsupported</code> | <code>x64</code>, <code>arm64</code> | <code>Client</code> | Accepted by argparse but package_macos currently aborts. |
| <a id="entry-package-platform-ios-15d87c2d03"></a><code>package.platform.iOS</code> | <code>iOS</code> | <code>unsupported</code> | <code>arm64</code>, <code>simulator</code> | <code>Client</code> | Accepted by argparse but package_ios currently aborts. |

## Pack tokens

| Stable ID | Pack | Category | Status | Platforms | Targets | Effect |
| --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-package-pack-raw-d7354ed4ba"></a><code>package.pack.Raw</code> | <code>Raw</code> | <code>artifact</code> | <code>implemented</code> | <code>Windows</code>, <code>Linux</code>, <code>Android</code>, <code>Web</code> | <code>Server</code>, <code>Client</code>, <code>Editor</code>, <code>Mapper</code>, <code>Baker</code> | Retain the staged target directory after finalization. |
| <a id="entry-package-pack-zip-3dcbf3c445"></a><code>package.pack.Zip</code> | <code>Zip</code> | <code>artifact</code> | <code>implemented</code> | <code>Windows</code>, <code>Linux</code>, <code>Android</code>, <code>Web</code> | <code>Server</code>, <code>Client</code>, <code>Editor</code>, <code>Mapper</code>, <code>Baker</code> | Create a deterministic ZIP beside the staged target directory. |
| <a id="entry-package-pack-singlezip-0c96b3b8c6"></a><code>package.pack.SingleZip</code> | <code>SingleZip</code> | <code>artifact</code> | <code>implemented</code> | <code>Windows</code>, <code>Linux</code>, <code>Android</code>, <code>Web</code> | <code>Server</code>, <code>Client</code>, <code>Editor</code>, <code>Mapper</code>, <code>Baker</code> | Append the staged target directory to one package-wide ZIP; identical same-name members are stored once, while conflicting contents fail packaging. |
| <a id="entry-package-pack-tar-e6de1a9557"></a><code>package.pack.Tar</code> | <code>Tar</code> | <code>artifact</code> | <code>implemented</code> | <code>Linux</code> | <code>Server</code>, <code>Client</code>, <code>Editor</code>, <code>Mapper</code>, <code>Baker</code> | Create an uncompressed tar archive. |
| <a id="entry-package-pack-targz-41851c8dfc"></a><code>package.pack.TarGz</code> | <code>TarGz</code> | <code>artifact</code> | <code>implemented</code> | <code>Linux</code> | <code>Server</code>, <code>Client</code>, <code>Editor</code>, <code>Mapper</code>, <code>Baker</code> | Create a gzip-compressed tar archive. |
| <a id="entry-package-pack-root-e56dc75506"></a><code>package.pack.Root</code> | <code>Root</code> | <code>artifact</code> | <code>implemented</code> | <code>Windows</code>, <code>Linux</code>, <code>Android</code>, <code>Web</code> | <code>Server</code>, <code>Client</code>, <code>Editor</code>, <code>Mapper</code>, <code>Baker</code> | Merge the staged target directory into the package output root. |
| <a id="entry-package-pack-wix-a1cec8e2e5"></a><code>package.pack.Wix</code> | <code>Wix</code> | <code>artifact</code> | <code>implemented</code> | <code>Windows</code> | <code>Client</code> | Build a required MSI from the staged Windows client payload; POSTFIX is appended to the installer base name. |
| <a id="entry-package-pack-apk-961529b696"></a><code>package.pack.Apk</code> | <code>Apk</code> | <code>artifact</code> | <code>implemented</code> | <code>Android</code> | <code>Client</code> | Run Gradle and copy the selected signed/debug APK beside the staged project. |
| <a id="entry-package-pack-debug-dca973b711"></a><code>package.pack.Debug</code> | <code>Debug</code> | <code>binary-selection</code> | <code>implemented</code> | <code>Windows</code>, <code>Linux</code>, <code>Android</code>, <code>Web</code> | <code>Server</code>, <code>Client</code>, <code>Editor</code>, <code>Mapper</code>, <code>Baker</code> | Select debug binary inputs; Android uses assembleDebug. |
| <a id="entry-package-pack-nores-5895efedf1"></a><code>package.pack.NoRes</code> | <code>NoRes</code> | <code>resource</code> | <code>implemented</code> | <code>Windows</code>, <code>Linux</code> | <code>Server</code>, <code>Client</code>, <code>Editor</code>, <code>Mapper</code>, <code>Baker</code> | Skip resource staging and embedded/config/build-name patching. |
| <a id="entry-package-pack-headless-c204e2e7ca"></a><code>package.pack.Headless</code> | <code>Headless</code> | <code>binary-selection</code> | <code>implemented</code> | <code>Windows</code>, <code>Linux</code> | <code>Server</code>, <code>Client</code> | Include the regular and Headless binary variants. |
| <a id="entry-package-pack-service-afbcd6373d"></a><code>package.pack.Service</code> | <code>Service</code> | <code>binary-selection</code> | <code>implemented</code> | <code>Windows</code> | <code>Server</code> | Include the regular server and Windows Service variants. |
| <a id="entry-package-pack-daemon-7e0a3ed687"></a><code>package.pack.Daemon</code> | <code>Daemon</code> | <code>binary-selection</code> | <code>implemented</code> | <code>Linux</code> | <code>Server</code> | Include the regular server and Linux Daemon variants. |
| <a id="entry-package-pack-totalprofiling-c82c1f4dfd"></a><code>package.pack.TotalProfiling</code> | <code>TotalProfiling</code> | <code>binary-selection</code> | <code>implemented</code> | <code>Windows</code>, <code>Linux</code> | <code>Server</code>, <code>Client</code> | Include the regular and total-profiling binary variants. |
| <a id="entry-package-pack-ondemandprofiling-c1bba3db77"></a><code>package.pack.OnDemandProfiling</code> | <code>OnDemandProfiling</code> | <code>binary-selection</code> | <code>implemented</code> | <code>Windows</code>, <code>Linux</code> | <code>Server</code>, <code>Client</code> | Include the regular and on-demand-profiling binary variants. |
| <a id="entry-package-pack-ogl-fef4944b2d"></a><code>package.pack.OGL</code> | <code>OGL</code> | <code>binary-selection</code> | <code>implemented</code> | <code>Windows</code> | <code>Client</code> | Include the regular and OpenGL client variants and patch ForceOpenGL=1. |
| <a id="entry-package-pack-lib-a225228362"></a><code>package.pack.Lib</code> | <code>Lib</code> | <code>binary-selection</code> | <code>implemented</code> | <code>Windows</code> | <code>Client</code>, <code>Baker</code> | Package the shared-library application instead of the executable. |
| <a id="entry-package-pack-webserver-c35e2bc615"></a><code>package.pack.WebServer</code> | <code>WebServer</code> | <code>payload</code> | <code>implemented</code> | <code>Web</code> | <code>Client</code> | Add the local simple-web-server.py helper to the browser payload. |
| <a id="entry-package-pack-appimage-5f139403c1"></a><code>package.pack.AppImage</code> | <code>AppImage</code> | <code>artifact</code> | <code>placeholder</code> | <code>Linux</code> | <code>Client</code> | Reserved token; the current Linux packager emits no AppImage artifact. |
